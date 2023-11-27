﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExConsolidateGraph.h"

#define LOCTEXT_NAMESPACE "PCGExConsolidateGraph"

int32 UPCGExConsolidateGraphSettings::GetPreferredChunkSize() const { return 32; }

PCGExIO::EInitMode UPCGExConsolidateGraphSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::DuplicateInput; }

FPCGElementPtr UPCGExConsolidateGraphSettings::CreateElement() const
{
	return MakeShared<FPCGExConsolidateGraphElement>();
}

FPCGContext* FPCGExConsolidateGraphElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExConsolidateGraphContext* Context = new FPCGExConsolidateGraphContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExConsolidateGraphSettings* Settings = Context->GetInputSettings<UPCGExConsolidateGraphSettings>();
	check(Settings);

	Context->bConsolidateEdgeType = Settings->bConsolidateEdgeType;
	return Context;
}

bool FPCGExConsolidateGraphElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExConsolidateGraphElement::Execute);

	FPCGExConsolidateGraphContext* Context = static_cast<FPCGExConsolidateGraphContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
		Context->SetState(PCGExMT::EState::ReadyForNextGraph);
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextGraph))
	{
		if (!Context->AdvanceGraph(true))
		{
			Context->SetState(PCGExMT::EState::Done); //No more params
		}
		else
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
		}
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO(false))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextGraph); //No more points, move to next params
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingPoints);
		}
	}

	// 1st Pass on points

	auto InitializePointsFirstPass = [&](UPCGExPointIO* PointIO)
	{
		Context->IndicesRemap.Empty(PointIO->NumInPoints);
		PointIO->BuildMetadataEntries();
		Context->PrepareCurrentGraphForPoints(PointIO->Out, true); // Prepare to read PointIO->Out
	};

	auto ProcessPoint = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
	{
		FWriteScopeLock WriteLock(Context->IndicesLock);
		const int64 Key = PointIO->GetOutPoint(PointIndex).MetadataEntry;
		const int64 CachedIndex = Context->CachedIndex->GetValueFromItemKey(Key);
		Context->IndicesRemap.Add(CachedIndex, PointIndex); // Store previous
		Context->CachedIndex->SetValue(Key, PointIndex);    // Update cached value with fresh one
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->AsyncProcessingCurrentPoints(InitializePointsFirstPass, ProcessPoint))
		{
			Context->SetState(PCGExMT::EState::ProcessingPoints2ndPass);
		}
	}

	// 2nd Pass on points - Swap indices with updated ones

	auto ConsolidatePoint = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
	{
		FReadScopeLock ReadLock(Context->IndicesLock);
		const FPCGPoint& Point = PointIO->GetOutPoint(PointIndex);
		for (const PCGExGraph::FSocketInfos& SocketInfos : Context->SocketInfos)
		{
			const int64 OldRelationIndex = SocketInfos.Socket->GetTargetIndex(Point.MetadataEntry);

			if (OldRelationIndex == -1) { continue; } // No need to fix further

			const int64 NewRelationIndex = GetFixedIndex(Context, OldRelationIndex);
			const PCGMetadataEntryKey Key = Point.MetadataEntry;
			PCGMetadataEntryKey NewEntryKey = PCGInvalidEntryKey;

			if (NewRelationIndex != -1) { NewEntryKey = PointIO->GetOutPoint(NewRelationIndex).MetadataEntry; }
			else { SocketInfos.Socket->SetEdgeType(Key, EPCGExEdgeType::Unknown); }

			SocketInfos.Socket->SetTargetIndex(Key, NewRelationIndex);
			SocketInfos.Socket->SetTargetEntryKey(Key, NewEntryKey);
		}
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints2ndPass))
	{
		if (Context->AsyncProcessingCurrentPoints(ConsolidatePoint))
		{
			Context->SetState(Context->bConsolidateEdgeType ? PCGExMT::EState::ProcessingPoints3rdPass : PCGExMT::EState::ReadyForNextPoints);
		}
	}

	// Optional 3rd Pass on points - Recompute edges type

	auto ConsolidateEdgesType = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
	{
		Context->ComputeEdgeType(PointIO->GetOutPoint(PointIndex), PointIndex, PointIO);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints3rdPass))
	{
		if (Context->AsyncProcessingCurrentPoints(ConsolidateEdgesType))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
		}
	}

	// Done

	if (Context->IsState(PCGExMT::EState::Done))
	{
		Context->IndicesRemap.Empty();
		Context->OutputPointsAndParams();
		return true;
	}

	return false;
}

int64 FPCGExConsolidateGraphElement::GetFixedIndex(FPCGExConsolidateGraphContext* Context, int64 InIndex)
{
	if (const int64* FixedRelationIndexPtr = Context->IndicesRemap.Find(InIndex)) { return *FixedRelationIndexPtr; }
	return -1;
}

#undef LOCTEXT_NAMESPACE

﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExSubdivideEdges.h"


#include "Graph/Edges/Relaxing/PCGExRelaxClusterOperation.h"

#define LOCTEXT_NAMESPACE "PCGExSubdivideEdges"
#define PCGEX_NAMESPACE SubdivideEdges

PCGExData::EInit UPCGExSubdivideEdgesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGExData::EInit UPCGExSubdivideEdgesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(SubdivideEdges)

bool FPCGExSubdivideEdgesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SubdivideEdges)

	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendOperation)

	return true;
}

bool FPCGExSubdivideEdgesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSubdivideEdgesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SubdivideEdges)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExSubdivideEdges::FProcessorBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExSubdivideEdges::FProcessorBatch>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExSubdivideEdges
{
	FProcessor::~FProcessor()
	{
	}

	TSharedPtr<PCGExCluster::FCluster> FProcessor::HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef)
	{
		return MakeShared<PCGExCluster::FCluster>(
			InClusterRef, VtxDataFacade->Source, VtxDataFacade->Source,
			true, false, false);
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSubdivideEdges::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		if (!DirectionSettings.InitFromParent(ExecutionContext, StaticCastWeakPtr<FProcessorBatch>(ParentBatch).Pin()->DirectionSettings, EdgeDataFacade))
		{
			return false;
		}

		Blending = Context->Blending->CopyOperation<UPCGExSubPointsBlendOperation>();
		PCGEx::InitArray(Subdivisions, EdgeDataFacade->GetNum());

		StartParallelLoopForEdges();

		return true;
	}

	void FProcessor::ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count)
	{
		const TSharedRef<PCGExData::FPointIO>& PointIO = EdgeDataFacade->Source;

		DirectionSettings.SortEndpoints(Cluster.Get(), Edge);

		const PCGExCluster::FNode& StartNode = *(Cluster->Nodes->GetData() + (*Cluster->NodeIndexLookup)[Edge.Start]);
		const PCGExCluster::FNode& EndNode = *(Cluster->Nodes->GetData() + (*Cluster->NodeIndexLookup)[Edge.End]);

		// Create subdivision items
		FSubdivision& Sub = Subdivisions[EdgeIndex];

		Sub.NumSubdivisions = 0;

		// Check if that edge should be subdivided. How depends on the test source
		// Can be:
		// - Edge start test
		// - Edge end test
		// - Edge itself test


		Sub.Start = Cluster->GetPos(StartNode);
		Sub.End = Cluster->GetPos(EndNode);
		Sub.Dist = FVector::Distance(Sub.Start, Sub.End);
	}

	void FProcessor::CompleteWork()
	{
	}

	void FProcessor::Write()
	{
		FClusterProcessor::Write();
	}

	void FProcessorBatch::GatherRequiredVtxAttributes(PCGExData::FReadableBufferConfigList& ReadableBufferConfigList)
	{
		TBatchWithGraphBuilder<FProcessor>::GatherRequiredVtxAttributes(ReadableBufferConfigList);
		DirectionSettings.GatherRequiredVtxAttributes(ExecutionContext, ReadableBufferConfigList);
	}

	void FProcessorBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SubdivideEdges)

		DirectionSettings = Settings->DirectionSettings;
		if (!DirectionSettings.Init(ExecutionContext))
		{
			bIsBatchValid = false;
			return;
		}

		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

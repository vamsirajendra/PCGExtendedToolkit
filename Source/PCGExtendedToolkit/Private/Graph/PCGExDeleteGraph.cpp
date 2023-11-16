﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExDeleteGraph.h"

#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Graph/PCGExGraphHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExDeleteGraph"

int32 UPCGExDeleteGraphSettings::GetPreferredChunkSize() const { return 32; }

FPCGElementPtr UPCGExDeleteGraphSettings::CreateElement() const
{
	return MakeShared<FPCGExDeleteGraphElement>();
}

PCGEx::EIOInit UPCGExDeleteGraphSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

FPCGContext* FPCGExDeleteGraphElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExDeleteGraphContext* Context = new FPCGExDeleteGraphContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

void FPCGExDeleteGraphElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExGraphProcessorElement::InitializeContext(InContext, InputData, SourceComponent, Node);
	//FPCGExDeleteGraphContext* Context = static_cast<FPCGExDeleteGraphContext*>(InContext);
	// ...
}

bool FPCGExDeleteGraphElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDeleteGraphElement::Execute);

	FPCGExDeleteGraphContext* Context = static_cast<FPCGExDeleteGraphContext*>(InContext);
	if (!Validate(Context)) { return true; }
	
	Context->Points->ForEach(
		[&Context](UPCGExPointIO* PointIO, int32)
		{
			auto DeleteSockets = [&PointIO](const UPCGExGraphParamsData* Params, int32)
			{
				for (const PCGExGraph::FSocket& Socket : Params->GetSocketMapping()->Sockets)
				{
					//TODO: Remove individual socket attributes
					Socket.DeleteFrom(PointIO->Out);
				}
				PointIO->Out->Metadata->DeleteAttribute(Params->CachedIndexAttributeName);
			};
			Context->Params.ForEach(Context, DeleteSockets);
		});

	Context->OutputPointsAndParams();
	
	return true;
}

#undef LOCTEXT_NAMESPACE

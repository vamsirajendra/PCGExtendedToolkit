﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExBridgeEdgeClusters.h"

#include "Data/PCGExPointIOMerger.h"
#include "Geometry/PCGExGeoDelaunay.h"

#define LOCTEXT_NAMESPACE "PCGExBridgeEdgeClusters"
#define PCGEX_NAMESPACE BridgeEdgeClusters

PCGExData::EInit UPCGExBridgeEdgeClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExBridgeEdgeClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(BridgeEdgeClusters)


FPCGExBridgeEdgeClustersContext::~FPCGExBridgeEdgeClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	ProjectionSettings.Cleanup();
}


bool FPCGExBridgeEdgeClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BridgeEdgeClusters)

	PCGEX_FWD(ProjectionSettings)
	PCGEX_FWD(GraphBuilderSettings)

	return true;
}

bool FPCGExBridgeEdgeClustersElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBridgeEdgeClustersElement::Execute);

	PCGEX_CONTEXT(BridgeEdgeClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExBridgeClusters::FProcessorBatch>(
			[&](PCGExData::FPointIOTaggedEntries* Entries)
			{
				if (Entries->Entries.Num() == 1)
				{
					// No clusters to consolidate, just dump existing points
					Context->CurrentIO->InitializeOutput(PCGExData::EInit::Forward);
					Entries->Entries[0]->InitializeOutput(PCGExData::EInit::Forward);
					return false;
				}

				return true;
			},
			[&](PCGExBridgeClusters::FProcessorBatch* NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("No bridge was created."));
			Context->OutputPointsAndEdges();
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	for (PCGExClusterMT::FClusterProcessorBatchBase* Batch : Context->Batches)
	{
		const PCGExBridgeClusters::FProcessorBatch* BridgeBatch = static_cast<PCGExBridgeClusters::FProcessorBatch*>(Batch);
		const int64 ClusterId = BridgeBatch->VtxIO->GetOut()->UID;
		PCGExData::WriteMark(BridgeBatch->ConsolidatedEdges->GetOut()->Metadata, PCGExGraph::Tag_ClusterId, ClusterId);

		FString OutId;
		PCGExGraph::SetClusterVtx(BridgeBatch->VtxIO, OutId);
		PCGExGraph::MarkClusterEdges(BridgeBatch->ConsolidatedEdges, OutId);
	}

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExBridgeClusters
{
	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		PCGEX_SETTINGS(BridgeEdgeClusters)

		return true;
	}

	void FProcessor::ProcessSingleEdge(PCGExGraph::FIndexedEdge& Edge)
	{
		PCGEX_SETTINGS(BridgeEdgeClusters)
	}

	void FProcessor::CompleteWork()
	{
	}

	//////// BATCH

	FProcessorBatch::FProcessorBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, const TArrayView<PCGExData::FPointIO*> InEdges):
		TBatch(InContext, InVtx, InEdges)
	{
	}

	FProcessorBatch::~FProcessorBatch()
	{
		PCGEX_DELETE(Merger)

		ConsolidatedEdges = nullptr;
		Bridges.Empty();
	}

	bool FProcessorBatch::PrepareProcessing()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BridgeEdgeClusters)
		const FPCGExBridgeEdgeClustersContext* InContext = static_cast<FPCGExBridgeEdgeClustersContext*>(Context);

		ConsolidatedEdges = TypedContext->MainEdges->Emplace_GetRef(PCGExData::EInit::NewOutput);

		if (!TBatch::PrepareProcessing()) { return false; }

		return true;
	}

	void FProcessorBatch::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TBatch<FProcessor>::Process(AsyncManager);

		// Start merging right away
		TSet<FName> IgnoreAttributes = {PCGExGraph::Tag_ClusterId};

		Merger = new FPCGExPointIOMerger(*ConsolidatedEdges);
		Merger->Append(Edges);
		Merger->Merge(AsyncManagerPtr, &IgnoreAttributes);
	}

	bool FProcessorBatch::PrepareSingle(FProcessor* ClusterProcessor)
	{
		PCGEX_SETTINGS(BridgeEdgeClusters)
		ConsolidatedEdges->Tags->Append(ClusterProcessor->EdgesIO->Tags);

		return true;
	}

	void FProcessorBatch::CompleteWork()
	{
		const int32 NumValidClusters = GatherValidClusters();

		if (Processors.Num() != NumValidClusters)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some vtx/edges groups have invalid clusters. Make sure to sanitize the input first."));
		}

		if (ValidClusters.IsEmpty()) { return; } // Skip work completion entirely

		Merger->Write(AsyncManagerPtr); // Write base attributes value while finding bridges		

		////
		PCGEX_SETTINGS(BridgeEdgeClusters)
		const FPCGExBridgeEdgeClustersContext* InContext = static_cast<FPCGExBridgeEdgeClustersContext*>(Context);

		const int32 NumBounds = ValidClusters.Num();
		EPCGExBridgeClusterMethod SafeMethod = Settings->BridgeMethod;

		if (NumBounds <= 4)
		{
			if (SafeMethod == EPCGExBridgeClusterMethod::Delaunay3D) { SafeMethod = EPCGExBridgeClusterMethod::MostEdges; }
		}
		else if (NumBounds <= 3)
		{
			if (SafeMethod == EPCGExBridgeClusterMethod::Delaunay2D) { SafeMethod = EPCGExBridgeClusterMethod::MostEdges; }
		}

		// First find which cluster are connected

		TArray<FBox> Bounds;
		Bounds.SetNumUninitialized(NumBounds);
		for (int i = 0; i < NumBounds; i++) { Bounds[i] = ValidClusters[i]->Bounds; }

		if (SafeMethod == EPCGExBridgeClusterMethod::Delaunay3D)
		{
			PCGExGeo::TDelaunay3* Delaunay = new PCGExGeo::TDelaunay3();

			TArray<FVector> Positions;
			Positions.SetNum(NumBounds);

			for (int i = 0; i < NumBounds; i++) { Positions[i] = Bounds[i].GetCenter(); }

			if (Delaunay->Process(Positions, false)) { Bridges.Append(Delaunay->DelaunayEdges); }
			else { PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Delaunay 3D failed. Are points coplanar? If so, use Delaunay 2D instead.")); }

			Positions.Empty();
			PCGEX_DELETE(Delaunay)
		}
		else if (SafeMethod == EPCGExBridgeClusterMethod::Delaunay2D)
		{
			PCGExGeo::TDelaunay2* Delaunay = new PCGExGeo::TDelaunay2();

			TArray<FVector> Positions;
			Positions.SetNum(NumBounds);

			for (int i = 0; i < NumBounds; i++) { Positions[i] = Bounds[i].GetCenter(); }

			if (Delaunay->Process(Positions, InContext->ProjectionSettings)) { Bridges.Append(Delaunay->DelaunayEdges); }
			else { PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Delaunay 2D failed.")); }

			Positions.Empty();
			PCGEX_DELETE(Delaunay)
		}
		else if (SafeMethod == EPCGExBridgeClusterMethod::LeastEdges)
		{
			TSet<int32> VisitedEdges;
			for (int i = 0; i < NumBounds; i++)
			{
				VisitedEdges.Add(i); // As to not connect to self or already connected
				double Distance = TNumericLimits<double>::Max();
				int32 ClosestIndex = -1;

				for (int j = 0; j < NumBounds; j++)
				{
					if (i == j || VisitedEdges.Contains(j)) { continue; }

					if (const double Dist = FVector::DistSquared(Bounds[i].GetCenter(), Bounds[j].GetCenter());
						Dist < Distance)
					{
						ClosestIndex = j;
						Distance = Dist;
					}
				}

				if (ClosestIndex == -1) { continue; }

				Bridges.Add(PCGEx::H64(i, ClosestIndex));
			}
		}
		else if (SafeMethod == EPCGExBridgeClusterMethod::MostEdges)
		{
			for (int i = 0; i < NumBounds; i++)
			{
				for (int j = 0; j < NumBounds; j++)
				{
					if (i == j) { continue; }
					Bridges.Add(PCGEx::H64U(i, j));
				}
			}
		}
	}

	void FProcessorBatch::Write()
	{
		TBatch<FProcessor>::Write();

		TArray<FPCGPoint>& MutableEdges = ConsolidatedEdges->GetOut()->GetMutablePoints();
		UPCGMetadata* Metadata = ConsolidatedEdges->GetOut()->Metadata;

		for (const uint64 Bridge : Bridges)
		{
			int32 EdgePointIndex;
			FPCGPoint& EdgePoint = ConsolidatedEdges->NewPoint(EdgePointIndex);

			uint32 Start;
			uint32 End;
			PCGEx::H64(Bridge, Start, End);

			AsyncManagerPtr->Start<FPCGExCreateBridgeTask>(
				EdgePointIndex, ConsolidatedEdges,
				this, ValidClusters[Start], ValidClusters[End]);
		}

		// Force writing cluster ID to Vtx, otherwise we inherit from previous metadata.
		const uint64 ClusterId = VtxIO->GetOut()->UID;
		PCGEx::TFAttributeWriter<int64>* ClusterIdWriter = new PCGEx::TFAttributeWriter<int64>(PCGExGraph::Tag_ClusterId);
		for (int64& Id : ClusterIdWriter->Values) { Id = ClusterId; }
		PCGEX_ASYNC_WRITE_DELETE(AsyncManagerPtr, ClusterIdWriter);

		// TODO : OPTIM : We can easily build this batch' cluster by appending existing ones into a big one and just add edges
		
	}


	bool FPCGExCreateBridgeTask::ExecuteTask()
	{
		int32 IndexA = -1;
		int32 IndexB = -1;

		double Distance = TNumericLimits<double>::Max();

		const TArray<PCGExCluster::FNode>& NodesRefA = *ClusterA->Nodes;
		const TArray<PCGExCluster::FNode>& NodesRefB = *ClusterB->Nodes;

		//Brute force find closest points
		for (const PCGExCluster::FNode& Node : NodesRefA)
		{
			const PCGExCluster::FNode& OtherNode = NodesRefB[ClusterB->FindClosestNode(Node.Position)];

			if (const double Dist = FVector::DistSquared(Node.Position, OtherNode.Position);
				Dist < Distance)
			{
				IndexA = Node.PointIndex;
				IndexB = OtherNode.PointIndex;
				Distance = Dist;
			}
		}

		UPCGMetadata* EdgeMetadata = PointIO->GetOut()->Metadata;

		FPCGMetadataAttribute<int64>* InVtxEndpointAtt = static_cast<FPCGMetadataAttribute<int64>*>(Batch->VtxIO->GetIn()->Metadata->GetMutableAttribute(PCGExGraph::Tag_VtxEndpoint));

		FPCGPoint& EdgePoint = PointIO->GetOut()->GetMutablePoints()[TaskIndex];

		const FPCGPoint& StartPoint = Batch->VtxIO->GetOutPoint(IndexA);
		const FPCGPoint& EndPoint = Batch->VtxIO->GetOutPoint(IndexB);

		EdgePoint.Transform.SetLocation(FMath::Lerp(StartPoint.Transform.GetLocation(), EndPoint.Transform.GetLocation(), 0.5));

		uint32 StartIdx;
		uint32 StartNumEdges;

		uint32 EndIdx;
		uint32 EndNumEdges;

		PCGEx::H64(InVtxEndpointAtt->GetValueFromItemKey(Batch->VtxIO->GetInPoint(IndexA).MetadataEntry), StartIdx, StartNumEdges);
		PCGEx::H64(InVtxEndpointAtt->GetValueFromItemKey(Batch->VtxIO->GetInPoint(IndexB).MetadataEntry), EndIdx, EndNumEdges);

		FPCGMetadataAttribute<int64>* EdgeEndpointsAtt = static_cast<FPCGMetadataAttribute<int64>*>(EdgeMetadata->GetMutableAttribute(PCGExGraph::Tag_EdgeEndpoints));
		FPCGMetadataAttribute<int64>* OutVtxEndpointAtt = static_cast<FPCGMetadataAttribute<int64>*>(Batch->VtxIO->GetOut()->Metadata->GetMutableAttribute(PCGExGraph::Tag_VtxEndpoint));

		EdgeEndpointsAtt->SetValue(EdgePoint.MetadataEntry, PCGEx::H64(StartIdx, EndIdx));
		OutVtxEndpointAtt->SetValue(StartPoint.MetadataEntry, PCGEx::H64(StartIdx, StartNumEdges + 1));
		OutVtxEndpointAtt->SetValue(EndPoint.MetadataEntry, PCGEx::H64(EndIdx, EndNumEdges + 1));

		return true;
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

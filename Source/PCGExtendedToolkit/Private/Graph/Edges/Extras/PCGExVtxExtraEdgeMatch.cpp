﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/Extras/PCGExVtxExtraEdgeMatch.h"

#include "Data/PCGExPointFilter.h"

#define LOCTEXT_NAMESPACE "PCGExVtxExtraEdgeMatch"
#define PCGEX_NAMESPACE PCGExVtxExtraEdgeMatch

void UPCGExVtxExtraEdgeMatch::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	const UPCGExVtxExtraEdgeMatch* TypedOther = Cast<UPCGExVtxExtraEdgeMatch>(Other);
	if (TypedOther)
	{
		Descriptor = TypedOther->Descriptor;
	}
}

void UPCGExVtxExtraEdgeMatch::ClusterReserve(const int32 NumClusters)
{
	Super::ClusterReserve(NumClusters);
	FilterManagers.SetNumUninitialized(NumClusters);
	for (int i = 0; i < NumClusters; i++) { FilterManagers[i] = nullptr; }
}

void UPCGExVtxExtraEdgeMatch::PrepareForCluster(const FPCGContext* InContext, const int32 ClusterIdx, PCGExCluster::FCluster* Cluster, PCGExDataCaching::FPool* VtxDataCache, PCGExDataCaching::FPool* EdgeDataCache)
{
	Super::PrepareForCluster(InContext, ClusterIdx, Cluster, VtxDataCache, EdgeDataCache);
	if (FilterFactories && !FilterFactories->IsEmpty())
	{
		/*
		PCGExPointFilter::TEarlyExitFilterManager* FilterManager = new PCGExPointFilter::TEarlyExitFilterManager(VtxDataCache);
		FilterManagers[ClusterIdx] = FilterManager;
		FilterManager->bCacheResults = false;
		FilterManager->Register(InContext, *FilterFactories);
		*/
		// TODO : Prepare for cluster etc 
	}
}

bool UPCGExVtxExtraEdgeMatch::PrepareForVtx(const FPCGContext* InContext, PCGExData::FPointIO* InVtx, PCGExDataCaching::FPool* VtxDataCache)
{
	if (!Super::PrepareForVtx(InContext, InVtx, VtxDataCache)) { return false; }

	if (!Descriptor.MatchingEdge.Validate(InContext))
	{
		bIsValidOperation = false;
		return false;
	}

	if (!Descriptor.DotComparisonSettings.Init(InContext, VtxDataCache))
	{
		bIsValidOperation = false;
		return false;
	}

	if (Descriptor.DirectionSource == EPCGExFetchType::Attribute)
	{
		DirCache = PrimaryDataCache->GetOrCreateGetter<FVector>(Descriptor.Direction);
		if (!DirCache)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Direction attribute is invalid"));
			bIsValidOperation = false;
			return false;
		}
	}

	Descriptor.MatchingEdge.Init(InVtx);

	return bIsValidOperation;
}

void UPCGExVtxExtraEdgeMatch::ProcessNode(const int32 ClusterIdx, const PCGExCluster::FCluster* Cluster, PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency)
{
	PCGExPointFilter::TManager* EdgeFilters = FilterManagers[ClusterIdx]; //TODO : Implement properly

	const FPCGPoint& Point = Vtx->GetInPoint(Node.PointIndex);

	double BestDot = TNumericLimits<double>::Min();
	int32 IBest = -1;
	const double DotB = Descriptor.DotComparisonSettings.GetDot(Node.PointIndex);

	FVector NodeDirection = DirCache ? DirCache->Values[Node.PointIndex] : Descriptor.DirectionConstant;
	if (Descriptor.bTransformDirection) { NodeDirection = Point.Transform.TransformVectorNoScale(NodeDirection); }

	for (int i = 0; i < Adjacency.Num(); i++)
	{
		const PCGExCluster::FAdjacencyData& A = Adjacency[i];
		const double DotA = FVector::DotProduct(NodeDirection, A.Direction);

		if (Descriptor.DotComparisonSettings.Test(DotA, DotB))
		{
			if (DotA > BestDot)
			{
				BestDot = DotA;
				IBest = i;
			}
		}
	}

	if (IBest != -1) { Descriptor.MatchingEdge.Set(Node.PointIndex, Adjacency[IBest], (*Cluster->Nodes)[Adjacency[IBest].NodeIndex].Adjacency.Num()); }
	else { Descriptor.MatchingEdge.Set(Node.PointIndex, 0, FVector::ZeroVector, -1, -1, 0); }
}

void UPCGExVtxExtraEdgeMatch::Write()
{
	Super::Write();
	Descriptor.MatchingEdge.Write();
}

void UPCGExVtxExtraEdgeMatch::Write(PCGExMT::FTaskManager* AsyncManager)
{
	Super::Write(AsyncManager);
	Descriptor.MatchingEdge.Write(AsyncManager);
}

void UPCGExVtxExtraEdgeMatch::Cleanup()
{
	PCGEX_DELETE_TARRAY(FilterManagers)

	Descriptor.MatchingEdge.Cleanup();
	Super::Cleanup();
}

void UPCGExVtxExtraEdgeMatch::InitEdgeFilters()
{
	if (bEdgeFilterInitialized) { return; }

	FWriteScopeLock WriteScopeLock(FilterLock);

	bEdgeFilterInitialized = true;
}

#if WITH_EDITOR
FString UPCGExVtxExtraEdgeMatchSettings::GetDisplayName() const
{
	/*
	if (Descriptor.SourceAttributes.IsEmpty()) { return TEXT(""); }
	TArray<FName> Names = Descriptor.SourceAttributes.Array();

	if (Names.Num() == 1) { return Names[0].ToString(); }
	if (Names.Num() == 2) { return Names[0].ToString() + TEXT(" (+1 other)"); }

	return Names[0].ToString() + FString::Printf(TEXT(" (+%d others)"), (Names.Num() - 1));
	*/
	return TEXT("");
}
#endif

UPCGExVtxExtraOperation* UPCGExVtxExtraEdgeMatchFactory::CreateOperation() const
{
	UPCGExVtxExtraEdgeMatch* NewOperation = NewObject<UPCGExVtxExtraEdgeMatch>();
	PCGEX_VTX_EXTRA_CREATE

	if (!FilterFactories.IsEmpty())
	{
		NewOperation->FilterFactories = const_cast<TArray<UPCGExFilterFactoryBase*>*>(&FilterFactories);
	}

	return NewOperation;
}

TArray<FPCGPinProperties> UPCGExVtxExtraEdgeMatchSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGEx::SourceAdditionalReq, "Additional Requirements for the match", Advanced, {})
	return PinProperties;
}

UPCGExParamFactoryBase* UPCGExVtxExtraEdgeMatchSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExVtxExtraEdgeMatchFactory* NewFactory = NewObject<UPCGExVtxExtraEdgeMatchFactory>();
	NewFactory->Descriptor = Descriptor;
	PCGExFactories::GetInputFactories(InContext, PCGEx::SourceAdditionalReq, NewFactory->FilterFactories, PCGExFactories::ClusterEdgeFilters, false);
	return Super::CreateFactory(InContext, NewFactory);
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "PCGExVtxPropertyFactoryProvider.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExGraph.h"

#include "PCGExVtxPropertySpecialEdges.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSpecialEdgesSettings
{
	GENERATED_BODY()

	/** Shortest edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExEdgeOutputWithIndexSettings ShortestEdge = FPCGExEdgeOutputWithIndexSettings(TEXT("Shortest"));

	/** Longest edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExEdgeOutputWithIndexSettings LongestEdge = FPCGExEdgeOutputWithIndexSettings(TEXT("Longest"));

	/** Average edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExSimpleEdgeOutputSettings AverageEdge = FPCGExSimpleEdgeOutputSettings(TEXT("Average"));
};

/**
 * ó
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExVtxPropertySpecialEdges : public UPCGExVtxPropertyOperation
{
	GENERATED_BODY()

public:
	FPCGExSpecialEdgesSettings Descriptor;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;
	virtual bool PrepareForVtx(const FPCGContext* InContext, PCGExData::FFacade* InVtxDataFacade) override;
	virtual void ProcessNode(const int32 ClusterIdx, const PCGExCluster::FCluster* Cluster, PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency) override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExVtxPropertySpecialEdgesFactory : public UPCGExVtxPropertyFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExSpecialEdgesSettings Descriptor;
	virtual UPCGExVtxPropertyOperation* CreateOperation() const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|VtxProperty")
class PCGEXTENDEDTOOLKIT_API UPCGExVtxPropertySpecialEdgesSettings : public UPCGExVtxPropertyProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(VtxSpecialEdges, "Vtx : Special Edges", "Edge' edge cases (pun not intended)")
#endif
	//~End UPCGSettings

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Direction Settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExSpecialEdgesSettings Descriptor;
};

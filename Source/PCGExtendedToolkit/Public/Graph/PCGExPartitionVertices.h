﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExEdgesProcessor.h"

#include "PCGExPartitionVertices.generated.h"


UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExPartitionVerticesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PartitionVertices, "Graph : Partition Vertices", "Split Vtx into per-cluster groups.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorGraph; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExEdgesProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExEdgesProcessorSettings interface
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPartitionVerticesContext final : public FPCGExEdgesProcessorContext
{
	friend class UPCGExPartitionVerticesSettings;
	friend class FPCGExPartitionVerticesElement;

	virtual ~FPCGExPartitionVerticesContext() override;

	PCGExData::FPointIOCollection* VtxPartitions = nullptr;
	TArray<PCGExGraph::FIndexedEdge> IndexedEdges;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPartitionVerticesElement final : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExCreateVtxPartitionTask final : public PCGExMT::FPCGExTask
{
public:
	FPCGExCreateVtxPartitionTask(PCGExData::FPointIO* InPointIO,
	                             PCGExData::FPointIO* InEdgeIO,
	                             TMap<int64, int32>* InEndpointsLookup) :
		FPCGExTask(InPointIO),
		EdgeIO(InEdgeIO),
		EndpointsLookup(InEndpointsLookup)
	{
	}

	PCGExData::FPointIO* EdgeIO = nullptr;
	TMap<int64, int32>* EndpointsLookup = nullptr;

	virtual bool ExecuteTask() override;
};

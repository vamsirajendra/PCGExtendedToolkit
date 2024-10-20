﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExDataForward.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Misc/PCGExFindPointOnBounds.h"
#include "PCGExFindPointOnBoundsClusters.generated.h"

UCLASS(Abstract, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFindPointOnBoundsClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindPointOnBoundsClusters, "Cluster : Find point on Bounds", "Find the closest vtx or edge on each cluster' bounds.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorCluster; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

	/** What type of proximity to look for */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExClusterClosestSearchMode SearchMode = EPCGExClusterClosestSearchMode::Node;

	/** Data output mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointOnBoundsOutputMode OutputMode = EPCGExPointOnBoundsOutputMode::Merged;

	/** U Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|UVW", meta=(PCG_Overridable, DisplayName="U"))
	double UConstant = 1;

	/** V Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|UVW", meta=(PCG_Overridable, DisplayName="V"))
	double VConstant = 1;

	/** W Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|UVW", meta=(PCG_Overridable, DisplayName="W"))
	double WConstant = 0;

	/** TBD */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	//FPCGExAttributeToTagDetails TargetAttributesToTags;

	/** Which Seed attributes to forward on paths. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	//FPCGExForwardDetails TargetForwarding;

private:
	friend class FPCGExFindPointOnBoundsClustersElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFindPointOnBoundsClustersContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExFindPointOnBoundsClustersElement;

	FPCGExAttributeToTagDetails TargetAttributesToTags;
	TSharedPtr<PCGExData::FDataForwardHandler> TargetForwardHandler;

	virtual void ClusterProcessing_InitialProcessingDone() override;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFindPointOnBoundsClustersElement final : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExFindPointOnBoundsClusters
{
	class FProcessor final : public PCGExClusterMT::TClusterProcessor<FPCGExFindPointOnBoundsClustersContext, UPCGExFindPointOnBoundsClustersSettings>
	{
		mutable FRWLock BestIndexLock;

		FVector SearchPosition = FVector::ZeroVector;
		int32 BestIndex = -1;
		double BestDistance = DBL_MAX;

	public:
		int32 Picker = -1;

		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TClusterProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		void UpdateCandidate(const FVector& InPosition, const int32 InIndex);
		virtual void ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count) override;
		virtual void ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}

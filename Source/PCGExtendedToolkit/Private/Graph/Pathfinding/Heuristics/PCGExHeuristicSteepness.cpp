﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicSteepness.h"

void UPCGExHeuristicSteepness::PrepareForCluster(const PCGExCluster::FCluster* InCluster)
{
	UpwardVector = UpVector.GetSafeNormal();
	Super::PrepareForCluster(InCluster);
}

UPCGExHeuristicOperation* UPCGHeuristicsFactorySteepness::CreateOperation() const
{
	UPCGExHeuristicSteepness* NewOperation = NewObject<UPCGExHeuristicSteepness>();
	PCGEX_FORWARD_HEURISTIC_DESCRIPTOR
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExHeuristicsSteepnessProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGHeuristicsFactorySteepness* NewFactory = NewObject<UPCGHeuristicsFactorySteepness>();
	PCGEX_FORWARD_HEURISTIC_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExHeuristicsSteepnessProviderSettings::GetDisplayName() const
{
	return GetDefaultNodeName().ToString()
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Descriptor.WeightFactor) / 1000.0));
}
#endif

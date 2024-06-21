﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingNavmesh.h"

#include "NavigationSystem.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExPathfinding.cpp"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExPathfindingNavmeshElement"
#define PCGEX_NAMESPACE PathfindingNavmesh

TArray<FPCGPinProperties> UPCGExPathfindingNavmeshSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINT(PCGExGraph::SourceSeedsLabel, "Seeds points for pathfinding.", Required, {})
	PCGEX_PIN_POINT(PCGExGraph::SourceGoalsLabel, "Goals points for pathfinding.", Required, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPathfindingNavmeshSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputPathsLabel, "Paths output.", Required, {})
	return PinProperties;
}

#if WITH_EDITOR
void UPCGExPathfindingNavmeshSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (GoalPicker) { GoalPicker->UpdateUserFacingInfos(); }
	if (Blending) { Blending->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGExData::EInit UPCGExPathfindingNavmeshSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FName UPCGExPathfindingNavmeshSettings::GetMainInputLabel() const { return PCGExGraph::SourceSeedsLabel; }
FName UPCGExPathfindingNavmeshSettings::GetMainOutputLabel() const { return PCGExGraph::OutputPathsLabel; }

PCGEX_INITIALIZE_ELEMENT(PathfindingNavmesh)

void UPCGExPathfindingNavmeshSettings::PostInitProperties()
{
	Super::PostInitProperties();
	PCGEX_OPERATION_DEFAULT(GoalPicker, UPCGExGoalPickerRandom)
	PCGEX_OPERATION_DEFAULT(Blending, UPCGExSubPointsBlendInterpolate)
}

FPCGExPathfindingNavmeshContext::~FPCGExPathfindingNavmeshContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE_TARRAY(PathQueries)

	PCGEX_DELETE(GoalsPoints)
	PCGEX_DELETE(OutputPaths)

	PCGEX_DELETE(SeedTagValueGetter)
	PCGEX_DELETE(GoalTagValueGetter)

	PCGEX_DELETE(SeedForwardHandler)
	PCGEX_DELETE(GoalForwardHandler)
}

bool FPCGExPathfindingNavmeshElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingNavmesh)

	Context->SeedsPoints = Context->MainPoints->Pairs[0];

	PCGEX_OPERATION_BIND(GoalPicker, UPCGExGoalPickerRandom)
	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInterpolate)


	Context->SeedsPoints = Context->TryGetSingleInput(PCGExGraph::SourceSeedsLabel, true);
	if (!Context->SeedsPoints) { return false; }

	Context->GoalsPoints = Context->TryGetSingleInput(PCGExGraph::SourceGoalsLabel, true);
	if (!Context->GoalsPoints) { return false; }


	if (Settings->bUseSeedAttributeToTagPath)
	{
		Context->SeedTagValueGetter = new PCGEx::FLocalToStringGetter();
		Context->SeedTagValueGetter->Capture(Settings->SeedTagAttribute);
		if (!Context->SeedTagValueGetter->SoftGrab(*Context->SeedsPoints))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Missing specified Attribute to Tag on Seed points."));
			return false;
		}
	}

	if (Settings->bUseGoalAttributeToTagPath)
	{
		Context->GoalTagValueGetter = new PCGEx::FLocalToStringGetter();
		Context->GoalTagValueGetter->Capture(Settings->GoalTagAttribute);
		if (!Context->GoalTagValueGetter->SoftGrab(*Context->GoalsPoints))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Missing specified Attribute to Tag on Goal points."));
			return false;
		}
	}

	Context->SeedForwardHandler = new PCGExDataBlending::FDataForwardHandler(&Settings->SeedForwardAttributes, Context->SeedsPoints);
	Context->GoalForwardHandler = new PCGExDataBlending::FDataForwardHandler(&Settings->GoalForwardAttributes, Context->GoalsPoints);

	Context->FuseDistance = Settings->FuseDistance * Settings->FuseDistance;

	Context->OutputPaths = new PCGExData::FPointIOCollection();
	Context->OutputPaths->DefaultOutputLabel = PCGExGraph::OutputPathsLabel;

	// Prepare path queries

	Context->GoalPicker->PrepareForData(Context->SeedsPoints, Context->GoalsPoints);
	PCGExPathfinding::ProcessGoals(
		Context->SeedsPoints, Context->GoalPicker,
		[&](const int32 SeedIndex, const int32 GoalIndex)
		{
			Context->PathQueries.Add(
				new PCGExPathfinding::FPathQuery(
					SeedIndex, Context->SeedsPoints->GetInPoint(SeedIndex).Transform.GetLocation(),
					GoalIndex, Context->GoalsPoints->GetInPoint(GoalIndex).Transform.GetLocation()));
		});

	return true;
}

bool FPCGExPathfindingNavmeshElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfindingNavmeshElement::Execute);

	PCGEX_CONTEXT(PathfindingNavmesh)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->AdvancePointsIO();
		Context->GoalPicker->PrepareForData(Context->CurrentIO, Context->GoalsPoints);
		Context->SetState(PCGExMT::State_ProcessingPoints);
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto NavClusterTask = [&](const int32 SeedIndex, const int32 GoalIndex)
		{
			const int32 PathIndex = Context->PathQueries.Add(
				new PCGExPathfinding::FPathQuery(
					SeedIndex, Context->CurrentIO->GetInPoint(SeedIndex).Transform.GetLocation(),
					GoalIndex, Context->GoalsPoints->GetInPoint(GoalIndex).Transform.GetLocation()));

			Context->GetAsyncManager()->Start<FSampleNavmeshTask>(PathIndex, Context->CurrentIO, &Context->PathQueries);
		};

		PCGExPathfinding::ProcessGoals(Context->CurrentIO, Context->GoalPicker, NavClusterTask);
		Context->SetAsyncState(PCGExGraph::State_Pathfinding);
	}

	if (Context->IsState(PCGExGraph::State_Pathfinding))
	{
		PCGEX_ASYNC_WAIT

		Context->OutputPaths->OutputTo(Context);
		Context->Done();
		Context->ExecuteEnd();
	}

	return Context->IsDone();
}

bool FSampleNavmeshTask::ExecuteTask()
{
	FPCGExPathfindingNavmeshContext* Context = static_cast<FPCGExPathfindingNavmeshContext*>(Manager->Context);
	PCGEX_SETTINGS(PathfindingNavmesh)

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Context->World);

	if (!NavSys || !NavSys->GetDefaultNavDataInstance()) { return false; }

	PCGExPathfinding::FPathQuery* Query = (*Queries)[TaskIndex];

	const FPCGPoint* Seed = Context->CurrentIO->TryGetInPoint(Query->SeedIndex);
	const FPCGPoint* Goal = Context->GoalsPoints->TryGetInPoint(Query->GoalIndex);

	if (!Seed || !Goal) { return false; }

	FPathFindingQuery PathFindingQuery = FPathFindingQuery(
		Context->World, *NavSys->GetDefaultNavDataInstance(),
		Query->SeedPosition, Query->GoalPosition, nullptr, nullptr,
		TNumericLimits<FVector::FReal>::Max(),
		Context->bRequireNavigableEndLocation);

	PathFindingQuery.NavAgentProperties = Context->NavAgentProperties;

	const FPathFindingResult Result = NavSys->FindPathSync(
		Context->NavAgentProperties, PathFindingQuery,
		Context->PathfindingMode == EPCGExPathfindingNavmeshMode::Regular ? EPathFindingMode::Type::Regular : EPathFindingMode::Type::Hierarchical);

	if (Result.Result != ENavigationQueryResult::Type::Success) { return false; } ///


	const TArray<FNavPathPoint>& Points = Result.Path->GetPathPoints();

	TArray<FVector> PathLocations;
	PathLocations.Reserve(Points.Num());

	PathLocations.Add(Query->SeedPosition);
	for (const FNavPathPoint& PathPoint : Points) { PathLocations.Add(PathPoint.Location); }
	PathLocations.Add(Query->GoalPosition);

	PCGExMath::FPathMetricsSquared Metrics = PCGExMath::FPathMetricsSquared(PathLocations[0]);
	int32 FuseCountReduce = Settings->bAddGoalToPath ? 2 : 1;
	for (int i = Settings->bAddSeedToPath; i < PathLocations.Num(); i++)
	{
		FVector CurrentLocation = PathLocations[i];
		if (i > 0 && i < (PathLocations.Num() - FuseCountReduce))
		{
			if (Metrics.IsLastWithinRange(CurrentLocation, Context->FuseDistance))
			{
				PathLocations.RemoveAt(i);
				i--;
				continue;
			}
		}

		Metrics.Add(CurrentLocation);
	}

	if (PathLocations.Num() <= 2) { return false; } //


	const int32 NumPositions = PathLocations.Num();
	const int32 LastPosition = NumPositions - 1;

	PCGExData::FPointIO* PathPoints = Context->OutputPaths->Emplace_GetRef(PointIO, PCGExData::EInit::NewOutput);
	UPCGPointData* OutData = PathPoints->GetOut();
	TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
	MutablePoints.SetNumUninitialized(NumPositions);

	FVector Location;
	for (int i = 0; i < LastPosition; i++)
	{
		Location = PathLocations[i];
		(MutablePoints[i] = *Seed).Transform.SetLocation(Location);
	}

	Location = PathLocations[LastPosition];
	(MutablePoints[LastPosition] = *Goal).Transform.SetLocation(Location);

	PCGExDataBlending::FMetadataBlender* TempBlender =
		Context->Blending->CreateBlender(PathPoints, Context->GoalsPoints);

	TArrayView<FPCGPoint> View(MutablePoints);
	Context->Blending->BlendSubPoints(View, Metrics, TempBlender);

	if (GetDefault<UPCGExGlobalSettings>()->IsSmallPointSize(MutablePoints.Num())) { TempBlender->Write(); }
	else { TempBlender->Write(Manager); }

	PCGEX_DELETE(TempBlender)

	if (!Settings->bAddSeedToPath) { MutablePoints.RemoveAt(0); }
	if (!Settings->bAddGoalToPath) { MutablePoints.Pop(); }

	if (Settings->bUseSeedAttributeToTagPath) { PathPoints->Tags->RawTags.Add(Context->SeedTagValueGetter->SoftGet(*Seed, TEXT(""))); }
	if (Settings->bUseGoalAttributeToTagPath) { PathPoints->Tags->RawTags.Add(Context->GoalTagValueGetter->SoftGet(*Goal, TEXT(""))); }

	Context->SeedForwardHandler->Forward(Query->SeedIndex, PathPoints);
	Context->GoalForwardHandler->Forward(Query->GoalIndex, PathPoints);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

﻿// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetStaging/PCGExAssetStaging.h"

#define LOCTEXT_NAMESPACE "PCGExAssetStagingElement"
#define PCGEX_NAMESPACE AssetStaging

PCGExData::EIOInit UPCGExAssetStagingSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_INITIALIZE_ELEMENT(AssetStaging)

TArray<FPCGPinProperties> UPCGExAssetStagingSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	if (CollectionSource == EPCGExCollectionSource::AttributeSet)
	{
		PCGEX_PIN_PARAM(PCGExAssetCollection::SourceAssetCollection, "Attribute set to be used as collection.", Required, {})
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExAssetStagingSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	if (OutputMode == EPCGExStagingOutputMode::CollectionMap)
	{
		PCGEX_PIN_PARAM(PCGExStaging::OutputCollectionMapLabel, "Collection map generated by a staging node.", Required, {})
	}
	return PinProperties;
}

bool FPCGExAssetStagingElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(AssetStaging)

	if (Settings->CollectionSource == EPCGExCollectionSource::Asset)
	{
		Context->MainCollection = Settings->AssetCollection.LoadSynchronous();
		if (!Context->MainCollection)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Missing asset collection."));
			return false;
		}
	}
	else
	{
		if (Settings->OutputMode == EPCGExStagingOutputMode::CollectionMap)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Collection Map output is not supported with collections built from attribute sets."));
			return false;
		}

		Context->MainCollection = Settings->AttributeSetDetails.TryBuildCollection(Context, PCGExAssetCollection::SourceAssetCollection, false);
		if (!Context->MainCollection)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to build collection from attribute set."));
			return false;
		}
	}

	PCGEX_VALIDATE_NAME(Settings->AssetPathAttributeName)

	if (Settings->WeightToAttribute == EPCGExWeightOutputMode::Raw || Settings->WeightToAttribute == EPCGExWeightOutputMode::Normalized)
	{
		PCGEX_VALIDATE_NAME(Settings->WeightAttributeName)
	}

	if (Settings->OutputMode == EPCGExStagingOutputMode::CollectionMap)
	{
		Context->CollectionPickDatasetPacker = MakeShared<PCGExStaging::FPickPacker>(Context);
	}

	return true;
}


void FPCGExAssetStagingContext::RegisterAssetDependencies()
{
	FPCGExPointsProcessorContext::RegisterAssetDependencies();

	PCGEX_SETTINGS_LOCAL(AssetStaging)

	if (Settings->CollectionSource == EPCGExCollectionSource::AttributeSet)
	{
		MainCollection->GetAssetPaths(RequiredAssets, PCGExAssetCollection::ELoadingFlags::Recursive);
	}
	else
	{
		MainCollection->GetAssetPaths(RequiredAssets, PCGExAssetCollection::ELoadingFlags::RecursiveCollectionsOnly);
	}
}


void FPCGExAssetStagingElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	FPCGExPointsProcessorElement::PostLoadAssetsDependencies(InContext);

	PCGEX_CONTEXT_AND_SETTINGS(AssetStaging)

	if (Settings->CollectionSource == EPCGExCollectionSource::AttributeSet)
	{
		// Internal collection, assets have been loaded at this point
		Context->MainCollection->RebuildStagingData(true);
	}
}

bool FPCGExAssetStagingElement::PostBoot(FPCGExContext* InContext) const
{
	PCGEX_CONTEXT_AND_SETTINGS(AssetStaging)

	Context->MainCollection->LoadCache();

	return FPCGExPointsProcessorElement::PostBoot(InContext);
}

bool FPCGExAssetStagingElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAssetStagingElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(AssetStaging)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExAssetStaging::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExAssetStaging::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = Settings->bPruneEmptyPoints;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	if (Settings->OutputMode == EPCGExStagingOutputMode::CollectionMap)
	{
		UPCGParamData* OutputSet = NewObject<UPCGParamData>();
		Context->CollectionPickDatasetPacker->PackToDataset(OutputSet);

		FPCGTaggedData& OutData = Context->OutputData.TaggedData.Emplace_GetRef();
		OutData.Pin = PCGExStaging::OutputCollectionMapLabel;
		OutData.Data = OutputSet;
	}

	return Context->TryComplete();
}

namespace PCGExAssetStaging
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExAssetStaging::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		FittingHandler.ScaleToFit = Settings->ScaleToFit;
		FittingHandler.Justification = Settings->Justification;

		if (!FittingHandler.Init(ExecutionContext, PointDataFacade)) { return false; }

		Variations = Settings->Variations;
		Variations.Init(Settings->Seed);

		NumPoints = PointDataFacade->GetNum();

		Helper = MakeUnique<PCGExAssetCollection::TDistributionHelper<UPCGExAssetCollection, FPCGExAssetCollectionEntry>>(Context->MainCollection, Settings->DistributionSettings);
		if (!Helper->Init(ExecutionContext, PointDataFacade)) { return false; }

		bOutputWeight = Settings->WeightToAttribute != EPCGExWeightOutputMode::NoOutput;
		bNormalizedWeight = Settings->WeightToAttribute != EPCGExWeightOutputMode::Raw;
		bOneMinusWeight = Settings->WeightToAttribute == EPCGExWeightOutputMode::NormalizedInverted || Settings->WeightToAttribute == EPCGExWeightOutputMode::NormalizedInvertedToDensity;

		if (Settings->WeightToAttribute == EPCGExWeightOutputMode::Raw)
		{
			WeightWriter = PointDataFacade->GetWritable<int32>(Settings->WeightAttributeName, PCGExData::EBufferInit::New);
		}
		else if (Settings->WeightToAttribute == EPCGExWeightOutputMode::Normalized)
		{
			NormalizedWeightWriter = PointDataFacade->GetWritable<double>(Settings->WeightAttributeName, PCGExData::EBufferInit::New);
		}

		if (Settings->OutputMode == EPCGExStagingOutputMode::Attributes)
		{
			bInherit = PointDataFacade->GetIn()->Metadata->HasAttribute(Settings->AssetPathAttributeName);
#if PCGEX_ENGINE_VERSION > 503
			PathWriter = PointDataFacade->GetWritable<FSoftObjectPath>(Settings->AssetPathAttributeName, bInherit ? PCGExData::EBufferInit::Inherit : PCGExData::EBufferInit::New);
#else
			PathWriter = PointDataFacade->GetWritable<FString>(Settings->AssetPathAttributeName, bInherit ? PCGExData::EBufferInit::Inherit : PCGExData::EBufferInit::New);
#endif
		}
		else
		{
			bInherit = PointDataFacade->GetIn()->Metadata->HasAttribute(PCGExStaging::Tag_EntryIdx);
			HashWriter = PointDataFacade->GetWritable<int64>(PCGExStaging::Tag_EntryIdx, bInherit ? PCGExData::EBufferInit::Inherit : PCGExData::EBufferInit::New);
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope)
	{
		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		auto InvalidPoint = [&]()
		{
			if (bInherit) { return; }

			if (Settings->bPruneEmptyPoints)
			{
				Point.MetadataEntry = -2;
				return;
			}

			if (PathWriter)
			{
#if PCGEX_ENGINE_VERSION > 503
				PathWriter->GetMutable(Index) = FSoftObjectPath{};
#else
				PathWriter->GetMutable(Index) = TEXT("");
#endif
			}
			else
			{
				HashWriter->GetMutable(Index) = -1;
			}

			if (bOutputWeight)
			{
				if (WeightWriter) { WeightWriter->GetMutable(Index) = -1; }
				else if (NormalizedWeightWriter) { NormalizedWeightWriter->GetMutable(Index) = -1; }
			}
		};

		if (!PointFilterCache[Index])
		{
			InvalidPoint();
			return;
		}

		const FPCGExAssetCollectionEntry* Entry = nullptr;
		const UPCGExAssetCollection* EntryHost = nullptr;

		const int32 Seed = PCGExRandom::GetSeedFromPoint(
			Helper->Details.SeedComponents, Point,
			Helper->Details.LocalSeed, Settings, Context->SourceComponent.Get());

		Helper->GetEntry(Entry, Index, Seed, EntryHost);

		if (!Entry || !Entry->Staging.Bounds.IsValid)
		{
			InvalidPoint();
			return;
		}

		if (bOutputWeight)
		{
			double Weight = bNormalizedWeight ? static_cast<double>(Entry->Weight) / static_cast<double>(Context->MainCollection->LoadCache()->WeightSum) : Entry->Weight;
			if (bOneMinusWeight) { Weight = 1 - Weight; }
			if (WeightWriter) { WeightWriter->GetMutable(Index) = Weight; }
			else if (NormalizedWeightWriter) { NormalizedWeightWriter->GetMutable(Index) = Weight; }
			else { Point.Density = Weight; }
		}

		if (PathWriter)
		{
#if PCGEX_ENGINE_VERSION > 503
			PathWriter->GetMutable(Index) = Entry->Staging.Path;
#else
			PathWriter->GetMutable(Index) = Entry->Staging.Path.ToString();
#endif
		}
		else
		{
			HashWriter->GetMutable(Index) = Context->CollectionPickDatasetPacker->GetPickIdx(EntryHost, Entry->Staging.InternalIndex);
		}

		if (Variations.bEnabledBefore) { Variations.Apply(Point, Entry->Variations, EPCGExVariationMode::Before); }

		FTransform OutTransform = Point.Transform;
		FBox OutBounds = Entry->Staging.Bounds;

		FittingHandler.ComputeTransform(Index, OutTransform, OutBounds);

		Point.BoundsMin = OutBounds.Min;
		Point.BoundsMax = OutBounds.Max;
		Point.Transform = OutTransform;

		if (Variations.bEnabledAfter) { Variations.Apply(Point, Entry->Variations, EPCGExVariationMode::After); }
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}

	void FProcessor::Write()
	{
		// TODO : Find a better solution
		TArray<FPCGPoint>& MutablePoints = PointDataFacade->GetOut()->GetMutablePoints();

		int32 WriteIndex = 0;
		for (int32 i = 0; i < NumPoints; i++) { if (MutablePoints[i].MetadataEntry != -2) { MutablePoints[WriteIndex++] = MutablePoints[i]; } }

		MutablePoints.SetNum(WriteIndex);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

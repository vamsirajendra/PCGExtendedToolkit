﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExRandomFilter.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

bool UPCGExRandomFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Config.bUseLocalCurve) { Config.LocalWeightCurve.ExternalCurve = Config.WeightCurve.Get(); }
	return Super::Init(InContext);
}

void UPCGExRandomFilterFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	if (Config.bPerPointWeight && !Config.bRemapWeightInternally) { FacadePreloader.Register<double>(InContext, Config.Weight); }
}

void UPCGExRandomFilterFactory::RegisterAssetDependencies(FPCGExContext* InContext) const
{
	Super::RegisterAssetDependencies(InContext);
	InContext->AddAssetDependency(Config.WeightCurve.ToSoftObjectPath());
}

bool UPCGExRandomFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_CONDITIONAL(Config.bPerPointWeight, Config.Weight, Consumable)

	return true;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExRandomFilterFactory::CreateFilter() const
{
	PCGEX_MAKE_SHARED(Filter, PCGExPointsFilter::FRandomFilter, this)
	Filter->WeightCurve = Config.LocalWeightCurve.GetRichCurveConst();
	return Filter;
}

bool PCGExPointsFilter::FRandomFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
{
	if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }

	Threshold = TypedFilterFactory->Config.Threshold;

	if (TypedFilterFactory->Config.bPerPointWeight)
	{
		if (TypedFilterFactory->Config.bRemapWeightInternally)
		{
			WeightBuffer = PointDataFacade->GetBroadcaster<double>(TypedFilterFactory->Config.Weight, true);
			WeightRange = WeightBuffer->Max;

			if (WeightBuffer->Min < 0)
			{
				WeightOffset = FMath::Abs(WeightBuffer->Min);
				WeightRange += WeightOffset;
			}
		}
		else
		{
			WeightBuffer = PointDataFacade->GetScopedBroadcaster<double>(TypedFilterFactory->Config.Weight);
		}

		if (!WeightBuffer)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Weight attribute: \"{0}\"."), FText::FromName(TypedFilterFactory->Config.Weight.GetName())));
			return false;
		}
	}

	return true;
}

PCGEX_CREATE_FILTER_FACTORY(Random)

#if WITH_EDITOR
FString UPCGExRandomFilterProviderSettings::GetDisplayName() const
{
	return TEXT("Random");
}
#endif


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

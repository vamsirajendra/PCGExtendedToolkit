﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Shapes/PCGExShapeProcessor.h"

#define LOCTEXT_NAMESPACE "PCGExShapeProcessorElement"

UPCGExShapeProcessorSettings::UPCGExShapeProcessorSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPCGExShapeProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	if (!IsInputless())
	{
		if (GetMainAcceptMultipleData()) { PCGEX_PIN_POINTS(GetMainInputLabel(), "The point data to be processed.", Required, {}) }
		else { PCGEX_PIN_POINT(GetMainInputLabel(), "The point data to be processed.", Required, {}) }
	}

	PCGEX_PIN_PARAMS(PCGExShapes::SourceShapeBuildersLabel, "Shape builders that will be used by this element.", Required, {})

	if (SupportsPointFilters())
	{
		if (RequiresPointFilters()) { PCGEX_PIN_PARAMS(GetPointFilterLabel(), GetPointFilterTooltip(), Required, {}) }
		else { PCGEX_PIN_PARAMS(GetPointFilterLabel(), GetPointFilterTooltip(), Normal, {}) }
	}

	return PinProperties;
}

PCGExData::EInit UPCGExShapeProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_CONTEXT(ShapeProcessor)

bool FPCGExShapeProcessorElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ShapeProcessor)

	if (!PCGExFactories::GetInputFactories(
		Context, PCGExShapes::SourceShapeBuildersLabel, Context->BuilderFactories,
		{PCGExFactories::EType::ShapeBuilder}, true))
	{
		return false;
	}

	return true;
}


#undef LOCTEXT_NAMESPACE

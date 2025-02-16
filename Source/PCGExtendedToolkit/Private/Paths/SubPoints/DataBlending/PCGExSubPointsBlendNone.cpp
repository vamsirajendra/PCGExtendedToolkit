﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendNone.h"

#include "Data/Blending/PCGExMetadataBlender.h"


void UPCGExSubPointsBlendNone::BlendSubPoints(
	const PCGExData::FPointRef& From,
	const PCGExData::FPointRef& To,
	const TArrayView<FPCGPoint>& SubPoints,
	const PCGExPaths::FPathMetrics& Metrics,
	PCGExDataBlending::FMetadataBlender* InBlender, const int32 StartIndex) const
{
}

TSharedPtr<PCGExDataBlending::FMetadataBlender> UPCGExSubPointsBlendNone::CreateBlender(
	const TSharedRef<PCGExData::FFacade>& InPrimaryFacade,
	const TSharedRef<PCGExData::FFacade>& InSecondaryFacade,
	const PCGExData::ESource SecondarySource, const TSet<FName>* IgnoreAttributeSet)
{
	PCGEX_MAKE_SHARED(NewBlender, PCGExDataBlending::FMetadataBlender, &BlendingDetails)
	NewBlender->PrepareForData(InPrimaryFacade, InSecondaryFacade, SecondarySource);
	return NewBlender;
}

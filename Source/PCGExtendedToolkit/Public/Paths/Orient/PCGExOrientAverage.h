﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOrientOperation.h"
#include "PCGExOrientAverage.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Average")
class PCGEXTENDEDTOOLKIT_API UPCGExOrientAverage : public UPCGExOrientOperation
{
	GENERATED_BODY()

public:
	virtual void Orient(PCGEx::FPointRef& Point, const PCGEx::FPointRef& Previous, const PCGEx::FPointRef& Next, const double Factor) const override;
	
};

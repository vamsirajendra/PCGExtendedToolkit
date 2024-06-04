﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampling.h"
#include "PCGExSettings.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Geometry/PCGExGeo.h"

#include "PCGExSampleProjectedNearestPoint.generated.h"

#define PCGEX_FOREACH_FIELD_PROJECTNEARESTPOINT(MACRO)\
MACRO(Success, bool)\
MACRO(Transform, FTransform)\
MACRO(LookAtTransform, FTransform)\
MACRO(Distance, double)\
MACRO(SignedDistance, double)\
MACRO(Angle, double)\
MACRO(NumSamples, int32)

namespace PCGExDataBlending
{
	class FMetadataBlender;
	struct FPropertiesBlender;
}

class UPCGExFilterFactoryBase;

namespace PCGExDataFilter
{
	class TEarlyExitFilterManager;
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExSampleProjectedNearestPointSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExSampleProjectedNearestPointSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleProjectedNearestPoint, "Sample : Projected Nearest Point", "Sample nearest target points on a projection plane");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExEditorSettings>()->NodeColorSampler; }
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionSettings ProjectionSettings;

	/** Sampling method.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	EPCGExSampleMethod SampleMethod = EPCGExSampleMethod::WithinRange;

	/** Minimum target range. Used as fallback if LocalRangeMin is enabled but missing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, ClampMin=0))
	double RangeMin = 0;

	/** Maximum target range. Used as fallback if LocalRangeMax is enabled but missing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, ClampMin=0))
	double RangeMax = 300;

	/** Use a per-point minimum range*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalRangeMin = false;

	/** Attribute or property to read the minimum range from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, EditCondition="bUseLocalRangeMin", EditConditionHides))
	FPCGAttributePropertyInputSelector LocalRangeMin;

	/** Use a per-point maximum range*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalRangeMax = false;

	/** Attribute or property to read the maximum range from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable, EditCondition="bUseLocalRangeMax", EditConditionHides))
	FPCGAttributePropertyInputSelector LocalRangeMax;

	/** Distance method to be used for source & target points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	FPCGExDistanceSettings DistanceSettings;

	/** Weight method used for blending */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable))
	EPCGExRangeType WeightMethod = EPCGExRangeType::FullRange;

	/** Curve that balances weight over distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable))
	TSoftObjectPtr<UCurveFloat> WeightOverDistance;

	/** Attributes to sample from the targets */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Blending", meta=(PCG_Overridable))
	TMap<FName, EPCGExDataBlendingType> TargetAttributes;

	/** Write the sampled distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Blending", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bBlendPointProperties = false;

	/** The constant to use as Up vector for the look at transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Blending", meta=(PCG_Overridable, EditCondition="bBlendPointProperties"))
	FPCGExPropertiesBlendingSettings PointPropertiesBlendingSettings = FPCGExPropertiesBlendingSettings(EPCGExDataBlendingType::None);

	/** Write whether the sampling was sucessful or not to a boolean attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteSuccess = false;

	/** Name of the 'boolean' attribute to write sampling success to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteSuccess"))
	FName SuccessAttributeName = FName("bSamplingSuccess");

	/** Write the sampled transform. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteTransform = false;

	/** Name of the 'transform' attribute to write sampled Transform to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteTransform"))
	FName TransformAttributeName = FName("WeightedTransform");


	/** Write the sampled transform. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteLookAtTransform = false;

	/** Name of the 'transform' attribute to write sampled Transform to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteLookAtTransform"))
	FName LookAtTransformAttributeName = FName("WeightedLookAt");

	/** The axis to align transform the look at vector to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Align", EditCondition="bWriteLookAtTransform", EditConditionHides))
	EPCGExAxisAlign LookAtAxisAlign = EPCGExAxisAlign::Forward;

	/** Up vector source.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Use Up from...", EditCondition="bWriteLookAtTransform", EditConditionHides))
	EPCGExSampleSource LookAtUpSelection;

	/** The attribute or property on selected source to use as Up vector for the look at transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Up Vector", EditCondition="bWriteLookAtTransform && LookAtUpSelection!=EPCGExSampleSource::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector LookAtUpSource;

	/** The constant to use as Up vector for the look at transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Up Vector", EditCondition="bWriteLookAtTransform && LookAtUpSelection==EPCGExSampleSource::Constant", EditConditionHides))
	FVector LookAtUpConstant = FVector::UpVector;


	/** Write the sampled distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDistance = false;

	/** Name of the 'double' attribute to write sampled distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteDistance"))
	FName DistanceAttributeName = FName("WeightedDistance");

	/** Write the sampled Signed distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteSignedDistance = false;

	/** Name of the 'double' attribute to write sampled Signed distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteSignedDistance"))
	FName SignedDistanceAttributeName = FName("WeightedSignedDistance");

	/** Axis to use to calculate the distance' sign*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Axis", EditCondition="bWriteSignedDistance", EditConditionHides))
	EPCGExAxis SignAxis = EPCGExAxis::Forward;

	/** Write the sampled angle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteAngle = false;

	/** Name of the 'double' attribute to write sampled Signed distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteAngle"))
	FName AngleAttributeName = FName("WeightedAngle");

	/** Axis to use to calculate the angle*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Axis", EditCondition="bWriteAngle", EditConditionHides))
	EPCGExAxis AngleAxis = EPCGExAxis::Forward;

	/** Unit/range to output the angle to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, DisplayName=" └─ Range", EditCondition="bWriteAngle", EditConditionHides))
	EPCGExAngleRange AngleRange = EPCGExAngleRange::PIRadians;

	/** Write the sampled distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNumSamples = false;

	/** Name of the 'int32' attribute to write the number of sampled neighbors to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteNumSamples"))
	FName NumSamplesAttributeName = FName("NumSamples");
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSampleProjectedNearestPointContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExSampleProjectedNearestPointElement;

	using PointOctree = TOctree2<FPCGPointRef, FPCGPointRefSemantics>;

	virtual ~FPCGExSampleProjectedNearestPointContext() override;

	PCGExData::FPointIO* Targets = nullptr;

	TArray<UPCGExFilterFactoryBase*> PointFilterFactories;
	PCGExDataFilter::TEarlyExitFilterManager* PointFilterManager = nullptr;

	TArray<UPCGExFilterFactoryBase*> ValueFilterFactories;
	PCGExDataFilter::TEarlyExitFilterManager* ValueFilterManager = nullptr;

	TArray<FPCGPoint> ProjectedSourceIO;
	TArray<FPCGPoint> ProjectedTargetIO;

	PCGEx::FLocalSingleFieldGetter* RangeMinGetter;
	PCGEx::FLocalSingleFieldGetter* RangeMaxGetter;
	PCGEx::FLocalVectorGetter* LookAtUpGetter;

	FVector SafeUpVector = FVector::UpVector;

	TObjectPtr<UCurveFloat> WeightCurve = nullptr;

	FPCGExBlendingSettings BlendingSettings;
	PCGExDataBlending::FMetadataBlender* Blender = nullptr;

	PCGEX_FOREACH_FIELD_PROJECTNEARESTPOINT(PCGEX_OUTPUT_DECL)

	FPCGExGeo2DProjectionSettings ProjectionSettings;
	PointOctree* ProjectedTargetOctree = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSampleProjectedNearestPointElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSampleProjectedPointTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExSampleProjectedPointTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};

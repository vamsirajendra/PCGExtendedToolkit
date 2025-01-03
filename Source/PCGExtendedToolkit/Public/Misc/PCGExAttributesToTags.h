﻿// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExDataForward.h"


#include "PCGExAttributesToTags.generated.h"

UENUM()
enum class EPCGExAttributeToTagsResolution : uint8
{
	Self      = 0 UMETA(DisplayName = "Self", ToolTip="Matches a single entry to each input collection, from itself"),
	EntryToCollection      = 1 UMETA(DisplayName = "Entry to Collection", ToolTip="Matches a Source entries to each input collection"),
	CollectionToCollection = 2 UMETA(DisplayName = "Collection to Collection", ToolTip="Matches a single entry per source to matching collection (requires the same number of collections in both pins)"),
};

UENUM()
enum class EPCGExCollectionEntrySelection : uint8
{
	FirstIndex  = 0 UMETA(DisplayName = "First Entry", ToolTip="Uses the first entry in the matching collection"),
	LastIndex   = 1 UMETA(DisplayName = "Last Entry", ToolTip="Uses the last entry in the matching collection"),
	RandomIndex = 2 UMETA(DisplayName = "Random Entry", ToolTip="Uses a random entry in the matching collection")
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExAttributesToTagsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(AttributesToTags, "Attributes to Tags", "Use point attributes or set to tag the data.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscWrite; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Resolution mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExAttributeToTagsResolution Resolution = EPCGExAttributeToTagsResolution::EntryToCollection;
	
	/** Selection mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Resolution != EPCGExAttributeToTagsResolution::EntryToCollection"))
	EPCGExCollectionEntrySelection Selection = EPCGExCollectionEntrySelection::FirstIndex;

	/** If enabled, prefix the attribute value with the attribute name  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bPrefixWithAttributeName = true;

	/** Attributes which value will be used as tags. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FPCGAttributePropertyInputSelector> Attributes;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAttributesToTagsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExAttributesToTagsElement;
	TArray<TSharedPtr<PCGExData::FFacade>> SourceDataFacades;
	TArray<FPCGExAttributeToTagDetails> Details;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAttributesToTagsElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExAttributesToTags
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExAttributesToTagsContext, UPCGExAttributesToTagsSettings>
	{
	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
	};
}

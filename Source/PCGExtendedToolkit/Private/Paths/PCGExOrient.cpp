﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExOrient.h"

#include "Paths/Orient/PCGExOrientAverage.h"

#define LOCTEXT_NAMESPACE "PCGExOrientElement"
#define PCGEX_NAMESPACE Orient

PCGEX_INITIALIZE_ELEMENT(Orient)

FName UPCGExOrientSettings::GetPointFilterLabel() const { return FName("FlipOrientationConditions"); }

bool FPCGExOrientElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Orient)

	if (!Settings->Orientation)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Please select an orientation module in the detail panel."));
		return false;
	}

	PCGEX_OPERATION_BIND(Orientation, UPCGExOrientAverage)
	Context->Orientation->bClosedPath = Settings->bClosedPath;
	Context->Orientation->OrientAxis = Settings->OrientAxis;
	Context->Orientation->UpAxis = Settings->UpAxis;

	return true;
}

bool FPCGExOrientElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExOrientElement::Execute);

	PCGEX_CONTEXT(Orient)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExOrient::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bInvalidInputs = true;
					Entry->InitializeOutput(PCGExData::EInit::Forward);
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExOrient::FProcessor>* NewBatch)
			{
				NewBatch->PrimaryOperation = Context->Orientation;
				NewBatch->SetPointsFilterData(&Context->FilterFactories);
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any paths to orient."));
			return true;
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->OutputMainPoints();
	Context->Done();
	Context->ExecuteEnd();

	return true;
}

namespace PCGExOrient
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(Orient)

		DefaultPointFilterValue = Settings->bFlipDirection;
		
		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LastIndex = PointIO->GetNum() - 1;
		Orient = Cast<UPCGExOrientOperation>(PrimaryOperation);
		Orient->PrepareForData(PointIO);

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point)
	{
		PCGEX_SETTINGS(Orient)

		PCGEx::FPointRef Current = PointIO->GetOutPointRef(Index);
		if (Orient->bClosedPath)
		{
			const PCGEx::FPointRef Previous = Index == 0 ? PointIO->GetInPointRef(LastIndex) : PointIO->GetInPointRef(Index - 1);
			const PCGEx::FPointRef Next = Index == LastIndex ? PointIO->GetInPointRef(0) : PointIO->GetInPointRef(Index + 1);
			Orient->Orient(Current, Previous, Next, PointFilterCache[Index] ? -1 : 1);
		}
		else
		{
			const PCGEx::FPointRef Previous = Index == 0 ? Current : PointIO->GetInPointRef(Index - 1);
			const PCGEx::FPointRef Next = Index == LastIndex ? PointIO->GetInPointRef(LastIndex) : PointIO->GetInPointRef(Index + 1);
			Orient->Orient(Current, Previous, Next, PointFilterCache[Index] ? -1 : 1);
		}
	}

	void FProcessor::CompleteWork()
	{
	}

	void FProcessor::Write()
	{
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

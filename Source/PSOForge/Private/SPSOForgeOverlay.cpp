// Copyright (c) 2026 PSOForge contributors. MIT License.

#include "SPSOForgeOverlay.h"
#include "PSOForgeSubsystem.h"

#include "Materials/MaterialInterface.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SPSOForgeOverlay::Construct(const FArguments& InArgs, UPSOForgeSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;

	ChildSlot
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Top)
	.Padding(24.0f)
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f))
		.Padding(12.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("PSOForge Capture")))
				.ColorAndOpacity(FLinearColor(1.0f, 0.8f, 0.2f))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(this, &SPSOForgeOverlay::GetStatusText)
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SAssignNew(UIMaterialStrip, SHorizontalBox)
			]
		]
	];
}

FText SPSOForgeOverlay::GetStatusText() const
{
	if (const UPSOForgeSubsystem* Pinned = Subsystem.Get())
	{
		return FText::FromString(Pinned->GetStatusString());
	}
	return FText::GetEmpty();
}

void SPSOForgeOverlay::SetUIMaterials(const TArray<UMaterialInterface*>& Materials)
{
	if (!UIMaterialStrip.IsValid())
	{
		return;
	}

	UIMaterialStrip->ClearChildren();
	UIMaterialBrushes.Reset();

	for (UMaterialInterface* Material : Materials)
	{
		if (!Material)
		{
			continue;
		}

		TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
		Brush->SetResourceObject(Material);
		Brush->ImageSize = FVector2D(48.0f, 48.0f);
		UIMaterialBrushes.Add(Brush);

		UIMaterialStrip->AddSlot()
		.AutoWidth()
		.Padding(2.0f)
		[
			SNew(SImage).Image(Brush.Get())
		];
	}
}

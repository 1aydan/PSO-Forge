// Copyright (c) 2026 PSOForge contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/SCompoundWidget.h"

class UPSOForgeSubsystem;
class UMaterialInterface;
class SHorizontalBox;

/**
 * Minimal viewport overlay showing capture progress. Also renders the current batch's UI-domain
 * materials as small images so their Slate shaders are drawn (and their PSOs created) at least once.
 */
class SPSOForgeOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPSOForgeOverlay) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UPSOForgeSubsystem* InSubsystem);

	void SetUIMaterials(const TArray<UMaterialInterface*>& Materials);

private:
	FText GetStatusText() const;

	TWeakObjectPtr<UPSOForgeSubsystem> Subsystem;
	TSharedPtr<SHorizontalBox> UIMaterialStrip;
	TArray<TSharedPtr<FSlateBrush>> UIMaterialBrushes;
};

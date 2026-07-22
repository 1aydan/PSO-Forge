// Copyright (c) 2026 PSOForge contributors. MIT License.

#include "PSOForgeWarmupWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "ShaderPipelineCache.h"

UPSOForgeWarmupWidget::UPSOForgeWarmupWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPSOForgeWarmupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CurrentProgress = 0.0f;
	RemainingPrecompiles = 0;
	ElapsedSeconds = 0.0f;
	bCompilationCompleted = false;

	ApplyVisuals();

	if (bAutoStartCompilation)
	{
		StartShaderCompile();
	}
}

void UPSOForgeWarmupWidget::NativeDestruct()
{
	if (bCompilationActive && !bCompilationCompleted)
	{
		FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Background);
	}

	Super::NativeDestruct();
}

void UPSOForgeWarmupWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bCompilationActive || bCompilationCompleted)
	{
		return;
	}

	ElapsedSeconds += InDeltaTime;
	UpdateProgress();

	const bool bMinDisplayElapsed = ElapsedSeconds >= MinDisplaySeconds;
	const bool bPrecompilesComplete = RemainingPrecompiles <= 0;
	const bool bTimedOut = MaxCompilationTimeSeconds > 0.0f && ElapsedSeconds >= MaxCompilationTimeSeconds;

	if ((bPrecompilesComplete && bMinDisplayElapsed) || bTimedOut)
	{
		CompleteShaderCompile();
	}
}

void UPSOForgeWarmupWidget::StartShaderCompile()
{
	bCompilationActive = true;
	bCompilationCompleted = false;
	ElapsedSeconds = 0.0f;

	FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Fast);
	FShaderPipelineCache::ResumeBatching();

	RemainingPrecompiles = FShaderPipelineCache::NumPrecompilesRemaining();
	InitialPrecompileCount = FMath::Max(1, RemainingPrecompiles);
	CurrentProgress = RemainingPrecompiles > 0 ? 0.0f : 1.0f;

	ApplyVisuals();
	OnShaderProgressChanged.Broadcast(CurrentProgress, RemainingPrecompiles);
	HandleCompilationProgressChanged(CurrentProgress, RemainingPrecompiles);
}

void UPSOForgeWarmupWidget::CompleteShaderCompile()
{
	if (bCompilationCompleted)
	{
		return;
	}

	bCompilationCompleted = true;
	bCompilationActive = false;
	RemainingPrecompiles = 0;
	CurrentProgress = 1.0f;

	FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Background);

	ApplyVisuals();
	HandleCompilationProgressChanged(CurrentProgress, RemainingPrecompiles);
	HandleCompilationCompleted();
	OnShaderCompileCompleted.Broadcast();

	if (bOpenLevelOnComplete && !NextLevelName.IsNone())
	{
		UGameplayStatics::OpenLevel(this, NextLevelName);
	}
}

void UPSOForgeWarmupWidget::UpdateProgress()
{
	RemainingPrecompiles = FShaderPipelineCache::NumPrecompilesRemaining();
	CurrentProgress = 1.0f - (static_cast<float>(RemainingPrecompiles) / static_cast<float>(InitialPrecompileCount));
	CurrentProgress = FMath::Clamp(CurrentProgress, 0.0f, 1.0f);

	ApplyVisuals();
	OnShaderProgressChanged.Broadcast(CurrentProgress, RemainingPrecompiles);
	HandleCompilationProgressChanged(CurrentProgress, RemainingPrecompiles);
}

void UPSOForgeWarmupWidget::ApplyVisuals() const
{
	if (PB_ShaderProgress)
	{
		PB_ShaderProgress->SetPercent(CurrentProgress);
		PB_ShaderProgress->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	if (TXT_ShaderProgress)
	{
		const int32 RoundedPercent = FMath::Clamp(FMath::RoundToInt(CurrentProgress * 100.0f), 0, 100);
		TXT_ShaderProgress->SetText(FText::Format(ProgressTextFormat, FText::AsNumber(RoundedPercent), FText::AsNumber(RemainingPrecompiles)));
		TXT_ShaderProgress->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	
	if (TXT_ElapsedSeconds)
	{
		TXT_ElapsedSeconds->SetText(FText::AsTimespan(ElapsedSeconds));
		TXT_ElapsedSeconds->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

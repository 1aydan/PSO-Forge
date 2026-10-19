// Copyright (c) 2026 PSOForge contributors. MIT License.

#include "PSOForgeWarmupWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "PipelineStateCache.h"
#include "ShaderPipelineCache.h"

DEFINE_LOG_CATEGORY_STATIC(LogPSOForgeWarmup, Log, All);

bool UPSOForgeWarmupWidget::IsBundledCacheOnPrecacheThreadPool()
{
	// Mirrors UsePSOPrecachingThreadPool() in ShaderPipelineCache.cpp.
	static const IConsoleVariable* CVarUsePrecachingThreadPool =
		IConsoleManager::Get().FindConsoleVariable(TEXT("r.ShaderPipelineCache.UsePrecachingThreadPool"));
	return PipelineStateCache::IsPSOPrecachingEnabled()
		&& CVarUsePrecachingThreadPool != nullptr
		&& CVarUsePrecachingThreadPool->GetInt() != 0;
}

int32 UPSOForgeWarmupWidget::GetRemainingPreCompiles()
{
	const int32 Total = static_cast<int32>(FShaderPipelineCache::NumPrecompilesRemaining());

	// On the thread pool the bundled cache's compiles only show up as precache requests, so keep them.
	if (IsBundledCacheOnPrecacheThreadPool())
	{
		return Total;
	}

	// Runtime precache requests keep arriving and would hold the warmup open after the bundled cache is done.
	const int32 RuntimePrecache = static_cast<int32>(PipelineStateCache::NumActivePrecacheRequests());
	return FMath::Max(0, Total - RuntimePrecache);
}

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
	// The count is floored at 1 while precompiling, so the flag has to gate the drain too.
	const bool bPrecompilesComplete = !bPipelineCachePrecompiling && RemainingPrecompiles <= 0;
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
	InitialPrecompileCount = 0;
	CurrentProgress = 0.0f;

	FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Fast);
	FShaderPipelineCache::ResumeBatching();

	// The cache opens on the render thread, so the count still reads 0 here. UpdateProgress() baselines
	// InitialPrecompileCount once real work shows up.
	RemainingPrecompiles = GetRemainingPreCompiles();
	bPipelineCachePrecompiling = FShaderPipelineCache::IsPrecompiling();

	// On the thread pool the calls above do nothing; the bundled cache was usually handed off during engine init.
	static bool bWarnedThreadPoolHandOff = false;
	if (!bWarnedThreadPoolHandOff && FPlatformProperties::RequiresCookedData() && !bPipelineCachePrecompiling && IsBundledCacheOnPrecacheThreadPool())
	{
		bWarnedThreadPoolHandOff = true;
		UE_LOG(LogPSOForgeWarmup, Warning, TEXT("Bundled PSO cache was handed to the PSO precache thread pool during engine init, before this warmup started; ")
			TEXT("progress only covers PSOs still compiling. For full coverage set r.ShaderPipelineCache.UsePrecachingThreadPool=0 ")
			TEXT("and r.ShaderPipelineCache.StartupMode=0 so the cache waits for this widget."));
	}

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
	bPipelineCachePrecompiling = false;
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
	RemainingPrecompiles = GetRemainingPreCompiles();
	bPipelineCachePrecompiling = FShaderPipelineCache::IsPrecompiling();

	// The count climbs as later batches enqueue, so the denominator is the high-water mark.
	InitialPrecompileCount = FMath::Max(InitialPrecompileCount, RemainingPrecompiles);

	float NewProgress = 0.0f;
	if (InitialPrecompileCount > 0)
	{
		NewProgress = 1.0f - (static_cast<float>(RemainingPrecompiles) / static_cast<float>(InitialPrecompileCount));
	}
	NewProgress = FMath::Clamp(NewProgress, 0.0f, 1.0f);

	if (bPipelineCachePrecompiling || RemainingPrecompiles > 0)
	{
		NewProgress = FMath::Min(NewProgress, MaxInProgressFraction);
	}

	// Raising the baseline shrinks the ratio; plateau rather than run backwards.
	CurrentProgress = FMath::Max(CurrentProgress, NewProgress);

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
		// FTimespan's implicit int64 constructor takes ticks, so seconds must be converted explicitly.
		TXT_ElapsedSeconds->SetText(FText::AsTimespan(FTimespan::FromSeconds(ElapsedSeconds)));
		TXT_ElapsedSeconds->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

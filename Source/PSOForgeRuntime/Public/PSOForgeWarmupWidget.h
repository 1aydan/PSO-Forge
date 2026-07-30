// Copyright (c) 2026 PSOForge contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PSOForgeWarmupWidget.generated.h"

class UProgressBar;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShaderProgressChanged, float, Progress, int32, RemainingPrecompiles);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShaderCompileCompleted);

/**
 * Startup/loading-screen widget that drives the shader pipeline cache precompile pass: switches the
 * bundled PSO cache to fast batching, reports progress from the remaining precompile count, and
 * restores background batching when finished.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PSOFORGERUNTIME_API UPSOForgeWarmupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPSOForgeWarmupWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(BlueprintAssignable, Category = "Shader Compile")
	FOnShaderProgressChanged OnShaderProgressChanged;

	UPROPERTY(BlueprintAssignable, Category = "Shader Compile")
	FOnShaderCompileCompleted OnShaderCompileCompleted;

	/** Starts or restarts the warmup pass. Called automatically from NativeConstruct when enabled. */
	UFUNCTION(BlueprintCallable, Category = "Shader Compile")
	void StartShaderCompile();

	/** Finishes the warmup pass immediately, restoring background batching and optionally opening the next level. */
	UFUNCTION(BlueprintCallable, Category = "Shader Compile")
	void CompleteShaderCompile();

	UFUNCTION(BlueprintPure, Category = "Shader Compile")
	float GetShaderProgress() const { return CurrentProgress; }

	UFUNCTION(BlueprintPure, Category = "Shader Compile")
	int32 GetRemainingPrecompiles() const { return RemainingPrecompiles; }
	
	UFUNCTION(BlueprintPure, Category = "Shader Compile")
	float GetElapsedSeconds() const { return ElapsedSeconds; }

	UFUNCTION(BlueprintPure, Category = "Shader Compile")
	bool IsShaderCompileActive() const { return bCompilationActive; }

protected:
	/** Optional progress bar binding. Name the UMG ProgressBar PB_ShaderProgress to auto-wire it. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PB_ShaderProgress;

	/** Optional percent text binding. Name the UMG TextBlock TXT_ShaderProgress to auto-wire it. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_ShaderProgress;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_ElapsedSeconds;

	/** Start the shader pipeline cache pass as soon as the widget is constructed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Compile")
	bool bAutoStartCompilation = true;

	/** Automatically open NextLevelName when warmup completes or times out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Compile")
	bool bOpenLevelOnComplete = true;

	/** Destination level after the startup warmup. None disables travel even when bOpenLevelOnComplete is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Compile", meta = (EditCondition = "bOpenLevelOnComplete"))
	FName NextLevelName = NAME_None;

	/** Minimum time to keep the screen visible, even if no PSOs are pending. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Compile", meta = (ClampMin = "0.0"))
	float MinDisplaySeconds = 1.0f;

	/** Safety timeout so a bad platform/cache state cannot trap the player. <= 0 disables the timeout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Compile", meta = (ClampMin = "0.0"))
	float MaxCompilationTimeSeconds = 60.0f;

	/** Ceiling on progress while warmup runs, so the bar cannot read full before it is. 1.0 disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Compile", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MaxInProgressFraction = 0.98f;

	/** Progress text format. {0} is rounded percent, {1} is remaining precompile count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Compile|Text")
	FText ProgressTextFormat = NSLOCTEXT("PSOForgeWarmup", "ProgressTextFormat", "{0}%");
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shader Compile|Text")
	float ElapsedSeconds = 0.0f;

	/** Blueprint hook for animated/styled progress updates beyond the optional bound widgets. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Shader Compile")
	void HandleCompilationProgressChanged(float Progress, int32 Remaining);

	/** Blueprint hook before level travel or final completion broadcast. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Shader Compile")
	void HandleCompilationCompleted();

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** High-water mark of the remaining count, used as the progress denominator. */
	int32 InitialPrecompileCount = 0;
	int32 RemainingPrecompiles = 0;
	float CurrentProgress = 0.0f;
	bool bCompilationActive = false;
	bool bCompilationCompleted = false;
	/** Cached FShaderPipelineCache::IsPrecompiling(). */
	bool bPipelineCachePrecompiling = false;

	/** Remaining bundled-cache precompiles, excluding runtime PSO precache requests. */
	static int32 GetRemainingPreCompiles();

	void UpdateProgress();
	void ApplyVisuals() const;
};

// Copyright (c) 2026 PSOForge contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "Scalability.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "UObject/SoftObjectPath.h"
#include "PSOForgeSubsystem.generated.h"

class APSOForgeShowcaseActor;
class APSOForgeEnvironmentRig;
class ACameraActor;
class UMaterialInterface;
class SPSOForgeOverlay;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPSOForgeProgress, float /*Alpha*/, const FString& /*Status*/);

/**
 * Drives a PSO capture pass: batch-loads every material and Niagara system under the configured scan
 * paths, renders them on showcase geometry, and sweeps every scalability preset per batch so the draw
 * PSOs for each quality tier are created (and recorded when the process runs with -logPSO).
 *
 * Start via the PSOForge.Start console command, or launch the game with -PSOForge to auto-run and quit
 * on completion (used by Tools/Invoke-PSOForge.ps1).
 */
UCLASS()
class PSOFORGE_API UPSOForgeSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void StartCapture(bool bInAutoQuitOnComplete);
	void CancelCapture();
	bool IsCapturing() const;
	FString GetStatusString() const;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickable() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;

	FOnPSOForgeProgress OnProgressUpdated;
	FSimpleMulticastDelegate OnCaptureComplete;

private:
	enum class EPSOForgeState : uint8
	{
		Idle,
		OpeningCaptureMap,
		Scanning,
		LoadingBatch,
		SpawningShowcase,
		ApplyingPreset,
		WarmingUp,
		DespawningBatch,
		WaitingForGC,
		Finished
	};

	void EnterState(EPSOForgeState NewState);
	void TickScanning();
	void TickLoadingBatch();
	void TickSpawningShowcase();
	void TickApplyingPreset();
	void TickWarmingUp(float DeltaTime);
	void TickDespawningBatch();
	void TickWaitingForGC();
	void FinishCapture(bool bCancelled);

	void EnsureStage();
	void TearDownStage();
	void BroadcastProgress(const FString& Status);
	int32 CurrentBatchEnd() const;
	bool BatchesRemain() const;
	void HandlePostLoadMap(UWorld* LoadedWorld);
	const TCHAR* StateName() const;

	FStreamableManager StreamableManager;
	TSharedPtr<FStreamableHandle> BatchHandle;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> LoadedBatch;

	UPROPERTY(Transient)
	TObjectPtr<APSOForgeShowcaseActor> Showcase;

	UPROPERTY(Transient)
	TObjectPtr<APSOForgeEnvironmentRig> EnvironmentRig;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> CaptureCamera;

	TSharedPtr<SPSOForgeOverlay> Overlay;

	TArray<FSoftObjectPath> MaterialPaths;
	TArray<FSoftObjectPath> NiagaraPaths;

	EPSOForgeState State = EPSOForgeState::Idle;
	Scalability::FQualityLevels SavedQualityLevels;

	// Single persistent PostLoadMapWithWorld subscription, removed only in Deinitialize. Never
	// remove a delegate from inside its own broadcast: Remove() destroys the executing instance
	// and its captures, which is a use-after-free.
	FDelegateHandle PostLoadMapHandle;

	double StateEnterTimeSeconds = 0.0;
	float WarmupSecondsElapsed = 0.0f;
	int32 WarmupFramesElapsed = 0;
	int32 GCFramesElapsed = 0;
	int32 BatchStart = 0;
	int32 PresetIndex = 0;
	int32 ItemsProcessed = 0;
	int32 TotalItems = 0;
	bool bNiagaraPhase = false;
	bool bAutoQuitOnComplete = false;
	bool bAutoStartPending = false;
	bool bWaitingForMapLoad = false;
	bool bStageReady = false;
};

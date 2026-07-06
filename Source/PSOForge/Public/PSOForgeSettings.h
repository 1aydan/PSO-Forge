// Copyright (c) 2026 PSOForge contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "PSOForgeSettings.generated.h"

class UStaticMesh;
class USkeletalMesh;
class UWorld;

/**
 * Project configuration for the PSOForge capture pass.
 * Stored in DefaultGame.ini under [/Script/PSOForge.PSOForgeSettings].
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "PSOForge"))
class PSOFORGE_API UPSOForgeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPSOForgeSettings();

	virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }

	/** Content roots scanned for materials and Niagara systems. */
	UPROPERTY(EditAnywhere, Config, Category = "Scan", meta = (ContentDir))
	TArray<FDirectoryPath> MaterialScanPaths;

	/** Assets whose package path contains any of these substrings are skipped. */
	UPROPERTY(EditAnywhere, Config, Category = "Scan")
	TArray<FString> ExcludePathPatterns;

	/** Materials loaded and rendered per batch. */
	UPROPERTY(EditAnywhere, Config, Category = "Capture", meta = (ClampMin = 1))
	int32 BatchSize = 32;

	/** Niagara systems loaded and simulated per batch. */
	UPROPERTY(EditAnywhere, Config, Category = "Capture", meta = (ClampMin = 1))
	int32 NiagaraBatchSize = 8;

	/** Frames each batch is rendered under each quality preset before moving on. */
	UPROPERTY(EditAnywhere, Config, Category = "Capture", meta = (ClampMin = 1))
	int32 WarmupFramesPerPreset = 15;

	/** Minimum seconds a Niagara batch simulates under each preset. */
	UPROPERTY(EditAnywhere, Config, Category = "Capture", meta = (ClampMin = 0.0))
	float NiagaraSimSeconds = 1.5f;

	/** Frames waited after garbage collection between batches. */
	UPROPERTY(EditAnywhere, Config, Category = "Capture", meta = (ClampMin = 1))
	int32 GCWaitFrames = 2;

	/** Scalability levels to sweep (0=Low .. 3=Epic, 4=Cinematic). */
	UPROPERTY(EditAnywhere, Config, Category = "Capture")
	TArray<int32> QualityPresets;

	/** Safety timeout for draining pipeline precompiles per batch/preset. */
	UPROPERTY(EditAnywhere, Config, Category = "Capture", meta = (ClampMin = 1.0))
	float PrecompileDrainTimeoutSeconds = 60.0f;

	/** Post-process materials blended onto the capture camera at once (rotated through the batch). */
	UPROPERTY(EditAnywhere, Config, Category = "Capture", meta = (ClampMin = 1))
	int32 MaxPostProcessMaterialsAtOnce = 4;

	/** Mesh every surface material is rendered on. */
	UPROPERTY(EditAnywhere, Config, Category = "Showcase")
	TSoftObjectPtr<UStaticMesh> ShowcaseStaticMesh;

	/** Flat mesh rendered alongside the primary mesh (two-sided/foliage-style coverage) and used as the floor. */
	UPROPERTY(EditAnywhere, Config, Category = "Showcase")
	TSoftObjectPtr<UStaticMesh> ShowcasePlaneMesh;

	/** Skeletal mesh used for materials flagged bUsedWithSkeletalMesh. Leave empty to skip the skinned pass. */
	UPROPERTY(EditAnywhere, Config, Category = "Showcase")
	TSoftObjectPtr<USkeletalMesh> ShowcaseSkeletalMesh;

	/**
	 * Optional dedicated capture map. When unset (default) the capture runs in the current map and
	 * spawns a transient environment rig (lights, fog, floor, camera) for deterministic pass coverage.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Showcase")
	TSoftObjectPtr<UWorld> CaptureMap;

	/** Spawn the transient environment rig even when a capture map is provided. */
	UPROPERTY(EditAnywhere, Config, Category = "Showcase")
	bool bSpawnEnvironmentRig = true;
};

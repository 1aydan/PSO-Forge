// Copyright (c) 2026 PSOForge contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "PSOForgeShowcase.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class UStaticMesh;
class USkeletalMesh;
class UCameraComponent;
class UPSOForgeSettings;

/**
 * Transient actor that renders one batch of assets so their draw PSOs are actually created.
 * Surface materials get static/plane (+skeletal/instanced when flagged) meshes, decal materials a
 * decal component, post-process materials rotate through the capture camera's blendables.
 */
UCLASS(NotPlaceable, Transient)
class APSOForgeShowcaseActor : public AActor
{
	GENERATED_BODY()

public:
	APSOForgeShowcaseActor();

	/** Builds showcase components for a batch of materials. Returns UI-domain materials for the Slate overlay. */
	TArray<UMaterialInterface*> BuildForMaterials(const TArray<UMaterialInterface*>& Materials, const UPSOForgeSettings& Settings,
		UStaticMesh* PrimaryMesh, UStaticMesh* PlaneMesh, USkeletalMesh* SkeletalMesh);

	/** Builds Niagara components for a batch of systems. */
	void BuildForNiagara(const TArray<UNiagaraSystem*>& Systems);

	/** Rotates post-process materials onto the camera; call once per warmup frame. */
	void TickShowcase(UCameraComponent* Camera, int32 MaxPostProcessAtOnce);

	/** Removes any blendables this actor pushed onto the camera. */
	void ClearPostProcess(UCameraComponent* Camera);

private:
	FVector GridLocation(int32 Index, float ZOffset) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> PostProcessMaterials;

	int32 PostProcessRotationIndex = 0;
};

/**
 * Transient lighting/environment rig spawned when capturing in an arbitrary map, so the common render
 * passes (dynamic shadows, local light shadows, fog, sky) are active regardless of the host level.
 */
UCLASS(NotPlaceable, Transient)
class APSOForgeEnvironmentRig : public AActor
{
	GENERATED_BODY()

public:
	APSOForgeEnvironmentRig();

	void BuildFloor(UStaticMesh* PlaneMesh);
};

/** Minimal game mode for an optional dedicated capture map: spectator pawn, no HUD. */
UCLASS()
class APSOForgeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APSOForgeGameMode();
};

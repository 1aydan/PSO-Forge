// Copyright (c) 2026 PSOForge contributors. MIT License.

#include "PSOForgeShowcase.h"
#include "PSOForge.h"
#include "PSOForgeSettings.h"

#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/SpectatorPawn.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

static constexpr int32 GPSOForgeGridColumns = 8;
static constexpr float GPSOForgeGridSpacing = 250.0f;

APSOForgeShowcaseActor::APSOForgeShowcaseActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

FVector APSOForgeShowcaseActor::GridLocation(int32 Index, float ZOffset) const
{
	const int32 Col = Index % GPSOForgeGridColumns;
	const int32 Row = Index / GPSOForgeGridColumns;
	const float HalfWidth = (GPSOForgeGridColumns - 1) * GPSOForgeGridSpacing * 0.5f;
	return FVector(Row * GPSOForgeGridSpacing, Col * GPSOForgeGridSpacing - HalfWidth, ZOffset);
}

TArray<UMaterialInterface*> APSOForgeShowcaseActor::BuildForMaterials(const TArray<UMaterialInterface*>& Materials,
	const UPSOForgeSettings& Settings, UStaticMesh* PrimaryMesh, UStaticMesh* PlaneMesh, USkeletalMesh* SkeletalMesh)
{
	TArray<UMaterialInterface*> UIMaterials;

	int32 GridIndex = 0;
	for (UMaterialInterface* MaterialInterface : Materials)
	{
		if (!MaterialInterface)
		{
			continue;
		}

		const UMaterial* BaseMaterial = MaterialInterface->GetMaterial();
		if (!BaseMaterial)
		{
			continue;
		}

		switch (BaseMaterial->MaterialDomain)
		{
		case MD_Surface:
		{
			if (PrimaryMesh)
			{
				UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(this);
				MeshComp->SetStaticMesh(PrimaryMesh);
				MeshComp->SetMaterial(0, MaterialInterface);
				MeshComp->SetForcedLodModel(1);
				MeshComp->SetCastShadow(true);
				MeshComp->SetupAttachment(GetRootComponent());
				MeshComp->SetRelativeLocation(GridLocation(GridIndex, 200.0f));
				MeshComp->RegisterComponent();
			}

			if (PlaneMesh)
			{
				UStaticMeshComponent* PlaneComp = NewObject<UStaticMeshComponent>(this);
				PlaneComp->SetStaticMesh(PlaneMesh);
				PlaneComp->SetMaterial(0, MaterialInterface);
				PlaneComp->SetForcedLodModel(1);
				PlaneComp->SetCastShadow(true);
				PlaneComp->SetupAttachment(GetRootComponent());
				PlaneComp->SetRelativeLocation(GridLocation(GridIndex, 60.0f));
				PlaneComp->RegisterComponent();
			}

			if (SkeletalMesh && BaseMaterial->bUsedWithSkeletalMesh)
			{
				USkeletalMeshComponent* SkelComp = NewObject<USkeletalMeshComponent>(this);
				SkelComp->SetSkeletalMesh(SkeletalMesh);
				for (int32 SlotIndex = 0; SlotIndex < SkelComp->GetNumMaterials(); ++SlotIndex)
				{
					SkelComp->SetMaterial(SlotIndex, MaterialInterface);
				}
				SkelComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
				SkelComp->SetForcedLOD(1);
				SkelComp->SetCastShadow(true);
				SkelComp->SetupAttachment(GetRootComponent());
				SkelComp->SetRelativeLocation(GridLocation(GridIndex, 420.0f));
				SkelComp->RegisterComponent();
			}

			if (PrimaryMesh && BaseMaterial->bUsedWithInstancedStaticMeshes)
			{
				UInstancedStaticMeshComponent* ISMComp = NewObject<UInstancedStaticMeshComponent>(this);
				ISMComp->SetStaticMesh(PrimaryMesh);
				ISMComp->SetMaterial(0, MaterialInterface);
				ISMComp->SetCastShadow(true);
				ISMComp->SetupAttachment(GetRootComponent());
				ISMComp->SetRelativeLocation(GridLocation(GridIndex, 650.0f));
				ISMComp->RegisterComponent();
				for (int32 Instance = 0; Instance < 4; ++Instance)
				{
					ISMComp->AddInstance(FTransform(FVector(0.0f, Instance * 120.0f - 180.0f, 0.0f)));
				}
			}
			break;
		}

		case MD_DeferredDecal:
		{
			UDecalComponent* DecalComp = NewObject<UDecalComponent>(this);
			DecalComp->SetDecalMaterial(MaterialInterface);
			DecalComp->DecalSize = FVector(200.0f, 200.0f, 200.0f);
			DecalComp->SetupAttachment(GetRootComponent());
			DecalComp->SetRelativeLocation(GridLocation(GridIndex, 100.0f));
			DecalComp->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
			DecalComp->RegisterComponent();
			break;
		}

		case MD_PostProcess:
			PostProcessMaterials.Add(MaterialInterface);
			break;

		case MD_UI:
			UIMaterials.Add(MaterialInterface);
			break;

		default:
			// Volume and light-function domains are not exercised yet.
			break;
		}

		++GridIndex;
	}

	return UIMaterials;
}

void APSOForgeShowcaseActor::BuildForNiagara(const TArray<UNiagaraSystem*>& Systems)
{
	int32 GridIndex = 0;
	for (UNiagaraSystem* System : Systems)
	{
		if (!System)
		{
			continue;
		}

		UNiagaraComponent* NiagaraComp = NewObject<UNiagaraComponent>(this);
		NiagaraComp->SetAsset(System);
		NiagaraComp->SetupAttachment(GetRootComponent());
		NiagaraComp->SetRelativeLocation(GridLocation(GridIndex, 300.0f));
		NiagaraComp->RegisterComponent();
		NiagaraComp->Activate(true);
		++GridIndex;
	}
}

void APSOForgeShowcaseActor::TickShowcase(UCameraComponent* Camera, int32 MaxPostProcessAtOnce)
{
	if (!Camera || PostProcessMaterials.Num() == 0)
	{
		return;
	}

	Camera->PostProcessSettings.WeightedBlendables.Array.Reset();
	const int32 Count = FMath::Min(MaxPostProcessAtOnce, PostProcessMaterials.Num());
	for (int32 Offset = 0; Offset < Count; ++Offset)
	{
		const int32 MaterialIndex = (PostProcessRotationIndex + Offset) % PostProcessMaterials.Num();
		Camera->PostProcessSettings.WeightedBlendables.Array.Add(
			FWeightedBlendable(1.0f, PostProcessMaterials[MaterialIndex]));
	}
	PostProcessRotationIndex = (PostProcessRotationIndex + Count) % FMath::Max(1, PostProcessMaterials.Num());
}

void APSOForgeShowcaseActor::ClearPostProcess(UCameraComponent* Camera)
{
	if (Camera)
	{
		Camera->PostProcessSettings.WeightedBlendables.Array.Reset();
	}
}

APSOForgeEnvironmentRig::APSOForgeEnvironmentRig()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	UDirectionalLightComponent* Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetCastShadows(true);
	Sun->SetRelativeRotation(FRotator(-50.0f, 30.0f, 0.0f));
	Sun->SetupAttachment(GetRootComponent());

	USkyLightComponent* Sky = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->SetupAttachment(GetRootComponent());

	USkyAtmosphereComponent* Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
	Atmosphere->SetupAttachment(GetRootComponent());

	UPointLightComponent* Point = CreateDefaultSubobject<UPointLightComponent>(TEXT("PointLight"));
	Point->SetMobility(EComponentMobility::Movable);
	Point->SetCastShadows(true);
	Point->SetRelativeLocation(FVector(200.0f, -400.0f, 500.0f));
	Point->SetAttenuationRadius(3000.0f);
	Point->SetupAttachment(GetRootComponent());

	USpotLightComponent* Spot = CreateDefaultSubobject<USpotLightComponent>(TEXT("SpotLight"));
	Spot->SetMobility(EComponentMobility::Movable);
	Spot->SetCastShadows(true);
	Spot->SetRelativeLocation(FVector(200.0f, 400.0f, 800.0f));
	Spot->SetRelativeRotation(FRotator(-70.0f, 0.0f, 0.0f));
	Spot->SetAttenuationRadius(3000.0f);
	Spot->SetupAttachment(GetRootComponent());

	UExponentialHeightFogComponent* Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(GetRootComponent());
}

void APSOForgeEnvironmentRig::BuildFloor(UStaticMesh* PlaneMesh)
{
	if (!PlaneMesh)
	{
		return;
	}

	UStaticMeshComponent* Floor = NewObject<UStaticMeshComponent>(this);
	Floor->SetStaticMesh(PlaneMesh);
	Floor->SetupAttachment(GetRootComponent());
	Floor->SetRelativeLocation(FVector(500.0f, 0.0f, 0.0f));
	Floor->SetRelativeScale3D(FVector(60.0f, 60.0f, 1.0f));
	Floor->RegisterComponent();
}

APSOForgeGameMode::APSOForgeGameMode()
{
	DefaultPawnClass = ASpectatorPawn::StaticClass();
	HUDClass = nullptr;
}

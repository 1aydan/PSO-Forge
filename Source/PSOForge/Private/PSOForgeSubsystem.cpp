// Copyright (c) 2026 PSOForge contributors. MIT License.

#include "PSOForgeSubsystem.h"
#include "PSOForge.h"
#include "PSOForgeSettings.h"
#include "PSOForgeShowcase.h"
#include "SPSOForgeOverlay.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "NiagaraSystem.h"
#include "PipelineFileCache.h"
#include "RenderingThread.h"
#include "ShaderPipelineCache.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

// One grid slot per material; keep the whole batch inside the camera frustum.
static FTransform PSOForgeStageTransform()
{
	return FTransform(FVector(0.0f, 0.0f, 0.0f));
}

bool UPSOForgeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer) && !IsRunningDedicatedServer() && !IsRunningCommandlet();
}

void UPSOForgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UPSOForgeSubsystem::HandlePostLoadMap);

	bAutoStartPending = FParse::Param(FCommandLine::Get(), TEXT("PSOForge"));
	if (bAutoStartPending)
	{
		UE_LOG(LogPSOForge, Display, TEXT("-PSOForge detected: capture will auto-start after the first map loads."));
	}
}

void UPSOForgeSubsystem::Deinitialize()
{
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}
	if (BatchHandle.IsValid())
	{
		BatchHandle->CancelHandle();
		BatchHandle.Reset();
	}
	Super::Deinitialize();
}

bool UPSOForgeSubsystem::IsCapturing() const
{
	return State != EPSOForgeState::Idle && State != EPSOForgeState::Finished;
}

bool UPSOForgeSubsystem::IsTickable() const
{
	return IsCapturing();
}

UWorld* UPSOForgeSubsystem::GetTickableGameObjectWorld() const
{
	return GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
}

TStatId UPSOForgeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPSOForgeSubsystem, STATGROUP_Tickables);
}

void UPSOForgeSubsystem::StartCapture(bool bInAutoQuitOnComplete)
{
	if (IsCapturing())
	{
		UE_LOG(LogPSOForge, Warning, TEXT("Capture already in progress."));
		return;
	}

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogPSOForge, Error, TEXT("No world available; cannot start capture."));
		return;
	}

	bAutoQuitOnComplete = bInAutoQuitOnComplete;
	SavedQualityLevels = Scalability::GetQualityLevels();
	MaterialPaths.Reset();
	NiagaraPaths.Reset();
	BatchStart = 0;
	PresetIndex = 0;
	ItemsProcessed = 0;
	TotalItems = 0;
	bNiagaraPhase = false;
	bStageReady = false;

	if (!FPipelineFileCacheManager::LogPSOtoFileCache())
	{
		UE_LOG(LogPSOForge, Warning, TEXT("PSO logging is NOT active (launch with -logPSO in a cooked build). Capture will run but nothing will be recorded."));
	}

	FShaderPipelineCache::ResumeBatching();

	if (Overlay.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
		Overlay.Reset();
	}
	if (GEngine && GEngine->GameViewport)
	{
		Overlay = SNew(SPSOForgeOverlay, this);
		GEngine->GameViewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 1000);
	}

	// Open the dedicated capture map when one is configured and we are not already in it.
	const UPSOForgeSettings* Settings = GetDefault<UPSOForgeSettings>();
	const FSoftObjectPath MapPath = Settings->CaptureMap.ToSoftObjectPath();
	if (MapPath.IsValid() && FPackageName::DoesPackageExist(MapPath.GetLongPackageName())
		&& World->GetOutermost()->GetName() != MapPath.GetLongPackageName())
	{
		bWaitingForMapLoad = true;
		EnterState(EPSOForgeState::OpeningCaptureMap);
		UGameplayStatics::OpenLevel(World, FName(*MapPath.GetLongPackageName()));
	}
	else
	{
		EnterState(EPSOForgeState::Scanning);
	}

	UE_LOG(LogPSOForge, Display, TEXT("PSOForge capture started (AutoQuit=%d, LoggingActive=%d)."),
		bAutoQuitOnComplete ? 1 : 0, FPipelineFileCacheManager::LogPSOtoFileCache() ? 1 : 0);
}

void UPSOForgeSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	// Defer real work to the next tick: this runs inside UEngine::LoadMap's broadcast, where
	// starting captures, opening levels, or touching the viewport widget stack is unsafe.
	if (bWaitingForMapLoad)
	{
		bWaitingForMapLoad = false;
		LoadedWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (State == EPSOForgeState::OpeningCaptureMap)
			{
				// The viewport widget stack is torn down on travel; re-add the overlay.
				if (Overlay.IsValid() && GEngine && GEngine->GameViewport)
				{
					GEngine->GameViewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 1000);
				}
				EnterState(EPSOForgeState::Scanning);
			}
		}));
	}
	else if (bAutoStartPending && !IsCapturing())
	{
		bAutoStartPending = false;
		LoadedWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (!IsCapturing())
			{
				StartCapture(/*bAutoQuitOnComplete*/ true);
			}
		}));
	}
}

void UPSOForgeSubsystem::CancelCapture()
{
	if (!IsCapturing())
	{
		return;
	}
	UE_LOG(LogPSOForge, Display, TEXT("Capture cancelled."));
	FinishCapture(/*bCancelled*/ true);
}

void UPSOForgeSubsystem::EnterState(EPSOForgeState NewState)
{
	State = NewState;
	StateEnterTimeSeconds = FPlatformTime::Seconds();
	WarmupSecondsElapsed = 0.0f;
	WarmupFramesElapsed = 0;
	GCFramesElapsed = 0;
}

void UPSOForgeSubsystem::Tick(float DeltaTime)
{
	switch (State)
	{
	case EPSOForgeState::OpeningCaptureMap:
		break; // Waiting on OnPostLoadMap.
	case EPSOForgeState::Scanning:
		TickScanning();
		break;
	case EPSOForgeState::LoadingBatch:
		TickLoadingBatch();
		break;
	case EPSOForgeState::SpawningShowcase:
		TickSpawningShowcase();
		break;
	case EPSOForgeState::ApplyingPreset:
		TickApplyingPreset();
		break;
	case EPSOForgeState::WarmingUp:
		TickWarmingUp(DeltaTime);
		break;
	case EPSOForgeState::DespawningBatch:
		TickDespawningBatch();
		break;
	case EPSOForgeState::WaitingForGC:
		TickWaitingForGC();
		break;
	default:
		break;
	}
}

void UPSOForgeSubsystem::TickScanning()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	if (AssetRegistryModule.Get().IsLoadingAssets())
	{
		BroadcastProgress(TEXT("Waiting for asset registry..."));
		return;
	}

	const UPSOForgeSettings* Settings = GetDefault<UPSOForgeSettings>();

	FARFilter Filter;
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UMaterialInterface::StaticClass()->GetClassPathName());
	Filter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
	for (const FDirectoryPath& Dir : Settings->MaterialScanPaths)
	{
		Filter.PackagePaths.Add(FName(*Dir.Path));
	}

	TArray<FAssetData> Assets;
	AssetRegistryModule.Get().GetAssets(Filter, Assets);

	const FTopLevelAssetPath NiagaraClassPath = UNiagaraSystem::StaticClass()->GetClassPathName();
	for (const FAssetData& Asset : Assets)
	{
		const FString PackagePath = Asset.PackageName.ToString();
		bool bExcluded = false;
		for (const FString& Pattern : Settings->ExcludePathPatterns)
		{
			if (!Pattern.IsEmpty() && PackagePath.Contains(Pattern))
			{
				bExcluded = true;
				break;
			}
		}
		if (bExcluded)
		{
			continue;
		}

		if (Asset.AssetClassPath == NiagaraClassPath)
		{
			NiagaraPaths.Add(Asset.ToSoftObjectPath());
		}
		else
		{
			MaterialPaths.Add(Asset.ToSoftObjectPath());
		}
	}

	TotalItems = MaterialPaths.Num() + NiagaraPaths.Num();
	UE_LOG(LogPSOForge, Display, TEXT("Scan complete: %d materials, %d Niagara systems, %d presets."),
		MaterialPaths.Num(), NiagaraPaths.Num(), Settings->QualityPresets.Num());

	if (TotalItems == 0)
	{
		FinishCapture(/*bCancelled*/ false);
		return;
	}

	bNiagaraPhase = MaterialPaths.Num() == 0;
	BatchStart = 0;
	EnterState(EPSOForgeState::LoadingBatch);
}

int32 UPSOForgeSubsystem::CurrentBatchEnd() const
{
	const UPSOForgeSettings* Settings = GetDefault<UPSOForgeSettings>();
	const TArray<FSoftObjectPath>& Paths = bNiagaraPhase ? NiagaraPaths : MaterialPaths;
	const int32 BatchSize = bNiagaraPhase ? Settings->NiagaraBatchSize : Settings->BatchSize;
	return FMath::Min(BatchStart + BatchSize, Paths.Num());
}

bool UPSOForgeSubsystem::BatchesRemain() const
{
	const TArray<FSoftObjectPath>& Paths = bNiagaraPhase ? NiagaraPaths : MaterialPaths;
	return BatchStart < Paths.Num();
}

void UPSOForgeSubsystem::TickLoadingBatch()
{
	if (!BatchHandle.IsValid())
	{
		const TArray<FSoftObjectPath>& Paths = bNiagaraPhase ? NiagaraPaths : MaterialPaths;
		TArray<FSoftObjectPath> BatchPaths;
		for (int32 Index = BatchStart; Index < CurrentBatchEnd(); ++Index)
		{
			BatchPaths.Add(Paths[Index]);
		}

		// Showcase meshes ride along with the first batch.
		const UPSOForgeSettings* Settings = GetDefault<UPSOForgeSettings>();
		if (!bStageReady)
		{
			if (!Settings->ShowcaseStaticMesh.IsNull())
			{
				BatchPaths.Add(Settings->ShowcaseStaticMesh.ToSoftObjectPath());
			}
			if (!Settings->ShowcasePlaneMesh.IsNull())
			{
				BatchPaths.Add(Settings->ShowcasePlaneMesh.ToSoftObjectPath());
			}
			if (!Settings->ShowcaseSkeletalMesh.IsNull())
			{
				BatchPaths.Add(Settings->ShowcaseSkeletalMesh.ToSoftObjectPath());
			}
		}

		BroadcastProgress(FString::Printf(TEXT("Loading %s batch (%d assets)..."),
			bNiagaraPhase ? TEXT("Niagara") : TEXT("material"), BatchPaths.Num()));
		BatchHandle = StreamableManager.RequestAsyncLoad(BatchPaths);
	}

	if (BatchHandle.IsValid() && BatchHandle->HasLoadCompleted())
	{
		LoadedBatch.Reset();
		TArray<UObject*> Loaded;
		BatchHandle->GetLoadedAssets(Loaded);
		for (UObject* Object : Loaded)
		{
			LoadedBatch.Add(Object);
		}
		EnterState(EPSOForgeState::SpawningShowcase);
	}
	else if (FPlatformTime::Seconds() - StateEnterTimeSeconds > GetDefault<UPSOForgeSettings>()->PrecompileDrainTimeoutSeconds)
	{
		UE_LOG(LogPSOForge, Warning, TEXT("Batch load timed out at index %d; skipping batch."), BatchStart);
		if (BatchHandle.IsValid())
		{
			BatchHandle->CancelHandle();
			BatchHandle.Reset();
		}
		EnterState(EPSOForgeState::DespawningBatch);
	}
}

void UPSOForgeSubsystem::EnsureStage()
{
	if (bStageReady)
	{
		return;
	}

	UWorld* World = GetGameInstance()->GetWorld();
	const UPSOForgeSettings* Settings = GetDefault<UPSOForgeSettings>();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	if (Settings->bSpawnEnvironmentRig && !EnvironmentRig)
	{
		EnvironmentRig = World->SpawnActor<APSOForgeEnvironmentRig>(PSOForgeStageTransform().GetLocation(), FRotator::ZeroRotator, SpawnParams);
		EnvironmentRig->BuildFloor(Settings->ShowcasePlaneMesh.Get());
	}

	if (!CaptureCamera)
	{
		CaptureCamera = World->SpawnActor<ACameraActor>(FVector(-2200.0f, 0.0f, 900.0f), FRotator(-12.0f, 0.0f, 0.0f), SpawnParams);
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetViewTargetWithBlend(CaptureCamera, 0.0f);
		}
	}

	bStageReady = true;
}

void UPSOForgeSubsystem::TickSpawningShowcase()
{
	EnsureStage();

	UWorld* World = GetGameInstance()->GetWorld();
	const UPSOForgeSettings* Settings = GetDefault<UPSOForgeSettings>();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;
	Showcase = World->SpawnActor<APSOForgeShowcaseActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (bNiagaraPhase)
	{
		TArray<UNiagaraSystem*> Systems;
		for (UObject* Object : LoadedBatch)
		{
			if (UNiagaraSystem* System = Cast<UNiagaraSystem>(Object))
			{
				Systems.Add(System);
			}
		}
		Showcase->BuildForNiagara(Systems);
	}
	else
	{
		TArray<UMaterialInterface*> Materials;
		for (UObject* Object : LoadedBatch)
		{
			if (UMaterialInterface* Material = Cast<UMaterialInterface>(Object))
			{
				Materials.Add(Material);
			}
		}

		const TArray<UMaterialInterface*> UIMaterials = Showcase->BuildForMaterials(Materials, *Settings,
			Settings->ShowcaseStaticMesh.Get(), Settings->ShowcasePlaneMesh.Get(), Settings->ShowcaseSkeletalMesh.Get());
		if (Overlay.IsValid())
		{
			Overlay->SetUIMaterials(UIMaterials);
		}
	}

	PresetIndex = 0;
	EnterState(EPSOForgeState::ApplyingPreset);
}

void UPSOForgeSubsystem::TickApplyingPreset()
{
	const UPSOForgeSettings* Settings = GetDefault<UPSOForgeSettings>();
	if (Settings->QualityPresets.IsValidIndex(PresetIndex))
	{
		// Drain rendering *before* touching scalability. Scalability cvars are ECVF_RenderThreadSafe,
		// so their non-game-thread copy is written by an enqueued render command while shadow setup
		// (FSceneRenderer::CreateDynamicShadows, gated on GetShadowQuality() > 0) and shadow projection
		// both read that copy from async worker tasks. A write landing between those two reads leaves a
		// frame that built shadows at one quality and projects them at another; dropping to sg 0
		// (r.ShadowQuality=0) that way reaches the unclamped 1..5 switch in
		// FProjectedShadowInfo::RenderOnePassPointLightProjection and trips check(0). Flushing first
		// means no frame is in flight when the write happens; flushing afterwards is too late.
		FlushRenderingCommands();

		Scalability::FQualityLevels Levels;
		Levels.SetFromSingleQualityLevel(Settings->QualityPresets[PresetIndex]);
		Levels.ShadowQuality = FMath::Max(Levels.ShadowQuality, Settings->MinShadowQuality);
		Scalability::SetQualityLevels(Levels);

		BroadcastProgress(FString::Printf(TEXT("Preset %d/%d (sg level %d)"),
			PresetIndex + 1, Settings->QualityPresets.Num(), Settings->QualityPresets[PresetIndex]));
	}
	EnterState(EPSOForgeState::WarmingUp);
}

void UPSOForgeSubsystem::TickWarmingUp(float DeltaTime)
{
	const UPSOForgeSettings* Settings = GetDefault<UPSOForgeSettings>();

	++WarmupFramesElapsed;
	WarmupSecondsElapsed += DeltaTime;

	if (Showcase && CaptureCamera)
	{
		Showcase->TickShowcase(CaptureCamera->GetCameraComponent(), Settings->MaxPostProcessMaterialsAtOnce);
	}

	const bool bFramesDone = WarmupFramesElapsed >= Settings->WarmupFramesPerPreset;
	const bool bNiagaraDone = !bNiagaraPhase || WarmupSecondsElapsed >= Settings->NiagaraSimSeconds;
	const bool bPrecompilesDrained = FShaderPipelineCache::NumPrecompilesRemaining() == 0;
	const bool bTimedOut = FPlatformTime::Seconds() - StateEnterTimeSeconds > Settings->PrecompileDrainTimeoutSeconds;

	if ((bFramesDone && bNiagaraDone && bPrecompilesDrained) || bTimedOut)
	{
		if (bTimedOut)
		{
			UE_LOG(LogPSOForge, Warning, TEXT("Warmup timed out (batch %d, preset index %d); continuing."), BatchStart, PresetIndex);
		}

		++PresetIndex;
		if (PresetIndex < Settings->QualityPresets.Num())
		{
			EnterState(EPSOForgeState::ApplyingPreset);
		}
		else
		{
			ItemsProcessed += CurrentBatchEnd() - BatchStart;
			EnterState(EPSOForgeState::DespawningBatch);
		}
	}
}

void UPSOForgeSubsystem::TickDespawningBatch()
{
	if (Showcase)
	{
		if (CaptureCamera)
		{
			Showcase->ClearPostProcess(CaptureCamera->GetCameraComponent());
		}
		Showcase->Destroy();
		Showcase = nullptr;
	}
	if (Overlay.IsValid())
	{
		Overlay->SetUIMaterials({});
	}

	LoadedBatch.Reset();
	if (BatchHandle.IsValid())
	{
		BatchHandle->ReleaseHandle();
		BatchHandle.Reset();
	}

	BroadcastProgress(TEXT("Collecting garbage..."));
	if (GEngine)
	{
		GEngine->ForceGarbageCollection(true);
	}
	EnterState(EPSOForgeState::WaitingForGC);
}

void UPSOForgeSubsystem::TickWaitingForGC()
{
	if (++GCFramesElapsed < GetDefault<UPSOForgeSettings>()->GCWaitFrames)
	{
		return;
	}

	BatchStart = CurrentBatchEnd();
	if (!BatchesRemain())
	{
		if (!bNiagaraPhase && NiagaraPaths.Num() > 0)
		{
			bNiagaraPhase = true;
			BatchStart = 0;
		}
		else
		{
			FinishCapture(/*bCancelled*/ false);
			return;
		}
	}
	EnterState(EPSOForgeState::LoadingBatch);
}

void UPSOForgeSubsystem::FinishCapture(bool bCancelled)
{
	bWaitingForMapLoad = false;
	TearDownStage();
	FlushRenderingCommands();
	Scalability::SetQualityLevels(SavedQualityLevels);

	LoadedBatch.Reset();
	if (BatchHandle.IsValid())
	{
		BatchHandle->CancelHandle();
		BatchHandle.Reset();
	}

	const uint32 PSOsLogged = FPipelineFileCacheManager::NumPSOsLogged();
	if (!bCancelled)
	{
		// Flush the recording before any exit; the .rec file is finalized on clean shutdown.
		FShaderPipelineCache::SavePipelineFileCache(FPipelineFileCacheManager::SaveMode::Incremental);
		UE_LOG(LogPSOForge, Display, TEXT("CaptureComplete Materials=%d Niagara=%d PSOsLogged=%u"),
			MaterialPaths.Num(), NiagaraPaths.Num(), PSOsLogged);
	}

	EnterState(EPSOForgeState::Finished);
	BroadcastProgress(bCancelled ? TEXT("Cancelled") : TEXT("Capture complete"));
	OnCaptureComplete.Broadcast();

	if (Overlay.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	}
	Overlay.Reset();

	if (!bCancelled && bAutoQuitOnComplete)
	{
		UE_LOG(LogPSOForge, Display, TEXT("Auto-quit requested; exiting."));
		FPlatformMisc::RequestExit(/*Force*/ false);
	}
}

void UPSOForgeSubsystem::TearDownStage()
{
	if (Showcase)
	{
		if (CaptureCamera)
		{
			Showcase->ClearPostProcess(CaptureCamera->GetCameraComponent());
		}
		Showcase->Destroy();
		Showcase = nullptr;
	}
	if (EnvironmentRig)
	{
		EnvironmentRig->Destroy();
		EnvironmentRig = nullptr;
	}
	if (CaptureCamera)
	{
		CaptureCamera->Destroy();
		CaptureCamera = nullptr;
	}
	bStageReady = false;
}

void UPSOForgeSubsystem::BroadcastProgress(const FString& Status)
{
	const float Alpha = TotalItems > 0 ? static_cast<float>(ItemsProcessed) / static_cast<float>(TotalItems) : 0.0f;
	OnProgressUpdated.Broadcast(Alpha, Status);
}

const TCHAR* UPSOForgeSubsystem::StateName() const
{
	switch (State)
	{
	case EPSOForgeState::Idle: return TEXT("Idle");
	case EPSOForgeState::OpeningCaptureMap: return TEXT("OpeningCaptureMap");
	case EPSOForgeState::Scanning: return TEXT("Scanning");
	case EPSOForgeState::LoadingBatch: return TEXT("LoadingBatch");
	case EPSOForgeState::SpawningShowcase: return TEXT("SpawningShowcase");
	case EPSOForgeState::ApplyingPreset: return TEXT("ApplyingPreset");
	case EPSOForgeState::WarmingUp: return TEXT("WarmingUp");
	case EPSOForgeState::DespawningBatch: return TEXT("DespawningBatch");
	case EPSOForgeState::WaitingForGC: return TEXT("WaitingForGC");
	case EPSOForgeState::Finished: return TEXT("Finished");
	default: return TEXT("Unknown");
	}
}

FString UPSOForgeSubsystem::GetStatusString() const
{
	const UPSOForgeSettings* Settings = GetDefault<UPSOForgeSettings>();
	return FString::Printf(TEXT("PSOForge: %s | phase=%s | item %d/%d | preset %d/%d | precompiles remaining=%u | PSOs logged=%u | recording=%s"),
		StateName(),
		bNiagaraPhase ? TEXT("Niagara") : TEXT("Materials"),
		ItemsProcessed, TotalItems,
		FMath::Min(PresetIndex + 1, Settings->QualityPresets.Num()), Settings->QualityPresets.Num(),
		FShaderPipelineCache::NumPrecompilesRemaining(),
		FPipelineFileCacheManager::NumPSOsLogged(),
		FPipelineFileCacheManager::LogPSOtoFileCache() ? TEXT("ON") : TEXT("OFF"));
}

// Copyright (c) 2026 PSOForge contributors. MIT License.

#include "PSOForge.h"
#include "PSOForgeSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

#if UE_BUILD_SHIPPING
#error PSOForge is a development-only capture tool and must not be compiled into Shipping builds. Check the TargetConfigurationAllowList in PSOForge.uplugin.
#endif

DEFINE_LOG_CATEGORY(LogPSOForge);

static UPSOForgeSubsystem* GetPSOForgeSubsystem(UWorld* World)
{
	if (World && World->GetGameInstance())
	{
		return World->GetGameInstance()->GetSubsystem<UPSOForgeSubsystem>();
	}
	return nullptr;
}

static FAutoConsoleCommandWithWorld GPSOForgeStartCmd(
	TEXT("PSOForge.Start"),
	TEXT("Starts a PSOForge capture pass in the current world (run in a cooked build with -logPSO to record PSOs)."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		if (UPSOForgeSubsystem* Subsystem = GetPSOForgeSubsystem(World))
		{
			Subsystem->StartCapture(/*bAutoQuitOnComplete*/ false);
		}
	}));

static FAutoConsoleCommandWithWorld GPSOForgeStopCmd(
	TEXT("PSOForge.Stop"),
	TEXT("Cancels a running PSOForge capture pass."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		if (UPSOForgeSubsystem* Subsystem = GetPSOForgeSubsystem(World))
		{
			Subsystem->CancelCapture();
		}
	}));

static FAutoConsoleCommandWithWorld GPSOForgeStatusCmd(
	TEXT("PSOForge.Status"),
	TEXT("Logs the current PSOForge capture state."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		if (UPSOForgeSubsystem* Subsystem = GetPSOForgeSubsystem(World))
		{
			UE_LOG(LogPSOForge, Display, TEXT("%s"), *Subsystem->GetStatusString());
		}
	}));

void FPSOForgeModule::StartupModule()
{
	UE_LOG(LogPSOForge, Log, TEXT("PSOForge module loaded (development-only, excluded from Shipping)."));
}

void FPSOForgeModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FPSOForgeModule, PSOForge)

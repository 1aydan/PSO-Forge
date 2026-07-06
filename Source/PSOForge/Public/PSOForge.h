// Copyright (c) 2026 PSOForge contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

PSOFORGE_API DECLARE_LOG_CATEGORY_EXTERN(LogPSOForge, Log, All);

class FPSOForgeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

// Copyright (c) 2026 PSOForge contributors. MIT License.

#include "Modules/ModuleManager.h"

// Shipping-safe player-facing runtime (warmup widget); the capture tooling lives in the
// development-only PSOForge module.
IMPLEMENT_MODULE(FDefaultModuleImpl, PSOForgeRuntime)

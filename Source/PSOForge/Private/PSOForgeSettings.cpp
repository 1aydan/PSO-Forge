// Copyright (c) 2026 PSOForge contributors. MIT License.

#include "PSOForgeSettings.h"

#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"

UPSOForgeSettings::UPSOForgeSettings()
{
	FDirectoryPath GameRoot;
	GameRoot.Path = TEXT("/Game");
	MaterialScanPaths.Add(GameRoot);

	QualityPresets = { 0, 1, 2, 3 };

	ShowcaseStaticMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	ShowcasePlaneMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Plane.Plane")));
}

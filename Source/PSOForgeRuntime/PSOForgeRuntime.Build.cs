// Copyright (c) 2026 PSOForge contributors. MIT License.

using UnrealBuildTool;

public class PSOForgeRuntime : ModuleRules
{
	public PSOForgeRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
			"RHI",
			"Slate",
			"SlateCore"
		});
	}
}

// Copyright (c) 2026 PSOForge contributors. MIT License.

using UnrealBuildTool;

public class PSOForge : ModuleRules
{
	public PSOForge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
			"RHI",
			"AssetRegistry",
			"Niagara",
			"Slate",
			"SlateCore",
			"DeveloperSettings",
			"InputCore"
		});
	}
}

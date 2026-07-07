// Copyright (c) 2026 PSOForge contributors. MIT License.

using UnrealBuildTool;

public class PSOForgeEditor : ModuleRules
{
	public PSOForgeEditor(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"Projects",
			"DesktopPlatform",
			"WorkspaceMenuStructure"
		});
	}
}

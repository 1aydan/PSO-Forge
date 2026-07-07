// Copyright (c) 2026 PSOForge contributors. MIT License.

#include "SPSOForgeCacheBuilder.h"

#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "PSOForgeEditor"

static const FName GPSOForgeCacheBuilderTabName(TEXT("PSOForgeCacheBuilder"));

static TSharedRef<SDockTab> SpawnPSOForgeCacheBuilderTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SPSOForgeCacheBuilder)
		];
}

class FPSOForgeEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		MenuGroup = WorkspaceMenu::GetMenuStructure().GetToolsCategory()->AddGroup(
			FName(TEXT("PSOForge")),
			LOCTEXT("MenuGroup", "PSO Forge"),
			LOCTEXT("MenuGroupTooltip", "PSO capture and bundled-cache tooling"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "MaterialEditor.TogglePlatformStats.Tab"));

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(GPSOForgeCacheBuilderTabName,
			FOnSpawnTab::CreateStatic(&SpawnPSOForgeCacheBuilderTab))
			.SetDisplayName(LOCTEXT("TabTitle", "Cache Builder"))
			.SetTooltipText(LOCTEXT("TabTooltip", "Convert recorded PSOs (.rec.upipelinecache) and cook stable keys (.shk) into a bundled PSO cache (.spc)."))
			.SetGroup(MenuGroup.ToSharedRef())
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "MaterialEditor.TogglePlatformStats.Tab"));
	}

	virtual void ShutdownModule() override
	{
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GPSOForgeCacheBuilderTabName);
		}
		if (MenuGroup.IsValid())
		{
			WorkspaceMenu::GetMenuStructure().GetToolsCategory()->RemoveItem(MenuGroup.ToSharedRef());
			MenuGroup.Reset();
		}
	}

private:
	TSharedPtr<FWorkspaceItem> MenuGroup;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPSOForgeEditorModule, PSOForgeEditor)

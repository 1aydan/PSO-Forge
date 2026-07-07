// Copyright (c) 2026 PSOForge contributors. MIT License.

#include "SPSOForgeCacheBuilder.h"

#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "PSOForgeCacheBuilder"

static FString PSOForgeShaderFormatFromShkName(const FString& FileName)
{
	// ShaderStableInfo-<Name>-<FORMAT>.shk
	FString Base = FPaths::GetBaseFilename(FileName);
	int32 LastDash = INDEX_NONE;
	if (Base.FindLastChar(TEXT('-'), LastDash) && LastDash + 1 < Base.Len())
	{
		return Base.Mid(LastDash + 1);
	}
	return FString();
}

void SPSOForgeCacheBuilder::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		// Toolbar row
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Refresh", "Refresh"))
				.OnClicked_Lambda([this]() { RefreshLists(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("AddFolder", "Add Recordings Folder..."))
				.ToolTipText(LOCTEXT("AddFolderTip", "Add a folder of QA-collected .rec.upipelinecache files"))
				.OnClicked(this, &SPSOForgeCacheBuilder::OnAddFolderClicked)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNullWidget::NullWidget
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("Format", "Shader format:"))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SAssignNew(FormatCombo, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&ShaderFormats)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
				{
					return SNew(STextBlock).Text(FText::FromString(*Item));
				})
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type)
				{
					SelectedFormat = Item;
				})
				[
					SNew(STextBlock).Text_Lambda([this]()
					{
						return FText::FromString(SelectedFormat.IsValid() ? **SelectedFormat : TEXT("<cook first>"));
					})
				]
			]
		]

		// Dev-managed recording folders (cooked builds live wherever the team puts them)
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 4.0f)
		[
			SNew(STextBlock).Text(LOCTEXT("Folders", "Recording folders (scanned recursively — point at a build root or a CollectedPSOs folder):"))
		]
		+ SVerticalBox::Slot().MaxHeight(96.0f).Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SAssignNew(FoldersView, SListView<TSharedPtr<FString>>)
				.ListItemsSource(&RecordingFolders)
				.OnGenerateRow(this, &SPSOForgeCacheBuilder::MakeFolderRow)
				.SelectionMode(ESelectionMode::None)
			]
		]

		// File lists
		+ SVerticalBox::Slot().FillHeight(0.5f).Padding(8.0f, 0.0f)
		[
			SNew(SSplitter)
			+ SSplitter::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(LOCTEXT("Recordings", "Recorded PSOs (.rec.upipelinecache) — run the game with -logPSO to collect"))
				]
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					[
						SAssignNew(RecordingsView, SListView<TSharedPtr<FPSOForgeCandidateFile>>)
						.ListItemsSource(&Recordings)
						.OnGenerateRow(this, &SPSOForgeCacheBuilder::MakeFileRow)
						.SelectionMode(ESelectionMode::None)
					]
				]
			]
			+ SSplitter::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(LOCTEXT("StableKeys", "Stable shader keys (.shk) — produced by cooking with NeedsShaderStableKeys=true"))
				]
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					[
						SAssignNew(StableKeysView, SListView<TSharedPtr<FPSOForgeCandidateFile>>)
						.ListItemsSource(&StableKeys)
						.OnGenerateRow(this, &SPSOForgeCacheBuilder::MakeFileRow)
						.SelectionMode(ESelectionMode::None)
					]
				]
			]
		]

		// Output + actions
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bDeployToBuild ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bDeployToBuild = State == ECheckBoxState::Checked; })
				[
					SNew(STextBlock).Text(LOCTEXT("Deploy", "Deploy to Build/Windows/PipelineCaches (bundled by the next package)"))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(12.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(this, &SPSOForgeCacheBuilder::GetOutputPathPreview)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Cancel", "Cancel"))
				.IsEnabled_Lambda([this]() { return Task.IsValid() && Task->IsRunning(); })
				.OnClicked(this, &SPSOForgeCacheBuilder::OnCancelClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
				.Text(LOCTEXT("Build", "Build Cache"))
				.IsEnabled(this, &SPSOForgeCacheBuilder::CanBuild)
				.OnClicked(this, &SPSOForgeCacheBuilder::OnBuildClicked)
			]
		]

		// Summary + log
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f)
		[
			SNew(STextBlock).Text(this, &SPSOForgeCacheBuilder::GetSummaryText)
		]
		+ SVerticalBox::Slot().FillHeight(0.5f).Padding(8.0f)
		[
			SAssignNew(LogBox, SMultiLineEditableTextBox)
			.IsReadOnly(true)
			.AlwaysShowScrollbars(true)
		]
	];

	LoadRecordingFolders();
	RefreshLists();
}

void SPSOForgeCacheBuilder::LoadRecordingFolders()
{
	TArray<FString> Folders;
	GConfig->GetArray(TEXT("PSOForgeCacheBuilder"), TEXT("RecordingFolders"), Folders, GEditorPerProjectIni);
	if (Folders.Num() == 0)
	{
		Folders = FPSOForgeCacheBuildTask::DefaultRecordingFolders();
	}

	RecordingFolders.Reset();
	for (const FString& Folder : Folders)
	{
		RecordingFolders.Add(MakeShared<FString>(Folder));
	}
}

void SPSOForgeCacheBuilder::SaveRecordingFolders() const
{
	TArray<FString> Folders;
	for (const TSharedPtr<FString>& Folder : RecordingFolders)
	{
		Folders.Add(*Folder);
	}
	GConfig->SetArray(TEXT("PSOForgeCacheBuilder"), TEXT("RecordingFolders"), Folders, GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);
}

TSharedRef<ITableRow> SPSOForgeCacheBuilder::MakeFolderRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("RemoveFolder", "X"))
			.ToolTipText(LOCTEXT("RemoveFolderTip", "Remove this folder"))
			.OnClicked_Lambda([this, Item]()
			{
				RecordingFolders.Remove(Item);
				SaveRecordingFolders();
				RefreshLists();
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(FText::FromString(*Item)).ToolTipText(FText::FromString(*Item))
		]
	];
}

SPSOForgeCacheBuilder::~SPSOForgeCacheBuilder()
{
	if (Task.IsValid())
	{
		Task->OnLogLine.Unbind();
		Task->OnFinished.Unbind();
		Task->Cancel();
	}
}

void SPSOForgeCacheBuilder::RefreshLists()
{
	TArray<FString> FoldersToScan;
	for (const TSharedPtr<FString>& Folder : RecordingFolders)
	{
		FoldersToScan.Add(*Folder);
	}

	Recordings.Reset();
	for (FPSOForgeCandidateFile& File : FPSOForgeCacheBuildTask::DiscoverFiles(FoldersToScan, TEXT("*.rec.upipelinecache"), /*bRecursive*/ true))
	{
		Recordings.Add(MakeShared<FPSOForgeCandidateFile>(MoveTemp(File)));
	}

	StableKeys.Reset();
	ShaderFormats.Reset();
	for (FPSOForgeCandidateFile& File : FPSOForgeCacheBuildTask::DiscoverFiles(FPSOForgeCacheBuildTask::CookedMetadataFolders(), TEXT("*.shk"), /*bRecursive*/ false))
	{
		const FString Format = PSOForgeShaderFormatFromShkName(File.Path);
		if (!Format.IsEmpty() && !ShaderFormats.ContainsByPredicate([&Format](const TSharedPtr<FString>& Existing) { return *Existing == Format; }))
		{
			ShaderFormats.Add(MakeShared<FString>(Format));
		}
		StableKeys.Add(MakeShared<FPSOForgeCandidateFile>(MoveTemp(File)));
	}

	if (!SelectedFormat.IsValid() || !ShaderFormats.ContainsByPredicate([this](const TSharedPtr<FString>& Existing) { return *Existing == *SelectedFormat; }))
	{
		SelectedFormat = ShaderFormats.Num() > 0 ? ShaderFormats[0] : nullptr;
	}

	if (FoldersView.IsValid())
	{
		FoldersView->RequestListRefresh();
	}
	if (RecordingsView.IsValid())
	{
		RecordingsView->RequestListRefresh();
	}
	if (StableKeysView.IsValid())
	{
		StableKeysView->RequestListRefresh();
	}
	if (FormatCombo.IsValid())
	{
		FormatCombo->RefreshOptions();
	}
}

TSharedRef<ITableRow> SPSOForgeCacheBuilder::MakeFileRow(TSharedPtr<FPSOForgeCandidateFile> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const FString Label = FString::Printf(TEXT("%s   (%lld KB, %s)"),
		*FPaths::GetCleanFilename(Item->Path), Item->SizeBytes / 1024, *Item->ModTime.ToString());

	return SNew(STableRow<TSharedPtr<FPSOForgeCandidateFile>>, OwnerTable)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([Item]() { return Item->bSelected ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([Item](ECheckBoxState State) { Item->bSelected = State == ECheckBoxState::Checked; })
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Label))
			.ToolTipText(FText::FromString(Item->Path))
		]
	];
}

TArray<FString> SPSOForgeCacheBuilder::SelectedPaths(const TArray<TSharedPtr<FPSOForgeCandidateFile>>& Items, const FString& FormatFilter) const
{
	// Filter by shader format when any file name carries the format token; otherwise take everything
	// selected (recordings from older captures may not encode the format).
	TArray<FString> Paths;
	bool bAnyMatchesFormat = false;
	for (const TSharedPtr<FPSOForgeCandidateFile>& Item : Items)
	{
		if (Item->bSelected && Item->Path.Contains(FormatFilter))
		{
			bAnyMatchesFormat = true;
			break;
		}
	}
	for (const TSharedPtr<FPSOForgeCandidateFile>& Item : Items)
	{
		if (Item->bSelected && (!bAnyMatchesFormat || Item->Path.Contains(FormatFilter)))
		{
			Paths.Add(Item->Path);
		}
	}
	return Paths;
}

bool SPSOForgeCacheBuilder::CanBuild() const
{
	return SelectedFormat.IsValid()
		&& !(Task.IsValid() && Task->IsRunning())
		&& Recordings.ContainsByPredicate([](const TSharedPtr<FPSOForgeCandidateFile>& Item) { return Item->bSelected; })
		&& StableKeys.ContainsByPredicate([](const TSharedPtr<FPSOForgeCandidateFile>& Item) { return Item->bSelected; });
}

FText SPSOForgeCacheBuilder::GetOutputPathPreview() const
{
	if (!SelectedFormat.IsValid())
	{
		return LOCTEXT("NoFormat", "No .shk stable keys found — cook the project first (requires NeedsShaderStableKeys=true).");
	}
	const FString FileName = FString::Printf(TEXT("%s_%s.spc"), FApp::GetProjectName(), **SelectedFormat);
	return FText::FromString(bDeployToBuild
		? FString(TEXT("Build/Windows/PipelineCaches/")) + FileName
		: FString(TEXT("Saved/PSOForge/")) + FileName);
}

void SPSOForgeCacheBuilder::AppendLog(const FString& Line)
{
	if (LogBox.IsValid())
	{
		const FString Existing = LogBox->GetText().ToString();
		LogBox->SetText(FText::FromString(Existing.IsEmpty() ? Line : Existing + TEXT("\n") + Line));
		LogBox->ScrollTo(ETextLocation::EndOfDocument);
	}
}

FReply SPSOForgeCacheBuilder::OnBuildClicked()
{
	if (!CanBuild())
	{
		return FReply::Handled();
	}

	SummaryLine = TEXT("Building...");
	if (LogBox.IsValid())
	{
		LogBox->SetText(FText::GetEmpty());
	}

	Task = MakeShared<FPSOForgeCacheBuildTask>();
	Task->OnLogLine.BindSP(this, &SPSOForgeCacheBuilder::AppendLog);
	Task->OnFinished.BindLambda([this](bool bSuccess, const FString& Summary)
	{
		SummaryLine = Summary;
		AppendLog(Summary);
	});

	if (!Task->Start(SelectedPaths(Recordings, *SelectedFormat), SelectedPaths(StableKeys, *SelectedFormat),
		*SelectedFormat, bDeployToBuild))
	{
		SummaryLine = TEXT("Could not start the cache build — see log.");
	}
	return FReply::Handled();
}

FReply SPSOForgeCacheBuilder::OnCancelClicked()
{
	if (Task.IsValid())
	{
		Task->Cancel();
	}
	return FReply::Handled();
}

FReply SPSOForgeCacheBuilder::OnAddFolderClicked()
{
	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
	{
		FString Folder;
		const void* ParentWindow = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared());
		if (DesktopPlatform->OpenDirectoryDialog(ParentWindow, TEXT("Add a folder to scan for .rec.upipelinecache files (build roots are fine)"),
			FPaths::ProjectSavedDir(), Folder))
		{
			const FString FullFolder = FPaths::ConvertRelativePathToFull(Folder);
			const bool bAlreadyListed = RecordingFolders.ContainsByPredicate(
				[&FullFolder](const TSharedPtr<FString>& Existing) { return *Existing == FullFolder; });
			if (!bAlreadyListed)
			{
				RecordingFolders.Add(MakeShared<FString>(FullFolder));
				SaveRecordingFolders();
			}
			RefreshLists();
		}
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

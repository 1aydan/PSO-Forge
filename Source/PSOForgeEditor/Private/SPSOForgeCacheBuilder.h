// Copyright (c) 2026 PSOForge contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "PSOForgeCacheBuildTask.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SMultiLineEditableTextBox;
class STextBlock;
template <typename OptionType> class SComboBox;

/**
 * Tools-menu panel that turns recorded PSOs (.rec.upipelinecache) plus cook stable keys (.shk)
 * into the bundled .spc cache, without leaving the editor.
 */
class SPSOForgeCacheBuilder : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPSOForgeCacheBuilder) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SPSOForgeCacheBuilder() override;

private:
	void RefreshLists();
	void LoadRecordingFolders();
	void SaveRecordingFolders() const;
	FReply OnBuildClicked();
	FReply OnCancelClicked();
	FReply OnAddFolderClicked();
	bool CanBuild() const;
	FText GetOutputPathPreview() const;
	FText GetSummaryText() const { return FText::FromString(SummaryLine); }
	void AppendLog(const FString& Line);
	TSharedRef<ITableRow> MakeFileRow(TSharedPtr<FPSOForgeCandidateFile> Item, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> MakeFolderRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable);
	TArray<FString> SelectedPaths(const TArray<TSharedPtr<FPSOForgeCandidateFile>>& Items, const FString& FormatFilter) const;

	TArray<TSharedPtr<FPSOForgeCandidateFile>> Recordings;
	TArray<TSharedPtr<FPSOForgeCandidateFile>> StableKeys;
	TArray<TSharedPtr<FString>> ShaderFormats;
	TSharedPtr<FString> SelectedFormat;
	/** Dev-managed folders scanned recursively for recordings; persisted per project. */
	TArray<TSharedPtr<FString>> RecordingFolders;
	FString SummaryLine;
	bool bDeployToBuild = true;

	TSharedPtr<SListView<TSharedPtr<FString>>> FoldersView;
	TSharedPtr<SListView<TSharedPtr<FPSOForgeCandidateFile>>> RecordingsView;
	TSharedPtr<SListView<TSharedPtr<FPSOForgeCandidateFile>>> StableKeysView;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> FormatCombo;
	TSharedPtr<SMultiLineEditableTextBox> LogBox;
	TSharedPtr<FPSOForgeCacheBuildTask> Task;
};

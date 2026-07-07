// Copyright (c) 2026 PSOForge contributors. MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"

class FMonitoredProcess;

/** One discovered .rec.upipelinecache or .shk file offered in the cache builder UI. */
struct FPSOForgeCandidateFile
{
	FString Path;
	int64 SizeBytes = 0;
	FDateTime ModTime;
	bool bSelected = true;
};

/**
 * Runs `UnrealEditor-Cmd -run=ShaderPipelineCacheTools expand` as a monitored subprocess:
 * copies the chosen recordings and stable-key files to a scratch folder, launches the commandlet,
 * streams its output, and on success optionally deploys the .spc into Build/<Platform>/PipelineCaches.
 * All delegates fire on the game thread.
 */
class FPSOForgeCacheBuildTask : public TSharedFromThis<FPSOForgeCacheBuildTask>
{
public:
	DECLARE_DELEGATE_OneParam(FOnLogLine, const FString&);
	DECLARE_DELEGATE_TwoParams(FOnFinished, bool /*bSuccess*/, const FString& /*Summary*/);

	~FPSOForgeCacheBuildTask();

	/** Discovery helpers shared with the UI. Recursive scan lets a folder be a whole build root. */
	static TArray<FString> DefaultRecordingFolders();
	static TArray<FString> CookedMetadataFolders();
	static TArray<FPSOForgeCandidateFile> DiscoverFiles(const TArray<FString>& Folders, const FString& ExtensionFilter, bool bRecursive);

	bool Start(const TArray<FString>& RecordingFiles, const TArray<FString>& StableKeyFiles,
		const FString& ShaderFormat, bool bDeployToBuild);
	void Cancel();
	bool IsRunning() const { return bRunning; }

	FOnLogLine OnLogLine;
	FOnFinished OnFinished;

private:
	void HandleOutputLine(const FString& Line);
	void HandleCompleted(int32 ReturnCode);

	TSharedPtr<FMonitoredProcess> Process;
	FString OutputSpcPath;
	FString DeployDir;
	FString LastPSOCountLine;
	bool bDeploy = false;
	bool bRunning = false;
};

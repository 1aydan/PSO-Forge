// Copyright (c) 2026 PSOForge contributors. MIT License.

#include "PSOForgeCacheBuildTask.h"

#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/MonitoredProcess.h"
#include "Misc/Paths.h"

static FString PSOForgeScratchDir()
{
	return FPaths::ProjectSavedDir() / TEXT("PSOForge") / TEXT("EditorBuild");
}

FPSOForgeCacheBuildTask::~FPSOForgeCacheBuildTask()
{
	Cancel();
}

TArray<FString> FPSOForgeCacheBuildTask::DefaultRecordingFolders()
{
	// Seed folders for a fresh panel; devs add their own build locations, which persist per project.
	const FString ProjectName = FApp::GetProjectName();
	TArray<FString> Folders;
	Folders.Add(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("CollectedPSOs")));

	const FString StagedRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("StagedBuilds"));
	if (IFileManager::Get().DirectoryExists(*StagedRoot))
	{
		Folders.Add(StagedRoot);
	}
	const FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
	if (!LocalAppData.IsEmpty() && IFileManager::Get().DirectoryExists(*(LocalAppData / ProjectName / TEXT("Saved/CollectedPSOs"))))
	{
		Folders.Add(LocalAppData / ProjectName / TEXT("Saved/CollectedPSOs"));
	}
	return Folders;
}

TArray<FString> FPSOForgeCacheBuildTask::CookedMetadataFolders()
{
	// Saved/Cooked/<Platform>/<Project>/Metadata/PipelineCaches for every cooked platform.
	TArray<FString> Folders;
	const FString CookedRoot = FPaths::ProjectSavedDir() / TEXT("Cooked");
	TArray<FString> PlatformDirs;
	IFileManager::Get().FindFiles(PlatformDirs, *(CookedRoot / TEXT("*")), /*Files*/ false, /*Directories*/ true);
	for (const FString& Platform : PlatformDirs)
	{
		Folders.Add(CookedRoot / Platform / FApp::GetProjectName() / TEXT("Metadata/PipelineCaches"));
	}
	return Folders;
}

TArray<FPSOForgeCandidateFile> FPSOForgeCacheBuildTask::DiscoverFiles(const TArray<FString>& Folders, const FString& ExtensionFilter, bool bRecursive)
{
	TArray<FPSOForgeCandidateFile> Results;
	TSet<FString> Seen;
	for (const FString& Folder : Folders)
	{
		TArray<FString> Files;
		if (bRecursive)
		{
			// Folders may be whole build roots (cooked builds live wherever the dev put them);
			// FindFilesRecursive returns full paths.
			IFileManager::Get().FindFilesRecursive(Files, *Folder, *ExtensionFilter, /*Files*/ true, /*Directories*/ false);
		}
		else
		{
			TArray<FString> Names;
			IFileManager::Get().FindFiles(Names, *(Folder / ExtensionFilter), /*Files*/ true, /*Directories*/ false);
			for (const FString& Name : Names)
			{
				Files.Add(Folder / Name);
			}
		}

		for (const FString& File : Files)
		{
			const FString FullPath = FPaths::ConvertRelativePathToFull(File);
			bool bAlreadySeen = false;
			Seen.Add(FullPath, &bAlreadySeen);
			if (bAlreadySeen)
			{
				continue;
			}

			FPSOForgeCandidateFile Candidate;
			Candidate.Path = FullPath;
			Candidate.SizeBytes = IFileManager::Get().FileSize(*Candidate.Path);
			Candidate.ModTime = IFileManager::Get().GetTimeStamp(*Candidate.Path);
			if (Candidate.SizeBytes > 0)
			{
				Results.Add(MoveTemp(Candidate));
			}
		}
	}
	Results.Sort([](const FPSOForgeCandidateFile& A, const FPSOForgeCandidateFile& B) { return A.ModTime > B.ModTime; });
	return Results;
}

bool FPSOForgeCacheBuildTask::Start(const TArray<FString>& RecordingFiles, const TArray<FString>& StableKeyFiles,
	const FString& ShaderFormat, bool bDeployToBuild)
{
	if (bRunning || RecordingFiles.Num() == 0 || StableKeyFiles.Num() == 0)
	{
		return false;
	}

	const FString Scratch = PSOForgeScratchDir();
	const FString RecDir = Scratch / TEXT("rec");
	const FString ShkDir = Scratch / TEXT("shk");
	IFileManager::Get().DeleteDirectory(*Scratch, /*RequireExists*/ false, /*Tree*/ true);
	IFileManager::Get().MakeDirectory(*RecDir, /*Tree*/ true);
	IFileManager::Get().MakeDirectory(*ShkDir, /*Tree*/ true);

	for (const FString& File : RecordingFiles)
	{
		IFileManager::Get().Copy(*(RecDir / FPaths::GetCleanFilename(File)), *File);
	}
	for (const FString& File : StableKeyFiles)
	{
		IFileManager::Get().Copy(*(ShkDir / FPaths::GetCleanFilename(File)), *File);
	}

	const FString ProjectName = FApp::GetProjectName();
	OutputSpcPath = FPaths::ProjectSavedDir() / TEXT("PSOForge") / FString::Printf(TEXT("%s_%s.spc"), *ProjectName, *ShaderFormat);
	IFileManager::Get().Delete(*OutputSpcPath, /*RequireExists*/ false);
	bDeploy = bDeployToBuild;
	DeployDir = FPaths::ProjectDir() / TEXT("Build/Windows/PipelineCaches");
	LastPSOCountLine.Reset();

	const FString EditorCmd = FPaths::ConvertRelativePathToFull(
		FPaths::EngineDir() / TEXT("Binaries") / FPlatformProcess::GetBinariesSubdirectory() / TEXT("UnrealEditor-Cmd.exe"));
	const FString Args = FString::Printf(TEXT("\"%s\" -run=ShaderPipelineCacheTools expand \"%s\" \"%s\" \"%s\" -unattended"),
		*FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()),
		*FPaths::ConvertRelativePathToFull(RecDir / TEXT("*.rec.upipelinecache")),
		*FPaths::ConvertRelativePathToFull(ShkDir / TEXT("*.shk")),
		*FPaths::ConvertRelativePathToFull(OutputSpcPath));

	OnLogLine.ExecuteIfBound(FString::Printf(TEXT("Running: %s %s"), *EditorCmd, *Args));

	Process = MakeShared<FMonitoredProcess>(EditorCmd, Args, /*bHidden*/ true);

	TWeakPtr<FPSOForgeCacheBuildTask> WeakTask = AsShared();
	Process->OnOutput().BindLambda([WeakTask](FString Line)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakTask, Line = MoveTemp(Line)]()
		{
			if (TSharedPtr<FPSOForgeCacheBuildTask> Pinned = WeakTask.Pin())
			{
				Pinned->HandleOutputLine(Line);
			}
		});
	});
	Process->OnCompleted().BindLambda([WeakTask](int32 ReturnCode)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakTask, ReturnCode]()
		{
			if (TSharedPtr<FPSOForgeCacheBuildTask> Pinned = WeakTask.Pin())
			{
				Pinned->HandleCompleted(ReturnCode);
			}
		});
	});
	Process->OnCanceled().BindLambda([WeakTask]()
	{
		AsyncTask(ENamedThreads::GameThread, [WeakTask]()
		{
			if (TSharedPtr<FPSOForgeCacheBuildTask> Pinned = WeakTask.Pin())
			{
				Pinned->HandleCompleted(/*ReturnCode*/ -1);
			}
		});
	});

	if (!Process->Launch())
	{
		OnLogLine.ExecuteIfBound(TEXT("Failed to launch UnrealEditor-Cmd.exe."));
		Process.Reset();
		return false;
	}

	bRunning = true;
	return true;
}

void FPSOForgeCacheBuildTask::Cancel()
{
	if (Process.IsValid() && bRunning)
	{
		Process->Cancel(/*bKillTree*/ true);
	}
}

void FPSOForgeCacheBuildTask::HandleOutputLine(const FString& Line)
{
	// The commandlet logs merged totals as "... N PSOs ..." lines; keep the last one for the summary.
	if (Line.Contains(TEXT(" PSO")))
	{
		LastPSOCountLine = Line;
	}
	OnLogLine.ExecuteIfBound(Line);
}

void FPSOForgeCacheBuildTask::HandleCompleted(int32 ReturnCode)
{
	if (!bRunning)
	{
		return;
	}
	bRunning = false;
	Process.Reset();

	const int64 OutputSize = IFileManager::Get().FileSize(*OutputSpcPath);
	const bool bSuccess = ReturnCode == 0 && OutputSize > 0;

	FString Summary;
	if (bSuccess)
	{
		FString FinalPath = OutputSpcPath;
		if (bDeploy)
		{
			const FString Deployed = DeployDir / FPaths::GetCleanFilename(OutputSpcPath);
			IFileManager::Get().MakeDirectory(*DeployDir, /*Tree*/ true);
			if (IFileManager::Get().Copy(*Deployed, *OutputSpcPath) == COPY_OK)
			{
				FinalPath = Deployed;
			}
			else
			{
				Summary = TEXT("Built, but deploying to Build/Windows/PipelineCaches failed. ");
			}
		}
		Summary += FString::Printf(TEXT("Cache built: %s (%lld KB)."), *FinalPath, OutputSize / 1024);
		if (!LastPSOCountLine.IsEmpty())
		{
			Summary += FString::Printf(TEXT(" %s"), *LastPSOCountLine.TrimStartAndEnd());
		}
	}
	else
	{
		Summary = ReturnCode == -1
			? TEXT("Cache build cancelled.")
			: FString::Printf(TEXT("Cache build failed (exit code %d) — see log above."), ReturnCode);
	}

	OnFinished.ExecuteIfBound(bSuccess, Summary);
}

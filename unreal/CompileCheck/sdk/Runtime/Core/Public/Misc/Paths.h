// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/Paths.h (from UE 5.x source knowledge).
// Not part of CoreMinimal.h.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Containers/StringFwd.h"

class FPaths
{
public:
	static bool ShouldSaveToUserDir();
	static FString LaunchDir();
	static FString EngineDir();
	static FString EngineUserDir();
	static FString EngineVersionAgnosticUserDir();
	static FString EngineContentDir();
	static FString EngineConfigDir();
	static FString EngineIntermediateDir();
	static FString EngineSavedDir();
	static FString EnginePluginsDir();
	static FString RootDir();
	static FString ProjectDir();
	static FString ProjectUserDir();
	static FString ProjectContentDir();
	static FString ProjectConfigDir();
	static const FString& ProjectSavedDir();
	static FString ProjectIntermediateDir();
	static FString ProjectPluginsDir();
	static FString ProjectModsDir();
	static FString ProjectPersistentDownloadDir();
	static FString ProjectLogDir();
	static FString AutomationDir();
	static FString AutomationTransientDir();
	static FString AutomationLogDir();
	static FString CloudDir();
	static FString GameDevelopersDir();
	static FString BugItDir();
	static FString ScreenShotDir();
	static FString VideoCaptureDir();
	static FString ProfilingDir();
	static FString ShaderWorkingDir();
	static FString GeneratedConfigDir();
	static FString SandboxesDir();
	static FString SourceConfigDir();
	static FString GetProjectFilePath();
	static bool IsProjectFilePathSet();
	static FString GetExtension(const FString& InPath, bool bIncludeDot = false);
	static FString GetCleanFilename(const FString& InPath);
	static FString GetCleanFilename(FString&& InPath);
	static FString GetBaseFilename(const FString& InPath, bool bRemovePath = true);
	static FString GetBaseFilename(FString&& InPath, bool bRemovePath = true);
	static FString GetPath(const FString& InPath);
	static FString GetPath(FString&& InPath);
	static FString GetPathLeaf(const FString& InPath);
	static FString GetPathLeaf(FString&& InPath);
	static FString ChangeExtension(const FString& InPath, const FString& InNewExtension);
	static FString SetExtension(const FString& InPath, const FString& InNewExtension);
	static bool FileExists(const FString& InPath);
	static bool DirectoryExists(const FString& InPath);
	static bool IsDrive(const FString& InPath);
	static bool IsRelative(const FString& InPath);
	static void NormalizeFilename(FString& InPath);
	static bool IsSamePath(const FString& PathA, const FString& PathB);
	static bool IsUnderDirectory(const FString& InPath, const FString& InDirectory);
	static void NormalizeDirectoryName(FString& InPath);
	static bool CollapseRelativeDirectories(FString& InPath, bool bCollapseAllPossible = false);
	static void RemoveDuplicateSlashes(FString& InPath);
	static FString RemoveDuplicateSlashes(const FString& InPath);
	static void MakeStandardFilename(FString& InPath);
	static void MakePlatformFilename(FString& InPath);
	static bool MakePathRelativeTo(FString& InPath, const TCHAR* InRelativeTo);
	static FString ConvertRelativePathToFull(const FString& InPath);
	static FString ConvertRelativePathToFull(FString&& InPath);
	static FString ConvertRelativePathToFull(const FString& BasePath, const FString& InPath);
	static FString ConvertRelativePathToFull(const FString& BasePath, FString&& InPath);
	static FString ConvertToSandboxPath(const FString& InPath, const TCHAR* InSandboxName);
	static FString CreateTempFilename(const TCHAR* Path, const TCHAR* Prefix = TEXT(""), const TCHAR* Extension = TEXT(".tmp"));
	static FString GetInvalidFileSystemChars();
	static FString MakeValidFileName(const FString& InString, const TCHAR InReplacementChar = TEXT('\0'));
	static bool ValidatePath(const FString& InPath, FText* OutReason = nullptr);
	static void Split(const FString& InPath, FString& PathPart, FString& FilenamePart, FString& ExtensionPart);
	static const FString& GetRelativePathToRoot();
	template <typename... PathTypes>
	static FString Combine(PathTypes&&... InPaths) requires (sizeof...(PathTypes) > 0) && ((std::is_convertible_v<PathTypes&&, const TCHAR*> || std::is_convertible_v<PathTypes&&, const FString&> || std::is_convertible_v<PathTypes&&, FStringView>) && ...) UE_STUB_BODY
};

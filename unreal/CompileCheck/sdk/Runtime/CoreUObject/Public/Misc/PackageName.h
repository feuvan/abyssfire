// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/Misc/PackageName.h (from UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"

struct FPackagePath;

class FPackageName
{
public:
	enum class EFlexNameType { Invalid, PackageName, LocalPath, ObjectPath };
	static FString ConvertToLongScriptPackageName(const TCHAR* InShortName);
	static bool TryConvertFilenameToLongPackageName(const FString& InFilename, FString& OutPackageName, FString* OutFailureReason = nullptr);
	static FString FilenameToLongPackageName(const FString& InFilename);
	static bool TryConvertLongPackageNameToFilename(const FString& InLongPackageName, FString& OutFilename, const FString& InExtension = TEXT(""));
	static FString LongPackageNameToFilename(const FString& InLongPackageName, const FString& InExtension = TEXT(""));
	static FString GetLongPackagePath(const FString& InLongPackageName);
	static FString GetLongPackageAssetName(const FString& InLongPackageName);
	static FString GetShortName(const FString& LongName);
	static FString GetShortName(const UPackage* Package);
	static FName GetShortFName(const FString& LongName);
	static bool IsValidLongPackageName(FStringView InLongPackageName, bool bIncludeReadOnlyRoots = false, FText* OutReason = nullptr);
	static bool IsValidObjectPath(FStringView InObjectPath, FText* OutReason = nullptr);
	static bool IsValidPath(FStringView InPath);
	static bool DoesPackageExist(const FString& LongPackageName, FString* OutFilename = nullptr, bool InAllowTextFormats = true);
	static bool DoesPackageExist(const FString& LongPackageName, const FGuid* Guid, FString* OutFilename, bool InAllowTextFormats = true);
	static bool DoesPackageExist(const FPackagePath& PackagePath, FPackagePath* OutPackagePath = nullptr);
	static bool SearchForPackageOnDisk(const FString& PackageName, FString* OutLongPackageName = nullptr, FString* OutFilename = nullptr);
	static FString ObjectPathToPackageName(const FString& InObjectPath);
	static FString ObjectPathToObjectName(const FString& InObjectPath);
	static bool IsScriptPackage(FStringView InPackageName);
	static const FString& GetAssetPackageExtension();
	static const FString& GetMapPackageExtension();
	static void RegisterMountPoint(const FString& RootPath, const FString& ContentPath);
	static void UnRegisterMountPoint(const FString& RootPath, const FString& ContentPath);
	static bool MountPointExists(const FString& RootPath);
};

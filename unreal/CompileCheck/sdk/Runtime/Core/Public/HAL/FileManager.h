// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/HAL/FileManager.h (from UE 5.x source knowledge).
// Not part of CoreMinimal.h.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/DateTime.h"
#include "Templates/Function.h"

class FArchive;

enum EFileWrite
{
	FILEWRITE_None = 0x00,
	FILEWRITE_NoFail = 0x01,
	FILEWRITE_NoReplaceExisting = 0x02,
	FILEWRITE_EvenIfReadOnly = 0x04,
	FILEWRITE_Append = 0x08,
	FILEWRITE_AllowRead = 0x10,
	FILEWRITE_Silent = 0x20,
};
enum EFileRead
{
	FILEREAD_None = 0x00,
	FILEREAD_NoFail = 0x01,
	FILEREAD_Silent = 0x02,
	FILEREAD_AllowWrite = 0x04,
};
enum ECopyResult { COPY_OK = 0x00, COPY_Fail = 0x01, COPY_Canceled = 0x02 };
struct FCopyProgress { virtual bool Poll(float Fraction) = 0; };

class IFileManager
{
protected:
	IFileManager() {}
public:
	static IFileManager& Get();
	virtual void ProcessCommandLineOptions() = 0;
	virtual void SetSandboxEnabled(bool bInEnabled) = 0;
	virtual bool IsSandboxEnabled() const = 0;
	virtual FArchive* CreateFileReader(const TCHAR* Filename, uint32 ReadFlags = 0) = 0;
	virtual FArchive* CreateFileWriter(const TCHAR* Filename, uint32 WriteFlags = 0) = 0;
	virtual FArchive* CreateDebugFileWriter(const TCHAR* Filename, uint32 WriteFlags = 0) = 0;
	virtual bool IsReadOnly(const TCHAR* Filename) = 0;
	virtual bool Delete(const TCHAR* Filename, bool RequireExists = false, bool EvenReadOnly = false, bool Quiet = false) = 0;
	virtual uint32 Copy(const TCHAR* Dest, const TCHAR* Src, bool Replace = true, bool EvenIfReadOnly = false, bool Attributes = false, FCopyProgress* Progress = nullptr, EFileRead ReadFlags = FILEREAD_None, EFileWrite WriteFlags = FILEWRITE_None) = 0;
	virtual bool Move(const TCHAR* Dest, const TCHAR* Src, bool Replace = true, bool EvenIfReadOnly = false, bool Attributes = false, bool bDoNotRetryOrError = false) = 0;
	virtual bool FileExists(const TCHAR* Filename) = 0;
	virtual bool DirectoryExists(const TCHAR* InDirectory) = 0;
	virtual bool MakeDirectory(const TCHAR* Path, bool Tree = false) = 0;
	virtual bool DeleteDirectory(const TCHAR* Path, bool RequireExists = false, bool Tree = false) = 0;
	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) = 0;
	virtual void FindFiles(TArray<FString>& FileNames, const TCHAR* Filename, bool Files, bool Directories) = 0;
	virtual void FindFiles(TArray<FString>& FoundFiles, const TCHAR* Directory, const TCHAR* FileExtension = nullptr) = 0;
	virtual void FindFilesRecursive(TArray<FString>& FileNames, const TCHAR* StartDirectory, const TCHAR* Filename, bool Files, bool Directories, bool bClearFileNames = true) = 0;
	virtual bool IterateDirectory(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor) = 0;
	virtual bool IterateDirectory(const TCHAR* Directory, IPlatformFile::FDirectoryVisitorFunc Visitor) = 0;
	virtual bool IterateDirectoryRecursively(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor) = 0;
	virtual bool IterateDirectoryRecursively(const TCHAR* Directory, IPlatformFile::FDirectoryVisitorFunc Visitor) = 0;
	virtual double GetFileAgeSeconds(const TCHAR* Filename) = 0;
	virtual FDateTime GetTimeStamp(const TCHAR* Path) = 0;
	virtual void GetTimeStampPair(const TCHAR* PathA, const TCHAR* PathB, FDateTime& OutTimeStampA, FDateTime& OutTimeStampB) = 0;
	virtual bool SetTimeStamp(const TCHAR* Path, FDateTime TimeStamp) = 0;
	virtual FDateTime GetAccessTimeStamp(const TCHAR* Filename) = 0;
	virtual FString ConvertToRelativePath(const TCHAR* Filename) = 0;
	virtual FString ConvertToAbsolutePathForExternalAppForRead(const TCHAR* Filename) = 0;
	virtual FString ConvertToAbsolutePathForExternalAppForWrite(const TCHAR* Filename) = 0;
	virtual int64 FileSize(const TCHAR* Filename) = 0;
	virtual bool SendMessageToServer(const TCHAR* Message, IPlatformFile::IFileServerMessageHandler* Handler) = 0;
	virtual FString GetFilenameOnDisk(const TCHAR* Filename) = 0;
};

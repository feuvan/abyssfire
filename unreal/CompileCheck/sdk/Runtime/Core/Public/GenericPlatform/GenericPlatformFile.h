// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/GenericPlatform/GenericPlatformFile.h
// (from UE 5.x source knowledge). IPlatformFile / IFileHandle: the pak-aware low-level file layer.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Misc/DateTime.h"
#include "Templates/Function.h"

enum class EPlatformFileRead : uint8 { None = 0x0, AllowWrite = 0x01 };
enum class EPlatformFileWrite : uint8 { None = 0x0, AllowRead = 0x01 };

class IFileHandle
{
public:
	virtual ~IFileHandle() {}
	virtual int64 Tell() = 0;
	virtual bool Seek(int64 NewPosition) = 0;
	virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd = 0) = 0;
	virtual bool Read(uint8* Destination, int64 BytesToRead) = 0;
	virtual bool ReadAt(uint8* Destination, int64 BytesToRead, int64 Offset) = 0;
	virtual bool Write(const uint8* Source, int64 BytesToWrite) = 0;
	virtual bool Flush(const bool bFullFlush = false) = 0;
	virtual bool Truncate(int64 NewSize) = 0;
	virtual void ShrinkBuffers() {}
	virtual int64 Size();
};

struct FFileStatData
{
	FFileStatData();
	FFileStatData(FDateTime InCreationTime, FDateTime InAccessTime, FDateTime InModificationTime, const int64 InFileSize, const bool InIsDirectory, const bool InIsReadOnly);
	FDateTime CreationTime;
	FDateTime AccessTime;
	FDateTime ModificationTime;
	int64 FileSize;
	bool bIsDirectory : 1;
	bool bIsReadOnly : 1;
	bool bIsValid : 1;
};

class IPlatformFile
{
public:
	static IPlatformFile& GetPlatformPhysical();
	static const TCHAR* GetPhysicalTypeName();
	virtual ~IPlatformFile() {}
	virtual void SetSandboxEnabled(bool bInEnabled) {}
	virtual bool IsSandboxEnabled() const { return false; }
	virtual bool ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const { return false; }
	virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CmdLine) = 0;
	virtual IPlatformFile* GetLowerLevel() = 0;
	virtual void SetLowerLevel(IPlatformFile* NewLowerLevel) = 0;
	virtual const TCHAR* GetName() const = 0;
	virtual bool FileExists(const TCHAR* Filename) = 0;
	virtual int64 FileSize(const TCHAR* Filename) = 0;
	virtual bool DeleteFile(const TCHAR* Filename) = 0;
	virtual bool IsReadOnly(const TCHAR* Filename) = 0;
	virtual bool MoveFile(const TCHAR* To, const TCHAR* From) = 0;
	virtual bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) = 0;
	virtual FDateTime GetTimeStamp(const TCHAR* Filename) = 0;
	virtual void SetTimeStamp(const TCHAR* Filename, FDateTime DateTime) = 0;
	virtual FDateTime GetAccessTimeStamp(const TCHAR* Filename) = 0;
	virtual FString GetFilenameOnDisk(const TCHAR* Filename) = 0;
	virtual IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite = false) = 0;
	virtual IFileHandle* OpenReadNoBuffering(const TCHAR* Filename, bool bAllowWrite = false);
	virtual IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) = 0;
	virtual bool DirectoryExists(const TCHAR* Directory) = 0;
	virtual bool CreateDirectory(const TCHAR* Directory) = 0;
	virtual bool DeleteDirectory(const TCHAR* Directory) = 0;
	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) = 0;

	class FDirectoryVisitor
	{
	public:
		virtual ~FDirectoryVisitor() {}
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) = 0;
	};
	typedef TFunctionRef<bool(const TCHAR*, bool)> FDirectoryVisitorFunc;
	class FDirectoryStatVisitor
	{
	public:
		virtual ~FDirectoryStatVisitor() {}
		virtual bool Visit(const TCHAR* FilenameOrDirectory, const FFileStatData& StatData) = 0;
	};
	typedef TFunctionRef<bool(const TCHAR*, const FFileStatData&)> FDirectoryStatVisitorFunc;
	class IFileServerMessageHandler
	{
	public:
		virtual ~IFileServerMessageHandler() {}
	};

	virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) = 0;
	virtual bool IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor) = 0;
	virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitorFunc Visitor);
	virtual bool IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitorFunc Visitor);
	virtual bool IterateDirectoryRecursively(const TCHAR* Directory, FDirectoryVisitor& Visitor);
	virtual bool IterateDirectoryStatRecursively(const TCHAR* Directory, FDirectoryStatVisitor& Visitor);
	virtual bool IterateDirectoryRecursively(const TCHAR* Directory, FDirectoryVisitorFunc Visitor);
	virtual bool IterateDirectoryStatRecursively(const TCHAR* Directory, FDirectoryStatVisitorFunc Visitor);
	virtual void FindFiles(TArray<FString>& FoundFiles, const TCHAR* Directory, const TCHAR* FileExtension);
	virtual void FindFilesRecursively(TArray<FString>& FoundFiles, const TCHAR* Directory, const TCHAR* FileExtension);
	virtual bool DeleteDirectoryRecursively(const TCHAR* Directory);
	virtual bool CreateDirectoryTree(const TCHAR* Directory);
	virtual bool CopyFile(const TCHAR* To, const TCHAR* From, EPlatformFileRead ReadFlags = EPlatformFileRead::None, EPlatformFileWrite WriteFlags = EPlatformFileWrite::None);
	virtual bool CopyDirectoryTree(const TCHAR* DestinationDirectory, const TCHAR* Source, bool bOverwriteAllExisting);
	virtual FString ConvertToAbsolutePathForExternalAppForRead(const TCHAR* Filename);
	virtual FString ConvertToAbsolutePathForExternalAppForWrite(const TCHAR* Filename);
};

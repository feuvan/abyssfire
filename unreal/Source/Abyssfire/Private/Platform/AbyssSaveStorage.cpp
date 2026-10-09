#include "Platform/AbyssSaveStorage.h"

#include "Abyssfire.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Templates/UniquePtr.h"

#include "abyss/save/SaveData.h"

namespace
{
	const TCHAR* const AbyssSaveStorage_SaveSettingsFileName = TEXT("settings.json");

	IPlatformFile& AbyssSaveStorage_SavePlatformFile()
	{
		return FPlatformFileManager::Get().GetPlatformFile();
	}

	FString AbyssSaveStorage_SlotFileName(int32 Slot)
	{
		// DECISIONS U2: Saved/SaveGames/abyssfire_slot{N}.json
		return FString::Printf(TEXT("abyssfire_slot%d.json"), Slot);
	}
}

FAbyssSaveStorage::FAbyssSaveStorage()
	: SaveDir(FPaths::ProjectSavedDir() / TEXT("SaveGames"))
{
	AbyssSaveStorage_SavePlatformFile().CreateDirectoryTree(*SaveDir);
}

FAbyssSaveStorage::~FAbyssSaveStorage()
{
	Flush();
}

FString FAbyssSaveStorage::GetSlotPath(int32 Slot) const
{
	return SaveDir / AbyssSaveStorage_SlotFileName(Slot);
}

bool FAbyssSaveStorage::ReadFileBytes(const FString& Path, std::string& Out)
{
	Out.clear();
	TArray<uint8> Bytes;
	if (!AbyssSaveStorage_SavePlatformFile().FileExists(*Path) || !FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent))
	{
		return false;
	}
	Out.assign(reinterpret_cast<const char*>(Bytes.GetData()), static_cast<size_t>(Bytes.Num()));
	return true;
}

bool FAbyssSaveStorage::WriteFileAtomic(const FString& Path, const std::string& Bytes)
{
	IPlatformFile& PlatformFile = AbyssSaveStorage_SavePlatformFile();
	const FString TmpPath = Path + TEXT(".tmp");
	const FString BakPath = Path + TEXT(".bak");

	PlatformFile.CreateDirectoryTree(*FPaths::GetPath(Path));
	{
		TUniquePtr<IFileHandle> Handle(PlatformFile.OpenWrite(*TmpPath, /*bAppend*/ false, /*bAllowRead*/ false));
		if (!Handle)
		{
			UE_LOG(LogAbyss, Error, TEXT("Save: cannot open %s for writing"), *TmpPath);
			return false;
		}
		const bool bWritten = Bytes.empty()
			|| Handle->Write(reinterpret_cast<const uint8*>(Bytes.data()), static_cast<int64>(Bytes.size()));
		const bool bFlushed = bWritten && Handle->Flush(/*bFullFlush*/ true);
		if (!bWritten || !bFlushed)
		{
			UE_LOG(LogAbyss, Error, TEXT("Save: writing %s failed"), *TmpPath);
			Handle.Reset();
			PlatformFile.DeleteFile(*TmpPath);
			return false;
		}
	}
	// Keep the previous good file as .bak (fallback when the main file fails to parse).
	if (PlatformFile.FileExists(*Path))
	{
		if (PlatformFile.FileExists(*BakPath))
		{
			PlatformFile.DeleteFile(*BakPath);
		}
		if (!PlatformFile.MoveFile(*BakPath, *Path))
		{
			UE_LOG(LogAbyss, Warning, TEXT("Save: could not keep a backup of %s"), *Path);
			PlatformFile.DeleteFile(*Path);
		}
	}
	if (!PlatformFile.MoveFile(*Path, *TmpPath))
	{
		UE_LOG(LogAbyss, Error, TEXT("Save: renaming %s -> %s failed"), *TmpPath, *Path);
		return false;
	}
	return true;
}

bool FAbyssSaveStorage::Read(int32_t Slot, std::string& Out)
{
	check(IsInGameThread());
	if (!IsValidSlot(Slot))
	{
		return false;
	}
	Flush();
	return ReadFileBytes(GetSlotPath(Slot), Out);
}

bool FAbyssSaveStorage::ReadBackup(int32 Slot, std::string& Out)
{
	check(IsInGameThread());
	if (!IsValidSlot(Slot))
	{
		return false;
	}
	Flush();
	return ReadFileBytes(GetSlotPath(Slot) + TEXT(".bak"), Out);
}

bool FAbyssSaveStorage::HasBackup(int32 Slot)
{
	check(IsInGameThread());
	if (!IsValidSlot(Slot))
	{
		return false;
	}
	Flush();
	return AbyssSaveStorage_SavePlatformFile().FileExists(*(GetSlotPath(Slot) + TEXT(".bak")));
}

bool FAbyssSaveStorage::Write(int32_t Slot, std::string_view Bytes)
{
	check(IsInGameThread());
	if (!IsValidSlot(Slot))
	{
		return false;
	}
	// The worker owns its own copy; writes are chained so they land in submission order.
	auto Body = [TargetPath = GetSlotPath(Slot), Payload = std::string(Bytes)]()
	{
		WriteFileAtomic(TargetPath, Payload);
	};
	if (LastWrite.IsValid() && !LastWrite.IsCompleted())
	{
		LastWrite = UE::Tasks::Launch(TEXT("AbyssSaveWrite"), MoveTemp(Body), UE::Tasks::Prerequisites(LastWrite));
	}
	else
	{
		LastWrite = UE::Tasks::Launch(TEXT("AbyssSaveWrite"), MoveTemp(Body));
	}
	return true;
}

bool FAbyssSaveStorage::WriteSync(int32 Slot, std::string_view Bytes)
{
	check(IsInGameThread());
	if (!IsValidSlot(Slot))
	{
		return false;
	}
	Flush();
	return WriteFileAtomic(GetSlotPath(Slot), std::string(Bytes));
}

void FAbyssSaveStorage::Flush()
{
	if (LastWrite.IsValid())
	{
		LastWrite.Wait();
		LastWrite = UE::Tasks::FTask();
	}
}

bool FAbyssSaveStorage::Remove(int32_t Slot)
{
	check(IsInGameThread());
	if (!IsValidSlot(Slot))
	{
		return false;
	}
	Flush();
	IPlatformFile& PlatformFile = AbyssSaveStorage_SavePlatformFile();
	const FString Path = GetSlotPath(Slot);
	bool bOk = true;
	for (const FString& Candidate : { Path, Path + TEXT(".bak"), Path + TEXT(".tmp") })
	{
		if (PlatformFile.FileExists(*Candidate) && !PlatformFile.DeleteFile(*Candidate))
		{
			UE_LOG(LogAbyss, Warning, TEXT("Save: could not delete %s"), *Candidate);
			bOk = false;
		}
	}
	return bOk;
}

void FAbyssSaveStorage::List(std::vector<abyss::SaveSlotInfo>& Out)
{
	check(IsInGameThread());
	Flush();
	Out.clear();
	Out.reserve(NumSlots);
	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		abyss::SaveSlotInfo Info;
		Info.slot = Slot;
		std::string Bytes;
		abyss::SaveData Parsed;
		bool bParsed = ReadFileBytes(GetSlotPath(Slot), Bytes)
			&& abyss::ParseSave(Bytes, Parsed, nullptr) == abyss::SaveError::None;
		if (!bParsed)
		{
			// Main file missing / corrupt: show the backup's character so the player can still pick the slot.
			bParsed = ReadFileBytes(GetSlotPath(Slot) + TEXT(".bak"), Bytes)
				&& abyss::ParseSave(Bytes, Parsed, nullptr) == abyss::SaveError::None;
		}
		if (bParsed)
		{
			Info = abyss::SummarizeSave(Slot, Parsed);
			Info.slot = Slot;
			Info.exists = true;
		}
		Out.push_back(std::move(Info));
	}
}

bool FAbyssSaveStorage::ReadSettings(std::string& Out)
{
	check(IsInGameThread());
	return ReadFileBytes(SaveDir / AbyssSaveStorage_SaveSettingsFileName, Out);
}

bool FAbyssSaveStorage::WriteSettings(std::string_view Bytes)
{
	check(IsInGameThread());
	return WriteFileAtomic(SaveDir / AbyssSaveStorage_SaveSettingsFileName, std::string(Bytes));
}

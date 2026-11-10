// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/FileHelper.h (from UE 5.x source knowledge).
// Not part of CoreMinimal.h.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"
#include "Containers/StringView.h"
#include "HAL/FileManager.h"

struct FFileHelper
{
	enum class EHashOptions { None = 0, EnableVerify = 1 << 0, ErrorMissingHash = 1 << 1 };
	enum class EEncodingOptions { AutoDetect, ForceAnsi, ForceUnicode, ForceUTF8, ForceUTF8WithoutBOM };
	enum class EColorChannel { R, G, B, A, All };
	static bool LoadFileToArray(TArray<uint8>& Result, const TCHAR* Filename, uint32 Flags = 0);
	static bool LoadFileToArray(TArray64<uint8>& Result, const TCHAR* Filename, uint32 Flags = 0);
	static void BufferToString(FString& Result, const uint8* Buffer, int32 Size);
	static bool LoadFileToString(FString& Result, const TCHAR* Filename, EHashOptions VerifyFlags = EHashOptions::None, uint32 ReadFlags = 0);
	static bool LoadFileToString(FString& Result, class IPlatformFile* PlatformFile, const TCHAR* Filename, EHashOptions VerifyFlags = EHashOptions::None, uint32 ReadFlags = 0);
	static bool LoadFileToStringArray(TArray<FString>& Result, const TCHAR* Filename);
	static bool LoadFileToStringArrayWithPredicate(TArray<FString>& Result, const TCHAR* Filename, TFunctionRef<bool(const FString&)> Predicate);
	static bool SaveArrayToFile(TArrayView<const uint8> Array, const TCHAR* Filename, IFileManager* FileManager = &IFileManager::Get(), uint32 WriteFlags = 0);
	static bool SaveArrayToFile(const TArray64<uint8>& Array, const TCHAR* Filename, IFileManager* FileManager = &IFileManager::Get(), uint32 WriteFlags = 0);
	static bool SaveStringToFile(FStringView String, const TCHAR* Filename, EEncodingOptions EncodingOptions = EEncodingOptions::AutoDetect, IFileManager* FileManager = &IFileManager::Get(), uint32 WriteFlags = 0);
	static bool SaveStringArrayToFile(const TArray<FString>& Lines, const TCHAR* Filename, EEncodingOptions EncodingOptions = EEncodingOptions::AutoDetect, IFileManager* FileManager = &IFileManager::Get(), uint32 WriteFlags = 0);
	static bool GenerateNextBitmapFilename(const FString& Pattern, const FString& Extension, FString& OutFilename, IFileManager* FileManager = &IFileManager::Get());
	static bool IsFilenameValidForSaving(const FString& Filename, FText& ErrorMessage);
};

// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/HAL/PlatformFileManager.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"

class IPlatformFile;

class FPlatformFileManager
{
public:
	FPlatformFileManager();
	IPlatformFile& GetPlatformFile();
	void SetPlatformFile(IPlatformFile& NewTopmostPlatformFile);
	IPlatformFile* FindPlatformFile(const TCHAR* Name);
	IPlatformFile* GetPlatformFile(const TCHAR* Name);
	void TickActivePlatformFile();
	void InitializeNewAsyncIO();
	static FPlatformFileManager& Get();
	void RemovePlatformFile(IPlatformFile* PlatformFileToRemove);
	bool InsertPlatformFile(IPlatformFile* NewPlatformFile);
};

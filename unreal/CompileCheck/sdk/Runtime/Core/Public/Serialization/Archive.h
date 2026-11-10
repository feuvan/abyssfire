// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Serialization/Archive.h (from UE 5.x source knowledge).
// Only the surface the module could plausibly touch (file reader / writer handles).
#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"

class FArchive
{
public:
	FArchive() = default;
	FArchive(const FArchive&) = default;
	FArchive& operator=(const FArchive& ArchiveToCopy) = default;
	virtual ~FArchive();
	virtual void Serialize(void* V, int64 Length);
	virtual int64 Tell();
	virtual int64 TotalSize();
	virtual bool AtEnd();
	virtual void Seek(int64 InPos);
	virtual void Flush();
	virtual bool Close();
	virtual FString GetArchiveName() const;
	bool IsLoading() const;
	bool IsSaving() const;
	bool IsError() const;
	void SetError();
	friend FArchive& operator<<(FArchive& Ar, FString& Value);
	friend FArchive& operator<<(FArchive& Ar, int32& Value);
	friend FArchive& operator<<(FArchive& Ar, uint32& Value);
	friend FArchive& operator<<(FArchive& Ar, int64& Value);
	friend FArchive& operator<<(FArchive& Ar, uint64& Value);
	friend FArchive& operator<<(FArchive& Ar, float& Value);
	friend FArchive& operator<<(FArchive& Ar, double& Value);
	friend FArchive& operator<<(FArchive& Ar, bool& D);
	friend FArchive& operator<<(FArchive& Ar, uint8& Value);
};

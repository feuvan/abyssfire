// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/Serialization/BulkData.h (UE 5.x source knowledge).
// FBulkData / FByteBulkData: the payload of a texture mip, a sound, ... Lock(LOCK_READ_WRITE) returns the bytes (loads
// them if needed) and MUST be paired with Unlock(). Only the lock / size API a game module touches.
#pragma once

#include "CoreMinimal.h"

/** Flags for FBulkData::Lock. */
enum EBulkDataLockFlags
{
	LOCK_READ_ONLY = 1,
	LOCK_READ_WRITE = 2,
};

class FBulkData
{
public:
	FBulkData();
	FBulkData(const FBulkData& Other);
	~FBulkData();
	FBulkData& operator=(const FBulkData& Other);
	/** Returns the size of the bulk data in bytes. */
	int64 GetBulkDataSize() const;
	/** Locks the bulk data and returns a pointer to it (LockFlags: EBulkDataLockFlags). */
	void* Lock(uint32 LockFlags);
	/** Unlocks bulk data after which point the pointer returned by Lock no longer is valid. */
	void Unlock() const;
	// LockReadOnly / Realloc / RemoveBulkData are not declared: the module does not call them (Realloc's parameters
	// differ between engine versions).
};

class FByteBulkData : public FBulkData
{
};

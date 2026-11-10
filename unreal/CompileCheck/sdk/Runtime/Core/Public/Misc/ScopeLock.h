// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/ScopeLock.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "HAL/CriticalSection.h"

class FScopeLock
{
public:
	[[nodiscard]] explicit FScopeLock(FCriticalSection* InSynchObject);
	~FScopeLock();
	void Unlock();
private:
	FScopeLock();
	FScopeLock(const FScopeLock& InScopeLock);
	FScopeLock& operator=(FScopeLock& InScopeLock);
};
class FScopeUnlock
{
public:
	[[nodiscard]] explicit FScopeUnlock(FCriticalSection* InSynchObject);
	~FScopeUnlock();
};

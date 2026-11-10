// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/HAL/CriticalSection.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"

class FCriticalSection
{
public:
	FCriticalSection();
	~FCriticalSection();
	FCriticalSection(const FCriticalSection&) = delete;
	FCriticalSection& operator=(const FCriticalSection&) = delete;
	void Lock();
	bool TryLock();
	void Unlock();
};
class FRWLock
{
public:
	FRWLock();
	~FRWLock();
	FRWLock(const FRWLock&) = delete;
	FRWLock& operator=(const FRWLock&) = delete;
	void ReadLock();
	void WriteLock();
	void ReadUnlock();
	void WriteUnlock();
};

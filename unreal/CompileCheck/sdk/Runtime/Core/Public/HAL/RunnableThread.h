// Stub SDK. Real headers: Engine/Source/Runtime/Core/Public/HAL/RunnableThread.h and GenericPlatform/GenericPlatformAffinity.h
// (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "HAL/PlatformAffinity.h"

class FRunnable;

class FRunnableThread
{
public:
	enum class ThreadType { Real, Fake, Forkable };
	static uint32 GetTlsSlot();
	static FRunnableThread* Create(class FRunnable* InRunnable, const TCHAR* ThreadName, uint32 InStackSize = 0,
		EThreadPriority InThreadPri = TPri_Normal, uint64 InThreadAffinityMask = FPlatformAffinity::GetNoAffinityMask(),
		EThreadCreateFlags InCreateFlags = EThreadCreateFlags::None);
	virtual void SetThreadPriority(EThreadPriority NewPriority) = 0;
	virtual bool SetThreadAffinity(const struct FThreadAffinity& Affinity);
	virtual void Suspend(bool bShouldPause = true) = 0;
	virtual bool Kill(bool bShouldWait = true) = 0;
	virtual void WaitForCompletion() = 0;
	virtual ThreadType GetThreadType() const;
	uint32 GetThreadID() const;
	const FString& GetThreadName() const;
	EThreadPriority GetThreadPriority() const;
	FRunnableThread();
	virtual ~FRunnableThread();
};

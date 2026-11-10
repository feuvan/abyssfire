// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/TimerManager.h (from UE 5.x source knowledge).
// FTimerManager: SetTimer overloads (member function, FTimerDelegate, TFunction), ClearTimer, IsTimerActive, ...
#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Delegates/Delegate.h"

class UObject;
DECLARE_DELEGATE(FTimerDelegate);

struct FTimerManagerTimerParameters
{
	bool bLoop = false;
	bool bMaxOncePerFrame = false;
	float FirstDelay = -1.f;
};

class FTimerManager
{
public:
	virtual void Tick(float DeltaTime);
	virtual ~FTimerManager();
	template <class UserClass>
	void SetTimer(FTimerHandle& InOutHandle, UserClass* InObj, typename FTimerDelegate::TMethodPtr<UserClass> InTimerMethod, float InRate, bool InbLoop = false, float InFirstDelay = -1.f) UE_STUB_BODY
	template <class UserClass>
	void SetTimer(FTimerHandle& InOutHandle, const UserClass* InObj, typename FTimerDelegate::TConstMethodPtr<UserClass> InTimerMethod, float InRate, bool InbLoop = false, float InFirstDelay = -1.f) UE_STUB_BODY
	void SetTimer(FTimerHandle& InOutHandle, FTimerDelegate const& InDelegate, float InRate, bool InbLoop, float InFirstDelay = -1.f);
	void SetTimer(FTimerHandle& InOutHandle, float InRate, bool InbLoop, float InFirstDelay = -1.f);
	void SetTimer(FTimerHandle& InOutHandle, TFunction<void(void)>&& Callback, float InRate, bool InbLoop, float InFirstDelay = -1.f);
	void SetTimer(FTimerHandle& InOutHandle, FTimerDelegate const& InDelegate, float InRate, const FTimerManagerTimerParameters& InTimerParameters);
	template <class UserClass>
	FTimerHandle SetTimerForNextTick(UserClass* inObj, typename FTimerDelegate::TMethodPtr<UserClass> inTimerMethod) UE_STUB_BODY
	FTimerHandle SetTimerForNextTick(FTimerDelegate const& InDelegate);
	FTimerHandle SetTimerForNextTick(TFunction<void(void)>&& Callback);
	void ClearTimer(FTimerHandle& InHandle);
	void ClearAllTimersForObject(void const* Object);
	void PauseTimer(FTimerHandle InHandle);
	void UnPauseTimer(FTimerHandle InHandle);
	float GetTimerRate(FTimerHandle InHandle) const;
	bool IsTimerActive(FTimerHandle InHandle) const;
	bool IsTimerPaused(FTimerHandle InHandle) const;
	bool IsTimerPending(FTimerHandle InHandle) const;
	bool TimerExists(FTimerHandle InHandle) const;
	float GetTimerElapsed(FTimerHandle InHandle) const;
	float GetTimerRemaining(FTimerHandle InHandle) const;
	bool HasBeenTickedThisFrame() const;
};

// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Containers/Ticker.h (from UE 5.x source knowledge).
// FTSTicker: thread-safe core ticker. The delegate returns true to keep ticking.
#pragma once

#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "Templates/SharedPointer.h"

DECLARE_DELEGATE_RetVal_OneParam(bool, FTickerDelegate, float);

class FTSTicker
{
	struct FElement;
public:
	using FDelegateHandle = TWeakPtr<FElement>;
	static FTSTicker& GetCoreTicker();
	FDelegateHandle AddTicker(const FTickerDelegate& InDelegate, float InDelay = 0.0f);
	FDelegateHandle AddTicker(const TCHAR* InName, float InDelay, TFunction<bool(float)> Function);
	static void RemoveTicker(FDelegateHandle Handle);
	void Tick(float DeltaTime);
	void Reset();
};
class FTSTickerObjectBase
{
public:
	FTSTickerObjectBase(float InDelay = 0.0f, FTSTicker& InTicker = FTSTicker::GetCoreTicker());
	virtual ~FTSTickerObjectBase();
	virtual bool Tick(float DeltaTime) = 0;
};

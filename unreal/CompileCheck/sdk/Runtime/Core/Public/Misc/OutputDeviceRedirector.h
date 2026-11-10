// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/OutputDeviceRedirector.h (from UE 5.x source knowledge).
// GLog is a macro for the global log redirector (CoreGlobals.h); add / remove devices here.
#pragma once

#include "CoreTypes.h"
#include "Misc/OutputDevice.h"

class FOutputDeviceRedirector final : public FOutputDevice
{
public:
	FOutputDeviceRedirector();
	static FOutputDeviceRedirector* Get();
	void AddOutputDevice(FOutputDevice* OutputDevice);
	void RemoveOutputDevice(FOutputDevice* OutputDevice);
	bool IsRedirectingTo(FOutputDevice* OutputDevice);
	void SetCurrentThreadAsPrimaryThread();
	void EnableBacklog(bool bEnable);
	void Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const FName& Category, const double Time) override;
	void Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const FName& Category) override;
	void Flush() override;
	void Panic();
	void TearDown() override;
};
FOutputDeviceRedirector* GetGlobalLogSingleton();

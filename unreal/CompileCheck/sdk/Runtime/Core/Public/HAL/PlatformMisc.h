// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/HAL/PlatformMisc.h (from UE 5.x source knowledge).
// The real FPlatformMisc is F<Platform>PlatformMisc : FGenericPlatformMisc; only the generic interface is declared here
// (platform-only members such as FAndroidMisc / FIOSPlatformMisc extras are not available through this SDK).
#pragma once

#include "GenericPlatform/GenericPlatformMisc.h"

struct FPlatformMisc : public FGenericPlatformMisc
{
};

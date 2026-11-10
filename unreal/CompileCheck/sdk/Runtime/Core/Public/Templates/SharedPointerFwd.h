// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Templates/SharedPointerFwd.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"

enum class ESPMode : uint8
{
	NotThreadSafe = 0,
	ThreadSafe = 1,
};
template <class ObjectType, ESPMode Mode = ESPMode::ThreadSafe> class TSharedRef;
template <class ObjectType, ESPMode Mode = ESPMode::ThreadSafe> class TSharedPtr;
template <class ObjectType, ESPMode Mode = ESPMode::ThreadSafe> class TWeakPtr;
template <class ObjectType, ESPMode Mode = ESPMode::ThreadSafe> class TSharedFromThis;

// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Containers/AllowShrinking.h (UE 5.4+; UE 5.x source knowledge).
// A scoped enum: a bool or an integer does not convert to it (the engine keeps deprecated bool overloads; not declared).
#pragma once

#include "CoreTypes.h"

// Specifies whether or not a container's removal operation should attempt to auto-shrink the container's reserved memory usage
enum class EAllowShrinking : uint8
{
	No,
	Yes,
	Default = Yes
};

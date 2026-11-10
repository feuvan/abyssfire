// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/MemStack.h (UE 5.x source knowledge). Only the names:
// the stub TArray does not model allocators beyond their type.
#pragma once

#include "CoreTypes.h"
#include "Containers/ContainerAllocationPolicies.h"

class FMemStack;
class FMemMark;

template <uint32 Alignment = DEFAULT_ALIGNMENT>
class TMemStackAllocator;

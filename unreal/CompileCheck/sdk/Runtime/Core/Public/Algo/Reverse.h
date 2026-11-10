// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Algo/Reverse.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Algo/Sort.h"

namespace Algo
{
	template <typename T, int32 ArraySize> void Reverse(T (&Array)[ArraySize]) UE_STUB_BODY
	template <typename T> void Reverse(T* Array, int32 ArraySize) UE_STUB_BODY
	template <typename ContainerType> requires UE::Stub::CContiguousRange<ContainerType>
	void Reverse(ContainerType&& Container) UE_STUB_BODY
}

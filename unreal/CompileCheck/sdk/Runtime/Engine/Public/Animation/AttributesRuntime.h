// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/Animation/AttributesRuntime.h (UE 5.x source knowledge).
// Only the attribute containers that travel with a pose.
#pragma once

#include "CoreMinimal.h"

namespace UE
{
	namespace Anim
	{
		struct FStackAttributeContainer
		{
			void Empty();
		};
		struct FHeapAttributeContainer
		{
			void Empty();
		};
	}
}

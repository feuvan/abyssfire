// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/SceneTypes.h (UE 5.x source knowledge). Only the material
// quality level enum is declared.
#pragma once

#include "CoreMinimal.h"

namespace EMaterialQualityLevel
{
	enum Type : uint8
	{
		Low,
		High,
		Medium,
		Epic,
		Num,
	};
}

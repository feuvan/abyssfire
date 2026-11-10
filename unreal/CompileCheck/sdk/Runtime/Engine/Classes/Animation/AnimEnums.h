// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Animation/AnimEnums.h (UE 5.x source knowledge; reflected
// enum: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

/** Root motion extraction modes of an anim instance. */
UENUM()
namespace ERootMotionMode
{
	enum Type : int
	{
		NoRootMotionExtraction,
		IgnoreRootMotion,
		RootMotionFromEverything,
		RootMotionFromMontagesOnly,
	};
}

UENUM()
enum EAdditiveBasePoseType : int
{
	ABPT_None,
	ABPT_RefPose,
	ABPT_AnimScaled,
	ABPT_AnimFrame,
	ABPT_LocalAnimFrame,
	ABPT_MAX,
};

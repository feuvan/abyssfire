// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Animation/AnimTypes.h (UE 5.x source knowledge; reflected
// enums: checked against the 5.6 stub). Only the additive enums are declared.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimEnums.h"

/** Types of additive animation. */
UENUM()
enum EAdditiveAnimationType : int
{
	/** No additive. */
	AAT_None,
	/** Local space additive. */
	AAT_LocalSpaceBase,
	/** Mesh space additive. */
	AAT_RotationOffsetMeshSpace,
	AAT_MAX,
};

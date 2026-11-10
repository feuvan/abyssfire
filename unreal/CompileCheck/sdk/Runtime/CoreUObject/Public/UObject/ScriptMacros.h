// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/ScriptMacros.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

struct FFrame;
#define RESULT_PARAM Z_Param__Result
#define RESULT_DECL void* const RESULT_PARAM
#define DECLARE_FUNCTION(func) static void func(UObject* Context, FFrame& Stack, RESULT_DECL)
#define DEFINE_FUNCTION(func) void func(UObject* Context, FFrame& Stack, RESULT_DECL)

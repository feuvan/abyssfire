// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/CoreMiscDefines.h (from UE 5.x source knowledge).
#pragma once

#include "HAL/Platform.h"

#define LOCALIZED_SEEKFREE_SUFFIX TEXT("_LOC")
#define PLAYWORLD_PACKAGE_PREFIX TEXT("UEDPIE")

#define PURE_VIRTUAL(func, ...) { LowLevelFatalError(TEXT("Pure virtual not implemented (%s)"), TEXT(#func)); __VA_ARGS__ }

enum { INDEX_NONE = -1 };
enum { UNICODE_BOM = 0xfeff };

enum EForceInit { ForceInit, ForceInitToZero };
enum ENoInit { NoInit };
enum EInPlace { InPlace };
enum EPerElement { PerElement };

#define UE_EMPTY
#define UE_INTERNAL
#define UE_EXPERIMENTAL(Version, Message)
#define UE_NONCOPYABLE(TypeName) \
	TypeName(TypeName&&) = delete; \
	TypeName(const TypeName&) = delete; \
	TypeName& operator=(const TypeName&) = delete; \
	TypeName& operator=(TypeName&&) = delete;

#define UE_BUILD_UNITY_EXCLUDED 0

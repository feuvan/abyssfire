// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/Build.h (from UE 5.x source knowledge).
// Build configuration switches. UBT passes UE_BUILD_<CONFIG>, WITH_EDITOR, UE_GAME / UE_EDITOR, ... on the command line
// (CompileCheck/run.sh does the same per configuration); this header only supplies the defaults, as the real one does.
#pragma once

#ifndef UE_BUILD_DEBUG
	#define UE_BUILD_DEBUG 0
#endif
#ifndef UE_BUILD_DEVELOPMENT
	#define UE_BUILD_DEVELOPMENT 0
#endif
#ifndef UE_BUILD_TEST
	#define UE_BUILD_TEST 0
#endif
#ifndef UE_BUILD_SHIPPING
	#define UE_BUILD_SHIPPING 0
#endif
#ifndef UE_GAME
	#define UE_GAME 0
#endif
#ifndef UE_EDITOR
	#define UE_EDITOR 0
#endif
#ifndef UE_SERVER
	#define UE_SERVER 0
#endif
#ifndef UE_BUILD_SHIPPING_WITH_EDITOR
	#define UE_BUILD_SHIPPING_WITH_EDITOR 0
#endif
#if !(UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT || UE_BUILD_TEST || UE_BUILD_SHIPPING)
	#error "Exactly one of UE_BUILD_DEBUG / UE_BUILD_DEVELOPMENT / UE_BUILD_TEST / UE_BUILD_SHIPPING must be 1 (run.sh sets it)"
#endif

#ifndef WITH_EDITOR
	#define WITH_EDITOR 0
#endif
#ifndef WITH_ENGINE
	#define WITH_ENGINE 1
#endif
#ifndef WITH_UNREAL_DEVELOPER_TOOLS
	#define WITH_UNREAL_DEVELOPER_TOOLS 0
#endif
#ifndef WITH_PLUGIN_SUPPORT
	#define WITH_PLUGIN_SUPPORT 0
#endif
#ifndef IS_MONOLITHIC
	#define IS_MONOLITHIC 0
#endif
#ifndef IS_PROGRAM
	#define IS_PROGRAM 0
#endif
#ifndef WITH_EDITORONLY_DATA
	#define WITH_EDITORONLY_DATA WITH_EDITOR
#endif
#ifndef WITH_SERVER_CODE
	#define WITH_SERVER_CODE 1
#endif

#if UE_BUILD_DEBUG
	#define DO_GUARD_SLOW 1
	#define DO_CHECK 1
	#define DO_ENSURE 1
	#define STATS 1
	#define ALLOW_DEBUG_FILES 1
	#define ALLOW_CONSOLE 1
	#define NO_LOGGING 0
#elif UE_BUILD_DEVELOPMENT
	#define DO_GUARD_SLOW 0
	#define DO_CHECK 1
	#define DO_ENSURE 1
	#define STATS 1
	#define ALLOW_DEBUG_FILES 1
	#define ALLOW_CONSOLE 1
	#define NO_LOGGING 0
#elif UE_BUILD_TEST
	#define DO_GUARD_SLOW 0
	#define DO_CHECK 0
	#define DO_ENSURE 0
	#define STATS 0
	#define ALLOW_DEBUG_FILES 1
	#define ALLOW_CONSOLE 1
	#define NO_LOGGING 1
#else
	#define DO_GUARD_SLOW 0
	#define DO_CHECK 0
	#define DO_ENSURE 0
	#define STATS 0
	#define ALLOW_DEBUG_FILES 0
	#define ALLOW_CONSOLE 0
	#define NO_LOGGING 1
#endif
#define DO_CHECK_SLOW DO_GUARD_SLOW
#ifndef USE_LOGGING_IN_SHIPPING
	#define USE_LOGGING_IN_SHIPPING 0
#endif
#ifndef USE_CHECKS_IN_SHIPPING
	#define USE_CHECKS_IN_SHIPPING 0
#endif
#define UE_ENABLE_ICU 1

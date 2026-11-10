// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/HAL/Platform.h (+ GenericPlatform/GenericPlatform.h,
// <Platform>/<Platform>Platform.h, Clang/ClangPlatform.h). From UE 5.x source knowledge.
// Fundamental types and compiler / platform macros. On every target of this check (Mac, iOS, Android) the platform types
// are `typedef char16_t WIDECHAR; typedef WIDECHAR TCHAR;` (PLATFORM_TCHAR_IS_CHAR16): WIDECHAR and TCHAR are ONE type,
// TEXT("x") and WIDETEXT("x") are both u"x", and wchar_t / L"x" is no engine character type at all. Windows (wchar_t)
// is not a checked configuration.
#pragma once

#include "Misc/Build.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

// ---- platform switches: UBT defines the active PLATFORM_<X>=1, the rest default to 0 (as the real Platform.h does) ----
#ifndef PLATFORM_WINDOWS
	#define PLATFORM_WINDOWS 0
#endif
#ifndef PLATFORM_MICROSOFT
	#define PLATFORM_MICROSOFT 0
#endif
#ifndef PLATFORM_MAC
	#define PLATFORM_MAC 0
#endif
#ifndef PLATFORM_IOS
	#define PLATFORM_IOS 0
#endif
#ifndef PLATFORM_TVOS
	#define PLATFORM_TVOS 0
#endif
#ifndef PLATFORM_VISIONOS
	#define PLATFORM_VISIONOS 0
#endif
#ifndef PLATFORM_APPLE
	#define PLATFORM_APPLE 0
#endif
#ifndef PLATFORM_ANDROID
	#define PLATFORM_ANDROID 0
#endif
#ifndef PLATFORM_LINUX
	#define PLATFORM_LINUX 0
#endif
#ifndef PLATFORM_LINUXARM64
	#define PLATFORM_LINUXARM64 0
#endif
#ifndef PLATFORM_UNIX
	#define PLATFORM_UNIX 0
#endif
#ifndef PLATFORM_SWITCH
	#define PLATFORM_SWITCH 0
#endif
#ifndef PLATFORM_FREEBSD
	#define PLATFORM_FREEBSD 0
#endif
#if (PLATFORM_MAC + PLATFORM_IOS + PLATFORM_ANDROID + PLATFORM_WINDOWS + PLATFORM_LINUX) != 1
	#error "Exactly one PLATFORM_<X> must be 1 (run.sh sets it per configuration)"
#endif
#ifndef PLATFORM_DESKTOP
	#define PLATFORM_DESKTOP (PLATFORM_MAC || PLATFORM_WINDOWS || PLATFORM_LINUX)
#endif
#define PLATFORM_64BITS 1
#define PLATFORM_LITTLE_ENDIAN 1
#define PLATFORM_TCHAR_IS_CHAR16 1
#define PLATFORM_TCHAR_IS_UTF8CHAR 0
#define PLATFORM_WCHAR_IS_4_BYTES 1
#define PLATFORM_HAS_TOUCH_MAIN_SCREEN (PLATFORM_IOS || PLATFORM_ANDROID)
#define PLATFORM_SUPPORTS_UNALIGNED_LOADS 1
#define PLATFORM_COMPILER_CLANG 1
#define PLATFORM_CPU_X86_FAMILY 0
#define PLATFORM_CPU_ARM_FAMILY 1
#define PLATFORM_ENABLE_VECTORINTRINSICS 0
#define PLATFORM_CACHE_LINE_SIZE 64
#define PLATFORM_USES_ANSI_STRING_FOR_EXTERNAL_PROFILING 1

// ---- fundamental types (FGenericPlatformTypes) ----
typedef unsigned char uint8;
typedef unsigned short int uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;
typedef signed char int8;
typedef signed short int int16;
typedef signed int int32;
typedef signed long long int64;
typedef char ANSICHAR;
typedef char16_t WIDECHAR;   // F{Mac,IOS,Android}PlatformTypes override the generic wchar_t
typedef char8_t UTF8CHAR;
// FGenericPlatformTypes::CHAR16 / CHAR32 are plain integers (uint16 / uint32), not char16_t / char32_t: a UCS2CHAR* is not
// a TCHAR*. The SDK declares no overload or trait for them (see TIsCharType), so an integer is never taken for a character.
typedef uint16 UCS2CHAR;
typedef uint16 UTF16CHAR;
typedef uint32 UTF32CHAR;
typedef WIDECHAR TCHAR;
typedef std::uintptr_t UPTRINT;
typedef std::intptr_t PTRINT;
typedef std::size_t SIZE_T;
typedef std::ptrdiff_t SSIZE_T;
typedef int32 TYPE_OF_NULL;
typedef decltype(nullptr) TYPE_OF_NULLPTR;

// ---- text literals ----
#define UTF8TEXT_PASTE(x) u8 ## x
#define UTF16TEXT_PASTE(x) u ## x
#if PLATFORM_TCHAR_IS_CHAR16
	#define WIDETEXT_PASTE(x) u ## x
#else
	#define WIDETEXT_PASTE(x) L ## x
#endif
#define TEXT_PASTE(x) WIDETEXT(x)
#if !defined(TEXT)
	#define TEXT(x) TEXT_PASTE(x)
#endif
#define UTF8TEXT(x) (UE::Core::Private::ToUTF8Literal(UTF8TEXT_PASTE(x)))
#define UTF16TEXT(x) UTF16TEXT_PASTE(x)
#define WIDETEXT(x) WIDETEXT_PASTE(x)
#define ANSITEXT(x) x
namespace UE::Core::Private
{
	template <SIZE_T N>
	constexpr const UTF8CHAR (&ToUTF8Literal(const char8_t (&Array)[N]))[N] { return Array; }
}

// ---- compiler ----
#define FORCEINLINE inline __attribute__((always_inline))
#define FORCENOINLINE __attribute__((noinline))
#define FORCEINLINE_DEBUGGABLE FORCEINLINE
#define VARARGS
#define CDECL
#define STDCALL
#define RESTRICT __restrict
#define ABSTRACT abstract
#define CONSTEXPR constexpr
#define UE_CONSTEVAL consteval
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define UE_LIKELY(x) LIKELY(x)
#define UE_UNLIKELY(x) UNLIKELY(x)
#define UE_NODISCARD [[nodiscard]]
#define UE_NODISCARD_CTOR [[nodiscard]]
#define UE_NORETURN [[noreturn]]
#define UE_LIFETIMEBOUND [[clang::lifetimebound]]
#define UE_NO_UNIQUE_ADDRESS [[no_unique_address]]
#define UE_ASSUME(x) __builtin_assume(x)
#define ASSUME(x) __builtin_assume(x)
#define DLLEXPORT __attribute__((visibility("default")))
#define DLLIMPORT __attribute__((visibility("default")))
#define MS_ALIGN(n)
#define GCC_ALIGN(n) __attribute__((aligned(n)))
#define GCC_PACK(n) __attribute__((packed, aligned(n)))
#define PLATFORM_BREAK() __builtin_trap()
#define UE_DEBUG_BREAK() PLATFORM_BREAK()
#define UE_DEPRECATED(Version, Message) [[deprecated(Message " - please update your code to the new API before upgrading to the next release, otherwise your project will no longer compile.")]]
#define UE_DEPRECATED_FORGAME UE_DEPRECATED
#define PRAGMA_DISABLE_DEPRECATION_WARNINGS _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
#define PRAGMA_ENABLE_DEPRECATION_WARNINGS _Pragma("clang diagnostic pop")
#define UE_PUSH_MACRO(name) _Pragma("push_macro(" name ")")
#define UE_POP_MACRO(name) _Pragma("pop_macro(" name ")")
#define PREPROCESSOR_TO_STRING(Token) PREPROCESSOR_TO_STRING_INNER(Token)
#define PREPROCESSOR_TO_STRING_INNER(Token) #Token
#define PREPROCESSOR_JOIN(TokenA, TokenB) PREPROCESSOR_JOIN_INNER(TokenA, TokenB)
#define PREPROCESSOR_JOIN_INNER(TokenA, TokenB) TokenA##TokenB
#define UE_JOIN(TokenA, TokenB) PREPROCESSOR_JOIN(TokenA, TokenB)
#define UE_STRINGIZE(Token) PREPROCESSOR_TO_STRING(Token)

// ---- module API macros of the engine modules in this SDK (UBT generates these per module; all empty here: no link) ----
#define CORE_API
#define COREUOBJECT_API
#define ENGINE_API
#define INPUTCORE_API
#define SLATECORE_API
#define SLATE_API
#define APPLICATIONCORE_API
#define RENDERCORE_API
#define RHI_API
#define DEVELOPERSETTINGS_API
#define AUDIOMIXER_API
#define ENHANCEDINPUT_API
#define PROCEDURALMESHCOMPONENT_API
#define JSON_API
#define UNREALED_API
#define PHYSICSCORE_API
#define AUDIOEXTENSIONS_API
#define ANIMATIONCORE_API

// Stub SDK convention: the SDK is declaration-only, but a function template whose signature can contain a closure type
// (a lambda passed as functor / predicate / task body) must have a body, because C++ forbids using an undefined function
// whose type has no linkage. Such declarations end with UE_STUB_BODY instead of ';'. The body never runs (nothing is
// linked); it only makes the declaration a definition. Works for any return type and for constructors.
namespace UE::Stub
{
	[[noreturn]] void DeclarationOnly() noexcept;
}
#define UE_STUB_BODY { ::UE::Stub::DeclarationOnly(); }


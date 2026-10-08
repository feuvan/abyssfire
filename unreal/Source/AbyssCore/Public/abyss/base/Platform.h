// AbyssCore - core-wide platform header. Include it FIRST in every core .cpp (ue58-platform.md 3.3).
//
// * Turns floating-point contraction (FMA fusion of a*b+c) off so results are identical on x64 (MSVC), arm64
//   (Apple clang, NDK clang) and Linux. CMake additionally passes -ffp-contract=off (GCC ignores the STDC pragma).
//   The pragmas are NOT enough on their own: -ffp-contract=fast / -ffast-math / MSVC /fp:fast override them (clang
//   ignores both contract pragmas under -ffp-contract=fast and folds std::isnan to false under -ffast-math). The build
//   must therefore select precise FP semantics: AbyssCore.Build.cs and Abyssfire.Build.cs set
//   `FPSemantics = FPSemanticsMode.Precise` (the public headers carry inline math that is also compiled in the game
//   module), CMake passes -ffp-contract=off / /fp:precise, and the tripwire below stops a core TU built with fast-math.
// * ABYSS_API exports the core's symbols when AbyssCore is its own shared library (UE modular builds: editor, Windows
//   DLL builds; see the block below). Every non-template class with out-of-line members and every free function
//   declared in Public/ carries it.
// * Never include UE headers from the core (only Private/UE/AbyssCoreModule.cpp does).
// * No non-ASCII characters anywhere in core sources; all player text comes from Data/*.json.
#pragma once

#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#pragma clang fp contract(off)
#elif defined(_MSC_VER)
#pragma fp_contract(off)
#pragma float_control(precise, on)
#endif

// FP tripwire (core translation units only: ABYSS_CORE_BUILDING is a private definition of the core module / CMake
// target). A fast-math build changes JsHypot (MapGen goldens, W10), contracts a*b+c and folds NaN checks.
#if defined(ABYSS_CORE_BUILDING) && (defined(__FAST_MATH__) || defined(_M_FP_FAST))
#error "AbyssCore must not be built with fast-math (-ffast-math, -ffp-model=fast, /fp:fast): use precise FP semantics"
#endif

// ---- symbol export (ABYSS_API) ---------------------------------------------------------------------------------------
// ABYSS_CORE_DLL: 1 when AbyssCore is linked as its own shared library (UE TargetLinkType.Modular; set by
// AbyssCore.Build.cs as a public definition, so the game module sees the same value). ABYSS_CORE_BUILDING: 1 only while
// compiling the core itself (private definition). Monolithic builds (shipping, mobile, CMake tests) define neither or
// ABYSS_CORE_DLL=0 and ABYSS_API expands to nothing. UBT's own ABYSSCORE_API cannot be used: it expands to DLLEXPORT,
// which only UE headers define, and core TUs never include those.
#if defined(ABYSS_CORE_DLL) && ABYSS_CORE_DLL
#if defined(_WIN32)
#if defined(ABYSS_CORE_BUILDING) && ABYSS_CORE_BUILDING
#define ABYSS_API __declspec(dllexport)
#else
#define ABYSS_API __declspec(dllimport)
#endif
#else
#define ABYSS_API __attribute__((visibility("default")))
#endif
#if defined(_MSC_VER)
// C4251/C4275: std:: members / bases of exported classes have no dll-interface. Harmless here: the core and the game
// module are built by the same toolchain with the same STL, and per-module operator new/delete forward to FMemory.
#pragma warning(disable : 4251 4275)
#endif
#else
#define ABYSS_API
#endif

#include <cstddef>
#include <cstdint>

namespace abyss {

// Version of the core library API (bumped when public headers change incompatibly).
inline constexpr int kCoreApiVersion = 2;

}  // namespace abyss

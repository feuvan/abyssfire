// AbyssCore: the portable C++20 gameplay core (ue58-platform.md 4.4, ARCHITECTURE 1).
// Every rule and all game state live here. No UE header is included anywhere except Private/UE/AbyssCoreModule.cpp.
// The same sources build with CMake in unreal/CoreTests (GCC + Clang, -Werror) for the unit tests.
using System.IO;
using UnrealBuildTool;

public class AbyssCore : ModuleRules
{
    public AbyssCore(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.CPlusPlus;
        PCHUsage = PCHUsageMode.NoPCHs;           // core never includes CoreMinimal.h
        CppStandard = CppStandardVersion.Cpp20;
        bEnableExceptions = false;
        bUseRTTI = false;
        // Keep UE macros (check, PI, ...) from AbyssCoreModule.cpp out of the core translation units.
        bUseUnity = false;

        // Deterministic floating point (ue58-platform.md 3.3, DECISIONS W10 / S3): precise semantics, no FMA contraction,
        // no fast-math. The Platform.h pragmas alone do not survive -ffp-contract=fast / -ffast-math / MSVC /fp:fast, and
        // Platform.h #errors when a core TU is compiled with fast-math. Abyssfire.Build.cs must set the same mode because
        // the public headers' inline math (Lerp, DistSq, Vec2::Length, IsoPx, SpatialGrid, ...) is compiled there too.
        // [Verify] the 5.8 name: ModuleRules.FPSemantics / FPSemanticsMode.Precise (UE 5.x; maps to /fp:precise and
        // -ffp-contract=off).
        FPSemantics = FPSemanticsMode.Precise;

        // Symbol export (Public/abyss/base/Platform.h ABYSS_API): in modular builds (editor, Windows DLL builds) AbyssCore
        // is its own DLL/dylib and the Abyssfire module links against it; monolithic builds (shipping, mobile) need
        // nothing. ABYSS_CORE_DLL is public so both modules agree; ABYSS_CORE_BUILDING marks the core's own TUs.
        PublicDefinitions.Add(Target.LinkType == TargetLinkType.Modular ? "ABYSS_CORE_DLL=1" : "ABYSS_CORE_DLL=0");
        PrivateDefinitions.Add("ABYSS_CORE_BUILDING=1");

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
        // RapidJSON is vendored under ThirdParty/rapidjson and included only by Private/base/Json.cpp through
        // Private/base/RapidJsonConfig.h (namespace abyss_rapidjson, no exceptions) - DECISIONS U3.
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty", "rapidjson"));
        // Only Private/UE/AbyssCoreModule.cpp uses this (IMPLEMENT_MODULE). The CMake build has no UE headers,
        // so an accidental UE include anywhere else in the core fails CI.
        PrivateDependencyModuleNames.Add("Core");
    }
}

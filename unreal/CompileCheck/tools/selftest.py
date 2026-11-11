#!/usr/bin/env python3
"""Proves the compile check bites: plants known faults in a scratch copy of Source/Abyssfire and requires each one to be
reported at the planted place. Run through  ./run.sh --selftest  (uses <build>/selftest; mac-editor, except the faults
that sit in a platform / target branch: those run in every configuration and must fail in exactly the right ones).

A fault that is NOT reported means the stub SDK or the fake UHT has become too permissive: fix that before trusting a
PASS of the real module.
"""
import json
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CHECK_DIR = os.path.dirname(HERE)
MODULE_DIR = os.path.join(os.path.dirname(CHECK_DIR), 'Source', 'Abyssfire')
sys.dont_write_bytecode = True  # no __pycache__ next to the sources
sys.path.insert(0, HERE)
from stubpath import find_stub  # noqa: E402

# name, file, text to find (must occur exactly once), replacement, expected diagnostic (file suffix, message regex), run
FAULTS = [
    {
        'name': 'wrong engine argument type',
        'what': 'FSceneView::ProjectWorldToScreen(const FVector&, const FIntRect&, const FMatrix&, FVector2D&) called with the two vectors swapped',
        'file': 'Private/Input/AbyssPicking.cpp',
        'find': 'FSceneView::ProjectWorldToScreen(Corner, Projection.ViewRect, Projection.ViewProjection, Projected)',
        'replace': 'FSceneView::ProjectWorldToScreen(Projected, Projection.ViewRect, Projection.ViewProjection, Corner)',
        'expect_file': 'Private/Input/AbyssPicking.cpp',
        'expect': r'no viable conversion|cannot bind|no matching (member )?function|cannot initialize a parameter',
        'run': 'A',
    },
    {
        'name': 'missing include',
        'what': '#include "Animation/AnimSequence.h" removed: UAnimSequence is only forward-declared where it is used',
        'file': 'Private/Anim/AbyssAnimInstance.cpp',
        'find': '#include "Animation/AnimSequence.h"\n',
        'replace': '',
        'expect_file': 'Private/Anim/AbyssAnimInstance.cpp',
        'expect': r"incomplete type '(const )?UAnimSequence'",
        'run': 'A',
    },
    {
        'name': '.h / .cpp signature mismatch',
        'what': 'AAbyssGameMode::PlayerCanRestart_Implementation defined with AController* where the header declares APlayerController*',
        'file': 'Private/Framework/AbyssGameMode.cpp',
        'find': 'bool AAbyssGameMode::PlayerCanRestart_Implementation(APlayerController* ',
        'replace': 'bool AAbyssGameMode::PlayerCanRestart_Implementation(AController* ',
        'expect_file': 'Private/Framework/AbyssGameMode.cpp',
        'expect': r"out-of-line definition of 'PlayerCanRestart_Implementation' does not match any declaration",
        'run': 'A',
    },
    {
        'name': 'module missing from Build.cs',
        'what': '"EnhancedInput" removed from the dependency list: its headers must no longer be found',
        'file': 'Abyssfire.Build.cs',
        'find': '"EnhancedInput",',
        'replace': '',
        'expect_file': '',
        'expect': r"'(EnhancedInputComponent|EnhancedInputSubsystems|InputAction|InputActionValue|InputMappingContext|InputModifiers)\.h' file not found",
        'run': 'B',
    },
    {
        'name': 'UHT rule: generated header not last',
        'what': 'an include placed after AbyssCameraRig.generated.h',
        'file': 'Public/Camera/AbyssCameraRig.h',
        'find': '#include "AbyssCameraRig.generated.h"\n',
        'replace': '#include "AbyssCameraRig.generated.h"\n#include "Math/Color.h"\n',
        'expect_file': 'Public/Camera/AbyssCameraRig.h',
        'expect': r'generated-include',
        'run': 'B',
        'uht': True,
    },
    {
        'name': 'UHT rule: UPROPERTY inside a platform #if',
        'what': 'a reflected property wrapped in #if PLATFORM_IOS (UHT does not evaluate it: only WITH_EDITOR / WITH_EDITORONLY_DATA)',
        'file': 'Public/Anim/AbyssAnimInstance.h',
        'find': '\tUPROPERTY(Transient)\n\tTObjectPtr<UAnimSequence> AdditiveSeq;\n',
        'replace': '#if PLATFORM_IOS\n\tUPROPERTY(Transient)\n\tTObjectPtr<UAnimSequence> AdditiveSeq;\n#endif\n',
        'expect_file': 'Public/Anim/AbyssAnimInstance.h',
        'expect': r'preprocessor',
        'run': 'B',
        'uht': True,
    },
    {
        'name': 'UHT rule: bool bitfield UPROPERTY',
        'what': 'a reflected "bool bX : 1" (UHT: bool bitfields are not supported)',
        'file': 'Public/Input/AbyssInputConfig.h',
        'find': '\tUPROPERTY(Transient)\n\tTObjectPtr<UInputMappingContext> GlobalContext;\n',
        'replace': '\tUPROPERTY(Transient)\n\tTObjectPtr<UInputMappingContext> GlobalContext;\n\tUPROPERTY(Transient)\n\tbool bPlantedFlag : 1;\n',
        'expect_file': 'Public/Input/AbyssInputConfig.h',
        'expect': r'property-type.*bool bitfield',
        'run': 'B',
        'uht': True,
    },
    {
        'name': 'UHT rule: virtual BlueprintImplementableEvent',
        'what': 'a BlueprintImplementableEvent declared virtual (UHT: should not be virtual, use BlueprintNativeEvent)',
        'file': 'Public/Input/AbyssInputConfig.h',
        'find': '\tUPROPERTY(Transient)\n\tTObjectPtr<UInputMappingContext> MenuPadContext;\n',
        'replace': '\tUPROPERTY(Transient)\n\tTObjectPtr<UInputMappingContext> MenuPadContext;\n\tUFUNCTION(BlueprintImplementableEvent)\n\tvirtual void PlantedEvent();\n',
        'expect_file': 'Public/Input/AbyssInputConfig.h',
        'expect': r'specifier.*must not be virtual',
        'run': 'B',
        'uht': True,
    },
    {
        'name': 'unity build: the same file-scope helper in two files',
        'what': 'two .cpp files that each compile alone define the same function in an anonymous namespace: a redefinition once UBT '
                'puts them in one unity blob',
        'file': 'Private/World/AbyssWorldTypes.cpp',
        'append': '\nnamespace { int32 AbyssPlantedUnityHelper() { return 1; } }\n',
        'expect_file': '',
        'expect': r"redefinition of 'AbyssPlantedUnityHelper'",
        'run': 'C',
    },
    {
        'name': 'unity build: the second file of the pair',
        'what': 'the other half of the planted pair',
        'file': 'Private/Vfx/AbyssVfxDefaults.cpp',
        'append': '\nnamespace { int32 AbyssPlantedUnityHelper() { return 2; } }\n',
        'expect_file': '',
        'expect': r"redefinition of 'AbyssPlantedUnityHelper'",
        'run': 'C',
    },
    {
        'name': 'a parameter that shadows a member (-Wshadow)',
        'what': 'UAbyssAudioSystem::PlayStinger with a parameter named like the member DuckInSec: -Wshadow, an error with '
                'BuildSettingsVersion.Latest (the bug CC3 found in this function)',
        'file': 'Private/Audio/AbyssAudioSystem.cpp',
        'find': 'void UAbyssAudioSystem::PlayStinger(FName Asset, double DuckDb, double DuckFadeInSec, double DuckFadeOutSec)\n{\n',
        'replace': 'void UAbyssAudioSystem::PlayStinger(FName Asset, double DuckDb, double DuckInSec, double DuckFadeOutSec)\n{\n'
                   '\tconst double DuckFadeInSec = DuckInSec;\n',
        'expect_file': 'Private/Audio/AbyssAudioSystem.cpp',
        'expect': r"declaration shadows a field of 'UAbyssAudioSystem'",
        'run': 'D',
    },
    {
        'name': 'wrong audio component argument',
        'what': 'UAudioComponent::FadeOut(Duration, Level, Curve) called without the level: the curve enum is not a float',
        'file': 'Private/Audio/AbyssAudioSystem.cpp',
        'find': 'AmbienceComponent->FadeOut(FadeOutSec, 0.f, EAudioFaderCurve::Linear);',
        'replace': 'AmbienceComponent->FadeOut(FadeOutSec, EAudioFaderCurve::Linear);',
        'expect_file': 'Private/Audio/AbyssAudioSystem.cpp',
        'expect': r"cannot initialize a parameter of type 'float' with an rvalue of type 'EAudioFaderCurve'",
        'run': 'D',
    },
    {
        'name': 'UI: a helper used without its header (non-unity)',
        'what': '#include "UI/Core/AbyssUiDraw.h" removed from SAbyssHud.cpp: AbyssEase is then only visible when a unity blob '
                'happens to include the header first (the missing include CC4 found in this file)',
        'file': 'Private/UI/Hud/SAbyssHud.cpp',
        'find': '#include "UI/Core/AbyssUiDraw.h"\n',
        'replace': '',
        'expect_file': 'Private/UI/Hud/SAbyssHud.cpp',
        'expect': r"use of undeclared identifier 'AbyssEase'",
        'run': 'E',
    },
    {
        'name': 'UI: Slate style argument by value',
        'what': 'SScrollBox .ScrollBarStyle(...) given the style by reference instead of its address (SLATE_STYLE_ARGUMENT is a pointer)',
        'file': 'Private/UI/Widgets/AbyssUiWidgets.cpp',
        'find': '.ScrollBarStyle(&Ctx.Style().ScrollBar())',
        'replace': '.ScrollBarStyle(Ctx.Style().ScrollBar())',
        'expect_file': 'Private/UI/Widgets/AbyssUiWidgets.cpp',
        'expect': r"no matching member function for call to 'ScrollBarStyle'",
        'run': 'E',
    },
    {
        'name': 'UI: OnPaint override with a wrong signature',
        'what': 'SAbyssMinimap::OnPaint declared with `uint32 LayerId` (the engine: int32): `override` no longer overrides',
        'file': 'Private/UI/Hud/SAbyssMinimap.h',
        'find': 'FSlateWindowElementList& OutDrawElements, int32 LayerId,',
        'replace': 'FSlateWindowElementList& OutDrawElements, uint32 LayerId,',
        'expect_file': '',
        'expect': r"'OnPaint' marked 'override' but does not override|only virtual member functions can be marked 'override'|out-of-line definition of 'OnPaint' does not match",
        'run': 'E',
    },
    {
        'name': 'UI: texture mip data without TextureResource.h',
        'what': '#include "TextureResource.h" removed from AbyssMinimapTexture.cpp: FTexturePlatformData / FTexture2DMipMap are '
                'only forward-declared by Engine/Texture2D.h',
        'file': 'Private/UI/Hud/AbyssMinimapTexture.cpp',
        'find': '#include "TextureResource.h"\n',
        'replace': '',
        'expect_file': 'Private/UI/Hud/AbyssMinimapTexture.cpp',
        'expect': r"incomplete type '(const )?FTexturePlatformData'|unknown type name 'FTexture2DMipMap'|incomplete type 'FTexture2DMipMap'",
        'run': 'E',
    },
    {
        'name': 'Panels: a widget used through a forward declaration only',
        'what': '#include "Widgets/Layout/SScrollBox.h" removed from SAbyssAchievementsPanel.cpp: the panel header and '
                'AbyssUiWidgets.h only forward-declare SScrollBox',
        'file': 'Private/UI/Panels/SAbyssAchievementsPanel.cpp',
        'find': '#include "Widgets/Layout/SScrollBox.h"\n',
        'replace': '',
        'expect_file': 'Private/UI/Panels/SAbyssAchievementsPanel.cpp',
        'expect': r"incomplete type '(const )?SScrollBox'|incomplete type named in nested name specifier|unknown type name 'SScrollBox'|use of undeclared identifier 'SScrollBox'",
        'run': 'F',
    },
    {
        'name': 'Menu: the stretch direction given to the stretch rule',
        'what': 'SScaleBox .Stretch(...) called with EStretchDirection::DownOnly (two different enums)',
        'file': 'Private/UI/Menu/SAbyssMainMenu.cpp',
        'find': '.Stretch(EStretch::ScaleToFit)',
        'replace': '.Stretch(EStretchDirection::DownOnly)',
        'expect_file': 'Private/UI/Menu/SAbyssMainMenu.cpp',
        'expect': r"no matching member function for call to 'Stretch'",
        'run': 'F',
    },
    {
        'name': 'Menu: a date formatted with an FString',
        'what': 'FDateTime::ToString(const TCHAR*) called with an FString (no implicit conversion: *Format)',
        'file': 'Private/UI/Menu/SAbyssMainMenu.cpp',
        'find': 'return Local.ToString(TEXT("%Y-%m-%d %H:%M"));',
        'replace': 'return Local.ToString(FString(TEXT("%Y-%m-%d %H:%M")));',
        'expect_file': 'Private/UI/Menu/SAbyssMainMenu.cpp',
        'expect': r"no matching member function for call to 'ToString'|no viable conversion from 'FString' to 'const TCHAR \*'",
        'run': 'F',
    },
    {
        'name': 'Story: a pointer handler declared with the wrong event type',
        'what': 'SAbyssStoryOverlay::OnMouseButtonDoubleClick declared with FKeyEvent (the engine: FPointerEvent): `override` no longer overrides',
        'file': 'Private/UI/Story/SAbyssStoryOverlay.h',
        'find': 'virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;',
        'replace': 'virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FKeyEvent& InMouseEvent) override;',
        'expect_file': '',
        'expect': r"'OnMouseButtonDoubleClick' marked 'override' but does not override|only virtual member functions can be marked 'override'|out-of-line definition of 'OnMouseButtonDoubleClick' does not match",
        'run': 'F',
    },
    {
        'name': "width lint: an int64 under '%d' (opt-in, UE_STUB_STRICT_FORMAT_WIDTH)",
        'what': "FMath::RoundToInt(double) (an int64 since UE 5.0) passed to '%d' in FString::Printf: accepted by the engine's "
                'format check, a width mismatch for C varargs (the slip CC5 found three times); the lint run must list it',
        'file': 'Private/UI/Panels/SAbyssSkillTreePanel.cpp',
        'find': 'FMath::RoundToInt32(abyss::SkillDamageMultiplier(Rules, Skill, At) * 100.0),',
        'replace': 'FMath::RoundToInt(abyss::SkillDamageMultiplier(Rules, Skill, At) * 100.0),',
        'expect_file': 'Private/UI/Panels/SAbyssSkillTreePanel.cpp',
        'expect': r"TCheckedFormatStringPrivate<char16_t, long long, int, .*is not a constant expression",
        'run': 'G',
    },
    # ---- the final audit: fake-UHT rules, configuration-specific branches, the Build.cs lint, the link scan, --msvc-lint ----
    {
        'name': 'UHT rule: a UPROPERTY named like a property of an engine base class',
        'what': 'AAbyssCameraRig gets a UPROPERTY "RootComponent" (AActor has one): UHT rejects the shadowing, C++ accepts the member',
        'file': 'Public/Camera/AbyssCameraRig.h',
        'find': '\tUPROPERTY(VisibleAnywhere, Category = "Abyss")\n\tTObjectPtr<USceneComponent> SceneRoot;\n',
        'replace': '\tUPROPERTY(VisibleAnywhere, Category = "Abyss")\n\tTObjectPtr<USceneComponent> SceneRoot;\n\tUPROPERTY(Transient)\n\tTObjectPtr<USceneComponent> RootComponent;\n',
        'expect_file': 'Public/Camera/AbyssCameraRig.h',
        'expect': r"property-shadow.*'RootComponent' cannot be defined in 'AAbyssCameraRig'",
        'run': 'B',
        'uht': True,
        'needs_stub': True,   # the rule reads the engine classes from the Python stub
    },
    {
        'name': 'UHT rule: a UObject class by value as a UPROPERTY',
        'what': 'a reflected member of type UInputMappingContext (UHT: "Missing \'*\' in Expected a pointer type")',
        'file': 'Public/Input/AbyssInputConfig.h',
        'find': '\tUPROPERTY(Transient)\n\tTObjectPtr<UInputMappingContext> StoryContext;\n',
        'replace': '\tUPROPERTY(Transient)\n\tTObjectPtr<UInputMappingContext> StoryContext;\n\tUPROPERTY(Transient)\n\tUInputMappingContext PlantedByValue;\n',
        'expect_file': 'Public/Input/AbyssInputConfig.h',
        'expect': r'property-type.*UObject class used by value',
        'run': 'B',
        'uht': True,
        'needs_stub': True,   # the rule reads the engine classes from the Python stub
    },
    {
        'name': 'configurations: a fault inside #if PLATFORM_ANDROID || PLATFORM_IOS',
        'what': 'an undeclared name in the mobile branch of AbyssPlatform::IsMobilePlatform: android-game and ios-shipping fail, the Mac ones pass',
        'file': 'Private/Platform/AbyssPlatform.cpp',
        'find': '#if PLATFORM_ANDROID || PLATFORM_IOS\n\t\treturn true;\n',
        'replace': '#if PLATFORM_ANDROID || PLATFORM_IOS\n\t\treturn AbyssPlantedMobileOnly;\n',
        'expect_file': 'Private/Platform/AbyssPlatform.cpp',
        'expect': r"use of undeclared identifier 'AbyssPlantedMobileOnly'",
        'configs': ['android-game', 'ios-shipping'],
        'run': 'M',
    },
    {
        'name': 'configurations: a fault inside #if UE_BUILD_SHIPPING',
        'what': 'an undeclared name in the shipping branch of the sim configuration: only ios-shipping fails',
        'file': 'Private/Framework/AbyssGameInstance.cpp',
        'find': '#if UE_BUILD_SHIPPING\n\tConfig.enableDebugCommands = false;\n',
        'replace': '#if UE_BUILD_SHIPPING\n\tConfig.enableDebugCommands = AbyssPlantedShippingOnly;\n',
        'expect_file': 'Private/Framework/AbyssGameInstance.cpp',
        'expect': r"use of undeclared identifier 'AbyssPlantedShippingOnly'",
        'configs': ['ios-shipping'],
        'run': 'M',
    },
    {
        'name': 'configurations: a fault inside #if WITH_EDITOR',
        'what': 'an undeclared name in the editor-only body of SetMaterialInstanceScalarParameter: only mac-editor fails',
        'file': 'Private/ContentBuild/AbyssContentBuildLibrary.cpp',
        'find': 'Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(ParameterName), Value);',
        'replace': 'Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(ParameterName), AbyssPlantedEditorOnly);',
        'expect_file': 'Private/ContentBuild/AbyssContentBuildLibrary.cpp',
        'expect': r"use of undeclared identifier 'AbyssPlantedEditorOnly'",
        'configs': ['mac-editor'],
        'run': 'M',
    },
    {
        'name': 'configurations: a name that only the editor build declares',
        'what': 'a local declared inside #if WITH_EDITOR and used outside it: the three game configurations fail, mac-editor passes',
        'file': 'Private/Framework/AbyssGameInstance.cpp',
        'find': '\tConfig.milestone1 = true;   // U6 / Q3\n',
        'replace': '#if WITH_EDITOR\n\tconst bool bAbyssPlantedEditorFlag = true;\n#endif\n\tConfig.milestone1 = bAbyssPlantedEditorFlag;\n',
        'expect_file': 'Private/Framework/AbyssGameInstance.cpp',
        'expect': r"use of undeclared identifier 'bAbyssPlantedEditorFlag'",
        'configs': ['mac-game', 'android-game', 'ios-shipping'],
        'run': 'M',
    },
    {
        'name': 'Build.cs lint: a module only reachable through another module',
        'what': '"Slate" removed from Abyssfire.Build.cs: its headers are still found (Engine re-exports Slate), every file compiles, and the '
                'modular editor build would fail to link',
        'file': 'Abyssfire.Build.cs',
        'find': '"Slate", "SlateCore",',
        'replace': '"SlateCore",',
        'expect_file': '',
        'expect': r'build-cs.*belongs to the engine module Slate, which Abyssfire.Build.cs does not list',
        'run': 'L',
        'build_cs': True,
    },
    {
        'name': 'link scan: one helper name in two anonymous namespaces (different parameters)',
        'what': 'AbyssPlantedBlend(float) in one .cpp and AbyssPlantedBlend(double) in another: both files and the unity unit compile; in a '
                'unity blob the two silently become one overload set',
        'file': 'Private/World/AbyssWorldTypes.cpp',
        'append': '\nnamespace { float AbyssPlantedBlend(float A) { return A; } }\n',
        'expect_file': '',
        'expect': r"unity-collision.*'\(anonymous\)::AbyssPlantedBlend' has internal linkage and is also defined in",
        'run': 'L',
        'scan': True,
    },
    {
        'name': 'link scan: the second file of the pair',
        'what': 'the other half of the planted pair',
        'file': 'Private/Vfx/AbyssVfxDefaults.cpp',
        'append': '\nnamespace { double AbyssPlantedBlend(double A) { return A; } }\n',
        'expect_file': '',
        'expect': r"unity-collision.*'\(anonymous\)::AbyssPlantedBlend'",
        'run': 'L',
        'scan': True,
    },
    {
        'name': 'link scan: one helper name in a named private namespace of two files (different parameters)',
        'what': 'AbyssPlantedPrivate::Pick(int32) in one .cpp and AbyssPlantedPrivate::Pick(float) in another, declared in no header: '
                'external linkage, so no redefinition anywhere; one overload set in a unity blob',
        'file': 'Private/World/AbyssTerrain.cpp',
        'append': '\nnamespace AbyssPlantedPrivate { int32 Pick(int32 A) { return A; } }\n',
        'expect_file': '',
        'expect': r"unity-collision.*'AbyssPlantedPrivate::Pick' is declared in no header and is defined with other parameters in",
        'run': 'L',
        'scan': True,
    },
    {
        'name': 'link scan: the second file of that pair',
        'what': 'the other half of the planted pair',
        'file': 'Private/World/AbyssSetPieces.cpp',
        'append': '\nnamespace AbyssPlantedPrivate { int32 Pick(float A) { return static_cast<int32>(A); } }\n',
        'expect_file': '',
        'expect': r"unity-collision.*'AbyssPlantedPrivate::Pick'",
        'run': 'L',
        'scan': True,
    },
    {
        'name': 'link scan: declared, used, never defined',
        'what': 'a function declared and called in a .cpp, with no definition anywhere: every file compiles, the link fails',
        'file': 'Private/World/AbyssZoneActor.cpp',
        'append': '\nint32 AbyssPlantedMissing(int32 Value);\nint32 AbyssPlantedCaller() { return AbyssPlantedMissing(1); }\n',
        'expect_file': 'Private/World/AbyssZoneActor.cpp',
        'expect': r"undefined.*function 'AbyssPlantedMissing' is declared here, used, and defined in no translation unit",
        'run': 'L',
        'scan': True,
    },
    {
        'name': 'link scan: a non-inline definition in a header',
        'what': 'a function with a body and without `inline` in Public/World/AbyssWorldTypes.h: a duplicate symbol in every pair of files that include it',
        'file': 'Public/World/AbyssWorldTypes.h',
        'append': '\nABYSSFIRE_API int32 AbyssPlantedInHeader() { return 1; }\n',
        'expect_file': 'Public/World/AbyssWorldTypes.h',
        'expect': r"header-definition.*function 'AbyssPlantedInHeader' is defined in a header without `inline`",
        'run': 'L',
        'scan': True,
    },
    {
        'name': 'link scan: an Apple type name in an anonymous namespace',
        'what': 'namespace { struct Rect {...}; } compiles here; on macOS / iOS MacTypes.h has a global Rect and the name is ambiguous',
        'file': 'Private/Vfx/AbyssVfxRecipe.cpp',
        'append': '\nnamespace { struct Rect { int32 Left = 0; }; }\n',
        'expect_file': 'Private/Vfx/AbyssVfxRecipe.cpp',
        'expect': r"platform-name.*struct 'Rect' at global scope",
        'run': 'L',
        'scan': True,
    },
    {
        'name': 'link scan: an identifier that is a platform macro',
        'what': 'a local variable named `nil` (an Objective-C macro on every Apple translation unit)',
        'file': 'Private/Vfx/AbyssVfxSystem.cpp',
        'append': '\nnamespace { int32 AbyssPlantedNil() { const int32 nil = 0; return nil; } }\n',
        'expect_file': 'Private/Vfx/AbyssVfxSystem.cpp',
        'expect': r"platform-name.*identifier 'nil' is a macro",
        'run': 'L',
        'scan': True,
    },
    {
        'name': 'link scan: std::uint64_t and size_t in one std::max',
        'what': 'std::max(std::uint64_t, size_t): one type on Linux / Android (unsigned long), two on macOS / iOS (unsigned long long, unsigned long)',
        'file': 'Private/World/AbyssAssetLibrary.cpp',
        'append': '\n#include <algorithm>\n#include <cstdint>\nnamespace { std::uint64_t AbyssPlantedWidest(std::uint64_t A, size_t B) { return std::max(A, B); } }\n',
        'expect_file': 'Private/World/AbyssAssetLibrary.cpp',
        'expect': r"lp64-mix.*'max' mixes",
        'run': 'L',
        'scan': True,
    },
    {
        'name': 'link scan: an AbyssCore symbol the module uses without ABYSS_API',
        'what': 'ABYSS_API removed from abyss::JsonValue in a scratch copy of AbyssCore/Public: every file compiles; the modular editor build '
                'cannot link the module against the AbyssCore library',
        'core_file': 'abyss/base/Json.h',
        'find': 'class ABYSS_API JsonValue {',
        'replace': 'class JsonValue {',
        'expect_file': '',
        'expect': r"core-export.*AbyssCore member function 'abyss::JsonValue::\w+' .* is used here but not exported",
        'run': 'L',
        'scan': True,
    },
    {
        'name': '--msvc-lint: a constructor parameter that hides a member',
        'what': 'the clang twin of MSVC C4458, an error in a Windows UE build; not part of the default (Mac / Android) warning set',
        'file': 'Private/World/AbyssWorldTypes.cpp',
        'append': '\nnamespace { struct FAbyssPlantedHide { int32 Value; explicit FAbyssPlantedHide(int32 Value) : Value(Value) {} }; }\n',
        'expect_file': 'Private/World/AbyssWorldTypes.cpp',
        'expect': r"constructor parameter 'Value' shadows the field 'Value' of 'FAbyssPlantedHide'",
        'run': 'N',
    },
]
RUNS = {
    'A': ['Private/Input/AbyssPicking.cpp', 'Private/Anim/AbyssAnimInstance.cpp', 'Private/Framework/AbyssGameMode.cpp'],
    'B': ['Private/Input/AbyssInputConfig.cpp', 'Private/Input/AbyssInputSubsystem.cpp'],
    'C': ['Private/World/AbyssWorldTypes.cpp', 'Private/Vfx/AbyssVfxDefaults.cpp'],
    'D': ['Private/Audio/AbyssAudioSystem.cpp'],
    'E': ['Private/UI/Hud/SAbyssHud.cpp', 'Private/UI/Widgets/AbyssUiWidgets.cpp', 'Private/UI/Hud/SAbyssMinimap.cpp',
          'Private/UI/Hud/AbyssMinimapTexture.cpp'],
    'F': ['Private/UI/Panels/SAbyssAchievementsPanel.cpp', 'Private/UI/Menu/SAbyssMainMenu.cpp', 'Private/UI/Story/SAbyssStoryOverlay.cpp'],
    'G': ['Private/UI/Panels/SAbyssSkillTreePanel.cpp'],
    'L': ['Private/World/AbyssWorldTypes.cpp', 'Private/Vfx/AbyssVfxDefaults.cpp'],
    'M': ['Private/Platform/AbyssPlatform.cpp', 'Private/Framework/AbyssGameInstance.cpp', 'Private/ContentBuild/AbyssContentBuildLibrary.cpp'],
    'N': ['Private/World/AbyssWorldTypes.cpp'],
}
# extra --define of a run (the opt-in lints of the SDK)
RUN_DEFINES = {'G': ['UE_STUB_STRICT_FORMAT_WIDTH=1']}
# extra options of a run; the configurations of a run (default: mac-editor only)
RUN_OPTIONS = {'L': ['--link-scan'], 'N': ['--msvc-lint']}
RUN_CONFIGS = {'M': ['mac-editor', 'mac-game', 'android-game', 'ios-shipping']}
# a run that passes when nothing is planted wrongly: the faults of these runs are only visible to a lint, not the compiler
ALL_CONFIGS = ['mac-editor', 'mac-game', 'android-game', 'ios-shipping']


# tools/stub_bodies.py must find every body-less template declaration: a miss makes module code that instantiates the
# template with an anonymous-namespace or local type fail with "used but not defined", a false MODULE BUG.
BODIES_PROBE = '''// selftest probe for tools/stub_bodies.py (not an SDK header)
#pragma once
template <typename T>
struct TBodiesProbe
{
	bool Has(const T& Item) const requires requires(const T& A, const T& B) { { A == B } -> std::convertible_to<bool>; };
	void Sort() requires requires(const T& A, const T& B) { { A < B } -> std::convertible_to<bool>; };
	template <typename P> int Count(P Pred) const requires std::is_invocable_r_v<bool, P&, const T&>;
	void Next();
	static constexpr int Size() { return sizeof(T); }
	virtual void Pure() = 0;
};
template <typename T> concept CBodiesProbe = requires(const T& A) { A.Num(); };
template <typename... A> requires (sizeof...(A) > 0) && (std::is_integral_v<A> && ...)
void BodiesProbeJoin(A... Args) UE_STUB_BODY
void BodiesProbeNotATemplate(int Value);
template <typename T> void BodiesProbeAfterConstrainedBody(T Value);
template <typename T> requires requires(const T& A) { A.Num(); }
int BodiesProbeHeadRequires(const T& Range);
template <typename T> T* BodiesProbeDeclaredOnly(); // no-body: only declared in the engine
// the explicit-parameter rule: T is named by the caller and nothing says what the engine's body needs of it
struct FBodiesProbeHolder
{
	template <typename T> T* ExplicitUnconstrained() const;
	template <typename T, typename U> T* ExplicitSecond(const U& Value) const;
	template <typename T> requires std::is_enum_v<T> T ExplicitHeadRequires() const;
	template <typename T> T* ExplicitTrailingRequires() const requires requires { T::StaticClass(); };
	template <std::integral T> T ExplicitConcept() const;
	template <typename T> void Deduced(const T& Value) const;
	template <typename T> explicit operator T*() const;
	template <typename T> T* ExplicitAllowed() const; // explicit-ok: the probe says so
	template <typename T> T* ExplicitWithBody() const { return static_cast<T*>(nullptr); }
};
'''
BODIES_EXPECT = ['Has(', 'Sort(', 'Count(', 'Next(', 'BodiesProbeAfterConstrainedBody(', 'BodiesProbeHeadRequires(',
                 'ExplicitUnconstrained(', 'ExplicitSecond(', 'ExplicitHeadRequires(', 'ExplicitTrailingRequires(', 'ExplicitConcept(',
                 'Deduced(', 'operator T*(', 'ExplicitAllowed(']
# after the bodies are added, exactly these are reported as "explicit type parameter without a requirement"
EXPLICIT_EXPECT = [("'ExplicitUnconstrained'", "'T'"), ("'ExplicitSecond'", "'T'")]


def run_bodies_probe(build):
    """stub_bodies.py on a header with every declaration shape: --check counts them, a run fixes exactly those lines."""
    root = os.path.join(build, 'selftest', 'bodies')
    if os.path.exists(root):
        shutil.rmtree(root)
    os.makedirs(root)
    header = os.path.join(root, 'BodiesProbe.h')
    open(header, 'w', encoding='utf-8').write(BODIES_PROBE)
    tool = [sys.executable, os.path.join(HERE, 'stub_bodies.py'), '--sdk', root]
    check = subprocess.run(tool + ['--check'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
    found = re.search(r'stub_bodies: (\d+) declaration', check.stdout)
    found = int(found.group(1)) if found else -1
    subprocess.run(tool, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
    fixed = [line for line in open(header, encoding='utf-8').read().split('\n') if re.search(r'\bUE_STUB_BODY\b(\s*//.*)?$', line.rstrip())]
    wrong = [e for e in BODIES_EXPECT if sum(1 for line in fixed if e in line) != 1]
    extra = len(fixed) - 1 - len(BODIES_EXPECT)   # one UE_STUB_BODY is in the probe already
    again = subprocess.run(tool + ['--check'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
    left = re.search(r'stub_bodies: (\d+) declaration', again.stdout)
    left = int(left.group(1)) if left else -1
    declared_only = 'BodiesProbeDeclaredOnly(); // no-body' in open(header, encoding='utf-8').read()
    ok = check.returncode == 1 and found == len(BODIES_EXPECT) and not wrong and extra == 0 and left == 0 and declared_only
    detail = '%d of %d body-less template declarations found (requires-expressions, constrained heads, members), then none left' % (found, len(BODIES_EXPECT))
    if not ok:
        detail = 'found %d (expected %d); not given a body: %s; unexpected bodies: %d; left after the rewrite: %d; no-body kept: %s' % (
            found, len(BODIES_EXPECT), ', '.join(wrong) or '-', extra, left, declared_only)
    rows = [('stub_bodies.py on a probe header', 'REPORTED' if ok else 'NOT DETECTED', detail)]
    # the explicit-parameter rule: a stub-bodied template whose explicit type parameter nothing constrains
    flagged = re.findall(r"BodiesProbe\.h:\d+: ('[^']+'): the type parameter ('\w+') is explicit", again.stdout)
    explicit_ok = sorted(flagged) == sorted(EXPLICIT_EXPECT) and again.returncode == 1
    rows.append(('stub_bodies.py: explicit type parameter without a requirement', 'REPORTED' if explicit_ok else 'NOT DETECTED',
                 '%d of %d unconstrained templates reported, %d constrained / deduced / annotated ones left alone' % (len(flagged), len(EXPLICIT_EXPECT), 7)
                 if explicit_ok else 'reported %s, expected %s (exit %d)' % (flagged, EXPLICIT_EXPECT, again.returncode)))
    return ok and explicit_ok, rows


CASES_FILES = [os.path.join(CHECK_DIR, 'tests', 'negative_cases.cpp'), os.path.join(CHECK_DIR, 'tests', 'negative_boundaries.cpp')]
CASE_RE = re.compile(r'^#elif CASE == (\d+)\s*//\s*EXPECT(?:\[([\w-]+)\])?:\s*(.+?)\s*$', re.M)


def run_negative_cases(build, jobs, cxx):
    """tests/negative_*.cpp: code UE rejects must be rejected by the SDK too; the control case of each file must compile."""
    ok, rows = True, []
    for cases_file in CASES_FILES:
        file_ok, file_rows = run_negative_file(cases_file, build, jobs, cxx)
        ok = ok and file_ok
        rows += file_rows
    return ok, rows


def run_negative_file(CASES_FILE, build, jobs, cxx):
    import argparse
    import concurrent.futures
    import compile_check as cc
    ns = argparse.Namespace(build_dir=build, cxx=cxx, define=[], no_pch=False, force=False, only=[], jobs=jobs)
    checker = cc.Checker(ns)
    label = os.path.basename(CASES_FILE)
    text = open(CASES_FILE, encoding='utf-8').read()
    cases = [(int(n), cfg or 'mac-editor', rx) for n, cfg, rx in CASE_RE.findall(text)]
    lines = {int(m.group(1)): text.count('\n', 0, m.start()) + 2 for m in CASE_RE.finditer(text)}
    source = text.split('\n')
    bases = {}
    rows = []
    ok = True
    for config in sorted(set(c for _, c, _ in cases) | {'mac-editor'}):
        base = checker.command_base(config)
        pch, diags = checker.build_pch(config, base)
        if not pch:
            return False, [('negative cases', 'CANNOT RUN', 'the precompiled header of %s does not build: %s' % (config, diags[0]['message'] if diags else '?'))]
        bases[config] = base['cmd'] + ['-include-pch', pch]

    def compile_case(number, config):
        code, output = cc.run(bases[config] + ['-DCASE=%d' % number, CASES_FILE])
        return code, cc.parse_diagnostics(output), output

    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, jobs)) as pool:
        control = {config: pool.submit(compile_case, 0, config) for config in bases}
        futures = [(n, cfg, rx, pool.submit(compile_case, n, cfg)) for n, cfg, rx in cases]
        for config, future in sorted(control.items()):
            code, diags, output = future.result()
            if code != 0 or diags:
                ok = False
                first = diags[0] if diags else {'line': 0, 'message': output.strip()[:200]}
                rows.append(('%s: control (correct code), %s' % (label, config), 'REJECTED', 'tests/%s:%s: %s' % (label, first['line'], first['message'])))
            else:
                rows.append(('%s: control (correct code), %s' % (label, config), 'COMPILES', 'the correct twin of every case'))
        missed = 0
        for number, config, rx, future in futures:
            code, diags, output = future.result()
            snippet = source[lines[number] - 1].strip() if lines[number] - 1 < len(source) else ''
            hit = None
            for d in diags:
                if d['level'] == 'error' and d['file'].endswith(label) and re.search(rx, d['message'] + ' ' + d['option'], re.I):
                    hit = d
                    break
            if hit is None:
                # the error may be reported inside an SDK template with a note pointing at the case
                for d in diags:
                    if d['level'] == 'error' and re.search(rx, d['message'] + ' ' + d['option'], re.I):
                        hit = d
                        break
            if code == 0 or hit is None:
                ok = False
                missed += 1
                why = 'compiles' if code == 0 else 'fails, but not with /%s/: %s' % (rx, (diags[0]['message'] if diags else output.strip())[:160])
                rows.append(('%s: case %d (%s)' % (label, number, config), 'NOT DETECTED', '%s  -> %s' % (snippet[:110], why)))
        rows.append(('%s: negative cases' % label, 'REJECTED' if missed == 0 else 'NOT DETECTED', '%d of %d cases of invalid UE code are rejected' % (len(cases) - missed, len(cases))))
    return ok, rows


def find_fault(fault, report):
    """(hit text or None, detail) for one planted fault in the report of its run."""
    expect_file = fault.get('expect_file', '')
    if fault.get('uht'):
        for f in report.get('uht', {}).get('findings', []):
            if f.get('level') == 'error' and f.get('file', '').replace(os.sep, '/').endswith(expect_file) and re.search(fault['expect'], f.get('rule', '') + ' ' + f.get('message', '')):
                return '%s:%s: [uht/%s] %s' % (f['file'], f['line'], f['rule'], f['message'])
        return None
    if fault.get('scan') or fault.get('build_cs'):
        findings = report.get('link_scan', {}).get('findings', []) if fault.get('scan') else report.get('build_cs', {}).get('findings', [])
        for f in findings:
            if f.get('level') == 'error' and f.get('file', '').replace(os.sep, '/').endswith(expect_file) and re.search(fault['expect'], f.get('rule', '') + ' ' + f.get('message', '')):
                return '%s:%s: [%s] %s' % (f['file'], f['line'], f['rule'], f['message'])
        return None
    hit, hit_configs = None, []
    for config, cfg in report.get('configs', {}).items():
        for unit in cfg['files']:
            for d in unit['diagnostics']:
                if d['level'] == 'error' and d['file'].replace(os.sep, '/').endswith(expect_file) and re.search(fault['expect'], d['message']):
                    hit = hit or '%s:%d: %s' % (d['file'], d['line'], d['message'])
                    if config not in hit_configs:
                        hit_configs.append(config)
    if hit and 'configs' in fault:
        # a fault in a platform / target branch: reported in exactly the configurations that compile the branch
        if sorted(hit_configs) != sorted(fault['configs']):
            fault['why'] = 'reported in {%s}, expected exactly {%s}' % (', '.join(hit_configs), ', '.join(fault['configs']))
            return None
        hit += '  {%s only}' % ', '.join(hit_configs)
    return hit


def main(args=None):
    build = os.path.abspath(getattr(args, 'build_dir', None) or os.environ.get('ABYSS_CC_BUILD') or os.path.join(CHECK_DIR, 'build'))
    root = os.path.join(build, 'selftest')
    core_public = os.path.join(os.path.dirname(CHECK_DIR), 'Source', 'AbyssCore', 'Public')
    ok = True
    rows = []
    # Faults whose rule reads the engine's classes from the Python stub cannot bite without it: say so up front and
    # fail for that reason, not as "NOT DETECTED" (which reads as a hole in the lint).
    stub = find_stub(os.path.dirname(CHECK_DIR))
    have_stub = os.path.exists(stub)
    if not have_stub:
        needing = [f['name'] for f in FAULTS if f.get('needs_stub')]
        print('== selftest: no engine Python stub at %s (set ABYSS_UNREAL_STUB): %d planted faults need it and are not run:' % (stub, len(needing)))
        for name in needing:
            print('              %s' % name)
    for run_name, only in sorted(RUNS.items()):
        scratch = os.path.join(root, run_name, 'Abyssfire')
        if os.path.exists(scratch):
            shutil.rmtree(scratch)
        shutil.copytree(MODULE_DIR, scratch)
        faults = [f for f in FAULTS if f['run'] == run_name]
        scratch_core = None
        if any('core_file' in f for f in faults):
            # a fault in the core's public headers: a scratch copy of AbyssCore/Public (the core itself is never edited)
            scratch_core = os.path.join(root, run_name, 'AbyssCorePublic')
            if os.path.exists(scratch_core):
                shutil.rmtree(scratch_core)
            shutil.copytree(core_public, scratch_core)
        for fault in faults:
            if fault.get('needs_stub') and not have_stub:
                rows.append((fault['name'], 'NEEDS STUB', 'not run: the engine Python stub was not found (%s; set ABYSS_UNREAL_STUB)' % stub))
                ok = False
                fault['planted'] = False
                continue
            path = os.path.join(scratch_core, fault['core_file']) if 'core_file' in fault else os.path.join(scratch, fault['file'])
            shown = fault.get('core_file') or fault['file']
            text = open(path, encoding='utf-8').read()
            if 'append' in fault:
                open(path, 'w', encoding='utf-8').write(text + fault['append'])
                fault['planted'] = True
                continue
            if text.count(fault['find']) != 1:
                rows.append((fault['name'], 'CANNOT PLANT', '%s: the text to mutate occurs %d times (update tools/selftest.py)' % (shown, text.count(fault['find']))))
                ok = False
                fault['planted'] = False
                continue
            open(path, 'w', encoding='utf-8').write(text.replace(fault['find'], fault['replace']))
            fault['planted'] = True
        cmd = [sys.executable, os.path.join(HERE, 'compile_check.py'), '--module-dir', scratch, '--build-dir', os.path.join(root, run_name, 'build'), '--force']
        for config in RUN_CONFIGS.get(run_name, ['mac-editor']):
            cmd += ['--config', config]
        for o in only:
            cmd += ['--only', o]
        for d in RUN_DEFINES.get(run_name, []):
            cmd += ['--define', d]
        cmd += RUN_OPTIONS.get(run_name, [])
        if scratch_core:
            cmd += ['--core-public', scratch_core]
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
        report_path = os.path.join(root, run_name, 'build', 'report.json')
        report = json.load(open(report_path, encoding='utf-8')) if os.path.exists(report_path) else {'configs': {}, 'uht': {'findings': []}}
        if proc.returncode == 0:
            rows.append(('run ' + run_name, 'NOT DETECTED', 'the check passed although faults were planted'))
            ok = False
        elif not report.get('configs'):
            rows.append(('run ' + run_name, 'CANNOT RUN', proc.stdout.strip()[-300:]))
            ok = False
        for fault in faults:
            if not fault.get('planted'):
                continue
            hit = find_fault(fault, report)
            rows.append((fault['name'], 'REPORTED' if hit else 'NOT DETECTED', hit or ('%s (%s)' % (fault['what'], fault['why']) if fault.get('why') else fault['what'])))
            ok = ok and hit is not None
    bodies_ok, bodies_rows = run_bodies_probe(build)
    ok = ok and bodies_ok
    rows += bodies_rows
    neg_ok, neg_rows = run_negative_cases(build, getattr(args, 'jobs', 3) or 3, getattr(args, 'cxx', None) or 'clang++')
    ok = ok and neg_ok
    rows += neg_rows
    print('== selftest: faults planted in a scratch copy of Source/Abyssfire (%s) and tests/negative_*.cpp' % root)
    for name, status, detail in rows:
        print('%-13s %s' % (status, name))
        print('              %s' % detail[:300])
    planted = sum(1 for f in FAULTS if f.get('planted'))
    print('%d faults planted in %d runs' % (planted, len(RUNS)))
    needs_stub_only = not ok and not any(status in ('NOT DETECTED', 'CANNOT PLANT', 'CANNOT RUN') for _, status, _ in rows)
    fail_text = ('FAIL (incomplete: the stub-dependent faults were not run, no engine Python stub; everything that ran was reported)' if needs_stub_only
                 else 'FAIL (the check missed a planted fault: the SDK / fake UHT / a lint is too permissive)')
    print('SELFTEST: %s' % ('PASS (every planted fault and every negative case was reported)' if ok else fail_text))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())

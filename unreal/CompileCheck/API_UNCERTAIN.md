# Engine API that could not be confirmed for UE 5.8

The compile check type-checks the module against a stub SDK. Reflected API is cross-checked against the engine's Python
stub (UE 5.6); everything else comes from UE 5.x source knowledge. This file lists the places where it could not be
told whether UE 5.8 has an API with the signature the module (or the SDK) assumes. Each entry says what the code
assumes, the evidence both ways, what the SDK declares, and the safest alternative. An entry is closed either by a real
UE 5.8 build or by moving the module to an API that is not in doubt.

Verdict vocabulary: see README.md ("The three verdicts").

The final audit went through every entry and gave each a verdict from UE 5.x source knowledge (it had no engine
either, so a verdict is a second opinion, not a confirmation). Each entry ends with that verdict.

**First engine build (2026-11): UE 5.8.3 (Release-5.8), macOS arm64, commit `83228c4`.** `AbyssfireEditor` compiled and
the smoke autotest ran. A real compile confirms exactly one thing about this file: every engine call the module makes
DIRECTLY exists in 5.8.3 with a compatible signature. That closes #3 and #8 (see *Closed by the first engine build*)
and confirms the module-side resolutions of #2 and #7. It does not close a hedged call: `if constexpr (requires ...)`
compiles whichever way the engine is, so the build cannot say which branch 5.8.3 took. #1, #4, #5 and #6 stay open
until the engine headers are read (the greps below) or the run log shows the fallback's message; so does #9, added
after that build with the autotest's smoothness measurement and hedged from the start. None of the five can stop a
build. The build was the editor target on the Mac only: the game, Android and iOS targets have not been built.

## What to check on the Mac (one question per open entry)

Paths are relative to the engine's `Engine/Source`. Each line is a `grep` on one engine header, then what the answer
means for the module.

| # | Question | If the answer is "no" |
|---|---|---|
| 1 | `grep -n GetMaterialResource Runtime/Engine/Classes/Materials/MaterialInterface.h`: is there an overload taking `ERHIFeatureLevel::Type` or one taking `EShaderPlatform`, returning `FMaterialResource*`? And `grep -n "FinishCompilation\|GetCompileErrors\|IsCompilationFinished" Runtime/Engine/Public/MaterialShared.h`? | Editor target only: `AbyssContentBuildLibrary.cpp` / `AbyssZoneActor.cpp` fail in `AbyssEditorMaterialResource` / `ShaderGateResource`; return an empty error list / switch the shader gate off (entry 1) |
| 5 | `grep -n MarkRenderInstancesDirty Runtime/Engine/Classes/Components/ActorComponent.h`: declared, and under `public:`? | Nothing fails to compile. The log shows once "Instanced meshes: UActorComponent::MarkRenderInstancesDirty not found on this engine" and instance updates re-create scene proxies: apply the alternative of entry 5 if the frame time shows it |
| 4 | `grep -n TCopyQualifiersFromTo_T Runtime/CoreUObject/Public/Templates/Casts.h`: do `Cast` / `CastChecked` return a qualifier-copying pointer? | Nothing fails (the SDK has the strict form); the check is then stricter than the engine for casts of const objects |
| 6 | `grep -n "GetAudioListenerPosition" Runtime/Engine/Classes/GameFramework/PlayerController.h`: is it `const`? | Nothing fails (the module calls it on a non-const controller) |
| 9 | `grep -n "struct FStatUnitData" -A 40 Runtime/Engine/Public/UnrealClient.h` and `grep -n GetStatUnitData Runtime/Engine/Public/UnrealClient.h`: does `FCommonViewportClient` have a public `GetStatUnitData()`, and does the struct have `RawGameThreadTime`, `RawRenderThreadTime`, `RawRHITTime`, `RawGPUFrameTime[]`? | Nothing fails to compile. An autotest run with `-abyssstatunit` then logs "stat unit data NOT on this engine (thread times null)" at its start and `report.json` has `perf.threadTimes.framesWithThreadTimes` 0: read `stat unit` off the screen instead (entry 9) |

The first 5.8 build closed #8 and #3 (their rows are gone from this table and their `// UNVERIFIED` markers from the
SDK). For the hedged entries the run log is a second signal: #5's fallback logs "Instanced meshes:
UActorComponent::MarkRenderInstancesDirty not found on this engine" once per run. The smoke run at `83228c4` played
fights (VFX pools update their instances every frame there) and its `report.json` lists all 34 Warning lines of the
run: that line is not among them, so 5.8.3 took the direct branch. #5 is left under *Open* with this noted, until the
header is read: a log is evidence of the branch taken, not of the declaration. The README says how to re-check the
whole SDK against a real engine tree.

## Open

### 1. `UMaterialInterface::GetMaterialResource`: feature level or shader platform

- **Where**: `Source/Abyssfire/Private/ContentBuild/AbyssContentBuildLibrary.cpp:22-33` (`AbyssEditorMaterialResource`),
  used by `GetMaterialCompileErrors` (`:162`, the call is at `:170`), and the same hedge in
  `Source/Abyssfire/Private/World/AbyssZoneActor.cpp:284-296` (`ShaderGateResource`, the zone's shader gate, which also
  hedges `FMaterial::IsCompilationFinished()` at `:313-325`). Editor only (`#if WITH_EDITOR`).
- **What the code assumes**: one of `GetMaterialResource(EShaderPlatform)` or
  `GetMaterialResource(ERHIFeatureLevel::Type)` exists and returns `FMaterialResource*`, with
  `FMaterial::FinishCompilation()` and `FMaterial::GetCompileErrors()` on the result. The module already hedges with
  `if constexpr (requires ...)`: the shader-platform form is used when it compiles, the feature-level form otherwise.
- **Evidence for the feature-level form**: `GetMaterialResource(ERHIFeatureLevel::Type, EMaterialQualityLevel::Type =
  Num)` is the long-standing declaration (UE 4.2x through 5.x) on `UMaterialInterface`, `UMaterial` and
  `UMaterialInstance`.
- **Evidence for the shader-platform form**: recent engine branches are believed to key material resources by
  `EShaderPlatform` and to deprecate the feature-level overload. Not reflected, so the Python stub cannot settle it.
- **What the SDK declares**: the feature-level overloads. The shader-platform overloads are in a block marked
  `// UNVERIFIED` in `sdk/Runtime/Engine/Classes/Materials/MaterialInterface.h`, compiled only with
  `UE_STUB_UNVERIFIED_MATERIAL_RESOURCE_BY_SHADER_PLATFORM=1`. Both branches of the module's hedge type-check:
  `./run.sh --config mac-editor --only ContentBuild --only World` (fallback branch) and the same with
  `--define UE_STUB_UNVERIFIED_MATERIAL_RESOURCE_BY_SHADER_PLATFORM=1` (first branch).
- **Residual risk**: 5.8 could have removed the feature-level overload and given the other one a different shape, or
  moved `FinishCompilation` / `GetCompileErrors`. Then `GetMaterialCompileErrors` fails to compile in the editor target
  only.
- **Safest alternative**: none that is better known. If it fails on the Mac, the function can return an empty array
  without touching the resource (it only feeds the content-build log), which removes the dependency altogether.
- **Audit verdict**: the feature-level form (`ERHIFeatureLevel::Type, EMaterialQualityLevel::Type = Num`, const and
  non-const) exists at least through 5.6; a shader-platform overload next to a deprecated (not yet removed)
  feature-level one is plausible for 5.7 / 5.8; `FMaterial::FinishCompilation` / `IsCompilationFinished` /
  `GetCompileErrors` exist under `WITH_EDITOR`. Keep the hedge. Stays open: editor target only.

### 4. `Cast` / `CastChecked` / `ExactCast`: does the result keep the constness of the argument

- **Where**: every `Cast<T>(X)` / `CastChecked<T>(X)` of the module (61 calls when this was recorded); the SDK
  declaration is `sdk/Runtime/CoreUObject/Public/Templates/Casts.h`.
- **What is in doubt**: the return type. Older engines declare `To* Cast(From* Src)`: constness is dropped, so
  `AActor* A = Cast<AActor>(ConstObject)` compiles. Recent engines are believed to declare
  `TCopyQualifiersFromTo_T<From, To>* Cast(From* Src)` (and the same for `ExactCast`, `CastChecked` and the
  `TObjectPtr` overloads): the result of casting a `const UObject*` is a `const AActor*` and the line above is an error.
- **Evidence for the propagating form**: memory of the 5.x `Templates/Casts.h` (the `TCopyQualifiersFromTo_T` return
  type and the `Templates/CopyQualifiersFromTo.h` header that exists for it). **Against**: nothing found; not reflected,
  so the Python stub cannot settle it, and there is no engine source here.
- **What the SDK declares**: the const-propagating form, in a block marked `// UNVERIFIED` (always on). It is the strict
  side: code that compiles against it also compiles against the const-dropping form, so a PASS holds for either engine.
  The `TWeakObjectPtr` overloads propagate too (the engine's own are believed to return a plain `T*`; a
  `TWeakObjectPtr<const U>` argument is the only case that differs, and the module has none).
- **State of the module**: it compiles against the propagating form in every checked file (no cast of a const object
  into a mutable pointer). `tests/negative_cases.cpp` cases 122 and 123 pin the strict behaviour, the control case its
  correct twin (`const AActor* A = Cast<AActor>(ConstObject)`).
- **Residual risk**: none for the build (the strict form was chosen). If 5.8 drops constness, case 122 / 123 describe
  code the engine accepts; that only makes this check stricter than the engine.
- **Safest alternative for new code**: keep the result as const as the argument, or cast a mutable pointer.
- **Audit verdict**: believed as written (`Cast` / `CastChecked` / `ExactCast` return
  `TCopyQualifiersFromTo_T<From, To>*`). No build risk in either case.

### 5. `UActorComponent::MarkRenderInstancesDirty()` (hedged in the module)

- **Where**: `AbyssWorldUtil::MarkInstancesDirty` (`Source/Abyssfire/Public/World/AbyssWorldTypes.h:127-146`), called
  on `UInstancedStaticMeshComponent` from `Private/World/AbyssZoneActor.cpp:1762`, `:1966`,
  `Private/World/AbyssSetPieces.cpp:322` and the VFX pools (`Private/Vfx/AbyssVfxSystem.cpp:382`, `:1580`). Before CC2
  these five places called `Component->MarkRenderInstancesDirty()` directly.
- **What the code assumes**: after a batch of `SetCustomDataValue` / `SetCustomData` / `BatchUpdateInstancesTransforms`
  calls made with `bMarkRenderStateDirty = false`, one `MarkRenderInstancesDirty()` sends the changed instance data to
  the renderer at the end of the frame, without re-creating the render state.
- **Evidence for**: memory of `Components/ActorComponent.h` since the 5.4 instance-data manager: `bRenderInstancesDirty`,
  `void MarkRenderInstancesDirty();` ("marks the instances as dirty - changes to instance transforms / custom data will
  be sent to the rendering thread at the end of the frame") and `SendRenderInstanceData_Concurrent()`, with the ISM
  setters calling it when `bMarkRenderStateDirty` is true. **Against**: nothing found, but it is not reflected (the 5.6
  Python stub has no `mark_render_*` at all), and its access level (public on `UActorComponent`) is from memory.
- **What changed in the module**: the five calls go through one helper with the module's usual hedge,
  `if constexpr (requires(ComponentType& C) { C.MarkRenderInstancesDirty(); })`: the incremental update where the engine
  has the member (and it is accessible), else `MarkRenderStateDirty()`, which every engine has. Behaviour is unchanged
  on an engine with the member.
- **What the SDK declares**: `void MarkRenderInstancesDirty();` on `UActorComponent`
  (`sdk/Runtime/Engine/Classes/Components/ActorComponent.h`), in a block marked `// UNVERIFIED`, on by default;
  `--define UE_STUB_UNVERIFIED_NO_MARK_RENDER_INSTANCES_DIRTY=1` removes it. Both branches of the hedge type-check in
  every configuration: `./run.sh --only World --only Actors --only Vfx` and the same with that `--define`.
- **Residual risk**: no compile risk either way. If 5.8 lacks the member, the fallback re-creates the scene proxy of
  every touched instanced component on each update (the occlusion fade and the VFX pools do that every frame): correct
  picture, a frame-time cost to look for in the first Mac profile. `stat scenerendering` / a missing
  `MarkRenderInstancesDirty` in the engine header tells which branch was compiled.
- **Safest alternative if the fallback cost shows**: pass `bMarkRenderStateDirty = true` to the LAST update call of a
  batch (`SetCustomDataValue`, `SetCustomData` and `BatchUpdateInstancesTransforms` all take the flag); the engine then
  picks its own cheapest path.
- **The fallback is not silent (final audit)**: the `else` branch of the helper calls
  `AbyssWorldUtil::WarnInstancedFallback()` (`Private/World/AbyssWorldTypes.cpp`), which logs one `LogAbyss` warning per
  run ("Instanced meshes: UActorComponent::MarkRenderInstancesDirty not found on this engine ..."), like the module's
  other engine-version shims. Before that, a 5.8 without the member would have built green and run the slow path every
  frame without a trace. Mac checklist: search the first editor log for "Instanced meshes:".
- **Audit verdict**: believed as written (`void MarkRenderInstancesDirty();`, public on `UActorComponent` since 5.4);
  the hedge is harmless and stays.
- **Run evidence (first engine run, UE 5.8.3, commit `83228c4`)**: the fallback's once-per-run Warning is absent from
  the smoke autotest's complete warning list although instanced components were updated throughout (every VFX pool,
  each frame it draws). 5.8.3 therefore has the function and the module calls it. Kept open only for the
  header check; nothing depends on the answer.

### 9. `FCommonViewportClient::GetStatUnitData()` and the members of `FStatUnitData` (hedged in the module)

- **Where**: `Source/Abyssfire/Private/Framework/AbyssAutoTestSubsystem.cpp`, `AbyssAutoTest_ReadStatUnit` /
  `AbyssAutoTest_CopyStatUnit` (called from `FAbyssAutoTestRun::CloseFrame`, only in an autotest run with
  `-abyssstatunit`, after the stat was turned on at the title screen: `AbyssAutoTest_SetStatUnit`, hedged
  `GEngine->SetEngineStat(World, GameViewportClient, "Unit", true)`, else `GEngine->Exec(World, TEXT("stat unit"))`).
- **What the code assumes**: `UGameViewportClient` inherits `const FStatUnitData* GetStatUnitData() const` (and
  `bool IsStatEnabled(const FString&) const`, asked with "Unit" before the toggle is sent) from `FCommonViewportClient`, and the struct has the unfiltered values of the last `stat unit` draw in milliseconds:
  `RawGameThreadTime`, `RawRenderThreadTime`, `RawRHITTime`, `RawGPUFrameTime[GPU index]`. The numbers describe the
  frame before the one at whose end they are read (the stat is drawn from last frame's thread timers), so the autotest
  writes them one sample back (`AbyssSmoothnessRules::ShiftThreadTimesBack`), next to the `frameMs` they time. Also:
  `UEngine::SetEngineStat(UWorld*, FCommonViewportClient*, const FString& InName, const bool bShow)` is public, takes
  the stat's name without `STAT_`, and sets rather than toggles (it sends the toggle only when
  `ViewportClient->IsStatEnabled(InName) != bShow`).
- **Why this and not the engine globals**: `stat unit` itself reads `GGameThreadTime`, `GRenderThreadTime`,
  `GRHIThreadTime` (RenderCore) and `RHIGetGPUFrameCycles()` (RHI). Those are plain global variables and a free
  function; their header and even their existence changed between 5.x releases (the GPU profiler was reworked), and C++
  has no way to ask at compile time whether a global name exists, so a direct use cannot be hedged: on an engine
  without the name the Mac build breaks. A class member can be asked for (`requires { Data.RawGameThreadTime; }`), so
  the module reads the copy `stat unit` keeps. The same reasoning rules out `GNumDrawCallsRHI` /
  `GNumPrimitivesDrawnRHI` (draw calls, triangles) and the Slate batch counters: they stay operator console steps
  (`report.json` `perf.operatorConsole`, README "In-game autotest").
- **Evidence for**: memory of `UnrealClient.h` (4.2x through 5.x): `struct FStatUnitData` with the filtered and the
  `Raw...` floats, `FCommonViewportClient::GetStatUnitData()` returning the pointer allocated in its constructor, and
  `UGameViewportClient::Draw` calling `DrawStat` while the stat is enabled. **Against**: not reflected (nothing in the
  5.6 Python stub); the member names are from memory; whether Metal reports a GPU time in `-game` is unknown (the
  audit asks for exactly that check).
- **The hedge**: `if constexpr (requires { Engine->SetEngineStat(World, Client, FString(), true); })` for the switch-on
  (absent, or the stat not on afterwards by `IsStatEnabled`: the console command is sent, as before; the run log says
  which of "set on the game viewport" / "requested by console command" happened).
  `if constexpr (requires { Client->GetStatUnitData(); })`, then one `requires` per member. A missing
  entry point or member leaves the value at `Unavailable`, which `report.json` writes as `null` and `frames.csv` as an
  empty field, never as 0. A value of exactly 0 from the engine (the stat is not being drawn, or the RHI has no GPU
  timer) is treated as unavailable too.
- **What the SDK declares**: `struct FStatUnitData` and `FCommonViewportClient::GetStatUnitData()` in
  `sdk/Runtime/Engine/Public/UnrealClient.h`, in two blocks marked `// UNVERIFIED`, and `UEngine::SetEngineStat` in
  `sdk/Runtime/Engine/Classes/Engine/Engine.h` in a third, all on by default;
  `--define UE_STUB_UNVERIFIED_NO_STAT_UNIT_DATA=1` removes the three. Both branches type-check:
  `./run.sh --only Framework` and the same with that `--define`.
- **The fallback is not silent**: every autotest run logs at its start which of `FApp::GetIdleTime` and the stat unit
  data this build has (`AutoTest: frame samples with FApp::GetIdleTime yes, stat unit data readable ...`), and
  `report.json` `perf.threadTimes` counts the frames that carried thread and GPU times.
- **Residual risk**: none for the build. A wrong assumption about what the values mean (filtered instead of raw, a
  different unit) would show as thread times that do not add up against `frameMs`: compare one run's `perf.run.threads`
  with the overlay on screen. A wrong assumption about the one-frame offset (the values time the same frame, or the
  render / GPU ones lag two) would put a hitch's split on the row before or after it in `longestFrames` / `frames.csv`
  (group averages are unaffected): on the first run look at a zone-build row, whose `gameMs` should hold its
  `zoneBuildMs`.
- **Mac checklist**: the two greps in the table above, and `grep -n SetEngineStat Runtime/Engine/Classes/Engine/Engine.h`;
  then one run with `-abyssstatunit` and a look at
  `perf.threadTimes.framesWithGpuTime` (0 = Metal gives no GPU number in `-game`: use `stat gpu` / Instruments).

### 6. `APlayerController::GetAudioListenerPosition`: const or not (hedged in the module)

- **Where**: `Source/Abyssfire/Private/Audio/AbyssAudioSystem.cpp:1267-1273` (`ComputeVirtualEmitter`: the listener's
  location, front and right axes for the A4 panning of world cues and point emitters).
- **What the code assumed**: the call was made through a `const APlayerController*`, so it needed
  `virtual void GetAudioListenerPosition(FVector& OutLocation, FVector& OutFrontDir, FVector& OutRightDir) const`.
- **Evidence for the const form**: memory of the UE 5.x `GameFramework/PlayerController.h`, where it sits next to
  `GetAudioListenerAttenuationOverridePosition(FVector&) const`. **Against**: older engines declare it without `const`,
  and the release in which the qualifier arrived is not known here. Not a `UFUNCTION` (the 5.6 Python stub has only
  `set_audio_listener_override` / `clear_audio_listener_override` on `PlayerController`), no engine source in the
  container.
- **What changed in the module**: the local is now a plain `APlayerController*` (`GetListenerController()` already
  returns one). A call through a non-const pointer is valid for both declarations; behaviour is unchanged.
- **What the SDK declares**: the const form in a block marked `// UNVERIFIED` (on by default) in
  `sdk/Runtime/Engine/Classes/GameFramework/PlayerController.h`;
  `--define UE_STUB_UNVERIFIED_NO_CONST_AUDIO_LISTENER_POSITION=1` swaps in the non-const form. Both type-check in
  every configuration: `./run.sh --only Audio` and the same with that `--define`. `tests/negative_cases.cpp` case 154
  pins the three `FVector&` parameters.
- **Residual risk**: none for this call. A module class that overrides the function would have to match the engine's
  constness (the module has no such override).
- **Safest form for new code**: call it on a non-const controller.
- **Audit verdict**: believed as written (const in 5.x). No build risk in either case.

## Closed by the first engine build (UE 5.8.3, commit `83228c4`)

Both were direct calls; the module compiled against the real headers, so the API exists as the module uses it. The
`// UNVERIFIED` markers are removed from the SDK. What the build does not show is noted per entry.

### 3. `FCoreDelegates::GetApplicationWillTerminateDelegate()` (closed)

- **Where**: `Source/Abyssfire/Private/Framework/AbyssGameInstance.cpp` (`AddUObject` at Init, `RemoveAll` at
  Shutdown).
- **Was in doubt**: whether 5.8 has the static accessor, and its return type.
- **Closed by**: the two lines compiled on 5.8.3. The accessor exists and returns a multicast delegate with
  `AddUObject` / `RemoveAll`.
- **Not shown by the build**: the exact return type (the SDK declares `FTSSimpleMulticastDelegate&`; the module does
  not depend on it).

### 8. The Slate `FVector2D` bridge (`UE_ENABLE_SLATE_VECTOR_DEPRECATION_MECHANISMS`) (closed)

- **Where**: the 22 lines in 12 files where the module hands an `FVector2D` to Slate without an explicit conversion
  (`./run.sh --define UE_ENABLE_SLATE_VECTOR_DEPRECATION_MECHANISMS=0` lists them: `FGeometry::AbsoluteToLocal`,
  `IsUnderLocation`, `FSlateApplication::LocateWindowUnderMouse`, `FSlateRect`, `FSlateImageBrush`,
  `SScrollBox::FArguments::ScrollBarThickness`).
- **Was in doubt**: whether 5.8 still builds with the mechanism on (it was the one entry that could stop a game
  target).
- **Closed by**: all 22 argument sites compiled on 5.8.3, so the engine's default is still 1 there.
- **Not shown by the build**: which Slate functions take the bridge types and which still take `FVector2D` is still
  from memory in the SDK (both compile in the module either way), and the mechanism is a deprecation path: it will
  go in some later engine. The bridge-off run above stays the list of lines to convert when it does
  (`UE::Slate::CastToVector2f(...)` / `FVector2f(...)` per argument).

## Resolved by changing the module

### 2. `FVector2D::ComponentMin` / `ComponentMax`

- **Where**: `Source/Abyssfire/Private/Input/AbyssPicking.cpp:92-93` (screen-space bounds of a pick box; lines 90-91 before the
  fix).
- **What the code assumed**: `FVector2D` has member functions `ComponentMin(Other)` / `ComponentMax(Other)`.
- **Evidence**: those members exist on `FVector` (`TVector`), `FIntPoint` and `FIntVector`. `TVector2` instead has the
  static `Min(A, B)`, `Max(A, B)` and `Clamp` (since UE 4). No reflected counterpart to check.
- **Resolution**: the module now calls `FVector2D::Min(OutMin, Projected)` / `FVector2D::Max(OutMax, Projected)`,
  which exist in every UE 4 / 5 version and compute the same per-component result. Even if a later engine added the
  member forms, the static forms remain valid.
- **Confirmed**: the static forms compiled on UE 5.8.3 (commit `83228c4`). The pick now builds its rectangle in
  `Private/Input/AbyssPickRules.h` (`RectOf`) and no longer calls them.

### 7. `FSlateRenderer::GetResourceHandle(const FSlateBrush&, FVector2f, float)`: type of the size parameter

- **Where**: `Source/Abyssfire/Private/UI/Core/AbyssUiDraw.cpp:319` (`FAbyssPainter::CustomVerts`: the resource handle
  that `FSlateDrawElement::MakeCustomVerts` needs for the texture of a custom-vertex shape).
- **What the code assumed**: the three-argument virtual with `FVector2f LocalSize`
  (`Renderer->GetResourceHandle(*SourceBrush, FVector2f::ZeroVector, 1.0f)`).
- **Evidence**: the three-argument form arrived with vector-image brushes in 5.0 taking `FVector2D LocalSize`; it is
  believed to take `FVector2f` since the Slate float conversion (5.2). `FVector2f` does not convert implicitly to
  `FVector2D`, so the call compiles under only one of the two. Not reflected.
- **Resolution**: the module now calls the one-argument overload `GetResourceHandle(const FSlateBrush&)`, the form
  `SMeshWidget` and other engine code have used since UE 4 (non-virtual since 5.0, forwarding a zero size and scale 1,
  which only a vector image would read). Same result for a texture brush; no dependence on the parameter type.
  `tests/negative_cases.cpp` case 175 pins the by-reference parameter.
- **Confirmed**: the one-argument call compiled on UE 5.8.3 (commit `83228c4`).
- **Residual risk**: none known. The SDK still declares the three-argument virtual with `FVector2f` (unused by the
  module).

## SDK declarations made at lower confidence

These are not used in a way that decides a verdict today, but later stages should know they are from memory. None is
reflected in a form the Python stub exposes.

After the first engine build (UE 5.8.3): a row whose "If wrong" names a line the module calls directly is confirmed
in the direction that matters for the build (the call compiles); the `BindAction` payload row is one of them. A row
where the SDK is the stricter side, or that the module does not use, is as unconfirmed as before: the build says
nothing about declarations the module does not reach.

| SDK declaration | Assumption | If wrong |
|---|---|---|
| `Misc/VarArgs.h` format-string checker | Reimplements `UE::Core::Private::FormatStringSan`: `%s` needs a `TCHAR*`, `%d` an integer or enum, `%f` a float / double; too few or too many arguments are errors | A stricter or looser real checker; the module only uses plain `%s %d %f %lld %u %x` forms |
| `sdk/modules.json` | PUBLIC dependency lists of the engine modules (`Engine` re-exports `Slate`, `SlateCore`, `InputCore`, `RenderCore`, `RHI`, `PhysicsCore`, `AudioExtensions`, `DeveloperSettings`; `RHI` re-exports `ApplicationCore`) | A header could be reachable here and not in UBT. The Build.cs lint of every run (final audit) now requires each included engine header to belong to a module `Abyssfire.Build.cs` lists itself, so no module file depends on a re-export (10 modules, 169 distinct headers; `AudioMixer`, `DeveloperSettings` and `RenderCore` are listed and not included from) |
| `UObject/Class.h` includes `Math/RandomStream.h`, `Misc/Guid.h`, `HAL/CriticalSection.h` | The real header's include list | `Public/Camera/AbyssCameraRig.h` now includes `Math/RandomStream.h` itself, so nothing depends on it |
| `UEnhancedInputComponent::BindAction(..., Func, VarTypes... Vars)` | Payload arguments after the member function (the form Lyra uses) | `Private/Input/AbyssPlayerControllerInput.cpp:157` binds with an `ActionId` payload |
| `EMaterialParameterType::TextureCollection` | Present between `Texture` and `Font` (the 5.6 stub has texture-collection parameter getters) | Enumerator order only; the module uses `EMaterialParameterType::Texture` |
| `SConstraintCanvas::FSlot` ZOrder | Not declared (CC5; CC1 had `SLATE_ATTRIBUTE(float, ZOrder)`). UE 4 took a `TAttribute<float>`; 5.x is believed to be `SLATE_ARGUMENT(TOptional<float>, ZOrder)` with `SetZOrder(float)`. Neither could be confirmed and the module never sets it | A first `.ZOrder(...)` on a constraint canvas slot is reported as "no member named 'ZOrder'": add the form the engine has then. The slot's `Offset` / `Anchors` / `Alignment` / `AutoSize` attributes are what the module uses (`Private/UI/Root/SAbyssUiRoot.cpp`, `Private/UI/Story/SAbyssStoryOverlay.cpp`) |
| `HashCombineFast` (`Templates/TypeHash.h`) | Only the two-argument form is declared. A variadic overload may exist in 5.x (an API reference summary says so; its parameter list could not be read) | Valid code that passes three or more hashes is rejected here; nesting the two-argument form is valid in every version. The module does not call it |
| `TDereferenceWrapper` rule (`Templates/Sorting.h`) | `TArray::Sort` / `StableSort` / `Heapify` / `HeapPush` / `HeapPop`, `TSet::Sort`, `TMap::KeySort` / `ValueSort` and `Sort(T**, ...)` hand a predicate the pointees of raw-pointer elements (`T&`), and `Sort()` compares pointees with `operator<`; non-pointer elements must be accepted as `const T&`. The heap functions without a predicate reject raw-pointer elements (the engine's default there is `TLess<ElementType>` behind the wrapper) | A predicate that takes non-const `T&` for a non-pointer element, or a predicate-less heap of raw pointers, is rejected here although the engine may accept it; the const-reference predicate and an explicit heap predicate are valid either way. The module sorts only values (`int32`, structs, strings) |
| Sorting a container of `TObjectPtr<T>` (`UObject/ObjectPtr.h`) | Not declared at all: `TArray<TObjectPtr<T>>::Sort` / `StableSort` / `Heapify`, `TSet` / `TMap` sorts of `TObjectPtr` are rejected. The engine is believed to specialise `TDereferenceWrapper` for `TObjectPtr` (predicate gets `T&`, as for `T*`), which could not be confirmed | A first use in the module is reported as "no matching member function": decide then (predicate over `const T&` if the specialisation exists, else over the `TObjectPtr`), or sort a `TArray<T*>` / use `Algo::Sort` with an explicit projection. The module does not sort such containers |
| `TPanelChildren<SlotType>::Sort` | The predicate is `bool(const SlotType&, const SlotType&)` | The engine passes the slots through `TUniquePtr` dereference (non-const `SlotType&`); a predicate with non-const parameters is rejected here. The module does not call it |
| `FChildren::ForEachWidget` | `Pred(SWidget&)`, and `Pred(const SWidget&)` on const children | The module does not call it |
| Functions not declared in the SDK: nothing in the module uses them, and their exact 5.x parameter types or body requirements are not known here (for `MakeShareable(Object, Deleter)`: `TSharedPtr` / `TSharedRef` here lack the proxy constructor it needs) | `FSlateLayoutTransform::TransformPoint` / `TransformVector`, `FMath::CubicInterp`, `MakeShareable(Object, Deleter)`, `UE::Tasks::FTaskEvent::AddPrerequisites`, the free `UE::Tasks::Wait(Tasks, Timeout)` | A later use is reported as a missing member: declare it then, with its real parameter types |
| Character types (`HAL/Platform.h`, `TIsCharType`) | Mac / iOS / Android: `typedef char16_t WIDECHAR; typedef WIDECHAR TCHAR;`, `WIDETEXT("x")` is `u"x"`. `UCS2CHAR` / `UTF16CHAR` are `uint16` and `UTF32CHAR` is `uint32` (`FGenericPlatformTypes::CHAR16` / `CHAR32`). The engine's `UCS2CHAR` overloads of `FString` and its `TIsCharType` specialisations for `UCS2CHAR`, `UTF32CHAR` and (with `PLATFORM_TCHAR_IS_CHAR16`) `wchar_t` are deliberately not declared | `Str += SomeUint16`, `Str + SomeUint32`, `Str += L'x'` and `FString(const uint16*)` are rejected here although the engine is believed to accept them (it appends a code unit, which is rarely what such code means). Nothing here accepts an integer or an `L""` literal as a character. The module uses none of these |
| `FString` character operators (`Containers/UnrealString.h`) | `operator+=(Char)`, `operator+(const FString&, Char)`, `operator+(FString&&, Char)` and `AppendChar(Char)` are templates constrained by `TIsCharType` (`AppendChar` took a plain `TCHAR` before the 5.4 string refactor). `operator+(Char, const FString&)` is not declared: it could not be confirmed | `TEXT('#') + Str` and `Str.AppendChar(SomeInteger)` are rejected here; `TEXT("#") + Str` and `Str.AppendChar(static_cast<TCHAR>(Code))` are valid in every version. The module appends `TCHAR` values only |
| `FName` constructors (`UObject/NameTypes.h`) | Implicit from `const WIDECHAR*` / `ANSICHAR*` / `UTF8CHAR*` and `EName`; `explicit` from `TStringView<ANSICHAR / WIDECHAR / UTF8CHAR>`; none from `FString` (so `FName(Str)` goes through the explicit view constructor and `FName N = Str;` fails). The `(Name, Number)` and `(Len, Name)` forms are declared for `WIDECHAR` only | A call with an `ANSICHAR*` name and a number is rejected here although the engine has that overload |
| `TArray` allocator- / element-changing construction and assignment (`Containers/Array.h`) | Both converting constructors are plain `explicit`; the move form also needs the elements to be constructible from a const reference (`MoveOrCopy` copies when the allocation cannot change hands); there is an allocator-changing copy assignment and no allocator-changing move assignment | Moving a `TArray` of move-only elements between allocators that the engine can move between (`TCanMoveBetweenAllocators`, e.g. the 32- and 64-bit default allocators) is rejected here. The module has no such array |
| `FText::FormatNamed` / `FormatOrdered` (`Internationalization/Text.h`) | `FormatNamed` takes at least one Name / Value pair, names construct an `FString`, values an `FFormatArgumentValue`; `FormatOrdered` takes at least one value (the engine's `TextFormatUtil` helpers have no overload for an empty pack) | A call with only the format is rejected here; `FText::Format(Fmt, FFormatNamedArguments())` is valid either way. The module calls neither |
| `FSoftObjectPath` constructors (`UObject/SoftObjectPath.h`) | Since 5.1 only the `FString` constructor is implicit; the `const TCHAR*`, string-view and `nullptr` constructors are `explicit`; `operator=` takes `FString` and `const TCHAR*` | If one of them is implicit in 5.8, `FSoftObjectPath P = TEXT("...")` or passing a literal / `nullptr` for an `FSoftObjectPath` parameter is rejected here although valid. The module always writes `FSoftObjectPath(...)` |
| `TTuple` / `TPair` forwarding constructor (`Templates/Tuple.h`) | Implicit when every element converts implicitly, `explicit` otherwise (so `TPair<FName, int32> P = { Id, 1 };` compiles). UE 4 had it plainly `explicit`; the 5.x form is from memory | If it is plainly `explicit` in 5.8, a braced pair passed where a `TPair` is expected (`Array.Add({ A, B })`, `return { A, B };`) compiles here and not on the Mac; `MakeTuple(A, B)` / `TPair<K, V>(A, B)` / `Emplace(A, B)` are valid either way |
| `UInstancedStaticMeshComponent::SetCustomData(int32, TArrayView<const float>, bool)` | The array-view parameter (5.1+; 5.0 took `const TArray<float>&`). Not reflected | `Private/World/AbyssZoneActor.cpp:883`, `:1964` pass an explicit `TArrayView<const float>`: an error only if 5.8 went back to `TArray` |
| `UStaticMeshComponent::GetStaticMesh()` | Returns `TObjectPtr<UStaticMesh>` (5.x), not a raw pointer | The module assigns it to `UStaticMesh*` / compares it with one, valid for both |
| `TObjectPtr` comparison (`UObject/ObjectPtr.h`) | `operator==(const TObjectPtr<U>&)`, `operator==(nullptr)` and a generic `operator==(const U&)` over what converts to `const T*` or has `Get()` (the engine's `ObjectPtr_Private::CoerceToPointer`); `!=` and the reversed forms come from C++20 | `Ptr == RawPointer` resolves to the member (exact match) instead of being ambiguous with the built-in comparison. A pointer to a BASE of `T` compares through `operator T*()` |
| `FStreamableManager::RequestAsyncLoad` / `RequestSyncLoad` (`Engine/StreamableManager.h`) | The four long-standing overloads (path array or single path; `FStreamableDelegate` or `TFunction<void()>&&`). 5.5+ are believed to fold them into templates with an extra source-location parameter | `Private/World/AbyssAssetLibrary.cpp:480` passes only a moved `TArray<FSoftObjectPath>`: valid for both shapes |
| `Rendering/SkeletalMeshRenderData.h` | Only forward-declares `FSkeletalMeshLODRenderData` (strict side); `LODRenderData` is a `TIndirectArray` with `Num()` / `operator[]` / `IsValidIndex()` and the engine's range-`for` iterators (`TDereferencingIterator`, in `Containers/Array.h`: `T&`, and `const T&` through a const array; negative cases 142 / 143) | The module includes `Rendering/SkeletalMeshLODRenderData.h` itself and only counts and indexes. Mutation (`Add`, `RemoveAt`, `Empty`) is not declared |
| `UMaterialParameterCollectionInstance` (`Materials/MaterialParameterCollectionInstance.h`) | Not exposed to Python. `SetScalarParameterValue(FName, float)` and two overloads of `SetVectorParameterValue`: `(FName, const FLinearColor&)` and, since the final audit, `(FName, const FVector4&)` (believed to exist since the 5.0 large-world change; two reviews remembered it, none could confirm it) | Both module calls (`Private/Vfx/AbyssVfxSystem.cpp:1760`, `Private/World/AbyssWorldBuilder.cpp:2254`) pass a `const FLinearColor&`: an exact match whichever overload set the engine has. If 5.8 only has the `FLinearColor` form, an `FVector4` argument passes here and fails there. A braced list (`{ R, G, B, A }`) picks the `FLinearColor` overload with clang (the four-component `FVector4` constructor is explicit) and may be ambiguous for another compiler: pass a typed `FLinearColor` (negative case 212 pins that other vector types are rejected) |
| `ULightComponent::SetLightFColor(FColor)` | The C++-only twin of the reflected `SetLightColor(FLinearColor, bool)` | `Private/World/AbyssZoneActor.cpp:1415`; `SetLightColor(FLinearColor(Bytes), true)` is the reflected alternative |
| `UActorComponent::SetCanEverAffectNavigation(bool)` | Public, C++-only (the property `bCanEverAffectNavigation` is reflected) | Ten calls in the actors and the zone actor |
| `StaticEnum<T>()` / `StaticStruct<T>()` / `StaticClass<T>()` (`UObject/Class.h`) | Declared without a body, as in the engine (`UObject/ReflectedTypeAccessors.h`); the fake UHT emits the `StaticClass<UFoo>` specialisation per module class | A call with a type UHT did not reflect compiles here and in UE, and fails to LINK in UE: invisible to this check. The module calls none |
| `SNew` / `SAssignNew` allocation | `MakeShared<WidgetType>()` (the engine's `TWidgetAllocator`), so the widget needs an accessible default constructor. `SUserWidget`-derived widgets (allocated by `WidgetType::New()`) are not modelled | `SNew` of an `SUserWidget` subclass with a private constructor is rejected here; the module has none |
| `EAllowShrinking` (`Containers/AllowShrinking.h`) | `enum class EAllowShrinking : uint8 { No, Yes, Default = Yes }` (5.4+); the deprecated `bool` overloads of `RemoveAt` / `RemoveAtSwap` / `Pop` / `SetNum` are not declared | `Array.RemoveAtSwap(I, 1, false)` is rejected here (a deprecation warning in 5.4+); the module passes `EAllowShrinking::No` |
| `FMath` constexpr helpers (`Math/UnrealMathUtility.h`) | `FindDeltaAngleDegrees` / `Radians`, `UnwindDegrees` / `Radians`, `Wrap`, `GridSnap`, `SinCos`, `GetRangePct`, `SmoothStep`, `FInterpTo` have the engine's bodies as remembered (a `constexpr` template cannot be stub-bodied) | Only the signatures decide what compiles; the bodies never run |
| `USoundBase::IsLooping() const` (`Sound/SoundBase.h`) | A public virtual, C++-only (not reflected). `GetDuration()` / `GetMaxDistance()` exist too and are not declared (unused) | `Private/Audio/AbyssAudioSystem.cpp:926` (a Verbose log when the requested loop flag and the asset disagree). If 5.8 has no such public member that one line fails: drop the log, or compare `Sound->GetDuration()` with `INDEFINITELY_LOOPING_DURATION` |
| Public data members of the audio classes (`Sound/SoundBase.h`, `Components/AudioComponent.h`, `Sound/SoundAttenuation.h`) | `USoundBase::Duration`, `UAudioComponent::VolumeMultiplier` / `bIsUISound` / `bAllowSpatialization`, `USoundAttenuation::Attenuation` and the `FSoundAttenuationSettings` flags are written and read directly. The properties and their types are checked against the 5.6 stub; that they are `public:` in C++ is from memory (the engine's own `UGameplayStatics` sets the component's flags the same way) | If one is protected in 5.8, the reflected functions do the same job where they exist: `SetUISound(bool)`, `SetVolumeMultiplier(float)`, `AdjustAttenuation(Settings)`; `bAllowSpatialization` has none (`CreateSound2D` already clears it, so the module's writes after `CreateSound2D` could simply go) |
| Includes of the audio headers | `Components/AudioComponent.h` includes `Sound/SoundAttenuation.h` (it holds an `FSoundAttenuationSettings` by value) and only forward-declares `USoundBase`; the engine's header is believed to include `Sound/SoundWave.h` (and through it `Sound/SoundBase.h`) as well. `Sound/SoundBase.h` and `Sound/SoundAttenuation.h` include `UObject/Object.h`; the base interfaces of `USoundBase` / `UAudioComponent` (`IInterface_AssetUserData`, `ISoundParameterControllerInterface`, `FQuartzTickableObject`) are not modelled | Strict side: using `USoundBase` members needs `Sound/SoundBase.h` here (negative case 153). `AbyssAudioSystem.cpp` includes all three headers itself |
| `FPlatformTime` reachability (`HAL/PlatformTime.h`) | Neither the SDK's `CoreMinimal.h` nor its `Stats/Stats.h` includes `HAL/PlatformTime.h`. The engine's `Stats/Stats.h` (and so every UObject header) is believed to include it | Strict side: a module file that calls `FPlatformTime` without the include fails here and probably not on the Mac. `Private/Audio/AbyssAudioSystem.cpp` and (CC4) `Private/UI/Core/AbyssUiContext.cpp` now include it (as six other module files already did); `Private/UI/Core/AbyssUiContext.cpp:129` is the UI stage's |
| `USkeletalMeshComponent::SetAnimClass` / `SetAnimInstanceClass` (`Components/SkeletalMeshComponent.h`) | Both are reflected (checked against the 5.6 stub); that `SetAnimClass` is the `virtual` one is from memory | Only an override would notice; the module overrides neither |
| `FPostProcessSettings::bOverride_AutoExposureApplyPhysicalCameraExposure` (`Engine/Scene.h`) | Declared `uint32 : 1` while the other `bOverride_` flags are `uint8 : 1`; the engine's exact field type was not confirmed | None on what compiles (the module assigns `true`) |
| Slate widget `FArguments` (`Widgets/Layout/SBox.h`, `SBorder.h`, `SScrollBox.h`, `SSafeZone.h`, `SDPIScaler.h`, `SScaleBox.h`, `Widgets/SCanvas.h`, `Widgets/Text/STextBlock.h`, `Widgets/Input/SButton.h`) | The argument names and types are the engine's `SLATE_ARGUMENT` / `SLATE_ATTRIBUTE` / `SLATE_EVENT` lists as remembered (unchanged since UE 4 for what the module passes). Not reflected. The style defaults (`FCoreStyle` / `FAppStyle` lookups) are replaced by `nullptr`, and `Styling/CoreStyle.h` is not included, so a module file that uses `FCoreStyle` after including only a widget header is rejected here (strict side) | A misnamed or mistyped argument makes the check accept or reject a call wrongly. The arguments the module passes are listed by `tools/sdk_usage.py --only UI --header Slate`; negative cases 158-181 pin their types |
| `SBox::FArguments::Padding` | Only the `TAttribute<FMargin>` form. Convenience overloads `Padding(float)`, `Padding(float, float)`, `Padding(float, float, float, float)` may exist on `SBox` (they do on panel slots, `TPaddingWidgetSlotMixin`); not confirmed, so not declared | `SNew(SBox).Padding(4.f, 2.f)` is rejected here. The module always passes an `FMargin` (valid either way) |
| `SScrollBox::FSlot` | A `TBasicLayoutWidgetSlot` (padding and alignment). The engine has added a size rule to it by 5.6: the 5.6 Python stub's `ScrollBoxSlot` has a `size` property next to padding and alignment, so the Slate slot carries `TResizingWidgetSlotMixin` (believed `AutoSize()` / `FillSize()` / `MinSize()` / `MaxSize()`); the mixin's exact members are not known here, so it is not declared | Strict side: a scroll box slot with a size rule is rejected here and valid on the Mac; the module uses padding and alignment only |
| `SButton::SButton()` | Public (so that `SNew(SButton)`, which is `MakeShared<SButton>()`, compiles) | None for callers |
| `SBox` / `SSafeZone` / `SDPIScaler` base classes | `SBox : SPanel` (no `ChildSlot`, no `SetColorAndOpacity`; `SetContent(const TSharedRef<SWidget>&)`), `SSafeZone : SBox`, `SDPIScaler : SPanel`, `SScaleBox` / `SBorder` / `SScrollBox : SCompoundWidget`, `SButton : SBorder`, `STextBlock : SLeafWidget` | The module only derives from `SCompoundWidget` and `SLeafWidget`; a wrong base here would only matter for members reached through it (negative case 168) |
| Widget style structs (`Styling/SlateTypes.h`: `FButtonStyle`, `FScrollBarStyle`, `FScrollBoxStyle`, `FTextBlockStyle`) | Properties, their types and the base `FSlateWidgetStyle` are checked against the 5.6 stub. The chaining `Set*` builders and the static `GetDefault()` are C++-only: names from memory (`SetNormal`, `SetHovered`, `SetPressed`, `SetDisabled`, `SetNormalPadding`, `SetPressedPadding`; `SetHorizontalBackgroundImage`, `SetVerticalBackgroundImage`, `Set{Vertical,Horizontal}{Top,Bottom}SlotImage`, `SetNormalThumbImage`, `SetHoveredThumbImage`, `SetDraggedThumbImage`) | `Private/UI/Core/AbyssUiStyle.cpp:271-298`. If a builder is missing in 5.8, assign the property instead (`Style.Normal = Brush;`): the properties are the verified part |
| `EAllowOverscroll` (`Framework/Layout/Overscroll.h`), `FOnClicked` (`Framework/SlateDelegates.h`), `EConsumeMouseWheel` (`Styling/SlateTypes.h`, reflected) | The headers these live in; `SScrollBox.h` / `SButton.h` include them. `SScrollBox`'s `OnUserScrolled` event and `ExternalScrollbar` need `Widgets/Layout/SScrollBar.h`, which is not in the SDK: the event is not declared, `SScrollBar` only forward-declared | The module reaches the three through the widget headers and binds no scroll event |
| `TextureResource.h` / `Serialization/BulkData.h` | `FTexturePlatformData::Mips` is a `TIndirectArray<FTexture2DMipMap>`, `FTexture2DMipMap::BulkData` an `FByteBulkData` with `void* Lock(uint32 LockFlags)` / `void Unlock() const` and `LOCK_READ_WRITE`; `UTexture2D::GetPlatformData()` (5.0+) returns `FTexturePlatformData*` and `Engine/Texture2D.h` only forward-declares the struct (strict side) | `Private/UI/AbyssUiSubsystem.cpp:516-530`, `Private/UI/Hud/AbyssMinimapTexture.cpp:159-162`: the classic transient-texture fill; both files include `TextureResource.h` |
| `SceneView.h` | `FSceneViewProjectionData` with public `ViewOrigin` / `ViewRotationMatrix` / `ProjectionMatrix`, protected view rectangles behind `GetViewRect()` / `GetConstrainedViewRect()`, and `ComputeViewProjectionMatrix()`; `static bool FSceneView::ProjectWorldToScreen(const FVector&, const FIntRect&, const FMatrix&, FVector2D&, bool = false)`; `ULocalPlayer::GetProjectionData(FViewport*, FSceneViewProjectionData&, int32 = INDEX_NONE) const` | `Private/UI/World/SAbyssWorldLayer.cpp:845-867`. `APlayerController::ProjectWorldLocationToScreen` (reflected, already used by the picking code) is the alternative if any of the three moved |
| `FSlateApplication::GetDragTriggerDistance()`, `SWidget::GetPrepassLayoutScaleMultiplier()` | Public, const, return `float` | `Private/UI/Widgets/AbyssUiWidgets.cpp:972`, `Private/UI/Panels/SAbyssSkillTreePanel.cpp:224`; `Private/UI/Hud/SAbyssHudWidgets.cpp:156` |
| `RequiredArgs` (`Widgets/DeclarativeSyntaxSupport.h`) | The positional `SNew` arguments are perfectly forwarded to `Construct` (the engine's `T1RequiredArgs`...: `Forward<Arg0Type>(Arg0)`), so a temporary binds to a `const T&` or `T&&` parameter and an lvalue to `T&` / `const T&` | CC1's version handed every argument on as an lvalue-of-rvalue-reference and rejected `SNew(SWidget, Ptr.ToSharedRef())` with a `const TSharedRef<T>&` parameter: a false error, fixed in CC4 (control case in `NegativeUiCases`, cases 176 / 177) |
| `SBox::ComputeDesiredSize` (`Widgets/Layout/SBox.h`) | `protected` (CC4's SDK had it public; the CC4 gate note). Negative case 182; an `SBox` subclass overriding it is in the control case | If it is public in 5.8, an outside call is rejected here although valid; outside code asks `GetDesiredSize()` in every version. The module makes no such call |
| `STextBlock::ComputeDesiredSize` (`Widgets/Text/STextBlock.h`) | Declared `private` in the SDK. The engine is believed to have it in the public `// SWidget interface` block (low confidence) | Strict side: `TextBlock->ComputeDesiredSize(1.f)` is rejected here and may be valid on the Mac. The module measures text through `FSlateFontMeasure` (`FAbyssPainter::Measure`) and makes no such call |
| `TWidgetSlotWithAttributeSupport<SlotType>` (`Layout/WidgetSlotWithAttributeSupport.h`, new in CC5) | The base of `TBasicLayoutWidgetSlot` (so of box panel, overlay and scroll box slots) and of `SCanvas::FSlot`; `SConstraintCanvas::FSlot` stays a plain `TSlotBase`. Header path and hierarchy are from memory; the engine's second base `SlateAttributePrivate::ISlateAttributeContainer` and the `TSlateSlotAttribute` member template are not modelled | Only code that names the slot bases notices (negative case 183 and its twin); the module never does |
| `SBoxPanel` slot size rules (`Widgets/SBoxPanel.h`) | `AutoWidth` / `FillWidth` / `MaxWidth` and the vertical twins are declared. `MinWidth` / `MinHeight` and `FillContentWidth` / `FillContentHeight` (newer 5.x additions; CC1 had declared them) were removed in CC5: unused by the module, and the release that added each is not known here | Strict side: a slot with `.MinWidth(...)` is rejected here; valid on a recent engine |
| `FMath::RoundToInt(double)` and friends (`Math/UnrealMathUtility.h`) | `RoundToInt` / `FloorToInt` / `CeilToInt` / `TruncToInt` return `int32` for a `float` and `int64` for a `double` (5.0+, the large-world-coordinates change); the `...ToInt32` forms return `int32` for both | Decides only the opt-in width lint (`--define UE_STUB_STRICT_FORMAT_WIDTH=1`): the engine's format check wants an integer under `%d` and, as far as remembered, does not look at its size. CC5 moved the three `%d` call sites that passed `RoundToInt(double)` to `RoundToInt32` (`Private/Actors/AbyssCorpses.cpp:813`, `Private/UI/Panels/SAbyssSkillTreePanel.cpp:552`, `:907`), valid whichever way the engine checks |
| `FDateTime` arithmetic (`Misc/DateTime.h`) | `FDateTime + FTimespan`, `FDateTime - FDateTime` (a time span), `ToString(const TCHAR*)`, `FromUnixTimestamp(int64)`, `Now()` / `UtcNow()`; an `operator+(const FDateTime&)` is declared too (believed to exist; unused) | `Private/UI/Menu/SAbyssMainMenu.cpp:117-119` (the save slot's local time) uses the first group only |
| SDK include edges the Panels / Menu / Story pass rests on | `tools/include_audit.py` lists them (83 edges; `cc5_include_edges.txt` of the CC5 run). All are edges the engine headers are believed to have (`CoreMinimal.h` to the Core containers / math / delegates / text; `Widgets/SWidget.h` to `Layout/Geometry.h`, `Input/Reply.h`, `Input/CursorReply.h`, `Types/PaintArgs.h`; `Widgets/DeclarativeSyntaxSupport.h` to `SlotBase.h`, `Misc/Attribute.h`; the widget headers to their slot / style / enum headers) | An edge the engine lacks lets a module file compile here without an include it needs on the Mac. The lowest-confidence one, `Styling/WidgetStyle.h` to `Misc/Attribute.h`, is never the only path (every file that relies on it also includes a widget header) |
| `Widgets/DeclarativeSyntaxSupport.h` and `Widgets/WidgetPixelSnapping.h` (final audit) | `DeclarativeSyntaxSupport.h` only forward-declares `SWidget` (it included `Widgets/SWidget.h` before, an edge the engine is believed not to have); `EWidgetPixelSnapping` lives in its own header (the 5.6 stub names the file `WidgetPixelSnapping.h`, module SlateCore; the `Widgets/` directory is from memory). The audit also removed three other doubted edges in a scratch SDK (`Engine/EngineBaseTypes.h` to `Misc/CoreDelegates.h`, `UObject/Class.h` to `Math/RandomStream.h` and `HAL/CriticalSection.h`, `Engine/World.h` to `Engine/GameInstance.h`) and the module still passed; those three are still in the SDK | Strict side now: a file that includes only `DeclarativeSyntaxSupport.h` and uses `SWidget` members is rejected (`tests/negative_boundaries.cpp` case 1). Every module file that includes it also includes `SCompoundWidget.h` / `SLeafWidget.h` / `SPanel.h` or a module widget header |
| `TWeakObjectPtr::operator bool` (`UObject/WeakObjectPtrTemplates.h`, final audit) | `explicit operator bool() const = delete;` (the engine deliberately has no boolean conversion). The SDK had a working one before | `if (Weak)` / `!Weak` are rejected (negative case 196); `IsValid()` / `Get()` are valid in every version. The module used neither form |
| Weak lambdas (`Delegates/Delegate.h`, final audit) | `CreateWeakLambda` / `BindWeakLambda` / `AddWeakLambda` take a UObject owner only (the instance holds a `TWeakObjectPtr<UserClass>`); a `TSharedFromThis` object uses the `SPLambda` forms. The SDK accepted both before | A widget passed to `AddWeakLambda` is rejected (negative cases 197-199, 211). The module's one use (`Private/Framework/AbyssPlayerController.cpp:37`) passes a UObject |
| `FTCHARToUTF8` / `FUTF8ToTCHAR` (`Containers/StringConv.h`, final audit) | `FTCHARToUTF8::Get()` is a `const ANSICHAR*`; `FUTF8ToTCHAR` is built from `const ANSICHAR*` (the converter's `LegacyFromType`) or `const UTF8CHAR*` (its `FromType`). The SDK had `UTF8CHAR` on both before, which rejected the usual `std::string(Conv.Get(), Conv.Length())` and accepted `const UTF8CHAR* P = FTCHARToUTF8(...).Get()` | Negative cases 204 / 205 and their twins. The module uses `StringCast<UTF8CHAR>`, `FString::ConstructFromPtrSize` and the `UTF8_TO_TCHAR` macro, none of the two classes |
| `TObjectPtr` construction (`UObject/ObjectPtr.h`, final audit) | The constructor from `T&` is `explicit`; `TObjectPtr<T>` asserts that a COMPLETE `T` derives from `UObject` (the engine's `ResolveTypeIsComplete` probe; a forward-declared `T` is fine) | `G(ActorRef)` for a `TObjectPtr<AActor>` parameter and `TObjectPtr<FSomeStruct>` are rejected (negative cases 201 / 202). The module does neither |
| Constructor forms of engine classes (`UObject/ObjectMacros.h`: `UE_STUB_CLASS_BODY` / `UE_STUB_CLASS_BODY_DEFAULT_CTOR`, final audit) | `USubsystem` and its family (`UDynamicSubsystem`, `UEngineSubsystem`, `UGameInstanceSubsystem`, `UWorldSubsystem`, `UTickableWorldSubsystem`, `ULocalPlayerSubsystem`, the two Enhanced Input subsystems) only have a default constructor; every other SDK class keeps both forms (`Super()` and `Super(ObjectInitializer)`), which is right for `UObject`, `AActor` and the classes with the usual `(const FObjectInitializer& = FObjectInitializer::Get())` constructor, and is NOT known for each remaining class | `Super(ObjectInitializer)` in a subsystem is rejected (negative case 200); for another engine class that only has a default constructor it would still pass here. No module file mentions `FObjectInitializer`; the fake UHT generates the constructor form of the base class |
| `FSlateColor` constructors (`Styling/SlateColor.h`, final audit) | Default, `const FLinearColor&`, `const TSharedRef<FLinearColor>&`. The `FColor` constructor the SDK had is gone (not believed to exist: an `FColor` argument then needs two user conversions); the engine's `FSlateColor(EStyleColor)` is not declared (`Styling/StyleColors.h` is not in the SDK) | `FSlateColor C = FColor::Red;` and passing an `FColor` for an `FSlateColor` parameter are rejected (negative case 203); `FSlateColor(FColor::Red)` and `.ColorAndOpacity(FColor::White)` (a `TAttribute`) compile either way. An `EStyleColor` argument is rejected here although valid (strict side). The module passes `FLinearColor` |
| The bridge structs of `Types/SlateVector2.h` (final audit) | `FDeprecateSlateVector2D` is a global-scope struct derived from `FVector2f` (checked against the 5.6 stub: name, base, header; a USTRUCT is never in a namespace). The optional variant behind `UE::Slate::FDeprecateOptionalVector2DParameter` is not reflected: that it is a global struct named `FDeprecateSlateOptionalVector2D` is from memory | Only code that names the structs notices (negative case 206); the module names neither, and Slate's signatures use the `UE::Slate` aliases |
| `FSlateApplicationBase::GetRenderer()`, `FSlateApplication::GetInteractiveTopLevelWindows()` (final audit) | One `FSlateRenderer* GetRenderer() const` (the SDK had a const / non-const pair that handed a `const FSlateRenderer*` out of a const application); the window list is returned by value | Only callers through a const application or binding the list to a non-const reference notice. `Private/UI/Core/AbyssUiDraw.cpp` uses the renderer through the non-const singleton |
| `TBitArray` / `TSparseArray` (final audit) | Not in the SDK, although the engine's `CoreMinimal.h` includes `Containers/BitArray.h` and `Containers/SparseArray.h` | Strict side: a first use in the module is reported as "no template named 'TBitArray'": add the two headers then, with their real members. The module uses neither |
| `[[nodiscard]]` (final audit: `Math/UnrealMathUtility.h`, `Math/Vector.h`, `Vector2D.h`, `Vector4.h`, `Rotator.h`, `Quat.h`, `Containers/Array.h`, `Map.h`, `Set.h`) | Every value-returning function of `FMath` / `FGenericPlatformMath`, the value-returning const / static members of the vector, rotator and quaternion types, and the queries of the containers (`Num`, `IsEmpty`, `Contains`, `Find`, `FindByKey`, `FindByPredicate`, `IndexOfByKey`, `FilterByPredicate`, `Last`, `Top`, `GetData`, ...; not `Add`, `Emplace`, `AddUnique`, `Remove*`, `Pop`, `FindOrAdd`, `Normalize`) carry the attribute, as the engine's do since 5.1 (`UE_NODISCARD`). That each single one carries it in 5.8 is from memory | Strict side where wrong: a discarded result is `-Werror,-Wunused-result` here (negative cases 207-210) and would only be dead code in the engine; write `(void)` or use the value. The module discards none (a libclang scan of the audit, and every run since) |
| UE clang warning set in `tools/compile_check.py` | `-Wall -Werror` minus UE's `-Wno-*` list, `-Wshadow` and `-Wundef` as errors | A warning UE disables could fail here (strict side) or the reverse |
| `FSceneView::ProjectWorldToScreen` with one `ULocalPlayer::GetProjectionData` per pick (`Private/Input/AbyssPicking.cpp`, audit F4) | The same screen point as `APlayerController::ProjectWorldLocationToScreen(World, Out, false)`, which is believed to do exactly this per call and then run `PostProcessWorldToScreen` (a no-op outside split screen). The declarations compiled on 5.8.3 already (`SAbyssWorldLayer.cpp` uses the same pattern); the equivalence is from memory | A pick rectangle offset from before by whatever the controller's post-process adds. Mac check: hover and click a monster at each screen edge |
| `USkinnedMeshComponent::SetForcedLOD` on a husk whose component tick is paused (`AAbyssCharacterActor::SetForcedBodyLod`, audit F8) | Reflected (5.6 stub). The LOD is believed to be chosen, and its bones refreshed, in the component's tick, so the body ticks again for a few drawn frames; the held pose is believed to re-evaluate at the new LOD | A settled husk snaps to its reference pose or keeps LOD0. Mac check: playtest P-list husks at maximum zoom after the crumble |
| Slate ticks a widget from its paint, after the window's layout prepass (`SAbyssCombatLog::Tick` rebuilding its child tree, audit F3 / S-A review R1) | From memory (`SWidget::Paint` calls `Tick`; `SWindow`'s prepass runs before the paint). The code does not depend on it: `Rebuild` calls `SlatePrepass(GetPrepassLayoutScaleMultiplier())` on the tree it built before putting it into `ChildSlot`, which is right in either order (one extra prepass of about ten widgets per rebuild if the engine prepasses after tick) | If the prepass in `Rebuild` were wrong or missing: the log's header and lines blank for one frame per logged line. Mac check: step frame by frame through a fight |
| `UDirectionalLightComponent::SetShadowDistanceFadeoutFraction` (`AAbyssZoneActor::ApplyQuality`, audit F1) | Reflected: a UFUNCTION with that name and a float in the 5.6 stub (`check_reflected.py` verifies it on every run); called directly, not hedged | 5.8 without it: `AbyssZoneActor.cpp` fails to compile in every target; set the `ShadowDistanceFadeoutFraction` property and `MarkRenderStateDirty()` instead |
| `FApp::GetIdleTime()` (autotest frame samples, `AbyssAutoTest_IdleMs`) | Seconds the game thread slept in this frame for the frame-rate cap (`t.MaxFPS`), set in `UEngine::UpdateTimeAndHandleMaxTickRate`; long-standing static of `FApp`. Called through a hedge (`requires { AppType::GetIdleTime(); }`) | Missing: `idleMs` is null in `report.json` / empty in `frames.csv`, and the run's first log line says so. A different meaning (it also counts the vsync wait, or excludes the "overshoot") only shifts how much of a frame is reported as idle |
| `FGenericPlatformMisc::GetPrimaryGPUBrand()` / `GetCPUBrand()` (autotest `environment.gpu` / `.cpu`) | Static `FString` getters on `FPlatformMisc`; hedged the same way. Used instead of `GRHIAdapterName` (a global, in some releases a macro over `GRHIGlobals`: not hedgeable) | Missing: an empty string in the report |
| `GEngine->Exec(World, TEXT("stat unit"))` and `FCommonViewportClient::IsStatEnabled(TEXT("Unit"))` (autotest `-abyssstatunit`; the fallback since `UEngine::SetEngineStat` is tried first, entry 9) | The command turns the engine stat on for the game viewport (the form `AbyssPlatform.cpp` already uses for `DisableAllScreenMessages`, which compiled on 5.8.3). It toggles, so the autotest sends it once per run and not at all when `IsStatEnabled` (hedged with `requires`, declared in the `// UNVERIFIED` block of entry 9) says the stat already shows | Not handled: the run logs "stat unit not handled by this build" and the thread times stay null. No `IsStatEnabled` on this engine, or another stat name: a stat that an operator or a config had already turned on is toggled off by the autotest (thread times null, `framesWithThreadTimes` 0): do not combine the two |
| `FViewport::GetSizeXY()` / `IsFullscreen()`, `UGameUserSettings::GetFullscreenMode()` / `IsVSyncEnabled()` / `GetScreenResolution()` (autotest `environment`) | The viewport's size in pixels and its fullscreen flag from source knowledge; the three settings getters are reflected (5.6 stub). Called directly | A wrong value in the report's environment block only |
| Console variable names read into `environment.consoleVariables` (`GAbyssAutoTestConsoleVariables`: `r.SecondaryScreenPercentage.GameViewport`, `r.TSR.History.ScreenPercentage`, `r.PSOPrecaching`, the twelve `sg.*` groups, ...) | Names from memory (the audit rates `r.SecondaryScreenPercentage.GameViewport` at about 85 %). Looked up with `IConsoleManager::FindConsoleVariable`, never written | A name this engine lacks is listed under `environment.consoleVariablesMissing` instead of being reported with a value: that list is the answer to "does 5.8 have it" |
| Order of `FTSTicker::GetCoreTicker()` in the engine frame (autotest frame boundary, `FAbyssAutoTestRun::Tick`) | Ticked once per `FEngineLoop::Tick`, after the world tick and the Slate tick of the frame. A sample's `frameMs` is the time between two such ticks, so it is exact wherever the ticker sits; the tags and the driver's numbers of "this frame" assume the world tick came before it | Ticker before the world tick: every tag, step count and hook counter is attributed one frame late (the sums and percentiles do not change; `longestFrames` would show a zone build on the frame after the long one) |

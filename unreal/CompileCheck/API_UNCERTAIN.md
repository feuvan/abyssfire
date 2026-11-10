# Engine API that could not be confirmed for UE 5.8

The compile check type-checks the module against a stub SDK. Reflected API is cross-checked against the engine's Python
stub (UE 5.6); everything else comes from UE 5.x source knowledge. This file lists the places where it could not be
told whether UE 5.8 has an API with the signature the module (or the SDK) assumes. Each entry says what the code
assumes, the evidence both ways, what the SDK declares, and the safest alternative. An entry is closed either by a real
UE 5.8 build or by moving the module to an API that is not in doubt.

Verdict vocabulary: see README.md ("The three verdicts").

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

### 3. `FCoreDelegates::GetApplicationWillTerminateDelegate()`

- **Where**: `Source/Abyssfire/Private/Framework/AbyssGameInstance.cpp:65` (`AddUObject`) and `:82` (`RemoveAll`).
- **What the code assumes**: a static accessor `FCoreDelegates::GetApplicationWillTerminateDelegate()` returning a
  reference to a multicast delegate without parameters that has `AddUObject(Object, &Class::Method)` and
  `RemoveAll(Object)`.
- **Evidence for**: in UE 5.x the lifecycle delegate is reached through this accessor in engine code, and direct use of
  the static member `ApplicationWillTerminateDelegate` is deprecated in its favour (the module's own README lists the
  member as deprecated for that reason). The delegate type is thread-safe (`TTSMulticastDelegate<void()>`), which has
  both members the module calls.
- **Evidence against**: none found, but nothing here can confirm it either: `FCoreDelegates` is not reflected (the
  5.6 Python stub has no such class; its `application_will_terminate_delegate` hits belong to `UApplicationLifecycleComponent`, a Blueprint-facing
  component), there is no engine source in the container and the API reference could not be opened. The release in
  which the accessor appeared and the exact return type (`FApplicationLifetimeDelegate&`, as declared, or a
  differently named thread-safe delegate type) are from memory.
- **What the SDK declares**: both forms in `sdk/Runtime/Core/Public/Misc/CoreDelegates.h`: the static member, and the
  accessor inside a block marked `// UNVERIFIED`. The accessor is on by default because the module calls it; the
  marker is what records that a PASS of `AbyssGameInstance.cpp` rests on it.
- **Residual risk**: one line each way. If 5.8 lacks the accessor, both calls fail to compile in every target; if it
  has the accessor with another delegate type, they still compile as long as that type has `AddUObject` / `RemoveAll`.
- **Safest alternative**: `FCoreDelegates::ApplicationWillTerminateDelegate.AddUObject(...)` / `.RemoveAll(this)`, the
  long-standing static member (UE 4 through 5.x). It was not chosen because it is the form believed deprecated: it
  would trade a possible error for a likely deprecation warning, and for an error once the member is removed. The
  module does not depend on the callback for correctness (the save that counts runs on
  `ApplicationWillEnterBackgroundDelegate`; terminate may never fire on mobile), so if both forms fail on the Mac the
  two lines can be dropped without losing a save.

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

## SDK declarations made at lower confidence

These are not used in a way that decides a verdict today, but later stages should know they are from memory. None is
reflected in a form the Python stub exposes.

| SDK declaration | Assumption | If wrong |
|---|---|---|
| `Misc/VarArgs.h` format-string checker | Reimplements `UE::Core::Private::FormatStringSan`: `%s` needs a `TCHAR*`, `%d` an integer or enum, `%f` a float / double; too few or too many arguments are errors | A stricter or looser real checker; the module only uses plain `%s %d %f %lld %u %x` forms |
| `sdk/modules.json` | PUBLIC dependency lists of the engine modules (`Engine` re-exports `Slate`, `SlateCore`, `InputCore`, `RenderCore`, `RHI`, `PhysicsCore`, `AudioExtensions`, `DeveloperSettings`; `RHI` re-exports `ApplicationCore`) | A header could be reachable here and not in UBT. In the areas checked so far the module includes only from modules that `Abyssfire.Build.cs` lists itself |
| `UObject/Class.h` includes `Math/RandomStream.h`, `Misc/Guid.h`, `HAL/CriticalSection.h` | The real header's include list | `Public/Camera/AbyssCameraRig.h` now includes `Math/RandomStream.h` itself, so nothing depends on it |
| `UEnhancedInputComponent::BindAction(..., Func, VarTypes... Vars)` | Payload arguments after the member function (the form Lyra uses) | `Private/Input/AbyssPlayerControllerInput.cpp:157` binds with an `ActionId` payload |
| `EMaterialParameterType::TextureCollection` | Present between `Texture` and `Font` (the 5.6 stub has texture-collection parameter getters) | Enumerator order only; the module uses `EMaterialParameterType::Texture` |
| `SConstraintCanvas::FSlot::ZOrder` | `SLATE_ATTRIBUTE(float, ZOrder)` | Matters to the UI stage if a caller passes something other than a number |
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
| `Rendering/SkeletalMeshRenderData.h` | Only forward-declares `FSkeletalMeshLODRenderData` (strict side); `LODRenderData` is a `TIndirectArray` with `Num()` / `operator[]` / `IsValidIndex()` only | The module includes `Rendering/SkeletalMeshLODRenderData.h` itself and only counts and indexes. A range-`for` over `LODRenderData` is rejected here although the engine allows it |
| `UMaterialParameterCollectionInstance` (`Materials/MaterialParameterCollectionInstance.h`) | Not exposed to Python. `SetScalarParameterValue(FName, float)` and `SetVectorParameterValue(FName, const FLinearColor&)` only; an `FVector4` overload may exist | The module passes `float` and `FLinearColor` |
| `ULightComponent::SetLightFColor(FColor)` | The C++-only twin of the reflected `SetLightColor(FLinearColor, bool)` | `Private/World/AbyssZoneActor.cpp:1415`; `SetLightColor(FLinearColor(Bytes), true)` is the reflected alternative |
| `UActorComponent::SetCanEverAffectNavigation(bool)` | Public, C++-only (the property `bCanEverAffectNavigation` is reflected) | Ten calls in the actors and the zone actor |
| `StaticEnum<T>()` / `StaticStruct<T>()` / `StaticClass<T>()` (`UObject/Class.h`) | Declared without a body, as in the engine (`UObject/ReflectedTypeAccessors.h`); the fake UHT emits the `StaticClass<UFoo>` specialisation per module class | A call with a type UHT did not reflect compiles here and in UE, and fails to LINK in UE: invisible to this check. The module calls none |
| `SNew` / `SAssignNew` allocation | `MakeShared<WidgetType>()` (the engine's `TWidgetAllocator`), so the widget needs an accessible default constructor. `SUserWidget`-derived widgets (allocated by `WidgetType::New()`) are not modelled | `SNew` of an `SUserWidget` subclass with a private constructor is rejected here; the module has none |
| `EAllowShrinking` (`Containers/AllowShrinking.h`) | `enum class EAllowShrinking : uint8 { No, Yes, Default = Yes }` (5.4+); the deprecated `bool` overloads of `RemoveAt` / `RemoveAtSwap` / `Pop` / `SetNum` are not declared | `Array.RemoveAtSwap(I, 1, false)` is rejected here (a deprecation warning in 5.4+); the module passes `EAllowShrinking::No` |
| `FMath` constexpr helpers (`Math/UnrealMathUtility.h`) | `FindDeltaAngleDegrees` / `Radians`, `UnwindDegrees` / `Radians`, `Wrap`, `GridSnap`, `SinCos`, `GetRangePct`, `SmoothStep`, `FInterpTo` have the engine's bodies as remembered (a `constexpr` template cannot be stub-bodied) | Only the signatures decide what compiles; the bodies never run |
| UE clang warning set in `tools/compile_check.py` | `-Wall -Werror` minus UE's `-Wno-*` list, `-Wshadow` and `-Wundef` as errors | A warning UE disables could fail here (strict side) or the reverse |

# CompileCheck: an engine-free compile check of `Source/Abyssfire`

The UE 5.8 presentation module (`unreal/Source/Abyssfire`) cannot be built where the core is developed: there is no
engine in the container. This directory makes a clang front end do the part of a UE build that needs no engine binary:
every `.cpp` of the module is parsed and type-checked, one translation unit per file, against a declaration-only copy
of the engine API, with the command line UnrealBuildTool would use.

```
unreal/CompileCheck/
  run.sh                  the entry point (wraps tools/compile_check.py)
  sdk/                    the stub SDK: engine headers at their real include paths, one tree per engine module
  sdk/modules.json        engine modules: include roots and PUBLIC dependencies (as in each module's .Build.cs)
  uht/fake_uht.py         fake UnrealHeaderTool: writes <Header>.generated.h / .gen.cpp and lints the UHT rules
  tools/compile_check.py  the driver: configurations, PCH, parallel compiles, report
  tools/check_reflected.py  cross-checks the SDK's reflected API against the engine's Python stub (UE 5.6)
  tools/stub_bodies.py    keeps SDK templates usable with lambdas / local types, and constrained (see "Adding to the SDK")
  tools/sdk_usage.py      lists the SDK declarations a set of module files uses: what their PASS rests on
  tools/selftest.py       plants faults in a scratch copy of the module and runs tests/negative_cases.cpp
  tests/negative_cases.cpp  code UE rejects: every case must be rejected by the SDK too
  API_UNCERTAIN.md        engine API the module uses that could not be confirmed for 5.8
  build/                  output (git-ignored); override with --build-dir or ABYSS_CC_BUILD
```

## Running it

```
unreal/CompileCheck/run.sh                       # everything, 4 configurations, 3 jobs; exit 0 only on a full pass
unreal/CompileCheck/run.sh --only Framework      # one area: Private/Framework + Public/Framework (and their .gen.cpp)
unreal/CompileCheck/run.sh --only Private/Input/Touch --only AbyssText.cpp --config mac-editor
unreal/CompileCheck/run.sh --selftest            # prove the check bites (planted faults + negative cases)
unreal/CompileCheck/run.sh --list                # the translation units
unreal/CompileCheck/run.sh --notes               # compiler notes under each diagnostic
python3 unreal/CompileCheck/tools/check_reflected.py --show PlayerController --grep touch   # look a class up in the 5.6 stub
python3 unreal/CompileCheck/tools/check_reflected.py --find spawn_sound                     # search reflected functions
python3 unreal/CompileCheck/tools/sdk_usage.py --only World --baseline Framework --uses     # SDK declarations World adds
```

Needs `clang++` (18 used here; `--cxx` / `ABYSS_CC_CXX` to choose) and `python3`. Results are cached per translation
unit (dependency files), so a re-run after an edit only recompiles what the edit reaches; `--force` ignores the cache.
A result that stopped at a missing header is never reused (the dependency file cannot name a header that did not exist
yet), so a file is compiled again as soon as the SDK gains the header.
Output: per-configuration pass counts, the diagnostics merged across configurations (errors, warnings UE turns into
errors, `-Wshadow` findings, plain warnings), a summary table by area, and `build/report.json`,
`build/uht_report.json`, `build/compile_commands.json`.

The engine Python stub is found at `unreal/Scripts/.cache/unreal_stub/unreal.py` (in a git worktree: the main
checkout's copy; `ABYSS_UNREAL_STUB` overrides). Without it the reflected cross-check and the UHT type lint are skipped.

## What a run does

1. `tools/stub_bodies.py --check`: the SDK itself is well-formed for lambdas / local types, and no stub-bodied template
   has an explicit type parameter that nothing constrains.
2. `tools/check_reflected.py`: every class / struct / enum / UFUNCTION / exposed UPROPERTY the SDK marks as reflected
   exists in the engine's Python stub with that base class, engine module and header file name; each UFUNCTION with
   those parameter names, parameter types, default arguments and return type; each property with that type (`int32`
   against `int64`, `float` against `double`, a bitfield as `bool`); each enum with those enumerators. A default
   argument the engine does not have is an error (it makes a call with too few arguments compile); the other
   mismatches are warnings to resolve.
3. `uht/fake_uht.py`: generated headers for the 25 reflected module headers, plus the UHT rule lint.
4. For each configuration: a real clang PCH of what UBT force-includes (`Definitions.Abyssfire.h` +
   `Private/AbyssfirePCH.h`), then `clang++ -fsyntax-only -std=c++20` over every module `.cpp` and every `.gen.cpp`,
   one translation unit each, and last the **unity build**: every module `.cpp` that passed, `#include`d in path order
   into ONE more translation unit (`--no-unity` skips it).

Configurations (the module's `#if`s are `WITH_EDITOR`, `UE_BUILD_SHIPPING`, `PLATFORM_ANDROID || PLATFORM_IOS`,
`PLATFORM_IOS`):

| name | platform | target | notable defines |
|---|---|---|---|
| `mac-editor` | Mac | AbyssfireEditor Development, modular | `WITH_EDITOR=1`, `WITH_EDITORONLY_DATA=1`, `ABYSS_CORE_DLL=1` |
| `mac-game` | Mac | Abyssfire Development, monolithic | `WITH_EDITOR=0` |
| `android-game` | Android | Abyssfire Development, monolithic | `PLATFORM_ANDROID=1` |
| `ios-shipping` | iOS | Abyssfire Shipping, monolithic | `PLATFORM_IOS=1`, `UE_BUILD_SHIPPING=1` (no logging, no checks, no stats) |

What is emulated from UBT: include roots are the module's `Public` and `Private` (not the module directory), the UHT
output directory, `AbyssCore/Public`, and the SDK roots of the modules `Abyssfire.Build.cs` lists plus, transitively,
their public dependencies (`sdk/modules.json`), so a header of a module that is not a dependency is not found; the PCH
is force-included; `-fno-exceptions -fno-rtti`; UE's clang warning set (`-Wall -Werror` minus UE's `-Wno-*` list, plus
`-Wshadow` and `-Wundef` as errors, deprecations as warnings). There is no `-fpermissive`, `-w` or blanket suppression.
One translation unit per file means a non-unity build: a file that only compiles because a unity blob happens to
include a header for it fails here (UBT compiles files in your working set non-unity, so that is a real failure mode).
The opposite failure is covered by the unity build unit: UBT builds a game module in unity blobs by default, where two
files that each compile alone can collide (the same helper name in two anonymous namespaces, a file-scope `static`, a
macro left defined). All passing files in one translation unit is the superset of whatever blobs UBT forms.

## What a PASS proves, and what it cannot

Proves, for every file in every configuration: the preprocessor paths are well-formed; every include resolves within
the module's dependency closure; every name, member, overload and conversion type-checks against the declared engine
API; `.h` and `.cpp` agree; `override`s override; abstract classes are not instantiated; format strings of `UE_LOG` /
`FString::Printf` / `checkf` match their arguments; delegates bind functions of the right signature; Slate declarative
syntax names real arguments; reflected headers are self-sufficient and follow the UHT rules that are linted.

Cannot prove:
- **Linking.** Nothing is linked: a missing definition, a missing `*_API` export or a missing module in
  `Abyssfire.Build.cs` that only shows at link time is invisible.
- **Real UHT.** The fake UHT is a scanner for the declaration shapes this module uses. It does not validate metadata
  values, Blueprint exposure rules, replication, or anything it does not parse (it reports `uht-parse` instead).
- **Fidelity of the SDK.** Reflected API is checked against the engine's own Python stub, but that stub is UE 5.6 and
  the project targets 5.8. Everything else (templates, Slate, Core, non-reflected members) is written from UE 5.x
  source knowledge: a wrong declaration there makes the check pass or fail wrongly. Declarations are deliberately on
  the strict side where unsure (a missing transitive include in the SDK costs one explicit `#include` in the module).
  Known approximations are listed in `API_UNCERTAIN.md`.
- **Behaviour.** No code runs. Engine semantics (GC, threading, timing, rendering) are untested; that is what
  `PresentationTests` (pure rules) and the Mac playtest are for.
- **Other toolchains.** Apple clang / the Android NDK / MSVC differ from clang 18 in details; platform SDK headers
  (Objective-C on iOS, JNI on Android) are not modelled.

## The three verdicts

Every diagnostic gets one of three verdicts; none is silenced.

- **STUB GAP**: the SDK lacks or misdeclares a real engine API. Add the faithful declaration at the real include path,
  with the real name and signature (see below). Never add a catch-all, never declare a member on a base class when the
  engine has it on a derived one, never widen a parameter type to make an error go away.
- **MODULE BUG**: the module is wrong (wrong argument type, missing include, signature mismatch, API that does not
  exist). Fix the module minimally, keep behaviour, and note file:line and why a real build would fail.
- **API-UNCERTAIN**: it cannot be told whether UE 5.8 has this API with this signature. Do not quietly stub it: add an
  entry to `API_UNCERTAIN.md` (file:line, what the code assumes, evidence both ways, the safest well-known
  alternative), declare it in the SDK inside a block marked `// UNVERIFIED` (off by default behind a
  `UE_STUB_UNVERIFIED_*` macro when it is an alternative to a known form; `--define NAME=1` type-checks against it),
  and prefer changing the module to a well-known API. When the module hedges the call itself
  (`if constexpr (requires ...)`), guard the block the other way round,
  `#if !(defined(UE_STUB_UNVERIFIED_NO_X) && UE_STUB_UNVERIFIED_NO_X)`: on by default, and `--define` type-checks the
  module's fallback for an engine without the API. Every run ends its summary with the list of `// UNVERIFIED`
  blocks and whether each was compiled (`report.json`: `unverified`): a PASS rests on the ones that are on.

## Adding to the SDK

- **A header** goes where the engine has it: `sdk/<module root>/<include path>` (`Engine` has two roots, `Classes`
  and `Public`; plugins live under `sdk/Plugins/...`). Start it with the provenance comment
  `// Stub SDK. Real header: Engine/Source/...` and say where the content comes from: "reflected: checked against the
  5.6 stub" or "UE 5.x source knowledge". Include what the real header includes (as far as known) and nothing more:
  do not put the world into `CoreMinimal.h`.
- **A module**: add it to `sdk/modules.json` with its include roots and the PUBLIC dependencies of its `.Build.cs`.
  It becomes visible to the module only through `Abyssfire.Build.cs` (directly or as a public dependency).
- **A reflected class** `class UFoo : public UBar { UE_STUB_CLASS_BODY(UFoo, UBar) public: ... };` (structs:
  `UE_STUB_STRUCT_BODY(FFoo)`, native interfaces: `UE_STUB_IINTERFACE_BODY(IFoo, UFoo)`). Mark what is reflected with
  the real `UFUNCTION(...)` / `UPROPERTY(...)` / `UENUM(...)` lines (they expand to nothing): `check_reflected.py`
  then verifies the class, its base, module and header, each Blueprint-exposed function (name, `ScriptName`,
  parameters) and each editor-visible property against the Python stub. Members without a marker are source knowledge
  and unverifiable: keep them to what the module needs. Look a class up first:
  `python3 tools/check_reflected.py --show Foo`. BlueprintNativeEvents also need their `virtual ..._Implementation`.
  Write the engine's `meta = (...)` on the marker when it carries `ScriptName` (an enum or function renamed for
  Python) or a `Param = value` default (a Blueprint default the C++ declaration does not have): the checker reads both.
- **Templates need bodies.** C++ forbids using an undefined function whose type involves a lambda or a local type, and
  module code instantiates engine templates with such types constantly. Every function template and every member of a
  class template therefore ends with `UE_STUB_BODY` instead of `;` (a body that calls a `[[noreturn]]` function; it
  never runs; a trailing `requires requires(...) { ... }` clause keeps its braces, the body comes after it). Write the
  declaration with `;` and run `python3 tools/stub_bodies.py`: it rewrites them (and `run.sh`
  refuses to start while any is missing). It leaves alone `constexpr` functions, deduced return types, pure virtuals,
  members defined out of line, and constructors that need a base / member initialiser must be written by hand
  (`: FBase(nullptr) UE_STUB_BODY`).
- **An explicit type parameter needs its requirement.** `template <class T> T* GetHUD() const UE_STUB_BODY` compiles
  with any `T`, also a forward-declared or unrelated one, while the engine's body (`Cast<T>`, `T::StaticClass()`,
  `static_cast<T*>`) rejects those. `stub_bodies.py --check` (and so every run) fails on a stub-bodied template whose
  own type parameter is not deducible from the arguments and appears in no requires-clause: write the engine's body
  when it is a one-liner (`return Cast<T>(GetHUD());`), or the requires-clause that states what the body needs. A
  deliberate exception carries `// explicit-ok: <why>` on the declaration; a template the engine itself only declares
  (`StaticEnum<T>()`, defined by UHT's specialisations) carries `// no-body: <why>` after its `;` and stays body-less.
  Members of a class template are not seen by this rule (the class parameter is theirs): give those the engine's body
  too when it constrains `T` (`TSubclassOf<T>::Get`, `TSoftObjectPtr<T>::Get`).
- **A template parameter is never a licence.** An engine template that takes "any" predicate, key, functor or
  argument pack compiles in UE only when its body does. The SDK has no bodies, so the declaration must carry what the
  body needs as a `requires` clause (a predicate is `bool(Element)`, a key needs `Element == Key`, a sort predicate goes
  through `TDereferenceWrapper`, `EmplaceValue` needs `is_constructible`, ...), or the engine's real body when that is a
  one-liner. If the body is not known well enough to say what it needs, do not declare the function until the module
  uses it. A parameter pack is only for what is variadic in the engine (`EmplaceValue`, not `SetValue`).
- **Real shapes.** Containers, strings, smart pointers, delegates, `TAttribute`, `SNew` / `SLATE_*` are real templates
  and macros so that a wrong argument type fails; keep it that way, and add a case to `tests/negative_cases.cpp` when
  a new API family arrives (plus its correct twin in `CASE 0`).
- **A conversion is API too.** A constructor that takes one argument is an implicit conversion unless the engine marks
  it `explicit`, and a wrong one makes every call site that passes the wrong type compile. Declare a converting
  constructor only when the engine has it, with the engine's explicitness: `FName` has none from `FString` and only
  explicit ones from string views; `TArray`'s element- / allocator-changing constructors are explicit;
  `FSoftObjectPath` is implicit from `FString` only. Character overloads are over the engine's character types
  (`WIDECHAR` is `TCHAR` on the checked platforms; `wchar_t`, `uint16`, `uint32` are not characters here), never over
  "anything that converts to `TCHAR`". When the explicitness is not known, declare it `explicit` (the strict side) and
  note it in `API_UNCERTAIN.md`.
- **An overload set is API too.** One overload declared where the engine has two rejects valid calls of the other (the
  strict side), but it also ACCEPTS a call the engine finds ambiguous: a braced list, or a type that converts to both
  parameter types. When a second overload is believed to exist and cannot be confirmed, record it in
  `API_UNCERTAIN.md` and check that the module's calls pass the exact parameter type (as for
  `UMaterialParameterCollectionInstance::SetVectorParameterValue`).
- **Keep unverifiable surface small.** A member without a `UFUNCTION` / `UPROPERTY` marker cannot be cross-checked:
  declare it when the module uses it, not because the engine has it. `python3 tools/sdk_usage.py` (libclang through
  ctypes, no package needed) lists the SDK declarations a set of module files really uses, `--baseline` subtracts what
  earlier areas already used: review that list when an area turns green, and move what is doubtful to
  `API_UNCERTAIN.md`.
- **Slate widgets.** A widget header declares the engine's `SLATE_BEGIN_ARGS` list (argument names and types are the
  API a declarative call is checked against), `Construct`, the slot type with its mixins, and only the setters the
  module calls. The `FArguments` constructor keeps the engine's defaults except the style lookups
  (`FCoreStyle::Get().GetWidgetStyle<...>(...)` becomes `nullptr`): `Styling/CoreStyle.h` is not part of the SDK until
  the module itself uses it. A slot gets exactly the engine's mixins (`SCanvas::FSlot`: alignment, no padding;
  `SScrollBox::FSlot`: padding and alignment, no size rule), so a setter borrowed from another panel fails.
- **An engine switch is modelled as a switch.** `Types/SlateVector2.h` has both sides of
  `UE_ENABLE_SLATE_VECTOR_DEPRECATION_MECHANISMS` (the `FVector2D` bridge of Slate's float API). The default is the
  engine's (on); `./run.sh --define UE_ENABLE_SLATE_VECTOR_DEPRECATION_MECHANISMS=0` lists every line of the module
  that relies on the bridge (API_UNCERTAIN.md #8). Results of Slate getters are wrapped explicitly in the module
  (`FVector2D Size(Geometry.GetLocalSize());`): keep new code that way.
- After any SDK change: `./run.sh --selftest` and `python3 tools/check_reflected.py`.

## The fake UHT

`UCLASS(...)` expands to `<CURRENT_FILE_ID>_<line>_PROLOG` and `GENERATED_BODY()` to
`<CURRENT_FILE_ID>_<line>_GENERATED_BODY`, the real mechanism; both are only defined by the generated header, which
also sets `CURRENT_FILE_ID`, so a missing or misplaced `#include "X.generated.h"` fails to compile. The generated body
declares `Super`, `ThisClass`, `StaticClass()`, the deleted copy / move, the `FObjectInitializer` constructor (only when
the class declares none), the virtual destructor (only when the class declares none), `DECLARE_FUNCTION(execFoo)` per
UFUNCTION and ends with `private:`. Each reflected header also gets a `.gen.cpp` that includes the header on its own
after the PCH, default-constructs the class unless it is `Abstract` (so an abstract UCLASS without the specifier
fails), and calls each UFUNCTION as the thunk would.

Lint rules (`error` fails the run, `warn` / `note` are listed): `generated-include` (present and last),
`generated-body`, `prefix` (U / A / F / E / I), `class-base`, `specifier` (known UCLASS / UPROPERTY / UFUNCTION /
USTRUCT / UENUM specifiers; a `BlueprintImplementableEvent` has no body and is not `virtual`), `property-type` (types
UHT can reflect; module types must themselves be reflected; engine types are looked up in the Python stub; no `bool`
bitfield), `enum-underlying` (a `BlueprintType` enum class is `uint8`: error; an enum class without an underlying type:
warning, UHT accepts it as `int`), `preprocessor` (no reflection macro inside `#if` other than
`WITH_EDITOR` / `WITH_EDITORONLY_DATA`), `macro-in-cpp`, `header-name`, `interface`, `uht-parse`, and
`raw-object-pointer` (a UObject pointer member without `UPROPERTY`: `warn` in a reflected class, `note` elsewhere).

## Self-test

`./run.sh --selftest` copies the module to `<build>/selftest`, plants one fault per kind (wrong engine argument type,
missing include, `.h` / `.cpp` signature mismatch, a module removed from `Build.cs`, an include after the generated
header, a `UPROPERTY` inside `#if PLATFORM_IOS`, a `bool` bitfield `UPROPERTY`, a `virtual` BlueprintImplementableEvent,
the same anonymous-namespace helper in two files for the unity build, a parameter named like a member of its class
(`-Wshadow`), an audio component call with a wrong argument, and in the UI: a helper used without its header, a Slate
style argument passed by value, an `OnPaint` override with a wrong parameter type, texture mip data used without
`TextureResource.h`) and requires each to be reported at the planted
place; runs `tools/stub_bodies.py` on a probe header that holds every declaration shape (it must find exactly the
body-less templates, give them bodies and leave the rest alone, keep a `// no-body:` declaration, and report exactly
the stub-bodied templates with an unconstrained explicit type parameter); then compiles `tests/negative_cases.cpp` once
per case and
requires an error matching the case's `EXPECT` pattern, while the control case (the correct twin of every case, plus
SDK templates instantiated with anonymous-namespace and local types) must compile cleanly. A fault that is not reported
means the SDK or the fake UHT has become too permissive: fix that before trusting a PASS.

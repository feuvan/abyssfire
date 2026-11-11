# CompileCheck: an engine-free compile check of `Source/Abyssfire`

The UE 5.8 presentation module (`unreal/Source/Abyssfire`) cannot be built where the core is developed: there is no
engine in the container. This directory makes a clang front end do the part of a UE build that needs no engine binary:
every `.cpp` of the module is parsed and type-checked, one translation unit per file, against a declaration-only copy
of the engine API, with the command line UnrealBuildTool would use; a second pass over the whole module then looks for
what only a linker or a unity blob would show.

```
unreal/CompileCheck/
  run.sh                  the entry point (wraps tools/compile_check.py)
  sdk/                    the stub SDK: engine headers at their real include paths, one tree per engine module
  sdk/modules.json        engine modules: include roots and PUBLIC dependencies (as in each module's .Build.cs)
  uht/fake_uht.py         fake UnrealHeaderTool: writes <Header>.generated.h / .gen.cpp and lints the UHT rules
  tools/compile_check.py  the driver: configurations, PCH, parallel compiles, Build.cs lint, report
  tools/link_scan.py      whole-module AST lints: unity collisions, undefined / duplicate symbols, exports, platform names
  tools/check_reflected.py  cross-checks the SDK's reflected API against the engine's Python stub (UE 5.6)
  tools/stub_bodies.py    keeps SDK templates usable with lambdas / local types, and constrained (see "Adding to the SDK")
  tools/sdk_usage.py      lists the SDK declarations a set of module files uses: what their PASS rests on
  tools/include_audit.py  how each module file reaches the SDK headers it uses: the include edges a PASS rests on
  tools/selftest.py       plants faults in a scratch copy of the module and runs tests/negative_*.cpp
  tests/negative_cases.cpp       code UE rejects: every case must be rejected by the SDK too
  tests/negative_boundaries.cpp  the same for include boundaries (a type used through a forward declaration only)
  API_UNCERTAIN.md        engine API the module uses that could not be confirmed for 5.8, with the Mac checklist
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
unreal/CompileCheck/run.sh --no-link-scan        # compiles only (the link scan is skipped with --only anyway; --link-scan forces it)
python3 unreal/CompileCheck/tools/link_scan.py --config mac-editor                          # the link scan alone, after a run
python3 unreal/CompileCheck/tools/check_reflected.py --show PlayerController --grep touch   # look a class up in the 5.6 stub
python3 unreal/CompileCheck/tools/check_reflected.py --find spawn_sound                     # search reflected functions
python3 unreal/CompileCheck/tools/sdk_usage.py --only World --baseline Framework --uses     # SDK declarations World adds
python3 unreal/CompileCheck/tools/sdk_usage.py --only Private/UI/Panels --json u.json && \
    python3 unreal/CompileCheck/tools/include_audit.py u.json --edges                       # SDK include edges Panels rest on
unreal/CompileCheck/run.sh --stdlib libc++                                    # against libc++ (what Xcode / the NDK use)
unreal/CompileCheck/run.sh --stdlib libc++ --define _LIBCPP_REMOVE_TRANSITIVE_INCLUDES=1    # ... and include-what-you-use for <std> headers
unreal/CompileCheck/run.sh --define UE_STUB_STRICT_FORMAT_WIDTH=1 --no-unity  # lint: a 64-bit integer under '%d' (not a UE error)
unreal/CompileCheck/run.sh --msvc-lint --build-dir /tmp/cc-msvc               # lint: MSVC-only warnings UE treats as errors on Windows
unreal/CompileCheck/run.sh --define UE_ENABLE_SLATE_VECTOR_DEPRECATION_MECHANISMS=0         # the lines that need Slate's FVector2D bridge
```

Needs `clang++` (18 used here; `--cxx` / `ABYSS_CC_CXX` to choose), `python3`, and for the link scan the `libclang`
shared library of the same clang (found under `/usr/lib/llvm-*/lib`; `ABYSS_LIBCLANG` to choose; without it the scan
is skipped and says so). Results are cached per translation unit (dependency files), so a re-run after an edit only
recompiles and rescans what the edit reaches; `--force` ignores the cache. A result that stopped at a missing header
is never reused (the dependency file cannot name a header that did not exist yet), so a file is compiled again as soon
as the SDK gains the header. A full uncached run takes about 5 minutes on 3 otherwise idle cores (4 x 102 compiles,
then 4 x 101 scans); the selftest 3 to 4.

`--stdlib libc++` compiles against LLVM's libc++ instead of the system default (libstdc++ on Linux): Xcode and the
Android NDK both use libc++, whose headers include each other differently, so a missing `<std>` include can pass with one
library and fail with the other. It needs the libc++ headers of the clang in use (`apt-get install libc++-18-dev
libc++abi-18-dev` for clang 18) and its own build directory; `--define _LIBCPP_REMOVE_TRANSITIVE_INCLUDES=1` adds
libc++'s strict mode (each `<std>` header only declares its own content, where newer libc++ releases are heading). Both
are optional extra runs: the default run does not need libc++.

Output: per-configuration pass counts, the diagnostics merged across configurations (errors, warnings UE turns into
errors, `-Wshadow` findings, plain warnings), the Build.cs lint, the link-scan findings, a summary table by area, the
list of `// UNVERIFIED` SDK blocks the result rests on, and `build/report.json`, `build/uht_report.json`,
`build/compile_commands.json`.

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
4. **Build.cs lint**: every engine header a module file includes must belong to a module `Abyssfire.Build.cs` lists
   itself. UBT also gives the module the include paths of the PUBLIC dependencies of its dependencies (`Engine`
   re-exports `Slate`, `SlateCore`, `InputCore`, `RenderCore`, `RHI`, ...), but links it only against what its own
   Build.cs names: a header that is merely reachable compiles everywhere and fails to link in the modular editor build.
   An error; listed-but-unused modules are notes.
5. For each configuration: a real clang PCH of what UBT force-includes (`Definitions.Abyssfire.h` +
   `Private/AbyssfirePCH.h`), then `clang++ -fsyntax-only -std=c++20` over every module `.cpp` and every `.gen.cpp`,
   one translation unit each, and last the **unity build**: every module `.cpp` that passed, `#include`d in path order
   into ONE more translation unit (`--no-unity` skips it).
6. **Link scan** (`tools/link_scan.py`; skipped with `--only` unless `--link-scan`): every translation unit of each
   configuration is parsed again with libclang and the facts are put together, as a linker or a unity blob would:
   - `unity-collision`: the same internal-linkage name (file-scope `static`, anonymous namespace) defined in two `.cpp`
     files, or the same undeclared helper name in one named namespace of two `.cpp` files with different parameters.
     The unity unit of step 5 only catches the pairs that are a redefinition; two functions with different parameters
     compile there, silently become one overload set, and a call can bind to the other file's helper in a unity build
     only. The module has 345 internal-linkage names and no collision.
   - `undefined`: declared in a module file, used (or virtual, or a destructor) and defined in no translation unit.
   - `duplicate` / `header-definition`: a non-inline definition in two files / in a header.
   - `core-export` (the modular editor configuration): a non-inline AbyssCore function the module uses without
     `ABYSS_API`. The scan compiles with `-fvisibility=hidden`, so an unexported symbol is hidden as in the editor
     build; the module uses 151 core symbols, all exported.
   - `platform-name`: a global or anonymous-namespace name of the module that Apple's `MacTypes.h` / Objective-C
     headers also define (`Rect`, `Point`, `Cell`, `Size`, `Ptr`, `Component`, ...), or any identifier spelled like a
     macro of those headers (`YES`, `NO`, `nil`, `MIN`, `MAX`, `DOMAIN`, ...).
   - `lp64-mix`: one `min` / `max` / `clamp` call (std or `FMath`) that mixes `std::int64_t` / `std::uint64_t` with
     `size_t` / `ptrdiff_t` / `long` (one type on Linux and Android, two on macOS / iOS), or a format string with a
     platform-dependent length modifier (`%ld`, `%lu`, `%zu`).

Configurations (the module's `#if`s are `WITH_EDITOR`, `UE_BUILD_SHIPPING`, `PLATFORM_ANDROID || PLATFORM_IOS`,
`PLATFORM_IOS`; every branch is compiled by at least one configuration):

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
The opposite failure is covered by the unity build unit and the `unity-collision` rule of the link scan.

## What a PASS proves

For every file in every configuration: the preprocessor paths are well-formed; every include resolves within the
module's dependency closure, and belongs to a module the Build.cs lists; every name, member, overload and conversion
type-checks against the declared engine API; `.h` and `.cpp` agree; `override`s override; abstract classes are not
instantiated; format strings of `UE_LOG` / `FString::Printf` / `checkf` match their arguments; delegates bind functions
of the right signature; Slate declarative syntax names real arguments; results the engine marks `[[nodiscard]]` are not
dropped; reflected headers are self-sufficient and follow the UHT rules that are linted. For the module as a whole:
all files also compile as one unity translation unit; no file-local helper name is shared between two `.cpp` files;
every function the module declares and uses is defined exactly once; no header defines a non-inline symbol; every
AbyssCore symbol it uses is exported; no global name collides with Apple's platform headers.

The selftest shows that these statements are checked, not assumed: 39 faults planted in real module files (each
reported at the planted place, the configuration-specific ones in exactly the configurations that compile the branch)
and 214 negative cases of invalid UE code (each rejected with the expected error, next to its correct twin).

## What it cannot prove

- **Fidelity of the SDK.** This is the limit that matters most. Reflected API (classes, bases, UFUNCTIONs, properties,
  enums) is checked against the engine's own Python stub, but that stub is UE 5.6 and the project targets 5.8.
  Everything else (templates, containers, Slate, Core, non-reflected members, access levels, which header includes
  which) is written from UE 5.x source knowledge: a wrong declaration makes the check pass or fail wrongly. The final
  audit found six such places that were too wide (a boolean conversion on `TWeakObjectPtr`, weak lambdas on shared
  objects, the legacy UTF-8 converters, an implicit `TObjectPtr` constructor, `FObjectInitializer` constructors on
  subsystems, an `FSlateColor` constructor) and one include edge too many; none of them carried a module file, and each
  is now pinned by a negative case. More of the same kind can exist. Declarations are deliberately on the strict side
  where unsure (a missing transitive include in the SDK costs one explicit `#include` in the module).
- **Deprecation and removal.** No SDK declaration carries `UE_DEPRECATED`: an API that was deprecated before 5.8 and
  has since been removed passes here. None is known in what the module uses.
- **The engine's side of the link.** The link scan covers the module's own symbols and the AbyssCore exports. It cannot
  see whether an engine function the module calls is exported from its module (`*_API`), and nothing here runs the real
  linker.
- **The C# build files.** `Abyssfire.Build.cs` is read with a regular expression (dependency lists only); the two
  `Target.cs` files and the `.uproject` are never evaluated. Wrong C# there, a missing plugin entry, or the
  `FPSemantics = FPSemanticsMode.Precise` line (still marked `[Verify]`) show up on the first UBT run only.
- **Real UHT.** The fake UHT is a scanner for the declaration shapes this module uses. It does not validate metadata
  values, Blueprint exposure rules, replication, or anything it does not parse (it reports `uht-parse` instead). Its
  property-shadow rule only knows the engine properties the Python stub lists (the editor-visible ones).
- **Apple and Android toolchains.** Every configuration is compiled for the host (x86-64 Linux) with the platform's
  defines, as plain C++. So `char` is signed and the C library is glibc everywhere; the Mac and iOS runs do not see
  Objective-C++ or Apple's headers; and `std::int64_t` / `std::uint64_t` are `long` / `unsigned long` in every run,
  as on Android, while they are `long long` on macOS / iOS (where `size_t` then is a different type from
  `std::uint64_t`). The `lp64-mix` and `platform-name` rules of the link scan cover the two failure shapes this is
  known to hide (a template argument deduced from both kinds, and names that Apple's headers own); other differences
  of Apple clang, the NDK's clang, their SDK headers (Objective-C on iOS, JNI on Android) are not modelled. The
  standard library is the system's unless `--stdlib libc++` is given; that run uses the libc++ of clang 18, not
  Apple's or the NDK's build of it.
- **Windows.** `Abyssfire.uproject` lists Windows as a target platform, and no configuration models it: `TCHAR` is
  `char16_t` in every run (never `wchar_t`), there is no `__declspec(dllexport)` / `dllimport`, no MSVC front end and
  none of its diagnostics. What is available instead is a lint, `--msvc-lint`: the clang twins of the MSVC warnings UE
  treats as errors on Windows (`-Wmismatched-tags` for C4099, `-Wshadow-all` without the base-class-field case for
  C4456-C4459, `-Wsign-compare` for C4018 / C4389). The module is clean under it in all four configurations, has no
  raw string literal over MSVC's 16,380-byte limit (the largest is 13,791 bytes, `Private/Vfx/AbyssVfxDefaults.cpp`),
  no `[=]` captures, no macros in headers and `#pragma once` everywhere (the audit's scans). Two member names equal
  Win32 macros (`FAbyssUiContext::PlaySound`, `UAbyssAnimInstance::GetCurrentTime`); UE undefines both after its own
  Windows includes, so the risk is low. A Windows build is still a first attempt when it happens.
- **Behaviour.** No code runs. Engine semantics (GC, threading, timing, rendering) are untested; that is what
  `PresentationTests` (pure rules) and the Mac playtest are for.
- **Format widths.** `FString::Printf` / `UE_LOG` are checked the way the engine checks them: `%d` wants an integer,
  whatever its size. An `int64` under `%d` (typically `FMath::RoundToInt(double)`, which returns `int64` since 5.0)
  compiles in UE and here, and is still a width mismatch for C varargs. The opt-in lint
  `--define UE_STUB_STRICT_FORMAT_WIDTH=1` reports those places as errors (64-bit under `%d` / `%u` / `%x`, 32-bit
  under `%lld`); the consteval check escalates through lambdas, so a mismatch inside a lambda is reported where the
  lambda is called. Use `FMath::RoundToInt32` (or `%lld` with a `long long` cast) at the reported call.

## What is still unverified

A PASS rests on three kinds of unconfirmed knowledge, in decreasing order of risk:

1. **Open entries of `API_UNCERTAIN.md`** (engine API the module calls that could not be confirmed for 5.8). Each is
   declared in the SDK in a block marked `// UNVERIFIED`, and every run ends its summary with those blocks and whether
   each was compiled. Today:
   - #8, Slate's `FVector2D` bridge (`UE_ENABLE_SLATE_VECTOR_DEPRECATION_MECHANISMS`): on in 5.6 (the Python stub
     shows it), believed on in 5.8. If it is off, 22 lines in 12 files fail in every target. The one entry that can
     stop a game target.
   - #1, `UMaterialInterface::GetMaterialResource` (feature level or shader platform): hedged in the module with
     `if constexpr (requires ...)`; editor target only.
   - #3, `FCoreDelegates::GetApplicationWillTerminateDelegate()`: believed real; two lines.
   - #5, `UActorComponent::MarkRenderInstancesDirty()`: hedged; the fallback compiles and logs once per run.
   - #4 (`Cast` keeps constness) and #6 (`GetAudioListenerPosition` is const): cannot break the build either way.
   `API_UNCERTAIN.md` starts with one `grep` per entry to run on the Mac.
2. **Lower-confidence SDK declarations** (the table at the end of `API_UNCERTAIN.md`): 66 rows of declarations
   made from memory where it matters which way they are wrong, each with what happens if it is.
3. **Everything else that is not reflected**: all of Core's templates and Slate's widgets as remembered.
   `tools/sdk_usage.py` lists the 2,300-odd SDK declarations the module uses; the reflected ones (418 UFUNCTIONs, 454
   UPROPERTYs, 92 classes, 66 structs, 87 enums) are the verified part.

## Pointing the check at a real engine

When an engine source tree or an installed build is available (the Mac, or a Linux machine with the engine source),
the stub SDK stops being the authority. The procedure below replaces it step by step; it is a procedure, not code in
this directory. `$UE` is the engine root, `$P` the project (`unreal/Abyssfire.uproject`).

1. **Let UBT write the real compile commands and run the real UHT.**
   `"$UE/Engine/Build/BatchFiles/RunUBT.sh" -Mode=GenerateClangDatabase -Project="$P" AbyssfireEditor Mac Development`
   (`RunUBT.bat` on Windows; `Linux` instead of `Mac` on a Linux source build; repeat with `Abyssfire Mac Development`,
   `Abyssfire Android Development`, `Abyssfire IOS Shipping`). The mode prepares the target as a build would, which
   includes running UnrealHeaderTool, then writes `compile_commands.json` (into the engine root unless `-OutputDir=`
   says otherwise) with one entry per source file: the exact clang command UBT would run, with the real definitions,
   include paths, PCH and generated-header directory (`Intermediate/Build/<Platform>/<Arch>/<Target>/Inc/Abyssfire/UHT`).
   If that directory is empty afterwards, build the target once normally: the commands need the real generated headers.
   The mode's file filters differ between engine versions (see `GenerateClangDatabase.cs` under
   `Engine/Source/Programs/UnrealBuildTool/Modes`). A UHT error at this point is a real one: compare it with the fake
   UHT's lint, and teach the lint the rule if it missed it.
2. **The authoritative compile.** Either build normally, or keep the one-file-per-translation-unit strictness of this
   check: run the database entries of `Source/Abyssfire` with `-fsyntax-only` added (clang-tidy, clangd and
   `run-clang-tidy` read the same file), or build with `-DisableUnity -NoPCH`. Every error is now a fact about 5.8:
   a MODULE BUG, or a place where the stub SDK was wrong and this check passed anyway (fix the SDK header and add a
   negative case, so the container check does not regress).
3. **Test the stub SDK against the real headers.** `tests/negative_cases.cpp` and `tests/negative_boundaries.cpp` are
   the SDK's specification in executable form: CASE 0 is code the engine must accept, every other case code it must
   reject. Take one database entry of a module file, replace the source file with `tests/negative_cases.cpp`, add
   `-DCASE=0`, then each `-DCASE=N`. A control that does not compile, or a case that compiles, is a declaration the
   SDK has wrong (too strict or too wide) at exactly that API; correct the SDK header to what the real header says.
   (The test file includes engine headers by their real paths, so it needs no change.)
4. **Close the uncertain entries.** Run the greps at the top of `API_UNCERTAIN.md`, delete the entries they confirm,
   remove the matching `// UNVERIFIED` markers and `UE_STUB_UNVERIFIED_*` switches, and remove a hedge from the module
   only when the supported engine range no longer needs it.
5. **Diff what the module uses.** `python3 tools/sdk_usage.py --json used.json` lists every SDK declaration the module
   uses with its SDK header and line. The SDK tree mirrors the engine's (`sdk/Runtime/Core/Public/X.h` is
   `$UE/Engine/Source/Runtime/Core/Public/X.h`, `sdk/Plugins/...` is `$UE/Engine/Plugins/...`), so each can be looked
   up in the real header; `tools/include_audit.py used.json --edges` lists the SDK include edges to compare with the
   real `#include` lines. Regenerate the Python stub with the 5.8 editor (`unreal/Scripts`) and re-run
   `tools/check_reflected.py`: the reflected part is then checked against the target version, not 5.6.
6. **Keep both.** The real build is the authority; this check stays the fast one that runs where the core is
   developed. Each discrepancy found in steps 2-5 goes into the SDK with a negative case.

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
  module's fallback for an engine without the API. A hedge must not be silent at run time: the fallback branch logs
  once (`AbyssWorldUtil::WarnInstancedFallback`, `WarnShaderGateInactive`), so a green build that took the slow or
  inactive path shows in the log. Every run ends its summary with the list of `// UNVERIFIED` blocks and whether each
  was compiled (`report.json`: `unverified`): a PASS rests on the ones that are on.

## Adding to the SDK

- **A header** goes where the engine has it: `sdk/<module root>/<include path>` (`Engine` has two roots, `Classes`
  and `Public`; plugins live under `sdk/Plugins/...`). Start it with the provenance comment
  `// Stub SDK. Real header: Engine/Source/...` and say where the content comes from: "reflected: checked against the
  5.6 stub" or "UE 5.x source knowledge". Include what the real header includes (as far as known) and nothing more:
  do not put the world into `CoreMinimal.h`, and forward-declare where the engine forward-declares
  (`Widgets/DeclarativeSyntaxSupport.h` only forward-declares `SWidget`).
- **A module**: add it to `sdk/modules.json` with its include roots and the PUBLIC dependencies of its `.Build.cs`.
  It becomes visible to the module only through `Abyssfire.Build.cs` (directly or as a public dependency), and the
  Build.cs lint wants it listed directly as soon as a module file includes one of its headers.
- **A reflected class** `class UFoo : public UBar { UE_STUB_CLASS_BODY(UFoo, UBar) public: ... };` (structs:
  `UE_STUB_STRUCT_BODY(FFoo)`, native interfaces: `UE_STUB_IINTERFACE_BODY(IFoo, UFoo)`). Mark what is reflected with
  the real `UFUNCTION(...)` / `UPROPERTY(...)` / `UENUM(...)` lines (they expand to nothing): `check_reflected.py`
  then verifies the class, its base, module and header, each Blueprint-exposed function (name, `ScriptName`,
  parameters) and each editor-visible property against the Python stub. Members without a marker are source knowledge
  and unverifiable: keep them to what the module needs. Look a class up first:
  `python3 tools/check_reflected.py --show Foo`. BlueprintNativeEvents also need their `virtual ..._Implementation`.
  Write the engine's `meta = (...)` on the marker when it carries `ScriptName` (an enum or function renamed for
  Python) or a `Param = value` default (a Blueprint default the C++ declaration does not have): the checker reads both.
- **A constructor form is API too.** `UE_STUB_CLASS_BODY` gives a class both `TClass()` and
  `TClass(const FObjectInitializer&)`; a class that only has a default constructor in the engine (`USubsystem` and its
  family) uses `UE_STUB_CLASS_BODY_DEFAULT_CTOR`, so `Super(ObjectInitializer)` in a derived class fails as in UE. The
  fake UHT reads the macro to decide which constructor a constructor-less module class gets.
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
  through `TDereferenceWrapper`, `EmplaceValue` needs `is_constructible`, a weak lambda needs a UObject owner, ...), or
  the engine's real body when that is a one-liner. If the body is not known well enough to say what it needs, do not
  declare the function until the module uses it. A parameter pack is only for what is variadic in the engine
  (`EmplaceValue`, not `SetValue`).
- **Real shapes.** Containers, strings, smart pointers, delegates, `TAttribute`, `SNew` / `SLATE_*` are real templates
  and macros so that a wrong argument type fails; keep it that way, and add a case to `tests/negative_cases.cpp` when
  a new API family arrives (plus its correct twin in `CASE 0`).
- **A conversion is API too.** A constructor that takes one argument is an implicit conversion unless the engine marks
  it `explicit`, and a wrong one makes every call site that passes the wrong type compile. Declare a converting
  constructor only when the engine has it, with the engine's explicitness: `FName` has none from `FString` and only
  explicit ones from string views; `TArray`'s element- / allocator-changing constructors are explicit;
  `FSoftObjectPath` is implicit from `FString` only; `TObjectPtr` is implicit from a pointer and explicit from a
  reference; `FSlateColor` has no constructor from `FColor`; `TWeakObjectPtr` has no boolean conversion (deleted).
  Character overloads are over the engine's character types (`WIDECHAR` is `TCHAR` on the checked platforms; `wchar_t`,
  `uint16`, `uint32` are not characters here), never over "anything that converts to `TCHAR`". When the explicitness is
  not known, declare it `explicit` (the strict side) and note it in `API_UNCERTAIN.md`.
- **An attribute is API too.** UE builds with `-Werror`, so `[[nodiscard]]` on an engine function makes a dropped
  result a build error. `FMath`, the vector / rotator / quaternion types and the container queries carry it here as in
  the engine; give it to a new declaration when the engine's has it (and not to mutating calls whose result is
  routinely dropped: `Add`, `Emplace`, `Remove`, `FindOrAdd`, `Normalize`).
- **An overload set is API too.** One overload declared where the engine has two rejects valid calls of the other (the
  strict side), but it also ACCEPTS a call the engine finds ambiguous: a braced list, or a type that converts to both
  parameter types. When a second overload is believed to exist and cannot be confirmed, record it in
  `API_UNCERTAIN.md` and check that the module's calls pass the exact parameter type (as for
  `UMaterialParameterCollectionInstance::SetVectorParameterValue`).
- **Keep unverifiable surface small.** A member without a `UFUNCTION` / `UPROPERTY` marker cannot be cross-checked:
  declare it when the module uses it, not because the engine has it. `python3 tools/sdk_usage.py` (libclang through
  ctypes, no package needed) lists the SDK declarations a set of module files really uses, `--baseline` subtracts what
  earlier areas already used: review that list when an area turns green, and move what is doubtful to
  `API_UNCERTAIN.md`. Its `--json` output feeds `tools/include_audit.py`, which says for every (module file, SDK
  header) pair whether the file includes the header itself (or through a module header), reaches it only through other
  SDK headers (`--edges` lists those include edges: each must exist in the engine, or the file compiles here and not
  there), only names a forward-declared type, or depends on the force-included PCH. Review the edge list with the
  declarations: an SDK header that includes more than the engine's hides a missing include in the module.
- **Slate widgets.** A widget header declares the engine's `SLATE_BEGIN_ARGS` list (argument names and types are the
  API a declarative call is checked against), `Construct`, the slot type with its mixins, and only the setters the
  module calls. The `FArguments` constructor keeps the engine's defaults except the style lookups
  (`FCoreStyle::Get().GetWidgetStyle<...>(...)` becomes `nullptr`): `Styling/CoreStyle.h` is not part of the SDK until
  the module itself uses it. A slot gets exactly the engine's mixins (`SCanvas::FSlot`: alignment, no padding;
  `SScrollBox::FSlot`: padding and alignment, no size rule; `SConstraintCanvas::FSlot`: its own four attributes and
  nothing else), so a setter borrowed from another panel fails. Slots that hold Slate attributes derive from
  `TWidgetSlotWithAttributeSupport` (`TBasicLayoutWidgetSlot`, `SCanvas::FSlot`), a constraint canvas slot from
  `TSlotBase`. Access levels are API too: `ComputeDesiredSize` is protected in `SBox` / `SBorder` / `SCanvas` (outside
  code asks `GetDesiredSize()`), `OnPaint` / `ComputeDesiredSize` are private pure virtuals of `SWidget`.
- **An engine switch is modelled as a switch.** `Types/SlateVector2.h` has both sides of
  `UE_ENABLE_SLATE_VECTOR_DEPRECATION_MECHANISMS` (the `FVector2D` bridge of Slate's float API; the bridge struct
  `FDeprecateSlateVector2D` is a global-scope USTRUCT, only its aliases live in `UE::Slate`). The default is the
  engine's (on); `./run.sh --define UE_ENABLE_SLATE_VECTOR_DEPRECATION_MECHANISMS=0` lists every line of the module
  that relies on the bridge (API_UNCERTAIN.md #8). Results of Slate getters are wrapped explicitly in the module
  (`FVector2D Size(Geometry.GetLocalSize());`): keep new code that way.
- After any SDK change: `./run.sh --selftest` and `python3 tools/check_reflected.py`.

## The fake UHT

`UCLASS(...)` expands to `<CURRENT_FILE_ID>_<line>_PROLOG` and `GENERATED_BODY()` to
`<CURRENT_FILE_ID>_<line>_GENERATED_BODY`, the real mechanism; both are only defined by the generated header, which
also sets `CURRENT_FILE_ID`, so a missing or misplaced `#include "X.generated.h"` fails to compile. The generated body
declares `Super`, `ThisClass`, `StaticClass()`, the deleted copy / move, a constructor (only when the class declares
none: the `FObjectInitializer` form, or `TClass();` when its base class only has a default constructor, as UHT
decides), the virtual destructor (only when the class declares none), `DECLARE_FUNCTION(execFoo)` per UFUNCTION and
ends with `private:`. Each reflected header also gets a `.gen.cpp` that includes the header on its own after the PCH,
default-constructs the class unless it is `Abstract` (so an abstract UCLASS without the specifier fails), and calls
each UFUNCTION as the thunk would.

Lint rules (`error` fails the run, `warn` / `note` are listed): `generated-include` (present and last),
`generated-body`, `prefix` (U / A / F / E / I), `class-base`, `specifier` (known UCLASS / UPROPERTY / UFUNCTION /
USTRUCT / UENUM specifiers; a `BlueprintImplementableEvent` has no body and is not `virtual`), `property-type` (types
UHT can reflect; module types must themselves be reflected; engine types are looked up in the Python stub; no `bool`
bitfield; no UObject class by value), `property-shadow` (a UPROPERTY with the name of a UPROPERTY / UFUNCTION of a base
class: module bases from their headers, engine bases from the Python stub, compared as UHT compares names; C++ accepts
such a member, UHT does not), `enum-underlying` (a `BlueprintType` enum class is `uint8`: error; an enum class without
an underlying type: warning, UHT accepts it as `int`), `preprocessor` (no reflection macro inside `#if` other than
`WITH_EDITOR` / `WITH_EDITORONLY_DATA`), `macro-in-cpp`, `header-name`, `interface`, `uht-parse`, and
`raw-object-pointer` (a UObject pointer member without `UPROPERTY`: `warn` in a reflected class, `note` elsewhere).

Standing findings (0 errors): `Public/World/AbyssZoneActor.h:316`, `:317` (`PendingInstances`, `ComponentByMesh`:
`TMap` with `TObjectPtr<UStaticMesh>` keys and no `UPROPERTY`; used only during the zone build, the meshes are held by
the asset library; confirm on the Mac that no garbage collection can run between filling and clearing them) and a
note on `Public/Anim/AbyssAnimInstance.h:25`.

## Self-test

`./run.sh --selftest` copies the module to `<build>/selftest` and plants faults, one compile-check run per group, and
requires each to be reported at the planted place:

- compiler: a wrong engine argument type, a missing include, a `.h` / `.cpp` signature mismatch, a module removed from
  `Build.cs`, a parameter named like a member (`-Wshadow`), an audio component call with a wrong argument; in the UI: a
  helper used without its header, a Slate style argument passed by value, an `OnPaint` override with a wrong parameter
  type, texture mip data used without `TextureResource.h`, a widget used through a forward declaration only, the stretch
  direction given to a scale box's stretch rule, a date formatted with an `FString`, a pointer handler overridden with
  the wrong event type; and, in a run with `UE_STUB_STRICT_FORMAT_WIDTH=1`, an `int64` under `%d`;
- unity build: the same anonymous-namespace helper in two files (a redefinition in the unity unit);
- configurations (all four are run, and the fault must be reported in exactly the listed ones): a fault inside
  `#if PLATFORM_ANDROID || PLATFORM_IOS` (android-game, ios-shipping), inside `#if UE_BUILD_SHIPPING` (ios-shipping),
  inside `#if WITH_EDITOR` (mac-editor), and a name only the editor build declares (the three game configurations);
- fake UHT: an include after the generated header, a `UPROPERTY` inside `#if PLATFORM_IOS`, a `bool` bitfield
  `UPROPERTY`, a `virtual` BlueprintImplementableEvent, a `UPROPERTY` named like one of `AActor`, a UObject class by
  value as a `UPROPERTY`;
- Build.cs lint: `"Slate"` removed (everything still compiles through `Engine`'s public dependencies);
- link scan: one helper name with different parameters in two anonymous namespaces, and in one named namespace of two
  files (both invisible to the compiler and to the unity unit); a function declared, used and never defined; a
  non-inline definition in a header; `ABYSS_API` removed from a core class in a scratch copy of `AbyssCore/Public`; a
  struct named `Rect` in an anonymous namespace; a local named `nil`; `std::max(std::uint64_t, size_t)`;
- `--msvc-lint`: a constructor parameter that hides a member.

It then runs `tools/stub_bodies.py` on a probe header that holds every declaration shape (it must find exactly the
body-less templates, give them bodies and leave the rest alone, keep a `// no-body:` declaration, and report exactly
the stub-bodied templates with an unconstrained explicit type parameter), and compiles `tests/negative_cases.cpp` and
`tests/negative_boundaries.cpp` once per case, requiring an error that matches the case's `EXPECT` pattern, while the
control case of each file (the correct twin of every case, plus SDK templates instantiated with anonymous-namespace
and local types) must compile cleanly. A fault that is not reported means the SDK, the fake UHT or a lint has become
too permissive: fix that before trusting a PASS.

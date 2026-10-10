# unreal/Scripts — content build (UE editor Python)

`build_content.py` turns the generated art (`Art/Export/`: FBX, palette atlases, portraits, the art `manifest.json`)
and the rendered audio (`Audio/Export/`) into the project's content under `/Game/Abyssfire`, creates every material
the game uses, and writes the one persistent map `L_Main` (DECISIONS P5–P11, `Docs/spec/ue58-platform.md` §11). It is
plain CPython 3.11 + the standard library, run by the Unreal Editor (`-run=pythonscript`). Nothing under
`unreal/Content/` is edited by hand: re-run the build instead.

| Path | What |
|---|---|
| `build_content.py` | entry point (options below) |
| `build_content.sh` | macOS / Linux wrapper: builds `AbyssfireEditor`, runs the build headless, checks the report |
| `abyss_content/` | the package: `manifest.py` (plan), `hlsl.py` (all material shader code), `pngio.py`, `report.py`, `ue/` (editor side) |
| `tests/` | `python3 -m unittest discover -s unreal/Scripts/tests` — no Unreal needed (see *Developer checks*) |
| `fonts/` | the backbone's font subsetter (not part of the content build) |

The C++ side has one small helper, `Source/Abyssfire/{Public,Private}/ContentBuild/AbyssContentBuildLibrary.*`
(Python: `unreal.AbyssContentBuildLibrary`): it names skeletal-mesh sockets (read-only to Python) and waits for shader /
asset compilation. Build the editor target before running the content build.

---

## 1. One-time setup on macOS (Apple Silicon — the primary platform)

Requirements: Apple Silicon Mac (M1/M2 minimum, M3+ recommended; UE 5.8 dropped Intel-Mac rendering), macOS Sequoia
15.x (Sonoma 14.5 minimum), ≥ 32 GB RAM, ≥ 150 GB free disk (engine ~60 GB + DerivedDataCache + Android SDK).

1. **Xcode 26.1.1** (26.0 minimum; **26.4 is not compatible with UE 5.8**). Download the `.xip` from
   developer.apple.com → *More downloads*, put `Xcode.app` in `/Applications`, then:
   ```bash
   sudo xcode-select -s /Applications/Xcode.app
   sudo xcodebuild -license accept
   xcodebuild -runFirstLaunch
   xcodebuild -downloadComponent MetalToolchain   # Xcode 26 no longer ships it; the editor and the cook compile Metal with it
   xcodebuild -downloadPlatform iOS               # only for iOS packaging
   ```
2. **Unreal Engine 5.8.3**: install the Epic Games Launcher → *Unreal Engine* → *Library* → `+` → **5.8.3** (or the
   newest 5.8.x hotfix). In *Options* tick **Mac**, **iOS** and **Android** target platforms (Windows targets are built
   on a Windows PC). Default location: `/Users/Shared/Epic Games/UE_5.8`.
3. Clone the repository (no Git LFS: art and audio exports are ordinary committed files) and set two variables for the
   commands below:
   ```bash
   cd /path/to/abyssfire
   export UE="/Users/Shared/Epic Games/UE_5.8"
   export PROJ="$PWD/unreal/Abyssfire.uproject"
   ```
4. Android packaging only: Android Studio (or the command-line tools) with SDK Platform 36, Build-Tools 36.0.0,
   Platform-Tools, **NDK r27c (27.2.12479018)**, CMake and **JDK 21**; export `ANDROID_HOME`, `NDKROOT` and `JAVA_HOME`,
   then let UE check them:
   ```bash
   "$UE/Engine/Build/BatchFiles/RunUAT.sh" Turnkey -command=VerifySdk -platform=Android -UpdateIfNeeded
   ```
5. iOS / Mac distribution only: put your Apple Developer **team id** into `Config/DefaultEngine.ini`
   (`CodeSigningTeam=` and `IOSTeamID=`, marked TODO, DECISIONS P12) — never commit someone else's id.

## 2. Build the editor and the content

```bash
# project files (once, and after adding source files) — optional for command-line builds, needed for Xcode
"$UE/Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh" -project="$PROJ" -game

# editor target (compiles AbyssCore + Abyssfire, including the content-build helper)
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" AbyssfireEditor Mac Development -project="$PROJ" -waitmutex

# content build, headless (first run 10–30 min: Interchange imports + shader compilation for every material)
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" -run=pythonscript \
  -script="$PWD/unreal/Scripts/build_content.py" -unattended -nosplash -nop4 -NoSound -stdout -FullStdOutLogOutput
```

If the engine has `Engine/Binaries/Mac/UnrealEditor-Cmd`, use it instead of the `.app` binary (same arguments). The
wrapper does all three steps and checks the result:

```bash
unreal/Scripts/build_content.sh                     # UE_ROOT=... to override the engine path; SKIP_EDITOR_BUILD=1
```

Options go inside the `-script="…"` string (the wrapper forwards its own arguments; values cannot contain spaces there):

| Option | Effect |
|---|---|
| `--only STEPS` / `--skip STEPS` | comma list of `textures, mpc, materials, instances, fxquad, skeletal, anims, static, assign, audio, level, verify` |
| `--family NAMES` | only manifest assets matching a family / category / name, e.g. `--family heroes` |
| `--force` | re-import and rebuild everything (otherwise unchanged sources, graphs and settings are skipped) |
| `--force-materials` | rebuild the master materials even when their graph is unchanged |
| `--prune-audio`, `--audio-args "…"` | passed to `Audio/ue/import_audio.py` |
| `--report PATH` | JSON report path (default `$AF_CONTENT_REPORT`, else `unreal/Saved/ContentBuild/build_report.json`) |
| `--strict` | warnings fail the build too |
| `--plan` | validate `manifest.json` and print the plan; works with a plain `python3`, writes nothing to the project |

**Result.** `unreal/Saved/ContentBuild/build_report.json` (`"ok": true|false`, per-step status and counts, every asset
created / re-imported / unchanged, material shader statistics, verification lines) and `build_report.md` (the same,
readable). A failed build also ends with a Python exception, so the log shows `LogPython: Error` (CI checks both,
`Docs/CI.md` §8). Re-running with unchanged inputs changes nothing.

From the editor UI instead: *Tools → Execute Python Script…* → `unreal/Scripts/build_content.py`, or in the *Output
Log* Python console: `py "/path/to/abyssfire/unreal/Scripts/build_content.py" --only materials`.

## 3. Run the game in the editor

```bash
open "$PROJ"        # or: "$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ"
```

`L_Main` opens (it is the editor start-up and game default map, `Config/DefaultEngine.ini`). Press **Play**: the title
screen appears; *New Game* builds the first zone at runtime. Useful console commands: `abyss.NewGame`,
`abyss.Continue`, `abyss.Debug` (`Source/Abyssfire/README.md`). Mobile look on the Mac: *Settings → Preview Rendering
Level → Android Vulkan / iOS* (every material must compile there too).

## 4. Package

All from the repository root, after the content build (cooking reads `unreal/Content`). Development builds shown;
use `-clientconfig=Shipping` for releases (CI: `unreal/CI/scripts/ue-package.sh`).

```bash
UAT="$UE/Engine/Build/BatchFiles/RunUAT.sh"
COMMON=(-project="$PROJ" -build -cook -stage -pak -iostore -compressed -package -archive -nop4 -utf8output -unattended)

# macOS (Apple Silicon): Abyssfire.app in out/Mac
"$UAT" BuildCookRun "${COMMON[@]}" -platform=Mac -clientconfig=Development -archivedirectory="$PWD/out/Mac"

# iOS (needs the team id from step 1.5 and a connected / registered device for Development)
"$UAT" BuildCookRun "${COMMON[@]}" -platform=IOS -clientconfig=Development -archivedirectory="$PWD/out/IOS"
#   App Store / TestFlight: add -distribution (distribution certificate + provisioning profile)

# Android (arm64, ASTC textures; .apk + .aab)
"$UAT" BuildCookRun "${COMMON[@]}" -platform=Android -cookflavor=ASTC -clientconfig=Development \
  -archivedirectory="$PWD/out/Android"
#   Play upload: add -distribution and the signing keys in Config/Android/AndroidEngine.ini (never committed)
```

**Windows** (on a Windows 10/11 PC with Visual Studio 2026 — or 2022 17.14+ — and MSVC 14.50; avoid 14.51) — the
content can be rebuilt there with the same script, or copied over from the Mac (`unreal/Content/`):

```bat
set UE=C:\Program Files\Epic Games\UE_5.8
"%UE%\Engine\Build\BatchFiles\Build.bat" AbyssfireEditor Win64 Development -project="%CD%\unreal\Abyssfire.uproject" -waitmutex
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\unreal\Abyssfire.uproject" -run=pythonscript ^
  -script="%CD%\unreal\Scripts\build_content.py" -unattended -nosplash -nop4 -stdout -FullStdOutLogOutput
"%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%CD%\unreal\Abyssfire.uproject" -platform=Win64 ^
  -clientconfig=Development -build -cook -stage -pak -iostore -compressed -package -archive ^
  -archivedirectory="%CD%\out\Win64" -prereqs -nop4 -utf8output -unattended
```

After an engine upgrade delete `unreal/Binaries`, `unreal/Intermediate`, `unreal/Saved` and `unreal/DerivedDataCache`
and rebuild (ue58-platform.md §12).

---

## 5. What the build creates (contract with the runtime)

The runtime contract is `Source/Abyssfire/Public/World/WorldContract.md`; the look is `Art/blender/README.md` §2.

| Content | Path | Notes |
|---|---|---|
| Skeletal / static meshes, clips | `/Game/Abyssfire/<dir of the manifest fbx>/<Name>` | Interchange, normals as authored (hull custom normals), vertex colours `AF_Data`, no materials / physics / collision / Nanite |
| Skeletons | `/Game/Abyssfire/Skeletons/SKEL_<Family>` | created by the family's first mesh, shared by the rest and by every clip |
| Sockets | on each skeletal mesh (`fx_*`, `visor`) / static mesh (`tip`, `mid`, …) | from the manifest; static ones also from `SOCKET_` empties |
| LODs | `lods[]` of the manifest imported as LOD n, screen sizes set | |
| Palettes | `/Game/Abyssfire/Textures/T_AF_Palette_<Family>_{BC,P}` | uncompressed, nearest, no mips (exact swatches) |
| Portraits, icons, glyphs | `/Game/Abyssfire/UI/{Portraits,Icons}/T_UI_*` | UserInterface2D, no mips |
| FX sprites | `/Game/Abyssfire/FX/Textures/T_FX_*` | linear (masks); `T_AF_FX_DefaultGlow` is the generated fallback |
| `MPC_AF_Lighting` | `/Game/Abyssfire/Materials` | `SunDir`, `KeyLightDir` (manifest key light), `SunColor`, `Ambient`, `RimColor`, `CameraTanHalfFovY`, `TimeSec` |
| Masters | `/Game/Abyssfire/Materials`: `M_AF_Toon`, `M_AF_Toon_Foliage`, `M_AF_Toon_Slime`, `M_AF_Outline`, `M_AF_Outline_Foliage`, `M_AF_Terrain`, `M_AF_Water`, `M_AF_PP_Grade`; `/Materials/FX`: `M_AF_FX_Additive`, `M_AF_FX_Translucent`, `M_AF_FX_Mesh`, `M_AF_Ghost`, `M_AF_LightPool`, `M_AF_TargetRing`, `M_AF_AffixAura`, `M_AF_BlobShadow` | shader code in `abyss_content/hlsl.py` |
| Instances | `/Game/Abyssfire/Materials/Instances`: `MI_AF_Toon_<Family>`, `MI_AF_Outline_<Family>_<Class>` | slot 0 / slot 1 of every mesh; the hull sections cast no shadow |
| `SM_FX_Quad` | `/Game/Abyssfire/FX` | 100 × 100 cm XY quad (unless the art ships one) |
| Audio | `/Game/Abyssfire/Audio/...` | `Audio/ue/import_audio.py` (audio agent) |
| `L_Main` | `/Game/Abyssfire/Maps/L_Main` | empty persistent map; world settings only |

Material behaviour in short:

* **`M_AF_Toon`** — Unlit, masked (dithered fade): 3-band half-Lambert in sRGB space from the palette atlases
  (`shade / base / light`, bands at 0.42 / 0.86 ± 0.01), grounding from `AF_Data.r`, banded rim on the lit half,
  emissive regions unshaded (× 1.6), then the gameplay feedback from custom primitive data: `HitFlash` (0),
  `PainTint` (1–4), `Telegraph` (5–8), `StatusTint` (9–12), `Fade` (13), `Ghost` (14), `Highlight` (15); instanced
  dressing reads `PerInstanceCustomData` 0 Fade / 2 Highlight. `_Foliage` adds wind (WPO from the instance `Random`),
  `_Slime` the jelly rim and a dithered body (R12).
* **`M_AF_Outline`** — ink `mix(#120C18, line(c), o)`, screen-constant width: WPO dilates the baked hull to
  `OutlinePx1080 × ViewSizeY / 1080` px (manifest `shading.outline.wpo`); material quality *Low* keeps the baked hull.
* **`M_AF_Terrain`** — lit (receives sun shadows), base colour from the web tile-transition algorithm over the runtime
  `TileIds` texture (exact port of `ZoneTerrain.ts` weights / ranks / lattice noise / liquid, lip and dark edges) with
  procedural ground patterns (patches, specks, accent details); ambient fill from `MPC Ambient`. Low quality drops the
  boundary noise and the details.
* **`M_AF_Water`**, **`M_AF_FX_*`**, **`M_AF_Ghost`**, quads, **`M_AF_BlobShadow`**, **`M_AF_PP_Grade`** — see the
  docstrings in `abyss_content/ue/materials.py`.

## 6. First run on 5.8.3 — things to look at once

The build was written against the UE 5.6 Python API (names checked automatically, see below) without an engine at
hand. On the first real run check:

1. The report is `"ok": true` and `materials` lists every master with non-zero pixel-shader instructions. A master
   with `NO SHADER` failed to compile: open it, the error names the Custom node (`AF_*`); fix the HLSL in
   `abyss_content/hlsl.py` (the tests below compile it as C++ too).
2. *Preview Rendering Level → Android Vulkan* and *iOS*: `M_AF_Toon`, `M_AF_Outline`, `M_AF_Terrain` compile.
3. `SK_Hero_Warrior` in the mesh editor: 2 material slots with `MI_AF_Toon_Heroes` / `MI_AF_Outline_Heroes_Hero`,
   sockets `fx_*` and `visor` on the listed bones, LOD1 present; the clips play on `SKEL_Human`; `A_Hero_Warrior_HurtAdd`
   is additive (local space, frame 0).
4. The outline is about 3.5 px at 1080p around the hero in PIE and stays that thick when zooming the camera.
5. `M_AF_PP_Grade` runs *after tonemapping*; if colours look washed out or too dark, the input is linear on this engine
   version: tell the maintainers (the grade assumes display-encoded input, like the web canvas).
6. Warnings about redirectors: run *Fix Up Redirectors* on `/Game/Abyssfire` once.

## 7. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `AbyssContentBuildLibrary missing` warning, no skeletal sockets | the editor target was not rebuilt after pulling; build `AbyssfireEditor` first |
| `unreal.X does not exist in this engine version` | an API rename in 5.8: the message names the class / property / enum; update `abyss_content/ue/*` |
| `import produced no asset` | Interchange rejected the FBX; the log has `LogInterchange` errors. Legacy importer fallback for one run: add `Interchange.FeatureFlags.Import.FBX False` under `[ConsoleVariables]` in `Config/DefaultEngine.ini` (ue58-platform.md §11.3) |
| `bound the mesh to … instead of SKEL_…` | the FBX armature differs from the family skeleton (bone names / hierarchy): re-export from the art kit |
| `r.Substrate is True` error | `Config/DefaultEngine.ini` must keep `r.Substrate=False` (DECISIONS P11) |
| pink materials in game | a texture with other compression / sRGB settings was bound at runtime; re-run the build (it resets texture settings) |
| everything re-imports every run | the source files change (line endings / LFS filters) or `IMPORT_SETTINGS_VERSION` was bumped |

## 8. Developer checks (no Unreal needed)

```bash
python3 -m unittest discover -s unreal/Scripts/tests -v
python3 unreal/Scripts/build_content.py --plan            # manifest validation + plan
python3 unreal/Scripts/tests/fetch_ue_stub.py             # optional: UE Python API stub for test_api_names.py
```

* `test_hlsl.py` compiles every Custom-node body as C++ against `tests/hlsl_emu.h` (an HLSL-subset emulation; needs
  `clang++` or `g++`) and checks the toon / ink / outline maths against the art kit's reference formula (≤ 0.6/255),
  the terrain node on a synthetic map, dithering and the quad falloffs.
* `test_api_names.py` checks every editor-API name the build uses (classes, subsystem methods, properties, enum
  members, material-expression properties) against a generated stub: the PyPI `unreal-stub` (UE 5.6) fetched by
  `fetch_ue_stub.py`, or your own editor's stub (`UE_PY_STUB=<project>/Intermediate/PythonStub/unreal.py`, written
  with *Editor Preferences → Python → Developer Mode*) — prefer the 5.8.3 one once installed.
* `test_flow.py` runs the whole build twice against `tests/fake_unreal.py` (an in-memory `unreal`) and checks the
  results and that a second run is a no-op. `test_manifest.py` covers the planner, `pngio` and the generated textures.

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
(Python: `unreal.AbyssContentBuildLibrary`): it names skeletal-mesh sockets (read-only to Python), sets skeletal LOD
screen sizes (no longer exposed to Python in 5.8), sets material-instance parameters MaterialEditingLibrary did not
apply, waits for shader / asset compilation and reads a material's shader compile errors. Build the editor target
before running the content build: every function the scripts call is listed in `abyss_content/ue/core.py`
(`CONTENT_LIB_FUNCTIONS`), and an editor binary built before one of them existed is reported once at the start of the
build (*STALE EDITOR BINARY*, naming the missing functions and the rebuild command); the steps then skip what needs them
with one note each instead of a warning per asset.

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
unreal/Scripts/build_content.sh --shaders           # also compile every master for real and report compile errors
```

`SKIP_EDITOR_BUILD=1` skips the editor build only while the project's editor module
(`unreal/Binaries/<Mac|Linux>/UnrealEditor-Abyssfire*.dylib` / `libUnrealEditor-Abyssfire*.so`, else
`UnrealEditor.modules`) is newer than every `*.h / *.cpp / *.inl / *.cs` under `unreal/Source`; otherwise (or when it is
missing) the wrapper prints a warning and builds `AbyssfireEditor` anyway. If that build fails while a (stale) binary
exists, the wrapper warns and runs the content build on that binary, which reports the helpers it lacks once
(`STALE EDITOR BINARY`). `SKIP_EDITOR_BUILD=force` never builds (same warning when the binary is stale). Each build it
runs leaves `Binaries/<host>/.abyss_editor_build_stamp`, so an up-to-date binary that UBT did not need to touch counts as
current.

**Shaders.** The headless run is a `-run=pythonscript` commandlet, which renders with the NullRHI: no shader is
compiled and `MaterialEditingLibrary.get_statistics` reads 0. The report therefore lists every master as *not compiled
(NullRHI commandlet; run build_content.sh --shaders)* with one note, not as a failure. `--shaders` adds
`-AllowCommandletRendering` to the editor command line (real RHI; slower, every master compiles for the editor's shader
platform) and is passed on to `build_content.py`, whose verify step then waits for compilation, reads the instruction
counts and turns `AbyssContentBuildLibrary.get_material_compile_errors` into report errors (a master without a pixel
shader is an error too). After every run the wrapper prints the first 40 shader-error lines of the editor log
(`Failed to compile Material`, `[SM5]/[SM6] … error`, `error: … .usf`, `Material … had errors`); with `--shaders` they
fail the run. `AF_CONTENT_NULLRHI=1|0` overrides the NullRHI detection.

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
| `--shaders` | compiled shaders required: compile errors and masters without a pixel shader fail the build (the wrapper also adds `-AllowCommandletRendering`; on a NullRHI commandlet the option itself is an error) |
| `--plan` | validate `manifest.json` and print the plan; works with a plain `python3`, writes nothing to the project |

**Result.** `unreal/Saved/ContentBuild/build_report.json` (`"ok": true|false`, per-step status and counts, every asset
created / re-imported / unchanged, material shader statistics, verification lines, and `compatibility`: engine-version
differences the build worked around, one line per build with the number of assets each touched — e.g. an Interchange
property this engine renamed or dropped, or 5.8's instance-parameter setters returning False) and `build_report.md`
(the same, readable). A failed build also ends with a Python exception, so the log shows `LogPython: Error` (CI checks
both, `Docs/CI.md` §8). Re-running with unchanged inputs changes nothing.

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

**Standalone `-game` run** (the editor binary without the editor UI, e.g. for a playtest at the window size players get):

```bash
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJ" -game -windowed -ResX=1280 -ResY=720 -log
```

The content is uncooked there, so any shader the derived-data cache does not hold yet is compiled the first time it is
needed, and until it is ready UE draws the mesh with its default material (lit grey, black away from the sun). In
`-game` runs the game therefore waits at each zone entry, behind the fade, until pending shaders and asset builds are
done (at most `abyss.WaitForCompilationMaxSec`, 20 s; the window does not respond meanwhile, and the log says
`Uncooked run: finishing N shader jobs…`), and keeps zone dressing that is still compiling hidden until it is ready; in
PIE only the hiding applies (`Source/Abyssfire/Public/World/WorldContract.md` §6). Run
`unreal/Scripts/build_content.sh --shaders` once before playtesting (it compiles every master for this machine's shader
platform into the local cache, so there is little left to wait for), or test a cooked package (§4), which never compiles
shaders at run time. UE also reports the compiles in its on-screen message block (`Preparing Shaders (N)`); the game
hides that block in non-shipping game runs (`AbyssPlatform::ApplyScreenMessagePolicy`, called from
`UAbyssGameInstance::Init`; the log says `On-screen engine messages hidden`), so shader, map and
`AddOnScreenDebugMessage` warnings do not cover the HUD:

| Want | How |
|---|---|
| the engine messages for the whole run | add `-abyssscreenmessages` to the command line |
| show / hide them at run time | in the console: `EnableAllScreenMessages`, `DisableAllScreenMessages`, `ToggleAllScreenMessages` |
| hide them with a binary built before this change | add `-ExecCmds="DisableAllScreenMessages"` to the command line |

PIE (the editor's *Play*) and Shipping builds are unaffected: the editor keeps the messages, Shipping never draws them.

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
| Skeletal / static meshes, clips | `/Game/Abyssfire/<dir of the manifest fbx>/<Name>` | Interchange, normals as authored (hull custom normals), vertex colours `AF_Data`, no materials / physics / collision / Nanite; clips import frames 0..`frames` at `fps` of the manifest (explicit range: Blender's FBX exporter writes a fixed 0–1 s global timeline; an engine that cannot set the range imports the keyed range, the same frames, with one compatibility note) |
| Skeletons | `/Game/Abyssfire/Skeletons/SKEL_<Family>` | created by the family's first mesh, shared by the rest and by every clip |
| Sockets | on each skeletal mesh (`fx_*`, `visor`) / static mesh (`tip`, `mid`, …) | from the manifest; static ones also from `SOCKET_` empties |
| LODs | `lods[]` of the manifest imported as LOD n, screen sizes set | |
| Palettes | `/Game/Abyssfire/Textures/T_AF_Palette_<Family>_{BC,P}` | uncompressed, nearest, no mips (exact swatches) |
| Portraits, icons, glyphs | `/Game/Abyssfire/UI/{Portraits,Icons}/T_UI_*` | UserInterface2D, no mips |
| FX sprites | `/Game/Abyssfire/FX/Textures/T_FX_*` | linear (masks); `T_AF_FX_DefaultGlow` is the generated fallback |
| `MPC_AF_Lighting` | `/Game/Abyssfire/Materials` | `SunDir`, `KeyLightDir` (manifest key light), `SunColor`, `Ambient`, `RimColor`, `CameraTanHalfFovY`, `TimeSec`, `SunScale` (.55), `InkScale` (1.5), toon light slots `Light0PosR..Light7PosR` / `Light0Col..Light7Col` (0); writers in WorldContract §3.5 |
| Masters | `/Game/Abyssfire/Materials`: `M_AF_Toon`, `M_AF_Toon_Foliage`, `M_AF_Toon_Slime`, `M_AF_Toon_Fade`, `M_AF_Toon_Foliage_Fade`, `M_AF_Outline`, `M_AF_Outline_Foliage`, `M_AF_Toon_Ash`, `M_AF_Outline_Ash`, `M_AF_Toon_Ashnumb`, `M_AF_SelectRim`, `M_AF_Terrain`, `M_AF_Terrain_Painted`, `M_AF_Water`, `M_AF_PP_Grade`, `M_AF_PP_AOComposite`; `/Materials/FX`: `M_AF_FX_Additive`, `M_AF_FX_Translucent`, `M_AF_FX_Mesh`, `M_AF_Ghost`, `M_AF_LightPool`, `M_AF_TargetRing`, `M_AF_AffixAura`, `M_AF_BlobShadow`, `M_AF_Telegraph` | shader code in `abyss_content/hlsl.py` |
| Instances | `/Game/Abyssfire/Materials/Instances`: `MI_AF_Toon_<Family>`, `MI_AF_Outline_<Family>_<Class>`, `MI_AF_Terrain_<Theme>`, `MI_AF_SelectRim` | slot 0 / slot 1 of every mesh; the hull sections cast no shadow. Toon instances carry their family's `CastShadowStrength` (heroes / monsters / NPCs / pets / weapons .7, pickups .8, props 1, foliage canopies .6, ground cover 1; `manifest.plan_look_parameters`), the plains ground cover `BaseSaturation` / `BaseValue` .85 / .93. `MI_AF_Terrain_<Theme>`: `M_AF_Terrain_Painted` with the manifest `terrain.<theme>` layers + detail atlas (only when all of them were exported); plains `GroundSaturation` / `GroundValue` .85 / .93. `MI_AF_SelectRim`: the hero selection rim (#FFF4DC, 2 px) |
| Terrain textures | `/Game/Abyssfire/Textures/T_Terrain_<Theme>_*_BC`, `T_Water_<Theme>_BC` | world (sRGB, mips, wrap); the `_Details_BC` atlas BC7, clamp; imported even without the texture scan (the manifest names them) |
| `SM_FX_Quad` | `/Game/Abyssfire/FX` | 100 × 100 cm XY quad (unless the art ships one) |
| Audio | `/Game/Abyssfire/Audio/...` | `Audio/ue/import_audio.py` (audio agent) |
| `L_Main` | `/Game/Abyssfire/Maps/L_Main` | empty persistent map; world settings only |

Material behaviour in short:

* **`M_AF_Toon`** — shadow-catcher lit toon (DECISIONS P11 revised, art pass; WorldContract §3.3): DefaultLit with the
  normal input in world space. Custom node `AF_ToonSplit` computes the toon colour below (lit) and its shade-band floor
  (shade) and outputs Emissive = shade + (1 − k)(lit − shade) and, through its additional output `Lit`, BaseColor =
  k (lit − shade) / SunColor, Normal = MPC `KeyLightDir`, Specular 0, Metallic 0, Roughness 1: in full sun the frame is
  exactly the unlit toon, in a cast shadow it drops to the shade band by k = `CastShadowStrength`; without sun shadows
  (mobile low / mid) it equals the unlit look. Torches and spell glows add through the eight MPC light slots
  (`Light<i>PosR` / `Light<i>Col`, written by `UAbyssVfxSystem`); no engine local light lights the toon. Masked
  (dithered fade: a 4 × 4 Bayer shifted every TSR / TAA jitter sample
  (`View.TemporalAAParams`), resolved by the AA, static without it; the shadow pass ignores the instanced
  dressing's occlusion fade): 3-band half-Lambert in sRGB space from the palette atlases
  (`shade / base / light`, bands at 0.42 / 0.86 ± 0.01), grounding from `AF_Data.r`, banded rim on the lit half,
  emissive regions unshaded (× 1.6), then the gameplay feedback from custom primitive data: `HitFlash` (0),
  `PainTint` (1–4), `Telegraph` (5–8), `StatusTint` (9–12), `Fade` (13), `Ghost` (14), `Highlight` (15); instanced
  dressing reads `PerInstanceCustomData` 0 Fade / 2 Highlight; `BaseSaturation` / `BaseValue` grade the palette colour
  (ground cover, R19). `_Foliage` adds wind (WPO from the instance `Random`). `_Slime` (the jelly rim and an
  alpha-blended translucent body, `BodyOpacity` × fade, back faces culled; R12) stays **unlit** (`AF_Toon`), as do
  `M_AF_Toon_Fade` / `M_AF_Toon_Foliage_Fade`, the translucent twins of `M_AF_Toon` / `_Foliage` (alpha 1 − fade)
  for the occlusion-fade ghosts of the zone dressing on tiers without temporal AA (DECISIONS R1;
  `AAbyssZoneActor` creates them at runtime, no manifest slot names them; they show the unshadowed colour).
* **`M_AF_Outline`** — ink `mix(#120C18, line(c), o)`, screen-constant width: WPO dilates the baked hull to
  `OutlinePx1080 × InkScale × ViewSizeY / 1080` px (manifest `shading.outline.wpo`; MPC `InkScale` 1.5, so the class
  values read as px per 720 rows: hero / weapon / NPC 3.5, monster 3, interactive 2, decor props 1.4, foliage 1.2;
  DECISIONS R3 revised) on every quality tier (the mobile-low baked-only fallback is gone; `OutlineScreenSpace` 0 is
  the kill switch). The WPO keeps the per-frame view inputs (camera-relative position, view forward, tan half FOV):
  the hull never casts a shadow, so no MPC copy of the camera is needed.
* **`M_AF_SelectRim`** — the selection rim (DECISIONS R16, WorldContract §7): unlit, masked, skeletal usage; the ink
  hull widened `RimPx720 × 1.5` px per 1080 rows beyond the ink and pushed up to `RimDepthPushCm` (30) along the camera
  ray (same screen position, behind the figure, so it only shows outside the silhouette; clamped 2 cm above the
  owner's ground plane, `ActorPositionWS`, so it still closes round the feet); colour `RimColor`; dithers with
  the body's `Fade` (CPD 13); `BodyMask` 1 hides a slot. `MI_AF_SelectRim` holds the hero rim; the runtime derives the
  target (#FF5A3C) and hover (#FFB03C, both 1.5 px) rims from it.
* **`M_AF_Terrain`** — lit (receives sun shadows), base colour from the web tile-transition algorithm over the runtime
  `TileIds` texture (exact port of `ZoneTerrain.ts` weights / ranks / lattice noise / liquid, lip and dark edges) with
  procedural ground patterns (patches, specks, accent details) × MPC `SunScale` (the sun itself is pi lux); ambient
  fill from `MPC Ambient`; `GroundSaturation` / `GroundValue` (default 1 = bit-identical) compress the composited
  ground in display space (R19). Low quality drops the boundary noise and the details.
* **`M_AF_Terrain_Painted`** — the same mask and edge tints over the art's painted ground (`Art/blender/environment/
  terrain.py`, its `composite()` is the reference): the five `Tile<i>Layer` textures sampled once at `UV0 / 3` with the
  shared wrap sampler, the winner's / runner-up's picked in the Custom node, the `Details` atlas cell of the own tile
  (explicit gradients from the continuous tile coordinate, so tile borders keep their mip) composited over the winner.
  No procedural patterns, so far less ALU than `M_AF_Terrain`, but every ground pixel on every quality tier reads the
  four `TileIds` texels, all five 1024² layers and the atlas (the layers are sampled outside the node, so Low, which
  drops only the boundary noise, reads them too; `M_AF_Terrain`'s Low read only `TileIds`). The theme adds six 1024²
  textures (≈ 4.7 MB BC, ≈ 8 MB ASTC 4x4 with mips). Not yet profiled on an ES3.1 / iOS device. Used through
  `MI_AF_Terrain_<Theme>`; zones whose theme has no painted art keep `M_AF_Terrain`.
* **`M_AF_PP_AOComposite`** — post-process blendable after depth of field: scene colour × lerp(1, SSAO,
  `AOStrength` .25) on the visible ground only (`PPI_SCENE_DEPTH` reaching `PPI_CUSTOM_DEPTH`, which the terrain alone
  writes while the blendable is bound; toon pixels carry the fake KeyLightDir normal, DECISIONS P11 / P14); bound on
  desktop balanced / high only. `M_AF_PP_Grade` (the exact web grade) is still built but no longer bound.
* **`M_AF_Water`** (its sun term reads MPC `SunScale`), **`M_AF_FX_*`**, **`M_AF_Ghost`**, quads,
  **`M_AF_BlobShadow`** — see the docstrings in `abyss_content/ue/materials.py`.
* **`M_AF_Telegraph`** (dark remake U2 / 2G, DECISIONS DR5 / DR16) — the enemy ground telegraph on `SM_FX_Quad`:
  unlit translucent, two-sided; the custom node `AF_Telegraph` (`hlsl.telegraph_node`) measures the pixel against the
  shape in five CPD groups (`hlsl.TELEGRAPH_CPD`, the runtime's `AbyssTelegraphRules::Cpd`; WorldContract §3.7) and
  draws the crimson rim (`T_FX_TelegraphRim`), the bone fill with its front and grain (`T_FX_TelegraphFill`), the flash
  and the arena's outward rim and wash. Colours and scalars are the manifest's `materials.M_AF_Telegraph` contract
  (`manifest.telegraph_contract`; shipped values when a field is missing). The two textures come from `VFX/` of the
  art export (imported as FX sprites, clamped: the shader wraps them); a plan without them warns and the material
  falls back to `T_AF_FX_DefaultGlow`. The five CPD vectors reach the node as float4 (`Graph.float4`: AppendVector of
  the parameter's RGB output 0 and its A; output 0 alone is RGB and `.a` would not compile). Blank pixels (a decal's
  corners, a ring's hole, the arena's inside) return before any texture sample. `tests/test_hlsl.py` runs the node
  through the HLSL emulation against the art's preview port and the runtime's CPD layout, and checks the early exit is
  exact on a dense grid.
* **`M_AF_Toon_Ash` / `M_AF_Outline_Ash`** (dark remake U3 / 2G, DECISIONS DR6 / DR16) — the twins a dying monster's toon
  / ink slots move to (`UAbyssCorpseLayer`, a dynamic instance per slot copying the slot's parameters): built by
  `build_toon(..., ash=True)` / `build_outline(..., ash=True)` with the originals' settings, outputs and CPD; the ash
  branch adds `AF_AshMask` (`hlsl.ash_mask_node`: object-space noise plates cut from the top down by `Dissolve` over
  `HuskTopCm`, multiplied into the dither and shadow masks, a pale edge band) and, on the toon twin, `AF_AshGrey`
  (`hlsl.ash_grey_node`: the toon split's Emit / Lit to the ash colours by `Grey`). Scalars `Grey`, `Dissolve`,
  `HuskTopCm`, `EdgeWidth`, `NoiseTileCm` and the colours are plain parameters (not CPD); their defaults are the
  manifest's `materials.M_AF_AshDissolve` contract (`manifest.ash_contract`), the noise `T_FX_AshNoise` (planned even
  without a scan; a plan without it warns). `AF_AshMask` reads the pre-skinned position through a `VertexInterpolator`
  (`materials._pre_skinned_position_ps`: UE compiles PreSkinnedPosition in the vertex shader only; 2G review H-M1, which
  rebuilt both twins once). The originals' graphs are untouched (the dry-run material fingerprints match the last
  build). `tests/test_hlsl.py` checks both nodes against the art's preview math and the runtime header's
  defaults; `tests/test_flow.py` the twins' settings, outputs and parameters.
* **`M_AF_Toon_Ashnumb`** (dark remake K6 / 2G stage H, DECISIONS DR16) — the twin the hero's toon slots move to while it
  carries Ashnumb stacks (`AAbyssCharacterActor::UpdateAshnumbBand`, a dynamic instance per slot copying the slot's
  parameters): built by `build_toon(..., band=True)` with `M_AF_Toon`'s settings, outputs and CPD; `AF_AshnumbBand`
  (`hlsl.ashnumb_band_node`) after the split bands the toon from the feet up (the pre-skinned object-space z over
  `BandHeightCm`) to `BandTop`, toward the ash in the lit bands / the shade ash in the shade band and in cast shadow,
  with a pale line on the top edge. `BandTop` / `BandHeightCm` are the runtime's per-hero scalars; the look's defaults
  (`BandSoftness`, `BandStrength`, `BandEdgeWidth`, `BandAshColor`, `BandAshShade`, `BandEdgeColor`) come from the art
  lane's `Art/Export/VFX/vfx_recipes.json dark.materials.AshnumbTint` (`manifest.ashnumb_contract`; read next to the
  manifest, the shipped values when the file is missing). Every other master's dry-run fingerprint is unchanged.
  `tests/test_hlsl.py` checks the node against the art's band mock (`Art/blender/vfx/ashnumb_band.py`) in sun and in
  shadow and the runtime header's defaults; `tests/test_flow.py` the twin's settings, outputs and parameters;
  `tests/test_manifest.py` the contract. 2G review: the pre-skinned position reaches the node through a
  `VertexInterpolator` (H-M1), and the node takes the split's `HitFlash` / `PainTint` / `PainAmt` / `Telegraph` /
  `TelegraphAmt` / `StatusTint` / `StatusAmt` (`hlsl.ASHNUMB_FEEDBACK_INPUTS`, the same CPD parameters) and gives the ash
  targets the same feedback, so a hit still flashes the banded legs white and status tints colour the ash (H-m1;
  `test_hlsl` checks the band against feedback-then-band exactly and the real split + band chain at flash 1).

**Art pass: one-time rebuild.** The art pass changes the graphs of `M_AF_Toon`, `M_AF_Toon_Foliage`, `M_AF_Toon_Slime`,
`M_AF_Toon_Fade`, `M_AF_Toon_Foliage_Fade`, `M_AF_Outline`, `M_AF_Outline_Foliage`, `M_AF_Terrain`,
`M_AF_Terrain_Painted` and `M_AF_Water` and adds `M_AF_SelectRim`, `M_AF_PP_AOComposite` and `MI_AF_SelectRim`, so the
first build after it rebuilds those masters once (their fingerprints changed; the others report *unchanged*) and every
toon / terrain instance gets its new scalars. `MPC_AF_Lighting` gains `SunScale`, `InkScale` and the 16 light-slot
vectors (existing entries keep their GUIDs), so every material that reads the collection recompiles its shaders once.
Expect the next `build_content.sh --shaders` to take about as long as a first build. `AF_ToonSplit`'s `Lit` is a Custom
node *additional output*: should an engine not expose that pin, the build wires a single-output twin
(`AF_ToonSplit_Lit`, the same code) instead and reports it once under *compatibility*.

## 6. First run on 5.8.3 — things to look at once

The build was written against the UE 5.6 Python API (names checked automatically, see below) without an engine at
hand. On the first real run check:

1. The report is `"ok": true`. Run once with `build_content.sh --shaders`: `materials` then lists every master with
   non-zero pixel-shader instructions and no compile errors (without `--shaders` they read *not compiled*, see §2). A
   master with `COMPILE ERRORS` / `NO SHADER` failed to compile: the error names the Custom node (`AF_*`); fix the HLSL
   in `abyss_content/hlsl.py` (the tests below compile it as C++ too).
2. *Preview Rendering Level → Android Vulkan* and *iOS*: `M_AF_Toon`, `M_AF_Outline`, `M_AF_SelectRim`,
   `M_AF_Terrain`, `M_AF_Terrain_Painted` compile (`M_AF_PP_AOComposite` is bound on desktop only; it must at least
   not break the mobile cook).
3. `SK_Hero_Warrior` in the mesh editor: 2 material slots with `MI_AF_Toon_Heroes` / `MI_AF_Outline_Heroes_Hero`,
   sockets `fx_*` and `visor` on the listed bones, LOD1 present; the clips play on `SKEL_Human`; `A_Hero_Warrior_HurtAdd`
   is additive (local space, frame 0).
4. The outline is about 3.5 px at 720p (5.25 px at 1080p) around the hero in PIE, on every quality tier, and stays
   that thick when zooming the camera; the selection rim adds 2 px of #FFF4DC outside it (target 1.5 px #FF5A3C).
5. `M_AF_Toon` in the material editor: shading model *Default Lit*, *Tangent Space Normal* off, the `AF_ToonSplit`
   node shows two output pins (`return`, `Lit`) with `Lit` wired to Base Color (or the build reported the
   `AF_ToonSplit_Lit` twin). In PIE a goblin walking into a tree's cast shadow drops toward its shade band but stays
   readable; no shadow acne on characters; on *Preview Rendering Level → Android Vulkan* (no CSM on mobile low / mid)
   the toon looks exactly like the unlit toon.
6. Warnings about redirectors: run *Fix Up Redirectors* on `/Game/Abyssfire` once.

## 7. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `STALE EDITOR BINARY` warning (missing `AbyssContentBuildLibrary` functions), LOD screen sizes / sockets skipped | the editor target was not rebuilt after pulling (`SKIP_EDITOR_BUILD=force`, or the wrapper's rebuild of a stale binary failed: its UBT errors are above the warning); fix the build if it failed, then run the rebuild command the warning prints, or `build_content.sh` without `SKIP_EDITOR_BUILD` |
| every master *not compiled (NullRHI commandlet …)* | expected for a plain headless run; `build_content.sh --shaders` compiles them |
| `… parameter X not applied (MaterialEditingLibrary returned …; helper unavailable …)` | the value did not stick when read back and the C++ fallback is missing (stale editor binary) or failed too; the message lists the parent's parameters. 5.8's setters return False even when they apply the value: that is only a compatibility note |
| `MI_…: parent has no <kind> parameter X (…)` | the instance plan (manifest slot `params`, `materials.py` defaults) names a parameter the master does not declare (typo, or the master renamed it); the message lists the master's parameters. Checked before any setter, because an instance keeps an override for any name |
| `slot N (M_AF_Outline) has no triangles` note | the manifest's `triangles` says the hull is empty, so Interchange imports no such slot; nothing to do |
| `unreal.X does not exist in this engine version` | an API rename in 5.8: the message names the class / property / enum; update `abyss_content/ue/*` |
| `import produced no asset` | Interchange rejected the FBX; the log has `LogInterchange` errors. Legacy importer fallback for one run: add `Interchange.FeatureFlags.Import.FBX False` under `[ConsoleVariables]` in `Config/DefaultEngine.ini` (ue58-platform.md §11.3) |
| `bound the mesh to … instead of SKEL_…` | the FBX armature differs from the family skeleton (bone names / hierarchy): re-export from the art kit |
| `r.Substrate is True` error | `Config/DefaultEngine.ini` must keep `r.Substrate=False` (DECISIONS P11) |
| pink materials in game | a texture with other compression / sRGB settings was bound at runtime; re-run the build (it resets texture settings) |
| dark grey / black lumps without ink where bushes, grass or banners stand in an uncooked run; the log shows `Missing cached shadermap for …`, `Uncooked run: finishing …` / `stopped waiting after …` or `Shader gate: …` (on screen `Preparing Shaders (N)`, in PIE or with `-abyssscreenmessages`) | the masters are still compiling for this RHI and UE draws those meshes with its default material meanwhile (lit grey, black away from the sun). `-game` runs wait for pending compiles at each zone entry (bounded) and hide zone dressing until its shaders are ready; PIE only hides the dressing (`abyss.WaitForCompilation`, `abyss.WaitForCompilationMaxSec`, `abyss.DecorShaderGate`, `abyss.DecorShaderGateMaxSec`: `Source/Abyssfire/Public/World/WorldContract.md` §6). Run `build_content.sh --shaders` once so the DDC already holds every master's shaders. A `Failed to compile Material` line instead means the master is broken, not slow. Cooked builds are not affected |
| everything re-imports every run | the source files change (line endings / LFS filters) or `IMPORT_SETTINGS_VERSION` was bumped |

## 8. Developer checks (no Unreal needed)

```bash
python3 -m unittest discover -s unreal/Scripts/tests -v
python3 unreal/Scripts/build_content.py --plan            # manifest validation + plan
python3 unreal/Scripts/tests/fetch_ue_stub.py             # optional: UE Python API stub for test_api_names.py
```

* `test_hlsl.py` compiles every Custom-node body as C++ against `tests/hlsl_emu.h` (an HLSL-subset emulation; needs
  `clang++` or `g++`; a node's additional outputs become reference parameters) and checks the toon / ink / outline
  maths against the art kit's reference formula (≤ 0.6/255), the terrain node on a synthetic map, dithering and the
  quad falloffs; and the art pass: `AF_ToonSplit` (Emit + Lit × SunColor = `AF_Toon` in full sun over normals,
  palettes, sun tints and every feedback combination; the shade band at k = 1; HitFlash / emissive; the torch slots;
  `hlsl.toon_split_reference` and the look-dev composite), the ink scale (3.5 px at 720 rows at 2125 cm), the selection
  rim (5.5 px, the view push keeps the projection and stops 2 cm above the owner's ground plane), the ground grade
  (1 / 1 bit-identical) and the AO composite (ground pixels only).
* `test_api_names.py` checks every editor-API name the build uses (classes, subsystem methods, properties, enum
  members, material-expression properties) against a generated stub: the PyPI `unreal-stub` (UE 5.6) fetched by
  `fetch_ue_stub.py`, or your own editor's stub (`UE_PY_STUB=<project>/Intermediate/PythonStub/unreal.py`, written
  with *Editor Preferences → Python → Developer Mode*) — prefer the 5.8.3 one once installed.
* `test_flow.py` runs the whole build twice against `tests/fake_unreal.py` (an in-memory `unreal` that behaves like UE
  5.8 where it matters: instance-parameter setters return False, NullRHI commandlet, 5.8's Interchange deprecations;
  switches for a stale / missing helper library, the 5.6 Interchange API and a Custom node without additional output
  pins) and checks the results and that a second run is a no-op. Its `output_width` models the pin widths the engine
  compiles (output 0 of a vector parameter / texture sample / vertex colour is RGB; only "RGBA" is four wide), and
  every Custom-node input of every master must get the width its `NodeInput` declares (2G review B1). Its
  `vertex_only_leaks` models shader frequency: a vertex-shader-only expression (PreSkinnedPosition, PreSkinnedNormal)
  may reach a material output other than World Position Offset only through a `VertexInterpolator` (2G review H-M1;
  the test also proves the rule fails the direct wire).
  `test_editor_compat.py` covers those engine-version
  paths (parameter read-back, parent check and C++ fallback, the stale-binary warning, the shader check, Interchange
  replacements, the clip frame range and its fallback, the `AF_ToonSplit_Lit` twin). `test_manifest.py` covers the
  planner (empty outline slots, clip FBX spans, the art-pass look parameters and `MI_AF_SelectRim`), `pngio` and the
  generated textures. `test_build_content_sh.py`
  runs `build_content.sh` against a stand-in engine (stale-binary rebuild and its failure, `SKIP_EDITOR_BUILD=force`,
  `--shaders`, the log scan).
* `test_api_names.py` also keeps the C++ helper's UFUNCTIONs, `core.CONTENT_LIB_FUNCTIONS` and the helper calls in the
  scripts in step, and checks that every renamed Interchange setting (`meshes.VERSIONED_SETTINGS`) keeps a UE 5.6
  spelling.
* `test_module_headers.py` (2G review B2) fails any public header of `Source/Abyssfire` that names a private
  `Abyss*Rules::` type without including that rules header or declaring its namespace (the class's .cpp and UHT's
  .gen.cpp would not compile): public headers take plain values.
* `test_ui_i18n_keys.py` greps `Source/Abyssfire` for every literal i18n key (Loc* / LocalizeOr / NameOr / `{ "key",
  TEXT("English") }` rows) and fails when one is missing from `Data/i18n_zh-CN.json` or `Data/i18n_en.json` (the UI
  would silently show the English fallback in the zh-CN build). New UI text goes into `PORT_STRINGS`
  (`Tools/export-data/src/tables/i18n.ts`); then re-run the exporter and `Scripts/fonts/build_fonts.py`.
* `test_ui_text_fit.py` (dark remake 1C U5) measures every Chapter 1 string with the bundled fonts (fontTools advance
  widths; skipped without fontTools) against the budgets of the UE panels that show it (quest card, quest log, tracker,
  toasts and banners, combat log, puzzle, frame titles), zh-CN and en, desktop and touch. A failure names the string,
  the panel and the overflow: shorten the text or change the panel (the budgets mirror the Slate code; keep them in
  step).
* `test_font_coverage.py` (1C review) is `fonts/build_fonts.py --check` as a test: every CJK character of
  `Data/*.json` must be in the four bundled Simplified Chinese faces (Sans / Serif, Regular / Bold; the fallback of every
  UI face), or it would draw as a tofu box in UE. A content change that adds a character fails here until
  `python3 unreal/Scripts/fonts/build_fonts.py` (downloads verified against `fonts/fonts.lock.json`; `--offline
  --cache <dir>` with a populated cache) rewrites `unreal/Fonts`, which is committed. Skipped without fontTools.

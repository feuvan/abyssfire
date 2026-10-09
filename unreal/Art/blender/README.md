# Abyssfire Blender art kit (`unreal/Art/blender/`)

Python package `kit/` (Blender 5.0 as a Python module, `bpy`) shared by every Chapter-1 generator script, plus
`tests/smoke_test.py`. It turns the binding art contract — ARCHITECTURE.md §5, DECISIONS P5/P7/R*/W1,
`Docs/spec/art-inventory-ch1.md` §0–§3 — into code: palettes, the toon look, outline hulls, the humanoid rig,
clips, FBX export for UE 5.8, `manifest.json`, and review renders.

```bash
# every script: Blender Python, headless
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/<generator>.py
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/tests/smoke_test.py   # ~12 s, exit 0 = pass
```

Layout (ARCHITECTURE §2; the spec's `Art/Blender/scripts/` is this folder):

| Path | Content |
|---|---|
| `Art/blender/kit/` | the kit (this document) |
| `Art/blender/<family>.py` | generators (heroes, goblins, slime, NPC kit, props, foliage, FX meshes, icons) |
| `Art/blender/palettes/<Family>.json` | swatch registries (committed, append-only → stable UVs) |
| `Art/blender/source/` | optional `.blend` dumps (`kit.scene.save_blend`), git-ignored |
| `Art/Export/<Category>/<Asset>.fbx` | `SK_*`, `A_*`, `SM_*` (categories: Characters, Monsters, NPCs, Weapons, Props, Terrain, Foliage, VFX, Pickups) |
| `Art/Export/Textures/` | `T_AF_Palette_<Family>_BC.png` / `_P.png` |
| `Art/Export/manifest.json` | the asset manifest read by `Scripts/build_content.py` and the core (`Data/assets.json`) |
| `Art/Previews/<Asset>/` | review renders (≤ 200 KB PNG each) |

Env overrides (tests): `AF_EXPORT_ROOT`, `AF_PREVIEW_ROOT`, `AF_PALETTE_ROOT`.

---

## 1. Conventions

* **Units**: Metric, unit scale 1, **1 BU = 1 m = 1 tile**. FBX: `apply_unit_scale`, `FBX_SCALE_ALL`,
  `global_scale 1` → 1 m arrives as **100 uu**. Verified by the smoke test round trip (armature scale 1, 1.76 m).
* **Axes**: characters/directional props face **Blender −Y**, +Z up → **UE +Y** (Blender X → UE X, Y → −Y).
  The UE actor rotates the mesh yaw −90°. Manifest positions/bounds are already in UE cm/axes.
* **Pivots**: characters at the ground point between the feet (`root` at the origin); props at the centre of the
  ground footprint, Z = 0 on the ground; weapons at the grip.
* **Time**: 60 fps, frame = round(ms × 0.06); clip lengths and notifies are stored in **ms**.
* **Determinism**: `kit.scene.rng("SK_Hero_Warrior", "variant")` / `seed_for(...)` (SHA-1 based, never `hash()`);
  palette registries are append-only; no wall-clock or set-order dependence.
* **Naming** (spec §2.2): `SK_<Cat>_<Name>`, `A_<Cat>_<Name>_<Action>`, `SM_<Cat>_<Name>`, materials
  `MI_AF_Toon_<Family>` (slot 0) + `M_AF_Outline` (slot 1), textures `T_AF_Palette_<Family>_BC/_P`.

## 2. The look (what `M_AF_Toon` must reproduce)

The preview material is an **emission-only Cycles material** computing exactly the colour the unlit UE material
computes, so a 4–8 sample render is converged and *is* the UE look (view transform "Standard" = plain sRGB).
`kit.shading.toon_reference()` is the same maths in Python; the smoke test renders flat swatches in every band and
checks the pixels against it (max error 0.64/255). All tone maths runs on **sRGB-encoded values** like the web:

```
c, (s, l, e, o) = T_AF_Palette_BC(uv), T_AF_Palette_P(uv)       nearest filter, no mips
ndl   = dot(N, L_KEY);  h = ndl*0.5 + 0.5                         half-Lambert
shade = mix(mix(c, (30,20,60)/255, .35), 0, s);  light = mix(c, (255,244,214)/255, l)
col   = mix(shade, c,     smoothstep(0.41, 0.43, h))              T_shade 0.42, band width 0.02
col   = mix(col,   light, smoothstep(0.85, 0.87, h))              T_light 0.86
col   = mix(col, #0A0818, 0.22 * AF_Data.r)                       grounding (baked weight, see below)
col   = mix(col, #FFECC8, 0.55 * (1 - max(N·V,0))^4 * smoothstep(0, .05, ndl))   rim on the lit half
col   = mix(col, c, e)                                            emissive regions are unshaded
out   = srgb_to_linear(col) * (1 + 0.6 e)                         (UE: × shadow attenuation on ndl first)
```

* **Key light `L_KEY`** — re-derived for the W1 camera (the spec's "sun yaw 0°, pitch −45°" belonged to the old
  yaw −135° camera): light **from UE yaw −45° (exactly the camera's left), 55° elevation**;
  `dirToLight UE = (0.4056, −0.4056, 0.8192)`. In the game camera's view space it comes from the screen
  upper-left diagonal (right −0.57, up +0.53, toward viewer +0.63). Camera-facing fronts sit in the base band,
  tops/upper-left rims catch the warm band, the shade band sits on the lower-right of forms; surfaces turned to
  screen-right (the `se` 3/4 view) read darker — the 3D form cue the web could not show. Chosen on the smoke-test
  knight among (−25°,58°), (−45°,55°), (−45°,45°), (−10°,50°). The UE directional light / MPC `KeyLightDir`
  must use this vector (it is in `manifest.json → shading.keyLight`).
* **Grounding**: the factor `smoothstep(0.38 H, 0.05 H, z)` is baked per vertex into colour attribute
  **`AF_Data.r`** (linear float; FBX `colors_type='LINEAR'`, read raw in UE) so no per-asset height parameter is
  needed and it follows the feet in animation. `AF_Data.a = 1` on the body.
* **Palette atlas** (spec §1.7): 256², 16×16 swatches of 16 px. `_BC` sRGB base colour; `_P` linear:
  R shadowAmt, G lightAmt, B emissive, A outline mix. Unused swatches are magenta. UE import: BC sRGB on,
  P sRGB off, **Filter Nearest, No Mipmaps**, compression UserInterface2D/BC7 (exact swatches).
* **Outline** (P7, R3): baked **inverted hull** in the same mesh, slot 1 `M_AF_Outline`. Hull vertices are pushed
  out along an area-weighted smoothed normal shared by every corner at the same position (hard edges and seams
  never split the outline) by the class width (`kit.outline.WIDTH_CM`: hero/boss 1.8 cm, NPC 1.6, monster 1.5,
  small monster 1.4, interactive 1.3, weapon 1.3, decor 1.2), faces flipped, same skin weights (skins with the
  body), **custom normals = outward smoothed normal**, `AF_Data` = normal×0.5+0.5 with A = 0. Colour:
  `mix(#120C18 ink, line(c), o)` with `line(c) = mix(mix(c,(20,10,30)/255,.5),0,.55)`; default `o = 1`
  (darkened local colour, R3). UE: unlit, opaque, default back-face culling (draws the far side = the rim), and
  for screen-constant width under zoom: `WPO = VertexNormalWS × (OutlinePx × PixelWorldSize − BakedWidth)`.
  At the default camera distance the baked 1.8 cm is 2.16 px at 1080p (`px1080AtDefaultDistance`) — lighter than
  the web's heavy ink, so every outline slot also carries **`outlinePx1080`** (`kit.outline.SCREEN_PX_1080`,
  spec §1.3: hero/boss/weapon 3.5, NPC/monster 3.0, interactive 2.0, decor 1.2; ∝ viewport height) — the
  `OutlinePx` the UE material instance must use. Heroes ink-weight the hull colour with `o = 0.4` (60 % toward
  `#120C18`, slot field `outlineMix`). Game-camera previews render at that width (`Review(game_outline_px=)`).
  Parts with `outline=False` (visor slits, gems, small emissive bits, bands that sit inside a neighbour's hull)
  get no hull; `Builder.add(part, …, hull=proxy)` bakes the hull from a simpler shape instead (face attribute
  `af_hullonly`: the proxy's hull copy is kept, its own faces dropped) — mail rows, grooved helms, folded capes.
* **Camera (W1)**: yaw 45°, pitch −50°, horizontal FOV 35°, focus 50 cm above ground, default distance
  **25.37 m** = 16 × 12 tiles on 16:9 (`shading.CAM_DIST_DEFAULT`).

## 3. API summary

```python
import kit            # generators live in Art/blender/, so `kit` is importable directly
from kit import scene, palette, shading, mesh as M, rig as R, outline, anim, export, review, asset
from kit import Palette, Region, Bind, HumanoidSpec, Pose, Clip, Key, PoseLib
```

**scene** — `reset(fps=60)`, `seed_for(*parts)`, `rng(*parts)`, `ms_to_frame`, `frame_to_ms`, `link`, `select_only`,
`apply_transforms`, `world_bounds(objs)`, `blender_to_ue_cm(v)`, `look_at_matrix`, `save_blend(asset, family)`.

**color** — exact `tone(hex, shadow=.42, light=.32) → {base, shade, light, line}` (sRGB 0..255), `hex_to_rgb`,
`hex_linear`, sRGB↔linear.

**palette** — `Region(hex, s=.42, l=.32, e=0, o=1)` (`.tone()`, `.line_region()` for modelled crease strips);
`Palette(family)` with `.add(name, region) → swatch`, `.bake_uvs(mesh)` (collapses UVs of face attr `af_swatch`),
`.save()` (registry JSON + both PNGs), `.material_name`, `.manifest_entry()`. Families: Heroes, Monsters_Plains,
NPC, Props_Plains, Foliage_Plains, FX, Test.

**shading** — constants (`T_SHADE`, `T_LIGHT`, `L_KEY`, rim, grounding, ink, camera), `toon_material(pal)`,
`outline_material(pal)`, `flat_toon_material(name, hex)`, `stage_ground_material(...)`, `toon_reference(...)`,
`shading_manifest()`, `key_light_ue(yaw, elev)`, `build_toon_group(l_key)` (rebuilds in place).

**mesh** — every helper returns a `Part` (bmesh) in a local frame; `Part.transform/translate/rotate/scale/
mirrored_x/bend/taper/deform/smooth/flat/auto_sharp/merge/tris`.
Shapes: `sweep(path, profile, scales, up, closed_path, cap0/cap1 'fan'|'ngon'|None, dome0/1, twist_deg)` (the
workhorse) → `tube(points, radii, sides, exp)`, `capsule(a, b, ra, rb)`, `belt(rx, ry, z, height, thickness)`,
`strap(points, width, thickness, up)`; `loft_z([(z, rx, ry[, cx, cy[, exp]]), …])`, `lathe([(r, z), …])`,
`ellipsoid(radii, center)`, `cylinder`, `box(size, center, bevel)`, `plate(outline2d, thickness, bevel)`,
`sheet(w_top, w_bottom, length, thickness, wrap_radius, flare, hem_wave)` (two-sided: face layer `af_sub`
1 = front / 2 = back), `mitten(length, width, thickness, side, curl_deg)`, `boot(length, width, height, shaft)`,
`ear(length, width, curl_deg)`, `horn(base_r, length, bend_deg)`, `gem(radius, top, bottom)`,
`skin_tube(points, edges, radii, subdiv)` (Skin modifier). `superellipse(rx, ry, n, exp)` profiles (exp 2 ellipse,
3–4 rounded box).
`Builder(asset, palette, prefix, lod_ratio=1, lod_skip=None)`: `.regions(name=Region…)`, `.add(part, region, bind,
name, outline=True, sub_regions={1: 'lining'}, hull=None)`, `.build(name, grounding_height)` → mesh object with
`af_swatch`/`af_part`/`af_nohull`/`af_hullonly` face attributes, palette UVs and `AF_Data`. A reduced LOD is the
same generator run with `lod_ratio` < 1 (every part and hull proxy collapse-decimated by `decimate(part, ratio)`)
and `lod_skip(name)` dropping sub-readability details. `tri_count(obj, material_index)`.

**rig** — `HumanoidSpec` (metres: thigh, shin, ankle, torso, neck, head, hip_half, shoulder_half, upper_arm,
fore_arm, hand, foot_len, arm_down, rest_lean, rest_head, face, chains) with `.crown_height()`, `.fit_height(m)`,
`.scaled(k)`, `from_web(units, cm_per_unit)`; `build_humanoid(spec) → Rig` (`.obj`, `.joints` rest landmarks:
pelvis, neck, chest, head_center, head_pivot, crown, shoulder/elbow/wrist/knuckles/handtip/grip_{l,r},
hip/knee/ankle/ball/toe/heel_{l,r}, eye_{l,r}; `.rest(bone)`, `.head/.tail`, `.sockets`).
`frame(origin, y_dir, z_hint)` placement matrices. Skinning: `Bind.rigid(bone)`, `Bind.blend(*bones, falloff=4,
smooth=0)`, `skin(obj, rig, binds)` (inverse-distance-to-bone-segment weights, ≤ 4 influences),
`bind_auto_heat(obj, rig)` (Blender bone heat, works headless), `cleanup_weights`, `weight_stats`;
`add_socket`, `socket_manifest`, `attach_to_bone(obj, rig, bone)` (bone-head attachment = UE socket convention).

**outline** — `bake_hull(obj, width_m, toon_mat, outline_mat)` (honours `af_hullonly` proxies), `WIDTH_CM`,
`SCREEN_PX_1080`, `set_preview_width(obj, m)` (GN modifier for review only; stripped before export),
`pixel_world_size`, `default_px_at_game_distance`.

**anim** — `Pose(rig)`: `.rot(bone, pitch, roll, yaw)` (relative to parent, character axes: X left, −Y forward,
Z up), `.swing/.lift/.spread/.twist` (relative to the bone's rest direction), `.world(...)` / `.aim(bone, dir, up)`
(absolute; feet flat, weapons), `.move(bone, offset)`, `.foot(side, at|offset, pole)` / `.hand(...)` (analytic
two-bone IK, never bends a joint backwards), `.mirrored()`, `.copy()`, `.then(fn)`; `evaluate(pose)`,
`apply_pose`, `clear_pose`, `lerp_pose`. `Clip(name, length_ms, keys=[Key(pose, t|ms, ease)], loop, key_span_ms,
notifies={'Contact': 308}, sampler=fn(ms)→Pose, layers, additive, ref_speed)` — sampled **every frame** with the
web easing (`smooth`/`in`/`out`/`linear`/`hold`, ease of the destination key), slerp, IK solved on interpolated
targets; `bake(rig, clip, "Hero_Warrior") → Action A_Hero_Warrior_<Clip>` (root never keyed: in place).
`run_gait(t, speed, cycle_ms, duty, lift, bob)` — no-slide gait (planted feet move at exactly `speed`, flight phase
when duty < 0.5; R9/QUIRK A5); `gait(...)` = the web gait; `run_contacts_ms`; `PoseLib(rig)` (named poses stored
as JSON on the armature, `.mirror`, `.apply`).

**export** — `export_skeletal_mesh(rig, meshes, asset, category)`, `export_animation(rig, action, category)`,
`export_static_mesh(obj, asset, category, sockets, collision)`; `Manifest()` (`set_palette`, `set_asset`,
`save`), `skeletal_entry`, `static_entry`, `clip_entry`, `bounds_ue`.

**review** — `Review(asset, root, meshes, height, blob_radius)`: `.game_view()` (`game.png` 640×360 at the W1
camera + `game_1080crop.png` native 1080p pixels), `.closeup()` (front 3/4, outline 2.5 px), `.turnaround()`
(8 facings, one scale), `.contact_sheet(rig, [(clip, action)], frames=8)` (`anims.png`; notify frames get an orange
border + label); `game_outline_px=` renders the game views at the UE screen-constant ink width. Offline UI renders
(portraits, icons): `ink_render(meshes, res, silhouette_px, interior_px, depth_step)` — hulls hidden, the web's ink
drawn in screen space instead (constant-width `#120C18` silhouette + contour lines where a nearer surface overlaps
a farther one, found on a 2× depth pass; no hull saw-teeth or slivers at 1:1), `composite_glow` (bakes a runtime
FX sprite such as `visorGlow` into a texture that gets no bloom), `project_px`. Low level: `setup_render`,
`game_camera`, `frame_points`, `mesh_points`, `stage`, `render_array`, `draw_text` (5×7 font), `grid`, `FACINGS`
(`front`, `se`, `side`, `ne`, `back`, …).

**asset** — `finish_mesh(builder, pal, outline_class, rig=, grounding_height=)` (build → skin → materials → hull);
`ship_character(asset, token, category, rig, body, pal, clips, outline_class, game_ids, attachments, blob_radius)`
(bake, `SK_` + one `A_` per clip, manifest, full review set); `ship_static(...)`.

**pngio** — `write_png` (exact bytes, adaptive filters), `write_png_budget` (≤ 200 KB: truecolour → 256-colour
palette → downscale), `read_png`.

## 4. Skeleton (`SK_Humanoid` → `SKEL_Human`, `SKEL_Goblin`)

`root → pelvis → spine_01..03 → neck_01 → head`; `clavicle_l/r → upperarm → lowerarm → hand → thumb_01,
fingers_01`; `thigh → calf → foot → ball`; **`weapon_r` / `weapon_l`** (children of the hands, head = grip centre,
local **+Y = blade direction (forward in rest), +Z = up** → weapon meshes are modelled with the grip at the origin
and the blade along +Y, and attach to the UE sockets with an identity transform); IK helpers `ik_foot_root,
ik_foot_l/r, ik_hand_root, ik_hand_gun, ik_hand_l/r` (copy their FK bones via constraints, so clips carry them);
optional `jaw, eye_l, eye_r`; any chain via `HumanoidSpec.chains` (`cape_c_01..`, `ear_l_01..`, …).
Rest pose: A-pose (arms 50° down), legs straight with a 3° knee pre-bend, elbows 8°; limb bones rolled so local X
is the hinge. Every exported bone is flagged deform (exporter uses `use_armature_deform_only`); IK/weapon/root
bones carry no weights. Goblins: `HumanoidSpec(skeleton="SKEL_Goblin", rest_lean=0.3, rest_head=-0.28, …)
.fit_height(0.92)` — same names, hunched rest. Sockets in the manifest: `fx_feet, fx_chest, fx_head,
fx_overhead, fx_hand_l/r` (+ `fx_eye_l/r`), as bone-relative UE cm.

## 5. FBX + UE import

| File | Content | Exporter |
|---|---|---|
| `SK_*.fbx` | armature (object exported as `Armature`, so UE adds no extra root) + skinned mesh, bind pose, no anim, 2 material slots, custom normals, `AF_Data` colours | `bake_anim=False` |
| `A_*_<Action>.fbx` | armature only, one action, take named like the file, frames 0..N | `bake_anim=True`, all bones, no NLA, no "all actions", force start/end keys, step 1, simplify 0 |
| `SM_*.fbx` | mesh + `SOCKET_<name>` empties (+ `UCX_<SM>_NN`) | |

Common: axis forward −Z / up Y, no leaf bones, deform bones only, primary/secondary bone axis Y/X, face smoothing,
linear vertex colours. UE import: **Import Normals** (not Compute), Force Front X off, Convert Scene on, don't
import materials/textures (build from the manifest). Animations import **onto the existing skeleton**
(`SKEL_Human` …): an armature-only FBX carries no bind pose, so its node rest = the first frame — UE uses the
animated local transforms, which the smoke test verifies are identical to the source (≤ 0.01 mm, 0.0000°).

## 6. `manifest.json`

```jsonc
{ "schemaVersion": 1, "generator": "...", "units": {...},
  "shading": { tShade, tLight, bandWidth, toneConstants, rim, grounding, emissiveBoost, ink,
               keyLight: { fromYawUE, elevationDeg, dirToLightUE, dirToLightView }, camera: {...} },
  "palettes": { "<Family>": { material, parent, baseColor{name,file,srgb}, params{…channels}, filter, mips } },
  "gameIds": { "player_warrior": "SK_Hero_Warrior", … },
  "assets": { "SK_…": { kind, category, fbx, gameIds, skeleton, scale, heightCm, boundsCm{min,max}, bones,
      triangles{toon,outline}, materialSlots[{index,name,parent,palette,(class,widthCm,px1080AtDefaultDistance)}],
      sockets[{name,bone,relLocCm,relRotDeg,restLocCm}], blobShadowRadiusCm, attachments[{object,socket}],
      anims[{name,asset,fbx,lengthMs,frames,fps,loop,notifies[{name,ms}],contactMs,releaseMs,additive,refSpeedCmS}],
      previews[] },
    "SM_…": { kind, category, fbx, gameIds, boundsCm, triangles, materialSlots, sockets[{name,locCm,rotDeg}],
      footprintTiles, blocking, attachSocket, previews[] } } }
```

## 7. Smoke test (`tests/smoke_test.py`)

Builds `SK_Test_KitKnight` (a compact cousin of the warrior: plate + crimson tabard and two-sided cape, plumed great
helm with ember T-visor, sword `SM_Test_KitKnight_Sword` on `weapon_r`, heater shield on `weapon_l`; 3.8 k toon tris
+ 3.6 k hull; 39 bones incl. a `cape_c` chain), bakes Idle 1000 ms / Run 700 ms (no-slide gait at 3.35 m/s,
FootL/FootR) / Attack01 615 ms (Contact 308, warrior key poses), exports to a temp export root, re-imports into an
empty scene and checks: bone set, no extra root, scale, material slots, triangle count, hull faces, UVs, `AF_Data`,
weights (≤ 4, none unweighted), custom normals (hull outward), height in metres, per-clip frame counts and take
names, **posed bones identical to the source**, weapon sockets; manifest fields; shader conformance; PNG budgets.
Then renders `Art/Previews/SK_Test_KitKnight/{game, game_1080crop, closeup, turnaround, anims,
roundtrip_reimport}.png` — the last one is the **re-imported** mesh driven by the **re-imported** Attack01 at the
contact frame with the re-imported sword/shield on their sockets (what UE receives).

## 8. Known gaps / next steps

* Preview shows blob shadows only (no sun self-shadowing) — the UE desktop look adds the shadow term on `ndl`.
* `T_<Asset>_Detail` decal sheets (second UV set) are not implemented yet; small details are modelled geometry
  (crease strips via `Region.line_region()`, inset plates) for now.
* Skeletal LODs: Blender's FBX exporter cannot write an `FbxLODGroup`, so a generated LOD ships as its own file
  `SK_<Asset>_LOD1.fbx` (same armature + bind pose) listed in the manifest entry's `lods[]` (import as LOD 1:
  `USkeletalMeshEditorSubsystem::ImportLOD` / Interchange). The warrior has one; other SKs still rely on UE
  reduction. SM LOD groups are not generated yet. Hull budgets: see the warrior manifest `budget` record.
* The secondary-motion chains are keyed in the clips (mobile path); AnimDynamics setup is UE-side.

## 9. Hero generators (`heroes/`)

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/heroes/warrior.py          # ship (~4 min)
#   --no-previews / --no-verify      skip the review renders / the FBX re-import check
#   --quick                          mesh iteration: READY pose close-up, turnaround, game crop → scratch
#   --anims [Clip …] [--facing=se]   bake + contact sheets → scratch
#   --frames Clip:frame … [--facings=se,side,back]   large debug stills → scratch
```

| File | Content |
|---|---|
| `heroes/common.py` | shared by every hero: `Profile` (smooth web-ring lofts + surface `point`/`normal`), `surface_band` / `surface_strip` / `surface_dot` (trims that hug a loft), `thick_patch` / `slab2d` (closed solids from point grids), `split_faces` (keep concave grooves out of the hull), the **`BP` body pose** (web `HumanPose` in 3D: pelvis, lean/twist, IK foot/hand targets, item aims, cloth flow, whole-body `spin` about a `pivot`), `to_pose` (incl. natural hammer grip: the hand rolls with its item so the fist stays in line with the forearm, and the elbow swivels when a blade would run along the forearm; `off_fit` = strapped shield: the face turns so the forearm never points into it), **`SkinPoints`** (a vertex sample of the skinned meshes posed by linear-blend skinning in numpy straight from a Pose — the exact Armature deformation without a depsgraph, for solvers), `ground_clamp` (probe spheres or `SkinPoints`; `snap` rests the lowest point on the floor), `plant_feet` (raises a foot whose sole / toe / heel is under the floor), `body_clip` / `keyed` (web easing on BP channels — angles interpolate linearly, so spins and rolls pass 180°) |
| `heroes/qa.py` | per-frame geometry QA on the deformed meshes: lowest point of body / main-hand / off-hand item, sword × shield, sword × body, shield × body (grip zones excluded only against their own fist), cape × armour (outer skin, interior, below the pinned yoke), visor yaw / pitch on contact frames; `verdict()` against limits |
| `heroes/warrior.py` | `SK_Hero_Warrior` mesh (+ `lod=1` → `SK_Hero_Warrior_LOD1`), rig spec (cape / tabard / plume chains), `SM_Hero_Warrior_Sword` / `_Shield`, CLI |
| `heroes/warrior_clips.py` | READY + cloth solver (gravity-aware tabards, ground constraint on every chain, **cape envelope** against the skinned armour) + all 15 clips (Idle, Walk, Run @ 3.333 m/s, Attack01-03, Cast01-02, Cast_Whirlwind, Cast_Charge, Hurt, HurtAdd, Dodge, Death, Portrait); chest-relative authoring helpers (`chest`, `shield_guard`, `shield_at`, `look`) |
| `heroes/warrior_ship.py` | bake, **QA gate** (`qa.py`, limits `QA_LIMITS` / `QA_CLIP_LIMITS` / `QA_HEAD`: the ship exits 1 when a clip exceeds them), FBX export (SK, LOD1, A_ per clip, weapon SMs), manifest (anims, notifies, `fx` colours, portrait, `lods`, `budget`), re-import verification, review set in `Art/Previews/hero_warrior/` |

Conventions learnt on the warrior (apply them to the next heroes):

* **Concave details never get a hull.** An inverted hull inks every concave wall that faces away from the camera
  (a visor groove became a thick black bar over the ember). Model grooves with `profile_mesh(disp=…)`, then
  `split_faces` the displaced faces into an `outline=False` part; the hull's front side is culled anyway.
* **Cloth angles are world-space** (web `clothChain`): the cape hangs at `max(rest + flow, lean × w_k + 0.8 rest)`;
  `cling` (rolls, falls) wraps it along the curled back and `curl` tucks the hem under the hips. Two modelled crease
  strips per cape read as folds under 3-band shading. **Then the envelope** (`cape_push`): the real armour, skinned
  (`SkinPoints`: back plate, gorget, belt, skirt, helm; pauldrons, arms, gauntlets and the sword where they reach in
  behind the cloth), is projected into each chain's plane and every link turns back just enough to keep the lining
  ≥ 3 mm (torso) / 2 cm (limbs) off it — the rest offset of the cloth surface from the chain line (U-curl, folds,
  drape curvature) included, and what the bind pose already has (the drape hugging the shoulder blades) tolerated.
  A leaning, twisting back plate no longer pokes through the cape on contact frames.
* **Author upper-body targets on the chest** (`chest()`, `shield_guard()`, `shield_at()`): hand targets are
  body-space points, so a key that lunges, leans and twists leaves an absolute target *behind* the torso — the
  shield arm wrapped round the back through the cape. Carry the READY guard with the chest and open it from there.
* **Strapped shield** (`off_fit`): the forearm runs behind the boards, so the face is turned until the forearm points
  out of it by ≥ 0.15 (elbow behind the grip plane). **Hammer grip**: a blade parallel to the forearm drives the
  pommel into the vambrace — raised swords are held with the forearm level (high guard), sweeps end with the blade
  ~90° to the forearm; `_swivel_elbow` turns the elbow toward the blade when a bent arm allows it.
* **Contacts look at the target** (`look()`): the head counter-turns 80 % of the chest + hip turn and tips down at
  most 8°, so the visor and the shield emblem stay on screen on the hit-stop frame. **Each contact has its own
  silhouette**: Attack01 lunge + diagonal down-cut, Attack02 square wide stance with the blade level across the body
  and the shield arm flung out low, Attack03 the longest lowest lunge at full extension, Cast01 upright pointing at
  shoulder height.
* **Feet**: every upright clip runs `plant_feet` (no sole, heel or toe under the floor); rolls and falls clamp the
  whole body on the skinned meshes (`grounded`, `snap` for kneeling / lying: the death kneel rests on knee cop,
  greave and tucked toes).
* **Readability cheats are explicit pose choices**: blades roll ~50° in horizontal sweeps so the flat shows from the
  high camera; the warrior's guard blade sits 6° lower than the web's so the fist shows above the crossguard.
* Web hand targets "behind the head" are inside a 3D helm — keep raised hands ≥ 0.2 m off the head centre.
* **Cloth in spins**: a two-link panel (tabard) computes a *hanging* solution (pulled toward world down, tip link
  more than the root so it bends; the pull fades as gravity becomes exactly opposite, so it never flips) and a
  *wrapped* one (laid on the curled body), keeps whichever tip hangs lower (blended ±15°), and clamps both to the
  free wedge between thighs and torso. Every chain (cape, tabards, plume) is then kept above the floor (links that
  would end below z = clearance turn to the nearest angle that keeps them on it). Weapons are ground-clamp probes
  too (`weapon_probes`): a forward roll must not drive a forward-pointing blade into the floor — tuck it sideways.
* **Camera-aware proportions**: pitch −50° foreshortens the body but not the helm; the warrior helm loft is
  × 0.92 (`HELM_K`) with the freed height given to torso/legs (crown still 176 cm): 3.06 → 3.28 heads on screen.
* **Focal accent**: a *dark* `#0C0A12` slit (4.6 cm, shallow recess) with a thin ember line (1.5 cm, rgb(255,150) +
  a 5.5 mm rgb(255,210) core) on its lower lip — visible past the upper lip from the −50° camera — and a ≥ 3 cm steel
  gap under the narrowed brow band, so at 1080p it reads as a glowing eye slit, not a second gold band; the runtime
  `visorGlow` sprite is 12 cm, α 0.6 (`game_visorglow.png` checks it with/without). Raised sword hands go beside or
  above the helm (front, se and sw views), never in front of the visor; the Attack01 wind-up holds the fist at
  ≈ 1.8–1.9 m, above and just behind the crown (the arm's full reach while leaning back).
* **Weapons (spec §3.1)**: sword blade 80 cm guard → tip with a 2.7 → 2.0 cm diamond section and a ridge fuller
  (edge-on it stays readable), crossguard 28 cm drooping toward the grip; heater 36 × 60 cm.
* **Locomotion blade**: carried 35° outward and nearly down (165°) — any forward component cancels the outward one
  in the `se` projection and lays the blade over the near leg.
* **Portrait** (`T_UI_Portrait_*`, R11): `review.ink_render` (screen-space ink, 4.5 px silhouette at 512²) +
  `composite_glow` for the visor glow — no hull at 1:1; `portrait_zoom2x.png` is the 2× defect check.
* Previews: `game_poses.png` (key frames at native 1080p, W1 camera, UE ink width) is the readability check,
  `game_contacts.png` the four strikes' contact / release silhouettes at game size (se + sw);
  `poses_*.png` the intersection check; every clip has `anim_<Clip>.png` (se, notify frames boxed in orange) and every
  attack / cast (+ dodge, death) `anim_back_<Clip>.png` (ne / back / nw at pitch −50°, key poses + contact); `lod1.png`
  LOD0 vs LOD1; `weapons.png` includes a hilt close-up (pommel cabochon, down-swept guard). The ship prints the QA
  table (`[qa]`) and fails on any weapon intersection, any point > 5 mm under the floor, cape × armour over the yoke
  baseline, or a contact visor > 12° off the strike line.

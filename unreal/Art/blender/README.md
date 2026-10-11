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
col   = mix(col, #FFECC8, 0.55 * smoothstep(.56^4, .62^4, (1 - max(N·V,0))^4) * smoothstep(0, .05, ndl))
                                                                  banded rim on the lit half (shading.RIM_*)
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
  out along an angle-weighted smoothed normal shared by every corner at the same position (hard edges and seams
  never split the outline) by the class width (`kit.outline.WIDTH_CM`: hero/boss 1.8 cm, NPC 1.6, monster 1.5,
  small monster 1.4, interactive 1.3, weapon 1.3, decor 1.2), faces flipped, same skin weights (skins with the
  body), **custom normals = outward smoothed normal**, `AF_Data` = normal×0.5+0.5 with A = 0. UE: unlit, opaque,
  default back-face culling (draws the far side = the rim).
  * **Colour** `mix(#120C18 ink, line(c), o)`, `line(c) = mix(mix(c,(20,10,30)/255,.5),0,.55)`, `o` = palette
    `P.a` — never lighter than the region's line tone. Spec §1.3: characters ink, props their line tone →
    `outline.INK_MIX` (hero/boss/NPC/monster/weapon **0.4** = 60 % ink, hull lum ≤ 33 for any base colour;
    interactive/decor 1.0). Characters build their regions with `outline.ink_regions(REGIONS, "monster")`
    (the warrior's `HULL_INK_O`); `finish_mesh` warns about any hull swatch lighter than lum 40.
  * **Width = screen-constant WPO** (spec §1.3 "extrusion in clip space"). The far side of the hull is the body
    silhouette **dilated by exactly `OutlinePx1080 × ViewSizeY / 1080` pixels in screen space**:
    ```
    P = AbsoluteWorldPosition (excl. material offsets)   N = VertexNormalWS (the imported custom normal)
    V = normalize(P − CameraPosition)                     depth = TransformPosition(P, World→View).z
    Np = N − dot(N, V)·V                                  dir = Np / max(length(Np), 0.25)      (INK_EPS)
    width = OutlinePx1080 · 2 · depth · TanHalfFOV.y / 1080    (ViewSize cancels: ∝ viewport height)
    WPO = dir · width − N · BakedWidth
    ```
    `OutlinePx1080` = slot field **`outlinePx1080`** (`outline.SCREEN_PX_1080`, spec §1.3: hero/boss/weapon 3.5,
    NPC/monster 3.0, interactive 2.0, decor 1.2), `BakedWidth` = `widthCm`. At the W1 default distance 1 px at
    1080p = 0.833 cm (hero ink 2.92 cm). Normals within asin(0.25) ≈ 14° of the view ray (interior) fade
    linearly. A plain push along N (`N × (width − Baked)`, the formula until art review 6) reaches only
    sin∠(N, V) of the request: sheet edges, rims, blade bevels and plume strands tilted toward the camera lost
    ~30 % of their ink. **Mobile low** (no WPO): the baked hull alone, hero 1.8 cm = 2.16 px at the default
    distance (measured median ≈ 2.4 px on the warrior). The whole contract is in `manifest.json →
    shading.outline` (`outline.outline_manifest()`).
  * **Preview = the same formula**: `outline.set_preview_screen(obj, px=…|world=…)` (Geometry Nodes, per-vertex
    view depth, camera position / forward re-aimed before every `review.render_array` by `refresh_preview`, so
    poses, facings and attached weapons stay exact); `review.set_outline_px(meshes, px)` uses it; `px=None` /
    `set_preview_width(obj, m)` = the old push along N (the baked hull alone: `set_preview_width(obj, baked)`).
    `outline.preview_self_test()` checks it (sphere 3.50 px for a 3.5 px request; a 1 cm plate tilted 70°:
    3.50 px vs 3.15 px along N). **Root cause of the "missing ink" in review 3**: the preview group negated the
    custom normal (assumed it followed the flipped winding) — game views asked 2.9 cm got 0.7 cm.
  * **Nothing may stand proud of the ink.** A no-hull trim (band, seam, rivet, crease strip, gem) or a detailed
    part bulging out of its low-poly hull proxy eats the silhouette ink by its height (each mm = 0.12 px at 1080p).
    `outline.protrusion_report(obj)` lists them (`finish_mesh` prints every part > 2 mm). Fix with
    **`Builder.add(part, …, hull=proxy | None, hull_pad=True)`** + **`Builder.add(trim, …, outline=False,
    covered_by=<id>)`**: at build the proxy (or a copy of the part) is inflated locally (`mesh.pad_proxy`) until it
    encloses the part and every covered trim (+0.5 mm). Parts with `outline=False` (visor slits, gems, small
    emissive bits) get no hull; `hull=proxy` alone bakes the hull from a simpler shape (face attribute
    `af_hullonly`: the proxy's hull copy is kept, its own faces dropped) — mail rows, grooved helms, folded capes.
  * **Ink gate** (`review.ink_gate(class)`, `outline.GATE_*`): at native 1080p with the shipped width, ≥ 90 % of
    the silhouette edge pixels carry a rim ≥ `outlinePx1080 − 0.5` px (hero 3.0, NPC/monster 2.5, interactive
    1.5, decor 0.7) with a pixel darker than lum 40 (line-tone classes 64) within 3 px, and the solid hull pixels'
    lum p95 ≤ 40 before the bloom / glow-card post. Measured on every game-camera cell (`Review.game_shot(...,
    measure=True)`; `game_inkgate.png` marks each edge pixel green / red); `Review.assert_ink()` /
    `review.assert_ink_gate(reports)` raise `InkGateError`; `asset.ship_character` / `ship_static` record
    `inkGate` in the manifest entry and raise; the warrior ship exits 1.
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
name, outline=True, sub_regions={1: 'lining'}, hull=None, hull_pad=False, covered_by=None)` → part id (padded hull
proxies: see §2 Outline; `pad_proxy(part, points)`), `.cover(pid, part)` (pad `pid`'s proxy round a shape that is
not added — a detail modelled collapsed in the bind pose and scaled in by a clip), `.build(name, grounding_height)` → mesh object with
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
`SCREEN_PX_1080`, `INK_MIX`, `INK_CLASSES`, `INK_EPS`, `GATE_SLACK_PX` / `GATE_FRAC` / `GATE_DARK_LUM`;
preview (GN modifier, review only, stripped before export): `set_preview_screen(obj, px= | world=)` (the UE WPO),
`refresh_preview(scene)`, `set_preview_width(obj, m)` (push along N), `remove_preview`, `preview_self_test()`;
`screen_k`, `ink_width_world(px1080, depth)` (Python reference of the WPO width), `pixel_world_size`,
`default_px_at_game_distance`; colour: `hull_rgb(hex, o)`, `lum`, `ink_regions(regions, class)`,
`dark_limit(class)`, `hull_color_report(obj, pal, class)`; `protrusion_report(obj, min_mm, names)`;
`outline_manifest()`.

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
`save`), `skeletal_entry`, `static_entry`, `clip_entry`, `bounds_ue`. **`Manifest.save()` merges**: under an
exclusive lock (`manifest.json.lock`) it re-reads the file and writes only the `assets` / `palettes` / `gameIds`
keys this instance added, changed or deleted since it was loaded (other generators' entries are kept), atomically;
`shading` / `units` are rewritten from the kit. Always go through `Manifest`, never write the JSON directly.

**review** — `Review(asset, root, meshes, height, blob_radius, game_outline_px=, glows=, bloom=True,
outline_class=)`: `.game_view()` (`game.png` 640×360 at the W1 camera; `game_1080crop.png` native 1080p pixels —
shipped look next to the baked hull alone, each with its measured rim; `game_inkgate.png` the 3× edge map),
`.closeup()` (front 3/4), `.turnaround()` (8 facings, one scale), `.material_ab()` (full toon stack vs no rim / no
grounding + diff), `.contact_sheet(rig, [(clip, action)], frames=8)` (`anims.png`; notify frames get an orange
border + label). `game_outline_px=` (`ship_*` pass `SCREEN_PX_1080[class]`) renders game views at the UE
screen-constant ink and close-ups / sheets with the same ink in world units (`ink_world`: the shipped weight,
magnified); `.game_shot(border, measure=True, tag=)` = one game-camera cell (+ `post`: emissive **bloom**
`BLOOM_LOBES` and runtime **glow cards** `glows(cam, res)` with the web falloff `GLOW_STOPS`) measured by the ink
gate → `.ink_reports`, `.ink_verdict()`, `.assert_ink()`; module level `ink_gate(class)`, `INK_GATE`,
`ink_stats(body, hull, rgb, …)`, `coverage_pass`, `ink_gate_ok`, `ink_verdict`, `assert_ink_gate`,
`InkGateError`, `ink_map`, `emissive_pass`, `bloom`, `glow_card`, `blur`. Offline UI renders
(portraits, icons): `ink_render(meshes, res, silhouette_px, interior_px, depth_step)` — hulls hidden, the web's ink
drawn in screen space instead (constant-width `#120C18` silhouette + contour lines where a nearer surface overlaps
a farther one, found on a 2× depth pass; no hull saw-teeth or slivers at 1:1), `composite_glow` (bakes a runtime
FX sprite such as `visorGlow` into a texture that gets no bloom), `project_px`. Low level: `setup_render`,
`game_camera`, `frame_points`, `mesh_points`, `stage`, `render_array`, `draw_text` (5×7 font), `grid`, `FACINGS`
(`front`, `se`, `side`, `ne`, `back`, …).

**asset** — `finish_mesh(builder, pal, outline_class, rig=, grounding_height=)` (build → skin → materials → hull;
stores `af_parts`, prints `[ink]` hull-colour / protrusion warnings);
`ship_character(asset, token, category, rig, body, pal, clips, outline_class, game_ids, attachments, blob_radius,
ink_gate=True)` (bake, `SK_` + one `A_` per clip, manifest, full review set at the shipped ink, ink gate →
`inkGate` in the entry, raises `InkGateError`); `ship_static(..., ink_gate=True)`.

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
| `SK_*.fbx` | armature (object exported as `Armature`, so UE adds no extra root) + skinned mesh, bind pose, no anim, 2 material slots (1 for a mesh with no hull: never an empty outline slot), custom normals, `AF_Data` colours | `bake_anim=False` |
| `A_*_<Action>.fbx` | armature only, one action, take named like the file, frames 0..N (N + 1 keys); file timeline (`GlobalSettings` `TimeSpanStart/Stop`) = take (`AnimationStack`, `Takes/Take`) = 0..N / 60 s in exact FBX ticks, every key on its frame | `bake_anim=True`, all bones, no NLA, no "all actions", force start/end keys, step 1, simplify 0; then `fbxtime.set_clip_range` (the exporter hard-codes a 0..1 s timeline and truncates some frame times one tick short) |
| `SM_*.fbx` | mesh in its own space (node transform identity; held items are unparented for the export) + `SOCKET_<name>` empties (+ `UCX_<SM>_NN`) | |

Common: axis forward −Z / up Y, no leaf bones, deform bones only, primary/secondary bone axis Y/X, face smoothing,
linear vertex colours. UE import: **Import Normals** (not Compute), Force Front X off, Convert Scene on, don't
import materials/textures (build from the manifest). Animations import **onto the existing skeleton**
(`SKEL_Human` …): an armature-only FBX carries no bind pose, so its node rest = the first frame — UE uses the
animated local transforms, which the smoke test verifies are identical to the source (≤ 0.01 mm, 0.0000°).

## 6. `manifest.json`

```jsonc
{ "schemaVersion": 1, "generator": "...", "units": {...},
  "shading": { tShade, tLight, bandWidth, toneConstants, rim, grounding, emissiveBoost, ink,
               keyLight: { fromYawUE, elevationDeg, dirToLightUE, dirToLightView }, camera: {...},
               outline: { model, color{formula, ink, lineTint…, inkMixByClass}, bakedWidthCm{class}, outlinePx1080ByClass,
                          wpo{formula[], eps, params, meaning, atDefaultDistance, mobileLow, preview}, gate } },
  "palettes": { "<Family>": { material, parent, baseColor{name,file,srgb}, params{…channels}, filter, mips } },
  "gameIds": { "player_warrior": "SK_Hero_Warrior", … },
  "assets": { "SK_…": { kind, category, fbx, gameIds, skeleton, scale, heightCm, boundsCm{min,max}, bones,
      triangles{toon,outline}, materialSlots[{index,name,parent,palette,(class,widthCm,px1080AtDefaultDistance,
      outlinePx1080,outlineMix)}], inkGate{pass,cells,minFracRim,minFracDark,medianRimPx,maxHullLumP95,gate},
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
  `USkeletalMeshEditorSubsystem::ImportLOD` / Interchange). The heroes and the Chapter-1 monsters (§10) ship one;
  other SKs still rely on UE reduction. SM LOD groups are not generated yet. Hull budgets: see the warrior manifest `budget` record.
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
| `heroes/warrior_ship.py` | bake, **QA gate** (`qa.py`, limits `QA_LIMITS` / `QA_CLIP_LIMITS` / `QA_HEAD`: the ship exits 1 when a clip exceeds them), **ink gate** (25 gated 1080p cells, `inkGate` in the manifest; exit 1 on a failure), FBX export (SK, LOD1, A_ per clip, weapon SMs), manifest (anims, notifies, `fx` colours, portrait, `lods`, `budget`), re-import verification, review set in `Art/Previews/hero_warrior/` |

Conventions learnt on the warrior (apply them to the next heroes):

* **Concave details never get a hull.** An inverted hull inks every concave wall that faces away from the camera
  (a visor groove became a thick black bar over the ember). Model grooves with `profile_mesh(disp=…)`, then
  `split_faces` the displaced faces into an `outline=False` part; the hull's front side is culled anyway.
* **Nothing stands proud of the ink** (art review 6, ink gate): every no-hull trim is `covered_by=` the padded hull
  of the plate it sits on (`hull_pad=True`) — helm (brow band, seams, rivets, breaths), cuirass (ridge, seams, fauld,
  sigil), mail skirt (rows, belt), fist (knuckle plate + thumb: they stood 2 cm proud — the sword hand lost 2.5 px
  of ink), pauldron (rim, rivet), rerebrace (lame), greave (shin ridge), knee cop (rivet), sabaton (lames), plume
  (strands), cape (hem, creases), tabard (V trim), buckle; the shield has one padded slab hull over the whole heater
  (the emblem stood 5–8 mm proud of the rims edge-on), the sword's grip / pommel cover the wraps and the cabochon.
  `outline.protrusion_report` is empty for all three meshes; the cloth proxies' lens rim is `CAPE_LENS` 2 mm.
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
* **Focal accent**: a *dark* `#0C0A12` slit (5.8 cm, shallow recess) holding a 4.2 cm emissive ember graded
  `#FF8A2A` → `#FFB45C` (2.6 cm) → `#FFD08A` hot core (1.4 cm), and a ≥ 3 cm steel gap under the narrowed brow band,
  so at 1080p it reads as an ember-lit T-visor, not a second gold band; the runtime `visorGlow` card
  (manifest `fx.visorGlow`: additive, 10 cm `#FF8A2A` α 0.75 with a 3.5 cm `#FFD08A` core α 0.9, web light falloff,
  faded by how much the visor faces the camera) + bloom on the emissive regions are in every game-camera preview
  (`game_visorglow.png` checks with/without). Raised sword hands go beside or
  above the helm (front, se and sw views), never in front of the visor; the Attack01 wind-up holds the fist at
  ≈ 1.8–1.9 m, above and just behind the crown (the arm's full reach while leaning back).
* **Weapons (spec §3.1)**: sword blade 80 cm guard → tip with a hollow-ground diamond section (two facets per flat
  meeting at the fuller ridge, bevelled edges: one facet always faces the key light) and blade `shadowAmt` .15 (the
  shade band stays mid-light steel, never a dark iron bar), ridge fuller; crossguard 28 cm drooping toward the grip;
  heater 36 × 60 cm.
* **Locomotion blade**: carried 35° outward and nearly down (165°) — any forward component cancels the outward one
  in the `se` projection and lays the blade over the near leg.
* **Portrait** (`T_UI_Portrait_*`, R11): `review.ink_render` (screen-space ink, 4.5 px silhouette at 512²) +
  `composite_glow` for the visor glow — no hull at 1:1; `portrait_zoom2x.png` is the 2× defect check.
* Previews: `game_poses.png` (key frames at native 1080p, W1 camera, UE ink width, bloom, visor card) is the
  readability check, `game_contacts.png` the four strikes' contact / release silhouettes at game size (se + sw),
  `game_facings.png` idle / run from front, sw, ne, back — every one of these cells is **ink-gated** (25 cells;
  the ship exits 1 on a failure, `game_inkgate.png` shows the edge map); `material_ab.png` the rim / grounding A/B;
  `poses_*.png` the intersection check; every clip has `anim_<Clip>.png` (se, notify frames boxed in orange) and every
  attack / cast (+ dodge, death) `anim_back_<Clip>.png` (ne / back / nw at pitch −50°, key poses + contact); `lod1.png`
  LOD0 vs LOD1; `weapons.png` includes a hilt close-up (pommel cabochon, down-swept guard). The ship prints the QA
  table (`[qa]`) and fails on any weapon intersection, any point > 5 mm under the floor, cape × armour over the yoke
  baseline, or a contact visor > 12° off the strike line.

### Mage and rogue (`heroes/mage*.py`, `heroes/rogue*.py`)

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/heroes/mage.py     # ship (~6 min)
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/heroes/rogue.py    # ship (~6 min)
#   same CLI as the warrior, plus  --anims … --qa [--qa-parts] [--qa-frames=metric,…]   per-frame QA in scratch
#                                  --game Clip:frame:facing … [--inkmaps]   ink-gated 1080p crops; --inkmaps also writes
#                                  each cell's edge map and names the mesh part under every thin-ink edge pixel
```

| File | Content |
|---|---|
| `heroes/hero_base.py` | shared by the mage and the rogue (independent of the warrior's files): loft / painted-strip helpers, `shell_ring` (two-sided cloth ring + hull proxy; `hull_row_step` / `hull_cols`), `HeroAnims` (web → body space, chest-relative authoring, `look`, foot planting, `grounded` on the skinned meshes, cloth chain toolkit: lagged hanging chains, gravity in spins, obstacle `envelope`, floor constraint), `PairQA` (per-frame ground / item × body / cloth × body) |
| `heroes/hero_ship.py` | `HeroDef` + `cli`: bake, QA gate, FBX (SK, LOD1, `A_` per clip, item SMs), manifest (merging; `itemAttachments` incl. the rogue's clip-only bow swap), re-import verification, the review set with the ink gate on every game cell (a body lying away from the root slides the fixed 1:1 crop, never resizes it), portrait |
| `heroes/mage.py` · `mage_clips.py` · `mage_ship.py` | `SK_Hero_Mage` (robe skirt / sash / hood tip / beard / tome / bell-sleeve chains), `SM_Hero_Mage_Staff` + `SM_Hero_Mage_Crystal` (child item on the staff's `crystal` socket); 15 clips incl. `Cast_Blizzard` / `Cast_Meteor` (R5); skirt leg envelope, staff fit (cone search clear of robe / hood / beard / sleeves / floor), face-down Death drape |
| `heroes/rogue.py` · `rogue_clips.py` · `rogue_ship.py` | `SK_Hero_Rogue` (scarf A/B and hood-tip chains, tassets on the thighs), `SM_Hero_Rogue_Dagger` / `_Dagger_L` (mirror), `SM_Hero_Rogue_Bow` (flash-bow swapped into the off hand between `FX_BowShow` / `FX_BowHide` in `Cast_Multishot`), `SM_Hero_Rogue_Quiver` (body socket `quiver`); 14 clips |

Conventions learnt on the mage and rogue:

* **A robe skirt is four 2-link chains**: the legs push it out (an envelope of the skinned leggings / boots,
  shared by angular weight), the floor holds the chain ends ≥ 7 cm; a tall kneel keeps the thighs inside the cone
  (a sit-back kneel or a child's-pose fold drives the knees through the front panel). Face down (`spin` 86°), each
  link turns only a share toward world down (`SKIRT_FALL`), the top panel barely (`SKIRT_FALL_IN`: with four chains
  the hem ring between them would cut the calves), the sides droop to the floor and the front panel lies flat under
  the knees (`SKIRT_FLOOR_SPUN`, faded in by the spin) — the robe wraps the legs instead of flaring into a funnel.
* **Ink on cloth that bends**: a hull proxy on every other row cuts its chords into a bent cloth; a pinched hem
  lens has only outward and down-the-cloth normals, so the diagonal hem of a flared skirt kept ~80 % of the ink.
  The mage's skirt proxy (`skirt_hull`) swells over the gold hem band and curls under it in a quarter-round, the
  bell sleeves keep a proxy on every row and the gold cuff carries its own padded hull.
* **Held items in falls**: the per-frame cone search picks a different azimuth every frame once a shaft meets the
  floor (a flickering staff); from the topple key on the Death staff follows an authored world-space tip-over and
  settles onto the floor (`lay_staff`: azimuth + the pitch that rests the claw head and butt on the ground). A
  falling arm's bell sleeve rides back toward the elbow (`SLEEVE_SLIDE` scaled by the spin's gravity weight).
* **Budgets**: LOD0 toon 16–18 k, LOD1 ≤ 7.1 k toon, hull ≤ 0.6 × toon (the mage 0.60: skirt and bells keep
  full-row proxies); the drawn LOD0 total (toon + hull + items, the `budget.drawnLod0` record) is warrior 33.0 k,
  mage 28.8 k, rogue 34.6 k (the flash-bow, shown in one clip only, counted).

## 10. Monster generators (`monsters/`)

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/goblin.py          # ship (~2 min)
#   goblin_chief.py · goblin_shaman.py · slime.py · elite_crown.py — same CLI:
#   --quick (READY close-ups / turnaround / game crop → scratch) · --anims [Clip …] [--facing=se] · --no-previews · --no-verify
```

| File | Content |
|---|---|
| `monsters/mgeo.py` | geometry helpers: `grid_loft` / `split_by` (af_u/af_a regions), `SpineLoft` (hunched torsos, mantles, skirts), `ProfileLoft`, `surface_strap` / `surface_ring` (solid straps, belts, rims), **`surface_paint`** (single-sided painted strokes: fur, war paint, seams, twist lines — 2 tris per step), `surface_dot(low=True)` (25-tri studs / rivets), `flap`, `leaf_solid`, `feather`, `chain_strip`, `cone` |
| `monsters/goblins.py` | `GoblinKit`: `SKEL_Goblin` (kit humanoid builder, goblin proportions, the union of the family's chains: `ear_*`, `jaw`, `eye_*`, `loin_*`, `pelt_*`, `headdress_*`, `totem_feather_*`) and the shared body (skull, face, ears, pot-belly torso, limbs, fists, feet, loincloth); `skull_hide` / `torso_hide` cut skin under a helm / cuirass / shawl; `builder(…, lod)` + `LOD1_SKIP` |
| `monsters/mclips.py` | `GoblinAnims`: web `humanoidTracks` → BP poses (READY, thrust / overhead / cast attacks, recoil, backward death), no-slide walk, Stun, contact / windup notifies |
| `monsters/goblin.py` · `goblin_chief.py` · `goblin_shaman.py` · `slime.py` | the four SKs + held items (spear, cleaver, totem) |
| `monsters/goblin_hunt.py` | hunt-leader attachments (red cap, pendant, loot sack; manifest `variants`, scale 1.25) |
| `monsters/elite_crown.py` | `SM_FX_EliteCrown` (+ `crown_over_monsters.png`) |
| `monsters/mship.py` | ship: bake, FBX (SK, **LOD1**, `A_` per clip, item SMs), manifest (merging), per-frame QA, ink gate, re-import verification (`verify_export`, `verify_lod`), the review set (`lod1.png` = LOD0 vs LOD1 incl. the mobile-low look) |

Budgets (spec §1.9; `budget` in each manifest entry; toon = every toon-lit slot, the slime's dithered shell
included): goblin 6.1 k / LOD1 3.2 k, chief 9.0 k / 3.2 k, shaman 8.9 k / 3.4 k, slime 2.65 k / 1.47 k; hull ≤ 0.75 ×
toon. LOD1 `screenSize` 0.05: a monster is 0.10–0.20 at every W1 zoom, so desktop always draws LOD0; LOD1 is the
mobile-low gameplay mesh (skeletal LOD bias 1, baked hull only) and keeps every identity cue (eyes + pupils, grin,
teeth, ears, war paint, mask, headdress).

Conventions learnt on the monsters:

* **Budget where it is seen**: small knobs and trims are low-poly (a 1–2 cm stud is 2–3 px at 1080p), painted
  details are single-sided strips (`surface_paint`), and skin wholly under gear is cut (`skull_hide` /
  `torso_hide`; the hull is still baked from the closed shape) — the chief went 17.8 k → 9.0 k with no visible change
  in the game crops or the close-ups.
* **Clean bind pose**: details a clip scales in (the slime's X-eyes and angry brows) are modelled collapsed to 2 %
  about their bone and scaled by 1 / 0.02 in the clips; `Builder.cover` keeps their full-size shape inside the
  shell's padded ink.

## 11. NPC generators (`npcs/`)

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/npcs/npc_ship.py <Npc|all>   # ship (~2 min each)
#   <Npc> = Blacksmith | Merchant | QuestElder | PlainsHerbalist | PlainsWanderer | EscortTravelingMerchant |
#           RescueLostTraveler | WanderingMerchant | StashKeeper (or the snake-case preview name)
#   --quick [--pose=Clip:frame] [--facing=se] · --anims [Clip …] · --no-previews · --no-verify
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/npcs/npc_ship.py props      # SM_NPCKit_Prop_*
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/npcs/npc_ship.py dressing   # anvil stump + quest markers
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/npcs/npc_ship.py kit        # SK_NPCKit_* parts
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/npcs/npc_ship.py lineup     # R2 height lineup
```

| File | Content |
|---|---|
| `npcs/folk.py` | the townsfolk kit (spec §5.1, web `Townsfolk.ts`): `Look` (web `FolkLook`), builds slim / normal / stout / dwarf fitted to the R2 heights (1.72–1.76 m), the shared head (× `HEAD_K` 0.92) with modelled eyes / pupils / catch-lights / brows / nose / mouth / ears / blush, hair (short · long · bald · ponytail · bun · braid; a cap clipped by the web hairline, its sunk inside dropped), beards (stubble · short · full · long · braided · mustache; the hanging mass grows out of the jaw shell), hats (cap · headband · widebrim; `hat_tilt` / `visor_k` / `brim_k` are the W1 camera cheats that keep the face out of the brim's shadow), tops (tunic · vest · robe · coat; neckline opening, placket, hem, 4-chain skirt), belt, apron, mantle / scarf, travel pack (+ the peddler's pot and rug), spectacles, key ring, arms (bare arms = skin + tunic shoulder cap), fists round a grip or open mittens, legs and boots; `build(pal, rig, lod, lod_ratio)` = one merged `SK_NPC_<Id>`, `build_kit_part` = one part group on its own |
| `npcs/looks.py` | the nine Chapter-1 looks (§5.2 hex values + DECISIONS I7 stash keeper): `NpcDef` = look + work loop + READY tweaks + game ids, display names, runtime FX cards (`orbGlow`, `herbGlow`, `coinGlint`, `anvilSparks`), scenery, extra clips, fidget gesture |
| `npcs/props.py` | `SM_NPCKit_Prop_*` held props in weapon-bone space (hammer, tongs + hot bar, staff + orb, scroll, purse, herb pouch, sword, book), `SM_Prop_Camp_AnvilStump` (socket `strike`), `SM_UI_QuestMarker_Excl / _Quest / _QuestGrey` |
| `npcs/npc_clips.py` | `NpcAnims` (on `hero_base.HeroAnims`): READY per work style, `IdleNPC`, `IdleNPC_Fidget` (personal gesture), `Alert`, `Talk`, `Work_Hammer / Count / Read / Guard / Lookout`; escort `Idle`, `Walk` 1.2 m/s, `Run` 2.85 m/s (no-slide), `Hurt`, `Death`; rescue `Cower`; cloth (skirt chains with the leg envelope, hair tail, beard, scarf, pack bounce) |
| `npcs/npc_ship.py` | build (LOD1 rebuilt to the §1.9 5 k budget), bake, per-frame QA (`ToonGroundQA`: items' ground on their toon surface — a planted staff / sword point's hull under the floor is expected), FBX (SK, LOD1, `A_` per clip), manifest (`npcStates`, `fx`, `scenery`, `attachments`, `budget`, `lods`, `portrait`), re-import verification, review set in `Previews/npc_<id>/` with the ink gate on every game-camera cell, portrait `T_UI_Portrait_<Id>` (512², screen-space ink) |
| `npcs/npc_dressing.py` · `npcs/npc_kit_parts.py` | props (`Previews/npc_props/props.png`, leading face 3/4 front + 3/4 back), the anvil stump (static ship with `ground_occludes`: the hull under its foot is the terrain's), the quest markers (full camera billboard on `fx_overhead`; `Previews/npc_markers/markers_over_npc.png` on the elder + gated 1080p cells `markers_close.png`), the R2 lineup (`Previews/npc_family/`); the `SK_NPCKit_*` parts (each the merged mesh's own generator, on its look's rig) + `Previews/npc_kit/kit_parts_a/_b.png` |

Budgets (§1.9): LOD0 8.4–11.8 k toon, LOD1 ≤ 5 k (ratio scaled per look), hull ≤ 0.63 × toon, 36–50 bones. LOD1
`screenSize` 0.05 (as the monsters: desktop always draws LOD0, LOD1 is the mobile-low mesh). Ink: NPC class 3.0 px,
gate 2.5 px on ≥ 90 % of the edges of every gated 1080p cell (13–17 per NPC: poses, facings, extra clips).

Conventions learnt on the NPCs:

* **Faces at pitch −50°**: a cap visor or a wide brim rooted at the brow hides the eyes from the W1 camera (the eye →
  camera ray meets the underside of the peak). Hats are pushed back (`hat_tilt` 10–16°) and peaks shortened
  (`visor_k`); work loops that look down (counting, reading) bow the head ~5° less than the web.
* **Hands near the face**: shading the eyes = the wrist at the temple, the flat hand across the brow, the elbow out
  at shoulder height; hammer lifts travel up the arm's own side — no forearm or tool crosses the face in se / sw.
* **Beards**: the hanging mass starts inside the jaw (spine root under the skin), jaw-wide, bushiest under the chin —
  a mass whose top stands proud of the lip reads as a collar / funnel in the portrait.
* **Planted props** (staff foot, sword point) rest their toon surface on the floor; QA measures items on the toon
  slot (`ToonGroundQA`).
* **Hair and headbands are inked by the skull's padded hull** (`covered_by=` the skull), never by a hull of their
  own: the screen-constant ink pushes a hull sideways in screen space, and along a hairline at the temple / sideburn
  (where the skin beside it recedes from the camera) a separate hair shell's hull lay over the cheek as a dark wedge
  from the ear to the eye. The hairline also grows out of the skin (buried lower edge on the face side). Kit parts
  (no skull) keep their own proxy hull.
* **LOD1 keeps the face exact**: eyes, pupils, brows, mouth and mustache bypass the collapse decimation
  (`Folk._add_exact`; lower ring counts instead) and the hair cap is rebuilt coarse rather than decimated — a
  collapsed eye white sinks under the skin and a decimated hairline creeps over the eye.
* **Grounded falls** check the boots at full resolution (`NpcAnims.grounded`): every-2nd-vertex sampling missed a
  tucked toe cap by 1.6 cm.

## 12. Environment generators (`environment/`)

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/<family>.py [names…]
#   <family> = foliage | rocks | camp | story | interactive | goblin_camp     (names: substrings, e.g. Oak Outcrop)
#   (no flag)       ship: FBX + manifest entry (merged) + review set + ink gate per asset, then the family sheets
#   --sheet         iteration sheet (3/4 view + 1080p W1 crop ×2 per asset) → scratch
#   --quick         every asset in a row with the warrior (close 3/4 + native 1080p game crop) → scratch
#   --family        rebuild the family sheets in Previews/env_family/ only        · --no-previews
#   rocks.py --ridge    wall outcrops on the tile grid as the runtime places them (Previews/env_family/outcrop_ridge.png)
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/terrain.py    # texture set + manifest terrain section
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/vignette.py   # the plains vignette
```

| File | Content |
|---|---|
| `environment/envkit.py` | shared by the family (the kit is unchanged): `Lump` cloud lumps (union of spheres, `cull_hidden` drops faces inside a neighbour), `RockShape` (radial boulder: bumps, flat foot, `top_cut`, `boxy` superellipsoid, `centre`) + `rock`, `rock_cap` (moss / snow blanket with drips; `inner_shell=False` = open cap) + `cull_under_cap`, `drip_edge`, `ring_loft`, `blade`, `ribbon` (painted strips on a surface), `bloom`, `plank`, `spike` (`rows` > 2: rings for local hull padding), `rope_band`; `finish` (build → **spherised normals** per canopy lump → **wind data** `AF_Data.g/.b` + UV1 `UVWind` → toon + outline → hull); `ship` (FBX, merged manifest entry with `footprintTiles` / `blocking` / `outlinePx1080` / `outlineMix` / foliage parents / wind contract, review set with `ground_occludes`, ink gate recorded); `EnvManifest` (also merges the top-level `terrain` section); `family_sheet` (every variant at the W1 camera, native 1080p, the warrior for scale), `scene_shot`, `tile_hash` (= `AbyssWorldUtil::TileHash`), `place`, `import_warrior` / `toon_material_files` (shipped hero + palettes read from disk) |
| `environment/foliage.py` | `SM_Foliage_Plains_Oak_A/B/C` (`tree`), `_FruitTree_A/B` (`tree_round`), `_BerryBush_A/B` (`bush`), `_GrassTuft_A/B/C` (`grass`), `_FlowerPatch_A/B` (`flower`), `_RedMushrooms_A/B` (`mushroom_red`), `_LilyPad_A/B` (`lily_pad_plains`) — palette `Foliage_Plains`, parents `M_AF_Toon_Foliage` / `M_AF_Outline_Foliage`; ground cover (grass, flowers) has no hull and uses `MI_AF_Toon_Foliage_Plains_Ground` |
| `environment/rocks.py` | `SM_Rock_Plains_Cluster_A/B/C` (`rock`), `_MossBoulder_A` (`boulder`), wall tiles `SM_Outcrop_Plains_V0..V5` (`outcrop_plains`; V2 / V5 hedges), skirt `SM_Terrain_Plains_SkirtHill_A..C` (`skirt_hill_plains`, ground: no ink) |
| `environment/camp.py` | `SM_Palisade_Plains_Stake_A/B/C` (`palisade_plains`, 1.30 / 1.40 / 1.50 m, R1), `_GatePost` (`palisade_gate_plains`), level-design runs `_Straight` / `_Corner`; camp kit `SM_Prop_Camp_Campfire/Well/Tent/Banner/Barrel/Crate/Torch/Hearthstone` (`camp_<type>`; sockets `flame`, `glow`, `gem`) |
| `environment/story.py` | story decor `SM_Prop_Story_ElvenRuins` (`decor_ruins`), `_GoblinTotem` (`decor_war_banner`), `_BrokenAltar` (`decor_broken_altar`, R6 canonical), `_DryWell` (`decor_dry_well`); lore `SM_Prop_Lore_AncientTablet/TornJournal/CrystalShard/CarvedStone` (`lore_<spriteType>`) |
| `environment/interactive.py` | `SK_Prop_TreasureChest` (`SKEL_Prop_Chest`: root + lid, `A_Prop_TreasureChest_Idle` / `_Open` 400 ms, notify `ChestOpen`), `SM_Prop_GoldPile`, `SM_Prop_Event_RunePillar`, `SM_Prop_ExitPortal` (later chapters), `SM_Prop_ExitGate_Sealed` (W7), markers `SM_FX_ClueMagnifier`, `SM_FX_GuideArrow`, gather node `SM_Prop_Gather_EmeraldHerb` |
| `environment/goblin_camp.py` | spec §6.9 / R7: `SM_Prop_Goblin_Hut_A/B` (2 × 2), `_FirePit`, `_BoneTotem`, `_SpearRack`, `_Crates` |
| `environment/terrain.py` | `Art/Export/Terrain/`: `T_Terrain_Plains_Grass/Dirt/Stone/Camp_BC`, `T_Water_Plains_BC` (1024², one 3 × 3-tile period, painted from `Data/terrain_styles.json` like `GroundPainter`), `T_Terrain_Plains_Details_BC` (per-tile detail atlas), `T_Terrain_Plains_Noise_M`, `T_Water_Plains_Mask_M`; `composite()` = the exact web transition rules (`MF_TileBlend`); the UE material inputs (textures, colour spaces, UV rule, transition formulas, water constants) in `manifest.json → terrain.plains`; previews `Previews/terrain_plains/` |
| `environment/vignette.py` | the plains vignette (`Previews/env_vignette/`): every mesh **re-imported from its FBX** with the shipped palettes, placed like `AAbyssZoneActor` places it (outcrop variant / yaw from `tileHash`, palisade stakes by the neighbour mask, camp props facing the camp centre, lily pads on water tiles), the composited terrain with contact shadows, the warrior; `vignette_footprints.png` paints the W5 footprints from the manifest |

Previews: per asset `Previews/<SM_…>/` `game.png`, `game_1080crop.png` (shipped ink + bloom next to the baked hull alone),
`game_inkgate.png`, `closeup.png`, `turnaround.png` (the chest adds `anim_Idle.png`, `anim_Open.png`); family sheets in
`Previews/env_family/` (`foliage_trees`, `foliage_fruittrees`, `foliage_small`, `rocks_outcrops`, `outcrop_ridge`,
`camp_palisade`, `camp_kit`, `camp_kit_small`, `story_decor`, `lore_props`, `interactive_props`, `exit_gates`,
`goblin_camp`); `Previews/terrain_plains/`; `Previews/env_vignette/`.

Ink: decor class (line tone, `INK_MIX` 1.0) at 1.2 px — **1.4 px for `Props_Plains` decor** (`envkit.PX_OVERRIDE`;
poles and stakes are 8–12 px wide at 1080p and their lit edge carries the warm rim), interactive props 2.0 px; every
gated cell is a native-1080p W1 crop measured with the terrain occluding the hull under the floor
(`ground_occludes`). Every entry records `footprintTiles` + `blocking` (W5: trees, boulders, outcrops, stakes, tents,
wells, barrels, crates, tall story decor and the goblin huts / totem / rack / crates block; ground cover, mushrooms,
bushes, campfire, banners, torches, lore props, markers and the exit gate do not — the hero walks onto the exit tile
for the W7 message). Budgets: oak 4.4 k toon, fruit tree 3.4 k, bush 2.2 k, outcrop 1.5–1.8 k, ground cover ≤ 360.

Conventions learnt on the environment:

* **Canopies want spherised normals** (spec §6.5): each cloud lump's shading normals are blended 85 % toward its
  ellipsoid's (`finish(spherise=…)`), so the lobed silhouette keeps its scallops while the cel bands sit on the lump
  as one clean crescent; at 60 % the lobes facet into crumpled-paper bands.
* **Wide crowns**: the web oak is wider than tall (7 lumps over 115 px); a ring of 5 light lumps round a dark core
  with an upper ring and one crowning lump reads as the web tree from every yaw — a vertical stack reads as a column.
* **Flat faces turned from the key light go wholly to the shade band** (a stele, a gable): pillow them (a loft with a
  superellipse section, exp ≈ 3) so the three bands spread across the face. Bulging the perimeter of an n-gon does
  not work — the face needs interior vertices.
* **Thin poles over bright grass**: a 1.4 px line-tone ink anti-aliases to lum ≈ 70 on a lit pole edge; regions of
  thin parts take `o` 0.3–0.6 (darker toward ink) — banner crossbar, poles, ropes, pegs.
* **Bloom eats ink**: emissive regions (crystal, portal, herb bud) bloom over their own outline; keep their `e` low,
  their `o` near ink and let the glow come from the runtime cards / Niagara.
* **Open shells need a closed hull proxy** (`Builder.add(hull=proxy, hull_pad=True)`): a solidified hut dome with a
  door gap split its own hull (60 % rim); the proxy must hug the shell (+1 cm) — padding inflates it where needed, an
  oversized proxy draws a dark crescent.
* **Hull padding is per proxy vertex**: a 3-ring post padded round its lashings grew a dark slab along its whole
  height; `spike(rows=14)` keeps the growth local. Square lashings align their superellipse corners with the post's.
* **Caps**: moss blankets are open (`inner_shell=False`) and the rock under them is culled (`cull_under_cap`):
  wall tiles are many (≈ 800 in the plains), 3.8 k → 1.6–1.8 k toon with no visible change.
* **Ground-like meshes and the rim**: the banded rim lights any slope tilted > 24° away from the W1 camera; the
  skirt mounds keep every slope ≤ 22° (h ≤ 0.26 × the short radius, apron ≤ 10°) and no light band (`l` 0).
* **Water props float**: lily pads ride 1.6 cm over their pivot (UE puts the pivot 0.5 cm over the opaque water) —
  at the surface the near half of the ink would be under the water.

## 13. Items: equipment weapons and ground loot (`items/`)

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/items/weapons.py     # ship (~4 min)
#   --only sword,axe · --quick <families> (studio + tiers + in-hand cells → scratch) · --lineup · --no-previews · --no-verify
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/items/loot.py        # ship (~5 min)
#   --only bag,gold,potion,scroll,fruit,gem,quest,coin (subset; family sheets only on a full ship) · --quick [groups]
```

| File | Content |
|---|---|
| `items/itemkit.py` | shared: item data (`Data/item_bases.json`), the tier ladder (`weapon_tier`: levelReq ≥ 1 T1 · 6 T2 · 15 T3 · 20 T4 · 28 T5 · 35 T6, `w_rusty_sword` Worn), `plan_roles` + `AtlasVariant` (tier atlases = the `Weapons` atlas with the role swatches recoloured → `T_AF_Palette_Weapons_<Tier>_BC/_P`, `MI_AF_Toon_Weapons_<Tier>`), heroes re-imported from their shipped FBX (`import_hero`, `hero_pose`, `attach` = UE socket convention), studio cells, `GameCells` (W1 1080p 1:1 ink-gated cells, `ground_occludes`), the quality-presentation composite (`tint_body`, `add_glow`, `add_beam`, canonical `QUALITY`), `ship_static_item`, `verify_fbx`, **`revolve`** / `lathe_y` / `ring_y`, `loft_rings`, `surface_strip` |
| `items/weapon_tiers.py` | role colours per family and tier from the web icon variant tables (`ItemIcons.ts` SWORDS / DAGGERS / BOWS / staff heads / shield kinds / drawAxe / drawMace / wandSpec); higher tiers make the web's glow accents emissive roles |
| `items/weapons.py` | `SM_Weapon_<Sword·Axe·Mace·Dagger·Bow·Staff·Wand·Shield>` (Previews `SM_Weapon_<F>/`: `closeup`, `tiers`, `game_inhand` = READY + Attack01 contact on the warrior / mage / rogue, ink-gated, `game`, `game_inkgate`; `weapons_lineup/lineup.png`) |
| `items/loot.py` | ground loot (spec §7): `SM_Loot_Bag`, `SM_Pickup_GoldCoin` (VFX), `SM_Pickup_Gold_S/M/L`, `SM_Pickup_PotionHP_S/–/L`, `SM_Pickup_PotionMP_S/–`, `SM_Pickup_Antidote`, `SM_Pickup_Scroll_TP/ID`, `SM_Pickup_LeyFruit`, 17 × `SM_Pickup_Gem_<Type>_<Tier>`, `SM_Pickup_Quest_Herb/Gel/Pendant` (Previews `<asset>/` + `loot_family/`) |

**Weapons** (DECISIONS I6 / R4): one mesh per core `WeaponType`, modelled in its socket's bone space (grip at the
origin, blade / shaft +Y, shields face +Y like `SM_Hero_Warrior_Shield`, bows limbs ±Z with the string toward the
archer), attached with an identity transform; `heldBy: []` makes them the fallback of `FindWeapon` (a hero's own
`SM_Hero_*` weapon still wins for its class). Manifest: `weaponType`, `attachSocket`, `tiers` (tier → MI),
`bases` (item base id → tier), `tierNames`, `inHandLowestCm` (lowest point of the weapon in each tested pose).
UE: swap slot 0 to `tiers[bases[<equipped baseId>]]`; slot 1 stays `M_AF_Outline` (weapon ink 3.5 px).

**Ground loot**: palette `Pickups`, category `Pickups`, outline class **interactive** (line-tone ink 2.0 px, gate
1.5 px), pivots at the ground contact, readable from any yaw (props spawn at a hashed yaw and bob 0–12.8 cm). Every
entry carries a `loot` record — `tint` (only the bag: Tint × 0.85 in the canonical palette, normal untinted),
`qualityPalette`, `beam` (spec §8.7 pool / pillar per quality), `bob` — and the sockets `beam` (ground centre),
`label` (name plate, top + 18 cm), `glint`. Game ids: `loot_bag`, `potion_drop_hp|mp[_s|_m|_l]`, `gold_pile_s|m|l`
(+ `goldAmount` bands), `fx_coin`, `quest_item_<kind>`, and `loot_<baseId>` for gems / potions / scrolls / ley fruit
(the world resolves `loot_bag` for every ground item today; per-base meshes are an opt-in lookup).

Conventions learnt on the items:

* **`kit.mesh.lathe` / `sweep` collapse an end ring when the first or last two profile points share a height** (the
  end tangent is zero, so the ring loses one axis): open caps and flipped hull triangles that the preview draws as
  dark wedges across the surface (and UE would cull). Flat shoulders, coin faces and bands use **`itemkit.revolve`**
  (explicit rings via `loft_rings`, outward normals); `lathe_y` / `ring_y` are built on it.
* **Small props and the inverted hull**: a 1.3 cm baked hull is a third of a 6 cm coin; on thin flat parts its far
  side stands 1.3 cm above the face and reads as a heavy crescent in a magnified close-up. In game the WPO makes it
  2 px everywhere — so pickup close-ups draw the ink in **screen pixels** (3 px), not the kit's world-unit close-up
  ink. Padding a hull proxy round a part that stands far proud of a single fan vertex inflates it into a tent (the
  locket's ruby): such details get their own hull.
* **Flat insets of a concave outline cross themselves** (`offset_closed` at the heart's tip and notch): pillow lofts
  scale rings toward the centre instead.
* **Off-hand items are reviewed from sw** (the holding hand faces the camera); hero + item cells are gated together
  with `ground_occludes` (the hull under a planted sole is the terrain's, as the hero ships measure it).
* **Glowing gems and the dark check**: bloom over a near-white stone lifts its line-tone ink above lum 64 — the
  diamond regions use a darker hull mix (`o` .7) and the perfect cuts a modest table emission (.22).
* **Budgets**: weapons 0.7–3.5 k toon (+ 0.24–1.4 k hull); pickups 0.7–3 k toon (+ ≤ 1.5 k hull), the gold heap
  5.6 k (22 coins on a mound, hull from the mound proxy only) and the herb bundle 3 k + 3 k hull (15 leaves, each
  its own ink); gems 28–70 tris; heap coins 10 sides, loose coins 12 (a 6 cm coin is ~7 px at 1080p).

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
gate → `.ink_reports`, `.ink_verdict()`, `.assert_ink()` (opt-in `.ink_ignore` = callable(border) → bool mask of
edge pixels the gate leaves out: an intentional no-hull light line such as a bow string); module level `ink_gate(class)`, `INK_GATE`,
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
palette → downscale), **`write_png_native`** (≤ 200 KB per file **truecolour at scale 1**: never quantised or
resampled — split into `<stem>_2.png` … tiles along the sheet's pad lines, optional per-tile `annotate`, stale tiles
removed, every file checked by `assert_native` / `png_info`; raises `NativePreviewError` when even 8 tiles do not fit),
`read_png`. Use the native writer for anything labelled 1:1 (art review 9: the budget fallback turned crimson brown and
shrank a "1:1" crop to 0.85×).

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
  other SKs still rely on UE reduction. Static meshes follow the same convention where the spec asks for a LOD1 (the
  plains trees: `SM_<Asset>_LOD1.fbx` + `lods[]`, `envkit.export_lod1`); the content build imports skeletal LODs only
  so far (`Scripts/abyss_content/ue/meshes.py import_static` ignores `lods`). Hull budgets: see the warrior manifest
  `budget` record.
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
| `heroes/hero_ship.py` | `HeroDef` + `cli`: bake, QA gate (+ the per-hero `extra_qa` hook: metrics into the manifest's `qa.metrics`), FBX (SK, LOD1, `A_` per clip, item SMs), manifest (merging; `boundsCm` in the bind pose; `itemAttachments` incl. the rogue's clip-only bow swap; `blobShadowNote`), re-import verification, the review set with the ink gate on every game cell (a body lying away from the root slides the fixed 1:1 crop, never resizes it), portrait |
| `heroes/mage.py` · `mage_clips.py` · `mage_ship.py` | `SK_Hero_Mage` (robe skirt / sash / hood tip / beard / tome / bell-sleeve chains), `SM_Hero_Mage_Staff` + `SM_Hero_Mage_Crystal` (child item on the staff's `crystal` socket); 15 clips incl. `Cast_Blizzard` / `Cast_Meteor` (R5); skirt leg envelope, staff fit (cone search clear of robe / hood / beard / sleeves / floor), face-down Death drape |
| `heroes/rogue.py` · `rogue_clips.py` · `rogue_ship.py` | `SK_Hero_Rogue` (scarf A/B and hood-tip chains, tassets on the thighs), `SM_Hero_Rogue_Dagger` / `_Dagger_L` (mirror), `SM_Hero_Rogue_Bow` (flash-bow swapped into the off hand between `FX_BowShow` / `FX_BowHide` in `Cast_Multishot`), `SM_Hero_Rogue_Quiver` (body socket `quiver`); 14 clips |

Conventions learnt on the mage and rogue:

* **A robe skirt is four 2-link chains**: the legs push it out (an envelope of the skinned leggings / boots,
  shared by angular weight), the floor holds the chain ends ≥ 7 cm; a tall kneel keeps the thighs inside the cone
  (a sit-back kneel or a child's-pose fold drives the knees through the front panel). Face down (`spin` 86°), each
  link turns only a share toward world down (`SKIRT_FALL`), the top panel barely (`SKIRT_FALL_IN`: with four chains
  the hem ring between them would cut the calves), the sides droop to the floor and the front panel lies flat under
  the knees (`SKIRT_FLOOR_SPUN`, faded in by the spin). That hanging solution only *starts* the face-down drape
  (art reviews 7–8: it still stood open as a rigid cone, hem ≈ 2× the hips, boots in the opening like a lampshade):
  `drape_skirt` solves the eight links by Gauss-Newton on the skinned skirt vertices — gravity, the lower skirt
  collapsing sideways onto the legs (`DRAPE_W["lat"]`), every vertex ≥ `DRAPE_FLOOR` (1.4 cm) and ≥ `DRAPE_CLEAR` off
  tapered leg capsules (cloth *under* a prone leg too, `DRAPE_UNDER`), three starts (the hanging solution, the
  typical correction `DRAPE_START`, the previous frame's drape) — so the back panel lies on the calves, the sides
  and front on the floor and the hem ring wraps the boots. QA metric `deathHemHipRatio` (`mage_ship.death_qa`, the
  `HeroDef.extra_qa` hook, manifest `qa.metrics`): hem width ≤ 1.25 × hip width over the held face-down frames
  (shipped 1.12–1.13; the HEAD ship stood at ≈ 2×), hem ≥ 1 cm above the floor (shipped 4 cm).
* **Upright robe panels lie on a lifted knee** (`hero_base.envelope_min_reach`, front / back chains only): the
  greedy envelope keeps the upper link minimal, so a lunging thigh just past the chain joint fanned the hem link out
  at the thigh's angle — a flat tray 30 cm past the foot on the Attack03 hit-stop. The upper link may open further
  when that lets the hem link hang back (hem ≥ 5 cm closer, the hem link clearing its points by 1.5 cm more for the
  ring between chains); kneels and spun poses keep the greedy envelope. Leg points in the pinned waist band
  (`SKIRT_YOKE_K`) no longer drive the chains (a 1 cm deficit 2 cm under the root asked for a 40–120° link), and
  the band's skin weights stay on the pelvis down to the chains' root (k .09 → .21): chain weight *above* the root
  folded the band inward under the sash (a dark lens of the lining side).
* **Robe rim wash** (W1 camera 50° up: a vertical cloth side faces the view at N·V ≤ 0.64, so the banded rim covered
  a third of the skirt in pale grey-lavender): the outer skirt's exported custom normals are tilted up
  (`mage.SKIRT_NORMAL_LIFT` 0.25, n + 0.25·Z — the only camera-invariant bias, the hero turns under a fixed
  camera). Rim area −30 %, the indigo kept; the trade is a little more light band.
* **Face openings** (mage, rogue): the hood's opening is a displaced loft grid; the cells straddling the edge made
  recess walls that stepped out past the rim trim as dark squares. `hero_base.snap_open_edge` slides their
  undisplaced vertices onto the edge curve so every wall ends under the rim. The mage's face sits 2.6 cm deep and
  narrower (a ring of the dark hood interior frames it) with the shade band down over the eye line.
* **Sag bones keep their rest roll**: the bell sleeves' `sleeve_*_01` is aimed with its own rest +Z carried by the
  forearm — aimed with the forearm's +Z it twisted against its rest roll and the bell pinched into an hourglass
  (a thin dark 'post' between the upper sleeve and the bell at 1080p).
* **Held items read in the se view**: the se camera sits between the hero's front and right, so a blade held
  forward-right points at the camera (edge-on stub) and one pointing forward-down overlays the legs. The rogue's
  blades are authored by elevation / azimuth (`RogueAnims.dir_kw`): READY main blade raised 32°, out-and-back 125°
  (screen-left) with its flat rolled to the camera; contacts cut out to the right (Attack01) or fling both blades
  wide (Attack02); the mage's staff stands out at the right side (`STAFF_HAND`, `STAFF_LAT` −0.17) so the crystal
  is beside the hood, not in front of the face. Death relaxes the rogue's grips (`lay_blades`: blades turned into
  the floor plane, flats down, like the warrior's sword).
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
  mage 29.1 k, rogue 34.6 k (the flash-bow, shown in one clip only, counted).

## 10. Monster generators (`monsters/`)

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/goblin.py          # ship (~2 min)
#   goblin_chief.py · goblin_shaman.py · slime.py · elite_crown.py — same CLI:
#   --quick (READY close-ups / turnaround / game crop → scratch) · --anims [Clip …] [--facing=se] · --no-previews · --no-verify
#   goblin_chief.py --portrait   (T_UI_Portrait_GoblinChief + its 2× check → scratch and Art/Export/Portraits)
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/lineup.py          # R2 lineup (shipped FBX)
```

| File | Content |
|---|---|
| `monsters/mgeo.py` | geometry helpers: `grid_loft` / `split_by` (af_u/af_a regions), `SpineLoft` (hunched torsos, mantles, skirts), `ProfileLoft`, `surface_strap` / `surface_ring` (solid straps, belts, rims), **`surface_paint`** (single-sided painted strokes: fur, war paint, seams, twist lines — 2 tris per step), `surface_dot(low=True)` (25-tri studs / rivets), `flap`, `leaf_solid`, `feather`, `chain_strip`, `cone` |
| `monsters/goblins.py` | `GoblinKit`: `SKEL_Goblin` (kit humanoid builder, goblin proportions, the union of the family's chains: `ear_*`, `jaw`, `eye_*`, `loin_*`, `pelt_*`, `headdress_*`, `totem_feather_*`) and the shared body (skull, face, ears, pot-belly torso, limbs, fists, feet, loincloth); `skull_hide` / `torso_hide` cut skin under a helm / cuirass / shawl; `builder(…, lod)` + `LOD1_SKIP` |
| `monsters/mclips.py` | `GoblinAnims`: web `humanoidTracks` → BP poses (READY, thrust / overhead / cast attacks, recoil, backward death), no-slide walk, Stun, contact / windup notifies |
| `monsters/goblin.py` · `goblin_chief.py` · `goblin_shaman.py` · `slime.py` | the four SKs + held items (spear, cleaver, totem); the chief also ships `T_UI_Portrait_GoblinChief` (Portrait pose, manifest `portrait`) and inks as a **boss** (spec §1.3: 3.5 px, gate 3.0 px) |
| `monsters/goblin_hunt.py` | hunt-leader attachments (red cap, pendant, loot sack; manifest `variants`, scale 1.25) |
| `monsters/elite_crown.py` | `SM_FX_EliteCrown`: the web three-point front crown + low back band, diamond ruby, 542 toon + 336 hull tris (+ `crown_over_monsters.png`) |
| `monsters/lineup.py` | `Previews/mon_family/lineup_game.png` (+ `lineup_inkgate.png`): the warrior, the four monsters and a hunt leader ×1.25 with cap + crown, re-imported from their FBX, W1 camera, native 1080p, each figure ink-gated with its class gate |
| `monsters/mship.py` | ship: bake, FBX (SK, **LOD1**, `A_` per clip, item SMs), manifest (merging), per-frame QA, ink gate (+ the fraction at the class width itself, `inkGate.specPx` / `minFracRimSpec`), re-import verification (`verify_export`, `verify_lod`), the review set (`lod1.png` = LOD0 vs LOD1 incl. the mobile-low look, labels = the shipped LOD's tris; `poses_close_a/_b.png` 5 cells each at 1:1), `portrait_shot` (UI portraits of monsters) |

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
* **Cloth × limb QA** (art review): every goblin ship gates its cloth against the limbs with limit 0 intersecting
  triangle pairs on every frame — `QA_PAIRS` in each script (goblin / chief loincloth × legs + knee caps / guards,
  chief mantle + pelt × elbows / forearms / fists, shaman hide skirt × legs, shawl × forearms; the upper-arm tubes
  root under a mantle by construction and are not paired). Fixes that worked: the loincloth envelope's leg spheres
  sized to the knee caps (`flaps(thigh_r=…)`), the shaman's **skirt envelope** (`ShamanAnims`: the skinned skirt
  and legs tested in the pelvis frame, the front / back chains turned out until the legs are inside), side panels
  weighted onto the thighs, a slimmer thigh root under a tight waist band (`add_leg(top_up, top_r)`), free fists
  kept ahead of / below a mantle or shawl hem, and shallower death buckles (a deep knee bend lifts the knee to the
  belt, through any front panel).
* **Faces at the hit-stop frame**: a helmed or masked head hunched over a lunge shows only its dome to the W1
  camera; the contact key counter-pitches the head (chief −60° on a 21.8° lean) and the chief's Stun lolls face-up
  (`stun(head_droop, head_roll, head_nod)`).

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
| `npcs/looks.py` | the nine Chapter-1 looks (§5.2 hex values + DECISIONS I7 stash keeper): `NpcDef` = look + work loop + READY tweaks + game ids, display names, runtime FX cards (`orbGlow`, `herbGlow`, `coinGlint`, `hotBarGlow`, `anvilSparks`; depth-tested), scenery, extra clips, fidget gesture |
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
* **Hands near the face** (art review 9): W1 projects anything *before* the face down the screen over it, and the NPC
  turns to the hero, so the near hand is se's (right) or sw's (left) — every gesture keeps eyes, nose and mouth
  clear in both. The Alert greeting goes out beside the head (≥ 0.35 m lateral, crown height, no forward offset; the
  runtime holds that frame while the hero is near); a right-hand gesturer's Talk beats go out past the shoulder
  (|x| ≥ shoulder + 0.2, ~10 cm forward) so the open hand leaves the torso silhouette in se; the hat tip takes the
  brim at its side. Shading the eyes (`Work_Lookout`): elbow out (≥ 0.30 m) and back, the forearm running sideways
  along the upper brow, the flat hand on the forehead ~9 cm above the eye line, carried by the head (`head_pt`). The
  two-bone IK refuses an elbow behind the head plane ("never bend backwards" against the rest hinge): a clavicle
  swing back (−22°) lets it. Hammer lifts travel up the arm's own side.
* **Small hanging hardware gets per-piece hulls**: one padded proxy round the stash keeper's key ring, shafts and
  bits grew a dark slab 2–3× the keys; every piece (tab, ring, bows, shafts, bits) now carries its own unpadded hull,
  in aged brass `#B08A3A` (a flat bright ring with keys under one side read as the `#F1C40F` '?' turn-in marker).
* **Runtime glow cards are depth-tested**: the previews fade every card (`orbGlow`, `herbGlow`, `coinGlint`,
  `hotBarGlow`, the strike flash) by an occlusion test (`npc_ship.card_visibility`: 7 rays to the core, hits > 8 cm
  in front hide it) — the smith's hot bar no longer glows through his shoulder in ne; each manifest `fx` entry says so
  (`depthTest`).
* **Wide brims** (`brim_k` ≈ 1, `brim_curl`, `brim_droop`): narrower brims read as bowlers; the eyes are kept by
  `hat_tilt` and the front tip alone. The wandering merchant's brim droops at the sides and back (felt), the escort's
  is flatter and wider (the two peddlers no longer twin in silhouette).
* **Escort locomotion**: arms counter-swing the legs (they had swung with the same-side leg: a pacing waddle); the run
  leans ~10° (hips 5° + spine 13°) with the head held level under the brim, 4.5 cm bob, knees that kick the coat hem.
  The face-down Death drapes the coat with the mage's shares (`SKIRT_FALL`, front panel flat on the floor) and topples
  through a key at 44° (no 80 ms pop); before a spin anything forward of the body ends up under it — keep the hands
  level with the chest front.
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
| `environment/envkit.py` | shared by the family: `Lump` cloud lumps (union of spheres, `cull_hidden` drops faces inside a neighbour; icosphere `subdiv` 3 = 320 tris, 4 = 1280), `RockShape` (radial boulder: bumps, flat foot, `top_cut`, `boxy` superellipsoid, `centre`) + `rock`, `rock_cap` (moss / snow blanket with drips; `inner_shell=False` = open cap) + `cull_under_cap`, `drip_edge`, `ring_loft`, `blade`, `ribbon` (painted strips on a surface), `bloom`, `plank`, `spike` (`rows` > 2: rings for local hull padding), `rope_band`; `finish` (build → **spherised normals** per canopy lump (`lump_info(…, lift)`: + a tilt toward +Z) → **wind data** `AF_Data.g/.b` + UV1 `UVWind` → toon + outline → hull); `cloth_wind` (prop cloth weights: 0 at the hanging line → 1 at the tail, poles rigid); `ship` (FBX, merged manifest entry with `footprintTiles` / `blocking` / `outlinePx1080` / `outlineMix` / foliage parents / wind contract — `wind_slot=PROP_WIND_SLOT` for prop cloth —, review set with `ground_occludes`, ink gate recorded); **`EnvReview`** / `save_native` (every env preview truecolour at scale 1, `pngio.write_png_native`; the 1:1 game crop fitted to the asset's projected bounds + 28 px instead of the kit's square box); `export_lod1` / `lod_preview` (static LOD1 FBX + `lods[]` + `lod1.png`); `EnvManifest` (also merges the top-level `terrain` section); `family_sheet` (every variant at the W1 camera, native 1080p, cells sized from the assets' projected bounds incl. the scale warrior, short labels), `scene_shot`, `tile_hash` (= `AbyssWorldUtil::TileHash`), `place`, `import_warrior` / `toon_material_files` (shipped hero + palettes read from disk) |
| `environment/foliage.py` | `SM_Foliage_Plains_Oak_A/B/C` (`tree`, + `_LOD1`), `_FruitTree_A/B` (`tree_round`, + `_LOD1`), `_BerryBush_A/B` (`bush`), `_GrassTuft_A/B/C` (`grass`), `_FlowerPatch_A/B` (`flower`), `_RedMushrooms_A/B` (`mushroom_red`), `_LilyPad_A/B` (`lily_pad_plains`) — palette `Foliage_Plains`, parents `M_AF_Toon_Foliage` / `M_AF_Outline_Foliage`; ground cover (grass, flowers) has no hull and uses `MI_AF_Toon_Foliage_Plains_Ground` |
| `environment/rocks.py` | `SM_Rock_Plains_Cluster_A/B/C` (`rock`), `_MossBoulder_A` (`boulder`), wall tiles `SM_Outcrop_Plains_V0..V5` (`outcrop_plains`; V2 / V5 hedges); the skirt hills were retired (art review 9: the runtime skirt is the terrain heightfield + the grove pool) |
| `environment/camp.py` | `SM_Palisade_Plains_Stake_A/B/C` (`palisade_plains`, 1.32 / 1.40 / 1.48 m tip included, mean 1.4 m, R1), `_GatePost` (`palisade_gate_plains`, not blocking), level-design runs `_Straight` / `_Corner`; camp kit `SM_Prop_Camp_Campfire/Well/Tent/Banner/Barrel/Crate/Torch/Hearthstone` (`camp_<type>`; sockets `flame`, `glow`, `gem`; the tent records a 2 × 2 body footprint, the banner sways on `MI_AF_Toon_Props_Plains_Wind`) |
| `environment/story.py` | story decor `SM_Prop_Story_ElvenRuins` (`decor_ruins`), `_GoblinTotem` (`decor_war_banner`), `_BrokenAltar` (`decor_broken_altar`, R6 canonical), `_DryWell` (`decor_dry_well`); lore `SM_Prop_Lore_AncientTablet/TornJournal/CrystalShard/CarvedStone` (`lore_<spriteType>`) |
| `environment/interactive.py` | `SK_Prop_TreasureChest` (`SKEL_Prop_Chest`: root + lid, `A_Prop_TreasureChest_Idle` / `_Open` 400 ms, notify `ChestOpen`; previews, `boundsCm` and the ink gate in the **closed rest pose**, plus `game_open_1080crop.png` / `closeup_open.png` gated on the Open end frame, `openBoundsCm`), `SM_Prop_GoldPile`, `SM_Prop_Event_RunePillar`, `SM_Prop_ExitPortal` (later chapters), `SM_Prop_ExitGate_Sealed` (W7), markers `SM_FX_ClueMagnifier`, `SM_FX_GuideArrow`, gather node `SM_Prop_Gather_EmeraldHerb` |
| `environment/goblin_camp.py` | spec §6.9 / R7: `SM_Prop_Goblin_Hut_A/B` (2 × 2 body), `_FirePit`, `_BoneTotem` (hide strips sway), `_SpearRack`, `_Crates` — non-blocking decorations placed by the level script (`placement` in each entry) |
| `environment/terrain.py` | `Art/Export/Terrain/`: `T_Terrain_Plains_Grass/Dirt/Stone/Camp_BC`, `T_Water_Plains_BC` (1024², one 3 × 3-tile period, painted from `Data/terrain_styles.json` like `GroundPainter`), `T_Terrain_Plains_Details_BC` (per-tile detail atlas), `T_Terrain_Plains_Noise_M`, `T_Water_Plains_Mask_M`; `composite()` = the exact web transition rules (`MF_TileBlend`); the UE material inputs (textures, colour spaces, UV rule, transition formulas, water constants) in `manifest.json → terrain.plains`; previews `Previews/terrain_plains/` |
| `environment/vignette.py` | the plains vignette (`Previews/env_vignette/`): every mesh **re-imported from its FBX** with the shipped palettes, placed like `AAbyssZoneActor` places it (outcrop variant / yaw from `tileHash`, palisade stakes by the neighbour mask, camp props facing the camp centre, lily pads on water tiles), the composited terrain with contact shadows, the warrior; `vignette.png` is a native 640 × 360 render, the crops native 1080p tiles read back pixel-exact, `hero_check` asserts the warrior's crimson and ink; `vignette_footprints.png` paints the W5 footprints from the manifest (half-tile gate posts skipped) |

Previews: per asset `Previews/<SM_…>/` `game.png`, `game_1080crop.png` (shipped ink + bloom next to the baked hull alone),
`game_inkgate.png`, `closeup.png`, `turnaround.png` (+ `turnaround_2.png` … when a sheet is split into truecolour tiles;
trees add `lod1.png`; the chest adds `game_open_1080crop.png`, `closeup_open.png`, `anim_Idle.png`, `anim_Open.png`); family sheets in
`Previews/env_family/` (`foliage_trees`, `foliage_fruittrees`, `foliage_small`, `rocks_outcrops`, `outcrop_ridge`,
`camp_palisade`, `camp_kit`, `camp_kit_small`, `story_decor`, `lore_props`, `interactive_props`, `exit_gates`,
`goblin_camp`); `Previews/terrain_plains/`; `Previews/env_vignette/`.

Ink: decor class (line tone, `INK_MIX` 1.0) at 1.2 px — **1.4 px for `Props_Plains` decor** (`envkit.PX_OVERRIDE`;
poles and stakes are 8–12 px wide at 1080p and their lit edge carries the warm rim), interactive props 2.0 px; every
gated cell is a native-1080p W1 crop measured with the terrain occluding the hull under the floor
(`ground_occludes`). Every entry records `footprintTiles` + `blocking` (W5: trees, boulders, outcrops, stakes, tents,
wells, barrels, crates, tall story decor and the goblin huts / totem / rack / crates block; ground cover, mushrooms,
bushes, campfire, banners, torches, lore props, markers and the exit gate do not — the hero walks onto the exit tile
for the W7 message). Goblin-camp props are non-blocking (spec §6.9: level-script dressing), the gate posts too (the
wall tile beside them blocks). Budgets (art review 9; hull ≈ 0.5 × toon via decimated hull proxies): oak 4.6 k toon
(hull 0.46 ×) + LOD1 1.5 k, fruit tree 3.3 k (0.57 ×) + LOD1 1.45 k, bush 3.3–3.5 k (0.5 ×), outcrop 1.27–1.47 k (0.46–0.62 ×;
drawn total −34 % on V0), rock clusters ≤ 0.6 k drawn, ground cover ≤ 300, mushrooms ≤ 0.8 k, lily pads ≤ 0.5 k.

Conventions learnt on the environment:

* **Canopies want spherised normals** (spec §6.5): each cloud lump's shading normals are blended 85 % toward its
  ellipsoid's (`finish(spherise=…)`), so the lobed silhouette keeps its scallops while the cel bands sit on the lump
  as one clean crescent; at 60 % the lobes facet into crumpled-paper bands.
* **Small lumps need segments, not lobes** (art review 9, bush): a 320-tri icosphere (subdiv 3) of a 9-lobe union reads
  as a faceted rock at 1080p. The bush samples its 8 outer lumps at subdiv 4 and collapse-decimates them to 35 %
  (`canopy(fine=(toon, hull))`: the decimation keeps the lobes' cusps; the hull proxy is the same lump at 17 %), a
  ring of 6–7 smaller lumps gives the scalloped silhouette, and `lift` 0.3 tilts the spherised normals toward +Z so
  the banded rim shrinks to a thin warm edge instead of a chalky grey-green crescent.
* **Boxes on the ground and the inverted hull** (art review 9, chest): under the W1 camera the ink beside a box's lower
  vertical edges can only come from a hull surface facing up there — a vertical hull wall is edge-on and its bottom is
  under the terrain. The chest's body proxy is a dense tube that hugs the planks (+1 mm), **curls under** at the base
  (inset 13 mm at the floor) and widens back under the floor (the contact line reads as buried, like a character's
  soles); a plain box proxy padded round straps, lock and seam grew out and under the floor (wedges, a line off the
  planks). Previews and bounds are taken in the rest pose: `export_animation` leaves the last clip assigned.
* **Hull proxies for open, culled shells** (outcrop masses culled under their moss caps, open caps): a *coarser
  resampling* padded until it encloses the detailed surface drew dark slabs and crescents beside the boulder (its
  boundary no longer matches); the detailed part itself collapse-decimated to 50 % (`M.decimate(part.copy(), .5)`,
  same boundary) pads cleanly. Canopy lumps use the same (`canopy(hull_ratio=…)`).
* **Review PNGs are native** (art review 9): never `write_png_budget` for a 1:1 render — `EnvReview` / `save_native`
  split into truecolour tiles instead; the vignette checks the warrior's crimson after writing.
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
* **Ground-like meshes**: shaded mounds cannot match the painted, unlit terrain (the retired skirt hills read as flat
  pale ovals with a hard seam); ground relief belongs to the terrain heightfield / material.
* **Water props float**: lily pads ride 1.6 cm over their pivot (UE puts the pivot 0.5 cm over the opaque water) —
  at the surface the near half of the ink would be under the water.

## 13. Items: equipment weapons and ground loot (`items/`)

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/items/weapons.py     # ship (+ clip sheets ~2 h)
#   --only sword,axe · --quick <families> (studio + tiers + in-hand cells → scratch) · --lineup · --no-previews · --no-verify
#   --no-clips (ship in ~6 min, listing the clip sheets already on disk)
#   --clips sword,axe [--classes Mage] [--clip-names Attack01]   only the per-clip contact sheets (+ manifest previews)
#   --strips Warrior:staff:Run:4:se,sw … [--table t.json]   per-frame readability strips of whole clips → scratch
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/items/weapon_grips.py --qa   # whole-clip QA table
#   --search [Warrior,Mage,Rogue] [--fams staff,bow] [--out table.json]   re-plan grips (~30 min per class)
#   --tracks Warrior[,Mage,Rogue] [--fams …] [--out table.json]   keyed grip tracks for every clip a static grip fails
#   --hold Rogue:staff:Attack03[,…] [--out table.json]   hit-stop hold pass on a staff / bow / shield / axe strike track
#   --roll Warrior,Mage,Rogue --fams axe | --roll Rogue:shield:Attack03[:window][:x|y|z]   readability roll about the
#        haft (axe) / forearm (shield) on strike and cast clips, kept only where the exact QA passes every frame
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/items/loot.py        # ship (~5 min)
#   --only bag,gold,potion,scroll,fruit,gem,quest,coin (subset; family sheets only on a full ship) · --quick [groups]
```

| File | Content |
|---|---|
| `items/itemkit.py` | shared: item data (`Data/item_bases.json`), the tier ladder (`weapon_tier`: levelReq ≥ 1 T1 · 6 T2 · 15 T3 · 20 T4 · 28 T5 · 35 T6, `w_rusty_sword` Worn), `plan_roles` + `AtlasVariant` (tier atlases = the `Weapons` atlas with the role swatches recoloured → `T_AF_Palette_Weapons_<Tier>_BC/_P`, `MI_AF_Toon_Weapons_<Tier>`), heroes re-imported from their shipped FBX (`import_hero`, `hero_pose`, `attach` = UE socket convention), studio cells, `GameCells` (W1 1080p 1:1 ink-gated cells, `ground_occludes`), the quality-presentation composite (`tint_body`, `add_glow`, `add_beam`, canonical `QUALITY`), `ship_static_item`, `verify_fbx`, **`revolve`** / `lathe_y` / `ring_y`, `loft_rings`, `surface_strip` |
| `items/weapon_tiers.py` | role colours per family and tier from the web icon variant tables (`ItemIcons.ts` SWORDS / DAGGERS / BOWS / staff heads / shield kinds / drawAxe / drawMace / wandSpec); higher tiers make the web's glow accents emissive roles |
| `items/weapons.py` | `SM_Weapon_<Sword·Axe·Mace·Dagger·Bow·Staff·Wand·Shield>` (Previews `SM_Weapon_<F>/`: `closeup`, `tiers`, `game_inhand_<Class>` = READY, a Run frame, the Attack01-03 contacts and the Cast01 release at the shipped grips with the class's default attachments, for each class the runtime shows the generic mesh on, ink-gated (hand items from se, off-hand from sw, a slung shield from ne; the bow's no-hull string is left out of the gate: `Review.ink_ignore`), **`clips_<Class>_<Clip>.png`** (every clip of every class the mesh is shown on: every 4th frame + the contact / release frame in orange, se and sw rows for hand items, ne for the slung shield, one crop per clip so the motion reads against the ground grid, W1 1080p 1:1, `write_png_native` row tiles `_2` …), `game`, `game_inkgate`; `weapons_lineup/lineup.png`); the ship runs the whole-clip in-hand QA (clearance + the bow's readability gate) and exits 1 on a failure |
| `items/weapon_grips.py` · `weapon_grips.json` | per-(class, family) **grips** (socket-space transforms: `carry` for READY / locomotion / hurt, `strike` for attacks / casts, `dodge` / `death` or per-clip ones where neither passes, and **keyed tracks** where no static grip clears a clip; the bow's hand per class) + `GripQA` (every frame of every hero clip on the shipped FBX: lowest point, item × body / head / the class's other default item, face disc; a bow's limbs and string ≥ 2 cm off the rigid body: `CLEAR`, `clearCm`) + the search that produced the table (`--search`: coarse SDF of the posed body, thousands of candidates scored for clearance and readability, then the exact QA); `--qa` prints the table |
| `items/loot.py` | ground loot (spec §7): `SM_Loot_Bag`, `SM_Pickup_GoldCoin` (VFX), `SM_Pickup_Gold_S/M/L`, `SM_Pickup_PotionHP_S/–/L`, `SM_Pickup_PotionMP_S/–`, `SM_Pickup_Antidote`, `SM_Pickup_Scroll_TP/ID`, `SM_Pickup_LeyFruit`, 17 × `SM_Pickup_Gem_<Type>_<Tier>`, `SM_Pickup_Quest_Herb/Gel/Pendant` (Previews `<asset>/` + `loot_family/`) |

**Weapons** (DECISIONS I6 / R4): one mesh per core `WeaponType`, modelled in its socket's bone space (grip at the
origin, blade / shaft +Y, shields face +Y like `SM_Hero_Warrior_Shield`, bows limbs ±Z with the string toward the
archer); `heldBy: []` makes them the fallback of `FindWeapon` (a hero's own `SM_Hero_*` weapon still wins for its
class: warrior sword / shield, mage staff, rogue dagger / bow). Manifest: `weaponType`, `attachSocket`, `tiers`
(tier → MI), `bases` (item base id → tier), `tierNames`, **`grips`** (per class: `socket`, `default` = the carry grip
and `clips` {clip: grip} where it differs, each `locCm` / `rotDeg` FRotator / `quat` FQuat XYZW, socket-relative, or
`{keys: [{ms, locCm, rotDeg, quat}]}` = a keyed track sampled by clip time (slerp / lerp, held at the ends);
`shown` = the runtime shows this mesh on that class today), `gripBlendMs`, `inHandQA` (per class: whole-clip lowest
point, body / cloth / head / other-item pairs, face frames, pass), the bow's **`occupiesOffhand`**. UE (not wired
yet — see the entries' `note`): attach to `grips[cls].socket` with `grips[cls].default`, blend to
`grips[cls].clips[clip]` when a clip starts; hide the class's default off-hand (and show no main-hand mesh) while a
bow is equipped; swap slot 0 to `tiers[bases[<equipped baseId>]]`; slot 1 stays `M_AF_Outline` (weapon ink 3.5 px).

**Ground loot**: palette `Pickups`, category `Pickups`, outline class **interactive** (line-tone ink 2.0 px, gate
1.5 px), pivots at the ground contact, readable from any yaw (props spawn at a hashed yaw and bob 0–12.8 cm). Every
entry carries a `loot` record — `tint` (only the bag: Tint × 0.85 in the canonical palette, normal untinted),
`qualityPalette`, `beam` (spec §8.7 pool / pillar per quality), `bob` (the bag 1600 ms; potions 1200 ms = spec §7
5 px / 600 ms yoyo), HP / MP potions a `glow` (spec §7 `#FF4A4A` / `#4A8AFF`, 22 cm, **alpha-blended** peak α .85 ≈ .45 at the
flask's silhouette, on `glint`, placed **behind** the flask: depth-tested, so the flask's ink stays dark), the ley
fruit an additive mint `glow` (#6FF0C8 α .6, 20 cm), scrolls `milestone: later` (DECISIONS I2 / I4) —
and the sockets `beam` (ground centre), `label` (name plate, top + 18 cm), `glint`. `SM_Pickup_GoldCoin` is the
`reward.gold` burst mesh, not a ground pickup: no `loot` record, no sockets (its `game.png` / `game_1080crop.png`
show a burst of six in the `M_AF_FX_Mesh` look over a goblin). Game ids: `loot_bag`, `potion_drop_hp|mp[_s|_m|_l]`,
`gold_pile_s|m|l` (+ `goldAmount` bands), `fx_coin`, `quest_item_<kind>`, and `loot_<baseId>` for gems / potions /
scrolls / ley fruit (the world resolves `loot_bag` for every ground item today; per-base meshes are an opt-in
lookup). `game_inkgate.png` shows the whole shipped cell (`loot.LootReview`): flat pickups gate only their edges above
the ground-contact line, the count of ignored contact edges is printed.

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
* **Generic weapons ride another class's clips** (art review: weapons-loot): an identity attach is right only for
  the class whose weapon the clip was authored for. QA every frame of every clip, not READY + one contact (the
  rogue's Attack03 hit-stop put a staff orb 8.6 cm under the floor, the Dodge 15.6 cm), with the class's default
  attachments in the scene (a bow on `weapon_l` shared the warrior's shield socket), and give each (class, family)
  grips: the staff carried head-up beside the warrior / rogue, short weapons hanging low and outboard at the mage's
  side (never over the hood and eyes), strikes on the identity attach (the clip's own weapon motion). A grip turns
  the item about the grip centre and may slide along a haft; at 1080p the handle's angle in the fist does not read,
  the forearm-to-item angle does.
* **Some arms cannot carry an item at all**: the mage's left hand swings across the robe in Walk / Run, so a shield
  strapped to that forearm crosses the body on 12 of 60 Walk frames at any grip — the generic shield is **slung on her
  back** (`socket` `spine_03`, `slung`, per-clip alternatives where the hood flops back in Hurt / Death / the big
  casts). The rogue's shield is hidden for frames 2–7 of the Dodge roll (`hideMs`) — the forearm is tucked into the
  body there and nothing clears it.
* **Static grips are not always enough**: a 1.27 m staff carried by the warrior hits his strapped shield in Walk and
  his own body in every sword swing at any one grip. Such clips get a keyed grip track: the coarse SDF ranks
  candidates per frame, the exact QA checks a pool of them on every frame, and a Viterbi pass threads one feasible
  grip per frame (≤ 12°/frame, flicks to 24°/frame only to dodge an intersection), widening the pool round any frame it
  cannot clear. A 4 cm SDF misses thin parts (pauldrons, fingers) and a 40-point cloud misses a shield face: rank with
  the SDF, decide with the exact test.
* **Readability is a whole-clip, on-screen property** (art review, second pass): a track that only had to clear the
  body swung the warrior's Run staff up the view ray behind the helm for half the cycle, pointed his Cast01 staff and
  the rogue's Attack03 staff at the camera (an orb blob at the fist) and turned the bow edge-on in every warrior
  strike. Staff tracks score the fist → orb vector projected in the se *and* sw views (`read_staff_screen`: ≥ 65 %
  of its length, the orb above the fist — carry .30, casts .40, attacks level — and not behind the head); bow strikes
  keep their D toward the camera (`read_bow`); staffs and bows keep out of the face disc in strikes and casts too,
  from 2 frames before the contact / release on (`FACE_STRIKE`; the wind-ups whip the fist itself across the face,
  rolls, falls and the mage's Cast_Meteor summon are excepted) — the QA now gates face frames (`LIMITS.face` 0). The turn between frames costs the smaller of the turn in the fist and
  the turn in the world (a staff held steady while the casting hand rolls fast under it is calm on screen; the
  grip-relative limit alone could not follow the Cast01 thrust), staff / bow strikes may flick > 12°/frame to stay
  readable (cost 30 instead of 300), and where the plan's soft cost stays > 2 above a frame's best candidate a
  **readability rescue** checks that frame's next readable candidates exactly and re-plans. The check is
  `weapons.py --strips Cls:fam:Clip[:step[:se,sw]]` (every 4th frame from se and sw) — the six in-hand cells sample
  only six frames.
* **Hold the hit-stop in the world** (art review, third look): track grips are fixed in the fist, so following a hand
  that rolls ~15°/frame through a lunge costs a flick every frame — the plan kept the rogue's Attack03 staff
  foreshortened into an orb blob at the fist through the hit-stop (se: 29 % of its length on screen). Staff / bow
  strike tracks weigh readability × `HIT_READ_W` (4) from 2 frames before the contact / release to `HIT_READ_TAIL` (6)
  after it, and `--hold` post-processes a track: one *world* orientation held over that window (the grip
  counter-rotates in the fist), slerped in and out over 4 frames (≤ 24°/frame), the readable one (se *and* sw) that
  passes the exact QA on every frame. The rogue's Attack03 and Cast02 staff hold (penalty .40 / .35 → 0); the rogue's
  Attack01 and the warrior's strikes have no feasible hold (the strapped shield, the plate) and keep their tracks.
* **Bows need their D toward the camera**: the W1 camera saw a bow in the hero's sagittal plane edge-on (a stick);
  the brace is 15 cm (web bulge 13 / 84) and the string a light no-hull cord 1.3 cm across in the bow's plane; the
  grips turn the plane toward the camera in se *and* sw and keep limbs and string ≥ 2 cm off the torso and helm
  (the search rejects candidates closer than 2.8 cm on the coarse SDF; the exact QA measures `clearCm`).
* **Clearance is not readability — gate both** (art review, fourth look): the warrior's bow on `weapon_l` passed the
  in-hand QA while it dangled edge-on at the hip (a brown bar) and an empty right fist struck, at every contact.
  The bow now rides the warrior's **right hand** (`socket` `weapon_r`, as the mage's; DECISIONS R13), so the swing
  carries it, every strike / cast has a keyed track planned with `read_bow` (×`HIT_READ_W` on the hit-stop) and the
  `--hold` pass; and the QA **gates readability**: `read_metric` (min over se / sw of |bow plane normal · view ray|,
  the D's on-screen fraction) on every strike / cast frame, `READ_GATE` floors .45 on the hit-stop window and .15 on
  any frame (manifest `inHandQA.classes.<cls>.read`, `readGate`); the axe and shield report the same metric.
* **Flats to the camera** (fourth look): the axe bit went edge-on at the warrior's Cast01 release (a stick with a dark
  tip) and the rogue's Attack02 / Cast01 (a spear, a grey blob); `--roll` turns each strike / cast grip about the
  haft (≤ 90°, keyed tracks key by key) to the angle that best shows the bit in se *and* sw on the hit-stop among
  the angles whose grips pass the exact QA (warrior Cast01 .12 → .45, rogue Attack02 .21 → .54, Cast01 .16 → .76).
  The rogue's Attack03 shield (edge-on, a pale blade) got the `--hold` pass with `read_shield_face` (the face ≥ 30°
  toward the sw camera): no roll about the forearm or across the boards cleared her lunge, one held world angle did.
* **A carry grip must clear the body on screen** (fourth look): the warrior's wand hung along his iron greaves and
  vanished (only the crystal read). `read_wand_out` scores the crystal / upper shaft's projected distance to the
  nearest body / attachment vertex (≥ 6 / 4 cm, se weighted ×1, sw ×.35: the right hand sits behind the body from
  sw) and the rod's on-screen length; the warrior / rogue wand carry is searched with it (not the sword's identity
  hang), the T1 shaft is a value lighter (#9A54AC, lightAmt .55) and the crystal faintly self-lit (e .6).
* **One mesh per family (R4)**: `w_wooden_shield` (T1) is a round planked buckler in the web; here it is the heater
  with the T1 plank boards and iron boss (R4: one mesh per weapon base type family) — an accepted trade-off.
* **Glowing gems and the dark check**: bloom over a near-white stone lifts its line-tone ink above lum 64 — the
  diamond regions use a darker hull mix (`o` .45, coloured stones .5) under the glowing tables (e .42; diamonds a
  pale ice-blue table e .30) and glint stars (e 1.0). The same holds for any emissive accent that meets the
  silhouette: the ley fruit's seams bloomed the ink past the limit at e 1.0 (e .7 + hull mix .5 pass).
* **Small hulls round small details** (fourth look, weapons): the sword / dagger guard bars padded round their ball
  finials and the wand crystal padded round its three prongs tented into dark diamonds past the knobs — finials,
  crystal and prongs carry their own hulls; a wrap helix rides its grip's radius profile (`helix(r_fn=, sides=)`,
  seated on the polygon's flats, ~1.5 mm proud) instead of floating off a tapered grip; the mace band follows the
  ball's curvature on the equator (≤ 6.5 mm proud) with the spikes above and below it, never through it.
* **Heaps** (art review 4): a mound proxy padded round the coins on it tented into a detached dark arch behind the
  pile (and left the coins' real silhouette without ink); a shrink-wrapped height-field proxy inks its own dips
  between coins (concave walls). The heap keeps the mound's own hull and gives the coins on its outer ring — the
  only ones that reach the silhouette, the camera at −50° sees the whole 27° top — their own hulls. Fourth look:
  coins sunk flush in a smooth mound read as dough with embossed octagons — the mound is lower and a shade darker
  (#D69A2A, the shadowed coins inside), every coin rests *on* it with its rim band showing (Poisson-spaced, ±16°),
  three coins stand in the crest and a three-coin stack crowns it (one padded cylinder hull per stack), three coins
  slid off the foot set the lower edge, and the leaning coin faces between the key light and the camera with low-
  shadow regions (`rim_lean` / `face_lean`, s .14–.16) so it stays gold at any spawn yaw.
* **Glows over terrain** (fourth look): an additive red card over the plains grass turned it amber (the 'rare'
  colour). The HP / MP cards are alpha-blended ('over', `loot.glow.blend` `alpha`, peak .85 so the ring at the flask
  reads ~.45 red / blue — red mixed into green stays khaki below ~60 % coverage); the ley fruit's mint glow (#6FF0C8
  α .6) stays additive: a light, not a colour laid over the ground.
* **Glass from −50°**: the colourless shoulder of a flask fills the top of the silhouette; potions fill the bulb to
  the shoulder and tint the glass 22 % toward the liquid; painted glints sit on the key-lit upper-left at the 'front'
  yaw (and its opposite), ~45° in from the silhouette so the ink never covers them.
* **Tiers in silhouette** (gems): radius ramp 7.0 → 14.8 cm (about 14 → 30 cm across: pickup scale beside 24 cm
  potions — at 8–16 cm they were 10–22 px dots and the tiers could not be told apart) and crown form chipped ·
  faceted · crowned (a second, steeper crown ring) · brilliant / perfect (16-sided); tiers ≥ 3 and every diamond carry
  a glowing table and an emissive four-point glint star (eight points on the perfect cut: the web's tier-5 rays);
  coloured stones keep low lightAmt and little white on crown and table, the ruby's crown facets e .3 (the warm rim
  band on the steep facets read salmon), the emerald a deep body with bright facets (equal value with the grass).
* **Figure-ground on the plains** (fourth look): saturated greens at the grass's value (#74A247) read only by their
  ink — the antidote's lime liquid and near-white glass are lighter, the herb's blossoms 50 % larger and near-white
  (e .12) with pale twine, the gel's highlight band wider and brighter (e .85).
* **Thin hanging hardware** (the pendant chain): a 9 mm round cord disappears in its own 2 px ink; lay it as a 1.5 cm
  flat cord (coiled beside the locket) and give small fittings (the bail) a light metal.
* **Budgets**: weapons 0.7–3.5 k toon (+ 0.24–1.4 k hull); pickups 0.7–3 k toon (+ ≤ 1.5 k hull), the gold heap
  3.0 k + 1.4 k hull (27 light heap coins: 8 sides, one face unless tilted up) and the herb bundle 3.5 k + 3.4 k hull
  (15 leaves, each its own ink, pale twine and three pale blossoms for value contrast on the grass); gems 40–254 tris (glint stars included);
  loose coins 10–12 sides (a 6 cm coin is ~7 px at 1080p).

## 14. VFX: textures, FX meshes, recipes, previews (`vfx/`)

```bash
/opt/venvs/blender/bin/python unreal/Art/blender/vfx/fx_textures.py      # T_FX_* + Previews/vfx_textures/ (numpy, ~1 min)
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/vfx/fx_meshes.py [SM_FX_…]   # ship (~8 min)
#   --quick (close 3/4 + 1080p context crop per mesh → scratch) · --no-previews
python3 unreal/Art/blender/vfx/fx_recipes.py                              # vfx_recipes.json + fx_palettes.json (pure Python)
python3 unreal/Art/blender/vfx/fx_validate.py                             # recipes vs the runtime parser + coverage (exit 1)
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/vfx/fx_preview.py [groups|ids]  # 1:1 frames
#   groups: combat utility status world warrior mage rogue ambient fallback (default all) · --scratch · --cells
#   · --soft-ground (draw the proposed 0.8 cm ground fade instead of the shipped 8 cm DepthFade)
```

DECISIONS P6: VFX are code-driven pooled ISM particles (`UAbyssVfxSystem`, no Niagara). The art ships what those
emitters draw — sprite textures, FX meshes, and the recipe document the runtime merges over its built-in defaults
(WorldContract 5; same id replaces).

| File | Content |
|---|---|
| `vfx/fxpaint.py` | signed-distance painting toolkit (numpy): fields, cel bands, strokes, halos, strips |
| `vfx/fx_textures.py` | `Export/VFX/T_FX_<Sprite>.png` — the web `FxTextures.ts` painters redrawn as cel sprites (crisp silhouette, 2–3 value bands, one halo only where the shape emits light), **linear** RGBA (sRGB off), white = tintable, pre-coloured cells (8 flame palettes incl. the camp flame, frost shard, arrow, coin, rock, chain); strips of N cells picked by the instance Variant; `sprites.json` (cells, blend, note) |
| `vfx/fxkit.py` | the `M_AF_FX_Mesh` look in Cycles (one tint per particle → toon bands → ink hull, **value channel `AF_Data.a`**, 4×4 Bayer-dithered fade, HDR), `FxBuilder` (regions = values, resolved per part), `finish(… flat_lit, mitre)`, **`mitre_hull`** (re-seats a faceted mesh's hull as an exact mitred offset), ship + manifest entry (`fx` record), shipped characters for scale |
| `vfx/fx_meshes.py` | `SM_FX_*` (below) + previews + the FX ink gate |
| `vfx/fx_recipes.py` | every effect of spec §8.5–§8.7 + the 40 skills as runtime recipes (web compositions converted at 100/45 cm per px); `fx_palettes.json` (element palettes, flame gradients, class / skill / quality / affix / spirit / death colours, status tints, story embers) |
| `vfx/fx_validate.py` | mirrors `FAbyssVfxLibrary::ParseLayer` (a wrong JSON type is silently ignored there, an unknown enum skips the recipe), sprite / mesh existence and cell ranges, runtime clamps, coverage of every bound event id and every skill's `skill.` / `proj.` / `ground.` ids |
| `vfx/fx_preview.py` | Python port of `UAbyssVfxSystem` (presets, emission, anchors, blow frame incl. its random climb, motion, size / alpha curves, colours, quad and mesh transforms) composited over Blender renders of the plains terrain, **the hero class that owns the effect** (warrior / mage with staff + crystal / rogue with daggers, quiver and the Cast_Multishot flash-bow) in its own clip at the event's beat + t, actor anchors = the posed rig's `fx_chest` / `fx_head` / `fx_overhead` / `fx_hand_r` sockets re-read every 60 Hz tick while the clip plays (as `AAbyssCharacterActor::GetAnchorLocation`), goblins and — for world / prop loops — the shipped prop the effect sits on (`WORLD_PROPS`: campfire, torch, hearthstone, exit portal / sealed gate, lore tablet, herb, clue marker facing the camera, the chest in its Open clip, rune pillar, the loot bag incl. its pickup flight), the blacksmith in Work_Hammer at the anvil stump: per-pixel ray / quad intersection, depth test with the shipped 8 cm DepthFade on every quad, translucent before additive, the runtime's light pools (+2 cm, α × .5), FX meshes rendered with `M_AF_FX_Mesh`, the StatusTint multiply on a status target, the dodge afterimages and the soul echo's hero ghost (Ghost look, #9FE6FF α .55 ↔ .35, bob 6 px), bloom → `Previews/vfx_effects/<recipe>/<ms>[-<ms>]ms.png` (native 1080p, scale 1, truecolour, one or two key frames per file, one tight crop per effect = what it touched + its actors; a frame over 200 KB becomes stacked native strips `<ms>ms_2.png`; a frame that cannot fit **fails the run** — nothing is ever resampled) |

FX meshes (all drawn by `M_AF_FX_Mesh`: opaque masked toon + ink, one tint per particle, so structure comes from
geometry, the value channel and the hull; light, smoke and energy stay sprites): arrows (steel, poison, ice),
`IceShard`, `Fireball` / `SpiritBolt` (flat-lit cel fire: HDR tint, value bands clip toward white at the leading
cap), `Rock_A/B` debris, `MeteorRock` (dark seams), `HexRing` / `HexDome` / `HexPlate` (hollow hex frames),
`Bubble` (camera-facing inked rim ring + highlight arc + glint), `FrostNova` / `FrostCage` / `FrostCrust`
(crystals), `StunStar`, `ChainLink` / `Chain`, the swoosh ribbons `SlashArc_Warrior` (+ `_Back` mirrored for a
negative spin) / `_Rogue` / `_Mage` and `WhirlBand`, plus the item family's `SM_Pickup_GoldCoin`. Each manifest
entry carries `fx` {orient, pivot, defaultTint, intensity, values, recipes, …} and `boundsDiameterCm` (what a
recipe's `sizeCm` scales). Gate: baked hull alone (no outline WPO in the FX material) ≥ baked − 0.5 px on ≥ 90 %
of the 1080p silhouette edges with an ink pixel within 3 px.

Conventions learnt on the VFX:

* **Every FX region is the same white `Region`**, so the palette merges them into one swatch — values must be
  resolved per part (`af_part` → region), never per swatch (the first pass gave every mesh its last region's value).
* **Faceted crystals need mitred hulls**: the kit pushes hull vertices along the smoothed normal, which leaves a
  pyramid's flanks `w·cos φ` off the body (a sharp tip kept ~⅓ of its ink); `mitre_hull` solves `n_i·δ = w` per
  corner (least squares, capped 2–2.6 w) — every face exactly `w` off, pointed ink at the tips like a pen line.
* **Swooshes, not bananas**: a solid crescent of uniform width reads as an object. Slash ribbons taper from a
  pointed head (the leading end for the recipe's spin sign) to a hair-thin tail and carry three radial value bands
  (white-hot outer edge 1.0, body .8, trailing smear .6); a backhand needs the mirrored mesh, not a negative spin
  on the forehand one.
* **Masked meshes dissolve, they don't fade**: any alpha < 1 is a 4 × 4 screen-door (thresholds (b + .5) / 16, so
  α < 31/32 already drops pixels), raw checker noise wherever there is no TAA (mobile low). `fx_recipes.
  mesh_dissolve` holds every mesh layer's dithered window (fade-in + late tail) to 2 frames at 60 Hz (`DITHER_MAX_MS`
  34; at `alphaPow` 12 ice armor's nova dithered for ~130 ms, the mana-shield bubble for ~190 ms, a slash arc at 5
  for ~115 ms), and small flying pieces (rock debris, ice shards) never dissolve — α 1 and a late shrink (grow × .15,
  `sizePow` .3; the coins' .25).
* **Bodies swallow bursts**: sprites are depth-tested against opaque characters and the chest anchor is the body's
  centre. Strike recipes pull their target-side bursts 22 cm back along the blow (`STRIKE_CM`); the recommended
  runtime fix (camera-ward bias for actor-anchored quads) is in `integration.bodyOcclusion`.
* **Additive light vanishes over the bright plains**: rings, shocks, beams, comet tails and shouts are two layers —
  a translucent tinted body for presence + the additive glow on top (the web's `ring()` did the same).
* **Blow-less events have a fixed frame**: level-up, pickups, statuses, loot, portal and prop loops get no blow, so
  their frame is world +X/+Y — with the fixed W1 yaw, `offsetCm` d·(−0.455, −0.455, 0.766) is the view ray: their
  body-anchored quads sit 40 cm camera-ward (`CAM_BIAS_CM`, `integration.cameraBias`) and draw over the body.
* **Loops start empty**: a 1.2 /s status ring shows nothing for 0.8 s — slow loop layers get a one-particle burst
  copy (`prewarm`, `integration.loopPrewarm`).
* **Ground quads sit 8–11 cm over the terrain** (`fx_recipes.ground_lift`): `M_AF_FX_Sprite` fades every quad by
  DepthFade(8 cm) and the runtime has no ground case, so at 1–3 cm rings, runes, decals, pools and auras drew at
  10–30 %; from 6.2 cm (8 cm of view depth at the W1 pitch) they are whole. The previews draw the shipped fade on every
  quad. The runtime's own light pools (+2 cm) need the same lift (`integration.groundSoftFade`).
* **Loops that must stand** (the loot beam on a bag until pickup) cannot loop an alpha curve: a steady layer (one
  particle per half period P living 2P, fadeIn .5 → the overlapping triangles sum to a constant) + a pulse layer (one
  per 2P living 2P) give .55·α ↔ α with period 2P (`loot.beam.*`, the web's ×.55 yoyo).
* **Markers that must stay up are `held`** (`fx_recipes.held`, the same steady + pulse pair as the loot beam): the
  elite fire orb and blood orb, the bleed wound. One-shot particles re-spawned every second with a steep `alphaPow`
  were faint most of the time (the web's orbs are persistent circles with a yoyo tween). A marker needs a crisp
  shape, not a soft Glow / Puff: the fire orb is the Core sprite's disc, the blood bead the inked Drop, and the
  ambient pollen / dust are Core specks that hold their alpha (`alphaPow` 2.5) — soft dots fading over 8–14 s were
  invisible at 1:1.
* **Props face the way the game places them**: the exit gates face into the map (mesh −Y → Blender yaw −135° to
  the W1 camera, as the env family sheets); at 45° the preview showed the sealed gate's back without its planks.
* **A hue must survive the ground**: additive rings in #FF4400 / #FF2222 / #CC0000 all came out the same orange on
  dirt and purples went grey on grass — affix auras are a translucent Shock disc (thin fill + crisp rim in the true
  hue) + a faint additive rim, verified on dirt and grass side by side (`elite.*`). A filled additive body glow tints
  the body (extra_strong turned the goblin khaki): power pulses are hollow rings round the body. A dark hue needs a
  deeper body held near opaque: vampiric #CC0000 at the shared α .72 still read as a darker fire ring, so its disc
  is crimson #A80A24 at α .92 → .45 (`aura(…, body=, body_alpha=)`) — blood, not fire.
* **Bodies hide self-buffs too**: a chest-anchored glow on a blow-ful self cast (frenzy, poison blade, vanish) is
  inside the body; use hollow rings larger than the silhouette, ground pools, the hand socket, or spawn puffs on a
  ring ≥ 45 cm round the body (the vanish smoke bomb) until the runtime camera-ward bias lands.
* **Aligned slash cells face the blow at rotation 0**: with `alignToBlow` the quad's up (the arc's apex) points
  along the blow on screen and the arc's span (`aspect` 2) runs across it — a tall ")" for a sideways blow;
  `rotationDeg` ±90 lays it flat (∩ / ∪). The lethal strike's tall blood crescent is rotation 0 (a solid translucent
  crescent + a thin additive edge), its steel counter-slash −62° crosses it.
* **Launch points come from the owning class**: mage and rogue effects are cast by their own rig in their own clip
  (Cast01 / Cast_Blizzard / Cast_Meteor / Cast_Multishot) at its release beat. The mage's `hand` resolves to the staff
  `crystal` socket (spec 8.1; at Cast01's release the crystal is 73 cm past `fx_hand_r`): the previews draw it so, and
  the runtime needs the matching `AAbyssCharacterActor::GetAnchorLocation` change (`integration.launchSocket`).

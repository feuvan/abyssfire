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

Env overrides (tests): `AF_EXPORT_ROOT`, `AF_PREVIEW_ROOT`, `AF_PALETTE_ROOT`. Dev-mode output (`--quick`, `--debug`,
iteration sheets; never shipped) goes to `kit.paths.scratch_root(<sub>)` = `$AF_SCRATCH_ROOT/<sub>`, default
`<system temp>/abyss_art/<sub>` (created on demand).

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
NPC, Props_Plains, Foliage_Plains, FX, Test, Weapons, Pickups, Pets, and the dark remake's Monsters_March,
Props_March, Foliage_March (§19).

**theme** (pure Python) — themed game ids for a zone's presentation theme (§19): `themed("tree", "march")` →
`tree@march`, `themed_ids(ids, theme)`, `split`, `resolve(gameIds, id, theme)` (the runtime order: `<id>@<theme>`,
then `<id>`), `family("Props", "march")` → `Props_March`, `audit(manifest)`; `THEMES`.

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
`shading` / `units` are rewritten from the kit; any other top-level section (a family's own, merged per key by its
subclass: `terrain` — `EnvManifest`, `ui` — `IconManifest`) is written whole only when this instance changed it, else
the disk copy is kept (coherence pass: a plain `Manifest` loaded before an icon run used to write its stale `ui` back
and drop the new icons; the smoke test checks both cases). Always go through `Manifest`, never write the JSON directly.

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

`previews[]` (review only, no consumer): paths relative to `Art/Previews/` (`hero_warrior/game.png`) — the kit's
`asset.ship_*`, the environment and the VFX shippers write them relative to `Art/` (`Previews/SM_Prop_Camp_Tent/…`);
`coherence/checklist.py` resolves both. The Chapter-1 checklist of every id the game resolves is
`Art/Export/CHECKLIST.md` (§17).

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
| `monsters/goblins.py` | `GoblinKit`: `SKEL_Goblin` (kit humanoid builder, goblin proportions, the union of the family's chains: `ear_*`, `jaw`, `eye_*`, `loin_*`, `pelt_*`, `headdress_*`, `totem_feather_*`) and the shared body (skull, face, ears, pot-belly torso, limbs, fists, feet, loincloth); `skull_hide` / `torso_hide` cut skin under a helm / cuirass / shawl; `builder(…, lod, hull_ratio)` + `LOD1_SKIP`; **`LodHullBuilder`** (LOD1: every outlined part's hull baked from a padded proxy decimated `hull_ratio` × further — chief 0.72, shaman 0.8 — so LOD1 hull ≤ 0.75 × toon) |
| `monsters/mclips.py` | `GoblinAnims`: web `humanoidTracks` → BP poses (READY, thrust / overhead / cast attacks, recoil, backward death), no-slide walk, Stun, contact / windup notifies |
| `monsters/goblin.py` · `goblin_chief.py` · `goblin_shaman.py` · `slime.py` | the four SKs + held items (spear, cleaver, totem); the chief also ships `T_UI_Portrait_GoblinChief` (Portrait pose, manifest `portrait`) and inks as a **boss** (spec §1.3: 3.5 px, gate 3.0 px); the goblin, shaman and slime keep the monster class (1.5 cm baked) but ship a **per-asset 3.5 px request** (`OUTLINE_PX`, art review: at 3.0 px ear tips, spear, feathers, horns, fingers and the slime's base kept < 3 px on 20–60 % of the edges) and are gated at 3.0 px |
| `monsters/goblin_hunt.py` | hunt-leader attachments (red cap, pendant, loot sack; manifest `variants`, scale 1.25) |
| `monsters/elite_crown.py` | `SM_FX_EliteCrown`: the web three-point front crown + low back band, diamond ruby, 542 toon + 336 hull tris (+ `crown_over_monsters.png`) |
| `monsters/lineup.py` | `Previews/mon_family/lineup_game.png` (+ `lineup_inkgate.png`): the warrior, the four monsters and a hunt leader ×1.25 with cap + crown, re-imported from their FBX, W1 camera, native 1080p, each figure ink-gated at its shipped request − 0.5 px (manifest `outlinePx1080`), runtime-lit FX faces (emissive swatches: the spirit-fire) off for the measurement as in the ship gates |
| `monsters/mship.py` | ship: bake, FBX (SK, **LOD1**, `A_` per clip, item SMs), manifest (merging), per-frame QA, ink gate (+ the fraction at the class width itself, `inkGate.specPx` / `minFracRimSpec`), re-import verification (`verify_export`, `verify_lod`), the review set (`lod1.png` = LOD0 vs LOD1 incl. the mobile-low look, labels = the shipped LOD's tris; `poses_close_a/_b.png` 5 cells each at 1:1), `portrait_shot` (UI portraits of monsters); **`MonsterShip(outline_px=, outline_px_note=)`**: a per-asset `OutlinePx1080` on the hull slot (manifest `outlinePx1080` + `outlinePxClass` / `outlinePxNote`; the UE importer makes its own `M_AF_Outline` instance), the held items / attachments ink with it, previews render it and the gate is `px − GATE_SLACK_PX` (`ink_px`, `gate_for`; `inkGate.outlinePx1080`) |

Budgets (spec §1.9; `budget` in each manifest entry; toon = every toon-lit slot, the slime's dithered shell
included): goblin 6.1 k / LOD1 3.2 k, chief 8.96 k / 3.25 k, shaman 8.9 k / 3.4 k, slime 2.92 k / 1.50 k; hull ≤ 0.75 ×
toon at both LODs (LOD1 hull / toon: goblin 0.68, chief 0.68, shaman 0.65, slime 0.33). LOD1 `screenSize` 0.05: a monster is 0.10–0.20 at every W1 zoom, so desktop always draws LOD0; LOD1 is the
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
| `npcs/npc_clips.py` | `NpcAnims` (on `hero_base.HeroAnims`): READY per work style, `IdleNPC`, `IdleNPC_Fidget` (personal gesture), `Alert`, `Talk`, `Work_Hammer / Count / Read / Guard / Lookout`; escort `Idle`, `Walk` 1.2 m/s, `Run` 2.85 m/s (no-slide), `Hurt`, `Death`; rescue `Cower` (a kneel); cloth (skirt chains with the leg envelope + the `coat_th_*` thigh drivers, hair tail, beard, scarf, pack bounce) |
| `npcs/npc_ship.py` | build (LOD1 rebuilt to the §1.9 5 k budget), bake, per-frame QA (`ToonGroundQA`: items' ground on their toon surface — a planted staff / sword point's hull under the floor is expected; skirted looks add `CoatLegQA`: `coatLegs` = trouser / boot vertices beyond the coat or robe, gate 0 — `QA_CLIP_LIMITS` lets the face-down Death cross the front panel pressed into the floor under the body by ≤ 12 mm; `coatLegInk` the same for the legs' baked hull, reported), FBX (SK, LOD1, `A_` per clip), manifest (`npcStates`, `fx`, `scenery`, `attachments`, `budget`, `lods`, `portrait`), re-import verification, review set in `Previews/npc_<id>/` with the ink gate on every game-camera cell (`NpcReview`: every PNG truecolour at scale 1, `save_native` tiles), portrait `T_UI_Portrait_<Id>` (512², screen-space ink) |
| `npcs/npc_dressing.py` · `npcs/npc_kit_parts.py` | props (`Previews/npc_props/props*.png`, leading face 3/4 front + 3/4 back), the anvil stump (static ship with `ground_occludes`: the hull under its foot is the terrain's), the quest markers (full camera billboard on `fx_overhead`; `Previews/npc_markers/markers_over_npc.png` on the elder + gated 1080p cells `markers_close.png`), the R2 lineup (`Previews/npc_family/`); the `SK_NPCKit_*` parts (each the merged mesh's own generator, on its look's rig) + `Previews/npc_kit/kit_parts_a/_b*.png`. Props and kit parts are never drawn alone: their `inkGate` is `{standalone: false, measuredOn: [SK_NPC_…], pass}` — the gated 1080p cells of the NPCs that hold / wear them (`held_ink_gate`; ship the NPCs first) |

Budgets (§1.9): LOD0 8.4–11.8 k toon, LOD1 ≤ 5 k (ratio scaled per look), hull ≤ 0.63 × toon, 36–52 bones. LOD1
`screenSize` 0.05 (as the monsters: desktop always draws LOD0, LOD1 is the mobile-low mesh). Ink: NPC class (1.6 cm
baked) with a **per-asset 3.5 px request** (`npc_ship.PX` / `ink_slot`: manifest `outlinePx1080` 3.5, `outlinePxClass`
3.0, `outlinePxNote`; the held props and the `SK_NPCKit_*` parts ink with it), gated at 3.0 px on ≥ 90 % of the edges
of every gated 1080p cell (13–17 per NPC: poses, facings, extra clips) — the coherence pass measured the class's 3.0 px
at 61–83 % against the Chapter-1 character gate (≥ 3 px), the goblins beside them already ship 3.5 px.

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
* **Coats and robes follow the thighs** (NPC review 2: the escort's run drove the lifted thighs 6 cm through the
  coat's front panels, high under the belt, where no skirt-chain angle moves the cloth without flaring the whole hem):
  skirted looks carry two one-bone drivers `coat_th_l/r_01` on the hip joints that copy each thigh's *forward* swing
  only (`NpcAnims.drive_coat`; a leg swung back must not drag the panel under the belt into the body), and the cloth
  before each thigh rides with them (`Folk.skirt_thigh_share`: peak 0.72, ±26° round the front, k .05 → .22 → .50 →
  1.0). The chain envelope takes the drivers' own push off each leg point's deficit (`_drive_dr`) and divides the rest
  by the chains' free share (floor 0.6); clearance 2.2 cm (the legs' 1.6 cm baked ink + margin) + 1.2 cm toward the
  hem; the side chains follow the back panel's push beyond 28° (heels tucked behind a kneel). The skirt hull proxy
  keeps every row (`shell_ring(hull_row_step=1)`): on every other row its chords cut into the bent cloth as dark ink
  specks. `CoatLegQA` joins each leg vertex to its own core (the nearest point on its thigh / calf bone) — a leg that
  leaves the coat under the hem never crosses it, a knee through a panel does.
* **Kneels, not squats, under long cloth**: the rescue's `Cower` squatted with the knees spread and drove the coat out
  as a flat cone over the ground; on both knees, hunched, heels back, the coat drapes over the thighs and meets the
  floor round the knees (`NpcAnims.COWER`, `grounded` post).
* **Chest holds keep the elbow ≥ ~50°** (NPC review 1): a fist high on the chest with the elbow out folded the arm to a
  32° hinge and the coat sleeve webbed into a pointed sheet past the elbow; the lost traveller holds the pack strap
  below the chest, elbow down at the side. Sleeves have elbow loops either side of the hinge (7 rings).
* **Long hair rides the head to the nape**: the elder's back mass is head-only down to under the occiput and blends
  onto `hair_tail` below the nape, and the hair chain's envelope ignores the skull and the mass (looking up, the
  occiput swung into the chain plane and the envelope flipped the hair out behind the head as a fin).
* **LOD1 rebuilds thin rings and hats, never collapses them**: sleeves and cuffs (12 sides), the wide brim's crown,
  hull proxy, accent band and brim (coarser grids) and the peddler's bedroll bypass the decimation (`_add_exact` /
  `_exact_via`): a collapsed wrist left the escort's cuff floating over a dark notch, the wanderer's crimson band
  turned salmon across the crown, and an exact band padded into a crown proxy decimated to a few faces inflated it
  into a dark sail over the hat. The rest of the mesh takes the budget (`build_lod1` iterates the ratio).
* **Staff and orb** (NPC review 4, 9): grip at the chest (`STAFF_DOWN` 1.24 m to the planted foot, `STAFF_UP` 0.50 m:
  the orb at the crown, the hand 0.38 m out: beside the head); orb `l` .30 `e` .18 under a 12 cm `#FFCF50` α .30
  halo, no core — at e .35 / l .5 under an 11 cm card with a pale core the 1080p orb bloomed to white.
* **Previews are truecolour** (`NpcReview`, `save_native`, the props / markers / lineup / kit sheets): the 256-colour
  budget fallback mapped the elder's small green orb to the metal grey of the props sheet and the lineup, and
  quantised the measured 1080p cells.
* **The peddler's huge pack** (`pack_scale` > `Folk.PEDDLER_K`): 1.3× taller (the bundle rises above the shoulders to
  the hat, web) and 1.2× deeper, the bedroll front to back on top — across it, its ends flanked the head at ear height
  and read as earmuffs from the front.

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
| `environment/envkit.py` | shared by the family: `Lump` cloud lumps (union of spheres, `cull_hidden` drops faces inside a neighbour; icosphere `subdiv` 3 = 320 tris, 4 = 1280), `RockShape` (radial boulder: bumps, flat foot, `top_cut`, `boxy` superellipsoid, `centre`) + `rock`, `rock_cap` (moss / snow blanket with drips; `inner_shell=False` = open cap) + `cull_under_cap`, `drip_edge`, `ring_loft`, `blade`, `ribbon` (painted strips on a surface), `bloom`, `plank`, `spike` (`rows` > 2: rings for local hull padding), `rope_band`; art review 10: `unflare_foot` (a rock's undercut at the floor pushed out to the belly), `rock_contains` / `cull_inside` (faces buried in a neighbour rock), `facet_normals` (creased shading normals on big smooth faces), `ground_hull` (hull vertices within 2 cm of the floor extrude horizontally, never below it); `finish` (build → **spherised normals** per canopy lump (`lump_info(…, lift)`: + a tilt toward +Z) → `facet=` creases → **wind data** `AF_Data.g/.b` + UV1 `UVWind` → toon + outline → hull → `ground_band=` ground hull); `cloth_wind` (prop cloth weights: 0 at the hanging line → 1 at the tail, poles rigid); `ship` (FBX, merged manifest entry with `footprintTiles` / `blocking` / `outlinePx1080` / `outlineMix` / foliage parents / wind contract — `wind_slot=PROP_WIND_SLOT` for prop cloth —, review set with `ground_occludes`, ink gate recorded — plus the **upper bound** `BAKED_RIM_MAX_FACTOR`: the baked-hull rim median ≤ 1.75 × `outlinePx1080`, `inkGate.bakedRimPx`, DECISIONS R14); **`EnvReview`** / `save_native` (every env preview truecolour at scale 1, `pngio.write_png_native`; the 1:1 game crop fitted to the asset's projected bounds + 28 px instead of the kit's square box); `export_lod1` / `lod_preview` (static LOD1 FBX + `lods[]` + `lod1.png`); `EnvManifest` (also merges the top-level `terrain` section); `family_sheet` (every variant at the W1 camera, native 1080p, cells sized from the assets' projected bounds incl. the scale warrior, short labels), `scene_shot`, `tile_hash` (= `AbyssWorldUtil::TileHash`), `place`, `import_warrior` / `toon_material_files` (shipped hero + palettes read from disk) |
| `environment/foliage.py` | `SM_Foliage_Plains_Oak_A/B/C` (`tree`, + `_LOD1`), `_FruitTree_A/B` (`tree_round`, + `_LOD1`), `_BerryBush_A/B` (`bush`), `_GrassTuft_A/B/C` (`grass`), `_FlowerPatch_A/B` (`flower`), `_RedMushrooms_A/B` (`mushroom_red`), `_LilyPad_A/B` (`lily_pad_plains`) — palette `Foliage_Plains`, parents `M_AF_Toon_Foliage` / `M_AF_Outline_Foliage`; ground cover (grass, flowers) has no hull and uses `MI_AF_Toon_Foliage_Plains_Ground` |
| `environment/rocks.py` | `SM_Rock_Plains_Cluster_A/B/C` (`rock`), `_MossBoulder_A` (`boulder`), wall tiles `SM_Outcrop_Plains_V0..V5` (`outcrop_plains`; V2 / V5 hedges); the skirt hills were retired (art review 9: the runtime skirt is the terrain heightfield + the grove pool) |
| `environment/camp.py` | `SM_Palisade_Plains_Stake_A/B/C` (`palisade_plains`, 1.32 / 1.40 / 1.48 m tip included, mean 1.4 m, R1), `_GatePost` (`palisade_gate_plains`, not blocking), level-design runs `_Straight` / `_Corner`; camp kit `SM_Prop_Camp_Campfire/Well/Tent/Banner/Barrel/Crate/Torch/Hearthstone` (`camp_<type>`; sockets `flame`, `glow`, `gem`; the tent records a 2 × 2 body footprint, the banner sways on `MI_AF_Toon_Props_Plains_Wind`) |
| `environment/story.py` | story decor `SM_Prop_Story_ElvenRuins` (`decor_ruins`), `_GoblinTotem` (`decor_war_banner`), `_BrokenAltar` (`decor_broken_altar`, R6 canonical), `_DryWell` (`decor_dry_well`); lore `SM_Prop_Lore_AncientTablet/TornJournal/CrystalShard/CarvedStone` (`lore_<spriteType>`) |
| `environment/interactive.py` | `SK_Prop_TreasureChest` (`SKEL_Prop_Chest`: root + lid, `A_Prop_TreasureChest_Idle` / `_Open` 400 ms, notify `ChestOpen`; previews, `boundsCm` and the ink gate in the **closed rest pose**, plus `game_open_1080crop.png` / `closeup_open.png` gated on the Open end frame, `openBoundsCm`), `SM_Prop_GoldPile`, `SM_Prop_Event_RunePillar`, `SM_Prop_ExitPortal` (later chapters), `SM_Prop_ExitGate_Sealed` (W7), markers `SM_FX_ClueMagnifier`, `SM_FX_GuideArrow`, gather node `SM_Prop_Gather_EmeraldHerb` |
| `environment/goblin_camp.py` | spec §6.9 / R7: `SM_Prop_Goblin_Hut_A/B` (2 × 2 body), `_FirePit`, `_BoneTotem` (hide strips sway), `_SpearRack`, `_Crates` — non-blocking decorations placed by the level script (`placement` in each entry) |
| `environment/terrain.py` | `Art/Export/Terrain/`: `T_Terrain_Plains_Grass/Dirt/Stone/Camp_BC`, `T_Water_Plains_BC` (1024², one 3 × 3-tile period, painted from `Data/terrain_styles.json` like `GroundPainter`), `T_Terrain_Plains_Details_BC` (per-tile detail atlas), `T_Terrain_Plains_Noise_M`, `T_Water_Plains_Mask_M`; `composite()` = the exact web transition rules (`MF_TileBlend`); the UE material inputs (textures, colour spaces, UV rule, transition formulas, water constants) in `manifest.json → terrain.plains`; previews `Previews/terrain_plains/` |
| `environment/vignette.py` | the plains vignette (`Previews/env_vignette/`): every mesh **re-imported from its FBX** with the shipped palettes, placed like `AAbyssZoneActor` places it (outcrop variant / yaw from `tileHash`, palisade stakes by the neighbour mask, camp props facing the camp centre, lily pads on water tiles), the composited terrain with contact shadows, the warrior; `vignette.png` is a native 640 × 360 render, the crops native 1080p tiles read back pixel-exact, `hero_check` asserts the warrior's crimson and ink; `ground_check` asserts a plain grass tile renders within 3/255 of its painted texels (the ground plane is blacked out of the bloom source); `check_dry` keeps every prop's base off water and wall tiles (`--check`: dressing only, no render); tents turn their door to within 60° of screen-down; `vignette_footprints.png` paints the W5 footprints from the manifest (half-tile gate posts skipped) |

Previews: per asset `Previews/<SM_…>/` `game.png`, `game_1080crop.png` (shipped ink + bloom next to the baked hull alone),
`game_inkgate.png`, `closeup.png`, `turnaround.png` (+ `turnaround_2.png` … when a sheet is split into truecolour tiles;
trees add `lod1.png`; the chest adds `game_open_1080crop.png`, `closeup_open.png`, `anim_Idle.png`, `anim_Open.png`); family sheets in
`Previews/env_family/` (`foliage_trees`, `foliage_fruittrees`, `foliage_small`, `rocks_outcrops`, `outcrop_ridge`,
`camp_palisade`, `camp_kit`, `camp_kit_small`, `story_decor`, `lore_props`, `interactive_props`, `exit_gates`,
`goblin_camp`); `Previews/terrain_plains/`; `Previews/env_vignette/`.

Ink: decor class (line tone, `INK_MIX` 1.0) at 1.2 px — **1.4 px for `Props_Plains` decor** (DECISIONS R14, `envkit.PX_OVERRIDE`;
poles and stakes are 8–12 px wide at 1080p and their lit edge carries the warm rim), interactive props 2.0 px; every
gated cell is a native-1080p W1 crop measured with the terrain occluding the hull under the floor
(`ground_occludes`). Every entry records `footprintTiles` + `blocking` (W5: trees, boulders, outcrops, stakes, tents,
wells, barrels, crates, tall story decor and the goblin huts / totem / rack / crates block; ground cover, mushrooms,
bushes, campfire, banners, torches, lore props, markers and the exit gate do not — the hero walks onto the exit tile
for the W7 message). Goblin-camp props are non-blocking (spec §6.9: level-script dressing), the gate posts too (the
wall tile beside them blocks). Budgets (art review 9; hull ≈ 0.5 × toon via decimated hull proxies): oak 4.6 k toon + LOD1 1.5 k, fruit tree
3.3 k + LOD1 1.45 k; art review 10: canopy hull proxies at 80 %, padded closed and culled after (`canopy(cull_hull=False)`;
hull ≈ 0.6 × toon — at 55 % the padding put the baked rim at 2.2–2.7 px), the barrel's hoops and the torch's wraps / rim
carry their own ring hulls (padding the 3–5-ring proxies round them inflated the whole hull to ≈ 3 px); bush ≤ 2 k toon (hull proxies at the toon
ratio: coarser ones padded past the baked-rim bound), outcrop 1.1–1.2 k toon + ≈ 1.1 k hull (masses and caps bake their hulls
from themselves), rock clusters ≤ 0.6 k drawn, ground cover, mushrooms and lily pads ≤ 300 toon (spec §1.9).

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
* **Art review 10 — hulls at the floor**: a hull vertex whose body point is within 2 cm of the floor pushed down-and-out
  drew thin spikes fanning along the ground (`finish(ground_band=0.02)` → `ground_hull`: horizontal extrusion, never
  below the floor; floor-facing bottoms and buried points stay under the terrain). A round rock on a narrow foot shows
  floor under its belly and its contact-line ink becomes a separate line with a gap: bed the foot (`unflare_foot`).
* **Padded proxies must hug**: the gate now has an upper bound (baked-hull rim median ≤ 1.75 × `outlinePx1080`). A
  mound's proxy padded round coins tilted off its slope made a ragged 3–6 cm shell (gold pile: one unpadded mound hull,
  flush coins without hulls, hulled stacks / spills); decimated *open* shells (outcrop masses culled under their caps)
  padded by up to 7 cm — bake from the shell itself when its open boundary hides under another part.
* **Separate rocks are separate**: a front stone whose centre sits inside the big mass reads as one egg; put its centre
  just outside (its own hull then inks it against the mass) and cull faces / cap drips buried in the neighbour
  (`rock_contains` / `cull_inside`).
* **Big smooth faces airbrush the bands**: the 0.02-wide band smoothsteps spread over dozens of pixels on gently curved
  faces; crease the shading normals (`facet_normals`) or build flat panels (the tent's three creased roof panels per
  side).
* **Decals on faceted parts**: a flat plate's long triangles span the ridge between two facets and sink behind it (the
  hearth rune's "outlined" flame): split the decal on the ridge and snap every vertex onto its facet; studs on a lathe
  cap sit at a facet centre on the facet plane (toadstool spots).

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
#   --readfix Warrior,Mage,Rogue --fams axe,shield,staff [--no-retrack] | --readfix Rogue:shield:Cast01   every strike /
#        cast clip that fails READ_GATE: per-frame roll → hit-stop hold → re-planned track (+ hold, roll), exact QA kept
#   --rolltrack Cls[:fam:Clip] --fams axe   per-frame readability roll (Viterbi) · --carry-roll Cls --fams axe   one
#        roll for every carry clip · --lift Cls:fam:Clip[:whole][:clothN]   tilt the item about the fist where it or its
#        ink hull dips under the ground (whole: re-plan the clip, e.g. for a cloth-pair margin: cloth17)
#   --tracks … --force Clip,… --force-only   re-track just those clips (no whole-class QA first) · --qa --json out.json
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
  hang), the crystal is faintly self-lit (e .6). Fifth look: the violet shaft had the hue and value of the mage's robe — the T1 rod is a light honey wood (#C8955C) with the web's violet on its three rings (role `ring`), a gold collar under the setting (686 → 806 toon tris: inside the 0.7 k floor). The wand and the mace have no item base yet (`bases` empty): dormant until the core `WeaponType` gains them.
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
  (e .12) with pale twine, the gel's highlight band wider and brighter (e .85). Low wide blobs stand on a tucked base
  (coherence pass, `GEL_LIFT`): spread flat, the W1 camera's lower contour is the ground contact and its ink lies under
  the terrain — a green gel then meets the green grass with no line along ~40 % of its silhouette.
* **Thin hanging hardware** (the pendant chain): a 9 mm round cord disappears in its own 2 px ink; lay it as a 1.5 cm
  flat cord (coiled beside the locket) and give small fittings (the bail) a light metal.
* **Readability is gated, not only planned** (art review, fifth look): the staff, the axe and the shield shipped
  failing what their planners scored, because only the bow was in `READ_GATE`. All four are gated now (manifest
  `readGate` per family, `inHandQA.classes.<cls>.read`): the axe takes the bow's floors (min over se / sw of |bit
  normal · view ray|: hit-stop mean .45, every hit-stop frame .35, any other frame .15 except ≤ 2-frame flips); the
  shield its **signed** face metric (face normal · direction to the sw camera, < 0 = its back: hit-stop mean .50 =
  30°, every hit-stop frame .25 = 15° — the rogue's Cast01 release flings the off hand forward and .27 is the most
  any strap-plausible attitude reaches there — never back-facing for > 2 frames; candidates: the forearm's own roll ±90°,
  the boards ±30° on the arm, `shield_candidates`) and the face disc from 2 frames before the contact
  on (in the wind-up a soft planning cost graded by how much of the face the boards cover: the rogue's off-hand fist
  itself crosses her face in the Attack02 / 03 and Cast01 / 02 wind-ups, where an exact track found no shield attitude
  on that forearm that clears it; hurt flinches are exempt — a raised guard); the staff
  `staff_frame` below — on **every** carry frame (Idle / Walk / Run / Hurt; the warrior's Hurt recoil may dip for ≤ 2
  frames, `flinch`: no grip of 12 k that clears the plate keeps the orb up there) ≥ .65 with the orb ≥ .30 of the shaft
  above the fist, visible and clear of the head; on strikes / casts the bow's floors, the orb visible and clear of the
  head on the hit-stop and lifted ≥ .30 at a cast's release.
* **Hidden is not on screen** (fifth look): `read_staff_screen` measured the fist → orb vector's *projected* length,
  so the warrior's staff passed while it stood behind his body from sw (the right hand is the far one there) with only
  the orb poking out beside the plume. `Occluder` (per frame and view: barycentric surface samples ≤ 1.4 cm apart of
  the posed body — without the holding hand — and the class's attachments that stay beside the item, splatted into a
  1.2 cm depth buffer) gives `staff_screen`: `vis` = the shaft's on-screen length **not hidden behind the body or the
  shield** / the 81.5 cm shaft (the hilt zone counts where the fist itself is visible), `su` the orb's lift, `orb`,
  `behind`, `headd` (the orb's screen distance from the head), `out` / `orbOut` (off the body's silhouette). Both views
  are full penalties (sw weighs as se), with steep floors just above the gate; strikes also keep the orb a hand off
  the head / hood (≥ 30 cm on screen) and the orb plus a third of the shaft off the silhouette (the rogue's Attack03 se
  contact laid the staff across her torso, the orb against the hood). Staff tracks plan on a 600-direction sphere (on
  the warrior's Idle only ~0.4 % of all grips keep .65 in both views, and most of those cut the plate).
* **The bit follows the hand** (fifth look): one roll per clip left the axe bit edge-on at most hit-stops (Warrior
  Attack02 .007, Mage Meteor .005). `roll_track` picks a roll offset about the haft per frame (−90…90°, ≤ 30°/frame,
  Viterbi) among the offsets that pass the exact QA; where the haft itself points along a view ray no roll helps
  (the mage's Blizzard release: haft · view .97–1.0) and `read_fix` goes on to the hit-stop **hold** (one world
  attitude, `read_axe_floor`), then a re-planned track (`Searcher.track` with the family's readability, the hit-stop
  ×`HIT_READ_W` for axe and shield too) + hold + roll — first passing step wins. `carry_roll` turns the axe's
  READY / locomotion / hurt grips by one angle for the best mean over every carry frame (mage mean .11 → .48).
* **Lowest point = toon + ink hull** (fifth look): the clip sheets' LOW label measured every vertex (with the review
  render's screen-constant hull) and showed hulls 1–2 cm in the terrain while the QA, on toon vertices at −0.5 cm,
  passed. `Item.low` / `weapons.item_low` both measure the toon surface and the **baked** hull now, floor 0 cm;
  `lift_pass` tilts an item about the fist in the world (6–40°, ≤ 15°/frame, ±5 frames of ramp) where it dips (rogue
  sword / axe / mace Attack03 and Death, the warrior's axe Dodge and bow Death), and with `whole` + `clothN` re-plans a
  clip for a cloth-pair margin.
* **Grips ship only where the mesh is shown**: a class whose own `SM_Hero_*` weapon of the family wins `FindWeapon`
  (mage staff, rogue bow, warrior shield / sword, rogue dagger) gets no `grips` record and no `inHandQA` class
  (`inHandQA.notShipped`): those grips were never planned to pass (the rogue's bow: 573 body pairs).
* **Potion glow core** (fifth look): the spec halo (#FF4A4A α .45 at the ring, 'over') mixes to khaki over the plains
  grass (174,146,75 — next to the 'rare' gold) and the MP halo to teal; a second card (`loot.glow.core`: #D81822 /
  #1E46E6, α .92 solid to 1.22 × the flask radius, gone at 1.65 × — at least 15 cm, or the slim S vial kept a 6 px
  khaki ring) keeps the band hugging the bulb red / blue over
  grass (213,48,39) and dirt (216,46,42).
* **Layered thin parts and the ink measure**: the herb's fifteen leaves and the pendant's coiled cord each carry a
  hull, so the rim walk crosses two or three hull layers (median 4 px against the 2 px class). Their hulls are baked
  thinner (`loot.HULL_SCALE`: leaves / crescent ×.8, blossoms ×.85, cord / bail ×.75 — the UE WPO adds the same term to
  every hull vertex) with darker line tones (hull mix .7–.8) for margin under lum 64; the remaining median is a
  documented per-asset exception (`inkGate.exception`).
* **Review scope**: the family's previews are `Previews/SM_Weapon_{Axe,Bow,Dagger,Mace,Shield,Staff,Sword,Wand}/`
  (closeup, tiers, `game_inhand_<Class>`, `clips_<Class>_<Clip>[_n]`, game, game_inkgate), `SM_Pickup_*/` (33),
  `SM_Loot_Bag/`, `weapons_lineup/` and `loot_family/` — there is no `item_*` directory.
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
| `vfx/fx_textures.py` | `Export/VFX/T_FX_<Sprite>.png` — the web `FxTextures.ts` painters redrawn as cel sprites (crisp silhouette, 2–3 value bands, one halo only where the shape emits light), **linear** RGBA (sRGB off), white = tintable, pre-coloured cells (8 flame palettes incl. the camp flame, frost shard, arrow, coin, rock, chain); strips of N cells picked by the instance Variant; every cell inside a transparent gutter (`fxpaint.gutter`); `sprites.json` (cells, blend, size, note, the Impact `cellScale`) |
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
* **Projectile heads must not outlive the flight**: `HandleProjectileEnded` stops the head recipe with
  `Stop(handle, false)` — emission ends, live particles stay on the (now parked) anchor. A count-1 head living the
  longest flight (520 ms) stood in the target at full opacity; arrow / shard heads are `fx_recipes.arrow_head`:
  60 /s copies living 90 ms on the attached anchor (gone ≤ 75 ms after the impact: solid, then the 2-frame
  dissolve), flat on the ground plane and turned to the flight (orient ground + `alignToBlow`, so no copy takes the
  `dir: blow` random climb). A piercing arrow flies on through its target from the `.hit` recipe (a free arrow at
  the per-target arrow speed 2525 cm/s, braking, its comet fading, the mesh shrinking out) — level only with the
  layer key `flat` (`integration.flat`, simulated by the previews).
* **Cells never touch their border**: a shape or halo reaching the cell edge at non-zero alpha is clipped hard by
  the quad (and bleeds into the strip neighbour under bilinear). `fxpaint.gutter` fades the outer px of a cell
  (`fx_textures.GUTTER_SIDES`; Bolt / Beam / Flame reach their borders by design); the impact stars are drawn
  fitted inside a 6 px gutter (`impact_fit`) and the recipes enlarge their quads by 1 / `cellScale` (`sprites.json`)
  so they keep their size.
* **A weight step must read at 1:1**: hit.impact.light and .normal differed only by ring r 10 vs 13 and 4 vs 6
  sparks and looked alike; the light tap also lowers its core / glow / ring alpha and flies 4 short slow sparks
  (`light_tap`). The runtime plays `hit.impact` until it resolves the profile id (`integration.hitImpactProfiles`).
* **Evenly spaced orbiters need slots, not a rate**: a rate emitter gives each orbiter a random angle (the stun
  showed 2 bunched stars). status.stun bursts 3 stars (orbit angle 360·i/3) every 500 ms — at 240 °/s exactly one
  slot — each landing on the previous set's stars (`STUN_SLOT_MS`).

## 15. Icons, glyphs, emblems and portraits (`icons/`)

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/icons/items.py       # 100 item icons (~4 min)
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/icons/skills.py      # 40 × 2 skill textures (~2 min)
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/icons/glyphs.py      # 15 glyphs, 3 quest items, 2 emblems
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/icons/portraits.py   # portrait coverage + medallion sheets
#   items.py / skills.py / glyphs.py --only a,b,c   → scratch sheets only (iteration), nothing shipped
```

The icons are rendered from the **shipped** meshes (`SM_Weapon_*`, `SM_Pickup_*`, `SM_UI_QuestMarker_*`): re-run
`items.py`, `skills.py` and `glyphs.py` after those re-ship, and `portraits.py` after any portrait owner re-ships (the
stash alias is a byte copy). Each script rewrites its own preview sheets (stale `_<n>` tiles of a sheet are removed);
a whole sheet that is no longer written (fewer pages) is not, so clear the family's preview folder first.

| File | Content |
|---|---|
| `icons/iconkit.py` | the icon renderer (spec §9, R10): orthographic studio (camera along +Y, the **game's view-space key light** — upper-left, toward the viewer — through the shared `AF_ToonCore`), shipped meshes re-imported from `Export/` (`import_fbx` with any palette atlas, `import_fx` for FX meshes in a tinted toon look), `IconPalette` (icon-only models: registry + atlases in scratch, never shipped), `place` (orient by local axes: `toward`, `up`, `roll` / `yaw` / `pitch` / `spin`), `fit` / `Frame` (96-unit icon space), `render_icon` (colour + depth + region-key passes at 2× → coloured line art → web post), `icon_gate`, `atlas` / `ui_sheet` previews, `IconManifest` (merges the top-level `ui` section) |
| `icons/shapes.py` | icon modelling helpers in the studio frame (X right, Z up, viewer −Y): `pillow` (cushioned slab from an outline), `slab`, `torus`, `links` (chains), `brilliant` (faceted stone), `lump`, `Surface` (front ray-cast probe), `dome_on`, `align`, outline builders (`bez`, `blob2d`, `circle2d`, `rrect2d`, `star2d`, `heart2d`, `capsule2d`, `ribbon2d`, `spiral2d`, …) |
| `icons/item_models.py` | the icon-only item models, one builder per web icon family (`helm`, `armor`, `gloves`, `boots`, `belts`, `ring`, `amulet`, `material`), authored in the web's 96-unit coordinates (`W(x, y)`, 1 u = 1 cm) with `drape_strip` / `surf_tube` details laid on the surfaces |
| `icons/weapon_models.py` | icon-only weapon models with the base's **shape** (the web variant tables `ItemIcons.ts` SWORDS / DAGGERS / BOWS / STAFFS / SHIELDS): `blade_parts` (length × half-width, profile straight · broad · wavy · serrated · curved · needle as the web `bladeWidth`, guard bar · cross · wing · disc, grip, pommel, gem, fuller, rust, drip, glow; hollow-ground diamond with bright edge bevels and the web's darker far flat), `bow_model` (half-height, bulge, recurve, limb width, horn / steel tips, bands, spikes, feathers, glow), `staff_model` (oak · arcane · rune · elder · lich · abyss heads and hafts), `shield_model` (round wood · iron heater · tower · kite · rune · abyss); scaled like the web (`framed`: blades by the web `bladeSpec` fit, daggers smaller) |
| `icons/gem_models.py` | icon-only cut stones (web `IconKit.ts` `facetGem` / `gemCut`, `ItemIcons.ts` `gemSpec`): `facet_gem` builds a crown seen table-on — girdle polygon, table (outline × `table`, shifted up-left), one ring of facets (`plain`), two rings (`step`: the emerald) or a round brilliant (`brilliant`: octagonal table, 8 star, 8 kite, 16 upper-girdle facets) — every facet its own swatch from the web ramp (glint · light · base · shade · deep by how it faces the light ± an alternating offset; `Facets.colours` nudges equal neighbours apart) so the seam pass draws the facet lines; a line-less table overlay (darker lower-right, the white reflection wedge); girdle band + pavilion behind. `gem(typ, tier)`: ruby oval, sapphire pear, emerald step cut, topaz trillion, diamond brilliant (tier 1: the web's chipped plain crown), `GEM_PITCH` −9° / `GEM_YAW` −6°, fixed frame (icon units = web units); `essence()` (m_essence) |
| `icons/items.py` | the item-icon table: every base of `Data/item_bases.json` → one look (swords, daggers, bows, staffs, shields: `weapon_models` per base; axe / mace / wand: `SM_Weapon_<F>` + the tier palette from the manifest `bases`, the axe / mace haft shortened for the icon, `HAFT_SQUEEZE`; consumables: `SM_Pickup_*`; gems: `gem_models`; the rest: `item_models`), coverage check, ship |
| `icons/motifs.py` | skill / glyph motif kit: `Comp` (modelled parts + placed shipped meshes), `flame`, `swoosh`, `arrow`, `skull`, `heart_pts`, `cloud`, `bolt`, `crystal`, `star`, `drop`, `snowflake`, `ring2d` / `ring3`, `ribbon3` (swept curved ribbons), `weapon` (a shipped weapon placed like the web `blade()`, `widen` for emblem readability), `blade` (the base's own web blade from `weapon_models`, placed like the web `blade(c, key, …)`), `fx` |
| `icons/skills.py` | the 40 skill motifs (web `SkillIcons.ts` MOTIFS, one function each) + the emblem plate |
| `icons/glyphs.py` | `T_UI_Glyph_*` (15), `T_UI_QuestItem_*` (Ch1: herb and gel as icon models of the web drawings — `q_herb`, `q_gel`; pendant from its ground mesh), `T_UI_Emblem_Villain` / `_Generic` |
| `icons/portraits.py` | portrait coverage (every Ch1 speaker resolved like `FAbyssUiContext::Portrait`), checks, alias copies, medallion previews |

Outputs: `Art/Export/Icons/{Items,Skills,Glyphs,QuestItems}/T_UI_*.png` (straight-alpha RGBA, transparent background,
no UI frame — Slate draws slots, quality borders and the skill frame), `Art/Export/Portraits/T_UI_Emblem_*.png` and the
alias `T_UI_Portrait_Stash.png`; the content build imports every `T_UI_*` it finds under `Export/{Icons,Portraits}`.
`manifest.json → ui.<texture>`: `kind` (`itemIcon` · `skillIcon` · `skillMotif` · `glyph` · `questItemIcon` · `emblem` ·
`portrait` · `portraitAlias`), `file`, `px`, ids (`iconId` + `bases` + `default`, `skillId` + `damageType` + `tree` +
`backdrop`, `glyphId`, `questItemKind`, `artIds`), `source` (mesh + palette / tier, or the icon model), `gate`, `previews`.
Previews: `Previews/icons_items/<category>_p<n>_1x.png` (native), `_half.png` (0.5×), `_ui.png` (48 / 36 px in slots with
the five quality frames); `Previews/icons_skills/<class>_p<n>_1x.png`, `_half.png`, `_motifs_half.png`, `_ui.png`;
`Previews/icons_glyphs/`, `Previews/icons_quest/`; `Previews/portraits/medallions_<n>.png` (conversation 64 px brush in a
circle, story `say` masked r 53, class well 66, class card 170) and `emblems_1x.png`.

The look (web `IconKit.ts inked()` on top of the 3-D toon render):

* **Line art in screen space**, not the hull (a 1.3 cm hull is a third of a 6 cm coin at icon scale): an **overlap line**
  (`LINE_U` 1.15 u, the nearer part's line tone mixed 18 % toward the ink) on the far side of every true depth
  discontinuity (`depth_breaks`: a jump > 1.6 u *and* > 3× the surface's own slope on both sides — a sphere's steep rim is
  not an overlap), and a thinner two-sided **seam** (`SEAM_U` 0.5 u each side) where two palette regions of different
  base colour meet on one surface (region keys = object × swatch from a UV pass, mode-filtered so sub-pixel strips do not
  dash; emissive regions and `nolines` objects get none).
* **Web post**: rim `rgba(255,240,210,.45)` 1.1 u on the upper-left edge, ink `#140C1C` 2 u (skill motifs 2.1 × 0.9,
  glyphs 3.2 u with rim 1.4 u, quest markers 3.6 u), drop shadow black .55 blur 3 u offset (1.5, 2.5) u, under-glows
  (`auto_glow` = the emissive regions' own colour; or explicit `Accent('glow', under=True)`), over-accents (four-point
  `sparkle`, `rays` for tier-5 gems). Everything renders at 2× and is box-filtered down.
* **Gate** (`icon_gate`, recorded per texture): ink ≥ (ink − 0.5 u) on ≥ 98 % of the outer silhouette, ink pixels' lum
  p95 ≤ 40, an empty border (nothing clipped), the body spanning 30–93 % of the square, and nothing of the finished
  texture on its edge (`finalBorderAlphaMax` ≤ 2): the drop shadow and the soft accents (`glow`, `webglow`, `glowline`,
  `rays`, `auto_glow`) fade out over the outer `GLOW_EDGE_U` 7 u, so a big emissive haze (the abyssal bow) or a tight
  glyph's shadow never ends in a straight cut in the slot.

Conventions learnt on the icons:

* **Presented like the web**: blades on the diagonal tip up-right with a slight `spin` so one hollow-ground facet takes
  the light; bows string down-right; shields face-on with a little yaw; the axe / mace / wand meshes pitched ~35–58° away so
  the ortho camera foreshortens the shaft and the head reads at 42 px — the axe and mace hafts are also shortened for
  the icon (`items.HAFT_SQUEEZE`: local y between butt and head × 0.60 / 0.52, the butt moved up rigidly; art review r3:
  at 36 px the long shaft left a stick with a dot), so the head spans ~40–55 % of the square; gems table-on like the web
  with a small tip (`gem_models.GEM_PITCH` −9°: a sliver of girdle shows; the old −32° turned the shipped pickups'
  twisted pavilions into pleats); potions share one scale on a
  common baseline (S < M < L), their under-glow scaled with the bottle (0.56 × its height, centred on the body); scrolls turned −28°; boots a pair turned 24° to 3/4 (`boot_solid`: a round shaft and a toe
  box, not a cushion).
* **Shape per base, not per tier** (art review of the icons): the inventory tells a broad sword from a short sword by its
  silhouette, so the five families with web variant tables render `weapon_models` per base (31 looks, every `edgePx`
  in a family different); the in-world `SM_Weapon_<F>` stays one mesh with tier atlases (R4). T5 / T6 and staffs get the
  under-glow of their emissive accents.
* **Two-tone blades**: the icon key light comes toward the viewer, so no front-facing flat ever reaches the toon shade
  band — the far flat of a blade is painted darker (`blade_dk`, the web's `bt.shade` at α .6); `PatchedPalette`
  recolours single swatches of shipped meshes for an icon, `swatch_faces` / `delete_swatch_faces` / `swatch_centroid`
  edit or locate painted parts (the pendant's ground coil replaced by a loop of links, its glow centred on the ruby).
* **Facet line work = per-facet swatches** (art review r3): a convex stone has no depth breaks and one-swatch facets
  draw no seams, so cut stones (`gem_models`) give every facet its own colour from the web ramp and keep neighbours
  ≥ 0.075 apart (`seam_min_contrast` .05, `seam_u` .42): the seam pass then outlines every facet like the web's
  facet seams. Clear stones (topaz, diamond) pull their shade / deep facets 30 % back toward the body colour. The
  same trick draws a leaf's midrib (two half-leaf pillows, lit and shaded) and twine wraps (abutting segments).
* **Saw-teeth on near edge-on forms**: a torus band or a gem girdle seen almost edge-on folds over itself in short
  steps the depth-break test catches as dashes; `IconStyle(depth_step_u=, line_min_u=)` keeps the real overlaps and
  drops line pieces shorter than `line_min_u` (rings 3 u / 9 u). Nested colour layers (the villain emblem's flame
  tongues) sit 0.6 u apart under a 2.4 u `depth_step_u`, so each tongue gets one overlap line and its layers meet in
  seams (2.2 u apart they drew doubled, offset outlines).
* **Glows the web way**: `Accent('webglow')` uses the web `glow()` stops (α · .45 α at 0.4 r · 0), which still shows
  round a body that fills most of its radius (the ley fruit, the quest gel, the gem tiers ≥ 3, the rare essence);
  `'glow'` is the steeper (1 − d)² falloff and vanishes under a body that covers its centre.
* **Over-accents that are never inked** (the web draws them in `over`): `'sparkle'` (always upright, four points —
  a rotated glint reads as a cancel mark in a slot), `'webrays'` (gem tier 5: `n` short strokes outside the stone,
  `width_u` at the root tapering to a point, bright `mix(colour, white, .45)`, soft halo, no edge window) and
  `'bubble'` (venom bubbles: body `hex`, rim `hex2`, a thin outline in the body's own line tone — modelled rings took
  the 2.1 u motif ink and read as black donuts).
* **Emblem readability**: skill motifs widen shipped blades 1.4–1.7× across (`motifs.weapon(widen=)`) — at 42 px a true
  5 cm sword blade is a hairline next to the web's 8-unit broadsword.
* **Pillows fill concave curves** (`pillow` scales rings toward the centroid): spirals, comet tails, wisps and trap jaws use
  `ribbon3` (a swept flat tube); `links` builds each chain link as a real oval ring (a scaled torus thins its wire).
* **Compositing order**: `over(dst, src)` = src over dst — under-glows go behind the inked body *and* its shadow, sparkles
  last.
* **Portraits** are not re-rendered here (their character generators own them); `portraits.py` verifies them and writes
  only alias copies where the runtime name differs (`stash` → `T_UI_Portrait_Stash` ← `T_UI_Portrait_StashKeeper`;
  **re-run it after the stash keeper re-ships**, the alias is a byte copy). It also covers the Q3 companion
  (`pet_sprite` → `T_UI_Portrait_Pet_Sprite`, resolved like `FAbyssUiContext::PetPortrait`). Each manifest entry lists
  the one medallion tile its row lands in (labels sit inside the row, so a split tile never orphans one).
  The conversation panel and the class cards draw the square portrait inside a circle without a mask (the busts' flat
  bottom shows below the ring, `medallions_*.png`); the story overlay's `TexturedCircle` is the clean medallion.
* **UI textures are minified 4–7×** (256² icons drawn at 36–48 px, 128² glyphs at 14–32 px): they are imported **with
  mips** (`Scripts/abyss_content/ue/textures.py`: kinds `ui_icon` / `ui_portrait` → `TMGS_SIMPLE_AVERAGE`,
  `TEXTUREGROUP_UI`, never streamed; the manifest entries carry the same hint), or Slate's bilinear sampling would skip
  texels of the 1–2 px line art. The `_ui.png` sheets show what Slate draws (`slate_sample`: the SimpleAverage chain,
  the nearest level for the slot size — UE's UI group `MipFilter=point` — sampled bilinearly), not an ideal box filter.
* **`previews[]` per texture**: each manifest entry lists only the sheets that show it — and, for a sheet the native
  writer split into `_2.png` … tiles, only the tile(s) its cell lands in (`save_preview_cells` + `atlas_boxes` /
  `ui_sheet_boxes`).

## 16. Ley-beast companion (`pets/`)

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/pets/sprite.py [--stage 1|2]   # SK_Pet_Sprite[_E1|_E2]
#   --quick [Clip frame] (close-ups / turnaround / game crop → $AF_SCRATCH_ROOT or the system temp dir)
#   --anims [Clip …] · --no-previews · --no-verify
```

`SK_Pet_Sprite` (灵脉精灵, `beast_pet_sprite` / `pet_sprite`; DECISIONS Q3: Chapter 1 ships the `pet_sprite`
companion) on `SKEL_Pet_Sprite`, palette family `Pets`, category `Characters`, ink class `small_monster` (1.4 cm baked)
with a per-asset **3.5 px** request (`OUTLINE_PX`, gate 3.0 px: at the class's 3.0 px wing tips, sprout and wisp kept
≥ 3 px on only 72–78 % of the worst cells; the web inks pets heavier than heroes).
The web `Sprite.ts` / `Fly.ts` translated: a ≈ 20 cm glowing seed-ball body hovering 0.47 m up (web hover 22 u + 3,
1 u = 1.86 cm), big dark eyes with glints, smile / "o" mouth, peach cheeks, a two-leaf sprout, four dragonfly wings,
two small arms reaching forward, a wisp tail ending in an emissive tip. 17 bones, all children of `root` (`body`,
`tail_01..04`, `wing_l/r_01/02`, `arm_l/r`, `sprout`, `eye_l/r`, `mouth`, `mouth_o`); clips keyed as bone basis
matrices from the web `flyPose` tracks (`SpriteAnims`): Idle 667 · Walk 600 (ref 4.2 m/s) · Attack01 333 (Contact
250) · Cast01 400 (Release 200) · Hurt 200 — no Death (an exhausted pet idles at 0.6×). The body is not emissive
and the tip only faintly (its bloom washed the ink off the rim of a 60 px creature): the ley light is the runtime glow
cards — body aura and tail-tip glow — placed **behind** the pet (manifest `fx.*.placement`, depth-tested like the elite
crown's halo). The previews composite them behind the coverage (`BehindGlow`, patched into `mship.MReview` for the
pet's process only), so they show the shipped look and the ink gate still sees the ink; the portrait
`T_UI_Portrait_Pet_Sprite` (`AbyssUiContext::PetPortrait`) lays them under the creature. Shipped through
`monsters/mship.py` (`MonsterShip(category="Characters")`: FBX + LOD1 + manifest + QA + re-import verification + the
full preview set in `Previews/pet_sprite/`).

Evolution (`--stage`, web `PetStage`, artId `beast_pet_sprite_e1` / `_e2`, pets.json levels 10 / 20):
`SK_Pet_Sprite_E1` (×1.13 of stage 0, lighter skin, longer wings, a crown of five leaves) and `SK_Pet_Sprite_E2`
(×1.27, five blossoms gold / pink / cream, cream wings with gold veins, a gold halo circlet round the sprout) — same
17 bones and clip set, previews `Previews/pet_sprite_e1/`, `pet_sprite_e2/`, aura +2 u and +.05 α per stage, orbiting
motes in `fx.motes`. The crown pieces hug the head (`conform`) inside the ball's padded ink, as the web paints them on
the head: five separately inked 2 px leaves read as a black cap at 1080p. One portrait (stage 0).

**Superseded in the dark remake (§28):** `pet_sprite` is re-dressed as 灯魂 / the Lanternwisp
(`pets/sprite_lanternwisp.py`, the same rig and clips). The ids `beast_pet_sprite`, `pet_sprite`, `_e1`, `_e2` and the
portrait `T_UI_Portrait_Pet_Sprite` belong to `SK_Pet_Lanternwisp*` now; `sprite.py` ships the seed sprite with no game
ids (`SUPERSEDED_BY`, `LEGACY_GAME_IDS`) and no portrait, so a re-run never takes them back.

## 17. Chapter-1 coherence pass (`coherence/`)

```bash
python3 unreal/Art/blender/coherence/checklist.py                       # → Art/Export/CHECKLIST.md (no bpy; exit 1 on a GAP)
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/coherence/lineup.py      # lineups + metrics
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/coherence/vignettes.py   # camp + goblin camp
#   the march (§31): lineup.py --march [characters monsters <prop group>…] [--characters | --props]
#                    vignettes.py --march | knellwatch red_field ashwheat
```

| File | Content |
|---|---|
| `coherence/cohkit.py` | re-imports shipped FBX with their own family palettes and manifest ink (`import_character`: clip pose, attachments, body / child items, hunt `variants` with the elite crown on `fx_overhead`, NPC scenery; `import_static`), the composited plains ground (`terrain.composite` + web contact shadows), `render` (screen-constant ink per mesh + bloom), `ink_gate_group` (one figure's gate on its own coverage, rendered only in its box) |
| `coherence/lineup.py` | `Previews/ch1_coherence/lineup_characters*.png` (heroes, ley sprite, monsters + both hunt variants, every NPC, the sprite's two evolved looks) and `lineup_props_{interactive,camp,story,goblin,loot,foliage}*.png` (+ `_inkgate`; `loot` = every ground-loot kind side by side), packed so no two silhouettes overlap; per mesh: ink gate (characters at the Chapter-1 gate: rim ≥ 3 px, `cohkit.CHAR_GATE_PX`; edges within 2 px of a bright emissive FX accent left out as every ship does, `EMISSIVE_EDGE_MIN`), median rim vs the shipped request, CIELAB chroma and HSV saturation (mean, p75), value spread, lit side, posed top vs the exported rest top and `heightCm` → `coherence/lineup_metrics.json` with the cross-family rules |
| `coherence/vignettes.py` | `vignette_camp*.png` (MapGen camp stamp + `BuildWalls` palisade + the props and NPC slots **parsed from `CampLayout.h`** (`kCampProps`, `kCampNpcSlots`, `CampNpcFacing`: every camp NPC looks at the fire (no tents, W11), a hero within 3 tiles turns them; variant by `TileHash(RoundToTile(centre), 23)`) + the heroes at the gate) and `vignette_goblin_camp*.png` ((40,52): huts, pit, totems, goblins, shaman, chief, the red-cap and a slime on the trail, the rogue and the ley sprite coming up it); native 640 × 360 overviews + 1080p 1:1 crops |
| `coherence/checklist.py` | every id the game resolves for `emerald_plains` (the runtime chains of `WorldContract.md` §2.2 and `AbyssUiContext`): heroes + signature clips, I6 weapon types, zone monsters + mini-boss + hunt variants + crown, camp / field / quest / event NPCs + markers + scenery, the Q3 pet, MapGen decor pools, walls, camp props (`CampLayout.h`), story / lore / hidden-area / exit / event / quest meshes, ground loot, 97 item-icon bases, 40 skill icons + VFX recipes (sprites, meshes), glyphs, portraits and emblems of every Ch1 speaker, palettes, terrain; per-kind manifest fields, files, LODs, clips, previews ≤ 200 KB, ink gates; the §1.3 rules (class width or a documented per-asset request above it), the Chapter-1 character gate (every hero / monster / NPC / companion `inkGate` gated at ≥ 3 px and passed), R2 / §1.9 rules + the lineup metrics → `Art/Export/CHECKLIST.md` (every requirement, every manifest asset and UI texture with its status) |

Cross-family findings fixed in the responsible scripts (pass 1): the kit `Manifest.save` dropped other generators'
`ui` / `terrain` updates (stale copy written back; now only sections changed by the saving instance are written,
smoke-test check); `Pets` added to the kit's palette families; `SM_Prop_GoldPile` (`environment/interactive.py`)
rebuilt in the loot family's coin language (it read as a cheese wheel next to `SM_Pickup_Gold_L`); the missing Q3
companion `SK_Pet_Sprite` + its evolved looks + its portrait (`pets/sprite.py`).

Final pass (after the family review rounds):
* **Character ink**: heroes and the boss ship 3.5 px, the goblin family a documented per-asset 3.5 px (art review), but
  the NPCs and the ley sprite still shipped their classes' 3.0 px gated at 2.5 px — against the Chapter-1 character gate
  (dark rim ≥ 3 px on ≥ 90 % of the silhouette edges at 1080p) the NPCs kept 61–83 % in the lineup and the sprite 72–78 %
  in its worst cells, thinner-inked than the goblins beside them. `npcs/npc_ship.py` (`PX` 3.5, `ink_slot`, `ink_gate`:
  the NPCs, their held props and the `SK_NPCKit_*` parts) and `pets/sprite.py` (`OUTLINE_PX` 3.5) now ship the same
  documented per-asset request (`outlinePx1080` 3.5, `outlinePxClass`, `outlinePxNote`) gated at 3.0 px; all 9 NPCs,
  the props, the kit parts and the 3 sprite stages re-shipped (NPC cells min rim 93–97 %, sprite 99–100 %). Every
  Chapter-1 character now inks 3.5 px; `checklist.py` gates every shipped character's `inkGate` at ≥ 3 px and accepts a
  per-asset request only above its class with its note.
* **`SM_Pickup_Quest_Gel`** (`items/loot.py`): spread flat on the ground (r 9 cm at z 0, widest 2 cm up), the W1
  camera's lower contour was the ground contact — ~41 % of its ink lay under the terrain and the light-green blob met the
  green grass with no line (lineup gate 85.7 % dark); it now stands on a tucked base (`GEL_LIFT`, belly 3.8 cm up) and
  is inked all round (loot re-shipped; its icon is its own model, unaffected).
* **`outlineMix`** missing on the hero items (`heroes/common.ship_weapon`), the anvil stump and the quest markers
  (`npcs/npc_dressing.py`): written by the scripts now and filled in place through the kit writer (metadata only).
* **Camp vignette** read a stale copy of the camp layout: it parses `CampLayout.h` (layout v2: NPCs at the tent doors).
* **Dev-mode paths**: a dozen family scripts hard-coded this pipeline's session scratch directory for their `--quick` /
  `--debug` output (the icon kit's icon-only palettes too): all use the new `kit.paths.scratch_root` (smoke-test check).

Measured and accepted (`coherence/lineup_metrics.json`, final pass): §1.5 hierarchy by the p75 **CIELAB chroma** C* of
each mesh's toon pixels — heroes 34 ≥ NPCs 27, monsters 56 ≥ NPCs, heroes ≥ decor 30, interactive 49 ≥ decor (HSV
saturation is kept in the metrics but not ruled on: (max − min) / max rates the camp's dark wood and canvas ≈ .55, as
"saturated" as the warrior's crimson; the goblins' green skin out-chromas the heroes as the spec's "high, greener"
allows, the potions and gems are the loot's colour pops, and the grass tufts sit at the `#74a247` family the spec names
for the ground). Ink medians hero 3.8 · boss 3.9 · monster 3.8 · ley sprite 3.9 · NPC 3.7 · interactive 2.1 · decor
1.4 px; 22 character figures ≥ 3 px on ≥ 94.8 % of their edges; every family lit from the upper left (one `L_KEY`; only
the icon studio has its own view-space key); one toon band set (`tShade` .42, `tLight` .86) for every family (foliage
adds wind only); FBX scale: every re-imported character's posed top within 15 % of its exported rest top (the chief's
horns and the shaman's feather fan stand 11 / 9 % above their R2 body heights, by design); palettes 256² / 16 px swatches
everywhere (no texel-density drift), terrain 341 px per tile, UI textures 512 (portraits, emblems) / 256 (item, skill,
quest icons) / 128 (glyphs). The vignettes place everything the way the runtime does (`BuildWalls` stakes, `CampLayout.h`
props, NPC slots and facings, MapGen camp stamp); the goblin camp's props have no runtime placement yet (spec §6.9 leaves
it to the level script), so its layout here is a proposal.

## 18. Dark-remake readability tooling (`coherence/darkgate.py`, `vfx/hue_audit.py`)

Work package 0C / A0 of `Docs/dark-remake/implementation-plan.md` (rules: `monsters.md` §1.2, README DR5). Both tools
run on whatever `Art/Export` and `unreal/Data` hold, so they gate every new dark-remake asset (1D: "darkgate on every
new monster, hue audit clean") and wrote the baseline on today's assets into `Art/Previews/ch1_dark/`.

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/coherence/darkgate.py   # ~25 s, exit 1 on a FAIL
#   [SK_… …] only these monsters (a partial roster updates its figures in the report and never overwrites the lineup)
#   --theme=<t> (default march once A10 exports it, plains until then) · --tile=darkest|grass|dirt|stone|camp
#   --ground-hex=#RRGGBB (flat stress ground) · --facing=se · --clip=Idle --frame=0 · --no-variants · --per-page=6
#   --out=<dir> --report=<json> --no-report
#   --zone=<zone> (roster = that zone's monsters) · --all (every shipped monster; default on a themed ground: the
#   monsters of the zones the theme presents, emerald_plains for the march — §31)
python3 unreal/Art/blender/vfx/hue_audit.py        # stdlib only, ~1 s, exit 1 on a FAIL
#   --recipes=<json> --out=<dir> --report=<json> --no-report --report-only (exit 0) --quiet
```

**Darkest-tile gate** (`darkgate.py`). Every shipped monster (manifest `SkeletalMesh` in `Monsters/`, each hunt
`variants` entry as its own figure with its scale, attachments and elite crown) is re-imported through `cohkit` exactly
as the lineups do, stood on a field of the darkest walkable terrain material (grass / dirt / stone / camp by the median
L* of an 8 × 8 composite; never water) at the W1 game camera, native 1080p, contact shadows under the feet, and
measured on its own coverage:

| Metric | Gate | Measured as |
|---|---|---|
| rim | dark ink ≥ 3 px on ≥ 90 % of the silhouette edge pixels | `cohkit.ink_gate_group` at `CHAR_GATE_PX` (+ the kit's dark / hull-colour checks; edges at a bright emissive accent left out as every ship does) |
| lit | ΔL* ≥ 15 | the lit side = solid toon pixels in the key light's light band (h ≥ `T_LIGHT`) or rim light (weight ≥ .5), from a light pass that evaluates the `kit.shading` terms per pixel; ΔL* = median of \|L* − L*ground\| over them (CIELAB, final frame vs the composited tile). The signed region median is reported as `signedDeltaL` |
| marker | ≥ 2 px | the family-marker swatches (manifest `"marker": {"swatches": [...]}` → `darkgate.MARKERS` → swatches named `*.eye` / `*.sclera` / `*.marker` on the mesh) rendered as a coverage mask with the rest of the figure occluding; the largest solid k × k block. No marker = FAIL (every hostile carries one) |

These definitions are pending confirmation (`Docs/DECISIONS.md` R21). The rim gate is ground-independent (the ink hull's
width); the report adds, ungated, how far the ink and the marker sit from the ground's L* (`rim.inkL` / `inkDeltaL` /
`fracInkClear`, `marker.medianL` / `deltaL` / `emissivePixels`) — on a near-black stress ground (`--ground-hex=#141210`,
L* 5.6) every figure keeps its rim verdict while its ink sits ≈ 2 ΔL* from the ground, the case the march review has to
judge. A pass within 1.5 ΔL*, 3 rim points or at exactly 2 marker px is listed in `atEdge` (log and report).

Outputs: `darkgate_<theme>_<tile>.png` (verdict per figure) and `…_map.png` (edge pixels green / red by ink, the lit
side washed yellow, the visible marker cyan or red), tiled `_2` when a native PNG would pass 200 KB, and
`darkgate_report.json` with one run per ground key (`plains:grass`; later `march:<tile>`, `hex:<RRGGBB>`, `@<facing>`).

Baseline (`plains:grass`, L* 58.7; the march set does not exist yet, so today's darkest plains tile stands in):

| Figure | Rim (≥ 3 px) | ΔL* (signed) | Marker | |
|---|---|---|---|---|
| Goblin | 99.0 % | 15.2 (+15.2) | 3 px eye | PASS, at the edge (lit) |
| Sneek the Thief | 96.8 % | 15.2 (+15.2) | 4 px | PASS, at the edge (lit) |
| Gruk Redcap | 100 % | 6.6 (−0.2) | 4 px | FAIL: the red cap's lit top (L* ≈ 45–55) sits on the grass's value |
| Goblin Chief | 100 % | 15.0 (+12.4) | 2 px | PASS, at the edge (lit, marker; dark leather and fur under bright iron) |
| Goblin Shaman | 99.1 % | 11.6 (+11.6) | 1 px | FAIL: hide / fur lit side close to the grass; the spirit-ember eyes are a 1 px accent (in game the `maskEmbers` FX glow adds a halo the gate does not count) |
| Green Slime | 100 % | 15.9 (+15.5) | 6 px | PASS, at the edge (lit) |

**This is the baseline, not an acceptance pass** (DECISIONS R21): the plan's 0C expectation that today's goblins pass
on grass does not hold — two figures fail and every pass sits within 1 ΔL* of the threshold, so 15.0 is no headroom.
Every figure keeps its rim; the lit-side misses are value collisions with the bright plains grass that the much darker
march ground removes (a flat #2B2724 stress ground, L* 16, passes the goblin and both hunt leaders at ΔL* 42–58; on
#141210 every figure but the shaman (1 px marker) passes). Not applied: the zone grade (`zone_moods`, P14) acts on the
whole frame and is reviewed in game (*mac-look*).

**Hue audit** (`hue_audit.py`, no bpy). Resolves every colour each recipe layer of `Export/VFX/vfx_recipes.json` can
draw (colour source × recipe palette × pre-coloured `T_FX_*` texels, through the runtime's palette and preset rules),
with the event colours `UAbyssVfxSystem` passes read from `unreal/Data` (elite `vfxColor`, ranged monsters'
`derived.projectileColor`, the monster-kill burst, class / skill impact colours, spirit colours, pet bolt colours), plus
the monster palettes' emissive / eye swatches and the monster manifest `fx` colours. Sides come from the recipe id
(docstring). Gating rules: **violet** — no hero colour at HSV sat > .5 within 30° of breach-violet #B02FD0 (hue 288°;
the mana shield #8A5CFF sits 31.1° away); **fill** — no hero layer draws a filling ground shape (a ground filled sprite
growing ≥ 1.5 × that still holds ≥ half its peak alpha at 95 % of the growth); **ember** — no enemy colour at hue
15–35°, sat > .6 (value ≥ .35). Advisory: friendly / neutral hits, later-chapter event colours (monster bolts outside
`emerald_plains`, pets met after Chapter 1), enemy colours within 5° of the ember band, enemy fills and their hazard
colours. Writes `hue_audit_report.json` and `hue_audit.png` (hue × saturation per side, guard bands shaded, gating
failures ringed).

Baseline (FAIL, to be cleared by A12 before 1D closes): **violet** in 10 hero recipes — the rogue's `shadow` palette
(#7030C0 / #B478FF / #24103E, hue 266–267°) in `skill.backstab`, `skill.death_mark`, `skill.shadow_step`,
`skill.vanish`, and `arcane_torrent`'s impact colour #B07CFF (264°) in `hit.impact*`; **fill** in 4 hero layers —
the `Puddle` decals of `proj.poison_arrow.hit`, `skill.bleed_strike`, `skill.poison_blade` and the meteor's growing
ground glow (`skill.meteor` layer 1); **ember** in `elite.fire_enhanced` (#FF4400 / #FF7A3A / #FF992B). Monster art
passes the ember rule, but the goblin chief's `eyeEmbers` FX #FF5A2A (14°) and eye swatch #FFB13A (36°) and the
wind-up tint #FF7A5C (11°) sit just outside the band (near-ember advisory). Unresolved (runtime-only, friendly or
neutral): loot quality colours, lore props, the `proj.default` fallback.

The ember value floor (.35) and the Chapter-1 scope of event colours (monster bolts gate for `emerald_plains`, pet bolts
for pets met in Chapter 1) are relaxations of DR5's wording, pending confirmation (`Docs/DECISIONS.md` R22). The
report's `relaxations` block lists both with their reasons and what they exempt today: the floor only the monsters'
pupil swatch #1A0E08; the scope `proj.monster_bolt*` in the later fire monsters' #FF6600 and `proj.pet_bolt*` in the
arcane #CC44CC. Each chapter's dark pass adds its zone and pets to the gating sets (`CH1_ZONES`, `CH1_PET_CHAPTERS`).

## 19. The march theme: palettes and themed ids (`march/`, `kit/theme.py`)

Work package A1 of `Docs/dark-remake/implementation-plan.md` §5: the colour script of 焚境 · The Burning March as
swatch registries, so every Wave-1 generator (A2-A11) draws from one place.

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/march/march_palettes.py   # register + atlases
#   --check (rules + capacity on a scratch copy, writes nothing) · --families=Monsters_March,NPC · --list (name → index)
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/march/march_review.py     # ~4 min
#   [--sheets] [--characters] [--props [camp,story,ashwheat,interactive,foliage]]
```

| Family | Atlas | Holds | Used by |
|---|---|---|---|
| `Monsters_March` | `T_AF_Palette_Monsters_March_BC/_P` (`MI_AF_Toon_Monsters_March`) | 127 swatches: `breach.*` (shared violet accents), `unburied.*` (the kit), `crossbow.* arbalest.* towershield.* standard.* pitwarden.*`, `living.* zealot.* flail.* deserter.* drum.* harke.* dreck.*`, `gnawer.* bonegorger.* clapper.* hungerbell.*`, `corpseherd.* crook.*`, `valdras.*`, `clot.* bloat.*`, `elite_crown.*` (W1 recolour, keys of `elite_crown.py`), `elite_sigil.*` | A2-A8 |
| `Props_March` | `…Props_March…` (`MI_AF_Toon_Props_March`) | 100: `camp.* bell.* banner.* namewall.*`, the refuges `hearth.* forge.* lantern.* crowmarket.*`, `brazier.coals`, `wagon.* bridge.* cottage.* pit.*`, the Breach Rim `dais.* ritual.* bannerring.* stake.*`, story `redbanner.* belfry.* altar.*`, lore / interactive `orderpost.* journal.* seal.* catechism.* lore.glyph echo.* strongbox.* handbell.glow barrowmoss.*`, `rock.* boulder.* outcrop.* ground.*` | A10, A11 (gather node) |
| `Foliage_March` | `…Foliage_March…` (parent `M_AF_Toon_Foliage`) | 28: `deadoak.* charred.* briar.* ashgrass.* poppy.* ashcap.* reed.* scum.*` | A10 |
| `NPC` (appended) | `T_AF_Palette_NPC_*` (indices 30, 176-200 new) | the eleven march looks `npc.march.<snake>.<slot>` (exactly `folk.regions_for` of `march_palettes.NPC_LOOKS`) + kit parts `npckit.march.<part>` | A9 |

Rules (the module docstring has the full list; `--check` gates them): low base values with a deeper shade
(shadowAmt .48-.55) and a light band that clears the darkest reference ground by ≥ 15 L* (the darkgate lit rule:
dark cloth takes the `dark` preset, lightAmt .36); one marker per hostile family, named `*.eye` / `*.marker`
(`unburied.eye`, `corpseherd.eye`, `valdras.marker` violet; `gnawer.eye` / `bonegorger.eye` bone-yellow eyeshine;
`zealot.marker`, `deserter.marker`, `harke.marker` crimson cloth; `clot.marker`, `bloat.marker` violet) — ship
scripts still list them in the manifest `"marker"`; no enemy ember (emissive or eye); ember only for refuges,
interaction and the Abyssfire in Props_March; violet glows only on the Breach Rim / Pit / enemy art.

**NPC capacity.** The NPC atlas held 175 of 256; re-dressing eleven looks with their own skins, hair and legs needs
~160 more, so the march looks change only the identity pieces and reuse the plains NPC values (same hex *and* tone
params) for everything else: 26 new swatches, 55 left for A9. Build the march looks with
`folk.Folk(look, asset, prefix=f"npc.march.{snake}")` — never re-register a plains `npc.<snake>.*` name with march
values (that moves the name and frees a swatch the shipped plains NPC still samples; `register` refuses it).

**Themed ids.** A themed asset lists `kit.theme.themed_ids([base], "march")` in its `gameIds` (`tree@march`,
`camp_tent@march`, `npc_quest_elder@march`, `elite_crown@march` …); `export.Manifest.set_asset` maps them, and the
runtime resolves `<id>@<zone theme>` before `<id>` (U1). `march_palettes.VARIANTS` lists every planned slot with its
asset name and package; brand-new remake ids (`monster_<id>`, `npc_knell_chaplain`, `npc_crow_trader`, the quest
items) stay plain. `kit.theme.audit(manifest)` flags a themed id without a base id, an unknown theme or a missing /
unlisted asset.

**Review** (`Art/Previews/march_palettes/`, nothing ships): the swatch sheets (`palette_*`: index, shade · base ·
light · hull, names, hex, E; the NPC sheet marks the appended swatches) and today's shipped meshes re-paletted with
the march swatches (colour only, `*` = a palette proxy on another mesh) on the reference march ground
(`MARCH_GROUND`, an A10 proposal: ash grass #4C4940 L* 31, mud #3A332D L* 22, stone #55524C, camp ash #47423C,
water #121418), at the W1 camera, native 1080p, shipped ink and bloom; `…_grade` an approximate P14 march grade (gain,
saturation, contrast only). `lineup_metrics.json`: per figure the ink gate on the dark ground, CIELAB C* p75 and an
approximate lit ΔL* (darkgate.py stays the gate for the shipped monsters). First pass: every figure's ink ≥ 3 px on
94-100 % of its edges, monster lit ΔL* +26 (clot) to +44, the hero (C* 36) out-chromas every NPC (C* 10-30); the
zealot's crimson robe (its family marker, C* 51) is the loudest enemy colour by design.

## 20. The Living — dark-remake A3 (`monsters/living*.py`, `oath_zealot.py`, `deserter.py`)

The march's living enemies (monsters.md §2.3): 赤誓狂信徒 Red-Oath Zealot, 逃兵 Deserter, 军鼓手 Deserter Drummer and
the hunts 拾银者·德雷克 / 断誓者·哈克 (the deserter ×1.25 + attachments + elite crown, manifest `variants`).

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/oath_zealot.py
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/deserter.py [deserter|drummer|all]
#   --quick (close-ups / turnaround / game crop → $AF_SCRATCH_ROOT; deserter.py --quick [deserter|drummer|dreck|harke])
#   --anims [Clip …] · --no-previews · --no-verify   (Monsters_March atlas + manifest writes are locked and merged)
```

| File | Holds |
|---|---|
| `monsters/living.py` | the kit: `LivingFolk` on the townsfolk body (`npcs/folk.py`, R2 heights, `SKEL_Human` + the folk cloth chains) turned into a monster — r1: the Unburied body (`LivingDims`: the footman's bone lengths, hip / shoulder widths and 1.76 m frame scaled to each height; head share 20-23 % at W1 like the footman's 22.6 %), narrower skirt flare (`hip_w` 5.3), a hard face (dark sunk eyes, no whites / pupils / catch-lights, scowl, frown, hollow cheeks), hood (brim, crest, peak, drape) + cowl + ragged capelet, mail coif, face scarf, ragged robe skirt (hem band, folds, mail hem), rope belt, quilting, torn-off insignia, crimson armband / belt rag (the marker), elbow sleeves over bare forearms with the oath-scar, full sleeves + bracers; `MarchBuilder` (on `goblins.LodHullBuilder`) maps every region key onto a registered `Monsters_March` swatch name **and** value (`LivingLook.palette`), so a build never appends or moves a swatch; `build_rig` (+ `fx_eye_l/r`) |
| `monsters/living_clips.py` | `LivingAnims` on `npc_clips.NpcAnims` (the folk cloth: skirt chains, coat thigh drivers, pack): READY per role, Idle, no-slide Walk (FootL/R), Attack01 (WindupEnd = 62 % of Contact), Hurt 300, Death 900 (knees buckle, fold forward, face down, weapon flat; the NPC floor post + `floor_items`: rigid attachments the fallen body rests on), Stun 600 |
| `monsters/living_ship.py` | `build_lod1` (ratio fitted to the 3 k budget), shared QA groups, `install_coat_qa` (the NPCs' `ToonGroundQA` + `CoatLegQA` for robes), `corpse_preview` (`game_corpse.png`: the last Death frame centred on the body, gated), `hunt_previews` (`hunt_leaders.png` / `hunt_close.png` with the march-recoloured crown), telegraph / trail / marker / remake manifest extras |
| `monsters/oath_zealot.py` | `SK_Mon_OathZealot` + `SM_MonWeapon_Flail` (`monster_oath_zealot`) |
| `monsters/deserter.py` | `SK_Mon_Deserter` + `SM_MonWeapon_Hatchet` (`monster_deserter`, `hunt_dreck`, `hunt_harke`; attachments `SM_Mon_Deserter_Att_TagChain` / `_LootSack` (Dreck), `_RedScarf` / `_Gorget` (Harke)); `SK_Mon_DeserterDrummer` + `SM_MonWeapon_DrumStick` on `weapon_r` **and** `weapon_l` (`monster_deserter_drummer`; `Cast01` 1000 ms drum loop, `FX_DrumBeat_1..4` at 125/375/625/875, `Release` 125) |

Colour: only A1 `Monsters_March` swatches (`living.*`, `zealot.*`, `flail.*`, `deserter.*`, `drum.*`, `harke.*`,
`dreck.*` + shared `unburied.*` leather / mail / iron); no ember and no glow anywhere (eyes `living.eye_dark`; the
zealot's `zealot.scar` e .25 is crimson, its `[W2]` frenzy flare is a material parameter). Markers (manifest
`"marker"`): zealot `zealot.marker` (the crimson robe), deserters `deserter.marker` (armband + belt rag; the drummer's
rag is tied round his helm, the one place the W1 camera always sees), Harke `harke.marker` (the red scarf).
Wind-up tint (`fx.telegraph`) #FFE8DC → #D11F2E elite, weapon trail #E8E0CC.

Budgets (monsters.md §2.2; manifest `budget`): zealot 6.79 k / LOD1 3.00 k, deserter 6.91 k / 2.98 k, drummer
6.81 k / 2.98 k toon; hull ≤ 0.75 × toon (0.72, 0.73, 0.53): coarser closed proxies for the hood, skull (10 × 12) and
skirt, LOD1 hulls decimated 0.8 × further. Ink: the monster class + the 3.5 px request (gate 3 px): min rim 91.9 %
(zealot, Attack01 Contact front), 96.6 % (deserter, corpse ne), 93.9 % (drummer, Cast01 F8 front).

QA (`mship` + `install_coat_qa`): every pair 0 on every clip except the documented Death allowances — the robe
pressed into the knees on the floor (`coatLegsMm` ≤ 25 mm) and the drummer lying on his drum (`drumBody`); the hunt
attachments rest on the floor (`floor_items`, ≥ 0.3 cm). darkgate (R21 pending, reported not tuned): all five
figures PASS on the march mud stress ground (`--ground-hex=#3A332D`, L* 22: lit ΔL* +31 to +40, marker 3-26 px,
rim 95-99 %); on today's plains grass (L* 59, the default until A10 exports the march terrain) all five FAIL lit
(ΔL* 7-12): the march palettes are tuned for the march's dark ground. Ungated R21 diagnostics on mud: ink ΔL* ≈ 17
vs the ground, crimson markers only ΔL* 5-12 (they separate by hue, not value).

**Art review r1** (all three re-shipped from the r1 sources; QA, ink gate, re-import + LOD1 re-import PASS):

| # | Change |
|---|---|
| 1 | Body on the Unburied proportions (`living.LivingDims`, `UNBURIED_BODY`): head share at W1 deserter 31.3 → 22.7 %, drummer 29.3 → 22.5 %, zealot 29.7 → 19.7 % (footman 22.6, shieldbearer 23.1); longer thighs / shins, narrower hip and skirt flare, hood volume cut (`HOOD_THICK`). `monsters/lineup_living.py` → `Previews/mon_family_march/living_unburied_lineup*.png` + `.json` (head shares on the labels; survives a `lineup_march.py` run) and re-shoots `lineup_march`'s `living` page |
| 2 | Drummer: Cast01 strokes from the shoulder (sticks to head height, elbows out, knee / torso bob, the back view pumps the elbows), Idle rests with the sticks lowered, Attack01 an overhand forward blow that lands clear of the drum. `Previews/mon_deserter_drummer/cast_vs_idle.png`: Cast01 vs Idle F0 ≥ 1.83 × Idle's own motion in all 8 facings (min 4.8-5.7 k px) |
| 3 | Socket `fx_drum` on `pelvis` at the drum head; `fx.drumBeat.socket` = `fx_drum` |
| 4 | Hunt previews (`hunt_leaders.png`, `hunt_close.png`) under `SM_FX_EliteCrown_March`; `monsters/living_darkgate.py` runs darkgate with the march crown (`cohkit.import_character` hard-codes the plains `SM_FX_EliteCrown` for every hunt variant: flagged to A0 / A8, not patched here) |
| 5 | Dreck: diagonal bandolier sampled on the coat's outer surface (`_outer_surface`), larger bright tags (`dreck.tags`) across the chest; loot sack on the left hip (`coat_th_l_01`) seen from se / front |
| 6 | Harke: gorget enlarged and lowered, the crescent reads below the scarf |
| 7 | Deserter hood: peak + drape behind the crown (`hood_tail`), cowl folds, a lighter rolled edge round the face (`hood_edge`), centre seam |
| 8 | Deserter Idle: hatchet low at the right side, blade down, in profile (`READY_AXE`) |
| 9 | Zealot flail: the chain + ball are skinned to `flail_01` (child of `weapon_r`, socket `fx_flail` = the ball) and aimed every frame by a damped Verlet pendulum on the haft tip (world space through the Death spin, body / robe / floor proxies); with the haft raised the chain keeps ≥ 55° off the haft line on the gravity side (`MIN_BEND`), so the ball trails through the windup and whips at Contact. `SM_MonWeapon_Flail` is the haft only |
| 10 | Zealot oath-scar: a brand (teardrop + bar) inside a darker burned ring (`scar_edge`), emissive 0.25 |
| 11 | Death: manifest `fx.blobShadow` (`deathFollowBone` `pelvis` over `deathFollowMs` 380-760, else fade to `deathFadeAlpha` 0.35); drummer rolls onto his side so the drum shows (`DEATH_ROLL` −85°); zealot robe longer in the final pose (hem lowered) |
| 12 | Drummer `items.png` without the `_L` stick cells |

**Art review r1, re-check** (the 13 points verified on the r1 previews; what was still short, fixed; all three
re-shipped from today's sources, zealot unchanged — QA 0 on every pair, ink gate, re-import + LOD1 re-import PASS):

| # | Change |
|---|---|
| 1 | The deserter still read heavy beside the footman at W1 (shoulder-row width 0.47-0.54 of the figure vs 0.36): `head_k` .56 → .53, coat cut higher and narrower (`hem_k` .34 → .40, hem 5.1 × 4.7 → 4.8 × 4.5 units: more thigh, no bell), pack .95 → .80, the READY left elbow in at the side (it stood akimbo); drummer `head_k` .57 → .54, coat .38 / 5.1 × 4.7. Head share at W1 (`living_unburied_lineup.json`): deserter 22.7 → 22.2 %, drummer 22.5 → 21.5 % (brief 20-22 %; footman 22.6) |
| 2 | Drummer Attack01 Contact re-keyed: at the W1 camera (se: forward runs down the screen) the r1 contact still crossed the drum's front, so it read as a beat; the arm now reaches forward-right above the shoulder (out 0.28 m, no torso twist) with the stick sloping down at the target's head, beside the drum in se and full-reach in sw / front / side, and the follow-through carries on down the right side. Cast01 vs Idle unchanged: ×1.79-2.54 Idle's own motion in all 8 facings |
| 4 | Hunt previews and the Living darkgate re-rendered after A8's crown tarnish (`elite_sigil.py` 18:53): `hunt_leaders.png` / `hunt_close.png` build `SM_FX_EliteCrown_March` from its generator (590 toon tris = manifest), darkgate imports the shipped FBX; the stale r0 `darkgate_plains_grass_OathZealot_Deserter_DeserterDrummer*` sheets (plains crown, r0 bodies) re-rendered too (plains grass is not the Living's ground: 0/5 lit, as before) |
| 5 | Dreck: tags 3.6 × 4.6 → 4.3 × 5.5 cm, nine instead of eleven (`TAG_SIZE`, `TAG_COUNT`); the hip sack 9 cm off the coat (the shorter coat's panel swung into it in Walk) |

darkgate (`living_darkgate.py`, march crown on Dreck / Harke): 5/5 PASS on `#3A332D` (lit ΔL* +30.6 to +47.9),
`march:dirt` (+32.9 to +50.2) and `march:grass` (+20.1 to +37.4); rim 96.3-99.8 %, marker 3-23 px.

## 21. The Gravegnawers — dark-remake A4 (`monsters/gnawers.py`, `gravegnawer.py`, `bonegorger.py`)

The march's carrion beasts (monsters.md §2.4): 啃冢者 Gravegnawer (swarmer, corpse-eater), 饕骸者 Bonegorger (bruiser)
and the hunt 饿钟 Hungerbell (the bonegorger ×1.25 + attachments + the elite marker, manifest `variants`). Re-skins on
`SKEL_Goblin` (same bone names and clip machinery as the shipped goblins); `goblins.py` / `goblin_chief.py` are not
edited — the kit subclasses them.

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/gravegnawer.py
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/bonegorger.py
#   --quick (close-ups / turnaround / game crop → $AF_SCRATCH_ROOT/a4/…; bonegorger.py --quick hunt: with
#   Hungerbell's attachments) · --anims [Clip …] [--facing=se] · --no-previews · --no-verify
#   (one ship at a time: each rewrites the Monsters_March atlas)
```

| File | Holds |
|---|---|
| `monsters/gnawers.py` | the kit: `GnawerKit` (on `goblins.GoblinKit`) — the head modelled in its own frame (`Shape.head_tilt`: the skull stays level, muzzle forward, on a neck thrust off a near-level back), a small low cranium, a long muzzle with the lower jaw a separate solid on `jaw` (hinged under the eye, the whole jaw drops), a near-black maw, needle teeth overlapping the lips (`Shape.teeth_dn_k`: per-tooth lengths, the bonegorger's underbite tusks), slanted **bone-yellow eyeshine** without pupils under a low brow, torn ear stubs, an emaciated torso (pinched waist, painted ribs, spine knobs, shoulder blades) or a bloated one (`Shape.belly_k`), arms + 15 %, grave grime on the forearms and shins, clawed fists and feet, a tabard loin scrap and an arm rag (trophies), the rope belt; `MarchBuilder` (registered `Monsters_March` names **and** values: a build never appends or moves a swatch); `march_stage` (every preview on the march ash grass #4C4940); `GnawerAnims` (on `mclips.GoblinAnims`): an authored contact (`WindupEnd` = 62 %), the biting jaw, hand lifts for clips that lower the hands from a knuckle-drag READY, the carcass Death (the chest pitches into the ground, then it rolls onto its side), manifest extras |
| `monsters/gravegnawer.py` | `SK_Mon_Gravegnawer` + `SM_MonWeapon_LashedSpear` (a broken crusade spearhead lashed to a stick with a tabard rag; `monster_gravegnawer`) |
| `monsters/bonegorger.py` | `SK_Mon_Bonegorger` + `SM_MonWeapon_BellClapper` (a church-bell clapper as a club: hanging eye, grease-black shank with a rag-and-rope grip, bronze ball with verdigris; `monster_bonegorger`, `hunt_hungerbell`); attachments `SM_Mon_Bonegorger_Att_BellLip` (a jagged lip of Ashwheat's bell wedged on the ball, `weapon_r` space) / `_TagNecklace` (bronze name-tags threaded flat on a cord round the neck root, `neck_01`); mange spikes, belly folds. No apron, no cleaver, no hooks, no larder |

Clips. Gravegnawer (speed 66 → 1.98 m/s): Idle 900 (**knuckle-drag**: the back near level, the long free arm on its
knuckles, the head sweeping and sniffing), Walk 480 (low and fast, no slide), Attack01 600 (a lunging stab and bite:
the jaw gapes on the wind-up and snaps shut on Contact 300, WindupEnd 186), Hurt 220, Death 600, Stun 600.
Bonegorger (speed 42 → 1.26 m/s): Idle 1400 (wet breathing, the gut heaving, the jaw working), Walk 860, Attack01 1200
(the clapper hauled up outside the right shoulder and clubbed down-forward; WindupEnd 434, Contact 700), **Attack02
1400 = the [W2] 钟锤 Clapper Slam** (reared up, the clapper high overhead, the whole body slammed down; the ball strikes
the ground ≈ 0.95 m ahead on Contact 800, WindupEnd 496, `FX_BellRing` 800), Hurt 260, Death 900 (onto the belly,
rolled onto its side), Stun 700. Hungerbell plays the bonegorger's clips at ×1.25.

Colour: only A1 `Monsters_March` swatches (`gnawer.*`, `bonegorger.*`, `clapper.*`, `hungerbell.tags`; the palette
holds 127 swatches before and after). Marker (manifest `"marker"`): `gnawer.eye` / `bonegorger.eye` bone-yellow
#EDE6B0, emissive .35 — the only glow; no ember anywhere (the clapper bronze #8A6A3A is hue 36°, sat .58). Wind-up
tint (`fx.telegraph`) #FFE8DC → #D11F2E elite, weapon trail #E8E0CC.

Budgets (monsters.md §5; manifest `budget`): gravegnawer 5.92 k / LOD1 3.07 k toon (≤ 6.1 k / 3.2 k), bonegorger
5.74 k / 3.18 k (≤ 9 k / 3.3 k); hull ≤ 0.75 × toon (0.67 / 0.63, 0.72 / 0.70); LOD1 keeps the claws (a gnawer
identity cue; the goblins skip theirs). Ink: the monster class + the 3.5 px request (gate 3 px): min rim 94.5 %
(gravegnawer) and 94.7 % (bonegorger). **Every protruding detail inks on its own hull** — claws, toe claws, the belt
knot and ends, rag tails, the spear's lashing (one closed proxy round the six turns), mange spikes, each name-tag:
`covered_by` a neighbour, the neighbour's padded proxy balloons round them (the fist's grew 7 cm round the claws, the
spear rag's 13 cm into a dark wedge under the blade).

QA (`mship`): every pair 0 on every clip, no allowances — gravegnawer `loinLeg`, `spearBody`, ground; bonegorger
`clapperBody`, `armHead` (the long arms vs head and jaw), `lipBody`, `tagsBody` (Hungerbell's attachments against
the body), ground (the bell lip is in the Death ground clamp: the fall rests the lowest of body, clapper and lip on
the floor). What it took: the deeper knuckle-drag crouch needed a narrower, shorter-standing loin scrap and the hands
lifted in Walk / Stun; the dying gravegnawer's hind legs half stretched (drawn up, they trapped the scrap against the
belly); the clapper swung up and over **outside** the right shoulder and a short butt (crossing the body it went
through the torso and skull); the tags threaded flat on the cord at the throat (dangling tags, and tags on the nape,
were swept by the arms; the skull hangs over the upper neck).

darkgate (R21 pending, reported not tuned; `Previews/ch1_dark/darkgate_hex_*_Gravegnawer_Bonegorger*`):

| Ground | Gravegnawer | Bonegorger | Hungerbell ×1.25 |
|---|---|---|---|
| march mud stress `--ground-hex=#3A332D` (L* 22) | PASS: rim 97.1 %, ΔL* +38.5, marker 3 px | PASS: 99.8 %, +33.9, 3 px | PASS: 98.4 %, +33.9, 3 px |
| march ash grass `--ground-hex=#4C4940` (L* 31) | PASS: +29.3 | PASS: +24.7 | PASS: +24.7 |
| plains grass (L* 59, the default until A10 exports the march terrain) | FAIL lit: ΔL* 2.0 | FAIL lit: 3.4 | FAIL lit: 3.4 |

The ash-grey hide (#6F6A60 / #5F5A50 lit to L* 55-60) sits at the plains grass value; on the march grounds it clears
by 25-38. The eyeshine was enlarged after the first gate run measured exactly 2 px (at the edge): now 3 px blocks,
21-44 emissive pixels, ΔL* 63-74 against the ground. Ungated R21 diagnostics on mud: ink ΔL* ≈ 16 (the ink clears the
ground on 72-83 % of the edge pixels; the lit side carries the read there).

## 22. The Unburied — dark-remake A2 (`monsters/unburied.py`, `uclips.py`, `risen_*.py`, `dead_standard_bearer.py`)

The crusade's dead (monsters.md §2.2): 复起步卒 Risen Footman (shambler, the default Raised body), 复起弩手 Risen
Arbalester (ranged), 复起盾卒 Risen Shieldbearer (shield wall) and 亡旗手 Dead Standard-Bearer (summoner / aura,
champion). A new monster kit on `SKEL_Human`; no edits to `goblins.py` / `slime.py` / `goblin_shaman.py` (the kit
builds on `goblins.LodHullBuilder` and `mclips.GoblinAnims`). monsters.md assigns no hunt to this family (the hunts
are Living / Gravegnawer); `sub_pitwarden` is W3 (A13).

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/risen_footman.py
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/risen_arbalest.py
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/risen_shieldbearer.py
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/dead_standard_bearer.py
#   --quick [dir] (READY close-ups se / front / back, turnaround, game crop → $AF_SCRATCH_ROOT/mq/<member>)
#   risen_footman.py --anims [Clip …] [--facing=se] [--shovel] · --no-previews · --no-verify   (~4 min a ship)
```

| File | Holds |
|---|---|
| `monsters/unburied.py` | the kit: `UnburiedKit` (`HumanoidSpec` 1.76 m to the bare crown, R2; realistic proportions, a gaunt head ~15 % over life size for the camera) — burnt-clay ash skin finely cracked, dark sockets with **violet pinpoint eyes** (exact, never decimated), slack mouth, no bone / wounds; quilted gambeson torso, torn crimson tabard with tarnished trim, the scorched open palm on the chest, charred hem, two skirt panels on `tabard_f/b_01..02` chains (envelope-solved against the skinned legs), mail shreds on the upper arms, a mail skirt, belt, worn boots, broken arrow shafts; `ULook` member gear (coif, bolt quiver, pavise, hip sword, `eye_scale`, `lod1_skip`); head looks as `head`-bone attachments `SM_Mon_Unburied_Att_KettleHelm` (default) / `_Hood` (`hood_shell`: an arched face window, its ink hull baked from a **closed** proxy sunk under the face) and the bare scalp; `SM_MonWeapon_Sword` (the hero sword T1 shapes, notched, dull steel), `SM_MonWeapon_Shovel` (the burial detail); `MBuilder` (regions = `Monsters_March` names, registered with the exact A1 values; `use_appended` for the family's own appended swatches, `APPENDED`); the crimson-cloth toon slot (`cloth_slot`, `MI_AF_Toon_Unburied_Cloth`); attachment LOD1s (`att_builder`); manifest `looks`, eye glow cards (`fx.eyeGlow`, #D08CFF), `budget_record`, `looks_preview`, `ship_member` (the members' ship: attachment LOD1 export, per-look budget totals, shared attachments' `heldBy` / `previews` merged, truecolour close-up sheets) |
| `monsters/uclips.py` | `UnburiedAnims` (on `GoblinAnims`, human proportions): the hunched READY (lean 12°, broken-neck tilt 15°, a dropped shoulder; the bind pose stays upright), Idle 1600, Walk (a dragging right foot, the toe scraping, a limp; FootL / FootR), Attack01 (WindupEnd = 0.62 × Contact), Hurt 320, Death 900 (knees buckle, kneel, fold face-down: the ash-husk pose U3 crumbles), **Rise** 1200 (Death reversed and retimed, `RiseUp` 840), Stun 700, one-shot `Idle_Order` mimes |
| `monsters/risen_footman.py` | `SK_Mon_RisenFootman` (`monster_risen_footman`): overhead chop (Contact 600), `Idle_Order` (the sentry shoulders arms and scans a line that is gone), `Idle_Order_Dig` (the burial detail digs, `FX_Dig`; the shovel look); weapon looks sword ×2 / shovel ×1 |
| `monsters/risen_arbalest.py` | `SK_Mon_RisenArbalest` (`monster_risen_arbalest`) + `SM_MonWeapon_Crossbow` (steel prod, stirrup, spanned string; empty: the bolt is the runtime projectile from the `tip` socket at Contact 450); quiver at the hip, pavise on the back; the crossbow rides both fists in every upright clip; Death folds and rolls onto the flank (`DEATH_ROLL`: the pavise lands on its edge); `Idle_Order` cranks the windlass (`FX_Crank`) |
| `monsters/risen_shieldbearer.py` | `SK_Mon_RisenShieldbearer` (`monster_risen_shieldbearer`) + `SM_MonWeapon_TowerShield` (`weapon_l`; 56 × 107 cm, bowed, crimson field, the scorched palm, ash streaks, iron rim and band; sockets `face` (the W2 block sparks) / `rim_top`) + `SM_MonWeapon_Mace` (the hero mace generator in dull iron); mail coif; shield bash Contact 500; `Idle_Order` braces and raps the rim twice (`FX_ShieldRap`); dying, the shield is planted, then topples face-up beside him |
| `monsters/dead_standard_bearer.py` | `SK_Mon_DeadStandardBearer` (`monster_dead_standard_bearer`) + `SM_MonWeapon_StandardPole` (3.0 m; crossbar 1.82 m above the fist, a tarnished open-hand finial): the banner is **cloth in the skeletal mesh** (`banner_01..03` under `weapon_r`, 46 bones; three vertical folds, the palm painted on both faces, a tattered hem), turned about the crossbar each frame by gravity plus a looping flutter, never behind the pole's plane (`BANNER_MIN_OPEN`); borne high so the banner clears the face at the W1 camera; Attack01 topples the standard onto the target (Contact 650, both fists), **Cast01** = Muster (`Plant` 450, `Release` 900), `Idle_Order` dips it in salute (`FX_BannerSnap`), Death kneels and folds at the foot of the planted standard, the hand still on it |

Colour: the A1 `Monsters_March` swatches (`unburied.*`, `crossbow.*`, `arbalest.*`, `towershield.*`, `standard.*`)
plus four **appended** in art review r2 (`unburied.APPENDED`, registry 134 → 138, append-only): `standard.banner`
(#6E1A1C, light .25), `arbalest.pavise_field` (#5A2422, .22), `towershield.face` (#7A2422, .22) and `unburied.cowl`
(#3A3633, .22: the hood look), plus `unburied.tabard_cloth` (#6B1F1F, .22; art review r3, registry index 152:
the tabard's chest and skirt panels — region `tabard` is remapped onto it in `UnburiedKit.regions`; the shared
`unburied.tabard` .30 stays registered for the drum hoop and the Living). The crimson cloth — the tabard and its paint, the banner, the pavise, the tower
shield's board — is on toon slot 2 `MI_AF_Toon_Unburied_Cloth` (parent `M_AF_Toon`, `RimOverride` 1,
`RimOverrideColor` #8E3A34 sRGB, `RimOverrideAlpha` .38: the `MI_AF_Toon_Valdras_Cloth` parameters, §25; the shield
lists the slot in its own `materialSlots`). Marker (manifest `"marker"`): `unburied.eye` #D08CFF emissive, the only
glow; no ember anywhere. Wind-up tint (`fx.telegraph`) #FFE8DC → #D11F2E elite; trails #C8C4BC. Manifest `looks` lists the
per-instance head looks (kettle helm / bare scalp / hood) and weapon looks with weights; until the UE layer picks per
instance (U-lane), every instance wears `attachments`.

Budgets (monsters.md §2.2; manifest `budget`, toon = every toon slot): footman 6.57 k / LOD1 2.94 k toon, arbalester
6.96 k / 2.97 k, shieldbearer 6.95 k / 2.95 k (≤ 7 k / 3 k), standard-bearer 7.32 k / 2.98 k (≤ 7.5 k / 3 k; its LOD1
drops the banner trim and scabbard chape); hull 0.62-0.70 × toon (LOD1 0.51-0.55). The head looks and held items are
separate static meshes, each with a LOD1 (`<asset>_LOD1.fbx` + manifest `lods`: kettle helm 1352 → 472, hood 1074 →
492, sword 1350 → 368, shovel 296 → 216, mace 1590 → 786, tower shield 1160 → 526; crossbow 468 and pole 528 have
none); `budget.looks` records every per-instance look's total (body + head + held items) at LOD0 / LOD1 — footman
kettle helm + sword 9.27 k / 3.78 k, bare scalp + shovel 6.87 k / 3.15 k; arbalester 7.43-8.78 k / 3.44-3.93 k;
shieldbearer (mace + tower shield) 9.70-11.05 k / 4.26-4.75 k; standard-bearer 7.85-9.20 k / 3.51-4.00 k. The UE
importer reads `lods` on skeletal meshes only (as the environment kit's static LOD1s): until it imports static LODs,
mobile-low draws the LOD0 attachments. Ink: the monster class + the 3.5 px request (gate 3 px): min rim 95.9 %
(footman), 97.2 % (arbalester), 96.7 % (shieldbearer), 95.6 % (standard-bearer, Attack01 contact sw). QA (`mship`): every pair 0 on every clip — weapon / shield / pole vs body, helm and hood
vs body, mace vs shield, helm vs shield, banner vs body / pole / helm, tabard panels vs legs, ground; the only
exemptions are the footman's look-bound idles (`qa_clip_limits`: the sword pair in `Idle_Order_Dig`, which plays on
the shovel look only, and the shovel pair in `Idle_Order`).

Kit changes made for this family (all opt-in or behaviour-preserving): `mship.Item.preview_up` `"weapon"` /
`"weapon_flat"` (a variant held item laid out like a weapon in `items.png`) and `"z"` (a shield upright);
`MonsterShip.native_sheets` (the `game_*` sheets written truecolour at scale 1 with `pngio.write_png_native`, tiled
`_2 … _n`, instead of palette-quantised and downscaled — every Unburied ship sets it); `pngio.write_png` writes
through a temp file and `os.replace`, so parallel ships no longer race on the shared family atlas (a reader had loaded
a half-written `T_AF_Palette_Monsters_March_BC.png` → a magenta cell).

darkgate (R21 pending, reported not tuned; `Previews/ch1_dark/darkgate_*_RisenFootman_RisenArbalest_*`; re-run
after art review r3):

| Ground | Footman | Arbalester | Shieldbearer | Standard-bearer |
|---|---|---|---|---|
| march terrain, darkest tile (`march:dirt`, L* 19.5; the default theme) | PASS: rim 98.2 %, ΔL* +43.8, marker 3 px | PASS: 99.2 %, +44.1, 3 px | PASS: 97.3 %, +43.9, 3 px | PASS: 97.9 %, +41.3, 3 px |
| march mud stress `--ground-hex=#3A332D` (L* 22) | PASS: +41.5 | PASS: +41.8 | PASS: +41.6 | PASS: +39.0 |
| march ash grass `--ground-hex=#4C4940` (L* 31) | PASS: +32.3 | PASS: +32.6 | PASS: +32.4 | PASS: +29.8 |
| `--tile=grass` (`march:grass`, L* 32.3) | PASS: +31.0 | PASS: +31.3 | PASS: +31.1 | PASS: +28.5 |
| `--tile=stone` (`march:stone`, L* 32.4) | PASS: +30.9 | PASS: +31.2 | PASS: +31.0 | PASS: +28.4 |
| near-black `--ground-hex=#141210` (L* 6) | PASS: +57.7 | PASS: +58.0 | PASS: +57.8 | PASS: +55.2 |
| plains grass (L* 59; before r2, not re-run) | FAIL lit: ΔL* 5.7 | FAIL lit: 6.2 | FAIL lit: 6.0 | FAIL lit: 5.7 |

The gate's lit band is the brightest one (the ash skin and the helm). The crimson cloth's own lit bands (tabard
#8C4E47 L* 40, banner #92504A L* 42, tower shield #97524A L* 44) clear the darkest march tile by +21-25 (the A1
lit-band rule, 15) but sit only +8-12 over the L* 32 grass / stone tiles: "≤ L* 45 crimson" and "+15 over every march
tile" cannot both hold for a dark crimson, so the figure's read there comes from the skin and helm (+28-31).

The ash skin and kettle helms light to L* 61-64: on the march grounds they clear by 30-58, on the bright plains grass
they sit at its value (as A3 / A4). The shieldbearer's eyes measured exactly 2 px at first (the coif's face window
hides part of them): its pinpoints are 1.2 × (`eye_scale`), now 3 px / 20 emissive px. Ungated R21 diagnostics on
mud: ink ΔL* ≈ 16-17 (clear of the ground on 69-90 % of the edge pixels), marker ΔL* 58-60.

Art review r2 (what changed and why):

* **Hood ink hull.** The hood's hull was padded from its open shell: `pad_proxy` pushed the cut's edge faces out to
  enclose a frayed rim that crossed the face window — 47 hull faces with 10-39 cm edges, the outline slot 34 × 42 ×
  40 cm round an 18.5 × 26 × 27 cm toon shell, black wedges and ink over the face in every hood cell. `hood_shell`
  now builds the toon with an arched face window (cells cut, the boundary snapped onto the window curve, normals
  forced outward before the 6 mm solidify) and a **closed** proxy (`hull=…, hull_pad=True`) that follows the hood and
  dips 4 mm under the skin inside the window, the frayed edge riding the lip just outside the cut. Hull bounds 1.5-1.8
  cm past the toon per side; the longest hull edges (12 cm) are the proxy's caps inside the head. The hood takes the
  appended `unburied.cowl` (the legs' `unburied.hood` lit its crown L* 55, paler than the face it framed); its nape
  edge rides 1 cm higher (−0.040: the hoodBody QA caught the back edge in the torso when the head snaps back in Hurt).
* **Crimson wash.** The cream rim (#FFECC8 .55) on the cloth preset's .30 light band turned flat crimson panels
  peach at grazing angles (banner #BE8E7B L* 63 on 39 % of it SW). The cloth slot above plus the narrower bands:
  banner SW / NE median L* 28, p90 42 (Idle F0 SW, Walk F9 / F40 SW); footman tabard SW p90 46; the shieldbearer's
  shield NW Death F54 median 40 (#83413B); the arbalester's pavise in Death SE median 39.5 (#7B5048, was #CBAE95
  L* 73). Whole parts only on slot 2: a part split between slots shares vertices across the seam and the kit's
  preview hull (Set Position on material index == 1, interpolated to points) bulged the seam over the ink.
* **Pavise.** Slung canted (`PAVISE_YAW` 18° about the spine round its right edge), so the folded corpse carries it
  tilted, its iron rim and edge up; with the darker field it no longer reads as the brightest shape on the husk.
* **Eye sheets.** `material_ab` / `lod1` / `looks` / `closeup_*` / `poses_close_*` / `turnaround` were written by the
  preview budget writer's 256-colour fallback: the few violet eye pixels snapped to the label cream #F0ECDC (the
  "cream discs" and the standard-bearer's missing cards). `ship_member` writes them truecolour (`write_png_native`,
  tiled `_2 …`); the eyes read violet with and without the cards (`fx.eyeGlow` off: the bead stays #FFADFF in the
  preview formula, not white).
* **Shared attachments.** `heldBy` and `previews` of the kettle helm and hood list all four members (union with the
  manifest instead of `mship`'s `[holder]`); the standard-bearer ships last so the shared head-look FBX is his 1.78 m
  fit (1 % loose on the 1.76 m members).
* **Sword** trimmed to 1350 toon (12 blade rows + 3 per notch, 8-sided grip / wrap, 10-sided pommel; the notches
  still bite the silhouette).
* **Standard Attack01.** The topple stops 26° above level with the fists drawn in: at Contact the crossbar is 1.86 m
  out at 1.91 m (the crown of a hero at the 1.6-tile range; it was 2.18 m out at 1.66 m, the shaft through the target),
  the banner 1.66-1.92 m out; manifest `fx.contactReach` records it for the core / VFX lanes.

Art review r3 (the r1 review re-checked on the r2 ship; what was still open):

* **Tabard lit band.** The rim override could not reach it: `unburied.tabard` (#6B1F1F, cloth .30) lights to
  #975F56 itself (L* 46, a* 22, a dusty salmon), so the skirt panels turned to the key light still read pale — 20 %
  (Idle F0 SW) to 55 % (Attack01 F36 SW) of the footman's crimson above L* 45, 13 % on the standard-bearer's Walk F9
  SW. The appended `unburied.tabard_cloth` (.22) lights to #8C4E47 (L* 40, a* 25, brick beside the banner's #92504A):
  over every `game_*` cell of the four members the share above L* 45 is now ≤ 1.5 % (p90 ≤ 41, peach ≤ 0.7 %);
  banner SW / NE p90 40-41.
* **Pavise on the husk.** The 18° cant left the field 21° off flat at the Death end, in the lit band (N·L .96, N·V
  .80 in se) in every facing, a dusty-rose slab (#7E524A). `ArbalestAnims.death_keys` rolls the fold onto the flank
  (`DEATH_ROLL` twist +50°, side −10°; Rise starts from it): the field ends 65° off vertical, edge-on in sw / nw /
  back / side_l, a tilted base-band crimson board in se / ne / front; over the eight facings its mean camera-facing
  area 0.71 → 0.38 and the lit-and-visible share 0.36 → 0.05 (probed on `spine_03` with `A.mats`, then baked: QA 0,
  Death F54 se ink 89 → 96-97 %).
* Re-checked and unchanged since r2: hood hull bounds 1.5-1.7 cm past the toon per side (LOD0 24.2 × 28.1 × 30.2 cm
  round 21.1 × 24.9 × 26.8 cm; no hull edge over 12.6 cm), violet eyes on every truecolour sheet, `heldBy` / `previews`
  union, the attachment LOD1s and `budget.looks`, and the Attack01 crossbar at Contact 1.86 m out at 1.90 m (re-measured
  on the baked clip).

## 23. Breach-blood — dark-remake A5 (`monsters/breach.py`)

The breach's spawn (monsters.md §2.1): 渊血凝块 Breachclot (fodder) and 烬胀囊 Cinderbloat (exploder). Re-themes on
`SKEL_Slime` (the 16 slime bone names and hierarchy, so both import onto the shipped skeleton); `slime.py` is only read
(profile, helpers) — never edited. No hunt in this family. Breach-blood leaves a tar puddle, not a corpse (fx `puddle`).

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/breach.py [clot|bloat|all]
#   --quick [Clip frame] (close-ups / turnaround / game crop → $AF_SCRATCH_ROOT/a5/…) · --game (every clip baked: the
#   game_poses / game_facings / poses_close sheets + containment QA) · --anims [Clip …] [--facing=se] · --qa
#   · --no-previews · --no-verify   (~1-1.5 min a ship; one ship at a time: each rewrites the Monsters_March atlas)
```

| Asset | Look |
|---|---|
| `SK_Mon_Breachclot` (`monster_breachclot`; 0.43 × 0.88 m, the slime's dome lumped a little) | a **translucent tar shell pushed almost to black** (`clot.pitch` #160609, a crimson-black; slot 2 `MI_AF_Toon_Slime_Breachclot` on `M_AF_Toon_Slime`: `BodyOpacity` .62, R12 blend, `RimOverride` 1 / `RimOverrideColor` #B02FD0 / `RimOverrideAlpha` 1.0 — a crisp breach-violet fresnel crescent on the key-lit side); inside, a dented kettle helm **lying on its side** (28 cm across the brim; crown axis tipped 59° toward the back, square to both front 3/4 views of the W1 camera, so se, sw and front see its profile: crown dome on the brim line, a raised **medial comb** over the crown (`HELM_COMB`, one lathe column: the crest of the profile, and in the back views — which look down the crown — a lit / shade split across the disc), a flattened dent beside it on its up-facing flank, a torn notch in the brim), two crossbow bolt-heads (broad 3.8 cm wedges, ≈ 4 px) and **four violet sparks** (`clot.marker` #D08CFF, emissive: the family marker) drifting round them — ≥ 17 cm (screen) from the helm's crown in se / sw / front; an oval + dot of wet gloss on the back shoulder (`GLINTS`, u .66: upper-left on the key-lit rim in se, off the helm in all eight facings); an unshaded tar backing bowl (`clot.pitchdepth`) the blend resolves against. No eyes. |
| `SK_Mon_Cinderbloat` (`monster_cinderbloat`; the clot × 1.35, × 1.12 taller: swollen, 0.65 m — manifest `heightCm` 65.0) | an **opaque cracked crust of ash-dusted char** in plates (`bloat.ashchar` #2E2C34: light band #7D7872 ash grey, L* 51, C* 4; base band charcoal L* 18; shade near-black — the crisp toon terminator between ash and char; the radius dips along every crack, so the plates bulge between them), dark cracks radiating from a **torn vent** at the crown (a dark **Y-shaped tear** — three uneven arms, `VENT_ARMS`, the long one 6.7 cm — inside four uneven torn lips, `VENT_LIPS`; breach-violet only as a **sliver on the inner lip** of the long arm, `vent_lip_strip`: never a ring, a star or a pupil), branches and ring cracks, **breach-violet fissures** (`bloat.marker` #B02FD0, emissive .8 — never ember) in the primaries and the upper ring, and the clot's violet rim on the crust (slot 2 `MI_AF_Toon_Cinderbloat_Crust` on `M_AF_Toon`, `RimOverride` .65). Ash flakes drifting off it are a runtime emitter (`fx.ashFlakes`, A12). |

Clips (the slime's `SlimePose` model: squash / stretch as bone scale, lean, sideways tilt, hop, the pseudopod; the inner
items ride the body). Clot (speed 38 → 1.14 m/s; `refSpeedCmS` 120): Idle 667, Walk 600 (no-slide hop, `FX_Land` 510),
Attack01 500 (the slime's slam; WindupEnd 155, Contact 250; the wind-up rears 1.2 cm off the ground, the sparks swell and
gather to the front), Hurt 200 (the sparks dim), Death 500 (melts into a tar puddle, sx 1.6 / sy .5: the sparks gutter
out, the helm turns brim-down and settles with the bolts, their tumble dying out with it), Stun 600. Bloat (speed 30 →
0.9 m/s; `refSpeedCmS` 90): Idle 1200 (a wheezing breath with a catch), Walk 1000 (a lurch, `FX_Land` 850), Attack01
750 (a heavy slam; WindupEnd 248, Contact 400), Hurt 260, **Death 900 = the fuse** (`FX_Fuse` 0: the crust gasps, swells
and shudders while glowing violet seams break out of the cracks — the `core` bone scales the fuse ridges from .55
(hidden) to 1.08; the fuse light swells, `fx.fuse.glow` r 45 cm, α .12 → .65; **`FX_Burst` 800**: A12's violet-black
`death.burst` + ash ring, the crimson ring decal r 2.2 m fills to it, and the crust **collapses** — every body bone to
.001 about the ground centre between 783 and 800 ms, so each fissure, ridge and the vent go out with it — while six low
dark char shards (`bloat.crack`, near-black in every band; on the bones the bloat left unused) are thrown out on a low
arc and land flat on the scorched puddle by 900 ms, within 0.55 m of the centre, inside the 1.2 × decal), Stun 700.
Both **Stun** loops are a lopsided sag (`STUN_TILT` .36 ± .13): the body slumps toward its left and holds there (the
tilt never crosses zero: ≈ 15 cm at the clot's crown, 19 cm at the bloat's — the first pass's 11 / 14 cm was still a
subtle shear from the 50° camera), squashed lower than Idle, its top circling slowly off-axis (lean ± .16, yaw ± 7°);
the clot's sparks slow, dim and sink. A stunned blob differs from an idle one without the R8 stars (`anim_Stun`: the
outline itself goes lopsided and rolls). Manifest `boneUse` lists every inner bone.

**The fuse's runtime contract** (`breach.FUSE_RUNTIME`, manifest `fx.fuse.runtime` and the Death clip's `runtime` /
`usage`): `AAbyssCharacterActor::BeginDeath` plays Death at clamp(clipLength / deathDuration, .5, 2); at the core default
deathDuration (500 ms) that is rate 1.8 and `FX_Burst` lands at 444 ms, under the ≥ 800 ms fuse rule (monsters.md 1.3),
and a directional killing blow throws, hops and spins the body and starts its fade at 0.25 × duration (or after the
throw) — before the burst. The cinderbloat needs **deathDuration 900 ms (rate 1), no death throw / hop / spin, no body
fade before `FX_Burst`** (fade the shards with the puddle after the clip). Raised to the core (monster
`deathDurationMs`) and UE (`BeginDeath`) lanes.

Colour: `Monsters_March` (A1) plus the A5 appends (the registry stays append-only, so the art review 2 values are new
names; the replaced ones stay registered, unused): `clot.tar` 127, `clot.gloss` 128, `bloat.char` 129, `clot.depth` 130
(A5); art review 2: `clot.pitch` 138 (#160609, no light band), `clot.pitchdepth` 139 (#110509, emissive 1: the backing
bowl), `bloat.ashchar` 142 (#2E2C34, s .52 / l .38); `bloat.charcrust` 140 and `bloat.ashcrust` 141 were registered by a
first try (ash on up-facing faces, char on the flanks, split per face — the 7 cm faces drew the boundary as
stair-steps) and are unused. The reasons are in `A5_SWATCHES`: the A1 `clot.shell` band read as matte pinkish plastic
through the blend (any light band on a red-brown base lights toward the warm cream, `light = mix(c, #FFF4D6, l)`);
`clot.tar` (L* 8) sat 11 below the real dirt and read dark plum on dark brown, held apart only by the ink; the A1
`bloat.crust` and A5 `bloat.char` bands read as a bread roll / truffle / fleshy sac (rose-taupe #755B4D, L* 41). Marker
(manifest `"marker"`): `clot.marker` / `bloat.marker`. Wind-up tint #FFE8DC → #D11F2E elite. No ember anywhere (hue
audit: enemy art 0 ember).

Budgets (monsters.md §5: 2.9 k / 1.5 k; manifest `budget`): clot 2.17 k / LOD1 0.88 k toon (hull 0.53 ×), bloat 2.90 k
(2896) / 1.35 k (hull 0.57 ×, LOD1 0.66 ×). The bloat's ink hull proxy is the crust rebuilt on its own 48 columns without the
extra rows, grooves half as deep (`HULL_SIDES`, `HULL_GROOVE`): padding the crust's own grooved copy (0.76 ×) did not
converge in the grooves and left the draped cracks up to 6.8 mm proud of the ink — now nothing stands proud. LOD1
rebuilds the crust at 24 columns without grooves (its hull inflated past the decal lift) and drops the branches, the
ring cracks and the ring fissures (`lod1_skip`): its slot-0 decals 580 tris against LOD0's 780 (incl. the vent and the
shards). Each shard carries a smooth copy of itself as its hull proxy (the split corner normals opened the hull).
The vent's three pieces are draped with clearance passes (`Drape(..., clear=True)`: points along every edge and the
centroid of each face measured along the decal's own normal against the crust and the dark bands under the violet
sliver; corners raised 2-4 mm, inside the padded hull — 0 parts proud of the ink): the lip band's 10 cm radial quads
had sunk under the crown between their corners (a hole in the long arm). The cracks and fissures are *not* cleared:
a two-vertex strip 5 cm wide bridging a 4 cm groove sinks up to 12 mm at a face centre, and lifting it that far floats
it off the crust; following the groove needs a third vertex across every strip (≈ 250 tris over the 2.9 k budget).
They show dashed in places in the close-ups, barely at the game camera — a known residual.
Ink: the monster class + the 3.5 px request (gate 3 px): min rim **98.5 %** (clot, Attack01 F5 se — 91.6 % before the
wind-up rear: planted, the stretched dome's bottom contour met its contact line and the ground hid the ink under it;
padding the hull's upper or lower contour did not move it) and **96.3 %** (bloat, Death F51: the shards in flight).
Containment QA (`breach.containment_qa`, beside `mship`'s): helm / bolts / sparks / backing bowl vs the shell, the fuse
ridges and the shards vs the crust, every frame of every clip — 0 pairs (the ridges are allowed through the crust in
Death only, on purpose), shell never under the ground; the helm turns brim-down by settle .6 in Death (tipped on its
side, its brim reached under the sole when it sank and turned at the same pace).

What it took. The clot's interior: an inward-facing inner dome on the shell slot (the first attempt) stacked two blended
layers (UE sorts sections of one draw by triangle order) and let 14 % of the ground through; the slime's lit backing
bowl caught the warm rim at low pitches and in the stretch poses (Blender keeps custom normals in each face's frame) —
the bowl is now unshaded and stops at 9 cm, above it the rays meet the ink hull's far side. The gloss: a crescent inside
the contour read well facing the hero but turned into a white bar under the helm when the clot faced away; two glints
near the crown sat on the helm's crown in the back views (a catchlight), on the right flank they touched the helm in
se; on the back shoulder (u .66) they clear it in all eight facings. An upright helm seen from the W1 pitch was a grey
disc with concentric brim and crown rings, and a spark over its crown made it one grey eyeball with a violet pupil;
tipped toward a flank, one of se / sw looked down its crown; tipped toward the back, both see the profile — but the
back views (ne / nw / back) then look down the crown (19-41° off its axis). Lying flat (90°) spreads that evenly
(every facing 50-130°) but turns se / sw / front into the open bowl: a dark ring, worse. So the tip stays at 59° and
the crown carries a medial comb (on one lathe column: a crisp crease; half height at the apex so the helm, settled
brim-down at `HELM_FLOOR` .025 in Death, stays under the melted puddle): the back views see a helmet's top split
lit / shade, the front views a peaked kettle-hat crest. The bloat's first torn vent (an elongated slit, a violet lip
≥ 5 mm all round, dark lips round both) bloomed into a closed violet ring in a dark almond at 1080p — an eye; it is now a
Y-shaped tear with the violet as a sliver on one inner flank. The bloat's manifest `heightCm` was 58.1 (`BreachSpec`
left out the × 1.12 swell; the dome is 65.0). The bloat's cracks are
**draped** on the built crust (`Drape`: nearest point on the mesh + a 1-2.4 mm lift), not on the analytic surface — the
48 columns sample the 3 cm grooves coarsely and the chords hid the strips (dashed fissures). The fuse glow card was
75 cm: it washed the silhouette violet and failed the dark-ink check; 45 cm swells inside the crust. The first burst
splatted the intact crust into a 1.6 × disc that kept its lit band, glowing fissures and vent (a squashed pancake).

Kit changes (opt-in, behaviour-preserving; smoke test PASS): `kit.shading.build_toon_group(rim_hex=, rim_alpha=,
name=)` + `rim_override_group(hex, alpha)` — the toon core of an instance with `RimOverride` 1 (the default
`AF_ToonCore` is unchanged); `coherence/cohkit._assign` keeps a slot's manifest `RimOverrideColor` — a hex string or the
sRGB float list of the importer workaround below (`_rim_hex`) — (a slime-type shell gets it on the dithered shell; an
opaque toon slot becomes material index 3, flagged `af_toon_slot` so the bloom pass reads only its emissive swatches)
and `darkgate.figure_pass` treats index 3 as body. `mship` is not edited: `breach._wide_cells` wraps `mship._crop_hw` so
a ship that sets `crop_hw` gets wider game cells (the puddles ran off the default cells; every other asset keeps its
cells), `native_sheets` is on.

**darkgate** (R21 pending; `Previews/ch1_dark/darkgate_*_Breachclot_Cinderbloat*`, Idle F0 se). Art review 2: the
gate's ground is A10's **real march terrain** (`march:dirt`, the darkest walkable tile by median L*, 19.5; the A1 hex
proxy #3A332D, L* 21.8, flattered the clot by 2.3), and both figures pass it:

| Ground | Breachclot | Cinderbloat |
|---|---|---|
| **march:dirt** (A10, L* 19.5 — the gate) | **PASS**: ΔL* 16.3 (signed −16.3: lit L* 3.2, black tar by negative contrast; 91 % of the lit side clears the dirt by ≥ 15 — the tar below it, the violet rim (L* ≈ 38) above it); rim 100 %, marker 3 px; *at the edge* (lit, within 1.5 of 15 — black is 19.5 at most) | **PASS**: ΔL* 31.2 (lit L* 50.7, the ash band), rim 100 %, marker 3 px |
| march:grass (A10, L* 32.3) | PASS: −29.1 | PASS: +18.5 |
| stress `--ground-hex=#3A332D` (A1 mud proxy, L* 21.8) | PASS: −18.6 | PASS: +28.9 |
| stress `--ground-hex=#4C4940` (A1 ash grass, L* 31) | PASS: −27.8 | PASS: +19.7 |
| stress near-black `--ground-hex=#141210` (L* 5.6) | FAIL lit: −2.4 (a black blob on black ground: its rim, sparks and ink carry it; 29 % of its lit side — the rim — clears) | PASS: +45.1 |
| plains grass (L* 58.7, the Phase 0 baseline; neither lives on it) | PASS: −55.5 | FAIL lit: −8.0 (the ash band sits near the bright plains grass) |

Before: on march:dirt the clot FAILED (lit L* 8.1, ΔL* 11.4: the A1 proxy had hidden 2.3 of the shortfall) and the
bloat's rose-taupe band sat 10 L* above the ash grass. The clot meets the gate as option (b) of the review — shell and
backing pushed almost to black — plus a full-strength rim; the bloat's lit side is the ash band. (Re-run after art
review 2's second pass — comb, glints, torn-Y vent, stun, the bloat's `heightCm`: same numbers to ± 0.1.) Option (a),
a wider violet rim on the key-lit side, is not reachable from the art side: the rim's width (`RIM_POW`, `RB0`, `RB1`)
is a compile-time constant of the toon HLSL (`Scripts/abyss_content/hlsl.py`), not an instance parameter. The clot's
negative contrast leaves it at the edge on the dirt (a darker zone grade on the ground narrows it) and failing on
near-black ground; a per-instance rim-width input on `M_AF_Toon_Slime` (importer / UE owner) would let it clear both
by positive contrast — raised in the report.

**The importer workaround** (art review 2; remove it when the importer is fixed): `Scripts/abyss_content/manifest.
_param_value` sends every `"#RRGGBB"` slot param through `hex_to_linear`, but the toon HLSL declares `RimOverrideColor`
sRGB-encoded (`hlsl.py` TOON_INPUTS; `ue/materials.py` defaults it to sRGB floats), so a hex rim imported as the linear
floats of its own value — #B02FD0 → (.434, .028, .632) read as sRGB ≈ #6F07A1, ~15 L* darker: the clot's main separator
on dark ground and part of both monsters' family read. A `[r, g, b]` list passes `_param_value` unchanged, so both breach
slots write `RimOverrideColor` as the **sRGB-encoded floats** `[0.6902, 0.1843, 0.8157]` (= hex / 255,
`breach.rim_param`; the manifest says so in `fx.breachRim.paramNote`): UE then renders exactly the previews' rim and the
darkgate numbers above. Checked by running the importer's own `_param_value` on the shipped manifest (vector
(.6902, .1843, .8157, 1), no warning); not yet seen in the UE editor on march dirt (no editor in this lane). Owner of
the real fix: the UE importer (`Scripts/abyss_content`): treat sRGB shader inputs (`RimOverrideColor`) as sRGB, then the
breach slots go back to hex. The same importer bug hits the other hex `RimOverrideColor` slots shipped since — the
Unburied cloth (`MI_AF_Toon_*_Cloth`, #8E3A34), Valdras's cloth and the risen shieldbearer's — which keep their hex until
their owners re-ship. Wave 2: the burst / ring decal / ash flakes are runtime FX (A12) and core rules (monsters.md §2.1
[W2]); the clot's sparks bloom only through the .62 shell in UE (the preview's bloom pass sees them occluded: darkgate
`emissivePixels` 0).

## 24. The Corpseherd — dark-remake A6 (`monsters/corpseherd.py`)

尸牧婆 the Corpseherd (monsters.md §2.5, chapter1.md §10 / `cs_rf_herd`): the Chapter-1 mini-boss (raiser, M3 the
Sheepfold), a washerwoman from the crusade's baggage train who keeps the dead walking in circles with a bell on a
crook. A **re-dress of the goblin shaman** (monsters.md §5): the shaman's `SKEL_Goblin` rig (same 56 bones; unused
union chains re-placed, none added: `totem_feather_01` = the bell's pendulum, `totem_feather_02` = the crook rag), its
skirt envelope and cast clip machinery (`goblin_shaman.ShamanAnims`, extended), dressed in a human palette.
`goblins.py` / `goblin_shaman.py` / `slime.py` are not edited.

**Size (art review r1).** monsters.md's "×1.3 (≈ 1.21 m)" is the shaman's heightCm 93.4 × 1.3. Built at ×1.3 on the
shaman's rig unit she measured heightCm 112.4 and a stooped READY hood top of 104 cm — 64 % of the 176 cm footman,
child-sized under the hood (story bible: children are never monsters) — and the manifest also wrote `scale` 1.3, which
the runtime multiplies with the built-in factor (`AssetScale = Art->Scale * VisualScale`, AbyssCharacterActor.cpp):
drawn ×1.69 (≈ 146 cm), 1.95 cm ink (≈ 4.5 px instead of the gated 3.5), feet sliding ×1.3, and none of the checks
measuring what the game draws. Now the factor is **×1.5 on the shaman's rig unit, built into the mesh** (`SCALE`;
`DESIGN_SCALE` 1.3 and `GROW` = 1.5 / 1.3 scale every metre constant of the file — hood, veil, skirt, shawl, free hand,
shoes, every pose target — the crook and the bell keep their size) and the manifest ships **`scale` 1.0** with a
`scaleNote` (as Cinderbloat ×1.35 and Valdras ×1.4). The size is set on the height she is seen at: the **stooped READY
hood top measures 120.4 cm** (the brief's ≈ 1.21 m; 70 % of the footman's posed 171 cm, ≈ 0.68 of the 176 cm hero).
Manifest `heightCm` (**129.7**) is the rig's unstooped crown (`rig.spec.crown_height`), not her posed height: the
review's "≈ ×1.4" read heightCm 112.4 as the stooped height, and the first r1 re-ship at ×1.4 (heightCm 121) still
left the hood top at 112.4 cm. Posed top with the bell 128.4 cm (`lineup_march.json`; the lineup label prints
heightCm), the crook's hook ≈ 1.40 m in READY, boundsCm z 132.0, blob shadow 34.6 cm. Her game `family` is
`unburied` (monsters.md §2.0: the violet eyes are the Unburied marker) — manifest `remake.family`.

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/corpseherd.py
#   --quick (READY close-ups, turnaround, game crop → $AF_SCRATCH_ROOT/a6/corpseherd/quick) · --anims [Clip …]
#   [--facing=se] · --portrait (the bust + 2× zoom) · --no-previews · --no-verify   (~6 min a ship)
```

| Asset | Holds |
|---|---|
| `SK_Mon_Corpseherd` (`monster_miniboss_corpseherd`, `miniboss_corpseherd`) | no goblin head is built: a **deep cowl** (`corpseherd.hood`; round crown — a peak over the red skirt read as an executioner — the brow 4 cm over the face, the lower rows flaring into the shawl, painted creases so the lit crown never reads as a helmet) over a near-black void with the **violet eyes** (`corpseherd.eye` #D08CFF, emissive, rigid on `eye_l/r`; the family marker); a **widow's veil** (`VeilSurf`, in the hood's darker `corpseherd.hood`) hanging from the bridge of the nose like a cloth — three soft vertical folds (two valleys painted dark), a ragged hem with a few longer tatters, its top in the brow's painted shadow — draped onto the chin's point and falling past it to the hood's lower lip (the chin reads only through the cloth: a short veil over a dark gap and a grey crescent read as a visor over an open mouth, art review r1); the stoop (`READY_LEAN`, `head_tilt` −.20) over a **dowager's hump** under a rag **shawl** (crimson banner patch with the scorched palm on the back); a narrow flat trunk; a **long skirt patched from crusade banners** (rag panels, three crimson gores, folds, a trailing back hem, a scorched hem band) on `loin_f/b_01..02` + the legs, envelope-solved against the skinned legs and the floor; a rope belt; long ashen fingers (`corpseherd.hands` #A89A8C); the **cracked bronze handbell** (`crook.bell`, ×1.45 so it reads at the W1 camera; crack, chipped lip, verdigris) hanging from the hook on `totem_feather_01` (a pendulum the clips swing); a torn banner rag on the shaft. Sockets: the kit's `fx_feet` / `fx_chest` / `fx_head` / `fx_overhead` / `fx_hand_l/r` / `fx_eye_l/r` + `fx_bell` (the bell's mouth: bolt + ring origin) |
| `SM_MonWeapon_Crook` (`weapon_r`, `staff`) | a shepherd's crook of pale ash-wood (`crook.wood` #D9CEB4), 1.25 m, knots and grain strokes, the grip wound with cord, rolled in the fist so the hook opens outward; sockets `tip`, `mid`, `bell_tie` |
| `T_UI_Portrait_Corpseherd` | hood, veil, eyes, the bell beside the hood (`Export/Portraits`): `corpseherd.portrait` frames a box round the eyes (`PORTRAIT_FRAME`: hood and eyes near the upper-third centre, the bell beside the hood at the left — the old 40 cm bust framing gave the crook and bell the upper-left half and put the face low right); the eyes are drawn at 45 % (`PORTRAIT_EYE_K`, the in-game marker keeps its size) with the toon's emissive boost off, so they render their swatch #D08CFF (not the overbright #FFAEFF), under a violet halo open at its centre (`PORTRAIT_HALO`: no white core) |

Clips (speed 45 → 1.35 m/s, `refSpeedCmS` 135; attack every 1300 ms, range 3): Idle 1600 (breathing, a faint rocking,
the bell swaying), Walk 560 (a short shuffle under the skirt, the crook carried upright; FootL / FootR), **Attack01**
1000 (the bell-toll bolt [W1]: the crook hauled back, then swung out at the target so the bell is flung forward and
tolls — `WindupEnd` 310, `Contact` = `Release` = `FX_BellToll` 500; manifest `ranged`, `projectile`), **Cast01** 1300
(召羊 Call the Flock [W2]: she rises out of the stoop, the crook raised high and shaken, the free hand over the flock —
`FX_bell_ring` + `Release` 650; `skill` `corpseherd_call_flock`), **Taunt** 2000 (the pre-fight lines, under line 1
"Hush... don't startle my flock."; a seamless loop: she sways from hip to hip as if lulling a child — `TAUNT_SWAY`: 10°
spine roll, 2.6 cm hips, 12° hood roll, the bell swinging 16° out / 5° in — while the long hand comes up to the veil,
palm to her lips, held for a third of the loop around 1000 ms, then sinks out over the flock palm-down; the old ±4°
rocking and shoulder-high palm were Idle and a wave at the W1 camera), Hurt 300, **Death** 1400 (she sinks onto her
knees, the skirt pooling, and folds forward over them, the hood bowed low over her right hand; the crook tips over to
her right front and **slides through her fist** as it falls — the BP channel `slide` moves `weapon_r` down its own axis
0.6 m, so the crook FBX must ride the `weapon_r` bone — and lies along her right side, the fist closed under the hook,
the bell on the ground against her knuckles: `cs_rf_herd` "her hand still on the bell" / "she rings it once, softly";
the manifest notes say the same), Stun 700 (loop). `weapon_r`'s location is keyed in every clip (`Clip.key_locations`,
an opt-in of `kit.anim.bake`): unkeyed, Death's 60 cm slide stayed on the bone in every clip exported or previewed
after it (the FBX bake takes each bone's current transform — the re-import verify caught 600 mm in Taunt / Stun).

Colour: only registered A1 `Monsters_March` swatches (`corpseherd.*`, `crook.*`, shared `unburied.*` / `clapper.*`)
through `gnawers.MarchBuilder` (registered names **and** values: the build never appends or moves a swatch; 131
before and after). Marker (manifest `"marker"`): `corpseherd.eye` + the runtime `fx.eyeGlow` cards — with the
Corpseherd's own `facingFade` `clamp(3 * (dot(headFwd, toCamera) - 0.4), 0, 1)` and `depthTest` (the Unburied's
`2·dot + 0.5` kept half a card on a side-on face: a violet haze over the hood's crown in the ne / nw previews); the
review cards (`corpseherd.eye_glows`) use the same fade and a camera → eye ray that the hood's crown or brow hides;
the only glow; no ember anywhere (the bronze is hue 36°, sat .58). Wind-up tint (`fx.telegraph`) #FFE8DC → #D11F2E
elite. The bolt / ripple colour is `derived.projectileColor` #B02FD0 (`fx.bellToll`, `fx.bellRing`).

Budgets (monsters.md §5: 9 k / 3.4 k; manifest `budget`): LOD0 8.18 k toon + 3.99 k hull (0.49 ×; the folded veil),
LOD1 2.55 k + 1.18 k (0.46 ×; skips seams, patches, creases, the bell's crack / patina / loop / clapper, the hood brim,
thumbs, cuffs). Ink (measured on the ×1.5 mesh the game draws): the monster class + the 3.5 px request (gate 3 px): min
rim 94.0 % (game_poses Cast01 F39 se), median 3.99 px; no part stands proud of the ink (the shawl's 11 rows keep the hump's chords under the painted sigil and
creases; the skirt's LOD1 hull is an envelope proxy). Verify + LOD1 verify PASS (re-import, bones, bounds, clips to
0.03 mm, crook sockets).

QA (`mship`, limit 0 on every frame of every clip, no allowances): `skirtLeg` (skirt panels / gores / hem × legs),
`shawlArm` (shawl × forearms, cuffs, hands), `hoodHand` (hood + veil × the free hand), `crookBody` (crook × every
part but the gripping hand), `crookBell` (crook × bell), ground (body −0.3 cm worst, Death; the crook rests on the floor in Death, 0.0 cm). What it
took: the skirt solver bumps the loin chains only while a leg is inside its sector (`CHAIN_SECTOR` 62°) or a
chain-carried skirt point sinks under the floor (never freezing a chain), thin legs and side calf weights; the grip
below the wrist and the hand held out (-0.29) so the shaft clears the cuff and skirt; a smaller shake and a
`bell_side` channel so the bell never swings through the shaft in Cast01 / Death; Death grounded twice (the kneel,
then the folded rest).

darkgate (R21 pending — reported, not tuned; `Previews/ch1_dark/darkgate_*_Corpseherd*`; re-run on the ×1.5 mesh at
manifest scale 1.0, i.e. what the game draws):

| Ground | Corpseherd |
|---|---|
| march dirt (`march:dirt`, the darkest walkable tile, L* 19.5) | PASS: rim 98.9 % (median 4.49 px), ΔL* +32.1, marker 3 px (25 px) — was 2 px, at the edge |
| march grass `--tile=grass` (L* 32.3) | PASS: ΔL* +19.3 |
| march mud stress `--ground-hex=#3A332D` (L* 22) | PASS: ΔL* +29.8 |
| march ash grass `--ground-hex=#4C4940` (L* 31) | PASS: ΔL* +20.6 |
| near-black `--ground-hex=#141210` (L* 6) | PASS: ΔL* +46.0 |
| plains grass `--theme=plains` (L* 59; she never stands on it) | FAIL lit: ΔL* 7.5 (signed −7.1) |

The rag / hood greys and the pale crook light to L* 47-64 (median 51.6): on the march grounds they clear by
19-46; on the bright plains grass the figure sits just under its value (as A2 / A3 / A4). The eyes measured 1 px,
then 2 px (at the edge) in the hood's shadow: they are larger (`EYE_R`) and stand 9 mm proud of the void
(`EYE_OUT`), clear of the brow; at ×1.5 they are 3 px blocks with 25 marker px on every march ground (in the bust
the portrait draws them smaller, `PORTRAIT_EYE_K`, so they stay burning points). Ungated R21 diagnostics are in
`darkgate_report.json` (runs `march:dirt`, `march:grass`, `hex:*`, `plains:grass`).

Cross-lane notes. monsters.md names the Cast01 recipe `notify.bell_ring`; the runtime (`UAbyssVfxSystem::PlayNotify`)
resolves a notify as `notify.<NotifyName>`, so the shipped `FX_bell_ring` needs a recipe `notify.FX_bell_ring` (or an
alias) — A12; likewise `notify.FX_BellToll` for the bolt's launch flash. Taunt is authored as a loop: the core holds
it until the three pre-fight lines are dismissed (the hush lands around 1000 ms of each loop). `contactMs` 500 /
`releaseMs` 500 are authored; the core's ranged attack fires the projectile from `fx_bell` at Release. Core lane:
(1) the manifest `scale` is 1.0 (the ×1.3 double-scale is resolved; do not set a VisualScale for
`miniboss_corpseherd`); (2) Death translates `weapon_r` (the crook slides through her fist), so the crook must stay
attached to the `weapon_r` bone (manifest `attachments`), not to a fixed hand socket; (3) `fx.eyeGlow` carries the
Corpseherd's own `facingFade` and `depthTest: true` — `AbyssArtManifest` parses only `visorGlow` today, so when the
eye cards land, read both; (4) `cs_rf_herd` steps 2-3 ("her hand still on the bell", "she rings it once") now match
the Death end pose — no narration change needed.

## 25. Lord-Marshal Valdras and the elite marker — dark-remake A7 / A8 (`monsters/valdras*.py`, `elite_sigil.py`, `lineup_march.py`)

焚旗元帅·瓦德拉斯 Lord-Marshal Valdras (monsters.md §2.6, chapter1.md §11; `valdras`, the Chapter-1 story boss at the
Breach Rim): a **new mesh on SKEL_Human** — blackened plate whose seams leak breach-violet, a great helm with a burned
plume and **one glowing visor slit**, the crusade's scorched **standard grown through his spine** and rising over his
head ("the banner that will not fall"), a two-handed oath-sword whose grip has melted into his gauntlet. No face, no
crown, no skull, no throne. The kit is `valdras.py` (mesh + CLI), the clips `valdras_clips.py` (`ValdrasAnims` on the
Unburied `UnburiedAnims` base), the ship `valdras_ship.py` (`mship.MonsterShip`).

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/valdras.py
#   (no flag) ship (~7 min) · --quick [dir] [--pose=march] · --anims [Clip …] [--facing=se]
#   --frames Clip:frame[:facing] … (native 1080p cells + close-ups) · --qa [Clip …] [--debug] (PairQA, no export)
#   --portrait (the bust alone; set AF_EXPORT_ROOT to a scratch dir) · --no-previews · --no-verify
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/valdras_rim.py    # Breach Rim mood cell
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/elite_sigil.py
#   [--quick [--over]] · --gaps (gnawer facing sheets → scratch) · --patch-gnawer-sockets (manifest fx_overhead)
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/lineup_march.py  # [--ground-hex=#RRGGBB]
```

| Asset | Holds |
|---|---|
| `SK_Mon_Valdras` (`monster_valdras`) | 2.10 m to the bare crown (2.13 m helm; the hero is 1.76 m), ×1.4 the hero's bulk (0.82 m pauldron span), the standard's finial at 2.65 m (2.97 m before review round 2: the pole was cut 32 cm, its top now 35 cm over the helm): monsters.md's "×1.4 (≈ 2.4 m with the standard)" is modelled into the mesh, manifest `scale` 1.0 (the core sets no visual scale on the story boss; `scaleNote`). Great helm (barrel loft, flat crown, ridge, brow band, breaths, rivets) with the slit painted on the plate (`valdras.marker` #E8D8FF core in breach-violet lips `valdras.seam`, a dark frame round both) and a char-black plume (`valdras.plume_char`: eleven tapered four-sided locks fanning back from the crest and drooping, ash-tipped on the outer third; LOD1 keeps six); gorget, cuirass with a keel and violet cracks (`seam` / `seam_core`), the **back rupture** where the pole leaves the spine (violet ring, torn lips, roots); three fauld lames, tassets, tabard remnants (`tabard_f/b`, crimson with charred hems); flat superellipsoid pauldrons with two lames and tall haute-piece flanges; couters / poleyns with wings, cuff gauntlets, sabatons. **Seams on every join** (review round 2; `seam_ring`, 2.2–2.8 cm violet bands fitted to the plate, ≥ 2 px of core at 1080p): gorget (neck + lower lame), the keel (a seam strip with a `seam_core` line between `plate_edge` bevels), waist, fauld lames, pauldron lames, couters, cuffs, cuisse top and knee, poleyn → greave, ankles. The **standard**: one straight, rigid iron-bound pole on `standard_01..02` (from `spine_03`; weighted wholly to `standard_01` bar a 5 cm root blend, so it never bends), crossbar with knobs, a blackened open-hand finial with a violet glint, and the banner on `flag_01..03` (0.76 m: three folds, burnt tongues on the tails, the crusade's scorched palm on both faces — the front palm set 15 cm off the pole at ×0.85, so the pole never cuts it). Sockets: the kit's `fx_*` + `visor` (the runtime `visorGlow` card), `standard_top`, `banner` (the Roar ignition), `standard_root`, `fx_elite` (below); `fx_overhead` sits on the head (+45 cm), under the banner, never in it |
| `SM_MonWeapon_OathSword` (`weapon_r`, `greatsword`, two-handed) | 1.60 m: pommel, wound grip, swept quillons, a long fullered blade (`valdras.sword` #6A6D73, `sword_edge`), 9.6 cm wide at the cross, 6.8 cm at 80 % (`SW_HALF0` / `SW_HALF80`; review round 2: 7.6 → 9.6 cm so it reads as a greatsword beside the 0.82 m pauldrons); the **melt** (six heavy gauntlet-dark lumps over the cross and ricasso, drips off the quillons and the ricasso, ~1 cm violet melt seams between them) is modelled on the item round the right fist, so the sword never leaves `weapon_r`; the left fist rides the grip (`two_y`) or the pommel (`leftGripCm`). Sockets `tip`, `mid`, `guard`, `pommel` (`attackTrail` on tip / mid) |
| `T_UI_Portrait_Valdras` | review round 2: the helm turned three-quarter (yaw −100°, pitch −5°), one pauldron, the char-black plume, the charred banner edge and the scorched palm behind; no foreground fist (the left hand rests on the grip below the frame); the plate in the round-2 swatches. The slit is a **portrait variant** of the visor (`kit.portrait_slit`; the 4 cm game slit stays, it is the 3-px marker at the W1 camera): review round 3 — the round-2 nested bands still ran full length with square ends and read as an LED bar — five tapered lenses (`lens_band`: full height at the middle, .22–.40 of it at the tips), the hotter the shorter: `valdras.slit_deep` #4A1466 (2.25 cm, the full slit + 3.5°) → `valdras.seam` → `breach.pinpoint` → `valdras.marker` → a `seam_core` line over the middle half only, so the slit burns white at its centre and fades to deep-violet points (~8 % of the helm's height at the middle, a third of round 1's slab); the halo follows the slit (a 24 cm #B02FD0 α .34 wash, 12 cm violet cards at ±26°, 7 cm #D08CFF cards at 0 / ±14°, a 3.5 cm #F4ECFF hot centre; no flat white core); framing capped 42 cm over the head centre (`PORTRAIT_TOP`); `portrait_zoom2x.png` is the 2× crop of helm, slit and plume |

Clips (speed 48 → 1.44 m/s, `refSpeedCmS` 144; attack every 1300 ms): **Idle** 1860 (at attention, the sword planted
point-down, the left hand on the pommel, the cross square to the front; the banner breathing on a wind loop),
**Walk** 1000 (a march, the sword carried at shoulder-arms; FootL / FootR), **Attack01** 1300 (the two-hit combo:
a rising cut from the planted guard, `WindupEnd` 403, `Contact` 650 — the hit carries the blade high and out to
his right (twist 24°, lean 18°) so the cut reads in the back facings too —, then a backhand cut, `Contact2` 1000),
**Attack02** 1300 (Banner Sweep [W2]: coiled, the blade swept low across the front, `WindupEnd` 434, `Contact` 700),
**Cast01** 1500 (Oath-Brand slam [W2]: both hands high over the helm, the sword driven ~32 cm into the ground,
`WindupEnd` 558, `Contact` = `FX_Brand` 900, then wrenched free), **Roar** 1200 (boss intro + phase changes: the
helm thrown back −31°, chest out (lean −18°), the feet set wide and turned out, the free arm flung high; the
standard **flares** — the banner lifts and streams; `FX_Roar` 450 = the P3 breach ignition on the `banner` socket), **Lunge** 1300 (P3 charge: crouch, then a long thrust, `WindupEnd` 496, `Contact`
800), Hurt 360, **Death** 2000 (the vigil: he goes down on both knees, the sword planted point-down before him, the
helm bowed onto the cross; `Kneel` 1100; the standard **droops**: the rigid pole tips as one piece 36° back and to
his left from the rupture, the crossbar tilting with it, the banner slack and plumb under it, `Droop` 1450),
**Rise** 1500 (Death reversed: his spawn / the intro; `RiseUp` 1050; it inherits the rigid droop), Stun 800 (loop). Windups ≥ 0.62 × contact (≥ 700 ms on the
specials, chapter1.md §11).

The standard (`valdras_clips.standard_cloth`, review round 2): the pole is **one rigid piece** on `standard_01`
pivoting at the rupture; `standard_02` (crossbar, finial) never turns against it (the old two-bone droop folded the
pole at its knee). It takes back 65 % (`KEEP_UPRIGHT`) of the back's tilt only when the back would tip it forward
(into the helm), so the cuts and the kneel still carry it. `droop` (Death) aims it `DROOP_DEG` 36° from upright
toward the back-left of the pelvis heading (`DROOP_DIR`); the banner (`flag_01..03`) hinges about the crossbar under
gravity with a looping flutter (620 ms), leans with the clip's motion, flares in Roar (`flare`), and drooped it goes
slack — no wind, plumb under the tilted bar, the fly end sagging below its low end (`DROOP_SLACK`); it is clamped never
to fold in front of the pole.

Colour: the A1 `Monsters_March` `valdras.*` swatches, plus **appended** A7 swatches (`use_a7` maps the region keys
through `A7_REGIONS`; the registry is append-only, so the first-round `valdras.blackplate` / `banner` / `tabard`
stay in it unused). Review round 2: the plate's **base and sheen split** — `valdras.plate_black` (#1C2438, shadow .62
/ light .44: a cool near-black mass, base L* 14, whose light band is a narrow neutral-cool sheen, L* 53.6 measured)
on the plates, `valdras.plate_rim` (#2A3246, .48 / .56) on the bevels, flanges and lame edges; the cloth a brick
crimson with a narrow lit band — `valdras.banner_brick` (#701614, .52 / .18) and, review round 3,
`valdras.tabard_crimson` (#6E1814, .52 / .12; round 2's `valdras.tabard_brick` #621A16 at .19 still lit the tabard
#7F433A, L* 36 / a* 25, dusty in the back views of Death and Roar, and stays in the registry unused): the flared
banner samples #8A3E37 (L* 36, a* 31; the first round's #864943 read salmon-brown, a* 25), the tabard's light band
#7F312A (L* 33, a* 34);
`valdras.char` (#1A1210) on the charred hems, burn tongues and the palm, `valdras.plume_char` (#1A1414, .55 / .12) on
the plume. The banner and tabard faces
take a second toon slot, `MI_AF_Toon_Valdras_Cloth` (parent `M_AF_Toon`, `RimOverride` 1, `RimOverrideColor`
#8E3A34 sRGB, alpha .38): the kit's cream rim turned the dark crimson pale pink at grazing angles. Marker (manifest
`"marker"`): `valdras.marker` (the slit) + the runtime `fx.visorGlow` card on `visor` (radius 9 cm, core #F4ECFF,
facing fade, off at .72 of Death). Seams, the finial glint and the melt bloom violet; nothing is ember.
`fx.attackTrail` #B02FD0 (tip / mid), `fx.telegraph` #FFE8DC → #D11F2E elite, `fx.standard` (sockets, flare / droop
timings).

Budgets (monsters.md §2.6: 11 k / 4 k, a boss exception; manifest `budget`): LOD0 10.92 k toon (9.74 k slot 0 +
1.18 k cloth) + 6.35 k hull (0.58 ×), LOD1 3.93 k (3.39 k + 0.54 k) + 2.56 k (0.65 ×; fewer segments on the helm,
cuirass, pauldrons, limbs and pole, the builder's LOD1 decimation, the banner's palm and burn tongues, the joint seams
and ridges dropped (`LOD1_SKIP`), six of the eleven plume locks). The round-2 seams and plume were paid for with
8-sided seam rings, four-sided plume locks and lighter knuckles. The LOD1 loop counts every toon slot (`toon_tris`); the faulds and
tabard keep their own LOD1 hull (`add_own_hull`: a further hull decimation left the cloth 3.8 px proud of its ink).
`SM_MonWeapon_OathSword` 2.65 k + 1.90 k. Ink: boss class, 1.8 cm baked / 3.5 px screen (gate 3 px): 21 cells, min
rim 95.6 %, median 4.14 px, dark 99.6 %; the only parts proud of the ink are a joint seam (≤ 0.7 px), the shoulder
seams and the banner's charred trims (≤ 0.35 px) and the LOD1 tabard (≤ 0.7 px). Verify + LOD1 verify PASS (45 bones,
3 slots, clips, sockets, sword).

QA (`mship` PairQA, limit 0 on every frame of every clip): `swordBody` (sword × everything but the gripping fists and
the standard), `swordStd` (sword × standard), `flagBody` (banner × body), `poleUpper` (pole × helm, plume, pauldrons,
arms), `tabardLegs`, ground (body −0.5 cm); one allowance: Cast01's sword may go 35 cm into the ground (the brand is a
plunge, −32.2 cm at Contact). What it took: the pole set back (knee y .335, top .375) and the plume trimmed at the back;
quillons shortened and swept to the blade, the carry rolled 90° at the wrist, `edge_roll` so the planted cross is
square to the front; the two-knee vigil (one knee turned the tabard into a pale plank); elbows lifted in Death.
Round 2 (the shorter, rigid pole and the new plume): the pole top set at y .40, the keep-upright one-sided (only a
forward tip is taken back), Roar's helm −31°, the plume shortened, Attack01's left elbow lowered: every clip PASS.

The **elite marker** (monsters.md §3, A8): `SM_FX_EliteSigil` (`elite_sigil`, W2) — a 29 cm broken iron halo
(`elite_sigil.iron` / `iron_line`), seven uneven nails, the hoop torn open at the back with two shards adrift, a
crimson ember (`elite_sigil.core` #C0283A, never orange) in an iron claw at the front; it floats level 6 cm over
`fx_overhead` (pivot under the hoop: `SIGIL_FLOAT`) and turns at 45°/s with a 3 cm bob (`fx.motion`), absolute scale;
608 toon + 548 hull, ink 100 %. `SM_FX_EliteCrown_March` (`elite_crown@march`, W1) — `elite_crown.build_crown`
unchanged in shape, review round 2 **tarnished**: its regions remapped onto appended swatches (`CROWN_TARNISH`:
`elite_crown.iron` #55575B / `iron_hi` #66686B, grey iron with a low sheen, never chrome) with dark patina pits
(`elite_crown.patina` #2E2B26, `_add_patina`) and the crimson stone; 590 + 336, ink 93.2 %. Previews in
`Previews/SM_FX_EliteSigil/`, `Previews/SM_FX_EliteCrown_March/` (close-ups, turnaround, the measured game crop
without the glow card, `game_glow.png` with it; `*_over_monsters*.png` over the footman, zealot, gravegnawer,
bonegorger and Valdras at the W1 camera, each cell labelled with its measured clear gap; `*_over_gnawers_facings.png`:
the gnawers in se / sw / ne / nw, gate `GAP_MIN_PX` 4).

**Valdras opts out** (manifest `eliteMarker: "none"`, `eliteMarkerNote`): the visor slit is his marker, monsters.md
§2.6 bans a crown on him, and `fx_overhead` projected the marker onto the standard's pole. His over-monsters cell is
shown bare, then with the marker on the fallback socket **`fx_elite`** (head bone, 36 cm forward of and 70 cm over the
head centre, labelled "fallback, off"): sigil 8 px, crown 9 px clear in se; ≥ 6.6 px in se / sw / ne / nw (lower it
lands on the plume in the front facings, higher on the finial). The **gnawers' `fx_overhead`** moved off the head bone
(review round 2): `gnawers.GnawerKit._place_overhead` puts it on `root`, centred over the feet, `OVERHEAD_K` 1.57 × the
head centre's height (gravegnawer 120 cm, bonegorger 145 cm), so the hunched, bobbing head never rides into the
marker; the shipped FBX stand (sockets are created in UE from the manifest; `--patch-gnawer-sockets` wrote them).
Gaps: sigil 33 / 32 / 11 / 11 px (gravegnawer se / sw / ne / nw), 28 / 27 / 9 / 10 px (bonegorger); the crown ≥ 16 px.

`lineup_march.py` → `Previews/mon_family_march/lineup_march_{unburied,living,breach_elites}*.png` (+ `_inkgate`,
`lineup_march.json`): the march roster re-imported from `Art/Export` beside the shipped warrior on the march mud
(#3A332D, until A10 exports the march terrain), each figure ink-gated with its class.
Gate: each figure at its request − `GATE_SLACK_PX` (3.0 px for the characters; 2.5 px for a figure carrying a 3.0 px
elite marker, the markers' own ship gate). All 18 figures PASS (rims 94.6-100 %); round 2 re-rendered the
`unburied` and `breach_elites` pages (Valdras rim 98.3 %, the tarnished crown, the sigil on the gnawer's new socket).

darkgate on Valdras (review round 2, re-run in round 3 on the re-shipped mesh with the same numbers;
`Previews/ch1_dark/darkgate_*_Valdras*`, lit L* 53.6, rim 98.1 % at median 4.20 px, marker 3 px / 72 px on every
ground):

| Ground | Valdras |
|---|---|
| `--theme=march` dirt (L* 19.5) | PASS: ΔL* +34.1 |
| `--theme=march` grass (L* 32.3) | PASS: ΔL* +21.3 |
| `--theme=march` stone (L* 32.4) | PASS: ΔL* +21.2 |
| march ash grass `--ground-hex=#4C4940` (L* 31.0) | PASS: ΔL* +22.6 (round 1: FAIL, +13.9) |
| march mud stress `--ground-hex=#3A332D` (L* 21.8) | PASS: ΔL* +31.8 |
| near-black `--ground-hex=#141210` (L* 5.6) | PASS: ΔL* +48.0 |

Round 1 lit the blackened plate at L* 44.9 (`valdras.blackplate`), 1.1 short of 15 over the ash grass: the plate now
reads near-black in its mass and lit only in a narrow cool sheen (L* 53.6, `valdras.plate_black`), which clears every
march ground by ≥ 21. The plains (L* 59) are no longer gated: Valdras never stands on them and the march terrain is
exported (round 3 refreshed his row in the report's `plains:grass` Phase 0 baseline run, which still held the round-1
lit L* 44.9: now 53.6, ΔL* 5.2, listed failed there as every march figure is).

**Breach Rim mood cell** (`valdras_rim.py` → `Previews/mon_valdras/rim_mood_{idle,roar,death}{,_2}.png`,
`rim_mood.json`): Valdras on a broken stone dais at the W1 camera, Idle F0, Roar contact (F27) and the Death end
(F120), each neutral (`_`) and under the `breach_rim` zone mood (`Data/zone_moods.json`: ambient #6A4A8A α .35, sat
.6, contrast 1.15, haze + vignette, the grade at P14 55 %) with the violet uplight on the torch slot (#B02FD0 α .6 ×
lightScale .40 × `CharLightGain` 3.5, radius 4 m — assumed, the zone's light rig is not exported). Measured per
cell: the marker block (3–4 px; 1 px at the Death end, the helm bowed and `visorGlow` off by design), the marker
ΔL* over the plate ring round it (+50…+55 under the mood, +68 for the 1-px Death block), the seams' ΔE against the plate (77–79 neutral, 61–64 under
the mood) and the lit band's ΔL* over the dais top (+22…+24 in both).

Cross-lane notes. Core: the runtime must skip the overhead elite marker for an asset whose manifest says
`eliteMarker: "none"` (Valdras; `AbyssWorldBuilder::UpdateElite`), and read the gnawers' moved `fx_overhead` (root
bone) from the manifest. The zealot's `fx_overhead` still lets the sigil touch the hood (gap −1 px in
`sigil_over_monsters.png`; the crown clears it by 9 px) — A3's socket. `SK_Mon_Corpseherd` used to ship manifest `scale` 1.3 with the ×1.3 already in the mesh (drawn ×1.69):
resolved in its art review r1 — scale 1.0, the size built in at ×1.5 (§24). The Roar ignition (`fx.standard`) and `FX_Brand` / `FX_Roar`
need recipes `notify.FX_Roar` / `notify.FX_Brand` (A12).

## 26. The march NPCs — dark-remake A9 (`npcs/march_*.py`)

Work package A9 of `Docs/dark-remake/implementation-plan.md` §5: the nine Chapter-1 looks re-dressed for 焚境 · The
Burning March plus the two new NPCs (chapter1.md §3), the kit parts of the re-dress, and the props. Everything is built
on the shipped folk kit (`folk.py`, `npc_clips.py`, `npc_ship.py`) through new modules; the plains NPCs, their
palette names and their files are untouched (`ember_tower` and the web-exported chapters keep them).

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/npcs/march_ship.py <Npc|all>   # ~2 min each
#   <Npc> = QuestElder_March | KnellChaplain | Blacksmith_March | Merchant_March | StashKeeper_March |
#           PlainsHerbalist_March | PlainsWanderer_March | CrowTrader | EscortTravelingMerchant_March |
#           RescueLostTraveler_March | WanderingMerchant_March (or the role, snake name or game id)
#   --quick (close-ups, turnaround, game crop + a per-part triangle list → $AF_SCRATCH_ROOT/npcs_march/<snake>)
#   --anims [Clip …] [--facing=se] · --game [Clip …] (gated 1080p cells) · --barrow (Bern's barrow.png) ·
#   --no-previews · --no-verify
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/npcs/march_ship.py props      # held props + barrow
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/npcs/march_ship.py dressing   # charred anvil
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/npcs/march_ship.py kit        # SK_NPCKit_* parts
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/npcs/march_ship.py lineup     # family lineup
```

Ship the NPCs before `props` / `kit` (their ink gates are recorded from the NPCs that hold / wear them), one at a time.

| File | Content |
|---|---|
| `npcs/march_folk.py` | `MarchLook` (`folk.Look` + `dress` pieces + `expr`), `MarchFolk(folk.Folk)`: the march face expressions (`EXPRESSIONS`: kind / flat / grim / weary / haunted / fear / sly — brow line, mouth curve, no cheek blush but on the kind), the cropped hair style, the hood (a hat kind: a deep cloth hood over a short cowl that blends head → neck / clavicles; hair and ears under it are not built), and the dress pieces: half-plate (layered dented spaulders, vambraces), gorget, crusade tabard panels with the sigil cut out of the chest (a ragged window onto the plate), the neck burn scar, the rope cincture, the ash smear, Hald's sons' name-tags, Vira's knotted tally cord, Marl's rolled sleeves / stained gloves / apron stains, Mag's crow-feather collar and slung boots, the peddler's relic tray, the pilgrim's wrist ropes and cloak, Aldwin's threadbare cassock and monk's fringe, Tamsin's crypt keys, Bern's earth-caked coat and his barrow's wheel; the face cues (`lashes`, `lip`, `jaw_k`, `nose_k`, the `pixie` crop); kit groups `halfplate`, `gorget`, `hood`, `collar`, `tray` |
| `npcs/march_looks.py` | the eleven `MarchNpcDef`s: colours exactly `march_palettes.NPC_LOOKS[<role>]` (`look()` refuses a colour override; styles may change), work loop, fidget, fx, game ids, notes |
| `npcs/march_props.py` | `SM_NPCKit_Prop_{Handbell,Lantern,Ledger,Vial,Barrow,Spade,SwordNotched}` (kit + plains prop swatches only), the barrow's pitch about its axle (`barrow_matrix`, `lift_pitch`) and its wheel (`wheel_parts`, built into Bern's mesh), `SM_Prop_March_AnvilStump`; the march kinds join `props.PROPS` in the process (no plains file changes) |
| `npcs/march_clips.py` | `MarchAnims(NpcAnims)`: `Work_Toll` (Aldwin's handbell, `FX_HandbellRing` at 30 %), `Work_Vial` (Marl holds the phial before her raised lantern's panes), `Work_Barrow`, `Work_Spade` (Tamsin leans on her grave spade), the peddler's tray hand; anchored hands (Bern's fists on the barrow grips in every clip but the greeting / talk hand; Aldwin's free hand on his sternum; Tamsin's spade hand and stacked left hand; the peddler's on the tray rim) in each clip's post step; Bern's barrow bones (`barrow_state`: pitched onto the wheel in Walk / Run, the wheel spinning); Bern's `Death` (he sinks to his knees over the handles); a smaller head turn for the hooded deserter's look-round |
| `npcs/march_ship.py` | build (the look's march palette prefix `npc.march.<role>`; Bern's socket `barrow` on his bone `barrow_01`), `npc_ship`'s ship / QA / previews / verification / portrait on the march build (the review stage on the march ash grass `MARCH_GROUND["0"]`; the prop pairs leave every arm part out, Marl's rolled sleeves and gloves too; `BarrowQA`; the lifted loops' last wheel key), manifest extras (`march`, the barrow attachment note, `barrowRig`), Bern's `barrow.png` (`barrow_sheet`: profile, legs / pitch / wheel per Walk / Run phase), dev modes (`--qa [Clip …]` runs the per-frame QA alone; `--barrow`) |
| `npcs/march_dressing.py` | props (the barrow exported in `barrow_01`'s frame; its sheet cell with the wheel), the charred anvil, the kit parts (the cassock cell from the front, the arms at rest), the lineup |

| Asset | Game ids | Role | Work / extra clips | Portrait |
|---|---|---|---|---|
| `SK_NPC_QuestElder_March` | `npc_quest_elder@march` | 瑟琳·哈洛队长 Captain Serin Harrow | Guard (her sword) | `T_UI_Portrait_QuestElder_March` |
| `SK_NPC_KnellChaplain` | `npc_knell_chaplain` | 奥德温修士 Brother Aldwin | Toll (handbell) | `T_UI_Portrait_KnellChaplain` |
| `SK_NPC_Blacksmith_March` | `npc_blacksmith@march` | 哈尔德·熔剑 Hald Swordmelter | Hammer (charred anvil scenery) | `…_Blacksmith_March` |
| `SK_NPC_Merchant_March` | `npc_merchant@march` | 军需官薇拉 Quartermaster Vira | Read (ledger) | `…_Merchant_March` |
| `SK_NPC_StashKeeper_March` | `npc_stash@march` | 守冢人塔姆欣 Tamsin the Gravewarden | Spade (her grave spade) | `…_StashKeeper_March` |
| `SK_NPC_PlainsHerbalist_March` | `npc_plains_herbalist@march` | 军医玛勒 Mother Marl | Vial (lantern glow) | `T_UI_Portrait_PlainsHerbalist_March` |
| `SK_NPC_PlainsWanderer_March` | `npc_plains_wanderer@march` | 逃兵柯兰 Corran the deserter | Guard (his notched old sword) | `T_UI_Portrait_PlainsWanderer_March` |
| `SK_NPC_CrowTrader` | `npc_crow_trader` | 鸦嫂玛格 Mag Crowe | Count | `T_UI_Portrait_CrowTrader` |
| `SK_NPC_EscortTravelingMerchant_March` | `npc_escort_traveling_merchant@march` | 掘墓人伯恩 Old Bern | Barrow; Idle, Walk, Run, Hurt, Death | `…_EscortTravelingMerchant_March` |
| `SK_NPC_RescueLostTraveler_March` | `npc_rescue_lost_traveler@march` | 被缚的朝圣者 the Bound Pilgrim | Lookout; Cower | `…_RescueLostTraveler_March` |
| `SK_NPC_WanderingMerchant_March` | `npc_wandering_merchant@march` | 圣骨贩子 the Relic Peddler | Count | `…_WanderingMerchant_March` |

Kit parts `SK_NPCKit_HalfPlate`, `_Gorget`, `_Top_Cassock`, `_Hat_Hood`, `_FeatherCollar`, `_RelicTray`; props
`SM_NPCKit_Prop_Handbell` (socket `bell`), `_Lantern` (socket `glow`), `_Ledger` (Vira), `_Vial`, `_Spade` (Tamsin;
socket `tip`), `_SwordNotched` (Corran; the Captain keeps the plains `SM_NPCKit_Prop_Sword`), `_Barrow` (sockets
`handle_l` / `handle_r`, attaches to Bern's socket `barrow` on his bone `barrow_01`); `SM_Prop_March_AnvilStump`
(`npc_scenery_anvil@march`).

Conventions learnt on the march NPCs:

* **Colours are A1's.** The palette held 55 free swatches; the re-dress takes 5 (the half-plate's dent / rim tone, the
  shroud fold line, the anvil's three char tones). Every other piece re-registers a kit or plains value under the
  look's prefix (same value → same atlas index): a different shape never needs a colour. The slung boots use the
  shared bedroll tan (`#8A6A4A`), not the dark boot leather — on Mag's black coat next to her brown purse the dark
  pair vanished and the leather pair read as a second purse — over her boots' dark leather as the outsole band.
  Reused values for new shapes: Aldwin's fringe in his brow-ash grey (`#6E6A66`), the peddler's tray lining in the
  lantern frame's black, Bern's wheel in the barrow wood + `METAL_DARK`, the shrouds' grave earth in the cord tan, and
  the barrow's shrouds a value step down in Vira's greige sleeve linen (`#9A9080`, `march_props.SHROUD_HEX`; the
  phial keeps the pale `#B8B0A0`).
  **For A1** (review 12, colours A1-locked, not changed here): Bern's coat and the Relic Peddler's coat share the
  maroon `#6B3E39` — on Bern the "earth-stained coat" reads wine red, and the two field wanderers share a colour
  read; moving Bern's coat to an earth brown already in the registry (e.g. Tamsin's `#3E3630` or the trousers'
  `#4A3A2A`) would fix both. Until then his earth stains cover the hem all round and splash to the thighs.
* **The march faces are not cheerful.** The plains folk all smile with rosy cheeks; the dark remake's survivors take a
  per-look expression (brow line, mouth curve, blush only on the `kind`): the Captain grim, Aldwin weary under
  heavy lids, Corran haunted, the pilgrim afraid, Mag and the peddler sly.
* **A woman's face inside the folk kit** (review 3: the Captain read as a middle-aged man in her portrait — the
  unisex folk face + heavy brows + a square jaw + a bowl crop): `expr="grim_f"` (the grim line kept on a thinner,
  higher, arched brow, `brow_k` 0.8), `lashes` (an upper-lid line with a flick at the outer corner), `lip` (a
  cupid's bow in the rosier skin tone), `jaw_k` 0.86 (the jaw and chin rings narrowed), `nose_k` 0.84, and
  `crop="pixie"`: a side-swept fringe falling lower on her right, the sides over the tops of the ears (bare ears read
  boyish), pointed locks before them, the nape longer and tapered. Mag Crowe (鸦嫂) takes the same cues lighter
  (`lashes`, `lip`, `jaw_k` 0.9, `nose_k` 0.9) under her sly brow: with the bowl fringe of her braid and the black
  collar her portrait read as a man too.
* **Hoods**: a head cap with a face opening above the hairline (the brows stay clear at pitch −50°), room for the
  ears, a soft point at the back; the cowl's top rows ride the head (≤ 85 %) and its hem the neck and clavicles, so
  a look round (the deserter's fidget at 70 % of the plains turn) keeps the face in the opening.
* **A prop that moves on its own gets a bone** (review 1: rigid in root space, Bern dragged the barrow on its legs
  through Walk / Run). Bern's rig adds two chain bones (`MarchFolk.spec`): `barrow_01` — head at the wheel's axle,
  the socket `barrow` on it, the barrow exported in its rest frame (`march_ship.to_bone_space`) — and its child
  `barrow_wheel_01` along the axle. Every clip keys both: resting on the legs (pitch 0) in IdleNPC / Idle /
  Work_Barrow / Alert / Talk / Fidget / Hurt / Death; in Walk / Run `barrow_01` pitches up ~5° about the axle
  (`march_props.lift_pitch`: the legs 10 cm off the ground, ±0.6° with each step) and the fists are fitted per frame
  to the pitched grips (`fit_item` on the weapon bone head). The wheel is built into Bern's skeletal mesh, rigid on
  `barrow_wheel_01` — the runtime attaches one rigid prop per NPC, so a spinning wheel needs a bone — and turns 1×
  per 1.2 m Walk loop and 1½× per 2.0 m Run loop (the 0.20 m wheel rolls 0.95 / 1.59 turns: quarter-turn multiples
  keep the four-fold batten cross seamless at the wrap; the bake keys a lifted loop's last frame at the full turn, not
  the first frame's 0°). The lift is not keyed inside the loops (a loop has no first / last 150 ms): the runtime's
  clip blend (AnimTiming idle ↔ walk / run, ~150 ms) carries `barrow_01` from 0° to the lift and back, so the legs
  come off and set down over that blend. `barrow.png` (in the NPC's previews) shows it in profile from a low camera,
  one framing for every cell: IdleNPC / Work_Barrow on the legs (+0.0 cm), Walk / Run at four phases with the legs'
  clearance, the pitch and the wheel's spin printed per cell (phases chosen so the four-fold batten cross shows a
  new angle in each).
  `BarrowQA` per frame: `gripMm` (anchored fists to the posed `handle_l` / `handle_r`, ≤ 8 mm; at the plains lean
  the resting grips were 4-6 cm out of his reach — he now stoops 10° over them), `legLiftCm` (Walk / Run 7.5-13 cm,
  every other clip on the ground ± 0.8 cm). The wheel stands out past the shrouds' head ends (axle 1.80 m ahead) so
  its spin shows from the game camera.
* **Reading at 1080p**: identity props were scaled up until they read — the handbell's mouth 13 cm, the relic tray
  40 × 23 cm, the phial 12 cm in pale glass with the black blood inside (a near-black bottle merged with its own
  ink); the sigil cut-out is a 10 cm ragged window; the ash on Aldwin's brow is a soft thumb-smear (a band read as a
  unibrow).
* **Identity pieces read by their shape, not their value** (review 2, 4, 7, 8): Mag's collar is flat vane feathers
  (a centre rib, longest over the shoulders, fanned out past them in two overlapping rows; black on black, a cool
  grey sheen band only along the lit (her right) side's vanes, no pale tips — round quills with lit tips read as a
  grey beard / icicles); her boots are tied at the shafts on her left shoulder: the chest one hangs on the upper
  chest above the purse hand, its toe pointing out past her left side and its shaft rolled 40° out at the bottom so
  the boot's side — the L of shaft and foot over a thick dark outsole and heel — faces the W1 camera (hung plumb, its
  L stood edge-on to that camera and the shaft opening read as a cup; `MarchFolk.SLUNG_BOOTS`); the back one hangs
  on her left shoulder blade. Her hair is braided down her back so the front locks no longer hide the chest boot. Bern's shrouds are bodies — head knob, neck pinch, shoulders, a taper to the
  feet, the feet standing up under the cloth, cords at neck, waist and ankles — stained with grave earth up their
  sides toward the bed, so the pale mass is the tops only and Bern stays the focal point. The peddler's relics are
  reliquary wares (bone fragments bound in cord, a finger bone in a brass-wire cage, wax-sealed ossuary boxes, a
  knuckle on a cord; no skull, no skeleton) on a black lining, the tray tilted 16° to the buyer: level, its floor lay
  at a grazing angle to the closeup / portrait cameras, whose rim light washed it out behind the relics. The
  Captain's sigil is a hole: the cloth's cut edge as a dark bevel, light frayed threads into it, the cloth's shadow
  across the plate's top inside, a dent and a bright scratch on the plate; her burn's edge is two-octave rough with
  satellite flecks.
* **Threadbare is a silhouette, a hairline is a value** (review 9): Aldwin's cassock hem and cuffs end in ragged tabs
  (their own hulls: they change the outline) with torn notches between, two patches in the cassock's shade tone sewn
  with the rope's tan; his monk's fringe runs temple to temple, thick, in the ash grey of his brow smear with brown
  strands (the plains brown fringe read as a shadow); his free hand lies on the sternum, the elbow tucked in (8 cm
  higher and out by the shoulder it read as scratching his ear).
* **No two camp NPCs share a loop** (review 10): Tamsin leans on a grave spade (`Work_Spade`, both hands stacked on
  its grip — the right fist on the shaft under the T, the left fist fitted onto the T itself (`fit_item`; at a wrist
  target it floated 8 cm above, by her chin at the game camera) — the blade planted before her right foot) instead of
  Vira's ledger loop; her crypt keys are a big ring at
  the left hip (three long keys, heavy bows). Corran leans on `SM_NPCKit_Prop_SwordNotched` (notched edges, a chipped
  point, an iron guard), the Captain on the plains sword. The pilgrim's cloak hangs to mid-back, open in front
  between edges at ±50° that fall as long as its sides (front panels ending at the collar still read as a bib from
  the game camera), the hood a fat roll bunched round the back of the neck (review 11: the short capelet read as a
  green bib).
* **Notify names are global**: the Corpseherd's bell-toll bolt is `FX_BellToll` (a violet VFX recipe); the chaplain's
  handbell stroke is `FX_HandbellRing` (an audio cue only, no light).
* **A held light greets, it does not wave**: Marl's free hand is the lantern hand; a raised wave turned the rigid
  lantern into her forearm (QA `x:Lantern` 170), so her `Alert` lifts the lantern forward to shoulder height and her
  `Talk` gestures with it at chest height, the lantern kept plumb (`_hanging`). W1 (pitch −50°) drops a forward reach
  down the screen by ~0.77 × its length, so the Alert fist rises 24 cm above the shoulder for the lantern to sit at
  shoulder height on screen (review 5: at shoulder height in the world it read as held out at the waist), and a fist
  raised above the chest keeps its elbow out at the fist's height: a forearm rising from below runs up through the
  lantern hanging under it. `Work_Vial` raises the lantern fist to just above the shoulder (elbow out at its height)
  and brings the phial up before the lower panes (the glow card behind the phial) at the 'turned' key (48 %): with the
  fist at mid-chest the lantern hung under the phial's forearm, hidden from the se camera. The prop QA pairs leave out every arm part (Marl's rolled
  sleeves and gloves are `forearm_*` / `sleeve_roll_*` / `glove_cuff_*`).
* **Small pieces over another garment take that garment's ink**: Hald's name-tags and the cord's V over the apron
  bib are `covered_by` the bib's padded hull (they stood 2-3 cm proud of the torso's ink); the rest of the cord lies
  on the shirt. The Captain's burn is a ragged streak under the right ear down the jaw, mottled with islands of whole
  skin (a round patch read as a blush), plus a sweep down the neck to the gorget.
* **LOD1 budgets (5 k)**: `MarchFolk.LOD1_SKIP` also drops the relic wraps / wires / wax seals, stains, the tag cord,
  boot laces, lashes, the sigil's threads / scratch / dent, the feather ribs, the cassock's fray tabs and patch
  stitches and the burn's neck sweep / islands / flecks; the peddler's relics (4 sides, unbevelled boxes, no cage caps
  or crossed pair) and tray strap are coarser at LOD1. A skip name must not end in `l` / `r` (the test strips a side
  suffix with `rstrip("_lr")`: `relic_seal` never matched — renamed `relic_wax`).
* **Warm light only at refuges**: Marl's lantern (`lanternGlow` #FFB060 card on the lantern's `glow` socket, emissive
  panes `#F0A060` e .55) and Hald's hot bar; no other march NPC glows. Mag and the Relic Peddler toss their coins
  without the plains `coinGlint` card (review 6: #FFD860 at .9 rendered as a warm gold point light — a loot sparkle in
  a field event): their manifest `fx` is empty; the coin is its own brass mesh.

**Lineup and darkest tile** (`Previews/npc_family_march/`): `lineup_game*.png` — the eleven at the W1 camera, native
1080p, in se (Bern in profile: his 1.9 m barrow runs out of a figure cell in se), with the warrior for the R2 height
check (crowns 1.72-1.76 m); `lineup_close*.png`. `march_darkgate.py` (`coherence/darkgate.py` on these NPCs; the
re-import attaches a socket that is not a bone — Bern's `barrow` — to the bone the manifest names for it, `barrow_01`)
on the darkest march tile (`march:dirt`, L* 19.5), six to a page, Old Bern last on his page and in profile so the
barrow runs out of the row instead of over the pilgrim: rim + lit **11/11** — rim 94.5 % (Hald) to 99.5 % (the
pilgrim; Aldwin 95.9 %, up from 93.2 % with the fray tabs and the fringe), lit ΔL* 24.8 (Mag, black on black: the
lowest; 24.4 before the collar rebuild) to 61.9 (the Captain). The `marker` column (a hostile family's accent,
monsters.md §1.2) is **N/A** for a friendly NPC: the cells print `MK N/A`, nothing is painted cyan on the gate map, and
every figure's PASS / `ok` and the run verdict are rim + lit (`marker.na` per figure, `npcVerdict` in
`darkgate_npc_report.json`). R21 (the metric definitions) is still pending.

**Budgets after the review**: LOD0 9.1-11.9 k toon, LOD1 4.74-4.95 k (all ≤ 5 k); `SM_NPCKit_Prop_Barrow` 3988 toon +
2016 hull (the body-shaped shrouds: 12-sided sweeps, 12-point cords, an 8 × 6 earth-stain grid — 6.4 k at the first
tessellation); Bern's wheel is 480 toon + 184 hull triangles of his own mesh.

## 27. The march environment — dark-remake A10 part 1 (`environment/*_march.py`)

The plains decor, camp and story slots re-dressed for 焚境 · The Burning March (chapter1.md §2, §12, §15): the same game
ids, footprints and blocking (W5 / W11), themed ids `<id>@march` (§19) that the runtime resolves before `<id>` for
`emerald_plains` (U1), palettes `Props_March` / `Foliage_March` (A1). Set pieces, lore / interactive props and the echo
material are part 2 (`setpieces_march.py`).

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/terrain_march.py   # textures + terrain.march
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/foliage_march.py   # 16 + 5 LOD1
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/rocks_march.py     # 10 (+ --ridge)
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/camp_march.py      # 12
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/story_march.py     # 3 (+ retires renamed)
#   [names…] --quick (close 3/4 + 1080p game crop, front to the camera → $AF_SCRATCH_ROOT/env) · --sheet · --family
#   (family sheets only) · --no-previews; terrain_march.py --scratch (iteration) · --textures-only · --no-previews
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/vignette_march.py  # the scene
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/verify_march.py    # re-import check
```

| File | Content |
|---|---|
| `environment/envkit_march.py` | the theme layer over `envkit` (nothing in `envkit.py` / `kit/` changed): `regions(family, prefix)` = A1's registered regions by name (a march mesh samples exactly the colour script's swatches), `region(family, name)`; `Props_March` decor ink 1.4 px (`envkit.PX_OVERRIDE`, R14), `PROP_WIND_SLOT` `MI_AF_Toon_Props_March_Wind` (the plains parameters), `GROUND_SLOT` `MI_AF_Toon_Foliage_March_Ground`; `check_light` (refuses an ember emissive outside `march_palettes.WARM_ALLOWED` and a breach-violet glow outside the Breach Rim / Pit); `use_march_stage` (every review stage of the process on the march ash grass `#4C4940`, no green grid); `ship` (themed ids, `theme` / `themeOf` / `themeNote` in the entry, the light check, previews on the march ground); `family_sheet` → `Previews/env_family_march/` |
| `environment/terrain_march.py` | `terrain.py`'s painter on `Data/terrain_styles.json themes.march` (the core lane's style, A1's reference ground) + art extras painted into the BC textures (ash flecks on the grass, wheel ruts and scuffs in the mud, ash on the flagstones, charred board ends on the camp ground, oily smears on the black water) and two recorded art overrides (the data's near-black `#2E2B27` grass scorch, 27 per period, read as holes and swallowed dark monsters: `#3A3631`, two per tile; **the water** — env art review: the data's tar-black `#121418` (L* 6) made every pond a dead hole, the painted water is `#262A30` (L* 17), patches `#22262B` / `#2C3036`, shallows `#30343A`, a faint grey sheen band `#4E5258` at the shore: `artExtras.waterOverride`; the runtime water surface `M_AF_Water` reads `Data/terrain_styles.json`, so the same values are a core-lane handoff for `themes.march.ground.3`); `T_Terrain_March_Grass/Dirt/Stone/Camp_BC`, `T_Water_March_BC`, `T_Terrain_March_Details_BC`, `T_Water_March_Mask_M` (the lattice noise is shared: `T_Terrain_Plains_Noise_M`); `manifest.json → terrain.march` (the plains schema + `theme`, `zones`, `artExtras`, water `wave` / `waveAlpha` / `sheen`); previews `Previews/terrain_march/` |
| `environment/foliage_march.py` | dead trees grown from crooked tube branches (`Grower`: per-depth plan of children / length / spread / taper / wobble / rise, side twigs, the ash as `af_sub` on the faces turned up, charred undersides), `flared_trunk` (the plains root flare + a lens-shaped fire scar / alligator char blotches as sub-regions), `bark_lines`, `stiff_wind`; `DeadOak_A/B/C` (`tree@march`, + LOD1), `CharredOrchard_A/B` (`tree_round@march`, black apples, + LOD1), `Briar_A/B` (`bush@march`: a low thicket — 14 / 16 thorny canes arching in hoops over a dark core `briar.core`, each hoop on the thicket's dome with its plane at a random tilt so they cross instead of radiating, three heights, the dimmed bark `briar.cane_dark`, one escaped cane, rags ≈ 20 × 28 cm snagged where the top hoops run down the side; ≤ 1.8 k toon. The 20-cane fountain read as a pale radial starburst at 1080p; the plains bush's cloud of lumps in any dead colour read as a rock pile), `AshGrass_A/B/C` (`grass@march`, no vertex under z 0: `clamp_ground`), `AshPoppy_A/B` (`flower@march`: dull blood red `poppy.petal_dull` #6E2420, light .12), `AshCap_A/B` (`mushroom_red@march`), `Reeds_A/B` (`lily_pad_plains@march`: dead reeds standing out of the water as uninked ground cover on `MI_AF_Toon_Foliage_March_Ground` — the inked slick plates read as slabs on the pond and are gone, floating 1.6 cm over the pivot). Dead oaks: crowns ≤ 3 m (2.1-2.9 m; limbs steeper and shorter), bark `deadoak.bark_dim` #463C34 lit L* 37 and ash `deadoak.ash_dim` (appended swatches aliased onto the `bark` / `ash` keys, `dim_bark`) |
| `environment/rocks_march.py` | `BrokenStone_A/B/C` (`rock@march`: dressed blocks snapped across, chipped, chisel marks, a column drum), `AshBoulder_A` (`boulder@march`), `SM_Outcrop_March_V0..V5` (`outcrop_plains@march`: `rocks.outcrop_boulders` in dark stone `#4A4640` under a dull ash crust `outcrop.crust_dim` #57534D (lit L* 43.5; aliased onto the `cap` key, `outcrop.cap` stays registered); V2 / V5, the plains hedges, carry a fire-killed snag in the dimmed bark; the V0 / V4 poppy `outcrop.poppy_dull`); `--ridge` → `env_family_march/outcrop_ridge.png` |
| `environment/camp_march.py` | Knellwatch's kit (chapter1.md §2): `SM_Palisade_March_Pew_A/B/C` (`palisade_plains@march`: a sharpened pew-end board with its seat rail, a charred stake with a nailed crusade shield bearing the **open palm** scorched dark (`palm_sigil`; never a cross — story-bible §12), a stake with a wheel hub of sharpened spokes), `_GatePost` (`palisade_gate_plains@march`: kettle helms, the black bell pennant), `SM_Prop_March_BellFrame` (`camp_tent@march` + `setpiece_bell_frame`; the bell a dull dark bronze `camp.bronze_dull` with the lip as its one lit rim, verdigris in thin streaks), `_Hearth` (`camp_campfire@march`, inside ≈ 1.1 m), `_HearthDressing` (`setpiece_hearth_dressing`: the stools and blanket line, blocking `HEARTH_DRESSING_CELLS`), `_KnellwatchBanner` (`camp_banner@march`), `_SupplyCrate` (`camp_crate@march`, the open palm stencilled #6B2420 + chalk tallies), `_RationSacks` (`camp_barrel@march`, chalk laid on the cloth), `_Well` (`camp_well@march`), `_Brazier` (`camp_torch@march`) |
| `environment/story_march.py` | `SM_Prop_March_BrokenBelfry` (`decor_ruins@march`, story_rf_belfry "Ashwheat's Broken Belfry" — renamed from `SM_Prop_March_FallenBelfry`: "Fallen" is on story-bible §12's never-use list; `retire_renamed` drops the old entry, FBX and previews; the split bell in the keepers' dull bronze with verdigris streaks, mud-caked), `_RedBanner` (`decor_war_banner@march`, story_rf_red_banner), `_PenitentsAltar` (`decor_broken_altar@march`, story_rf_penitents_altar) |
| `environment/vignette_march.py` | the plains vignette map dressed through `<id>@march` (`kit.theme` resolution, as U1 will) on the composited march terrain, the warrior on the road facing a risen footman, a gravegnawer and a Red-Oath zealot, the Captain by the hearth → `Previews/env_vignette_march/` |
| `environment/verify_march.py` | re-imports every `theme: march` environment FBX (+ LOD1) and checks it against its entry: slots and the toon slot's name, triangles per slot, palette UVs on swatch centres, `AF_Data`, `UVWind` on wind assets, bounds ±0.5 cm, sockets, LOD1 triangles, themed ids mapped with a base id (`theme.audit`), footprint / blocking equal to the plains slot's, the normal gate (≤ 2 % of the toon loops > 60° off their face per LOD; set-piece art review r1) → `env_family_march/reimport_check.json`, exit 1 on a failure; covers the set pieces (`environment.setpieces_march`) too |

Reserved light (chapter1.md §15) as built: the only warm emissives are the hearth (`hearth.ember*`, socket `flame` for the
warm light) and the bell-frame lantern (`lantern.glow` #FFB060, socket `lantern`); the brazier's coals (`brazier.coals`
#6A2A1A, e .25) sit below the ember band under soot. Its recipe is **`camp.torch@march`** (`vfx/fx_recipes_dark.py`,
`vfx_recipes.json`: grey-brown smoke, a few dull red sparks #7A3420, ash flakes; **no light**); the brazier's entry carries
`vfx {socket flame, recipe camp.torch@march, light false, status}`. **Not shippable until U1**: `AAbyssZoneActor::
BuildCamps` adds `camp.torch` (a flame + a #FFB060 light) at every torch prop — EffectSpots must resolve
`<recipe>@<zone presentation theme>` first, as the decor `<id>@march` lookup does (handoff). No violet anywhere in part 1.
`envkit_march.check_light` gates every ship.

Runtime notes (U1 / U4): camp layout v2 places no tents, so the bell frame (the hub centrepiece and the lantern, one of
the four reserved warm lights) never appeared — it also answers to the set-piece id **`setpiece_bell_frame`**: U4 places it
in Knellwatch (door −Y toward the hearth, the 2 × 2 body blocking, ≥ 2 tiles clear of every NPC slot; socket `bell` = the
toll's ripple origin, `lantern` = the warm light); the hearth is the respawn point (not blocking, inside ≈ 1.1 m like the
plains campfire) and **`setpiece_hearth_dressing`** (its stools and blanket line) is placed with every march hearth, same
tile and yaw, blocking its `blockingCells` (mesh-local offsets; the hearth cell never); the march wind instance `MI_AF_Toon_Props_March_Wind`
(banner, Red Banner, hearth blankets) and the ground-cover instance `MI_AF_Toon_Foliage_March_Ground` take the plains
parameters; the reeds float like the lily pads (pivot 0.5 cm over the water).

Regions appended on top of A1's colour script (append-only, dark and low-chroma like it; each documented where it is
declared): `briar.mass`, `briar.cane` (the canes in the dead oak's bark value: A1's `briar.stem` merged with its ink into 2.5 px lines), `briar.mass_rust` / `briar.leaf_rust` (dead leaves), `reed.slick` (`scum.oil`'s glossy band lit the flat scum a pale slab) (`foliage_march.EXTRA_REGIONS`), the outcrop / boulder stone `#4A4640` and crust `#64605A`, stone
line tones and the outcrops' dull poppy (`rocks_march.EXTRA`), `camp.pole` / `camp.bar` (thin parts' outline mix, like the
plains), `camp.charred_deep` (roofs: A1's `camp.charred` lit band turned a sky-facing roof tan), `story.redbanner_dark`; env art
review (march): `camp.bronze_dull` / `story.bronze_dull` #64553C and `camp.patina_streak` (the bells), `briar.cane_dark`
#463C34, `briar.core` #2E2824, `briar.rag_tabard` #5E1C1C, `deadoak.bark_dim` / `outcrop.bark_dim` #463C34,
`deadoak.ash_dim` #66605A, `poppy.petal_dull` / `outcrop.poppy_dull` #6E2420, `outcrop.crust_dim` #57534D — new names on
new swatches, aliased onto the generators' keys (`b.swatch[key] = b.region(new, …)`), so every registered name keeps its
swatch.
The walls and the boulder use the darker stone because ~800 wall tiles at A1's `outcrop.stone` (lit L* 53) out-valued
the monsters' lit sides.

Ink and budgets: every shipped asset passes the env ink gate on the march ground (decor 1.2 / 1.4 px, line tone, baked-rim
bound) — numbers in each entry's `inkGate`. Dead oaks 3.6–4.8 k toon (+ hull ≈ 0.9 × toon: thin branches carry their own
hulls), LOD1 0.9–1.2 k; orchard 2.6–3.2 k, LOD1 1.2–1.5 k; briar 1.66 / 1.77 k toon + 1.04 / 1.08 k hull (was 2.7–2.9 k: under the plains bush's 1.95 k; thorns un-inked: the gate integrates a sub-3 px part's hull right through it); ground cover ≤ 330 (no ink); reeds 0.27–0.45 k (no ink); rocks 0.2–1.1 k; outcrops 1.1–1.7 k; camp 0.3–1.8 k; story 0.8–2.2 k.

Review (all native 1080p, truecolour): per asset `Previews/<asset>/` (`game`, `game_1080crop`, `game_inkgate`, `closeup`,
`turnaround`, trees `lod1`); family sheets `Previews/env_family_march/` (`foliage_trees`, `foliage_orchard`,
`foliage_small`, `rocks_outcrops`, `outcrop_ridge`, `camp_palisade`, `camp_kit`, `camp_kit_small`, `story_decor`);
`Previews/terrain_march/`; `Previews/env_vignette_march/`. Labels sit in reserved bands (`envkit.write_banded`,
`banded_grid`): the sheet header on line 1 of each row band, each cell's name on line 2 above its column, a scene shot's or
vignette crop's label in a 16 px band above it — never over a prop's top edge.

Darkgate on the march ground (`coherence/darkgate.py`, which picks the darkest walkable material once
`terrain.march` exists; `Previews/ch1_dark/darkgate_march_dirt_p*.png`, report `darkgate_report.json` run `march:dirt`):
material medians L* grass 32.3, **dirt 19.5** (the gate tile), stone 32.4, camp 28.3. 20 / 22 figures pass; Breachclot
fails (lit L* 8.1, dL −11.4: its void body is darker than the mud, as it was on the flat `hex:3A332D` baseline) and the
goblin shaman's marker reads 1 px; the Corpseherd, goblin, goblin chief and risen arbalester pass at the marker edge
(the Corpseherd's re-ship at ×1.5, §24, clears it: a 3 px marker, 25 px).
The earlier flat `hex:4C4940` run (the march grass value) shows Cinderbloat and Valdras under dL 15 on the grass.
(Breach art review 2 re-shipped both breach monsters: on `march:dirt` the Breachclot now passes, dL −16.3 — 21 / 22
figures pass, the goblin shaman's 1 px marker is the one miss — and the Cinderbloat clears the dirt and `march:grass`
by 31 / 18; §23.) These
are monster-side values reported, not tuned away: R21 (the metric definitions) is still pending.

### Env art review (march), round 1 — what changed

All re-shipped (FBX, merged manifest entries, previews, ink gate on the march ground, `verify_march` re-import check),
family sheets and the vignette re-rendered.

| # | Asset / file | Change |
|---|---|---|
| 1 | `SM_Palisade_March_Pew_B` | the black cross-bar mark on the crusade shield (a Latin / Templar cross at 1080p; story-bible §12) is the church's **open palm**, fingers up, scorched dark (`camp_march.palm_sigil`: a palm strip, four parted fingers, the thumb), a scorch streak and two scratches on the red field; on every palisade stake slot |
| 2 | `SM_Outcrop_March_V0..V5` | the ash crust `outcrop.crust_dim` #57534D (light .12, lit L* 43.5; was #64605A lit 55); the snag in the dimmed oak bark. Vignette lstats (the vignette at native 1080p, an ID pass per group — every group's toon slot a flat emission, ink and the rest black — and the L* of the beauty frame over each group's pure pixels): ridge p50 / p95 / p99 **29.9 / 45.0 / 69.0** (was 29.9 / 55.3 / 71.8; the p99 is the kit's cream Fresnel rim on the silhouettes, 3.6 % of the ridge's pixels) against the hero 66.9 / 84.2 and the Captain 73.1 / 84.7 — the ridge's p95 sits ≈ 22 L* under the hero's median |
| 3 | `SM_Foliage_March_Briar_A/B` | rebuilt as a low thicket (`build_briar`): 14 / 16 canes arching in hoops over a dark core `briar.core` #2E2824, each hoop on the thicket's dome with its plane at a random tilt (they cross; no spokes from a centre), three heights, the dimmed bark `briar.cane_dark`, one escaped cane; rags ≈ 20 × 28 cm snagged where the top hoops run down the side (A undyed + a tabard scrap `briar.rag_tabard` #5E1C1C, B one large scrap). 1.0 × 1.06 × 0.58 m / 1.22 × 1.01 × 0.49 m; **1656 / 1772 toon + 1044 / 1076 hull** (was 2693 / 2911 + 1937 / 2095; the plains BerryBush_B is 1951 + 1628) — ≈ 210 k fewer zone-decor triangles over ~200 bush tiles |
| 4 | `SM_Prop_March_Brazier`, `vfx_recipes.json` | new recipe **`camp.torch@march`** (`fx_recipes_dark.world`: smoke #3E3833, dull red sparks #7A3420, ash flakes; **no light**), binding `runtime` in the `dark` section; `fx_validate` 177 recipes, 0 errors; the brazier entry's `vfx` block + note: **not shippable with `camp.torch`** until U1 resolves `<recipe>@<theme>` |
| 5 | `SM_Prop_March_BellFrame` | also answers to `setpiece_bell_frame` (U4 places it; camp layout v2 has no tents) |
| 6 | `SM_Prop_March_Hearth` + `SM_Prop_March_HearthDressing` | the hearth keeps inside ≈ 1.1 m × 0.3 m on the 1 × 1 respawn tile; the stools and blanket line are the blocking set piece `setpiece_hearth_dressing` (`blockingCells` mesh-local) |
| 7 | `SM_Prop_March_SupplyCrate` | the ring-and-sword stencil (read as Φ) is the open palm in #6B2420, the chalk tallies kept |
| 8 | `SM_Prop_March_BrokenBelfry` | renamed from `SM_Prop_March_FallenBelfry` (manifest entry + `decor_ruins@march`, FBX, previews retired by `story_march.retire_renamed`; `renamedFrom`, displayName "Ashwheat's Broken Belfry"); the split bell a dull dark bronze `story.bronze_dull` #64553C (lit #706148), the lip the one lit rim, verdigris in thin streaks, mud-caked |
| 9 | `SM_Prop_March_BellFrame` bell | the same bronze: `camp.bronze_dull` body, `bell.bronze` lip only, `camp.patina_streak` streaks |
| 10 | `SM_Foliage_March_DeadOak_A..C` | limbs steeper and shorter, forks spread less: crowns 2.92 × 2.30 / 2.29 × 2.63 / 2.08 × 2.52 m (A was 4.2 m; the plains oak 2.5 m), 4.2 m tall; bark `deadoak.bark_dim` #463C34 (lit L* 37, was 46), ash `deadoak.ash_dim` #66605A. The crown half-width ≤ 1.46 m now stays within the neighbouring tile, so the occluder fade (`DecorFadeTarget` .75 → alpha .25 on the bounds ellipsoid, tall decor) covers at most the adjacent tile |
| 11 | `SM_Foliage_March_AshPoppy_A/B` | `poppy.petal_dull` #6E2420, light .12: W1 game-crop petals median #753D36 / p90 #80453D (was ≈ #A2665B); the outcrops' poppy `outcrop.poppy_dull` likewise |
| 12 | `terrain_march` water + `SM_Foliage_March_Reeds_A/B` | painted water lifted to L* 17 (art override, `artExtras.waterOverride`; core handoff for `terrain_styles.json`); the reeds lost their slick plates and are uninked ground cover (`MI_AF_Toon_Foliage_March_Ground`) |
| 13 | `SM_Prop_March_RationSacks` | the chalk tally is ray-cast onto the lumpy cloth (`on_sack`): its padded hull no longer bulges into the black slab seen from behind; ink gate PASS |
| 14 | `SM_Foliage_March_AshGrass_A..C` (+ poppies) | `clamp_ground`: no vertex under z 0 (`boundsCm.min.z` 0.0) |
| 15 | `envkit.write_banded` / `banded_grid`, `vignette_march` | labels in reserved bands above the image (family sheets, scene shots, the vignette and its crops) |
| 16 | darkgate `march:dirt` | re-run on the full roster, the dirt kept at L* 19.5 (not lightened to compensate): **21 / 22 pass** — the Breachclot passes since A5's re-ship (dL −16.3, at the edge: lit), the goblin shaman's marker (a 1 px block, 4 px) is the one miss (handoff); at the edge: the Corpseherd, goblin, goblin chief and risen arbalester markers (2 px) |

Handoffs (other lanes; nothing outside `Art/` was edited):

* **U1 (core)**: `AAbyssZoneActor::BuildCamps` → EffectSpots must play `camp.torch@<zone presentation theme>` before
  `camp.torch` (the brazier smoulder, no light; today every march camp gets eight extra #FFB060 lights).
* **U4 (core)**: place `setpiece_bell_frame` in Knellwatch (door −Y toward the hearth, the 2 × 2 body blocking, ≥ 2
  tiles clear of NPC slots) and `setpiece_hearth_dressing` with every march hearth (same tile and yaw, block its
  `blockingCells`, never the respawn cell).
* **Core data**: `Data/terrain_styles.json themes.march.ground.3` → base `#262A30`, patch colours `#22262B` /
  `#2C3036`, `liquid.shallow #30343A`, `liquid.foam #4E5258` (the runtime water surface reads these; the painted
  texture and the previews already do); `Data/assets.json` picks up the belfry rename on the next export.
* **Monster lanes**: the goblin shaman's marker (1 px block on `march:dirt`).

## 28. March quest items, the barrowmoss node and the Lanternwisp — dark-remake A11 (`items/quest_march.py`, `icons/quest_march.py`, `pets/sprite_lanternwisp.py`)

Work package A11 (implementation-plan.md §5; chapter1.md S1-S3 / S5 / §14; story-bible.md §8-9): the Chapter-1 quest
pickups `ichor`, `moss`, `tags`, the S2 gather node, their inventory icons, and `pet_sprite` re-dressed as 灯魂 / the
Lanternwisp (three stages) with its portrait.

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/items/quest_march.py        # 3 pickups + node (~45 s)
#   --only ichor,moss,tags,node · --quick [ichor,moss,tags,node] (→ $AF_SCRATCH_ROOT/items/quest_march) · --no-previews · --no-verify
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/icons/quest_march.py        # 3 icons (re-run after the pickups)
#   --only ichor,moss,tags   scratch sheets only
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/pets/sprite_lanternwisp.py [--stage 1|2]   # ~1 min a stage
#   --quick [Clip frame] · --portrait (candidates) · --anims [Clip …] (→ $AF_SCRATCH_ROOT/abyss_art_pet/pet_lanternwisp[_eN]) · --no-previews · --no-verify
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/pets/lanternwisp_darkgate.py [--tile=grass]   # darkgate on the 3 stages
```

| Asset | Game ids | Palette | Toon + hull tris |
|---|---|---|---|
| `SM_Pickup_Quest_Ichor` | `quest_item_ichor` (S1 `mat_breach_ichor`, breachclot drop) | `Pickups` (appended `qmarch.*`) | 2494 + 564 |
| `SM_Pickup_Quest_Moss` | `quest_item_moss` (S2 `mat_barrowmoss`) | `Pickups` | 2430 + 1156 |
| `SM_Pickup_Quest_Tags` | `quest_item_tags` (S3 `mat_tag_pouch`, `hunt_dreck` drop) | `Pickups` (+ `qmarch.pouch*`, `silver*`, `tarnish`, review r2) | 2356 + 1276 |
| `SM_Prop_Gather_Barrowmoss` | `gather_moss`, `gather_herb@march` | `Props_March` (A1's `barrowmoss.*`, `camp.ash`: no new swatch) | 2128 + 720 |
| `T_UI_QuestItem_ichor / _moss / _tags` | `ui` kind `questItemIcon`, `questItemKind` | rendered from the shipped pickups | 256² |
| `SK_Pet_Lanternwisp` / `_E1` / `_E2` (+ `_LOD1`, `A_Pet_Lanternwisp[_EN]_{Idle,Walk,Attack01,Cast01,Hurt}`) | `beast_pet_sprite` + `pet_sprite` / `beast_pet_sprite_e1` / `_e2` | `Pets` (appended `lanternwisp*.*`) | 3320 / 3808 / 3988 + 2436 / 2484 / 2484; LOD1 1626 / 1750 / 1840 + 1230 / 1254 / 1254 |
| `T_UI_Portrait_Pet_Sprite` (overwritten) | `ui` portrait, `artIds` `pet_sprite`, owner `SK_Pet_Lanternwisp` | | 512² |

**Ids.** The runtime resolves a quest node `quest_node:<itemKind>` through `gather_<kind>` → `quest_item_<kind>` →
`<kind>` → `gather_node` (`AbyssWorldBuilder`), so the S2 spots (itemKind `moss`) need `gather_moss` (a plain remake id);
the node also carries A1's planned slot `gather_herb@march` (`march_palettes.VARIANTS`) so a herb-kind gather in a
march zone never shows an emerald herb. The drops (`ichor`, `tags`) find `quest_item_<kind>`. The pet takes the
**base** ids: the remake renames `pet_sprite` everywhere (GameData `data.pet.pet_sprite.name` = 灯魂 / Lanternwisp) and a
companion follows the hero across themes (`ember_tower` keeps the plains presentation, the pet must not change there),
so `@march` would be wrong; `SK_Pet_Sprite*` keep their files with `gameIds: []` + `supersededBy`, and the portrait is
written under the runtime's name for `pet_sprite` (`AbyssUiContext::PetPortrait`). The `ui` entry names its new owner
and carries `icons/portraits.check` of the new texture; `Previews/pet_lanternwisp/portrait_medallion.png` draws it as
the UI medallions (`portraits.medallion_sheet`: conversation, say, well, card). The shared sheet
`Previews/portraits/medallions_*.png` still shows the seed sprite until `icons/portraits.py` runs (not run here: it
rewrites every portrait entry and other lanes' portraits are in flight) — **schedule it before UI sign-off**.

**Pickups** (loot conventions of §13: interactive line ink 2.0 px, pivot on the ground, readable from any spawn yaw,
`loot` record + `beam` / `label` / `glint` sockets; quest pickups bob and rock like every quest node). Each is
judged on the march ground (`envkit_march.use_march_stage`) and in `march_tiles.png` on all four march ground values
(ash grass, the darkest walkable mud #3A332D, stone, camp ash; front and sw, every cell ink-gated: 100 % rim / dark).
* *Ichor* — Mother Marl's sample phial: near-black ichor (#150F17, a lit band so it shows on the mud) up to the
  shoulder, a violet meniscus (#A63CC8 e .45) and five violet sparks (the breach colour on its own blood; no other
  pickup carries violet), pale smoky glass above, an undyed linen wrap and twine below (handled through cloth), cork,
  neck twine, a parchment label with two ink strokes, a tar drip down the shoulder. First pass read as a pale "milk
  bottle" with a purple band (the glass dome dominated from −50°): the level rose to the shoulder, the glass took a
  smoky tint and the wrap moved down.
* *Moss* — a handful of barrowmoss (A1's moss values) bound at one end like a sheaf: a ragged head whose outline is
  broken by seven outlined tufts high on its flanks and end (low ones read as feet), lighter tufts and six large pale
  spore caps (2.4-2.8 cm, #C8D49A e .35, the gather hint: ≥ 2 px at 1080p) on the crown, one binding of two turns of
  pale twine at a smooth neck, and an 8-root fan (±70°, two forked) out of the bound end (thin hulls ×.75). Art review
  r2: the first roll — an even cocoon, two twine bands, 1.4 cm caps — read as a grub or larva at game scale.
* *Tags* — Dreck's dark, soot-stained leather pouch (#4A3D35, `qmarch.pouch*`) on its side, mouth open on a dark
  interior, a pale drawstring with knotted ends, six oval **tarnished silver** name-tags (`qmarch.silver` #7E8389 face,
  `silver_rim` #B4BCC4 pale cool rim, `silver_stamp` #26282C: two stamped strokes and a punched hole each; two blackened
  with soot `tarnish`) on pale cords (`qmarch.thong` #BBA27A) running back into the mouth. Art review r2: the first pass
  — a tan pouch with pale-gold "bronze" tags (#A07C3C, rim #D2AE68) — read as a coin purse spilling gold next to the
  gold piles (#E8C060) and the loot bag; the tags now match Dreck the Silverpicker's own bandolier (Monsters_March
  `dreck.tags` #A8ACB0) and S3's melted silver; the retired `leather*` / `bronze*` / `stamp` / `patina` swatches stay
  registered (append-only). Budget (§13: pickups ≤ 3 k toon + 1.5 k hull): 3674 + 1866 → 2356 + 1276 (a 16-sided
  pouch on 11 rings, 10-sided tags with a 4-point rim, two stamps a tag, 5-sided cords). `Previews/quest_march/
  tags_vs_loot.png`: the pouch beside SM_Loot_Bag and the gold piles S / M / L at 1080p on the ash grass and the mud.

**Gather node** — seven barrowmoss cushions (each its own padded ink: one proxy padded round all of them tented dark
wedges out past the clod; seated low, `squash_bottom` .35) with lighter tufts and pale spore caps over a **low** clod
of ash earth (2.4 cm high), one self-contained clump. Art review r2: the runtime spawns every `quest_node:<kind>` with
`EAbyssPropStyle::Pickup` (`AbyssWorldBuilder.cpp`; `AbyssPropActor.cpp`: bob 6 px / 900 ms yoyo, rock ±8°), so the
broken grave marker the first pass leaned it against floated and wobbled in all seven S2 spots — a haunted headstone,
not a gather point; it is gone (a broken marker belongs to the static Burial Trenches decor; a non-rocking gather
style for this id would be the core lane's call, not relied on here). The baked hull hugs the ground
(`envkit.ground_hull`, band 2 cm) so the ink makes no dark tray under the cushions, and the node runs the pickups'
`march_tiles.png` gate (front / sw on the four march grounds; its cells join its ink verdict: 9 cells, 100 % rim / dark).
Spore *stalks* were tried and dropped: a 3 px stalk under a 2 px line is all ink, and their padded hulls ballooned
into black shards. No ember; the reserved-light check (`envkit_march.check_light`) passes.

**Icons** (§15 look, from the shipped meshes): the phial upright, tipped toward the viewer so the violet meniscus and
the label read, a soft violet under-glow (α .34); the moss bundle on the diagonal seen from above-front (art review r2:
crown-on, the old roll read as stones on a green melon and lost its roots) — ragged head low-left, twine, the root fan
opening up-right inside the frame — with a pale gather-hint glow; the dark pouch from above with the silver tags fanned
out and a cool glint (#F2F6FF, no gold). Gate: ink on 100 % of the silhouette, span .84.
Previews `Previews/icons_quest/quest_items_march_{1x,half,ui}.png`.

**The Lanternwisp** — a re-skin of `pets/sprite.py` on the same `SKEL_Pet_Sprite` (`sprite.build_rig`, `SpriteAnims`:
same 17 bones, rest joints and clip set; the module's names, glows and FX are re-pointed by `reskin()`): the seed ball
becomes the **lantern-gold core** (#F4C460, not emissive: the warm aura card #FFC878 behind it is the light), a face of
soot-dark eyes, smile and warm blush; a **bronze lantern cap** on the crown (on `body`) whose **bail ring** swings on
the `sprout` bone; the four wings become **soot wisps** — wavering smoke ribbons swept along an S-spine whose far end
hooks back behind the pet (a curl trailing the lantern, never a point: a pointed curved tongue read as a horn), three
soft sin² puffs (no cusps), a rounder section (.56), a small rounded last puff; 16 rings × 9 sides (14 × 8 faceted the
first chain into grey stone shards — rubble in the back views) with a hull proxy on the same rings at 7 sides (hull ≤
.75 × toon), graded by `af_sub` in five bands from a **soot root #3E3834** (darker than the ash ground #4C4940, a weak
light band .14: at .36 its lit side turned mid-grey and read as a grey stick at 1080p) to an **ash-pale tip** #A39A90
over the last ~30 % (`WISP_BANDS` .68 / .77 / .85 / .92; the first pass split them hard at half length); the tail is
the **flame** — a tongue leaving the back of the core (from under the face it read as a beak), swept into a shallow
trail behind (hanging under the core its split tip read as a foot), S-flickering, graded core gold → amber → pale gold
(`flame_pale` #FCCB76) → the ember tip (`tail_tip`), split near its end by a second shorter tongue, each tongue on its
own unpadded hull (art review r2: the smooth orange cone read as a duck's foot / beak); the tail card #FFC070 and the
`fx_tail` socket sit at the flame's tip (`TAIL_TIP`); a **tiny bronze bell** hangs on a short loop forward of the belly
(under the chin the high camera never saw it), its clapper peeking 5 mm under the mouth. E1 (×1.13) — art review r2:
its crown petals are sub-pixel at 1080p, so E1 reads by a **lit band** (`core_band` #FFF0C4, no emission: a .2 glow's
bloom ate the side ink) round the upper core under the cap and a **long forked flame** (×1.45, two long tongues from
.46 of its length); plus the five gold petals lying up the cap's slope (lowered: the cap's padded ink stood 14 mm off
them; dropped at LOD1) and ember motes. E2 (×1.27): the smoke burns clean — pale ash-gold wisps graded #D8C8A4 →
#F0E4C4 (80 % ink in their line tone), a brass cap and bell, the lit band, the forked flame (×1.36), an emissive gold
halo up the cap's slope (at the cap's widest ring its bloom washed the side ink out: dark 87 → 94 %), three motes. Warm
on purpose (DR5: warmth is the hero side's; monsters.md's `false_lantern` is cold #CFE8FF with a mouth, this one warm
with a bell).

*Ink (art review r2)*: at the sprite's 3.5 px the Lanternwisp measured a 4.3-4.4 px median (the old sprite ~3.9) and
big black halos round the bell and face in the close-ups: the padded proxies stood off the eye stack (6-10 mm proud),
the clapper (11 mm below the bell) and the crown; the eye stack is now 2-5 mm proud (ball pad 14.6 → 9.8 mm), the
clapper lifted (bell pad 11.7 → 5.7 mm), the crown petals lowered (E1 / E2 cap pad 13.5 → 9.7 mm), the flame's tongues
pad 0.5 mm, and the per-asset request is **3.2 px** (`OUTLINE_PX`, manifest `outlinePx1080` + `outlinePxNote`), gated
at the Chapter-1 character gate **3.0 px** (not request − 0.5: `mship.gate_for` is wrapped). 29 gated 1080p cells per
stage: median rim **3.82 / 3.69 / 3.64 px**, min rim at 3 px 97.3 / 96.4 / 96.8 %, min dark 100 / 97.6 / 95.9 %; QA, FBX
+ LOD1 re-import: pass.

*Portrait (art review r2)*: the seed sprite's portrait code (Idle F10, yaw −110°) caught the wisps mid-flap — two grey
blades and a wisp tip standing above the cap like a horn — and hid the flame. The Lanternwisp has its own `portrait`:
the Idle frame whose highest wisp tip sits furthest under the cap's crown (`portrait_frame`: F0, 15.4 cm under it), yaw
−95° (toward profile, so the flame trails out behind the core and the wisps sit soft behind it), pitch −20°, fill .86
(core, cap, bell and flame all in frame); `--portrait` renders candidates to scratch.

*Darkgate* (`pets/lanternwisp_darkgate.py`: `coherence/darkgate.py` on the three stages, marker N/A for a friendly pet
like the march NPCs, verdict rim + lit; `Previews/pet_lanternwisp/darkgate_march_*.png`, `darkgate_pet_report.json`):
march:dirt (L* 19.5) and march:grass (L* 32.3) **3/3 PASS** — rim 99.3 / 99.5 / 99.1 % (median 3.73-3.81 px), lit ΔL*
54.4 / 54.8 / 69.8 on the mud, 41.6 / 42.0 / 57.0 on the grass.

Previews `Previews/pet_lanternwisp[_e1|_e2]/` (the mship set + `march_tiles.png`: Idle se / Walk sw on the four march
grounds; `portrait_medallion.png` on the base stage).

Checks: kit smoke test PASS; `kit.theme.audit` clean; every `gameIds` entry is listed by its asset; registries
append-only (`Pickups` 91 → 114 → 120 in review r2; `Pets` free slots 2 / 23 / 26 reused + appended, review r2: free
slot 49 reused + 53-62 appended, every existing name on its swatch; `Props_March` unchanged by A11). Darkgate: the
Lanternwisp above (static pickups and the node are judged by `march_tiles.png` on the four march grounds); the hue
audit's enemy-art rule passes (its one gating failure, review r2, is `hit.impact*` #B07CFF from
`classes.mage.arcane_torrent.impactColor` in `unreal/Data`: the core lane's).

## 29. The march set pieces and the echo material — dark-remake A10 part 2 (`environment/setpieces_march.py`)

The Chapter 1 set pieces the UE set-piece layer places at tiles (U4: `set_pieces.json` → prop groups, non-blocking by
default, `Private/World/AbyssSetPieces.cpp`; chapter1.md §1, §2, §5, §9.1, §10, §12) and the lore echo's material.
Palette `Props_March` (A1) plus a few appended regions; ink decor 1.4 px (R14), the barricade and the order post
interactive 2.0 px; flat ground layers (the ditch bed, the ash circle) without ink, like ground cover.

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/setpieces_march.py [names…]
#   (no flag)   ship: LOD0 + LOD1 FBX, merged manifest entry, review set on the march ground, ink gate, reserved-light
#               check, hull pad gate, normal gate, re-import check of both FBX (exit 1 on an ink / pad / normal /
#               verify failure); all assets → also the family sheets
#   --quick [--facing=front|se|…]   close 3/4 + native 1080p game crop with the warrior → $AF_SCRATCH_ROOT/a10sp
#   --family · --scenes [--scene=ashwheat,breach_rim,sheepfold,knellwatch_gate,knellwatch_camp,east_road]
#   --echo [--lore=lore_rf_echo_03] (the MI_AF_Echo contract + previews)
#   --no-previews · --no-verify · --no-family
```

| Asset | game ids | where | look |
|---|---|---|---|
| `SM_Prop_March_AshwheatCottage_A/B` | `goblin_hut@march`, `setpiece_ashwheat_cottage` | Ashwheat (40,52) | the goblin-camp hut slot re-themed (the camp at (40,52) is Ashwheat; same 2 × 2 footprint, non-blocking): a low wattle-and-daub cottage, charred frame, stepped burnt thatch (A: a hole burned over the window; B: half the roof gone, bare rafters; under the holes a charred loft floor `setpiece.floor_char` L* ≈ 17 with fallen rafters and ash, the dark under-plates only under thatch that remains), the door shut and barred **from inside** (nothing nailed outside), a pale cold candle in the window (`setpiece.candle` #D2CCB8, not ember), soot; sockets `knock` (the knocking emitter), `candle` |
| `SM_Prop_March_BurnedBridge` + `_Ditch` | `setpiece_burned_bridge`, `setpiece_burned_bridge_ditch` | (38,48) | the near (−X) dry-stone abutment whole (two courses, a coping course, wing walls back along ±Y) with the deck stub on its stringers (its last plank hanging into the gap), the far abutment a low ruin (one course, a stone tumbled into the bed) with its stringer fallen diagonally down into the ditch, charred piling stumps (the near bent taller); the dry bed with mud cracks is the no-ink ground layer, its bank lips broken into lobes and near-gaps with paler crumbled edges, stones on the rim (place both at one tile and yaw); sockets `smoulder`, `echo` (lore_rf_echo_02's bridgehead) |
| `SM_Prop_March_SheepfoldWagon_A/B` | `setpiece_sheepfold_wagon` | the Sheepfold (60,55), ring r6 | overturned crusade supply wagons (weathered boards, the crimson-painted middle board, iron straps): A on its side, the bed open to −Y (face it to the ring's centre), a wheel broken off; B upside down and rolled ~24° onto its back side edge so the underside (axles, bolsters, wheels in the air) tips to the front, the front end propped on its broken wheel, the snapped tongue hanging from the front axle to the ground |
| `SM_Prop_March_BreachDais` | `setpiece_breach_dais` | the Breach Rim (15,95) | the Seal of Veins: a walkable round dais (r 2.3 m, top 5–7 cm) of cold dark slabs (`setpiece.dais_slab`) round the carved seal (rings + seven veins with violet pinpoints), split from the centre to the west (−X, the chasm decal's side) by the breach crack; through the missing outer slab, black depth with violet fissures and two hot cores; heaved slabs, broken kerb, three snapped pillars; sockets `breach` (uplight / sparks), `centre` |
| `SM_Prop_March_RitualCircle` + `_Ash` | `setpiece_ritual_circle`, `setpiece_ritual_circle_ash` | (11,91), (19,99) | five ash heaps with burnt stakes and Red-Oath rags round a centre heap (inked) + the no-ink ground layer: scorch, an ash ring (`setpiece.ash_poured` #7A756E, rendered L* ≈ 46 on the L* 31 ground) scuffed thin in places and broken where it was walked through, seven uneven radial spokes (two broken and smeared) to an inner arc, scattered smudges (no star, no cross, no wheel); socket `raise` (P2's standard-bearer) |
| `SM_Prop_March_RingBanner_A/B` | `setpiece_banner_ring` | 12 at r 9 round the dais | a scorched crusade banner on a planted spear (`MI_AF_Toon_Props_March_Wind`): the cloth a closed grid sheet (`ragged_cloth`: bellied, rippling near the hem, burnt tails; the charred hem band and the dark stripe are faces of it), the crusade's **open palm** faded on both faces (`setpiece.sigil_faded`; `palm_plates`: `camp_march.palm_sigil`'s palm, four fanned fingers and thumb as strip-gridded plates laid on the cloth's true surface 2 mm proud — the earlier bowl-and-teardrop read as a half-moon smile at 1080p, story-bible §12; kept in LOD1); B torn to its upper half, leaning, a smaller palm; socket `flame` (P3 ignition) |
| `SM_Prop_March_HelmetStake_A/B` | `setpiece_helmet_stake` | the Dead Ranks approach | crusade kettle hats (ogival crown, comb, a brim turned well down, dull `setpiece.helm_iron`; the first round-crowned version read as a WWI helmet, a flat brim from above as a mushroom cap) tilted 20-25° on planted spears, the spear blade through the crown (A, and B's middle helm), a crimson strap tail and rag; B three spears, a fallen helm, a split heater shield; helms keep their LOD0 topology in LOD1 (the brim ring is the silhouette) |
| `SM_Prop_March_BoneyardGate` | `decor_defend_boneyard_gate` (M5's `defendTarget.spriteKey`), `setpiece_boneyard_gate` | (10,32) | a roofed timber lychgate on the burial road: four banded posts on pads, a charred plank roof (three tones within ±3 L* at random, two planks burned through, two short — the old near-black / mid-brown alternation was a barcode over the leaves), shut braced leaves with crusade shields, the burial bell (dull `bronze_dull` body, lit lip) on an iron bracket off the front tie beam below the eave (under the ridge the roof hid it), stake wings; no bones; sockets `bell` (the self-ringing siege bell), `fx_hit` |
| `SM_Prop_March_NameWall` | `setpiece_name_wall` | Knellwatch, inner palisade | ~230 bronze name-tags (`namewall.tag` / `tag_dark`) nailed in eight rows on a stretch of palisade, some on cords, gaps where tags were taken down; a shelf of unclaimed bundles and two unlit candle stubs, chalk tallies; socket `nail_tag` (Hald's recovered tag) |
| `SM_Prop_March_PitMouthSealed` | `setpiece_pit_mouth_sealed` | the Pit entrance (17,26) | the burn pit's black mouth ringed with stones and ash, a squared collar, charred planks and an iron grate chained to stakes; smoulder (`brazier.coals`, no light) and breach seep (`pit.seep`) in the gaps; sockets `smoke` (the still-smoking recipe, 0.4 m over the grate) and `entrance` (the pit's centre on the ground, z 0, the mesh's axes with −Y the approach side: the `subDungeonEntrances` point (17,26) where the runtime anchors the sealed-entrance interaction — inert until W3 — and, once the seal breaks, the descent) |
| `SM_Prop_March_WagonBarricade` | `exit_gate_sealed@march`, `exit_sealed@march` | the east road exit (119,60) | two crusade wagons side-on in a chevron, shields on their sides, the load heaped in the beds (`grain_sack`: undyed-tan `camp.sack` pillows with a rope-tied neck and a crease, two to a heap; `supply_crate`: crates standing proud of the side boards, planked lids, a seam round the sides, chalk tallies on the lid, one stacked — the old ellipsoid sacks read as grey boulders, plain cubes as ore), a pew jammed in the gap, angled stakes on a ground rail; the plains sealed gate's footprint [1, 1], blocking false and socket `fx_vortex` (the exit's breach fire beyond) |
| `SM_Prop_March_OrderPost` | `lore_ancient_tablet@march`, `lore_order_post` | lore_rf_01 (45,35) | the Marshal's Last Order nailed to a leaning post: parchment (curled corner), four lines, his crimson seal, a torn older notice; the lore glyph (`lore.glyph`, the one warm mark) at its head, socket `glyph` |

**Contract notes for U4 / the core lane.** The set-piece ids are new plain ids (`setpiece_*`) except where a slot
already exists: the goblin-camp hut (`goblin_hut@march`: U4 places Ashwheat from the never-placed goblin-camp ids), the
sealed exit, the lore tablet and M5's `decor_defend_boneyard_gate`. `_Ditch` / `_Ash` go at the same tile and yaw as
their partner. Every entry records `footprintTiles`, `blocking: false`, `placement` (where, from chapter1.md) and a
`note` on facing / sockets. Fronts are −Y (the kit convention) except where the note says otherwise (the dais crack is
−X = west at yaw 0).

**Reserved light.** `envkit_march.check_light` gates every ship (LOD0 and LOD1): violet only on the dais (`dais.*`) and
the pit seep (`pit.*`); the one warm emissive is the order post's `lore.glyph` (interaction). Ashwheat's candles are
pale (#D2CCB8, sat .1): the scouts' "lights", not a refuge.

**Hull pad gate.** `Builder.add(hull_pad=True)` grows a part's hull proxy locally round whatever stands proud of it
(`covered_by`), by pushing the nearest proxy face's corners out along their normals: on a coarse proxy (an 8-corner
box, a 7-sided tube, an outline-only n-gon) a few cm of trim pushes a whole corner out into a black wedge of ink — the
first builds padded the cottage walls 50 cm (door frame, soot over the wall plate), the wagon floors 50 cm (corner
straps 43 cm away on the sides), the banner cloth 39 cm (hem decals hanging off its tails) and the ink gate passed
them all. `ship` now fails any padded part over `PAD_MAX_MM` 25 mm (`pad` in `report.json`; largest after the fix:
the pit's seal plank 23 mm). The rules: details inside a face never reach the silhouette — no ink, **no cover**; a
structural trim on an edge (wagon straps, gate battens and brace, a helm's comb, the banner lash) gets its own ink; a
part whose frame stands proud gets an explicit `hull=` proxy out to that frame (the cottage walls and gables); a
decal on a curved surface needs vertices inside it (`strip_plate`).

**LOD1 and the normal gate (art review r1).** Whole-mesh collapse decimation (the kit's `Builder(lod_ratio=0.5)`)
drops the sharp-edge flags, so each decimated 2-12 mm decal and plate smoothed its top into its rim and bottom: the
ditch and ash-circle LOD1 rendered as lumpy boulders (27 % / 34 % of their toon loops > 60° off their face, 35 / 62
flipped), the dais seal a pale dome and its breach depth a lit wedge (13.2 %, 175 flipped), the pit mouth a tan slab —
and the re-import check (triangles, UVs, bounds, sockets) passed them. Now: `SPBuilder` rebuilds LOD1 **part by part**:
the ground layers (`_Ditch`, `_Ash`) are not decimated at all (LOD1 = LOD0); parts `lod_keep` names — decals and flat
plates (`groove`, `underlay`, `crack*`, `fissure`, `hole`, `scorch`, `ash_ring`, `spoke`, `bank`, `lip`), the seal,
the breach cores and pinpoints (`core*`), the pit seep and smoulder, helm brims (`helm*`), shields, the banner palm,
the cottage loft floor — keep their LOD0 topology; every other part is decimated alone and its normals re-derived from
its own decimated faces (`repair_normals`: flat stays flat, smoothed parts get every edge over 40° hard); helpers build
their own LOD1 shapes (unchamfered boxes, 14-segment rims, 6-sided hubs, 4-sided spokes, subdiv-1 stones and heaps). **Normal gate**:
`ship` and the re-import check (`verify_static`) fail any LOD with more than 2 % of its toon loops at loopN·faceN <
0.5 (`normal_sanity`, recorded as `normals` in `report.json`); `verify_march.py` runs the same gate on every march
environment FBX (+ LOD1) and now covers this family (families with authored normals — spherised foliage, faceted
rocks — and `AUTHORED_NORMALS_ASSETS` — the penitents' altar, whose grass tuft is 34 of its 38 bad loops, LOD0 2.2 % —
gate their LOD1 against LOD0 + 2 points): 59 assets, 0 failures. All 18 set pieces: LOD0 ≤ 0.8 %, LOD1 ≤ 0.1 %. (The ash heaps' smooth
rim also averaged their buried flat underside into the lit top: `ash_heap` keeps edges over 55° hard.)

**Budget (art review r1).** Hull proxies coarser than their parts where the pad gate allows (≤ 25 mm) and the ink does
not thicken: plain boxes round the chamfered planks, boards, stakes, axles, bolsters, end boards and crates (12 hull
triangles, not 44); one-row barricade stakes; lighter wheels (rim 16 segments, was 20; spokes 5-sided, was 6). Not
coarser: wheels (a 14-segment rim / 4-sided spoke proxy padded up to 19 mm round the finer part and thickened the
wagons' baked ink to 2.1 px for the 1.4 px class), rocks and heaps (a subdiv-1 proxy of the same `RockShape` has other
bumps — the pad grew 3-12 cm round it) and the shields (an unchamfered proxy's fan faces padded 26-33 mm round the rim
strip). Kept above the 0.5-0.6 × convention: the bridge (0.96: its dry-stone courses and ash heaps carry their own
hull, the rocks family's rule), the ritual circle (1.0: 1 k triangles of heaps and burnt stakes), the pit mouth (0.84:
rim stones and heaps), the helmet stakes (0.82-0.86: thin spears and the helms' own hull). LOD1 targets ~50 % of LOD0
except where the review asked for LOD0 topology:

| asset | LOD0 toon / hull (hull × toon) | LOD1 toon (× LOD0) | before r1 |
|---|---|---|---|
| `WagonBarricade` | 8 696 / 5 308 (0.61) | 3 048 (0.35) | 8 104 / 6 728; LOD1 5 276 (0.65) |
| `SheepfoldWagon_A` / `_B` | 2 772 / 1 724 (0.62) · 2 834 / 1 886 (0.67) | 1 036 (0.37) · 1 174 (0.41) | 2 712 / 2 472 · 2 956 / 2 716; 0.74 · 0.76 |
| `BurnedBridge` | 3 164 / 3 036 (0.96: stones and heaps keep their own hull) | 1 320 (0.42) | 2 856 / 2 856; 0.69 |
| `RitualCircle` | 1 072 / 1 072 (1.0: heaps and burnt stakes, ~1 k) | 360 (0.34) | 1 072 / 1 072; 0.49 |
| `BoneyardGate` | 4 698 / 2 386 (0.51) | 1 530 (0.33) | 4 790 / 2 974; 0.54 |
| `BreachDais` | 4 108 / 2 320 (0.56) | 3 640 (0.89: the seal, its grooves, the void and its cores at LOD0 topology) | 4 108 / 2 320; 0.73 |
| `PitMouthSealed` | 2 566 / 2 150 (0.84: rim stones and heaps) | 1 378 (0.54) | 2 566 / 2 278; 0.76 |

**Lessons (art review of this family).** Flat ground decals must not carry an inverted hull: the screen-space ink of a
hull a few mm thick smears into a black wedge along the decal's far edge (ditch, ash circle) → separate no-ink ground
layers. Boxes need their 45° bevel chamfers sharp (`setpieces_march.box`: auto-sharp 30°): the kit's 50° smooths the
chamfer into the face and a big flat face whose corner normals lean out splits its cel band along the triangle
diagonal (a dark wedge on every roof plank); the same for per-vertex jitter (non-planar quads) — jitter whole boards by
rotation, never vertices. Thatch reads as thatch with stepped courses (each lip lit), short straw streaks down each
course and a lighter cut-straw band; flat slabs read as planks. Up-facing faces sit in the light band, so dark
materials on big tops need a restrained `lightAmt` (`dais_slab`, `wood_old`, `char_black`, `helm_iron`) or they turn
pale tan / grey.

### The echo material (`MI_AF_Echo`, `--echo`)

The lore `echo` look (U1: a translucent figure frozen in its last pose for 3 s; lore_rf_echo_01..03) is the soul echo's
master `M_AF_Ghost` (additive: Tint × Ghost (1 − Fade)(0.85 + 0.3 edge) lerp(0.55, 1, Body)) with the march's cold
`echo.body` colour — the manifest section `materials.MI_AF_Echo` (merged per key by `MaterialsManifest`): parent, the
CPD `Tint` (#A8B0C4 × 0.5, linear given), `Ghost` 1, the `Fade` timeline (450 ms in, 2100 ms hold with a 4 Hz ±8 %
flicker, 450 ms out), the trigger, the A12 recipe hook, the rules (cold pale only — echoes are the breach's victims,
never violet, never ember; the ink slot draws at Body 0 = a soft cold rim, never a dark line) and per echo the
recommended figure (shipped NPC skeleton + clip + frame, offset, yaw; `clipAsset` when the clip belongs to another
mesh on the same SKEL_Human). `Previews/setpieces_march/echo_<lore id>.png`: the solid figure U1 poses beside the
shipped look (the scene + the ghost maths rendered in Cycles and added in linear light) at the W1 camera, native 1080p.
lore_rf_echo_03 (art review r1: at (−0.4, 2.75) the kneeler straddled the dais kerb — the additive ghost a pale cloud
cut by grey kerb bars — and Cower 30 held its head in its hands): the Bound Pilgrim at offset (−3.05, 0.27), yaw 66°,
on open ground in the kerb's west gap at the breach, facing the seal with its back to the camera, so only the dark
breach depth lies behind the ghost; the escort rig's Death kneel (frame 22, `clipAsset`
`SK_NPC_EscortTravelingMerchant_March`) with the arms laid behind the back, wrists crossed at the small of the back —
a preview stand-in posed on the rig (`pose_arms_behind`; contract `poseOverride`). The dedicated `Kneel_Bound` clip
(kneel, hands bound behind, head bowed) stays the A9 follow-up (`followUp`); children are never among the echoes
(story-bible §11).

Review: per asset `Previews/<asset>/` (`game`, `game_1080crop`, `game_inkgate`, `closeup`, `turnaround`, `lod1`);
`Previews/env_family_march/setpieces_{ashwheat,breach_rim,wagons,knellwatch,small}.png`; composed scenes at the W1
camera with the shipped neighbours (Valdras on the dais, the Corpseherd in her ring, the A10 part-1 belfry, pew
palisade, hearth, bell frame and anvil stump) `Previews/setpieces_march/scene_{ashwheat,breach_rim,sheepfold,
knellwatch_gate,knellwatch_camp,east_road}.png`; the echoes `echo_<lore id>.png`; `report.json` (triangles, ink
gate, hull pad, normal gate, re-import check per asset).

## 30. The dark VFX family — dark-remake A12 (`vfx/fx_recipes_dark.py`, `vfx/fx_textures_dark.py`, `vfx/fx_preview_dark.py`)

Work package A12 (implementation-plan.md §5; monsters.md §1.2 DR5 / §1.3, story-bible tone pillars: aftermath not
gore, ash not blood): the march's own effects on top of the shipped VFX kit (§14) — the enemy telegraph decal, breach
fire, the ash ends of the dead, raises, bells, the echo, elite affixes recoloured for the march — plus the hue-audit
clean-up of today's hero recipes. No new monster, mesh or FBX (darkgate: nothing to run); textures, recipes and two
material contracts.

```bash
/opt/venvs/blender/bin/python unreal/Art/blender/vfx/fx_textures_dark.py [names…] [--sheet-only]
#   T_FX_TelegraphRim / TelegraphFill / FlameBreach / AshFlake / Ripple / RaiseSigil / AshNoise (numpy, ~20 s)
#   + sprites.json (merged) + manifest.json materials.M_AF_Telegraph / M_AF_AshDissolve + Previews/vfx_textures_dark/
python3 unreal/Art/blender/vfx/fx_recipes.py          # registers fx_recipes_dark (apply) → vfx_recipes.json + fx_palettes.json
python3 unreal/Art/blender/vfx/fx_validate.py         # 0 errors / 0 warnings
python3 unreal/Art/blender/vfx/hue_audit.py           # DR5 rules → Previews/ch1_dark/hue_audit_report.json + .png
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/vfx/fx_preview_dark.py [ids…|groups…]
#   groups: ambient elite notify death monster telegraph hazard corpse world (default: all 50 scenes, ~40 min)
#   --scratch (→ $AF_SCRATCH_ROOT/vfx3d/effects_dark) · --list · --index (Previews/vfx_effects/README_dark.md)
#   --measure [asset…] (socket offsets in the actor frame at the MEASURE rows' beats)
/opt/venvs/blender/bin/python unreal/Art/blender/vfx/fx_textures_dark.py --contracts   # contracts → manifest only
```

**Colour script.** Breach-violet #B02FD0 round near-black #1A0820 (pinpoints #D08CFF, hot core #F0E0FF) is enemy
magic that is not a damage hazard; crimson #D11F2E + bone #FFE8DC is the damage telegraph and nothing else; ash
(#EEEAE2 / #B8B2A6 / #8A8480 / #3A3631) is how things end; the Living carry no light; ember orange never appears in an
enemy recipe (hue audit). Palettes appended to `fx_palettes.json`: `breach`, `ash`, `hazard`, `echo`, `crimson`,
`gravecold`, `embers` (ambient.sparks' own, so an untinted context keeps abyss_rift's ember storm).

| Texture | Cells | What |
|---|---|---|
| `T_FX_TelegraphRim` | 256 × 64 strip | the telegraph rim profile: tileable in U (arc length), V = inward distance; a scorched outer line, a 45° hatch band fading inward |
| `T_FX_TelegraphFill` | 256² | cracked-ash Voronoi plates (values .66–1, exact bisector edges), tiled in world XY so the grain never swims with the fill |
| `T_FX_FlameBreach` | 4 (tall, forked, lick, wisp) | pre-coloured breach fire: black body #1A0820, deep #561278, violet edge #B02FD0, pale root tongues #D08CFF / #F0E0FF (no bubble core) |
| `T_FX_AshFlake` | 4 | flat cel ash flakes (lit face, shade band, curled rim; tinted ash grey): 0–2 irregular plates, 3 a curled flake |
| `T_FX_Ripple` | 2 | a struck bell's tone: broken outer + inner rings over a faint halo (0); hairline version for radii of metres (1) |
| `T_FX_RaiseSigil` | 1 | the raise marker: ring, seven hooked fingers, broken inner ring |
| `T_FX_AshNoise` | 1 (data) | `M_AF_AshDissolve` input: R low-frequency fBm front, G crack mask, B per-plate random (24 plates ≈ 12 cm on the body) |

| Recipes | Bound to | Look |
|---|---|---|
| `ambient.ashfall` | ambience `ashfall` (W1) | slow tumbling ash flakes + specks over a 9 m disc, the weather tint |
| `ambient.sparks` (recoloured) | ambience sparks | the event colour: the Breach Rim's violet sparks; ember storm without a tint |
| `elite.fire_enhanced / swift / teleporting / extra_strong / curse_aura / vampiric / frozen`, `elite.unburied` | elite affix loops | march recolours with silhouettes of their own: breach flames at the feet over a thin rim (Scorched), a pale dust swirl (Driven), big flickering violet tears and no rim ring (Breachwalker), weight — an iron-dark disc with a held crack, an ash heartbeat rim and dust shoved off the feet (Brutish; never crimson), a fine curse ripple, ~1–2 alive per elite (Dirgecursed), a near-black tar disc with big spiralling tar drops and a dull contracting ring (Breach-Drinker), a grey-blue rime crust with cold breath (Gravecold), violet cracks under the raised (Unburied) |
| `elite.fire_enhanced.death` / `elite.frozen.death` | W2 on-death affix effects (monsters.md §3) | Scorched: the corpse smoulders with breach fire for 1 s under U2's elite-style r2 decal, then a breach-fire burst at r2 (FX_Burst's pieces); Gravecold: grey-blue rime fades in over r2.5 and mist draws in (the pre-cue: a slow, no crimson), the ring snaps out at 600 ms, the crust holds 2.5 s |
| `teleport.blink` | EliteBlink | violet tear out / in, ash puff |
| `notify.FX_Fuse` / `FX_Burst` / `FX_Land` | cinderbloat / breachclot clips | the fuse's violet spits and tightening ring; the burst (breach flames on a ring + centre, an ash shock ring, a scorch with violet cracks, ash thrown; no gore); a dust pat on landing |
| `notify.FX_BellToll` / `FX_bell_ring` / `FX_BellRing` | Corpseherd Attack01 @500 / Cast01 @650, Bonegorger Attack02 @800 | bell tone ripples at the measured bell (the flock call: air ripples + two fine ground ripples); the clapper slam is an impact — a fast dust ring on the ground, a split, thick dust and grit (no ripple: that language is the keepers' bell) |
| `notify.FX_Roar` / `FX_Brand` / `FX_DrumBeat_1` (+ `_2..4` aliases) / `FX_Dig` / `FX_BannerSnap` | Valdras Roar / Cast01, drummer, footman, standard-bearer | breach fire flaring along the banner (on the cloth and pole, short rise), violet streaks and a pale shock ring; the sword-tip brand (fine ripple, cracks, breach fire); a drum thump; clods off the shovel blade; ash shaken off the banner |
| `death.unburied` / `breach` / `living` / `gravegnawers` / `valdras` / `cinderbloat` | monster kill (runtime: PlayFirst death.<defId> → death.<family> → death.burst, Blow = the victim's facing) | a violet breath leaving the head; tar + violet sparks; dust and a mud slap as the body lands (no light); dust and ash flecks; Valdras: violet guttering out of the standard along its droop (upright 0–860, falling 860–1260, drooped 1260–1720 ms, actor frame) and the seams, two fine ripples, dust as he kneels; the cinderbloat: a dull crust crack and an ash puff only (its fuse and burst notifies are the death) |
| `monster.raise.mark` / `monster.raise` | the raise marker (900 ms) / EvMonsterRaised (W2) | the sigil contracting under the corpse with gathering pinpoints; ash bursting off the ground, a ripple, the eyes igniting |
| `monster.block` | EvBlock (W2) | a thin clang arc and steel sparks on the shield face |
| `monster.ash_flakes` | the cinderbloat's `fx.ashFlakes` loop | flakes and a thin smoke off the crust top, a rare violet spit |
| `monster.dead_standard_bearer.aura` | the bearer's +15 % aura loop (W2, r 5 tiles) | a soft cloth-dark crimson stain breathing on the ground, ash drifting off the banner; no rim line, no fill sweep, no loose red puffs (they read as blood) (DECISIONS_DARK DR5-aura) |
| `monster.valdras.standard_fire` / `.feed` | P3 phase loop / Feed (W2) | breach fire clinging to the standard; the corpse's ash streaming into it |
| `telegraph.contact` (+ `.line`, `.cone`) | U2 contact beat (W2), unit radius | a crimson rim snap and bone dust at the rim |
| `hazard.breach_fire` / `breach_seep` / `banner_ring` | EvHazard / EvArena (W2) | violet cracks in a scorch with breach fire; one continuous fissure along the seep line (Blow = origin → target) under the hazard-style crimson line through both ticks; a wall of breach fire round the arena edge (dense tongues + licks + a smoke band) under the arena-style decal |
| `corpse.crumble` / `expire` / `burn` / `consume` | U3 husk timeline (W2) | ash dust and flakes as the body crumbles; the husk lifting away; a flare in the fire's colour, soot, scorch; pale ash, flakes and grit tossed at the gnawer's jaws (origin = the gnawer beside the husk) |
| `prop.echo` | U1 echo lore look (3 s) | cold ash motes rising off the figure, a faint cold ground ring (never violet, never ember) |
| `camp.bell_toll` | `SM_Prop_March_BellFrame` socket `bell` (U4 / U7) | three slow soft tone ripples in the air at the bell, a faint swell on the ground, ash shaken off the frame (kept apart from the hostile clapper slam) |
| `camp.torch@march` | `SM_Prop_March_Brazier` socket `flame` (U1 themed id) | a smoulder: grey-brown smoke a step lighter than the mud, a few dull red sparks, ash; no light |

**Material contracts** (`manifest.json → materials`, written per key by `fx_textures_dark.MaterialsManifest`; the
recipe JSON's `dark.materials` carries the same text):

* `M_AF_Telegraph` (U2): one pooled translucent ground quad per telegraph (8 cm lift, no depth fade); circle / ring /
  cone / line from signed distances; rim = `T_FX_TelegraphRim` × crimson along the boundary (26 cm, 30 elite, 36
  boss with an inner rim at 82 %), fill = bone × the `T_FX_TelegraphFill` grain at 30 % (`FillGrain`; at .45 and
  above the plate edges read as flagstones) in world XY, growing from the origin to reach the rim exactly at
  contact with a bright front band; alpha .07 → .22 (bosses .27: translucency blends in linear light, so bone at .26
  rising to a .45 flash read as a near-opaque white paving over the hero's feet); urgency pulse over the last 30 %
  (bosses 50 %); contact = fill flash .35 + rim #FF3A48 for 110 ms, fade 170 ms, and `telegraph.contact*` at the
  same instant; the `hazard` style is rim only with a slow pulse, on for the zone's whole damage window (a hazard
  after a windup — the breach-seep — keeps its crimson line from the windup's contact to the last tick); the `arena`
  style (EvArena: the damage is outside) is a crimson rim measured outward (the hatch band faces the side that
  hurts, 34 cm), a faint crimson wash outside (.12, ramping in over 60 cm) and nothing inside.
* `M_AF_AshDissolve` (U3): every toon slot of a dead body in ash mode — hold 450 ms, grey 380 ms (lit AshColor
  #6E6A62 / shade #3E3B37, emissive markers out), crumble 900 ms to 38 % by whole plates (B 0.6 + R 0.3 + (1 − G) 0.15
  + height 0.3 against Dissolve × 1.4, pale edge band), the husk until the TTL, then `expire`; `burn` greys at once;
  Rise reverses it. The ink hull follows the same mask.

**Decisions (`dark.decisions`, for DECISIONS DR5 — core lane).** `DR5-aura`: the standard-bearer's "crimson
ground halo" is drawn in the banner's cloth-dark crimson (#4A0E12 / #6A1418 / #7A1E22) with no rim and no fill sweep —
warning crimson #D11F2E stays the damage telegraph's; `DR5-slow`: a slow zone that does not damage (Gravecold's frost
ring) carries its own pre-cue in the affix colour, no crimson decal; `DR5-arena`: EvArena uses the arena style.

**Hue-audit clean-up (DR5).** The rogue's `shadow` palette moves to a dusk violet-grey (#ECE6FA / #A898D8 / #5E5294 /
#1C1828, saturated slots under .5) and `skill.vanish`'s scorch follows; the poison / bleed puddles start nearer their
size (grow 1.2 / 1.22) and the meteor's incoming shadow grows 1.46×; `elite.fire_enhanced` loses its #FF4400.
`hue_audit.py` counts `death.<family>` as enemy and the outline sprites `Ripple` / `RaiseSigil` / `FlameBreach` /
`AshFlake` as never-filling. Result: fill OK, ember OK (recipes and art); violet fails only on
`hit.impact*` (35 hits) through `classes.json` `arcane_torrent.impactColor` #B07CFF (sat .514) — core-lane data
(`Tools/export-data/src/tables/skills.ts:146`, `arcane_torrent: 0xb07cff` → `0xb48cff`, sat .45). A simulated run on a
Data copy with #B48CFF passes every rule (VFX r5).

**Integration notes for the core lane** (also in the recipe JSON's `integration`): `deathFamily` (PlayFirst
death.<defId> → death.<family> → death.burst, played with Blow = the victim's facing so death offsets are the dying
actor's frame; the cinderbloat's own quiet `death.cinderbloat` keeps the family burst off its fuse), `ambienceTint` (play `ambient.<ambience>` with the weather tint),
`monsterLoops` (`monster.ash_flakes` on spawn for `fx.ashFlakes`), `eliteDeath`, `monsterAuras`, `telegraph`,
`ashDissolve`, `notifyPerMonster`
(`notify.FX_Roar` is shared with the goblin chief), `monsterProjectiles` (a per-monster projectile vfxId would let the
Corpseherd's toll fly as a ripple); the W2 events (EvMonsterRaised, EvBlock, EvHazard / EvArena, EvMonsterPhase, U3)
play the ids above. The `dark.bindings` section lists every id with its event and wave.

**Previews.** `fx_preview_dark.py` is `fx_preview.py`'s runtime port on the march: the shipped march terrain, the
march figures re-imported through `coherence/cohkit` (manifest attachments, `looks.weapon` such as the footman's
shovel, sockets resolved through the manifest) and posed by their exported clips — every bone copies the clip's own
armature in pose space, whose action runs on an NLA strip offset per actor (the clip FBXs carry a bind pose of their
own; playing their local channels on the SK armature turned A2's standard and shovel 90°), anchors from the posed
`fx_*` sockets, plus per-pixel emulations of `M_AF_Telegraph`, `M_AF_AshDissolve` (screen space over a plate without the
body, the noise tile sized to 60 cm at the camera) and A10's `MI_AF_Echo`. Offsets on bells, the banner, the sword tip
and the clapper come from `--measure` at the notify beats (VFX r5: re-measured on the 20:17–21:16 exports — FX_BellToll
→ (84, 34, 130), FX_bell_ring → (20, 29, 165), FX_Roar's banner (−98, 0, 177) → emitter (−94, 0, 190), Valdras' Death
droop path; the rest moved < 10 cm). Every scene directory is named after the recipe it validates
(`Previews/vfx_effects/<recipe id>/`, native 1080p, 1:1, the same 200 KB / strip rules), and
`Previews/vfx_effects/README_dark.md` (written by every full run and by `--index`) maps all 50 `dark.bindings` ids to
their directory and lists what each scene plays and which telegraph styles it emulates (`death.cinderbloat` is the
whole kill → fuse → burst chain; `elite.fire_enhanced.death` shows the elite style; `hazard.banner_ring` the arena
style; `hazard.breach_seep` the windup → hazard hand-over). Death scenes use the `death` event kind (Blow = the
victim's facing); `death.valdras` is killed from his side to prove the plume stays on the standard. Textures:
`Previews/vfx_textures_dark/`.

**Lessons (art review of this family).** On #3A332D mud, additive light alone vanishes: every ground ring carries a
translucent dark body under the additive line, and dust needs a pale grey (#77705F, not #4A463E) to read at all.
Thick rings stretched over metres read as UI circles — big radii use the hairline Ripple cell. Small breach flames
read as beads: fire needs density and height (hazard 54–90 cm at 24/s; the arena ring ~60 tongues alive). A radial
crack cell repeated along a line reads as starbursts — stretch it along the blow (aspect .32, `alignToBlow`).
Translucent fills blend in linear light: alpha .3 is already strong. A crumble driven by fine fBm is pixel speckle at
the game camera — whole plates must dominate. Particles spawned at a fixed height inside a body never draw; use the
body's own socket (`head` = the cinderbloat's crust top).

A death recipe's offsets live in the dying actor's frame (Blow = its facing): blow-frame offsets put Valdras' plume
in front of him on a kill from behind, and fixed blow-frame heights hang in the air once the standard droops — follow
the measured socket path in delayed layers. Smoke darker than the mud (#3E3833) vanishes from the game camera.
Particles spawned inside a slumped husk never draw: spawn the Consume grit at the gnawer's jaws.

**Open (not this lane).** ~~A5: the 21:10 `A_Mon_Cinderbloat_Death.fbx` keys the `core` bone at ×1.65 → ×2.41~~ — resolved by the Wave 1 coherence pass (§31): the FBX curves hold the authored 0.55 → 1.34; the ×1/0.55 came from Blender's FBX importer (a bone's static `Lcl Scaling` taken as a rest it cannot store), which `cohkit._import` now neutralises, and the death scenes above were re-rendered. ~~The manifest's `fx.fuse.burst.recipe` says `death.burst`~~ — now `notify.FX_Burst` (`fallback`: `death.burst`; `breach.fx_for` and the manifest entry, §31). Core lane: `arcane_torrent.impactColor` (above); the `deathFamily` contract (Blow = the victim's
facing, `death.cinderbloat`), `eliteDeath`, `monsterAuras`, the `arena` / hazard-window decal styles (U2) and the three
`dark.decisions` for DECISIONS DR5.

## 31. Wave 1 coherence pass — the march (`coherence/*`, `environment/lore_march.py`)

The final coherence pass of dark-remake Wave 1: every Chapter 1 asset of 焚境 · The Burning March re-imported from
`Art/Export` (what UE receives) and judged together at the W1 game camera on the shipped march terrain, the cross-family
rules measured, the gaps closed in the responsible scripts and re-shipped, and the manifest validated against every
Chapter 1 id of `monsters.md` / `chapter1.md`.

```bash
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/coherence/lineup.py --march   # ~7 min
#   [characters monsters camp camp_kit story ashwheat sheepfold_east breach_rim banners_stakes knellwatch foliage
#    rocks interactive] [--characters | --props]   (a partial run merges into lineup_metrics_march.json)
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/coherence/vignettes.py --march  # or knellwatch red_field ashwheat
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/coherence/darkgate.py --tile=dirt   # + grass stone camp, --ground-hex=…
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/lore_march.py [names…] [--quick | --family]
python3 unreal/Art/blender/vfx/hue_audit.py
python3 unreal/Art/blender/coherence/checklist.py          # → Art/Export/CHECKLIST.md (march mode: Data/maps.json presentationTheme)
```

| File | Change |
|---|---|
| `coherence/cohkit.py` | `set_theme` / `variant_names` (`<id>@march` first, as U1), `crown_asset` (hunts stand under `elite_crown@march`), the march ground (`terrain_march`); an attachment on a named socket that is not a bone maps to the socket's bone (Old Bern's `barrow` on `barrow_01`); **`_import` patches a clip file's static bone scales** (below) |
| `coherence/lineup.py` | `--march`: two character pages (`characters`: heroes, Lanternwisp, Valdras, the Corpseherd, the four Unburied / the 11 march NPCs, Bern side-on; `monsters`: the Living, Dreck and Harke, the drummer, gnawers, breach-blood, Hungerbell, the wisp's evolved looks) and 11 prop groups → `Previews/ch1_coherence_march/`, `coherence/lineup_metrics_march.json`; the key-light rule now reads the light pass (light-band centroid vs shade band, family median in the upper-left half-plane, outliers listed), the R2 re-import rule falls back to the bounding diagonal for re-posed bind poses (the standard-bearer); march rules: DR5 no enemy ember, violet only on enemies and breach-sourced props, warm light only on refuges / interaction / loot (`march_palettes.WARM_ALLOWED`), value hierarchy (actors above decor) |
| `coherence/vignettes.py` | `knellwatch` (the camp stamp with the pew palisade, `CampLayout.h` props themed, the U4 hearth dressing / bell frame / name wall, the five Camp 1 NPCs of `Data/maps.json` on slots 0-4, the party at the gate), `red_field` (the Red Banner, helmet stakes, the shield wall, the standard-bearer, gnawers at the mound, the warrior at contact), `ashwheat` (the ditch and burned bridge, three cottages, the mage and the Lanternwisp, gnawers in the wheat) |
| `coherence/darkgate.py` | the roster of a themed ground is the zone's own (`Data/monsters.json` byZone + mini-boss + hunts, resolved `@march`): 16 figures; `--zone=`, `--all` |
| `coherence/checklist.py` | march mode (the zone's `presentationTheme`): `@march` resolution, every `march_palettes.VARIANTS` slot must resolve to its themed asset, themed portraits, boss / mini-boss portraits, echo lore via `MI_AF_Echo`, U4 set pieces, elite markers, march terrain, monsters.md §5 budgets of the remake roster, the march lineup rules; NPC LOD0 under the §1.9 band is a NOTE (a performance budget) |
| `environment/lore_march.py` (new) | the five planned slots A10 left unbuilt: `SM_Prop_March_TornJournal` (`lore_torn_journal@march`), `SM_Prop_March_SealShard` (`lore_crystal_shard@march`), `SM_Prop_March_CatechismStone` (`lore_carved_stone@march`), `SM_Prop_March_HandbellPost` (`decor_event_puzzle_rune_pillar@march`), `SK_Prop_March_Strongbox` (`treasure_chest@march`, `decor_treasure_chest@march`; the plains chest rig and clips re-dressed, `A_Prop_March_Strongbox_{Idle,Open}`) |
| `environment/verify_march.py` | the lore family; skeletal assets checked as such (`check_skeletal`: one mesh + armature, bones, UVMap, AF data, clip frame counts) |
| `monsters/valdras.py` | `SM_MonWeapon_OathSword` inked with the boss hull (`finish_item_boss`: 1.8 cm / 3.5 px, as the chief's cleaver) instead of the monster item hull (1.5 cm) |
| `npcs/march_props.py` | the charred anvil's face `iron_face_worn` #868C95 (appended): the plains polish #C8D2E2 was the brightest value in the Knellwatch camp, a near-white block above the smith |
| `monsters/breach.py` | `fx.fuse.burst.recipe` → `notify.FX_Burst` (`fallback`: `death.burst`); the manifest entry patched in place (metadata only) |

**Re-imported clips with bone scale (`cohkit._import`).** Blender's FBX exporter writes a clip's first pose as the bone
nodes' static transform, so every bone keyed at a scale there (the slime family's X-eyes and lids at .05, the clot's
sparks, the cinderbloat's fuse ridges at .55 and its six burst shards at .001, the wisps' mouth and tail stretch, cloth
chains) carries a static `Lcl Scaling`. Blender's importer builds the rest from it and keys the pose relative to it,
but an edit bone cannot hold a scale: the scale is dropped from the rest while the keys stay divided by it. Pose space
came out `1 / static` too large — at frame 0 every hidden part was drawn at full size (the slime's X-eyes over its
eyes, the bloat's shards and ridges), and through the bloat's fuse its ridges grew ×1.82 off the crust (the A12 review's
"core keyed ×1.65 → ×2.41"; the FBX curves hold the authored 0.55 → 1.34) and its shards ×1000 after the burst. UE
evaluates the curves — absolute local transforms — against the skeleton's bind pose, so the assets were right and the
previews wrong (`mship`'s verify already compared the curves for this reason). `_import` now imports clip files from a
copy whose bone nodes' static scale is 1 (the curves untouched; deleted after the import): checked against a
composition of the raw FBX curves down the chain on the warrior's tabard, the bonegorger's loincloth and the bloat
(0.0 cm, scales equal). Everything built on `cohkit` was re-rendered with it: the darkgate grounds, the march character
pages, the Knellwatch and Ashwheat vignettes, the plains character lineup and vignettes, and A12's breach scenes
(`death.breach`, `death.cinderbloat`, `notify.FX_{Fuse,Burst,Land}`, `monster.ash_flakes`).

**Results (final run).**

* **Lineups** (`Previews/ch1_coherence_march/lineup_march_{characters,monsters}*.png`, 11 prop groups): every rule
  OK — §1.5 chroma hierarchy (p75 C*: heroes 31.5 ≥ NPCs 14.6; monsters 13.4 within the rule's 2-point tolerance of
  the NPCs — the dark remake's enemies carry their colour in emissive markers, not in albedo; heroes and interactive
  15.1 ≥ decor 9.0); measured ink hero 3.75 · boss 4.12 · monster 4.12 ·
  small monster 3.86 · NPC 3.88 · interactive 2.13 · decor 1.44 px; 35/35 figures keep a ≥ 3 px rim on ≥ 94.7 % of
  their edges; 99 meshes gated; key light from the upper left in every family (monsters 14/16: Valdras and the
  standard-bearer, whose banner planes turned from the key dominate their shade band, listed); FBX re-import scale
  34/34 (the standard-bearer by its bounding diagonal); DR5: no ember emissive on 19 hostile figures and markers, no
  breach-violet on 19 friendly figures, breach-violet props only the dais and the ichor, warm light only on the bell
  frame, the hearth, the hearthstone and the seal shard; value hierarchy (lit p90 luma heroes 190, NPCs 164, monsters
  138, pets 240 above decor 116).
* **Vignettes** (`vignette_{knellwatch,red_field,ashwheat}*.png` + 1080p crops): placed as the runtime does (camp
  stamp, `CampLayout.h`, Camp 1 NPC slots, `@march` ids); the set-piece positions (bell frame, hearth dressing, name
  wall, the Red Field shield wall, the Ashwheat bridge and cottages) are proposals for U4's `set_pieces.json`.
* **Darkgate** (16 figures: the Unburied four, the Living three + Dreck and Harke, the gnawers + Hungerbell, breach-blood
  two, the Corpseherd, Valdras; hunts under `elite_crown@march`): 16/16 PASS on every march material (dirt L* 19.5,
  grass 32.3, stone 32.4, camp 28.3) and on #3A332D / #4C4940; minimum rim 96.3 % (the zealot), minimum lit ΔL* 16.3
  (the breachclot on dirt, at the edge) and the Corpseherd's 2 px marker (at the edge everywhere). #141210 (L* 5.6,
  the near-black stress ground, no march material) 15/16: the breachclot's pitch body (L* 3.2) sits under the ground
  (ΔL* 2.5) — its rim (100 %) and marker (3 px) hold; the lever is the toon rim term, not the asset (R21 pending).
* **Hue audit**: fill OK, ember OK (recipes and 117 enemy-art colours); violet FAILS only on `hit.impact*` (35 hits)
  from `classes.mage.arcane_torrent.impactColor` #B07CFF — core-lane data (`Tools/export-data/src/tables/skills.ts:146`
  → `0xb48cff`, re-export).
* **Checklist** (`Art/Export/CHECKLIST.md`, march mode): 419 requirements — 418 OK, 1 NOTE (Valdras opts out of the
  elite marker), 0 GAP; coherence 0 GAP, 1 NOTE (`SK_NPC_Merchant_March` 7646 toon tris, under the §1.9 NPC band's
  8000 floor: a performance budget); every monsters.md §5 budget met (e.g. Valdras 10920 / 11000, LOD1 3930 / 4000).
* `environment/verify_march.py`: 64 march environment assets re-imported, 0 failures; kit smoke test PASS.

The plains:grass darkgate run in `darkgate_report.json` is the Phase-0 baseline (§18) and predates the clip-import fix;
it was not re-run (the slime's X-eyes and lids were drawn over its eye marker there).

**Handoffs.** Core lane: `arcane_torrent.impactColor` (above); re-export `Data/assets.json` from the manifest (it
lacks the five lore / event assets of `lore_march.py` and the re-shipped OathSword / anvil entries). U4: the set-piece
placements above.

## 32. Wave 2 art — dark-remake 2H (`monsters/clip_patch.py`, `corpse_husks.py`, `elite_sigil.py`, `vfx/fx_recipes_dark.py`, `vfx/ashnumb_band.py`)

Work package 2H (implementation-plan.md §5 / Phase 2): the art the Wave 2 core (K2-K6) drives — the clips monsters.md
assigns that Wave 1 shipped without, the elite sigil on the whole roster, the ash-husk corpse hooks U3 reads, and the
A12 W2 recipe gaps. Nothing of the shipped meshes changed (no mesh re-export): new clips go in with `clip_patch.py`.

```bash
/opt/venvs/blender/bin/python unreal/Art/blender/monsters/clip_patch.py <generator> <Clip> … [--keys notes,fx] \
    [--sockets fx_elite] [--replace Clip,…] [--dry-run]    # add clips to a shipped monster (below)
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/elite_sigil.py --solve-elite \
    [--only=FOOTMAN,…] [--cap=20|none] [--dry-run]         # fx_elite per asset -> fx_elite_w2.json + manifest
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/elite_sigil.py --roster [--scratch]
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/elite_sigil.py --over
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/corpse_husks.py [assets…] \
    [--no-previews] [--dry-run] [--scratch]                # manifest corpse blocks + Previews/corpse_husks/
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/oath_zealot.py --frenzy-read
python3 unreal/Art/blender/vfx/fx_recipes.py && python3 unreal/Art/blender/vfx/fx_validate.py
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/vfx/fx_preview_dark.py wave2
EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/vfx/ashnumb_band.py   # the band mock
```

**Clip audit** (monsters.md §2 / chapter1.md §10-11 against the manifest; contact / release in ms at play rate 1):

| Monster | Wave 2 mechanic | Clip | Beat | State |
|---|---|---|---|---|
| `breachclot` | Ashnumb on hit, tar puddle | — (Death melts) | — | nothing to add |
| `cinderbloat` | on-death burst (fuse ≥ 800) | Death | FX_Fuse 0, FX_Burst 800 | W1 |
| `risen_footman` / `risen_arbalest` / `risen_shieldbearer` | the Unburied rise; kite (Walk); frontal block (`monster.block` at socket `face`) | Rise | RiseUp 840 (1200) | W1 |
| `dead_standard_bearer` | Muster, aura | Cast01 / Rise | Plant 450, Release 900 / RiseUp 840 | W1 |
| `oath_zealot` | frenzy (K4 one-row phase, enterAction Roar) | **Roar** 1200 | **FX_Frenzy 480** (no Contact) | **2H** |
| `deserter` | morale flee | Walk at a run rate (manifest notes) | — | W1 |
| `deserter_drummer` | the drum call (2 s channel) | Cast01 loop 1000 | FX_DrumBeat 125 / 375 / 625 / 875, Release 125 | W1 |
| `gravegnawer` | Consume (2 s channel on a corpse) | **Consume** loop 1000 | **Bite 330, Bite_2 830** | **2H** |
| `bonegorger` / Hungerbell | Clapper Slam (+ the call) | Attack02 1400 | WindupEnd 496, Contact 800, FX_BellRing 800 | W1 |
| `miniboss_corpseherd` | Call the Flock | Cast01 1300 | FX_bell_ring = Release 650 | W1 |
| | Keen (stretch, below 50 %: r4, 800 ms) | **Cast02** 1600 | **WindupEnd 496, Contact = Release = FX_Keen 800** | **2H** |
| | an Unburied rise (her data family) | **Rise** 1400 | **RiseUp 980** | **2H** |
| `valdras` | P1 Banner Sweep / Oath-Brand, P2-P3 Roar, P3 Lunge, combo | Attack02 700 / Cast01 900 (FX_Brand) / Roar (FX_Roar 450) / Lunge 800 / Attack01 650 + Contact2 1000 | as listed | W1 (Breach-seep, Feed, the banner ring are hazards / VFX: no clip) |

* **Names.** The Keen is `Cast02` (K1's AnimAction Cast02; UE 1C already plays the clip of that name with its Release
  beat); Consume has no AnimAction yet — the core emits it as an explicit clip (`EvPlayAnim.clip`, the hero signature
  path) or U3 plays it by name, like Valdras' `Lunge`. Every new attack-like clip carries both Contact and Release.
* **`clip_patch.py`.** Blender's FBX exporter stamps each file with its creation time and the builders do not keep a
  stable vertex order, so a full re-ship rewrites every FBX of an asset. The tool ships the generator into a scratch
  root (QA, ink gate, re-import verify as usual), proves that every shipped clip is unchanged (curve by curve, every
  key; the clip entries, bones, triangle counts and palette registries too) and only then copies the new clip FBXs, the
  requested entry keys / sockets and the new clips' previews (`anim_<Clip>`, `anim_back_<Clip>`, `game_w2` from
  `mship.w2_sheet`). All three patches: every shipped clip SAME (0 delta), QA PASS on every limited pair (the gnawer's left knee
  first went 1.4 cm into the ground in a deeper crouch: raised), ink gate PASS.
* **The zealot** also got `fx.frenzy` (the scar emissive .25 → .7 at FX_Frenzy, the frenzy loop). Its `fx_overhead`
  is the kit's again (art review r1: the first pass raised it 10 cm for the sigil; the sigil now has `fx_elite`).
  `--replace` (art review r1): Roar went back in alone after its brace was eased (below) — every other clip SAME.

**Elite sigil over the roster** (`Previews/SM_FX_EliteSigil/sigil_over_roster_*.png` + `.json`; Idle F0 at the W1
camera, native 1080p, the four diagonals, hunt leaders at ×1.25). A8 measured five monsters in se; the whole roster
failed on 11 of 15 figures on the kit's `fx_overhead` (crown + 25 cm on the head bone: it tilts with the hunch, so in
the back views the hoop's near rim lands on the helm / hood; the clots' dome; the standard-bearer's banner). The sigil
rides its own socket, **`fx_elite`** (art review r1: the first pass moved `fx_overhead` itself on 10 assets, but UE
hangs much more there — nameplates and floating text, `status.stun` stars, the `skill.death_mark` skull, the W1 crown
— so the standard-bearer's nameplate and stun stars went 1.5 m over his helm and the hunched figures' stun stars
stopped following the head; `fx_overhead` is back to Wave 1 on every asset, byte for byte). `--solve-elite` walks
candidates around the W1 socket's posed position (on `root`, level and steady, or on its own bone; offsets left /
forward / up, cheapest by a lateral-heavy distance first) and takes the first that passes both gates for the asset and
all its variants:

* **clear** — ≥ 4 px (solved to 5) between the sigil (ink included) and the figure in se / sw / ne / nw at the
  **trough of the bob** (`fx.motion` `bob: symmetric`: a sine of ±`bobCm` 3 cm about the rest; at ~0.9 px/cm the
  r0 "5 px at rest" cases were 2-3 px at the trough);
* **close** — ≤ 20 px at rest in se / sw (art review r1: the root sockets left the shieldbearer's, the Corpseherd's and
  Hungerbell's sigil 33-99 px off the head, shifted sideways; in a crowd that reads as a neighbour's marker).

Result (`monsters/fx_elite_w2.json`, `mship.ship` writes it on every re-ship; the manifest sockets — UE builds them
from `relLocCm`, the shipped SK FBX stand): all on `root`; footman 8 cm right, 16 forward, 16 up of the W1 socket;
arbalester 8 right, 8 fwd, 20 up; shieldbearer 16 right, 32 fwd, 32 up (the shield's rim); zealot, deserters and
drummer straight up 12-16 cm; gnawer 24 fwd; bonegorger 32 fwd, 4 up; breachclot / cinderbloat 32 / 36 up; the
Corpseherd 24 right, 8 fwd, 36 up (her crook); the standard-bearer 45 cm to his left (off the pole) and 17 forward at 2.46 m (his crown 1.78 m): no candidate over the helm clears the banner in every facing, so the sigil floats over the banner's free side instead of crowning the finial (3.54 m, the first pass); his pick is the passing candidate nearest the head on screen in se / sw (`ELITE_CLOSEST`, the whole grid walked: 78 px; opened down to the helm's height the nearest one floats beside the helm 69 cm out, which in the Muster reads as a footman's — rejected). Measured (rest / trough): worst trough 5 px
(zealot se / sw), widest se / sw gap 19.6 px (Hungerbell se, was 99), the shieldbearer se 10.4 (was 35), the
Corpseherd se 18 (was 33.5); 15 / 15 figures pass both gates. `sigil_over_monsters` / `_gnawers_facings` show the
sigil on `fx_elite`; the crown sheets are the committed W1 ones (re-rendered on the restored sockets: identical within
render noise, kept). Manifest `SM_FX_EliteSigil`: `attachSocket` / `fx.socket` = `fx_elite` (`socketNote`: fall back to
`fx_overhead` on an asset without one), `fx.motion.bob`, `clearance` from the report. The swap crown → sigil itself is
UE's (2G): `elite_crown@march` still names `SM_FX_EliteCrown_March` on `fx_overhead` (PresentationTests `theme_test`
pins it; `UpdateElite` attaches statically, no `fx.motion`).

**Corpse hooks** (manifest `corpse` on the 13 roster assets; `corpse_husks.py`; previews `Previews/corpse_husks/
<asset>/`: the Death end +16 ms, the grey done +830, the crumble +1280 with `corpse.crumble`, the husk +2130). The block
is presentation only: `look` (`husk` — Unburied, Living, the Corpseherd, Valdras; `carcass` — the gnawers, same ash
timeline, `consumable` / `raisable` false; `puddle` — breach-blood, `registry` false), `deathClip` / `holdFromMs`,
`timeline` (the `M_AF_AshDissolve` numbers by reference; `holdUntil: "story"` for the Corpseherd's `cs_rf_herd` and
Valdras' `cs_rf_marshal_falls`), `husk` measured on the shipped FBX at the Death clip's last frame (actor frame:
forward, right, up cm; `centerCm`, `boundsCm`, `radiusCm`, `topCm`, `anchors` chest / head / pelvis), `rise` (own Rise
clip + RiseUp: the **self-rise only**, the Unburied trait / affix), `raisedAs: monster_risen_footman` on every husk (what
a raise by another monster stands up, monsters.md §2.0 — the Unburied with their own Rise too; art review r1: U3 could
not tell a third-party raise from the self-rise), `raisable` / `consumable` false on the carcasses and on the two story
bodies (`storyGuard`: the Corpseherd and Valdras are never eaten or raised by another monster, permanently),
`recipes`, `items` (held items dissolve with the body), `shadow`. Measured: the Unburied fold ~1-1.4 m tall (the shieldbearer's shield on its edge 137 cm, the
standard-bearer's planted standard 315 cm), the Living lie flat 0.9 m ahead of the root (top 40-104 cm), the carcasses
59 / 127 cm, the puddles ≤ 12 cm.

**A12 W2 recipes** (`fx_recipes_dark.wave2`; bindings in the JSON's `dark` section; previews in
`Previews/vfx_effects/<id>/`): `notify.FX_Frenzy` (the chant: a pale breath out of the hood, dust shoved off his feet,
ash off the robe — the Living carry no light), `monster.oath_zealot.frenzy` (the phase loop: panting breath, dust),
`notify.FX_Keen` (violet sound rings before the veil, a hairline ripple sweeping to r 4 in 220 ms and flaring at the
rim, ash blown outward; U2's crimson ring fills to 800), `status.ashnumb` (ash flaking off the hero's limbs, dust at the
feet; the per-stack grey band is the `dark.materials.AshnumbTint` contract — never violet, never a ground fill, mocked
on the heroes in `Previews/vfx_effects/AshnumbTint/`) and `corpse.consume` re-timed for the real
Consume clip (the jaws at 45-52 cm, 6-25 cm up: the grit spawns at 36 cm forward, 30-34 cm up). Lesson: a rate emitter
without `durationMs: -1` stops at once; a status loop previewed with a zero blow loses its camera bias (play it as a
blow-less `self` event, as the runtime's +X frame) and its flakes hide inside the body.

**Art review round 1** (the nine points, as resolved):

1. `fx_overhead` moved on 10 assets → **restored** to Wave 1 on all of them; the sigil has `fx_elite` (above). No
   `fx_overhead` move is kept, so no stun / death-mark / nameplate re-check is needed.
2. Bob trough → gated: clearance measured at rest − 3 cm (`fx.motion.bob: symmetric`), 5 px worst.
3. Zealot frenzy read → `oath_zealot.py --frenzy-read` (`Previews/mon_oath_zealot/game_w2_frenzy*.png`: darkgate's
   lineup on march dirt, 1080p, bloom, the darkgate marker metric on the scar swatch): at `scarEmissive` .7 the scar is a
   **1 px** block (3-11 px of it) in Idle / Walk / Attack01 se / sw — 2 px only in Walk sw. So the breath carries it:
   `monster.oath_zealot.frenzy` pants every ~0.3 s (rate 3.4, puffs 24-34 cm, alpha .62, 0.7-0.95 s) — a pale cloud
   always before the hood (`Previews/vfx_effects/monster.oath_zealot.frenzy/`); no glow. `fx.frenzy.read` says so.
4. Ashnumb → `vfx/ashnumb_band.py` mocks `AshnumbTint` as material nodes (height-fraction band, `Strength` .85 to
   AshColor / AshShade by the toon's lit / shade split, the EdgeColor line) on the warrior, mage and rogue at 0 / 1 / 3 /
   5 stacks, march dirt, 1080p (`Previews/vfx_effects/AshnumbTint/band_stacks*.png`). The r0 top (stacks × .12) made
   one stack the boots alone, hidden under the warrior's shield at the W1 camera: now `BandTopBase` .12 + .10 / stack
   (mid-shin, thighs, hips at 1 / 3 / 5). Reads clearly on the mage's robe and the rogue's leather; on the warrior's
   pale steel the ash reads as a darker, warmer band from 3 stacks (1 stack barely shows there: the flakes carry it).
   Flakes: 18-28 cm (were 12-16), two tones (pale + `#4C4944`, which reads on steel), spawned on a 40-42 cm ring (the
   30 cm ring put most inside or behind the body), rate 20.
5. Keen ripple → the ground ripple sweeps r 0.4 → 4 m in **220 ms** after contact and flares at the rim for 480 ms
   (was 700 ms linear: a hero at the rim saw it ~650 ms after the slow); the air rings at the veil stay slower.
6. Corpse blocks → `storyGuard` + `raisable` / `consumable` false on the Corpseherd and Valdras; `raisedAs` on every
   husk; `rise.use` = the self-rise only.
7. Roar brace → pelvis 3 cm down (was 8), lean 14 (22), hip_pitch 0 (6), flow .12 (.25): the robe stays a cone
   (`game_w2.png` Roar F13 SE, `anim_back_Roar.png` BACK F13); `clip_patch.py --replace Roar`, every other clip SAME.
8. Sigil offsets → the close gate (≤ 20 px in se / sw) and the lateral-heavy cost (above).
9. Wave 1 darkgate previews → re-run on the restored sockets: **pixel-identical** to the committed
   `Previews/ch1_dark/darkgate_march_dirt_p*.png` and the same report numbers (16 / 16 PASS), so the hunts' crowns in
   them match the shipped `fx_overhead` again.

**Checks (2H, after review round 1).** kit smoke PASS (62 ok); `fx_validate` 185 recipes, 0 / 0; hue audit: fill OK,
ember OK on 51 enemy recipes and 117 art colours, violet fails only on the core-lane `arcane_torrent.impactColor`
(as before); checklist 419 OK / NOTE, 0 GAP (CHECKLIST.md unchanged); `verify_march.py` 64 / 64; darkgate on march dirt
16 / 16 PASS, previews and numbers unchanged; `Scripts/build_content.py --plan --strict` OK, 317 animations, 0
problems; the zealot's scratch ship: QA within limits on Roar, ink gate PASS (32 cells, min rim 91.9 %), re-import
verify PASS.

**Handoffs.** Data: re-export (`node unreal/Tools/export-data/run.mjs`: only `Data/assets.json` + `Data/index.json`
change, from the manifest; the core reads clip contacts there). Core (2A-2E): ability `clip` names and beats above
(Keen = `Cast02` 800, Clapper Slam = `Attack02` 800, Banner Sweep `Attack02` 700, Oath-Brand `Cast01` 900, Lunge
`Lunge` 800, Call the Flock `Cast01` 650, Muster `Cast01` 900, the frenzy `Roar`, Consume `Consume` × 2 loops), an
AnimAction or explicit clip for `Consume` / `Lunge`; the corpse guards (`raisable` / `consumable` / `raisedAs`) mirror
K3's rules. UE (2G): the sigil swap on **`fx_elite`** (fallback `fx_overhead`; `fx.motion` with the symmetric bob;
absolute scale), `fx.frenzy` (+ its `read`), `AshnumbTint` (new `BandTopBase`), the corpse blocks (U3),
`corpse.consume` once per channel.

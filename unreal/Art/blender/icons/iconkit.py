"""Icon / portrait kit (spec art-inventory-ch1.md §9, DECISIONS R10 / R11): Blender renders of the 3D models in
the M_AF_Toon look, finished with the **web icon language** (``src/graphics/icons/IconKit.ts`` ``inked()``):

* authored in a **96-unit square** (``UNIT``), rendered at 256² (items, skills), 128² / 64² glyph sizes on demand;
* the same toon maths as the game (``kit.shading`` AF_ToonCore, palette atlases) under an **orthographic studio
  camera** with the game's view-space key light — from the screen upper-left, toward the viewer
  (right −0.57, up +0.53, toward viewer +0.63; README §2), so every icon is lit like the world;
* **coloured line art** in screen space (no hull: a 1.3 cm hull is a third of a 6 cm coin at icon scale) — the
  web strokes every shape in its own ``tone().line`` colour: here a line where a nearer surface overlaps a farther
  one (``LINE_U``, the near part's line tone) and a thinner two-sided line where two palette regions of different
  colour meet on the same surface (``SEAM_U``); emissive regions (glints, glows) get none;
* the web post: a warm **rim** ``rgba(255,240,210,.45)`` 1.1 u on the upper-left edge, the dark **ink silhouette**
  ``#140C1C`` 2 u (2.1 skill motifs, 3.6 quest markers), a soft **drop shadow** (black .55, blur 3 u, offset
  1.5 u / 2.5 u), optional glows under and sparkles over (one focal accent);
* **transparent background, no UI frame** (Slate draws slot / quality frames, R10).

Every icon is measured (``gate``): ink ≥ (ink − 0.5 u) on ≥ 98 % of the outer silhouette, ink pixels lum ≤ 40,
nothing clipped (empty border), the body fills 45–92 % of the square. Previews: per-category atlases at 1:1 and
0.5× (``atlas``) and a UI-size sheet (48 / 36 px on dark slots with the quality frames, ``ui_sheet``).

Files: ``Art/Export/Icons/<Group>/T_UI_<Kind>_<id>.png`` (the content build scans ``Export/Icons``), emblems in
``Art/Export/Portraits``; ``manifest.json → ui`` (merged per texture name by ``IconManifest``).
"""
from __future__ import annotations

import json
import math
import os
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Iterable, Sequence

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

import kit  # noqa: E402,F401
from kit import color, export, paths, pngio, review, scene, shading  # noqa: E402
from kit import mesh as M  # noqa: E402
from kit.palette import GRID, Palette, Region  # noqa: E402

# ── constants (web IconKit.ts) ──────────────────────────────────────────────────────────────────────────
UNIT = 96.0
ICON_PX = 256
SS = 2                                   # supersampling of every pass (lines and silhouettes anti-aliased)
INK_HEX = "#140C1C"                      # IconKit inked(): ink '#140c1c'
INK_U = 2.0                              # item icons; skill motifs 2.1; quest markers 3.6
RIM_RGBA = (255, 240, 210, 0.45)         # rim light on the upper-left edge
RIM_U = 1.1
SHADOW_ALPHA = 0.55                      # ctx.shadowColor rgba(0,0,0,.55)
SHADOW_BLUR_U = 3.0                      # shadowBlur 3u (canvas: sigma = blur / 2)
SHADOW_OFF_U = (1.5, 2.5)
LINE_U = 1.15                            # overlap line (nearer part's line tone), ≈ the web stroke 1.4u half-covered
SEAM_U = 0.5                             # each side of a region seam (total ≈ 1 u)
LINE_INK_MIX = 0.18                      # line colour = mix(line tone, ink, .18)
STUDIO_DIST = 20.0

# key light in the camera's view space (README §2: the game key light seen from the W1 camera)
L_VIEW = Vector((-0.57, 0.53, 0.63)).normalized()        # (right, up, toward viewer)


def view_to_world(v: Sequence[float]) -> Vector:
    """Studio camera looks along +Y with +Z up: right = +X, up = +Z, toward the viewer = −Y."""
    return Vector((v[0], -v[2], v[1]))


L_ICON = view_to_world(L_VIEW).normalized()

EXPORT_ICONS = paths.export_root() / "Icons"
EXPORT_PORTRAITS = paths.export_root() / "Portraits"
SCRATCH = Path(os.environ.get("AF_ICON_SCRATCH",
                              "/tmp/claude-0/-home-user-abyssfire/b3225f99-25a3-5b78-996f-643be8685889/scratchpad/ic"))
ICON_PAL_DIR = SCRATCH / "palettes"       # icon-only models: their palettes never ship (regenerated each run)

QUALITY = {   # slot frames for the UI-size sheet only (Slate draws them in game; canonical palette spec §1.1)
    "normal": "#C8C8C8", "magic": "#4F8CFF", "rare": "#FFD84A", "legendary": "#FF8A2A", "set": "#3ECF6A",
}


# ── palettes for icon-only models ───────────────────────────────────────────────────────────────────────
class IconPalette(Palette):
    """A palette for models that exist only to be rendered as icons: same swatch registry / atlas code as the
    shipped families, but registry and atlases live in the scratch dir (nothing of it ships; regenerated from
    code on every run, so it starts empty and stays deterministic)."""

    def __init__(self, name: str):
        self._icon_name = name
        reg = ICON_PAL_DIR / f"Icons_{name}.json"
        if reg.exists():
            reg.unlink()
        super().__init__("Test")                      # passes the family check; every path is overridden below
        self.family = f"Icons_{name}"

    @property
    def registry_path(self) -> Path:
        return ICON_PAL_DIR / f"Icons_{self._icon_name}.json"

    def texture_names(self) -> tuple[str, str]:
        return (f"T_AF_Palette_Icons_{self._icon_name}_BC", f"T_AF_Palette_Icons_{self._icon_name}_P")

    def texture_paths(self) -> tuple[Path, Path]:
        bc, p = self.texture_names()
        return ICON_PAL_DIR / f"{bc}.png", ICON_PAL_DIR / f"{p}.png"

    @property
    def material_name(self) -> str:
        return f"MI_AF_Toon_Icons_{self._icon_name}"


class ShippedPalette:
    """A committed atlas (``Weapons_T3``, ``Pickups``, …) used read-only for preview materials."""

    def __init__(self, family: str):
        self.family = family

    @property
    def material_name(self) -> str:
        return f"MI_AF_Toon_{self.family}"

    def texture_names(self) -> tuple[str, str]:
        return f"T_AF_Palette_{self.family}_BC", f"T_AF_Palette_{self.family}_P"

    def texture_paths(self) -> tuple[Path, Path]:
        bc, p = self.texture_names()
        return paths.texture_dir() / f"{bc}.png", paths.texture_dir() / f"{p}.png"

    def save(self) -> tuple[Path, Path]:
        return self.texture_paths()


_ATLAS_CACHE: dict[str, tuple[np.ndarray, np.ndarray]] = {}


def atlas_arrays(pal) -> tuple[np.ndarray, np.ndarray]:
    """(BC HxWx3, P HxWx4) uint8 of a palette (read back from its PNGs)."""
    key = pal.texture_names()[0]
    if key not in _ATLAS_CACHE:
        pbc, pp = pal.save() if isinstance(pal, Palette) else pal.texture_paths()
        _ATLAS_CACHE[key] = (pngio.read_png(pbc)[:, :, :3], pngio.read_png(pp))
    return _ATLAS_CACHE[key]


# ── studio ──────────────────────────────────────────────────────────────────────────────────────────────
def studio() -> bpy.types.Object:
    """Empty scene, the toon core lit by the icon key light, an orthographic camera looking along +Y."""
    scene.reset()
    _ATLAS_CACHE.clear()
    shading.build_toon_group(L_ICON)
    shading.build_outline_group()
    cam = bpy.data.objects.new("AF_IconCam", bpy.data.cameras.new("AF_IconCam"))
    scene.link(cam)
    bpy.context.scene.camera = cam
    cam.data.type = "ORTHO"
    cam.data.clip_start = 0.01
    cam.data.clip_end = 200.0
    cam.rotation_euler = (math.radians(90.0), 0.0, 0.0)
    cam.location = (0.0, -STUDIO_DIST, 0.0)
    return cam


def clear_objects(keep: Sequence[bpy.types.Object] = ()) -> None:
    keep = set(keep) | {bpy.context.scene.camera}
    for o in list(bpy.data.objects):
        if o not in keep:
            me = o.data if o.type == "MESH" else None
            bpy.data.objects.remove(o, do_unlink=True)
            if me is not None and me.users == 0:
                bpy.data.meshes.remove(me)


def _transparent_material() -> bpy.types.Material:
    m = bpy.data.materials.get("AF_Icon_Hide")
    if m is None:
        m = bpy.data.materials.new("AF_Icon_Hide")
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.clear()
        out = nt.nodes.new("ShaderNodeOutputMaterial")
        nt.links.new(nt.nodes.new("ShaderNodeBsdfTransparent").outputs[0], out.inputs["Surface"])
    return m


def set_look(obj: bpy.types.Object, pal) -> bpy.types.Object:
    """Slot 0 = the toon preview material of ``pal`` (Palette / IconPalette / ShippedPalette), every other slot
    (the baked hull) hidden: icons draw their ink in screen space."""
    mt = shading.toon_material(pal)
    mats = obj.data.materials
    if len(mats) == 0:
        mats.append(mt)
    else:
        mats[0] = mt
    for i in range(1, len(mats)):
        mats[i] = _transparent_material()
    obj["af_icon_pal"] = pal.family
    _PALS[pal.family] = pal
    return obj


_PALS: dict[str, object] = {}


def import_fbx(category: str, asset: str, pal=None) -> bpy.types.Object:
    """A shipped static mesh (what UE receives) with ``pal`` (default: its manifest palette)."""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(paths.category_dir(category) / f"{asset}.fbx"))
    new = [o for o in bpy.data.objects if o not in before]
    mesh = next(o for o in new if o.type == "MESH")
    for o in new:
        if o is not mesh:
            bpy.data.objects.remove(o, do_unlink=True)
    mesh.parent = None
    mesh.matrix_world = Matrix.Identity(4)
    mesh.name = asset
    if pal is None:
        pal = ShippedPalette(manifest_palette(asset))
    set_look(mesh, pal)
    return mesh


def fx_tint_material(tint_hex: str, s: float = 0.42, l: float = 0.40, e: float = 0.0) -> bpy.types.Material:
    """Toon material for an imported FX mesh (``M_AF_FX_Mesh`` look: one tint, the value channel ``AF_Data.a``
    darkens toward the ink) lit by the icon key light through the shared toon core."""
    name = f"AF_IconFx_{tint_hex}_{s:.2f}_{l:.2f}_{e:.2f}"
    m = bpy.data.materials.get(name)
    if m is not None:
        return m
    shading.ensure_groups()
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    b = shading.NB(nt)
    attr = b.node("ShaderNodeAttribute", attribute_type="GEOMETRY", attribute_name="AF_Data")
    mixn = b.node("ShaderNodeMix", data_type="VECTOR", clamp_factor=True)
    b.l.new(attr.outputs["Alpha"], mixn.inputs[0])
    ink = color.hex_to_rgb(INK_HEX)
    tint = color.hex_to_rgb(tint_hex)
    mixn.inputs[4].default_value = tuple(c / 255.0 for c in ink)
    mixn.inputs[5].default_value = tuple(c / 255.0 for c in tint)
    grp = b.node("ShaderNodeGroup", node_tree=bpy.data.node_groups[shading.TOON_GROUP])
    b.l.new(mixn.outputs[1], grp.inputs["Base"])
    grp.inputs["Params"].default_value = (s, l, e)
    grp.inputs["Grounding"].default_value = 0.0
    em = b.node("ShaderNodeEmission")
    b.l.new(grp.outputs["Color"], em.inputs["Color"])
    out = b.node("ShaderNodeOutputMaterial")
    b.l.new(em.outputs[0], out.inputs["Surface"])
    return m


def import_fx(asset: str, tint_hex: str, s: float = 0.42, l: float = 0.40, e: float = 0.0) -> bpy.types.Object:
    """A shipped FX mesh (``Export/VFX``) in a tinted toon look, its hull slot hidden."""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(paths.category_dir("VFX") / f"{asset}.fbx"))
    new = [o for o in bpy.data.objects if o not in before]
    mesh = next(o for o in new if o.type == "MESH")
    for o in new:
        if o is not mesh:
            bpy.data.objects.remove(o, do_unlink=True)
    mesh.parent = None
    mesh.matrix_world = Matrix.Identity(4)
    mats = mesh.data.materials
    mats[0] = fx_tint_material(tint_hex, s, l, e)
    for i in range(1, len(mats)):
        mats[i] = _transparent_material()
    return mesh


_MANIFEST: dict | None = None


def manifest() -> dict:
    global _MANIFEST
    if _MANIFEST is None:
        _MANIFEST = json.loads(paths.manifest_path().read_text())
    return _MANIFEST


def manifest_palette(asset: str) -> str:
    e = manifest()["assets"].get(asset, {})
    for s in e.get("materialSlots", []):
        if s.get("index") == 0 and s.get("palette"):
            return s["palette"]
    raise KeyError(f"{asset}: no palette in the manifest")


def place(obj: bpy.types.Object, toward: Sequence[float] = (0, -1, 0), up: Sequence[float] = (0, 0, 1),
          right: Sequence[float] | None = None, roll: float = 0.0, yaw: float = 0.0, pitch: float = 0.0,
          at: Sequence[float] = (0, 0, 0), scale: float = 1.0, spin: float = 0.0) -> bpy.types.Object:
    """Orient ``obj`` for the studio camera: the local axis ``toward`` faces the viewer, the local axis ``up``
    points up the screen (then ``roll`` deg clockwise in the screen plane: 45 = up-right diagonal); ``yaw`` /
    ``pitch`` turn it about the screen's vertical / horizontal axis afterwards (degrees, + = right side / top
    toward the viewer) to show depth; ``spin`` first turns the object about its own ``up`` axis (a blade's
    facets toward the light). ``at`` = world position of the local origin."""
    t = Vector(toward).normalized()
    u = Vector(up)
    u = (u - t * u.dot(t)).normalized()
    r = u.cross(t) if right is None else Vector(right).normalized()
    # local basis (r, u, t) → world (+X, +Z, −Y)
    loc = Matrix((r, u, t)).transposed()          # columns r, u, t (local vectors)
    world = Matrix((Vector((1, 0, 0)), Vector((0, 0, 1)), Vector((0, -1, 0)))).transposed()
    rot = world @ loc.inverted()
    if spin:
        rot = rot @ Matrix.Rotation(math.radians(spin), 3, Vector(up).normalized())
    if roll:
        rot = Matrix.Rotation(math.radians(roll), 3, "Y") @ rot
    if yaw:
        rot = Matrix.Rotation(math.radians(-yaw), 3, "Z") @ rot
    if pitch:
        rot = Matrix.Rotation(math.radians(pitch), 3, "X") @ rot
    m = rot.to_4x4() * 1.0
    m = Matrix.Translation(Vector(at)) @ m @ Matrix.Scale(scale, 4)
    obj.matrix_world = m
    return obj


def screen_xy(cam: bpy.types.Object, p: Sequence[float], size: int = ICON_PX) -> tuple[float, float]:
    """World point → icon pixel (x right, y down) for the current framing."""
    co = review.project(cam, Vector(p))
    return co[0] * size, (1.0 - co[1]) * size


def mesh_points(objs: Sequence[bpy.types.Object]) -> np.ndarray:
    out = []
    dg = bpy.context.evaluated_depsgraph_get()
    for o in objs:
        if o.type != "MESH" or o.hide_render:
            continue
        ev = o.evaluated_get(dg)
        me = ev.to_mesh()
        n = len(me.vertices)
        co = np.empty(n * 3, np.float32)
        me.vertices.foreach_get("co", co)
        co = co.reshape(n, 3)
        # only vertices of visible (slot 0) faces
        mi = np.empty(len(me.polygons), np.int32)
        me.polygons.foreach_get("material_index", mi)
        if (mi != 0).any():
            keep = np.zeros(n, bool)
            for p in me.polygons:
                if p.material_index == 0:
                    keep[list(p.vertices)] = True
            co = co[keep]
        ev.to_mesh_clear()
        mw = np.array(o.matrix_world, np.float64)
        co = co @ mw[:3, :3].T + mw[:3, 3]
        out.append(co)
    return np.concatenate(out) if out else np.zeros((0, 3))


@dataclass
class Frame:
    """Ortho framing: ``world_per_unit`` (m per icon unit) and the world point at the icon centre."""
    world_per_unit: float
    cx: float
    cz: float


def fit(objs: Sequence[bpy.types.Object], fill: float = 0.80, center_u: tuple[float, float] = (47.5, 47.0),
        world_per_unit: float | None = None, anchor: str = "center") -> Frame:
    """Frame ``objs``: the larger side of their screen bounds spans ``fill`` × 96 u (or a fixed
    ``world_per_unit`` for families whose sizes must compare: gem tiers, potion sizes), its centre at
    ``center_u`` (icon units from the top-left; default a hair up-left of the middle: the drop shadow falls
    down-right). ``anchor`` 'bottom' keeps the bottom at the same place (center_u y = the bottom)."""
    pts = mesh_points(objs)
    lo, hi = pts.min(0), pts.max(0)
    w, h = hi[0] - lo[0], hi[2] - lo[2]
    wpu = world_per_unit if world_per_unit is not None else max(w, h) / (fill * UNIT)
    cx = (lo[0] + hi[0]) / 2.0 - (center_u[0] - UNIT / 2) * wpu
    if anchor == "bottom":
        cz = lo[2] + (center_u[1] - UNIT / 2) * wpu
    else:
        cz = (lo[2] + hi[2]) / 2.0 + (center_u[1] - UNIT / 2) * wpu
    return Frame(wpu, cx, cz)


def apply_frame(cam: bpy.types.Object, fr: Frame) -> None:
    cam.data.ortho_scale = fr.world_per_unit * UNIT
    cam.location = (fr.cx, -STUDIO_DIST, fr.cz)


# ── passes ──────────────────────────────────────────────────────────────────────────────────────────────
def _id_material(k: int, uv: bool) -> bpy.types.Material:
    """Emission = (u, v, k/64) of the palette UVs (the swatch) — or (AF_Data.a, 0, k/64) for meshes without a
    palette (FX meshes: value channel) — rendered raw to EXR."""
    name = f"AF_Icon_ID_{k}_{int(uv)}"
    m = bpy.data.materials.get(name)
    if m is not None:
        return m
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    em = nt.nodes.new("ShaderNodeEmission")
    comb = nt.nodes.new("ShaderNodeCombineXYZ")
    comb.inputs[2].default_value = (k + 1) / 64.0
    if uv:
        src = nt.nodes.new("ShaderNodeUVMap")
        src.uv_map = "UVMap"
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(src.outputs["UV"], sep.inputs[0])
        nt.links.new(sep.outputs[0], comb.inputs[0])
        nt.links.new(sep.outputs[1], comb.inputs[1])
    else:
        attr = nt.nodes.new("ShaderNodeAttribute")
        attr.attribute_type = "GEOMETRY"
        attr.attribute_name = "AF_Data"
        nt.links.new(attr.outputs["Alpha"], comb.inputs[0])
    nt.links.new(comb.outputs[0], em.inputs["Color"])
    nt.links.new(em.outputs[0], out.inputs["Surface"])
    return m


class _PerObjectSlots:
    """Swap slot 0 of each mesh to ``fn(index, obj)`` and every other slot to the hidden material; restore."""

    def __init__(self, meshes, fn):
        self.meshes, self.fn, self.saved = meshes, fn, []

    def __enter__(self):
        hide = _transparent_material()
        for i, m in enumerate(self.meshes):
            mats = m.data.materials
            self.saved.append([mats[j] for j in range(len(mats))])
            mats[0] = self.fn(i, m)
            for j in range(1, len(mats)):
                mats[j] = hide
        return self

    def __exit__(self, *exc):
        for m, old in zip(self.meshes, self.saved):
            for j, mat in enumerate(old):
                m.data.materials[j] = mat


def _visible_meshes() -> list[bpy.types.Object]:
    return [o for o in bpy.context.scene.objects if o.type == "MESH" and not o.hide_render]


@dataclass
class Passes:
    col: np.ndarray        # HxWx4 float straight alpha (supersampled)
    depth: np.ndarray      # HxW (inf = empty)
    key: np.ndarray        # HxW int64 region key (object × swatch, or object × FX value), −1 = empty
    base: np.ndarray       # HxWx3 float: the region's base colour (sRGB 0..1; the render where unknown)
    cov: np.ndarray        # HxW bool
    e: np.ndarray          # HxW emissive amount of the visible region (0..1)
    nolines: np.ndarray    # HxW bool: region excluded from seam lines


def mode_filter(key: np.ndarray, r: int = 1, keep: int = 4) -> np.ndarray:
    """Replace a pixel's region key by its neighbourhood's most common key when fewer than ``keep`` of its
    (2r+1)² neighbours share it — sub-pixel strips (painted creases, AA slivers) stop producing seam dashes."""
    offs = [(dy, dx) for dy in range(-r, r + 1) for dx in range(-r, r + 1)]
    shifted = [_shift(key, dy, dx, -1) for dy, dx in offs]
    counts = [sum((s_ == cand).astype(np.int16) for s_ in shifted) for cand in shifted]
    own = counts[offs.index((0, 0))]
    best = np.zeros_like(key)
    best_n = np.full(key.shape, -1, np.int16)
    for cand, n in zip(shifted, counts):
        better = (n > best_n) & (cand >= 0)
        best = np.where(better, cand, best)
        best_n = np.where(better, n, best_n)
    return np.where((own < keep) & (key >= 0), best, key)


def render_passes(size: int = ICON_PX, samples: int = 12) -> Passes:
    meshes = _visible_meshes()
    res = (size * SS, size * SS)
    col = review._render_rgba(res, samples).astype(np.float32) / 255.0
    with _PerObjectSlots(meshes, lambda i, m: review._flat_material("AF_Ink_Depth", "depth")):
        dz = review._render_rgba(res, 1, exr=True, sharp=True)
    with _PerObjectSlots(meshes, lambda i, m: _id_material(i, "af_icon_pal" in m.keys())):
        di = review._render_rgba(res, 1, exr=True, sharp=True)
    cov = dz[:, :, 3] > 0.5
    depth = np.where(cov, dz[:, :, 0], np.inf).astype(np.float32)
    ids = di[:, :, :3].astype(np.float32)
    e = np.zeros(cov.shape, np.float32)
    nol = np.zeros(cov.shape, bool)
    base = col[:, :, :3].copy()
    key = np.full(cov.shape, -1, np.int64)
    k = np.rint(ids[:, :, 2] * 64.0).astype(np.int64) - 1
    for i, m in enumerate(meshes):
        sel = cov & (k == i)
        if not sel.any():
            continue
        if m.get("af_icon_nolines"):
            nol |= sel
        fam = m.get("af_icon_pal")
        if fam is None or fam not in _PALS:            # FX mesh: the value channel is the region
            key[sel] = i * 4096 + np.rint(ids[:, :, 0][sel] * 32.0).astype(np.int64)
            continue
        bc, pp = atlas_arrays(_PALS[fam])
        u = np.clip((ids[:, :, 0][sel] * GRID).astype(int), 0, GRID - 1)
        v = np.clip(((1.0 - ids[:, :, 1][sel]) * GRID).astype(int), 0, GRID - 1)
        px = pp.shape[0] // GRID
        e[sel] = pp[v * px + px // 2, u * px + px // 2, 2] / 255.0
        base[sel] = bc[v * px + px // 2, u * px + px // 2, :3] / 255.0
        key[sel] = i * 4096 + v * GRID + u
    key = mode_filter(key)
    return Passes(col, depth, key, base, cov, e, nol)


def depth_breaks(depth: np.ndarray, cov: np.ndarray, thr: float, ratio: float = 3.0) -> np.ndarray:
    """Far-side pixels of depth discontinuities between 4-neighbours: |jump| > thr and > ``ratio`` × the slope just
    before and after it (smooth steep surfaces are not breaks)."""
    d = np.where(cov, depth, np.nan).astype(np.float64)
    out = np.zeros(depth.shape, bool)
    for axis in (0, 1):
        def sh(a, k):
            return np.roll(a, -k, axis=axis)
        jump = sh(d, 1) - d                                  # d[x+1] - d[x]
        before = np.abs(d - sh(d, -1))                       # |d[x] - d[x-1]|
        after = np.abs(sh(d, 2) - sh(d, 1))                  # |d[x+2] - d[x+1]|
        expected = np.fmax(before, after)
        expected = np.where(np.isnan(expected), 0.0, expected)
        brk = (np.abs(jump) > thr) & (np.abs(jump) > ratio * expected) & ~np.isnan(jump)
        far_next = brk & (jump > 0)                          # x+1 is farther
        far_here = brk & (jump < 0)
        out |= far_here
        out |= np.roll(far_next, 1, axis=axis)
    out &= cov
    return out


# ── 2-D helpers ─────────────────────────────────────────────────────────────────────────────────────────
def _shift(a: np.ndarray, dy: int, dx: int, fill) -> np.ndarray:
    return review._shift(a, dy, dx, fill)


def _disk(r: float) -> list[tuple[int, int]]:
    return review._disk(r)


def dilate(mask: np.ndarray, r: float) -> np.ndarray:
    """Max-filter of a soft mask (HxW float) with a disk of radius ``r`` px (sub-pixel: the outer ring weighted)."""
    out = mask.copy()
    ri = int(math.ceil(r))
    for dy in range(-ri, ri + 1):
        for dx in range(-ri, ri + 1):
            d = math.hypot(dx, dy)
            if d == 0 or d > r + 0.5:
                continue
            w = min(1.0, r + 0.5 - d)
            out = np.maximum(out, _shift(mask, dy, dx, 0.0) * w)
    return out


def blur(a: np.ndarray, sigma: float) -> np.ndarray:
    return review.blur(a, sigma)


def down(a: np.ndarray, k: int) -> np.ndarray:
    h, w = a.shape[0] // k, a.shape[1] // k
    return a[:h * k, :w * k].reshape(h, k, w, k, *a.shape[2:]).mean(axis=(1, 3))


def line_tone(rgb: np.ndarray) -> np.ndarray:
    """web tone().line on sRGB 0..1 arrays."""
    return (rgb + np.array((20, 10, 30), np.float32) / 255.0) * 0.5 * 0.45


def over(dst: np.ndarray, src: np.ndarray) -> np.ndarray:
    """Straight-alpha 'over' of float RGBA arrays."""
    sa = src[..., 3:4]
    da = dst[..., 3:4]
    oa = sa + da * (1 - sa)
    rgb = (src[..., :3] * sa + dst[..., :3] * da * (1 - sa)) / np.maximum(oa, 1e-6)
    return np.concatenate([rgb, oa], axis=-1)


def solid(mask: np.ndarray, rgb: Sequence[float], alpha: float = 1.0) -> np.ndarray:
    out = np.zeros(mask.shape + (4,), np.float32)
    out[..., :3] = np.array(rgb, np.float32)
    out[..., 3] = mask * alpha
    return out


def hex01(h: str) -> np.ndarray:
    return np.array(color.hex_to_rgb(h), np.float32) / 255.0


def poly_mask(h: int, w: int, polys: Sequence[Sequence[tuple[float, float]]], ss: int = 4) -> np.ndarray:
    """Anti-aliased coverage of polygons (pixel coords, even-odd per polygon, union over polygons)."""
    ys = (np.arange(h * ss) + 0.5) / ss
    xs = (np.arange(w * ss) + 0.5) / ss
    X, Y = np.meshgrid(xs, ys)
    acc = np.zeros((h * ss, w * ss), bool)
    for poly in polys:
        P = np.asarray(poly, np.float64)
        x0, y0 = P[:, 0].min(), P[:, 1].min()
        x1, y1 = P[:, 0].max(), P[:, 1].max()
        r0, r1 = max(0, int(y0 * ss) - 1), min(h * ss, int(y1 * ss) + 2)
        c0, c1 = max(0, int(x0 * ss) - 1), min(w * ss, int(x1 * ss) + 2)
        if r1 <= r0 or c1 <= c0:
            continue
        xx, yy = X[r0:r1, c0:c1], Y[r0:r1, c0:c1]
        inside = np.zeros(xx.shape, bool)
        n = len(P)
        for i in range(n):
            ax, ay = P[i]
            bx, by = P[(i + 1) % n]
            cond = (ay > yy) != (by > yy)
            with np.errstate(divide="ignore", invalid="ignore"):
                xint = ax + (yy - ay) * (bx - ax) / (by - ay)
            inside ^= cond & (xx < xint)
        acc[r0:r1, c0:c1] |= inside
    return down(acc.astype(np.float32), ss)


def sparkle_poly(x: float, y: float, r: float, rot_deg: float = 0.0, n: int = 4, k: float = 0.2,
                 steps: int = 10) -> list[tuple[float, float]]:
    """web sparkle(): a four-point twinkle (quadratic curves through (k r, k r) control points)."""
    pts = []
    ca, sa = math.cos(math.radians(rot_deg)), math.sin(math.radians(rot_deg))
    tips = [(math.cos(2 * math.pi * i / n - math.pi / 2), math.sin(2 * math.pi * i / n - math.pi / 2)) for i in range(n)]
    for i in range(n):
        a = tips[i]
        b = tips[(i + 1) % n]
        mid = ((a[0] + b[0]) * k, (a[1] + b[1]) * k)
        for s in range(steps):
            t = s / steps
            px = (1 - t) ** 2 * a[0] + 2 * (1 - t) * t * mid[0] + t * t * b[0]
            py = (1 - t) ** 2 * a[1] + 2 * (1 - t) * t * mid[1] + t * t * b[1]
            pts.append((px, py))
    return [(x + r * (p[0] * ca - p[1] * sa), y + r * (p[0] * sa + p[1] * ca)) for p in pts]


@dataclass
class Accent:
    """An overlay painted after the inked body (web ``over``) or before it (``under``) in icon pixels —
    computed from the framing (``at`` = world point) or given in icon units (``u``)."""
    kind: str                       # 'sparkle' | 'glow' | 'glowline' (``at`` → ``to``) | 'rays' (gem tier 5)
    at: Sequence[float] | None = None
    to: Sequence[float] | None = None
    u: tuple[float, float] | None = None
    r_u: float = 6.0
    hex: str = "#FFFFFF"
    alpha: float = 1.0
    rot: float = 0.0
    core: float = 0.0               # glow: hot core (fraction of r)
    under: bool = False


def _accent_layer(a: Accent, size: int, cam) -> np.ndarray:
    s = size / UNIT
    if a.at is not None:
        x, y = screen_xy(cam, a.at, size)
    else:
        x, y = a.u[0] * s, a.u[1] * s
    r = a.r_u * s
    if a.kind == "sparkle":
        m = poly_mask(size, size, [sparkle_poly(x, y, r, a.rot)])
        return solid(m, hex01(a.hex), a.alpha)
    if a.kind == "rays":            # web gem tier 5: 8 short strokes round the stone (r_u = inner radius)
        polys = []
        for i in range(8):
            ang = 2 * math.pi * i / 8 + 0.2
            c_, s_ = math.cos(ang), math.sin(ang)
            r0, r1, w = r + 4 * (size / UNIT), r + 11 * (size / UNIT), 0.75 * (size / UNIT)
            polys.append([(x + c_ * r0 - s_ * w, y + s_ * r0 + c_ * w), (x + c_ * r1 - s_ * w * .4, y + s_ * r1 + c_ * w * .4),
                          (x + c_ * r1 + s_ * w * .4, y + s_ * r1 - c_ * w * .4), (x + c_ * r0 + s_ * w, y + s_ * r0 - c_ * w)])
        return solid(poly_mask(size, size, polys), hex01(a.hex), a.alpha)
    if a.kind in ("glow", "glowline"):
        yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
        if a.kind == "glowline":
            x2, y2 = screen_xy(cam, a.to, size)
            vx, vy = x2 - x, y2 - y
            t = np.clip(((xx + 0.5 - x) * vx + (yy + 0.5 - y) * vy) / max(vx * vx + vy * vy, 1e-6), 0, 1)
            d = np.hypot(xx + 0.5 - (x + t * vx), yy + 0.5 - (y + t * vy)) / max(r, 1e-3)
        else:
            d = np.hypot(xx + 0.5 - x, yy + 0.5 - y) / max(r, 1e-3)
        g = np.clip(1 - d, 0, 1) ** 2 * a.alpha
        if a.core > 0:
            g = np.maximum(g, np.clip(1 - d / a.core, 0, 1) ** 1.5 * min(1.0, a.alpha * 1.4))
        return solid(g, hex01(a.hex), 1.0)
    raise ValueError(a.kind)


# ── the icon render ─────────────────────────────────────────────────────────────────────────────────────
@dataclass
class IconStyle:
    ink_u: float = INK_U
    rim_u: float = RIM_U
    line_u: float = LINE_U
    seam_u: float = SEAM_U
    seams: bool = True
    seam_min_contrast: float = 0.10     # RGB distance (0..1.7) under which two regions are one shape
    depth_step_u: float = 1.6           # overlap lines where the nearer surface is ≥ this many units in front
    shadow: bool = True
    samples: int = 12
    auto_glow: float = 0.0              # > 0: a soft under-glow of the emissive regions' colour (web ``under``)
    auto_glow_r: float = 1.9            # glow radius × the emissive area's radius (≥ 9 u)


@dataclass
class IconResult:
    rgba: np.ndarray                    # HxWx4 uint8, straight alpha
    body: np.ndarray                    # HxW float coverage of the model (no ink, no shadow)
    inked: np.ndarray                   # HxW float coverage of body + ink
    gate: dict = field(default_factory=dict)


def render_icon(size: int = ICON_PX, style: IconStyle | None = None, accents: Sequence[Accent] = (),
                post: Callable[[np.ndarray, float], np.ndarray] | None = None) -> IconResult:
    """Render the visible meshes through the current studio framing into a finished icon (see module doc)."""
    st = style or IconStyle()
    cam = bpy.context.scene.camera
    ps = render_passes(size, st.samples)
    H, W = ps.cov.shape
    pxu = size * SS / UNIT                                  # supersampled pixels per unit
    wpu = cam.data.ortho_scale / UNIT
    col = ps.col
    rgb = col[:, :, :3]
    a = col[:, :, 3]
    # 1 — overlap lines: a nearer surface within line_u in front by more than depth_step → far pixel inked
    thr = st.depth_step_u * wpu
    lim = np.where(ps.cov, ps.depth - thr, -np.inf)
    best = np.full((H, W), np.inf, np.float32)
    lcol = np.zeros((H, W, 3), np.float32)
    lmask = np.zeros((H, W), bool)
    for dy, dx in _disk(st.line_u * pxu):
        dn = _shift(ps.depth, dy, dx, np.inf)
        nearer = (dn < lim) & (dn < best)
        if nearer.any():
            cn = _shift(rgb, dy, dx, 0.0)
            en = _shift(ps.e, dy, dx, 0.0)
            ok = nearer & (en < 0.35)
            lcol[ok] = cn[ok]
            best[ok] = dn[ok]
            lmask |= ok
    # keep only lines that start at a true depth discontinuity (a jump much larger than the surface's own slope
    # on either side) — a sphere's steep rim is a smooth slope, not an overlap
    lmask &= dilate(depth_breaks(ps.depth, ps.cov, thr).astype(np.float32), st.line_u * pxu + 1.0) > 0.5
    # 2 — region seams: two palette regions of different colour meeting on one surface (both sides, own tone)
    smask = np.zeros((H, W), bool)
    if st.seams:
        base_ok = ps.cov & (ps.e < 0.35) & ~ps.nolines
        dep = np.where(ps.cov, ps.depth, 1e9)
        for dy, dx in _disk(st.seam_u * pxu):
            q_k = _shift(ps.key, dy, dx, -1)
            q_d = _shift(dep, dy, dx, 1e9)
            q_b = _shift(ps.base, dy, dx, 0.0)
            q_e = _shift(ps.e, dy, dx, 1.0)
            q_nl = _shift(ps.nolines, dy, dx, True)
            diff = (q_k != ps.key) & (q_k >= 0)
            same_surface = np.abs(q_d - dep) <= thr
            contrast = np.abs(q_b - ps.base).sum(axis=2) > st.seam_min_contrast
            smask |= base_ok & diff & same_surface & contrast & (q_e < 0.35) & ~q_nl
    ink = hex01(INK_HEX)
    lc = line_tone(lcol)
    lc = lc + (ink - lc) * LINE_INK_MIX
    sc = line_tone(rgb)
    sc = sc + (ink - sc) * LINE_INK_MIX
    rgb = np.where(smask[:, :, None], sc, rgb)
    rgb = np.where(lmask[:, :, None], lc, rgb)
    # 3 — rim light on the upper-left silhouette edge
    if st.rim_u > 0:
        r = max(1, int(round(st.rim_u * pxu)))
        rim = a * (1.0 - _shift(a, -r, -r, 0.0))
        rc = np.array(RIM_RGBA[:3], np.float32) / 255.0
        k = (rim * RIM_RGBA[3])[:, :, None]
        rgb = rgb + (rc - rgb) * k
    body = np.dstack([rgb, a])
    glow_layer = None
    if st.auto_glow > 0:
        em = ps.cov & (ps.e >= 0.35)
        if em.sum() > 6:
            ys, xs = np.nonzero(em)
            gx, gy = xs.mean() / SS, ys.mean() / SS
            spread = max(np.sqrt(((xs - xs.mean()) ** 2 + (ys - ys.mean()) ** 2).mean()) / SS * 1.4, 1.0)
            gcol = np.clip(ps.base[em].mean(axis=0) * 1.15, 0, 1)
            gr = max(spread * st.auto_glow_r, 9.0 * size / UNIT)
            yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
            d = np.hypot(xx + 0.5 - gx, yy + 0.5 - gy) / gr
            g = np.clip(1 - d, 0, 1) ** 2 * st.auto_glow
            glow_layer = solid(g, gcol, 1.0)
    # 4 — downsample, ink silhouette behind the body
    pm = down(np.dstack([body[:, :, :3] * a[:, :, None], a]), SS)
    fa = pm[:, :, 3]
    frgb = pm[:, :, :3] / np.maximum(fa[:, :, None], 1e-6)
    out = np.dstack([frgb, fa]).astype(np.float32)
    s = size / UNIT
    inkm = dilate(np.clip(fa * 1.25, 0, 1), st.ink_u * s) if st.ink_u > 0 else fa
    inked = over(solid(inkm, ink), out)
    layers = inked
    # 5 — drop shadow
    if st.shadow:
        sh = _shift(inked[:, :, 3], -int(round(SHADOW_OFF_U[1] * s)), -int(round(SHADOW_OFF_U[0] * s)), 0.0)
        sh = blur(sh, SHADOW_BLUR_U * s / 2.0) * SHADOW_ALPHA
        layers = over(solid(sh, (0, 0, 0)), inked)
    if glow_layer is not None:                      # under-glow: behind the inked body and its shadow
        layers = over(glow_layer, layers)
    for ac in accents:                              # over(dst, src) = src composited over dst
        lay = _accent_layer(ac, size, cam)
        layers = over(lay, layers) if ac.under else over(layers, lay)
    if post is not None:
        layers = post(layers, s)
    rgba = (np.clip(layers, 0, 1) * 255.0 + 0.5).astype(np.uint8)
    res = IconResult(rgba, fa, inked[:, :, 3])
    gate_rgba = (np.clip(inked, 0, 1) * 255.0 + 0.5).astype(np.uint8)     # the inked body, before glows / accents
    res.gate = icon_gate(res, st.ink_u, size, gate_rgba)
    return res


# ── gate ────────────────────────────────────────────────────────────────────────────────────────────────
def icon_gate(res: IconResult, ink_u: float, size: int, inked_rgba: np.ndarray | None = None) -> dict:
    """Measured on the finished icon: the ink rim round the body (every outer-silhouette pixel ≥ ink − 0.5 u
    from the model), the ink's darkness, an empty border (nothing clipped) and the fill of the square."""
    s = size / UNIT
    body = res.body > 0.5
    full = res.inked > 0.5
    edge = full & ~(_shift(full, 1, 0, False) & _shift(full, -1, 0, False) & _shift(full, 0, 1, False)
                    & _shift(full, 0, -1, False))
    need = max(1.0, (ink_u - 0.5) * s)
    near = body.copy()
    for dy, dx in _disk(need - 0.5):
        near |= _shift(body, dy, dx, False)
    edge_px = int(edge.sum())
    thin = int((edge & near).sum())
    frac = 1.0 - thin / max(edge_px, 1)
    touch = res.body > 0.02
    for dy, dx in _disk(2.0):
        touch = touch | _shift(res.body > 0.02, dy, dx, False)
    ink_px = (res.inked > 0.98) & ~touch
    src = res.rgba if inked_rgba is None else inked_rgba
    rgb = src[:, :, :3].astype(np.float32)
    lum = 0.2126 * rgb[:, :, 0] + 0.7152 * rgb[:, :, 1] + 0.0722 * rgb[:, :, 2]
    ink_lum = float(np.percentile(lum[ink_px], 95)) if ink_px.any() else 0.0
    a = src[:, :, 3]
    border = int(max(a[0].max(), a[-1].max(), a[:, 0].max(), a[:, -1].max()))
    ys, xs = np.nonzero(body)
    if len(xs):
        span = max(xs.max() - xs.min() + 1, ys.max() - ys.min() + 1) / size
    else:
        span = 0.0
    ok = frac >= 0.98 and ink_lum <= 40.0 and border == 0 and 0.30 <= span <= 0.93
    return {"pass": bool(ok), "inkFrac": round(frac, 4), "inkPxRequired": round(need, 2), "inkLumP95": round(ink_lum, 1),
            "borderAlphaMax": border, "span": round(float(span), 3), "edgePx": edge_px}


# ── icon-only models ────────────────────────────────────────────────────────────────────────────────────
def build_model(name: str, pal, regions: dict, parts: Sequence[tuple], nolines: bool = False) -> bpy.types.Object:
    """One mesh from ``parts`` = [(Part, region name), …] with ``regions`` = {name: Region} on ``pal``."""
    b = M.Builder(name, pal, prefix=name)
    b.regions(**regions)
    for part, reg in parts:
        if part is not None:
            b.add(part, reg, outline=False)
    obj = b.build(name)
    _ATLAS_CACHE.pop(pal.texture_names()[0], None)
    set_look(obj, pal)
    if nolines:
        obj["af_icon_nolines"] = 1
    return obj


# ── output ──────────────────────────────────────────────────────────────────────────────────────────────
def write_icon(rel: str, rgba: np.ndarray) -> Path:
    """Write a shipped icon texture under ``Art/Export`` (``rel`` e.g. 'Icons/Items/T_UI_ItemIcon_w_axe.png')."""
    p = paths.export_root() / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    pngio.write_png(p, rgba)
    return p


def preview_dir(name: str) -> Path:
    d = paths.preview_root() / name
    d.mkdir(parents=True, exist_ok=True)
    return d


SLOT_BG = (42, 37, 48)
SHEET_BG = (24, 20, 30)


def _composite(bg_rgb, rgba: np.ndarray) -> np.ndarray:
    a = rgba[:, :, 3:4].astype(np.float32) / 255.0
    return (np.array(bg_rgb, np.float32) * (1 - a) + rgba[:, :, :3].astype(np.float32) * a + 0.5).astype(np.uint8)


def _resize(rgba: np.ndarray, px: int) -> np.ndarray:
    """Premultiplied area resample of an RGBA icon to ``px``² (what Slate's mip-less UI filtering approximates
    at an integer-ish ratio)."""
    f = rgba.astype(np.float32) / 255.0
    pm = np.dstack([f[:, :, :3] * f[:, :, 3:4], f[:, :, 3:4]])
    h = rgba.shape[0]
    # area resample via integral image
    edges = np.linspace(0, h, px + 1)
    ii = np.zeros((h + 1, h + 1, 4), np.float64)
    ii[1:, 1:] = pm.cumsum(0).cumsum(1)
    out = np.zeros((px, px, 4), np.float64)
    for i in range(px):
        y0, y1 = edges[i], edges[i + 1]
        for j in range(px):
            x0, x1 = edges[j], edges[j + 1]
            out[i, j] = _box(ii, y0, y1, x0, x1)
    a = out[:, :, 3:4]
    rgb = out[:, :, :3] / np.maximum(a, 1e-6)
    return (np.clip(np.dstack([rgb, a]), 0, 1) * 255 + 0.5).astype(np.uint8)


def _box(ii, y0, y1, x0, x1):
    """Mean over a fractional box from an integral image (bilinear on the integral)."""
    def I(y, x):
        yi, xi = int(math.floor(y)), int(math.floor(x))
        yi1, xi1 = min(yi + 1, ii.shape[0] - 1), min(xi + 1, ii.shape[1] - 1)
        fy, fx = y - yi, x - xi
        return (ii[yi, xi] * (1 - fy) * (1 - fx) + ii[yi1, xi] * fy * (1 - fx) + ii[yi, xi1] * (1 - fy) * fx
                + ii[yi1, xi1] * fy * fx)
    return (I(y1, x1) - I(y0, x1) - I(y1, x0) + I(y0, x0)) / max((y1 - y0) * (x1 - x0), 1e-9)


def resize(rgba: np.ndarray, px: int) -> np.ndarray:
    if rgba.shape[0] == px:
        return rgba
    if rgba.shape[0] % px == 0:
        k = rgba.shape[0] // px
        f = rgba.astype(np.float32) / 255.0
        pm = down(np.dstack([f[:, :, :3] * f[:, :, 3:4], f[:, :, 3:4]]), k)
        a = pm[:, :, 3:4]
        return (np.clip(np.dstack([pm[:, :, :3] / np.maximum(a, 1e-6), a]), 0, 1) * 255 + 0.5).astype(np.uint8)
    return _resize(rgba, px)


def atlas(items: Sequence[tuple[str, np.ndarray]], cols: int, scale: float = 1.0, pad: int = 6,
          label_h: int = 12, bg=SHEET_BG, slot=SLOT_BG) -> np.ndarray:
    """A sheet of icons on slot-coloured cells with labels (5×7 font), icons at ``scale`` (1 = native pixels)."""
    px = int(round(items[0][1].shape[0] * scale))
    cw, ch = px + 2 * pad, px + 2 * pad + label_h
    rows = (len(items) + cols - 1) // cols
    img = np.zeros((rows * ch + pad, cols * cw + pad, 3), np.uint8)
    img[:] = bg
    for i, (name, rgba) in enumerate(items):
        r, c = divmod(i, cols)
        x0, y0 = pad + c * cw, pad + r * ch
        cell = np.zeros((px + 2 * pad - 4, px + 2 * pad - 4, 3), np.uint8)
        cell[:] = slot
        img[y0:y0 + cell.shape[0], x0:x0 + cell.shape[1]] = cell
        ic = resize(rgba, px)
        img[y0 + pad - 2:y0 + pad - 2 + px, x0 + pad - 2:x0 + pad - 2 + px] = _composite(
            slot, ic)
        review.draw_text(img, x0 + 2, y0 + px + 2 * pad - 2, name[:max(4, (cw - 4) // 6)], rgb=(200, 196, 210))
    return img


def ui_sheet(items: Sequence[tuple[str, np.ndarray]], cols: int = 8, sizes=(48, 36),
             qualities: Sequence[str] = ("normal", "magic", "rare", "legendary", "set")) -> np.ndarray:
    """In-game sizes: each icon at 48 and 36 px in a dark slot with a 2 px quality frame (as the inventory /
    skill bar draws them), plus a 36 px copy on the lighter panel colour — the readability check."""
    pad = 6
    cw = sizes[0] + sizes[1] + 3 * pad + 8
    ch = sizes[0] + 2 * pad + 14
    rows = (len(items) + cols - 1) // cols
    img = np.zeros((rows * ch + pad, cols * cw + pad, 3), np.uint8)
    img[:] = SHEET_BG
    for i, (name, rgba) in enumerate(items):
        r, c = divmod(i, cols)
        x0, y0 = pad + c * cw, pad + r * ch
        q = hex01(QUALITY[qualities[i % len(qualities)]]) * 255
        for j, (sz, bg) in enumerate(((sizes[0], (21, 18, 26)), (sizes[1], (58, 52, 64)))):
            xo = x0 + (0 if j == 0 else sizes[0] + pad + 4)
            box = np.zeros((sz + 4, sz + 4, 3), np.uint8)
            box[:] = q.astype(np.uint8)
            box[2:-2, 2:-2] = bg
            ic = resize(rgba, sz)
            box[2:-2, 2:-2] = _composite(bg, ic)
            img[y0:y0 + sz + 4, xo:xo + sz + 4] = box
        review.draw_text(img, x0, y0 + sizes[0] + 8, name[:14], rgb=(190, 186, 200))
    return img


def save_preview(name: str, file: str, img: np.ndarray) -> list[str]:
    """Native truecolour preview (``pngio.write_png_native``: split into ≤ 200 KB tiles, never resampled)."""
    out = pngio.write_png_native(preview_dir(name) / file, img)
    return [Path(p).relative_to(paths.preview_root()).as_posix() for p, _ in out]


# ── manifest ────────────────────────────────────────────────────────────────────────────────────────────
class IconManifest(export.Manifest):
    """The kit manifest writer, also merging the top-level ``ui`` section (UI textures: icons, glyphs, emblems,
    portraits; one key per texture name). Same lock, same per-key merge as ``assets``."""

    MERGED = ("assets", "palettes", "gameIds", "ui")


IMPORT_HINT = {"compression": "UserInterface2D", "group": "UI", "mips": True, "mipGen": "SimpleAverage",
               "note": "drawn at 1/4-1/7 of the texture size (icons 36-48 px, glyphs 14-32 px): without mips Slate's "
                       "bilinear minification aliases the 1-2 px line art"}


def ui_entry(kind: str, rel_file: str, px: int, **extra) -> dict:
    e = {"kind": kind, "file": rel_file, "px": px, "srgb": True, "alpha": "straight", "import": IMPORT_HINT}
    e.update(extra)
    return e


def lum(rgb) -> float:
    return 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2]

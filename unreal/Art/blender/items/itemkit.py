"""Shared helpers for the equipment and ground-loot generators (``items/weapons.py``, ``items/loot.py``).

* **Tier atlases** (DECISIONS I6 / R4: one mesh per weapon base family, tiered *material* variants): every family
  mesh is UV-collapsed onto the ``Weapons`` palette (= tier T1); each other tier is the same 256² atlas with the
  family's role swatches recoloured — ``T_AF_Palette_Weapons_<Tier>_BC/_P`` → ``MI_AF_Toon_Weapons_<Tier>``
  (``build_content.py`` makes one MI per manifest palette). Swapping slot 0's MI re-skins the mesh; geometry,
  sockets and hulls are shared. ``plan_roles`` keeps every role on its own swatch whenever two roles differ in any
  tier (identical T1 values would otherwise share a swatch and be recoloured together).
* **Hero rigs from the shipped FBX** (what UE receives): ``import_hero`` re-imports ``SK_Hero_<Class>`` and drives
  it with ``A_Hero_<Class>_Idle`` frame 0 (the READY pose) so the generic weapons are judged in the real hands.
* **Previews**: studio cells (``studio``), W1 game-camera 1080p 1:1 cells with the ink gate (``GameCells``), the
  quality-tint / pool / beam composite of the UE loot presentation (``quality_post``).
* **Ship**: ``ship_static_item`` = FBX + manifest entry (merging writer) + ``inkGate``; ``verify_fbx`` re-imports.
"""
from __future__ import annotations

import json
import math
import sys
from dataclasses import replace
from pathlib import Path
from typing import Callable, Sequence

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

import kit  # noqa: E402,F401
from kit.paths import scratch_root  # noqa: E402
from kit import color, export, mesh as M, outline, paths, pngio, review, scene, shading  # noqa: E402
from kit.palette import GRID, SWATCH_PX, Palette, Region, _q8  # noqa: E402

SCRATCH = scratch_root("items")
DATA = paths.UNREAL_ROOT / "Data"


# ── item data ───────────────────────────────────────────────────────────────────────────────────────────
def item_bases() -> dict:
    return json.loads((DATA / "item_bases.json").read_text())


def weapon_bases() -> list[dict]:
    return list(item_bases()["groups"]["weapons"])


# Tier ladder: the weapon bases climb in the same level steps in every family (1 · 6/8/10 · 15 · 20 · 28 · 35);
# the rusty sword (the only "worn" base) keeps its own atlas.
TIERS = ("T1", "T2", "T3", "T4", "T5", "T6")
TIER_MIN_LEVEL = {"T1": 1, "T2": 6, "T3": 15, "T4": 20, "T5": 28, "T6": 35}
WORN = "Worn"


def weapon_tier(base: dict) -> str:
    if "rusty" in base["id"]:
        return WORN
    lv = int(base.get("levelReq", 1))
    return max((t for t in TIERS if lv >= TIER_MIN_LEVEL[t]), key=lambda t: TIER_MIN_LEVEL[t])


# ── palettes ────────────────────────────────────────────────────────────────────────────────────────────
def _nudge(hex_: str, k: int) -> str:
    r, g, b = color.hex_to_rgb(hex_)
    b = b + k if b + k <= 255 else b - k
    return color.rgb_to_hex((r, g, int(b)))


def plan_roles(tables: dict[str, dict[str, dict[str, Region]]], base: str = "T1") -> dict[str, dict[str, Region]]:
    """``tables[prefix][variant][role]`` → the base-variant regions per prefix, nudged (blue ±1/255, invisible) so
    that two roles that differ in any variant never share a swatch. Every variant must list the same roles."""
    used: dict[tuple, tuple] = {}
    out: dict[str, dict[str, Region]] = {}
    for prefix in sorted(tables):
        var = tables[prefix]
        roles = sorted(var[base])
        for v, regs in var.items():
            missing = set(roles) ^ set(regs)
            if missing:
                raise ValueError(f"{prefix}/{v}: roles differ from {base}: {sorted(missing)}")
        out[prefix] = {}
        for role in roles:
            sig = tuple(var[v][role].key() for v in sorted(var))
            r = var[base][role]
            k = 0
            while True:
                cand = r if k == 0 else replace(r, hex=_nudge(r.hex, k))
                key = cand.key()
                if key not in used or used[key] == sig:
                    used[key] = sig
                    break
                k += 1
            out[prefix][role] = cand
    return out


class AtlasVariant:
    """A recoloured copy of ``base``'s atlas (same swatch layout): ``T_AF_Palette_<Family>_<Variant>_BC/_P`` and
    the instance ``MI_AF_Toon_<Family>_<Variant>``. ``regions`` = {swatch name: Region} overriding the base."""

    def __init__(self, base: Palette, variant: str, regions: dict[str, Region]):
        self.base = base
        self.variant = variant
        self.family = f"{base.family}_{variant}"
        self.regions = regions

    @property
    def material_name(self) -> str:
        return f"MI_AF_Toon_{self.family}"

    def texture_names(self) -> tuple[str, str]:
        return f"T_AF_Palette_{self.family}_BC", f"T_AF_Palette_{self.family}_P"

    def texture_paths(self) -> tuple[Path, Path]:
        bc, p = self.texture_names()
        return paths.texture_dir() / f"{bc}.png", paths.texture_dir() / f"{p}.png"

    def arrays(self) -> tuple[np.ndarray, np.ndarray]:
        bc, pp = self.base.atlas_arrays()
        for name, r in self.regions.items():
            i = self.base.index(name)
            col, row = i % GRID, i // GRID
            ys = slice(row * SWATCH_PX, (row + 1) * SWATCH_PX)
            xs = slice(col * SWATCH_PX, (col + 1) * SWATCH_PX)
            bc[ys, xs] = [int(c) for c in color.hex_to_rgb(r.hex)]
            pp[ys, xs] = (_q8(r.s), _q8(r.l), _q8(r.e), _q8(r.o))
        return bc, pp

    def save(self) -> tuple[Path, Path]:
        bc, pp = self.arrays()
        pbc, ppp = self.texture_paths()
        pbc.parent.mkdir(parents=True, exist_ok=True)
        pngio.write_png(pbc, bc)
        pngio.write_png(ppp, pp)
        return pbc, ppp

    def manifest_entry(self) -> dict:
        e = self.base.manifest_entry()
        pbc, ppp = self.texture_paths()
        bcn, pn = self.texture_names()
        e["material"] = self.material_name
        e["baseColor"] = dict(e["baseColor"], name=bcn, file=paths.rel_to_export(pbc))
        e["params"] = dict(e["params"], name=pn, file=paths.rel_to_export(ppp))
        e["variantOf"] = self.base.family
        e["variant"] = self.variant
        return e


class ReadOnlyPalette:
    """The committed atlas of another family (heroes) for preview materials — never rewritten by this script."""

    def __init__(self, family: str):
        self.family = family

    @property
    def material_name(self) -> str:
        return f"MI_AF_Toon_{self.family}"

    def texture_names(self) -> tuple[str, str]:
        return f"T_AF_Palette_{self.family}_BC", f"T_AF_Palette_{self.family}_P"

    def save(self) -> tuple[Path, Path]:
        bc, p = self.texture_names()
        return paths.texture_dir() / f"{bc}.png", paths.texture_dir() / f"{p}.png"


def set_toon(obj: bpy.types.Object, pal) -> None:
    """Swap slot 0 / 1 preview materials of ``obj`` to ``pal`` (a Palette, AtlasVariant or ReadOnlyPalette).
    ``shading.outline_material`` rebuilds the one material named ``M_AF_Outline`` in place: the existing one (the
    shipped item's slot 1, exported under that name) is parked while this palette's copy is made under its own name."""
    keep = bpy.data.materials.get(shading.OUTLINE_MATERIAL)
    if keep is not None:
        keep.name = "__af_outline_keep"
    mt, mo = shading.toon_material(pal), shading.outline_material(pal)
    mo.name = f"M_AF_Outline__{pal.family}"
    if keep is not None:
        keep.name = shading.OUTLINE_MATERIAL
    mats = obj.data.materials
    if len(mats) >= 2:
        mats[0], mats[1] = mt, mo
    else:
        mats.clear()
        mats.append(mt)


# ── heroes (re-imported FBX) ────────────────────────────────────────────────────────────────────────────
HERO_HEIGHT = {"Warrior": 1.76, "Mage": 1.71, "Rogue": 1.62}


def import_hero(cls: str, clip: str = "Idle", frame: int = 0):
    """``SK_Hero_<cls>.fbx`` driven by ``A_Hero_<cls>_<clip>.fbx`` at ``frame`` (Idle 0 = the READY pose), with the
    Heroes preview materials and the hero ink. Returns (armature, body mesh)."""
    d = paths.category_dir("Characters")
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(d / f"SK_Hero_{cls}.fbx"))
    new = [o for o in bpy.data.objects if o not in before]
    arm = next(o for o in new if o.type == "ARMATURE")
    body = next(o for o in new if o.type == "MESH")
    arm.name, body.name = f"HERO_{cls}", f"HERO_{cls}_body"
    before = set(bpy.data.objects)
    acts = set(bpy.data.actions)
    bpy.ops.import_scene.fbx(filepath=str(d / f"A_Hero_{cls}_{clip}.fbx"))
    arm2 = next(o for o in bpy.data.objects if o not in before and o.type == "ARMATURE")
    act = next(a for a in bpy.data.actions if a not in acts)
    for pb in arm.pose.bones:
        c = pb.constraints.new("COPY_TRANSFORMS")
        c.target, c.subtarget = arm2, pb.name
        c.target_space = c.owner_space = "POSE"
    arm2.hide_render = True
    for o in [o for o in bpy.data.objects if o not in before and o.type == "MESH"]:
        o.hide_render = True
    bpy.context.scene.frame_set(int(act.frame_range[0]) + frame)
    set_toon(body, ReadOnlyPalette("Heroes"))
    body["af_outline_width"] = outline.WIDTH_CM["hero"] / 100.0
    body["af_outline_class"] = "hero"
    arm["af_anim_armature"] = arm2.name
    arm[f"af_clip_{clip}"] = arm2.name
    arm[f"af_clip_{clip}_start"] = int(act.frame_range[0])
    bpy.context.view_layer.update()
    return arm, body


def hero_pose(arm, cls: str, clip: str, frame: int) -> None:
    """Drive the imported hero with ``A_Hero_<cls>_<clip>.fbx`` at ``frame`` (the clip armature is imported once and
    cached; the body's COPY_TRANSFORMS constraints are re-targeted to it)."""
    key = f"af_clip_{clip}"
    if key not in arm:
        before = set(bpy.data.objects)
        acts = set(bpy.data.actions)
        bpy.ops.import_scene.fbx(filepath=str(paths.category_dir("Characters") / f"A_Hero_{cls}_{clip}.fbx"))
        a2 = next(o for o in bpy.data.objects if o not in before and o.type == "ARMATURE")
        a2.hide_render = True
        a2.name = f"ANIM_{cls}_{clip}"
        act = next(a for a in bpy.data.actions if a not in acts)
        arm[key] = a2.name
        arm[key + "_start"] = int(act.frame_range[0])
    a2 = bpy.data.objects[arm[key]]
    for pb in arm.pose.bones:
        for c in pb.constraints:
            if c.type == "COPY_TRANSFORMS":
                c.target = a2
    bpy.context.scene.frame_set(int(arm[key + "_start"]) + frame)
    bpy.context.view_layer.update()


def min_z(objs) -> float:
    """Lowest world z (m) of the posed meshes."""
    return float(review.mesh_points(list(objs))[:, 2].min())


def attach(obj: bpy.types.Object, arm: bpy.types.Object, bone: str) -> None:
    """UE socket convention: the item's local frame = the bone frame at the bone head."""
    obj.parent = arm
    obj.parent_type = "BONE"
    obj.parent_bone = bone
    obj.matrix_parent_inverse = Matrix.Translation((0.0, -arm.data.bones[bone].length, 0.0))
    obj.matrix_basis = Matrix.Identity(4)


def detach(obj: bpy.types.Object, world: Matrix | None = None) -> None:
    obj.parent = None
    obj.matrix_parent_inverse = Matrix.Identity(4)
    obj.matrix_world = world if world is not None else Matrix.Identity(4)


def remove_hero(arm: bpy.types.Object) -> None:
    a2 = bpy.data.objects.get(arm.get("af_anim_armature", ""))
    for o in [o for o in bpy.data.objects if o.parent in (arm, a2) and o.type == "MESH" and o.parent_type != "BONE"]:
        bpy.data.objects.remove(o)
    for o in (a2, arm):
        if o is not None:
            bpy.data.objects.remove(o)


# ── studio cells ────────────────────────────────────────────────────────────────────────────────────────
def studio(objs: Sequence[bpy.types.Object], res=(300, 360), pitch: float = -12.0, yaw: float = shading.CAM_YAW_UE,
           ink_px: float = 1.6, fill: float = 0.9, label: str = "", pts: np.ndarray | None = None,
           bg: str = review.STUDIO_BG, samples: int = 8, fov: float = 24.0) -> np.ndarray:
    """One studio render (no stage) of ``objs`` framed from a UE yaw / pitch, ink ``ink_px`` screen pixels."""
    review.hide_stage(True)
    review.setup_render(res, samples, bg_hex=bg)
    p = review.mesh_points(objs) if pts is None else pts
    review.frame_points(p, res, fov_h=fov, yaw_ue=yaw, pitch_ue=pitch, fill=fill, include_ground=False)
    review.set_outline_px(objs, ink_px, Vector(p.mean(0)))
    img = review.render_array()
    if label:
        review.draw_text(img, 3, 3, label)
    return img


def only_visible(keep: Sequence[bpy.types.Object]):
    """Context manager: hide every other mesh in renders."""
    class _C:
        def __enter__(self):
            self.prev = {o.name: o.hide_render for o in bpy.data.objects
                         if o.type == "MESH" and o.name != "AF_Stage"}
            ks = {o.name for o in keep}
            for o in bpy.data.objects:
                if o.type == "MESH" and o.name != "AF_Stage":
                    o.hide_render = o.name not in ks
            return self

        def __exit__(self, *exc):
            for n, h in self.prev.items():
                if n in bpy.data.objects:
                    bpy.data.objects[n].hide_render = h
    return _C()


def pad_to(img: np.ndarray, h: int, w: int, bg=None) -> np.ndarray:
    """Centre ``img`` in an h × w cell: padded with its corner colour (or ``bg``), centre-cropped when larger."""
    out = np.zeros((h, w, 3), np.uint8)
    out[:, :] = img[0, 0] if bg is None else bg
    sy, sx = max(0, (img.shape[0] - h) // 2), max(0, (img.shape[1] - w) // 2)
    src = img[sy:sy + h, sx:sx + w]
    y0, x0 = (h - src.shape[0]) // 2, (w - src.shape[1]) // 2
    out[y0:y0 + src.shape[0], x0:x0 + src.shape[1]] = src
    return out


def save(out_dir: Path, name: str, img: np.ndarray) -> Path:
    out_dir.mkdir(parents=True, exist_ok=True)
    p = out_dir / name
    pngio.write_png_budget(p, img)
    return p


# ── W1 game cells (1080p 1:1, ink-gated) ────────────────────────────────────────────────────────────────
class GameCells:
    """1080p 1:1 crops at the W1 game camera (default distance) with the shipped screen-constant ink, bloom and the
    kit's ink gate (``review.Review.game_shot``). ``root`` is yawed by the facings."""

    def __init__(self, asset: str, root: bpy.types.Object, meshes: Sequence[bpy.types.Object], height: float,
                 outline_class: str, blob_radius: float = 0.3, out_dir: Path | None = None, glows=None,
                 ground_occludes: bool = True):
        """``ground_occludes``: the stage hides the hull under the floor in the ink measurement, as the terrain does
        in UE (planted soles; ``Review.ground_occludes``)."""
        self.rv = review.Review(asset, root, list(meshes), height, blob_radius=blob_radius,
                                game_outline_px=outline.SCREEN_PX_1080[outline_class], outline_class=outline_class,
                                out_dir=out_dir or SCRATCH / asset, glows=glows)
        self.rv.ground_occludes = ground_occludes

    @property
    def reports(self) -> list[dict]:
        return self.rv.ink_reports

    def frame(self, focus=(0.0, 0.0), pad: float = 0.03, min_px: tuple[int, int] = (0, 0)):
        """Camera at the default W1 distance looking at ``focus`` (ground XY) and the crop box round the meshes."""
        review.hide_stage(False)
        review.setup_render((1920, 1080), self.rv.samples)
        cam = review.game_camera(focus)
        box = self.rv.game_crop_box(cam, pad=pad)
        if min_px[0] or min_px[1]:
            cx, cy = (box[0] + box[2]) / 2, (box[1] + box[3]) / 2
            hw = max((box[2] - box[0]) / 2, min_px[0] / 1920 / 2)
            hh = max((box[3] - box[1]) / 2, min_px[1] / 1080 / 2)
            box = (max(0, cx - hw), max(0, cy - hh), min(1, cx + hw), min(1, cy + hh))
        return cam, box

    def cell(self, tag: str, facing: str | float = "se", focus=(0.0, 0.0), measure: bool = True, pad: float = 0.03,
             min_px: tuple[int, int] = (0, 0), label: bool = True, post: Callable | None = None):
        if facing is not None:
            self.rv.facing(facing)
        cam, box = self.frame(focus, pad, min_px)
        self.rv.game_outline(1080)
        img, st = self.rv.game_shot(box, measure=measure, tag=tag)
        if post is not None:
            img = post(img, cam, box)
        if label:
            review.draw_text(img, 3, 3, tag.upper())
            if st:
                review.draw_text(img, 3, 12, self.rv.ink_label(st), rgb=(255, 220, 140))
        return img, st

    def wide(self, name: str, facing: str = "se", text: str = "") -> np.ndarray:
        """``game.png``: the 640×360 W1 frame (the whole default view) for scale."""
        self.rv.facing(facing)
        review.hide_stage(False)
        review.setup_render(review.GAME_RES, self.rv.samples)
        review.game_camera()
        self.rv.game_outline(review.GAME_RES[1])
        img, _ = self.rv.game_shot(None, review.GAME_RES)
        review.draw_text(img, 6, 6, text or f"{name}  GAME W1 YAW45 PITCH-50 FOV35  640X360")
        return img

    def inkmap(self, img: np.ndarray, st: dict) -> np.ndarray:
        ys, xs = st["edge_yx"]
        y0, y1 = max(0, int(ys.min()) - 6), min(img.shape[0], int(ys.max()) + 7)
        x0, x1 = max(0, int(xs.min()) - 6), min(img.shape[1], int(xs.max()) + 7)
        return review.ink_map(img, st, zoom=3)[y0 * 3:y1 * 3, x0 * 3:x1 * 3]

    def verdict(self) -> dict:
        return review.ink_verdict(self.reports)


def ink_entry(v: dict) -> dict:
    return {k: v[k] for k in ("pass", "cells", "minFracRim", "minFracDark", "medianRimPx", "maxHullLumP95", "gate")}


# ── loot presentation composite (UE AbyssWorldBuilder: QualityTint, pool, NS_Loot_Beam) ──────────────────
QUALITY = {   # canonical palette (spec §1.1, QUIRK A1): normal untinted
    "normal": "#C8C8C8", "magic": "#4F8CFF", "rare": "#FFD84A", "legendary": "#FF8A2A", "set": "#3ECF6A",
}


def _lin(a):
    a = np.asarray(a, np.float32) / 255.0
    return np.where(a <= 0.04045, a / 12.92, ((a + 0.055) / 1.055) ** 2.4)


def _srgb(a):
    a = np.clip(a, 0.0, 1.0)
    return (np.where(a <= 0.0031308, a * 12.92, 1.055 * np.power(a, 1 / 2.4) - 0.055) * 255.0 + 0.5).clip(0, 255)


def tint_body(img: np.ndarray, body: np.ndarray, hex_: str, amount: float) -> np.ndarray:
    """UE M_AF_Toon StatusTint: lin *= lerp(1, tint, amount) on the toon (body) pixels only (the hull keeps ink)."""
    t = _lin(color.hex_to_rgb(hex_))
    lin = _lin(img)
    k = (1.0 - amount) + amount * t
    w = np.clip(body, 0, 1)[..., None]
    lin = lin * (1 - w) + lin * k * w
    return _srgb(lin).astype(np.uint8)


def add_glow(img: np.ndarray, cx: float, cy: float, rx: float, ry: float, hex_: str, alpha: float,
             core: float = 0.0) -> np.ndarray:
    """Additive soft ellipse (ground pool / beam core), web falloff."""
    h, w = img.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    d = np.sqrt(((xx - cx) / max(rx, 1e-3)) ** 2 + ((yy - cy) / max(ry, 1e-3)) ** 2)
    f = np.interp(d, [0.0, 0.15, 0.4, 0.7, 1.0], [1.0, 0.9, 0.6, 0.25, 0.0]).astype(np.float32)
    lin = _lin(img) + _lin(color.hex_to_rgb(hex_))[None, None] * (f * alpha)[..., None]
    return _srgb(lin).astype(np.uint8)


def add_beam(img: np.ndarray, x: float, y_base: float, y_top: float, width: float, hex_: str, alpha: float,
             core_alpha: float) -> np.ndarray:
    """``NS_Loot_Beam`` pillar: a vertical additive gradient (quality colour, fading toward the top) + white core."""
    h, w = img.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    t = np.clip((y_base - yy) / max(y_base - y_top, 1.0), 0, 1)
    vert = np.where((yy <= y_base) & (yy >= y_top), (1 - t) ** 1.2, 0.0)
    across = np.clip(1 - np.abs(xx - x) / (width / 2), 0, 1) ** 1.5
    core = np.clip(1 - np.abs(xx - x) / (width * 0.16), 0, 1) * np.where((yy >= y_base - 0.8 * (y_base - y_top))
                                                                         & (yy <= y_base), 1.0, 0.0)
    lin = _lin(img)
    lin = lin + _lin(color.hex_to_rgb(hex_))[None, None] * (vert * across * alpha)[..., None]
    lin = lin + np.ones(3, np.float32)[None, None] * (vert * core * core_alpha)[..., None]
    return _srgb(lin).astype(np.uint8)


# ── ship ────────────────────────────────────────────────────────────────────────────────────────────────
def ship_static_item(obj: bpy.types.Object, asset: str, category: str, pal, outline_class: str,
                     sockets: Sequence[dict] = (), game_ids: Sequence[str] = (), extra: dict | None = None,
                     palettes: Sequence = ()) -> dict:
    """FBX + manifest entry (merging writer; ``palettes`` = extra atlas variants to register)."""
    fbx = export.export_static_mesh(obj, asset, category, sockets)
    man = export.Manifest()
    man.set_palette(pal)
    for p in palettes:
        man.set_palette(p)
    e = export.static_entry(obj, fbx, category, pal, outline_class, game_ids, sockets, extra=extra)
    if outline_class in outline.INK_MIX and len(e["materialSlots"]) > 1:
        e["materialSlots"][1]["outlineMix"] = outline.INK_MIX[outline_class]
    man.set_asset(asset, e)
    man.save()
    return e


def update_entries(updates: dict[str, dict]) -> None:
    """Merge extra fields into already shipped manifest entries (previews, inkGate …)."""
    man = export.Manifest()
    for name, fields in updates.items():
        if name in man.data["assets"]:
            man.data["assets"][name].update(fields)
    man.save()


def verify_fbx(asset: str, category: str, expect_tris: int, sockets: Sequence[str], family: str) -> list[str]:
    """Re-import an exported SM into the current scene, check slots / triangles / sockets / AF_Data / custom normals,
    delete it again. Returns the failures (empty = OK)."""
    fails = []
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(paths.category_dir(category) / f"{asset}.fbx"))
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == "MESH"]
    if len(meshes) != 1:
        fails.append(f"{asset}: {len(meshes)} meshes")
    else:
        me = meshes[0].data
        mats = [m.name.split(".")[0] for m in me.materials]
        if mats[:1] != [f"MI_AF_Toon_{family}"] or (len(mats) > 1 and mats[1] != "M_AF_Outline"):
            fails.append(f"{asset}: material slots {mats}")
        tris = sum(p.loop_total - 2 for p in me.polygons)
        if tris != expect_tris:
            fails.append(f"{asset}: triangles {tris} != {expect_tris}")
        if "AF_Data" not in me.color_attributes:
            fails.append(f"{asset}: no AF_Data")
        if not me.has_custom_normals:
            fails.append(f"{asset}: no custom normals")
        names = {o.name.split(".")[0] for o in new if o.type == "EMPTY"}
        for s in sockets:
            if f"SOCKET_{s}" not in names:
                fails.append(f"{asset}: socket {s} missing ({sorted(names)})")
    for o in new:
        bpy.data.objects.remove(o)
    return fails


# ── small geometry helpers ──────────────────────────────────────────────────────────────────────────────
def revolve(profile_rz: Sequence[tuple[float, float]], sides: int = 16, exp: float = 2.0, cap0="fan", cap1="fan",
            squash_y: float = 1.0, phase: float = 0.0) -> M.Part:
    """Revolve ``(radius, z)`` round +Z from explicit rings (``loft_rings``). Unlike ``kit.mesh.lathe`` (a sweep
    along the ring centres) consecutive profile points may share a height — flat shoulders, coin faces, band
    edges: the sweep's end tangent is zero there and its first / last ring collapses into a line (open caps,
    flipped hull triangles drawn over the surface). Caps: 'fan' or None; closed results get outward normals."""
    prof = M.superellipse(1.0, squash_y, sides, exp, phase)
    rings = [[(r * x, r * y, z) for x, y in prof] for r, z in profile_rz]
    return loft_rings(rings, cap0=None if cap0 is None else "fan", cap1=None if cap1 is None else "fan")


def lathe_y(profile_ry: Sequence[tuple[float, float]], sides: int = 12, exp: float = 2.0, cap0="fan", cap1="fan",
            squash: float = 1.0) -> M.Part:
    """Revolve ``(radius, y)`` (bottom → top along +Y) — grips, pommels, collars on an item's +Y axis."""
    p = revolve([(r, y) for r, y in profile_ry], sides=sides, exp=exp, cap0=cap0, cap1=cap1, squash_y=squash)
    return p.rotate("x", -90)          # revolve +Z → item +Y


def ring_y(y: float, r: float, h: float, t: float, sides: int = 12) -> M.Part:
    """A band of height ``h`` round the +Y axis at ``y`` (outer radius ``r``, wall ``t``)."""
    return lathe_y([(r - t, y - h / 2), (r, y - h / 2), (r, y + h / 2), (r - t, y + h / 2)], sides=sides,
                   cap0="fan", cap1="fan").auto_sharp(50)


def ell(r, center, sides=12, rings=7) -> M.Part:
    return M.ellipsoid(r, center, sides=sides, rings=rings)


def frame_y(origin: Vector, y_axis: Vector, z_hint: Vector) -> Matrix:
    """4×4 frame whose +Y is ``y_axis`` and +Z leans to ``z_hint``."""
    y = Vector(y_axis).normalized()
    z = Vector(z_hint) - y * Vector(z_hint).dot(y)
    z.normalize()
    x = y.cross(z)
    m = Matrix((x, y, z)).transposed().to_4x4()
    m.translation = Vector(origin)
    return m


def loft_rings(rings: Sequence[Sequence[Sequence[float]]], cap0: str | None = "fan", cap1: str | None = "fan",
               dome0: float = 0.0, dome1: float = 0.0) -> M.Part:
    """Closed loft through explicit rings (each a CCW-from-above loop of 3-D points, all the same count), bottom →
    top — lumpy sacks, pleated necks, flasks with painted bands. Fan caps at the ring centroids (± dome along Z)."""
    bm = bmesh.new()
    vs = [[bm.verts.new(tuple(p)) for p in r] for r in rings]
    n = len(vs[0])
    for r0, r1 in zip(vs, vs[1:]):
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((r0[i], r0[j], r1[j], r1[i]))
    for cap, ring, dome, sgn in ((cap0, vs[0], dome0, -1.0), (cap1, vs[-1], dome1, 1.0)):
        if cap is None:
            continue
        c = sum((v.co for v in ring), Vector()) / len(ring)
        cv = bm.verts.new(c + Vector((0, 0, sgn * dome)))
        order = list(reversed(ring)) if sgn < 0 else list(ring)
        for i in range(n):
            bm.faces.new((order[i], order[(i + 1) % n], cv))
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return M.Part(bm).smooth(True)


def surface_strip(points: Sequence[Vector], normals: Sequence[Vector], width: float, lift: float = 0.0008,
                  depth: float = 0.0025) -> M.Part:
    """A painted strip lying on a surface (crease lines, veins, highlight streaks): ``points`` on the surface with
    their outward ``normals``; stands ``lift`` proud, sinks ``depth`` (closed solid)."""
    o3, i3 = [], []
    for k, (p, n) in enumerate(zip(points, normals)):
        p, n = Vector(p), Vector(n).normalized()
        t = (Vector(points[min(k + 1, len(points) - 1)]) - Vector(points[max(k - 1, 0)])).normalized()
        s = t.cross(n).normalized()
        o3.append([p + n * lift - s * width / 2, p + n * lift, p + n * lift + s * width / 2])
        i3.append([p - n * depth - s * width / 2, p - n * depth, p - n * depth + s * width / 2])
    import common as _C
    return _C.thick_patch(o3, i3)

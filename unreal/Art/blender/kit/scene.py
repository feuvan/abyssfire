"""Scene utilities: factory reset, units, deterministic seeds, object/collection helpers.

Units (spec art-inventory-ch1.md §2.3): Metric, unit scale 1.0, **1 Blender unit = 1 m = 1 tile**.
The FBX exporter (kit.export) writes centimetres for UE (``apply_unit_scale`` + ``FBX_SCALE_ALL``), so
1 m in Blender arrives as 100 uu in UE. Characters face **−Y**, +Z up, pivot on the ground between the feet.
"""
from __future__ import annotations

import hashlib
import random
from typing import Iterable, Sequence

import bpy
from mathutils import Matrix, Vector

FPS = 60  # spec §0.2: animation authored at 60 fps, frame = ms × 0.06


def reset(fps: int = FPS) -> bpy.types.Scene:
    """Factory-reset to an empty scene with the kit's unit / frame-rate conventions."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    us = sc.unit_settings
    us.system = "METRIC"
    us.scale_length = 1.0
    us.length_unit = "METERS"
    sc.render.fps = fps
    sc.render.fps_base = 1.0
    sc.frame_start = 0
    sc.frame_end = 0
    sc.frame_current = 0
    return sc


def seed_for(*parts: object) -> int:
    """Stable 32-bit seed from any identifying parts (asset name, variant…). Never use hash()."""
    h = hashlib.sha1("|".join(str(p) for p in parts).encode("utf-8")).digest()
    return int.from_bytes(h[:4], "little")


def rng(*parts: object) -> random.Random:
    """Deterministic RNG for a generator (same asset name → same output on every machine)."""
    return random.Random(seed_for(*parts))


def ms_to_frame(ms: float, fps: int = FPS) -> int:
    return int(round(ms * fps / 1000.0))


def frame_to_ms(frame: float, fps: int = FPS) -> float:
    return frame * 1000.0 / fps


def collection(name: str, parent: bpy.types.Collection | None = None) -> bpy.types.Collection:
    col = bpy.data.collections.get(name)
    if col is None:
        col = bpy.data.collections.new(name)
        (parent or bpy.context.scene.collection).children.link(col)
    return col


def link(obj: bpy.types.Object, col: bpy.types.Collection | None = None) -> bpy.types.Object:
    (col or bpy.context.scene.collection).objects.link(obj)
    return obj


def ensure_object_mode() -> None:
    if bpy.context.object is not None and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")


def select_only(objs: Iterable[bpy.types.Object], active: bpy.types.Object | None = None) -> None:
    ensure_object_mode()
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    objs = list(objs)
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = active or (objs[0] if objs else None)


def apply_transforms(obj: bpy.types.Object, location=True, rotation=True, scale=True) -> None:
    select_only([obj])
    bpy.ops.object.transform_apply(location=location, rotation=rotation, scale=scale)


def remove(obj: bpy.types.Object) -> None:
    data = obj.data
    bpy.data.objects.remove(obj, do_unlink=True)
    if data is not None and getattr(data, "users", 1) == 0:
        if isinstance(data, bpy.types.Mesh):
            bpy.data.meshes.remove(data)
        elif isinstance(data, bpy.types.Armature):
            bpy.data.armatures.remove(data)


def world_bounds(objs: Sequence[bpy.types.Object], evaluated: bool = True) -> tuple[Vector, Vector]:
    """Axis-aligned world bounds (metres) of mesh objects, evaluated (posed) by default."""
    dg = bpy.context.evaluated_depsgraph_get()
    lo = Vector((1e9, 1e9, 1e9))
    hi = Vector((-1e9, -1e9, -1e9))
    found = False
    for o in objs:
        if o.type != "MESH":
            continue
        ob = o.evaluated_get(dg) if evaluated else o
        me = ob.to_mesh()
        mw = ob.matrix_world
        for v in me.vertices:
            p = mw @ v.co
            lo = Vector((min(lo.x, p.x), min(lo.y, p.y), min(lo.z, p.z)))
            hi = Vector((max(hi.x, p.x), max(hi.y, p.y), max(hi.z, p.z)))
            found = True
        ob.to_mesh_clear()
    if not found:
        return Vector((0, 0, 0)), Vector((0, 0, 0))
    return lo, hi


def blender_to_ue_cm(v: Sequence[float]) -> list[float]:
    """Blender metres (X right, Y back, Z up; right-handed) → UE centimetres (left-handed, Y flipped)."""
    return [round(v[0] * 100.0, 3), round(-v[1] * 100.0, 3), round(v[2] * 100.0, 3)]


def blender_dir_to_ue(v: Sequence[float]) -> list[float]:
    return [v[0], -v[1], v[2]]


def look_at_matrix(eye: Vector, target: Vector, up: Vector = Vector((0, 0, 1))) -> Matrix:
    """Camera-style world matrix (−Z looks at target, +Y up)."""
    f = (target - eye).normalized()
    r = f.cross(up)
    if r.length < 1e-6:
        r = f.cross(Vector((0, 1, 0)))
    r.normalize()
    u = r.cross(f)
    m = Matrix((
        (r.x, u.x, -f.x, eye.x),
        (r.y, u.y, -f.y, eye.y),
        (r.z, u.z, -f.z, eye.z),
        (0, 0, 0, 1),
    ))
    return m


def save_blend(asset: str, family: str) -> str:
    """Save the generated scene to ``Art/blender/source/<family>/<asset>.blend`` (spec §2.1; regenerable,
    git-ignored) — handy for inspecting a generator's output in the Blender UI."""
    from . import paths
    p = paths.BLENDER_DIR / "source" / family / f"{asset}.blend"
    p.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(p), compress=True)
    return str(p)

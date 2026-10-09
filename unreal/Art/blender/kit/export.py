"""FBX export for UE 5.8 (Interchange) + ``Art/Export/manifest.json`` (spec §2.2–§2.5, ARCHITECTURE §5).

Settings (spec §2.3): Metric, 1 BU = 1 m; ``apply_unit_scale=True``, ``apply_scale_options='FBX_SCALE_ALL'``,
``global_scale=1.0`` → 1 m = 100 uu; ``axis_forward='-Z'``, ``axis_up='Y'`` (characters face −Y in Blender →
+Y in UE); ``add_leaf_bones=False``; ``use_armature_deform_only=True`` (every exported bone is flagged deform —
IK helpers / weapon bones simply carry no weights); ``primary_bone_axis='Y'``, ``secondary_bone_axis='X'``;
``mesh_smooth_type='FACE'`` with custom normals exported (hull outward normals); ``bake_anim_simplify_factor=0``.
The armature object is exported under the name ``Armature`` (UE drops it instead of adding an extra root bone).

Files:
* ``SK_<…>.fbx``  armature + skinned mesh in bind pose, no animation;
* ``A_<…>_<Action>.fbx``  armature only, one action per file, take named like the file, frame range = clip;
* ``SM_<…>.fbx``  static mesh (+ ``SOCKET_<name>`` empties, ``UCX_<SM>_NN`` collision);
  UE import: *Import Normals* (not "compute"), *Force Front X Axis* off, convert scene on, no material import
  (materials are built from the manifest).
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Iterable, Sequence

import bpy
from mathutils import Vector

from . import mesh as kmesh
from . import outline, paths, rig as krig, scene, shading

KIT_VERSION = "1.0.0"
SCHEMA_VERSION = 1

FBX_COMMON = dict(
    apply_unit_scale=True,
    apply_scale_options="FBX_SCALE_ALL",
    global_scale=1.0,
    axis_forward="-Z",
    axis_up="Y",
    add_leaf_bones=False,
    use_armature_deform_only=True,
    primary_bone_axis="Y",
    secondary_bone_axis="X",
    armature_nodetype="NULL",
    mesh_smooth_type="FACE",
    use_mesh_modifiers=True,
    use_custom_props=False,
    use_tspace=False,
    use_triangles=False,
    colors_type="LINEAR",
    prioritize_active_color=True,
    path_mode="STRIP",
    embed_textures=False,
    use_selection=True,
)


class _ArmatureName:
    """Temporarily rename the armature object to ``Armature`` (restored on exit)."""

    def __init__(self, arm_obj: bpy.types.Object):
        self.obj = arm_obj
        self.old = arm_obj.name
        self.clash = bpy.data.objects.get("Armature") if self.old != "Armature" else None

    def __enter__(self):
        if self.clash is not None:
            self.clash.name = "_ArmatureClash"
        self.obj.name = "Armature"
        return self

    def __exit__(self, *exc):
        self.obj.name = self.old
        if self.clash is not None:
            self.clash.name = "Armature"


def _strip_preview(objs: Iterable[bpy.types.Object]) -> None:
    for o in objs:
        if o.type == "MESH":
            outline.remove_preview(o)


def export_skeletal_mesh(rig: krig.Rig, meshes: Sequence[bpy.types.Object], asset: str, category: str) -> Path:
    """Export ``SK_<…>.fbx`` (bind pose, no animation)."""
    out = paths.category_dir(category) / f"{asset}.fbx"
    out.parent.mkdir(parents=True, exist_ok=True)
    _strip_preview(meshes)
    from . import anim
    anim.assign(rig, None)
    anim.clear_pose(rig)
    scene.select_only([rig.obj, *meshes], active=rig.obj)
    with _ArmatureName(rig.obj):
        bpy.ops.export_scene.fbx(filepath=str(out), object_types={"ARMATURE", "MESH"}, bake_anim=False,
                                 **FBX_COMMON)
    return out


def export_animation(rig: krig.Rig, action: bpy.types.Action, category: str) -> Path:
    """Export one action as ``<action name>.fbx`` (armature only; take = file name)."""
    from . import anim
    out = paths.category_dir(category) / f"{action.name}.fbx"
    out.parent.mkdir(parents=True, exist_ok=True)
    sc = bpy.context.scene
    anim.assign(rig, action)
    sc.frame_start, sc.frame_end = int(action.frame_start), int(action.frame_end)
    old_scene_name = sc.name
    sc.name = action.name
    scene.select_only([rig.obj], active=rig.obj)
    try:
        with _ArmatureName(rig.obj):
            bpy.ops.export_scene.fbx(filepath=str(out), object_types={"ARMATURE"}, bake_anim=True,
                                     bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                                     bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True,
                                     bake_anim_step=1.0, bake_anim_simplify_factor=0.0, **FBX_COMMON)
    finally:
        sc.name = old_scene_name
    return out


def export_static_mesh(obj: bpy.types.Object, asset: str, category: str,
                       sockets: Sequence[dict] = (), collision: Sequence[bpy.types.Object] = ()) -> Path:
    """Export ``SM_<…>.fbx``; ``sockets`` = [{'name', 'pos' (m), 'rot' (deg)}] → ``SOCKET_<name>`` empties."""
    out = paths.category_dir(category) / f"{asset}.fbx"
    out.parent.mkdir(parents=True, exist_ok=True)
    _strip_preview([obj])
    old = obj.name
    obj.name = asset
    temp = []
    for s in sockets:
        e = bpy.data.objects.new(f"SOCKET_{s['name']}", None)
        scene.link(e)
        e.empty_display_size = 0.05
        e.location = Vector(s["pos"])
        e.rotation_euler = [__import__("math").radians(a) for a in s.get("rot", (0, 0, 0))]
        e.parent = obj
        temp.append(e)
    for i, c in enumerate(collision):
        c.name = f"UCX_{asset}_{i:02d}"
    try:
        scene.select_only([obj, *temp, *collision], active=obj)
        bpy.ops.export_scene.fbx(filepath=str(out), object_types={"MESH", "EMPTY"}, bake_anim=False,
                                 **FBX_COMMON)
    finally:
        for e in temp:
            bpy.data.objects.remove(e)
        obj.name = old
    return out


# ── manifest ────────────────────────────────────────────────────────────────────────────────────────────
class Manifest:
    """``Art/Export/manifest.json``: shading constants, palettes, assets keyed by UE asset name.

    The file is **shared by every generator** (several may run at once): ``save()`` takes an exclusive lock
    (``manifest.json.lock``), re-reads the file and **merges** — in ``assets``, ``palettes`` and ``gameIds`` only
    the keys this instance added, changed (``set_asset`` / ``set_palette`` / direct edits of ``data``) or deleted
    since it was loaded are written; everything else on disk (other generators' entries) is kept. The kit-owned
    sections (``shading``, ``units``, ``schemaVersion``, ``generator``) are always rewritten from the kit. The
    write is atomic (temp file + rename)."""

    MERGED = ("assets", "palettes", "gameIds")

    def __init__(self, path: Path | None = None):
        self.path = Path(path) if path else paths.manifest_path()
        self.data = self._read()
        for k in self.MERGED:
            self.data.setdefault(k, {})
        self._base = json.loads(json.dumps({k: self.data[k] for k in self.MERGED}))
        self._stamp()

    def _read(self) -> dict:
        return json.loads(self.path.read_text()) if self.path.exists() else {}

    def _stamp(self) -> None:
        self.data["schemaVersion"] = SCHEMA_VERSION
        self.data["generator"] = f"abyssfire blender kit {KIT_VERSION} (Blender {bpy.app.version_string})"
        self.data["units"] = {"blender": "1 BU = 1 m", "ue": "cm", "axes": "UE (X fwd, Y right, Z up)",
                              "characterForward": "+Y (mesh); actor rotates the mesh yaw -90",
                              "fps": scene.FPS}
        sh = shading.shading_manifest()
        sh["outline"] = outline.outline_manifest()
        self.data["shading"] = sh

    def set_palette(self, pal) -> None:
        self.data["palettes"][pal.family] = pal.manifest_entry()

    def set_asset(self, name: str, entry: dict) -> None:
        self.data["assets"][name] = entry
        for gid in entry.get("gameIds", []):
            self.data["gameIds"][gid] = name

    def save(self) -> Path:
        import fcntl
        import os
        self.path.parent.mkdir(parents=True, exist_ok=True)
        lock = self.path.with_name(self.path.name + ".lock")
        with open(lock, "w") as lk:
            fcntl.flock(lk, fcntl.LOCK_EX)
            try:
                disk = self._read()
                ours = json.loads(json.dumps({k: self.data[k] for k in self.MERGED}))
                merged = dict(disk)
                for sec in self.MERGED:
                    base, mine, out = self._base.get(sec, {}), ours[sec], dict(disk.get(sec, {}))
                    for k, v in mine.items():
                        if k not in base or base[k] != v:
                            out[k] = v                     # added / changed here
                    for k in base:
                        if k not in mine:
                            out.pop(k, None)               # deleted here
                    merged[sec] = out
                for k, v in self.data.items():
                    if k not in self.MERGED:
                        merged[k] = v
                tmp = self.path.with_name(f".{self.path.name}.{os.getpid()}.tmp")
                tmp.write_text(json.dumps(merged, indent=1, sort_keys=True, ensure_ascii=True) + "\n")
                os.replace(tmp, self.path)
            finally:
                fcntl.flock(lk, fcntl.LOCK_UN)
        self.data = merged
        self._base = json.loads(json.dumps({k: merged[k] for k in self.MERGED}))
        return self.path


def bounds_ue(objs: Sequence[bpy.types.Object]) -> dict:
    lo, hi = scene.world_bounds(objs)
    a, b = scene.blender_to_ue_cm(lo), scene.blender_to_ue_cm(hi)
    return {"min": [min(a[i], b[i]) for i in range(3)], "max": [max(a[i], b[i]) for i in range(3)]}


def clip_entry(clip, action: bpy.types.Action, fbx: Path | None) -> dict:
    e = {
        "name": clip.name,
        "asset": action.name,
        "lengthMs": round(clip.length_ms, 1),
        "frames": clip.frames,
        "fps": scene.FPS,
        "loop": clip.loop,
        "notifies": clip.notify_list(),
    }
    if fbx is not None:
        e["fbx"] = paths.rel_to_export(fbx)
    if "Contact" in clip.notifies:
        e["contactMs"] = round(float(clip.notifies["Contact"]), 1)
    if "Release" in clip.notifies:
        e["releaseMs"] = round(float(clip.notifies["Release"]), 1)
    if clip.additive:
        e["additive"] = True
    if clip.ref_speed:
        e["refSpeedCmS"] = round(clip.ref_speed * 100.0, 1)
    return e


def skeletal_entry(rig: krig.Rig, body: bpy.types.Object, fbx: Path, category: str, palette,
                   outline_class: str, anims: list[dict], game_ids: Sequence[str] = (),
                   blob_radius_cm: float | None = None, extra: dict | None = None) -> dict:
    width = float(body.get("af_outline_width", 0.0))
    entry = {
        "kind": "SkeletalMesh",
        "category": category,
        "fbx": paths.rel_to_export(fbx),
        "gameIds": list(game_ids),
        "skeleton": rig.spec.skeleton,
        "scale": 1.0,
        "heightCm": round(rig.spec.crown_height() * 100.0, 1),
        "boundsCm": bounds_ue([body]),
        "bones": len(rig.obj.data.bones),
        "triangles": {"toon": kmesh.tri_count(body, 0), "outline": kmesh.tri_count(body, 1)},
        "materialSlots": [
            {"index": 0, "name": palette.material_name, "parent": "M_AF_Toon", "palette": palette.family},
            {"index": 1, "name": shading.OUTLINE_MATERIAL,
             "parent": "M_AF_Outline", "palette": palette.family, "class": outline_class,
             "widthCm": round(width * 100.0, 2),
             "px1080AtDefaultDistance": round(outline.default_px_at_game_distance(width), 2),
             "outlinePx1080": outline.SCREEN_PX_1080.get(outline_class, 0.0)},
        ],
        "sockets": krig.socket_manifest(rig),
        "anims": anims,
    }
    if blob_radius_cm is not None:
        entry["blobShadowRadiusCm"] = blob_radius_cm
    if extra:
        entry.update(extra)
    return entry


def static_entry(obj: bpy.types.Object, fbx: Path, category: str, palette, outline_class: str,
                 game_ids: Sequence[str] = (), sockets: Sequence[dict] = (), footprint_tiles=None,
                 blocking: bool | None = None, extra: dict | None = None) -> dict:
    width = float(obj.get("af_outline_width", 0.0))
    entry = {
        "kind": "StaticMesh",
        "category": category,
        "fbx": paths.rel_to_export(fbx),
        "gameIds": list(game_ids),
        "scale": 1.0,
        "boundsCm": bounds_ue([obj]),
        "triangles": {"toon": kmesh.tri_count(obj, 0), "outline": kmesh.tri_count(obj, 1)},
        "materialSlots": [
            {"index": 0, "name": palette.material_name, "parent": "M_AF_Toon", "palette": palette.family},
        ] + ([{"index": 1, "name": "M_AF_Outline", "parent": "M_AF_Outline", "palette": palette.family,
               "class": outline_class, "widthCm": round(width * 100.0, 2),
               "outlinePx1080": outline.SCREEN_PX_1080.get(outline_class, 0.0)}] if width > 0 else []),
        "sockets": [{"name": s["name"], "locCm": scene.blender_to_ue_cm(s["pos"]),
                     "rotDeg": list(s.get("rot", (0, 0, 0)))} for s in sockets],
    }
    if footprint_tiles is not None:
        entry["footprintTiles"] = list(footprint_tiles)
    if blocking is not None:
        entry["blocking"] = blocking
    if extra:
        entry.update(extra)
    return entry

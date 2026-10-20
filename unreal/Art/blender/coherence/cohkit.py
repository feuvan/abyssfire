"""Shared helpers of the Chapter-1 coherence pass (``coherence/lineup.py``, ``coherence/vignettes.py``).

Everything is **re-imported from ``Art/Export``** (what UE receives): skeletal meshes posed by their exported clip,
held items / body items / variant attachments on their sockets, static meshes, each drawn with its own family palette
(read from the shipped PNG atlases) and its manifest outline class at the UE screen-constant ink
(``outlinePx1080`` of the slot; Props_Plains decor ``envkit.PX_OVERRIDE``). Placement follows the runtime
(``AAbyssZoneActor`` / ``UAbyssWorldBuilder``: mesh yaw −90°, camp props facing the fire, variants by tile hash,
the elite crown on ``fx_overhead`` at absolute scale, yawed to the camera). The ground is the shipped plains terrain
set composited with the exact transition rules (``environment/terrain.composite``) plus the web contact shadows.

**Presentation theme** (dark remake U1, ``kit/theme.py``): ``set_theme("march")`` makes every lookup resolve
``<id>@march`` before ``<id>`` as the runtime does for ``emerald_plains`` (``variant_names``), stands hunt variants under
the themed elite crown (``elite_crown@march`` → ``SM_FX_EliteCrown_March``; the plains crown was hard-coded before) and
composites the ground from the theme's shipped terrain set (``environment/terrain_march``: the march textures with the
march ranks, lips and liquid colours). The default (``None``) is the plains look of the Phase-0 coherence pass.
"""
from __future__ import annotations

import json
import math
import os
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from types import SimpleNamespace

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "environment", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import outline, paths, pngio, review, scene, shading  # noqa: E402
from kit import rig as krig  # noqa: E402

import envkit as E  # noqa: E402
import terrain as T  # noqa: E402

SCRATCH = Path(os.environ.get("AF_SCRATCH_ROOT", tempfile.gettempdir())) / "abyss_art_coh"   # ground cache (never ships)
MESH_YAW = -90.0                          # AAbyssZoneActor::MeshYawOffsetDeg
CHAR_GATE_PX = 3.0                        # Chapter-1 character gate: a dark rim >= 3 px on >= 90 % of the
                                          # silhouette edges at 1080p (heroes, monsters, NPCs, the companion)
CHAR_CLASSES = ("hero", "boss", "monster", "small_monster", "npc")
EMISSIVE_EDGE_MIN = 0.25                  # linear bloom-source level (lin(c)·e·(1 + 0.6 e)) of an FX accent whose edge
                                          # the ink gate leaves out: the spirit-fire, glints; not a faintly lit body
                                          # (the gel's e .10 body ≈ 0.08)
_MAN: dict | None = None
_MATS: dict[str, tuple] = {}
THEME: str | None = None                  # presentation theme of the lookups (None = the plains base ids)
PLAINS_CROWN = "SM_FX_EliteCrown"


def set_theme(theme: str | None) -> None:
    """Resolve themed ids (``<id>@<theme>`` first), the elite crown and the ground under ``theme``."""
    global THEME
    THEME = None if theme in (None, "", "plains") else theme


def manifest() -> dict:
    global _MAN
    if _MAN is None:
        _MAN = json.loads(paths.manifest_path().read_text())
    return _MAN


def entry(name: str) -> dict:
    return manifest()["assets"][name]


def variant_names(game_id: str, theme: str | None = "__module__") -> list[str]:
    """Every asset listing ``<game_id>@<theme>`` (sorted by name, the runtime's variant order), else every asset
    listing ``<game_id>`` — the runtime lookup (U1) under the presentation theme."""
    th = THEME if theme == "__module__" else theme
    assets = manifest()["assets"]
    cands = ([f"{game_id}@{th}"] if th else []) + [game_id]
    for gid in cands:
        names = sorted(n for n, e in assets.items() if gid in (e.get("gameIds") or []))
        if names:
            return names
    return []


def crown_asset() -> str:
    """The elite crown the runtime stands over an elite under the current theme (``elite_crown@<theme>``)."""
    names = variant_names("elite_crown")
    return names[0] if names else PLAINS_CROWN


def reset() -> None:
    """``scene.reset()`` + drop the cached preview materials (freed by the reset: a stale reference segfaults)."""
    _MATS.clear()
    scene.reset()


def mats(family: str):
    if family in _MATS:
        try:
            _MATS[family][0].name
        except ReferenceError:
            _MATS.pop(family)
    if family not in _MATS:
        _MATS[family] = E.toon_material_files(family)
    return _MATS[family]


def slot_info(e: dict) -> tuple[str, str, float, float]:
    """(palette, outline class, screen px at 1080p, baked width m) of a manifest entry."""
    slots = e["materialSlots"]
    fam = slots[0].get("palette") or "Props_Plains"
    hull = next((s for s in slots if s.get("index", 0) >= 1 and "class" in s
                 and str(s.get("parent", "")).startswith("M_AF_Outline")), None)     # M_AF_Outline(_Foliage)
    if hull is None:
        return fam, "none", 0.0, 0.0
    cls = hull.get("class", "decor")
    px = E.PX_OVERRIDE.get((fam, cls), hull.get("outlinePx1080", outline.SCREEN_PX_1080.get(cls, 1.2)))
    return fam, cls, float(px), hull.get("widthCm", outline.WIDTH_CM.get(cls, 1.2)) / 100.0


def _import(fbx: Path) -> list:
    """Import ``fbx``; a clip file whose bone nodes carry a static scale is imported from a patched copy (see
    :func:`_unit_static_scale`), so the re-imported pose is the one UE plays."""
    src, tmp = fbx, _unit_static_scale(fbx)
    before = set(bpy.data.objects)
    try:
        bpy.ops.import_scene.fbx(filepath=str(tmp or src))
    finally:
        if tmp is not None:
            tmp.unlink(missing_ok=True)
    return [o for o in bpy.data.objects if o not in before]


def _fbx_records(data: bytes, pos: int, end: int, wide: bool):
    """Binary FBX node records in ``data[pos:end]`` → [(name, props, children)]; props = [(type, value, offset)]
    (the offset of a scalar's value bytes, so a value can be patched in place)."""
    import struct
    out = []
    hw = 8 if wide else 4
    while pos < end:
        if wide:
            e_off, n_props, plen = struct.unpack_from("<QQQ", data, pos)
        else:
            e_off, n_props, plen = struct.unpack_from("<III", data, pos)
        if e_off == 0:
            break
        nlen = data[pos + 3 * hw]
        name = bytes(data[pos + 3 * hw + 1: pos + 3 * hw + 1 + nlen])
        q = pos + 3 * hw + 1 + nlen
        props = []
        for _ in range(n_props):
            t = chr(data[q])
            q += 1
            if t in "SR":
                ln = struct.unpack_from("<I", data, q)[0]
                props.append((t, bytes(data[q + 4:q + 4 + ln]), q + 4))
                q += 4 + ln
            elif t in "fdlib":
                _n, _enc, clen = struct.unpack_from("<III", data, q)
                props.append((t, None, q))
                q += 12 + clen
            else:
                size, fmt = {"Y": (2, "<h"), "C": (1, "<?"), "I": (4, "<i"), "F": (4, "<f"), "D": (8, "<d"),
                             "L": (8, "<q")}[t]
                props.append((t, struct.unpack_from(fmt, data, q)[0], q))
                q += size
        kids = _fbx_records(data, q, e_off, wide) if q < e_off else []
        out.append((name, props, kids))
        pos = e_off
    return out


def _unit_static_scale(fbx: Path) -> Path | None:
    """A copy of clip file ``fbx`` with every bone node's static ``Lcl Scaling`` set to 1, or None when there is none
    to fix. The exporter writes the clip's first pose as the nodes' static transform, so a bone keyed at a scale there
    (the slime family's X-eyes / lids at .05, the cinderbloat's fuse ridges at .55 and burst shards at .001, the
    sprites' mouth, cloth chains' stretch) carries it as its static scale. Blender's FBX importer builds the rest from
    the static transform and keys the pose relative to it, but an edit bone cannot hold a scale: the scale is dropped
    from the rest while the keys stay divided by it, so pose space came out ``1 / static`` too large (the cinderbloat's
    ridges ×1.82 through its fuse, its shards ×1000 after the burst; every hidden part shown at frame 0). UE evaluates
    the curves — absolute local transforms, which this leaves untouched — against the skeleton's bind pose; with unit
    static scales the importer's pose space is exactly that."""
    import struct
    data = bytearray(fbx.read_bytes())
    if not data.startswith(b"Kaydara FBX Binary") or b"AnimationCurve" not in data:
        return None
    version = struct.unpack_from("<I", data, 23)[0]
    recs = _fbx_records(data, 27, len(data), version >= 7500)
    patched = 0
    for name, _p, kids in recs:
        if name != b"Objects":
            continue
        for mname, mprops, mkids in kids:
            if mname != b"Model" or len(mprops) < 3 or mprops[2][1] != b"LimbNode":
                continue
            for pname, _pp, pkids in mkids:
                if pname != b"Properties70":
                    continue
                for _n, pr, _k in pkids:
                    if pr and pr[0][1] == b"Lcl Scaling":
                        ds = [x for x in pr if x[0] == "D"][:3]
                        if len(ds) == 3 and max(abs(x[1] - 1.0) for x in ds) > 1e-9:
                            for _t, _v, off in ds:
                                struct.pack_into("<d", data, off, 1.0)
                            patched += 1
    if not patched:
        return None
    out = SCRATCH / "clip_fbx" / f"{os.getpid()}_{fbx.name}"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(bytes(data))
    return out


def _dither_shell(mt, opacity: float):
    """``MI_AF_Toon_Slime`` preview (as monsters/lineup): the toon at ``opacity``, back faces culled."""
    m = mt.copy()
    m.name = "AF_Coh_SlimeShell"
    nt = m.node_tree
    em = next(n for n in nt.nodes if n.type == "EMISSION")
    out = next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL")
    tr = nt.nodes.new("ShaderNodeBsdfTransparent")
    mix = nt.nodes.new("ShaderNodeMixShader")
    mix.inputs["Fac"].default_value = opacity
    nt.links.new(tr.outputs[0], mix.inputs[1])
    nt.links.new(em.outputs[0], mix.inputs[2])
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    cull = nt.nodes.new("ShaderNodeMixShader")
    tr2 = nt.nodes.new("ShaderNodeBsdfTransparent")
    nt.links.new(geo.outputs["Backfacing"], cull.inputs["Fac"])
    nt.links.new(mix.outputs[0], cull.inputs[1])
    nt.links.new(tr2.outputs[0], cull.inputs[2])
    nt.links.new(cull.outputs[0], out.inputs["Surface"])
    m["af_toon_slot"] = True
    return m


def _rim_override(e: dict, name: str):
    """(colour, alpha) of a manifest slot whose instance overrides the rim (``params`` ``RimOverrideColor``, with
    ``RimOverride`` > 0), else None — e.g. the breach-blood shells / crust (monsters/breach.py)."""
    base = name.split(".")[0]                      # an imported material may come back as "<name>.001"
    for sl in e.get("materialSlots", []):
        if sl.get("name") in (name, base):
            p = sl.get("params") or {}
            if p.get("RimOverrideColor") and float(p.get("RimOverride", 1.0)) > 0:
                return _rim_hex(p["RimOverrideColor"]), float(p.get("RimOverrideAlpha", shading.RIM_ALPHA))
    return None


def _rim_hex(v) -> str:
    """A manifest ``RimOverrideColor`` as "#RRGGBB": a hex string, or a list of **sRGB-encoded** floats — the
    importer workaround of README §23 (the HLSL input is sRGB; a list passes the UE importer unchanged)."""
    if isinstance(v, (list, tuple)):
        return "#" + "".join(f"{int(round(max(0.0, min(1.0, float(c))) * 255)):02X}" for c in v[:3])
    return str(v)


def _with_rim(m, rim):
    """``m`` with its toon core swapped for the rim-override core (``shading.rim_override_group``)."""
    grp = next(n for n in m.node_tree.nodes if n.type == "GROUP")
    grp.node_tree = shading.rim_override_group(*rim)
    return m


def _assign(mesh, e: dict) -> None:
    """Family toon / outline (/ slime shell) by the imported material names, back in manifest slot order. A slot
    whose manifest ``params`` override the rim keeps that rim: a slime-type shell gets it on the dithered shell, a
    toon slot becomes its own material (index 3)."""
    fam, cls, px, width = slot_info(e)
    mt, mo = mats(fam)
    mats_ = mesh.data.materials
    names = [m.name if m is not None else "" for m in mats_]
    want = {i: (1 if "Outline" in nm else 2 if "Slime" in nm else 3 if _rim_override(e, nm) else 0)
            for i, nm in enumerate(names)}
    if len(names) == 0:
        mats_.append(mt)
        want = {0: 0}
    mi = np.zeros(len(mesh.data.polygons), np.int32)
    mesh.data.polygons.foreach_get("material_index", mi)
    if len(mi):
        mesh.data.polygons.foreach_set("material_index", np.array([want.get(int(k), 0) for k in mi], np.int32))
    n_slots = max(want.values()) + 1
    new = [mt, mo]
    if n_slots > 2:
        shell = next((x for x in e["materialSlots"] if "Slime" in str(x.get("name", ""))), None)
        if shell is not None:
            op = (shell.get("params") or {}).get("BodyOpacity", 0.62)
            sm = _dither_shell(mt, op)
            rim = _rim_override(e, shell["name"])
            new.append(_with_rim(sm, rim) if rim else sm)
        else:
            new.append(mt)
    if n_slots > 3:
        rim = next(_rim_override(e, nm) for i, nm in enumerate(names) if want[i] == 3)
        rm = mt.copy()
        rm.name = "AF_Coh_RimToon"
        rm["af_toon_slot"] = True          # an extra toon slot: its own emissive regions in the bloom pass, no more
        new.append(_with_rim(rm, rim))
    while len(mats_) < n_slots:
        mats_.append(None)
    while len(mats_) > n_slots:
        mats_.pop()
    for i in range(len(mats_)):
        mats_[i] = new[i] if i < len(new) else mt
    mesh.data.update()
    if cls != "none":
        mesh["af_outline_width"] = width
        mesh["af_outline_class"] = cls
        mesh["af_px1080"] = px
    mesh["af_family"] = fam


def import_static(name: str):
    """A shipped static mesh (sockets / collision dropped), materials from its family, at the origin."""
    e = entry(name)
    new = _import(paths.export_root() / e["fbx"])
    mesh = next(o for o in new if o.type == "MESH" and not o.name.startswith(("SOCKET_", "UCX_")))
    for o in new:
        if o is not mesh:
            bpy.data.objects.remove(o)
    mesh.parent = None
    mesh.matrix_world = Matrix.Identity(4)
    _assign(mesh, e)
    mesh.name = f"COH_{name}"
    return mesh


@dataclass
class Figure:
    asset: str
    label: str
    root: object
    arm: object
    meshes: list
    cls: str
    px: float
    height_m: float
    extra: list = field(default_factory=list)       # crown etc. (own class)
    family: str = ""
    n_body: int = 1                                   # meshes[:n_body] came from the SK file

    def top_cm(self) -> float:
        """Highest point of the posed skinned body (not held items), cm above the ground."""
        return float(review.mesh_points(self.meshes[: self.n_body])[:, 2].max()) * 100.0

    def all(self) -> list:
        return list(self.meshes) + list(self.extra)


def _socket_world(e: dict, arm, name: str) -> Vector:
    s = next(x for x in e["sockets"] if x["name"] == name)
    rest = Vector((s["restLocCm"][0] / 100.0, -s["restLocCm"][1] / 100.0, s["restLocCm"][2] / 100.0))
    b = arm.data.bones[s["bone"]]
    pb = arm.pose.bones[s["bone"]]
    return arm.matrix_world @ (pb.matrix @ (b.matrix_local.inverted() @ rest))


def import_character(asset: str, at, yaw_deg: float, clip: str | None = None, frame: int = 0,
                     variant: str | None = None, label: str = "", scenery: bool = False,
                     cam=None) -> Figure:
    """A shipped skeletal mesh posed by ``A_<token>_<clip>.fbx`` at ``frame`` (default: the first clip of the
    entry, Idle / IdleNPC), with its attachments, body / child items, an optional hunt ``variant`` (scale,
    attachments, elite crown on ``fx_overhead`` yawed to ``cam``) and optional NPC scenery (the anvil stump)."""
    e = entry(asset)
    root_dir = paths.export_root()
    new = _import(root_dir / e["fbx"])
    arm = next(o for o in new if o.type == "ARMATURE")
    meshes = [o for o in new if o.type == "MESH"]
    clips = {a["name"]: a for a in e.get("anims", [])}
    a = clips.get(clip) if clip else (clips.get("Idle") or clips.get("IdleNPC") or next(iter(clips.values()), None))
    arm2 = None
    if a is not None:
        new2 = _import(root_dir / a["fbx"])
        arm2 = next((o for o in new2 if o.type == "ARMATURE"), None)
        for o in new2:
            o.hide_render = True
        if arm2 is not None:
            for pb in arm.pose.bones:
                c = pb.constraints.new("COPY_TRANSFORMS")
                c.target, c.subtarget = arm2, pb.name
                c.target_space = c.owner_space = "POSE"
            act = arm2.animation_data.action if arm2.animation_data else None
            if act is not None:
                bpy.context.scene.frame_set(int(act.frame_range[0]) + frame)
    for m in meshes:
        _assign(m, e)
    n_body = len(meshes)
    fam, cls, px, _ = slot_info(e)
    rig = SimpleNamespace(obj=arm)
    items: dict[str, object] = {}
    var = (e.get("variants") or {}).get(variant) if variant else None
    atts = list(e.get("attachments", []) or []) + (list(var.get("attachments", [])) if var else [])
    for att in atts:
        it = import_static(att["object"])
        bone = att["socket"]
        if bone not in arm.data.bones:                 # a named socket on another bone (Old Bern's ``barrow`` socket
            sk = next((x for x in e.get("sockets", []) if x["name"] == bone), None)    # on ``barrow_01``, rel 0)
            if sk is None or sk.get("bone") not in arm.data.bones:
                raise KeyError(f"{asset}: attachment socket {bone!r} is neither a bone nor a socket on one")
            if any(abs(float(v)) > 1e-6 for v in list(sk.get("relLocCm", [])) + list(sk.get("relRotDeg", []))):
                raise NotImplementedError(f"{asset}: socket {bone!r} has a bone-relative offset")
            bone = sk["bone"]
        krig.attach_to_bone(it, rig, bone)
        items[att["object"]] = it
        meshes.append(it)
    for ia in e.get("itemAttachments", []) or []:
        if ia.get("kind") == "swap":
            continue                                  # clip-only (the rogue's flash-bow)
        it = import_static(ia["object"])
        if ia.get("kind") == "body":
            s = next(x for x in e["sockets"] if x["name"] == ia["socket"])
            krig.attach_to_bone(it, rig, s["bone"])
            q = Vector((s["restLocCm"][0] / 100.0, -s["restLocCm"][1] / 100.0, s["restLocCm"][2] / 100.0))
            it.matrix_basis = Matrix.Translation(arm.data.bones[s["bone"]].matrix_local.inverted() @ q)
        else:                                          # child item on a parent item's socket (the mage crystal)
            parent = items.get(ia.get("parentItem"))
            pe = entry(ia["parentItem"])
            sk = next(x for x in pe.get("sockets", []) if x["name"] == ia["socket"])
            it.parent = parent
            it.matrix_parent_inverse = Matrix.Identity(4)
            it.matrix_basis = Matrix.Translation(Vector((sk["locCm"][0] / 100.0, -sk["locCm"][1] / 100.0,
                                                         sk["locCm"][2] / 100.0)))
        meshes.append(it)
    root = bpy.data.objects.new(f"COH_Root_{asset}_{len(bpy.data.objects)}", None)
    scene.link(root)
    arm.parent = root
    if arm2 is not None:
        arm2.parent = root
    extra = []
    if scenery:
        for sc in e.get("scenery", []) or []:
            st = import_static(sc["asset"])
            st.parent = root
            st.matrix_parent_inverse = Matrix.Identity(4)
            o = sc["offsetCm"]
            st.matrix_basis = Matrix.Translation(Vector((o[0] / 100.0, -o[1] / 100.0, o[2] / 100.0))) @ \
                Matrix.Rotation(math.radians(sc.get("yawDeg", 0.0)), 4, "Z")
            extra.append(st)
    scale = var.get("scale", 1.0) if var else 1.0
    root.location = Vector(at)
    root.rotation_euler = (0.0, 0.0, math.radians(yaw_deg))
    root.scale = (scale, scale, scale)
    bpy.context.view_layer.update()
    if var and var.get("elite"):
        cname = crown_asset()
        ce = entry(cname)
        crown = import_static(cname)
        wp = _socket_world(e, arm, "fx_overhead")
        yaw = 0.0
        if cam is not None:
            dv = cam.matrix_world.translation - wp
            yaw = math.atan2(dv.x, -dv.y)
        crown.matrix_world = Matrix.Translation(wp) @ Matrix.Rotation(yaw, 4, "Z")
        crown["af_px1080"] = slot_info(ce)[2]
        extra.append(crown)
    h = e.get("heightCm", 0.0) * scale / 100.0
    return Figure(asset, label or asset, root, arm, meshes, cls, px, h, extra, fam, n_body)


# ── ground ──────────────────────────────────────────────────────────────────────────────────────────────
def ground(tiles: np.ndarray, origin: tuple[int, int], ppt: int = 64, shadows=(), name: str = "coh_ground"):
    """Composited ground of the current theme (plains, or the shipped march set: ``set_theme``) over
    ``tiles[row, col]`` (origin = (col0, row0)) + web contact shadows ``[(col, row, radius_m, alpha)]`` (true circles,
    the web radial falloff); a textured emission plane."""
    if THEME == "march":
        import terrain_march as TM                     # the march textures + ranks / lips / liquid colours
        tex = TM.load_textures()
        layers = T.Layers(tex["tex"], tex["details"])
        img = TM.composite(tiles, layers, ppt=ppt, origin=origin).astype(np.float32)
    else:
        tex = T.load_textures()
        layers = T.Layers(tex["tex"], tex["details"])
        img = T.composite(tiles, layers, ppt=ppt, origin=origin).astype(np.float32)
    H, W = img.shape[:2]
    c0, r0 = origin
    for (c, r, rad, alpha) in shadows:
        rp = rad * ppt
        cx, cy = (c - c0 + 0.5) * ppt, (r - r0 + 0.5) * ppt
        x0, x1 = int(max(cx - rp, 0)), int(min(cx + rp + 1, W))
        y0, y1 = int(max(cy - rp, 0)), int(min(cy + rp + 1, H))
        if x0 >= x1 or y0 >= y1:
            continue
        ys, xs = np.mgrid[y0:y1, x0:x1]
        d = np.hypot(xs - cx, ys - cy) / rp
        a = np.where(d < 0.65, alpha - (alpha - 0.26 * alpha / 0.45) * d / 0.65,
                     (0.26 * alpha / 0.45) * (1 - (d - 0.65) / 0.35))
        a = np.clip(a, 0, 1) * (d < 1)
        img[y0:y1, x0:x1] = img[y0:y1, x0:x1] * (1 - a[..., None]) + np.array([8, 6, 16]) * a[..., None]
    SCRATCH.mkdir(parents=True, exist_ok=True)
    p = SCRATCH / f"{name}.png"
    pngio.write_png(p, img.clip(0, 255).astype(np.uint8))
    rows, cols = tiles.shape
    me = bpy.data.meshes.new(name)
    x0, x1 = c0 - 0.5, c0 + cols - 0.5
    y0, y1 = -(r0 - 0.5), -(r0 + rows - 0.5)
    me.from_pydata([(x0, y0, 0), (x1, y0, 0), (x1, y1, 0), (x0, y1, 0)], [], [(0, 1, 2, 3)])
    uv = me.uv_layers.new(name="UVMap")
    for li, (u, v) in enumerate(((0, 1), (1, 1), (1, 0), (0, 0))):
        uv.data[li].uv = (u, v)
    old = bpy.data.objects.get("AF_Stage")
    if old is not None:
        bpy.data.objects.remove(old)
    # named like the kit stage: review.coverage_pass keeps it as a black occluder (ground=True) or hides it,
    # review.emissive_pass hides it
    ob = bpy.data.objects.new("AF_Stage", me)
    scene.link(ob)
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    tx = nt.nodes.new("ShaderNodeTexImage")
    tx.image = bpy.data.images.load(str(p))
    tx.interpolation = "Linear"
    em = nt.nodes.new("ShaderNodeEmission")
    o = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(tx.outputs["Color"], em.inputs["Color"])
    nt.links.new(em.outputs[0], o.inputs["Surface"])
    me.materials.append(mat)
    return ob


# ── rendering ───────────────────────────────────────────────────────────────────────────────────────────
def apply_ink(meshes, k: float = 1.0) -> None:
    """Each mesh at its shipped screen ink (``af_px1080``) scaled to the render height (k = res_y / 1080)."""
    for m in meshes:
        if "af_outline_width" in m and m.type == "MESH":
            review.set_outline_px([m], float(m.get("af_px1080", 1.2)) * k)


LAST_EMISSIVE: dict = {}


def render(meshes, box=None, res=(1920, 1080), samples: int = 8):
    """(final with bloom, raw before bloom) uint8 at ``res`` (cropped to ``box``) for the current camera. The
    emissive pass of the same frame stays in ``LAST_EMISSIVE["em"]`` (for ``ink_gate_group``)."""
    k = res[1] / 1080.0
    apply_ink(meshes, k)
    raw = review.render_array(box)
    saved = {}
    for o in meshes:
        if o.type == "MESH":
            saved.setdefault(o.data.name, (o.data, list(o.data.materials)))
    em = review.emissive_pass(meshes, box)
    LAST_EMISSIVE["em"] = em
    for data, ms in saved.values():
        for i, mt in enumerate(ms):
            data.materials[i] = mt
    return review.bloom(raw, em, res[1]), raw


def projected_box(cam, objs, res=(1920, 1080), pad_px=(30, 30), max_pts: int = 5000):
    pts = review.mesh_points(objs)
    if len(pts) > max_pts:
        pts = pts[:: int(math.ceil(len(pts) / max_pts))]
    xy = np.array([review.project(cam, Vector(p)) for p in pts])
    px, py = pad_px[0] / res[0], pad_px[1] / res[1]
    return (max(0.0, xy[:, 0].min() - px), max(0.0, xy[:, 1].min() - py),
            min(1.0, xy[:, 0].max() + px), min(1.0, xy[:, 1].max() + py))


def ink_gate_group(meshes, every, frame_box, img, raw, cls: str, cam=None, res=(1920, 1080),
                   min_px: float | None = None, emissive: np.ndarray | None = None) -> dict:
    """Ink gate of one figure / prop on its own coverage (every other mesh hidden). The coverage passes render only
    the group's own projected box (+ 14 px) inside the ``frame_box`` render; ``img`` / ``raw`` (the frame render
    after / before bloom) are cropped to it. ``min_px`` overrides the class gate's rim threshold (characters: the
    Chapter-1 character gate ``CHAR_GATE_PX``). ``emissive`` (the frame's emissive pass, ``LAST_EMISSIVE``): edge
    pixels within 2 px of a bright emissive region (``EMISSIVE_EDGE_MIN``) are left out, as every family ship
    measures its ink — an unshaded,
    blooming FX accent at the silhouette (the shaman's spirit-fire, a glowing crystal) carries no hull by design
    (README §2 Outline: ``outline=False`` on small emissive bits); counted in ``ignoredEmissive``. Returns the
    ``review.ink_stats`` record + ``ok``, ``body``, ``hull``, ``rgb`` (the group crop) and ``box``."""
    gate = dict(review.ink_gate(cls))
    if min_px is not None:
        gate["minPx"] = float(min_px)
    cam = cam or bpy.context.scene.camera
    gb = projected_box(cam, meshes, res, pad_px=(14, 14))
    gb = (max(gb[0], frame_box[0]), max(gb[1], frame_box[1]), min(gb[2], frame_box[2]), min(gb[3], frame_box[3]))
    others = [o for o in every if o not in meshes and not o.hide_render]
    for o in others:
        o.hide_render = True
    try:
        body, hull = review.coverage_pass(meshes, gb, ground=True, extra_body=True)
        b0, h0 = review.coverage_pass(meshes, gb, ground=False, extra_body=True)
    finally:
        for o in others:
            o.hide_render = False
    fx0, fy0 = review.border_origin(frame_box, res)
    gx0, gy0 = review.border_origin(gb, res)
    h, w = body.shape
    oy, ox = gy0 - fy0, gx0 - fx0
    sub = img[oy:oy + h, ox:ox + w]
    subr = raw[oy:oy + h, ox:ox + w]
    if sub.shape[:2] != body.shape:                       # rounding at the frame edge: pad / trim to the coverage
        H, W = body.shape
        pad = np.zeros((H, W, 3), np.uint8)
        pad[:sub.shape[0], :sub.shape[1]] = sub[:H, :W]
        sub = pad
        padr = np.zeros((H, W, 3), np.uint8)
        padr[:subr.shape[0], :subr.shape[1]] = subr[:H, :W]
        subr = padr
    buried = (b0 + h0) - (body + hull) > 0.25
    for _ in range(2):
        pb = np.pad(buried, 1, constant_values=False)
        buried = buried | pb[:-2, 1:-1] | pb[2:, 1:-1] | pb[1:-1, :-2] | pb[1:-1, 2:]
    glow = np.zeros_like(buried)
    if emissive is not None:
        H, W = body.shape
        es = emissive[oy:oy + H, ox:ox + W, :3]
        g = np.zeros((H, W), bool)
        g[:es.shape[0], :es.shape[1]] = es.max(-1) > EMISSIVE_EDGE_MIN
        for _ in range(2):
            pg = np.pad(g, 1, constant_values=False)
            g = g | pg[:-2, 1:-1] | pg[2:, 1:-1] | pg[1:-1, :-2] | pg[1:-1, 2:]
        glow = g
    st = review.ink_stats(body, hull, sub, min_px=gate["minPx"], dark_lum=gate["darkLum"],
                          within=gate["darkWithinPx"], hull_rgb=subr, ignore=buried | glow)
    if emissive is not None:
        A = np.clip(body + hull, 0, 1) >= 0.5
        pa = np.pad(A, 1, constant_values=False)
        edge = A & (~pa[:-2, 1:-1] | ~pa[2:, 1:-1] | ~pa[1:-1, :-2] | ~pa[1:-1, 2:])
        st["ignoredEmissive"] = int((edge & glow & ~buried).sum())
    st["gate"] = gate
    st["ok"] = review.ink_gate_ok(st, gate)
    st.update(body=body, hull=hull, rgb=sub, box=gb, offset=(ox, oy))
    return st


def crop(full: np.ndarray, box, res=(1920, 1080)) -> np.ndarray:
    x0, y0 = review.border_origin(box, res)
    w = int(round((box[2] - box[0]) * res[0]))
    h = int(round((box[3] - box[1]) * res[1]))
    return full[y0:y0 + h, x0:x0 + w]


def hsv_sat(rgb: np.ndarray) -> np.ndarray:
    a = rgb.astype(np.float32) / 255.0
    mx, mn = a.max(-1), a.min(-1)
    return np.where(mx > 1e-4, (mx - mn) / np.maximum(mx, 1e-4), 0.0)


def lab_chroma(rgb: np.ndarray) -> np.ndarray:
    """CIELAB chroma C*ab (D65) of sRGB uint8 pixels — perceived colourfulness. HSV saturation (``hsv_sat``) rates a
    dark brown (wood, leather: (mx − mn) / mx) as saturated as a hero's crimson; C* does not."""
    a = rgb[..., :3].astype(np.float32) / 255.0
    lin = np.where(a <= 0.04045, a / 12.92, ((a + 0.055) / 1.055) ** 2.4)
    m = np.array([[0.4124, 0.3576, 0.1805], [0.2126, 0.7152, 0.0722], [0.0193, 0.1192, 0.9505]], np.float32)
    xyz = lin @ m.T / np.array([0.95047, 1.0, 1.08883], np.float32)
    f = np.where(xyz > 216 / 24389, np.cbrt(xyz), (24389 / 27 * xyz + 16) / 116)
    a_ = 500.0 * (f[..., 0] - f[..., 1])
    b_ = 200.0 * (f[..., 1] - f[..., 2])
    return np.hypot(a_, b_)


def lum(rgb: np.ndarray) -> np.ndarray:
    return rgb[..., :3].astype(np.float32) @ np.array([0.299, 0.587, 0.114], np.float32)

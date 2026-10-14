"""Shared helpers of the VFX family (``Art/blender/vfx/``): FX meshes for the runtime's pooled ISM particles
(DECISIONS P6, WorldContract 5) and their preview look.

How UE draws an FX mesh (``UAbyssVfxSystem``: every slot of a mesh layer gets ``M_AF_FX_Mesh``,
``Scripts/abyss_content/hlsl.py`` AF_FxMesh / AF_FxMeshMask):

    lc   = saturate(instance colour)  (linear: palette / hex colour × layer intensity)
    c    = sRGB-encode(lc)
    hl   = dot(N, KeyLightDir) * 0.5 + 0.5                         N = imported vertex normal
    col  = mix(shade(c), c, smoothstep(.41,.43,hl)) → mix(…, light(c), smoothstep(.85,.87,hl))   (tone(): s .42, l .32)
    col  = mix(INK #120C18, col, AF_Data.a)                         hull (A = 0) → ink; body A < 1 = darker value
    out  = sRGB-decode(col) × max(1, max(instance colour))          (HDR → bloom)
    mask = instance alpha > 4×4 Bayer threshold (screen space)       masked, single-sided (back faces culled)

So an FX mesh is **one tint per particle**; its internal structure comes from geometry, toon bands, the baked ink
hull and the **value channel AF_Data.a** (1 = full tint, lower = toward ink): an arrow's steel head 1.0, shaft 0.6.
``fx_material`` reproduces that maths in Cycles (exact bands, Bayer dither on the full-frame pixel grid), so the
previews are what UE renders. Meshes face **Blender −Y** (= UE +Y): velocity-oriented layers align UE +Y with the
velocity, ground / upright layers yaw about Z (``alignToBlow`` puts +Y along the blow).
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path
from types import SimpleNamespace
from typing import Sequence

import bpy
import numpy as np
from mathutils import Vector

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

from kit import color, export, outline, paths, review, scene, shading  # noqa: E402
from kit.paths import scratch_root  # noqa: E402
from kit import mesh as M  # noqa: E402
from kit.palette import Palette  # noqa: E402

CATEGORY = "VFX"
FAMILY = "FX"
INK_HEX = shading.INK_HEX
PX_CM = 2.0 * shading.CAM_DIST_DEFAULT * math.tan(math.radians(shading.CAM_FOV_H / 2.0)) * (9.0 / 16.0) / 1080.0 * 100.0
"""cm per 1080p pixel at the W1 default distance (≈ 0.83)."""
SCRATCH = scratch_root("vfx3d")
FX_MATERIAL = "M_AF_FX_Mesh"


# ── the M_AF_FX_Mesh look ──────────────────────────────────────────────────────────────────────────────
def _enc(v: float) -> float:
    return color.linear_to_srgb1(max(0.0, min(1.0, v)))


def bands(tint_lin: Sequence[float]) -> dict:
    """AF_FxMesh colours for an instance colour (linear, may exceed 1): sRGB-encoded shade / base / light, the
    ink, and the HDR factor."""
    lc = [max(0.0, min(1.0, float(c))) for c in tint_lin[:3]]
    c = [_enc(v) * 255.0 for v in lc]
    t = color.tone(c, 0.42, 0.32)
    return {"shade": [v / 255 for v in t["shade"]], "base": [v / 255 for v in t["base"]],
            "light": [v / 255 for v in t["light"]], "ink": [v / 255 for v in color.hex_to_rgb(INK_HEX)],
            "hdr": max(1.0, max(float(x) for x in tint_lin[:3]))}


def tint_linear(hex_color: str, intensity: float = 1.0) -> tuple[float, float, float]:
    r, g, b = color.hex_linear(hex_color)
    return (r * intensity, g * intensity, b * intensity)


_BAYER_IMG = None


def _bayer_image():
    """4×4 ordered-dither thresholds exactly as AF_FxMeshMask ((b + .5) / 16, b = bit-interleave of x^y and y)."""
    global _BAYER_IMG
    if _BAYER_IMG is not None and _BAYER_IMG.name in bpy.data.images:
        return _BAYER_IMG
    img = bpy.data.images.new("AF_Bayer4", 4, 4, float_buffer=True)
    img.colorspace_settings.name = "Non-Color"
    px = np.zeros((4, 4, 4), np.float32)
    for qy in range(4):
        for qx in range(4):
            m = qx ^ qy
            b = ((m & 1) << 3) | ((qy & 1) << 2) | (m & 2) | ((qy & 2) >> 1)
            # Blender image rows go bottom → top; SvPosition y goes top → bottom (row qy from the top)
            px[3 - qy, qx] = ((b + 0.5) / 16.0,) * 3 + (1.0,)
    img.pixels.foreach_set(px.ravel())
    img.pack()
    _BAYER_IMG = img
    return img


def fx_material(tint_lin: Sequence[float], alpha: float = 1.0, name: str | None = None,
                res: tuple[int, int] = (1920, 1080)) -> bpy.types.Material:
    """Cycles material = UE M_AF_FX_Mesh for one instance colour / alpha (cached by value). Single-sided like UE:
    back faces are transparent (the inverted hull's near side), the dither threshold uses full-frame pixels."""
    key = name or ("AF_FxMesh_%s_%.3f_%d" % ("_".join(f"{v:.4f}" for v in tint_lin[:3]), alpha, res[1]))
    m = bpy.data.materials.get(key)
    if m is not None:
        return m
    bd = bands(tint_lin)
    shading.ensure_groups()
    m = bpy.data.materials.new(key)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    b = shading.NB(nt)
    out = b.node("ShaderNodeOutputMaterial")
    geo = b.node("ShaderNodeNewGeometry")
    attr = b.node("ShaderNodeAttribute", attribute_type="GEOMETRY", attribute_name="AF_Data")
    lk = shading.L_KEY
    dot = b.node("ShaderNodeVectorMath", operation="DOT_PRODUCT")
    b.l.new(geo.outputs["Normal"], dot.inputs[0])
    dot.inputs[1].default_value = (lk[0], lk[1], lk[2])
    hl = b.node("ShaderNodeMath", operation="MULTIPLY_ADD")
    b.l.new(dot.outputs["Value"], hl.inputs[0])
    hl.inputs[1].default_value = 0.5
    hl.inputs[2].default_value = 0.5
    half = shading.BAND_WIDTH / 2.0

    def smooth(e0, e1):
        mr = b.node("ShaderNodeMapRange", interpolation_type="SMOOTHSTEP")
        b.l.new(hl.outputs[0], mr.inputs["Value"])
        mr.inputs["From Min"].default_value = e0
        mr.inputs["From Max"].default_value = e1
        return mr.outputs["Result"]
    s1, s2 = smooth(shading.T_SHADE - half, shading.T_SHADE + half), smooth(shading.T_LIGHT - half,
                                                                             shading.T_LIGHT + half)

    def rgb(c):
        n = b.node("ShaderNodeRGB")
        n.outputs[0].default_value = (c[0], c[1], c[2], 1.0)
        return n.outputs[0]

    def mix(fac, a, c):
        n = b.node("ShaderNodeMix", data_type="RGBA", blend_type="MIX", clamp_factor=True)
        b.l.new(fac, n.inputs[0])
        b.l.new(a, n.inputs[6])
        b.l.new(c, n.inputs[7])
        return n.outputs[2]
    col = mix(s2, mix(s1, rgb(bd["shade"]), rgb(bd["base"])), rgb(bd["light"]))
    col = mix(attr.outputs["Alpha"], rgb(bd["ink"]), col)          # sRGB-encoded maths, like the HLSL
    # decode: ((s + .055) / 1.055) ^ 2.4 (the linear toe below 0.04 is < 0.3 / 255 off)
    mad = b.node("ShaderNodeVectorMath", operation="MULTIPLY_ADD")
    b.l.new(col, mad.inputs[0])
    mad.inputs[1].default_value = (1 / 1.055,) * 3
    mad.inputs[2].default_value = (0.055 / 1.055,) * 3
    gam = b.node("ShaderNodeGamma")
    b.l.new(mad.outputs[0], gam.inputs["Color"])
    gam.inputs["Gamma"].default_value = 2.4
    em = b.node("ShaderNodeEmission")
    b.l.new(gam.outputs[0], em.inputs["Color"])
    em.inputs["Strength"].default_value = bd["hdr"]
    tr = b.node("ShaderNodeBsdfTransparent")
    # visibility: front face AND alpha > Bayer threshold
    vis = b.node("ShaderNodeMath", operation="SUBTRACT")
    vis.inputs[0].default_value = 1.0
    b.l.new(geo.outputs["Backfacing"], vis.inputs[1])
    if alpha < 0.999:
        tc = b.node("ShaderNodeTexCoord")
        sc = b.node("ShaderNodeVectorMath", operation="MULTIPLY")
        b.l.new(tc.outputs["Window"], sc.inputs[0])
        sc.inputs[1].default_value = (res[0] / 4.0, res[1] / 4.0, 1.0)
        tx = b.node("ShaderNodeTexImage", interpolation="Closest", extension="REPEAT")
        tx.image = _bayer_image()
        b.l.new(sc.outputs[0], tx.inputs["Vector"])
        gt = b.node("ShaderNodeMath", operation="LESS_THAN")       # threshold < alpha
        b.l.new(tx.outputs["Color"], gt.inputs[0])
        gt.inputs[1].default_value = alpha
        both = b.node("ShaderNodeMath", operation="MULTIPLY")
        b.l.new(vis.outputs[0], both.inputs[0])
        b.l.new(gt.outputs[0], both.inputs[1])
        vis = both
    mx = b.node("ShaderNodeMixShader")
    b.l.new(vis.outputs[0], mx.inputs[0])
    b.l.new(tr.outputs[0], mx.inputs[1])
    b.l.new(em.outputs[0], mx.inputs[2])
    b.l.new(mx.outputs[0], out.inputs["Surface"])
    m["af_fx_tint"] = list(tint_lin[:3])
    return m


def set_fx_look(obj: bpy.types.Object, tint_lin: Sequence[float], alpha: float = 1.0,
                res: tuple[int, int] = (1920, 1080)) -> None:
    """Both slots (body + hull) → the FX mesh look for one instance colour (UE assigns M_AF_FX_Mesh to every slot)."""
    mat = fx_material(tint_lin, alpha, res=res)
    for i in range(len(obj.data.materials)):
        obj.data.materials[i] = mat


# ── building ───────────────────────────────────────────────────────────────────────────────────────────
class FxBuilder(M.Builder):
    """``kit.mesh.Builder`` with per-region **values** (AF_Data.a; the FX palette is only the region registry,
    never saved: the runtime material ignores palette UVs)."""

    def __init__(self, asset: str, values: dict[str, float], prefix: str | None = None):
        super().__init__(asset, Palette(FAMILY), prefix=prefix or asset.lower())
        from kit.palette import Region
        self.values = dict(values)
        for k in values:
            self.region(k, Region("#FFFFFF", .42, .32))


def mitre_hull(obj: bpy.types.Object, width: float, limit: float = 2.6) -> dict:
    """Re-seat the baked hull of a faceted mesh as a **mitred offset**: every hull face sits exactly ``width`` off
    its body face (the kit pushes vertices along the angle-weighted smoothed normal, which leaves a pyramid's
    flanks only ``width × cos φ`` off — a sharp crystal tip kept ~1/3 of its ink). Per body position the hull
    displacement δ solves ``n_i · δ = width`` for the distinct face normals n_i around it (least squares,
    minimum norm), capped at ``limit × width`` (pointed ink at sharp tips, like a pen line). Hull vertices are
    found through the stored hull normal (AF_Data.rgb = n·0.5 + 0.5, hull = body + n·width)."""
    me = obj.data
    nv = len(me.vertices)
    co = np.zeros(nv * 3, np.float64)
    me.vertices.foreach_get("co", co)
    co = co.reshape(nv, 3)
    afd = me.color_attributes["AF_Data"]
    data = np.zeros(nv * 4, np.float32)
    afd.data.foreach_get("color", data)
    data = data.reshape(nv, 4)
    mat = np.zeros(len(me.polygons), np.int32)
    me.polygons.foreach_get("material_index", mat)
    hull_v = set()
    for p in me.polygons:
        if p.material_index == 1:
            hull_v.update(p.vertices)
    if not hull_v:
        return {"mitred": 0}
    hv = np.array(sorted(hull_v))
    n = data[hv, :3].astype(np.float64) * 2.0 - 1.0
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-9)
    body = co[hv] - n * width
    keys = [tuple(np.round(b, 5)) for b in body]
    # face normals of the hull faces (reversed winding → negate) gathered per body position
    fn: dict[tuple, list] = {}
    idx_of = {int(v): i for i, v in enumerate(hv)}
    for p in me.polygons:
        if p.material_index != 1:
            continue
        nrm = -np.array(p.normal[:], np.float64)
        for v in p.vertices:
            fn.setdefault(keys[idx_of[v]], []).append(nrm)
    moved, cap = 0, 0
    newco = co.copy()
    cache: dict[tuple, np.ndarray] = {}
    for i, v in enumerate(hv):
        k = keys[i]
        if k not in cache:
            ns = []
            for a in fn.get(k, []):
                if not any(float(a @ b) > 0.996 for b in ns):
                    ns.append(a)
            if len(ns) < 2:
                d = (ns[0] if ns else n[i]) * width
            else:
                A = np.array(ns)
                d = np.linalg.lstsq(A, np.full(len(ns), width), rcond=None)[0]
                L = float(np.linalg.norm(d))
                if L > limit * width:
                    d *= limit * width / L
                    cap += 1
                if float(d @ n[i]) <= 0.2 * width:      # concave / degenerate: keep the smoothed push
                    d = n[i] * width
            cache[k] = d
        newco[v] = body[i] + cache[k]
        moved += 1
    me.vertices.foreach_set("co", newco.ravel())
    me.update()
    return {"mitred": moved, "capped": cap}


def finish(b: FxBuilder, width_cm: float, flat_lit: bool = False, default_tint: str = "#FFFFFF",
           intensity: float = 1.0, lit_dir: Sequence[float] = (0.0, 0.0, 1.0),
           mitre: float = 0.0) -> bpy.types.Object:
    """Build → AF_Data.a values per region → (flat-lit normals) → baked ink hull (slot 1) → preview look.

    ``flat_lit``: every body normal = ``lit_dir`` (mesh space) so the toon maths lands the whole surface in one
    band — an unshaded, emissive-looking cel shape (fire, spirit flame) whose structure is the value channel and
    the ink. ``mitre`` > 0: re-seat the hull as a mitred offset (``mitre_hull``, cap = ``mitre`` × width) — for
    faceted crystals and chips whose sharp tips and flanks lose ink to the smoothed push."""
    # values by PART (af_part → the region it was added with): every FX region is the same white Region, so the
    # palette merges them into one swatch and af_swatch cannot tell them apart
    part_region = [p["region"] for p in b.parts]
    values = b.values
    obj = b.build(b.asset)
    me = obj.data
    nf = len(me.polygons)
    pa = np.zeros(nf, np.int32)
    me.attributes["af_part"].data.foreach_get("value", pa)
    nv = len(me.vertices)
    vals = np.ones(nv, np.float32)
    for p, s in zip(me.polygons, pa.tolist()):
        v = values.get(part_region[s] if 0 <= s < len(part_region) else "", 1.0)
        for vi in p.vertices:
            vals[vi] = v
    afd = me.color_attributes["AF_Data"]
    data = np.zeros(nv * 4, np.float32)
    afd.data.foreach_get("color", data)
    data = data.reshape(nv, 4)
    data[:, 3] = vals
    afd.data.foreach_set("color", data.ravel())
    if flat_lit:
        n = Vector(lit_dir).normalized()
        me.normals_split_custom_set([tuple(n)] * len(me.loops))
    mt = bpy.data.materials.new(FX_MATERIAL)
    mo = bpy.data.materials.new(FX_MATERIAL + "_Ink")
    outline.bake_hull(obj, width_cm / 100.0, mt, mo)
    if mitre > 0:
        st = mitre_hull(obj, width_cm / 100.0, mitre)
        obj["af_mitre"] = json.dumps(st)
    obj["af_outline_width"] = width_cm / 100.0
    obj["af_outline_class"] = "fx"
    obj["af_default_tint"] = default_tint
    obj["af_intensity"] = intensity
    obj["af_values"] = json.dumps(values)
    # export material names (two distinct materials keep the two FBX slots) – the preview look replaces them
    obj["af_export_mats"] = json.dumps([mt.name, mo.name])
    return obj


def export_mats(obj: bpy.types.Object) -> None:
    names = json.loads(obj["af_export_mats"])
    for i, n in enumerate(names):
        obj.data.materials[i] = bpy.data.materials[n]


def preview_mats(obj: bpy.types.Object, tint_hex: str | None = None, intensity: float | None = None,
                 alpha: float = 1.0) -> None:
    t = tint_linear(tint_hex or obj["af_default_tint"], obj["af_intensity"] if intensity is None else intensity)
    set_fx_look(obj, t, alpha)


# ── shipping ───────────────────────────────────────────────────────────────────────────────────────────
def tri(obj, slot: int) -> int:
    return M.tri_count(obj, slot)


def manifest_entry(obj, fbx: Path, fx: dict, previews: Sequence[str], ink: dict | None) -> dict:
    width = float(obj["af_outline_width"])
    lo, hi = scene.world_bounds([obj])
    diam = (hi - lo).length * 100.0
    entry = {
        "kind": "StaticMesh",
        "category": CATEGORY,
        "family": "vfx",
        "fbx": paths.rel_to_export(fbx),
        "gameIds": [],
        "scale": 1.0,
        "boundsCm": export.bounds_ue([obj]),
        "boundsDiameterCm": round(diam, 1),
        "triangles": {"toon": tri(obj, 0), "outline": tri(obj, 1)},
        "materialSlots": [
            {"index": 0, "name": FX_MATERIAL, "parent": FX_MATERIAL},
            {"index": 1, "name": FX_MATERIAL, "parent": FX_MATERIAL, "note": "baked ink hull (AF_Data.a = 0 → ink); "
             "no outline WPO in M_AF_FX_Mesh", "widthCm": round(width * 100.0, 2),
             "px1080AtDefaultDistance": round(width * 100.0 / PX_CM, 2)},
        ],
        "sockets": [],
        "blocking": False,
        "fx": fx,
        "previews": list(previews),
    }
    if ink is not None:
        entry["inkGate"] = ink
    return entry


def ship(obj, fx: dict, previews: Sequence[str], ink: dict | None) -> dict:
    """FBX ``Export/VFX/<asset>.fbx`` + merged manifest entry (``kit.export.Manifest``)."""
    export_mats(obj)
    fbx = export.export_static_mesh(obj, obj.name, CATEGORY)
    preview_mats(obj)
    man = export.Manifest()
    entry = manifest_entry(obj, fbx, fx, previews, ink)
    man.set_asset(obj.name, entry)
    man.save()
    return entry


# ── characters for scale (shipped FBX + palettes on disk) ─────────────────────────────────────────────
def import_character(sk: str, category: str, family: str, clip: str | None = None, frame: int = 0,
                     at=(0.0, 0.0, 0.0), yaw_deg: float = 0.0, items: Sequence[tuple[str, str, str]] = (),
                     outline_class: str = "monster"):
    """A shipped skeletal mesh (``Export/<category>/<sk>.fbx``) posed by ``A_<token>_<clip>.fbx`` at ``frame``,
    drawn with its family palette (toon + outline from the PNGs). ``items`` = [(SM name, category, bone)].
    Returns (root empty, meshes, armature)."""
    sys.path.insert(0, str(HERE.parent / "environment"))
    import envkit as E
    root_dir = paths.export_root()
    skp = root_dir / category / f"{sk}.fbx"
    if not skp.exists():
        return None
    mt, mo = E.toon_material_files(family)
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(skp))
    new = [o for o in bpy.data.objects if o not in before]
    arm = next(o for o in new if o.type == "ARMATURE")
    meshes = [o for o in new if o.type == "MESH"]
    if clip:
        token = sk[3:]
        an = root_dir / category / f"A_{token}_{clip}.fbx"
        if an.exists():
            b2 = set(bpy.data.objects)
            bpy.ops.import_scene.fbx(filepath=str(an))
            new2 = [o for o in bpy.data.objects if o not in b2]
            arm2 = next((o for o in new2 if o.type == "ARMATURE"), None)
            if arm2 is not None:
                for pb in arm.pose.bones:
                    c = pb.constraints.new("COPY_TRANSFORMS")
                    c.target, c.subtarget = arm2, pb.name
                    c.target_space = c.owner_space = "POSE"
                arm2.hide_render = True
                act = arm2.animation_data.action if arm2.animation_data else None
                if act is not None:
                    bpy.context.scene.frame_set(int(act.frame_range[0]) + frame)
                for o in new2:
                    if o is not arm2:
                        o.hide_render = True
    for m in meshes:
        E._set_slots(m, mt, mo)
        m["af_outline_width"] = outline.WIDTH_CM[outline_class] / 100.0
        m["af_outline_class"] = outline_class
    from kit import rig as krig
    rig = SimpleNamespace(obj=arm)
    for name, cat, bone in items:
        f = root_dir / cat / f"{name}.fbx"
        if not f.exists():
            continue
        b3 = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(f))
        new3 = [o for o in bpy.data.objects if o not in b3]
        wm = next(o for o in new3 if o.type == "MESH")
        for o in new3:
            if o is not wm:
                bpy.data.objects.remove(o)
        fam = _manifest_palette(name) or family
        wt, wo = E.toon_material_files(fam)
        E._set_slots(wm, wt, wo)
        wm["af_outline_width"] = outline.WIDTH_CM["weapon"] / 100.0
        wm["af_outline_class"] = "weapon"
        krig.attach_to_bone(wm, rig, bone)
        meshes.append(wm)
    root = bpy.data.objects.new(f"{sk}_Root", None)
    scene.link(root)
    arm.parent = root
    root.location = Vector(at)
    root.rotation_euler = (0, 0, math.radians(yaw_deg))
    bpy.context.view_layer.update()
    return root, meshes, arm


def _manifest_palette(asset: str) -> str | None:
    try:
        e = json.loads(paths.manifest_path().read_text())["assets"].get(asset) or {}
        return (e.get("materialSlots") or [{}])[0].get("palette")
    except (OSError, ValueError):
        return None


def import_warrior(clip: str = "Idle", frame: int = 0, at=(0.0, 0.0, 0.0), yaw_deg: float = 0.0):
    return import_character("SK_Hero_Warrior", "Characters", "Heroes", clip, frame, at, yaw_deg,
                            [("SM_Hero_Warrior_Sword", "Weapons", "weapon_r"),
                             ("SM_Hero_Warrior_Shield", "Weapons", "weapon_l")], outline_class="hero")


def import_goblin(clip: str = "Idle", frame: int = 0, at=(0.0, 0.0, 0.0), yaw_deg: float = 0.0):
    return import_character("SK_Mon_Goblin", "Monsters", "Monsters_Plains", clip, frame, at, yaw_deg,
                            [("SM_Mon_Goblin_Spear", "Weapons", "weapon_r")], outline_class="monster")


def static_import(name: str, category: str):
    """A shipped SM (e.g. ``SM_Pickup_GoldCoin``) as a Blender mesh object (materials stripped)."""
    f = paths.export_root() / category / f"{name}.fbx"
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(f))
    new = [o for o in bpy.data.objects if o not in before]
    m = next(o for o in new if o.type == "MESH")
    for o in new:
        if o is not m:
            bpy.data.objects.remove(o)
    m.parent = None
    m.matrix_world.identity()
    return m

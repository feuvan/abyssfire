"""Inverted-hull outline baking (DECISIONS P7, R3; spec art-inventory-ch1.md §1.3, §2.3).

``bake_hull(obj, width)`` appends to the **same mesh** a copy of every outlined face (``af_nohull == 0``):

* vertices pushed out along an **angle-weighted smoothed normal** (shared by every corner at the same position,
  so hard edges and part seams never split the outline) by ``width`` metres — world width per R3;
* face winding **flipped** (UE's default back-face culling then draws only the far side = the ink rim;
  the Cycles preview uses ``Geometry.Backfacing`` to make the near side transparent);
* **material slot 1 = ``M_AF_Outline``** (slot 0 = the family toon material);
* same vertex groups / weights as the source vertices → the hull **skins with the body**;
* custom normals: hull corners = the smoothed **outward** normal (UE: import normals, then
  ``WPO = VertexNormalWS × (OutlinePx·PixelWorldSize − BakedWidth)`` keeps the width constant on screen when
  zooming); body corners keep their own shading normals;
* ``AF_Data`` colour on hull vertices = smoothed normal × 0.5 + 0.5 (RGB, bind pose), A = 0 (spec §2.3);
* face attribute ``af_hull`` = 1;
* hull-only proxies (face attribute ``af_hullonly``, ``Builder.add(hull=…)``): a simpler shape stands in for a
  detailed part — its hull copy is kept, its own faces are removed (low-poly hull for mail, grooved helms, folds).

Outline world widths by asset class (R3: 1.2–1.8 cm by asset size) live in ``WIDTH_CM``.
"""
from __future__ import annotations

from collections import defaultdict

import bmesh
import bpy
import numpy as np
from mathutils import Vector

from . import shading

WIDTH_CM = {
    "hero": 1.8, "boss": 1.8, "npc": 1.6, "monster": 1.5, "small_monster": 1.4,
    "interactive": 1.3, "weapon": 1.3, "decor": 1.2, "none": 0.0,
}
HULL_SLOT = 1
# Screen-constant ink width at 1080p per class (spec §1.3 table; ∝ viewport height) — the UE outline material
# grows the baked hull to it: WPO = VertexNormalWS × (OutlinePx × PixelWorldSize − BakedWidth). The baked width
# alone is ~2 px at the default W1 distance (R3), lighter than the web's heavy silhouette ink.
SCREEN_PX_1080 = {
    "hero": 3.5, "boss": 3.5, "npc": 3.0, "monster": 3.0, "small_monster": 3.0,
    "interactive": 2.0, "weapon": 3.5, "decor": 1.2, "none": 0.0,
}


def bake_hull(obj: bpy.types.Object, width: float, toon_material=None, outline_material=None) -> dict:
    """Bake the hull into ``obj`` (call after skinning and palette UVs). Returns stats."""
    me = obj.data
    nloops_all = len(me.loops)
    body_normals = np.zeros(nloops_all * 3, np.float32)
    me.corner_normals.foreach_get("vector", body_normals)
    body_normals = body_normals.reshape(-1, 3)
    # hull-only proxy faces (``Builder.add(hull=…)``) contribute their hull copy only: their toon faces (and
    # loops) are dropped below, so keep the body normals of the other loops, in order
    keep_loop = np.ones(nloops_all, bool)
    if "af_hullonly" in me.attributes:
        ho = np.zeros(len(me.polygons), np.int32)
        me.attributes["af_hullonly"].data.foreach_get("value", ho)
        ls0 = np.zeros(len(me.polygons), np.int32)
        lt0 = np.zeros(len(me.polygons), np.int32)
        me.polygons.foreach_get("loop_start", ls0)
        me.polygons.foreach_get("loop_total", lt0)
        for st, tt, h in zip(ls0.tolist(), lt0.tolist(), ho.tolist()):
            if h:
                keep_loop[st:st + tt] = False
    body_normals = body_normals[keep_loop]
    nloops0 = int(keep_loop.sum())
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.normal_update()
    nohull = bm.faces.layers.int.get("af_nohull")
    hullonly = bm.faces.layers.int.get("af_hullonly")
    lay = bm.faces.layers.int.get("af_hull") or bm.faces.layers.int.new("af_hull")   # before any element refs
    src = [f for f in bm.faces if (nohull is None or f[nohull] == 0)]
    # smoothed normal per position, **angle weighted** (art review 6): each face counts by its corner angle at the
    # vertex, not its area — at the edge of a thin sheet (cape, tabard, plume, shield rim) or a part junction the
    # big front / back faces cancelled and the small wall barely steered an area-weighted normal, so the hull grew
    # mostly toward / away from the camera there and the silhouette ink thinned to ~2 px at 1080p
    acc: dict[tuple, Vector] = defaultdict(lambda: Vector((0.0, 0.0, 0.0)))

    def key(co):
        return (round(co.x, 5), round(co.y, 5), round(co.z, 5))
    for f in src:
        for lp in f.loops:
            acc[key(lp.vert.co)] += f.normal * lp.calc_angle()
    originals = set(bm.verts)
    dup = bmesh.ops.duplicate(bm, geom=src)
    new_faces = [g for g in dup["geom"] if isinstance(g, bmesh.types.BMFace)]
    # vert_map holds both directions (old→new and new→old): keep old→new only
    vmap = {o: n for o, n in dup["vert_map"].items() if o in originals and n not in originals}
    col = bm.verts.layers.float_color.get("AF_Data")
    hull_n = {}
    for old, new in vmap.items():
        if not isinstance(old, bmesh.types.BMVert):
            continue
        n = acc.get(key(old.co))
        n = n.normalized() if n is not None and n.length > 1e-12 else Vector((0, 0, 1))
        new.co = old.co + n * width
        hull_n[new] = n
        if col is not None:
            new[col] = (n.x * 0.5 + 0.5, n.y * 0.5 + 0.5, n.z * 0.5 + 0.5, 0.0)
    for f in new_faces:
        f[lay] = 1
    n_hull = len(new_faces)
    bmesh.ops.reverse_faces(bm, faces=new_faces)        # (may re-create the faces: collect them again)
    new_faces = [f for f in bm.faces if f[lay] == 1]
    for f in new_faces:
        f.material_index = HULL_SLOT
        f.smooth = True
    assert len(new_faces) == n_hull
    if hullonly is not None:          # drop the proxies' own (toon) faces and their now-loose vertices
        proxy = [f for f in bm.faces if f[hullonly] == 1 and f[lay] == 0]
        if proxy:
            bmesh.ops.delete(bm, geom=proxy, context="FACES")
            loose = [v for v in bm.verts if not v.link_faces]
            if loose:
                bmesh.ops.delete(bm, geom=loose, context="VERTS")
    bm.verts.index_update()
    hull_n_by_index = {v.index: n for v, n in hull_n.items()}
    bm.to_mesh(me)
    bm.free()
    # custom normals: body loops keep their normals (same order, they come first); hull loops = outward smooth
    nl = np.zeros((len(me.loops), 3), np.float32)
    nl[:nloops0] = body_normals
    lv = np.zeros(len(me.loops), np.int32)
    me.loops.foreach_get("vertex_index", lv)
    hull_attr = np.zeros(len(me.polygons), np.int32)
    me.attributes["af_hull"].data.foreach_get("value", hull_attr)
    ls = np.zeros(len(me.polygons), np.int32)
    lt = np.zeros(len(me.polygons), np.int32)
    me.polygons.foreach_get("loop_start", ls)
    me.polygons.foreach_get("loop_total", lt)
    for s_, t_, h_ in zip(ls.tolist(), lt.tolist(), hull_attr.tolist()):
        if h_:
            for li in range(s_, s_ + t_):
                nl[li] = tuple(hull_n_by_index[int(lv[li])])
    me.normals_split_custom_set([tuple(v) for v in nl])
    # materials: slot 0 toon, slot 1 outline (bmesh.to_mesh clamps indices to existing slots → set after)
    if toon_material is not None or outline_material is not None:
        me.materials.clear()
        me.materials.append(toon_material)
        me.materials.append(outline_material)
    while len(me.materials) < 2:
        me.materials.append(None)
    me.polygons.foreach_set("material_index", (hull_attr > 0).astype(np.int32) * HULL_SLOT)
    me.update()
    obj["af_outline_width"] = width
    return {"hullFaces": len(new_faces), "widthCm": round(width * 100, 3)}


# ── preview: constant-ish screen width ──────────────────────────────────────────────────────────────────
# The preview reproduces the UE outline WPO exactly: hull vertices move along their **custom (imported) normal**
# = the outward smoothed normal ``bake_hull`` stores, skinned with the mesh. Blender ≥ 4.1 / 5.0's Normal node
# (``legacy_corner_normals`` off) returns those custom normals. (Until art review 6 the group negated the normal
# on the assumption that it followed the flipped hull winding, i.e. pointed inward: every preview grew the hull
# the wrong way — game views asked for 2.9 cm got 0.7 cm, close-ups asked for 0.9 cm got 2.7 cm.)
PREVIEW_GROUP = "AF_OutlinePreview_v2"
PREVIEW_MOD = "AF_OutlinePreview"


def _preview_group() -> bpy.types.NodeTree:
    ng = bpy.data.node_groups.get(PREVIEW_GROUP)
    if ng is not None:
        return ng
    ng = bpy.data.node_groups.new(PREVIEW_GROUP, "GeometryNodeTree")
    ng.interface.new_socket("Geometry", in_out="INPUT", socket_type="NodeSocketGeometry")
    ng.interface.new_socket("Extra", in_out="INPUT", socket_type="NodeSocketFloat")
    ng.interface.new_socket("Geometry", in_out="OUTPUT", socket_type="NodeSocketGeometry")
    n, l = ng.nodes, ng.links
    gi, go = n.new("NodeGroupInput"), n.new("NodeGroupOutput")
    sp = n.new("GeometryNodeSetPosition")
    nrm = n.new("GeometryNodeInputNormal")
    if hasattr(nrm, "legacy_corner_normals"):
        nrm.legacy_corner_normals = False             # custom normals = UE's VertexNormalWS
    unit = n.new("ShaderNodeVectorMath")
    unit.operation = "NORMALIZE"
    att = n.new("GeometryNodeInputMaterialIndex")     # hull faces = slot 1 (survives FBX round trips)
    gt = n.new("FunctionNodeCompare")
    gt.data_type = "INT"
    gt.operation = "EQUAL"
    gt.inputs["B"].default_value = HULL_SLOT
    sc = n.new("ShaderNodeVectorMath")
    sc.operation = "SCALE"
    l.new(gi.outputs["Geometry"], sp.inputs["Geometry"])
    l.new(att.outputs["Material Index"], gt.inputs["A"])
    l.new(gt.outputs["Result"], sp.inputs["Selection"])
    l.new(nrm.outputs["Normal"], unit.inputs[0])
    l.new(unit.outputs["Vector"], sc.inputs[0])
    l.new(gi.outputs["Extra"], sc.inputs["Scale"])
    l.new(sc.outputs["Vector"], sp.inputs["Offset"])
    l.new(sp.outputs["Geometry"], go.inputs["Geometry"])
    return ng


def preview_self_test() -> dict:
    """Bake a hull on a unit sphere, ask the preview for 3.0 cm and 0.9 cm and measure the evaluated hull radius.
    Returns {'grow': cm, 'shrink': cm, 'ok': bool} (ok = both within 0.5 mm of the request)."""
    me = bpy.data.meshes.new("_af_hull_test")
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=24, v_segments=12, radius=0.5)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    ob = bpy.data.objects.new("_af_hull_test", me)
    bpy.context.scene.collection.objects.link(ob)
    bake_hull(ob, 0.018)

    def hull_r() -> float:
        dg = bpy.context.evaluated_depsgraph_get()
        ev = ob.evaluated_get(dg)
        m = ev.to_mesh()
        co = np.zeros(len(m.vertices) * 3)
        m.vertices.foreach_get("co", co)
        co = co.reshape(-1, 3)
        vs = sorted({v for p in m.polygons if p.material_index == HULL_SLOT for v in p.vertices})
        r = float(np.linalg.norm(co[vs], axis=1).max())
        ev.to_mesh_clear()
        return r
    out = {}
    for tag, w in (("grow", 0.030), ("shrink", 0.009)):
        set_preview_width(ob, w)
        out[tag] = round((hull_r() - 0.5) * 100.0, 3)
    out["ok"] = abs(out["grow"] - 3.0) < 0.05 and abs(out["shrink"] - 0.9) < 0.05
    bpy.data.objects.remove(ob)
    bpy.data.meshes.remove(me)
    return out


def set_preview_width(obj: bpy.types.Object, target_width: float, baked: float | None = None) -> None:
    """Grow/shrink the baked hull to ``target_width`` metres for a review render (after skinning)."""
    baked = float(obj.get("af_outline_width", 0.0)) if baked is None else baked
    mod = obj.modifiers.get(PREVIEW_MOD)
    if mod is None:
        mod = obj.modifiers.new(PREVIEW_MOD, "NODES")
        mod.node_group = _preview_group()
    ident = next(s.identifier for s in mod.node_group.interface.items_tree
                 if getattr(s, "in_out", "") == "INPUT" and s.name == "Extra")
    mod[ident] = float(target_width - baked)
    obj.update_tag()


def remove_preview(obj: bpy.types.Object) -> None:
    mod = obj.modifiers.get(PREVIEW_MOD)
    if mod is not None:
        obj.modifiers.remove(mod)


def pixel_world_size(cam_obj: bpy.types.Object, point: Vector, res_x: int) -> float:
    """World metres covered by one pixel at ``point`` for a perspective camera (horizontal fit)."""
    cam = cam_obj.data
    if cam.type == "ORTHO":
        return cam.ortho_scale / res_x
    d = (cam_obj.matrix_world.inverted() @ Vector(point)).z * -1.0
    import math
    return 2.0 * d * math.tan(cam.angle_x / 2.0) / res_x


def default_px_at_game_distance(width_m: float, res_x: int = 1920) -> float:
    """Screen width (px) of a baked hull at the default game camera distance, for a given resolution."""
    import math
    wpp = 2.0 * shading.CAM_DIST_DEFAULT * math.tan(math.radians(shading.CAM_FOV_H / 2.0)) / res_x
    return width_m / wpp

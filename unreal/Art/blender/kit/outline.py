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
from mathutils import Matrix, Vector

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


# ── what M_AF_Outline does in UE (screen-constant ink) + the exact preview of it ────────────────────────
# The baked hull alone (mobile low: no WPO) is ``WIDTH_CM`` world units — hero 1.8 cm ≈ 2.2 px at the default W1
# distance. Every other quality level grows it in the vertex shader (World Position Offset of the outline material)
# so that its far side is the body silhouette **dilated by exactly OutlinePx1080 × ViewSizeY / 1080 pixels in screen
# space** (spec §1.3 "extrusion … in clip space so thickness is constant on screen"):
#
#   P      = AbsoluteWorldPosition (excluding material offsets)     the hull vertex = body vertex + N × BakedWidth
#   N      = VertexNormalWS                                         the imported custom normal (outward, smoothed)
#   V      = normalize(P − CameraPosition)
#   depth  = dot(P − CameraPosition, CameraForwardWS)               view-space Z (TransformPosition World→View).z
#   Np     = N − dot(N, V) · V                                      the normal's component across the view ray
#   dir    = Np / max(length(Np), INK_EPS)                          unit screen direction (normalised clip-space N)
#   width  = OutlinePx1080 · 2 · depth · tan(FOVy / 2) / 1080       ViewSize cancels: ∝ viewport height, any aspect
#   WPO    = dir · width − N · BakedWidth
#
# tan(FOVy/2) = tan(FOVx/2) · ViewSizeY / ViewSizeX (UE ViewProperty "Tan(0.5 × FieldOfView)".y). At the W1 default
# distance (depth 25.37 m at the focus, 16:9) 1 px at 1080p = 0.833 cm, so the hero's 3.5 px = 2.92 cm.
# Why the normalised screen direction and not a plain push along N (``N · (width − BakedWidth)``, the formula until
# art review 6): a push along N reaches only sin(angle(N, V)) of the request on screen — on sheet edges, plate rims,
# blade bevels and plume strands tilted toward the camera the ink fell to ~0.7 of the request (the warrior's front
# view measured 77 % of silhouette edges ≥ 3 px for a 3.5 px request); the normalised direction gives the full width
# wherever the normal is more than asin(INK_EPS) ≈ 14° off the view ray, i.e. everywhere near a silhouette.
# Normals within that cone (interior, facing the camera or straight away) fade linearly, which keeps the
# interior stable.
INK_EPS = 0.25
# Hull colour (M_AF_Outline): mix(#120C18 ink, line(c), o), o = palette P.a. Spec §1.3: heroes / monsters / NPCs
# ink, interactive props and decor their line tone. Characters keep a 40 % hint of the local line tone (the
# warrior's 0.4: 60 % ink, hull lum ≤ 33 for any base colour); line-tone classes use o = 1 (≤ lum 61 by construction).
INK_MIX = {
    "hero": 0.4, "boss": 0.4, "npc": 0.4, "monster": 0.4, "small_monster": 0.4, "weapon": 0.4,
    "interactive": 1.0, "decor": 1.0, "none": 1.0,
}
INK_CLASSES = ("hero", "boss", "npc", "monster", "small_monster", "weapon")
# The ink gate (``kit.review``): at native 1080p pixels, with the shipped (screen-constant) width, ≥ GATE_FRAC of
# the silhouette edge pixels must carry a rim of ≥ (OutlinePx1080 − GATE_SLACK_PX) px (hero 3.5 → 3.0 px; NPC /
# monster 3.0 → 2.5; interactive 2.0 → 1.5; decor 1.2 → 0.7), have a pixel darker than the dark limit within 3 px,
# and the solid hull pixels' 95th-percentile luminance must stay ≤ the limit (ink classes lum 40 = the line tone of
# the steel / crimson regions; line-tone classes 64 = above any line(c)).
GATE_SLACK_PX = 0.5
GATE_FRAC = 0.90
GATE_DARK_LUM = {"ink": 40.0, "line": 64.0}

PREVIEW_GROUP = "AF_OutlinePreview_v3"
PREVIEW_MOD = "AF_OutlinePreview"
# The preview group runs after the Armature modifier (posed object-space positions, custom normals rotated with
# the skin): offset = N·Extra (legacy push along N, ``set_preview_width``) + Screen·(dir·(Width0 + K·depth) −
# N·Baked) (the UE WPO above, ``set_preview_screen``; camera position / forward passed in object space and
# refreshed before every review render by ``refresh_preview``). Blender ≥ 4.1's Normal node with
# ``legacy_corner_normals`` off returns the custom (imported) normals = UE's VertexNormalWS. (Until art review 6
# the group negated the normal, assuming it followed the flipped hull winding: every game view grew the hull the
# wrong way — asked 2.9 cm, got 0.7 cm — so the 1080p previews showed almost no ink.)
_INPUTS = (("Extra", "NodeSocketFloat", 0.0), ("Screen", "NodeSocketFloat", 0.0), ("Width0", "NodeSocketFloat", 0.0),
           ("K", "NodeSocketFloat", 0.0), ("Baked", "NodeSocketFloat", 0.0), ("Cam", "NodeSocketVector", None),
           ("Fwd", "NodeSocketVector", None), ("Ortho", "NodeSocketFloat", 0.0), ("Eps", "NodeSocketFloat", INK_EPS))


def _preview_group() -> bpy.types.NodeTree:
    ng = bpy.data.node_groups.get(PREVIEW_GROUP)
    if ng is not None:
        return ng
    ng = bpy.data.node_groups.new(PREVIEW_GROUP, "GeometryNodeTree")
    ng.interface.new_socket("Geometry", in_out="INPUT", socket_type="NodeSocketGeometry")
    for name, ty, dv in _INPUTS:
        s = ng.interface.new_socket(name, in_out="INPUT", socket_type=ty)
        if dv is not None:
            s.default_value = dv
    ng.interface.new_socket("Geometry", in_out="OUTPUT", socket_type="NodeSocketGeometry")
    n, l = ng.nodes, ng.links
    gi, go = n.new("NodeGroupInput"), n.new("NodeGroupOutput")

    def vm(op, a, b=None, scale=None):
        nd = n.new("ShaderNodeVectorMath")
        nd.operation = op
        l.new(a, nd.inputs[0])
        if b is not None:
            l.new(b, nd.inputs[1])
        if scale is not None:
            l.new(scale, nd.inputs["Scale"])
        return nd.outputs["Value"] if op in ("DOT_PRODUCT", "LENGTH") else nd.outputs["Vector"]

    def fm(op, a, b):
        nd = n.new("ShaderNodeMath")
        nd.operation = op
        for k, v in enumerate((a, b)):
            if isinstance(v, (int, float)):
                nd.inputs[k].default_value = float(v)
            else:
                l.new(v, nd.inputs[k])
        return nd.outputs[0]
    sp = n.new("GeometryNodeSetPosition")
    nrm = n.new("GeometryNodeInputNormal")
    if hasattr(nrm, "legacy_corner_normals"):
        nrm.legacy_corner_normals = False             # custom normals = UE's VertexNormalWS
    pos = n.new("GeometryNodeInputPosition").outputs["Position"]
    N = vm("NORMALIZE", nrm.outputs["Normal"])
    D = vm("SUBTRACT", pos, gi.outputs["Cam"])
    mix = n.new("ShaderNodeMix")
    mix.data_type = "VECTOR"
    mix.clamp_factor = True
    l.new(gi.outputs["Ortho"], mix.inputs[0])
    l.new(vm("NORMALIZE", D), mix.inputs[4])
    l.new(gi.outputs["Fwd"], mix.inputs[5])
    V = vm("NORMALIZE", mix.outputs[1])
    depth = vm("DOT_PRODUCT", D, gi.outputs["Fwd"])
    target = fm("ADD", gi.outputs["Width0"], fm("MULTIPLY", gi.outputs["K"], depth))
    Np = vm("SUBTRACT", N, vm("SCALE", V, scale=vm("DOT_PRODUCT", N, V)))
    inv = fm("DIVIDE", 1.0, fm("MAXIMUM", vm("LENGTH", Np), gi.outputs["Eps"]))
    off_s = vm("SUBTRACT", vm("SCALE", Np, scale=fm("MULTIPLY", inv, target)),
               vm("SCALE", N, scale=gi.outputs["Baked"]))
    off = vm("ADD", vm("SCALE", N, scale=gi.outputs["Extra"]), vm("SCALE", off_s, scale=gi.outputs["Screen"]))
    att = n.new("GeometryNodeInputMaterialIndex")     # hull faces = slot 1 (survives FBX round trips)
    gt = n.new("FunctionNodeCompare")
    gt.data_type = "INT"
    gt.operation = "EQUAL"
    gt.inputs["B"].default_value = HULL_SLOT
    l.new(gi.outputs["Geometry"], sp.inputs["Geometry"])
    l.new(att.outputs["Material Index"], gt.inputs["A"])
    l.new(gt.outputs["Result"], sp.inputs["Selection"])
    l.new(off, sp.inputs["Offset"])
    l.new(sp.outputs["Geometry"], go.inputs["Geometry"])
    return ng


def _modifier(obj: bpy.types.Object):
    mod = obj.modifiers.get(PREVIEW_MOD)
    if mod is None:
        mod = obj.modifiers.new(PREVIEW_MOD, "NODES")
    if mod.node_group is None or mod.node_group.name != PREVIEW_GROUP:
        mod.node_group = _preview_group()
    return mod


def _set_inputs(mod, **values) -> None:
    ids = {s.name: s.identifier for s in mod.node_group.interface.items_tree
           if getattr(s, "in_out", "") == "INPUT" and s.socket_type != "NodeSocketGeometry"}
    for k, v in values.items():
        mod[ids[k]] = tuple(float(c) for c in v) if hasattr(v, "__len__") else float(v)


def set_preview_width(obj: bpy.types.Object, target_width: float, baked: float | None = None) -> None:
    """Grow/shrink the baked hull to ``target_width`` metres **along N** for a review render (after skinning).
    ``set_preview_width(obj, baked)`` = the baked hull alone (mobile low). Screen-constant ink: ``set_preview_screen``."""
    baked = float(obj.get("af_outline_width", 0.0)) if baked is None else baked
    mod = _modifier(obj)
    _set_inputs(mod, Extra=target_width - baked, Screen=0.0, Width0=0.0, K=0.0, Baked=baked)
    for k in ("af_ink_mode", "af_ink_value"):
        if k in obj:
            del obj[k]
    obj.update_tag()


def set_preview_screen(obj: bpy.types.Object, px: float | None = None, world: float | None = None,
                       cam_obj: bpy.types.Object | None = None) -> None:
    """The UE outline WPO (see above) for a review render: ``px`` = ink width in pixels **of the current render
    resolution** (per-vertex view depth, exactly what UE draws), or ``world`` = a constant width in metres pushed
    across the view ray (close-ups showing the shipped weight magnified). The camera inputs are refreshed before
    every ``review.render_array`` (``refresh_preview``), so poses, facings and attached weapons stay exact."""
    if (px is None) == (world is None):
        raise ValueError("set_preview_screen: give exactly one of px / world")
    _modifier(obj)
    obj["af_ink_mode"] = "px" if px is not None else "world"
    obj["af_ink_value"] = float(px if px is not None else world)
    _apply_screen(obj, cam_obj or bpy.context.scene.camera)


def screen_k(cam_obj: bpy.types.Object, px: float, res_x: int | None = None) -> tuple[float, float, bool]:
    """(K, Width0, ortho) such that ``Width0 + K × depth`` is ``px`` render pixels at view depth ``depth``."""
    import math
    sc = bpy.context.scene
    res_x = res_x or int(sc.render.resolution_x * sc.render.resolution_percentage / 100)
    cam = cam_obj.data
    if cam.type == "ORTHO":
        return 0.0, px * cam.ortho_scale / res_x, True
    return px * 2.0 * math.tan(cam.angle_x / 2.0) / res_x, 0.0, False


def _apply_screen(obj: bpy.types.Object, cam_obj: bpy.types.Object | None) -> None:
    if cam_obj is None:
        return
    mod = obj.modifiers.get(PREVIEW_MOD)
    mode = obj.get("af_ink_mode")
    if mod is None or mode not in ("px", "world"):
        return
    inv = obj.matrix_world.inverted()
    cw = cam_obj.matrix_world
    fwd = -(cw.to_3x3() @ Vector((0.0, 0.0, 1.0))).normalized()
    cam_l = inv @ cw.translation
    fwd_l = (inv.to_3x3() @ fwd).normalized()
    if mode == "px":
        k, w0, ortho = screen_k(cam_obj, float(obj["af_ink_value"]))
    else:
        k, w0, ortho = 0.0, float(obj["af_ink_value"]), cam_obj.data.type == "ORTHO"
    _set_inputs(mod, Extra=0.0, Screen=1.0, Width0=w0, K=k, Baked=float(obj.get("af_outline_width", 0.0)),
                Cam=cam_l, Fwd=fwd_l, Ortho=1.0 if ortho else 0.0, Eps=INK_EPS)
    obj.update_tag()


def refresh_preview(scene: bpy.types.Scene | None = None) -> int:
    """Re-aim every screen-constant preview hull at the scene camera (object transforms, poses and the camera may
    have changed since ``set_preview_screen``). Called by ``review.render_array`` before each render."""
    sc = scene or bpy.context.scene
    n = 0
    for o in sc.objects:
        if o.type == "MESH" and o.get("af_ink_mode") in ("px", "world"):
            _apply_screen(o, sc.camera)
            n += 1
    return n


def remove_preview(obj: bpy.types.Object) -> None:
    mod = obj.modifiers.get(PREVIEW_MOD)
    if mod is not None:
        obj.modifiers.remove(mod)
    for k in ("af_ink_mode", "af_ink_value"):
        if k in obj:
            del obj[k]


def _evaluated_hull(ob) -> tuple[np.ndarray, np.ndarray]:
    dg = bpy.context.evaluated_depsgraph_get()
    ev = ob.evaluated_get(dg)
    m = ev.to_mesh()
    co = np.zeros(len(m.vertices) * 3)
    m.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3) @ np.array(ob.matrix_world)[:3, :3].T + np.array(ob.matrix_world)[:3, 3]
    hull = sorted({v for p in m.polygons if p.material_index == HULL_SLOT for v in p.vertices})
    body = sorted({v for p in m.polygons if p.material_index != HULL_SLOT for v in p.vertices})
    ev.to_mesh_clear()
    return co[body], co[hull]


def preview_self_test() -> dict:
    """Checks the preview against the formulas above (no render):

    * ``grow`` / ``shrink``: a unit sphere's hull asked for 3.0 / 0.9 cm along N (``set_preview_width``);
    * ``screenPx``: the same sphere 25 m in front of a 35° camera asked for 3.5 px at 1920 wide
      (``set_preview_screen``): projected hull radius − body radius in pixels;
    * ``sheetPx`` / ``sheetPxAlongN``: a 1 cm thick plate tilted 70° toward that camera, its near edge on the
      silhouette — projected ink beyond the edge with the screen-constant WPO vs the old push along N.
    Returns the measurements + ``ok`` (sphere within 0.5 mm, screen widths within 0.15 px)."""
    import math
    import bmesh as _bm
    made = []

    def obj(name, build):
        me = bpy.data.meshes.new(name)
        bm = _bm.new()
        build(bm)
        bm.to_mesh(me)
        bm.free()
        for p in me.polygons:
            p.use_smooth = True
        ob = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(ob)
        bake_hull(ob, 0.018)
        made.append((ob, me))
        return ob
    ob = obj("_af_hull_test", lambda bm: _bm.ops.create_uvsphere(bm, u_segments=24, v_segments=12, radius=0.5))
    out = {}
    for tag, w in (("grow", 0.030), ("shrink", 0.009)):
        set_preview_width(ob, w)
        _, hull = _evaluated_hull(ob)
        out[tag] = round((float(np.linalg.norm(hull, axis=1).max()) - 0.5) * 100.0, 3)
    cam = bpy.data.objects.new("_af_hull_cam", bpy.data.cameras.new("_af_hull_cam"))
    bpy.context.scene.collection.objects.link(cam)
    cam.data.sensor_fit = "HORIZONTAL"
    cam.data.angle = math.radians(35.0)
    cam.location = (0.0, -25.0, 0.0)
    cam.rotation_euler = (math.radians(90.0), 0.0, 0.0)          # looks along +Y
    sc = bpy.context.scene
    saved = (sc.camera, sc.render.resolution_x, sc.render.resolution_y, sc.render.resolution_percentage)
    sc.camera = cam
    sc.render.resolution_x, sc.render.resolution_y, sc.render.resolution_percentage = 1920, 1080, 100
    bpy.context.view_layer.update()
    f = 1920 / (2.0 * math.tan(math.radians(17.5)))

    def proj(p):                                               # screen x/z offsets in px from the image centre
        d = p[:, 1] + 25.0
        return np.stack([p[:, 0] / d * f, p[:, 2] / d * f], 1)
    set_preview_screen(ob, px=3.5)
    body, hull = _evaluated_hull(ob)
    out["screenPx"] = round(float(np.linalg.norm(proj(hull), axis=1).max() - np.linalg.norm(proj(body), axis=1).max()), 3)

    def plate_bm(bm):          # 60 × 60 × 1 cm plate, its edges subdivided (mid-edge normals = side + face only)
        _bm.ops.create_cube(bm, size=1.0, matrix=Matrix.Diagonal((0.6, 0.6, 0.01, 1.0)))
        _bm.ops.subdivide_edges(bm, edges=[e for e in bm.edges
                                           if abs(e.verts[0].co.z - e.verts[1].co.z) < 1e-6], cuts=7)
    plate = obj("_af_hull_plate", plate_bm)
    plate.rotation_euler = (math.radians(-70.0), 0.0, 0.0)      # near edge (−Y) lifted to the top of the frame
    bpy.context.view_layer.update()
    for tag, fn in (("sheetPx", lambda: set_preview_screen(plate, px=3.5)),
                    ("sheetPxAlongN", lambda: set_preview_width(plate, 3.5 * 24.9 / f))):
        fn()
        refresh_preview()
        body, hull = _evaluated_hull(plate)
        out[tag] = round(float(proj(hull)[:, 1].max() - proj(body)[:, 1].max()), 3)
    sc.camera, sc.render.resolution_x, sc.render.resolution_y, sc.render.resolution_percentage = saved
    for o, m in made:
        bpy.data.objects.remove(o)
        bpy.data.meshes.remove(m)
    bpy.data.objects.remove(cam)
    out["ok"] = (abs(out["grow"] - 3.0) < 0.05 and abs(out["shrink"] - 0.9) < 0.05
                 and abs(out["screenPx"] - 3.5) < 0.15 and abs(out["sheetPx"] - 3.5) < 0.15)
    return out


def pixel_world_size(cam_obj: bpy.types.Object, point: Vector, res_x: int) -> float:
    """World metres covered by one pixel at ``point`` for a perspective camera (horizontal fit)."""
    cam = cam_obj.data
    if cam.type == "ORTHO":
        return cam.ortho_scale / res_x
    d = (cam_obj.matrix_world.inverted() @ Vector(point)).z * -1.0
    import math
    return 2.0 * d * math.tan(cam.angle_x / 2.0) / res_x


def ink_width_world(px_1080: float, depth: float, fov_h: float = shading.CAM_FOV_H,
                    aspect: float = 16.0 / 9.0) -> float:
    """The UE WPO width (metres) at view depth ``depth`` for ``OutlinePx1080`` (Python reference of the formula)."""
    import math
    tan_y = math.tan(math.radians(fov_h) / 2.0) / aspect
    return px_1080 * 2.0 * depth * tan_y / 1080.0


def default_px_at_game_distance(width_m: float, res_x: int = 1920) -> float:
    """Screen width (px) of a baked hull at the default game camera distance, for a given resolution."""
    import math
    wpp = 2.0 * shading.CAM_DIST_DEFAULT * math.tan(math.radians(shading.CAM_FOV_H / 2.0)) / res_x
    return width_m / wpp


def hull_rgb(base_hex: str, o: float) -> tuple[float, float, float]:
    """Python reference of the hull colour (sRGB 0..255): mix(#120C18, line(c), o)."""
    from . import color
    ink = color.hex_to_rgb(shading.INK_HEX)
    line = color.tone(base_hex)["line"]
    return color.mix(ink, line, o)


def lum(rgb) -> float:
    return 0.299 * rgb[0] + 0.587 * rgb[1] + 0.114 * rgb[2]


def ink_regions(regions: dict, outline_class: str) -> dict:
    """``{name: Region}`` with every region's outline mix set to the class's ``INK_MIX`` (the warrior pattern):
    ``b.regions(**outline.ink_regions(REGIONS, "monster"))``."""
    from dataclasses import replace
    o = INK_MIX[outline_class]
    return {k: replace(r, o=o) for k, r in regions.items()}


def dark_limit(outline_class: str) -> float:
    return GATE_DARK_LUM["ink" if outline_class in INK_CLASSES else "line"]


def hull_color_report(obj: bpy.types.Object, palette, outline_class: str) -> list[str]:
    """Swatches drawn by ``obj``'s hull whose colour is lighter than the class's dark limit (ink classes: lum 40 —
    a hull lighter than that reads as a soft halo, not ink). Empty = OK. ``asset.finish_mesh`` prints these."""
    me = obj.data
    if "af_swatch" not in me.attributes or "af_hull" not in me.attributes:
        return []
    sw = np.zeros(len(me.polygons), np.int32)
    hl = np.zeros(len(me.polygons), np.int32)
    me.attributes["af_swatch"].data.foreach_get("value", sw)
    me.attributes["af_hull"].data.foreach_get("value", hl)
    lim = dark_limit(outline_class)
    out = []
    for i in sorted(set(sw[hl > 0].tolist())):
        s = palette.swatches[i] if 0 <= i < len(palette.swatches) else None
        if s is None:
            continue
        L = lum(hull_rgb(s["hex"], s["o"]))
        if L > lim:
            out.append(f"swatch {i} {s['hex']} o={s['o']:.2f} ({', '.join(s['names'][:3])}): hull lum {L:.0f} > {lim:.0f}")
    return out


def protrusion_report(obj: bpy.types.Object, min_mm: float = 1.0, names: list[str] | None = None,
                      max_mm: float = 40.0) -> list[dict]:
    """Parts whose surface sticks **out of the ink**: for every toon vertex, the signed distance to the hull's
    source surface (hull vertices moved back by the baked width along their stored normal = the surface the
    screen-constant ink is measured from). A no-hull trim, rivet or crease strip standing ``d`` mm proud of the
    plate that carries the hull, or a detailed part bulging out of its low-poly hull proxy, eats ``d`` of the ink
    wherever it reaches the silhouette (0.83 cm = 1 px at 1080p at the default W1 distance). Returns
    ``[{part, maxMm, px1080, verts}]`` (max outside distance per part ≥ ``min_mm``), worst first; bind pose.
    Points more than ``max_mm`` from every hull surface are not "proud" of a plate but enclosed some other way (a
    shield face between its rims) and are ignored."""
    from mathutils.bvhtree import BVHTree
    me = obj.data
    width = float(obj.get("af_outline_width", 0.0))
    if width <= 0 or "AF_Data" not in me.color_attributes:
        return []
    nv = len(me.vertices)
    co = np.zeros(nv * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(nv, 3)
    afd = me.color_attributes["AF_Data"]
    dat = np.zeros(len(afd.data) * 4, np.float32)
    afd.data.foreach_get("color", dat)
    dat = dat.reshape(-1, 4)
    mi = np.zeros(len(me.polygons), np.int32)
    me.polygons.foreach_get("material_index", mi)
    part = np.zeros(len(me.polygons), np.int32)
    if "af_part" in me.attributes:
        me.attributes["af_part"].data.foreach_get("value", part)
    hull_v = sorted({v for p in me.polygons if mi[p.index] == HULL_SLOT for v in p.vertices})
    if not hull_v:
        return []
    src = co.copy()
    if afd.domain == "POINT":
        src[hull_v] = co[hull_v] - (dat[hull_v, :3] * 2.0 - 1.0) * width
    # one closed hull volume per hull part (a point inside any of them is covered; nearest-surface signs alone
    # are wrong where volumes overlap)
    comps: dict[int, list] = {}
    for p in me.polygons:
        if mi[p.index] == HULL_SLOT:
            comps.setdefault(int(part[p.index]), []).append(tuple(p.vertices))
    vols = []
    pad_m = max_mm / 1000.0
    for pid_, polys in comps.items():
        vs = sorted({v for f in polys for v in f})
        lo, hi = src[vs].min(0) - pad_m, src[vs].max(0) + pad_m
        vols.append((lo, hi, BVHTree.FromPolygons([tuple(v) for v in src], polys)))
    vpart = {}
    for p in me.polygons:
        if mi[p.index] != HULL_SLOT:
            for v in p.vertices:
                vpart.setdefault(v, int(part[p.index]))
    worst: dict[int, list] = {}
    for v, pid in vpart.items():
        c = co[v]
        best = None
        for lo, hi, bvh in vols:
            if np.any(c < lo) or np.any(c > hi):
                continue
            hit = bvh.find_nearest(Vector(c))
            if hit[0] is None:
                continue
            d = (Vector(c) - hit[0]).dot(-hit[1])      # hull faces are flipped: outward = −face normal
            best = d if best is None else min(best, d)
            if best <= 0:
                break
        if best is not None and min_mm / 1000.0 < best <= pad_m:
            w = worst.setdefault(pid, [0.0, 0])
            w[0] = max(w[0], best)
            w[1] += 1
    mm_per_px = ink_width_world(1.0, shading.CAM_DIST_DEFAULT) * 1000.0
    out = [{"part": (names[pid] if names and pid < len(names) else str(pid)), "maxMm": round(d * 1000.0, 2),
            "px1080": round(d * 1000.0 / mm_per_px, 2), "verts": n} for pid, (d, n) in worst.items()]
    return sorted(out, key=lambda r: -r["maxMm"])


def outline_manifest() -> dict:
    """``manifest.json → shading.outline``: everything the UE M_AF_Outline material needs to reproduce the preview."""
    from . import color
    d = shading.CAM_DIST_DEFAULT
    return {
        "model": "inverted hull baked into every mesh (P7/R3): material slot 1 M_AF_Outline, unlit, opaque, default "
                 "back-face culling (the hull's winding is flipped, so its far side draws the rim); same skin weights",
        "color": {
            "formula": "out = srgb_to_linear(mix(ink, line(c), o)); line(c) = mix(mix(c, lineTint, lineTintMix), 0, "
                       "lineDarken); c = T_AF_Palette_BC(uv) (sRGB values), o = T_AF_Palette_P(uv).a",
            "ink": shading.INK_HEX, "lineTint": list(color.LINE_TINT), "lineTintMix": color.LINE_TINT_MIX,
            "lineDarken": color.LINE_DARKEN, "inkMixByClass": dict(INK_MIX),
            "note": "mix(ink, line(c), o) is never lighter than line(c); ink classes keep hull lum <= 40 (o <= 0.4)"},
        "bakedWidthCm": dict(WIDTH_CM),
        "outlinePx1080ByClass": dict(SCREEN_PX_1080),
        "wpo": {
            "formula": ["P = AbsoluteWorldPosition (excluding material offsets)",
                        "N = VertexNormalWS (imported custom normal)",
                        "V = normalize(P - CameraPosition)",
                        "depth = TransformPosition(P, World->View).z  (= dot(P - CameraPosition, CameraForward))",
                        "Np = N - dot(N, V) * V",
                        "dir = Np / max(length(Np), eps)",
                        "width = OutlinePx1080 * 2 * depth * TanHalfFOV.y / 1080",
                        "WPO = dir * width - N * BakedWidth"],
            "eps": INK_EPS,
            "params": {"OutlinePx1080": "materialSlots[1].outlinePx1080 (per asset class)",
                       "BakedWidth": "materialSlots[1].widthCm / 100 (m) -> cm in UE",
                       "TanHalfFOV.y": "ViewProperty 'Tan(0.5 x FieldOfView)'.y = tan(FOVx/2) * ViewSizeY / ViewSizeX"},
            "meaning": "the hull's far side = the body silhouette dilated by OutlinePx1080 * ViewSizeY / 1080 pixels "
                       "in screen space (clip-space extrusion along the normalised projected normal)",
            "atDefaultDistance": {"depthCm": round(d * 100.0, 1),
                                  "cmPerPx1080": round(ink_width_world(1.0, d) * 100.0, 4),
                                  "heroInkCm": round(ink_width_world(SCREEN_PX_1080["hero"], d) * 100.0, 3)},
            "mobileLow": "WPO off: the baked width alone (hero 1.8 cm = "
                         f"{default_px_at_game_distance(WIDTH_CM['hero'] / 100.0):.2f} px at the default distance)",
            "preview": "kit.outline.set_preview_screen (Geometry Nodes, same formula per vertex)"},
        "gate": {"rimPx": "outlinePx1080 - " + str(GATE_SLACK_PX), "fracEdges": GATE_FRAC,
                 "darkLum": dict(GATE_DARK_LUM), "darkWithinPx": 3, "measuredAt": "native 1080p, W1 camera"},
    }

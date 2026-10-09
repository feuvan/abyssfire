"""Toon shading constants + Cycles preview materials that reproduce ``M_AF_Toon`` / ``M_AF_Outline`` exactly.

The preview is an *emission-only* Cycles material: the colour the UE unlit toon material computes is
evaluated per shading point and emitted, so a 1–8 sample Cycles render is converged and pixel-comparable with
UE (view transform "Standard" = plain sRGB encoding, like UE with the toon grade disabled).

Per pixel (all tone maths on **sRGB-encoded** values, spec art-inventory-ch1.md §1.2–§1.4):

    c, (s, l, e, o) = T_AF_Palette_BC(uv), T_AF_Palette_P(uv)          nearest, no mips
    ndl   = dot(N, L_KEY)                     N = shading normal (world), L_KEY = toward the key light
    h     = ndl * 0.5 + 0.5                   half-Lambert
    shade = mix(mix(c, (30,20,60)/255, .35), 0, s)
    light = mix(c, (255,244,214)/255, l)
    col   = mix(shade, c,     smoothstep(T_SHADE - W/2, T_SHADE + W/2, h))      T_SHADE 0.42, W 0.02
    col   = mix(col,   light, smoothstep(T_LIGHT - W/2, T_LIGHT + W/2, h))      T_LIGHT 0.86
    col   = mix(col, #0A0818, 0.22 * g)       g = AF_Data.r (grounding weight baked per vertex, see mesh.py)
    fres  = (1 - max(dot(N, V), 0))^4                                           V = toward the camera
    rim   = 0.55 * smoothstep(RIM_T0, RIM_T1, fres) * smoothstep(0, 0.05, ndl)    banded Fresnel (see RIM_*)
    col   = mix(col, #FFECC8, rim)
    col   = mix(col, c, e)                    emissive regions are unshaded
    out   = srgb_to_linear(col) * (1 + e * EMISSIVE_BOOST)

Outline (inverted hull, second material slot): ink = mix(#120C18, line(c), o) with
line(c) = mix(mix(c, (20,10,30)/255, .5), 0, .55); the hull is drawn only where it faces the camera *after*
its winding was flipped (Cycles: Geometry.Backfacing → transparent; UE: default back-face culling).

``L_KEY`` (screen upper-left key light) — see ``key_light_derivation()``.
"""
from __future__ import annotations

import math
from typing import Sequence

import bpy
from mathutils import Vector

from . import color

# ── constants shared with M_AF_Toon (also written to manifest.json "shading") ───────────────────────────
T_SHADE = 0.42
T_LIGHT = 0.86
BAND_WIDTH = 0.02
RIM_HEX = "#FFECC8"
RIM_ALPHA = 0.55
RIM_POWER = 4.0
RIM_MASK_EDGE = 0.05
# Banded Fresnel (art review 6): the spec's power-4 Fresnel × 0.55 peaks only on the last grazing sliver, which the
# ink hull covers, so the rim never showed. The web rim is a crisp 0.9-unit warm stroke just inside the ink on the
# lit edges (Rig.ts:418-431); like the light bands, the Fresnel term is quantised: full 0.55 where
# (1 − N·V)^4 > 0.59^4 (N·V < 0.41), a 1-texel-style smoothstep of ±0.03 in (1 − N·V). At the W1 pitch that is a
# 2–3 px band on limbs and the helm and ≈ 5 px on the torso flank at 1080p, on the key-facing half only.
RIM_EDGE = 0.59
RIM_SOFT = 0.03
RIM_T0 = (RIM_EDGE - RIM_SOFT) ** RIM_POWER
RIM_T1 = (RIM_EDGE + RIM_SOFT) ** RIM_POWER
GROUND_TINT_HEX = "#0A0818"
GROUNDING_ALPHA = 0.22
GROUNDING_FROM = 0.38   # × character height: no darkening above this
GROUNDING_TO = 0.05     # × character height: full darkening below this
INK_HEX = "#120C18"
EMISSIVE_BOOST = 0.6    # preview overbright for e = 1 (UE: same factor before bloom)

# Camera (DECISIONS W1). UE yaw/pitch in degrees, horizontal FOV.
CAM_YAW_UE = 45.0
CAM_PITCH_UE = -50.0
CAM_FOV_H = 35.0
CAM_FOCUS_Z = 0.5
CAM_DIST_DEFAULT = 8.0 / math.tan(math.radians(CAM_FOV_H / 2.0))   # ≈ 25.37 m: 16 × 12 tiles on 16:9

# Key light: sun azimuth/elevation in UE terms (direction the light COMES FROM).
# Re-derived for the W1 camera (the spec's "yaw 0°, pitch −45°" sun was written for the old yaw −135° camera).
# Azimuth −45° is exactly the camera's left (camera right = UE (−0.707, 0.707)); at 55° elevation the light
# arrives from the screen's upper-left diagonal (view space: right −0.57, up +0.53, toward viewer +0.63), so:
# camera-facing fronts sit in the base band, tops and upper-left rims catch the warm light band, and the shade
# band falls on the lower-right of every form — the web cel() look. Faces turned to screen-right (the "se"
# 3/4 view) read darker, which is the 3D form cue the web could not show. Chosen on the kit smoke-test
# character among (−25°,58°), (−45°,55°), (−45°,45°), (−10°,50°) — see Art/blender/README.md.
KEY_FROM_YAW_UE = -45.0    # azimuth the light comes from (UE yaw; 0 = +X, −90 = −Y)
KEY_ELEVATION = 55.0       # degrees above the horizon


def _unit(v: Sequence[float]) -> Vector:
    return Vector(v).normalized()


def ue_dir_to_blender(v: Sequence[float]) -> Vector:
    return Vector((v[0], -v[1], v[2]))


def key_light_ue(yaw_from: float = KEY_FROM_YAW_UE, elevation: float = KEY_ELEVATION) -> Vector:
    """Unit vector toward the key light, UE axes (X fwd, Y right, Z up)."""
    y, e = math.radians(yaw_from), math.radians(elevation)
    return Vector((math.cos(e) * math.cos(y), math.cos(e) * math.sin(y), math.sin(e))).normalized()


L_KEY_UE = key_light_ue()
L_KEY = ue_dir_to_blender(L_KEY_UE)   # Blender world axes


def camera_basis_ue(yaw: float = CAM_YAW_UE, pitch: float = CAM_PITCH_UE) -> tuple[Vector, Vector, Vector]:
    """(forward, right, up) of the game camera in UE axes."""
    y, p = math.radians(yaw), math.radians(pitch)
    fwd = Vector((math.cos(p) * math.cos(y), math.cos(p) * math.sin(y), math.sin(p)))
    right = Vector((-math.sin(y), math.cos(y), 0.0))
    up = Vector((-math.sin(p) * math.cos(y), -math.sin(p) * math.sin(y), math.cos(p)))
    return fwd, right, up


def key_light_derivation() -> dict:
    """The key light expressed in the game camera's view space (for docs / manifest)."""
    f, r, u = camera_basis_ue()
    lv = (L_KEY_UE.dot(r), L_KEY_UE.dot(u), -L_KEY_UE.dot(f))
    return {"fromYawUE": KEY_FROM_YAW_UE, "elevationDeg": KEY_ELEVATION,
            "dirToLightUE": [round(c, 6) for c in L_KEY_UE],
            "dirToLightView": {"right": round(lv[0], 4), "up": round(lv[1], 4), "towardViewer": round(lv[2], 4)}}


def shading_manifest() -> dict:
    return {
        "model": "M_AF_Toon (unlit, custom 3-band half-Lambert, sRGB-space tone)",
        "tShade": T_SHADE, "tLight": T_LIGHT, "bandWidth": BAND_WIDTH,
        "toneConstants": {"coolShade": list(color.COOL_SHADE), "coolShadeMix": color.COOL_SHADE_MIX,
                          "warmLight": list(color.WARM_LIGHT), "lineTint": list(color.LINE_TINT),
                          "lineTintMix": color.LINE_TINT_MIX, "lineDarken": color.LINE_DARKEN},
        "rim": {"color": RIM_HEX, "alpha": RIM_ALPHA, "power": RIM_POWER, "maskEdge": RIM_MASK_EDGE,
                "band": [round(RIM_T0, 6), round(RIM_T1, 6)],
                "formula": "alpha * smoothstep(band0, band1, (1 - max(N.V, 0))^power) * smoothstep(0, maskEdge, N.L)"},
        "grounding": {"color": GROUND_TINT_HEX, "alpha": GROUNDING_ALPHA, "fromH": GROUNDING_FROM,
                      "toH": GROUNDING_TO, "source": "vertex colour AF_Data.r (baked weight, linear)"},
        "emissiveBoost": EMISSIVE_BOOST,
        "ink": INK_HEX,
        "keyLight": key_light_derivation(),
        "camera": {"yawUE": CAM_YAW_UE, "pitchUE": CAM_PITCH_UE, "fovH": CAM_FOV_H,
                   "defaultDistanceCm": round(CAM_DIST_DEFAULT * 100.0, 1), "focusZCm": CAM_FOCUS_Z * 100.0},
    }


# ── node-building helpers ───────────────────────────────────────────────────────────────────────────────
class NB:
    """Tiny node-graph builder: values or sockets accepted everywhere."""

    def __init__(self, tree: bpy.types.NodeTree):
        self.t = tree
        self.n = tree.nodes
        self.l = tree.links
        self._x = 0

    def node(self, kind: str, **props):
        nd = self.n.new(kind)
        for k, v in props.items():
            setattr(nd, k, v)
        nd.location = (self._x, 0)
        self._x += 40
        return nd

    def _set(self, sock, v):
        if isinstance(v, bpy.types.NodeSocket):
            self.l.new(v, sock)
        elif v is not None:
            if hasattr(sock, "default_value"):
                dv = sock.default_value
                if isinstance(v, (int, float)) and hasattr(dv, "__len__"):
                    sock.default_value = [float(v)] * len(dv)
                elif hasattr(dv, "__len__") and len(v) < len(dv):
                    sock.default_value = list(v) + [1.0] * (len(dv) - len(v))
                else:
                    sock.default_value = v

    def math(self, op: str, a, b=None, c=None, clamp=False):
        nd = self.node("ShaderNodeMath", operation=op, use_clamp=clamp)
        self._set(nd.inputs[0], a)
        if b is not None:
            self._set(nd.inputs[1], b)
        if c is not None:
            self._set(nd.inputs[2], c)
        return nd.outputs[0]

    def vmath(self, op: str, a, b=None, scale=None):
        nd = self.node("ShaderNodeVectorMath", operation=op)
        self._set(nd.inputs[0], a)
        if b is not None:
            self._set(nd.inputs[1], b)
        if scale is not None:
            self._set(nd.inputs["Scale"], scale)
        out = nd.outputs["Value"] if op in ("DOT_PRODUCT", "LENGTH", "DISTANCE") else nd.outputs["Vector"]
        return out

    def mix(self, fac, a, b):
        """Linear (unclamped) colour mix a→b by fac (vector-typed Mix → exact lerp)."""
        nd = self.node("ShaderNodeMix", data_type="VECTOR", clamp_factor=False)
        self._set(nd.inputs[0], fac)
        self._set(nd.inputs[4], a)
        self._set(nd.inputs[5], b)
        return nd.outputs[1]

    def smooth(self, x, lo, hi):
        nd = self.node("ShaderNodeMapRange", interpolation_type="SMOOTHSTEP", clamp=True)
        self._set(nd.inputs["Value"], x)
        nd.inputs["From Min"].default_value = lo
        nd.inputs["From Max"].default_value = hi
        nd.inputs["To Min"].default_value = 0.0
        nd.inputs["To Max"].default_value = 1.0
        return nd.outputs["Result"]

    def sep(self, v):
        nd = self.node("ShaderNodeSeparateXYZ")
        self._set(nd.inputs[0], v)
        return nd.outputs[0], nd.outputs[1], nd.outputs[2]

    def comb(self, x, y, z):
        nd = self.node("ShaderNodeCombineXYZ")
        self._set(nd.inputs[0], x)
        self._set(nd.inputs[1], y)
        self._set(nd.inputs[2], z)
        return nd.outputs[0]

    def const_vec(self, v):
        nd = self.node("ShaderNodeCombineXYZ")
        for i in range(3):
            nd.inputs[i].default_value = float(v[i])
        return nd.outputs[0]

    def srgb_to_linear(self, v):
        """Exact piecewise sRGB EOTF per channel."""
        chans = []
        for c in self.sep(v):
            c = self.math("MAXIMUM", c, 0.0)
            lo = self.math("DIVIDE", c, 12.92)
            hi = self.math("POWER", self.math("DIVIDE", self.math("ADD", c, 0.055), 1.055), 2.4)
            is_hi = self.math("GREATER_THAN", c, 0.04045)
            chans.append(self.math("ADD", self.math("MULTIPLY", hi, is_hi),
                                   self.math("MULTIPLY", lo, self.math("SUBTRACT", 1.0, is_hi))))
        return self.comb(*chans)


def _srgb01(hex_or_rgb) -> tuple[float, float, float]:
    c = color.hex_to_rgb(hex_or_rgb) if isinstance(hex_or_rgb, str) else hex_or_rgb
    return (c[0] / 255.0, c[1] / 255.0, c[2] / 255.0)


# ── node groups ─────────────────────────────────────────────────────────────────────────────────────────
TOON_GROUP = "AF_ToonCore"
OUTLINE_GROUP = "AF_OutlineCore"


def _group(name: str, inputs: list[tuple[str, str]], outputs: list[tuple[str, str]]):
    ng = bpy.data.node_groups.get(name)
    if ng is None:
        ng = bpy.data.node_groups.new(name, "ShaderNodeTree")
        for nm, ty in inputs:
            ng.interface.new_socket(nm, in_out="INPUT", socket_type=ty)
        for nm, ty in outputs:
            ng.interface.new_socket(nm, in_out="OUTPUT", socket_type=ty)
    else:   # rebuild the graph in place: same sockets, so materials keep their links
        ng.nodes.clear()
    gi = ng.nodes.new("NodeGroupInput")
    go = ng.nodes.new("NodeGroupOutput")
    return ng, gi, go


TOON_TERMS = {"rim": True, "grounding": True}     # the terms the AF_ToonCore group currently includes


def build_toon_group(l_key: Sequence[float] | None = None, rim: bool = True,
                     grounding: bool = True) -> bpy.types.NodeTree:
    """AF_ToonCore(Base sRGB, Params(s,l,e), Grounding) → Color (linear). ``rim`` / ``grounding`` = False leave
    that term out (review A/B cells only; every shipped preview uses the full stack)."""
    lk = Vector(l_key) if l_key is not None else L_KEY
    TOON_TERMS.update(rim=rim, grounding=grounding)
    ng, gi, go = _group(TOON_GROUP,
                        [("Base", "NodeSocketVector"), ("Params", "NodeSocketVector"), ("Grounding", "NodeSocketFloat")],
                        [("Color", "NodeSocketVector")])
    b = NB(ng)
    c = gi.outputs["Base"]
    s, l, e = b.sep(gi.outputs["Params"])
    geo = b.node("ShaderNodeNewGeometry")
    n = geo.outputs["Normal"]
    v = geo.outputs["Incoming"]
    ndl = b.vmath("DOT_PRODUCT", n, b.const_vec(lk))
    h = b.math("MULTIPLY_ADD", ndl, 0.5, 0.5)
    w = BAND_WIDTH / 2.0
    w_base = b.smooth(h, T_SHADE - w, T_SHADE + w)
    w_light = b.smooth(h, T_LIGHT - w, T_LIGHT + w)
    cool = _srgb01(color.COOL_SHADE)
    warm = _srgb01(color.WARM_LIGHT)
    shade0 = b.mix(color.COOL_SHADE_MIX, c, b.const_vec(cool))
    shade = b.vmath("SCALE", shade0, scale=b.math("SUBTRACT", 1.0, s))
    light = b.mix(l, c, b.const_vec(warm))
    col = b.mix(w_base, shade, c)
    col = b.mix(w_light, col, light)
    if grounding:
        col = b.mix(b.math("MULTIPLY", gi.outputs["Grounding"], GROUNDING_ALPHA), col,
                    b.const_vec(_srgb01(GROUND_TINT_HEX)))
    if rim:
        ndv = b.math("MAXIMUM", b.vmath("DOT_PRODUCT", n, v), 0.0)
        fres = b.math("POWER", b.math("SUBTRACT", 1.0, ndv), RIM_POWER)
        band = b.smooth(fres, RIM_T0, RIM_T1)
        rim_w = b.math("MULTIPLY", b.math("MULTIPLY", band, RIM_ALPHA), b.smooth(ndl, 0.0, RIM_MASK_EDGE))
        col = b.mix(rim_w, col, b.const_vec(_srgb01(RIM_HEX)))
    col = b.mix(e, col, c)
    lin = b.srgb_to_linear(col)
    lin = b.vmath("SCALE", lin, scale=b.math("MULTIPLY_ADD", e, EMISSIVE_BOOST, 1.0))
    ng.links.new(lin, go.inputs["Color"])
    return ng


def build_outline_group() -> bpy.types.NodeTree:
    """AF_OutlineCore(Base sRGB, OutlineMix) → Color (linear)."""
    ng, gi, go = _group(OUTLINE_GROUP, [("Base", "NodeSocketVector"), ("OutlineMix", "NodeSocketFloat")],
                        [("Color", "NodeSocketVector")])
    b = NB(ng)
    c = gi.outputs["Base"]
    line = b.vmath("SCALE", b.mix(color.LINE_TINT_MIX, c, b.const_vec(_srgb01(color.LINE_TINT))),
                   scale=1.0 - color.LINE_DARKEN)
    col = b.mix(gi.outputs["OutlineMix"], b.const_vec(_srgb01(INK_HEX)), line)
    ng.links.new(b.srgb_to_linear(col), go.inputs["Color"])
    return ng


def ensure_groups(force: bool = False) -> None:
    if force or bpy.data.node_groups.get(TOON_GROUP) is None:
        build_toon_group()
    if force or bpy.data.node_groups.get(OUTLINE_GROUP) is None:
        build_outline_group()


# ── materials ───────────────────────────────────────────────────────────────────────────────────────────
OUTLINE_MATERIAL = "M_AF_Outline"


def _load_image(path, name: str) -> bpy.types.Image:
    img = bpy.data.images.get(name)
    if img is not None and bpy.path.abspath(img.filepath) == str(path):
        img.reload()
    else:
        if img is not None:
            bpy.data.images.remove(img)
        img = bpy.data.images.load(str(path), check_existing=False)
        img.name = name
    img.colorspace_settings.name = "Non-Color"
    img.alpha_mode = "CHANNEL_PACKED"
    return img


def _palette_images(palette) -> tuple[bpy.types.Image, bpy.types.Image]:
    pbc, pp = palette.save()
    nbc, np_ = palette.texture_names()
    return _load_image(pbc, nbc), _load_image(pp, np_)


def _new_material(name: str) -> bpy.types.Material:
    m = bpy.data.materials.get(name)
    if m is None:
        m = bpy.data.materials.new(name)
    m.use_nodes = True
    m.node_tree.nodes.clear()
    return m


def _palette_samplers(b: NB, bc_img, p_img, uv_name: str = "UVMap"):
    uv = b.node("ShaderNodeUVMap", uv_map=uv_name)
    t_bc = b.node("ShaderNodeTexImage", image=bc_img, interpolation="Closest", extension="EXTEND")
    t_p = b.node("ShaderNodeTexImage", image=p_img, interpolation="Closest", extension="EXTEND")
    b.l.new(uv.outputs["UV"], t_bc.inputs["Vector"])
    b.l.new(uv.outputs["UV"], t_p.inputs["Vector"])
    return t_bc, t_p


def toon_material(palette) -> bpy.types.Material:
    """Preview material ``MI_AF_Toon_<Family>`` (slot 0 of every generated mesh)."""
    ensure_groups()
    bc_img, p_img = _palette_images(palette)
    m = _new_material(palette.material_name)
    b = NB(m.node_tree)
    t_bc, t_p = _palette_samplers(b, bc_img, p_img)
    attr = b.node("ShaderNodeAttribute", attribute_type="GEOMETRY", attribute_name="AF_Data")
    grp = b.node("ShaderNodeGroup", node_tree=bpy.data.node_groups[TOON_GROUP])
    b.l.new(t_bc.outputs["Color"], grp.inputs["Base"])
    b.l.new(t_p.outputs["Color"], grp.inputs["Params"])
    g, _, _ = b.sep(attr.outputs["Vector"])
    b.l.new(g, grp.inputs["Grounding"])
    em = b.node("ShaderNodeEmission")
    b.l.new(grp.outputs["Color"], em.inputs["Color"])
    em.inputs["Strength"].default_value = 1.0
    out = b.node("ShaderNodeOutputMaterial")
    b.l.new(em.outputs[0], out.inputs["Surface"])
    m["af_palette"] = palette.family
    return m


def outline_material(palette) -> bpy.types.Material:
    """Preview material ``M_AF_Outline`` (slot 1): ink where the flipped hull faces the camera."""
    ensure_groups()
    bc_img, p_img = _palette_images(palette)
    m = _new_material(OUTLINE_MATERIAL)
    b = NB(m.node_tree)
    t_bc, t_p = _palette_samplers(b, bc_img, p_img)
    grp = b.node("ShaderNodeGroup", node_tree=bpy.data.node_groups[OUTLINE_GROUP])
    b.l.new(t_bc.outputs["Color"], grp.inputs["Base"])
    b.l.new(t_p.outputs["Alpha"], grp.inputs["OutlineMix"])
    em = b.node("ShaderNodeEmission")
    b.l.new(grp.outputs["Color"], em.inputs["Color"])
    tr = b.node("ShaderNodeBsdfTransparent")
    geo = b.node("ShaderNodeNewGeometry")
    mx = b.node("ShaderNodeMixShader")
    b.l.new(geo.outputs["Backfacing"], mx.inputs["Fac"])
    b.l.new(em.outputs[0], mx.inputs[1])
    b.l.new(tr.outputs[0], mx.inputs[2])
    out = b.node("ShaderNodeOutputMaterial")
    b.l.new(mx.outputs[0], out.inputs["Surface"])
    return m


def emissive_pass_material(toon_mat: bpy.types.Material) -> bpy.types.Material:
    """Review-only bloom source for a palette toon material: the emissive regions' linear colour
    ``srgb_to_linear(c) · e · (1 + e·EMISSIVE_BOOST)`` (what UE's bloom picks up), black elsewhere."""
    grp = next((n for n in toon_mat.node_tree.nodes if n.type == "GROUP"), None)
    if grp is None or not grp.inputs["Base"].links:
        m = _new_material(toon_mat.name + "_EmissivePass")
        em = m.node_tree.nodes.new("ShaderNodeEmission")
        em.inputs["Color"].default_value = (0, 0, 0, 1)
        out = m.node_tree.nodes.new("ShaderNodeOutputMaterial")
        m.node_tree.links.new(em.outputs[0], out.inputs["Surface"])
        return m
    bc_img = grp.inputs["Base"].links[0].from_node.image
    p_img = grp.inputs["Params"].links[0].from_node.image
    m = _new_material(toon_mat.name + "_EmissivePass")
    b = NB(m.node_tree)
    t_bc, t_p = _palette_samplers(b, bc_img, p_img)
    _, _, e = b.sep(t_p.outputs["Color"])
    lin = b.srgb_to_linear(t_bc.outputs["Color"])
    k = b.math("MULTIPLY", e, b.math("MULTIPLY_ADD", e, EMISSIVE_BOOST, 1.0))
    em = b.node("ShaderNodeEmission")
    b._set(em.inputs["Color"], b.vmath("SCALE", lin, scale=k))
    out = b.node("ShaderNodeOutputMaterial")
    b.l.new(em.outputs[0], out.inputs["Surface"])
    return m


def flat_toon_material(name: str, hex_color: str, s: float = color.DEFAULT_SHADOW,
                       l: float = color.DEFAULT_LIGHT) -> bpy.types.Material:
    """Toon material with a constant region (stage props, ground): same maths, no textures."""
    ensure_groups()
    m = _new_material(name)
    b = NB(m.node_tree)
    grp = b.node("ShaderNodeGroup", node_tree=bpy.data.node_groups[TOON_GROUP])
    grp.inputs["Base"].default_value = _srgb01(hex_color)
    grp.inputs["Params"].default_value = (s, l, 0.0)
    em = b.node("ShaderNodeEmission")
    b.l.new(grp.outputs["Color"], em.inputs["Color"])
    out = b.node("ShaderNodeOutputMaterial")
    b.l.new(em.outputs[0], out.inputs["Surface"])
    return m


def stage_ground_material(name: str = "AF_StageGround", ground_hex: str = "#74A247",
                          blob_radius: float = 0.39, grid: bool = True) -> bpy.types.Material:
    """Review ground: plains grass, faint 1 m tile grid, web contact shadow (spec §1.4) at the origin.

    Blob: rgba(8,6,16, .45) at the centre → .26 at 65 % → 0 at ``blob_radius`` (a true circle in 3D).
    """
    m = _new_material(name)
    b = NB(m.node_tree)
    tc = b.node("ShaderNodeTexCoord")
    pos = tc.outputs["Object"]
    x, y, _ = b.sep(pos)
    r = b.math("DIVIDE", b.math("SQRT", b.math("ADD", b.math("MULTIPLY", x, x), b.math("MULTIPLY", y, y))),
               blob_radius)
    # piecewise linear gradient 0→.65→1
    a1 = b.math("ADD", 0.45, b.math("MULTIPLY", b.math("MINIMUM", b.math("DIVIDE", r, 0.65), 1.0), -0.19))
    a2 = b.math("MULTIPLY", 0.26, b.math("SUBTRACT", 1.0,
                                         b.math("DIVIDE", b.math("MAXIMUM", b.math("SUBTRACT", r, 0.65), 0.0), 0.35)))
    seg2 = b.math("GREATER_THAN", r, 0.65)
    alpha = b.math("ADD", b.math("MULTIPLY", a1, b.math("SUBTRACT", 1.0, seg2)), b.math("MULTIPLY", a2, seg2))
    alpha = b.math("MULTIPLY", b.math("MAXIMUM", alpha, 0.0), b.math("LESS_THAN", r, 1.0))
    base = b.const_vec(_srgb01(ground_hex))
    col = base
    if grid:
        def line(coord):
            f = b.math("FRACT", b.math("ADD", coord, 0.5))
            d = b.math("MINIMUM", f, b.math("SUBTRACT", 1.0, f))
            return b.math("LESS_THAN", d, 0.008)
        gl = b.math("MAXIMUM", line(x), line(y))
        col = b.mix(b.math("MULTIPLY", gl, 0.18), col, b.const_vec(_srgb01("#2F4A1C")))
    col = b.mix(alpha, col, b.const_vec(_srgb01((8, 6, 16))))
    em = b.node("ShaderNodeEmission")
    b._set(em.inputs["Color"], b.srgb_to_linear(col))
    out = b.node("ShaderNodeOutputMaterial")
    b.l.new(em.outputs[0], out.inputs["Surface"])
    return m


# ── Python reference of M_AF_Toon (used by tests; mirror it in the UE material) ─────────────────────────
def _smoothstep(e0: float, e1: float, x: float) -> float:
    t = min(1.0, max(0.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


def toon_reference(base_hex: str, s: float, l: float, e: float, n: Sequence[float], v: Sequence[float],
                   grounding: float = 0.0, l_key: Sequence[float] | None = None) -> tuple[float, float, float]:
    """Final display colour (sRGB 0..255) of M_AF_Toon for a normal ``n`` and view vector ``v`` (toward the
    camera), Blender world axes. Equals the Cycles preview to 8-bit precision."""
    lk = Vector(l_key) if l_key is not None else L_KEY
    N, V = Vector(n).normalized(), Vector(v).normalized()
    c = [x / 255.0 for x in color.hex_to_rgb(base_hex)]
    cool = [x / 255.0 for x in color.COOL_SHADE]
    warm = [x / 255.0 for x in color.WARM_LIGHT]
    gt = [x / 255.0 for x in color.hex_to_rgb(GROUND_TINT_HEX)]
    rimc = [x / 255.0 for x in color.hex_to_rgb(RIM_HEX)]
    ndl = N.dot(lk)
    h = ndl * 0.5 + 0.5
    wb = _smoothstep(T_SHADE - BAND_WIDTH / 2, T_SHADE + BAND_WIDTH / 2, h)
    wl = _smoothstep(T_LIGHT - BAND_WIDTH / 2, T_LIGHT + BAND_WIDTH / 2, h)
    out = []
    fres = (1 - max(N.dot(V), 0.0)) ** RIM_POWER
    rim = RIM_ALPHA * _smoothstep(RIM_T0, RIM_T1, fres) * _smoothstep(0.0, RIM_MASK_EDGE, ndl)
    for i in range(3):
        shade = (c[i] + (cool[i] - c[i]) * color.COOL_SHADE_MIX) * (1 - s)
        light = c[i] + (warm[i] - c[i]) * l
        col = shade + (c[i] - shade) * wb
        col = col + (light - col) * wl
        col = col + (gt[i] - col) * (GROUNDING_ALPHA * grounding)
        col = col + (rimc[i] - col) * rim
        col = col + (c[i] - col) * e
        lin = color.srgb_to_linear1(min(max(col, 0.0), 1.0)) * (1 + e * EMISSIVE_BOOST)
        out.append(color.linear_to_srgb1(min(lin, 1.0)) * 255.0)
    return (out[0], out[1], out[2])

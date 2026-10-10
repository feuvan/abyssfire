"""HLSL of every Custom material node, generated from the manifest shading contract (Art/blender/README.md 2).

Rules for the code below (it is pasted into UMaterialExpressionCustom nodes, compiled for SM5 desktop and ES3.1 / Metal
mobile, ue58-platform.md 6.3):
* a node body is the body of a function; inputs are its parameters. No helper functions, no `static`, no includes.
* only SM5 intrinsics; scalars stay scalars (no swizzle on a float input), vector inputs are read component-wise
  (`X.r`, `X.g`, `X.b`) so the code compiles whether UE hands them over as float3 or float4;
* colour maths that must match the art previews runs on sRGB-encoded values exactly like the web `tone()` (the
  palette base colour is sampled as an sRGB texture and re-encoded here);
* literals are baked from the manifest at build time (the build regenerates every material on each run).

tests/test_hlsl.py compiles every node against tests/hlsl_emu.h (a C++ emulation of the HLSL subset) and checks the
toon / outline maths against the README reference formulas.
"""
from __future__ import annotations

import functools
import math
import re
from dataclasses import dataclass

from .manifest import Shading, hex_to_linear, hex_to_rgb255
from .pngio import GLOW_STOPS

# Terrain paint indices with a style (terrain_styles.json keys; WorldContract 3.1: walls -> WallPaintTile, camp walls -> 5).
TERRAIN_TILES = (0, 1, 2, 3, 5)
TERRAIN_LIQUID_TILE = 3
TERRAIN_FIELDS = ("Base", "PatchA", "PatchB", "Lip", "Accent")


@dataclass(frozen=True)
class NodeInput:
    name: str
    ctype: str          # emulation type: float, float2, float3, float4, Texture2D


@dataclass(frozen=True)
class CustomNode:
    key: str
    description: str
    output: str         # float | float2 | float3 | float4
    inputs: tuple[NodeInput, ...]
    code: str
    stage: str = "pixel"  # pixel | vertex

    @property
    def input_names(self) -> list[str]:
        return [i.name for i in self.inputs]


def lit(value: float) -> str:
    """A float literal valid in HLSL and C++ ('1.0', '0.0031308', '1e-06')."""
    if not math.isfinite(value):
        raise ValueError(f"non-finite literal {value}")
    text = format(float(value), ".9g")
    if "e" in text or "E" in text:
        mantissa, exponent = text.lower().split("e")
        if "." not in mantissa:
            mantissa += ".0"
        return f"{mantissa}e{exponent}"
    if "." not in text:
        text += ".0"
    return text


def vec3(v: tuple[float, float, float] | list[float]) -> str:
    return f"float3({lit(v[0])}, {lit(v[1])}, {lit(v[2])})"


def srgb255_vec(rgb: tuple[float, float, float]) -> str:
    return vec3((rgb[0] / 255.0, rgb[1] / 255.0, rgb[2] / 255.0))


def hex_srgb_vec(value: str) -> str:
    return srgb255_vec(hex_to_rgb255(value))


def _fill(template: str, values: dict[str, str]) -> str:
    """Replaces @name@ placeholders; fails on a leftover placeholder."""
    out = template
    for key, val in values.items():
        out = out.replace(f"@{key}@", val)
    left = re.findall(r"@[A-Za-z0-9_]+@", out)
    if left:
        raise ValueError(f"unfilled HLSL placeholders: {sorted(set(left))}")
    return out.strip() + "\n"


# Inline snippets (no helper functions in Custom nodes).
def _encode(dst: str, src: str) -> str:
    """linear -> sRGB of a saturated float3 (exact inverse of the texture decode)."""
    return (f"float3 {dst} = saturate({src});\n"
            f"{dst} = lerp({dst} * 12.92, 1.055 * pow(max({dst}, float3(1e-6, 1e-6, 1e-6)), 0.416666667) - 0.055, "
            f"step(0.0031308, {dst}));\n")


def _decode(dst: str, src: str) -> str:
    """sRGB -> linear of a float3 in [0, 1]."""
    return (f"float3 {dst} = saturate({src});\n"
            f"{dst} = lerp({dst} / 12.92, pow(max(({dst} + 0.055) / 1.055, float3(1e-6, 1e-6, 1e-6)), 2.4), "
            f"step(0.04045, {dst}));\n")


def _hash12(dst: str, x: str, y: str) -> str:
    """Hash without sine (Dave Hoskins' hash12): stable in fp32 for small integer lattice coordinates."""
    return (f"float3 {dst}_p = frac(float3({x}, {y}, {x}) * 0.1031);\n"
            f"{dst}_p = {dst}_p + dot({dst}_p, float3({dst}_p.y, {dst}_p.z, {dst}_p.x) + 33.33);\n"
            f"float {dst} = frac(({dst}_p.x + {dst}_p.y) * {dst}_p.z);\n")


# ---------------------------------------------------------------------------------------------------------------------
# Toon (M_AF_Toon family): README 2 formula + gameplay feedback (art-inventory-ch1.md 1.8, WorldContract 3.3)
# ---------------------------------------------------------------------------------------------------------------------
TOON_INPUTS = (
    NodeInput("Base", "float3"),            # T_AF_Palette_BC (sRGB texture, decoded by the sampler)
    NodeInput("P", "float4"),               # T_AF_Palette_P: R shadowAmt, G lightAmt, B emissive, A outline mix
    NodeInput("N", "float3"),               # VertexNormalWS
    NodeInput("V", "float3"),               # CameraVectorWS (towards the camera)
    NodeInput("L", "float4"),               # MPC KeyLightDir (towards the light)
    NodeInput("SunCol", "float4"),          # MPC SunColor (linear)
    NodeInput("RimCol", "float4"),          # MPC RimColor (sRGB-encoded values, #FFECC8)
    NodeInput("Ground", "float"),           # vertex colour AF_Data.r (baked grounding weight, linear)
    NodeInput("GroundEnable", "float"),
    NodeInput("SunTint", "float"),
    NodeInput("RimOverride", "float"),
    NodeInput("RimOverrideColor", "float3"),  # sRGB-encoded values
    NodeInput("RimOverrideAlpha", "float"),
    NodeInput("HitFlash", "float"),         # CPD 0
    NodeInput("PainTint", "float3"),        # CPD 1..3 (linear)
    NodeInput("PainAmt", "float"),          # CPD 4
    NodeInput("Telegraph", "float3"),       # CPD 5..7
    NodeInput("TelegraphAmt", "float"),     # CPD 8
    NodeInput("StatusTint", "float3"),      # CPD 9..11
    NodeInput("StatusAmt", "float"),        # CPD 12
    NodeInput("Ghost", "float"),            # CPD 14
    NodeInput("Highlight", "float"),        # CPD 15
    NodeInput("Inst", "float2"),            # PerInstanceCustomData (x Fade, y Highlight), interpolated
    NodeInput("HighlightColor", "float3"),  # linear
)

_TOON = """
@ENCODE_C@
float sAmt = saturate(P.r);
float lAmt = saturate(P.g);
float eAmt = saturate(P.b);
float3 n = normalize(N);
float3 lk = normalize(float3(L.r, L.g, L.b));
float ndl = dot(n, lk);
float hl = ndl * 0.5 + 0.5;
float3 shadeC = lerp(lerp(c, @COOL@, @COOL_MIX@), float3(0.0, 0.0, 0.0), sAmt);
float3 lightC = lerp(c, @WARM@, lAmt);
float3 col = lerp(shadeC, c, smoothstep(@TS0@, @TS1@, hl));
col = lerp(col, lightC, smoothstep(@TL0@, @TL1@, hl));
col = lerp(col, @GROUND@, @GROUND_ALPHA@ * saturate(Ground) * saturate(GroundEnable));
float3 v = normalize(V);
float facing = saturate(dot(n, v));
float fres = pow(max(1.0 - facing, 1e-5), @RIM_POW@);
float rimOn = saturate(RimOverride);
float rimA = lerp(@RIM_ALPHA@, RimOverrideAlpha, rimOn);
float rimW = rimA * smoothstep(@RB0@, @RB1@, fres) * smoothstep(0.0, @RIM_EDGE@, ndl);
float3 rimC = lerp(float3(RimCol.r, RimCol.g, RimCol.b), float3(RimOverrideColor.r, RimOverrideColor.g, RimOverrideColor.b), rimOn);
col = lerp(col, rimC, rimW);
col = lerp(col, c, eAmt);
@DECODE_LIN@
float3 one = float3(1.0, 1.0, 1.0);
float3 sunC = float3(SunCol.r, SunCol.g, SunCol.b);
lin = lin * lerp(one, sunC, saturate(SunTint) * (1.0 - eAmt)) * (1.0 + @EM_BOOST@ * eAmt);
lin = lin * lerp(one, float3(PainTint.r, PainTint.g, PainTint.b), saturate(PainAmt));
lin = lin * lerp(one, float3(Telegraph.r, Telegraph.g, Telegraph.b), saturate(TelegraphAmt));
float3 tintC = float3(StatusTint.r, StatusTint.g, StatusTint.b);
lin = lin * lerp(one, tintC, saturate(StatusAmt));
lin = lerp(lin, tintC * (0.35 + 0.9 * fres), saturate(Ghost));
float hi = saturate(max(Highlight, Inst.y));
float rim2 = (1.0 - facing) * (1.0 - facing);
lin = lin + float3(HighlightColor.r, HighlightColor.g, HighlightColor.b) * (hi * (0.18 + 0.82 * rim2));
lin = lerp(lin, one, saturate(HitFlash));
return lin;
"""


def toon_node(sh: Shading) -> CustomNode:
    half = sh.band_width / 2.0
    code = _fill(_TOON, {
        "ENCODE_C": _encode("c", "Base"),
        "DECODE_LIN": _decode("lin", "col"),
        "COOL": srgb255_vec(sh.cool_shade),
        "COOL_MIX": lit(sh.cool_shade_mix),
        "WARM": srgb255_vec(sh.warm_light),
        "TS0": lit(sh.t_shade - half), "TS1": lit(sh.t_shade + half),
        "TL0": lit(sh.t_light - half), "TL1": lit(sh.t_light + half),
        "GROUND": hex_srgb_vec(sh.ground_color),
        "GROUND_ALPHA": lit(sh.ground_alpha),
        "RIM_POW": lit(sh.rim_power),
        "RIM_ALPHA": lit(sh.rim_alpha),
        "RB0": lit(sh.rim_band[0]), "RB1": lit(sh.rim_band[1]),
        "RIM_EDGE": lit(sh.rim_mask_edge),
        "EM_BOOST": lit(sh.emissive_boost),
    })
    return CustomNode("AF_Toon", "Abyssfire toon: 3-band half-Lambert in sRGB space + rim + grounding + feedback",
                      "float3", TOON_INPUTS, code)


# Dithered opacity (occlusion / death fade, slime body): ordered 4x4 Bayer, stable under TSR and MSAA.
DITHER_INPUTS = (NodeInput("Fade", "float"), NodeInput("Inst", "float2"), NodeInput("BodyOpacity", "float"))
_DITHER = """
float a = saturate(BodyOpacity) * (1.0 - saturate(max(Fade, Inst.x)));
float2 q = floor(fmod(Parameters.SvPosition.xy, float2(4.0, 4.0)));
uint qx = (uint)q.x;
uint qy = (uint)q.y;
uint m = qx ^ qy;
uint b = ((m & 1u) << 3) | ((qy & 1u) << 2) | (m & 2u) | ((qy & 2u) >> 1);
float t = ((float)b + 0.5) / 16.0;
return a > t ? 1.0 : 0.0;
"""


def dither_node() -> CustomNode:
    return CustomNode("AF_Dither", "Abyssfire dithered opacity mask (Bayer 4x4)", "float", DITHER_INPUTS,
                      _fill(_DITHER, {}))


# ---------------------------------------------------------------------------------------------------------------------
# Outline hull (M_AF_Outline): colour mix(ink, line(c), o) + screen-constant WPO (README 2 "Width = screen-constant WPO")
# ---------------------------------------------------------------------------------------------------------------------
INK_INPUTS = (
    NodeInput("Base", "float3"), NodeInput("P", "float4"), NodeInput("Highlight", "float"), NodeInput("Inst", "float2"),
    NodeInput("HitFlash", "float"), NodeInput("HighlightColor", "float3"),
)
_INK = """
@ENCODE_C@
float3 lineC = lerp(lerp(c, @LINE_TINT@, @LINE_TINT_MIX@), float3(0.0, 0.0, 0.0), @LINE_DARKEN@);
float3 col = lerp(@INK@, lineC, saturate(P.a));
@DECODE_LIN@
float hi = saturate(max(Highlight, Inst.y));
lin = lerp(lin, float3(HighlightColor.r, HighlightColor.g, HighlightColor.b), hi * 0.85);
lin = lerp(lin, float3(1.0, 1.0, 1.0), saturate(HitFlash));
return lin;
"""


def ink_node(sh: Shading) -> CustomNode:
    code = _fill(_INK, {
        "ENCODE_C": _encode("c", "Base"),
        "DECODE_LIN": _decode("lin", "col"),
        "LINE_TINT": srgb255_vec(sh.line_tint),
        "LINE_TINT_MIX": lit(sh.line_tint_mix),
        "LINE_DARKEN": lit(sh.line_darken),
        "INK": hex_srgb_vec(sh.ink),
    })
    return CustomNode("AF_InkColor", "Abyssfire outline colour: mix(ink, line(c), o)", "float3", INK_INPUTS, code)


OUTLINE_WPO_INPUTS = (
    NodeInput("Pos", "float3"),     # WorldPosition, camera relative, excluding material offsets
    NodeInput("N", "float3"),       # VertexNormalWS (the imported custom normal of the hull)
    NodeInput("F", "float3"),       # camera forward (view +Z in world space)
    NodeInput("T", "float2"),       # ViewProperty TanHalfFieldOfView
    NodeInput("Px", "float"),       # OutlinePx1080 (per class)
    NodeInput("Baked", "float"),    # BakedWidthCm (the width the hull was baked with)
    NodeInput("Enable", "float"),   # OutlineScreenSpace (1; 0 = baked hull only)
)
_OUTLINE_WPO = """
float3 n = normalize(N);
float dist = length(Pos);
float3 v = dist > 0.0001 ? Pos / dist : float3(0.0, 0.0, 1.0);
float depth = max(dot(Pos, F), 1.0);
float3 np = n - dot(n, v) * v;
float3 dir = np / max(length(np), @EPS@);
float width = Px * 2.0 * depth * T.y / 1080.0;
return (dir * width - n * Baked) * saturate(Enable);
"""


def outline_wpo_node(sh: Shading) -> CustomNode:
    return CustomNode("AF_OutlineWPO", "Abyssfire outline: hull silhouette dilated by OutlinePx1080 * ViewSizeY / 1080 px",
                      "float3", OUTLINE_WPO_INPUTS, _fill(_OUTLINE_WPO, {"EPS": lit(sh.outline_eps)}), stage="vertex")


# Foliage wind (M_AF_Toon_Foliage / M_AF_Outline_Foliage): sway grows with height, phase from the ISM Random float.
WIND_INPUTS = (
    NodeInput("LP", "float3"), NodeInput("Rand", "float"), NodeInput("Time", "float"), NodeInput("WindDir", "float3"),
    NodeInput("Amp", "float"), NodeInput("Height", "float"), NodeInput("Speed", "float"),
)
_WIND = """
float h = saturate(LP.z / max(Height, 1.0));
float w = h * h;
float ph = Rand * 6.2831853 + Time * Speed;
float s = sin(ph) + 0.35 * sin(ph * 2.3 + 1.7);
float2 d2 = float2(WindDir.r, WindDir.g);
float dl = max(length(d2), 0.0001);
float3 d = float3(d2.x / dl, d2.y / dl, 0.0);
return d * (s * Amp * w) - float3(0.0, 0.0, abs(s) * Amp * w * 0.15);
"""


def wind_node() -> CustomNode:
    return CustomNode("AF_Wind", "Abyssfire foliage sway", "float3", WIND_INPUTS, _fill(_WIND, {}), stage="vertex")


# ---------------------------------------------------------------------------------------------------------------------
# VFX particles (M_AF_FX_*): WorldContract 3.7 / 5.4 (instance data 0..3 RGBA, 4 Variant, 5 Age; sprite strips)
# ---------------------------------------------------------------------------------------------------------------------
FX_UV_INPUTS = (NodeInput("UV", "float2"), NodeInput("Var", "float2"), NodeInput("Cells", "float"))
_FX_UV = """
float cells = max(floor(Cells + 0.5), 1.0);
float idx = clamp(floor(Var.x + 0.5), 0.0, cells - 1.0);
return float2((saturate(UV.x) + idx) / cells, UV.y);
"""

FX_ADD_INPUTS = (NodeInput("Tex", "float4"), NodeInput("Col", "float4"), NodeInput("Soft", "float"))
_FX_ADD = """
float k = Tex.a * saturate(Col.a) * saturate(Soft);
return float3(Tex.r * Col.r, Tex.g * Col.g, Tex.b * Col.b) * k;
"""

FX_TRANS_COLOR_INPUTS = (NodeInput("Tex", "float4"), NodeInput("Col", "float4"))
_FX_TRANS_COLOR = """
return float3(Tex.r * Col.r, Tex.g * Col.g, Tex.b * Col.b);
"""

FX_TRANS_ALPHA_INPUTS = (NodeInput("Tex", "float4"), NodeInput("Col", "float4"), NodeInput("Soft", "float"))
_FX_TRANS_ALPHA = """
return saturate(Tex.a * Col.a * saturate(Soft));
"""


def fx_nodes() -> dict[str, CustomNode]:
    return {
        "uv": CustomNode("AF_FxUV", "sprite strip cell", "float2", FX_UV_INPUTS, _fill(_FX_UV, {})),
        "add": CustomNode("AF_FxAdd", "additive sprite colour", "float3", FX_ADD_INPUTS, _fill(_FX_ADD, {})),
        "trans_color": CustomNode("AF_FxTransColor", "translucent sprite colour", "float3", FX_TRANS_COLOR_INPUTS,
                                  _fill(_FX_TRANS_COLOR, {})),
        "trans_alpha": CustomNode("AF_FxTransAlpha", "translucent sprite opacity", "float", FX_TRANS_ALPHA_INPUTS,
                                  _fill(_FX_TRANS_ALPHA, {})),
    }


# FX meshes (rocks, coins, hex plates): toon-banded instance colour, the baked hull (AF_Data.a = 0) drawn in ink.
FX_MESH_INPUTS = (NodeInput("Col", "float4"), NodeInput("N", "float3"), NodeInput("L", "float4"),
                  NodeInput("Body", "float"))
_FX_MESH = """
float3 lc = saturate(float3(Col.r, Col.g, Col.b));
@ENCODE_C@
float3 n = normalize(N);
float hl = dot(n, normalize(float3(L.r, L.g, L.b))) * 0.5 + 0.5;
float3 shadeC = lerp(lerp(c, @COOL@, @COOL_MIX@), float3(0.0, 0.0, 0.0), 0.42);
float3 lightC = lerp(c, @WARM@, 0.32);
float3 col = lerp(shadeC, c, smoothstep(@TS0@, @TS1@, hl));
col = lerp(col, lightC, smoothstep(@TL0@, @TL1@, hl));
col = lerp(@INK@, col, saturate(Body));
@DECODE_LIN@
float hdr = max(1.0, max(Col.r, max(Col.g, Col.b)));
return lin * hdr;
"""


def fx_mesh_node(sh: Shading) -> CustomNode:
    half = sh.band_width / 2.0
    code = _fill(_FX_MESH, {
        "ENCODE_C": _encode("c", "lc"),
        "DECODE_LIN": _decode("lin", "col"),
        "COOL": srgb255_vec(sh.cool_shade), "COOL_MIX": lit(sh.cool_shade_mix), "WARM": srgb255_vec(sh.warm_light),
        "TS0": lit(sh.t_shade - half), "TS1": lit(sh.t_shade + half),
        "TL0": lit(sh.t_light - half), "TL1": lit(sh.t_light + half),
        "INK": hex_srgb_vec(sh.ink),
    })
    return CustomNode("AF_FxMesh", "toon-banded FX mesh colour", "float3", FX_MESH_INPUTS, code)


FX_MESH_MASK_INPUTS = (NodeInput("Col", "float4"),)
_FX_MESH_MASK = """
float a = saturate(Col.a);
float2 q = floor(fmod(Parameters.SvPosition.xy, float2(4.0, 4.0)));
uint qx = (uint)q.x;
uint qy = (uint)q.y;
uint m = qx ^ qy;
uint b = ((m & 1u) << 3) | ((qy & 1u) << 2) | (m & 2u) | ((qy & 2u) >> 1);
return a > ((float)b + 0.5) / 16.0 ? 1.0 : 0.0;
"""


def fx_mesh_mask_node() -> CustomNode:
    return CustomNode("AF_FxMeshMask", "dithered FX mesh fade", "float", FX_MESH_MASK_INPUTS, _fill(_FX_MESH_MASK, {}))


# ---------------------------------------------------------------------------------------------------------------------
# Utility materials (WorldContract 3.7)
# ---------------------------------------------------------------------------------------------------------------------
GHOST_INPUTS = (NodeInput("Tint", "float3"), NodeInput("Ghost", "float"), NodeInput("Fade", "float"),
                NodeInput("N", "float3"), NodeInput("V", "float3"), NodeInput("Body", "float"))
_GHOST = """
float facing = saturate(dot(normalize(N), normalize(V)));
float edge = pow(max(1.0 - facing, 1e-5), 1.5);
float k = saturate(Ghost) * (1.0 - saturate(Fade)) * (0.85 + 0.3 * edge) * lerp(0.55, 1.0, saturate(Body));
return float3(Tint.r, Tint.g, Tint.b) * k;
"""


def ghost_node() -> CustomNode:
    return CustomNode("AF_Ghost", "additive spirit silhouette (afterimages, soul echo)", "float3", GHOST_INPUTS,
                      _fill(_GHOST, {}))


def _glow_expr(r: str) -> str:
    """The web glow falloff as nested selects over GLOW_STOPS."""
    stops = list(GLOW_STOPS)
    expr = "0.0"
    for (r0, a0), (r1, a1) in reversed(list(zip(stops, stops[1:]))):
        seg = f"lerp({lit(a0)}, {lit(a1)}, ({r} - {lit(r0)}) / {lit(r1 - r0)})"
        expr = f"({r} < {lit(r1)} ? {seg} : {expr})"
    return expr


QUAD_INPUTS = (NodeInput("UV", "float2"), NodeInput("Tint", "float3"), NodeInput("Alpha", "float"))
_LIGHT_POOL = """
float r = length(float2(UV.x - 0.5, UV.y - 0.5)) * 2.0;
float f = @GLOW@;
return float3(Tint.r, Tint.g, Tint.b) * (f * saturate(Alpha));
"""
_TARGET_RING = """
float r = length(float2(UV.x - 0.5, UV.y - 0.5)) * 2.0;
float ring = smoothstep(0.74, 0.82, r) * (1.0 - smoothstep(0.92, 1.0, r));
float fill = (1.0 - smoothstep(0.7, 0.82, r)) * 0.12;
return float3(Tint.r, Tint.g, Tint.b) * ((ring + fill) * saturate(Alpha));
"""
_AFFIX_AURA = """
float r = length(float2(UV.x - 0.5, UV.y - 0.5)) * 2.0;
float disc = pow(saturate(1.0 - r), 1.6) * 0.8;
float ring = smoothstep(0.8, 0.88, r) * (1.0 - smoothstep(0.94, 1.0, r)) * 0.35;
return float3(Tint.r, Tint.g, Tint.b) * ((disc + ring) * saturate(Alpha));
"""


def quad_nodes() -> dict[str, CustomNode]:
    return {
        "light_pool": CustomNode("AF_LightPool", "fake light pool (web light falloff)", "float3", QUAD_INPUTS,
                                 _fill(_LIGHT_POOL, {"GLOW": _glow_expr("r")})),
        "target_ring": CustomNode("AF_TargetRing", "target ring", "float3", QUAD_INPUTS, _fill(_TARGET_RING, {})),
        "affix_aura": CustomNode("AF_AffixAura", "elite affix aura", "float3", QUAD_INPUTS, _fill(_AFFIX_AURA, {})),
    }


# Blob shadow (art-inventory-ch1.md 1.4): rgba(8,6,16, .45) centre -> .26 at 65 % -> 0 at the edge.
BLOB_INPUTS = (NodeInput("UV", "float2"), NodeInput("Fade", "float"))
_BLOB = """
float r = length(float2(UV.x - 0.5, UV.y - 0.5)) * 2.0;
float a = r < 0.65 ? lerp(0.45, 0.26, r / 0.65) : (r < 1.0 ? lerp(0.26, 0.0, (r - 0.65) / 0.35) : 0.0);
return a * (1.0 - saturate(Fade));
"""
BLOB_COLOR_LINEAR = hex_to_linear("#080610")


def blob_node() -> CustomNode:
    return CustomNode("AF_BlobAlpha", "blob shadow opacity", "float", BLOB_INPUTS, _fill(_BLOB, {}))


# ---------------------------------------------------------------------------------------------------------------------
# Post-process mood grade (M_AF_PP_Grade, world-map-nav.md 14.1 + ColorGradePipeline.ts)
# ---------------------------------------------------------------------------------------------------------------------
GRADE_INPUTS = (
    NodeInput("Scene", "float4"), NodeInput("UV", "float2"), NodeInput("Time", "float"),
    NodeInput("Ambient", "float3"), NodeInput("AmbientAlpha", "float"), NodeInput("Haze", "float3"),
    NodeInput("HazeAlpha", "float"), NodeInput("Saturation", "float"), NodeInput("Contrast", "float"),
    NodeInput("Lift", "float3"), NodeInput("Gain", "float3"),
)
_GRADE = """
float3 c = saturate(float3(Scene.r, Scene.g, Scene.b));
float t = Time * 1000.0;
float aa = saturate(AmbientAlpha + sin(t * 0.0015) * 0.015 * step(0.0001, AmbientAlpha));
@ENCODE_AMB@
c = c * lerp(float3(1.0, 1.0, 1.0), amb, aa);
float2 drift = float2(sin(t * 0.0008) * 0.15, cos(t * 0.00056) * 0.1);
float2 hd = float2(UV.x - 0.5 - drift.x, UV.y - 0.5 - drift.y) / 0.625;
float hz = saturate(1.0 - length(hd));
@ENCODE_HAZE@
c = c + haze * (saturate(HazeAlpha) * (0.8 + sin(t * 0.0007) * 0.2) * hz);
float lum = dot(c, float3(0.299, 0.587, 0.114));
c = lerp(float3(lum, lum, lum), c, Saturation);
c = (c - 0.5) * Contrast + 0.5;
float sh = 1.0 - smoothstep(0.0, 0.55, lum);
float hi = smoothstep(0.45, 1.0, lum);
c = c + float3(Lift.r, Lift.g, Lift.b) * sh + float3(Gain.r, Gain.g, Gain.b) * hi;
return saturate(c);
"""


def grade_node() -> CustomNode:
    code = _fill(_GRADE, {
        "ENCODE_AMB": _encode("amb", "float3(Ambient.r, Ambient.g, Ambient.b)"),
        "ENCODE_HAZE": _encode("haze", "float3(Haze.r, Haze.g, Haze.b)"),
    })
    return CustomNode("AF_Grade", "web mood grade (ambient multiply, haze, saturation / contrast, lift / gain)",
                      "float3", GRADE_INPUTS, code)


# ---------------------------------------------------------------------------------------------------------------------
# Water surface (M_AF_Water; WorldContract 3.2 + AbyssTerrain.cpp: vertex colour R = shore factor, G = depth factor)
# ---------------------------------------------------------------------------------------------------------------------
WATER_INPUTS = (
    NodeInput("Shore", "float"), NodeInput("Depth", "float"), NodeInput("UV", "float2"), NodeInput("Time", "float"),
    NodeInput("WaterBase", "float3"), NodeInput("WaterShallow", "float3"), NodeInput("WaterFoam", "float3"),
    NodeInput("WaterWave", "float3"), NodeInput("SunCol", "float4"), NodeInput("Amb", "float4"),
    NodeInput("L", "float4"), NodeInput("SunScale", "float"),
)
_WATER_FOAM = """
float shore = saturate(Shore);
float wob = 0.08 * sin(Time * 1.7 + (UV.x + UV.y) * 3.1) + 0.04 * sin(Time * 2.9 - UV.x * 5.3);
float foam = smoothstep(0.55, 0.62, shore + wob);
"""
_WATER = """
@ENCODE_BASE@
@ENCODE_SHALLOW@
@ENCODE_FOAM@
@ENCODE_WAVE@
float d = smoothstep(0.0, 1.0, saturate(Depth));
float3 col = lerp(shallowC, baseC, d);
float2 q = float2(UV.x + Time * 0.05, UV.y + Time * 0.03) * 1.5;
float2 cell = floor(q);
float2 fq = float2(q.x - cell.x - 0.5, q.y - cell.y - 0.5);
@HASH_A@
@HASH_B@
float2 off = float2(ha - 0.5, hb - 0.5) * 0.5;
float ex = (fq.x - off.x) / 0.16;
float ey = (fq.y - off.y) / 0.035;
float fleck = (1.0 - smoothstep(0.7, 1.0, ex * ex + ey * ey)) * step(ha, 0.55);
col = lerp(col, waveC, 0.7 * fleck * (0.6 + 0.4 * sin(Time * 1.3 + hb * 6.2831853)));
@FOAM@
col = lerp(col, foamC, foam * 0.92);
@DECODE_LIN@
float3 light = SunScale * saturate(L.z) * float3(SunCol.r, SunCol.g, SunCol.b) + float3(Amb.r, Amb.g, Amb.b);
return lin * light;
"""
WATER_OPACITY_INPUTS = (NodeInput("Shore", "float"), NodeInput("Depth", "float"), NodeInput("UV", "float2"),
                        NodeInput("Time", "float"))
_WATER_OPACITY = """
@FOAM@
float d = smoothstep(0.0, 1.0, saturate(Depth));
return saturate(lerp(0.62, 0.9, d) + 0.5 * foam);
"""


def water_nodes() -> dict[str, CustomNode]:
    color = _fill(_WATER, {
        "ENCODE_BASE": _encode("baseC", "float3(WaterBase.r, WaterBase.g, WaterBase.b)"),
        "ENCODE_SHALLOW": _encode("shallowC", "float3(WaterShallow.r, WaterShallow.g, WaterShallow.b)"),
        "ENCODE_FOAM": _encode("foamC", "float3(WaterFoam.r, WaterFoam.g, WaterFoam.b)"),
        "ENCODE_WAVE": _encode("waveC", "float3(WaterWave.r, WaterWave.g, WaterWave.b)"),
        "HASH_A": _hash12("ha", "cell.x", "cell.y"),
        "HASH_B": _hash12("hb", "cell.x + 17.0", "cell.y - 9.0"),
        "FOAM": _WATER_FOAM.strip(),
        "DECODE_LIN": _decode("lin", "col"),
    })
    opacity = _fill(_WATER_OPACITY, {"FOAM": _WATER_FOAM.strip()})
    return {
        "color": CustomNode("AF_Water", "toon water surface colour", "float3", WATER_INPUTS, color),
        "opacity": CustomNode("AF_WaterOpacity", "water surface opacity", "float", WATER_OPACITY_INPUTS, opacity),
    }


# ---------------------------------------------------------------------------------------------------------------------
# Terrain (M_AF_Terrain): exact port of the web tile transitions (ZoneTerrain.ts quadMask) over the TileIds texture,
# with a procedural approximation of the painted 3x3-tile ground patterns (patches, specks, accent details).
# ---------------------------------------------------------------------------------------------------------------------
def mulberry32(seed: int):
    """The web lattice PRNG (TerrainLattice.ts rng)."""
    state = [seed & 0xFFFFFFFF]

    def imul(a: int, b: int) -> int:
        return (a * b) & 0xFFFFFFFF

    def nxt() -> float:
        state[0] = (state[0] + 0x6D2B79F5) & 0xFFFFFFFF
        t = state[0]
        t = imul(t ^ (t >> 15), t | 1)
        t ^= (t + imul(t ^ (t >> 7), t | 61)) & 0xFFFFFFFF
        t &= 0xFFFFFFFF
        return ((t ^ (t >> 14)) & 0xFFFFFFFF) / 4294967296.0

    return nxt


NOISE_PERIOD = 3
NOISE_RES = 40
NOISE_FREQS_P3 = ((1, 2), (2, -1), (3, 1), (-1, 3), (4, 2), (2, 5), (5, -3), (-4, 5), (7, 2), (3, 7), (8, -5), (-6, 8))


@functools.lru_cache(maxsize=1)
def lattice_noise_waves() -> tuple[list[tuple[int, int, float, float]], float, float]:
    """Waves (k, l, amp, phase) of the web's period-3 lattice noise and the min / max of its sampled table (buildNoise),
    so the shader evaluates the same normalised field analytically."""
    r = mulberry32(9173 + NOISE_PERIOD)
    waves = []
    for k, l in NOISE_FREQS_P3:
        mag = math.hypot(k, l)
        waves.append((k, l, 1.0 / mag ** 0.9, r() * math.pi * 2.0))
    n = NOISE_RES * NOISE_PERIOD
    lo, hi = math.inf, -math.inf
    for j in range(n):
        v = j / NOISE_RES
        for i in range(n):
            u = i / NOISE_RES
            s = 0.0
            for k, l, amp, ph in waves:
                s += amp * math.sin((2.0 * math.pi * (k * u + l * v)) / NOISE_PERIOD + ph)
            lo = min(lo, s)
            hi = max(hi, s)
    return waves, lo, hi


def lattice_noise_reference(u: float, v: float) -> float:
    waves, lo, hi = lattice_noise_waves()
    s = sum(amp * math.sin((2.0 * math.pi * (k * u + l * v)) / NOISE_PERIOD + ph) for k, l, amp, ph in waves)
    return (s - lo) / (hi - lo) * 2.0 - 1.0


def terrain_inputs() -> tuple[NodeInput, ...]:
    ins = [NodeInput("UV", "float2"), NodeInput("Wall", "float"), NodeInput("Skirt", "float"),
           NodeInput("TileIds", "Texture2D"), NodeInput("MapCols", "float"), NodeInput("MapRows", "float")]
    for t in TERRAIN_TILES:
        for f in TERRAIN_FIELDS:
            ins.append(NodeInput(f"Tile{t}{f}", "float3"))
        ins.append(NodeInput(f"Tile{t}Rank", "float"))
    ins += [NodeInput("WaterShallow", "float3"), NodeInput("WaterFoam", "float3"), NodeInput("WaterBank", "float3")]
    return tuple(ins)


def _select(m: str, field_name: str) -> str:
    """Nested select over the styled paint indices (unknown -> the last one)."""
    tiles = list(TERRAIN_TILES)
    expr = f"Tile{tiles[-1]}{field_name}"
    for t in reversed(tiles[:-1]):
        expr = f"({m} == {t} ? Tile{t}{field_name} : {expr})"
    return expr


def _f3(expr: str) -> str:
    return f"float3(({expr}).r, ({expr}).g, ({expr}).b)"


def _noise_block(dst: str, m: str, waves, lo: float, hi: float) -> str:
    lines = [f"float {dst}_u = g.x + 0.5 + (float){m} * 0.37;",
             f"float {dst}_v = g.y + 0.5 - (float){m} * 0.61;",
             f"float {dst}_s = 0.0;"]
    for k, l, amp, ph in waves:
        w = 2.0 * math.pi / NOISE_PERIOD
        lines.append(f"{dst}_s = {dst}_s + {lit(amp)} * sin({lit(w * k)} * {dst}_u + {lit(w * l)} * {dst}_v + {lit(ph)});")
    lines.append(f"float {dst} = ({dst}_s - {lit(lo)}) * {lit(2.0 / (hi - lo))} - 1.0;")
    return "\n".join(lines) + "\n"


def _pattern_block(dst: str, m: str, own: str, detailed: bool) -> str:
    """Ground colour (sRGB) of paint `m` at g: base, two cel patch layers, specks, own-tile accent details."""
    p = dst
    out = [
        _encode(f"{p}_base", _f3(_select(m, "Base"))),
        _encode(f"{p}_pa", _f3(_select(m, "PatchA"))),
        _encode(f"{p}_pb", _f3(_select(m, "PatchB"))),
        _encode(f"{p}_lip", _f3(_select(m, "Lip"))),
        _encode(f"{p}_acc", _f3(_select(m, "Accent"))),
        f"float {p}_mf = (float){m};\n",
        f"float {p}_qu = g.x + 0.5 + {p}_mf * 0.73;\n",
        f"float {p}_qv = g.y + 0.5 + {p}_mf * 0.29;\n",
        # periodic (3-tile) patch field with integer frequencies
        f"float {p}_pf = (sin(2.0943951 * (2.0 * {p}_qu + {p}_qv) + 1.3 + {p}_mf * 1.7)"
        f" + 0.8 * sin(2.0943951 * (3.0 * {p}_qv - {p}_qu) + 4.1 + {p}_mf * 0.9)"
        f" + 0.5 * sin(2.0943951 * (4.0 * {p}_qu - 2.0 * {p}_qv) + 2.2 - {p}_mf * 1.3)) * 0.4347826;\n",
        f"float3 {dst} = {p}_base;\n",
        f"{dst} = lerp({dst}, {p}_pa, smoothstep(0.40, 0.44, {p}_pf));\n",
        f"{dst} = lerp({dst}, {p}_pb, smoothstep(-0.48, -0.52, {p}_pf) * 0.85);\n",
    ]
    if detailed:
        out += [
            # specks (tufts / pebbles / leaves): one candidate per quarter-tile cell
            f"float2 {p}_sq = float2({p}_qu * 4.0, {p}_qv * 4.0);\n",
            f"float2 {p}_sc = floor({p}_sq);\n",
            _hash12(f"{p}_h1", f"{p}_sc.x + {p}_mf * 17.0", f"{p}_sc.y - {p}_mf * 5.0"),
            _hash12(f"{p}_h2", f"{p}_sc.x + 41.0", f"{p}_sc.y + {p}_mf * 3.0 + 7.0"),
            f"float2 {p}_so = float2({p}_sq.x - {p}_sc.x - 0.5 - ({p}_h1 - 0.5) * 0.5,"
            f" {p}_sq.y - {p}_sc.y - 0.5 - ({p}_h2 - 0.5) * 0.5);\n",
            f"float {p}_sr = 0.11 + 0.08 * {p}_h2;\n",
            f"float {p}_sk = (1.0 - smoothstep({p}_sr * 0.8, {p}_sr, length({p}_so))) * step({p}_h1, 0.34)"
            f" * ({m} == {TERRAIN_LIQUID_TILE} ? 0.0 : 1.0);\n",
            f"float3 {p}_skc = {p}_h2 > 0.5 ? {p}_lip : {p}_pa * 0.82;\n",
            f"{dst} = lerp({dst}, {p}_skc, {p}_sk);\n",
            # accent details on the pixel's own tile (web: tileHash(c, r, 7) % 12 >= 6)
            f"float {p}_dt = ({own}_var >= 6 && {m} == {own}_m && {m} != {TERRAIN_LIQUID_TILE}) ? 1.0 : 0.0;\n",
            f"float {p}_acm = 0.0;\n",
        ]
        for i, (ax, ay) in enumerate(((0.18, -0.12), (-0.2, 0.06), (0.04, 0.22), (-0.06, -0.24))):
            out += [
                _hash12(f"{p}_d{i}", f"{own}_c.x + {lit(3.1 * (i + 1))}", f"{own}_c.y - {lit(1.7 * (i + 1))}"),
                f"float2 {p}_dp{i} = float2(g.x - {own}_c.x - {lit(ax)} - ({p}_d{i} - 0.5) * 0.12,"
                f" g.y - {own}_c.y - {lit(ay)} - ({p}_d{i} - 0.5) * 0.12);\n",
                f"{p}_acm = max({p}_acm, 1.0 - smoothstep(0.045, 0.06, length({p}_dp{i})));\n",
            ]
        out += [f"{dst} = lerp({dst}, {p}_acc, {p}_acm * {p}_dt);\n"]
    return "".join(out)


def terrain_node(detailed: bool = True) -> CustomNode:
    """detailed=False is the low-quality branch (QualitySwitch Low): no boundary noise, no specks / details."""
    waves, lo, hi = lattice_noise_waves()
    c: list[str] = [
        "int cols = max((int)(MapCols + 0.5), 1);\n",
        "int rows = max((int)(MapRows + 0.5), 1);\n",
        "float2 g = UV;\n",
        "float2 b0 = floor(g);\n",
        "float2 fr = float2(g.x - b0.x, g.y - b0.y);\n",
        "int bx = (int)b0.x;\n",
        "int by = (int)b0.y;\n",
    ]
    for k in range(4):
        ox, oy = k & 1, k >> 1
        du = "1.0 - fr.x" if ox else "fr.x"
        dv = "1.0 - fr.y" if oy else "fr.y"
        c += [
            f"int cx{k} = clamp(bx + {ox}, 0, cols - 1);\n",
            f"int cy{k} = clamp(by + {oy}, 0, rows - 1);\n",
            f"float4 id{k} = TileIds.Load(int3(cx{k}, cy{k}, 0));\n",
            f"int m{k} = (int)(id{k}.r * 255.0 + 0.5);\n",
            f"float du{k} = {du};\n",
            f"float dv{k} = {dv};\n",
            f"float d2{k} = du{k} * du{k} + dv{k} * dv{k};\n",
            f"float w{k} = d2{k} < 1.0 ? (1.0 - d2{k}) * (1.0 - d2{k}) : 0.0;\n",
        ]
    # own tile (nearest centre) and its detail variant
    c += [
        "int ko = (fr.x >= 0.5 ? 1 : 0) + (fr.y >= 0.5 ? 2 : 0);\n",
        "float4 ido = ko == 0 ? id0 : (ko == 1 ? id1 : (ko == 2 ? id2 : id3));\n",
        "int own_m = (int)(ido.r * 255.0 + 0.5);\n",
        "int own_var = (int)(ido.g * 255.0 + 0.5);\n",
        "float2 own_c = float2(b0.x + (float)(ko & 1), b0.y + (float)(ko >> 1));\n",
        "float sw = max(w0 + w1 + w2 + w3, 1e-5);\n",
    ]
    for k in range(4):
        others = " + ".join(f"(m{j} == m{k} ? w{j} : 0.0)" for j in range(4) if j != k)
        c.append(f"float f{k} = (w{k} + {others}) / sw;\n")
    c.append("bool same = (m0 == m1) && (m0 == m2) && (m0 == m3);\n")
    for k in range(4):
        c.append(f"float rk{k} = {_select(f'm{k}', 'Rank')};\n")
        c.append(f"float sc{k} = -1000.0;\n")
        c.append(f"if (w{k} > 0.0) {{\n")
        if detailed:
            c.append("float nz = 0.0;\n")
            c.append("if (!same) {\n")
            c.append(_noise_block(f"n{k}", f"m{k}", waves, lo, hi))
            c.append(f"nz = n{k};\n")
            c.append("}\n")
            c.append(f"sc{k} = f{k} + rk{k} * 0.025 + 0.3 * nz;\n")
        else:
            c.append(f"sc{k} = f{k} + rk{k} * 0.025;\n")
        c.append("}\n")
    c += [
        "int mb = m0; float s1 = sc0; float fb = f0;\n",
        "if (sc1 > s1) { mb = m1; s1 = sc1; fb = f1; }\n",
        "if (sc2 > s1) { mb = m2; s1 = sc2; fb = f2; }\n",
        "if (sc3 > s1) { mb = m3; s1 = sc3; fb = f3; }\n",
        "int ms = mb; float s2 = -1000.0;\n",
        "if (w0 > 0.0 && m0 != mb && sc0 > s2) { ms = m0; s2 = sc0; }\n",
        "if (w1 > 0.0 && m1 != mb && sc1 > s2) { ms = m1; s2 = sc1; }\n",
        "if (w2 > 0.0 && m2 != mb && sc2 > s2) { ms = m2; s2 = sc2; }\n",
        "if (w3 > 0.0 && m3 != mb && sc3 > s2) { ms = m3; s2 = sc3; }\n",
        "bool single = s2 < -999.0;\n",
        "float dlt = single ? 1.0 : s1 - s2;\n",
        "float al = dlt >= 0.025 ? 1.0 : 0.5 + dlt / 0.05;\n",
    ]
    c.append(_pattern_block("c1", "mb", "own", detailed))
    c.append(_pattern_block("c2", "ms", "own", detailed))
    c += [
        "float3 col = lerp(c2, c1, al);\n",
        _encode("shallowS", "float3(WaterShallow.r, WaterShallow.g, WaterShallow.b)"),
        _encode("foamS", "float3(WaterFoam.r, WaterFoam.g, WaterFoam.b)"),
        _encode("bankS", "float3(WaterBank.r, WaterBank.g, WaterBank.b)"),
        f"float rb = {_select('mb', 'Rank')};\n",
        f"float rs = {_select('ms', 'Rank')};\n",
        "if (!single) {\n",
        f"if (mb == {TERRAIN_LIQUID_TILE}) {{\n",
        "col = lerp(col, shallowS, (1.0 - smoothstep(0.5, 0.95, fb)) * 0.75);\n",
        "if (dlt < 0.065) { col = lerp(col, foamS, pow(max(1.0 - dlt / 0.065, 0.0), 0.6) * 0.92); }\n",
        f"}} else if (ms == {TERRAIN_LIQUID_TILE}) {{\n",
        "if (dlt < 0.045) { col = lerp(col, bankS, 0.7 * (1.0 - dlt / 0.045)); }\n",
        "else if (dlt < 0.1) { col = lerp(col, bankS, 0.25 * (1.0 - (dlt - 0.045) / 0.055)); }\n",
        "} else if (rb > rs) {\n",
        "if (dlt < 0.032) { col = lerp(col, c1_lip, 0.75); }\n",
        "} else if (rb < rs) {\n",
        f"if (dlt < 0.055) {{ col = lerp(col, {hex_srgb_vec('#241A3A')}, 0.2 * (1.0 - dlt / 0.055)); }}\n",
        "}\n",
        "}\n",
        # contact shade under wall outcrops, darker skirt beyond the map border
        "col = lerp(col, col * float3(0.62, 0.66, 0.6), saturate(Wall) * 0.45);\n",
        "col = lerp(col, col * 0.86, saturate(Skirt));\n",
        _decode("lin", "col"),
        "return lin;\n",
    ]
    key = "AF_Terrain" if detailed else "AF_TerrainLow"
    return CustomNode(key, "web tile transitions + procedural ground patterns", "float3", terrain_inputs(), "".join(c))


TERRAIN_EMISSIVE_INPUTS = (NodeInput("Albedo", "float3"), NodeInput("Amb", "float4"))
_TERRAIN_EMISSIVE = """
return Albedo * float3(Amb.r, Amb.g, Amb.b);
"""


def terrain_emissive_node() -> CustomNode:
    return CustomNode("AF_TerrainAmbient", "ground ambient fill (MPC Ambient)", "float3", TERRAIN_EMISSIVE_INPUTS,
                      _fill(_TERRAIN_EMISSIVE, {}))


def all_nodes(sh: Shading) -> list[CustomNode]:
    """Every node the build creates (tests compile them all)."""
    nodes = [toon_node(sh), dither_node(), ink_node(sh), outline_wpo_node(sh), wind_node(), fx_mesh_node(sh),
             fx_mesh_mask_node(), ghost_node(), blob_node(), grade_node(), terrain_node(True), terrain_node(False),
             terrain_emissive_node()]
    nodes += list(fx_nodes().values()) + list(quad_nodes().values()) + list(water_nodes().values())
    return nodes


# Identifiers that must not appear as names in Custom-node code (HLSL keywords / UE globals / GLSL reserved words that
# survive cross-compilation badly).
RESERVED_IDENTIFIERS = frozenset("""
line point triangle lineadj triangleadj sample linear centroid nointerpolation noperspective precise shared groupshared
uniform in out inout vector matrix string texture sampler pass technique compile register packoffset snorm unorm half
dword min16float input output filter smooth flat fixed active common partition patch superp resource
View ResolvedView Primitive Material PI Parameters_ GetPrimitiveData
""".split())

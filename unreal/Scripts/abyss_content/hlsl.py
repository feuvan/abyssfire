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
    # C++ default argument in the test emulation only (tests/test_hlsl.py cpp_signature): an input appended to an
    # existing node keeps the older calls compiling with the value that reproduces the old behaviour. UE Custom-node
    # inputs have no defaults: the material graph always wires them.
    emu_default: str | None = None


@dataclass(frozen=True)
class CustomNode:
    key: str
    description: str
    output: str         # float | float2 | float3 | float4
    inputs: tuple[NodeInput, ...]
    code: str
    stage: str = "pixel"  # pixel | vertex
    # UMaterialExpressionCustom::AdditionalOutputs: `inout` parameters of the generated function after the inputs,
    # assigned by the code (`Lit = ...;`); the main output stays the `return` value.
    additional_outputs: tuple[NodeInput, ...] = ()

    @property
    def input_names(self) -> list[str]:
        return [i.name for i in self.inputs]

    def twin(self, output: str) -> "CustomNode":
        """The same code as a single-output node returning additional output `output` (key `<key>_<output>`): the
        content build's fallback for an engine that does not expose the additional output pin (ue/graph.py)."""
        outs = {o.name: o for o in self.additional_outputs}
        if output not in outs:
            raise ValueError(f"{self.key} has no additional output {output!r}")
        lines = self.code.rstrip("\n").split("\n")
        if not re.fullmatch(r"return [A-Za-z_][A-Za-z0-9_]*;", lines[-1].strip()):
            raise ValueError(f"{self.key}: the code must end with a single `return <name>;` line")
        decls = [f"{o.ctype} {o.name} = {_zero(o.ctype)};" for o in self.additional_outputs]
        code = "\n".join(decls + lines[:-1] + [f"return {output};"]) + "\n"
        return CustomNode(f"{self.key}_{output}", f"{self.description} ({output} only)", outs[output].ctype,
                          self.inputs, code, self.stage)


def _zero(ctype: str) -> str:
    n = {"float": 1, "float2": 2, "float3": 3, "float4": 4}[ctype]
    return "0.0" if n == 1 else f"{ctype}({', '.join(['0.0'] * n)})"


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
    # DECISIONS R19 (ground value): the palette colour pulled toward its luma (saturation) and scaled (value) in
    # display space before shading; 1 / 1 = the palette as painted (MI_AF_Toon_Foliage_<Theme>_Ground: grass, flowers).
    NodeInput("BaseSaturation", "float", "1.f"),
    NodeInput("BaseValue", "float", "1.f"),
)

LUMA_WEIGHTS = (0.299, 0.587, 0.114)    # web tone() / ColorGradePipeline luma (display-space saturation)


def _sat_value(var: str, sat: str, value: str) -> str:
    """lerp(luma(var), var, sat) * value on an sRGB-encoded float3, written so that sat = value = 1 leaves `var`
    bit-identical (x + (x - l) * 0 = x, x * 1 = x): the defaults reproduce today's colours exactly."""
    return (f"float {var}_luma = dot({var}, {vec3(LUMA_WEIGHTS)});\n"
            f"{var} = ({var} + ({var} - {var}_luma) * ({sat} - 1.0)) * {value};\n")


def sat_value_reference(srgb: tuple[float, float, float], sat: float, value: float) -> tuple[float, float, float]:
    """Python reference of _sat_value (sRGB 0..1 in and out)."""
    luma = sum(w * c for w, c in zip(LUMA_WEIGHTS, srgb))
    return tuple((luma + (c - luma) * sat) * value for c in srgb)  # type: ignore[return-value]


_TOON = """
@ENCODE_C@
@BASE_GRADE@
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


def _toon_constants(sh: Shading) -> dict[str, str]:
    half = sh.band_width / 2.0
    return {
        "ENCODE_C": _encode("c", "Base"),
        "BASE_GRADE": _sat_value("c", "BaseSaturation", "BaseValue"),
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
    }


def toon_node(sh: Shading) -> CustomNode:
    """AF_Toon: today's unlit toon colour (M_AF_Toon_Slime, the fade ghosts M_AF_Toon_Fade / _Foliage_Fade)."""
    code = _fill(_TOON, {**_toon_constants(sh), "DECODE_LIN": _decode("lin", "col")})
    return CustomNode("AF_Toon", "Abyssfire toon: 3-band half-Lambert in sRGB space + rim + grounding + feedback",
                      "float3", TOON_INPUTS, code)


# Shadow-catcher lit toon (M_AF_Toon / M_AF_Toon_Foliage, DECISIONS P11 revised): DefaultLit with
#   Emissive  = shade + (1 - k)(lit - shade)                    (shade = the toon's shade-band floor)
#   BaseColor = k (lit - shade) / SunColor                      (Lit output)
#   Normal    = MPC KeyLightDir, Specular 0, Metallic 0, Roughness 1
# The directional light (pi lux x SunColor, along -KeyLightDir) adds BaseColor x SunColor x shadow, so in full sun the
# sum is exactly today's AF_Toon colour and in a cast shadow it drops to shade + (1 - k)(lit - shade). k =
# CastShadowStrength (characters .7, loot .8, props 1, canopies .6). Where no sun shadow is rendered (mobile low / mid)
# the frame equals the unlit toon. Torches, campfires and spell glows light toon surfaces through the MPC light slots
# Light<i>PosR (xyz absolute cm, w radius cm) / Light<i>Col (rgb = colour x intensity x lightScale x CharLightGain,
# a > 0 = slot on), written by UAbyssVfxSystem (8 desktop / 4 mobile); no engine local light ever lights the toon.
TOON_LIGHT_SLOTS = 8
TOON_SPLIT_INPUTS = TOON_INPUTS + (
    NodeInput("Pw", "float3"),                  # absolute WorldPosition (cm)
    NodeInput("CastShadowStrength", "float"),   # MI scalar k
) + tuple(NodeInput(f"L{i}", "float4") for i in range(TOON_LIGHT_SLOTS)) \
  + tuple(NodeInput(f"C{i}", "float4") for i in range(TOON_LIGHT_SLOTS))
TOON_SPLIT_OUTPUTS = (NodeInput("Lit", "float3"),)
TORCH_WRAP = (0.35, 0.40, 0.50)     # back-facing floor, smoothstep edges on dot(n, l) * .5 + .5

_TOON_SPLIT = """
@ENCODE_C@
@BASE_GRADE@
float sAmt = saturate(P.r);
float lAmt = saturate(P.g);
float eAmt = saturate(P.b);
float3 n = normalize(N);
float3 lk = normalize(float3(L.r, L.g, L.b));
float ndl = dot(n, lk);
float hl = ndl * 0.5 + 0.5;
float3 shadeC = lerp(lerp(c, @COOL@, @COOL_MIX@), float3(0.0, 0.0, 0.0), sAmt);
float3 lightC = lerp(c, @WARM@, lAmt);
float gw = @GROUND_ALPHA@ * saturate(Ground) * saturate(GroundEnable);
float3 colL = lerp(shadeC, c, smoothstep(@TS0@, @TS1@, hl));
colL = lerp(colL, lightC, smoothstep(@TL0@, @TL1@, hl));
colL = lerp(colL, @GROUND@, gw);
float3 colS = lerp(shadeC, c, smoothstep(@TS0@, @TS1@, 0.0));
colS = lerp(colS, lightC, smoothstep(@TL0@, @TL1@, 0.0));
colS = lerp(colS, @GROUND@, gw);
float3 v = normalize(V);
float facing = saturate(dot(n, v));
float fres = pow(max(1.0 - facing, 1e-5), @RIM_POW@);
float rimOn = saturate(RimOverride);
float rimA = lerp(@RIM_ALPHA@, RimOverrideAlpha, rimOn);
float rimW = rimA * smoothstep(@RB0@, @RB1@, fres) * smoothstep(0.0, @RIM_EDGE@, ndl);
float3 rimC = lerp(float3(RimCol.r, RimCol.g, RimCol.b), float3(RimOverrideColor.r, RimOverrideColor.g, RimOverrideColor.b), rimOn);
colL = lerp(colL, rimC, rimW);
colL = lerp(colL, c, eAmt);
colS = lerp(colS, c, eAmt);
@DECODE_LIN_L@
@DECODE_LIN_S@
float3 one = float3(1.0, 1.0, 1.0);
float3 sunC = float3(SunCol.r, SunCol.g, SunCol.b);
float3 tnt = lerp(one, sunC, saturate(SunTint) * (1.0 - eAmt)) * (1.0 + @EM_BOOST@ * eAmt);
float3 mS = min(linL, linS);
float kCast = saturate(CastShadowStrength);
float3 Emit = (mS + (1.0 - kCast) * (linL - mS)) * tnt;
Lit = kCast * (linL - mS) * tnt / max(sunC, float3(0.05, 0.05, 0.05));
@DECODE_BASE@
float3 pw = float3(Pw.r, Pw.g, Pw.b);
float3 torch = float3(0.0, 0.0, 0.0);
@TORCH@
Emit = Emit + linBase * (1.0 - eAmt) * torch;
float3 painM = lerp(one, float3(PainTint.r, PainTint.g, PainTint.b), saturate(PainAmt));
Emit = Emit * painM;
Lit = Lit * painM;
float3 teleM = lerp(one, float3(Telegraph.r, Telegraph.g, Telegraph.b), saturate(TelegraphAmt));
Emit = Emit * teleM;
Lit = Lit * teleM;
float3 tintC = float3(StatusTint.r, StatusTint.g, StatusTint.b);
float3 statM = lerp(one, tintC, saturate(StatusAmt));
Emit = Emit * statM;
Lit = Lit * statM;
float gh = saturate(Ghost);
Emit = lerp(Emit, tintC * (0.35 + 0.9 * fres), gh);
Lit = Lit * (1.0 - gh);
float hi = saturate(max(Highlight, Inst.y));
float rim2 = (1.0 - facing) * (1.0 - facing);
Emit = Emit + float3(HighlightColor.r, HighlightColor.g, HighlightColor.b) * (hi * (0.18 + 0.82 * rim2));
float hf = saturate(HitFlash);
Emit = lerp(Emit, one, hf);
Lit = Lit * (1.0 - hf);
return Emit;
"""

# One MPC light slot (unrolled; a slot is skipped while its colour alpha is 0): the web light falloff on the toon,
# att = saturate(1 - d^2 / r^2)^2, wrapped by the normal so the far side keeps 35 %.
_TORCH_SLOT = """
if (C@I@.a > 0.0) {
float3 td@I@ = float3(L@I@.r, L@I@.g, L@I@.b) - pw;
float tdd@I@ = dot(td@I@, td@I@);
float tr@I@ = max(L@I@.a, 1.0);
float ta@I@ = saturate(1.0 - tdd@I@ / (tr@I@ * tr@I@));
float tn@I@ = dot(n, td@I@ / max(sqrt(tdd@I@), 0.01)) * 0.5 + 0.5;
torch = torch + float3(C@I@.r, C@I@.g, C@I@.b) * (ta@I@ * ta@I@ * lerp(@W0@, 1.0, smoothstep(@W1@, @W2@, tn@I@)));
}
"""


def toon_split_node(sh: Shading) -> CustomNode:
    """AF_ToonSplit: main output Emit (Emissive), additional output Lit (BaseColor) of the shadow-catcher lit toon."""
    w0, w1, w2 = TORCH_WRAP
    torch = "".join(_fill(_TORCH_SLOT, {"I": str(i), "W0": lit(w0), "W1": lit(w1), "W2": lit(w2)})
                    for i in range(TOON_LIGHT_SLOTS))
    code = _fill(_TOON_SPLIT, {**_toon_constants(sh), "DECODE_LIN_L": _decode("linL", "colL"),
                               "DECODE_LIN_S": _decode("linS", "colS"), "DECODE_BASE": _decode("linBase", "c"),
                               "TORCH": torch.strip()})
    return CustomNode("AF_ToonSplit", "Abyssfire shadow-catcher toon: Emit (emissive) + Lit (base colour, sun-shadowed)",
                      "float3", TOON_SPLIT_INPUTS, code, additional_outputs=TOON_SPLIT_OUTPUTS)


def toon_split_reference(sh: Shading, base_srgb: tuple[float, float, float], params: tuple[float, float, float],
                         n, v, grounding: float = 0.0, cast_shadow: float = 1.0,
                         sun_color: tuple[float, float, float] = (1.0, 1.0, 1.0), sun_tint: float = 1.0,
                         base_saturation: float = 1.0, base_value: float = 1.0
                         ) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    """Python reference of AF_ToonSplit without feedback and torch slots: (Emit, Lit), linear. base_srgb is the palette
    colour in sRGB 0..1, params = (shadowAmt, lightAmt, emissive). The rendered colour under a sun-shadow term S is
    Emit + Lit x sun_color x S (Art-pass look-dev: shade + (lit - shade) x (1 - k + k S))."""
    def smooth(e0: float, e1: float, x: float) -> float:
        t = min(max((x - e0) / (e1 - e0), 0.0), 1.0)
        return t * t * (3.0 - 2.0 * t)

    def unit(a) -> list[float]:
        ln = math.sqrt(sum(x * x for x in a))
        return [x / ln for x in a]

    def mix(a, b, t: float) -> list[float]:
        return [x + (y - x) * t for x, y in zip(a, b)]

    def decode(x: float) -> float:
        x = min(max(x, 0.0), 1.0)
        return x / 12.92 if x < 0.04045 else ((x + 0.055) / 1.055) ** 2.4

    s, l, e = (min(max(x, 0.0), 1.0) for x in params)
    c = list(sat_value_reference(base_srgb, base_saturation, base_value))
    half = sh.band_width / 2.0
    N, V, Lk = unit(n), unit(v), unit(sh.key_light)
    ndl = sum(a * b for a, b in zip(N, Lk))
    hl = ndl * 0.5 + 0.5
    cool = [x / 255.0 for x in sh.cool_shade]
    warm = [x / 255.0 for x in sh.warm_light]
    ground = [x / 255.0 for x in hex_to_rgb255(sh.ground_color)]
    rim_c = [x / 255.0 for x in hex_to_rgb255(sh.rim_color)]
    shade = [x * (1.0 - s) for x in mix(c, cool, sh.cool_shade_mix)]
    light = mix(c, warm, l)
    gw = sh.ground_alpha * min(max(grounding, 0.0), 1.0)
    col_l = mix(mix(mix(shade, c, smooth(sh.t_shade - half, sh.t_shade + half, hl)), light,
                    smooth(sh.t_light - half, sh.t_light + half, hl)), ground, gw)
    col_s = mix(mix(mix(shade, c, smooth(sh.t_shade - half, sh.t_shade + half, 0.0)), light,
                    smooth(sh.t_light - half, sh.t_light + half, 0.0)), ground, gw)
    facing = min(max(sum(a * b for a, b in zip(N, V)), 0.0), 1.0)
    fres = max(1.0 - facing, 1e-5) ** sh.rim_power
    rim_w = sh.rim_alpha * smooth(sh.rim_band[0], sh.rim_band[1], fres) * smooth(0.0, sh.rim_mask_edge, ndl)
    col_l = mix(mix(col_l, rim_c, rim_w), c, e)
    col_s = mix(col_s, c, e)
    k = min(max(cast_shadow, 0.0), 1.0)
    emit, lit_out = [], []
    for i in range(3):
        tnt = (1.0 + (sun_color[i] - 1.0) * min(max(sun_tint, 0.0), 1.0) * (1.0 - e)) * (1.0 + sh.emissive_boost * e)
        lin_l, lin_s = decode(col_l[i]), decode(col_s[i])
        m = min(lin_l, lin_s)
        emit.append((m + (1.0 - k) * (lin_l - m)) * tnt)
        lit_out.append(k * (lin_l - m) * tnt / max(sun_color[i], 0.05))
    return (emit[0], emit[1], emit[2]), (lit_out[0], lit_out[1], lit_out[2])


# Dithered opacity (occlusion / death fade of the masked toon and ink, FX meshes): ordered 4x4 Bayer threshold `t`,
# shifted every temporal AA sample. View.TemporalAAParams = (jitter index k, sequence length n, ...): UE's FViewInfo
# TemporalJitterIndex / TemporalJitterSequenceLength, (0, 1) on every frame without temporal AA; TSR runs a prime n
# (11 at 100 % screen percentage), TAA r.TemporalAASamples (8). The shift visits k * p / n (mod 1) with p the integer
# nearest 0.618 n: coprime with a prime n, so one sequence visits every 1/n step once (exact coverage per pixel) in a
# golden-ratio order (no runs of kept samples). Each frame keeps exactly a x 16 of the 16 cells (the thresholds stay
# 1/16 apart, only shifted) and every pixel about a of the samples, so TSR / TAA resolve the fade to a smooth alpha
# (DECISIONS R1) instead of the fixed screen door of the third Mac playtest. Without temporal AA, k = 0: the static
# ordered pattern (no flicker); the zone dressing fades through a translucent copy there (AAbyssZoneActor,
# abyss.FadeGhost).
_BAYER_TEMPORAL = """
float2 q = floor(fmod(Parameters.SvPosition.xy, float2(4.0, 4.0)));
uint qx = (uint)q.x;
uint qy = (uint)q.y;
uint m = qx ^ qy;
uint b = ((m & 1u) << 3) | ((qy & 1u) << 2) | (m & 2u) | ((qy & 2u) >> 1);
float tn = max(View.TemporalAAParams.y, 1.0);
float ts = frac(View.TemporalAAParams.x * floor(tn * 0.618034 + 0.5) / tn);
float t = frac(((float)b + 0.5) / 16.0 + ts);
"""

DITHER_INPUTS = (NodeInput("Fade", "float"), NodeInput("Inst", "float2"), NodeInput("BodyOpacity", "float"))
_DITHER = """
float a = saturate(BodyOpacity) * (1.0 - saturate(max(Fade, Inst.x)));
@BAYER@
return a > t ? 1.0 : 0.0;
"""


def dither_node() -> CustomNode:
    return CustomNode("AF_Dither", "Abyssfire dithered opacity mask (Bayer 4x4, rotated per temporal AA sample)",
                      "float", DITHER_INPUTS, _fill(_DITHER, {"BAYER": _BAYER_TEMPORAL.strip()}))


# Shadow-pass mask of M_AF_Toon / M_AF_Toon_Foliage (ShadowPassSwitch): the death / prop fade (CPD 13) and BodyOpacity
# only. The occlusion fade of instanced dressing (PerInstanceCustomData 0) never thins its sun shadow: a faded tree
# still stands there, and the translucent copy that replaces the hidden instance on tiers without temporal AA casts none.
DITHER_SHADOW_INPUTS = (NodeInput("Fade", "float"), NodeInput("BodyOpacity", "float"))
_DITHER_SHADOW = """
float a = saturate(BodyOpacity) * (1.0 - saturate(Fade));
@BAYER@
return a > t ? 1.0 : 0.0;
"""


def dither_shadow_node() -> CustomNode:
    return CustomNode("AF_DitherShadow", "Abyssfire shadow-pass opacity mask (no instance occlusion fade)", "float",
                      DITHER_SHADOW_INPUTS, _fill(_DITHER_SHADOW, {"BAYER": _BAYER_TEMPORAL.strip()}))


# Blended opacity (M_AF_Toon_Slime and the M_AF_Toon_Fade / M_AF_Toon_Foliage_Fade copies of faded dressing,
# translucent): BodyOpacity faded by the death / occlusion fade. A Bayer screen door is visible wherever nothing
# averages it (MSAA on mobile, FXAA on the desktop low tier); the slime previews render this blend (DECISIONS R12).
_FADE_OPACITY = """
return saturate(BodyOpacity) * (1.0 - saturate(max(Fade, Inst.x)));
"""


def fade_opacity_node() -> CustomNode:
    return CustomNode("AF_FadeOpacity", "Abyssfire blended body opacity (translucent slime shell)", "float",
                      DITHER_INPUTS, _fill(_FADE_OPACITY, {}))


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
    # DECISIONS R3 (revised, art pass): MPC InkScale (1.5: the class values read as px per 720 rows) and extra pixels
    # beyond the scaled ink (0 for the ink; M_AF_SelectRim: RimPx720 x 1.5).
    NodeInput("InkScale", "float", "1.f"),
    NodeInput("ExtraPx1080", "float", "0.f"),
)
_OUTLINE_WPO = """
float3 n = normalize(N);
float dist = length(Pos);
float3 v = dist > 0.0001 ? Pos / dist : float3(0.0, 0.0, 1.0);
float depth = max(dot(Pos, F), 1.0);
float3 np = n - dot(n, v) * v;
float3 dir = np / max(length(np), @EPS@);
float width = (Px * InkScale + ExtraPx1080) * 2.0 * depth * T.y / 1080.0;
return (dir * width - n * Baked) * saturate(Enable);
"""


def outline_wpo_node(sh: Shading) -> CustomNode:
    return CustomNode("AF_OutlineWPO", "Abyssfire outline: hull silhouette dilated by (OutlinePx1080 x InkScale + "
                      "ExtraPx1080) x ViewSizeY / 1080 px", "float3", OUTLINE_WPO_INPUTS,
                      _fill(_OUTLINE_WPO, {"EPS": lit(sh.outline_eps)}), stage="vertex")


def outline_px_at(px1080: float, rows: float, ink_scale: float = 1.5, extra_px1080: float = 0.0) -> float:
    """Screen width (px) of the AF_OutlineWPO dilation at a viewport `rows` high (DECISIONS R3)."""
    return (px1080 * ink_scale + extra_px1080) * rows / 1080.0


# Selection rim (M_AF_SelectRim, DECISIONS R16): the rim hull's outline offset `Off` (AF_OutlineWPO with the rim's extra
# pixels) plus a push of up to RimDepthPushCm along the camera ray through the offset vertex: same screen position,
# deeper, so the rim only shows outside the whole figure's silhouette (never round a sword held in front of the body).
# Near the ground the push is clamped so the pushed vertex stays RIM_GROUND_MARGIN_CM above the owner's ground plane
# (`Feet` = ActorPositionWS, camera-relative, the actor stands on the ground): an unclamped 30 cm push sinks the rim
# below the terrain wherever the silhouette is lower than 30 x sin(depression) ~ 19-26 cm (boots, goblin legs, most of
# the slime), and the depth test removes it there. Pos / Off / Feet share the translated-world frame (z up).
RIM_GROUND_MARGIN_CM = 2.0
RIM_MIN_DROP = 0.2      # the ray's minimum downward slope used by the clamp (a level ray would allow any push)
VIEW_PUSH_INPUTS = (NodeInput("Pos", "float3"), NodeInput("Off", "float3"), NodeInput("Push", "float"),
                    NodeInput("Feet", "float3", "float3(0.f, 0.f, -1.0e6f)"))
_VIEW_PUSH = """
float3 p = Pos + Off;
float d = length(p);
float3 dir = d > 0.0001 ? p / d : float3(0.0, 0.0, 1.0);
float above = p.z - Feet.z - @MARGIN@;
float push = clamp(above / max(-dir.z, @MIN_DROP@), 0.0, max(Push, 0.0));
return Off + dir * push;
"""


def view_push_node() -> CustomNode:
    return CustomNode("AF_ViewPush", "selection rim: outline offset pushed along the camera ray, kept above the ground",
                      "float3", VIEW_PUSH_INPUTS,
                      _fill(_VIEW_PUSH, {"MARGIN": lit(RIM_GROUND_MARGIN_CM), "MIN_DROP": lit(RIM_MIN_DROP)}),
                      stage="vertex")


SELECT_RIM_MASK_INPUTS = (NodeInput("Fade", "float"), NodeInput("BodyOpacity", "float"), NodeInput("BodyMask", "float"))
_SELECT_RIM_MASK = """
float a = saturate(BodyOpacity) * (1.0 - saturate(Fade));
@BAYER@
return (a > t ? 1.0 : 0.0) * (1.0 - saturate(BodyMask));
"""


def select_rim_mask_node() -> CustomNode:
    """The body's dithered Fade (CPD 13, the same temporal Bayer as AF_Dither) x (1 - BodyMask): BodyMask 1 = this slot
    draws nothing (the weapon copies' slot 0)."""
    return CustomNode("AF_SelectRimMask", "selection rim opacity: the body's dithered fade, BodyMask hides a slot",
                      "float", SELECT_RIM_MASK_INPUTS, _fill(_SELECT_RIM_MASK, {"BAYER": _BAYER_TEMPORAL.strip()}))


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


# The FX mesh fade over life: the same temporally rotated Bayer as AF_Dither (resolved by TSR / TAA; static and brief
# elsewhere). The ink hull is baked into the same mesh and material, so it keeps the same pixels as the body.
FX_MESH_MASK_INPUTS = (NodeInput("Col", "float4"),)
_FX_MESH_MASK = """
float a = saturate(Col.a);
@BAYER@
return a > t ? 1.0 : 0.0;
"""


def fx_mesh_mask_node() -> CustomNode:
    return CustomNode("AF_FxMeshMask", "dithered FX mesh fade", "float", FX_MESH_MASK_INPUTS,
                      _fill(_FX_MESH_MASK, {"BAYER": _BAYER_TEMPORAL.strip()}))


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


# Light AO (M_AF_PP_AOComposite, DECISIONS P14; desktop balanced / high only, bound by AAbyssZoneActor): the scene colour
# before tonemapping (BL_SCENE_COLOR_AFTER_DOF) darkened by the engine's SSAO, AOStrength of the way, on the visible
# ground only. The terrain alone renders custom depth (AAbyssZoneActor, while AO is on), so a pixel is ground where the
# scene depth reaches the custom depth: characters, props, foliage, ink and the selection rim stand in front of it and
# keep their colour (the lit toon's GBuffer normal is the constant MPC KeyLightDir, which classic SSAO would orient its
# hemisphere by, and the rim sits 30 cm behind its figure). No custom depth at all (the far plane) = no AO.
AO_GROUND_TOLERANCE_CM = 1.0    # depth slack (both passes rasterise the same terrain; a sole touching the ground)
AO_GROUND_FADE_CM = 4.0         # in front of the ground by more than tolerance + fade: no AO
AO_COMPOSITE_INPUTS = (NodeInput("Scene", "float4"), NodeInput("AO", "float4"), NodeInput("AOStrength", "float"),
                       NodeInput("SceneDepth", "float4", "float4(0.f, 0.f, 0.f, 0.f)"),
                       NodeInput("GroundDepth", "float4", "float4(0.f, 0.f, 0.f, 0.f)"))
_AO_COMPOSITE = """
float gap = GroundDepth.r - SceneDepth.r;
float ground = 1.0 - saturate((gap - @TOL@) / @FADE@);
float ao = lerp(1.0, saturate(AO.r), saturate(AOStrength) * ground);
return float3(Scene.r, Scene.g, Scene.b) * ao;
"""


def ao_composite_node() -> CustomNode:
    return CustomNode("AF_AOComposite", "scene colour x lerp(1, SSAO, AOStrength) on the visible ground (custom depth)",
                      "float3", AO_COMPOSITE_INPUTS,
                      _fill(_AO_COMPOSITE, {"TOL": lit(AO_GROUND_TOLERANCE_CM), "FADE": lit(AO_GROUND_FADE_CM)}))


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


# DECISIONS R19 (ground value): the composited ground (sRGB) pulled toward its luma and scaled, so actors pop; MI scalars
# (MI_AF_Terrain_Plains .85 / .93), 1 / 1 = bit-identical to the uncompressed ground.
GROUND_GRADE_INPUTS = (NodeInput("GroundSaturation", "float", "1.f"), NodeInput("GroundValue", "float", "1.f"))
GROUND_GRADE_CODE = _sat_value("col", "GroundSaturation", "GroundValue")


def terrain_inputs() -> tuple[NodeInput, ...]:
    ins = [NodeInput("UV", "float2"), NodeInput("Wall", "float"), NodeInput("Skirt", "float"),
           NodeInput("TileIds", "Texture2D"), NodeInput("MapCols", "float"), NodeInput("MapRows", "float")]
    for t in TERRAIN_TILES:
        for f in TERRAIN_FIELDS:
            ins.append(NodeInput(f"Tile{t}{f}", "float3"))
        ins.append(NodeInput(f"Tile{t}Rank", "float"))
    ins += [NodeInput("WaterShallow", "float3"), NodeInput("WaterFoam", "float3"), NodeInput("WaterBank", "float3")]
    return tuple(ins) + GROUND_GRADE_INPUTS


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


def _terrain_mask_code(detailed: bool, prelude: tuple[str, ...] = ()) -> list[str]:
    """The web tile mask (ZoneTerrain.ts quadMask) up to the winner mb / runner-up ms and the coverage al, plus the own
    tile (own_m, own_var, own_c). `prelude` lines go right after `g` (uniform control flow: derivatives)."""
    waves, lo, hi = lattice_noise_waves()
    c: list[str] = [
        "int cols = max((int)(MapCols + 0.5), 1);\n",
        "int rows = max((int)(MapRows + 0.5), 1);\n",
        "float2 g = UV;\n",
        *prelude,
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
    return c


def _terrain_tint_code(lazy: bool) -> list[str]:
    """c1 (winner) over c2 (runner-up) by al, then the web edge tints (shallows / foam, bank, lip, dark edge), the wall
    contact shade and the skirt; returns linear colour. lazy=True encodes the edge colours only inside their branches
    (the painted node: no c1_lip from a pattern block, and no sRGB encodes of uniforms on every interior pixel)."""
    shallow = _encode("shallowS", "float3(WaterShallow.r, WaterShallow.g, WaterShallow.b)")
    foam = _encode("foamS", "float3(WaterFoam.r, WaterFoam.g, WaterFoam.b)")
    bank = _encode("bankS", "float3(WaterBank.r, WaterBank.g, WaterBank.b)")
    c: list[str] = ["float3 col = lerp(c2, c1, al);\n"]
    if not lazy:
        c += [shallow, foam, bank]
    c += [
        f"float rb = {_select('mb', 'Rank')};\n",
        f"float rs = {_select('ms', 'Rank')};\n",
        "if (!single) {\n",
        f"if (mb == {TERRAIN_LIQUID_TILE}) {{\n",
    ]
    if lazy:
        c += [shallow, foam]
    c += [
        "col = lerp(col, shallowS, (1.0 - smoothstep(0.5, 0.95, fb)) * 0.75);\n",
        "if (dlt < 0.065) { col = lerp(col, foamS, pow(max(1.0 - dlt / 0.065, 0.0), 0.6) * 0.92); }\n",
        f"}} else if (ms == {TERRAIN_LIQUID_TILE}) {{\n",
    ]
    if lazy:
        c.append(bank)
    c += [
        "if (dlt < 0.045) { col = lerp(col, bankS, 0.7 * (1.0 - dlt / 0.045)); }\n",
        "else if (dlt < 0.1) { col = lerp(col, bankS, 0.25 * (1.0 - (dlt - 0.045) / 0.055)); }\n",
        "} else if (rb > rs) {\n",
    ]
    if lazy:
        c += ["if (dlt < 0.032) {\n", _encode("c1_lip", _f3(_select("mb", "Lip"))),
              "col = lerp(col, c1_lip, 0.75);\n", "}\n"]
    else:
        c.append("if (dlt < 0.032) { col = lerp(col, c1_lip, 0.75); }\n")
    c += [
        "} else if (rb < rs) {\n",
        f"if (dlt < 0.055) {{ col = lerp(col, {hex_srgb_vec('#241A3A')}, 0.2 * (1.0 - dlt / 0.055)); }}\n",
        "}\n",
        "}\n",
        # contact shade under wall outcrops, darker skirt beyond the map border
        "col = lerp(col, col * float3(0.62, 0.66, 0.6), saturate(Wall) * 0.45);\n",
        "col = lerp(col, col * 0.86, saturate(Skirt));\n",
        GROUND_GRADE_CODE,
        _decode("lin", "col"),
        "return lin;\n",
    ]
    return c


def terrain_node(detailed: bool = True) -> CustomNode:
    """detailed=False is the low-quality branch (QualitySwitch Low): no boundary noise, no specks / details."""
    c = _terrain_mask_code(detailed)
    c.append(_pattern_block("c1", "mb", "own", detailed))
    c.append(_pattern_block("c2", "ms", "own", detailed))
    c += _terrain_tint_code(lazy=False)
    key = "AF_Terrain" if detailed else "AF_TerrainLow"
    return CustomNode(key, "web tile transitions + procedural ground patterns", "float3", terrain_inputs(), "".join(c))


# ---------------------------------------------------------------------------------------------------------------------
# Painted terrain (M_AF_Terrain_Painted; manifest terrain.<theme>, Art/blender/environment/terrain.py composite()): the
# same tile mask, but the ground colours are the art's painted 3 x 3-tile layers (sampled outside the node at UV0 / 3
# with one shared wrap sampler, handed in as linear Tile<i>Layer) and the per-tile detail atlas, composited over the
# winner exactly like terrain.composite(): own tile h = tileHash(c, r, 7) (TileIds G = h % 12) >= 6 -> cell (row of
# the own paint, column h % 3), tile-local frac(UV0 + 0.5), straight alpha, only where the winner is the own material.
# ---------------------------------------------------------------------------------------------------------------------
TERRAIN_DETAIL_ROWS = (0, 1, 2, 5)    # atlas rows: grass, dirt, stone, camp (water has no painted detail: lily meshes)


def terrain_painted_inputs() -> tuple[NodeInput, ...]:
    ins = [NodeInput("UV", "float2"), NodeInput("Wall", "float"), NodeInput("Skirt", "float"),
           NodeInput("TileIds", "Texture2D"), NodeInput("MapCols", "float"), NodeInput("MapRows", "float")]
    for t in TERRAIN_TILES:
        ins += [NodeInput(f"Tile{t}Layer", "float3"), NodeInput(f"Tile{t}Lip", "float3"),
                NodeInput(f"Tile{t}Rank", "float")]
    ins += [NodeInput("WaterShallow", "float3"), NodeInput("WaterFoam", "float3"), NodeInput("WaterBank", "float3"),
            NodeInput("Details", "Texture2D")]
    return tuple(ins) + GROUND_GRADE_INPUTS


def terrain_painted_node(detailed: bool = True) -> CustomNode:
    """detailed=False (QualitySwitch Low) drops the boundary noise only; the layers and details are texture reads."""
    cells = float(len(TERRAIN_DETAIL_ROWS))
    row = "-1.0"
    for i, t in reversed(list(enumerate(TERRAIN_DETAIL_ROWS))):
        row = f"(own_m == {t} ? {lit(float(i))} : {row})"
    inv = lit(1.0 / cells)
    # atlas UV derivatives from the continuous tile coordinate: the cell jump at tile borders must not pick a coarse mip
    prelude = (f"float2 dgx = ddx(g) * {inv};\n", f"float2 dgy = ddy(g) * {inv};\n")
    c = _terrain_mask_code(detailed, prelude)
    c += [
        _encode("c1", _f3(_select("mb", "Layer"))),
        _encode("c2", _f3(_select("ms", "Layer"))),
        f"float drow = {row};\n",
        "float dsel = (own_var >= 6 && mb == own_m && drow >= 0.0) ? 1.0 : 0.0;\n",
        f"float2 duv = float2(((float)(own_var % 3) + g.x - own_c.x + 0.5) * {inv},"
        f" (max(drow, 0.0) + g.y - own_c.y + 0.5) * {inv});\n",
        "float4 dcell = Details.SampleGrad(DetailsSampler, duv, dgx, dgy);\n",
        _encode("detS", "float3(dcell.r, dcell.g, dcell.b)"),
        "c1 = lerp(c1, detS, saturate(dcell.a) * dsel);\n",
    ]
    c += _terrain_tint_code(lazy=True)
    key = "AF_TerrainPainted" if detailed else "AF_TerrainPaintedLow"
    return CustomNode(key, "web tile transitions over the painted ground layers + detail atlas", "float3",
                      terrain_painted_inputs(), "".join(c))


TERRAIN_EMISSIVE_INPUTS = (NodeInput("Albedo", "float3"), NodeInput("Amb", "float4"))
_TERRAIN_EMISSIVE = """
return Albedo * float3(Amb.r, Amb.g, Amb.b);
"""


def terrain_emissive_node() -> CustomNode:
    return CustomNode("AF_TerrainAmbient", "ground ambient fill (MPC Ambient)", "float3", TERRAIN_EMISSIVE_INPUTS,
                      _fill(_TERRAIN_EMISSIVE, {}))


# ---------------------------------------------------------------------------------------------------------------------
# Enemy telegraph decal (M_AF_Telegraph; dark remake U2 / 2G, DECISIONS DR5 / DR16). The A12 contract (manifest
# materials.M_AF_Telegraph) as Art/blender/vfx/fx_preview_dark.py telegraph_layer emulates it per pixel, with the
# shapes as exact signed distances (inside = the core's ShapeContains, body radius 0; the line is the core's box) and
# the edge anti-aliased over the pixel footprint. Everything per decal / per frame is custom primitive data, in the
# order of Source/Abyssfire/Private/Vfx/AbyssTelegraphRules.h `Cpd` (tests/test_hlsl.py reads that header):
#   ShapeP  (0..3)   shape code (0 circle, 1 ring, 2 cone, 3 line), A (radius / length cm), B (inner radius cm / half
#                    opening rad / half width cm), the rim width (cm)
#   QuadP   (4..7)   the quad's size (cm) and the shape's origin in its frame (cm; SM_FX_Quad: UV (0,0) at (-50, +50))
#   InnerP  (8..11)  inner rim alpha, its inset from the boundary (cm), its width (cm); the arena's wash band (cm)
#   ModeP   (12..15) fill on, rim outside (arena), flash, progress
#   AlphaP  (16..19) fill (or wash) alpha, the front band's alpha, the rim alpha (urgency / pulse / fade applied), spare
# Constants (the contract's params, baked as parameter defaults): the colours, FillGrain, FillTileCm, FrontWidthCm,
# RimTileCm, WashFadeInCm. Textures: Rim (T_FX_TelegraphRim, U along the boundary / RimTileCm, V = the distance into
# the rim / RimWidthCm, clamped to the strip's first and last row centres) and Fill (T_FX_TelegraphFill on world XY /
# FillTileCm, so the grain never swims with the progress); both sampled with explicit gradients from continuous
# coordinates (no mip seam where the arc length wraps) and wrapped in code (FX textures import clamped).
# Blank pixels return early (2G review m2), after the derivatives (uniform flow) and before any texture sample: past the
# boundary of a decal drawn inward (the quad's corners, a ring's hole) and inside the arena (rim and wash outside, no
# inner rim); the full path gives exactly float4(0) there. The quad still covers them (the arena's quad spans the whole
# disc, 18.6 m across at 9 tiles without the wash): the early exit saves their shading, not their rasterisation or blend.
# ---------------------------------------------------------------------------------------------------------------------
TELEGRAPH_RIM_ROWS = 64         # T_FX_TelegraphRim is a 256 x 64 profile strip (fx_textures_dark.py)
TELEGRAPH_WASH_FADE_OUT_CM = 100.0   # AbyssTelegraphRules::ArenaWashFadeOutCm: the band's last 100 cm fade out
TELEGRAPH_CPD = {"ShapeP": 0, "QuadP": 4, "InnerP": 8, "ModeP": 12, "AlphaP": 16}
TELEGRAPH_CPD_FIELDS = {        # AbyssTelegraphRules::Cpd names, in channel order
    "ShapeP": ("Shape", "A", "B", "RimWidthCm"),
    "QuadP": ("SizeXCm", "SizeYCm", "LocalOriginXCm", "LocalOriginYCm"),
    "InnerP": ("InnerRimAlpha", "InnerRimInsetCm", "InnerRimWidthCm", "WashBandCm"),
    "ModeP": ("FillOn", "RimOutside", "Flash", "Progress"),
    "AlphaP": ("FillAlpha", "FrontAlpha", "RimAlpha", "Spare"),
}
TELEGRAPH_INPUTS = (
    NodeInput("UV", "float2"), NodeInput("Pw", "float3"),
    NodeInput("ShapeP", "float4"), NodeInput("QuadP", "float4"), NodeInput("InnerP", "float4"),
    NodeInput("ModeP", "float4"), NodeInput("AlphaP", "float4"),
    NodeInput("FillColor", "float3"), NodeInput("RimColor", "float3"), NodeInput("FrontColor", "float3"),
    NodeInput("FlashRimColor", "float3"), NodeInput("WashColor", "float3"),
    NodeInput("FillGrain", "float"), NodeInput("FillTileCm", "float"), NodeInput("FrontWidthCm", "float"),
    NodeInput("RimTileCm", "float"), NodeInput("WashFadeInCm", "float"),
    NodeInput("Rim", "Texture2D"), NodeInput("Fill", "Texture2D"),
)
_TELEGRAPH = """
float2 p = float2((UV.x - 0.5) * QuadP.r - QuadP.b, (0.5 - UV.y) * QuadP.g - QuadP.a);
float2 dpx = ddx(p);
float2 dpy = ddy(p);
float lx = length(dpx);
float ly = length(dpy);
float px = max(max(lx, ly), 1.0);
float fillTile = max(FillTileCm, 1.0);
float2 fw = float2(Pw.x, Pw.y) / fillTile;
float2 fgx = ddx(fw);
float2 fgy = ddy(fw);
float shp = ShapeP.r;
float ra = ShapeP.g;
float rb = ShapeP.b;
float rimW = max(ShapeP.a, 1.0);
float rr = length(p);
float ang = atan2(p.y, p.x);
float w = ra - rr;
float edgeU = ang * ra;
float fillR = rr;
float fillMax = ra;
if (shp > 0.5 && shp < 1.5)
{
    w = min(ra - rr, rr - rb);
    fillR = rr - rb;
    fillMax = ra - rb;
}
else if (shp > 1.5 && shp < 2.5 && rb < @FULL_TURN@)
{
    float qx = abs(p.y);
    float qy = p.x;
    float cx = sin(rb);
    float cy = cos(rb);
    float tt = clamp(qx * cx + qy * cy, 0.0, ra);
    float mx = qx - cx * tt;
    float my = qy - cy * tt;
    float md = sqrt(mx * mx + my * my);
    float sd = cy * qx - cx * qy;
    float sg = sd > 0.0 ? 1.0 : (sd < 0.0 ? -1.0 : 0.0);
    w = -max(rr - ra, md * sg);
    edgeU = (ra - rr) < md ? ang * ra : rr;
}
else if (shp > 2.5)
{
    float hl = 0.5 * ra;
    float bx = abs(p.x - hl) - hl;
    float by = abs(p.y) - rb;
    float ox = max(bx, 0.0);
    float oy = max(by, 0.0);
    w = -(sqrt(ox * ox + oy * oy) + min(max(bx, by), 0.0));
    float ends = min(p.x, ra - p.x);
    float sides = rb - abs(p.y);
    edgeU = sides < ends ? p.x : p.y;
    fillR = p.x;
}
float outside = ModeP.g;
float wr = outside > 0.5 ? -w : w;
if (wr < -0.5 * px && (outside < 0.5 || InnerP.r <= 0.0))
{
    return float4(0.0, 0.0, 0.0, 0.0);
}
float aa = saturate(wr / px + 0.5);
float rimTile = max(RimTileCm, 1.0);
float2 ruv = float2(frac(edgeU / rimTile), clamp(wr / rimW, @VMIN@, @VMAX@));
float4 rs = Rim.SampleGrad(RimSampler, ruv, float2(lx / rimTile, lx / rimW), float2(ly / rimTile, ly / rimW));
float rimA = rs.a * AlphaP.b * aa * (wr <= rimW ? 1.0 : 0.0);
float flash = saturate(ModeP.b);
float3 rimC = lerp(float3(RimColor.r, RimColor.g, RimColor.b), float3(FlashRimColor.r, FlashRimColor.g, FlashRimColor.b), flash);
float3 rimRgb = float3(rs.r * rimC.r, rs.g * rimC.g, rs.b * rimC.b);
float di = abs(w - InnerP.g);
float innerA = saturate(1.0 - (di - InnerP.b * 0.5) / max(1.5, px)) * InnerP.r * (w > 0.0 ? 1.0 : 0.0);
rimA = max(rimA, innerA);
float3 fillRgb = float3(0.0, 0.0, 0.0);
float fillA = 0.0;
if (outside > 0.5)
{
    float washBand = InnerP.a;
    float washIn = saturate(wr / max(WashFadeInCm, 1.0));
    float washOut = washBand > 0.0 ? saturate((washBand - wr) / @WASH_FADE_OUT@) : 0.0;
    fillRgb = float3(WashColor.r, WashColor.g, WashColor.b);
    fillA = AlphaP.r * washIn * washOut * (wr > 0.0 ? 1.0 : 0.0);
}
else if (ModeP.r > 0.5)
{
    float4 fs = Fill.SampleGrad(FillSampler, float2(frac(fw.x), frac(fw.y)), fgx, fgy);
    float3 grain = (1.0 - FillGrain * (1.0 - float3(fs.r, fs.g, fs.b))) * float3(FillColor.r, FillColor.g, FillColor.b);
    float front = saturate(ModeP.a) * fillMax;
    float filled = (w > 0.0 && fillR <= front) ? 1.0 : 0.0;
    float frontBand = fillR >= front - FrontWidthCm ? filled : 0.0;
    fillRgb = lerp(grain, float3(FrontColor.r, FrontColor.g, FrontColor.b), max(frontBand, flash));
    fillA = filled * lerp(AlphaP.r, AlphaP.g, frontBand);
}
float alpha = rimA + fillA * (1.0 - rimA);
float3 rgb = (rimRgb * rimA + fillRgb * (fillA * (1.0 - rimA))) / max(alpha, 1e-6);
return float4(rgb.r, rgb.g, rgb.b, saturate(alpha));
"""


def telegraph_node() -> CustomNode:
    rows = float(TELEGRAPH_RIM_ROWS)
    return CustomNode("AF_Telegraph", "enemy ground telegraph (rim + fill to contact; zones; the arena ring)", "float4",
                      TELEGRAPH_INPUTS, _fill(_TELEGRAPH, {
                          "FULL_TURN": lit(3.14159),
                          "VMIN": lit(0.5 / rows), "VMAX": lit(1.0 - 0.5 / rows),
                          "WASH_FADE_OUT": lit(TELEGRAPH_WASH_FADE_OUT_CM),
                      }))


# ---------------------------------------------------------------------------------------------------------------------
# Ash dissolve (dark remake U3 / 2G stage C; manifest materials.M_AF_AshDissolve, Art/blender/vfx/fx_recipes_dark.py
# ASH_DISSOLVE, preview port fx_preview_dark.apply_ash): the twins M_AF_Toon_Ash / M_AF_Outline_Ash a dead body swaps to
# (AAbyssCharacterActor::SetAshLook; per actor dynamic instances with the scalars Grey / Dissolve / HuskTopCm).
#
# AF_AshMask: a pixel drops when (B x .6 + R x .3 + (1 - G) x .15 + heightFraction x .3) < Dissolve x 1.4 - T_FX_AshNoise
# sampled at the pre-skinned (object-space) XZ / NoiseTileCm (R fBm front, G cracks, B per-plate random: whole plates
# fall), heightFraction = the height over the actor's feet / HuskTopCm; the surviving band EdgeWidth above the cut is the
# edge (additional output). Dissolve 0 keeps every pixel; Dissolve 1 drops every pixel (the score never reaches 1.4).
# The texture is clamped at import (FX sprites): the node wraps it (frac) with explicit gradients of the continuous UV.
# AF_AshGrey: the toon's two parts (Emit, Lit: the shadow-catcher split) lerp toward the ash colours by Grey, as the
# preview greys the final image: the luminance L of the unshadowed colour (Emit + Lit x sun) and of the shadowed one (Emit)
# picks lerp(AshShade, AshColor, saturate((L - .02) / .12)); a near-black texel (L < .012: pupils, slits) keeps x .9; the
# edge band glows EdgeColor at .6. Grey 0 and no edge return Emit / Lit unchanged (the swap is invisible).
# ---------------------------------------------------------------------------------------------------------------------
ASH_SCORE_WEIGHTS = (0.6, 0.3, 0.15, 0.3)    # B (plates), R (front), 1 - G (cracks), height fraction
ASH_CUT_SCALE = 1.4
ASH_LUMA = (0.2126, 0.7152, 0.0722)          # linear luminance (the preview's lin_lum)
ASH_LUMA_FLOOR = 0.02
ASH_LUMA_SPAN = 0.12
ASH_INK_LUMA = 0.012
ASH_INK_KEEP = 0.9
ASH_EDGE_MIX = 0.6
ASH_MASK_INPUTS = (
    NodeInput("LP", "float3"),         # PreSkinnedPosition (object space, cm)
    NodeInput("Pw", "float3"),         # WorldPosition, camera relative
    NodeInput("Feet", "float3"),       # ActorPositionWS, camera relative
    NodeInput("Dissolve", "float"),    # MID scalar 0..1
    NodeInput("TopCm", "float"),       # MID scalar HuskTopCm
    NodeInput("TileCm", "float"),      # NoiseTileCm (60)
    NodeInput("EdgeWidth", "float"),
    NodeInput("Noise", "Texture2D"),   # T_FX_AshNoise
)
ASH_MASK_OUTPUTS = (NodeInput("Edge", "float"),)
_ASH_MASK = """
float2 nuv = float2(LP.x, LP.z) / max(TileCm, 1.0);
float2 ngx = ddx(nuv);
float2 ngy = ddy(nuv);
float4 nz = Noise.SampleGrad(NoiseSampler, float2(frac(nuv.x), frac(nuv.y)), ngx, ngy);
float hf = saturate((Pw.z - Feet.z) / max(TopCm, 1.0));
float score = nz.b * @WB@ + nz.r * @WR@ + (1.0 - nz.g) * @WG@ + hf * @WH@;
float cut = saturate(Dissolve) * @CUT@;
float on = Dissolve > 0.0 ? 1.0 : 0.0;
float keep = (on > 0.5 && score < cut) ? 0.0 : 1.0;
Edge = (on > 0.5 && keep > 0.5 && score < cut + EdgeWidth) ? 1.0 : 0.0;
return keep;
"""


def ash_mask_node() -> CustomNode:
    wb, wr, wg, wh = ASH_SCORE_WEIGHTS
    return CustomNode("AF_AshMask", "ash dissolve: plates drop below Dissolve (keep 0 / 1) and the edge band",
                      "float", ASH_MASK_INPUTS,
                      _fill(_ASH_MASK, {"WB": lit(wb), "WR": lit(wr), "WG": lit(wg), "WH": lit(wh),
                                        "CUT": lit(ASH_CUT_SCALE)}),
                      additional_outputs=ASH_MASK_OUTPUTS)


ASH_GREY_INPUTS = (
    NodeInput("Emit", "float3"),       # AF_ToonSplit's main output
    NodeInput("Lit", "float3"),        # AF_ToonSplit's Lit
    NodeInput("SunCol", "float4"),     # MPC SunColor (linear)
    NodeInput("Grey", "float"),        # MID scalar 0..1
    NodeInput("AshColor", "float3"),   # linear
    NodeInput("AshShade", "float3"),
    NodeInput("EdgeColor", "float3"),
    NodeInput("Edge", "float"),        # AF_AshMask's Edge
)
ASH_GREY_OUTPUTS = (NodeInput("AshLit", "float3"),)
_ASH_GREY = """
float3 sunC = max(float3(SunCol.r, SunCol.g, SunCol.b), float3(0.05, 0.05, 0.05));
float3 cSun = Emit + Lit * sunC;
float3 cSh = Emit;
float3 lw = @LUMA@;
float lSun = dot(cSun, lw);
float lSh = dot(cSh, lw);
float3 ashC = float3(AshColor.r, AshColor.g, AshColor.b);
float3 ashS = float3(AshShade.r, AshShade.g, AshShade.b);
float3 aSun = lerp(ashS, ashC, saturate((lSun - @FLOOR@) / @SPAN@));
float3 aSh = lerp(ashS, ashC, saturate((lSh - @FLOOR@) / @SPAN@));
aSun = lSun < @INK@ ? cSun * @INK_KEEP@ : aSun;
aSh = lSh < @INK@ ? cSh * @INK_KEEP@ : aSh;
float g = saturate(Grey);
float3 outSun = lerp(cSun, aSun, g);
float3 outSh = lerp(cSh, aSh, g);
float e = saturate(Edge) * @EDGE_MIX@;
float3 edgeC = float3(EdgeColor.r, EdgeColor.g, EdgeColor.b);
outSun = lerp(outSun, edgeC, e);
outSh = lerp(outSh, edgeC, e);
AshLit = max(outSun - outSh, float3(0.0, 0.0, 0.0)) / sunC;
return outSh;
"""


def ash_grey_node() -> CustomNode:
    return CustomNode("AF_AshGrey", "ash dissolve: the toon split greyed to the ash colours (Emit; AshLit = base colour)",
                      "float3", ASH_GREY_INPUTS,
                      _fill(_ASH_GREY, {"LUMA": vec3(ASH_LUMA), "FLOOR": lit(ASH_LUMA_FLOOR), "SPAN": lit(ASH_LUMA_SPAN),
                                        "INK": lit(ASH_INK_LUMA), "INK_KEEP": lit(ASH_INK_KEEP),
                                        "EDGE_MIX": lit(ASH_EDGE_MIX)}),
                      additional_outputs=ASH_GREY_OUTPUTS)

def all_nodes(sh: Shading) -> list[CustomNode]:
    """Every node the build creates (tests compile them all)."""
    split = toon_split_node(sh)
    nodes = [toon_node(sh), split, split.twin("Lit"), dither_node(), dither_shadow_node(), fade_opacity_node(),
             ink_node(sh), outline_wpo_node(sh), view_push_node(), select_rim_mask_node(), wind_node(), fx_mesh_node(sh),
             fx_mesh_mask_node(), ghost_node(), blob_node(), grade_node(), ao_composite_node(),
             terrain_node(True), terrain_node(False), terrain_painted_node(True), terrain_painted_node(False),
             terrain_emissive_node(), telegraph_node()]
    ash_mask, ash_grey = ash_mask_node(), ash_grey_node()
    nodes += [ash_mask, ash_mask.twin("Edge"), ash_grey, ash_grey.twin("AshLit")]
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

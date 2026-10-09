"""Colour maths: an exact port of the web ``tone()`` (src/graphics/sprites/rig/Rig.ts:144-154).

All tone maths happens on **sRGB-encoded values** (0..255), exactly like the web canvas code and like
``M_AF_Toon`` (spec art-inventory-ch1.md §1.2). Conversion to linear only happens at the very end
(render / emissive output).
"""
from __future__ import annotations

from typing import Iterable, Sequence, Tuple

RGB = Tuple[float, float, float]

# Constants of tone() (sRGB 0..255).
COOL_SHADE: RGB = (30.0, 20.0, 60.0)     # shade = mix(mix(c, COOL_SHADE, .35), black, shadowAmt)
WARM_LIGHT: RGB = (255.0, 244.0, 214.0)  # light = mix(c, WARM_LIGHT, lightAmt)
LINE_TINT: RGB = (20.0, 10.0, 30.0)      # line  = mix(mix(c, LINE_TINT, .5), black, .55)
COOL_SHADE_MIX = 0.35
LINE_TINT_MIX = 0.5
LINE_DARKEN = 0.55
DEFAULT_SHADOW = 0.42
DEFAULT_LIGHT = 0.32


def hex_to_rgb(h: str) -> RGB:
    """'#9AA6BA' / '9aa6ba' / 'rgb(1,2,3)' → (154., 166., 186.) sRGB 0..255."""
    s = h.strip()
    if s.startswith("rgb"):
        inner = s[s.index("(") + 1:s.rindex(")")]
        parts = [float(p) for p in inner.split(",")[:3]]
        return (parts[0], parts[1], parts[2])
    s = s.lstrip("#")
    if len(s) == 3:
        s = "".join(ch * 2 for ch in s)
    if len(s) != 6:
        raise ValueError(f"bad hex colour {h!r}")
    return (float(int(s[0:2], 16)), float(int(s[2:4], 16)), float(int(s[4:6], 16)))


def rgb_to_hex(c: Sequence[float]) -> str:
    return "#" + "".join(f"{int(round(max(0.0, min(255.0, v)))):02X}" for v in c[:3])


def mix(a: Sequence[float], b: Sequence[float], t: float) -> RGB:
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t)


def tone(base: str | Sequence[float], shadow: float = DEFAULT_SHADOW, light: float = DEFAULT_LIGHT) -> dict:
    """Return {'base','shade','light','line'} as sRGB 0..255 tuples (web ``tone()``)."""
    c = hex_to_rgb(base) if isinstance(base, str) else tuple(float(v) for v in base[:3])
    shade = mix(mix(c, COOL_SHADE, COOL_SHADE_MIX), (0.0, 0.0, 0.0), shadow)
    lite = mix(c, WARM_LIGHT, light)
    line = mix(mix(c, LINE_TINT, LINE_TINT_MIX), (0.0, 0.0, 0.0), LINE_DARKEN)
    return {"base": c, "shade": shade, "light": lite, "line": line}


def srgb_to_linear1(v: float) -> float:
    """Exact sRGB EOTF for one channel in 0..1."""
    return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4


def linear_to_srgb1(v: float) -> float:
    v = max(0.0, v)
    return v * 12.92 if v <= 0.0031308 else 1.055 * (v ** (1.0 / 2.4)) - 0.055


def to_linear(c255: Iterable[float]) -> RGB:
    """sRGB 0..255 → linear 0..1 (for Blender colour sockets / emission)."""
    t = tuple(srgb_to_linear1(float(v) / 255.0) for v in c255)
    return (t[0], t[1], t[2])


def hex_linear(h: str) -> RGB:
    return to_linear(hex_to_rgb(h))


def hex_linear4(h: str, a: float = 1.0) -> Tuple[float, float, float, float]:
    r, g, b = hex_linear(h)
    return (r, g, b, a)

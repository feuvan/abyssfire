"""Minimal deterministic PNG writer / reader (8-bit RGBA, stdlib only) and the content build's generated textures.

The generated textures are tiny utility images the materials need as parameter defaults before any art exists (a
texture parameter without a default does not compile), plus the soft glow sprite the VFX materials fall back to when the
art has no T_FX_Glow yet (Source/Abyssfire/Private/Vfx/AbyssVfxSystem.cpp falls back to T_FX_Glow, then to the
material default). They are written to unreal/Saved/ContentBuild/Generated and imported like any other texture.
"""
from __future__ import annotations

import math
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path

_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def _chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)


def encode_png(width: int, height: int, rgba: bytes | bytearray) -> bytes:
    """8-bit RGBA, no interlace, filter 0 on every row: byte-identical output for identical pixels."""
    if width <= 0 or height <= 0:
        raise ValueError("PNG size must be positive")
    if len(rgba) != width * height * 4:
        raise ValueError(f"expected {width * height * 4} bytes of RGBA, got {len(rgba)}")
    stride = width * 4
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        raw += rgba[y * stride:(y + 1) * stride]
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return _SIGNATURE + _chunk(b"IHDR", ihdr) + _chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + _chunk(b"IEND", b"")


def write_png(path: Path, width: int, height: int, rgba: bytes | bytearray) -> bool:
    """Writes the PNG unless an identical file exists. Returns True when the file changed."""
    data = encode_png(width, height, rgba)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_file() and path.read_bytes() == data:
        return False
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_bytes(data)
    tmp.replace(path)
    return True


@dataclass(frozen=True)
class PngInfo:
    width: int
    height: int
    bit_depth: int
    color_type: int

    @property
    def channels(self) -> int:
        return {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(self.color_type, 0)

    @property
    def has_alpha(self) -> bool:
        return self.color_type in (4, 6)


def read_png_info(path: Path) -> PngInfo:
    """Header of a PNG (size, bit depth, colour type); raises ValueError for anything that is not a PNG."""
    with open(path, "rb") as f:
        head = f.read(33)
    if len(head) < 33 or head[:8] != _SIGNATURE or head[12:16] != b"IHDR":
        raise ValueError(f"{path}: not a PNG file")
    width, height, depth, ctype = struct.unpack(">IIBB", head[16:26])
    return PngInfo(width, height, depth, ctype)


def decode_png_rgba(data: bytes) -> tuple[int, int, bytes]:
    """Decodes 8-bit RGBA / RGB / grey PNGs (non-interlaced) to RGBA bytes. Used by the tests."""
    if data[:8] != _SIGNATURE:
        raise ValueError("not a PNG")
    pos = 8
    width = height = depth = ctype = interlace = 0
    idat = bytearray()
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        kind = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
    if depth != 8 or interlace != 0 or ctype not in (0, 2, 4, 6):
        raise ValueError(f"unsupported PNG (depth {depth}, colour type {ctype}, interlace {interlace})")
    bpp = {0: 1, 2: 3, 4: 2, 6: 4}[ctype]
    raw = zlib.decompress(bytes(idat))
    stride = width * bpp
    out = bytearray()
    prev = bytearray(stride)
    p = 0
    for _ in range(height):
        ftype = raw[p]
        line = bytearray(raw[p + 1:p + 1 + stride])
        p += 1 + stride
        for i in range(stride):
            left = line[i - bpp] if i >= bpp else 0
            up = prev[i]
            ul = prev[i - bpp] if i >= bpp else 0
            if ftype == 1:
                line[i] = (line[i] + left) & 255
            elif ftype == 2:
                line[i] = (line[i] + up) & 255
            elif ftype == 3:
                line[i] = (line[i] + ((left + up) >> 1)) & 255
            elif ftype == 4:
                pa, pb, pc = abs(up - ul), abs(left - ul), abs(left + up - 2 * ul)
                pred = left if pa <= pb and pa <= pc else (up if pb <= pc else ul)
                line[i] = (line[i] + pred) & 255
        prev = line
        for x in range(width):
            px = line[x * bpp:(x + 1) * bpp]
            if ctype == 0:
                out += bytes((px[0], px[0], px[0], 255))
            elif ctype == 2:
                out += bytes((px[0], px[1], px[2], 255))
            elif ctype == 4:
                out += bytes((px[0], px[0], px[0], px[1]))
            else:
                out += bytes(px)
    return width, height, bytes(out)


# ---------------------------------------------------------------------------------------------------------------------
# Generated textures
# ---------------------------------------------------------------------------------------------------------------------
# Web light / glow falloff (world-map-nav.md 14.1 "falloff stops 1, .9@.15, .6@.4, .25@.7, 0@1"; manifest fx.visorGlow).
GLOW_STOPS: tuple[tuple[float, float], ...] = ((0.0, 1.0), (0.15, 0.9), (0.4, 0.6), (0.7, 0.25), (1.0, 0.0))


def glow_falloff(r: float) -> float:
    """Piecewise-linear web glow falloff at normalised radius r (0 centre, 1 edge)."""
    if r <= 0.0:
        return GLOW_STOPS[0][1]
    for (r0, a0), (r1, a1) in zip(GLOW_STOPS, GLOW_STOPS[1:]):
        if r <= r1:
            return a0 + (a1 - a0) * (r - r0) / (r1 - r0)
    return 0.0


def solid_rgba(width: int, height: int, rgba: tuple[int, int, int, int]) -> bytes:
    return bytes(rgba) * (width * height)


def glow_rgba(size: int) -> bytes:
    """White RGB, alpha = web glow falloff (the additive FX material multiplies rgb by alpha)."""
    out = bytearray()
    half = size / 2.0
    for y in range(size):
        for x in range(size):
            r = math.hypot(x + 0.5 - half, y + 0.5 - half) / half
            a = max(0, min(255, int(math.floor(glow_falloff(r) * 255.0 + 0.5))))
            out += bytes((255, 255, 255, a))
    return bytes(out)


@dataclass(frozen=True)
class GeneratedTexture:
    name: str            # UE asset name
    folder: str          # content folder
    file_name: str
    width: int
    height: int
    rgba: bytes
    srgb: bool
    kind: str            # "palette_bc" | "palette_p" | "data" | "fx" (texture settings in ue/textures.py)


def default_textures(default_folder: str, fx_folder: str, shadow_amt: float = 0.42, light_amt: float = 0.32
                     ) -> list[GeneratedTexture]:
    """The generated textures, from the paths constants (no unreal import)."""
    from . import paths  # local import keeps this module free of package-level state for the tests

    def byte(v: float) -> int:
        return max(0, min(255, int(math.floor(v * 255.0 + 0.5))))

    return [
        # Base colour default of every palette parameter (sRGB, sampled as Color like the real _BC atlases).
        GeneratedTexture(paths.T_DEFAULT_WHITE, default_folder, f"{paths.T_DEFAULT_WHITE}.png", 4, 4,
                         solid_rgba(4, 4, (255, 255, 255, 255)), True, "palette_bc"),
        # Palette params default: shadowAmt .42, lightAmt .32, emissive 0, outline mix 1 (art-inventory-ch1.md 1.2).
        GeneratedTexture(paths.T_DEFAULT_PALETTE_P, default_folder, f"{paths.T_DEFAULT_PALETTE_P}.png", 4, 4,
                         solid_rgba(4, 4, (byte(shadow_amt), byte(light_amt), 0, 255)), False, "palette_p"),
        # Terrain TileIds default (WorldContract 3.1): paint 0 grass (R), variant 0 (G), walkable (B), raw 0 (A).
        GeneratedTexture(paths.T_DEFAULT_TILE_IDS, default_folder, f"{paths.T_DEFAULT_TILE_IDS}.png", 4, 4,
                         solid_rgba(4, 4, (0, 0, 255, 0)), False, "data"),
        # Soft round glow (64^2, the web Glow texture size, art-inventory-ch1.md 8.4).
        GeneratedTexture(paths.T_DEFAULT_GLOW, fx_folder, f"{paths.T_DEFAULT_GLOW}.png", 64, 64,
                         glow_rgba(64), False, "fx"),
    ]

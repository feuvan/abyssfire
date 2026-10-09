#!/usr/bin/env python3
"""Installer artwork for the Abyssfire packages (CI job `installer-assets`, unreal/Docs/CI.md "Icons and artwork").

    python3 unreal/CI/installers/assets/make_installer_assets.py --out unreal/CI/out/installer-assets
            [--source PNG] [--version 1.2.3] [--stdlib]

Writes into --out:

    app-icon-1024.png         master app icon (macOS-style rounded square)
    Abyssfire.ico             Windows icon, 16..256 px (NSIS / MSI / shortcuts / UE Build/Windows/Application.ico)
    Abyssfire.icns            macOS icon (DMG volume icon, .app icon when the project has none)
    dmg-background.png        660x420 DMG window background (+ dmg-background@2x.png for Retina)
    nsis-welcome.bmp          164x314 MUI2 welcome / finish page bitmap (24-bit BMP)
    nsis-header.bmp           150x57  MUI2 header bitmap
    ue/Windows/Application.ico                    staged into unreal/Build/ when the project commits none
    ue/Android/res/drawable*/icon.png             (same)
    source.txt                which picture the icon was derived from

Icon source, first match wins: --source; unreal/Art/Export/Icons/T_UI_AppIcon.png; any
unreal/Art/Export/Icons/*AppIcon*.png; unreal/Art/Export/Portraits/T_UI_Portrait_Hero_Warrior.png; any
T_UI_Portrait_*.png; otherwise a procedural ember emblem. Text uses the project's OFL fonts (unreal/Fonts).

Pillow is used when installed (pip install pillow); without it (or with --stdlib) a pure standard-library path
draws the procedural emblem and an untitled background, so the macOS runner can still produce every file.
"""
from __future__ import annotations

import argparse
import math
import struct
import sys
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
UNREAL = HERE.parents[2]
FONTS = UNREAL / "Fonts"
EXPORT = UNREAL / "Art" / "Export"

BG_TOP = (18, 12, 30)
BG_BOTTOM = (46, 18, 40)
EMBER = (255, 138, 40)
EMBER_HOT = (255, 214, 120)
GOLD = (214, 170, 82)

DMG_W, DMG_H = 660, 420
DMG_APP_X, DMG_APPS_X, DMG_ICON_Y = 170, 490, 205  # must match installers/mac/dmg_style.py defaults

ANDROID_ICONS = {"drawable": 128, "drawable-ldpi": 36, "drawable-mdpi": 48, "drawable-hdpi": 72,
                 "drawable-xhdpi": 96, "drawable-xxhdpi": 144, "drawable-xxxhdpi": 192}
ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]


def find_source(explicit: str | None) -> Path | None:
    if explicit:
        p = Path(explicit)
        if not p.is_file():
            sys.exit(f"--source {p} does not exist")
        return p
    icons = EXPORT / "Icons"
    portraits = EXPORT / "Portraits"
    candidates = [icons / "T_UI_AppIcon.png", *sorted(icons.glob("*AppIcon*.png")),
                  portraits / "T_UI_Portrait_Hero_Warrior.png", *sorted(portraits.glob("T_UI_Portrait_*.png"))]
    return next((c for c in candidates if c.is_file()), None)


# ── procedural geometry (shared by both paths) ────────────────────────────────────────────────────────────
def smoothstep(e0: float, e1: float, x: float) -> float:
    t = min(1.0, max(0.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


def mix(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(len(a)))


def rounded_rect_sdf(x: float, y: float, half: float, radius: float) -> float:
    qx, qy = abs(x) - half + radius, abs(y) - half + radius
    return math.hypot(max(qx, 0.0), max(qy, 0.0)) + min(max(qx, qy), 0.0) - radius


def flame_sdf(x: float, y: float, scale: float) -> float:
    """Teardrop flame in [-1, 1] coordinates (y down): a disc below, tapering to a tip above."""
    x, y = x / scale, y / scale
    disc = math.hypot(x, y - 0.28) - 0.42
    # tip: distance to the segment from the disc's top to (0, -0.78), widening downwards
    t = min(1.0, max(0.0, (y + 0.78) / 1.06))
    width = 0.42 * t ** 1.4
    tip = max(abs(x) - width, -0.78 - y, y - 0.28)
    k = 0.12  # smooth union
    h = max(k - abs(disc - tip), 0.0) / k
    return (min(disc, tip) - h * h * k * 0.25) * scale


def emblem_rgba(u: float, v: float) -> tuple[float, float, float, float]:
    """Procedural app icon at normalised coords u, v in [0, 1]: rounded square, glow, ember flame."""
    x, y = u * 2 - 1, v * 2 - 1
    d = rounded_rect_sdf(x, y, 0.82, 0.36)
    if d > 0.01:
        return (0.0, 0.0, 0.0, 0.0)
    alpha = 1.0 - smoothstep(-0.01, 0.01, d)
    col = mix(BG_TOP, BG_BOTTOM, v)
    glow = math.exp(-((x * x) * 2.2 + ((y - 0.25) ** 2) * 2.0))
    col = mix(col, (120, 40, 20), 0.55 * glow)
    ring = smoothstep(0.035, 0.0, abs(d + 0.045))
    col = mix(col, GOLD, 0.85 * ring)
    outer = flame_sdf(x, y + 0.02, 0.62)
    inner = flame_sdf(x, y - 0.12, 0.36)
    col = mix(col, EMBER, 1.0 - smoothstep(-0.01, 0.01, outer))
    col = mix(col, EMBER_HOT, 1.0 - smoothstep(-0.01, 0.01, inner))
    return (col[0], col[1], col[2], 255.0 * alpha)


def dmg_rgb(px: float, py: float, w: int, h: int) -> tuple[float, float, float]:
    """DMG background without text: vertical gradient, ember glow under the icons, an arrow between them."""
    sx, sy = px / w * DMG_W, py / h * DMG_H
    col = mix(BG_TOP, BG_BOTTOM, sy / DMG_H)
    glow = math.exp(-(((sx - DMG_W / 2) / 260) ** 2 + ((sy - DMG_H) / 140) ** 2))
    col = mix(col, (150, 52, 18), 0.6 * glow)
    # arrow: shaft + head between the two icon slots
    ax0, ax1, ay = DMG_APP_X + 86, DMG_APPS_X - 86, DMG_ICON_Y
    head = 22.0
    in_shaft = ax0 <= sx <= ax1 - head and abs(sy - ay) <= 3.0
    hx = sx - (ax1 - head)
    in_head = 0 <= hx <= head and abs(sy - ay) <= (head - hx) * 0.75
    if in_shaft or in_head:
        col = mix(col, EMBER, 0.9)
    return col


# ── standard-library writers ──────────────────────────────────────────────────────────────────────────────
def png_bytes(width: int, height: int, rows: list[bytes], rgba: bool) -> bytes:
    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + r for r in rows)
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6 if rgba else 2, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def render_rows(width: int, height: int, fn, rgba: bool) -> list[bytes]:
    rows = []
    for j in range(height):
        row = bytearray()
        for i in range(width):
            c = fn((i + 0.5) / width, (j + 0.5) / height)
            row += bytes(max(0, min(255, int(round(ch)))) for ch in (c if rgba else c[:3]))
        rows.append(bytes(row))
    return rows


def bmp_bytes(width: int, height: int, rows_rgb: list[bytes]) -> bytes:
    stride = (width * 3 + 3) & ~3
    pixels = bytearray()
    for r in reversed(rows_rgb):  # bottom-up
        line = bytearray()
        for i in range(width):
            red, green, blue = r[i * 3], r[i * 3 + 1], r[i * 3 + 2]
            line += bytes((blue, green, red))
        line += b"\x00" * (stride - len(line))
        pixels += line
    header = struct.pack("<2sIHHI", b"BM", 54 + len(pixels), 0, 0, 54)
    info = struct.pack("<IiiHHIIiiII", 40, width, height, 1, 24, 0, len(pixels), 2835, 2835, 0, 0)
    return header + info + bytes(pixels)


def ico_bytes(pngs: dict[int, bytes]) -> bytes:
    sizes = sorted(pngs)
    out = struct.pack("<HHH", 0, 1, len(sizes))
    offset = 6 + 16 * len(sizes)
    body = b""
    for s in sizes:
        data = pngs[s]
        out += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
        body += data
    return out + body


def icns_bytes(pngs: dict[int, bytes]) -> bytes:
    types = {16: b"icp4", 32: b"icp5", 64: b"icp6", 128: b"ic07", 256: b"ic08", 512: b"ic09", 1024: b"ic10"}
    body = b"".join(types[s] + struct.pack(">I", 8 + len(pngs[s])) + pngs[s] for s in sorted(pngs) if s in types)
    return b"icns" + struct.pack(">I", 8 + len(body)) + body


def build_stdlib(out: Path, version: str) -> str:
    pngs = {}
    for s in sorted(set(ICO_SIZES + [512, 1024] + list(ANDROID_ICONS.values()))):
        pngs[s] = png_bytes(s, s, render_rows(s, s, emblem_rgba, True), True)
    (out / "app-icon-1024.png").write_bytes(pngs[1024])
    (out / "Abyssfire.ico").write_bytes(ico_bytes({s: pngs[s] for s in ICO_SIZES}))
    (out / "Abyssfire.icns").write_bytes(icns_bytes({s: pngs[s] for s in (16, 32, 64, 128, 256, 512, 1024)}))
    for scale, name in ((1, "dmg-background.png"), (2, "dmg-background@2x.png")):
        w, h = DMG_W * scale, DMG_H * scale
        rows = render_rows(w, h, lambda u, v: dmg_rgb(u * w, v * h, w, h), False)
        (out / name).write_bytes(png_bytes(w, h, rows, False))
    for name, (w, h) in (("nsis-welcome.bmp", (164, 314)), ("nsis-header.bmp", (150, 57))):
        def side(u, v, w=w, h=h):
            c = mix(BG_TOP, BG_BOTTOM, v)
            e = emblem_rgba((u - 0.5) * w / min(w, h) * 1.25 + 0.5, (v - (0.3 if h > w else 0.5)) * h / min(w, h) * 1.25 + 0.5)
            return mix(c, e[:3], e[3] / 255.0)
        (out / name).write_bytes(bmp_bytes(w, h, render_rows(w, h, side, False)))
    write_ue_icons(out, lambda s: pngs[s], (out / "Abyssfire.ico").read_bytes())
    return "procedural emblem (standard-library renderer)"


def write_ue_icons(out: Path, png_for_size, ico: bytes) -> None:
    (out / "ue" / "Windows").mkdir(parents=True, exist_ok=True)
    (out / "ue" / "Windows" / "Application.ico").write_bytes(ico)
    for folder, size in ANDROID_ICONS.items():
        d = out / "ue" / "Android" / "res" / folder
        d.mkdir(parents=True, exist_ok=True)
        (d / "icon.png").write_bytes(png_for_size(size))


# ── Pillow path ───────────────────────────────────────────────────────────────────────────────────────────
def build_pillow(out: Path, source: Path | None, version: str) -> str:
    from io import BytesIO

    from PIL import Image, ImageDraw, ImageFilter, ImageFont

    S = 1024

    def emblem_image(size: int) -> "Image.Image":
        return Image.frombytes("RGBA", (size, size), b"".join(render_rows(size, size, emblem_rgba, True)))

    def rounded_mask(size: int, inset: float, radius: float) -> "Image.Image":
        m = Image.new("L", (size * 4, size * 4), 0)
        ImageDraw.Draw(m).rounded_rectangle([inset * 4, inset * 4, (size - inset) * 4, (size - inset) * 4],
                                            radius=radius * 4, fill=255)
        return m.resize((size, size), Image.Resampling.LANCZOS)

    if source is None:
        icon = emblem_image(256).resize((S, S), Image.Resampling.LANCZOS)
        what = "procedural emblem"
    else:
        art = Image.open(source).convert("RGBA")
        bbox = art.getchannel("A").getbbox() or (0, 0, *art.size)
        art = art.crop(bbox)
        # macOS icon grid: 824 px body on a 1024 canvas, corner radius ~185
        inset, radius = 100, 185
        body = S - 2 * inset
        base = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        bg = Image.new("RGBA", (S, S))
        grad = Image.linear_gradient("L").resize((S, S))
        bg = Image.composite(Image.new("RGBA", (S, S), BG_BOTTOM + (255,)), Image.new("RGBA", (S, S), BG_TOP + (255,)), grad)
        glow = Image.new("L", (S, S), 0)
        ImageDraw.Draw(glow).ellipse([S * 0.18, S * 0.42, S * 0.82, S * 1.02], fill=200)
        glow = glow.filter(ImageFilter.GaussianBlur(S * 0.09))
        bg = Image.composite(Image.new("RGBA", (S, S), (150, 52, 18, 255)), bg, glow)
        # the portrait fills the body width, anchored to the bottom (the character rises out of the frame)
        scale = body * 1.08 / max(art.size)
        art = art.resize((max(1, int(art.width * scale)), max(1, int(art.height * scale))), Image.Resampling.LANCZOS)
        layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        layer.alpha_composite(art, ((S - art.width) // 2, S - inset - art.height + int(body * 0.02)))
        bg.alpha_composite(layer)
        mask = rounded_mask(S, inset, radius)
        base.paste(bg, (0, 0), mask)
        ring = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        ImageDraw.Draw(ring).rounded_rectangle([inset + 6, inset + 6, S - inset - 6, S - inset - 6], radius=radius - 6,
                                               outline=GOLD + (255,), width=14)
        base.alpha_composite(ring)
        shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        shadow.putalpha(rounded_mask(S, inset, radius).filter(ImageFilter.GaussianBlur(18)).point(lambda a: a * 0.45))
        icon = Image.alpha_composite(shadow, base)
        what = str(source.relative_to(UNREAL.parent)) if source.is_relative_to(UNREAL.parent) else str(source)

    icon.save(out / "app-icon-1024.png", optimize=True)
    icon.save(out / "Abyssfire.ico", sizes=[(s, s) for s in ICO_SIZES])
    icon.save(out / "Abyssfire.icns")

    def font(name: str, size: int):
        p = FONTS / name
        return ImageFont.truetype(str(p), size) if p.is_file() else ImageFont.load_default()

    def glyph_pixels(f, ch: str) -> bytes:
        img = Image.new("L", (96, 96), 0)
        ImageDraw.Draw(img).text((8, 8), ch, font=f, fill=255)
        return img.tobytes()

    def has_glyphs(f, text: str) -> bool:
        # A missing glyph renders as .notdef: compare with a code point no font has.
        try:
            notdef = glyph_pixels(f, "\U0010FFFD")
            return all(glyph_pixels(f, ch) != notdef for ch in text if not ch.isspace())
        except Exception:  # noqa: BLE001 - bitmap default font
            return False

    for scale, name in ((1, "dmg-background.png"), (2, "dmg-background@2x.png")):
        w, h = DMG_W * scale, DMG_H * scale
        img = Image.frombytes("RGB", (w, h), b"".join(render_rows(w, h, lambda u, v: dmg_rgb(u * w, v * h, w, h), False)))
        d = ImageDraw.Draw(img)
        title = font("Cinzel-Bold.ttf", 34 * scale)
        cjk = font("NotoSerifSC-Bold.otf", 30 * scale)
        small = font("NotoSansSC-Regular.otf", 13 * scale)
        latin = font("Cinzel-Regular.ttf", 12 * scale)
        t1, t2 = "ABYSSFIRE", "渊火"
        w1 = d.textlength(t1, font=title)
        w2 = d.textlength(t2, font=cjk) if has_glyphs(cjk, t2) else 0
        x = (w - (w1 + (18 * scale if w2 else 0) + w2)) / 2
        d.text((x, 34 * scale), t1, font=title, fill=EMBER_HOT)
        if w2:
            d.text((x + w1 + 18 * scale, 36 * scale), t2, font=cjk, fill=EMBER)
        line_zh = "将渊火拖入应用程序文件夹完成安装"
        line_en = "Drag Abyssfire to Applications to install"
        y = (DMG_ICON_Y + 110) * scale
        if has_glyphs(small, line_zh):
            d.text((w / 2, y), line_zh, font=small, fill=(232, 220, 205), anchor="ma")
            y += 22 * scale
        d.text((w / 2, y), line_en, font=latin, fill=(200, 186, 170), anchor="ma")
        if version:
            d.text((w - 14 * scale, h - 12 * scale), f"v{version}", font=latin, fill=(150, 130, 120), anchor="rd")
        img.save(out / name, optimize=True)

    for name, (w, h) in (("nsis-welcome.bmp", (164, 314)), ("nsis-header.bmp", (150, 57))):
        grad = Image.linear_gradient("L").resize((w, h))
        img = Image.composite(Image.new("RGB", (w, h), BG_BOTTOM), Image.new("RGB", (w, h), BG_TOP), grad)
        side = min(w, h - 8) if h < w else int(w * 0.86)
        ic = icon.resize((side, side), Image.Resampling.LANCZOS)
        pos = ((w - side) // 2, int(h * 0.12)) if h > w else (w - side - 4, (h - side) // 2)
        img.paste(ic, pos, ic)
        if h > w:
            d = ImageDraw.Draw(img)
            d.text((w / 2, int(h * 0.12) + side + 14), "ABYSSFIRE", font=font("Cinzel-Bold.ttf", 17), fill=EMBER_HOT, anchor="ma")
            zh = font("NotoSerifSC-Bold.otf", 22)
            if has_glyphs(zh, "渊火"):
                d.text((w / 2, int(h * 0.12) + side + 40), "渊火", font=zh, fill=EMBER, anchor="ma")
        img.save(out / name, format="BMP")

    def png_for_size(s: int) -> bytes:
        b = BytesIO()
        icon.resize((s, s), Image.Resampling.LANCZOS).save(b, format="PNG", optimize=True)
        return b.getvalue()

    write_ue_icons(out, png_for_size, (out / "Abyssfire.ico").read_bytes())
    return what


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", required=True)
    ap.add_argument("--source", help="PNG to derive the app icon from")
    ap.add_argument("--version", default="", help="shown in a corner of the DMG background")
    ap.add_argument("--stdlib", action="store_true", help="never use Pillow (procedural artwork)")
    args = ap.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    source = find_source(args.source)
    use_pillow = not args.stdlib
    if use_pillow:
        try:
            import PIL  # noqa: F401
        except ImportError:
            print("Pillow not installed: using the standard-library renderer", file=sys.stderr)
            use_pillow = False
    what = build_pillow(out, source, args.version) if use_pillow else build_stdlib(out, args.version)
    (out / "source.txt").write_text(what + "\n", encoding="utf-8")
    for p in sorted(out.rglob("*")):
        if p.is_file():
            print(f"{p.stat().st_size:>9}  {p.relative_to(out)}")
    print(f"icon source: {what}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

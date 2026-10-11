"""Toon VFX textures ``T_FX_<Sprite>`` (spec art-inventory-ch1.md §8.4, WorldContract 5.4) — the web painters of
``src/graphics/vfx/FxTextures.ts`` (+ the camp flame of ``CampProps.ts``) redrawn as cel-shaded sprites.

    /opt/venvs/blender/bin/python unreal/Art/blender/vfx/fx_textures.py [--sheet-only] [names…]

Writes ``Art/Export/VFX/T_FX_<Name>.png`` (RGBA8, **linear** values — `build_content.py` imports `T_FX_*` with sRGB
off) and ``Art/Previews/vfx_textures/*.png`` (every sprite at 1:1 over the dark studio and the plains grass, tinted
with the element palettes the recipes use, as the UE sprite materials draw them).

Style (docs/art-direction.md, spec §1.1): the web textures are soft canvas gradients; here every sprite is built
from distance fields with **cel steps** — a crisp silhouette, 2–3 value bands (hot core / body / soft trailing
band), one soft halo only where the shape emits light — so additive FX stay graphic over the bright plains instead
of washing into blurs. Light comes from the upper left on the solid sprites (smoke, drops, puddles, shards).

Material contract (`Scripts/abyss_content/hlsl.py` AF_FxAdd / AF_FxTrans): additive = tex.rgb·tint·tex.a·α,
translucent = colour tex.rgb·tint, opacity tex.a·α. White-ish RGB = tintable; pre-coloured cells (flames, frost
shard, arrow, coin, rock, chain) store the linear colour and are drawn with a white tint. Strips: N equal cells side
by side, the instance Variant picks one (`SPRITES[name]['cells']` → `vfx_recipes.json` `sprites`).
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))

from kit import paths, pngio  # noqa: E402
import fxpaint as P  # noqa: E402
from fxpaint import Canvas  # noqa: E402

OUT = paths.export_root() / "VFX"
PREVIEW = paths.preview_root() / "vfx_textures"
WHITE = np.ones(3, np.float32)

# Flame strip cells (rim, mid, core) — web FxTextures.ts:241-247 + the camp flame (CampProps.ts drawFlame, plains
# #FF8800: outer 25 % → #B01810, mid 55 % → #FFB030, inner 85 % → #FFF0A0, line 50 % → #400808).
FLAME_CELLS = [
    ("fire", ("#E8401A", "#FF9A2A", "#FFF0B0")),
    ("frost", ("#2A86D8", "#8EE6FF", "#FFFFFF")),
    ("poison", ("#2E9A2A", "#8EE83A", "#EAFFB8")),
    ("shadow", ("#5A1A9A", "#A05CF0", "#F0D8FF")),
    ("holy", ("#F0A020", "#FFE070", "#FFFBE8")),
    ("rage", ("#B01818", "#FF4A2A", "#FFC8A0")),
    ("plain", ("#B8B8B8", "#E8E8E8", "#FFFFFF")),
    ("camp", None),
]
SLASH_CELLS = ["warrior", "rogue", "mage", "heavy"]
IMPACT_CELLS = ["normal", "heavy", "crit", "kill"]


# ── shared painters ────────────────────────────────────────────────────────────────────────────────────
def radial(cv: Canvas, table, r: float | None = None) -> np.ndarray:
    r = r or min(cv.w, cv.h) / 2
    d = np.hypot(cv.x - cv.w / 2, cv.y - cv.h / 2) / r
    return P.stops(d, table)


def glow_under(cv: Canvas, d, width: float, alpha: float, color=WHITE, power: float = 1.7) -> None:
    """Soft light halo outside the field ``d`` (the web ``shadowBlur``), painted under the crisp layers."""
    cv.under(color, P.halo(d, width, power) * alpha * (d > -0.5))


def flame_path(cx: float, by: float, w: float, h: float, sway: float, n: int = 14):
    """Web ``flameShape`` (CampProps.ts:20-27): round base at ``by``, tip swaying by ``sway``."""
    p = []
    p += P.bezier((cx - w / 2, by - h * .18), (cx - w / 2, by - h * .55), (cx + sway * .4 - w * .25, by - h * .7),
                  (cx + sway, by - h), n)
    p += P.bezier((cx + sway, by - h), (cx + sway * .4 + w * .3, by - h * .62), (cx + w / 2, by - h * .5),
                  (cx + w / 2, by - h * .18), n)[1:]
    p += P.bezier((cx + w / 2, by - h * .18), (cx + w / 2, by + h * .04), (cx - w / 2, by + h * .04),
                  (cx - w / 2, by - h * .18), n)[1:-1]
    return p


def tongue_path(cx: float, base: float, bw: float, top: float):
    """Web particle flame (FxTextures.ts flame()): tip at ``top``, round base of half-width ``bw`` whose centre is
    at ``base − 0.55 bw``, a hooked left flank."""
    H = base - top
    p = P.bezier((cx + bw * .1, top), (cx + bw * .35, top + H * .3), (cx + bw * 1.1, top + H * .5),
                 (cx + bw, base - bw * .55), 12)
    cy = base - bw * .55
    for i in range(1, 13):
        a = math.pi * i / 12          # 0 → π along the lower half (canvas arc, y down)
        p.append((cx + math.cos(a) * bw, cy + math.sin(a) * bw))
    p += P.bezier((cx - bw, cy), (cx - bw * 1.05, top + H * .45), (cx - bw * .2, top + H * .35),
                  (cx - bw * .05, top + H * .18), 10)[1:]
    p += P.bezier((cx - bw * .05, top + H * .18), (cx - bw * .05, top + H * .06), (cx + bw * .1, top), None, 6)[1:-1]
    return p


# ── sprites ────────────────────────────────────────────────────────────────────────────────────────────
def paint_glow():
    cv = Canvas(128, 128)
    a = radial(cv, [(0, 1), (.12, .96), (.2, .78), (.45, .3), (.75, .08), (1, 0)])
    cv.over(WHITE, a)
    return cv.result()


def paint_core():
    cv = Canvas(64, 64)
    d = P.circle(cv, 32, 32, 13.0)
    glow_under(cv, d, 10.0, 0.4)
    cv.over(WHITE, cv.cov(d, 1.2))
    cv.over(WHITE, radial(cv, [(0, 1), (.38, .95), (.62, .35), (1, 0)]) * 0.5)
    return cv.result()


def paint_lightpool():
    cv = Canvas(128, 128)
    cv.over(WHITE, radial(cv, [(0, 1), (.15, .9), (.4, .6), (.7, .25), (1, 0)]))
    return cv.result()


def paint_spark():
    """Four-point glint: a soft halo star, a crisp concave star and a white-hot round core (web star4)."""
    cv = Canvas(128, 128)
    c = 64
    outer = P.star(cv, c, c, 62, 15, 4, curve=0.32)
    inner = P.star(cv, c, c, 50, 11, 4, curve=0.36)
    cv.over(WHITE, radial(cv, [(0, .45), (.3, .18), (.6, .05), (1, 0)]))
    cv.over(WHITE * 0.85, cv.cov(outer, 0.8) * 0.55)
    cv.over(WHITE, cv.cov(inner))
    cv.over(WHITE, cv.cov(P.circle(cv, c, c, 7.5)))
    return cv.result()


def paint_star():
    """Five-point cartoon star (stun stars / sparkle motes fallback): crisp body, brighter inset, halo."""
    cv = Canvas(64, 64)
    body = P.star(cv, 32, 33, 27, 12, 5)
    body = P.smooth_union(body, body, 2.0) + 0.0
    glow_under(cv, body, 6.0, 0.35)
    cv.over(WHITE * 0.82, cv.cov(body))
    cv.over(WHITE, cv.cov(P.star(cv, 30.5, 31, 17, 7.6, 5)))
    return cv.result()


def paint_streak():
    """Velocity streak, head at the top (quad +Y = velocity): crisp tapered body, bright head, hot core line."""
    W, H = 32, 128
    cv = Canvas(W, H)
    c = W / 2
    pts_r = P.bezier((c, H - 2), (c + 3, H * .6), (c + 12, H * .2), (c, 3), 18)
    pts_l = [(2 * c - x, y) for x, y in reversed(pts_r)]
    body = P.polygon(cv, pts_r + pts_l[1:-1])
    t = 1.0 - cv.y / H                     # 0 tail → 1 head
    ramp = P.steps(t, [0.0, 0.35, 0.7, 1.0], [0.12, 0.42, 0.72], 0.06)
    glow_under(cv, body, 3.0, 0.35)
    cv.over(WHITE, cv.cov(body) * ramp)
    core = P.stroke(cv, [(c, H * .38), (c, 6)], [1.6, 3.4])
    cv.over(WHITE, cv.cov(core))
    return cv.result()


def paint_comet():
    """Comet: round white-hot head at the top, two-band tapered tail fading down (velocity orient)."""
    W, H = 64, 192
    cv = Canvas(W, H)
    c, hy, hr = W / 2, 30.0, 25.0
    tail = P.polygon(cv, P.bezier((c - hr, hy), (c - hr * .9, H * .45), (c - 4, H * .8), (c, H - 2), 16)
                     + P.bezier((c, H - 2), (c + 4, H * .8), (c + hr * .9, H * .45), (c + hr, hy), 16)[1:])
    head = P.circle(cv, c, hy, hr)
    shape = P.union(tail, head)
    t = np.clip((cv.y - hy) / (H - hy), 0, 1)
    tail_a = P.steps(1 - t, [0.0, 0.3, 0.6, 0.9], [0.18, 0.5, 0.82], 0.05)
    glow_under(cv, shape, 6.0, 0.3)
    cv.over(WHITE * 0.85, cv.cov(shape) * np.where(cv.y < hy, 1.0, tail_a))
    inner = P.union(P.circle(cv, c, hy, hr * 0.62),
                    P.polygon(cv, [(c - hr * .5, hy), (c, H * .62), (c + hr * .5, hy)]))
    cv.over(WHITE, cv.cov(inner) * np.where(cv.y < hy, 1.0, np.clip(1.2 - t * 1.8, 0, 1)))
    return cv.result()


def paint_ember():
    W, H = 32, 64
    cv = Canvas(W, H)
    c = W / 2
    pts = P.bezier((c, H * .06), (c + W * .36, H * .4), (c + W * .32, H * .82), (c, H * .92), 14)
    pts += [(2 * c - x, y) for x, y in reversed(pts)][1:-1]
    body = P.polygon(cv, pts)
    glow_under(cv, body, 6.0, 0.55)
    cv.over(WHITE * 0.8, cv.cov(body) * 0.85)
    cv.over(WHITE, cv.cov(P.ellipse(cv, c, H * .64, W * .17, H * .2)))
    return cv.result()


def paint_drop():
    """Liquid droplet (tint blood / venom): body, lower-right shade, rim line, top-left highlight."""
    W, H = 32, 48
    cv = Canvas(W, H)
    c, r = W / 2, W * .36
    cy = H - r - H * .06
    pts = P.bezier((c, H * .04), (c + r * .3, cy - r * 1.1), (c + r, cy - r * .6), (c + r, cy), 12)
    for i in range(1, 12):
        a = math.pi * i / 12
        pts.append((c + math.cos(a) * r, cy + math.sin(a) * r))
    pts += P.bezier((c - r, cy), (c - r, cy - r * .6), (c - r * .3, cy - r * 1.1), (c, H * .04), 12)[1:-1]
    body = P.polygon(cv, pts)
    inside = cv.cov(body)
    cv.over(WHITE * 0.62, inside)
    shade = P.intersect(body + 1.6, P.circle(cv, c + r * .6, cy + r * .55, r * 1.05))
    cv.over(WHITE * 0.33, cv.cov(shade))
    rim = np.abs(body + 0.9) - 0.9
    cv.over(WHITE * 0.22, cv.cov(rim))
    cv.over(WHITE, cv.cov(P.ellipse(cv, c - r * .38, cy - r * .3, r * .2, r * .32, -0.4)))
    return cv.result()


def puff_lobes(cx, cy, R, seed, lobes=6, flat=1.0):
    """Cartoon cloud: a core disc and ``lobes`` round bumps on its rim (deterministic)."""
    r = P.rng(seed)
    out = [(cx, cy + R * 0.08, R * 0.78)]
    for i in range(lobes):
        a = (i / lobes) * math.pi * 2 + r() * 0.6 - math.pi / 2
        rr = R * (0.46 + r() * 0.2)
        out.append((cx + math.cos(a) * R * 0.62, cy + math.sin(a) * R * 0.56 * flat, rr))
    return out


def cloud(cv: Canvas, lobes, rim_soft=5.0, light=0.95, base=0.64, shade=0.36, alpha=0.92):
    """Paint lobes as a toon cloud: soft feather rim, mid body, a lit crescent on the upper-left of every bump
    (the overlapping-balls read of a cartoon puff) and one shade crescent along the lower-right silhouette."""
    circles = [P.circle(cv, x, y, rr) for x, y, rr in lobes]
    body = P.union(*circles)
    R = max(rr for _, _, rr in lobes)
    cv.over(WHITE * base, cv.cov(body, rim_soft) * 0.5)
    cv.over(WHITE * base, cv.cov(body) * alpha)
    sx, sy = R * 0.22, R * 0.26
    shifted = P.union(*[P.circle(cv, x - sx, y - sy, rr) for x, y, rr in lobes])
    cv.over(WHITE * shade, cv.cov(P.subtract(body + 1.0, shifted)) * alpha)
    lit = []
    for (x, y, rr), f in zip(lobes, circles):
        lit.append(P.subtract(f + 1.5, P.circle(cv, x + rr * 0.3, y + rr * 0.36, rr * 0.98)))
    lit_f = P.intersect(P.union(*lit), body + 1.5)
    cv.over(WHITE * light, cv.cov(lit_f) * alpha)


def paint_puff_cell(seed: int, W=128):
    """Cartoon smoke puff (web smokePuff, toon): lumpy lobes, lit bump crescents, lower-right shade, soft rim."""
    cv = Canvas(W, W)
    cloud(cv, puff_lobes(W / 2, W / 2, W * 0.3, seed))
    return cv.result()


def paint_smoke():
    return P.strip([paint_puff_cell(7), paint_puff_cell(23)])


def paint_dust():
    """Kicked-up ground dust (charge / dodge / whirlwind edge): a low, wide cloud of bumps on a flat base."""
    W, H = 128, 128
    cv = Canvas(W, H)
    lobes = [(64, 88, 26), (36, 86, 19), (92, 88, 20), (52, 68, 20), (78, 70, 18), (112, 94, 12), (16, 96, 11),
             (64, 58, 13)]
    cv2 = Canvas(W, H)
    cloud(cv2, lobes, light=0.96, base=0.68, shade=0.42)
    img = cv2.result()
    base = np.clip((106.0 - (np.arange(H)[:, None] + 0.5)) / 1.0 + 0.5, 0, 1)   # flat ground line
    img[..., 3] *= base
    return img


def paint_puff():
    cv = Canvas(64, 64)
    cv.over(WHITE, radial(cv, [(0, .75), (.35, .62), (.55, .4), (.8, .12), (1, 0)]))
    return cv.result()


def flame_cell(pal) -> np.ndarray:
    """One flame tongue (64×128 cell, base on the bottom edge = the upright quad's anchor): rim layer with a soft
    glow, mid and core tongues, hard cel edges (web flame(); pre-coloured, linear)."""
    W, H = 64, 128
    cv = Canvas(W, H)
    c, base = W / 2, H - 3.0
    if pal is None:                     # camp flame (CampProps drawFlame, plains #FF8800)
        col = P.hex_rgb("#FF8800")
        outer = P.srgb_to_lin(P.mix(col, P.hex_rgb("#B01810"), .25))
        mid = P.srgb_to_lin(P.mix(col, P.hex_rgb("#FFB030"), .55))
        inner = P.srgb_to_lin(P.mix(col, P.hex_rgb("#FFF0A0"), .85))
        line = P.srgb_to_lin(P.mix(col, P.hex_rgb("#400808"), .5))
        w, h = W * .62, H * .9
        main = P.polygon(cv, flame_path(c, base, w, h, w * .1))
        side_l = P.polygon(cv, flame_path(c - w * .28, base - h * .05, w * .42, h * .58, -w * .2))
        side_r = P.polygon(cv, flame_path(c + w * .28, base - h * .05, w * .4, h * .5, w * .2))
        allf = P.union(main, side_l, side_r)
        glow_under(cv, allf, 7.0, 0.5, outer)
        cv.over(outer, cv.cov(P.union(side_l, side_r)))
        cv.over(line, cv.cov(main - 0.0))
        cv.over(outer, cv.cov(main + 1.1))
        cv.over(mid, cv.cov(P.polygon(cv, flame_path(c + w * .04, base, w * .68, h * .74, w * .08))))
        cv.over(inner, cv.cov(P.polygon(cv, flame_path(c + w * .06, base - 1, w * .36, h * .44, w * .04))))
        return cv.result()
    rim, mid, core = (P.lin(h) for h in pal)
    layers = [((1.0, 1.0), rim, 0.88), ((0.72, 0.8), mid, 0.97), ((0.42, 0.52), core, 1.0)]
    for i, ((sx, sy), col, a) in enumerate(layers):
        bw = W * .42 * sx
        top = base - (base - 4.0) * sy
        f = P.polygon(cv, tongue_path(c, base, bw, top))
        if i == 0:
            glow_under(cv, f, 6.0, 0.55, rim)
        cv.over(col, cv.cov(f) * a)
    return cv.result()


def paint_flame():
    return P.strip([flame_cell(pal) for _, pal in FLAME_CELLS])


def shard_cell(light, shade, line, W=32, H=96):
    cv = Canvas(W, H)
    c = W / 2
    top, bot, mid, hw = H * .02, H * .98, H * .62, W * .44
    left = P.polygon(cv, [(c, top), (c - hw, mid), (c, bot)])
    right = P.polygon(cv, [(c, top), (c + hw, mid), (c, bot)])
    whole = P.union(left, right)
    cv.over(light, cv.cov(left) * 0.96)
    cv.over(shade, cv.cov(right) * 0.96)
    rim = np.abs(whole + 1.0) - 1.0
    cv.over(line, cv.cov(rim))
    cv.over(WHITE, cv.cov(P.stroke(cv, [(c, top + H * .07), (c, bot - H * .12)], [1.0, 2.2])))
    return cv.result()


def paint_shard():
    return shard_cell(WHITE, P.lin("#9A9AA8"), P.lin("#D8D8E0"))


def paint_shard_frost():
    return shard_cell(P.lin("#EAFCFF"), P.lin("#5AB4EC"), P.lin("#2A70C0"))


def paint_ring():
    """Ground ring (radius 0.4375 of the quad, web 56/128): crisp hot core line, inner band, soft halo."""
    cv = Canvas(256, 256)
    R = 112.0
    d = np.abs(np.hypot(cv.x - 128, cv.y - 128) - R)
    cv.over(WHITE, P.halo(d - 5.0, 16.0, 1.8) * 0.28)
    cv.over(WHITE * 0.9, cv.cov(d - 6.0) * 0.6)
    cv.over(WHITE, cv.cov(d - 2.6))
    return cv.result()


def paint_shock():
    """Shockwave disc: faint fill, a cel band, then the bright rim (web stops, stepped)."""
    cv = Canvas(256, 256)
    t = np.hypot(cv.x - 128, cv.y - 128) / 128.0
    a = P.steps(t, [0.05, 0.16, 0.38, 1.0, 0.0], [0.56, 0.78, 0.87, 0.965], 0.012)
    a = a * (t < 1.0) + 0.0
    soft = P.stops(t, [(0, 0), (.55, .04), (.8, .28), (.93, .9), (.97, .6), (1, 0)])
    cv.over(WHITE, np.maximum(a * 0.92, soft))
    return cv.result()


def crescent(cv: Canvas, cx, cy, ro, thick, tip_y):
    """True crescent: outer circle (centre ``(cx, cy)``, radius ``ro``) minus an inner circle dropped so the band is
    ``thick`` px at the top and the two circles meet in sharp tips at height ``tip_y``."""
    top = cy - ro
    k = ro - thick                         # inner radius − drop
    dy = cy - tip_y
    xt2 = ro * ro - dy * dy
    # xt² + (dy + drop)² = (k + drop)²  →  drop = (xt² + dy² − k²) / (2 (k − dy))
    drop = (xt2 + dy * dy - k * k) / (2.0 * (k - dy))
    ri = k + drop
    del top
    return P.subtract(P.circle(cv, cx, cy, ro), P.circle(cv, cx, cy + drop, ri)), (cx, cy + drop, ri)


def slash_cell(kind: str) -> np.ndarray:
    """Crescent weapon swipe (256×128; arc centre at the bottom-centre like the web fx_slash so the quad's +Y is
    the direction the blade travels): sharp tips, white-hot leading edge, body band, fading trailing band."""
    W, H = 256, 128
    cv = Canvas(W, H)
    cx, cy = W / 2, H
    ro = H - 5.0
    if kind == "rogue":
        a, _ = crescent(cv, cx, cy, ro, 13, H - 14)
        b, _ = crescent(cv, cx, cy, ro - 22, 9, H - 10)
        both = P.union(a, b)
        glow_under(cv, both, 6.0, 0.45)
        cv.over(WHITE * 0.72, cv.cov(b) * 0.85)
        cv.over(WHITE * 0.85, cv.cov(a))
        cv.over(WHITE, cv.cov(crescent(cv, cx, cy, ro, 5, H - 22)[0]))
        cv.over(WHITE, cv.cov(crescent(cv, cx, cy, ro - 22, 3.5, H - 20)[0]) * 0.9)
        return cv.result()
    if kind == "mage":
        body, (icx, icy, iri) = crescent(cv, cx, cy, ro, 22, H - 10)
        glow_under(cv, body, 8.0, 0.5)
        inner_d = np.hypot(cv.x - icx, cv.y - icy) - iri          # 0 on the trailing edge, < 0 outside
        band = P.steps(np.clip(inner_d / 22.0, 0, 1), [0.35, 0.7, 1.0], [0.35, 0.68], 0.05)
        cv.over(WHITE * 0.82, cv.cov(body) * band)
        cv.over(WHITE, cv.cov(crescent(cv, cx, cy, ro, 7, H - 24)[0]))
        r = P.rng(5)
        for i in range(9):                       # arcane motes trailing inside the arc
            ang = math.pi * (0.16 + 0.68 * i / 8) + (r() - 0.5) * 0.06
            rr = ro - 34 - r() * 12
            x, y = cx + math.cos(ang) * rr, cy - math.sin(ang) * rr
            sz = 2.4 + 2.6 * math.sin(math.pi * i / 8)
            cv.over(WHITE, cv.cov(P.star(cv, x, y, sz * 2.0, sz * 0.6, 4, curve=0.3)))
        return cv.result()
    if kind == "heavy":
        body, (icx, icy, iri) = crescent(cv, cx, cy, ro, 40, H - 8)
        r = P.rng(13)
        teeth = []
        for i in range(13):                      # ragged trailing edge: notches bitten out of the inner rim
            ang = math.pi * (0.12 + 0.76 * i / 12)
            x, y = icx + math.cos(ang) * iri, icy - math.sin(ang) * iri
            teeth.append(P.polygon(cv, [(x + math.cos(ang + 1.9) * 9, y - math.sin(ang + 1.9) * 9),
                                        (x + math.cos(ang) * (10 + r() * 12), y - math.sin(ang) * (10 + r() * 12)),
                                        (x + math.cos(ang - 1.9) * 9, y - math.sin(ang - 1.9) * 9)]))
        body = P.subtract(body, P.union(*teeth))
        glow_under(cv, body, 7.0, 0.5)
        cv.over(WHITE * 0.72, cv.cov(body) * 0.9)
        cv.over(WHITE * 0.92, cv.cov(crescent(cv, cx, cy, ro, 16, H - 16)[0]))
        cv.over(WHITE, cv.cov(crescent(cv, cx, cy, ro, 6, H - 30)[0]))
        return cv.result()
    # warrior / basic: the web crescent, banded
    body, (icx, icy, iri) = crescent(cv, cx, cy, ro, 32, H - 8)
    glow_under(cv, body, 7.0, 0.5)
    inner_d = np.hypot(cv.x - icx, cv.y - icy) - iri
    band = P.steps(np.clip(inner_d / 32.0, 0, 1), [0.25, 0.55, 0.85], [0.3, 0.6], 0.04)
    cv.over(WHITE * 0.85, cv.cov(body) * band)
    cv.over(WHITE * 0.95, cv.cov(crescent(cv, cx, cy, ro, 14, H - 16)[0]))
    cv.over(WHITE, cv.cov(crescent(cv, cx, cy, ro, 6, H - 28)[0]))
    return cv.result()


def paint_slash():
    return P.strip([slash_cell(k) for k in SLASH_CELLS])


def paint_claw():
    cv = Canvas(128, 128)
    marks = []
    for i in (-1, 0, 1):
        ox = 64 + i * 24
        top = 12 + abs(i) * 8
        bot = 116 - abs(i) * 8
        pts = P.bezier((ox - 18, top), (ox + 12, 52), (ox + 16, bot), None, 16)
        widths = [10.0 * math.sin(math.pi * (k / 16) ** 0.85) + 0.6 for k in range(17)]
        marks.append(P.stroke(cv, pts, widths))
    f = P.union(*marks)
    glow_under(cv, f, 7.0, 0.55)
    cv.over(WHITE * 0.85, cv.cov(f))
    cv.over(WHITE, cv.cov(f + 2.2))
    return cv.result()


def bolt_cell(seed: int) -> np.ndarray:
    """Jagged vertical bolt (64×256; Beam quads stretch the cell along the span), branch, glow and white core."""
    W, H = 64, 256
    cv = Canvas(W, H)
    r = P.rng(101 + seed * 17)
    n = 9
    pts = [(W / 2, H - 1.0)]
    for i in range(1, n):
        pts.append((W / 2 + (r() - 0.5) * W * 0.46, H - 1 - (i / n) * (H - 2) + (r() - 0.5) * 10))
    pts.append((W / 2, 1.0))
    bi = 3 + int(r() * 3)
    b0 = pts[bi]
    sgn = 1 if r() > 0.5 else -1
    branch = [b0, (b0[0] + sgn * 12, b0[1] - 20), (b0[0] + sgn * 17, b0[1] - 40)]
    main = P.stroke(cv, pts, 4.4)
    br = P.stroke(cv, branch, [3.0, 2.2, 1.0])
    allf = P.union(main, br)
    cv.over(WHITE, P.halo(allf, 14.0, 1.6) * 0.22)
    cv.over(WHITE * 0.85, cv.cov(allf - 3.0) * 0.5)
    cv.over(WHITE, cv.cov(allf))
    return cv.result()


def paint_bolt():
    return P.strip([bolt_cell(v) for v in range(4)])


def glyph(cv: Canvas, x, y, ang, s, kind):
    """Small angular rune glyph (three strokes) centred at (x, y), up = outward."""
    ca, sa = math.cos(ang), math.sin(ang)

    def T(u, v):          # u along the ring tangent, v outward
        return (x + u * -sa * s + v * ca * s, y + u * ca * s + v * sa * s)
    shapes = [
        [[(0, -1), (0, 1)], [(0, 0.2), (0.7, 0.9)], [(0, -0.3), (-0.6, 0.3)]],
        [[(-0.6, -1), (-0.6, 1)], [(-0.6, 1), (0.6, 0)], [(0.6, 0), (-0.6, -0.2)]],
        [[(-0.7, -1), (0, 1)], [(0, 1), (0.7, -1)], [(-0.4, -0.2), (0.4, -0.2)]],
        [[(0, -1), (0, 1)], [(-0.7, 0.6), (0.7, 0.6)], [(-0.5, -0.4), (0.5, -0.4)]],
    ][kind % 4]
    return P.union(*[P.stroke(cv, [T(*a), T(*b)], 2.6) for a, b in shapes])


def paint_rune():
    """Magic circle: double outer ring with rune glyphs, hexagram, inner ring, centre ring (web fx_rune)."""
    cv = Canvas(256, 256)
    c = 128
    rr = np.hypot(cv.x - c, cv.y - c)
    lines = [np.abs(rr - 118) - 2.6, np.abs(rr - 98) - 1.4, np.abs(rr - 60) - 1.8, np.abs(rr - 22) - 1.4]
    for k in range(2):
        pts = []
        for i in range(3):
            a = -math.pi / 2 + k * math.pi / 3 + i * 2 * math.pi / 3
            pts.append((c + math.cos(a) * 98, c + math.sin(a) * 98))
        for i in range(3):
            lines.append(P.stroke(cv, [pts[i], pts[(i + 1) % 3]], 2.8))
    for i in range(16):
        a = i / 16 * math.pi * 2 + math.pi / 16
        if i % 2 == 0:
            lines.append(glyph(cv, c + math.cos(a) * 108, c + math.sin(a) * 108, a, 5.2, i // 2))
        else:
            lines.append(P.circle(cv, c + math.cos(a) * 108, c + math.sin(a) * 108, 2.2))
    for i in range(6):                            # inner dots at the hexagram's inner vertices
        a = -math.pi / 2 + i * math.pi / 3 + math.pi / 6
        lines.append(P.circle(cv, c + math.cos(a) * 60, c + math.sin(a) * 60, 3.4))
    f = P.union(*lines)
    cv.over(WHITE, P.halo(f, 9.0, 1.6) * 0.4)
    cv.over(WHITE, cv.cov(f))
    return cv.result()


def paint_scorch():
    """Char blotch (tinted dark by the recipe): irregular stepped char bands that follow the blot's edge, and
    branching cracks (darker)."""
    cv = Canvas(256, 256)
    c = 128
    blob = P.wobble_circle(cv, c, c, 100, 0.08, 5, 55)
    inner = P.wobble_circle(cv, c + 4, c - 3, 62, 0.12, 4, 56)
    core = P.wobble_circle(cv, c - 2, c + 2, 30, 0.16, 3, 57)
    cv.over(WHITE, cv.cov(blob, 6.0) * 0.32)
    cv.over(WHITE, cv.cov(inner, 1.5) * 0.62)
    cv.over(WHITE, cv.cov(core, 1.0) * 0.86)
    r = P.rng(55)
    cracks = []
    for i in range(7):
        ang = i / 7 * math.pi * 2 + r() * 0.5
        d0 = 16 + r() * 10
        pts, ws = [(c + math.cos(ang) * d0, c + math.sin(ang) * d0)], [4.8]
        a, dist = ang, d0
        for s_ in range(3):
            a += (r() - 0.5) * 0.7
            dist += 18 + r() * 16
            pts.append((c + math.cos(a) * dist, c + math.sin(a) * dist))
            ws.append(4.8 * (1 - (s_ + 1) / 3.4))
        cracks.append(P.stroke(cv, pts, ws))
        if r() > 0.45:
            bx, by = pts[1]
            fa = a + (0.7 if r() > 0.5 else -0.7)
            cracks.append(P.stroke(cv, [(bx, by), (bx + math.cos(fa) * 20, by + math.sin(fa) * 20)], [2.6, 0.6]))
    cv.over(WHITE * 0.32, cv.cov(P.union(*cracks)) * 0.95)
    return cv.result()


def crack_field(cv: Canvas):
    c = cv.w / 2
    r = P.rng(77)
    strokes = []
    for i in range(9):
        a = i / 9 * math.pi * 2 + r() * 0.4
        x, y = c + math.cos(a) * 12, c + math.sin(a) * 12
        pts, ws = [(x, y)], [7.0]
        segs = 3 + int(r() * 2)
        for s in range(segs):
            ln = 20 + r() * 20
            aa = a + (r() - 0.5) * 0.7
            x, y = x + math.cos(aa) * ln, y + math.sin(aa) * ln
            pts.append((x, y))
            ws.append(7.0 * (1 - (s + 1) / (segs + 0.6)))
        strokes.append(P.stroke(cv, pts, ws))
        if r() > 0.4:                       # fork
            k = 1 + int(r() * (len(pts) - 2))
            bx, by = pts[k]
            fa = a + (0.6 if r() > 0.5 else -0.6)
            strokes.append(P.stroke(cv, [(bx, by), (bx + math.cos(fa) * 18, by + math.sin(fa) * 18)], [3.2, 0.8]))
    return P.union(*strokes)


def paint_crack():
    cv = Canvas(256, 256)
    f = P.union(crack_field(cv), P.wobble_circle(cv, 128, 128, 15, 0.15, 3, 78))
    cv.over(WHITE, cv.cov(f) * 0.92)
    return cv.result()


def paint_crack_glow():
    cv = Canvas(256, 256)
    f = P.union(crack_field(cv), P.wobble_circle(cv, 128, 128, 15, 0.15, 3, 78))
    cv.over(WHITE, P.halo(f, 12.0, 1.6) * 0.5)
    cv.over(WHITE, cv.cov(f))
    return cv.result()


def paint_frost():
    """Frost crust decal: a faceted pale ice patch (two cel tones) with crystal spikes and barbs, white rims."""
    cv = Canvas(256, 256)
    c = 128
    r = P.rng(31)
    patch = P.wobble_circle(cv, c, c, 92, 0.1, 6, 31)
    cv.over(WHITE * 0.75, cv.cov(patch, 10.0) * 0.45)
    shards = []
    for i in range(11):
        a = i / 11 * math.pi * 2 + r() * 0.3
        d0, ln, wd = 10 + r() * 10, 60 + r() * 40, 9 + r() * 6
        ax, ay = c + math.cos(a) * d0, c + math.sin(a) * d0
        tx, ty = c + math.cos(a) * (d0 + ln), c + math.sin(a) * (d0 + ln)
        nx, ny = -math.sin(a), math.cos(a)
        mx, my = c + math.cos(a) * (d0 + ln * .45), c + math.sin(a) * (d0 + ln * .45)
        shards.append(P.polygon(cv, [(ax, ay), (mx + nx * wd, my + ny * wd), (tx, ty), (mx - nx * wd, my - ny * wd)]))
    sf = P.union(*shards)
    cv.over(WHITE * 0.7, cv.cov(sf) * 0.6)
    # lit facet (left half of each shard ≈ upper-left side of the field)
    lit = P.intersect(sf + 1.5, (cv.x - c) * 0.7 + (cv.y - c) * 0.7 + 10.0)
    cv.over(WHITE * 0.95, cv.cov(lit) * 0.55)
    spikes = []
    for i in range(14):
        a = r() * math.pi * 2
        d0, ln = 20 + r() * 50, 18 + r() * 26
        x, y = c + math.cos(a) * d0, c + math.sin(a) * d0
        ex, ey = x + math.cos(a) * ln, y + math.sin(a) * ln
        mx, my = x + math.cos(a) * ln * .55, y + math.sin(a) * ln * .55
        spikes.append(P.stroke(cv, [(x, y), (ex, ey)], [3.2, 1.0]))
        for s in (0.7, -0.7):
            spikes.append(P.stroke(cv, [(mx, my), (mx + math.cos(a + s) * 9, my + math.sin(a + s) * 9)], [2.0, 0.8]))
    spk = P.union(*spikes)
    cv.over(WHITE, P.halo(spk, 5.0) * 0.35)
    cv.over(WHITE, cv.cov(spk))
    return cv.result()


def paint_puddle():
    """Liquid puddle (tint venom / blood): smooth blob, lower-right deep tone, bright rim, glint."""
    cv = Canvas(256, 256)
    c = 128
    r = P.rng(9)
    pts = []
    for i in range(14):
        a = i / 14 * math.pi * 2
        rr = c * (0.62 + r() * 0.28)
        pts.append((c + math.cos(a) * rr, c + math.sin(a) * rr))
    dense = []
    for i in range(14):
        p0, p1, p2 = pts[i - 1], pts[i], pts[(i + 1) % 14]
        m0 = ((p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2)
        m1 = ((p1[0] + p2[0]) / 2, (p1[1] + p2[1]) / 2)
        dense += P.bezier(m0, p1, m1, None, 6)[:-1]
    blob = P.polygon(cv, dense)
    cv.over(WHITE * 0.7, cv.cov(blob) * 0.8)
    shifted = P.polygon(cv, [(x - 18, y - 20) for x, y in dense])
    deep = P.subtract(blob + 3.0, shifted)
    cv.over(WHITE * 0.42, cv.cov(deep) * 0.85)
    rim = np.abs(blob + 2.0) - 2.0
    cv.over(WHITE, cv.cov(rim) * 0.9)
    cv.over(WHITE, cv.cov(P.ellipse(cv, c - 28, c - 28, 17, 8, -0.5)))
    cv.over(WHITE, cv.cov(P.circle(cv, c - 6, c - 40, 4)))
    return cv.result()


def hex_pts(cx, cy, r, rot=math.pi / 6):
    return [(cx + math.cos(rot + i * math.pi / 3) * r, cy + math.sin(rot + i * math.pi / 3) * r) for i in range(6)]


def paint_hex():
    cv = Canvas(64, 64)
    c = 32
    outer = P.polygon(cv, hex_pts(c, c, 28))
    cv.over(WHITE, cv.cov(outer) * 0.28)
    hl = P.intersect(outer, P.circle(cv, c - 11, c - 13, 19))
    cv.over(WHITE, cv.cov(hl) * 0.22)
    rim = np.abs(P.polygon(cv, hex_pts(c, c, 26.5))) - 2.3
    cv.over(WHITE, P.halo(rim, 5.0) * 0.5)
    cv.over(WHITE, cv.cov(rim))
    inner = np.abs(P.polygon(cv, hex_pts(c, c, 18.5))) - 0.9
    cv.over(WHITE, cv.cov(inner) * 0.6)
    return cv.result()


def paint_bubble():
    """Energy bubble: faint fill, a cel fresnel band, crisp bright rim, highlight arc + glint (web fx_bubble)."""
    cv = Canvas(256, 256)
    c = 128
    t = np.hypot(cv.x - c, cv.y - c) / 126.0
    a = P.steps(t, [0.07, 0.16, 0.42, 1.0, 0.0], [0.62, 0.82, 0.915, 0.985], 0.01)
    cv.over(WHITE, a)
    arc = []
    for i in range(17):
        ang = math.pi * (1.06 + 0.36 * i / 16)
        arc.append((c + math.cos(ang) * 92, c + math.sin(ang) * 92))
    widths = [8.0 * math.sin(math.pi * i / 16) + 1.0 for i in range(17)]
    cv.over(WHITE, cv.cov(P.stroke(cv, arc, widths)))
    cv.over(WHITE, cv.cov(P.circle(cv, c - 34, c - 72, 6.0)))
    return cv.result()


def paint_flake():
    cv = Canvas(64, 64)
    c = 32
    arms = []
    for i in range(6):
        a = i / 6 * math.pi * 2 + math.pi / 2
        ex, ey = c + math.cos(a) * 27, c + math.sin(a) * 27
        arms.append(P.stroke(cv, [(c, c), (ex, ey)], [4.6, 2.2]))
        mx, my = c + math.cos(a) * 16, c + math.sin(a) * 16
        for s in (0.8, -0.8):
            arms.append(P.stroke(cv, [(mx, my), (mx + math.cos(a + s) * 9, my + math.sin(a + s) * 9)], [2.8, 1.2]))
    f = P.union(*arms, P.polygon(cv, hex_pts(c, c, 6.5, math.pi / 2)))
    cv.over(WHITE, P.halo(f, 5.0) * 0.45)
    cv.over(WHITE, cv.cov(f))
    return cv.result()


def beam_cell(streaks: bool) -> np.ndarray:
    """Light column (64×256, base on the bottom edge — upright quads stand on it): core stripe, cel side band,
    soft edge; vertical fade to the top (web fx_beam). ``streaks``: rising light streaks (loot / level-up pillar)."""
    W, H = 64, 256
    cv = Canvas(W, H)
    u = np.abs(cv.x - W / 2) / (W / 2)
    v = cv.y / H
    horiz = np.maximum(P.steps(1 - u, [0.0, 0.4, 1.0], [0.48, 0.84], 0.04), P.stops(u, [(0, 1), (.5, .35), (1, 0)]))
    vert = P.stops(v, [(0, 0), (.15, .12), (.45, .55), (.88, 1.0), (1.0, .7)])
    cv.over(WHITE, horiz * vert)
    if streaks:
        r = P.rng(404)
        for i in range(8):
            x = W / 2 + (i / 7 - 0.5) * W * 0.66 + (r() - 0.5) * 4
            y0 = H * (0.08 + r() * 0.42)
            y1 = y0 + H * (0.14 + r() * 0.16)
            s = P.stroke(cv, [(x, y0), (x, y1)], [2.6, 0.3])         # bright rising heads, fading tails
            cv.over(WHITE, cv.cov(s) * (0.6 + 0.4 * r()))
        core = P.stroke(cv, [(W / 2, H * 0.1), (W / 2, H)], [1.0, 7.0])
        cv.over(WHITE, cv.cov(core) * 0.9)
    return cv.result()


def paint_beam():
    return P.strip([beam_cell(False), beam_cell(True)])


def paint_coin():
    cv = Canvas(32, 32)
    o = P.circle(cv, 16, 16, 13)
    cv.over(P.lin("#8A5410"), cv.cov(o))
    cv.over(P.lin("#E8A82A"), cv.cov(o + 1.6))
    cv.over(P.lin("#FFD95A"), cv.cov(P.intersect(o + 1.6, P.circle(cv, 14.2, 14.2, 10.2))))
    cv.over(P.lin("#FFF4B8"), cv.cov(P.ellipse(cv, 11.5, 11.5, 4.0, 3.0, -0.6)))
    return cv.result()


def paint_plus():
    cv = Canvas(64, 64)
    bar = lambda x0, y0, x1, y1: P.subtract(P.polygon(cv, [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]), cv.x * 0 - 1e9)
    f = P.union(P.polygon(cv, [(24, 7), (40, 7), (40, 57), (24, 57)]), P.polygon(cv, [(7, 24), (57, 24), (57, 40), (7, 40)]))
    f = f - 0.0
    rounded = f + 3.0
    rounded = P.smooth_union(rounded, rounded, 1.0) - 3.0
    cv.over(WHITE, P.halo(rounded, 8.0) * 0.45)
    cv.over(WHITE * 0.82, cv.cov(rounded))
    inner = P.union(P.polygon(cv, [(28.5, 12), (35.5, 12), (35.5, 52), (28.5, 52)]),
                    P.polygon(cv, [(12, 28.5), (52, 28.5), (52, 35.5), (12, 35.5)]))
    cv.over(WHITE, cv.cov(inner))
    del bar
    return cv.result()


def paint_skull():
    cv = Canvas(128, 128)
    head = P.circle(cv, 64, 53, 35)
    jaw = P.polygon(cv, [(42, 66), (86, 66), (86, 96), (42, 96)]) - 0.0
    jaw = jaw + 7.0
    jaw = P.smooth_union(jaw, jaw, 1.0) - 7.0
    skull = P.smooth_union(head, jaw, 6.0)
    holes = P.union(P.ellipse(cv, 49.5, 56, 11.2, 13.5, 0.25), P.ellipse(cv, 78.5, 56, 11.2, 13.5, -0.25),
                    P.polygon(cv, [(64, 69), (58.5, 80), (69.5, 80)]),
                    P.polygon(cv, [(53.5, 88), (58, 88), (58, 101), (53.5, 101)]),
                    P.polygon(cv, [(70, 88), (74.5, 88), (74.5, 101), (70, 101)]))
    s = P.subtract(skull, holes)
    cv.over(WHITE, P.halo(skull, 10.0) * 0.45)
    cv.over(WHITE * 0.86, cv.cov(s))
    cv.over(WHITE, cv.cov(P.intersect(s + 2.0, P.circle(cv, 52, 38, 20))))
    return cv.result()


def paint_arrow():
    """Cel arrow pointing up (quad +Y = flight): wood shaft, white/red fletching, steel head (pre-coloured)."""
    W, H = 32, 128
    cv = Canvas(W, H)
    c = W / 2
    shaft = P.stroke(cv, [(c, H - 14), (c, 30)], 4.0)
    cv.over(P.lin("#4A2E18"), cv.cov(shaft - 1.2))
    cv.over(P.lin("#A8703C"), cv.cov(shaft))
    cv.over(P.lin("#D8A468"), cv.cov(P.stroke(cv, [(c - 0.9, H - 18), (c - 0.9, 34)], 1.0)))
    for side, col in ((-1, "#E8E2D0"), (1, "#C84A3A")):
        f = P.polygon(cv, [(c + side * 1.5, H - 30), (c + side * 11, H - 4), (c + side * 1.5, H - 14)])
        cv.over(P.lin("#7A3A2A"), cv.cov(f - 1.0))
        cv.over(P.lin(col), cv.cov(f))
    head = P.polygon(cv, [(c, 2), (c + 9, 24), (c, 19), (c - 9, 24)])
    cv.over(P.lin("#46505E"), cv.cov(head - 1.1))
    cv.over(P.lin("#E8EEF6"), cv.cov(head))
    cv.over(P.lin("#98A4B8"), cv.cov(P.intersect(head, cv.x - c)))
    return cv.result()


def rock_cell(seed: int) -> np.ndarray:
    cv = Canvas(32, 32)
    r = P.rng(300 + seed)
    pts = []
    for i in range(7):
        a = i / 7 * math.pi * 2 + r() * 0.4
        pts.append((16 + math.cos(a) * (10 + r() * 4), 16 + math.sin(a) * (8.5 + r() * 3.5)))
    f = P.polygon(cv, pts)
    cv.over(P.lin("#3A2E24"), cv.cov(f))
    cv.over(P.lin("#8A7358"), cv.cov(f + 1.3))
    cv.over(P.lin("#5C4A3A"), cv.cov(P.intersect(f + 1.3, P.circle(cv, 23, 22, 12))))
    cv.over(P.lin("#BCA27C"), cv.cov(P.intersect(f + 1.3, P.circle(cv, 10, 9, 7))))
    return cv.result()


def paint_rock():
    return P.strip([rock_cell(0), rock_cell(1)])


def paint_chain():
    cv = Canvas(64, 32)
    ring = np.abs(P.ellipse(cv, 32, 16, 24, 11)) - 5.2
    cv.over(P.lin("#3A4250"), cv.cov(ring))
    cv.over(P.lin("#A8B4C4"), cv.cov(ring + 1.6))
    hl = P.intersect(ring + 3.2, (cv.y - 16) + (cv.x - 32) * 0.2 + 2.0)
    cv.over(P.lin("#EEF4FA"), cv.cov(hl))
    return cv.result()


def impact_cell(kind: str) -> np.ndarray:
    """Hit impact stars per hit profile (combat-feel §11): normal (light / normal) 6-point burst, heavy 8-point
    jagged burst, crit spiky starburst + white cross, kill burst + shattered ring."""
    cv = Canvas(128, 128)
    c = 64
    r = P.rng({"normal": 1, "heavy": 2, "crit": 3, "kill": 4}[kind])

    def burst(n, ro, ri, jitter, rot=0.0):
        pts = []
        for i in range(2 * n):
            a = rot + i * math.pi / n - math.pi / 2
            rr = (ro * (1 - jitter + 2 * jitter * r())) if i % 2 == 0 else ri * (0.85 + 0.3 * r())
            pts.append((c + math.cos(a) * rr, c + math.sin(a) * rr))
        return P.polygon(cv, pts)
    if kind == "normal":
        outer, inner, core = burst(6, 50, 20, 0.14), burst(6, 34, 13, 0.1, 0.2), P.circle(cv, c, c, 9)
    elif kind == "heavy":
        outer, inner, core = burst(8, 58, 22, 0.2), burst(8, 40, 15, 0.16, 0.18), P.circle(cv, c, c, 11)
    elif kind == "crit":
        outer, inner = burst(12, 62, 24, 0.24), burst(12, 44, 18, 0.2, 0.13)
        core = P.star(cv, c, c, 40, 6, 4, curve=0.55)
    else:
        outer, inner, core = burst(10, 60, 26, 0.22), burst(10, 42, 18, 0.18, 0.31), P.circle(cv, c, c, 12)
    glow_under(cv, outer, 8.0, 0.4)
    cv.over(WHITE * 0.8, cv.cov(outer) * 0.8)
    cv.over(WHITE * 0.92, cv.cov(inner))
    cv.over(WHITE, cv.cov(core))
    if kind == "kill":
        ring = np.abs(np.hypot(cv.x - c, cv.y - c) - 54) - 2.4
        ang = np.arctan2(cv.y - c, cv.x - c)
        gaps = np.cos(ang * 7 + 0.4) > 0.35
        cv.over(WHITE, cv.cov(ring) * gaps)
    if kind == "crit":
        ring = np.abs(np.hypot(cv.x - c, cv.y - c) - 57) - 1.6
        cv.over(WHITE, cv.cov(ring) * 0.85)
    return cv.result()


def paint_impact():
    return P.strip([impact_cell(k) for k in IMPACT_CELLS])


def paint_wisp():
    """Spirit-fire wisp (pet bolts, soul echo): round head at the top, a curling tapered tail."""
    W, H = 64, 128
    cv = Canvas(W, H)
    head = P.circle(cv, 32, 34, 21)
    tail_pts = P.bezier((32, 34), (52, 70), (10, 86), (30, 124), 20)
    widths = [40 * (1 - k / 20) ** 1.3 + 1.0 for k in range(21)]
    tail = P.stroke(cv, tail_pts, widths)
    f = P.smooth_union(head, tail, 6.0)
    glow_under(cv, f, 8.0, 0.5)
    cv.over(WHITE * 0.7, cv.cov(f) * 0.85)
    inner = P.smooth_union(P.circle(cv, 30, 32, 12), P.stroke(cv, tail_pts[:9], [16 * (1 - k / 9) + 1 for k in range(9)]), 4)
    cv.over(WHITE, cv.cov(inner))
    return cv.result()


def paint_swirl():
    """Whirlwind ground swirl: three tapering spiral strokes."""
    cv = Canvas(256, 256)
    c = 128
    strokes = []
    for k in range(3):
        pts, ws = [], []
        for i in range(25):
            t = i / 24
            a = k * 2 * math.pi / 3 + t * 2.4
            rr = 40 + 78 * t
            pts.append((c + math.cos(a) * rr, c + math.sin(a) * rr))
            ws.append(1.0 + 11.0 * math.sin(math.pi * t ** 0.8))
        strokes.append(P.stroke(cv, pts, ws))
    f = P.union(*strokes)
    cv.over(WHITE, P.halo(f, 8.0) * 0.35)
    cv.over(WHITE * 0.8, cv.cov(f))
    cv.over(WHITE, cv.cov(f + 3.0))
    return cv.result()


# name → (painter, cells, blend the recipes use, description)
SPRITES = {
    "Glow": (paint_glow, 1, "additive", "soft radial light (web fx_glow)"),
    "Core": (paint_core, 1, "additive", "white-hot impact core: crisp disc + halo (web fx_core)"),
    "LightPool": (paint_lightpool, 1, "additive", "fake light pool, web glow falloff GLOW_STOPS"),
    "Spark": (paint_spark, 1, "additive", "four-point glint (web star4)"),
    "Star": (paint_star, 1, "additive", "five-point cartoon star (stun / sparkle fallback)"),
    "Streak": (paint_streak, 1, "additive", "velocity streak, head at the top"),
    "Comet": (paint_comet, 1, "additive", "comet head + two-band tail, head at the top"),
    "Ember": (paint_ember, 1, "additive", "teardrop ember"),
    "Drop": (paint_drop, 1, "translucent", "liquid droplet, cel shaded (tint blood / venom)"),
    "Smoke": (paint_smoke, 2, "translucent", "cartoon smoke puffs, two lobe layouts"),
    "Dust": (paint_dust, 1, "translucent", "low kicked-up dust cloud (charge, dodge, whirlwind)"),
    "Puff": (paint_puff, 1, "translucent", "soft round puff (web fx_puff)"),
    "Flame": (paint_flame, len(FLAME_CELLS), "translucent",
              "pre-coloured cel flame tongues: " + ", ".join(f"{i} {n}" for i, (n, _) in enumerate(FLAME_CELLS))),
    "Shard": (paint_shard, 1, "translucent", "faceted crystal shard (grey)"),
    "ShardFrost": (paint_shard_frost, 1, "translucent", "faceted ice shard, pre-coloured"),
    "Ring": (paint_ring, 1, "additive", "ground ring at 0.4375 of the quad"),
    "Shock": (paint_shock, 1, "additive", "shockwave disc with a bright rim"),
    "Slash": (paint_slash, len(SLASH_CELLS), "additive",
              "crescent swipes, arc centre at the bottom edge: " + ", ".join(f"{i} {n}" for i, n in enumerate(SLASH_CELLS))),
    "Claw": (paint_claw, 1, "additive", "three-talon rake"),
    "Bolt": (paint_bolt, 4, "additive", "vertical jagged bolts (beam quads)"),
    "Rune": (paint_rune, 1, "additive", "magic circle with rune glyphs"),
    "Scorch": (paint_scorch, 1, "translucent", "char decal (tint dark)"),
    "Crack": (paint_crack, 1, "translucent", "ground fissures (tint dark)"),
    "CrackGlow": (paint_crack_glow, 1, "additive", "glowing fissures (same geometry as Crack)"),
    "Frost": (paint_frost, 1, "additive", "frost crust decal"),
    "Puddle": (paint_puddle, 1, "translucent", "liquid puddle decal (tint venom / blood)"),
    "Hex": (paint_hex, 1, "additive", "hex shield plate glow"),
    "Bubble": (paint_bubble, 1, "additive", "energy bubble with fresnel rim"),
    "Flake": (paint_flake, 1, "additive", "snowflake"),
    "Beam": (paint_beam, 2, "additive", "light column (0 plain, 1 pillar with rising streaks)"),
    "Coin": (paint_coin, 1, "translucent", "cel gold coin, pre-coloured (mesh fallback)"),
    "Plus": (paint_plus, 1, "additive", "heal plus"),
    "Skull": (paint_skull, 1, "additive", "death-mark skull sigil"),
    "Arrow": (paint_arrow, 1, "translucent", "cel arrow pointing up, pre-coloured (mesh fallback)"),
    "Rock": (paint_rock, 2, "translucent", "cel rock chips, pre-coloured (mesh fallback)"),
    "Chain": (paint_chain, 1, "translucent", "cel chain link, pre-coloured (mesh fallback)"),
    "Impact": (paint_impact, len(IMPACT_CELLS), "additive",
               "hit impact stars per profile: " + ", ".join(f"{i} {n}" for i, n in enumerate(IMPACT_CELLS))),
    "Wisp": (paint_wisp, 1, "additive", "spirit-fire wisp, head at the top"),
    "Swirl": (paint_swirl, 1, "additive", "whirlwind ground swirl"),
}


def sprite_table() -> dict:
    """``sprites`` section of vfx_recipes.json + documentation (cells, blend, size)."""
    out = {}
    for name, (_, cells, blend, desc) in SPRITES.items():
        p = OUT / f"T_FX_{name}.png"
        size = None
        if p.exists():
            import struct
            with open(p, "rb") as f:
                f.read(16)
                w, h = struct.unpack(">II", f.read(8))
            size = [w, h]
        out[name] = {"cells": cells, "blend": blend, "texture": f"T_FX_{name}", "size": size, "note": desc}
    return out


# ── preview sheet ──────────────────────────────────────────────────────────────────────────────────────
GRASS = "#74A247"
DARK = "#2A2533"
TINTS = {   # how the recipes tint each sprite in the preview (palette mid / rim)
    "Glow": "#FFA532", "Core": "#FFFFFF", "LightPool": "#FF8800", "Spark": "#FFE27A", "Star": "#FFE27A",
    "Streak": "#FFF2C0", "Comet": "#FFA532", "Ember": "#FF5418", "Drop": "#E8342C", "Smoke": "#5A5060",
    "Dust": "#C8B89A", "Puff": "#CFEFFF", "Flame": "#FFFFFF", "Shard": "#FFFFFF", "ShardFrost": "#FFFFFF",
    "Ring": "#F4AC28", "Shock": "#FFA532", "Slash": "#AABBD4", "Claw": "#FF5A3A", "Bolt": "#CFE0FF",
    "Rune": "#8A5CFF", "Scorch": "#3A2A22", "Crack": "#2A1C14", "CrackGlow": "#FF9A3A", "Frost": "#A6F0FF",
    "Puddle": "#3FAE2E", "Hex": "#FFE27A", "Bubble": "#9A7CFF", "Flake": "#FFFFFF", "Beam": "#FFE27A",
    "Coin": "#FFFFFF", "Plus": "#A8F07A", "Skull": "#B478FF", "Arrow": "#FFFFFF", "Rock": "#FFFFFF",
    "Chain": "#FFFFFF", "Impact": "#FFD27A", "Wisp": "#9CE0FF", "Swirl": "#EEF3FB",
}


def composite(tex: np.ndarray, bg_hex: str, tint_hex: str, additive: bool, scale: int = 1) -> np.ndarray:
    """The UE sprite material over a flat background (linear light), → sRGB uint8."""
    t = tex.astype(np.float32) / 255.0
    if scale > 1:
        t = t.repeat(scale, 0).repeat(scale, 1)
    bg = np.broadcast_to(P.lin(bg_hex), t.shape[:2] + (3,)).copy()
    tint = P.lin(tint_hex)
    col = t[..., :3] * tint
    a = t[..., 3:4]
    out = bg + col * a if additive else bg * (1 - a) + col * a
    return (P.lin_to_srgb(np.clip(out, 0, 1)) * 255 + 0.5).astype(np.uint8)


def label(img: np.ndarray, text: str) -> np.ndarray:
    from kit import review
    out = img.copy()
    review.draw_text(out, 3, 3, text)
    return out


def sheet(names, page_w: int = 1180) -> None:
    """Every sprite cell over the dark studio and the plains grass, drawn by the UE sprite material maths
    (additive / translucent, linear light), shelf-packed into pages ≤ 200 KB."""
    from kit import review
    PREVIEW.mkdir(parents=True, exist_ok=True)
    groups = []
    for name in names:
        tex = pngio.read_png(OUT / f"T_FX_{name}.png")
        _, cells, blend, _ = SPRITES[name]
        h, w = tex.shape[:2]
        scale = 2 if max(h, w // cells) <= 64 else 1
        add = blend == "additive"
        cellw = w // cells
        tiles = []
        for i in range(cells):
            cell = tex[:, i * cellw:(i + 1) * cellw]
            tiles.append(composite(cell, DARK, TINTS.get(name, "#FFFFFF"), add, scale))
            tiles.append(composite(cell, GRASS, TINTS.get(name, "#FFFFFF"), add, scale))
        row = review.grid(tiles, len(tiles), pad=2)
        if row.shape[1] > page_w:
            row = review.grid(tiles, max(2, (page_w // (tiles[0].shape[1] + 2)) // 2 * 2), pad=2)
        lab = np.zeros((12, max(row.shape[1], 150), 3), np.uint8)
        lab[:] = (24, 20, 30)
        review.draw_text(lab, 3, 3, f"{name} {w}X{h} {cells}C {'ADD' if add else 'TRANS'}")
        row = np.pad(row, ((0, 0), (0, lab.shape[1] - row.shape[1]), (0, 0)), constant_values=24)
        groups.append(np.vstack([lab, row]))
    pages, shelf, shelves, x = [], [], [], 0
    for g in groups:
        if shelf and x + g.shape[1] > page_w:
            shelves.append(shelf)
            shelf, x = [], 0
        shelf.append(g)
        x += g.shape[1] + 6
    if shelf:
        shelves.append(shelf)
    rows = []
    for sh in shelves:
        hh = max(g.shape[0] for g in sh)
        parts = []
        for g in sh:
            parts.append(np.pad(g, ((0, hh - g.shape[0]), (0, 6), (0, 0)), constant_values=18))
        r = np.hstack(parts)
        rows.append(np.pad(r, ((0, 6), (0, page_w + 12 - r.shape[1]), (0, 0)), constant_values=18))
    cur, cur_h = [], 0
    for r in rows:
        if cur and cur_h + r.shape[0] > 760:
            pages.append(cur)
            cur, cur_h = [], 0
        cur.append(r)
        cur_h += r.shape[0]
    if cur:
        pages.append(cur)
    for old in PREVIEW.glob("textures_*.png"):
        old.unlink()
    for i, pg in enumerate(pages):
        pngio.write_png_budget(PREVIEW / f"textures_{i + 1}.png", np.vstack(pg))
        print(f"  {PREVIEW / f'textures_{i + 1}.png'}")


def main(argv) -> int:
    sheet_only = "--sheet-only" in argv
    names = [a for a in argv if not a.startswith("--")] or list(SPRITES)
    OUT.mkdir(parents=True, exist_ok=True)
    if not sheet_only:
        for name in names:
            fn = SPRITES[name][0]
            img = P.to_u8(fn())
            size = pngio.write_png(OUT / f"T_FX_{name}.png", img)
            print(f"T_FX_{name}: {img.shape[1]}x{img.shape[0]} {size / 1024:.1f} KB")
    sheet(list(SPRITES))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

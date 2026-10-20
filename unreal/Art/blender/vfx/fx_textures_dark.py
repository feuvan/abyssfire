"""Dark-remake VFX textures (焚境 · The Burning March, work package A12) — ``T_FX_<Sprite>`` sprites the march
recipes (``vfx/fx_recipes_dark.py``) and the telegraph / ash-dissolve material contracts draw.

    /opt/venvs/blender/bin/python unreal/Art/blender/vfx/fx_textures_dark.py [--sheet-only | --contracts] [names…]

Same contract as ``fx_textures.py`` (``Scripts/abyss_content/hlsl.py`` AF_FxAdd / AF_FxTrans, WorldContract 5.4):
RGBA8, **linear** values (``T_FX_*`` import with sRGB off), white RGB = tintable, pre-coloured cells store the linear
colour and are drawn with a white tint; strips of N equal cells (the instance Variant picks one). Built from signed
distance fields with cel steps (``fxpaint``) like the plains set, so the march sprites sit in the same family.

| Sprite | Cells | Blend | What |
|---|---|---|---|
| ``TelegraphRim`` | 1 (256×64) | translucent | the enemy telegraph rim **profile strip** for ``M_AF_Telegraph`` (U2): row 0 = the shape's boundary, row 63 = ``RimWidthCm`` inside; a crisp scorched line (burnt, uneven inner edge) then a decaying band of 45° warning hatch. Tileable in U (along the edge), so one strip serves circles, cones, lines and rings |
| ``TelegraphFill`` | 1 (256²) | translucent | the bone **fill grain** for ``M_AF_Telegraph``: a tileable cracked-ash network (values .66–1, alpha 1) the material multiplies with the bone fill #FFE8DC, sampled in world XY so it never swims with the progress |
| ``FlameBreach`` | 4 (64×128) | translucent | **breach fire** (violet-black flame, monsters.md §1.2, never ember), pre-coloured: a near-black body #1A0820 inside a glowing breach-violet edge #B02FD0, a deep-violet inner tongue and a pale #D08CFF base core; 0 tall tongue, 1 forked, 2 low wide lick (ground fires), 3 thin wisp |
| ``AshFlake`` | 4 (32²) | translucent | flat cel ash flakes (lit face, shade band, curled rim; white-coded, tinted ash grey): ashfall, the crumble, the fuse, bursts. 0-2 irregular plates, 3 a curled flake |
| ``Ripple`` | 2 (256²) | additive | a **sound ripple**: two concentric broken rings (the outer at 0.4375 of the quad like ``Ring``, so ring maths are shared) — bells (the Corpseherd's violet toll, the Hungerbell's bone toll, the Knellwatch bell) and the Dirgecursed aura; 0 standard, 1 fine (hairlines for radii of metres) |
| ``RaiseSigil`` | 1 (256²) | additive | the **raise marker**: a ring with seven hooked fingers reaching inward and a broken inner ring — the dead pulled up from below (breach-violet in the recipes; deliberately not the mage's rune circle) |
| ``AshNoise`` | 1 (256²) | translucent | data for the ash-dissolve material (U3, ``M_AF_AshDissolve``): R tileable fBm (the dissolve front), G the crack network (the husk breaks along it first), B per-flake random cells (which plates fall when); alpha 1 |

The run also writes the material contracts ``M_AF_Telegraph`` / ``M_AF_AshDissolve`` (``fx_recipes_dark.TELEGRAPH`` /
``ASH_DISSOLVE``) into ``manifest.json → materials`` (merged per key under the kit's lock).

Previews: ``Art/Previews/vfx_textures_dark/textures_*.png`` (every cell at 1:1 over the march mud #3A332D and ash
grass #4C4940 and the dark studio, tinted as the recipes tint it). ``sprites.json`` is merged (the plains entries
are kept; ``fx_textures.py`` merges the same way, so a re-run of either keeps both sets).
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
import fx_textures as FT  # noqa: E402

OUT = paths.export_root() / "VFX"
PREVIEW = paths.preview_root() / "vfx_textures_dark"
WHITE = np.ones(3, np.float32)

# breach-violet family (A1 Monsters_March: breach.glow / core / pinpoint; Valdras' seam core)
VIOLET, VIOLET_BLACK, VIOLET_PIN, VIOLET_HOT = "#B02FD0", "#1A0820", "#D08CFF", "#F0E0FF"
FLAME_BREACH_CELLS = ["tall", "forked", "lick", "wisp"]
ASHFLAKE_CELLS = ["plate_a", "plate_b", "plate_c", "curl"]


# ── telegraph (M_AF_Telegraph inputs) ───────────────────────────────────────────────────────────────────
def paint_telegraph_rim() -> np.ndarray:
    """Rim profile strip 256 × 64: row 0 is the telegraph shape's boundary, the last row ``RimWidthCm`` inside.
    A crisp line (7 px ± a burnt wobble on its inner edge) then a band of 45° hatch whose alpha decays inward —
    the warning language, readable over any ground and over the bone fill when the fill reaches the rim. Every
    wobble is an integer harmonic of the strip length and the hatch period divides it: tileable along U."""
    W, H = 256, 64
    cv = Canvas(W, H)
    u = cv.x / W
    wob = (1.5 * np.sin(2 * math.pi * 3 * u + 0.7) + 0.9 * np.sin(2 * math.pi * 7 * u + 2.1)
           + 0.5 * np.sin(2 * math.pi * 13 * u + 4.0))
    line_w = 7.0 + wob
    t = np.clip((cv.y - line_w) / (H - line_w), 0.0, 1.0)
    decay = (1.0 - t) ** 1.7
    period, duty = 16.0, 0.42
    ph = np.mod(cv.x + cv.y, period) / period              # 45° stripes; 256 % 16 == 0 → tileable in U
    tri = np.minimum(ph, 1.0 - ph)                          # 0 at the stripe centre … .5 between
    stripe = np.clip((duty * 0.5 - tri) * period / 1.2 + 0.5, 0.0, 1.0)
    band = decay * (0.26 + 0.34 * stripe) * (cv.y > line_w - 0.5)
    cv.over(WHITE * 0.94, band)
    # a thin darker seam right under the line: the line reads as a cut edge, not a glow
    seam = np.abs(cv.y - (line_w + 1.6)) - 0.9
    cv.over(WHITE * 0.55, cv.cov(seam) * 0.5)
    cv.over(WHITE, cv.cov(cv.y - line_w))
    return cv.result()


def _voronoi_edges(W: int, n: int, seed: int) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Tileable Voronoi of ``n`` well-spaced seeds on a W × W torus: (exact distance px to the cell border — the
    bisector of the two nearest seeds, (d2² − d1²) / 2|s2 − s1| —, F1 px, nearest cell id). F2 − F1 would fan
    into wide wedges between two close seeds."""
    r = P.rng(seed)
    min_sep = 0.6 * W / math.sqrt(n)
    pts: list[tuple[float, float]] = []
    tries = 0
    while len(pts) < n and tries < n * 200:
        tries += 1
        q = (r() * W, r() * W)
        ok = True
        for p in pts:
            dx = abs(q[0] - p[0])
            dy = abs(q[1] - p[1])
            if math.hypot(min(dx, W - dx), min(dy, W - dy)) < min_sep:
                ok = False
                break
        if ok:
            pts.append(q)
    S = np.array(pts, np.float32)
    ys, xs = np.mgrid[0:W, 0:W].astype(np.float32) + 0.5
    m = len(S)
    D = np.empty((m, W, W), np.float32)
    OX = np.empty((m, W, W), np.float32)                   # torus-unwrapped seed positions per pixel
    OY = np.empty((m, W, W), np.float32)
    for i, (px, py) in enumerate(S):
        dx = xs - px
        dy = ys - py
        dx = dx - W * np.round(dx / W)
        dy = dy - W * np.round(dy / W)
        D[i] = np.hypot(dx, dy)
        OX[i] = xs - dx
        OY[i] = ys - dy
    order = np.argsort(D, axis=0)
    i1, i2 = order[0], order[1]
    take = lambda A, I: np.take_along_axis(A, I[None], 0)[0]  # noqa: E731
    d1, d2 = take(D, i1), take(D, i2)
    sep = np.hypot(take(OX, i2) - take(OX, i1), take(OY, i2) - take(OY, i1))
    edge = (d2 * d2 - d1 * d1) / np.maximum(2.0 * sep, 1e-3)
    return edge, d1, i1.astype(np.int32)


def _value_noise(W: int, cells: int, seed: int) -> np.ndarray:
    """Periodic value noise (W × W, lattice ``cells`` × ``cells``, smoothstep interpolation) in [0, 1]."""
    r = P.rng(seed)
    lat = np.array([[r() for _ in range(cells)] for _ in range(cells)], np.float32)
    g = (np.arange(W, dtype=np.float32) + 0.5) * cells / W
    i0 = np.floor(g).astype(np.int32)
    f = g - i0
    f = f * f * (3 - 2 * f)
    i0 %= cells
    i1 = (i0 + 1) % cells
    a = lat[np.ix_(i0, i0)]
    b = lat[np.ix_(i0, i1)]
    c = lat[np.ix_(i1, i0)]
    d = lat[np.ix_(i1, i1)]
    fy, fx = f[:, None], f[None, :]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def paint_telegraph_fill() -> np.ndarray:
    """Fill grain 256² (tileable): a cracked-ash network (two Voronoi scales) and a soft value drift, values
    .66–1 — the bone fill reads as ground giving way, not as a flat plastic disc; alpha 1."""
    W = 256
    e1, _, _ = _voronoi_edges(W, 26, 9101)
    e2, _, _ = _voronoi_edges(W, 84, 9102)
    drift = 0.5 * _value_noise(W, 8, 9103) + 0.5 * _value_noise(W, 16, 9104)
    v = 0.9 + 0.1 * (drift - 0.5) * 2.0
    big = np.clip(1.0 - (e1 - 1.2) / 1.6, 0.0, 1.0)          # main cracks ~2.5 px
    small = np.clip(1.0 - (e2 - 0.5) / 1.0, 0.0, 1.0)        # hairline cracks
    v = v - 0.24 * big - 0.1 * small
    v = np.clip(v, 0.66, 1.0)
    img = np.ones((W, W, 4), np.float32)
    img[..., 0] = img[..., 1] = img[..., 2] = v
    return img


# ── breach fire ─────────────────────────────────────────────────────────────────────────────────────────
def breach_flame_cell(kind: str) -> np.ndarray:
    """One violet-black flame (64 × 128, base on the bottom edge = the upright quad's anchor): a breach-violet
    halo, the violet edge, a near-black body inset 2.6 px, a deep-violet inner tongue and a pale base core."""
    W, H = 64, 128
    cv = Canvas(W, H)
    c, base = W / 2, H - 3.0
    tongues = {
        "tall": [(c, W * .34, 8.0)],
        "forked": [(c - 7.0, W * .25, 12.0), (c + 9.0, W * .21, 34.0)],
        "lick": [(c, W * .42, H * .46), (c - 13.0, W * .2, H * .62), (c + 14.0, W * .18, H * .66)],
        "wisp": [(c + 2.0, W * .19, 5.0)],
    }[kind]
    fields = [P.polygon(cv, FT.tongue_path(x, base, bw, top)) for x, bw, top in tongues]
    outer = P.union(*fields)
    violet, black = P.lin(VIOLET), P.lin(VIOLET_BLACK)
    deep, pin, hot = P.lin("#561278"), P.lin(VIOLET_PIN), P.lin(VIOLET_HOT)
    FT.glow_under(cv, outer, 7.0, 0.5, violet)
    cv.over(violet, cv.cov(outer))
    cv.over(black, cv.cov(outer + 2.6))
    inner = []
    for x, bw, top in tongues:
        h = base - top
        inner.append(P.polygon(cv, FT.tongue_path(x + bw * .05, base - 1.0, bw * .52, base - h * .5)))
    cv.over(deep, cv.cov(P.union(*inner)) * 0.95)
    # the hot root: a small pale-violet tongue at the base of each flame (fire burns hottest at its root), a
    # thinner near-white sliver inside it — flame shapes, never a round "bubble"
    for x, bw, top in tongues:
        h = base - top
        root = P.polygon(cv, FT.tongue_path(x + bw * .08, base - 1.5, bw * .3, base - h * .22))
        cv.over(pin, cv.cov(root))
        sliver = P.polygon(cv, FT.tongue_path(x + bw * .1, base - 2.0, bw * .13, base - h * .14))
        cv.over(hot, cv.cov(sliver) * 0.9)
    return cv.result()


def paint_flame_breach() -> np.ndarray:
    return P.strip([breach_flame_cell(k) for k in FLAME_BREACH_CELLS])


# ── ash ─────────────────────────────────────────────────────────────────────────────────────────────────
def ash_flake_cell(kind: str, seed: int) -> np.ndarray:
    """A flat cel ash flake (32²): irregular plate, lit face 1.0, a lower-right shade band .62, a darker curled
    rim .42 (white-coded: the recipe tints it ash grey)."""
    W = 32
    cv = Canvas(W, W)
    r = P.rng(seed)
    c = W / 2
    n = 5 + int(r() * 3)
    pts = []
    for i in range(n):
        a = 2 * math.pi * i / n + r() * 0.5
        rad = W * (0.26 + r() * 0.17)
        pts.append((c + math.cos(a) * rad, c + math.sin(a) * rad * 0.8))
    body = P.polygon(cv, pts)
    if kind == "curl":
        body = P.subtract(body, P.circle(cv, c + W * .2, c - W * .16, W * .26))
    cv.over(WHITE, cv.cov(body))
    shade = P.intersect(body + 1.2, P.circle(cv, c + W * .32, c + W * .3, W * .36))
    cv.over(WHITE * 0.62, cv.cov(shade))
    rim = np.abs(body + 0.7) - 0.7
    cv.over(WHITE * 0.42, cv.cov(rim) * 0.9)
    return P.gutter(cv.result(), "TBLR", 0.5, 2.5)


def paint_ash_flake() -> np.ndarray:
    return P.strip([ash_flake_cell(k, 701 + i * 13) for i, k in enumerate(ASHFLAKE_CELLS)])


def paint_ash_noise() -> np.ndarray:
    """``M_AF_AshDissolve`` data (256², tileable, linear): R fBm (dissolve front), G crack mask (1 on the cracks),
    B a random value per plate (which plates drop first)."""
    W = 256
    # a low-frequency front (a 32-lattice octave turned the crumble into pixel speckle at the game camera) and
    # ~12 cm plates (24 per 60 cm tile): the husk breaks off in flakes, not grain
    fbm = 0.6 * _value_noise(W, 4, 501) + 0.4 * _value_noise(W, 8, 502)
    fbm = (fbm - fbm.min()) / max(fbm.max() - fbm.min(), 1e-6)
    edge, _, idx = _voronoi_edges(W, 24, 504)
    crack = np.clip(1.0 - (edge - 0.8) / 1.6, 0.0, 1.0)
    r = P.rng(505)
    cell_val = np.array([r() for _ in range(24)], np.float32)
    img = np.ones((W, W, 4), np.float32)
    img[..., 0] = fbm
    img[..., 1] = crack
    img[..., 2] = cell_val[idx]
    return img


# ── bells and the raise marker ──────────────────────────────────────────────────────────────────────────
def _broken_ring(cv: Canvas, c: float, R: float, width: float, gaps: list[tuple[float, float]]) -> np.ndarray:
    """Coverage of a ring of radius ``R`` with angular gaps [(centre rad, half-width rad)] (rounded ends)."""
    d, a = P.polar(cv, c, c)
    ring = np.abs(d - R) - width * 0.5
    cov = cv.cov(ring)
    for gc, gw in gaps:
        da = np.abs(np.angle(np.exp(1j * (a - gc))))
        arc_px = (da - gw) * R                                # px along the ring from the gap's edge
        cov = cov * np.clip(arc_px / 1.2 + 0.5, 0.0, 1.0)
    return cov


def ripple_cell(fine: bool) -> np.ndarray:
    """Sound ripple (256²): an outer broken ring at 0.4375 of the quad (``Ring`` maths) and a fainter inner ring
    at 0.37, each cut by uneven gaps, over a faint inner halo — a struck bell's tone, not a solid shock ring.
    ``fine``: hairline rings for radii of metres (the standard cell's lines turn into soft bands when a quad is
    stretched over 8 m)."""
    W = 256
    cv = Canvas(W, W)
    c = W / 2
    r = P.rng(4401)
    R1, R2 = 112.0, 95.0
    g1 = [(2 * math.pi * i / 6 + r() * 0.6, 0.07 + r() * 0.08) for i in range(6)]
    g2 = [(2 * math.pi * (i + 0.5) / 5 + r() * 0.6, 0.09 + r() * 0.1) for i in range(5)]
    w1, w2, halo = (1.25, 0.9, 5.0) if fine else (3.4, 2.2, 12.0)
    d1 = np.abs(np.hypot(cv.x - c, cv.y - c) - R1)
    cv.over(WHITE, P.halo(d1 - w1 * 0.6, halo, 1.8) * 0.22)
    cv.over(WHITE * 0.85, _broken_ring(cv, c, R2, w2, g2) * 0.6)
    cv.over(WHITE, _broken_ring(cv, c, R1, w1, g1))
    return cv.result()


def paint_ripple() -> np.ndarray:
    return P.strip([ripple_cell(False), ripple_cell(True)])


def paint_raise_sigil() -> np.ndarray:
    """Raise marker (256²): a ring, seven tapered hooked fingers curling inward from it, and a broken inner ring."""
    W = 256
    cv = Canvas(W, W)
    c = W / 2
    r = P.rng(6607)
    R = 104.0
    d = np.abs(np.hypot(cv.x - c, cv.y - c) - R)
    cv.over(WHITE, P.halo(d - 2.0, 14.0, 1.8) * 0.25)
    fingers = []
    for i in range(7):
        a0 = 2 * math.pi * i / 7 + r() * 0.25
        L = 0.36 + r() * 0.1
        pts, ws = [], []
        for k in range(9):
            t = k / 8
            rad = R - 2.0 - t * R * L
            a = a0 + 0.42 * t * t                           # the hook: curls clockwise toward the tip
            pts.append((c + math.cos(a) * rad, c + math.sin(a) * rad))
            ws.append(7.5 * (1 - t) + 1.4 * t)
        fingers.append(P.stroke(cv, pts, ws))
        kx, ky = c + math.cos(a0 + 0.05) * (R - R * L * 0.45), c + math.sin(a0 + 0.05) * (R - R * L * 0.45)
        fingers.append(P.circle(cv, kx, ky, 4.4))           # a knuckle
    f = P.union(*fingers)
    cv.over(WHITE * 0.9, cv.cov(f))
    gaps = [(2 * math.pi * k / 3 + 0.4, 0.22) for k in range(3)]
    cv.over(WHITE * 0.8, _broken_ring(cv, c, 46.0, 2.4, gaps) * 0.85)
    cv.over(WHITE, cv.cov(np.abs(np.hypot(cv.x - c, cv.y - c) - R) - 1.9))
    return cv.result()


# name → (painter, cells, blend the recipes use, description)
SPRITES = {
    "TelegraphRim": (paint_telegraph_rim, 1, "translucent",
                     "M_AF_Telegraph rim profile strip (row 0 = the boundary, tileable along the edge): scorched line "
                     "+ decaying 45-degree hatch"),
    "TelegraphFill": (paint_telegraph_fill, 1, "translucent",
                      "M_AF_Telegraph fill grain (tileable cracked ash, values .66-1, world XY)"),
    "FlameBreach": (paint_flame_breach, len(FLAME_BREACH_CELLS), "translucent",
                    "pre-coloured breach fire (violet-black, never ember): " +
                    ", ".join(f"{i} {n}" for i, n in enumerate(FLAME_BREACH_CELLS))),
    "AshFlake": (paint_ash_flake, len(ASHFLAKE_CELLS), "translucent",
                 "cel ash flakes (tint ash grey): " + ", ".join(f"{i} {n}" for i, n in enumerate(ASHFLAKE_CELLS))),
    "Ripple": (paint_ripple, 2, "additive", "bell sound ripple: two broken rings, outer at 0.4375 of the quad; "
                                            "0 standard, 1 fine (radii of metres)"),
    "RaiseSigil": (paint_raise_sigil, 1, "additive", "raise marker: ring + seven inward hooked fingers"),
    "AshNoise": (paint_ash_noise, 1, "translucent",
                 "M_AF_AshDissolve data: R fBm front, G crack mask, B per-plate random (tileable)"),
}


def sprite_table() -> dict:
    out = {}
    for name, (_, cells, blend, desc) in SPRITES.items():
        p = OUT / f"T_FX_{name}.png"
        size = None
        if p.exists():
            info = pngio.png_info(p)
            size = [info["width"], info["height"]]
        out[name] = {"cells": cells, "blend": blend, "texture": f"T_FX_{name}", "size": size, "note": desc,
                     "owner": "A12 (vfx/fx_textures_dark.py)"}
    return out


def write_sprite_json() -> None:
    """Merge the dark sprites into ``sprites.json`` (every other entry kept)."""
    path = HERE / "sprites.json"
    table = json.loads(path.read_text()) if path.exists() else {}
    for name, entry in sprite_table().items():
        cur = dict(table.get(name, {}))
        cur.update(entry)
        table[name] = cur
    path.write_text(json.dumps(dict(sorted(table.items())), indent=1) + "\n")


# ── preview sheet ───────────────────────────────────────────────────────────────────────────────────────
BACKS = (("MUD", "#3A332D"), ("ASH", "#4C4940"), ("STUDIO", "#1C1A20"))
TINTS = {"TelegraphRim": "#D11F2E", "TelegraphFill": "#FFE8DC", "FlameBreach": "#FFFFFF", "AshFlake": "#B8B2A6",
         "Ripple": "#B02FD0", "RaiseSigil": "#B02FD0", "AshNoise": "#FFFFFF"}


def sheet(names) -> list[Path]:
    from kit import review
    PREVIEW.mkdir(parents=True, exist_ok=True)
    rows = []
    for name in names:
        tex = pngio.read_png(OUT / f"T_FX_{name}.png")
        _, cells, blend, _ = SPRITES[name]
        h, w = tex.shape[:2]
        cw = w // cells
        scale = 2 if max(h, cw) <= 64 else 1
        add = blend == "additive"
        tiles = []
        if name == "AshNoise":
            for ch in range(3):
                g = (tex[..., ch:ch + 1].repeat(3, axis=2)).astype(np.uint8)
                tiles.append(g)
        else:
            for i in range(cells):
                cell = tex[:, i * cw:(i + 1) * cw]
                for _, bg in BACKS:
                    tiles.append(FT.composite(cell, bg, TINTS.get(name, "#FFFFFF"), add, scale))
        if name == "TelegraphRim":                      # tiled ×2 along U: the seam must not show
            two = np.concatenate([tex, tex], axis=1)
            tiles = [FT.composite(two, bg, TINTS[name], False, 1) for _, bg in BACKS]
        row = review.grid(tiles, min(len(tiles), max(1, 1180 // (tiles[0].shape[1] + 2))), pad=2)
        lab = np.zeros((12, max(row.shape[1], 240), 3), np.uint8)
        lab[:] = (24, 20, 30)
        review.draw_text(lab, 3, 3, f"{name} {w}X{h} {cells}C {'ADD' if add else 'TRANS'}  MUD ASH STUDIO")
        row = np.pad(row, ((0, 0), (0, lab.shape[1] - row.shape[1]), (0, 0)), constant_values=24)
        rows.append(np.vstack([lab, row]))
    width = max(r.shape[1] for r in rows)
    rows = [np.pad(r, ((0, 6), (0, width - r.shape[1]), (0, 0)), constant_values=18) for r in rows]
    pages, cur, cur_h = [], [], 0
    for r in rows:
        if cur and cur_h + r.shape[0] > 900:
            pages.append(cur)
            cur, cur_h = [], 0
        cur.append(r)
        cur_h += r.shape[0]
    if cur:
        pages.append(cur)
    for old in PREVIEW.glob("textures_*.png"):
        old.unlink()
    out = []
    for i, pg in enumerate(pages):
        p = PREVIEW / f"textures_{i + 1}.png"
        out += [q for q, _ in pngio.write_png_native(p, np.vstack(pg), axis=0, max_tiles=4)]
    for p in out:
        print(f"  {p}")
    return out


def write_material_contracts() -> Path:
    """``manifest.json → materials``: the art-side contracts of ``M_AF_Telegraph`` (U2) and ``M_AF_AshDissolve``
    (U3) from ``fx_recipes_dark`` (merged per key under the kit manifest's lock, like A10's ``MI_AF_Echo``)."""
    from kit import export
    import fx_recipes_dark as DK

    class MaterialsManifest(export.Manifest):
        MERGED = ("assets", "palettes", "gameIds", "materials")

    man = MaterialsManifest()
    for name, contract in (("M_AF_Telegraph", DK.TELEGRAPH), ("M_AF_AshDissolve", DK.ASH_DISSOLVE)):
        man.data["materials"][name] = json.loads(json.dumps(contract))
    return man.save()


def main(argv) -> int:
    if "--contracts" in argv:          # only the material contracts → manifest.json materials (no texture repaint)
        print(f"  {write_material_contracts()} (materials M_AF_Telegraph, M_AF_AshDissolve)")
        return 0
    sheet_only = "--sheet-only" in argv
    names = [a for a in argv if not a.startswith("--")] or list(SPRITES)
    OUT.mkdir(parents=True, exist_ok=True)
    if not sheet_only:
        for name in names:
            img = P.to_u8(SPRITES[name][0]())
            size = pngio.write_png(OUT / f"T_FX_{name}.png", img)
            print(f"T_FX_{name}: {img.shape[1]}x{img.shape[0]} {size / 1024:.1f} KB")
        write_sprite_json()
        print("  sprites.json (dark sprites merged)")
        print(f"  {write_material_contracts()} (materials M_AF_Telegraph, M_AF_AshDissolve)")
    sheet(list(SPRITES))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

"""Emerald Plains terrain texture set + water (spec art-inventory-ch1.md §6.1, world-map-nav.md §15.1; web
``GroundPainter.ts``, ``TerrainLattice.ts``, ``ZoneTerrain.ts`` quadMask; WorldContract §3.1–3.2).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/terrain.py [--no-previews]

Writes ``Art/Export/Terrain/``:

| Texture | Content |
|---|---|
| ``T_Terrain_Plains_Grass_BC`` / ``_Dirt_BC`` / ``_Stone_BC`` / ``_Camp_BC`` / ``_Water_BC`` | 1024² sRGB, one **3 × 3-tile period** of each ground material (web PERIOD 3: every tile is a window onto one periodic world, so tiles of a material always match at their edges). Painted from ``Data/terrain_styles.json`` exactly like ``GroundPainter`` — base, cel patches, tufts / pebbles / cracks / leaves / waves / Voronoi slabs — in **tile space** (no iso squash, render-only); upright motifs (grass tufts) are drawn leaning toward screen-up of the fixed W1 camera (+col +row) and stretched 1 / sin 50° so they read upright in the game view. Flat toon colour, no noise. |
| ``T_Terrain_Plains_Details_BC`` | 1024² RGBA, 4 × 4 cells of one tile each: row = paint index 0 grass / 1 dirt / 2 stone / 5 camp, column = detail variant 1..3 (col 4: water lily pads); the web's interior-only per-tile details (flowers, bigTuft, stones, crackStar, straw, lilyPads) with straight alpha |
| ``T_Terrain_Plains_Noise_M`` | 256² linear, R = the period-3 lattice noise ``latNoise`` (0..1 = −1..1) the transitions use (``MF_TileBlend``: score += 0.3 × noise), G = the same field for paint index 1 (offset per material as in ``AF_Terrain``), B = the cel patch field (``_pattern_block`` pf), A = 1 |
| ``T_Water_Plains_Mask_M`` | 512² linear, period 3: R = wave flecks (``#A9E2EE`` α .7), G = foam breakup noise for the shoreline band, B = patch mask (``#3B8AB0`` / ``#459AC0``) |

The UE material inputs are recorded in ``manifest.json → terrain.plains`` (written through ``envkit.EnvManifest``).
Previews (``Art/Previews/terrain_plains/``): ``layers.png`` (each texture tiled 2 × 2 periods), ``details.png``,
``transitions.png`` (a test map — camp, path, pond, stone, walls — composited with the exact web transition rules
(``AF_Terrain`` port) and rendered at the W1 game camera with the warrior), ``water.png``.
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import numpy as np  # noqa: E402

import envkit as E  # noqa: E402
from kit import paths, pngio  # noqa: E402

PERIOD = 3
RES = 1024
STYLES = json.loads((paths.UNREAL_ROOT / "Data" / "terrain_styles.json").read_text())["themes"]["plains"]
PX_TILE = 1.0 / 45.255           # web world px (ground) → tiles
UP = np.array([1.0, 1.0]) / math.sqrt(2.0)     # screen-up on the ground plane under the W1 camera (+col +row)
FORE = 1.0 / math.sin(math.radians(50.0))     # stretch of upright painted motifs (pitch −50°)
OUT = paths.export_root() / "Terrain"
SM = PX_TILE * 1.3               # small motifs (tufts, pebbles, leaves, flowers): +30 % so they keep ~2 texels / px
#                                  at the W1 default zoom (a tile is ≈ 120 px at 1080p vs the web's 96)


# ── web PRNG / hashes ───────────────────────────────────────────────────────────────────────────────────
def mulberry32(seed: int):
    state = [seed & 0xFFFFFFFF]

    def nxt() -> float:
        state[0] = (state[0] + 0x6D2B79F5) & 0xFFFFFFFF
        t = state[0]
        t = ((t ^ (t >> 15)) * (t | 1)) & 0xFFFFFFFF
        t ^= (t + (((t ^ (t >> 7)) * (t | 61)) & 0xFFFFFFFF)) & 0xFFFFFFFF
        return ((t ^ (t >> 14)) & 0xFFFFFFFF) / 4294967296.0
    return nxt


def tile_hash(col: int, row: int, salt: int = 0) -> int:
    h = (col * 374761393 + row * 668265263 + salt * 2246822519) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return (h ^ (h >> 16)) & 0xFFFFFFFF


def rgb(c: int) -> np.ndarray:
    return np.array([(c >> 16) & 255, (c >> 8) & 255, c & 255], np.float32)


def mix(a: int, b: int, t: float) -> np.ndarray:
    return rgb(a) + (rgb(b) - rgb(a)) * t


# ── canvas with periodic SDF drawing ────────────────────────────────────────────────────────────────────
class Canvas:
    """RGBA float canvas over ``span`` tiles (periodic when ``wrap``); shapes are signed-distance masks with a
    1-texel anti-aliased edge, drawn in local windows (and their periodic images)."""

    def __init__(self, res: int, span: float, base=None, wrap: bool = True, alpha: bool = False):
        self.res, self.span, self.wrap = res, span, wrap
        self.k = res / span                       # texels per tile
        self.img = np.zeros((res, res, 4), np.float32)
        if base is not None:
            self.img[:, :, :3] = base
            self.img[:, :, 3] = 1.0
        self.alpha = alpha

    def _windows(self, u: float, v: float, reach: float):
        offs = (-self.span, 0.0, self.span) if self.wrap else (0.0,)
        for du in offs:
            for dv in offs:
                cu, cv = u + du, v + dv
                i0 = int(math.floor((cu - reach) * self.k))
                i1 = int(math.ceil((cu + reach) * self.k))
                j0 = int(math.floor((cv - reach) * self.k))
                j1 = int(math.ceil((cv + reach) * self.k))
                i0c, i1c, j0c, j1c = max(i0, 0), min(i1, self.res), max(j0, 0), min(j1, self.res)
                if i0c >= i1c or j0c >= j1c:
                    continue
                ii = (np.arange(i0c, i1c) + 0.5) / self.k - cu
                jj = (np.arange(j0c, j1c) + 0.5) / self.k - cv
                X, Y = np.meshgrid(ii, jj)
                yield (slice(j0c, j1c), slice(i0c, i1c)), X, Y

    def paint(self, u, v, reach, sdf, color, alpha: float = 1.0):
        """``sdf(X, Y)`` (tiles, relative to (u, v)) < 0 inside."""
        aa = 0.75 / self.k
        c = np.asarray(color, np.float32)
        for sl, X, Y in self._windows(u, v, reach):
            d = sdf(X, Y)
            a = np.clip(0.5 - d / (2 * aa), 0.0, 1.0) * alpha
            if not a.any():
                continue
            dst = self.img[sl]
            dst[:, :, :3] = dst[:, :, :3] * (1 - a[:, :, None]) + c * a[:, :, None]
            dst[:, :, 3] = dst[:, :, 3] * (1 - a) + a
            self.img[sl] = dst

    def u8(self, with_alpha: bool = False) -> np.ndarray:
        out = self.img.copy()
        if with_alpha:
            rgbv = np.where(out[:, :, 3:4] > 1e-4, out[:, :, :3], 0)
            return np.dstack([rgbv, out[:, :, 3:4] * 255]).clip(0, 255).round().astype(np.uint8)
        return out[:, :, :3].clip(0, 255).round().astype(np.uint8)


# ── shape SDFs (tiles) ──────────────────────────────────────────────────────────────────────────────────
def sdf_blob(r: float, rand, squash: float = 1.0):
    """Wobbly 8-lobe blob (web blobAt): radius r × (0.72 + rand·0.5) at 8 angles, smooth periodic interpolation."""
    rot = rand[0] * math.tau
    radii = np.array([r * (0.72 + rand[(i + 1) % len(rand)] * 0.5) for i in range(8)])

    def f(X, Y):
        th = np.arctan2(Y / squash, X) - rot
        t = (th / math.tau * 8) % 8
        i0 = np.floor(t).astype(int) % 8
        fr = t - np.floor(t)
        w = (1 - np.cos(fr * math.pi)) / 2
        rr = radii[i0] * (1 - w) + radii[(i0 + 1) % 8] * w
        return np.hypot(X, Y / squash) - rr
    return f


def sdf_ellipse(rx: float, ry: float, rot: float = 0.0, cx: float = 0.0, cy: float = 0.0):
    c, s = math.cos(rot), math.sin(rot)

    def f(X, Y):
        x, y = X - cx, Y - cy
        xr, yr = x * c + y * s, -x * s + y * c
        q = np.hypot(xr / rx, yr / ry)
        return (q - 1.0) * min(rx, ry)
    return f


def sdf_stroke(pts, widths):
    """Polyline with per-point half widths (round caps): min over segments of distance − interpolated width."""
    pts = [np.asarray(p, np.float64) for p in pts]
    if isinstance(widths, (int, float)):
        widths = [widths] * len(pts)

    def f(X, Y):
        d = np.full(X.shape, 1e9, np.float32)
        for k in range(len(pts) - 1):
            a, b = pts[k], pts[k + 1]
            ab = b - a
            L2 = max(float(ab @ ab), 1e-12)
            t = np.clip(((X - a[0]) * ab[0] + (Y - a[1]) * ab[1]) / L2, 0, 1)
            px, py = a[0] + ab[0] * t, a[1] + ab[1] * t
            w = widths[k] + (widths[k + 1] - widths[k]) * t
            d = np.minimum(d, np.hypot(X - px, Y - py) - w)
        return d
    return f


def quad_curve(a, c, b, n: int = 8):
    a, c, b = (np.asarray(p, np.float64) for p in (a, c, b))
    return [a * (1 - t) ** 2 + c * 2 * (1 - t) * t + b * t * t for t in np.linspace(0, 1, n)]


# ── features (web featuresFor) ──────────────────────────────────────────────────────────────────────────
def layer_reach(layer) -> float:
    k = layer["kind"]
    if k == "patch":
        return layer["r"][1] * 1.3 / 16
    if k == "crack":
        return layer["len"][1] * 3.2 / 16
    if k == "wave":
        return layer["len"][1] * 0.7 / 16
    return 8 / 16


def features(layer, seed: int):
    r = mulberry32(seed)
    count = round(layer.get("count", 0) * PERIOD * PERIOD)
    grid = max(1, math.ceil(math.sqrt(max(count, 1))))
    cell = PERIOD / grid
    order = list(range(grid * grid))
    for i in range(len(order) - 1, 0, -1):
        j = int(math.floor(r() * (i + 1)))
        order[i], order[j] = order[j], order[i]
    out = []
    for i in range(count):
        g = order[i % len(order)]
        gx, gy = g % grid, g // grid
        u = (gx + r()) * cell
        v = (gy + r()) * cell
        _seed = math.floor(r() * 1e9)
        rand = [r() for _ in range(12)]
        out.append((u, v, rand))
    return out


def lerp(a, b, t):
    return a + (b - a) * t


def draw_tuft(cv: Canvas, u, v, h_px, color, light, blades, rand, scale: float = 1.0):
    """Web drawTuft: a contact shade + ``blades`` tapered blades; painted leaning toward screen-up (+col +row) and
    stretched by 1 / sin(pitch) so the motif reads upright from the W1 camera."""
    hp = h_px * SM * FORE * scale
    side = np.array([UP[1], -UP[0]])
    cv.paint(u, v, hp, sdf_ellipse(hp * 0.42, hp * 0.16, math.atan2(side[1], side[0])), rgb(color), 0.35)
    for i in range(blades):
        t = 0.0 if blades == 1 else i / (blades - 1) - 0.5
        bx = side * (t * hp * 0.5)
        lean = (t * 0.9 + (rand[i % len(rand)] - 0.5) * 0.5) * hp
        bh = hp * (0.7 + rand[(i + 3) % len(rand)] * 0.45) * (1 - abs(t) * 0.35)
        w = 0.55 * SM * 1.2 * scale
        tip = bx + side * (lean * 0.55) + UP * bh
        ctrl = bx + side * (lean * 0.12) + UP * (bh * 0.55)
        pts = quad_curve(bx, ctrl, tip, 6)
        widths = [w * (1 - k / 5) + 0.0005 for k in range(6)]
        cv.paint(u, v, hp * 1.6, sdf_stroke(pts, widths), rgb(color))
        if rand[(i + 6) % len(rand)] > 0.35:
            p2 = quad_curve(bx + (tip - bx) * 0.55 - side * w * 0.3, bx + (tip - bx) * 0.8 - side * w * 0.2, tip, 4)
            cv.paint(u, v, hp * 1.6, sdf_stroke(p2, [w * 0.35, w * 0.3, w * 0.25, w * 0.15]), rgb(light), 0.9)


def draw_pebble(cv: Canvas, u, v, size_px, color, shade, light, rot):
    rx = size_px * SM
    ry = rx * 0.62
    cv.paint(u, v, rx * 2, sdf_ellipse(rx, ry, rot, rx * 0.18, ry * 0.3), rgb(shade))
    cv.paint(u, v, rx * 2, sdf_ellipse(rx * 0.92, ry * 0.88, rot), rgb(color))
    cv.paint(u, v, rx * 2, sdf_ellipse(rx * 0.42, ry * 0.36, rot, -rx * 0.28, -ry * 0.32), rgb(light))


def crack_pts(u0, v0, length, rand, segs=3, wander=1.6):
    pts = [np.array([0.0, 0.0])]
    ang = rand[0] * math.tau
    for j in range(segs):
        ang += (rand[(j + 1) % len(rand)] - 0.5) * wander
        L = length * (0.5 + rand[(j + 4) % len(rand)] * 0.6) / segs * 1.4
        pts.append(pts[-1] + np.array([math.cos(ang) * L, math.sin(ang) * L]))
    return pts


def paint_layer(cv: Canvas, layer, feats):
    k = layer["kind"]
    for u, v, rand in feats:
        if k == "patch":
            cols = layer["colors"]
            col = cols[int(math.floor(rand[11] * len(cols)))]
            r = lerp(layer["r"][0], layer["r"][1], rand[10]) * PX_TILE
            cv.paint(u, v, r * 1.3, sdf_blob(r, rand), rgb(col), layer.get("alpha", 1.0))
        elif k == "tuft":
            draw_tuft(cv, u, v, lerp(layer["size"][0], layer["size"][1], rand[10]), layer["color"],
                      layer["light"], layer.get("blades", 3), rand)
        elif k == "pebble":
            draw_pebble(cv, u, v, lerp(layer["size"][0], layer["size"][1], rand[10]), layer["color"], layer["shade"],
                        layer["light"], (rand[9] - 0.5) * 0.8)
        elif k == "crack":
            L = lerp(layer["len"][0], layer["len"][1], rand[10]) * PX_TILE
            pts = crack_pts(u, v, L, rand)
            wpx = layer.get("width", 2) * PX_TILE
            if "light" in layer:
                off = np.array([0.8, 1.2]) * PX_TILE
                cv.paint(u, v, L * 2, sdf_stroke([p + off for p in pts], wpx * 0.4), rgb(layer["light"]), 0.8)
            cv.paint(u, v, L * 2, sdf_stroke(pts, wpx * 0.5), rgb(layer["color"]))
        elif k == "leaf":
            cols = layer["colors"]
            col = cols[int(math.floor(rand[11] * len(cols)))]
            sz = lerp(layer["size"][0], layer["size"][1], rand[10]) * SM
            cv.paint(u, v, sz * 1.2, sdf_ellipse(sz, sz * 0.42, rand[9] * math.pi), rgb(col))
        elif k == "wave":
            L = lerp(layer["len"][0], layer["len"][1], rand[10]) * PX_TILE
            a = np.array([-L / 2, 0.0])
            b = np.array([L / 2, 0.0])
            c = np.array([0.0, -L * 0.22])
            rot = rand[9] * 0.6 - 0.3 + math.pi / 4
            R = np.array([[math.cos(rot), -math.sin(rot)], [math.sin(rot), math.cos(rot)]])
            pts = [R @ p for p in quad_curve(a, c, b, 6)]
            cv.paint(u, v, L, sdf_stroke(pts, 0.3 * PX_TILE * 1.3), rgb(layer["color"]), layer.get("alpha", 0.7))


def paint_slabs(cv: Canvas, layer, base: int, seed: int):
    """Voronoi running-bond slabs with mortar and upper-left bevels (web paintSlabs), periodic, in tile space."""
    r = mulberry32(seed)
    n = round(layer["cells"] * PERIOD)
    seeds = []
    for j in range(n):
        for i in range(n):
            off = (j % 2) * 0.5
            seeds.append(((i + off + 0.5 + (r() - 0.5) * 0.7) / n * PERIOD, (j + 0.5 + (r() - 0.5) * 0.7) / n * PERIOD,
                          r()))
    S = np.array([(su, sv) for su, sv, _ in seeds])
    tones_ = np.array([t for _, _, t in seeds])
    cand, ctone = [], []
    for k in (-1, 0, 1):
        for m in (-1, 0, 1):
            cand.append(S + np.array([k * PERIOD, m * PERIOD]))
            ctone.append(tones_)
    C = np.concatenate(cand)
    T = np.concatenate(ctone)
    res = cv.res
    g = (np.arange(res) + 0.5) / cv.k
    U, V = np.meshgrid(g, g)
    P = np.stack([U.ravel(), V.ravel()], 1)
    d2 = ((P[:, None, :] - C[None, :, :]) ** 2).sum(2)
    idx = np.argsort(d2, axis=1)[:, :2]
    s1, s2 = C[idx[:, 0]], C[idx[:, 1]]
    e = s2 - s1
    el = np.maximum(np.linalg.norm(e, axis=1), 1e-9)
    mid = (s1 + s2) / 2
    edge = np.abs(((P - mid) * e).sum(1) / el)
    jit = layer["jitter"] / 0.06
    tone_cols = np.array([mix(base, 0x000000, 0.07 * jit), mix(base, 0x000000, 0.035 * jit), rgb(base),
                          mix(base, 0xFFFFFF, 0.035 * jit), mix(base, 0xFFFFFF, 0.07 * jit)])
    col = tone_cols[np.minimum(4, (T[idx[:, 0]] * 5).astype(int))]
    w = layer["mortarW"]
    # screen-space light from the upper-left: the edge toward the camera-left/top is the lit bevel
    sx, sy = (e[:, 0] - e[:, 1]) * 2, (e[:, 0] + e[:, 1])
    dot = (-sx - sy) * math.sqrt(0.5) / np.maximum(np.hypot(sx, sy), 1e-9)
    bevel = (edge >= w) & (edge < w * 2.6)
    col = np.where((bevel & (dot > 0.25))[:, None], rgb(layer["bevelLight"]), col)
    col = np.where((bevel & (dot < -0.25))[:, None], rgb(layer["bevelShade"]), col)
    col = np.where((edge < w)[:, None], rgb(layer["mortar"]), col)
    cv.img[:, :, :3] = col.reshape(res, res, 3)
    cv.img[:, :, 3] = 1.0


def paint_material(paint_idx: str, res: int = RES) -> np.ndarray:
    st = STYLES["ground"][paint_idx]
    cv = Canvas(res, PERIOD, rgb(st["base"]))
    seed = 1013 + int(paint_idx) * 7907
    for i, layer in enumerate(st["layers"]):
        if layer["kind"] == "slabs":
            paint_slabs(cv, layer, st["base"], seed + i * 7919)
            continue
        paint_layer(cv, layer, features(layer, seed + i * 7919))
    return cv.u8()


# ── interior details (web paintGroundDetails) ───────────────────────────────────────────────────────────
def tuft_colors(st):
    t = next((l for l in st["layers"] if l["kind"] == "tuft"), None)
    if t:
        return t["color"], t["light"]
    return int.from_bytes(bytes(mix(st["base"], 0, 0.25).astype(np.uint8)), "big"), \
        int.from_bytes(bytes(mix(st["base"], 0xFFFFFF, 0.25).astype(np.uint8)), "big")


def pebble_colors(st):
    p = next((l for l in st["layers"] if l["kind"] == "pebble"), None)
    if p:
        return p["color"], p["shade"], p["light"]
    to = lambda a: int.from_bytes(bytes(a.round().astype(np.uint8)), "big")  # noqa: E731
    return to(mix(st["base"], 0xFFFFFF, 0.12)), to(mix(st["base"], 0x1A1030, 0.3)), to(mix(st["base"], 0xFFFFFF, 0.3))


def paint_detail(cv: Canvas, kind: str, st, r, cx: float, cy: float):
    """One interior motif round the tile centre (cx, cy) (tiles), web paintDetail in tile space."""
    x = cx + (r() - 0.5) * 0.36
    y = cy + (r() - 0.5) * 0.36
    acc = st["accents"] or [0xFFFFFF]
    pick = lambda: acc[int(math.floor(r() * len(acc)))]  # noqa: E731
    P = SM
    if kind == "flowers":
        leaf, _ = tuft_colors(st)
        n = 3 + int(math.floor(r() * 4))
        color = pick()
        for i in range(n):
            fx, fy = x + (r() - 0.5) * 9 * P, y + (r() - 0.5) * 9 * P
            cv.paint(fx, fy, 0.08, sdf_ellipse(1.1 * P, 0.5 * P, -0.4, -0.9 * P, 0.9 * P), rgb(leaf))
            c = color if r() < 0.75 else pick()
            cv.paint(fx, fy, 0.08, sdf_ellipse(1.25 * P, 1.25 * P, 0, 0.35 * P, 0.35 * P), mix(c, 0x402040, 0.35))
            cv.paint(fx, fy, 0.08, sdf_ellipse(1.2 * P, 1.2 * P), rgb(c))
            cv.paint(fx, fy, 0.08, sdf_ellipse(0.45 * P, 0.45 * P, 0, -0.2 * P, -0.2 * P), mix(c, 0xFFF4C0, 0.6))
    elif kind == "bigTuft":
        c, l = tuft_colors(st)
        rr = [r() for _ in range(12)]
        draw_tuft(cv, x - 1.5 * P, y, 6 + r() * 2, c, l, 5, rr)
        draw_tuft(cv, x + 2 * P, y + 1.2 * P, 4.5 + r() * 1.5, c, l, 3, rr[3:] + rr[:3])
    elif kind == "stones":
        c, sh, l = pebble_colors(st)
        n = 2 + int(math.floor(r() * 2))
        for i in range(n):
            draw_pebble(cv, x + (r() - 0.5) * 8 * P, y + (r() - 0.5) * 8 * P, 1.8 + r() * 1.6, c, sh, l,
                        (r() - 0.5) * 0.6)
    elif kind == "crackStar":
        dark = mix(st["base"], 0x140A20, 0.35)
        n = 3 + int(math.floor(r() * 3))
        for i in range(n):
            rand = [i / n + r() * 0.1, r(), r(), r(), r(), r(), r()]
            pts = crack_pts(x, y, (4 + r() * 4) * P, rand, 2)
            cv.paint(x, y, 0.2, sdf_stroke(pts, 0.35 * P), dark)
    elif kind == "straw":
        c = acc[0]
        n = 5 + int(math.floor(r() * 5))
        for i in range(n):
            sx, sy = x + (r() - 0.5) * 10 * P, y + (r() - 0.5) * 10 * P
            ang = (r() - 0.5) * 1.4 + math.pi / 4
            L = (2 + r() * 2.5) * P
            col = rgb(c) if r() < 0.5 else mix(c, 0x6A4A20, 0.3)
            cv.paint(sx, sy, L, sdf_stroke([np.zeros(2), np.array([math.cos(ang) * L, math.sin(ang) * L])], 0.25 * P),
                     col)
    elif kind == "lilyPads":
        leaf = acc[0]
        n = 1 + int(math.floor(r() * 3))
        for i in range(n):
            lx, ly = x + (r() - 0.5) * 8 * P, y + (r() - 0.5) * 8 * P
            lr = (1.6 + r() * 1.2) * P
            notch = r() * math.tau

            def pad(X, Y, lr=lr, notch=notch):
                d = np.hypot(X, Y) - lr
                a = np.arctan2(Y, X) - notch
                a = (a + math.pi) % math.tau - math.pi
                return np.where(np.abs(a) < 0.25, np.maximum(d, 0.01), d)
            cv.paint(lx + 0.3 * P, ly + 0.35 * P, lr * 1.5, pad, mix(leaf, 0x102030, 0.4))
            cv.paint(lx, ly, lr * 1.5, pad, rgb(leaf))
            if len(acc) > 1 and r() < 0.4:
                cv.paint(lx, ly - 0.3 * P, 0.05, sdf_ellipse(0.8 * P, 0.8 * P), rgb(acc[1]))


DETAIL_ROWS = ("0", "1", "2", "5")


def paint_details(res: int = RES) -> np.ndarray:
    cell = res // 4
    atlas = Canvas(res, 4.0, None, wrap=False)
    for row, key in enumerate(DETAIL_ROWS):
        st = STYLES["ground"][key]
        for variant in (1, 2, 3):
            r = mulberry32(1013 * 31 + variant * 977 + (int(key) * 3 + variant) * 131 + 17)
            n = 1 if r() < 0.75 else 2
            for _ in range(n):
                kind = st["details"][int(math.floor(r() * len(st["details"])))]
                paint_detail(atlas, kind, st, r, variant - 1 + 0.5, row + 0.5)
    water = STYLES["ground"]["3"]
    r = mulberry32(4242)
    for k in range(4):
        paint_detail(atlas, "lilyPads", water, r, 3.5, k + 0.5)
    return atlas.u8(with_alpha=True)


# ── noise / patch fields (the AF_Terrain maths, baked) ──────────────────────────────────────────────────
NOISE_FREQS_P3 = ((1, 2), (2, -1), (3, 1), (-1, 3), (4, 2), (2, 5), (5, -3), (-4, 5), (7, 2), (3, 7), (8, -5), (-6, 8))


def lattice_noise(u: np.ndarray, v: np.ndarray) -> np.ndarray:
    """Web latNoise (period 3), normalised to −1..1 with the sampled table's min / max (buildNoise)."""
    r = mulberry32(9173 + PERIOD)
    waves = []
    for k, l in NOISE_FREQS_P3:
        mag = math.hypot(k, l)
        waves.append((k, l, 1.0 / mag ** 0.9, r() * math.pi * 2))
    n = 40 * PERIOD
    gu, gv = np.meshgrid(np.arange(n) / 40.0, np.arange(n) / 40.0)

    def field(U, V):
        s = np.zeros_like(U, dtype=np.float64)
        for k, l, amp, ph in waves:
            s += amp * np.sin(2 * math.pi * (k * U + l * V) / PERIOD + ph)
        return s
    t = field(gu, gv)
    lo, hi = t.min(), t.max()
    return (field(u, v) - lo) / (hi - lo) * 2 - 1


def patch_field(u, v, m: float):
    qu, qv = u + 0.5 + m * 0.73, v + 0.5 + m * 0.29
    return (np.sin(2.0943951 * (2 * qu + qv) + 1.3 + m * 1.7) + 0.8 * np.sin(2.0943951 * (3 * qv - qu) + 4.1 + m * 0.9)
            + 0.5 * np.sin(2.0943951 * (4 * qu - 2 * qv) + 2.2 - m * 1.3)) * 0.4347826


def paint_noise(res: int = 256) -> np.ndarray:
    g = (np.arange(res) + 0.5) / res * PERIOD
    U, V = np.meshgrid(g, g)
    out = np.zeros((res, res, 4), np.float32)
    out[:, :, 0] = lattice_noise(U + 0.5, V + 0.5) * 0.5 + 0.5
    out[:, :, 1] = lattice_noise(U + 0.5 + 0.37, V + 0.5 - 0.61) * 0.5 + 0.5
    out[:, :, 2] = np.clip(patch_field(U, V, 0.0) * 0.5 + 0.5, 0, 1)
    out[:, :, 3] = 1.0
    return (out * 255).round().astype(np.uint8)


def paint_water_mask(res: int = 512) -> np.ndarray:
    st = STYLES["ground"]["3"]
    waves = Canvas(res, PERIOD, np.zeros(3))
    patches = Canvas(res, PERIOD, np.zeros(3))
    for i, layer in enumerate(st["layers"]):
        feats = features(layer, 1013 + 3 * 7907 + i * 7919)
        if layer["kind"] == "wave":
            lay = dict(layer, color=0xFFFFFF, alpha=1.0)
            paint_layer(waves, lay, feats)
        elif layer["kind"] == "patch":
            lay = dict(layer, colors=[0xFFFFFF], alpha=1.0)
            paint_layer(patches, lay, feats)
    g = (np.arange(res) + 0.5) / res * PERIOD
    U, V = np.meshgrid(g, g)
    foam = lattice_noise(U * 2 % PERIOD, V * 2 % PERIOD) * 0.5 + 0.5
    out = np.zeros((res, res, 4), np.float32)
    out[:, :, 0] = waves.img[:, :, 0] / 255.0
    out[:, :, 1] = foam
    out[:, :, 2] = patches.img[:, :, 0] / 255.0
    out[:, :, 3] = 1.0
    return (out * 255).round().clip(0, 255).astype(np.uint8)


# ── composite (Python port of AF_Terrain over a tile map) — previews / vignette ground ──────────────────
class Layers:
    def __init__(self, tex: dict, details: np.ndarray):
        self.tex = {k: v.astype(np.float32) for k, v in tex.items()}
        self.details = details.astype(np.float32)

    def sample(self, key: str, u, v):
        t = self.tex[key]
        n = t.shape[0]
        i = ((u / PERIOD) % 1.0 * n).astype(int) % n
        j = ((v / PERIOD) % 1.0 * n).astype(int) % n
        return t[j, i, :3]


def composite(tiles: np.ndarray, layers: Layers, ppt: int = 48, origin=(0, 0)) -> np.ndarray:
    """``tiles[row, col]`` paint ids (0 grass 1 dirt 2 stone 3 water 5 camp; 4 wall → grass, 6 camp wall → camp) →
    sRGB image (rows × ppt, cols × ppt) using the exact web transition rules (spec §6.1 / hlsl.terrain_node)."""
    rows, cols = tiles.shape
    paint = tiles.copy()
    paint[paint == 4] = 0
    paint[paint == 6] = 5
    H, W = rows * ppt, cols * ppt
    gy, gx = np.meshgrid((np.arange(H) + 0.5) / ppt - 0.5, (np.arange(W) + 0.5) / ppt - 0.5, indexing="ij")
    gx += origin[0]
    gy += origin[1]
    b0x, b0y = np.floor(gx).astype(int), np.floor(gy).astype(int)
    frx, fry = gx - b0x, gy - b0y
    ms, ws = [], []
    for k in range(4):
        ox, oy = k & 1, k >> 1
        cx = np.clip(b0x + ox - origin[0], 0, cols - 1)
        cy = np.clip(b0y + oy - origin[1], 0, rows - 1)
        m = paint[cy, cx]
        du = (1 - frx) if ox else frx
        dv = (1 - fry) if oy else fry
        d2 = du * du + dv * dv
        w = np.where(d2 < 1, (1 - d2) ** 2, 0.0)
        ms.append(m)
        ws.append(w)
    ms, ws = np.stack(ms), np.stack(ws)
    sw = np.maximum(ws.sum(0), 1e-5)
    ranks = {int(k): v["rank"] for k, v in STYLES["ground"].items()}
    rank_lut = np.zeros(8)
    for k, v in ranks.items():
        rank_lut[k] = v
    f = np.zeros_like(ws)
    for k in range(4):
        f[k] = sum(np.where(ms[j] == ms[k], ws[j], 0.0) for j in range(4)) / sw
    same = (ms == ms[0:1]).all(0)
    sc = np.full_like(ws, -1000.0)
    for k in range(4):
        rk = rank_lut[ms[k]]
        nz = lattice_noise(gx + 0.5 + ms[k] * 0.37, gy + 0.5 - ms[k] * 0.61)
        nz = np.where(same, 0.0, nz)
        sc[k] = np.where(ws[k] > 0, f[k] + rk * 0.025 + 0.3 * nz, -1000.0)
    order = np.argmax(sc, 0)
    mb = np.take_along_axis(ms, order[None], 0)[0]
    s1 = np.take_along_axis(sc, order[None], 0)[0]
    fb = np.take_along_axis(f, order[None], 0)[0]
    sc2 = np.where((ms != mb[None]) & (ws > 0), sc, -1000.0)
    o2 = np.argmax(sc2, 0)
    msec = np.take_along_axis(ms, o2[None], 0)[0]
    s2 = np.take_along_axis(sc2, o2[None], 0)[0]
    single = s2 < -999
    msec = np.where(single, mb, msec)
    dlt = np.where(single, 1.0, s1 - s2)
    al = np.where(dlt >= 0.025, 1.0, 0.5 + dlt / 0.05)
    keymap = {0: "0", 1: "1", 2: "2", 3: "3", 5: "5"}

    def pattern(m):
        out = np.zeros(gx.shape + (3,), np.float32)
        for key in np.unique(m):
            sel = m == key
            out[sel] = layers.sample(keymap.get(int(key), "0"), gx[sel], gy[sel])
        return out
    c1, c2 = pattern(mb), pattern(msec)
    # own-tile interior details (variant = tileHash(c, r, 7) % 12 >= 6 → 1 + h % 3)
    own_x, own_y = np.round(gx).astype(int), np.round(gy).astype(int)
    det = np.zeros(gx.shape + (4,), np.float32)
    cell = layers.details.shape[0] // 4
    xs0, ys0 = own_x.min(), own_y.min()
    htab = np.array([[tile_hash(int(c), int(r), 7) for c in range(xs0, own_x.max() + 1)]
                     for r in range(ys0, own_y.max() + 1)], np.int64)
    hs = htab[own_y - ys0, own_x - xs0]
    var = hs % 12
    has = var >= 6
    vidx = 1 + (hs % 3)
    rowmap = {0: 0, 1: 1, 2: 2, 5: 3, 3: -1}
    own_m = paint[np.clip(own_y - origin[1], 0, rows - 1), np.clip(own_x - origin[0], 0, cols - 1)]
    lu = gx - own_x + 0.5
    lv = gy - own_y + 0.5
    for m_key, rrow in rowmap.items():
        sel = has & (own_m == m_key) & (mb == m_key)
        if not sel.any():
            continue
        if rrow < 0:
            ci = (3 * cell + lu[sel] * cell).astype(int).clip(0, 4 * cell - 1)
            rj = ((vidx[sel] - 1) * cell + lv[sel] * cell).astype(int).clip(0, 4 * cell - 1)
        else:
            ci = ((vidx[sel] - 1) * cell + lu[sel] * cell).astype(int).clip(0, 4 * cell - 1)
            rj = (rrow * cell + lv[sel] * cell).astype(int).clip(0, 4 * cell - 1)
        det[sel] = layers.details[rj, ci]
    c1 = c1 * (1 - det[:, :, 3:4] / 255.0) + det[:, :, :3] * (det[:, :, 3:4] / 255.0)
    col = c2 + (c1 - c2) * al[:, :, None]
    liq = STYLES["ground"]["3"]["liquid"]
    shallow, foam, bank = rgb(liq["shallow"]), rgb(liq["foam"]), rgb(liq["bank"])
    lip_of = {int(k): rgb(v.get("lip", 0)) if "lip" in v else (rgb(v["base"]) + (np.array([255, 242, 214]) - rgb(v["base"])) * 0.25)
              for k, v in STYLES["ground"].items()}
    rb = rank_lut[mb]
    rs = rank_lut[msec]
    ns = ~single
    wat = ns & (mb == 3)
    t = (1 - np.clip((fb - 0.5) / 0.45, 0, 1) ** 2 * (3 - 2 * np.clip((fb - 0.5) / 0.45, 0, 1))) * 0.75
    col = np.where(wat[:, :, None], col + (shallow - col) * t[:, :, None], col)
    fm = wat & (dlt < 0.065)
    ft = np.where(fm, np.maximum(1 - dlt / 0.065, 0) ** 0.6 * 0.92, 0)
    col = col + (foam - col) * ft[:, :, None]
    bk = ns & (msec == 3) & (mb != 3)
    bt = np.where(bk & (dlt < 0.045), 0.7 * (1 - dlt / 0.045),
                  np.where(bk & (dlt < 0.1), 0.25 * (1 - (dlt - 0.045) / 0.055), 0))
    col = col + (bank - col) * bt[:, :, None]
    lipm = ns & (mb != 3) & (msec != 3) & (rb > rs) & (dlt < 0.032)
    lipc = np.zeros_like(col)
    for k, v in lip_of.items():
        lipc[mb == k] = v
    col = np.where(lipm[:, :, None], col + (lipc - col) * 0.75, col)
    dk = ns & (mb != 3) & (msec != 3) & (rb < rs) & (dlt < 0.055)
    dt = np.where(dk, 0.2 * (1 - dlt / 0.055), 0)
    col = col + (rgb(0x241A3A) - col) * dt[:, :, None]
    return col.clip(0, 255).round().astype(np.uint8)


# ── build ───────────────────────────────────────────────────────────────────────────────────────────────
TEXTURES = {"0": "T_Terrain_Plains_Grass_BC", "1": "T_Terrain_Plains_Dirt_BC", "2": "T_Terrain_Plains_Stone_BC",
            "3": "T_Water_Plains_BC", "5": "T_Terrain_Plains_Camp_BC"}


def build_textures(res: int = RES) -> dict:
    OUT.mkdir(parents=True, exist_ok=True)
    tex = {}
    for key, name in TEXTURES.items():
        img = paint_material(key, res)
        tex[key] = img
        pngio.write_png(OUT / f"{name}.png", img)
        print(f"  {OUT / (name + '.png')}")
    det = paint_details(res)
    pngio.write_png(OUT / "T_Terrain_Plains_Details_BC.png", det)
    pngio.write_png(OUT / "T_Terrain_Plains_Noise_M.png", paint_noise())
    pngio.write_png(OUT / "T_Water_Plains_Mask_M.png", paint_water_mask())
    return {"tex": tex, "details": det}


def load_textures() -> dict:
    tex = {k: pngio.read_png(OUT / f"{n}.png")[:, :, :3] for k, n in TEXTURES.items()}
    det = pngio.read_png(OUT / "T_Terrain_Plains_Details_BC.png")
    return {"tex": tex, "details": det}


def manifest_section() -> dict:
    g = STYLES["ground"]
    hexs = lambda c: "#%06X" % c  # noqa: E731
    layers = {}
    for key, name in TEXTURES.items():
        st = g[key]
        layers[key] = {
            "name": {"0": "grass", "1": "dirt", "2": "stone", "3": "water", "5": "camp"}[key],
            "texture": name, "file": f"Terrain/{name}.png", "srgb": True, "rank": st["rank"],
            "base": hexs(st["base"]), "lip": hexs(st["lip"]) if "lip" in st else None,
            "accents": [hexs(a) for a in st["accents"]], "details": st["details"]}
    return {"plains": {
        "material": "M_AF_Terrain (+ M_AF_Water)",
        "source": "Data/terrain_styles.json themes.plains (same colours as the runtime Tile<i>* vectors)",
        "periodTiles": PERIOD,
        "uv": "UV0 = tile space (col, row; tile centre at integers); layer textures repeat every 3 tiles: "
              "sample at UV0 / 3, wrap, bilinear + mips (flat toon colour, no noise)",
        "layers": layers,
        "details": {"texture": "T_Terrain_Plains_Details_BC", "file": "Terrain/T_Terrain_Plains_Details_BC.png",
                    "srgb": True, "alpha": "straight",
                    "cells": "4 x 4 cells of one tile: row 0 grass, 1 dirt, 2 stone, 3 camp (paint 5); column = "
                             "variant - 1 (variants 1..3); column 3 = water lily-pad motifs (rows = variant - 1)",
                    "rule": "own tile (nearest centre) h = tileHash(col, row, 7): h % 12 >= 6 -> variant 1 + h % 3; "
                            "sample cell at frac(UV0 + 0.5) (tile-local 0..1); composite over the winning material "
                            "only when it is the own tile's material (AF_Terrain _pattern_block own_var / own_m)"},
        "noise": {"texture": "T_Terrain_Plains_Noise_M", "file": "Terrain/T_Terrain_Plains_Noise_M.png", "srgb": False,
                  "channels": {"r": "latNoise(u + .5, v + .5) * .5 + .5 (period 3) — transition wobble: score += "
                                    "0.3 * (2r - 1) with u, v offset by (m * .37, -m * .61) per material m",
                               "g": "the same field pre-offset for m = 1",
                               "b": "cel patch field pf (AF_Terrain _pattern_block, m = 0) * .5 + .5",
                               "a": "1"},
                  "use": "mobile / low quality: replaces the 12-sine analytic noise of AF_Terrain"},
        "transitions": [
            "per pixel: the 4 tiles round its tile corner, w = (1 - d^2)^2 (d = distance to the tile centre), "
            "normalised per material",
            "score s = w_norm + rank * 0.025 + 0.3 * noise_m; winner t1, runner-up t2, delta = s1 - s2",
            "coverage a = 1 if delta >= 0.025 else 0.5 + delta / 0.05 (crisp anti-aliased cel edge)",
            "t1 liquid: shallow tint (1 - smoothstep(.5, .95, w1)) * .75 toward WaterShallow; foam WaterFoam where "
            "delta < .065: (1 - delta / .065)^.6 * .92",
            "t2 liquid: bank WaterBank .7 * (1 - delta / .045) for delta < .045, then .25 * (1 - (delta - .045) / "
            ".055) up to .1",
            "rank1 > rank2: lip (Tile<i>Lip, else base + 25 % toward #FFF2D6) at .75 for delta < .032",
            "rank1 < rank2: dark edge #241A3A at .2 * (1 - delta / .055) for delta < .055",
            "walls paint the dominant ground (grass), camp walls camp ground; vertex G (wall coverage) darkens x "
            "(.62,.66,.6) * .45; vertex B (skirt) * .86"],
        "water": {"material": "M_AF_Water", "surfaceZcm": -12, "bedZcm": -60, "bankSlopeTiles": 0.3,
                  "base": hexs(g["3"]["base"]), "shallow": hexs(g["3"]["liquid"]["shallow"]),
                  "foam": hexs(g["3"]["liquid"]["foam"]), "bank": hexs(g["3"]["liquid"]["bank"]),
                  "wave": "#A9E2EE", "texture": "T_Water_Plains_BC",
                  "mask": {"texture": "T_Water_Plains_Mask_M", "file": "Terrain/T_Water_Plains_Mask_M.png",
                           "srgb": False, "channels": {"r": "wave flecks (#A9E2EE x .7), scroll UV0/3 + t*(.05,.03)",
                                                       "g": "foam breakup noise: foam = smoothstep(.55, .62, shore + "
                                                            "(g - .5) * .2)", "b": "patch mask (#3B8AB0/#459AC0)",
                                                       "a": "1"}},
                  "lilyPads": "SM_Foliage_Plains_LilyPad_A/B on ~1 in 6 water tiles (game id lily_pad_plains)"},
        "params": {f"Tile{k}{f}": v for k in ("0", "1", "2", "3", "5") for f, v in
                   (("Base", hexs(g[k]["base"])), ("Rank", g[k]["rank"]))},
        "previews": ["Previews/terrain_plains/layers.png", "Previews/terrain_plains/details.png",
                     "Previews/terrain_plains/transitions.png", "Previews/terrain_plains/water.png"],
    }}


def test_map() -> np.ndarray:
    """A 14 × 12 test map: camp ground + palisade corner, a dirt path, a pond with shore, a stone patch, walls."""
    m = np.zeros((12, 14), np.int32)
    m[1:6, 1:7] = 5
    m[1, 1:7] = 6
    m[1:6, 1] = 6
    for c in range(3, 14):
        r = 6 + int(round(1.2 * math.sin(c * 0.6)))
        m[r, c] = 1
        m[r + 1, c] = 1 if c % 3 else m[r + 1, c]
    m[4:6, 3:5] = 1
    m[8:11, 2:6] = 3
    m[9, 6] = 3
    m[2:4, 10:13] = 2
    m[0, :] = 4
    m[:, 13] = 4
    return m


def previews(tex: dict) -> None:
    import envkit  # noqa: F401  (bpy only needed for the game-camera render)
    from kit import review
    out = paths.preview_root() / "terrain_plains"
    out.mkdir(parents=True, exist_ok=True)
    cells = []
    for key in ("0", "1", "2", "5", "3"):
        t = tex["tex"][key]
        tile = np.tile(t, (2, 2, 1))[::4, ::4]
        review.draw_text(tile, 4, 4, f"{TEXTURES[key]}  2X2 PERIODS (6 TILES)")
        cells.append(tile)
    pngio.write_png_budget(out / "layers.png", review.grid(cells, 3, pad=2))
    det = tex["details"]
    bg = np.zeros_like(det[:, :, :3])
    bg[:, :] = (60, 52, 70)
    a = det[:, :, 3:4] / 255.0
    comp = (det[:, :, :3] * a + bg * (1 - a)).astype(np.uint8)[::2, ::2].copy()
    review.draw_text(comp, 4, 4, "DETAILS ATLAS: ROWS GRASS DIRT STONE CAMP / COLS VARIANT 1-3 + LILY")
    pngio.write_png_budget(out / "details.png", comp)
    layers = Layers(tex["tex"], tex["details"])
    m = test_map()
    img = composite(m, layers, ppt=64)
    pngio.write_png(E.SCRATCH / "terrain_composite.png", img)
    render_ground(img, m.shape, out)


def render_ground(img: np.ndarray, shape, out: Path) -> None:
    """The composited test map on a ground plane at the W1 camera (default distance, 1080p crop) with the
    warrior standing on the camp ground."""
    import bpy
    from mathutils import Vector
    from kit import scene, review, shading
    scene.reset()
    rows, cols = shape
    E.SCRATCH.mkdir(parents=True, exist_ok=True)
    p = E.SCRATCH / "terrain_ground.png"
    pngio.write_png(p, img)
    me = bpy.data.meshes.new("ground")
    # tile (c, r) centre → Blender (c, −r): UE Y = row, Blender Y = −UE Y
    x0, x1 = -0.5, cols - 0.5
    y0, y1 = 0.5, -(rows - 0.5)
    me.from_pydata([(x0, y0, 0), (x1, y0, 0), (x1, y1, 0), (x0, y1, 0)], [], [(0, 1, 2, 3)])
    uv = me.uv_layers.new(name="UVMap")
    for li, (u, v) in enumerate(((0, 1), (1, 1), (1, 0), (0, 0))):
        uv.data[li].uv = (u, v)
    ob = bpy.data.objects.new("ground", me)
    scene.link(ob)
    mat = bpy.data.materials.new("ground")
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    tx = nt.nodes.new("ShaderNodeTexImage")
    tx.image = bpy.data.images.load(str(p))
    tx.interpolation = "Linear"
    em = nt.nodes.new("ShaderNodeEmission")
    o = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(tx.outputs["Color"], em.inputs["Color"])
    nt.links.new(em.outputs[0], o.inputs["Surface"])
    me.materials.append(mat)
    w = E.import_warrior(at=(3.5, -3.0, 0), yaw_deg=-135)
    review.setup_render((1920, 1080), 8)
    cam = review.game_camera((cols / 2 - 0.5, -(rows / 2 - 0.5)))
    if w:
        for mm in w[1]:
            from kit import outline
            review.set_outline_px([mm], outline.SCREEN_PX_1080[mm.get("af_outline_class", "hero")])
    full = review.render_array()

    def crop_at(col, row, w, h):
        x, y = review.project_px(cam, Vector((col, -row, 0.0)), (1920, 1080))
        x0 = int(min(max(x - w / 2, 0), 1920 - w))
        y0 = int(min(max(y - h / 2, 0), 1080 - h))
        return full[y0:y0 + h, x0:x0 + w].copy()
    tr = crop_at(cols / 2 - 0.5, rows / 2 - 0.5, 1040, 600)
    review.draw_text(tr, 6, 6, "TERRAIN PLAINS TEST MAP: CAMP GROUND, PATH, POND, STONE, WALL GROUND  W1 1080P 1:1")
    pngio.write_png_budget(out / "transitions.png", tr)
    # water close crop (shallows, foam, bank) round the pond (rows 8-10, cols 2-6)
    wa = crop_at(4.0, 9.2, 640, 400)
    review.draw_text(wa, 6, 6, "WATER: SHALLOW TINT, FOAM BAND, BANK, WAVE FLECKS, LILY PADS  1080P 1:1")
    pngio.write_png_budget(out / "water.png", wa)
    print(f"  {out / 'transitions.png'}")


def main(argv: list[str]) -> int:
    tex = build_textures()
    man = E.EnvManifest()
    man.data["terrain"] = manifest_section()
    man.save()
    if "--no-previews" not in argv:
        previews(tex)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

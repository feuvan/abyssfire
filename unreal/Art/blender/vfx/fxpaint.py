"""Signed-distance painting toolkit for the toon VFX textures (``T_FX_*``) — pure numpy, no bpy.

Every shape is a signed distance field in *output* pixels (negative inside). A ``Canvas`` evaluates fields on a
supersampled grid (``ss``), turns them into anti-aliased coverage, composites RGBA layers and box-filters down to
the output size. The web painters (``src/graphics/vfx/FxTextures.ts``) draw with canvas paths, gradients and
``shadowBlur``; here the same shapes are fields, so a band (cel step), an outline stroke or a glow falloff is just a
function of the distance — crisp toon edges with exact AA and no noise.

Conventions (UE ``M_AF_FX_*``, WorldContract 5.4): RGBA, **linear** values (the textures import with sRGB off), RGB
is the multiplier the instance colour tints (white-ish = tintable; pre-coloured cells store linear colour), A is the
coverage. Image row 0 is the quad's +Y edge: billboards' screen-up, velocity quads' head, upright quads' top.
"""
from __future__ import annotations

import math
from typing import Callable, Iterable, Sequence

import numpy as np

Field = np.ndarray


# ── colour ─────────────────────────────────────────────────────────────────────────────────────────────
def hex_rgb(h: str) -> np.ndarray:
    s = h.lstrip("#")
    return np.array([int(s[i:i + 2], 16) for i in (0, 2, 4)], np.float32) / 255.0


def srgb_to_lin(c):
    c = np.asarray(c, np.float32)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4).astype(np.float32)


def lin_to_srgb(c):
    c = np.clip(np.asarray(c, np.float32), 0.0, None)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1.0 / 2.4) - 0.055).astype(np.float32)


def lin(h: str) -> np.ndarray:
    """'#RRGGBB' (sRGB) → linear RGB (what a pre-coloured texel stores so a white tint shows the hex)."""
    return srgb_to_lin(hex_rgb(h))


def mix(a, b, t):
    return np.asarray(a, np.float32) + (np.asarray(b, np.float32) - np.asarray(a, np.float32)) * t


# ── deterministic random ───────────────────────────────────────────────────────────────────────────────
def rng(seed: int):
    """mulberry32 (same generator family as the web painters' ``rng``)."""
    s = [seed & 0xFFFFFFFF]

    def r() -> float:
        s[0] = (s[0] + 0x6D2B79F5) & 0xFFFFFFFF
        t = s[0]
        t = ((t ^ (t >> 15)) * (t | 1)) & 0xFFFFFFFF
        t ^= (t + (((t ^ (t >> 7)) * (t | 61)) & 0xFFFFFFFF)) & 0xFFFFFFFF
        return ((t ^ (t >> 14)) & 0xFFFFFFFF) / 4294967296.0
    return r


# ── canvas ─────────────────────────────────────────────────────────────────────────────────────────────
class Canvas:
    """RGBA float canvas of ``w × h`` output pixels, evaluated ``ss``× supersampled.

    ``x`` / ``y`` are the sample positions in output-pixel units (y down, origin top-left), so every field and
    width below is in output pixels."""

    def __init__(self, w: int, h: int, ss: int = 4):
        self.w, self.h, self.ss = w, h, ss
        ys, xs = np.mgrid[0:h * ss, 0:w * ss].astype(np.float32)
        self.x = (xs + 0.5) / ss
        self.y = (ys + 0.5) / ss
        self.rgb = np.zeros((h * ss, w * ss, 3), np.float32)
        self.a = np.zeros((h * ss, w * ss), np.float32)
        self.aa = 0.75          # coverage ramp width (output px)

    # coverage of a field (negative inside); ``soft`` widens the ramp (feathered edge, output px)
    def cov(self, d: Field, soft: float = 0.0) -> Field:
        k = self.aa + soft
        return np.clip(0.5 - d / k, 0.0, 1.0)

    def over(self, color, alpha: Field) -> None:
        """Paint ``color`` (linear RGB or HxWx3) with coverage ``alpha`` over the canvas (premultiplied over)."""
        a = np.clip(alpha, 0.0, 1.0)
        c = np.asarray(color, np.float32)
        if c.ndim == 1:
            c = c[None, None, :]
        # premultiplied over: C = c·a + C·(1−a), A = a + A·(1−a); stored un-premultiplied at the end
        pre = self.rgb * self.a[..., None]
        pre = c * a[..., None] + pre * (1.0 - a[..., None])
        self.a = a + self.a * (1.0 - a)
        self.rgb = np.where(self.a[..., None] > 1e-6, pre / np.maximum(self.a[..., None], 1e-6), 0.0)

    def under(self, color, alpha: Field) -> None:
        """Paint behind what is already there (glows / halos under a crisp body)."""
        a = np.clip(alpha, 0.0, 1.0)
        c = np.asarray(color, np.float32)
        if c.ndim == 1:
            c = c[None, None, :]
        pre_top = self.rgb * self.a[..., None]
        pre = pre_top + c * a[..., None] * (1.0 - self.a[..., None])
        na = self.a + a * (1.0 - self.a)
        self.rgb = np.where(na[..., None] > 1e-6, pre / np.maximum(na[..., None], 1e-6), 0.0)
        self.a = na

    def multiply_alpha(self, m: Field) -> None:
        self.a = self.a * np.clip(m, 0, 1)

    def result(self) -> np.ndarray:
        """Box-filter to the output size → HxWx4 float (straight alpha, RGB linear)."""
        ss = self.ss
        a = self.a.reshape(self.h, ss, self.w, ss).mean(axis=(1, 3))
        pre = (self.rgb * self.a[..., None]).reshape(self.h, ss, self.w, ss, 3).mean(axis=(1, 3))
        rgb = np.where(a[..., None] > 1e-5, pre / np.maximum(a[..., None], 1e-5), 0.0)
        return np.concatenate([np.clip(rgb, 0, 1), np.clip(a, 0, 1)[..., None]], axis=2).astype(np.float32)


def to_u8(img: np.ndarray) -> np.ndarray:
    """Float RGBA → uint8; RGB under zero alpha is bled from the neighbours so mips / bilinear never pull black
    fringes into a tinted edge."""
    rgb, a = img[..., :3].copy(), img[..., 3]
    mask = a > 0.002
    if (~mask).any() and mask.any():
        filled = rgb * mask[..., None]
        wsum = mask.astype(np.float32)
        for _ in range(6):
            f = np.pad(filled, ((1, 1), (1, 1), (0, 0)), mode="edge")
            w = np.pad(wsum, 1, mode="edge")
            acc = sum(f[1 + dy:f.shape[0] - 1 + dy, 1 + dx:f.shape[1] - 1 + dx] for dy in (-1, 0, 1)
                      for dx in (-1, 0, 1))
            wac = sum(w[1 + dy:w.shape[0] - 1 + dy, 1 + dx:w.shape[1] - 1 + dx] for dy in (-1, 0, 1)
                      for dx in (-1, 0, 1))
            grow = (wsum == 0) & (wac > 0)
            filled = np.where(grow[..., None], acc / np.maximum(wac[..., None], 1e-6), filled)
            wsum = np.where(grow, 1.0, wsum)
        rgb = np.where(mask[..., None], rgb, np.where(wsum[..., None] > 0, filled, 1.0))
    out = np.concatenate([rgb, a[..., None]], axis=2)
    return (np.clip(out, 0, 1) * 255.0 + 0.5).astype(np.uint8)


# ── fields ─────────────────────────────────────────────────────────────────────────────────────────────
def circle(cv: Canvas, cx: float, cy: float, r: float) -> Field:
    return np.hypot(cv.x - cx, cv.y - cy) - r


def ellipse(cv: Canvas, cx: float, cy: float, rx: float, ry: float, rot: float = 0.0) -> Field:
    """Approximate ellipse distance (exact on the axes; good for AA and bands)."""
    c, s = math.cos(rot), math.sin(rot)
    dx, dy = cv.x - cx, cv.y - cy
    u, v = dx * c + dy * s, -dx * s + dy * c
    k = np.hypot(u / rx, v / ry)
    g = np.hypot(u / (rx * rx), v / (ry * ry))
    return (k - 1.0) * k / np.maximum(g, 1e-6) * 1.0


def segment(cv: Canvas, ax, ay, bx, by) -> Field:
    """Distance to segment AB (unsigned)."""
    px, py = cv.x - ax, cv.y - ay
    vx, vy = bx - ax, by - ay
    t = np.clip((px * vx + py * vy) / max(vx * vx + vy * vy, 1e-9), 0, 1)
    return np.hypot(px - vx * t, py - vy * t)


def stroke(cv: Canvas, pts: Sequence[tuple[float, float]], widths) -> Field:
    """Variable-width polyline (round joins / caps): distance − half width, widths per point."""
    if np.isscalar(widths):
        widths = [widths] * len(pts)
    d = np.full(cv.x.shape, 1e9, np.float32)
    for (ax, ay), (bx, by), wa, wb in zip(pts[:-1], pts[1:], widths[:-1], widths[1:]):
        px, py = cv.x - ax, cv.y - ay
        vx, vy = bx - ax, by - ay
        t = np.clip((px * vx + py * vy) / max(vx * vx + vy * vy, 1e-9), 0, 1)
        dd = np.hypot(px - vx * t, py - vy * t) - (wa + (wb - wa) * t) * 0.5
        d = np.minimum(d, dd)
    return d


def polygon(cv: Canvas, pts: Sequence[tuple[float, float]]) -> Field:
    """Signed distance to a closed polygon (even-odd inside)."""
    n = len(pts)
    d = np.full(cv.x.shape, 1e9, np.float32)
    inside = np.zeros(cv.x.shape, bool)
    for i in range(n):
        ax, ay = pts[i]
        bx, by = pts[(i + 1) % n]
        d = np.minimum(d, segment(cv, ax, ay, bx, by))
        cond = ((ay > cv.y) != (by > cv.y)) & (cv.x < (bx - ax) * (cv.y - ay) / ((by - ay) if by != ay else 1e-9) + ax)
        inside ^= cond
    return np.where(inside, -d, d)


def smooth_union(a: Field, b: Field, k: float) -> Field:
    h = np.clip(0.5 + 0.5 * (b - a) / k, 0, 1)
    return b + (a - b) * h - k * h * (1 - h)


def union(*fs: Field) -> Field:
    out = fs[0]
    for f in fs[1:]:
        out = np.minimum(out, f)
    return out


def subtract(a: Field, b: Field) -> Field:
    return np.maximum(a, -b)


def intersect(a: Field, b: Field) -> Field:
    return np.maximum(a, b)


def polar(cv: Canvas, cx: float, cy: float):
    dx, dy = cv.x - cx, cv.y - cy
    return np.hypot(dx, dy), np.arctan2(dy, dx)


def star(cv: Canvas, cx: float, cy: float, r_out: float, r_in: float, points: int, rot: float = 0.0,
         curve: float = 0.0) -> Field:
    """Star polygon (``points`` tips); ``curve`` > 0 pinches the edges inward (concave four-point glints)."""
    pts = []
    m = 2 * points
    for i in range(m):
        a = rot + i * math.pi / points - math.pi / 2
        r = r_out if i % 2 == 0 else r_in
        pts.append((cx + math.cos(a) * r, cy + math.sin(a) * r))
    if curve <= 0:
        return polygon(cv, pts)
    # concave: subdivide every edge and pull the midpoints toward the centre
    dense = []
    for i in range(m):
        ax, ay = pts[i]
        bx, by = pts[(i + 1) % m]
        for k in range(6):
            t = k / 6.0
            x, y = ax + (bx - ax) * t, ay + (by - ay) * t
            pull = curve * math.sin(math.pi * t)
            x, y = x + (cx - x) * pull, y + (cy - y) * pull
            dense.append((x, y))
    return polygon(cv, dense)


def wobble_circle(cv: Canvas, cx: float, cy: float, r: float, amp: float, lobes: int, seed: int,
                  squash: float = 1.0) -> Field:
    """A hand-drawn blob: radius modulated by a few low harmonics (deterministic)."""
    rr = rng(seed)
    harm = [(k, amp * (0.5 + rr()) / k ** 0.6, rr() * 6.283) for k in range(2, 2 + lobes)]
    d, a = polar(cv, cx, cy)
    d = np.hypot(cv.x - cx, (cv.y - cy) / squash)
    rad = np.full_like(d, r)
    for k, am, ph in harm:
        rad = rad + r * am * np.cos(k * a + ph)
    return d - rad


def bezier(p0, p1, p2, p3=None, n: int = 16) -> list[tuple[float, float]]:
    out = []
    for i in range(n + 1):
        t = i / n
        if p3 is None:
            x = (1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0]
            y = (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1]
        else:
            x = ((1 - t) ** 3 * p0[0] + 3 * (1 - t) ** 2 * t * p1[0] + 3 * (1 - t) * t * t * p2[0]
                 + t ** 3 * p3[0])
            y = ((1 - t) ** 3 * p0[1] + 3 * (1 - t) ** 2 * t * p1[1] + 3 * (1 - t) * t * t * p2[1]
                 + t ** 3 * p3[1])
        out.append((x, y))
    return out


# ── falloffs ───────────────────────────────────────────────────────────────────────────────────────────
def stops(t: Field, table: Sequence[tuple[float, float]]) -> Field:
    xs, ys = zip(*table)
    return np.interp(t, xs, ys).astype(np.float32)


def halo(d: Field, width: float, power: float = 1.6) -> Field:
    """Glow outside a shape: 1 at the edge → 0 at ``width`` px (eased)."""
    t = np.clip(d / max(width, 1e-6), 0.0, 1.0)
    return np.where(d <= 0, 1.0, (1.0 - t) ** power).astype(np.float32)


def steps(v: Field, levels: Sequence[float], edges: Sequence[float], aa: float = 0.02) -> Field:
    """Cel-band a 0..1 ramp: value ``levels[i]`` between ``edges[i-1]`` and ``edges[i]`` (smoothed by ``aa``)."""
    out = np.full(v.shape, levels[0], np.float32)
    for lv, e in zip(levels[1:], edges):
        k = np.clip((v - e) / aa + 0.5, 0, 1)
        out = out + (lv - out) * k
    return out


def blur(a: np.ndarray, sigma: float) -> np.ndarray:
    """Separable Gaussian (edge-clamped) for 2-D or HxWxC arrays."""
    if sigma <= 0:
        return a
    r = max(1, int(math.ceil(sigma * 3)))
    x = np.arange(-r, r + 1, dtype=np.float32)
    k = np.exp(-0.5 * (x / sigma) ** 2)
    k /= k.sum()

    def conv(arr, axis):
        pad = [(0, 0)] * arr.ndim
        pad[axis] = (r, r)
        p = np.pad(arr, pad, mode="edge")
        out = np.zeros_like(arr)
        for i, w in enumerate(k):
            sl = [slice(None)] * arr.ndim
            sl[axis] = slice(i, i + arr.shape[axis])
            out += w * p[tuple(sl)]
        return out
    return conv(conv(a.astype(np.float32), 0), 1)


def strip(cells: Iterable[np.ndarray]) -> np.ndarray:
    """Horizontal strip of equal cells (WorldContract 5.4: the instance Variant picks the cell)."""
    return np.concatenate(list(cells), axis=1)

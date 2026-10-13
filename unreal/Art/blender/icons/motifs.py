"""Motif building blocks for skill emblems, glyphs and emblems (web ``SkillIcons.ts`` / ``UiGlyphs.ts`` helpers as
3-D toon models): authored in the web's 96-unit coordinates (``W(x, y)``, 1 u = 1 cm, y down; depth ``d`` in units,
negative = toward the viewer) so compositions port one to one.

``Comp`` collects modelled parts (on an icon palette) and placed shipped meshes (weapons, FX meshes) into one
render; ``flame`` / ``swoosh`` / ``arrow`` / ``skull`` / ``heart`` / ``cloud`` / ``bolt`` / ``crystal`` /
``star`` / ``ring2d`` / ``drop`` … add the web's recurring shapes.
"""
from __future__ import annotations

import math
from typing import Sequence

from mathutils import Matrix, Vector

import iconkit as K
import shapes as S
from item_models import W, Wp, Wb, U, _mix
from kit import mesh as M
from kit.palette import Region


def R(h, s=.42, l=.40, e=0.0, o=1.0) -> Region:
    return Region(h, s, l, e, o)


def glowr(h, e=.6, l=.45) -> Region:
    return Region(h, .25, l, e)


class Comp:
    """One motif: modelled parts (regions on ``pal``) + already-placed shipped meshes."""

    def __init__(self, name: str, pal: K.IconPalette):
        self.name, self.pal = name, pal
        self.parts: list[tuple] = []
        self.regs: dict[str, Region] = {}
        self.objs: list = []
        self.acc: list = []
        self.style: dict = {}

    def region(self, key: str, reg: Region) -> str:
        if key in self.regs and self.regs[key] != reg:
            raise ValueError(f"{self.name}: region {key} redefined")
        self.regs[key] = reg
        return key

    def add(self, part, key: str, reg: Region | None = None):
        if part is None:
            return None
        if reg is not None:
            k, n = key, 2
            while k in self.regs and self.regs[k] != reg:      # same tag, other values: its own region
                k, n = f"{key}_{n}", n + 1
            self.regs[k] = reg
            key = k
        elif key not in self.regs:
            raise KeyError(f"{self.name}: region {key} used before it was defined")
        self.parts.append((part, key))
        return part

    def obj(self, o):
        self.objs.append(o)
        return o

    def build(self, nolines_keys: Sequence[str] = ()) -> list:
        out = list(self.objs)
        if self.parts:
            main = [(p, k) for p, k in self.parts if k not in nolines_keys]
            nol = [(p, k) for p, k in self.parts if k in nolines_keys]
            if main:
                out.append(K.build_model(self.name, self.pal, {k: self.regs[k] for _, k in main}, main))
            if nol:
                out.append(K.build_model(self.name + "_nl", self.pal, {k: self.regs[k] for _, k in nol}, nol,
                                         nolines=True))
        return out


def at(x, y, d=0.0) -> Vector:
    """web (x, y) + depth d (units, − = toward the viewer) → model point."""
    px, pz = W(x, y)
    return Vector((px, d * U, pz))


def pill(pts_web, depth=4.0, back=None, d=0.0, edge=.45, dome=.3, wall=None, smooth=False) -> M.Part:
    """Pillow from web points (``smooth`` = Catmull-Rom through them) at depth ``d``."""
    pts = S.blob2d(Wp(pts_web), 6) if smooth else Wp(pts_web)
    p = S.pillow(pts, depth=depth * U, back=(depth * .35 if back is None else back) * U,
                 wall=(depth * .3 if wall is None else wall) * U, edge=edge, dome=dome)
    return p.translate((0, d * U, 0))


def pill2(pts_model, depth=4.0, back=None, d=0.0, edge=.45, dome=.3, wall=None) -> M.Part:
    p = S.pillow(pts_model, depth=depth * U, back=(depth * .35 if back is None else back) * U,
                 wall=(depth * .3 if wall is None else wall) * U, edge=edge, dome=dome)
    return p.translate((0, d * U, 0))


def xform2d(pts_web, cx, cy, rot_rad=0.0, s=1.0):
    """Web points relative to (0, 0) → rotated (canvas: + = clockwise) / scaled / moved to (cx, cy)."""
    c, sn = math.cos(rot_rad), math.sin(rot_rad)
    return [(cx + (x * c - y * sn) * s, cy + (x * sn + y * c) * s) for x, y in pts_web]


# ── recurring shapes ────────────────────────────────────────────────────────────────────────────────────
FLAME_COLS = ("#E8401C", "#FF9A22", "#FFE070")


def flame(c: Comp, x, y, h, w, lean=0.0, d=0.0, rot=0.0, cols=FLAME_COLS, tag="fl"):
    """web flame(): three nested layers (red / orange / yellow), base at (x, y), height h, half-width w."""
    for i, (s, col) in enumerate(zip((1.0, .72, .42), cols)):
        hh, ww = h * s, w * s
        tip = (lean * hh, -hh)
        pts = S.chain2d(S.bez([(-ww, -ww * .4), (-ww * 1.1, -hh * .45), (-ww * .2 + lean * hh * .4, -hh * .6), tip], 10),
                        S.bez([tip, (ww * .5 + lean * hh * .5, -hh * .55), (ww * 1.2, -hh * .4), (ww, -ww * .4)], 10),
                        S.bez([(ww, -ww * .4), (0, ww * .9), (-ww, -ww * .4)], 8))
        pts = xform2d(pts, x, y, rot)
        c.add(pill(pts, depth=3.0 + i, d=d - i * 2.2, edge=.6, dome=.3), f"{tag}{i}",
              R(col, .3, .45, e=(.25, .45, .7)[i]))


def swoosh(c: Comp, cx, cy, r, a0, a1, w, col="#FFF0C8", d=-2.0, tag="sw"):
    """web swoosh(): tapered crescent along an arc (head at a1)."""
    n = 24
    pts = [(cx + math.cos(a0 + (a1 - a0) * t) * r, cy + math.sin(a0 + (a1 - a0) * t) * r) for t in [i / n for i in range(n + 1)]]
    left, right = [], []
    for i in range(n + 1):
        t = i / n
        a = a0 + (a1 - a0) * t
        v = w * (t ** .8) * ((1 - t) ** .25)
        nx, ny = math.cos(a), math.sin(a)
        if a1 > a0:
            left.append(pts[i]); right.append((pts[i][0] - nx * v, pts[i][1] - ny * v))
        else:
            left.append(pts[i]); right.append((pts[i][0] + nx * v, pts[i][1] + ny * v))
    outline = left + list(reversed(right))
    c.add(pill(outline, depth=2.5, d=d, edge=.7, dome=.2), tag, R(col, .25, .6, e=.35))


def ring2d(c: Comp, cx, cy, rx, ry, w, col, d=0.0, tag="ring", a0=0.0, a1=360.0, e=.4, depth=1.5):
    """An elliptical band (web stroked ellipse / arc) as a flat cushioned ribbon."""
    n = max(8, int(abs(a1 - a0) / 6))
    pts = [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * i / n)), cy + ry * math.sin(math.radians(a0 + (a1 - a0) * i / n)))
           for i in range(n + 1)]
    if abs(a1 - a0) >= 359.9:
        part = ring3(rx, ry, w, depth)
        part.translate(at(cx, cy, d))
        c.add(part, tag, R(col, .3, .5, e=e))
        return
    outline = S.ribbon2d(Wp(pts), lambda t: w / 2 * U * (0.35 + 0.65 * math.sin(math.pi * t) ** .5))
    c.add(pill2(outline, depth=depth, d=d, edge=.8), tag, R(col, .3, .5, e=e))


def ring3(rx: float, ry: float, w: float, depth: float, n: int = 64) -> M.Part:
    """A closed elliptical band in the screen plane (web units): tube of width ``w`` and depth ``depth``."""
    path = [Vector((rx * U * math.cos(2 * math.pi * i / n), 0.0, ry * U * math.sin(2 * math.pi * i / n))) for i in range(n)]
    prof = M.superellipse(depth * .5 * U, w * .5 * U, 8, 2.4)
    return M.sweep(path, prof, up=[(0, -1, 0)] * n, closed_path=True).smooth(True)


def arrow(c: Comp, a, b, w=1.0, head="#D0D8E2", fletch="#E8E0D0", shaft="#9A6A3A", d=0.0, tag="ar", glow_head=0.0):
    """web arrow(): shaft, steel head, two fletchings (screen-plane, cushioned)."""
    ang = math.atan2(b[1] - a[1], b[0] - a[0])
    L = math.hypot(b[0] - a[0], b[1] - a[1])

    def T(pts):  # local (x along the arrow, tip at 0) → web
        return xform2d(pts, b[0], b[1], ang)
    sh = T(S.capsule2d((-L, 0), (-6 * w, 0), 1.9 * w))
    c.add(pill(sh, depth=1.9 * w, d=d, edge=.9, dome=.1), f"{tag}_shaft", R(shaft, .42, .38))
    for s in (-1, 1):
        f = T([(-L + 1, 0), (-L - 3 * w, s * 6 * w), (-L + 9 * w, s * 5.5 * w), (-L + 10 * w, 0)])
        c.add(pill(f, depth=1.2, d=d - .5, edge=.7), f"{tag}_fl", R(fletch, .42, .4))
    hd = T([(2 * w, 0), (-9 * w, -5 * w), (-6.5 * w, 0), (-9 * w, 5 * w)])
    c.add(pill(hd, depth=2.0 * w, d=d - .8, edge=.6, dome=.5), f"{tag}_head", R(head, .38, .55, e=glow_head))


def heart_pts(x, y, s):
    """web heart(): bezier heart, tip at (x, y + 26 s)."""
    return S.chain2d(S.bez([(x, y + 26 * s), (x - 36 * s, y + 2 * s), (x - 26 * s, y - 26 * s), (x, y - 12 * s)], 14),
                     S.bez([(x, y - 12 * s), (x + 26 * s, y - 26 * s), (x + 36 * s, y + 2 * s), (x, y + 26 * s)], 14))


def cloud_pts(x, y, s):
    return S.blob2d([(x - 30 * s, y + 8 * s), (x - 32 * s, y - 6 * s), (x - 18 * s, y - 14 * s), (x - 8 * s, y - 24 * s),
                     (x + 10 * s, y - 22 * s), (x + 20 * s, y - 12 * s), (x + 32 * s, y - 8 * s), (x + 32 * s, y + 8 * s)], 6)


def cloud(c: Comp, x, y, s, col, d=0.0, tag="cl", depth=9.0, lobes=True):
    """A cloud: a cushioned silhouette plus three domed lumps on its top (cel bands on each lump)."""
    c.add(pill(cloud_pts(x, y, s), depth=depth * s, d=d, edge=.6, dome=.4), tag, R(col, .42, .42))
    if lobes:
        for (lx, ly, lr) in ((-12, -10, 11), (6, -14, 12), (20, -4, 9)):
            lump = M.ellipsoid((lr * s * U, lr * .55 * s * U, lr * s * U * .9), (0, 0, 0), sides=20, rings=10)
            lump.translate(at(x + lx * s, y + ly * s, d - depth * s * .55))
            c.add(lump, tag)


def skull(c: Comp, x, y, s, eye="#100810", col="#ECE2C6", d=0.0, tag="sk", eye_e=0.0):
    """web skull(): cranium + cheek / jaw, dark (or glowing) eye sockets, nose, teeth lines."""
    head = S.chain2d(S.bez([(x - 16 * s, y + 4 * s), (x - 20 * s, y - 22 * s), (x + 20 * s, y - 22 * s), (x + 16 * s, y + 4 * s)], 16),
                     [(x + 10 * s, y + 10 * s), (x + 9 * s, y + 18 * s), (x - 9 * s, y + 18 * s), (x - 10 * s, y + 10 * s)])
    c.add(pill(head, depth=9 * s, d=d, edge=.55, dome=.35), tag, R(col, .42, .42))
    for sx in (-1, 1):
        eye_pts = S.circle2d(5 * s, 16, x + sx * 7 * s, y - 1 * s, rz=5.6 * s)
        c.add(pill(eye_pts, depth=1.2, d=d - 9 * s * .78, edge=.8), f"{tag}_eye",
              R(eye, .2, .3, e=eye_e) if eye_e else R(eye, .2, .05))
    nose = [(x, y + 5 * s), (x + 2.6 * s, y + 9 * s), (x - 2.6 * s, y + 9 * s)]
    c.add(pill(nose, depth=1.0, d=d - 9 * s * .74, edge=.8), f"{tag}_dark", R("#100810", .2, .05))
    for i in (-1, 0, 1):
        tl = S.capsule2d((x + i * 4 * s, y + 12.5 * s), (x + i * 4 * s, y + 17.5 * s), .55 * s)
        c.add(pill(tl, depth=.8, d=d - 9 * s * .52, edge=.9), f"{tag}_dark")


def bolt(c: Comp, pts, w, d=0.0, tag="bo", col="#FFF08A"):
    """web bolt(): a tapering zig-zag ribbon (emissive yellow)."""
    outline = S.ribbon2d(pts, lambda t: w * (1 - t * .6))
    c.add(pill(outline, depth=2.4, d=d, edge=.7), tag, R(col, .25, .6, e=.55))


def crystal(c: Comp, x, y, h, w, rot=0.0, d=0.0, col="#8AD8FF", tag="cr", e=.15):
    """web freeze crystal: a hexagonal prism with a pointed top, base at (x, y), leaning ``rot`` rad (flat shaded)."""
    import bmesh
    hx = w * U
    H = h * U
    rings = []
    for z, k in ((0.0, .85), (H - hx, 1.0), (H, 0.0)):
        rings.append([(k * hx * math.cos(math.radians(30 + 60 * i)), k * hx * .7 * math.sin(math.radians(30 + 60 * i)), z) for i in range(6)])
    p = M.loft_z([(0.0, hx * .85, hx * .6), (H - hx * 1.3, hx, hx * .7), (H - .001, hx * .08, hx * .06)], sides=6,
                 cap0="fan", cap1="fan", dome1=hx * .02).flat()
    p.rotate("y", math.degrees(rot))
    p.translate(at(x, y, d))
    c.add(p, tag, R(col, .35, .55, e=e))


def star(c: Comp, x, y, n, r1, r2, col, d=0.0, rot=-90.0, depth=3.0, tag="st", e=.3, l=.5):
    pts = S.star2d(n, r1, r2, rot=-rot, cx=x, cz=y)
    # star2d works in model-like coords (y up); convert from web coords
    pts = [(px, py) for px, py in pts]
    c.add(pill(pts, depth=depth, d=d, edge=.6, dome=.5), tag, R(col, .3, l, e=e))


def drop(c: Comp, x, y, s, col, d=0.0, tag="dr", e=.2):
    """web drip: teardrop pointing up, (x, y) = its centre."""
    pts = S.chain2d(S.bez([(x, y - 8 * s), (x + 6 * s, y), (x, y + 4 * s)], 8), S.bez([(x, y + 4 * s), (x - 6 * s, y), (x, y - 8 * s)], 8))
    c.add(pill(pts, depth=3.0 * s, d=d, edge=.7, dome=.4), tag, R(col, .3, .5, e=e))


def snowflake(c: Comp, x, y, r, col="#EAF8FF", d=-6.0, tag="sf"):
    for i in range(6):
        a = math.pi / 3 * i
        ca, sa = math.cos(a), math.sin(a)

        def rot(px, py):
            return (x + px * ca - py * sa, y + px * sa + py * ca)
        arm = S.capsule2d(rot(0, 0), rot(0, -r), r * .09)
        c.add(pill(arm, depth=1.0, d=d, edge=.9), tag, R(col, .2, .5, e=.5))
        for sx in (-1, 1):
            tw = S.capsule2d(rot(0, -r * .5), rot(sx * r * .28, -r * .72), r * .07)
            c.add(pill(tw, depth=.8, d=d, edge=.9), tag)


def speed_lines(c: Comp, segs, w=1.2, col="#FFFFFF", d=-4.0, tag="spd"):
    for a, b in segs:
        c.add(pill(S.capsule2d(a, b, w, w * .4), depth=1.0, d=d, edge=.9), tag, R(col, .2, .5, e=.5))


def weapon(c: Comp, family: str, tier: str, x, y, rot_rad, s, d=0.0, spin=-12.0, widen: float = 1.0):
    """A shipped ``SM_Weapon_<family>`` (tier palette) placed like the web ``blade(c, key, …, x, y, rot, s)``:
    grip origin at (x, y), the blade pointing up rotated ``rot`` (canvas radians, + = clockwise)."""
    pal = "Weapons" if tier == "T1" else f"Weapons_{tier}"
    o = K.import_fbx("Weapons", f"SM_Weapon_{family.capitalize()}", K.ShippedPalette(pal))
    ref = {"sword": 0.54 / 0.875, "dagger": 0.38 / 0.378, "axe": 0.70 / 0.727, "staff": 0.80 / 0.916,
           "shield": 0.62 / 0.58, "bow": 0.84 / 0.972, "mace": 0.6 / 0.612, "wand": .5 / .327}[family]
    if family == "shield":
        K.place(o, toward=(0, 1, 0), up=(0, 0, 1), roll=math.degrees(rot_rad), at=at(x, y, d), scale=s * ref, yaw=10)
    elif family == "bow":
        K.place(o, toward=(-1, 0, 0), up=(0, 0, 1), roll=math.degrees(rot_rad), at=at(x, y, d), scale=s * ref)
    else:
        K.place(o, toward=(1, 0, 0), up=(0, 1, 0), roll=math.degrees(rot_rad), at=at(x, y, d), scale=s * ref, spin=spin)
    if widen != 1.0:            # emblem readability: a broader blade / guard (local X, Z) at the same length
        o.matrix_world = o.matrix_world @ Matrix.Diagonal((widen, 1.0, widen, 1.0))
    return c.obj(o)


def hood_pts():
    return S.chain2d(S.bez([(48, 10), (74, 12), (80, 40), (76, 62)], 10), S.bez([(76, 62), (70, 70), (62, 66)], 4),
                     S.bez([(62, 66), (56, 76), (48, 70)], 4), S.bez([(48, 70), (40, 78), (32, 66)], 4),
                     S.bez([(32, 66), (24, 70), (20, 62)], 4), S.bez([(20, 62), (16, 40), (22, 12), (48, 10)], 10))


def fx(c: Comp, asset: str, tint: str, x, y, d=0.0, scale=1.0, toward=(0, -1, 0), up=(0, 0, 1), roll=0.0, yaw=0.0,
       pitch=0.0, l=.45, e=.2):
    o = K.import_fx(asset, tint, l=l, e=e)
    K.place(o, toward=toward, up=up, roll=roll, yaw=yaw, pitch=pitch, at=at(x, y, d), scale=scale)
    return c.obj(o)


def ribbon3(c: Comp, pts_web, width, depth=2.0, d=0.0, tag="rb", reg: Region | None = None, sides: int = 8):
    """A long curved ribbon (spirals, wisps, tails): a flat elliptic tube swept along a web-space path — unlike a
    pillow it never fills the inside of a curve. ``width(t)`` = half-width in units."""
    pts = [at(x, y, d) for x, y in pts_web]
    n = len(pts)
    ws = [(width(i / (n - 1)) if callable(width) else width) for i in range(n)]
    prof = M.superellipse(1.0, 1.0, sides, 2.2)
    part = M.sweep(pts, prof, scales=[(depth * .5 * U, max(w, .05) * U) for w in ws], up=[(0, -1, 0)] * n,
                   dome0=0.0, dome1=0.0).smooth(True)
    c.add(part, tag, reg)
    return part

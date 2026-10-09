"""Geometry helpers shared by the Chapter-1 monster generators (``monsters/*.py``).

* :func:`grid_loft` — a closed tube from any ``point(u, a)`` surface function (rings of angles ``a`` along ``u``),
  with per-vertex ``af_u`` / ``af_a`` layers so a loft can be split into regions afterwards (:func:`split_by`).
* :class:`SpineLoft` — a torso / skirt / mantle surface lofted along a curved spine (the goblins' hunched back):
  rings ``(s, lat, fwd, centre_fwd)`` interpolated with Catmull-Rom, ``point(s, a, out)`` / ``normal(s, a)`` like
  ``heroes/common.Profile`` (a = 0 front, +90 the character's left).
* :func:`surface_strap` / :func:`surface_ring` — straps, belts and seams that lie on such a surface (flat on it,
  so they band with it under the toon light), :func:`flap` — two-sided ragged cloth panels.
* :func:`leaf` — feathers, ears and blades as bevelled leaf plates; :func:`cone` small claws / teeth / spikes.

Body space = armature space: X = character left, −Y = forward, Z = up (metres).
"""
from __future__ import annotations

import math
from typing import Callable, Sequence

import bmesh
import numpy as np
from mathutils import Matrix, Vector

from kit import mesh as M, rig as R

import common as C


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def smooth01(x: float) -> float:
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


# ── lofts from surface functions ───────────────────────────────────────────────────────────────────────
def grid_loft(fn: Callable[[float, float], Vector], us: Sequence[float], angles: Sequence[float],
              cap0: str | None = "fan", cap1: str | None = "fan", dome0: float = 0.0, dome1: float = 0.0,
              closed: bool = True) -> M.Part:
    """Rings ``fn(u, a)`` for every ``u`` (bottom → top) and angle ``a`` (degrees, increasing = counter-clockwise
    seen from the +u end); fan caps pushed ``dome*`` along the end tangents. Vertex float layers ``af_u`` /
    ``af_a`` keep each vertex's surface parameters (``split_by``). ``closed=False`` leaves the angle seam open
    (a sheet, e.g. a mantle that is open at the front)."""
    bm = bmesh.new()
    lu = bm.verts.layers.float.new("af_u")
    la = bm.verts.layers.float.new("af_a")
    rings = []
    for u in us:
        ring = []
        for a in angles:
            v = bm.verts.new(tuple(fn(u, a)))
            v[lu], v[la] = u, a
            ring.append(v)
        rings.append(ring)
    n = len(angles)
    seg = n if closed else n - 1
    for r0, r1 in zip(rings, rings[1:]):
        for i in range(seg):
            j = (i + 1) % n
            bm.faces.new((r0[i], r0[j], r1[j], r1[i]))
    if closed:
        cen = [sum((v.co for v in r), Vector()) / n for r in rings]
        for cap, k, dome, sgn in ((cap0, 0, dome0, -1), (cap1, -1, dome1, 1)):
            if cap is None:
                continue
            ring = rings[k]
            t = (cen[-1] - cen[-2]) if k == -1 else (cen[1] - cen[0])
            t = t.normalized() if t.length > 1e-9 else Vector((0, 0, 1))
            c = bm.verts.new(cen[k] + t * dome * sgn)
            c[lu], c[la] = us[k], 0.0
            for i in range(n):
                j = (i + 1) % n
                bm.faces.new((ring[j], ring[i], c) if sgn < 0 else (ring[i], ring[j], c))
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update()
    return M.Part(bm).smooth(True)


def split_by(part: M.Part, pred: Callable[[float, float, Vector], bool]) -> tuple[M.Part, M.Part]:
    """Split a :func:`grid_loft` part into (faces where ``pred(u, a, centre)`` is False, … True) using the
    face's mean ``af_u`` / ``af_a`` (angles averaged on the circle)."""
    def pr(bm, f):
        lu = bm.verts.layers.float["af_u"]
        la = bm.verts.layers.float["af_a"]
        us = [v[lu] for v in f.verts]
        sx = sum(math.sin(math.radians(v[la])) for v in f.verts)
        cx = sum(math.cos(math.radians(v[la])) for v in f.verts)
        return pred(sum(us) / len(us), math.degrees(math.atan2(sx, cx)), f.calc_center_median())
    return C.split_faces(part, pr)


def angles(n: int, offset: float = 0.0, extra: Sequence[float] = ()) -> list[float]:
    """``n`` evenly spaced angles in (−180, 180] (+ extra ones), sorted ascending."""
    out = {round(((offset + 360.0 * i / n + 180.0) % 360.0) - 180.0, 4) for i in range(n)}
    out |= {round(((a + 180.0) % 360.0) - 180.0, 4) for a in extra}
    return sorted(out)


def _superellipse(rx: float, ry: float, a: float, e: float) -> tuple[float, float]:
    """(lateral, forward) of angle ``a`` (0 front, +90 left) on a superellipse."""
    t = math.radians(a - 90.0)
    c, s = math.cos(t), math.sin(t)
    return rx * math.copysign(abs(c) ** (2.0 / e), c), -ry * math.copysign(abs(s) ** (2.0 / e), s)


class SpineLoft:
    """Surface lofted along a smooth spine curve through ``pts`` (bottom → top, armature space).

    ``rings``: ``(s, lat, fwd[, centre_fwd[, centre_lat]])`` with ``s`` in 0..1 along the curve (may extend past
    the ends: the curve is extrapolated along the end tangents), radii in metres; ``fwd_dir`` = the body's
    forward (−Y). Ring planes are perpendicular to the curve."""

    def __init__(self, pts: Sequence[Vector], rings: Sequence[Sequence[float]], exp: float = 2.2,
                 fwd_dir: Vector = Vector((0, -1, 0))):
        self.pts = [Vector(p) for p in pts]
        d = [0.0]
        for a, b in zip(self.pts, self.pts[1:]):
            d.append(d[-1] + (b - a).length)
        self.L = d[-1]
        self.d = [x / self.L for x in d]
        self.xs = [[p[i] for p in self.pts] for i in range(3)]
        rs = sorted([tuple(r) + (0.0,) * (5 - len(r)) for r in rings], key=lambda r: r[0])
        self.rs = [r[0] for r in rs]
        self.ch = [[r[i] for r in rs] for i in range(1, 5)]
        self.exp = exp
        self.fwd = Vector(fwd_dir)

    def curve(self, s: float) -> Vector:
        if s < 0.0:
            return self.curve(0.0) + self.tangent(0.0) * (s * self.L)
        if s > 1.0:
            return self.curve(1.0) + self.tangent(1.0) * ((s - 1.0) * self.L)
        return Vector([C._hermite(self.d, self.xs[i], s) for i in range(3)])

    def tangent(self, s: float) -> Vector:
        s = max(0.0, min(1.0, s))
        e = 1e-3
        t = Vector([C._hermite(self.d, self.xs[i], min(1.0, s + e)) - C._hermite(self.d, self.xs[i], max(0.0, s - e))
                    for i in range(3)])
        return t.normalized()

    def frame(self, s: float) -> tuple[Vector, Vector, Vector, Vector]:
        """(centre, tangent, forward, lateral(left))."""
        T = self.tangent(s)
        lat = T.cross(self.fwd)                  # up × forward(−Y) = +X (left)
        if lat.length < 1e-6:
            lat = Vector((1, 0, 0))
        lat.normalize()
        fw = lat.cross(T).normalized()
        return self.curve(s), T, fw, lat

    def ring(self, s: float) -> tuple[float, float, float, float]:
        lat, fwd, cf, cl = (C._hermite(self.rs, c, s) for c in self.ch)
        return max(lat, 1e-4), max(fwd, 1e-4), cf, cl

    def point(self, s: float, a: float, out: float = 0.0, exp: float | None = None) -> Vector:
        P, T, F, Lt = self.frame(s)
        rl, rf, cf, cl = self.ring(s)
        x, y = _superellipse(rl, rf, a, exp or self.exp)
        p = P + F * (cf + y) + Lt * (cl + x)
        if out:
            p += self.normal(s, a, exp) * out
        return p

    def normal(self, s: float, a: float, exp: float | None = None) -> Vector:
        ds = 1e-3
        ta = self.point(s, a + 0.5, 0, exp) - self.point(s, a - 0.5, 0, exp)
        ts = self.point(s + ds, a, 0, exp) - self.point(s - ds, a, 0, exp)
        n = ta.cross(ts)
        if n.length < 1e-9:
            return self.frame(s)[2]
        n.normalize()
        P, T, F, Lt = self.frame(s)
        rl, rf, cf, cl = self.ring(s)
        if n.dot(self.point(s, a, 0, exp) - (P + F * cf + Lt * cl)) < 0:
            n = -n
        return n

    def loft(self, s0: float, s1: float, rings: int = 12, sides: int = 24, cap0: str | None = "fan",
             cap1: str | None = "fan", dome0: float = 0.0, dome1: float = 0.0, ss: Sequence[float] | None = None,
             disp: Callable[[float, float], float] | None = None, extra_angles: Sequence[float] = (),
             a_offset: float = 0.0) -> M.Part:
        us = list(ss) if ss is not None else [s0 + (s1 - s0) * i / (rings - 1) for i in range(rings)]
        fn = (lambda s, a: self.point(s, a, disp(s, a))) if disp else (lambda s, a: self.point(s, a))
        return grid_loft(fn, us, angles(sides, a_offset, extra_angles), cap0, cap1, dome0, dome1)


class ProfileLoft:
    """``heroes/common.Profile`` lofted with :func:`grid_loft` (``af_u`` = z) and placed by a frame matrix
    (heads, helms, masks built in a local frame: X left, −Y forward, Z up)."""

    def __init__(self, prof: C.Profile, frame: Matrix | None = None):
        self.p = prof
        self.m = frame if frame is not None else Matrix.Identity(4)
        self.r = self.m.to_3x3()

    def point(self, z: float, a: float, out: float = 0.0) -> Vector:
        return self.m @ self.p.point(z, a, out)

    def normal(self, z: float, a: float) -> Vector:
        return (self.r @ self.p.normal(z, a)).normalized()

    def local(self, v) -> Vector:
        return self.m @ Vector(v)

    def loft(self, zs: Sequence[float], sides: int = 24, cap0: str | None = "fan", cap1: str | None = "fan",
             dome0: float = 0.0, dome1: float = 0.0, disp=None, extra_angles: Sequence[float] = ()) -> M.Part:
        fn = (lambda z, a: self.p.point(z, a, disp(z, a))) if disp else (lambda z, a: self.p.point(z, a))
        part = grid_loft(fn, list(zs), angles(sides, 0.0, extra_angles), cap0, cap1, dome0, dome1)
        return part.transform(self.m)


# ── things that lie on a surface ──────────────────────────────────────────────────────────────────────
def surface_strap(surf, path: Sequence[tuple[float, float]], width: float, thick: float, out0: float = -0.003,
                  steps: int = 24, round_edge: bool = False) -> M.Part:
    """A strap lying on ``surf`` (``point(u, a, out)`` / ``normal``) along a path of (u, a) control points
    (Catmull-Rom in parameter space): closed solid, flat on the surface, ``thick`` proud of it."""
    us = [p[0] for p in path]
    as_ = [p[1] for p in path]
    ts = [i / (len(path) - 1) for i in range(len(path))]
    cen = []
    for k in range(steps + 1):
        t = k / steps
        cen.append((C._hermite(ts, us, t), C._hermite(ts, as_, t)))
    pts = [surf.point(u, a) for u, a in cen]
    outer, inner = [], []
    for k, (u, a) in enumerate(cen):
        tg = (pts[min(k + 1, steps)] - pts[max(k - 1, 0)]).normalized()
        n = surf.normal(u, a)
        side = tg.cross(n).normalized() * (width / 2)
        p = pts[k]
        if round_edge:
            ro = [p - side + n * thick * 0.35, p - side * 0.5 + n * thick * 0.9, p + n * thick,
                  p + side * 0.5 + n * thick * 0.9, p + side + n * thick * 0.35]
            ri = [p - side + n * out0, p - side * 0.5 + n * out0, p + n * out0, p + side * 0.5 + n * out0,
                  p + side + n * out0]
        else:
            ro = [p - side + n * thick, p + side + n * thick]
            ri = [p - side + n * out0, p + side + n * out0]
        outer.append(ro)
        inner.append(ri)
    return C.thick_patch(outer, inner).auto_sharp(55)


def surface_ring(surf, u_of_a: Callable[[float], float], height: float, thick: float, out0: float = -0.003,
                 segs: int = 32, a0: float = -180.0, a1: float = 180.0) -> M.Part:
    """A band around ``surf`` (belts, rims, collars): centre line u = ``u_of_a(a)``; ``height`` measured along
    the surface, ``thick`` proud of it."""
    full = abs((a1 - a0) - 360.0) < 1e-6
    n = segs if full else segs + 1
    angs = [a0 + (a1 - a0) * i / segs for i in range(n)]
    outer, inner = [], []
    for sgn in (-0.5, 0.5):
        ro, ri = [], []
        for a in angs:
            u = u_of_a(a)
            p0 = surf.point(u, a)
            du = 1e-3
            along = (surf.point(u + du, a) - surf.point(u - du, a)).normalized()
            n = surf.normal(u, a)
            p = p0 + along * (sgn * height)
            ro.append(p + n * thick)
            ri.append(p + n * out0)
        outer.append(ro)
        inner.append(ri)
    return C.thick_patch(outer, inner, wrap_u=full).auto_sharp(50)


def surface_dot(surf, u: float, a: float, r: float, h: float, sides: int = 8) -> M.Part:
    """Stud / rivet / bead sitting on ``surf``."""
    p = surf.point(u, a)
    n = surf.normal(u, a)
    d = M.lathe([(r, -0.004), (r, h * 0.35), (r * 0.72, h * 0.85), (r * 0.15, h)], sides=sides, cap1="fan")
    return d.transform(R.frame(p, n, (0, 0, 1)) @ Matrix.Rotation(math.radians(-90), 4, "X"))


def surface_patch(surf, u0: float, u1: float, a0: float, a1: float, lift: float, nu: int = 4, na: int = 8,
                  shape: Callable[[float, float], float] | None = None) -> M.Part:
    """Single-sided painted patch on ``surf`` (belly highlight, war paint, spiral): a grid over u0..u1 × a0..a1,
    ``lift`` metres off the surface; ``shape(tu, ta) → 0..1`` trims the patch into an ellipse-like outline by
    pulling the outer rows to the centre line (keeps quads)."""
    grid = []
    for i in range(nu + 1):
        tu = i / nu
        row = []
        for j in range(na + 1):
            ta = j / na
            k = shape(tu, ta) if shape else 1.0
            a = lerp(a0, a1, 0.5 + (ta - 0.5) * k)
            u = lerp(u0, u1, tu)
            row.append(surf.point(u, a, lift))
        grid.append(row)
    bm = bmesh.new()
    vs = [[bm.verts.new(tuple(p)) for p in row] for row in grid]
    for r in range(nu):
        for c in range(na):
            bm.faces.new((vs[r][c], vs[r][c + 1], vs[r + 1][c + 1], vs[r + 1][c]))
    bm.normal_update()
    f = next(iter(bm.faces))
    cu, ca = (u0 + u1) / 2, (a0 + a1) / 2
    if f.normal.dot(surf.normal(cu, ca)) < 0:
        bmesh.ops.reverse_faces(bm, faces=list(bm.faces))
        bm.normal_update()
    return M.Part(bm).smooth(True)


# ── cloth, leaves, claws ───────────────────────────────────────────────────────────────────────────────
def flap(w_top: float, w_bot: float, length: float, thick: float = 0.008, cols: int = 6, rows: int = 5,
         notch: float = 0.0, tails: int = 2, ragged: float = 0.0, seed: int = 0, bulge: float = 0.0,
         flare: float = 0.0) -> M.Part:
    """Two-sided cloth panel hanging from z = 0 to −length, facing −Y (front face ``af_sub`` 1, back 2):
    ``notch`` cuts a V (``tails`` = 2) or a zig-zag (more tails) into the hem, ``ragged`` jitters the hem
    (torn hide), ``bulge`` bows the middle forward (over a belly), ``flare`` pushes the hem forward."""
    import random
    rnd = random.Random(seed)
    jit = [rnd.uniform(-1, 1) for _ in range(cols + 1)]
    grid = []
    for r in range(rows + 1):
        t = r / rows
        w = lerp(w_top, w_bot, t)
        row = []
        for c in range(cols + 1):
            u = c / cols - 0.5
            z = -length * t
            if r == rows:
                if notch:
                    ph = (u + 0.5) * tails
                    tri = abs((ph % 1.0) - 0.5) * 2          # 1 at the tails' points, 0 in the notches
                    z += notch * (1.0 - tri)
                if ragged:
                    z += ragged * jit[c]
            y = -bulge * math.sin(math.pi * t) * (1 - (2 * u) ** 2) - flare * t * t
            row.append(Vector((u * w, y, z)))
        grid.append(row)
    bm = bmesh.new()
    vs = [[bm.verts.new(tuple(p)) for p in row] for row in grid]
    for r in range(rows):
        for c in range(cols):
            bm.faces.new((vs[r][c], vs[r + 1][c], vs[r + 1][c + 1], vs[r][c + 1]))
    bm.normal_update()
    bmesh.ops.solidify(bm, geom=list(bm.faces), thickness=thick)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update()
    sub = bm.faces.layers.int.new("af_sub")
    for f in bm.faces:
        d = f.normal.y
        f[sub] = 1 if d < -0.35 else (2 if d > 0.35 else 0)
    return M.Part(bm).auto_sharp(70)


def leaf(length: float, width: float, thick: float, sides: int = 8, curl_deg: float = 0.0,
         bend_deg: float = 0.0, taper: float = 0.8, tip: float = 1.0, base_round: float = 0.35,
         center: bool = True) -> M.Part:
    """Leaf / feather / ear / blade plate along +Y (base at the origin), width across X, thickness along Z,
    bevelled; ``curl_deg`` cups it about Y (edges toward +Z), ``bend_deg`` bends the tip toward +Z.
    ``taper`` shapes the outline (sin^taper), ``base_round`` rounds the base end."""
    pts = []
    for i in range(sides + 1):
        t = i / sides
        x = width * 0.5 * (math.sin(math.pi * (base_round + (1 - base_round) * t)) ** taper) * (1.0 - 0.25 * t) ** tip
        pts.append((max(x, width * 0.02 if i < sides else 0.0), length * t))
    outline = [(x, y) for x, y in pts] + [(-x, y) for x, y in reversed(pts[1:-1])]
    outline = [(x, y) for x, y in outline]
    p = M.plate(C.ccw(outline), thick, bevel=thick * 0.35, center=center)
    if curl_deg:
        p.bend(curl_deg, width * 0.5, along="x", toward="z")
    if bend_deg:
        p.bend(bend_deg, length, along="y", toward="z")
    return p.smooth(True)


def cone(base: Vector, tip: Vector, r: float, sides: int = 6, r_tip: float = 0.0) -> M.Part:
    """Small cone (claw, tooth, spike) from ``base`` to ``tip``."""
    d = (tip - base)
    L = d.length
    p = M.loft_z([(0.0, r, r), (L * 0.55, r * 0.6 + r_tip * 0.4, r * 0.6 + r_tip * 0.4), (L, max(r_tip, 1e-4),
                                                                                        max(r_tip, 1e-4))],
                 sides=sides, cap0="fan", cap1="fan")
    m = R.frame(base, d, Vector((0, 0, 1)) if abs(d.normalized().z) < 0.9 else Vector((0, -1, 0)))
    # loft_z grows along +Z: rotate local Z onto the frame's Y
    return p.transform(m @ Matrix.Rotation(math.radians(-90), 4, "X"))


def frame_z(origin: Vector, z_dir: Vector, y_hint: Vector) -> Matrix:
    """4×4 frame with +Z along ``z_dir`` and +Y close to ``y_hint`` (for parts modelled along Z)."""
    z = Vector(z_dir).normalized()
    y = Vector(y_hint) - z * Vector(y_hint).dot(z)
    if y.length < 1e-6:
        y = z.orthogonal()
    y.normalize()
    x = y.cross(z)
    m = Matrix.Identity(4)
    for i in range(3):
        m[i][0], m[i][1], m[i][2], m[i][3] = x[i], y[i], z[i], origin[i]
    return m

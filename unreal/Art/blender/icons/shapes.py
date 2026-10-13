"""Modelling helpers for icon-only models (armour pieces, jewellery, materials, skill motifs, glyphs).

Icon models are built **directly in the studio frame** of ``iconkit``: X right, Z up, the viewer at −Y (front =
−Y). 2-D outlines are (x, z) pairs as seen from the front. Everything returns ``kit.mesh.Part`` (bmesh) so the
kit's ``Builder`` merges parts into one palette-mapped mesh.

* ``pillow``   — a cushioned 2.5-D slab from an outline (rounded front, flatter back, short wall): plates, torsos,
                 hearts, flames, labels, leaves — the web's cel shapes with real normals;
* ``slab``     — a crisp extruded outline with a bevel (``kit.mesh.plate`` turned to face the viewer);
* ``torus`` / ``links`` — rings, chain links along a path; ``brilliant`` — a faceted cut stone (flat shaded);
* ``lump``     — a seeded noisy rock; ``bez`` / ``circle2d`` / ``rrect2d`` / ``star2d`` / ``heart2d`` /
                 ``flame2d`` / ``offset2d`` — outlines.
"""
from __future__ import annotations

import math
import random
from typing import Callable, Sequence

import bmesh
from mathutils import Matrix, Vector

from kit import mesh as M

V2 = tuple[float, float]


# ── outlines ────────────────────────────────────────────────────────────────────────────────────────────
def bez(ctrl: Sequence[V2], n: int = 16, include_start: bool = True) -> list[V2]:
    """Bézier curve of any order through control points (De Casteljau)."""
    out = []
    for i in range(0 if include_start else 1, n + 1):
        t = i / n
        pts = [tuple(p) for p in ctrl]
        while len(pts) > 1:
            pts = [((1 - t) * a[0] + t * b[0], (1 - t) * a[1] + t * b[1]) for a, b in zip(pts, pts[1:])]
        out.append(pts[0])
    return out


def chain2d(*segments: Sequence[V2]) -> list[V2]:
    """Concatenate polylines, dropping a repeated joint point."""
    out: list[V2] = []
    for seg in segments:
        for p in seg:
            if out and abs(out[-1][0] - p[0]) < 1e-9 and abs(out[-1][1] - p[1]) < 1e-9:
                continue
            out.append(tuple(p))
    if len(out) > 2 and abs(out[0][0] - out[-1][0]) < 1e-9 and abs(out[0][1] - out[-1][1]) < 1e-9:
        out.pop()
    return out


def circle2d(r: float, n: int = 24, cx: float = 0.0, cz: float = 0.0, rz: float | None = None,
             phase: float = 90.0) -> list[V2]:
    rz = r if rz is None else rz
    return [(cx + r * math.cos(math.radians(phase) + 2 * math.pi * i / n),
             cz + rz * math.sin(math.radians(phase) + 2 * math.pi * i / n)) for i in range(n)]


def rrect2d(w: float, h: float, r: float, n: int = 4, cx: float = 0.0, cz: float = 0.0) -> list[V2]:
    r = min(r, w / 2, h / 2)
    out = []
    for (sx, sz, a0) in ((1, 1, 0), (-1, 1, 90), (-1, -1, 180), (1, -1, 270)):
        ccx, ccz = cx + sx * (w / 2 - r), cz + sz * (h / 2 - r)
        for i in range(n + 1):
            a = math.radians(a0 + 90 * i / n)
            out.append((ccx + r * math.cos(a), ccz + r * math.sin(a)))
    return out


def star2d(n: int, r1: float, r2: float, rot: float = 90.0, cx: float = 0.0, cz: float = 0.0) -> list[V2]:
    out = []
    for i in range(2 * n):
        r = r1 if i % 2 == 0 else r2
        a = math.radians(rot) + math.pi * i / n
        out.append((cx + r * math.cos(a), cz + r * math.sin(a)))
    return out


def heart2d(r: float, n: int = 40, cx: float = 0.0, cz: float = 0.0) -> list[V2]:
    """Classic heart (lobes up, tip down), about 2r wide."""
    out = []
    for i in range(n):
        t = 2 * math.pi * i / n
        x = 16 * math.sin(t) ** 3
        z = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
        out.append((cx + x * r / 16.0, cz + z * r / 16.0 + r * 0.1))
    return out


def flame2d(w: float, h: float, lean: float = 0.0, n: int = 14, tongues: int = 0, cx: float = 0.0,
            cz: float = 0.0, seed: int = 0) -> list[V2]:
    """A teardrop flame standing on its round base (bottom at cz), tip at cz + h leaning by ``lean`` × w;
    ``tongues`` adds side licks."""
    rnd = random.Random(seed)
    right = bez([(cx, cz), (cx + w * 0.62, cz + 0.02 * h), (cx + w * 0.55, cz + 0.55 * h),
                 (cx + lean * w, cz + h)], n)
    left = bez([(cx + lean * w, cz + h), (cx - w * 0.45, cz + 0.62 * h), (cx - w * 0.62, cz + 0.05 * h),
                (cx, cz)], n, include_start=False)
    pts = right + left
    if tongues:
        out = []
        for i, (x, z) in enumerate(pts):
            t = i / len(pts)
            bump = 0.0
            for k in range(tongues):
                c = (k + 1) / (tongues + 1)
                bump += max(0.0, 1 - abs(t - c) * 18) * w * (0.14 + 0.06 * rnd.random())
            dx = x - cx
            out.append((x + math.copysign(bump, dx), z + bump * 0.6))
        pts = out
    return pts


def offset2d(pts: Sequence[V2], d: float) -> list[V2]:
    """Approximate parallel offset of a closed outline (miter-limited; small ``d`` only)."""
    n = len(pts)
    area = sum(pts[i][0] * pts[(i + 1) % n][1] - pts[(i + 1) % n][0] * pts[i][1] for i in range(n))
    s = 1.0 if area > 0 else -1.0
    out = []
    for i in range(n):
        a, b, c = Vector((*pts[i - 1], 0)), Vector((*pts[i], 0)), Vector((*pts[(i + 1) % n], 0))
        t1 = (b - a).normalized()
        t2 = (c - b).normalized()
        n1 = Vector((t1.y, -t1.x, 0)) * s
        n2 = Vector((t2.y, -t2.x, 0)) * s
        nm = (n1 + n2)
        if nm.length < 1e-6:
            nm = n1
        nm.normalize()
        k = 1.0 / max(0.35, nm.dot(n1))
        p = b + nm * d * k
        out.append((p.x, p.y))
    return out


def transform2d(pts: Sequence[V2], rot: float = 0.0, scale: float | V2 = 1.0, at: V2 = (0.0, 0.0)) -> list[V2]:
    sx, sz = (scale, scale) if isinstance(scale, (int, float)) else scale
    c, s = math.cos(math.radians(rot)), math.sin(math.radians(rot))
    return [(at[0] + (x * sx) * c - (z * sz) * s, at[1] + (x * sx) * s + (z * sz) * c) for x, z in pts]


def _ccw(pts: Sequence[V2]) -> list[V2]:
    n = len(pts)
    area = sum(pts[i][0] * pts[(i + 1) % n][1] - pts[(i + 1) % n][0] * pts[i][1] for i in range(n))
    return list(pts) if area > 0 else list(reversed(pts))


def centroid2d(pts: Sequence[V2]) -> V2:
    n = len(pts)
    a = cx = cz = 0.0
    for i in range(n):
        x0, z0 = pts[i]
        x1, z1 = pts[(i + 1) % n]
        cr = x0 * z1 - x1 * z0
        a += cr
        cx += (x0 + x1) * cr
        cz += (z0 + z1) * cr
    if abs(a) < 1e-12:
        return (sum(p[0] for p in pts) / n, sum(p[1] for p in pts) / n)
    return (cx / (3 * a), cz / (3 * a))


# ── solids ──────────────────────────────────────────────────────────────────────────────────────────────
def pillow(outline: Sequence[V2], depth: float = 0.04, back: float | None = None, wall: float = 0.01,
           edge: float = 0.45, dome: float = 0.25, y0: float = 0.0, center: V2 | None = None,
           rings: Sequence[float] = (0.0, 0.05, 0.13, 0.25, 0.4, 0.58, 0.78), tilt: V2 = (0.0, 0.0)) -> M.Part:
    """Cushioned slab from a 2-D outline (x, z): the front (toward −Y) rises ``depth`` over a rounded edge
    (``edge`` = the inset fraction where the rounding ends; ``dome`` = extra crown toward the centre), the back
    by ``back`` (default 30 % of the front), joined by a ``wall`` of that thickness at ``y0``. Rings are the
    outline scaled toward ``center`` (default its centroid) — fine for convex or mildly concave shapes.
    ``tilt`` = (dx, dz) shift of the crown (a bulge that leans, e.g. toward the light)."""
    pts = _ccw(outline)
    c = centroid2d(pts) if center is None else center
    back = depth * 0.3 if back is None else back

    def h(d, amt):
        k = min(1.0, d / max(edge, 1e-6))
        return amt * (math.sqrt(max(0.0, 1 - (1 - k) ** 2)) * (1 - dome) + dome * d)

    bm = bmesh.new()

    def ring(d, sign, amt):
        s = 1.0 - d
        y = y0 + sign * (wall / 2 + h(d, amt))
        return [bm.verts.new((c[0] + (x - c[0]) * s + tilt[0] * d, y, c[1] + (z - c[1]) * s + tilt[1] * d))
                for x, z in pts]

    back_rings = [ring(d, +1, back) for d in reversed(rings)]          # inner → outer
    front_rings = [ring(d, -1, depth) for d in rings]                  # outer → inner
    allr = back_rings + front_rings
    n = len(pts)
    for r0, r1 in zip(allr, allr[1:]):
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((r0[i], r0[j], r1[j], r1[i]))
    cb = bm.verts.new((c[0] + tilt[0], y0 + wall / 2 + h(1.0, back), c[1] + tilt[1]))
    cf = bm.verts.new((c[0] + tilt[0], y0 - wall / 2 - h(1.0, depth), c[1] + tilt[1]))
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((back_rings[0][j], back_rings[0][i], cb))
        bm.faces.new((front_rings[-1][i], front_rings[-1][j], cf))
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return M.Part(bm).smooth(True)


def slab(outline: Sequence[V2], thick: float, y0: float = 0.0, bevel: float = 0.0, segments: int = 1) -> M.Part:
    """Crisp extruded outline (front face toward −Y, centred on ``y0``), optional bevel."""
    p = M.plate(_ccw(outline), thick, bevel=bevel, segments=segments)
    p.rotate("x", 90.0)
    return p.translate((0, y0, 0))


def torus(R: float, r: float, center=(0.0, 0.0, 0.0), axis=(0.0, -1.0, 0.0), sides: int = 10, segs: int = 32,
          rz: float | None = None, exp: float = 2.0, Rz: float | None = None) -> M.Part:
    """Ring of major radius ``R`` (``Rz`` for an oval) round ``axis`` through ``center``; tube radius ``r``
    (``rz`` along the axis)."""
    ax = Vector(axis).normalized()
    u = ax.orthogonal().normalized()
    v = ax.cross(u).normalized()
    Rz = R if Rz is None else Rz
    path = [Vector(center) + u * (R * math.cos(2 * math.pi * i / segs)) + v * (Rz * math.sin(2 * math.pi * i / segs))
            for i in range(segs)]
    prof = M.superellipse(rz if rz is not None else r, r, sides, exp)
    return M.sweep(path, prof, up=[ax] * segs, closed_path=True).smooth(True)


def links(points: Sequence[Sequence[float]], link_len: float, wire: float, width: float | None = None,
          face: Sequence[float] = (0.0, -1.0, 0.0)) -> M.Part:
    """Chain links along a 3-D polyline: alternating flat / edge-on oval rings (``link_len`` long)."""
    pts = [Vector(p) for p in points]
    seg = []
    total = 0.0
    for a, b in zip(pts, pts[1:]):
        seg.append((a, b, (b - a).length))
        total += (b - a).length
    n = max(2, int(total / (link_len * 0.78)))
    width = link_len * 0.62 if width is None else width
    out = None
    fv = Vector(face).normalized()
    for k in range(n):
        d = (k + 0.5) * total / n
        acc = 0.0
        for a, b, L in seg:
            if acc + L >= d:
                t = (d - acc) / L
                p = a + (b - a) * t
                tang = (b - a).normalized()
                break
            acc += L
        else:
            p, tang = pts[-1], (pts[-1] - pts[-2]).normalized()
        side = tang.cross(fv).normalized()
        axis = fv if k % 2 == 0 else side
        long_dir = tang
        # oval ring in local XY (X = along the chain), round wire of radius ``wire``
        a_, b_ = link_len / 2 - wire, max(width / 2 - wire, wire * 1.2)
        loop = [Vector((a_ * math.cos(2 * math.pi * i / 16), b_ * math.sin(2 * math.pi * i / 16), 0.0)) for i in range(16)]
        ring = M.sweep(loop, M.superellipse(wire, wire, 6), up=[(0, 0, 1)] * 16, closed_path=True).smooth(True)
        # local X → tangent, local Z → axis
        y_dir = axis.cross(long_dir).normalized()
        m = Matrix((long_dir, y_dir, axis)).transposed().to_4x4()
        m.translation = p
        ring.transform(m)
        out = ring if out is None else out.merge(ring)
    return out


def brilliant(r: float, sides: int = 8, crown: float = 0.35, pavilion: float = 0.75, table: float = 0.55,
              axis=(0.0, -1.0, 0.0), center=(0.0, 0.0, 0.0), squash: float = 1.0) -> M.Part:
    """Faceted stone (table toward ``axis``): table ring, crown, girdle, pavilion point; flat shaded."""
    prof = [(table * r, crown * r), (r, 0.0), (r, -0.06 * r)]
    bm = bmesh.new()
    rings = []
    for k, (rr, z) in enumerate(prof):
        ring = []
        for i in range(sides):
            a = 2 * math.pi * (i + (0.5 if k == 0 else 0.0)) / sides
            ring.append(bm.verts.new((rr * math.cos(a), rr * math.sin(a) * squash, z)))
        rings.append(ring)
    top = bm.verts.new((0, 0, crown * r))
    bot = bm.verts.new((0, 0, -pavilion * r))
    for i in range(sides):
        j = (i + 1) % sides
        bm.faces.new((rings[0][i], rings[0][j], top))
    for r0, r1 in zip(rings, rings[1:]):
        for i in range(sides):
            j = (i + 1) % sides
            bm.faces.new((r0[i], r1[i], r1[j], r0[j]) if False else (r1[i], r1[j], r0[j], r0[i]))
    for i in range(sides):
        j = (i + 1) % sides
        bm.faces.new((rings[-1][j], rings[-1][i], bot))
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    p = M.Part(bm).flat()
    ax = Vector(axis).normalized()
    rot = Vector((0, 0, 1)).rotation_difference(ax).to_matrix().to_4x4()
    p.transform(rot)
    return p.translate(center)


def lump(radii: Sequence[float], seed: int, center=(0.0, 0.0, 0.0), noise: float = 0.18, sides: int = 14,
         rings: int = 9, flat_bottom: float | None = None, facets: bool = True) -> M.Part:
    """A seeded irregular rock: an ellipsoid with low-frequency radial noise (faceted)."""
    rnd = random.Random(seed)
    bumps = [(Vector((rnd.uniform(-1, 1), rnd.uniform(-1, 1), rnd.uniform(-1, 1))).normalized(),
              rnd.uniform(-noise, noise)) for _ in range(9)]
    p = M.ellipsoid(tuple(radii), (0, 0, 0), sides=sides, rings=rings)

    def f(co: Vector) -> Vector:
        d = co.normalized() if co.length > 1e-9 else Vector((0, 0, 1))
        k = 1.0
        for b, a in bumps:
            k += a * max(0.0, d.dot(b)) ** 3
        v = co * k
        if flat_bottom is not None:
            v.z = max(v.z, flat_bottom)
        return v
    p.deform(f)
    if facets:
        p.flat()
    return p.translate(center)


def tube2(points: Sequence[Sequence[float]], radii, sides: int = 10, **kw) -> M.Part:
    return M.tube([Vector(p) for p in points], radii, sides=sides, **kw)


def path3(pts2: Sequence[V2], y: float = 0.0) -> list[Vector]:
    return [Vector((x, y, z)) for x, z in pts2]


def ribbon2d(pts2: Sequence[V2], width: Callable[[float], float] | float) -> list[V2]:
    """Outline of a 2-D ribbon (the web ``ribbonPath``) along a polyline in the (x, z) plane, half-width
    ``width(t)`` — feed it to ``pillow`` / ``slab`` (swooshes, horns seen side-on, straps)."""
    n = len(pts2)
    left, right = [], []
    for i in range(n):
        a = pts2[max(0, i - 1)]
        b = pts2[min(n - 1, i + 1)]
        dx, dz = b[0] - a[0], b[1] - a[1]
        L = math.hypot(dx, dz) or 1.0
        nx, nz = -dz / L, dx / L
        w = width(i / (n - 1)) if callable(width) else width
        left.append((pts2[i][0] + nx * w, pts2[i][1] + nz * w))
        right.append((pts2[i][0] - nx * w, pts2[i][1] - nz * w))
    outline = left + list(reversed(right))
    return outline


def spiral2d(r0: float, r1: float, turns: float, n: int = 60, phase: float = 0.0) -> list[V2]:
    out = []
    for i in range(n + 1):
        t = i / n
        a = math.radians(phase) + 2 * math.pi * turns * t
        r = r0 + (r1 - r0) * t
        out.append((r * math.cos(a), r * math.sin(a)))
    return out


class Surface:
    """Ray-cast probe of a part seen from the front (−Y): ``at(x, z)`` → (point, normal) on its nearest front
    surface — lays painted strips, rivets and studs onto cushioned plates."""

    def __init__(self, *parts: M.Part):
        from mathutils.bvhtree import BVHTree
        bm = bmesh.new()
        for p in parts:
            tmp = p.copy()
            me_bm = tmp.bm
            # merge geometry
            vmap = {}
            for v in me_bm.verts:
                vmap[v] = bm.verts.new(v.co)
            for f in me_bm.faces:
                try:
                    bm.faces.new([vmap[v] for v in f.verts])
                except ValueError:
                    pass
            me_bm.free()
        bm.normal_update()
        self.bm = bm
        self.bvh = BVHTree.FromBMesh(bm)

    def at(self, x: float, z: float, lift: float = 0.0) -> tuple[Vector, Vector] | tuple[None, None]:
        loc, nrm, _i, _d = self.bvh.ray_cast(Vector((x, -50.0, z)), Vector((0, 1, 0)))
        if loc is None:
            return None, None
        if nrm.y > 0:
            nrm = -nrm
        return loc + nrm * lift, nrm

    def path(self, pts2: Sequence[V2], lift: float = 0.0) -> list[Vector]:
        out = []
        for x, z in pts2:
            p, _ = self.at(x, z, lift)
            if p is not None:
                out.append(p)
        return out

    def free(self) -> None:
        self.bm.free()


def resample2d(pts: Sequence[V2], step: float, closed: bool = False) -> list[V2]:
    """Evenly spaced points along a polyline."""
    P = list(pts) + ([pts[0]] if closed else [])
    out = [P[0]]
    acc = 0.0
    for a, b in zip(P, P[1:]):
        L = math.hypot(b[0] - a[0], b[1] - a[1])
        t = step - acc
        while t <= L:
            out.append((a[0] + (b[0] - a[0]) * t / L, a[1] + (b[1] - a[1]) * t / L))
            t += step
        acc = (acc + L) % step
    return out


def dome_on(surface: Surface, x: float, z: float, r: float, h: float | None = None, lift: float = 0.0,
            sides: int = 10) -> M.Part | None:
    """A small dome (rivet, stud, gem seat) seated on a surface at (x, z), its axis along the surface normal."""
    p, n = surface.at(x, z, lift)
    if p is None:
        return None
    h = r * 0.6 if h is None else h
    d = M.ellipsoid((r, r, h), (0, 0, 0), sides=sides, rings=6)
    rot = Vector((0, 0, 1)).rotation_difference(-n if n.y > 0 else n).to_matrix().to_4x4()
    # normal points toward −Y (outward of the front face); align +Z of the dome to the normal
    rot = Vector((0, 0, 1)).rotation_difference(n).to_matrix().to_4x4()
    d.transform(rot)
    return d.translate(p)


def align(part: M.Part, y_dir, z_hint=(0.0, 0.0, 1.0), at=(0.0, 0.0, 0.0)) -> M.Part:
    """Turn a part modelled along +Y (bending toward +Z: horns, spikes, feathers) so +Y → ``y_dir`` and +Z leans to
    ``z_hint``; then move its origin to ``at``."""
    y = Vector(y_dir).normalized()
    z = Vector(z_hint) - y * Vector(z_hint).dot(y)
    if z.length < 1e-6:
        z = y.orthogonal()
    z.normalize()
    x = y.cross(z)
    m = Matrix((x, y, z)).transposed().to_4x4()
    m.translation = Vector(at)
    return part.transform(m)


def blob2d(pts: Sequence[V2], n: int = 8, tension: float = 0.5) -> list[V2]:
    """Smooth closed curve through points (Catmull-Rom), like the web ``blobPath``."""
    out = []
    m = len(pts)
    for i in range(m):
        p0, p1, p2, p3 = pts[(i - 1) % m], pts[i], pts[(i + 1) % m], pts[(i + 2) % m]
        for k in range(n):
            t = k / n
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * ((2 * p1[j]) + (-p0[j] + p2[j]) * t + (2 * p0[j] - 5 * p1[j] + 4 * p2[j] - p3[j]) * t2
                                    + (-p0[j] + 3 * p1[j] - 3 * p2[j] + p3[j]) * t3) for j in range(2)))
    return out


def capsule2d(a: V2, b: V2, ra: float, rb: float | None = None, n: int = 10) -> list[V2]:
    """Stadium outline from a to b (radii ra → rb)."""
    rb = ra if rb is None else rb
    ang = math.atan2(b[1] - a[1], b[0] - a[0])
    out = []
    for i in range(n + 1):
        t = ang + math.pi / 2 + math.pi * i / n
        out.append((b[0] + rb * math.cos(t - math.pi), b[1] + rb * math.sin(t - math.pi)))
    for i in range(n + 1):
        t = ang - math.pi / 2 + math.pi * i / n
        out.append((a[0] + ra * math.cos(t + math.pi), a[1] + ra * math.sin(t + math.pi)))
    return _ccw(out)

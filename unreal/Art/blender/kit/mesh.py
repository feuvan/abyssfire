"""Low-poly stylised modelling helpers (bmesh), and the part ``Builder``.

Every helper returns a :class:`Part` (a bmesh with outward-facing, closed geometry unless stated) in a local
frame; place it with ``.transform(frame)`` (see ``kit.rig.frame``) or ``.translate/.rotate/.scale``.
``Builder.add(part, region, bind)`` merges it into the asset mesh and tags every face with

* ``af_swatch`` — palette swatch (material region) → collapsed UVs at build time,
* ``af_part``   — part id → skin binding (``kit.rig.Bind``),
* ``af_nohull`` — 1 = no outline hull for this part (tiny emissive bits, eyes),
* ``af_hullonly`` — 1 = a low-poly hull proxy: only its baked hull copy is kept (``Builder.add(hull=…)``).

Shapes: :func:`sweep` (generic profile along a path) → :func:`tube`, :func:`capsule`, :func:`belt`,
:func:`strap`; :func:`loft_z` / :func:`lathe` / :func:`ellipsoid` (stacked superellipse rings); :func:`box`
(bevelled), :func:`plate` (bevelled extruded outline, optional bend), :func:`sheet` (cloth panel with thickness,
wrap, flare and hem wave), :func:`mitten`, :func:`boot`, :func:`ear`, :func:`horn`, :func:`gem`,
:func:`skin_tube` (Skin modifier), :func:`cylinder`.
Budgets (spec §1.9): heroes 14–18 k tris LOD0, NPC 8–12 k, goblins 6–9 k, slime 2–3 k — ``tri_count``.
"""
from __future__ import annotations

import math
from typing import Callable, Iterable, Sequence

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

from . import palette as _pal
from . import scene

V3 = Sequence[float]


# ── Part ────────────────────────────────────────────────────────────────────────────────────────────────
class Part:
    def __init__(self, bm: bmesh.types.BMesh | None = None):
        self.bm = bm if bm is not None else bmesh.new()

    def copy(self) -> "Part":
        me = bpy.data.meshes.new("_tmp_copy")
        self.bm.to_mesh(me)
        bm = bmesh.new()
        bm.from_mesh(me)
        bpy.data.meshes.remove(me)
        return Part(bm)

    def transform(self, m: Matrix) -> "Part":
        m4 = m.to_4x4() if len(m) == 3 else m
        bmesh.ops.transform(self.bm, matrix=m4, verts=self.bm.verts)
        if m4.to_3x3().determinant() < 0:
            bmesh.ops.reverse_faces(self.bm, faces=self.bm.faces)
        return self

    def translate(self, v: V3) -> "Part":
        bmesh.ops.translate(self.bm, vec=Vector(v), verts=self.bm.verts)
        return self

    def rotate(self, axis: V3 | str, deg: float, pivot: V3 = (0, 0, 0)) -> "Part":
        ax = {"x": (1, 0, 0), "y": (0, 1, 0), "z": (0, 0, 1)}[axis] if isinstance(axis, str) else axis
        r = Matrix.Rotation(math.radians(deg), 4, Vector(ax))
        p = Matrix.Translation(Vector(pivot))
        return self.transform(p @ r @ p.inverted())

    def scale(self, s: float | V3, pivot: V3 = (0, 0, 0)) -> "Part":
        sv = (s, s, s) if isinstance(s, (int, float)) else s
        m = Matrix.Diagonal((sv[0], sv[1], sv[2], 1.0))
        p = Matrix.Translation(Vector(pivot))
        return self.transform(p @ m @ p.inverted())

    def mirrored_x(self) -> "Part":
        return self.copy().scale((-1, 1, 1))

    def deform(self, fn: Callable[[Vector], Vector]) -> "Part":
        for v in self.bm.verts:
            v.co = Vector(fn(v.co.copy()))
        return self

    def bend(self, angle_deg: float, length: float, along: str = "y", toward: str = "z") -> "Part":
        """Bend the part along ``along`` (0..length) so its far end turns ``angle_deg`` toward ``toward``."""
        th = math.radians(angle_deg)
        if abs(th) < 1e-6:
            return self
        R = length / th
        ia = "xyz".index(along)
        it = "xyz".index(toward)

        def f(co: Vector) -> Vector:
            a, t = co[ia], co[it]
            phi = a / R
            out = co.copy()
            out[ia] = (R - t) * math.sin(phi)
            out[it] = R - (R - t) * math.cos(phi)
            return out
        return self.deform(f)

    def taper(self, along: str, k0: float, k1: float, lo: float, hi: float, axes: str = "xz") -> "Part":
        ia = "xyz".index(along)

        def f(co: Vector) -> Vector:
            t = min(1.0, max(0.0, (co[ia] - lo) / max(hi - lo, 1e-9)))
            k = k0 + (k1 - k0) * t
            out = co.copy()
            for ax in axes:
                out["xyz".index(ax)] *= k
            return out
        return self.deform(f)

    def smooth(self, on: bool = True) -> "Part":
        for f in self.bm.faces:
            f.smooth = on
        for e in self.bm.edges:
            e.smooth = True
        return self

    def flat(self) -> "Part":
        return self.smooth(False)

    def auto_sharp(self, deg: float = 40.0) -> "Part":
        """Smooth shading with sharp edges above ``deg`` (hard-surface plates, blades, boots)."""
        self.smooth(True)
        self.bm.normal_update()
        thr = math.radians(deg)
        for e in self.bm.edges:
            if len(e.link_faces) == 2 and e.calc_face_angle(0.0) > thr:
                e.smooth = False
        return self

    def recalc_normals(self) -> "Part":
        self.bm.normal_update()
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        return self

    def merge(self, other: "Part") -> "Part":
        _append(self.bm, other.bm, {})
        return self

    def tris(self) -> int:
        return sum(len(f.verts) - 2 for f in self.bm.faces)

    def bounds(self) -> tuple[Vector, Vector]:
        cs = np.array([tuple(v.co) for v in self.bm.verts])
        return Vector(cs.min(0)), Vector(cs.max(0))


def _append(dst: bmesh.types.BMesh, src: bmesh.types.BMesh, int_layers: dict[str, int],
            sub_swatch: dict[int, int] | None = None) -> list:
    """Copy src geometry into dst; set face int layers; keep smooth flags / sharp edges.

    ``sub_swatch`` maps the source face layer ``af_sub`` values to swatch indices (two-sided sheets)."""
    vmap = {}
    src_sub = src.faces.layers.int.get("af_sub") if sub_swatch else None
    for v in src.verts:
        vmap[v] = dst.verts.new(v.co)
    out = []
    lays = {k: dst.faces.layers.int.get(k) or dst.faces.layers.int.new(k) for k in int_layers}
    for f in src.faces:
        try:
            nf = dst.faces.new([vmap[v] for v in f.verts])
        except ValueError:   # duplicate face — skip
            continue
        nf.smooth = f.smooth
        for k, val in int_layers.items():
            nf[lays[k]] = val
        if src_sub is not None and f[src_sub] in sub_swatch:
            nf[lays["af_swatch"]] = sub_swatch[f[src_sub]]
        out.append(nf)
    dst.edges.ensure_lookup_table()
    for e in src.edges:
        if not e.smooth:
            a, b = vmap[e.verts[0]], vmap[e.verts[1]]
            ne = dst.edges.get((a, b))
            if ne is not None:
                ne.smooth = False
    return out


# ── profiles ────────────────────────────────────────────────────────────────────────────────────────────
def superellipse(rx: float, ry: float, n: int, exp: float = 2.0, phase: float = 0.0) -> list[tuple[float, float]]:
    """CCW closed profile; ``exp`` 2 = ellipse, 3–4 = rounded box, 1 = diamond."""
    pts = []
    for i in range(n):
        a = phase + 2 * math.pi * i / n
        c, s = math.cos(a), math.sin(a)
        pts.append((rx * math.copysign(abs(c) ** (2.0 / exp), c), ry * math.copysign(abs(s) ** (2.0 / exp), s)))
    return pts


def rect_profile(w: float, h: float, cx: float = 0.0, cy: float = 0.0) -> list[tuple[float, float]]:
    return [(cx - w / 2, cy - h / 2), (cx + w / 2, cy - h / 2), (cx + w / 2, cy + h / 2), (cx - w / 2, cy + h / 2)]


# ── sweep (the workhorse) ───────────────────────────────────────────────────────────────────────────────
def _frames(path: list[Vector], up: Vector | Sequence[Vector] | None, closed: bool):
    n = len(path)
    T = []
    for i in range(n):
        if closed:
            d = path[(i + 1) % n] - path[(i - 1) % n]
        elif i == 0:
            d = path[1] - path[0]
        elif i == n - 1:
            d = path[-1] - path[-2]
        else:
            d = (path[i + 1] - path[i]).normalized() + (path[i] - path[i - 1]).normalized()
        T.append(d.normalized())
    N = []
    if up is not None and not isinstance(up, Vector) and len(up) == n and not isinstance(up[0], (int, float)):
        for i in range(n):
            u = Vector(up[i])
            u = u - T[i] * u.dot(T[i])
            N.append(u.normalized())
        return T, N
    upv = Vector(up) if up is not None else None
    if upv is not None and closed:
        for i in range(n):
            u = upv - T[i] * upv.dot(T[i])
            N.append(u.normalized())
        return T, N
    # parallel transport from an initial normal
    if upv is None:
        upv = Vector((0, 0, 1)) if abs(T[0].z) < 0.9 else Vector((0, -1, 0))
    u = upv - T[0] * upv.dot(T[0])
    if u.length < 1e-6:
        u = T[0].orthogonal()
    N.append(u.normalized())
    for i in range(1, n):
        q = T[i - 1].rotation_difference(T[i])
        nn = q @ N[-1]
        nn = nn - T[i] * nn.dot(T[i])
        N.append(nn.normalized())
    return T, N


def sweep(path: Sequence[V3], profile: Sequence[tuple[float, float]] | Callable[[int], Sequence[tuple[float, float]]],
          scales: Sequence[float | tuple[float, float]] | None = None, up: V3 | Sequence[V3] | None = None,
          closed_path: bool = False, cap0: str | None = "fan", cap1: str | None = "fan",
          dome0: float = 0.0, dome1: float = 0.0, twist_deg: float = 0.0) -> Part:
    """Sweep a CCW 2-D ``profile`` (x along the frame normal N, y along B = T×N) along ``path``.

    ``scales`` per path point (scalar or (sx, sy)); ``profile`` may be a function of the point index (for
    lofting different shapes). Caps: 'fan' (centre vertex pushed out by ``dome*``), 'ngon', or None (open).
    """
    P = [Vector(p) for p in path]
    T, N = _frames(P, up, closed_path)
    bm = bmesh.new()
    rings = []
    n = len(P)
    for i in range(n):
        prof = profile(i) if callable(profile) else profile
        s = scales[i] if scales is not None else 1.0
        sx, sy = (s, s) if isinstance(s, (int, float)) else s
        tw = math.radians(twist_deg) * (i / max(1, n - 1))
        Ni = Matrix.Rotation(tw, 3, T[i]) @ N[i]
        Bi = T[i].cross(Ni)
        rings.append([bm.verts.new(P[i] + Ni * (a * sx) + Bi * (b * sy)) for a, b in prof])
    m = len(rings[0])
    seg = n if closed_path else n - 1
    for k in range(seg):
        r0, r1 = rings[k], rings[(k + 1) % n]
        for i in range(m):
            j = (i + 1) % m
            bm.faces.new((r0[i], r0[j], r1[j], r1[i]))
    if not closed_path:
        for cap, ring, dome, sign, c in ((cap0, rings[0], dome0, -1, 0), (cap1, rings[-1], dome1, 1, -1)):
            if cap is None:
                continue
            ring_o = list(reversed(ring)) if sign < 0 else list(ring)
            if cap == "ngon":
                bm.faces.new(ring_o)
            else:
                ctr = sum((v.co for v in ring), Vector()) / len(ring) + T[c] * (dome * sign)
                cv = bm.verts.new(ctr)
                for i in range(len(ring_o)):
                    bm.faces.new((ring_o[i], ring_o[(i + 1) % len(ring_o)], cv))
    part = Part(bm).smooth(True)
    return part


def tube(points: Sequence[V3], radii: Sequence[float | tuple[float, float]], sides: int = 10, exp: float = 2.0,
         up: V3 | None = None, cap0: str | None = "fan", cap1: str | None = "fan", dome0: float | None = None,
         dome1: float | None = None, phase: float = 0.0) -> Part:
    """Round (or superellipse) tube through ``points`` with per-point radii — limbs, horns, tails, shafts."""
    prof = superellipse(1.0, 1.0, sides, exp, phase)
    r0 = radii[0] if isinstance(radii[0], (int, float)) else max(radii[0])
    r1 = radii[-1] if isinstance(radii[-1], (int, float)) else max(radii[-1])
    return sweep(points, prof, scales=radii, up=up, cap0=cap0, cap1=cap1,
                 dome0=(r0 * 0.6 if dome0 is None else dome0), dome1=(r1 * 0.6 if dome1 is None else dome1))


def capsule(a: V3, b: V3, ra: float, rb: float | None = None, sides: int = 10, cap_rings: int = 2,
            mid: int = 1, up: V3 | None = None, exp: float = 2.0) -> Part:
    """Tapered capsule from ``a`` to ``b`` with rounded ends (limb segments, fingers, thumbs)."""
    rb = ra if rb is None else rb
    A, B = Vector(a), Vector(b)
    d = (B - A)
    dn = d.normalized()
    pts, rads = [], []
    for j in range(1, cap_rings + 1):           # start dome: tip → A
        ang = (math.pi / 2) * j / (cap_rings + 1)
        pts.append(A - dn * ra * math.cos(ang) * 0.9)
        rads.append(ra * math.sin(ang))
    for i in range(mid + 2):
        t = i / (mid + 1)
        pts.append(A + d * t)
        rads.append(ra + (rb - ra) * t)
    for j in range(cap_rings, 0, -1):           # end dome: B → tip
        ang = (math.pi / 2) * j / (cap_rings + 1)
        pts.append(B + dn * rb * math.cos(ang) * 0.9)
        rads.append(rb * math.sin(ang))
    return sweep(pts, superellipse(1, 1, sides, exp), scales=rads, up=up, dome0=ra * 0.1, dome1=rb * 0.1)


def loft_z(rings: Sequence[tuple], sides: int = 16, exp: float = 2.0, cap0: str | None = "fan",
           cap1: str | None = "fan", dome0: float = 0.0, dome1: float = 0.0, phase: float = 0.0) -> Part:
    """Vertical loft: rings ``(z, rx, ry[, cx, cy[, exp]])`` (bottom → top) — torsos, helms, skirts, heads.

    ``rx`` is the X (left-right) radius, ``ry`` the Y (front-back) radius; ``cx, cy`` shift the ring centre
    (e.g. a forward-leaning helm visor), a 6th value overrides the superellipse exponent per ring.
    """
    path, scales, profs = [], [], []
    for r in rings:
        z, rx, ry = r[0], r[1], r[2]
        cx, cy = (r[3], r[4]) if len(r) >= 5 else (0.0, 0.0)
        e = r[5] if len(r) >= 6 else exp
        path.append(Vector((cx, cy, z)))
        scales.append((1.0, 1.0))
        profs.append(superellipse(rx, ry, sides, e, phase))
    # profile x → N, y → B; with up = +Y for a +Z path: N = +Y, B = Z × Y = −X. Map (rx along X, ry along Y):
    prof_fn = lambda i: [(y, -x) for (x, y) in profs[i]]  # noqa: E731
    return sweep(path, prof_fn, up=[(0, 1, 0)] * len(path), cap0=cap0, cap1=cap1, dome0=dome0, dome1=dome1)


def lathe(profile_rz: Sequence[tuple[float, float]], sides: int = 16, exp: float = 2.0, squash_y: float = 1.0,
          cap0: str | None = "fan", cap1: str | None = "fan") -> Part:
    """Revolve ``(radius, z)`` pairs (bottom → top) around Z — pommels, pots, helms, mushroom caps."""
    return loft_z([(z, r, r * squash_y) for r, z in profile_rz], sides, exp, cap0, cap1)


def ellipsoid(radii: V3, center: V3 = (0, 0, 0), sides: int = 16, rings: int = 9, exp: float = 2.0,
              exp_v: float = 2.0) -> Part:
    """Low-poly ellipsoid (poles as fans). ``exp`` boxes the horizontal section, ``exp_v`` the vertical."""
    rx, ry, rz = radii
    rs = []
    for i in range(1, rings):
        a = math.pi * i / rings
        c, s = math.cos(a), math.sin(a)
        z = -rz * math.copysign(abs(c) ** (2.0 / exp_v), c)
        k = abs(s) ** (2.0 / exp_v)
        rs.append((z, rx * k, ry * k))
    p = loft_z(rs, sides, exp, dome0=rz - abs(rs[0][0]), dome1=rz - abs(rs[-1][0]))
    return p.translate(center)


def cylinder(r0: float, r1: float, z0: float, z1: float, sides: int = 12, exp: float = 2.0) -> Part:
    return loft_z([(z0, r0, r0), (z1, r1, r1)], sides, exp, cap0="ngon", cap1="ngon").auto_sharp(50)


def box(size: V3, center: V3 = (0, 0, 0), bevel: float = 0.0, segments: int = 1) -> Part:
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector(size), verts=bm.verts)
    if bevel > 0:
        bmesh.ops.bevel(bm, geom=list(bm.edges) + list(bm.verts), offset=bevel, segments=segments,
                        profile=0.5, affect="EDGES", clamp_overlap=True)
    bmesh.ops.translate(bm, vec=Vector(center), verts=bm.verts)
    return Part(bm).auto_sharp(50)


def plate(outline: Sequence[tuple[float, float]], thickness: float, bevel: float = 0.0, segments: int = 1,
          center: bool = True) -> Part:
    """Extrude a CCW 2-D outline (XY) by ``thickness`` along Z, optional bevel — blades, plates, shields."""
    bm = bmesh.new()
    z0 = -thickness / 2 if center else 0.0
    vs = [bm.verts.new((x, y, z0)) for x, y in outline]
    f = bm.faces.new(list(reversed(vs)))   # bottom faces −Z
    ext = bmesh.ops.extrude_face_region(bm, geom=[f])
    top = [g for g in ext["geom"] if isinstance(g, bmesh.types.BMVert)]
    bmesh.ops.translate(bm, vec=Vector((0, 0, thickness)), verts=top)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    if bevel > 0:
        bmesh.ops.bevel(bm, geom=list(bm.edges), offset=bevel, segments=segments, profile=0.5,
                        affect="EDGES", clamp_overlap=True)
    ngons = [f for f in bm.faces if len(f.verts) > 4]
    if ngons:
        bmesh.ops.triangulate(bm, faces=ngons, quad_method="BEAUTY", ngon_method="EAR_CLIP")
    return Part(bm).auto_sharp(35)


def sheet(width_top: float, width_bottom: float, length: float, thickness: float = 0.012, cols: int = 6,
          rows: int = 5, wrap_radius: float | None = None, flare: float = 0.0, hem_wave: float = 0.0,
          hem_waves: float = 2.0, sag: float = 0.0) -> Part:
    """Cloth panel hanging from z = 0 down to −length, facing −Y — tabards, capes, aprons, loincloths, skirts.

    ``wrap_radius`` curls the panel around a vertical axis (> 0 edges curve back to +Y — a front tabard;
    < 0 edges curve forward — a back cape); ``flare`` pushes the lower rows out along −Y (+ = forward);
    ``hem_wave`` adds a sine wave to the hem (amplitude in m); ``sag`` bows the panel's middle forward.
    """
    bm = bmesh.new()
    grid = []
    for r in range(rows + 1):
        t = r / rows
        w = width_top + (width_bottom - width_top) * t
        row = []
        for c in range(cols + 1):
            u = c / cols - 0.5
            x = u * w
            z = -length * t
            if r == rows and hem_wave:
                z += hem_wave * math.sin(u * math.pi * 2 * hem_waves)
            y = -flare * t * t - sag * math.sin(math.pi * t) * (1 - (2 * u) ** 2)
            if wrap_radius:
                R = wrap_radius
                ang = x / R
                x, y = R * math.sin(ang), y + R * (1 - math.cos(ang))
            row.append(bm.verts.new((x, y, z)))
        grid.append(row)
    for r in range(rows):
        for c in range(cols):
            bm.faces.new((grid[r][c], grid[r + 1][c], grid[r + 1][c + 1], grid[r][c + 1]))
    # faces now point −Y (front); thicken, then tag sides in face layer ``af_sub``:
    # 1 = front face (−Y side), 2 = back face (+Y side), 0 = rim — map them with Builder.add(sub_regions=…)
    bm.normal_update()                       # new bmesh faces have no normals until updated
    bmesh.ops.solidify(bm, geom=list(bm.faces), thickness=thickness)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update()
    sub = bm.faces.layers.int.new("af_sub")
    for f in bm.faces:
        d = f.normal.y
        f[sub] = 1 if d < -0.35 else (2 if d > 0.35 else 0)
    return Part(bm).auto_sharp(70)


def belt(rx: float, ry: float, z: float, height: float, thickness: float, sides: int = 20, exp: float = 2.4,
         cy: float = 0.0, tilt_deg: float = 0.0) -> Part:
    """Closed band around the body: inner superellipse (rx, ry) at height ``z`` — belts, collars, rims."""
    loop = []
    for x, y in superellipse(rx, ry, sides, exp):
        loop.append(Vector((x, y + cy, z)))
    if tilt_deg:
        rot = Matrix.Rotation(math.radians(tilt_deg), 3, "X")
        c = Vector((0, cy, z))
        loop = [c + rot @ (p - c) for p in loop]
    # profile: x along N (= up), y along B (= T × N, outward for a CCW loop)
    prof = rect_profile(height, thickness, 0.0, thickness / 2)
    up = Matrix.Rotation(math.radians(tilt_deg), 3, "X") @ Vector((0, 0, 1))
    return sweep(loop, prof, up=up, closed_path=True).auto_sharp(60)


def strap(points: Sequence[V3], width: float, thickness: float, up: V3 | Sequence[V3] | None = None,
          closed: bool = False) -> Part:
    """Flat strap/ribbon along ``points`` (width across, thickness along ``up``) — bandoliers, cords, bindings."""
    prof = rect_profile(thickness, width)
    return sweep(points, prof, up=up, closed_path=closed, cap0="ngon", cap1="ngon").auto_sharp(60)


def mitten(length: float, width: float, thickness: float, side: str = "l", curl_deg: float = 22.0,
           thumb: bool = True) -> Part:
    """Mitten hand along +Y (wrist at the origin), thumb toward +Z, palm toward +X ('l') / −X ('r').

    Place with ``kit.rig.frame(wrist, hand_dir, z_hint=forward)``.
    """
    w, t, L = width, thickness, length
    prof = superellipse(1, 1, 10, 3.0)
    path = [(0, 0.0, 0), (0, 0.30 * L, 0), (0, 0.62 * L, 0), (0, 0.86 * L, 0)]
    scales = [(0.38 * t, 0.36 * w), (0.5 * t, 0.5 * w), (0.48 * t, 0.49 * w), (0.36 * t, 0.42 * w)]
    hand = sweep(path, prof, scales=scales, up=(1, 0, 0), dome0=0.0, dome1=0.12 * L)
    # curl fused fingers toward the palm (+X), from the knuckles on
    k0 = 0.45 * L

    def curl(co: Vector) -> Vector:
        if co.y <= k0:
            return co
        a = math.radians(curl_deg) * min(1.0, (co.y - k0) / (L - k0))
        dy, dx = co.y - k0, co.x
        return Vector((dx * math.cos(a) + dy * math.sin(a), k0 + dy * math.cos(a) - dx * math.sin(a), co.z))
    hand.deform(curl)
    if thumb:
        th = capsule((0.18 * t, 0.12 * L, 0.30 * w), (0.38 * t, 0.50 * L, 0.48 * w), 0.17 * w, 0.14 * w, sides=8,
                     cap_rings=1, mid=0, up=(1, 0, 0))
        hand.merge(th)
    if side == "r":
        hand.scale((-1, 1, 1))
    return hand.smooth(True)


def boot(length: float, width: float, height: float, shaft: float = 0.0, toe_up: float = 0.015,
         exp: float = 3.0, sides: int = 12) -> Part:
    """Oversized cartoon boot/foot; origin at the ankle on the ground, toe toward −Y, sole flat at z = 0.

    ``length`` heel→toe (heel sits 0.22·L behind the ankle), ``height`` instep height, ``shaft`` extra boot
    shaft height above the ankle (0 = shoe).
    """
    L, W, H = length, width, height
    # (y, half-width, half-height, centre z)
    st = [
        (0.22 * L, 0.36 * W, 0.40 * H, 0.42 * H),
        (0.10 * L, 0.46 * W, 0.52 * H, 0.52 * H),
        (-0.15 * L, 0.50 * W, 0.46 * H, 0.46 * H),
        (-0.45 * L, 0.52 * W, 0.36 * H, 0.36 * H + toe_up * 0.3),
        (-0.68 * L, 0.46 * W, 0.28 * H, 0.28 * H + toe_up),
    ]
    path = [Vector((0, y, cz)) for y, _, _, cz in st]
    scales = [(hh, hw) for _, hw, hh, _ in st]
    prof = superellipse(1, 1, sides, exp)
    # x of profile → N (= up), y → B (= T × N = (−Y) × Z = −X): fine, symmetric
    p = sweep(path, prof, scales=scales, up=(0, 0, 1), dome0=0.05 * L, dome1=0.10 * L)
    p.deform(lambda co: Vector((co.x, co.y, max(co.z, 0.0))))
    if shaft > 0:
        sh = loft_z([(0.25 * H, 0.42 * W, 0.40 * W, 0, 0.04 * L), (0.55 * H + shaft, 0.44 * W, 0.42 * W, 0, 0.02 * L),
                     (0.62 * H + shaft, 0.50 * W, 0.48 * W, 0, 0.02 * L)], sides=sides, exp=2.4)
        p.merge(sh)
    return p.auto_sharp(55)


def ear(length: float, width: float, thickness: float = 0.012, curl_deg: float = 25.0, tip_bend_deg: float = 0.0,
        sides: int = 7) -> Part:
    """Leaf-shaped ear along +Y (base at the origin), cupped about Y, front face toward −Z."""
    pts = []
    for i in range(sides + 1):
        t = i / sides
        x = width * 0.5 * math.sin(math.pi * t) ** 0.8 * (1.0 - 0.35 * t)
        pts.append((x, length * t))
    outline = [(x, y) for x, y in pts] + [(-x, y) for x, y in reversed(pts[1:-1])]
    # CCW check: right side going up then left side coming down → CCW
    p = plate(outline, thickness, bevel=thickness * 0.35)
    if curl_deg:
        p.bend(curl_deg, width * 0.5, along="x", toward="z")
    if tip_bend_deg:
        p.bend(tip_bend_deg, length, along="y", toward="z")
    return p.auto_sharp(50)


def horn(base_r: float, length: float, bend_deg: float = 60.0, sides: int = 8, segments: int = 6,
         tip_r: float | None = None) -> Part:
    """Tapered horn from the origin along +Y, curving toward +Z by ``bend_deg``."""
    tip_r = base_r * 0.12 if tip_r is None else tip_r
    pts, rads = [], []
    th = math.radians(bend_deg)
    for i in range(segments + 1):
        t = i / segments
        if abs(th) < 1e-4:
            pts.append(Vector((0, length * t, 0)))
        else:
            R = length / th
            phi = th * t
            pts.append(Vector((0, R * math.sin(phi), R * (1 - math.cos(phi)))))
        rads.append(base_r + (tip_r - base_r) * t ** 0.9)
    return tube(pts, rads, sides=sides, dome1=tip_r * 0.5, dome0=0.0)


def gem(radius: float, top: float, bottom: float, sides: int = 6) -> Part:
    """Faceted crystal (bipyramid with a girdle band) along +Z, flat shaded."""
    p = loft_z([(-bottom * 0.0, radius, radius), (top * 0.25, radius * 0.92, radius * 0.92)], sides,
               dome0=bottom, dome1=top * 0.75)
    return p.flat()


def skin_tube(points: Sequence[V3], edges: Sequence[tuple[int, int]], radii: Sequence[float | tuple[float, float]],
              subdiv: int = 1, root: int = 0) -> Part:
    """Organic branching shape via the Skin modifier (+ optional subdivision) — torsos, tails, roots, branches."""
    me = bpy.data.meshes.new("_skin_src")
    me.from_pydata([tuple(p) for p in points], list(edges), [])
    ob = bpy.data.objects.new("_skin_src", me)
    scene.link(ob)
    sk = ob.modifiers.new("Skin", "SKIN")
    sk.use_smooth_shade = True
    for i, sv in enumerate(me.skin_vertices[0].data):
        r = radii[i]
        sv.radius = (r, r) if isinstance(r, (int, float)) else tuple(r)
        sv.use_root = i == root
    if subdiv > 0:
        ss = ob.modifiers.new("Sub", "SUBSURF")
        ss.levels = subdiv
        ss.render_levels = subdiv
    dg = bpy.context.evaluated_depsgraph_get()
    ev = ob.evaluated_get(dg)
    tmp = ev.to_mesh()
    bm = bmesh.new()
    bm.from_mesh(tmp)
    ev.to_mesh_clear()
    bpy.data.objects.remove(ob)
    bpy.data.meshes.remove(me)
    return Part(bm).smooth(True)


# ── Builder ─────────────────────────────────────────────────────────────────────────────────────────────
def decimate(part: Part, ratio: float, min_tris: int = 48) -> Part:
    """Collapse-decimate a part to ``ratio`` of its triangles (Blender Decimate modifier, triangulated); parts at
    or under ``min_tris`` are kept as they are. Face int layers (``af_sub``) and smooth flags survive."""
    n = part.tris()
    if ratio >= 0.999 or n <= min_tris:
        return part
    me = bpy.data.meshes.new("_af_decimate")
    part.bm.to_mesh(me)
    ob = bpy.data.objects.new("_af_decimate", me)
    scene.link(ob)
    mod = ob.modifiers.new("dec", "DECIMATE")
    mod.decimate_type = "COLLAPSE"
    mod.ratio = max(ratio, min_tris / n)
    mod.use_collapse_triangulate = True
    dg = bpy.context.evaluated_depsgraph_get()
    m2 = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    bm = bmesh.new()
    bm.from_mesh(m2)
    bpy.data.objects.remove(ob)
    bpy.data.meshes.remove(me)
    bpy.data.meshes.remove(m2)
    part.bm.free()
    return Part(bm)


class Builder:
    """Accumulates parts into one mesh with region / part / outline tags.

    >>> b = Builder("SK_Hero_Warrior", pal, prefix="warrior")
    >>> b.regions(steel=Region("#9AA6BA", .42, .45), crimson=Region("#A82230"))
    >>> b.add(tube(...), "steel", Bind.blend("thigh_l", "calf_l"))
    >>> obj = b.build(grounding_height=1.76)
    """

    def __init__(self, asset: str, palette: _pal.Palette, prefix: str | None = None, lod_ratio: float = 1.0,
                 lod_skip: Callable[[str], bool] | None = None):
        """``lod_ratio`` < 1 builds a reduced LOD from the same generator code: every part (and hull proxy) is
        collapse-decimated to that ratio (:func:`decimate`) and parts whose name ``lod_skip(name)`` accepts are
        left out (rivets, strands, seams — details below the readability floor at LOD distances)."""
        self.asset = asset
        self.palette = palette
        self.prefix = prefix or asset
        self.bm = bmesh.new()
        self.swatch: dict[str, int] = {}
        self.parts: list[dict] = []
        self.lod_ratio = lod_ratio
        self.lod_skip = lod_skip
        self._pending: dict[int, dict] = {}       # padded hull proxies, appended at build (``hull_pad``)

    def region(self, key: str, region: _pal.Region) -> int:
        idx = self.palette.add(f"{self.prefix}.{key}", region)
        self.swatch[key] = idx
        return idx

    def regions(self, **kw: _pal.Region) -> None:
        for k, r in kw.items():
            self.region(k, r)

    def add(self, part: Part, region: str, bind=None, name: str | None = None, outline: bool = True,
            sub_regions: dict[int, str] | None = None, hull: Part | None = None, hull_pad: bool = False,
            covered_by: int | None = None) -> int:
        """Merge ``part``; ``sub_regions`` = {af_sub value: region} (e.g. a sheet's {1: 'lining'}).

        ``hull``: a simpler closed shape (same frame, same bind) used **only** to bake this part's outline hull
        (face attribute ``af_hullonly``; ``kit.outline.bake_hull`` keeps its hull copy and drops its toon faces),
        so a detailed part (mail rows, a grooved helm, a folded cape) gets a low-poly hull. Returns the part id
        (−1 when the LOD skips it).

        ``hull_pad`` (outlined parts): the hull is baked from a proxy — ``hull``, or a copy of the part itself — that
        ``build`` **inflates locally** (``pad_proxy``) until it encloses this part and every part later added with
        ``covered_by=<this id>``, + 0.5 mm. A low-poly proxy's chords, and no-hull trims, bands, seams, rivets and
        crease strips standing proud of the plate, otherwise eat the silhouette ink wherever they reach it (each mm
        proud = 0.12 px of the 1080p ink at the W1 default distance; ``kit.outline.protrusion_report`` lists them).
        ``covered_by``: this (usually ``outline=False``) part is enclosed by that part's padded proxy."""
        for r in [region, *(sub_regions or {}).values()]:
            if r not in self.swatch:
                raise KeyError(f"region {r!r} not declared for {self.asset}")
        if self.lod_skip is not None and name and self.lod_skip(name):
            part.bm.free()
            if hull is not None:
                hull.bm.free()
            return -1
        if self.lod_ratio < 1.0:
            part = decimate(part, self.lod_ratio)
            if hull is not None:
                hull = decimate(hull, self.lod_ratio)
        pid = len(self.parts)
        pad = bool(hull_pad and outline)
        self.parts.append({"name": name or f"part{pid}", "region": region, "bind": bind, "outline": outline,
                           "tris": part.tris(), "hullProxy": hull is not None or pad})
        subs = {k: self.swatch[v] for k, v in (sub_regions or {}).items()}
        if covered_by is not None and covered_by >= 0:
            if covered_by not in self._pending:
                raise ValueError(f"covered_by={covered_by}: part {covered_by} of {self.asset} has no padded hull "
                                 f"(add it with hull_pad=True)")
            self._pending[covered_by]["cover"].append(_vert_array(part.bm))
        if pad:
            proxy = hull if hull is not None else part.copy()
            hid = len(self.parts)
            self.parts.append({"name": f"{name or f'part{pid}'}_hull", "region": region, "bind": bind,
                               "outline": True, "tris": proxy.tris(), "hullOnly": True, "padded": True})
            self._pending[pid] = {"hull": proxy, "hid": hid, "region": region, "subs": subs or None,
                                  "cover": [_vert_array(part.bm)]}
        nohull = 0 if (outline and hull is None and not pad) else 1
        _append(self.bm, part.bm, {"af_swatch": self.swatch[region], "af_part": pid, "af_nohull": nohull,
                                   "af_hullonly": 0}, subs or None)
        part.bm.free()
        if pad:
            return pid
        if hull is not None and outline:
            hid = len(self.parts)
            self.parts.append({"name": f"{name or f'part{pid}'}_hull", "region": region, "bind": bind,
                               "outline": True, "tris": hull.tris(), "hullOnly": True})
            _append(self.bm, hull.bm, {"af_swatch": self.swatch[region], "af_part": hid, "af_nohull": 0,
                                       "af_hullonly": 1})
        if hull is not None:
            hull.bm.free()
        return pid

    def binds(self) -> dict:
        return {i: p["bind"] for i, p in enumerate(self.parts)}

    def build(self, name: str | None = None, grounding_height: float | None = None) -> bpy.types.Object:
        """Create the mesh object: triangulated n-gons, palette UVs, AF_Data colour (grounding weight)."""
        for pid, pend in sorted(self._pending.items()):      # padded hull proxies (``hull_pad``)
            mm = pad_proxy(pend["hull"], np.concatenate(pend["cover"]))
            self.parts[pend["hid"]]["padMm"] = round(mm * 1000.0, 2)
            _append(self.bm, pend["hull"].bm, {"af_swatch": self.swatch[pend["region"]], "af_part": pend["hid"],
                                               "af_nohull": 0, "af_hullonly": 1}, pend["subs"])
            pend["hull"].bm.free()
        self._pending = {}
        bm = self.bm
        ngons = [f for f in bm.faces if len(f.verts) > 4]
        if ngons:
            bmesh.ops.triangulate(bm, faces=ngons, quad_method="BEAUTY", ngon_method="BEAUTY")
        me = bpy.data.meshes.new(name or self.asset)
        bm.to_mesh(me)
        bm.free()
        self.bm = bmesh.new()
        # move bmesh int layers to proper face attributes (to_mesh keeps them as INT face attributes)
        self.palette.bake_uvs(me)
        set_af_data(me, grounding_height)
        obj = bpy.data.objects.new(name or self.asset, me)
        scene.link(obj)
        return obj


def _vert_array(bm: bmesh.types.BMesh) -> np.ndarray:
    return np.array([v.co[:] for v in bm.verts], np.float64).reshape(-1, 3)


def pad_proxy(proxy: Part, pts: np.ndarray, margin: float = 0.0005, iters: int = 12) -> float:
    """Inflate a closed hull proxy **locally** along its (position-shared, angle-weighted) vertex normals until
    every point of ``pts`` is inside it by ≥ ``margin``: each point outside pushes the vertices of its nearest
    proxy face out by its distance + margin, weighted toward the face vertices nearest to it (inverse distance,
    the nearest vertex at full strength), iterated — the hull grows only where something stands proud of it.
    Returns the largest vertex offset (m)."""
    from collections import defaultdict
    from mathutils.bvhtree import BVHTree
    bm = proxy.bm
    bm.verts.ensure_lookup_table()
    bm.faces.ensure_lookup_table()
    bm.normal_update()

    def key(co):
        return (round(co.x, 5), round(co.y, 5), round(co.z, 5))
    acc: dict = defaultdict(lambda: Vector((0.0, 0.0, 0.0)))
    for f in bm.faces:
        for lp in f.loops:
            acc[key(lp.vert.co)] += f.normal * lp.calc_angle()
    vn = []
    for v in bm.verts:
        n = acc[key(v.co)]
        vn.append(n.normalized() if n.length > 1e-12 else Vector(v.normal))
    base = [v.co.copy() for v in bm.verts]
    pad = np.zeros(len(bm.verts))
    pts = [Vector(p) for p in np.asarray(pts, np.float64).reshape(-1, 3)]
    for _ in range(iters):
        bvh = BVHTree.FromBMesh(bm)
        inc = np.zeros(len(bm.verts))
        for p in pts:
            loc, nrm, fi, _d = bvh.find_nearest(p)
            if loc is None:
                continue
            d = (p - loc).dot(nrm)
            if d > -0.2 * margin:
                vs = bm.faces[fi].verts
                w = [1.0 / ((v.co - loc).length + 1e-4) for v in vs]
                wm = max(w)
                for v, wi in zip(vs, w):
                    inc[v.index] = max(inc[v.index], (d + margin) * wi / wm)
        if not inc.any():
            break
        pad += inc
        for v in bm.verts:
            v.co = base[v.index] + vn[v.index] * float(pad[v.index])
        bm.normal_update()
    return float(pad.max()) if len(pad) else 0.0


def set_af_data(me: bpy.types.Mesh, grounding_height: float | None) -> None:
    """AF_Data (FLOAT_COLOR, point): R = grounding weight smoothstep(.38H, .05H, z), G/B reserved, A = 1."""
    nv = len(me.vertices)
    co = np.zeros(nv * 3, np.float32)
    me.vertices.foreach_get("co", co)
    z = co.reshape(nv, 3)[:, 2]
    data = np.zeros((nv, 4), np.float32)
    data[:, 3] = 1.0
    if grounding_height:
        from .shading import GROUNDING_FROM, GROUNDING_TO
        e0, e1 = GROUNDING_FROM * grounding_height, GROUNDING_TO * grounding_height
        t = np.clip((z - e0) / (e1 - e0), 0.0, 1.0)
        data[:, 0] = t * t * (3 - 2 * t)
    attr = me.color_attributes.get("AF_Data") or me.color_attributes.new("AF_Data", "FLOAT_COLOR", "POINT")
    attr.data.foreach_set("color", data.ravel())
    me.color_attributes.active_color = attr
    me.color_attributes.render_color_index = me.color_attributes.find("AF_Data")


def tri_count(obj: bpy.types.Object, material_index: int | None = None) -> int:
    me = obj.data
    n = 0
    for p in me.polygons:
        if material_index is None or p.material_index == material_index:
            n += p.loop_total - 2
    return n

"""Shared helpers for the hero generators (``heroes/warrior.py``, later mage / rogue).

* :class:`Profile` — smooth radial profile (z → side radius, front/back radius, forward offset) through web-style
  rings, with ``loft`` / ``point`` / ``normal`` so trims (bands, ridges, slits) sit exactly on a lofted surface.
* :func:`thick_patch` — a closed solid from an outer + inner grid of points (conforming plates, panels, capes).
* :func:`smooth_closed` — closed Catmull-Rom outline (shield / emblem outlines from the web control points).
* :class:`BP` — the web ``HumanPose`` in 3D (pelvis offset, lean/twist, head, IK foot/hand targets, weapon
  orientations, cloth flow, **whole-body spin about a pivot**) and :func:`to_pose` → ``kit.anim.Pose``.
  Keys interpolate *these* channels (angles linearly, so spins/rolls can exceed 180°), then the pose is solved.
* :func:`body_clip` — a ``kit.anim.Clip`` sampled from BP keys with the web easing.
* :func:`ship_hero` — bake, export SK + A_ + weapon SM FBX, manifest entries, full review set.

Body space = armature space: X = character left, −Y = forward, Z = up, metres, angles in degrees.
"""
from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Sequence

import bpy
import bmesh
import numpy as np
from mathutils import Matrix, Quaternion, Vector

import kit
from kit import anim, export, mesh as M, paths, review, rig as R, scene, shading
from kit.anim import Clip, Pose


def V(lat: float = 0.0, fwd: float = 0.0, up: float = 0.0) -> Vector:
    """Body-space vector from (lateral +left, forward, up)."""
    return Vector((lat, -fwd, up))


# ── smooth profiles ─────────────────────────────────────────────────────────────────────────────────────
def _hermite(xs: Sequence[float], ys: Sequence[float], x: float) -> float:
    """Non-uniform Catmull-Rom through (xs, ys), xs ascending; clamped at the ends."""
    n = len(xs)
    if x <= xs[0]:
        return ys[0]
    if x >= xs[-1]:
        return ys[-1]
    i = max(0, min(n - 2, int(np.searchsorted(xs, x) - 1)))
    x0, x1 = xs[i], xs[i + 1]

    def slope(k):
        if k == 0:
            return (ys[1] - ys[0]) / (xs[1] - xs[0])
        if k == n - 1:
            return (ys[-1] - ys[-2]) / (xs[-1] - xs[-2])
        return (ys[k + 1] - ys[k - 1]) / (xs[k + 1] - xs[k - 1])
    h = x1 - x0
    t = (x - x0) / h
    m0, m1 = slope(i) * h, slope(i + 1) * h
    t2, t3 = t * t, t * t * t
    return ((2 * t3 - 3 * t2 + 1) * ys[i] + (t3 - 2 * t2 + t) * m0 + (-2 * t3 + 3 * t2) * ys[i + 1]
            + (t3 - t2) * m1)


class Profile:
    """Radial profile through rings ``(z, rx, ry[, cy[, cx]])`` (any order): ``rx`` side radius, ``ry``
    front/back radius, ``cy`` centre Y (negative = forward), ``cx`` centre X. Superellipse exponent ``exp``.

    Angles ``a`` (degrees) go round the section: 0 = front (−Y), +90 = character left (+X), 180 = back.
    """

    def __init__(self, rings: Sequence[Sequence[float]], exp: float = 2.0):
        rs = sorted([tuple(r) + (0.0,) * (5 - len(r)) for r in rings], key=lambda r: r[0])
        self.z = [r[0] for r in rs]
        self.ch = [[r[i] for r in rs] for i in range(1, 5)]
        self.exp = exp

    @property
    def z0(self) -> float:
        return self.z[0]

    @property
    def z1(self) -> float:
        return self.z[-1]

    def at(self, z: float) -> tuple[float, float, float, float]:
        rx, ry, cy, cx = (_hermite(self.z, c, z) for c in self.ch)
        return max(rx, 1e-4), max(ry, 1e-4), cy, cx

    def point(self, z: float, a: float, out: float = 0.0, exp: float | None = None) -> Vector:
        """Surface point at height ``z`` and angle ``a`` (deg), pushed ``out`` metres along the normal."""
        rx, ry, cy, cx = self.at(z)
        e = exp or self.exp
        t = math.radians(a - 90.0)            # superellipse param: 0 → +X; front (a = 0) → −Y
        c, s = math.cos(t), math.sin(t)
        p = Vector((cx + rx * math.copysign(abs(c) ** (2.0 / e), c), cy + ry * math.copysign(abs(s) ** (2.0 / e), s), z))
        if out:
            p += self.normal(z, a, exp) * out
        return p

    def normal(self, z: float, a: float, exp: float | None = None) -> Vector:
        d = 1e-3
        p0 = self.point(z, a, 0, exp)
        ta = self.point(z, a + 0.5, 0, exp) - self.point(z, a - 0.5, 0, exp)
        tz = self.point(z + d, a, 0, exp) - self.point(z - d, a, 0, exp)
        n = ta.cross(tz)
        if n.length < 1e-9:
            return Vector((0, 0, 1))
        n.normalize()
        rx, ry, cy, cx = self.at(z)
        if n.dot(p0 - Vector((cx, cy, z))) < 0:
            n = -n
        return n

    def loft(self, z0: float | None = None, z1: float | None = None, rings: int = 12, sides: int = 24,
             exp: float | None = None, cap0: str | None = "fan", cap1: str | None = "fan", dome0: float = 0.0,
             dome1: float = 0.0, zs: Sequence[float] | None = None) -> M.Part:
        z0 = self.z0 if z0 is None else z0
        z1 = self.z1 if z1 is None else z1
        zz = list(zs) if zs is not None else [z0 + (z1 - z0) * i / (rings - 1) for i in range(rings)]
        ring_list = []
        for z in zz:
            rx, ry, cy, cx = self.at(z)
            ring_list.append((z, rx, ry, cx, cy))
        return M.loft_z(ring_list, sides=sides, exp=exp or self.exp, cap0=cap0, cap1=cap1, dome0=dome0, dome1=dome1)


# ── solids from point grids ─────────────────────────────────────────────────────────────────────────────
def thick_patch(outer: Sequence[Sequence[Vector]], inner: Sequence[Sequence[Vector]], wrap_u: bool = False,
                smooth: bool = True, sub_tags: bool = False) -> M.Part:
    """Closed solid between two grids (rows × cols) of matching points: ``outer`` surface, ``inner`` surface
    and side walls along the open boundary. ``wrap_u`` closes the columns into a ring (bands).
    ``sub_tags``: face layer ``af_sub`` 1 = outer, 2 = inner, 0 = walls (Builder ``sub_regions``)."""
    bm = bmesh.new()
    rows, cols = len(outer), len(outer[0])
    vo = [[bm.verts.new(tuple(p)) for p in row] for row in outer]
    vi = [[bm.verts.new(tuple(p)) for p in row] for row in inner]
    sub = bm.faces.layers.int.new("af_sub") if sub_tags else None
    ucount = cols if wrap_u else cols - 1

    def face(vs, tag):
        try:
            f = bm.faces.new(vs)
        except ValueError:
            return
        if sub is not None:
            f[sub] = tag
    for r in range(rows - 1):
        for c in range(ucount):
            c1 = (c + 1) % cols
            face((vo[r][c], vo[r + 1][c], vo[r + 1][c1], vo[r][c1]), 1)
            face((vi[r][c], vi[r][c1], vi[r + 1][c1], vi[r + 1][c]), 2)
    for c in range(ucount):                       # top / bottom walls
        c1 = (c + 1) % cols
        face((vo[0][c], vo[0][c1], vi[0][c1], vi[0][c]), 0)
        face((vo[-1][c], vi[-1][c], vi[-1][c1], vo[-1][c1]), 0)
    if not wrap_u:                                # side walls
        for r in range(rows - 1):
            face((vo[r][0], vi[r][0], vi[r + 1][0], vo[r + 1][0]), 0)
            face((vo[r][-1], vo[r + 1][-1], vi[r + 1][-1], vi[r][-1]), 0)
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    # recalc may flip everything inward for thin shells: make sure the 'outer' faces point away from 'inner'
    if sub is not None:
        flip = 0
        cnt = 0
        for f in bm.faces:
            if f[sub] == 1:
                c = f.calc_center_median()
                cnt += 1
                # outer face normal should point away from the matching inner point (approx: use the first one)
                if f.normal.dot(c - _closest_inner(inner, c)) < 0:
                    flip += 1
        if cnt and flip > cnt / 2:
            bmesh.ops.reverse_faces(bm, faces=bm.faces)
    bm.normal_update()
    part = M.Part(bm)
    return part.smooth(True) if smooth else part


def _closest_inner(inner, c: Vector) -> Vector:
    best, bd = None, 1e9
    for row in inner[:: max(1, len(inner) // 4)]:
        for p in row[:: max(1, len(row) // 4)]:
            d = (p - c).length
            if d < bd:
                best, bd = p, d
    return best


def surface_band(prof: Profile, z: float, height: float, thick: float, a0: float = -180.0, a1: float = 180.0,
                 segs: int = 40, rise: float = 0.0, out0: float = -0.004, exp: float | None = None,
                 z_of_a: Callable[[float], float] | None = None) -> M.Part:
    """A band hugging ``prof`` between z ± height/2 (closed ring when a0..a1 spans 360°): trims, belts,
    visor slits. ``thick`` = how far it stands proud, ``out0`` = inner face offset (slightly sunk)."""
    full = abs((a1 - a0) - 360.0) < 1e-6
    n = segs if full else segs + 1
    angs = [a0 + (a1 - a0) * i / segs for i in range(n)]
    outer, inner = [], []
    for dz in (-height / 2, height / 2):
        ro, ri = [], []
        for a in angs:
            zc = (z_of_a(a) if z_of_a else z) + dz
            ro.append(prof.point(zc, a, thick, exp))
            ri.append(prof.point(zc, a, out0, exp))
        outer.append(ro)
        inner.append(ri)
    return thick_patch(outer, inner, wrap_u=full).auto_sharp(50)


def surface_strip(prof: Profile, a: float, z0: float, z1: float, width: float, thick: float, steps: int = 10,
                  out0: float = -0.004, exp: float | None = None, half_round: bool = False) -> M.Part:
    """A vertical strip (ridge, seam, vertical visor slit) on ``prof`` at angle ``a`` from z0 to z1."""
    outer, inner = [], []
    zs = [z0 + (z1 - z0) * i / steps for i in range(steps + 1)]
    for z in zs:
        rx, ry, cy, cx = prof.at(z)
        circ = max(rx, ry)
        da = math.degrees(width / 2 / max(circ, 1e-3))
        if half_round:
            row_o = [prof.point(z, a + da * u, thick * math.sqrt(max(0.0, 1 - u * u)) + 0.0005, exp)
                     for u in (-1, -0.5, 0, 0.5, 1)]
            row_i = [prof.point(z, a + da * u, out0, exp) for u in (-1, -0.5, 0, 0.5, 1)]
        else:
            row_o = [prof.point(z, a - da, thick, exp), prof.point(z, a + da, thick, exp)]
            row_i = [prof.point(z, a - da, out0, exp), prof.point(z, a + da, out0, exp)]
        outer.append(row_o)
        inner.append(row_i)
    return thick_patch(outer, inner).auto_sharp(50)


def surface_dot(prof: Profile, z: float, a: float, r: float, h: float, sides: int = 8,
                exp: float | None = None) -> M.Part:
    """Rivet / stud: a small dome sitting on ``prof``."""
    p = prof.point(z, a, 0.0, exp)
    n = prof.normal(z, a, exp)
    d = M.lathe([(r, -0.004), (r, h * 0.35), (r * 0.7, h * 0.85), (r * 0.15, h)], sides=sides, cap1="fan")
    return d.transform(R.frame(p, n, (0, 0, 1)) @ Matrix.Rotation(math.radians(-90), 4, "X"))


def smooth_closed(pts: Sequence[tuple[float, float]], per_seg: int = 6) -> list[tuple[float, float]]:
    """Closed centripetal-ish Catmull-Rom through 2-D control points (CCW stays CCW)."""
    P = [Vector((x, y)) for x, y in pts]
    n = len(P)
    out = []
    for i in range(n):
        p0, p1, p2, p3 = P[(i - 1) % n], P[i], P[(i + 1) % n], P[(i + 2) % n]
        for s in range(per_seg):
            t = s / per_seg
            t2, t3 = t * t, t * t * t
            q = 0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3)
            out.append((q.x, q.y))
    return out


def signed_area(pts: Sequence[tuple[float, float]]) -> float:
    return 0.5 * sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1]
                     for i in range(len(pts)))


def ccw(pts: Sequence[tuple[float, float]]) -> list[tuple[float, float]]:
    return list(pts) if signed_area(pts) > 0 else list(reversed(pts))


def offset_closed(pts: Sequence[tuple[float, float]], d: float) -> list[tuple[float, float]]:
    """Offset a CCW closed polyline outward by ``d`` (negative = inward), miter-limited."""
    n = len(pts)
    out = []
    for i in range(n):
        a, b, c = Vector(pts[i - 1]), Vector(pts[i]), Vector(pts[(i + 1) % n])
        e0 = (b - a).normalized()
        e1 = (c - b).normalized()
        n0 = Vector((e0.y, -e0.x))
        n1 = Vector((e1.y, -e1.x))
        m = (n0 + n1)
        if m.length < 1e-6:
            m = n0
        m.normalize()
        k = 1.0 / max(0.35, m.dot(n0))
        q = b + m * d * k
        out.append((q.x, q.y))
    return out


def plate2d(outline: Sequence[tuple[float, float]], thickness: float, bevel: float = 0.0) -> M.Part:
    return M.plate(ccw(outline), thickness, bevel=bevel)


# ── body pose model ─────────────────────────────────────────────────────────────────────────────────────
DIR_FIELDS = {"wpn", "wpn_up", "off", "off_up", "elbow_l", "elbow_r", "knee_l", "knee_r", "spin_axis"}
BP_DEFAULTS: dict = dict(
    pelvis=Vector((0, 0, 0)), hip_pitch=0.0, hip_yaw=0.0, hip_roll=0.0,
    lean=0.0, twist=0.0, side=0.0,
    head_pitch=0.0, head_yaw=0.0, head_roll=0.0,
    foot_l=None, foot_r=None, foot_l_rot=(0.0, 0.0, 0.0), foot_r_rot=(0.0, 0.0, 0.0), toe_l=0.0, toe_r=0.0,
    knee_l=Vector((0.15, -1, 0)), knee_r=Vector((-0.15, -1, 0)),
    hand_l=None, hand_r=None, elbow_l=Vector((0.6, 0.8, -0.3)), elbow_r=Vector((-0.6, 0.8, -0.3)),
    wrist_l=(0.0, 0.0, 0.0), wrist_r=(0.0, 0.0, 0.0), clav_l=(0.0, 0.0), clav_r=(0.0, 0.0),
    wpn=Vector((0, -1, 0)), wpn_up=Vector((0, 0, 1)), off=Vector((0, -1, 0)), off_up=Vector((0, 0, 1)),
    spin=0.0, spin_axis=Vector((1, 0, 0)), pivot=Vector((0, 0, 0.8)),
    flow=0.12, wave=0.0, sway=0.0, plume=0.0, lift=0.0, cling=0.0, curl=0.0, off_fit=0.0,
)


class BP:
    """Keyframeable body pose (channels above). ``foot_*`` / ``hand_*`` are ankle / wrist targets
    (body space); ``foot_*_rot`` = (pitch toe-down +, roll, yaw toe-out +) degrees; ``clav_*`` = (lift, swing);
    ``wpn``/``off`` + ``*_up`` = item bone +Y / +Z (body space); ``spin`` degrees about ``spin_axis`` through
    ``pivot``; ``flow`` cloth streaming 0..1 (web), ``wave`` cloth phase (rad), ``lift`` extra ground offset."""

    __slots__ = ("c",)

    def __init__(self, **kw):
        object.__setattr__(self, "c", {})
        for k, v in BP_DEFAULTS.items():
            self.c[k] = v.copy() if isinstance(v, Vector) else v
        self.set(**kw)

    def set(self, **kw) -> "BP":
        for k, v in kw.items():
            if k not in BP_DEFAULTS:
                raise KeyError(f"unknown BP channel {k}")
            if isinstance(v, (tuple, list)) and k not in ("foot_l_rot", "foot_r_rot", "wrist_l", "wrist_r",
                                                          "clav_l", "clav_r"):
                v = Vector(v)
            if isinstance(v, Vector):
                v = v.copy()
                if k in DIR_FIELDS:
                    v.normalize()
            self.c[k] = v
        return self

    def __getattr__(self, k):
        try:
            return object.__getattribute__(self, "c")[k]
        except KeyError:
            raise AttributeError(k)

    def but(self, **kw) -> "BP":
        b = BP()
        for k, v in self.c.items():
            b.c[k] = v.copy() if isinstance(v, Vector) else v
        return b.set(**kw)

    def copy(self) -> "BP":
        return self.but()

    def moved(self, d: Vector) -> "BP":
        """Shift pelvis + IK targets (whole body translation)."""
        b = self.copy()
        b.c["pelvis"] = b.pelvis + d
        for k in ("foot_l", "foot_r", "hand_l", "hand_r"):
            if b.c[k] is not None:
                b.c[k] = b.c[k] + d
        b.c["pivot"] = b.pivot + d
        return b


def _slerp_dir(a: Vector, b: Vector, t: float) -> Vector:
    a, b = a.normalized(), b.normalized()
    d = max(-1.0, min(1.0, a.dot(b)))
    if d > 0.9999:
        return a.lerp(b, t).normalized()
    if d < -0.9999:      # opposite: rotate about any perpendicular
        ax = a.orthogonal().normalized()
        return Quaternion(ax, math.pi * t) @ a
    q = a.rotation_difference(b)
    ang = q.angle
    return Quaternion(q.axis, ang * t) @ a


def lerp_bp(a: BP, b: BP, t: float) -> BP:
    out = BP()
    for k in BP_DEFAULTS:
        va, vb = a.c[k], b.c[k]
        if va is None or vb is None:
            out.c[k] = (vb if t >= 0.5 else va) if (va is None or vb is None) else None
            if va is None and vb is not None:
                out.c[k] = vb.copy()
            elif vb is None and va is not None:
                out.c[k] = va.copy()
            continue
        if isinstance(va, Vector):
            out.c[k] = _slerp_dir(va, vb, t) if k in DIR_FIELDS else va.lerp(vb, t)
        elif isinstance(va, tuple):
            out.c[k] = tuple(x + (y - x) * t for x, y in zip(va, vb))
        else:
            out.c[k] = va + (vb - va) * t
    return out


def blade(theta: float, lat: float = 0.0, roll: float = 0.0) -> tuple[Vector, Vector]:
    """Web weapon angle → (dir, up): ``theta`` degrees from vertical (+ = forward), ``lat`` lateral tilt
    (+ = toward the character's left), ``roll`` about the blade. ``up`` (the item's +Z, the leading edge for
    the sword) = the direction the tip moves when ``theta`` increases (forward swing)."""
    th = math.radians(theta)
    d = V(0, math.sin(th), math.cos(th))
    e = V(0, math.cos(th), -math.sin(th))
    if lat:
        d = (d + V(lat, 0, 0)).normalized()
        e = (e - d * e.dot(d)).normalized()
    if roll:
        e = Quaternion(d, math.radians(roll)) @ e
    return d, e


@dataclass
class ChainInfo:
    """Rest angles of a cloth chain (degrees back from straight down, per bone)."""
    names: list
    rest: list


def chain_rest_angles(rig: R.Rig, prefix: str) -> ChainInfo:
    names = sorted([b.name for b in rig.obj.data.bones if b.name.startswith(prefix + "_")])
    rest = []
    for n in names:
        d = (rig.tail(n) - rig.head(n)).normalized()
        rest.append(math.degrees(math.atan2(d.y, -d.z)))      # + = back (+Y) from straight down
    return ChainInfo(names, rest)


def set_chain(p: Pose, info: ChainInfo, absolute: Sequence[float], acc: float, side: Sequence[float] = ()) -> None:
    """Rotate a hanging chain so bone k points ``absolute[k]`` degrees back from straight down, given the
    accumulated forward pitch ``acc`` of the chain's parent (rel pitch adds to the parent's)."""
    total = 0.0
    for k, n in enumerate(info.names):
        want = absolute[min(k, len(absolute) - 1)]
        rel = want - info.rest[k] - acc - total
        total += rel
        roll = side[k] if k < len(side) else 0.0
        p.rot(n, pitch=rel, roll=roll)


def to_pose(rig: R.Rig, bp: BP, cloth: Callable[[Pose, BP], None] | None = None,
            grips: Sequence[str] = ("r",)) -> Pose:
    """Solve a :class:`BP` into a kit Pose (IK targets, aims, spin).

    ``grips``: hands whose orientation follows their item with a **natural hammer grip** — the item points
    along ``wpn``/``off`` and is rolled about its own axis so the fist stays in line with the forearm
    (``wrist_*`` roll = extra roll in degrees); other hands follow the forearm (``wrist_*`` = rel rotation)."""
    p = Pose(rig)
    J = rig.joints
    p.move("pelvis", bp.pelvis + Vector((0, 0, bp.lift)))
    p.rot("pelvis", pitch=bp.hip_pitch, roll=bp.hip_roll, yaw=bp.hip_yaw)
    for b, w in (("spine_01", 0.30), ("spine_02", 0.35), ("spine_03", 0.35)):
        p.rot(b, pitch=bp.lean * w, roll=bp.side * w, yaw=bp.twist * w)
    p.rot("neck_01", pitch=bp.head_pitch * 0.35, yaw=bp.head_yaw * 0.4, roll=bp.head_roll * 0.3)
    p.rot("head", pitch=bp.head_pitch * 0.65, yaw=bp.head_yaw * 0.6, roll=bp.head_roll * 0.7)
    lift_v = Vector((0, 0, bp.lift))
    for s in ("l", "r"):
        tgt = getattr(bp, f"foot_{s}")
        tgt = (J[f"ankle_{s}"] if tgt is None else tgt) + lift_v
        pitch, roll, yaw = getattr(bp, f"foot_{s}_rot")
        sg = 1.0 if s == "l" else -1.0
        p.foot(s, at=tgt, pole=getattr(bp, f"knee_{s}"))
        p.world(f"foot_{s}", pitch=pitch, roll=roll * sg, yaw=yaw * sg)
        toe = getattr(bp, f"toe_{s}")
        if toe:
            p.rot(f"ball_{s}", pitch=-toe)
    for s in ("l", "r"):
        tgt = getattr(bp, f"hand_{s}")
        if tgt is not None:
            p.hand(s, at=tgt + lift_v, pole=getattr(bp, f"elbow_{s}"))
        if s not in grips:
            wp, wr, wy = getattr(bp, f"wrist_{s}")
            p.rot(f"hand_{s}", pitch=wp, roll=wr, yaw=wy)
        cl, cs = getattr(bp, f"clav_{s}")
        if cl:
            p.lift(f"clavicle_{s}", cl)
        if cs:
            p.swing(f"clavicle_{s}", cs)
    p.aim("weapon_r", bp.wpn, bp.wpn_up)
    off, off_up = bp.off, bp.off_up
    if bp.off_fit > 0 and bp.hand_l is not None:
        # strapped shield: the forearm runs behind the board, so it must point *out of* the face direction by a
        # margin (elbow ≥ ~4 cm behind the grip plane: couter wing and vambrace clear the back); turn the face
        # just enough
        f = (anim.evaluate(p, return_matrices=True)["lowerarm_l"].to_3x3() @ Vector((0, 1, 0))).normalized()
        d = off.dot(f)
        lim = 0.15 * bp.off_fit
        if d < lim:
            off = (off + f * (lim - d)).normalized()
            off_up = (off_up - off * off_up.dot(off)).normalized()
    p.aim("weapon_l", off, off_up)
    if grips:
        mats = anim.evaluate(p, return_matrices=True)
        for s in grips:
            d = bp.wpn if s == "r" else bp.off
            if _swivel_elbow(rig, p, mats, s, d):
                mats = anim.evaluate(p, return_matrices=True)
        for s in grips:
            wb = f"weapon_{s}"
            d = bp.wpn if s == "r" else bp.off
            f = (mats[f"lowerarm_{s}"].to_3x3() @ Vector((0, 1, 0))).normalized()
            fp = f - d * f.dot(d)
            roll = getattr(bp, f"wrist_{s}")[1]
            if fp.length < 0.15:
                up = (bp.wpn_up if s == "r" else bp.off_up).copy()
            else:
                fp.normalize()
                a, c = _grip_coeffs(rig, s)
                up = fp * c - d.cross(fp) * a
            if roll:
                up = Quaternion(d, math.radians(roll)) @ up
            p.aim(wb, d, up)
            p.abs[f"hand_{s}"] = p.abs[wb].copy()
    if cloth is not None:
        cloth(p, bp)
    if abs(bp.spin) > 1e-6:
        apply_spin(rig, p, bp.spin_axis, bp.spin, bp.pivot + lift_v)
    return p


GRIP_MAX_ALIGN = 0.45     # cos 63°: a hammer grip cannot bend the wrist further (pommel into the forearm)


def _swivel_elbow(rig: R.Rig, p: Pose, mats: dict, s: str, d: Vector) -> bool:
    """Hammer grip sanity: the item leaves the fist at ~90° to the forearm, so a blade that points *along* the
    forearm (raised sword over a raised forearm, a sweep that ends pointing where the forearm points) drives the
    grip and pommel into the vambrace. When ``d · forearm`` exceeds ``GRIP_MAX_ALIGN`` the elbow is swivelled
    about the shoulder → wrist axis toward the blade's side (the analytic two-bone elbow circle) — the least turn
    that brings the forearm back to a natural wrist bend. Returns True when the pole was changed."""
    chain = f"arm_{s}"
    if chain not in p.ik:
        return False
    f0 = (mats[f"lowerarm_{s}"].to_3x3() @ Vector((0, 1, 0))).normalized()
    if f0.dot(d) <= GRIP_MAX_ALIGN:
        return False
    tgt, pole = p.ik[chain]
    S = mats[f"upperarm_{s}"].translation.copy()
    bones = rig.obj.data.bones
    l1, l2 = bones[f"upperarm_{s}"].length, bones[f"lowerarm_{s}"].length
    w = tgt - S
    D = max(abs(l1 - l2) + 1e-4, min(l1 + l2 - 1e-5, w.length))
    u = w.normalized()
    a = (l1 * l1 - l2 * l2 + D * D) / (2 * D)
    b = math.sqrt(max(l1 * l1 - a * a, 0.0))
    v0 = pole - u * pole.dot(u)
    dp = d - u * d.dot(u)
    if v0.length < 1e-6 or dp.length < 1e-6:
        return False
    v0.normalize()
    dp.normalize()

    def fwd(v: Vector) -> Vector:
        return ((S + u * D) - (S + u * a + v * b)).normalized()
    best = None
    for i in range(1, 37):
        v = _slerp_dir(v0, dp, i / 36.0)
        if fwd(v).dot(d) <= GRIP_MAX_ALIGN:
            best = v
            break
    if best is None:
        best = dp
    p.ik[chain] = (tgt, best)
    m2 = anim.evaluate(p, return_matrices=True)
    f1 = (m2[f"lowerarm_{s}"].to_3x3() @ Vector((0, 1, 0))).normalized()
    if f1.dot(d) >= f0.dot(d):            # the solver refused that bend (never backwards): keep the authored pole
        p.ik[chain] = (tgt, pole)
        return False
    return True


_GRIP_CACHE: dict = {}


def _grip_coeffs(rig: R.Rig, s: str) -> tuple[float, float]:
    """Rest finger direction expressed in the item bone frame → (a, c) on its X / Z axes (normalised)."""
    key = (rig.obj.name, s)
    if key not in _GRIP_CACHE:
        bones = rig.obj.data.bones
        hr = bones[f"hand_{s}"].matrix_local.to_3x3()
        wr = bones[f"weapon_{s}"].matrix_local.to_3x3()
        v = wr.inverted() @ hr @ Vector((0, 1, 0))
        n = math.hypot(v.x, v.z)
        _GRIP_CACHE[key] = (v.x / n, v.z / n)
    return _GRIP_CACHE[key]


def apply_spin(rig: R.Rig, p: Pose, axis: Vector, deg: float, pivot: Vector) -> None:
    """Rigidly rotate the whole posed body about ``pivot`` (pelvis frame, IK targets/poles, absolute aims)."""
    q = Quaternion(Vector(axis).normalized(), math.radians(deg))
    Rm = q.to_matrix()
    head0 = rig.head("pelvis")
    h = head0 + p.loc.get("pelvis", Vector())
    h2 = pivot + Rm @ (h - pivot)
    p.loc["pelvis"] = h2 - head0
    p.rel["pelvis"] = q @ p.rel.get("pelvis", Quaternion())
    for k, (t, pole) in list(p.ik.items()):
        p.ik[k] = (pivot + Rm @ (t - pivot), Rm @ pole)
    for k, a in list(p.abs.items()):
        p.abs[k] = q @ a


def posed_points(rig: R.Rig, p: Pose, probes: Sequence[tuple]) -> list[tuple[Vector, float]]:
    """World points of probes ``(bone, where, radius)`` for a solved pose: ``where`` is a fraction along the bone
    or a bone-local offset in metres (x, y along the bone, z) — e.g. a sword tip or shield rim point on
    ``weapon_r`` / ``weapon_l``, whose local frame is the item mesh frame."""
    mats = anim.evaluate(p, return_matrices=True)
    out = []
    for bone, f, r in probes:
        m = mats[bone]
        if isinstance(f, (int, float)):
            loc = Vector((0.0, rig.obj.data.bones[bone].length * f, 0.0))
        else:
            loc = Vector(f)
        out.append((m @ loc, r))
    return out


class SkinPoints:
    """A vertex sample of skinned meshes posed by **linear blend skinning in numpy** straight from a kit Pose's
    bone matrices — the exact deformation the Armature modifier (and UE) applies, without a depsgraph update, so
    pose solvers can test the real armour surface (cloth collision, feet on the floor, weapons in the ground).

    ``add_mesh(obj, part_names, pick)`` samples a Builder mesh (toon faces of the parts ``pick(name)`` accepts);
    ``add_rigid(obj, bone, step)`` samples an item attached to ``bone`` (weapon meshes: item-local = bone frame).
    Each point keeps a tag (its part name) so callers can select subsets (``mask(tags)``)."""

    def __init__(self, rig: R.Rig):
        self.rig = rig
        bones = rig.obj.data.bones
        self.bone_names = [b.name for b in bones]
        self.bidx = {n: i for i, n in enumerate(self.bone_names)}
        self.rest_inv = np.array([np.array(b.matrix_local.inverted()) for b in bones], np.float64)
        self.co = np.zeros((0, 4))
        self.idx = np.zeros((0, 4), np.int64)
        self.w = np.zeros((0, 4))
        self.tags: list[str] = []

    def _append(self, co: np.ndarray, idx: np.ndarray, w: np.ndarray, tags: list[str]) -> None:
        self.co = np.vstack([self.co, np.c_[co, np.ones(len(co))]])
        self.idx = np.vstack([self.idx, idx])
        self.w = np.vstack([self.w, w])
        self.tags += tags

    def add_mesh(self, obj: bpy.types.Object, part_names: Sequence[str], pick: Callable[[str], bool],
                 step: int = 1) -> "SkinPoints":
        me = obj.data
        nf = len(me.polygons)
        fp = np.zeros(nf, np.int32)
        me.attributes["af_part"].data.foreach_get("value", fp)
        mi = np.zeros(nf, np.int32)
        me.polygons.foreach_get("material_index", mi)
        lt = np.zeros(nf, np.int32)
        me.polygons.foreach_get("loop_total", lt)
        lv = np.zeros(len(me.loops), np.int32)
        me.loops.foreach_get("vertex_index", lv)
        vpart = np.full(len(me.vertices), -1, np.int32)
        fpv = np.where(mi == 0, fp, -1)
        vpart[lv] = np.repeat(fpv, lt)
        keep = [i for i in range(len(me.vertices)) if vpart[i] >= 0 and pick(part_names[vpart[i]])][::step]
        if not keep:
            return self
        gname = {g.index: g.name for g in obj.vertex_groups}
        idx = np.zeros((len(keep), 4), np.int64)
        w = np.zeros((len(keep), 4))
        co = np.zeros((len(keep), 3))
        for r, vi in enumerate(keep):
            v = me.vertices[vi]
            gs = sorted(((g.weight, gname[g.group]) for g in v.groups if gname.get(g.group) in self.bidx),
                        reverse=True)[:4]
            tot = sum(x for x, _ in gs) or 1.0
            for j, (x, n) in enumerate(gs):
                idx[r, j], w[r, j] = self.bidx[n], x / tot
            co[r] = tuple(v.co)
        self._append(co, idx, w, [part_names[vpart[i]] for i in keep])
        return self

    def add_rigid(self, obj: bpy.types.Object, bone: str, step: int = 1, tag: str | None = None) -> "SkinPoints":
        me = obj.data
        co = np.zeros(len(me.vertices) * 3)
        me.vertices.foreach_get("co", co)
        co = co.reshape(-1, 3)[::step]
        # item-local = posed bone frame → express in the bone's rest frame so the generic skinning applies
        rest = np.array(self.rig.obj.data.bones[bone].matrix_local)
        co = (np.c_[co, np.ones(len(co))] @ rest.T)[:, :3]
        idx = np.zeros((len(co), 4), np.int64)
        idx[:, 0] = self.bidx[bone]
        w = np.zeros((len(co), 4))
        w[:, 0] = 1.0
        self._append(co, idx, w, [tag or bone] * len(co))
        return self

    def mask(self, tags) -> np.ndarray:
        want = set(tags)
        return np.array([t in want for t in self.tags], bool)

    def posed(self, mats: dict, sel: np.ndarray | None = None) -> np.ndarray:
        """Posed armature-space positions (N×3) for bone matrices ``anim.evaluate(pose, True)``."""
        S = np.array([np.array(mats[n]) for n in self.bone_names], np.float64) @ self.rest_inv
        co, idx, w = (self.co, self.idx, self.w) if sel is None else (self.co[sel], self.idx[sel], self.w[sel])
        Sv = np.einsum("vjab,vb->vja", S[idx], co)
        return (Sv[..., :3] * w[..., None]).sum(1)


def ground_clamp(rig: R.Rig, bp: BP, cloth, probes, clearance=0.0, snap: bool = False) -> BP:
    """Raise the whole pose so no probe goes below z = clearance (rolls, falls); ``snap`` also lowers it so the
    lowest probe rests exactly on the ground (kneeling, lying). ``probes``: probe spheres ``(bone, where, r)``
    or a :class:`SkinPoints` sample of the real meshes (exact)."""
    if isinstance(probes, SkinPoints):          # clearance may be per point (e.g. plates resting on cloth)
        p = to_pose(rig, bp, None)
        z = probes.posed(anim.evaluate(p, return_matrices=True))[:, 2] - clearance
        low = float(z.min())
        clearance = 0.0
    else:
        p = to_pose(rig, bp, cloth)
        low = min(pt.z - r for pt, r in posed_points(rig, p, probes))
    if low < clearance or snap:
        return bp.but(lift=bp.lift + (clearance - low))
    return bp


def plant_feet(rig: R.Rig, bp: BP, feet: dict, clearance: float = 0.0) -> BP:
    """Keep each foot on or above the floor: ``feet`` = {'l': SkinPoints-mask pair, 'r': …} as
    ``(points, mask)``. A foot whose lowest point (sole, toe cap, heel) is below ``clearance`` has its ankle target
    raised by the deficit — feet are oriented absolutely, so the whole foot rises; the knee takes up the slack.
    Upright poses only (spins use :func:`ground_clamp`)."""
    if abs(bp.spin) > 1e-6 and abs(Vector(bp.spin_axis).normalized().z) < 0.999:
        return bp                                     # (turns about the vertical keep the feet upright)
    p = to_pose(rig, bp, None)
    mats = anim.evaluate(p, return_matrices=True)
    out = bp
    for s, (pts, sel) in feet.items():
        low = float(pts.posed(mats, sel)[:, 2].min())          # the pose already carries ``lift``
        if low < clearance:
            tgt = getattr(bp, f"foot_{s}")
            tgt = (rig.joints[f"ankle_{s}"] if tgt is None else tgt) + Vector((0, 0, clearance - low))
            out = out.but(**{f"foot_{s}": tgt})
    return out


# ── clips ───────────────────────────────────────────────────────────────────────────────────────────────
@dataclass
class K:
    t: float          # normalised key time (× key span)
    bp: BP
    ease: str = "smooth"


def sample_keys(keys: Sequence[K], span_ms: float, t_ms: float) -> BP:
    ks = sorted(keys, key=lambda k: k.t)
    t = t_ms / span_ms if span_ms > 0 else 0.0
    if t <= ks[0].t:
        return ks[0].bp.copy()
    if t >= ks[-1].t:
        return ks[-1].bp.copy()
    for a, b in zip(ks, ks[1:]):
        if a.t <= t <= b.t:
            u = (t - a.t) / max(b.t - a.t, 1e-9)
            return lerp_bp(a.bp, b.bp, anim.ease(u, b.ease))
    return ks[-1].bp.copy()


class ClothClock:
    """The clip time being solved, so a cloth solver can read the body's **past** (secondary-motion lag: a cape
    tip follows the lean / flow of 60 ms ago). Outside a clip (single poses, solvers) ``past`` returns None."""
    sampler: Callable[[float], BP] | None = None
    t: float = 0.0
    length: float = 0.0
    loop: bool = False

    @classmethod
    def past(cls, dt_ms: float) -> BP | None:
        if cls.sampler is None or dt_ms <= 0.0:
            return None
        t = cls.t - dt_ms
        t = t % cls.length if cls.loop and cls.length > 0 else max(0.0, t)
        return cls.sampler(t)


def body_clip(rig: R.Rig, name: str, length_ms: float, sampler: Callable[[float], BP], cloth=None,
              loop: bool = False, notifies: dict | None = None, ref_speed: float | None = None,
              additive: bool = False, post: Callable[[BP, float], BP] | None = None) -> Clip:
    def fn(t_ms: float) -> Pose:
        bp = sampler(t_ms)
        if post is not None:
            bp = post(bp, t_ms)
        ClothClock.sampler, ClothClock.t, ClothClock.length, ClothClock.loop = sampler, t_ms, length_ms, loop
        try:
            return to_pose(rig, bp, cloth)
        finally:
            ClothClock.sampler = None
    return Clip(name, length_ms, loop=loop, sampler=fn, notifies=notifies or {}, ref_speed=ref_speed,
                additive=additive)


def keyed(keys: Sequence[K], span_ms: float, layer: Callable[[BP, float], BP] | None = None):
    def s(t_ms: float) -> BP:
        bp = sample_keys(keys, span_ms, t_ms)
        return layer(bp, t_ms) if layer else bp
    return s


# ── ship ────────────────────────────────────────────────────────────────────────────────────────────────
def ship_weapon(obj: bpy.types.Object, asset: str, pal, sockets: Sequence[dict], socket_bone: str,
                weapon_type: str, extra: dict | None = None) -> dict:
    fbx = export.export_static_mesh(obj, asset, "Weapons", sockets)
    man = export.Manifest()
    man.set_palette(pal)
    e = export.static_entry(obj, fbx, "Weapons", pal, "weapon", sockets=sockets,
                            extra={"attachSocket": socket_bone, "weaponType": weapon_type, **(extra or {})})
    man.set_asset(asset, e)
    man.save()
    return {"fbx": str(fbx), "entry": e}


def slab2d(outline: Sequence[tuple[float, float]], y_back: float, y_front: float, rings: int = 6) -> M.Part:
    """Closed slab from a star-shaped CCW outline in the XZ plane (x across, z up), between depths ``y_back`` and
    ``y_front`` (+Y = front). Both faces are polar grids with a centre fan, so the slab bends smoothly."""
    bm = bmesh.new()
    cx = sum(p[0] for p in outline) / len(outline)
    cz = sum(p[1] for p in outline) / len(outline)
    n = len(outline)

    def face_grid(y):
        c = bm.verts.new((cx, y, cz))
        grid = []
        for r in range(1, rings + 1):
            f = r / rings
            grid.append([bm.verts.new((cx + (x - cx) * f, y, cz + (z - cz) * f)) for x, z in outline])
        return c, grid
    cf, gf = face_grid(y_front)
    cb, gb = face_grid(y_back)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((cf, gf[0][i], gf[0][j]))
        bm.faces.new((cb, gb[0][j], gb[0][i]))
        for r in range(rings - 1):
            bm.faces.new((gf[r][i], gf[r + 1][i], gf[r + 1][j], gf[r][j]))
            bm.faces.new((gb[r][i], gb[r][j], gb[r + 1][j], gb[r + 1][i]))
        bm.faces.new((gf[-1][i], gb[-1][i], gb[-1][j], gf[-1][j]))
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return M.Part(bm).smooth(True)


def split_faces(part: M.Part, pred: Callable[[bmesh.types.BMesh, bmesh.types.BMFace], bool]) -> tuple[M.Part, M.Part]:
    """Split a part into (faces where ``pred(bm, face)`` is False, faces where it is True) — e.g. to keep concave
    grooves out of the outline hull (an inverted hull inks every concave wall that faces away from the camera)."""
    outs = []
    for want in (False, True):
        bm = part.bm.copy()
        kill = [f for f in bm.faces if bool(pred(bm, f)) != want]
        bmesh.ops.delete(bm, geom=kill, context="FACES")
        loose = [v for v in bm.verts if not v.link_faces]
        bmesh.ops.delete(bm, geom=loose, context="VERTS")
        outs.append(M.Part(bm))
    part.bm.free()
    return outs[0], outs[1]

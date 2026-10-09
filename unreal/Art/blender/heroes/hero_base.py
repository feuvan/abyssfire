"""Shared machinery of the mage and rogue generators (``heroes/mage.py``, ``heroes/rogue.py``).

The warrior (``warrior.py`` / ``warrior_clips.py``) is the reference implementation; this module holds what the two
cloth-and-leather heroes share on top of ``common.py``, so neither depends on the warrior's own files:

* mesh helpers (``profile_mesh`` loft with radial displacement, painted strips / band sheets that follow a loft,
  ``ell`` ellipsoids with an equator ring, frames) — the warrior's helpers, same behaviour;
* ``hero_clip`` — ``common.body_clip`` with per-hero **grips** (the rogue holds a dagger in each fist);
* :class:`HeroAnims` — web → body-space conversion, chest-relative authoring, contact ``look``, foot planting,
  whole-body ground contact on the skinned meshes, and the **cloth chain toolkit**: hanging chains with secondary
  lag, gravity in spins, an obstacle **envelope** (a chain turns away just enough to clear posed points — legs
  under a robe skirt, a leaning back under a scarf) and a floor constraint;
* :class:`PairQA` — per-frame geometry QA (ground, item × body, cloth × body) on the deformed meshes.

Body space = armature space: X = character left, −Y = forward, Z = up, metres, degrees.
"""
from __future__ import annotations

import math
from typing import Callable, Sequence

import bpy
import bmesh
import numpy as np
from mathutils import Matrix, Quaternion, Vector

from kit import anim, mesh as M, rig as R, scene
from kit.anim import Clip, Pose

import common as C
from common import BP, K, V, Profile

RUN_SPEED = 120.0 / 36.0          # m/s: hero moveSpeed 120 / 36 px per tile (DECISIONS S5, R9) = 3.333
WALK_SPEED = 1.2


# ── mesh helpers (warrior.py, same behaviour) ───────────────────────────────────────────────────────────
def profile_mesh(prof: Profile, zs, sides: int, disp=None, exp: float | None = None, cap0: str | None = "fan",
                 cap1: str | None = "fan", dome0: float = 0.0, dome1: float = 0.0, a_offset: float = 0.0,
                 extra_angles=()) -> M.Part:
    """Loft through ``prof`` at heights ``zs`` (bottom → top) with an optional radial displacement
    ``disp(z, a) → metres``; vertices displaced inward carry the bmesh int layer ``af_disp`` = 1."""
    bm = bmesh.new()
    rings = []
    angs = sorted(set([round((a_offset + 360.0 * i / sides) % 360.0, 4) for i in range(sides)]
                      + [round(a % 360.0, 4) for a in extra_angles]))
    sides = len(angs)
    dl = bm.verts.layers.int.new("af_disp")
    for z in zs:
        ring = []
        for a in angs:
            d = disp(z, ((a + 180.0) % 360.0) - 180.0) if disp else 0.0
            v = bm.verts.new(tuple(prof.point(z, a, d, exp)))
            v[dl] = 1 if d < -1e-6 else 0
            ring.append(v)
        rings.append(ring)
    for r0, r1 in zip(rings, rings[1:]):
        for i in range(sides):
            j = (i + 1) % sides
            bm.faces.new((r0[i], r0[j], r1[j], r1[i]))
    for cap, ring, dome, sgn, z in ((cap0, rings[0], dome0, -1, zs[0]), (cap1, rings[-1], dome1, 1, zs[-1])):
        if cap is None:
            continue
        rx, ry, cy, cx = prof.at(z)
        c = bm.verts.new((cx, cy, z + sgn * dome))
        for i in range(sides):
            j = (i + 1) % sides
            bm.faces.new((ring[i], ring[j], c) if sgn < 0 else (ring[j], ring[i], c))
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return M.Part(bm).smooth(True)


def sheet_grid(grid) -> M.Part:
    """Single-sided quad sheet from a rows × cols grid of points."""
    bm = bmesh.new()
    vs = [[bm.verts.new(tuple(p)) for p in row] for row in grid]
    for r in range(len(vs) - 1):
        for c in range(len(vs[0]) - 1):
            bm.faces.new((vs[r][c], vs[r][c + 1], vs[r + 1][c + 1], vs[r + 1][c]))
    bm.normal_update()
    return M.Part(bm).smooth(True)


def outward(part: M.Part, center_fn) -> M.Part:
    """Flip a sheet whose faces point toward ``center_fn(face centre)`` (the axis of the loft it lies on)."""
    f = next(iter(part.bm.faces))
    c = f.calc_center_median()
    if f.normal.dot(c - center_fn(c)) < 0:
        bmesh.ops.reverse_faces(part.bm, faces=list(part.bm.faces))
        part.bm.normal_update()
    return part


def _axis_of(prof: Profile):
    def axis(c):
        rx, ry, cy, cx = prof.at(c.z)
        return Vector((cx, cy, c.z))
    return axis


def painted_strip(prof: Profile, a: float, z0: float, z1: float, width: float, lift, steps: int = 10,
                  a_of_z: Callable[[float], float] | None = None) -> M.Part:
    """A flat painted line on a loft (web strokes): a single-sided sheet ``width`` metres across at angle ``a``
    (or ``a_of_z(z)``) from ``z0`` to ``z1``, ``lift(z, a)`` metres off the profile. It bands with the surface."""
    grid = []
    for i in range(steps + 1):
        z = z0 + (z1 - z0) * i / steps
        ac = a_of_z(z) if a_of_z else a
        per_deg = (prof.point(z, ac + 0.5) - prof.point(z, ac - 0.5)).length
        da = width / 2 / max(per_deg, 1e-6)
        grid.append([prof.point(z, ac + da * t, lift(z, ac + da * t)) for t in (-1.0, 0.0, 1.0)])
    return outward(sheet_grid(grid), _axis_of(prof))


def band_sheet(prof: Profile, z: float, height: float, out: float, a0: float, a1: float, segs: int = 20,
               z_of_a: Callable[[float], float] | None = None) -> M.Part:
    """Single-sided band on a loft between z ± height/2 and angles a0..a1, ``out`` metres off the surface."""
    grid = [[prof.point((z_of_a(a0 + (a1 - a0) * i / segs) if z_of_a else z) + dz, a0 + (a1 - a0) * i / segs, out)
             for i in range(segs + 1)] for dz in (-height / 2, 0.0, height / 2)]
    return outward(sheet_grid(grid), _axis_of(prof))


def ell(r, center, sides=16, rings=8, exp=2.0, exp_v=2.0) -> M.Part:
    """kit ellipsoid with an **even** ring count (an equator ring: a hull proxy reaches the full radius)."""
    return M.ellipsoid(r, center, sides=sides, rings=rings, exp=exp, exp_v=exp_v)


def axis_frame(origin: Vector, axis: Vector) -> Matrix:
    """Frame whose +Z is ``axis`` (for loft_z parts placed along a limb)."""
    z = Vector(axis).normalized()
    x = z.orthogonal().normalized()
    y = z.cross(x)
    m = Matrix.Identity(4)
    for i in range(3):
        m[i][0], m[i][1], m[i][2], m[i][3] = x[i], y[i], z[i], origin[i]
    return m


def axis_frame_x(origin: Vector, axis: Vector, x_hint: Vector) -> Matrix:
    """Frame whose +Z is ``axis`` and +X as close as possible to ``x_hint``."""
    z = Vector(axis).normalized()
    x = Vector(x_hint) - z * Vector(x_hint).dot(z)
    if x.length < 1e-6:
        x = z.orthogonal()
    x.normalize()
    y = z.cross(x)
    m = Matrix.Identity(4)
    for i in range(3):
        m[i][0], m[i][1], m[i][2], m[i][3] = x[i], y[i], z[i], origin[i]
    return m


def rot_to(a: Vector, b: Vector) -> Matrix:
    return Vector(a).rotation_difference(Vector(b)).to_matrix().to_4x4()


def basis(x_axis: Vector, y_axis: Vector) -> Matrix:
    x = Vector(x_axis).normalized()
    y = (Vector(y_axis) - x * Vector(y_axis).dot(x)).normalized()
    z = x.cross(y)
    m = Matrix.Identity(4)
    for i in range(3):
        m[i][0], m[i][1], m[i][2] = x[i], y[i], z[i]
    return m


def shell_ring(outer_fn, rows: Sequence[float], cols: int, thick: float, lens: float = 0.0,
               sub_tags: bool = True) -> tuple[M.Part, M.Part]:
    """A closed (wrap-around) two-sided cloth shell — a robe skirt, a capelet — from ``outer_fn(k, u, d)`` → point
    (``k`` 0..1 down the cloth, ``u`` 0..1 round it, ``d`` offset along the outward normal). Returns (shell with
    ``af_sub`` 1 outside / 2 inside, hull proxy whose bottom edge ends in a pinched lens ``lens`` beyond the hem)."""
    us = [c / cols for c in range(cols)]
    outer = [[outer_fn(k, u, thick / 2) for u in us] for k in rows]
    inner = [[outer_fn(k, u, -thick / 2) for u in us] for k in rows]
    part = C.thick_patch(outer, inner, wrap_u=True, sub_tags=sub_tags)
    hk = [(k, 1.0) for k in rows[::2]] + ([(rows[-1], 1.0)] if (len(rows) - 1) % 2 else [])
    if lens > 0:
        hk += [(rows[-1] + 0.6 * lens, 0.8), (rows[-1] + lens, 0.0)]
    hull = C.thick_patch([[outer_fn(k, u, thick / 2 * fk + 1e-4) for u in us] for k, fk in hk],
                         [[outer_fn(k, u, -thick / 2 * fk - 1e-4) for u in us] for k, fk in hk], wrap_u=True)
    return part, hull


def curve_tube(points: Sequence[Vector], radii, sides: int = 8, up=None, dome: float | None = None) -> M.Part:
    """Round tube through ``points`` (cords, prongs, sash knots)."""
    return M.tube([tuple(p) for p in points], radii, sides=sides, up=up, dome0=dome, dome1=dome)


def catmull(pts: Sequence[Vector], n: int) -> list[Vector]:
    """Resample an open Catmull-Rom spline through ``pts`` into ``n`` + 1 points."""
    P = [Vector(p) for p in pts]
    out = []
    for i in range(n + 1):
        t = i / n * (len(P) - 1)
        j = min(int(t), len(P) - 2)
        f = t - j
        p0, p1, p2, p3 = P[max(j - 1, 0)], P[j], P[j + 1], P[min(j + 2, len(P) - 1)]
        f2, f3 = f * f, f * f * f
        out.append(0.5 * ((2 * p1) + (-p0 + p2) * f + (2 * p0 - 5 * p1 + 4 * p2 - p3) * f2
                          + (-p0 + 3 * p1 - 3 * p2 + p3) * f3))
    return out


def smooth01(x):
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3 - 2 * x)


def chain_weights(co_param: np.ndarray, n: int, yoke: np.ndarray | None = None) -> np.ndarray:
    """Weights along one chain of ``n`` bones from a 0..1 parameter down the cloth (bone-centre blending,
    the warrior's ``cape_weights``): N × (1 + n) — column 0 is the parent (yoke)."""
    k = np.clip(co_param, 0.0, 1.0)
    W = np.zeros((len(k), 1 + n))
    y = np.zeros(len(k)) if yoke is None else yoke
    W[:, 0] = y
    q = np.clip(k * n - 0.5, 0.0, n - 1.0)
    i0 = np.minimum(np.floor(q).astype(int), max(n - 2, 0))
    f = q - i0
    for i in range(n):
        wi = np.where(i0 == i, 1.0 - f, 0.0) + np.where(i0 + 1 == i, f, 0.0) if n > 1 else np.ones(len(k))
        W[:, 1 + i] = (1.0 - y) * wi
    return W


# ── clips with grips ────────────────────────────────────────────────────────────────────────────────────
def hero_clip(rig: R.Rig, name: str, length_ms: float, sampler: Callable[[float], BP], cloth=None,
              loop: bool = False, notifies: dict | None = None, ref_speed: float | None = None,
              additive: bool = False, post: Callable[[BP, float], BP] | None = None,
              grips: Sequence[str] = ("r",)) -> Clip:
    """``common.body_clip`` with the hero's grips (hands whose item drives a natural hammer grip)."""
    def fn(t_ms: float) -> Pose:
        bp = sampler(t_ms)
        if post is not None:
            bp = post(bp, t_ms)
        C.ClothClock.sampler, C.ClothClock.t, C.ClothClock.length, C.ClothClock.loop = sampler, t_ms, length_ms, loop
        try:
            return C.to_pose(rig, bp, cloth, grips=grips)
        finally:
            C.ClothClock.sampler = None
    return Clip(name, length_ms, loop=loop, sampler=fn, notifies=notifies or {}, ref_speed=ref_speed,
                additive=additive)


# ── chain plane geometry ────────────────────────────────────────────────────────────────────────────────
def envelope(angles: Sequence[float], lengths: Sequence[float], dn: np.ndarray, bk: np.ndarray,
             clear: np.ndarray | float, max_deg: float = 100.0, slack: float = 0.004) -> list[float]:
    """Greedy envelope from the chain root down: each link turns toward +B (never back) just enough that every
    obstacle point (depth ``dn`` below the root along D, ``bk`` along B) in its depth range lies ``clear`` metres
    on the −B side of the chain line. Angles in degrees from D toward B."""
    if len(dn) == 0:
        return list(angles)
    clear = np.broadcast_to(np.asarray(clear, float), dn.shape)
    s_dn = s_bk = 0.0
    out = []
    for i, L in enumerate(lengths):
        a = angles[i]
        last = i == len(lengths) - 1
        sel = (dn > s_dn + slack) & (dn <= s_dn + L + (0.06 if last else 0.02))
        if sel.any():
            need = float(np.degrees(np.arctan2(bk[sel] + clear[sel] - s_bk, dn[sel] - s_dn)).max())
            a = max(a, min(need, max_deg))
        out.append(a)
        s_dn += L * math.cos(math.radians(a))
        s_bk += L * math.sin(math.radians(a))
    return out


def chain_ends(angles: Sequence[float], lengths: Sequence[float]) -> list[tuple[float, float]]:
    s_dn = s_bk = 0.0
    out = []
    for a, L in zip(angles, lengths):
        s_dn += L * math.cos(math.radians(a))
        s_bk += L * math.sin(math.radians(a))
        out.append((s_dn, s_bk))
    return out


class HeroAnims:
    """Base of ``MageAnims`` / ``RogueAnims``: helpers + cloth toolkit. Subclasses implement ``ready()``,
    ``cloth(p, bp)``, the clips and ``all_clips()``; class attributes ``GRIPS``, ``CHAINS`` (prefixes),
    ``FEET`` (part names per side), ``ITEMS`` [(object name, bone)] for ground contact, ``CLOTH_PARTS``
    (part-name prefixes left out of the ground sample)."""

    GRIPS: tuple = ("r",)
    CHAINS: tuple = ()
    FEET: dict = {}
    ITEMS: tuple = ()
    CLOTH_PARTS: tuple = ()
    ON_CLOTH: tuple = ()               # parts that rest on folded cloth when the body lies on the floor
    ON_CLOTH_CLEAR = 0.02

    def __init__(self, rig: R.Rig, D):
        self.rig = rig
        self.D = D
        self.U = D.U
        self.J = rig.joints
        self.AZ = self.J["ankle_l"].z
        self.chains = {n: C.chain_rest_angles(rig, n) for n in self.CHAINS}
        self.leg = rig.spec.thigh + rig.spec.shin
        self.bones = rig.obj.data.bones

    # ── web → body space ────────────────────────────────────────────────────────────────────────────────
    def w(self, fwd_u: float, up_u: float, lat: float = 0.0) -> Vector:
        return V(lat, fwd_u * self.U, up_u * self.U)

    def web_pelvis(self, x: float, y: float) -> Vector:
        """Web root (x, y sprite units) → pelvis offset from the rest pelvis."""
        return V(0.0, (x - 48.0) * self.U, (91.0 - y) * self.U - self.J["pelvis"].z)

    def web_foot(self, x: float, lat: float, y: float = 91.0) -> Vector:
        return V(lat, (x - 48.0) * self.U, self.AZ + (91.0 - y) * self.U)

    def web_hand(self, x: float, y: float, lat: float) -> Vector:
        return V(lat, (x - 48.0) * self.U, (91.0 - y) * self.U)

    # ── solving ─────────────────────────────────────────────────────────────────────────────────────────
    def solve(self, bp: BP) -> Pose:
        return C.to_pose(self.rig, bp, self.cloth, grips=self.GRIPS)

    def cloth(self, p: Pose, bp: BP) -> None:            # subclasses
        pass

    def pose(self, base: BP | None = None, **kw) -> BP:
        return (base or self.ready()).but(**kw)

    def blade_kw(self, theta: float, lat: float = 0.0, roll: float = 0.0, key: str = "wpn") -> dict:
        d, e = C.blade(theta, lat=lat, roll=roll)
        return {key: d, f"{key}_up": e}

    @staticmethod
    def waver(period_ms: float, phase: float = 0.0):
        def layer(bp: BP, t: float) -> BP:
            return bp.but(wave=2 * math.pi * t / period_ms + phase)
        return layer

    # ── chest-relative authoring (warrior_clips) ────────────────────────────────────────────────────────
    def chest_m(self, bp: BP):
        p = C.to_pose(self.rig, bp.but(hand_l=None, hand_r=None), None, grips=())
        m = anim.evaluate(p, return_matrices=True)["spine_03"]
        return m @ self.bones["spine_03"].matrix_local.inverted()

    def chest(self, bp: BP, lat: float, fwd: float, up: float) -> Vector:
        return self.chest_m(bp) @ V(lat, fwd, up)

    def chest_dir(self, bp: BP, lat: float, fwd: float, up: float) -> Vector:
        return (self.chest_m(bp).to_3x3() @ V(lat, fwd, up)).normalized()

    @staticmethod
    def look(bp: BP, k: float = 0.8, max_down: float = 8.0, yaw: float = 0.0) -> BP:
        """The face stays on the strike line: the head turns back ``k`` of the chest + hip turn and tips down at
        most ``max_down`` degrees in the world however far the body leans."""
        hp = bp.head_pitch - max(0.0, bp.hip_pitch + bp.lean + bp.head_pitch - max_down)
        return bp.but(head_yaw=-k * (bp.twist + bp.hip_yaw) + yaw, head_pitch=hp)

    def with_pivot(self, bp: BP, k: float = 0.5) -> BP:
        P = self.J["pelvis"] + bp.pelvis
        a = math.radians(bp.lean + bp.hip_pitch)
        return bp.but(pivot=P + V(0, math.sin(a), math.cos(a)) * (self.rig.spec.torso * k))

    def clip(self, name, length, keys, span, loop=False, notifies=None, ref_speed=None, additive=False,
             layer=None, post=None, key_poses=None):
        lay = layer or self.waver(length)
        c = hero_clip(self.rig, name, length, C.keyed(keys, span, lay), cloth=self.cloth, loop=loop,
                      notifies=notifies, ref_speed=ref_speed, additive=additive, post=post or self.plant,
                      grips=self.GRIPS)
        c.key_poses_ms = key_poses or sorted({round(k.t * span, 1) for k in keys})
        return c

    def proc_clip(self, name, length, sampler, loop=False, notifies=None, ref_speed=None, post=None,
                  key_poses=None):
        c = hero_clip(self.rig, name, length, sampler, cloth=self.cloth, loop=loop, notifies=notifies,
                      ref_speed=ref_speed, post=post or self.plant, grips=self.GRIPS)
        if key_poses:
            c.key_poses_ms = key_poses
        return c

    # ── skinned samples (feet, ground) ──────────────────────────────────────────────────────────────────
    def body_obj(self):
        return next((o for o in self.rig.obj.children if o.type == "MESH" and o.get("af_lod") == 0
                     and "af_parts" in o), None)

    def skin(self):
        """Lazily sampled SkinPoints of the LOD0 body: ``self.feet`` (plant), ``self.ground_pts`` (rolls, falls)
        and ``self.env`` (cloth obstacles, subclasses pick parts with ``env_pick``). None without meshes."""
        if getattr(self, "_skin", False) is not False:
            return self._skin
        import json
        self._skin = None
        body = self.body_obj()
        if body is None:
            return None
        names = json.loads(body["af_parts"])
        self.part_names = names
        feet = C.SkinPoints(self.rig).add_mesh(body, names, lambda n: n in self.FEET["l"] + self.FEET["r"])
        self.feet = {s: (feet, feet.mask(self.FEET[s])) for s in ("l", "r")}
        grd = C.SkinPoints(self.rig).add_mesh(body, names, lambda n: not n.startswith(self.CLOTH_PARTS), step=2)
        for nm, bone in self.ITEMS:
            ob = bpy.data.objects.get(nm)
            if ob is not None:
                grd.add_rigid(ob, bone, step=2, tag=nm)
        self.ground_pts = grd
        self._ground_clear = np.where(grd.mask(self.ON_CLOTH), self.ON_CLOTH_CLEAR, 0.0)
        self._skin = self.make_env(body, names)
        return self._skin

    def make_env(self, body, names):
        """Subclasses: obstacle SkinPoints for the cloth envelope (returned object is cached as ``_skin``)."""
        return C.SkinPoints(self.rig)

    def plant(self, bp: BP, t: float = 0.0) -> BP:
        """Post step of upright clips: no sole, heel or toe below the floor."""
        return C.plant_feet(self.rig, bp, self.feet) if self.skin() is not None else bp

    def grounded(self, snap: bool = False, pivot_pelvis: bool = True):
        """Post step of rolls / falls: the whole body + items kept on the floor, measured on the skinned meshes;
        ``snap``: the lowest point rests exactly on the floor."""
        def post(bp: BP, t: float) -> BP:
            if snap and pivot_pelvis:
                bp = bp.but(pivot=self.J["pelvis"] + bp.pelvis)
            if self.skin() is None:
                return bp
            p = C.to_pose(self.rig, bp, None, grips=self.GRIPS)
            z = self.ground_pts.posed(anim.evaluate(p, return_matrices=True))[:, 2] - self._ground_clear
            low = float(z.min())
            if low < 0.0 or snap:
                return bp.but(lift=bp.lift - low)
            return bp
        return post

    # ── locomotion feet (warrior gait_feet, no-slide) ───────────────────────────────────────────────────
    def gait_feet(self, t: float, speed: float, cycle: float, duty: float, lift: float, bob: float,
                  peel: float = 30.0):
        fl, fr, dz, swing = anim.run_gait(t, speed, cycle, duty=duty, lift=lift, bob=bob)
        out = []
        for off, ph0 in ((fl, 0.0), (fr, 0.5)):
            p = (t + ph0) % 1.0
            pitch, toe, raise_, back = 0.0, 0.0, 0.0, 0.0
            if p < duty:
                s = p / duty
                if s > 0.6:
                    k = (s - 0.6) / 0.4
                    pitch = peel * k * k
                    toe = pitch
            else:
                s = (p - duty) / (1 - duty)
                if s < 0.45:
                    pitch = peel * (1 - s / 0.45) ** 1.5
                elif s > 0.8:
                    pitch = -12.0 * (s - 0.8) / 0.2
            if pitch > 0:
                r = 0.11
                raise_ = r * math.sin(math.radians(pitch))
                back = r * (1 - math.cos(math.radians(pitch)))
            out.append((off + V(0, -back, raise_ if p < duty else raise_ * 0.6), pitch, toe))
        return out, dz, swing

    # ── cloth toolkit ───────────────────────────────────────────────────────────────────────────────────
    @staticmethod
    def arc(d: float) -> float:
        return (d + 180.0) % 360.0 - 180.0

    def gravity(self, bp: BP) -> tuple[float, float]:
        """World −Z in the pre-spin armature frame as a sagittal chain angle (deg back from straight down) and the
        strength of its pull (0 upright, 1 once the body is spun ≥ 40° in the sagittal plane)."""
        if abs(bp.spin) < 1e-6:
            return 0.0, 0.0
        q = Quaternion(Vector(bp.spin_axis).normalized(), math.radians(bp.spin))
        g = q.inverted() @ Vector((0.0, 0.0, -1.0))
        sag = math.hypot(g.y, g.z)
        if sag < 0.2:
            return 0.0, 0.0
        ga = math.degrees(math.atan2(g.y, -g.z))
        k = max(0.0, min(1.0, (abs(ga) - 10.0) / 30.0))
        return ga, k * k * (3 - 2 * k) * min(1.0, (sag - 0.2) / 0.5)

    def world_fn(self, p: Pose, bp: BP):
        """(world(point), mats, Rm) for the floor constraint; None while upright (no chain reaches the floor)."""
        if abs(bp.spin) < 1e-6 and bp.lift >= -1e-6 and bp.pelvis.z > -0.22:
            return None
        mats = anim.evaluate(p, return_matrices=True)
        if abs(bp.spin) > 1e-6:
            Rm = Quaternion(Vector(bp.spin_axis).normalized(), math.radians(bp.spin)).to_matrix()
            P = bp.pivot + Vector((0, 0, bp.lift))
        else:
            Rm, P = Quaternion().to_matrix(), Vector()

        def world(x: Vector) -> Vector:        # posed (pre-spin, lift included) → after the spin about P
            return P + Rm @ (x - P)
        return world, mats, Rm

    def above_ground(self, info, a, wf, parent: str, clear: float, lo=None, hi=None, side=None):
        """Sagittal chain angles (armature space, deg back from down) with every link whose end would dip below
        ``clear`` turned to the nearest angle that keeps it on the floor."""
        if wf is None:
            return list(a)
        world, mats, Rm = wf
        pm = mats[parent] @ self.bones[parent].matrix_local.inverted()
        s = world(pm @ self.bones[info.names[0]].head_local)
        out = list(a)
        lat = 0.0
        for i, n in enumerate(info.names):
            ln = self.bones[n].length
            if side:
                lat += side[i] if i < len(side) else 0.0

            def end(ang, lat=lat):
                r = math.radians(ang)
                lr = math.radians(lat)
                return s + Rm @ Vector((math.sin(lr) * math.cos(r), math.sin(r), -math.cos(r) * math.cos(lr))) * ln
            if end(out[i]).z < clear:
                lo_i = (lo[i] - 20.0) if lo else -1e9
                hi_i = (hi[i] + 20.0) if hi else 1e9
                best = None
                for step in range(1, 91):
                    for sg in (1, -1):
                        cand = out[i] + sg * step * 2.0
                        if lo_i <= cand <= hi_i and end(cand).z >= clear:
                            best = cand
                            break
                    if best is not None:
                        break
                if best is None:
                    best = max((out[i] + d for d in range(-90, 91, 2) if lo_i <= out[i] + d <= hi_i),
                               key=lambda c: end(c).z, default=out[i])
                out[i] = best
            s = end(out[i])
        return out

    def plane(self, mats: dict, parent: str, root_bone: str, lateral: bool = False, side: float = 1.0):
        """Chain plane of a cloth chain hanging from ``parent``: (root position, D world-down ⟂ hinge, B = the
        swing direction (+ back for sagittal chains, + outward for lateral ones), hinge axis X, parent's
        accumulated angle in this plane). Positions are armature space (pre-spin)."""
        M3 = mats[parent].to_3x3() @ self.bones[parent].matrix_local.to_3x3().inverted()
        if lateral:
            X = (M3 @ Vector((0, -1, 0))).normalized()          # hinge = the parent's forward axis
            out_dir = (M3 @ Vector((side, 0, 0))).normalized()
        else:
            X = (M3 @ Vector((1, 0, 0))).normalized()
        Dn = Vector((0, 0, -1))
        Dn = (Dn - X * Dn.dot(X)).normalized()
        B = X.cross(Dn) if not lateral else out_dir - X * out_dir.dot(X)
        B = (B - Dn * B.dot(Dn)).normalized()
        v = M3 @ Vector((0, 0, -1))
        acc = math.degrees(math.atan2(v.dot(B), v.dot(Dn)))
        root = (mats[parent] @ self.bones[parent].matrix_local.inverted()) @ self.bones[root_bone].head_local
        return root, Dn, B, X, acc

    def chain_lengths(self, info) -> list[float]:
        return [self.bones[n].length for n in info.names]

    def lagged(self, bp: BP, lag_ms: float) -> BP:
        return C.ClothClock.past(lag_ms) or bp

    def hang(self, bp: BP, info, gain: float, lag_ms: float, wave_amp: float, phase: float, acc_w=None,
             body_min: float | None = None, gravity_k=(0.2, 0.45, 0.7, 0.85, 0.95)) -> list[float]:
        """Web ``clothChain`` angles of a hanging chain (deg back from straight down, armature space): rest +
        flow streaming growing toward the tip, read from the body ``lag_ms`` ago at the tip (secondary motion),
        a travelling wave, never forward of ``body_min`` + the lagged lean (``acc_w`` per link: how much of the
        lean the link inherits), pulled toward world down when the body is spun."""
        n = len(info.names)
        out = []
        ga, gs = self.gravity(bp)
        for i in range(n):
            k = (i + 1) / n
            pb = self.lagged(bp, lag_ms * k)
            fl = pb.flow - 0.12
            want = info.rest[i] + math.degrees(fl * gain * (0.45 + 0.75 * k))
            want += math.degrees(math.sin(bp.wave - 2.2 * k + phase) * wave_amp * k * (0.4 + pb.flow))
            if acc_w is not None:
                lean = pb.hip_pitch + pb.lean
                want = max(want, lean * acc_w[min(i, len(acc_w) - 1)] + (body_min if body_min is not None
                                                                         else 0.8 * info.rest[i]))
            if gs > 0:
                d = self.arc(ga - want)
                want += d * gs * gravity_k[min(i, len(gravity_k) - 1)] * (1.0 - (abs(d) / 180.0) ** 8)
            out.append(want)
        return out


# ── per-frame QA on the deformed meshes ─────────────────────────────────────────────────────────────────
class PairQA:
    """Per frame of every clip: lowest point (cm) of the body and of each item, and intersecting triangle pairs
    between named face groups (``pairs``: {metric: (groupA, groupB)}). Groups are sets of body part names
    (``_l``/``_r`` variants match) or ``item:<object name>`` (toon faces of an attached item, optionally minus a
    grip zone ``grips[name](co_item_local) → bool``)."""

    def __init__(self, rig, body, items: dict, part_names: Sequence[str], groups: dict, pairs: dict,
                 grips: dict | None = None):
        from qa import _face_parts, _match
        self.rig, self.body, self.items = rig, body, items
        me = body.data
        mi = np.zeros(len(me.polygons), np.int32)
        me.polygons.foreach_get("material_index", mi)
        self.toon = mi == 0
        self.names = _face_parts(body, part_names)
        self.sets = {}
        for g, parts in groups.items():
            if parts == "*":
                self.sets[g] = np.nonzero(self.toon)[0]
            else:
                self.sets[g] = np.nonzero(self.toon & _match(self.names, parts))[0]
        self.item_faces = {}
        for nm, ob in items.items():
            m = ob.data
            mi_ = np.zeros(len(m.polygons), np.int32)
            m.polygons.foreach_get("material_index", mi_)
            c = np.zeros(len(m.polygons) * 3, np.float32)
            m.polygons.foreach_get("center", c)
            c = c.reshape(-1, 3)
            t = mi_ == 0
            g = (grips or {}).get(nm)
            if g is not None:
                t = t & ~np.array([g(Vector(x)) for x in c], bool)
            self.item_faces[nm] = np.nonzero(t)[0]
        self.pairs = pairs

    def frame(self) -> dict:
        from qa import Checker
        gb = Checker._geom(self.body)
        out = {}
        t_all, zb = Checker._tree(gb, np.nonzero(self.toon)[0])
        out["groundBody"] = zb * 100.0
        trees = {}
        for g, faces in self.sets.items():
            trees[g] = Checker._tree(gb, faces)[0]
        for nm, ob in self.items.items():
            if ob.hide_render or ob.parent is None:
                continue
            gi = Checker._geom(ob)
            t, z = Checker._tree(gi, self.item_faces[nm])
            trees["item:" + nm] = t
            allf = np.nonzero(np.ones(len(ob.data.polygons), bool))[0]
            out["ground:" + nm] = Checker._tree(gi, allf)[1] * 100.0
        for metric, (a, b) in self.pairs.items():
            out[metric] = Checker._pairs(trees.get(a), trees.get(b))
        return out

    def run(self, clips, actions, names: Sequence[str] | None = None, before: Callable | None = None) -> dict:
        rep = {}
        saved = self.rig.obj.rotation_euler.copy()
        self.rig.obj.rotation_euler = (0, 0, 0)
        for c, a in zip(clips, actions):
            if names and c.name not in names:
                continue
            if before is not None:
                before(c.name)
            anim.assign(self.rig, a)
            frames = []
            for fr in range(0, c.frames + 1):
                anim.set_frame(fr)
                f = self.frame()
                f["frame"] = fr
                frames.append(f)
            keys = sorted({k for f in frames for k in f if k != "frame"})
            worst = {}
            for k in keys:
                vals = [f[k] for f in frames if k in f]
                worst[k] = min(vals) if k.startswith("ground") else max(vals)
            rep[c.name] = {"frames": frames, "worst": worst}
        self.rig.obj.rotation_euler = saved
        anim.set_frame(0)
        return rep

    @staticmethod
    def verdict(rep: dict, limits: dict, per_clip: dict | None = None) -> list[str]:
        fails = []
        for clip, r in rep.items():
            lim = dict(limits)
            lim.update((per_clip or {}).get(clip, {}))
            for k, v in r["worst"].items():
                key = "ground" if k.startswith("ground") else k
                if key not in lim:
                    continue
                bad = v < lim[key] if key == "ground" else v > lim[key]
                if bad:
                    frs = [f["frame"] for f in r["frames"] if k in f
                           and (f[k] < lim[key] if key == "ground" else f[k] > lim[key])]
                    fails.append(f"{clip}: {k} {v:.1f} beyond {lim[key]} (frames {frs[:12]})")
        return fails

    @staticmethod
    def table(rep: dict) -> str:
        keys = sorted({k for r in rep.values() for k in r["worst"]})
        short = {k: k.replace("ground:", "g:").replace("SM_Hero_", "")[:12] for k in keys}
        rows = [f"{'clip':16s} " + " ".join(f"{short[k]:>12s}" for k in keys)]
        for clip, r in rep.items():
            w = r["worst"]
            rows.append(f"{clip:16s} " + " ".join(
                (f"{w[k]:12.1f}" if k.startswith("ground") else f"{int(w[k]):12d}") if k in w else f"{'-':>12s}"
                for k in keys))
        return "\n".join(rows)


def attach_at(obj: bpy.types.Object, rig: R.Rig, bone: str, local: Matrix | None = None) -> None:
    """Parent ``obj`` to ``bone`` (head = UE socket convention) with an extra socket-local transform."""
    R.attach_to_bone(obj, rig, bone)
    if local is not None:
        obj.matrix_basis = local


def socket_local(rig: R.Rig, name: str) -> tuple[str, Matrix]:
    """(bone, bone-head-relative matrix) of a body socket (``rig.sockets`` entry with armature-space ``pos``)."""
    s = next(x for x in rig.sockets if x["name"] == name)
    b = rig.obj.data.bones[s["bone"]]
    loc = b.matrix_local.inverted() @ Vector(s["pos"])
    rot = s.get("rot", (0, 0, 0))
    m = Matrix.Translation(loc) @ (scene_euler(rot))
    return s["bone"], m


def scene_euler(rot_deg) -> Matrix:
    from mathutils import Euler
    return Euler(tuple(math.radians(a) for a in rot_deg), "XYZ").to_matrix().to_4x4()

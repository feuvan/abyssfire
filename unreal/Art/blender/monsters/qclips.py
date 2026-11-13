"""Clip library of ``SKEL_Quadruped`` (Chapter 2, package 4R-1; chapter2.md §4.4, §17.2) — the pose solver and the
clips every member of the rig family shares: the moon-hound first, then the Pack-Leader (``howl``), Volgan (his own
timings: every clip takes its lengths and beats as arguments) and the wolf cub.

**Pose** (:class:`QP`): the trunk as a curve (``hip`` offset, ``pitch`` / ``yaw`` / ``roll`` about the tail root,
``arch`` / ``bend`` / ``twist`` spread over ``spine_01..04``), neck and head, jaw, ears, tail, and the four paws as
targets — offsets (lateral +left, forward, up) of each paw's ball from its rest stand, in ground space while the
animal stands (``follow`` 0: a planted paw stays put whatever the trunk does: crouches, lunges and roaches are
authored on the trunk alone) or carried by the trunk (``follow`` 1: a leap, a body lying on its side).

**Solver** (:meth:`QuadAnims.solve`): trunk FK, then per leg a two-bone IK of the upper pair toward the wrist / hock,
whose place is the paw's ball minus the pastern / metatarsus at its rest slope (+ ``*_fold``: the paw trailing back,
a lifted leg) — elbows bend back, stifles forward. An out-of-reach target is clamped (the paw leaves the target);
``reach_error`` reports it, and the gaits assert no planted paw slides (R9: ≤ 2 mm).

Clips are baked as bone basis matrices (``kit.anim.bake_basis``, as the slime's): ``Clip.basis_fn(t_ms)``. Conventions
of the family: in place (the core moves the actor; play rate = v / ref speed), ``FootFL / FootFR / FootBL / FootBR``
at each paw's touchdown, ``WindupEnd`` at 62 % of ``Contact`` (the wind-up tint), ``Contact`` = the core's beat.
Angles in degrees: pitch + = that end **down** (head, forehand), yaw + = to the animal's left, roll + = the back toward
its left; ``tail`` lift + = up.
"""
from __future__ import annotations

import math
import sys
from dataclasses import dataclass, fields, replace
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

from mathutils import Matrix, Quaternion, Vector  # noqa: E402

from kit import anim, scene  # noqa: E402
from kit.anim import Clip  # noqa: E402
from kit.skel_quadruped import FORELEG, HINDLEG, LEGS, NECK, SPINE, TAIL  # noqa: E402

WINDUP_OF_CONTACT = 0.62
X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
V0 = (0.0, 0.0, 0.0)


def RX(d): return Matrix.Rotation(math.radians(d), 4, "X")      # noqa: E704
def RY(d): return Matrix.Rotation(math.radians(d), 4, "Y")      # noqa: E704
def RZ(d): return Matrix.Rotation(math.radians(d), 4, "Z")      # noqa: E704


def about(p: Vector, r: Matrix) -> Matrix:
    return Matrix.Translation(p) @ r @ Matrix.Translation(-p)


@dataclass(frozen=True)
class QP:
    """One pose of the quadruped (module docstring). Every field is a number or a tuple of numbers: poses
    interpolate field by field (:func:`lerp_qp`)."""
    hip: tuple = V0             # (lat, fwd, up) m: the tail root's offset
    pitch: float = 0.0          # trunk about the tail root: + = forehand down
    yaw: float = 0.0
    roll: float = 0.0
    arch: float = 0.0           # spine curl over spine_01..04: + = roach (forehand curls down and under), − = hollow
    bend: float = 0.0           # lateral spine bend: + = the head end to the left
    twist: float = 0.0          # the chest rolled against the hips
    neck: tuple = (0.0, 0.0)    # (pitch, yaw) over neck_01..02
    head: tuple = V0            # (pitch, yaw, roll)
    jaw: float = 0.0            # open, degrees
    ears: tuple = (0.0, 0.0)    # (left, right): + = pricked forward, − = laid back
    tail: tuple = V0            # (lift at the root, yaw, curl per further bone: + = up)
    fl: tuple = V0              # paw targets (lat, fwd, up) from the rest stand
    fr: tuple = V0
    bl: tuple = V0
    br: tuple = V0
    fold: tuple = (0.0, 0.0, 0.0, 0.0)   # (fl, fr, bl, br) pastern / metatarsus folded back, degrees
    toe: tuple = (0.0, 0.0, 0.0, 0.0)    # paws pitched: + = toes down
    follow: float = 0.0         # 0 = paw targets in ground space … 1 = carried by the trunk
    reach: tuple = (0.0, 0.0)   # (fore, hind) m: the shoulder / hip joints slid forward (scapula glide)

    def but(self, **kw) -> "QP":
        return replace(self, **kw)


def lerp_qp(a: QP, b: QP, t: float) -> QP:
    kw = {}
    for f in fields(QP):
        va, vb = getattr(a, f.name), getattr(b, f.name)
        if isinstance(va, tuple):
            kw[f.name] = tuple(x + (y - x) * t for x, y in zip(va, vb))
        else:
            kw[f.name] = va + (vb - va) * t
    return QP(**kw)


def add_qp(a: QP, **delta) -> QP:
    """``a`` with ``delta`` added field-wise (layers: breathing, sway)."""
    kw = {}
    for k, d in delta.items():
        v = getattr(a, k)
        kw[k] = tuple(x + y for x, y in zip(v, d)) if isinstance(v, tuple) else v + d
    return replace(a, **kw)


def sample_keys(keys, t_ms: float) -> QP:
    """``keys`` = [(ms, QP, ease)] sorted by time; the ease belongs to the destination key (the kit's rule)."""
    if t_ms <= keys[0][0]:
        return keys[0][1]
    for (t0, p0, _), (t1, p1, e1) in zip(keys, keys[1:]):
        if t0 <= t_ms <= t1:
            return lerp_qp(p0, p1, anim.ease((t_ms - t0) / max(t1 - t0, 1e-6), e1))
    return keys[-1][1]


def step_cycle(ph: float, duty: float, stride: float, lift: float, fold: float, centre: float = 0.0,
               over: float = 0.0):
    """One paw over a gait cycle: ``ph`` in [0, 1), planted for the first ``duty`` of it (moving back at constant
    speed from ``centre + stride/2`` to ``centre − stride/2``: no slide), then swung forward with ``lift`` and the
    paw folded back. ``over`` (m): the swing's follow-through and reach — the paw trails that much further back
    after it leaves the ground and reaches that much further forward before it is drawn back onto its touchdown
    spot (a gallop's extended and gathered flight frames; the stance is untouched). Returns (fwd, up, fold, toe)."""
    ph %= 1.0
    if ph < duty:
        return centre + stride * (0.5 - ph / duty), 0.0, 0.0, 0.0
    u = (ph - duty) / (1.0 - duty)
    e = u * u * (3.0 - 2.0 * u)
    s = math.sin(math.pi * u)
    return centre + stride * (-0.5 + e) - over * math.sin(2.0 * math.pi * u), lift * s ** 0.8, fold * math.sin(math.pi * min(1.0, u * 1.25)) ** 0.7, 35.0 * s


class QuadAnims:
    """Solver + clip library on a built ``SKEL_Quadruped`` rig. ``ready``: the member's stand (a :class:`QP`)."""

    def __init__(self, rig, ready: QP | None = None, run_speed: float = 3.75, walk_speed: float = 1.3):
        self.rig = rig
        self.J = rig.joints
        arm = rig.obj.data
        self.rest = {b.name: b.matrix_local.copy() for b in arm.bones}
        self.order = [b.name for b in anim._bones_in_order(arm)]
        self.parent = {b.name: (b.parent.name if b.parent else None) for b in arm.bones}
        self.R0 = ready or QP()
        self.run_speed, self.walk_speed = run_speed, walk_speed
        self.k = rig.spec.withers / 0.75                   # the member's size against the hound (lengths scale)
        self.reach_error = 0.0
        self.feet = {}
        self.legs = {}
        for leg, kind, s in LEGS:
            names = [f"{b}_{s}" for b in (FORELEG if kind == "fore" else HINDLEG)]
            pts = [self.J[f"{n}_{s}"] for n in (("shoulder", "elbow", "wrist", "fore_ball", "fore_tip")
                                                if kind == "fore" else ("hip", "stifle", "hock", "hind_ball", "hind_tip"))]
            self.legs[leg] = (kind, names, pts)

    def ready(self) -> QP:
        return self.R0

    # ── solver ──────────────────────────────────────────────────────────────────────────────────────────
    def solve(self, q: QP) -> dict:
        """Armature-space posed matrices of every deform bone."""
        J, rest = self.J, self.rest
        D = {}
        p0 = J["pelvis"]
        off = Vector((q.hip[0], -q.hip[1], q.hip[2]))
        D["pelvis"] = Matrix.Translation(p0 + off) @ RZ(q.yaw) @ RX(q.pitch) @ RY(q.roll) @ Matrix.Translation(-p0)
        prev = "pelvis"
        for n in SPINE[1:]:
            D[n] = D[prev] @ about(J[n], RZ(q.bend / 4.0) @ RX(q.arch / 4.0) @ RY(q.twist / 4.0))
            prev = n
        for n in NECK:
            D[n] = D[prev] @ about(J[n], RZ(q.neck[1] / 2.0) @ RX(q.neck[0] / 2.0))
            prev = n
        D["head"] = D[prev] @ about(J["head"], RZ(q.head[1]) @ RX(q.head[0]) @ RY(q.head[2]))
        D["jaw"] = D["head"] @ about(J["jaw"], RX(q.jaw))
        for s, e in (("l", q.ears[0]), ("r", q.ears[1])):
            D[f"ear_{s}"] = D["head"] @ about(J[f"ear_{s}"], RX(e))
        prev = "pelvis"
        for i, n in enumerate(TAIL):
            lift = q.tail[0] if i == 0 else q.tail[2]
            D[n] = D[prev] @ about(J[n], RX(lift) @ RZ(-q.tail[1] * (1.0 if i == 0 else 0.6)))
            prev = n
        out = {n: D[n] @ rest[n] for n in D}
        heading = RZ(q.yaw).to_3x3()
        err = 0.0
        for i, (leg, (kind, names, pts)) in enumerate(self.legs.items()):
            fore = kind == "fore"
            par = D["spine_04" if fore else "pelvis"]
            H0, K0, W0, B0, T0 = pts
            l1, l2, l3, lt = (K0 - H0).length, (W0 - K0).length, (B0 - W0).length, (T0 - B0).length
            H = par @ (H0 + Vector((0.0, -q.reach[0 if fore else 1], 0.0)))
            o = getattr(q, leg)
            o = Vector((o[0], -o[1], o[2]))
            f = max(0.0, min(1.0, q.follow))
            B = (B0 + o).lerp(par @ (B0 + o), f)
            # a paw's ball never goes under its standing height (+ 1 cm once the body rolls: a tilted paw's rim)
            B.z = max(B.z, B0.z + 0.010 * self.k * min(1.0, abs(q.roll) / 30.0))
            R3 = heading.to_quaternion().slerp(par.to_3x3().to_quaternion(), f).to_matrix()
            hinge = (R3 @ X).normalized()
            d3 = Quaternion(hinge, math.radians(q.fold[i])) @ (R3 @ (B0 - W0).normalized())
            W = B - d3 * l3
            d = W - H
            dist = d.length
            lo, hi = abs(l1 - l2) + 1e-4, (l1 + l2) * 0.9985
            if dist > hi or dist < lo:
                dc = min(hi, max(lo, dist))
                W = H + d.normalized() * dc
                err = max(err, (W + d3 * l3 - B).length)
                B = W + d3 * l3
                d, dist = W - H, dc
            dn = d / dist
            pole = R3 @ (Y if fore else -Y)
            pp = pole - dn * pole.dot(dn)
            pp = pp.normalized() if pp.length > 1e-6 else (hinge.cross(dn)).normalized()
            a = (l1 * l1 - l2 * l2 + dist * dist) / (2.0 * dist)
            h = math.sqrt(max(l1 * l1 - a * a, 0.0))
            K = H + dn * a + pp * h
            tdir = Quaternion(hinge, math.radians(q.toe[i])) @ (R3 @ (T0 - B0).normalized())
            for nm, (p, y) in zip(names, ((H, K - H), (K, W - K), (W, B - W), (B, tdir * lt))):
                out[nm] = self._frame(p, y, hinge)
            self.feet[leg] = B.copy()
        self.reach_error = err
        return out

    @staticmethod
    def _frame(head: Vector, y_dir: Vector, hinge: Vector) -> Matrix:
        y = y_dir.normalized()
        x = hinge - y * hinge.dot(y)
        x.normalize()
        z = x.cross(y)
        m = Matrix.Identity(4)
        for i in range(3):
            m[i][0], m[i][1], m[i][2], m[i][3] = x[i], y[i], z[i], head[i]
        return m

    def basis(self, q: QP) -> dict:
        """Pose-bone basis matrices (``kit.anim.bake_basis``): parent-relative against the rest pose."""
        posed = self.solve(q)
        rest = self.rest
        out = {}
        for n in self.order:
            if n not in posed:
                continue
            par = self.parent[n]
            ref = (posed[par] @ rest[par].inverted() @ rest[n]) if par in posed else rest[n]
            out[n] = ref.inverted() @ posed[n]
        return out

    # ── ground ──────────────────────────────────────────────────────────────────────────────────────────
    probes: tuple = ()          # ((bone, rest point, lateral half extent, vertical half extent), …): the body's
    #                             volume as ellipses in each bone's rest frame (set by the member from its mesh)

    def lowest(self, q: QP) -> float:
        """Height of the lowest probe of the posed body (m; < 0 = under the ground)."""
        posed = self.solve(q)
        z = 1e9
        for bone, p, a, b in self.probes:
            m = posed[bone] @ self.rest[bone].inverted()
            c = m @ Vector(p)
            r = m.to_3x3()
            z = min(z, c.z - math.sqrt((a * (r @ X).z) ** 2 + (b * (r @ Z).z) ** 2 + (min(a, b) * (r @ Y).z) ** 2))
        return z

    def grounded(self, q: QP, settle: bool = False, margin: float = 0.004) -> QP:
        """``q`` lifted until no probe is under the ground (+ ``margin``); ``settle``: also lowered onto it (a body
        that lies: the corpse rests on its side, it does not float)."""
        z = self.lowest(q) - margin
        if z < 0.0 or settle:
            q = q.but(hip=(q.hip[0], q.hip[1], q.hip[2] - z))
        return q

    def apply(self, q: QP) -> None:
        """Pose the armature directly (portraits, dev renders): no action."""
        import bpy
        anim.assign(self.rig, None)
        b = self.basis(q)
        for pb in self.rig.obj.pose.bones:
            pb.matrix_basis = b.get(pb.name, Matrix.Identity(4))
        bpy.context.view_layer.update()

    # ── clip plumbing ───────────────────────────────────────────────────────────────────────────────────
    def clip(self, name, length_ms, fn, loop=False, notifies=None, ref_speed=None, key_ms=None, usage=None):
        c = Clip(name, length_ms, loop=loop, notifies=notifies or {}, ref_speed=ref_speed)
        c.pose_fn = fn
        c.basis_fn = lambda t: self.basis(fn(t))
        c.sampler = lambda t: None
        c.key_poses_ms = key_ms or [length_ms * i / 5 for i in range(6)]
        c.manifest_extra = {"usage": usage} if usage else {}
        return c

    def keyed(self, name, length_ms, keys, notifies=None, layer=None, usage=None):
        keys = sorted(keys, key=lambda k: k[0])

        def fn(t):
            q = sample_keys(keys, t)
            return layer(q, t) if layer else q
        return self.clip(name, length_ms, fn, notifies=notifies, key_ms=sorted({k[0] for k in keys}), usage=usage)

    def slide_report(self, c) -> float:
        """Largest slide (m) of a planted paw over a gait clip (``c.duty_of`` {leg: (phase0, duty)},
        ``c.gait_speed``): for every leg and every pair of consecutive frames inside its stance, the paw's realised
        motion (after the IK's reach clamp) against ground moving back at the gait speed. 0 = no slide."""
        L, n = c.length_ms, c.frames
        worst = 0.0
        prev = None
        for f in range(n + 1):
            t = scene.frame_to_ms(f)
            self.solve(c.pose_fn(t % L))
            cur = {k: v.copy() for k, v in self.feet.items()}
            if prev is not None:
                t0 = scene.frame_to_ms(f - 1)
                dt = (t - t0) / 1000.0
                for leg, (ph0, duty) in c.duty_of.items():
                    pa = ((t0 / L) - ph0) % 1.0
                    if pa + (t - t0) / L <= duty:
                        d = cur[leg] - prev[leg]
                        worst = max(worst, abs(d.y - c.gait_speed * dt), abs(d.z), abs(d.x))
            prev = cur
        return worst

    # ── the library ─────────────────────────────────────────────────────────────────────────────────────
    def idle(self, length: float = 1600.0):
        """Idle (loop): the stand breathing — the ribs' rise as a small roach and a hand's breadth of hip bob — the
        low head quartering slowly, one ear turning after a sound, the straight tail barely moving."""
        R0 = self.R0
        k = self.k

        def fn(t):
            ph = 2.0 * math.pi * t / length
            br = math.sin(ph)
            return add_qp(R0, hip=(0.0, 0.0, 0.006 * k * br), arch=1.6 * br, pitch=-0.5 * br,
                          neck=(1.5 * math.sin(ph + 0.6), 4.0 * math.sin(ph)),
                          head=(1.2 * math.sin(ph + 1.1), 3.0 * math.sin(ph + 0.4), 0.0),
                          ears=(4.0 * max(0.0, math.sin(ph + 2.0)), 7.0 * max(0.0, math.sin(ph - 0.7))),
                          tail=(1.5 * math.sin(ph + 0.8), 3.0 * math.sin(ph + 2.2), 1.0 * br))
        return self.clip("Idle", length, fn, loop=True, key_ms=[length * i / 4 for i in range(5)])

    def idle_sniff(self, length: float = 2400.0):
        """Idle_Sniff (one-shot idle variant, READY → READY): the nose goes down to the roots, works left and
        right along the ground, the tail lifts a little; the head comes up and it listens."""
        R0 = self.R0
        k = self.k
        down = R0.but(hip=(0.0, 0.01 * k, -0.012 * k), pitch=5.0, arch=3.0, neck=(46.0, 0.0), head=(16.0, 0.0, 0.0),
                      ears=(8.0, 8.0), tail=(8.0, 0.0, 2.0), reach=(0.02 * k, 0.0))
        keys = [(0.0, R0, "smooth"), (420.0, down, "smooth"),
                (820.0, down.but(neck=(48.0, 14.0), head=(14.0, 8.0, 4.0)), "smooth"),
                (1250.0, down.but(neck=(47.0, -12.0), head=(15.0, -9.0, -4.0)), "smooth"),
                (1600.0, down.but(neck=(44.0, -2.0)), "smooth"),
                (1950.0, R0.but(neck=(-12.0, 6.0), head=(-6.0, 6.0, 0.0), ears=(14.0, 14.0), tail=(4.0, 0.0, 1.0)),
                 "smooth"),
                (length, R0, "smooth")]
        s = length / 2400.0
        keys = [(t * s, p, e) for t, p, e in keys]

        def layer(q, t):
            w = max(0.0, min(1.0, (t - 380.0 * s) / 120.0, (1640.0 * s - t) / 120.0))
            return add_qp(q, head=(1.8 * w * math.sin(t * 0.055), 0.0, 0.0), jaw=0.0)
        return self.keyed("Idle_Sniff", length, keys, layer=layer,
                          usage="idle variant: nose to the ground along the roots; one-shot from READY back to READY")

    # gaits ---------------------------------------------------------------------------------------------
    TROT = {"fl": 0.0, "br": 0.0, "fr": 0.5, "bl": 0.5}
    GALLOP = {"bl": 0.0, "br": 0.10, "fl": 0.46, "fr": 0.58}       # transverse gallop: hinds, a gather, fores, flight

    def walk(self, length: float = 700.0, duty: float = 0.5, lift: float = 0.07, name: str = "Walk"):
        """Walk (loop): the trot — diagonal pairs, the head low and level, the back swinging a little with the
        stride. In place, no slide: planted paws move back at exactly ``walk_speed``."""
        R0 = self.R0
        k = self.k
        v = self.walk_speed
        stride = v * duty * length / 1000.0
        cen = {"fl": 0.035 * k, "fr": 0.035 * k, "bl": 0.04 * k, "br": 0.04 * k}   # stance centred in each leg's reach
        duty_of = {leg: (ph, duty) for leg, ph in self.TROT.items()}

        def fn(t):
            u = (t / length) % 1.0
            ph = 2.0 * math.pi * u
            st = {leg: step_cycle(u - p0, duty, stride, lift * k, 38.0 if leg[0] == "f" else 30.0, cen[leg])
                  for leg, p0 in self.TROT.items()}
            q = R0.but(hip=(R0.hip[0], R0.hip[1], R0.hip[2] - 0.074 * k + 0.008 * k * math.cos(2.0 * ph)),
                       pitch=R0.pitch + 0.5 * math.sin(2.0 * ph), arch=R0.arch + 0.8 * math.sin(2.0 * ph + 0.6),
                       bend=3.0 * math.sin(ph), twist=2.0 * math.sin(ph + 0.5),
                       neck=(R0.neck[0] + 4.0 + 1.5 * math.sin(2.0 * ph + 1.0), -2.0 * math.sin(ph)),
                       head=(R0.head[0] - 3.0, -1.0 * math.sin(ph), 0.0), ears=(-10.0, -10.0),
                       tail=(R0.tail[0] - 2.0 + 2.0 * math.sin(2.0 * ph), 6.0 * math.sin(ph + 0.8), 1.5),
                       reach=(0.015 * k, 0.0),
                       **{leg: (getattr(R0, leg)[0], s[0], s[1]) for leg, s in st.items()},
                       fold=tuple(st[leg][2] for leg in ("fl", "fr", "bl", "br")),
                       toe=tuple(st[leg][3] for leg in ("fl", "fr", "bl", "br")))
            return q
        notif = {f"Foot{leg.upper()}": round((p0 % 1.0) * length, 1) for leg, p0 in self.TROT.items()}
        c = self.clip(name, length, fn, loop=True, notifies=notif, ref_speed=v,
                      key_ms=[length * i / 4 for i in range(5)],
                      usage=f"the trot ({v:g} m/s at rate 1: play rate = v / ref); the slower of the two gaits (UE "
                            "picks Walk or Run by the nearer ref speed)")
        c.duty_of, c.gait_speed = duty_of, v
        return c

    def run(self, length: float = 460.0, duty: float = 0.22, lift: float = 0.12, name: str = "Run",
            over: tuple = (0.11, 0.13)):
        """Run (loop): the gallop at ``run_speed`` — the hinds strike together under the body (the back roached),
        drive and trail out behind, the fores reach out ahead (the extended flight: the back stretched hollow) and
        take the ground, sweep under, and the hinds come forward past them (the gathered flight). ``over`` (fore,
        hind; m at the hound's size): the swing's follow-through / reach beyond the stance (``step_cycle``). In
        place, no slide: the planted span is ``run_speed`` × the stance time and nothing else."""
        R0 = self.R0
        k = self.k
        v = self.run_speed
        stride = v * duty * length / 1000.0
        cen = {"fl": 0.04 * k, "fr": 0.04 * k, "bl": 0.10 * k, "br": 0.10 * k}
        duty_of = {leg: (ph, duty) for leg, ph in self.GALLOP.items()}

        def fn(t):
            u = (t / length) % 1.0
            ph = 2.0 * math.pi * u
            st = {leg: step_cycle(u - p0, duty, stride, lift * k, 60.0 if leg[0] == "f" else 48.0, cen[leg],
                                  over[0 if leg[0] == "f" else 1] * k)
                  for leg, p0 in self.GALLOP.items()}
            gather = math.cos(ph - 2.0 * math.pi * 0.02)             # +1: hinds landing (roach), −1: fores (hollow)
            q = R0.but(hip=(0.0, 0.0, R0.hip[2] - 0.088 * k + 0.022 * k * math.sin(ph - 2.0 * math.pi * 0.30)),
                       pitch=R0.pitch - 2.0 * math.sin(ph - 2.0 * math.pi * 0.12),
                       arch=R0.arch + 9.0 * gather + 3.0,
                       neck=(R0.neck[0] - 6.0 - 5.0 * gather, 0.0), head=(R0.head[0] + 6.0 + 3.0 * gather, 0.0, 0.0),
                       jaw=6.0 + 4.0 * math.sin(ph), ears=(-26.0, -26.0),
                       tail=(R0.tail[0] + 4.0 - 6.0 * gather, 0.0, -2.0 * gather),
                       reach=(0.035 * k * -gather, 0.02 * k * gather),
                       **{leg: (getattr(R0, leg)[0] * 0.6, s[0], s[1]) for leg, s in st.items()},
                       fold=tuple(st[leg][2] for leg in ("fl", "fr", "bl", "br")),
                       toe=tuple(st[leg][3] for leg in ("fl", "fr", "bl", "br")))
            return q
        notif = {f"Foot{leg.upper()}": round((p0 % 1.0) * length, 1) for leg, p0 in self.GALLOP.items()}
        c = self.clip(name, length, fn, loop=True, notifies=notif, ref_speed=v,
                      key_ms=[length * i / 6 for i in range(7)],
                      usage=f"the gallop ({v:g} m/s at rate 1 = the monster's data speed: play rate = v / ref)")
        c.duty_of, c.gait_speed = duty_of, v
        return c

    # attacks -------------------------------------------------------------------------------------------
    def bite_poses(self) -> dict:
        R0 = self.R0
        k = self.k
        coil = R0.but(hip=(0.0, -0.07 * k, -0.05 * k), pitch=-3.0, arch=10.0, neck=(-14.0, -6.0), head=(-12.0, -6.0, 6.0),
                      jaw=30.0, ears=(-32.0, -32.0), tail=(10.0, 0.0, 3.0), reach=(-0.02 * k, 0.0))
        gape = coil.but(hip=(0.0, -0.09 * k, -0.06 * k), jaw=42.0, neck=(-18.0, -8.0))
        hit = R0.but(hip=(0.0, 0.16 * k, -0.095 * k), pitch=5.0, arch=-10.0, neck=(-6.0, 4.0), head=(4.0, 4.0, -8.0),
                     jaw=3.0, ears=(-36.0, -36.0), tail=(-6.0, 0.0, -3.0), reach=(0.05 * k, -0.02 * k),
                     fl=(0.0, 0.16 * k, 0.0), fold=(0.0, 0.0, 0.0, 0.0))
        shake = hit.but(hip=(0.0, 0.13 * k, -0.085 * k), head=(6.0, -12.0, 14.0), neck=(-4.0, -8.0), jaw=6.0)
        # the recovery: the left fore that stepped into the bite is picked up and set back on its stand (a step:
        # a planted paw is never dragged over the ground), then the stand
        mid = tuple(0.5 * (a + b) for a, b in zip(hit.fl, R0.fl))
        back = R0.but(hip=(0.0, 0.04 * k, -0.03 * k), neck=(-4.0, 0.0), jaw=14.0, ears=(-20.0, -20.0),
                      fl=(mid[0], mid[1], 0.045 * k), fold=(32.0, 0.0, 0.0, 0.0), toe=(20.0, 0.0, 0.0, 0.0))
        down = R0.but(hip=(0.0, 0.015 * k, -0.012 * k), neck=(-2.0, 0.0), jaw=6.0, ears=(-12.0, -12.0))
        return dict(coil=coil, gape=gape, hit=hit, shake=shake, back=back, down=down)

    def attack(self, length: float = 800.0, contact: float = 320.0, name: str = "Attack01", usage: str | None = None):
        """Attack01, the bite: coiled back on the hocks with the jaw gaping (the wind-up tint reads on the open
        maw), the whole body thrown forward and the jaws snapped shut on ``Contact``; a short shake, then back to
        the stand. One contact, one damage roll."""
        R0 = self.R0
        P = self.bite_poses()
        w = round(WINDUP_OF_CONTACT * contact, 1)
        step = 0.5 * (contact + w)
        k = self.k
        s = (length - contact) / 480.0                      # the recovery's beats keep their share of it
        lifted = P["gape"].but(fl=(0.0, 0.08 * k, 0.07 * k), fold=(40.0, 0.0, 0.0, 0.0), toe=(30.0, 0.0, 0.0, 0.0),
                               hip=(0.0, 0.02 * k, -0.06 * k))
        keys = [(0.0, R0, "smooth"), (contact * 0.34, P["coil"], "out"), (w, P["gape"], "smooth"),
                (step, lifted, "in"), (contact, P["hit"], "in"), (contact + 110.0 * s, P["shake"], "out"),
                (contact + 150.0 * s, P["shake"], "hold"), (contact + 265.0 * s, P["back"], "smooth"),
                (contact + 360.0 * s, P["down"], "smooth"), (length, R0, "smooth")]
        return self.keyed(name, length, keys, notifies={"WindupEnd": w, "Contact": contact},
                          usage=usage or "the bite: one Contact (the jaws shut on it), one damage roll")

    def pounce(self, length: float = 1000.0, leap: float = 380.0, contact: float = 650.0, name: str = "Pounce",
               usage: str | None = None):
        """Pounce: the crouch (hips down and back, the chest to the ground, the tail up: the crimson line shows from
        the first frame), the spring at ``Leap`` (the paws leave the ground), flight — fores stretched ahead, the back
        hollow, the jaw open — the fores and the jaws strike on ``Contact``, the hinds land behind them, it gathers
        itself. In place: the core dashes the actor down the line; the clip carries the arc."""
        R0 = self.R0
        k = self.k
        w = round(WINDUP_OF_CONTACT * contact, 1)
        crouch = R0.but(hip=(0.0, -0.12 * k, -0.17 * k), pitch=9.0, arch=12.0, neck=(-22.0, 0.0), head=(-8.0, 0.0, 0.0),
                        jaw=8.0, ears=(-34.0, -34.0), tail=(22.0, 0.0, 6.0), reach=(0.03 * k, 0.03 * k))
        deep = crouch.but(hip=(0.0, -0.15 * k, -0.20 * k), pitch=11.0, arch=15.0, tail=(26.0, 0.0, 8.0), jaw=14.0)
        spring = R0.but(hip=(0.0, 0.10 * k, -0.02 * k), pitch=-16.0, arch=-10.0, neck=(8.0, 0.0), head=(-2.0, 0.0, 0.0),
                        jaw=26.0, ears=(-38.0, -38.0), tail=(-4.0, 0.0, -4.0), reach=(0.05 * k, -0.04 * k),
                        fl=(0.0, 0.22 * k, 0.20 * k), fr=(0.0, 0.26 * k, 0.24 * k),
                        fold=(50.0, 44.0, 0.0, 0.0), toe=(20.0, 20.0, 0.0, 0.0))     # the hinds planted: the push
        fly = QP(hip=(0.0, 0.14 * k, 0.26 * k), pitch=-9.0, arch=-14.0, neck=(6.0, 0.0), head=(2.0, 0.0, 0.0), jaw=40.0,
                 ears=(-40.0, -40.0), tail=(-10.0, 0.0, -5.0), follow=1.0, reach=(0.05 * k, -0.04 * k),
                 fl=(0.0, 0.30 * k, 0.10 * k), fr=(0.0, 0.34 * k, 0.12 * k), bl=(0.0, -0.34 * k, 0.12 * k),
                 br=(0.0, -0.30 * k, 0.10 * k), fold=(18.0, 14.0, 34.0, 30.0), toe=(10.0, 10.0, 50.0, 50.0))
        apex = fly.but(hip=(0.0, 0.16 * k, 0.33 * k), pitch=0.0, arch=-8.0, jaw=44.0)
        # the landing: the fores strike where they then stand (planted from Contact on), the hinds come down on
        # their own stand; the fores are brought back under the body one at a time — steps, never a drag
        f_l, f_r = (R0.fl[0], 0.20 * k, 0.0), (R0.fr[0], 0.24 * k, 0.0)
        strike = R0.but(hip=(0.0, 0.18 * k, 0.02 * k), pitch=15.0, arch=6.0, neck=(-8.0, 0.0), head=(10.0, 0.0, 0.0),
                        jaw=2.0, ears=(-38.0, -38.0), tail=(14.0, 0.0, 4.0), reach=(0.05 * k, 0.03 * k),
                        fl=f_l, fr=f_r,
                        bl=(R0.bl[0], -0.02 * k, 0.16 * k), br=(R0.br[0], 0.02 * k, 0.20 * k),
                        fold=(0.0, 0.0, 56.0, 60.0), toe=(0.0, 0.0, 40.0, 40.0))
        land = R0.but(hip=(0.0, 0.11 * k, -0.10 * k), pitch=5.0, arch=12.0, neck=(-4.0, 0.0), head=(6.0, -6.0, 6.0),
                      jaw=6.0, ears=(-30.0, -30.0), tail=(6.0, 0.0, 2.0), fl=f_l, fr=f_r)
        up = R0.but(hip=(0.0, 0.07 * k, -0.05 * k), pitch=2.0, arch=5.0, jaw=10.0, ears=(-16.0, -16.0), fr=f_r)

        def mid(a, b):
            return (0.5 * (a[0] + b[0]), 0.5 * (a[1] + b[1]), 0.05 * k)
        step_l = lerp_qp(land, up, 0.5).but(fl=mid(f_l, R0.fl), fr=f_r, fold=(34.0, 0.0, 0.0, 0.0),
                                            toe=(20.0, 0.0, 0.0, 0.0))
        step_r = lerp_qp(up, R0, 0.5).but(fr=mid(f_r, R0.fr), fold=(0.0, 34.0, 0.0, 0.0), toe=(0.0, 20.0, 0.0, 0.0))
        settle = R0.but(hip=(0.0, 0.01 * k, -0.01 * k), jaw=4.0, ears=(-8.0, -8.0))
        post = length - contact
        keys = [(0.0, R0, "smooth"), (leap * 0.50, crouch, "out"), (leap * 0.84, deep, "smooth"),
                (leap, spring, "in"), (leap + (contact - leap) * 0.34, fly, "out"),
                (leap + (contact - leap) * 0.66, apex, "smooth"), (contact, strike, "in"),
                (contact + post * 0.27, land, "out"), (contact + post * 0.41, step_l, "linear"),
                (contact + post * 0.55, up, "linear"), (contact + post * 0.69, step_r, "linear"),
                (contact + post * 0.83, settle, "linear"), (length, R0, "smooth")]
        return self.keyed(name, length, keys,
                          notifies={"WindupEnd": w, "Leap": leap, "FX_PounceLeap": leap, "Contact": contact,
                                    "FX_PounceLand": contact},
                          usage=usage or "the pounce (a dash line): Leap = the paws leave the ground, Contact = the "
                                         "fores and the jaws strike at the line's end (the core's windupMs); the "
                                         "clip is in place, the core dashes the actor")

    def howl(self, length: float = 1400.0, contact: float = 1000.0, name: str = "Cast01", usage: str | None = None):
        """The howl (the Pack-Leader's Cast01, Volgan's Cast02): it plants the fores, the hindquarters sink, the
        head comes up over the back line and the throat opens to the sky; the call goes out on ``Contact``
        (``FX_Howl``); the head comes down."""
        R0 = self.R0
        k = self.k
        w = round(WINDUP_OF_CONTACT * contact, 1)
        sit = R0.but(hip=(0.0, -0.05 * k, -0.10 * k), pitch=-12.0, arch=-4.0, neck=(-30.0, 0.0), head=(-10.0, 0.0, 0.0),
                     ears=(-14.0, -14.0), tail=(-14.0, 0.0, -4.0), reach=(-0.02 * k, 0.02 * k))
        throat = sit.but(pitch=-16.0, neck=(-74.0, 0.0), head=(-34.0, 0.0, 0.0), jaw=10.0, ears=(-34.0, -34.0),
                         tail=(-18.0, 0.0, -5.0))
        call = throat.but(neck=(-80.0, 0.0), head=(-38.0, 0.0, 0.0), jaw=34.0)
        hold = call.but(neck=(-78.0, 5.0), head=(-36.0, 4.0, 0.0), jaw=30.0)
        keys = [(0.0, R0, "smooth"), (contact * 0.30, sit, "smooth"), (w, throat, "smooth"), (contact, call, "in"),
                (contact + (length - contact) * 0.45, hold, "smooth"), (length, R0, "smooth")]
        return self.keyed(name, length, keys, notifies={"WindupEnd": w, "Contact": contact, "FX_Howl": contact},
                          usage=usage or "the howl (kind call): the call goes out on Contact (FX_Howl)")

    # reactions -----------------------------------------------------------------------------------------
    def hurt(self, length: float = 280.0):
        """Hurt: struck from the front — the forehand thrown back and down, the head snapped aside, a yelp."""
        R0 = self.R0
        k = self.k
        hit = R0.but(hip=(0.0, -0.09 * k, -0.07 * k), pitch=-6.0, arch=12.0, bend=-8.0, neck=(-16.0, -18.0),
                     head=(-10.0, -16.0, 14.0), jaw=22.0, ears=(-38.0, -38.0), tail=(-22.0, 6.0, -10.0),
                     fl=(0.0, 0.03 * k, 0.05 * k), fold=(30.0, 0.0, 0.0, 0.0))
        keys = [(0.0, lerp_qp(R0, hit, 0.6), "smooth"), (length * 0.22, hit, "out"),
                (length * 0.60, lerp_qp(R0, hit, 0.45), "smooth"), (length, R0, "smooth")]
        return self.keyed("Hurt", length, keys)

    def death_poses(self) -> dict:
        """The fall onto its side: ``lie`` is the corpse's pose (held from the clip's end)."""
        R0 = self.R0
        k = self.k
        J = self.J
        side_z = 0.118 * k                                    # the spine line's height when it lies on its side
        buckle = R0.but(hip=(-0.02 * k, -0.04 * k, -0.10 * k), pitch=10.0, arch=8.0, roll=8.0, bend=-6.0,
                        neck=(18.0, -10.0), head=(10.0, -10.0, 8.0), jaw=16.0, ears=(-30.0, -30.0),
                        tail=(-20.0, 0.0, -8.0), fl=(0.03 * k, 0.04 * k, 0.0), fr=(-0.04 * k, 0.10 * k, 0.0))
        drop = QP(hip=(0.05 * k, -0.02 * k, -(J["pelvis"].z - 0.30 * k)), pitch=6.0, roll=52.0, arch=10.0, bend=-10.0,
                  neck=(8.0, -14.0), head=(4.0, -10.0, 10.0), jaw=20.0, ears=(-34.0, -34.0), tail=(-26.0, 0.0, -10.0),
                  follow=1.0, fl=(0.0, 0.10 * k, 0.10 * k), fr=(0.0, 0.16 * k, 0.16 * k),
                  bl=(0.0, -0.06 * k, 0.12 * k), br=(0.0, -0.02 * k, 0.18 * k), fold=(20.0, 30.0, 20.0, 30.0))
        lie = QP(hip=(0.10 * k, 0.0, -(J["pelvis"].z - side_z)), pitch=0.0, roll=90.0, arch=14.0, bend=0.0, twist=0.0,
                 neck=(16.0, 0.0), head=(8.0, 0.0, 0.0), jaw=12.0, ears=(-36.0, -36.0), tail=(-30.0, 0.0, -12.0),
                 follow=1.0, fl=(0.0, 0.14 * k, 0.07 * k), fr=(0.0, 0.24 * k, 0.12 * k),
                 bl=(0.0, -0.12 * k, 0.06 * k), br=(0.0, -0.04 * k, 0.12 * k), fold=(24.0, 40.0, 16.0, 30.0),
                 toe=(25.0, 35.0, 25.0, 35.0))
        bounce = lie.but(hip=(0.10 * k, 0.0, -(J["pelvis"].z - side_z) + 0.025 * k), roll=84.0, neck=(8.0, 0.0),
                         fr=(0.0, 0.20 * k, 0.16 * k), br=(0.0, -0.02 * k, 0.16 * k))
        if self.probes:
            lie = self.grounded(lie, settle=True)
            bounce = lie.but(hip=(lie.hip[0], lie.hip[1], lie.hip[2] + 0.025 * k), roll=84.0, neck=(8.0, 0.0),
                             fr=(0.0, 0.20 * k, 0.16 * k), br=(0.0, -0.02 * k, 0.16 * k))
            drop = self.grounded(drop)
        return dict(buckle=buckle, drop=drop, bounce=bounce, lie=lie)

    def death(self, length: float = 900.0):
        """Death: the forehand buckles, it goes down over its left shoulder and rolls onto its side, the legs run
        out and are still. The last frame is the corpse (``corpse {leaves: 'remains'}``): the runtime greys it to
        a husk."""
        R0 = self.R0
        P = self.death_poses()
        s = length / 900.0
        keys = [(0.0, R0, "smooth"), (170.0 * s, P["buckle"], "out"), (430.0 * s, P["drop"], "in"),
                (590.0 * s, P["lie"], "in"), (700.0 * s, P["bounce"], "out"), (820.0 * s, P["lie"], "in"),
                (length, P["lie"], "hold")]
        return self.keyed("Death", length, keys, layer=(lambda q, t: self.grounded(q)) if self.probes else None,
                          usage="rolls onto its side; the last frame is the corpse pose the husk holds")

    def stun(self, length: float = 700.0):
        """Stun (loop): braced wide and swaying, the head hanging and rolling, the tail down."""
        R0 = self.R0
        k = self.k

        def fn(t):
            ph = 2.0 * math.pi * t / length
            return R0.but(hip=(0.022 * k * math.sin(ph), 0.0, -0.05 * k + 0.008 * k * math.cos(2.0 * ph)),
                          pitch=4.0, roll=3.0 * math.sin(ph), arch=4.0, bend=4.0 * math.sin(ph + 0.8),
                          neck=(30.0 + 4.0 * math.sin(2.0 * ph), 12.0 * math.sin(ph + 0.5)),
                          head=(12.0, 8.0 * math.cos(ph), 16.0 * math.sin(ph + 0.3)), jaw=14.0 + 4.0 * math.sin(ph),
                          ears=(-22.0 + 6.0 * math.sin(ph), -22.0 - 6.0 * math.sin(ph)),
                          tail=(-26.0, 5.0 * math.sin(ph + 1.5), -8.0),
                          fl=(0.05 * k, 0.03 * k, 0.0), fr=(-0.05 * k, 0.03 * k, 0.0),
                          bl=(0.04 * k, -0.02 * k, 0.0), br=(-0.04 * k, -0.02 * k, 0.0))
        return self.clip("Stun", length, fn, loop=True, key_ms=[length * i / 4 for i in range(5)],
                         usage="stunned (stun stars over fx_overhead): loops until the stun ends")

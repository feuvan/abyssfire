"""Poses, clips and baking (spec art-inventory-ch1.md §0.2, §2.5, §3.4, §10).

Pose model (mirrors the web ``HumanPose``: angles + IK targets + pelvis offset, interpolated then *solved*):

* ``Pose.rot(bone, pitch=, roll=, yaw=)`` — rotation **relative to the parent**, about **character axes**
  (X = character left, Y = back, Z = up; the character faces −Y). ``pitch`` > 0 tips an upward bone forward,
  ``roll`` > 0 tips it toward the character's left, ``yaw`` > 0 turns it to the left. Independent of bone roll.
* ``Pose.swing / lift / spread / twist`` — rotations relative to the bone's **rest direction**
  (tip forward / up / outward, twist about the bone axis) — natural for A-pose limbs.
* ``Pose.world(bone, ...)`` / ``Pose.aim(bone, direction, up)`` — **absolute** orientation in character space
  (feet flat on the ground, a weapon pointing where the web's ``wpn`` angle says).
* ``Pose.move(bone, offset)`` — armature-space translation of the bone head (pelvis bob/lunge).
* ``Pose.foot(side, at=…)`` / ``Pose.hand(side, at=…)`` — two-bone IK targets (ankle / wrist), with a pole
  direction; solved analytically into FK rotations, so exported clips are plain FK.

Clips are sampled **per frame at 60 fps** with the web's easing (ease of the *destination* key: 'smooth' =
smoothstep, 'in' = t³, 'out' = 1−(1−t)³, 'linear', 'hold'), quaternion slerp, and IK solved on the
interpolated targets (planted feet stay planted). Root is never keyed (in place, no root motion).
Notifies (``Contact``, ``Release``, ``FootL``…) are stored in ms for the manifest and as pose markers.
"""
from __future__ import annotations

import json
import math
from dataclasses import dataclass, field
from typing import Callable, Iterable, Sequence

import bpy
from mathutils import Euler, Matrix, Quaternion, Vector

from . import scene
from .rig import FWD, LEFT, UP, Rig

IK_CHAINS = {
    "leg_l": ("thigh_l", "calf_l", "foot_l"),
    "leg_r": ("thigh_r", "calf_r", "foot_r"),
    "arm_l": ("upperarm_l", "lowerarm_l", "hand_l"),
    "arm_r": ("upperarm_r", "lowerarm_r", "hand_r"),
}
DEFAULT_POLE = {"leg_l": FWD, "leg_r": FWD, "arm_l": Vector((0.3, 1.0, -0.2)), "arm_r": Vector((-0.3, 1.0, -0.2))}
SKIP_KEY_BONES = {"root", "ik_foot_root", "ik_foot_l", "ik_foot_r", "ik_hand_root", "ik_hand_gun",
                  "ik_hand_l", "ik_hand_r"}


def _q_axis(axis: Vector, deg: float) -> Quaternion:
    return Quaternion(Vector(axis).normalized(), math.radians(deg))


# ── Pose ────────────────────────────────────────────────────────────────────────────────────────────────
class Pose:
    def __init__(self, rig: Rig):
        self.rig = rig
        self.rel: dict[str, Quaternion] = {}
        self.abs: dict[str, Quaternion] = {}
        self.loc: dict[str, Vector] = {}
        self.ik: dict[str, tuple[Vector, Vector]] = {}   # chain → (target, pole dir)

    # rest helpers
    def _dir(self, bone: str) -> Vector:
        b = self.rig.obj.data.bones[bone]
        return (b.tail_local - b.head_local).normalized()

    def _push_rel(self, bone: str, q: Quaternion) -> "Pose":
        self.rel[bone] = q @ self.rel.get(bone, Quaternion())
        return self

    def rot(self, bone: str, pitch: float = 0.0, roll: float = 0.0, yaw: float = 0.0) -> "Pose":
        q = Euler((math.radians(pitch), math.radians(roll), math.radians(yaw)), "XYZ").to_quaternion()
        return self._push_rel(bone, q)

    def swing(self, bone: str, deg: float) -> "Pose":
        """Tip toward the character's front (+) / back (−)."""
        d = self._dir(bone)
        ax = d.cross(FWD)
        if ax.length < 1e-4:
            ax = LEFT if d.dot(UP) > 0 else -LEFT
        return self._push_rel(bone, _q_axis(ax, deg))

    def lift(self, bone: str, deg: float) -> "Pose":
        """Tip toward up (+) / down (−)."""
        d = self._dir(bone)
        ax = d.cross(UP)
        if ax.length < 1e-4:
            ax = LEFT
        return self._push_rel(bone, _q_axis(ax, deg))

    def spread(self, bone: str, deg: float) -> "Pose":
        """Tip away from the body midline (+) / toward it (−)."""
        d = self._dir(bone)
        side = 1.0 if self.rig.obj.data.bones[bone].head_local.x >= 0 else -1.0
        out = Vector((side, 0, 0))
        ax = d.cross(out)
        if ax.length < 1e-4:
            ax = FWD
        return self._push_rel(bone, _q_axis(ax, deg))

    def twist(self, bone: str, deg: float) -> "Pose":
        return self._push_rel(bone, _q_axis(self._dir(bone), deg))

    def world(self, bone: str, pitch: float = 0.0, roll: float = 0.0, yaw: float = 0.0) -> "Pose":
        """Absolute orientation: rest orientation rotated about character axes."""
        self.abs[bone] = Euler((math.radians(pitch), math.radians(roll), math.radians(yaw)), "XYZ").to_quaternion()
        return self

    def aim(self, bone: str, direction: Sequence[float], up: Sequence[float] = (0, 0, 1)) -> "Pose":
        """Absolute: point the bone's +Y along ``direction`` with +Z toward ``up`` (weapons, props)."""
        from .rig import frame
        rest = self.rig.obj.data.bones[bone].matrix_local.to_3x3()
        tgt = frame((0, 0, 0), direction, up).to_3x3()
        self.abs[bone] = (tgt @ rest.inverted()).to_quaternion()
        return self

    def move(self, bone: str, offset: Sequence[float]) -> "Pose":
        self.loc[bone] = self.loc.get(bone, Vector()) + Vector(offset)
        return self

    def foot(self, side: str, at: Sequence[float] | None = None, offset: Sequence[float] = (0, 0, 0),
             pole: Sequence[float] | None = None) -> "Pose":
        """IK the leg so the ankle reaches ``at`` (armature space) or rest ankle + ``offset``."""
        tgt = Vector(at) if at is not None else self.rig.joint(f"ankle_{side}") + Vector(offset)
        self.ik[f"leg_{side}"] = (tgt, Vector(pole) if pole is not None else DEFAULT_POLE[f"leg_{side}"].copy())
        return self

    def hand(self, side: str, at: Sequence[float] | None = None, offset: Sequence[float] = (0, 0, 0),
             pole: Sequence[float] | None = None) -> "Pose":
        tgt = Vector(at) if at is not None else self.rig.joint(f"wrist_{side}") + Vector(offset)
        self.ik[f"arm_{side}"] = (tgt, Vector(pole) if pole is not None else DEFAULT_POLE[f"arm_{side}"].copy())
        return self

    # combination
    def copy(self) -> "Pose":
        p = Pose(self.rig)
        p.rel = {k: q.copy() for k, q in self.rel.items()}
        p.abs = {k: q.copy() for k, q in self.abs.items()}
        p.loc = {k: v.copy() for k, v in self.loc.items()}
        p.ik = {k: (a.copy(), b.copy()) for k, (a, b) in self.ik.items()}
        return p

    def then(self, fn: Callable[["Pose"], object]) -> "Pose":
        """Apply an in-place modifier and return a copy (``base.then(lambda p: p.rot(...))``)."""
        p = self.copy()
        fn(p)
        return p

    def mirrored(self) -> "Pose":
        """Swap _l/_r and mirror across the character's YZ plane."""
        def sw(n: str) -> str:
            return n[:-2] + ("_r" if n.endswith("_l") else "_l") if n.endswith(("_l", "_r")) else n

        def mq(q: Quaternion) -> Quaternion:
            return Quaternion((q.w, q.x, -q.y, -q.z))

        def mv(v: Vector) -> Vector:
            return Vector((-v.x, v.y, v.z))
        p = Pose(self.rig)
        p.rel = {sw(k): mq(q) for k, q in self.rel.items()}
        p.abs = {sw(k): mq(q) for k, q in self.abs.items()}
        p.loc = {sw(k): mv(v) for k, v in self.loc.items()}
        p.ik = {sw(k): (mv(a), mv(b)) for k, (a, b) in self.ik.items()}
        return p

    def to_json(self) -> dict:
        return {"rel": {k: list(q) for k, q in self.rel.items()}, "abs": {k: list(q) for k, q in self.abs.items()},
                "loc": {k: list(v) for k, v in self.loc.items()},
                "ik": {k: [list(a), list(b)] for k, (a, b) in self.ik.items()}}

    @staticmethod
    def from_json(rig: Rig, d: dict) -> "Pose":
        p = Pose(rig)
        p.rel = {k: Quaternion(v) for k, v in d.get("rel", {}).items()}
        p.abs = {k: Quaternion(v) for k, v in d.get("abs", {}).items()}
        p.loc = {k: Vector(v) for k, v in d.get("loc", {}).items()}
        p.ik = {k: (Vector(a), Vector(b)) for k, (a, b) in d.get("ik", {}).items()}
        return p


def lerp_pose(a: Pose, b: Pose, t: float) -> Pose:
    """Interpolate (slerp rotations, lerp offsets / IK targets). Missing entries count as identity/zero;
    an IK chain present in only one pose is filled with the other pose's FK end position."""
    p = Pose(a.rig)
    for k in set(a.rel) | set(b.rel):
        p.rel[k] = a.rel.get(k, Quaternion()).slerp(b.rel.get(k, Quaternion()), t)
    for k in set(a.abs) | set(b.abs):
        p.abs[k] = a.abs.get(k, Quaternion()).slerp(b.abs.get(k, Quaternion()), t)
    for k in set(a.loc) | set(b.loc):
        p.loc[k] = a.loc.get(k, Vector()).lerp(b.loc.get(k, Vector()), t)
    for k in set(a.ik) | set(b.ik):
        ta = a.ik.get(k) or (_fk_end(a, k), b.ik[k][1])
        tb = b.ik.get(k) or (_fk_end(b, k), a.ik[k][1])
        p.ik[k] = (ta[0].lerp(tb[0], t), ta[1].lerp(tb[1], t).normalized())
    return p


def _fk_end(p: Pose, chain: str) -> Vector:
    mats = evaluate(p, return_matrices=True)
    return mats[IK_CHAINS[chain][1]] @ Vector((0, p.rig.obj.data.bones[IK_CHAINS[chain][1]].length, 0))


# ── solver ──────────────────────────────────────────────────────────────────────────────────────────────
def _bones_in_order(arm: bpy.types.Armature) -> list:
    out, seen = [], set()

    def visit(b):
        if b.name in seen:
            return
        if b.parent is not None:
            visit(b.parent)
        seen.add(b.name)
        out.append(b)
    for b in arm.bones:
        visit(b)
    return out


def _solve_two_bone(H: Vector, tgt: Vector, l1: float, l2: float, pole: Vector, rest_hinge: Vector):
    d = tgt - H
    D = d.length
    u = d.normalized() if D > 1e-9 else Vector((0, 0, -1))
    D = max(abs(l1 - l2) + 1e-4, min(l1 + l2 - 1e-5, D))
    v = pole - u * pole.dot(u)
    if v.length < 1e-6:
        v = rest_hinge.cross(u)
    v.normalize()
    hinge = v.cross(u)
    if hinge.dot(rest_hinge) < 0:      # never bend a joint backwards
        v = -v
        hinge = -hinge
    a = (l1 * l1 - l2 * l2 + D * D) / (2 * D)
    b = math.sqrt(max(l1 * l1 - a * a, 0.0))
    K = H + u * a + v * b
    T = H + u * D
    return K, T, hinge.normalized()


def _frame3(x: Vector, y: Vector) -> Matrix:
    y = y.normalized()
    x = (x - y * x.dot(y)).normalized()
    z = x.cross(y)
    return Matrix((x, y, z)).transposed()


def evaluate(pose: Pose, return_matrices: bool = False):
    """Solve a pose → {bone: (basis_loc Vector, basis_quat Quaternion)} (or posed armature matrices)."""
    arm = pose.rig.obj.data
    posed: dict[str, Matrix] = {}
    basis: dict[str, tuple[Vector, Quaternion]] = {}
    pending: dict[str, Matrix] = {}      # lower-bone target rotations from IK
    ik_upper = {IK_CHAINS[c][0]: c for c in pose.ik}
    ik_ends = {IK_CHAINS[c][2] for c in pose.ik}
    for b in _bones_in_order(arm):
        rest = b.matrix_local
        if b.parent is not None:
            base = posed[b.parent.name] @ b.parent.matrix_local.inverted() @ rest
        else:
            base = rest.copy()
        base_rot = base.to_3x3().normalized()
        rest_rot = rest.to_3x3().normalized()
        if b.name in ik_upper:
            chain = ik_upper[b.name]
            tgt, pole = pose.ik[chain]
            lower = arm.bones[IK_CHAINS[chain][1]]
            rest_hinge = base_rot @ Vector((1, 0, 0))
            K, T, hinge = _solve_two_bone(base.translation.copy(), tgt, b.length, lower.length, pole, rest_hinge)
            tr = _frame3(hinge, K - base.translation)
            pending[lower.name] = _frame3(hinge, T - K)
            q = (base_rot.inverted() @ tr).to_quaternion()
        elif b.name in pending:
            q = (base_rot.inverted() @ pending[b.name]).to_quaternion()
        elif b.name in pose.abs:
            q = (base_rot.inverted() @ pose.abs[b.name].to_matrix() @ rest_rot).to_quaternion()
        elif b.name in ik_ends and b.name not in pose.rel:
            q = (base_rot.inverted() @ rest_rot).to_quaternion()     # IK'd foot/hand keeps its rest orientation
        elif b.name in pose.rel:
            q = (rest_rot.inverted() @ pose.rel[b.name].to_matrix() @ rest_rot).to_quaternion()
        else:
            q = Quaternion()
        loc = base_rot.inverted() @ pose.loc[b.name] if b.name in pose.loc else Vector()
        basis[b.name] = (loc, q)
        posed[b.name] = base @ (Matrix.Translation(loc) @ q.to_matrix().to_4x4())
    return posed if return_matrices else basis


def apply_pose(pose: Pose) -> None:
    """Set the armature's pose bones (for stills / portraits / previews)."""
    obj = pose.rig.obj
    for name, (loc, q) in evaluate(pose).items():
        pb = obj.pose.bones[name]
        pb.rotation_mode = "QUATERNION"
        pb.location = loc
        pb.rotation_quaternion = q
    bpy.context.view_layer.update()


def clear_pose(rig: Rig) -> None:
    for pb in rig.obj.pose.bones:
        pb.location = (0, 0, 0)
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.scale = (1, 1, 1)
    bpy.context.view_layer.update()


# ── easing / clips ──────────────────────────────────────────────────────────────────────────────────────
def ease(t: float, kind: str) -> float:
    """Web ``applyEase`` (src/graphics/sprites/rig/Rig.ts:76-84)."""
    t = min(1.0, max(0.0, t))
    if kind == "in":
        return t * t * t
    if kind == "out":
        return 1 - (1 - t) ** 3
    if kind == "linear":
        return t
    if kind == "hold":
        return 0.0 if t < 1 else 1.0
    return t * t * (3 - 2 * t)


@dataclass
class Key:
    pose: Pose
    t: float | None = None        # normalised time 0..1 of the key span (web ``at``)
    ms: float | None = None       # or absolute ms
    ease: str = "smooth"          # easing used travelling INTO this key


@dataclass
class Clip:
    """A clip: keys (web-style) or a procedural sampler, plus notifies; length in ms at play-rate 1.

    ``key_span_ms``: time of normalised t = 1 (web one-shots: (frames − 1) / fps; loops: = length).
    ``layers``: additive callables ``fn(pose_copy, t_ms) -> None`` applied after key interpolation
    (breathing, cloth flow, secondary chains).
    """

    name: str                      # action token: Idle, Run, Attack01, Cast01, Hurt, Death, Dodge, …
    length_ms: float
    keys: list = field(default_factory=list)
    loop: bool = False
    key_span_ms: float | None = None
    notifies: dict = field(default_factory=dict)     # {'Contact': 308.0, 'FootL': [..]}
    sampler: Callable[[float], Pose] | None = None
    layers: list = field(default_factory=list)
    additive: bool = False
    ref_speed: float | None = None  # m/s for locomotion (play rate = v / ref)

    @property
    def frames(self) -> int:
        return scene.ms_to_frame(self.length_ms)

    def key_ms(self, k: Key) -> float:
        if k.ms is not None:
            return k.ms
        span = self.key_span_ms if self.key_span_ms is not None else self.length_ms
        return (k.t or 0.0) * span

    def sample(self, t_ms: float) -> Pose:
        if self.sampler is not None:
            p = self.sampler(t_ms)
        else:
            ks = sorted(self.keys, key=self.key_ms)
            if not ks:
                raise ValueError(f"clip {self.name} has no keys")
            if t_ms <= self.key_ms(ks[0]):
                p = ks[0].pose.copy()
            elif t_ms >= self.key_ms(ks[-1]):
                p = ks[-1].pose.copy()
            else:
                p = ks[-1].pose.copy()
                for a, b in zip(ks, ks[1:]):
                    ta, tb = self.key_ms(a), self.key_ms(b)
                    if ta <= t_ms <= tb:
                        p = lerp_pose(a.pose, b.pose, ease((t_ms - ta) / max(tb - ta, 1e-6), b.ease))
                        break
        for layer in self.layers:
            layer(p, t_ms)
        return p

    def notify_list(self) -> list[dict]:
        out = []
        for name, v in sorted(self.notifies.items()):
            for ms in (v if isinstance(v, (list, tuple)) else [v]):
                out.append({"name": name, "ms": round(float(ms), 1)})
        return sorted(out, key=lambda d: (d["ms"], d["name"]))


def action_name(asset_token: str, clip: Clip) -> str:
    """``A_<Family>_<Name>_<Action>`` from an asset token like ``Hero_Warrior`` (spec §2.2)."""
    return f"A_{asset_token}_{clip.name}"


def _new_action(name: str, arm_obj: bpy.types.Object):
    old = bpy.data.actions.get(name)
    if old is not None:
        bpy.data.actions.remove(old)
    act = bpy.data.actions.new(name)
    slot = act.slots.new(id_type="OBJECT", name=arm_obj.name)
    layer = act.layers.new("Layer")
    strip = layer.strips.new(type="KEYFRAME")
    cb = strip.channelbags.new(slot)
    return act, slot, cb


def bake(rig: Rig, clip: Clip, asset_token: str, bones: Iterable[str] | None = None) -> bpy.types.Action:
    """Sample ``clip`` every frame (60 fps) and write an action with linear keys (root untouched)."""
    arm = rig.obj
    n = clip.frames
    names = [b.name for b in _bones_in_order(arm.data)
             if b.name not in SKIP_KEY_BONES and (bones is None or b.name in bones)]
    samples = []
    for f in range(n + 1):
        t = scene.frame_to_ms(f)
        if clip.loop and f == n:
            t = clip.length_ms if clip.sampler is None else 0.0
        samples.append(evaluate(clip.sample(min(t, clip.length_ms))))
    if clip.loop:
        samples[-1] = samples[0]
    act, slot, cb = _new_action(action_name(asset_token, clip), arm)
    for name in names:
        prev = None
        quats, locs = [], []
        for s in samples:
            loc, q = s[name]
            if prev is not None and prev.dot(q) < 0:
                q = -q
            prev = q
            quats.append(q)
            locs.append(loc)
        dp_q = f'pose.bones["{name}"].rotation_quaternion'
        dp_l = f'pose.bones["{name}"].location'
        for i in range(4):
            _write_fcurve(cb, dp_q, i, name, [q[i] for q in quats])
        if any(l.length > 1e-7 for l in locs):
            for i in range(3):
                _write_fcurve(cb, dp_l, i, name, [l[i] for l in locs])
    for nm, ms in [(d["name"], d["ms"]) for d in clip.notify_list()]:
        m = act.pose_markers.new(nm)
        m.frame = scene.ms_to_frame(ms)
    act.use_frame_range = True
    act.frame_start, act.frame_end = 0, n
    act.use_cyclic = clip.loop
    act.use_fake_user = True
    act["af_clip"] = json.dumps({"name": clip.name, "lengthMs": clip.length_ms, "loop": clip.loop,
                                 "notifies": clip.notify_list(), "additive": clip.additive,
                                 "refSpeed": clip.ref_speed})
    assign(rig, act)
    return act


def _write_fcurve(cb, path: str, index: int, group: str, values: list[float]) -> None:
    fc = cb.fcurves.new(path, index=index, group_name=group)
    fc.keyframe_points.add(len(values))
    co = []
    for f, v in enumerate(values):
        co += [float(f), float(v)]
    fc.keyframe_points.foreach_set("co", co)
    for kp in fc.keyframe_points:
        kp.interpolation = "LINEAR"
    fc.update()


def assign(rig: Rig, act: bpy.types.Action | None) -> None:
    arm = rig.obj
    ad = arm.animation_data or arm.animation_data_create()
    ad.action = act
    if act is not None:
        ad.action_slot = act.slots[0]
        bpy.context.scene.frame_start = int(act.frame_start)
        bpy.context.scene.frame_end = int(act.frame_end)


def set_frame(frame: int) -> None:
    bpy.context.scene.frame_set(int(frame))


# ── procedural helpers (ports of the web gait / idle) ───────────────────────────────────────────────────
def gait(t: float, stride: float, lift: float, bob: float, foot_spread: float = 0.0):
    """Web ``gait()`` in 3D (Humanoid.ts:208-232): ``t`` in [0,1) → (footL_offset, footR_offset, pelvis_dz, swing).

    Offsets are armature-space deltas from the rest ankles (−Y = forward). ``stride`` is the half
    amplitude (m), ``lift`` the foot lift, ``bob`` the pelvis drop at passing.
    """
    ph = t * 2 * math.pi

    def foot(p, x):
        return Vector((x, -stride * math.cos(p), lift * max(0.0, -math.sin(p))))
    return foot(ph, foot_spread), foot(ph + math.pi, -foot_spread), -bob * abs(math.sin(ph)), math.cos(ph)


def run_gait(t: float, speed: float, cycle_ms: float, duty: float = 0.3, lift: float = 0.15,
             bob: float = 0.03, foot_spread: float = 0.0):
    """No-slide locomotion gait (R9 / QUIRK A5): each foot is planted for ``duty`` of the cycle and moves
    back at exactly ``speed`` (m/s) while planted, then swings forward with a lift arc (flight phase when
    duty < 0.5). ``t`` in [0,1). Left foot touches down at t = 0, right at t = 0.5.

    Returns (footL_offset, footR_offset, pelvis_dz, swing) — offsets are armature-space deltas from the rest
    ankles; ``swing`` = +1 when the left foot is forward (arms counter-swing).
    """
    S = speed * cycle_ms / 1000.0 * duty / 2.0      # half stance length

    def foot(p, x):
        p %= 1.0
        if p < duty:                                   # stance: front (−S) → back (+S), on the ground
            s = p / duty
            return Vector((x, -S + 2 * S * s, 0.0)), math.sin(math.pi * s)
        s = (p - duty) / (1.0 - duty)                  # swing: back → front with lift
        e = s * s * (3 - 2 * s)
        return Vector((x, S - 2 * S * e, lift * math.sin(math.pi * s) ** 0.8)), 0.0
    fl, wl = foot(t, foot_spread)
    fr, wr = foot(t + 0.5, -foot_spread)
    dz = -bob * max(wl, wr) + bob * 0.4 * (1 - max(wl, wr))
    return fl, fr, dz, -fl.y / max(S, 1e-6)


def run_contacts_ms(cycle_ms: float) -> dict:
    return {"FootL": [0.0], "FootR": [round(cycle_ms * 0.5, 1)]}


def foot_plant_ms(length_ms: float) -> dict:
    """Contact times of the web gait (feet plant when lift ends): left at t=0, right at t=0.5."""
    return {"FootL": [0.0], "FootR": [round(length_ms * 0.5, 1)]}


# ── pose library ────────────────────────────────────────────────────────────────────────────────────────
class PoseLib:
    """Named poses for a rig; stored as JSON on the armature (``af_poses``) for inspection / reuse."""

    def __init__(self, rig: Rig):
        self.rig = rig
        self.poses: dict[str, Pose] = {}
        raw = rig.obj.data.get("af_poses")
        if raw:
            for k, v in json.loads(raw).items():
                self.poses[k] = Pose.from_json(rig, v)

    def add(self, name: str, pose: Pose) -> Pose:
        self.poses[name] = pose
        self._save()
        return pose

    def __getitem__(self, name: str) -> Pose:
        return self.poses[name].copy()

    def __contains__(self, name: str) -> bool:
        return name in self.poses

    def mirror(self, src: str, dst: str) -> Pose:
        return self.add(dst, self.poses[src].mirrored())

    def apply(self, name: str) -> None:
        apply_pose(self.poses[name])

    def _save(self) -> None:
        self.rig.obj.data["af_poses"] = json.dumps({k: p.to_json() for k, p in sorted(self.poses.items())})

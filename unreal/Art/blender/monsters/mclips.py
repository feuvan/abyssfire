"""Goblin-family clips (spec §4 general rules, §4.2–§4.4 clip tables, §10 master list; DECISIONS R9 / A5 / A15).

Poses are authored on the hero ``BP`` body pose (``heroes/common.py``: pelvis, lean/twist, IK feet / hands, weapon
aims, whole-body spin about a pivot, cloth flow) and solved by ``to_pose`` — the same machinery as the warrior.
Web side-view numbers (``MonsterKit.humanoidTracks``: READY → wind (t .33, ease-out) → mid (.67, ease-in) → HIT
(1, linear) = contact, recoil hurt, the backward death fall) are converted with the monster's rig unit ``U``
(forward = x − CENTER_X, height = GROUND_Y − y) and given the lateral placement / twist a side view cannot say.

Timing (spec §4 / §10, play rate 1): Idle 667 loop · Walk 600 loop (no-slide trot at the monster's ref speed:
planted feet move at exactly the ground speed, R9) · Attack01 500 (WindupEnd 155, Contact 250 — the web's last
attack frame — then an authored recovery to READY by 500) · Hurt 200 · Death 500 (+ hold / dithered fade in UE)
· Stun 600 loop (C5) and the per-monster extras (chief Roar 1200, shaman Taunt 900).
"""
from __future__ import annotations

import math
from typing import Callable

import numpy as np
from mathutils import Quaternion, Vector

from kit import anim
from kit.anim import Pose

import common as C
from common import BP, K, V

# goblin channels on the shared body pose: jaw opening (deg), the attack/cast intensity (web ``fx``)
C.BP_DEFAULTS.setdefault("jaw", 0.0)
C.BP_DEFAULTS.setdefault("fx", 0.0)

WINDUP_END_MS = 155.0          # telegraph tint ends (62 % of the contact time, spec §1.8 / §10)
CONTACT_MS = 250.0


class GoblinAnims:
    """Clip factory for one goblin-family mesh (``kit`` = its :class:`goblins.GoblinKit`)."""

    def __init__(self, kit, ref_speed: float, walk: dict, ready_fn: Callable[["GoblinAnims"], BP],
                 extra_cloth: Callable[[Pose, BP, "GoblinAnims"], None] | None = None):
        self.kit = kit
        self.rig = kit.rig
        self.J = kit.J
        self.U = kit.U
        self.ref_speed = ref_speed
        self.walk_p = walk
        self.extra_cloth = extra_cloth
        self.AZ = self.J["ankle_l"].z
        self.leg = self.rig.spec.thigh + self.rig.spec.shin
        self._ready_fn = ready_fn
        self.feet = None

    # ── web → body space ───────────────────────────────────────────────────────────────────────────────
    def web_pelvis(self, x: float, y: float, lat: float = 0.0) -> Vector:
        return V(lat, (x - 48.0) * self.U, (91.0 - y) * self.U - self.J["pelvis"].z)

    def web_foot(self, x: float, lat: float, y: float = 91.0) -> Vector:
        return V(lat, (x - 48.0) * self.U, self.AZ + (91.0 - y) * self.U)

    def web_hand(self, x: float, y: float, lat: float) -> Vector:
        return V(lat, (x - 48.0) * self.U, (91.0 - y) * self.U)

    def ready(self) -> BP:
        return self._ready_fn(self)

    # ── cloth / secondary (ears, loincloth flaps, + the monster's own chains) ───────────────────────────
    def cloth(self, p: Pose, bp: BP) -> None:
        flick = math.sin(bp.wave + 1.0) * 0.6 + bp.flow * 1.5
        for s, sg in (("l", 1), ("r", -1)):
            # web flick: the tip swings back / up with the flow (running, recoil) and a sine
            p.rot(f"ear_{s}_01", pitch=-2.0 * flick, yaw=sg * (-3.0 * flick))
            p.rot(f"ear_{s}_02", pitch=-3.0 * flick + 2.0 * math.sin(bp.wave * 2.0 + sg),
                  yaw=sg * (-2.5 * flick))
        # flaps hang in the world (aims are absolute; apply_spin turns them with a falling body)
        sw = bp.sway
        for nm, sg in (("loin_f", 1.0), ("loin_b", -1.0)):
            base = 6.0 + bp.flow * 28.0
            a1 = math.radians(base * 0.7 + sg * 10.0 * sw)
            a2 = math.radians(base + sg * 14.0 * sw + 4.0 * math.sin(bp.wave * 2 + sg))
            for k, a in ((1, a1), (2, a2)):
                d = Vector((0.0, -sg * math.sin(a), -math.cos(a)))
                p.aim(f"{nm}_{k:02d}", d, Vector((0.0, -sg * math.cos(a), math.sin(a))))
        if bp.jaw:
            p.rot("jaw", pitch=bp.jaw)
        if self.extra_cloth is not None:
            self.extra_cloth(p, bp, self)

    def solve(self, bp: BP) -> Pose:
        return C.to_pose(self.rig, bp, self.cloth)

    def plant(self, bp: BP, t: float = 0.0) -> BP:
        if self.feet is None:
            return bp
        return C.plant_feet(self.rig, bp, self.feet)

    def clip(self, name, length, keys, span, loop=False, notifies=None, ref_speed=None, layer=None, post=None):
        lay = layer or (lambda bp, t: bp.but(wave=2 * math.pi * t / length))
        c = C.body_clip(self.rig, name, length, C.keyed(keys, span, lay), cloth=self.cloth, loop=loop,
                        notifies=notifies, ref_speed=ref_speed, post=post or self.plant)
        c.key_poses_ms = sorted({round(k.t * span, 1) for k in keys})
        return c

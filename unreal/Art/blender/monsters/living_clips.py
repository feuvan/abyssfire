"""Clips of the Living (dark remake A3; monsters.md §2.3 / §5): the monster clip set on the townsfolk body.

``LivingAnims`` stands on ``npc_clips.NpcAnims`` (the folk body's cloth: robe / coat skirts on the four ``skirt_*``
chains with the leg envelope and the ``coat_th_*`` thigh drivers, the pack bounce, the floor rules of falls) and
replaces the NPC's work loops with a monster's: a READY stance per role (``ready()``, subclass), and

* ``Idle`` loop — breathing over READY, a small weapon sway (``idle_ms``);
* ``Walk`` loop — the no-slide gait (planted feet move at exactly ``ref_speed``, R9; FootL / FootR), counter-swung
  free arm, the weapon carried by ``carry()`` (subclass);
* ``Attack01`` — authored keys (``attack_keys()``, subclass) with **WindupEnd** (the clip-derived telegraph tint ends,
  62 % of the contact, as the goblins) and **Contact** at the monsters.md contact beat;
* ``Hurt`` 300 — a recoil off the hit and back;
* ``Death`` 900 — the hit, the knees buckle, the body folds forward and topples face down (the Unburied's ash-husk
  fall: "folds forward, crumbles"), the weapon flung flat on the ground beside the body;
* ``Stun`` 600 loop (C5) — dazed sway, head lolling, the weapon hand drooping;
* ``Cast01`` (the drummer, subclass).

Poses are ``heroes/common.BP`` body poses solved by ``common.to_pose`` (IK feet / hands, item aims, whole-body spin).
"""
from __future__ import annotations

import math
from types import SimpleNamespace

import numpy as np
from mathutils import Quaternion, Vector

from kit import anim

import common as C
from common import BP, K, V
import hero_base as HB
from npc_clips import NpcAnims

FPS_MS = 1000.0 / 60.0


class LivingAnims(NpcAnims):
    """Monster clip factory on a ``living.LivingFolk`` body. ``items`` = {object name: (object, bone)} of the held
    weapons (ground contact in falls)."""

    REF_SPEED = 1.5
    WALK_MS = 1000.0
    WALK = dict(duty=0.60, lift=0.075, bob=0.016, lean=6.0, crouch=-0.03, stance=0.11, hip_pitch=2.0)
    IDLE_MS = 1200.0
    ATTACK_MS = 800.0
    CONTACT_MS = 450.0
    WINDUP_FRAC = 0.62                # WindupEnd = 62 % of the contact (goblins: 155 / 250)
    HURT_MS = 300.0
    DEATH_MS = 900.0
    STUN_MS = 600.0
    GRIPS_LIVING: tuple = ("r",)
    CLOTH_EXTRA = ("mail_hem", "mail_row", "rope_end", "belt_rag", "quilt", "cowl", "tatter", "armband_tail")

    def __init__(self, rig, folk, items: dict):
        npc = SimpleNamespace(id=folk.asset, work="Guard", gesture_hand="l", extra_clips=(), scenery=(), fx={})
        super().__init__(rig, folk, npc, {n: o for n, (o, _) in items.items()})
        self.GRIPS = self.GRIPS_LIVING
        self.ITEMS = tuple((n, bone) for n, (_, bone) in items.items())
        self.CLOTH_PARTS = tuple(self.CLOTH_PARTS) + self.CLOTH_EXTRA
        self.item_objs = items

    # ── poses (subclasses) ──────────────────────────────────────────────────────────────────────────────
    def ready(self) -> BP:
        if self._ready is None:
            self._ready = self.make_ready()
        return self._ready.copy()

    def make_ready(self) -> BP:
        raise NotImplementedError

    def carry(self, bp: BP, sw: float, ph: float) -> BP:
        """Walk arms + weapons from the swing (``sw`` +1 = left foot forward)."""
        return bp

    def attack_keys(self) -> list:
        """[(ms, BP, ease)] after READY at 0 (READY is appended at the end)."""
        raise NotImplementedError

    def death_items(self) -> dict:
        """Weapon aims of the death keys: {'hit': {...}, 'kneel': {...}, 'topple': {...}, 'down': {...}}."""
        return {}

    # ── helpers ─────────────────────────────────────────────────────────────────────────────────────────
    def side_pt(self, s: str, out: float, fwd: float, up: float) -> Vector:
        """A point relative to the shoulder of side ``s``: ``out`` away from the body, ``fwd``, ``up`` (metres)."""
        sg = 1.0 if s == "l" else -1.0
        return self.shoulder(s) + Vector((sg * out, -fwd, up))

    def wkw(self, theta: float, lat: float = 0.0, roll: float = 0.0, key: str = "wpn") -> dict:
        d, e = C.blade(theta, lat=lat, roll=roll)
        return {key: d, f"{key}_up": e}

    # ── Idle ────────────────────────────────────────────────────────────────────────────────────────────
    def idle_layer(self, bp: BP, t: float, ph: float) -> BP:
        """Per-monster idle detail (weapon sway …)."""
        return bp

    def idle(self, name: str = "Idle"):
        R0 = self.ready()
        L = self.IDLE_MS
        U = self.U

        def s(t_ms: float) -> BP:
            t = (t_ms / L) % 1.0
            ph = 2 * math.pi * t
            b = math.sin(ph)
            kw = dict(pelvis=R0.pelvis + V(0, 0, b * 0.6 * U), lean=R0.lean - b * 1.0,
                      head_yaw=R0.head_yaw + 4.0 * math.sin(ph + 0.4), head_pitch=R0.head_pitch + math.sin(ph - 0.6) * 1.5,
                      flow=R0.flow + b * 0.04, wave=ph)
            for side in ("l", "r"):
                h = getattr(R0, f"hand_{side}")
                if h is not None:
                    kw[f"hand_{side}"] = h + V(0, 0, (math.sin(ph - (0.5 if side == "l" else 0.0)) * 0.5) * U)
            return self.idle_layer(R0.but(**kw), t, ph)
        c = self.proc_clip(name, L, s, loop=True)
        c.key_poses_ms = [0.0, L * 0.25, L * 0.5, L * 0.75]
        return c

    # ── Walk ────────────────────────────────────────────────────────────────────────────────────────────
    def walk(self, name: str = "Walk"):
        J = self.J
        R0 = self.ready()
        w = self.WALK
        L, speed = self.WALK_MS, self.REF_SPEED

        def s(t_ms: float) -> BP:
            t = (t_ms / L) % 1.0
            ((fl, pl, tl), (fr, pr, tr)), dz, swing = self.gait_feet(t, speed, L, w["duty"], w["lift"], w["bob"])
            ph = 2 * math.pi * t
            sw = max(-1.0, min(1.0, swing))
            st = w["stance"]
            bp = R0.but(
                pelvis=V(0, 0.015, w["crouch"] + dz), hip_pitch=w["hip_pitch"], hip_yaw=-6.0 * sw,
                hip_roll=-2.5 * math.sin(ph), lean=w["lean"] + 1.2 * abs(math.sin(ph)), twist=6.0 * sw,
                side=0.0, head_pitch=R0.head_pitch - 0.3 * w["lean"], head_yaw=-2.5 * sw,
                foot_l=J["ankle_l"] + V(st - J["ankle_l"].x, 0, 0) + fl,
                foot_r=J["ankle_r"] + V(-(st + J["ankle_r"].x), 0, 0) + fr,
                foot_l_rot=(pl, 0, 7), foot_r_rot=(pr, 0, 7), toe_l=tl, toe_r=tr,
                knee_l=V(0.15, 1, 0), knee_r=V(-0.15, 1, 0), flow=0.2 + 0.08 * math.sin(2 * ph), wave=2 * ph)
            return self.carry(bp, sw, ph)
        c = self.proc_clip(name, L, s, loop=True, notifies=anim.run_contacts_ms(L), ref_speed=speed)
        c.key_poses_ms = [round(L * i / 8, 1) for i in range(8)]
        return c

    # ── Attack01 ────────────────────────────────────────────────────────────────────────────────────────
    def attack(self, name: str = "Attack01"):
        R0 = self.ready()
        L, Cm = self.ATTACK_MS, self.CONTACT_MS
        keys = [K(0.0, R0)]
        for ms, bp, ease in self.attack_keys():
            keys.append(K(ms / L, bp, ease))
        keys.append(K(1.0, R0, "smooth"))
        nt = {"WindupEnd": round(self.WINDUP_FRAC * Cm, 1), "Contact": Cm}
        c = self.clip(name, L, keys, L, notifies=nt)
        c.key_poses_ms = sorted({0.0, *[round(k.t * L, 1) for k in keys], L})
        return c

    # ── Hurt ────────────────────────────────────────────────────────────────────────────────────────────
    def recoil(self) -> BP:
        R0 = self.ready()
        return R0.but(lean=R0.lean - 13.0, head_pitch=R0.head_pitch - 12.0, head_yaw=R0.head_yaw + 8.0,
                      pelvis=R0.pelvis + V(0, -0.03, -0.012), twist=R0.twist - 6.0,
                      hand_l=R0.hand_l + V(0.04, -0.03, 0.10) if R0.hand_l is not None else None,
                      hand_r=R0.hand_r + V(-0.04, -0.04, 0.09) if R0.hand_r is not None else None,
                      clav_l=(8.0, -4.0), clav_r=(8.0, -4.0), flow=0.35)

    def hurt(self):
        R0 = self.ready()
        rec = self.recoil()
        part = C.lerp_bp(R0, rec, 0.45)
        L = self.HURT_MS
        keys = [K(0.0, R0), K(0.22, rec, "out"), K(0.55, part, "smooth"), K(1.0, R0, "smooth")]
        c = self.clip("Hurt", L, keys, L)
        c.key_poses_ms = [0.0, L * 0.22, L * 0.55, L]
        return c

    # ── Death ───────────────────────────────────────────────────────────────────────────────────────────
    floor_items: tuple = ()          # (object, bone) rigid attachments the fallen body rests on (hunt variants)

    def floor_points(self):
        """``C.SkinPoints`` of ``floor_items`` (every 2nd vertex), or None: the Death post lifts the lying body so
        they stay on the floor (Dreck's tag chain, Harke's gorget under the chest)."""
        if not self.floor_items:
            return None
        if getattr(self, "_floor_pts", None) is None:
            sp = C.SkinPoints(self.rig)
            for obj, bone in self.floor_items:
                # the toon surface only (slot 0): the baked ink hull stands 1.5 cm proud and would float the body
                me = obj.data
                mi = np.zeros(len(me.polygons), np.int32)
                me.polygons.foreach_get("material_index", mi)
                keep = sorted({v for f in me.polygons if mi[f.index] == 0 for v in f.vertices})
                co = np.array([tuple(me.vertices[i].co) for i in keep], np.float64)
                rest = np.array(self.rig.obj.data.bones[bone].matrix_local)
                co = (np.c_[co, np.ones(len(co))] @ rest.T)[:, :3]
                idx = np.zeros((len(co), 4), np.int64)
                idx[:, 0] = sp.bidx[bone]
                w = np.zeros((len(co), 4))
                w[:, 0] = 1.0
                sp._append(co, idx, w, [obj.name] * len(co))
            self._floor_pts = sp
        return self._floor_pts

    DEATH_ROLL = 0.0                 # deg the fallen body rolls about its long axis (− = the left side up)
    COAT_FLOOR_MARGIN = 0.006        # face down the coat's sampled points kept this far over the floor

    def fall_spin(self, pitch: float):
        """(spin deg, axis) of the forward fall pitched ``pitch`` deg, with the share ``pitch / 86`` of
        ``DEATH_ROLL`` about the (world) long axis after it (the drummer rolls onto his right side: the drum shows)."""
        X = Vector((1, 0, 0))
        if not self.DEATH_ROLL:
            return pitch, X
        q = Quaternion(Vector((0, 1, 0)), math.radians(self.DEATH_ROLL * pitch / 86.0)) @ \
            Quaternion(X, math.radians(pitch))
        ax, ang = q.to_axis_angle()
        return math.degrees(ang), Vector(ax)

    def death(self):
        """Hit → the knees buckle → folds forward over the knees → face down, the weapon flat beside the body.
        The NPC fall (``NpcAnims.death``: skirt drape face down, the floor post) with the monster's weapon."""
        R0 = self.ready()
        J = self.J
        kd = 0.32 * self.F.D.hip_z / 0.81          # the kneel's drop (r0 0.32 m on the folk legs; the Unburied's longer)
        it = self.death_items()
        hit = R0.but(**{**dict(lean=R0.lean - 14.0, head_pitch=-16.0, pelvis=R0.pelvis + V(0, -0.03, 0.0), flow=0.35,
                               hand_l=R0.hand_l + V(0.06, -0.02, 0.12), hand_r=R0.hand_r + V(-0.06, -0.02, 0.12)),
                        **it.get("hit", {})})
        kneel = R0.but(**{**dict(
            pelvis=V(0, 0.06, -kd), lean=26.0, hip_pitch=10.0, head_pitch=8.0, twist=0.0, hip_yaw=0.0,
            foot_l=J["ankle_l"] + V(0.03, -0.12, 0.0), foot_r=J["ankle_r"] + V(-0.03, -0.08, 0.0),
            foot_l_rot=(55.0, 0, 10), foot_r_rot=(50.0, 0, 8), knee_l=V(0.2, 1, -0.4), knee_r=V(-0.2, 1, -0.4),
            hand_l=J["hip_l"] + V(0.20, 0.30, -0.05), hand_r=J["hip_r"] + V(-0.22, 0.24, 0.0),
            elbow_l=V(0.6, -0.8, -0.2), elbow_r=V(-0.6, -0.8, -0.2), flow=0.25), **it.get("kneel", {})})
        sp44, ax44 = self.fall_spin(44.0)
        sp86, ax86 = self.fall_spin(86.0)
        topple = kneel.but(**{**dict(
            spin=sp44, spin_axis=ax44, pivot=V(0, 0.12, 0.10), pelvis=V(0, 0.03, -0.17), lean=13.0,
            hip_pitch=3.0, head_pitch=-18.0, head_yaw=-14.0,
            foot_l=J["ankle_l"] + V(0.03, -0.07, 0.03), foot_r=J["ankle_r"] + V(-0.03, -0.05, 0.03),
            foot_l_rot=(58.0, 0, 8), foot_r_rot=(54.0, 0, 6),
            hand_l=V(0.30, 0.48, 0.92), hand_r=V(-0.34, 0.42, 0.92), elbow_l=V(0.9, -0.2, -0.2),
            elbow_r=V(-0.9, -0.2, -0.2), flow=0.35), **it.get("topple", {})})
        down = R0.but(**{**dict(
            spin=sp86, spin_axis=ax86, pivot=V(0, 0.16, 0.08), pelvis=V(0, 0.0, -0.01), lean=8.0,
            hip_pitch=0.0, twist=0.0, hip_yaw=0.0, side=0.0, head_pitch=-34.0, head_yaw=-28.0, head_roll=0.0,
            foot_l=J["ankle_l"] + V(0.035, -0.02, 0.0), foot_r=J["ankle_r"] + V(-0.035, -0.02, 0.0),
            foot_l_rot=(62.0, 0, 6), foot_r_rot=(58.0, 0, 4), knee_l=V(0.15, 1, 0), knee_r=V(-0.15, 1, 0),
            hand_l=V(0.30, 0.10, 1.50), hand_r=V(-0.44, 0.04, 1.02), elbow_l=V(0.9, 0.3, -0.2),
            elbow_r=V(-0.9, 0.2, 0.0), flow=0.1), **it.get("down", {})})
        L = self.DEATH_MS
        keys = [K(0.0, R0), K(0.12, hit, "out"), K(0.38, kneel, "in"), K(0.60, topple, "smooth"),
                K(0.84, down, "smooth"), K(1.0, down)]
        up_only = self.grounded(snap=False)
        snap = self.grounded(snap=True, pivot_pelvis=False)

        def post(bp: BP, t: float) -> BP:
            w = HB.smooth01((abs(bp.spin) - 8.0) / 26.0)
            a = up_only(bp, t)
            if w <= 0.0:
                return a
            b = snap(bp, t)
            out = bp.but(lift=a.lift + (b.lift - a.lift) * w)
            fp = self.floor_points()
            if w > 0.5 and self.skin() is not None and (self.F.skirted or fp is not None):
                pz = C.to_pose(self.rig, out, self.cloth, grips=self.GRIPS)
                mats = anim.evaluate(pz, return_matrices=True)
                low = 0.0
                if self.F.skirted:
                    low = min(low, float(self.coat_pts().posed(mats)[:, 2].min()) - self.COAT_FLOOR_MARGIN)
                if fp is not None:          # chest / pack attachments (the hunt variants) rest on the floor too
                    low = min(low, float(fp.posed(mats)[:, 2].min()) - 0.003)
                if low < 0.0:
                    out = out.but(lift=out.lift - low)
            return out
        c = self.clip("Death", L, keys, L, post=post)
        c.key_poses_ms = [0.0, L * 0.12, L * 0.38, L * 0.60, L * 0.84, L]
        return c

    # ── Stun ────────────────────────────────────────────────────────────────────────────────────────────
    def stun_items(self, bp: BP, ph: float) -> dict:
        return {}

    def stun(self):
        R0 = self.ready()
        U = self.U
        L = self.STUN_MS

        def s(t: float) -> BP:
            ph = 2 * math.pi * t / L
            c, sn = math.cos(ph), math.sin(ph)
            bp = R0.but(pelvis=R0.pelvis + V(0.35 * U * c, 0.6 * U, -1.4 * U + 0.4 * U * sn),
                        lean=R0.lean + 7.0 + 2.0 * sn, side=5.0 * c, head_pitch=R0.head_pitch + 18.0 + 6.0 * sn,
                        head_roll=14.0 * c, head_yaw=10.0 * sn,
                        hand_r=R0.hand_r + V(0.01, -1.5 * U, -3.0 * U + 0.5 * U * c) if R0.hand_r is not None else None,
                        hand_l=R0.hand_l + V(0.0, -2.0 * U, -3.0 * U + 0.6 * U * sn) if R0.hand_l is not None else None,
                        knee_l=V(0.4, 1, 0), knee_r=V(-0.4, 1, 0), clav_l=(-4.0, 0.0), clav_r=(-4.0, 0.0),
                        flow=0.15, wave=ph, sway=0.4 * c)
            return bp.but(**self.stun_items(bp, ph))
        c = self.proc_clip("Stun", L, s, loop=True)
        c.key_poses_ms = [0.0, L * 0.25, L * 0.5, L * 0.75]
        return c

    def monster_clips(self) -> list:
        return [self.idle(), self.walk(), self.attack(), self.hurt(), self.death(), self.stun()]

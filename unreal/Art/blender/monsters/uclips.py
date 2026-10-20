"""The Unburied clip library (monsters.md §2.2 "Shared clips"; ``mclips.GoblinAnims`` adapted to human proportions).

Poses are authored on the hero ``BP`` body pose (``heroes/common.py``) in metres (body space: X left, −Y forward, Z up;
``V(lat, fwd, up)``) and solved by ``common.to_pose`` — the goblin machinery (``GoblinAnims``: keyed clips with the
web easing, no-slide gait, foot planting and whole-body grounding on the skinned meshes) with:

* the **hunched rest** in READY (lean 12°, the head counter-pitched so the face stays on the hero, tilted 15° like a
  broken neck, one shoulder dropped) — the bind pose stays upright;
* the **tabard panels** (``tabard_f/b`` chains) hung from the belt and turned out just enough to clear the posed legs,
  knee pads, boots and mail skirt: the envelope is solved against a vertex sample of the real skinned meshes
  (``bind_meshes``), not spheres;
* clips (lengths in ms; contacts from monsters.md §2.2): ``Idle`` (slow breathing sway, a twitch) · ``Idle_Order``
  (the member's last order, a mime: a one-shot idle variant from READY back to READY) · ``Walk`` (a dragging right
  foot: the toe scrapes, a limp; FootL / FootR) · ``Attack01`` (WindupEnd = 0.62 × Contact, the core's wind-up share,
  ``anim_timing.attackPhases``; Contact per member) · ``Hurt`` · ``Death`` (folds forward onto the knees and down: the
  husk pose U3 crumbles) · ``Rise`` (Death reversed and retimed: raises, spawns, the Pit's dormant footmen) · ``Stun``.
"""
from __future__ import annotations

import json
import math
from typing import Callable

import numpy as np
from mathutils import Matrix, Vector

from kit import anim
from kit.anim import Pose

import common as C
import hero_base as HB
from common import BP, K, V
from mclips import GoblinAnims

WINDUP_OF_CONTACT = 0.62          # Data/anim_timing.json attackPhases.windupOfContact (the telegraph tint ends there)


class UnburiedAnims(GoblinAnims):
    """Clip factory for one Unburied mesh (``kit`` = :class:`unburied.UnburiedKit`).

    ``contact_ms``: Attack01's Contact (monsters.md §2.2); ``attack_ms``: the clip length; ``keys``:
    ``fn(A) → dict`` of the member's poses (``wind``, ``hold``, ``hit``, ``follow``; optional ``recover``,
    ``recoil``)."""

    FLAP_CLEAR = 0.010
    PANEL_HALF = 0.15            # tabard panel half width (envelope obstacles outside it are ignored)

    def __init__(self, kit, ref_speed: float, walk: dict, ready_fn: Callable, contact_ms: float,
                 attack_ms: float, keys: Callable, extra_cloth=None, grips=("r",)):
        super().__init__(kit, ref_speed, walk, ready_fn, extra_cloth=extra_cloth, style="custom", contact_wpn=0.0,
                         web_ready={})
        self.contact_ms = float(contact_ms)
        self.windup_ms = round(WINDUP_OF_CONTACT * self.contact_ms, 1)
        self.attack_ms = float(attack_ms)
        self.keys_fn = keys
        self.grips = tuple(grips)
        self.env = None
        self.panel_bones = {nm: [b for b in ("_01", "_02")] for nm in ("tabard_f", "tabard_b")}

    # ── helpers ────────────────────────────────────────────────────────────────────────────────────────
    def joint(self, name: str) -> Vector:
        return self.J[name].copy()

    def solve(self, bp: BP) -> Pose:
        return C.to_pose(self.rig, bp, self.cloth, grips=self.grips)

    def mats(self, bp: BP, cloth: bool = False) -> dict:
        p = C.to_pose(self.rig, bp, self.cloth if cloth else None, grips=self.grips)
        return anim.evaluate(p, return_matrices=True)

    def item_point(self, bp: BP, bone: str, local) -> Vector:
        """Posed point on an item (``weapon_r`` / ``weapon_l`` bone frame = the item's mesh frame)."""
        m = self.mats(bp)[bone]
        return m @ Vector(local)

    def grip_offset(self, side: str) -> Vector:
        """Wrist − grip in the rest pose, in the weapon bone's frame (for a hand placed round a haft point)."""
        rb = self.rig.obj.data.bones[f"weapon_{side}"]
        w = self.J[f"wrist_{side}"]
        return rb.matrix_local.inverted().to_3x3() @ (w - rb.head_local)

    def second_hand(self, bp: BP, y_along: float, lat: float = 0.0, up: float = 0.0, pole=None) -> BP:
        """The left wrist on the right item's haft ``y_along`` metres from the grip (shovel, crossbow stock):
        solved on the pose without the left hand, the wrist placed so the fist closes round that point."""
        m = self.mats(bp.but(hand_l=None))["weapon_r"]
        off = self.grip_offset("r")
        off = Vector((-off.x, off.y, off.z))
        tgt = m @ (Vector((lat, y_along, up)) + off)
        return bp.but(hand_l=tgt, elbow_l=pole if pole is not None else bp.elbow_l)

    # ── cloth: the tabard panels ───────────────────────────────────────────────────────────────────────
    def cloth(self, p: Pose, bp: BP) -> None:
        self.panels(p, bp)
        if self.extra_cloth is not None:
            self.extra_cloth(p, bp, self)

    def bind_meshes(self, body, items: dict | None = None, foot_parts=None) -> None:
        super().bind_meshes(body, items, foot_parts or (lambda n: n.startswith(("boot_", "sole_"))))
        names = json.loads(body["af_parts"])
        pick = lambda n: (n.startswith(("leg_", "knee_", "boot", "mail_skirt", "quiver", "scabbard"))   # noqa: E731
                          and not n.endswith("_hull"))
        self.env = C.SkinPoints(self.rig).add_mesh(body, names, pick, step=3)

    def panels(self, p: Pose, bp: BP) -> None:
        """Tabard skirt panels hanging from the belt (absolute aims: world down before a spin), streaming with the
        flow / sway, each link turned out just enough to clear the posed legs + mail skirt by ``FLAP_CLEAR``."""
        mats = anim.evaluate(p, return_matrices=True)
        pts = None
        if self.env is not None:
            pts = self.env.posed(mats)
        sw = bp.sway
        for nm, sg in (("tabard_f", 1.0), ("tabard_b", -1.0)):
            if f"{nm}_01" not in self.rig.obj.data.bones:
                continue
            base = 3.0 + bp.flow * 24.0
            want = [base * 0.6 + sg * 8.0 * sw, base + sg * 12.0 * sw + 3.0 * math.sin(bp.wave * 2 + sg)]
            T = mats[f"{nm}_01"].translation
            L1 = self.rig.obj.data.bones[f"{nm}_01"].length
            L2 = self.rig.obj.data.bones[f"{nm}_02"].length
            obst = []
            if pts is not None:
                sel = np.abs(pts[:, 0] - T.x) < self.PANEL_HALF
                for q in pts[sel]:
                    obst.append((Vector(q), 0.0))
            a1 = self._flap_env(T, L1, want[0], sg, obst)
            d1 = Vector((0.0, -sg * math.sin(math.radians(a1)), -math.cos(math.radians(a1))))
            M1 = T + d1 * L1
            a2 = self._flap_env(M1, L2 + 0.03, max(want[1], a1 - 22.0), sg, obst)
            for k, a in ((1, a1), (2, a2)):
                r = math.radians(a)
                d = Vector((0.0, -sg * math.sin(r), -math.cos(r)))
                p.aim(f"{nm}_{k:02d}", d, Vector((0.0, math.cos(r), -sg * math.sin(r))))

    def _flap_env(self, T: Vector, L: float, a: float, sg: float, legs) -> float:
        need = a
        clear = self.FLAP_CLEAR
        for c, r in legs:
            rel = c - T
            down = -rel.z
            out = -sg * rel.y
            dist = math.hypot(down, out)
            if down <= 0.0 or dist > L + r + clear:
                continue
            ac = math.degrees(math.atan2(out, down))
            rr = min(0.999, (r + clear) / max(dist, 1e-6))
            need = max(need, ac + math.degrees(math.asin(rr)))
        return min(need, 85.0)

    def plant(self, bp: BP, t: float = 0.0) -> BP:
        """Feet on the floor (ankle targets raised by the deficit); a foot the leg cannot lift any further (a
        dragged, straightened leg) lifts the whole body instead."""
        bp = super().plant(bp, t)
        if self.feet is None:
            return bp
        mats = anim.evaluate(C.to_pose(self.rig, bp, None, grips=self.grips), return_matrices=True)
        low = min(float(pts.posed(mats, sel)[:, 2].min()) for pts, sel in self.feet.values())
        if low < -0.001:
            bp = bp.but(lift=bp.lift - low)
        return bp

    # ── clips ──────────────────────────────────────────────────────────────────────────────────────────
    def clip(self, name, length, keys, span, loop=False, notifies=None, ref_speed=None, layer=None, post=None):
        lay = layer or self.waver(length)
        c = self.body_clip(name, length, C.keyed(keys, span, lay), loop=loop, notifies=notifies,
                           ref_speed=ref_speed, post=post or self.plant)
        c.key_poses_ms = sorted({round(k.t * span, 1) for k in keys})
        return c

    two_hand: tuple | None = None          # (y along the right item, lat, up): the left fist kept on it
    two_hand_clips: tuple = ()             # clips that keep it there every frame (post step)

    def body_clip(self, name, length, sampler, loop=False, notifies=None, ref_speed=None, post=None):
        """``hero_base.hero_clip`` with this mesh's grips (a two-handed crossbow, a strapped shield) and cloth; a
        two-handed item (``two_hand``) keeps the left fist on it in ``two_hand_clips`` (before the feet are planted)."""
        post = post if post is not None else self.plant
        if self.two_hand is not None and name in self.two_hand_clips:
            y, lat, up = self.two_hand
            inner = post

            def post(bp, t, _inner=inner):
                return _inner(self.second_hand(bp, y, lat, up), t)
        return HB.hero_clip(self.rig, name, length, sampler, cloth=self.cloth, loop=loop, notifies=notifies,
                            ref_speed=ref_speed, post=post, grips=self.grips)

    # Idle 1600 ms loop: a slow sway on bent knees, the chest heaving without breath, a twitch of the head at ~60 %
    def idle(self, length: float = 1600.0, extra=None):
        R0 = self.ready()

        def s(t: float) -> BP:
            ph = 2 * math.pi * t / length
            b = math.sin(ph)
            tw = math.exp(-((t / length - 0.62) / 0.035) ** 2)          # the twitch
            bp = R0.but(
                pelvis=R0.pelvis + V(0.006 * math.sin(ph * 0.5), 0.0, 0.008 * b), lean=R0.lean + 1.6 * b,
                side=R0.side + 1.5 * math.sin(ph * 0.5), head_pitch=R0.head_pitch - 2.0 * b + 6.0 * tw,
                head_roll=R0.head_roll + 4.0 * tw, head_yaw=R0.head_yaw + 4.0 * math.sin(ph * 0.5) - 8.0 * tw,
                hand_r=R0.hand_r + V(0.0, 0.0, 0.010 * b), hand_l=R0.hand_l + V(0.0, 0.008 * b, 0.014 * math.sin(ph - 0.6)),
                clav_l=(R0.clav_l[0] + 1.5 * b, R0.clav_l[1]), clav_r=(R0.clav_r[0] + 1.0 * b, R0.clav_r[1]),
                flow=R0.flow + 0.04 * b, wave=ph, sway=0.12 * math.sin(ph + 1.0))
            return extra(bp, t) if extra else bp
        return self.body_clip("Idle", length, s, loop=True)

    # Walk: the dragging right foot (toe scraping, a short low swing), a limp onto the left leg
    def walk(self, name: str = "Walk", extra=None):
        w = self.walk_p
        R0 = self.ready()
        J = self.J
        length = w["cycle"]
        speed = self.ref_speed

        def s(t_ms: float) -> BP:
            t = (t_ms / length) % 1.0
            fl, fr, dz, swing = anim.run_gait(t, speed, length, duty=w["duty"], lift=w["lift"], bob=w["bob"])
            # the right foot drags: a low swing (its toe scraping), the foot pitched toe-down through the swing
            pr = (t + 0.5) % 1.0
            drag = pr >= w["duty"]
            sr = (pr - w["duty"]) / (1.0 - w["duty"]) if drag else 0.0
            fr = Vector((fr.x, fr.y, fr.z * w["drag_lift"]))
            r_pitch = (w["drag_pitch"] * math.sin(math.pi * sr) if drag else 0.0) + w["drag_pitch"] * 0.25
            # left foot: a normal heel-toe step
            pl = t % 1.0
            l_pitch = 0.0
            if pl >= w["duty"]:
                sl = (pl - w["duty"]) / (1.0 - w["duty"])
                l_pitch = 18.0 * (1 - sl) ** 2 - 8.0 * max(0.0, sl - 0.8) / 0.2
            sw = max(-1.0, min(1.0, swing))
            # the limp: the pelvis sinks and rolls onto the right (dragging) leg while it bears the weight
            on_r = max(0.0, math.sin(2 * math.pi * (t - 0.5))) if (t >= 0.5) else 0.0
            ph = 2 * math.pi * t
            bp = R0.but(
                pelvis=R0.pelvis + V(0.012 * math.sin(ph), 0.0, dz - w["limp"] * on_r),
                hip_roll=-3.5 * math.sin(ph) + 4.0 * on_r, hip_yaw=R0.hip_yaw - 6.0 * sw,
                lean=R0.lean + w["lean"] + 2.0 * abs(math.sin(ph)), side=R0.side + 3.0 * on_r,
                twist=R0.twist + w["twist"] * sw, head_pitch=R0.head_pitch - w["lean"] * 0.7,
                head_yaw=R0.head_yaw - 3.0 * sw, head_roll=R0.head_roll - 3.0 * on_r,
                foot_l=J["ankle_l"] + V(w["stance"], 0, 0) + fl, foot_r=J["ankle_r"] - V(w["stance"], 0, 0) + fr,
                foot_l_rot=(l_pitch, 0, 6), foot_r_rot=(r_pitch, 0, -6),
                knee_l=V(0.15, 1, 0), knee_r=V(-0.15, 1, 0),
                hand_r=R0.hand_r + V(0, w["arm"] * 0.4 * sw, 0.01 * abs(sw)),
                hand_l=R0.hand_l + V(0, -w["arm"] * sw, 0.012 * abs(sw)),
                flow=R0.flow + 0.2 + 0.05 * math.sin(2 * ph), wave=2 * ph, sway=0.25 * math.sin(ph))
            return extra(bp, t, sw) if extra else bp
        notif = anim.run_contacts_ms(length)
        return self.body_clip(name, length, s, loop=True, notifies=notif, ref_speed=speed)

    # Attack01: READY → lift → hold (WindupEnd) → strike = Contact → follow → recover
    def attack(self, name: str = "Attack01", length: float | None = None, notifies: dict | None = None,
               recover: BP | None = None):
        R0 = self.ready()
        k = self.keys_fn(self)
        L = length or self.attack_ms
        c_ms, w_ms = self.contact_ms, self.windup_ms
        lift_ms = k.get("lift_ms", 0.55 * w_ms)
        keys = [K(0.0, R0)]
        if "pre" in k:
            keys.append(K(k.get("pre_ms", 0.45 * lift_ms) / L, k["pre"], "out"))
        keys += [K(lift_ms / L, k["wind"], "smooth" if "pre" in k else "out"), K(w_ms / L, k["hold"], "smooth"),
                K(c_ms / L, k["hit"], "in"), K(min(L - 120.0, c_ms + k.get("follow_ms", 110.0)) / L, k["follow"], "out")]
        if "recover" in k:
            keys.append(K(min(L - 40.0, c_ms + k.get("recover_ms", 260.0)) / L, k["recover"], "smooth"))
        keys.append(K(1.0, recover or R0, "smooth"))
        nt = {"WindupEnd": w_ms, "Contact": c_ms}
        nt.update(notifies or {})
        c = self.clip(name, L, keys, L, notifies=nt)
        c.key_poses_ms = sorted({round(kk.t * L, 1) for kk in keys})
        return c

    # Hurt 320 ms: they barely flinch — a stagger back, the head snapping on the broken neck
    def recoil(self) -> BP:
        k = self.keys_fn(self)
        if "recoil" in k:
            return k["recoil"]
        R0 = self.ready()
        return R0.but(pelvis=R0.pelvis + V(0, -0.035, -0.01), lean=R0.lean - 12.0, head_pitch=R0.head_pitch - 18.0,
                      head_roll=R0.head_roll + 10.0, twist=R0.twist - 6.0, hand_r=R0.hand_r + V(0.0, -0.06, 0.06),
                      hand_l=R0.hand_l + V(0.02, -0.08, 0.10), clav_l=(8.0, 0.0), clav_r=(8.0, 0.0), flow=0.45)

    def hurt(self, length: float = 320.0):
        R0 = self.ready()
        rec = self.recoil()
        part = rec.but(pelvis=R0.pelvis.lerp(rec.pelvis, 0.4), lean=R0.lean - 4.0, head_pitch=R0.head_pitch - 6.0,
                       head_roll=R0.head_roll + 4.0, hand_r=R0.hand_r.lerp(rec.hand_r, 0.4),
                       hand_l=R0.hand_l.lerp(rec.hand_l, 0.4), clav_l=(3.0, 0.0), clav_r=(3.0, 0.0), flow=0.25)
        keys = [K(0.0, R0), K(0.22, rec, "out"), K(0.6, part, "smooth"), K(1.0, R0, "smooth")]
        c = self.clip("Hurt", length, keys, length)
        c.key_poses_ms = [0.0, round(0.22 * length, 1), round(0.6 * length, 1), length]
        return c

    # Death 900 ms: the knees give, he kneels, the torso folds forward over the knees onto the ground (the husk)
    def death_keys(self) -> list:
        k = self.keys_fn(self)
        if "death" in k:
            return k["death"]
        return default_death(self)

    def death(self, length: float = 900.0):
        rec, buckle, kneel, fold = self.death_keys()
        R0 = self.ready()
        keys = [K(0.0, R0), K(0.10, rec, "out"), K(0.34, buckle, "in"), K(0.60, kneel, "smooth"),
                K(1.0, fold, "out")]
        c = self.clip("Death", length, keys, length, post=self.grounded(snap=True, pivot_pelvis=False))
        c.key_poses_ms = [0.0, 0.10 * length, 0.34 * length, 0.60 * length, length]
        return c

    # Rise 1200 ms: Death reversed and retimed — a puppet's pull: the torso unfolds first, the head comes up last
    def rise(self, length: float = 1200.0):
        rec, buckle, kneel, fold = self.death_keys()
        R0 = self.ready()
        lurch = kneel.but(lean=kneel.lean - 10.0, head_pitch=kneel.head_pitch - 6.0, head_roll=kneel.head_roll + 4.0)
        keys = [K(0.0, fold), K(0.12, fold.but(lean=fold.lean - 4.0), "smooth"), K(0.42, lurch, "out"),
                K(0.70, buckle.but(head_pitch=buckle.head_pitch + 4.0), "in"),
                K(0.86, R0.but(head_pitch=R0.head_pitch + 14.0, head_roll=R0.head_roll + 6.0), "out"),
                K(1.0, R0, "smooth")]
        c = self.clip("Rise", length, keys, length, post=self.grounded(snap=False, pivot_pelvis=False),
                      notifies={"RiseUp": round(0.70 * length, 1)})
        c.key_poses_ms = [0.0, 0.12 * length, 0.42 * length, 0.70 * length, 0.86 * length, length]
        c.manifest_extra = {"usage": "raises (Corpseherd, standard-bearer, Valdras P2), spawns, the Pit's dormant "
                                     "footmen; Death reversed and retimed (monsters.md §2.2)"}
        return c

    # Stun 700 ms loop: swaying on the spot, the head lolling on the broken neck, the weapon drooping
    def stun(self, length: float = 700.0, droop: float = -8.0, **_):
        R0 = self.ready()

        def s(t: float) -> BP:
            ph = 2 * math.pi * t / length
            c, sn = math.cos(ph), math.sin(ph)
            th = math.degrees(math.atan2(-R0.wpn.y, R0.wpn.z)) + droop + 5.0 * sn
            lat = R0.wpn.x / max(1e-6, math.hypot(R0.wpn.y, R0.wpn.z)) - 0.12
            wd, wu = C.blade(th, lat=lat)
            return R0.but(
                pelvis=R0.pelvis + V(0.012 * c, 0.01, -0.035 + 0.01 * sn), lean=R0.lean + 6.0 + 2.0 * sn,
                side=R0.side + 5.0 * c, head_pitch=R0.head_pitch + 10.0 + 5.0 * sn, head_roll=R0.head_roll + 14.0 * c,
                head_yaw=R0.head_yaw + 9.0 * sn, hand_r=R0.hand_r + V(-0.04, 0.03, -0.02 + 0.015 * c), wpn=wd, wpn_up=wu,
                hand_l=R0.hand_l + V(0.0, -0.04, -0.09 + 0.02 * sn), clav_l=(-6.0, 0.0), clav_r=(-4.0, 0.0),
                flow=0.15, wave=ph, sway=0.4 * c)
        return self.body_clip("Stun", length, s, loop=True)

    def one_shot(self, name: str, length: float, keys: list, notifies: dict | None = None, usage: str = "",
                 post=None):
        """A keyed one-shot from READY back to READY (Idle_Order mimes, Cast01)."""
        c = self.clip(name, length, keys, length, notifies=notifies, post=post)
        if usage:
            c.manifest_extra = {"usage": usage}
        return c


def default_death(A: UnburiedAnims) -> list:
    """Recoil → knees buckle → kneel (knees on the ground, torso upright-ish) → folded face-down over the knees."""
    R0 = A.ready()
    J = A.J
    rec = A.recoil()
    sh = A.rig.spec
    knee_h = 0.06
    buckle = rec.but(pelvis=V(0, -0.02, -0.22), lean=6.0, hip_pitch=6.0, head_pitch=6.0, head_roll=R0.head_roll + 6.0,
                     head_yaw=8.0, foot_l=J["ankle_l"] + V(0.0, 0.06, 0), foot_r=J["ankle_r"] + V(0.0, -0.10, 0),
                     knee_l=V(0.25, 1, 0), knee_r=V(-0.25, 1, 0),
                     hand_r=R0.hand_r + V(-0.04, 0.02, -0.14), hand_l=R0.hand_l + V(0.04, -0.05, -0.16),
                     clav_l=(-6.0, 0.0), clav_r=(-6.0, 0.0), flow=0.5)
    # kneel: thighs near vertical, shins on the floor behind (ankles back, soles up)
    hz = knee_h + sh.thigh * 0.94
    P0 = J["pelvis"]
    kneel = buckle.but(pelvis=V(0, -0.06, hz + sh.pelvis_offset - P0.z),
                       lean=18.0, hip_pitch=8.0, head_pitch=8.0, head_roll=R0.head_roll + 6.0, head_yaw=16.0,
                       foot_l=V(J["ankle_l"].x + 0.02, -0.40, 0.11), foot_r=V(J["ankle_r"].x - 0.02, -0.36, 0.11),
                       foot_l_rot=(75.0, 0, 6), foot_r_rot=(75.0, 0, -6), knee_l=V(0.1, 1, -0.3),
                       knee_r=V(-0.1, 1, -0.3), hand_r=V(-0.30, 0.18, 0.30), hand_l=V(0.26, 0.20, 0.28),
                       elbow_r=V(-0.8, 0.2, -0.4), elbow_l=V(0.8, 0.2, -0.4), flow=0.3)
    fold = kneel.but(pelvis=kneel.pelvis + V(0, 0.06, -0.08), lean=58.0, hip_pitch=34.0, head_pitch=-14.0,
                     head_roll=R0.head_roll + 4.0, head_yaw=30.0,
                     hand_r=V(-0.34, 0.42, 0.06), hand_l=V(0.30, 0.44, 0.06), elbow_r=V(-0.9, 0.1, 0.2),
                     elbow_l=V(0.9, 0.1, 0.2), clav_l=(-10.0, 0.0), clav_r=(-10.0, 0.0), flow=0.05)
    k = A.keys_fn(A)
    for nm, bp in (("buckle", buckle), ("kneel", kneel), ("fold", fold)):
        for item in ("wpn", "off"):             # the held items laid down (a sword flat on the ground, a shield …)
            d = k.get(f"{nm}_{item}")
            if d is not None:
                d = Vector(d).normalized()
                up = k.get(f"{nm}_{item}_up") or Vector((d.y, -d.x, 0.0))
                bp.set(**{item: d, f"{item}_up": Vector(up).normalized()})
        for key in ("hand_l", "hand_r", "elbow_l", "elbow_r"):
            v = k.get(f"{nm}_{key}")
            if v is not None:
                bp.set(**{key: v})
    fix = k.get("death_fix")                # a member's last word on the kneel / fold poses (a levelled crossbow)
    if fix is not None:
        buckle, kneel, fold = fix(buckle), fix(kneel), fix(fold)
    return [rec, buckle, kneel, fold]

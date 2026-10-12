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
from typing import Callable, Sequence

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
CENTER_X, GROUND_Y = 48.0, 91.0


def deg(rad: float) -> float:
    return math.degrees(rad)


class GoblinAnims:
    """Clip factory for one goblin-family mesh (``kit`` = its :class:`goblins.GoblinKit`).

    ``style``: web attack style (``thrust`` goblin, ``overhead`` chief, ``cast`` shaman); ``contact_wpn``: weapon
    angle at the contact (deg from vertical, + forward); ``reach_u``: web upper arm + forearm (units)."""

    def __init__(self, kit, ref_speed: float, walk: dict, ready_fn: Callable[["GoblinAnims"], BP],
                 extra_cloth: Callable[[Pose, BP, "GoblinAnims"], None] | None = None, style: str = "thrust",
                 contact_wpn: float = 86.0, death_dir: int = -1, web_ready: dict | None = None):
        self.kit = kit
        self.rig = kit.rig
        self.J = kit.J
        self.U = kit.U
        self.ref_speed = ref_speed
        self.walk_p = walk
        self.extra_cloth = extra_cloth
        self.style = style
        self.contact_wpn = contact_wpn
        self.death_dir = death_dir
        self.web_ready = web_ready or {}
        self.AZ = self.J["ankle_l"].z
        self.leg = self.rig.spec.thigh + self.rig.spec.shin
        self.reach_u = kit.w.upper_arm + kit.w.fore_arm
        self._ready_fn = ready_fn
        self._ready = None
        self.feet = None
        self.ground_pts = None
        self.item_probes: list = []          # (bone, item-local point, radius) for the ground clamp
        self.overrides: dict[str, Callable[[], object]] = {}

    # ── web → body space ───────────────────────────────────────────────────────────────────────────────
    def web_pelvis(self, x: float, y: float, lat: float = 0.0) -> Vector:
        return V(lat, (x - CENTER_X) * self.U, (GROUND_Y - y) * self.U - self.J["pelvis"].z)

    def web_foot(self, x: float, lat: float, y: float = GROUND_Y) -> Vector:
        return V(lat, (x - CENTER_X) * self.U, self.AZ + (GROUND_Y - y) * self.U)

    def web_hand(self, x: float, y: float, lat: float) -> Vector:
        return V(lat, (x - CENTER_X) * self.U, (GROUND_Y - y) * self.U)

    def ready(self) -> BP:
        if self._ready is None:
            self._ready = self._ready_fn(self)
        return self._ready.copy()

    def shoulder(self, s: str = "r", bp: BP | None = None) -> Vector:
        """Posed shoulder joint (body space) of ``bp`` (default READY)."""
        p = C.to_pose(self.rig, bp or self.ready(), None)
        return anim.evaluate(p, return_matrices=True)[f"upperarm_{s}"].translation.copy()

    def sh(self, dx: float, dy: float, s: str = "r", lat: float | None = None, bp: BP | None = None) -> Vector:
        """Web ``offset(sk.shN, dx·reach, dy·reach)`` (dy + = down) from the READY shoulder, ``lat`` absolute."""
        S = self.shoulder(s, bp)
        r = self.reach_u * self.U
        out = S + V(0.0, dx * r, -dy * r)
        if lat is not None:
            out.x = lat
        return out

    def pose(self, base: BP, **kw) -> BP:
        return base.but(**kw)

    # ── cloth / secondary (ears, loincloth flaps, + the monster's own chains) ───────────────────────────
    def cloth(self, p: Pose, bp: BP) -> None:
        flick = math.sin(bp.wave + 1.0) * 0.6 + bp.flow * 1.5
        for s, sg in (("l", 1), ("r", -1)):
            # web flick: the tip swings back / up with the flow (running, recoil) and a sine
            p.rot(f"ear_{s}_01", pitch=-2.0 * flick, yaw=sg * (-3.0 * flick))
            p.rot(f"ear_{s}_02", pitch=-3.0 * flick + 2.0 * math.sin(bp.wave * 2.0 + sg),
                  yaw=sg * (-2.5 * flick))
        self.flaps(p, bp)
        if bp.jaw:
            p.rot("jaw", pitch=bp.jaw)
        if self.extra_cloth is not None:
            self.extra_cloth(p, bp, self)

    FLAP_CLEAR = 0.012          # m kept between a flap and the thigh / knee surface in front of / behind it

    def flaps(self, p: Pose, bp: BP, names=(("loin_f", 1.0), ("loin_b", -1.0)), base_deg: float = 6.0,
              flow_deg: float = 28.0, thigh_r: float | None = None, extra: dict | None = None) -> None:
        """Loincloth flaps hanging in the world (aims are absolute; ``apply_spin`` turns them with a falling body):
        the web flow + sway angles, then an **envelope** — each link turns out just enough to stay
        ``FLAP_CLEAR`` in front of (front flap) / behind (back flap) the posed thighs and knees. ``extra``
        ({chain: deg}) turns both links of a chain further out (a solver on the real meshes, e.g. the shaman's
        skirt envelope)."""
        sw = bp.sway
        mats = anim.evaluate(p, return_matrices=True)
        tr = thigh_r if thigh_r is not None else self.U * 2.6
        legs = []
        for s in ("l", "r"):
            H = mats[f"thigh_{s}"].translation
            Kn = mats[f"calf_{s}"].translation
            A = mats[f"foot_{s}"].translation
            for k in range(7):
                legs.append((H.lerp(Kn, k / 6), tr * (1 - 0.25 * k / 6)))
            for k in range(1, 4):
                legs.append((Kn.lerp(A, k / 3), tr * 0.75))
        for nm, sg in names:
            if f"{nm}_01" not in self.rig.obj.data.bones:
                continue
            base = base_deg + bp.flow * flow_deg
            want = [base * 0.7 + sg * 10.0 * sw, base + sg * 14.0 * sw + 4.0 * math.sin(bp.wave * 2 + sg)]
            T = mats[f"{nm}_01"].translation
            L1 = self.rig.obj.data.bones[f"{nm}_01"].length
            L2 = self.rig.obj.data.bones[f"{nm}_02"].length
            a1 = self._flap_env(T, L1, want[0], sg, legs)
            d1 = Vector((0.0, -sg * math.sin(math.radians(a1)), -math.cos(math.radians(a1))))
            M1 = T + d1 * L1
            a2 = self._flap_env(M1, L2, max(want[1], a1 - 25.0), sg, legs)
            if extra and extra.get(nm):
                a1, a2 = min(a1 + extra[nm], 80.0), min(a2 + extra[nm], 85.0)
            for k, a in ((1, a1), (2, a2)):
                # bone +Z rests along +Y (back) for both flaps; swinging the tip by ``a`` turns it with the link
                # (the old up vector rolled the front flap 180°: its dark lining faced forward)
                r = math.radians(a)
                d = Vector((0.0, -sg * math.sin(r), -math.cos(r)))
                p.aim(f"{nm}_{k:02d}", d, Vector((0.0, math.cos(r), -sg * math.sin(r))))

    def _flap_env(self, T: Vector, L: float, a: float, sg: float, legs) -> float:
        """Smallest angle ≥ ``a`` (deg, + = away from the body: forward for the front flap) at which a link from
        ``T`` of length ``L`` clears every leg sphere ``(centre, radius)`` by ``FLAP_CLEAR`` on its side."""
        need = a
        for c, r in legs:
            rel = c - T
            down = -rel.z
            out = -sg * rel.y                          # + = on the flap's side (in front for the front flap)
            dist = math.hypot(down, out)
            if down <= 0.0 or dist > L + r:
                continue
            # angle of the sphere centre from straight down, then the tangent offset so the link passes outside
            ac = math.degrees(math.atan2(out, down))
            rr = min(0.999, (r + self.FLAP_CLEAR) / max(dist, 1e-6))
            need = max(need, ac + math.degrees(math.asin(rr)))
        return min(need, 80.0)

    def solve(self, bp: BP) -> Pose:
        return C.to_pose(self.rig, bp, self.cloth)

    # ── ground contact on the real meshes (heroes/common.SkinPoints) ──────────────────────────────────────
    def bind_meshes(self, body, items: dict | None = None, foot_parts: Callable[[str], bool] | None = None) -> None:
        """Sample the skinned body (+ held items, ``{bone: obj}``) for foot planting and whole-body grounding."""
        import json
        names = json.loads(body["af_parts"])
        fp = foot_parts or (lambda n: n.startswith("foot_") or n.startswith("toe_claw_"))
        self.feet = {}
        for s in ("l", "r"):
            pts = C.SkinPoints(self.rig).add_mesh(body, names, lambda n, s=s: fp(n) and n.endswith(f"_{s}"))
            self.feet[s] = (pts, None)
        g = C.SkinPoints(self.rig).add_mesh(body, names, lambda n: not n.endswith("_hull"), step=1)
        for bone, obj in (items or {}).items():
            g.add_rigid(obj, bone, step=1, tag=bone)
        self.ground_pts = g

    def plant(self, bp: BP, t: float = 0.0) -> BP:
        if self.feet is None:
            return bp
        return C.plant_feet(self.rig, bp, self.feet)

    def grounded(self, snap: bool = False, pivot_pelvis: bool = False):
        """Post step of falls: the whole posed body — **cloth chains included** (pelt, flaps, skirt, feathers;
        ``common.ground_clamp`` poses without the cloth, which left a fallen pelt 2–3 cm under the floor) — and the
        held item kept on the floor; ``snap`` rests the lowest point exactly on it."""
        def post(bp: BP, t: float) -> BP:
            if pivot_pelvis:
                bp = bp.but(pivot=self.J["pelvis"] + bp.pelvis)
            if self.ground_pts is None:
                return bp
            p = C.to_pose(self.rig, bp, self.cloth)
            low = float(self.ground_pts.posed(anim.evaluate(p, return_matrices=True))[:, 2].min())
            if low < 0.0 or snap:
                return bp.but(lift=bp.lift - low)
            return bp
        return post

    def clip(self, name, length, keys, span, loop=False, notifies=None, ref_speed=None, layer=None, post=None):
        lay = layer or self.waver(length)
        c = C.body_clip(self.rig, name, length, C.keyed(keys, span, lay), cloth=self.cloth, loop=loop,
                        notifies=notifies, ref_speed=ref_speed, post=post or self.plant)
        c.key_poses_ms = sorted({round(k.t * span, 1) for k in keys})
        return c

    @staticmethod
    def waver(period_ms: float, phase: float = 0.0):
        def layer(bp: BP, t: float) -> BP:
            ph = 2 * math.pi * t / period_ms + phase
            return bp.but(wave=ph, sway=bp.sway + 0.15 * math.sin(ph))
        return layer

    # ── Idle 667 ms loop (web humanoidMonster idle: bob .6 u, head ±.04 rad, hands, weapon ±.04 rad) ──────
    def idle(self, length: float = 667.0, weapon_wave: float = 2.3, extra: Callable[[BP, float], BP] | None = None):
        R0 = self.ready()
        U = self.U

        def s(t: float) -> BP:
            ph = 2 * math.pi * t / length
            b = math.sin(ph)
            th = self._theta(R0.wpn) + weapon_wave * math.sin(ph - 0.3)
            wd, wu = self._aim(R0, th)
            bp = R0.but(
                pelvis=R0.pelvis + V(0, 0, b * 0.6 * U), lean=R0.lean - b * 1.2,
                head_pitch=R0.head_pitch + deg(math.sin(ph - 0.6) * 0.04), head_yaw=R0.head_yaw + 3.0 * math.sin(ph * 0.5),
                hand_r=R0.hand_r + V(0, 0, b * 0.5 * U), wpn=wd, wpn_up=wu,
                hand_l=R0.hand_l + V(0, 0, math.sin(ph - 0.5) * 0.6 * U),
                clav_l=(math.sin(ph - 0.4) * 2.0, R0.clav_l[1]), clav_r=(math.sin(ph - 0.4) * 2.0, R0.clav_r[1]),
                flow=R0.flow + math.sin(ph) * 0.05, wave=ph, sway=math.sin(ph + 1.0) * 0.15,
                jaw=R0.jaw + 2.0 * max(0.0, math.sin(ph - 1.0)))
            return extra(bp, t) if extra else bp
        return C.body_clip(self.rig, "Idle", length, s, cloth=self.cloth, loop=True, post=self.plant)

    def _theta(self, wdir: Vector) -> float:
        """Weapon direction → web angle from vertical (deg, + forward) in the sagittal plane."""
        return math.degrees(math.atan2(-wdir.y, wdir.z))

    def _aim(self, R0: BP, theta: float, lat: float | None = None) -> tuple[Vector, Vector]:
        lat0 = R0.wpn.x / max(1e-6, math.hypot(R0.wpn.y, R0.wpn.z)) if lat is None else lat
        return C.blade(theta, lat=lat0)

    # ── Walk 600 ms loop @ ref speed, no foot sliding (R9) ──────────────────────────────────────────────────
    def gait_feet(self, t: float, speed: float, cycle: float, duty: float, lift: float, bob: float,
                  peel: float = 34.0):
        fl, fr, dz, swing = anim.run_gait(t, speed, cycle, duty=duty, lift=lift, bob=bob)
        out = []
        for off, ph0 in ((fl, 0.0), (fr, 0.5)):
            p = (t + ph0) % 1.0
            pitch, toe = 0.0, 0.0
            if p < duty:
                s = p / duty
                if s > 0.55:                         # heel peels off about the ball
                    k = (s - 0.55) / 0.45
                    pitch = peel * k * k
                    toe = pitch * 0.8
            else:
                s = (p - duty) / (1 - duty)
                if s < 0.45:
                    pitch = peel * (1 - s / 0.45) ** 1.5
                elif s > 0.8:
                    pitch = -14.0 * (s - 0.8) / 0.2
            raise_ = back = 0.0
            if pitch > 0:
                r = self.rig.spec.foot_len * 0.5
                raise_ = r * math.sin(math.radians(pitch))
                back = r * (1 - math.cos(math.radians(pitch)))
            out.append((off + V(0, -back, raise_ if p < duty else raise_ * 0.6), pitch, toe))
        return out, dz, swing

    def walk(self, name: str = "Walk", extra: Callable[[BP, float, float], BP] | None = None):
        w = self.walk_p
        J = self.J
        R0 = self.ready()
        U = self.U
        length = w["cycle"]
        speed = self.ref_speed

        def s(t_ms: float) -> BP:
            t = (t_ms / length) % 1.0
            ((fl, pl, tl), (fr, pr, tr)), dz, swing = self.gait_feet(t, speed, length, w["duty"], w["lift"], w["bob"])
            ph = 2 * math.pi * t
            sw = max(-1.0, min(1.0, swing))
            th = self._theta(R0.wpn) - 6.0 * sw
            wd, wu = self._aim(R0, th)
            arm = w["arm"]
            bp = R0.but(
                pelvis=R0.pelvis + V(0, 0.01, w["crouch"] + dz), hip_yaw=R0.hip_yaw - 7.0 * sw,
                hip_roll=-3.0 * math.sin(ph), lean=R0.lean + w["lean"] + 2.0 * abs(math.sin(ph)),
                twist=R0.twist + w.get("twist", 9.0) * sw, head_pitch=R0.head_pitch - w["lean"] * 0.6,
                head_yaw=-4.0 * sw,
                foot_l=J["ankle_l"] + V(w.get("stance", 0.0), 0, 0) + fl,
                foot_r=J["ankle_r"] - V(w.get("stance", 0.0), 0, 0) + fr,
                foot_l_rot=(pl, 0, 8), foot_r_rot=(pr, 0, 8), toe_l=tl, toe_r=tr,
                knee_l=V(0.25, 1, 0), knee_r=V(-0.25, 1, 0),
                # counter-swing with the chest turn (+twist = right shoulder forward when the left foot leads)
                hand_r=R0.hand_r + w.get("hand_off", V(0, 0, 0)) + V(0, arm * w.get("hand_swing", 0.5) * sw,
                                                                     0.25 * U * abs(sw)), wpn=wd, wpn_up=wu,
                hand_l=R0.hand_l + V(0, -arm * sw, 0.3 * U * abs(sw) - 0.2 * U),
                clav_l=(1.5 * abs(sw), -3.0 * sw), clav_r=(1.5 * abs(sw), 2.0 * sw),
                flow=R0.flow + 0.25 + 0.06 * math.sin(2 * ph), wave=2 * ph, sway=0.3 * math.sin(ph),
                jaw=R0.jaw + 3.0)
            return extra(bp, t, sw) if extra else bp
        notif = anim.run_contacts_ms(length)
        return C.body_clip(self.rig, name, length, s, cloth=self.cloth, loop=True, notifies=notif, ref_speed=speed,
                           post=self.plant)

    # ── Attack01 500 ms: READY → wind (83) → mid (167) → HIT = Contact (250) → recovery (web keys 0/.33/.67/1) ─
    def attack_keys(self) -> list:
        """Web ``humanoidTracks`` for ``self.style`` → (wind, mid, hit) BPs. Monster scripts may override
        ``self.overrides['attack_keys']``."""
        if "attack_keys" in self.overrides:
            return self.overrides["attack_keys"]()
        R0 = self.ready()
        rx, ry = self.web_ready["root"]
        fn = self.web_ready["footN"]
        lean0 = self.web_ready["lean"]
        U = self.U
        latr = R0.hand_r.x
        if self.style == "thrust":
            wind = self.pose(R0, pelvis=self.web_pelvis(rx - 2.5, ry + 0.8), lean=deg(-0.12), twist=R0.twist - 16.0,
                             hip_yaw=R0.hip_yaw - 8.0, head_yaw=10.0,
                             hand_r=self.sh(-0.40, 0.30, lat=latr - 0.02), elbow_r=V(-0.9, 0.5, -0.4),
                             **self._wkw(R0, 77.0), hand_l=self.sh(0.2, 0.25, "l", lat=R0.hand_l.x),
                             clav_r=(4.0, -6.0), fx=0.4, jaw=8.0)
            mid = self.pose(R0, pelvis=self.web_pelvis(rx + 1.0, ry + 1.0), lean=deg(0.15), twist=R0.twist + 4.0,
                            hand_r=self.sh(0.30, 0.20, lat=latr + 0.01), **self._wkw(R0, 83.0),
                            foot_r=self.web_foot(fn + 3.5, R0.foot_r.x), fx=0.7, jaw=12.0)
            hit = self.pose(R0, pelvis=self.web_pelvis(rx + 3.5, ry + 1.6), lean=deg(0.32), twist=R0.twist + 22.0,
                            hip_yaw=R0.hip_yaw + 10.0, head_pitch=R0.head_pitch - 6.0,
                            hand_r=self.sh(0.98, 0.12, lat=latr * 0.25), elbow_r=V(-0.6, 0.3, -0.7),
                            **self._wkw(R0, self.contact_wpn, lat=0.05),
                            hand_l=self.sh(-0.2, 0.35, "l", lat=R0.hand_l.x + 0.03), elbow_l=V(0.9, 0.3, -0.2),
                            foot_r=self.web_foot(fn + 5.0, R0.foot_r.x), foot_r_rot=(0, 0, 4),
                            clav_r=(-2.0, 10.0), fx=1.0, jaw=16.0, flow=0.3)
            return [self.look(wind), self.look(mid), self.look(hit)]
        raise ValueError(f"no default attack keys for style {self.style!r}")

    @staticmethod
    def look(bp: BP, k: float = 0.8, yaw: float = 0.0) -> BP:
        """The head counter-turns ``k`` of the chest + hip turn (the face stays on the strike line)."""
        return bp.but(head_yaw=-(bp.twist + bp.hip_yaw) * k + yaw)

    def _wkw(self, R0: BP, theta: float, lat: float | None = None, roll: float = 0.0) -> dict:
        lat0 = R0.wpn.x / max(1e-6, math.hypot(R0.wpn.y, R0.wpn.z)) if lat is None else lat
        d, u = C.blade(theta, lat=lat0, roll=roll)
        return {"wpn": d, "wpn_up": u}

    def attack(self, name: str = "Attack01", length: float = 500.0, notifies: dict | None = None,
               recover: BP | None = None):
        R0 = self.ready()
        wind, mid, hit = self.attack_keys()
        follow = self.overrides.get("follow", lambda h: h)(hit)
        back = self.overrides.get("recover", lambda: None)()
        keys = [K(0.0, R0), K(83.3 / 250.0, wind, "out"), K(166.7 / 250.0, mid, "in"), K(1.0, hit, "linear"),
                K(320.0 / 250.0, follow, "out")]
        if back is not None:
            keys.append(K(400.0 / 250.0, back, "smooth"))
        keys.append(K(length / 250.0, recover or R0, "smooth"))
        nt = {"WindupEnd": WINDUP_END_MS, "Contact": CONTACT_MS}
        nt.update(notifies or {})
        c = self.clip(name, length, keys, 250.0, notifies=nt)
        c.key_poses_ms = [0.0, 83.3, 166.7, 250.0, 320.0, length]
        return c

    # ── Hurt 200 ms: recoil (pelvis −3.5 u, lean −19°, head −20°, hands up) → partial recovery ─────────────────
    def recoil(self) -> BP:
        if "recoil" in self.overrides:
            return self.overrides["recoil"]()
        R0 = self.ready()
        rx, ry = self.web_ready["root"]
        U = self.U
        th = self._theta(R0.wpn) - deg(0.4)
        return self.pose(R0, pelvis=self.web_pelvis(rx - 3.5, ry + 1.0), lean=deg(-0.34), head_pitch=deg(-0.35),
                         head_yaw=8.0, twist=R0.twist - 6.0,
                         hand_r=R0.hand_r + V(-0.01, -2.5 * U, 3.0 * U), hand_l=R0.hand_l + V(0.02, -2.0 * U, 4.0 * U),
                         **self._wkw(R0, th), foot_l=R0.foot_l + V(0, -1.5 * U, 0) if R0.foot_l is not None else None,
                         clav_l=(10.0, 0.0), clav_r=(10.0, 0.0), flow=0.45, jaw=14.0)

    def hurt(self, length: float = 200.0):
        R0 = self.ready()
        rec = self.recoil()
        rx, ry = self.web_ready["root"]
        U = self.U
        th = self._theta(R0.wpn) - deg(0.15)
        part = self.pose(rec, pelvis=self.web_pelvis(rx - 1.8, ry + 0.5), lean=deg(-0.14),
                         head_pitch=R0.head_pitch + deg(-0.12), head_yaw=3.0,
                         hand_r=R0.hand_r + V(0, -1.0 * U, 1.0 * U), hand_l=R0.hand_l + V(0, -1.0 * U, 1.5 * U),
                         **self._wkw(R0, th), clav_l=(3.0, 0.0), clav_r=(3.0, 0.0), flow=0.25, jaw=6.0)
        keys = [K(0.0, rec), K(0.5, part, "out"), K(1.0, R0.but(jaw=2.0), "smooth")]
        c = self.clip("Hurt", length, keys, length)
        c.key_poses_ms = [0.0, length * 0.5, length]
        return c

    # ── Death 500 ms: recoil → knees buckle → tipping (spin 54°) → lying on the back (spin 87°) ──────────────
    def death_keys(self) -> list:
        if "death_keys" in self.overrides:
            return self.overrides["death_keys"]()
        R0 = self.ready()
        rec = self.recoil()
        rx, ry = self.web_ready["root"]
        fn, ff = self.web_ready["footN"], self.web_ready["footF"]
        U = self.U
        d = self.death_dir
        P0 = self.J["pelvis"]
        buckle = self.pose(rec, pelvis=self.web_pelvis(rx - 2.5, ry + 7.0), lean=deg(0.2 * d) * 0.5 - 4.0,
                           head_pitch=deg(-0.1) - 10.0, head_yaw=10.0, head_roll=6.0,
                           foot_r=self.web_foot(fn - 1.0, R0.foot_r.x), foot_l=self.web_foot(ff, R0.foot_l.x),
                           knee_l=V(0.5, 1, 0), knee_r=V(-0.5, 1, 0),
                           hand_r=self.web_hand(rx + 5.0, ry + 4.0, R0.hand_r.x - 0.04),
                           hand_l=self.web_hand(rx + 1.0, ry + 2.0, R0.hand_l.x + 0.05),
                           **self._wkw(R0, self._theta(R0.wpn) + 25.0), flow=0.5, jaw=22.0)
        Pt = P0 + self.web_pelvis(rx + 1.5, GROUND_Y - 10.0)
        tip = self.pose(buckle, pelvis=self.web_pelvis(rx + 1.5, GROUND_Y - 10.0), lean=-6.0, head_pitch=-14.0,
                        foot_r=Pt + V(-0.06, -0.10, -0.24), foot_l=Pt + V(0.06, -0.02, -0.27),
                        hand_r=Pt + V(-0.17, -0.06, 0.24), hand_l=Pt + V(0.17, 0.0, 0.26),
                        **self._wkw(R0, self._theta(R0.wpn) + 50.0), spin=d * 54.0, spin_axis=V(1, 0, 0),
                        flow=0.6, jaw=26.0)
        Pl = P0 + self.web_pelvis(rx + 2.0, GROUND_Y - 4.8)
        lying = self.pose(R0, pelvis=self.web_pelvis(rx + 2.0, GROUND_Y - 4.8), lean=-deg(self.web_ready["lean"]) * 0.6,
                          head_pitch=-10.0, head_roll=-24.0, head_yaw=10.0, twist=-4.0, hip_yaw=0.0,
                          foot_r=Pl + V(-0.08, -0.16, -0.30), foot_l=Pl + V(0.11, -0.05, -0.30),
                          foot_r_rot=(30.0, 0, 25), foot_l_rot=(18.0, 0, 40), knee_l=V(0.8, 0.6, 0),
                          knee_r=V(-0.4, 1, 0),
                          hand_r=Pl + V(-0.26, -0.02, 0.20), elbow_r=V(-0.5, 0.2, -1.0),
                          hand_l=Pl + V(0.24, 0.03, 0.17), elbow_l=V(0.6, 0.2, -1.0),
                          **self._wkw(R0, self._theta(R0.wpn) + deg(1.4) * -d * 0.5),
                          spin=d * 87.0, spin_axis=V(1, 0, 0), flow=0.05, jaw=18.0)
        return [rec, buckle, tip, lying]

    def death(self, length: float = 500.0):
        rec, buckle, tip, lying = self.death_keys()
        piv = lambda bp, k=0.35: bp.but(pivot=self.J["pelvis"] + bp.pelvis + V(0, 0, -0.02))
        keys = [K(0.0, piv(rec)), K(0.33, piv(buckle)), K(0.67, piv(tip), "in"), K(1.0, piv(lying), "out")]
        c = self.clip("Death", length, keys, length, post=self.grounded(snap=True))
        c.key_poses_ms = [0.0, 165.0, 335.0, length]
        return c

    # ── Stun 600 ms loop (C5): dazed sway, head lolling, weapon drooping ──────────────────────────────────────
    def stun(self, length: float = 600.0, droop: float = 40.0, hand_r: Vector | None = None,
             wlat: float | None = None, head_droop: float = 14.0, head_roll: float = 16.0, head_nod: float = 6.0):
        """Stun 600 ms loop: dazed sway, the head lolling (``head_droop`` deg past READY, ± ``head_nod``; rolling ±
        ``head_roll``), weapon drooping. A helmed head (the chief) lolls less so the face stays on screen."""
        R0 = self.ready()
        U = self.U
        th0 = self._theta(R0.wpn) + droop
        if hand_r is not None:
            R0 = R0.but(hand_r=hand_r - V(0.01, -1.5 * U, -2.5 * U))

        def s(t: float) -> BP:
            ph = 2 * math.pi * t / length
            c, sn = math.cos(ph), math.sin(ph)
            wd, wu = self._aim(R0, th0 + 6.0 * sn, wlat)
            return R0.but(
                pelvis=R0.pelvis + V(0.35 * U * c, 0.6 * U, -1.2 * U + 0.4 * U * sn), lean=R0.lean + 6.0 + 2.0 * sn,
                side=5.0 * c, head_pitch=R0.head_pitch + head_droop + head_nod * sn, head_roll=head_roll * c,
                head_yaw=10.0 * sn,
                hand_r=R0.hand_r + V(0.01, -1.5 * U, -2.5 * U + 0.5 * U * c), wpn=wd, wpn_up=wu,
                hand_l=R0.hand_l + V(0.0, -2.0 * U, -3.0 * U + 0.6 * U * sn),
                knee_l=V(0.4, 1, 0), knee_r=V(-0.4, 1, 0),
                clav_l=(-4.0, 0.0), clav_r=(-4.0, 0.0), flow=0.15, wave=ph, sway=0.4 * c, jaw=10.0 + 4.0 * sn)
        return C.body_clip(self.rig, "Stun", length, s, cloth=self.cloth, loop=True, post=self.plant)

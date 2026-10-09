"""Mage poses and clips (spec art-inventory-ch1.md §3.2 / §3.4 / §3.5, DECISIONS R5/R9). Web keys:
``src/graphics/sprites/players/PlayerMage.ts:472-628`` (READY, ATTACK, CAST, HURT, DODGE, DEATH, idle/walk).

The 3D poses add what one side view cannot say: the staff held out at the right side (web ``zN`` 5 u, ``STAFF_LAT``
0.3: the crystal clears the hood from every facing), the off hand's spell gestures, chest twist, foot turn-out.
Cloth: the robe skirt's four chains (``skirt_f/b/l/r``) hang plumb, stream with ``flow`` and are pushed out by the
legs under them (an envelope of the skinned leggings and boots, distributed by each chain's angular weight); sash
tails, hood tip, beard and tome hang from their parents with secondary lag, stay off the body and off the floor.
"""
from __future__ import annotations

import math

import numpy as np
from mathutils import Matrix, Quaternion, Vector

from kit import anim
from kit.anim import Pose

import common as C
from common import BP, K, V
import hero_base as HB
from hero_base import RUN_SPEED, WALK_SPEED


class MageAnims(HB.HeroAnims):
    GRIPS = ("r",)
    CHAINS = ("skirt_f", "skirt_b", "skirt_l", "skirt_r", "sash_a", "sash_b", "hood_tip", "beard", "tome")
    FEET = {"l": ("boot_l", "boot_cuff_l"), "r": ("boot_r", "boot_cuff_r")}
    ITEMS = (("SM_Hero_Mage_Staff", "weapon_r"), ("SM_Hero_Mage_Crystal", "weapon_r"))
    CLOTH_PARTS = ("skirt", "lining", "rune", "hem", "robe_seam", "skirt_fold", "skirt_edge", "sash_tail", "hood_tip",
                   "tome", "beard")
    ON_CLOTH = ("robe", "closure", "sash", "sash_knot", "leg")
    ON_CLOTH_CLEAR = 0.012
    LEG_PARTS = ("leg", "boot", "boot_cuff")
    TORSO_PARTS = ("mantle", "mantle_trim", "clasp", "clasp_gem", "robe", "closure", "sash", "sash_knot")
    SKIRT_CLEAR = 0.014        # leg surface → skirt mid-surface (cloth half thickness 4 mm + 1 cm)

    def __init__(self, rig, D):
        super().__init__(rig, D)
        import mage as MG
        self.MG = MG
        # rest geometry of the four skirt chains in the pelvis rest frame: root radius, per-link rest outward angles
        self.skirt = {}
        for c, ac in MG.SKIRT_ANG.items():
            info = self.chains[f"skirt_{c}"]
            pts = [self.rig.head(info.names[0])] + [self.rig.tail(n) for n in info.names]
            dirv = Vector((math.sin(math.radians(ac)), -math.cos(math.radians(ac)), 0.0))
            rad = [p.dot(dirv) for p in pts]
            outs = [math.degrees(math.atan2(rad[i + 1] - rad[i], pts[i].z - pts[i + 1].z)) for i in range(2)]
            self.skirt[c] = dict(ang=ac, dir=dirv, root=pts[0], root_r=rad[0], rest_out=outs,
                                 lengths=self.chain_lengths(info))
        self.z0 = MG.th(MG.SKIRT_TOP_H)
        self.z1 = MG.u(MG.SKIRT_HEM_Z)

    # ── skinned obstacles ───────────────────────────────────────────────────────────────────────────────
    def make_env(self, body, names):
        legs = C.SkinPoints(self.rig).add_mesh(body, names, lambda n: n.rstrip("_lr") in self.LEG_PARTS, step=2)
        self.legs = legs
        self.torso_env = C.SkinPoints(self.rig).add_mesh(body, names, lambda n: n in self.TORSO_PARTS, step=3)
        return legs

    # ── ready (web READY :472-487) ──────────────────────────────────────────────────────────────────────
    STAFF_THETA, STAFF_LAT = 6.0, -0.30         # web wpn 0.1 rad, STAFF_LAT 0.3 (out to the right)

    def staff_kw(self, theta: float, lat: float = 0.0, roll: float = 0.0) -> dict:
        d, e = C.blade(theta, lat=lat, roll=roll)
        return dict(wpn=d, wpn_up=e)

    def ready(self) -> BP:
        return BP(
            pelvis=self.web_pelvis(46.8, 65.0), hip_pitch=1.0, hip_yaw=-4.0, lean=2.3, twist=6.0, head_pitch=-1.0,
            head_yaw=-3.0,
            foot_r=self.web_foot(52.0, -0.10), foot_l=self.web_foot(43.0, 0.105),
            foot_r_rot=(0, 0, 10), foot_l_rot=(0, 0, 24), knee_r=V(-0.15, 1, 0), knee_l=V(0.25, 1, 0),
            hand_r=V(-0.300, 0.10, 0.92), elbow_r=V(-0.75, 0.55, -0.35),
            **self.staff_kw(self.STAFF_THETA, self.STAFF_LAT),
            hand_l=V(0.215, 0.045, 0.80), elbow_l=V(0.7, 0.6, -0.3), wrist_l=(8.0, 0.0, -6.0),
            clav_r=(2.0, 0.0), flow=0.12,
        )

    # ── cloth ───────────────────────────────────────────────────────────────────────────────────────────
    def _acc(self, mats, parent: str, hinge: Vector, down=Vector((0, 0, -1))) -> float:
        M3 = mats[parent].to_3x3() @ self.bones[parent].matrix_local.to_3x3().inverted()
        Dn = (down - hinge * down.dot(hinge)).normalized()
        Bk = hinge.cross(Dn)
        v = M3 @ Vector((0, 0, -1))
        return math.degrees(math.atan2(v.dot(Bk), v.dot(Dn)))

    def skirt_angles(self, p: Pose, bp: BP, mats) -> dict:
        """Per chain: (sagittal absolute angles, outward extra per link) after flow, gravity and the leg envelope."""
        MG = self.MG
        Pm = mats["pelvis"] @ self.bones["pelvis"].matrix_local.inverted()
        inv = Pm.inverted()
        X = (Pm.to_3x3() @ Vector((1, 0, 0))).normalized()
        acc = self._acc(mats, "pelvis", X)
        fl = bp.flow - 0.12
        ga, gs = self.gravity(bp)
        legs = None
        if self.skin() is not None:
            pts = self.legs.posed(mats)
            q = (np.c_[pts, np.ones(len(pts))] @ np.array(inv).T)[:, :3]          # pelvis rest frame
            th = np.degrees(np.arctan2(q[:, 0], -q[:, 1]))
            rho = np.hypot(q[:, 0], q[:, 1])
            k = (self.z0 - q[:, 2]) / (self.z0 - self.z1)
            legs = (q, th, rho, k)
        out = {}
        for c, sk in self.skirt.items():
            info = self.chains[f"skirt_{c}"]
            # sagittal stream (+ back) — the whole skirt streams back with flow; front / back chains also flare
            stream = [math.degrees(fl * 0.55 * (0.5 + 0.5 * (i + 1) / 2)) for i in range(2)]
            wave = [math.degrees(math.sin(bp.wave * 2 - 1.3 * (i + 1) + sk["ang"] * 0.02) * 0.025 * (i + 1) * (0.4 + bp.flow))
                    for i in range(2)]
            if c in "fb":
                sgn = -1.0 if c == "f" else 1.0                 # outward = back for b, forward for f
                out_ang = [sk["rest_out"][i] + sgn * (stream[i] + wave[i]) for i in range(2)]
            else:
                out_ang = list(sk["rest_out"])
            if gs > 0 and c in "fb":            # spun body: the cloth falls toward the floor (pre-spin frame)
                sgn = -1.0 if c == "f" else 1.0
                for i in range(2):
                    abs_i = sgn * out_ang[i] + acc
                    d = self.arc(ga - abs_i)
                    abs_i += d * gs * (0.55, 0.8)[i]
                    out_ang[i] = sgn * (abs_i - acc)
            if legs is not None:
                q, th, rho, k = legs
                w = np.asarray(HB.smooth01(1.0 - np.abs((th - sk["ang"] + 180.0) % 360.0 - 180.0) / 90.0))
                sel = (w > 0.22) & (k > 0.08) & (k < 1.25)
                if sel.any():
                    kk = np.clip(k[sel], 0.0, 1.0)
                    r_at = np.array([self._skirt_r(kv, tv) for kv, tv in zip(kk, th[sel])])
                    delta = rho[sel] + self.SKIRT_CLEAR - r_at
                    need = np.array([self._skirt_r(kv, sk["ang"]) for kv in kk]) + np.maximum(delta, 0.0) / w[sel]
                    hit = delta > 0
                    if hit.any():
                        dn = sk["root"].z - q[sel][hit, 2]
                        bk = need[hit] - sk["root_r"]
                        out_ang = HB.envelope(out_ang, sk["lengths"], dn, bk, 0.0, max_deg=80.0, slack=0.0)
            out[c] = out_ang
        return {"acc": acc, "out": out}

    def _skirt_r(self, k: float, a: float) -> float:
        p = self.MG.skirt_point(min(max(k, 0.0), 1.0), a, 0.0)
        return math.hypot(p.x, p.y)

    def set_skirt(self, p: Pose, bp: BP, sk_res: dict, wf) -> None:
        acc = sk_res["acc"]
        for c, sk in self.skirt.items():
            info = self.chains[f"skirt_{c}"]
            out_ang = sk_res["out"][c]
            if c in "fb":
                sgn = -1.0 if c == "f" else 1.0
                absang = [sgn * a + acc for a in out_ang]
                absang = self.above_ground(info, absang, wf, "pelvis", 0.018)
                C.set_chain(p, info, absang, acc)
            else:
                fl = bp.flow - 0.12
                absang = [info.rest[i] + math.degrees(fl * 0.5 * (0.6 + 0.4 * (i + 1) / 2)) + acc * 0.0
                          for i in range(2)]
                ga, gs = self.gravity(bp)
                if gs > 0:
                    for i in range(2):
                        d = self.arc(ga - absang[i])
                        absang[i] += d * gs * (0.5, 0.75)[i]
                extra = [out_ang[i] - sk["rest_out"][i] for i in range(2)]
                sg = -1.0 if c == "l" else 1.0          # roll that tips a downward bone outward
                rel = [sg * extra[0], sg * (extra[1] - extra[0])]
                absang = self.above_ground(info, absang, wf, "pelvis", 0.018)
                C.set_chain(p, info, absang, acc, rel)

    def cloth(self, p: Pose, bp: BP) -> None:
        mats = anim.evaluate(p, return_matrices=True)
        wf = self.world_fn(p, bp)
        sk = self.skirt_angles(p, bp, mats)
        self.set_skirt(p, bp, sk, wf)
        acc = sk["acc"]
        ch = self.chains
        # sash tails: hang behind the skirt, never inside its back (the back chain's angles + a margin)
        skb = [-a for a in sk["out"]["b"]]
        for nm, ph in (("sash_a", 0.0), ("sash_b", 0.9)):
            info = ch[nm]
            want = self.hang(bp, info, gain=1.05, lag_ms=70.0, wave_amp=0.07, phase=ph)
            floor_b = [-(skb[0]) + acc + 3.0, -(skb[0]) + acc + 3.0, -(skb[1]) + acc + 4.0]
            want = [max(want[i], floor_b[i]) if self.gravity(bp)[1] < 0.5 else want[i] for i in range(3)]
            want = self.above_ground(info, want, wf, "pelvis", 0.012)
            C.set_chain(p, info, want, acc, [math.degrees(bp.sway) * 0.0 + 1.5 * bp.flow * (1 if nm == "sash_a" else -1)
                                              for _ in range(3)])
        # tome: a pendulum at the right hip (lagged swing, streams back with flow), kept outside the flared skirt
        info = ch["tome"]
        pb = self.lagged(bp, 90.0)
        swing = math.degrees(math.sin(bp.wave + 0.4) * 0.10) + math.degrees((pb.flow - 0.12) * 0.55)
        absang = [info.rest[0] + swing]
        ga, gs = self.gravity(bp)
        if gs > 0:
            d = self.arc(ga - absang[0])
            absang[0] += d * gs * 0.85
        absang = self.above_ground(info, absang, wf, "pelvis", 0.03)
        ext_f = max(0.0, sk["out"]["f"][0] - self.skirt["f"]["rest_out"][0])
        ext_r = max(0.0, sk["out"]["r"][0] - self.skirt["r"]["rest_out"][0])
        ext = 0.4 * ext_f + 0.75 * ext_r
        a = math.radians(self.MG.TOME_A)
        C.set_chain(p, info, absang, acc)
        p.rot("tome_01", pitch=-ext * math.cos(a) * 0.9, roll=ext * abs(math.sin(a)))
        # hood tip: hangs from the back of the hood, streams with flow, rests on the mantle
        self._hood_tip(p, bp, wf)
        # beard: stiff-ish, mostly follows the head, sags toward the world down, stays in front of the chest
        self._beard(p, bp, wf)

    def _hood_tip(self, p: Pose, bp: BP, wf) -> None:
        info = self.chains["hood_tip"]
        mats = anim.evaluate(p, return_matrices=True)
        Hm = mats["head"] @ self.bones["head"].matrix_local.inverted()
        X = (Hm.to_3x3() @ Vector((1, 0, 0))).normalized()
        acc = self._acc(mats, "head", X)
        want = self.hang(bp, info, gain=0.9, lag_ms=80.0, wave_amp=0.05, phase=0.4,
                         acc_w=(0.35, 0.5), body_min=None)
        # keep the tip's rest drape relative to the head when the head tilts (a stiff hood), then let gravity win
        want = [0.6 * (info.rest[i] + acc) + 0.4 * want[i] for i in range(2)]
        if self.skin() is not None:
            root, Dn, B, _, _ = self.plane(mats, "head", "hood_tip_01")
            pts = self.torso_env.posed(mats)
            rel = pts - np.array(root)
            dn, bk = rel @ np.array(Dn), rel @ np.array(B)
            ang_rel = [a - acc for a in want]
            ang_rel = HB.envelope([a + acc - acc for a in ang_rel], self.chain_lengths(info), dn, bk, 0.035)
            want = [a for a in ang_rel]
            want = [a for a in want]
            want = [w for w in want]
            want = [want[i] for i in range(2)]
            want = [a for a in want]
            want = [want[i] if True else 0 for i in range(2)]
            want = [w + 0.0 for w in want]
            want = [w for w in want]
            want = [w for w in want]
            want = [w for w in want]
            want = [w for w in want]
            want = [w for w in want]
            want = [w + acc for w in want]
        want = self.above_ground(info, want, wf, "head", 0.02)
        C.set_chain(p, info, want, acc)

    def _beard(self, p: Pose, bp: BP, wf) -> None:
        info = self.chains["beard"]
        mats = anim.evaluate(p, return_matrices=True)
        Hm = mats["head"] @ self.bones["head"].matrix_local.inverted()
        X = (Hm.to_3x3() @ Vector((1, 0, 0))).normalized()
        acc = self._acc(mats, "head", X)
        pb = self.lagged(bp, 60.0)
        fl = pb.flow - 0.12
        want = []
        for i in range(2):
            follow = info.rest[i] + acc                       # stiff: rides with the head
            free = info.rest[i] * 0.4                         # hanging toward the world down
            a = 0.62 * follow + 0.38 * free + math.degrees(fl * 0.45 * (i + 1) / 2)
            a += math.degrees(math.sin(bp.wave + 0.8 - 1.4 * i) * 0.03 * (0.4 + pb.flow))
            want.append(a)
        ga, gs = self.gravity(bp)
        if gs > 0:
            for i in range(2):
                d = self.arc(ga - want[i])
                want[i] += d * gs * (0.35, 0.6)[i]
        if self.skin() is not None and gs < 0.5:
            root, Dn, B, _, _ = self.plane(mats, "head", "beard_01")
            pts = self.torso_env.posed(mats)
            rel = pts - np.array(root)
            dn, fw = rel @ np.array(Dn), -(rel @ np.array(B))        # forward coordinate
            fwd = HB.envelope([-(a - acc) for a in want], self.chain_lengths(info), dn, fw, 0.022, max_deg=70.0)
            want = [-(a) + acc for a in fwd]
        want = self.above_ground(info, want, wf, "head", 0.015)
        C.set_chain(p, info, want, acc)

    def all_clips(self):
        return []

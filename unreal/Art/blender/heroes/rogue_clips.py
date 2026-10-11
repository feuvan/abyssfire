"""Rogue poses and clips (spec art-inventory-ch1.md §3.3 / §3.4 / §3.5, §10; DECISIONS R5/R9). Web keys:
``src/graphics/sprites/players/PlayerRogue.ts:422-588`` (READY, ATTACK, CAST, HURT, DODGE, DEATH, idle / walk).

Timings (spec §10): Idle 667 loop · Walk 1000 @ 1.2 m/s · Run 580 @ 3.333 m/s (no-slide gait, FootL/R) ·
Attack01-03 444 (Contact 222; keys over 388.9 ms) · Cast01 / Cast02 / Cast_Multishot 533 (Release 246; keys over
466.7 ms) · Hurt 267 (+ HurtAdd) · Dodge 240 (tuck roll) · Death 545 · Portrait.

The 3D poses keep the web's read — the smallest hero, crouched, both blades held wide (web ``zN`` / ``zF``) — and give
every contact / release its own silhouette: Attack01 the web double slash (diagonal cut + off-hand stab), Attack02 an
X cross-slash (both blades swept out from a crossed guard), Attack03 a twin plunge (both blades driven down from
overhead into a crouch), Cast01 the web coil-and-fling, Cast02 a trap set at the feet, Cast_Multishot (R5 signature)
a side-on bow draw with the flash-bow in the off hand and the right dagger reversed along the drawing forearm.

Cloth: the two scarf tails hang from the knot at the back of the neck with secondary lag, stream with ``flow``, swing
apart and sway, and are kept off the capelet, jerkin, tassets, quiver and legs (envelope of the skinned / attached
surfaces) and off the floor; the hood tip rides the hood. The tassets follow the thighs through their skin weights.
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


class RogueAnims(HB.HeroAnims):
    GRIPS = ("r", "l")
    CHAINS = ("scarf_a", "scarf_b", "hood_tip")
    FEET = {"l": ("boot_l", "boot_cuff_l"), "r": ("boot_r", "boot_cuff_r")}
    ITEMS = (("SM_Hero_Rogue_Dagger", "weapon_r"), ("SM_Hero_Rogue_Dagger_L", "weapon_l"))
    CLOTH_PARTS = ("scarf_tail", "hood_tip")
    ON_CLOTH = ("capelet", "hood", "scarf_knot", "jerkin", "bandolier")
    ON_CLOTH_CLEAR = 0.008
    TORSO_PARTS = ("capelet", "capelet_edge", "clasp", "jerkin", "bandolier", "knife", "knife_pommel", "belt", "buckle",
                   "tasset", "pouch", "pouch_flap", "scarf_knot")
    SCARF_PARTS = ("capelet", "jerkin", "bandolier", "belt", "buckle", "tasset", "pouch", "pouch_flap", "leg_l",
                   "leg_r", "kneepad_l", "kneepad_r")
    SCARF_CLEAR = 0.024

    TASSET_CLEAR = 0.018        # thigh surface → panel mid-surface (plate 6 mm + bow + skinning slack)
    TASSET_MAX = 130.0          # deg: a panel never swings further out than this (knees pulled up: flipped up)

    def __init__(self, rig, D):
        super().__init__(rig, D)
        import rogue as RG
        self.RG = RG
        # tasset hinges (pelvis rest frame = armature rest space): head, unit direction, length, outward axis
        self.tassets = []
        for i, (a0, a1) in enumerate(RG.tasset_panels()):
            bn = f"tasset_{i}_01"
            b = self.bones[bn]
            H, T = b.head_local.copy(), b.tail_local.copy()
            ac = math.radians((a0 + a1) / 2)
            rad = Vector((math.sin(ac), -math.cos(ac), 0.0))
            d = (T - H).normalized()
            axis = d.cross(rad).normalized()                 # rotating d about it moves the hem outward
            half = math.radians((a1 - a0) / 2)
            self.tassets.append(dict(bone=bn, H=H, d=d, L=(T - H).length, rad=rad, axis=axis, half=half,
                                     z=b.matrix_local.to_3x3().col[2].copy()))

    # ── skinned obstacles ───────────────────────────────────────────────────────────────────────────────
    def make_env(self, body, names):
        self.leg_env = C.SkinPoints(self.rig).add_mesh(body, names, lambda n: n.rstrip("_lr") in ("leg", "kneepad"),
                                                       step=1)
        self.torso_env = C.SkinPoints(self.rig).add_mesh(body, names, lambda n: n in self.TORSO_PARTS, step=3)
        self.scarf_env = C.SkinPoints(self.rig).add_mesh(body, names, lambda n: n in self.SCARF_PARTS, step=2)
        import bpy
        q = bpy.data.objects.get(self.RG.QUIVER)
        if q is not None and any(s["name"] == "quiver" for s in self.rig.sockets):
            # the quiver rides the pelvis at its socket (item-local = socket frame, attached or not yet): a scarf
            # obstacle and a ground point (it lies under the body in Death)
            bone, m = HB.socket_local(self.rig, "quiver")
            co = np.zeros(len(q.data.vertices) * 3)
            q.data.vertices.foreach_get("co", co)
            co = co.reshape(-1, 3)[::2]
            rest = np.array(self.rig.obj.data.bones[bone].matrix_local)
            co = (np.c_[co, np.ones(len(co))] @ (rest @ np.array(m)).T)[:, :3]
            for env in (self.scarf_env, self.ground_pts):
                idx = np.zeros((len(co), 4), np.int64)
                idx[:, 0] = env.bidx[bone]
                w = np.zeros((len(co), 4))
                w[:, 0] = 1.0
                env._append(co, idx, w, ["quiver"] * len(co))
            self._ground_clear = np.concatenate([self._ground_clear, np.zeros(len(co))])
        return self.torso_env

    def preview_hook(self, clip, ms: float) -> None:
        """Review renders only: the flash-bow's energy string (runtime FX in UE, ``fx.bowString``)."""
        import rogue_ship
        rogue_ship.string_hook(clip, ms)

    # ── hammer-grip sanity (post step) ──────────────────────────────────────────────────────────────────
    GRIP_DOT = (-0.30, 0.40)          # blade · forearm: the pommel stays off the bracer, the blade off the forearm
    REVERSE_DOT = (-0.68, -0.40)      # right hand reversed (``cling`` > 0.5, the archery draw): along, not on, it

    def fit_daggers(self, bp: BP) -> BP:
        """Keep each blade within a natural hammer-grip angle of its forearm (the IK decides the forearm, the key
        decides the blade): the least turn of the blade in the plane of blade and forearm that brings blade · forearm
        into ``GRIP_DOT`` (a key past it put the pommel into the bracer or laid the blade along the forearm)."""
        if bp.hand_r is None and bp.hand_l is None:
            return bp
        p = C.to_pose(self.rig, bp.but(spin=0.0), None, grips=self.GRIPS)      # blade keys are pre-spin
        mats = anim.evaluate(p, return_matrices=True)
        out = {}
        for s, key in (("r", "wpn"), ("l", "off")):
            f = (mats[f"lowerarm_{s}"].to_3x3() @ Vector((0, 1, 0))).normalized()
            d = getattr(bp, key).normalized()
            lo, hi = self.REVERSE_DOT if (s == "r" and bp.cling > 0.5) else self.GRIP_DOT
            c = d.dot(f)
            if lo <= c <= hi:
                continue
            u = d - f * c
            if u.length < 1e-4:
                u = getattr(bp, key + "_up") - f * getattr(bp, key + "_up").dot(f)
            u.normalize()
            c2 = min(hi, max(lo, c))
            out[key] = (f * c2 + u * math.sqrt(max(0.0, 1.0 - c2 * c2))).normalized()
        return bp.but(**out) if out else bp

    def post(self, bp: BP, t: float = 0.0) -> BP:
        return self.plant(self.fit_daggers(bp), t)

    # ── ready (web READY :422-437): crouched, both blades held wide ─────────────────────────────────────
    def dkw(self, theta: float, lat: float = 0.0, roll: float = 0.0) -> dict:
        return self.blade_kw(theta, lat, roll, key="wpn")

    def okw(self, theta: float, lat: float = 0.0, roll: float = 0.0) -> dict:
        return self.blade_kw(theta, lat, roll, key="off")

    def ready(self) -> BP:
        return BP(
            pelvis=self.web_pelvis(46.5, 66.4), hip_pitch=2.0, hip_yaw=-6.0, lean=11.5, twist=-6.0,
            head_pitch=-8.0, head_yaw=10.0,
            foot_r=self.web_foot(55.0, -0.15), foot_l=self.web_foot(40.5, 0.13),
            foot_r_rot=(0, 0, 12), foot_l_rot=(0, 0, 30), knee_r=V(-0.35, 1, 0), knee_l=V(0.4, 1, 0),
            # (the web's 83° / 46° blades, raised for the −50° camera: a level blade reads as pointing down)
            hand_r=self.web_hand(56.5, 65.0, -0.30), elbow_r=V(-0.8, -0.4, -0.4), **self.dkw(40.0, -0.6),
            hand_l=self.web_hand(53.0, 58.0, 0.26), elbow_l=V(0.8, -0.3, -0.5), **self.okw(26.0, 0.6),
            clav_r=(0.0, 4.0), clav_l=(3.0, 4.0), flow=0.15,
        )

    # ── cloth ───────────────────────────────────────────────────────────────────────────────────────────
    def cloth(self, p: Pose, bp: BP) -> None:
        wf = self.world_fn(p, bp)
        self._tassets(p, bp)
        self._scarf(p, bp, wf)
        self._hood_tip(p, bp, wf)

    def _tassets(self, p: Pose, bp: BP) -> None:
        """Split tassets: each panel turns about its hinge line (under the belt) just far enough out that its plate
        clears the posed thighs and knee pads under it (an envelope of the skinned legs in the panel's plane), plus
        a little flow streaming on the back panels."""
        if self.skin() is None:
            return
        mats = anim.evaluate(p, return_matrices=True)
        Pm = mats["pelvis"] @ self.bones["pelvis"].matrix_local.inverted()
        inv = np.array(Pm.inverted())
        pts = self.leg_env.posed(mats)
        q = (np.c_[pts, np.ones(len(pts))] @ inv.T)[:, :3]             # pelvis rest frame
        R3 = Pm.to_3x3().normalized()
        pb = self.lagged(bp, 60.0)
        fl = max(0.0, pb.flow - 0.15)
        from mathutils import Quaternion as Q_
        for t in self.tassets:
            H, d, L, rad, axis = t["H"], t["d"], t["L"], t["rad"], t["axis"]
            rel = q - np.array(H)
            dn = rel @ np.array(-Vector((0, 0, 1)))
            tang = Vector((0, 0, 1)).cross(rad)
            lat = rel @ np.array(tang)
            out = rel @ np.array(rad)
            a_rest = math.atan2(d.dot(rad), -d.z)
            # back panels stream out a little with the flow
            a = a_rest + math.radians(14.0 * fl * max(0.0, -rad.y))
            # points under the panel (not at the hinge: there the plate is still pelvis-bound under the belt); the
            # clearance ramps in over the first 4 cm below the hinge
            sel = (dn > 0.012) & (dn < L * 1.15) & (np.abs(lat) < 0.075) & (out > -0.06)
            if sel.any():
                clr = self.TASSET_CLEAR * np.clip(dn[sel] / 0.04, 0.0, 1.0)
                need = float(np.arctan2(out[sel] + clr, dn[sel]).max())
                a = max(a, min(need, a_rest + math.radians(self.TASSET_MAX)))
            delta = a - a_rest
            if abs(delta) < 1e-5:
                continue
            qr = Q_(axis, delta)
            d_new = R3 @ (qr @ d)
            z_new = R3 @ (qr @ t["z"])
            p.aim(t["bone"], d_new, z_new)

    def _scarf(self, p: Pose, bp: BP, wf) -> None:
        """Tails A (dark, long) / B: web clothChain (base angle 0.3 + 0.6 flow rad back from down, travelling wave)
        → ``hang`` with secondary lag; swung apart (A to the left, B to the right) and swaying; envelope of the
        skinned back / tassets / legs / quiver in each tail's plane; above the floor."""
        mats = anim.evaluate(p, return_matrices=True)
        env = None
        if self.skin() is not None:
            env = self.scarf_env.posed(mats)
        for nm, ph, spread, gain in (("scarf_a", 0.0, 5.0, 1.55), ("scarf_b", 1.4, -6.0, 1.4)):
            info = self.chains[nm]
            root, Dn, B, X, acc = self.plane(mats, "spine_03", info.names[0])
            want = self.hang(bp, info, gain=gain, lag_ms=110.0, wave_amp=0.10, phase=ph)
            if env is not None:
                rel = env - np.array(root)
                lat = rel @ np.array(X)
                sel = np.abs(lat - math.copysign(0.012, spread)) < 0.10
                if sel.any():
                    want = HB.envelope(want, self.chain_lengths(info), rel[sel] @ np.array(Dn),
                                       rel[sel] @ np.array(B), self.SCARF_CLEAR, max_deg=120.0)
            want = self.above_ground(info, want, wf, "spine_03", 0.05)
            pb = self.lagged(bp, 80.0)
            side = []
            for i in range(len(info.names)):
                k = (i + 1) / len(info.names)
                sway = math.degrees(math.sin(bp.wave * 1.0 + ph - 1.6 * k) * 0.05 * (0.3 + pb.flow))
                side.append((spread if i == 0 else 0.0) + sway + bp.sway * 2.0 * k)
            C.set_chain(p, info, want, acc, side)

    def _acc(self, mats, parent: str, hinge: Vector, down=Vector((0, 0, -1))) -> float:
        M3 = mats[parent].to_3x3() @ self.bones[parent].matrix_local.to_3x3().inverted()
        Dn = (down - hinge * down.dot(hinge)).normalized()
        Bk = hinge.cross(Dn)
        v = M3 @ Vector((0, 0, -1))
        return math.degrees(math.atan2(v.dot(Bk), v.dot(Dn)))

    def _hood_tip(self, p: Pose, bp: BP, wf) -> None:
        info = self.chains["hood_tip"]
        mats = anim.evaluate(p, return_matrices=True)
        Hm = mats["head"] @ self.bones["head"].matrix_local.inverted()
        X = (Hm.to_3x3() @ Vector((1, 0, 0))).normalized()
        acc = self._acc(mats, "head", X)
        free = self.hang(bp, info, gain=0.9, lag_ms=80.0, wave_amp=0.05, phase=0.4, acc_w=(0.3, 0.45))
        want = [0.6 * (info.rest[i] + acc) + 0.4 * free[i] for i in range(2)]
        if self.skin() is not None:
            root, Dn, B, _, _ = self.plane(mats, "head", "hood_tip_01")
            pts = self.torso_env.posed(mats)
            rel = pts - np.array(root)
            want = HB.envelope(want, self.chain_lengths(info), rel @ np.array(Dn), rel @ np.array(B), 0.025)
        want = self.above_ground(info, want, wf, "head", 0.02)
        C.set_chain(p, info, want, acc)

    def world_fn(self, p: Pose, bp: BP):
        """Scarf tails reach the thighs: the floor frame is built for every crouch / spin (base class: only for
        deep ones)."""
        if abs(bp.spin) < 1e-6 and bp.lift >= -1e-6 and bp.pelvis.z > -0.12:
            return None
        mats = anim.evaluate(p, return_matrices=True)
        if abs(bp.spin) > 1e-6:
            Rm = Quaternion(Vector(bp.spin_axis).normalized(), math.radians(bp.spin)).to_matrix()
            P = bp.pivot + Vector((0, 0, bp.lift))
        else:
            Rm, P = Matrix.Identity(3), Vector()

        def world(x: Vector) -> Vector:
            return P + Rm @ (x - P)
        return world, mats, Rm

    # ── Idle (667 ms loop, web idlePose: light on the feet, a bounce rather than a breath) ───────────────
    def idle(self):
        R0 = self.ready()
        U = self.U

        def s(t: float) -> BP:
            ph = 2 * math.pi * t / 667.0
            b = math.sin(ph)
            return R0.but(
                pelvis=R0.pelvis + V(0, math.sin(ph) * 0.3 * U, (abs(b) * 0.6 - 0.3) * U),
                lean=R0.lean + b * 0.6,
                head_pitch=R0.head_pitch + math.degrees(math.sin(ph - 0.5) * 0.04),
                hand_r=R0.hand_r + V(0, 0, -math.sin(ph - 0.3) * 0.5 * U),
                hand_l=R0.hand_l + V(0, 0, -math.sin(ph - 0.8) * 0.6 * U),
                **self.dkw(40.0 + math.degrees(math.sin(ph) * 0.04), -0.6),
                **self.okw(26.0 + math.degrees(math.sin(ph - 0.6) * 0.05), 0.6),
                flow=0.15 + math.sin(ph) * 0.07, wave=ph, sway=math.sin(ph + 1.0) * 0.15)
        c = self.proc_clip("Idle", 667.0, s, loop=True)
        c.key_poses_ms = [0.0, 166.8, 333.5, 500.3]
        return c

    # ── locomotion (no foot sliding, R9): Run 580 ms @ 3.333 m/s, Walk 1000 ms @ 1.2 m/s ────────────────
    def locomotion(self, name: str, length: float, speed: float, duty: float, lift: float, bob: float,
                   lean: float, arm: float, flow: float, crouch: float, stance: float = 0.085):
        J = self.J

        def s(t_ms: float) -> BP:
            t = (t_ms / length) % 1.0
            ((fl, pl, tl), (fr, pr, tr)), dz, swing = self.gait_feet(t, speed, length, duty, lift, bob)
            ph = 2 * math.pi * t
            sw = max(-1.0, min(1.0, swing))
            sp = speed / RUN_SPEED
            # web walkPose: both blades carried forward low, the main hand swinging ±4 u, the off hand ±3 u
            bp = BP(
                pelvis=V(0, 0.02 * sp, crouch + dz), hip_pitch=2.0, hip_yaw=-8.0 * sw * sp ** 0.5,
                hip_roll=-2.0 * math.sin(ph), lean=lean + 1.5 * abs(math.sin(ph)), twist=9.0 * sw * sp ** 0.5,
                head_pitch=-lean * 0.6, head_yaw=-3.0 * sw,
                foot_l=J["ankle_l"] + V(stance - J["ankle_l"].x, 0, 0) + fl,
                foot_r=J["ankle_r"] + V(-(stance + J["ankle_r"].x), 0, 0) + fr,
                foot_l_rot=(pl, 0, 6), foot_r_rot=(pr, 0, 6), toe_l=tl, toe_r=tr,
                knee_l=V(0.15, 1, 0), knee_r=V(-0.15, 1, 0),
                hand_r=V(-0.28, 0.22 - arm * 1.2 * sw, 0.80 + 0.03 * abs(sw)), elbow_r=V(-0.8, -0.4, -0.4),
                **self.dkw(48.0 - 8.0 * sw, -0.6),
                hand_l=V(0.24, 0.16 + arm * 0.9 * sw, 0.92 + 0.02 * abs(sw)), elbow_l=V(0.8, -0.3, -0.5),
                **self.okw(30.0 + 6.0 * sw, 0.6),
                clav_r=(0.0, 4.0 - 4.0 * sw), clav_l=(2.0, 4.0 + 3.0 * sw),
                flow=flow + 0.1 * math.sin(2 * ph), wave=2 * ph, sway=0.25 * math.sin(ph))
            return bp
        c = self.proc_clip(name, length, s, loop=True, notifies=anim.run_contacts_ms(length), ref_speed=speed)
        c.key_poses_ms = [round(length * i / 8, 1) for i in range(8)]
        return c

    def run(self):
        return self.locomotion("Run", 580.0, RUN_SPEED, duty=0.25, lift=0.11, bob=0.028, lean=15.0, arm=0.10,
                               flow=0.5, crouch=-0.085)

    def walk(self):
        return self.locomotion("Walk", 1000.0, WALK_SPEED, duty=0.62, lift=0.07, bob=0.012, lean=9.0, arm=0.05,
                               flow=0.25, crouch=-0.06)

    # ── Attack01: the web double slash (ATTACK :449-478; 444 ms, Contact 222, keys over 388.9 ms) ────────
    # Contact silhouette: deep lunge (lean 33°), the main blade cut diagonally forward-down across the strike line,
    # the off blade held forward ready; then the off-hand stab at full reach.
    SPAN_A = 388.9

    def attack01(self):
        R0 = self.ready()
        W = self.pose(R0, pelvis=self.web_pelvis(44.5, 67.6), lean=3.5, head_pitch=-3.0, twist=24.0, hip_yaw=6.0,
                      foot_r=self.web_foot(56.0, -0.15), foot_l=self.web_foot(39.5, 0.13),
                      hand_r=V(0.04, 0.20, 1.20), elbow_r=V(-0.6, -0.5, 0.3), **self.dkw(-30.0, 0.55),
                      hand_l=V(0.30, 0.36, 0.96), elbow_l=V(0.8, -0.3, -0.5), **self.okw(60.0, 0.75),
                      clav_r=(14.0, 14.0), flow=0.25)
        W = self.look(W)
        S = self.pose(R0, pelvis=self.web_pelvis(51.0, 67.6), lean=24.0, twist=8.0, hip_yaw=2.0,
                      foot_r=self.web_foot(60.0, -0.15), foot_l=self.web_foot(40.0, 0.13),
                      hand_r=V(-0.10, 0.42, 1.18), elbow_r=V(-0.8, -0.3, 0.0), **self.dkw(40.0, -0.1),
                      hand_l=V(0.30, 0.36, 0.94), **self.okw(66.0, 0.75), clav_r=(10.0, 16.0), flow=0.5)
        S = self.look(S)
        Ct = self.pose(R0, pelvis=self.web_pelvis(54.0, 68.8), lean=33.0, hip_pitch=4.0, twist=-14.0, hip_yaw=-6.0,
                       foot_r=self.web_foot(63.0, -0.16), foot_l=self.web_foot(42.0, 0.13),
                       foot_l_rot=(8.0, 0, 30), toe_l=8.0,
                       hand_r=V(-0.30, 0.60, 0.80), elbow_r=V(-0.8, -0.3, -0.4), **self.dkw(118.0, -0.55),
                       hand_l=V(0.30, 0.42, 0.98), elbow_l=V(0.8, -0.3, -0.5), **self.okw(70.0, 0.75),
                       clav_r=(4.0, 18.0), clav_l=(2.0, 10.0), flow=0.8)
        Ct = self.look(Ct, max_down=10.0)
        F = self.pose(Ct, pelvis=self.web_pelvis(53.5, 69.0), lean=34.0, twist=12.0, hip_yaw=4.0,
                      hand_r=V(-0.34, 0.36, 0.62), elbow_r=V(-0.8, 0.2, -0.3), **self.dkw(150.0, -0.6),
                      hand_l=V(0.14, 0.64, 1.00), elbow_l=V(0.7, -0.2, -0.6), **self.okw(64.0, 0.6),
                      clav_l=(4.0, 20.0), clav_r=(2.0, 6.0), flow=0.6)
        F = self.look(F, max_down=10.0)
        Rc = self.pose(R0, pelvis=self.web_pelvis(49.5, 67.4), lean=20.0, twist=-2.0,
                       foot_r=self.web_foot(58.0, -0.15), hand_r=V(-0.30, 0.34, 0.74), **self.dkw(96.0, -0.4),
                       flow=0.3)
        keys = [K(0.0, R0), K(0.28, W, "out"), K(0.43, S, "in"), K(0.571, Ct, "linear"), K(0.71, F, "out"),
                K(0.86, Rc), K(1.0, R0)]
        return self.clip("Attack01", 444.0, keys, self.SPAN_A, notifies={"Contact": 222.0})

    # ── Attack02: X cross-slash — both blades crossed high, then swept out and down (same timing) ────────
    # Contact silhouette: both arms flung wide and low in front, the blades pointing out-forward-down: a wide Y.
    def attack02(self):
        R0 = self.ready()
        W = self.pose(R0, pelvis=R0.pelvis + V(0, -0.02, 0.02), lean=-2.0, head_pitch=-2.0, twist=0.0, hip_yaw=0.0,
                      foot_r=self.web_foot(54.0, -0.15), foot_l=self.web_foot(41.0, 0.14),
                      hand_r=V(-0.24, 0.12, 1.40), elbow_r=V(-0.9, -0.2, -0.2), **self.dkw(-38.0, -0.55),
                      hand_l=V(0.24, 0.12, 1.38), elbow_l=V(0.9, -0.2, -0.2), **self.okw(-38.0, 0.55),
                      clav_r=(22.0, 4.0), clav_l=(22.0, 4.0), flow=0.25)
        W = self.look(W)
        S = self.pose(R0, pelvis=R0.pelvis + V(0, 0.06, -0.04), lean=16.0, twist=0.0, hip_yaw=0.0,
                      foot_r=self.web_foot(58.0, -0.16), foot_l=self.web_foot(41.0, 0.14),
                      hand_r=V(-0.22, 0.42, 1.18), elbow_r=V(-0.8, -0.3, -0.3), **self.dkw(60.0, -0.5),
                      hand_l=V(0.22, 0.42, 1.16), elbow_l=V(0.8, -0.3, -0.3), **self.okw(60.0, 0.5),
                      clav_r=(8.0, 14.0), clav_l=(8.0, 14.0), flow=0.5)
        S = self.look(S)
        Ct = self.pose(R0, pelvis=R0.pelvis + V(0, 0.13, -0.08), lean=26.0, hip_pitch=3.0, twist=0.0, hip_yaw=0.0,
                       foot_r=self.web_foot(61.0, -0.17), foot_l=self.web_foot(41.0, 0.15),
                       foot_l_rot=(8.0, 0, 28), toe_l=8.0,
                       hand_r=V(-0.44, 0.40, 0.84), elbow_r=V(-0.6, -0.5, -0.5), **self.dkw(118.0, -0.95),
                       hand_l=V(0.44, 0.38, 0.86), elbow_l=V(0.6, -0.5, -0.5), **self.okw(118.0, 0.95),
                       clav_r=(2.0, 8.0), clav_l=(2.0, 8.0), flow=0.8)
        Ct = self.look(Ct, max_down=10.0)
        F = self.pose(Ct, hand_r=V(-0.50, 0.24, 0.84), **self.dkw(140.0, -1.2), hand_l=V(0.50, 0.22, 0.86),
                      **self.okw(140.0, 1.2), flow=0.6)
        F = self.look(F, max_down=10.0)
        Rc = self.pose(R0, pelvis=R0.pelvis + V(0, 0.06, -0.03), lean=16.0, twist=-3.0, hand_r=V(-0.34, 0.30, 0.78),
                       hand_l=V(0.30, 0.22, 0.92), flow=0.3)
        keys = [K(0.0, R0), K(0.28, W, "out"), K(0.43, S, "in"), K(0.571, Ct, "linear"), K(0.71, F, "out"),
                K(0.86, Rc), K(1.0, R0)]
        return self.clip("Attack02", 444.0, keys, self.SPAN_A, notifies={"Contact": 222.0})

    # ── Attack03: twin plunge — both blades raised overhead, driven down together into a crouch ─────────────
    # Contact silhouette: deep crouch-lunge, both fists together low in front, both blades plunged down-forward.
    def attack03(self):
        R0 = self.ready()
        W = self.pose(R0, pelvis=R0.pelvis + V(0, -0.03, 0.05), lean=-6.0, head_pitch=-6.0, twist=0.0, hip_yaw=0.0,
                      foot_r=self.web_foot(53.5, -0.15), foot_l=self.web_foot(41.5, 0.14),
                      foot_r_rot=(10.0, 0, 12), toe_r=10.0, foot_l_rot=(10.0, 0, 28), toe_l=10.0,
                      hand_r=V(-0.16, 0.12, 1.56), elbow_r=V(-0.9, -0.2, 0.1), **self.dkw(70.0, -0.3),
                      hand_l=V(0.16, 0.12, 1.54), elbow_l=V(0.9, -0.2, 0.1), **self.okw(70.0, 0.3),
                      clav_r=(26.0, 4.0), clav_l=(26.0, 4.0), flow=0.3)
        W = W.but(head_yaw=0.0)
        S = self.pose(R0, pelvis=R0.pelvis + V(0, 0.07, -0.06), lean=20.0, twist=0.0, hip_yaw=0.0,
                      foot_r=self.web_foot(59.0, -0.16), foot_l=self.web_foot(41.5, 0.14),
                      hand_r=V(-0.13, 0.46, 1.20), elbow_r=V(-0.9, -0.2, -0.2), **self.dkw(120.0, -0.2),
                      hand_l=V(0.13, 0.46, 1.18), elbow_l=V(0.9, -0.2, -0.2), **self.okw(120.0, 0.2),
                      clav_r=(14.0, 10.0), clav_l=(14.0, 10.0), flow=0.6)
        S = self.look(S)
        Ct = self.pose(R0, pelvis=R0.pelvis + V(0, 0.16, -0.17), lean=32.0, hip_pitch=5.0, twist=0.0, hip_yaw=0.0,
                       foot_r=self.web_foot(64.0, -0.17), foot_l=self.web_foot(41.0, 0.15),
                       foot_l_rot=(14.0, 0, 26), toe_l=14.0, knee_r=V(-0.4, 1, 0),
                       hand_r=V(-0.10, 0.62, 0.62), elbow_r=V(-0.9, -0.1, -0.3), **self.dkw(150.0, -0.15),
                       hand_l=V(0.10, 0.60, 0.62), elbow_l=V(0.9, -0.1, -0.3), **self.okw(150.0, 0.15),
                       clav_r=(2.0, 14.0), clav_l=(2.0, 14.0), flow=0.85)
        Ct = self.look(Ct, max_down=12.0)
        F = self.pose(Ct, hand_r=V(-0.10, 0.58, 0.56), **self.dkw(160.0, -0.15), hand_l=V(0.10, 0.56, 0.56),
                      **self.okw(160.0, 0.15), flow=0.6)
        F = self.look(F, max_down=12.0)
        Rc = self.pose(R0, pelvis=R0.pelvis + V(0, 0.07, -0.06), lean=20.0, twist=-3.0,
                       foot_r=self.web_foot(58.0, -0.16), hand_r=V(-0.28, 0.34, 0.74), hand_l=V(0.24, 0.26, 0.88),
                       flow=0.35)
        keys = [K(0.0, R0), K(0.28, W, "out"), K(0.43, S, "in"), K(0.571, Ct, "linear"), K(0.71, F, "out"),
                K(0.86, Rc), K(1.0, R0)]
        return self.clip("Attack03", 444.0, keys, self.SPAN_A, notifies={"Contact": 222.0})

    # ── Cast01: coil → spring, the main hand flung forward (web CAST :480-505; 533 ms, Release 246) ──────
    SPAN_C = 466.7
    REL = 246.0 / 466.7

    def cast01(self):
        R0 = self.ready()
        Ch = self.pose(R0, pelvis=self.web_pelvis(46.0, 69.5), lean=17.0, head_pitch=1.0, twist=-20.0, hip_yaw=-8.0,
                       foot_r=self.web_foot(56.0, -0.15), foot_l=self.web_foot(39.5, 0.13),
                       hand_r=V(-0.15, 0.26, 1.00), elbow_r=V(-0.9, -0.2, -0.4), **self.dkw(34.0, -0.8),
                       hand_l=V(0.14, 0.26, 0.96), elbow_l=V(0.9, -0.2, -0.4), **self.okw(30.0, 0.9),
                       clav_r=(4.0, 12.0), clav_l=(4.0, 12.0), flow=0.3)
        Ch = self.look(Ch)
        Rl = self.pose(R0, pelvis=self.web_pelvis(51.5, 67.5), lean=26.0, hip_pitch=3.0, twist=16.0, hip_yaw=6.0,
                       foot_r=self.web_foot(60.0, -0.16), foot_l=self.web_foot(40.0, 0.13),
                       foot_l_rot=(8.0, 0, 30), toe_l=8.0,
                       hand_r=V(-0.34, 0.54, 1.08), elbow_r=V(-0.8, -0.2, -0.5), **self.dkw(40.0, -1.4),
                       hand_l=V(0.38, -0.06, 0.82), elbow_l=V(0.7, 0.5, -0.4), **self.okw(20.0, 1.5),
                       clav_r=(6.0, 20.0), clav_l=(2.0, -8.0), flow=0.7)
        Rl = self.look(Rl)
        H = self.pose(Rl, lean=23.0, hand_r=V(-0.35, 0.50, 1.04), **self.dkw(44.0, -1.4), hand_l=V(0.37, -0.02, 0.84),
                      flow=0.4)
        H = self.look(H)
        keys = [K(0.0, R0), K(0.36, Ch, "out"), K(self.REL, Rl, "in"), K(0.72, H), K(1.0, R0)]
        return self.clip("Cast01", 533.0, keys, self.SPAN_C, notifies={"Release": 246.0})

    # ── Cast02: a trap set at the feet (explosive / slow / chain trap, vanish) ───────────────────────────
    def cast02(self):
        R0 = self.ready()
        Ch = self.pose(R0, pelvis=R0.pelvis + V(0, -0.01, -0.06), lean=14.0, head_pitch=6.0, twist=-10.0,
                       foot_r=self.web_foot(55.0, -0.16), foot_l=self.web_foot(40.5, 0.14),
                       hand_r=V(-0.26, 0.14, 1.10), elbow_r=V(-0.9, 0.0, -0.3), **self.dkw(10.0, -0.9),
                       hand_l=V(0.26, 0.20, 0.96), **self.okw(42.0, 0.45), clav_r=(16.0, 2.0), flow=0.3)
        # release: a deep crouch on the left knee's side, the right fist driven to the ground in front (the trap),
        # the off blade held out behind for balance
        Rl = self.pose(R0, pelvis=R0.pelvis + V(0, 0.08, -0.20), lean=34.0, hip_pitch=2.0, head_pitch=-14.0,
                       twist=-6.0, hip_yaw=-4.0, foot_r=self.web_foot(58.5, -0.18), foot_l=self.web_foot(40.0, 0.15),
                       foot_l_rot=(30.0, 0, 26), toe_l=30.0, knee_r=V(-0.5, 1, 0), knee_l=V(0.3, 1, -0.3),
                       hand_r=V(-0.30, 0.62, 0.22), elbow_r=V(-0.9, 0.0, 0.2), **self.dkw(100.0, -0.8),
                       hand_l=V(0.38, -0.02, 0.84), elbow_l=V(0.7, 0.4, -0.5), **self.okw(30.0, 0.8),
                       clav_r=(0.0, 18.0), clav_l=(6.0, -6.0), flow=0.6)
        Rl = self.look(Rl, max_down=14.0)
        H = self.pose(Rl, hand_r=V(-0.30, 0.60, 0.26), flow=0.35)
        keys = [K(0.0, R0), K(0.36, Ch, "out"), K(self.REL, Rl, "in"), K(0.72, H), K(1.0, R0)]
        return self.clip("Cast02", 533.0, keys, self.SPAN_C, notifies={"Release": 246.0})

    # ── Cast_Multishot (R5 signature): the flash-bow drawn side-on (multishot, piercing / poison arrow, rain) ─
    BOW_SHOW, BOW_HIDE = 40.0, 400.0

    def multishot(self):
        R0 = self.ready()
        up_cant = V(0.22, 0.02, 1.0).normalized()
        Rs = self.pose(R0, pelvis=R0.pelvis + V(0, 0.0, 0.01), lean=6.0, twist=-34.0, hip_yaw=-18.0, head_yaw=40.0,
                       head_pitch=-4.0, foot_r=self.web_foot(52.0, -0.17), foot_l=self.web_foot(42.0, 0.15),
                       foot_r_rot=(0, 0, 40), foot_l_rot=(0, 0, 10),
                       hand_l=V(0.16, 0.44, 1.14), elbow_l=V(0.6, -0.3, -0.7), off=up_cant, off_up=V(0, 1, 0),
                       hand_r=V(0.00, 0.36, 1.14), elbow_r=V(-0.7, -0.3, -0.5),
                       wpn=V(-0.6, -0.35, -0.7).normalized(), wpn_up=V(0, 0, 1),
                       clav_l=(8.0, 14.0), clav_r=(10.0, 6.0), flow=0.3, cling=1.0)
        Dr = self.pose(R0, pelvis=R0.pelvis + V(0, 0.0, 0.0), lean=4.0, twist=-48.0, hip_yaw=-24.0, head_yaw=66.0,
                       head_pitch=-2.0, head_roll=-6.0,
                       foot_r=self.web_foot(51.5, -0.18), foot_l=self.web_foot(43.0, 0.16),
                       foot_r_rot=(0, 0, 50), foot_l_rot=(0, 0, 14),
                       hand_l=V(0.0, 0.64, 1.13), elbow_l=V(0.7, -0.2, -0.7), off=up_cant, off_up=V(0, 1, 0),
                       hand_r=V(-0.22, 0.14, 1.07), elbow_r=V(-0.6, -0.8, 0.2),
                       wpn=V(-0.6, -0.3, -0.72).normalized(), wpn_up=V(0, 0, 1),
                       clav_l=(10.0, 20.0), clav_r=(14.0, -12.0), flow=0.35, cling=1.0)
        Rl = self.pose(Dr, lean=8.0, head_pitch=0.0, hand_r=V(-0.28, -0.06, 1.08), elbow_r=V(-0.6, -0.8, 0.2),
                       wpn=V(-0.65, -0.3, -0.68).normalized(), hand_l=V(0.02, 0.66, 1.15), clav_r=(16.0, -18.0),
                       flow=0.7)
        H = self.pose(Rl, lean=7.0, hand_r=V(-0.29, -0.04, 1.05), hand_l=V(0.03, 0.62, 1.12), flow=0.4)
        Out = self.pose(R0, hand_l=V(0.22, 0.30, 1.02), elbow_l=V(0.8, -0.3, -0.5), flow=0.3)
        keys = [K(0.0, R0), K(0.2, Rs, "out"), K(0.36, Dr), K(self.REL, Rl, "in"), K(0.72, H),
                K(0.9, Out), K(1.0, R0)]
        return self.clip("Cast_Multishot", 533.0, keys, self.SPAN_C,
                         notifies={"FX_BowShow": self.BOW_SHOW, "Release": 246.0, "FX_BowHide": self.BOW_HIDE})

    # ── Hurt 267 ms (web HURT; keys over 200 ms) + HurtAdd ──────────────────────────────────────────────
    def hit_pose(self) -> BP:
        R0 = self.ready()
        return self.pose(R0, pelvis=self.web_pelvis(43.5, 67.0), lean=-11.5, hip_pitch=-3.0, head_pitch=-24.0,
                         head_yaw=6.0, twist=0.0, foot_r=self.web_foot(54.0, -0.15), foot_l=self.web_foot(39.0, 0.13),
                         hand_r=V(-0.30, 0.08, 0.92), elbow_r=V(-0.8, -0.2, -0.5), **self.dkw(52.0, -0.5),
                         hand_l=V(0.22, -0.04, 1.06), elbow_l=V(0.8, 0.2, -0.5), **self.okw(11.0, 0.6),
                         clav_r=(10.0, -4.0), clav_l=(12.0, -6.0), flow=0.5)

    def hurt(self):
        R0, Hp = self.ready(), self.hit_pose()
        mid = self.pose(Hp, pelvis=self.web_pelvis(43.0, 67.4), lean=-5.7, head_pitch=-17.0, flow=0.4)
        keys = [K(0.0, Hp), K(0.33, mid), K(1.0, R0)]
        return self.clip("Hurt", 267.0, keys, 200.0)

    def hurt_add(self):
        R0 = self.ready()
        J = self.pose(R0, lean=R0.lean - 10.0, head_pitch=R0.head_pitch - 14.0, twist=R0.twist + 6.0,
                      hand_r=R0.hand_r + V(-0.03, -0.05, 0.06), hand_l=R0.hand_l + V(0.02, -0.05, 0.05),
                      clav_r=(6.0, -3.0), clav_l=(6.0, 0.0), flow=0.3)
        keys = [K(0.0, R0), K(60.0 / 267.0, J, "out"), K(1.0, R0, "smooth")]
        return self.clip("HurtAdd", 267.0, keys, 267.0, additive=True)

    # ── Dodge 240 ms: forward tuck roll about mid-spine (web DODGE :509-531; keys over 200 ms) ────────────
    def with_pivot(self, bp: BP, k: float = 0.5) -> BP:
        P = self.J["pelvis"] + bp.pelvis
        a = math.radians(bp.lean + bp.hip_pitch)
        return bp.but(pivot=P + V(0, math.sin(a), math.cos(a)) * (self.rig.spec.torso * k))

    def tuck(self, x: float, y: float, spin: float) -> BP:
        R0 = self.ready()
        return self.pose(R0, pelvis=self.web_pelvis(x, y), lean=43.0, hip_pitch=6.0, head_pitch=29.0, twist=0.0,
                         hip_yaw=0.0, head_yaw=0.0,
                         foot_r=self.web_foot(x + 5, -0.11, y + 10), foot_l=self.web_foot(x + 2.5, 0.11, y + 10.5),
                         foot_r_rot=(30.0, 0, 0), foot_l_rot=(30.0, 0, 0), knee_r=V(-0.2, 1, 0), knee_l=V(0.2, 1, 0),
                         hand_r=self.web_hand(x + 6, y - 1, -0.26), elbow_r=V(-0.9, -0.2, -0.4),
                         **self.dkw(30.0, -0.7),
                         hand_l=self.web_hand(x + 5, y - 4, 0.24), elbow_l=V(0.9, -0.2, -0.4), **self.okw(30.0, 0.7),
                         spin=spin, spin_axis=V(1, 0, 0), flow=0.85)

    def dodge(self):
        R0 = self.ready()
        d0 = self.pose(R0, pelvis=self.web_pelvis(48.0, 71.5), lean=34.0, head_pitch=11.0, twist=0.0, hip_yaw=0.0,
                       head_yaw=0.0, foot_r=self.web_foot(56.0, -0.15), foot_l=self.web_foot(42.0, 0.13),
                       hand_r=self.web_hand(57.0, 66.0, -0.30), **self.dkw(60.0, -0.9),
                       hand_l=self.web_hand(55.0, 66.0, 0.22), **self.okw(103.0, 0.5),
                       spin_axis=V(1, 0, 0), flow=0.4)
        d8 = self.pose(R0, pelvis=self.web_pelvis(49.5, 70.0), lean=26.0, head_pitch=4.0, twist=0.0, hip_yaw=0.0,
                       head_yaw=0.0, foot_r=self.web_foot(57.0, -0.15), foot_l=self.web_foot(43.0, 0.13),
                       hand_r=self.web_hand(58.0, 66.0, -0.28), **self.dkw(97.0, -0.4),
                       hand_l=self.web_hand(54.0, 60.0, 0.24), **self.okw(63.0, 0.45),
                       spin=347.0, spin_axis=V(1, 0, 0), flow=0.5)
        keys = [K(0.0, self.with_pivot(d0)), K(0.2, self.with_pivot(self.tuck(47.0, 72.0, 80.0))),
                K(0.4, self.with_pivot(self.tuck(48.0, 70.5, 172.0)), "linear"),
                K(0.6, self.with_pivot(self.tuck(48.5, 71.5, 264.0)), "linear"),
                K(0.8, self.with_pivot(d8)), K(1.0, self.with_pivot(R0.but(spin=360.0, spin_axis=V(1, 0, 0))))]
        gr = self.grounded()
        return self.clip("Dodge", 240.0, keys, 200.0, post=lambda bp, t: gr(self.fit_daggers(bp), t))

    @staticmethod
    def fall_spin(pitch: float, roll: float) -> dict:
        """Fall backward (``pitch`` about X) then roll toward the left side by ``roll`` (about the world forward axis):
        one axis-angle for BP.spin."""
        q = Quaternion((0, 1, 0), math.radians(roll)) @ Quaternion((1, 0, 0), math.radians(pitch))
        ax = q.axis
        ang = math.degrees(q.angle)
        if ax.x > 0:
            ax, ang = -ax, -ang
        return {"spin": ang, "spin_axis": Vector(ax)}

    # ── Death 545 ms: stagger back → knees → tip over backward → on the back (web DEATH :533-554) ─────────
    def death(self):
        R0, Hp = self.ready(), self.hit_pose()
        d22 = self.pose(Hp, pelvis=self.web_pelvis(43.0, 70.0), lean=-3.0, head_pitch=6.0,
                        hand_r=V(-0.32, 0.04, 0.80), **self.dkw(126.0, -0.5))
        d45 = self.pose(R0, pelvis=self.web_pelvis(45.0, 76.0), lean=7.0, head_pitch=20.0, twist=-4.0, hip_yaw=2.0,
                        foot_r=self.web_foot(54.0, -0.15), foot_l=self.web_foot(36.0, 0.13, 90.5),
                        foot_l_rot=(62.0, 0, 10), toe_l=50.0, knee_l=V(0.2, 1, -0.2),
                        hand_r=V(-0.30, 0.16, 0.50), elbow_r=V(-0.9, -0.2, 0.0), **self.dkw(160.0, -0.5),
                        hand_l=V(0.30, 0.10, 0.56), elbow_l=V(0.9, -0.2, 0.0), **self.okw(137.0, 0.5),
                        flow=0.25)
        P72 = self.J["pelvis"] + self.web_pelvis(45.0, 82.0)
        d72 = self.pose(d45, pelvis=self.web_pelvis(45.0, 82.0), lean=-6.0, head_pitch=-17.0,
                        foot_r=P72 + V(-0.13, 0.22, -0.50), foot_l=P72 + V(0.12, 0.12, -0.55),
                        foot_l_rot=(20.0, 0, 10), toe_l=0.0,
                        hand_r=V(-0.34, 0.08, 0.60), **self.dkw(137.0, -0.6), hand_l=V(0.30, 0.04, 0.62),
                        **self.fall_spin(-54.0, 14.0), flow=0.6)
        P100 = self.J["pelvis"] + self.web_pelvis(47.0, 86.5)
        d100 = self.pose(d45, pelvis=self.web_pelvis(47.0, 86.5), lean=4.0, head_pitch=-10.0, head_roll=-24.0,
                         head_yaw=10.0, twist=-4.0, hip_yaw=0.0, hip_pitch=-4.0,
                         foot_r=P100 + V(-0.14, -0.02, -0.74), foot_l=P100 + V(0.30, 0.04, -0.62),
                         foot_r_rot=(25.0, 0, 30), foot_l_rot=(10.0, 0, 50), toe_l=0.0, knee_l=V(1.0, 0.45, 0),
                         knee_r=V(-0.2, 1, 0),
                         hand_r=P100 + V(-0.50, -0.04, 0.30), elbow_r=V(-0.4, -0.3, -1.0),
                         wpn=V(-0.70, 0.05, -0.70).normalized(), wpn_up=V(0, 0, 1),
                         hand_l=P100 + V(0.44, -0.04, 0.22), elbow_l=V(1.0, -0.2, -0.6),
                         off=V(0.6, 0.78, 0.1).normalized(), off_up=V(0, 0, 1),
                         **self.fall_spin(-87.0, 32.0), flow=0.05)
        keys = [K(0.0, Hp), K(0.22, d22), K(0.45, d45), K(0.72, d72, "in"), K(1.0, d100, "out")]
        gr = self.grounded(snap=True)
        return self.clip("Death", 545.0, keys, 454.5, post=lambda bp, t: gr(self.fit_daggers(bp), t))

    # ── Portrait (static, spec §3.4 / §9.5): a blade raised beside the hood ─────────────────────────────
    def portrait(self):
        R0 = self.ready()
        P = self.pose(R0, hand_r=V(-0.17, 0.20, 1.20), elbow_r=V(-0.8, -0.2, -0.5), **self.dkw(12.0, -0.25),
                      hand_l=V(0.20, 0.24, 1.00), elbow_l=V(0.8, -0.2, -0.5), **self.okw(70.0, 0.5),
                      head_yaw=6.0, head_pitch=2.0, twist=-6.0, lean=8.0, clav_r=(12.0, 6.0), flow=0.2)
        keys = [K(0.0, P), K(1.0, P)]
        return self.clip("Portrait", 1000.0 / 60.0, keys, 1000.0 / 60.0, layer=lambda bp, t: bp)

    def all_clips(self):
        return [self.idle(), self.walk(), self.run(), self.attack01(), self.attack02(), self.attack03(),
                self.cast01(), self.cast02(), self.multishot(), self.hurt(), self.hurt_add(), self.dodge(),
                self.death(), self.portrait()]

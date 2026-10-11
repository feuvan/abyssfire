"""Mage poses and clips (spec art-inventory-ch1.md §3.2 / §3.4 / §3.5, §10; DECISIONS R5/R9). Web keys:
``src/graphics/sprites/players/PlayerMage.ts:472-628`` (READY, ATTACK, CAST, HURT, DODGE, DEATH, idle / walk).

Timings (spec §10): Idle 750 loop · Walk 1000 @ 1.2 m/s · Run 720 @ 3.333 m/s (no-slide gait, FootL/R) ·
Attack01-03 533 (Contact 267; keys over 467 ms) · Cast01 / Cast02 / Cast_Blizzard / Cast_Meteor 500 (Release 230;
keys over 437.5 ms) · Hurt 308 (+ HurtAdd) · Dodge 273 · Death 667 · Portrait.

The 3D poses add what one side view cannot say: the staff held out at the right side (web ``zN`` 5 u, ``STAFF_LAT``
0.3, so the crystal stands beside the hood from every facing), the off hand's spell gestures, chest twist, foot
turn-out. Every contact / release has its own silhouette (Attack01 diagonal overhead bash, Attack02 level backhand
sweep, Attack03 lunging crystal thrust; Cast01 staff + palm thrust, Cast02 staff planted + palm raised, Blizzard both
arms flung wide, Meteor the staff driven down in a crouch).

Cloth: the robe skirt's four chains (``skirt_f/b/l/r``) hang plumb, stream with ``flow`` and are pushed out by the
legs under them (an envelope of the skinned leggings and boots, distributed by each chain's angular weight); sash
tails, hood tip, beard and tome hang from their parents with secondary lag, stay off the body and off the floor.
"""
from __future__ import annotations

import math

import numpy as np
from mathutils import Vector

from kit import anim
from kit.anim import Pose

import common as C
from common import BP, K, V
import hero_base as HB
from hero_base import RUN_SPEED, WALK_SPEED


class MageAnims(HB.HeroAnims):
    GRIPS = ("r",)
    CHAINS = ("skirt_f", "skirt_b", "skirt_l", "skirt_r", "sash_a", "sash_b", "hood_tip", "beard", "tome",
              "sleeve_l", "sleeve_r")
    SLEEVE_SAG = 28.0          # deg: max droop of a bell's flared half toward world down (bell clearance)
    SLEEVE_SLIDE = 0.085       # m: how far the bell slides toward the elbow on a vertical forearm
    FEET = {"l": ("boot_l", "boot_cuff_l"), "r": ("boot_r", "boot_cuff_r")}
    ITEMS = (("SM_Hero_Mage_Staff", "weapon_r"),)
    CLOTH_PARTS = ("skirt", "lining", "rune", "hem", "robe_seam", "skirt_fold", "skirt_edge", "sash_tail", "hood_tip",
                   "tome", "beard")
    ON_CLOTH = ("robe", "closure", "sash", "sash_knot", "leg", "mantle", "mantle_trim")
    ON_CLOTH_CLEAR = 0.012
    LEG_PARTS = ("leg", "boot", "boot_cuff")
    TORSO_PARTS = ("mantle", "mantle_trim", "clasp", "clasp_gem", "robe", "closure", "sash", "sash_knot")
    SKIRT_CLEAR = 0.045        # leg surface → skirt mid-surface (cloth 4 mm + the linear-blend corner cut at the joint)
    # (web skirtSection: the hem always rounds both feet — a = 5.5 + 0.45 × spread): the whole leg incl. the boots
    # pushes the cloth; HEM_FADE only drops points below the hem
    HEM_FADE = (0.96, 1.04)
    SKIRT_MAX = 80.0           # max outward angle of a skirt link (deg from the pelvis' down axis)
    SKIRT_FLOOR = 0.07         # chain ends this high at least: the ring between chains and the hem lens hang lower
    # a spun body (the face-down Death): each link turns this share of the way toward world down — the robe stays
    # wrapped round the straight legs (a long robe lying on prone legs), the top settling onto them, the sides
    # drooping to the floor; the floor and leg tests then stop it
    # per chain (link 1, link 2): the top panel (b) settles onto the buttocks / thighs with its upper link and keeps
    # the hem link along the calves, so the hem rests just over the boots (a hem pulled onto them cut the calves)
    SKIRT_FALL = {"f": (0.22, 0.34), "b": (0.22, 0.34), "l": (0.22, 0.34), "r": (0.22, 0.34)}
    SKIRT_FALL_IN = 0.25
    SKIRT_WRAP_LR = (0.0, 0.0)
    SKIRT_SPUN_BOUND = False
    SKIRT_FALL_LR = (0.25, 0.45)
    SKIRT_FALL_CLEAR = 0.02
    SKIRT_FLOOR_SPUN = 0.012

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
        # skirt radius table (k, angle) for the envelope
        self._ks = np.linspace(0.0, 1.0, 21)
        self._as = np.linspace(-180.0, 180.0, 73)
        self._rt = np.array([[math.hypot(*tuple(MG.skirt_point(k, a))[:2]) for a in self._as] for k in self._ks])

    # ── skinned obstacles ───────────────────────────────────────────────────────────────────────────────
    STAFF_PARTS = ("robe", "closure", "mantle", "mantle_trim", "clasp", "hood", "hood_tip", "beard", "sash",
                   "sash_knot", "skirt", "hem", "leg_l", "leg_r", "boot_l", "boot_r", "sleeve_l", "bell_l",
                   "cuff_band_l", "hand_l", "tome", "tome_cord", "sleeve_r", "bell_r", "cuff_band_r", "face")
    STAFF_CLEAR = 0.012
    STAFF_MAX_TURN = 45.0      # deg: the fit never turns the staff further from the authored key than this
    STAFF_HEAD_R = 0.150       # m: the claw head's radius round the crystal (a staff lying on the floor rests on it)

    def make_env(self, body, names):
        legs = C.SkinPoints(self.rig).add_mesh(body, names, lambda n: n.rstrip("_lr") in self.LEG_PARTS, step=1)
        self.legs = legs
        self.torso_env = C.SkinPoints(self.rig).add_mesh(body, names, lambda n: n in self.TORSO_PARTS, step=3)
        cloth = ("skirt", "hem")      # the big skirt: every vertex (a 10 cm sample gap let the shaft slip through)
        self.staff_env = (C.SkinPoints(self.rig)
                          .add_mesh(body, names, lambda n: n in self.STAFF_PARTS and n not in cloth, step=2)
                          .add_mesh(body, names, lambda n: n in cloth, step=1))
        self.tome_pts = C.SkinPoints(self.rig).add_mesh(body, names, lambda n: n.startswith("tome") and n != "tome_cord")
        self.skirt_pts = C.SkinPoints(self.rig).add_mesh(body, names, lambda n: n in ("skirt", "hem"), step=2)
        self.staff_near = self.staff_env.mask(["sleeve_r", "bell_r", "cuff_band_r"])
        # the bell's flared rim stands ~11 cm off the forearm: only its points within 3 cm of the grip (the fist in
        # the cuff) are the hand's own; beyond that a shaft tilted toward the elbow cuts the bell
        self.staff_bell = self.staff_env.mask(["bell_r", "cuff_band_r"])
        import mage as MG
        U = MG.U
        self.staff_s = np.linspace(-MG.STAFF_DOWN * U, (MG.CRYSTAL_AT + 4.6) * U, 44)
        # shaft radius ~1.3 u, the claw head ± 4.5 u round the crystal
        self.staff_r = np.where(self.staff_s > (MG.STAFF_UP - 3.0) * U, 4.6 * U, 1.35 * U)
        return legs

    LEFT_ON_STAFF = ("hand_l", "cuff_band_l", "bell_l", "sleeve_l")

    def swivel_staff_arm(self, bp: BP) -> BP:
        """Choose the right elbow on its IK circle so the forearm is as perpendicular to the staff as the arm allows
        (a hammer grip holds the shaft across the fist: a shaft along the forearm runs through the bell sleeve),
        staying near the authored elbow and never folding the elbow into the body."""
        if abs(bp.spin) > 1e-6 or bp.hand_r is None:
            return bp
        from mathutils import Quaternion
        p = C.to_pose(self.rig, bp.but(hand_r=None, hand_l=None), None, grips=())
        mats = anim.evaluate(p, return_matrices=True)
        S = mats["upperarm_r"].translation.copy()
        T = bp.hand_r + V(0, 0, bp.lift)
        l1, l2 = self.bones["upperarm_r"].length, self.bones["lowerarm_r"].length
        w = T - S
        D = max(abs(l1 - l2) + 1e-4, min(l1 + l2 - 1e-5, w.length))
        u = w.normalized()
        a = (l1 * l1 - l2 * l2 + D * D) / (2 * D)
        b = math.sqrt(max(l1 * l1 - a * a, 0.0))
        v0 = bp.elbow_r - u * bp.elbow_r.dot(u)
        if v0.length < 1e-6 or b < 1e-4:
            return bp
        v0.normalize()
        d = bp.wpn.normalized()
        best, bc = v0, 1e9
        for k in range(-24, 25):
            th = math.radians(k * 7.5)
            v = Quaternion(u, th) @ v0
            E = S + u * a + v * b
            f = ((S + u * D) - E).normalized()
            cost = abs(f.dot(d)) + 0.35 * (th / math.pi) ** 2 + 0.8 * max(0.0, v.x - 0.1) + 0.3 * max(0.0, -v.y - 0.5)
            if cost < bc:
                best, bc = v, cost
        return bp.but(elbow_r=best)

    def _staff_obstacles(self, bp: BP, d: Vector):
        """Posed obstacle points + the grip for the staff fit (cloth solved with the current staff direction)."""
        p = C.to_pose(self.rig, bp.but(wpn=d), self.cloth, grips=self.GRIPS)
        mats = anim.evaluate(p, return_matrices=True)
        g = np.array(mats["weapon_r"].translation)
        pts = self.staff_env.posed(mats)
        return g, pts, mats

    def _staff_pen(self, g, pts, d: np.ndarray, two=None, left=None, floor=None) -> float:
        """Deepest penetration (m) of the staff (shaft + claw head) along ``d`` from grip ``g`` into the obstacle
        points or the floor; ≤ 0 = clear."""
        s_all = self.staff_s
        S = g[None, :] + d[None, :] * s_all[:, None]
        dist = np.linalg.norm(S[:, None, :] - pts[None, :, :], axis=2)
        pen = self.staff_r[:, None] + self.STAFF_CLEAR - dist
        near = (np.abs(s_all)[:, None] < np.where(self.staff_bell, 0.03, 0.075)[None, :]) & self.staff_near[None, :]
        if two is not None:
            near |= (np.abs(s_all - two)[:, None] < 0.13) & left[None, :]
        pen = np.where(near | (np.abs(s_all)[:, None] < 0.05), -1.0, pen)
        worst = float(pen.max())
        if floor is not None:
            world_z, = floor
            worst = max(worst, float((self.staff_r + 0.005 - world_z(S)).max()))
        return worst

    def fit_staff(self, bp: BP, t: float = 0.0) -> BP:
        """Post step: (1) swivel the elbow for a perpendicular grip; (2) if the authored staff cuts the skinned
        robe, hood, beard, sleeves, legs, tome or the floor, search a cone of directions round it (rings of 4°
        up to ``STAFF_MAX_TURN``, 16 azimuths, the body evaluated once — the shaft direction barely moves it) and
        take the least deviation that clears by ``STAFF_CLEAR``, then verify on the re-solved pose. A left hand
        resting on the shaft (two-handed keys) is carried along."""
        if self.skin() is None:
            return bp
        from mathutils import Quaternion
        bp = self.swivel_staff_arm(bp)
        d0 = bp.wpn.normalized()
        two = None
        if bp.hand_l is not None and bp.hand_r is not None:
            rel = bp.hand_l - bp.hand_r
            sl = rel.dot(d0)
            if (rel - d0 * sl).length < 0.09 and -0.6 < sl < 0.6:
                two = (sl, rel - d0 * sl)
        left = np.array(self.staff_env.mask(self.LEFT_ON_STAFF))
        floor = None
        if abs(bp.spin) > 1e-6 or bp.lift < -1e-6 or bp.pelvis.z < -0.12:
            wf = self.world_fn(C.to_pose(self.rig, bp, None, grips=self.GRIPS), bp)
            world, _, Rm = wf
            Rn = np.array(Rm)
            P = np.array(bp.pivot + Vector((0, 0, bp.lift))) if abs(bp.spin) > 1e-6 else np.zeros(3)
            floor = (lambda S: ((S - P) @ Rn.T + P)[:, 2],)
        else:
            floor = (lambda S: S[:, 2],)

        def with_d(d):
            out = bp.but(wpn=d)
            if two is not None:
                out = out.but(hand_l=out.hand_r + d * two[0] + two[1])
            return out
        g, pts, _ = self._staff_obstacles(with_d(d0), d0)
        tw = two[0] if two else None
        pen0 = self._staff_pen(g, pts, np.array(d0), tw, left, floor)
        if pen0 <= 0.0:
            return with_d(d0)
        a = d0.orthogonal().normalized()
        b_ = d0.cross(a).normalized()
        best = (pen0, d0)
        for deg in range(4, int(self.STAFF_MAX_TURN) + 1, 4):
            found = None
            for k in range(16):
                ph = 2 * math.pi * k / 16
                ax = a * math.cos(ph) + b_ * math.sin(ph)
                d = (Quaternion(ax, math.radians(deg)) @ d0).normalized()
                v = self._staff_pen(g, pts, np.array(d), tw, left, floor)
                if v < best[0]:
                    best = (v, d)
                if v <= -0.004 and (found is None or v < found[0]):
                    found = (v, d)
            if found is not None:
                best = found
                break
        d = best[1]
        # verify on the re-solved pose (the grip and sleeves move a little with the shaft's roll); one more ring
        # search round the result if a residual remains
        g2, pts2, _ = self._staff_obstacles(with_d(d), d)
        if self._staff_pen(g2, pts2, np.array(d), tw, left, floor) > 0.0:
            a = d.orthogonal().normalized()
            b_ = d.cross(a).normalized()
            for deg in (3, 6, 9):
                cand = [(self._staff_pen(g2, pts2, np.array((Quaternion(a * math.cos(ph) + b_ * math.sin(ph),
                                                                        math.radians(deg)) @ d).normalized()),
                                         tw, left, floor), ph) for ph in np.linspace(0, 2 * math.pi, 12,
                                                                                    endpoint=False)]
                v, ph = min(cand)
                if v <= 0.0:
                    d = (Quaternion(a * math.cos(ph) + b_ * math.sin(ph), math.radians(deg)) @ d).normalized()
                    break
        return with_d(d)

    def post(self, bp: BP, t: float = 0.0) -> BP:
        return self.plant(self.fit_staff(bp, t), t)

    def world_fn(self, p: Pose, bp: BP):
        """The robe's hem hangs 10 cm off the floor: any crouch or lunge can put it under, so the floor frame is
        always built (the base class skips it for upright, unspun poses)."""
        mats = anim.evaluate(p, return_matrices=True)
        if abs(bp.spin) > 1e-6:
            from mathutils import Quaternion
            Rm = Quaternion(Vector(bp.spin_axis).normalized(), math.radians(bp.spin)).to_matrix()
            P = bp.pivot + Vector((0, 0, bp.lift))
        else:
            from mathutils import Matrix
            Rm, P = Matrix.Identity(3), Vector()

        def world(x: Vector) -> Vector:
            return P + Rm @ (x - P)
        return world, mats, Rm

    def _gvec(self, bp: BP) -> Vector:
        """World down in the pre-spin armature frame (pelvis rest frame for the skirt solver)."""
        from mathutils import Quaternion
        if abs(bp.spin) < 1e-6:
            return Vector((0, 0, -1))
        return Quaternion(Vector(bp.spin_axis).normalized(), math.radians(bp.spin)).inverted() @ Vector((0, 0, -1))

    def _skirt_r(self, k: np.ndarray, a: np.ndarray) -> np.ndarray:
        fk = np.clip(k, 0.0, 1.0) * (len(self._ks) - 1)
        fa = (np.mod(a + 180.0, 360.0)) / 5.0
        i, j = np.minimum(fk.astype(int), len(self._ks) - 2), np.minimum(fa.astype(int), len(self._as) - 2)
        x, y = fk - i, fa - j
        t = self._rt
        return ((1 - x) * (1 - y) * t[i, j] + x * (1 - y) * t[i + 1, j] + (1 - x) * y * t[i, j + 1]
                + x * y * t[i + 1, j + 1])

    # ── ready (web READY :472-487) ──────────────────────────────────────────────────────────────────────
    STAFF_THETA, STAFF_LAT = -1.0, -0.10         # web wpn 0.1 rad, STAFF_LAT 0.3 (out to the right)

    def staff_kw(self, theta: float, lat: float = 0.0, roll: float = 0.0) -> dict:
        d, e = C.blade(theta, lat=lat, roll=roll)
        return dict(wpn=d, wpn_up=e)

    def ready(self) -> BP:
        return BP(
            pelvis=self.web_pelvis(46.8, 65.0), hip_pitch=1.0, hip_yaw=-4.0, lean=2.3, twist=7.0, head_pitch=-1.5,
            head_yaw=-4.0,
            foot_r=self.web_foot(52.0, -0.10), foot_l=self.web_foot(43.0, 0.105),
            foot_r_rot=(0, 0, 10), foot_l_rot=(0, 0, 24), knee_r=V(-0.15, 1, 0), knee_l=V(0.25, 1, 0),
            hand_r=V(-0.35, 0.14, 1.03), elbow_r=V(-0.5, -0.8, -0.2),
            **self.staff_kw(self.STAFF_THETA, self.STAFF_LAT),
            hand_l=V(0.19, 0.17, 0.86), elbow_l=V(0.8, 0.3, -0.5), wrist_l=(-10.0, 0.0, -20.0),
            clav_r=(3.0, 2.0), flow=0.12,
        )

    # ── cloth ───────────────────────────────────────────────────────────────────────────────────────────
    def _acc(self, mats, parent: str, hinge: Vector, down=Vector((0, 0, -1))) -> float:
        M3 = mats[parent].to_3x3() @ self.bones[parent].matrix_local.to_3x3().inverted()
        Dn = (down - hinge * down.dot(hinge)).normalized()
        Bk = hinge.cross(Dn)
        v = M3 @ Vector((0, 0, -1))
        return math.degrees(math.atan2(v.dot(Bk), v.dot(Dn)))

    def skirt_angles(self, p: Pose, bp: BP, mats) -> dict:
        """Per chain: outward angles per link (deg from the pelvis' down axis, + = away from the body) after flow
        and the leg envelope. Leg points (skinned leggings + boots, in the pelvis rest frame) that the rest skirt
        would not clear by ``SKIRT_CLEAR`` push the chains: the skin weights blend the four chains round the skirt,
        so a point at angle θ moves radially by Σ_c a_c x_c (a_c = normalised angular weight × cos(θ − θ_c));
        each chain takes the minimum-norm share x_c = δ a_c / Σ a² of the point's deficit δ, then
        ``hero_base.envelope`` turns its links just enough to reach every requirement."""
        Pm = mats["pelvis"] @ self.bones["pelvis"].matrix_local.inverted()
        inv = Pm.inverted()
        X = (Pm.to_3x3() @ Vector((1, 0, 0))).normalized()
        acc = self._acc(mats, "pelvis", X)
        ga, gs = self.gravity(bp)
        names = list(self.skirt)
        req = {c: None for c in names}
        spun: dict = {}
        if self.skin() is not None:
            pts = self.legs.posed(mats)
            q = (np.c_[pts, np.ones(len(pts))] @ np.array(inv).T)[:, :3]          # pelvis rest frame
            th = np.degrees(np.arctan2(q[:, 0], -q[:, 1]))
            rho = np.hypot(q[:, 0], q[:, 1])
            k = (self.z0 - q[:, 2]) / (self.z0 - self.z1)
            fade = np.asarray(HB.smooth01((self.HEM_FADE[1] - k) / (self.HEM_FADE[1] - self.HEM_FADE[0])))
            sel = (k > 0.04) & (fade > 0.02)
            if sel.any():
                q, th, rho, k, fade = q[sel], th[sel], rho[sel], np.clip(k[sel], 0.0, 1.0), fade[sel]
                # + extra room round the chain joint (k ≈ .5), where linear blending cuts the bent chain's corner
                clear = self.SKIRT_CLEAR + 0.02 * np.exp(-((k - 0.5) / 0.12) ** 2)
                if gs > 0 and self.SKIRT_SPUN_BOUND:
                    # a spun body: gravity pulls chains *inward* past their rest radius (the top panel of a
                    # face-down robe settling onto the legs), so every leg point in a chain's sector bounds it, not
                    # only the points outside the rest skirt
                    for c in names:
                        dth = np.abs((th - self.skirt[c]["ang"] + 180.0) % 360.0 - 180.0)
                        m = (dth < 40.0) & (fade > 0.3)
                        if m.any():
                            need = rho[m] * np.cos(np.radians(dth[m])) + clear[m] + self.SKIRT_FALL_CLEAR
                            spun[c] = (self.skirt[c]["root"].z - q[m, 2], need - self.skirt[c]["root_r"])
                delta = (rho + clear - self._skirt_r(k, th)) * fade
                hit = delta > 0
                if hit.any():
                    q, th, k, delta = q[hit], th[hit], k[hit], delta[hit]
                    lat = np.stack([HB.smooth01(1.0 - np.abs((th - self.skirt[c]["ang"] + 180.0) % 360.0 - 180.0)
                                                / 90.0) for c in names], 1)
                    wn = lat / np.maximum(lat.sum(1, keepdims=True), 1e-6)
                    cosd = np.stack([np.cos(np.radians(th - self.skirt[c]["ang"])) for c in names], 1)
                    a = np.maximum(wn * cosd, 0.0)
                    x = delta[:, None] * a / np.maximum((a * a).sum(1, keepdims=True), 1e-6)
                    for j, c in enumerate(names):
                        m = a[:, j] > 0.04
                        if m.any():
                            need = self._skirt_r(k[m], np.full(int(m.sum()), self.skirt[c]["ang"])) + x[m, j]
                            req[c] = (self.skirt[c]["root"].z - q[m, 2], need - self.skirt[c]["root_r"])
        for c, r in spun.items():
            req[c] = r if req[c] is None else (np.r_[req[c][0], r[0]], np.r_[req[c][1], r[1]])
        out = {}
        for c, sk in self.skirt.items():
            # streaming: the whole skirt trails back with flow (front chain drawn in, back chain out), the lagged
            # flow at the hem (secondary motion)
            out_ang = list(sk["rest_out"])
            for i in range(2):
                pb = self.lagged(bp, 50.0 * (i + 1))
                fl = pb.flow - 0.12
                stream = math.degrees(fl * 0.62 * (0.5 + 0.5 * (i + 1) / 2))
                wave = math.degrees(math.sin(bp.wave * 2 - 1.3 * (i + 1) + sk["ang"] * 0.02) * 0.022 * (i + 1)
                                    * (0.4 + bp.flow))
                if c == "f":
                    out_ang[i] -= stream * 0.8 - wave
                elif c == "b":
                    out_ang[i] += stream + wave
                else:
                    out_ang[i] += wave * 0.5
            if gs > 0:
                # a spun body (falls): the cloth falls toward the floor in the pre-spin frame — before the leg
                # envelope, so the skirt drapes over the legs instead of through them
                gv = self._gvec(bp)
                dirc = sk["dir"]
                for i in range(2):
                    tgt = math.degrees(math.atan2(gv.dot(dirc), -gv.z))
                    dg = self.arc(tgt - out_ang[i])
                    # toward the body (the top panel of a face-down robe) only settles a little: with four chains the
                    # hem ring between them (linear blend) would cut the legs and boots under it
                    out_ang[i] += dg * gs * self.SKIRT_FALL[c][i] * (1.0 if dg > 0 else self.SKIRT_FALL_IN)
                    if c in "lr":
                        out_ang[i] -= gs * self.SKIRT_WRAP_LR[i]     # the sides fold in round the legs
            if req[c] is not None:
                dn, bk = req[c]
                out_ang = HB.envelope(out_ang, sk["lengths"], dn, bk, 0.0, max_deg=self.SKIRT_MAX, slack=0.0)
            out[c] = out_ang
        return {"acc": acc, "out": out, "ga": ga, "gs": gs}

    def set_skirt(self, p: Pose, bp: BP, sk_res: dict, wf) -> None:
        acc, ga, gs = sk_res["acc"], sk_res["ga"], sk_res["gs"]
        for c, sk in self.skirt.items():
            info = self.chains[f"skirt_{c}"]
            out_ang = sk_res["out"][c]
            if c in "fb":
                sgn = -1.0 if c == "f" else 1.0           # outward = forward for f (−), back for b (+)
                absang = [sgn * a + acc for a in out_ang]
                # face down, the front panel is trapped under the legs: it lies flat on the floor
                fl = self.SKIRT_FLOOR
                if c == "f" and gs > 0:
                    k = min(1.0, max(0.0, (abs(bp.spin) - 50.0) / 30.0))     # only once nearly flat
                    fl += (self.SKIRT_FLOOR_SPUN - self.SKIRT_FLOOR) * k
                absang = self.above_ground(info, absang, wf, "pelvis", fl)
                C.set_chain(p, info, absang, acc)
            else:
                fl = self.lagged(bp, 60.0).flow - 0.12
                absang = [info.rest[i] + math.degrees(fl * 0.5 * (0.6 + 0.4 * (i + 1) / 2)) for i in range(2)]
                if gs > 0:
                    for i in range(2):
                        d = self.arc(ga - absang[i])
                        absang[i] += d * gs * self.SKIRT_FALL_LR[i]
                extra = [out_ang[i] - sk["rest_out"][i] for i in range(2)]
                sg = -1.0 if c == "l" else 1.0          # roll that tips a downward bone outward
                rel = [sg * extra[0], sg * (extra[1] - extra[0])]
                # the floor test sees the outward roll too (+x = left): a flared side chain reaches lower
                absang = self.above_ground(info, absang, wf, "pelvis", self.SKIRT_FLOOR,
                                           side=[-sg * extra[0], -sg * (extra[1] - extra[0])])
                C.set_chain(p, info, absang, acc, rel)

    def cloth(self, p: Pose, bp: BP) -> None:
        mats = anim.evaluate(p, return_matrices=True)
        wf = self.world_fn(p, bp)
        sk = self.skirt_angles(p, bp, mats)
        self.set_skirt(p, bp, sk, wf)
        acc = sk["acc"]
        ch = self.chains
        gs = sk["gs"]
        # sash tails: hang behind the skirt, never inside its back (the back chain's angles + a margin)
        skp = None
        if self.skin() is not None:
            m2 = anim.evaluate(p, return_matrices=True)        # the skirt is solved: its posed surface
            skp = self.skirt_pts.posed(m2)
        for nm, ph in (("sash_a", 0.0), ("sash_b", 0.9)):
            info = ch[nm]
            want = self.hang(bp, info, gain=1.05, lag_ms=70.0, wave_amp=0.07, phase=ph)
            if skp is not None:
                # the tails lie on the skirt's back, never inside it: envelope of the posed skirt surface in the
                # tail's plane (points within ±4 cm of the tail laterally)
                root, Dn, B, X, _ = self.plane(m2, "pelvis", info.names[0])
                rel = skp - np.array(root)
                lat = rel @ np.array(X)
                sel = np.abs(lat) < 0.04
                if sel.any():
                    want = HB.envelope(want, self.chain_lengths(info), rel[sel] @ np.array(Dn),
                                       rel[sel] @ np.array(B), 0.02)
            want = self.above_ground(info, want, wf, "pelvis", 0.012)
            C.set_chain(p, info, want, acc, [1.5 * bp.flow * (1 if nm == "sash_a" else -1)] * 3)
        # tome: a pendulum at the right hip (lagged swing, streams back with flow), pushed out by the skirt
        info = ch["tome"]
        pb = self.lagged(bp, 90.0)
        swing = math.degrees(math.sin(bp.wave + 0.4) * 0.10) + math.degrees((pb.flow - 0.12) * 0.55)
        absang = [info.rest[0] + swing]
        if gs > 0:
            d = self.arc(sk["ga"] - absang[0])
            absang[0] += d * gs * 0.85
        absang = self.above_ground(info, absang, wf, "pelvis", 0.03)
        ext_f = max(0.0, sk["out"]["f"][0] - self.skirt["f"]["rest_out"][0])
        ext_r = max(0.0, sk["out"]["r"][0] - self.skirt["r"]["rest_out"][0])
        ext = 0.35 * ext_f + 0.8 * ext_r
        a = math.radians(self.MG.TOME_A)
        C.set_chain(p, info, absang, acc)
        p.rot("tome_01", pitch=-ext * abs(math.cos(a)) * 0.9, roll=ext * abs(math.sin(a)))
        self._tome_off_staff(p)
        self._hood_tip(p, bp, wf)
        self._beard(p, bp, wf)
        self._sleeves(p, bp)

    def _sleeves(self, p: Pose, bp: BP) -> None:
        """Bell sleeves: the flared half sags toward world down (more the further the forearm points from down,
        at most ``SLEEVE_SAG``) and trails a little with the flow."""
        from mathutils import Quaternion
        mats = anim.evaluate(p, return_matrices=True)
        if abs(bp.spin) > 1e-6:
            g = Quaternion(Vector(bp.spin_axis).normalized(), math.radians(bp.spin)).inverted() @ Vector((0, 0, -1))
        else:
            g = Vector((0, 0, -1))
        back = Vector((0, 1, 0))
        for side in ("l", "r"):
            la = mats[f"lowerarm_{side}"].to_3x3()
            a = (la @ Vector((0, 1, 0))).normalized()
            th = a.angle(g)
            sag = min(math.radians(self.SLEEVE_SAG), 0.40 * th)
            ax = a.cross(g)
            d = (Quaternion(ax.normalized(), sag) @ a) if ax.length > 1e-6 else a
            pb = self.lagged(bp, 50.0)
            fl = max(0.0, pb.flow - 0.12)
            ax2 = d.cross(back)
            if ax2.length > 1e-6:
                d = Quaternion(ax2.normalized(), math.radians(10.0 * fl)) @ d
            up = la @ Vector((0, 0, 1))
            # a raised forearm: the wide sleeve slides down toward the elbow (uncovering the under-sleeve), so the
            # cuff never stands open round a raised fist (or a staff held upright in it)
            rise = max(0.0, -a.dot(g))
            if abs(bp.spin) > 1e-6:
                # face down, an arm flung out past the hood: the sleeve rode back toward the elbow in the fall
                ga, gs = self.gravity(bp)
                rise = max(rise, gs)
            slide = self.SLEEVE_SLIDE * rise ** 0.8
            p.aim(f"sleeve_{side}_01", d, up - d * up.dot(d))
            # always keyed (+1 µm): a clip without the slide must not inherit the previous action's location
            # (kit bake writes location curves only for bones that move)
            p.move(f"sleeve_{side}_01", -a * (slide + 1e-6))

    TOME_STAFF_CLEAR = 0.02

    def _tome_off_staff(self, p: Pose) -> None:
        """The staff brushes the hanging tome aside: if the shaft passes through the book, the tome pendulum turns
        about its hook (the smallest swing of a ±50° grid) until it clears the shaft by ``TOME_STAFF_CLEAR``."""
        if self.skin() is None:
            return
        from mathutils import Quaternion
        mats = anim.evaluate(p, return_matrices=True)
        T = self.tome_pts.posed(mats)
        Wm = mats["weapon_r"]
        g = np.array(Wm.translation)
        dd = np.array((Wm.to_3x3() @ Vector((0, 1, 0))).normalized())
        lo, hi = self.staff_s[0], self.staff_s[-1]
        r = 1.35 * self.U + self.TOME_STAFF_CLEAR

        def clear(pts):
            t = np.clip((pts - g) @ dd, lo, hi)
            return float(np.linalg.norm(pts - (g + t[:, None] * dd[None, :]), axis=1).min()) - r
        if clear(T) >= 0.0:
            return
        H = np.array(mats["tome_01"].translation)
        Pm = (mats["pelvis"].to_3x3() @ self.bones["pelvis"].matrix_local.to_3x3().inverted()).to_quaternion()
        # only swings that carry the book outward (or along the skirt): never into the robe
        P0 = np.array(mats["pelvis"].translation)
        up = np.array(Pm @ Vector((0, 0, 1)))
        cen = T.mean(0)
        rad = (cen - P0) - up * ((cen - P0) @ up)
        rad /= max(np.linalg.norm(rad), 1e-6)
        best = None
        for mag in range(5, 55, 5):
            for k in range(12):
                ax = Vector((math.cos(2 * math.pi * k / 12), math.sin(2 * math.pi * k / 12), 0.0))
                q = Quaternion(Pm @ ax, math.radians(mag))
                Rm = np.array(q.to_matrix())
                Tq = H + (T - H) @ Rm.T
                if (Tq.mean(0) - cen) @ rad < -0.004:
                    continue
                c = clear(Tq)
                if c >= 0.0 and (best is None or c < best[0]):
                    best = (c, q)
            if best is not None:
                break
        if best is None:
            return
        q_rel = Pm.inverted() @ best[1] @ Pm
        p.rel["tome_01"] = q_rel @ p.rel.get("tome_01", Quaternion())

    def _hood_tip(self, p: Pose, bp: BP, wf) -> None:
        """The liripipe hangs from the back of the hood, rides partly with the head (a stiff hood), streams with the
        flow, and is kept off the mantle and the robe's back (envelope of the skinned torso)."""
        info = self.chains["hood_tip"]
        mats = anim.evaluate(p, return_matrices=True)
        Hm = mats["head"] @ self.bones["head"].matrix_local.inverted()
        X = (Hm.to_3x3() @ Vector((1, 0, 0))).normalized()
        acc = self._acc(mats, "head", X)
        free = self.hang(bp, info, gain=0.9, lag_ms=80.0, wave_amp=0.05, phase=0.4, acc_w=(0.3, 0.45))
        want = [0.55 * (info.rest[i] + acc) + 0.45 * free[i] for i in range(2)]
        if self.skin() is not None:
            root, Dn, B, _, _ = self.plane(mats, "head", "hood_tip_01")
            pts = self.torso_env.posed(mats)
            rel = pts - np.array(root)
            dn, bk = rel @ np.array(Dn), rel @ np.array(B)
            # the envelope works in the chain plane's angles (deg from D toward B, + = back)
            want = HB.envelope(want, self.chain_lengths(info), dn, bk, 0.03)
        want = self.above_ground(info, want, wf, "head", 0.02)
        C.set_chain(p, info, want, acc)

    def _beard(self, p: Pose, bp: BP, wf) -> None:
        """Stiff-ish: rides with the head, sags a little toward world down, and stays in front of the chest."""
        info = self.chains["beard"]
        mats = anim.evaluate(p, return_matrices=True)
        Hm = mats["head"] @ self.bones["head"].matrix_local.inverted()
        X = (Hm.to_3x3() @ Vector((1, 0, 0))).normalized()
        acc = self._acc(mats, "head", X)
        pb = self.lagged(bp, 60.0)
        fl = pb.flow - 0.12
        want = []
        for i in range(2):
            follow = info.rest[i] + acc
            free = info.rest[i] * 0.4
            a = 0.7 * follow + 0.3 * free + math.degrees(fl * 0.35 * (i + 1) / 2)
            a += math.degrees(math.sin(bp.wave + 0.8 - 1.4 * i) * 0.025 * (0.4 + pb.flow))
            want.append(a)
        ga, gs = self.gravity(bp)
        if gs > 0:
            for i in range(2):
                d = self.arc(ga - want[i])
                want[i] += d * gs * (0.3, 0.55)[i]
        if self.skin() is not None and gs < 0.5:
            root, Dn, B, _, _ = self.plane(mats, "head", "beard_01")
            pts = self.torso_env.posed(mats)
            rel = pts - np.array(root)
            dn, fw = rel @ np.array(Dn), -(rel @ np.array(B))        # forward coordinate
            fwd = HB.envelope([-a for a in want], self.chain_lengths(info), dn, fw, 0.024, max_deg=75.0)
            want = [-a for a in fwd]
        want = self.above_ground(info, want, wf, "head", 0.015)
        C.set_chain(p, info, want, acc)

    # ── shared helpers ──────────────────────────────────────────────────────────────────────────────────
    def staff_at(self, bp: BP, lat: float, fwd: float, up: float, theta: float, slat: float = 0.0,
                 roll: float = 0.0, **kw) -> BP:
        """Staff hand at a chest-relative point (bind chest frame), the staff at web angle ``theta`` (body space)."""
        return bp.but(hand_r=self.chest(bp, lat, fwd, up), **self.staff_kw(theta, slat, roll), **kw)

    def palm(self, bp: BP, lat: float, fwd: float, up: float, **kw) -> BP:
        return bp.but(hand_l=self.chest(bp, lat, fwd, up), **kw)

    # ── Idle (750 ms loop, web idlePose) ────────────────────────────────────────────────────────────────
    def idle(self):
        R0 = self.ready()
        U = self.U

        def s(t: float) -> BP:
            ph = 2 * math.pi * t / 750.0
            b = math.sin(ph)
            return R0.but(
                pelvis=R0.pelvis + V(0, 0, b * 0.5 * U), lean=R0.lean - b * 0.9,
                head_pitch=R0.head_pitch + math.degrees(math.sin(ph - 0.7) * 0.035),
                hand_r=R0.hand_r + V(0, 0, b * 0.35 * U),
                hand_l=R0.hand_l + V(0.0, math.sin(ph) * 0.6 * U, (math.sin(ph - 0.4) * 0.8 - 0.5) * U),
                **self.staff_kw(self.STAFF_THETA + math.degrees(math.sin(ph - 0.3) * 0.025), self.STAFF_LAT),
                clav_r=(R0.clav_r[0] + math.sin(ph - 0.4) * 1.2, R0.clav_r[1]),
                flow=0.12 + math.sin(ph) * 0.06, wave=ph, sway=math.sin(ph + 1.0) * 0.12)
        c = self.proc_clip("Idle", 750.0, s, loop=True)
        c.key_poses_ms = [0.0, 187.5, 375.0, 562.5]
        return c

    # ── locomotion (no foot sliding, R9): Run 720 ms @ 3.333 m/s, Walk 1000 ms @ 1.2 m/s ────────────────
    def locomotion(self, name: str, length: float, speed: float, duty: float, lift: float, bob: float,
                   lean: float, arm: float, flow: float, crouch: float, stance: float = 0.095, walk: bool = False):
        J = self.J
        R0 = self.ready()

        def s(t_ms: float) -> BP:
            t = (t_ms / length) % 1.0
            ((fl, pl, tl), (fr, pr, tr)), dz, swing = self.gait_feet(t, speed, length, duty, lift, bob)
            ph = 2 * math.pi * t
            sw = max(-1.0, min(1.0, swing))
            sp = speed / RUN_SPEED
            if walk:
                # web: the staff planted like a walking stick in rhythm with the stride
                hand_r = V(-0.35, 0.14 + 0.065 * -sw, 1.02 + 0.018 * abs(math.sin(ph)))
                st = self.staff_kw(self.STAFF_THETA + 6.0 * -sw, self.STAFF_LAT)
            else:
                # run: the staff carried at the side, tilted forward, the fist swinging with the stride
                # carried at the side in front of the hanging tome, swinging like a walking stick
                hand_r = V(-0.41, 0.15 + 0.05 * -sw, 0.98 + 0.03 * abs(sw))
                st = self.staff_kw(5.0 + 7.0 * -sw, 0.06)
            bp = BP(
                pelvis=V(0, 0.02 * sp, crouch + dz), hip_pitch=1.5, hip_yaw=-7.0 * sw * sp ** 0.5,
                hip_roll=-2.0 * math.sin(ph), lean=lean + 1.2 * abs(math.sin(ph)), twist=8.0 * sw * sp ** 0.5,
                head_pitch=-lean * 0.55, head_yaw=-3.0 * sw,
                foot_l=J["ankle_l"] + V(stance - J["ankle_l"].x, 0, 0) + fl,
                foot_r=J["ankle_r"] + V(-(stance + J["ankle_r"].x), 0, 0) + fr,
                foot_l_rot=(pl, 0, 6), foot_r_rot=(pr, 0, 6), toe_l=tl, toe_r=tr,
                knee_l=V(0.12, 1, 0), knee_r=V(-0.12, 1, 0),
                hand_r=hand_r, elbow_r=V(-0.5, -0.8, -0.2), **st,
                hand_l=V(0.21, 0.10 + arm * 1.1 * sw, 0.86 + 0.05 * max(0.0, sw)), elbow_l=V(0.8, 0.3, -0.5),
                wrist_l=(-10.0, 0.0, -20.0),
                clav_r=(2.0, 2.0), clav_l=(1.0 * abs(sw), 3.0 * sw),
                flow=flow + 0.08 * math.sin(2 * ph), wave=2 * ph, sway=0.22 * math.sin(ph))
            return bp
        c = self.proc_clip(name, length, s, loop=True, notifies=anim.run_contacts_ms(length), ref_speed=speed)
        c.key_poses_ms = [round(length * i / 8, 1) for i in range(8)]
        return c

    def run(self):
        return self.locomotion("Run", 720.0, RUN_SPEED, duty=0.26, lift=0.09, bob=0.026, lean=8.0, arm=0.10,
                               flow=0.36, crouch=-0.06)

    def walk(self):
        return self.locomotion("Walk", 1000.0, WALK_SPEED, duty=0.62, lift=0.07, bob=0.012, lean=4.0, arm=0.05,
                               flow=0.2, crouch=-0.035, walk=True)

    # ── Attack01: overhead diagonal staff bash (web ATTACK :497-525; 533 ms, Contact 267, keys over 467 ms) ─
    # Contact silhouette: lunge, lean 17°, the staff swept down the strike line (69°: crystal forward at chest
    # height of the target), the off hand pulled back low.
    def attack01(self):
        R0 = self.ready()
        W = self.pose(R0, pelvis=self.web_pelvis(45.0, 65.8), lean=-11.0, hip_pitch=-1.0, head_pitch=3.0,
                      twist=-18.0, hip_yaw=-2.0, foot_r=self.web_foot(55.0, -0.11), foot_l=self.web_foot(41.5, 0.11),
                      hand_r=V(-0.34, -0.10, 1.52), elbow_r=V(-0.9, 0.2, 0.2), **self.staff_kw(-36.0, -0.35),
                      hand_l=V(0.22, 0.30, 1.12), elbow_l=V(0.9, 0.0, -0.5), wrist_l=(-40.0, 0.0, -10.0),
                      clav_r=(22.0, -4.0), flow=0.2)
        W = self.look(W, k=0.6)
        S = self.pose(R0, pelvis=self.web_pelvis(48.5, 66.0), lean=4.6, head_pitch=2.0, twist=-6.0, hip_yaw=4.0,
                      foot_r=self.web_foot(57.5, -0.11), foot_l=self.web_foot(41.5, 0.11),
                      hand_r=V(-0.27, 0.30, 1.40), elbow_r=V(-0.9, -0.2, -0.3), **self.staff_kw(32.0, -0.20),
                      hand_l=V(0.22, 0.20, 0.98), elbow_l=V(0.9, 0.0, -0.5), wrist_l=(-20.0, 0.0, -10.0),
                      clav_r=(12.0, 6.0), flow=0.3)
        S = self.look(S)
        Ct = self.pose(R0, pelvis=self.web_pelvis(51.5, 66.8), lean=17.0, hip_pitch=3.0, head_pitch=-4.0, twist=6.0,
                       hip_yaw=4.0, foot_r=self.web_foot(60.0, -0.12), foot_l=self.web_foot(42.0, 0.12),
                       foot_l_rot=(6.0, 0, 24), toe_l=6.0,
                       hand_r=V(-0.20, 0.52, 1.04), elbow_r=V(-0.8, -0.3, -0.5), **self.staff_kw(69.0, -0.10),
                       hand_l=V(0.26, -0.06, 0.80), elbow_l=V(0.9, 0.2, -0.3), wrist_l=(10.0, 0.0, -20.0),
                       clav_r=(6.0, 14.0), flow=0.5)
        Ct = self.look(Ct)
        F = self.pose(Ct, pelvis=self.web_pelvis(51.0, 66.8), lean=18.0, hand_r=V(-0.20, 0.50, 0.94),
                      **self.staff_kw(77.0, -0.10), flow=0.4)
        F = self.look(F)
        Rc = self.pose(R0, pelvis=self.web_pelvis(48.5, 65.6), lean=8.0, twist=2.0,
                       foot_r=self.web_foot(55.0, -0.11), hand_r=V(-0.28, 0.32, 0.96),
                       **self.staff_kw(40.0, -0.2), flow=0.22)
        keys = [K(0.0, R0), K(0.28, W, "out"), K(0.43, S, "in"), K(0.571, Ct, "linear"), K(0.71, F, "out"),
                K(0.86, Rc), K(1.0, R0)]
        return self.clip("Attack01", 533.0, keys, 466.7, notifies={"Contact": 267.0})

    # ── Attack02: level backhand sweep, right → left (same timing) ──────────────────────────────────────
    # Contact silhouette: square wide stance, chest turned into the sweep, the staff level across the body with
    # the crystal sweeping out to the left-front, the off hand flung out to the right / back.
    def attack02(self):
        R0 = self.ready()
        W = self.pose(R0, pelvis=R0.pelvis + V(0, -0.04, -0.05), lean=4.0, twist=-36.0, hip_yaw=-10.0,
                      foot_r=self.web_foot(51.0, -0.16), foot_l=self.web_foot(44.0, 0.15),
                      hand_r=V(-0.40, -0.10, 1.12), elbow_r=V(-0.6, -0.5, -0.5),
                      wpn=V(-0.70, -0.62, 0.36).normalized(), wpn_up=V(0, 0, 1), clav_r=(8.0, -10.0),
                      hand_l=V(0.30, 0.22, 1.00), elbow_l=V(0.9, 0.0, -0.4), flow=0.22)
        W = self.look(W)
        S = self.pose(W, pelvis=R0.pelvis + V(0, 0.02, -0.08), lean=7.0, twist=-10.0, hip_yaw=0.0,
                      foot_r=self.web_foot(53.0, -0.18), hand_r=V(-0.36, 0.30, 1.14), elbow_r=V(-0.8, -0.4, -0.4),
                      wpn=V(-0.86, 0.48, 0.12).normalized(), clav_r=(6.0, 4.0), flow=0.32)
        S = self.look(S)
        Ct = self.pose(R0, pelvis=R0.pelvis + V(0, 0.05, -0.10), lean=9.0, hip_pitch=2.0, twist=26.0, hip_yaw=10.0,
                       foot_r=self.web_foot(55.0, -0.20), foot_l=self.web_foot(42.0, 0.19),
                       foot_r_rot=(0, 0, -8), foot_l_rot=(8.0, 0, 30), toe_l=8.0,
                       hand_r=V(-0.02, 0.52, 1.16), elbow_r=V(-0.6, -0.6, -0.5),
                       wpn=V(0.80, 0.58, 0.14).normalized(), wpn_up=V(0, 0, 1), clav_r=(6.0, 16.0),
                       hand_l=V(0.42, -0.14, 0.94), elbow_l=V(1.0, 0.0, -0.2), wrist_l=(20.0, 0.0, 30.0),
                       clav_l=(4.0, -10.0), flow=0.5)
        Ct = self.look(Ct)
        F = self.pose(Ct, twist=34.0, hip_yaw=14.0, hand_r=V(0.20, 0.40, 1.16), elbow_r=V(-0.3, 0.5, -0.6),
                      wpn=V(0.62, -0.74, 0.16).normalized(), clav_r=(4.0, 18.0), flow=0.42)
        F = self.look(F)
        Rc = self.pose(R0, pelvis=R0.pelvis + V(0, 0.03, -0.05), lean=6.0, twist=10.0, hip_yaw=6.0,
                       hand_r=V(-0.10, 0.36, 1.00), elbow_r=V(-0.8, -0.3, -0.4),
                       wpn=V(0.30, 0.40, 0.86).normalized(), wpn_up=V(0, 0, 1), flow=0.25)
        Rc = self.look(Rc)
        keys = [K(0.0, R0), K(0.28, W, "out"), K(0.43, S, "in"), K(0.571, Ct, "linear"), K(0.71, F, "out"),
                K(0.86, Rc), K(1.0, R0)]
        return self.clip("Attack02", 533.0, keys, 466.7, notifies={"Contact": 267.0})

    # ── Attack03: arcane palm strike (same timing) ──────────────────────────────────────────────────────
    # Contact silhouette: left foot and left shoulder driven forward, the off-hand palm thrust out at chest height
    # (the web's palm flare), the staff held upright back at the right side — an open, two-sided shape unlike the
    # staff swings of Attack01 / 02.
    def attack03(self):
        R0 = self.ready()
        W = self.pose(R0, pelvis=R0.pelvis + V(0, -0.04, -0.05), lean=4.0, twist=26.0, hip_yaw=8.0, head_yaw=-18.0,
                      foot_r=self.web_foot(51.0, -0.12), foot_l=self.web_foot(44.0, 0.13),
                      hand_r=V(-0.38, 0.02, 1.08), elbow_r=V(-0.5, -0.8, -0.2), **self.staff_kw(-6.0, 0.05),
                      hand_l=V(0.25, 0.00, 0.98), elbow_l=V(0.8, -0.5, -0.3), wrist_l=(-30.0, 0.0, -30.0),
                      clav_l=(4.0, -12.0), flow=0.2)
        S = self.pose(R0, pelvis=R0.pelvis + V(0, 0.04, -0.07), lean=9.0, twist=6.0, hip_yaw=2.0,
                      foot_r=self.web_foot(50.0, -0.12), foot_l=self.web_foot(53.0, 0.13),
                      hand_r=V(-0.38, -0.04, 1.08), elbow_r=V(-0.5, -0.8, -0.2), **self.staff_kw(-8.0, 0.05),
                      hand_l=V(0.20, 0.30, 1.10), elbow_l=V(0.8, -0.4, -0.4), wrist_l=(-50.0, 0.0, -10.0),
                      clav_l=(6.0, 4.0), flow=0.35)
        S = self.look(S)
        Ct = self.pose(R0, pelvis=self.web_pelvis(51.0, 68.0), lean=14.0, hip_pitch=3.0, head_pitch=-4.0, twist=-26.0,
                       hip_yaw=-10.0, foot_l=self.web_foot(60.0, 0.15), foot_r=self.web_foot(41.0, -0.15),
                       foot_r_rot=(10.0, 0, 20), toe_r=10.0, knee_l=V(0.2, 1, 0),
                       hand_r=V(-0.40, -0.10, 1.14), elbow_r=V(-0.6, -0.8, -0.1), **self.staff_kw(-10.0, 0.04),
                       hand_l=V(0.10, 0.66, 1.24), elbow_l=V(0.7, -0.3, -0.6), wrist_l=(-75.0, 0.0, 10.0),
                       clav_l=(8.0, 24.0), clav_r=(4.0, -8.0), flow=0.55)
        Ct = self.look(Ct)
        F = self.pose(Ct, hand_l=V(0.10, 0.64, 1.20), flow=0.45)
        F = self.look(F)
        Rc = self.pose(R0, pelvis=R0.pelvis + V(0, 0.03, -0.04), lean=8.0, twist=-8.0, hip_yaw=-4.0,
                       foot_l=self.web_foot(48.0, 0.12), hand_l=V(0.20, 0.30, 0.98), wrist_l=(-30.0, 0.0, -10.0),
                       flow=0.25)
        Rc = self.look(Rc)
        keys = [K(0.0, R0), K(0.28, W, "out"), K(0.43, S, "in"), K(0.571, Ct, "linear"), K(0.71, F, "out"),
                K(0.86, Rc), K(1.0, R0)]
        return self.clip("Attack03", 533.0, keys, 466.7, notifies={"Contact": 267.0})

    # ── Cast01: staff raised overhead → staff + palm thrust (web CAST :527-554; 500 ms, Release 230) ─────
    # Release silhouette: lean 15°, the staff thrust forward-up out at the right side (its crystal clear of the
    # hood from the front 3/4 view, web viewPose), the off-hand palm pushed forward at shoulder height.
    REL = 230.0 / 437.5

    def cast01(self):
        R0 = self.ready()
        Ch = self.pose(R0, pelvis=self.web_pelvis(47.0, 65.4), lean=-4.6, head_pitch=-12.6, twist=-4.0,
                       foot_r=self.web_foot(54.0, -0.11), foot_l=self.web_foot(41.5, 0.11),
                       hand_r=V(-0.40, 0.04, 1.62), elbow_r=V(-1.0, 0.0, -0.1), **self.staff_kw(3.0, -0.25),
                       hand_l=V(0.26, 0.14, 1.36), elbow_l=V(1.0, 0.0, -0.4), wrist_l=(-55.0, 0.0, 0.0),
                       clav_r=(20.0, 2.0), clav_l=(12.0, 4.0), flow=0.35)
        Ch = Ch.but(head_yaw=-0.6 * (Ch.twist + Ch.hip_yaw))
        Rl = self.pose(R0, pelvis=self.web_pelvis(50.5, 66.2), lean=15.0, hip_pitch=2.0, head_pitch=-6.0, twist=8.0,
                       hip_yaw=6.0, foot_r=self.web_foot(58.5, -0.12), foot_l=self.web_foot(42.0, 0.12),
                       foot_l_rot=(6.0, 0, 26), toe_l=6.0,
                       hand_r=V(-0.30, 0.50, 1.30), elbow_r=V(-0.9, -0.2, -0.4), **self.staff_kw(62.0, -0.25),
                       hand_l=V(0.12, 0.62, 1.20), elbow_l=V(0.8, -0.3, -0.5), wrist_l=(-70.0, 0.0, 10.0),
                       clav_r=(8.0, 10.0), clav_l=(8.0, 18.0), flow=0.55)
        Rl = self.look(Rl)
        H = self.pose(Rl, lean=12.0, hand_r=V(-0.30, 0.47, 1.26), **self.staff_kw(58.0, -0.25),
                      hand_l=V(0.13, 0.56, 1.14), flow=0.3)
        H = self.look(H)
        keys = [K(0.0, R0), K(0.36, Ch, "out"), K(self.REL, Rl, "in"), K(0.72, H), K(1.0, R0)]
        return self.clip("Cast01", 500.0, keys, 437.5, notifies={"Release": 230.0})

    # ── Cast02: staff planted, palm raised (self-buffs: ice_armor / mana_shield / teleport) ─────────────
    def cast02(self):
        R0 = self.ready()
        Ch = self.pose(R0, pelvis=R0.pelvis + V(0, -0.02, -0.08), lean=10.0, head_pitch=12.0, twist=-6.0,
                       foot_r=self.web_foot(52.0, -0.13), foot_l=self.web_foot(42.0, 0.13),
                       hand_r=V(-0.30, 0.14, 1.12), elbow_r=V(-0.9, 0.1, -0.4), **self.staff_kw(-6.0, -0.15),
                       hand_l=V(0.12, 0.26, 0.96), elbow_l=V(0.8, 0.2, -0.5), wrist_l=(30.0, 0.0, -10.0),
                       flow=0.25)
        # release: the staff butt driven to the ground beside the right foot (staff vertical), the left palm raised
        # high, the head lifted to it — an upright, open silhouette
        Rl = self.pose(R0, pelvis=R0.pelvis + V(0, 0.0, -0.01), lean=-6.0, hip_pitch=-2.0, head_pitch=-18.0,
                       twist=6.0, foot_r=self.web_foot(52.5, -0.15), foot_l=self.web_foot(42.0, 0.15),
                       hand_r=V(-0.30, 0.16, 0.98), elbow_r=V(-0.9, 0.0, -0.4), **self.staff_kw(0.0, -0.06),
                       hand_l=V(0.30, 0.16, 1.70), elbow_l=V(1.0, 0.2, -0.2), wrist_l=(-75.0, 0.0, 0.0),
                       clav_l=(22.0, 4.0), flow=0.5)
        H = self.pose(Rl, head_pitch=-14.0, lean=-4.0, hand_l=V(0.30, 0.15, 1.66), flow=0.32)
        keys = [K(0.0, R0), K(0.36, Ch, "out"), K(self.REL, Rl, "in"), K(0.72, H), K(1.0, R0)]
        return self.clip("Cast02", 500.0, keys, 437.5, notifies={"Release": 230.0})

    # ── Cast_Blizzard (R5 signature): both arms raised, then flung wide — the storm called over the field ───
    def blizzard(self):
        R0 = self.ready()
        Ch = self.pose(R0, pelvis=R0.pelvis + V(0, -0.03, -0.02), lean=-8.0, head_pitch=-20.0, twist=0.0,
                       foot_r=self.web_foot(53.0, -0.15), foot_l=self.web_foot(42.0, 0.15),
                       hand_r=V(-0.32, 0.02, 1.70), elbow_r=V(-1.0, 0.0, -0.1), **self.staff_kw(-4.0, -0.30),
                       hand_l=V(0.32, 0.04, 1.72), elbow_l=V(1.0, 0.0, -0.1), wrist_l=(-80.0, 0.0, 0.0),
                       clav_r=(26.0, 0.0), clav_l=(26.0, 0.0), flow=0.4)
        Ch = Ch.but(head_yaw=0.0)
        # release: arms swept wide and forward at shoulder height, the staff crystal-out to the right, the palm
        # spread to the left — a wide T across the strike line, leaning in
        Rl = self.pose(R0, pelvis=R0.pelvis + V(0, 0.06, -0.07), lean=14.0, hip_pitch=2.0, head_pitch=-4.0,
                       twist=0.0, hip_yaw=0.0, foot_r=self.web_foot(57.0, -0.16), foot_l=self.web_foot(41.0, 0.16),
                       foot_l_rot=(6.0, 0, 24), toe_l=6.0,
                       hand_r=V(-0.56, 0.32, 1.30), elbow_r=V(-0.6, -0.6, -0.4),
                       wpn=V(-0.62, 0.62, 0.48).normalized(), wpn_up=V(0, 0, 1),
                       hand_l=V(0.56, 0.32, 1.30), elbow_l=V(0.6, -0.6, -0.4), wrist_l=(-55.0, 0.0, 30.0),
                       clav_r=(8.0, 12.0), clav_l=(8.0, 12.0), flow=0.7)
        Rl = self.look(Rl)
        H = self.pose(Rl, lean=11.0, hand_r=V(-0.54, 0.30, 1.24), hand_l=V(0.54, 0.30, 1.22), flow=0.4)
        H = self.look(H)
        keys = [K(0.0, R0), K(0.36, Ch, "out"), K(self.REL, Rl, "in"), K(0.72, H), K(1.0, R0)]
        return self.clip("Cast_Blizzard", 500.0, keys, 437.5, notifies={"Release": 230.0})

    # ── Cast_Meteor (R5 signature): rise up with the staff overhead, then drive it down in a crouch ─────────
    def meteor(self):
        R0 = self.ready()
        Ch = self.pose(R0, pelvis=R0.pelvis + V(0, -0.02, 0.0), lean=-10.0, hip_pitch=-2.0, head_pitch=-22.0,
                       foot_r=self.web_foot(52.0, -0.13), foot_l=self.web_foot(43.0, 0.13),
                       foot_r_rot=(14.0, 0, 10), toe_r=14.0, foot_l_rot=(14.0, 0, 20), toe_l=14.0,
                       hand_r=V(-0.22, 0.06, 1.80), elbow_r=V(-1.0, 0.2, 0.0), **self.staff_kw(-12.0, -0.10),
                       hand_l=V(0.24, 0.06, 1.74), elbow_l=V(1.0, 0.2, -0.1), wrist_l=(-80.0, 0.0, 0.0),
                       clav_r=(30.0, 0.0), clav_l=(28.0, 0.0), flow=0.45)
        Ch = Ch.but(head_yaw=0.0)
        Rl = self.pose(R0, pelvis=R0.pelvis + V(0, 0.05, -0.08), lean=26.0, hip_pitch=4.0, head_pitch=-10.0,
                       twist=4.0, foot_r=self.web_foot(54.0, -0.16), foot_l=self.web_foot(41.5, 0.15),
                       foot_l_rot=(14.0, 0, 26), toe_l=14.0, knee_r=V(-0.25, 1, 0),
                       hand_r=V(-0.28, 0.46, 0.82), elbow_r=V(-0.9, 0.0, -0.4), **self.staff_kw(132.0, -0.20),
                       hand_l=V(0.28, 0.30, 0.70), elbow_l=V(0.9, 0.0, -0.4), wrist_l=(30.0, 0.0, -10.0),
                       clav_r=(4.0, 14.0), clav_l=(0.0, 10.0), flow=0.75)
        Rl = self.look(Rl, max_down=14.0)
        H = self.pose(Rl, lean=22.0, pelvis=R0.pelvis + V(0, 0.04, -0.07), hand_r=V(-0.28, 0.44, 0.86),
                      **self.staff_kw(126.0, -0.20), flow=0.45)
        H = self.look(H, max_down=14.0)
        keys = [K(0.0, R0), K(0.36, Ch, "out"), K(self.REL, Rl, "in"), K(0.72, H), K(1.0, R0)]
        return self.clip("Cast_Meteor", 500.0, keys, 437.5, notifies={"Release": 230.0})

    # ── Hurt 308 ms (web HURT; keys over 231 ms) + HurtAdd ──────────────────────────────────────────────
    def hit_pose(self) -> BP:
        R0 = self.ready()
        return self.pose(R0, pelvis=self.web_pelvis(44.0, 66.0), lean=-18.0, hip_pitch=-3.0, head_pitch=-22.0,
                         head_yaw=6.0, foot_r=self.web_foot(52.5, -0.11), foot_l=self.web_foot(41.0, 0.11),
                         hand_r=V(-0.38, -0.10, 1.16), elbow_r=V(-0.8, -0.4, -0.3), **self.staff_kw(-14.0, -0.40),
                         hand_l=V(0.24, 0.16, 1.14), elbow_l=V(0.9, 0.0, -0.4), wrist_l=(-30.0, 0.0, -10.0),
                         clav_r=(12.0, -4.0), clav_l=(8.0, 2.0), flow=0.45)

    def hurt(self):
        R0, Hp = self.ready(), self.hit_pose()
        mid = self.pose(Hp, pelvis=self.web_pelvis(43.5, 66.4), lean=-13.7, head_pitch=-14.3, flow=0.35)
        keys = [K(0.0, Hp), K(0.33, mid), K(1.0, R0)]
        return self.clip("Hurt", 308.0, keys, 230.8)

    def hurt_add(self):
        R0 = self.ready()
        J = self.pose(R0, lean=R0.lean - 10.0, head_pitch=R0.head_pitch - 14.0, twist=R0.twist + 6.0,
                      hand_r=R0.hand_r + V(-0.04, -0.04, 0.06), hand_l=R0.hand_l + V(0.0, -0.04, 0.05),
                      clav_r=(6.0, -3.0), clav_l=(6.0, 0.0), flow=0.3)
        keys = [K(0.0, R0), K(60.0 / 308.0, J, "out"), K(1.0, R0, "smooth")]
        return self.clip("HurtAdd", 308.0, keys, 308.0, additive=True)

    # ── Dodge 273 ms: low arcane leap / skim (web DODGE :557-579; keys over 227 ms) ─────────────────────
    def dodge(self):
        R0 = self.ready()
        d0 = self.pose(R0, pelvis=self.web_pelvis(47.0, 68.5), lean=20.0, head_pitch=4.0, twist=0.0, hip_yaw=0.0,
                       foot_r=self.web_foot(53.0, -0.12), foot_l=self.web_foot(42.5, 0.12),
                       hand_r=V(-0.32, 0.22, 0.78), **self.staff_kw(52.0, -0.35),
                       hand_l=V(0.22, 0.16, 0.76), flow=0.3)
        up = self.pose(R0, pelvis=self.web_pelvis(50.0, 60.0), lean=34.0, hip_pitch=2.0, head_pitch=-6.0, twist=0.0,
                       hip_yaw=0.0, foot_r=self.web_foot(49.0, -0.10, 82.0), foot_l=self.web_foot(39.0, 0.10, 80.0),
                       foot_r_rot=(40.0, 0, 6), foot_l_rot=(50.0, 0, 10), toe_l=20.0, toe_r=20.0,
                       knee_r=V(-0.1, 1, 0.2), knee_l=V(0.1, 1, 0.2),
                       hand_r=V(-0.42, 0.40, 1.14), elbow_r=V(-0.8, -0.2, -0.5), **self.staff_kw(70.0, 0.12),
                       hand_l=V(0.30, -0.24, 1.04), elbow_l=V(0.8, 0.4, -0.3), wrist_l=(30.0, 0.0, 0.0),
                       flow=0.95)
        sk = self.pose(up, pelvis=self.web_pelvis(51.0, 59.0), lean=30.0, foot_r=self.web_foot(52.0, -0.10, 80.0),
                       foot_l=self.web_foot(41.0, 0.10, 81.0), hand_r=V(-0.42, 0.40, 1.10),
                       **self.staff_kw(64.0, 0.12), flow=1.0)
        land = self.pose(R0, pelvis=self.web_pelvis(50.0, 69.5), lean=17.0, head_pitch=6.0, twist=0.0, hip_yaw=0.0,
                         foot_r=self.web_foot(55.0, -0.13), foot_l=self.web_foot(44.0, 0.13),
                         hand_r=V(-0.42, 0.22, 0.95), **self.staff_kw(14.0, -0.05), hand_l=V(0.24, 0.10, 0.80),
                         flow=0.5)
        keys = [K(0.0, d0), K(0.25, up, "out"), K(0.5, sk), K(0.75, land, "in"), K(1.0, R0)]
        return self.clip("Dodge", 273.0, keys, 227.0)

    # ── Death 667 ms: crumple to the knees, pitch forward face-down, the staff falls in front ──────────────
    @staticmethod
    def fall_spin(pitch: float, roll: float) -> dict:
        """Fall forward (``pitch`` about the lateral axis, + = face first) then roll by ``roll`` about the world
        forward axis (+ = onto the left side): one axis-angle for BP.spin."""
        from mathutils import Quaternion
        q = Quaternion((0, 1, 0), math.radians(roll)) @ Quaternion((1, 0, 0), math.radians(pitch))
        ax, ang = q.axis, math.degrees(q.angle)
        if ax.x < 0:
            ax, ang = -ax, -ang
        return {"spin": ang, "spin_axis": Vector(ax)}

    # ── Death 667 ms: buckle → tall kneel → topple forward over the knees → face-down (web DEATH: drops to the
    # knees, pitches forward face-down, the staff falls) ────────────────────────────────────────────────────
    def death(self):
        R0, Hp = self.ready(), self.hit_pose()
        d22 = self.pose(Hp, pelvis=self.web_pelvis(45.5, 71.0), lean=8.0, head_pitch=22.0,
                        foot_r=self.web_foot(50.0, -0.12), foot_l=self.web_foot(41.0, 0.12, 88.0),
                        foot_l_rot=(30.0, 0, 16), toe_l=26.0, knee_r=V(-0.1, 1, -0.1), knee_l=V(0.1, 1, -0.1),
                        hand_r=V(-0.40, 0.16, 0.90), **self.staff_kw(12.0, -0.4), hand_l=V(0.24, 0.12, 0.82))
        # tall kneel (thighs near vertical, inside the skirt cone; shins back along the floor), slumped, the head
        # bowed, the staff tilting out of a loosening grip
        KN = V(0.0, 0.05, 0.03)                       # knee line on the floor = the topple's pivot
        d45 = self.pose(R0, pelvis=self.web_pelvis(47.5, 77.0), lean=16.0, hip_pitch=2.0, head_pitch=26.0, twist=-4.0,
                        hip_yaw=2.0, foot_r=self.web_foot(37.6, -0.12, 89.5), foot_l=self.web_foot(37.0, 0.12, 89.5),
                        foot_r_rot=(78.0, 0, 6), foot_l_rot=(78.0, 0, 10), knee_r=V(-0.1, 1, -0.3),
                        knee_l=V(0.1, 1, -0.3),
                        hand_r=V(-0.42, 0.20, 0.52), elbow_r=V(-0.9, 0.0, 0.0), **self.staff_kw(-4.0, -0.62),
                        hand_l=V(0.26, 0.14, 0.40), elbow_l=V(0.9, 0.0, 0.0), flow=0.2)

        def knee_back(bp: BP, deg: float) -> BP:
            """Pre-rotate the shins / feet back about the knee line by ``deg`` so the topple (spin ``deg``) leaves
            them on the floor."""
            from mathutils import Quaternion
            q = Quaternion((1, 0, 0), math.radians(-deg))
            out = {}
            for s in ("l", "r"):
                f = getattr(bp, f"foot_{s}")
                out[f"foot_{s}"] = KN + q @ (f - KN)
                pr = getattr(bp, f"foot_{s}_rot")
                out[f"foot_{s}_rot"] = (pr[0] - deg, pr[1], pr[2])
            return bp.but(**out)
        # toppling forward over the knees (a dead weight: arms swinging limp ahead, the staff falling away right)
        from mathutils import Quaternion
        sp72 = self.fall_spin(44.0, 6.0)
        q72 = Quaternion(sp72["spin_axis"], math.radians(sp72["spin"]))
        # the staff tips out to the right and back as the body goes over (world direction, butt ahead of the hood)
        w72 = q72.inverted() @ Vector((-0.55, 0.78, -0.30)).normalized()
        d72 = knee_back(self.pose(d45, lean=22.0, head_pitch=18.0, hand_r=V(-0.52, 0.26, 0.74),
                                  elbow_r=V(-0.9, 0.0, -0.2), wpn=w72, wpn_up=V(0, 1, 0),
                                  hand_l=V(0.30, 0.30, 0.42), elbow_l=V(0.9, 0.0, -0.2), flow=0.55), 44.0)
        d72 = d72.but(pivot=KN, **sp72)
        # face-down (authored standing, sunk so the knees sit on the pivot, then spun 86° forward and rolled onto
        # the left side): legs straight, insteps on the floor, the staff arm flung ahead, the left hand by the hood,
        # the hood turned to the side
        dz = 0.41 - 0.03
        d100 = self.pose(R0, pelvis=R0.pelvis + V(0, 0.03, -dz), lean=-4.0, hip_pitch=0.0, twist=0.0, hip_yaw=0.0,
                         head_pitch=-6.0, head_yaw=-62.0, head_roll=-10.0,
                         foot_r=V(-0.10, 0.02, 0.058 - dz), foot_l=V(0.11, 0.0, 0.058 - dz),
                         foot_r_rot=(62.0, 0, 14), foot_l_rot=(58.0, 0, 22), toe_l=0.0, toe_r=0.0,
                         knee_r=V(-0.1, 1, 0), knee_l=V(0.1, 1, 0),
                         hand_r=V(-0.46, 0.10, 1.28), elbow_r=V(-0.7, -0.3, 0.0),
                         wpn=V(-0.25, -0.07, -0.97).normalized(), wpn_up=V(0, 1, 0),
                         hand_l=V(0.34, 0.03, 1.00), elbow_l=V(0.9, -0.2, -0.3), wrist_l=(0.0, 0.0, 0.0),
                         pivot=KN, **self.fall_spin(86.0, 6.0), flow=0.05)
        keys = [K(0.0, Hp), K(0.22, d22), K(0.45, d45), K(0.72, d72, "in"), K(1.0, d100, "out")]
        gr = self.grounded(snap=True, pivot_pelvis=False)
        span = 555.6
        t0 = 0.72 * span
        lay = self.lay_staff(t0, span, Vector((-0.28, 0.96, 0.0)))
        # from the topple key on, the staff follows its authored fall into the laid direction (no per-frame fit)
        return self.clip("Death", 667.0, keys, span,
                         post=lambda bp, t: lay(gr(self.fit_staff(bp, t) if t < t0 else bp, t), t))

    def lay_staff(self, t0: float, t1: float, azim: Vector):
        """Post step of the face-down Death: from ``t0`` to ``t1`` (ms) the staff settles onto the floor beside the
        body — along the world azimuth ``azim``, pitched so the claw head (``STAFF_HEAD_R``) and the butt rest on the
        floor — blended from the fitted direction (one smooth settle, no per-frame search: the fit's ring search
        picked a different azimuth every frame once the shaft met the floor)."""
        import mage as MG
        from mathutils import Quaternion
        s_head, s_butt = MG.CRYSTAL_AT * self.U, MG.STAFF_DOWN * self.U
        a = Vector((azim.x, azim.y, 0.0)).normalized()

        def post(bp: BP, t: float) -> BP:
            w = float(HB.smooth01((t - t0) / max(t1 - t0, 1e-6)))
            if w <= 0.0 or abs(bp.spin) < 1e-6:
                return bp
            p = C.to_pose(self.rig, bp, None, grips=self.GRIPS)
            G = anim.evaluate(p, return_matrices=True)["weapon_r"].translation
            dz = (self.STAFF_HEAD_R - G.z) / s_head
            dz = max(-0.6, min(dz, (G.z - 0.045) / s_butt))
            d = (a * math.sqrt(max(0.0, 1.0 - dz * dz)) + Vector((0, 0, dz))).normalized()
            q = Quaternion(Vector(bp.spin_axis).normalized(), math.radians(bp.spin))
            pre = (q.inverted() @ d).normalized()
            cur = bp.wpn.normalized()
            return bp.but(wpn=cur.slerp(pre, w) if cur.dot(pre) > -0.999 else pre)
        return post

    # ── Portrait (static, spec §3.4 / §9.5): the staff upright beside the hood, the crystal at the cheek ─────
    def portrait(self):
        R0 = self.ready()
        P = self.pose(R0, hand_r=V(-0.27, 0.16, 1.10), elbow_r=V(-0.8, 0.2, -0.5), **self.staff_kw(-4.0, -0.20),
                      hand_l=V(0.16, 0.24, 1.10), elbow_l=V(0.9, 0.0, -0.5), wrist_l=(-60.0, 0.0, 10.0),
                      head_yaw=6.0, head_pitch=4.0, twist=-4.0, clav_r=(10.0, 4.0), flow=0.15)
        keys = [K(0.0, P), K(1.0, P)]
        return self.clip("Portrait", 1000.0 / 60.0, keys, 1000.0 / 60.0, layer=lambda bp, t: bp)

    def all_clips(self):
        return [self.idle(), self.walk(), self.run(), self.attack01(), self.attack02(), self.attack03(),
                self.cast01(), self.cast02(), self.blizzard(), self.meteor(), self.hurt(), self.hurt_add(),
                self.dodge(), self.death(), self.portrait()]

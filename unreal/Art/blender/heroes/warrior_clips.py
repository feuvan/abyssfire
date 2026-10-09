"""Warrior poses and clips (spec art-inventory-ch1.md §3.1 ready pose, §3.4 clip table + key poses, §3.5
signature montages, DECISIONS R5/R9/C4). Web keys: ``src/graphics/sprites/players/PlayerWarrior.ts:558-737``.

Web side-view rig units are converted with ``U`` (forward = x − CENTER_X, height = GROUND_Y − y); the 3D
poses add what a single side view cannot say (lateral hand placement, shield facing, foot turn-out, hip/chest
twist), and the run cycle is re-authored without foot sliding for the hero ground speed 3.333 m/s (S5/R9).
"""
from __future__ import annotations

import math

import numpy as np

from mathutils import Quaternion, Vector

from kit import anim
from kit.anim import Pose

import common as C
from common import BP, K, V

RUN_SPEED = 120.0 / 36.0          # m/s: hero moveSpeed 120 / 36 px per tile (DECISIONS S5, R9) = 3.333
WALK_SPEED = 1.2


class WarriorAnims:
    def __init__(self, rig, D):
        self.rig = rig
        self.D = D
        self.U = D.U
        J = rig.joints
        self.J = J
        self.AZ = J["ankle_l"].z
        self.chains = {n: C.chain_rest_angles(rig, n) for n in ("cape_c", "cape_l", "cape_r", "tabard_f",
                                                                  "tabard_b", "plume")}
        self.leg = rig.spec.thigh + rig.spec.shin

    # ── helpers ────────────────────────────────────────────────────────────────────────────────────────
    def w(self, fwd_u: float, up_u: float, lat: float = 0.0) -> Vector:
        """Web side-view point (units from CENTER_X / GROUND_Y) → body-space point (lat in metres)."""
        return V(lat, fwd_u * self.U, up_u * self.U)

    def web_pelvis(self, x: float, y: float) -> Vector:
        """Web root (x, y in sprite units) → pelvis offset from the rest pelvis."""
        return V(0.0, (x - 48.0) * self.U, (91.0 - y) * self.U - self.J["pelvis"].z)

    def web_foot(self, x: float, lat: float, y: float = 91.0) -> Vector:
        return V(lat, (x - 48.0) * self.U, self.AZ + (91.0 - y) * self.U)

    def web_hand(self, x: float, y: float, lat: float) -> Vector:
        """Web hand centre → wrist target (the fist sits ~5 cm further along the hand)."""
        return V(lat, (x - 48.0) * self.U, (91.0 - y) * self.U)

    def thigh_fwd(self, bp: BP, s: str) -> float:
        J = self.J
        hip = J[f"hip_{s}"] + bp.pelvis
        ank = getattr(bp, f"foot_{s}")
        ank = J[f"ankle_{s}"] if ank is None else ank
        d = ank - hip
        L = d.length
        ang = math.degrees(math.atan2(-d.y, -d.z))
        bend = math.degrees(math.acos(max(-1.0, min(1.0, L / self.leg))))
        return ang + bend

    # ── the real armour, skinned (cloth envelope, feet on the floor, ground contact) ────────────────────
    # plates the cape lies on / hangs over (torso: always; limbs: only where they reach in behind the cape —
    # the cape's side edges legitimately tuck behind the arms)
    ENV_TORSO = ("cuirass", "gorget", "belt", "buckle", "fauld", "mailskirt", "helm", "seam_b", "nape_rivet",
                 "plume_holder")
    ENV_LIMBS = ("pauldron", "pauldron_rim", "pauldron_rivet", "lame2", "lame3", "rerebrace", "rere_lame", "ua",
                 "couter", "couter_wing", "vambrace", "cuff", "fist", "thumb", "knuckles")
    FEET = {"l": ("sabaton_l", "sabaton_lame0_l", "sabaton_lame1_l"),
            "r": ("sabaton_r", "sabaton_lame0_r", "sabaton_lame1_r")}
    CAPE_CLEAR = (0.0105, 0.020)  # inner lining off the plates: torso (cloth half-thickness 7.5 mm + 3 mm), limbs
    CAPE_YOKE = 0.055           # top of the cape pinned under the pauldrons (not pushed by them)

    def skin(self):
        """Lazily sampled :class:`common.SkinPoints` of the LOD0 body (+ sword, shield) — None without meshes."""
        if getattr(self, "_skin", False) is not False:
            return self._skin
        import json
        import bpy
        import warrior as W
        body = next((o for o in self.rig.obj.children if o.type == "MESH" and o.get("af_lod") == 0
                     and "af_parts" in o), None)
        self._skin = None
        if body is None:
            return None
        names = json.loads(body["af_parts"])
        env = C.SkinPoints(self.rig).add_mesh(body, names, lambda n: n in self._env_names(), step=2)
        sw = bpy.data.objects.get(W.SWORD)
        if sw is not None:                    # a sword carried or trailing behind lifts the cape too
            env.add_rigid(sw, "weapon_r", step=2, tag="sword")
        self.env_limb = env.mask([n + s for n in self.ENV_LIMBS for s in ("", "_l", "_r")] + ["sword"])
        feet = C.SkinPoints(self.rig).add_mesh(body, names, lambda n: n in self.FEET["l"] + self.FEET["r"])
        self.feet = {s: (feet, feet.mask(self.FEET[s])) for s in ("l", "r")}
        # cloth is left out: its chains are solved after the body and keep themselves off the floor
        grd = C.SkinPoints(self.rig).add_mesh(body, names, lambda n: not n.startswith(("cape", "tabard", "plume")))
        for nm, bone in ((W.SWORD, "weapon_r"), (W.SHIELD, "weapon_l")):
            ob = bpy.data.objects.get(nm)
            if ob is not None:
                grd.add_rigid(ob, bone, step=2, tag=nm)
        self.ground_pts = grd
        self._cape_len = 0.735
        self._skin = env
        self._cape_tables()
        # the modelled drape hugs the upper back (the U-curl wraps the shoulder blades): what the bind pose already
        # has is the design, so torso points only push the cape by what a pose adds on top of it
        self._tol = {}
        fr0 = self.cape_frame(Pose(self.rig))
        for name in ("cape_c", "cape_l", "cape_r"):
            info = self.chains[name]
            self._tol[name] = np.where(self.env_limb, 0.0, np.maximum(0.0, -self._slack(name, info, info.rest, fr0)))
        return env

    def _env_names(self) -> set:
        return {n + s for n in self.ENV_TORSO + self.ENV_LIMBS for s in ("", "_l", "_r")}

    def _cape_tables(self) -> None:
        """Rest offsets of the cape surface in front of each chain's bone line, on a (uu, k) grid: the U-curl,
        the folds and the drape's curvature between bone joints bring the cloth closer to (or further from) the
        body than the straight bones (bilinear lookup in ``_cape_off``)."""
        import warrior as W
        self._uu = np.linspace(-1.0, 1.0, 41)
        self._kk = np.linspace(0.0, 1.0, 21)
        self._off = {}
        ztop = W.CAPE(0.0, 0.0).z
        for name, uc in (("cape_c", 0.0), ("cape_l", 0.72), ("cape_r", -0.72)):
            info = self.chains[name]
            zs = [self.rig.head(info.names[0]).z] + [self.rig.tail(n).z for n in info.names]
            ys = [self.rig.head(info.names[0]).y] + [self.rig.tail(n).y for n in info.names]
            tab = np.zeros((len(self._uu), len(self._kk)))
            for j, k in enumerate(self._kk):
                line = float(np.interp(-(ztop - self._cape_len * k), [-z for z in zs], ys))
                for i, uu in enumerate(self._uu):
                    tab[i, j] = line - W.CAPE(uu, k).y
            self._off[name] = tab

    def _cape_off(self, name: str, uu: np.ndarray, k: np.ndarray) -> np.ndarray:
        tab = self._off[name]
        fi = np.clip((uu + 1.0) / 2.0 * (len(self._uu) - 1), 0, len(self._uu) - 1.0001)
        fj = np.clip(k * (len(self._kk) - 1), 0, len(self._kk) - 1.0001)
        i, j = fi.astype(int), fj.astype(int)
        a, b = fi - i, fj - j
        return ((1 - a) * (1 - b) * tab[i, j] + a * (1 - b) * tab[i + 1, j] + (1 - a) * b * tab[i, j + 1]
                + a * b * tab[i + 1, j + 1])

    def cape_frame(self, p: Pose):
        """Obstacles for the cape in the chains' plane: posed armour points, the plane axes (X lateral = the
        posed chest's left axis, D = world down ⟂ X, B = back) and the chest's actual sagittal pitch."""
        sp = self.skin()
        mats = anim.evaluate(p, return_matrices=True)
        bones = self.rig.obj.data.bones
        M3 = mats["spine_03"].to_3x3() @ bones["spine_03"].matrix_local.to_3x3().inverted()
        X = (M3 @ Vector((1, 0, 0))).normalized()
        Dn = Vector((0, 0, -1))
        Dn = (Dn - X * Dn.dot(X)).normalized()
        B = X.cross(Dn)
        v = M3 @ Vector((0, 0, -1))
        acc = math.degrees(math.atan2(v.dot(B), v.dot(Dn)))
        pts = sp.posed(mats) if sp is not None else None
        return pts, mats, np.array(X), np.array(Dn), np.array(B), acc

    def _cape_points(self, name: str, frame, limb_uu: float = 0.85):
        """(band mask, depth below the chain root, effective back coordinate incl. surface offset + clearance)."""
        pts, mats, X, Dn, B, acc = frame
        rc = np.array(mats["cape_c_01"].translation)
        r0 = np.array(mats[self.chains[name].names[0]].translation)
        rel_c = pts - rc
        rel = pts - r0
        dn, bk = rel @ Dn, rel @ B
        k = np.clip((rel_c @ Dn) / self._cape_len, 0.0, 1.0)
        import warrior as W
        uu = (rel_c @ X) / (W.u(7.4) + W.u(2.2) * k)
        lo, hi = {"cape_c": (-0.55, 0.55), "cape_l": (0.2, 1.0), "cape_r": (-1.0, -0.2)}[name]
        limb = self.env_limb
        band = (uu >= lo) & (uu <= hi + (limb_uu - 0.85)) & (uu >= lo - (limb_uu - 0.85))
        band &= np.where(limb, np.abs(uu) <= limb_uu, True)
        band &= dn > np.where(limb, self.CAPE_YOKE, 0.03)
        clear = np.where(limb, self.CAPE_CLEAR[1], self.CAPE_CLEAR[0])
        return band, dn, bk + self._cape_off(name, uu, k) + clear

    def _slack(self, name: str, info, angles, frame) -> np.ndarray:
        """How far (m, along B) the chain line at ``angles`` passes behind each point's clearance surface
        (negative = the point would cut the cloth); +inf outside the chain's band."""
        band, dn, be = self._cape_points(name, frame)
        bones = self.rig.obj.data.bones
        out = np.full(len(dn), np.inf)
        s_dn = s_bk = 0.0
        for i, n in enumerate(info.names):
            L = bones[n].length
            a = math.radians(angles[i])
            e_dn = s_dn + L * math.cos(a)
            sel = band & (dn > s_dn) & (dn <= e_dn + (1e9 if i == len(info.names) - 1 else 0.0))
            out[sel] = s_bk + (dn[sel] - s_dn) * math.tan(a) - be[sel]
            s_dn, s_bk = e_dn, s_bk + L * math.sin(a)
        return out

    def cape_push(self, name: str, info, want: list, frame, limb_uu: float = 0.85) -> list:
        """Greedy envelope from the yoke down: each link turns back (never forward) just enough that every
        armour point in its band and depth range lies ``CAPE_CLEAR`` in front of the cloth surface (the rest
        offset of the surface from the chain line included). Cloth lies **on** a leaning / twisting back plate,
        and an arm reaching in behind it lifts the cape over the arm instead of punching through."""
        if frame[0] is None:
            return want
        band, dn, bk = self._cape_points(name, frame, limb_uu)
        if not band.any():
            return want
        dn, bk = dn[band], bk[band] - self._tol[name][band]
        bones = self.rig.obj.data.bones
        s_dn = s_bk = 0.0
        out = []
        for i, n in enumerate(info.names):
            L = bones[n].length
            a = want[i]
            sel = (dn > s_dn + 0.004) & (dn <= s_dn + L + 0.02)
            if sel.any():
                need = float(np.degrees(np.arctan2(bk[sel] - s_bk, dn[sel] - s_dn)).max())
                a = max(a, min(need, 100.0))
            out.append(a)
            s_dn += L * math.cos(math.radians(a))
            s_bk += L * math.sin(math.radians(a))
        return out

    # ── cloth (cape / tabards / plume), web clothChain: ang = flow·1.2·(0.6+0.4k) + wave ──────────────
    CAPE_LAG = 60.0          # ms: the hem link follows the body's lean / flow of 60 ms ago (root: 15 ms)
    CAPE_EDGE_LAG = 18.0     # ms: the edge chains trail the centre chain (phase offset → overlapping folds)
    CAPE_COUPLE = 10.0       # deg: max difference between neighbouring chains' links (relative to the rest drape)

    def _couple_cape(self, wants: dict) -> None:
        """Keep the three cape chains one sheet: per link, a chain whose deviation from its rest drape lags its
        neighbour's by more than ``CAPE_COUPLE`` is turned back to it (turning back only — away from the body —
        so the armour envelope still holds). cape_c is the neighbour of both edges."""
        ch = self.chains
        for _ in range(2):
            for i in range(len(wants["cape_l"])):
                ic = min(len(wants["cape_c"]) - 1, int(round((i + 1) / 3 * 4)) - 1)
                dc = wants["cape_c"][ic] - ch["cape_c"].rest[ic]
                for e in ("cape_l", "cape_r"):
                    de = wants[e][i] - ch[e].rest[i]
                    if de < dc - self.CAPE_COUPLE:
                        wants[e][i] += (dc - self.CAPE_COUPLE) - de
                    elif dc < de - self.CAPE_COUPLE:
                        wants["cape_c"][ic] += (de - self.CAPE_COUPLE) - dc
                        dc = wants["cape_c"][ic] - ch["cape_c"].rest[ic]

    def cloth(self, p: Pose, bp: BP) -> None:
        ch = self.chains
        self._ground = self._world_fn(p, bp)
        acc = bp.hip_pitch + bp.lean
        fl = bp.flow - 0.12
        ph = bp.wave
        frame = self.cape_frame(p) if self.skin() is not None else None
        acc_c = frame[5] if frame is not None else acc
        # cape: web angles are world-space (flow streams it back from straight down); the back plate only
        # pushes it further back when the body leans forward, more at the shoulders — then the envelope of the
        # real (skinned) armour keeps every link outside the plates (cape_push).
        # Art review 6 (no stacked boards in fast motion): every link reads the body's lean / flow **lagged** by
        # its depth down the chain (0 → CAPE_LAG ms at the hem, the edge chains a further CAPE_EDGE_LAG ms), the
        # streaming grows toward the tips (progressive curl), and the three chains are coupled (no link of one
        # chain more than CAPE_COUPLE° from its neighbour, relative to their rest drape) so the U section stays one
        # sheet; the smooth surface weights (warrior.cape_weights) blend between them.
        wf = (1.0, 0.85, 0.65, 0.5)
        wants = {}
        for name in ("cape_c", "cape_l", "cape_r"):
            info = ch[name]
            n = len(info.names)
            want = []
            for i in range(n):
                k = (i + 1) / n
                kk = min(3, int(round(k * 3)))
                lag = self.CAPE_LAG * (i + 1) / n + (self.CAPE_EDGE_LAG if name != "cape_c" else 0.0)
                pb = C.ClothClock.past(lag) or bp
                fl_i, acc_i = pb.flow - 0.12, pb.hip_pitch + pb.lean
                ph_i = ph - 2.0 * math.pi * lag / 700.0
                dweb = math.degrees(fl_i * 1.2 * (0.45 + 0.75 * k))
                wav = math.degrees(math.sin(ph_i - 2.2 * k) * 0.06 * k * (0.4 + pb.flow))
                if name != "cape_c":
                    wav += math.degrees(math.sin(ph_i - 2.2 * k + (0.5 if name == "cape_l" else -0.5)) * 0.03 * k)
                billow = 1.0 if name == "cape_c" else 0.9       # centre streams a little further than the edges
                free = max(info.rest[i] + dweb * billow, acc_i * wf[kk] + 0.8 * info.rest[i])
                # cling: wraps the curled back (upper cape follows the chest, the hem follows the pelvis)
                body = (bp.hip_pitch + bp.lean * max(0.0, 1.0 - 0.95 * i / max(1, n - 1)) + info.rest[i] + dweb * 0.3
                        - bp.curl * (i / max(1, n - 1)) ** 1.5)
                want.append(free + (body - free) * bp.cling + wav)
            if frame is not None:
                # wrapped round a rolling / falling body the cape also reaches the arms at its sides
                want = self.cape_push(name, info, want, frame, 0.85 + 0.6 * bp.cling)
            wants[name] = want
        self._couple_cape(wants)
        for name in ("cape_c", "cape_l", "cape_r"):
            info = ch[name]
            n = len(info.names)
            spread = 2.0 * bp.flow * (1 if name == "cape_l" else -1 if name == "cape_r" else 0)
            side = [spread + bp.sway * 4 * (i + 1) / n for i in range(n)]
            want = self._above_ground(name, info, wants[name], self._ground, "spine_03")
            C.set_chain(p, info, want, acc_c, side)
        # tabards. Angles are armature-space (pre-spin) degrees back from straight down; see ``tabards``
        self.tabards(p, bp, fl, ph)
        # plume: semi-stiff horsehair crest — lags 40 % of the head pitch, streams with the flow, droops a
        # little toward world down when the body is spun (the tip more than the root)
        pl = ch["plume"]
        acch = acc + bp.head_pitch
        want = [pl.rest[i] + acch * 0.6 + math.degrees(fl * 0.5 * (i + 1) / 3)
                + math.degrees(math.sin(ph - 1.2 * i) * 0.05 * (i + 1) / 3) + bp.plume * (i + 1) / 3
                for i in range(3)]
        ga, gs = self.gravity(bp)
        if gs > 0:
            for i in range(3):
                d = self._arc(ga - want[i])
                want[i] += d * gs * (0.10, 0.22, 0.34)[i] * (1.0 - (abs(d) / 180.0) ** 4)
        want = self._above_ground("plume", pl, want, self._ground, "head")
        C.set_chain(p, pl, want, acch, [bp.sway * 6 * (i + 1) / 3 for i in range(3)])

    @staticmethod
    def _arc(d: float) -> float:
        """Wrap an angle difference to (−180, 180]."""
        return (d + 180.0) % 360.0 - 180.0

    def gravity(self, bp: BP) -> tuple[float, float]:
        """World −Z in the pre-spin armature frame as a sagittal chain angle (deg back from straight down) and
        the strength of the pull (0 upright, 1 once the body is spun ≥ 40° in the sagittal plane)."""
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

    @staticmethod
    def _smooth(e0: float, e1: float, x: float) -> float:
        t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
        return t * t * (3 - 2 * t)

    def tabards(self, p: Pose, bp: BP, fl: float, ph: float) -> None:
        """Front / back tabard panels (two links each).

        Upright, a panel is streamed by the flow and pushed by the thighs (web ``viewTabard``). Once the body is
        spun (rolls, falls) two solutions compete: **hanging** — pulled toward world down, the tip link more
        than the root (so the panel bends), fading out when gravity is exactly opposite so it never flips — and
        **wrapped** — laid onto the curled body (front panel in the belly fold and draped over the knees, back
        panel round the seat and along the backs of the thighs). The one whose tip hangs lower wins, blended
        over ±15°; both stay inside the free wedge between the thighs and the torso, and no joint goes below
        the ground. Result: no rigid board sticking out of a tucked roll or a fall."""
        ch = self.chains
        accp = bp.hip_pitch
        tl, tr = self.thigh_fwd(bp, "l"), self.thigh_fwd(bp, "r")
        T = max(tl, tr)
        Tb = max(-tl, -tr)
        L = bp.hip_pitch + bp.lean                 # torso direction = L − 180
        ga, gs = self.gravity(bp)
        wav = math.degrees(math.sin(ph * 2) * 0.03 * (0.3 + bp.flow))
        knee = self._smooth(50.0, 90.0, T)         # thighs tucked: the front panel may drape over the knees
        # front: backward limit = the leading thigh, forward limit = the belly / cuirass
        tf = ch["tabard_f"]
        w1 = min(tf.rest[0] - fl * 6, -(T - 9.0))
        w2 = min(tf.rest[1] - fl * 4, -(T - 6.0) * 0.92, w1 + 4)
        hi_f = (-(T - 9.0), -(T - 6.0) * 0.92 + 30.0 * knee)
        lo_f = (L - 180.0 + 30.0, L - 180.0 + 22.0)
        wf1 = 0.5 * (hi_f[0] + max(lo_f[0], hi_f[0] - 40.0))
        wrap_f = (wf1, wf1 + 34.0 * knee)
        # back: forward limit = the seat / backs of the thighs, backward limit = along the back plate
        tb = ch["tabard_b"]
        b1 = max(tb.rest[0] + fl * 18, Tb + 10.0)
        b2 = max(tb.rest[1] + fl * 28, (Tb + 8.0) * 0.95, b1 - 3)
        lo_b = (max(Tb + 10.0, min(bp.hip_pitch - 16.0, -T + 77.0)),
                max((Tb + 8.0) * 0.95, -90.0 + (-T + 130.0) * knee))    # tucked: along the thigh backs
        hi_b = (L + 180.0 - 28.0, L + 180.0 - 22.0)
        wrap_b = (lo_b[0] + 3.0, lo_b[1] + 4.0)
        wf = self._ground
        for name, info, free, lo, hi, wrap in (("tabard_f", tf, (w1, w2), lo_f, hi_f, wrap_f),
                                               ("tabard_b", tb, (b1, b2), lo_b, hi_b, wrap_b)):
            a = self._panel(free, ga, gs, lo, hi, wrap)
            a = self._above_ground(name, info, [a[0] + wav * 0.5, a[1] + wav], wf, "pelvis", lo, hi)
            C.set_chain(p, info, a, accp)

    def _panel(self, free, ga, gs, lo, hi, wrap):
        def clamp(v):
            out = []
            for i in range(2):
                lo_i, hi_i = (lo[i], hi[i]) if lo[i] <= hi[i] else ((lo[i] + hi[i]) / 2,) * 2
                out.append(min(max(v[i], lo_i), hi_i))
            return out
        hang = list(free)
        if gs > 0:
            for i, k in enumerate((0.78, 0.98)):
                d = self._arc(ga - hang[i])
                hang[i] += d * gs * k * (1.0 - (abs(d) / 180.0) ** 24)
        hang = clamp(hang)
        if gs <= 0:
            return hang
        body = clamp(wrap)
        dev_h, dev_b = abs(self._arc(hang[1] - ga)), abs(self._arc(body[1] - ga))
        beta = self._smooth(-15.0, 15.0, dev_b - dev_h)          # 1 → hanging is lower
        beta = max(beta, 1.0 - gs)
        return [body[i] + (hang[i] - body[i]) * beta for i in range(2)]

    def _world_fn(self, p: Pose, bp: BP):
        """Ground frame for the cloth: posed (pre-spin) bone matrices and armature point → world point after the
        spin / lift. None while the body is upright (no chain can reach the ground)."""
        if abs(bp.spin) < 1e-6 and bp.lift >= 0 and bp.pelvis.z > -0.25:
            return None
        mats = anim.evaluate(p, return_matrices=True)
        if abs(bp.spin) > 1e-6:
            Rm = Quaternion(Vector(bp.spin_axis).normalized(), math.radians(bp.spin)).to_matrix()
            P = bp.pivot + Vector((0, 0, bp.lift))
        else:
            Rm, P = Quaternion().to_matrix(), Vector()

        def world(x: Vector) -> Vector:
            return P + Rm @ (x - P)
        return world, mats, Rm

    # clearance of each chain's bone line above the ground (cloth half-thickness + hull; the crest is thick)
    GROUND_CLEAR = {"tabard_f": 0.045, "tabard_b": 0.045, "cape_c": 0.026, "cape_l": 0.026, "cape_r": 0.026,
                    "plume": 0.045}

    def _above_ground(self, prefix: str, info, a, wf, parent: str, lo=None, hi=None):
        """Chain angles (armature-space, deg back from down) → the same with every link whose end would go below
        the ground turned to the nearest angle that keeps it on the ground (cloth lies on the floor instead of
        through it). ``lo``/``hi`` (per link) bound the search so a panel never swings through the body."""
        if wf is None:
            return list(a)
        world, mats, Rm = wf
        bones = self.rig.obj.data.bones
        clear = self.GROUND_CLEAR[prefix]
        pm = mats[parent] @ bones[parent].matrix_local.inverted()
        s = world(pm @ bones[info.names[0]].head_local)
        out = list(a)
        for i, n in enumerate(info.names):
            ln = bones[n].length

            def end(ang):
                r = math.radians(ang)
                return s + Rm @ Vector((0.0, math.sin(r), -math.cos(r))) * ln
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
                if best is None:          # root already on the floor: lay the link as flat as it goes
                    best = max((out[i] + d for d in range(-90, 91, 2) if lo_i <= out[i] + d <= hi_i),
                               key=lambda c: end(c).z, default=out[i])
                out[i] = best
            s = end(out[i])
        return out

    def solve(self, bp: BP) -> Pose:
        return C.to_pose(self.rig, bp, self.cloth)

    # ── ready (web READY :558-572) ──────────────────────────────────────────────────────────────────────
    READY_THETA, READY_LAT = 118.0, -0.40      # web READY wpn 1.95 rad (112°); tipped 6° lower so the fist shows

    def ready(self) -> BP:
        wd, wu = C.blade(self.READY_THETA, lat=self.READY_LAT)
        return BP(
            pelvis=self.web_pelvis(47.0, 64.6) + V(0, 0, 0.0),
            hip_pitch=1.5, hip_yaw=9.0, lean=2.5, twist=-10.0, head_pitch=-3.0, head_yaw=4.0,
            foot_r=self.web_foot(54.0, -0.118), foot_l=self.web_foot(41.5, 0.128),
            foot_r_rot=(0, 0, 7), foot_l_rot=(0, 0, 26),
            knee_r=V(-0.10, 1, 0), knee_l=V(0.35, 1, 0),
            hand_r=V(-0.245, 0.20, 0.80), elbow_r=V(-0.5, -0.8, -0.35),
            wpn=wd, wpn_up=wu,
            hand_l=V(0.03, 0.28, 1.02), elbow_l=V(1.0, 0.25, -0.55), wrist_l=(0.0, 0.0, 0.0),
            off=V(-0.40, 0.90, 0.16), off_up=V(0, 0, 1),
            clav_l=(0.0, 6.0), flow=0.12, off_fit=1.0,
        )

    # ── generic per-clip layers ─────────────────────────────────────────────────────────────────────────
    @staticmethod
    def waver(period_ms: float, phase: float = 0.0):
        def layer(bp: BP, t: float) -> BP:
            return bp.but(wave=2 * math.pi * t / period_ms + phase)
        return layer

    def pose(self, base: BP | None = None, **kw) -> BP:
        return (base or self.ready()).but(**kw)

    def blade_kw(self, theta: float, lat: float = 0.0, roll: float = 0.0) -> dict:
        d, e = C.blade(theta, lat=lat, roll=roll)
        return dict(wpn=d, wpn_up=e)

    # ── chest-relative authoring ────────────────────────────────────────────────────────────────────────
    def chest_m(self, bp: BP):
        """Posed chest (spine_03) transform relative to the bind pose for ``bp`` (arms ignored)."""
        p = C.to_pose(self.rig, bp.but(hand_l=None, hand_r=None), None)
        m = anim.evaluate(p, return_matrices=True)["spine_03"]
        return m @ self.rig.obj.data.bones["spine_03"].matrix_local.inverted()

    def chest(self, bp: BP, lat: float, fwd: float, up: float) -> Vector:
        """A point authored in the **bind-pose** body frame (V(lat, fwd, up)) carried by the posed chest: hand
        targets that stay in front of the shoulders whatever the key's lunge, lean and twist (absolute body-space
        targets put the shield hand behind a lunging, leaning torso — the arm wrapped round the back through
        the cape)."""
        return self.chest_m(bp) @ V(lat, fwd, up)

    def chest_dir(self, bp: BP, lat: float, fwd: float, up: float) -> Vector:
        return (self.chest_m(bp).to_3x3() @ V(lat, fwd, up)).normalized()

    def shield_at(self, bp: BP, lat: float, fwd: float, up: float, face=(-0.40, 0.90, 0.16), **kw) -> BP:
        """Shield hand + facing, both chest-relative (``face`` = V(lat, fwd, up) of the shield normal)."""
        return bp.but(hand_l=self.chest(bp, lat, fwd, up), off=self.chest_dir(bp, *face),
                      off_up=self.chest_dir(bp, 0.0, 0.0, 1.0), **kw)

    @staticmethod
    def look(bp: BP, k: float = 0.8, max_down: float = 8.0, yaw: float = 0.0) -> BP:
        """Keep the visor on the strike line (art review 3): the head turns back ``k`` of the chest + hip turn,
        and tips down at most ``max_down`` degrees in the world however far the body leans."""
        hp = bp.head_pitch - max(0.0, bp.hip_pitch + bp.lean + bp.head_pitch - max_down)
        return bp.but(head_yaw=-k * (bp.twist + bp.hip_yaw) + yaw, head_pitch=hp)

    def clip(self, name, length, keys, span, loop=False, notifies=None, ref_speed=None, additive=False,
             layer=None, post=None):
        lay = layer or self.waver(length)
        c = C.body_clip(self.rig, name, length, C.keyed(keys, span, lay), cloth=self.cloth, loop=loop,
                        notifies=notifies, ref_speed=ref_speed, additive=additive, post=post or self.plant)
        c.key_poses_ms = sorted({round(k.t * span, 1) for k in keys})   # key poses (review sheets)
        return c

    def plant(self, bp: BP, t: float = 0.0) -> BP:
        """Post step of every upright clip (art review 7): no sabaton, heel or toe below the floor."""
        return C.plant_feet(self.rig, bp, self.feet) if self.skin() is not None else bp

    # plates the cape lies under: on the floor they rest on the folded cloth, not through it
    ON_CAPE = ("cuirass", "ridge", "side_seam", "gorget", "belt", "buckle", "buckle_hole", "buckle_tongue", "fauld",
               "mailskirt")
    ON_CAPE_CLEAR = 0.035

    def grounded(self, snap: bool = False):
        """Post step of rolls / falls: the whole body (+ sword and shield) kept on the floor, measured on the real
        skinned meshes (``snap``: the lowest point rests exactly on it; the back plate and hips rest on the cape)."""
        def post(bp: BP, t: float) -> BP:
            if snap:
                bp = bp.but(pivot=self.J["pelvis"] + bp.pelvis)
            if self.skin() is None:
                return C.ground_clamp(self.rig, bp, self.cloth, self.PROBES + self.weapon_probes, 0.0, snap=snap)
            if not hasattr(self, "_ground_clear"):
                self._ground_clear = np.where(self.ground_pts.mask(self.ON_CAPE), self.ON_CAPE_CLEAR, 0.0)
            return C.ground_clamp(self.rig, bp, self.cloth, self.ground_pts, self._ground_clear, snap=snap)
        return post

    # ── Idle (1000 ms loop, web idlePose) ────────────────────────────────────────────────────────────────
    def idle(self):
        R0 = self.ready()
        U = self.U

        def s(t: float) -> BP:
            ph = 2 * math.pi * t / 1000.0
            b = math.sin(ph)
            wd, wu = C.blade(self.READY_THETA + math.degrees(math.sin(ph - 0.3) * 0.03), lat=self.READY_LAT)
            return R0.but(
                pelvis=R0.pelvis + V(0, 0, b * 0.55 * U),
                lean=R0.lean - b * 1.0, head_pitch=R0.head_pitch + math.degrees(math.sin(ph - 0.6) * 0.03),
                hand_r=R0.hand_r + V(0, 0, b * 0.45 * U), wpn=wd, wpn_up=wu,
                hand_l=R0.hand_l + V(0, 0, math.sin(ph - 0.5) * 0.55 * U),
                clav_l=(math.sin(ph - 0.4) * 1.5, R0.clav_l[1]), clav_r=(math.sin(ph - 0.4) * 1.5, 0.0),
                flow=0.12 + math.sin(ph) * 0.05, wave=ph, sway=math.sin(ph + 1.0) * 0.15)
        return C.body_clip(self.rig, "Idle", 1000.0, s, cloth=self.cloth, loop=True, post=self.plant)

    # ── locomotion (no foot sliding, R9): Run 700 ms @ 3.333 m/s, Walk 1000 ms @ 1.2 m/s ────────────────
    def gait_feet(self, t: float, speed: float, cycle: float, duty: float, lift: float, bob: float,
                  peel: float = 30.0):
        fl, fr, dz, swing = anim.run_gait(t, speed, cycle, duty=duty, lift=lift, bob=bob)
        out = []
        for off, ph0, lat in ((fl, 0.0, 0.0), (fr, 0.5, 0.0)):
            p = (t + ph0) % 1.0
            pitch, toe, raise_, back = 0.0, 0.0, 0.0, 0.0
            if p < duty:
                s = p / duty
                if s > 0.6:                          # heel peels off about the ball
                    k = (s - 0.6) / 0.4
                    pitch = peel * k * k
                    toe = pitch
            else:
                s = (p - duty) / (1 - duty)
                if s < 0.45:                         # toe-off: foot trails toe-down, levelling out
                    pitch = peel * (1 - s / 0.45) ** 1.5
                elif s > 0.8:                        # reach: toe up before the heel strikes
                    pitch = -12.0 * (s - 0.8) / 0.2
            if pitch > 0:
                r = 0.13
                raise_ = r * math.sin(math.radians(pitch))
                back = r * (1 - math.cos(math.radians(pitch)))
            out.append((off + V(0, -back, raise_ if p < duty else raise_ * 0.6), pitch, toe))
        return out, dz, swing

    def locomotion(self, name: str, length: float, speed: float, duty: float, lift: float, bob: float,
                   lean: float, arm: float, flow: float, crouch: float, stance: float = 0.10):
        J = self.J
        R0 = self.ready()

        def s(t_ms: float) -> BP:
            t = (t_ms / length) % 1.0
            ((fl, pl, tl), (fr, pr, tr)), dz, swing = self.gait_feet(t, speed, length, duty, lift, bob)
            ph = 2 * math.pi * t
            sw = max(-1.0, min(1.0, swing))
            # blade carried outside the legs and trailing (review: the forward guard ran down the screen across
            # the legs whenever the hero moved toward the camera); Idle keeps the forward guard. The sword hand
            # never swings behind the hip (its pommel went into the cape's side)
            wd, wu = C.blade(self.LOCO_THETA + 6 * sw, lat=self.LOCO_LAT)
            sp = speed / RUN_SPEED
            bp = BP(
                pelvis=V(0, 0.02 * sp, crouch + dz), hip_pitch=2.0, hip_yaw=-8.0 * sw * sp ** 0.5,
                hip_roll=-2.5 * math.sin(ph), lean=lean + 1.5 * abs(math.sin(ph)), twist=10.0 * sw * sp ** 0.5,
                head_pitch=-lean * 0.55, head_yaw=-4.0 * sw,
                foot_l=J["ankle_l"] + V(stance - 0.098, 0, 0) + fl, foot_r=J["ankle_r"] + V(-(stance - 0.098), 0, 0) + fr,
                foot_l_rot=(pl, 0, 6), foot_r_rot=(pr, 0, 6), toe_l=tl, toe_r=tr,
                knee_l=V(0.12, 1, 0), knee_r=V(-0.12, 1, 0),
                hand_r=V(-0.32, 0.12 + arm * 0.7 * sw, 0.90 + 0.035 * abs(sw)), elbow_r=V(-0.6, -0.7, -0.3),
                wpn=wd, wpn_up=wu, elbow_l=R0.elbow_l, clav_l=R0.clav_l, clav_r=(1.5 * abs(sw), 3.0 * sw),
                flow=flow + 0.08 * math.sin(2 * ph), wave=2 * ph, sway=0.25 * math.sin(ph), off_fit=1.0)
            # the READY shield rides with the leaning chest (an absolute target put the helm into its top edge)
            return self.shield_guard(bp, (0.0, -0.02 * sw, 0.012 * math.sin(2 * ph)))
        notif = anim.run_contacts_ms(length)
        return C.body_clip(self.rig, name, length, s, cloth=self.cloth, loop=True, notifies=notif, ref_speed=speed,
                           post=self.plant)

    # locomotion blade: 165° from vertical, tilted 35° outward. (130–140° / lat −0.45 still projected straight down
    # the screen over the near leg in the se view at the W1 camera — the forward and outward components cancel.)
    LOCO_THETA, LOCO_LAT = 165.0, -0.70

    def run(self):
        return self.locomotion("Run", 700.0, RUN_SPEED, duty=0.30, lift=0.16, bob=0.032, lean=9.0, arm=0.14,
                               flow=0.38, crouch=-0.075)

    def walk(self):
        return self.locomotion("Walk", 1000.0, WALK_SPEED, duty=0.62, lift=0.075, bob=0.014, lean=4.0, arm=0.08,
                               flow=0.18, crouch=-0.035)

    # Shield guard of the READY pose in the bind-pose chest frame (keys carry it with the chest, then open it)
    def guard(self) -> tuple[Vector, Vector]:
        if not hasattr(self, "_guard"):
            R0 = self.ready()
            m = self.chest_m(R0).inverted()
            self._guard = (m @ R0.hand_l, (m.to_3x3() @ R0.off).normalized())
        return self._guard

    def shield_guard(self, bp: BP, d=(0.0, 0.0, 0.0), turn: float = 0.0, **kw) -> BP:
        """The READY shield carried by the chest, moved by ``d`` = V(lat, fwd, up) (bind frame) and turned
        ``turn`` degrees outward (to the character's left) about the vertical."""
        g, f = self.guard()
        p = g + V(*d)
        f = Quaternion(Vector((0, 0, 1)), math.radians(turn)) @ f
        m = self.chest_m(bp)
        return bp.but(hand_l=m @ p, off=(m.to_3x3() @ f).normalized(), off_up=(m.to_3x3() @ Vector((0, 0, 1))).normalized(),
                      **kw)

    # ── Attack01: overhead diagonal chop (web ATTACK :584-615; 615 ms, Contact 308, keys over 538 ms) ───
    # Contact silhouette: lunge, lean 23°, blade cutting down across the body (129°), shield opened to the left.
    def attack01(self):
        R0 = self.ready()
        # wind-up (art review 4): the sword hand above and behind the crown (1.9 m), blade up-back −52°, so the
        # gauntlet clears the helm instead of covering the visor (the web's raised-blade frames 2–3)
        W = self.pose(R0, pelvis=self.web_pelvis(45.0, 66.6), lean=-13.8, hip_pitch=-2.0, head_pitch=2.0,
                      twist=-26.0, hip_yaw=4.0,
                      foot_r=self.web_foot(56.5, -0.12), foot_l=self.web_foot(40.0, 0.13),
                      hand_r=V(-0.31, -0.28, 1.95), elbow_r=V(-0.9, 0.3, 0.1), **self.blade_kw(-52, -0.45),
                      clav_r=(30.0, -6.0), flow=0.25)
        W = self.look(self.shield_guard(W, (0.02, 0.0, 0.0)), k=0.6)
        # the raise arcs out to the right first, so blade and pommel never sweep through the cuirass or the helm
        Wa = C.lerp_bp(R0, W, 0.45).but(hand_r=V(-0.46, 0.08, 1.30), elbow_r=V(-1.0, 0.0, -0.2),
                                         wpn=V(-0.80, 0.10, 0.58).normalized(), wpn_up=V(0, 0, 1))
        S = self.pose(R0, pelvis=self.web_pelvis(48.5, 66.8), lean=5.7, hip_pitch=1.0, head_pitch=3.0, twist=-8.0,
                      hip_yaw=8.0, foot_r=self.web_foot(58.5, -0.12), foot_l=self.web_foot(40.0, 0.13),
                      hand_r=V(-0.25, 0.50, 1.48), elbow_r=V(-0.9, -0.2, -0.4), **self.blade_kw(43, -0.15),
                      clav_r=(12.0, 6.0), flow=0.35)
        S = self.look(self.shield_guard(S, (0.13, -0.03, 0.0), turn=22.0, elbow_l=V(1.0, 0.1, -0.4)))
        # contact (art review 6): a true downward chop — the fist driven down to chest height in front of the
        # sternum, blade 129° down the strike line (no sideways diagonal), the head over the front knee, the hips
        # and chest square to the target (twist 4°) — not the twisted, cape-flared silhouette of Attack03 / Cast01
        Ct = self.pose(R0, pelvis=self.web_pelvis(53.0, 68.0), lean=24.0, hip_pitch=4.0, head_pitch=-6.0, twist=4.0,
                       hip_yaw=4.0, foot_r=self.web_foot(61.0, -0.12), foot_l=self.web_foot(40.5, 0.14),
                       foot_l_rot=(8.0, 0, 24), toe_l=8.0,
                       hand_r=V(-0.13, 0.66, 1.06), elbow_r=V(-0.8, -0.3, -0.5), **self.blade_kw(129, 0.04),
                       clav_r=(4.0, 14.0), flow=0.55)
        Ct = self.look(self.shield_guard(Ct, (0.22, -0.07, 0.04), turn=34.0, elbow_l=V(1.0, 0.2, -0.4)))
        F = self.pose(Ct, pelvis=self.web_pelvis(52.5, 68.4), lean=26.0, head_pitch=-8.0, twist=6.0,
                      hand_r=V(-0.12, 0.58, 0.84), **self.blade_kw(146, 0.06), clav_r=(2.0, 12.0), flow=0.45)
        F = self.look(self.shield_guard(F, (0.18, -0.04, 0.08), turn=30.0, elbow_l=V(1.0, 0.2, -0.4)))
        Rc = self.pose(R0, pelvis=self.web_pelvis(49.0, 66.6), lean=12.6, head_pitch=-4.0, twist=0.0, hip_yaw=10.0,
                       foot_r=self.web_foot(56.0, -0.12), hand_r=V(-0.17, 0.36, 0.78), **self.blade_kw(125, -0.1),
                       flow=0.25)
        Rc = self.look(self.shield_guard(Rc, (0.08, 0.0, 0.04), turn=12.0))
        keys = [K(0.0, R0), K(0.13, Wa, "out"), K(0.28, W), K(0.43, S, "in"), K(0.571, Ct, "linear"),
                K(0.71, F, "out"), K(0.86, Rc), K(1.0, R0)]
        return self.clip("Attack01", 615.0, keys, 538.0, notifies={"Contact": 308.0})

    # ── Attack02: forehand horizontal sweep, right → left (same timing) ─────────────────────────────────
    # Contact silhouette: square, wide stance, chest turned into the cut, blade level across the body at shoulder
    # height sweeping to the left, the shield arm flung out low to the left (counterweight, out of the blade's way).
    def attack02(self):
        R0 = self.ready()
        W = self.pose(R0, pelvis=R0.pelvis + V(0, -0.05, -0.06), lean=4.0, twist=-40.0, hip_yaw=-10.0,
                      foot_r=self.web_foot(52.0, -0.17), foot_l=self.web_foot(43.0, 0.16),
                      hand_r=V(-0.44, -0.08, 1.22), elbow_r=V(-0.6, -0.6, -0.5),
                      wpn=V(-0.62, -0.78, 0.10).normalized(), wpn_up=V(0, 0, 1), clav_r=(8.0, -10.0), flow=0.22,
                      wrist_r=(0.0, 40.0, 0.0))
        W = self.look(self.shield_guard(W, (0.0, 0.02, 0.0), turn=-6.0))
        S = self.pose(W, pelvis=R0.pelvis + V(0, 0.02, -0.09), lean=8.0, twist=-12.0, hip_yaw=0.0,
                      foot_r=self.web_foot(54.0, -0.19), hand_r=V(-0.40, 0.30, 1.26), elbow_r=V(-0.8, -0.4, -0.4),
                      wpn=V(-0.92, 0.38, 0.04).normalized(), clav_r=(6.0, 4.0), flow=0.32)
        S = self.look(self.shield_guard(S, (0.16, -0.02, -0.10), turn=40.0, elbow_l=V(1.0, 0.0, -0.3)))
        Ct = self.pose(R0, pelvis=R0.pelvis + V(0, 0.06, -0.12), lean=10.0, hip_pitch=2.0, twist=26.0, hip_yaw=10.0,
                       foot_r=self.web_foot(55.5, -0.21), foot_l=self.web_foot(42.0, 0.20),
                       foot_r_rot=(0, 0, -10), foot_l_rot=(10.0, 0, 34), toe_l=10.0,
                       hand_r=V(-0.02, 0.58, 1.28), elbow_r=V(-0.6, -0.6, -0.5),
                       wpn=V(0.74, 0.67, 0.04).normalized(), wpn_up=V(0, 0, 1), wrist_r=(0.0, 40.0, 0.0),
                       clav_r=(6.0, 16.0), flow=0.5)
        Ct = self.look(Ct.but(hand_l=V(0.50, 0.12, 0.84), elbow_l=V(1.0, -0.2, 0.2),
                              off=V(0.70, 0.66, -0.20).normalized(), off_up=V(0.25, 0.0, 1.0).normalized(),
                              clav_l=(-4.0, -8.0)))
        F = self.pose(Ct, twist=38.0, hip_yaw=16.0, hand_r=V(0.28, 0.42, 1.30), elbow_r=V(-0.3, 0.5, -0.6),
                      wpn=V(0.42, -0.90, 0.10).normalized(), clav_r=(4.0, 18.0), flow=0.42)
        F = self.look(F.but(hand_l=V(0.52, 0.06, 0.82)))
        # recovery swings the blade back level to forward-left (above the low shield, clear of the helm), then
        # down into the guard on the right
        Rc = self.pose(R0, pelvis=R0.pelvis + V(0, 0.04, -0.06), lean=7.0, twist=10.0, hip_yaw=8.0,
                       foot_r=self.web_foot(54.0, -0.15), foot_l=self.web_foot(42.0, 0.16),
                       hand_r=V(0.0, 0.50, 1.15), elbow_r=V(-0.8, -0.4, -0.4),
                       wpn=V(0.55, 0.83, -0.05).normalized(), wpn_up=V(0, 0, 1), flow=0.25)
        Rc = self.look(Rc.but(hand_l=V(0.36, 0.16, 0.90), elbow_l=V(1.0, 0.0, -0.3),
                              off=V(0.35, 0.92, 0.0).normalized(), off_up=V(0, 0, 1)))
        keys = [K(0.0, R0), K(0.28, W, "out"), K(0.43, S, "in"), K(0.571, Ct, "linear"), K(0.71, F, "out"),
                K(0.86, Rc), K(1.0, R0)]
        return self.clip("Attack02", 615.0, keys, 538.0, notifies={"Contact": 308.0})

    # ── Attack03: lunging thrust (same timing) ──────────────────────────────────────────────────────────
    # Contact silhouette: the longest, lowest stance of the set — front knee deep, back leg straight, the sword
    # arm at full extension at chest height, blade level, the shield drawn in to the chest.
    def attack03(self):
        R0 = self.ready()
        W = self.pose(R0, pelvis=R0.pelvis + V(0, -0.07, -0.07), lean=-2.0, twist=-30.0, hip_yaw=-6.0,
                      foot_r=self.web_foot(55.0, -0.12), foot_l=self.web_foot(41.0, 0.13),
                      hand_r=V(-0.37, -0.08, 1.06), elbow_r=V(-0.8, -0.6, -0.2), **self.blade_kw(84, -0.30, roll=70),
                      clav_r=(4.0, -14.0), flow=0.2)
        W = self.look(self.shield_guard(W, (0.20, 0.02, 0.04), turn=24.0))
        S = self.pose(W, pelvis=R0.pelvis + V(0, 0.05, -0.11), lean=8.0, twist=-8.0, hip_yaw=4.0,
                      foot_r=self.web_foot(60.0, -0.13), hand_r=V(-0.20, 0.30, 1.02), **self.blade_kw(89, 0.04, roll=70),
                      flow=0.3)
        S = self.look(self.shield_guard(S, (0.14, -0.02, 0.04), turn=30.0))
        # contact (art review 6): a straight, square-on thrust — hips and chest facing the target (no twist), the
        # sword arm locked straight out from the shoulder at chest height, the blade level along the strike line,
        # the shield tucked to the chest; the longest, lowest stance of the set
        Ct = self.pose(R0, pelvis=self.web_pelvis(54.5, 72.0), lean=14.0, hip_pitch=5.0, twist=0.0, hip_yaw=2.0,
                       foot_r=self.web_foot(68.0, -0.15), foot_l=self.web_foot(36.5, 0.16),
                       foot_l_rot=(18.0, 0, 30), toe_l=18.0, knee_r=V(-0.15, 1, 0),
                       hand_r=V(-0.11, 0.92, 1.05), elbow_r=V(-0.9, 0.0, -0.5), **self.blade_kw(83, 0.0, roll=70),
                       clav_r=(4.0, 24.0), flow=0.55)
        Ct = self.look(self.shield_guard(Ct, (0.10, -0.06, 0.05), turn=26.0))
        F = self.pose(Ct, hand_r=V(-0.11, 0.89, 1.02), **self.blade_kw(86, 0.0, roll=70), flow=0.45)
        F = self.look(self.shield_guard(F, (0.10, -0.06, 0.05), turn=26.0))
        Rc = self.pose(R0, pelvis=R0.pelvis + V(0, 0.06, -0.08), lean=10.0, twist=6.0, hip_yaw=12.0,
                       foot_r=self.web_foot(58.0, -0.13), hand_r=V(-0.20, 0.38, 0.88), **self.blade_kw(108, -0.15),
                       flow=0.25)
        Rc = self.look(self.shield_guard(Rc, (0.08, 0.0, 0.02), turn=14.0))
        keys = [K(0.0, R0), K(0.28, W, "out"), K(0.43, S, "in"), K(0.571, Ct, "linear"), K(0.71, F, "out"),
                K(0.86, Rc), K(1.0, R0)]
        return self.clip("Attack03", 615.0, keys, 538.0, notifies={"Contact": 308.0})

    # ── Cast01: sword raised overhead → thrust forward (web CAST :617-637; 727 ms, Release 334) ─────────
    # Release silhouette: upright, the arm straight out at shoulder height pointing the blade at the target (76°,
    # a little above the web's 83° so it reads as pointing, not thrusting), the shield lowered to the side —
    # distinct from the low, lunging Attack03.
    def cast01(self):
        R0 = self.ready()
        # (review: the blade stood in front of the visor; the fist rises to crown height beside the helm, forearm
        # level — a blade raised along a raised forearm drives the pommel into the vambrace)
        Ch = self.pose(R0, pelvis=self.web_pelvis(47.0, 67.4), lean=-3.4, head_pitch=-11.5, twist=-6.0,
                       foot_r=self.web_foot(55.5, -0.12), foot_l=self.web_foot(40.5, 0.13),
                       hand_r=V(-0.50, 0.02, 1.56), elbow_r=V(-1.0, -0.7, 0.0), **self.blade_kw(4, -0.22),
                       clav_r=(18.0, 4.0), flow=0.18)
        Ch = self.shield_guard(Ch, (0.14, -0.02, -0.06), turn=26.0, elbow_l=V(1.0, 0.2, -0.4))
        Ch = Ch.but(head_yaw=-0.6 * (Ch.twist + Ch.hip_yaw))
        Rl = self.pose(R0, pelvis=self.web_pelvis(50.0, 66.4), lean=13.0, hip_pitch=2.0, head_pitch=-4.0, twist=12.0,
                       hip_yaw=10.0, foot_r=self.web_foot(57.0, -0.14), foot_l=self.web_foot(41.0, 0.15),
                       foot_l_rot=(6.0, 0, 28), toe_l=6.0,
                       hand_r=V(-0.10, 0.74, 1.34), elbow_r=V(-0.7, -0.3, -0.6), **self.blade_kw(76, 0.04, roll=60),
                       clav_r=(10.0, 16.0), flow=0.5)
        # (art review 6) the shield arm drawn back and out to the left, face turned outward — the chest opens and
        # the release reads as a pointing gesture, not another strike
        Rl = self.look(self.shield_guard(Rl, (0.30, -0.24, -0.14), turn=68.0, elbow_l=V(1.0, -0.6, -0.4),
                                         off_fit=1.8, clav_l=(2.0, -14.0)))
        H = self.pose(Rl, lean=11.0, hand_r=V(-0.10, 0.70, 1.30), **self.blade_kw(80, 0.04, roll=60), flow=0.3)
        H = self.look(H)
        # recovery: the sword comes down into the guard first, the shield swings back across afterwards
        Rc = self.look(self.shield_guard(self.pose(R0, lean=6.0, flow=0.2), (0.18, -0.10, -0.08), turn=40.0,
                                         elbow_l=V(1.0, -0.2, -0.4), off_fit=1.8))
        keys = [K(0.0, R0), K(0.36, Ch, "out"), K(0.5, Rl, "in"), K(0.72, H), K(0.88, Rc), K(1.0, R0)]
        return self.clip("Cast01", 727.0, keys, 636.0, notifies={"Release": 334.0})

    # ── Cast02: shield-up war cry (buffs: shield_wall / iron_fortress / taunt_roar / frenzy) ────────────
    def cast02(self):
        R0 = self.ready()
        Ch = self.pose(R0, pelvis=R0.pelvis + V(0, -0.02, -0.10), lean=14.0, head_pitch=14.0, twist=-8.0,
                       foot_r=self.web_foot(55.0, -0.15), foot_l=self.web_foot(41.0, 0.15),
                       hand_r=V(-0.40, -0.04, 0.84), elbow_r=V(-0.6, -0.7, -0.2), **self.blade_kw(-125, -0.75),
                       flow=0.2)
        Ch = self.shield_guard(Ch, (0.0, -0.02, -0.02))
        Rl = self.pose(R0, pelvis=R0.pelvis + V(0, 0.01, -0.02), lean=-9.0, hip_pitch=-2.0, head_pitch=-22.0,
                       twist=4.0, foot_r=self.web_foot(56.0, -0.16), foot_l=self.web_foot(41.0, 0.16),
                       hand_r=V(-0.50, 0.08, 1.66), elbow_r=V(-1.0, -0.5, 0.2), **self.blade_kw(-6, -0.75),
                       clav_r=(18.0, 0.0), clav_l=(10.0, 8.0), flow=0.5, plume=-6.0)
        Rl = self.shield_guard(Rl, (0.16, 0.0, 0.20), turn=28.0, elbow_l=V(1.0, -0.2, -0.5))
        H = self.pose(Rl, head_pitch=-16.0, lean=-6.0, hand_r=V(-0.49, 0.08, 1.62), flow=0.32)
        H = self.shield_guard(H, (0.16, 0.0, 0.18), turn=28.0)
        keys = [K(0.0, R0), K(0.36, Ch, "out"), K(0.5, Rl, "in"), K(0.72, H), K(1.0, R0)]
        return self.clip("Cast02", 727.0, keys, 636.0, notifies={"Release": 334.0})

    # ── Hurt 333 ms (web HURT; keys over 250 ms) + HurtAdd (upper-body jolt, additive on frame 0) ───────
    def hit_pose(self) -> BP:
        """HIT: knocked back, head snapped back, weapon arm thrown up and back (blade near vertical, outward) so
        the recoil reads in silhouette from the −50° camera, not only through the helm tilt."""
        R0 = self.ready()
        return self.pose(R0, pelvis=self.web_pelvis(44.5, 65.8), lean=-19.5, hip_pitch=-3.0, head_pitch=-21.8,
                         head_yaw=6.0, foot_r=self.web_foot(54.5, -0.12), foot_l=self.web_foot(40.0, 0.13),
                         hand_r=V(-0.40, -0.16, 1.28), elbow_r=V(-0.8, -0.5, -0.3), **self.blade_kw(14, -0.45),
                         hand_l=V(0.10, 0.20, 1.10), off=V(-0.15, 0.95, 0.30), clav_r=(14.0, -6.0), clav_l=(8.0, 2.0),
                         flow=0.45, plume=10.0)

    def hurt(self):
        R0, Hp = self.ready(), self.hit_pose()
        mid = self.pose(Hp, pelvis=self.web_pelvis(44.0, 67.2), lean=-14.9, head_pitch=-12.6, flow=0.35)
        keys = [K(0.0, Hp), K(0.33, mid), K(1.0, R0)]
        return self.clip("Hurt", 333.0, keys, 250.0)

    def hurt_add(self):
        R0 = self.ready()
        J = self.pose(R0, lean=R0.lean - 10.0, head_pitch=R0.head_pitch - 14.0, twist=R0.twist + 6.0,
                      hand_r=R0.hand_r + V(-0.05, -0.04, 0.06), hand_l=R0.hand_l + V(0.0, -0.04, 0.05),
                      clav_r=(6.0, -3.0), clav_l=(6.0, R0.clav_l[1]), flow=0.3, plume=6.0)
        keys = [K(0.0, R0), K(60.0 / 333.0, J, "out"), K(1.0, R0, "smooth")]
        return self.clip("HurtAdd", 333.0, keys, 333.0, additive=True)

    # ── Dodge 300 ms: forward tuck roll about mid-spine (web DODGE :645-667; keys over 250 ms) ──────────
    # tucked blade: out to the right and trailing back along the roll (web 149° = down the shins, which a 3D
    # forward roll drives into the floor); a sideways/back blade only turns about the roll axis
    TUCK_WPN = V(-0.85, -0.50, 0.05).normalized()

    def tuck(self, x: float, y: float, spin: float) -> BP:
        R0 = self.ready()
        bp = self.pose(R0, pelvis=self.web_pelvis(x, y), lean=40.0, hip_pitch=6.0, head_pitch=29.0, twist=0.0,
                         hip_yaw=0.0, foot_r=self.web_foot(x + 6, -0.11, y + 10), foot_l=self.web_foot(x + 3, 0.11, y + 11),
                         foot_r_rot=(30.0, 0, 0), foot_l_rot=(30.0, 0, 0),
                         hand_r=self.web_hand(x + 6, y - 1, -0.36), elbow_r=V(-0.9, -0.2, -0.4), wpn=self.TUCK_WPN,
                         wpn_up=V(0, 0, 1), elbow_l=V(0.9, -0.2, -0.5),
                         spin=spin, spin_axis=V(1, 0, 0), flow=0.45, plume=12.0, cling=0.6, curl=20.0)
        # the shield rides on the left side of the tuck like a wheel cover — face out, edge-on to the roll, clear
        # of the pauldron, knees and helm
        return self.shield_at(bp, 0.31, 0.12, 1.10, face=(1.0, 0.08, 0.0), elbow_l=V(0.5, -0.4, -0.8))

    def with_pivot(self, bp: BP, k: float = 0.5) -> BP:
        """Spin pivot at ``k`` along the (leaned) spine, like the web ``pivot``."""
        P = self.J["pelvis"] + bp.pelvis
        a = math.radians(bp.lean + bp.hip_pitch)
        return bp.but(pivot=P + V(0, math.sin(a), math.cos(a)) * (self.rig.spec.torso * k))

    # (bone, fraction along the bone, radius): the armour volume around the skeleton, for ground contact
    PROBES = [("head", 0.37, 0.19), ("head", 1.0, 0.04), ("spine_03", 0.5, 0.16), ("spine_01", 0.5, 0.15),
              ("pelvis", 0.0, 0.15), ("thigh_l", 1.0, 0.075), ("thigh_r", 1.0, 0.075),
              ("foot_l", 0.0, 0.0765), ("foot_r", 0.0, 0.0765), ("ball_l", 1.0, 0.023), ("ball_r", 1.0, 0.023),
              ("hand_l", 0.4, 0.066), ("hand_r", 0.4, 0.066), ("lowerarm_l", 0.0, 0.068),
              ("lowerarm_r", 0.0, 0.068), ("upperarm_l", 0.0, 0.11), ("upperarm_r", 0.0, 0.11)]

    @property
    def weapon_probes(self) -> list:
        """Held items for the ground clamp (item-local metres on ``weapon_r`` / ``weapon_l``): sword tip, mid,
        crossguard ends, pommel; shield centre + eight rim points (the bent heater outline)."""
        if not hasattr(self, "_wprobes"):
            import warrior as W
            pr = [("weapon_r", (0.0, W.SWORD_TIP, 0.0), 0.006), ("weapon_r", (0.0, 0.5, 0.0), 0.012),
                  ("weapon_r", (0.0, 0.06, 0.124), 0.016), ("weapon_r", (0.0, 0.06, -0.124), 0.016),
                  ("weapon_r", (0.0, -0.125, 0.0), 0.035),
                  ("weapon_l", (W.SHIELD_CX, W.SHIELD_FACE_Y + 0.012, W.SHIELD_CZ), 0.012)]
            out = W.heater_outline()
            for i in range(0, len(out), max(1, len(out) // 8)):
                x, z = out[i]
                pr.append(("weapon_l", tuple(W._bend_shield(Vector((x, -0.004, z)))), 0.024))
            self._wprobes = pr
        return self._wprobes

    def dodge(self):
        R0 = self.ready()
        d0 = self.pose(R0, pelvis=self.web_pelvis(48.0, 72.0), lean=31.5, head_pitch=14.0, twist=0.0, hip_yaw=0.0,
                       foot_r=self.web_foot(56.0, -0.11), foot_l=self.web_foot(42.0, 0.12), foot_l_rot=(10.0, 0, 10),
                       toe_l=10.0, hand_r=self.web_hand(57.0, 69.0, -0.34), **self.blade_kw(120, -0.7),
                       spin_axis=V(1, 0, 0), flow=0.35, cling=0.6)
        d0 = self.shield_at(d0, 0.30, 0.16, 1.12, face=(0.85, 0.50, 0.0), elbow_l=V(0.7, -0.2, -0.7))
        d8 = self.pose(R0, pelvis=self.web_pelvis(50.0, 71.0), lean=23.0, head_pitch=6.0, twist=0.0, hip_yaw=0.0,
                       foot_r=self.web_foot(57.0, -0.11), foot_l=self.web_foot(44.0, 0.12),
                       hand_r=self.web_hand(58.0, 67.0, -0.37), **self.blade_kw(126, -0.62),
                       spin=344.0, spin_axis=V(1, 0, 0), flow=0.45, cling=0.6)
        d8 = self.shield_at(d8, 0.28, 0.18, 1.12, face=(0.75, 0.65, 0.0), elbow_l=V(0.7, -0.2, -0.7))
        keys = [K(0.0, self.with_pivot(d0)), K(0.2, self.with_pivot(self.tuck(47.0, 73.0, 74.5))),
                K(0.4, self.with_pivot(self.tuck(48.0, 71.0, 166.2)), "linear"),
                K(0.6, self.with_pivot(self.tuck(48.5, 72.0, 257.8)), "linear"),
                K(0.8, self.with_pivot(d8)), K(1.0, self.with_pivot(R0.but(spin=360.0, spin_axis=V(1, 0, 0))))]

        return self.clip("Dodge", 300.0, keys, 250.0, post=self.grounded())

    # Death rest pose (body frame before the −87° fall; +Y back = the floor): sword arm out on the floor with the
    # blade lying flat, shield arm down beside the hip with the heater resting face-up on its rim and the fist
    # (hands relative to the fallen pelvis P100)
    D100_HAND_R = V(-0.58, -0.03, 0.24)
    D100_WPN = V(-0.62, 0.02, -0.78).normalized()
    D100_ROLL = 0.0
    D100_HAND_L = V(0.47, -0.03, 0.16)
    D100_OFF = V(0.62, 0.78, 0.0).normalized()
    D100_OFF_UP = V(0.0, 0.0, 1.0)

    # ── Death 750 ms: stagger → knees → tip over backward → on the back (web DEATH :669-687; 625 ms keys) ─
    def death(self):
        R0, Hp = self.ready(), self.hit_pose()
        d2 = self.pose(Hp, pelvis=self.web_pelvis(43.0, 68.0), lean=-24.0, head_pitch=-25.8,
                       hand_r=V(-0.36, -0.03, 0.80), **self.blade_kw(103, -0.4))
        # on the left knee: knee cop and tucked toes both on the ground (art review 6)
        d45 = self.pose(R0, pelvis=self.web_pelvis(45.0, 78.5), lean=11.5, head_pitch=23.0, twist=-6.0, hip_yaw=4.0,
                        foot_r=self.web_foot(53.0, -0.15), foot_l=self.web_foot(36.0, 0.13, 90.5), foot_l_rot=(62.0, 0, 10),
                        toe_l=52.0, knee_l=V(0.2, 1, -0.2),
                        hand_r=V(-0.40, 0.14, 0.46), elbow_r=V(-0.9, -0.3, 0.1), **self.blade_kw(124, -0.85),
                        hand_l=V(0.39, 0.04, 0.56), elbow_l=V(1.0, 0.0, 0.0), off=V(0.80, 0.45, -0.40),
                        flow=0.2, plume=4.0, cling=0.3, off_fit=2.0)
        P70 = self.J["pelvis"] + self.web_pelvis(45.0, 82.0)
        d70 = self.pose(d45, pelvis=self.web_pelvis(45.0, 82.0), lean=-5.7, head_pitch=-17.0,
                        foot_r=P70 + V(-0.13, 0.20, -0.52), foot_l=P70 + V(0.12, 0.10, -0.58),
                        foot_l_rot=(20.0, 0, 10), toe_l=0.0,
                        hand_r=V(-0.40, 0.05, 0.62), **self.blade_kw(149, -0.6), hand_l=V(0.38, 0.04, 0.62),
                        spin=-54.0, spin_axis=V(1, 0, 0), flow=0.5, cling=0.5)
        P100 = self.J["pelvis"] + self.web_pelvis(47.0, 86.5)
        # dead, not resting (art review 6): the helm lies back on the ground rolled to one side (visor ≈ straight
        # up, not lifted to the chest), the right leg straight, the left splayed out with the knee fallen open
        d100 = self.pose(d45, pelvis=self.web_pelvis(47.0, 86.5), lean=6.0, head_pitch=-12.0, head_roll=-26.0,
                         head_yaw=8.0, twist=-4.0, hip_yaw=0.0, hip_pitch=-4.0,
                         foot_r=P100 + V(-0.12, -0.02, -0.80), foot_l=P100 + V(0.36, 0.05, -0.62),
                         foot_r_rot=(25.0, 0, 30), foot_l_rot=(10.0, 0, 55), toe_l=0.0, knee_l=V(1.0, 0.45, 0),
                         knee_r=V(-0.2, 1, 0),
                         hand_r=P100 + self.D100_HAND_R, elbow_r=V(-0.4, -0.3, -1.0), wpn=self.D100_WPN,
                         wpn_up=V(0, 0, 1), wrist_r=(0.0, self.D100_ROLL, 0.0),
                         hand_l=P100 + self.D100_HAND_L, elbow_l=V(1.0, -0.2, -0.6), off=self.D100_OFF,
                         off_up=self.D100_OFF_UP,
                         spin=-87.0, spin_axis=V(1, 0, 0), flow=0.05, plume=-4.0, cling=1.0)
        keys = [K(0.0, Hp), K(0.2, d2), K(0.45, d45), K(0.7, d70, "in"), K(1.0, d100, "out")]

        return self.clip("Death", 750.0, keys, 625.0, post=self.grounded(snap=True))

    # ── Cast_Whirlwind (R5 signature): coil, 1.25-turn blade sweep (body 360° + uncoil), settle ──────────
    def whirlwind(self):
        R0 = self.ready()
        coil = self.pose(R0, pelvis=R0.pelvis + V(0, 0.0, -0.09), lean=10.0, twist=-50.0, hip_yaw=-16.0, head_yaw=30.0,
                         foot_r=self.web_foot(52.0, -0.17), foot_l=self.web_foot(44.0, 0.17),
                         foot_r_rot=(0, 0, 20), foot_l_rot=(0, 0, 20),
                         hand_r=V(-0.36, -0.20, 1.00), elbow_r=V(-0.4, -0.8, -0.4), wpn=V(-0.45, -0.88, 0.15).normalized(),
                         wpn_up=V(0, 0, 1), clav_r=(4.0, -12.0), flow=0.3)
        coil = self.shield_guard(coil, (0.04, 0.0, 0.0), turn=10.0)
        spin = self.pose(R0, pelvis=R0.pelvis + V(0, 0.0, -0.11), lean=12.0, twist=0.0, hip_yaw=0.0, head_yaw=0.0,
                         head_pitch=4.0, foot_r=self.web_foot(49.5, -0.15), foot_l=self.web_foot(46.5, 0.15),
                         foot_r_rot=(8.0, 0, 15), foot_l_rot=(8.0, 0, 15), toe_l=8.0, toe_r=8.0,
                         hand_r=V(-0.58, 0.14, 1.06), elbow_r=V(-0.8, -0.2, -0.6), wpn=V(-0.96, 0.25, -0.05).normalized(),
                         wpn_up=V(0, 0, 1), elbow_l=V(1.0, -0.1, -0.5),
                         clav_r=(6.0, 6.0), spin_axis=V(0, 0, 1), flow=0.55, plume=8.0,
                         wrist_r=(0.0, -50.0, 0.0))
        spin = self.shield_guard(spin, (0.10, -0.02, 0.0), turn=30.0)
        J = self.J

        def s(t: float) -> BP:
            t0, t1, t2 = 110.0, 560.0, 727.0
            if t <= t0:
                bp = C.lerp_bp(R0, coil, anim.ease(t / t0, "out"))
                return bp.but(spin_axis=V(0, 0, 1), pivot=J["pelvis"] + bp.pelvis, wave=2 * math.pi * t / 727)
            if t <= t1:
                u = (t - t0) / (t1 - t0)
                e = anim.ease(u, "smooth")
                a = 0.12
                bp = C.lerp_bp(coil, spin, min(1.0, u / a) ** 0.7) if u < a else spin.copy()
                # chest uncoils (−50° → +35°) on top of the body turn: the blade sweeps 1.25 turns
                tw = -50.0 + 85.0 * anim.ease(u, "smooth")
                hop = 0.035 * math.sin(math.pi * min(1.0, u / 0.9))
                bp = bp.but(spin=360.0 * e, twist=tw * 0.6, hip_yaw=tw * 0.4, spin_axis=V(0, 0, 1),
                            pivot=J["pelvis"] + bp.pelvis, lift=hop, flow=0.55, sway=0.6 * math.sin(2 * math.pi * u),
                            wave=2 * math.pi * t / 727)
                return bp
            u = (t - t1) / (t2 - t1)
            end = spin.but(twist=35.0 * 0.6, hip_yaw=35.0 * 0.4)
            bp = C.lerp_bp(end, R0, anim.ease(u, "smooth"))
            return bp.but(spin=0.0, spin_axis=V(0, 0, 1), pivot=J["pelvis"] + bp.pelvis, wave=2 * math.pi * t / 727)
        c = C.body_clip(self.rig, "Cast_Whirlwind", 727.0, s, cloth=self.cloth, notifies={"Release": 334.0},
                        post=self.plant)
        c.key_poses_ms = [0.0, 110.0, 220.0, 334.0, 450.0, 560.0, 727.0]
        return c

    # ── Cast_Charge (R5 signature, C4 dash 0.25 s): brace behind the shield, sprint, shield-bash at 334 ──
    def charge(self):
        R0 = self.ready()
        # sword arm low at the side, the blade trailing out to the right and back — outside the cape
        com = dict(lean=26.0, hip_pitch=4.0, head_pitch=-14.0, twist=12.0, hip_yaw=6.0, elbow_l=V(1.0, 0.3, -0.5),
                   hand_r=V(-0.40, 0.04, 0.86), elbow_r=V(-1.0, 0.0, -0.3), wpn=V(-0.70, -0.55, -0.45).normalized(),
                   wpn_up=V(0, 0, 1), clav_l=(4.0, 14.0), flow=0.7, plume=12.0)
        brace = self.pose(R0, pelvis=R0.pelvis + V(0, -0.02, -0.12), foot_r=self.web_foot(43.0, -0.14),
                          foot_l=self.web_foot(55.0, 0.13), foot_r_rot=(20.0, 0, 15), toe_r=20.0, **com)
        brace = self.look(self.shield_guard(brace.but(flow=0.4), (0.02, 0.04, 0.06), turn=-4.0), max_down=14.0)
        brace = brace.but(hand_r=self.chest(brace, -0.37, 0.06, 0.92))
        A = self.pose(R0, pelvis=R0.pelvis + V(0, 0.06, -0.10), foot_l=self.web_foot(58.0, 0.12),
                      foot_r=self.web_foot(44.0, -0.12, 86.0), foot_r_rot=(35.0, 0, 10), toe_r=10.0, **com)
        A = self.look(self.shield_guard(A, (0.02, 0.04, 0.06), turn=-4.0), max_down=14.0)
        A = A.but(hand_r=self.chest(A, -0.37, 0.04, 0.92))
        B = self.pose(R0, pelvis=R0.pelvis + V(0, 0.06, -0.08), foot_r=self.web_foot(57.0, -0.12),
                      foot_l=self.web_foot(43.0, 0.12, 86.0), foot_l_rot=(35.0, 0, 10), toe_l=10.0, **com)
        B = self.look(self.shield_guard(B, (0.02, 0.04, 0.06), turn=-4.0), max_down=14.0)
        B = B.but(hand_r=self.chest(B, -0.37, 0.08, 0.92))
        # the bash (art review 6): shield first — the chest turns right so the left shoulder drives in behind the
        # shield, whose face is square to the strike line (≤ 25°, CHARGE_FACE), the pelvis and chest come up out
        # of the sprint crouch, the sword is cocked high and back over the right shoulder for the follow-up
        hit = self.pose(R0, pelvis=R0.pelvis + V(0, 0.14, -0.09), lean=20.0, hip_pitch=4.0, head_pitch=-12.0,
                        twist=-24.0, hip_yaw=-10.0, foot_l=self.web_foot(62.0, 0.13), foot_r=self.web_foot(37.0, -0.14),
                        foot_r_rot=(28.0, 0, 12), toe_r=28.0, knee_l=V(0.2, 1, 0),
                        elbow_l=V(1.0, -0.3, -0.5), elbow_r=V(-1.0, 0.3, -0.3), **self.blade_kw(-30, -0.55),
                        clav_l=(6.0, 26.0), clav_r=(12.0, 8.0), flow=0.75, plume=14.0, off_fit=1.0)
        hit = self.look(hit, max_down=12.0)
        hit = self.bash(hit, (0.08, 0.36, 1.27), (-0.44, 0.04, 1.40))
        follow = self.pose(hit, pelvis=R0.pelvis + V(0, 0.12, -0.10), lean=19.0, twist=-18.0, hip_yaw=-8.0, flow=0.5)
        follow = self.look(follow, max_down=12.0)
        follow = self.bash(follow, (0.10, 0.30, 1.22), (-0.46, 0.04, 1.34))
        # the blade swings down round the outside of the right shoulder (hand wide, blade out to the right)
        swing = self.pose(follow, hand_r=V(-0.58, 0.10, 1.10), elbow_r=V(-1.0, 0.2, -0.4), **self.blade_kw(100, -0.92),
                          flow=0.4)
        # recovery: the blade comes down round the outside of the right arm (out to the side, then into the guard)
        rec = self.pose(R0, pelvis=R0.pelvis + V(0, 0.05, -0.08), lean=12.0, flow=0.3, hand_r=V(-0.42, 0.12, 0.98),
                        elbow_r=V(-1.0, 0.0, -0.3), **self.blade_kw(140, -0.85))
        rec = self.look(self.shield_guard(rec, (0.0, 0.02, 0.0)))
        keys = [K(0.0, R0), K(95.0 / 727, brace, "out"), K(175.0 / 727, A), K(255.0 / 727, B),
                K(334.0 / 727, hit, "linear"), K(470.0 / 727, follow, "out"), K(535.0 / 727, swing),
                K(600.0 / 727, rec), K(1.0, R0)]
        return self.clip("Cast_Charge", 727.0, keys, 727.0, notifies={"Release": 334.0})

    CHARGE_FACE = V(-0.08, 1.0, 0.05).normalized()     # shield face at the bash: along the strike line

    def bash(self, bp: BP, shield_pt, sword_pt) -> BP:
        """Cast_Charge bash keys: the shield hand at ``shield_pt`` (bind chest frame, carried by the posed chest)
        with the face held along the strike line in body space, the sword hand at ``sword_pt`` (chest frame)."""
        bp = bp.but(hand_l=self.chest(bp, *shield_pt), off=self.CHARGE_FACE, off_up=V(0, 0, 1))
        return bp.but(hand_r=self.chest(bp, *sword_pt))

    # ── Portrait (static, spec §3.4 / §9.5): sword up before the right shoulder, helm toward the viewer ───
    def portrait(self):
        R0 = self.ready()
        # sword raised beside the helm, blade up and out over the right pauldron so nothing crosses the ember visor;
        # rolled 45° and the fist a little lower (art review 6, portrait at 2×): the crossguard reads as a bar above
        # the fist and the pommel cabochon shows under it, instead of a gold hook drooping over the gauntlet
        P = self.pose(R0, hand_r=V(-0.32, 0.18, 1.04), elbow_r=V(-0.7, -0.3, -0.8), **self.blade_kw(-15, -0.45, 45.0),
                      head_yaw=8.0, head_pitch=3.0, twist=-6.0, hand_l=V(0.08, 0.26, 1.0), clav_r=(6.0, 6.0),
                      flow=0.15)
        keys = [K(0.0, P), K(1.0, P)]
        return self.clip("Portrait", 1000.0 / 60.0, keys, 1000.0 / 60.0, layer=lambda bp, t: bp)

    def all_clips(self):
        return [self.idle(), self.walk(), self.run(), self.attack01(), self.attack02(), self.attack03(),
                self.cast01(), self.cast02(), self.whirlwind(), self.charge(), self.hurt(), self.hurt_add(),
                self.dodge(), self.death(), self.portrait()]

"""NPC poses and clips (spec art-inventory-ch1.md §5.3; web ``src/graphics/sprites/rig/NpcKit.ts`` readyFor :86-113,
workTrack :119-178, talk / alert / idle :184-232).

Shared set, baked per NPC on its own ``SKEL_Human`` proportions (the web's per-NPC READY pose and props make every
loop personal):

* ``IdleNPC`` 1500 loop — breath bob ±0.5 u, spine settle, slow head turn ±0.08 rad, hands bob (web idle);
* ``IdleNPC_Fidget`` ~2600 one-shot (NEW) — a personality gesture that starts and ends on the IdleNPC pose: the smith
  rolls his shoulders, the merchant strokes his mustache, the elder looks to the sky and nods, the herbalist tucks her hair, the
  swordsman tips his hat, the escort lifts his, the lost traveller glances over both shoulders, the peddler hitches
  his pack, the stash keeper pushes up his spectacles;
* ``Alert`` 667 one-shot (motion 500) — from the work pose: head up −7°, stretch, then the free hand raised to head
  height in greeting (ease-out);
* ``Talk`` 1200 loop — the gesture hand from the chest out and up (+3 u fwd, −4 u up), lean +2°, head −3° … lower,
  head +3°;
* ``Work_<Style>`` loop — Hammer 1333 (``FX_HammerStrike`` 427 ms: the hammer face lands on the hot bar held on the
  anvil by the tongs), Count 1600 / 2000 (coin toss: ``FX_CoinToss`` / ``FX_CoinCatch``), Read 2667, Guard 2000
  (leaning on the planted sword), Lookout 2000 (hand shading the eyes);
* escort only: ``Idle`` (the UE escort actor's idle), ``Walk`` 1000 @ 1.2 m/s, ``Run`` 700 @ 2.85 m/s (no-slide
  gait, FootL/R), ``Hurt`` 300, ``Death`` 600; rescue only: ``Cower`` 1000 loop (NEW).

Cloth: robe / coat skirts (four ``skirt_*`` chains, the mage's leg envelope), ponytail / long hair (``hair_tail``),
long beards (``beard``), the scarf tail (``scarf_a``) and the travel pack (``pack_01``) hang with secondary lag and
stay off the body and the floor.
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
import props as P

WALK_SPEED = 1.2
ESCORT_RUN_SPEED = 2.85            # spec §5.3: escort speed 2.842 tiles/s at hero speed 120


def envelope_soft(angles, lengths, dn: np.ndarray, bk: np.ndarray, clear, max_deg: float = 80.0,
                  slack: float = 0.004) -> list[float]:
    """``hero_base.envelope`` with look-ahead: a link also answers for the points within half the next link past
    its end (as if the chain continued straight), so a knee just below the first skirt link turns the first link a
    little instead of throwing the second one out horizontally (the greedy envelope's 60–70° kink)."""
    if len(dn) == 0:
        return list(angles)
    clear = np.broadcast_to(np.asarray(clear, float), dn.shape)
    s_dn = s_bk = 0.0
    out = []
    n = len(lengths)
    for i, L in enumerate(lengths):
        a = angles[i]
        reach = 0.5 * lengths[i + 1] if i + 1 < n else 0.06
        sel = (dn > s_dn + slack) & (dn <= s_dn + L + reach)
        if sel.any():
            need = float(np.degrees(np.arctan2(bk[sel] + clear[sel] - s_bk, dn[sel] - s_dn)).max())
            a = max(a, min(need, max_deg))
        out.append(a)
        s_dn += L * math.cos(math.radians(a))
        s_bk += L * math.sin(math.radians(a))
    return out


class NpcAnims(HB.HeroAnims):
    LEG_PARTS = ("leg", "boot", "boot_shaft", "boot_cuff")
    SKIRT_CLEAR = 0.015
    HEM_FADE = (0.96, 1.04)
    SKIRT_MAX = 80.0
    SKIRT_FLOOR = 0.05

    def __init__(self, rig, folk, npc, items: dict | None = None):
        self.F = folk
        self.npc = npc
        self.look = folk.look
        chains = []
        if folk.skirted:
            chains += ["skirt_f", "skirt_b", "skirt_l", "skirt_r"]
        names = {b.name for b in rig.obj.data.bones}
        for c in ("hair_tail", "beard", "scarf_a", "pack"):
            if f"{c}_01" in names:
                chains.append(c)
        self.CHAINS = tuple(chains)
        lk = self.look
        self.GRIPS = tuple(s for s, it in (("r", lk.item), ("l", lk.off)) if it in ("hammer", "tongs", "staff", "sword"))
        self.FEET = {s: (f"boot_{s}", f"boot_shaft_{s}") for s in ("l", "r")}
        self.ITEMS = tuple((P.PROPS["herbs" if (it == "pouch" and npc.id == "PlainsHerbalist") else it], bone)
                           for it, bone in ((lk.item, "weapon_r"), (lk.off, "weapon_l")) if it != "none")
        self.CLOTH_PARTS = ("skirt", "hem", "fold", "apron_panel", "pocket", "hair", "beard", "braid", "scarf_tail",
                            "mustache")
        super().__init__(rig, folk.D)
        self.U = folk.U
        self.sg_gest = 1.0 if npc.gesture_hand == "l" else -1.0
        self.items = items or {}
        if folk.skirted:
            self._init_skirt()
        self._ready = None

    # ── geometry helpers ────────────────────────────────────────────────────────────────────────────────
    def shoulder(self, s: str) -> Vector:
        return self.J[f"shoulder_{s}"].copy()

    def arm_len(self) -> float:
        return self.rig.spec.upper_arm + self.rig.spec.fore_arm

    def hang_hand(self, s: str, out: float = 0.06, fwd: float = 0.05, k: float = 0.92) -> Vector:
        sg = 1.0 if s == "l" else -1.0
        S = self.shoulder(s)
        bel = self.F.belly * self.U * 0.55
        return S + V(sg * (out + bel), fwd + bel * 0.4, -k * self.arm_len())

    def head_pt(self, bp: BP, lat: float, fwd: float, up: float) -> Vector:
        """Posed point in the head frame (rest-space offset from the head centre)."""
        p = C.to_pose(self.rig, bp.but(hand_l=None, hand_r=None), None, grips=())
        m = anim.evaluate(p, return_matrices=True)["head"] @ self.bones["head"].matrix_local.inverted()
        return m @ (self.J["head_center"] + V(lat, fwd, up))

    def item_pt(self, bp: BP, side: str, local) -> Vector:
        p = C.to_pose(self.rig, bp, None, grips=self.GRIPS)
        return anim.evaluate(p, return_matrices=True)[f"weapon_{side}"] @ Vector(local)

    def fit_item(self, bp: BP, side: str, local, target: Vector, iters: int = 4) -> BP:
        """Move the hand target so the item's local point lands on ``target`` (hammer face on the bar, tongs on the
        anvil, staff foot / sword tip on the ground)."""
        key = f"hand_{side}"
        for _ in range(iters):
            d = target - self.item_pt(bp, side, local)
            if d.length < 5e-4:
                break
            bp = bp.but(**{key: getattr(bp, key) + d})
        return bp

    # ── READY (web readyFor :86-113 + the NPC's ready tweaks) ───────────────────────────────────────────
    def ready(self) -> BP:
        if self._ready is not None:
            return self._ready.copy()
        J = self.J
        g = self.F.g
        sp = 0.02 * (g - 1.0) / 0.2
        bp = BP(
            pelvis=V(0, 0.005, -0.012), hip_pitch=0.6, hip_yaw=-3.0, hip_roll=0.0, lean=2.3, twist=3.0,
            head_pitch=0.0, head_yaw=-3.0,
            foot_l=J["ankle_l"] + V(0.012 + sp, -0.035, 0), foot_r=J["ankle_r"] + V(-0.010 - sp, 0.055, 0),
            foot_l_rot=(0, 0, 15), foot_r_rot=(0, 0, 9), knee_l=V(0.2, 1, 0), knee_r=V(-0.15, 1, 0),
            hand_l=self.hang_hand("l"), hand_r=self.hang_hand("r"),
            elbow_l=V(0.6, -0.8, -0.2), elbow_r=V(-0.6, -0.8, -0.2), wrist_l=(0, 0, 0), wrist_r=(0, 0, 0),
            flow=0.08,
        )
        if self.F.skirted:
            # robes / long coats: the feet together under the cloth, knees straight (the 4-chain skirt bulges as a
            # whole panel; a stepped-forward knee would push the entire front out)
            bp = bp.but(pelvis=V(0, 0.003, -0.006), foot_l=J["ankle_l"] + V(0.006, -0.012, 0),
                        foot_r=J["ankle_r"] + V(-0.006, 0.016, 0), foot_l_rot=(0, 0, 12), foot_r_rot=(0, 0, 8))
        bp = getattr(self, f"_ready_{self.npc.work.lower()}")(bp)
        self._ready = bp
        return bp.copy()

    def _down_item(self, side: str, theta: float, lat: float) -> dict:
        key = "wpn" if side == "r" else "off"
        d, e = C.blade(theta, lat=lat)
        return {key: d, f"{key}_up": e}

    def _hanging(self, side: str, fwd_face: float = 1.0) -> dict:
        """A bag hanging plumb from the fist (web HANGING props): +Y down, its stamped face (+Z) forward."""
        key = "wpn" if side == "r" else "off"
        return {key: V(0, 0, -1), f"{key}_up": V(0, fwd_face, 0)}

    def _ready_hammer(self, bp: BP) -> BP:
        # web: hammer hanging in the near hand (wpn 2.7 rad ≈ 155°), tongs in the far hand (off 2.4 ≈ 138°); 3D: both
        # carried a little out from the legs
        return bp.but(hand_r=self.hang_hand("r", out=0.10, fwd=0.06, k=0.9), hand_l=self.hang_hand("l", 0.10, 0.08, 0.88),
                      **self._down_item("r", 160.0, -0.22), **self._down_item("l", 145.0, 0.25),
                      elbow_l=V(0.7, -0.6, -0.2), elbow_r=V(-0.7, -0.6, -0.2))

    def _ready_count(self, bp: BP) -> BP:
        # web count: the purse in the off hand before the belly (x + 7), the near hand palm-up before the chest
        F = self.F
        bel = F.belly * F.U
        zb = F.th(F.L * 0.42)
        front = -(F.torso.at(zb)[1] - F.torso.at(zb)[2])        # belly front y (negative = forward)
        hand_on = "l" if self.look.off != "none" else "r"
        out = {}
        if self.npc.id == "PlainsHerbalist":
            # her herb pouch hangs from the near (right) hand; the far hand rests at the belly
            out.update(hand_r=V(-0.13, -front + 0.15, zb + 0.06), **self._hanging("r"),
                       hand_l=V(0.10, -front + 0.10, zb - 0.02), wrist_l=(-20.0, 0.0, -40.0),
                       elbow_r=V(-0.8, -0.4, -0.3), elbow_l=V(0.8, -0.4, -0.3))
        else:
            out.update(hand_l=V(0.13, -front + 0.10 + bel * 0.2, zb + 0.02), **self._hanging("l"),
                       hand_r=V(-0.12, -front + 0.14, zb + 0.10), wrist_r=(-25.0, 0.0, 35.0),
                       elbow_l=V(0.8, -0.4, -0.3), elbow_r=V(-0.8, -0.4, -0.3))
        del hand_on
        return bp.but(**out)

    def _ready_read(self, bp: BP) -> BP:
        F = self.F
        zc = F.th(F.L - 4.5)
        front = -(F.torso.at(zc)[1] - F.torso.at(zc)[2])
        if self.look.item == "staff":
            # web elder READY: lean 0.08, head 0.04, staff hand at chest x + 8 held out to the side, staff upright;
            # the scroll in the far hand before the chest; 3D: the staff foot planted on the ground
            hand_r = V(-0.33, -front + 0.05, zc - 0.12)
            sd, se = C.blade(-3.0, lat=-0.07)
            bp = bp.but(lean=4.6, head_pitch=2.0, hand_r=hand_r, wpn=sd, wpn_up=se, elbow_r=V(-0.9, -0.3, -0.3),
                        hand_l=V(0.06, -front + 0.19, zc - 0.22), elbow_l=V(0.8, -0.5, -0.4),
                        off=V(-0.12, 0.42, 0.90), off_up=V(0.05, -0.90, 0.42))
            foot = Vector((0, -P.STAFF_DOWN, 0))
            p = self.item_pt(bp, "r", foot)
            bp = self.fit_item(bp, "r", foot, Vector((p.x, p.y, 0.004)))
        else:
            # the stash keeper: the open book in the far hand before the chest, the near hand at its pages
            bp = bp.but(lean=4.0, head_pitch=6.0, hand_l=V(0.06, -front + 0.20, zc - 0.03),
                        elbow_l=V(0.8, -0.5, -0.4), off=V(-0.1, 0.05, 1.0), off_up=V(0.15, -0.75, 0.55),
                        hand_r=V(-0.10, -front + 0.17, zc - 0.10), elbow_r=V(-0.8, -0.5, -0.3),
                        wrist_r=(-20.0, 0.0, 30.0))
        return bp

    SWORD_TIP = (0.0, P.SWORD_TIP_Y, 0.0)

    def _ready_guard(self, bp: BP) -> BP:
        # leaning on the sword (spec "leans on it"): the point planted before the near foot, the near hand on the
        # pommel, the far thumb hooked in the belt; weight on the back leg
        J = self.J
        tip = V(-0.25, 0.33, 0.004)
        hand = V(-0.19, 0.22, 0.0)
        d = (tip - hand)
        d.z = -P.SWORD_TIP_Y
        d.normalize()
        up = V(0, 1, 0) - d * V(0, 1, 0).dot(d)
        bp = bp.but(lean=5.0, hip_roll=-3.0, side=2.0, hip_yaw=-6.0, twist=4.0, pelvis=V(0.01, 0.0, -0.016),
                    foot_r=J["ankle_r"] + V(-0.012, 0.035, 0), foot_l=J["ankle_l"] + V(0.012, -0.03, 0),
                    hand_r=tip - d * P.SWORD_TIP_Y, wpn=d, wpn_up=up.normalized(), elbow_r=V(-0.8, -0.5, -0.1),
                    hand_l=self.J["hip_l"] + V(0.12, 0.05, 0.05), elbow_l=V(1.0, -0.2, 0.0), wrist_l=(10.0, 0.0, -60.0))
        return self.fit_item(bp, "r", self.SWORD_TIP, tip)

    def _ready_lookout(self, bp: BP) -> BP:
        # hands at rest: the far hand on the pack strap at the chest
        F = self.F
        zc = F.th(F.L - 3.0)
        return bp.but(hand_l=F.torso.point(zc, 34.0, 0.03) + V(0.0, 0.03, -0.03), elbow_l=V(0.8, -0.4, -0.5),
                      wrist_l=(-30.0, 0.0, -50.0))

    # ── cloth ───────────────────────────────────────────────────────────────────────────────────────────
    def _init_skirt(self) -> None:
        F = self.F
        self.skirt = {}
        for c, ac in (("f", 0.0), ("l", 90.0), ("b", 180.0), ("r", -90.0)):
            info = self.chains[f"skirt_{c}"]
            pts = [self.rig.head(info.names[0])] + [self.rig.tail(n) for n in info.names]
            dirv = Vector((math.sin(math.radians(ac)), -math.cos(math.radians(ac)), 0.0))
            rad = [p.dot(dirv) for p in pts]
            outs = [math.degrees(math.atan2(rad[i + 1] - rad[i], pts[i].z - pts[i + 1].z)) for i in range(2)]
            self.skirt[c] = dict(ang=ac, dir=dirv, root=pts[0], root_r=rad[0], rest_out=outs,
                                 lengths=self.chain_lengths(info))
        self.z0, self.z1 = F.skirt_z0, F.skirt_z1
        self._ks = np.linspace(0.0, 1.0, 21)
        self._as = np.linspace(-180.0, 180.0, 73)
        self._rt = np.array([[math.hypot(*tuple(F.skirt_point(k, a))[:2]) for a in self._as] for k in self._ks])

    def make_env(self, body, names):
        legs = C.SkinPoints(self.rig).add_mesh(body, names, lambda n: n.rstrip("_lr") in self.LEG_PARTS, step=1)
        self.legs = legs
        torso = ("torso", "mantle", "scarf_wrap", "scarf_knot", "apron_bib", "belt", "pack", "bedroll", "pack_pocket",
                 "pack_pot", "pack_rug", "hair_back", "skull", "neck")
        self.torso_env = C.SkinPoints(self.rig).add_mesh(body, names, lambda n: n in torso, step=2)
        return legs

    def _gvec(self, bp: BP) -> Vector:
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

    def _acc(self, mats, parent: str, hinge: Vector, down=Vector((0, 0, -1))) -> float:
        M3 = mats[parent].to_3x3() @ self.bones[parent].matrix_local.to_3x3().inverted()
        Dn = (down - hinge * down.dot(hinge)).normalized()
        Bk = hinge.cross(Dn)
        v = M3 @ Vector((0, 0, -1))
        return math.degrees(math.atan2(v.dot(Bk), v.dot(Dn)))

    def skirt_angles(self, p: Pose, bp: BP, mats) -> dict:
        """The mage's skirt solver (mage_clips.MageAnims.skirt_angles): flow streaming, gravity in falls, and the
        skinned legs pushing the four chains out (minimum-norm share of each leg point's deficit + envelope)."""
        Pm = mats["pelvis"] @ self.bones["pelvis"].matrix_local.inverted()
        inv = Pm.inverted()
        X = (Pm.to_3x3() @ Vector((1, 0, 0))).normalized()
        acc = self._acc(mats, "pelvis", X)
        ga, gs = self.gravity(bp)
        names = list(self.skirt)
        req = {c: None for c in names}
        if self.skin() is not None:
            pts = self.legs.posed(mats)
            q = (np.c_[pts, np.ones(len(pts))] @ np.array(inv).T)[:, :3]
            th = np.degrees(np.arctan2(q[:, 0], -q[:, 1]))
            rho = np.hypot(q[:, 0], q[:, 1])
            k = (self.z0 - q[:, 2]) / (self.z0 - self.z1)
            fade = np.asarray(HB.smooth01((self.HEM_FADE[1] - k) / (self.HEM_FADE[1] - self.HEM_FADE[0])))
            sel = (k > 0.20) & (fade > 0.02)          # the waist band is pinned: only knees / shins / boots push
            if sel.any():
                q, th, rho, k, fade = q[sel], th[sel], rho[sel], np.clip(k[sel], 0.0, 1.0), fade[sel]
                clear = self.SKIRT_CLEAR + 0.012 * np.exp(-((k - 0.5) / 0.12) ** 2)
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
        out = {}
        for c, sk in self.skirt.items():
            out_ang = list(sk["rest_out"])
            for i in range(2):
                pb = self.lagged(bp, 50.0 * (i + 1))
                fl = pb.flow - 0.12
                stream = math.degrees(fl * 0.62 * (0.5 + 0.5 * (i + 1) / 2))
                wave = math.degrees(math.sin(bp.wave * 2 - 1.3 * (i + 1) + sk["ang"] * 0.02) * 0.02 * (i + 1)
                                    * (0.4 + bp.flow))
                if c == "f":
                    out_ang[i] -= stream * 0.8 - wave
                elif c == "b":
                    out_ang[i] += stream + wave
                else:
                    out_ang[i] += wave * 0.5
            if gs > 0:
                gv = self._gvec(bp)
                for i in range(2):
                    tgt = math.degrees(math.atan2(gv.dot(sk["dir"]), -gv.z))
                    out_ang[i] += self.arc(tgt - out_ang[i]) * gs * (0.55, 0.85)[i]
            if req[c] is not None:
                dn, bk = req[c]
                out_ang = envelope_soft(out_ang, sk["lengths"], dn, bk, 0.0, max_deg=self.SKIRT_MAX, slack=0.0)
            out[c] = out_ang
        return {"acc": acc, "out": out, "ga": ga, "gs": gs}

    def set_skirt(self, p: Pose, bp: BP, sk_res: dict, wf) -> None:
        acc, ga, gs = sk_res["acc"], sk_res["ga"], sk_res["gs"]
        for c, sk in self.skirt.items():
            info = self.chains[f"skirt_{c}"]
            out_ang = sk_res["out"][c]
            if c in "fb":
                sgn = -1.0 if c == "f" else 1.0
                absang = [sgn * a + acc for a in out_ang]
                absang = self.above_ground(info, absang, wf, "pelvis", self.SKIRT_FLOOR)
                C.set_chain(p, info, absang, acc)
            else:
                fl = self.lagged(bp, 60.0).flow - 0.12
                absang = [info.rest[i] + math.degrees(fl * 0.5 * (0.6 + 0.4 * (i + 1) / 2)) for i in range(2)]
                if gs > 0:
                    for i in range(2):
                        d = self.arc(ga - absang[i])
                        absang[i] += d * gs * (0.5, 0.75)[i]
                extra = [out_ang[i] - sk["rest_out"][i] for i in range(2)]
                sg = -1.0 if c == "l" else 1.0
                rel = [sg * extra[0], sg * (extra[1] - extra[0])]
                absang = self.above_ground(info, absang, wf, "pelvis", self.SKIRT_FLOOR)
                C.set_chain(p, info, absang, acc, rel)

    def _front_chain(self, p: Pose, bp: BP, wf, name: str, parent: str, follow: float, clear: float,
                     sway: float = 0.025) -> None:
        """A chain hanging in front of the chest (long beard, scarf tail): rides ``follow`` of its parent's tilt,
        sags toward world down, streams with the flow, kept ``clear`` in front of the torso (envelope)."""
        info = self.chains[name]
        mats = anim.evaluate(p, return_matrices=True)
        Hm = mats[parent] @ self.bones[parent].matrix_local.inverted()
        X = (Hm.to_3x3() @ Vector((1, 0, 0))).normalized()
        acc = self._acc(mats, parent, X)
        pb = self.lagged(bp, 60.0)
        fl = pb.flow - 0.12
        want = []
        n = len(info.names)
        for i in range(n):
            a = follow * (info.rest[i] + acc) + (1 - follow) * info.rest[i] * 0.4
            a += math.degrees(fl * 0.35 * (i + 1) / n)
            a += math.degrees(math.sin(bp.wave + 0.8 - 1.4 * i) * sway * (0.4 + pb.flow))
            want.append(a)
        ga, gs = self.gravity(bp)
        if gs > 0:
            for i in range(n):
                d = self.arc(ga - want[i])
                want[i] += d * gs * (0.3 + 0.25 * i / max(1, n - 1))
        if self.skin() is not None and gs < 0.5:
            root, Dn, B, _, _ = self.plane(mats, parent, info.names[0])
            pts = self.torso_env.posed(mats)
            rel = pts - np.array(root)
            dn, fw = rel @ np.array(Dn), -(rel @ np.array(B))
            fwd = HB.envelope([-a for a in want], self.chain_lengths(info), dn, fw, clear, max_deg=80.0)
            want = [-a for a in fwd]
        want = self.above_ground(info, want, wf, parent, 0.015)
        C.set_chain(p, info, want, acc)

    def _back_chain(self, p: Pose, bp: BP, wf, name: str, parent: str, clear: float, follow: float = 0.45) -> None:
        """Ponytail / long hair: hangs from the back of the head, streams with the flow, kept off the back (and the
        pack) by the torso envelope."""
        info = self.chains[name]
        mats = anim.evaluate(p, return_matrices=True)
        Hm = mats[parent] @ self.bones[parent].matrix_local.inverted()
        X = (Hm.to_3x3() @ Vector((1, 0, 0))).normalized()
        acc = self._acc(mats, parent, X)
        free = self.hang(bp, info, gain=0.9, lag_ms=80.0, wave_amp=0.05, phase=0.4)
        want = [follow * (info.rest[i] + acc) + (1 - follow) * free[i] for i in range(len(info.names))]
        if self.skin() is not None:
            root, Dn, B, _, _ = self.plane(mats, parent, info.names[0])
            pts = self.torso_env.posed(mats)
            rel = pts - np.array(root)
            lat = rel @ np.array((Hm.to_3x3() @ Vector((1, 0, 0))).normalized())
            sel = np.abs(lat) < 0.06
            if sel.any():
                want = HB.envelope(want, self.chain_lengths(info), rel[sel] @ np.array(Dn), rel[sel] @ np.array(B),
                                   clear)
        want = self.above_ground(info, want, wf, parent, 0.02)
        C.set_chain(p, info, want, acc)

    def cloth(self, p: Pose, bp: BP) -> None:
        wf = self.world_fn(p, bp)
        if self.F.skirted:
            mats = anim.evaluate(p, return_matrices=True)
            self.set_skirt(p, bp, self.skirt_angles(p, bp, mats), wf)
        if "hair_tail" in self.chains:
            self._back_chain(p, bp, wf, "hair_tail", "head", 0.03 if self.look.hair[1] != "long" else 0.012,
                             follow=0.35 if self.look.hair[1] != "long" else 0.6)
        if "beard" in self.chains:
            self._front_chain(p, bp, wf, "beard", "head", follow=0.5, clear=0.02)
        if "scarf_a" in self.chains:
            self._front_chain(p, bp, wf, "scarf_a", "spine_03", follow=0.85, clear=0.012, sway=0.04)
        if "pack" in self.chains:
            pb = self.lagged(bp, 70.0)
            bounce = math.degrees((pb.pelvis.z - bp.pelvis.z) * 2.2)
            p.rot("pack_01", pitch=-max(-6.0, min(6.0, bounce)))

    def grounded(self, snap: bool = False, pivot_pelvis: bool = True):
        """``HeroAnims.grounded`` (whole body on the floor, measured on every 2nd skinned vertex) + the boots at
        full resolution: a tucked toe cap's lowest vertex can fall between the sampled ones (−1.6 cm in the
        escort's fall)."""
        base = super().grounded(snap=snap, pivot_pelvis=pivot_pelvis)

        def post(bp: BP, t: float) -> BP:
            bp = base(bp, t)
            if self.skin() is None:
                return bp
            mats = anim.evaluate(C.to_pose(self.rig, bp, None, grips=self.GRIPS), return_matrices=True)
            low = min(float(sp.posed(mats, m)[:, 2].min()) for sp, m in self.feet.values())
            return bp.but(lift=bp.lift - low) if low < 0.0 else bp
        return post

    # ── helpers for keys ────────────────────────────────────────────────────────────────────────────────
    def free_hand(self) -> str:
        return "l" if self.sg_gest > 0 else "r"

    def gesture_points(self, R0: BP) -> tuple[Vector, Vector, Vector]:
        """Talk gesture targets (rest armature space, carried by the chest): chest → out & up → lower."""
        s = self.free_hand()
        sg = 1.0 if s == "l" else -1.0
        S = self.shoulder(s)
        F = self.F
        zc = F.th(F.L - 4.0)
        front = -(F.torso.at(zc)[1] - F.torso.at(zc)[2])
        it = self.look.item if s == "r" else self.look.off
        lo = 0.14 if it in ("hammer", "tongs", "sword", "pouch") else (0.08 if it == "scroll" else 0.0)
        fo = {"pouch": 0.13, "scroll": 0.08}.get(it, 0.0)     # hanging bags / scrolls held clear of belly / beard
        fy = {"pouch": 0.05, "scroll": 0.08}.get(it, 0.0)
        # ``front`` is the chest's front-surface y (negative = forward): the targets sit that far + d before it
        # W1: pitch −50° drops a forward hand down the screen by ~0.77 × its reach, so the "out & up" beat rises to
        # the chin (the web's +4 u read at a side view)
        a = Vector((sg * (0.10 + lo), front - (0.12 + fy), zc + 0.02 - fo))
        b = Vector((sg * (abs(S.x) + 0.10 + lo * 0.5), front - (0.17 + fy), zc + 0.24 - fo))
        c = Vector((sg * (abs(S.x) + 0.04 + lo * 0.5), front - (0.15 + fy), zc - 0.04 - fo))
        return a, b, c

    def gesture_item(self, side: str, t: str) -> dict:
        """Keep a held prop sensible while its hand gestures: bags hang, scrolls / books stay readable, tools point
        down-out."""
        it = self.look.item if side == "r" else self.look.off
        key = "wpn" if side == "r" else "off"
        sg = 1.0 if side == "l" else -1.0
        if it == "pouch":
            return self._hanging(side)
        if it in ("hammer", "tongs"):
            return self._down_item(side, 100.0 if t == "up" else 150.0, sg * 0.9)
        if it == "scroll":
            # presented to the listener: rolled end forward and out, text up
            return {key: V(sg * 0.45, 0.85, 0.25), f"{key}_up": V(0, -0.3, 0.95)}
        if it == "book":
            return {key: V(-0.1, 0.05, 1.0), f"{key}_up": V(0.15, -0.75, 0.55)}
        return {}

    # ── IdleNPC (1500 loop, web idle :215-230) ──────────────────────────────────────────────────────────
    def idle_sampler(self, length: float = 1500.0):
        R0 = self.ready()
        U = self.U

        def s(t_ms: float) -> BP:
            t = (t_ms / length) % 1.0
            ph = 2 * math.pi * t
            b = math.sin(ph)
            kw = dict(pelvis=R0.pelvis + V(0, 0, b * 0.5 * U), lean=R0.lean - b * 0.9,
                      head_yaw=R0.head_yaw + math.degrees(0.08) * math.sin(ph),
                      head_pitch=R0.head_pitch + math.sin(ph - 0.6) * 1.2,
                      flow=R0.flow + b * 0.04, wave=ph)
            for side in ("l", "r"):
                hk = f"hand_{side}"
                h = getattr(R0, hk)
                if h is None:
                    continue
                pinned = (self.npc.work == "Guard" and side == "r") or (self.look.item == "staff" and side == "r")
                if not pinned:
                    kw[hk] = h + V(0, 0, (math.sin(ph - (0.5 if side == "l" else 0.0)) * 0.45) * U)
            return R0.but(**kw)
        return s

    def idle(self, name: str = "IdleNPC"):
        c = self.proc_clip(name, 1500.0, self.idle_sampler(), loop=True)
        c.key_poses_ms = [0.0, 375.0, 750.0, 1125.0]
        return c

    # ── Alert (667 one-shot, web :204-208) ──────────────────────────────────────────────────────────────
    def alert(self):
        R0 = self.ready()
        W0 = self.work_keys()[0].bp
        s = self.free_hand()
        sg = 1.0 if s == "l" else -1.0
        hand = self.J["head_center"] + V(sg * 0.26, 0.12, -0.02)
        o = "r" if s == "l" else "l"
        so = 1.0 if o == "l" else -1.0
        hold = getattr(R0, f"hand_{o}")
        out_kw = {f"hand_{o}": hold + V(so * 0.05, 0.03, 0.0)} if hold is not None and not (
            self.npc.work == "Guard" or (self.look.item == "staff" and o == "r")) else {}
        g0 = getattr(R0, f"hand_{s}")
        out_kw[f"hand_{s}"] = g0 + V(sg * 0.16, 0.06, 0.02)
        up = R0.but(head_pitch=R0.head_pitch - 7.0, lean=R0.lean - 1.5, pelvis=R0.pelvis + V(0, 0, 0.008), **out_kw,
                    **self.gesture_item(s, "mid"))
        wave = up.but(head_pitch=R0.head_pitch - 4.0, head_yaw=R0.head_yaw + sg * 4.0, twist=R0.twist - sg * 4.0,
                      **{f"hand_{s}": self.chest(R0, hand.x, -hand.y, hand.z), f"elbow_{s}": V(sg * 1.0, 0.3, -0.6),
                         f"wrist_{s}": (-10.0, 0.0, sg * 10.0)}, **self.gesture_item(s, "up"))
        keys = [K(0.0, W0), K(0.4, up), K(1.0, wave, "out")]
        c = self.clip("Alert", 667.0, keys, 500.0)
        c.key_poses_ms = [0.0, 200.0, 350.0, 500.0, 667.0]
        return c

    # ── Talk (1200 loop, web :192-203) ──────────────────────────────────────────────────────────────────
    def talk(self):
        R0 = self.ready()
        s = self.free_hand()
        sg = 1.0 if s == "l" else -1.0
        a, b, c3 = self.gesture_points(R0)
        hk, ek, wk = f"hand_{s}", f"elbow_{s}", f"wrist_{s}"
        e = V(sg * 0.9, -0.2, -0.5)
        k0 = R0.but(**{hk: self.chest(R0, a.x, -a.y, a.z), ek: e, wk: (-35.0, 0.0, sg * 50.0)},
                    head_pitch=R0.head_pitch + 1.0, **self.gesture_item(s, "mid"))
        k1 = R0.but(lean=R0.lean + 2.0, head_pitch=R0.head_pitch - 3.0, head_yaw=R0.head_yaw + sg * 3.0,
                    twist=R0.twist + sg * 3.0,
                    **{hk: self.chest(R0, b.x, -b.y, b.z), ek: e, wk: (-55.0, 0.0, sg * 65.0)},
                    **self.gesture_item(s, "up"))
        k2 = R0.but(head_pitch=R0.head_pitch + 3.0, head_yaw=R0.head_yaw - sg * 2.0,
                    **{hk: self.chest(R0, c3.x, -c3.y, c3.z), ek: e, wk: (-40.0, 0.0, sg * 45.0)},
                    **self.gesture_item(s, "mid"))
        keys = [K(0.0, k0), K(0.33, k1), K(0.66, k2), K(1.0, k0)]
        c = self.clip("Talk", 1200.0, keys, 1200.0, loop=True)
        c.key_poses_ms = [0.0, 300.0, 400.0, 600.0, 800.0, 1000.0]
        return c

    # ── work loops ──────────────────────────────────────────────────────────────────────────────────────
    def work_keys(self) -> list:
        return getattr(self, f"_work_{self.npc.work.lower()}")()

    def work(self):
        w = self.npc.work
        keys = self.work_keys()
        L = self.npc.work_ms
        notifies = {}
        if w == "Hammer":
            notifies = {"FX_HammerStrike": round(L * 0.32, 1)}
        elif w == "Count" and self.npc.id != "PlainsHerbalist":
            notifies = {"FX_CoinToss": 0.0, "FX_CoinCatch": round(L * 0.6, 1)}
        c = self.clip(f"Work_{w}", L, keys, L, loop=True, notifies=notifies)
        c.key_poses_ms = sorted({round(k.t * L, 1) for k in keys if k.t < 1.0} | {round(L * 0.16, 1),
                                                                                    round(L * 0.8, 1)})
        return c

    # Hammer (1333, web :119-127): t0 raised behind the head (−63°), lean −5°, eyes on the anvil · t .32 STRIKE
    # (ease-in): across the body onto the anvil face (152°), lean +14°, dip 1.2 u · t .45 rebound · t .8 raise
    ANVIL_AT = Vector((0.12, -0.52, 0.0))

    def strike_point(self) -> Vector:
        u = 0.0319
        return self.ANVIL_AT + Vector((0.0, 1.0 * u, 17.0 * u + 0.004))

    def bar_target(self) -> Vector:
        return self.strike_point() + Vector((0.0, 0.0, 0.55 * 0.0319 * 0.985 + 0.002))

    def _work_hammer(self) -> list:
        """3D smithing: the anvil before the belly (a touch left), the hot bar held on its face by the tongs from the
        left; the hammer raised in front of the right shoulder (head up-back over it, eyes on the work) and brought
        down in front of the body onto the bar — the swing never passes the head or the chest."""
        R0 = self.ready()
        U = self.U
        hit = self.bar_target()
        tong_local = Vector((0.0, 14.3 * P.U, 0.55 * P.U))
        hl = Vector((0.15, -0.36, 0.80))
        dl = (hit - hl).normalized()
        el = (V(0, 0, 1) - dl * V(0, 0, 1).dot(dl)).normalized()

        def with_tongs(bp: BP) -> BP:
            bp = bp.but(hand_l=hl, off=dl, off_up=el, elbow_l=V(0.9, -0.3, -0.3))
            return self.fit_item(bp, "l", tong_local, hit)
        face_local = Vector((0.0, 12.1 * P.U, 4.35 * P.U))
        S_r = self.shoulder("r")
        # raised: the fist up before the right shoulder, the hammer head up and back over it
        rd, ru = C.blade(-40.0, lat=-0.40)
        raised = with_tongs(R0.but(lean=-4.0, head_pitch=14.0, head_yaw=8.0, twist=-8.0, hip_yaw=-4.0,
                                   hand_r=S_r + V(-0.05, 0.20, 0.36), elbow_r=V(-0.9, -0.2, -0.3), wpn=rd, wpn_up=ru,
                                   clav_r=(12.0, 6.0)))
        # mid-swing (t .2): the fist comes forward over the anvil line, the hammer upright above it
        md, mu = C.blade(30.0, lat=-0.30)
        mid = with_tongs(R0.but(lean=6.0, head_pitch=16.0, head_yaw=10.0, twist=2.0,
                                hand_r=S_r + V(0.06, 0.40, 0.12), elbow_r=V(-0.9, -0.1, -0.4), wpn=md, wpn_up=mu,
                                clav_r=(6.0, 8.0)))
        # strike: lean 14°, head down 18°, dip 1.2 u; the hammer face on the hot bar
        sd, se = C.blade(150.0, lat=0.10)
        strike = R0.but(lean=14.0, hip_pitch=3.0, head_pitch=16.0, head_yaw=10.0, twist=6.0, hip_yaw=3.0,
                        pelvis=R0.pelvis + V(0.0, 1.0 * U, -1.2 * U), wpn=sd, wpn_up=se,
                        hand_r=hit + V(-0.17, -0.12, 0.33), elbow_r=V(-0.9, -0.3, -0.2), clav_r=(-4.0, 6.0))
        strike = with_tongs(strike)
        strike = self.fit_item(strike, "r", face_local, hit + Vector((0, 0, 0.004)))
        rd2, ru2 = C.blade(128.0, lat=0.0)
        rebound = strike.but(lean=12.0, head_pitch=14.0, pelvis=R0.pelvis + V(0.0, 0.8 * U, -0.8 * U),
                             hand_r=strike.hand_r + V(0.0, 0.02, 0.07), wpn=rd2, wpn_up=ru2)
        rebound = with_tongs(rebound)
        # the lift draws the fist up the right side (hammer head swinging out and back), never across the face
        lift = with_tongs(mid.but(lean=2.0, head_pitch=13.0, hand_r=S_r + V(-0.12, 0.16, 0.16),
                                  elbow_r=V(-1.0, -0.2, -0.3),
                                  **dict(zip(("wpn", "wpn_up"), C.blade(-20.0, lat=-0.55)))))
        return [K(0.0, raised), K(0.20, mid, "in"), K(0.32, strike, "in"), K(0.45, rebound), K(0.65, lift, "out"),
                K(1.0, raised, "out")]

    # Count (web :135-140): t0 hand at −6 u · t .3 toss up to −12 u (ease-out, coin glint) · t .6 catch (ease-in)
    def _work_count(self) -> list:
        R0 = self.ready()
        U = self.U
        if self.npc.id == "PlainsHerbalist":
            # the herb pouch lifted to the nose, examined, lowered (the glow pulses over the hand)
            hr = R0.hand_r
            up_ = R0.but(hand_r=hr + V(0.04, 0.03, 0.17), head_pitch=R0.head_pitch + 4.0, head_yaw=-8.0,
                         elbow_r=V(-0.9, -0.2, -0.3), **self._hanging("r"))
            k0 = R0.but(head_pitch=R0.head_pitch + 8.0, head_yaw=-6.0)
            k2 = R0.but(hand_r=hr + V(0.02, 0.01, 0.02), head_pitch=R0.head_pitch + 10.0, head_yaw=-10.0)
            return [K(0.0, k0), K(0.3, up_, "out"), K(0.6, k2, "in"), K(1.0, k0)]
        hr = R0.hand_r
        # eyes on the coin (head pitches held ~5° under the web's so the face still reads from the W1 camera)
        k0 = R0.but(head_pitch=R0.head_pitch + 9.0, head_yaw=-6.0)
        toss = R0.but(hand_r=hr + V(0.0, 0.02, 0.19), head_pitch=R0.head_pitch - 3.0, head_yaw=-4.0,
                      wrist_r=(-35.0, 0.0, 30.0), lean=R0.lean - 1.0)
        catch = R0.but(hand_r=hr + V(0.0, 0.0, -0.03), head_pitch=R0.head_pitch + 11.0, head_yaw=-7.0,
                       pelvis=R0.pelvis + V(0, 0, -0.3 * U))
        return [K(0.0, k0), K(0.3, toss, "out"), K(0.6, catch, "in"), K(1.0, k0)]

    # Read (web :141-145): head bowed 17° ↔ 21°, lean +4°, the page hand turning
    def _work_read(self) -> list:
        R0 = self.ready()
        if self.look.item == "staff":
            k0 = R0.but(head_pitch=17.0, lean=R0.lean)
            k1 = R0.but(head_pitch=21.0, lean=R0.lean + 2.2, hand_l=R0.hand_l + V(0.0, 0.004, 0.012),
                        head_yaw=R0.head_yaw + 3.0)
            return [K(0.0, k0), K(0.5, k1), K(1.0, k0)]
        hr = R0.hand_r
        k0 = R0.but(head_pitch=19.0)
        k1 = R0.but(head_pitch=22.0, lean=R0.lean + 2.0, hand_r=hr + V(0.05, 0.02, 0.05), wrist_r=(-30.0, 0.0, 10.0))
        k2 = R0.but(head_pitch=20.0, hand_r=hr + V(0.09, 0.03, 0.06), wrist_r=(-40.0, 0.0, -20.0))
        return [K(0.0, k0), K(0.4, k1), K(0.6, k2), K(1.0, k0)]

    # Guard (web :151-155): near-still, head −1° ↔ +2°, lean 0 ↔ 2°, 0.5 u dip
    def _work_guard(self) -> list:
        R0 = self.ready()
        U = self.U
        k0 = R0.but(head_pitch=-1.0)
        k1 = R0.but(head_pitch=2.0, lean=R0.lean + 2.0, pelvis=R0.pelvis + V(0, 0, -0.5 * U), head_yaw=-6.0)
        return [K(0.0, k0), K(0.5, k1), K(1.0, k0)]

    # Lookout (web :146-150): hand raised to the brow (−19 u), head −8° ↔ −5°, lean 3° ↔ 6°, weight shift 0.8 u
    def _work_lookout(self) -> list:
        R0 = self.ready()
        U = self.U
        # the wrist at the right temple, the flat hand across the brow above the eyes (palm down), the elbow out to
        # the side at shoulder height: from the W1 camera the face stays visible under the hand
        brow = self.J["head_center"] + V(-0.12, 0.185, 0.13)
        base = R0.but(hand_r=brow, elbow_r=V(-1.0, 0.15, 0.75), wrist_r=(0.0, 0.0, 0.0), clav_r=(22.0, 4.0))
        k0 = base.but(head_pitch=-8.0, lean=3.0, head_yaw=-8.0)
        k1 = base.but(head_pitch=-5.0, lean=6.0, head_yaw=6.0, pelvis=R0.pelvis + V(0.8 * U, 0, 0.3 * U),
                      hip_roll=2.0, hand_r=brow + V(0.0, 0.01, -0.01))
        return [K(0.0, k0), K(0.5, k1), K(1.0, k0)]

    # ── IdleNPC_Fidget (NEW): one personal gesture, IdleNPC pose in and out ─────────────────────────────
    def fidget(self):
        R0 = self.ready()
        kind = self.npc.fidget
        L = 2600.0
        keys = getattr(self, f"_fidget_{kind}")(R0)
        c = self.clip("IdleNPC_Fidget", L, keys, L, loop=False)
        c.key_poses_ms = [round(k.t * L, 1) for k in keys]
        return c

    def _hand_to(self, R0: BP, s: str, pt: Vector, **kw) -> BP:
        sg = 1.0 if s == "l" else -1.0
        return R0.but(**{f"hand_{s}": pt, f"elbow_{s}": V(sg * 0.9, 0.2, -0.5)}, **kw)

    def _fidget_stretch(self, R0):
        a = R0.but(lean=-9.0, head_pitch=-10.0, clav_l=(10.0, -12.0), clav_r=(10.0, -12.0),
                   hand_l=R0.hand_l + V(0.06, -0.06, 0.05), hand_r=R0.hand_r + V(-0.06, -0.06, 0.05),
                   pelvis=R0.pelvis + V(0, -0.01, 0.012))
        b = R0.but(lean=-4.0, head_roll=8.0, head_pitch=-4.0, side=-3.0, clav_l=(4.0, 6.0), clav_r=(-2.0, 8.0))
        c = R0.but(head_roll=-8.0, side=3.0, clav_l=(-2.0, 8.0), clav_r=(4.0, 6.0))
        return [K(0.0, R0), K(0.25, a, "out"), K(0.45, a), K(0.6, b), K(0.78, c), K(1.0, R0)]

    def _fidget_mustache(self, R0):
        s = "r"
        p1 = self.J["head_center"] + V(-0.02, 0.24, -0.11)
        p2 = p1 + V(-0.09, -0.01, -0.02)
        a = self._hand_to(R0, s, p1, wrist_r=(-60.0, 0.0, 40.0), head_pitch=-4.0, head_roll=-5.0, head_yaw=-6.0)
        b = self._hand_to(R0, s, p2, wrist_r=(-60.0, 0.0, 40.0), head_pitch=-6.0, head_roll=-5.0, head_yaw=-8.0)
        return [K(0.0, R0), K(0.22, a, "out"), K(0.40, b), K(0.52, a), K(0.70, b), K(1.0, R0)]

    def _fidget_ponder(self, R0):
        """The elder lifts his eyes to the sky, leans on the staff with a slow breath, then nods."""
        a = R0.but(head_pitch=-16.0, head_yaw=-12.0, lean=R0.lean - 3.0, pelvis=R0.pelvis + V(0, 0, 0.006))
        b = R0.but(head_pitch=-12.0, head_yaw=10.0, lean=R0.lean - 2.0)
        c = R0.but(head_pitch=R0.head_pitch + 10.0, head_yaw=-2.0, lean=R0.lean + 2.0)
        return [K(0.0, R0), K(0.3, a, "out"), K(0.55, b), K(0.78, c), K(1.0, R0)]

    def _fidget_beard(self, R0):
        s = "l"
        HKu = self.F.HK
        top = self.J["head_center"] + V(0.08, 0.24, -6.2 * HKu)
        bot = top + V(0.02, 0.03, -0.17)
        it = {"off": V(1.0, 0.15, -0.55).normalized(), "off_up": V(0, -1, 0)}
        a = self._hand_to(R0, s, top, wrist_l=(-40.0, 0.0, -30.0), head_pitch=R0.head_pitch - 6.0, **it)
        b = self._hand_to(R0, s, bot, wrist_l=(-40.0, 0.0, -30.0), head_pitch=R0.head_pitch - 2.0, **it)
        return [K(0.0, R0), K(0.22, a, "out"), K(0.42, b), K(0.56, a), K(0.76, b), K(1.0, R0)]

    def _fidget_hair(self, R0):
        s = "l"
        p = self.J["head_center"] + V(0.20, 0.02, 0.02)
        a = self._hand_to(R0, s, p, wrist_l=(-70.0, 0.0, -60.0), head_roll=7.0, head_yaw=10.0, head_pitch=4.0)
        b = self._hand_to(R0, s, p + V(-0.01, -0.05, -0.04), wrist_l=(-70.0, 0.0, -60.0), head_roll=9.0,
                          head_yaw=12.0, head_pitch=6.0)
        return [K(0.0, R0), K(0.3, a, "out"), K(0.55, b), K(0.7, a), K(1.0, R0)]

    def _fidget_hat(self, R0):
        s = self.free_hand()
        sg = 1.0 if s == "l" else -1.0
        brim = self.J["head_center"] + V(sg * 0.10, 0.26, 0.11)
        a = self._hand_to(R0, s, brim, **{f"wrist_{s}": (-70.0, 0.0, sg * -40.0)}, head_pitch=R0.head_pitch + 2.0,
                          **self.gesture_item(s, "up"))
        b = self._hand_to(R0, s, brim + V(0.0, 0.03, -0.035), **{f"wrist_{s}": (-75.0, 0.0, sg * -40.0)},
                          head_pitch=R0.head_pitch + 9.0, **self.gesture_item(s, "up"))
        return [K(0.0, R0), K(0.28, a, "out"), K(0.48, b), K(0.66, b), K(1.0, R0)]

    def _fidget_look(self, R0):
        a = R0.but(head_yaw=55.0, twist=18.0, hip_yaw=6.0, head_pitch=-3.0)
        b = R0.but(head_yaw=-58.0, twist=-18.0, hip_yaw=-6.0, head_pitch=-3.0)
        return [K(0.0, R0), K(0.22, a, "out"), K(0.4, a), K(0.62, b), K(0.8, b), K(1.0, R0)]

    def _fidget_straps(self, R0):
        F = self.F
        zc = F.th(F.L - 2.6)
        pl = F.torso.point(zc, 30.0, 0.05)
        pr = F.torso.point(zc, -30.0, 0.05)
        # the near hand hooks the right strap and hitches the pack up with a shrug; the purse hand stays low (a bag
        # hanging from a fist at the chest would swing into the belly)
        del pl
        a = R0.but(hand_r=pr + V(0, 0.02, -0.04), elbow_r=V(-0.9, 0.0, -0.4), wrist_r=(-30.0, 0.0, 20.0),
                   hand_l=R0.hand_l + V(0.02, 0.0, 0.01), head_pitch=4.0, head_yaw=-6.0)
        b = a.but(clav_l=(14.0, 0.0), clav_r=(16.0, 0.0), pelvis=R0.pelvis + V(0, 0, 0.02),
                  hand_l=a.hand_l + V(0, 0, 0.035), hand_r=a.hand_r + V(0, 0, 0.06), head_pitch=-2.0)
        return [K(0.0, R0), K(0.25, a, "out"), K(0.45, b, "out"), K(0.6, a, "in"), K(0.72, b), K(0.84, a),
                K(1.0, R0)]

    def _fidget_specs(self, R0):
        s = "r"
        bridge = self.J["head_center"] + V(0.0, 0.25, 0.03)
        a = self._hand_to(R0, s, bridge + V(-0.05, 0.02, -0.06), wrist_r=(-80.0, 0.0, 30.0), head_pitch=-2.0)
        b = self._hand_to(R0, s, bridge + V(-0.05, 0.02, -0.03), wrist_r=(-85.0, 0.0, 30.0), head_pitch=-6.0)
        return [K(0.0, R0), K(0.3, a, "out"), K(0.45, b), K(0.62, b), K(1.0, R0)]

    # ── escort: locomotion, Hurt, Death ─────────────────────────────────────────────────────────────────
    def locomotion(self, name: str, length: float, speed: float, duty: float, lift: float, bob: float,
                   lean: float, arm: float, flow: float, crouch: float, stance: float = 0.11):
        J = self.J
        R0 = self.ready()

        def s(t_ms: float) -> BP:
            t = (t_ms / length) % 1.0
            ((fl, pl, tl), (fr, pr, tr)), dz, swing = self.gait_feet(t, speed, length, duty, lift, bob)
            ph = 2 * math.pi * t
            sw = max(-1.0, min(1.0, swing))
            sp = speed / 3.333
            if speed > 2.0:
                # run: elbows bent ~90°, the fists pumping fore and aft at the hips (the purse hand swings less)
                hl = self.shoulder("l") + V(0.15, 0.08 + arm * 0.6 * sw, -0.40 + 0.05 * max(0.0, sw))
                hr = self.shoulder("r") + V(-0.10, 0.06 - arm * sw, -0.40 + 0.06 * max(0.0, -sw))
            else:
                hl = self.hang_hand("l", 0.17, 0.07, 0.84) + V(0, arm * 0.3 * sw, 0.02 * abs(sw))
                hr = self.hang_hand("r", 0.08, 0.05, 0.86) + V(0, -arm * sw, 0.03 * abs(sw))
            bp = BP(
                pelvis=V(0, 0.02 * sp, crouch + dz), hip_pitch=1.5, hip_yaw=-6.0 * sw * sp ** 0.5,
                hip_roll=-2.5 * math.sin(ph), lean=lean + 1.2 * abs(math.sin(ph)), twist=7.0 * sw * sp ** 0.5,
                head_pitch=-lean * 0.5, head_yaw=-2.5 * sw,
                foot_l=J["ankle_l"] + V(stance - J["ankle_l"].x, 0, 0) + fl,
                foot_r=J["ankle_r"] + V(-(stance + J["ankle_r"].x), 0, 0) + fr,
                foot_l_rot=(pl, 0, 7), foot_r_rot=(pr, 0, 7), toe_l=tl, toe_r=tr,
                knee_l=V(0.15, 1, 0), knee_r=V(-0.15, 1, 0),
                hand_l=hl, hand_r=hr, elbow_l=V(0.5, -0.9, -0.3), elbow_r=V(-0.5, -0.9, -0.3),
                wrist_r=(-20.0, 0.0, 0.0) if speed > 2.0 else (0.0, 0.0, 0.0),
                off=V(0.3, -0.12 * sw, -1.0), off_up=V(0, 1, 0), flow=flow + 0.08 * math.sin(2 * ph), wave=2 * ph)
            return bp
        c = self.proc_clip(name, length, s, loop=True, notifies=anim.run_contacts_ms(length), ref_speed=speed)
        c.key_poses_ms = [round(length * i / 8, 1) for i in range(8)]
        return c

    def walk(self):
        return self.locomotion("Walk", 1000.0, WALK_SPEED, duty=0.62, lift=0.07, bob=0.014, lean=4.0, arm=0.11,
                               flow=0.18, crouch=-0.02)

    def run(self):
        return self.locomotion("Run", 700.0, ESCORT_RUN_SPEED, duty=0.30, lift=0.10, bob=0.03, lean=11.0, arm=0.20,
                               flow=0.38, crouch=-0.05)

    def hurt(self):
        R0 = self.ready()
        U = self.U
        hit = R0.but(lean=-11.0, head_pitch=-12.0, pelvis=R0.pelvis + V(0, -0.025, -0.01),
                     hand_l=R0.hand_l + V(0.04, 0.05, 0.12), hand_r=R0.hand_r + V(-0.05, 0.04, 0.10),
                     clav_l=(6.0, -4.0), clav_r=(6.0, -4.0), flow=0.3)
        keys = [K(0.0, R0), K(0.25, hit, "out"), K(1.0, R0)]
        c = self.clip("Hurt", 300.0, keys, 300.0)
        c.key_poses_ms = [0.0, 75.0, 150.0, 300.0]
        del U
        return c

    def death(self):
        """Hit, knees buckle, falls forward onto the belly (the pack on top), arms thrown forward then limp."""
        R0 = self.ready()
        J = self.J
        hit = R0.but(lean=-10.0, head_pitch=-14.0, pelvis=R0.pelvis + V(0, -0.02, 0.0), flow=0.3,
                     hand_l=R0.hand_l + V(0.05, 0.04, 0.10), hand_r=R0.hand_r + V(-0.05, 0.04, 0.10))
        kneel = R0.but(pelvis=V(0, 0.06, -0.30), lean=24.0, hip_pitch=10.0, head_pitch=6.0,
                       foot_l=J["ankle_l"] + V(0.03, -0.12, 0.0), foot_r=J["ankle_r"] + V(-0.03, -0.08, 0.0),
                       foot_l_rot=(55.0, 0, 10), foot_r_rot=(50.0, 0, 8), knee_l=V(0.2, 1, -0.4),
                       knee_r=V(-0.2, 1, -0.4),
                       hand_l=self.J["hip_l"] + V(0.20, 0.30, -0.05), hand_r=self.J["hip_r"] + V(-0.10, 0.30, -0.05),
                       flow=0.25)
        down = kneel.but(spin=78.0, spin_axis=Vector((1, 0, 0)), pivot=V(0, 0.12, 0.25), lean=12.0, head_pitch=-30.0,
                         hand_l=V(0.32, 0.55, 0.85), hand_r=V(-0.34, 0.50, 0.88), elbow_l=V(0.9, 0.0, 0.3),
                         elbow_r=V(-0.9, 0.0, 0.3), flow=0.1, head_yaw=-25.0)
        keys = [K(0.0, R0), K(0.15, hit, "out"), K(0.5, kneel, "in"), K(0.85, down, "in"), K(1.0, down)]
        c = self.clip("Death", 600.0, keys, 600.0, post=self.grounded(snap=False))
        c.key_poses_ms = [0.0, 90.0, 300.0, 420.0, 510.0, 600.0]
        return c

    # ── rescue: Cower (NEW, 1000 loop) ──────────────────────────────────────────────────────────────────
    def cower(self):
        R0 = self.ready()
        J = self.J
        hc = J["head_center"]
        base = R0.but(pelvis=V(0, 0.08, -0.22), lean=26.0, hip_pitch=8.0, head_pitch=14.0,
                      foot_l=J["ankle_l"] + V(0.06, -0.06, 0.0), foot_r=J["ankle_r"] + V(-0.06, 0.08, 0.0),
                      foot_l_rot=(0, 0, 20), foot_r_rot=(0, 0, 14), knee_l=V(0.5, 1, 0), knee_r=V(-0.5, 1, 0),
                      elbow_l=V(0.6, 1.0, -0.2), elbow_r=V(-0.6, 1.0, -0.2), wrist_l=(-60.0, 0.0, -40.0),
                      wrist_r=(-60.0, 0.0, 40.0), clav_l=(10.0, 12.0), clav_r=(10.0, 12.0))
        hl = hc + V(0.10, 0.24, 0.02)
        hr = hc + V(-0.12, 0.22, 0.06)

        def s(t_ms: float) -> BP:
            t = t_ms / 1000.0
            ph = 2 * math.pi * t
            tr = math.sin(ph * 9.0) * 0.004
            look = 22.0 * math.sin(ph) + 6.0 * math.sin(3 * ph)
            bp = base.but(head_yaw=look, twist=look * 0.25, pelvis=base.pelvis + V(tr, 0, tr * 0.5),
                          head_pitch=14.0 + 3.0 * math.sin(2 * ph + 1.0), wave=ph, flow=0.1)
            return bp.but(hand_l=self.chest(bp, hl.x + tr, -hl.y, hl.z), hand_r=self.chest(bp, hr.x - tr, -hr.y, hr.z))
        c = self.proc_clip("Cower", 1000.0, s, loop=True)
        c.key_poses_ms = [0.0, 250.0, 500.0, 750.0]
        return c

    # ── the set ─────────────────────────────────────────────────────────────────────────────────────────
    def all_clips(self) -> list:
        out = [self.idle(), self.fidget(), self.alert(), self.talk(), self.work()]
        for x in self.npc.extra_clips:
            if x == "Idle":
                out.append(self.idle("Idle"))
            elif x == "Walk":
                out.append(self.walk())
            elif x == "Run":
                out.append(self.run())
            elif x == "Hurt":
                out.append(self.hurt())
            elif x == "Death":
                out.append(self.death())
            elif x == "Cower":
                out.append(self.cower())
        return out

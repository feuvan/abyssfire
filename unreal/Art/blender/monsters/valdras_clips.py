"""Clips of 焚旗元帅·瓦德拉斯 Lord-Marshal Valdras (``SK_Mon_Valdras``; monsters.md §2.6, chapter1.md §11).

Poses are authored on the hero ``BP`` body pose (``heroes/common.py``, metres in body space: ``V(lat, fwd, up)``) and
solved by ``common.to_pose`` through :class:`ValdrasAnims` (on ``uclips.UnburiedAnims``: keyed clips with the web
easing, foot planting on the skinned sabatons, the ``tabard_f/b`` remnants hung from the waist and turned out of the
posed legs) with three additions:

* **two hands on the oath-sword**: BP channels ``two`` (0 … 1, how far the left fist is on the grip) and ``two_y``
  (where along the grip, item metres: the grip proper or the pommel) blend the left hand onto the right fist's item
  every frame (``post``) — the right grip is melted into the gauntlet and never leaves it;
* **the standard** (``standard_cloth``): the pole is rigid and pivots at the rupture; ``droop`` tips it as one piece
  back and to his left as the banner that would not fall finally sags (Death), the cloth going slack under the
  tilted crossbar; the banner (``flag_01..03``) is turned about the crossbar by
  gravity + a looping wind it should not have, streaming back from the pole, never through the pole's plane (it hangs
  behind the pole: the head is on the other side); ``flare`` (Roar) whips it out almost level, the flutter doubled;
* **sword planting**: poses that rest the point on the ground (READY / Idle at attention, Roar, Death) solve the right
  fist height so the tip touches the floor (``planted``).

Timing (data: L9, attack 1300 ms, speed 48 → 1.44 m/s): Idle 1860 loop · Walk 1000 loop (a march) · Attack01 1300
(two-hit: WindupEnd 403, Contact 650, Contact2 1000) · Attack02 1300 (Banner Sweep: WindupEnd 434, Contact 700) ·
Cast01 1500 (Oath-Brand slam: WindupEnd 558, Contact 900) · Roar 1200 (FX_Roar 450) · Lunge 1300 (WindupEnd 496,
Contact 800) · Hurt 360 · Death 2000 (Kneel 1100, Droop 1450) · Rise 1500 (RiseUp 1050) · Stun 800 loop. Specials
wind up ≥ 700 ms (chapter1.md §11: bosses ≥ 700 ms; the crimson decals fill to full at Contact).
"""
from __future__ import annotations

import math

import numpy as np
from mathutils import Quaternion, Vector

from kit import anim
from kit.anim import Pose

import common as C
import hero_base as HB
from common import BP, K, V
from mclips import GoblinAnims
from uclips import UnburiedAnims, WINDUP_OF_CONTACT

import valdras as VD

# Valdras's channels on the shared body pose
C.BP_DEFAULTS.setdefault("two", 0.0)        # left fist on the sword grip (0 … 1)
C.BP_DEFAULTS.setdefault("two_y", VD.SW_LEFT_Y)
C.BP_DEFAULTS.setdefault("droop", 0.0)      # the standard sags (Death)
C.BP_DEFAULTS.setdefault("flare", 0.0)      # the standard flares (Roar)

CONTACT_MS = 650.0
ATTACK_MS = 1300.0
CONTACT2_MS = 1000.0
SWEEP_CONTACT = 700.0
BRAND_CONTACT, CAST_MS = 900.0, 1500.0
LUNGE_CONTACT, LUNGE_MS = 800.0, 1300.0
ROAR_MS, ROAR_FX = 1200.0, 450.0
DEATH_MS, RISE_MS = 2000.0, 1500.0
IDLE_MS, STUN_MS, HURT_MS = 1860.0, 800.0, 360.0
WALK = dict(cycle=1000.0, duty=0.60, lift=0.10, bob=0.022, lean=2.0, twist=5.0, arm=0.16, stance=0.02)
FLUTTER_MS = 620.0
FLAG_MIN_OPEN = math.radians(3.0)
KEEP_UPRIGHT = 0.65               # how much of the back's tilt the upper pole takes back (the standard stays upright)


def windup(contact: float) -> float:
    return round(WINDUP_OF_CONTACT * contact, 1)


def aim(lat: float, fwd: float, up: float, edge=None) -> dict:
    """Sword direction (+Y of the item) and its leading edge (+Z) as close to ``edge`` (default world up) as the
    direction allows (the hammer grip rolls it anyway unless the forearm lines up with the blade)."""
    d = V(lat, fwd, up).normalized()
    e = Vector(edge) if edge is not None else Vector((0, 0, 1))
    e = e - d * e.dot(d)
    if e.length < 1e-4:
        e = V(0, 1, 0) - d * d.dot(V(0, 1, 0))
    return {"wpn": d, "wpn_up": e.normalized()}


class ValdrasAnims(UnburiedAnims):
    """Clip factory of the Lord-Marshal (``kit`` = :class:`valdras.ValdrasKit`)."""

    PANEL_HALF = 0.16
    FLAP_CLEAR = 0.012

    def __init__(self, kit):
        super().__init__(kit, VD.REF_SPEED, WALK, ready, CONTACT_MS, ATTACK_MS, lambda A: {},
                         extra_cloth=standard_cloth)

    # ── ground contact: the sabatons; the tabard envelope against the armoured legs ───────────────────────
    def bind_meshes(self, body, items: dict | None = None, foot_parts=None) -> None:
        import json
        GoblinAnims.bind_meshes(self, body, items, foot_parts or (lambda n: n.startswith("sabaton")))
        names = json.loads(body["af_parts"])
        pick = lambda n: (n.startswith(("cuisse", "poleyn", "greave", "tasset", "thigh", "sabaton"))   # noqa: E731
                          and not n.endswith("_hull"))
        self.env = C.SkinPoints(self.rig).add_mesh(body, names, pick, step=3)

    # ── the left fist on the grip (``two`` / ``two_y``) ────────────────────────────────────────────────────
    def two_hands(self, bp: BP) -> BP:
        w = max(0.0, min(1.0, bp.two))
        if w <= 1e-4:
            return bp
        two = self.second_hand(bp, bp.two_y, 0.0, 0.0)
        hl = Vector(bp.hand_l) if bp.hand_l is not None else Vector(two.hand_l)
        ww = w * w * (3.0 - 2.0 * w)
        return bp.but(hand_l=hl.lerp(Vector(two.hand_l), ww))

    def body_clip(self, name, length, sampler, loop=False, notifies=None, ref_speed=None, post=None):
        inner = post if post is not None else self.plant

        def post2(bp, t, _inner=inner):
            return _inner(self.two_hands(bp), t)
        return HB.hero_clip(self.rig, name, length, sampler, cloth=self.cloth, loop=loop, notifies=notifies,
                            ref_speed=ref_speed, post=post2, grips=self.grips)

    def solve(self, bp: BP) -> Pose:
        return C.to_pose(self.rig, self.two_hands(bp), self.cloth, grips=self.grips)

    # ── the cross's orientation: roll the hammer grip so the item's +Z (the cross, the edges) lies along ``edge`` ─
    def edge_roll(self, bp: BP, edge) -> BP:
        bp0 = bp.but(wrist_r=(bp.wrist_r[0], 0.0, bp.wrist_r[2]))
        m = self.mats(self.two_hands(bp0))["weapon_r"].to_3x3()
        d = (m @ Vector((0, 1, 0))).normalized()
        z = (m @ Vector((0, 0, 1))).normalized()
        e = Vector(edge) - d * Vector(edge).dot(d)
        if e.length < 1e-4:
            return bp0
        e.normalize()
        ang = math.degrees(math.atan2(d.dot(z.cross(e)), z.dot(e)))
        return bp0.but(wrist_r=(bp.wrist_r[0], ang, bp.wrist_r[2]))

    # ── the planted sword: solve the fist height so the point touches the floor ───────────────────────────
    def planted(self, bp: BP, floor: float = 0.006, iters: int = 4) -> BP:
        for _ in range(iters):
            tip = self.item_point(self.two_hands(bp), "weapon_r", (0.0, VD.SW_TIP, 0.0))
            dz = tip.z + bp.lift - floor
            if abs(dz) < 5e-4:
                break
            bp = bp.but(hand_r=Vector(bp.hand_r) - Vector((0.0, 0.0, dz)))
        return bp

    # ── clips ────────────────────────────────────────────────────────────────────────────────────────────
    def idle(self, length: float = IDLE_MS, extra=None):
        """Idle 1860 ms loop, at attention: a slow heave of the plate (breath that is not breath), the helm turning a
        little to sweep the dais, the banner snapping in its wind; the sword stays planted, both hands on the hilt."""
        R0 = self.ready()

        def s(t: float) -> BP:
            ph = 2 * math.pi * t / length
            b = math.sin(ph)
            look = math.sin(ph * 1.0 + 0.4)
            return R0.but(pelvis=R0.pelvis + V(0.0, 0.0, 0.006 * b), lean=R0.lean + 1.2 * b,
                          head_pitch=R0.head_pitch - 1.5 * b, head_yaw=R0.head_yaw + 7.0 * look,
                          clav_l=(R0.clav_l[0] + 2.0 * b, R0.clav_l[1]), clav_r=(R0.clav_r[0] + 2.0 * b, R0.clav_r[1]),
                          hand_l=Vector(R0.hand_l) + V(0, 0, 0.004 * b), flow=R0.flow + 0.05 * b, wave=ph,
                          sway=0.10 * math.sin(ph + 1.0))
        return self.body_clip("Idle", length, s, loop=True, post=self.plant_sword)

    def plant_sword(self, bp: BP, t: float = 0.0) -> BP:
        return self.plant(self.planted(bp), t)

    def walk(self, name: str = "Walk", extra=None):
        """Walk 1000 ms loop at 1.44 m/s: a march — upright, a long heavy stride heel-first, the oath-sword carried at
        shoulder arms on the right (the blade upright before the pauldron), the left arm swinging in a fist; the
        standard bobs and the banner streams back."""
        w = self.walk_p
        R0 = self.ready()
        J = self.J
        length = w["cycle"]
        speed = self.ref_speed
        M0 = march_pose(self)

        def s(t_ms: float) -> BP:
            t = (t_ms / length) % 1.0
            fl, fr, dz, swing = anim.run_gait(t, speed, length, duty=w["duty"], lift=w["lift"], bob=w["bob"])
            ph = 2 * math.pi * t
            sw = max(-1.0, min(1.0, swing))
            pitches = []
            for s_, off in (("l", 0.0), ("r", 0.5)):
                p_ = (t + off) % 1.0
                if p_ >= w["duty"]:
                    sl = (p_ - w["duty"]) / (1.0 - w["duty"])
                    pitches.append(14.0 * (1 - sl) ** 2 - 12.0 * max(0.0, sl - 0.7) / 0.3)
                else:
                    pitches.append(0.0)
            return M0.but(
                pelvis=M0.pelvis + V(0.010 * math.sin(ph), 0.0, dz), hip_roll=-3.0 * math.sin(ph),
                hip_yaw=M0.hip_yaw - 7.0 * sw, lean=M0.lean + w["lean"] + 1.0 * abs(math.sin(ph)),
                twist=M0.twist + w["twist"] * sw, head_yaw=M0.head_yaw - 4.0 * sw,
                foot_l=J["ankle_l"] + V(w["stance"], 0, 0) + fl, foot_r=J["ankle_r"] - V(w["stance"], 0, 0) + fr,
                foot_l_rot=(pitches[0], 0, 4), foot_r_rot=(pitches[1], 0, 4),
                knee_l=V(0.10, 1, 0), knee_r=V(-0.10, 1, 0),
                hand_r=Vector(M0.hand_r) + V(0, 0.020 * sw, 0.010 * abs(sw)),
                hand_l=Vector(M0.hand_l) + V(0.0, -w["arm"] * sw, 0.06 * max(0.0, -sw) + 0.02 * abs(sw)),
                flow=M0.flow + 0.25 + 0.05 * math.sin(2 * ph), wave=2 * ph, sway=0.25 * math.sin(ph))
        notif = anim.run_contacts_ms(length)
        return self.body_clip(name, length, s, loop=True, notifies=notif, ref_speed=speed)

    def hurt(self, length: float = HURT_MS):
        """Hurt 360 ms: he barely gives — the helm snaps back, the shoulders jolt, the banner shudders; the planted
        sword holds him."""
        R0 = self.ready()
        rec = R0.but(pelvis=R0.pelvis + V(0, -0.025, -0.012), lean=R0.lean - 7.0, head_pitch=R0.head_pitch - 14.0,
                     head_roll=R0.head_roll + 6.0, twist=R0.twist - 5.0, clav_l=(6.0, 0.0), clav_r=(6.0, 0.0),
                     flow=0.5)
        part = R0.but(lean=R0.lean - 2.0, head_pitch=R0.head_pitch - 4.0, flow=0.3)
        keys = [K(0.0, R0), K(0.22, rec, "out"), K(0.62, part, "smooth"), K(1.0, R0, "smooth")]
        c = self.clip("Hurt", length, keys, length, post=self.plant_sword)
        c.key_poses_ms = [0.0, round(0.22 * length, 1), round(0.62 * length, 1), length]
        return c

    def death_keys(self) -> list:
        return death_poses(self)

    def death(self, length: float = DEATH_MS):
        """Death 2000 ms: the blow lands, he reels, leans on the planted oath-sword, sinks onto his right knee with
        both fists on the hilt, bows his helm onto them — and the standard on his back finally droops, the banner
        hanging slack (``Kneel`` 1100, ``Droop`` 1450). The pose he dies in is the cutscene's (cs_rf_marshal_falls)."""
        rec, lean_on, kneel, bow = self.death_keys()
        R0 = self.ready()
        keys = [K(0.0, R0), K(0.08, rec, "out"), K(0.30, lean_on, "smooth"), K(0.55, kneel, "in"),
                K(0.72, bow, "out"), K(1.0, bow.but(head_pitch=bow.head_pitch + 3.0), "smooth")]
        c = self.clip("Death", length, keys, length, notifies={"Kneel": 1100.0, "Droop": 1450.0},
                      post=self.death_post(snap=False))
        c.key_poses_ms = [0.0, 0.08 * length, 0.30 * length, 0.55 * length, 0.72 * length, length]
        return c

    def death_post(self, snap: bool = False):
        g = self.grounded(snap=snap, pivot_pelvis=False)

        def post(bp, t):
            return self.plant(g(self.planted(bp), t), t)
        return post

    def rise(self, length: float = RISE_MS):
        """Rise 1500 ms (Death reversed and retimed: his spawn / the farmable return): the banner lifts first, then he
        pushes up off the planted sword, rises off the knee and stands to attention (``RiseUp`` 1050)."""
        rec, lean_on, kneel, bow = self.death_keys()
        R0 = self.ready()
        lift = bow.but(droop=0.35, head_pitch=bow.head_pitch - 18.0, flow=0.4)
        keys = [K(0.0, bow), K(0.16, lift, "smooth"), K(0.46, kneel.but(droop=0.0, head_pitch=kneel.head_pitch - 10.0),
                                                       "smooth"),
                K(0.70, lean_on.but(head_pitch=lean_on.head_pitch - 8.0), "in"), K(1.0, R0, "out")]
        c = self.clip("Rise", length, keys, length, notifies={"RiseUp": 1050.0}, post=self.death_post(snap=False))
        c.key_poses_ms = [0.0, 0.16 * length, 0.46 * length, 0.70 * length, length]
        c.manifest_extra = {"usage": "his spawn and the farmable return after the finale (monster_ai storyBoss, M7): "
                                     "Death reversed and retimed — kneeling at the dais, he rises to attention"}
        return c

    def stun(self, length: float = STUN_MS, **_):
        """Stun 800 ms loop: dazed on his feet, swaying, the helm lolling, the sword point dragged across the
        ground, the banner flapping."""
        R0 = self.ready()

        def s(t: float) -> BP:
            ph = 2 * math.pi * t / length
            c, sn = math.cos(ph), math.sin(ph)
            return R0.but(pelvis=R0.pelvis + V(0.014 * c, -0.01, -0.035 + 0.008 * sn), lean=R0.lean + 6.0 + 2.0 * sn,
                          side=R0.side + 4.0 * c, head_pitch=R0.head_pitch + 14.0 + 4.0 * sn,
                          head_roll=R0.head_roll + 10.0 * c, head_yaw=R0.head_yaw + 8.0 * sn,
                          hand_r=Vector(R0.hand_r) + V(0.04 * c, 0.02, -0.03), two=0.6, flow=0.2, wave=ph,
                          sway=0.4 * c)
        return self.body_clip("Stun", length, s, loop=True, post=self.plant_sword)


# ── poses ──────────────────────────────────────────────────────────────────────────────────────────────────────
def ready(A: ValdrasAnims) -> BP:
    """READY = at attention: upright, chin up (the visor slit on the hero), feet set, the oath-sword planted
    point-down before him, the right fist (melted to the grip) at the navel, the left hand resting on the pommel."""
    J = A.J
    bp = BP(pelvis=V(0.0, 0.0, -0.012), lean=2.0, hip_pitch=0.0, head_pitch=-9.0, head_yaw=0.0,
            foot_l=J["ankle_l"] + V(0.04, 0.03, 0.0), foot_r=J["ankle_r"] + V(-0.04, 0.0, 0.0),
            foot_l_rot=(0.0, 0.0, 9.0), foot_r_rot=(0.0, 0.0, 9.0), knee_l=V(0.15, 1, 0), knee_r=V(-0.15, 1, 0),
            hand_r=V(-0.035, 0.40, 1.30), elbow_r=V(-1.0, 0.25, -0.3), **aim(0.0, 0.10, -1.0, edge=V(1, 0, 0)),
            hand_l=V(0.05, 0.36, 1.50), elbow_l=V(1.0, 0.25, -0.3), two=1.0, two_y=VD.SW_POMMEL_HOLD,
            clav_l=(-4.0, 0.0), clav_r=(-4.0, 0.0), flow=0.15)
    # the cross square to his front (a T before him: the hilt reads as the crusade's cross), point on the floor
    return A.planted(A.edge_roll(bp, V(1.0, 0.0, 0.0)))


def march_pose(A: ValdrasAnims) -> BP:
    """The march carry: the oath-sword upright at the right shoulder (shoulder arms), the fist at the hip."""
    R0 = A.ready()
    bp = R0.but(lean=1.0, head_pitch=-8.0, hand_r=V(-0.45, 0.10, 1.12), elbow_r=V(-0.9, -0.4, -0.2),
                **aim(-0.30, 0.02, 1.0, edge=V(0, 1, 0)), hand_l=V(0.30, 0.04, 1.06), elbow_l=V(0.8, -0.2, -0.5),
                two=0.0, clav_l=(-2.0, 0.0), clav_r=(0.0, 0.0), flow=0.3)
    return bp.but(wrist_r=(0.0, 90.0, 0.0))             # the cross square to the forearm (clear of the vambrace)


def death_poses(A: ValdrasAnims) -> list:
    """Recoil → leaning on the planted sword → down on both knees (a knight's vigil: shins on the floor behind, the
    fists on the hilt before him) → the helm bowed onto the fists, the standard drooping."""
    R0 = A.ready()
    J = A.J
    rec = R0.but(pelvis=R0.pelvis + V(0, -0.04, -0.03), lean=R0.lean - 10.0, head_pitch=R0.head_pitch - 22.0,
                 head_roll=8.0, twist=-6.0, two=0.5, flow=0.6)
    lean_on = R0.but(pelvis=R0.pelvis + V(0, 0.04, -0.16), lean=14.0, head_pitch=R0.head_pitch + 8.0, head_roll=4.0,
                     foot_r=J["ankle_r"] + V(-0.04, -0.08, 0.0), knee_l=V(0.2, 1, 0), knee_r=V(-0.2, 1, 0),
                     hand_r=V(-0.035, 0.44, 1.18), two=1.0, flow=0.3)
    sh = A.rig.spec
    knee_h = 0.078
    hz = knee_h + sh.thigh * 0.95
    P0 = J["pelvis"]
    kneel = lean_on.but(pelvis=V(0.0, 0.04, hz + sh.pelvis_offset - P0.z), lean=8.0, hip_pitch=-6.0,
                        head_pitch=4.0, foot_l=V(J["ankle_l"].x + 0.02, -0.44, 0.115),
                        foot_r=V(J["ankle_r"].x - 0.02, -0.44, 0.115), foot_l_rot=(74.0, 0.0, 4.0),
                        foot_r_rot=(74.0, 0.0, 4.0), knee_l=V(0.1, 1, -0.3), knee_r=V(-0.1, 1, -0.3),
                        hand_r=V(-0.035, 0.46, 1.02), elbow_l=V(1.0, 0.1, 0.5), elbow_r=V(-1.0, 0.1, 0.3),
                        droop=0.25, flow=0.15)
    bow = kneel.but(lean=22.0, head_pitch=36.0, head_roll=5.0, droop=1.0, flow=0.0, hand_r=V(-0.035, 0.46, 0.96))
    return [rec, lean_on, kneel, bow]


# ── the standard (``extra_cloth``) ─────────────────────────────────────────────────────────────────────────────
_REST: dict = {}


DROOP_DEG = 36.0                  # Death: the whole standard tips this far from upright, at the rupture …
DROOP_DIR = (0.62, 0.78)          # … toward (his left +X, back +Y), in the pelvis' heading: the crossbar tilts ~22°
DROOP_SLACK = (0.0, -9.0, 14.0)   # the drooped banner's flag_01..03 hinge (deg) off plumb: the cloth sags in on
#                                   the pole at mid-height and its burnt tails swing back out, limp


def standard_cloth(p: Pose, bp: BP, A: ValdrasAnims) -> None:
    """The standard (review round 2). The pole is **one rigid piece** on ``standard_01``, which pivots at the
    rupture in the backplate: it holds itself upright (``KEEP_UPRIGHT`` of the way back to its rest direction
    whatever the back does) and, with ``droop``, tips as one piece ``DROOP_DEG`` back and to his left from upright
    — the crossbar tilting with it; ``standard_02`` (crossbar, finial) never turns against ``standard_01``. The banner
    (``flag_01..03``) hinges about the crossbar: gravity + a looping wind it should not have, ``flare`` (Roar) whips
    it out; drooped it goes slack — plumb under the tilted bar (the fly end sags below the low end of the bar), no
    wind, the tails hanging limp."""
    rig = A.rig
    bones = rig.obj.data.bones
    key = id(rig)
    if key not in _REST:
        _REST[key] = {n: bones[n].matrix_local.to_3x3().copy() for n in ("standard_01", "standard_02", "flag_01",
                                                                        "flag_02", "flag_03", "pelvis")}
    rest = _REST[key]
    clk = C.ClothClock
    t, L = (clk.t, clk.length) if clk.sampler is not None and clk.length > 0 else (0.0, FLUTTER_MS)
    droop = max(0.0, min(1.0, bp.droop))
    dsm = droop * droop * (3.0 - 2.0 * droop)
    mats = anim.evaluate(p, return_matrices=True)
    m1 = mats["standard_01"].to_3x3()                                  # rigid with the back (identity basis)
    q_r = m1 @ rest["standard_01"].inverted()
    d_rig = (m1 @ Vector((0.0, 1.0, 0.0))).normalized()
    d_rest = (rest["standard_01"] @ Vector((0.0, 1.0, 0.0))).normalized()
    pel = mats["pelvis"].to_3x3() @ rest["pelvis"].inverted()           # the body's heading (yaw of the hips)
    fwd = pel @ Vector((0.0, -1.0, 0.0))
    fwd.z = 0.0
    fwd = fwd.normalized() if fwd.length > 1e-4 else Vector((0.0, -1.0, 0.0))
    left = Vector((0.0, 0.0, 1.0)).cross(fwd).normalized()             # +X of a body facing −Y
    # upright (the banner that will not fall), swaying a little at the rupture. The pole pivots at the spine, so
    # it rights itself only when the back tips it **forward** (over the helm); when he leans or throws his head
    # back it goes back with him (righting it there would swing the whole pole into the helm and plume)
    tipf = (d_rig - d_rest).dot(fwd)
    kf = max(0.0, min(1.0, (tipf + 0.02) / 0.06))
    d_up = C._slerp_dir(d_rig, d_rest, KEEP_UPRIGHT * kf * kf * (3.0 - 2.0 * kf))
    sway_ax = left
    d_up = Quaternion(sway_ax, math.radians(-2.5 * bp.sway)) @ d_up
    # drooped: upright tipped DROOP_DEG toward back-left of the hips' heading (world terms: what the cutscene shows)
    tip = (left * DROOP_DIR[0] - fwd * DROOP_DIR[1]).normalized()
    ax = Vector((0.0, 0.0, 1.0)).cross(tip).normalized()
    d_droop = Quaternion(ax, math.radians(DROOP_DEG)) @ d_rest
    d_fin = C._slerp_dir(d_up, d_droop, dsm) if dsm > 1e-4 else d_up
    q_fin = d_rig.rotation_difference(d_fin)
    q1 = q_fin.to_matrix() @ q_r
    m1n = q1 @ rest["standard_01"]
    p.aim("standard_01", m1n @ Vector((0.0, 1.0, 0.0)), m1n @ Vector((0.0, 0.0, 1.0)))
    p.abs.pop("standard_02", None)                                      # rigid on standard_01
    q2 = q1                                                             # the same rotation from rest
    m2 = q2 @ rest["standard_02"]
    xw = (q2 @ Vector((1.0, 0.0, 0.0))).normalized()                 # the crossbar axis
    hang = -(m2 @ Vector((0.0, 1.0, 0.0))).normalized()              # down the pole
    nb = xw.cross(hang).normalized()                                   # behind the pole's plane
    down = Vector((0, 0, -1))
    dproj = down - xw * down.dot(xw)
    g = 0.0
    if dproj.length > 1e-4:
        dproj.normalize()
        g = math.atan2(nb.dot(dproj), hang.dot(dproj))
    # the rigid (rest) angle of the cloth from the pole: rotations below are relative to it
    r0 = rest["flag_01"] @ Vector((0, 1, 0))
    h0 = -(rest["standard_02"] @ Vector((0, 1, 0)))
    nb0 = Vector((1, 0, 0)).cross(h0).normalized()
    phi0 = math.atan2(nb0.dot(r0), h0.dot(r0))
    flare = max(0.0, min(1.0, bp.flare))
    n = max(1, round(L / (FLUTTER_MS * (0.5 if flare > 0.5 else 1.0))))
    ph = 2.0 * math.pi * n * t / L
    amp = (0.65 + 0.35 * min(1.0, bp.flow * 2.0)) * (1.0 - droop) * (1.0 + 0.6 * flare)
    for k, nm in enumerate(("flag_01", "flag_02", "flag_03")):
        wave = math.radians(7.0 + 3.0 * k) * amp * math.sin(ph - 1.1 * k)
        lean = math.radians(8.0 + 6.0 * k) * (1.0 - droop) + math.radians(24.0 + 10.0 * k) * flare
        phi = g + lean + wave + math.radians(DROOP_SLACK[k]) * dsm
        phi = max(phi, FLAG_MIN_OPEN + 0.4 * phi0)
        q = Quaternion(xw, phi - phi0)
        rmat = q.to_matrix() @ q2 @ rest[nm]
        p.aim(nm, rmat @ Vector((0, 1, 0)), rmat @ Vector((0, 0, 1)))


def anims(kit) -> ValdrasAnims:
    return ValdrasAnims(kit)


# ── attack clips ───────────────────────────────────────────────────────────────────────────────────────────────
def attack01(A: ValdrasAnims):
    """Attack01 1300 ms, the two-hit combo: he wrenches the oath-sword out of the ground and cocks it high over the
    right shoulder (WindupEnd 403: the wind-up tint reads), brings it down in a great diagonal cut across the front
    with a step onto the left foot (Contact 650), lets it run low to the left, rips it back up in a rising backhand
    to the right (Contact2 1000) and drives it point-down into the ground again (READY)."""
    R0 = A.ready()
    J = A.J
    pull = R0.but(lean=0.0, twist=-10.0, hand_r=V(-0.16, 0.36, 1.42), elbow_r=V(-1.0, 0.0, -0.4),
                  **aim(-0.30, 0.55, -0.70), two=1.0, two_y=VD.SW_LEFT_Y, head_pitch=-12.0, flow=0.25)
    cock = R0.but(pelvis=R0.pelvis + V(0, -0.02, 0.01), lean=-5.0, twist=-30.0, hip_yaw=-12.0, head_pitch=-14.0,
                  head_yaw=18.0, hand_r=V(-0.34, 0.12, 1.86), elbow_r=V(-0.9, 0.3, -0.2),
                  **aim(-0.45, -0.30, 0.85), elbow_l=V(0.6, 0.6, -0.4), two=1.0, two_y=VD.SW_LEFT_Y, clav_r=(16.0, 0.0),
                  flow=0.35)
    hold = cock.but(hand_r=V(-0.35, 0.10, 1.92), **aim(-0.45, -0.36, 0.82), twist=-33.0)
    swing = cock.but(twist=-12.0, lean=4.0, hand_r=V(-0.20, 0.36, 1.86), elbow_r=V(-1.0, 0.0, -0.2),
                     **aim(-0.25, 0.45, 0.85), head_pitch=-20.0, flow=0.45)
    # review round 2: the first cut carried wider and higher (the blade out flat to his left at shoulder height at
    # Contact), so from the back facings it clears the banner and the body at the blow (blade visible at the W1
    # camera: NE 85 %, BACK / NW 100 %; it was hidden from NE)
    hit = R0.but(pelvis=R0.pelvis + V(0.02, 0.12, -0.10), lean=18.0, hip_pitch=4.0, twist=24.0, hip_yaw=10.0,
                 head_pitch=-30.0, head_yaw=-22.0, foot_l=J["ankle_l"] + V(0.05, 0.34, 0.0),
                 foot_r=J["ankle_r"] + V(-0.04, -0.08, 0.0), hand_r=V(0.22, 0.54, 1.40), elbow_r=V(-0.6, -0.2, -0.6),
                 **aim(0.97, 0.06, 0.12), elbow_l=V(0.5, 0.3, -1.0), two=1.0, two_y=VD.SW_LEFT_Y, clav_r=(-2.0, 8.0),
                 flow=0.55)
    follow = hit.but(hand_r=V(0.34, 0.50, 1.24), **aim(0.98, 0.04, -0.06), twist=34.0, lean=20.0)
    low = follow.but(hand_r=V(0.30, 0.52, 1.12), **aim(0.95, 0.12, -0.24), twist=30.0)
    hit2 = R0.but(pelvis=R0.pelvis + V(-0.02, 0.10, -0.06), lean=12.0, twist=-18.0, hip_yaw=-8.0, head_pitch=-26.0,
                  head_yaw=12.0, foot_l=J["ankle_l"] + V(0.05, 0.34, 0.0), foot_r=J["ankle_r"] + V(-0.04, -0.08, 0.0),
                  hand_r=V(-0.26, 0.54, 1.46), elbow_r=V(-0.9, 0.0, -0.4), **aim(-0.75, 0.25, 0.62),
                  elbow_l=V(0.7, 0.4, -0.5), two=1.0, two_y=VD.SW_LEFT_Y, flow=0.55)
    follow2 = hit2.but(hand_r=V(-0.36, 0.30, 1.70), **aim(-0.55, -0.10, 0.85), twist=-26.0, lean=6.0)
    drop = R0.but(lean=6.0, twist=-8.0, head_pitch=-12.0, hand_r=V(-0.12, 0.52, 1.36), elbow_r=V(-1.0, 0.0, -0.3),
                  **aim(-0.15, 0.80, -0.55), two=1.0, two_y=VD.SW_LEFT_Y, flow=0.35)
    keys = [K(0.0, R0), K(230 / ATTACK_MS, pull, "out"), K(330 / ATTACK_MS, cock, "smooth"),
            K(windup(CONTACT_MS) / ATTACK_MS, hold, "smooth"), K(540 / ATTACK_MS, swing, "in"),
            K(CONTACT_MS / ATTACK_MS, hit, "linear"),
            K(760 / ATTACK_MS, follow, "out"), K(860 / ATTACK_MS, low, "smooth"),
            K(CONTACT2_MS / ATTACK_MS, hit2, "in"), K(1090 / ATTACK_MS, follow2, "out"),
            K(1200 / ATTACK_MS, drop, "smooth"), K(1.0, R0, "in")]
    c = A.clip("Attack01", ATTACK_MS, keys, ATTACK_MS, post=attack_post(A),
               notifies={"WindupEnd": windup(CONTACT_MS), "Contact": CONTACT_MS, "Contact2": CONTACT2_MS})
    c.key_poses_ms = [0.0, 230.0, 330.0, windup(CONTACT_MS), 540.0, CONTACT_MS, 760.0, 860.0, CONTACT2_MS, 1090.0, 1200.0,
                      ATTACK_MS]
    c.manifest_extra = {"usage": "basic attack (W1: one hit at Contact 650 — the authored long contact; W2: the combo's "
                                 "second hit at Contact2 1000)", "hits": [CONTACT_MS, CONTACT2_MS]}
    return c


def attack_post(A: ValdrasAnims):
    """End-of-clip re-plant: the last key is READY (point on the floor); in between the feet are planted only."""
    def post(bp, t):
        return A.plant(bp, t)
    return post


def attack02(A: ValdrasAnims):
    """Attack02 1300 ms, 旗扫 Banner Sweep (cone 120°, r 2.6): he coils to the right with the sword drawn back low,
    the standard turning with his back (WindupEnd 434), then sweeps the blade flat across the whole front (Contact
    700, the blade straight ahead), the banner whipping round after it, and plants the sword again."""
    R0 = A.ready()
    J = A.J
    lift = R0.but(lean=0.0, twist=-14.0, head_pitch=-12.0, hand_r=V(-0.22, 0.44, 1.42), elbow_r=V(-1.0, 0.0, -0.4),
                  **aim(-0.45, 0.50, 0.55), two=1.0, two_y=VD.SW_LEFT_Y, flow=0.25)
    coil = R0.but(pelvis=R0.pelvis + V(0.0, -0.02, -0.07), lean=10.0, twist=-42.0, hip_yaw=-16.0, head_pitch=-20.0,
                  head_yaw=34.0, foot_l=J["ankle_l"] + V(0.06, 0.18, 0.0), foot_r=J["ankle_r"] + V(-0.06, -0.10, 0.0),
                  hand_r=V(-0.40, -0.02, 1.12), elbow_r=V(-0.6, 0.6, -0.5), **aim(-0.75, -0.55, -0.25),
                  elbow_l=V(0.6, 0.5, -0.6), two=1.0, two_y=VD.SW_LEFT_Y, flow=0.4)
    hold = coil.but(twist=-46.0, hand_r=V(-0.42, -0.05, 1.10), **aim(-0.70, -0.62, -0.20))
    sweep = R0.but(pelvis=R0.pelvis + V(0.0, 0.08, -0.09), lean=14.0, twist=6.0, hip_yaw=4.0, head_pitch=-24.0,
                   head_yaw=-4.0, foot_l=J["ankle_l"] + V(0.06, 0.18, 0.0), foot_r=J["ankle_r"] + V(-0.06, -0.10, 0.0),
                   hand_r=V(-0.04, 0.70, 1.20), elbow_r=V(-0.8, -0.2, -0.5), **aim(0.10, 1.0, -0.10),
                   elbow_l=V(0.8, -0.2, -0.5), two=1.0, two_y=VD.SW_LEFT_Y, flow=0.7)
    follow = sweep.but(twist=38.0, hip_yaw=14.0, head_yaw=-24.0, hand_r=V(0.36, 0.46, 1.16),
                       **aim(0.95, 0.30, -0.08), flow=0.8)
    settle = follow.but(twist=26.0, hand_r=V(0.26, 0.48, 1.24), **aim(0.70, 0.40, -0.50), lean=10.0)
    keys = [K(0.0, R0), K(150 / ATTACK_MS, lift, "out"), K(320 / ATTACK_MS, coil, "smooth"),
            K(windup(SWEEP_CONTACT) / ATTACK_MS, hold, "smooth"),
            K(SWEEP_CONTACT / ATTACK_MS, sweep, "in"), K(850 / ATTACK_MS, follow, "out"),
            K(1040 / ATTACK_MS, settle, "smooth"), K(1.0, R0, "in")]
    c = A.clip("Attack02", ATTACK_MS, keys, ATTACK_MS, post=attack_post(A),
               notifies={"WindupEnd": windup(SWEEP_CONTACT), "Contact": SWEEP_CONTACT})
    c.key_poses_ms = [0.0, 150.0, 320.0, windup(SWEEP_CONTACT), SWEEP_CONTACT, 850.0, 1040.0, ATTACK_MS]
    c.manifest_extra = {"usage": "[W2] 旗扫 Banner Sweep (chapter1.md §11 P1: cone 120°, r 2.6, windup 700, the crimson "
                                 "cone decal fills to full at Contact)", "skill": "banner_sweep"}
    return c


def cast01(A: ValdrasAnims):
    """Cast01 1500 ms, 誓印 Oath-Brand slam (circle r 1.8 at the hero, windup 900): he wrenches the sword up and
    lifts it high in both fists, point down (WindupEnd 558), then drives it into the ground before him with his whole
    weight, down on bent knees (Contact 900: the brand); holds, wrenches it out and stands to attention."""
    R0 = A.ready()
    J = A.J
    lift = R0.but(lean=-2.0, head_pitch=-16.0, hand_r=V(-0.10, 0.30, 1.62), elbow_r=V(-0.9, 0.3, -0.2),
                  **aim(-0.10, 0.40, -0.90), two=1.0, two_y=VD.SW_LEFT_Y, flow=0.3)
    high = R0.but(pelvis=R0.pelvis + V(0, -0.02, 0.02), lean=-10.0, head_pitch=-24.0, hand_r=V(-0.08, 0.30, 2.18),
                  elbow_r=V(-0.9, 0.2, 0.1), **aim(-0.04, 0.42, -0.90), elbow_l=V(0.9, 0.2, 0.1), two=1.0,
                  two_y=VD.SW_LEFT_Y, clav_l=(14.0, 0.0), clav_r=(14.0, 0.0), flow=0.35)
    held = high.but(hand_r=V(-0.08, 0.28, 2.22), lean=-11.0)
    slam = R0.but(pelvis=R0.pelvis + V(0, 0.10, -0.24), lean=28.0, hip_pitch=6.0, head_pitch=-24.0,
                  foot_l=J["ankle_l"] + V(0.05, 0.16, 0.0), foot_r=J["ankle_r"] + V(-0.05, -0.08, 0.0),
                  knee_l=V(0.3, 1, 0), knee_r=V(-0.3, 1, 0), hand_r=V(-0.04, 0.62, 0.92), elbow_r=V(-0.9, 0.0, -0.4),
                  **aim(0.0, 0.18, -1.0), elbow_l=V(0.9, 0.0, -0.4), two=1.0, two_y=VD.SW_LEFT_Y, flow=0.7)
    press = slam.but(pelvis=slam.pelvis + V(0, 0.0, -0.02), lean=30.0, hand_r=V(-0.04, 0.62, 0.89))
    wrench = R0.but(lean=8.0, head_pitch=-14.0, hand_r=V(-0.06, 0.48, 1.32), **aim(0.0, 0.30, -0.95), two=1.0,
                    two_y=VD.SW_LEFT_Y)
    keys = [K(0.0, R0), K(300 / CAST_MS, lift, "out"), K(windup(BRAND_CONTACT) / CAST_MS, high, "smooth"),
            K(740 / CAST_MS, held, "smooth"), K(BRAND_CONTACT / CAST_MS, slam, "in"), K(1120 / CAST_MS, press, "out"),
            K(1300 / CAST_MS, wrench, "smooth"), K(1.0, R0, "smooth")]
    c = A.clip("Cast01", CAST_MS, keys, CAST_MS, post=attack_post(A),
               notifies={"WindupEnd": windup(BRAND_CONTACT), "Contact": BRAND_CONTACT, "FX_Brand": BRAND_CONTACT})
    c.key_poses_ms = [0.0, 300.0, windup(BRAND_CONTACT), 740.0, BRAND_CONTACT, 1120.0, 1300.0, CAST_MS]
    c.manifest_extra = {"usage": "[W2] 誓印 Oath-Brand Slam (chapter1.md §11 P1: circle r 1.8 at the hero's position, "
                                 "windup 900; the burning brand decal is core VFX at FX_Brand)", "skill": "oath_brand"}
    return c


def roar(A: ValdrasAnims):
    """Roar 1200 ms (phase change; the boss intro under the title): he draws himself up, then throws the helm back
    and the left arm out wide, fist clenched — the standard on his back **flares**: the banner whips out level behind
    him and snaps (FX_Roar 450: P3 lights its breach fire there); the sword stays planted."""
    R0 = A.ready()
    J = A.J
    draw = R0.but(lean=6.0, head_pitch=4.0, clav_l=(10.0, 0.0), clav_r=(10.0, 0.0), flow=0.3, flare=0.15)
    # review round 2: the phase change reads in silhouette without the VFX — the helm thrown back (22° more than
    # READY's chin-up, on a back arched 18°: ~40° off level), the chest out (hips forward, the back arched), the stance widened, the free arm flung out
    # and up
    roar_ = R0.but(pelvis=R0.pelvis + V(0, 0.03, -0.03), lean=-18.0, hip_pitch=-5.0, head_pitch=-31.0,
                   head_yaw=6.0, foot_l=J["ankle_l"] + V(0.11, 0.04, 0.0), foot_r=J["ankle_r"] + V(-0.11, -0.02, 0.0),
                   foot_l_rot=(0.0, 0.0, 14.0), foot_r_rot=(0.0, 0.0, 14.0), knee_l=V(0.35, 1, 0),
                   knee_r=V(-0.35, 1, 0), hand_l=V(0.68, 0.16, 1.80), elbow_l=V(0.6, -0.3, -0.6), two=0.0,
                   clav_l=(22.0, 6.0), clav_r=(4.0, 0.0), flow=0.8, flare=1.0)
    shake = roar_.but(head_pitch=-33.0, head_yaw=-4.0, hand_l=V(0.70, 0.18, 1.84))
    keys = [K(0.0, R0), K(240 / ROAR_MS, draw, "out"), K(ROAR_FX / ROAR_MS, roar_, "out"),
            K(640 / ROAR_MS, shake, "smooth"), K(860 / ROAR_MS, roar_.but(head_pitch=-29.0), "smooth"),
            K(1.0, R0, "smooth")]

    def layer(bp, t):
        bp = A.waver(ROAR_MS)(bp, t)
        if ROAR_FX <= t <= 950.0:
            k = math.sin(math.pi * (t - ROAR_FX) / (950.0 - ROAR_FX))
            bp = bp.but(head_roll=bp.head_roll + 3.0 * k * math.sin(t * 0.09),
                        pelvis=bp.pelvis + V(0, 0, 0.003 * k * math.sin(t * 0.13)))
        return bp
    c = A.clip("Roar", ROAR_MS, keys, ROAR_MS, notifies={"FX_Roar": ROAR_FX}, layer=layer, post=A.plant_sword)
    c.key_poses_ms = [0.0, 240.0, ROAR_FX, 640.0, 860.0, ROAR_MS]
    c.manifest_extra = {"usage": "boss intro cs_boss_valdras (under the title) and each phase change (chapter1.md §11: "
                                 "P2 Roll Call, P3 No Retreat — the standard ignites at FX_Roar); still takes damage"}
    return c


def lunge(A: ValdrasAnims):
    """Lunge 1300 ms, 冲锋 (P3: line 5 × 1.2, windup 800): he sinks into a long stance with the oath-sword drawn back
    at the right hip, point at the hero (WindupEnd 496), then drives forward in a lunging thrust, arms locked out
    (Contact 800; the runtime carries him down the line), and recovers to attention."""
    R0 = A.ready()
    J = A.J
    crouch = R0.but(pelvis=R0.pelvis + V(0.0, -0.06, -0.16), lean=16.0, twist=-26.0, hip_yaw=-10.0, head_pitch=-22.0,
                    head_yaw=20.0, foot_l=J["ankle_l"] + V(0.06, 0.26, 0.0), foot_r=J["ankle_r"] + V(-0.06, -0.30, 0.0),
                    knee_l=V(0.3, 1, 0), hand_r=V(-0.42, -0.02, 1.12), elbow_r=V(-0.6, 0.6, -0.5),
                    **aim(-0.28, 1.0, 0.08), elbow_l=V(0.6, 0.4, -0.6), two=1.0, two_y=VD.SW_LEFT_Y, flow=0.4)
    hold = crouch.but(pelvis=crouch.pelvis + V(0, -0.02, -0.01), hand_r=V(-0.43, -0.06, 1.12))
    thrust = R0.but(pelvis=R0.pelvis + V(0.0, 0.32, -0.20), lean=24.0, hip_pitch=6.0, twist=6.0, head_pitch=-22.0,
                    foot_l=J["ankle_l"] + V(0.06, 0.62, 0.0), foot_r=J["ankle_r"] + V(-0.06, -0.38, 0.0),
                    foot_r_rot=(28.0, 0.0, 6.0), knee_l=V(0.3, 1, 0), hand_r=V(-0.06, 1.06, 1.30),
                    elbow_r=V(-0.8, 0.0, -0.6), **aim(0.0, 1.0, -0.04), elbow_l=V(0.8, 0.0, -0.6), two=1.0,
                    two_y=VD.SW_LEFT_Y, flow=0.9)
    follow = thrust.but(hand_r=V(-0.06, 1.10, 1.24), **aim(0.0, 1.0, -0.12), lean=26.0)
    back = R0.but(pelvis=R0.pelvis + V(0.0, 0.14, -0.10), lean=12.0, foot_l=J["ankle_l"] + V(0.06, 0.30, 0.0),
                  hand_r=V(-0.10, 0.62, 1.30), **aim(0.0, 0.70, -0.70), two=1.0, two_y=VD.SW_LEFT_Y)
    keys = [K(0.0, R0), K(350 / LUNGE_MS, crouch, "out"), K(windup(LUNGE_CONTACT) / LUNGE_MS, hold, "smooth"),
            K(LUNGE_CONTACT / LUNGE_MS, thrust, "in"), K(900 / LUNGE_MS, follow, "out"),
            K(1100 / LUNGE_MS, back, "smooth"), K(1.0, R0, "smooth")]
    c = A.clip("Lunge", LUNGE_MS, keys, LUNGE_MS, post=attack_post(A),
               notifies={"WindupEnd": windup(LUNGE_CONTACT), "Contact": LUNGE_CONTACT})
    c.key_poses_ms = [0.0, 350.0, windup(LUNGE_CONTACT), LUNGE_CONTACT, 900.0, 1100.0, LUNGE_MS]
    c.manifest_extra = {"usage": "[W2] 冲锋 Lunge (chapter1.md §11 P3: line 5 × 1.2, windup 800; in place — the core "
                                 "moves him down the line)", "skill": "marshal_lunge"}
    return c


def clips(A: ValdrasAnims) -> list:
    return [A.idle(), A.walk(), attack01(A), attack02(A), cast01(A), roar(A), lunge(A), A.hurt(), A.death(),
            A.rise(), A.stun()]


def portrait_pose(A: ValdrasAnims) -> BP:
    """Portrait pose (``T_UI_Portrait_Valdras``, the boss intro): at attention, the helm turned a little toward the
    viewer and lifted, the slit burning; the hilt of the planted oath-sword and his fists low in the frame."""
    R0 = A.ready()
    # review round 2: the left hand off the pommel (it filled the bust's lower third as a faceted lump) — down on
    # the grip by the melted cross, below the frame
    return A.planted(R0.but(head_pitch=-12.0, head_yaw=-12.0, twist=-4.0, flow=0.3, two_y=-0.11))

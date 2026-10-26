"""赤誓狂信徒 Red-Oath Zealot — ``SK_Mon_OathZealot`` + ``SM_MonWeapon_Flail`` (dark remake A3; monsters.md §2.3).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/oath_zealot.py [--quick] [--anims [Clip …]] [--no-previews] [--no-verify]
        --frenzy-read [--scratch]   (Wave 2: the oath-scar at the frenzy's emissive, measured → game_w2_frenzy.png)

The Living's frenzy melee (L6, speed 52 → 1.56 m/s, Contact ≈ 420 ms, bleed on hit). Art brief: a hooded red robe
over mail (``zealot.robe`` #7E1416 — the family marker: crimson cloth, no glow), rope belts, bare forearms with the
**oath-scar** (a fist crushing a flame) burned in, faint crimson emissive ``zealot.scar`` #C0283A; a flail; skin
#B88A6E. Built with the Living kit (``living.py`` on the townsfolk body, ``SKEL_Human``): 1.75 m, a deep hood whose
brim shades the eyes (the lower face shows: a hard mouth, hollow cheeks), a cowl and ragged capelet, the robe cut at
the knee in a ragged hem over a mail hem, sleeves torn off at the elbow.

Clips (``ZealotAnims`` on ``living_clips.LivingAnims``): Idle 1200 (the flail swinging low), Walk 900 @ 1.56 m/s
(a stalking lean), Attack01 750 (an overhead flail blow: WindupEnd 260, Contact 420), Hurt 300, Death 900 (folds
forward, face down, the flail flung flat), Stun 600. [W2] frenzy at ≤ 50 % life (the oath-scar's emissive is a
material parameter: the runtime raises it); Wave 2 (2H, ``clip_patch.py``): **Roar** 1200 (the frenzy's enter action:
he gathers over the scarred fist, rears up shouting the oath with the fist over the hood and the flail whirled
overhead, ``FX_Frenzy`` 480 = the scar flare, manifest ``fx.frenzy``). ``fx_overhead`` stays the kit's (crown + 25 cm:
nameplates, stun stars, the death mark and the W1 crown ride it); the elite sigil rides its own ``fx_elite``
(``elite_sigil.py --solve-elite`` → ``fx_elite_w2.json``, written by ``mship.ship``).
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "npcs", HERE.parent / "march", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Quaternion, Vector  # noqa: E402

from kit import anim, mesh as M, outline, rig as R, scene, shading  # noqa: E402
from kit.paths import scratch_root  # noqa: E402
from kit.palette import Palette  # noqa: E402
from kit.rig import Bind  # noqa: E402

import common as C  # noqa: E402
from common import BP, K, V  # noqa: E402
import folk as F  # noqa: E402
import living as LV  # noqa: E402
from living_clips import LivingAnims  # noqa: E402

ASSET = "SK_Mon_OathZealot"
TOKEN = "Mon_OathZealot"
FLAIL = "SM_MonWeapon_Flail"
PREVIEW_DIR = "mon_oath_zealot"
GAME_IDS = ("monster_oath_zealot",)
HEIGHT = 1.75
REF_SPEED = 52 * 0.03            # monsters.md speed 52 → tiles/s (1 tile = 1 m)
MARKER = ("zealot.marker",)

ALIAS = dict(
    skin="living.skin", skin_shade="living.skin_shade", mouth="living.mouth", eye="living.eye_dark",
    brow="living.hair", hair="living.hair",
    top="zealot.robe", top_shade="zealot.robe_dark", sleeve="zealot.robe", trim="zealot.robe_dark",
    hood="zealot.robe", hood_line="zealot.robe_dark", marker="zealot.marker",
    legs="unburied.leather_dark", boots="unburied.leather", boot_line="unburied.leather_dark",
    belt="zealot.rope", rope="zealot.rope", gold="unburied.helm_dent", leather="unburied.leather",
    leather_dark="unburied.leather_dark", mail="unburied.mail", mail_line="unburied.mail_line", scar="zealot.scar",
    scar_edge="living.mouth", chain="flail.chain", iron="flail.iron",
    hem="unburied.sigil",
)
BODY = F.Build(thigh=12.5, shin=12, upper_arm=9.5, fore_arm=9, torso=18.0, neck=7.0, ankle=2.2, sh=5.6, hip=2.4,
               girth=1.0, belly=0.0, height=HEIGHT)
LOOK = LV.LivingLook(
    skin="#B88A6E", body=BODY, height=HEIGHT, head_k=0.56, top=("#7E1416", "coat", None), palette=ALIAS,
    hood=True, hood_brim=2.95, hood_side=58.0, hood_peak=1.1, cowl_folds=True, hood_seam=True, coif=True, limb_r=0.74, sleeve="elbow", scar=True,
    hem_k=-0.50, hem_w=(5.6, 5.0), hip_w=5.0, hem_dark=0.16, rag_hem=0.06, rope_belt=True, brow_scowl=1.4,
)

FLAIL_ALIAS = dict(haft="flail.haft", iron="flail.iron", chain="flail.chain", grip="unburied.leather",
                   grip_line="unburied.leather_dark")
HAFT_TOP = 0.40                 # m along +Y from the grip
CHAIN_DROOP = 38.0              # deg the chain hangs off the haft axis toward item −Z
CHAIN_LEN = 0.13
BALL_R = 0.042


def chain_dir() -> Vector:
    a = math.radians(CHAIN_DROOP)
    return Vector((0.0, math.cos(a), -math.sin(a)))


PIVOT = Vector((0.0, HAFT_TOP + 0.015, 0.0))        # the chain's eye at the haft top (weapon_r space)
CHAIN_BONE = "flail_01"
CHAIN_REACH = CHAIN_LEN + BALL_R * 0.9                 # pivot → ball centre


def ball_center() -> Vector:
    """Ball centre in weapon_r space at the rest droop (the bind pose of ``flail_01``)."""
    return PIVOT + chain_dir() * CHAIN_REACH


# ── flail ──────────────────────────────────────────────────────────────────────────────────────────────
# art review r1 (item 9): the r0 flail was one rigid SM — through the overhead windup the ball stood straight up in
# line with the haft (a mace). The haft stays ``SM_MonWeapon_Flail`` on ``weapon_r``; the chain and the spiked ball
# are skinned into the zealot's mesh on their own bone ``flail_01`` (child of ``weapon_r`` at the haft's eye), aimed
# every frame by a pendulum on the posed haft tip (``ZealotAnims.flail_chain``): the ball trails behind and below
# the haft in the windup and whips through at Contact.
def build_flail(pal: Palette) -> bpy.types.Object:
    """A footman's flail haft: a 48 cm ash haft (leather grip, iron pommel, collar and the chain's eye). Grip at the
    origin, haft along +Y (weapon_r). The chain and the ball are ``flail_head`` parts of the body (``flail_01``)."""
    b = LV.MarchBuilder(FLAIL, pal, FLAIL_ALIAS)
    b.declare()
    rig = Bind.rigid("root")
    haft = M.tube([(0, -0.075, 0), (0, 0.10, 0), (0, 0.30, 0), (0, HAFT_TOP, 0)], [0.0145, 0.014, 0.0135, 0.013],
                  sides=8, dome0=0.006, dome1=0.0)
    hid = b.add(haft, "haft", rig, name="haft", hull_pad=True)
    pommel = M.ellipsoid((0.019, 0.016, 0.019), (0, -0.082, 0), sides=10, rings=6)
    b.add(pommel, "iron", rig, name="pommel", hull_pad=True)
    for k in range(6):                              # leather grip wrap (raised turns)
        y = -0.05 + k * 0.022
        ring = M.tube([(0, y - 0.008, 0), (0, y + 0.008, 0)], [0.0158] * 2, sides=8, dome0=0.0, dome1=0.0)
        b.add(ring, "grip" if k % 2 == 0 else "grip_line", rig, name="grip", outline=False, covered_by=hid)
    collar = M.tube([(0, HAFT_TOP - 0.05, 0), (0, HAFT_TOP - 0.01, 0), (0, HAFT_TOP + 0.005, 0)],
                    [0.0175, 0.0185, 0.012], sides=8, dome0=0.0, dome1=0.004)
    b.add(collar, "iron", rig, name="collar", hull_pad=True)
    pts = [PIVOT + Vector((0.0, 0.0105 * math.cos(2 * math.pi * j / 8) - 0.004, 0.0105 * math.sin(2 * math.pi * j / 8)))
           for j in range(8)]
    eye = M.sweep(pts, M.superellipse(1, 1, 5, 2.0), scales=[(0.0035, 0.0035)] * 8, closed_path=True,
                  up=(1, 0, 0), cap0=None, cap1=None)
    b.add(eye, "chain", rig, name="eye", hull=M.ellipsoid((0.006, 0.016, 0.016), tuple(PIVOT - Vector((0, 0.004, 0))),
                                                          sides=8, rings=4), hull_pad=True)
    obj = b.build(FLAIL)
    mt = shading.toon_material(pal)
    outline.bake_hull(obj, outline.WIDTH_CM[LV.OUTLINE_CLASS] / 100.0, mt, shading.outline_material(pal))
    for m in outline.hull_color_report(obj, pal, LV.OUTLINE_CLASS):
        print(f"[ink] WARNING {FLAIL}: {m}")
    obj["af_outline_class"] = LV.OUTLINE_CLASS
    return obj


def add_chain_bone(rig) -> None:
    """``flail_01`` under ``weapon_r``: head at the haft's eye, along the rest droop, X = the haft's X."""
    arm = rig.obj
    W = arm.data.bones["weapon_r"].matrix_local.copy()
    head = W @ PIVOT
    d = (W.to_3x3() @ chain_dir()).normalized()
    scene.select_only([arm])
    bpy.ops.object.mode_set(mode="EDIT")
    eb = arm.data.edit_bones
    bn = eb.new(CHAIN_BONE)
    bn.head, bn.tail = head, head + d * CHAIN_REACH
    bn.align_roll(W.to_3x3() @ Vector((0.0, 0.0, 1.0)) - d * (W.to_3x3() @ Vector((0.0, 0.0, 1.0))).dot(d))
    bn.parent = eb["weapon_r"]
    bn.use_deform = True
    bpy.ops.object.mode_set(mode="OBJECT")


FLAIL_HEAD = ("flail_link", "flail_ball", "flail_band", "flail_spike")


def add_flail_head(f, b, ids) -> None:
    """The chain (four links) and the spiked ball (r 4.2 cm, ten spikes), built along +Y from the eye and placed on
    ``flail_01``'s rest frame, skinned rigidly to it (the same regions as the r0 SM: ``flail.chain`` / ``flail.iron``)."""
    rig = f.rig
    B = rig.obj.data.bones[CHAIN_BONE].matrix_local.copy()
    bind = Bind.rigid(CHAIN_BONE)
    d = Vector((0.0, 1.0, 0.0))
    side, perp = Vector((1.0, 0.0, 0.0)), Vector((0.0, 0.0, 1.0))
    nlink = 4
    step = CHAIN_LEN / nlink
    proxy = M.capsule((0.0, 0.0, 0.0), (0.0, CHAIN_LEN, 0.0), 0.0105, 0.0105, sides=8, cap_rings=1, mid=3)
    cid = None
    for i in range(nlink):
        c = d * (step * (i + 0.5))
        ax = side if i % 2 == 0 else perp
        pts = [c + d * (math.cos(2 * math.pi * j / 6) * step * 0.62) + ax * (math.sin(2 * math.pi * j / 6) * 0.0085)
               for j in range(6)]
        link = M.sweep(pts, M.superellipse(1, 1, 4, 2.0), scales=[(0.0034, 0.0034)] * 6, closed_path=True,
                       up=tuple(ax.cross(d)), cap0=None, cap1=None).transform(B)
        if i == 0:
            cid = b.add(link, "chain", bind, name="flail_link", hull=proxy.transform(B), hull_pad=True)
        else:
            b.add(link, "chain", bind, name="flail_link", outline=False, covered_by=cid)
    bc = d * CHAIN_REACH
    ball = M.ellipsoid((BALL_R,) * 3, tuple(bc), sides=10 if not f.lod else 8, rings=7 if not f.lod else 6)
    bid = b.add(ball.transform(B), "iron", bind, name="flail_ball", hull_pad=True)
    band = M.tube([bc - d * 0.006, bc + d * 0.006], [BALL_R * 1.04] * 2, sides=12, dome0=0.0, dome1=0.0)
    b.add(band.transform(B), "chain", bind, name="flail_band", outline=False, covered_by=bid)
    rng = scene.rng(FLAIL, "spikes")
    golden = math.pi * (3.0 - math.sqrt(5.0))
    for i in range(10):
        y = 1.0 - 2.0 * (i + 0.5) / 10
        r = math.sqrt(max(0.0, 1.0 - y * y))
        th = golden * i + rng.uniform(-0.15, 0.15)
        dv = (d * y + side * (math.cos(th) * r) + perp * (math.sin(th) * r)).normalized()
        base = bc + dv * (BALL_R * 0.82)
        tip = bc + dv * (BALL_R + 0.024)
        spike = M.tube([tuple(base), tuple(tip)], [0.0085, 0.0012], sides=5, dome0=0.0, dome1=0.0)
        b.add(spike.transform(B), "chain", bind, name="flail_spike", hull_pad=True)


def flail_grip(co) -> bool:
    """Flail faces inside the fist (the grip wrap), item-local."""
    return -0.06 < co.y < 0.07 and math.hypot(co.x, co.z) < 0.03


# ── clips ──────────────────────────────────────────────────────────────────────────────────────────────
class ZealotAnims(LivingAnims):
    REF_SPEED = REF_SPEED
    SKIRT_CLEAR = 0.040          # leg → robe clearance of the cloth envelope (NPCs 0.022: the robe's ragged hem)
    COAT_FLOOR_MARGIN = 0.014    # the r1 robe reaches mid-shin: its ragged hem dips between the sampled points
    WALK_MS = 900.0
    WALK = dict(duty=0.60, lift=0.062, bob=0.016, lean=11.0, crouch=-0.035, stance=0.105, hip_pitch=3.0)
    IDLE_MS = 1200.0
    ATTACK_MS = 750.0
    CONTACT_MS = 420.0

    def make_ready(self) -> BP:
        """Frenzied: crouched, leaning in, the head up at the enemy (the hood's brim shades the eyes), the left fist
        clenched before the chest (the oath-scar toward the camera), the flail low at the right side, haft forward
        and down, the ball hanging off it."""
        J = self.J
        bp = BP(
            pelvis=V(0, 0.03, -0.05), hip_pitch=4.0, hip_yaw=-8.0, hip_roll=0.0, lean=12.0, twist=6.0,
            head_pitch=-12.0, head_yaw=-4.0,
            foot_l=J["ankle_l"] + V(0.03, 0.10, 0), foot_r=J["ankle_r"] + V(-0.05, -0.11, 0),
            foot_l_rot=(0, 0, 10), foot_r_rot=(0, 0, 24), knee_l=V(0.3, 1, 0), knee_r=V(-0.35, 1, 0),
            hand_r=self.side_pt("r", 0.17, 0.20, -0.46), elbow_r=V(-0.8, -0.2, -0.5),
            **self.wkw(120.0, lat=-0.30),
            hand_l=self.side_pt("l", 0.02, 0.32, -0.26), elbow_l=V(0.8, -0.3, -0.6), wrist_l=(-20.0, 0.0, -30.0),
            flow=0.12,
        )
        return bp

    def idle_layer(self, bp: BP, t: float, ph: float) -> BP:
        # the flail sways like a pendulum in the loose wrist (the haft rocks ± 12°)
        return bp.but(**self.wkw(120.0 + 12.0 * math.sin(ph * 2.0), lat=-0.30 + 0.08 * math.cos(ph * 2.0)))

    def carry(self, bp: BP, sw: float, ph: float) -> BP:
        R0 = self.ready()
        return bp.but(hand_r=self.side_pt("r", 0.15, 0.10 + 0.08 * sw, -0.50 + 0.02 * abs(sw)),
                      elbow_r=V(-0.8, -0.2, -0.5), **self.wkw(135.0 - 18.0 * sw, lat=-0.35),
                      hand_l=self.side_pt("l", 0.06, 0.20 - 0.14 * sw, -0.34 + 0.04 * max(0.0, -sw)),
                      elbow_l=R0.elbow_l, wrist_l=R0.wrist_l, head_pitch=-10.0)

    def attack_keys(self) -> list:
        R0 = self.ready()
        J = self.J
        wind = R0.but(pelvis=V(0, -0.01, -0.04), lean=2.0, twist=-24.0, hip_yaw=-16.0, head_pitch=-14.0,
                      head_yaw=8.0, hand_r=self.side_pt("r", 0.12, -0.10, 0.30), elbow_r=V(-0.9, 0.1, 0.4),
                      **self.wkw(-28.0, lat=-0.15),
                      hand_l=self.side_pt("l", 0.16, 0.36, -0.12), elbow_l=V(0.8, -0.2, -0.5), clav_r=(14.0, -6.0),
                      flow=0.25)
        mid = R0.but(pelvis=V(0, 0.03, -0.05), lean=11.0, twist=-4.0, hip_yaw=-6.0, head_pitch=-16.0,
                     hand_r=self.side_pt("r", 0.04, 0.18, 0.36), elbow_r=V(-0.9, -0.2, 0.3), **self.wkw(30.0, lat=0.0),
                     hand_l=self.side_pt("l", 0.12, 0.20, -0.26), clav_r=(12.0, 4.0), flow=0.35)
        hit = R0.but(pelvis=V(0, 0.07, -0.08), lean=24.0, hip_pitch=7.0, twist=16.0, hip_yaw=4.0, head_pitch=-26.0,
                     head_yaw=-6.0, foot_l=J["ankle_l"] + V(0.03, 0.17, 0), foot_r=J["ankle_r"] + V(-0.05, -0.12, 0),
                     hand_r=self.side_pt("r", 0.00, 0.50, -0.30), elbow_r=V(-0.7, -0.4, -0.4),
                     **self.wkw(108.0, lat=0.12),
                     hand_l=self.side_pt("l", 0.12, -0.04, -0.44), elbow_l=V(0.7, 0.3, -0.5), clav_r=(-4.0, 10.0),
                     flow=0.5)
        follow = hit.but(lean=26.0, twist=20.0, hand_r=self.side_pt("r", -0.02, 0.44, -0.44),
                         **self.wkw(140.0, lat=0.02), flow=0.4)
        return [(230.0, wind, "out"), (340.0, mid, "in"), (420.0, hit, "linear"), (520.0, follow, "out")]

    # ── Roar [W2] (Wave 2 art, 2H): the frenzy's enter action (implementation-plan.md K4: the zealot's one-row phase
    # table, monsters.md 2.3 "the robe whips, the oath-scar flares, a shouted chant") ─────────────────────────────
    ROAR_MS = 1200.0
    ROAR_SHOUT = 480.0               # FX_Frenzy: the chant breaks out, the scar flares (fx.frenzy)

    def roar(self):
        """``Roar`` 1200 ms: he gathers (crouched over the left fist pulled to the chest, the scar turned up), then rears
        out of the crouch shouting the oath — chin up, chest out, the scarred left fist punched up over the hood, the
        flail hauled up and round overhead so the chain whips — holds the chant (a shaking head, the robe streaming)
        and drops back into the frenzied READY (``FX_Frenzy`` 480; the core enters the phase at the start)."""
        R0 = self.ready()
        # art review r1: the r0 brace (pelvis 8 cm down, lean 22, hip_pitch 6, flow .25) flared the robe into a flat,
        # hard-cornered wedge for ~200 ms (F6-F13, the frenzy's announcement); a shallower crouch keeps the knees from
        # shoving the hem out, so the skirt stays a cone and the gather is carried by the torso over the fist
        brace = R0.but(pelvis=V(0, 0.0, -0.03), lean=14.0, hip_pitch=0.0, twist=10.0, hip_yaw=-4.0, head_pitch=-2.0,
                       head_yaw=-2.0, hand_l=self.side_pt("l", 0.0, 0.24, -0.22), elbow_l=V(0.8, -0.3, -0.6),
                       wrist_l=(-40.0, 0.0, -40.0), hand_r=self.side_pt("r", 0.18, 0.04, -0.44),
                       elbow_r=V(-0.8, 0.0, -0.5), **self.wkw(150.0, lat=-0.42), flow=0.12)
        shout = R0.but(pelvis=V(0, 0.02, -0.02), lean=-6.0, hip_pitch=-2.0, twist=-6.0, hip_yaw=-6.0,
                       head_pitch=-30.0, head_yaw=-4.0, hand_l=self.side_pt("l", 0.08, 0.10, 0.44),
                       elbow_l=V(0.9, 0.0, -0.4), wrist_l=(0.0, 0.0, -50.0),
                       hand_r=self.side_pt("r", 0.24, 0.02, 0.28), elbow_r=V(-0.9, 0.0, -0.4),
                       **self.wkw(-24.0, lat=-0.45), clav_l=(18.0, 0.0), clav_r=(14.0, 0.0), flow=0.75)
        whirl = shout.but(lean=-3.0, twist=4.0, head_pitch=-26.0, hand_r=self.side_pt("r", 0.12, 0.22, 0.34),
                          **self.wkw(28.0, lat=0.10), flow=0.65)
        L, Cm = self.ROAR_MS, self.ROAR_SHOUT
        keys = [K(0.0, R0), K(220.0 / L, brace, "out"), K(Cm / L, shout, "out"), K(700.0 / L, whirl, "smooth"),
                K(880.0 / L, shout.but(flow=0.5), "smooth"), K(1.0, R0, "smooth")]

        def layer(bp: BP, t: float) -> BP:
            if Cm <= t <= 900.0:                       # the chant shakes his head and fist
                k = math.sin(math.pi * (t - Cm) / (900.0 - Cm))
                ph = (t - Cm) * 2.0 * math.pi / 95.0
                bp = bp.but(head_roll=bp.head_roll + 4.0 * k * math.sin(ph),
                            head_yaw=bp.head_yaw + 3.0 * k * math.sin(ph * 0.5 + 0.7),
                            hand_l=bp.hand_l + V(0.0, 0.0, 0.012 * k * math.sin(ph + 0.4)))
            return bp
        wave = self.waver(L)
        c = self.clip("Roar", L, keys, L, notifies={"FX_Frenzy": Cm}, layer=lambda bp, t: layer(wave(bp, t), t),
                      key_poses=[0.0, 220.0, Cm, 700.0, 880.0, L])
        c.manifest_extra = {"usage": "[W2] the frenzy (implementation-plan.md K4: the zealot's one-row phase table at "
                                     "<= 50 % life, enterAction Roar; still takes damage): the shouted chant — he rears "
                                     "up, the scarred fist over the hood, the flail whirled overhead; FX_Frenzy 480 = "
                                     "the chant's breath and the scar flare (fx.frenzy: the scar emissive rises to "
                                     "frenzyEmissive and stays for the phase; A12 notify.FX_Frenzy + the "
                                     "monster.oath_zealot.frenzy loop)"}
        return c

    def death_items(self) -> dict:
        up = Vector((0.0, 1.0, 0.0))
        return {"hit": self.wkw(70.0, lat=-0.4), "kneel": self.wkw(150.0, lat=-0.55),
                "topple": {"wpn": Vector((-0.75, 0.0, 0.66)), "wpn_up": up},
                "down": {"wpn": Vector((-0.80, 0.0, 0.60)), "wpn_up": up}}

    def stun_items(self, bp: BP, ph: float) -> dict:
        # the flail hangs out from the limp hand, the ball clear of the robe
        return self.wkw(148.0 + 6.0 * math.sin(ph), lat=-0.48)

    # ── the flail's chain (art review r1, item 9): a pendulum on the posed haft tip ────────────────────
    SIM_MS = 320.0               # history simulated before every frame (the swing's memory)
    SIM_STEP = 1000.0 / 60.0
    SIM_DAMP = 0.90              # velocity kept per step (air, the links' friction: the ball trails, it does not
                                 # fling out along the haft on the upswing)
    SIM_G = 14.0                 # a heavy head (a little over g: it hangs and trails rather than floats)
    MIN_BEND = 55.0              # haft raised: the chain keeps at least this bend off the haft line, falling to the
                                 # gravity side (the ball trails behind / below the tip, never stands on it — r1 #9)
    BEND_FULL_Z = 0.40           # haft-up component (world z of the haft axis) at which the full bend applies
    BALL_CLEAR = 0.075           # ball centre ↔ body proxy clearance (ball + spikes)
    FLOOR_Z = 0.07               # face down the ball rests on the floor (ball + spike under the centre)

    def _tip_mats(self, bp: BP):
        pose = C.to_pose(self.rig, bp, None, grips=self.GRIPS)
        return anim.evaluate(pose, return_matrices=True)

    @staticmethod
    def _spin_fn(bp: BP):
        """World ← pre-spin map of a body pose (``common.apply_spin``: the cloth solvers run before the spin)."""
        if abs(bp.spin) < 1e-6:
            return lambda v: v.copy(), Quaternion()
        q = Quaternion(Vector(bp.spin_axis).normalized(), math.radians(bp.spin))
        Rm = q.to_matrix()
        pv = bp.pivot + Vector((0.0, 0.0, bp.lift))
        return (lambda v: pv + Rm @ (v - pv)), q

    def _sample(self, mats, bp: BP):
        f, _ = self._spin_fn(bp)
        return f(mats["weapon_r"] @ PIVOT), [(f(a), f(c), r0, r1) for a, c, r0, r1 in self._proxies(mats)]

    def _tip_at(self, t_ms: float):
        key = (getattr(self, "_flail_clip", None), round(t_ms, 2))
        memo = self.__dict__.setdefault("_tip_memo", {})
        if key not in memo:
            pb = C.ClothClock.sampler(t_ms)
            memo[key] = self._sample(self._tip_mats(pb), pb)
        return memo[key]

    def _proxies(self, mats) -> list:
        """Body proxies the ball stays out of (pre-spin armature space): head, chest, belly (spheres), thighs and
        shins (capsules: the robe follows them) and the robe (a cone on the pelvis, waist → flared hem)."""
        J = self.J
        out = []
        for bone, pt, r in (("head", J["head_center"], 0.13), ("spine_03", J["chest"], 0.19),
                            ("spine_01", self.J["pelvis"] + Vector((0, 0, 0.12)), 0.19)):
            m = mats[bone] @ self.bones[bone].matrix_local.inverted()
            out.append((m @ pt, m @ pt, r, r))
        for s_ in ("l", "r"):
            mh = mats[f"thigh_{s_}"] @ self.bones[f"thigh_{s_}"].matrix_local.inverted()
            mk = mats[f"calf_{s_}"] @ self.bones[f"calf_{s_}"].matrix_local.inverted()
            out.append((mh @ J[f"hip_{s_}"], mk @ J[f"knee_{s_}"], 0.15, 0.14))
            out.append((mk @ J[f"knee_{s_}"], mk @ J[f"ankle_{s_}"], 0.14, 0.12))
        F = self.F
        mp = mats["pelvis"] @ self.bones["pelvis"].matrix_local.inverted()
        r0 = max(F.skirt.at(F.skirt_z0)[:2]) + 0.02
        r1 = max(F.skirt.at(F.skirt_z1)[:2]) + 0.05
        cy0, cy1 = F.skirt.at(F.skirt_z0)[2], F.skirt.at(F.skirt_z1)[2]
        out.append((mp @ Vector((0.0, cy0, F.skirt_z0)), mp @ Vector((0.0, cy1, F.skirt_z1)), r0, r1))
        return out

    @staticmethod
    def _push(b: Vector, proxies, clear: float) -> Vector:
        for a, c, r0, r1 in proxies:
            ab = c - a
            t = 0.0 if ab.length_squared < 1e-9 else max(0.0, min(1.0, (b - a).dot(ab) / ab.length_squared))
            q = a + ab * t
            v = b - q
            need = r0 + (r1 - r0) * t + clear
            if v.length < need:
                v = v.normalized() if v.length > 1e-6 else Vector((0.0, -1.0, 0.0))
                b = q + v * need
        return b

    def _min_bend(self, d: Vector, a: Vector) -> Vector:
        """Keep the chain ``d`` at least ``MIN_BEND`` off the haft axis ``a`` while the haft points up (scaled by its
        up component, so a hanging or levelled haft — Idle, Walk, the Contact whip — is the free pendulum). Inside the
        cone the bend opens to the limit and its side turns toward gravity's (the ball hangs behind / below the tip);
        continuous at the cone's edge, so no frame pops."""
        if a.z <= 0.0:
            return d
        need = math.radians(self.MIN_BEND) * min(1.0, a.z / self.BEND_FULL_Z)
        th = a.angle(d) if d.length > 1e-6 else 0.0
        if th >= need:
            return d
        down = Vector((0.0, 0.0, -1.0))
        g = down - a * down.dot(a)
        g = g.normalized() if g.length > 1e-5 else Vector((0.0, -1.0, 0.0))
        dp = d - a * d.dot(a)
        k = (need - th) / max(need, 1e-6)
        if dp.length > 1e-5:
            w = dp.normalized().lerp(g, k)
            w = w - a * w.dot(a)
            w = w.normalized() if w.length > 1e-5 else g
        else:
            w = g
        return (a * math.cos(need) + w * math.sin(need)).normalized()

    def flail_chain(self, p, bp: BP) -> None:
        """Aim ``flail_01`` at the ball of a damped pendulum (Verlet, the chain an inextensible rod) driven by the
        haft tip over the last ``SIM_MS``, in world space (each sample through its own Death spin): the ball hangs
        under gravity, trails the haft through the windup and whips through when the haft stops at Contact; it keeps
        out of the head / chest / belly / legs / robe and off the floor."""
        if C.ClothClock.sampler is None:
            return
        mats = anim.evaluate(p, return_matrices=True)
        tip_now, prox_now = self._sample(mats, bp)
        _, q_now = self._spin_fn(bp)
        n = int(round(self.SIM_MS / self.SIM_STEP))
        t0 = C.ClothClock.t
        hist = []
        for k in range(n, 0, -1):
            t = t0 - k * self.SIM_STEP
            t = t % C.ClothClock.length if C.ClothClock.loop and C.ClothClock.length > 0 else max(0.0, t)
            hist.append(self._tip_at(t))
        hist.append((tip_now, prox_now))
        g = Vector((0.0, 0.0, -self.SIM_G)) * (self.SIM_STEP / 1000.0) ** 2
        b = hist[0][0] + Vector((0.0, 0.0, -CHAIN_REACH))
        b_prev = b.copy()
        for tip, prox in hist[1:]:
            nb = b + (b - b_prev) * self.SIM_DAMP + g
            for _ in range(2):
                nb = tip + (nb - tip).normalized() * CHAIN_REACH
                nb = self._push(nb, prox, self.BALL_CLEAR)
                nb.z = max(nb.z, self.FLOOR_Z)
            b_prev, b = b, nb
        d = (b - tip_now).normalized()
        f_now, _ = self._spin_fn(bp)
        d = self._min_bend(d, (tip_now - f_now(mats["weapon_r"].translation)).normalized())
        b = self._push(tip_now + d * CHAIN_REACH, prox_now, self.BALL_CLEAR)
        d = (b - tip_now).normalized()
        if tip_now.z + d.z * CHAIN_REACH < self.FLOOR_Z:
            # the rod's own end (not the clamped sample) on the floor: keep the heading, lift the slope
            dz = max(-1.0, min(1.0, (self.FLOOR_Z - tip_now.z) / CHAIN_REACH))
            hz = Vector((d.x, d.y, 0.0))
            hz = hz.normalized() if hz.length > 1e-6 else Vector((1.0, 0.0, 0.0))
            d = hz * math.sqrt(max(0.0, 1.0 - dz * dz)) + Vector((0.0, 0.0, dz))
        d = q_now.inverted() @ d                       # back to the pre-spin frame (apply_spin turns it again)
        x = mats["weapon_r"].to_3x3() @ Vector((1.0, 0.0, 0.0))
        up = x.cross(d)
        up = up.normalized() if up.length > 1e-4 else Vector((0.0, 0.0, 1.0))
        p.aim(CHAIN_BONE, d, up)

    def cloth(self, p, bp: BP) -> None:
        super().cloth(p, bp)
        self.flail_chain(p, bp)

    def monster_clips(self) -> list:
        clips = super().monster_clips() + [self.roar()]
        for c in clips:
            f0 = c.sampler

            def fn(t_ms, f0=f0, name=c.name):
                prev = getattr(self, "_flail_clip", None)
                self._flail_clip = name
                try:
                    return f0(t_ms)
                finally:
                    self._flail_clip = prev
            c.sampler = fn
        return clips


# ── build / ship ───────────────────────────────────────────────────────────────────────────────────────
def build_body(pal: Palette, folk: LV.LivingFolk, rig, lod: int = 0, lod_ratio=None):
    return folk.build(pal, rig, lod=lod, lod_ratio=lod_ratio, extra=add_flail_head)


def build_all():
    scene.reset()
    pal = Palette(LV.FAMILY)
    folk = LV.LivingFolk(LOOK, ASSET)
    rig = LV.build_rig(folk)
    add_chain_bone(rig)
    R.add_socket(rig, "fx_flail", CHAIN_BONE, rig.obj.data.bones["weapon_r"].matrix_local @ ball_center())
    body = build_body(pal, folk, rig)
    flail = build_flail(pal)
    return pal, folk, rig, body, flail


def anims(folk, rig, flail) -> ZealotAnims:
    return ZealotAnims(rig, folk, {flail.name: (flail, "weapon_r")})


def quick(out: Path) -> None:
    import mship
    pal, folk, rig, body, flail = build_all()
    A = anims(folk, rig, flail)
    mship.quick_review(ASSET, rig, [body], [(flail, "weapon_r")], A, out, HEIGHT, LV.OUTLINE_CLASS, px=LV.OUTLINE_PX)


def anim_quick(out: Path, names, facing: str = "se") -> None:
    import mship
    pal, folk, rig, body, flail = build_all()
    R.attach_to_bone(flail, rig, "weapon_r")
    A = anims(folk, rig, flail)
    cs = [c for c in A.monster_clips() if not names or c.name in names]
    mship.anim_review(rig, [body], [(flail, "weapon_r")], cs, TOKEN, out, HEIGHT, LV.OUTLINE_CLASS, facing=facing,
                      px=LV.OUTLINE_PX)


DISPLAY = {"zh-CN": "赤誓狂信徒", "en": "Red-Oath Zealot"}
GRIP_PARTS = ("hand_r", "thumb_r", "wrist_r", "forearm_r", "scar", "scar_edge", "scar_bar") + FLAIL_HEAD


def ship(previews: bool = True, verify: bool = True) -> int:
    import living_ship as LS
    import mship
    LS.march_stage()
    pal, folk, rig, body, flail = build_all()
    body1 = LS.build_lod1(folk, pal, rig, extra=add_flail_head)
    R.attach_to_bone(flail, rig, "weapon_r")
    A = anims(folk, rig, flail)
    cs = A.monster_clips()
    items = [mship.Item(flail, FLAIL, "weapon_r", "flail",
                        sockets=[{"name": "tip", "pos": tuple(PIVOT)}, {"name": "mid", "pos": (0.0, HAFT_TOP * 0.5, 0.0)}],
                        extra={"lengthCm": round((HAFT_TOP + 0.015 + 0.09) * 100.0, 1),
                               "note": "the haft only: the chain and the spiked ball are part of SK_Mon_OathZealot, "
                                       "skinned to the bone flail_01 (child of weapon_r at the haft's eye 'tip'; "
                                       "socket fx_flail = the ball), aimed every frame of every clip by a damped "
                                       "pendulum on the haft tip (art review r1: the r0 rigid chain stood the ball up "
                                       "in line with the haft through the windup, a mace)"},
                        grip=flail_grip)]
    groups = LS.qa_groups(body, GRIP_PARTS)
    names = LS.part_names(body)
    groups["flailHead"] = [n for n in names if n.startswith(FLAIL_HEAD)]
    groups["body_noflail"] = mship.parts_except(body, FLAIL_HEAD + ("hand_r", "thumb_r", "wrist_r"))
    ms = mship.MonsterShip(
        asset=ASSET, token=TOKEN, preview_dir=PREVIEW_DIR, pal=pal, A=A, body=body, items=items, clips=cs,
        outline_class=LV.OUTLINE_CLASS, game_ids=list(GAME_IDS), blob_radius=0.30, height=HEIGHT, lod1=body1,
        display=DISPLAY,
        fx={"telegraph": {**LS.TELEGRAPH, "untilMs": round(A.WINDUP_FRAC * A.CONTACT_MS, 1)},
            "attackTrail": {"color": LS.TRAIL, "alpha": 0.45, "sockets": ["tip", "mid"], "item": FLAIL,
                            "ballSocket": "fx_flail",
                            "note": "the haft's tip / mid; the ball (skeleton socket fx_flail on flail_01) whips "
                                    "through ~1 frame after the tip: a second ribbon from it reads the whirr"},
            "blobShadow": LS.DEATH_SHADOW,
            "oathScar": {"region": "zealot.scar", "color": "#C0283A", "emissive": 0.25, "frenzyEmissive": 0.7,
                         "note": "[W2] the scar flares on frenzy (<= 50 % life): raise the emissive of the scar "
                                 "swatch on the instance (crimson, never ember)"},
            "frenzy": {"clip": "Roar", "atMs": A.ROAR_SHOUT, "notify": "FX_Frenzy", "recipe": "notify.FX_Frenzy",
                       "loop": "monster.oath_zealot.frenzy", "scarEmissive": 0.7, "scarRampMs": 160,
                       "note": "[W2] Wave 2 art (2H): the frenzy's enter action (K4 phase enterAction Roar) plays "
                               "Roar; at FX_Frenzy (480) the scar's emissive ramps 0.25 -> 0.7 over scarRampMs and "
                               "stays until death, and the loop recipe runs on the zealot (origin, attach; Blow = its "
                               "facing) for the rest of the phase. The Living carry no light: the loop is breath and "
                               "dust, the only glow is the scar (crimson, never ember)",
                       "read": "art review r1 (oath_zealot.py --frenzy-read, Previews mon_oath_zealot/"
                               "game_w2_frenzy.png): at scarEmissive the scar is a 1 px block at 1080p on the darkest "
                               "march tile (2 px only in Walk sw), so it is a close-up detail; the persistent read of "
                               "a frenzied zealot is the loop's breath cloud before the hood (monster.oath_zealot."
                               "frenzy, a pant every ~0.3 s) plus the faster clips"}},
        qa_groups=groups,
        qa_pairs={"flailBody": ("item:" + FLAIL, "body_nf"), "ballBody": ("flailHead", "body_noflail")},
        qa_limits={"ground": -0.5, "flailBody": 0, "ballBody": 0, "coatLegs": 0},
        # face down (Death 34-42) the knees press the robe's front panel into the floor under the body, out of every
        # view (the NPCs' rule, npc_ship.QA_CLIP_LIMITS; deeper here: the zealot topples over a lunging stance)
        qa_clip_limits={"Death": {"coatLegs": 1e9, "coatLegsMm": 25.0}},
        outline_px=LV.OUTLINE_PX, outline_px_note=LV.OUTLINE_PX_NOTE,
        game_keys=[("Idle", 0, "se"), ("Walk", 13, "se"), ("Attack01", 14, "se"), ("Attack01", 25, "se"),
                   ("Attack01", 25, "sw"), ("Hurt", 4, "se"), ("Death", 30, "se"), ("Stun", 10, "se")],
        extra_previews=mship.chain_previews(
            LS.corpse_preview,
            mship.w2_sheet([("Roar", 13, "se"), ("Roar", 29, "se"), ("Roar", 41, "se"), ("Roar", 29, "sw"),
                            ("Roar", 29, "ne"), ("Roar", 41, "nw"), ("Roar", 51, "front"), ("Roar", 62, "se")])),
        back_clips=("Attack01", "Death", "Roar"),
        contact_keys=[("Attack01", 25, "se"), ("Attack01", 25, "sw"), ("Attack01", 25, "front"),
                      ("Attack01", 25, "ne")],
        close_keys=[("Attack01", 8, "se"), ("Attack01", 16, "se"), ("Attack01", 25, "se"), ("Attack01", 25, "side"),
                    ("Hurt", 4, "se"), ("Death", 22, "se"), ("Death", 36, "se"), ("Death", 54, "se"),
                    ("Stun", 10, "front"), ("Walk", 13, "side")],
        skeleton_bones_required=("root", "pelvis", "head", "weapon_r", "weapon_l", "skirt_f_02", "coat_th_l_01",
                                 "ik_foot_l", "ik_hand_gun"),
        lod_note=f"LOD0 budget {LS.LOD0_BUDGET} / LOD1 {LS.LOD1_BUDGET} toon tris (monsters.md 2.2, the Unburied kit "
                 "the Living share).",
        extra={**LS.remake_extra("flail_overhead", A.CONTACT_MS, REF_SPEED,
                                 "Clips in place; the core moves the actor (play rate = v / ref). Death folds forward "
                                 "face down (W2: the ash husk takes its pose). Stun (C5) loops. [W2] frenzy at <= 50 % "
                                 "life: Roar (the shouted chant, FX_Frenzy 480; fx.frenzy), then the same clips "
                                 "faster (attack speed x1.35, move x1.2)."),
               **LS.marker_extra(MARKER)},
    )
    LS.report_budget(ms)
    LS.install_coat_qa()
    res = mship.ship(ms, previews=previews, verify=verify)
    return mship.main_result(res)


# ── the frenzy's read at the game camera (Wave 2 art review r1) ─────────────────────────────────────────────────────
# After Roar nothing but the oath-scar persists on a frenzied zealot (the Living carry no light; the loop's breath is
# faint), so the scar at fx.frenzy's emissive has to read as a marker of its own: ``frenzy_read`` runs
# coherence/darkgate.py's lineup on the darkest march tile (W1 camera, native 1080p, bloom on) with the scar swatch at
# FRENZY_E and measures it with the darkgate marker metric (the largest solid k x k block of the visible swatch, the
# rest of the figure occluding; gate >= 2 px) — the calm scar (.25) beside it for comparison.
FRENZY_E = 0.7
CALM_E = 0.25
FRENZY_KEYS = [("Idle", 0, "se", FRENZY_E), ("Idle", 0, "sw", FRENZY_E), ("Walk", 13, "se", FRENZY_E),
               ("Walk", 13, "sw", FRENZY_E), ("Attack01", 14, "se", FRENZY_E), ("Attack01", 14, "sw", FRENZY_E),
               ("Idle", 0, "se", CALM_E), ("Idle", 0, "sw", CALM_E)]


def _set_swatch_emissive(family: str, index: int, e: float) -> None:
    """The emissive (params B) of one swatch in the loaded palette image, in memory only (nothing is saved)."""
    from kit import palette as P
    img = bpy.data.images.get(f"T_AF_Palette_{family}_P")
    if img is None:
        raise RuntimeError(f"palette image of {family} not loaded")
    w, h = img.size
    px = np.array(img.pixels[:], np.float32).reshape(h, w, 4)
    row, col = index // P.GRID, index % P.GRID
    y0, y1 = h - (row + 1) * P.SWATCH_PX, h - row * P.SWATCH_PX
    px[y0:y1, col * P.SWATCH_PX:(col + 1) * P.SWATCH_PX, 2] = round(e * 255.0) / 255.0
    img.pixels = px.ravel()
    img.update()


def frenzy_read(out_dir: Path, scratch: Path) -> list[dict]:
    """``game_w2_frenzy.png``: per ``FRENZY_KEYS`` cell the darkgate render (left) and its gate map (right, the
    visible scar cyan / red) of the zealot with the oath-scar at that emissive; returns the scar's marker records."""
    if str(HERE.parent / "coherence") not in sys.path:
        sys.path.insert(0, str(HERE.parent / "coherence"))
    import cohkit as K
    import darkgate as DG
    from kit import pngio, review
    ground = DG.Ground(None, "darkest", None)
    K.set_theme(ground.theme)
    reg = DG.registry(LV.FAMILY)
    scar = reg["zealot.scar"]
    spec0, imp0 = DG.marker_spec, K.import_character
    state = {"e": FRENZY_E}

    def spec(fig):
        sp = spec0(fig)
        return {**sp, "swatches": ["zealot.scar"], "indices": [scar], "source": "the oath-scar (frenzy read)"}

    def imp(*a, **kw):
        fig = imp0(*a, **kw)
        _set_swatch_emissive(LV.FAMILY, scar, state["e"])
        return fig
    DG.marker_spec, K.import_character = spec, imp
    cells, recs = [], []
    try:
        for clip, fr, fac, e in FRENZY_KEYS:
            state["e"] = e
            tag = f"frenzy_{clip}_{fr}_{fac}_{int(e * 100)}"
            rec = DG.shoot([(ASSET, None, "ZEALOT")], ground, fac, clip, fr, tag, scratch)[0]
            mk = rec["marker"]
            recs.append({"clip": clip, "frame": fr, "facing": fac, "scarEmissive": e, "blockPx": mk["blockPx"],
                         "pixels": mk["pixels"], "medianRgb": mk.get("medianRgb"), "deltaL": mk.get("deltaL"),
                         "ok": mk["blockPx"] >= DG.MARKER_PX})
            pair = []
            for f in (scratch / f"{tag}.png", scratch / f"{tag}_map.png"):
                im = np.ascontiguousarray(pngio.read_png(f)[:, :, :3])
                pair.append(im)
            h = max(x.shape[0] for x in pair)
            pair = [np.pad(x, ((0, h - x.shape[0]), (0, 0), (0, 0)), constant_values=24) for x in pair]
            cell = np.ascontiguousarray(np.concatenate(pair, axis=1))
            cell[:24] = 24
            review.draw_text(cell, 3, 3, f"{clip} F{fr} {fac.upper()}  SCAR E {e:.2f}" +
                             ("  FRENZY" if e >= FRENZY_E else "  CALM"))
            review.draw_text(cell, 3, 13, f"SCAR BLOCK {mk['blockPx']}PX ({mk['pixels']} PX) "
                             + ("OK" if mk["blockPx"] >= DG.MARKER_PX else "BELOW 2PX (DARKGATE MK = THE SCAR)"),
                             rgb=(160, 230, 160) if mk["blockPx"] >= DG.MARKER_PX else (240, 90, 90))
            cells.append(cell)
            print(f"  [frenzy] {clip} F{fr} {fac} e {e}: scar block {mk['blockPx']} px ({mk['pixels']} px)",
                  flush=True)
    finally:
        DG.marker_spec, K.import_character = spec0, imp0
    hh = max(c.shape[0] for c in cells)
    ww = max(c.shape[1] for c in cells)
    cells = [np.pad(c, ((0, hh - c.shape[0]), (0, ww - c.shape[1]), (0, 0)), constant_values=24) for c in cells]
    out_dir.mkdir(parents=True, exist_ok=True)
    for q, sz in pngio.write_png_native(out_dir / "game_w2_frenzy.png", review.grid(cells, 2)):
        print(f"  {q.name} {sz // 1024} KB")
    (scratch / "frenzy_read.json").write_text(json.dumps(recs, indent=1) + "\n")
    return recs


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    scratch = scratch_root("living", "oath_zealot")
    if "--frenzy-read" in args:
        from kit import paths
        out = scratch_root("living", "frenzy") if "--scratch" in args else paths.preview_dir(PREVIEW_DIR)
        recs = frenzy_read(out, scratch_root("living", "frenzy_dg"))
        if "--scratch" not in args:
            from kit import export
            man = export.Manifest()
            e = man.data["assets"][ASSET]
            e["previews"] = sorted(set(e.get("previews", [])) | {f"{PREVIEW_DIR}/{q.name}"
                                                                 for q in out.glob("game_w2_frenzy*.png")})
            man.save()
        hot = [r for r in recs if r["scarEmissive"] >= FRENZY_E]
        print(f"[frenzy] the scar at e {FRENZY_E}: block >= 2 px in {sum(r['ok'] for r in hot)} / {len(hot)} cells "
              "(a report, not a gate: fx.frenzy.read)")
    elif "--quick" in args:
        k = args.index("--quick")
        quick(Path(args[k + 1]) if len(args) > k + 1 and not args[k + 1].startswith("--") else scratch)
    elif "--anims" in args:
        names = [a for a in args[args.index("--anims") + 1:] if not a.startswith("--")]
        fac = next((a.split("=")[1] for a in args if a.startswith("--facing=")), "se")
        anim_quick(scratch / "anims", names, fac)
    else:
        sys.exit(ship(previews="--no-previews" not in args, verify="--no-verify" not in args))

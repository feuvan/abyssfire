"""灵脉精灵 Ley Sprite — ``SK_Pet_Sprite`` on ``SKEL_Pet_Sprite``: the Chapter-1 ley-beast companion (DECISIONS Q3:
"Chapter 1 ships the ley-beast slice: ``pet_sprite`` companion"; web ``src/graphics/sprites/pets/Sprite.ts``,
``Fly.ts``, ``PetKit.ts``).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/pets/sprite.py
        [--stage 0|1|2] [--quick [Clip frame]] [--anims Clip …] [--no-previews] [--no-verify]

A little ley-line sprite hovering at knee height: a glowing green-gold seed of a body (≈ 20 cm ball) with a
big-eyed face (dark green eyes ``#1E3A28`` with glints, a small smile ``#3A6A3A``, peach cheeks), a two-leaf sprout
on its head (``#5AB84A`` / light ``#A4D389``), four dragonfly wings (pale mint ``#D2FFDC`` with a vein each) off
the upper back, two tiny arms and a wisp tail (``#8AE07A``) that tapers into a glowing tip.

* **Evolution stages** (``--stage``; web ``PetStage`` / ``scale [1.15, 1.3, 1.46]``): 0 = ``SK_Pet_Sprite``
  (``beast_pet_sprite`` / ``pet_sprite``), 1 = ``SK_Pet_Sprite_E1`` (``beast_pet_sprite_e1``, 觉醒 Lv 10: ×1.13,
  lighter skin, longer wings, a crown of five leaves, two orbiting motes), 2 = ``SK_Pet_Sprite_E2``
  (``beast_pet_sprite_e2``, Lv 20: ×1.27, five blossoms, gold-veined cream wings, an emissive halo ring over the
  sprout, three motes). Same 17 bones and clip set; previews ``Previews/pet_sprite[_e1|_e2]/``, one portrait
  (stage 0). The aura grows 2 u and +.05 α per stage.

* **Scale** (web pet units → m): frame 56 px / 96 u × stage scale 1.15 × ``PET_BASE_SCALE`` 1.1 = 0.738 world px
  per unit → **1.86 cm / u** (2.552 cm per upright px, 2.21 per ground px: 1.88 / 1.63, averaged). Body centre
  25 u ≈ 0.47 m above the ground (hover 22 u + 3), top of the sprout ≈ 0.70 m, bob ±3.7 cm. Wings and arms are 25 %
  larger than the web's and the sprout stem twice as thick so they stay above the §0.3 readability floor at the W1
  camera (the web's 1.5 u limbs are 2.8 cm — 3 px at 1080p).
* **Rig (SKEL_Pet_Sprite, 17 bones, all children of ``root``)**: ``body`` (hover, bob, lunge, pitch: the seed ball,
  face, sprout root), ``tail_01..04`` (the wisp, raised / waved per the web ``tailPts``), ``wing_l_01/02``,
  ``wing_r_01/02`` (spread = flap, sweep), ``arm_l/r``, ``sprout`` (sway), ``eye_l/r`` (blink = vertical scale),
  ``mouth`` (smile) and ``mouth_o`` (the open "o" mouth, collapsed at rest, scaled in by the web ``beak``). Clips
  are keyed as bone basis matrices (``kit.anim.bake_basis``) from the web ``flyPose`` tracks.
* **Clips** (web 4 / 6 / 4 / 4 / 2 frames at 6 / 10 / 12 / 10 / 10 fps): Idle 667 loop (3 flaps), Walk 600 loop
  (4 flaps, pitched forward; ref 4.2 m/s = ``companion.followSpeedTilesPerSec``), Attack01 333 (rise and dive,
  **Contact 250** = ``PET_ATTACK_CONTACT``: the ley zap lands on the last frame), Cast01 400 (gather, wings wide →
  peak, **Release 200** = ``PET_CAST_RELEASE_FRAME`` 2 → the heal pulse), Hurt 200. No Death: an exhausted pet
  idles at 0.6× (``AAbyssCharacterActor``).
* **Ink**: ``small_monster`` class (1.4 cm baked, 60 % ink) with a per-asset **3.5 px** request at 1080p
  (``OUTLINE_PX``, manifest ``outlinePx1080`` + ``outlinePxNote``; gate 3.0 px, the Chapter-1 character gate): at the
  class's 3.0 px the wing tips, sprout and tail wisp kept a rim >= 3 px on only 72-78 % of the worst cells; the web
  inks pets heavier than heroes (1.5 u vs 1.1). The body is not emissive (the bloom of a pale glowing ball washed out its ink rim): the ley
  light is the aura card behind it. **FX** (runtime cards, manifest ``fx``): body aura ``#C8FF90`` α .32 r 13 u, tail-tip glow
  ``#E8FF90`` α .6 + trailing sparkles, cast heal ring, attack zap at ``fx_hand_r``.
"""
from __future__ import annotations

import math
import os
import sys
import tempfile
from dataclasses import dataclass, replace
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "monsters", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Quaternion, Vector  # noqa: E402

from kit import anim, mesh as M, outline, paths, pngio, review, rig as R, scene, shading  # noqa: E402
from kit.anim import Clip  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402
from kit.rig import Bind  # noqa: E402

def _stage_arg() -> int:
    """``--stage N`` (0 base, 1 awakened ``_E1``, 2 ``_E2``): the evolution look (web ``PetStage``)."""
    for i, a in enumerate(sys.argv):
        if a == "--stage" and i + 1 < len(sys.argv):
            return int(sys.argv[i + 1])
        if a.startswith("--stage="):
            return int(a.split("=", 1)[1])
    return 0


STAGE = _stage_arg()
STAGE_SCALE = (1.15, 1.3, 1.46)  # web SpriteSpec.scale per stage
SUFFIX = "" if STAGE == 0 else f"_E{STAGE}"
ASSET = "SK_Pet_Sprite" + SUFFIX
TOKEN = "Pet_Sprite" + SUFFIX
SKELETON = "SKEL_Pet_Sprite"    # shared by the three stages (same bones, per-mesh reference pose, like SKEL_Human)
FAMILY = "Pets"
PREFIX = "sprite" if STAGE == 0 else f"sprite_e{STAGE}"
CATEGORY = "Characters"
OUTLINE_CLASS = "small_monster"
OUTLINE_PX = 3.5                # per-asset hull request (coherence pass): see OUTLINE_PX_NOTE
OUTLINE_PX_NOTE = ("coherence pass: at the small-monster class's 3.0 px the wing tips, sprout and tail wisp kept a rim "
                   ">= 3 px on only 72-78 % of the silhouette in the worst 1080p cells (Idle back) against the Chapter-1 "
                   "character gate (dark rim >= 3 px on >= 90 %); 3.5 px, as the heroes, monsters and NPCs, holds it "
                   "(gate 3.0 px) — the web inks pets heavier than heroes (1.5 u vs 1.1)")
U = 0.0186 * STAGE_SCALE[STAGE] / STAGE_SCALE[0]   # m per web pet unit (stage 0: 1.86 cm)
HOVER_U = 22.0                  # web STYLE.hover
HEAD_U = 3.0                    # head centre 3 u above the pose body point
BODY_R = (5.4, 5.4, 5.2)        # ball radii (lateral, forward, up), web head p3(5.4, 5.2, 5.4)
REF_SPEED = 4.2                 # m/s (pets.json companion.followSpeedTilesPerSec)
CONTACT_MS = 250.0
RELEASE_MS = 200.0
HEIGHT = 0.70 * U / 0.0186 + (0.0 if STAGE == 0 else 0.03 * STAGE)   # rest top (sprout leaves; crown / halo)
SKIN = ("#C8F4B0", "#D0F8B8", "#E0FCC8")[STAGE]
PREVIEW_DIR = "pet_sprite" if STAGE == 0 else f"pet_sprite_e{STAGE}"
LEGACY_GAME_IDS = ["beast_pet_sprite", "pet_sprite"] if STAGE == 0 else [f"beast_pet_sprite_e{STAGE}"]   # artId per PetStage
# Dark remake A11: pet_sprite is re-dressed as 灯魂 / the Lanternwisp (pets/sprite_lanternwisp.py, the same rig and clips),
# which ships under these ids and the portrait name; the seed sprite keeps its files but claims no ids, so a re-run of
# this script never takes them back.
SUPERSEDED_BY = "SK_Pet_Lanternwisp" + SUFFIX
GAME_IDS: list[str] = []
DISPLAY = ({"zh-CN": "灵脉精灵", "en": "Ley Sprite"}, {"zh-CN": "灵脉精灵·觉醒", "en": "Ley Sprite (Awakened)"},
           {"zh-CN": "灵脉精灵·花冠", "en": "Ley Sprite (Bloom)"})[STAGE]
TAIL = ("#8AE07A", "#9AE880", "#B0F090")[STAGE]

# web STYLE (Sprite.ts): hover 22, flapMid .95, flapAmp .5, flapsIdle 3, flapsWalk 4, bob 2
FLAP_MID, FLAP_AMP, FLAPS_IDLE, FLAPS_WALK, BOB = 0.95, 0.5, 3, 4, 2.0

REGIONS = outline.ink_regions(dict(
    skin=Region(SKIN, .30, .50),                       # tn(SKIN[stage], .5, .3); the ley light is the aura card
    tail=Region(TAIL, .30, .50),                       # (no emissive body: its bloom washed the ink rim out)
    tail_tip=Region("#E8FF90", .0, .25, e=0.15),      # the wisp tip: lit, its light is the card behind it
    leaf=Region("#5AB84A", .45, .45),                  # tn(LEAF, .45)
    leaf_lt=Region("#A4D389", .40, .40),               # leaf.light
    stem=Region("#4A9A3C", .42, .35),
    vein=Region("#5AB84A").shade_region(.30, .12),
    wing=Region("#D2FFDC" if STAGE < 2 else "#ECFFC8", .22, .35),   # rgba(210,255,220,.72) / stage 2
    wing_vein=Region("#8CCB98" if STAGE < 2 else "#E0B848", .25, .25),       # veins; stage 2 gold (240,200,80)
    iris=Region("#1E3A28", .30, .12),
    pupil=Region("#0A1A10", .30, .05),
    glint=Region("#FFFFFF", .0, .10, e=0.6),
    mouth=Region("#3A6A3A", .30, .10),
    mouth_in=Region("#2A4A2A", .30, .05),
    cheek=Region("#F2B8A0", .20, .30),                 # rgba(255,170,150,.5) over the skin
    **({} if STAGE == 0 else dict(
        crown_leaf=Region("#5AB84A", .45, .45),        # stage 1: leaf crown (leaf.base)
        petal_gold=Region("#FFD860", .30, .40),        # stage 2: blossom crown
        petal_pink=Region("#FF9AC0", .30, .40),
        petal_cream=Region("#FFF4C0", .30, .40),
        blossom=Region("#E89020", .35, .30),
        halo=Region("#FFEC8C", .0, .20, e=0.45),       # stage 2 halo rgba(255,236,140,.85): a soft bloom
    )),
), OUTLINE_CLASS)


def w2b(x: float, y: float, z: float = 0.0) -> Vector:
    """Web pet-local (x forward, y down, z lateral + = the creature's right) → Blender metres (−Y forward, +Z up,
    +X = the creature's left)."""
    return Vector((-z * U, -x * U, -y * U))


C0 = w2b(0.0, -(HOVER_U + HEAD_U))            # rest body (ball) centre


def ball_dir(fx: float, fy: float, fz: float) -> Vector:
    """Unit direction on the ball from web surface coords (forward, down, lateral)."""
    return Vector((-fz, -fx, -fy)).normalized()


def ball_point(d: Vector, lift: float = 0.0) -> Vector:
    """Point on the (ellipsoid) ball along direction ``d`` from its centre, + ``lift`` m along the normal."""
    rx, ry, rz = BODY_R[0] * U, BODY_R[1] * U, BODY_R[2] * U
    k = 1.0 / math.sqrt((d.x / rx) ** 2 + (d.y / ry) ** 2 + (d.z / rz) ** 2)
    p = d * k
    n = Vector((p.x / rx ** 2, p.y / ry ** 2, p.z / rz ** 2)).normalized()
    return C0 + p + n * lift, n


# ── wings ───────────────────────────────────────────────────────────────────────────────────────────────
WINGS = {  # name: (anchor web (fwd, down, lat), side 'l'/'r', pair)
    "wing_l_01": ((-3.2, 1.5, -1.6), "l", 1), "wing_r_01": ((-3.2, 1.5, 1.6), "r", 1),
    "wing_l_02": ((-3.4, 2.6, -1.4), "l", 2), "wing_r_02": ((-3.4, 2.6, 1.4), "r", 2),
}
WING_L = (( 9.0, 10.5, 12.5)[STAGE] * 1.25, (9.0, 10.5, 12.5)[STAGE] * 0.8 * 1.25)   # span per pair (u), +25 %
WING_W = (2.6 * 1.25, 2.2 * 1.25)


def wing_frame(spread: float, sweep: float, side: str) -> Matrix:
    """3×3 (span, chord, normal) of a wing (web ``wing``: u = (−sin w, −cos s·cos w, side·sin s·cos w), v = (−cos w,
    sin w·0.3 + .12, 0)); spread 0 = up, π/2 = out, side 'l' = +X."""
    sx = 1.0 if side == "l" else -1.0
    cs, ss, cw, sw = math.cos(spread), math.sin(spread), math.cos(sweep), math.sin(sweep)
    span = Vector((sx * ss * cw, sw, cs * cw)).normalized()
    chord = Vector((0.0, cw, -(sw * 0.3 + 0.12)))
    chord = (chord - span * chord.dot(span)).normalized()
    nrm = span.cross(chord).normalized()
    m = Matrix.Identity(3)
    for i in range(3):
        m[i][0], m[i][1], m[i][2] = span[i], chord[i], nrm[i]
    return m


def wing_rest(pair: int) -> tuple[float, float]:
    return (FLAP_MID + (0.45 if pair == 2 else 0.0), 0.5 if pair == 1 else 0.9)


# ── pose model (web FlyPose) ────────────────────────────────────────────────────────────────────────────
@dataclass
class FP:
    bx: float = 0.0
    by: float = -HOVER_U
    pitch: float = 0.0
    wn: float = FLAP_MID          # near (right) wings
    wf: float = FLAP_MID          # far (left) wings
    sweep: float = 0.0
    hx: float = 0.0
    hy: float = 0.0
    hp: float = 0.0
    beak: float = 0.0
    talon: float = 0.0
    tail: float = 0.0
    lid: float = 1.0
    glow: float = 0.0
    fx: float = 0.0

    def lerp(self, o: "FP", k: float) -> "FP":
        return FP(*[a + (b - a) * k for a, b in zip(self.__dict__.values(), o.__dict__.values())])


REST = FP()


def P(**kw) -> FP:
    return replace(REST, **kw)


def track(keys, t: float) -> FP:
    """Web ``flyTrack``: smoothstep between keys."""
    if t <= keys[0][0]:
        return keys[0][1]
    for (a0, p0), (a1, p1) in zip(keys, keys[1:]):
        if t <= a1:
            u = (t - a0) / max(1e-4, a1 - a0)
            return p0.lerp(p1, u * u * (3 - 2 * u))
    return keys[-1][1]


def fly_pose(action: str, t: float) -> FP:
    """Web ``flyPose`` (Fly.ts) with the Sprite tail wiggle (``tail + sin(2πt)·0.3``)."""
    ph = t * math.tau
    if action == "idle":
        f = math.cos(ph * FLAPS_IDLE)
        # the web blinks on one of four frames (t .75); here a 120 ms close / open round t .75 of the 667 ms loop
        blink = max(0.0, 1.0 - abs(t - 0.75) / 0.09)
        p = P(by=-HOVER_U + math.sin(ph) * BOB, wn=FLAP_MID + f * FLAP_AMP,
              wf=FLAP_MID + math.cos(ph * FLAPS_IDLE - 0.35) * FLAP_AMP, hp=math.sin(ph - 0.8) * 0.05,
              tail=math.sin(ph + 1) * 0.15, lid=1.0 - 0.8 * blink)
    elif action == "walk":
        f = math.cos(ph * FLAPS_WALK)
        p = P(bx=0.5, by=-HOVER_U + math.sin(ph * FLAPS_WALK + 0.8) * BOB * 0.8, pitch=0.14,
              wn=FLAP_MID + f * FLAP_AMP * 1.25, wf=FLAP_MID + math.cos(ph * FLAPS_WALK - 0.35) * FLAP_AMP * 1.25,
              sweep=0.15, tail=-0.2)
    elif action == "attack":
        up = P(bx=-3, by=-HOVER_U - 4, pitch=-0.3, wn=0.2, wf=0.25, hp=-0.25, talon=0.3, tail=0.3, fx=0.2)
        dive = P(bx=4, by=-HOVER_U + 3, pitch=0.35, wn=1.3, wf=1.25, sweep=0.6, hp=0.2, beak=0.6, talon=0.7,
                 tail=-0.3, fx=0.7)
        hit = P(bx=8, by=-HOVER_U + 5, pitch=0.2, wn=0.5, wf=0.55, sweep=0.2, hp=0.35, hx=1, beak=1, talon=1,
                tail=-0.2, fx=1)
        p = track([(0, REST), (0.33, up), (0.67, dive), (1, hit)], t)
    elif action == "cast":
        gather = P(by=-HOVER_U + 1.5, pitch=0.05, wn=1.9, wf=1.9, hp=0.15, lid=0.5, glow=0.4)
        peak = P(by=-HOVER_U - 4, pitch=-0.12, wn=1.35, wf=1.35, hp=-0.25, beak=0.6, lid=0.3, glow=1, tail=0.3)
        settle = P(by=-HOVER_U - 2.5, pitch=-0.05, wn=0.9, wf=0.95, hp=-0.1, beak=0.2, lid=0.8, glow=0.7)
        p = track([(0, REST), (0.33, gather), (0.67, peak), (1, settle)], t)
    else:  # hurt
        hit = P(bx=-4, by=-HOVER_U - 1, pitch=-0.4, wn=0.4, wf=2.0, hp=-0.4, lid=0.15, tail=0.4)
        p = track([(0, hit), (1, P(bx=-2, pitch=-0.15, wn=1.0, wf=1.3, hp=-0.15, lid=0.6))], t)
    p.tail += math.sin(ph) * 0.3
    return p


# ── rig ─────────────────────────────────────────────────────────────────────────────────────────────────
TAIL_K = (0.0, 0.25, 0.5, 0.75, 1.0)


def tail_point(p: FP, k: float, t: float) -> Vector:
    """Web ``tailPts`` (absolute pet coords: x forward, y down from the ground) → Blender."""
    return w2b(p.bx - 1 - k * 6 - p.pitch * k * 6, p.by + 3 + k * 9 - p.tail * k * 5,
               math.sin(t * math.tau + k * 3) * 2 * k)


def tail_rest(k: float) -> Vector:
    return w2b(-1 - k * 6, -HOVER_U + 3 + k * 9, 0.0)


@dataclass
class SpriteSpec:
    name: str = ASSET
    skeleton: str = SKELETON

    def crown_height(self) -> float:
        return HEIGHT


def arm_points(side: str, talon: float = 0.0, glow: float = 0.0) -> tuple[Vector, Vector]:
    """Shoulder / hand in body-local web units (``b.local(head, .5, 5, ±2.6)``; hand +1.5 + talon·2.5 forward,
    +1.6 − glow·3.5 down), as Blender offsets from the ball centre."""
    lat = 2.6 if side == "r" else -2.6
    sh = w2b(1.6, 3.1, lat * 1.05)                  # tucked under the face, reaching forward like a chick's
    hand = w2b(1.6 + 2.2 + talon * 2.5, 3.1 + 1.0 - glow * 3.5, lat * 1.0)
    return sh, hand


SPROUT_DIR = ball_dir(-0.1, -1.0, 0.0)
EYE = {s: ball_dir(0.74, 0.08, (0.5 if s == "r" else -0.5)) for s in ("l", "r")}
MOUTH_DIR = ball_dir(0.9, 0.45, 0.0)
CHEEK = {s: ball_dir(0.6, 0.45, (0.75 if s == "r" else -0.75)) for s in ("l", "r")}
EYE_R = 1.45 * U * 1.05
FX_REST_K = 0.02


def build_rig() -> R.Rig:
    scene.ensure_object_mode()
    data = bpy.data.armatures.new(SKELETON)
    obj = bpy.data.objects.new(ASSET, data)
    scene.link(obj)
    data.display_type = "STICK"
    scene.select_only([obj])
    bpy.ops.object.mode_set(mode="EDIT")
    eb = data.edit_bones
    J = {}

    def bone(name, head, tail, parent="root", x_axis=(1, 0, 0)):
        b = eb.new(name)
        b.head, b.tail = Vector(head), Vector(tail)
        if parent:
            b.parent = eb[parent]
        y = (b.tail - b.head).normalized()
        x = Vector(x_axis)
        if abs(x.dot(y)) > 0.95:
            x = Vector((0, 1, 0))
        x = (x - y * x.dot(y)).normalized()
        b.align_roll(x.cross(y))
        b.use_deform = True
        J[name] = Vector(head)
        return b
    bone("root", (0, 0, 0), (0, 0, 0.05), parent=None)
    bone("body", C0, C0 + Vector((0, 0, 0.08)))
    pts = [tail_rest(k) for k in TAIL_K]
    for i in range(4):
        bone(f"tail_{i + 1:02d}", pts[i], pts[i + 1])
    for nm, (anc, side, pair) in WINGS.items():
        a = C0 + w2b(*anc)
        f = wing_frame(*wing_rest(pair), side)
        bone(nm, a, a + f.col[0] * 0.05)
    for s in ("l", "r"):
        sh, hand = arm_points(s)
        bone(f"arm_{s}", C0 + sh, C0 + hand)
    top, _ = ball_point(SPROUT_DIR, -0.004)
    bone("sprout", top, top + Vector((0, 0, 0.06)))
    for s in ("l", "r"):
        e, n = ball_point(EYE[s], 0.0)
        bone(f"eye_{s}", e, e + n * 0.03)
    m, n = ball_point(MOUTH_DIR, 0.0)
    bone("mouth", m, m + n * 0.03)
    bone("mouth_o", m + n * 0.002, m + n * 0.032)
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in obj.pose.bones:
        pb.rotation_mode = "QUATERNION"
    obj["af_skeleton"] = SKELETON
    hand_r = C0 + arm_points("r")[1]
    hand_l = C0 + arm_points("l")[1]
    zap = C0 + w2b(8.0, 3.0, 0.0)
    sockets = [
        {"name": "fx_feet", "bone": "root", "pos": Vector((0, 0, 0))},
        {"name": "fx_chest", "bone": "body", "pos": C0},
        {"name": "fx_head", "bone": "body", "pos": C0 + Vector((0, 0, BODY_R[2] * U * 0.6))},
        {"name": "fx_overhead", "bone": "body", "pos": C0 + Vector((0, 0, HEIGHT - C0.z + 0.25))},
        {"name": "fx_hand_r", "bone": "body", "pos": zap},          # the attack zap / bolt origin (web local 8, 3)
        {"name": "fx_hand_l", "bone": "arm_l", "pos": hand_l},
        {"name": "fx_paw_r", "bone": "arm_r", "pos": hand_r},
        {"name": "fx_tail", "bone": "tail_04", "pos": pts[4]},
        {"name": "fx_eye_l", "bone": "eye_l", "pos": J["eye_l"]},
        {"name": "fx_eye_r", "bone": "eye_r", "pos": J["eye_r"]},
    ]
    return R.Rig(obj, SpriteSpec(), J, sockets)


# ── mesh ────────────────────────────────────────────────────────────────────────────────────────────────
def collapsed(head: Vector) -> Matrix:
    return Matrix.Translation(head) @ Matrix.Diagonal((FX_REST_K,) * 3 + (1.0,)) @ Matrix.Translation(-head)


def frame_z(origin: Vector, z_dir: Vector, y_hint: Vector) -> Matrix:
    z = Vector(z_dir).normalized()
    y = Vector(y_hint) - z * Vector(y_hint).dot(z)
    if y.length < 1e-6:
        y = z.orthogonal()
    y.normalize()
    x = y.cross(z)
    m = Matrix.Identity(4)
    for i in range(3):
        m[i][0], m[i][1], m[i][2], m[i][3] = x[i], y[i], z[i], origin[i]
    return m


def teardrop(length: float, width: float, n: int = 10, tip_round: float = 0.35) -> list[tuple[float, float]]:
    """CCW outline of a wing / leaf from the root (0,0) to the tip (length, 0): the web quadratic wing (fat in the
    front half, the trailing edge 0.9 × as deep)."""
    up, lo = [], []
    for i in range(n + 1):
        t = i / n
        y = width * (4 * t * (1 - t)) ** 0.75 * (1.0 - 0.15 * t)
        up.append((length * t, y))
        lo.append((length * t, -0.9 * y))
    pts = lo + list(reversed(up[1:-1]))
    return pts


def wing_part(pair: int) -> M.Part:
    L, W = WING_L[pair - 1] * U, WING_W[pair - 1] * U
    pts = teardrop(L, W, 12)
    p = M.plate(pts, 0.006, bevel=0.0)
    p.deform(lambda co: Vector((co.x, co.y, co.z + 0.012 * (co.x / L) ** 2)))       # a gentle upward curl
    return p.smooth(True)


def wing_vein(pair: int) -> M.Part:
    L = WING_L[pair - 1] * U
    pts = [Vector((L * t, 0.0, 0.0035 + 0.012 * t * t)) for t in (0.05, 0.35, 0.65, 0.88)]
    return M.strap(pts, 0.009, 0.004, up=(0, 0, 1))


def leaf_part(length: float, width: float) -> M.Part:
    p = M.plate(teardrop(length, width, 10), 0.005)
    p.deform(lambda co: Vector((co.x, co.y, co.z + 0.25 * length * (co.x / length) ** 2
                                - 0.15 * width * (co.y / width) ** 2)))
    return p.smooth(True)


def build_body(pal: Palette, rig: R.Rig, lod: int = 0):
    from kit import asset
    if lod:
        skip = ("wing_vein", "cheek", "glint", "crown_core")
        b = M.Builder(ASSET, pal, prefix=PREFIX, lod_ratio=0.5, lod_skip=lambda n: n in skip)
    else:
        b = M.Builder(ASSET, pal, prefix=PREFIX)
    b.regions(**REGIONS)
    rx, ry, rz = BODY_R[0] * U, BODY_R[1] * U, BODY_R[2] * U
    ball = M.ellipsoid((rx, ry, rz), C0, sides=22, rings=12)
    bid = b.add(ball, "skin", Bind.rigid("body"), name="ball", hull_pad=True)
    # wisp tail: seed body tapering into light (web tube radii 3.4 … 0.4 u, ends in a glowing tip)
    tpts = [tail_rest(k / 5) for k in range(6)]
    rads = [r * U for r in (3.4, 2.9, 2.2, 1.6, 1.15, 0.85)]
    tail = M.tube(tpts, rads, sides=10)
    tail_bind = Bind.blend("tail_01", "tail_02", "tail_03", "tail_04", falloff=4, smooth=0.01)
    b.add(tail, "tail", tail_bind, name="tail", hull_pad=True)
    tip = M.ellipsoid((1.25 * U, 1.25 * U, 1.4 * U), tpts[-1] + Vector((0, 0.002, -0.004)), sides=8, rings=5)
    b.add(tip, "tail_tip", Bind.rigid("tail_04"), name="tail_tip", hull_pad=True)
    # wings (pair 1 above pair 2), modelled at their rest spread / sweep, a vein strip on top
    for nm, (anc, side, pair) in WINGS.items():
        a = C0 + w2b(*anc)
        f = wing_frame(*wing_rest(pair), side).to_4x4()
        f.translation = a
        wp = wing_part(pair).transform(f)
        wid = b.add(wp, "wing", Bind.rigid(nm), name="wing", hull_pad=True)
        b.add(wing_vein(pair).transform(f), "wing_vein", Bind.rigid(nm), name="wing_vein", outline=False,
              covered_by=wid)
    # tiny arms (stubby capsules, readable at 1080p)
    for s in ("l", "r"):
        sh, hand = arm_points(s)
        arm = M.capsule(C0 + sh, C0 + hand, 1.15 * U, 0.95 * U, sides=8, cap_rings=2)
        b.add(arm, "skin", Bind.rigid(f"arm_{s}"), name=f"arm_{s}", hull_pad=True)
    # sprout: a curved stem from the crown and two leaves (dark / light) fanned left-right, tilted up
    top, nrm = ball_point(SPROUT_DIR, -0.006)
    stem_pts = [top + Vector((0, 0.002 * k, 0.018 * k)) + Vector((0, 0.006 * k * k / 9, 0)) for k in range(4)]
    stem = M.tube(stem_pts, [0.0085, 0.0075, 0.0068, 0.006], sides=8)
    sid = b.add(stem, "stem", Bind.rigid("sprout"), name="stem", hull_pad=True)
    st_top = stem_pts[-1]
    for s, reg, ang in (("l", "leaf", 1.0), ("r", "leaf_lt", -1.0)):
        L, W = 5.6 * U * 1.1, 2.0 * U * 1.1
        lf = leaf_part(L, W)
        # leaf root at the stem top, pointing out to the side (+X left / −X right), raised 28°, yawed a little back
        m = Matrix.Translation(st_top) @ Matrix.Rotation(math.radians(ang * 14.0), 4, "Z") @ \
            Matrix.Rotation(math.radians(-ang * 28.0), 4, "Y")
        if ang < 0:
            m = m @ Matrix.Rotation(math.pi, 4, "Z")
        lp = lf.transform(m)
        lid = b.add(lp, reg, Bind.rigid("sprout"), name=f"leaf_{s}", hull_pad=True)
        vp = [m @ Vector((L * t, 0.0, 0.004 + 0.25 * L * t * t)) for t in (0.08, 0.4, 0.75)]
        b.add(M.strap(vp, 0.006, 0.003, up=[(m.to_3x3() @ Vector((0, 0, 1)))] * 3), "vein", Bind.rigid("sprout"),
              name="leaf_vein", outline=False, covered_by=lid)
    # face: big dark eyes (iris dome, pupil, glint upper-left = toward the key light), smile, "o" mouth, cheeks
    for s in ("l", "r"):
        e, n = ball_point(EYE[s], -0.003)
        m = frame_z(e, n, Vector((0, 0, 1)))
        iris = M.ellipsoid((EYE_R, EYE_R * 1.12, 0.010), (0, 0, 0.002), sides=14, rings=6).transform(m)
        b.add(iris, "iris", Bind.rigid(f"eye_{s}"), name=f"iris_{s}", outline=False, covered_by=bid)
        sg = 1.0 if s == "l" else -1.0
        pup = M.ellipsoid((EYE_R * 0.52, EYE_R * 0.6, 0.006), (-sg * EYE_R * 0.12, -EYE_R * 0.08, 0.008), sides=10,
                          rings=5).transform(m)
        b.add(pup, "pupil", Bind.rigid(f"eye_{s}"), name=f"pupil_{s}", outline=False, covered_by=bid)
        gl = M.ellipsoid((EYE_R * 0.32, EYE_R * 0.32, 0.004), (sg * EYE_R * 0.30, EYE_R * 0.40, 0.011), sides=8,
                         rings=4).transform(m)
        b.add(gl, "glint", Bind.rigid(f"eye_{s}"), name="glint", outline=False, covered_by=bid)
        ck, cn = ball_point(CHEEK[s], 0.0005)
        cm = frame_z(ck, cn, Vector((0, 0, 1)))
        cheek = M.ellipsoid((1.3 * U, 0.75 * U, 0.003), (0, 0, 0), sides=10, rings=4).transform(cm)
        b.add(cheek, "cheek", Bind.rigid("body"), name="cheek", outline=False, covered_by=bid)
    mo, mn = ball_point(MOUTH_DIR, 0.0)
    mm = frame_z(mo, mn, Vector((0, 0, 1)))
    arc = [mm @ Vector((x, -0.004 * (1 - (x / 0.016) ** 2) + 0.002, 0.0015)) for x in (-0.016, -0.008, 0.0, 0.008, 0.016)]
    smile = M.strap(arc, 0.0045, 0.003, up=[mm.to_3x3() @ Vector((0, 0, 1))] * len(arc))
    b.add(smile, "mouth", Bind.rigid("mouth"), name="smile", outline=False, covered_by=bid)
    o_m = M.ellipsoid((0.0085, 0.010, 0.004), (0, -0.002, 0.001), sides=10, rings=4).transform(mm)
    b.cover(bid, o_m)
    o_m.transform(collapsed(rig.joints["mouth_o"]))
    b.add(o_m, "mouth_in", Bind.rigid("mouth_o"), name="mouth_o", outline=False)
    if STAGE > 0:
        add_crown(b, bid)
    body = asset.finish_mesh(b, pal, OUTLINE_CLASS, rig=rig, grounding_height=HEIGHT,
                             name=f"{ASSET}_LOD{lod}" if lod else None)
    body["af_lod"] = lod
    return body


def tangent_axes(n: Vector) -> tuple[Vector, Vector]:
    """The (x, y) axes of the tangent plane at a ball normal ``n`` (y as close to world up as the plane allows)."""
    f = frame_z(Vector((0, 0, 0)), n, Vector((0, 0, 1)) if abs(n.z) < 0.9 else Vector((0, -1, 0)))
    return f.col[0].xyz, f.col[1].xyz


def conform(part: M.Part, p: Vector, n: Vector, lift: float) -> M.Part:
    """Lay a part modelled in a local tangent frame (x, y in the plane, z = height) onto the ball: each vertex goes
    to the surface point under ``p + x·t1 + y·t2`` and ``lift + z`` along that point's normal — a crown piece that
    hugs the head (inside the ball's padded ink, like the web's surface-drawn crown) instead of a flat card."""
    t1, t2 = tangent_axes(n)

    def fn(co: Vector) -> Vector:
        q = p + t1 * co.x + t2 * co.y - C0
        s_, ns = ball_point(q.normalized(), 0.0)
        return s_ + ns * (lift + co.z)
    return part.deform(fn)


def add_crown(b: M.Builder, bid: int) -> None:
    """Evolved looks (web Sprite.ts ``stage > 0``): five leaves (stage 1) or five blossoms (stage 2) round the top of
    the head at ``surface(head, (cos a·.35, −.8, sin a·.7))``, a ∈ [−.9, .9]. The web paints them on the head (thin
    leaf line, no ink on the blossoms), so here they hug the ball (``conform``) inside its padded ink
    (``covered_by`` the ball): five separately inked 2 px leaves would merge into a black cap at 1080p. Stage 2
    adds the gold halo — the web's ring over the crown (rgba(255,236,140,.85)) — as an emissive circlet round the
    sprout's foot, on the head so its light never lies on the silhouette ink."""
    cols = ("petal_gold", "petal_pink", "petal_cream", "petal_pink", "petal_gold")
    for i in range(5):
        a = -0.9 + i / 4 * 1.8
        d = ball_dir(math.cos(a) * 0.35, -0.8, math.sin(a) * 0.7)
        p, n = ball_point(d, 0.0)
        up = (C0 + SPROUT_DIR * BODY_R[2] * U) - p           # toward the sprout: the leaf tip / blossom lift
        if STAGE == 1:
            # web ellipse(0.9, 1.8) at q − 1 u, turned a·0.6; 20 % larger (the wings' readability rule). The long
            # axis points up the head toward the sprout (tangent-plane angle phi), then fans by a·0.6.
            t1, t2 = tangent_axes(n)
            phi = math.atan2(-up.dot(t1), up.dot(t2))
            lf = M.ellipsoid((1.1 * U, 2.25 * U, 0.32 * U), (0, 1.2 * U, 0.0), sides=10, rings=5)
            lf.transform(Matrix.Rotation(phi + a * 0.6, 4, "Z"))
            b.add(conform(lf, p, n, 0.12 * U), "crown_leaf", Bind.rigid("body"), name="crown", outline=False,
                  covered_by=bid)
        else:
            for k in range(5):
                pa = k / 5 * math.tau
                pet = M.ellipsoid((0.95 * U, 0.95 * U, 0.26 * U), (math.cos(pa) * 1.15 * U, math.sin(pa) * 1.15 * U,
                                                                  0.0), sides=7, rings=3)
                b.add(conform(pet, p, n, 0.16 * U), cols[i], Bind.rigid("body"), name="crown_petal", outline=False,
                      covered_by=bid)
            core = M.ellipsoid((0.62 * U, 0.62 * U, 0.34 * U), (0, 0, 0), sides=7, rings=3)
            b.add(conform(core, p, n, 0.36 * U), "blossom", Bind.rigid("body"), name="crown_core", outline=False,
                  covered_by=bid)
    if STAGE == 2:
        p, n = ball_point(SPROUT_DIR, 0.0)
        ring = M.sweep([Vector((math.cos(t) * 2.6 * U, math.sin(t) * 2.6 * U, 0.0))
                        for t in (k / 20 * math.tau for k in range(20))],
                       M.superellipse(0.34 * U, 0.3 * U, 5), closed_path=True, up=(0, 0, 1))
        b.add(conform(ring, p, n, 0.32 * U), "halo", Bind.rigid("body"), name="halo", outline=False, covered_by=bid)


# ── clips (bone basis matrices from the web pose) ───────────────────────────────────────────────────────
class SpriteAnims:
    def __init__(self, rig: R.Rig):
        self.rig = rig
        self.rest = {b.name: b.matrix_local.copy() for b in rig.obj.data.bones}

    def body_x(self, p: FP) -> Matrix:
        """Armature-space map of the ball: moved to the pose's head centre, pitched by hp + 0.4·pitch about it."""
        c = w2b(p.bx + p.hx * 0.5, p.by - HEAD_U + p.hy)
        ang = p.hp + p.pitch * 0.4
        return Matrix.Translation(c) @ Matrix.Rotation(ang, 4, "X") @ Matrix.Translation(-C0)

    def basis(self, p: FP, t: float) -> dict:
        out = {}
        rest = self.rest

        def put(name, X):
            Mr = rest[name]
            out[name] = Mr.inverted() @ X @ Mr
        B = self.body_x(p)
        put("body", B)
        # tail: each segment from its posed head toward the next posed joint (rest length kept by an axial scale)
        pr = [tail_rest(k) for k in TAIL_K]
        pp = [tail_point(p, k, t) for k in TAIL_K]
        for i in range(4):
            a, b2 = pr[i], pr[i + 1]
            a2, c2 = pp[i], pp[i + 1]
            d0, d1 = (b2 - a), (c2 - a2)
            q = d0.normalized().rotation_difference(d1.normalized()).to_matrix().to_4x4()
            k = d1.length / max(d0.length, 1e-6)
            Mr = rest[f"tail_{i + 1:02d}"]
            ax = Mr.to_3x3()
            S = ax.to_4x4() @ Matrix.Diagonal((1.0, k, 1.0, 1.0)) @ ax.inverted().to_4x4()   # stretch along the bone
            put(f"tail_{i + 1:02d}", Matrix.Translation(a2) @ q @ S @ Matrix.Translation(-a))
        # wings: rest frame → pose frame about the anchor, carried by the ball
        for nm, (anc, side, pair) in WINGS.items():
            s0, w0 = wing_rest(pair)
            spread = (p.wn if side == "r" else p.wf) + (0.45 if pair == 2 else 0.0)
            sweep = (0.5 + 0.4 * p.sweep) if pair == 1 else (0.9 + 0.3 * p.sweep)
            Rw = (wing_frame(spread, sweep, side) @ wing_frame(s0, w0, side).transposed()).to_4x4()
            a = C0 + w2b(*anc)
            put(nm, B @ Matrix.Translation(a) @ Rw @ Matrix.Translation(-a))
        # arms: reach forward with ``talon``, up with ``glow``
        for s in ("l", "r"):
            sh, h0 = arm_points(s)
            _, h1 = arm_points(s, p.talon, p.glow)
            d0, d1 = (h0 - sh), (h1 - sh)
            q = d0.normalized().rotation_difference(d1.normalized()).to_matrix().to_4x4()
            a = C0 + sh
            put(f"arm_{s}", B @ Matrix.Translation(a) @ q @ Matrix.Translation(-a))
        # sprout sway (web: sin(2πt)·0.8 u at the 3.5 u stem top) + a nod with the pitch
        top = rest["sprout"].translation
        sway = math.atan2(math.sin(t * math.tau) * 0.8, 3.5)
        nod = -0.35 * (p.pitch + p.hp)
        Rs = Matrix.Rotation(sway, 4, "Y") @ Matrix.Rotation(nod, 4, "X")
        put("sprout", B @ Matrix.Translation(top) @ Rs @ Matrix.Translation(-top))
        # eyes: blink / squint = vertical scale in the eye frame
        for s in ("l", "r"):
            e = rest[f"eye_{s}"]
            lid = max(0.08, min(1.0, p.lid))
            D = Matrix.Diagonal((1.0, lid, 1.0, 1.0))          # bone Y = the eye normal; scale the eye's height:
            ax = e.to_3x3()
            up_local = ax.inverted() @ Vector((0, 0, 1))
            # squash along the bone-space axis closest to world up (the eye's vertical)
            i = max(range(3), key=lambda j: abs(up_local[j]))
            D = Matrix.Identity(4)
            D[i][i] = lid
            put(f"eye_{s}", B @ e @ D @ e.inverted())
        # mouth: the smile shrinks out, the "o" scales in with ``beak``
        ko = max(0.0, min(1.0, (p.beak - 0.2) / 0.35))
        ks = max(0.001, 1.0 - ko)
        m = rest["mouth"].translation
        put("mouth", B @ Matrix.Translation(m) @ Matrix.Diagonal((ks, ks, ks, 1.0)) @ Matrix.Translation(-m))
        mo = rest["mouth_o"].translation
        sc = max(0.001, ko) / FX_REST_K
        put("mouth_o", B @ Matrix.Translation(mo) @ Matrix.Diagonal((sc, sc, sc, 1.0)) @ Matrix.Translation(-mo))
        return out

    def clip(self, name, length_ms, action, span_ms, loop=False, notifies=None, ref_speed=None, keys_ms=None):
        """``action`` sampled over ``span_ms`` (loops: t = ms / length; one-shots: the web track runs to the last
        frame at ``span_ms`` = (frames − 1) / fps, then holds)."""
        c = Clip(name, length_ms, loop=loop, notifies=notifies or {}, ref_speed=ref_speed)

        def fn(ms):
            t = (ms / length_ms) % 1.0 if loop else min(1.0, ms / span_ms)
            return self.basis(fly_pose(action, t), t)
        c.basis_fn = fn
        c.sampler = lambda t: None
        c.key_poses_ms = keys_ms or [length_ms * i / 5 for i in range(6)]
        return c

    def all_clips(self):
        idle = self.clip("Idle", 667.0, "idle", 667.0, loop=True, keys_ms=[0, 111, 222, 333, 500, 556])
        walk = self.clip("Walk", 600.0, "walk", 600.0, loop=True, ref_speed=REF_SPEED,
                         keys_ms=[0, 75, 150, 225, 300, 450])
        atk = self.clip("Attack01", 333.0, "attack", 250.0, notifies={"Contact": CONTACT_MS},
                        keys_ms=[0, 83, 167, 250, 333])
        cast = self.clip("Cast01", 400.0, "cast", 300.0, notifies={"Release": RELEASE_MS},
                         keys_ms=[0, 100, 200, 300, 400])
        hurt = self.clip("Hurt", 200.0, "hurt", 100.0, keys_ms=[0, 50, 100, 200])
        return [idle, walk, atk, cast, hurt]


# ── glows (runtime cards in the previews) ───────────────────────────────────────────────────────────────
AURA = ("body", C0, 13.0 + 2 * STAGE, "#C8FF90", 0.32 + 0.05 * STAGE)   # web glow(c, 13 + 2·stage, .32 + .05·stage): BEHIND
TAIL_GLOW = ("tail_04", None, 4.0, "#E8FF90", 0.6)   # web glow(tip, 4, 0xe8ff90, .6), behind the tip (see glows)


def glows(rig, aura: bool = True):
    """Runtime glow cards: the body aura and the tail-tip glow, both **behind** the pet (UE: depth-tested additive
    cards pushed a little further from the camera, like the elite crown's halo): their light shows round the
    silhouette, never over the body or its ink — on a 60 px creature the web's over-the-tip card washed the ink off a
    tenth of the rim. Previews composite them with ``BehindGlow`` (``elite_crown.halo_view``'s coverage mask);
    the portrait lays them under the creature. ``aura=False``: the tail glow only."""
    def wp(bone, pos):
        b = rig.obj.data.bones[bone]
        pb = rig.obj.pose.bones[bone]
        return rig.obj.matrix_world @ (pb.matrix @ (b.matrix_local.inverted() @ Vector(pos)))

    def fn(cam, res):
        out = []
        cards = [TAIL_GLOW] + ([AURA] if aura else [])
        for bone, pos, r_u, col, a in cards:
            pos = tail_rest(1.0) if pos is None else pos
            p = wp(bone, pos)
            px, py = review.project_px(cam, p, res)
            wpp = outline.pixel_world_size(cam, p, res[0])
            out.append(dict(center_px=(px, py), radius_px=r_u * U / wpp, color=col, alpha=a, behind=True))
        return out
    return fn


class BehindGlow:
    """Mixin for a ``review.Review`` (or ``mship.MReview``): glow cards flagged ``behind`` are composited only where
    the pet's body + ink do not cover the pixel (coverage pass of the same crop), the rest as the kit draws them.
    Same renders and gate as the base class otherwise."""

    def post(self, img, cam, res, border=None, glow: bool = True, bloom_on=None):
        cards = self.glows(cam, res) if (glow and self.glows is not None) else []
        if not any(g.get("behind") for g in cards):
            return super().post(img, cam, res, border, glow=glow, bloom_on=bloom_on)
        out = super().post(img, cam, res, border, glow=False, bloom_on=bloom_on)
        body, hull = review.coverage_pass(self.meshes, border, extra_body=getattr(self, "extra_body_slots", False))
        cov = np.clip(body + hull, 0.0, 1.0)[:, :, None]
        x0, y0 = review.border_origin(border, res)
        for g in cards:
            a = g.get("alpha", 0.0) * self.glow_scale
            if a <= 0.0:
                continue
            c = (g["center_px"][0] - x0, g["center_px"][1] - y0)
            lit = review.glow_card(out, c, g["radius_px"], g["color"], a, g.get("core"), g.get("core_frac", 0.35),
                                   g.get("core_alpha", 0.0) * self.glow_scale)
            if g.get("behind"):
                lit = (lit.astype(np.float32) * (1.0 - cov) + out.astype(np.float32) * cov + 0.5).astype(np.uint8)
            out = lit
        return out


FX = {
    "aura": {"socket": "fx_chest", "color": "#C8FF90", "alpha": round(0.32 + 0.05 * STAGE, 2),
             "radiusCm": round((13 + 2 * STAGE) * U * 100, 1),
             "blend": "additive", "falloff": "web glow", "placement": "behind the pet (camera-facing card pushed "
             "20 cm away from the camera, depth-tested: a halo round the silhouette that never covers the body or "
             "its ink)", "perStage": "+2 u radius and +.05 alpha per evolution stage"},
    "tailGlow": {"socket": "fx_tail", "color": "#E8FF90", "alpha": 0.6, "radiusCm": round(4 * U * 100, 1),
                 "placement": "behind the tip (depth-tested card pushed 5 cm from the camera: light round the inked "
                 "tip; the emissive tip itself blooms)",
                 "sparkles": {"color": "#FAFFD0", "count": 3, "trail": "behind the tip, fading"}},
    "castHeal": {"notify": "Release", "ring": "#D2FFAA", "glow": "#A8FF80", "alphaPerGlow": 0.45,
                 "radiusCm": [round(6 * U * 100, 1), round(15 * U * 100, 1)], "sparkles": 5},
    "attackZap": {"notify": "Contact", "socket": "fx_hand_r", "color": "#D0FF80", "alpha": 0.7,
                  "radiusCm": round(7 * U * 100, 1), "projectile": "proj.pet_bolt"},
    "inkColor": "#1A3A1C (web pet ink; the 3D hull mixes #120C18 with the local line tone at 60 % ink)",
}
if STAGE > 0:   # web Sprite.ts fx: orbiting motes (stage 1: 2, stage 2: 3) and the stage-2 halo glow
    FX["motes"] = {"count": 2 if STAGE == 1 else 3, "orbitRadiusCm": round(10 * U * 100, 1), "periodMs": "the clip loop",
                   "glow": {"color": "#E0FF90", "alpha": 0.7, "radiusCm": round(3 * U * 100, 1)}, "core": "#FBFFE0",
                   "path": "cos/sin(a) * 10 u round the body, height bob sin(2a) * 1.5 u"}
if STAGE == 2:
    FX["haloGlow"] = {"socket": "sprout", "color": "#FFE890", "alpha": 0.35, "radiusCm": round(5 * U * 100, 1),
                      "note": "over the emissive SM halo ring (mesh part 'halo', no hull)"}


class _A:
    def __init__(self, rig):
        self.rig = rig


def build_all():
    scene.reset()
    pal = Palette(FAMILY)
    rig = build_rig()
    body = build_body(pal, rig)
    return pal, rig, body


def portrait(ms, rv, actions) -> str:
    """``T_UI_Portrait_Pet_Sprite`` (pets panel, ``AbyssUiContext::PetPortrait``): 512² RGBA, 3/4 front, the whole
    little creature (it is all head), toon + screen-space ink (``review.ink_render``), the body aura and tail glow
    composited (no bloom on a UI texture); ``portrait.png`` / ``portrait_zoom2x.png`` in the preview set."""
    import mship
    rig = ms.A.rig
    acts = dict(zip([c.name for c in ms.clips], actions))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(10)
    rv.facing(-110.0)
    bpy.context.view_layer.update()
    res = (512, 512)
    review.hide_stage(True)
    review.setup_render(res, 16)
    vis = [ms.body]
    pts = review.mesh_points(vis)
    cam = review.frame_points(pts, res, fov_h=24, pitch_ue=-14.0, fill=0.92, include_ground=False)
    for m in vis:
        outline.remove_preview(m)
    rgba = review.ink_render(vis, res, silhouette_px=mship.PORTRAIT_INK_PX, interior_px=2.0, depth_step=0.012)
    cards = glows(rig, aura=True)(cam, res)
    halo = np.zeros_like(rgba)                       # the aura under the creature (a halo card behind the body)
    for g in cards:
        if g.get("behind"):
            review.composite_glow(halo, g["center_px"], g["radius_px"], g["color"], g["alpha"], core=0.25)
    a = rgba[:, :, 3:4].astype(np.float32) / 255.0
    ha = halo[:, :, 3:4].astype(np.float32) / 255.0
    out_a = a + ha * (1 - a)
    rgb = (rgba[:, :, :3] * a + halo[:, :, :3] * ha * (1 - a)) / np.maximum(out_a, 1e-6)
    rgba = np.concatenate([rgb, out_a * 255.0], axis=2).clip(0, 255).astype(np.uint8)
    for g in cards:
        if not g.get("behind"):
            review.composite_glow(rgba, g["center_px"], g["radius_px"], g["color"], g["alpha"], core=0.25)
    name = "T_UI_Portrait_Pet_Sprite"
    outp = paths.export_root() / "Portraits" / f"{name}.png"
    outp.parent.mkdir(parents=True, exist_ok=True)
    pngio.write_png(outp, rgba)
    review.hide_stage(False)
    bg = np.array([59, 53, 70], np.float32)
    a = rgba[:, :, 3:4].astype(np.float32) / 255.0
    comp = (rgba[:, :, :3].astype(np.float32) * a + bg * (1 - a) + 0.5).astype(np.uint8)
    review.draw_text(comp, 4, 4, name.upper())
    rv._save("portrait.png", comp)
    z = comp[96:352, 128:384].repeat(2, 0).repeat(2, 1)
    review.draw_text(z, 4, 4, "PORTRAIT 2X ZOOM")
    rv._save("portrait_zoom2x.png", z)
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)
    rv.facing("se")
    print(f"[portrait] {outp}")
    return f"Portraits/{name}.png"


def ship(previews: bool = True, verify: bool = True) -> int:
    import mship
    pal, rig, body = build_all()
    body1 = build_body(pal, rig, lod=1)
    S = SpriteAnims(rig)
    cs = S.all_clips()
    ms = mship.MonsterShip(
        asset=ASSET, token=TOKEN, preview_dir=PREVIEW_DIR, pal=pal, A=_A(rig), body=body, items=[], clips=cs,
        outline_class=OUTLINE_CLASS, game_ids=GAME_IDS, blob_radius=round(8 * U * 1.05, 3),
        height=HEIGHT, display=DISPLAY, fx=FX, glows=glows(rig),
        qa_limits={"ground": -0.5}, category=CATEGORY,
        game_keys=[("Idle", 0, "se"), ("Idle", 24, "se"), ("Walk", 9, "se"), ("Walk", 27, "se"),
                   ("Attack01", 8, "se"), ("Attack01", 15, "se"), ("Cast01", 12, "se"), ("Hurt", 0, "se")],
        contact_keys=[("Attack01", 15, "se"), ("Attack01", 15, "side"), ("Cast01", 12, "se"), ("Cast01", 12, "front")],
        close_keys=[("Idle", 0, "front"), ("Idle", 45, "se"), ("Walk", 9, "side"), ("Attack01", 5, "se"),
                    ("Attack01", 15, "se"), ("Cast01", 12, "se"), ("Cast01", 12, "back"), ("Hurt", 0, "se"),
                    ("Hurt", 0, "ne"), ("Walk", 20, "back")],
        back_clips=("Attack01", "Cast01"),
        skeleton_bones_required=("root", "body", "tail_01", "tail_02", "tail_03", "tail_04", "wing_l_01", "wing_l_02",
                                 "wing_r_01", "wing_r_02", "arm_l", "arm_r", "sprout", "eye_l", "eye_r", "mouth",
                                 "mouth_o"),
        extra={"refSpeedCmS": REF_SPEED * 100.0, "hoverCm": round(C0.z * 100, 1), "flyer": True,
               "stage": STAGE, "supersededBy": SUPERSEDED_BY, "legacyGameIds": LEGACY_GAME_IDS,
               **({"evolvedLooks": {"beast_pet_sprite_e1": "SK_Pet_Sprite_E1",
                                                     "beast_pet_sprite_e2": "SK_Pet_Sprite_E2"}} if STAGE == 0 else
                                  {"evolvedFrom": "SK_Pet_Sprite" if STAGE == 1 else "SK_Pet_Sprite_E1",
                                   "sharesClipsWith": "SK_Pet_Sprite (same SKEL_Pet_Sprite bones and clip set)"}),
               "notes": "DECISIONS Q3 Chapter-1 ley-beast companion. SKEL_Pet_Sprite: hover / bob / lunge on body, "
                        "wisp tail tail_01..04, four flapping wings, arms, sprout sway, blink (eye scale), smile / "
                        "'o' mouth. Clips in place (the core moves the pet; play rate = v / 4.2 m/s). No Death: the "
                        "exhausted pet idles at 0.6x. Contact 250 / Release 200 = PET_ATTACK_CONTACT / "
                        "PET_CAST_RELEASE_FRAME at the web frame rates."},
        lod_note="Pet companion budget (no §1.9 row; the slime's small-creature row): LOD0 <= 4k toon tris, LOD1 "
                 "<= 2k, 17 bones.",
        lod1=body1, portrait=None,           # T_UI_Portrait_Pet_Sprite is the Lanternwisp's now (SUPERSEDED_BY)
        outline_px=OUTLINE_PX, outline_px_note=OUTLINE_PX_NOTE,
    )
    if not issubclass(mship.MReview, BehindGlow):      # this process only: the pet's cards sit behind it
        mship.MReview = type("PetReview", (BehindGlow, mship.MReview), {})
    res = mship.ship(ms, previews=previews, verify=verify)
    return mship.main_result(res)


def quick(out: Path, clip: str | None = None, frame: int = 0) -> None:
    pal, rig, body = build_all()
    S = SpriteAnims(rig)
    out.mkdir(parents=True, exist_ok=True)
    if clip:
        c = next(c for c in S.all_clips() if c.name == clip)
        anim.bake_basis(rig, c, TOKEN, c.basis_fn)
        anim.set_frame(frame)
    rv = type("PetQuickReview", (BehindGlow, review.Review), {})(ASSET, rig.obj, [body], HEIGHT, blob_radius=8 * U, out_dir=out,
                       game_outline_px=OUTLINE_PX, outline_class=OUTLINE_CLASS,
                       glows=glows(rig))
    rv.closeup("se", pitch=-25.0)
    (out / "closeup.png").rename(out / "closeup_se.png")
    rv.closeup("front", pitch=-20.0)
    (out / "closeup.png").rename(out / "closeup_front.png")
    rv.turnaround()
    rv.game_view()
    for p, s in rv.written:
        print(f"  {p.name} {s // 1024} KB")
    from kit import mesh
    print(f"  tris toon {mesh.tri_count(body, 0)} hull {mesh.tri_count(body, 1)}")


def anim_quick(out: Path, names) -> None:
    import mship
    pal, rig, body = build_all()
    S = SpriteAnims(rig)
    cs = [c for c in S.all_clips() if not names or c.name in names]
    mship.anim_review(rig, [body], [], cs, TOKEN, out, HEIGHT, OUTLINE_CLASS, blob_radius=8 * U)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    scratch = Path(os.environ.get("AF_SCRATCH_ROOT", tempfile.gettempdir())) / "abyss_art_pet"   # --quick / --anims
    if "--quick" in args:
        k = args.index("--quick")
        rest = [a for a in args[k + 1:] if not a.startswith("--")]
        quick(scratch, rest[0] if rest else None, int(rest[1]) if len(rest) > 1 else 0)
    elif "--anims" in args:
        names = [a for a in args[args.index("--anims") + 1:] if not a.startswith("--")]
        anim_quick(scratch / "anims", names)
    else:
        sys.exit(ship(previews="--no-previews" not in args, verify="--no-verify" not in args))

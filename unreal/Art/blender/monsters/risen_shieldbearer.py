"""复起盾卒 Risen Shieldbearer — ``SK_Mon_RisenShieldbearer`` (dark remake monsters.md §2.2, Unburied kit variant;
implementation-plan.md A2).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/risen_shieldbearer.py
        (no flag) ship · --quick [dir] · --no-previews · --no-verify

The shield wall: a dead crusader in a mail coif behind a tall tower shield (``SM_MonWeapon_TowerShield``, new held item
on ``weapon_l``: a crimson field with the scorched open-palm sigil, ash-streaked, an iron rim, planks and straps behind)
with a flanged mace (``SM_MonWeapon_Mace``: the hero ``SM_Weapon_Mace`` generator re-used in the Unburied's dull iron).
The shield reads from the W1 camera as one flat slab covering the left side of the body (monsters.md).

Behaviour (monsters.md): L7, speed 30 (0.90 m/s), very high defence; Attack01 a shield bash — Contact 500 ms
(WindupEnd 310). [W2] frontal block (100° arc, ``EvBlock`` sparks on the shield face: socket ``face``) and the Wall.
Idle_Order: the line holds — he braces behind the shield and raps its rim twice with the mace.
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "items", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import mesh as M, rig as R, scene  # noqa: E402
from kit.palette import Palette  # noqa: E402
from kit.paths import scratch_root  # noqa: E402

import common as C  # noqa: E402
from common import BP, K, V  # noqa: E402
import mgeo as G  # noqa: E402
import unburied as UB  # noqa: E402
import risen_footman as RF  # noqa: E402
from uclips import UnburiedAnims  # noqa: E402

ASSET = "SK_Mon_RisenShieldbearer"
TOKEN = "Mon_RisenShieldbearer"
PREVIEW = "mon_risen_shieldbearer"
GAME_IDS = ["monster_risen_shieldbearer"]
DISPLAY = {"zh-CN": "复起盾卒", "en": "Risen Shieldbearer"}
SHIELD = "SM_MonWeapon_TowerShield"
MACE = "SM_MonWeapon_Mace"
REF_SPEED = 30 * 0.03
CONTACT_MS = 500.0
ATTACK_MS = 1000.0
WALK = dict(RF.WALK, cycle=1200.0, drag_lift=0.30, limp=0.016)
LOOK = UB.ULook(coif=True, arrows=((0.62, 178.0, 12.0, 0.26, True),), eye_scale=1.2)
BLOB = 0.34
MACE_HEAD_Y, MACE_HEAD_R = 0.486, 0.075      # the mace head's centre along the haft and its radius with the flanges
SH_W, SH_TOP, SH_BOT, SH_FACE = 0.56, 0.50, -0.52, 0.115     # tower shield (weapon_l space: face +Y, up +Z)


# ── the tower shield ───────────────────────────────────────────────────────────────────────────────────────────
def shield_y(x: float, z: float) -> float:
    """Face depth of the board: bowed round the bearer (edges 9 cm back), a slight vertical camber."""
    return SH_FACE - 0.09 * (x / (SH_W / 2)) ** 2 - 0.015 * ((z - (SH_TOP + SH_BOT) / 2) / ((SH_TOP - SH_BOT) / 2)) ** 2


def shield_top(x: float) -> float:
    return SH_TOP + 0.05 * (1.0 - (x / (SH_W / 2)) ** 2)


SHIELD_CLOTH = ("sigil", "sigil_finger", "ash", "scorch")     # paint on the field: on the cloth slot with it


def build_shield(pal: Palette, lod: int = 0) -> tuple[bpy.types.Object, list]:
    """Tower shield in weapon_l space (the fist at the origin round a vertical handle, the face toward +Y, up +Z):
    a 56 × 107 cm board bowed round the bearer, crimson field (front) / planks (back) / dark rim, the scorched
    open-palm sigil, two ash streaks, an iron band, the handle and the forearm strap behind. The crimson field and
    its paint are on the crimson-cloth slot (``UB.CLOTH_MATERIAL``, the rim overridden: art review r2 — the cream
    rim turned the board a pale peach seen edge-on, SIDE_L and NW Death F54)."""
    b = UB.att_builder(SHIELD, pal, lod, prefix="towershield")
    b.use("towershield.field", "towershield.ash", "towershield.sigil", "towershield.rim", "towershield.back",
          "unburied.leather_dark", "unburied.tabard_char")
    b.use_appended("towershield.face")
    b.swatch["field"] = b.swatch["towershield.face"]      # the field's narrower light band (UB.APPENDED)
    cols, rows = 10, 8
    th = 0.030
    outer, inner = [], []
    for r in range(rows + 1):
        ro, ri = [], []
        for c in range(cols + 1):
            x = -SH_W / 2 + SH_W * c / cols
            z = SH_BOT + (shield_top(x) - SH_BOT) * r / rows
            y = shield_y(x, z)
            ro.append(Vector((x, y + th / 2, z)))
            ri.append(Vector((x, y - th / 2, z)))
        outer.append(ro)
        inner.append(ri)
    board = C.thick_patch(outer, inner, sub_tags=True)
    bid = b.add(board, "field", name="shield_board", sub_regions={1: "field", 2: "back", 0: "rim"}, hull_pad=True)
    lift = th / 2 + 0.0015
    # iron rim round the face
    edge = []
    for c in range(cols + 1):
        x = -SH_W / 2 + SH_W * c / cols
        edge.append((x, shield_top(x)))
    edge += [(SH_W / 2, SH_BOT), (-SH_W / 2, SH_BOT), (-SH_W / 2, shield_top(-SH_W / 2))]
    pts = []
    for (x0, z0), (x1, z1) in zip(edge, edge[1:]):
        for k in range(4):
            x, z = x0 + (x1 - x0) * k / 4, z0 + (z1 - z0) * k / 4
            pts.append(Vector((x * 0.985, shield_y(x, z) + 0.004, z if z > SH_BOT + 0.01 else SH_BOT + 0.008)))
    pts.append(pts[0])
    b.add(M.tube(pts, [0.011] * len(pts), sides=5), "rim", name="shield_rim", outline=False, covered_by=bid)
    # an iron band across the upper third
    zb = SH_BOT + 0.70 * (SH_TOP - SH_BOT)
    band = [Vector((x, shield_y(x, zb) + lift, zb)) for x in np.linspace(-SH_W / 2 * 0.97, SH_W / 2 * 0.97, 9)]
    b.add(M.tube(band, [0.012] * len(band), sides=4), "rim", name="shield_band", outline=False, covered_by=bid)

    class Face:
        def point(self, u, a, out=0.0):            # u = z, a = x (m)
            return Vector((a, shield_y(a, u) + lift + out, u))

        def normal(self, u, a):
            return Vector((0, 1, 0))
    face = Face()
    # the scorched open palm (25 cm), centred a little above the middle
    hc, wc = SH_BOT + 0.47 * (SH_TOP - SH_BOT), 0.0
    b.add(G.surface_patch(face, hc - 0.07, hc + 0.035, wc - 0.055, wc + 0.055, 0.0, nu=2, na=4,
                          shape=lambda tu, ta: 0.8 + 0.2 * math.sin(math.pi * tu)), "sigil", name="sigil",
          outline=False, covered_by=bid)
    for k, dx in enumerate((-0.040, -0.0135, 0.0135, 0.040)):
        ln = (0.085, 0.105, 0.098, 0.078)[k]
        b.add(G.surface_paint(face, [(hc + 0.03, wc + dx), (hc + 0.03 + ln * 0.5, wc + dx * 1.05),
                                     (hc + 0.03 + ln, wc + dx * 1.1)], 0.021, 0.0, steps=3, taper=0.25),
              "sigil", name="sigil_finger", outline=False, covered_by=bid)
    b.add(G.surface_paint(face, [(hc - 0.04, wc + 0.05), (hc + 0.0, wc + 0.085), (hc + 0.04, wc + 0.105)], 0.021,
                          0.0, steps=3, taper=0.3), "sigil", name="sigil_finger", outline=False, covered_by=bid)
    # ash streaks running down from the top edge, a charred lower corner
    for x0, z0, ln in ((-0.17, SH_TOP - 0.02, 0.42), (0.19, SH_TOP - 0.06, 0.30), (0.07, SH_TOP - 0.03, 0.18)):
        b.add(G.surface_paint(face, [(z0, x0), (z0 - ln * 0.5, x0 + 0.01), (z0 - ln, x0 - 0.005)], 0.030, 0.0004,
                              steps=4, taper=0.6), "ash", name="ash", outline=False, covered_by=bid)
    b.add(G.surface_patch(face, SH_BOT + 0.02, SH_BOT + 0.17, 0.12, 0.27, 0.0002, nu=2, na=2,
                          shape=lambda tu, ta: 0.4 + 0.6 * (1 - tu)), "unburied.tabard_char", name="scorch",
          outline=False, covered_by=bid)
    # handle (vertical bar in the fist) and the forearm strap above it, behind the board
    yb = shield_y(0.0, 0.0) - th / 2
    handle = M.tube([(0, yb, -0.10), (0, 0.004, -0.07), (0, 0.004, 0.07), (0, yb, 0.10)], [0.014] * 4, sides=6)
    b.add(handle, "unburied.leather_dark", name="handle")
    strap = M.tube([(-0.06, yb, 0.20), (-0.03, yb - 0.035, 0.22), (0.03, yb - 0.035, 0.22), (0.06, yb, 0.20)],
                   [0.012] * 4, sides=5)
    b.add(strap, "unburied.leather_dark", name="strap")
    obj = UB._finish_item(b, pal, SHIELD + ("_LOD1" if lod else ""))
    # the whole board (field, plank back, edge) and its paint: a part split between slots shares vertices across
    # them, and the kit's preview outline (Set Position on material index == 1, interpolated to the points) then
    # took the 0 / 2 seam for hull and bulged the board's edge over its ink (rim gate 72 %)
    UB.cloth_slot(obj, pal, UB.part_ids(b, ("shield_board",) + SHIELD_CLOTH))
    socks = [{"name": "face", "pos": (0.0, SH_FACE + 0.02, (SH_TOP + SH_BOT) / 2)},
             {"name": "rim_top", "pos": (0.0, SH_FACE, SH_TOP + 0.05)}]
    return obj, socks


def build_mace(pal: Palette, lod: int = 0) -> tuple[bpy.types.Object, list]:
    """The hero flanged mace (``items/weapons.build_mace``: leather-wrapped haft, a ball head with rhombic spikes,
    a capped top) in the Unburied's dull iron and dark wood — no gold."""
    import weapons as WP
    b = UB.att_builder(MACE, pal, lod, prefix="unburied")
    b.alias(haft="standard.pole", wrap="unburied.leather_dark", butt="unburied.helm_dent",
            collar="unburied.helm_dent", head="towershield.rim", cap="unburied.helm_dent", flange="towershield.rim")
    socks = WP.build_mace(b)
    obj = UB._finish_item(b, pal, MACE + ("_LOD1" if lod else ""))
    return obj, socks


# ── poses ──────────────────────────────────────────────────────────────────────────────────────────────────────
def shield_dir(lat: float, fwd: float, up: float = 0.0):
    d = V(lat, fwd, up).normalized()
    u = Vector((0, 0, 1)) - d * d.z
    return {"off": d, "off_up": u.normalized()}


def ready(A: UnburiedAnims) -> BP:
    """READY: hunched behind the tower shield (held before the left side, its face turned forward and a little out),
    the mace hanging from the right fist, head up over the rim, tilted on the broken neck."""
    J = A.J
    md, mu = C.blade(136.0, lat=-0.36)
    return BP(pelvis=V(0.0, -0.02, -0.05), lean=11.0, hip_pitch=3.0, side=-2.0, twist=-6.0, hip_yaw=-4.0,
              head_pitch=-40.0, head_roll=14.0, head_yaw=-9.0,
              foot_l=J["ankle_l"] + V(0.03, 0.08, 0.0), foot_r=J["ankle_r"] + V(-0.03, -0.08, 0.0),
              foot_l_rot=(0.0, 0.0, 10.0), foot_r_rot=(14.0, 0.0, -12.0), knee_l=V(0.25, 1, 0), knee_r=V(-0.25, 1, 0),
              hand_r=V(-0.26, 0.10, 0.80), elbow_r=V(-0.9, -0.3, -0.2), wpn=md, wpn_up=mu,
              hand_l=V(0.21, 0.36, 0.95), elbow_l=V(0.9, -0.2, -0.4), **shield_dir(0.30, 0.95),
              clav_l=(-6.0, 6.0), clav_r=(2.0, 0.0), flow=0.1, off_fit=1.0)


def keys(A: UnburiedAnims) -> dict:
    """The shield bash: draw the shield in against the shoulder, twist away, then drive it forward with a step onto
    the left foot (Contact 500 ms), the mace held back."""
    R0 = A.ready()
    J = A.J
    wind = R0.but(lean=4.0, twist=R0.twist - 16.0, hip_yaw=-12.0, head_pitch=-26.0, hand_l=V(0.22, 0.28, 1.10),
                  elbow_l=V(0.9, 0.1, -0.4), **shield_dir(0.10, 1.0), hand_r=V(-0.28, -0.02, 0.86),
                  pelvis=R0.pelvis + V(0, -0.03, 0.0), clav_l=(4.0, -8.0), flow=0.25)
    hold = wind.but(twist=R0.twist - 20.0, hand_l=V(0.22, 0.26, 1.12))
    hit = R0.but(pelvis=R0.pelvis + V(0, 0.10, -0.05), lean=22.0, hip_pitch=6.0, twist=R0.twist + 14.0, hip_yaw=8.0,
                 head_pitch=-40.0, hand_l=V(0.12, 0.62, 1.04), elbow_l=V(0.9, -0.3, -0.3), **shield_dir(0.0, 1.0, 0.05),
                 hand_r=V(-0.30, -0.02, 0.88), foot_l=J["ankle_l"] + V(0.03, 0.30, 0.0), clav_l=(2.0, 14.0),
                 flow=0.45)
    follow = hit.but(hand_l=hit.hand_l + V(0.0, 0.03, -0.02), lean=24.0)
    recover = R0.but(pelvis=R0.pelvis + V(0, 0.05, -0.04), lean=16.0, hand_l=V(0.18, 0.42, 0.98),
                     foot_l=J["ankle_l"] + V(0.03, 0.18, 0.0))
    return dict(wind=wind, hold=hold, hit=hit, follow=follow, recover=recover, lift_ms=190.0, follow_ms=110.0,
                recover_ms=240.0,
                # dying: the shield planted on its foot before the knees …
                kneel_off=V(0.25, 1.0, 0.05), kneel_off_up=V(0.0, -0.05, 1.0),
                kneel_hand_l=V(0.36, 0.46, 0.58), kneel_elbow_l=V(0.9, 0.0, -0.3),
                buckle_hand_l=V(0.36, 0.48, 0.74), buckle_elbow_l=V(0.9, -0.1, -0.4), buckle_off=V(0.30, 1.0, -0.15),
                buckle_off_up=V(0.0, 0.15, 1.0),
                # … toppling back onto the strapped arm: it lies face up beside him, the scorched palm to the sky
                fold_off=V(0.10, 0.22, 0.97), fold_off_up=V(0.30, 0.95, -0.20), fold_hand_l=V(0.50, 0.60, 0.07),
                fold_wpn=V(-0.62, 0.78, -0.04))


def idle_order(A: UnburiedAnims):
    """Idle_Order 2400 ms: the line holds — he braces behind the shield (knees bent, the board squared to the front),
    draws the mace back and raps the board's inner rim twice with a flat swing (``FX_ShieldRap``: the head stops a
    hand short of the iron, the clang is the notify), and settles back."""
    R0 = A.ready()
    # the brace: the shield's foot planted on the ground before the left foot, crouched behind it
    brace = R0.but(pelvis=R0.pelvis + V(0, -0.02, -0.15), lean=24.0, hip_pitch=8.0, twist=-12.0, head_pitch=-46.0,
                   hand_l=V(0.15, 0.42, -SH_BOT + 0.03), elbow_l=V(0.9, -0.2, -0.3), **shield_dir(0.10, 1.0),
                   clav_l=(2.0, 10.0), flow=0.2)
    # the board's inner (right-hand) edge in the brace: the mace swung in flat from the right onto its iron rim
    edges = [A.item_point(brace, "weapon_l", (x, shield_y(x, 0.30), 0.30)) for x in (-SH_W / 2, SH_W / 2)]
    rim = min(edges, key=lambda v: v.x)                      # body right = −X
    d = V(0.86, 0.48, -0.16).normalized()                    # the haft flat across, the head leading
    head = rim + V(-(MACE_HEAD_R + 0.05), -0.03, 0.0)
    rap = brace.but(hand_r=head - d * MACE_HEAD_Y, elbow_r=V(-0.6, -0.4, -0.7), wpn=d, wpn_up=V(0.0, 0.0, 1.0),
                    twist=-4.0, flow=0.25)
    up = V(-0.45, -0.35, 0.82).normalized()
    lift = brace.but(hand_r=V(-0.40, 0.12, 1.06), elbow_r=V(-0.9, -0.3, -0.3), wpn=up, wpn_up=V(0.0, 1.0, 0.0),
                     twist=-18.0, flow=0.3)
    keys_ = [K(0.0, R0), K(400 / 2400, brace, "out"), K(700 / 2400, lift, "smooth"), K(860 / 2400, rap, "in"),
             K(1060 / 2400, lift, "out"), K(1220 / 2400, rap, "in"), K(1500 / 2400, lift, "out"),
             K(1850 / 2400, brace, "smooth"), K(1.0, R0, "smooth")]
    return A.one_shot("Idle_Order", 2400.0, keys_, notifies={"FX_ShieldRap": [860.0, 1220.0]},
                      usage="idle variant (monsters.md 'they keep their last order'): the line holds — braces and raps "
                            "the shield rim; one-shot from READY back to READY")


def anims(kit) -> UnburiedAnims:
    return UnburiedAnims(kit, REF_SPEED, WALK, ready, CONTACT_MS, ATTACK_MS, keys)


def clips(A: UnburiedAnims) -> list:
    return [A.idle(), idle_order(A), A.walk(), A.attack(), A.hurt(), A.death(), A.rise(), A.stun()]


# ── build ──────────────────────────────────────────────────────────────────────────────────────────────────────
def build_all(lod1: bool = False):
    scene.reset()
    pal = Palette(UB.FAMILY)
    kit = UB.UnburiedKit(ASSET, LOOK)
    body = kit.build(pal)
    body1 = RF.build_lod1(kit, pal) if lod1 else None
    helm = UB.build_kettle_helm(pal, kit)
    hood = UB.build_hood(pal, kit)
    shield, shield_socks = build_shield(pal)
    mace, mace_socks = build_mace(pal)
    return dict(pal=pal, kit=kit, body=body, body1=body1, helm=helm, hood=hood, shield=shield,
                shield_socks=shield_socks, mace=mace, mace_socks=mace_socks)


def dev_items(B, key: str) -> list:
    head = {"hood": B["hood"], "bare": None}.get(key, B["helm"])
    return [(B["mace"], "weapon_r"), (B["shield"], "weapon_l")] + ([(head, "head")] if head is not None else [])


def quick(out: Path) -> None:
    B = build_all()
    A = anims(B["kit"])
    rig = B["kit"].rig
    for o, bone in dev_items(B, "helm"):
        R.attach_to_bone(o, rig, bone)
    A.bind_meshes(B["body"], {"weapon_r": B["mace"], "weapon_l": B["shield"]})
    UB.dev_review(ASSET, rig, [B["body"], B["mace"], B["shield"], B["helm"]], A, out, 1.80, glows=RF.eye_glows(rig),
                  blob=BLOB)


# ── ship ───────────────────────────────────────────────────────────────────────────────────────────────────────
GRIP_PARTS = ("fist_r", "thumb_r", "knuckle", "wrist_r")
QA_GROUPS = dict(RF.QA_GROUPS)
QA_PAIRS = dict(RF.QA_PAIRS)


def mace_grip(co) -> bool:
    return -0.21 < co.y < 0.09


def shield_grip(co) -> bool:
    """The handle and strap behind the board (in the fist / round the forearm)."""
    return co.y < 0.06 and abs(co.x) < 0.08 and -0.13 < co.z < 0.25


def qa_setup(lod1: bool = False):
    import mship
    B = build_all(lod1=lod1)
    kit, body = B["kit"], B["body"]
    rig = kit.rig
    A = anims(kit)
    R.attach_to_bone(B["mace"], rig, "weapon_r")
    R.attach_to_bone(B["shield"], rig, "weapon_l")
    R.attach_to_bone(B["helm"], rig, "head")
    A.bind_meshes(body, {"weapon_r": B["mace"], "weapon_l": B["shield"]})
    cs = clips(A)
    items = [mship.Item(B["mace"], MACE, "weapon_r", "mace", sockets=B["mace_socks"],
                        extra={"note": "the hero SM_Weapon_Mace generator re-used in the Unburied palette "
                                       "(monsters.md: re-used SM_Weapon_Mace)"}, grip=mace_grip),
             mship.Item(B["shield"], SHIELD, "weapon_l", "shield", sockets=B["shield_socks"],
                        extra={"sizeCm": [round(SH_W * 100), round((SH_TOP + 0.05 - SH_BOT) * 100)],
                               "note": "monsters.md §2.2: a tall tower shield, crimson field with the scorched sigil, "
                                       "ash-streaked; the block sparks (EvBlock, W2) on the face socket",
                               "materialNote": f"slot 2 {UB.CLOTH_MATERIAL}: the crimson field and its paint on the "
                                               f"palette toon with RimOverride 1, RimOverrideColor {UB.CLOTH_RIM_HEX}, "
                                               f"RimOverrideAlpha {UB.CLOTH_RIM_ALPHA} (the cream rim turned the "
                                               "board pale peach edge-on); RimOverrideColor is sRGB",
                               **UB.item_slot_extra(B["shield"], B["pal"])},
                        grip=shield_grip, preview_up="z"),
             UB.head_item(B["helm"], UB.HELM, None),
             UB.head_item(B["hood"], UB.HOOD, "look_hood")]
    contact_f = scene.ms_to_frame(CONTACT_MS)
    windup_f = scene.ms_to_frame(A.windup_ms)
    body_nf = mship.parts_except(body, GRIP_PARTS)
    body_nl = mship.parts_except(body, ("hand_l", "wrist_l", "cuff_l", "arm_l", "knuckle"))
    body_nohead = mship.parts_except(body, UB.HEAD_PARTS + ("neck",))
    weapons = [{"id": "mace_shield", "weight": 1,
                "attachments": [{"object": MACE, "socket": "weapon_r"}, {"object": SHIELD, "socket": "weapon_l"}],
                "idleOrder": "Idle_Order"}]
    ms = mship.MonsterShip(
        asset=ASSET, token=TOKEN, preview_dir=PREVIEW, pal=B["pal"], A=A, body=body, items=items, clips=cs,
        outline_class=UB.OUTLINE_CLASS, game_ids=GAME_IDS, blob_radius=BLOB, height=1.80, lod1=B["body1"],
        display=DISPLAY, fx={"eyeGlow": RF.EYE_FX,
                             "telegraph": {"normal": "#FFE8DC", "elite": "#D11F2E", "untilMs": A.windup_ms,
                                           "note": "dark remake: bone fill / crimson rim (monsters.md §1.2)"},
                             "attackTrail": {"color": "#C8C4BC", "alpha": 0.35, "sockets": ["rim_top", "face"],
                                             "item": SHIELD},
                             "block": {"socket": "face", "item": SHIELD, "arcDeg": 100.0,
                                       "note": "[W2] frontal block sparks (EvBlock) on the shield face"}},
        glows=RF.eye_glows(rig),
        qa_groups={"body_nf": body_nf, "body_nl": body_nl, "body_nohead": body_nohead, **QA_GROUPS},
        qa_pairs={"maceBody": ("item:" + MACE, "body_nf"), "shieldBody": ("item:" + SHIELD, "body_nl"),
                  "maceShield": ("item:" + MACE, "item:" + SHIELD),
                  "helmBody": ("item:" + UB.HELM, "body_nohead"), "hoodBody": ("item:" + UB.HOOD, "body_nohead"),
                  "helmShield": ("item:" + UB.HELM, "item:" + SHIELD), **QA_PAIRS},
        qa_limits={"ground": -0.5, "maceBody": 0, "shieldBody": 0, "maceShield": 0, "helmBody": 0, "hoodBody": 0,
                   "helmShield": 0, **{k: 0 for k in QA_PAIRS}},
        outline_px=UB.OUTLINE_PX, outline_px_note=UB.OUTLINE_PX_NOTE, native_sheets=True,
        game_keys=[("Idle", 0, "se"), ("Walk", 10, "se"), ("Attack01", windup_f, "se"), ("Attack01", contact_f, "se"),
                   ("Attack01", contact_f, "sw"), ("Hurt", 6, "se"), ("Death", 54, "se"), ("Rise", 50, "se"),
                   ("Stun", 20, "se"), ("Idle_Order", 52, "se"), ("Idle", 0, "front"), ("Walk", 40, "sw")],
        close_keys=[("Attack01", windup_f, "se"), ("Attack01", contact_f, "se"), ("Attack01", contact_f, "side"),
                    ("Hurt", 6, "se"), ("Death", 20, "se"), ("Death", 31, "se"), ("Death", 54, "se"),
                    ("Rise", 50, "se"), ("Idle_Order", 52, "front"), ("Walk", 20, "side")],
        back_clips=("Attack01", "Death", "Rise"),
        skeleton_bones_required=("root", "pelvis", "head", "jaw", "eye_l", "weapon_r", "weapon_l", "tabard_f_02",
                                 "tabard_b_02", "ik_foot_l", "ik_hand_gun"),
        extra_previews=UB.looks_preview([("helm", [], []), ("bare coif", [], [B["helm"]]),
                                         ("hood", [B["hood"]], [B["helm"]])]),
        variant_previews=("looks.png",),
        lod_note="LOD1: the same kit at a collapse ratio iterated to <= 3 k toon (eyes, sockets and mouth built exact).",
        extra={"refSpeedCmS": round(REF_SPEED * 100.0, 1), "attackStyle": "bash", "family": "unburied",
               "marker": {"swatches": list(UB.MARKER), "note": "violet pinpoint eyes (monsters.md §1.2)"},
               "looks": UB.looks_manifest(weapons, "mace + tower shield"),
               "notes": "Shield wall: the tower shield on weapon_l covers the left side; Attack01 is a shield bash "
                        "(Contact 500). Idle_Order is a one-shot idle variant (brace and rap the rim). Death folds "
                        "forward (ash husk, U3), the shield laid on the ground; Rise = Death reversed. Stun loops."},
    )
    ms.extra["budget"] = UB.budget_record(ms, 7000, 3000)
    pal = B["pal"]

    def shield1():
        o, _ = build_shield(pal, 1)
        return o
    ms.lod1_items = {UB.HELM: lambda: UB.build_kettle_helm(pal, kit, 1), UB.HOOD: lambda: UB.build_hood(pal, kit, 1),
                     MACE: lambda: build_mace(pal, 1)[0], SHIELD: shield1}
    return ms


def ship(previews: bool = True, verify: bool = True) -> int:
    UB.march_stage()
    ms = qa_setup(lod1=True)
    return UB.ship_member(ms, ms.lod1_items, previews=previews, verify=verify)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    scratch = scratch_root("mq", "risen_shieldbearer")
    if "--quick" in args:
        k = args.index("--quick")
        quick(Path(args[k + 1]) if len(args) > k + 1 and not args[k + 1].startswith("--") else scratch)
    else:
        sys.exit(ship(previews="--no-previews" not in args, verify="--no-verify" not in args))

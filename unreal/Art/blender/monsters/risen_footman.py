"""复起步卒 Risen Footman — ``SK_Mon_RisenFootman`` (dark remake monsters.md §2.2, the first member of the Unburied kit;
implementation-plan.md A2).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/risen_footman.py
        (no flag) ship · --quick [dir] · --anims [Clip …] [--facing=se] · --no-previews · --no-verify

The shambler: a dead crusader in a torn crimson tabard over gambeson and mail, hunched, his head hanging on a broken
neck, the violet pinpoint eyes of the breach in dark sockets, dragging his right foot; a notched arming sword
(``SM_MonWeapon_Sword``) or — the burial detail, one in three — a shovel (``SM_MonWeapon_Shovel``). Per-instance head
looks (manifest ``looks``): kettle helm (default attachment), bare cracked scalp, hood.

Behaviour (monsters.md): L2, speed 32 (0.96 m/s = Walk ref speed), Attack01 a slow, heavy overhead chop — Contact
600 ms (WindupEnd 372 = 0.62 × Contact, the core's wind-up share), the default Raised body (``Rise``). Clips: Idle,
Idle_Order (the sentry's last order: shoulder arms, scan the line), Idle_Order_Dig (the burial detail digs), Walk,
Attack01, Hurt, Death (folds forward onto the knees: the ash-husk pose), Rise (Death reversed), Stun.
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402

from kit import outline, rig as R, scene  # noqa: E402
from kit.palette import Palette  # noqa: E402
from kit.paths import scratch_root  # noqa: E402

import common as C  # noqa: E402
from common import BP, K, V  # noqa: E402
import unburied as UB  # noqa: E402
from uclips import UnburiedAnims  # noqa: E402

ASSET = "SK_Mon_RisenFootman"
TOKEN = "Mon_RisenFootman"
PREVIEW = "mon_risen_footman"
GAME_IDS = ["monster_risen_footman"]
DISPLAY = {"zh-CN": "复起步卒", "en": "Risen Footman"}
REF_SPEED = 32 * 0.03            # monsters.md speed 32 → 0.96 tiles/s = m/s
CONTACT_MS = 600.0
ATTACK_MS = 1080.0
WALK = dict(cycle=1150.0, duty=0.62, lift=0.075, bob=0.016, lean=3.0, twist=6.0, arm=0.05, stance=0.0,
            drag_lift=0.22, drag_pitch=22.0, limp=0.022)
LOOK = UB.ULook()
BLOB = 0.30


def ready(A: UnburiedAnims) -> BP:
    """The shambler's READY: hunched (lean 12°), the head thrust forward and up so the eyes meet the hero, tilted
    15° on the broken neck, the left shoulder dropped, the right foot trailing on its toe; the sword hangs from the
    right fist, tip down and forward; the open left hand reaches."""
    J = A.J
    wd, wu = C.blade(146.0, lat=-0.32)
    return BP(pelvis=V(0.0, -0.02, -0.045), lean=12.0, hip_pitch=3.0, side=-3.0, twist=6.0, hip_yaw=-5.0,
              head_pitch=-40.0, head_roll=15.0, head_yaw=-6.0,
              foot_l=J["ankle_l"] + V(0.02, 0.06, 0.0), foot_r=J["ankle_r"] + V(-0.03, -0.08, 0.0),
              foot_l_rot=(0.0, 0.0, 8.0), foot_r_rot=(16.0, 0.0, -12.0), knee_l=V(0.25, 1, 0), knee_r=V(-0.25, 1, 0),
              hand_r=V(-0.27, 0.11, 0.80), elbow_r=V(-0.9, -0.3, -0.2), wpn=wd, wpn_up=wu,
              hand_l=V(0.22, 0.22, 0.93), elbow_l=V(0.7, -0.6, -0.3), wrist_l=(24.0, 0.0, -12.0),
              clav_l=(-9.0, 2.0), clav_r=(2.0, 0.0), flow=0.1)


def keys(A: UnburiedAnims) -> dict:
    """The overhead chop: the sword dragged up past the right shoulder, held over the helm (the wind-up tint reads),
    then brought down in one heavy diagonal cut with a lurch onto the left foot (Contact 600 ms)."""
    R0 = A.ready()
    J = A.J
    wind = R0.but(pelvis=R0.pelvis + V(0, -0.03, 0.025), lean=0.0, hip_pitch=0.0, twist=R0.twist - 20.0,
                  hip_yaw=-10.0, head_pitch=-14.0, head_roll=10.0, hand_r=V(-0.24, -0.04, 1.58),
                  elbow_r=V(-0.9, 0.2, 0.3), **A._wkw(R0, -38.0, lat=-0.30), hand_l=V(0.24, 0.20, 1.02),
                  clav_r=(18.0, -4.0), flow=0.3)
    hold = wind.but(hand_r=wind.hand_r + V(0.0, -0.03, 0.05), **A._wkw(R0, -50.0, lat=-0.30), lean=-3.0,
                    twist=R0.twist - 24.0)
    pre = R0.but(hand_r=V(-0.40, 0.04, 0.98), elbow_r=V(-0.6, 0.6, -0.4), **A._wkw(R0, 150.0, lat=-0.95),
                 twist=R0.twist - 8.0, lean=R0.lean - 4.0, flow=0.2)
    hit = R0.but(pelvis=R0.pelvis + V(0, 0.09, -0.07), lean=26.0, hip_pitch=6.0, twist=R0.twist + 16.0, hip_yaw=6.0,
                 head_pitch=-36.0, head_roll=8.0, hand_r=V(-0.21, 0.52, 0.93), elbow_r=V(-0.8, 0.2, -0.5),
                 **A._wkw(R0, 104.0, lat=-0.14), hand_l=V(0.27, 0.02, 0.98), elbow_l=V(0.9, 0.2, -0.3),
                 foot_l=J["ankle_l"] + V(0.02, 0.26, 0.0), clav_r=(-4.0, 10.0), clav_l=(-4.0, 0.0), flow=0.45)
    follow = hit.but(hand_r=hit.hand_r + V(0.0, 0.03, -0.12), **A._wkw(R0, 126.0, lat=-0.14), lean=28.0)
    recover = R0.but(pelvis=R0.pelvis + V(0, 0.05, -0.05), lean=18.0, hand_r=V(-0.22, 0.30, 0.82),
                     **A._wkw(R0, 140.0, lat=-0.2), foot_l=J["ankle_l"] + V(0.02, 0.18, 0.0))
    return dict(pre=pre, pre_ms=110.0, wind=wind, hold=hold, hit=hit, follow=follow, recover=recover, follow_ms=110.0,
                recover_ms=230.0,
                kneel_wpn=V(-0.45, 0.55, -0.70), fold_wpn=V(-0.62, 0.78, -0.04))


def idle_order(A: UnburiedAnims):
    """Idle_Order 2600 ms (one-shot idle variant): the sentry's last order — he straightens to attention, shoulders
    the sword, scans a line that is no longer there (left, right), and slumps back."""
    R0 = A.ready()
    att = R0.but(pelvis=R0.pelvis + V(0, 0.0, 0.03), lean=4.0, hip_pitch=0.0, side=0.0, head_pitch=-8.0,
                 head_roll=9.0, head_yaw=0.0, hand_r=V(-0.25, 0.20, 1.04), elbow_r=V(-0.9, 0.1, -0.4),
                 **A._wkw(R0, 4.0, lat=-0.30), hand_l=V(0.20, 0.06, 0.86), elbow_l=V(0.8, 0.3, -0.3),
                 clav_l=(-4.0, 0.0), clav_r=(4.0, 0.0), flow=0.12)
    left = att.but(head_yaw=48.0, twist=8.0, head_pitch=-6.0)
    right = att.but(head_yaw=-44.0, twist=-6.0, head_pitch=-6.0, head_roll=12.0)
    slump = att.but(lean=9.0, head_pitch=4.0, head_roll=18.0, head_yaw=-8.0)
    side = R0.but(hand_r=V(-0.36, 0.16, 0.97), elbow_r=V(-0.7, 0.5, -0.4), **A._wkw(R0, 96.0, lat=-0.45),
                  lean=R0.lean - 3.0)
    keys_ = [K(0.0, R0), K(170 / 2600, side, "smooth"), K(450 / 2600, att, "smooth"), K(900 / 2600, left, "smooth"),
             K(1250 / 2600, left), K(1700 / 2600, right, "smooth"), K(2000 / 2600, right),
             K(2250 / 2600, slump, "smooth"), K(2430 / 2600, side.but(head_roll=17.0), "smooth"), K(1.0, R0, "smooth")]
    return A.one_shot("Idle_Order", 2600.0, keys_, usage="idle variant (monsters.md 'they keep their last order'): "
                      "the sentry's watch; one-shot from READY back to READY, played between Idle loops (sword looks)")


def idle_order_dig(A: UnburiedAnims):
    """Idle_Order_Dig 2200 ms: the burial detail digs — both hands on the shovel, blade driven into the ground,
    levered back, a clod tossed over the left shoulder."""
    R0 = A.ready()
    J = A.J

    def two(bp, y):
        return A.second_hand(bp, y, pole=V(0.9, 0.3, -0.4))
    lift = two(R0.but(lean=10.0, head_pitch=-16.0, hand_r=V(-0.16, 0.36, 1.12), elbow_r=V(-0.9, -0.2, -0.3),
                      **A._wkw(R0, 160.0, lat=0.05), flow=0.2), 0.30)
    drive = two(R0.but(pelvis=R0.pelvis + V(0, 0.07, -0.07), lean=30.0, hip_pitch=6.0, head_pitch=-26.0,
                       hand_r=V(-0.14, 0.50, 0.80), elbow_r=V(-0.9, -0.1, -0.3), **A._wkw(R0, 160.0, lat=0.03),
                       foot_r=J["ankle_r"] + V(-0.02, 0.04, 0.0), foot_r_rot=(0.0, 0.0, -10.0), flow=0.35), 0.30)
    lever = two(drive.but(pelvis=drive.pelvis + V(0, -0.06, 0.03), lean=16.0, hand_r=V(-0.16, 0.30, 0.92),
                          **A._wkw(R0, 132.0, lat=0.05)), 0.30)
    toss = R0.but(lean=8.0, twist=26.0, hip_yaw=10.0, head_pitch=-14.0, head_yaw=-18.0,
                  hand_r=V(-0.10, 0.34, 1.14), elbow_r=V(-0.9, 0.1, -0.3), **A._wkw(R0, 62.0, lat=0.70),
                  hand_l=V(0.30, 0.12, 0.98), elbow_l=V(0.8, 0.3, -0.3), flow=0.3)
    keys_ = [K(0.0, R0), K(400 / 2200, lift, "out"), K(760 / 2200, drive, "in"), K(1050 / 2200, lever, "smooth"),
             K(1450 / 2200, toss, "out"), K(1.0, R0, "smooth")]
    return A.one_shot("Idle_Order_Dig", 2200.0, keys_, notifies={"FX_Dig": 760.0},
                      usage="idle variant of the shovel look (the burial detail digs); one-shot from READY to READY")


def anims(kit) -> UnburiedAnims:
    return UnburiedAnims(kit, REF_SPEED, WALK, ready, CONTACT_MS, ATTACK_MS, keys)


def clips(A: UnburiedAnims) -> list:
    return [A.idle(), idle_order(A), idle_order_dig(A), A.walk(), A.attack(), A.hurt(), A.death(), A.rise(),
            A.stun()]


# ── build ──────────────────────────────────────────────────────────────────────────────────────────────────
def build_all(lod1: bool = False):
    scene.reset()
    pal = Palette(UB.FAMILY)
    kit = UB.UnburiedKit(ASSET, LOOK)
    body = kit.build(pal)
    body1 = build_lod1(kit, pal) if lod1 else None
    helm = UB.build_kettle_helm(pal, kit)
    hood = UB.build_hood(pal, kit)
    sword, sword_socks = UB.build_sword(pal)
    shovel, shovel_socks = UB.build_shovel(pal)
    return dict(pal=pal, kit=kit, body=body, body1=body1, helm=helm, hood=hood, sword=sword, sword_socks=sword_socks,
                shovel=shovel, shovel_socks=shovel_socks)


LOD1_TRIS = 2950


def build_lod1(kit, pal):
    from mship import toon_tris
    ratio = UB.LOD1_RATIO
    body1 = kit.build(pal, lod=1, ratio=ratio, hull_ratio=0.75)
    n = toon_tris(body1)                 # every toon-lit slot (the crimson-cloth slot included)
    for _ in range(6):
        if n <= LOD1_TRIS:
            break
        me = body1.data
        bpy.data.objects.remove(body1, do_unlink=True)
        bpy.data.meshes.remove(me)
        ratio *= (LOD1_TRIS - 120) / n
        body1 = kit.build(pal, lod=1, ratio=ratio, hull_ratio=0.75)
        n = toon_tris(body1)
    print(f"[lod1] {kit.asset}: {n} toon tris at ratio {ratio:.3f}")
    return body1


eye_glows = UB.eye_glows
EYE_FX = UB.EYE_FX


def quick(out: Path) -> None:
    B = build_all()
    A = anims(B["kit"])
    rig = B["kit"].rig
    R.attach_to_bone(B["sword"], rig, "weapon_r")
    R.attach_to_bone(B["helm"], rig, "head")
    A.bind_meshes(B["body"], {"weapon_r": B["sword"]})
    UB.dev_review(ASSET, rig, [B["body"], B["sword"], B["helm"]], A, out, 1.80, glows=eye_glows(rig), blob=BLOB)


def anim_quick(out: Path, names, facing: str = "se", shovel: bool = False) -> None:
    import mship
    B = build_all()
    A = anims(B["kit"])
    item = B["shovel"] if shovel else B["sword"]
    R.attach_to_bone(item, B["kit"].rig, "weapon_r")
    A.bind_meshes(B["body"], {"weapon_r": item})
    cs = [c for c in clips(A) if not names or c.name in names]
    UB.march_stage()
    for o in bpy.data.objects:
        if o.type == "MESH" and o not in (B["body"], item, B["helm"]):
            o.hide_render = True
    mship.anim_review(B["kit"].rig, [B["body"]], [(item, "weapon_r"), (B["helm"], "head")], cs, TOKEN, out, 1.80,
                      UB.OUTLINE_CLASS, blob_radius=BLOB, facing=facing, px=UB.OUTLINE_PX)



def dev_items(B, key: str) -> list:
    """(object, bone) attached for the dev frame renders (``frames.py``): sword | shovel | bare | hood."""
    weapon = B["shovel"] if key == "shovel" else B["sword"]
    head = {"hood": B["hood"], "bare": None}.get(key, B["helm"])
    return [(weapon, "weapon_r")] + ([(head, "head")] if head is not None else [])



# ── ship ───────────────────────────────────────────────────────────────────────────────────────────────────
GRIP_PARTS = ("fist_r", "thumb_r", "knuckle", "wrist_r")
QA_GROUPS = {"tabard": ["tabard_skirt", "tabard_hem"],
             "legs": ["leg", "knee", "boot", "boot_shaft", "boot_cuff", "sole"]}
QA_PAIRS = {"tabardLegs": ("tabard", "legs")}


def sword_grip(co) -> bool:
    """Sword faces in the fist (grip, the guard's underside against the knuckles) — the QA baseline."""
    return -0.13 < co.y < 0.10


def shovel_grip(co) -> bool:
    return -0.08 < co.y < 0.08


def both(*fns):
    """Several ``extra_previews`` in order."""
    def fn(ms, rv, actions):
        for f in fns:
            f(ms, rv, actions)
    return fn


def dig_sheet(B):
    """``anim_Idle_Order_Dig.png`` re-rendered with the shovel (the clip belongs to the burial-detail look; the
    standard sheet carries the default sword)."""
    def fn(ms, rv, actions):
        from kit import anim
        sword, shovel = B["sword"], B["shovel"]
        acts = dict(zip([c.name for c in ms.clips], actions))
        clip = next(c for c in ms.clips if c.name == "Idle_Order_Dig")
        base = list(rv.meshes)
        was = shovel.hide_render
        try:
            sword.hide_render, shovel.hide_render = True, False
            rv.meshes[:] = [m for m in base if m is not sword] + [shovel]
            rv.contact_sheet(ms.A.rig, [(clip, acts[clip.name])], frames=8)
            (rv.out / "anims.png").replace(rv.out / "anim_Idle_Order_Dig.png")
        finally:
            rv.meshes[:] = base
            sword.hide_render, shovel.hide_render = False, was
        anim.assign(ms.A.rig, acts["Idle"])
        anim.set_frame(0)
    return fn


def ship(previews: bool = True, verify: bool = True) -> int:
    UB.march_stage()
    ms = qa_setup(lod1=True)
    return UB.ship_member(ms, ms.lod1_items, previews=previews, verify=verify)


def qa_setup(lod1: bool = False):
    """The ``MonsterShip`` of this member (built, rigged, items attached, clips authored; not baked)."""
    import mship
    B = build_all(lod1=lod1)
    kit, body = B["kit"], B["body"]
    rig = kit.rig
    A = anims(kit)
    R.attach_to_bone(B["sword"], rig, "weapon_r")
    R.attach_to_bone(B["helm"], rig, "head")
    A.bind_meshes(body, {"weapon_r": B["sword"]})
    A.ground_pts.add_rigid(B["shovel"], "weapon_r", step=1, tag="shovel")
    cs = clips(A)
    items = [mship.Item(B["sword"], UB.SWORD, "weapon_r", "sword", sockets=B["sword_socks"],
                        extra={"lengthCm": round((UB.SWORD_TIP + 0.13) * 100.0, 1), "look": "weapon",
                               "note": "notched arming sword: the hero SM_Weapon_Sword T1 shapes in the Unburied "
                                       "steel (monsters.md: re-used SM_Weapon_Sword T1)"},
                        grip=sword_grip),
             UB.head_item(B["helm"], UB.HELM, None),
             mship.Item(B["shovel"], UB.SHOVEL, "weapon_r", "shovel", sockets=B["shovel_socks"], variant="look_shovel",
                        extra={"look": "weapon", "lengthCm": round((UB.SHOVEL_TIP + 0.08) * 100.0, 1),
                               "note": "the burial detail (one in three footmen); held like the sword"},
                        grip=shovel_grip, preview_up="weapon_flat"),
             UB.head_item(B["hood"], UB.HOOD, "look_hood")]
    contact_f = scene.ms_to_frame(CONTACT_MS)
    windup_f = scene.ms_to_frame(A.windup_ms)
    weapons = [{"id": "sword", "weight": 2, "attachments": [{"object": UB.SWORD, "socket": "weapon_r"}],
                "idleOrder": "Idle_Order"},
               {"id": "shovel", "weight": 1, "attachments": [{"object": UB.SHOVEL, "socket": "weapon_r"}],
                "idleOrder": "Idle_Order_Dig",
                "attackTrail": {"color": "#C8C4BC", "alpha": 0.4, "sockets": ["blade_tip", "blade_mid"],
                                "item": UB.SHOVEL}}]
    body_nf = mship.parts_except(body, GRIP_PARTS)
    body_nh = mship.parts_except(body, GRIP_PARTS + ("hand_l", "wrist_l"))
    body_nohead = mship.parts_except(body, UB.HEAD_PARTS + ("neck",))
    ms = mship.MonsterShip(
        asset=ASSET, token=TOKEN, preview_dir=PREVIEW, pal=B["pal"], A=A, body=body, items=items, clips=cs,
        outline_class=UB.OUTLINE_CLASS, game_ids=GAME_IDS, blob_radius=BLOB, height=1.80, lod1=B["body1"],
        display=DISPLAY, fx={"eyeGlow": EYE_FX,
                             "telegraph": {"normal": "#FFE8DC", "elite": "#D11F2E", "untilMs": A.windup_ms,
                                           "note": "dark remake: bone fill / crimson rim (monsters.md §1.2)"},
                             "attackTrail": {"color": "#C8C4BC", "alpha": 0.4, "sockets": ["tip", "mid"],
                                             "item": UB.SWORD}},
        glows=eye_glows(rig),
        qa_groups={"body_nf": body_nf, "body_nh": body_nh, "body_nohead": body_nohead, **QA_GROUPS},
        qa_pairs={"swordBody": ("item:" + UB.SWORD, "body_nf"), "shovelBody": ("item:" + UB.SHOVEL, "body_nh"),
                  "helmBody": ("item:" + UB.HELM, "body_nohead"), "hoodBody": ("item:" + UB.HOOD, "body_nohead"),
                  **QA_PAIRS},
        qa_limits={"ground": -0.5, "swordBody": 0, "shovelBody": 0, "helmBody": 0, "hoodBody": 0,
                   **{k: 0 for k in QA_PAIRS}},
        qa_clip_limits={"Idle_Order_Dig": {"swordBody": 10 ** 6}, "Idle_Order": {"shovelBody": 10 ** 6}},
        outline_px=UB.OUTLINE_PX, outline_px_note=UB.OUTLINE_PX_NOTE, native_sheets=True,
        game_keys=[("Idle", 0, "se"), ("Walk", 10, "se"), ("Attack01", windup_f, "se"), ("Attack01", contact_f, "se"),
                   ("Attack01", contact_f, "sw"), ("Hurt", 6, "se"), ("Death", 54, "se"), ("Rise", 50, "se"),
                   ("Stun", 20, "se"), ("Idle_Order", 60, "se"), ("Idle", 0, "front"), ("Walk", 40, "sw")],
        close_keys=[("Attack01", windup_f, "se"), ("Attack01", contact_f, "se"), ("Attack01", contact_f, "side"),
                    ("Hurt", 6, "se"), ("Death", 20, "se"), ("Death", 31, "se"), ("Death", 54, "se"),
                    ("Rise", 50, "se"), ("Stun", 20, "front"), ("Walk", 20, "side")],
        back_clips=("Attack01", "Death", "Rise"),
        skeleton_bones_required=("root", "pelvis", "head", "jaw", "eye_l", "weapon_r", "weapon_l", "tabard_f_02",
                                 "tabard_b_02", "ik_foot_l", "ik_hand_gun"),
        extra_previews=both(UB.looks_preview([("helm sword", [], []), ("bare scalp", [], [B["helm"]]),
                                              ("hood", [B["hood"]], [B["helm"]]),
                                              ("shovel", [B["shovel"]], [B["sword"]])]),
                            dig_sheet(B)),
        variant_previews=("looks.png",),
        lod_note="LOD1: the same kit at a collapse ratio iterated to <= 3 k toon (eyes, sockets and mouth built exact).",
        extra={"refSpeedCmS": round(REF_SPEED * 100.0, 1), "attackStyle": "overhead", "family": "unburied",
               "marker": {"swatches": list(UB.MARKER), "note": "violet pinpoint eyes (monsters.md §1.2)"},
               "looks": UB.looks_manifest(weapons, "sword"),
               "notes": "Clips in place (play rate = v / ref). Idle_Order / Idle_Order_Dig are one-shot idle variants "
                        "(READY to READY) for the sword / shovel looks. Death folds forward into the ash-husk pose "
                        "(U3 dissolves it); Rise = Death reversed (raises, spawns, the Pit's dormant footmen). Stun "
                        "loops."},
    )
    ms.extra["budget"] = UB.budget_record(ms, 7000, 3000)
    pal = B["pal"]
    ms.lod1_items = {UB.HELM: lambda: UB.build_kettle_helm(pal, kit, 1), UB.HOOD: lambda: UB.build_hood(pal, kit, 1),
                     UB.SWORD: lambda: UB.build_sword(pal, 1)[0], UB.SHOVEL: lambda: UB.build_shovel(pal, 1)[0]}
    return ms


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    scratch = scratch_root("mq", "risen_footman")
    if "--quick" in args:
        k = args.index("--quick")
        quick(Path(args[k + 1]) if len(args) > k + 1 and not args[k + 1].startswith("--") else scratch)
    elif "--anims" in args:
        names = [a for a in args[args.index("--anims") + 1:] if not a.startswith("--")]
        fac = next((a.split("=")[1] for a in args if a.startswith("--facing=")), "se")
        anim_quick(scratch / "anims", names, fac, shovel="--shovel" in args)
    else:
        sys.exit(ship(previews="--no-previews" not in args, verify="--no-verify" not in args))


"""复起弩手 Risen Arbalester — ``SK_Mon_RisenArbalest`` (dark remake monsters.md §2.2, Unburied kit, attachment variant;
implementation-plan.md A2).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/risen_arbalest.py
        (no flag) ship · --quick [dir] · --anims [Clip …] [--facing=se] · --no-previews · --no-verify

The first ranged regular: a dead crusade arbalester with a heavy crossbow (``SM_MonWeapon_Crossbow``, new held item:
a steel prod, a long stock with a stirrup, the string spanned to the nut), a bolt quiver at the right hip and a pavise
slung on the back (both on the body). Same kit and head looks as the footman (kettle helm default, bare scalp, hood).

Behaviour (monsters.md): L5, speed 34 (1.02 m/s), range 6.0, attack interval 2200: Attack01 raises the crossbow to the
shoulder, aims and looses — Contact (the release, projectile spawn on the ``tip`` socket) 450 ms, WindupEnd 279; the
bolt is bone-white with a breach-violet trail (``projectileColor`` #B02FD0, core data). Idle_Order: "an arbalester works
an empty crossbow" — nose-down on its stirrup, the windlass cranked at the butt. The crossbow rides two hands in every
upright clip (``UnburiedAnims.two_hand``).
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
import bmesh  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import mesh as M, outline, rig as R, scene  # noqa: E402
from kit.palette import Palette  # noqa: E402
from kit.paths import scratch_root  # noqa: E402

import common as C  # noqa: E402
from common import BP, K, V  # noqa: E402
import hero_base as HB  # noqa: E402
import unburied as UB  # noqa: E402
import risen_footman as RF  # noqa: E402
from uclips import UnburiedAnims  # noqa: E402

ASSET = "SK_Mon_RisenArbalest"
TOKEN = "Mon_RisenArbalest"
PREVIEW = "mon_risen_arbalest"
GAME_IDS = ["monster_risen_arbalest"]
DISPLAY = {"zh-CN": "复起弩手", "en": "Risen Arbalester"}
CROSSBOW = "SM_MonWeapon_Crossbow"
REF_SPEED = 34 * 0.03
CONTACT_MS = 450.0
ATTACK_MS = 980.0
WALK = dict(RF.WALK, cycle=1080.0)
LOOK = UB.ULook(arrows=(), quiver=True, pavise=True, hand_l="fist")
BLOB = 0.32
# the left wrist on the fore-stock (offsets from the mirrored grip, item frame: +X = down, TOP_FRAME): over the stock
# while it is carried low (the arm hangs onto it), under it once it is raised (the elbow beneath, the aim)
LEFT_OVER = (0.10, -0.067, -0.036)     # (y along, lat, up): the wrist 4.5 cm over the stock, behind the nut
LEFT_UNDER = (0.18, 0.023, -0.041)     # the wrist 4.5 cm under the fore-stock (the fist closes round its belly)
LEFT_ROUND = -0.10                     # the wrist passes round the stock's left side between the two
TWO_HAND = (None, None, None)          # None: blended over → under by the right fist's height (ArbalestAnims.second_hand)
CB_TIP = 0.66
TOP_FRAME = Matrix.Rotation(math.radians(-90.0), 4, "Y")     # built top-up (+Z), shipped top on item −X
# art review r3: the folded husk rolls onto its flank (deg added to the fold's ``twist`` / ``side``), so the pavise —
# rigid on spine_03 — lands on its edge instead of face-up. Field normal at the fold: 21° → 65° off vertical; over the
# eight facings the field's mean camera-facing area 0.71 → 0.38 and the lit-band share (N·L > .72) 0.36 → 0.05 (se
# N·L .96 → .67: the base band); probed with A.mats on spine_03 (no bake)
DEATH_ROLL = (50.0, -10.0)


# ── the crossbow ──────────────────────────────────────────────────────────────────────────────────────────────
def build_crossbow(pal: Palette) -> tuple[bpy.types.Object, list]:
    """A heavy crusade crossbow in weapon_r space (grip at the origin, +Y = shooting direction, +Z = the stock's top):
    a long wooden tiller from the butt (−0.30) past the nut (+0.12) to the steel prod (+0.48, 64 cm wide, its arms
    swept forward), the string spanned back to the nut, an iron stirrup at the front, a long trigger lever below.
    Empty: the bolt is the runtime projectile (loosed at Contact from the ``tip`` socket)."""
    b = UB.MBuilder(CROSSBOW, pal, prefix="crossbow")
    b.use("crossbow.stock", "crossbow.stock_dark", "crossbow.iron", "crossbow.string", "unburied.rope")
    # tiller: a squared beam, deeper at the butt, a groove line on top
    rows = [(-0.30, 0.020, 0.034, -0.012), (-0.18, 0.019, 0.030, -0.006), (-0.04, 0.017, 0.022, 0.0),
            (0.10, 0.019, 0.024, 0.002), (0.40, 0.020, 0.022, 0.004), (0.52, 0.018, 0.020, 0.004)]
    stock = M.sweep([(0, y, cz) for y, _, _, cz in rows], M.superellipse(1, 1, 8, 3.2),
                    scales=[(h, w) for _, w, h, _ in rows], up=(1, 0, 0), dome0=0.004, dome1=0.003)
    sid = b.add(stock, "stock", name="stock", hull_pad=True)
    groove = M.box((0.006, 0.40, 0.003), (0, 0.30, 0.0255), bevel=0.0)
    b.add(groove, "stock_dark", name="groove", outline=False, covered_by=sid)
    # the nut (iron) and the trigger lever under the stock
    b.add(M.box((0.026, 0.024, 0.014), (0, 0.12, 0.024), bevel=0.003), "iron", name="nut", outline=False,
          covered_by=sid)
    lever = M.tube([(0, 0.08, -0.018), (0, 0.0, -0.028), (0, -0.07, -0.032)], [0.006, 0.006, 0.0055], sides=5)
    b.add(lever, "iron", name="lever", hull_pad=True)
    # the prod: a steel bow swept forward, rectangular section, thicker at the centre
    pts, rad = [], []
    for i in range(13):
        t = -1.0 + 2.0 * i / 12
        x = 0.32 * t
        y = 0.48 + 0.075 * (t * t) ** 0.9
        pts.append((x, y, 0.010))
        rad.append((0.012 - 0.004 * abs(t), 0.009 - 0.003 * abs(t)))
    prod = M.sweep(pts, M.superellipse(1, 1, 6, 2.6), scales=rad, up=(0, 0, 1), dome0=0.004, dome1=0.004)
    pid = b.add(prod, "iron", name="prod", hull_pad=True)
    # bindings of the prod to the stock
    for y in (0.46, 0.50):
        ring = M.tube([(0, y - 0.006, 0.004), (0, y + 0.006, 0.004)], [0.024, 0.024], sides=8, cap0=None, cap1=None)
        b.add(ring, "unburied.rope", name="binding", outline=False, covered_by=sid)
    # the string: from the prod tips back to the nut (a V, spanned)
    for sg in (1.0, -1.0):
        tip = Vector((sg * 0.32, 0.48 + 0.075, 0.010))
        nut = Vector((sg * 0.012, 0.12, 0.026))
        st = M.tube([tip, tip.lerp(nut, 0.5), nut], [0.0028] * 3, sides=4, dome0=0.0, dome1=0.0)
        b.add(st, "string", name="string")
    # the stirrup at the front
    s0 = Vector((0, 0.52, 0.004))
    stir = M.tube([s0 + Vector((0.040, 0.0, 0)), s0 + Vector((0.045, 0.08, 0)), s0 + Vector((0.0, 0.14, 0)),
                   s0 + Vector((-0.045, 0.08, 0)), s0 + Vector((-0.040, 0.0, 0))], [0.0065] * 5, sides=5)
    b.add(stir, "iron", name="stirrup", hull_pad=True)
    # turned on its side in weapon_r space (the top → item −X, the prod along ±Z): the fist's wrist sits on the item's
    # +Z side, so the right hand wraps the stock from its outer side (rifle grip) with the crossbow upright
    obj = b.build(CROSSBOW)
    obj.data.transform(TOP_FRAME)
    obj.data.update()
    from kit import shading
    mt = shading.toon_material(pal)
    outline.bake_hull(obj, outline.WIDTH_CM[UB.OUTLINE_CLASS] / 100.0, mt, shading.outline_material(pal))
    for msg in outline.hull_color_report(obj, pal, UB.OUTLINE_CLASS):
        print(f"[ink] WARNING {CROSSBOW}: {msg}")
    obj["af_outline_class"] = UB.OUTLINE_CLASS
    socks = [{"name": nm, "pos": tuple(round(c, 4) for c in TOP_FRAME @ Vector((0.0, y, 0.03)))}
             for nm, y in (("tip", CB_TIP), ("mid", 0.30), ("nut", 0.12))]
    del pid
    return obj, socks


# ── poses ──────────────────────────────────────────────────────────────────────────────────────────────────────
def cb_dir(lat: float, fwd: float, up: float, roll: float = 0.0):
    """Crossbow direction (+Y) for a body-space direction, and the item +Z that keeps its top (item −X) as close to
    world up as the direction allows (rolled ``roll`` degrees about the stock): +Z = dir × top (the prod's axis)."""
    d = V(lat, fwd, up).normalized()
    u = Vector((0, 0, 1)) - d * d.z
    u = u.normalized() if u.length > 1e-6 else Vector((0, -1, 0))
    if roll:
        u = (Matrix.Rotation(math.radians(roll), 3, d) @ u).normalized()
    return {"wpn": d, "wpn_up": d.cross(u).normalized()}


def ready(A: UnburiedAnims) -> BP:
    """READY: hunched like the footman, the crossbow carried low across the body (port arms, nose down to the
    front-left), the right fist at the stock's wrist by the right hip, the left fist under the fore-stock."""
    J = A.J
    bp = BP(pelvis=V(0.0, -0.02, -0.045), lean=12.0, hip_pitch=3.0, side=-3.0, twist=8.0, hip_yaw=-5.0,
            head_pitch=-36.0, head_roll=15.0, head_yaw=-8.0,
            foot_l=J["ankle_l"] + V(0.02, 0.06, 0.0), foot_r=J["ankle_r"] + V(-0.03, -0.08, 0.0),
            foot_l_rot=(0.0, 0.0, 8.0), foot_r_rot=(16.0, 0.0, -12.0), knee_l=V(0.25, 1, 0), knee_r=V(-0.25, 1, 0),
            hand_r=V(-0.20, 0.17, 0.86), elbow_r=V(-0.9, 0.2, -0.3), **cb_dir(0.55, 0.78, -0.30, roll=-10.0),
            elbow_l=V(0.8, -0.2, -0.5), clav_l=(-9.0, 2.0), clav_r=(2.0, 0.0), flow=0.1)
    return A.second_hand(level(A, bp), *TWO_HAND)


def level(A: UnburiedAnims, bp: BP, roll: float = 0.0) -> BP:
    """Roll the right fist about the stock so the crossbow's top faces ``wpn_up`` (the natural hammer grip would roll
    the prod with the forearm): ``wrist_r`` roll = the signed angle from the solved item +Z to ``wpn_up``."""
    m = A.mats(bp)["weapon_r"].to_3x3()
    d = Vector(bp.wpn).normalized()
    z = m @ Vector((0, 0, 1))
    zp = (z - d * z.dot(d)).normalized()
    w = Vector(bp.wpn_up)
    wp_ = (w - d * w.dot(d)).normalized()
    ang = math.degrees(math.atan2(d.dot(zp.cross(wp_)), zp.dot(wp_)))
    p, r, y = bp.wrist_r
    tot = (r + ang + roll + 180.0) % 360.0 - 180.0          # the short way round (keys interpolate the roll)
    return bp.but(wrist_r=(p, tot, y))


def keys(A: UnburiedAnims) -> dict:
    """Raise, aim, loose: the crossbow swings up in front (never across the chest) and the butt comes back against
    the front of the right shoulder, levelled on the target (the left shoulder turned in, the left arm long under the
    fore-stock, the head counter-pitched down the stock), released at Contact with a kick (the prod snaps, the nose
    jumps), lowered back in front to READY."""
    R0 = A.ready()
    th = TWO_HAND

    def two(bp, roll=0.0):
        return A.second_hand(level(A, bp, roll), *th)
    pre = two(R0.but(lean=9.0, twist=-16.0, hip_yaw=-8.0, hand_r=V(-0.13, 0.40, 1.04), elbow_r=V(-0.8, 0.0, -0.5),
                     **cb_dir(0.10, 1.0, -0.05), elbow_l=V(0.6, -0.2, -0.7), flow=0.15))
    # the aim stands half side-on (hips and chest turned 42° right, the face back on the target): the butt in the
    # right shoulder pocket, the left elbow under the fore-stock
    aim_body = dict(lean=8.0, hip_yaw=-18.0, twist=-24.0, side=-2.0, head_yaw=24.0, head_roll=14.0,
                    foot_l=R0.foot_l + V(0.02, 0.04, 0.0), foot_r=R0.foot_r + V(-0.02, -0.04, 0.0),
                    elbow_r=V(-0.8, -0.3, -0.5), elbow_l=V(0.3, -0.1, -1.0), clav_r=(6.0, 0.0))
    raise_ = two(R0.but(**aim_body, head_pitch=-12.0, hand_r=V(-0.16, 0.46, 1.32), **cb_dir(0.0, 1.0, 0.08),
                        flow=0.2))
    aim = two(R0.but(**aim_body, head_pitch=2.0, hand_r=V(-0.16, 0.42, 1.40), **cb_dir(0.0, 1.0, -0.03)))
    loose = two(aim.but(hand_r=V(-0.16, 0.40, 1.43), **cb_dir(0.0, 1.0, 0.16), lean=5.0, head_pitch=-4.0,
                        pelvis=R0.pelvis + V(0, -0.02, 0.0), flow=0.35))
    follow = two(aim.but(hand_r=V(-0.16, 0.41, 1.41), **cb_dir(0.0, 1.0, 0.06), lean=6.0, head_pitch=0.0))
    recover = two(R0.but(lean=10.0, twist=-14.0, hip_yaw=-8.0, hand_r=V(-0.17, 0.40, 1.05), elbow_r=V(-0.8, 0.0, -0.5),
                         **cb_dir(0.10, 1.0, -0.12), elbow_l=V(0.6, -0.2, -0.7)))
    return dict(pre=pre, pre_ms=110.0, wind=raise_, hold=aim, hit=loose, follow=follow, recover=recover,
                lift_ms=220.0, follow_ms=120.0, recover_ms=260.0,
                **{"kneel_" + k: v for k, v in cb_dir(-0.15, 1.0, -0.35).items()}, kneel_hand_r=V(-0.36, 0.30, 0.30),
                kneel_elbow_r=V(-0.9, 0.0, -0.3),
                **{"buckle_" + k: v for k, v in cb_dir(-0.05, 1.0, -0.45).items()}, buckle_hand_r=V(-0.30, 0.26, 0.70),
                **{"fold_" + k: v for k, v in cb_dir(-0.35, 0.94, -0.03).items()}, fold_hand_r=V(-0.46, 0.40, 0.07),
                death_fix=lambda bp: level(A, bp))


def idle_order(A: UnburiedAnims):
    """Idle_Order 2400 ms: works an empty crossbow — swings it nose-down onto its stirrup before the right foot, bends
    over it and cranks the windlass at the butt three turns (the left fist circling beside the stock), straightens and
    brings it back to the carry."""
    R0 = A.ready()
    down = level(A, R0.but(lean=30.0, hip_pitch=10.0, pelvis=R0.pelvis + V(0.0, -0.05, -0.07), head_pitch=-26.0,
                           hand_r=V(-0.12, 0.40, 0.70), elbow_r=V(-1.0, 0.2, 0.2),
                           **cb_dir(-0.05, 0.12, -1.0, roll=180.0), hand_l=V(0.02, 0.40, 0.96), elbow_l=V(0.9, 0.0, -0.3),
                           flow=0.25))
    out = level(A, R0.but(lean=14.0, hand_r=V(-0.26, 0.36, 0.92), elbow_r=V(-0.9, 0.1, -0.4),
                          **cb_dir(-0.10, 1.0, -0.60), hand_l=V(0.20, 0.16, 0.95), elbow_l=V(0.8, 0.3, -0.3), flow=0.15))
    butt = down.hand_r - Vector(down.wpn) * 0.30
    centre = butt + V(0.10, 0.0, -0.03)
    keys_ = [K(0.0, R0), K(200 / 2400, out, "smooth"), K(470 / 2400, down, "out")]
    for k in range(3):
        t0 = 600 + k * 450
        for j, ang in enumerate((0.0, 120.0, 240.0)):
            r = math.radians(ang)
            hl = centre + V(0.0, 0.05 * math.cos(r), 0.05 * math.sin(r))
            keys_.append(K((t0 + j * 150) / 2400, down.but(hand_l=hl, head_yaw=-6.0 + 4.0 * math.sin(r),
                                                           lean=30.0 + 2.0 * math.sin(r)), "smooth"))
    keys_ += [K(1950 / 2400, down, "smooth"), K(2200 / 2400, out, "smooth"), K(1.0, R0, "smooth")]
    return A.one_shot("Idle_Order", 2400.0, keys_, notifies={"FX_Crank": 600.0},
                      usage="idle variant (monsters.md: 'an arbalester works an empty crossbow'): cranks the windlass; "
                            "one-shot from READY back to READY")


class ArbalestAnims(UnburiedAnims):
    two_hand = TWO_HAND
    two_hand_clips = ("Idle", "Walk", "Attack01", "Hurt", "Stun")

    def second_hand(self, bp: BP, y_along=None, lat=None, up=None, pole=None) -> BP:
        if y_along is None:                 # the per-frame post of the two-handed clips: level, then the left fist
            bp = level(self, bp)
            f = min(1.0, max(0.0, (Vector(bp.hand_r).z - 1.10) / 0.16))
            f = f * f * (3.0 - 2.0 * f)
            y_along, lat, up = (a + (b - a) * f for a, b in zip(LEFT_OVER, LEFT_UNDER))
            up += LEFT_ROUND * math.sin(math.pi * f)
        return super().second_hand(bp, y_along, lat, up, pole)

    def death_keys(self) -> list:
        """The shared fold, rolled onto the flank at the end (``DEATH_ROLL``): the slung pavise comes to rest on its
        edge, its field turned off the key light, instead of a flat board laid face-up over the husk. Rise starts
        from the same rolled husk."""
        rec, buckle, kneel, fold = super().death_keys()
        tw, sd = DEATH_ROLL
        return [rec, buckle, kneel, level(self, fold.but(twist=fold.twist + tw, side=fold.side + sd))]


def anims(kit) -> UnburiedAnims:
    return ArbalestAnims(kit, REF_SPEED, WALK, ready, CONTACT_MS, ATTACK_MS, keys)


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
    cb, socks = build_crossbow(pal)
    return dict(pal=pal, kit=kit, body=body, body1=body1, helm=helm, hood=hood, crossbow=cb, crossbow_socks=socks)


def dev_items(B, key: str) -> list:
    head = {"hood": B["hood"], "bare": None}.get(key, B["helm"])
    return [(B["crossbow"], "weapon_r")] + ([(head, "head")] if head is not None else [])


def quick(out: Path) -> None:
    B = build_all()
    A = anims(B["kit"])
    rig = B["kit"].rig
    R.attach_to_bone(B["crossbow"], rig, "weapon_r")
    R.attach_to_bone(B["helm"], rig, "head")
    A.bind_meshes(B["body"], {"weapon_r": B["crossbow"]})
    UB.dev_review(ASSET, rig, [B["body"], B["crossbow"], B["helm"]], A, out, 1.80, glows=RF.eye_glows(rig), blob=BLOB)


# ── ship ───────────────────────────────────────────────────────────────────────────────────────────────────────
GRIP_PARTS = ("fist_r", "thumb_r", "knuckle", "wrist_r", "fist_l", "thumb_l", "wrist_l")
QA_GROUPS = dict(RF.QA_GROUPS)
QA_PAIRS = dict(RF.QA_PAIRS)


def crossbow_grip(co) -> bool:
    """Crossbow faces in the fists: the right fist and wrist round the stock's wrist (−13…+8 cm: the trigger hand's
    cuff rests on the stock behind the fist when it is raised), the left fist over / under the fore-stock (+4…+29 cm,
    ``LEFT_OVER`` / ``LEFT_UNDER``); the stock's width is item Z (the crossbow ships on its side, ``TOP_FRAME``)."""
    return (-0.13 < co.y < 0.08 and abs(co.z) < 0.04) or (0.04 < co.y < 0.29 and abs(co.z) < 0.04)


def qa_setup(lod1: bool = False):
    import mship
    B = build_all(lod1=lod1)
    kit, body = B["kit"], B["body"]
    rig = kit.rig
    A = anims(kit)
    R.attach_to_bone(B["crossbow"], rig, "weapon_r")
    R.attach_to_bone(B["helm"], rig, "head")
    A.bind_meshes(body, {"weapon_r": B["crossbow"]})
    cs = clips(A)
    items = [mship.Item(B["crossbow"], CROSSBOW, "weapon_r", "crossbow", sockets=B["crossbow_socks"],
                        extra={"lengthCm": 96.0, "spanCm": 64.0, "projectileSocket": "tip",
                               "note": "monsters.md §2.2: new held item SM_MonWeapon_Crossbow; empty (the bolt is the "
                                       "runtime projectile, bone-white with a breach-violet trail)"},
                        grip=crossbow_grip),
             UB.head_item(B["helm"], UB.HELM, None),
             UB.head_item(B["hood"], UB.HOOD, "look_hood")]
    contact_f = scene.ms_to_frame(CONTACT_MS)
    windup_f = scene.ms_to_frame(A.windup_ms)
    body_nf = mship.parts_except(body, GRIP_PARTS)
    body_nohead = mship.parts_except(body, UB.HEAD_PARTS + ("neck",))
    weapons = [{"id": "crossbow", "weight": 1, "attachments": [{"object": CROSSBOW, "socket": "weapon_r"}],
                "idleOrder": "Idle_Order"}]
    ms = mship.MonsterShip(
        asset=ASSET, token=TOKEN, preview_dir=PREVIEW, pal=B["pal"], A=A, body=body, items=items, clips=cs,
        outline_class=UB.OUTLINE_CLASS, game_ids=GAME_IDS, blob_radius=BLOB, height=1.80, lod1=B["body1"],
        display=DISPLAY, fx={"eyeGlow": RF.EYE_FX,
                             "telegraph": {"normal": "#FFE8DC", "elite": "#D11F2E", "untilMs": A.windup_ms,
                                           "note": "dark remake: bone fill / crimson rim (monsters.md §1.2)"},
                             "muzzle": {"socket": "tip", "item": CROSSBOW,
                                        "note": "projectile origin at Contact (the release)"}},
        glows=RF.eye_glows(rig),
        qa_groups={"body_nf": body_nf, "body_nohead": body_nohead, **QA_GROUPS},
        qa_pairs={"crossbowBody": ("item:" + CROSSBOW, "body_nf"), "helmBody": ("item:" + UB.HELM, "body_nohead"),
                  "hoodBody": ("item:" + UB.HOOD, "body_nohead"), **QA_PAIRS},
        qa_limits={"ground": -0.5, "crossbowBody": 0, "helmBody": 0, "hoodBody": 0, **{k: 0 for k in QA_PAIRS}},
        outline_px=UB.OUTLINE_PX, outline_px_note=UB.OUTLINE_PX_NOTE, native_sheets=True,
        game_keys=[("Idle", 0, "se"), ("Walk", 10, "se"), ("Attack01", windup_f, "se"), ("Attack01", contact_f, "se"),
                   ("Attack01", contact_f, "sw"), ("Hurt", 6, "se"), ("Death", 54, "se"), ("Rise", 50, "se"),
                   ("Stun", 20, "se"), ("Idle_Order", 70, "se"), ("Idle", 0, "front"), ("Walk", 40, "sw")],
        close_keys=[("Attack01", windup_f, "se"), ("Attack01", contact_f, "se"), ("Attack01", contact_f, "side"),
                    ("Hurt", 6, "se"), ("Death", 20, "se"), ("Death", 31, "se"), ("Death", 54, "se"),
                    ("Rise", 50, "se"), ("Idle_Order", 70, "front"), ("Walk", 20, "side")],
        back_clips=("Attack01", "Death", "Rise"),
        skeleton_bones_required=("root", "pelvis", "head", "jaw", "eye_l", "weapon_r", "weapon_l", "tabard_f_02",
                                 "tabard_b_02", "ik_foot_l", "ik_hand_gun"),
        extra_previews=UB.looks_preview([("helm", [], []), ("bare scalp", [], [B["helm"]]),
                                         ("hood", [B["hood"]], [B["helm"]])]),
        variant_previews=("looks.png",),
        lod_note="LOD1: the same kit at a collapse ratio iterated to <= 3 k toon (eyes, sockets and mouth built exact).",
        extra={"refSpeedCmS": round(REF_SPEED * 100.0, 1), "attackStyle": "ranged", "family": "unburied",
               "marker": {"swatches": list(UB.MARKER), "note": "violet pinpoint eyes (monsters.md §1.2)"},
               "looks": UB.looks_manifest(weapons, "crossbow"),
               "notes": "Ranged (range 6.0): Contact = the release (projectile from the crossbow's tip socket). The "
                        "crossbow rides both fists in Idle / Walk / Attack01 / Hurt / Stun. Idle_Order is a one-shot "
                        "idle variant (cranks the windlass). Death folds forward (ash husk, U3); Rise = Death "
                        "reversed. Stun loops."},
    )
    ms.extra["budget"] = UB.budget_record(ms, 7000, 3000)
    pal = B["pal"]
    ms.lod1_items = {UB.HELM: lambda: UB.build_kettle_helm(pal, kit, 1), UB.HOOD: lambda: UB.build_hood(pal, kit, 1)}
    return ms


def ship(previews: bool = True, verify: bool = True) -> int:
    UB.march_stage()
    ms = qa_setup(lod1=True)
    return UB.ship_member(ms, ms.lod1_items, previews=previews, verify=verify)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    scratch = scratch_root("mq", "risen_arbalest")
    if "--quick" in args:
        k = args.index("--quick")
        quick(Path(args[k + 1]) if len(args) > k + 1 and not args[k + 1].startswith("--") else scratch)
    else:
        sys.exit(ship(previews="--no-previews" not in args, verify="--no-verify" not in args))

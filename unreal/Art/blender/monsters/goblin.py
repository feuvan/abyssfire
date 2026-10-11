"""哥布林 Goblin — ``SK_Mon_Goblin`` (spec art-inventory-ch1.md §4.2, §4.5, §10; DECISIONS R2, R12, M5, A14).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/goblin.py [--quick] [--no-previews]

Hunched, big-headed raider (0.92 m): swept-back ears, hooked nose, toothy grin, bone-tooth necklace, rope belt,
sash, ragged loincloth and a crude knapped-stone spear (``SM_Mon_Goblin_Spear`` on ``weapon_r``). Clips: Idle 667,
Walk 600 (1.65 m/s, no slide), Attack01 thrust 500 (WindupEnd 155, Contact 250), Hurt 200, Death 500, Stun 600.
The quest-hunt leaders (§4.5) are this mesh at scale 1.25 with their accents as attachments:
``SM_Mon_Goblin_Att_RedCap`` (红帽格鲁克 Gruk Redcap) and ``SM_Mon_Goblin_Att_LootSack`` + ``_Att_Pendant``
(小贼斯尼克 Sneek the Thief) — manifest ``variants``.

Web source: ``src/graphics/sprites/monsters/Goblin.ts`` (palette :45-57, spear :284-294, proportions :296-302,
READY :328-338, walk :347), ``rig/MonsterKit.ts`` (thrust track).
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE.parent / "heroes"))
sys.path.insert(0, str(HERE))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import mesh as M, outline, rig as R, scene  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402
from kit.rig import Bind  # noqa: E402

import common as C  # noqa: E402
from common import BP, V  # noqa: E402
import goblin_hunt as GH  # noqa: E402
import goblins as GB  # noqa: E402
import mgeo as G  # noqa: E402
from mclips import GoblinAnims  # noqa: E402

ASSET = "SK_Mon_Goblin"
TOKEN = "Mon_Goblin"
SPEAR = "SM_Mon_Goblin_Spear"
PREFIX = "goblin"
WEB = GB.Web(U=0.0216 * 0.92 / 0.988)       # skull top 0.92 m (R2); web 2.16 cm/unit gave 0.99 m
LOOK = GB.Look()
OUTLINE_CLASS = "monster"
REF_SPEED = 1.65
WALK = dict(cycle=600.0, duty=0.34, lift=0.07, bob=0.018, lean=4.0, arm=0.07, crouch=-0.035, twist=4.0,
            hand_swing=0.35, hand_off=V(-0.024, 0.0, 0.016))

SPEAR_REGIONS = outline.ink_regions(dict(
    wood=Region("#7B5330", .42, .30),
    wood_dark=Region("#7B5330", .42, .30).shade_region(.30, .12),
    rope=Region("#B89A62"),
    rope_line=Region("#B89A62").shade_region(.30, .12),
    stone=Region("#9A948A", .42, .45),
    stone_dark=Region("#9A948A", .42, .45).shade_region(.30, .15),
), OUTLINE_CLASS)


def ready(A: GoblinAnims) -> BP:
    """Web READY (Goblin.ts :328-338): crouched, hunched (the rest lean), spear levelled forward at 69°. 3D: the
    spear hand rides out at the side (lat 6 u) and the tip angles 8° inward, so the 19 cm butt behind the grip
    passes outside the right hip instead of through the belly (the web draws it flat beside the body)."""
    u = A.U
    wd, wu = C.blade(70.0, lat=0.28)
    return BP(
        pelvis=A.web_pelvis(47.0, 74.0), hip_yaw=-6.0, twist=4.0, head_pitch=-6.0,
        foot_r=A.web_foot(53.0, -0.072), foot_l=A.web_foot(42.5, 0.078),
        foot_r_rot=(0.0, 0.0, 12.0), foot_l_rot=(0.0, 0.0, 24.0),
        knee_r=V(-0.45, 1, 0), knee_l=V(0.45, 1, 0),
        hand_r=V(-6.5 * u, 7.0 * u, 21.0 * u), elbow_r=V(-0.8, 0.5, -0.2),
        wpn=wd, wpn_up=wu,
        hand_l=V(4.2 * u, 5.5 * u, 20.0 * u), elbow_l=V(0.9, 0.4, -0.2), wrist_l=(10.0, 0.0, 0.0),
        flow=0.1,
    )


def u3(A, lat: float, fwd: float, up: float) -> Vector:
    """Body-space point from rig units (lat + left, fwd, up)."""
    return V(lat * A.U, fwd * A.U, up * A.U)


def install_poses(A: GoblinAnims) -> None:
    """Goblin-specific 3D spear handling on top of the web tracks (the shaft must ride outside the body)."""
    def attack_keys():
        R0 = A.ready()
        rx, ry = WEB_READY["root"]
        fn = WEB_READY["footN"]
        wind = R0.but(pelvis=A.web_pelvis(rx - 2.5, ry + 0.8), lean=-7.0, twist=R0.twist - 14.0, hip_yaw=-12.0,
                      hand_r=u3(A, -7.5, -2.0, 19.0), elbow_r=V(-0.7, 0.8, 0.1), **A._wkw(R0, 80.0, lat=0.05),
                      hand_l=u3(A, 5.5, 8.0, 23.0), elbow_l=V(0.9, 0.2, -0.3), clav_r=(2.0, -8.0), fx=0.4, jaw=8.0,
                      head_pitch=-10.0)
        mid = R0.but(pelvis=A.web_pelvis(rx + 1.0, ry + 1.0), lean=8.6, twist=R0.twist + 2.0,
                     hand_r=u3(A, -7.0, 6.0, 20.5), elbow_r=V(-0.8, 0.6, -0.1), **A._wkw(R0, 86.0, lat=0.06),
                     hand_l=u3(A, 5.0, 4.0, 22.0), foot_r=A.web_foot(fn + 3.5, R0.foot_r.x), fx=0.7, jaw=12.0)
        hit = R0.but(pelvis=A.web_pelvis(rx + 3.5, ry + 1.6), lean=18.0, twist=R0.twist + 16.0, hip_yaw=6.0,
                     head_pitch=-14.0, hand_r=u3(A, -6.5, 21.0, 21.0), elbow_r=V(-0.6, 0.2, -0.8),
                     **A._wkw(R0, 92.0, lat=0.12), hand_l=u3(A, 7.0, -4.0, 23.0), elbow_l=V(0.8, 0.3, -0.4),
                     foot_r=A.web_foot(fn + 5.0, R0.foot_r.x), foot_r_rot=(0, 0, 4), clav_r=(-2.0, 10.0),
                     fx=1.0, jaw=18.0, flow=0.3)
        return [A.look(wind), A.look(mid), A.look(hit)]

    def follow(hit):
        return hit.but(hand_r=hit.hand_r + u3(A, 0.0, 0.8, -0.4), lean=hit.lean + 2.0, jaw=12.0)

    def recover():
        R0 = A.ready()
        return R0.but(pelvis=R0.pelvis + u3(A, 0.0, 1.5, -0.6), lean=6.0, hand_r=u3(A, -8.0, 11.0, 19.5),
                      elbow_r=V(-0.9, 0.4, -0.3), **A._wkw(R0, 82.0, lat=0.16), jaw=6.0)

    def recoil():
        R0 = A.ready()
        rx, ry = WEB_READY["root"]
        return R0.but(pelvis=A.web_pelvis(rx - 3.5, ry + 1.0), lean=-19.5, head_pitch=-20.0, head_yaw=8.0,
                      twist=R0.twist - 6.0, hand_r=u3(A, -6.5, 3.0, 25.0), **A._wkw(R0, 45.0, lat=0.25),
                      hand_l=u3(A, 5.5, 2.0, 25.5), elbow_l=V(0.9, 0.3, -0.3), foot_l=R0.foot_l + V(0, -1.5 * A.U, 0),
                      clav_l=(10.0, 0.0), clav_r=(10.0, 0.0), flow=0.45, jaw=16.0)

    def death_keys():
        R0 = A.ready()
        rec = recoil()
        rx, ry = WEB_READY["root"]
        fn, ff = WEB_READY["footN"], WEB_READY["footF"]
        P0 = A.J["pelvis"]
        buckle = rec.but(pelvis=A.web_pelvis(rx - 2.5, ry + 7.0), lean=-10.0, head_pitch=-16.0, head_yaw=10.0,
                         head_roll=6.0, foot_r=A.web_foot(fn - 1.0, R0.foot_r.x), foot_l=A.web_foot(ff, R0.foot_l.x),
                         knee_l=V(0.6, 1, 0), knee_r=V(-0.6, 1, 0), hand_r=u3(A, -10.5, 4.0, 15.0),
                         elbow_r=V(-0.9, 0.3, -0.2), **A._wkw(R0, 110.0, lat=-0.15), hand_l=u3(A, 6.5, 3.0, 15.0),
                         flow=0.5, jaw=22.0)
        Pt = P0 + A.web_pelvis(rx + 1.5, 91.0 - 10.0)
        tip = buckle.but(pelvis=A.web_pelvis(rx + 1.5, 91.0 - 10.0), lean=-6.0, head_pitch=-14.0,
                         foot_r=Pt + u3(A, -3.0, -5.0, -11.0), foot_l=Pt + u3(A, 3.0, -1.0, -12.5),
                         hand_r=Pt + u3(A, -12.0, 0.0, 9.0), elbow_r=V(-0.9, 0.2, -0.3),
                         wpn=V(-0.45, -0.25, 0.86), wpn_up=V(0, -1, 0), hand_l=Pt + u3(A, 8.5, 1.0, 11.0),
                         spin=-54.0, spin_axis=V(1, 0, 0), flow=0.6, jaw=26.0)
        Pl = P0 + A.web_pelvis(rx + 2.0, 91.0 - 4.8)
        # on its back: the spear arm flung out, the spear lying on the ground beside the head (pre-spin it points
        # up and out — the plane that the −87° fall lays flat)
        lying = R0.but(pelvis=A.web_pelvis(rx + 2.0, 91.0 - 4.8), lean=-12.0, head_pitch=-10.0, head_roll=-24.0,
                       head_yaw=10.0, twist=-4.0, hip_yaw=0.0,
                       foot_r=Pl + u3(A, -4.0, -7.0, -14.0), foot_l=Pl + u3(A, 5.5, -2.5, -14.0),
                       foot_r_rot=(30.0, 0, 25), foot_l_rot=(18.0, 0, 40), knee_l=V(0.8, 0.6, 0), knee_r=V(-0.4, 1, 0),
                       hand_r=Pl + u3(A, -12.0, -2.5, 8.0), elbow_r=V(-0.4, 0.2, -1.0),
                       wpn=V(-0.55, 0.06, 0.83), wpn_up=V(0, -1, 0),
                       hand_l=Pl + u3(A, 11.0, -1.5, 7.5), elbow_l=V(0.6, 0.2, -1.0),
                       spin=-87.0, spin_axis=V(1, 0, 0), flow=0.05, jaw=18.0)
        return [rec, buckle, tip, lying]

    A.overrides.update(attack_keys=attack_keys, follow=follow, recover=recover, recoil=recoil,
                       death_keys=death_keys)


# ── spear ──────────────────────────────────────────────────────────────────────────────────────────────
def build_spear(pal: Palette) -> bpy.types.Object:
    """Crude spear (web spear :284-294, ≈ 60 cm): gnarled shaft from 9 u behind the grip to 12 u ahead, rope
    binding, knapped flint head to 18.4 u. Grip at the origin, tip along +Y (weapon_r socket)."""
    u = WEB.U
    b = M.Builder(SPEAR, pal, prefix="goblin_spear")
    b.regions(**SPEAR_REGIONS)
    rng = scene.rng(SPEAR, "shaft")
    ys = np.linspace(-9.0, 12.6, 9)
    pts = [Vector((u * 0.18 * math.sin(y * 0.55) + u * rng.uniform(-0.08, 0.08), y * u,
                   u * 0.15 * math.cos(y * 0.4))) for y in ys]
    rad = [u * (0.62 if y < -8.0 else 0.72 - 0.06 * (y + 9) / 21.6) for y in ys]
    shaft = M.tube(pts, rad, sides=8, up=(0, 0, 1), dome0=u * 0.35, dome1=0.0)
    sid = b.add(shaft, "wood", Bind.rigid("root"), name="shaft", hull_pad=True)
    for y in (-3.5, 4.5):                                             # knots
        k = M.ellipsoid((u * 0.55, u * 0.6, u * 0.5), (u * 0.62, y * u, 0.0), sides=8, rings=6)
        b.add(k, "wood_dark", Bind.rigid("root"), name="knot", outline=False, covered_by=sid)
    # flint head: a faceted leaf (flat-shaded diamond section, jittered knapping along the edges)
    head = []
    L0, L1 = 11.2 * u, 18.6 * u
    for i in range(9):
        t = i / 8
        y = L0 + (L1 - L0) * t
        w = 1.85 * u * math.sin(math.pi * (0.18 + 0.82 * t) ** 0.85) * (1 - t) ** 0.35 + 0.08 * u
        jit = 0.18 * u * math.sin(i * 2.7)
        head.append((y, w + jit, 0.55 * u * (1 - 0.7 * t) + 0.12 * u))
    import bmesh
    bm = bmesh.new()
    rows = []
    for y, w, th in head:
        rows.append([bm.verts.new((w, y, 0.0)), bm.verts.new((0.0, y, th)), bm.verts.new((-w, y, 0.0)),
                     bm.verts.new((0.0, y, -th))])
    for r0, r1 in zip(rows, rows[1:]):
        for j in range(4):
            bm.faces.new((r0[j], r0[(j + 1) % 4], r1[(j + 1) % 4], r1[j]))
    c0 = bm.verts.new((0.0, L0 - 0.6 * u, 0.0))
    c1 = bm.verts.new((0.0, L1 + 0.5 * u, 0.0))
    for j in range(4):
        bm.faces.new((rows[0][(j + 1) % 4], rows[0][j], c0))
        bm.faces.new((rows[-1][j], rows[-1][(j + 1) % 4], c1))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    stone = M.Part(bm).flat()
    hid = b.add(stone, "stone", Bind.rigid("root"), name="flint", hull_pad=True)
    # knapping scars: darker facets painted along the edges (both faces)
    for sgz in (1, -1):
        for i in range(4):
            t = 0.15 + i * 0.2
            y = L0 + (L1 - L0) * t
            sc = M.ellipsoid((0.5 * u, 0.75 * u, 0.12 * u), (0.55 * u * (1 - t) * (1 if i % 2 else -1), y,
                                                             sgz * (0.45 * u * (1 - 0.7 * t) + 0.08 * u)),
                             sides=6, rings=4)
            b.add(sc.flat(), "stone_dark", Bind.rigid("root"), name="scar", outline=False, covered_by=hid)
    # rope binding over the head's tang
    for k in range(5):
        y = (10.6 + k * 0.48) * u
        ring = M.tube([(0, y - 0.22 * u, 0), (0, y + 0.22 * u, 0)], [0.9 * u, 0.9 * u], sides=10, dome0=0.0,
                      dome1=0.0)
        b.add(ring, "rope" if k % 2 == 0 else "rope_line", Bind.rigid("root"), name="binding", outline=False,
              covered_by=sid)
    obj = b.build(SPEAR)
    from kit import shading
    mt = shading.toon_material(pal)
    outline.bake_hull(obj, outline.WIDTH_CM[OUTLINE_CLASS] / 100.0, mt, shading.outline_material(pal))
    for m in outline.hull_color_report(obj, pal, OUTLINE_CLASS):
        print(f"[ink] WARNING {SPEAR}: {m}")
    obj["af_outline_class"] = OUTLINE_CLASS
    return obj


# ── body ───────────────────────────────────────────────────────────────────────────────────────────────
def build_body(pal: Palette, kit: GB.GoblinKit) -> bpy.types.Object:
    from kit import asset
    b = M.Builder(ASSET, pal, prefix=PREFIX)
    b.regions(**GB.regions_for(LOOK))
    kit.build_parts(b)
    body = asset.finish_mesh(b, pal, OUTLINE_CLASS, rig=kit.rig, grounding_height=0.92)
    return body


def build_all():
    scene.reset()
    pal = Palette(GB.FAMILY)
    kit = GB.GoblinKit(ASSET, PREFIX, WEB, LOOK)
    body = build_body(pal, kit)
    spear = build_spear(pal)
    return pal, kit, body, spear


WEB_READY = dict(root=(47.0, 74.0), footN=53.0, footF=42.5, lean=0.34, head=-0.30)


def anims(kit) -> GoblinAnims:
    A = GoblinAnims(kit, REF_SPEED, WALK, ready, style="thrust", contact_wpn=92.0, web_ready=WEB_READY)
    install_poses(A)
    return A


def clips(A: GoblinAnims) -> list:
    return [A.idle(), A.walk(), A.attack(), A.hurt(), A.death(),
            A.stun(droop=55.0, hand_r=u3(A, -7.5, 4.0, 17.0), wlat=0.08)]


GRIP_PARTS = ("fist_r", "knuckle_r", "thumb_r", "claw_r")     # the fist holding the shaft (QA baseline)


def spear_grip(co) -> bool:
    """Spear faces inside the fist (the shaft between the knuckles), item-local."""
    return abs(co.y) < 0.06


# ── quest-hunt leaders (§4.5): attachments + preview ─────────────────────────────────────────────────────
def hunt_items(pal: Palette, kit) -> list:
    """The variant attachments (not attached by default; manifest ``variants`` lists who wears what)."""
    import mship
    note = {"variantScale": GH.SCALE, "attach": "bone socket, identity transform (weapon-socket convention)"}
    return [mship.Item(GH.build_redcap(pal, kit), GH.REDCAP, "head", variant="hunt_redcap_gruk", category="Monsters",
                       extra=dict(note)),
            mship.Item(GH.build_pendant(pal, kit), GH.PENDANT, GH.PENDANT_BONE, variant="hunt_pendant_thief",
                       category="Monsters", extra=dict(note, glint={"region": "gem", "color": "#E0283C"})),
            mship.Item(GH.build_lootsack(pal, kit), GH.LOOTSACK, "pelvis", variant="hunt_pendant_thief",
                       category="Monsters", extra=dict(note))]


def hunt_qa_groups(body) -> dict:
    """Body minus the parts an attachment is seated on (the cap's rim sits on the skull, brow and ear roots; the
    locket's chain hangs from the necklace cord) — everything else must stay clear in every frame."""
    import mship
    return {"body_nocap": mship.parts_except(body, ("skull", "brow", "ear_l", "ear_r")),
            "body_nocord": mship.parts_except(body, ("neck_cord",))}


HUNT_QA_PAIRS = {"capBody": ("item:" + GH.REDCAP, "body_nocap"), "pendantBody": ("item:" + GH.PENDANT, "body_nocord"),
                 "sackBody": ("item:" + GH.LOOTSACK, "body"), "pendantSpear": ("item:" + GH.PENDANT, "item:" + SPEAR),
                 "sackSpear": ("item:" + GH.LOOTSACK, "item:" + SPEAR)}


def hunt_previews(ms, rv, actions) -> None:
    """``hunt_leaders.png``: both quest-hunt leaders — the goblin ×1.25 with its attachments and the elite crown
    (``fx_overhead``, camera-yawed, unscaled, glow card) — native 1080p crops at the W1 camera, the ink measured
    on the leader without the crown FX (the crown ships its own gate); ``hunt_close.png``: close-ups se / front /
    back with the crown."""
    import elite_crown as EC
    from kit import anim, review
    rig = ms.A.rig
    acts = dict(zip([c.name for c in ms.clips], actions))
    crown = EC.build_crown(ms.pal)
    crown.hide_render = True
    base, glows0 = list(rv.meshes), rv.glows
    cells, close = [], []
    keys = [("Idle", 0, "se"), ("Attack01", 15, "sw")]
    try:
        for vid, var in GH.VARIANTS.items():
            objs = [it.obj for it in ms.items if it.variant == vid]
            for o in objs:
                o.hide_render = False
            rig.obj.scale = (var["scale"],) * 3
            label = var["displayName"]["en"].upper()
            for clip, fr, fac in keys:
                rv.meshes[:] = base + objs
                rv.glows = None
                rv.facing(fac)
                anim.assign(rig, acts[clip])
                anim.set_frame(fr)
                review.setup_render((1920, 1080), 8)
                cam = review.game_camera()
                zc = 0.62 * var["scale"]
                rv.game_outline(1080, Vector((0, 0, zc)))
                c = review.project(cam, Vector((0, 0, zc)))
                hw = 0.062
                hh = hw * 1920 / 1080
                box = (c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh)
                _, st = rv.game_shot(box, measure=True, tag=f"hunt_leaders.png {vid} {clip} F{fr} {fac}")
                EC.place_on(crown, rig, cam)
                crown.hide_render = False
                rv.meshes.append(crown)
                rv.glows = EC.crown_glow(crown)
                rv.game_outline(1080, Vector((0, 0, zc)))
                img, _ = rv.game_shot(box, measure=False)
                crown.hide_render = True
                review.draw_text(img, 3, 3, f"{label} X{var['scale']:g} {clip} F{fr} {fac}")
                review.draw_text(img, 3, img.shape[0] - 10, rv.ink_label(st), rgb=(255, 220, 140))
                cells.append(img)
            # close-ups at READY (Idle F0) with the crown
            anim.assign(rig, acts["Idle"])
            anim.set_frame(0)
            res = (256, 300)
            for fac, pitch in (("se", -35.0), ("front", -30.0), ("back", -35.0)):
                rv.facing(fac)
                rv.meshes[:] = base + objs
                review.setup_render(res, 8)
                EC.place_on(crown, rig, review.game_camera())
                crown.hide_render = False
                rv.meshes.append(crown)
                pts = review.mesh_points(rv.meshes)
                cam = review.frame_points(pts, res, fov_h=26, pitch_ue=pitch, fill=0.92)
                EC.place_on(crown, rig, cam)
                rv.ink(at=Vector(pts.mean(0)))
                img = review.render_array()
                crown.hide_render = True
                review.draw_text(img, 3, 3, f"{label} {fac.upper()}")
                close.append(img)
            for o in objs:
                o.hide_render = True
    finally:
        rig.obj.scale = (1.0, 1.0, 1.0)
        rv.meshes[:] = base
        rv.glows = glows0
        crown.hide_render = True
        rv.facing("se")
    rv._save("hunt_leaders.png", review.grid(cells, len(cells)))
    rv._save("hunt_close.png", review.grid(close, 3))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)


def quick(out: Path) -> None:
    import mship
    pal, kit, body, spear = build_all()
    A = anims(kit)
    mship.quick_review(ASSET, kit.rig, [body], [(spear, "weapon_r")], A, out, 0.92, OUTLINE_CLASS)


def anim_quick(out: Path, names, facing: str = "se") -> None:
    import mship
    pal, kit, body, spear = build_all()
    A = anims(kit)
    R.attach_to_bone(spear, kit.rig, "weapon_r")
    A.bind_meshes(body, {"weapon_r": spear})
    cs = [c for c in clips(A) if not names or c.name in names]
    mship.anim_review(kit.rig, [body], [(spear, "weapon_r")], cs, TOKEN, out, 0.92, OUTLINE_CLASS, facing=facing)


def ship(previews: bool = True, verify: bool = True) -> int:
    import mship
    pal, kit, body, spear = build_all()
    A = anims(kit)
    R.attach_to_bone(spear, kit.rig, "weapon_r")
    A.bind_meshes(body, {"weapon_r": spear})
    cs = clips(A)
    U = WEB.U
    items = [mship.Item(spear, SPEAR, "weapon_r", "spear",
                        sockets=[{"name": "tip", "pos": (0.0, 18.6 * U, 0.0)}, {"name": "mid", "pos": (0.0, 6.0 * U, 0.0)}],
                        extra={"lengthCm": round(27.6 * U * 100.0, 1)}, grip=spear_grip)] + hunt_items(pal, kit)
    ms = mship.MonsterShip(
        asset=ASSET, token=TOKEN, preview_dir="mon_goblin", pal=pal, A=A, body=body, items=items, clips=cs,
        outline_class=OUTLINE_CLASS, game_ids=["monster_goblin", *GH.VARIANTS], blob_radius=round(10 * U * 1.0, 3), height=0.92,
        display={"zh-CN": "哥布林", "en": "Goblin"},
        fx={"telegraph": {"normal": "#FFC4B0", "elite": "#FF7A5C", "untilMs": 155},
            "attackTrail": {"color": "#E8E0CC", "alpha": 0.45, "sockets": ["tip", "mid"], "item": SPEAR}},
        qa_groups={"body_nf": mship.parts_except(body, GRIP_PARTS), **hunt_qa_groups(body)},
        qa_pairs={"spearBody": ("item:" + SPEAR, "body_nf"), **HUNT_QA_PAIRS},
        qa_limits={"ground": -0.5, "spearBody": 0, **{k: 0 for k in HUNT_QA_PAIRS}},
        variants=GH.VARIANTS, extra_previews=hunt_previews, variant_previews=("hunt_leaders.png", "hunt_close.png"),
        game_keys=[("Idle", 0, "se"), ("Walk", 9, "se"), ("Attack01", 5, "se"), ("Attack01", 15, "se"),
                   ("Attack01", 15, "sw"), ("Hurt", 0, "se"), ("Death", 30, "se"), ("Stun", 10, "se")],
        close_keys=[("Attack01", 5, "se"), ("Attack01", 10, "se"), ("Attack01", 15, "se"), ("Attack01", 15, "side"),
                    ("Hurt", 0, "se"), ("Death", 10, "se"), ("Death", 20, "se"), ("Death", 30, "se"),
                    ("Stun", 10, "front"), ("Walk", 9, "side")],
        skeleton_bones_required=("root", "pelvis", "head", "jaw", "weapon_r", "weapon_l", "ear_l_02", "loin_f_02",
                                 "pelt_03", "headdress_04", "totem_feather_02", "ik_foot_l", "ik_hand_gun"),
        extra={"refSpeedCmS": REF_SPEED * 100.0, "attackStyle": "thrust",
               "notes": "Clips in place; the core moves the actor (play rate = v / ref). Death is followed by the "
                        "knockback impulse and a dithered fade (spec §4). Stun (C5) loops."},
    )
    res = mship.ship(ms, previews=previews, verify=verify)
    return mship.main_result(res)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    scratch = Path("/tmp/claude-0/-home-user-abyssfire/b3225f99-25a3-5b78-996f-643be8685889/scratchpad/mq/goblin")
    if "--quick" in args:
        k = args.index("--quick")
        quick(Path(args[k + 1]) if len(args) > k + 1 and not args[k + 1].startswith("--") else scratch)
    elif "--anims" in args:
        names = [a for a in args[args.index("--anims") + 1:] if not a.startswith("--")]
        fac = next((a.split("=")[1] for a in args if a.startswith("--facing=")), "se")
        anim_quick(scratch / "anims", names, fac)
    else:
        sys.exit(ship(previews="--no-previews" not in args, verify="--no-verify" not in args))

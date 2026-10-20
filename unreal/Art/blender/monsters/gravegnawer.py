"""啃冢者 Gravegnawer — ``SK_Mon_Gravegnawer`` (dark remake A4; monsters.md §2.4 swarmer + corpse-eater, §5).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/gravegnawer.py
        [--quick] [--anims Clip … --facing=se] [--no-previews] [--no-verify]

A re-skin of the goblin (``goblin.py``) through ``gnawers.py``: a hairless ash-grey carrion beast, hunched low on a
long forward-thrust neck, a long needle-toothed muzzle on a biting jaw, bone-yellow eyeshine (the family marker),
torn ear stubs, an emaciated ribbed torso with a knobbed spine, arms + 15 % ending in hooked claws, a scrap of
crusade tabard on a rope belt and a rag knotted round its left arm (trophies). Weapon: a broken crusade spearhead
lashed to a stick (``SM_MonWeapon_LashedSpear`` on ``weapon_r``; the goblin spear shortened, no bone weapon).

Clips (the goblin's set, retimed): Idle 900 (knuckle-drag: the back near level, the long free arm hanging to the
ground on its knuckles, the head sweeping and sniffing), Walk 480 (lower and faster: 1.98 m/s = speed 66 × 0.03, no
slide, the free hand lifted off the ground), Attack01 600 (a lunging stab and bite: the jaw gapes on the wind-up and
snaps shut on the Contact; WindupEnd 186, Contact 300 — monsters.md "quick bites, Contact ≈ 300 ms"), Hurt 220,
Death 600 (the chest pitches into the ground, then the carcass rolls onto its side), Stun 600. Game id
``monster_gravegnawer`` (Data ``spriteKey``).

Ink: every protruding detail inks on its own hull (claws, toe claws, the belt knot and ends, the rag tails, the
spear's lashing and rag) — covered by a neighbour, a padded proxy balloons round them (the fist's grew 7 cm round the
claws, the spear rag's 13 cm into a dark wedge under the blade).
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "march", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import mesh as M, outline, rig as R, scene, shading  # noqa: E402
from kit.paths import scratch_root  # noqa: E402
from kit.palette import Palette  # noqa: E402
from kit.rig import Bind  # noqa: E402

import common as C  # noqa: E402
from common import BP, K, V  # noqa: E402
import gnawers as GN  # noqa: E402
import goblins as GB  # noqa: E402
import mgeo as G  # noqa: E402

ASSET = "SK_Mon_Gravegnawer"
TOKEN = "Mon_Gravegnawer"
SPEAR = "SM_MonWeapon_LashedSpear"
PREFIX = "gravegnawer"
GAME_ID = "monster_gravegnawer"
U = 0.0222
WEB = GB.Web(U=U, thigh=9.4, shin=9.4, upper_arm=7.6 * 1.15, fore_arm=7.2 * 1.15, torso=13.0, neck=8.2, ankle=1.6,
             hip=2.1, shoulder=4.2, lean=0.95, head_tilt=0.25, root=(47.0, 74.0), bulk=0.86, head_k=0.84,
             foot_len=8.8, grin=0.0)
SHAPE = GN.Shape(head_drop=-1.8)
LOOK = GB.Look()                 # unused colours (the march swatches come from gnawers.GNAWER_ALIAS)
HEIGHT = 0.86                    # framing height of the hunched READY (crown ≈ 0.80 m, the spine hump ≈ 0.74 m)
REF_SPEED = 66 * 0.03            # monsters.md §2.0: speed 66 (data unit: tiles/s = speed × 0.03)
WALK = dict(cycle=480.0, duty=0.36, lift=0.052, bob=0.016, lean=5.0, arm=0.09, crouch=-0.05, twist=6.0,
            hand_swing=0.3, hand_off=V(-0.02, 0.0, 0.01))
CONTACT_MS = 300.0
ATTACK_MS = 600.0
MARKER = ("gnawer.eye",)

LOD1_RATIO = 0.55
LOD1_HULL = 0.8
LOD1_SKIP = ("rib", "spine_knob", "knuckle", "thumb", "belt_twist", "belt_end", "tooth_low", "nose", "blade",
             "arm_rag_tail")

SPEAR_ALIAS = dict(wood="gnawer.stick", rope="gnawer.rope", rope_line="gnawer.stick", steel="gnawer.spearhead",
                   nick="gnawer.hide_line", cloth="gnawer.trophy")
STICK_BACK, STICK_FRONT = -0.10, 0.27          # m: butt and lashed end of the stick (grip at 0)
HEAD_Y0, HEAD_TIP = 0.235, 0.43                 # m: the spearhead's socket start and the broken tip


# ── body ───────────────────────────────────────────────────────────────────────────────────────────────
def build_body(pal: Palette, kit: GN.GnawerKit, lod: int = 0) -> bpy.types.Object:
    from kit import asset
    b = GN.march_builder(ASSET, pal, GN.GNAWER_ALIAS, PREFIX, lod, LOD1_SKIP, LOD1_RATIO,
                         hull_ratio=LOD1_HULL if lod else 1.0)
    kit.add_head(b)
    kit.add_torso(b)
    kit.add_arm_rag(b, "l")
    for s, sg in (("l", 1), ("r", -1)):
        kit.add_arm(b, s, sg)
        kit.add_leg(b, s, sg)
    body = asset.finish_mesh(b, pal, GN.OUTLINE_CLASS, rig=kit.rig, grounding_height=HEIGHT,
                             name=f"{ASSET}_LOD{lod}" if lod else None)
    body["af_lod"] = lod
    return body


# ── weapon ─────────────────────────────────────────────────────────────────────────────────────────────
def build_spear(pal: Palette) -> bpy.types.Object:
    """A gnarled stick (≈ 37 cm) with a broken crusade spearhead lashed on its end (one wing snapped off at the
    tip, a nicked edge), a rope lashing and a tabard rag knotted under it. Grip at the origin, tip along +Y."""
    import bmesh
    b = GN.MarchBuilder(SPEAR, pal, SPEAR_ALIAS, prefix="gnawer_spear")
    b.declare()
    rig = Bind.rigid("root")
    rng = scene.rng(SPEAR, "stick")
    ys = np.linspace(STICK_BACK, STICK_FRONT, 8)
    pts = [Vector((0.003 * math.sin(y * 31.0) + rng.uniform(-0.0015, 0.0015), y, 0.0025 * math.cos(y * 23.0)))
           for y in ys]
    rad = [0.0125 - 0.002 * (y - STICK_BACK) / (STICK_FRONT - STICK_BACK) for y in ys]
    sid = b.add(M.tube(pts, rad, sides=8, up=(0, 0, 1), dome0=0.006, dome1=0.0), "wood", rig, name="stick",
                hull_pad=True)
    for y in (-0.05, 0.08):                                    # knots
        k = M.ellipsoid((0.007, 0.009, 0.006), (0.009, y, 0.0), sides=6, rings=4)
        b.add(k, "wood", rig, name="knot", outline=False, covered_by=sid)
    # socket: the spearhead's iron tube over the stick end
    sock = M.tube([Vector((0, HEAD_Y0, 0)), Vector((0, HEAD_Y0 + 0.07, 0))], [0.0145, 0.0105], sides=10,
                  dome0=0.0, dome1=0.0)
    b.add(sock, "steel", rig, name="socket", hull_pad=True)
    # the blade: a leaf with a mid ridge (diamond section); the right wing is broken off below the tip — a jagged
    # step — and the point leans left
    y0, y1 = HEAD_Y0 + 0.065, HEAD_TIP
    rows = 9

    def section(t: float, i: int = 0) -> tuple[float, float, float, float]:
        """(left half width, right half width, ridge x, ridge half thickness) at ``t`` along the blade."""
        w = 0.030 * math.sin(math.pi * (0.16 + 0.84 * t) ** 0.8) * (1 - t) ** 0.3 + 0.002
        wr = w * (0.35 + 0.25 * ((i % 2) == 0)) if t > 0.42 else w      # the snapped right wing
        return w, wr, -0.006 * t * t, 0.0055 * (1 - 0.6 * t) + 0.0012
    bm = bmesh.new()
    ring = []
    for i in range(rows):
        t = i / (rows - 1)
        y = y0 + (y1 - y0) * t
        wl, wr, cx, th = section(t, i)
        ring.append([bm.verts.new((cx + wl, y, 0.0)), bm.verts.new((cx, y, th)), bm.verts.new((cx - wr, y, 0.0)),
                     bm.verts.new((cx, y, -th))])
    for r0, r1 in zip(ring, ring[1:]):
        for j in range(4):
            bm.faces.new((r0[j], r0[(j + 1) % 4], r1[(j + 1) % 4], r1[j]))
    c0 = bm.verts.new((0.0, y0 - 0.004, 0.0))
    c1 = bm.verts.new((-0.007, y1 + 0.012, 0.0))
    for j in range(4):
        bm.faces.new((ring[0][(j + 1) % 4], ring[0][j], c0))
        bm.faces.new((ring[-1][j], ring[-1][(j + 1) % 4], c1))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    blade = M.Part(bm).flat()
    hid = b.add(blade, "steel", rig, name="blade", hull_pad=True)
    # nicks along the left edge (dark chips lying on both faces of the diamond section, 0.5 mm proud)
    for sz in (1, -1):
        for t, ln in ((0.30, 0.012), (0.55, 0.009), (0.75, 0.007)):
            y = y0 + (y1 - y0) * t
            wl, _, cx, th = section(t)
            x = cx + wl * 0.62
            z = th * (1.0 - 0.62) + 0.0004
            ch = M.ellipsoid((wl * 0.22, ln * 0.5, 0.0008), (x, y, sz * z), sides=6, rings=3)
            ch.transform(Matrix.Translation((x, y, sz * z)) @ Matrix.Rotation(-sz * math.atan2(th, wl), 4, "Y")
                         @ Matrix.Translation((-x, -y, -sz * z)))
            b.add(ch.flat(), "nick", rig, name="nick", outline=False, covered_by=hid)
    # rope lashing round the socket and the stick end: one inked binding (a closed proxy round all six turns —
    # covered by the socket, its padded proxy stretched 3 cm down the stick)
    ly0, ly1 = HEAD_Y0 - 0.0315, HEAD_Y0 + 0.0205
    lash_hull = M.tube([Vector((0, ly0, 0)), Vector((0, ly1, 0))], [0.0166, 0.0167], sides=10, dome0=0.0012,
                       dome1=0.0012)
    lid = None
    for k in range(6):
        y = HEAD_Y0 - 0.028 + k * 0.009
        r = 0.0155 if y < HEAD_Y0 else 0.0165
        rg = M.tube([(0, y - 0.0035, 0), (0, y + 0.0035, 0)], [r, r], sides=10, dome0=0.0, dome1=0.0)
        reg = "rope" if k % 2 == 0 else "rope_line"
        if lid is None:
            lid = b.add(rg, reg, rig, name="lashing", hull=lash_hull, hull_pad=True)
        else:
            b.add(rg, reg, rig, name="lashing", outline=False, covered_by=lid)
    # the trophy rag knotted round the stick under the lashing, two short tails (each inked on its own)
    rag = M.tube([(0, 0.168, 0), (0, 0.192, 0)], [0.0158, 0.0152], sides=10, dome0=0.0, dome1=0.0)
    b.add(rag, "cloth", rig, name="rag", hull_pad=True)
    for j, (dx, dz, ln) in enumerate(((0.012, -0.020, 0.07), (-0.010, -0.016, 0.05))):
        p0 = Vector((dx * 0.5, 0.18, -0.012))
        p1 = p0 + Vector((dx, -0.012, dz))
        p2 = p0 + Vector((dx * 1.4, -0.02, -ln))
        tail = G.chain_strip([p0, p1, p2], [0.016, 0.013, 0.008], Vector((0, 1, 0)), Vector((1 if dx > 0 else -1, 0, 0)),
                             0.003, tails=2, seed=41 + j, cols=3, per_seg=1)
        b.add(tail, "cloth", rig, name="rag_tail", sub_regions={1: "cloth", 2: "cloth"}, hull_pad=True)
    obj = b.build(SPEAR)
    mt = shading.toon_material(pal)
    outline.bake_hull(obj, outline.WIDTH_CM[GN.OUTLINE_CLASS] / 100.0, mt, shading.outline_material(pal))
    for m in outline.hull_color_report(obj, pal, GN.OUTLINE_CLASS):
        print(f"[ink] WARNING {SPEAR}: {m}")
    obj["af_outline_class"] = GN.OUTLINE_CLASS
    return obj


def build_all():
    scene.reset()
    pal = Palette(GN.FAMILY)
    kit = GN.GnawerKit(ASSET, PREFIX, WEB, LOOK, SHAPE)
    body = build_body(pal, kit)
    spear = build_spear(pal)
    return pal, kit, body, spear


# ── poses ──────────────────────────────────────────────────────────────────────────────────────────────
WEB_READY = dict(root=(47.0, 76.0), footN=54.0, footF=41.0, lean=0.95, head=0.25)
# deg past the rest lean: the back near level so the shoulders drop within reach of the ground (the long arms
# knuckle-drag); the head counter-pitches it (the muzzle stays level)
READY_LEAN = 18.0
FLAP_THIGH_R = 3.4             # u: the loin scrap's leg envelope (the deeper crouch brings the thighs forward)


def u3(A, lat: float, fwd: float, up: float) -> Vector:
    return V(lat * A.U, fwd * A.U, up * A.U)


# knuckle-drag (monsters.md 2.4): the free hand's knuckles rest on the ground ahead of the left foot, the long claws
# curled under
READY_HAND_L = (6.5, 9.5, 3.4)
READY_CLAV_L = (-14.0, 8.0)        # the left shoulder dropped and swung forward to the ground
READY_WRIST_L = (-55.0, 0.0, 0.0)
READY_HAND_R = (-8.5, 11.0, 14.0)   # the spear hand out past the right knee


def ready(A) -> BP:
    """Crouched low on bent legs, the long neck thrust forward, the spear low before the right hip angled down at the
    ground ahead, the free left fist knuckled on the ground (knuckle-drag)."""
    wd, wu = C.blade(104.0, lat=0.12)
    return BP(
        pelvis=A.web_pelvis(*WEB_READY["root"]), lean=READY_LEAN, hip_yaw=-6.0, twist=5.0,
        head_pitch=-4.0 - READY_LEAN,
        foot_r=A.web_foot(54.0, -0.075), foot_l=A.web_foot(41.0, 0.082),
        foot_r_rot=(0.0, 0.0, 14.0), foot_l_rot=(0.0, 0.0, 22.0), knee_r=V(-0.5, 1, 0), knee_l=V(0.5, 1, 0),
        hand_r=u3(A, *READY_HAND_R), elbow_r=V(-0.9, 0.2, -0.3), wpn=wd, wpn_up=wu,
        hand_l=u3(A, *READY_HAND_L), elbow_l=V(0.8, -0.3, 0.2), wrist_l=READY_WRIST_L, clav_l=READY_CLAV_L,
        flow=0.1)


def install_poses(A) -> None:
    """The lunging stab and bite: coil back with the jaw gaping and the spear drawn, lunge low, the spear thrust and
    the jaws snapping shut together on the contact; the free claws rake forward."""
    rx, ry = WEB_READY["root"]
    fn = WEB_READY["footN"]

    def lean_head(lean: float, muzzle: float) -> dict:
        """Body lean (deg) and the head pitch that keeps the muzzle at ``muzzle`` deg below level (READY: −4)."""
        return {"lean": lean, "head_pitch": muzzle - lean}

    def attack_keys():
        R0 = A.ready()
        wind = R0.but(pelvis=A.web_pelvis(rx - 2.5, ry + 1.2), **lean_head(4.0, -24.0), twist=R0.twist - 14.0,
                      hip_yaw=-12.0, hand_r=u3(A, -8.5, 5.0, 17.0), elbow_r=V(-0.9, 0.5, 0.0),
                      **A._wkw(R0, 92.0, lat=0.06), hand_l=u3(A, 8.0, 10.5, 9.5), elbow_l=V(0.9, 0.0, -0.2),
                      wrist_l=(-20.0, 0.0, 0.0), clav_r=(4.0, -8.0), fx=0.4, jaw=30.0)
        mid = R0.but(pelvis=A.web_pelvis(rx + 1.5, ry + 0.6), **lean_head(14.0, -4.0), twist=R0.twist + 2.0,
                     hand_r=u3(A, -8.0, 13.0, 15.5), elbow_r=V(-0.9, 0.3, -0.2), **A._wkw(R0, 98.0, lat=0.08),
                     hand_l=u3(A, 8.0, 13.0, 9.0), foot_r=A.web_foot(fn + 3.0, R0.foot_r.x), fx=0.7, jaw=38.0)
        hit = R0.but(pelvis=A.web_pelvis(rx + 5.0, ry + 1.4), **lean_head(24.0, 18.0), twist=R0.twist + 14.0,
                     hip_yaw=6.0, hand_r=u3(A, -6.0, 23.0, 12.5), elbow_r=V(-0.7, 0.2, -0.6),
                     **A._wkw(R0, 100.0, lat=0.1), hand_l=u3(A, 9.0, 16.0, 7.5), elbow_l=V(0.9, 0.2, -0.3),
                     wrist_l=(-10.0, 0.0, 0.0), foot_r=A.web_foot(fn + 5.5, R0.foot_r.x), foot_r_rot=(0, 0, 6),
                     clav_r=(-2.0, 10.0), fx=1.0, jaw=2.0, flow=0.3)
        return [A.look(wind, 0.6), A.look(mid, 0.6), A.look(hit, 0.6)]

    def follow(hit):
        return hit.but(hand_r=hit.hand_r + u3(A, 0.0, 0.8, -0.4), lean=hit.lean + 2.0, jaw=12.0)

    def recover():
        R0 = A.ready()
        return R0.but(pelvis=R0.pelvis + u3(A, 0.0, 1.8, -0.4), lean=READY_LEAN + 2.0, hand_r=u3(A, -8.0, 15.0, 14.0),
                      hand_l=R0.hand_l + u3(A, 0.0, 0.0, 2.0),
                      **A._wkw(R0, 100.0, lat=0.1), jaw=8.0)

    def recoil():
        R0 = A.ready()
        return R0.but(pelvis=A.web_pelvis(rx - 3.5, ry + 0.4), **lean_head(-2.0, -46.0), head_yaw=10.0,
                      twist=R0.twist - 8.0, hand_r=u3(A, -8.0, 7.0, 18.0), **A._wkw(R0, 70.0, lat=0.2),
                      hand_l=u3(A, 7.5, 7.0, 13.0), elbow_l=V(0.9, 0.2, -0.3), wrist_l=(0.0, 0.0, 0.0),
                      foot_l=R0.foot_l + V(0, -1.5 * A.U, 0), clav_l=(12.0, 0.0), clav_r=(12.0, 0.0), flow=0.45,
                      jaw=26.0)

    A.overrides.update(attack_keys=attack_keys, follow=follow, recover=recover, recoil=recoil)


def idle_layer(A):
    """Knuckle-drag idle: the head sweeps and sniffs (twice per loop), the jaw chatters, the shoulders roll."""
    def extra(bp: BP, t: float) -> BP:
        ph = 2 * math.pi * t / IDLE_MS
        sniff = max(0.0, math.sin(2 * ph * 2)) ** 3
        return bp.but(head_yaw=bp.head_yaw + 14.0 * math.sin(ph), head_pitch=bp.head_pitch - 6.0 * sniff,
                      jaw=bp.jaw + 6.0 * max(0.0, math.sin(ph * 3 + 0.5)) ** 2,
                      twist=bp.twist + 3.0 * math.sin(ph + 0.8))
    return extra


IDLE_MS = 900.0
# dazed: the spear sags 8° from READY in the hand held out past the right knee (the READY hand dropped 2.5 u put the
# shaft through the right knee; a 30° droop sank the head 13 cm into the ground)
STUN_DROOP = 8.0
STUN_WLAT = 0.0
STUN_HAND = (-11.5, 9.0, 15.5)


def anims(kit):
    A = GN.GnawerAnims(kit, REF_SPEED, WALK, ready, style="thrust", contact_wpn=100.0, web_ready=WEB_READY)
    A.contact_ms, A.attack_ms = CONTACT_MS, ATTACK_MS
    A.walk_hand_l_lift, A.stun_hand_l_lift = 3.5, 5.0     # off the ground while it lopes / sags (claws clear)
    A.FLAP_THIGH_R = FLAP_THIGH_R
    install_poses(A)
    return A


def clips(A) -> list:
    return [A.idle(length=IDLE_MS, weapon_wave=1.5, extra=idle_layer(A)), A.walk(), A.attack(), A.hurt(220.0),
            A.death(600.0), A.stun(droop=STUN_DROOP, wlat=STUN_WLAT, hand_r=u3(A, *STUN_HAND))]


GRIP_PARTS = ("fist_r", "knuckle_r", "thumb_r", "claw_r")


def spear_grip(co) -> bool:
    return abs(co.y) < 0.05


def quick(out: Path, jaw: float = 0.0) -> None:
    import mship
    GN.march_stage()
    pal, kit, body, spear = build_all()
    A = anims(kit)
    mship.quick_review(ASSET, kit.rig, [body], [(spear, "weapon_r")], A, out, HEIGHT, GN.OUTLINE_CLASS,
                       blob_radius=0.24, px=GN.OUTLINE_PX, pose=A.ready().but(jaw=jaw) if jaw else None)


def anim_quick(out: Path, names, facing: str = "se") -> None:
    import mship
    GN.march_stage()
    pal, kit, body, spear = build_all()
    A = anims(kit)
    R.attach_to_bone(spear, kit.rig, "weapon_r")
    A.bind_meshes(body, {"weapon_r": spear})
    cs = [c for c in clips(A) if not names or c.name in names]
    mship.anim_review(kit.rig, [body], [(spear, "weapon_r")], cs, TOKEN, out, HEIGHT, GN.OUTLINE_CLASS,
                      blob_radius=0.24, facing=facing, px=GN.OUTLINE_PX)


# cloth × limb gate: the loin scrap against the legs and knees; the arm rag's tails against the forearm
QA_GROUPS = {"loin": ["loin_f"], "legs": ["leg", "leg_grime", "knee"]}
QA_PAIRS = {"loinLeg": ("loin", "legs")}


def make_ship():
    """The ``mship.MonsterShip`` of the gravegnawer (built, clips authored; nothing exported)."""
    import mship
    GN.march_stage()
    pal, kit, body, spear = build_all()
    body1 = build_body(pal, kit, lod=1)
    A = anims(kit)
    R.attach_to_bone(spear, kit.rig, "weapon_r")
    A.bind_meshes(body, {"weapon_r": spear})
    cs = clips(A)
    items = [mship.Item(spear, SPEAR, "weapon_r", "spear",
                        sockets=[{"name": "tip", "pos": (0.0, HEAD_TIP, 0.0)}, {"name": "mid", "pos": (0.0, 0.2, 0.0)}],
                        extra={"lengthCm": round((HEAD_TIP - STICK_BACK + 0.012) * 100.0, 1),
                               "note": "monsters.md 2.4: a broken crusade spearhead lashed to a stick (the goblin "
                                       "spear shortened; no bone weapon)"}, grip=spear_grip)]
    ms = mship.MonsterShip(
        asset=ASSET, token=TOKEN, preview_dir="mon_gravegnawer", pal=pal, A=A, body=body, items=items, clips=cs,
        outline_class=GN.OUTLINE_CLASS, game_ids=[GAME_ID], blob_radius=0.26, height=HEIGHT, lod1=body1,
        display={"zh-CN": "啃冢者", "en": "Gravegnawer"},
        fx={"telegraph": {**GN.TELEGRAPH, "untilMs": round(A.windup_frac * CONTACT_MS, 1)},
            "attackTrail": {"color": GN.TRAIL, "alpha": 0.45, "sockets": ["tip", "mid"], "item": SPEAR}},
        qa_groups={"body_nf": mship.parts_except(body, GRIP_PARTS), **QA_GROUPS},
        qa_pairs={"spearBody": ("item:" + SPEAR, "body_nf"), **QA_PAIRS},
        qa_limits={"ground": -0.5, "spearBody": 0, **{k: 0 for k in QA_PAIRS}},
        outline_px=GN.OUTLINE_PX, outline_px_note=GN.OUTLINE_PX_NOTE,
        game_keys=[("Idle", 0, "se"), ("Walk", 9, "se"), ("Attack01", 9, "se"), ("Attack01", 18, "se"),
                   ("Attack01", 18, "sw"), ("Hurt", 0, "se"), ("Death", 36, "se"), ("Stun", 10, "se")],
        contact_keys=[("Attack01", 18, f) for f in ("se", "sw", "ne", "nw")],
        close_keys=[("Attack01", 9, "se"), ("Attack01", 14, "se"), ("Attack01", 18, "se"), ("Attack01", 18, "side"),
                    ("Hurt", 0, "se"), ("Death", 12, "se"), ("Death", 24, "se"), ("Death", 36, "se"),
                    ("Stun", 10, "front"), ("Walk", 9, "side")],
        skeleton_bones_required=("root", "pelvis", "head", "jaw", "weapon_r", "weapon_l", "ear_l_02", "loin_f_02",
                                 "pelt_03", "headdress_04", "totem_feather_02", "ik_foot_l", "ik_hand_gun"),
        lod_note="LOD0 budget 6.1k / LOD1 3.2k toon tris (monsters.md 5: gravegnawer = the goblin's budget).",
        extra={**GN.remake_extra("thrust_bite", CONTACT_MS, REF_SPEED,
                                 "Clips in place; the core moves the actor (play rate = v / ref). Attack01 is a lunging "
                                 "stab and bite (the jaw snaps shut on Contact). Death collapses face down (W2: the "
                                 "carcass the other gnawers ignore). Stun (C5) loops."),
               **GN.marker_extra(MARKER)},
    )
    return ms


def ship(previews: bool = True, verify: bool = True) -> int:
    import mship
    res = mship.ship(make_ship(), previews=previews, verify=verify)
    return mship.main_result(res)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    scratch = scratch_root("a4", "gravegnawer")
    if "--quick" in args:
        quick(scratch, float(next((a.split("=")[1] for a in args if a.startswith("--jaw=")), 0.0)))
    elif "--anims" in args:
        names = [a for a in args[args.index("--anims") + 1:] if not a.startswith("--")]
        fac = next((a.split("=")[1] for a in args if a.startswith("--facing=")), "se")
        anim_quick(scratch / "anims", names, fac)
    else:
        sys.exit(ship(previews="--no-previews" not in args, verify="--no-verify" not in args))

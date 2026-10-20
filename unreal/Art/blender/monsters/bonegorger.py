"""饕骸者 Bonegorger + the hunt 饿钟 Hungerbell — ``SK_Mon_Bonegorger`` (dark remake A4; monsters.md §2.4 bruiser, §5).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/bonegorger.py
        [--quick [hunt]] [--anims Clip … --facing=se] [--no-previews] [--no-verify]

The goblin chief's bruiser slot re-skinned through ``gnawers.py`` (monsters.md §5: "re-skin of goblin_chief.py"; the
chief's ``SKEL_Goblin``, bulk and overhead club, none of its gear): a **bloated gnawer** — a small low skull on a
thick neck, an **oversized underbite jaw** whose lower tusks rise past the blunt snout, bone-yellow eyeshine (the
family marker), torn ear stubs, a hunched hump of shoulders over a **sagging belly** (``bonegorger.belly``), mangy
dark tufts (``bonegorger.tuft``) on the shoulders, back and flanks, long thick arms ending in clawed fists, short
thick legs, a tabard rag knotted round the left arm. **No apron, no cleaver, no hooks, no larder.**

Weapon: ``SM_MonWeapon_BellClapper`` on ``weapon_r`` — the clapper of a church bell used as a club (bronze
``clapper.bronze`` #8A6A3A, grime, verdigris): the hanging eye at the butt, a grease-black shank (``clapper.bronze_dark``,
two worn bright rings) wound with a tabard rag and rope where the fist holds it, a heavy bronze ball and its short
flight. At READY its flight rests on the ground beside the right foot (it drags it). Every protruding detail inks on
its own hull (mange spikes, claws, the rag end, the bell lip's bead, each name-tag).

The hunt ``hunt_hungerbell`` 饿钟 Hungerbell (quest M2; manifest ``variants``): this mesh ×1.25 + the elite marker +
``SM_Mon_Bonegorger_Att_BellLip`` (a jagged lip of Ashwheat's bell still wedged on the clapper's ball, socket
``weapon_r``) + ``SM_Mon_Bonegorger_Att_TagNecklace`` (a cord of bronze name-tags cut off the dead, ``neck_01``).

Clips (speed 42 → 1.26 m/s): Idle 1400 (wet breathing: the belly heaves, the jaw hangs and works), Walk 860 (a heavy
lumber, no slide), Attack01 1200 (the long wind-up the brief asks for: the clapper hauled up over the right shoulder
and clubbed down-forward; WindupEnd 434, Contact 700), Attack02 1400 (钟锤 Clapper Slam [W2]: reared up on its hind
legs, the clapper high overhead, the whole body slammed down — the ball strikes the ground on Contact 800, WindupEnd
496, ``FX_BellRing`` 800: the bell note), Hurt 260, Death 900 (pitches onto its belly, rolls onto its side), Stun 700.
Game ids ``monster_bonegorger``, ``hunt_hungerbell``.
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
import bmesh  # noqa: E402
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

ASSET = "SK_Mon_Bonegorger"
TOKEN = "Mon_Bonegorger"
CLAPPER = "SM_MonWeapon_BellClapper"
BELL_LIP = "SM_Mon_Bonegorger_Att_BellLip"
TAGS = "SM_Mon_Bonegorger_Att_TagNecklace"
TAGS_BONE = "neck_01"           # the necklace rides the neck
NECK_AT = 0.22                 # where along the neck (0 root → 1 skull) the cord sits (higher, the skull hangs over it)
NECK_R = 3.4                   # u: the cord ring radius (the neck there ≈ 3.1 u)
TAG_ANGLES = (-38.0, -13.0, 12.0, 37.0)   # round the cord from the throat (0); the sides are bare (the arms)
PREFIX = "bonegorger"
GAME_ID = "monster_bonegorger"
HUNT_ID = "hunt_hungerbell"
HUNT_SCALE = 1.25
U = 0.0252
WEB = GB.Web(U=U, thigh=9.4, shin=8.8, upper_arm=9.0 * 1.15, fore_arm=8.5 * 1.15, torso=15.0, neck=6.0, ankle=1.9,
             hip=3.0, shoulder=6.0, lean=0.82, head_tilt=0.25, root=(46.5, 72.5), bulk=1.32, head_k=0.86,
             foot_len=9.6, grin=0.0)
# head units below: an oversized underbite jaw (wider, deeper and longer than the blunt snout; the chin juts past the
# nose), lower tusks rising past the upper lip, few and short upper needles
SHAPE = GN.Shape(
    muzzle=((1.9, 2.4, 3.3, 2.6), (1.4, 5.0, 3.0, 2.3), (0.8, 7.2, 2.5, 1.85), (0.15, 9.0, 1.95, 1.4),
            (-0.3, 10.0, 1.35, 1.0)),
    jaw=((-1.3, -0.6, 3.5, 1.9), (-2.8, 3.0, 3.8, 2.15), (-3.4, 6.8, 3.7, 2.0), (-3.0, 10.0, 3.1, 1.7),
         (-1.9, 12.2, 2.2, 1.25)),
    jaw_hinge=(-1.2, -0.4), jaw_tip=(-1.8, 12.4),
    teeth_up=(3.6, 5.3, 6.9, 8.3, 9.3), teeth_dn=(5.0, 7.4, 9.6, 11.2),
    tooth_len=1.2, tooth_r=0.34, tooth_len_dn=2.3, tooth_r_dn=0.5, teeth_dn_k=(0.6, 0.75, 1.05, 1.5),
    eye=(2.8, 3.6, 2.65), eye_r=(1.95, 1.4, 0.82), ear=(2.2, 2.6),
    head_tilt=0.12, head_drop=-1.4, arm_down=60.0, ribs=0, spine_knobs=0, tufts=9, belly_k=1.3,
    neck_k=1.5, fist_k=1.3)
LOOK = GB.Look()                 # unused colours (the march swatches come from gnawers.BONEGORGER_ALIAS)
HEIGHT = 1.08                    # framing height of the hunched READY (the hump of the shoulders ≈ 0.95 m)
REF_SPEED = 42 * 0.03            # monsters.md §2.0: speed 42 (tiles/s = speed × 0.03)
WALK = dict(cycle=860.0, duty=0.42, lift=0.06, bob=0.026, lean=3.0, arm=0.05, crouch=-0.03, twist=7.0,
            hand_swing=0.25, hand_off=V(-0.01, 0.0, 0.0))
CONTACT_MS, ATTACK_MS = 700.0, 1200.0          # monsters.md: "Contact ≈ 700 ms (the long wind-up reads)"
SLAM_CONTACT_MS, SLAM_MS = 800.0, 1400.0       # [W2] Clapper Slam: "windup 800 ms"
IDLE_MS = 1400.0
MARKER = ("bonegorger.eye",)

LOD0_BUDGET, LOD1_BUDGET = 9000, 3300          # monsters.md §5 (the chief's budget)
LOD1_RATIO = 0.5
LOD1_HULL = 0.75
LOD1_SKIP = ("spine_knob", "knuckle", "thumb", "tooth", "nose", "blade", "arm_rag_tail", "tuft", "belly_fold")

ALIAS = dict(GN.BONEGORGER_ALIAS, fold="bonegorger.hide_dark")
CLAPPER_ALIAS = dict(bronze="clapper.bronze", bronze_dark="clapper.bronze_dark", patina="clapper.patina",
                     cloth="gnawer.trophy", rope="gnawer.rope", rope_line="gnawer.stick")
ATT_ALIAS = dict(bronze="clapper.bronze", bronze_dark="clapper.bronze_dark", patina="clapper.patina",
                 tags="hungerbell.tags", cord="gnawer.rope")
# the clapper, along its local +Y from the grip (m)
EYE_Y = -0.14                  # centre of the hanging eye at the butt (short: a long butt stabbed the belly)
SHANK_Y0, SHANK_Y1 = -0.105, 0.36
BALL_Y, BALL_R = 0.47, 0.072   # the ball's centre and radius
TIP_Y = 0.625                  # end of the flight


# ── body ───────────────────────────────────────────────────────────────────────────────────────────────
class BonegorgerKit(GN.GnawerKit):
    """The gnawer kit bloated: a deeper belly sag, fat folds under it and the mangy tufts."""

    def add_belly_folds(self, b, tid: int) -> None:
        """Two sagging fold lines under the belly (painted, single-sided) — the gut hangs over the groin."""
        t = self.torso
        u = self.u
        spine = Bind.blend("pelvis", "spine_01", "spine_02", "spine_03", falloff=3.0, smooth=0.02)
        for s0, w, a in ((0.085, 0.62, 44.0), (0.16, 0.5, 36.0)):
            path = [(s0 + 0.03, -a), (s0, -a * 0.45), (s0 - 0.012, 0.0), (s0, a * 0.45), (s0 + 0.03, a)]
            st = G.surface_paint(t, path, u(w), u(0.1), steps=6, taper=0.5)
            b.add(st, "fold", spine, name="belly_fold", outline=False, covered_by=tid)


# (spine s, angle: 0 front, ±180 the back): the hump, the shoulders' backs and the spine — not the neck root (the
# Hungerbell's tag cord lies there)
MANGE = [(0.91, 122.0), (0.91, -126.0), (0.86, 152.0), (0.84, -160.0), (0.77, 178.0), (0.71, 140.0),
         (0.67, -150.0), (0.56, 165.0), (0.89, 96.0), (0.88, -98.0)]


def add_mange(kit: "BonegorgerKit", b, tid: int) -> None:
    """Mangy tufts that break the silhouette (monsters.md: "a bloated gnawer with mangy tufts"): ragged clumps of
    3 short coarse spikes standing off the hump, the shoulders and the back, raked back toward the tail; inked on
    their own hulls."""
    t = kit.torso
    u = kit.u
    spine = Bind.blend("pelvis", "spine_01", "spine_02", "spine_03", falloff=3.0, smooth=0.02)
    rng = scene.rng(ASSET, "mange")
    back = None
    for k, (s_, a) in enumerate(MANGE):
        n = t.normal(s_, a)
        p_lo, p_hi = t.point(s_ - 0.03, a), t.point(s_ + 0.03, a)
        down_spine = (p_lo - p_hi).normalized()
        for j in range(3):
            da = (j - 1) * 7.0 + rng.uniform(-2.5, 2.5)
            base = t.point(s_ + rng.uniform(-0.015, 0.015), a + da, -u(0.25))
            ln = u(rng.uniform(1.6, 2.5))
            d = (n * 0.75 + down_spine * 0.66 + Vector((rng.uniform(-.15, .15),) * 3)).normalized()
            b.add(G.cone(base, base + d * ln, u(0.42), sides=4), "tuft", spine, name="mange")
    del back, tid


def build_body(pal: Palette, kit: BonegorgerKit, lod: int = 0, ratio: float = LOD1_RATIO) -> bpy.types.Object:
    from kit import asset
    b = GN.march_builder(ASSET, pal, ALIAS, PREFIX, lod, LOD1_SKIP, ratio, hull_ratio=LOD1_HULL if lod else 1.0)
    kit.add_head(b)
    ids = kit.add_torso(b, trophies=False, belt=False)
    kit.add_tufts(b, ids["torso"])
    add_mange(kit, b, ids["torso"])
    kit.add_belly_folds(b, ids["torso"])
    kit.add_arm_rag(b, "l")
    for s, sg in (("l", 1), ("r", -1)):
        kit.add_arm(b, s, sg, r_scale=1.18)
        kit.add_leg(b, s, sg, r_scale=1.3, top_up=1.2, top_r=2.6)
    body = asset.finish_mesh(b, pal, GN.OUTLINE_CLASS, rig=kit.rig, grounding_height=HEIGHT,
                             name=f"{ASSET}_LOD{lod}" if lod else None)
    body["af_lod"] = lod
    return body


# ── the clapper ────────────────────────────────────────────────────────────────────────────────────────
def _finish_item(b, pal, name: str) -> bpy.types.Object:
    obj = b.build(name)
    mt = shading.toon_material(pal)
    outline.bake_hull(obj, outline.WIDTH_CM[GN.OUTLINE_CLASS] / 100.0, mt, shading.outline_material(pal))
    for m in outline.hull_color_report(obj, pal, GN.OUTLINE_CLASS):
        print(f"[ink] WARNING {name}: {m}")
    obj["af_outline_class"] = GN.OUTLINE_CLASS
    return obj


def _along_y(part: M.Part, y0: float = 0.0) -> M.Part:
    """A part modelled along +Z (``lathe`` / ``loft_z``) laid along the item's +Y from ``y0``."""
    return part.transform(Matrix.Translation((0.0, y0, 0.0)) @ Matrix.Rotation(math.radians(-90.0), 4, "X"))


def build_clapper(pal: Palette) -> bpy.types.Object:
    """The bell clapper club (≈ 82 cm): the hanging eye, the shank (grip at the origin), the ball and its flight.
    Tip along +Y, the swing's leading face +Z."""
    b = GN.MarchBuilder(CLAPPER, pal, CLAPPER_ALIAS, prefix="bonegorger_clapper")
    b.declare()
    rig = Bind.rigid("root")
    # the eye: a thick bronze loop at the butt (the clapper hung from the bell's crown staple)
    loop = [Vector((0.0, EYE_Y + 0.032 * math.cos(a), 0.034 * math.sin(a)))
            for a in np.linspace(0.0, 2 * math.pi, 13)[:-1]]
    b.add(M.sweep(loop, M.superellipse(1, 1, 7, 2.0), scales=[0.0105] * len(loop), closed_path=True,
                  up=(1, 0, 0)), "bronze_dark", rig, name="eye", hull_pad=True)
    # the shank: thickening toward the ball, a little bent; grime band under the ball
    ys = np.linspace(SHANK_Y0, SHANK_Y1, 8)
    pts = [Vector((0.002 * math.sin(y * 14.0), y, 0.0025 * math.sin(y * 9.0 + 0.5))) for y in ys]
    rad = [0.0145 + 0.008 * ((y - SHANK_Y0) / (SHANK_Y1 - SHANK_Y0)) ** 2 for y in ys]
    # the shank black with grime and old grease (the ball's bronze reads as the club's head against it)
    sid = b.add(M.tube(pts, rad, sides=9, up=(0, 0, 1), dome0=0.004, dome1=0.0), "bronze_dark", rig, name="shank",
                hull_pad=True)
    for y0, y1 in ((0.17, 0.19), (0.27, 0.30)):          # worn bright rings where it rubbed the bell
        r0 = 0.0145 + 0.008 * ((y0 - SHANK_Y0) / (SHANK_Y1 - SHANK_Y0)) ** 2 + 0.0008
        r1 = 0.0145 + 0.008 * ((y1 - SHANK_Y0) / (SHANK_Y1 - SHANK_Y0)) ** 2 + 0.0008
        b.add(M.tube([(0, y0, 0), (0, y1, 0)], [r0, r1], sides=9, dome0=0.0, dome1=0.0), "bronze", rig,
              name="wear", outline=False, covered_by=sid)
    # the ball and its flight (a lathe along the shank): neck → pear-shaped ball → short tapering flight
    prof = [(0.021, SHANK_Y1 - 0.01), (0.03, 0.39), (0.052, 0.408), (0.067, 0.43), (BALL_R, BALL_Y - 0.01),
            (BALL_R * 0.97, BALL_Y + 0.022), (0.058, 0.512), (0.036, 0.532), (0.028, 0.55), (0.025, 0.59),
            (0.017, TIP_Y - 0.006)]
    ball = M.lathe([(r, y) for r, y in prof], sides=14, cap0="fan", cap1="fan")
    bid = b.add(_along_y(ball), "bronze", rig, name="ball", hull_pad=True)
    # verdigris and grime on the ball (flattened dabs lying on it, 0.6 mm proud)
    rng = scene.rng(CLAPPER, "patina")
    for k in range(9):
        a = (k * 137.5 + rng.uniform(-15, 15)) % 360.0
        y = BALL_Y + rng.uniform(-0.04, 0.05)
        r_here = float(np.interp(y, [p[1] for p in prof], [p[0] for p in prof]))
        n = Vector((math.cos(math.radians(a)), 0.0, math.sin(math.radians(a))))
        c = Vector((0.0, y, 0.0)) + n * (r_here - 0.0005)
        dab = M.ellipsoid((rng.uniform(0.011, 0.02), rng.uniform(0.014, 0.024), 0.0018), (0, 0, 0), sides=7, rings=3)
        dab.transform(G.frame_z(c, n, Vector((0, 1, 0))))
        b.add(dab.flat(), "patina" if k % 3 else "bronze_dark", rig, name="patina", outline=False, covered_by=bid)
    # the grip: a tabard rag wound round the shank under the fist, a rope binding over it, one rag end hanging
    gid = b.add(M.tube([(0, -0.085, 0), (0, 0.065, 0)], [0.0215, 0.0215], sides=10, dome0=0.0, dome1=0.0), "cloth",
                rig, name="grip_rag", hull_pad=True)
    for k in range(5):
        y = -0.078 + k * 0.034
        b.add(M.tube([(0, y - 0.004, 0), (0, y + 0.004, 0)], [0.0232, 0.0232], sides=10, dome0=0.0, dome1=0.0),
              "rope", rig, name="grip_rope", outline=False, covered_by=gid)
    p0 = Vector((0.012, 0.055, -0.014))
    tail = G.chain_strip([p0, p0 + Vector((0.01, -0.01, -0.03)), p0 + Vector((0.016, -0.026, -0.07))],
                         [0.02, 0.017, 0.01], Vector((0, 1, 0)), Vector((1, 0, 0)), 0.003, tails=2, seed=57, cols=3,
                         per_seg=1)
    b.add(tail, "cloth", rig, name="grip_rag_tail", sub_regions={1: "cloth", 2: "cloth"}, hull_pad=True)
    return _finish_item(b, pal, CLAPPER)


def clapper_grip(co) -> bool:
    return -0.095 < co.y < 0.075


# ── Hungerbell attachments ─────────────────────────────────────────────────────────────────────────────
def build_bell_lip(pal: Palette) -> bpy.types.Object:
    """Hungerbell's clapper still carries a jagged lip of Ashwheat's bell (monsters.md §2.4): a curved shard of the
    bell's sound-bow (≈ 210° of a 20 cm bell mouth) wedged on the ball, its rim bead thick, its broken edge jagged.
    Item-local (``weapon_r`` space = the clapper's space)."""
    b = GN.MarchBuilder(BELL_LIP, pal, ATT_ALIAS, prefix="hungerbell_lip")
    b.declare()
    rig = Bind.rigid("root")
    rng = scene.rng(BELL_LIP, "jag")
    R0, th = BALL_R + 0.008, 0.009
    a0, a1, cols = -105.0, 105.0, 15
    y_rim = BALL_Y - 0.05
    bm = bmesh.new()
    outer, inner = [], []
    for i in range(cols + 1):
        a = math.radians(a0 + (a1 - a0) * i / cols)
        # broken top edge: a saw of uneven teeth; the ends taper (the crack lines)
        end = min(i, cols - i) / (cols / 2)
        h = (0.045 + 0.065 * end ** 0.6) * (0.6 + 0.4 * (i % 2)) + rng.uniform(-0.008, 0.008)
        flare = 0.028                                   # the bell mouth flares out toward the rim
        rows = []
        for t in (0.0, 0.5, 1.0):
            y = y_rim + h * t
            r = R0 + flare * (1.0 - t)
            rows.append((r, y))
        ring_o, ring_i = [], []
        for r, y in rows:
            n = Vector((math.sin(a), 0.0, math.cos(a)))
            ring_o.append(bm.verts.new(Vector((0.0, y, 0.0)) + n * (r + th * 0.5)))
            ring_i.append(bm.verts.new(Vector((0.0, y, 0.0)) + n * (r - th * 0.5)))
        outer.append(ring_o)
        inner.append(ring_i)
    for i in range(cols):
        for j in range(2):
            bm.faces.new((outer[i][j], outer[i + 1][j], outer[i + 1][j + 1], outer[i][j + 1]))
            bm.faces.new((inner[i][j + 1], inner[i + 1][j + 1], inner[i + 1][j], inner[i][j]))
        bm.faces.new((outer[i][0], inner[i][0], inner[i + 1][0], outer[i + 1][0]))      # rim
        bm.faces.new((outer[i + 1][2], inner[i + 1][2], inner[i][2], outer[i][2]))      # broken edge
    for i, sg in ((0, 1), (cols, -1)):                                                # the crack ends
        for j in range(2):
            f = (outer[i][j], outer[i][j + 1], inner[i][j + 1], inner[i][j])
            bm.faces.new(f if sg > 0 else tuple(reversed(f)))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    shard = M.Part(bm).flat()
    sid = b.add(shard, "bronze", rig, name="lip", hull_pad=True)
    # the sound-bow bead along the rim (thick, grimy) and verdigris runs down the outside
    arc = [Vector((0.0, y_rim + 0.004, 0.0)) + Vector((math.sin(math.radians(a)), 0, math.cos(math.radians(a))))
           * (R0 + 0.028 + th * 0.2) for a in np.linspace(a0 + 4, a1 - 4, 13)]
    b.add(M.tube(arc, [0.0085] * len(arc), sides=6, dome0=0.003, dome1=0.003), "bronze_dark", rig, name="lip_bead",
          hull_pad=True)
    for a in (-80.0, -48.0, -12.0, 22.0, 55.0, 86.0):
        n = Vector((math.sin(math.radians(a)), 0.0, math.cos(math.radians(a))))
        c = Vector((0.0, y_rim + 0.035, 0.0)) + n * (R0 + 0.014 + th * 0.5)
        run = M.ellipsoid((0.007, 0.018, 0.0012), (0, 0, 0), sides=6, rings=3)
        run.transform(G.frame_z(c, n, Vector((0, 1, 0))))
        b.add(run.flat(), "patina", rig, name="lip_patina", outline=False, covered_by=sid)
    return _finish_item(b, pal, BELL_LIP)


def build_tag_necklace(pal: Palette, kit: BonegorgerKit) -> bpy.types.Object:
    """A cord of bronze name-tags cut off the dead (monsters.md §2.4) slung round the root of the thick neck, sagging
    in front, the tags hanging off its front half over the chest. Built in armature space, shipped
    ``neck_01``-local."""
    b = GN.MarchBuilder(TAGS, pal, ATT_ALIAS, prefix="hungerbell_tags")
    b.declare()
    rig = Bind.rigid(TAGS_BONE)
    J = kit.J
    u = kit.u
    nk = kit.shape.neck_k
    n0 = J["neck"] + Vector((0, 0.004, -0.02))
    n1 = kit.hp(-1.6, -2.4)
    ax = (n1 - n0).normalized()                       # up the neck (forward and up on the hunched beast)
    fwd = Vector((0.0, -1.0, 0.0))
    side = ax.cross(fwd).normalized()
    front = side.cross(ax).normalized()               # ⟂ the neck, toward the chest / ground
    if front.dot(fwd) < 0:
        front = -front
    c = n0.lerp(n1, NECK_AT)
    rr = u(NECK_R)                                    # the cord hugs the neck root (off the shoulders and arms)
    pts, outs = [], []
    for a in np.arange(0.0, 360.0, 20.0):
        ar = math.radians(a)
        o = front * math.cos(ar) + side * math.sin(ar)
        sag = u(1.6) * max(0.0, math.cos(ar)) ** 2    # the cord sags in front
        pts.append(c + o * (rr + sag * 0.6) - ax * sag)
        outs.append(o)
    b.add(M.sweep(pts, M.superellipse(1, 1, 6, 2.0), scales=[u(0.4)] * len(pts), closed_path=True,
                  up=tuple(ax)), "cord", rig, name="tag_cord", hull_pad=True)
    rng = scene.rng(TAGS, "tags")
    for k, a in enumerate(TAG_ANGLES):
        # threaded on the cord like scales: each tag lies flat against the neck, its long side along the cord (a
        # dangling tag swung through the arms reaching forward / over the head)
        ar = math.radians(a + rng.uniform(-4, 4))
        o = front * math.cos(ar) + side * math.sin(ar)
        tang = (-front * math.sin(ar) + side * math.cos(ar)).normalized()
        sag = u(1.6) * max(0.0, math.cos(ar)) ** 2
        p0 = c + o * (rr + sag * 0.6) - ax * sag
        w, h, d = 0.022 * rng.uniform(0.9, 1.1), 0.032 * rng.uniform(0.85, 1.15), 0.0035
        tag = M.box((w, h, d), (0, 0, 0))             # x along the neck, y along the cord, z its thickness (out)
        m = G.frame_z(p0 + o * (u(0.25) + d * 0.5) - ax * u(0.15), o, tang)
        tag.transform(m @ Matrix.Rotation(math.radians(rng.uniform(-12, 12)), 4, "Z"))
        b.add(tag, "tags", rig, name="tag", hull_pad=True)
        b.add(M.ellipsoid((0.0055, 0.0055, 0.003), p0 + o * (u(0.25) + d + 0.001), sides=6, rings=3), "bronze_dark",
              rig, name="tag_ring", outline=False)
    obj = b.build(TAGS)
    inv = kit.rig.obj.data.bones[TAGS_BONE].matrix_local.inverted()
    obj.data.transform(inv)
    obj.data.update()
    mt = shading.toon_material(pal)
    outline.bake_hull(obj, outline.WIDTH_CM[GN.OUTLINE_CLASS] / 100.0, mt, shading.outline_material(pal))
    for msg in outline.hull_color_report(obj, pal, GN.OUTLINE_CLASS):
        print(f"[ink] WARNING {TAGS}: {msg}")
    obj["af_outline_class"] = GN.OUTLINE_CLASS
    return obj


VARIANTS = {
    HUNT_ID: {
        "displayName": {"zh-CN": "饿钟", "en": "Hungerbell"}, "scale": HUNT_SCALE, "elite": True,
        "attachments": [{"object": BELL_LIP, "socket": "weapon_r"}, {"object": TAGS, "socket": TAGS_BONE}],
        "marker": {"swatches": list(MARKER)},
        "note": "quest M2 hunt (monsters.md 2.4): SK_Mon_Bonegorger x1.25 + a jagged lip of Ashwheat's bell still "
                "wedged on the clapper's ball + a cord of bronze name-tags + the elite marker; minions 3 "
                "gravegnawer; [W2] every slam calls every gnawer within 10 tiles"},
}


def build_all(hunt: bool = True):
    scene.reset()
    pal = Palette(GN.FAMILY)
    kit = BonegorgerKit(ASSET, PREFIX, WEB, LOOK, SHAPE)
    body = build_body(pal, kit)
    clapper = build_clapper(pal)
    atts = (build_bell_lip(pal), build_tag_necklace(pal, kit)) if hunt else ()
    return pal, kit, body, clapper, atts


# ── poses ──────────────────────────────────────────────────────────────────────────────────────────────
WEB_READY = dict(root=(46.5, 72.5), footN=54.5, footF=40.0, lean=0.82, head=0.25)
READY_LEAN = 10.0
# the clapper hangs from the right fist, its ball resting on the ground beside the right foot (dragged)
READY_HAND_R = (-12.5, 8.0, 19.5)
READY_CLAPPER = (138.0, -0.3)       # C.blade(theta: deg from vertical, + forward; lat: − = out to the right)
SLAM_MID_TH = -5.0
DEATH_FEET = ((-3.5, 6.0, -2.0), (3.0, 7.5, 0.5))   # u, the unrolled body frame: legs drawn in (splayed, the
#                                                    upper one stuck up in the air once rolled)
DEATH_ROLL_CLAPPER = 90.0                           # grip roll of the flung clapper (wrist_r[1])
# clapper keys (hand u: lat, fwd, up; theta deg from vertical, + forward; lat − = out to the right): the raised
# clapper swings up and over OUTSIDE the right shoulder (crossing the body, it went through the torso and skull)
CLUB = {"wind": ((-16.0, -1.0, 34.0), -25.0, -0.65), "mid": ((-15.0, 3.0, 41.0), 8.0, -0.5),
        "hit": ((-10.5, 22.0, 16.0), 116.0, 0.0)}
SLAM = {"wind": ((-16.0, -2.0, 41.0), -20.0, -0.7), "mid": ((-16.0, -3.0, 45.0), SLAM_MID_TH, -0.7),
        "hit": ((-13.5, 21.0, 15.1), 121.0, -0.3)}          # the ball on the ground ≈ 0.95 m ahead on Contact
RECOVER = ((-12.0, 13.0, 19.0), 128.0, -0.25)
STUN_HAND = (-13.5, 8.0, 21.5)
READY_HAND_L = (11.5, 10.0, 13.0)


def u3(A, lat: float, fwd: float, up: float) -> Vector:
    return V(lat * A.U, fwd * A.U, up * A.U)


def ready(A) -> BP:
    """Hunched over its gut on short spread legs, the head low and forward on the thick neck, the clapper hanging
    from the right fist with its ball on the ground, the left fist hanging by the knee."""
    wd, wu = C.blade(*READY_CLAPPER)
    return BP(
        pelvis=A.web_pelvis(*WEB_READY["root"]), lean=READY_LEAN, hip_yaw=-6.0, twist=6.0,
        head_pitch=-6.0 - READY_LEAN,
        foot_r=A.web_foot(WEB_READY["footN"], -0.12), foot_l=A.web_foot(WEB_READY["footF"], 0.125),
        foot_r_rot=(0.0, 0.0, 16.0), foot_l_rot=(0.0, 0.0, 24.0), knee_r=V(-0.7, 1, 0), knee_l=V(0.7, 1, 0),
        hand_r=u3(A, *READY_HAND_R), elbow_r=V(-0.9, 0.0, -0.3), wpn=wd, wpn_up=wu,
        hand_l=u3(A, *READY_HAND_L), elbow_l=V(0.9, 0.0, -0.3), wrist_l=(-25.0, 0.0, 0.0),
        flow=0.1)


def install_poses(A) -> None:
    rx, ry = WEB_READY["root"]
    fn, ff = WEB_READY["footN"], WEB_READY["footF"]

    def lean_head(lean: float, muzzle: float) -> dict:
        """Body lean (deg) and the head pitch that keeps the muzzle at ``muzzle`` deg below level (READY: −6)."""
        return {"lean": lean, "head_pitch": muzzle - lean}

    def club_keys():
        """Attack01: the clapper hauled up over the right shoulder (wind: the gut swings back, the jaw gapes), held
        high (mid) and clubbed down-forward across the body onto the target (contact at ≈ waist height)."""
        R0 = A.ready()
        wind = R0.but(pelvis=A.web_pelvis(rx - 2.0, ry + 0.6), **lean_head(-8.0, -24.0), twist=R0.twist - 22.0,
                      hip_yaw=-10.0, hand_r=u3(A, *CLUB["wind"][0]), elbow_r=V(-0.9, -0.2, -0.3),
                      **A._wkw(R0, *CLUB["wind"][1:]), hand_l=u3(A, 13.5, 11.0, 19.0), elbow_l=V(0.9, 0.3, -0.3),
                      clav_r=(16.0, -6.0), fx=0.4, jaw=24.0)
        mid = R0.but(pelvis=A.web_pelvis(rx - 1.0, ry + 0.2), **lean_head(-4.0, -20.0), twist=R0.twist - 12.0,
                     hand_r=u3(A, *CLUB["mid"][0]), elbow_r=V(-0.9, -0.1, -0.4), **A._wkw(R0, *CLUB["mid"][1:]),
                     hand_l=u3(A, 13.5, 10.0, 20.0), clav_r=(20.0, 0.0), fx=0.7, jaw=30.0)
        hit = R0.but(pelvis=A.web_pelvis(rx + 4.0, ry + 2.2), **lean_head(28.0, 6.0), twist=R0.twist + 18.0,
                     hip_yaw=8.0, hand_r=u3(A, *CLUB["hit"][0]), elbow_r=V(-0.7, 0.2, -0.6),
                     **A._wkw(R0, *CLUB["hit"][1:]), hand_l=u3(A, 14.5, 5.0, 17.0), elbow_l=V(0.9, 0.2, -0.4),
                     foot_r=A.web_foot(fn + 5.0, R0.foot_r.x), foot_r_rot=(0, 0, 6), clav_r=(-4.0, 10.0), fx=1.0,
                     jaw=34.0, flow=0.3)
        return [A.look(wind, 0.6), A.look(mid, 0.6), A.look(hit, 0.6)]

    def slam_keys():
        """Attack02 钟锤 Clapper Slam: rears up on its hind legs with the clapper high over its head (the free arm up
        for balance), then the whole bloated body comes down behind it — the ball strikes the ground ahead on the
        contact and the free fist hits the ground beside it."""
        R0 = A.ready()
        wind = R0.but(pelvis=A.web_pelvis(rx - 2.5, ry - 1.5), **lean_head(-18.0, -26.0), twist=R0.twist - 8.0,
                      hand_r=u3(A, *SLAM["wind"][0]), elbow_r=V(-0.9, 0.1, -0.2), **A._wkw(R0, *SLAM["wind"][1:]),
                      hand_l=u3(A, 10.0, 4.0, 36.0), elbow_l=V(0.9, 0.1, -0.2), clav_l=(18.0, 0.0),
                      clav_r=(22.0, 0.0), fx=0.4, jaw=30.0)
        mid = R0.but(pelvis=A.web_pelvis(rx - 2.5, ry - 2.5), **lean_head(-24.0, -32.0), twist=R0.twist - 4.0,
                     foot_l=R0.foot_l + u3(A, 0.0, 1.0, 0.0),
                     hand_r=u3(A, *SLAM["mid"][0]), elbow_r=V(-0.9, 0.1, -0.2), **A._wkw(R0, *SLAM["mid"][1:]),
                     hand_l=u3(A, 10.5, 2.0, 40.0), clav_l=(22.0, 0.0), clav_r=(24.0, 0.0), fx=0.7, jaw=36.0)
        hit = R0.but(pelvis=A.web_pelvis(rx + 5.0, ry + 4.5), **lean_head(40.0, 14.0), twist=R0.twist + 6.0,
                     hip_yaw=4.0, hand_r=u3(A, *SLAM["hit"][0]), elbow_r=V(-0.7, 0.2, -0.7),
                     **A._wkw(R0, *SLAM["hit"][1:]), hand_l=u3(A, 9.5, 18.0, 6.5), elbow_l=V(0.9, 0.2, -0.5),
                     wrist_l=(-30.0, 0.0, 0.0), foot_r=A.web_foot(fn + 6.0, R0.foot_r.x), foot_r_rot=(0, 0, 6),
                     foot_l=A.web_foot(ff + 2.5, R0.foot_l.x), clav_r=(-6.0, 12.0), clav_l=(-6.0, 8.0), fx=1.0,
                     jaw=38.0, flow=0.4)
        return [A.look(wind, 0.5), A.look(mid, 0.5), A.look(hit, 0.5)]

    def follow(hit):
        return hit.but(hand_r=hit.hand_r + u3(A, 0.0, 0.8, 0.2), lean=hit.lean + 2.0, jaw=20.0)

    def recover():
        R0 = A.ready()
        return R0.but(pelvis=R0.pelvis + u3(A, 0.0, 1.5, -0.5), lean=READY_LEAN + 8.0, hand_r=u3(A, *RECOVER[0]),
                      **A._wkw(R0, *RECOVER[1:]), hand_l=R0.hand_l + u3(A, 1.5, -1.0, 2.0), jaw=14.0)

    def recoil():
        R0 = A.ready()
        return R0.but(pelvis=A.web_pelvis(rx - 3.0, ry + 0.6), **lean_head(-14.0, -40.0), head_yaw=10.0,
                      twist=R0.twist - 8.0, hand_r=u3(A, -13.5, 4.0, 24.0), **A._wkw(R0, 128.0, lat=-0.28),
                      hand_l=u3(A, 11.0, 8.0, 21.0), elbow_l=V(0.9, 0.2, -0.3), wrist_l=(0.0, 0.0, 0.0),
                      foot_l=R0.foot_l + u3(A, 0.0, -1.5, 0.0), clav_l=(12.0, 0.0), clav_r=(10.0, 0.0), flow=0.45,
                      jaw=30.0)

    def death_keys():
        """Recoil → the legs give under the gut, the clapper drags → it pitches forward onto the belly, the arms
        thrown out wide → rolled half onto its side, the clapper flung out on the right, the jaw slack (W2: the carcass
        the other gnawers ignore)."""
        R0 = A.ready()
        rec = recoil()
        rl = A.death_roll
        buckle = rec.but(pelvis=A.web_pelvis(rx + 0.5, ry + 5.0), **lean_head(26.0, 22.0), head_roll=8.0 * rl,
                         twist=R0.twist - 4.0, foot_r=R0.foot_r, foot_l=R0.foot_l, knee_l=V(0.8, 1, 0),
                         knee_r=V(-0.8, 1, 0), hand_r=u3(A, -15.0, 13.0, 12.0), elbow_r=V(-0.9, 0.1, -0.3),
                         **A._wkw(R0, 142.0, lat=-0.45), hand_l=u3(A, 15.0, 12.0, 11.0), elbow_l=V(0.9, 0.1, -0.3),
                         wrist_l=(0.0, 0.0, 0.0), clav_l=(-4.0, 0.0), clav_r=(-4.0, 0.0), flow=0.5, jaw=30.0)
        tip = buckle.but(pelvis=A.web_pelvis(rx + 2.0, ry + 9.0), **lean_head(50.0, 30.0), head_roll=16.0 * rl,
                         hand_r=u3(A, -17.0, 17.0, 11.5), **A._wkw(R0, 96.0, lat=-0.7), hand_l=u3(A, 11.0, 17.0, 5.0),
                         spin=rl * 30.0, spin_axis=V(0, 1, 0), flow=0.6, jaw=34.0)
        lying = R0.but(pelvis=A.web_pelvis(rx + 2.5, ry + 10.5), **lean_head(58.0, 30.0), head_roll=24.0 * rl,
                       head_yaw=-12.0 * rl, twist=-6.0, hip_yaw=0.0, hip_pitch=8.0,
                       foot_r=u3(A, *DEATH_FEET[0]), foot_l=u3(A, *DEATH_FEET[1]), foot_r_rot=(30.0, 0, 20),
                       foot_l_rot=(30.0, 0, 20), knee_l=V(0.6, 1, 0.2), knee_r=V(-0.6, 1, 0.2),
                       hand_r=u3(A, -18.0, 16.0, 5.0), elbow_r=V(-0.8, 0.2, -0.6),
                       **A._wkw(R0, 96.0, lat=-0.85), wrist_r=(0.0, DEATH_ROLL_CLAPPER, 0.0), hand_l=u3(A, 8.0, 17.0, 2.0),
                       elbow_l=V(0.8, 0.2, -0.6), wrist_l=(20.0, 0.0, 0.0), clav_l=(-6.0, 0.0), clav_r=(-6.0, 0.0),
                       spin=rl * 72.0, spin_axis=V(0, 1, 0), flow=0.05, jaw=30.0)
        return [rec, buckle, tip, lying]

    A.overrides.update(attack_keys=club_keys, slam_keys=slam_keys, follow=follow, recover=recover, recoil=recoil,
                       death_keys=death_keys)


def idle_layer(A):
    """Wet breathing: the gut heaves (the pelvis and lean on a slow breath), the jaw hangs and works, the head
    swings low (sniffing for the dead)."""
    def extra(bp: BP, t: float) -> BP:
        ph = 2 * math.pi * t / IDLE_MS
        breath = math.sin(ph)
        return bp.but(pelvis=bp.pelvis + V(0, 0, 0.004 * breath), lean=bp.lean + 1.5 * breath,
                      head_yaw=bp.head_yaw + 9.0 * math.sin(ph * 0.5 + 0.4),
                      head_pitch=bp.head_pitch + 3.0 * math.sin(ph + 1.2),
                      jaw=bp.jaw + 8.0 + 6.0 * max(0.0, math.sin(ph * 2 + 0.3)) ** 2,
                      twist=bp.twist + 2.0 * math.sin(ph * 0.5))
    return extra


def anims(kit):
    A = GN.GnawerAnims(kit, REF_SPEED, WALK, ready, style="overhead", contact_wpn=118.0, web_ready=WEB_READY)
    A.contact_ms, A.attack_ms = CONTACT_MS, ATTACK_MS
    A.FLAP_THIGH_R = 4.0
    A.death_spear = 140.0
    A.walk_hand_l_lift, A.stun_hand_l_lift = 1.0, 2.5
    A.walk_hand_r_lift = 2.0          # the dragged clapper's flight clears the ground through the stride
    install_poses(A)
    return A


def slam(A):
    """Attack02 (钟锤 Clapper Slam [W2]): the slam keys on the gnawer attack timing, Contact 800, the bell note."""
    c = A.attack("Attack02", length=SLAM_MS, contact=SLAM_CONTACT_MS, keys_fn=A.overrides["slam_keys"],
                 notifies={"FX_BellRing": SLAM_CONTACT_MS})
    c.manifest_extra = {"usage": "[W2] 钟锤 Clapper Slam (monsters.md 2.4): circle r2 at the target, windup 800 ms, "
                                 "x1.6, cooldown 6 s; the telegraph fills to full on Contact; FX_BellRing = the bell "
                                 "note on impact"}
    return c


STUN_DROOP = 4.0


def clips(A) -> list:
    return [A.idle(length=IDLE_MS, weapon_wave=0.8, extra=idle_layer(A)), A.walk(), A.attack(), slam(A), A.hurt(260.0),
            A.death(900.0), A.stun(length=700.0, droop=STUN_DROOP, wlat=-0.2, hand_r=u3(A, *STUN_HAND),
                                   head_droop=10.0, head_roll=12.0)]


GRIP_PARTS = ("fist_r", "knuckle_r", "thumb_r", "claw_r")


def quick(out: Path, hunt: bool = False) -> None:
    import mship
    GN.march_stage()
    pal, kit, body, clapper, atts = build_all(hunt)
    A = anims(kit)
    attach = [(clapper, "weapon_r")]
    if hunt:
        attach += [(atts[0], "weapon_r"), (atts[1], TAGS_BONE)]
    mship.quick_review(ASSET, kit.rig, [body], attach, A, out, HEIGHT, GN.OUTLINE_CLASS, blob_radius=0.36,
                       px=GN.OUTLINE_PX)


def anim_quick(out: Path, names, facing: str = "se") -> None:
    import mship
    GN.march_stage()
    pal, kit, body, clapper, atts = build_all(False)
    A = anims(kit)
    R.attach_to_bone(clapper, kit.rig, "weapon_r")
    A.bind_meshes(body, {"weapon_r": clapper})
    cs = [c for c in clips(A) if not names or c.name in names]
    mship.anim_review(kit.rig, [body], [(clapper, "weapon_r")], cs, TOKEN, out, HEIGHT, GN.OUTLINE_CLASS,
                      blob_radius=0.36, facing=facing, px=GN.OUTLINE_PX)


def build_lod1(pal, kit):
    """LOD1 within the 3.3 k budget (the ratio scaled down until the decimated parts fit)."""
    import mship
    ratio = LOD1_RATIO
    for _ in range(6):
        body1 = build_body(pal, kit, lod=1, ratio=ratio)
        n = mship.toon_tris(body1)
        if n <= LOD1_BUDGET:
            break
        me = body1.data
        bpy.data.objects.remove(body1, do_unlink=True)
        bpy.data.meshes.remove(me)
        ratio *= (LOD1_BUDGET - 100) / n
    print(f"[lod1] {ASSET}: {n} toon tris at ratio {ratio:.3f}")
    return body1


# limb gate: the long arms swinging the clapper overhead / thrown out in the fall against the head and the big jaw
QA_GROUPS = {"arms": ["arm", "elbow", "fist", "claw", "knuckle", "thumb"],
             "headneck": ["neck", "skull", "jaw", "muzzle", "brow"]}
QA_PAIRS = {"armHead": ("arms", "headneck")}


def make_ship():
    import living_ship as LS
    import mship
    GN.march_stage()
    pal, kit, body, clapper, (lip, tags) = build_all(True)
    body1 = build_lod1(pal, kit)
    A = anims(kit)
    R.attach_to_bone(clapper, kit.rig, "weapon_r")
    A.bind_meshes(body, {"weapon_r": clapper})
    # the fall rests the lowest of body, clapper and Hungerbell's bell lip on the floor (one Death for the mesh and
    # its variant: the lip is wider than the ball)
    R.attach_to_bone(lip, kit.rig, "weapon_r")
    A.ground_pts.add_rigid(lip, "weapon_r", step=1, tag="weapon_r")
    cs = clips(A)
    note = {"variantScale": HUNT_SCALE, "attach": "bone socket, identity transform (weapon-socket convention)"}
    items = [mship.Item(clapper, CLAPPER, "weapon_r", "club",
                        sockets=[{"name": "tip", "pos": (0.0, BALL_Y, 0.0)}, {"name": "mid", "pos": (0.0, 0.25, 0.0)}],
                        extra={"lengthCm": round((TIP_Y - EYE_Y + 0.045) * 100.0, 1),
                               "note": "monsters.md 2.4: a church-bell clapper as a club (bronze #8A6A3A); the "
                                       "cleaver of the chief it replaces is not used"}, grip=clapper_grip),
             mship.Item(lip, BELL_LIP, "weapon_r", variant=HUNT_ID, category="Monsters",
                        extra=dict(note, note2="worn over SM_MonWeapon_BellClapper (same weapon_r space)")),
             mship.Item(tags, TAGS, TAGS_BONE, variant=HUNT_ID, category="Monsters",
                        extra=dict(note, glint={"region": "tags", "color": "#C8A870"}))]
    groups = {"body_nf": mship.parts_except(body, GRIP_PARTS),
              "body_noneck": mship.parts_except(body, ("neck", "torso", "deltoid", "blade", "tuft", "spine_knob",
                                                       "jaw", "tooth_low")),
              **QA_GROUPS}
    pairs = {"clapperBody": ("item:" + CLAPPER, "body_nf"), "lipBody": ("item:" + BELL_LIP, "body_nf"),
             "tagsBody": ("item:" + TAGS, "body_noneck"), **QA_PAIRS}
    ms = mship.MonsterShip(
        asset=ASSET, token=TOKEN, preview_dir="mon_bonegorger", pal=pal, A=A, body=body, items=items, clips=cs,
        outline_class=GN.OUTLINE_CLASS, game_ids=[GAME_ID, HUNT_ID], blob_radius=0.36, height=HEIGHT, lod1=body1,
        display={"zh-CN": "饕骸者", "en": "Bonegorger"},
        fx={"telegraph": {**GN.TELEGRAPH, "untilMs": round(A.windup_frac * CONTACT_MS, 1)},
            "attackTrail": {"color": GN.TRAIL, "alpha": 0.45, "sockets": ["tip", "mid"], "item": CLAPPER}},
        qa_groups=groups, qa_pairs=pairs,
        qa_limits={"ground": -0.5, **{k: 0 for k in pairs}},
        outline_px=GN.OUTLINE_PX, outline_px_note=GN.OUTLINE_PX_NOTE,
        variants=VARIANTS,
        extra_previews=lambda ms_, rv, acts: (LS.corpse_preview(ms_, rv, acts),
                                              LS.hunt_previews(ms_, rv, acts, VARIANTS,
                                                               keys=(("Idle", 0, "se"), ("Attack02", 48, "sw")))),
        variant_previews=("hunt_leaders.png", "hunt_close.png"),
        game_keys=[("Idle", 0, "se"), ("Walk", 13, "se"), ("Attack01", 28, "se"), ("Attack01", 42, "se"),
                   ("Attack02", 48, "se"), ("Hurt", 0, "se"), ("Death", 54, "se"), ("Stun", 10, "se")],
        contact_keys=[("Attack01", 42, f) for f in ("se", "sw", "ne", "nw")],
        close_keys=[("Attack01", 14, "se"), ("Attack01", 28, "se"), ("Attack01", 42, "se"), ("Attack02", 32, "se"),
                    ("Attack02", 48, "se"), ("Attack02", 48, "side"), ("Hurt", 0, "se"), ("Death", 54, "se"),
                    ("Stun", 10, "front"), ("Walk", 13, "side")],
        back_clips=("Attack01", "Attack02", "Death"),
        skeleton_bones_required=("root", "pelvis", "head", "jaw", "weapon_r", "weapon_l", "ear_l_02", "loin_f_02",
                                 "pelt_03", "headdress_04", "totem_feather_02", "ik_foot_l", "ik_hand_gun"),
        lod_note=f"LOD0 budget {LOD0_BUDGET} / LOD1 {LOD1_BUDGET} toon tris (monsters.md 5: bonegorger = the chief's "
                 "budget).",
        extra={**GN.remake_extra("overhead_club", CONTACT_MS, REF_SPEED,
                                 "Clips in place; the core moves the actor (play rate = v / ref). Attack01 clubs "
                                 "down-forward with the bell clapper (Contact 700: the long wind-up reads); Attack02 "
                                 "is the [W2] Clapper Slam (ball into the ground on Contact 800, FX_BellRing). Death "
                                 "pitches onto the belly and rolls onto its side. Stun (C5) loops."),
               **GN.marker_extra(MARKER)},
    )
    return ms


def ship(previews: bool = True, verify: bool = True) -> int:
    import mship
    ms = make_ship()
    t0, h0 = mship.toon_tris(ms.body), M.tri_count(ms.body, 1)
    t1, h1 = mship.toon_tris(ms.lod1), M.tri_count(ms.lod1, 1)
    print(f"[budget] {ASSET}: LOD0 {t0} toon (<= {LOD0_BUDGET}) + {h0} hull ({h0 / max(1, t0):.2f} x), "
          f"LOD1 {t1} (<= {LOD1_BUDGET}) + {h1} hull ({h1 / max(1, t1):.2f} x)")
    res = mship.ship(ms, previews=previews, verify=verify)
    return mship.main_result(res)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    scratch = scratch_root("a4", "bonegorger")
    if "--quick" in args:
        quick(scratch / ("hunt" if "hunt" in args else "base"), "hunt" in args)
    elif "--anims" in args:
        names = [a for a in args[args.index("--anims") + 1:] if not a.startswith("--")]
        fac = next((a.split("=")[1] for a in args if a.startswith("--facing=")), "se")
        anim_quick(scratch / "anims", names, fac)
    else:
        sys.exit(ship(previews="--no-previews" not in args, verify="--no-verify" not in args))

"""哥布林萨满 Goblin Shaman — ``SK_Mon_GoblinShaman`` (spec art-inventory-ch1.md §4.4, §10; DECISIONS M5, R2).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/goblin_shaman.py
        [--quick] [--anims Clip … --facing=se] [--no-previews] [--no-verify]

The tribe's hunched spirit-caller (0.95 m, feathers to ≈ 1.07 m): a cracked bird-skull mask over brow and nose
with a jutting beak, two eye holes holding green spirit embers and a red war-paint stripe; a beaded hide band and a
fan of four feathers (teal / red / cream / red, dark barred tips) behind the head on ``headdress_*``; a mangy fur
shawl with dark strokes and a ragged fringe, a bone-tooth necklace with a glowing spirit-stone; a painted belly
spiral; a stitched hide skirt over the thighs (front / back panels on ``loin_*``) hung with bone charms, a belt and
a dark-hide pouch; two bone bracers on the totem arm. Weapon: ``SM_Mon_GoblinShaman_Totem`` — a gnarled staff
(21 u up / 12 u down) with a knot and two leather wraps, crowned with a horned skull holding the green spirit-fire
(emissive flame core, socket ``spirit_fire``); its two dangling feathers are part of the body on
``totem_feather_*`` (children of ``weapon_r``). M5: a **ranged caster** — Attack01 draws the totem back past the
shoulder, sweeps it up and thrusts it at the target, releasing the spirit-fire bolt at 250 ms (``FX_SpiritBolt``,
``Release``, ``Contact``). Clips: Idle 667, Walk 600 (1.35 m/s), Attack01 500, Hurt 200, Death 500 (the fire dies
out), Taunt 900, Stun 600.

Web source: ``src/graphics/sprites/monsters/GoblinShaman.ts`` (palette :41-60, mask :123-160, headdress :96-121,
totem :229-275, READY :480-490, ATTACK :510-515, fx :540-619).
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
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import mesh as M, outline, rig as R, scene, shading  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402
from kit.rig import Bind  # noqa: E402

import common as C  # noqa: E402
from common import BP, K, V  # noqa: E402
import goblins as GB  # noqa: E402
import mgeo as G  # noqa: E402
from mclips import GoblinAnims, WINDUP_END_MS, CONTACT_MS  # noqa: E402

ASSET = "SK_Mon_GoblinShaman"
TOKEN = "Mon_GoblinShaman"
TOTEM = "SM_Mon_GoblinShaman_Totem"
PREFIX = "shaman"
HEIGHT = 0.95
WEB = GB.Web(U=0.0217 * 0.93, thigh=9.0, shin=9.0, upper_arm=8.4, fore_arm=8.0, torso=13.0, neck=6.2, ankle=1.6,
             hip=2.2, shoulder=4.6, lean=0.30, head_tilt=-0.26, root=(46.0, 73.5), bulk=1.0, grin=0.6)
OUTLINE_CLASS = "monster"
REF_SPEED = 1.35
WALK = dict(cycle=600.0, duty=0.36, lift=0.06, bob=0.016, lean=3.0, arm=0.05, crouch=-0.03, twist=4.0,
            hand_swing=0.3)
WEB_READY = dict(root=(46.0, 73.5), footN=52.5, footF=42.0, lean=0.30, head=-0.26)
STAFF_UP, STAFF_DOWN = 21.0, 12.0

# headdress fan (teal / red / cream / red): (lateral root, length, splay deg from upright, + = the character's left)
FAN = ((-3.4, 11.5, -40.0), (-1.2, 13.0, -13.0), (1.2, 12.5, 13.0), (3.4, 11.0, 40.0))
FAN_ROOT = (5.3, -3.4)          # root on the back of the head (head units h, f)
FAN_BACK_DEG = 14.0             # lean back from upright (the web swept them ~36° back: end-on from behind at W1)

BONE = Region("#ECE2C6", .30, .40)
HIDE = Region("#7A5634", .42, .30)
WOOD = Region("#6A4A2A", .42, .30)
SPIRIT = "#4DFF7A"
SPIRIT_HOT = "#D4FFB0"
LOOK = GB.Look(
    skin="#6F9C46", skin_l=0.34, skin_dark="#48692C", skin_dark_l=0.20, ear_in="#3A5824", cloth="#5A4630",
    strap="#3E2C1C",
    extra=dict(
        fur=Region("#8A7A64", .42, .35), fur_dark=Region("#5A4C3E", .42, .25), hide=HIDE,
        hide_dark=Region("#543820", .42, .25), hide_seam=HIDE.shade_region(.30, .12),
        feather_red=Region("#C8402E", .42, .35), feather_teal=Region("#2F9C8E", .42, .35),
        feather_cream=Region("#E8DCB4", .42, .35), feather_tip=Region("#3A2A26", .35, .15),
        bead_red=Region("#D24A2A", .35, .35), bead_teal=Region("#3FBFAE", .35, .35),
        socket=Region("#10180C", .30, .08), ember=Region("#BEFFBE", .10, .30, e=1.0),
        gem=Region("#4DFF88", .30, .60, e=0.75), paint=Region("#BE281E", .35, .30),
        spiral=Region("#F0F0DC", .30, .30), bone_crack=BONE.shade_region(.30, .12),
    ))

TOTEM_REGIONS = outline.ink_regions(dict(
    wood=WOOD, wood_dark=Region("#4A321C", .42, .25), hide_dark=Region("#543820", .42, .25),
    hide_line=Region("#543820").shade_region(.30, .12), bone=BONE, socket=Region("#14200E", .30, .08),
    glint=Region("#B8FFC0", .10, .30, e=1.0), fire=Region(SPIRIT, .0, .0, e=1.0),
    fire_hot=Region(SPIRIT_HOT, .0, .0, e=1.0),
), OUTLINE_CLASS)


class ShamanKit(GB.GoblinKit):
    @staticmethod
    def torso_hide(s: float, a: float) -> bool:
        """Chest and shoulders inside the fur shawl (hem s ≈ 0.56–0.60, 0.35 u inside → 0.5 u out)."""
        return 0.70 < s < 1.0

    def totem_lash(self) -> Vector:
        """Where the totem's two feathers are tied: weapon_r space, 16.5 u up the staff, on its back side."""
        rb = self.rig.obj.data.bones["weapon_r"]
        return rb.matrix_local @ Vector((0.0, (STAFF_UP - 4.5) * self.U, -1.3 * self.U))

    def fan_roots(self):
        """The headdress fan: feathers stand nearly upright (FAN_UP / FAN_BACK) and splay sideways (FAN_SPLAY).
        The web's backward sweep (up .75, back .55 of the length) pointed every feather straight at the W1 camera
        from behind (pitch −50°: ne / back / nw saw a dark edge-on bar); upright and splayed, the vanes show
        their colours from the front and from behind alike."""
        out = []
        b = math.radians(FAN_BACK_DEG)
        for lat, ln, ang in FAN:
            a = math.radians(ang)
            r0 = self.hp(FAN_ROOT[0], FAN_ROOT[1], lat)
            r1 = self.hp(FAN_ROOT[0] + ln * math.cos(a) * math.cos(b), FAN_ROOT[1] - ln * math.sin(b),
                         lat + ln * math.sin(a) * math.cos(b))
            out.append((r0, r0 + (r1 - r0) * 0.55))
        return out


class _Off:
    def __init__(self, surf, off):
        self.s, self.off = surf, off

    def point(self, u, a, out=0.0):
        return self.s.point(u, a, self.off + out)

    def normal(self, u, a):
        return self.s.normal(u, a)


def skirt_bind(kit, top_z: float, bot_z: float) -> Bind:
    """Hide skirt weights: the waist on the pelvis, the front / back panels down the ``loin_f`` / ``loin_b`` chains
    (the flap envelope keeps them clear of the thighs), the sides blended onto the thighs."""
    J = kit.J
    names = ("pelvis", "loin_f_01", "loin_f_02", "loin_b_01", "loin_b_02", "thigh_l", "thigh_r")

    def fn(co):
        x, y, z = co[:, 0], co[:, 1], co[:, 2]
        t = np.clip((top_z - z) / max(top_z - bot_z, 1e-6), 0.0, 1.0)
        ang = np.arctan2(x, -y)                                 # 0 front, ±pi back, + left
        fr = np.clip(np.cos(ang), 0, None) ** 1.5
        bk = np.clip(-np.cos(ang), 0, None) ** 1.5
        sl = np.clip(np.sin(ang), 0, None) ** 1.5
        sr = np.clip(-np.sin(ang), 0, None) ** 1.5
        tot = fr + bk + sl + sr + 1e-6
        fr, bk, sl, sr = fr / tot, bk / tot, sl / tot, sr / tot
        top = 1.0 - GB._smooth_w(t, 0.0, 0.3)
        low = 1.0 - top
        lk = GB._smooth_w(t, 0.35, 0.8)
        w = np.stack([top + 1e-4, low * fr * (1 - lk), low * fr * lk, low * bk * (1 - lk), low * bk * lk,
                      low * sl * 0.8, low * sr * 0.8], axis=1)
        return w
    return Bind.custom(names, fn)


LOD1_RATIO = 0.42
LOD1_SKIP = ("skirt_seam", "charm_cord", "mask_crack", "totem_cord", "beak_keel", "bead", "charm", "elbow", "knee",
             "bracer")


def build_body(pal: Palette, kit: ShamanKit, lod: int = 0) -> bpy.types.Object:
    from kit import asset
    b = GB.builder(ASSET, pal, PREFIX, lod, LOD1_SKIP, LOD1_RATIO)
    b.regions(**GB.regions_for(LOOK))
    u, hk = kit.u, kit.hk
    J = kit.J
    ids = kit.add_head(b, face=False)
    skull = ids["skull"]
    head = Bind.rigid("head")
    hsurf = GB._HeadSurf(kit)
    # ── bird-skull mask: a shell over brow and nose (h 6.8 → 0.2, ±60°), the beak jutting forward ────────
    def mask_low(a):          # lower edge: dips under the eye holes toward the beak, rises at the temples
        k = abs(a) / 58.0
        return 0.4 - 1.0 * math.cos(math.radians(a) * 1.4) * (1 - k) - 0.6 * (1 - k) ** 3 + 1.2 * k ** 3
    def mask_top(a):          # brow edge: rounded down toward the temples (a skull, not a plate)
        return 6.85 - 2.2 * (abs(a) / 58.0) ** 2.2

    rows_o, rows_i = [], []
    angs = list(np.linspace(-58.0, 58.0, 15))
    for i in range(7):
        f = i / 6
        ro, ri = [], []
        for a in angs:
            h = mask_low(a) + (mask_top(a) - mask_low(a)) * f
            ro.append(kit.head_point(h, a, hk(0.62)))
            ri.append(kit.head_point(h, a, hk(0.05)))
        rows_o.append(ro)
        rows_i.append(ri)
    mask = C.thick_patch(rows_o, rows_i)
    mid = b.add(mask, "bone", head, name="mask", hull_pad=True)
    # beak: a wedge from the face to the tip (h −1.6, f 12.6), keel on top
    bk_pts = [kit.hp(3.0, 6.4), kit.hp(2.2, 8.0), kit.hp(1.0, 9.9), kit.hp(-0.4, 11.5), kit.hp(-1.5, 12.7)]
    bk_r = [(hk(1.75), hk(1.55)), (hk(1.5), hk(1.35)), (hk(1.1), hk(1.05)), (hk(0.62), hk(0.6)), (hk(0.12), hk(0.12))]
    beak = M.tube(bk_pts, bk_r, sides=10, up=tuple(kit.hdir(0, 0, 1)), dome0=0.0, dome1=hk(0.1), exp=2.6)
    bid = b.add(beak, "bone", head, name="beak", hull_pad=True)
    keel = [kit.hp(3.9, 6.0) , kit.hp(3.2, 7.8), kit.hp(2.1, 9.7), kit.hp(0.6, 11.4)]
    kn = []
    for i in range(len(keel)):
        tg = (keel[min(i + 1, 3)] - keel[max(i - 1, 0)]).normalized()
        n = kit.hdir(0, 0, 1).cross(tg).normalized()
        kn.append(n if n.dot(kit.hdir(1, 0.5)) > 0 else -n)
    outer = [[p - kit.hdir(0, 0, 1) * hk(0.28) + n * hk(0.25), p + kit.hdir(0, 0, 1) * hk(0.28) + n * hk(0.25)]
             for p, n in zip(keel, kn)]
    inner = [[p - kit.hdir(0, 0, 1) * hk(0.28) - n * hk(0.4), p + kit.hdir(0, 0, 1) * hk(0.28) - n * hk(0.4)]
             for p, n in zip(keel, kn)]
    b.add(C.thick_patch(outer, inner), "bone_crack", head, name="beak_keel", outline=False, covered_by=bid)
    # eye holes with green spirit embers (emissive), the war-paint stripe and a crack
    msurf = _Off(hsurf, hk(0.62))
    for sg in (1, -1):
        h0, a0 = 3.2, sg * 24.0
        hole = G.surface_patch(msurf, h0 - 0.8, h0 + 0.8, a0 - 9.0, a0 + 9.0, hk(0.03), nu=4, na=6,
                               shape=lambda tu, ta: math.sqrt(max(0.0, 1 - (2 * tu - 1) ** 2)))
        b.add(hole, "socket", head, name="mask_eye", outline=False, covered_by=mid)
        emb = M.ellipsoid((hk(0.55), hk(0.42), hk(0.25)), msurf.point(h0 + 0.15, a0, hk(0.04)), sides=6, rings=4)
        b.add(emb, "ember", head, name="mask_ember", outline=False, covered_by=mid)
    stripe = G.surface_paint(msurf, [(6.6, -40.0), (4.2, -43.0), (1.7, -44.0)], hk(0.85), hk(0.05), steps=6)
    b.add(stripe, "paint", head, name="mask_paint", outline=False, covered_by=mid)
    crack = G.surface_paint(msurf, [(6.7, 16.0), (5.7, 13.0), (5.1, 17.0), (4.5, 15.0)], hk(0.32), hk(0.05), steps=6)
    b.add(crack, "bone_crack", head, name="mask_crack", outline=False, covered_by=mid)
    # beaded hide band round the head, over the mask's top edge
    band = G.surface_ring(hsurf, lambda a: 6.15 + 0.2 * math.cos(math.radians(a)), hk(1.3), hk(0.95), segs=24)
    bnd = b.add(band, "hide", head, name="head_band", hull_pad=True)
    for a, reg in ((-92.0, "bead_red"), (126.0, "bead_teal"), (172.0, "bead_red"), (80.0, "bead_teal"),
                   (-150.0, "bead_teal"), (40.0, "bead_red"), (-40.0, "bead_teal")):
        h = 6.15 + 0.2 * math.cos(math.radians(a))
        bd = M.ellipsoid((hk(0.62), hk(0.62), hk(0.62)), hsurf.point(h, a, hk(1.25)), sides=6, rings=4)
        b.add(bd, reg, head, name="bead", outline=False, covered_by=bnd)
    # feather fan behind the head (headdress_01..04)
    bones = kit.rig.obj.data.bones
    for i, reg in enumerate(("feather_teal", "feather_red", "feather_cream", "feather_red")):
        nm = f"headdress_{i + 1:02d}"
        r0, r1 = bones[nm].head_local, bones[nm].tail_local
        ln = FAN[i][1]
        d = (r1 - r0).normalized()
        face = kit.hdir(0.35, -1.0, 0).normalized()         # the vane faces back and up (seen from behind / above)
        fth = G.feather(hk(ln), hk(2.6), hk(0.35), curl=0.12, bend=0.06, rows=6)
        fth.transform(R.frame(r0 - d * hk(0.8), d, -face))
        b.add(fth, reg, Bind.rigid(nm), name=f"feather_{i}", sub_regions={1: "feather_tip", 2: "fur_dark"},
              hull_pad=True)
    # ── torso: skin with the painted belly spiral, shawl, necklace with the spirit-stone ─────────────────
    t = kit.torso
    tid = kit.add_torso(b, necklace=False, sash=False, belt=False)["torso"]
    spine = Bind.blend("pelvis", "spine_01", "spine_02", "spine_03", falloff=3.0, smooth=0.02)
    pts = []
    for k in range(22):
        th = 0.2 + (math.pi * 1.7 - 0.2) * k / 21
        r = 1.0 + 0.8 * k / 21
        pts.append((0.36 + r * 0.075 * math.sin(th), 17.0 + r * 10.0 * math.cos(th)))
    sp = G.surface_paint(t, pts, u(0.8), u(0.1), steps=30)
    b.add(sp, "spiral", spine, name="belly_spiral", outline=False, covered_by=tid)
    shw = G.SpineLoft(kit.spine_points(), [(0.56, u(6.6), u(7.8), u(0.4)), (0.70, u(6.9), u(7.6), u(0.3)),
                                           (0.86, u(6.6), u(7.2), u(0.2)), (0.98, u(5.6), u(5.8), u(0.0)),
                                           (1.06, u(4.0), u(4.4), u(0.0)), (1.12, u(3.0), u(3.8), u(0.1))], exp=2.3)
    angs = G.angles(26)
    ss = list(np.linspace(0.6, 1.12, 6))
    outer, inner = [], []
    for r, s_ in enumerate(ss):
        ro, ri = [], []
        for i, a in enumerate(angs):
            s2 = s_ - ((0.05 if i % 2 else 0.0) + 0.015 * math.sin(i * 1.7) if r == 0 else 0.0)
            ro.append(shw.point(s2, a, u(0.5)))
            ri.append(shw.point(s2, a, -u(0.35)))
        outer.append(ro)
        inner.append(ri)
    shawl = C.thick_patch(outer, inner, wrap_u=True)
    sbind = kit.blend4("spine_03", "neck_01", "clavicle_l", "clavicle_r", "upperarm_l", "upperarm_r", falloff=3.0,
                       smooth=0.03)
    shid = b.add(shawl, "fur", sbind, name="shawl", hull_pad=True)
    for i in range(12):
        a = -180.0 + 30.0 * i + 9.0
        st = G.surface_paint(_Off(shw, u(0.5)), [(0.82, a), (0.70, a + 3.0), (0.64, a + 1.0)], u(0.75), u(0.1),
                             steps=4, taper=0.6)
        b.add(st, "fur_dark", sbind, name="fur_stroke", outline=False, covered_by=shid)
    # tooth necklace over the shawl front with the spirit-stone
    so = _Off(shw, u(0.5))
    cord = G.surface_strap(so, [(0.78, -52.0), (0.70, -26.0), (0.67, 0.0), (0.70, 26.0), (0.78, 52.0)], u(0.4),
                           u(0.22), steps=10)
    b.add(cord, "strap", sbind, name="neck_cord", outline=False, covered_by=shid)
    for i in range(5):
        a = -36.0 + i * 18.0
        s_ = 0.69 + 0.0004 * a * a / 9.0
        top = so.point(s_, a, u(0.25))
        down = (so.point(s_ - 0.06, a) - so.point(s_, a)).normalized()
        n = so.normal(s_, a)
        if i == 2:
            continue
        b.add(G.cone(top, top + down * u(2.0) + n * u(0.3), u(0.5), sides=5), "bone", sbind, name="neck_tooth",
              outline=False, covered_by=shid)
    gp = so.point(0.645, 0.0, u(0.55))
    gn = so.normal(0.645, 0.0)
    gem = M.gem(u(1.0), u(0.9), u(1.5))
    gem.transform(G.frame_z(gp, gn, Vector((0, 0, 1))) @ Matrix.Rotation(math.radians(90), 4, "X"))
    b.add(gem, "gem", sbind, name="spirit_stone", outline=False, covered_by=shid)
    # ── hide skirt with a stitched seam, belt, pouch, bone charms ──────────────────────────────────────
    P = J["pelvis"]
    top_z = t.point(0.10, 0.0).z
    bot_z = J["knee_l"].z - u(0.8)
    sk_rings = [(0.0, u(5.4), u(5.8), u(0.6)), (0.45, u(6.1), u(6.4), u(0.4)), (1.0, u(6.9), u(7.1), u(0.2))]
    rows_o, rows_i = [], []
    angs = G.angles(24)
    for k in range(6):
        f = k / 5
        z = top_z + (bot_z - top_z) * f
        lat = np.interp(f, [r[0] for r in sk_rings], [r[1] for r in sk_rings])
        dep = np.interp(f, [r[0] for r in sk_rings], [r[2] for r in sk_rings])
        cf = np.interp(f, [r[0] for r in sk_rings], [r[3] for r in sk_rings])
        ro, ri = [], []
        for i, a in enumerate(angs):
            ra = math.radians(a)
            zz = z
            if k == 5:                             # ragged hem
                zz += u(0.9) * (0.5 + 0.5 * math.sin(i * 2.3)) * (1 if i % 2 else 0.4)
            c = Vector((0.0, P.y - cf, zz))
            pt = c + Vector((lat * math.sin(ra), -dep * math.cos(ra), 0.0))
            n = Vector((math.sin(ra) / max(lat, 1e-6), -math.cos(ra) / max(dep, 1e-6), 0.0)).normalized()
            ro.append(pt + n * u(0.35))
            ri.append(pt - n * u(0.35))
        rows_o.append(ro)
        rows_i.append(ri)
    skirt = C.thick_patch(rows_o, rows_i, wrap_u=True, sub_tags=True)
    skid = b.add(skirt, "hide", skirt_bind(kit, top_z, bot_z), name="skirt", sub_regions={2: "hide_dark"},
                 hull_pad=True)
    for a in (17.0, 138.0, -120.0):                 # stitched seams (dashes)
        for k in range(5):
            f0, f1 = 0.08 + k * 0.18, 0.08 + k * 0.18 + 0.09
            ra = math.radians(a)

            def at(f, ra=ra):
                z = top_z + (bot_z - top_z) * f
                lat = np.interp(f, [r[0] for r in sk_rings], [r[1] for r in sk_rings]) + u(0.37)
                dep = np.interp(f, [r[0] for r in sk_rings], [r[2] for r in sk_rings]) + u(0.37)
                cf = np.interp(f, [r[0] for r in sk_rings], [r[3] for r in sk_rings])
                return Vector((lat * math.sin(ra), P.y - cf - dep * math.cos(ra), z))
            p0, p1 = at(f0), at(f1)
            b.add(M.tube([p0, p1], [u(0.26), u(0.26)], sides=4, dome0=u(0.1), dome1=u(0.1)), "hide_seam",
                  skirt_bind(kit, top_z, bot_z), name="skirt_seam", outline=False, covered_by=skid)
    bel = kit.add_rope_belt(b, tid, s0=0.10, region="strap", stroke=None, width_u=1.7, thick_u=0.9)
    pc = t.point(0.06, -52.0, u(1.6))
    pouch = M.ellipsoid((u(1.6), u(1.0), u(1.9)), pc, sides=8, rings=6)
    b.add(pouch, "hide_dark", Bind.blend("pelvis", "thigh_r", falloff=3.0), name="pouch", hull_pad=True)
    for a, ln in ((30.0, 2.8), (62.0, 2.2), (-14.0, 2.5)):
        top = t.point(0.09, a, u(1.0))
        n = t.normal(0.09, a)
        bot = top + Vector((0, 0, -u(ln))) + n * u(0.3)
        b.add(M.capsule(top, bot, u(0.15), u(0.15), sides=4, cap_rings=1, mid=0), "strap",
              Bind.blend("pelvis", "loin_f_01", falloff=3.0), name="charm_cord", outline=False, covered_by=skid)
        b.add(G.cone(bot + Vector((0, 0, u(0.3))), bot - Vector((0, 0, u(1.5))) + n * u(0.2), u(0.55), sides=5),
              "bone", Bind.blend("pelvis", "loin_f_01", "loin_f_02", falloff=3.0), name="charm", outline=False,
              covered_by=skid)
    # ── limbs + two bone bracers on the totem arm ──────────────────────────────────────────────────────
    for s, sg in (("l", 1), ("r", -1)):
        kit.add_arm(b, s, sg)
        kit.add_leg(b, s, sg)
    E, W = J["elbow_r"], J["wrist_r"]
    for f, r in ((0.35, 2.35), (0.70, 2.2)):
        ring = M.tube([E.lerp(W, f - 0.07), E.lerp(W, f + 0.07)], [u(r), u(r)], sides=12, dome0=0.0, dome1=0.0)
        b.add(ring, "bone", Bind.rigid("lowerarm_r"), name="bracer", hull_pad=True)
    # ── the totem's dangling feathers (body mesh, totem_feather_* chain under weapon_r) ─────────────────
    for nm, reg, ln in (("totem_feather_01", "feather_red", 6.5), ("totem_feather_02", "feather_teal", 5.5)):
        r0 = bones[nm].head_local
        d = (bones[nm].tail_local - r0).normalized()
        fth = G.feather(u(ln), u(1.8), u(0.3), curl=0.1, bend=0.05, rows=5)
        fth.transform(R.frame(r0, d, Vector((1, 0, 0))))
        b.add(fth, reg, Bind.rigid(nm), name=f"totem_{reg}", sub_regions={1: "feather_tip", 2: "fur_dark"},
              hull_pad=True)
        cordp = M.capsule(r0, r0 + d * u(0.6), u(0.14), u(0.14), sides=5, cap_rings=1)
        b.add(cordp, "strap", Bind.rigid(nm), name="totem_cord", outline=False)
    body = asset.finish_mesh(b, pal, OUTLINE_CLASS, rig=kit.rig, grounding_height=HEIGHT,
                             name=f"{ASSET}_LOD{lod}" if lod else None)
    body["af_lod"] = lod
    return body


# ── totem ──────────────────────────────────────────────────────────────────────────────────────────────
def add_fire(b: M.Builder, u: float) -> None:
    """Spirit-fire core: three emissive tongues + a hot core in a cup above the skull (the runtime adds the
    flicker and the additive glow, manifest ``fx.spiritFire``)."""
    rig = Bind.rigid("root")
    yc = (STAFF_UP + 2.4) * u
    fy = yc + 2.4 * u
    for k, (dx, dz, h, r, reg) in enumerate(((0.0, 0.0, 6.0, 1.7, "fire"), (-1.0, 0.4, 4.2, 1.0, "fire"),
                                             (1.1, -0.3, 4.6, 1.1, "fire"), (0.1, 0.0, 3.4, 0.9, "fire_hot"))):
        prof = [(r * 0.55 * u, 0.0), (r * u, h * 0.22 * u), (r * 0.85 * u, h * 0.5 * u), (r * 0.4 * u, h * 0.8 * u),
                (0.02 * u, h * u)]
        tongue = M.lathe(prof, sides=10, cap1="fan")
        tongue.transform(Matrix.Translation((dx * u, fy, dz * u)) @ Matrix.Rotation(math.radians(-90), 4, "X")
                         @ Matrix.Rotation(math.radians(8 * dx), 4, "Y"))
        tongue.deform(lambda co, k=k: Vector((co.x + 0.35 * u * math.sin((co.y - fy) / u * 0.9 + k),
                                              co.y, co.z)))
        b.add(tongue, reg, rig, name="fire", outline=False)


def build_fire(pal: Palette) -> bpy.types.Object:
    """The flame alone (review only: the ink is measured without this runtime-lit FX element)."""
    b = M.Builder(TOTEM + "_Fire", pal, prefix="shaman_totem")
    b.regions(**TOTEM_REGIONS)
    add_fire(b, WEB.U)
    obj = b.build(TOTEM + "_Fire")
    obj.data.materials.clear()
    obj.data.materials.append(shading.toon_material(pal))
    return obj


def build_totem(pal: Palette, fire: bool = True) -> bpy.types.Object:
    """Gnarled staff in weapon space (grip at the origin, +Y up the staff; web totem :229-275): shaft 21 u up /
    12 u down with a knot and two dark leather wraps, crowned by a horned skull (eye sockets with green glints)
    holding the emissive spirit-fire core (socket ``spirit_fire`` at its base)."""
    u = WEB.U
    b = M.Builder(TOTEM, pal, prefix="shaman_totem")
    b.regions(**TOTEM_REGIONS)
    rig = Bind.rigid("root")
    rng = scene.rng(TOTEM, "shaft")
    ys = np.linspace(-STAFF_DOWN, STAFF_UP - 1.0, 12)
    pts = [Vector((u * 0.45 * math.sin(y * 0.33 + 0.4) + u * rng.uniform(-0.1, 0.1), y * u,
                   u * 0.3 * math.cos(y * 0.27))) for y in ys]
    rad = [u * (0.78 - 0.18 * (y + STAFF_DOWN) / (STAFF_UP + STAFF_DOWN) + 0.08 * math.sin(y * 1.3)) for y in ys]
    shaft = M.tube(pts, rad, sides=9, up=(0, 0, 1), dome0=u * 0.35, dome1=0.0)
    sid = b.add(shaft, "wood", rig, name="shaft", hull_pad=True)
    for y, r in ((6.0, 1.25), (-6.5, 1.0)):
        k = M.ellipsoid((u * r, u * 1.4, u * r * 0.9), (-u * 0.55, y * u, 0.0), sides=10, rings=6)
        b.add(k, "wood_dark", rig, name="knot", hull_pad=True)
    for y0 in (STAFF_UP - 6.2, STAFF_UP - 4.0):
        w = M.tube([Vector((0, y0 * u, 0)), Vector((0, (y0 + 1.2) * u, 0))], [u * 1.05, u * 1.0], sides=10,
                   dome0=0.0, dome1=0.0)
        wid = b.add(w, "hide_dark", rig, name="wrap", hull_pad=True)
        for k in range(3):
            yy = (y0 + 0.2 + k * 0.4) * u
            l = M.tube([Vector((0, yy - 0.08 * u, 0)), Vector((0, yy + 0.08 * u, 0))], [u * 1.09] * 2, sides=10,
                       dome0=0.0, dome1=0.0)
            b.add(l, "hide_line", rig, name="wrap_line", outline=False, covered_by=wid)
    # horned skull crowning the staff: cranium (front = −Z of the item = toward the target when thrust), snout
    yc = (STAFF_UP + 2.4) * u
    cran = M.ellipsoid((u * 2.6, u * 2.8, u * 2.5), (0, yc, 0), sides=14, rings=9)
    cid = b.add(cran, "bone", rig, name="skull", hull_pad=True)
    snout = M.ellipsoid((u * 1.6, u * 1.3, u * 1.8), (0, yc - 1.4 * u, -1.9 * u), sides=10, rings=6)
    b.add(snout, "bone", rig, name="snout", outline=False, covered_by=cid)
    for x in (-1.15, 1.15):
        sock = M.ellipsoid((u * 0.8, u * 0.85, u * 0.5), (x * u, yc + 0.1 * u, -2.2 * u), sides=8, rings=5)
        b.add(sock, "socket", rig, name="socket", outline=False, covered_by=cid)
        gl = M.ellipsoid((u * 0.33, u * 0.33, u * 0.2), (x * u + 0.1 * u, yc + 0.3 * u, -2.62 * u), sides=6, rings=4)
        b.add(gl, "glint", rig, name="glint", outline=False, covered_by=cid)
    for sx in (-1, 1):                              # horns sweeping out and up (web: to ±4.4 u, 8.4 u above)
        ctl = [Vector((sx * 1.6, 1.5, 0.3)), Vector((sx * 4.6, 2.6, 0.6)), Vector((sx * 6.2, 4.6, 0.4)),
               Vector((sx * 5.2, 7.2, 0.0)), Vector((sx * 4.0, 8.6, -0.3))]
        dense = []
        for i in range(len(ctl) - 1):
            for k in range(3):
                dense.append(ctl[i].lerp(ctl[i + 1], k / 3))
        dense.append(ctl[-1])
        hp_ = [Vector((p.x * u, yc + p.y * u, p.z * u)) for p in dense]
        hr = [u * (1.05 * (1 - 0.9 * (i / (len(hp_) - 1)) ** 1.2) + 0.06) for i in range(len(hp_))]
        b.add(M.tube(hp_, hr, sides=9, dome0=0.0, dome1=u * 0.08), "bone", rig, name="horn", hull_pad=True)
    if fire:
        add_fire(b, u)
    obj = b.build(TOTEM if fire else TOTEM + "_NoFire")
    mt = shading.toon_material(pal)
    outline.bake_hull(obj, outline.WIDTH_CM[OUTLINE_CLASS] / 100.0, mt, shading.outline_material(pal))
    for msg in outline.hull_color_report(obj, pal, OUTLINE_CLASS):
        print(f"[ink] WARNING {TOTEM}: {msg}")
    obj["af_outline_class"] = OUTLINE_CLASS
    return obj


def build_all():
    scene.reset()
    pal = Palette(GB.FAMILY)
    kit = ShamanKit(ASSET, PREFIX, WEB, LOOK)
    body = build_body(pal, kit)
    totem = build_totem(pal)
    return pal, kit, body, totem


def build_review_totem(pal: Palette):
    """(totem without the flame, the flame) for the ink-gated previews; the flame is parented to the totem."""
    bare = build_totem(pal, fire=False)
    fire = build_fire(pal)
    fire.parent = bare
    fire.matrix_parent_inverse = Matrix.Identity(4)
    fire.matrix_basis = Matrix.Identity(4)
    return bare, fire


# ── poses ──────────────────────────────────────────────────────────────────────────────────────────────
def u3(A, lat: float, fwd: float, up: float) -> Vector:
    return V(lat * A.U, fwd * A.U, up * A.U)


def ready(A: GoblinAnims) -> BP:
    """Web READY (GoblinShaman.ts :480-490): hunched (lean .30), the totem held far forward and upright (9°)."""
    wd, wu = C.blade(-6.0, lat=-0.2)
    return BP(
        pelvis=A.web_pelvis(46.0, 73.5), hip_yaw=-6.0, twist=6.0, head_pitch=-6.0,
        foot_r=A.web_foot(52.5, -0.07), foot_l=A.web_foot(42.0, 0.075),
        foot_r_rot=(0.0, 0.0, 12.0), foot_l_rot=(0.0, 0.0, 22.0), knee_r=V(-0.45, 1, 0), knee_l=V(0.45, 1, 0),
        # 3D: the staff stands upright at the right side, just ahead — the web's far-forward grip put the horned
        # skull and its fire over the mask from every front game view (projected ≥ 40 px clear here)
        hand_r=u3(A, -12.0, 4.0, 23.0), elbow_r=V(-0.9, 0.2, -0.4), wpn=wd, wpn_up=wu,
        hand_l=u3(A, 4.5, 6.0, 21.0), elbow_l=V(0.9, 0.4, -0.2), wrist_l=(10.0, 0.0, 0.0), flow=0.1)


def install_poses(A: GoblinAnims) -> None:
    """Web ATTACK (:510-515, cast style): the totem drawn back past the shoulder and raised (−20°), swept up
    (26°), then thrust at the target with both hands (52°) — the spirit-fire bolt leaves the tip at 250 ms."""
    def attack_keys():
        R0 = A.ready()
        fn = WEB_READY["footN"]
        wind = R0.but(pelvis=A.web_pelvis(44.5, 74.0), lean=-11.5, head_pitch=-5.0, twist=R0.twist - 18.0,
                      hip_yaw=-10.0, hand_r=u3(A, -14.0, -1.0, 32.0), elbow_r=V(-0.9, 0.1, -0.4),
                      **A._wkw(R0, -20.0, lat=-0.35), hand_l=u3(A, 6.5, 8.0, 27.0), elbow_l=V(0.9, 0.2, -0.3),
                      clav_r=(12.0, -4.0), fx=0.8, jaw=10.0)
        mid = R0.but(pelvis=A.web_pelvis(46.5, 74.0), lean=-1.2, head_pitch=-5.0, twist=R0.twist - 4.0,
                     hand_r=u3(A, -12.5, 9.0, 33.0), elbow_r=V(-0.9, 0.1, -0.4), **A._wkw(R0, 26.0, lat=-0.25),
                     hand_l=u3(A, 6.0, 8.0, 27.0), clav_r=(12.0, 0.0), fx=0.9, jaw=14.0)
        hit = R0.but(pelvis=A.web_pelvis(48.5, 74.5), lean=16.0, head_pitch=-14.0, twist=R0.twist + 8.0,
                     hip_yaw=4.0, hand_r=u3(A, -5.0, 19.0, 27.0), elbow_r=V(-0.8, 0.2, -0.6),
                     **A._wkw(R0, 52.0, lat=-0.05), hand_l=u3(A, 3.5, 17.0, 25.0), elbow_l=V(0.8, 0.2, -0.6),
                     foot_r=A.web_foot(fn + 3.5, R0.foot_r.x), clav_r=(4.0, 8.0), clav_l=(4.0, 8.0), fx=1.0,
                     jaw=20.0, flow=0.3)
        return [A.look(wind), A.look(mid), A.look(hit)]

    def follow(hit):
        return hit.but(hand_r=hit.hand_r + u3(A, 0.0, 0.8, 0.3), jaw=16.0)

    def recover():
        R0 = A.ready()
        return R0.but(pelvis=R0.pelvis + u3(A, 0.0, 1.0, -0.4), lean=6.0, hand_r=u3(A, -11.0, 9.0, 24.0),
                      **A._wkw(R0, 12.0, lat=-0.15), hand_l=u3(A, 4.5, 9.0, 22.0), jaw=8.0)

    def recoil():
        R0 = A.ready()
        rx, ry = WEB_READY["root"]
        return R0.but(pelvis=A.web_pelvis(rx - 3.5, ry + 1.0), lean=-19.5, head_pitch=-20.0, head_yaw=8.0,
                      twist=R0.twist - 6.0, hand_r=u3(A, -11.0, 4.0, 27.0), **A._wkw(R0, -8.0, lat=0.1),
                      hand_l=u3(A, 5.5, 3.0, 25.0), elbow_l=V(0.9, 0.3, -0.3), foot_l=R0.foot_l + V(0, -1.5 * A.U, 0),
                      clav_l=(10.0, 0.0), clav_r=(10.0, 0.0), flow=0.45, jaw=16.0)

    def death_keys():
        R0 = A.ready()
        rec = recoil()
        rx, ry = WEB_READY["root"]
        fn, ff = WEB_READY["footN"], WEB_READY["footF"]
        P0 = A.J["pelvis"]
        buckle = rec.but(pelvis=A.web_pelvis(rx - 2.5, ry + 7.0), lean=-10.0, head_pitch=-16.0, head_yaw=10.0,
                         head_roll=6.0, foot_r=A.web_foot(fn - 1.0, R0.foot_r.x), foot_l=A.web_foot(ff, R0.foot_l.x),
                         knee_l=V(0.6, 1, 0), knee_r=V(-0.6, 1, 0), hand_r=u3(A, -11.5, 8.0, 16.0),
                         elbow_r=V(-0.9, 0.3, -0.2), **A._wkw(R0, 60.0, lat=-0.08), hand_l=u3(A, 6.5, 3.0, 15.0),
                         flow=0.5, jaw=22.0)
        Pt = P0 + A.web_pelvis(rx + 1.5, 91.0 - 10.0)
        tip = buckle.but(pelvis=A.web_pelvis(rx + 1.5, 91.0 - 10.0), lean=-6.0, head_pitch=-14.0,
                         foot_r=Pt + u3(A, -3.0, -5.0, -11.0), foot_l=Pt + u3(A, 3.0, -1.0, -12.5),
                         hand_r=Pt + u3(A, -13.5, 0.0, 9.0), elbow_r=V(-0.9, 0.2, -0.3),
                         wpn=V(-0.25, 0.45, 0.86), wpn_up=V(0, -1, 0), hand_l=Pt + u3(A, 8.5, 1.0, 11.0),
                         spin=-54.0, spin_axis=V(1, 0, 0), flow=0.6, jaw=26.0)
        Pl = P0 + A.web_pelvis(rx + 2.0, 91.0 - 4.8)
        lying = R0.but(pelvis=A.web_pelvis(rx + 2.0, 91.0 - 4.8), lean=-12.0, head_pitch=-10.0, head_roll=-24.0,
                       head_yaw=10.0, twist=-4.0, hip_yaw=0.0,
                       foot_r=Pl + u3(A, -4.0, -7.0, -14.0), foot_l=Pl + u3(A, 5.5, -2.5, -14.0),
                       foot_r_rot=(30.0, 0, 25), foot_l_rot=(18.0, 0, 40), knee_l=V(0.8, 0.6, 0), knee_r=V(-0.4, 1, 0),
                       hand_r=Pl + u3(A, -13.0, -2.5, 8.0), elbow_r=V(-0.4, 0.2, -1.0),
                       wpn=V(-0.3, 0.05, 0.95), wpn_up=V(0, -1, 0),
                       hand_l=Pl + u3(A, 11.0, -1.5, 7.5), elbow_l=V(0.6, 0.2, -1.0),
                       spin=-87.0, spin_axis=V(1, 0, 0), flow=0.05, jaw=18.0)
        return [rec, buckle, tip, lying]

    A.overrides.update(attack_keys=attack_keys, follow=follow, recover=recover, recoil=recoil,
                       death_keys=death_keys)


def shaman_cloth(p, bp: BP, A: GoblinAnims) -> None:
    """Feather fan (headdress_*) swaying with the flow (web sway sin(t + .5)·.06 − flow·.25 rad), and the totem
    feathers hanging in the world from the lash (absolute aims)."""
    sway = math.degrees(math.sin(bp.wave + 0.5) * 0.06 - bp.flow * 0.25)
    for i in range(4):
        p.rot(f"headdress_{i + 1:02d}", pitch=sway * (1 + i * 0.2), roll=2.0 * math.sin(bp.wave * 2 + i))
    for k, nm in enumerate(("totem_feather_01", "totem_feather_02")):
        a = math.radians(8.0 + 10.0 * math.sin(bp.wave + 1 + k) + 25.0 * bp.flow)
        lat = math.radians(-6.0 + 10 * k)
        d = Vector((math.sin(lat), math.sin(a), -math.cos(a))).normalized()
        p.aim(nm, d, Vector((1, 0, 0)))


def anims(kit) -> GoblinAnims:
    A = GoblinAnims(kit, REF_SPEED, WALK, ready, extra_cloth=shaman_cloth, style="cast", contact_wpn=52.0,
                    web_ready=WEB_READY)
    install_poses(A)
    return A


TAUNT_LAT, TAUNT_FWD, TAUNT_TILT = 14.5, -1.0, -0.42      # raised totem: at the right side, a little back, leaning out


def taunt(A: GoblinAnims):
    """Taunt 900 ms (NEW, pre-fight dialogue): the totem thrust high overhead and rattled, the free fist shaken,
    jaw open in a cackle; back to READY."""
    R0 = A.ready()
    # the totem goes up beside the head (outside the face in se / front / sw, like the warrior's raised hands)
    up = R0.but(pelvis=R0.pelvis + u3(A, 0, 0.5, 0.8), lean=-8.0, head_pitch=-16.0, twist=R0.twist - 6.0,
                hand_r=u3(A, -TAUNT_LAT, TAUNT_FWD, 39.0), elbow_r=V(-0.9, 0.1, -0.3),
                **A._wkw(R0, -10.0, lat=TAUNT_TILT),
                hand_l=u3(A, 8.0, 7.0, 31.0), elbow_l=V(0.9, 0.0, -0.4), clav_r=(16.0, 0.0), clav_l=(10.0, 0.0),
                jaw=26.0, flow=0.25)
    keys = [K(0.0, R0), K(200 / 900, up, "out"), K(700 / 900, up.but(jaw=22.0)), K(1.0, R0, "smooth")]

    def layer(bp, t):
        bp = A.waver(900.0)(bp, t)
        if 200 <= t <= 700:                              # rattle the totem + shake the fist
            k = math.sin(math.pi * (t - 200) / 500)
            r = math.sin(t * 0.075)
            d, u_ = C.blade(-10.0 + 9.0 * r * k, lat=TAUNT_TILT)
            bp = bp.but(wpn=d, wpn_up=u_, hand_l=bp.hand_l + V(0, 0, 0.012 * k * math.sin(t * 0.06)),
                        jaw=bp.jaw + 6.0 * k * max(0.0, math.sin(t * 0.05)))
        return bp
    c = A.clip("Taunt", 900.0, keys, 900.0, notifies={"FX_Taunt": 300.0}, layer=layer)
    c.key_poses_ms = [0.0, 200.0, 450.0, 700.0, 900.0]
    c.manifest_extra = {"usage": "pre-fight dialogue of miniboss_goblin_shaman (monsters-ai §8.3)"}
    return c


def clips(A: GoblinAnims) -> list:
    att = A.attack(notifies={"FX_SpiritBolt": CONTACT_MS, "Release": CONTACT_MS})
    att.manifest_extra = {"ranged": True, "projectile": "spirit-fire bolt from the totem's spirit_fire socket (M5)",
                          "attackRange": 3.0}
    return [A.idle(), A.walk(), att, A.hurt(), A.death(), taunt(A),
            A.stun(droop=100.0, hand_r=u3(A, -12.0, 6.0, 17.5), wlat=-0.12)]


GRIP_PARTS = ("fist_r", "knuckle_r", "thumb_r", "claw_r", "totem_feather_red", "totem_feather_teal", "totem_cord")


def totem_grip(co) -> bool:
    return -2.6 * WEB.U < co.y < 2.6 * WEB.U


def spirit_glows(rig, totem):
    """Runtime cards (manifest ``fx.spiritFire`` / ``maskEmbers``): the green glow round the totem's fire (web glow
    6 u α .35 + hot 2 u α .6), and the two mask-eye embers (web 2 u α .45)."""
    def fn(cam, res):
        from kit import review
        out = []
        sp = totem.matrix_world @ Vector((0.0, (STAFF_UP + 6.5) * WEB.U, 0.0))
        wpp = outline.pixel_world_size(cam, sp, res[0])
        px, py = review.project_px(cam, sp, res)
        out.append(dict(center_px=(px, py), radius_px=(6.0 * WEB.U) / wpp, color=SPIRIT, alpha=0.35,
                        core=SPIRIT_HOT, core_frac=0.33, core_alpha=0.6))
        return out
    return fn


FX = {"spiritFire": {"socket": "spirit_fire", "item": TOTEM, "color": SPIRIT, "coreColor": SPIRIT_HOT,
                     "radiusCm": 12.6, "radiusPerFxCm": 14.7, "alpha": 0.35, "alphaPerFx": 0.35,
                     "coreRadiusCm": 4.2, "coreAlpha": 0.6,
                     "flameSizeCm": [9.5, 19.0], "flickerHz": 3.0, "deathFade": "1 - 1.4 t",
                     "note": "the mesh carries a steady emissive flame core; the runtime adds the flickering tongues "
                             "and the additive glow (web spiritFlame + glow)"},
      "maskEmbers": {"sockets": ["fx_eye_l", "fx_eye_r"], "color": SPIRIT, "radiusCm": 4.3, "alpha": 0.45,
                     "alphaPerFx": 0.3},
      "spiritStone": {"color": SPIRIT, "pulsesPerIdle": 2, "radiusCm": 5.0, "alpha": 0.3},
      "castRuneRing": {"color": "#7DFFA0", "radiusCm": [240, 310], "fromMs": 83, "toMs": 250},
      "spiritBolt": {"notify": "FX_SpiritBolt", "atMs": 250, "socket": "spirit_fire", "color": SPIRIT,
                     "coreColor": SPIRIT_HOT, "flashRadiusCm": [15.0, 6.5]},
      "telegraph": {"normal": "#FFC4B0", "elite": "#FF7A5C", "untilMs": 155},
      "eliteCrown": {"socket": "fx_overhead", "asset": "SM_FX_EliteCrown"}}


def quick(out: Path) -> None:
    import mship
    pal, kit, body, totem = build_all()
    A = anims(kit)
    mship.quick_review(ASSET, kit.rig, [body], [(totem, "weapon_r")], A, out, HEIGHT, OUTLINE_CLASS,
                       blob_radius=0.24, glows=spirit_glows(kit.rig, totem))


def anim_quick(out: Path, names, facing: str = "se") -> None:
    import mship
    pal, kit, body, totem = build_all()
    A = anims(kit)
    R.attach_to_bone(totem, kit.rig, "weapon_r")
    A.bind_meshes(body, {"weapon_r": totem})
    cs = [c for c in clips(A) if not names or c.name in names]
    mship.anim_review(kit.rig, [body], [(totem, "weapon_r")], cs, TOKEN, out, HEIGHT, OUTLINE_CLASS,
                      blob_radius=0.24, facing=facing)


def ship(previews: bool = True, verify: bool = True) -> int:
    import mship
    pal, kit, body, totem = build_all()
    body1 = build_body(pal, kit, lod=1)
    A = anims(kit)
    R.attach_to_bone(totem, kit.rig, "weapon_r")
    A.bind_meshes(body, {"weapon_r": totem})
    cs = clips(A)
    U = WEB.U
    bare, fire = build_review_totem(pal)
    items = [mship.Item(totem, TOTEM, "weapon_r", "staff", preview_obj=bare, fx_objs=[fire],
                        sockets=[{"name": "spirit_fire", "pos": (0.0, (STAFF_UP + 4.8) * U, 0.0)},
                                 {"name": "tip", "pos": (0.0, (STAFF_UP + 10.0) * U, 0.0)},
                                 {"name": "mid", "pos": (0.0, 8.0 * U, 0.0)}],
                        extra={"lengthCm": round((STAFF_UP + STAFF_DOWN + 11.0) * U * 100.0, 1)}, grip=totem_grip)]
    ms = mship.MonsterShip(
        asset=ASSET, token=TOKEN, preview_dir="mon_goblin_shaman", pal=pal, A=A, body=body, items=items, clips=cs,
        outline_class=OUTLINE_CLASS, game_ids=["monster_goblin_shaman", "miniboss_goblin_shaman"],
        blob_radius=round(11 * U, 3), height=HEIGHT, display={"zh-CN": "哥布林萨满", "en": "Goblin Shaman"},
        lod1=body1,
        fx=FX, glows=spirit_glows(kit.rig, bare),
        qa_groups={"body_nf": mship.parts_except(body, GRIP_PARTS)},
        qa_pairs={"totemBody": ("item:" + TOTEM, "body_nf")},
        qa_limits={"ground": -0.5, "totemBody": 0},
        game_keys=[("Idle", 0, "se"), ("Walk", 9, "se"), ("Attack01", 5, "se"), ("Attack01", 10, "se"),
                   ("Attack01", 15, "se"), ("Attack01", 15, "sw"), ("Taunt", 24, "se"), ("Death", 30, "se")],
        close_keys=[("Attack01", 5, "se"), ("Attack01", 10, "se"), ("Attack01", 15, "se"), ("Attack01", 15, "side"),
                    ("Taunt", 24, "front"), ("Hurt", 0, "se"), ("Death", 20, "se"), ("Death", 30, "se"),
                    ("Stun", 10, "front"), ("Walk", 9, "side")],
        back_clips=("Attack01", "Death", "Taunt"),
        skeleton_bones_required=("root", "pelvis", "head", "jaw", "weapon_r", "weapon_l", "ear_l_02", "loin_f_02",
                                 "pelt_03", "headdress_04", "totem_feather_02", "ik_foot_l", "ik_hand_gun"),
        extra={"refSpeedCmS": REF_SPEED * 100.0, "attackStyle": "cast", "ranged": True, "elite": True,
               "notes": "M5: ranged caster (attackRange 3.0) — Attack01 releases the spirit-fire bolt from the totem "
                        "(spirit_fire socket on SM_Mon_GoblinShaman_Totem) at 250 ms. Death: the spirit-fire dies "
                        "out (runtime 1 - 1.4 t)."},
    )
    res = mship.ship(ms, previews=previews, verify=verify)
    return mship.main_result(res)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    scratch = Path("/tmp/claude-0/-home-user-abyssfire/b3225f99-25a3-5b78-996f-643be8685889/scratchpad/mq/shaman")
    if "--quick" in args:
        quick(scratch)
    elif "--anims" in args:
        names = [a for a in args[args.index("--anims") + 1:] if not a.startswith("--")]
        fac = next((a.split("=")[1] for a in args if a.startswith("--facing=")), "se")
        anim_quick(scratch / "anims", names, fac)
    else:
        sys.exit(ship(previews="--no-previews" not in args, verify="--no-verify" not in args))

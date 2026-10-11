"""哥布林首领 / 碎牙·格罗克 Grokk Brokentooth — ``SK_Mon_GoblinChief`` (spec art-inventory-ch1.md §4.3, §10; R2).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/goblin_chief.py
        [--quick] [--anims Clip … --facing=se] [--no-previews] [--no-verify]

A bulkier goblin warlord (helm crown 1.17 m, horn tips ≈ 1.38 m): horned iron helm with a dark rim band, rivets and
a nose guard; red war paint on the cheeks; lower tusks — the right one snapped off (碎牙); a wolf-pelt mantle over
the shoulders with dark fur strokes and a pelt tail down the back (``pelt_*`` chain); a skull pauldron on the weapon
shoulder; a studded leather cuirass, a strap belt with a trophy-skull buckle, loincloth flaps, leather bracers and
iron knee guards; ember-lit eyes (emissive + the ``eyeEmbers`` glow cards). Weapon: the rusted cleaver
``SM_Mon_GoblinChief_Cleaver`` (broad chopping blade, spine back / edge leading, honed edge, rust, a hole near the
tip, leather grip, iron pommel) on ``weapon_r``. Clips: Idle 667, Walk 600 (1.50 m/s, no slide), Attack01 overhead
chop 500 (WindupEnd 155, Contact 250), Hurt 200, Death 500, Roar 1200 (boss intro, FX_Roar 450), Stun 600.

Web source: ``src/graphics/sprites/monsters/GoblinChief.ts`` (palette :37-47, cleaver :134-160, helm :89-110,
war paint :112-127, READY :304-314, walk :326).
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

ASSET = "SK_Mon_GoblinChief"
TOKEN = "Mon_GoblinChief"
CLEAVER = "SM_Mon_GoblinChief_Cleaver"
PREFIX = "chief"
HEIGHT = 1.17
WEB = GB.Web(U=0.0239 * 0.94, thigh=10.5, shin=10.5, upper_arm=9.0, fore_arm=8.5, torso=14.5, neck=6.8, ankle=1.8,
             hip=2.6, shoulder=5.4, lean=0.24, head_tilt=-0.20, root=(46.5, 71.5), bulk=1.15, foot_len=9.2, grin=1.6)
OUTLINE_CLASS = "monster"
REF_SPEED = 1.50
WALK = dict(cycle=600.0, duty=0.36, lift=0.075, bob=0.02, lean=4.0, arm=0.06, crouch=-0.035, twist=5.0,
            hand_swing=0.35, hand_off=V(-0.02, 0.0, 0.01))
WEB_READY = dict(root=(46.5, 71.5), footN=54.0, footF=41.5, lean=0.24, head=-0.20)

IRON = Region("#7F8694", .42, .45)
LEATHER = Region("#5E3A22", .42, .30)
FUR = Region("#7A6E62", .42, .35)
BONE = Region("#ECE2C6", .30, .40)
LOOK = GB.Look(
    skin="#65913A", skin_l=0.32, skin_dark="#42632A", skin_dark_l=0.18, ear_in="#33501F", cloth="#6A3B24",
    strap="#3E2616", eye="#FFB13A", eye_e=0.6,
    extra=dict(
        iron=IRON, iron_dark=Region("#4D5260", .42, .32), iron_light=IRON.light_region(.30, .30),
        horn=Region("#E6DAB8", .30, .40), horn_band=Region("#BCAE8C", .30, .30),
        fur=FUR, fur_dark=Region("#4F463E", .42, .25), leather=LEATHER, leather_dark=LEATHER.shade_region(.30, .12),
        socket=Region("#2A1A12", .30, .10), paint=Region("#BE1E1E", .35, .30),
    ))

CLEAVER_K = 1.15            # "a huge rusted cleaver": the web blade scaled up a little for the 3D read
CLEAVER_REGIONS = outline.ink_regions(dict(
    # blade steel with a light shade band (the warrior's blade lesson: the shaded flat stays mid-grey steel,
    # never a dark iron slab)
    iron=Region("#8A919E", .16, .42), iron_dark=Region("#4D5260", .42, .32), edge=Region("#F0F4FA", .10, .30),
    rust=Region("#9C5A2C", .42, .25), hole=Region("#2A2A30", .30, .10), leather=LEATHER,
    leather_line=LEATHER.shade_region(.30, .12),
), OUTLINE_CLASS)


class ChiefKit(GB.GoblinKit):
    """The goblin body kit with the chief's ears (drooping a little under the helm rim) and tusks."""

    def ear_mid(self, sg: int) -> Vector:
        return self.hp(2.9, -1.7, sg * 9.8)

    def ear_tip(self, sg: int) -> Vector:
        return self.hp(4.4, -3.1, sg * 14.2)

    def add_teeth(self, b, jbind, skull_id) -> None:
        """Two lower tusks jutting up past the lip — the right one snapped off (碎牙) — and two upper teeth."""
        hk = self.hk
        up = self.hdir(1, 0)
        for a, broken in ((24.0, False), (-24.0, True)):
            base = self.head_point(-3.2, a, -hk(0.2))
            n = self.head_normal(-2.6, a)
            L = hk(1.4 if broken else 2.5)
            tip = base + up * L + n * hk(0.55)
            tusk = G.cone(base, tip, hk(0.62), sides=6, r_tip=hk(0.34) if broken else hk(0.06))
            b.add(tusk, "bone", jbind, name="tusk", outline=False, covered_by=skull_id)
        for a in (-9.0, 9.0):
            top = self.head_point(-2.15, a, hk(0.2))
            n = self.head_normal(-2.15, a)
            tooth = G.cone(top + up * hk(0.15), top - up * hk(1.0) + n * hk(0.15), hk(0.4), sides=5, r_tip=hk(0.08))
            b.add(tooth, "bone", jbind, name="tooth", outline=False, covered_by=skull_id)


class HelmSurf:
    """The helm dome surface (skull profile pushed out by ``off``), web head units for ``h``."""

    def __init__(self, kit: GB.GoblinKit, off: float):
        self.k, self.off = kit, off

    def point(self, h, a, out=0.0):
        return self.k.head_point(h, a, self.off + out)

    def normal(self, h, a):
        return self.k.head_normal(h, a)


def strip_along(pts, lat_dir: Vector, normal_fn, width: float, thick: float) -> M.Part:
    """Flat plate strip along ``pts`` lying on a surface with normals ``normal_fn(i)``."""
    outer, inner = [], []
    for i, p in enumerate(pts):
        nrm = normal_fn(i)
        w = width[i] if isinstance(width, (list, tuple)) else width
        sd = (lat_dir - nrm * lat_dir.dot(nrm)).normalized() * (w / 2)
        outer.append([p - sd + nrm * thick, p + sd + nrm * thick])
        inner.append([p - sd - nrm * 0.002, p + sd - nrm * 0.002])
    return C.thick_patch(outer, inner).auto_sharp(50)


# ── body ───────────────────────────────────────────────────────────────────────────────────────────────
def build_body(pal: Palette, kit: ChiefKit) -> bpy.types.Object:
    from kit import asset
    b = M.Builder(ASSET, pal, prefix=PREFIX)
    b.regions(**GB.regions_for(LOOK))
    u, hk = kit.u, kit.hk
    J = kit.J
    ids = kit.add_head(b)
    skull = ids["skull"]
    t = kit.torso
    tid = kit.add_torso(b, necklace=False, sash=False, belt=False)["torso"]
    kit.add_loincloth(b, w_top=6.2, w_bot=5.2, length=8.8)
    for s, sg in (("l", 1), ("r", -1)):
        kit.add_arm(b, s, sg, r_scale=1.15)
        kit.add_leg(b, s, sg, r_scale=1.12)
    head = Bind.rigid("head")
    # ── horned iron helm ────────────────────────────────────────────────────────────────────────────────
    hs = HelmSurf(kit, hk(0.62))
    helm = G.grid_loft(lambda h, a: hs.point(h, a), list(np.linspace(3.7, 7.02, 10)), G.angles(28), cap0="fan",
                       cap1="fan", dome1=hk(0.05))
    hid = b.add(helm, "iron", head, name="helm", hull_pad=True)
    band = G.surface_ring(hs, lambda a: 4.35 - 0.25 * math.cos(math.radians(a)), hk(1.55), hk(0.45), segs=40)
    b.add(band, "iron_dark", head, name="helm_band", outline=False, covered_by=hid)
    for k in range(10):
        a = -180.0 + 36.0 * k + 18.0
        h = 4.35 - 0.25 * math.cos(math.radians(a))
        b.add(G.surface_dot(hs, h, a, hk(0.42), hk(0.75)), "iron_light", head, name="helm_rivet", outline=False,
              covered_by=hid)
    # crest ridge front to back over the dome (one darker welded seam)
    ridge = [hs.point(h, 0.0, hk(0.05)) for h in np.linspace(4.9, 7.0, 6)] + \
            [hs.point(h, 180.0, hk(0.05)) for h in np.linspace(7.0, 4.9, 6)][1:]
    rn = [hs.normal(h, 0.0) for h in np.linspace(4.9, 7.0, 6)] + [hs.normal(h, 180.0) for h in np.linspace(7.0, 4.9, 6)][1:]
    b.add(strip_along(ridge, Vector((1, 0, 0)), lambda i: rn[i], hk(0.9), hk(0.35)), "iron_dark", head,
          name="helm_ridge", outline=False, covered_by=hid)
    # nose guard: a tapering plate over the nose bridge from the rim
    ng = [kit.hp(4.7, 5.7), kit.hp(3.9, 7.0), kit.hp(3.0, 8.5), kit.hp(2.35, 9.5)]
    nn = []
    for i in range(len(ng)):
        tg = (ng[min(i + 1, 3)] - ng[max(i - 1, 0)]).normalized()
        nn.append(kit.hdir(0, 0, 1).cross(tg).normalized() * -1.0)
    nn = [n if n.dot(kit.hdir(1, 0.6)) > 0 else -n for n in nn]
    b.add(strip_along(ng, kit.hdir(0, 0, 1), lambda i: nn[i], [hk(1.7), hk(1.5), hk(1.25), hk(1.0)], hk(0.45)),
          "iron_dark", head, name="nose_guard", hull_pad=True)
    # horns: bull horns sweeping out and up from the helm sides, ridged near the base
    for sg in (1, -1):
        ctl = [(5.0, -0.5, 4.2), (6.4, -0.2, 6.0), (8.3, -0.4, 8.1), (10.6, -1.2, 9.7), (12.9, -2.1, 10.3)]
        pts = [kit.hp(h, f, sg * l) for h, f, l in ctl]
        dense = []
        for i in range(len(pts) - 1):
            for k in range(3):
                dense.append(pts[i].lerp(pts[i + 1], k / 3))
        dense.append(pts[-1])
        rad = [hk(1.55 * (1 - 0.92 * (i / (len(dense) - 1)) ** 1.25) + 0.05) for i in range(len(dense))]
        horn = M.tube(dense, rad, sides=12, dome0=0.0, dome1=hk(0.1))
        hrn = b.add(horn, "horn", head, name="horn", hull_pad=True)
        for k, f in enumerate((0.22, 0.34, 0.46)):
            i = int(f * (len(dense) - 1))
            tg = (dense[i + 1] - dense[i - 1]).normalized()
            ring = M.tube([dense[i] - tg * hk(0.28), dense[i] + tg * hk(0.28)], [rad[i] + hk(0.12)] * 2, sides=12,
                          dome0=0.0, dome1=0.0)
            b.add(ring, "horn_band", head, name="horn_ridge", outline=False, covered_by=hrn)
    # war paint: two red stripes on each cheek
    hsurf = GB._HeadSurf(kit)
    for sg in (1, -1):
        for h0, h1 in ((1.45, 1.05), (0.25, -0.15)):
            st = G.surface_strap(hsurf, [(h0, sg * 24.0), (h1, sg * 44.0)], hk(0.8), hk(0.14), steps=6)
            b.add(st, "paint", head, name="war_paint", outline=False, covered_by=skull)
    # ── studded leather cuirass ─────────────────────────────────────────────────────────────────────────
    spine = Bind.blend("pelvis", "spine_01", "spine_02", "spine_03", falloff=3.0, smooth=0.02)
    ss = list(np.linspace(0.06, 0.80, 9))
    angs = G.angles(28)
    outer = [[t.point(s_, a, u(0.62)) for a in angs] for s_ in ss]
    inner = [[t.point(s_, a, u(0.06)) for a in angs] for s_ in ss]
    cu = C.thick_patch(outer, inner, wrap_u=True)
    cid = b.add(cu, "leather", spine, name="cuirass", hull_pad=True)
    for s_ in (0.27, 0.43, 0.59):
        for a in (-44.0, -22.0, 0.0, 22.0, 44.0, 136.0, 158.0, 180.0, -158.0, -136.0):
            b.add(G.surface_dot(_Off(t, u(0.62)), s_, a, u(0.62), u(0.55)), "iron_light", spine, name="stud",
                  outline=False, covered_by=cid)
    # lacing seam down each side
    for a in (90.0, -90.0):
        st = G.surface_strap(_Off(t, u(0.62)), [(0.10, a), (0.76, a)], u(0.7), u(0.12), steps=10)
        b.add(st, "leather_dark", spine, name="cuirass_seam", outline=False, covered_by=cid)
    # strap belt with a trophy-skull buckle
    bid = kit.add_rope_belt(b, cid, s0=0.07, region="strap", stroke=None, width_u=2.1, thick_u=0.95)
    bc = t.point(0.07, 14.0, u(1.3))
    bn = t.normal(0.07, 14.0)
    m = G.frame_z(bc, bn, Vector((0, 0, 1)))
    sk = M.ellipsoid((u(1.15), u(1.0), u(0.7)), (0, 0, 0), sides=12, rings=8).transform(m)
    sid = b.add(sk, "bone", Bind.blend("pelvis", "spine_01", falloff=3.0), name="buckle_skull", hull_pad=True)
    for x in (-0.42, 0.42):
        e = M.ellipsoid((u(0.32), u(0.36), u(0.2)), (0, 0, 0), sides=8, rings=5).transform(
            m @ Matrix.Translation((x * u(1.0), u(0.15), u(0.62))))
        b.add(e, "socket", Bind.blend("pelvis", "spine_01", falloff=3.0), name="buckle_eye", outline=False,
              covered_by=sid)
    # ── wolf-pelt mantle over the shoulders ─────────────────────────────────────────────────────────────
    mant = G.SpineLoft(kit.spine_points(), [(0.60, u(8.6), u(8.4), u(-0.4)), (0.74, u(9.0), u(8.6), u(-0.5)),
                                            (0.88, u(9.0), u(8.3), u(-0.5)), (0.98, u(7.6), u(7.0), u(-0.3)),
                                            (1.06, u(5.0), u(5.3), u(-0.1)), (1.12, u(3.6), u(4.2), u(0.1))],
                       exp=2.3)
    ms_ = list(np.linspace(0.62, 1.12, 9))
    angs = G.angles(32)
    hem = {a: 0.62 - (0.035 if (i % 2) else 0.0) - 0.02 * math.cos(math.radians(a)) for i, a in enumerate(angs)}
    outer, inner = [], []
    for r, s_ in enumerate(ms_):
        ro, ri = [], []
        for a in angs:
            s2 = hem[a] if r == 0 else s_
            ro.append(mant.point(s2, a, u(0.55)))
            ri.append(mant.point(s2, a, -u(0.35)))
        outer.append(ro)
        inner.append(ri)
    mantle = C.thick_patch(outer, inner, wrap_u=True)
    mbind = kit.blend4("spine_03", "neck_01", "clavicle_l", "clavicle_r", "upperarm_l", "upperarm_r", falloff=3.0,
                       smooth=0.03)
    mid_ = b.add(mantle, "fur", mbind, name="mantle", hull_pad=True)
    for i in range(16):
        a = -180.0 + 22.5 * i + 7.0
        st = G.surface_strap(_Off(mant, u(0.55)), [(0.86, a), (0.74, a + 4.0), (0.66, a + 2.0)], u(0.75), u(0.14),
                             steps=6)
        b.add(st, "fur_dark", mbind, name="fur_stroke", outline=False, covered_by=mid_)
    # pelt tail down the back on the pelt chain
    bones = kit.rig.obj.data.bones
    chain = [bones["pelt_01"].head_local, bones["pelt_02"].head_local, bones["pelt_03"].head_local,
             bones["pelt_03"].tail_local]
    tail = G.chain_strip(chain, [u(6.0), u(5.0), u(3.8), u(2.4)], Vector((1, 0, 0)), Vector((0, 1, 0.15)),
                         u(0.9), ragged=u(0.4), tails=3, seed=11)
    tb = GB.chain_bind(chain[0], chain[-1], ("spine_03", "pelt_01", "pelt_02", "pelt_03"))
    b.add(tail, "fur", tb, name="pelt_tail", sub_regions={1: "fur", 2: "fur_dark"}, hull_pad=True)
    # skull pauldron on the weapon (right) shoulder, over the mantle
    S = J["shoulder_r"]
    c = S + V(-u(1.6), u(0.6), u(3.3))
    out = (V(-1.0, 0.45, 0.35)).normalized()
    m = G.frame_z(c, out, Vector((0, 0, 1)))
    pb = Bind.blend("clavicle_r", "upperarm_r", falloff=2.0)
    cran = M.ellipsoid((u(2.3), u(2.5), u(2.0)), (0, 0, 0), sides=14, rings=8).transform(m)
    pid = b.add(cran, "bone", pb, name="pauldron_skull", hull_pad=True)
    for x in (-0.9, 0.9):
        e = M.ellipsoid((u(0.62), u(0.72), u(0.35)), (0, 0, 0), sides=8, rings=5).transform(
            m @ Matrix.Translation((x * u(1.0), u(0.25), u(1.72))))
        b.add(e, "socket", pb, name="pauldron_eye", outline=False, covered_by=pid)
    nose = M.ellipsoid((u(0.3), u(0.45), u(0.25)), (0, 0, 0), sides=6, rings=4).transform(
        m @ Matrix.Translation((0.0, -u(0.9), u(1.82))))
    b.add(nose, "socket", pb, name="pauldron_nose", outline=False, covered_by=pid)
    jaw = M.ellipsoid((u(1.6), u(0.9), u(1.2)), (0, 0, 0), sides=10, rings=6).transform(
        m @ Matrix.Translation((0.0, -u(1.9), u(0.9))))
    b.add(jaw, "bone", pb, name="pauldron_jaw", outline=False, covered_by=pid)
    # leather bracers and iron knee guards
    for s, sg in (("l", 1), ("r", -1)):
        E, W = J[f"elbow_{s}"], J[f"wrist_{s}"]
        br = M.tube([E.lerp(W, 0.2), E.lerp(W, 0.55), E.lerp(W, 0.85)], [u(2.25), u(2.15), u(2.0)], sides=12,
                    dome0=0.0, dome1=0.0)
        brid = b.add(br, "leather", Bind.blend(f"lowerarm_{s}", f"hand_{s}", falloff=4.0), name=f"bracer_{s}",
                     hull_pad=True)
        for f in (0.32, 0.72):
            ring = M.tube([E.lerp(W, f - 0.04), E.lerp(W, f + 0.04)], [u(2.35)] * 2, sides=12, dome0=0.0, dome1=0.0)
            b.add(ring, "leather_dark", Bind.blend(f"lowerarm_{s}", f"hand_{s}", falloff=4.0), name="bracer_lace",
                  outline=False, covered_by=brid)
        Kn = J[f"knee_{s}"]
        kg = M.ellipsoid((u(2.45), u(1.6), u(2.6)), Kn + V(0, u(1.05), u(0.3)), sides=14, rings=8)
        kid = b.add(kg, "iron", Bind.blend(f"thigh_{s}", f"calf_{s}", falloff=4.0), name=f"knee_guard_{s}",
                    hull_pad=True)
        b.add(G.surface_dot(_Ell(Kn + V(0, u(1.05), u(0.3)), (u(2.45), u(1.6), u(2.6))), 0.5, 0.0, u(0.45),
                            u(0.5)), "iron_light", Bind.blend(f"thigh_{s}", f"calf_{s}", falloff=4.0),
              name="knee_rivet", outline=False, covered_by=kid)
    body = asset.finish_mesh(b, pal, OUTLINE_CLASS, rig=kit.rig, grounding_height=HEIGHT)
    return body


class _Off:
    """A SpineLoft surface pushed out by ``off`` (for trims on a shell)."""

    def __init__(self, surf, off):
        self.s, self.off = surf, off

    def point(self, u, a, out=0.0):
        return self.s.point(u, a, self.off + out)

    def normal(self, u, a):
        return self.s.normal(u, a)


class _Ell:
    """Front point of an axis-aligned ellipsoid (u unused, a = 0 front) — for one rivet on a knee guard."""

    def __init__(self, c, r):
        self.c, self.r = Vector(c), r

    def point(self, u, a, out=0.0):
        return self.c + V(0, self.r[1] + out, 0)

    def normal(self, u, a):
        return V(0, 1, 0)


# ── cleaver ────────────────────────────────────────────────────────────────────────────────────────────
def build_cleaver(pal: Palette) -> bpy.types.Object:
    """Broad chopping blade (web cleaver :134-160 → ≈ 17 × 33 cm blade, ≈ 50 cm overall) in weapon space: grip
    at the origin, blade along +Y, the **edge on +Z** (the item's up = the leading edge of the chop), flats ±X."""
    u = WEB.U
    b = M.Builder(CLEAVER, pal, prefix="chief_cleaver")
    b.regions(**CLEAVER_REGIONS)
    rig = Bind.rigid("root")
    # blade outline in (z, y) from the web polygon (x → z, −y → y), slightly bellied edge
    web = [(-1.6, 2.6), (1.4, 3.2), (6.4, 6.4), (6.8, 15.4), (-1.2, 16.8), (-2.2, 15.0)]
    pts = []
    for i in range(len(web)):
        a, c = Vector(web[i]), Vector(web[(i + 1) % len(web)])
        n = 4 if i in (2, 3) else 2
        for k in range(n):
            q = a.lerp(c, k / n)
            if i == 2:               # belly of the edge
                q.x += 0.35 * math.sin(math.pi * k / n)
            pts.append((q.x * u, q.y * u))
    pts = C.ccw(pts)
    th = 0.85 * u

    def thick_at(z):        # spine thick, edge thin (wedge)
        k = (z / u + 2.2) / 9.0
        return th * (1.0 - 0.78 * max(0.0, min(1.0, k)))
    # a closed wedge slab: x across the thickness (thick spine, thin edge), outline in the (z, y) plane: x across thickness, outline in the (z, y) plane
    bm = bmesh.new()
    top = [bm.verts.new((thick_at(z) * 0.5, y, z)) for z, y in pts]
    bot = [bm.verts.new((-thick_at(z) * 0.5, y, z)) for z, y in pts]
    cz = sum(z for z, _ in pts) / len(pts)
    cy = sum(y for _, y in pts) / len(pts)
    ct = bm.verts.new((th * 0.5, cy, cz))
    cb = bm.verts.new((-th * 0.5, cy, cz))
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((ct, top[i], top[j]))
        bm.faces.new((cb, bot[j], bot[i]))
        bm.faces.new((top[j], top[i], bot[i], bot[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    blade = M.Part(bm).flat()
    bid = b.add(blade, "iron", rig, name="blade", hull_pad=True)
    # honed edge: a bright strip along the leading edge on both flats
    edge = [(6.4, 6.6), (6.62, 9.5), (6.75, 12.5), (6.8, 15.2)]
    for sx in (1, -1):
        outer, inner = [], []
        for z, y in edge:
            zz = z * u
            x = sx * (thick_at(zz) * 0.5 + 0.0006)
            outer.append([Vector((x, y * u, zz - 1.0 * u)), Vector((x, y * u, zz - 0.05 * u))])
            inner.append([Vector((x - sx * 0.002, y * u, zz - 1.0 * u)), Vector((x - sx * 0.002, y * u, zz - 0.05 * u))])
        b.add(C.thick_patch(outer, inner), "edge", rig, name="edge", outline=False, covered_by=bid)
        # rust blotches + the hole near the spine at the tip
        for (z, y, rz, ry, reg) in ((1.4, 12.2, 1.6, 1.1, "rust"), (3.6, 8.0, 1.1, 0.8, "rust"),
                                     (2.4, 4.6, 0.7, 0.5, "rust"), (0.6, 14.4, 0.9, 0.9, "hole")):
            zz = z * u
            d = M.ellipsoid((0.0016, ry * u, rz * u), (sx * (thick_at(zz) * 0.5), y * u, zz), sides=12, rings=4)
            b.add(d, reg, rig, name=reg, outline=False, covered_by=bid)
    # collar, leather-wrapped grip, iron pommel
    col = M.tube([Vector((0, 2.0 * u, -0.2 * u)), Vector((0, 3.1 * u, -0.2 * u))], [1.25 * u, 1.15 * u], sides=10,
                 dome0=0.0, dome1=0.0)
    b.add(col, "iron_dark", rig, name="collar", hull_pad=True)
    grip = M.tube([Vector((0, -5.0 * u, 0)), Vector((0, -1.5 * u, 0)), Vector((0, 2.1 * u, -0.1 * u))],
                  [0.92 * u, 0.98 * u, 0.95 * u], sides=10, dome0=0.0, dome1=0.0)
    gid = b.add(grip, "leather", rig, name="grip", hull_pad=True)
    for k in range(6):
        y = (-4.4 + k * 1.15) * u
        w = M.tube([Vector((0, y - 0.25 * u, 0)), Vector((0, y + 0.25 * u, 0))], [1.04 * u] * 2, sides=10,
                   dome0=0.0, dome1=0.0)
        b.add(w, "leather_line", rig, name="grip_wrap", outline=False, covered_by=gid)
    pom = M.ellipsoid((1.35 * u, 1.3 * u, 1.35 * u), (0, -5.75 * u, 0), sides=12, rings=8)
    b.add(pom, "iron_dark", rig, name="pommel", hull_pad=True)
    obj = b.build(CLEAVER)
    mt = shading.toon_material(pal)
    outline.bake_hull(obj, outline.WIDTH_CM[OUTLINE_CLASS] / 100.0, mt, shading.outline_material(pal))
    for msg in outline.hull_color_report(obj, pal, OUTLINE_CLASS):
        print(f"[ink] WARNING {CLEAVER}: {msg}")
    obj["af_outline_class"] = OUTLINE_CLASS
    return obj


def build_all():
    scene.reset()
    pal = Palette(GB.FAMILY)
    kit = ChiefKit(ASSET, PREFIX, WEB, LOOK)
    body = build_body(pal, kit)
    cleaver = build_cleaver(pal)
    return pal, kit, body, cleaver


# ── poses ──────────────────────────────────────────────────────────────────────────────────────────────
def u3(A, lat: float, fwd: float, up: float) -> Vector:
    return V(lat * A.U, fwd * A.U, up * A.U)


IDLE_ROLL = -60.0           # grip roll of the carried cleaver (hammer-grip solver: wrist_r[1] adds roll)
STRIKE = (0.0, 0.0, 0.0)    # the chop keys: edge leading, no extra roll


def ready(A: GoblinAnims) -> BP:
    """Web READY (GoblinChief.ts :304-314): lean .24, head −.2, cleaver forward at 52°, held out at the side;
    the blade rolled 60° so its flat faces up toward the key light and the high camera (an edge-on or shaded flat
    read as a dark slab)."""
    wd, wu = C.blade(52.0, lat=-0.05)
    return BP(
        pelvis=A.web_pelvis(46.5, 71.5), hip_yaw=-6.0, twist=4.0, head_pitch=-6.0,
        foot_r=A.web_foot(54.0, -0.085), foot_l=A.web_foot(41.5, 0.09),
        foot_r_rot=(0.0, 0.0, 12.0), foot_l_rot=(0.0, 0.0, 22.0), knee_r=V(-0.5, 1, 0), knee_l=V(0.5, 1, 0),
        hand_r=u3(A, -9.0, 8.0, 24.0), elbow_r=V(-0.9, 0.4, -0.2), wpn=wd, wpn_up=wu, wrist_r=(0.0, IDLE_ROLL, 0.0),
        hand_l=u3(A, 6.0, 6.0, 23.0), elbow_l=V(0.9, 0.4, -0.2), wrist_l=(10.0, 0.0, 0.0), flow=0.1)


def install_poses(A: GoblinAnims) -> None:
    """The overhead chop in 3D: the cleaver is raised at the right side, outside the right horn, comes over in
    front of the helm and chops down-forward (web contactWpn 2.1 rad)."""
    def attack_keys():
        R0 = A.ready()
        rx, ry = WEB_READY["root"]
        fn = WEB_READY["footN"]
        wind = R0.but(wrist_r=STRIKE, pelvis=A.web_pelvis(rx - 2.0, ry + 0.8), lean=-13.7, head_pitch=-2.0, twist=R0.twist - 16.0,
                      hip_yaw=-10.0, hand_r=u3(A, -13.0, -3.0, 40.0), elbow_r=V(-0.9, 0.1, -0.4),
                      **A._wkw(R0, -45.0, lat=-0.35), hand_l=u3(A, 7.0, 9.0, 27.0), elbow_l=V(0.9, 0.3, -0.3),
                      clav_r=(14.0, -4.0), fx=0.4, jaw=10.0)
        mid = R0.but(wrist_r=STRIKE, pelvis=A.web_pelvis(rx + 1.2, ry + 1.0), lean=5.7, twist=R0.twist + 2.0,
                     hand_r=u3(A, -9.5, 9.0, 41.0), elbow_r=V(-0.9, 0.0, -0.3), **A._wkw(R0, 30.0, lat=-0.15),
                     hand_l=u3(A, 7.5, 6.0, 26.0), foot_r=A.web_foot(fn + 3.5, R0.foot_r.x), clav_r=(12.0, 4.0),
                     fx=0.7, jaw=16.0)
        hit = R0.but(wrist_r=STRIKE, pelvis=A.web_pelvis(rx + 3.8, ry + 1.8), lean=21.8, head_pitch=-12.0, twist=R0.twist + 16.0,
                     hip_yaw=6.0, hand_r=u3(A, -7.5, 21.0, 21.0), elbow_r=V(-0.8, 0.2, -0.6),
                     **A._wkw(R0, 120.0, lat=0.05), hand_l=u3(A, 9.0, -2.0, 25.0), elbow_l=V(0.9, 0.3, -0.4),
                     foot_r=A.web_foot(fn + 5.0, R0.foot_r.x), foot_r_rot=(0, 0, 4), clav_r=(-2.0, 8.0), fx=1.0,
                     jaw=24.0, flow=0.3)
        return [A.look(wind), A.look(mid), A.look(hit)]

    def follow(hit):
        return hit.but(hand_r=hit.hand_r + u3(A, 0.0, 0.6, -1.2), **A._wkw(A.ready(), 128.0, lat=0.05), jaw=16.0)

    def recover():
        R0 = A.ready()
        return R0.but(pelvis=R0.pelvis + u3(A, 0.0, 1.5, -0.6), lean=8.0, hand_r=u3(A, -10.0, 13.0, 22.0),
                      **A._wkw(R0, 88.0, lat=-0.05), jaw=8.0)

    def recoil():
        R0 = A.ready()
        rx, ry = WEB_READY["root"]
        return R0.but(pelvis=A.web_pelvis(rx - 3.5, ry + 1.0), lean=-19.5, head_pitch=-20.0, head_yaw=8.0,
                      twist=R0.twist - 6.0, hand_r=u3(A, -10.0, 4.0, 29.0), **A._wkw(R0, 30.0, lat=-0.15),
                      hand_l=u3(A, 7.5, 3.0, 28.0), foot_l=R0.foot_l + V(0, -1.5 * A.U, 0), clav_l=(10.0, 0.0),
                      clav_r=(10.0, 0.0), flow=0.45, jaw=18.0)

    def death_keys():
        R0 = A.ready()
        rec = recoil()
        rx, ry = WEB_READY["root"]
        fn, ff = WEB_READY["footN"], WEB_READY["footF"]
        P0 = A.J["pelvis"]
        buckle = rec.but(pelvis=A.web_pelvis(rx - 2.5, ry + 7.0), lean=-10.0, head_pitch=-16.0, head_yaw=10.0,
                         head_roll=6.0, foot_r=A.web_foot(fn - 1.0, R0.foot_r.x), foot_l=A.web_foot(ff, R0.foot_l.x),
                         knee_l=V(0.6, 1, 0), knee_r=V(-0.6, 1, 0), hand_r=u3(A, -12.0, 4.0, 18.0),
                         elbow_r=V(-0.9, 0.3, -0.2), **A._wkw(R0, 100.0, lat=-0.3), hand_l=u3(A, 8.0, 3.0, 17.0),
                         flow=0.5, jaw=24.0)
        Pt = P0 + A.web_pelvis(rx + 1.5, 91.0 - 10.0)
        tip = buckle.but(pelvis=A.web_pelvis(rx + 1.5, 91.0 - 10.0), lean=-6.0, head_pitch=-14.0,
                         foot_r=Pt + u3(A, -3.0, -5.0, -13.0), foot_l=Pt + u3(A, 3.5, -1.0, -14.5),
                         hand_r=Pt + u3(A, -13.0, 0.0, 10.0), elbow_r=V(-0.9, 0.2, -0.3),
                         wpn=V(-0.5, -0.3, 0.81), wpn_up=V(0, -1, 0), hand_l=Pt + u3(A, 10.0, 1.0, 12.0),
                         spin=-54.0, spin_axis=V(1, 0, 0), flow=0.6, jaw=28.0)
        Pl = P0 + A.web_pelvis(rx + 2.0, 91.0 - 4.8)
        lying = R0.but(pelvis=A.web_pelvis(rx + 2.0, 91.0 - 4.8), lean=-8.0, head_pitch=-6.0, head_roll=-30.0,
                       head_yaw=12.0, twist=-4.0, hip_yaw=0.0,
                       foot_r=Pl + u3(A, -4.5, -8.0, -16.0), foot_l=Pl + u3(A, 6.0, -3.0, -16.0),
                       foot_r_rot=(30.0, 0, 25), foot_l_rot=(18.0, 0, 40), knee_l=V(0.8, 0.6, 0), knee_r=V(-0.4, 1, 0),
                       hand_r=Pl + u3(A, -14.0, -2.5, 10.0), elbow_r=V(-0.4, 0.2, -1.0),
                       wpn=V(-0.62, 0.0, 0.78), wpn_up=V(0, -1, 0),
                       hand_l=Pl + u3(A, 12.5, -1.5, 9.0), elbow_l=V(0.6, 0.2, -1.0),
                       spin=-87.0, spin_axis=V(1, 0, 0), flow=0.05, jaw=20.0)
        return [rec, buckle, tip, lying]

    A.overrides.update(attack_keys=attack_keys, follow=follow, recover=recover, recoil=recoil,
                       death_keys=death_keys)


def pelt_cloth(p, bp: BP, A: GoblinAnims) -> None:
    """Pelt tail hanging down the back (absolute aims, + flow / sway), kept off the back and buttocks."""
    from kit import anim as kanim
    mats = kanim.evaluate(p, return_matrices=True)
    sph = []
    for bn, r in (("pelvis", A.U * 6.6), ("spine_01", A.U * 6.6), ("spine_02", A.U * 6.8)):
        h, tl = mats[bn].translation, mats[bn] @ Vector((0, A.rig.obj.data.bones[bn].length, 0))
        for k in range(4):
            sph.append((h.lerp(tl, k / 3), r))
    for s in ("l", "r"):
        H, Kn = mats[f"thigh_{s}"].translation, mats[f"calf_{s}"].translation
        for k in range(5):
            sph.append((H.lerp(Kn, k / 4), A.U * 3.2))
    prev = mats["pelt_01"].translation
    for k, nm in enumerate(("pelt_01", "pelt_02", "pelt_03")):
        L = A.rig.obj.data.bones[nm].length
        rest = A.rig.obj.data.bones[nm]
        d0 = (rest.tail_local - rest.head_local).normalized()
        a0 = math.degrees(math.atan2(d0.y, -d0.z))          # + = back from straight down
        want = a0 * 0.5 + 6.0 + bp.flow * (18.0 + 8 * k) + 6.0 * math.sin(bp.wave + k * 0.7) + 8.0 * bp.sway
        a = A._flap_env(prev, L, want, -1.0, sph)
        r = math.radians(a)
        d = Vector((0.0, math.sin(r), -math.cos(r)))
        p.aim(nm, d, Vector((0.0, math.cos(r), math.sin(r))))
        prev = prev + d * L


def anims(kit) -> GoblinAnims:
    A = GoblinAnims(kit, REF_SPEED, WALK, ready, extra_cloth=pelt_cloth, style="overhead", contact_wpn=120.0,
                    web_ready=WEB_READY)
    install_poses(A)
    return A


def roar(A: GoblinAnims):
    """Roar 1200 ms (boss intro ``cs_boss_goblin_chief``, NEW): inhale and rear back, then a full-body roar —
    jaw wide, arms flung out and up, cleaver raised high at the side — held with a shudder, then back to READY."""
    R0 = A.ready()
    inhale = R0.but(pelvis=R0.pelvis + u3(A, 0, -1.0, 0.8), lean=-14.0, head_pitch=-22.0, twist=R0.twist - 4.0,
                    hand_r=u3(A, -11.0, 2.0, 20.0), **A._wkw(R0, 150.0, lat=-0.3), hand_l=u3(A, 9.0, 2.0, 21.0),
                    clav_l=(-6.0, 0.0), clav_r=(-6.0, 0.0), jaw=6.0, flow=0.2)
    roar_ = R0.but(pelvis=R0.pelvis + u3(A, 0, 0.5, -1.6), lean=-8.0, head_pitch=-14.0, twist=R0.twist,
                   foot_r=R0.foot_r + u3(A, -1.0, 1.0, 0), foot_l=R0.foot_l + u3(A, 1.0, -0.5, 0),
                   hand_r=u3(A, -17.0, 4.0, 40.0), elbow_r=V(-0.8, -0.2, -0.6), **A._wkw(R0, -10.0, lat=-0.55),
                   hand_l=u3(A, 16.0, 5.0, 37.0), elbow_l=V(0.8, -0.2, -0.6), clav_l=(18.0, 0.0), clav_r=(18.0, 0.0),
                   jaw=34.0, flow=0.55)
    shake = roar_.but(pelvis=roar_.pelvis + u3(A, 0.3, 0, 0.2), head_pitch=-22.0, head_yaw=4.0, jaw=30.0)
    keys = [K(0.0, R0), K(220 / 1200, inhale, "out"), K(450 / 1200, roar_, "out"), K(600 / 1200, shake),
            K(750 / 1200, roar_.but(head_yaw=-4.0)), K(900 / 1200, shake.but(head_pitch=-20.0)),
            K(1.0, R0, "smooth")]

    def layer(bp, t):
        bp = A.waver(1200.0)(bp, t)
        if 450 <= t <= 950:                      # shudder of the roar
            k = math.sin(math.pi * (t - 450) / 500)
            bp = bp.but(head_roll=3.0 * k * math.sin(t * 0.09), pelvis=bp.pelvis + V(0, 0, 0.003 * k * math.sin(t * 0.13)))
        return bp
    c = A.clip("Roar", 1200.0, keys, 1200.0, notifies={"FX_Roar": 450.0}, layer=layer)
    c.key_poses_ms = [0.0, 220.0, 450.0, 750.0, 900.0, 1200.0]
    c.manifest_extra = {"usage": "boss intro cs_boss_goblin_chief (quests-story-ch1 §10.2), plays under the title card"}
    return c


def clips(A: GoblinAnims) -> list:
    return [A.idle(), A.walk(), A.attack(), A.hurt(), A.death(), roar(A),
            A.stun(droop=60.0, hand_r=u3(A, -9.5, 5.0, 19.0), wlat=-0.1)]


GRIP_PARTS = ("fist_r", "knuckle_r", "thumb_r", "claw_r")


def cleaver_grip(co) -> bool:
    k = WEB.U * CLEAVER_K
    return -6.5 * k < co.y < 2.4 * k and abs(co.z) < 1.6 * k


def eye_glows(rig):
    """Runtime ember cards over both eyes (manifest ``fx.eyeEmbers``): additive, web glow r 2.4 u ≈ 5.5 cm,
    #FF5A2A α .35 (+ .3 with the attack ``fx``), faded by how much the face looks at the camera."""
    hb = rig.obj.data.bones["head"]

    def fn(cam, res):
        from kit import review
        out = []
        pb = rig.obj.pose.bones["head"]
        fwd = (rig.obj.matrix_world.to_3x3() @ pb.matrix.to_3x3() @ hb.matrix_local.to_3x3().inverted()
               @ Vector((0, -1, 0))).normalized()
        for s in ("l", "r"):
            e = next(x for x in rig.sockets if x["name"] == f"fx_eye_{s}")
            b = rig.obj.data.bones[e["bone"]]
            pbe = rig.obj.pose.bones[e["bone"]]
            wp = rig.obj.matrix_world @ (pbe.matrix @ (b.matrix_local.inverted() @ Vector(e["pos"])))
            vis = max(0.0, min(1.0, 2.0 * fwd.dot((cam.matrix_world.translation - wp).normalized()) + 0.4))
            px, py = review.project_px(cam, wp, res)
            wpp = outline.pixel_world_size(cam, wp, res[0])
            out.append(dict(center_px=(px, py), radius_px=0.055 / wpp, color="#FF5A2A", alpha=0.4 * vis,
                            core="#FFD08A", core_frac=0.35, core_alpha=0.5 * vis))
        return out
    return fn


FX = {"eyeEmbers": {"sockets": ["fx_eye_l", "fx_eye_r"], "color": "#FF5A2A", "radiusCm": 5.5, "alpha": 0.35,
                    "alphaPerFx": 0.3, "coreColor": "#FFD08A", "coreAlpha": 0.5, "blend": "additive",
                    "falloff": "web glow", "offInDeath": True,
                    "facingFade": "clamp(2 * dot(headFwd, toCamera) + 0.4, 0, 1)"},
      "telegraph": {"normal": "#FFC4B0", "elite": "#FF7A5C", "untilMs": 155},
      "attackTrail": {"color": "#E8ECF4", "alpha": 0.5, "sockets": ["tip", "mid"], "item": CLEAVER},
      "eliteCrown": {"socket": "fx_overhead", "asset": "SM_FX_EliteCrown"}}


def quick(out: Path) -> None:
    import mship
    pal, kit, body, cleaver = build_all()
    A = anims(kit)
    mship.quick_review(ASSET, kit.rig, [body], [(cleaver, "weapon_r")], A, out, HEIGHT, OUTLINE_CLASS,
                       blob_radius=0.29, glows=eye_glows(kit.rig))


def anim_quick(out: Path, names, facing: str = "se") -> None:
    import mship
    pal, kit, body, cleaver = build_all()
    A = anims(kit)
    R.attach_to_bone(cleaver, kit.rig, "weapon_r")
    A.bind_meshes(body, {"weapon_r": cleaver})
    cs = [c for c in clips(A) if not names or c.name in names]
    mship.anim_review(kit.rig, [body], [(cleaver, "weapon_r")], cs, TOKEN, out, HEIGHT, OUTLINE_CLASS,
                      blob_radius=0.29, facing=facing)


def ship(previews: bool = True, verify: bool = True) -> int:
    import mship
    pal, kit, body, cleaver = build_all()
    A = anims(kit)
    R.attach_to_bone(cleaver, kit.rig, "weapon_r")
    A.bind_meshes(body, {"weapon_r": cleaver})
    cs = clips(A)
    U = WEB.U * CLEAVER_K
    items = [mship.Item(cleaver, CLEAVER, "weapon_r", "axe",
                        sockets=[{"name": "tip", "pos": (0.0, 16.0 * U, 2.5 * U)},
                                 {"name": "mid", "pos": (0.0, 9.5 * U, 5.0 * U)}],
                        extra={"lengthCm": round(22.6 * U * 100.0, 1), "bladeCm": [round(9.0 * U * 100, 1),
                                                                                  round(14.2 * U * 100, 1)]},
                        grip=cleaver_grip)]
    ms = mship.MonsterShip(
        asset=ASSET, token=TOKEN, preview_dir="mon_goblin_chief", pal=pal, A=A, body=body, items=items, clips=cs,
        outline_class=OUTLINE_CLASS, game_ids=["monster_goblin_chief"], blob_radius=round(12 * WEB.U, 3), height=HEIGHT,
        display={"zh-CN": "哥布林首领 · 碎牙·格罗克", "en": "Goblin Chief · Grokk Brokentooth"},
        fx=FX, glows=eye_glows(kit.rig),
        qa_groups={"body_nf": mship.parts_except(body, GRIP_PARTS)},
        qa_pairs={"cleaverBody": ("item:" + CLEAVER, "body_nf")},
        qa_limits={"ground": -0.5, "cleaverBody": 0},
        game_keys=[("Idle", 0, "se"), ("Walk", 9, "se"), ("Attack01", 5, "se"), ("Attack01", 10, "se"),
                   ("Attack01", 15, "se"), ("Attack01", 15, "sw"), ("Roar", 27, "se"), ("Death", 30, "se")],
        close_keys=[("Attack01", 5, "se"), ("Attack01", 10, "se"), ("Attack01", 15, "se"), ("Attack01", 15, "side"),
                    ("Roar", 27, "front"), ("Hurt", 0, "se"), ("Death", 20, "se"), ("Death", 30, "se"),
                    ("Stun", 10, "front"), ("Walk", 9, "side")],
        back_clips=("Attack01", "Death", "Roar"),
        skeleton_bones_required=("root", "pelvis", "head", "jaw", "weapon_r", "weapon_l", "ear_l_02", "loin_f_02",
                                 "pelt_03", "headdress_04", "totem_feather_02", "ik_foot_l", "ik_hand_gun"),
        extra={"refSpeedCmS": REF_SPEED * 100.0, "attackStyle": "overhead", "elite": True,
               "notes": "Boss of Chapter 1. Clips in place; Roar plays under the boss title card. Death is followed "
                        "by the knockback impulse and a dithered fade (spec §4)."},
    )
    res = mship.ship(ms, previews=previews, verify=verify)
    return mship.main_result(res)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    scratch = Path("/tmp/claude-0/-home-user-abyssfire/b3225f99-25a3-5b78-996f-643be8685889/scratchpad/mq/chief")
    if "--quick" in args:
        quick(scratch)
    elif "--anims" in args:
        names = [a for a in args[args.index("--anims") + 1:] if not a.startswith("--")]
        fac = next((a.split("=")[1] for a in args if a.startswith("--facing=")), "se")
        anim_quick(scratch / "anims", names, fac)
    else:
        sys.exit(ship(previews="--no-previews" not in args, verify="--no-verify" not in args))

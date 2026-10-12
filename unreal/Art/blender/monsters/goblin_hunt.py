"""Quest-hunt leader accents for ``SK_Mon_Goblin`` (spec art-inventory-ch1.md §4.5, quirk A14; NEW, optional).

The two Chapter-1 hunt leaders are the goblin at scale 1.25 (≈ 1.15 m, R2) with an elite crown (§4.6) and these
attachments, each modelled in the local frame of the bone it rides (UE: attach to the bone by name, identity
transform — the weapon-socket convention):

* ``SM_Mon_Goblin_Att_RedCap`` (``head``) — 红帽格鲁克 Gruk Redcap: a crimson ``#B8322E`` pointed cap with a dark
  turned-up band, its sock flopping back between the ears ("his name says so").
* ``SM_Mon_Goblin_Att_Pendant`` (``spine_03``) — 小贼斯尼克 Sneek the Thief: the stolen heirloom (quest item
  ``mat_pendant``, web ``QuestItemIcons.pendant``): a gold locket ``#EEC05A`` with a red gem ``#E0283C`` (emissive
  glint) worn among his bone-tooth trophies on a short gold chain (the spec's "on the belt" collided with the spear
  butt / free fist / loincloth in every belt spot — per-frame QA).
* ``SM_Mon_Goblin_Att_LootSack`` (``pelvis``) — his loot: a bulging burlap sack tied to the belt at the left hip.
"""
from __future__ import annotations

import math

import bpy
import numpy as np
from mathutils import Matrix, Vector

from kit import mesh as M, outline, shading
from kit.palette import Palette, Region
from kit.rig import Bind

import common as C
import mgeo as G

OUTLINE_CLASS = "monster"
REDCAP = "SM_Mon_Goblin_Att_RedCap"
PENDANT = "SM_Mon_Goblin_Att_Pendant"
LOOTSACK = "SM_Mon_Goblin_Att_LootSack"
SCALE = 1.25
PENDANT_AT = (-10.0, 0.71)       # (torso angle, height s) of the locket: chest, between two teeth
PENDANT_OUT = 1.75
PENDANT_BONE = "spine_03"

REGIONS = outline.ink_regions(dict(
    cap=Region("#B8322E", .42, .32), cap_band=Region("#6E1C1A", .42, .22), cap_seam=Region("#B8322E").shade_region(.3, .12),
    gold=Region("#EEC05A", .35, .50), gold_dark=Region("#EEC05A", .35, .50).shade_region(.3, .15),
    gem=Region("#E0283C", .25, .45, e=0.55),
    sack=Region("#A07A4C", .42, .32), sack_patch=Region("#7C5A36", .42, .25), rope=Region("#B89A62", .42, .32),
    rope_line=Region("#B89A62").shade_region(.30, .12),
), OUTLINE_CLASS)


def _finish(b: M.Builder, pal: Palette, name: str, bone_matrix: Matrix) -> bpy.types.Object:
    obj = b.build(name)
    # armature space → the bone's local frame at its head (= the object frame under attach_to_bone)
    inv = bone_matrix.inverted()
    obj.data.transform(inv)
    obj.data.update()
    mt = shading.toon_material(pal)
    outline.bake_hull(obj, outline.WIDTH_CM[OUTLINE_CLASS] / 100.0, mt, shading.outline_material(pal))
    for msg in outline.hull_color_report(obj, pal, OUTLINE_CLASS):
        print(f"[ink] WARNING {name}: {msg}")
    obj["af_outline_class"] = OUTLINE_CLASS
    return obj


def build_redcap(pal: Palette, kit) -> bpy.types.Object:
    b = M.Builder(REDCAP, pal, prefix="goblin_redcap")
    b.regions(**REGIONS)
    hk = kit.hk
    rig = Bind.rigid("root")

    def rim(a):                       # above the brow in front, low at the nape, clear of the ear roots
        return 4.25 + 1.05 * math.cos(math.radians(a))
    hs = [0.0, 0.2, 0.45, 0.7, 0.88, 1.0]
    angs = G.angles(28)
    outer, inner = [], []
    for f in hs:
        ro, ri = [], []
        for a in angs:
            h = rim(a) + (7.0 - rim(a)) * f
            ro.append(kit.head_point(h, a, hk(0.75)))
            ri.append(kit.head_point(h, a, hk(0.12)))
        outer.append(ro)
        inner.append(ri)
    dome = C.thick_patch(outer, inner, wrap_u=True)
    did = b.add(dome, "cap", rig, name="cap", hull_pad=True)
    # the sock: from the crown back and down between the ears, drooping
    # (the tip stays within the skull's back plane, f ≥ −7: lying dead on his back the sock lies on the ground
    # past the crown instead of through it)
    pts = [kit.hp(6.6, -0.8), kit.hp(8.6, -1.8), kit.hp(10.1, -3.3), kit.hp(10.6, -5.0), kit.hp(10.0, -6.3),
           kit.hp(8.9, -6.9)]
    rad = [hk(3.6), hk(2.9), hk(2.1), hk(1.45), hk(0.9), hk(0.45)]
    sock = M.tube(pts, rad, sides=14, dome0=0.0, dome1=hk(0.35))
    sid = b.add(sock, "cap", rig, name="cap_sock", hull_pad=True)
    for k in range(3):                 # two creases where it folds
        i = 2 + k
        tg = (pts[i + 1] - pts[i - 1]).normalized() if i + 1 < len(pts) else (pts[i] - pts[i - 1]).normalized()
        ring = M.tube([pts[i] - tg * hk(0.18), pts[i] + tg * hk(0.18)], [rad[i] * 1.04] * 2, sides=14, dome0=0.0,
                      dome1=0.0)
        if k < 2:
            b.add(ring, "cap_seam", rig, name="cap_crease", outline=False, covered_by=sid)
    hsurf = _HS(kit, hk(0.75))
    band = G.surface_ring(hsurf, lambda a: rim(a) + 0.55, hk(1.35), hk(0.55), segs=36)
    b.add(band, "cap_band", rig, name="cap_band", outline=False, covered_by=did)
    return _finish(b, pal, REDCAP, kit.rig.obj.data.bones["head"].matrix_local)


class _HS:
    def __init__(self, kit, off):
        self.k, self.off = kit, off

    def point(self, h, a, out=0.0):
        return self.k.head_point(h, a, self.off + out)

    def normal(self, h, a):
        return self.k.head_normal(h, a)


def build_pendant(pal: Palette, kit) -> bpy.types.Object:
    """The stolen locket worn among his bone-tooth trophies: a short gold chain from the necklace cord, the locket
    in front of the teeth, on ``spine_03``. (The spec suggests the belt; every belt spot is crossed by the spear
    butt, the free fist's swing, the belly or the loincloth in some clip — per-frame QA — and the chest reads from
    every front view.)"""
    b = M.Builder(PENDANT, pal, prefix="goblin_pendant")
    b.regions(**REGIONS)
    u = kit.u
    t = kit.torso
    rig = Bind.rigid("root")
    a0, s0 = PENDANT_AT
    sc = 0.90 - 0.06 * math.cos(math.radians(a0)) ** 2           # the necklace cord (GoblinKit.add_necklace)
    top = t.point(sc, a0, u(0.55))
    n = t.normal(s0, a0)
    lc = t.point(s0, a0, u(PENDANT_OUT))
    bail = lc + (top - lc).normalized() * u(2.2)
    for k in range(4):
        f = (k + 0.5) / 4
        c = top.lerp(bail, f) + n * u(0.25 * math.sin(math.pi * f))
        lk = M.ellipsoid((u(0.22), u(0.36) if k % 2 else u(0.12), u(0.34)), c, sides=8, rings=5)
        b.add(lk, "gold", rig, name="chain", hull_pad=True)
    face = (n + Vector((0, 0, 0.2))).normalized()          # tipped up a little: the face catches the light
    m = G.frame_z(lc, face, Vector((0, 0, 1)))
    locket = M.ellipsoid((u(1.85), u(2.15), u(0.62)), (0, 0, 0), sides=16, rings=8).transform(m)
    lid = b.add(locket, "gold", rig, name="locket", hull_pad=True)
    ring = []
    for i in range(24):
        a = 2 * math.pi * i / 24
        ring.append(m @ Vector((math.cos(a) * u(1.55), math.sin(a) * u(1.85), u(0.47))))
    rimp = M.tube(ring + ring[:1], [u(0.13)] * 25, sides=6, dome0=0.0, dome1=0.0)
    b.add(rimp, "gold_dark", rig, name="locket_rim", outline=False, covered_by=lid)
    gem = M.ellipsoid((u(0.9), u(1.12), u(0.42)), (0, 0, u(0.5)), sides=10, rings=6).transform(m)
    b.add(gem.flat(), "gem", rig, name="gem", outline=False, covered_by=lid)
    return _finish(b, pal, PENDANT, kit.rig.obj.data.bones[PENDANT_BONE].matrix_local)


def build_lootsack(pal: Palette, kit) -> bpy.types.Object:
    b = M.Builder(LOOTSACK, pal, prefix="goblin_lootsack")
    b.regions(**REGIONS)
    u = kit.u
    t = kit.torso
    rig = Bind.rigid("root")
    a0 = 128.0
    top = t.point(0.08, a0, u(1.0))
    n = t.normal(0.08, a0)
    c = top + n * u(2.6) + Vector((0, 0, -u(3.6)))
    rng = np.random.default_rng(7)

    def lumpy(co):
        d = co - c
        r = d.length
        if r < 1e-6:
            return co
        # soft burlap lumps (half the old amplitude, longer waves: the toon bands drew the old ones as rock facets)
        k = 1.0 + 0.035 * math.sin(d.x / u(1.0) * 1.2 + 1.0) * math.sin(d.y / u(1.0) * 1.0) + 0.025 * math.sin(d.z / u(1.0) * 1.5)
        return c + d * k
    # 24 × 14 (was 16 × 10: flat facets read through the toon bands at the close-ups) and smooth normals
    sack = M.ellipsoid((u(3.0), u(2.7), u(3.3)), c, sides=24, rings=14).deform(lumpy)
    sid = b.add(sack.smooth(True), "sack", rig, name="sack", hull_pad=True)
    neck = M.tube([c + Vector((0, 0, u(2.6))), c + Vector((0, 0, u(3.7))), top + Vector((0, 0, -u(0.2)))],
                  [u(1.2), u(0.85), u(1.25)], sides=10, dome0=0.0, dome1=0.0)
    b.add(neck, "sack", rig, name="sack_neck", hull_pad=True)
    tie = M.tube([c + Vector((0, 0, u(3.35))), c + Vector((0, 0, u(3.9)))], [u(1.05)] * 2, sides=10, dome0=0.0,
                 dome1=0.0)
    b.add(tie, "rope", rig, name="sack_tie", hull_pad=True)
    # a patch and a few stitches (burlap): the stitches are flat dashes lying on the patch, inside its edge (art
    # review: boxes standing across the edge read as four grey prongs / claws in the silhouette)
    pm = G.frame_z(c + n * u(2.85) + Vector((0, 0, -u(0.4))), n, Vector((0, 0, 1)))
    patch = M.box((u(2.2), u(1.9), u(0.3)), (0, 0, 0), bevel=u(0.08)).transform(pm)
    b.add(patch, "sack_patch", rig, name="sack_patch", outline=False, covered_by=sid)
    for k in range(4):
        for y in (0.62, -0.62):
            st = M.box((u(0.2), u(0.42), u(0.08)), (u(-0.78 + 0.52 * k), u(y), u(0.16)), bevel=u(0.02)).transform(pm)
            b.add(st, "rope_line", rig, name="stitch", outline=False, covered_by=sid)
    return _finish(b, pal, LOOTSACK, kit.rig.obj.data.bones["pelvis"].matrix_local)


VARIANTS = {
    "hunt_redcap_gruk": {
        "displayName": {"zh-CN": "红帽格鲁克", "en": "Gruk Redcap"}, "scale": SCALE, "elite": True,
        "attachments": [{"object": REDCAP, "socket": "head"}],
        "note": "quest q_bandit_trouble hunt leader (quest_hunts.json): SK_Mon_Goblin x1.25 + red cap + elite crown"},
    "hunt_pendant_thief": {
        "displayName": {"zh-CN": "小贼斯尼克", "en": "Sneek the Thief"}, "scale": SCALE, "elite": True,
        "attachments": [{"object": PENDANT, "socket": PENDANT_BONE}, {"object": LOOTSACK, "socket": "pelvis"}],
        "note": "quest q_lost_pendant hunt leader: SK_Mon_Goblin x1.25 + the stolen pendant (glint) + loot sack "
                "+ elite crown. Deviation from spec §4.5 (pendant 'on the belt'): the locket hangs on the chest "
                "(spine_03) among the bone-tooth trophies — every belt spot is crossed by the spear butt, the free "
                "fist's swing, the belly or the loincloth in some clip (per-frame QA), and the chest reads from "
                "every front view"},
}

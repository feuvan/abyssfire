"""NPC kit props (spec art-inventory-ch1.md §5.1 ``drawProp`` :1205-1345, §15 ``SM_NPCKit_Prop_*``) and the NPC world
dressing meshes (§5.2 anvil scenery ``SM_Prop_Camp_AnvilStump``, §5.4 / §9.4 quest markers ``SM_UI_QuestMarker_*``).

Held props are modelled in **weapon-bone space** (kit convention): grip at the origin, **+Y along the item** (hammer
head, staff top, blade tip, the hanging bag), **+Z the leading face** (the hammer's striking face, the open book's
pages, the scroll's text) — they attach to ``weapon_r`` / ``weapon_l`` with an identity transform. Materials (web):
metal ``#9AA3B2`` (l .5), dark metal ``#5A606E``, wood ``#7A5230`` (l .3), paper ``#EFE3C2`` (s .25, l .4), gold
``#D9A640`` (l .5), leather ``#6A4228``; staff orb ``#7FD6A0`` (l .5), book ``#3A4A7A``. Web units ``u`` = the NPC rig
unit (3.19 cm; props use the unfitted unit so every NPC's hammer is the same mesh).
"""
from __future__ import annotations

import math

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

from kit import asset, mesh as M, outline, rig as R
from kit.palette import Palette, Region

import common as C
import hero_base as HB
from hero_base import ell

U = 0.0319 * 0.985          # the props' web unit (≈ the fitted NPC unit)
PROP_CLASS = "npc"

PROP_REGIONS = outline.ink_regions(dict(
    metal=Region("#9AA3B2", .42, .50),
    metal_dark=Region("#5A606E"),
    metal_face=Region("#C8D2E2", .30, .45),           # web working face rgba(200,210,226,.85)
    wood=Region("#7A5230", .42, .30),
    wood_dark=Region("#7A5230", .42, .30).shade_region(.30, .12),
    paper=Region("#EFE3C2", .25, .40),
    ink_line=Region("#EFE3C2", .25, .40).line_region(.2),
    gold=Region("#D9A640", .42, .50),
    leather=Region("#6A4228"),
    leather_dark=Region("#6A4228").shade_region(.30, .12),
    orb=Region("#7FD6A0", .25, .50, e=0.35),         # lit by the runtime orbGlow card; its bloom keeps the ink
    orb_hot=Region("#E8FFD0", .0, .0, e=1.0),
    book=Region("#3A4A7A"),
    book_dark=Region("#3A4A7A").shade_region(.30, .12),
    hot_iron=Region("#FF8A3A", .10, .30, e=0.85),
    hot_core=Region("#FFD08A", .0, .0, e=1.0),
    herb=Region("#6FA84A", .42, .35),
    herb_light=Region("#A6D46A", .30, .40),
    blade=Region("#B4BDCC", .15, .50),                # the warrior's blade rule: shade stays mid steel
    blade_edge=Region("#E2E8F2", .10, .45),
), PROP_CLASS)

PROPS = {
    "hammer": "SM_NPCKit_Prop_Hammer",
    "tongs": "SM_NPCKit_Prop_Tongs",
    "staff": "SM_NPCKit_Prop_Staff",
    "scroll": "SM_NPCKit_Prop_Scroll",
    "pouch": "SM_NPCKit_Prop_Pouch",
    "herbs": "SM_NPCKit_Prop_HerbPouch",
    "sword": "SM_NPCKit_Prop_Sword",
    "book": "SM_NPCKit_Prop_Book",
}

SWORD_BLADE = 22.0 * U      # 3D: the web's 17.5 u blade lengthened so the pommel sits at the hip when he leans on it
SWORD_TIP_Y = 2.3 * U + SWORD_BLADE + 0.9 * U
STAFF_DOWN = 0.96           # grip → foot (m): planted on the ground from the READY chest-height grip
STAFF_UP = 0.56             # grip → orb centre
ORB_R = 2.2 * U


def _builder(name: str, pal: Palette) -> M.Builder:
    b = M.Builder(name, pal, prefix=f"npcprop.{name[len('SM_NPCKit_Prop_'):].lower()}")
    b.regions(**PROP_REGIONS)
    return b


def _finish(b: M.Builder, pal: Palette, name: str) -> bpy.types.Object:
    obj = asset.finish_mesh(b, pal, PROP_CLASS)
    obj.name = name
    return obj


def build_hammer(pal: Palette):
    """Smith's hammer (web: handle 15 u, head 7.6 × 4.6 u): ash handle with a dark wedge and leather wrap at the
    grip, a steel head across the handle — a round striking face on +Z (the swing's leading side), a wedge peen
    behind."""
    b = _builder(PROPS["hammer"], pal)
    y0, y1 = -2.2 * U, 11.6 * U                    # held near the end of the handle (a smith's grip)
    ys = np.linspace(y0, y1, 9)
    hid = b.add(M.tube([(0, y, 0) for y in ys], [0.66 * U * (1.06 - 0.12 * (y - y0) / (y1 - y0)) for y in ys],
                       sides=10, dome0=0.3 * U, dome1=0.0), "wood", name="handle", hull_pad=True)
    b.add(M.tube([(0, -1.5 * U, 0), (0, 1.9 * U, 0)], [0.74 * U, 0.74 * U], sides=10, dome0=0.0, dome1=0.0),
          "leather", name="wrap", outline=False, covered_by=hid)
    for y in (-0.9 * U, 0.3 * U, 1.4 * U):
        b.add(M.tube([(0, y - 0.12 * U, 0), (0, y + 0.12 * U, 0)], [0.78 * U, 0.78 * U], sides=10, dome0=0.0,
                     dome1=0.0), "leather_dark", name="wrap_line", outline=False, covered_by=hid)
    yc = 12.1 * U
    head = M.box((3.7 * U, 4.6 * U, 5.2 * U), (0, yc, 0.2 * U), bevel=0.5 * U, segments=2)
    hd = b.add(head, "metal", name="head", hull_pad=True)
    face = M.cylinder(2.25 * U, 2.45 * U, 2.6 * U, 4.25 * U, sides=16)            # along +Z
    b.add(face.translate((0, yc, 0)), "metal", name="face", hull_pad=True)
    b.add(M.cylinder(2.45 * U, 2.35 * U, 4.25 * U, 4.45 * U, sides=16).translate((0, yc, 0)), "metal_face",
          name="face_disc", outline=False, covered_by=hd)
    peen = M.loft_z([(-3.75 * U, 0.35 * U, 2.0 * U), (-3.3 * U, 1.1 * U, 2.15 * U), (-2.3 * U, 1.75 * U, 2.3 * U)],
                    sides=12, exp=4.0, cap0="ngon", cap1="ngon").translate((0, yc, 0.2 * U))
    b.add(peen, "metal", name="peen", hull_pad=True)
    b.add(M.box((1.1 * U, 0.5 * U, 0.6 * U), (0, yc + 2.32 * U, 0.2 * U), bevel=0.001), "metal_dark",
          name="wedge", outline=False, covered_by=hd)
    obj = _finish(b, pal, PROPS["hammer"])
    return obj, [{"name": "head", "pos": (0, yc, 4.35 * U)}, {"name": "tip", "pos": (0, yc, 0)},
                 {"name": "mid", "pos": (0, 5.0 * U, 0)}]


def build_tongs(pal: Palette):
    """Smith's tongs (web: two bars 14 u) holding a glowing iron bar: the forge's focal accent (one glow, spec
    §1.5), cooling from a hot core."""
    b = _builder(PROPS["tongs"], pal)
    piv = 4.2 * U
    for sg in (1.0, -1.0):
        pts = [(sg * 0.32 * U, -1.5 * U, 0), (sg * 0.22 * U, piv - 0.6 * U, 0), (-sg * 0.1 * U, piv + 1.2 * U, 0),
               (-sg * 0.42 * U, 10.0 * U, 0), (-sg * 0.70 * U, 12.6 * U, 0.15 * U)]
        path = HB.catmull([Vector(p) for p in pts], 10)
        b.add(M.sweep(path, M.superellipse(1, 1, 8, 2.6), scales=[(0.34 * U, 0.42 * U)] * len(path), up=(0, 0, 1),
                      dome0=0.15 * U, dome1=0.1 * U), "metal_dark", name="arm", hull_pad=True)
        jaw = HB.catmull([Vector((-sg * 0.70 * U, 12.4 * U, 0.15 * U)), Vector((-sg * 1.0 * U, 13.6 * U, 0.5 * U)),
                          Vector((-sg * 0.55 * U, 14.6 * U, 0.6 * U))], 6)
        b.add(M.sweep(jaw, M.superellipse(1, 1, 8, 2.6), scales=[(0.36 * U, 0.48 * U)] * len(jaw), up=(0, 0, 1),
                      dome0=0.1 * U, dome1=0.15 * U), "metal_dark", name="jaw", hull_pad=True)
    b.add(ell((0.65 * U, 0.65 * U, 0.55 * U), (0, piv + 0.3 * U, 0), sides=10, rings=6), "metal", name="rivet",
          hull_pad=True)
    # the hot bar in the jaws: across +X, emissive, its middle white-hot
    bar_c = Vector((0, 14.3 * U, 0.55 * U))
    bid = b.add(M.box((5.2 * U, 1.1 * U, 1.1 * U), bar_c, bevel=0.3 * U), "hot_iron", name="hot_bar", hull_pad=True)
    b.add(M.box((2.4 * U, 1.15 * U, 1.15 * U), bar_c, bevel=0.3 * U), "hot_core", name="hot_core", outline=False,
          covered_by=bid)
    obj = _finish(b, pal, PROPS["tongs"])
    return obj, [{"name": "tip", "pos": tuple(bar_c)}, {"name": "glow", "pos": tuple(bar_c)}]


def build_staff(pal: Palette):
    """Elder's staff (web: 48 u shaft, orb r 2.2 u #7FD6A0 under a #FFCF50 glow): a knotted shaft planted on the
    ground from a chest-high grip, a gold collar with three short prongs cradling the orb."""
    b = _builder(PROPS["staff"], pal)
    y0, y1 = -STAFF_DOWN, STAFF_UP - ORB_R * 1.05
    n = 24
    ys = [y0 + (y1 - y0) * i / n for i in range(n + 1)]
    knots = (-0.62, -0.22, 0.22)

    def axis(y):
        f = (y - y0) / (y1 - y0)
        return Vector((0.012 * math.sin(math.pi * f * 1.3), y, 0.008 * math.sin(math.pi * f * 2.1)))

    def radius(y):
        r = 0.74 * U + 0.12 * U * (1 - (y - y0) / (y1 - y0))
        for kz in knots:
            r += 0.30 * U * math.exp(-((y - kz) / 0.022) ** 2)
        return r
    sid = b.add(M.sweep([axis(y) for y in ys], M.superellipse(1, 1, 10, 2.0, phase=0.3),
                        scales=[(radius(y), radius(y) * 0.92) for y in ys], up=(0, 0, 1), dome0=0.0, dome1=0.0,
                        twist_deg=30.0), "wood", name="shaft", hull_pad=True)
    for ph in (0.0, math.pi):
        gp = []
        for i in range(2, n - 1):
            y = ys[i]
            a = ph + 2.0 * (y - y0) / (y1 - y0) * math.pi
            r = radius(y) * 0.97
            gp.append(axis(y) + Vector((math.cos(a) * r, 0.0, math.sin(a) * r)))
        b.add(M.tube(gp, [0.0026] * len(gp), sides=4, dome0=0.0, dome1=0.0), "wood_dark", name="bark",
              outline=False, covered_by=sid)
    top = axis(y1)
    collar = M.lathe([(0.95 * U, -1.2 * U), (1.15 * U, -0.2 * U), (1.25 * U, 0.4 * U)], sides=14,
                     cap0="fan", cap1="fan").rotate("x", -90).translate(top)
    b.add(collar, "gold", name="collar", hull_pad=True)
    oc = Vector((0, STAFF_UP, 0))
    for k in range(3):
        a = math.radians(90 + 120 * k)
        base = top + Vector((math.cos(a) * 1.0 * U, 0.3 * U, math.sin(a) * 1.0 * U))
        mid = oc + Vector((math.cos(a) * ORB_R * 1.05, -ORB_R * 0.35, math.sin(a) * ORB_R * 1.05))
        tip = oc + Vector((math.cos(a) * ORB_R * 0.75, ORB_R * 0.45, math.sin(a) * ORB_R * 0.75))
        b.add(M.tube(HB.catmull([base, mid, tip], 6), [0.36 * U * (1 - 0.6 * i / 6) for i in range(7)], sides=6,
                     dome0=0.0, dome1=0.12 * U), "gold", name="prong", hull_pad=True)
    oid = b.add(ell((ORB_R, ORB_R, ORB_R), oc, sides=18, rings=12), "orb", name="orb", hull_pad=True)
    b.add(ell((ORB_R * 0.35, ORB_R * 0.22, ORB_R * 0.35), oc + Vector((0.45 * ORB_R, 0.45 * ORB_R, 0.62 * ORB_R)),
              sides=8, rings=4), "orb_hot", name="orb_hot", outline=False, covered_by=oid)
    b.add(M.cylinder(0.85 * U, 0.78 * U, -0.05, 0.0, sides=10).rotate("x", -90).translate((0, y0 + 0.05, 0)),
          "metal_dark", name="ferrule", hull_pad=True)
    obj = _finish(b, pal, PROPS["staff"])
    return obj, [{"name": "orb", "pos": tuple(oc)}, {"name": "tip", "pos": tuple(oc)},
                 {"name": "foot", "pos": (0, y0, 0)}]


def build_scroll(pal: Palette):
    """Elder's scroll (web: paper 3.6 × 10 u between two wooden rollers): held by the lower roller, the sheet
    rising along +Y, curled, its text (+Z) facing the reader."""
    b = _builder(PROPS["scroll"], pal)
    w, h = 3.4 * U, 9.6 * U
    rows, cols = 8, 7
    outer, inner = [], []
    for r in range(rows + 1):
        y = 0.6 * U + h * r / rows
        ro, ri = [], []
        for c in range(cols + 1):
            x = -w + 2 * w * c / cols
            z = -0.55 * U * (1 - (x / w) ** 2) + 0.35 * U * math.sin(math.pi * r / rows)
            ro.append(Vector((x, y, z + 0.0016)))
            ri.append(Vector((x, y, z - 0.0016)))
        outer.append(ro)
        inner.append(ri)
    pid = b.add(C.thick_patch(outer, inner), "paper", name="paper", hull_pad=True)
    for i in range(5):
        y = 2.2 * U + i * 1.5 * U
        ln = (1.0 - 0.25 * (i % 2)) * w * 0.75
        pts = [Vector((x, y, -0.55 * U * (1 - (x / w) ** 2) + 0.35 * U * math.sin(math.pi * (y - 0.6 * U) / h)
                       + 0.0024)) for x in np.linspace(-ln, ln * 0.8, 6)]
        b.add(M.tube(pts, [0.12 * U] * 6, sides=4, dome0=0.0, dome1=0.0), "ink_line", name="text", outline=False,
              covered_by=pid)
    for y, zc in ((0.3 * U, -0.2 * U), (0.6 * U + h + 0.25 * U, -0.2 * U)):
        b.add(M.capsule((-w - 0.9 * U, y, zc), (w + 0.9 * U, y, zc), 0.85 * U, 0.85 * U, sides=10, cap_rings=1,
                        mid=1), "wood", name="roller", hull_pad=True)
    obj = _finish(b, pal, PROPS["scroll"])
    return obj, [{"name": "tip", "pos": (0, 0.6 * U + h, 0)}]


def _bag(b: M.Builder, region: str, line: str, scale: float = 1.0) -> int:
    """A plump drawstring bag hanging along +Y from the origin (web pouch: 7.2 wide × 7 u, gold band at the neck)."""
    s = scale
    prof = [(0.55 * U * s, 0.0), (0.9 * U * s, 0.7 * U * s), (2.6 * U * s, 2.2 * U * s), (3.5 * U * s, 4.4 * U * s),
            (3.3 * U * s, 6.2 * U * s), (2.0 * U * s, 7.3 * U * s), (0.4 * U * s, 7.65 * U * s)]
    bag = M.lathe([(r, z) for r, z in prof], sides=16, squash_y=0.82).rotate("x", -90)
    bag.deform(lambda co: Vector((co.x * (1 + 0.06 * math.sin(co.y / U * 1.3)), co.y, co.z)))
    bid = b.add(bag, region, name="bag", hull_pad=True)
    for a in (-35.0, 35.0, 150.0):
        pts = []
        for i in range(7):
            t = i / 6
            y = (1.2 + 5.0 * t) * U * s
            r = np.interp(y, [p[1] for p in prof], [p[0] for p in prof]) * 1.003
            ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
            pts.append(Vector((r * sa, y, r * ca * 0.82)))
        b.add(M.tube(pts, [0.13 * U] * 7, sides=4, dome0=0.0, dome1=0.0), line, name="fold", outline=False,
              covered_by=bid)
    b.add(M.tube([(0, 0.55 * U * s, 0), (0, 1.35 * U * s, 0)], [1.05 * U * s, 1.12 * U * s], sides=14, dome0=0.0,
                 dome1=0.0), "gold", name="band", hull_pad=True)
    b.add(ell((0.95 * U * s, 0.6 * U * s, 0.8 * U * s), (0, -0.15 * U * s, 0), sides=10, rings=6), region,
          name="neck", hull_pad=True)
    return bid


def build_pouch(pal: Palette):
    b = _builder(PROPS["pouch"], pal)
    bid = _bag(b, "leather", "leather_dark", 1.0)
    # a coin emblem stamped on the front (+Z): the merchant's fat purse
    fr = Matrix.Translation((0, 4.6 * U, 2.75 * U)) @ Matrix.Rotation(math.radians(-8), 4, "X")
    coin = M.cylinder(1.05 * U, 1.0 * U, -0.25 * U, 0.12 * U, sides=14)
    b.add(coin.transform(fr), "gold", name="emblem", outline=False, covered_by=bid)
    obj = _finish(b, pal, PROPS["pouch"])
    return obj, [{"name": "tip", "pos": (0, 4.0 * U, 0)}]


def build_herb_pouch(pal: Palette):
    """The herbalist's pouch: the same drawstring bag in linen-green leather, dried herb sprigs poking out of the
    neck (her focal accent is the runtime green glow over the hand)."""
    b = _builder(PROPS["herbs"], pal)
    bid = _bag(b, "leather", "leather_dark", 0.85)
    for k, (a, ln, tilt) in enumerate(((20.0, 4.2, 18.0), (140.0, 3.6, 24.0), (260.0, 3.9, 14.0), (80.0, 3.0, 30.0))):
        ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
        base = Vector((0.35 * U * sa, 0.3 * U, 0.35 * U * ca))
        d = Vector((math.sin(math.radians(tilt)) * sa, -math.cos(math.radians(tilt)), math.sin(math.radians(tilt)) * ca))
        tip = base + d * ln * U
        b.add(M.tube([base, base.lerp(tip, 0.5), tip], [0.16 * U, 0.14 * U, 0.08 * U], sides=5, dome0=0.0,
                     dome1=0.05 * U), "herb", name="stem", hull_pad=True)
        for j in range(3):
            f = 0.45 + 0.22 * j
            p = base.lerp(tip, f)
            side = Vector((ca, 0, -sa)) * (1 if j % 2 else -1)
            leaf = ell((0.95 * U, 0.35 * U, 0.45 * U), (0, 0, 0), sides=8, rings=4)
            m = HB.basis(d, side) if abs(d.dot(side)) < 0.95 else Matrix.Identity(4)
            leaf.transform(Matrix.Translation(p + side * 0.55 * U) @ m @ Matrix.Rotation(math.radians(90), 4, "Z"))
            b.add(leaf, "herb_light" if j == 2 else "herb", name="leaf", hull_pad=True)
    del bid
    obj = _finish(b, pal, PROPS["herbs"])
    return obj, [{"name": "tip", "pos": (0, -2.5 * U, 0)}, {"name": "glow", "pos": (0, -2.5 * U, 0)}]


def build_sword(pal: Palette):
    """The wanderer's sword (web: blade 17.5 u, gold guard 6.8 u, leather grip; 3D: blade 22 u so the pommel sits
    at the hip when he leans on it): diamond-section blade with a fuller ridge (shade .15: never a dark bar)."""
    b = _builder(PROPS["sword"], pal)
    gy = 2.3 * U
    L = SWORD_BLADE
    w0, w1 = 1.15 * U, 0.95 * U
    t = 0.30 * U
    rows = 10
    bm = bmesh.new()
    ring = []
    for i in range(rows + 1):
        f = i / rows
        y = gy + 0.3 * U + L * f
        w = w0 + (w1 - w0) * f
        if f > 0.82:
            w *= max(0.05, 1 - ((f - 0.82) / 0.18) ** 1.2)
        th = t * (1 - 0.6 * max(0.0, f - 0.82) / 0.18)
        ring.append([bm.verts.new((w, y, 0)), bm.verts.new((0, y, th)), bm.verts.new((-w, y, 0)),
                     bm.verts.new((0, y, -th))])
    for r0, r1 in zip(ring, ring[1:]):
        for j in range(4):
            bm.faces.new((r0[j], r0[(j + 1) % 4], r1[(j + 1) % 4], r1[j]))
    bm.faces.new(list(reversed(ring[0])))
    tipv = bm.verts.new((0, gy + 0.3 * U + L + 0.6 * U, 0))
    for j in range(4):
        bm.faces.new((ring[-1][j], ring[-1][(j + 1) % 4], tipv))
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    blid = b.add(M.Part(bm).flat(), "blade", name="blade", hull_pad=True)
    for sg in (1.0, -1.0):
        b.add(M.tube([(0, gy + 1.0 * U, sg * t * 0.55), (0, gy + L * 0.75, sg * t * 0.45)], [0.18 * U, 0.12 * U],
                     sides=4, dome0=0.0, dome1=0.0), "blade_edge", name="fuller", outline=False, covered_by=blid)
    b.add(M.capsule((-3.4 * U, gy, 0), (3.4 * U, gy, 0), 0.55 * U, 0.55 * U, sides=8, cap_rings=1, mid=1),
          "gold", name="guard", hull_pad=True)
    gid = b.add(M.tube([(0, -2.4 * U, 0), (0, gy - 0.3 * U, 0)], [0.62 * U, 0.58 * U], sides=10, dome0=0.0,
                       dome1=0.0), "leather", name="grip", hull_pad=True)
    for y in np.linspace(-1.8 * U, gy - 0.7 * U, 4):
        b.add(M.tube([(0, y - 0.1 * U, 0), (0, y + 0.1 * U, 0)], [0.66 * U, 0.66 * U], sides=10, dome0=0.0,
                     dome1=0.0), "leather_dark", name="wrap_line", outline=False, covered_by=gid)
    b.add(ell((0.95 * U, 0.85 * U, 0.95 * U), (0, -2.9 * U, 0), sides=12, rings=8), "gold", name="pommel",
          hull_pad=True)
    obj = _finish(b, pal, PROPS["sword"])
    return obj, [{"name": "tip", "pos": (0, gy + L + 0.9 * U, 0)}, {"name": "mid", "pos": (0, gy + L * 0.5, 0)},
                 {"name": "pommel", "pos": (0, -2.9 * U, 0)}]


def build_book(pal: Palette):
    """The stash keeper's inventory book (web: 7.2 × 9.2 u #3A4A7A, page edge, gold band), held open in the palm:
    spine along +Y, covers open 150°, pages fanned toward +Z (the reader)."""
    b = _builder(PROPS["book"], pal)
    h = 4.6 * U
    w = 3.4 * U
    open_deg = 15.0
    sid = None
    for sg in (1.0, -1.0):
        a = math.radians(open_deg) * sg
        d = Vector((sg * math.cos(a), 0, math.sin(abs(a))))      # the cover runs out to ±X, tilted toward −Z
        n = Vector((-sg * math.sin(abs(a)), 0, math.cos(a)))
        c = d * (w * 0.5 + 0.2 * U) - n * 0.25 * U
        fr = HB.basis(d, Vector((0, 1, 0)))
        fr.translation = c
        cov = M.box((w + 0.4 * U, 2 * h + 0.4 * U, 0.35 * U), (0, 0, 0), bevel=0.12 * U)
        cid = b.add(cov.transform(fr), "book", name="cover", hull_pad=True)
        sid = sid if sid is not None else cid
        pages = M.box((w - 0.1 * U, 2 * h - 0.2 * U, 0.9 * U), (0, 0, 0.5 * U), bevel=0.25 * U)
        # pages curve up toward the spine (a fanned block)
        pages.deform(lambda co: Vector((co.x, co.y, co.z + 0.35 * U * (1 - abs(co.x) / w) * (co.z > 0.5 * U))))
        b.add(pages.transform(fr), "paper", name="pages", hull_pad=True)
        for i in range(4):
            y = -h * 0.55 + i * h * 0.36
            p0 = fr @ Vector((-w * 0.38, y, 1.0 * U))
            p1 = fr @ Vector((w * 0.32 - 0.4 * U * (i % 2), y, 0.98 * U))
            b.add(M.tube([p0, p1], [0.11 * U, 0.11 * U], sides=4, dome0=0.0, dome1=0.0), "ink_line", name="text",
                  outline=False, covered_by=None)
    b.add(M.tube([(0, -h - 0.15 * U, -0.25 * U), (0, h + 0.15 * U, -0.25 * U)], [0.55 * U, 0.55 * U], sides=10,
                 dome0=0.1 * U, dome1=0.1 * U), "book_dark", name="spine", hull_pad=True)
    b.add(M.box((0.4 * U, 0.2 * U, 2.0 * U), (0.0, h - 0.4 * U, 0.6 * U), bevel=0.0005), "gold", name="ribbon",
          hull_pad=True)
    obj = _finish(b, pal, PROPS["book"])
    return obj, [{"name": "tip", "pos": (0, 0, 1.0 * U)}]


BUILDERS = {"hammer": build_hammer, "tongs": build_tongs, "staff": build_staff, "scroll": build_scroll,
            "pouch": build_pouch, "herbs": build_herb_pouch, "sword": build_sword, "book": build_book}


def build_prop(kind: str, pal: Palette):
    """→ (object, sockets) for a look's prop name (``pouch`` held by the herbalist is the herb pouch)."""
    return BUILDERS[kind](pal)


# ── world dressing ──────────────────────────────────────────────────────────────────────────────────────
ANVIL = "SM_Prop_Camp_AnvilStump"
DRESS_REGIONS = outline.ink_regions(dict(
    bark=Region("#6E4A2C"),
    bark_dark=Region("#6E4A2C").shade_region(.30, .12),
    wood_top=Region("#9A7048", .42, .30),
    rings=Region("#9A7048", .42, .30).shade_region(.30, .12),
    iron=Region("#5C6270", .42, .40),
    iron_face=Region("#C8D2E2", .30, .45),
    gold=Region("#F1C40F", .30, .45),
    gold_dark=Region("#F1C40F", .30, .45).shade_region(.25, .15),
    grey=Region("#888888", .35, .40),
    grey_dark=Region("#888888", .35, .40).shade_region(.25, .15),
), "npc")


def build_anvil(pal: Palette):
    """Web anvilScenery (NpcKit.ts:276-345): a tree stump (bark #6E4A2C, cut top #9A7048, r 4 u, top 12 u) with an
    iron anvil (#5C6270, l .4) on it — top 17 u ≈ 54 cm, 11.5 u long × 4.6 u wide, the horn pointing away from the
    smith (+Y here = forward of the smith = −Y body… the prop's own front is −Y like every directional prop; the
    horn points −Y), a pale working face. Socket ``strike`` on the face where the hammer lands."""
    b = M.Builder(ANVIL, pal, prefix="npcprop.anvil")
    b.regions(**DRESS_REGIONS)
    u = 0.0319
    r = 4.0 * u
    top = 12.2 * u
    prof = []
    for i in range(9):
        z = top * i / 8
        rr = r * (1.0 + 0.16 * (1 - i / 8) ** 3) + 0.004 * math.sin(i * 1.7)
        prof.append((z, rr, rr * 0.94))
    stump = M.loft_z([(z, rx, ry) for z, rx, ry in prof], sides=18, exp=2.0, cap0="ngon", cap1="ngon")
    sid = b.add(stump, "bark", name="stump", hull_pad=True)
    for k in range(7):
        a = math.radians(20 + k * 51)
        pts = [Vector((math.cos(a) * r * (1.17 - 0.16 * t), math.sin(a) * r * 0.94 * (1.17 - 0.16 * t) * 1.0, top * t))
               for t in np.linspace(0.03, 0.9, 6)]
        pts = [p * (1.0 + 0.012) for p in pts]
        b.add(M.tube(pts, [0.0035] * 6, sides=4, dome0=0.0, dome1=0.0), "bark_dark", name="crack", outline=False,
              covered_by=sid)
    # roots splaying at the base
    for k in range(4):
        a = math.radians(40 + k * 90)
        d = Vector((math.cos(a), math.sin(a), 0))
        b.add(M.tube([d * r * 0.9 + Vector((0, 0, 0.06)), d * r * 1.35 + Vector((0, 0, 0.012)),
                      d * r * 1.6 + Vector((0, 0, -0.004))], [0.032, 0.022, 0.012], sides=8, dome0=0.0, dome1=0.008),
              "bark", name="root", hull_pad=True)
    top_disc = M.cylinder(r * 0.97, r * 0.97, top - 0.004, top + 0.006, sides=18)
    b.add(top_disc, "wood_top", name="cut_top", outline=False, covered_by=sid)
    for rr in (0.35, 0.65):
        ring = M.tube([Vector((math.cos(t) * r * rr, math.sin(t) * r * rr, top + 0.0065))
                       for t in np.linspace(0, 2 * math.pi, 20)], [0.0022] * 20, sides=4, dome0=0.0, dome1=0.0)
        b.add(ring, "rings", name="tree_ring", outline=False, covered_by=sid)
    # anvil: web side profile (horn forward) extruded across 4.6 u
    hw = 2.3 * u
    at = 17.0 * u
    prof2 = [(-6.0, 0.0), (3.4, 0.0), (7.5, 1.0), (3.2, 2.4), (2.2, 5.0), (-2.4, 5.0), (-3.0, 2.4), (-5.6, 1.6)]
    # local: x = along the anvil (+ = horn, → −Y), y = down from the face; build in XZ then map
    outline2 = [(-px * u, at - py * u) for px, py in prof2]      # (Y, Z): horn toward −Y
    outline2 = C.ccw(C.smooth_closed(outline2, 3))
    body = C.plate2d(outline2, 2 * hw, bevel=0.004)          # plate XY = (Y, Z), thickness along its Z = our X
    body.transform(Matrix(((0, 0, 1, 0), (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 0, 1))))
    # the horn tapers to a point across the width too
    body.deform(lambda co: Vector((co.x * (1.0 - 0.75 * max(0.0, (-co.y / u - 3.2) / 4.3)), co.y, co.z)))
    aid = b.add(body, "iron", name="anvil", hull_pad=True)
    face = M.box((hw * 1.9, 9.2 * u, 0.006), (0, 1.3 * u, at + 0.001), bevel=0.0015)
    b.add(face, "iron_face", name="anvil_face", outline=False, covered_by=aid)
    b.add(M.box((hw * 2.3, 6.4 * u, 0.8 * u), (0, 0.2 * u, top + 0.4 * u), bevel=0.003), "iron", name="anvil_base",
          hull_pad=True)
    obj = asset.finish_mesh(b, pal, "npc")
    obj.name = ANVIL
    strike = (0.0, 1.0 * u, at + 0.004)
    return obj, [{"name": "strike", "pos": strike}]


MARKERS = {"SM_UI_QuestMarker_Excl": ("excl", "gold"), "SM_UI_QuestMarker_Quest": ("quest", "gold"),
           "SM_UI_QuestMarker_QuestGrey": ("quest", "grey")}


def build_marker(name: str, pal: Palette):
    """Overhead quest marker (spec §5.4 / §9.4: '!' available, '?' turn-in in #F1C40F, grey '?' #888888 in progress):
    a chunky extruded glyph 34 cm tall, bevelled, inked, pivot at its bottom (socket = ``fx_overhead``; the UE
    actor bobs and billboards it)."""
    kind, reg = MARKERS[name]
    b = M.Builder(name, pal, prefix=f"npcprop.marker_{kind}_{reg}")
    b.regions(**DRESS_REGIONS)
    d = 0.07
    s = 0.34
    if kind == "excl":
        bar = [(-0.045, 0.11), (0.045, 0.11), (0.06, 0.34), (-0.06, 0.34)]
        b.add(C.plate2d([(x, y) for x, y in C.smooth_closed(bar, 2)], d, bevel=0.012).rotate("x", 90), reg,
              name="glyph", hull_pad=True)
        b.add(HB.ell((0.05, d * 0.55, 0.05), (0, 0, 0.045), sides=14, rings=8), reg, name="dot", hull_pad=True)
    else:
        pts = []
        for i in range(15):
            t = math.radians(200 - 250 * i / 14)
            pts.append(Vector((0.085 * math.cos(t), 0.0, 0.255 + 0.075 * math.sin(t))))
        pts += [Vector((0.03, 0, 0.165)), Vector((0.0, 0, 0.13)), Vector((0.0, 0, 0.11))]
        path = HB.catmull(pts, 24)
        b.add(M.sweep(path, M.superellipse(1, 1, 10, 3.0), scales=[(0.034, d * 0.5)] * len(path), up=(0, 1, 0),
                      dome0=0.02, dome1=0.006), reg, name="glyph", hull_pad=True)
        b.add(HB.ell((0.05, d * 0.55, 0.05), (0, 0, 0.045), sides=14, rings=8), reg, name="dot", hull_pad=True)
    del s
    obj = asset.finish_mesh(b, pal, "npc")
    obj.name = name
    return obj

"""Emerald Plains story decorations and lore collectibles (spec art-inventory-ch1.md §6.6, §6.7; web ``Ruins.ts``,
``WarBanner.ts``, ``BrokenAltar.ts``, ``Stonework.ts``, ``LoreProps.ts``; DECISIONS R6 — the broken altar is the
canonical ``story_ep_ancient_well`` mesh, the dry well a separate prop).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/story.py [names…]
        [--quick] [--sheet] [--no-previews]

| Asset | game ids | look |
|---|---|---|
| ``SM_Prop_Story_ElvenRuins`` | ``decor_ruins`` (story_ep_ruined_tower 倒塌的精灵塔) | coursed wall stub ≈ 0.84 × 1.07 m, a 2.25 m pier with an arch spring curving out 0.6 m, a broken column, fallen blocks; stone ``#9A9486``, arch ``#A8A292``, ivy ``#4F8A3A``, carved vine relief |
| ``SM_Prop_Story_GoblinTotem`` | ``decor_war_banner`` (story_ep_goblin_totem 哥布林图腾柱) | crooked pole ``#7A5238`` + crossbar, tattered red banner ``#B8322E`` with a crude ``#F0D8A0`` emblem, skull, lashings ``#C8A870``, hanging bones ``#E6DCC0``, stitched hides ``#7C4F2D``, base stones |
| ``SM_Prop_Story_BrokenAltar`` | ``decor_broken_altar`` (story_ep_ancient_well, R6) | slab ``#5E5864``, coursed body ``#6E6674``, split top ``#847C8A``, rubble, smouldering V-rune ``#FF5A3A`` / ``#FFD0A0``, drips ``#8A2A2A`` |
| ``SM_Prop_Story_DryWell`` | ``decor_dry_well`` (R6: separate prop) | round mossy dry well ≈ 1.3 m, moss ``#6A9A4A``, faint ley-green script ``#9DFFB0`` |
| ``SM_Prop_Lore_AncientTablet`` | ``lore_ancient_tablet`` | sandstone stele ``#B89A70`` ≈ 0.42 × 0.9 m, carved border, glowing golden script |
| ``SM_Prop_Lore_TornJournal`` | ``lore_torn_journal`` | open leather journal ``#7A3E24``, pages ``#F0E0BC``, a torn page lifting, blue quill ``#3A6AB8`` |
| ``SM_Prop_Lore_CrystalShard`` | ``lore_crystal_shard`` | three cyan shards ``#4FB8E8`` / ``#70D8FF`` (0.7 m) with a bright core, rocks ``#6A7080`` / ``#5A6070`` |
| ``SM_Prop_Lore_CarvedStone`` | ``lore_carved_stone`` | boulder ``#8A8E9A`` ≈ 0.6 m, moss cap ``#6A9A4A``, recessed panel with a moon-silver ``#B8D8FF`` spiral glyph |

Story decorations face the camera (``AAbyssZoneActor::BuildMarkers``: front = Blender −Y), take the decor ink and
fade when tall; lore props are interactive (2 px soft ink) with one glowing focal accent each; their light pools
are the runtime ``prop.lore`` decal.
"""
from __future__ import annotations

import math
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import envkit as E  # noqa: E402
import foliage as F  # noqa: E402
import rocks as RK  # noqa: E402
import bmesh  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import scene  # noqa: E402
from kit import mesh as M  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

CAT = "Props"


def _rz(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Z")


def _rx(deg):
    return Matrix.Rotation(math.radians(deg), 4, "X")


def _ry(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Y")


def stone(b: M.Builder, region: str, size, center, rnd: random.Random, tilt: float = 2.0, bevel: float = 0.025,
          chip: bool = False, yaw: float = 0.0, name: str = "block", hull: bool = True):
    """A dressed masonry block: bevelled box, a little out of true; ``chip`` knocks off one top corner."""
    p = M.box(size, (0, 0, 0), bevel=bevel)
    if chip:
        sx, sy, sz = size
        cx, cy = rnd.choice((-1, 1)) * sx / 2, rnd.choice((-1, 1)) * sy / 2
        cut = min(sx, sy) * 0.35
        for v in p.bm.verts:
            d = abs(v.co.x - cx) + abs(v.co.y - cy) + (sz / 2 - v.co.z)
            if d < cut:
                k = (cut - d) / cut
                v.co.z -= k * cut * 0.6
    p.transform(_rz(yaw + rnd.uniform(-tilt, tilt)) @ _rx(rnd.uniform(-tilt, tilt) * 0.5)
                @ _ry(rnd.uniform(-tilt, tilt) * 0.5))
    p.translate(center)
    return b.add(p, region, name=name, hull_pad=hull)


def ivy(b: M.Builder, start: Vector, normal: Vector, length: int, rnd: random.Random, leaf="ivy", leaf_lt="ivy_lt",
        stem="ivy_dk", step: float = 0.075, down: Vector = Vector((0, 0, -1))):
    """A strand of ivy hanging down a face (web ivy()): a stem with alternating leaves."""
    n = normal.normalized()
    side = n.cross(Vector((0, 0, 1))).normalized()
    p = start.copy()
    pts = [p.copy()]
    for i in range(length):
        p = p + down * step + side * rnd.uniform(-0.035, 0.035)
        pts.append(p.copy())
        c = p + side * (0.03 if i % 2 else -0.03) + n * 0.012
        lf = M.ellipsoid((0.045, 0.012, 0.03), (0, 0, 0), sides=7, rings=3)
        lf.transform(Matrix((side, n, Vector((0, 0, 1)))).transposed().to_4x4()
                     @ _ry(35 if i % 2 else -35))
        lf.translate(c)
        b.add(lf, leaf if i % 3 else leaf_lt, name="ivy_leaf", outline=False)
    b.add(E.ribbon(pts, [n] * len(pts), 0.012, lift=0.006, thick=0.006), stem, name="ivy_stem", outline=False)


# ── elven ruins ─────────────────────────────────────────────────────────────────────────────────────────
RUIN_REGIONS = dict(
    stone=Region("#9A9486", .45, .38),
    stone_lt=Region("#A39D8E", .45, .38),
    arch=Region("#A8A292", .45, .40),
    stone_line=Region("#9A9486").line_region(),
    relief=Region("#B4AE9C", .40, .40),
    ivy=Region("#4F8A3A", .40, .30),
    ivy_lt=Region("#6FAA4A", .40, .30),
    ivy_dk=Region("#3A6A2C", .40, .20),
    dark=Region("#3F7A33", .40, .14),
    light=Region("#8CC24F", .38, .12),
)


def build_ruins(pal: Palette):
    name = "SM_Prop_Story_ElvenRuins"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="ruins")
    b.regions(**RUIN_REGIONS)
    # back wall stub ≈ 0.84 × 1.07 m (courses of two/three blocks, broken stepped top), left of the pier
    wx0, wy = -1.05, 0.18
    rows = [(0.0, 0.24), (0.24, 0.22), (0.46, 0.22), (0.68, 0.2), (0.88, 0.19)]
    for k, (z0, hgt) in enumerate(rows):
        widths = [0.42, 0.42] if k % 2 == 0 else [0.24, 0.36, 0.24]
        x = wx0
        for j, w in enumerate(widths):
            if k >= 3 and j == 0 and k == 4:
                x += w
                continue                    # the broken top
            if k == 4 and j == 2:
                break
            stone(b, "stone" if (j + k) % 3 else "stone_lt", (w - 0.012, 0.34, hgt - 0.012),
                  (x + w / 2, wy, z0 + hgt / 2), rnd, chip=(k >= 3), name="wall")
            x += w
    # tall pier ≈ 0.5 × 2.25 m with the arch spring
    px, py = 0.0, 0.05
    z = 0.0
    k = 0
    pier_ids = []
    while z < 2.2:
        hgt = 0.24 if k % 2 == 0 else 0.2
        hgt = min(hgt, 2.25 - z)
        pier_ids.append(stone(b, "stone" if k % 3 else "stone_lt", (0.5 - 0.012, 0.5 - 0.012, hgt - 0.01),
                              (px, py, z + hgt / 2), rnd, tilt=1.2, name="pier"))
        z += hgt
        k += 1
    # carved vine relief up the pier front (−Y face): a lighter raised stem with leaf lobes
    rel = []
    for i in range(9):
        t = i / 8
        rel.append(Vector((px + 0.09 * math.sin(t * 7.0), py - 0.25, 0.25 + 1.75 * t)))
    b.add(E.ribbon(rel, [Vector((0, -1, 0))] * len(rel), 0.03, lift=0.002, thick=0.012), "relief", name="relief",
          outline=False)
    for i in range(1, 8):
        t = i / 8
        c = Vector((px + 0.09 * math.sin(t * 7.0) + (0.06 if i % 2 else -0.06), py - 0.258, 0.25 + 1.75 * t + 0.02))
        lf = M.ellipsoid((0.05, 0.012, 0.028), (0, 0, 0), sides=7, rings=3)
        lf.transform(_ry(40 if i % 2 else -40)).translate(c)
        b.add(lf, "relief", name="relief_leaf", outline=False)
    # arch spring: voussoirs curving out ≈ 0.6 m toward +X at the top
    for i in range(4):
        a0 = math.radians(90 - i * 22)
        a1 = math.radians(90 - (i + 1) * 22)
        R0, R1 = 0.52, 0.85
        cxa, cza = px + 0.62, 1.95
        rings = []
        for a in (a0, (a0 + a1) / 2, a1):
            ca, sa = math.cos(a), math.sin(a)
            rings.append([Vector((cxa - ca * R0, py + y, cza + sa * R0 * 0.9)) for y in (-0.22,)] +
                         [Vector((cxa - ca * R1, py - 0.22, cza + sa * R1 * 0.9)),
                          Vector((cxa - ca * R1, py + 0.22, cza + sa * R1 * 0.9)),
                          Vector((cxa - ca * R0, py + 0.22, cza + sa * R0 * 0.9))])
        part = E.ring_loft(rings, cap0="ngon", cap1="ngon", smooth=False).auto_sharp(40)
        part.translate((0, 0, 0.0))
        if i == 3:
            for v in part.bm.verts:          # broken end
                v.co.z -= 0.04 * (v.co.x - px - 0.5)
        b.add(part, "arch", name="voussoir", hull_pad=True)
    # low front broken column (+X, toward the camera side)
    cx, cy = 0.78, -0.42
    stone(b, "stone", (0.42, 0.42, 0.12), (cx, cy, 0.06), rnd, tilt=1, name="col_base")
    drum_z = 0.12
    for d in range(2):
        hgt = 0.3 if d == 0 else 0.22
        col = M.cylinder(0.15, 0.145, 0.0, hgt, sides=12)
        if d == 1:
            for v in col.bm.verts:
                if v.co.z > hgt - 0.01:
                    v.co.z += 0.09 * math.sin(math.atan2(v.co.y, v.co.x) * 2 + 0.5)
        col.translate((cx, cy, drum_z))
        cid = b.add(col, "stone_lt", name="drum", hull_pad=True)
        for f in range(3):
            a = math.radians(-120 + f * 30)
            p0 = Vector((cx + math.cos(a) * 0.152, cy + math.sin(a) * 0.152, drum_z + 0.03))
            p1 = p0 + Vector((0, 0, hgt - 0.06))
            b.add(E.ribbon([p0, (p0 + p1) / 2, p1], [Vector((math.cos(a), math.sin(a), 0))] * 3, 0.016, lift=0.0,
                           thick=0.002), "stone_line", name="flute", outline=False, covered_by=cid)
        drum_z += hgt + 0.004
    # fallen blocks + rubble
    stone(b, "stone", (0.42, 0.3, 0.24), (-0.42, -0.62, 0.11), rnd, tilt=10, yaw=25, chip=True, name="fallen")
    stone(b, "stone_lt", (0.3, 0.22, 0.18), (0.35, -0.85, 0.08), rnd, tilt=14, yaw=-30, chip=True, name="fallen")
    stone(b, "arch", (0.36, 0.2, 0.2), (-1.25, -0.35, 0.09), rnd, tilt=8, yaw=60, name="fallen")
    for q in range(3):
        a = rnd.uniform(0, math.tau)
        shape = E.RockShape((0.1, 0.08, 0.07), (name, "rub", q), bumps=4, amp=0.08, top_cut=0.3)
        b.add(E.rock(shape, subdiv=1, facet=26.0).translate((math.cos(a) * 0.9, math.sin(a) * 0.6 - 0.3, 0)),
              "stone", name="rubble", hull_pad=True)
    # ivy on the pier and the wall
    ivy(b, Vector((px - 0.12, py - 0.26, 2.2)), Vector((0, -1, 0)), 12, rnd)
    ivy(b, Vector((px + 0.26, py + 0.05, 2.15)), Vector((1, 0, 0)), 9, rnd)
    ivy(b, Vector((wx0 + 0.3, wy - 0.18, 1.0)), Vector((0, -1, 0)), 7, rnd)
    for x, y, h in ((-0.2, -0.32, 0.26), (0.55, -0.15, 0.2), (-0.9, -0.12, 0.22)):
        F.tuft(b, Vector((x, y, 0)), h, 8, rnd, segs=2, inked=True)
    return E.finish(b, pal, "decor", ground_h=1.4, name=name)


# ── goblin war totem ────────────────────────────────────────────────────────────────────────────────────
TOTEM_REGIONS = dict(
    wood=Region("#7A5238", .42, .30),
    wood_line=Region("#7A5238").line_region(),
    cloth=Region("#B8322E", .42, .35),
    cloth_dk=Region("#8A2420", .42, .25),
    paint=Region("#F0D8A0", .35, .30),
    bone=Region("#E6DCC0", .40, .30),
    bone_dk=Region("#3A2A24", .40, .20),
    rope=Region("#C8A870", .38, .30),
    hide=Region("#7C4F2D", .42, .32),
    hide_lt=Region("#9A6A42", .40, .30),
    stitch=Region("#E0C890", .35, .30),
    stone=Region("#8A8E98", .42, .35),
    feather=Region("#2A2430", .40, .25),
)


def skull(b: M.Builder, c: Vector, s: float, yaw: float = 0.0, region="bone", dark="bone_dk", name="skull"):
    """A cartoon skull (cranium, cheek, jaw, dark sockets) facing −Y, scale ``s`` (≈ cranium radius)."""
    rot = _rz(yaw)
    cr = M.ellipsoid((s, s * 0.95, s * 0.9), (0, 0, s * 0.25), sides=12, rings=7)
    jaw = M.ellipsoid((s * 0.62, s * 0.55, s * 0.32), (0, -s * 0.28, -s * 0.45), sides=10, rings=5)
    for p in (cr, jaw):
        p.transform(rot).translate(c)
    sid = b.add(cr, region, name=name, hull_pad=True)
    b.add(jaw, region, name=name + "_jaw")
    for sx in (-1, 1):
        eye = M.ellipsoid((s * 0.27, s * 0.12, s * 0.3), (sx * s * 0.36, -s * 0.83, s * 0.15), sides=8, rings=4)
        eye.transform(rot).translate(c)
        b.add(eye, dark, name="socket", outline=False, covered_by=sid)
    nose = M.ellipsoid((s * 0.1, s * 0.08, s * 0.16), (0, -s * 0.9, -s * 0.18), sides=6, rings=3)
    nose.transform(rot).translate(c)
    b.add(nose, dark, name="nose", outline=False, covered_by=sid)
    return sid


def bone(b: M.Builder, a: Vector, e: Vector, r: float, region="bone", name="bone"):
    pid = b.add(M.tube([a, a.lerp(e, 0.5), e], [r, r * 0.8, r], sides=7), region, name=name)
    d = (e - a).normalized()
    side = d.cross(Vector((0, 0, 1)))
    if side.length < 1e-3:
        side = Vector((1, 0, 0))
    side.normalize()
    for p in (a, e):
        for s in (-1, 1):
            b.add(M.ellipsoid((r * 1.15, r * 1.15, r * 1.15), p + side * s * r * 0.8, sides=7, rings=4), region,
                  name=name + "_knob")
    return pid


def build_totem(pal: Palette):
    name = "SM_Prop_Story_GoblinTotem"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="totem")
    b.regions(**TOTEM_REGIONS)
    H = 2.6
    # crooked pole
    pts = [Vector((0, 0, -0.05)), Vector((0.03, 0.01, 0.7)), Vector((-0.02, 0.0, 1.5)), Vector((0.05, -0.01, 2.2)),
           Vector((0.02, 0.0, H - 0.2))]
    pid = b.add(M.tube(pts, [0.075, 0.068, 0.062, 0.056, 0.05], sides=9), "wood", name="pole", hull_pad=True)
    # crossbar
    zc = 2.3
    b.add(M.tube([Vector((-0.42, -0.04, zc - 0.03)), Vector((0.0, -0.06, zc)), Vector((0.45, -0.04, zc + 0.04))],
                 [0.04, 0.045, 0.038], sides=7), "wood", name="bar")
    # tattered banner hanging from the bar (front −Y): ragged lower edge
    w0, L = 0.82, 1.05
    edge = [(0.0, 0.0), (w0, 0.04)]
    rag = [(w0 - 0.04, -L + 0.0), (w0 * 0.8, -L + 0.17), (w0 * 0.66, -L - 0.05), (w0 * 0.5, -L + 0.15),
           (w0 * 0.33, -L + 0.0), (w0 * 0.18, -L + 0.2), (0.03, -L + 0.05)]
    outline_pts = edge + rag
    cloth = M.plate(list(reversed(outline_pts)), 0.02, bevel=0.004)
    for v in cloth.bm.verts:
        v.co.z += 0.045 * math.sin(math.pi * v.co.x / w0) + 0.02 * math.sin(v.co.y * 6)
    cloth.transform(_rx(90)).translate((-0.41, -0.1, zc - 0.03))
    cid = b.add(cloth, "cloth", name="banner", hull_pad=True)
    # crude painted emblem (front face): a "Y" of thick strokes + a ring
    off = lambda x: -0.1 - 0.011 - 0.045 * math.sin(math.pi * (x + 0.41) / w0)  # noqa: E731
    strokes = [[(-0.2, zc - 0.25), (0.0, zc - 0.52), (0.2, zc - 0.25)], [(0.0, zc - 0.52), (0.0, zc - 0.82)]]
    for s in strokes:
        pts3 = [Vector((x, off(x), z)) for x, z in s]
        b.add(E.ribbon(pts3, [Vector((0, -1, 0))] * len(pts3), 0.07, lift=0.002, thick=0.003), "paint",
              name="emblem", outline=False, covered_by=cid)
    # skull on top
    skull(b, Vector((0.02, -0.02, H - 0.08)), 0.13)
    # lashings
    for z in (1.2, 1.28, zc - 0.08, zc + 0.08):
        b.add(M.belt(0.068, 0.068, z, 0.04, 0.014, sides=10), "rope", name="lash", outline=False, covered_by=pid)
    # hanging bone + bone string on the left end of the bar
    b.add(M.tube([Vector((-0.4, -0.04, zc - 0.04)), Vector((-0.42, -0.04, zc - 0.3))], [0.008, 0.008], sides=5),
          "rope", name="string")
    bone(b, Vector((-0.42, -0.04, zc - 0.32)), Vector((-0.41, -0.04, zc - 0.52)), 0.022)
    b.add(M.tube([Vector((0.43, -0.04, zc + 0.03)), Vector((0.45, -0.04, zc - 0.25))], [0.008, 0.008], sides=5),
          "rope", name="string")
    for q in range(3):
        c = Vector((0.45, -0.04, zc - 0.29 - q * 0.08))
        b.add(M.ellipsoid((0.03, 0.03, 0.035), c, sides=7, rings=4), "bone" if q % 2 == 0 else "bone_dk",
              name="bead")
    # feathers on the bar end
    for q in range(2):
        b.add(E.blade(Vector((-0.44, -0.05, zc)), (-0.3 + q * 0.2, -0.2, -1.0), 0.22, 0.05, bend=0.15),
              "feather", name="feather")
    # stitched hide wrapped round the pole mid-height
    hide = M.loft_z([(0.75, 0.11, 0.1), (0.95, 0.12, 0.11), (1.12, 0.1, 0.095)], sides=10, cap0=None, cap1=None)
    hide = E.ring_loft([[Vector((math.cos(a) * r, math.sin(a) * r * 0.92, z)) for a in
                         (math.tau * i / 10 for i in range(10))] for z, r in ((0.72, 0.105), (0.95, 0.12), (1.14, 0.1))],
                       cap0="ngon", cap1="ngon")
    hid = b.add(hide, "hide", name="hide", hull_pad=True)
    for q in range(5):
        z = 0.78 + q * 0.08
        a0 = -1.9
        p0 = Vector((math.cos(a0) * 0.115, math.sin(a0) * 0.115, z))
        p1 = Vector((math.cos(a0 + 0.35) * 0.115, math.sin(a0 + 0.35) * 0.115, z + 0.03))
        b.add(E.ribbon([p0, (p0 + p1) / 2, p1], [p0.normalized(), ((p0 + p1) / 2).normalized(), p1.normalized()],
                       0.012, lift=0.008, thick=0.003), "stitch", name="stitch", outline=False, covered_by=hid)
    # hide flap hanging on the right of the pole
    flap = M.plate([(0.0, 0.0), (0.0, -0.32), (0.08, -0.38), (0.16, -0.3), (0.18, 0.0)][::-1], 0.016, bevel=0.004)
    flap.transform(_rx(90) @ _rz(0)).translate((0.06, -0.08, 1.35))
    b.add(flap, "hide_lt", name="flap", hull_pad=True)
    # base stones
    for q, (x, y, r) in enumerate(((-0.17, -0.06, 0.12), (0.15, -0.1, 0.1), (0.02, 0.16, 0.11), (-0.1, 0.14, 0.08))):
        shape = E.RockShape((r, r * 0.9, r * 0.8), (name, "base", q), bumps=4, amp=0.08, top_cut=0.3)
        b.add(E.rock(shape, subdiv=1, facet=26.0).translate((x, y, 0)), "stone", name="stone", hull_pad=True)
    return E.finish(b, pal, "decor", ground_h=1.2, name=name)


# ── broken altar ────────────────────────────────────────────────────────────────────────────────────────
ALTAR_REGIONS = dict(
    slab=Region("#5E5864", .45, .38),
    body=Region("#6E6674", .45, .38),
    body_line=Region("#6E6674").line_region(),
    top=Region("#847C8A", .45, .40),
    rune=Region("#FF5A3A", .0, .0, e=1.0),
    rune_core=Region("#FFD0A0", .0, .0, e=1.0),
    ember=Region("#FFB070", .0, .0, e=1.0),
    drip=Region("#8A2A2A", .40, .30),
    dark=Region("#3F7A33", .40, .14),
    light=Region("#8CC24F", .38, .12),
)


def build_altar(pal: Palette):
    name = "SM_Prop_Story_BrokenAltar"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="altar")
    b.regions(**ALTAR_REGIONS)
    stone(b, "slab", (1.24, 0.72, 0.26), (0, 0.0, 0.13), rnd, tilt=0.6, name="slab")
    # coursed body ≈ 0.88 × 0.56 m in two courses of blocks
    body_ids = []
    for k, (z0, h) in enumerate(((0.26, 0.28), (0.54, 0.28))):
        ws = [0.44, 0.44] if k == 0 else [0.3, 0.3, 0.28]
        x = -0.44
        for j, w in enumerate(ws):
            body_ids.append(stone(b, "body", (w - 0.012, 0.52, h - 0.012), (x + w / 2, 0.0, z0 + h / 2), rnd, tilt=1.0,
                                  name="body"))
            x += w
    # split top slab (two halves, one tilted)
    stone(b, "top", (0.52, 0.66, 0.14), (-0.28, 0.0, 0.89), rnd, tilt=1.5, chip=True, name="top")
    s2 = M.box((0.5, 0.64, 0.14), (0, 0, 0), bevel=0.02)
    s2.transform(_ry(-7) @ _rz(4)).translate((0.29, 0.02, 0.88))
    b.add(s2, "top", name="top2", hull_pad=True)
    # smouldering V-rune on the front (−Y) of the body + its ember dot (focal accent)
    fy = -0.268
    vpts = [Vector((-0.17, fy, 0.74)), Vector((-0.05, fy, 0.46)), Vector((0.07, fy, 0.74))]
    for seg in ((vpts[0], vpts[1]), (vpts[1], vpts[2])):
        p0, p1 = seg
        b.add(E.ribbon([p0, (p0 + p1) / 2, p1], [Vector((0, -1, 0))] * 3, 0.055, lift=0.0, thick=0.006), "rune",
              name="rune", outline=False)
        b.add(E.ribbon([p0, (p0 + p1) / 2, p1], [Vector((0, -1, 0))] * 3, 0.02, lift=0.006, thick=0.003),
              "rune_core", name="rune_core", outline=False)
    b.add(M.ellipsoid((0.035, 0.012, 0.035), (-0.05, fy - 0.004, 0.83), sides=8, rings=4), "ember", name="ember",
          outline=False)
    # blood / wax drips down the top edge
    for x in (0.2, 0.33):
        b.add(E.ribbon([Vector((x, -0.33, 0.95)), Vector((x, -0.335, 0.86)), Vector((x + 0.005, -0.335, 0.74))],
                       [Vector((0, -1, 0))] * 3, [0.03, 0.025, 0.015], lift=0.0, thick=0.008), "drip", name="drip",
              outline=False)
    # rubble
    for q, (x, y, r) in enumerate(((0.78, -0.2, 0.14), (-0.82, 0.12, 0.11), (0.6, 0.42, 0.09))):
        shape = E.RockShape((r, r * 0.85, r * 0.7), (name, "rub", q), bumps=4, amp=0.08, top_cut=0.3)
        b.add(E.rock(shape, subdiv=1, facet=26.0).translate((x, y, 0)), "body", name="rubble", hull_pad=True)
    F.tuft(b, Vector((-0.66, -0.42, 0)), 0.22, 7, rnd, segs=2, inked=True)
    return E.finish(b, pal, "decor", ground_h=0.9, name=name)


# ── dry well (R6 separate prop) ─────────────────────────────────────────────────────────────────────────
WELL_REGIONS = dict(
    stone=Region("#8C8A80", .45, .36),
    stone_lt=Region("#9C9A8E", .45, .38),
    moss=Region("#6A9A4A", .40, .30),
    inner=Region("#2A2630", .30, .10),
    leaves=Region("#A8823A", .40, .30),
    script=Region("#9DFFB0", .0, .0, e=0.75),
    dark=Region("#3F7A33", .40, .14),
    light=Region("#8CC24F", .38, .12),
)


def build_drywell(pal: Palette):
    name = "SM_Prop_Story_DryWell"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="drywell")
    b.regions(**WELL_REGIONS)
    import camp as C
    R0, R1, H = 0.66, 0.5, 0.62
    ids = []
    for course in range(3):
        n = 10
        h = H / 3
        for i in range(n):
            if course == 2 and i in (3, 4):
                continue                         # a broken gap in the top course
            a0 = math.tau * (i + 0.5 * course) / n
            a1 = a0 + math.tau / n * 0.94
            blk = C._block(R1, R0 + rnd.uniform(-0.01, 0.02), a0, a1, course * h, (course + 1) * h - 0.012)
            ids.append(b.add(blk, "stone" if (i + course) % 3 else "stone_lt", name="block", hull_pad=True))
    # dark empty shaft + dry leaves at the bottom
    b.add(M.cylinder(R1 + 0.005, R1 + 0.005, -0.02, H - 0.25, sides=18), "inner", name="shaft", outline=False)
    for q in range(6):
        a = rnd.uniform(0, math.tau)
        rr = rnd.uniform(0, 0.32)
        b.add(M.ellipsoid((0.05, 0.03, 0.008), (math.cos(a) * rr, math.sin(a) * rr, H - 0.245), sides=6, rings=3),
              "leaves", name="leaf", outline=False)
    # moss draped over the rim in patches
    for q in range(4):
        a0 = rnd.uniform(0, math.tau)
        pts, nrm = [], []
        for t in range(6):
            a = a0 + t * 0.12
            pts.append(Vector((math.cos(a) * (R0 + R1) / 2, math.sin(a) * (R0 + R1) / 2, H + 0.002)))
            nrm.append(Vector((0, 0, 1)))
        b.add(E.ribbon(pts, nrm, 0.13, lift=0.0, thick=0.02), "moss", name="moss", outline=False)
    # faint ley-green elven script round the outer face (front half)
    for k in range(7):
        a = math.radians(-160 + k * 22)
        c = Vector((math.cos(a) * (R0 + 0.006), math.sin(a) * (R0 + 0.006), H * 0.5))
        n = Vector((math.cos(a), math.sin(a), 0))
        t = n.cross(Vector((0, 0, 1)))
        glyph = [c + t * 0.04 + Vector((0, 0, 0.06)), c, c - t * 0.04 + Vector((0, 0, 0.06))] if k % 2 else \
                [c + Vector((0, 0, 0.07)), c - Vector((0, 0, 0.06)), c - Vector((0, 0, 0.06)) + t * 0.04]
        b.add(E.ribbon(glyph, [n] * 3, 0.016, lift=0.002, thick=0.003), "script", name="script", outline=False)
    # fallen stones of the gap + tufts
    for q, (x, y) in enumerate(((0.62, 0.62), (0.9, 0.2))):
        shape = E.RockShape((0.14, 0.11, 0.1), (name, "fallen", q), bumps=4, amp=0.08, top_cut=0.35)
        b.add(E.rock(shape, subdiv=1, facet=24.0).translate((x, y, 0)), "stone", name="fallen", hull_pad=True)
    F.tuft(b, Vector((-0.6, -0.5, 0)), 0.26, 8, rnd, segs=2, inked=True)
    F.tuft(b, Vector((0.55, -0.55, 0)), 0.2, 7, rnd, segs=2, inked=True)
    return E.finish(b, pal, "decor", ground_h=0.8, name=name)


# ── lore collectibles (interactive ink) ─────────────────────────────────────────────────────────────────
LORE_REGIONS = dict(
    sandstone=Region("#B89A70", .42, .35),
    sandstone_line=Region("#B89A70").line_region(),
    sandstone_sh=Region("#B89A70").shade_region(.3, .15),
    script=Region("#E0901A", .0, .0, e=1.0),
    script_core=Region("#FFF2B0", .0, .0, e=1.0),
    leather=Region("#7A3E24", .42, .30),
    leather_dk=Region("#5A2A18", .42, .25),
    paper=Region("#F0E0BC", .35, .45),
    ink=Region("#8A6A4A", .40, .25),
    quill=Region("#3A6AB8", .42, .40),
    quill_dk=Region("#1E2E5A", .40, .25),
    crystal=Region("#4FB8E8", .30, .45, e=0.25),
    crystal_lt=Region("#70D8FF", .30, .45, e=0.35),
    crystal_core=Region("#F0FFFF", .0, .0, e=1.0),
    rock_a=Region("#6A7080", .42, .35),
    rock_b=Region("#5A6070", .42, .35),
    boulder=Region("#8A8E9A", .42, .35),
    boulder_line=Region("#8A8E9A").line_region(),
    panel=Region("#6E7280", .40, .25),
    moss=Region("#6A9A4A", .40, .30),
    glyph=Region("#B8D8FF", .0, .0, e=1.0),
    glyph_core=Region("#F0F8FF", .0, .0, e=1.0),
    lichen=Region("#C8D070", .35, .30),
    dark=Region("#4F7A2A", .40, .14),
    light=Region("#8AB84A", .38, .12),
)


def new_lore(pal, name):
    b = M.Builder(name, pal, prefix="lore")
    b.regions(**LORE_REGIONS)
    return b, scene.rng(name)


def build_tablet(pal: Palette):
    name = "SM_Prop_Lore_AncientTablet"
    b, rnd = new_lore(pal, name)
    W, H, T = 0.42, 0.9, 0.12
    # stele: flat front (−Y), rounded top, chipped right shoulder
    outline = []
    for i in range(13):
        a = math.pi * i / 12
        outline.append((W / 2 * math.cos(a), H - W / 2 + W / 2 * math.sin(a) * 0.9))
    outline = [(W / 2, 0.0)] + outline + [(-W / 2, 0.0)]
    outline = [(x if not (0.1 < x and 0.55 < y < 0.68) else x - 0.05, y) for x, y in outline]
    st = M.plate(outline, T, bevel=0.018)
    st.transform(_rx(90)).translate((0, 0, 0))
    sid = b.add(st, "sandstone", name="stele", hull_pad=True)
    # carved border (shade line) and the chip / crack (line tone)
    fy = -T / 2 - 0.001
    border = [Vector((-W / 2 + 0.05, fy, 0.06)), Vector((-W / 2 + 0.05, fy, H - 0.2))]
    for i in range(9):
        a = math.pi - math.pi * i / 8
        border.append(Vector(((W / 2 - 0.05) * math.cos(a), fy, H - W / 2 + (W / 2 - 0.05) * math.sin(a) * 0.9)))
    border.append(Vector((W / 2 - 0.05, fy, 0.06)))
    b.add(E.ribbon(border, [Vector((0, -1, 0))] * len(border), 0.016, lift=0.0, thick=0.003), "sandstone_sh",
          name="border", outline=False, covered_by=sid)
    b.add(E.ribbon([Vector((-W / 2 + 0.01, fy, 0.38)), Vector((-W / 2 + 0.07, fy, 0.34)),
                    Vector((-W / 2 + 0.05, fy, 0.24))], [Vector((0, -1, 0))] * 3, 0.012, lift=0.0, thick=0.003),
          "sandstone_line", name="crack", outline=False, covered_by=sid)
    # glowing golden script: 4 lines of zig-zag glyphs (focal accent)
    for i in range(4):
        z = H - 0.24 - i * 0.13
        w = 0.24 - (0.08 if i == 3 else 0.0)
        pts = [Vector((-0.12 + w * k / 6, fy - 0.001, z + (0.02 if k % 2 else -0.012) * (0.7 + rnd.random() * 0.4)))
               for k in range(7)]
        b.add(E.ribbon(pts, [Vector((0, -1, 0))] * len(pts), 0.026, lift=0.0, thick=0.003), "script",
              name="script", outline=False, covered_by=sid)
        b.add(E.ribbon(pts, [Vector((0, -1, 0))] * len(pts), 0.009, lift=0.003, thick=0.002), "script_core",
              name="script_core", outline=False, covered_by=sid)
    F.tuft(b, Vector((-0.24, -0.08, 0)), 0.16, 6, rnd, segs=2, inked=True)
    F.tuft(b, Vector((0.24, -0.04, 0)), 0.12, 5, rnd, segs=2, inked=True)
    return E.finish(b, pal, "interactive", ground_h=0.6, name=name)


def build_journal(pal: Palette):
    name = "SM_Prop_Lore_TornJournal"
    b, rnd = new_lore(pal, name)
    # open cover lying on the ground (two boards hinged at the spine along Y)
    for s in (-1, 1):
        cov = M.box((0.2, 0.29, 0.022), (s * 0.105, 0, 0.013), bevel=0.006)
        cov.transform(_ry(s * 4))
        b.add(cov, "leather", name="cover", hull_pad=True)
    b.add(M.tube([(0, -0.145, 0.018), (0, 0.145, 0.018)], [0.016, 0.016], sides=8), "leather_dk", name="spine")
    # page blocks curling up toward the spine
    for s in (-1, 1):
        rows = []
        for j in range(5):
            t = j / 4
            x = s * (0.012 + 0.18 * t)
            z = 0.03 + 0.045 * (1 - t) ** 2
            rows.append([Vector((x, y, z)) for y in (-0.13, 0.0, 0.13)])
        bm = bmesh.new()
        vv = [[bm.verts.new(p) for p in row] for row in rows]
        for j in range(4):
            for i in range(2):
                bm.faces.new((vv[j][i], vv[j][i + 1], vv[j + 1][i + 1], vv[j + 1][i]))
        bm.normal_update()
        bmesh.ops.solidify(bm, geom=list(bm.faces), thickness=0.018)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        pg = M.Part(bm).smooth(True)
        pid = b.add(pg, "paper", name="pages", hull_pad=True)
        for q in range(3):
            y = -0.07 + q * 0.06
            pts = [Vector((s * (0.04 + 0.12 * t), y + 0.004 * math.sin(t * 9), 0.03 + 0.045 * (1 - (0.15 + 0.7 * t)) ** 2 + 0.003))
                   for t in (0.15, 0.5, 0.85)]
            b.add(E.ribbon(pts, [Vector((0, 0, 1))] * 3, 0.008, lift=0.0, thick=0.002), "ink", name="writing",
                  outline=False, covered_by=pid)
    # the torn page lifting off (curled sheet over the right page)
    rows = []
    for j in range(4):
        t = j / 3
        rows.append([Vector((0.06 + 0.15 * t, y, 0.07 + 0.11 * t * t)) for y in (-0.03, 0.06)])
    bm = bmesh.new()
    vv = [[bm.verts.new(p) for p in row] for row in rows]
    for j in range(3):
        bm.faces.new((vv[j][0], vv[j][1], vv[j + 1][1], vv[j + 1][0]))
    bm.normal_update()
    bmesh.ops.solidify(bm, geom=list(bm.faces), thickness=0.006)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    tp = M.Part(bm).smooth(True)
    tp.transform(_rz(-20))
    b.add(tp, "paper", name="torn", hull_pad=True)
    # blue quill (colour pop) lying across the left page
    a, e = Vector((-0.27, -0.12, 0.03)), Vector((-0.04, 0.1, 0.075))
    vane = E.blade(a + (e - a) * 0.25, (e - a).normalized(), (e - a).length * 0.8, 0.05, bend=0.0,
                   side=Vector((0, 0, 1)).cross(e - a).normalized())
    b.add(vane, "quill", name="quill")
    b.add(M.tube([a, e], [0.005, 0.004], sides=5), "quill_dk", name="shaft")
    return E.finish(b, pal, "interactive", ground_h=0.2, name=name)


def shard(b: M.Builder, base: Vector, h: float, r: float, lean: Vector, region: str, core: str | None, name="shard"):
    """A faceted crystal shard (hexagonal prism with a pyramid tip) breaking out of the ground."""
    up = (Vector((0, 0, 1)) + lean).normalized()
    rot = Vector((0, 0, 1)).rotation_difference(up).to_matrix().to_4x4()
    rings = []
    for z, k in ((-0.05, 1.0), (h * 0.72, 0.92)):
        rings.append([Vector((math.cos(a) * r * k, math.sin(a) * r * k * 0.8, z)) for a in
                      (math.tau * i / 6 + 0.3 for i in range(6))])
    p = E.ring_loft(rings, cap0="ngon", cap1=None, smooth=False)
    top = [v for v in p.bm.verts if v.co.z > h * 0.5]
    top.sort(key=lambda v: math.atan2(v.co.y, v.co.x))
    apex = p.bm.verts.new((r * 0.1, 0, h))
    for i in range(len(top)):
        p.bm.faces.new((top[i], top[(i + 1) % len(top)], apex))
    p.recalc_normals()
    p.flat()
    p.transform(rot).translate(base)
    sid = b.add(p, region, name=name)
    if core:
        c = M.ellipsoid((r * 0.22, r * 0.22, h * 0.32), (0, 0, h * 0.42), sides=6, rings=4)
        c.transform(rot).translate(base + up * 0.0 - Vector((0, r * 0.75, 0)))
        b.add(c, core, name="core", outline=False)
    return sid


def build_crystal(pal: Palette):
    name = "SM_Prop_Lore_CrystalShard"
    b, rnd = new_lore(pal, name)
    shard(b, Vector((0.0, 0.02, 0)), 0.7, 0.1, Vector((0.05, 0.02, 0)), "crystal_lt", "crystal_core", name="big")
    shard(b, Vector((-0.17, 0.0, 0)), 0.38, 0.075, Vector((-0.35, -0.05, 0)), "crystal", None)
    shard(b, Vector((0.17, -0.04, 0)), 0.44, 0.08, Vector((0.32, -0.1, 0)), "crystal", None)
    shard(b, Vector((0.05, 0.16, 0)), 0.26, 0.06, Vector((0.1, 0.35, 0)), "crystal", None)
    for q, (x, y, r, reg) in enumerate(((-0.2, -0.14, 0.12, "rock_a"), (0.2, -0.18, 0.1, "rock_b"),
                                         (-0.05, 0.2, 0.09, "rock_a"))):
        shape = E.RockShape((r, r * 0.85, r * 0.7), (name, q), bumps=4, amp=0.08, top_cut=0.3)
        b.add(E.rock(shape, subdiv=1, facet=24.0).translate((x, y, 0)), reg, name="rock", hull_pad=True)
    return E.finish(b, pal, "interactive", ground_h=0.4, name=name)


def build_carved(pal: Palette):
    name = "SM_Prop_Lore_CarvedStone"
    b, rnd = new_lore(pal, name)
    shape = E.RockShape((0.42, 0.32, 0.36), (name, "body"), bumps=5, amp=0.06, top_cut=0.15)
    body = E.rock(shape, subdiv=2, facet=None)
    # flatten the front (−Y) into the recessed panel face
    for v in body.bm.verts:
        if v.co.y < -0.24 and 0.12 < v.co.z < 0.5:
            v.co.y = -0.24 - (v.co.y + 0.24) * 0.25
    body.recalc_normals()
    sid = b.add(body, "boulder", name="boulder", hull_pad=True)
    cap = E.rock_cap(shape, E.drip_edge(0.55, (name, "cap"), drips=2, depth=0.35), thick=0.025, cols=22, rows=4)
    b.add(cap, "moss", name="moss", hull_pad=True)
    # recessed panel + spiral glyph (focal accent)
    pc = Vector((0.0, -0.245, 0.3))
    panel = M.cylinder(0.16, 0.16, 0.0, 0.006, sides=16)
    panel.transform(_rx(90)).scale((1.0, 1.0, 0.82), pivot=(0, 0, 0)).translate(pc)
    b.add(panel, "panel", name="panel", outline=False, covered_by=sid)
    pts = []
    for k in range(27):
        t = k / 26
        a = t * math.pi * 3.4
        rr = 0.012 + t * 0.12
        pts.append(pc + Vector((math.cos(a) * rr, -0.006, math.sin(a) * rr * 0.82)))
    b.add(E.ribbon(pts, [Vector((0, -1, 0))] * len(pts), 0.022, lift=0.0, thick=0.003), "glyph", name="glyph",
          outline=False, covered_by=sid)
    b.add(E.ribbon(pts, [Vector((0, -1, 0))] * len(pts), 0.008, lift=0.003, thick=0.002), "glyph_core",
          name="glyph_core", outline=False, covered_by=sid)
    RK.crack(b, shape, Vector((0, 0, 0)), 0.0, math.radians(-20), 0.45, 0.12, "boulder_line", sid, rnd, 0.016)
    b.add(M.ellipsoid((0.05, 0.035, 0.01), (0.3, -0.12, 0.42), sides=7, rings=3), "lichen", name="lichen",
          outline=False)
    F.tuft(b, Vector((-0.42, -0.12, 0)), 0.15, 6, rnd, segs=2, inked=True)
    return E.finish(b, pal, "interactive", ground_h=0.5, name=name)


# ── registry ────────────────────────────────────────────────────────────────────────────────────────────
ASSETS: dict = {
    "SM_Prop_Story_ElvenRuins": (build_ruins, dict(game_ids=["decor_ruins"], footprint=[2, 1], blocking=True,
                                                   blob=1.1)),
    "SM_Prop_Story_GoblinTotem": (build_totem, dict(game_ids=["decor_war_banner"], footprint=[1, 1], blocking=True,
                                                    blob=0.35)),
    "SM_Prop_Story_BrokenAltar": (build_altar, dict(game_ids=["decor_broken_altar"], footprint=[1, 1], blocking=True,
                                                    blob=0.7)),
    "SM_Prop_Story_DryWell": (build_drywell, dict(game_ids=["decor_dry_well"], footprint=[1, 1], blocking=True,
                                                  blob=0.75)),
    "SM_Prop_Lore_AncientTablet": (build_tablet, dict(game_ids=["lore_ancient_tablet"], footprint=[1, 1],
                                                      blocking=False, blob=0.3)),
    "SM_Prop_Lore_TornJournal": (build_journal, dict(game_ids=["lore_torn_journal"], footprint=[1, 1],
                                                     blocking=False, blob=0.3)),
    "SM_Prop_Lore_CrystalShard": (build_crystal, dict(game_ids=["lore_crystal_shard"], footprint=[1, 1],
                                                      blocking=False, blob=0.3)),
    "SM_Prop_Lore_CarvedStone": (build_carved, dict(game_ids=["lore_carved_stone"], footprint=[1, 1],
                                                    blocking=False, blob=0.4)),
}
NOTES = {
    "story": "story decoration: faces the camera (front -Y), tall decor fade; inspect via its interaction radius "
             "(world-map-nav 12.2); footprint/blocking recorded for W5 (statue-like props block)",
    "lore": "lore collectible (quests-story-ch1 10.4): interactive ink 2 px line tone, glowing focal accent; the "
            "pulsing light pool is the runtime prop.lore decal (colour from the lore data)",
}


def main(argv: list[str]) -> int:
    quick = "--quick" in argv
    sheet = "--sheet" in argv
    previews = "--no-previews" not in argv
    names = [a for a in argv if not a.startswith("--")] or list(ASSETS)
    names = [n for n in ASSETS if any(n == a or a in n for a in names)]
    scene.reset()
    pal = Palette(E.PROPS)
    if sheet or quick:
        objs = [ASSETS[n][0](pal) for n in names]
        pal.save()
        if sheet:
            E.sheet(objs, "sheet_story_" + "_".join(n.split("_")[-1][:6] for n in names)[:80], cols=3)
        else:
            E.quick(objs, "story", spacing=2.6)
        return 0
    shipped = []
    for n in names:
        scene.reset()
        pal = Palette(E.PROPS)
        fn, kw = ASSETS[n]
        obj = fn(pal)
        pal.save()
        extra = {"displayName": n.split("_", 2)[-1], "family": "environment.story",
                 "note": NOTES["lore" if "Lore" in n else "story"]}
        E.ship(obj, n, CAT, pal, obj["af_outline_class"], extra=extra, previews=previews, close_facing="front", **kw)
        shipped.append(n)
    if previews and len(shipped) > 1:
        scene.reset()
        pal = Palette(E.PROPS)
        objs = [ASSETS[n][0](pal) for n in shipped]
        E.family_sheet(objs, "story_lore.png", "PLAINS STORY DECOR AND LORE", cols=3, half_w=0.07, focus_z=0.7)
    return E.summary()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

"""Camp kit and stake palisade (spec art-inventory-ch1.md §6.3, §6.4; web ``CampProps.ts``, ``OutcropPainter.ts``
stakes, ``TowerProps.ts`` hearthstone; camp theme ``src/data/camp-themes.ts`` plains; DECISIONS R1, W5, Q3).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/camp.py [names…]
        [--quick] [--no-previews]

Placement (camp layout v2, DECISIONS W11: core table ``abyss/world/CampLayout.h``; ``AAbyssZoneActor::BuildCamps /
BuildWalls``): camp props stand at sub-tile positions; tents, the well, barrels and crates turn their front (Blender
−Y) to the camp centre, the rest face the camera. Three tents per camp, NPCs 0.73–0.76 m in front of their doors. Palisade: one stake mesh per spot (centre + 0.2 / 0.4 toward every camp-wall neighbour, random yaw) —
``SM_Palisade_Plains_Stake_A/B/C`` (1.32 / 1.40 / 1.48 m incl. the tip, mean 1.4 m, R1); gate posts ``SM_Palisade_Plains_GatePost`` at
both sides of the 2-tile gate. ``_Straight`` / ``_Corner`` are the same stakes pre-assembled into 1-tile runs for
hand-dressed scenes (level design; the runtime builds walls from single stakes).

Blocking (W5, W11 + the core's ``CampPropBlockedTiles``): tents, the well, barrels and crates block their anchor tile
and every tile whose centre lies within their body (tent canvas; the others' bounds) grown by 0.3 tile; campfire,
banners, torches, the hearthstone and the gate posts do not. ``footprintTiles`` records the body's nominal tiles (tent 2 × 2; see the tent's ``footprintNote``).
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
import bmesh  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402

from kit import scene  # noqa: E402
from kit import mesh as M  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

CAT = "Props"


def _rx(deg):
    return Matrix.Rotation(math.radians(deg), 4, "X")


def _ry(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Y")


def _rz(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Z")


def grain(b: M.Builder, region: str, p0, p1, normal, width: float, pid: int, lift: float = 0.002):
    """A painted line (wood grain, seam, plank joint) lying on a flat face."""
    p0, p1, n = Vector(p0), Vector(p1), Vector(normal).normalized()
    pts = [p0.lerp(p1, t) for t in (0.0, 0.5, 1.0)]
    b.add(E.ribbon(pts, [n] * 3, [width * 0.6, width, width * 0.6], lift=lift, thick=0.002), region,
          name="grain", outline=False, covered_by=pid)


# ── palisade ────────────────────────────────────────────────────────────────────────────────────────────
PAL_REGIONS = dict(
    wood=Region("#9A6C3E", .34, .26),
    wood_dk=Region("#8E6338", .34, .26),
    wood_line=Region("#9A6C3E").line_region(),
    band=Region("#C9A868", .36, .30),
    ivy=Region("#7CAA48", .38, .30),
    pennant=Region("#3F8A36", .38, .30),
    cap=Region("#B08450", .34, .30),
)
# Overall heights **including the tip** (art review 9: the posts were 1.30 / 1.40 / 1.50 m *plus* a 16 cm tip =
# 1.47 / 1.57 / 1.67 m against R1's 1.4 m walls): 1.32 / 1.40 / 1.48 m, mean 1.40 m like the wall outcrops.
STAKE_H = {"A": 1.32, "B": 1.40, "C": 1.48}
STAKE_TIP = 0.6           # tip height / post width (spec: point 1.1 × half-width above the top)


def stake(b: M.Builder, at: Vector, h: float, rnd: random.Random, region: str = "wood", ivy: bool = False,
          yaw: float = 0.0, w: float = 0.14):
    """One sharpened stake (web stake()): square-ish post, pyramid tip, a grain line, two rope lashing bands at
    30 % and 68 % of the height, optional ivy leaves. ``h`` = overall height, tip included.

    Lashings (art review 9): fitted to the post's own (jittered) faces at their height, 6 mm thick — they stood
    ~2 cm proud and the padded proxy bulged the ink at every lashing (a scalloped silhouette at 1080p); now the
    padding grows the 14-row proxy by ≤ 7 mm in those rows only, so the ink keeps one width along the post."""
    tip = w * STAKE_TIP
    post_h = h - tip
    sp = E.spike(w, post_h, tip, seed=(at.x, at.y, h, rnd.random()), square=True, rows=14)
    faces = [(v.co.z, max(abs(v.co.x), abs(v.co.y))) for v in sp.bm.verts]
    sp.rotate("z", yaw).translate(at)
    pid = b.add(sp, region, name="stake", hull_pad=True)
    for f in (0.30, 0.68):
        zb = post_h * f
        near = [r for z, r in faces if abs(z - zb) < post_h / 10] or [w / 2]
        hw = max(near) + 0.001             # face distance (the spike's corners sit on the diagonals at ±hw, ±hw)
        band = E.rope_band((hw, hw), zb, height=0.05, thick=0.006, sides=16, exp=12.0)
        band.rotate("z", yaw).translate(at)
        b.add(band, "band", name="band", outline=False, covered_by=pid)
    hw = w / 2 * 1.02
    a = math.radians(yaw + 90 * rnd.randint(0, 3))       # a face normal (faces are axis-aligned in the spike frame)
    n = Vector((math.cos(a), math.sin(a), 0))
    face = at + n * hw * 1.005
    grain(b, "wood_line", face + Vector((0, 0, post_h * 0.42)), face + Vector((0, 0, post_h * 0.92)), n, 0.014, pid)
    if ivy:
        for q in range(4):
            aa = a + rnd.uniform(-0.6, 0.6)
            nn = Vector((math.cos(aa), math.sin(aa), 0))
            c = at + nn * (hw * 1.25) + Vector((0, 0, 0.18 + q * 0.16))
            leaf = M.ellipsoid((0.055, 0.03, 0.012), (0, 0, 0), sides=8, rings=3)
            leaf.transform(Matrix.Rotation(math.atan2(nn.y, nn.x) + math.pi / 2, 4, "Z") @ _rx(70 + rnd.uniform(-15, 15))
                           @ _ry(rnd.uniform(-35, 35)))
            leaf.translate(c)
            b.add(leaf, "ivy", name="ivy", outline=False, covered_by=pid)
    return pid


def build_stake(pal: Palette, variant: str):
    name = f"SM_Palisade_Plains_Stake_{variant}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="palisade")
    b.regions(**PAL_REGIONS)
    stake(b, Vector((0, 0, 0)), STAKE_H[variant], rnd, "wood" if variant != "B" else "wood_dk", ivy=variant == "C")
    return E.finish(b, pal, "decor", ground_h=1.2, name=name)


def build_run(pal: Palette, kind: str):
    """``Straight``: the stakes the runtime places on a 1-tile straight wall (−0.4 … +0.4 along X);
    ``Corner``: the half-runs toward +X and −Y meeting at the tile centre."""
    name = f"SM_Palisade_Plains_{kind}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="palisade")
    b.regions(**PAL_REGIONS)
    if kind == "Straight":
        spots = [Vector((x, 0, 0)) for x in (-0.4, -0.2, 0.0, 0.2, 0.4)]
    else:
        spots = [Vector((0, 0, 0))] + [Vector((x, 0, 0)) for x in (0.2, 0.4)] + [Vector((0, -y, 0)) for y in (0.2, 0.4)]
    for i, p in enumerate(spots):
        h = rnd.choice(list(STAKE_H.values())) + rnd.uniform(-0.04, 0.04)
        stake(b, p + Vector((rnd.uniform(-0.015, 0.015), rnd.uniform(-0.015, 0.015), 0)), h, rnd,
              "wood" if i % 3 else "wood_dk", ivy=(i == 2), yaw=rnd.uniform(-12, 12))
    return E.finish(b, pal, "decor", ground_h=1.2, name=name)


def build_gatepost(pal: Palette):
    name = "SM_Palisade_Plains_GatePost"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="palisade")
    b.regions(**PAL_REGIONS)
    h, w = 1.78, 0.22
    sp = E.spike(w, h, 0.0001, seed=name, square=True, taper=0.97, rows=18)
    pid = b.add(sp, "wood", name="post", hull_pad=True)
    # carved cap: a lighter pyramid block
    cap = E.spike(w * 1.18, 0.07, 0.2, seed=(name, "cap"), square=True, taper=1.0).translate((0, 0, h - 0.02))
    b.add(cap, "cap", name="cap", hull_pad=True)
    for f in (0.22, 0.5, 0.78):
        band = E.rope_band((w / 2 * 1.05, w / 2 * 1.05), h * f, height=0.06, thick=0.016, sides=16, exp=8.0)
        b.add(band, "band", name="band", outline=False, covered_by=pid)
    # arm + green camp pennant hanging toward the camp side (−Y)
    arm = M.box((0.08, 0.36, 0.07), (0, -0.22, h - 0.22), bevel=0.012)
    b.add(arm, "wood", name="arm")
    pts = [(-0.11, 0.0), (0.11, 0.0), (0.11, -0.5), (0.0, -0.4), (-0.11, -0.5)]
    pen = M.plate([(x, y) for x, y in reversed(pts)], 0.012, bevel=0.003)
    pen.transform(_rx(90)).transform(_rz(90)).translate((0, -0.34, h - 0.26))
    b.add(pen, "pennant", name="pennant")
    for q in range(3):
        grain(b, "wood_line", (w / 2 * 1.01, rnd.uniform(-0.05, 0.05), 0.2 + q * 0.45),
              (w / 2 * 1.01, rnd.uniform(-0.05, 0.05), 0.42 + q * 0.45), (1, 0, 0), 0.016, pid)
    return E.finish(b, pal, "decor", ground_h=1.2, name=name)


# ── camp kit ────────────────────────────────────────────────────────────────────────────────────────────
CAMP_REGIONS = dict(
    stone=Region("#7A7E88", .42, .35),
    stone_top=Region("#9A9EA8", .42, .40),
    well_stone=Region("#8A8E98", .42, .35),
    stone_line=Region("#8A8E98").line_region(),
    log=Region("#7A4A2A", .42, .30),
    log_end=Region("#E8B070", .35, .30),
    log_line=Region("#7A4A2A").line_region(),
    ash=Region("#4A4048", .40, .20),
    ember=Region("#FF7A2A", .0, .0, e=1.0),
    ember_hot=Region("#FFD080", .0, .0, e=1.0),
    ember_dim=Region("#B8401C", .0, .0, e=0.7),
    post=Region("#7A5238", .42, .30, o=.6),
    post_line=Region("#7A5238").line_region(),
    roof=Region("#9A4A32", .42, .35),
    roof_line=Region("#9A4A32").shade_region(.3, .12),
    bucket=Region("#8A6A44", .42, .32),
    rope=Region("#C8B088", .36, .30, o=.6),
    water=Region("#1A1A28", .30, .10),
    water_hi=Region("#5096C8", .30, .25),
    iron=Region("#5A5E6A", .42, .45),
    canvas=Region("#5A4028", .40, .35),          # spec §6.4 plains tentColor: light .35, shade .4
    canvas_in=Region("#2A1A22", .30, .10),
    canvas_line=Region("#5A4028").shade_region(.30, .12),
    canvas_trim=Region("#8C7050", .36, .30),
    gold=Region("#D8B04A", .42, .45, o=.6),
    peg=Region("#5A3A22", .42, .30, o=.6),
    pole=Region("#6E4A30", .42, .30, o=.55),
    bar=Region("#6E4A30", .42, .22, o=.3),       # banner crossbar: lit top edge seen from above keeps a dark line
    cloth=Region("#2A6A1A", .42, .35, o=.6),
    cloth_trim=Region("#1A4A10", .40, .30),
    stave=Region("#9A6A3E", .42, .35),
    stave_line=Region("#9A6A3E").shade_region(.3, .12),
    lid=Region("#B88A5A", .42, .35),
    plank=Region("#B08450", .42, .35),
    plank_line=Region("#B08450").shade_region(.3, .12),
    frame=Region("#7A5230", .42, .30),
    cup=Region("#5A5A68", .42, .40, o=.6),
    rim=Region("#3A2A24", .40, .25),
    rune_stone=Region("#8A8EA0", .42, .35),
    rune=Region("#FFA040", .0, .0, e=1.0),
    rune_dim=Region("#C86A2A", .10, .10, e=0.6),
    gem=Region("#FF9A3A", .10, .30, e=0.85),
    gem_glint=Region("#FFF0B0", .0, .0, e=0.9),
    gem_line=Region("#6A2A10", .40, .20),
    dark=Region("#4F8A3A", .40, .14),
    light=Region("#8CC24F", .38, .12),
)


def new(pal, name):
    b = M.Builder(name, pal, prefix="camp")
    b.regions(**CAMP_REGIONS)
    return b, scene.rng(name)


def log(b: M.Builder, a: Vector, c: Vector, r: float, name="log"):
    d = (c - a).normalized()
    pts = [a.lerp(c, t) for t in (0, 0.33, 0.66, 1.0)]
    tube = M.tube(pts, [r, r * 1.03, r * 0.98, r], sides=9, dome0=0.0, dome1=0.0)
    pid = b.add(tube, "log", name=name, hull_pad=True)
    for end, s in ((a, -1), (c, 1)):
        disc = M.cylinder(r * 0.82, r * 0.82, 0.0, 0.006, sides=9)
        disc.transform(Vector((0, 0, 1)).rotation_difference(d * s).to_matrix().to_4x4())
        disc.translate(end + d * s * 0.001)
        b.add(disc, "log_end", name="log_end", outline=False, covered_by=pid)
    return pid


def build_campfire(pal: Palette):
    name = "SM_Prop_Camp_Campfire"
    b, rnd = new(pal, name)
    # ash bed + ember coals (flat, emissive core)
    bed = M.cylinder(0.33, 0.30, -0.02, 0.025, sides=16)
    b.add(bed, "ash", name="ash")
    glow = M.cylinder(0.2, 0.17, 0.0, 0.032, sides=14)
    b.add(glow, "ember_dim", name="embers_dim", outline=False)
    for q in range(9):
        a = rnd.uniform(0, math.tau)
        rr = rnd.uniform(0.0, 0.17)
        c = Vector((math.cos(a) * rr, math.sin(a) * rr, 0.035))
        s = rnd.uniform(0.03, 0.05)
        b.add(M.ellipsoid((s, s * 0.8, s * 0.5), c, sides=6, rings=3), "ember_hot" if q % 3 == 0 else "ember",
              name="coal", outline=False)
    # two crossed logs with cut ends #E8B070
    log(b, Vector((-0.34, -0.12, 0.05)), Vector((0.26, 0.1, 0.2)), 0.065)
    log(b, Vector((0.34, -0.14, 0.05)), Vector((-0.24, 0.12, 0.2)), 0.065)
    log(b, Vector((0.02, 0.36, 0.05)), Vector((-0.02, -0.12, 0.2)), 0.055, name="log3")
    # ring of 10 stones
    for i in range(10):
        a = math.tau * i / 10 + rnd.uniform(-0.08, 0.08)
        c = Vector((math.cos(a) * 0.42, math.sin(a) * 0.42, 0))
        r = (rnd.uniform(0.09, 0.12), rnd.uniform(0.075, 0.095), rnd.uniform(0.07, 0.095))
        shape = E.RockShape(r, (name, i), bumps=4, amp=0.08, top_cut=0.3)
        part = E.rock(shape, subdiv=1, facet=26.0, yaw=math.degrees(a))
        E.unflare_foot(part, 0.045)             # bedded: no floor under the belly (art review 10 hull slivers)
        b.add(part.translate(c), "stone", name="stone", hull_pad=True)
    return E.finish(b, pal, "decor", ground_h=0.25, name=name, ground_band=0.02)


def build_well(pal: Palette):
    name = "SM_Prop_Camp_Well"
    b, rnd = new(pal, name)
    R0, R1, H = 0.56, 0.42, 0.58
    # coursed stone blocks (2 courses × 9) + a rim course of lighter cap stones
    for course in range(2):
        n = 9
        for i in range(n):
            a0 = math.tau * (i + 0.5 * course) / n
            a1 = a0 + math.tau / n * 0.94
            z0, z1 = course * H / 2 - 0.02, (course + 1) * H / 2 - 0.01
            blk = _block(R1, R0 + rnd.uniform(-0.01, 0.015), a0, a1, z0, z1)
            b.add(blk, "well_stone", name="block", hull_pad=True)
    for i in range(10):
        a0 = math.tau * i / 10
        a1 = a0 + math.tau / 10 * 0.95
        blk = _block(R1 - 0.03, R0 + 0.04, a0, a1, H - 0.01, H + 0.09)
        b.add(blk, "stone_top", name="cap", hull_pad=True)
    water = M.cylinder(R1 + 0.01, R1 + 0.01, 0.0, H - 0.18, sides=18)
    b.add(water, "water", name="water", outline=False)
    sheen = M.ellipsoid((0.2, 0.08, 0.006), (0.05, -0.04, H - 0.175), sides=10, rings=3)
    b.add(sheen, "water_hi", name="sheen", outline=False)
    # posts, windlass, roof
    for s in (-1, 1):
        post = M.box((0.11, 0.11, 1.45), (s * 0.5, 0.0, 0.62 + 0.72 - 0.08), bevel=0.015)
        pid = b.add(post, "post", name="post", hull_pad=True)
        grain(b, "post_line", (s * 0.5 - 0.056 * s, -0.02, 0.9), (s * 0.5 - 0.056 * s, 0.02, 1.4), (-s, 0, 0), 0.012, pid)
    axle = M.tube([(-0.5, 0, 1.2), (0.5, 0, 1.2)], [0.035, 0.035], sides=8)
    b.add(axle, "post", name="axle")
    drum = M.tube([(-0.16, 0, 1.2), (0.16, 0, 1.2)], [0.07, 0.07], sides=10)
    b.add(drum, "rope", name="drum")
    crank = M.box((0.04, 0.04, 0.2), (0.6, 0, 1.12), bevel=0.008)
    b.add(crank, "post", name="crank")
    rope = M.tube([(0.0, -0.06, 1.15), (0.0, -0.06, 0.82)], [0.012, 0.012], sides=6)
    b.add(rope, "rope", name="rope")
    bucket = M.loft_z([(0.62, 0.085, 0.085), (0.8, 0.11, 0.11)], sides=12, cap0="ngon", cap1="ngon")
    b.add(bucket.translate((0, -0.06, 0)), "bucket", name="bucket")
    hoop = M.belt(0.105, 0.105, 0.74, 0.025, 0.01, sides=12)
    b.add(hoop.translate((0, -0.06, 0)), "iron", name="hoop", outline=False)
    # gable roof of planks
    for s in (-1, 1):
        roof = M.box((1.42, 0.58, 0.05), (0, 0, 0), bevel=0.012)
        roof.transform(_rx(-s * 34)).translate((0, s * 0.24, 1.64))
        rid = b.add(roof, "roof", name="roof", hull_pad=True)
        n = Vector((0, s * math.sin(math.radians(34)), math.cos(math.radians(34))))
        for q in range(1, 6):
            x = -0.71 + q * 1.42 / 6
            p0 = Vector((x, s * 0.03, 1.64 + 0.2)) + n * 0.03
            p1 = Vector((x, s * 0.47, 1.64 - 0.11)) + n * 0.03
            grain(b, "roof_line", p0, p1, n, 0.016, rid)
    ridge = M.box((1.5, 0.09, 0.09), (0, 0, 1.86), bevel=0.012)
    b.add(ridge, "post", name="ridge")
    # a small rock at the foot (web)
    shape = E.RockShape((0.13, 0.11, 0.1), (name, "rock"), bumps=4, amp=0.08, top_cut=0.3)
    b.add(E.rock(shape, subdiv=1, facet=26.0).translate((0.62, -0.45, 0)), "well_stone", name="rock", hull_pad=True)
    return E.finish(b, pal, "decor", ground_h=0.9, name=name)


def _block(r_in, r_out, a0, a1, z0, z1, segs: int = 3):
    """A curved stone block of a round wall (closed)."""
    rings = []
    for i in range(segs + 1):
        a = a0 + (a1 - a0) * i / segs
        c, s = math.cos(a), math.sin(a)
        rings.append([Vector((c * r_in, s * r_in, z0)), Vector((c * r_out, s * r_out, z0)),
                      Vector((c * r_out, s * r_out, z1)), Vector((c * r_in, s * r_in, z1))])
    return E.ring_loft(rings, cap0="ngon", cap1="ngon", smooth=False).auto_sharp(40)


TENT_PROFILE = ((0.02, 1.70), (0.38, 1.08), (0.78, 0.55), (1.25, 0.0))   # ridge → eave (x, z): 60° / 53° / 49.5°
TENT_OVERHANG = 0.04                                                     # roof past each gable (closes the eaves)


def tent_roof_z(x: float) -> float:
    """Roof height at |x| on the creased profile (piecewise linear)."""
    x = abs(x)
    pts = TENT_PROFILE
    if x <= pts[0][0]:
        return pts[0][1]
    for (x0, z0), (x1, z1) in zip(pts, pts[1:]):
        if x <= x1:
            return z0 + (z1 - z0) * (x - x0) / (x1 - x0)
    return 0.0


def build_tent(pal: Palette):
    """Plains tent (web ``CampProps`` tent, spec §6.4): A-frame canvas ``#5A4028`` 2.5 × 1.96 m, apex 1.7 m. Art
    review 10: each roof side is **three flat creased panels** (60° / 53° / 49.5°, flat-shaded) so the toon bands
    snap per panel — the sagging sheet's smooth curvature spread the light / rim smoothsteps into airbrushed
    gradients and a whitish smear on the shaded side; the roof overhangs both gables by 4 cm so no interior shows
    at the eaves."""
    name = "SM_Prop_Camp_Tent"
    b, rnd = new(pal, name)
    W, D, Hr = TENT_PROFILE[-1][0], 0.98, TENT_PROFILE[0][1]
    th = 0.03
    Do = D + TENT_OVERHANG
    ys = [-Do, -D * 0.36, D * 0.36, Do]
    for s in (-1, 1):
        bm = bmesh.new()
        vv = [[bm.verts.new((s * x, y, z)) for y in ys] for x, z in TENT_PROFILE]
        for j in range(len(TENT_PROFILE) - 1):
            for i in range(len(ys) - 1):
                q = (vv[j][i], vv[j][i + 1], vv[j + 1][i + 1], vv[j + 1][i])
                bm.faces.new(q if s > 0 else tuple(reversed(q)))
        bm.normal_update()
        bmesh.ops.solidify(bm, geom=list(bm.faces), thickness=th)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        part = M.Part(bm).flat()
        rid = b.add(part, "canvas", name=f"roof{s}", hull_pad=True)
        # trim stripe near the hem + seams down the slope (lying on the panels)

        def on_roof(x, y):
            x = abs(x)
            e = 0.01
            z = tent_roof_z(x)
            dz = (tent_roof_z(min(x + e, W)) - tent_roof_z(max(x - e, 0.0))) / (min(x + e, W) - max(x - e, 0.0))
            n = Vector((s * -dz, 0.0, 1.0)).normalized()
            return Vector((s * x, y, z)) + n * th, n
        xt = 1.06
        trim = [on_roof(xt, y) for y in (-Do + 0.01, 0.0, Do - 0.01)]
        b.add(E.ribbon([q for q, _ in trim], [n for _, n in trim], 0.06, lift=0.002, thick=0.003), "canvas_trim",
              name="trim", outline=False, covered_by=rid)
        for y in (-D * 0.36, D * 0.36):
            sm = [on_roof(x, y) for x in (0.06, 0.2, 0.38, 0.58, 0.78, 1.0, 1.2)]
            b.add(E.ribbon([q for q, _ in sm], [n for _, n in sm], 0.014, lift=0.004, thick=0.002), "canvas_line",
                  name="seam", outline=False, covered_by=rid)
    # gables: back closed, front with a dark door and the flaps (one tied back)
    for y, front in ((D, False), (-D, True)):
        tri = ([(-W, 0.0), (W, 0.0)] + [(x, z) for x, z in reversed(TENT_PROFILE[1:-1])] + [(0.0, Hr)] +
               [(-x, z) for x, z in TENT_PROFILE[1:-1]])
        g = M.plate(tri if front else list(reversed(tri)), 0.025, bevel=0.004)
        g.transform(_rx(90)).translate((0, y + (0.012 if not front else -0.012) * 0, 0))
        gid = b.add(g, "canvas", name="gable", hull_pad=True)
        # web: light trim stripe 8 px (≈ 0.2 m) above the ground across the whole tent, seams from the apex
        sgn = -1 if front else 1
        face_y = y + sgn * 0.0127
        nrm = Vector((0, sgn, 0))
        zt = 0.2
        half = W - (W - TENT_PROFILE[-2][0]) * zt / TENT_PROFILE[-2][1] - 0.04
        spans = [(-half, -0.47), (0.47, half)] if front else [(-half, half)]
        for x0, x1 in spans:
            b.add(E.ribbon([Vector((x0, face_y, zt)), Vector(((x0 + x1) / 2, face_y, zt)), Vector((x1, face_y, zt))],
                           [nrm] * 3, 0.07, lift=0.0005, thick=0.002), "canvas_trim", name="trim", outline=False,
                  covered_by=gid)
        for xs in (-1, 1):
            xa = xs * (0.62 if front else 0.45)
            top_p = Vector((0, face_y, Hr * 0.94))
            bot_p = Vector((xa, face_y, 0.04))
            b.add(E.ribbon([top_p, top_p.lerp(bot_p, 0.5), bot_p], [nrm] * 3, 0.016, lift=0.0005, thick=0.0015),
                  "canvas_line", name="seam", outline=False, covered_by=gid)
        if front:
            door = M.plate([(-0.42, 0.0), (0.42, 0.0), (0.0, 1.22)], 0.01)
            door.transform(_rx(90)).translate((0, y - 0.016, 0.0))
            b.add(door, "canvas_in", name="door", outline=False, covered_by=gid)
            flap = M.plate([(-0.42, 0.0), (-0.12, 0.0), (0.0, 1.22), (-0.06, 1.0)], 0.014, bevel=0.003)
            flap.transform(_rx(90)).translate((0, y - 0.03, 0.0))
            b.add(flap, "canvas", name="flap", hull_pad=True)
            tied = M.plate([(0.0, 1.22), (0.2, 0.72), (0.44, 0.5), (0.5, 0.62), (0.32, 0.86)], 0.04, bevel=0.008)
            tied.transform(_rx(90)).translate((0, y - 0.035, 0.0))
            b.add(tied, "canvas", name="flap_tied", hull_pad=True)
            tie = M.box((0.05, 0.05, 0.05), (0.42, y - 0.06, 0.6), bevel=0.01)
            b.add(tie, "rope", name="tie")
            for xs in (-1, 1):
                pts = [Vector((0, y - 0.013, Hr * 0.98))] + [Vector((xs * x * 0.97, y - 0.013, z * 0.98 + 0.01))
                                                             for x, z in TENT_PROFILE[1:]]
                b.add(E.ribbon(pts, [Vector((0, -1, 0))] * len(pts), 0.03, lift=0.003, thick=0.003), "canvas_trim",
                      name="edge", outline=False, covered_by=gid)
    # ridge pole + gold pennant at the front, guy ropes and pegs
    pole = M.tube([(0, -Do - 0.08, Hr + 0.02), (0, Do + 0.08, Hr + 0.02)], [0.035, 0.035], sides=8)
    b.add(pole, "post", name="ridge")
    up = M.tube([(0, -Do - 0.06, Hr - 0.05), (0, -Do - 0.06, Hr + 0.4)], [0.025, 0.02], sides=6)
    b.add(up, "post", name="mast")
    pen = M.plate([(0.0, 0.0), (0.26, -0.07), (0.0, -0.15)], 0.012, bevel=0.003)
    pen.transform(_rx(90)).translate((0, -Do - 0.06, Hr + 0.4))
    b.add(pen, "gold", name="pennant")
    for xs, ys in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
        a = Vector((xs * 0.62, ys * (Do - 0.02), tent_roof_z(0.62) + th + 0.005))
        e = Vector((xs * (W + 0.42), ys * (D + 0.38), 0.03))
        b.add(M.tube([a, a.lerp(e, 0.5) + Vector((0, 0, -0.03)), e], [0.011] * 3, sides=5), "rope", name="guy")
        b.add(M.box((0.05, 0.05, 0.14), e + Vector((0, 0, 0.02)), bevel=0.01), "peg", name="peg")
    return E.finish(b, pal, "decor", ground_h=1.3, name=name)


def build_banner(pal: Palette):
    name = "SM_Prop_Camp_Banner"
    b, rnd = new(pal, name)
    H = 2.35
    pole = M.tube([(0, 0, -0.05), (0, 0, H * 0.5), (0, 0, H)], [0.05, 0.045, 0.04], sides=8)
    pid = b.add(pole, "pole", name="pole", hull_pad=True)
    fin = M.gem(0.07, 0.2, 0.08, sides=4).translate((0, 0, H + 0.04))
    b.add(fin, "gold", name="finial")
    bar = M.tube([(-0.07, 0, H - 0.17), (0.6, 0, H - 0.17)], [0.036, 0.036], sides=8)
    b.add(bar, "bar", name="bar")
    # swallow-tail cloth hanging from the crossbar (+X side), gentle waves
    w0, L = 0.5, 1.15
    pts = [(0.0, 0.0), (w0, 0.0), (w0 + 0.03, -L), (w0 / 2, -L + 0.22), (0.0, -L)]
    cloth = M.plate(list(reversed(pts)), 0.02, bevel=0.004)
    bm = cloth.bm

    def bulge(x, y):            # cloth bellied toward −Y, the tails flutter
        return 0.04 * math.sin(math.pi * min(max(x / w0, 0.0), 1.0)) + 0.03 * math.sin(x * 11 + 1.0) * max(0.0, (-y - 0.8) / 0.35)
    for v in bm.verts:
        v.co.z += bulge(v.co.x, v.co.y)
    cloth.transform(_rx(90)).translate((0.03, 0.0, H - 0.2))
    cid = b.add(cloth, "cloth", name="cloth", hull_pad=True)
    sway = {cid}
    trim = M.box((w0 - 0.02, 0.03, 0.06), (0.03 + w0 / 2, 0.0, H - 0.24), bevel=0.008)
    sway.add(b.add(trim, "cloth_trim", name="trim", outline=False, covered_by=cid))
    # gold sun disc with 8 rays, both faces
    cx, cz = 0.03 + w0 / 2, H - 0.2 - 0.5
    off = -bulge(w0 / 2, -0.5)          # the cloth surface at the emblem (rx(90): plate z → world −y)
    for side in (-1, 1):
        disc = M.cylinder(0.105, 0.105, -0.004, 0.004, sides=14)
        disc.transform(_rx(90)).translate((cx, off + side * 0.011, cz))
        sway.add(b.add(disc, "gold", name="sun", outline=False, covered_by=cid))
        for i in range(8):
            a = math.tau * i / 8
            p0 = Vector((cx + math.cos(a) * 0.14, off + side * 0.01, cz + math.sin(a) * 0.14))
            p1 = Vector((cx + math.cos(a) * 0.19, off + side * 0.01, cz + math.sin(a) * 0.19))
            p0.y += -bulge(p0.x - 0.03, -0.5) - off
            p1.y += -bulge(p1.x - 0.03, -0.5) - off
            sway.add(b.add(E.ribbon([p0, (p0 + p1) / 2, p1], [Vector((0, side, 0))] * 3, [0.026, 0.022, 0.014],
                                    lift=0.001, thick=0.003), "gold", name="ray", outline=False, covered_by=cid))
    # foot wedges
    for i in range(3):
        a = math.tau * i / 3 + 0.4
        shape = E.RockShape((0.09, 0.08, 0.07), (name, i), bumps=4, amp=0.08, top_cut=0.3)
        b.add(E.rock(shape, subdiv=1, facet=26.0).translate((math.cos(a) * 0.1, math.sin(a) * 0.1, 0)), "stone",
              name="wedge", hull_pad=True)
    # spec §6.4 WPO cloth sway: the swallow-tail sways from the crossbar (0) to the tails (1); pole rigid
    hull_ids = {k for k, p in enumerate(b.parts) if p.get("hullOnly") and p["name"].startswith("cloth")}
    wind = E.cloth_wind({pid: (H - 0.2, L, 0.15) for pid in sway | hull_ids})
    return E.finish(b, pal, "decor", ground_h=1.0, wind=wind, name=name)


def build_barrel(pal: Palette):
    name = "SM_Prop_Camp_Barrel"
    b, rnd = new(pal, name)
    H, r0, rm = 0.86, 0.27, 0.33
    prof = [(r0, 0.0), (rm * 0.95, H * 0.25), (rm, H * 0.5), (rm * 0.95, H * 0.75), (r0, H)]
    body = M.lathe(prof, sides=16, cap0="ngon", cap1=None)
    pid = b.add(body, "stave", name="body", hull_pad=True)
    lid = M.cylinder(r0 * 0.92, r0 * 0.92, H - 0.05, H - 0.012, sides=16)
    b.add(lid, "lid", name="lid")
    rim = M.belt(r0 - 0.012, r0 - 0.012, H - 0.04, 0.05, 0.02, sides=16)
    b.add(rim, "stave", name="rim")
    # iron hoops carry their own (ring) hulls: padding the 5-ring body proxy round 1.6 cm hoops inflated the whole
    # barrel's hull (art review 10: 2.75 px baked rim, over the 2.45 px bound) — the hoops now stand 1 cm proud and
    # bump the silhouette ink locally, like the web's banded barrel
    for z in (H * 0.2, H * 0.8):
        rr = r0 + (rm - r0) * math.sin(math.pi * z / H) * 0.98
        hoop = M.belt(rr, rr, z - 0.03, 0.06, 0.010, sides=16, exp=2.0)          # round (2.4 bulged at 45°)
        b.add(hoop, "iron", name="hoop")
    for i in range(8):
        a = math.tau * (i + 0.5) / 8
        pts, nrm = [], []
        for z in (0.04, H * 0.3, H * 0.5, H * 0.7, H - 0.04):
            rr = r0 + (rm - r0) * math.sin(math.pi * z / H)
            pts.append(Vector((math.cos(a) * rr, math.sin(a) * rr, z)))
            nrm.append(Vector((math.cos(a), math.sin(a), 0)))
        b.add(E.ribbon(pts, nrm, 0.012, lift=0.002, thick=0.002), "stave_line", name="stave", outline=False,
              covered_by=pid)
    for x in (-0.07, 0.07):
        grain(b, "stave_line", (x, -0.2, H - 0.011), (x, 0.2, H - 0.011), (0, 0, 1), 0.012, pid)
    return E.finish(b, pal, "decor", ground_h=0.7, name=name)


def build_crate(pal: Palette):
    name = "SM_Prop_Camp_Crate"
    b, rnd = new(pal, name)
    S = 0.74
    box = M.box((S - 0.04, S - 0.04, S - 0.04), (0, 0, S / 2), bevel=0.012)
    pid = b.add(box, "plank", name="box", hull_pad=True)
    h = S / 2
    fw = 0.075
    # frame battens on every edge + a diagonal brace per side face
    for ax in range(3):
        for s1 in (-1, 1):
            for s2 in (-1, 1):
                c = [0.0, 0.0, 0.0]
                size = [fw, fw, fw]
                size[ax] = S
                o = [i for i in range(3) if i != ax]
                c[o[0]] = s1 * (h - fw / 2 + 0.004)
                c[o[1]] = s2 * (h - fw / 2 + 0.004)
                c[2] += h
                b.add(M.box(size, c, bevel=0.01), "frame", name="batten", hull_pad=False)
    for nx, ny in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        n = Vector((nx, ny, 0))
        t = Vector((-ny, nx, 0))
        L = (S - 2 * fw) * math.sqrt(2) * 0.98
        dg = (t + Vector((0, 0, 1))).normalized()
        zz = dg.cross(n)
        brace = M.box((L, 0.025, 0.06), (0, 0, 0), bevel=0.008)
        brace.transform(Matrix((dg, n, zz)).transposed().to_4x4())
        brace.translate(n * (h + 0.006) + Vector((0, 0, h)))
        b.add(brace, "frame", name="brace")
        for q in (-1, 1):
            grain(b, "plank_line", n * (h - 0.018) + t * (-h + fw) + Vector((0, 0, h + q * 0.12)),
                  n * (h - 0.018) + t * (h - fw) + Vector((0, 0, h + q * 0.12)), n, 0.01, pid)
    for q in (-0.12, 0.12):
        grain(b, "plank_line", (-h + fw, q, S - 0.018), (h - fw, q, S - 0.018), (0, 0, 1), 0.01, pid)
    return E.finish(b, pal, "decor", ground_h=0.7, name=name)


def build_torch(pal: Palette):
    name = "SM_Prop_Camp_Torch"
    b, rnd = new(pal, name)
    top = 1.56
    pole = M.tube([(0, 0, -0.05), (0, 0, 0.8), (0, 0, top)], [0.05, 0.045, 0.04], sides=8)
    pid = b.add(pole, "pole", name="pole", hull_pad=True)
    # rope wraps and the bowl rim carry their own (ring) hulls: padding the 3-ring pole and cup proxies round them
    # inflated the whole pole's hull (art review 10: 2.98 px baked rim, over the 2.45 px bound)
    for z in (0.62, 1.12):
        band = M.belt(0.047, 0.047, z, 0.05, 0.009, sides=10, exp=2.0)
        b.add(band, "rope", name="wrap")
    cup = M.loft_z([(top - 0.02, 0.05, 0.05), (top + 0.1, 0.13, 0.13), (top + 0.16, 0.15, 0.15)], sides=10,
                   cap0="ngon", cap1=None)
    cid = b.add(cup, "cup", name="cup", hull_pad=True)
    rim = M.belt(0.142, 0.142, top + 0.14, 0.035, 0.014, sides=12, exp=2.0)
    b.add(rim, "rim", name="rim")
    # coals sit low in the cup (the flame VFX rises from socket flame): a radial ember bed like the campfire and
    # the web torch cup — dim rim #B8401C, #FF7A2A ring, hot #FFD080 centre — with six small scattered embers
    # (art review 9: two hot lumps on the dim bed read as a pair of eyes on every torch)
    for rr, z1, reg in ((0.125, top + 0.1, "ember_dim"), (0.085, top + 0.104, "ember"), (0.04, top + 0.108,
                                                                                         "ember_hot")):
        b.add(M.cylinder(rr, rr * 0.96, top + 0.07, z1, sides=12), reg, name="coals", outline=False)
    for q in range(6):
        a = q * 2.4 + rnd.uniform(-0.3, 0.3)
        rr = rnd.uniform(0.03, 0.095)
        sz = rnd.uniform(0.01, 0.016)
        b.add(M.ellipsoid((sz, sz * 0.85, sz * 0.6), (math.cos(a) * rr, math.sin(a) * rr, top + 0.108), sides=5,
                          rings=3), "ember_hot" if rr > 0.06 else "ember", name="coal", outline=False)
    for i in range(3):
        a = math.tau * i / 3
        shape = E.RockShape((0.08, 0.07, 0.06), (name, i), bumps=4, amp=0.08, top_cut=0.3)
        b.add(E.rock(shape, subdiv=1, facet=26.0).translate((math.cos(a) * 0.09, math.sin(a) * 0.09, 0)), "stone",
              name="wedge", hull_pad=True)
    return E.finish(b, pal, "decor", ground_h=1.0, name=name)


HEARTH_RUNE_Z = 0.43          # carved rune centre (socket ``glow``)
HEARTH_GEM_Z = 0.765          # crown gem centre, set in the upper front face (socket ``gem``)


def build_hearthstone(pal: Palette):
    """归炉 hearthstone (web ``TowerHearthstoneDrawer``: a slim pill menhir 16 × 34 px on two base stones, a carved
    flame-in-a-circle rune ``#FFA040`` and a faceted ember diamond ``#FF9A3A`` with a ``#FFF0B0`` glint in the
    crown). Art review 10: at the W1 pitch the old stone read as a squat bell and its focal accents as an orange 'O'
    and a dot at 1080p — the body is now ≈ 10 % slimmer and longer, the rune ring's stroke 3.2 cm round a **solid**
    12 cm flame teardrop, the crown gem a 9 cm diamond with a glint facet; the base stones' hull hugs the floor."""
    name = "SM_Prop_Camp_Hearthstone"
    b, rnd = new(pal, name)
    # two low base stones at the foot (the web's dark feet, 1.25 × the stone's width overall)
    for i, (x, r) in enumerate(((-0.13, (0.15, 0.13, 0.095)), (0.14, (0.13, 0.12, 0.085)))):
        shape = E.RockShape(r, (name, "base", i), bumps=4, amp=0.07, top_cut=0.35)
        part = E.rock(shape, subdiv=2, facet=30.0)
        E.unflare_foot(part, 0.05)              # bedded: no floor under the belly (art review 10 hull slivers)
        b.add(part.translate((x, 0.02, 0)), "stone", name="base", hull_pad=True)
    # standing rune stone: a slim pill — nearly parallel sides, a rounded crown (front −Y)
    rings = []
    for z, w, d in ((0.10, 0.205, 0.092), (0.46, 0.198, 0.088), (0.70, 0.185, 0.083), (0.80, 0.160, 0.076),
                    (0.865, 0.118, 0.062), (0.90, 0.055, 0.035)):
        rings.append([Vector((math.cos(a) * w, math.sin(a) * d, z)) for a in
                      (math.tau * i / 12 for i in range(12))])
    slab = E.ring_loft(rings, cap0="ngon", cap1="fan", smooth=False).auto_sharp(30)
    bvh = BVHTree.FromBMesh(slab.bm)
    sid = b.add(slab, "rune_stone", name="stone", hull_pad=True)
    # carved hearth rune **inset in** the front face (art review 9: a flat ring stood proud where the faceted face
    # curves back): a 3.2 cm ring round a solid flame teardrop (art review 10: 2.2 cm strokes and an outlined flame
    # thinned to under 2 px at 1080p)

    def on_face(x: float, z: float, inset: float = 0.0015):
        loc, nrm, _, _ = bvh.ray_cast(Vector((x, -1.0, z)), Vector((0, 1, 0)))
        if loc is None:
            return Vector((x, -0.09, z)), Vector((0, -1, 0))
        return loc - nrm * inset, nrm
    fz = HEARTH_RUNE_Z
    # rx 0.084: the ring stays on the two front facets (on the side facets it read as a sliver at the profile)
    ring = [on_face(math.cos(a) * 0.084, fz + math.sin(a) * 0.104) for a in (math.tau * i / 24 for i in range(25))]
    b.add(E.ribbon([p for p, _ in ring], [n for _, n in ring], 0.032, lift=0.0, thick=0.002), "rune",
          name="rune_ring", outline=False, covered_by=sid)            # face + 0.5 mm: flush, no sliver edge-on
    # the flame: a filled teardrop, tip up, 12 cm tall × 6.4 cm wide, built as two halves stitched on the x = 0
    # ridge of the two front facets (a flat plate's long triangles spanned the ridge and sank behind it: only the
    # rim showed — the "outlined flame"), every vertex 0.8 mm proud of its facet
    for sgn in (1.0, -1.0):
        bm = bmesh.new()
        outer, axis = [], []
        for k in range(11):
            t = math.pi * k / 10
            x = sgn * 0.043 * math.sin(t) * abs(math.sin(t / 2))
            z = 0.060 * math.cos(t) + 0.004
            po, no = on_face(x, fz + z, inset=0.0)
            pa, na = on_face(0.0, fz + z, inset=0.0)
            outer.append(bm.verts.new(po + no * 0.0008))
            axis.append(outer[-1] if k in (0, 10) else bm.verts.new(pa + na * 0.0008))
        for k in range(10):
            quad = [axis[k], outer[k], outer[k + 1], axis[k + 1]]
            uniq = []
            for v in quad:
                if v not in uniq:
                    uniq.append(v)
            f = bm.faces.new(uniq if sgn > 0 else list(reversed(uniq)))
        bm.normal_update()
        for f in bm.faces:
            if f.normal.y > 0:
                f.normal_flip()
        b.add(M.Part(bm).flat(), "rune", name="rune_flame", outline=False, covered_by=sid)
    # ember diamond set in the upper front face (web: just under the crown): a 4-sided bipyramid 12 cm tall with a
    # girdle vertex toward the camera (a rhombus; the lit upper-left and the shaded right facets show), its back
    # half sunk in the stone, its own hull (the web's dark line #6A2A10), a pale glint sliver on the lit facet
    gz = HEARTH_GEM_Z
    gr, gt = 0.042, 0.064
    gem = M.gem(gr, gt, 0.054, sides=4)
    front, gn = on_face(0.0, gz, inset=0.0)
    off = Vector((0.0, front.y - 0.008, gz))          # axis 8 mm before the face: both apices show
    gem.translate(off)
    gid = b.add(gem, "gem", name="gem", hull_pad=True)
    L = Vector((-gr * 0.92, 0.0, gt * 0.25)) + off            # upper ring vertex (left), (front), apex
    F = Vector((0.0, -gr * 0.92, gt * 0.25)) + off
    A = Vector((0.0, 0.0, gt)) + off
    nrm = (F - L).cross(A - L).normalized()
    if nrm.y > 0:
        nrm = -nrm
    tri = [L * 0.62 + F * 0.30 + A * 0.08, L * 0.28 + F * 0.60 + A * 0.12, L * 0.16 + F * 0.14 + A * 0.70]
    bm = bmesh.new()
    vs = [bm.verts.new(p + nrm * 0.0006) for p in tri] + [bm.verts.new(p + nrm * 0.0016) for p in tri]
    bm.faces.new(vs[:3][::-1])
    bm.faces.new(vs[3:])
    for i in range(3):
        j = (i + 1) % 3
        bm.faces.new((vs[i], vs[j], vs[3 + j], vs[3 + i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    b.add(M.Part(bm).flat(), "gem_glint", name="gem_glint", outline=False, covered_by=gid)
    return E.finish(b, pal, "interactive", ground_h=0.6, name=name, ground_band=0.02)


# ── registry ────────────────────────────────────────────────────────────────────────────────────────────
ASSETS: dict = {}
for _v in "ABC":
    ASSETS[f"SM_Palisade_Plains_Stake_{_v}"] = (lambda pal, v=_v: build_stake(pal, v),
                                               dict(game_ids=["palisade_plains"], footprint=[1, 1], blocking=True,
                                                    blob=0.12))
ASSETS["SM_Palisade_Plains_Straight"] = (lambda pal: build_run(pal, "Straight"), dict(blob=0.3, footprint=[1, 1],
                                                                                      blocking=True))
ASSETS["SM_Palisade_Plains_Corner"] = (lambda pal: build_run(pal, "Corner"), dict(blob=0.3, footprint=[1, 1],
                                                                                  blocking=True))
ASSETS["SM_Palisade_Plains_GatePost"] = (build_gatepost, dict(game_ids=["palisade_gate_plains"], footprint=[1, 1],
                                                              blocking=False, blob=0.16))
ASSETS["SM_Prop_Camp_Campfire"] = (build_campfire, dict(game_ids=["camp_campfire"], footprint=[1, 1], blocking=False,
                                                        blob=0.5, sockets=[{"name": "flame", "pos": (0, 0, 0.12)}]))
ASSETS["SM_Prop_Camp_Well"] = (build_well, dict(game_ids=["camp_well"], footprint=[1, 1], blocking=True, blob=0.7))
ASSETS["SM_Prop_Camp_Tent"] = (build_tent, dict(game_ids=["camp_tent"], footprint=[2, 2], blocking=True, blob=1.2))
ASSETS["SM_Prop_Camp_Banner"] = (build_banner, dict(game_ids=["camp_banner"], footprint=[1, 1], blocking=False,
                                                    blob=0.2, wind_slot=E.PROP_WIND_SLOT,
                                                    toon_params=E.PROP_WIND_PARAMS))
ASSETS["SM_Prop_Camp_Barrel"] = (build_barrel, dict(game_ids=["camp_barrel"], footprint=[1, 1], blocking=True,
                                                    blob=0.4))
ASSETS["SM_Prop_Camp_Crate"] = (build_crate, dict(game_ids=["camp_crate"], footprint=[1, 1], blocking=True, blob=0.5))
ASSETS["SM_Prop_Camp_Torch"] = (build_torch, dict(game_ids=["camp_torch"], footprint=[1, 1], blocking=False,
                                                  blob=0.15, sockets=[{"name": "flame", "pos": (0, 0, 1.70)}]))
ASSETS["SM_Prop_Camp_Hearthstone"] = (build_hearthstone, dict(game_ids=["camp_hearthstone", "decor_hearthstone"],
                                                              footprint=[1, 1], blocking=False, blob=0.35,
                                                              sockets=[{"name": "glow",
                                                                        "pos": (0, -0.10, HEARTH_RUNE_Z)},
                                                                       {"name": "gem",
                                                                        "pos": (0, -0.095, HEARTH_GEM_Z)}]))
NOTES = {
    "SM_Prop_Camp_Campfire": "flame + sparks + light = VFX recipe camp.campfire at socket flame (spec 8.7); the hero "
                             "respawns on it (not blocking)",
    "SM_Prop_Camp_Torch": "flame = VFX recipe camp.torch at socket flame",
    "SM_Prop_Camp_Hearthstone": "归炉 dormant until the Ember Tower milestone (DECISIONS Q3: not placed in milestone 1); "
                                "pulse 800 ms + ground glow #FF8A3A via NS_World_Hearthstone at socket glow",
    "SM_Prop_Camp_Tent": "faces the camp centre (front -Y = door); camp layout v2 (DECISIONS W11): 3 tents per camp, "
                         "the camp NPC stands 0.73-0.76 m in front of the door. Art review 10: the A-frame and the dark "
                         "door read only while the door faces within ~60 deg of screen-down (W1 camera azimuth 225 "
                         "deg); broadside it shows one roof slope (a flat canvas wall) - CampLayout v2 tents 2 / 3 "
                         "stand at 90 / 83 deg (world owner: clamp their facing or move them)",
    "SM_Prop_Camp_Tent#footprint": "footprintTiles [2,2] = the nominal canvas body (2.5 x 2.0 m; 3.4 x 2.8 m with the "
                                   "guy ropes); the core blocks the real footprint: every tile whose centre lies within "
                                   "the canvas grown by 0.3 tile at the tent's sub-tile position and facing (MapGen "
                                   "CampPropBlockedTiles, DECISIONS W11: 8-10 tiles per tent)",
    "SM_Palisade_Plains_Straight": "level-design run: the 5 stakes of a straight 1-tile wall (runtime places single "
                                   "stakes)",
    "SM_Palisade_Plains_Corner": "level-design corner: centre + half-runs toward +X and -Y",
    "SM_Palisade_Plains_GatePost": "one each side of the 2-tile camp gate (AAbyssZoneActor::BuildWalls: camp.x - 1.5 "
                                   "/ + 0.5 on the gate row, half-tile positions), pennant arm toward the camp (-Y); "
                                   "not blocking: the palisade wall tile beside it already blocks, and rounding a "
                                   "half-tile post onto the gate would close one of the two gate tiles",
}


def main(argv: list[str]) -> int:
    quick = "--quick" in argv
    sheet = "--sheet" in argv
    previews = "--no-previews" not in argv
    names = [a for a in argv if not a.startswith("--")] or list(ASSETS)
    names = [n for n in ASSETS if any(n == a or a in n for a in names)]
    scene.reset()
    pal = Palette(E.PROPS)
    if sheet:
        objs = [ASSETS[n][0](pal) for n in names]
        pal.save()
        E.sheet(objs, "sheet_" + "_".join(n.split("_")[-1][:6] for n in names)[:80], cols=3)
        return 0
    if quick:
        objs = [ASSETS[n][0](pal) for n in names]
        pal.save()
        E.quick(objs, "camp_" + "_".join(n.split("_")[-1][:6] for n in names)[:80], spacing=2.0)
        return 0
    shipped = []
    for n in (names if "--family" not in argv else []):
        scene.reset()
        pal = Palette(E.PROPS)
        fn, kw = ASSETS[n]
        obj = fn(pal)
        pal.save()
        extra = {"displayName": n.split("_", 2)[-1], "family": "environment.camp"}
        if n in NOTES:
            extra["note"] = NOTES[n]
        if f"{n}#footprint" in NOTES:
            extra["footprintNote"] = NOTES[f"{n}#footprint"]
        E.ship(obj, n, CAT, pal, obj["af_outline_class"], extra=extra, previews=previews, **kw)
        shipped.append(n)
    if "--family" in argv:
        shipped = names
    if previews and len(shipped) > 1:
        scene.reset()
        pal = Palette(E.PROPS)
        objs = [ASSETS[n][0](pal) for n in shipped]
        pal_objs = [o for o in objs if "Palisade" in o.name]
        kit = [o for o in objs if o not in pal_objs]
        if pal_objs:
            E.family_sheet(pal_objs, "camp_palisade.png", "PLAINS CAMP PALISADE (R1 1.4 M)", cols=4)
        for o in pal_objs:
            o.hide_render = True
        big = [o for o in kit if any(k in o.name for k in ("Tent", "Well", "Campfire", "Banner"))]
        small = [o for o in kit if o not in big]
        if big:
            E.family_sheet(big, "camp_kit.png", "PLAINS CAMP KIT: TENT WELL CAMPFIRE BANNER", cols=3)
        for o in big:
            o.hide_render = True
        if small:
            E.family_sheet(small, "camp_kit_small.png", "PLAINS CAMP KIT: BARREL CRATE TORCH HEARTHSTONE", cols=5)
    return E.summary()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

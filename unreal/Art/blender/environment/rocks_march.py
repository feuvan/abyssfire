"""March rocks and wall outcrops (dark remake A10; chapter1.md §15 "broken stone with tool marks", "ash-crusted
boulder"; palette ``Props_March``, A1 ``march/march_palettes.py``).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/rocks_march.py [names…]
        [--quick] [--sheet] [--family] [--ridge] [--no-previews]

| Asset | game id (themed) | look |
|---|---|---|
| ``SM_Rock_March_BrokenStone_A/B/C`` | ``rock@march`` | the field's stone is quarried and broken: dressed blocks ``#66625C`` snapped across, one with a chipped corner, chisel marks ``#4A4642`` on their faces, a broken column drum (B), rubble, a tuft of ash grass; ≈ the plains cluster's 0.6 × 0.5 m |
| ``SM_Rock_March_AshBoulder_A`` | ``boulder@march`` (blocks 1×1) | the plains boulder's mass in the outcrops' dark stone ``#4A4640`` (1.1 m + a 0.5 m stone) under a pale ash crust ``#8A8680`` with drips, a crack, dead grass at its foot |
| ``SM_Outcrop_March_V0..V5`` | ``outcrop_plains@march`` (wall tiles, same variant rule) | the plains outcrop masses (``rocks.outcrop_boulders``) in dark stone ``#4A4640`` under a dull ash crust ``#57534D`` (lit L* 43.5: the dark stone carries the ~800 wall tiles), dead-grass tufts, a dull poppy ``#6E2420`` on V0 / V4; V2 / V5 (the plains hedges) the same masses with a fire-killed oak stump standing out of them (split bark, two broken limbs) |

The meshes keep the plains footprints, blocking and R1 heights; shapes come from ``rocks.py`` / ``envkit.py``. Ink: decor
1.4 px (``Props_March`` = ``Props_Plains``, DECISIONS R14).
"""
from __future__ import annotations

import math
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import envkit_march as EM  # noqa: E402
import envkit as E  # noqa: E402
import foliage as F  # noqa: E402
import rocks as RK  # noqa: E402
import foliage_march as FM  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402

from kit import paths, scene  # noqa: E402
from kit import mesh as M  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

CAT = "Props"
FAM = EM.PROPS


def reg(name: str) -> Region:
    return EM.region(FAM, name)


# Regions on top of A1's colour script (appended; dark, low chroma): line tones of the stones, the outcrops' dull poppy
# (the plains V0 / V4 flower accent, in the foliage's one colour), the outcrop stone and crust (below).
EXTRA = {
    "crack": Region("#66625C").line_region(),
    "poppy": Region("#7A2A26", .42, .30),
    # wall outcrops: ~800 wall tiles per zone must sit below the monsters' lit values (darkgate: monster lit side
    # L* 50-65 on the march ground); A1's outcrop.stone #5E5A54 lit to L* 53 and the ridge out-valued the figures.
    # A darker mass (lit L* 45, shade L* 12) under A1's ash crust (outcrop.cap, lit L* 64): the crust carries the top.
    "stone_dark": Region("#4A4640", .50, .22),
    # … and seen from the W1 pitch the crust is most of a wall tile: A1's outcrop.cap (lit L* 64) turned the ridge
    # into a pale band; a dimmer ash crust (lit L* 54) still reads as ash over the dark stone
    "crust": Region("#64605A", .46, .24),
    # env art review (march): at L* 54-55 lit (highlight bands L* 70-77) the crust was still the brightest large mass
    # of the zone — ~800 wall tiles at the value of hero armour and NPC faces. The crust is a dull ash a step above
    # the stone's base, lit L* 43.5 with a short light band (``outcrop.crust_dim``, aliased onto the "cap" key so
    # the registered outcrop.cap swatch stays as it was): the dark stone carries the tile
    "crust_dim": Region("#57534D", .46, .12),
    # the outcrops' poppy as the field's (foliage_march: poppy.petal lit a dusty pink #A2665B)
    "poppy_dull": Region("#6E2420", .42, .12),
}


def _rx(deg):
    return Matrix.Rotation(math.radians(deg), 4, "X")


def _ry(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Y")


def _rz(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Z")


# ── broken, dressed stone ──────────────────────────────────────────────────────────────────────────────
def block(b: M.Builder, size, center, rot: Matrix, rnd: random.Random, region: str = "stone", chip: bool = False,
          snap: float = 0.0, marks: int = 0, name: str = "block") -> int:
    """A dressed block: bevelled box, ``chip`` knocks a corner off, ``snap`` (0..1) breaks the +X end off on a
    slant with a jagged face (the block was split); ``marks`` chisel strokes across the top face (in the block's
    frame, so they follow its tilt)."""
    sx, sy, sz = size
    p = M.box(size, (0, 0, 0), bevel=0.018, segments=1)
    bm = p.bm
    if chip:
        cx, cy = -sx / 2, rnd.choice((-1, 1)) * sy / 2
        cut = min(sx, sy) * 0.38
        for v in bm.verts:
            d = abs(v.co.x - cx) + abs(v.co.y - cy) + (sz / 2 - v.co.z)
            if d < cut:
                v.co.z -= (cut - d) / cut * cut * 0.6
    if snap > 0:
        # the +X end leans back into a slanted, irregular break
        for v in bm.verts:
            if v.co.x > 0:
                t = (v.co.z / sz + 0.5)
                v.co.x -= snap * sx * 0.45 * t + rnd.uniform(-0.01, 0.012) * (v.co.x > sx * 0.4)
    m = Matrix.Translation(Vector(center)) @ rot
    p.transform(m)
    pid = b.add(p, region, name=name, hull_pad=True)
    for q in range(marks):
        x = -sx * 0.35 + q * sx * 0.16
        a = Vector((x, -sy * 0.32, sz / 2 + 0.002))
        e = Vector((x + sx * 0.06, sy * 0.3, sz / 2 + 0.002))
        pts = [m @ a.lerp(e, t) for t in (0.0, 0.5, 1.0)]
        n = (m.to_3x3() @ Vector((0, 0, 1))).normalized()
        b.add(E.ribbon(pts, [n] * 3, [0.006, 0.01, 0.006], lift=0.0, thick=0.002), "toolmark", name="toolmark",
              outline=False, covered_by=pid)
    return pid


def drum(b: M.Builder, r: float, h: float, center, rot: Matrix, rnd: random.Random, name: str = "drum") -> int:
    """A broken column drum: a fluted-less cylinder whose top snapped on a slant."""
    p = M.cylinder(r, r * 0.97, 0.0, h, sides=12)
    for v in p.bm.verts:
        if v.co.z > h * 0.5:
            v.co.z -= (v.co.x / r + 1.0) * 0.5 * h * 0.35 + rnd.uniform(0, 0.015)
    p.transform(Matrix.Translation(Vector(center)) @ rot)
    return b.add(p, "stone", name=name, hull_pad=True)


ROCK_K = {"A": 1.0, "B": 0.92, "C": 1.06}


def build_broken_stone(pal: Palette, variant: str = "A"):
    name = f"SM_Rock_March_BrokenStone_{variant}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="rock")
    b.regions(stone=reg("rock.stone"), toolmark=reg("rock.toolmark"), crack=EXTRA["crack"],
              dark=reg("ground.dark"), light=reg("ground.light"))
    k = ROCK_K[variant]
    if variant == "B":
        drum(b, 0.17 * k, 0.32 * k, (-0.08, 0.04, -0.01), _rz(rnd.uniform(0, 360)), rnd)
        block(b, (0.3 * k, 0.22 * k, 0.16 * k), (0.2, -0.12, 0.07 * k), _rz(rnd.uniform(-30, 30)) @ _ry(-8), rnd,
              snap=0.6, marks=3, name="block2")
    else:
        block(b, (0.5 * k, 0.3 * k, 0.26 * k), (-0.04, 0.03, 0.12 * k), _rz(rnd.uniform(-25, 25)) @ _rx(2), rnd,
              chip=True, snap=0.5 if variant == "A" else 0.0, marks=4)
        block(b, (0.26 * k, 0.2 * k, 0.16 * k), (0.27 * k, -0.14, 0.09 * k), _rz(rnd.uniform(20, 60)) @ _ry(-18)
              @ _rx(6), rnd, chip=variant == "C", marks=2, name="block2")
    shape = E.RockShape((0.09 * k, 0.08 * k, 0.07 * k), (name, "rubble"), bumps=4, amp=0.08, top_cut=0.3)
    part = E.rock(shape, subdiv=1, facet=26.0, yaw=rnd.uniform(0, 360))
    E.unflare_foot(part, 0.04)
    b.add(part.translate((-0.3 * k, -0.17 * k, 0)), "stone", name="rubble", hull_pad=True)
    if variant != "B":
        F.tuft(b, Vector((0.1 * k, 0.22 * k, 0)), 0.2, 5, rnd, segs=2, inked=True)
    return E.finish(b, pal, "decor", ground_h=0.5, name=name, ground_band=0.02)


# ── ash-crusted boulder ────────────────────────────────────────────────────────────────────────────────
def build_ash_boulder(pal: Palette, variant: str = "A"):
    """The plains boulder (``rocks.build_boulder``: 1.1 m + a 0.5 m stone + a pebble, cracks, tufts at its foot)
    under an ash crust: an open cap (``envkit.rock_cap``) over the top third with three drips, the rock culled under
    it."""
    name = f"SM_Rock_March_AshBoulder_{variant}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="boulder")
    # the outcrops' dark stone under A1's pale ash (boulder.ash): A1's boulder.stone lit to L* 52 and the boulder
    # read brighter than the wall ridge beside it
    b.regions(rock=EXTRA["stone_dark"], ash=reg("boulder.ash"), rock_line=Region("#4A4640").line_region(),
              dark=reg("ground.dark"), light=reg("ground.light"))
    c0 = Vector((-0.08, 0.04, 0))
    shape = E.RockShape((0.56, 0.5, 0.68), (name, 0), bumps=6, amp=0.08, top_cut=0.26)
    edge = E.drip_edge(0.78, (name, "ash"), drips=3, depth=0.3, width=0.18, wobble=0.12)
    part = E.rock(shape, subdiv=2, facet=26.0)
    E.cull_under_cap(part, shape, edge)
    part.translate(c0)
    surf = BVHTree.FromBMesh(part.bm)          # cracks snap onto the facets (no floating strokes)
    pid = b.add(part, "rock", name="boulder")
    cap = E.rock_cap(shape, edge, thick=0.03, cols=28, rows=6, inner=0.05, inner_shell=False)
    b.add(cap.translate(c0), "ash", name="ash_cap")
    RK.crack(b, shape, c0, 0.0, math.radians(-115) + rnd.uniform(-0.3, 0.3), 0.42, 0.12, "rock_line", pid, rnd,
             0.022, surface=surf)
    s2, p2 = RK.add_rock(b, "rock", (0.27, 0.24, 0.27), (name, 1), Vector((0.5, -0.34, 0)),
                         yaw=rnd.uniform(0, math.tau), name="stone")
    RK.crack(b, s2, Vector((0.5, -0.34, 0)), 0.0, rnd.uniform(0, math.tau), 0.32, 0.1, "rock_line", p2, rnd, 0.02)
    RK.add_rock(b, "rock", (0.12, 0.1, 0.09), (name, 2), Vector((-0.58, -0.3, 0)), yaw=rnd.uniform(0, math.tau),
                subdiv=1, name="pebble")
    F.tuft(b, Vector((-0.52, -0.5, 0)), 0.3, 10, rnd, segs=2, inked=True)
    F.tuft(b, Vector((0.22, 0.56, 0)), 0.22, 7, rnd, segs=2, inked=True)
    F.tuft(b, Vector((0.12, -0.56, 0)), 0.18, 6, rnd, segs=2, inked=True)
    return E.finish(b, pal, "decor", ground_h=1.0, name=name, ground_band=0.02)


# ── wall outcrops (tile type 4) ─────────────────────────────────────────────────────────────────────────
def build_outcrop(pal: Palette, v: int):
    """``rocks.build_outcrop`` with the march regions: grey stone masses under an ash crust (``outcrop.cap``), dead
    grass; the hedge variants (V2, V5) carry a fire-killed stump."""
    name = f"SM_Outcrop_March_V{v}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="outcrop")
    b.regions(rock=EXTRA["stone_dark"], rock_line=Region("#4A4640").line_region(), cap=EXTRA["crust"],
              bark=EM.region(EM.FOLIAGE, "deadoak.bark"), char=EM.region(EM.FOLIAGE, "deadoak.char"),
              flower=EXTRA["poppy"], dark=reg("ground.dark"), light=reg("ground.light"))
    # env art review (march): the dimmed crust, the dead oak's dimmed bark on the snag, the dull poppy — new
    # swatches under new names, the keys rocks.outcrop_boulders / the Grower draw with re-pointed to them
    b.swatch["cap"] = b.region("crust_dim", EXTRA["crust_dim"])
    b.swatch["bark"] = b.region("bark_dim", FM.OAK_BARK_DIM)
    b.swatch["flower"] = b.region("poppy_dull", EXTRA["poppy_dull"])
    sph = None
    facet = RK.outcrop_boulders(b, name, v, rnd)
    if v % 3 == 2:
        # the plains hedge variants (a leafy cloud) have no dead counterpart that reads at the W1 pitch (a dry
        # thicket of lumps read as brown potatoes with hairs): V2 / V5 are the stone masses with a fire-killed stump
        # standing out of them — two broken limbs, bark split, the ridge's vertical accent
        stump(b, Vector((-0.3, 0.55, 0.0)), rnd, h=1.75 + 0.15 * (v == 5))
    return E.finish(b, pal, "decor", ground_h=1.2, spherise=sph, name=name, ground_band=0.02, facet=facet)


def stump(b: M.Builder, at: Vector, rnd: random.Random, h: float = 1.0) -> None:
    """A fire-killed snag (the dead oak's trunk broken off at ``h``: flare, a fire scar, split bark) rising behind the
    stone masses, with two broken limbs."""
    lean = Vector((rnd.uniform(-0.08, 0.08), rnd.uniform(-0.08, 0.08), 0.0))
    FM.flared_trunk(b, "bark", h, 0.16, 0.1, 0.7, lean, rnd, lobes=3, sides=12, scar="char",
                    scar_az=math.radians(-90), scar_z=(0.1, 0.8), origin=at)
    gr = FM.Grower(b, "bark", rnd, [(1, 1.0, 0.0, 0.55, 0.25, 0.06), (0, 0.6, 30.0, 0.45, 0.3, 0.02)], ash=None,
                   side_twigs=(), twig_len=0.2)
    a0 = rnd.uniform(0, math.tau)
    for i in range(2):
        az = a0 + math.pi * i + rnd.uniform(-0.4, 0.4)
        tilt = math.radians(rnd.uniform(35, 55))
        d = Vector((math.sin(tilt) * math.cos(az), math.sin(tilt) * math.sin(az), math.cos(tilt)))
        gr.grow(at + lean + Vector((0, 0, h - 0.15 - 0.35 * i)), d, rnd.uniform(0.3, 0.42), 0.06, 0, i)


# ── registry ────────────────────────────────────────────────────────────────────────────────────────────
ASSETS: dict = {}
for _v in "ABC":
    ASSETS[f"SM_Rock_March_BrokenStone_{_v}"] = (lambda pal, v=_v: build_broken_stone(pal, v),
                                                dict(base_ids=["rock"], footprint=[1, 1], blocking=False, blob=0.35))
ASSETS["SM_Rock_March_AshBoulder_A"] = (lambda pal: build_ash_boulder(pal, "A"),
                                        dict(base_ids=["boulder"], footprint=[1, 1], blocking=True, blob=0.7))
for _i in range(6):
    ASSETS[f"SM_Outcrop_March_V{_i}"] = (lambda pal, i=_i: build_outcrop(pal, i),
                                        dict(base_ids=["outcrop_plains"], footprint=[1, 1], blocking=True, blob=0.75))


def ridge_preview(tag: str = "outcrop_ridge.png") -> Path:
    """Wall tiles as ``AAbyssZoneActor::BuildWalls`` places them (variant = tileHash(c, r, 3) % 6, yaw = (hash >> 8)
    % 360), the warrior in front: the march tiles must merge into one ash-crusted ridge."""
    scene.reset()
    pal = Palette(FAM)
    tiles = [(c, 0) for c in range(7)] + [(6, 1), (6, 2)] + [(c, -1) for c in (1, 2, 4)]
    objs = []
    for col, row in tiles:
        h = E.tile_hash(col + 40, row + 12, 3)
        o = build_outcrop(pal, h % 6)
        o.name = f"{o.name}_{col}_{row}"
        E.place(o, col, row, -((h >> 8) % 360))
        objs.append(o)
    pal.save()
    out = paths.preview_root() / EM.FAMILY_DIR / tag
    return EM.scene_shot(objs, E.tile_to_blender(3.6, 0.2, 0.6), out, "MARCH WALL OUTCROPS ON THE TILE GRID  RUNTIME "
                         "VARIANT + YAW  W1 1080P 1:1", size_px=(900, 520),
                         warrior=(*E.tile_to_blender(3.4, -2.0)[:2], -100.0))


def main(argv: list[str]) -> int:
    if "--ridge" in argv:
        ridge_preview()
        return 0
    quick = "--quick" in argv
    sheet = "--sheet" in argv
    previews = "--no-previews" not in argv
    names = [a for a in argv if not a.startswith("--")] or list(ASSETS)
    names = [n for n in ASSETS if any(n == a or a in n for a in names)]
    scene.reset()
    pal = Palette(FAM)
    if sheet or quick:
        objs = [ASSETS[n][0](pal) for n in names]
        pal.save()
        for o in objs:
            o.rotation_euler = (0, 0, math.radians(-135.0))
        tag = "march_rocks_" + "_".join(n.split("_")[-2][:5] + n.split("_")[-1] for n in names)[:80]
        if sheet:
            E.sheet(objs, tag, cols=3)
        else:
            E.quick(objs, tag, spacing=1.7)
        return 0
    shipped = []
    for n in (names if "--family" not in argv else []):
        scene.reset()
        pal = Palette(FAM)
        fn, kw = ASSETS[n]
        obj = fn(pal)
        pal.save()
        extra = {"displayName": n.split("_", 2)[-1]}
        if "Outcrop" in n:
            extra["placement"] = RK.NOTES["outcrop"]
        EM.ship(obj, n, CAT, pal, obj["af_outline_class"], family="environment.rocks", extra=extra,
                previews=previews, **kw)
        shipped.append(n)
    if "--family" in argv:
        shipped = names
    if previews and len(shipped) > 1:
        scene.reset()
        pal = Palette(FAM)
        objs = [ASSETS[n][0](pal) for n in shipped]
        if objs:
            EM.family_sheet(objs, "rocks_outcrops.png", "MARCH BROKEN STONE ASH BOULDER WALL OUTCROPS", cols=4)
        if any("Outcrop" in n for n in shipped):
            ridge_preview()
    return E.summary()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

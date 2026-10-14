"""Emerald Plains rocks and wall outcrops (spec art-inventory-ch1.md §6.2, §6.5, §6.10; web
``Rock.ts``, ``Boulder.ts``, ``OutcropPainter.ts`` mossBoulder; DECISIONS R1 walls 1.4 m, W5).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/rocks.py [names…]
        [--quick] [--no-previews]

| Asset | game id | look |
|---|---|---|
| ``SM_Rock_Plains_Cluster_A/B/C`` | ``rock`` | three faceted rocks ``#8A8E98`` (main ≈ 0.53 × 0.43 m), flat foot, the lit top plane is the key light's band on the cut top, a crack, a grass tuft ``#4F8A3A`` / ``#8CC24F`` |
| ``SM_Rock_Plains_MossBoulder_A`` | ``boulder`` (tall, blocks 1×1) | ≈ 1.1 m boulder ``#8E919C`` + a 0.5 m stone, two cracks, a moss cap, grass tuft |
| ``SM_Outcrop_Plains_V0..V5`` | ``outcrop_plains`` (wall tiles, variant = tileHash(c, r, 3) % 6) | V0 V1 V3 V4: moss-capped boulder cluster ``#9C9888`` / cap ``#7CAA48`` with drips, 1 + v % 2 front stones, tufts ``#5A8A3A`` / ``#A9CC68``; V2 V5: leafy hedge ``#5F9A3E`` + a boulder. ≈ 1.25 m footprint, 1.35–1.45 m tall (R1), round in plan so neighbouring wall tiles (random yaw) merge into one ridge |
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
from mathutils import Matrix, Vector  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402

from kit import paths, scene  # noqa: E402
from kit import mesh as M  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

CAT_ROCK = "Props"


def crack(b: M.Builder, shape: E.RockShape, center: Vector, yaw: float, az: float, z0: float, z1: float,
          region: str, pid: int, rnd: random.Random, width: float = 0.022, surface=None):
    """A zig-zag crack (line tone) running down a rock face at azimuth ``az``. ``surface``: a BVH of the placed rock
    mesh — the crack points snap onto its facets (the analytic surface floats up to ≈ 1 cm over a 320-tri mass's
    chords, and an exact, unpadded hull then leaves the crack standing proud of the ink)."""
    pts, nrm = [], []
    for i in range(5):
        t = i / 4
        a = az + (0.12 if i % 2 else -0.08) * (1 if rnd.random() < 0.5 else 1) + rnd.uniform(-0.04, 0.04)
        # find the polar angle reaching height z on the surface by bisection
        target = z0 + (z1 - z0) * t
        lo, hi = 0.05, math.pi * 0.95
        for _ in range(24):
            mid = (lo + hi) / 2
            d = Vector((math.sin(mid) * math.cos(a), math.sin(mid) * math.sin(a), math.cos(mid)))
            if shape.point(d).z > target:
                lo = mid
            else:
                hi = mid
        d = Vector((math.sin(lo) * math.cos(a), math.sin(lo) * math.sin(a), math.cos(lo)))
        p = shape.point(d)
        eps = 0.02
        dx = shape.point(Vector((math.sin(lo) * math.cos(a + eps), math.sin(lo) * math.sin(a + eps), math.cos(lo)))) - p
        dz = shape.point(Vector((math.sin(lo + eps) * math.cos(a), math.sin(lo + eps) * math.sin(a), math.cos(lo + eps)))) - p
        n = dz.cross(dx).normalized()
        if n.dot(p - Vector((0, 0, shape.R.z * shape.centre))) < 0:
            n = -n
        pts.append(p)
        nrm.append(n)
    if surface is not None:
        rot = Matrix.Rotation(yaw, 3, "Z")
        wp, wn = [], []
        for p, n in zip(pts, nrm):
            q = rot @ p + center
            loc, fn, _, _ = surface.find_nearest(q)
            wp.append(loc if loc is not None else q)
            wn.append(fn if fn is not None and fn.dot(rot @ n) > 0 else rot @ n)
        part = E.ribbon(wp, wn, [width * (1 - 0.6 * i / 4) for i in range(5)], lift=0.002, thick=0.002)
    else:
        part = E.ribbon(pts, nrm, [width * (1 - 0.6 * i / 4) for i in range(5)], lift=0.003, thick=0.003)
        part.rotate("z", math.degrees(yaw))
        part.translate(center)
    # covered by the rock's padded proxy when it has one (outcrop masses bake theirs from a pre-padded proxy)
    b.add(part, region, name="crack", outline=False, covered_by=pid if pid in b._pending else None)


def add_rock(b: M.Builder, region: str, radii, seed, center, yaw: float = 0.0, top_cut: float = 0.3,
             facet: float | None = 26.0, subdiv: int = 2, bumps: int = 5, amp: float = 0.09, name: str = "rock"):
    shape = E.RockShape(radii, seed, bumps=bumps, amp=amp, top_cut=top_cut)
    part = E.rock(shape, center=center, subdiv=subdiv, facet=facet, yaw=math.degrees(yaw))
    pid = b.add(part, region, name=name, hull_pad=True)
    return shape, pid


# ── decor rocks ─────────────────────────────────────────────────────────────────────────────────────────
ROCK_REGIONS = dict(
    rock=Region("#8A8E98", .42, .35),
    rock_line=Region("#8A8E98").line_region(),
    dark=Region("#4F8A3A", .40, .14),
    light=Region("#8CC24F", .38, .12),
)


def build_rock_cluster(pal: Palette, variant: str = "A"):
    name = f"SM_Rock_Plains_Cluster_{variant}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="rock")
    b.regions(**ROCK_REGIONS)
    k = {"A": 1.0, "B": 0.9, "C": 1.08}[variant]
    main = [(0.27 * k, 0.22 * k, 0.27 * k), (-0.06, 0.02, 0)]
    shape, pid = add_rock(b, "rock", main[0], (name, 0), Vector(main[1]), yaw=rnd.uniform(0, math.tau))
    crack(b, shape, Vector(main[1]), 0.0, rnd.uniform(0, math.tau), 0.36 * k, 0.12 * k, "rock_line", pid, rnd)
    add_rock(b, "rock", (0.15 * k, 0.13 * k, 0.14 * k), (name, 1), Vector((0.27 * k, -0.06, 0)),
             yaw=rnd.uniform(0, math.tau), name="rock2")
    add_rock(b, "rock", (0.09 * k, 0.08 * k, 0.07 * k), (name, 2), Vector((-0.3 * k, -0.16 * k, 0)),
             yaw=rnd.uniform(0, math.tau), subdiv=1, name="rock3")
    if variant != "B":
        F.tuft(b, Vector((0.12 * k, 0.2 * k, 0)), 0.2, 5, rnd, segs=2, inked=True)
    return E.finish(b, pal, "decor", ground_h=0.5, name=name)


BOULDER_REGIONS = dict(
    rock=Region("#8E919C", .42, .35),
    rock_line=Region("#8E919C").line_region(),
    moss=Region("#6A9A4A", .40, .30),
    dark=Region("#4F8A3A", .40, .14),
    light=Region("#8CC24F", .38, .12),
)


def build_boulder(pal: Palette, variant: str = "A"):
    """Web ``Boulder.ts``: a faceted 1.1 m boulder (``drawRock`` 50 × 44 px) + a 0.5 m stone beside it, two
    cracks, a big moss tuft ``#4F8A3A`` / ``#8CC24F`` at its foot (the moss blankets belong to the wall
    outcrops)."""
    name = f"SM_Rock_Plains_MossBoulder_{variant}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="boulder")
    b.regions(**BOULDER_REGIONS)
    c0 = Vector((-0.08, 0.04, 0))
    shape = E.RockShape((0.56, 0.5, 0.68), (name, 0), bumps=6, amp=0.08, top_cut=0.26)
    part = E.rock(shape, center=c0, subdiv=2, facet=26.0)
    pid = b.add(part, "rock", name="boulder", hull_pad=True)
    for j in range(2):
        crack(b, shape, c0, 0.0, math.radians(-120) + j * 2.4 + rnd.uniform(-0.3, 0.3), 0.66, 0.16, "rock_line",
              pid, rnd, 0.03)
    s2, p2 = add_rock(b, "rock", (0.27, 0.24, 0.27), (name, 1), Vector((0.5, -0.34, 0)),
                      yaw=rnd.uniform(0, math.tau), name="stone")
    crack(b, s2, Vector((0.5, -0.34, 0)), 0.0, rnd.uniform(0, math.tau), 0.32, 0.1, "rock_line", p2, rnd, 0.02)
    add_rock(b, "rock", (0.12, 0.1, 0.09), (name, 2), Vector((-0.58, -0.3, 0)), yaw=rnd.uniform(0, math.tau),
             subdiv=1, name="pebble")
    F.tuft(b, Vector((-0.52, -0.5, 0)), 0.3, 10, rnd, segs=2, inked=True)
    F.tuft(b, Vector((0.22, 0.56, 0)), 0.22, 7, rnd, segs=2, inked=True)
    F.tuft(b, Vector((0.12, -0.56, 0)), 0.18, 6, rnd, segs=2, inked=True)
    return E.finish(b, pal, "decor", ground_h=1.0, name=name)


# ── wall outcrops (tile type 4) ─────────────────────────────────────────────────────────────────────────
OUTCROP_REGIONS = dict(
    rock=Region("#9C9888", .40, .32),
    rock_line=Region("#9C9888").line_region(),
    cap=Region("#7CAA48", .38, .30),
    hedge=Region("#5F9A3E", .40, .30),
    hedge_back=Region("#4A7E36", .38, .20),
    fleck=Region("#B5D86A", .22, .22),
    flower=Region("#F6D65A", .30, .30),
    dark=Region("#5A8A3A", .40, .14),
    light=Region("#A9CC68", .38, .12),
)


CAP_COLS = (28, 18)      # main mass / front stones (drips need ≥ 0.25 rad columns)
CAP_ROWS = (6, 4)


def outcrop_boulders(b: M.Builder, name: str, v: int, rnd: random.Random):
    """Web ``mossBoulder`` / ``layoutMasses``: one big back mass + 1 + v % 2 smaller **separate** front stones.
    Rounded blocks (superellipsoid, broad shoulders, slightly flattened crown) so neighbouring wall tiles read as one
    lumpy ridge rather than a row of eggs; each mass wears a thick moss blanket over its top third (cap ``#7CAA48``,
    ragged edge, 2–3 short drips, web ``capOver``); grass tufts hug the base.

    Art review 10: the front stones' centres sat inside the big mass, so each variant read as one egg; they now
    stand just outside it (centre 0.58 m from the big mass's, which moved 10 cm back and shrank ≈ 12 %), each with
    its own hull, so their ink separates them from the mass behind. Cap drips and rock faces buried in a neighbour
    mass are culled (drips poked through the rock on the shaded side). The big faces carry creased shading normals
    (``facet``: the shade band snaps instead of an airbrushed ramp) and the masses' hulls are baked from closed
    decimated copies of the rock (the decimated *culled* shells padded up to 7 cm: dark slabs, a 3 px baked rim);
    the caps carry their own hulls."""
    tall = rnd.uniform(1.34, 1.44)
    big_c = Vector((rnd.uniform(-0.04, 0.04), rnd.uniform(0.08, 0.12), 0))
    masses = [(big_c, (rnd.uniform(0.58, 0.63), rnd.uniform(0.54, 0.58), tall / 1.42), 1.0)]
    spots = [-40.0, 42.0, 0.0, -78.0]                      # azimuths from −Y (the front) round the big mass
    for i in range(1 + v % 2):
        az = math.radians(spots[(i + v) % len(spots)] + rnd.uniform(-8, 8))
        dist = 0.58 + rnd.uniform(-0.02, 0.03)
        h = tall * rnd.uniform(0.34, 0.46)
        masses.append((big_c + Vector((math.sin(az) * dist, -math.cos(az) * dist, 0)),
                       (rnd.uniform(0.28, 0.32), rnd.uniform(0.26, 0.30), h / 1.42), 0.62))
    built = []
    for k, (c, r, cov) in enumerate(masses):
        shape = E.RockShape(r, (name, "mass", k), bumps=6, amp=0.07, top_cut=0.1, boxy=2.35, centre=0.42)
        yaw = rnd.uniform(0, 360)
        edge = E.drip_edge(rnd.uniform(0.82, 0.92) if k == 0 else rnd.uniform(0.62, 0.7), (name, "cap", k),
                           drips=2 + (k == 0), depth=0.28 if k == 0 else 0.2, width=0.16, wobble=0.12)
        built.append((shape, yaw, c, edge))

    def inside_other(k):
        others = [(sh, yw, cc) for j, (sh, yw, cc, _) in enumerate(built) if j != k]
        return lambda p: any(E.rock_contains(sh, yw, cc, p, 0.004) for sh, yw, cc in others)
    pids = {}
    for k, (shape, yaw, c, edge) in enumerate(built):
        sub = 3 if k == 0 else 2
        part = E.rock(shape, subdiv=sub, facet=None)
        E.cull_under_cap(part, shape, edge)          # rock hidden under the moss blanket (wall tiles are many)
        part.rotate("z", yaw).translate(c)
        E.cull_inside(part, inside_other(k))         # buried in a neighbour mass
        # the hull is the culled shell itself (exact: no proxy, no padding): its open boundary lies under the moss cap,
        # so its hull's edge does too. Decimated proxies of these 320-tri masses sat up to 4 cm inside the surface
        # and padded into dark slabs (a 3.1 px baked rim on V3 for the 1.4 px decor ink)
        surf = BVHTree.FromBMesh(part.bm) if k == 0 else None
        pid = b.add(part, "rock", name=f"mass{k}")
        pids[pid] = (10 if k == 0 else 7, 0.35)
        thick = 0.034 if k == 0 else 0.024
        cap = E.rock_cap(shape, edge, thick=thick, cols=CAP_COLS[k > 0], rows=CAP_ROWS[k > 0], inner=0.05,
                         inner_shell=False)
        cap.rotate("z", yaw).translate(c)
        E.cull_inside(cap, inside_other(k), any_vertex=True)
        b.add(cap, "cap", name=f"cap{k}")             # its own hull: an open blanket whose lip hides in the rock
        if k == 0:
            # on the +X flank (world azimuth ±20°), clear of the front stones (−Y half) and their buried faces
            crack(b, shape, c, math.radians(yaw), rnd.uniform(-0.35, 0.35) - math.radians(yaw), shape.R.z * 0.6,
                  0.14, "rock_line", pid, rnd, 0.03, surface=surf)
    # tufts hugging the base (web tuftAt at both feet) + a flower accent (theme accent #F6D65A)
    for q in range(3):
        a = rnd.uniform(0, math.tau) if q else math.radians(-100)
        F.tuft(b, Vector((math.cos(a) * 0.6, math.sin(a) * 0.6, 0)), rnd.uniform(0.22, 0.3), 5, rnd, segs=2,
               inked=True, name="tuft")
    if v in (0, 4):
        a = rnd.uniform(0, math.tau)
        p = Vector((math.cos(a) * 0.66, math.sin(a) * 0.66, 0.16))
        b.add(E.bloom(p, 0.05, petals=5, cup=0.15, per_petal=2, walls=True), "flower", name="flower")
    return pids


def outcrop_hedge(b: M.Builder, name: str, v: int, rnd: random.Random):
    """Hedge variant: two scalloped leafy clouds (web hedge) with light leaf flecks, a boulder in front."""
    lumps = [
        (Vector((-0.12, 0.12, 0.62)), (0.62, 0.55, 0.62), "hedge_back", (name, "h0"), False),
        (Vector((0.28, -0.1, 0.48)), (0.45, 0.42, 0.48), "hedge", (name, "h1"), True),
        (Vector((-0.28, -0.18, 0.5)), (0.44, 0.4, 0.5), "hedge", (name, "h2"), True),
        (Vector((0.0, 0.05, 1.0)), (0.42, 0.4, 0.38), "hedge", (name, "h3"), True),
    ]
    sph, pids, _ = F.canopy(b, lumps, rnd, "fleck", flicks_per=4, flick_len=0.16, hull_ratio=F.TREE_HULL,
                            cull_hull=F.TREE_CULL_HULL)
    shape = E.RockShape((0.24, 0.22, 0.26), (name, "stone"), bumps=5, amp=0.08, top_cut=0.2)
    part = E.rock(shape, subdiv=1, facet=None).translate((0.32, -0.48, 0))
    b.add(part, "rock", name="stone", hull_pad=True)
    F.tuft(b, Vector((-0.5, -0.45, 0)), 0.26, 7, rnd, segs=2, inked=True)
    return sph


def build_outcrop(pal: Palette, v: int):
    name = f"SM_Outcrop_Plains_V{v}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="outcrop")
    b.regions(**OUTCROP_REGIONS)
    sph = facet = None
    if v % 3 == 2:
        sph = outcrop_hedge(b, name, v, rnd)
    else:
        facet = outcrop_boulders(b, name, v, rnd)
    return E.finish(b, pal, "decor", ground_h=1.2, spherise=sph, name=name, ground_band=0.02, facet=facet)


# ── terrain skirt ───────────────────────────────────────────────────────────────────────────────────────
# SM_Terrain_Plains_SkirtHill_A..C were retired in art review 9: the runtime skirt (AAbyssZoneActor::BuildSkirt) is
# the terrain heightfield (SkirtHillCm) dressed from the theme's grove pool and never placed game id
# skirt_hill_plains, and the meshes read as flat pale ovals with a hard seam (ground-coloured mounds can only match
# the painted unlit terrain if they sample it). Their FBX, previews and manifest entries are removed; a rebuild
# needs terrain-matched shading and a BuildSkirt hook first (spec §6.10).


# ── registry ────────────────────────────────────────────────────────────────────────────────────────────
ASSETS: dict = {}
for _v in "ABC":
    ASSETS[f"SM_Rock_Plains_Cluster_{_v}"] = (lambda pal, v=_v: build_rock_cluster(pal, v),
                                             dict(cat=CAT_ROCK, game_ids=["rock"], footprint=[1, 1], blocking=False,
                                                  blob=0.35))
ASSETS["SM_Rock_Plains_MossBoulder_A"] = (lambda pal: build_boulder(pal, "A"),
                                          dict(cat=CAT_ROCK, game_ids=["boulder"], footprint=[1, 1], blocking=True,
                                               blob=0.7))
for _i in range(6):
    ASSETS[f"SM_Outcrop_Plains_V{_i}"] = (lambda pal, i=_i: build_outcrop(pal, i),
                                         dict(cat=CAT_ROCK, game_ids=["outcrop_plains"], footprint=[1, 1],
                                              blocking=True, blob=0.75))
NOTES = {
    "outcrop": "wall tile (type 4): one per tile at the tile centre, yaw from tileHash; variant index = "
               "tileHash(col,row,3) % 6 over the variants sorted by name (V2, V5 hedges = web v % 3 == 2); "
               "R1 1.4 m; the tile is already non-walkable (footprint/blocking recorded for W5 completeness)",
}


def ridge_preview(tag: str = "outcrop_ridge.png") -> Path:
    """Wall tiles as the runtime places them (``AAbyssZoneActor::BuildWalls``: variant = tileHash(c, r, 3) % 6,
    yaw = (hash >> 8) % 360): a straight run of 6 tiles turning a corner, the warrior in front for scale — the
    tiles must merge into one moss-topped ridge."""
    scene.reset()
    pal = Palette(E.PROPS)
    tiles = [(c, 0) for c in range(7)] + [(6, 1), (6, 2)] + [(c, -1) for c in (1, 2, 4)]
    objs = []
    for col, row in tiles:
        h = E.tile_hash(col + 40, row + 12, 3)
        v = h % 6
        o = build_outcrop(pal, v)
        o.name = f"{o.name}_{col}_{row}"
        E.place(o, col, row, -((h >> 8) % 360))
        objs.append(o)
    pal.save()
    out = paths.preview_root() / "env_family" / tag
    return E.scene_shot(objs, E.tile_to_blender(3.6, 0.2, 0.6), out, "WALL OUTCROPS ON THE TILE GRID  RUNTIME VARIANT "
                        "+ YAW  W1 1080P 1:1", size_px=(900, 520), warrior=(*E.tile_to_blender(3.4, -2.0)[:2], -100.0))


def main(argv: list[str]) -> int:
    quick = "--quick" in argv
    sheet = "--sheet" in argv
    if "--ridge" in argv:
        ridge_preview()
        return 0
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
        E.quick(objs, "rocks_" + "_".join(n.split("_")[-1] for n in names)[:80], spacing=1.6)
        return 0
    shipped = []
    for n in (names if "--family" not in argv else []):
        scene.reset()
        pal = Palette(E.PROPS)
        fn, kw = ASSETS[n]
        kw = dict(kw)
        cat = kw.pop("cat")
        obj = fn(pal)
        pal.save()
        extra = {"displayName": n.split("_", 2)[-1], "family": "environment.rocks"}
        if "Outcrop" in n:
            extra["placement"] = NOTES["outcrop"]
        E.ship(obj, n, cat, pal, obj["af_outline_class"], extra=extra, previews=previews, **kw)
        shipped.append(n)
    if "--family" in argv:
        shipped = names
    if previews and len(shipped) > 1:
        scene.reset()
        pal = Palette(E.PROPS)
        objs = [ASSETS[n][0](pal) for n in shipped]
        if objs:
            E.family_sheet(objs, "rocks_outcrops.png", "PLAINS ROCKS AND WALL OUTCROPS", cols=4)
        if any("Outcrop" in n for n in shipped):
            ridge_preview()
    return E.summary()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

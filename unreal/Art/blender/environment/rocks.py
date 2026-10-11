"""Emerald Plains rocks, wall outcrops and the terrain skirt (spec art-inventory-ch1.md §6.2, §6.5, §6.10; web
``Rock.ts``, ``Boulder.ts``, ``OutcropPainter.ts`` mossBoulder; DECISIONS R1 walls 1.4 m, W5).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/rocks.py [names…]
        [--quick] [--no-previews]

| Asset | game id | look |
|---|---|---|
| ``SM_Rock_Plains_Cluster_A/B/C`` | ``rock`` | three faceted rocks ``#8A8E98`` (main ≈ 0.53 × 0.43 m), flat foot, the lit top plane is the key light's band on the cut top, a crack, a grass tuft ``#4F8A3A`` / ``#8CC24F`` |
| ``SM_Rock_Plains_MossBoulder_A`` | ``boulder`` (tall, blocks 1×1) | ≈ 1.1 m boulder ``#8E919C`` + a 0.5 m stone, two cracks, a moss cap, grass tuft |
| ``SM_Outcrop_Plains_V0..V5`` | ``outcrop_plains`` (wall tiles, variant = tileHash(c, r, 3) % 6) | V0 V1 V3 V4: moss-capped boulder cluster ``#9C9888`` / cap ``#7CAA48`` with drips, 1 + v % 2 front stones, tufts ``#5A8A3A`` / ``#A9CC68``; V2 V5: leafy hedge ``#5F9A3E`` + a boulder. ≈ 1.25 m footprint, 1.35–1.45 m tall (R1), round in plan so neighbouring wall tiles (random yaw) merge into one ridge |
| ``SM_Terrain_Plains_SkirtHill_A..C`` | ``skirt_hill_plains`` | rolling grass mounds beyond the border ring (no collision), hedge lumps and stones on their shoulders |
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
from mathutils import Vector  # noqa: E402

from kit import scene  # noqa: E402
from kit import mesh as M  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

CAT_ROCK = "Props"
CAT_TERRAIN = "Terrain"


def crack(b: M.Builder, shape: E.RockShape, center: Vector, yaw: float, az: float, z0: float, z1: float,
          region: str, pid: int, rnd: random.Random, width: float = 0.022):
    """A zig-zag crack (line tone) running down a rock face at azimuth ``az``."""
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
        if n.dot(p - Vector((0, 0, shape.R.z * 0.62))) < 0:
            n = -n
        pts.append(p)
        nrm.append(n)
    part = E.ribbon(pts, nrm, [width * (1 - 0.6 * i / 4) for i in range(5)], lift=0.003, thick=0.003)
    part.rotate("z", math.degrees(yaw))
    part.translate(center)
    b.add(part, region, name="crack", outline=False, covered_by=pid)


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
        F.tuft(b, Vector((0.12 * k, 0.2 * k, 0)), 0.2, 7, rnd, segs=2, inked=True)
    return E.finish(b, pal, "decor", ground_h=0.5, name=name)


BOULDER_REGIONS = dict(
    rock=Region("#8E919C", .42, .35),
    rock_line=Region("#8E919C").line_region(),
    moss=Region("#6A9A4A", .40, .30),
    dark=Region("#4F8A3A", .40, .14),
    light=Region("#8CC24F", .38, .12),
)


def build_boulder(pal: Palette, variant: str = "A"):
    name = f"SM_Rock_Plains_MossBoulder_{variant}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="boulder")
    b.regions(**BOULDER_REGIONS)
    c0 = Vector((-0.06, 0.04, 0))
    shape = E.RockShape((0.56, 0.5, 0.66), (name, 0), bumps=6, amp=0.08, top_cut=0.22)
    part = E.rock(shape, center=c0, subdiv=2, facet=24.0)
    cap = E.rock_cap(shape, E.drip_edge(0.62, (name, "cap"), drips=3, depth=0.42), center=c0, thick=0.03)
    pid = b.add(part, "rock", name="boulder", hull_pad=True)
    b.add(cap, "moss", name="moss", hull_pad=True)
    for j in range(2):
        crack(b, shape, c0, 0.0, rnd.uniform(0, math.tau) + j * 2.6, 0.62, 0.15, "rock_line", pid, rnd, 0.028)
    s2, p2 = add_rock(b, "rock", (0.27, 0.24, 0.26), (name, 1), Vector((0.5, -0.32, 0)),
                      yaw=rnd.uniform(0, math.tau), name="stone")
    crack(b, s2, Vector((0.5, -0.32, 0)), 0.0, rnd.uniform(0, math.tau), 0.32, 0.1, "rock_line", p2, rnd, 0.02)
    F.tuft(b, Vector((-0.55, -0.38, 0)), 0.26, 8, rnd, segs=2, inked=True)
    F.tuft(b, Vector((0.2, 0.55, 0)), 0.2, 6, rnd, segs=2, inked=True)
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


def outcrop_boulders(b: M.Builder, name: str, v: int, rnd: random.Random):
    """Big back mass + 1 + v % 2 front stones, each with a draped moss cap (drips) and tufts at the foot."""
    tall = rnd.uniform(1.30, 1.45)
    masses = [(Vector((rnd.uniform(-0.06, 0.06), rnd.uniform(0.0, 0.08), 0)),
               (rnd.uniform(0.56, 0.64), rnd.uniform(0.5, 0.58), tall / 1.62 * 1.0))]
    spots = [(-0.36, -0.38), (0.4, -0.3), (0.0, -0.46), (-0.45, 0.05)]
    for i in range(1 + v % 2):
        sx, sy = spots[(i + v) % len(spots)]
        h = tall * rnd.uniform(0.38, 0.55)
        masses.append((Vector((sx + rnd.uniform(-0.05, 0.05), sy + rnd.uniform(-0.04, 0.04), 0)),
                       (rnd.uniform(0.25, 0.33), rnd.uniform(0.22, 0.3), h / 1.62)))
    for k, (c, r) in enumerate(masses):
        shape = E.RockShape(r, (name, "mass", k), bumps=6, amp=0.07, top_cut=0.0)
        yaw = rnd.uniform(0, 360)
        part = E.rock(shape, subdiv=2 if k == 0 else 1, facet=None)
        part.rotate("z", yaw).translate(c)
        pid = b.add(part, "rock", name=f"mass{k}", hull_pad=True)
        edge = E.drip_edge(rnd.uniform(0.5, 0.6) if k == 0 else rnd.uniform(0.44, 0.54), (name, "cap", k),
                           drips=2 + (k == 0), depth=0.38, width=0.2)
        cap = E.rock_cap(shape, edge, thick=0.035 if k == 0 else 0.025, cols=28 if k == 0 else 18, rows=5 if k == 0 else 4)
        cap.rotate("z", yaw).translate(c)
        b.add(cap, "cap", name=f"cap{k}", hull_pad=True)
        if k == 0:
            crack(b, shape, c, math.radians(yaw), rnd.uniform(0, math.tau), r[2] * 1.05, 0.15, "rock_line", pid,
                  rnd, 0.03)
    # tufts hugging the base + a flower accent (theme accent #F6D65A)
    for q in range(3):
        a = rnd.uniform(0, math.tau)
        F.tuft(b, Vector((math.cos(a) * 0.55, math.sin(a) * 0.55, 0)), rnd.uniform(0.22, 0.3), 7, rnd, segs=2, inked=True,
               name="tuft")
    if v in (0, 4):
        a = rnd.uniform(0, math.tau)
        p = Vector((math.cos(a) * 0.62, math.sin(a) * 0.62, 0.16))
        b.add(E.bloom(p, 0.05, petals=5, cup=0.15, per_petal=2), "flower", name="flower", outline=False)


def outcrop_hedge(b: M.Builder, name: str, v: int, rnd: random.Random):
    """Hedge variant: two scalloped leafy clouds (web hedge) with light leaf flecks, a boulder in front."""
    lumps = [
        (Vector((-0.12, 0.12, 0.62)), (0.62, 0.55, 0.62), "hedge_back", (name, "h0"), False),
        (Vector((0.28, -0.1, 0.48)), (0.45, 0.42, 0.48), "hedge", (name, "h1"), True),
        (Vector((-0.28, -0.18, 0.5)), (0.44, 0.4, 0.5), "hedge", (name, "h2"), True),
        (Vector((0.0, 0.05, 1.0)), (0.42, 0.4, 0.38), "hedge", (name, "h3"), True),
    ]
    sph, pids, _ = F.canopy(b, lumps, rnd, "fleck", flicks_per=4, flick_len=0.16)
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
    sph = None
    if v % 3 == 2:
        sph = outcrop_hedge(b, name, v, rnd)
    else:
        outcrop_boulders(b, name, v, rnd)
    return E.finish(b, pal, "decor", ground_h=1.2, spherise=sph, name=name)


# ── terrain skirt ───────────────────────────────────────────────────────────────────────────────────────
SKIRT_REGIONS = dict(
    grass=Region("#74A247", .16, .12),
    grass_dark=Region("#6C9A43", .16, .10),
    hedge=Region("#5F9A3E", .40, .30),
    hedge_back=Region("#4A7E36", .38, .20),
    fleck=Region("#B5D86A", .22, .22),
    rock=Region("#9C9888", .40, .32),
    dark=Region("#5A8A3A", .40, .14),
    light=Region("#A9CC68", .38, .12),
)


def hill(size, height: float, seed, cols: int = 28, rows: int = 7):
    """A rolling mound: smooth superellipse plan ``size`` (x, y half-extents), cosine profile with gentle
    bumps, open bottom sunk 0.1 m (terrain hides it)."""
    rnd = random.Random(scene.seed_for("hill", seed))
    ph = [rnd.uniform(0, math.tau) for _ in range(4)]
    rings = []
    for j in range(rows + 1):
        t = j / rows                       # 0 rim → 1 top
        rad = math.cos(t * math.pi / 2) ** 0.8
        z = height * math.sin(t * math.pi / 2) ** 1.4 - 0.1 * (1 - t)
        ring = []
        for i in range(cols):
            a = math.tau * i / cols
            wob = 1 + 0.08 * math.sin(2 * a + ph[0]) + 0.05 * math.sin(3 * a + ph[1])
            zz = z * (1 + 0.12 * math.sin(a * 2 + ph[2]) * t)
            ring.append(Vector((math.cos(a) * size[0] * rad * wob, math.sin(a) * size[1] * rad * wob, zz)))
        rings.append(ring)
    rings[-1] = [Vector((0, 0, height * (1 + 0.04)))] * cols
    p = E.ring_loft(rings[:-1] + [[Vector((r.x * 0.02, r.y * 0.02, height)) for r in rings[-2]]], cap0=None,
                    cap1="fan")
    return p


def build_skirt(pal: Palette, variant: str):
    """Terrain skirt mound: ground, so no ink (spec §1.3) and low-contrast bands (s .16, l .12) that melt into
    the terrain grass; darker grass patches and a few painted tufts on the slopes. The grove foliage on and
    round it is placed by the runtime (AAbyssZoneActor::BuildSkirt)."""
    name = f"SM_Terrain_Plains_SkirtHill_{variant}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="skirt")
    b.regions(**SKIRT_REGIONS)
    size, h = {"A": ((3.2, 2.4), 1.25), "B": ((4.4, 2.8), 1.7), "C": ((2.6, 2.0), 0.95)}[variant]
    hp = hill(size, h, name)
    hid = b.add(hp, "grass", name="hill", outline=False)
    sph = {hid: (Vector((0, 0, -h * 0.6)), Vector((size[0] * 1.2, size[1] * 1.2, h * 2.2)), 0.6)}

    def surf(x, y):
        """Hill surface point / normal at (x, y) (numerical, from the cosine profile)."""
        rr = math.hypot(x / size[0], y / size[1])
        t = 1.0 - min(rr, 1.0)
        z = h * math.sin(max(t, 0.0) * math.pi / 2) ** 1.4 if rr < 1 else 0.0
        return z

    for q in range({"A": 4, "B": 6, "C": 3}[variant]):
        a0 = rnd.uniform(0, math.tau)
        rr = rnd.uniform(0.25, 0.7)
        cx, cy = math.cos(a0) * size[0] * rr, math.sin(a0) * size[1] * rr
        pts, nrm = [], []
        for k in range(5):
            x = cx + (k - 2) * 0.22 * math.cos(a0 + 1.4)
            y = cy + (k - 2) * 0.22 * math.sin(a0 + 1.4)
            z = surf(x, y)
            e = 0.05
            nx = (surf(x - e, y) - surf(x + e, y)) / (2 * e)
            ny = (surf(x, y - e) - surf(x, y + e)) / (2 * e)
            pts.append(Vector((x, y, z)))
            nrm.append(Vector((nx, ny, 1.0)).normalized())
        b.add(E.ribbon(pts, nrm, [0.35, 0.55, 0.62, 0.5, 0.3], lift=0.006, thick=0.004), "grass_dark",
              name="patch", outline=False)
    for q in range({"A": 5, "B": 7, "C": 4}[variant]):
        a = rnd.uniform(0, math.tau)
        rr = rnd.uniform(0.15, 0.85)
        x, y = math.cos(a) * size[0] * rr, math.sin(a) * size[1] * rr
        F.tuft(b, Vector((x, y, surf(x, y) - 0.01)), rnd.uniform(0.22, 0.32), 6, rnd, segs=2)
    return E.finish(b, pal, "none", ground_h=None, spherise=sph, name=name)


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
for _v in "ABC":
    ASSETS[f"SM_Terrain_Plains_SkirtHill_{_v}"] = (lambda pal, v=_v: build_skirt(pal, v),
                                                  dict(cat=CAT_TERRAIN, game_ids=["skirt_hill_plains"],
                                                       blocking=False, blob=0.01, ground=False))
NOTES = {
    "outcrop": "wall tile (type 4): one per tile at the tile centre, yaw from tileHash; variant index = "
               "tileHash(col,row,3) % 6 over the variants sorted by name (V2, V5 hedges = web v % 3 == 2); "
               "R1 1.4 m; the tile is already non-walkable (footprint/blocking recorded for W5 completeness)",
    "skirt": "terrain skirt (spec 6.10): scenery beyond the border ring, no collision, no gameplay footprint",
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
        E.quick(objs, "rocks_" + "_".join(n.split("_")[-1] for n in names)[:80],
                spacing=1.6 if all("Skirt" not in n for n in names) else 7.0)
        return 0
    shipped = []
    for n in names:
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
        if "Skirt" in n:
            extra["placement"] = NOTES["skirt"]
        E.ship(obj, n, cat, pal, obj["af_outline_class"], extra=extra, previews=previews,
               turnaround="Skirt" not in n, **kw)
        shipped.append(n)
    if previews and len(shipped) > 1:
        scene.reset()
        pal = Palette(E.PROPS)
        objs = [ASSETS[n][0](pal) for n in shipped if "Skirt" not in n]
        if objs:
            E.family_sheet(objs, "rocks_outcrops.png", "PLAINS ROCKS AND WALL OUTCROPS", cols=4, half_w=0.06,
                           focus_z=0.6)
    return E.summary()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

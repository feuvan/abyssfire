"""Emerald Plains foliage (spec art-inventory-ch1.md §6.5, §6.1 lily pads; web ``Tree.ts``, ``Bush.ts``,
``Flower.ts``, ``Mushroom.ts``; DECISIONS W5).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/foliage.py [names…]
        [--quick] [--no-previews]

| Asset | game id | look |
|---|---|---|
| ``SM_Foliage_Plains_Oak_A/B/C`` | ``tree`` (tall, blocks 1×1) | broad sunlit oak ≈ 4.1 m: flared trunk ``#80573A`` with bark grooves, two limbs, cloud canopy of dark ``#3C7040`` core lumps under light ``#62A23C`` lumps, leaf flicks ``#D6EC86`` |
| ``SM_Foliage_Plains_FruitTree_A/B`` | ``tree_round`` (tall, blocks) | ≈ 3.2 m, trunk ``#8A6040`` with a lean, lumps ``#4D8343`` / ``#7FB13F``, flicks ``#EEF29A``, red fruits ``#D8502E`` (focal accent) |
| ``SM_Foliage_Plains_BerryBush_A/B`` | ``bush`` | ≈ 1.15 × 0.8 m, lumps ``#376A3C`` / ``#5C9C3C``, flicks ``#D2EA83``, berries ``#C8324A`` |
| ``SM_Foliage_Plains_GrassTuft_A/B/C`` | ``grass`` (flat ground cover, no ink) | three fans of tapered blades ``#3F7A33`` / ``#8CC24F`` to ≈ 0.43 m, two tip flowers ``#F2E27A`` |
| ``SM_Foliage_Plains_FlowerPatch_A/B`` | ``flower`` (flat, no ink) | two tufts + six five-petal blooms ``#F2D24A #FFFFFF #E86A8A #B48CFF``, centres ``#F4A93A`` |
| ``SM_Foliage_Plains_RedMushrooms_A/B`` | ``mushroom_red`` | two toadstools, caps ``#D2402E`` with ``#FFF4E0`` spots, pale stems, a grass tuft |
| ``SM_Foliage_Plains_LilyPad_A/B`` | ``lily_pad_plains`` | notched pads ``#6FAE4A`` (B with a ``#F4D0E0`` water lily), floating at the water surface |

Foliage uses ``M_AF_Toon_Foliage`` (wind WPO) with spherised normals per canopy lump and the wind weights of
``envkit.set_wind``; trees, bushes and mushrooms carry the decor ink (1.2 px, line tone); grass and flowers are
ground cover without ink (spec §1.3) and use their own instance ``MI_AF_Toon_Foliage_Plains_Ground`` (low, gentle
sway).
"""
from __future__ import annotations

import math
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import envkit as E  # noqa: E402
import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402
from typing import Sequence  # noqa: E402
from mathutils.bvhtree import BVHTree  # noqa: E402

from kit import scene  # noqa: E402
from kit import mesh as M  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

CAT = "Foliage"
GROUND_SLOT = "MI_AF_Toon_Foliage_Plains_Ground"
GROUND_WIND = {"WindAmplitudeCm": 2.0, "WindHeightCm": 45.0, "WindSpeed": 1.6}


# ── shared foliage pieces ───────────────────────────────────────────────────────────────────────────────
def surface_hit(part, center: Vector, d: Vector):
    """(point, normal) where the ray from ``center`` along ``d`` leaves the closed part."""
    bvh = BVHTree.FromBMesh(part.bm)
    far = center + d.normalized() * 20.0
    loc, nrm, _, _ = bvh.ray_cast(far, -d.normalized())
    if loc is None:
        return center, d.normalized()
    return loc, nrm


def flick(part, center: Vector, az: float, el: float, length: float, width: float, rnd: random.Random,
          lift: float = 0.006):
    """A leaf-flick stroke lying on a lump (web canopy() leaf flicks): a tapered strip curving over the
    surface, sloping down-and-sideways from its base."""
    pts, nrms = [], []
    tilt = rnd.uniform(0.35, 0.75) * (1 if rnd.random() < 0.5 else -1)
    for i in range(5):
        t = i / 4
        a = az + tilt * t * length
        e = el - 0.55 * t * length
        d = Vector((math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e)))
        p, n = surface_hit(part, center, d)
        pts.append(p)
        nrms.append(n)
    widths = [width * (0.35 + 0.65 * math.sin(math.pi * min(0.95, 0.15 + t * 0.85))) for t in
              (i / 4 for i in range(5))]
    return E.ribbon(pts, nrms, widths, lift=lift, thick=0.004)


def lump_kw(r, outer: bool) -> dict:
    """Cloud-lump parameters: outer (visible) lumps get many small lobes at subdiv 3 (scalloped like the web
    cloudP), core lumps fewer, coarser — they are mostly hidden and culled."""
    big = max(r) > 0.45
    if outer:
        return dict(lobes=12 if big else 9, core=0.78 if big else 0.76, rho_lo=0.36 if big else 0.4,
                    rho_hi=0.46 if big else 0.5, subdiv=3)
    return dict(lobes=10, core=0.77, rho_lo=0.38, rho_hi=0.48, subdiv=3 if max(r) > 0.3 else 2)


def canopy(b: M.Builder, lumps, rnd: random.Random, flick_region: str | None, flicks_per: int = 4,
           flick_len: float = 0.17, extra_solids: Sequence = ()):
    """Add cloud lumps ``[(center, radii, region, seed, outer?)]``: faces hidden inside a neighbour are culled,
    each lump gets a padded hull and (outer lumps) leaf flicks on visible spots. Returns (spherise map, lump
    part ids, Lump objects)."""
    objs = [E.Lump(r, seed, center=Vector(c), **lump_kw(r, outer)) for c, r, region, seed, outer in lumps]
    parts = [o.part() for o in objs]
    sph, pids = {}, []
    for k, ((c, r, region, seed, outer), lo, part) in enumerate(zip(lumps, objs, parts)):
        c = Vector(c)
        others = [o for j, o in enumerate(objs) if j != k] + list(extra_solids)
        flick_parts = []
        if outer and flick_region:
            tries = 0
            while len(flick_parts) < flicks_per and tries < flicks_per * 4:
                tries += 1
                az = rnd.uniform(0, math.tau)
                el = rnd.uniform(0.3, 1.1)
                fp = flick(part, c, az, el, flick_len * rnd.uniform(0.85, 1.15) / max(r[0], 0.2) * 1.3, 0.042, rnd)
                mid = sum((v.co for v in fp.bm.verts), Vector()) / max(len(fp.bm.verts), 1)
                if any(o.inside(mid, -0.01) for o in others):
                    fp.bm.free()
                    continue
                flick_parts.append(fp)
        E.cull_hidden(part, others)
        pid = b.add(part, region, name=f"lump{k}", hull_pad=True)
        for fp in flick_parts:
            b.add(fp, flick_region, name=f"flick{k}", outline=False, covered_by=pid)
        sph[pid] = E.lump_info(r, c, 0.6)
        pids.append(pid)
    return sph, pids, objs


def trunk_radius(z: float, r_base: float, r_top: float, h: float, flare: float, lobes: int, ph: float, a: float):
    rz = r_base + (r_top - r_base) * min(1.0, z / h) ** 0.8
    f = flare * math.exp(-z / 0.16)
    return rz * (1 + f * max(0.0, math.cos(lobes * (a - ph))) ** 1.5 + 0.25 * f)


def trunk(b: M.Builder, region: str, groove_region: str, h: float, r_base: float, r_top: float, flare: float,
          lean: Vector, rnd: random.Random, lobes: int = 5, sides: int = 14, grooves: int = 3):
    ph = rnd.uniform(0, math.tau)
    zs = [-0.03, 0.04, 0.12, 0.25, 0.45, 0.8, 1.2, h * 0.85, h]
    rings = []
    for z in zs:
        off = lean * (max(z, 0) / h) ** 1.6
        rings.append([Vector((math.cos(a) * rr, math.sin(a) * rr, z)) + off for a, rr in
                      ((math.tau * i / sides, trunk_radius(max(z, 0), r_base, r_top, h, flare, lobes, ph,
                                                           math.tau * i / sides)) for i in range(sides))])
    part = E.ring_loft(rings, cap0="ngon", cap1="fan")
    tid = b.add(part, region, name="trunk", hull_pad=True)
    # bark grooves (web trunk(): 2–3 shade strokes)
    for g in range(grooves):
        a0 = ph + math.tau * (g + rnd.uniform(0.2, 0.8)) / grooves
        pts, nrm = [], []
        z0, z1 = rnd.uniform(0.15, 0.3), h * rnd.uniform(0.55, 0.8)
        for i in range(6):
            t = i / 5
            z = z0 + (z1 - z0) * t
            a = a0 + 0.25 * math.sin(t * 2.5 + g)
            rr = trunk_radius(z, r_base, r_top, h, flare, lobes, ph, a)
            off = lean * (z / h) ** 1.6
            p = Vector((math.cos(a) * rr, math.sin(a) * rr, z)) + off
            pts.append(p)
            nrm.append(Vector((math.cos(a), math.sin(a), 0)))
        w = [0.02 * (0.6 + 0.4 * math.sin(math.pi * i / 5)) + 0.006 for i in range(6)]
        b.add(E.ribbon(pts, nrm, w, lift=0.002, thick=0.003), groove_region, name="groove", outline=False,
              covered_by=tid)
    return tid


def limb(b: M.Builder, region: str, a: Vector, c: Vector, e: Vector, ra: float, re: float, name: str = "limb"):
    pts = [a + (c - a) * t * 2 if t < 0.5 else c + (e - c) * (t - 0.5) * 2 for t in (0, 0.25, 0.5, 0.75, 1.0)]
    # quadratic Bezier through a, c (control), e
    pts = [a * (1 - t) ** 2 + c * 2 * (1 - t) * t + e * t * t for t in (0, 0.25, 0.5, 0.75, 1.0)]
    radii = [ra + (re - ra) * t for t in (0, 0.25, 0.5, 0.75, 1.0)]
    return b.add(M.tube(pts, radii, sides=8), region, name=name)


def tree_wind(trunk_h: float, top: float, lump_pids: list[int]):
    phase = {p: (i * 0.618) % 1.0 for i, p in enumerate(lump_pids)}

    def fn(co: Vector, pid: int):
        if pid in phase:
            return 0.35 + 0.65 * E.smoothstep(trunk_h, top, co.z), phase[pid]
        return 0.3 * E.smoothstep(trunk_h * 0.45, trunk_h * 1.25, co.z), 0.0
    return fn


# ── trees ───────────────────────────────────────────────────────────────────────────────────────────────
OAK_REGIONS = dict(
    bark=Region("#80573A", .45, .25),
    bark_groove=Region("#80573A", .45, .25).shade_region(.30, .10),
    back=Region("#3C7040", .35, .18),
    front=Region("#62A23C", .40, .30),
    flick=Region("#D6EC86", .22, .25),
)
OAK_VARIANTS = {
    # (height of the crown base, canopy scale, lean, seed tweak, extra lump spread)
    "A": dict(trunk_h=1.9, scale=1.0, lean=(0.05, 0.0, 0.0), spread=1.0),
    "B": dict(trunk_h=1.75, scale=1.08, lean=(-0.10, 0.06, 0.0), spread=1.12),
    "C": dict(trunk_h=2.05, scale=0.93, lean=(0.02, -0.08, 0.0), spread=0.9),
}


def build_oak(pal: Palette, variant: str = "A"):
    name = f"SM_Foliage_Plains_Oak_{variant}"
    v = OAK_VARIANTS[variant]
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="oak")
    b.regions(**OAK_REGIONS)
    th, k, sp = v["trunk_h"], v["scale"], v["spread"]
    lean = Vector(v["lean"])
    trunk(b, "bark", "bark_groove", th + 0.45, 0.24, 0.15, 1.35, lean, rnd)
    top = lean + Vector((0, 0, th + 0.4))
    a0 = rnd.uniform(0, math.tau)
    for i in range(2):
        a = a0 + math.pi * i + rnd.uniform(-0.4, 0.4)
        d = Vector((math.cos(a), math.sin(a), 0))
        st = lean * ((th - 0.35) / (th + 0.45)) ** 1.6 + Vector((0, 0, th - 0.35))
        limb(b, "bark", st, st + d * 0.25 + Vector((0, 0, 0.35)), st + d * 0.62 * sp + Vector((0, 0, 0.75)),
             0.075, 0.04, name=f"limb{i}")
    lumps = []
    base = th + 0.55
    # dark core / underside lumps (web back lumps) …
    for i in range(3):
        a = a0 + math.tau * i / 3 + 0.5
        c = top + Vector((math.cos(a) * 0.38 * sp, math.sin(a) * 0.38 * sp, base - th - 0.4 + 0.35 + 0.15 * i))
        lumps.append((c, (0.72 * k, 0.72 * k, 0.56 * k), "back", (name, "core", i), False))
    # … under the light outer lumps (web front lumps) with leaf flicks
    for i in range(4):
        a = a0 + math.tau * i / 4 + rnd.uniform(-0.25, 0.25)
        rr = 0.66 * sp
        c = top + Vector((math.cos(a) * rr, math.sin(a) * rr, base - th - 0.4 + 0.25 + rnd.uniform(-0.08, 0.1)))
        r0 = rnd.uniform(0.55, 0.64) * k
        lumps.append((c, (r0, r0, r0 * 0.8), "front", (name, "ring", i), True))
    for i in range(2):
        a = a0 + math.pi * i + 0.8
        c = top + Vector((math.cos(a) * 0.32 * sp, math.sin(a) * 0.32 * sp, base - th - 0.4 + 0.95))
        r0 = rnd.uniform(0.56, 0.62) * k
        lumps.append((c, (r0, r0, r0 * 0.82), "front", (name, "up", i), True))
    c = top + Vector((rnd.uniform(-0.08, 0.08), rnd.uniform(-0.08, 0.08), base - th - 0.4 + 1.42 * k))
    lumps.append((c, (0.48 * k, 0.48 * k, 0.4 * k), "front", (name, "top", 0), True))
    sph, pids, _ = canopy(b, lumps, rnd, "flick", flicks_per=5)
    hi = max(l[0].z + l[1][2] for l in lumps)
    obj = E.finish(b, pal, "decor", ground_h=1.6, spherise=sph, wind=tree_wind(th, hi, pids), name=name)
    return obj



FRUIT_REGIONS = dict(
    bark=Region("#8A6040", .45, .25),
    bark_groove=Region("#8A6040", .45, .25).shade_region(.30, .10),
    back=Region("#4D8343", .35, .18),
    front=Region("#7FB13F", .40, .30),
    flick=Region("#EEF29A", .22, .25),
    fruit=Region("#D8502E", .42, .45),
    fruit_hi=Region("#FFD8A8", .20, .20),
)


def visible_spots(lobjs, rnd: random.Random, a0: float, per, el=(0.0, 0.6), lift: float = 0.03):
    """Points on the visible surface of lumps (not inside a neighbour), ``per[k]`` per lump, spread in
    azimuth — fruit and berries."""
    out = []
    for j, lo in enumerate(lobjs):
        n_ok, tries = 0, 0
        while n_ok < (per[j] if j < len(per) else 1) and tries < 20:
            tries += 1
            az = a0 + j * 2.1 + n_ok * 2.2 + rnd.uniform(-0.5, 0.5)
            e = rnd.uniform(*el)
            d = Vector((math.cos(e) * math.cos(az), math.cos(e) * math.sin(az), math.sin(e)))
            q = d.copy()
            r = lo.radial(Vector((q.x, q.y, q.z if q.z >= 0 else q.z)).normalized())
            p = lo.C + Vector((d.x * r * lo.R.x, d.y * r * lo.R.y, d.z * r * lo.R.z * (lo.squash if d.z < 0 else 1)))
            n = Vector(((p - lo.C).x / lo.R.x ** 2, (p - lo.C).y / lo.R.y ** 2, (p - lo.C).z / lo.R.z ** 2)).normalized()
            p = p + n * lift
            if any(o.inside(p, 0.0) for o in lobjs if o is not lo):
                continue
            out.append(p)
            n_ok += 1
    return out


def fruit(b: M.Builder, lump_part_pid, c: Vector, r: float, region="fruit", name="fruit"):
    f = M.ellipsoid((r, r, r * 0.95), c, sides=10, rings=6)
    return b.add(f, region, name=name)


def build_fruit_tree(pal: Palette, variant: str = "A"):
    name = f"SM_Foliage_Plains_FruitTree_{variant}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="fruittree")
    b.regions(**FRUIT_REGIONS)
    th = 1.6 if variant == "A" else 1.5
    lean = Vector((-0.12, 0.05, 0)) if variant == "A" else Vector((0.08, 0.10, 0))
    k = 1.0 if variant == "A" else 1.06
    trunk(b, "bark", "bark_groove", th + 0.35, 0.16, 0.10, 1.1, lean, rnd, lobes=4, sides=12, grooves=2)
    top = lean + Vector((0, 0, th + 0.3))
    a0 = rnd.uniform(0, math.tau)
    st = lean * ((th - 0.3) / (th + 0.35)) ** 1.6 + Vector((0, 0, th - 0.3))
    d = Vector((math.cos(a0), math.sin(a0), 0))
    limb(b, "bark", st, st + d * 0.2 + Vector((0, 0, 0.3)), st + d * 0.48 + Vector((0, 0, 0.62)), 0.06, 0.035)
    lumps = []
    z0 = th + 0.45
    for i in range(2):
        a = a0 + math.pi * i + 0.9
        c = top + Vector((math.cos(a) * 0.28, math.sin(a) * 0.28, z0 - th - 0.3 + 0.3))
        lumps.append((c, (0.6 * k, 0.6 * k, 0.48 * k), "back", (name, "core", i), False))
    for i in range(3):
        a = a0 + math.tau * i / 3 + rnd.uniform(-0.2, 0.2)
        c = top + Vector((math.cos(a) * 0.5 * k, math.sin(a) * 0.5 * k, z0 - th - 0.3 + 0.22 + rnd.uniform(-0.05, 0.08)))
        r0 = rnd.uniform(0.48, 0.54) * k
        lumps.append((c, (r0, r0, r0 * 0.82), "front", (name, "ring", i), True))
    c = top + Vector((0.05, -0.04, z0 - th - 0.3 + 0.78 * k))
    lumps.append((c, (0.5 * k, 0.5 * k, 0.4 * k), "front", (name, "top", 0), True))
    sph, pids, lobjs = canopy(b, lumps, rnd, "flick", flicks_per=4, flick_len=0.15)
    # five+ ripe fruits (web: 5 on the front lumps; all round here so every yaw shows a few)
    fr = visible_spots(lobjs[2:], rnd, a0, per=(2, 2, 2, 1), el=(-0.05, 0.55), lift=0.05)
    for q, p in enumerate(fr):
        fid = fruit(b, None, p, 0.078, name=f"fruit{q}")
    hi = max(l[0].z + l[1][2] for l in lumps)
    return E.finish(b, pal, "decor", ground_h=1.4, spherise=sph, wind=tree_wind(th, hi, pids), name=name)


BUSH_REGIONS = dict(
    back=Region("#376A3C", .35, .18),
    front=Region("#5C9C3C", .40, .30),
    flick=Region("#D2EA83", .22, .25),
    berry=Region("#C8324A", .42, .50),
    stem=Region("#5A4028", .45, .25),
)


def build_bush(pal: Palette, variant: str = "A"):
    name = f"SM_Foliage_Plains_BerryBush_{variant}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="bush")
    b.regions(**BUSH_REGIONS)
    w = 1.0 if variant == "A" else 1.08
    lumps = []
    a0 = rnd.uniform(0, math.tau)
    for i in range(2):
        a = a0 + math.pi * i
        c = Vector((math.cos(a) * 0.16, math.sin(a) * 0.16, 0.34))
        lumps.append((c, (0.42 * w, 0.42 * w, 0.36), "back", (name, "core", i), False))
    n_front = 4 if variant == "A" else 5
    for i in range(n_front):
        a = a0 + math.tau * i / n_front + rnd.uniform(-0.2, 0.2)
        rr = 0.3 * w
        c = Vector((math.cos(a) * rr, math.sin(a) * rr, 0.26 + rnd.uniform(0, 0.08)))
        r0 = rnd.uniform(0.26, 0.31) * w
        lumps.append((c, (r0, r0, r0 * 0.9), "front", (name, "ring", i), True))
    c = Vector((rnd.uniform(-0.05, 0.05), rnd.uniform(-0.05, 0.05), 0.5))
    lumps.append((c, (0.3 * w, 0.3 * w, 0.26), "front", (name, "top", 0), True))
    sph, pids, lobjs = canopy(b, lumps, rnd, "flick", flicks_per=3, flick_len=0.11)
    berries = visible_spots(lobjs[2:], rnd, a0, per=(2,) * 6, el=(0.0, 0.7), lift=0.025)
    for q, p in enumerate(berries[:9]):
        b.add(M.ellipsoid((0.045, 0.045, 0.043), p, sides=8, rings=5), "berry", name=f"berry{q}")
    phase = {p: (i * 0.618) % 1.0 for i, p in enumerate(pids)}
    return E.finish(b, pal, "decor", ground_h=0.8, spherise=sph,
                    wind=lambda co, pid: (0.2 + 0.8 * E.smoothstep(0.05, 0.75, co.z), phase.get(pid, 0.3)), name=name)


# ── ground cover (no ink) ───────────────────────────────────────────────────────────────────────────────
GRASS_REGIONS = dict(
    dark=Region("#3F7A33", .40, .14),
    light=Region("#8CC24F", .38, .12),
    tipflower=Region("#F2E27A", .25, .30),
)


def tuft(b: M.Builder, c: Vector, h: float, n: int, rnd: random.Random, regions=("dark", "light"),
         spread: float = 1.0, name: str = "tuft", sph: dict | None = None, segs: int = 3, inked: bool = False):
    """A fan of tapered blades (web tuft()) splayed all round ``c``: a few tall near-upright blades in the
    middle, the rest leaning out 30–55°, so the tuft reads as a star from the high W1 camera and from every yaw;
    mostly dark blades, every third light; normals spherised toward the tuft's dome (soft cel bands)."""
    pids = []
    a0 = rnd.uniform(0, math.tau)
    n_in = max(1, n // 4)
    for i in range(n):
        inner = i < n_in
        az = a0 + math.tau * (i / max(n_in, 1) if inner else (i - n_in) / max(n - n_in, 1)) + rnd.uniform(-0.3, 0.3)
        lean = (rnd.uniform(0.12, 0.3) if inner else rnd.uniform(0.6, 1.15)) * spread
        d = Vector((math.cos(az) * lean, math.sin(az) * lean, 1.0)).normalized()
        L = h * (rnd.uniform(0.85, 1.0) if inner else rnd.uniform(0.5, 0.78))
        base = c + Vector((math.cos(az) * 0.015, math.sin(az) * 0.015, -0.01))
        side = Vector((-math.sin(az), math.cos(az), 0))
        part = E.blade(base, d, L, 0.07 * (h / 0.4) ** 0.5, bend=0.12 if inner else 0.28, side=side, segs=segs,
                       walls=inked, thick=0.012 if inked else 0.004)
        region = regions[1] if (i % 3 == 1) else regions[0]
        # inked: a tuft hugging an outlined prop keeps the prop's line art (its blades are part of the silhouette)
        pid = b.add(part, region, name=name, outline=inked)
        pids.append(pid)
        if sph is not None:
            sph[pid] = (c + Vector((0, 0, h * 0.15)), Vector((h * 0.7, h * 0.7, h * 0.8)), 0.65)
    return pids


GRASS_VARIANTS = {"A": (0.43, 11, 7, 5), "B": (0.36, 10, 7, 5), "C": (0.40, 12, 6, 4)}


def build_grass(pal: Palette, variant: str = "A"):
    name = f"SM_Foliage_Plains_GrassTuft_{variant}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="grass")
    b.regions(**GRASS_REGIONS)
    h, n1, n2, n3 = GRASS_VARIANTS[variant]
    sph = {}
    tuft(b, Vector((-0.06, 0.03, 0)), h, n1, rnd, sph=sph)
    tuft(b, Vector((0.12, -0.06, 0)), h * 0.72, n2, rnd, sph=sph, segs=2)
    tuft(b, Vector((-0.15, -0.12, 0)), h * 0.48, n3, rnd, sph=sph, segs=2)
    for q, (x, y, z) in enumerate(((-0.12, 0.0, h * 0.86), (0.13, -0.06, h * 0.62))[: 2 if variant != "B" else 1]):
        b.add(E.bloom((x, y, z), 0.022, petals=5, cup=0.2, per_petal=2), "tipflower", name="tipflower",
              outline=False)
    return E.finish(b, pal, "none", ground_h=0.5, spherise=sph,
                    wind=lambda co, pid: (E.smoothstep(0.0, 0.42, co.z), (co.x * 3.1 + co.y * 1.7) % 1.0),
                    name=name)


FLOWER_REGIONS = dict(
    dark=Region("#3F7A33", .40, .14),
    light=Region("#8CC24F", .38, .12),
    yellow=Region("#F2D24A", .35, .35),
    white=Region("#FFFFFF", .30, .20),
    pink=Region("#E86A8A", .38, .35),
    violet=Region("#B48CFF", .38, .35),
    centre=Region("#F4A93A", .35, .35),
)


def build_flowers(pal: Palette, variant: str = "A"):
    name = f"SM_Foliage_Plains_FlowerPatch_{variant}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="flowers")
    b.regions(**FLOWER_REGIONS)
    sph = {}
    tuft(b, Vector((-0.1, 0.06, 0)), 0.24, 5, rnd, sph=sph, segs=2)
    tuft(b, Vector((0.12, -0.04, 0)), 0.18, 4, rnd, sph=sph, segs=2)
    cols = ["yellow", "white", "pink", "yellow", "violet", "white"]
    if variant == "B":
        cols = ["pink", "white", "yellow", "violet", "pink", "yellow"]
    spots = [(-0.18, -0.08), (-0.04, -0.16), (0.1, 0.1), (0.2, -0.12), (-0.02, 0.16), (0.02, -0.02)]
    for q, ((x, y), col) in enumerate(zip(spots, cols)):
        x += rnd.uniform(-0.03, 0.03)
        y += rnd.uniform(-0.03, 0.03)
        hgt = rnd.uniform(0.13, 0.27)
        top = Vector((x, y, hgt))
        lean = Vector((rnd.uniform(-0.05, 0.05), rnd.uniform(-0.05, 0.05), 1)).normalized()
        b.add(E.blade(Vector((x, y, -0.01)), (top - Vector((x, y, 0))).normalized(), hgt, 0.012, bend=0.05,
                      segs=2, walls=False), "dark", name="stem", outline=False)
        r = rnd.uniform(0.06, 0.075)
        b.add(E.bloom(top, r, petals=5, normal=lean, cup=0.18, phase=rnd.uniform(0, 1), per_petal=2), col,
              name="bloom", outline=False)
        b.add(M.ellipsoid((r * 0.36, r * 0.36, r * 0.22), top + lean * 0.008, sides=5, rings=3), "centre",
              name="centre", outline=False)
    return E.finish(b, pal, "none", ground_h=0.35, spherise=sph,
                    wind=lambda co, pid: (E.smoothstep(0.0, 0.33, co.z), (co.x * 2.3 + co.y * 3.7) % 1.0),
                    name=name)


MUSH_REGIONS = dict(
    cap=Region("#D2402E", .42, .40),
    spot=Region("#FFF4E0", .25, .20),
    stem=Region("#F0E2C8", .35, .25),
    gill=Region("#C9A88A", .40, .20),
    dark=Region("#3F7A33", .40, .14),
    light=Region("#8CC24F", .38, .12),
)


def toadstool(b: M.Builder, c: Vector, h: float, cap_r: float, rnd: random.Random, tilt: Vector, k: int):
    up = Vector((0, 0, 1)).lerp(tilt, 0.25).normalized()
    stem = M.lathe([(0.34 * cap_r, 0.0), (0.36 * cap_r, 0.25 * h), (0.3 * cap_r, 0.75 * h), (0.27 * cap_r, h)],
                   sides=10)
    cap = M.lathe([(0.25 * cap_r, h - 0.05 * cap_r), (0.92 * cap_r, h - 0.06 * cap_r), (1.0 * cap_r, h + 0.06 * cap_r),
                   (0.86 * cap_r, h + 0.42 * cap_r), (0.5 * cap_r, h + 0.68 * cap_r), (0.02, h + 0.76 * cap_r)],
                  sides=16, cap0="fan", cap1="fan")
    rot = Vector((0, 0, 1)).rotation_difference(up).to_matrix().to_4x4()
    for p in (stem, cap):
        p.transform(rot)
        p.translate(c)
    b.add(stem, "stem", name=f"stem{k}")
    cid = b.add(cap, "cap", name=f"cap{k}", hull_pad=True)
    # spots: small domes lying on the cap
    for q in range(6):
        az = rnd.uniform(0, math.tau) + q * 1.05
        el = rnd.uniform(0.25, 0.95)
        rr = cap_r * (0.95 - 0.55 * el)
        p = Vector((math.cos(az) * rr, math.sin(az) * rr, h + 0.06 * cap_r + 0.7 * cap_r * el))
        n = Vector((math.cos(az) * (1 - el * 0.6), math.sin(az) * (1 - el * 0.6), 0.4 + el)).normalized()
        sp = M.ellipsoid((0.17 * cap_r, 0.17 * cap_r, 0.04 * cap_r), (0, 0, 0), sides=8, rings=3)
        sp.transform(Vector((0, 0, 1)).rotation_difference(n).to_matrix().to_4x4())
        sp.translate(p)
        sp.transform(rot)
        sp.translate(c)
        b.add(sp, "spot", name=f"spot{k}", outline=False, covered_by=cid)


def build_mushrooms(pal: Palette, variant: str = "A"):
    name = f"SM_Foliage_Plains_RedMushrooms_{variant}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="mushroom")
    b.regions(**MUSH_REGIONS)
    big = (0.2, 0.11) if variant == "A" else (0.17, 0.10)
    toadstool(b, Vector((-0.05, 0.02, 0)), big[0], big[1], rnd, Vector((-0.3, 0.1, 1)), 0)
    toadstool(b, Vector((0.12, -0.06, 0)), big[0] * 0.62, big[1] * 0.68, rnd, Vector((0.4, -0.2, 1)), 1)
    if variant == "B":
        toadstool(b, Vector((-0.12, -0.12, 0)), 0.07, 0.05, rnd, Vector((-0.3, -0.4, 1)), 2)
    sph = {}
    tuft(b, Vector((0.02, 0.1, 0)), 0.16, 5, rnd, segs=2, inked=True)
    return E.finish(b, pal, "decor", ground_h=0.3, spherise=sph,
                    wind=lambda co, pid: (0.15 * E.smoothstep(0.0, 0.3, co.z), 0.4), name=name)


LILY_REGIONS = dict(
    pad=Region("#6FAE4A", .40, .30),
    pad_vein=Region("#6FAE4A", .40, .30).shade_region(.3, .1),
    lily=Region("#F4D0E0", .30, .25),
    lily_core=Region("#F6D65A", .30, .30),
)


def lily_pad(b: M.Builder, c: Vector, r: float, notch: float, rnd: random.Random, k: int):
    n = 18
    pts = []
    for i in range(n + 1):
        a = notch + 0.28 + (math.tau - 0.56) * i / n
        rr = r * (1 + 0.04 * math.sin(a * 3 + k))
        pts.append((math.cos(a) * rr, math.sin(a) * rr))
    pts.append((0.0, 0.0))
    pad = M.plate(pts, 0.012, bevel=0.003)
    pad.translate(c + Vector((0, 0, 0.006)))
    pid = b.add(pad, "pad", name=f"pad{k}", hull_pad=True)
    for v in range(3):
        a = notch + math.pi + (v - 1) * 0.9
        p0 = c + Vector((0, 0, 0.0125))
        p1 = p0 + Vector((math.cos(a) * r * 0.75, math.sin(a) * r * 0.75, 0))
        b.add(E.ribbon([p0, (p0 + p1) / 2, p1], [Vector((0, 0, 1))] * 3, [0.012, 0.01, 0.004], lift=0.0005,
                       thick=0.001), "pad_vein", name="vein", outline=False, covered_by=pid)
    return pid


def build_lily(pal: Palette, variant: str = "A"):
    name = f"SM_Foliage_Plains_LilyPad_{variant}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="lily")
    b.regions(**LILY_REGIONS)
    lily_pad(b, Vector((0.0, 0.0, 0)), 0.16, rnd.uniform(0, math.tau), rnd, 0)
    lily_pad(b, Vector((0.24, -0.1, 0)), 0.11, rnd.uniform(0, math.tau), rnd, 1)
    if variant == "B":
        lily_pad(b, Vector((-0.2, -0.16, 0)), 0.12, rnd.uniform(0, math.tau), rnd, 2)
        # water lily on the big pad: two rings of pointed petals + a golden heart
        c = Vector((0.02, 0.01, 0.02))
        for ring, (rad, cup, ph) in enumerate(((0.075, 0.55, 0.0), (0.05, 0.9, 0.3))):
            b.add(E.bloom(c + Vector((0, 0, 0.01 * ring)), rad, petals=7, cup=cup, phase=ph, per_petal=2,
                          thick=0.008, walls=True), "lily", name=f"lily{ring}")
        b.add(M.ellipsoid((0.02, 0.02, 0.016), c + Vector((0, 0, 0.03)), sides=8, rings=4), "lily_core",
              name="lilycore", outline=False)
    return E.finish(b, pal, "decor", ground_h=None, name=name)


# ── registry + ship ─────────────────────────────────────────────────────────────────────────────────────
# name → (builder, ship kwargs)
TREE = dict(game_ids=["tree"], footprint=[1, 1], blocking=True, foliage=True, blob=0.5)
ASSETS: dict = {}
for _v in "ABC":
    ASSETS[f"SM_Foliage_Plains_Oak_{_v}"] = (lambda pal, v=_v: build_oak(pal, v), dict(TREE))
for _v in "AB":
    ASSETS[f"SM_Foliage_Plains_FruitTree_{_v}"] = (
        lambda pal, v=_v: build_fruit_tree(pal, v),
        dict(game_ids=["tree_round"], footprint=[1, 1], blocking=True, foliage=True, blob=0.42))
    ASSETS[f"SM_Foliage_Plains_BerryBush_{_v}"] = (
        lambda pal, v=_v: build_bush(pal, v),
        dict(game_ids=["bush"], footprint=[1, 1], blocking=False, foliage=True, blob=0.5))
    ASSETS[f"SM_Foliage_Plains_FlowerPatch_{_v}"] = (
        lambda pal, v=_v: build_flowers(pal, v),
        dict(game_ids=["flower"], footprint=[1, 1], blocking=False, foliage=True, toon_slot=GROUND_SLOT,
             toon_params=GROUND_WIND, blob=0.2, turnaround=False))
    ASSETS[f"SM_Foliage_Plains_RedMushrooms_{_v}"] = (
        lambda pal, v=_v: build_mushrooms(pal, v),
        dict(game_ids=["mushroom_red"], footprint=[1, 1], blocking=False, foliage=True, blob=0.2))
    ASSETS[f"SM_Foliage_Plains_LilyPad_{_v}"] = (
        lambda pal, v=_v: build_lily(pal, v),
        dict(game_ids=["lily_pad_plains"], footprint=[1, 1], blocking=False, foliage=True, blob=0.01,
             turnaround=False))
for _v in "ABC":
    ASSETS[f"SM_Foliage_Plains_GrassTuft_{_v}"] = (
        lambda pal, v=_v: build_grass(pal, v),
        dict(game_ids=["grass"], footprint=[1, 1], blocking=False, foliage=True, toon_slot=GROUND_SLOT,
             toon_params=GROUND_WIND, blob=0.2, turnaround=False))

NOTES = {
    "tree": "W5: tall decor, blocks its tile (trunk + root flare); canopy overhangs the neighbours",
    "flower": "ground cover: no outline hull (spec 1.3), MI_AF_Toon_Foliage_Plains_Ground (gentle low sway)",
}


def main(argv: list[str]) -> int:
    quick = "--quick" in argv
    sheet = "--sheet" in argv
    previews = "--no-previews" not in argv
    names = [a for a in argv if not a.startswith("--")] or list(ASSETS)
    names = [n for n in ASSETS if any(n == a or a in n for a in names)]
    scene.reset()
    pal = Palette(E.FOLIAGE)
    if sheet:
        objs = [ASSETS[n][0](pal) for n in names]
        pal.save()
        E.sheet(objs, "sheet_" + "_".join(n.split("_")[-1][:6] for n in names)[:80], cols=3)
        return 0
    if quick:
        objs = [ASSETS[n][0](pal) for n in names]
        pal.save()
        E.quick(objs, "foliage_" + "_".join(n.split("_")[-2][:5] + n.split("_")[-1] for n in names)[:80],
                spacing=1.4 if all("Oak" not in n and "Fruit" not in n for n in names) else 3.0)
        return 0
    shipped = []
    for n in names:
        scene.reset()
        pal = Palette(E.FOLIAGE)
        fn, kw = ASSETS[n]
        obj = fn(pal)
        pal.save()
        kw = dict(kw)
        extra = {"displayName": n.replace("SM_Foliage_Plains_", ""), "family": "environment.foliage"}
        if kw.get("blocking"):
            extra["blockingNote"] = NOTES["tree"]
        if kw.get("toon_slot"):
            extra["groundCover"] = NOTES["flower"]
        E.ship(obj, n, CAT, pal, obj["af_outline_class"], extra=extra, previews=previews, **kw)
        shipped.append(n)
    # family sheet (rebuilt in one scene)
    if previews and len(shipped) > 1:
        scene.reset()
        pal = Palette(E.FOLIAGE)
        objs = [ASSETS[n][0](pal) for n in shipped]
        trees = [o for o in objs if "Oak" in o.name or "Fruit" in o.name]
        small = [o for o in objs if o not in trees]
        if trees:
            E.family_sheet(trees, "foliage_trees.png", "PLAINS TREES", cols=3, half_w=0.085, focus_z=1.7)
            for o in trees:
                o.hide_render = True
        if small:
            E.family_sheet(small, "foliage_small.png", "PLAINS SHRUBS GROUND COVER", cols=5, half_w=0.04,
                           focus_z=0.35)
    return E.summary()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

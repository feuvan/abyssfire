"""March foliage: the plains decor slots, dead (dark remake A10; chapter1.md §15, story-bible tone pillar
"aftermath, not gore"; palette ``Foliage_March`` from A1, ``march/march_palettes.py``).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/foliage_march.py [names…]
        [--quick] [--sheet] [--family] [--no-previews]

| Asset | game id (themed) | look |
|---|---|---|
| ``SM_Foliage_March_DeadOak_A/B/C`` (+ ``_LOD1``) | ``tree@march`` (tall, blocks 1×1) | a dead oak ≈ 4.2-4.3 m: flared trunk in the dimmed bark ``#463C34`` (lit L* 37) with a fire scar split down one side (charred heart ``#2A2220``, split lines ``#1C1614``), three / four gnarled limbs rising steeply and forking into bare crooked branches and twigs — a clawed crown ≤ 3 m across (2.1-2.9 m; env art review: 4.2 m crowns hid the neighbours' monsters); dull ash ``#66605A`` lying on the top of every limb, a few clinging dead leaves ``#5E4A36`` |
| ``SM_Foliage_March_CharredOrchard_A/B`` (+ ``_LOD1``) | ``tree_round@march`` (tall, blocks) | ≈ 3.1 m pruned orchard tree burnt black ``#3A322C``: a short leaning trunk opening into a vase of upward limbs, ash ``#6E6862`` on the limbs, five or six black apples ``#1E1418`` still hanging (glossy, a lit glint) |
| ``SM_Foliage_March_Briar_A/B`` | ``bush@march`` | a low bramble thicket ≈ 1.0-1.2 × 1.0 m, 0.5-0.58 m tall: 14 / 16 thorny canes arching in hoops over a dark core ``#2E2824`` (every hoop on the thicket's dome, its plane at a random tilt, so they cross instead of radiating), three heights, the dimmed bark tone ``#463C34`` (every fourth the weathered ``#5A4C40``, every fifth the old dark ``#3A3228``), thorns ``#6A5E4C``, dead rust leaves ``#4A3529``, one cane escaping low at a side; rags snagged where the top hoops run down the side, ≈ 20 × 28 cm (A: undyed ``#6A6050`` + a tabard scrap ``#5E1C1C``; B: one large tabard scrap); ≤ 1.8 k toon tris |
| ``SM_Foliage_March_AshGrass_A/B/C`` | ``grass@march`` (ground cover, no ink) | fans of dead blades ``#4A473F`` / ``#8A867A``, broken and bent, pale seed heads ``#A8A294``; no vertex under the ground (``clamp_ground``) |
| ``SM_Foliage_March_AshPoppy_A/B`` | ``flower@march`` (ground cover, no ink) | dead tufts and four-six dull blood-red poppies ``#6E2420`` (light .12: lit ``#7F3D36``, not the dusty pink of ``poppy.petal``; black centres; the foliage's only colour), two gone to grey ash ``#8E8A82`` and drooping |
| ``SM_Foliage_March_AshCap_A/B`` | ``mushroom_red@march`` | a cluster of pale ash-cap fungus: slender stems ``#9A948A`` under bell caps ``#B8B2A6`` with dark gills ``#6E665E`` (inky caps), dead grass |
| ``SM_Foliage_March_Reeds_A/B`` | ``lily_pad_plains@march`` (water) | dead reeds ``#4A4838`` with dark heads ``#2E2A24`` (ground cover, no ink) standing straight out of the water, a few snapped and folded over (env art review: the inked slick plates under them read as dark slabs on the pond — dropped; the terrain's water carries the oily sheen) |

Shapes come from ``foliage.py`` / ``envkit.py`` (trunk flare, tufts, toadstool lathe, canopy lumps, wind data); the
dead trees are new: crooked tube branches grown recursively (:func:`grow`) with the ash as an ``af_sub`` region on
the faces turned up (:func:`tag_top`). Ink: decor 1.2 px (``Foliage_March`` keeps the class width); ground cover
none. Wind: ``M_AF_Toon_Foliage`` with the baked weights — the dead trees sway stiffly (weights × 0.6), ground
cover on ``MI_AF_Toon_Foliage_March_Ground``.
"""
from __future__ import annotations

import math
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import envkit_march as EM  # noqa: E402  (first: sys.path, march stage, PX overrides)
import envkit as E  # noqa: E402
import foliage as F  # noqa: E402
import bmesh  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import scene  # noqa: E402
from kit import mesh as M  # noqa: E402
from kit.palette import Palette  # noqa: E402

CAT = "Foliage"
FAM = EM.FOLIAGE


def R(prefix: str, **extra):
    return EM.regions(FAM, prefix, **extra)


# Regions added on top of A1's colour script (appended to the Foliage_March registry; same rules: dark, low chroma)
from kit.palette import Region  # noqa: E402
EXTRA_REGIONS = {
    # the briar's dead mass: darker than the canes' bark and with a short light band — the cloud lumps read as a
    # pale dough pile with briar.leaf / briar.stem (L* 45 lit tops on the L* 31 ground)
    "briar": {"mass": Region("#36302A", .48, .18),
              # art review 2: the grey mound still read as a rock pile → a low mound of rust dead leaves
              "mass_rust": Region("#4A3529", .48, .18), "leaf_rust": Region("#5E4434", .46, .2),
              # art review 3: the canes carry the bramble's silhouette — weathered grey-brown like the dead oak's
              # bark (briar.stem #3A3228 merged with its ink into 2.5 px dark lines: ink gate upper bound)
              "cane": Region("#5A4C40", .48, .2),
              # env art review (march): the lit canes made a pale starburst at 1080p — the tangle's canes are the
              # dead oak's dimmed bark value (lit L* ≈ 37), the weathered briar.cane only on every fourth
              "cane_dark": Region("#463C34", .48, .14),
              # … and the shadow inside the tangle: a dark core under the hoops (lit L* ≈ 28)
              "core": Region("#2E2824", .48, .12),
              # the crusade-tabard scrap: briar.rag_crimson's light band (.24) lit the cloth a dusty pink; a dull
              # dark crimson with a short light band (lit L* ≈ 32, no pink)
              "rag_tabard": Region("#5E1C1C", .45, .14)},
    # art review 3: scum.oil's glossy light band lit the flat scum plate a pale L* 55 slab (a stone on the black
    # water); the slick stays dark and the scum.sheen streak on it is the only light
    "reed": {"slick": Region("#24262A", .35, .1)},
}


# ── shared pieces ───────────────────────────────────────────────────────────────────────────────────────
def tag_top(part: M.Part, thresh: float = 0.45, sub: int = 1) -> M.Part:
    """``af_sub = sub`` on the faces turned up (normal z > ``thresh``): ash lying on the top of a limb."""
    bm = part.bm
    bm.normal_update()
    lay = bm.faces.layers.int.get("af_sub") or bm.faces.layers.int.new("af_sub")
    for f in bm.faces:
        if f.normal.z > thresh:
            f[lay] = sub
    return part


def gnarl_path(start: Vector, d: Vector, length: float, segs: int, rnd: random.Random, wobble: float,
               rise: float = 0.0):
    """A crooked branch centre line: ``segs`` straight runs, each turned a little (``wobble``) and lifted
    (``rise``: + bends up toward the light, − droops)."""
    pts = [start.copy()]
    dd = d.normalized()
    for _ in range(segs):
        dd = (dd + Vector((rnd.uniform(-wobble, wobble), rnd.uniform(-wobble, wobble),
                           rnd.uniform(-wobble * 0.4, wobble * 0.4) + rise))).normalized()
        pts.append(pts[-1] + dd * (length / segs))
    return pts, dd


def turn(d: Vector, az: float, spread_deg: float) -> Vector:
    """``d`` tilted by ``spread_deg`` toward azimuth ``az`` (around an axis perpendicular to ``d``)."""
    d = d.normalized()
    ref = Vector((0, 0, 1)) if abs(d.z) < 0.95 else Vector((1, 0, 0))
    x = d.cross(ref).normalized()
    y = d.cross(x).normalized()
    axis = (x * math.cos(az) + y * math.sin(az)).normalized()
    return (Matrix.Rotation(math.radians(spread_deg), 3, axis) @ d).normalized()


def branch_sides(r: float) -> int:
    return 8 if r > 0.07 else 6 if r > 0.032 else 5 if r > 0.018 else 4


class Grower:
    """Recursive bare branches (tube per branch, children forking from its tip, side twigs along it).
    ``plan[depth] = (children, length factor, spread °, radius end factor, wobble, rise)``."""

    def __init__(self, b: M.Builder, region: str, rnd: random.Random, plan: list, ash: str | None = "ash",
                 ash_depth: int = 1, side_twigs: tuple = (1, 2), twig_len: float = 0.32, droop: float = 0.0,
                 under: str | None = None, lod: int = 0):
        """``under``: region of the faces turned down (``af_sub`` 2; the charred undersides of burnt limbs). ``lod``:
        LOD1 tubes take two sides and a segment less (the parts are mostly under the decimator's 48-tri floor)."""
        self.b, self.region, self.rnd, self.plan = b, region, rnd, plan
        self.under = under
        self.lod = lod
        self.ash, self.ash_depth = ash, ash_depth
        self.side_twigs, self.twig_len, self.droop = side_twigs, twig_len, droop
        self.parts: list[tuple[int, int, int]] = []          # (pid, depth, limb index)
        self.tips: list[tuple[Vector, Vector, int]] = []     # (tip, direction, depth)

    def grow(self, start: Vector, d: Vector, length: float, r0: float, depth: int, limb: int) -> None:
        kids, _, _, rk, wob, rise = self.plan[depth]
        rnd = self.rnd
        segs = 4 if depth == 0 else 3 if depth < 3 else 2
        pts, dend = gnarl_path(start, d, length, segs, rnd, wob, rise - self.droop * depth)
        if self.lod and segs > 2:                       # same random draws, fewer rings
            pts = pts[::2] if segs == 4 else [pts[0], pts[2], pts[3]]
            segs = len(pts) - 1
        r1 = max(r0 * rk, 0.009)
        radii = [r0 + (r1 - r0) * (i / segs) ** 0.8 for i in range(segs + 1)]
        sides = branch_sides(r0) if not self.lod else max(4, branch_sides(r0) - 2)
        part = M.tube(pts, radii, sides=sides, dome0=0.0, dome1=r1 * 0.9)
        part.recalc_normals()
        subs = None
        if self.ash and depth <= self.ash_depth:
            tag_top(part, 0.62)
            subs = {1: self.ash}
        if self.under and depth <= 2:
            bm = part.bm
            lay = bm.faces.layers.int.get("af_sub") or bm.faces.layers.int.new("af_sub")
            for f in bm.faces:
                if f.normal.z < -0.2:
                    f[lay] = 2
            subs = {**(subs or {}), 2: self.under}
        pid = self.b.add(part, self.region, name=f"br{depth}", sub_regions=subs)
        self.parts.append((pid, depth, limb))
        self.tips.append((pts[-1], dend, depth))
        if depth + 1 >= len(self.plan):
            return
        nk = kids if isinstance(kids, int) else rnd.choice(kids)
        lf, spread = self.plan[depth + 1][1], self.plan[depth + 1][2]
        a0 = rnd.uniform(0, math.tau)
        for k in range(nk):
            az = a0 + math.tau * k / nk + rnd.uniform(-0.4, 0.4)
            cd = turn(dend, az, spread * rnd.uniform(0.75, 1.2))
            self.grow(pts[-1] - dend * r1 * 0.6, cd, length * lf * rnd.uniform(0.85, 1.12), r1 * 1.05, depth + 1,
                      limb)
        # short side twigs along the branch (the dead crown's fine texture)
        if depth in self.side_twigs:
            for q in range(1 if depth == self.side_twigs[0] else 2):
                t = rnd.uniform(0.35, 0.7)
                i = min(int(t * segs), segs - 1)
                base = pts[i].lerp(pts[i + 1], t * segs - i)
                rr = radii[i] * 0.8
                cd = turn((pts[i + 1] - pts[i]).normalized(), rnd.uniform(0, math.tau), rnd.uniform(40, 65))
                tp, _ = gnarl_path(base, cd, self.twig_len * rnd.uniform(0.7, 1.1), 2, rnd, 0.35, 0.05)
                tw = M.tube(tp, [min(rr, 0.022), 0.012, 0.007], sides=4, dome0=0.0, dome1=0.006)
                tw.recalc_normals()
                pid = self.b.add(tw, self.region, name="twig")
                self.parts.append((pid, depth + 2, limb))


def flared_trunk(b: M.Builder, region: str, h: float, r_base: float, r_top: float, flare: float, lean: Vector,
                 rnd: random.Random, lobes: int = 4, sides: int = 18, scar: str | None = "char",
                 scar_az: float | None = None, scar_w: float = 1.1, scar_z: tuple = (0.12, 0.72),
                 blotch: float = 0.0, blotch_region: str | None = None, origin: Vector | None = None):
    """The plains trunk (``foliage.trunk_radius`` root flare) as a closed ring loft, with a **fire scar**: the faces in
    an azimuth window ``scar_w`` rad wide round ``scar_az``, ``scar_z`` × ``h`` high, get ``af_sub`` 2 (charred heart).
    Returns (part id, scar azimuth, phase)."""
    ph = rnd.uniform(0, math.tau)
    zs = [-0.04, 0.0, 0.035, 0.08, 0.14, 0.24, 0.45, 0.8, 1.2, h * 0.85, h]
    zs = sorted(set(z for z in zs if z <= h))
    rings = []
    for z in zs:
        off = lean * (max(z, 0) / h) ** 1.6
        rings.append([Vector((math.cos(a) * rr, math.sin(a) * rr, z)) + off for a, rr in
                      ((math.tau * i / sides, F.trunk_radius(max(z, 0), r_base, r_top, h, flare, lobes, ph,
                                                             math.tau * i / sides)) for i in range(sides))])
    part = E.ring_loft(rings, cap0="ngon", cap1="fan")
    sa = rnd.uniform(0, math.tau) if scar_az is None else scar_az
    subs = None
    if scar:
        bm = part.bm
        bm.normal_update()
        lay = bm.faces.layers.int.get("af_sub") or bm.faces.layers.int.new("af_sub")
        for f in bm.faces:
            c = f.calc_center_median()
            az = math.atan2(c.y - lean.y * (max(c.z, 0) / h) ** 1.6, c.x - lean.x * (max(c.z, 0) / h) ** 1.6)
            da = abs(math.atan2(math.sin(az - sa), math.cos(az - sa)))
            zt = c.z / h
            # the scar narrows to a point at both ends (a lens), like bark split open by heat
            half = scar_w / 2 * math.sin(math.pi * min(max((zt - scar_z[0]) / (scar_z[1] - scar_z[0]), 0.0), 1.0))
            if da < half and abs(f.normal.z) < 0.8:
                f[lay] = 2
        subs = {2: scar}
    if blotch > 0.0 and blotch_region:
        # burnt bark: alligator patches of char over the whole trunk (a fixed low-frequency field, so the patches
        # are blocks of faces, not salt)
        bm = part.bm
        lay = bm.faces.layers.int.get("af_sub") or bm.faces.layers.int.new("af_sub")
        k1, k2 = rnd.uniform(0, 6.3), rnd.uniform(0, 6.3)
        for f in bm.faces:
            c = f.calc_center_median()
            a = math.atan2(c.y, c.x)
            val = math.sin(3 * a + k1 + 4.0 * c.z) * math.sin(2 * a - k2 + 7.0 * c.z)
            if val > 1.0 - 2.0 * blotch and abs(f.normal.z) < 0.85 and f[lay] == 0:
                f[lay] = 3
        subs = {**(subs or {}), 3: blotch_region}
    if origin is not None:
        part.translate(origin)
    tid = b.add(part, region, name="trunk", hull_pad=True, sub_regions=subs)
    return tid, sa, ph


def trunk_point(az: float, z: float, h: float, r_base: float, r_top: float, flare: float, lobes: int, ph: float,
                lean: Vector, out: float = 0.002) -> tuple[Vector, Vector]:
    rr = F.trunk_radius(z, r_base, r_top, h, flare, lobes, ph, az) + out
    n = Vector((math.cos(az), math.sin(az), 0.0))
    return Vector((math.cos(az) * rr, math.sin(az) * rr, z)) + lean * (max(z, 0) / h) ** 1.6, n


def bark_lines(b: M.Builder, region: str, tid: int, h: float, geo: tuple, rnd: random.Random, n: int = 3,
               az0: float = 0.0, z_range=(0.2, 0.8), width: float = 0.018):
    """Dark split lines running down the bark (the web trunk strokes; here the cracks of the burnt bark)."""
    r_base, r_top, flare, lobes, ph, lean = geo
    for g in range(n):
        a0 = az0 + math.tau * (g + rnd.uniform(0.2, 0.8)) / n
        z0, z1 = h * z_range[0] * rnd.uniform(0.8, 1.2), h * z_range[1] * rnd.uniform(0.85, 1.0)
        pts, nrm = [], []
        for i in range(6):
            t = i / 5
            z = z0 + (z1 - z0) * t
            a = a0 + 0.18 * math.sin(t * 3.1 + g)
            p, nn = trunk_point(a, z, h, r_base, r_top, flare, lobes, ph, lean)
            pts.append(p)
            nrm.append(nn)
        w = [width * (0.5 + 0.5 * math.sin(math.pi * i / 5)) + 0.004 for i in range(6)]
        b.add(E.ribbon(pts, nrm, w, lift=0.001, thick=0.003), region, name="groove", outline=False, covered_by=tid)


def scar_split(b: M.Builder, region: str, tid: int, h: float, geo: tuple, sa: float, scar_z: tuple):
    """The split down the middle of the fire scar."""
    r_base, r_top, flare, lobes, ph, lean = geo
    pts, nrm = [], []
    for i in range(7):
        t = i / 6
        z = h * (scar_z[0] + (scar_z[1] - scar_z[0]) * (0.08 + 0.84 * t))
        p, nn = trunk_point(sa + 0.06 * math.sin(t * 5.0), z, h, r_base, r_top, flare, lobes, ph, lean)
        pts.append(p)
        nrm.append(nn)
    w = [0.006 + 0.02 * math.sin(math.pi * i / 6) for i in range(7)]
    b.add(E.ribbon(pts, nrm, w, lift=0.001, thick=0.003), region, name="groove", outline=False, covered_by=tid)


def stiff_wind(parts, trunk_h: float, top: float, scale: float = 0.6):
    """Dead wood sways stiffly: weight by height over the trunk and branch depth, phase per main limb."""
    depth = {pid: d for pid, d, _ in parts}
    limb = {pid: l for pid, _, l in parts}

    def fn(co: Vector, pid: int):
        if pid in depth:
            w = (0.25 + 0.75 * E.smoothstep(trunk_h, top, co.z)) * min(1.0, 0.35 + 0.25 * depth[pid])
            return w * scale, (limb[pid] * 0.618) % 1.0
        return 0.15 * scale * E.smoothstep(trunk_h * 0.45, trunk_h * 1.2, co.z), 0.0
    return fn


# ── dead oak ────────────────────────────────────────────────────────────────────────────────────────────
OAK_VARIANTS = {
    # trunk height, limb length, limb tilt from vertical (deg), lean, crown spread (azimuth jitter), seed tweak
    # env art review (march): the crowns spread 4.2 m on a 4.6 m tree (the plains oak ≈ 2.5 m) and hid monsters on
    # the neighbouring tiles — limbs rise steeper and shorter, the forks spread less: crowns ≤ ≈ 3 m
    "A": dict(trunk_h=1.75, limb=1.0, tilt=22.0, lean=(0.06, 0.0, 0.0), limbs=3),
    "B": dict(trunk_h=1.6, limb=1.06, tilt=25.0, lean=(-0.12, 0.05, 0.0), limbs=3),
    "C": dict(trunk_h=1.95, limb=1.04, tilt=24.0, lean=(0.03, -0.09, 0.0), limbs=4),
}
# (children, length factor, spread °, radius end factor, wobble, rise) per depth: limb → branch → sub-branch → twig
OAK_PLAN = [((2, 3), 1.0, 0.0, 0.62, 0.22, 0.08),
            (2, 0.66, 24.0, 0.55, 0.28, 0.06),
            ((1, 2, 2), 0.62, 28.0, 0.45, 0.32, 0.04),
            (0, 0.5, 30.0, 0.4, 0.38, 0.02)]
# … and the bark lit 8-10 L* lower (deadoak.bark lit L* 46 and its ash L* 64 read brighter than the mud): appended
# swatches, aliased onto the "bark" / "ash" keys so A1's deadoak.* names stay where they are
OAK_BARK_DIM = Region("#463C34", .48, .14)          # lit L* 37
OAK_ASH_DIM = Region("#66605A", .45, .2)            # lit L* ≈ 53


def dim_bark(b: M.Builder) -> None:
    """Point the builder's ``bark`` / ``ash`` keys at the dimmed swatches (``<prefix>.bark_dim`` / ``.ash_dim``)."""
    b.swatch["bark"] = b.region("bark_dim", OAK_BARK_DIM)
    if "ash" in b.swatch:
        b.swatch["ash"] = b.region("ash_dim", OAK_ASH_DIM)
TREE_LOD1 = {"oak": 0.5, "orchard": 0.55}


def tree_lod_skip(name: str) -> bool:
    """LOD1 (mobile low / far): the finest branches, twigs, bark lines, leaves and stalks go (each a few px wide at the
    LOD distances; the decimation keeps parts under 48 tris whole, so they must be dropped to reach 1.5 k)."""
    return name in ("twig", "groove", "leaf", "glint", "br3", "stalk")


def dead_leaves(b: M.Builder, tips, rnd: random.Random, n: int, region: str = "leaf"):
    """A few dead leaves still clinging: small curled ovals hanging at branch tips."""
    picks = [t for t in tips if t[2] >= 2]
    rnd.shuffle(picks)
    for tip, d, _ in picks[:n]:
        for q in range(2):
            lf = M.ellipsoid((0.045, 0.028, 0.008), (0, 0, 0), sides=6, rings=3)
            lf.transform(Matrix.Rotation(rnd.uniform(0, math.tau), 4, "Z") @ Matrix.Rotation(math.radians(
                rnd.uniform(50, 80)), 4, "X"))
            lf.translate(tip + Vector((rnd.uniform(-0.04, 0.04), rnd.uniform(-0.04, 0.04), -0.05 - 0.04 * q)))
            b.add(lf, region, name="leaf", outline=False)


def build_dead_oak(pal: Palette, variant: str = "A", lod: int = 0):
    name = f"SM_Foliage_March_DeadOak_{variant}"
    v = OAK_VARIANTS[variant]
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="deadoak", lod_ratio=TREE_LOD1["oak"] if lod else 1.0,
                  lod_skip=tree_lod_skip if lod else None)
    b.regions(**R("deadoak"))
    dim_bark(b)
    th = v["trunk_h"]
    lean = Vector(v["lean"])
    h = th + 0.25
    geo = (0.25, 0.14, 0.9, 4)
    scar_z = (0.08, 0.78)
    # the scar faces local -Y (the "front" every review shot turns to the camera; the runtime yaw is random)
    tid, sa, ph = flared_trunk(b, "bark", h, geo[0], geo[1], geo[2], lean, rnd, lobes=geo[3], scar="char",
                               scar_az=math.radians(-90) + rnd.uniform(-0.35, 0.35), scar_z=scar_z)
    g6 = (geo[0], geo[1], geo[2], geo[3], ph, lean)
    bark_lines(b, "split", tid, h, g6, rnd, n=5, az0=sa + 0.7)
    scar_split(b, "split", tid, h, g6, sa, scar_z)
    gr = Grower(b, "bark", rnd, OAK_PLAN, ash="ash", ash_depth=1, side_twigs=(1, 2), twig_len=0.3, lod=lod)
    top = lean * 1.0 + Vector((0, 0, h - 0.12))
    n = v["limbs"]
    a0 = rnd.uniform(0, math.tau)
    for i in range(n):
        az = a0 + math.tau * i / n + rnd.uniform(-0.3, 0.3)
        tilt = math.radians(v["tilt"] + rnd.uniform(-6, 8))
        d = Vector((math.sin(tilt) * math.cos(az), math.sin(tilt) * math.sin(az), math.cos(tilt)))
        st = top + Vector((math.cos(az), math.sin(az), 0)) * 0.05 - Vector((0, 0, 0.12 * (i % 2)))
        gr.grow(st, d, v["limb"] * rnd.uniform(0.9, 1.1), 0.115 if i < 2 else 0.095, 0, i)
    # a broken-off stub low on the trunk (a limb the fire took)
    sa2 = sa + math.pi * 0.75
    p0, _ = trunk_point(sa2, th * 0.6, h, *g6[:5], lean, out=-0.04)
    dd = Vector((math.cos(sa2) * 0.8, math.sin(sa2) * 0.8, 0.6)).normalized()
    stub = M.tube([p0, p0 + dd * 0.12, p0 + dd * 0.22], [0.08, 0.065, 0.045], sides=7, dome0=0.0, dome1=0.01)
    stub.recalc_normals()
    b.add(stub, "bark", name="br0")
    gr.parts.append((len(b.parts) - 1, 0, 0))
    if not lod:
        dead_leaves(b, gr.tips, rnd, 3 if variant != "C" else 4)
    hi = max(t[0].z for t in gr.tips)
    return E.finish(b, pal, "decor", ground_h=1.6, wind=stiff_wind(gr.parts, th, hi),
                    name=name if not lod else f"{name}_LOD1")


# ── charred orchard tree ────────────────────────────────────────────────────────────────────────────────
# (children, length factor, spread °, radius end factor, wobble, rise): a pruned vase of upward limbs, short forks and
# water-sprouts shooting straight up (the burnt orchard's tell)
ORCH_PLAN = [((2, 3), 1.0, 0.0, 0.6, 0.16, 0.14),
             (2, 0.62, 26.0, 0.5, 0.26, 0.10),
             (0, 0.5, 30.0, 0.42, 0.3, 0.16)]
ORCH_VARIANTS = {"A": dict(trunk_h=1.05, lean=(-0.16, 0.06, 0.0), limbs=4, limb=1.0, k=1.0),
                 "B": dict(trunk_h=0.95, lean=(0.10, 0.12, 0.0), limbs=5, limb=0.92, k=1.06)}


def apple(b: M.Builder, tip: Vector, r: float, rnd: random.Random, k: int):
    """A black apple hanging from a twig tip on a short stalk (glossy: its light band is the glint)."""
    c = tip + Vector((rnd.uniform(-0.03, 0.03), rnd.uniform(-0.03, 0.03), -0.075 - r))
    stalk = M.tube([tip, tip.lerp(c, 0.5) + Vector((0.01, 0, 0)), c + Vector((0, 0, r * 0.8))], [0.006, 0.005, 0.004],
                   sides=4, dome0=0.0, dome1=0.0)
    b.add(stalk, "char", name="stalk", outline=False)
    fr = M.ellipsoid((r, r, r * 0.92), c, sides=8, rings=5)
    return b.add(fr, "apple", name=f"apple{k}")


def build_orchard(pal: Palette, variant: str = "A", lod: int = 0):
    name = f"SM_Foliage_March_CharredOrchard_{variant}"
    v = ORCH_VARIANTS[variant]
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="charred", lod_ratio=TREE_LOD1["orchard"] if lod else 1.0,
                  lod_skip=tree_lod_skip if lod else None)
    b.regions(**R("charred"))
    th = v["trunk_h"]
    lean = Vector(v["lean"])
    h = th + 0.2
    geo = (0.17, 0.12, 0.75, 3)
    tid, sa, ph = flared_trunk(b, "bark", h, geo[0], geo[1], geo[2], lean, rnd, lobes=geo[3], scar=None,
                               blotch=0.42, blotch_region="char")
    g6 = (geo[0], geo[1], geo[2], geo[3], ph, lean)
    bark_lines(b, "char", tid, h, g6, rnd, n=4, az0=rnd.uniform(0, math.tau), z_range=(0.1, 0.9), width=0.016)
    gr = Grower(b, "bark", rnd, ORCH_PLAN, ash="ash", ash_depth=1, side_twigs=(1,), twig_len=0.26, under="char",
                lod=lod)
    top = lean + Vector((0, 0, h - 0.08))
    n = v["limbs"]
    a0 = rnd.uniform(0, math.tau)
    for i in range(n):
        az = a0 + math.tau * i / n + rnd.uniform(-0.25, 0.25)
        tilt = math.radians(rnd.uniform(28, 42))
        d = Vector((math.sin(tilt) * math.cos(az), math.sin(tilt) * math.sin(az), math.cos(tilt)))
        st = top + Vector((math.cos(az), math.sin(az), 0)) * 0.04 - Vector((0, 0, 0.1 * (i % 2)))
        gr.grow(st, d, v["limb"] * v["k"] * rnd.uniform(0.9, 1.1), 0.085, 0, i)
    # five or six black apples still hanging, on tips that face the outside of the crown
    tips = [t for t in gr.tips if t[2] >= 1]
    rnd.shuffle(tips)
    c0 = sum((t[0] for t in gr.tips), Vector()) / len(gr.tips)
    tips.sort(key=lambda t: -((t[0] - c0).xy.length - 0.2 * t[0].z))
    for k, (tip, d, _) in enumerate(tips[:6 if variant == "A" else 5]):
        apple(b, tip - d * 0.06, 0.05, rnd, k)
    hi = max(t[0].z for t in gr.tips)
    return E.finish(b, pal, "decor", ground_h=1.4, wind=stiff_wind(gr.parts, th, hi),
                    name=name if not lod else f"{name}_LOD1")


# ── briar ───────────────────────────────────────────────────────────────────────────────────────────────
# env art review (march): the 20-cane fountain from one root crown read at 1080p as a pale radial starburst (a dead
# spider, a dropped twig bundle), its rags invisible, and it cost 2.7-2.9 k toon tris on ~200 bush tiles. The
# bramble is now a low thicket: thorny canes arching in hoops over a dark core (each hoop on the thicket's dome,
# planes at every angle, so they cross instead of radiating from a centre), three heights, the dark bark tones, a
# few an escaped cane breaking the outline at one side, and one or two rags draped over the top hoops large enough to
# read (≈ 20 × 28 cm ≈ 10 × 14 px at the W1 camera). ≤ 2 k toon tris.
BRIAR_SIZE = {"A": dict(w=1.18, h=0.52, hoops=14, escaped=1, seed_az=0.0,
                        rags=(("rag", 0.22, 0.28), ("rag_tabard", 0.15, 0.2))),
              "B": dict(w=1.28, h=0.46, hoops=16, escaped=1, seed_az=1.3, rags=(("rag_tabard", 0.2, 0.27),))}
BRIAR_SCALES = (0.98, 0.9, 0.8, 0.7, 0.62)      # hoop dome scales: the three heights (top, mid, low) cycle through


def thorns_along(b: M.Builder, pts: list, side: Vector, n: int, rnd: random.Random, k: int) -> None:
    """``n`` thorns on a cane's outer side (no hull: 1-2 px spikes)."""
    m = len(pts) - 1
    for q in range(n):
        t = 0.2 + 0.65 * (q + rnd.uniform(0.2, 0.8)) / max(n, 1)
        i = min(int(t * m), m - 1)
        pp = pts[i].lerp(pts[i + 1], t * m - i)
        tang = (pts[i + 1] - pts[i]).normalized()
        nrm = tang.cross(side).normalized()
        if nrm.z < 0:
            nrm = -nrm
        dirn = (nrm * 0.7 + side * (1 if (q + k) % 2 else -1) * 0.6 - tang * 0.35).normalized()
        th = M.tube([pp, pp + dirn * 0.04], [0.008, 0.0015], sides=3, dome0=0.0, dome1=0.0)
        th.recalc_normals()
        b.add(th, "thorn", name="thorn", outline=False)


def rust_leaves(b: M.Builder, pts: list, n: int, rnd: random.Random) -> None:
    for q in range(n):
        pp = pts[rnd.randint(2, len(pts) - 3)]
        L, W = rnd.uniform(0.06, 0.08), rnd.uniform(0.032, 0.042)
        lf = M.plate([(0.0, 0.0), (L * 0.45, -W * 0.5), (L, 0.0), (L * 0.45, W * 0.5)], 0.005)
        lf.transform(Matrix.Rotation(rnd.uniform(0, math.tau), 4, "Z")
                     @ Matrix.Rotation(rnd.uniform(-0.6, 0.6), 4, "X") @ Matrix.Rotation(rnd.uniform(0.3, 0.8), 4, "Y"))
        lf.translate(pp)
        b.add(lf, "mass_rust", name="leaf", outline=False)


def hoop_cane(b: M.Builder, radii: Vector, s: float, rnd: random.Random, k: int, region: str, r0: float,
              thorns: int = 2, leaves: int = 1) -> tuple[int, list]:
    """A cane arching over the thicket's dome (half-axes ``radii`` × ``s``): an arc of a great circle whose plane
    is tilted at random (it runs over the top at any angle, crossing the others), from low on one side over to a
    hanging end on the other; the ends stay ≥ 9 cm up (a cane planted on the ground at the rim reads as a leg)."""
    nz = rnd.uniform(-0.45, 0.45)
    az = rnd.uniform(0, math.tau)
    n = Vector((math.cos(az) * math.sqrt(1 - nz * nz), math.sin(az) * math.sqrt(1 - nz * nz), nz))
    u = n.cross(Vector((0, 0, 1))).normalized()
    v = n.cross(u).normalized()
    if v.z < 0:
        v = -v
    a0 = math.pi * rnd.uniform(0.05, 0.3)
    a1 = math.pi * rnd.uniform(0.62, 0.95)
    if rnd.random() < 0.5:
        a0, a1 = math.pi - a1, math.pi - a0
    segs = 7
    pts = []
    wob = (rnd.uniform(0, math.tau), rnd.uniform(0.04, 0.08))
    for i in range(segs + 1):
        a = a0 + (a1 - a0) * i / segs
        d = u * math.cos(a) + v * math.sin(a)
        rr = 1.0 + wob[1] * math.sin(3.0 * a + wob[0])
        p = Vector((d.x * radii.x, d.y * radii.y, d.z * radii.z)) * (s * rr)
        p.z = max(p.z, 0.09)
        pts.append(p)
    radii_ = [r0 * (1.0 - 0.45 * abs(i / segs - 0.35) / 0.65) for i in range(segs + 1)]
    part = M.tube(pts, radii_, sides=4 if r0 > 0.016 else 3, dome0=0.006, dome1=0.006)
    part.recalc_normals()
    pid = b.add(part, region, name="cane")
    side = n.copy()
    thorns_along(b, pts, side, thorns, rnd, k)
    rust_leaves(b, pts, leaves, rnd)
    return pid, pts


def escaped_cane(b: M.Builder, radii: Vector, az: float, rnd: random.Random, k: int, region: str) -> list:
    """A long cane escaping the thicket on one side: it leaves the dome at mid height, arches out ≈ 25-35 cm and
    hangs its tip in the air (≥ 14 cm up)."""
    d = Vector((math.cos(az), math.sin(az), 0.0))
    side = Vector((-d.y, d.x, 0.0))
    start = Vector((d.x * radii.x * 0.7, d.y * radii.y * 0.7, radii.z * 0.38))
    reach = rnd.uniform(0.2, 0.28)
    lift = rnd.uniform(0.03, 0.07)
    pts = []
    for i in range(6):
        t = i / 5
        p = start + d * (reach * t) + side * (0.06 * math.sin(math.pi * t) * (1 if k % 2 else -1))
        p.z = start.z + lift * math.sin(math.pi * t * 0.6) - (start.z - 0.16) * t ** 1.6
        pts.append(p)
    part = M.tube(pts, [0.015, 0.014, 0.012, 0.01, 0.008, 0.007], sides=3, dome0=0.006, dome1=0.005)
    part.recalc_normals()
    b.add(part, region, name="cane")
    thorns_along(b, pts, side, 2, rnd, k)
    return pts


def draped_rag(b: M.Builder, pts: list, w: float, drop: float, region: str, rnd: random.Random) -> int:
    """A torn strip of cloth snagged on a cane where it runs down the thicket's side (the cane point farthest out
    between 45 % and 85 % of the height): it hangs outside the tangle, its hem swinging a little away from it,
    ragged, folded — the cloth's two faces are both modelled, so it reads from any yaw."""
    hi = max(p.z for p in pts)
    cand = [i for i in range(1, len(pts) - 1) if 0.45 * hi <= pts[i].z <= 0.85 * hi] or list(range(1, len(pts) - 1))
    i = max(cand, key=lambda j: Vector((pts[j].x, pts[j].y)).length)
    at = pts[i]
    tang = pts[min(i + 1, len(pts) - 1)] - pts[i - 1]
    tang = Vector((tang.x, tang.y, 0.0)).normalized()
    out = Vector((at.x, at.y, 0.0)).normalized()
    nrm = Vector((-tang.y, tang.x, 0.0))
    if nrm.dot(out) < 0:
        nrm = -nrm
    th = math.radians(rnd.uniform(5, 10))           # nearly plumb: a cloth tipped up to the key light lit pink
    up = (Vector((0, 0, 1)) * math.cos(th) - nrm * math.sin(th)).normalized()
    zax = tang.cross(up).normalized()
    hem = [(w / 2, -drop * rnd.uniform(0.82, 0.95)), (w * 0.2, -drop), (-w * 0.05, -drop * rnd.uniform(0.7, 0.82)),
           (-w * 0.3, -drop * rnd.uniform(0.9, 1.0)), (-w / 2, -drop * 0.78)]
    pl = M.plate(list(reversed([(-w / 2, 0.0), (w / 2, 0.0)] + hem)), 0.008, bevel=0.002)
    for vv in pl.bm.verts:                           # folds
        vv.co.z += 0.016 * math.sin(vv.co.x * 26.0)
    m = Matrix.Identity(4)
    for r_ in range(3):
        m[r_][0], m[r_][1], m[r_][2] = tang[r_], up[r_], zax[r_]
    pl.transform(Matrix.Translation(at + nrm * 0.022 + Vector((0, 0, 0.01))) @ m)
    return b.add(pl, region, name="rag", hull_pad=True)


def build_briar(pal: Palette, variant: str = "A"):
    """A bramble thicket ≈ 1.1-1.25 × 0.85-0.95 m, 0.5-0.56 m tall: a dark core (``briar.core``, the shadow
    inside the tangle) under 15 / 17 thorny canes arching in hoops over it at every angle and three heights, in the
    dimmed bark tone (``briar.cane_dark``; every fourth the weathered ``briar.cane``, every fifth the old dark
    ``briar.stem``), thorns and dead rust leaves; one cane escaping low at one side; rags snagged where the top
    hoops run down the side (A: an undyed rag ``briar.rag`` and a crusade-tabard scrap ``briar.rag_tabard``; B: one
    large tabard scrap)."""
    name = f"SM_Foliage_March_Briar_{variant}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="briar")
    b.regions(**R("briar", **EXTRA_REGIONS["briar"]))
    cfg = BRIAR_SIZE[variant]
    w, H = cfg["w"], cfg["h"]
    radii = Vector((0.52 * w, 0.4 * w, H))
    # the dark core: a lumpy low dome inside the hoops (what shows through the tangle)
    core = M.ellipsoid((radii.x * 0.66, radii.y * 0.66, H * 0.52), (0, 0, -0.02), sides=10, rings=5)
    ph = rnd.uniform(0, math.tau)
    for vv in core.bm.verts:
        a = math.atan2(vv.co.y, vv.co.x)
        k_ = 1.0 + 0.14 * math.sin(3 * a + ph) + 0.08 * math.sin(5 * a - ph)
        vv.co.x *= k_
        vv.co.y *= k_
        vv.co.z = max(vv.co.z * (1.0 + 0.2 * math.sin(4 * a + ph + 0.7 * vv.co.z * 10)), -0.02)
    core.smooth(True)
    b.add(core, "core", name="core", hull_pad=True)
    canes = []
    for k in range(cfg["hoops"]):
        s = BRIAR_SCALES[k % len(BRIAR_SCALES)] * rnd.uniform(0.95, 1.05)
        region = "cane" if k % 4 == 1 else ("stem" if k % 5 == 3 else "cane_dark")
        pid, pts = hoop_cane(b, radii, s, rnd, k, region, r0=0.018 if s > 0.85 else 0.015,
                             thorns=2, leaves=1 + (k % 2))
        canes.append((max(p.z for p in pts), pts))
    a_esc = cfg["seed_az"] + rnd.uniform(0, math.tau)
    for j in range(cfg["escaped"]):
        escaped_cane(b, radii, a_esc + (j - 1) * rnd.uniform(0.35, 0.55), rnd, j, "cane_dark")
    canes.sort(key=lambda c: -c[0])
    for j, (region, rw, rd) in enumerate(cfg["rags"]):
        _, pts = canes[j * 3]
        draped_rag(b, pts, rw * rnd.uniform(0.95, 1.05), rd * rnd.uniform(0.95, 1.05), region, rnd)
    return E.finish(b, pal, "decor", ground_h=0.12,
                    wind=lambda co, pid: (0.1 + 0.9 * E.smoothstep(0.1, 0.55, co.z), (co.x * 1.7 + co.y) % 1.0),
                    name=name)

# ── ground cover (no ink) ───────────────────────────────────────────────────────────────────────────────
GRASS_VARIANTS = {"A": (0.42, 11, 7, 5), "B": (0.36, 10, 7, 5), "C": (0.40, 12, 6, 4)}


def seed_head(b: M.Builder, at: Vector, rnd: random.Random, region: str = "seed", name: str = "seed"):
    """A pale seed head on a bent stalk tip (a 2 x 6 cm spindle)."""
    d = Vector((rnd.uniform(-0.5, 0.5), rnd.uniform(-0.5, 0.5), 1.0)).normalized()
    sh = M.ellipsoid((0.011, 0.011, 0.03), (0, 0, 0), sides=5, rings=3)
    sh.transform(Vector((0, 0, 1)).rotation_difference(d).to_matrix().to_4x4())
    sh.translate(at)
    b.add(sh, region, name=name, outline=False)
    stalk = M.tube([at - d * 0.2 + Vector((0, 0, -0.02)), at - d * 0.02], [0.004, 0.003], sides=3, dome0=0.0,
                   dome1=0.0)
    b.add(stalk, "dark", name="stalk", outline=False)


def build_ash_grass(pal: Palette, variant: str = "A"):
    name = f"SM_Foliage_March_AshGrass_{variant}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="ashgrass")
    b.regions(**R("ashgrass"))
    h, n1, n2, n3 = GRASS_VARIANTS[variant]
    sph = {}
    F.tuft(b, Vector((-0.06, 0.03, 0)), h, n1, rnd, sph=sph, spread=1.25)
    F.tuft(b, Vector((0.12, -0.06, 0)), h * 0.7, n2, rnd, sph=sph, segs=2, spread=1.35)
    F.tuft(b, Vector((-0.15, -0.12, 0)), h * 0.46, n3, rnd, sph=sph, segs=2, spread=1.4)
    for q, (x, y, z) in enumerate(((-0.08, 0.04, h * 0.95), (0.15, -0.05, h * 0.72), (-0.02, 0.1, h * 0.8))):
        if variant == "B" and q == 2:
            continue
        seed_head(b, Vector((x, y, z)), rnd)
    obj = E.finish(b, pal, "none", ground_h=0.5, spherise=sph,
                   wind=lambda co, pid: (E.smoothstep(0.0, 0.42, co.z), (co.x * 3.1 + co.y * 1.7) % 1.0),
                   name=name)
    clamp_ground(obj)
    return obj


POPPY_DULL = Region("#6E2420", .42, .12)


def clamp_ground(obj, floor: float = 0.0) -> int:
    """Ground cover: no vertex under the ground plane (env art review: bent blade tips of the wide tufts dipped
    below z 0 — the ``V`` tips cut through the terrain in the close-up). Returns the vertices moved."""
    n = 0
    for v in obj.data.vertices:
        if v.co.z < floor:
            v.co.z = floor
            n += 1
    obj.data.update()
    return n


def build_ash_poppy(pal: Palette, variant: str = "A"):
    """Dead tufts and dull poppies (the foliage's only colour): four upright blooms, two gone to ash and drooping."""
    name = f"SM_Foliage_March_AshPoppy_{variant}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="poppy")
    b.regions(**R("poppy", **{k: v for k, v in R("ashgrass").items()}))
    # env art review (march): poppy.petal #7A2A26 (light .30) lit to #A2665B, a dusty pink across 260 instances —
    # a dull blood red with a short light band (lit L* ≈ 34.5, hue 5°), stays under the crimson telegraphs
    b.swatch["petal"] = b.region("petal_dull", POPPY_DULL)
    sph = {}
    F.tuft(b, Vector((-0.1, 0.06, 0)), 0.24, 6, rnd, sph=sph, segs=2, spread=1.3)
    F.tuft(b, Vector((0.12, -0.04, 0)), 0.18, 5, rnd, sph=sph, segs=2, spread=1.3)
    cols = ["petal", "petal", "ash", "petal", "ash", "petal"] if variant == "A" else \
        ["petal", "ash", "petal", "petal", "petal", "ash"]
    spots = [(-0.18, -0.08), (-0.04, -0.16), (0.1, 0.1), (0.2, -0.12), (-0.02, 0.16), (0.04, -0.02)]
    for q, ((x, y), col) in enumerate(zip(spots, cols)):
        x += rnd.uniform(-0.03, 0.03)
        y += rnd.uniform(-0.03, 0.03)
        hgt = rnd.uniform(0.16, 0.3) * (0.75 if col == "ash" else 1.0)
        top = Vector((x, y, hgt))
        if col == "ash":                              # gone to ash: the head hangs over on its stalk
            nrm = Vector((rnd.uniform(-1, 1), rnd.uniform(-1, 1), -0.6)).normalized()
        else:
            nrm = Vector((rnd.uniform(-0.25, 0.25), rnd.uniform(-0.25, 0.25), 1)).normalized()
        b.add(E.blade(Vector((x, y, -0.01)), (top - Vector((x, y, 0))).normalized(), hgt, 0.011, bend=0.08,
                      segs=2, walls=False), "dark", name="stem", outline=False)
        r = rnd.uniform(0.055, 0.07) * (0.8 if col == "ash" else 1.0)
        b.add(E.bloom(top, r, petals=4, normal=nrm, cup=0.35, phase=rnd.uniform(0, 1), per_petal=2), col,
              name="bloom", outline=False)
        b.add(F.dome_fan(top + nrm * 0.006, nrm, r * 0.32, r * 0.32, r * 0.25), "centre", name="centre",
              outline=False)
    obj = E.finish(b, pal, "none", ground_h=0.35, spherise=sph,
                   wind=lambda co, pid: (E.smoothstep(0.0, 0.33, co.z), (co.x * 2.3 + co.y * 3.7) % 1.0),
                   name=name)
    clamp_ground(obj)
    return obj


# ── ash-cap fungus ──────────────────────────────────────────────────────────────────────────────────────
def inkcap(b: M.Builder, c: Vector, h: float, cap_r: float, tilt: Vector, k: int, sides: int = 7) -> int:
    """A pale ash-cap (inky-cap shape): a slender stem under a tall bell cap; the cap's underside (normals down)
    is the dark gill region (``af_sub`` 1)."""
    up = Vector((0, 0, 1)).lerp(tilt, 0.3).normalized()
    stem = M.lathe([(0.3 * cap_r, -0.01), (0.27 * cap_r, 0.5 * h), (0.22 * cap_r, h + 0.01)], sides=5, cap0=None,
                   cap1=None)
    cap = M.lathe([(0.2 * cap_r, h + 0.05 * cap_r), (0.98 * cap_r, h - 0.05 * cap_r), (0.9 * cap_r, h + 0.5 * cap_r),
                   (0.62 * cap_r, h + 1.05 * cap_r), (0.02, h + 1.3 * cap_r)], sides=sides, cap0="fan", cap1="fan")
    cap.recalc_normals()
    cap.bm.normal_update()
    lay = cap.bm.faces.layers.int.new("af_sub")
    for f in cap.bm.faces:
        if f.normal.z < -0.3:
            f[lay] = 1
    rot = Vector((0, 0, 1)).rotation_difference(up).to_matrix().to_4x4()
    for p in (stem, cap):
        p.transform(rot)
        p.translate(c)
    b.add(stem, "stem", name=f"stem{k}")
    return b.add(cap, "cap", name=f"cap{k}", hull_pad=True, sub_regions={1: "gill"})


def build_ashcap(pal: Palette, variant: str = "A"):
    """A cluster of pale ash-caps (≤ 300 toon tris like the plains toadstools): three / four bells at varied
    heights, leaning apart, a dead tuft hugging them (inked: part of the silhouette)."""
    name = f"SM_Foliage_March_AshCap_{variant}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="ashcap")
    b.regions(**R("ashcap", **{k: v for k, v in R("ashgrass").items() if k in ("dark", "light")}))
    caps = ([(Vector((-0.04, 0.02, 0)), 0.2, 0.06, Vector((-0.3, 0.1, 1))),
             (Vector((0.08, -0.04, 0)), 0.14, 0.05, Vector((0.4, -0.2, 1))),
             (Vector((-0.1, -0.08, 0)), 0.1, 0.04, Vector((-0.3, -0.5, 1)))] if variant == "A" else
            [(Vector((0.0, 0.03, 0)), 0.17, 0.055, Vector((0.1, 0.3, 1))),
             (Vector((0.1, -0.06, 0)), 0.22, 0.062, Vector((0.35, -0.1, 1))),
             (Vector((-0.09, -0.06, 0)), 0.12, 0.045, Vector((-0.4, -0.2, 1))),
             (Vector((0.16, 0.08, 0)), 0.08, 0.035, Vector((0.5, 0.3, 1)))])
    for k, (c, hh, r, tilt) in enumerate(caps):
        inkcap(b, c, hh, r, tilt, k, sides=7 if k < 2 else 6)
    F.tuft(b, Vector((0.02, 0.12, 0)), 0.16, 3, rnd, segs=2, inked=True)
    return E.finish(b, pal, "decor", ground_h=0.3,
                    wind=lambda co, pid: (0.1 * E.smoothstep(0.0, 0.3, co.z), 0.4), name=name)


# ── reeds and oily scum on the black water ──────────────────────────────────────────────────────────────
def scum(b: M.Builder, c: Vector, rx: float, ry: float, rnd: random.Random, k: int, region: str = "oil") -> int:
    """An irregular patch of oily scum floating on the water, with a sheen streak lying on it."""
    n = 12
    pts = []
    ph = rnd.uniform(0, math.tau)
    for i in range(n):
        a = math.tau * i / n
        kk = 1 + 0.18 * math.sin(3 * a + ph) + 0.1 * math.sin(5 * a + k)
        pts.append((math.cos(a) * rx * kk, math.sin(a) * ry * kk))
    pl = M.plate(pts, 0.01)
    pl.translate(c + Vector((0, 0, F.PAD_LIFT + 0.005)))
    pid = b.add(pl, region, name=f"scum{k}", hull_pad=True)
    ztop = F.PAD_LIFT + 0.0102
    a = ph
    d = Vector((math.cos(a), math.sin(a), 0.0))
    s = Vector((-d.y, d.x, 0.0))
    pts3 = [c + d * (rx * t * 0.6) + s * (ry * 0.25 * math.sin(t * 2.0)) + Vector((0, 0, ztop)) for t in
            (-1.0, -0.4, 0.2, 0.8)]
    b.add(E.ribbon(pts3, [Vector((0, 0, 1))] * 4, [0.012, 0.03, 0.026, 0.01], lift=0.0005, thick=0.0015), "sheen",
          name="sheen", outline=False, covered_by=pid)
    return pid


REED_W = 0.03       # blade width (ground cover, no ink: see build_reeds)


def reed_clump(b: M.Builder, c: Vector, n: int, h: float, rnd: random.Random, heads: int = 2):
    """Dead reeds: stiff blades (inked, walls) standing out of the water, a few snapped and bent over; brown
    cattail heads on ``heads`` of them."""
    for i in range(n):
        az = rnd.uniform(0, math.tau)
        lean = rnd.uniform(0.05, 0.3)
        d = Vector((math.cos(az) * lean, math.sin(az) * lean, 1.0)).normalized()
        L = h * rnd.uniform(0.6, 1.0)
        base = c + Vector((math.cos(az) * 0.03, math.sin(az) * 0.03, F.PAD_LIFT - 0.02))
        snapped = i % 4 == 3
        if snapped:                   # snapped: a short stub + the top half folded over
            L1 = L * 0.5
            b.add(E.blade(base, d, L1, REED_W * 0.92, bend=0.0, segs=1, walls=True, thick=0.01, tip_w=REED_W * 0.66),
                  "stem", name="reed")
            tip = base + d * L1
            d2 = Vector((math.cos(az + 0.6), math.sin(az + 0.6), -0.55)).normalized()
            b.add(E.blade(tip, d2, L * 0.45, REED_W * 0.66, bend=0.05, segs=2, walls=True, thick=0.01), "stem",
                  name="reed")
            continue
        b.add(E.blade(base, d, L, REED_W, bend=rnd.uniform(0.05, 0.15), segs=3, walls=True, thick=0.01), "stem",
              name="reed")
        if i < heads:
            top = base + d * (L * 0.82)
            hd = M.ellipsoid((0.018, 0.018, 0.06), (0, 0, 0), sides=6, rings=4)
            hd.transform(Vector((0, 0, 1)).rotation_difference(d).to_matrix().to_4x4())
            hd.translate(top)
            b.add(hd, "head", name="head")


def build_reeds(pal: Palette, variant: str = "A"):
    name = f"SM_Foliage_March_Reeds_{variant}"
    rnd = scene.rng(name)
    b = M.Builder(name, pal, prefix="reed")
    b.regions(**R("reed", **{f"{k}": v for k, v in R("scum").items()}, **EXTRA_REGIONS["reed"]))
    # env art review (march): the inked slick plates read as dark rectangular slabs on the pond — the reeds stand
    # straight out of the water now (the terrain's water carries the oily sheen)
    reed_clump(b, Vector((0.06, 0.04, 0)), 7, 0.55, rnd, heads=2)
    if variant == "B":
        reed_clump(b, Vector((-0.2, 0.12, 0)), 5, 0.42, rnd, heads=1)
    # ground cover like the ash grass (no hull, MI_AF_Toon_Foliage_March_Ground): without the slick plate the reeds
    # are only 2-3 cm blades, all ink at 1080p (baked rim 2.6 px > the decor gate's 2.1 px)
    return E.finish(b, pal, "none", ground_h=None, name=name,
                    wind=lambda co, pid: (0.4 * E.smoothstep(0.05, 0.5, co.z), (co.x * 2.1) % 1.0))


# ── registry ────────────────────────────────────────────────────────────────────────────────────────────
TREE = dict(base_ids=["tree"], footprint=[1, 1], blocking=True, foliage=True, blob=0.5)
ASSETS: dict = {}
for _v in "ABC":
    ASSETS[f"SM_Foliage_March_DeadOak_{_v}"] = (lambda pal, v=_v: build_dead_oak(pal, v), dict(TREE))
for _v in "AB":
    ASSETS[f"SM_Foliage_March_CharredOrchard_{_v}"] = (
        lambda pal, v=_v: build_orchard(pal, v),
        dict(base_ids=["tree_round"], footprint=[1, 1], blocking=True, foliage=True, blob=0.42))
    ASSETS[f"SM_Foliage_March_Briar_{_v}"] = (
        lambda pal, v=_v: build_briar(pal, v),
        dict(base_ids=["bush"], footprint=[1, 1], blocking=False, foliage=True, blob=0.5))
    ASSETS[f"SM_Foliage_March_AshPoppy_{_v}"] = (
        lambda pal, v=_v: build_ash_poppy(pal, v),
        dict(base_ids=["flower"], footprint=[1, 1], blocking=False, foliage=True, toon_slot=EM.GROUND_SLOT,
             toon_params=EM.GROUND_WIND, blob=0.2, turnaround=False))
    ASSETS[f"SM_Foliage_March_AshCap_{_v}"] = (
        lambda pal, v=_v: build_ashcap(pal, v),
        dict(base_ids=["mushroom_red"], footprint=[1, 1], blocking=False, foliage=True, blob=0.2))
    ASSETS[f"SM_Foliage_March_Reeds_{_v}"] = (
        lambda pal, v=_v: build_reeds(pal, v),
        dict(base_ids=["lily_pad_plains"], footprint=[1, 1], blocking=False, foliage=True, blob=0.01,
             toon_slot=EM.GROUND_SLOT, toon_params=EM.GROUND_WIND, turnaround=False))
for _v in "ABC":
    ASSETS[f"SM_Foliage_March_AshGrass_{_v}"] = (
        lambda pal, v=_v: build_ash_grass(pal, v),
        dict(base_ids=["grass"], footprint=[1, 1], blocking=False, foliage=True, toon_slot=EM.GROUND_SLOT,
             toon_params=EM.GROUND_WIND, blob=0.2, turnaround=False))

LOD1 = {f"SM_Foliage_March_DeadOak_{v}": (lambda pal, v=v: build_dead_oak(pal, v, lod=1)) for v in "ABC"}
LOD1.update({f"SM_Foliage_March_CharredOrchard_{v}": (lambda pal, v=v: build_orchard(pal, v, lod=1)) for v in "AB"})

NOTES = {
    "tree": "W5: tall decor, blocks its tile (trunk + root flare); the bare crown overhangs the neighbours like the "
            "plains canopy",
    "ground": "ground cover: no outline hull (spec 1.3), MI_AF_Toon_Foliage_March_Ground (gentle low sway)",
}


def main(argv: list[str]) -> int:
    quick = "--quick" in argv
    sheet = "--sheet" in argv
    previews = "--no-previews" not in argv
    names = [a for a in argv if not a.startswith("--")] or list(ASSETS)
    names = [n for n in ASSETS if any(n == a or a in n for a in names)]
    scene.reset()
    pal = Palette(FAM)
    if sheet:
        objs = [ASSETS[n][0](pal) for n in names]
        pal.save()
        E.sheet(objs, "march_sheet_" + "_".join(n.split("_")[-2][:5] + n.split("_")[-1] for n in names)[:80], cols=3)
        return 0
    if quick:
        objs = [ASSETS[n][0](pal) for n in names]
        pal.save()
        for o in objs:                     # the review "front": local -Y toward the camera
            o.rotation_euler = (0, 0, math.radians(-135.0))
        big = any(k in n for n in names for k in ("Oak", "Orchard"))
        E.quick(objs, "march_foliage_" + "_".join(n.split("_")[-2][:5] + n.split("_")[-1] for n in names)[:80],
                spacing=3.2 if big else 1.4)
        return 0
    shipped = []
    for n in (names if "--family" not in argv else []):
        scene.reset()
        pal = Palette(FAM)
        fn, kw = ASSETS[n]
        kw = dict(kw)
        extra = {"displayName": n.replace("SM_Foliage_March_", "")}
        lod_obj = None
        if n in LOD1:
            lod_obj = LOD1[n](pal)
            extra["lods"] = [E.export_lod1(lod_obj, n, CAT)]
            lod_obj.hide_render = True
        obj = fn(pal)
        pal.save()
        if lod_obj is not None:
            extra["budget"] = {"spec": "art-inventory-ch1 §1.9 large decor (trees): LOD0 3-5k, LOD1 1.5k",
                               "lod0": {"toon": E.tris(obj, 0), "outline": E.tris(obj, 1)},
                               "lod1": dict(extra["lods"][0]["triangles"])}
        if kw.get("blocking"):
            extra["blockingNote"] = NOTES["tree"]
        if kw.get("toon_slot"):
            extra["groundCover"] = NOTES["ground"]
        rx = (lambda rv, l0=obj, l1=lod_obj: E.lod_preview(rv, l0, l1)) if lod_obj is not None else None
        EM.ship(obj, n, CAT, pal, obj["af_outline_class"], family="environment.foliage", extra=extra,
                previews=previews, review_extra=rx, **kw)
        shipped.append(n)
    if "--family" in argv:
        shipped = names
    if previews and len(shipped) > 1:
        scene.reset()
        pal = Palette(FAM)
        objs = [ASSETS[n][0](pal) for n in shipped]
        trees = [o for o in objs if "Oak" in o.name or "Orchard" in o.name]
        small = [o for o in objs if o not in trees]
        oaks = [o for o in trees if "Oak" in o.name]
        orch = [o for o in trees if o not in oaks]
        if oaks:
            EM.family_sheet(oaks, "foliage_trees.png", "MARCH DEAD OAKS", cols=4)
        if orch:
            EM.family_sheet(orch, "foliage_orchard.png", "MARCH CHARRED ORCHARD", cols=4)
        for o in trees:
            o.hide_render = True
        if small:
            EM.family_sheet(small, "foliage_small.png", "MARCH BRIAR GROUND COVER ASH-CAP REEDS", cols=6)
    return E.summary()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

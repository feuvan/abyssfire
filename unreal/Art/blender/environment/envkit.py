"""Shared helpers of the environment family (spec art-inventory-ch1.md §6; DECISIONS R1, R6, R7, W5, W7).

Every environment generator (``foliage.py``, ``rocks.py``, ``camp.py``, ``story.py``, ``interactive.py``,
``goblin_camp.py``, ``terrain.py``, ``vignette.py``) builds its meshes with the kit's ``Builder`` and ships them
through :func:`ship` (FBX + merged manifest entry + the kit review set with the ink gate). What this module adds on
top of the kit (nothing in ``kit/`` is changed):

* **shapes** for nature and props: :func:`lump` (a foliage "cloud" lump = union of spheres → rounded lobes with
  sharp inward cusps, the web ``cloudP`` in 3D), :func:`rock` (radial-function boulder, faceted or smooth, flat
  foot, optional lit top plane) + :func:`rock_cap` (a moss cap draped over the same surface with drips),
  :func:`ring_loft` (closed tube through arbitrary rings: trunks with root flare, stumps, posts), :func:`blade`
  (tapered, curved two-sided grass blade / leaf), :func:`ribbon` (a thin strip hugging a surface: bark grooves,
  cracks, rope wraps), :func:`bloom` (five-petal flower), :func:`plank` (bevelled board), :func:`spike` (stake);
* **spherised normals** (spec §6.5 "toon with spherised normals per canopy lump"): ``finish(spherise={pid: …})``
  blends a part's shading normals toward its ellipsoid's normals before the hull is baked, so a lumpy canopy takes
  the web's clean three cel bands per lump;
* **wind-ready vertex data** (spec §6.5 WPO sway): ``finish(wind=fn)`` writes ``AF_Data.g`` = sway weight (0 rigid
  base … 1 free tip) and ``AF_Data.b`` = phase (0..1, per lump / branch) on the toon vertices, and the same pair in a
  second UV channel ``UVWind`` (UV1: U weight, V phase) that the baked hull copies (the hull's ``AF_Data`` RGB holds
  its normal), so body and ink can sway identically; ``M_AF_Toon_Foliage`` today uses the height-based
  ``AF_Wind`` node, the weights are there for the per-vertex upgrade;
* :func:`ship`: export + manifest (merging, foliage parent ``M_AF_Toon_Foliage``, ``footprintTiles`` / ``blocking``
  for W5, ``outlineMix``) + review (``game.png``, ``game_1080crop.png``, ``game_inkgate.png``, ``closeup.png``,
  ``turnaround.png``) with ``ground_occludes`` (hull under the floor is hidden by the terrain in UE);
* :func:`family_sheet`: every variant of a family at the W1 game camera, native 1080p pixels, side by side with
  the warrior silhouette for scale;
* :func:`toon_material_files` / :func:`import_warrior`: the shipped warrior (FBX + Idle clip + sword / shield on
  their sockets, Heroes palette) for scale in sheets and the vignette, without rewriting the Heroes palette.
"""
from __future__ import annotations

import json
import math
import random
import sys
from pathlib import Path
from types import SimpleNamespace
from typing import Callable, Sequence

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import export, outline, paths, pngio, review, scene, shading  # noqa: E402
from kit import mesh as M  # noqa: E402
from kit.mesh import Part  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

V3 = Sequence[float]
FOLIAGE_TOON = "M_AF_Toon_Foliage"
PROPS = "Props_Plains"
FOLIAGE = "Foliage_Plains"
RESULTS: list[dict] = []           # one record per shipped asset (printed by summary())
# Screen ink per (palette, class) where the environment departs from outline.SCREEN_PX_1080 (UE builds one outline
# instance per palette + class, so the value is family-wide). Props_Plains decor: 1.4 px instead of the spec's 1.2 —
# camp poles, stakes and banner shafts are 8–12 px wide at 1080p and their lit edge carries the warm rim; at 1.2 px
# the anti-aliased line never gets darker than lum ~67 over the grass (the ink gate's dark check, lum 64, failed on
# 20 % of their edges) — 1.4 px keeps decor well under interactive (2.0) and reads as the same thin line art.
PX_OVERRIDE = {(PROPS, "decor"): 1.4}


def ink_px(family: str, cls: str) -> float:
    return PX_OVERRIDE.get((family, cls), outline.SCREEN_PX_1080.get(cls, 0.0))


# ── manifest ────────────────────────────────────────────────────────────────────────────────────────────
class EnvManifest(export.Manifest):
    """The kit manifest writer, also merging the top-level ``terrain`` section this family owns (the terrain
    material inputs, spec §6.1). Same lock, same per-key merge; other sections behave exactly as in the kit."""

    MERGED = ("assets", "palettes", "gameIds", "terrain")


# ── small maths ─────────────────────────────────────────────────────────────────────────────────────────
def _v(p) -> Vector:
    return Vector((float(p[0]), float(p[1]), float(p[2])))


def smoothstep(e0: float, e1: float, x: float) -> float:
    t = min(1.0, max(0.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


def fib_dirs(n: int, rnd: random.Random, zmin: float = -1.0, zmax: float = 1.0, jitter: float = 0.25):
    """``n`` well-spread unit directions with z in [zmin, zmax] (golden-angle spiral + jitter)."""
    out = []
    ga = math.pi * (3 - math.sqrt(5))
    ph = rnd.random() * math.tau
    for i in range(n):
        t = (i + 0.5) / n
        z = zmax + (zmin - zmax) * t + (rnd.random() - 0.5) * jitter * (zmax - zmin) / max(n, 1)
        z = max(-1.0, min(1.0, z))
        a = ph + ga * i + (rnd.random() - 0.5) * jitter
        r = math.sqrt(max(0.0, 1 - z * z))
        out.append(Vector((r * math.cos(a), r * math.sin(a), z)))
    return out


def icosphere(subdiv: int = 2) -> bmesh.types.BMesh:
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=1.0)
    return bm


# ── foliage lump ────────────────────────────────────────────────────────────────────────────────────────
class Lump:
    """Cloud lump: the union of a core sphere and ``lobes`` sub-spheres reaching radius ≈ 1 along well spread,
    mostly upward directions, scaled to ``radii`` — rounded lobes with sharp inward cusps (web ``cloudP``).
    ``part()`` samples the radial union on an icosphere (``subdiv`` 2 = 320 tris, 3 = 1280); ``inside(p)`` is the
    exact solid (cull faces hidden inside a neighbouring lump: :func:`cull_hidden`)."""

    def __init__(self, radii: V3, seed, center: V3 = (0, 0, 0), lobes: int = 7, amp: float = 0.24,
                 subdiv: int = 2, squash_bottom: float = 0.7, zmin: float = -0.35, core: float | None = None,
                 rho_lo: float = 0.52, rho_hi: float = 0.66):
        rnd = random.Random(scene.seed_for("lump", seed))
        self.subs = [(Vector((0, 0, 0)), core if core is not None else 1.0 - amp)]
        for d in fib_dirs(lobes, rnd, zmin, 0.92):
            rho = rho_lo + rnd.random() * (rho_hi - rho_lo)
            a = (1.0 - rho) * (0.9 + rnd.random() * 0.2)
            self.subs.append((d * a, rho))
        self.R = _v(radii)
        self.C = _v(center)
        self.subdiv = subdiv
        self.squash = squash_bottom

    def radial(self, d: Vector) -> float:
        best = 0.0
        for c, rho in self.subs:
            b = d.dot(c)
            disc = b * b - c.length_squared + rho * rho
            if disc > 0:
                best = max(best, b + math.sqrt(disc))
        return best

    def _unit(self, p: Vector) -> Vector:
        q = p - self.C
        q = Vector((q.x / self.R.x, q.y / self.R.y, q.z / self.R.z))
        if q.z < 0:
            q.z /= self.squash
        return q

    def inside(self, p: Vector, margin: float = 0.0) -> bool:
        q = self._unit(p)
        L = q.length
        if L < 1e-9:
            return True
        return L < self.radial(q / L) - margin / max(min(self.R), 1e-3)

    def part(self) -> Part:
        bm = icosphere(self.subdiv)
        for v in bm.verts:
            d = v.co.normalized()
            p = d * self.radial(d)
            if p.z < 0:
                p.z *= self.squash
            v.co = self.C + Vector((p.x * self.R.x, p.y * self.R.y, p.z * self.R.z))
        part = Part(bm).smooth(True)
        part.recalc_normals()
        return part


def lump(radii: V3, seed, center: V3 = (0, 0, 0), **kw) -> Part:
    return Lump(radii, seed, center, **kw).part()


def cull_hidden(part: Part, others: Sequence, margin: float = 0.012) -> int:
    """Delete faces of ``part`` whose vertices all lie inside one of ``others`` (objects with ``inside(p,
    margin)``) — canopy lumps hidden in their neighbours. Returns the number of faces removed."""
    bm = part.bm
    inside = {v: any(o.inside(v.co, margin) for o in others) for v in bm.verts}
    dead = [f for f in bm.faces if all(inside[v] for v in f.verts)]
    if dead:
        bmesh.ops.delete(bm, geom=dead, context="FACES")
        loose = [v for v in bm.verts if not v.link_faces]
        if loose:
            bmesh.ops.delete(bm, geom=loose, context="VERTS")
    return len(dead)


def lump_info(radii: V3, center: V3 = (0, 0, 0), mix: float = 0.65) -> tuple:
    """``finish(spherise=…)`` entry for a lump: (centre, radii, mix toward the ellipsoid normal)."""
    return (_v(center), _v(radii), mix)


# ── rocks ───────────────────────────────────────────────────────────────────────────────────────────────
class RockShape:
    """Radial function of a stylised boulder: ellipsoid ``radii`` × (1 + low-frequency bumps), flat foot at z = 0
    (sunk ``sink`` below the ground), optional flattened top (``top_cut`` = fraction of the height, the lit top
    plane of the web rocks). ``point(d)`` → surface point (local, before ``center``)."""

    def __init__(self, radii: V3, seed, bumps: int = 5, amp: float = 0.10, sink: float = 0.03,
                 top_cut: float = 0.0, lean: V3 = (0, 0, 0)):
        rnd = random.Random(scene.seed_for("rock", seed))
        self.R = _v(radii)
        self.waves = [(Vector((rnd.uniform(-1, 1), rnd.uniform(-1, 1), rnd.uniform(-0.6, 1))).normalized(),
                       rnd.uniform(1.6, 3.4), rnd.uniform(0, math.tau), amp * rnd.uniform(0.5, 1.0))
                      for _ in range(bumps)]
        self.sink = sink
        self.top_cut = top_cut
        self.lean = _v(lean)

    def radial(self, d: Vector) -> float:
        s = 1.0
        for u, f, ph, a in self.waves:
            s += a * math.sin(f * d.dot(u) * math.pi + ph)
        return s

    def point(self, d: Vector, extra: float = 0.0) -> Vector:
        d = d.normalized()
        r = self.radial(d) + extra / max(min(self.R.x, self.R.y, self.R.z), 1e-3)
        p = Vector((d.x * self.R.x * r, d.y * self.R.y * r, d.z * self.R.z * r))
        # rocks sit on a centre at z = R.z * 0.62: the lower part is cut by the ground plane
        p.z += self.R.z * 0.62
        if self.top_cut > 0:
            top = self.R.z * (0.62 + 1.0 - self.top_cut) + extra
            if p.z > top:
                p.z = top + (p.z - top) * 0.18
        p.z = max(p.z, -self.sink)
        p += self.lean * (max(p.z, 0.0) / max(self.R.z, 1e-3))
        return p


def rock(shape: RockShape, center: V3 = (0, 0, 0), subdiv: int = 2, facet: float | None = 28.0,
         yaw: float = 0.0) -> Part:
    """Boulder mesh from a :class:`RockShape` (icosphere sampling). ``facet`` = auto-sharp angle (faceted rocks)
    or None (smooth, the rounded outcrop boulders)."""
    bm = icosphere(subdiv)
    for v in bm.verts:
        v.co = shape.point(v.co)
    # weld the flattened foot: verts pressed onto the floor plane stay (closed, no holes)
    part = Part(bm)
    part.recalc_normals()
    if facet is None:
        part.smooth(True)
    else:
        part.auto_sharp(facet)
    if yaw:
        part.rotate("z", yaw)
    part.translate(center)
    return part


def rock_cap(shape: RockShape, edge: Callable[[float], float], center: V3 = (0, 0, 0), thick: float = 0.035,
             cols: int = 28, rows: int = 5, inner: float = 0.03, yaw: float = 0.0) -> Part:
    """A cap (moss, snow) draped over ``shape``: from the top down to the polar angle ``edge(azimuth)`` (radians
    from +Z; drips = local bumps of ``edge``), standing ``thick`` proud of the rock and closed ``inner`` inside it."""
    bm = bmesh.new()
    outer_rows, inner_rows = [], []
    top_o = bm.verts.new(shape.point(Vector((0, 0, 1)), thick))
    top_i = bm.verts.new(shape.point(Vector((0, 0, 1)), -inner))
    for j in range(1, rows + 1):
        ro, ri = [], []
        for i in range(cols):
            az = math.tau * i / cols
            pol = edge(az) * (j / rows) ** 0.85
            d = Vector((math.sin(pol) * math.cos(az), math.sin(pol) * math.sin(az), math.cos(pol)))
            ro.append(bm.verts.new(shape.point(d, thick * (1.0 - 0.35 * (j == rows)))))
            ri.append(bm.verts.new(shape.point(d, -inner)))
        outer_rows.append(ro)
        inner_rows.append(ri)
    for i in range(cols):
        k = (i + 1) % cols
        bm.faces.new((top_o, outer_rows[0][i], outer_rows[0][k]))
        bm.faces.new((top_i, inner_rows[0][k], inner_rows[0][i]))
        for j in range(rows - 1):
            a, b = outer_rows[j], outer_rows[j + 1]
            bm.faces.new((a[i], b[i], b[k], a[k]))
            a, b = inner_rows[j], inner_rows[j + 1]
            bm.faces.new((a[k], b[k], b[i], a[i]))
        bm.faces.new((outer_rows[-1][i], inner_rows[-1][i], inner_rows[-1][k], outer_rows[-1][k]))
    part = Part(bm).smooth(True)
    part.recalc_normals()
    if yaw:
        part.rotate("z", yaw)
    return part.translate(center)


def drip_edge(base: float, seed, drips: int = 3, depth: float = 0.35, wobble: float = 0.10,
              width: float = 0.22) -> Callable[[float], float]:
    """Polar edge of a cap: ``base`` radians from the pole, a gentle wobble and ``drips`` narrow tongues."""
    rnd = random.Random(scene.seed_for("drip", seed))
    ph = [rnd.uniform(0, math.tau) for _ in range(3)]
    tongues = [(rnd.uniform(0, math.tau), depth * rnd.uniform(0.6, 1.0)) for _ in range(drips)]

    def f(az: float) -> float:
        e = base * (1 + wobble * math.sin(3 * az + ph[0]) + wobble * 0.6 * math.sin(5 * az + ph[1]))
        for a0, dp in tongues:
            dd = math.atan2(math.sin(az - a0), math.cos(az - a0))
            e += dp * math.exp(-(dd / width) ** 2)
        return min(e, math.pi * 0.92)
    return f


# ── lofts, blades, strips ───────────────────────────────────────────────────────────────────────────────
def ring_loft(rings: Sequence[Sequence[V3]], cap0: str | None = "fan", cap1: str | None = "fan",
              smooth: bool = True) -> Part:
    """Closed tube through ``rings`` (equal vertex counts, bottom → top / start → end)."""
    bm = bmesh.new()
    vr = [[bm.verts.new(_v(p)) for p in ring] for ring in rings]
    m = len(vr[0])
    for k in range(len(vr) - 1):
        a, b = vr[k], vr[k + 1]
        for i in range(m):
            j = (i + 1) % m
            bm.faces.new((a[i], a[j], b[j], b[i]))
    for cap, ring in ((cap0, vr[0]), (cap1, vr[-1])):
        if cap is None:
            continue
        if cap == "ngon":
            bm.faces.new(ring)
        else:
            c = bm.verts.new(sum((v.co for v in ring), Vector()) / len(ring))
            for i in range(m):
                bm.faces.new((ring[i], ring[(i + 1) % m], c))
    part = Part(bm).smooth(smooth)
    part.recalc_normals()
    return part


def circle(c: V3, r: float, n: int, z_axis: V3 = (0, 0, 1), rx: float | None = None, ry: float | None = None,
           phase: float = 0.0, radial: Callable[[float], float] | None = None) -> list[Vector]:
    """Ring of ``n`` points around ``c`` in the plane normal to ``z_axis``; ``radial(angle)`` scales the radius."""
    z = _v(z_axis).normalized()
    x = z.orthogonal().normalized()
    if abs(z.z) > 0.9:
        x = Vector((1, 0, 0)) - z * z.x
        x.normalize()
    y = z.cross(x)
    out = []
    for i in range(n):
        a = phase + math.tau * i / n
        k = radial(a) if radial else 1.0
        out.append(_v(c) + x * (math.cos(a) * (rx or r) * k) + y * (math.sin(a) * (ry or r) * k))
    return out


def blade(base: V3, direction: V3, length: float, width: float, bend: float = 0.25, side: V3 | None = None,
          segs: int = 3, thick: float = 0.004, tip_w: float = 0.0, walls: bool = True) -> Part:
    """Tapered curved blade (grass, leaf, feather, flame tongue): from ``base`` along ``direction``, curving by
    ``bend`` × length toward the horizontal; two-sided (front / back faces ``thick`` apart, ``af_sub`` 1 / 2)."""
    d = _v(direction).normalized()
    s = _v(side).normalized() if side is not None else d.cross(Vector((0, 0, 1)))
    if s.length < 1e-6:
        s = Vector((1, 0, 0))
    s.normalize()
    out = d.cross(s).normalized()                  # the blade's face normal
    horiz = Vector((d.x, d.y, 0))
    horiz = horiz.normalized() if horiz.length > 1e-6 else out
    pts, ws = [], []
    for i in range(segs + 1):
        t = i / segs
        p = _v(base) + d * (length * t) + horiz * (bend * length * t * t) - Vector((0, 0, bend * length * t * t * 0.6))
        pts.append(p)
        ws.append(width * (1 - t) + tip_w * t)
    bm = bmesh.new()
    sub = bm.faces.layers.int.new("af_sub")
    front, back = [], []
    for p, w in zip(pts, ws):
        h = s * (w / 2)
        front.append((bm.verts.new(p - h + out * thick / 2), bm.verts.new(p + h + out * thick / 2)))
        back.append((bm.verts.new(p - h - out * thick / 2), bm.verts.new(p + h - out * thick / 2)))
    for i in range(segs):
        f = bm.faces.new((front[i][0], front[i][1], front[i + 1][1], front[i + 1][0]))
        f[sub] = 1
        g = bm.faces.new((back[i][1], back[i][0], back[i + 1][0], back[i + 1][1]))
        g[sub] = 2
    for i in range(segs if walls else 0):           # side walls
        for k in (0, 1):
            a, b, c, e = front[i][k], front[i + 1][k], back[i + 1][k], back[i][k]
            bm.faces.new((a, e, c, b) if k == 0 else (a, b, c, e))
    if walls:
        bm.faces.new((front[0][0], back[0][0], back[0][1], front[0][1]))
        tip = [front[-1][0], front[-1][1], back[-1][1], back[-1][0]]
        try:
            bm.faces.new(tip)
        except ValueError:
            pass
    bm.normal_update()
    # orient: front faces along +out
    for f in bm.faces:
        if f[sub] == 1 and f.normal.dot(out) < 0:
            f.normal_flip()
        if f[sub] == 2 and f.normal.dot(out) > 0:
            f.normal_flip()
    bmesh.ops.recalc_face_normals(bm, faces=[f for f in bm.faces if f[sub] == 0])
    return Part(bm).smooth(True)


def ribbon(points: Sequence[V3], normals: Sequence[V3], width: float | Sequence[float], lift: float = 0.004,
           thick: float = 0.003) -> Part:
    """A thin closed strip lying on a surface (bark grooves, cracks, painted bands): centre line ``points``,
    surface ``normals``; ``lift`` above the surface."""
    n = len(points)
    ws = [width] * n if isinstance(width, (int, float)) else list(width)
    rings = []
    for i in range(n):
        p, nm = _v(points[i]), _v(normals[i]).normalized()
        t = (_v(points[min(i + 1, n - 1)]) - _v(points[max(i - 1, 0)])).normalized()
        s = t.cross(nm).normalized()
        c = p + nm * lift
        w = ws[i] / 2
        rings.append([c - s * w, c + s * w, c + s * w + nm * thick, c - s * w + nm * thick])
    return ring_loft(rings, cap0="ngon", cap1="ngon", smooth=False).auto_sharp(60)


def bloom(center: V3, radius: float, petals: int = 5, normal: V3 = (0, 0, 1), cup: float = 0.25,
          thick: float = 0.006, phase: float = 0.0, per_petal: int = 2, walls: bool = False) -> Part:
    """Flat five-petal flower (petal fan, slightly cupped) — the centre is added separately."""
    nm = _v(normal).normalized()
    x = nm.orthogonal().normalized()
    y = nm.cross(x)
    pts = []
    N = petals * per_petal
    for i in range(N):
        a = phase + math.tau * i / N
        k = 1.0 if per_petal <= 2 and i % per_petal == 0 else 0.55 + 0.45 * abs(math.cos(a * petals / 2)) ** 0.7
        if per_petal == 2 and i % 2 == 1:
            k = 0.5
        pts.append((a, k))
    bm = bmesh.new()
    c_top = bm.verts.new(_v(center) + nm * (thick / 2))
    c_bot = bm.verts.new(_v(center) - nm * (thick / 2))
    top, bot = [], []
    for a, k in pts:
        r = radius * k
        p = _v(center) + (x * math.cos(a) + y * math.sin(a)) * r + nm * (cup * radius * k * k)
        top.append(bm.verts.new(p + nm * (thick / 2)))
        bot.append(bm.verts.new(p - nm * (thick / 2)))
    for i in range(N):
        j = (i + 1) % N
        bm.faces.new((c_top, top[i], top[j]))
        bm.faces.new((c_bot, bot[j], bot[i]))
        if walls:
            bm.faces.new((top[i], bot[i], bot[j], top[j]))
    part = Part(bm).smooth(True)
    bm.normal_update()
    for f in bm.faces:              # top fan faces +normal, bottom fan −normal (open shells: no recalc)
        if c_top in f.verts and f.normal.dot(nm) < 0:
            f.normal_flip()
        elif c_bot in f.verts and f.normal.dot(nm) > 0:
            f.normal_flip()
    if walls:
        part.recalc_normals()
    return part


def plank(size: V3, center: V3 = (0, 0, 0), bevel: float = 0.012, rot: Matrix | None = None) -> Part:
    p = M.box(size, (0, 0, 0), bevel=bevel)
    if rot is not None:
        p.transform(rot)
    return p.translate(center)


def spike(width: float, height: float, tip: float, center: V3 = (0, 0, 0), sides: int = 6, taper: float = 0.92,
          seed=0, bend: float = 0.0, square: bool = True) -> Part:
    """Sharpened stake: a ``width``-thick post (square-ish when ``square``) rising ``height``, then a ``tip``
    pyramid; small irregularities from ``seed``."""
    rnd = random.Random(scene.seed_for("spike", seed))
    n = 4 if square else sides
    rings = []
    zs = [-0.04, height * 0.5, height]
    for k, z in enumerate(zs):
        w = width / 2 * (1.0 if k == 0 else taper ** k)
        ring = []
        for i in range(n):
            a = math.tau * (i + 0.5) / n
            j = 1 + (rnd.random() - 0.5) * 0.12
            rr = w * j * (math.sqrt(2.0) if square else 1.0)       # square: corners on the diagonals
            ring.append(Vector((math.cos(a) * rr, math.sin(a) * rr, z)) + Vector((bend * z * z / max(height, 1e-3), 0, 0)))
        rings.append(ring)
    apex = Vector((width * (rnd.random() - 0.5) * 0.15 + bend * height, 0, height + tip))
    p = ring_loft(rings, cap0="ngon", cap1=None, smooth=False)
    bm = p.bm
    top = [v for v in bm.verts if abs(v.co.z - height) < 1e-6]
    top.sort(key=lambda v: math.atan2(v.co.y, v.co.x - bend * height))
    av = bm.verts.new(apex)
    for i in range(len(top)):
        bm.faces.new((top[i], top[(i + 1) % len(top)], av))
    p.recalc_normals()
    return p.auto_sharp(35).translate(center)


def rope_band(radius_xy: tuple[float, float], z: float, height: float = 0.05, thick: float = 0.018,
              center: V3 = (0, 0, 0), sides: int = 10, exp: float = 2.0) -> Part:
    """A rope lashing / hoop round a post (closed band, outward bulge)."""
    return M.belt(radius_xy[0], radius_xy[1], z, height, thick, sides=sides, exp=exp).translate(center)


# ── finishing ───────────────────────────────────────────────────────────────────────────────────────────
def _face_parts(me) -> np.ndarray:
    pid = np.zeros(len(me.polygons), np.int32)
    me.attributes["af_part"].data.foreach_get("value", pid)
    return pid


def spherise_normals(obj, spherise: dict) -> None:
    """Blend the shading normals of parts ``{pid: (centre, radii, mix)}`` toward their ellipsoid's normals."""
    me = obj.data
    nl = len(me.loops)
    cur = np.zeros(nl * 3, np.float32)
    me.corner_normals.foreach_get("vector", cur)
    cur = cur.reshape(-1, 3)
    lv = np.zeros(nl, np.int32)
    me.loops.foreach_get("vertex_index", lv)
    co = np.zeros(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    pid = _face_parts(me)
    ls = np.zeros(len(me.polygons), np.int32)
    lt = np.zeros(len(me.polygons), np.int32)
    me.polygons.foreach_get("loop_start", ls)
    me.polygons.foreach_get("loop_total", lt)
    loop_pid = np.repeat(pid, lt)
    out = cur.copy()
    for p, (c, r, mix) in spherise.items():
        sel = loop_pid == p
        if not sel.any():
            continue
        rel = co[lv[sel]] - np.array(c[:], np.float32)
        rr = np.array(r[:], np.float32)
        n = rel / (rr * rr)
        n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-9)
        b = cur[sel] * (1 - mix) + n * mix
        b /= np.maximum(np.linalg.norm(b, axis=1, keepdims=True), 1e-9)
        out[sel] = b
    me.normals_split_custom_set([tuple(v) for v in out])


def set_wind(obj, fn: Callable[[Vector, int], tuple[float, float]]) -> None:
    """``AF_Data.g/.b`` (toon vertices) and the ``UVWind`` UV channel (all loops, copied by the hull) from
    ``fn(position, part_id) -> (sway weight 0..1, phase 0..1)``."""
    me = obj.data
    pid = _face_parts(me)
    vpid = np.full(len(me.vertices), -1, np.int32)
    for p in me.polygons:
        for v in p.vertices:
            if vpid[v] < 0:
                vpid[v] = pid[p.index]
    wp = np.zeros((len(me.vertices), 2), np.float32)
    for v in me.vertices:
        wp[v.index] = fn(v.co.copy(), int(vpid[v.index]))
    afd = me.color_attributes["AF_Data"]
    dat = np.zeros(len(afd.data) * 4, np.float32)
    afd.data.foreach_get("color", dat)
    dat = dat.reshape(-1, 4)
    dat[:, 1] = wp[:, 0]
    dat[:, 2] = wp[:, 1]
    afd.data.foreach_set("color", dat.ravel())
    uv = me.uv_layers.get("UVWind") or me.uv_layers.new(name="UVWind")
    lv = np.zeros(len(me.loops), np.int32)
    me.loops.foreach_get("vertex_index", lv)
    uv.data.foreach_set("uv", wp[lv].ravel())
    me.uv_layers.active = me.uv_layers["UVMap"]
    me.uv_layers["UVMap"].active_render = True


def finish(b: M.Builder, pal: Palette, cls: str, ground_h: float | None = None, spherise: dict | None = None,
           wind: Callable | None = None, name: str | None = None):
    """``Builder.build`` → spherised normals → wind data → toon + outline materials → baked hull (``cls``
    width; ``"none"`` = no hull: ground cover)."""
    obj = b.build(name, ground_h)
    obj["af_parts"] = json.dumps([p["name"] for p in b.parts])
    if spherise:
        spherise_normals(obj, spherise)
    if wind:
        set_wind(obj, wind)
    mt = shading.toon_material(pal)
    width = outline.WIDTH_CM[cls] / 100.0
    if width > 0:
        mo = shading.outline_material(pal)
        outline.bake_hull(obj, width, mt, mo)
        for msg in outline.hull_color_report(obj, pal, cls):
            print(f"[ink] WARNING {obj.name}: {msg}")
        for r in outline.protrusion_report(obj, 2.0, [p["name"] for p in b.parts])[:6]:
            print(f"[ink] {obj.name}: {r['part']} stands {r['maxMm']:.1f} mm proud of the ink")
    else:
        obj.data.materials.clear()
        obj.data.materials.append(mt)
    obj["af_outline_class"] = cls
    obj["af_family"] = pal.family
    return obj


def tris(obj, slot: int | None = 0) -> int:
    return M.tri_count(obj, slot)


# ── ship ────────────────────────────────────────────────────────────────────────────────────────────────
def ship(obj, name: str, category: str, pal: Palette, cls: str, *, game_ids: Sequence[str] = (),
         footprint: Sequence[int] | None = None, blocking: bool | None = None, sockets: Sequence[dict] = (),
         extra: dict | None = None, foliage: bool = False, toon_slot: str | None = None,
         toon_params: dict | None = None, previews: bool = True, height: float | None = None, blob: float = 0.3,
         facing: str = "front", glows=None, turnaround: bool = True, ground: bool = True,
         close_facing: str = "se", review_extra=None) -> dict:
    """Export ``SM_<…>.fbx`` + manifest entry (merged) + the review set (ink gate recorded, not raised: the
    caller collects ``RESULTS`` and fails the run at the end)."""
    if toon_slot:
        # Interchange names each UE material slot after the FBX material: the toon material must carry the slot
        # name the manifest records, or the content build falls back to matching by index.
        mat = obj.data.materials[0].copy()
        mat.name = toon_slot
        if mat.name != toon_slot:
            raise RuntimeError(f"{name}: material name {toon_slot!r} is taken ({mat.name!r})")
        obj.data.materials[0] = mat
    fbx = export.export_static_mesh(obj, name, category, sockets)
    man = EnvManifest()
    man.set_palette(pal)
    if foliage:
        man.data["palettes"][pal.family]["parent"] = FOLIAGE_TOON
    entry = export.static_entry(obj, fbx, category, pal, cls, game_ids, sockets, footprint, blocking, extra)
    slots = entry["materialSlots"]
    if foliage:
        slots[0]["parent"] = FOLIAGE_TOON
        if len(slots) > 1:
            slots[1]["parent"] = "M_AF_Outline_Foliage"
    if toon_slot:
        slots[0]["name"] = toon_slot
    if toon_params:
        slots[0]["params"] = dict(toon_params)
    if len(slots) > 1:
        slots[1]["outlineMix"] = outline.INK_MIX[cls]
        slots[1]["outlinePx1080"] = ink_px(pal.family, cls)
    if "AF_Data" in obj.data.color_attributes and obj.data.uv_layers.get("UVWind"):
        entry["wind"] = {"vertexColor": "AF_Data.g = sway weight (0 rigid base .. 1 free tip), AF_Data.b = phase 0..1 "
                                        "(toon vertices; hull AF_Data.rgb = its normal)",
                         "uv1": "UVWind: U = sway weight, V = phase (toon + hull loops: sway both alike)",
                         "material": "M_AF_Toon_Foliage / M_AF_Outline_Foliage (AF_Wind: height-based today)"}
    result = {"asset": name, "fbx": str(fbx), "toonTris": tris(obj, 0),
              "hullTris": tris(obj, 1) if len(obj.data.materials) > 1 else 0}
    if previews:
        lo, hi = scene.world_bounds([obj])
        h = height or max(hi.z, 0.2)
        px = ink_px(pal.family, cls) or None
        rv = review.Review(name, obj, [obj], h, blob_radius=blob, game_outline_px=px, outline_class=cls,
                           glows=glows)
        rv.ground_occludes = ground
        rv.game_view(facing)
        rv.closeup(close_facing)
        if turnaround:
            rv.turnaround()
        if review_extra is not None:
            review_extra(rv)
        entry["previews"] = [str(Path(p).relative_to(rv.out.parent.parent)) for p, _ in rv.written]
        if px:
            v = rv.ink_verdict()
            entry["inkGate"] = {k: v[k] for k in ("pass", "cells", "minFracRim", "minFracDark", "medianRimPx",
                                                  "maxHullLumP95", "gate")}
            result["ink"] = {k: v[k] for k in ("pass", "minFracRim", "minFracDark", "medianRimPx", "maxHullLumP95")}
            result["inkFailures"] = v["failures"]
    man.set_asset(name, entry)
    man.save()
    RESULTS.append(result)
    ink = result.get("ink")
    print(f"[ship] {name}: toon {result['toonTris']} hull {result['hullTris']} "
          + (f"ink rim {ink['minFracRim'] * 100:.1f}% dark {ink['minFracDark'] * 100:.1f}% -> "
             f"{'PASS' if ink['pass'] else 'FAIL'}" if ink else "no ink (ground cover)"))
    return result


def summary() -> int:
    fails = [r for r in RESULTS if r.get("ink") and not r["ink"]["pass"]]
    print(f"\n[env] shipped {len(RESULTS)} assets, ink failures: {len(fails)}")
    for r in RESULTS:
        ink = r.get("ink")
        print(f"  {r['asset']:42s} toon {r['toonTris']:6d} hull {r['hullTris']:6d}  "
              + (f"rim {ink['minFracRim'] * 100:5.1f}% dark {ink['minFracDark'] * 100:5.1f}% "
                 f"{'PASS' if ink['pass'] else 'FAIL'}" if ink else "-"))
    for r in fails:
        print(f"  FAIL {r['asset']}: " + "; ".join(r.get("inkFailures", [])))
    return 1 if fails else 0


# ── warrior for scale (shipped FBX, Heroes palette read from disk) ──────────────────────────────────────
def _image(path: Path, name: str):
    img = bpy.data.images.get(name)
    if img is None:
        img = bpy.data.images.load(str(path), check_existing=False)
        img.name = name
    img.colorspace_settings.name = "Non-Color"
    img.alpha_mode = "CHANNEL_PACKED"
    return img


def toon_material_files(family: str):
    """``(toon, outline)`` preview materials of an existing palette family, read from its PNGs (no registry
    write — the family belongs to another generator)."""
    shading.ensure_groups()
    tex = paths.texture_dir()
    bc = _image(tex / f"T_AF_Palette_{family}_BC.png", f"T_AF_Palette_{family}_BC")
    pp = _image(tex / f"T_AF_Palette_{family}_P.png", f"T_AF_Palette_{family}_P")
    mats = []
    for kind in ("toon", "outline"):
        m = bpy.data.materials.new(f"AF_Files_{family}_{kind}")
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.clear()
        b = shading.NB(nt)
        t_bc, t_p = shading._palette_samplers(b, bc, pp)
        out = b.node("ShaderNodeOutputMaterial")
        em = b.node("ShaderNodeEmission")
        em.inputs["Strength"].default_value = 1.0
        if kind == "toon":
            attr = b.node("ShaderNodeAttribute", attribute_type="GEOMETRY", attribute_name="AF_Data")
            grp = b.node("ShaderNodeGroup", node_tree=bpy.data.node_groups[shading.TOON_GROUP])
            b.l.new(t_bc.outputs["Color"], grp.inputs["Base"])
            b.l.new(t_p.outputs["Color"], grp.inputs["Params"])
            g, _, _ = b.sep(attr.outputs["Vector"])
            b.l.new(g, grp.inputs["Grounding"])
            b.l.new(grp.outputs["Color"], em.inputs["Color"])
            b.l.new(em.outputs[0], out.inputs["Surface"])
        else:
            grp = b.node("ShaderNodeGroup", node_tree=bpy.data.node_groups[shading.OUTLINE_GROUP])
            b.l.new(t_bc.outputs["Color"], grp.inputs["Base"])
            b.l.new(t_p.outputs["Alpha"], grp.inputs["OutlineMix"])
            b.l.new(grp.outputs["Color"], em.inputs["Color"])
            tr = b.node("ShaderNodeBsdfTransparent")
            geo = b.node("ShaderNodeNewGeometry")
            mx = b.node("ShaderNodeMixShader")
            b.l.new(geo.outputs["Backfacing"], mx.inputs["Fac"])
            b.l.new(em.outputs[0], mx.inputs[1])
            b.l.new(tr.outputs[0], mx.inputs[2])
            b.l.new(mx.outputs[0], out.inputs["Surface"])
        mats.append(m)
    return mats[0], mats[1]


def _set_slots(m, mt, mo) -> None:
    """Replace the imported slot materials in place (``materials.clear()`` would reset the face indices and
    draw the hull as toon)."""
    mats = m.data.materials
    while len(mats) < 2:
        mats.append(None)
    mats[0], mats[1] = mt, mo


def import_warrior(at: V3 = (0, 0, 0), yaw_deg: float = 0.0, clip: str = "Idle", frame: int = 0):
    """The shipped warrior (``SK_Hero_Warrior.fbx`` + ``A_Hero_Warrior_<clip>.fbx``, sword and shield on
    ``weapon_r`` / ``weapon_l``) in the full toon look. Returns (root empty, [meshes]) or None."""
    root_dir = paths.export_root()
    sk = root_dir / "Characters" / "SK_Hero_Warrior.fbx"
    an = root_dir / "Characters" / f"A_Hero_Warrior_{clip}.fbx"
    if not sk.exists():
        return None
    mt, mo = toon_material_files("Heroes")
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(sk))
    new = [o for o in bpy.data.objects if o not in before]
    arm = next(o for o in new if o.type == "ARMATURE")
    meshes = [o for o in new if o.type == "MESH"]
    if an.exists():
        b2 = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(an))
        new2 = [o for o in bpy.data.objects if o not in b2]
        arm2 = next((o for o in new2 if o.type == "ARMATURE"), None)
        if arm2 is not None:
            for pb in arm.pose.bones:
                c = pb.constraints.new("COPY_TRANSFORMS")
                c.target, c.subtarget = arm2, pb.name
                c.target_space = c.owner_space = "POSE"
            arm2.hide_render = True
            act = arm2.animation_data.action if arm2.animation_data else None
            if act is not None:
                bpy.context.scene.frame_set(int(act.frame_range[0]) + frame)
            for o in new2:
                if o is not arm2:
                    o.hide_render = True
    for m in meshes:
        _set_slots(m, mt, mo)
        m["af_outline_width"] = outline.WIDTH_CM["hero"] / 100.0
        m["af_outline_class"] = "hero"
    rig = SimpleNamespace(obj=arm)
    from kit import rig as krig
    for wname, bone in (("SM_Hero_Warrior_Sword", "weapon_r"), ("SM_Hero_Warrior_Shield", "weapon_l")):
        f = root_dir / "Weapons" / f"{wname}.fbx"
        if not f.exists():
            continue
        b3 = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(f))
        new3 = [o for o in bpy.data.objects if o not in b3]
        wm = next(o for o in new3 if o.type == "MESH")
        for o in new3:
            if o is not wm:
                bpy.data.objects.remove(o)
        _set_slots(wm, mt, mo)
        wm["af_outline_width"] = outline.WIDTH_CM["weapon"] / 100.0
        wm["af_outline_class"] = "weapon"
        krig.attach_to_bone(wm, rig, bone)
        meshes.append(wm)
    root = bpy.data.objects.new("WarriorRoot", None)
    scene.link(root)
    arm.parent = root
    root.location = _v(at)
    root.rotation_euler = (0, 0, math.radians(yaw_deg))
    bpy.context.view_layer.update()
    return root, meshes


# ── quick look (iteration renders → scratch) ──────────────────────────────────────────────────────────
SCRATCH = Path("/tmp/claude-0/-home-user-abyssfire/b3225f99-25a3-5b78-996f-643be8685889/scratchpad/env")


def quick(objs: Sequence, tag: str, warrior: bool = True, spacing: float = 1.6, close_res=(900, 560),
          half_w: float = 0.16) -> Path:
    """Iteration render: every object in a row (x spacing) + the warrior; a close 3/4 view and the native-1080p
    W1 game crop side by side → ``SCRATCH/<tag>.png``."""
    SCRATCH.mkdir(parents=True, exist_ok=True)
    review.stage(blob_radius=0.01)
    n = len(objs)
    for i, o in enumerate(objs):
        o.location = ((i - (n - 1) / 2) * spacing, 0, 0)
    meshes = list(objs)
    if warrior:
        w = import_warrior(at=((n + 1) / 2 * spacing - spacing * 0.3, 0.0, 0), yaw_deg=-90)
        if w is not None:
            meshes += w[1]
    bpy.context.view_layer.update()
    pts = review.mesh_points(meshes)
    review.setup_render(close_res, 8)
    review.frame_points(pts, close_res, fov_h=30, pitch_ue=-30, fill=0.94)
    for m in meshes:
        if "af_outline_width" in m:
            outline.set_preview_screen(m, world=review.ink_world(outline.SCREEN_PX_1080.get(m.get("af_outline_class", "decor"), 1.2)) * 1.6)
    a = review.render_array()
    review.setup_render((1920, 1080), 8)
    cam = review.game_camera(((pts[:, 0].min() + pts[:, 0].max()) / 2, 0))
    for m in meshes:
        if "af_outline_width" in m:
            review.set_outline_px([m], outline.SCREEN_PX_1080.get(m.get("af_outline_class", "decor"), 1.2))
    c = review.project(cam, Vector((float(pts[:, 0].mean()), 0, 0.8)))
    hh = half_w * 1920 / 1080 * 0.6
    b = review.render_array((max(0, c[0] - half_w), max(0, c[1] - hh), min(1, c[0] + half_w), min(1, c[1] + hh)))
    p = SCRATCH / f"{tag}.png"
    pngio.write_png(p, a)
    pngio.write_png(SCRATCH / f"{tag}_game.png", b)
    print(f"  {p}")
    return p


def sheet(objs: Sequence, tag: str, cols: int = 4, cell=(320, 320), pitch: float = -32.0, yaw_deg: float = -115.0,
          game: bool = True, half_w: float = 0.05) -> Path:
    """Iteration sheet: each object alone, framed at a 3/4 view (top row per object) + its native-1080p W1 game
    crop (second image) → ``SCRATCH/<tag>.png``."""
    SCRATCH.mkdir(parents=True, exist_ok=True)
    review.stage(blob_radius=0.01)
    for o in objs:
        o.hide_render = True
        o.location = (0, 0, 0)
    cells = []
    for o in objs:
        o.hide_render = False
        o.rotation_euler = (0, 0, math.radians(yaw_deg + 135.0 - 135.0))
        bpy.context.view_layer.update()
        cls = o.get("af_outline_class", "decor")
        px = ink_px(o.get("af_family", ""), cls)
        pts = review.mesh_points([o])
        review.setup_render(cell, 8)
        review.frame_points(pts, cell, fov_h=26, pitch_ue=pitch, fill=0.9)
        if px:
            outline.set_preview_screen(o, world=review.ink_world(px) * 1.4)
        img = review.render_array()
        review.draw_text(img, 3, 3, o.name.replace("SM_", "")[:34])
        cells.append(img)
        if game:
            review.setup_render((1920, 1080), 8)
            cam = review.game_camera()
            if px:
                review.set_outline_px([o], px)
            hz = float(pts[:, 2].max()) * 0.45
            c = review.project(cam, Vector((0, 0, hz)))
            hw = half_w
            hh = hw * cell[1] / cell[0] * 1920 / 1080
            g = review.render_array((max(0, c[0] - hw), max(0, c[1] - hh), min(1, c[0] + hw), min(1, c[1] + hh)))
            g = g.repeat(2, 0).repeat(2, 1)[:cell[1], :cell[0]]
            review.draw_text(g, 3, 3, "1080P W1 X2")
            cells.append(g)
        o.hide_render = True
    for o in objs:
        o.hide_render = False
    p = SCRATCH / f"{tag}.png"
    pngio.write_png(p, review.grid(cells, cols * (2 if game else 1), pad=2))
    print(f"  {p}")
    return p


# ── family sheet: variants at the W1 camera, native 1080p, with the warrior ─────────────────────────────
def family_sheet(objs: Sequence, out_name: str, label: str, cols: int = 4, half_w: float = 0.075,
                 warrior: bool = True, px_map: dict | None = None, focus_z: float = 0.6,
                 facing_deg: float = -135.0) -> Path:
    """One cell per object (all built at the origin): the W1 game camera at the default distance, a fixed
    1080p window (``half_w`` of the frame width either side of the prop) — true in-game pixels, shipped ink."""
    out_dir = paths.preview_root() / "env_family"
    out_dir.mkdir(parents=True, exist_ok=True)
    review.stage(blob_radius=0.3)
    for o in objs:
        o.hide_render = True
    cells = []
    hh = half_w * 1920 / 1080

    def box(cam):
        c = review.project(cam, Vector((0, 0, focus_z)))
        return (max(0, c[0] - half_w), max(0, c[1] - hh * 0.62), min(1, c[0] + half_w), min(1, c[1] + hh * 0.62))
    review.setup_render((1920, 1080), 8)
    cam = review.game_camera()
    for o in objs:
        o.hide_render = False
        o.rotation_euler = (0, 0, math.radians(facing_deg))
        cls = o.get("af_outline_class", "decor")
        px = (px_map or {}).get(o.name, ink_px(o.get("af_family", ""), cls))
        if px:
            review.set_outline_px([o], px)
        img = review.render_array(box(cam))
        review.draw_text(img, 3, 3, o.name.replace("SM_", "")[:30])
        cells.append(img)
        o.hide_render = True
    if warrior:
        w = import_warrior()
        if w is not None:
            root, meshes = w
            for m in meshes:
                review.set_outline_px([m], outline.SCREEN_PX_1080[m.get("af_outline_class", "hero")])
            img = review.render_array(box(cam))
            review.draw_text(img, 3, 3, "WARRIOR 1.76M (SCALE)")
            cells.append(img)
            for m in meshes:
                m.hide_render = True
    for o in objs:
        o.hide_render = False
    sheet = review.grid(cells, cols, pad=2)
    lab = np.zeros((14, sheet.shape[1], 3), np.uint8)
    lab[:, :] = (24, 20, 30)
    review.draw_text(lab, 4, 4, f"{label}  W1 GAME CAMERA  NATIVE 1080P  SHIPPED INK")
    p = out_dir / out_name
    pngio.write_png_budget(p, np.vstack([lab, sheet]))
    print(f"  {p}")
    return p

"""Icon-only gem models (art review of the icons, round 3: "render gems from icon-only models"): the web's cut
stones (``src/graphics/icons/IconKit.ts`` ``facetGem`` / ``gemCut``, ``ItemIcons.ts`` ``gemSpec``) rebuilt as faceted
3-D crowns seen table-on, lit by the icon key light through the toon core.

* **Cuts** (``gemCut``, ``gemSpec``): ruby ``oval`` (10-gon), sapphire ``pear`` (11-gon, point up), emerald a
  **step cut** (the web 8-gon with cut corners, two rings of steps), topaz ``trillion`` (3 tips, bowed sides), diamond
  a **round brilliant** (octagonal table, 8 star facets, 8 kite / bezel facets, 16 upper-girdle facets). Tier 1 is the
  web's chipped stone (a corner knocked off, the girdle roughened) on the plain crown; sizes grow with the tier.
* **Facet line work**: every crown facet takes its own swatch from the web facet ramp (glint · light · base · shade
  · deep, chosen like ``facetGem``: how much the facet faces the upper-left light ± an alternating offset), and two
  neighbours never share a colour (nudged apart), so the icon's region-seam pass draws a dark line along every facet
  edge — the web's facet seams. The table is the bright facet (two-tone, with the web's white reflection wedge as a
  line-less overlay inset from its edge).
* **Accents** (``gemSpec``): the upright four-point sparkles of ``facetGem`` (table, lower right) and of ``gemSpec``
  (upper right from tier 2, lower left from tier 4); under-glows ``webglow`` from tier 3; tier 5 the eight short,
  tapered, bright rays outside the stone (``Accent('webrays')``, un-inked).

``gem(typ, tier)`` returns ``item_models.Model`` with a fixed frame (1 icon unit = 1 web unit): the web coordinates
are the icon's. ``facet_gem`` is also used by other icon models (the rare essence of ``item_models.material``).
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field

import bmesh
from mathutils import Matrix, Vector

import iconkit as K
from item_models import DIAMOND, EMERALD, RUBY, SAPPHIRE, TOPAZ, U, Model, pal
from kit import color
from kit import mesh as M
from kit.palette import Region

COLOR = {"ruby": RUBY, "sapphire": SAPPHIRE, "emerald": EMERALD, "topaz": TOPAZ, "diamond": DIAMOND}
CUT = {"ruby": "oval", "sapphire": "pear", "emerald": "emerald", "topaz": "trillion", "diamond": "round"}
# girdle radius per tier (web units; web gemSpec 22 / 27 / 31 / 32 / 33, grown ~15 % so the cut reads in a 36 px slot)
R_TIER = {1: 26.0, 2: 30.5, 3: 33.5, 4: 35.0, 5: 34.5}
GEM_PITCH = -9.0          # degrees: the top tipped back a little — a sliver of girdle under the stone, the crown still
GEM_YAW = -6.0            # reads table-on like the web (the old −32° showed the twisted pavilion as pleats)


# ── web gemCut (IconKit.ts:283-330), y down, about (0, 0) ─────────────────────────────────────────────────
def gem_cut(kind: str, r: float) -> list[tuple[float, float]]:
    pts = []
    if kind in ("round", "oval"):
        sx = 1.0 if kind == "round" else 0.8
        for i in range(10):
            a = -math.pi / 2 + i / 10 * 2 * math.pi
            pts.append((math.cos(a) * r * sx, math.sin(a) * r))
    elif kind == "emerald":
        w, h, c = r * .72, r, r * .28
        pts = [(-w + c, -h), (w - c, -h), (w, -h + c), (w, h - c), (w - c, h), (-w + c, h), (-w, h - c), (-w, -h + c)]
    elif kind == "pear":
        for i in range(11):
            a = -math.pi / 2 + i / 11 * 2 * math.pi
            top = 1 - .55 * (-math.sin(a)) ** 3 if math.sin(a) < 0 else 1.0
            pts.append((math.cos(a) * r * .78 * top, math.sin(a) * r + (-r * .12 if math.sin(a) < 0 else 0.0)))
    elif kind == "trillion":
        for i in range(3):
            a = -math.pi / 2 + i / 3 * 2 * math.pi
            b = a + math.pi / 3
            pts.append((math.cos(a) * r, math.sin(a) * r))
            pts.append((math.cos(b) * r * .62, math.sin(b) * r * .62))
    else:
        raise KeyError(kind)
    return pts


def chip(pts: list[tuple[float, float]]) -> list[tuple[float, float]]:
    """web gemSpec tier 1: knock a corner off and roughen the girdle."""
    out = [(x * .86, y * .9) if i % 3 == 1 else (x, y) for i, (x, y) in enumerate(pts)]
    del out[int(len(out) * .3)]
    return out


# ── the web facet ramp (facetGem: tone(color, {shadow .5, light .45}), deep, glint) ───────────────────────────
def _mixh(a: str, b, t: float) -> str:
    ca = color.hex_to_rgb(a)
    cb = color.hex_to_rgb(b) if isinstance(b, str) else b
    return color.rgb_to_hex(tuple(ca[i] + (cb[i] - ca[i]) * t for i in range(3)))


CLEAR = ("#FFC02A", "#D8F4FF")      # topaz, diamond: light stones keep their dark facets nearer the body colour


def ramp(hex_: str) -> list[str]:
    """[glint, light, base, shade, deep] — the five facet fills of facetGem (clear / light stones: the shade and deep
    facets pulled 30 % back toward the body colour, so a topaz stays gold-brown instead of olive and a diamond grey-blue
    instead of slate — review art 4)."""
    shade = _mixh(_mixh(hex_, (30, 20, 60), .35), (0, 0, 0), .5)
    deep = _mixh(hex_, (16, 8, 32), .62)
    if hex_.upper() in CLEAR:
        shade, deep = _mixh(shade, hex_, .3), _mixh(deep, hex_, .25)
    return [_mixh(hex_, (255, 255, 255), .72), _mixh(hex_, (255, 244, 214), .45), hex_, shade, deep]


def facing_class(mx: float, my: float, alt: float) -> int:
    """facetGem: 1 = faces the upper-left light (mx, my = facet centre from the stone centre, y down)."""
    ln = math.hypot(mx, my) or 1.0
    f = (-mx * .72 - my * .7) / ln + alt
    return 0 if f > .62 else 1 if f > .18 else 2 if f > -.3 else 3 if f > -.7 else 4


def _dist(a: str, b: str) -> float:
    ca, cb = color.hex_to_rgb(a), color.hex_to_rgb(b)
    return sum(abs(ca[i] - cb[i]) for i in range(3)) / 255.0


SEAM_MIN = .05             # IconStyle.seam_min_contrast for gems; neighbouring facets differ by ≥ NUDGE_MIN
NUDGE_MIN = .075


@dataclass
class Facets:
    """Crown facets (3-D polygons in model metres) with a ramp class each; ``colours()`` gives every facet a fill that
    differs from all its edge neighbours (so the seam pass outlines it)."""
    tones: list[str]
    polys: list[list[Vector]] = field(default_factory=list)
    cls: list[int] = field(default_factory=list)

    def add(self, pts: list[Vector], c: int) -> None:
        self.polys.append(pts)
        self.cls.append(max(0, min(4, c)))

    def colours(self, fixed: list[tuple[list[Vector], str]] = ()) -> list[str]:
        def key(v):
            return (round(v.x * 1e5), round(v.y * 1e5), round(v.z * 1e5))
        edges: dict = {}
        allp = list(self.polys) + [p for p, _ in fixed]
        for fi, pts in enumerate(allp):
            n = len(pts)
            for i in range(n):
                e = tuple(sorted((key(pts[i]), key(pts[(i + 1) % n]))))
                edges.setdefault(e, []).append(fi)
        nb: dict[int, set] = {i: set() for i in range(len(allp))}
        for fs in edges.values():
            for a in fs:
                nb[a].update(f for f in fs if f != a)
        out: list[str | None] = [None] * len(self.polys) + [h for _, h in fixed]
        for fi, c in enumerate(self.cls):
            base = self.tones[c]
            # nudge toward the body colour first (a dark facet lightens, a light one deepens), then toward the extreme
            ext = "#000000" if c >= 3 else "#FFFFFF" if c <= 1 else self.tones[1]
            body = self.tones[2] if c != 2 else self.tones[3]
            cands = [base] + [_mixh(base, t, k) for k in (.3, .5) for t in (body, ext)] + [_mixh(base, body, .75)]
            taken = [out[j] for j in nb[fi] if out[j] is not None]
            out[fi] = next((h for h in cands if all(_dist(h, t) >= NUDGE_MIN for t in taken)), cands[-1])
        return out[:len(self.polys)]


def poly_part(pts: list[Vector]) -> M.Part:
    bm = bmesh.new()
    vs = [bm.verts.new(p) for p in pts]
    bm.faces.new(vs)
    bm.normal_update()
    return M.Part(bm).flat()


def _orient_front(pts: list[Vector]) -> list[Vector]:
    """Wind a crown polygon so its normal faces the viewer (−Y)."""
    n = Vector()
    for i in range(len(pts)):
        n += pts[i].cross(pts[(i + 1) % len(pts)])
    return pts if n.y < 0 else list(reversed(pts))


def _orient_back(pts: list[Vector]) -> list[Vector]:
    n = Vector()
    for i in range(len(pts)):
        n += pts[i].cross(pts[(i + 1) % len(pts)])
    return pts if n.y > 0 else list(reversed(pts))


def _clip_convex(subject: list[tuple[float, float]], clip: list[tuple[float, float]]) -> list[tuple[float, float]]:
    """Sutherland–Hodgman: ``subject`` clipped by the convex polygon ``clip`` (any winding)."""
    area = sum(clip[i][0] * clip[(i + 1) % len(clip)][1] - clip[(i + 1) % len(clip)][0] * clip[i][1]
               for i in range(len(clip)))
    sgn = 1.0 if area > 0 else -1.0
    out = list(subject)
    for i in range(len(clip)):
        a, b = clip[i], clip[(i + 1) % len(clip)]

        def inside(p):
            return sgn * ((b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])) >= 0

        def cut(p, q):
            dx, dy = q[0] - p[0], q[1] - p[1]
            ex, ey = b[0] - a[0], b[1] - a[1]
            den = dx * ey - dy * ex
            t = ((a[0] - p[0]) * ey - (a[1] - p[1]) * ex) / den if abs(den) > 1e-12 else 0.0
            return (p[0] + dx * t, p[1] + dy * t)
        inp, out = out, []
        if not inp:
            break
        for j in range(len(inp)):
            p, q = inp[j], inp[(j + 1) % len(inp)]
            if inside(q):
                if not inside(p):
                    out.append(cut(p, q))
                out.append(q)
            elif inside(p):
                out.append(cut(p, q))
    return out


def _shrink(poly: list[tuple[float, float]], d: float) -> list[tuple[float, float]]:
    """Inset a convex polygon by ``d`` (each edge moved in, corners re-intersected)."""
    n = len(poly)
    cx = sum(p[0] for p in poly) / n
    cy = sum(p[1] for p in poly) / n
    lines = []
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        ex, ey = b[0] - a[0], b[1] - a[1]
        ln = math.hypot(ex, ey) or 1.0
        nx, ny = -ey / ln, ex / ln
        if (cx - a[0]) * nx + (cy - a[1]) * ny < 0:
            nx, ny = -nx, -ny
        lines.append(((a[0] + nx * d, a[1] + ny * d), (ex, ey)))
    out = []
    for i in range(n):
        (p1, d1), (p2, d2) = lines[i - 1], lines[i]
        den = d1[0] * d2[1] - d1[1] * d2[0]
        if abs(den) < 1e-9:
            out.append(p2)
            continue
        t = ((p2[0] - p1[0]) * d2[1] - (p2[1] - p1[1]) * d2[0]) / den
        out.append((p1[0] + d1[0] * t, p1[1] + d1[1] * t))
    return out


# ── the faceted stone ─────────────────────────────────────────────────────────────────────────────────────
@dataclass
class GemInfo:
    obj: object                  # the stone (crown, girdle, pavilion)
    overlay: object              # table shading + reflection wedge (no lines)
    cx: float
    cy: float
    max_r: float
    table_c: tuple[float, float]
    crown_u: float

    def world(self, x: float, y: float, d: float | None = None) -> Vector:
        """web point (x, y) on the table plane (or ``d`` units toward the viewer) → world point after placement."""
        dd = -self.crown_u if d is None else -d
        return self.obj.matrix_world @ Vector(((x - 48.0) * U, dd * U, (48.0 - y) * U))


def facet_gem(name: str, pal_name: str, cx: float, cy: float, outline: list[tuple[float, float]], hex_: str,
              table: float = .52, table_off=(-.06, -.08), crown: float = .30, girdle: float = 1.2,
              pavilion: float = .75, style: str = "plain", seed_alt: int = 0) -> GemInfo:
    """The web facetGem as a crown: ``outline`` (web units about (cx, cy), y down) = the girdle; the table = outline ×
    ``table`` shifted by ``table_off`` × max radius, raised ``crown`` × max radius toward the viewer; ``style``
    'plain' (one ring of facets, web), 'step' (two rings: an emerald's steps), 'brilliant' (round: star / kite /
    upper-girdle facets; ``outline`` is then ignored except for its radius)."""
    tones = ramp(hex_)
    max_r = max(math.hypot(x, y) for x, y in outline)
    h = crown * max_r
    F = Facets(tones)

    def P(x, y, d=0.0):          # stone-relative web (x, y) + depth toward the viewer (units) → model metres
        return Vector(((cx + x - 48.0) * U, -d * U, (48.0 - (cy + y)) * U))

    girdle_ring: list[tuple[float, float]]
    table_poly: list[tuple[float, float]]
    if style == "brilliant":
        r = max_r
        rt, rs = .56 * r, .80 * r
        th = [-math.pi / 2 + math.pi / 8 + i * math.pi / 4 for i in range(8)]       # table corners (flat top edge)
        T = [(rt * math.cos(a), rt * math.sin(a)) for a in th]
        S_ = [(rs * math.cos(a + math.pi / 8), rs * math.sin(a + math.pi / 8)) for a in th]
        G = [(r * math.cos(a), r * math.sin(a)) for a in th]
        Mg = [(r * math.cos(a + math.pi / 8), r * math.sin(a + math.pi / 8)) for a in th]
        zs = h * (1 - rs * math.cos(math.pi / 8) / r) / (1 - rt / r)                 # kites planar
        for i in range(8):
            j = (i + 1) % 8
            # star facet: table edge i→j, apex S_i
            c3 = [P(*T[i], h), P(*T[j], h), P(*S_[i], zs)]
            mx, my = (T[i][0] + T[j][0] + S_[i][0]) / 3, (T[i][1] + T[j][1] + S_[i][1]) / 3
            F.add(_orient_front(c3), facing_class(mx, my, .18))
            # kite (bezel) facet at table corner i: T_i, S_{i-1}, G_i, S_i
            k4 = [P(*T[i], h), P(*S_[i - 1], zs), P(*G[i], 0), P(*S_[i], zs)]
            F.add(_orient_front(k4), facing_class(G[i][0], G[i][1], .12 if i % 2 == 0 else -.12))
            # upper girdle facets under star i: (S_i, G_i, M_i) and (S_i, M_i, G_j)
            for k, tri in enumerate(([S_[i], G[i], Mg[i]], [S_[i], Mg[i], G[j]])):
                pts = [P(*tri[0], zs), P(*tri[1], 0), P(*tri[2], 0)]
                mx, my = sum(p[0] for p in tri) / 3, sum(p[1] for p in tri) / 3
                F.add(_orient_front(pts), facing_class(mx, my, .22 if k == 0 else -.22))
        girdle_ring = [p for i in range(8) for p in (G[i], Mg[i])]
        table_poly = T
    else:
        n = len(outline)
        off = (table_off[0] * max_r, table_off[1] * max_r)
        rings = [list(outline)]
        depths = [0.0]
        ks = (table,) if style == "plain" else (.5 * (1 + table) + .06, table)
        for k in ks:
            rings.append([(x * k + off[0] * (1 - k) / (1 - table), y * k + off[1] * (1 - k) / (1 - table))
                          for x, y in outline])
            depths.append(h * (1 - k) / (1 - table) if style == "step" and k != table else h)
        if style == "step":           # steps: the outer step steeper, the inner one shallower (an emerald's crown)
            depths[1] = h * .62
        for ri in range(len(rings) - 1):
            a_r, b_r = rings[ri], rings[ri + 1]
            for i in range(n):
                j = (i + 1) % n
                q = [P(*a_r[i], depths[ri]), P(*a_r[j], depths[ri]), P(*b_r[j], depths[ri + 1]),
                     P(*b_r[i], depths[ri + 1])]
                mx, my = (a_r[i][0] + a_r[j][0]) / 2, (a_r[i][1] + a_r[j][1]) / 2
                alt = (.12 if (i + seed_alt) % 2 == 0 else -.12) + (.08 if ri > 0 else 0.0)
                F.add(_orient_front(q), facing_class(mx, my, alt))
        girdle_ring = list(outline)
        table_poly = rings[-1]

    # table (the bright facet, web: light → base gradient) and the girdle band / pavilion behind the crown
    tcol = _mixh(tones[1], tones[2], .4)
    table3 = _orient_front([P(x, y, h) for x, y in table_poly])
    cols = F.colours(fixed=[(table3, tcol)])
    regs: dict[str, Region] = {}
    parts = []

    def reg_for(hx: str, s=.35, l=.08, e=0.0) -> str:       # l low: the ramp already lights the upper-left
        k = f"f{hx[1:].lower()}_{int(e * 100)}"
        regs.setdefault(k, Region(hx, s, l, e))
        return k
    for pts, hx in zip(F.polys, cols):
        parts.append((poly_part(pts), reg_for(hx)))
    parts.append((poly_part(table3), reg_for(tcol)))
    g = girdle
    n = len(girdle_ring)
    gcol = _mixh(tones[3], tones[4], .4)
    for i in range(n):
        a, b = girdle_ring[i], girdle_ring[(i + 1) % n]
        q = [P(*a, 0), P(*b, 0), P(*b, -g), P(*a, -g)]
        parts.append((poly_part(_orient_out(q, P(0, 0, -g / 2))), reg_for(gcol, s=.3, l=.2)))
    culet = P(0, 0, -g - pavilion * max_r)
    for i in range(n):
        a, b = girdle_ring[i], girdle_ring[(i + 1) % n]
        tri = [P(*a, -g), P(*b, -g), culet]
        parts.append((poly_part(_orient_out(tri, P(0, 0, -g - pavilion * max_r * .4))),
                      reg_for(tones[3] if i % 2 else tones[4], s=.3, l=.2)))
    obj = K.build_model(name, pal(pal_name), regs, parts)

    # table overlay (no lines): the web gradient's darker lower-right and the white reflection wedge, inset from the
    # table edge so the table / crown seam stays
    inset = _shrink(table_poly, 1.3)
    tcx = sum(p[0] for p in table_poly) / len(table_poly)
    tcy = sum(p[1] for p in table_poly) / len(table_poly)
    kr = max(math.hypot(x - tcx, y - tcy) for x, y in table_poly)
    ov_regs, ov_parts = {}, []
    lift = h + .06
    # lower-right: the gradient's base → shade end, beyond a diagonal through the table centre
    half = _clip_convex([(tcx + .15 * kr - 3 * kr, tcy + .15 * kr + 3 * kr), (tcx + .15 * kr + 3 * kr, tcy + .15 * kr - 3 * kr),
                         (tcx + 4 * kr, tcy + 4 * kr)], inset)
    if len(half) >= 3:
        ov_regs["tlo"] = Region(_mixh(tones[2], tones[3], .18), .35, .15)
        ov_parts.append((poly_part(_orient_front([P(x, y, lift) for x, y in half])), "tlo"))
    wedge = [(tcx - max_r, tcy - max_r * .2), (tcx + max_r * .1, tcy - max_r), (tcx + max_r * .25, tcy - max_r),
             (tcx - max_r, tcy + max_r * .05)]
    wedge = _clip_convex(wedge, inset)
    if len(wedge) >= 3:
        ov_regs["wedge"] = Region(_mixh(tcol, "#FFFFFF", .42), .2, .1)
        ov_parts.append((poly_part(_orient_front([P(x, y, lift + .03) for x, y in wedge])), "wedge"))
    ov = K.build_model(name + "_tbl", pal(pal_name), ov_regs, ov_parts, nolines=True) if ov_parts else None
    return GemInfo(obj, ov, cx, cy, max_r, (cx + tcx, cy + tcy), h)


def _orient_out(pts: list[Vector], inner: Vector) -> list[Vector]:
    n = Vector()
    for i in range(len(pts)):
        n += pts[i].cross(pts[(i + 1) % len(pts)])
    c = sum(pts, Vector()) / len(pts)
    return pts if n.dot(c - inner) >= 0 else list(reversed(pts))


def place_gem(gi: GemInfo, pitch: float = GEM_PITCH, yaw: float = GEM_YAW) -> None:
    """Tip the stone about its own centre (the web centre stays put on the icon)."""
    piv = Vector(((gi.cx - 48.0) * U, 0.0, (48.0 - gi.cy) * U))
    for o in (gi.obj, gi.overlay):
        if o is None:
            continue
        K.place(o, pitch=pitch, yaw=yaw)
        rot = o.matrix_world.to_3x3()
        o.matrix_world = Matrix.Translation(piv - rot @ piv) @ rot.to_4x4()


def gem_sparkles(gi: GemInfo, n: int) -> list[K.Accent]:
    """facetGem's own sparkles: on the table (r = .34 max R), and a smaller one lower right."""
    out = []
    tx, ty = gi.table_c
    if n > 0:
        out.append(K.Accent("sparkle", at=tuple(gi.world(tx - gi.max_r * .28, ty - gi.max_r * .22)),
                            r_u=gi.max_r * .34, alpha=.95))
    if n > 1:
        out.append(K.Accent("sparkle", at=tuple(gi.world(gi.cx + gi.max_r * .5, gi.cy + gi.max_r * .35)),
                            r_u=gi.max_r * .2, alpha=.8))
    return out


# ── the item gems (web gemSpec) ─────────────────────────────────────────────────────────────────────────────
def gem(typ: str, tier: int) -> Model:
    hex_ = COLOR[typ]
    cut = CUT[typ]
    r = R_TIER[tier]
    cx, cy = 48.0, (50.0 if cut == "pear" else 49.0)
    pts = gem_cut(cut, r)
    style = "brilliant" if (typ == "diamond" and tier >= 2) else ("step" if typ == "emerald" else "plain")
    if tier == 1:
        pts = chip(pts)
    gi = facet_gem(f"gem_{typ}_{tier}", f"Gem_{typ}", cx, cy, pts, hex_, style=style,
                   crown=.34 if style == "brilliant" else .30)
    place_gem(gi)
    objs = [o for o in (gi.obj, gi.overlay) if o is not None]
    acc: list[K.Accent] = []
    if tier >= 3:   # web: glow(c, (cx, cy), r × (1.6 | 1.35), color, .5 | .35) under the stone, drawn 'lighter' —
        # composited 'over' here, so a little stronger (.45 | .62) to keep the tier step visible in a 36 px slot
        acc.append(K.Accent("webglow", u=(cx, cy), r_u=r * (1.6 if tier >= 4 else 1.35), hex=hex_,
                            alpha=.62 if tier >= 4 else .45, under=True))
    acc += gem_sparkles(gi, 2 if tier >= 2 else 1)
    if tier >= 2:
        acc.append(K.Accent("sparkle", at=tuple(gi.world(cx + r * .62, cy - r * .7)), r_u=6.0 if tier >= 3 else 4.5,
                            alpha=.95))
    if tier >= 4:
        acc.append(K.Accent("sparkle", at=tuple(gi.world(cx - r * .85, cy + r * .6)), r_u=4.5, alpha=.85))
    if tier >= 5:   # web: 8 short strokes r + 4 … r + 11 in mix(color, white, .3), additive — here bright, tapered
        acc.append(K.Accent("webrays", u=(cx, cy), r_u=r + 3.0, hex=_mixh(hex_, "#FFFFFF", .45), alpha=.95,
                            rot=math.degrees(.2), len_u=7.0, width_u=3.0))
    st = dict(seam_min_contrast=SEAM_MIN, seam_u=.42, ink_u=2.2, auto_glow=0.0)
    return Model(objs, acc, st, dict(frame=K.Frame(world_per_unit=U, cx=0.0, cz=0.0)))


# ── the rare essence (web drawEssence: a golden pear crystal and two orbiting trillion shards) ─────────────────
def _rot(pts: list[tuple[float, float]], deg: float) -> list[tuple[float, float]]:
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return [(x * c - y * s, x * s + y * c) for x, y in pts]


ESSENCE_SCALE = 1.12       # the web composition about (48, 50), grown to the other materials' size


def essence() -> Model:
    """web drawEssence (ItemIcons.ts:1793-1800) + essenceSparkles + glow (48, 50) r 42 #FFC23A α .7: the crystal a
    faceted pear (table .5), the shards faceted trillions turned off the horizontal (review r3: flat right-pointing
    shards read as 'play' buttons), the gold under-glow strong enough to read as the rare essence next to m_dust."""
    k = ESSENCE_SCALE

    def at_(x, y):
        return (48 + (x - 48) * k, 50 + (y - 50) * k)
    gis = [facet_gem("mat_essence", "Essence", *at_(48, 50), gem_cut("pear", 26 * k), "#FFBF2E", table=.5)]
    for i, (x, y, r, rot) in enumerate(((22, 72, 7.0, -16.0), (76, 30, 5.5, 22.0))):
        gis.append(facet_gem(f"mat_essence_shard{i}", "Essence", *at_(x, y), _rot(gem_cut("trillion", r * k), rot),
                             "#FFD35A", table=.5, seed_alt=i))
    for gi in gis:
        place_gem(gi, pitch=-6.0, yaw=-6.0)
    objs = [o for gi in gis for o in (gi.obj, gi.overlay) if o is not None]
    acc = [K.Accent("webglow", u=at_(48, 50), r_u=42 * k, hex="#FFC23A", alpha=.85, under=True)]
    acc += gem_sparkles(gis[0], 2)
    acc += [K.Accent("sparkle", u=at_(36, 30), r_u=6 * k, hex="#FFF6D8"),
            K.Accent("sparkle", u=at_(66, 70), r_u=4.5 * k, hex="#FFE7A0", alpha=.9)]
    st = dict(seam_min_contrast=SEAM_MIN, seam_u=.42, auto_glow=0.0)
    return Model(objs, acc, st, dict(frame=K.Frame(world_per_unit=U, cx=0.0, cz=0.0)))

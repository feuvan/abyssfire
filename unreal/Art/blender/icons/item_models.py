"""Icon-only models for the item bases that have no game mesh (spec §9.1: "armour / accessory pieces need small
dedicated icon models"): helms, body armour, gloves, boots, belts, rings, amulets and the crafting materials.

Each look follows the web icon of its base (``src/graphics/icons/ItemIcons.ts`` helmSpec / armorSpec / glovesSpec /
bootsSpec / beltSpec / ringSpec / amuletSpec / drawOre / drawScrap / drawDust / drawEssence: the same silhouettes,
palette constants and accents), rebuilt as small 3-D models in the studio frame (``shapes``: X right, Z up, viewer
at −Y; 1 web unit = 1 cm, ``W(x, y)`` converts the web's 96-unit coordinates) and shaded by the game's toon core.

Every builder returns ``Model(objs, accents, style)``; ``items.py`` frames and renders it.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Sequence

from mathutils import Matrix, Vector

import iconkit as K
import shapes as S
from kit import mesh as M
from kit.palette import Region

U = 0.01                                  # 1 web icon unit = 1 cm


def W(x: float, y: float) -> tuple[float, float]:
    """web icon coordinates (96-unit square, y down) → model (x, z) in metres."""
    return ((x - 48.0) * U, (48.0 - y) * U)


def Wp(pts) -> list[tuple[float, float]]:
    return [W(x, y) for x, y in pts]


def Wb(ctrl, n=12, include_start=True):
    return S.bez([W(x, y) for x, y in ctrl], n, include_start)


# web ItemIcons.ts palette (:95-121)
STEEL, IRON, DARK_IRON = "#C5CED9", "#8C95A3", "#4D5361"
GOLD, BRONZE, COPPER, SILVER, PLATINUM = "#E3B44C", "#C4843E", "#CF7A45", "#DDE3EC", "#E8EEF4"
LEATHER, DARK_LEATHER, WOOD, DARK_WOOD, CLOTH = "#8A5332", "#5C3522", "#A06D3C", "#6B4428", "#C2AD7E"
BONE, DEMON, DEMON_BLACK, CRIMSON, VIOLET, CYAN = "#E9DFC2", "#6A1F2C", "#2F2432", "#FF3A52", "#A968FF", "#56DCFF"
DRAGON = "#3F9D5C"
RUBY, SAPPHIRE, EMERALD, TOPAZ, DIAMOND = "#E0283C", "#2F6CF0", "#22C060", "#FFC02A", "#D8F4FF"


def metal(h, l=.50, s=.42):
    return Region(h, s, l)


def soft(h, l=.36, s=.42):
    return Region(h, s, l)


def stone(h, e=0.0, l=.45, s=.35):
    return Region(h, s, l, e=e)


def glow(h, e=1.0):
    return Region(h, 0.0, 0.0, e=e)


def dark(h="#16101C"):
    return Region(h, .2, .05)


@dataclass
class Model:
    objs: list
    accents: list = field(default_factory=list)
    style: dict = field(default_factory=dict)
    fit: dict = field(default_factory=dict)


_PALS: dict[str, K.IconPalette] = {}


def pal(name: str) -> K.IconPalette:
    if name not in _PALS:
        _PALS[name] = K.IconPalette(name)
    return _PALS[name]


def reset_palettes() -> None:
    _PALS.clear()


# ── shared bits ─────────────────────────────────────────────────────────────────────────────────────────
def drape_strip(surf: S.Surface, pts2, half_w: float, thick: float, lift: float = 0.0, cols: int = 3,
                sink: float = 0.004, step: float = 0.012) -> M.Part | None:
    """A strap / band / lame laid on a surface along a 2-D path (x, z): its top ``thick`` over the surface (+
    ``lift``), its bottom sunk ``sink`` into it; ends capped."""
    import bmesh
    pts2 = S.resample2d(list(pts2), step) if step else list(pts2)
    n = len(pts2)
    if n < 2:
        return None
    rows_top, rows_bot = [], []
    for i in range(n):
        a = pts2[max(0, i - 1)]
        b = pts2[min(n - 1, i + 1)]
        dx, dz = b[0] - a[0], b[1] - a[1]
        L = math.hypot(dx, dz) or 1.0
        nx, nz = -dz / L, dx / L
        top, bot = [], []
        for c in range(cols):
            u = (c / (cols - 1)) * 2 - 1
            x, z = pts2[i][0] + nx * half_w * u, pts2[i][1] + nz * half_w * u
            p, nrm = surf.at(x, z)
            if p is None:
                break
            crown = 1.0 - 0.35 * u * u
            top.append(p + nrm * (lift + thick * crown))
            bot.append(p - nrm * sink)
        if len(top) == cols:
            rows_top.append(top)
            rows_bot.append(bot)
    n = len(rows_top)
    if n < 2:
        return None
    bm = bmesh.new()
    T = [[bm.verts.new(p) for p in r] for r in rows_top]
    B = [[bm.verts.new(p) for p in r] for r in rows_bot]
    for i in range(n - 1):
        for c in range(cols - 1):
            bm.faces.new((T[i][c], T[i][c + 1], T[i + 1][c + 1], T[i + 1][c]))
            bm.faces.new((B[i][c], B[i + 1][c], B[i + 1][c + 1], B[i][c + 1]))
        bm.faces.new((T[i][0], T[i + 1][0], B[i + 1][0], B[i][0]))
        bm.faces.new((T[i][-1], B[i][-1], B[i + 1][-1], T[i + 1][-1]))
    for i in (0, n - 1):
        ring = T[i] + list(reversed(B[i]))
        bm.faces.new(ring)
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return M.Part(bm).smooth(True)


def surf_tube(surf: S.Surface, pts2, r: float, lift: float = 0.0, sides: int = 6) -> M.Part | None:
    pts = surf.path(pts2, lift + r * 0.6)
    if len(pts) < 2:
        return None
    return M.tube(pts, [r] * len(pts), sides=sides)


def inside(poly, x, z) -> bool:
    n = len(poly)
    c = False
    for i in range(n):
        ax, az = poly[i]
        bx, bz = poly[(i + 1) % n]
        if (az > z) != (bz > z) and x < ax + (z - az) * (bx - ax) / (bz - az):
            c = not c
    return c


def clip_runs(poly, a, b, step=0.015, inset_poly=None):
    """Points of the segment a→b inside ``poly`` as runs (lists)."""
    L = math.hypot(b[0] - a[0], b[1] - a[1])
    n = max(2, int(L / step))
    runs, cur = [], []
    for i in range(n + 1):
        t = i / n
        p = (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
        if inside(inset_poly or poly, *p):
            cur.append(p)
        elif cur:
            runs.append(cur)
            cur = []
    if cur:
        runs.append(cur)
    return [r for r in runs if len(r) >= 2]


def stud(surf, x, z, r, h=None, lift=0.0):
    return S.dome_on(surf, x, z, r, h, lift)


# ════════════════════════════════════════════════════════════════════════════════════════════════════════
# Helmets (web helmSpec)
# ════════════════════════════════════════════════════════════════════════════════════════════════════════
def _dome(rx, ry, h, z0=0.0, cx=0.0, slouch=0.0, n=8, sides=24, exp=2.0, flat_front=0.0):
    rings = []
    for i in range(n + 1):
        t = i / n
        a = t * math.pi / 2
        z = z0 + h * math.sin(a)
        k = math.cos(a)
        if i == n:
            k = 0.06
        rings.append((z, rx * k, ry * k, cx + slouch * t * t, 0.0))
    return M.loft_z(rings, sides=sides, exp=exp, cap0="fan", cap1="fan")


def helm(kind: str) -> Model:
    P = pal("Helms")
    A = []
    parts = []
    regs = {}
    if kind == "cloth":
        regs = dict(cap=soft("#4F7A4A", .34), band=soft(CLOTH, .34), pom=soft("#D8423A", .38),
                    seam=soft("#36543A", .2))
        cap = M.loft_z([(0.0, .31, .29, 0, 0), (.10, .32, .30, .02, 0), (.20, .29, .27, .07, 0), (.28, .22, .21, .13, 0),
                        (.33, .14, .13, .19, 0), (.355, .07, .07, .24, 0), (.36, .02, .02, .26, 0)], sides=28)
        parts.append((cap, "cap"))
        band = S.torus(.315, .055, center=(0, 0, .0), axis=(0, 0, 1), rz=.075, sides=10, segs=36)
        parts.append((band, "band"))
        cs = S.Surface(cap)
        for pts in (Wb([(30, 60), (36, 30), (58, 20)], 10), Wb([(50, 62), (56, 34)], 6)):
            parts.append((drape_strip(cs, pts, .006, .006, step=.008), "seam"))
        cs.free()
        parts.append((M.ellipsoid((.06, .06, .06), (.30, -.02, .30), sides=12, rings=7), "pom"))
        objs = [K.build_model("helm_cloth", P, regs, parts)]
        for o in objs:
            K.place(o, toward=(0, -1, 0), up=(0, 0, 1), pitch=-14, yaw=12)
        return Model(objs, A, dict(seam_min_contrast=0.12))
    if kind == "leather":
        regs = dict(cap=soft(LEATHER, .38), band=soft(DARK_LEATHER, .3), stitch=soft("#D2A27A", .4),
                    rivet=metal(BRONZE, .5))
        parts.append((_dome(.31, .29, .40, z0=-.02, sides=28), "cap"))
        parts.append((M.belt(.305, .285, -.06, .085, .022, sides=32, exp=2.0), "band"))
        for sx in (-1, 1):
            flap = S.pillow(Wp([(18, 52), (30, 54), (30, 80), (22, 84), (16, 70)]), depth=.025, back=.01, wall=.012)
            flap.translate((0, -.02, 0))
            if sx > 0:
                flap.scale((-1, 1, 1))
            flap.rotate("z", sx * 28)
            flap.translate((sx * .005, 0, -.02))
            parts.append((flap, "cap"))
        # crown seam stitches (dashes over the dome)
        for k in range(9):
            a = math.radians(-8 + k * 11)
            p0 = Vector((0, -.29 * math.cos(a), -.02 + .40 * math.sin(a)))
            a2 = math.radians(-8 + k * 11 + 6)
            p1 = Vector((0, -.29 * math.cos(a2), -.02 + .40 * math.sin(a2)))
            n0 = Vector((0, -math.cos(a) * .40, math.sin(a) * .29)).normalized()
            parts.append((M.capsule(p0 + n0 * .006, p1 + n0 * .006, .008, sides=6), "stitch"))
        parts.append((M.ellipsoid((.03, .02, .03), (0, -.305, -.02), sides=10, rings=6), "rivet"))
        objs = [K.build_model("helm_leather", P, regs, parts)]
        K.place(objs[0], pitch=-10, yaw=20)
        return Model(objs, A)
    if kind == "iron":
        regs = dict(dome=metal(IRON, .45), band=metal(DARK_IRON, .32), crest=metal(STEEL, .5),
                    rivet=metal(STEEL, .55), nasal=metal(STEEL, .5))
        parts.append((_dome(.30, .29, .40, z0=-.02, sides=30), "dome"))
        parts.append((M.belt(.295, .285, -.07, .11, .024, sides=36, exp=2.0), "band"))
        crest = M.tube([Vector((0, -.29 * math.cos(math.radians(a)), -.02 + .405 * math.sin(math.radians(a))))
                        for a in range(0, 181, 15)], [.026] * 13, sides=8, exp=3)
        parts.append((crest, "crest"))
        nasal = S.pillow(Wp([(44.5, 60), (51.5, 60), (51, 86), (48, 91), (45, 86)]), depth=.012, back=.004, wall=.01)
        nasal.translate((0, -.31, .08))
        parts.append((nasal, "nasal"))
        for k in range(-3, 4):
            if k == 0:
                continue
            a = math.radians(k * 24)
            parts.append((M.ellipsoid((.016, .012, .016), (.32 * math.sin(a), -.31 * math.cos(a), -.015), sides=8, rings=5), "rivet"))
        objs = [K.build_model("helm_iron", P, regs, parts)]
        K.place(objs[0], pitch=-8, yaw=18)
        A.append(K.Accent("sparkle", u=(31, 24), r_u=4.5, alpha=.9))
        return Model(objs, A)
    if kind == "full":
        regs = dict(body=metal("#A5AEBB", .45), slit=dark(), gold=metal(GOLD, .45), rim=metal(IRON, .4))
        body = M.loft_z([(-.36, .29, .27, 0, 0, 3.0), (-.1, .30, .28, 0, 0, 3.0), (.2, .29, .27, 0, 0, 2.8),
                         (.32, .24, .23, 0, 0, 2.4), (.39, .14, .14, 0, 0, 2.2), (.41, .03, .03, 0, 0, 2.0)],
                        sides=32, exp=3.0)
        parts.append((body, "body"))
        parts.append((M.box((.48, .04, .055), (0, -.29, .06), bevel=.012), "slit"))
        gold = M.box((.06, .03, .74), (0, -.295, .0), bevel=.012)
        parts.append((gold, "gold"))
        parts.append((M.belt(.29, .27, -.37, .05, .02, sides=36, exp=3.0), "rim"))
        for (x, z) in [(.10, -.08), (.16, -.08), (.10, -.16), (.16, -.16), (.10, -.24), (.16, -.24)]:
            parts.append((M.ellipsoid((.014, .02, .014), (x, -.29 + .004 * x / .16, z), sides=8, rings=5), "slit"))
        objs = [K.build_model("helm_full", P, regs, parts)]
        K.place(objs[0], pitch=-6, yaw=20)
        A.append(K.Accent("sparkle", u=(30, 22), r_u=4.5, alpha=.9))
        return Model(objs, A)
    if kind == "plate":
        regs = dict(body=metal(STEEL, .5), visor=metal("#AAB4C2", .45), slit=dark(), gold=metal(GOLD, .45),
                    plume=soft("#D8323A", .36), plume2=soft("#A01E2A", .25))
        skull = M.ellipsoid((.27, .29, .30), (0, .02, .06), sides=32, rings=16)
        skull.deform(lambda co: Vector((co.x, co.y, max(co.z, -.20))))
        parts.append((skull, "body"))
        neck = M.loft_z([(-.34, .24, .25, 0, .02), (-.22, .26, .27, 0, .02), (-.12, .265, .275, 0, .02)], sides=32)
        parts.append((neck, "body"))
        # visor: a pointed snout plate over the face (pillow seen from the front, swept back at the sides)
        vis = S.pillow(Wp([(22, 40), (48, 34), (74, 40), (78, 58), (60, 72), (48, 80), (36, 72), (18, 58)]),
                       depth=.07, back=.02, wall=.02, edge=.5, dome=.5)
        vis.deform(lambda co: Vector((co.x, co.y + 1.6 * co.x * co.x, co.z)))
        vis.translate((0, -.24, -.03))
        parts.append((vis, "visor"))
        sl = S.pillow(Wp([(26, 50), (48, 45), (70, 50), (70, 53.5), (48, 49), (26, 53.5)]), depth=.01, back=.004,
                      wall=.006)
        sl.deform(lambda co: Vector((co.x, co.y + 1.6 * co.x * co.x, co.z)))
        sl.translate((0, -.335, -.03))
        parts.append((sl, "slit"))
        for sx in (-1, 1):
            parts.append((M.ellipsoid((.03, .025, .03), (sx * .25, -.09, .0), sides=10, rings=6), "gold"))
        parts.append((M.belt(.245, .255, -.36, .05, .02, sides=36), "gold"))
        plume_pts = [Vector(p) for p in [(.0, .04, .33), (-.05, .08, .45), (-.16, .12, .52), (-.30, .16, .50),
                                         (-.42, .18, .40)]]
        parts.append((M.tube(plume_pts, [.05, .08, .085, .07, .03], sides=10), "plume"))
        plume2 = [Vector(p) for p in [(-.08, .04, .47), (-.20, .08, .53), (-.33, .12, .49)]]
        parts.append((M.tube(plume2, [.01, .012, .008], sides=6), "plume2"))
        objs = [K.build_model("helm_plate", P, regs, parts)]
        K.place(objs[0], pitch=-6, yaw=24)
        A.append(K.Accent("sparkle", u=(32, 26), r_u=4.5, alpha=.9))
        return Model(objs, A)
    if kind == "demon":
        regs = dict(body=Region("#4A2436", .45, .35), horn=soft("#D8C8A8", .35), hornband=soft("#A8987A", .2),
                    crest=Region("#8A2A3A", .4, .35), eye=glow(CRIMSON, 1.0), socket=dark("#10080C"))
        body = M.loft_z([(-.36, .25, .26, 0, 0), (-.14, .29, .29, 0, 0), (.10, .29, .29, 0, 0), (.24, .25, .25, 0, .01),
                         (.33, .17, .17, 0, .01), (.38, .06, .06, 0, .01), (.385, .02, .02, 0, .01)], sides=32)
        # jaw points: pull the bottom front into fangs
        def fangs(co):
            if co.z < -.25 and co.y < 0:
                k = (-.25 - co.z) / .11
                w = abs(math.sin(co.x / .25 * math.pi * 1.5))
                return Vector((co.x, co.y, co.z - k * .05 * w))
            return co
        body.deform(fangs)
        parts.append((body, "body"))
        for sx in (-1, 1):
            h = M.horn(.085, .50, bend_deg=110, sides=12, segments=12)
            h.rotate("z", -90 * sx)          # +Y → ±X, bending up (+Z)
            h.transform(Matrix.Rotation(math.radians(12 * sx), 4, "Y"))
            h.translate((sx * .22, .03, .16))
            parts.append((h, "horn"))
            for k in (0.2, 0.38, 0.55):
                a = math.radians(110 * k)
                R = .50 / math.radians(110)
                cx, cz = sx * (.22 + R * math.sin(a)), .16 + R * (1 - math.cos(a))
                rr = .085 * (1 - k * .85) + .012
                ring = S.torus(rr, .01, center=(0, 0, 0), axis=(math.cos(a) * sx, 0, math.sin(a)), sides=6, segs=16)
                ring.translate((cx, .03, cz))
                parts.append((ring, "hornband"))
        parts.append((S.slab(Wp([(48, 4), (54, 22), (48, 28), (42, 22)]), .05, y0=-.25), "crest"))
        for sx in (-1, 1):
            sock = S.slab([(sx * .045, .10), (sx * .21, .14), (sx * .21, .08), (sx * .05, .05)], .03, y0=-.282)
            parts.append((sock, "socket"))
            parts.append((M.ellipsoid((.042, .02, .022), (sx * .12, -.30, .098), sides=10, rings=5), "eye"))
        objs = [K.build_model("helm_demon", P, regs, parts)]
        K.place(objs[0], pitch=-6, yaw=10)
        return Model(objs, A, dict(auto_glow=0.55))
    if kind == "dragon":
        regs = dict(body=Region(DRAGON, .42, .40), scale=Region("#2E7A46", .35, .2), horn=soft("#E8D8A0", .4),
                    gold=metal(GOLD, .45), slit=dark("#10140C"), ruby=stone(RUBY, .35))
        body = M.loft_z([(-.30, .31, .31, 0, .01), (-.22, .29, .29, 0, 0), (-.05, .30, .30, 0, 0), (.12, .28, .28, 0, .01),
                         (.24, .22, .22, 0, .01), (.31, .13, .13, 0, .01), (.34, .05, .05, 0, .01),
                         (.345, .01, .01, 0, .01)], sides=32)
        parts.append((body, "body"))
        surf = S.Surface(body)
        nasal = S.pillow(Wp([(44, 50), (52, 50), (51.5, 78), (48, 86), (44.5, 78)]), depth=.02, back=.006, wall=.012)
        p0, _ = surf.at(0.0, -.15)
        nasal.translate((0, (p0.y if p0 is not None else -.3) - .006, 0))
        parts.append((nasal, "gold"))
        for r in range(4):
            for k in range(-4, 5):
                x = k * .065 + (r % 2) * .032
                z = .30 - r * .085
                pts = [(x + .028 * math.cos(a), z - .022 * math.sin(a)) for a in [i * math.pi / 6 for i in range(1, 6)]]
                t = surf_tube(surf, pts, .006)
                if t is not None:
                    parts.append((t, "scale"))
        brow = Wb([(18, 52), (48, 40), (78, 52)], 16) + Wb([(78, 60), (48, 48), (18, 60)], 16)
        bt = drape_strip(surf, Wb([(16, 56), (48, 44), (80, 56)], 18), .035, .018)
        if bt is not None:
            parts.append((bt, "gold"))
        slit = drape_strip(surf, Wb([(26, 64), (48, 57), (70, 64)], 14), .012, .006)
        if slit is not None:
            parts.append((slit, "slit"))
        surf.free()
        parts.append((S.slab(Wp([(40, 16), (46, 2), (52, 14), (58, 4), (60, 18)]), .03, y0=-.12), "gold"))
        for sx in (-1, 1):
            h = M.horn(.07, .46, bend_deg=55, sides=12, segments=12)
            S.align(h, (sx * .75, .55, .10), (sx * .2, -.2, 1.0), at=(sx * .21, .02, .16))
            parts.append((h, "horn"))
        parts.append((S.brilliant(.04, sides=8, axis=(0, -1, 0), center=(0, -.31, .02)), "ruby"))
        objs = [K.build_model("helm_dragon", P, regs, parts)]
        K.place(objs[0], pitch=-6, yaw=12)
        return Model(objs, A)
    raise KeyError(kind)


# ════════════════════════════════════════════════════════════════════════════════════════════════════════
# Body armour (web armorSpec / torsoPath)
# ════════════════════════════════════════════════════════════════════════════════════════════════════════
def torso2d():
    return S.chain2d(Wb([(34, 12), (48, 22), (62, 12)], 10), [W(76, 18)], Wb([(76, 18), (80, 28), (78, 38)], 6),
                     Wb([(78, 38), (70, 42), (70, 50)], 6), [W(72, 86)], Wb([(72, 86), (48, 92), (24, 86)], 10),
                     [W(26, 50)], Wb([(26, 50), (26, 42), (18, 38)], 6), Wb([(18, 38), (16, 28), (20, 18)], 6))


def armor(kind: str) -> Model:
    P = pal("Armour")
    main = {"quilted": CLOTH, "leather": LEATHER, "chain": "#9AA2B0", "scale": "#B58A4A", "plate": "#B4BECB",
            "heavy": "#AEB8C6", "demon": "#5A2230", "dragon": DRAGON}[kind]
    light = .42 if kind in ("plate", "heavy") else .38
    regs = dict(body=Region(main, .42, light))
    outline = torso2d()
    deep = kind in ("plate", "heavy", "demon")
    body = S.pillow(outline, depth=.15 if deep else .11, back=.03, wall=.03, edge=.40 if deep else .32,
                    dome=.45 if deep else .35, tilt=(-.01, .01))
    parts = [(body, "body")]
    surf = S.Surface(body)
    A = []
    style = {}
    inset = S.offset2d(outline, -.035)
    if kind == "quilted":
        regs.update(seam=Region("#8C7650", .3, .1), trim=soft("#8A6A3A", .3), belt=soft(DARK_LEATHER, .3),
                    buckle=metal(BRONZE, .5))
        for k in range(-60, 121, 11):
            for a, b in (((k, 0), (k + 96, 96)), ((k + 96, 0), (k, 96))):
                for run in clip_runs(inset, W(*a), W(*b), .01):
                    t = surf_tube(surf, run, .0045)
                    if t is not None:
                        parts.append((t, "seam"))
        neck = drape_strip(surf, Wb([(33, 13), (48, 25), (63, 13)], 12), .02, .02)
        parts.append((neck, "trim"))
        parts.append((drape_strip(surf, [W(24.5, 74), W(48, 75.5), W(71.5, 74)], .04, .02), "belt"))
        parts.append((drape_strip(surf, [W(45, 74.6), W(51, 74.6)], .05, .03), "buckle"))
    elif kind == "leather":
        regs.update(strap=soft(DARK_LEATHER, .3), buckle=metal(BRONZE, .5), pad=soft("#7C4A2C", .36),
                    stitch=soft("#C89A70", .35))
        parts.append((drape_strip(surf, S.bez([W(22, 20), W(48, 50), W(70, 80)], 14), .036, .02), "strap"))
        parts.append((drape_strip(surf, [W(24, 70), W(48, 71.5), W(72, 70)], .042, .022), "strap"))
        parts.append((drape_strip(surf, [W(44, 70.8), W(52, 70.8)], .06, .035), "buckle"))
        for x, rot in ((20, 18), (76, -18)):
            pad = S.pillow(S.circle2d(.08, 20, rz=.11), depth=.03, back=.01, wall=.012)
            pad.rotate("y", -rot)
            p, _ = surf.at(*W(x, 26))
            pad.translate((W(x, 26)[0], (p.y if p is not None else -.1) - .005, W(x, 26)[1]))
            parts.append((pad, "pad"))
        for run in clip_runs(inset, W(48, 22), W(48, 88), .012):
            dashes = [run[i:i + 2] for i in range(0, len(run) - 1, 3)]
            for d in dashes:
                t = surf_tube(surf, d, .005)
                if t is not None:
                    parts.append((t, "stitch"))
    elif kind == "chain":
        regs.update(ring=Region("#6E7686", .35, .25), trim=soft(LEATHER, .35))
        y = 16
        while y < 92:
            off = ((y // 5) % 2) * 3
            x = 14 + off
            while x < 84:
                arc = [W(x + 2.6 * math.cos(a), y + 2.6 * math.sin(a)) for a in
                       [math.pi * (0.15 + 0.7 * i / 5) for i in range(6)]]
                if all(inside(inset, *p) for p in arc):
                    t = surf_tube(surf, arc, .0055, sides=5)
                    if t is not None:
                        parts.append((t, "ring"))
                x += 6
            y += 5
        parts.append((drape_strip(surf, Wb([(32, 13), (48, 27), (64, 13)], 12), .022, .02), "trim"))
        parts.append((drape_strip(surf, Wb([(25, 84), (48, 90), (71, 84)], 12), .025, .02), "trim"))
    elif kind in ("scale", "dragon"):
        regs.update(scale=Region(main, .42, .45), scale_dk=Region(main, .5, .3))
        rows = list(range(22, 92, 7))
        for ri, yy in enumerate(rows):
            off = (ri % 2) * 4.5
            x = 14 + off
            while x < 86:
                c = W(x, yy)
                if inside(inset, *c) and inside(inset, *W(x, yy + 3)):
                    p, n = surf.at(*c)
                    if p is not None:
                        sc = S.pillow(S.chain2d(S.circle2d(.05, 14, phase=180)[0:8], [(.0, -.055)]), depth=.012,
                                      back=.003, wall=.005, edge=.6)
                        # upper scales in front at their lower edge
                        sc.rotate("x", -18)
                        sc.translate((c[0], p.y - .008 - ri * .0012, c[1] + .015))
                        parts.append((sc, "scale" if (ri + int(x)) % 3 else "scale_dk"))
                x += 9
        if kind == "scale":
            regs.update(belt=soft(DARK_LEATHER, .3), stud=metal(GOLD, .5))
            parts.append((drape_strip(surf, [W(24, 67.5), W(48, 69), W(72, 67.5)], .036, .03), "belt"))
            parts.append((S.dome_on(surf, *W(48, 68), .03, .02, lift=.03), "stud"))
        else:
            regs.update(gold=metal(GOLD, .45), ruby=stone(RUBY, .3))
            gorget = S.pillow(Wp([(34, 12), (41, 18), (48, 20), (55, 18), (62, 12), (58, 36), (48, 42), (38, 36)]),
                              depth=.03, back=.01, wall=.012)
            p, _ = surf.at(*W(48, 26))
            gorget.translate((0, p.y - .02, 0))
            parts.append((gorget, "gold"))
            parts.append((S.brilliant(.05, 8, axis=(0, -1, 0), center=(*[W(48, 30)[0]], p.y - .06, W(48, 30)[1])), "ruby"))
            for x, rot in ((20, .35), (76, -.35)):
                pd = M.ellipsoid((.13, .08, .10), (0, 0, 0), sides=20, rings=10)
                pd.rotate("y", math.degrees(rot))
                c = W(x, 24)
                pd.translate((c[0], -.06, c[1]))
                parts.append((pd, "gold"))
            A.append(K.Accent("sparkle", u=(16, 18), r_u=4.5, alpha=.9))
    elif kind in ("plate", "heavy"):
        trim = GOLD if kind == "heavy" else IRON
        regs.update(trim=metal(trim, .45), ridge=metal("#E8EEF4", .6), lame0=metal(main, .45),
                    lame1=metal(_mix(main, "#606878", .3), .42), lame2=metal(_mix(main, "#606878", .4), .4),
                    paul=metal(main, .5))
        parts.append((drape_strip(surf, [W(48, 22), W(48, 60)], .012, .012), "ridge"))
        for i in range(3):
            y = 64 + i * 8
            parts.append((drape_strip(surf, Wb([(25, y + 3), (48, y + 8), (71, y + 3)], 12), .042, .016 + i * .004,
                                      lift=.004 * i), f"lame{i}"))
        pr = .14 if kind == "heavy" else .125
        for x, rot in ((20, .35), (76, -.35)):
            c = W(x, 24)
            pd = M.ellipsoid((pr, pr * .7, pr * .82), (0, 0, 0), sides=24, rings=12)
            pd.rotate("y", math.degrees(rot))
            pd.translate((c[0], -.07, c[1]))
            ps = S.Surface(pd)
            arc = [(pr * .80 * math.cos(a), pr * .50 * math.sin(a) - pr * .12)
                   for a in [math.pi * (1.12 + .76 * i / 16) for i in range(17)]]
            arc = S.transform2d(arc, rot=-math.degrees(rot), at=c)
            tr = drape_strip(ps, arc, .02, .014, step=.01)
            ps.free()
            parts.append((pd, "paul"))
            parts.append((tr, "trim"))
        parts.append((drape_strip(surf, Wb([(34, 13), (48, 25), (62, 13)], 12), .025, .022), "trim"))
        if kind == "heavy":
            regs.update(gem=stone(SAPPHIRE, .3))
            p, _ = surf.at(*W(48, 40))
            parts.append((S.brilliant(.042, 8, axis=(0, -1, 0), center=(0, p.y - .022, W(48, 40)[1])), "gem"))
        A.append(K.Accent("sparkle", u=(15, 17), r_u=4.5, alpha=.9))
    elif kind == "demon":
        regs.update(bone=soft(BONE, .4), paul=Region("#3A1C28", .42, .35), gem=glow(CRIMSON, .9))
        for i in range(3):
            y = 36 + i * 10
            pts = [W(30 + t * 36, y - math.sin(t * math.pi) * 5) for t in [k / 10 for k in range(11)]]
            parts.append((drape_strip(surf, pts, .022, .02), "bone"))
        for x, s in ((20, -1), (76, 1)):
            c = W(x, 24)
            pd = M.ellipsoid((.13, .08, .10), (0, 0, 0), sides=20, rings=10)
            pd.rotate("y", 20 * s)
            pd.translate((c[0], -.06, c[1]))
            parts.append((pd, "paul"))
            for k in (-1, 0, 1):
                h = M.horn(.025, .14 if k == 0 else .11, bend_deg=25 * s, sides=8, segments=5)
                h.rotate("x", 90)                        # +Y → +Z (up)
                h.rotate("y", -k * 22 - 8 * s)
                h.translate((c[0] + k * .06, -.07, c[1] + .06 - abs(k) * .02))
                parts.append((h, "bone"))
        p, _ = surf.at(*W(48, 24))
        parts.append((M.ellipsoid((.04, .025, .04), (0, p.y - .01, W(48, 24)[1]), sides=12, rings=7), "gem"))
        style = dict(auto_glow=0.5)
    surf.free()
    obj = K.build_model(f"armor_{kind}", P, regs, parts)
    K.place(obj, yaw=-16 if deep else 8)
    return Model([obj], A, style)


def _mix(a: str, b: str, t: float) -> str:
    from kit import color
    return color.rgb_to_hex(color.mix(color.hex_to_rgb(a), color.hex_to_rgb(b), t))


# ════════════════════════════════════════════════════════════════════════════════════════════════════════
# Gloves (web glovesSpec)
# ════════════════════════════════════════════════════════════════════════════════════════════════════════
def gloves(kind: str) -> Model:
    P = pal("Hands")
    main = {"leather": LEATHER, "chain": "#9AA2B0", "iron": IRON, "plate": STEEL, "demon": "#4A2436"}[kind]
    metal_k = kind in ("iron", "plate", "demon")
    fing = LEATHER if kind == "chain" else main
    cuff_c = {"leather": DARK_LEATHER, "chain": LEATHER, "demon": "#3A1C28"}.get(kind, main)
    regs = dict(finger=Region(fing, .42, .42 if not metal_k else .45), back=Region(main, .42, .45),
                cuff=Region(cuff_c, .42, .45), band=Region(GOLD if kind == "plate" else _mix(cuff_c, "#000000", .15),
                                                          .42, .45),
                lame=Region(_mix(fing, "#1A1428", .45), .3, .15), rivet=metal(GOLD if kind == "plate" else STEEL, .55),
                bone=soft(BONE, .4), ring=Region("#6E7686", .35, .25), stitch=soft("#C89A70", .35),
                gem=stone("#A02838", .25))
    parts = []
    fingers = [((62, 44), (72, 16)), ((55, 42), (60, 8)), ((47, 42), (47, 10)), ((39, 44), (35, 16))]
    for i, (a, b) in enumerate(fingers):
        A3 = Vector((*W(*a),)).to_3d()
        pa = Vector((W(*a)[0], .02 + i * .004, W(*a)[1]))
        pb = Vector((W(*b)[0], .0 + i * .004, W(*b)[1]))
        parts.append((M.capsule(pa, pb, .052, .044, sides=12, cap_rings=3, mid=2), "finger"))
        if metal_k:
            for k in (0.38, 0.66):
                c = pa + (pb - pa) * k
                ring = S.torus(.049 - k * .008, .009, center=c, axis=(pb - pa), sides=6, segs=18)
                parts.append((ring, "lame"))
        if kind == "demon":
            tip = pb + (pb - pa).normalized() * .03
            claw = M.horn(.026, .10, bend_deg=-35, sides=8, segments=5)
            S.align(claw, (pb - pa), (0, -1, 0), at=tip)
            parts.append((claw, "bone"))
    th_a = Vector((W(34, 60)[0], -.02, W(34, 60)[1]))
    th_b = Vector((W(18, 42)[0], -.04, W(18, 42)[1]))
    parts.append((M.capsule(th_a, th_b, .062, .048, sides=12, cap_rings=3, mid=2), "finger"))
    if metal_k:
        parts.append((S.torus(.055, .01, center=th_a + (th_b - th_a) * .55, axis=th_b - th_a, sides=6, segs=18), "lame"))
    back = S.pillow(Wp([(30, 40), (50, 36), (68, 40), (70, 58), (64, 70), (36, 70), (28, 58)]), depth=.06, back=.02,
                    wall=.03, edge=.45, dome=.3)
    back.translate((0, -.03, 0))
    parts.append((back, "back"))
    surf = S.Surface(back)
    if kind == "chain":
        y = 38
        while y < 72:
            x = 28 + ((y // 5) % 2) * 3
            while x < 72:
                arc = [W(x + 2.5 * math.cos(a), y + 2.5 * math.sin(a)) for a in
                       [math.pi * (0.15 + 0.7 * i / 5) for i in range(6)]]
                t = surf_tube(surf, arc, .0055, sides=5)
                if t is not None and all(inside(Wp([(31, 42), (50, 38), (66, 42), (67, 57), (62, 67), (38, 67), (31, 57)]), *p) for p in arc):
                    parts.append((t, "ring"))
                x += 6
            y += 5
    elif metal_k:
        for yy in (48, 56, 64):
            st = drape_strip(surf, [W(28, yy), W(72, yy - 2)], .008, .006)
            if st is not None:
                parts.append((st, "lame"))
    else:
        for run in [Wb([(32, 46), (50, 42), (66, 46)], 12)]:
            for k in range(0, len(run) - 1, 2):
                t = surf_tube(surf, run[k:k + 2], .005)
                if t is not None:
                    parts.append((t, "stitch"))
    if kind == "demon":
        parts.append((stud(surf, *W(49, 52), .034, .022), "gem"))
    surf.free()
    flare = 5 if kind in ("plate", "demon") else 1
    cuff = S.pillow(Wp([(34 - flare, 68), (68 + flare, 68), (66 + flare * 1.6, 90), (32 - flare * 1.6, 90)]),
                    depth=.05, back=.03, wall=.03, edge=.5, dome=.2)
    cuff.translate((0, -.035, 0))
    parts.append((cuff, "cuff"))
    band = S.pillow(S.rrect2d(.40 + .02 * flare, .07, .03, 3, *W(51, 69.5)), depth=.03, back=.03, wall=.03)
    band.translate((0, -.05, 0))
    parts.append((band, "band"))
    cs = S.Surface(cuff)
    if kind in ("iron", "plate"):
        for x in (38, 50, 62):
            parts.append((stud(cs, *W(x, 80), .018, .012), "rivet"))
    if kind == "demon":
        for x in (34, 46, 58, 70):
            sp = M.horn(.02, .07, bend_deg=0, sides=6, segments=3)
            S.align(sp, (0, -.2, -1), (0, -1, 0), at=(W(x, 89)[0], -.06, W(x, 89)[1]))
            parts.append((sp, "bone"))
    cs.free()
    obj = K.build_model(f"gloves_{kind}", P, regs, parts)
    K.place(obj, roll=-12.6, yaw=-10)
    A = [K.Accent("sparkle", u=(30, 30), r_u=4, alpha=.85)] if metal_k and kind != "demon" else []
    return Model([obj], A, dict(seam_min_contrast=0.12))


# ════════════════════════════════════════════════════════════════════════════════════════════════════════
# Boots (web bootsSpec / bootPath)
# ════════════════════════════════════════════════════════════════════════════════════════════════════════
def boot2d():
    return S.chain2d([W(26, 10), W(68, 10), W(66, 58)], Wb([(66, 58), (70, 64), (82, 68)], 6),
                     Wb([(82, 68), (92, 72), (90, 84)], 6), [W(28, 84)], Wb([(28, 84), (26, 60), (30, 40)], 8))


def boots(kind: str) -> Model:
    P = pal("Feet")
    main = {"leather": LEATHER, "chain": "#9AA2B0", "iron": LEATHER, "plate": STEEL, "demon": "#4A2436"}[kind]
    regs = dict(boot=Region(main, .42, .42), back=Region(_mix(main, "#201830", .35), .42, .3),
                sole=Region("#3A2A24", .4, .2), cuff=Region(_mix(LEATHER, "#FFFFFF", .12), .42, .38),
                lace=soft("#E8D8B0", .35), ring=Region("#6E7686", .35, .25), cband=soft(LEATHER, .35),
                greave=metal(STEEL, .5), ridge=metal("#8C96A6", .4), rivet=metal(BRONZE, .5),
                gold=metal(GOLD, .45), cap=metal(STEEL, .5), lame=Region(_mix(main, "#1A1428", .45), .3, .15),
                dcuff=Region("#3A1C28", .42, .35), bone=soft(BONE, .4))
    parts = []
    outline = boot2d()
    for back_boot in (True, False):
        b = S.pillow(outline, depth=.10, back=.04, wall=.05, edge=.4, dome=.3)
        if back_boot:
            b.translate((-.12, .09, .06))
            parts.append((b, "back"))
            sole = S.pillow(S.chain2d([W(28, 84), W(88, 84)], Wb([(88, 84), (92, 84), (91, 90)], 4), [W(28, 90)]),
                            depth=.06, back=.04, wall=.04)
            sole.translate((-.12, .09, .06))
            parts.append((sole, "sole"))
            continue
        parts.append((b, "boot"))
        surf = S.Surface(b)
        sole = S.pillow(S.chain2d([W(27, 83), W(88, 83)], Wb([(88, 83), (93, 84), (92, 91)], 4), [W(27, 91)]),
                        depth=.115, back=.05, wall=.05, edge=.5)
        parts.append((sole, "sole"))
        if kind == "chain":
            y = 20
            while y < 76:
                x = 30 + ((y // 5) % 2) * 3
                while x < 90:
                    arc = [W(x + 2.5 * math.cos(a), y + 2.5 * math.sin(a)) for a in
                           [math.pi * (0.15 + 0.7 * i / 5) for i in range(6)]]
                    if all(inside(S.offset2d(outline, -.03), *p) for p in arc):
                        t = surf_tube(surf, arc, .0055, sides=5)
                        if t is not None:
                            parts.append((t, "ring"))
                    x += 6
                y += 5
            parts.append((drape_strip(surf, [W(27, 15), W(67, 15)], .04, .025), "cband"))
        if kind in ("plate", "demon"):
            for y in (30, 42, 54):
                parts.append((drape_strip(surf, [W(30, y), W(66, y + 1)], .008, .006), "lame"))
            for x in (66, 74, 82):
                parts.append((drape_strip(surf, [W(x, 66), W(x - 2, 82)], .008, .006), "lame"))
        if kind == "leather":
            cuff = S.pillow(S.chain2d([W(24, 9), W(70, 9), W(68, 26)], Wb([(68, 26), (46, 30), (26, 26)], 8)),
                            depth=.03, back=.01, wall=.02)
            p, _ = surf.at(*W(46, 18))
            cuff.translate((0, p.y - .01, 0))
            parts.append((cuff, "cuff"))
            for y in range(36, 62, 7):
                parts.append((surf_tube(surf, [W(57, y), W(66, y + 3)], .007), "lace"))
        elif kind == "iron":
            g = S.pillow(S.chain2d(Wb([(38, 8), (52, 4), (68, 8)], 6), [W(68, 58)], Wb([(68, 58), (58, 70), (44, 64)], 6)),
                         depth=.05, back=.01, wall=.02, edge=.5, dome=.5)
            p, _ = surf.at(*W(54, 35))
            g.translate((0, p.y - .015, 0))
            parts.append((g, "greave"))
            gs = S.Surface(g)
            parts.append((drape_strip(gs, [W(55, 10), W(55, 62)], .007, .008), "ridge"))
            for y in (18, 48):
                parts.append((stud(gs, *W(62, y), .018, .012), "rivet"))
            gs.free()
        elif kind == "plate":
            parts.append((drape_strip(surf, [W(26, 14), W(70, 14)], .045, .025), "gold"))
            parts.append((stud(surf, *W(44, 60), .085, .045), "cap"))
        elif kind == "demon":
            parts.append((drape_strip(surf, [W(26, 14), W(70, 14)], .045, .025), "dcuff"))
            for (x, y), d in (((28, 30), -1), ((28, 46), -1), ((70, 18), 1)):
                sp = M.horn(.028, .11, bend_deg=20 * d, sides=8, segments=4)
                S.align(sp, (d, .0, .35), (0, -1, 0), at=(W(x, y)[0], -.05, W(x, y)[1]))
                parts.append((sp, "bone"))
            sp = M.horn(.03, .10, bend_deg=0, sides=8, segments=3)
            S.align(sp, (1, 0, -.4), (0, -1, 0), at=(W(88, 80)[0], -.03, W(88, 80)[1]))
            parts.append((sp, "bone"))
        surf.free()
    obj = K.build_model(f"boots_{kind}", P, regs, parts)
    K.place(obj, yaw=-8)
    A = [K.Accent("sparkle", u=(42, 16), r_u=4, alpha=.85)] if kind in ("plate", "iron") else []
    return Model([obj], A, dict(seam_min_contrast=0.12))


# ════════════════════════════════════════════════════════════════════════════════════════════════════════
# Belts (web beltSpec)
# ════════════════════════════════════════════════════════════════════════════════════════════════════════
def belts(kind: str) -> Model:
    P = pal("Belts")
    wide = 1.35 if kind in ("heavy", "war", "plated") else 1.0
    leather = {"plated": "#4A3A34", "war": "#6A2A24"}.get(kind, LEATHER)
    regs = dict(band=Region(leather, .42, .35), stitch=soft(_mix(leather, "#FFFFFF", .22), .25),
                buckle=metal(STEEL if kind == "plated" else GOLD, .5), tongue=metal(_mix(GOLD, "#FFFFFF", .3), .5),
                pouch=Region(_mix(leather, "#FFFFFF", .1), .42, .38), flap=Region(_mix(leather, "#000000", .2), .42, .3),
                stud=metal(STEEL, .55), plate=metal(STEEL, .5))
    parts = []
    bw = .11 * wide
    band = M.belt(.34, .34, -bw / 2, bw, .022, sides=48, exp=2.0)
    parts.append((band, "band"))
    # stitching dashes along both edges
    for zz in (bw / 2 - .016, -bw / 2 + .016):
        for k in range(48):
            if k % 2 or not (26 <= k <= 46):           # front half only (the back half shows its lining)
                continue
            a0, a1 = 2 * math.pi * k / 48, 2 * math.pi * (k + .55) / 48
            p0 = Vector((.364 * math.cos(a0), .364 * math.sin(a0), zz))
            p1 = Vector((.364 * math.cos(a1), .364 * math.sin(a1), zz))
            parts.append((M.capsule(p0, p1, .0045, sides=5), "stitch"))
    # buckle at the front (−Y)
    frame_pts = [Vector((x, -.372, z)) for x, z in S.rrect2d(.20, bw + .09, .035, 4)]
    frame = M.sweep(frame_pts, M.superellipse(.022, .02, 8), up=[(0, -1, 0)] * len(frame_pts), closed_path=True)
    parts.append((frame, "buckle"))
    parts.append((M.capsule((0, -.385, bw / 2 + .04), (0, -.385, -bw / 2 - .02), .014, sides=6), "tongue"))
    # strap tail out of the buckle, hanging down at the front-right
    tail = S.pillow(S.chain2d([(.06, .03), (.22, .0), (.24, -.07), (.08, -.05)]), depth=.016, back=.006, wall=.02)
    tail.translate((0, -.36, -.02))
    tail.deform(lambda co: Vector((co.x, co.y + 1.2 * co.x * co.x, co.z)))
    parts.append((tail, "band"))
    if kind in ("heavy", "war"):
        cx, cy, ang = .24, -.27, 42
        pouch = M.box((.17, .10, .20), (0, 0, 0), bevel=.035, segments=2)
        pouch.translate((cx, cy, -.08))
        pouch.rotate("z", ang, pivot=(cx, cy, -.08))
        parts.append((pouch, "pouch"))
        flap = M.box((.18, .11, .06), (0, 0, 0), bevel=.025, segments=2)
        flap.translate((cx, cy - .006, .015))
        flap.rotate("z", ang, pivot=(cx, cy - .006, .015))
        parts.append((flap, "flap"))
    if kind == "war":
        for a in (-60, -32, 32, 60, 120, 150):
            r = math.radians(a - 90)
            c = Vector((.362 * math.cos(r), .362 * math.sin(r), 0))
            d = M.ellipsoid((.03, .03, .03), (0, 0, 0), sides=10, rings=6)
            d.translate(c)
            parts.append((d, "stud"))
    if kind == "plated":
        for a in (-62, -36, 36, 62, 118, 144):
            r = math.radians(a - 90)
            seg = M.belt(.362, .362, -bw / 2 - .008, bw + .016, .014, sides=48, exp=2.0)
            # keep only a short arc: squash everything outside ±9° onto the arc ends
            def clip_arc(co, r=r):
                ang = math.atan2(co.y, co.x)
                dang = (ang - r + math.pi) % (2 * math.pi) - math.pi
                dc = max(-math.radians(8), min(math.radians(8), dang))
                rad = math.hypot(co.x, co.y)
                return Vector((rad * math.cos(r + dc), rad * math.sin(r + dc), co.z))
            seg.deform(clip_arc)
            parts.append((seg, "plate"))
    obj = K.build_model(f"belt_{kind}", P, regs, parts)
    K.place(obj, pitch=38, yaw=0)
    A = [K.Accent("sparkle", u=(42, 66), r_u=4, alpha=.9)]
    return Model([obj], A, dict(seam_min_contrast=0.14))


# ════════════════════════════════════════════════════════════════════════════════════════════════════════
# Jewellery (web ringSpec / amuletSpec)
# ════════════════════════════════════════════════════════════════════════════════════════════════════════
RINGS = {"copper": (COPPER, None), "silver": (SILVER, SAPPHIRE), "gold": (GOLD, RUBY), "platinum": (PLATINUM, "#B070FF")}


def ring(kind: str) -> Model:
    P = pal("Jewels")
    band_c, gem_c = RINGS[kind]
    regs = dict(band=Region(band_c, .42, .55), prong=Region(band_c, .42, .5),
                engrave=Region(_mix(band_c, "#2A1820", .45), .3, .15))
    parts = []
    band = S.torus(.27, .045, center=(0, 0, 0), axis=(0, 0, 1), rz=.07, sides=12, segs=48, exp=2.4)
    parts.append((band, "band"))
    A = []
    style = {}
    if gem_c:
        regs.update(gem=stone(gem_c, .35, .5, .3))
        seat = M.loft_z([(.26, .045, .05), (.30, .09, .095), (.33, .115, .12)], sides=16, cap0="fan", cap1="fan")
        seat.rotate("x", 90)
        seat.translate((0, -.05, 0))
        seat_t = M.lathe([(.05, -.0), (.13, .07), (.15, .10)], sides=18)
        seat_t.translate((0, 0, .27))
        parts.append((seat_t, "prong"))
        g = S.brilliant(.15, sides=12, axis=(0, 0, 1), center=(0, 0, .40), crown=.42, pavilion=.6)
        parts.append((g, "gem"))
        for k in range(4):
            a = math.radians(45 + 90 * k)
            p0 = Vector((.135 * math.cos(a), .135 * math.sin(a), .36))
            p1 = Vector((.13 * math.cos(a), .13 * math.sin(a), .465))
            parts.append((M.capsule(p0, p1 + Vector((-.03 * math.cos(a), -.03 * math.sin(a), 0)), .02, .015, sides=6), "prong"))
        A.append(K.Accent("sparkle", u=(66, 40), r_u=5, alpha=.9))
        style = dict(auto_glow=0.0)
    else:
        A.append(K.Accent("sparkle", u=(70, 44), r_u=5, alpha=.9))
    obj = K.build_model(f"ring_{kind}", P, regs, parts)
    K.place(obj, pitch=55, yaw=-4)
    return Model([obj], A, style, dict(fill=.78, center_u=(47.5, 50)))


def amulet(kind: str) -> Model:
    P = pal("Jewels")
    regs = dict(chain=metal(GOLD, .45), cord=soft("#8A6A48", .3), bead=soft("#B04A3A", .4), bone=soft(BONE, .42),
                wrap=soft(DARK_LEATHER, .3), gold=metal(GOLD, .5), jade=stone("#2FB870", .25, .5, .3),
                star=metal(GOLD, .55), disc=Region("#2A2248", .42, .3), orb=Region("#5A8CFF", .35, .5, e=.55))
    parts = []
    path = [Vector((.32 * math.cos(math.pi * (1 - t)), 0, .38 - .34 * math.sin(math.pi * (1 - t)) * 1.0))
            for t in [i / 22 for i in range(23)]]
    path = [Vector((p.x, 0, .38 - (.38 - p.z))) for p in path]
    # U: from upper-left down to the pendant and back up
    path = [Vector((.32 * math.cos(math.pi + math.pi * t), .0, .36 + .34 * math.sin(math.pi + math.pi * t)))
            for t in [i / 24 for i in range(25)]]
    A = []
    style = {}
    if kind == "bone":
        parts.append((M.tube(path, [.012] * len(path), sides=6), "cord"))
        for i in (5, 19):
            parts.append((M.ellipsoid((.032, .032, .032), path[i], sides=10, rings=6), "bead"))
        fang = S.pillow(S.chain2d(Wb([(40, 44), (48, 40), (56, 44)], 6), Wb([(56, 44), (58, 66), (48, 90)], 10),
                                  Wb([(48, 90), (40, 70), (40, 44)], 10)), depth=.05, back=.04, wall=.02, edge=.6)
        fang.translate((0, -.01, -.04))
        parts.append((fang, "bone"))
        wrap = S.pillow(S.rrect2d(.21, .07, .03, 3, *W(48, 43.5)), depth=.06, back=.05, wall=.02)
        wrap.translate((0, -.01, -.04))
        parts.append((wrap, "wrap"))
        A.append(K.Accent("sparkle", u=(62, 54), r_u=4.5, alpha=.9))
    else:
        parts.append((S.links(path, .055, .009, face=(0, -1, 0)), "chain"))
        bail = S.torus(.03, .01, center=(0, -.005, .02), axis=(1, 0, 0), sides=6, segs=14)
        parts.append((bail, "gold"))
        if kind == "jade":
            frame = S.torus(.19, .028, center=(0, -.01, -.22), axis=(0, -1, 0), Rz=.19, sides=10, segs=40)
            parts.append((frame, "gold"))
            back_plate = S.pillow(S.circle2d(.19, 32, 0, -.22), depth=.015, back=.01, wall=.01)
            back_plate.translate((0, .01, 0))
            parts.append((back_plate, "gold"))
            g = S.brilliant(.17, sides=12, axis=(0, -1, 0), center=(0, -.02, -.22), crown=.35, pavilion=.3, table=.6,
                            squash=1.0)
            g.scale((1.08, 1, 1), pivot=(0, -.02, -.22))
            parts.append((g, "jade"))
            style = dict(auto_glow=0.25)
        else:
            star = S.pillow(S.star2d(8, .24, .13, cx=0, cz=-.22), depth=.03, back=.015, wall=.015, edge=.5)
            parts.append((star, "star"))
            disc = S.pillow(S.circle2d(.13, 28, 0, -.22), depth=.02, back=.01, wall=.01)
            disc.translate((0, -.03, 0))
            parts.append((disc, "disc"))
            parts.append((M.ellipsoid((.10, .07, .10), (0, -.07, -.22), sides=20, rings=10), "orb"))
            style = dict(auto_glow=0.45)
        A.append(K.Accent("sparkle", u=(64, 52), r_u=4.5, alpha=.9))
    obj = K.build_model(f"amulet_{kind}", P, regs, parts)
    K.place(obj, yaw=-6)
    return Model([obj], A, style)


# ════════════════════════════════════════════════════════════════════════════════════════════════════════
# Materials (web drawOre / drawScrap / drawDust / drawEssence)
# ════════════════════════════════════════════════════════════════════════════════════════════════════════
def material(kind: str) -> Model:
    P = pal("Mats")
    parts = []
    A = []
    style = {}
    if kind == "ore":
        regs = dict(rock=Region("#6A6070", .42, .32), crack=Region("#3E3644", .3, .1),
                    xtal=Region("#FF9A3A", .3, .5, e=.35))
        rock = S.lump((.34, .25, .28), seed=11, noise=.25, flat_bottom=-.2, sides=10, rings=7)
        parts.append((rock, "rock"))
        rs = S.Surface(rock)
        for (x, z, r, ax) in ((-.02, .04, .14, (-.25, -1, .5)), (.16, -.09, .10, (.45, -1, -.1)),
                              (-.18, -.07, .085, (-.55, -1, -.2)), (.10, .16, .07, (.3, -1, .7))):
            p, n = rs.at(x, z)
            g = M.gem(r * .7, r * .9, r * .3, sides=5)
            S.align(g, (Vector(ax).normalized() + n) * .5, (0, 0, 1), at=p - n * .02)
            parts.append((g, "xtal"))
        rs.free()
        style = dict(auto_glow=0.25)
        A.append(K.Accent("sparkle", u=(44, 40), r_u=4, alpha=.9))
        name = "mat_ore"
    elif kind == "scrap":
        regs = dict(plate=Region(IRON, .42, .45), hole=Region("#2A2530", .2, .05), nut=metal(STEEL, .5),
                    shave=metal("#E8EEF4", .55), shave_dk=Region("#3A3F4A", .3, .1), chip=Region(DARK_IRON, .42, .3),
                    rivet=Region("#6A7280", .4, .3))
        shard = S.pillow(Wp([(10, 66), (38, 46), (64, 56), (68, 72), (54, 88), (16, 88)]), depth=.025, back=.02,
                         wall=.025, edge=.25, dome=.1)
        shard.deform(lambda co: Vector((co.x, co.y + .25 * (co.x + .2) ** 2, co.z)))
        parts.append((shard, "plate"))
        ss = S.Surface(shard)
        for x, y in ((26, 72), (46, 76)):
            parts.append((stud(ss, *W(x, y), .022, .008), "rivet"))
        ss.free()
        nut = M.belt(.12, .12, -.04, .08, .07, sides=6, exp=2.0)
        nut.rotate("z", 30)
        nut.rotate("x", 90)
        nut.translate((*[W(64, 40)[0]], -.06, W(64, 40)[1]))
        parts.append((nut, "nut"))
        curl = S.spiral2d(.17, .07, 1.25, 40, phase=-23)
        pts = [Vector((W(28, 34)[0] + x, -.03 + .03 * i / 40, W(28, 34)[1] + z)) for i, (x, z) in enumerate(curl)]
        parts.append((M.tube(pts, [.024] * len(pts), sides=8, exp=3.2), "shave"))
        for (x, y, r) in ((78, 72, 4), (70, 84, 3), (84, 62, 2.6)):
            parts.append((S.lump((r * .012, r * .009, r * .01), seed=int(x + y), noise=.2, center=(*[W(x, y)[0]], -.02, W(x, y)[1])), "chip"))
        A.append(K.Accent("sparkle", u=(58, 30), r_u=4, alpha=.85))
        name = "mat_scrap"
    elif kind == "dust":
        regs = dict(heap=Region("#4F78F0", .35, .5, e=.12), speck=Region("#A8C4FF", .2, .6, e=.6),
                    speck_w=Region("#FFFFFF", .1, .6, e=.8))
        heap = S.lump((.40, .30, .34), seed=5, noise=.08, flat_bottom=-.0, facets=False)
        heap.deform(lambda co: Vector((co.x, co.y, max(0.0, co.z) * (1.0 - .55 * min(1.0, (abs(co.x) / .4) ** 1.5)))))
        heap.translate((0, 0, -.2))
        parts.append((heap, "heap"))
        hs = S.Surface(heap)
        import random
        rnd = random.Random(7)
        for i in range(34):
            x, z = rnd.uniform(-.32, .32), rnd.uniform(-.18, .10)
            d = stud(hs, x, z, .009 + .006 * (i % 3), .006)
            if d is not None:
                parts.append((d, "speck_w" if i % 4 == 0 else "speck"))
        hs.free()
        A += [K.Accent("sparkle", u=(50, 36), r_u=6, hex="#E8F0FF"), K.Accent("sparkle", u=(30, 48), r_u=3.5, hex="#BCD2FF"),
              K.Accent("sparkle", u=(72, 44), r_u=4, hex="#D6E2FF", alpha=.9),
              K.Accent("glow", u=(48, 60), r_u=34, hex="#5F8CFF", alpha=.5, under=True)]
        name = "mat_dust"
        style = dict(seams=False)
    elif kind == "essence":
        regs = dict(xtal=Region("#FFBF2E", .3, .5, e=.3), shard=Region("#FFD35A", .3, .5, e=.3))
        g = S.brilliant(.25, sides=9, axis=(0, -1, 0), center=(0, 0, 0), crown=.45, pavilion=.45, table=.5)
        g.deform(lambda co: Vector((co.x * (1 - .45 * max(0, co.z) / .25), co.y, co.z * 1.25 + (.08 if co.z > 0 else 0) * co.z / .25)))
        parts.append((g, "xtal"))
        for (x, z, r) in ((-.26, -.24, .07), (.28, .18, .055)):
            sh = S.brilliant(r, sides=3, axis=(0, -1, 0), center=(x, -.02, z), crown=.3, pavilion=.4, table=.3)
            parts.append((sh, "shard"))
        A += [K.Accent("glow", u=(48, 50), r_u=42, hex="#FFC23A", alpha=.6, under=True),
              K.Accent("sparkle", u=(36, 30), r_u=6, hex="#FFF6D8"), K.Accent("sparkle", u=(66, 70), r_u=4.5, hex="#FFE7A0", alpha=.9)]
        style = dict(seams=False)
        name = "mat_essence"
    else:
        raise KeyError(kind)
    obj = K.build_model(name, P, regs, parts)
    K.place(obj, yaw=-6 if kind != "dust" else 0, pitch=12 if kind == "dust" else 0)
    return Model([obj], A, style)

"""Icon-only weapon models (spec §9.1 Chapter-1 set, art review of the icons: "keep R4 for the in-world meshes, but
give the icons shape variants"). One parametric builder per web variant table of ``src/graphics/icons/ItemIcons.ts``:

* ``blade(base)``  — SWORDS / DAGGERS (:133-185): blade length × half-width and profile (straight · broad · wavy ·
  serrated · curved · needle, the web ``bladeWidth``), guard (bar · cross · wing · disc), grip length, pommel, gem,
  fuller, rust blotches, poison drip, glow streak;
* ``bow(base)``    — BOWS (:187-196): half-height, bulge, recurve, limb width, horn / steel tips, bands, spikes,
  feathers, glow;
* ``staff(base)``  — STAFFS (:198-202, ``staffSpec``): oak · arcane · rune · elder · lich · abyss heads and hafts;
* ``shield(base)`` — SHIELDS (:204-208, ``shieldSpec``): round wood · iron heater · tower · kite · rune · abyss.

The in-world ``SM_Weapon_<Family>`` stays one mesh per family (R4); its tier atlases are the in-hand look. The
inventory needs the base's *shape* (a broad sword is not a short sword with a bronze guard), so these models are
built from the web numbers (1 web unit = 1 cm, ``item_models.U``) with the same construction language as the shipped
weapons (hollow-ground diamond blades with bright edge bevels, wrapped grips, cabochon stones, toon roles) and are
never exported. Every builder returns ``item_models.Model`` (objs, accents, style, fit) for ``items.py``; the blade
builder also feeds the skill emblems (``motifs.blade``).
"""
from __future__ import annotations

import math
from dataclasses import dataclass

import bmesh
from mathutils import Matrix, Vector

import iconkit as K
import shapes as S
from item_models import (BONE, BRONZE, CRIMSON, CYAN, DARK_IRON, DARK_LEATHER, DEMON, DEMON_BLACK, GOLD, IRON,
                         LEATHER, RUBY, SILVER, STEEL, U, VIOLET, WOOD, Model, W, Wp, _mix, pal)
from kit import mesh as M
from kit.palette import Region


# ── web variant tables (ItemIcons.ts) ───────────────────────────────────────────────────────────────────
@dataclass(frozen=True)
class BladeStyle:
    len: float
    width: float
    shape: str                       # straight | broad | wavy | serrated | curved | needle
    blade: str
    guard: str
    guard_shape: str                 # bar | cross | wing | disc
    grip: str
    grip_len: float
    pommel: str
    guard_w: float | None = None
    gem: str | None = None
    glow: str | None = None
    rust: bool = False
    fuller: bool = False
    drip: str | None = None


SWORDS = {
    "w_short_sword": BladeStyle(52, 5.4, "straight", STEEL, IRON, "bar", LEATHER, 15, IRON, fuller=True),
    "w_rusty_sword": BladeStyle(50, 5.2, "straight", "#A89684", "#756C66", "bar", DARK_LEATHER, 15, "#756C66",
                                rust=True),
    "w_broad_sword": BladeStyle(54, 8, "broad", STEEL, BRONZE, "cross", LEATHER, 15, BRONZE, fuller=True),
    "w_militia_sword": BladeStyle(56, 6, "straight", "#D3DBE4", IRON, "cross", "#3E5A8A", 16, IRON, fuller=True),
    "w_claymore": BladeStyle(62, 7, "broad", "#D8E0EA", DARK_IRON, "cross", DARK_LEATHER, 20, GOLD, guard_w=20,
                             gem=RUBY, fuller=True),
    "w_flamberge": BladeStyle(62, 6.4, "wavy", "#E6D6C8", GOLD, "wing", "#8A1F24", 18, GOLD, guard_w=17,
                              gem="#FF7A1A", glow="#FF8A2A"),
    "w_demon_blade": BladeStyle(60, 7.4, "serrated", "#4A3446", DEMON, "wing", DEMON_BLACK, 17, DEMON, guard_w=18,
                                gem=CRIMSON, glow=CRIMSON),
}
DAGGERS = {
    "w_dagger": BladeStyle(36, 5.4, "straight", STEEL, IRON, "bar", LEATHER, 13, IRON, guard_w=10),
    "w_stiletto": BladeStyle(42, 3.4, "needle", SILVER, GOLD, "disc", "#3A2F4A", 14, GOLD),
    "w_kris": BladeStyle(38, 5, "wavy", "#CFC4B0", BRONZE, "bar", WOOD, 13, BRONZE, guard_w=9),
    "w_assassin_blade": BladeStyle(38, 6, "curved", "#BFD0C2", DARK_IRON, "bar", "#2C3A2C", 13, DARK_IRON, guard_w=9,
                                   drip="#7BE03A"),
    "w_shadow_blade": BladeStyle(40, 5.6, "curved", "#5A4A82", DARK_IRON, "wing", DEMON_BLACK, 13, DARK_IRON,
                                 guard_w=11, gem=VIOLET, glow=VIOLET),
    "w_abyssal_dagger": BladeStyle(40, 6.2, "serrated", "#3A2838", DEMON, "wing", DEMON_BLACK, 13, DEMON, guard_w=12,
                                   gem=CRIMSON, glow=CRIMSON),
}


@dataclass(frozen=True)
class BowStyle:
    h: float                         # half height (tip to tip = 2h)
    bulge: float
    recurve: float
    limb_w: float
    wood: str
    wrap: str
    tip: str | None = None
    bands: str | None = None
    glow: str | None = None
    feathers: str | None = None
    spikes: bool = False


BOWS = {
    "w_short_bow": BowStyle(42, 13, 0, 5, "#B67C45", "#6A3F24"),
    "w_long_bow": BowStyle(48, 13, 0, 4.8, "#8C5530", "#2F5A3A", tip="#D8C8A0"),
    "w_composite_bow": BowStyle(44, 12, 6, 5.2, "#A06A3A", "#7A2A24", tip="#3A2A24", bands="#E0CFA0"),
    "w_war_bow": BowStyle(46, 13, 8, 5.6, "#5C3A28", "#8A2A24", tip=STEEL, bands=IRON, spikes=True),
    "w_eagle_bow": BowStyle(46, 13, 6, 5.2, "#EEE4CC", "#2F5AA0", tip=GOLD, bands=GOLD, feathers="#F4F0E6"),
    "w_abyssal_bow": BowStyle(47, 14, 9, 5.6, "#2E2640", "#5A1F2C", tip="#6A3AA0", bands="#7A4AC0", glow=VIOLET,
                              spikes=True),
}
STAFFS = {"w_oak_staff": "oak", "w_arcane_staff": "arcane", "w_rune_staff": "rune", "w_elder_staff": "elder",
          "w_lich_staff": "lich", "w_abyssal_staff": "abyss"}
SHIELDS = {"w_wooden_shield": "wood", "w_iron_shield": "iron", "w_tower_shield": "tower", "w_kite_shield": "kite",
           "w_runic_shield": "rune", "w_abyssal_shield": "abyss"}
FAMILY_TABLE = {"sword": SWORDS, "dagger": DAGGERS, "bow": BOWS, "staff": STAFFS, "shield": SHIELDS}
DEFAULT_BASE = {"sword": "w_short_sword", "dagger": "w_dagger", "bow": "w_short_bow", "staff": "w_oak_staff",
                "shield": "w_wooden_shield"}


def style_of(fam: str, base: str | None):
    tab = FAMILY_TABLE[fam]
    return tab.get(base or DEFAULT_BASE[fam], tab[DEFAULT_BASE[fam]])


# ── regions (the shipped weapons' roles, weapon_tiers.py) ───────────────────────────────────────────────
def r_blade(h, l=.60):
    return Region(h, .15, l)


def r_metal(h, l=.50, s=.42):
    return Region(h, s, l)


def r_cloth(h, l=.32):
    return Region(h, .42, l)


def r_wood(h, l=.40):
    return Region(h, .42, l)


def r_stone(h, e=.3, l=.45):
    return Region(h, .35, l, e=e)


def r_glow(h, e=1.0):
    return Region(h, 0.0, 0.0, e=e)


def lum(h: str) -> float:
    r, g, b = (int(h[i:i + 2], 16) for i in (1, 3, 5))
    return .2126 * r + .7152 * g + .0722 * b


def shade_of(h: str, k: float = .42) -> str:
    return _mix(_mix(h, "#1E143C", .35), "#000000", k)


# ── geometry helpers ────────────────────────────────────────────────────────────────────────────────────
def P3(x: float, z: float, y: float = 0.0) -> Vector:
    """web local units (x across, z along the item, y depth: − = toward the viewer) → model metres."""
    return Vector((x * U, y * U, z * U))


def split_faces(part: M.Part, keep) -> M.Part | None:
    """A copy of ``part`` with only the faces ``keep(face_index)`` (regions painted on one loft)."""
    p = part.copy()
    dead = [f for i, f in enumerate(p.bm.faces) if not keep(i)]
    if len(dead) == len(p.bm.faces):
        p.bm.free()
        return None
    bmesh.ops.delete(p.bm, geom=dead, context="FACES")
    return p


def loft_sections(rings: list[list[Vector]], tip: Vector | None = None, cap0: bool = True) -> M.Part:
    """Quad loft of equal-length rings (+ an optional point at the end); face order: ring k segment i → k·m + i,
    then the base cap, then the tip fan (index m·(n−1) + 1 + i)."""
    bm = bmesh.new()
    R = [[bm.verts.new(p) for p in ring] for ring in rings]
    m = len(R[0])
    for r0, r1 in zip(R, R[1:]):
        for i in range(m):
            j = (i + 1) % m
            bm.faces.new((r0[i], r1[i], r1[j], r0[j]))
    if cap0:
        bm.faces.new(list(reversed(R[0])))
    if tip is not None:
        tv = bm.verts.new(tip)
        for i in range(m):
            bm.faces.new((R[-1][i], tv, R[-1][(i + 1) % m]))
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return M.Part(bm)


def tube_web(pts, radii, sides=10, y=0.0, exp=2.0, **kw) -> M.Part:
    """A tube through web-local (x, z) points at depth ``y`` (units)."""
    return M.tube([P3(x, z, y) for x, z in pts], [r * U for r in radii], sides=sides, exp=exp, **kw)


def helix_wrap(a: Vector, b: Vector, r: float, turns: float, width: float, thick: float) -> M.Part:
    """A flat strap spiralling round the segment a→b (metres) at radius ``r`` — grip wraps."""
    ax = (b - a)
    L = ax.length
    t_ = ax.normalized()
    u = t_.orthogonal().normalized()
    v = t_.cross(u)
    n = max(8, int(turns * 16))
    pts, ups = [], []
    for i in range(n + 1):
        s = i / n
        ang = 2 * math.pi * turns * s
        d = u * math.cos(ang) + v * math.sin(ang)
        pts.append(a + t_ * (L * s) + d * (r + thick * .5))
        ups.append(d)
    return M.strap(pts, width, thick, up=ups)


def gem_front(r: float, at: Vector, sides: int = 10, crown: float = .42) -> M.Part:
    """A brilliant facing the viewer (−Y) at ``at`` (metres)."""
    return S.brilliant(r, sides=sides, axis=(0, -1, 0), center=tuple(at), crown=crown, pavilion=.45, table=.52)


def orb(r: float, at: Vector) -> M.Part:
    return M.ellipsoid((r, r * .9, r), tuple(at), sides=20, rings=12)


def oval_ring(c: Vector, a: float, b: float, r: float, tilt: float = 0.0, n: int = 24) -> M.Part:
    """A wire ring round the Z axis through ``c`` (metres): half-width ``a`` along X, ``b`` along Y (depth), wire
    radius ``r``, tipped ``tilt`` degrees about Y (a wrap turn seen slanting)."""
    rot = Matrix.Rotation(math.radians(tilt), 3, "Y")
    path = [c + rot @ Vector((a * math.cos(2 * math.pi * i / n), b * math.sin(2 * math.pi * i / n), 0.0)) for i in range(n)]
    return M.sweep(path, M.superellipse(r, r, 6), up=[rot @ Vector((0, 0, 1))] * n, closed_path=True).smooth(True)


SPAN_CAP = .86          # the largest screen extent (× 96 u) any weapon may take: ink + shadow stay inside the square


def framed(objs, wpu: float, cap: float = SPAN_CAP) -> dict:
    """``fit`` kwargs: a fixed scale (``wpu`` m per icon unit, so the family's sizes compare as in the web), shrunk
    only when the placed model would exceed ``cap`` of the square."""
    pts = K.mesh_points(objs)
    ext = max(pts[:, 0].max() - pts[:, 0].min(), pts[:, 2].max() - pts[:, 2].min())
    return dict(world_per_unit=max(wpu, ext / (cap * K.UNIT)))


# ════════════════════════════════════════════════════════════════════════════════════════════════════════
# Blades (web bladeWidth / drawBladeLocal)
# ════════════════════════════════════════════════════════════════════════════════════════════════════════
def blade_width(st: BladeStyle, t: float) -> tuple[float, float]:
    """web bladeWidth: (lit-side half-width, shade-side half-width) at t (0 guard … 1 tip)."""
    w = st.width
    tip_start = .88 if st.shape == "broad" else .8

    def taper(k):
        if t < tip_start:
            return w * (1 - k * t)
        edge = w * (1 - k * tip_start)
        return edge * max(0.0, (1 - t) / (1 - tip_start)) ** .9
    if st.shape == "broad":
        v = taper(.1)
        return v, v
    if st.shape == "needle":
        v = w * max(0.0, 1 - t) ** .85 + .2
        return v, v
    if st.shape == "wavy":
        v = taper(.2) * (1 + .2 * math.sin(t * math.pi * 8) * min(1.0, (1 - t) * 4))
        return v, v
    if st.shape == "serrated":
        v = taper(.14)
        saw = 2.4 * ((t * 11) % 1) if .08 < t < .78 else 0.0
        return v, v + saw
    if st.shape == "curved":
        v = w * (.75 + .45 * math.sin(math.pi * min(1.0, t * 1.1) * .85)) * max(0.0, 1 - t) ** .55
        return v * .8, v * 1.1
    v = taper(.16)
    return v, v


def blade_stations(st: BladeStyle) -> list[float]:
    n = {"wavy": 72, "serrated": 40, "needle": 20}.get(st.shape, 24)
    ts = {i / n for i in range(n)}
    if st.shape == "serrated":                       # the saw's drops: a station either side of each tooth's edge
        for k in range(1, 11):
            c = k / 11
            if .08 < c < .78:
                ts |= {c - .004, c + .004}
        ts |= {.081, .779}
    if st.shape in ("broad", "straight", "curved", "serrated"):
        ts |= {.80, .88, .92, .95, .975}
    return sorted(t for t in ts if t < .985)


# section: (x across, y depth); edge bevels on segments 0, 3, 4, 7 (index i → i+1)
BEVEL_SEGS = {0, 3, 4, 7}
FRONT_FLATS = {4, 5, 6, 7}             # y < 0 side faces the viewer
DARK_FLATS = {1, 6}                    # the flats on the +X (shade) side of the ridge


def blade_section(xc: float, wl: float, wr: float, th: float, k: float = .74, e: float = .5) -> list[tuple[float, float]]:
    return [(xc + wr, 0.0), (xc + wr * k, th * e), (xc, th), (xc - wl * k, th * e), (xc - wl, 0.0),
            (xc - wl * k, -th * e), (xc, -th), (xc + wr * k, -th * e)]


def blade_parts(st: BladeStyle, regs: dict) -> list[tuple[M.Part, str]]:
    """The whole sword / dagger in its local frame: guard at the origin, the blade along +Z (web −y), the lit edge
    toward −X, the viewer at −Y. Adds its regions to ``regs``."""
    L = st.len
    curve = -.22 * L if st.shape == "curved" else 0.0
    ts = blade_stations(st)
    th0 = max(.85, .24 * st.width)

    def xc(t):
        return curve * t * t

    def zc(t):
        return 1.5 + t * (L - 1.5)

    def th(t):
        return th0 * (1 - .45 * t) + .25

    rings = []
    for t in ts:
        wl, wr = blade_width(st, t)
        base = min(wl, wr)
        sec = blade_section(xc(t), max(wl, .25), max(wr, .25), min(th(t), base * .55 + .2))
        rings.append([P3(x, zc(t), y) for x, y in sec])
    tip = P3(xc(1.0), L)
    loft = loft_sections(rings, tip)
    m = 8
    nquads = m * (len(rings) - 1)

    def seg_of(i):
        if i < nquads:
            return i % m
        if i == nquads:
            return -1                               # base cap
        return (i - nquads - 1) % m

    dark = lum(st.blade) < 110
    # the web's two-tone blade: the flat away from the light carries the shade (bt.shade at α .6) — painted, since the
    # key light comes toward the viewer and a front-facing flat never reaches the toon shade band
    regs.update(blade=r_blade(st.blade, .40 if dark else .45),
                blade_dk=r_blade(_mix(st.blade, shade_of(st.blade), .55), .30),
                edge=r_blade(_mix(st.blade, "#FFFFFF", .30 if dark else .45), .55 if dark else .70))
    parts = []
    sheen_t = .55 if st.drip else None
    if sheen_t is not None:                         # poison sheen on the top 45 % (web fillRect over the blade)
        regs["sheen"] = r_blade(_mix(st.blade, st.drip, .45), .45)
        regs["sheen_dk"] = r_blade(_mix(_mix(st.blade, st.drip, .45), shade_of(st.blade), .5), .3)
        regs["sheen_edge"] = r_blade(_mix(_mix(st.blade, st.drip, .45), "#FFFFFF", .4), .7)
        zcut = zc(sheen_t) * U

        def upper(i):
            f = loft.bm.faces[i]
            return f.calc_center_median().z > zcut
    else:
        def upper(i):
            return False
    for reg, pred in (("blade", lambda i: seg_of(i) not in BEVEL_SEGS | DARK_FLATS and not upper(i)),
                      ("blade_dk", lambda i: seg_of(i) in DARK_FLATS and not upper(i)),
                      ("edge", lambda i: seg_of(i) in BEVEL_SEGS and not upper(i)),
                      ("sheen", lambda i: seg_of(i) not in BEVEL_SEGS | DARK_FLATS and upper(i)),
                      ("sheen_dk", lambda i: seg_of(i) in DARK_FLATS and upper(i)),
                      ("sheen_edge", lambda i: seg_of(i) in BEVEL_SEGS and upper(i))):
        if reg not in regs:
            continue
        loft.bm.faces.ensure_lookup_table()
        p = split_faces(loft, pred)
        if p is not None:
            parts.append((p.auto_sharp(28), reg))
    loft.bm.free()

    def ridge_strip(t0, t1, half, lift, reg, sides=(-1, 1)):
        for sg in sides:
            pts, sc = [], []
            k = 14
            for i in range(k + 1):
                t = t0 + (t1 - t0) * i / k
                pts.append(P3(xc(t), zc(t), sg * (th(t) + lift)))
                taper_ = min(1.0, 4 * min(i, k - i) / k + .35)
                sc.append((.22 * U, half * U * taper_))
            prof = M.superellipse(1.0, 1.0, 8, 2.0)
            parts.append((M.sweep(pts, prof, scales=sc, up=[(0, 1, 0)] * len(pts)), reg))

    if st.fuller:                                   # web: line 1.6 in the line tone + a light line beside it
        regs["fuller"] = Region(st.blade, .42, .32).line_region()
        ridge_strip(.04, .62, .85, -.02, "fuller")
    if st.glow:                                     # web: lighter streak 3.2 (α .45) + core 1.2 down the blade
        regs["bglow"] = r_glow(_mix(st.glow, st.blade, .25), .85)
        regs["bcore"] = r_glow(_mix(st.glow, "#FFFFFF", .5), 1.0)
        ridge_strip(.07, .78, 1.7, .02, "bglow")
        ridge_strip(.10, .74, .7, .2, "bcore")
    if st.rust:
        regs["rust"] = Region("#9A5028", .35, .3)
        surf = S.Surface(*[p for p, r in parts if r in ("blade", "edge")])
        for x, y, r in ((2, -12, 3), (-2, -25, 2.2), (3, -33, 2.6), (-1, -41, 1.6), (2.5, -6, 1.8), (-2.5, -17, 1.4)):
            pr = S.pillow(S.circle2d(r * U, 14, x * U, -y * U, rz=r * 1.45 * U), depth=.18 * U, back=.1 * U,
                          wall=.1 * U, edge=.7)
            p, n = surf.at(x * U, -y * U)
            if p is None:
                continue
            pr.translate((0, p.y + .05 * U, 0))
            parts.append((pr, "rust"))
        surf.free()

    # grip (web capsule (0, 1) → (0, gripLen) r 3.1 → 2.8) + wrap lines in the grip's shade
    gl = st.grip_len
    regs["grip"] = r_cloth(st.grip, .34)
    regs["wrap"] = r_cloth(shade_of(st.grip, .30), .2)
    grip = M.capsule(P3(0, -1), P3(0, -gl), 3.1 * U, 2.8 * U, sides=12)
    parts.append((grip, "grip"))
    parts.append((helix_wrap(P3(0, -2.0), P3(0, -gl + 1.2), 2.95 * U, gl / 3.4, 1.1 * U, .45 * U), "wrap"))
    # pommel (r 4.2 at gripLen + 3.6) + gem
    regs["pommel"] = r_metal(st.pommel, .45)
    pz = -(gl + 3.6)
    parts.append((M.ellipsoid((4.2 * U, 3.6 * U, 4.2 * U), tuple(P3(0, pz)), sides=16, rings=9), "pommel"))
    if st.gem:
        regs["pgem"] = r_stone(st.gem, .35 if st.glow else .25)
        parts.append((gem_front(2.4 * U, P3(0, pz, -3.2), sides=8), "pgem"))
    # guard
    gw = st.guard_w if st.guard_w is not None else st.width + 8
    regs["guard"] = r_metal(st.guard, .45)
    if st.guard_shape == "bar":
        parts.append((M.capsule(P3(-gw, 0), P3(gw, 0), 2.8 * U, sides=12, cap_rings=3, up=(0, 0, 1)), "guard"))
    elif st.guard_shape == "disc":
        disc = M.lathe([(.4 * U, -1.4 * U), (gw * .75 * U, -1.0 * U), (gw * .78 * U, 0), (gw * .75 * U, 1.0 * U),
                        (.4 * U, 1.4 * U)], sides=28)
        parts.append((disc.auto_sharp(50), "guard"))
        parts.append((M.ellipsoid((2.6 * U, 2.6 * U, 2.2 * U), tuple(P3(0, 1.6)), sides=12, rings=6), "guard"))
    elif st.guard_shape == "cross":
        pts, rad = [], []
        n = 16
        for i in range(n + 1):
            t = i / n
            x = -gw + 2 * gw * t
            pts.append((x, (x / gw) ** 2 * 5 - 1))
            fl = 2 + 1.2 * (abs(t - .5) * 2) ** 3            # web: 2 + 1.2·|2t − 1|³, flaring at the quillon ends
            rad.append((fl * U, 2.3 * U))
        parts.append((M.tube([P3(x, z) for x, z in pts], rad, sides=12, dome0=1.2 * U, dome1=1.2 * U), "guard"))
        parts.append((M.box((8.4 * U, 6.4 * U, 6.0 * U), tuple(P3(0, -.5)), bevel=1.6 * U, segments=2), "guard"))
        if st.gem:
            regs["ggem"] = r_stone(st.gem, .35)
            parts.append((gem_front(2.4 * U, P3(0, -.4, -3.4), sides=8), "ggem"))
    elif st.guard_shape == "wing":
        w = st.width
        out = [(-gw - 2, -9), (-w - 1, -2.5), (0, -4), (w + 1, -2.5), (gw + 2, -9), (gw - 3, 1.5), (4, 4.2), (0, 6),
               (-4, 4.2), (-gw + 3, 1.5)]
        wing = S.pillow([(x * U, -y * U) for x, y in out], depth=2.6 * U, back=2.2 * U, wall=1.2 * U, edge=.45,
                        dome=.3)
        parts.append((wing, "guard"))
        if st.gem:
            regs["ggem"] = r_stone(st.gem, .45 if st.glow else .3)
            parts.append((gem_front(3.0 * U, P3(0, -.6, -3.6), sides=10), "ggem"))
    if st.drip:                                      # poison drop hanging off the edge near the tip
        regs["drip"] = Region(st.drip, .3, .5, e=.25)
        t = 1 - 2 / 28
        tx, tz = xc(t), zc(t)
        wl, wr = blade_width(st, t)
        d = S.blob2d([((tx + wr - 1.0) * U, (tz - 2) * U), ((tx + wr + 2.6) * U, (tz - 1) * U),
                      ((tx + wr + 3.4) * U, (tz - 9) * U), ((tx + wr + 2.0) * U, (tz - 12.5) * U),
                      ((tx + wr - .4) * U, (tz - 9) * U)], 6)
        parts.append((S.pillow(d, depth=2.0 * U, back=1.4 * U, wall=.8 * U, edge=.6, dome=.4), "drip"))
    return parts


BLADE_SPARKLE_T = .70
BLADE_K = 1.0           # web units → icon units on top of the web fit (our ink + shadow margin is wider)


def blade_model(fam: str, base: str | None) -> Model:
    st = style_of(fam, base)
    regs: dict = {}
    parts = blade_parts(st, regs)
    obj = K.build_model(f"wpn_{fam}_{base or 'default'}", pal(f"Wpn_{fam}"), regs, parts)
    K.place(obj, roll=45, pitch=8, spin=-14)
    L = st.len
    # sparkle on the lit side of the blade, a third down from the tip
    wl, _ = blade_width(st, BLADE_SPARKLE_T)
    xc = (-.22 * L if st.shape == "curved" else 0.0) * BLADE_SPARKLE_T ** 2
    sp = obj.matrix_world @ P3(xc - wl * .2, 1.5 + BLADE_SPARKLE_T * (L - 1.5), -2)
    acc = [K.Accent("sparkle", at=tuple(sp), r_u=5.0, alpha=.95)]
    span = st.len + st.grip_len + 9
    fitw = min(1.5, (100 if fam == "dagger" else 112) / span)       # web bladeSpec fit (daggers stay smaller)
    style = dict(auto_glow=.42 if st.glow else 0.0)
    return Model([obj], acc, style, framed([obj], U / (fitw * BLADE_K)))


# ════════════════════════════════════════════════════════════════════════════════════════════════════════
# Bows (web bowSpec)
# ════════════════════════════════════════════════════════════════════════════════════════════════════════
def bow_model(base: str | None) -> Model:
    st = style_of("bow", base)
    H, B, R = st.h, st.bulge, st.recurve
    shift = (B - R) / 2 - 2

    def at(t):          # web local (x, y): x − = the back of the bow (upper-left on screen), y + = lower limb
        return (-B * (1 - t * t) + R * abs(t) ** 4 + shift, t * H)

    N = 40
    pts = [at(u * 2 - 1) for u in [i / N for i in range(N + 1)]]
    regs = dict(wood=r_wood(st.wood, .40), grain=r_wood(_mix(st.wood, "#FFFFFF", .40), .3),
                wrap=r_cloth(st.wrap, .34), wrapl=r_cloth(shade_of(st.wrap, .3), .2),
                string=Region("#F4ECD4", .2, .5))
    parts = []

    def wl(t):
        a = abs(t * 2 - 1)
        return st.limb_w * (1 - .5 * a ** 1.3) + (1.3 * (1 - a / .14) if a < .14 else 0.0)

    # limb: an elliptic tube (in-plane half-width = the web ribbon, depth ≈ 0.75 × that)
    path = [P3(x, -y) for x, y in pts]
    sc = [(wl(i / N) * .78 * U, wl(i / N) * U) for i in range(N + 1)]
    limb = M.sweep(path, M.superellipse(1.0, 1.0, 12, 2.2), scales=sc, up=[(0, -1, 0)] * len(path),
                   dome0=1.2 * U, dome1=1.2 * U)
    parts.append((limb, "wood"))
    # lit-side grain streak (web: offset 0.45 limbW, light mix .45)
    gp = []
    for i in range(3, 38):
        x, y = pts[i]
        gp.append(P3(x - wl(i / N) * .38, -y, -wl(i / N) * .55))
    parts.append((M.sweep(gp, M.superellipse(1.0, 1.0, 6, 2.0), scales=[(.25 * U, .55 * U)] * len(gp),
                          up=[(0, -1, 0)] * len(gp)), "grain"))
    # string: tip → nock (1.2 behind the grip) → tip
    t_top, t_bot = pts[0], pts[-1]
    sp_ = [P3(t_top[0] + .4, -t_top[1], .3), P3(t_top[0] + 1.2, 0, .3), P3(t_bot[0] + .4, -t_bot[1], .3)]
    parts.append((M.tube(sp_, [.75 * U] * 3, sides=6), "string"))
    # grip wrap round the riser (web rounded rect ±7 tall)
    g0 = at(0)[0]
    gr = [P3(g0 + .2, z) for z in (-7, -3.5, 0, 3.5, 7)]
    wd, ww = ((st.limb_w + 1.3) * .78 + .6) * U, (st.limb_w + 1.8) * U
    parts.append((M.tube(gr, [(wd, ww)] * 5, sides=14, exp=2.6, dome0=.6 * U, dome1=.6 * U), "wrap"))
    for z in (-4.6, -1.6, 1.4, 4.4):                 # wrap turns (web: diagonal lines in the wrap's shade)
        parts.append((oval_ring(P3(g0 + .2, z), ww + .25 * U, wd + .25 * U, .5 * U, tilt=18), "wrapl"))
    if st.tip:
        regs["tip"] = r_metal(st.tip, .45)
        for a, b in ((0, 4), (N, N - 4)):
            pa, pb = pts[a], pts[b]
            parts.append((M.capsule(P3(pa[0], -pa[1]), P3(pb[0], -pb[1]), 2.3 * U, st.limb_w * .92 * U, sides=10), "tip"))
            if base == "w_long_bow":                 # horn nocks curling back at the tips
                sgn = -1 if a == 0 else 1
                h = M.horn(2.0 * U, 6.5 * U, bend_deg=70, sides=8, segments=6)
                S.align(h, (0, 0, -sgn), (1, 0, 0), at=P3(pa[0], -pa[1]))
                parts.append((h, "tip"))
    if st.bands:
        regs["band"] = r_metal(st.bands, .5)
        for u in (.2, .33, .67, .8):
            i = round(u * N)
            x, y = pts[i]
            nx = pts[min(N, i + 1)][0] - pts[max(0, i - 1)][0]
            ny = pts[min(N, i + 1)][1] - pts[max(0, i - 1)][1]
            ax = Vector((nx, 0, -ny)).normalized()
            parts.append((S.torus(wl(u) * .98 * U, .95 * U, center=tuple(P3(x, -y)), axis=tuple(ax), sides=8, segs=20,
                                  rz=1.3 * U), "band"))
    if st.spikes:                                    # blade fins on the back of each limb, raked toward its tip
        regs["spike"] = r_metal(st.tip or STEEL, .45)
        for u in (.25, .75):
            i = round(u * N)
            x, y = pts[i]
            sp = M.horn(2.3 * U, 9.5 * U, bend_deg=0, sides=8, segments=3)
            S.align(sp, (-1, 0, (.45 if u < .5 else -.45)), (0, -1, 0), at=P3(x - wl(u) * .5, -y))
            parts.append((sp, "spike"))
    if st.feathers:
        regs["feather"] = r_cloth(st.feathers, .38)
        regs["quill"] = r_cloth(shade_of(st.feathers, .25), .2)
        for i in (2, N - 2):
            x, y = pts[i]
            d = -1 if y < 0 else 1
            for k, (ox, oy) in enumerate(((0, 0), (2.5, 2.5 * d))):
                fp = S.blob2d([(x + ox, y + oy), (x - 8 + ox, y + d * 4 + oy), (x - 12 + ox, y + d * 12 + oy),
                               (x - 4 + ox, y + d * 8 + oy)], 6)
                parts.append((S.pillow([(px * U, -py * U) for px, py in fp], depth=1.4 * U, back=1.0 * U, wall=.6 * U,
                                       edge=.6).translate((0, (1.5 + 1.2 * k) * U, 0)), "feather"))
            parts.append((tube_web([(x - 1, -(y + d)), (x - 9, -(y + d * 9))], [.45, .3], sides=6, y=-1.2), "quill"))
    if st.glow:
        regs["lglow"] = r_glow(st.glow, .9)
        for a, b in ((6, 16), (25, 35)):
            g = []
            for i in range(a, b + 1):
                x, y = pts[i]
                g.append(P3(x, -y, -wl(i / N) * .72))
            parts.append((M.sweep(g, M.superellipse(1.0, 1.0, 6, 2.0), scales=[(.3 * U, .75 * U)] * len(g),
                                  up=[(0, -1, 0)] * len(g)), "lglow"))
    obj = K.build_model(f"wpn_bow_{base or 'default'}", pal("Wpn_bow"), regs, parts)
    K.place(obj, roll=45, pitch=-6)
    # focal sparkle on the upper limb's back (web eagle / default: the upper tip catches the light)
    x, y = pts[8]
    acc = [K.Accent("sparkle", at=tuple(obj.matrix_world @ P3(x - 3, -y, -4)), r_u=4.8, alpha=.95)]
    if st.glow:
        acc.append(K.Accent("glow", at=tuple(obj.matrix_world @ P3(-22.6, 0)), r_u=16, hex=st.glow, alpha=.3))
    return Model([obj], acc, dict(auto_glow=.35 if st.glow else 0.0), framed([obj], U / 1.18))


# ════════════════════════════════════════════════════════════════════════════════════════════════════════
# Staffs (web staffSpec) — authored in the web's 96-unit icon coordinates (W)
# ════════════════════════════════════════════════════════════════════════════════════════════════════════
STAFF_HAFT = {"oak": WOOD, "arcane": "#7A4A2E", "rune": "#6C7488", "elder": "#D8CCB0", "lich": BONE, "abyss": DEMON_BLACK}
STAFF_ORB = {"oak": "#6EE05A", "arcane": "#4F8CFF", "rune": CYAN, "elder": "#FFB84A", "lich": "#7DFF9A", "abyss": CRIMSON}


def V(x: float, y: float, d: float = 0.0) -> Vector:
    """web icon (x, y) + depth d (units, − toward the viewer) → model point."""
    px, pz = W(x, y)
    return Vector((px, d * U, pz))


def bez_pts(ctrl, n):
    return S.bez(ctrl, n)


def staff_model(base: str | None) -> Model:
    kind = style_of("staff", base)
    a, b = (14, 90), (64, 32)
    gnarled = kind in ("oak", "elder")
    hpts = []
    for i in range(25):
        t = i / 24
        wob = math.sin(t * math.pi * 3.2) * 1.8 if gnarled else 0.0
        hpts.append((a[0] + (b[0] - a[0]) * t + wob * .7, a[1] + (b[1] - a[1]) * t + wob * .7))
    haft_c = STAFF_HAFT[kind]
    orb_c = STAFF_ORB[kind]
    head = (68, 27)
    regs = dict(haft=r_wood(haft_c, .42 if kind not in ("elder", "lich") else .36))
    parts = []
    rad = [(4.4 - (i / 24) * .8 + (1.0 if gnarled and abs(i / 24 - .45) < .05 else 0.0)) * .82 for i in range(25)]
    haft = M.tube([V(x, y) for x, y in hpts], [r * U for r in rad], sides=12, dome0=1.0 * U, dome1=1.0 * U)
    parts.append((haft, "haft"))
    if gnarled:                                      # knots on the gnarled hafts
        regs["knot"] = r_wood(_mix(haft_c, "#000000", .12), .3)
        for t, side in ((.30, 1), (.62, -1)):
            i = round(t * 24)
            x, y = hpts[i]
            parts.append((M.ellipsoid((2.6 * U, 2.2 * U, 2.0 * U), tuple(V(x + side * 2.4, y + side * 2.0, -1.2)),
                                      sides=10, rings=6), "knot"))
    if kind == "lich":                               # bone ridges across the haft
        regs["ridge"] = r_wood(shade_of(haft_c, .25), .2)
        dx, dy = b[0] - a[0], b[1] - a[1]
        L = math.hypot(dx, dy)
        ax = Vector((dx / L, 0, -dy / L))
        for t in (.1, .26, .42, .74, .9):
            x, y = hpts[round(t * 24)]
            r = rad[round(t * 24)]
            parts.append((S.torus(r * 1.0 * U, .7 * U, center=tuple(V(x, y)), axis=tuple(ax), sides=6, segs=18), "ridge"))
    elif kind == "rune":                             # glowing cyan rune studs
        regs["rglow"] = r_glow(CYAN, 1.0)
        for t in (.25, .45, .65, .85):
            x, y = hpts[round(t * 24)]
            r = rad[round(t * 24)]
            parts.append((M.box((2.4 * U, 1.0 * U, 2.4 * U), tuple(V(x, y, -r * .9)), bevel=.4 * U), "rglow"))
    # grip wrap and butt cap
    wrap_c = DEMON if kind == "abyss" else ("#3A4A3A" if kind == "lich" else "#3E5A8A")
    regs.update(wrap=r_cloth(wrap_c, .34), wrapl=r_cloth(shade_of(wrap_c, .3), .2),
                butt=r_metal(GOLD if kind == "elder" else IRON, .45))
    g0, g1 = hpts[9], hpts[13]
    parts.append((M.tube([V(*g0), V(*g1)], [4.25 * .82 * U, 4.1 * .82 * U], sides=12, exp=2.4), "wrap"))
    parts.append((helix_wrap(V(*g0), V(*g1), 4.1 * .82 * U, 3.2, 1.1 * U, .35 * U), "wrapl"))
    dx, dy = (b[0] - a[0]), (b[1] - a[1])
    L = math.hypot(dx, dy)
    ux, uy = dx / L, dy / L
    ferrule = M.loft_z([(0, 3.9 * U, 3.9 * U), (3.5 * U, 4.2 * U, 4.2 * U), (6.0 * U, 3.0 * U, 3.0 * U),
                        (8.0 * U, .5 * U, .5 * U)], sides=12, cap0="fan", cap1="fan")
    ferrule.rotate("x", -90)                         # loft axis +Z → +Y (align's convention)
    S.align(ferrule, (-ux, 0, uy), (0, -1, 0), at=V(a[0] + ux * 4, a[1] + uy * 4))
    parts.append((ferrule, "butt"))
    acc = []
    hx, hy = head
    if kind == "oak":
        regs.update(twig=r_wood(WOOD, .4), leaf=Region("#5AB04A", .42, .42), vein=Region(shade_of("#5AB04A", .2), .3, .15),
                    gem=r_stone("#4AD05A", .3, .5))
        for ctrl in ([(62, 34), (56, 22), (58, 12), (63, 14)], [(64, 32), (78, 28), (84, 18), (80, 16)]):
            tp = bez_pts(ctrl, 10)
            parts.append((M.tube([V(x, y) for x, y in tp], [(2.6 * (1 - i / 10) + .6) * .85 * U for i in range(11)], sides=8,
                                 dome1=.4 * U), "twig"))
        for x, y, r in ((54, 14, -.6), (84, 30, .9)):
            leaf = [(0, -6.5), (3.8, 0), (0, 6.5), (-3.8, 0)]
            c_, s_ = math.cos(r), math.sin(r)
            lp = S.blob2d([W(x + px * c_ - py * s_, y + px * s_ + py * c_) for px, py in leaf], 6)
            parts.append((S.pillow(lp, depth=1.4 * U, back=.8 * U, wall=.5 * U, edge=.6).translate((0, -.5 * U, 0)), "leaf"))
            vp = [W(x + 0 * c_ - py * s_, y + 0 * s_ + py * c_) for py in (-5, 5)]
            parts.append((M.tube([Vector((vp[0][0], -2.3 * U, vp[0][1])), Vector((vp[1][0], -2.3 * U, vp[1][1]))],
                                 [.35 * U, .25 * U], sides=5), "vein"))
        g = S.brilliant(8 * U, sides=10, axis=(0, -1, 0), center=tuple(V(70, 22, -2.0)), crown=.42, pavilion=.5, table=.55)
        g.scale((1.0, 1.0, 1.28), pivot=tuple(V(70, 22, -2.0)))
        parts.append((g, "gem"))
    elif kind == "arcane":
        regs.update(gold=r_metal(GOLD, .45), orb=Region(orb_c, .30, .55, e=.45))
        arc = []
        for i in range(29):
            ang = math.pi * (.15 + 1.2 * i / 28)
            arc.append((hx + math.cos(ang) * 11.8, hy + math.sin(ang) * 11.8))
        parts.append((M.tube([V(x, y) for x, y in arc],
                             [(2.6 + 1.6 * math.sin(math.pi * i / 28)) * U for i in range(29)], sides=10, exp=2.4), "gold"))
        parts.append((orb(9 * U, V(hx + 1, hy - 1, -.5)), "orb"))
        for ang in (.15, 1.35):                      # knobs on the crescent's horns
            ax_, ay_ = hx + math.cos(math.pi * ang) * 10, hy + math.sin(math.pi * ang) * 10
            parts.append((M.ellipsoid((2.2 * U, 2.2 * U, 2.2 * U), tuple(V(ax_, ay_, -2.0)), sides=10, rings=6), "gold"))
    elif kind == "rune":
        regs.update(prong=r_metal("#8A94A8", .45), xtal=Region(CYAN, .3, .55, e=.45))
        for s in (-1, 1):
            pr = [(hx - 4, hy + 10), (hx + s * 10 - 2, hy - 2 - s * 2), (hx + s * 3, hy + 4)]
            parts.append((S.pillow(Wp(pr), depth=2.0 * U, back=1.6 * U, wall=.8 * U, edge=.5, dome=.4), "prong"))
        mq = [(0, -13), (5.2, -5), (5.2, 5), (0, 13), (-5.2, 5), (-5.2, -5)]
        mq = [(px * .8 + py * .5, py * .85 - px * .5) for px, py in mq]
        cx_, cy_ = hx, hy - 4
        xt = S.brilliant(1.0, sides=6, axis=(0, -1, 0), center=(0, 0, 0), crown=.5, pavilion=.5, table=.4)
        # stretch the hexagonal stone into the marquise along the web's tilted axis
        ax = Vector((.65, 0, .76)).normalized()       # along the haft (web: a tilted marquise above the prongs)
        xt.transform(Matrix.Diagonal((5.0 * U, 4.0 * U, 13.0 * U, 1.0)))
        xt.transform(Vector((0, 0, 1)).rotation_difference(ax).to_matrix().to_4x4())
        xt.translate(tuple(V(cx_, cy_, -2.0)))
        parts.append((xt.flat(), "xtal"))
    elif kind == "elder":
        regs.update(orb=Region(orb_c, .30, .55, e=.45), twig=r_wood("#D8CCB0", .36))
        parts.append((orb(11 * U, V(hx, hy)), "orb"))
        for ctrl, d in (([(60, 36), (52, 20), (62, 10), (72, 14)], -2.0), ([(64, 34), (82, 34), (84, 18), (76, 14)], -2.0)):
            tp = bez_pts(ctrl, 12)
            parts.append((M.tube([V(x, y, d * math.sin(math.pi * i / 12)) for i, (x, y) in enumerate(tp)],
                                 [(2.8 * (1 - i / 12) + .7) * .85 * U for i in range(13)], sides=8, dome1=.4 * U), "twig"))
    elif kind == "lich":
        regs.update(skull=Region(BONE, .42, .42), sock=Region("#1A1420", .2, .05), eyeg=r_glow(orb_c, 1.0),
                    horn=r_wood("#8A7A60", .36))
        sk = S.blob2d(Wp([(58, 18), (66, 10), (78, 12), (82, 22), (78, 32), (74, 38), (64, 38), (60, 30)]), 6)
        skull = S.pillow(sk, depth=6.0 * U, back=4.5 * U, wall=1.5 * U, edge=.6, dome=.35)
        parts.append((skull, "skull"))
        ss = S.Surface(skull)
        for (ex, ey, rx, ry, gr) in ((66, 25, 3.6, 4.2, 1.8), (76, 24, 3.2, 4.0, 1.6)):
            p, n = ss.at(*W(ex, ey))
            if p is not None:
                so = S.pillow(S.circle2d(rx * U, 16, *W(ex, ey), rz=ry * U), depth=.5 * U, back=.2 * U, wall=.2 * U)
                so.translate((0, p.y - .1 * U, 0))
                parts.append((so, "sock"))
                parts.append((M.ellipsoid((gr * U, .6 * U, gr * U), (W(ex + .5, ey + .5)[0], p.y - .9 * U, W(ex + .5, ey + .5)[1]),
                                          sides=10, rings=5), "eyeg"))
        p, n = ss.at(*W(70.5, 31.5))
        if p is not None:
            nose = S.pillow(Wp([(70, 29), (72.4, 33.4), (68.6, 33.4)]), depth=.4 * U, back=.2 * U, wall=.2 * U)
            nose.translate((0, p.y - .1 * U, 0))
            parts.append((nose, "sock"))
        for x in (65, 68, 71, 74):
            p, n = ss.at(*W(x, 37))
            if p is not None:
                parts.append((M.capsule((W(x, 35.2)[0], p.y - .2 * U, W(x, 35.2)[1]), (W(x, 38.5)[0], p.y - .2 * U, W(x, 38.5)[1]),
                                        .45 * U, sides=5), "sock"))
        ss.free()
        for ctrl, sx in (([(60, 16), (52, 10), (50, 2)], -1), ([(80, 14), (88, 10), (90, 2)], 1)):
            tp = bez_pts(ctrl, 8)
            parts.append((M.tube([V(x, y, -1.0) for x, y in tp], [(2.6 * (1 - i / 8) + .3) * U for i in range(9)], sides=8),
                          "horn"))
    elif kind == "abyss":
        regs.update(crown=Region("#40324E", .42, .40), orb=Region(orb_c, .30, .55, e=.5))
        cr = Wp([(56, 38), (50, 18), (58, 24), (60, 6), (68, 18), (78, 4), (78, 20), (92, 16), (80, 30), (74, 40)])
        parts.append((S.pillow(cr, depth=2.8 * U, back=2.4 * U, wall=1.0 * U, edge=.4, dome=.3, center=W(68, 28)), "crown"))
        parts.append((orb(8.5 * U, V(hx, hy, -2.6)), "orb"))
    obj = K.build_model(f"wpn_staff_{kind}", pal("Wpn_staff"), regs, parts)
    K.place(obj, yaw=-8)
    mw = obj.matrix_world
    if kind != "oak":
        acc.append(K.Accent("glow", at=tuple(mw @ V(*head)), r_u=14 if kind == "lich" else 22, hex=orb_c,
                            alpha=.30 if kind == "lich" else .40))
    acc.append(K.Accent("sparkle", at=tuple(mw @ V(hx - 12, hy - 10, -8)), r_u=4.5, alpha=.9))
    return Model([obj], acc, dict(auto_glow=0.0), framed([obj], U / .95))


# ════════════════════════════════════════════════════════════════════════════════════════════════════════
# Shields (web shieldSpec / heaterPath) — 96-unit icon coordinates
# ════════════════════════════════════════════════════════════════════════════════════════════════════════
def heater2d(cx, top, w, h, n=10):
    return S.chain2d(S.bez([(cx - w, top + 3), (cx, top - 4), (cx + w, top + 3)], n), [(cx + w, top + h * .38)],
                     S.bez([(cx + w, top + h * .38), (cx + w * .95, top + h * .8), (cx, top + h)], n),
                     S.bez([(cx, top + h), (cx - w * .95, top + h * .8), (cx - w, top + h * .38)], n))


def shield_outline(kind: str):
    if kind == "wood":
        return [(46 + 36 * math.cos(2 * math.pi * i / 48), 48 + 36 * math.sin(2 * math.pi * i / 48)) for i in range(48)]
    if kind == "tower":
        return S.chain2d(S.bez([(22, 14), (46, 6), (70, 14)], 10), [(70, 74)], S.bez([(70, 74), (46, 94), (22, 74)], 12))
    if kind == "kite":
        return S.chain2d(S.bez([(46, 6), (76, 8), (74, 34)], 8), S.bez([(74, 34), (68, 64), (46, 92)], 10),
                         S.bez([(46, 92), (24, 64), (18, 34)], 10), S.bez([(18, 34), (16, 8), (46, 6)], 8))
    if kind == "rune":       # the heater with a rune trim: a pointed arch on top, notched shoulders (its own silhouette)
        return S.chain2d([(14, 15), (30, 10), (46, 3), (62, 10), (78, 15), (78, 33), (74.5, 38.5), (78, 44)],
                         S.bez([(78, 44), (76, 74), (46, 91)], 10), S.bez([(46, 91), (16, 74), (14, 44)], 10),
                         [(17.5, 38.5), (14, 33)])
    if kind == "abyss":
        return [(46, 4), (58, 14), (78, 8), (72, 34), (80, 50), (64, 62), (46, 92), (28, 62), (12, 50), (20, 34), (14, 8),
                (34, 14)]
    return heater2d(46, 10, 32, 80)


SHIELD_COL = {"wood": ("#A8703C", IRON), "iron": ("#A4AEBB", DARK_IRON), "tower": ("#A8323A", DARK_IRON),
              "kite": ("#2F5AA8", GOLD), "rune": ("#4A5468", "#8A94A8"), "abyss": ("#3A2A4A", DEMON)}


def _scale_about(pts, k, c=(46, 48)):
    kx, ky = (k, k) if isinstance(k, (int, float)) else k
    return [(c[0] + (x - c[0]) * kx, c[1] + (y - c[1]) * ky) for x, y in pts]


def _bend(co: Vector, r: float = 1.2) -> Vector:
    """Curve the shield face round a vertical axis (sides back, away from the viewer)."""
    return Vector((co.x, co.y + (co.x * co.x) / (2 * r), co.z))


def shield_model(base: str | None) -> Model:
    kind = style_of("shield", base)
    face_c, rim_c = SHIELD_COL[kind]
    regs = dict(rim=r_metal(rim_c, .45), face=Region(face_c, .42, .35 if kind != "iron" else .40),
                edge=Region(_mix(rim_c, "#201830", .35), .42, .3))
    parts = []
    out_web = shield_outline(kind)
    outline = Wp(out_web)
    # body: the rim-coloured plate (its wall is the 3/4 edge thickness), the inset face proud of it
    body = S.pillow(outline, depth=1.4 * U, back=1.6 * U, wall=2.4 * U, edge=.12, dome=.05)
    body.deform(_bend)
    parts.append((body, "rim"))
    inset_web = _scale_about(out_web, (.84, .86))
    face = S.pillow(Wp(inset_web), depth=1.2 * U, back=.4 * U, wall=.6 * U, edge=.25, dome=.25)
    face.translate((0, -2.0 * U, 0))
    face.deform(_bend)
    parts.append((face, "face"))
    surf = S.Surface(face)
    clip = Wp(_scale_about(out_web, (.80, .82)))

    def strip(a, b, w, thick=.6, reg="deco"):
        from item_models import clip_runs, drape_strip
        for run in clip_runs(clip, W(*a), W(*b), .01):
            st_ = drape_strip(surf, run, w * U, thick * U, step=.008)
            if st_ is not None:
                parts.append((st_, reg))

    def rivet(x, y, r=2.0, reg="rivet", on=None):
        d = S.dome_on(on or surf, *W(x, y), r * U, r * .7 * U)
        if d is not None:
            parts.append((d, reg))

    acc = []
    style = {}
    if kind == "wood":
        regs.update(plank=Region(shade_of(face_c, .38), .3, .15), rivet=r_metal(STEEL, .55), boss=r_metal(IRON, .5))
        for x in (26, 38, 50, 62):
            strip((x, 8), (x, 90), .7, .25, "plank")
        rs = S.Surface(body)
        for i in range(8):
            a = i / 8 * math.pi * 2 + math.pi / 8
            rivet(46 + math.cos(a) * 33.0, 48 + math.sin(a) * 33.0, 1.9, on=rs)
        rs.free()
        boss = S.dome_on(surf, *W(46, 48), 10 * U, 5.0 * U, sides=24)
        parts.append((boss, "boss"))
        acc.append(("sparkle", (28, 20), 4.5, "#FFFFFF", .9))
    elif kind == "iron":
        regs.update(deco=Region("#3E5A8A", .42, .36), rivet=r_metal(STEEL, .55), boss=r_metal(STEEL, .5))
        strip((46, 0), (46, 96), 6.0, .5)
        boss = S.dome_on(surf, *W(46, 44), 8 * U, 4.2 * U, sides=24)
        parts.append((boss, "boss"))
        for x, y in ((22, 20), (70, 20), (24, 56), (68, 56)):
            rivet(x, y)
        acc.append(("sparkle", (28, 20), 4.5, "#FFFFFF", .9))
    elif kind == "tower":
        regs.update(deco=r_metal(GOLD, .45), rivet=r_metal(STEEL, .55), boss=r_metal(GOLD, .5))
        strip((46, 0), (46, 96), 4.0, .6)
        strip((0, 44), (96, 44), 4.0, .6)
        boss = S.dome_on(surf, *W(46, 44), 7 * U, 3.8 * U, lift=.5 * U, sides=24)
        parts.append((boss, "boss"))
        for x, y in ((26, 18), (66, 18), (26, 72), (66, 72)):
            rivet(x, y)
        acc.append(("sparkle", (28, 20), 4.5, "#FFFFFF", .9))
    elif kind == "kite":
        regs.update(deco=Region("#F0EAD8", .42, .4), star=r_metal(GOLD, .5))
        chev = [(0, 30), (46, 58), (96, 30), (96, 42), (46, 70), (0, 42)]
        # chevron: two straps meeting at the bottom point
        strip((14, 36), (46, 64), 6.0, .5)
        strip((46, 64), (78, 36), 6.0, .5)
        _ = chev
        star = S.pillow(S.star2d(4, 10 * U, 3.6 * U, cx=W(46, 34)[0], cz=W(46, 34)[1]), depth=1.6 * U, back=.4 * U,
                        wall=.4 * U, edge=.6, dome=.5)
        p, _n = surf.at(*W(46, 34))
        star.translate((0, (p.y if p is not None else -.04) - .6 * U, 0))
        parts.append((star, "star"))
        acc.append(("sparkle", (28, 20), 4.5, "#FFFFFF", .9))
    elif kind == "rune":
        regs.update(deco=r_glow(CYAN, .95), gem=r_stone(CYAN, .5, .5))
        for a, b in (((46, 24), (46, 70)), ((36, 36), (46, 46)), ((46, 46), (56, 36)), ((36, 56), (46, 62)),
                     ((46, 62), (56, 56))):
            strip(a, b, 1.2, .5)
        g = S.brilliant(5 * U, sides=10, axis=(0, -1, 0), center=(0, 0, 0), crown=.45, pavilion=.4, table=.55)
        p, _n = surf.at(*W(46, 46))
        g.translate((W(46, 46)[0], (p.y if p is not None else -.04) - 1.2 * U, W(46, 46)[1]))
        parts.append((g, "gem"))
        acc.append(("glow", (46, 46), 26, CYAN, .28))
        style = dict(auto_glow=.3)
    elif kind == "abyss":
        regs.update(sclera=Region("#FFE0A0", .35, .45), iris=r_glow(CRIMSON, .85), pupil=Region("#1A0A10", .2, .05),
                    spikes=r_metal(DEMON, .4))
        p, _n = surf.at(*W(46, 44))
        y0 = (p.y if p is not None else -.04)
        eye = S.pillow(S.circle2d(12 * U, 28, *W(46, 44), rz=7 * U), depth=1.6 * U, back=.4 * U, wall=.4 * U, edge=.6)
        parts.append((eye.translate((0, y0 - .2 * U, 0)), "sclera"))
        iris = S.pillow(S.circle2d(4 * U, 18, *W(46, 44), rz=6.5 * U), depth=.6 * U, back=.2 * U, wall=.2 * U)
        parts.append((iris.translate((0, y0 - 2.0 * U, 0)), "iris"))
        pup = S.pillow(S.circle2d(1.5 * U, 12, *W(46, 44), rz=5 * U), depth=.3 * U, back=.1 * U, wall=.1 * U)
        parts.append((pup.translate((0, y0 - 2.7 * U, 0)), "pupil"))
        acc.append(("glow", (46, 44), 20, CRIMSON, .4))
        style = dict(auto_glow=.3)
    surf.free()
    obj = K.build_model(f"wpn_shield_{kind}", pal("Wpn_shield"), regs, parts)
    K.place(obj, yaw=16, pitch=6)
    acc = [K.Accent(k, at=tuple(obj.matrix_world @ V(x, y, -8)), r_u=r, hex=h, alpha=al) for k, (x, y), r, h, al in acc]
    style.setdefault("seam_min_contrast", .12)
    return Model([obj], acc, style, framed([obj], U / .92))


# ── dispatch ────────────────────────────────────────────────────────────────────────────────────────────
def build(fam: str, base: str | None) -> Model:
    if fam in ("sword", "dagger"):
        return blade_model(fam, base)
    if fam == "bow":
        return bow_model(base)
    if fam == "staff":
        return staff_model(base)
    if fam == "shield":
        return shield_model(base)
    raise KeyError(fam)


def has_shape_variants(fam: str) -> bool:
    return fam in FAMILY_TABLE

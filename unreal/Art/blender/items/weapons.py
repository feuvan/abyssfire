"""Generic equipment weapons — one ``SM_Weapon_<Family>`` per weapon base family (DECISIONS I6 / R4, spec §3.6, §9.1).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/items/weapons.py            # ship all
    ... weapons.py --only sword,axe        # ship a subset (manifest entries of the others are kept)
    ... weapons.py --quick sword           # mesh iteration: studio + tiers + in-hand cells → scratch

Families = the core's ``WeaponType`` (sword · axe · mace · dagger · bow · staff · wand · shield; ``Data/item_bases.json``
has bases for all but mace and wand). Every mesh is authored in its socket's bone space (kit README §4: grip centre
at the origin, blade / shaft along +Y, +Z up; shields: face +Y, up +Z like ``SM_Hero_Warrior_Shield``; bows: limbs
along ±Y with the string toward the archer like ``SM_Hero_Rogue_Bow``), attaches with an identity transform, and is
painted on the ``Weapons`` palette by *roles* so the tier atlases (``weapon_tiers.py``: T1 iron · Worn · T2 … T6
abyssal, ``T_AF_Palette_Weapons_<Tier>``) re-skin it. The manifest entry lists ``tiers`` (tier → material instance)
and ``bases`` (item base id → tier) so the UE side can pick the MI for the equipped base.

Previews in ``Art/Previews/SM_Weapon_<Family>/``: ``closeup.png`` (studio views), ``tiers.png`` (every tier atlas),
``game_inhand.png`` (W1 camera, 1080p 1:1, READY + attack contact on the warrior / mage / rogue re-imported from
their shipped FBX, ink-gated), ``game.png`` (the 640×360 frame), ``game_inkgate.png``; ``Previews/weapons_lineup``.
"""
from __future__ import annotations

import argparse
import json
import math
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import itemkit as IK  # noqa: E402  (sets sys.path for kit / heroes)

import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import asset, mesh as M, outline, paths, review, scene, shading  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

import common as C  # noqa: E402  (heroes/common.py: slab2d, thick_patch, smooth_closed, offset_closed …)
import weapon_tiers as WT  # noqa: E402

CATEGORY = "Weapons"
FAMILY_PAL = "Weapons"
ASSET = {f: f"SM_Weapon_{f.capitalize()}" for f in WT.FAMILIES}
SOCKET = {"sword": "weapon_r", "axe": "weapon_r", "mace": "weapon_r", "dagger": "weapon_r", "staff": "weapon_r",
          "wand": "weapon_r", "bow": "weapon_l", "shield": "weapon_l"}
HEROES = ("Warrior", "Mage", "Rogue")


# ── shared helpers ──────────────────────────────────────────────────────────────────────────────────────
def tag(part: M.Part, fn) -> M.Part:
    """Face layer ``af_sub`` = fn(face centre, face) (0 keeps the part's region; Builder ``sub_regions``)."""
    lay = part.bm.faces.layers.int.get("af_sub") or part.bm.faces.layers.int.new("af_sub")
    for f in part.bm.faces:
        f[lay] = int(fn(f.calc_center_median(), f))
    return part


def helix(y0: float, y1: float, r: float, turns: float, width: float, thick: float, phase: float = 0.0,
          per_turn: int = 14) -> M.Part:
    """A flat strap spiralling round the +Y axis (grip wraps, bindings)."""
    n = max(4, int(turns * per_turn))
    pts, ups = [], []
    for i in range(n + 1):
        t = i / n
        a = phase + 2 * math.pi * turns * t
        c, s = math.cos(a), math.sin(a)
        pts.append(Vector(((r + thick * 0.5 - 0.0006) * c, y0 + (y1 - y0) * t, (r + thick * 0.5 - 0.0006) * s)))
        ups.append(Vector((c, 0.0, s)))
    return M.strap(pts, width, thick, up=ups)


def blade_section(w: float, th: float, bevel=(0.50, 0.17), hollow: float = 0.68) -> list[tuple[float, float]]:
    """Hollow-ground diamond (x = flat normal, z = edge direction), warrior review 6: edge, bevel, hollow, ridge."""
    bx, bz = th * bevel[0], w * bevel[1]
    hz = (w - bz) * 0.5
    half = [(0.0, w), (bx, w - bz), (th * hollow, hz), (th, 0.0), (th * hollow, -hz), (bx, -(w - bz)), (0.0, -w)]
    return half + [(-x, z) for x, z in half[-2:0:-1]]


def section_x(w: float, th: float, z: float, bevel=(0.50, 0.17), hollow: float = 0.68) -> float:
    pts = blade_section(w, th, bevel, hollow)[:7]
    az = abs(z)
    for (x0, z0), (x1, z1) in zip(pts[3::-1], pts[2::-1]):
        if z0 <= az <= z1:
            return x0 + (x1 - x0) * (az - z0) / max(z1 - z0, 1e-9)
    return 0.0


def blade_solid(rows, tip_y: float, bevel=(0.50, 0.17), hollow: float = 0.68, edge_sub: bool = False) -> M.Part:
    """Loft of ``blade_section`` rings ``rows = [(y, half_width, half_thick)]`` + a point vertex. ``edge_sub``:
    the bevel facets (edge → bevel break, both edges, both flats) get ``af_sub`` 2 (an edge role)."""
    bm = bmesh.new()
    rings = [[bm.verts.new((x, y, z)) for x, z in blade_section(w, th, bevel, hollow)] for y, w, th in rows]
    m = len(rings[0])
    sub = bm.faces.layers.int.new("af_sub")
    bev = {0, 5, 6, 11}               # section segments edge→bevel (+z: 0, 11; −z: 5, 6)
    for r0, r1 in zip(rings, rings[1:]):
        for i in range(m):
            j = (i + 1) % m
            f = bm.faces.new((r0[i], r1[i], r1[j], r0[j]))
            f[sub] = 2 if (edge_sub and i in bev) else 0
    bm.faces.new(list(reversed(rings[0])))
    tip = bm.verts.new((0, tip_y, 0))
    for i in range(m):
        f = bm.faces.new((rings[-1][i], tip, rings[-1][(i + 1) % m]))
        f[sub] = 2 if (edge_sub and i in bev) else 0
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    p = M.Part(bm)
    for e in p.bm.edges:
        e.smooth = True
    return p.auto_sharp(24)


def ridge_paint(y0: float, y1: float, wfn, thfn, half: float, bevel=(0.50, 0.17), hollow: float = 0.68,
                steps: int = 10) -> M.Part:
    """A painted strip riding the ridge of both flats (fuller line), 0.9 mm proud."""
    parts = None
    for sg in (1, -1):
        o3, i3 = [], []
        for k in range(steps + 1):
            y = y0 + (y1 - y0) * k / steps
            w, th = wfn(y), thfn(y)
            ro, ri = [], []
            for z in (-half, 0.0, half):
                xs = section_x(w, th, z, bevel, hollow)
                ro.append(Vector((sg * (xs + 0.0009), y, z)))
                ri.append(Vector((sg * (xs - 0.0025), y, z)))
            o3.append(ro)
            i3.append(ri)
        p = C.thick_patch(o3, i3)
        parts = p if parts is None else parts.merge(p)
    return parts


def smooth_interp(xs, ys, x: float) -> float:
    return float(np.interp(x, xs, ys))


def profile_slab(outline_uv, thick_fn, rings: int = 4) -> M.Part:
    """A plate from a CCW (u, v) outline (u out from the item axis along −Z, v along +Y), half-thickness
    ``thick_fn(u, v)`` along ±X — axe bits, flukes, flanges. Built with ``C.slab2d`` (polar grid faces)."""
    s = C.slab2d(C.ccw(outline_uv), -0.5, 0.5, rings=rings)
    # slab: x = u, y = depth (−.5 … .5), z = v  →  item: X = depth · 2 · thick, Y = v, Z = −u
    s.deform(lambda co: Vector((co.y * 2.0 * thick_fn(co.x, co.z), co.z, -co.x)))
    s.recalc_normals()
    return s


def cabochon(r: float, h: float, sides: int = 12) -> M.Part:
    """Gem dome along +Z (base at z = 0)."""
    return M.lathe([(r, 0.0), (r * 0.95, h * 0.35), (r * 0.70, h * 0.80), (r * 0.32, h * 0.98), (0.0006, h)],
                   sides=sides, cap0="fan", cap1=None)


# ── families ────────────────────────────────────────────────────────────────────────────────────────────
def build_sword(b: M.Builder) -> list[dict]:
    """Arming sword (web default w_sword: straight blade with a fuller, cross guard, wrapped leather grip, wheel
    pommel with a stone in each face). Blade 76 cm (the warrior's broadsword: 80), overall 98 cm."""
    y0, ytip = 0.096, 0.858
    y1 = ytip - 0.125

    def w(y):
        if y <= y1:
            return 0.0375 - 0.0065 * (y - y0) / (y1 - y0)
        g = (y - y1) / (ytip - y1)
        return max(0.0310 * math.sqrt(max(0.0, 1 - g ** 1.7)), 0.0015)

    def th(y):
        if y <= y1:
            return 0.0125 - 0.003 * (y - y0) / (y1 - y0)
        return max(0.0040, 0.0095 * (w(y) / 0.031) ** 0.6)
    rows = [(y, w(y), th(y)) for y in [y0 + (ytip - y0) * i / 16 for i in range(17)]]
    b.add(blade_solid(rows, ytip + 0.004), "blade", name="blade")
    b.add(ridge_paint(y0 + 0.035, y0 + 0.50, w, th, 0.0056), "fuller", name="fuller", outline=False)
    # cross guard: square-ish bar, quillons curling a little toward the blade, ball finials
    gz = [-0.118, -0.09, -0.06, -0.03, 0.0, 0.03, 0.06, 0.09, 0.118]
    gy = lambda z: 0.084 + 0.012 * (abs(z) / 0.118) ** 2.2   # noqa: E731
    gid = b.add(M.tube([(0, gy(z), z) for z in gz], [(0.010, 0.012), (0.011, 0.0125), (0.012, 0.013), (0.0125, 0.0135),
                                                     (0.013, 0.014), (0.0125, 0.0135), (0.012, 0.013), (0.011, 0.0125),
                                                     (0.010, 0.012)], sides=10, exp=3.0, up=(1, 0, 0)),
                "guard", name="guard", hull_pad=True)
    b.add(M.box((0.030, 0.026, 0.046), (0, 0.084, 0.0), bevel=0.006), "guard", name="guard_block",
          outline=False, covered_by=gid)
    for sg in (1, -1):
        b.add(IK.ell((0.0145, 0.0145, 0.0145), (0, gy(0.118) + 0.002, sg * 0.124), sides=10, rings=6), "guard",
              name="finial", outline=False, covered_by=gid)
    # grip + spiral leather wrap
    grip = b.add(M.tube([(0, -0.080, 0), (0, 0.0, 0), (0, 0.072, 0)], [0.0168, 0.0178, 0.0165], sides=12),
                 "grip", name="grip", hull_pad=True)
    b.add(helix(-0.072, 0.064, 0.0172, 4.2, 0.0085, 0.0032), "wrap", name="grip_wrap", outline=False,
          covered_by=grip)
    # wheel pommel (faces ±X) with a stone set in each face
    wheel = M.lathe([(0.022, -0.0125), (0.029, -0.010), (0.0315, -0.004), (0.0315, 0.004),
                     (0.029, 0.010), (0.022, 0.0125)], sides=18, cap0="fan", cap1="fan")
    wheel.rotate("y", 90).translate((0, -0.112, 0))
    pom = b.add(wheel.auto_sharp(40), "pommel", name="pommel", hull_pad=True)
    b.add(M.tube([(0, -0.086, 0), (0, -0.079, 0)], [0.0195, 0.0195], sides=12), "pommel", name="collar",
          outline=False, covered_by=grip)
    for sg in (1, -1):
        g = cabochon(0.0135, 0.0060).rotate("y", 90 * sg).translate((sg * 0.0118, -0.112, 0))
        b.add(g, "gem", name="pommel_gem", outline=False, covered_by=pom)
    return [{"name": "tip", "pos": (0, ytip, 0)}, {"name": "mid", "pos": (0, (y0 + ytip) / 2, 0)},
            {"name": "guard", "pos": (0, y0, 0)}]


def build_axe(b: M.Builder) -> list[dict]:
    """Battle axe (web drawAxe: wooden haft with a leather grip, crescent steel bit with a bright edge bevel, back
    fluke, top spike, dark-iron socket with gold rivets). Bit on −Z (the edge that leads a swing), 89 cm overall."""
    haft = b.add(M.tube([(0, -0.215, 0), (0, -0.06, 0), (0, 0.25, 0), (0, 0.66, 0)],
                        [0.0190, 0.0205, 0.0192, 0.0180], sides=10, exp=2.2), "haft", name="haft", hull_pad=True)
    b.add(helix(-0.150, 0.080, 0.0202, 6.0, 0.0125, 0.0032), "wrap", name="haft_wrap", outline=False,
          covered_by=haft)
    b.add(IK.lathe_y([(0.016, -0.244), (0.0245, -0.233), (0.0240, -0.214), (0.0195, -0.203)],
                     sides=12, cap0="fan", cap1=None).auto_sharp(45), "butt", name="butt")
    sock = b.add(IK.lathe_y([(0.0215, 0.430), (0.0275, 0.438), (0.0285, 0.53), (0.0275, 0.632), (0.019, 0.645)],
                            sides=12, exp=2.6, cap0=None, cap1="fan").auto_sharp(50), "socket", name="socket",
                 hull_pad=True)
    for yy in (0.462, 0.606):
        for sg in (1, -1):
            d = cabochon(0.0075, 0.0055, sides=8).rotate("y", 90 * sg).translate((sg * 0.0262, yy, 0))
            b.add(d, "rivet", name="rivet", outline=False, covered_by=sock)
    # crescent bit (u = out along −Z, v = up the haft): columns from the socket line A(s) to the edge arc E(s),
    # concave beards; the outer fifth of every column is the bright bevel band (role ``edge``: glows from T5)
    def A(s):
        return (0.021, 0.474 + 0.122 * s)

    def E(s):
        b0, c, b1 = (0.182, 0.392), (0.300, 0.552), (0.192, 0.716)
        return tuple((1 - s) ** 2 * b0[k] + 2 * s * (1 - s) * c[k] + s * s * b1[k] for k in range(2))

    def th(t):
        return float(np.interp(t, [0.0, 0.3, 0.6, 0.8, 1.0], [0.0105, 0.0085, 0.0062, 0.0045, 0.0012]))

    def grid(ts, sign):
        rows = []
        for i in range(15):
            sv = i / 14
            a, e = A(sv), E(sv)
            row = []
            for t in ts:
                u = a[0] + (e[0] - a[0]) * t
                v = a[1] + (e[1] - a[1]) * t + math.sin(math.pi * t) * (0.030 * (1 - sv) - 0.028 * sv)
                row.append(Vector((sign * th(t), v, -u)))
            rows.append(row)
        return rows
    t_main, t_edge = [0.0, 0.12, 0.26, 0.40, 0.54, 0.68, 0.80], [0.80, 0.90, 1.0]
    full = C.thick_patch(grid(t_main + t_edge[1:], 1), grid(t_main + t_edge[1:], -1))
    bid = b.add(C.thick_patch(grid(t_main, 1), grid(t_main, -1)).auto_sharp(38), "head", name="bit", hull=full,
                hull_pad=True)
    b.add(C.thick_patch(grid(t_edge, 1), grid(t_edge, -1)).auto_sharp(38), "edge", name="bit_edge", outline=False,
          covered_by=bid)
    # back fluke (+Z) and top spike
    fl = [(0.020, 0.592), (0.052, 0.586), (0.098, 0.570), (0.140, 0.552), (0.100, 0.528), (0.054, 0.506),
          (0.020, 0.496)]
    fluke = profile_slab([(u, v) for u, v in C.smooth_closed(fl, 3)],
                         lambda u, v: 0.0095 - 0.0055 * max(0.0, min(1.0, (u - 0.02) / 0.12)), rings=3)
    fluke.scale((1, 1, -1))
    b.add(fluke.auto_sharp(38), "head", name="fluke", hull_pad=True)
    spike = M.lathe([(0.0185, 0.0), (0.0150, 0.022), (0.0060, 0.060), (0.0008, 0.074)], sides=4, exp=2.0,
                    cap0="fan", cap1="fan").rotate("z", 45).rotate("x", -90).translate((0, 0.638, 0))
    b.add(spike.flat(), "head", name="spike")
    return [{"name": "tip", "pos": (0, 0.70, -0.215)}, {"name": "mid", "pos": (0, 0.54, -0.12)},
            {"name": "head", "pos": (0, 0.54, 0)}]


def build_mace(b: M.Builder) -> list[dict]:
    """Flanged mace (web drawMace: dark-wood haft wrapped in leather, iron head with six flanges, gold cap)."""
    haft = b.add(M.tube([(0, -0.172, 0), (0, -0.05, 0), (0, 0.39, 0)], [0.0180, 0.0195, 0.0172], sides=10),
                 "haft", name="haft", hull_pad=True)
    b.add(helix(-0.140, 0.075, 0.0190, 5.6, 0.0122, 0.0032, phase=0.6), "wrap", name="haft_wrap", outline=False,
          covered_by=haft)
    b.add(IK.lathe_y([(0.014, -0.203), (0.0235, -0.192), (0.0245, -0.180), (0.0185, -0.166)],
                     sides=12, cap0="fan", cap1=None).auto_sharp(45), "butt", name="butt")
    b.add(IK.lathe_y([(0.0180, 0.365), (0.0250, 0.372), (0.0262, 0.392), (0.0240, 0.410), (0.020, 0.414)],
                     sides=12, cap0=None, cap1=None).auto_sharp(45), "collar", name="collar")
    yc = 0.468
    core = b.add(IK.lathe_y([(0.020, 0.405), (0.034, 0.418), (0.043, 0.445), (0.045, yc), (0.042, 0.495),
                             (0.032, 0.520), (0.020, 0.532)], sides=12, cap0="fan", cap1="fan"),
                 "head", name="head_core", hull_pad=True)
    del core
    prof = [(0.022, 0.400), (0.050, 0.410), (0.078, 0.437), (0.096, 0.468), (0.080, 0.500), (0.054, 0.524),
            (0.030, 0.538), (0.022, 0.538)]
    fl_out = C.smooth_closed(prof + [(0.010, 0.47)], 3)
    for k in range(6):
        a = 360.0 * k / 6 + 15.0
        f = profile_slab(fl_out, lambda u, v: 0.0062 - 0.0022 * max(0.0, min(1.0, (u - 0.03) / 0.06)), rings=3)
        f.rotate("y", a)
        b.add(f.auto_sharp(40), "flange", name="flange", hull_pad=True)
    b.add(IK.lathe_y([(0.024, 0.522), (0.0255, 0.538), (0.019, 0.556), (0.009, 0.574), (0.0008, 0.582)],
                     sides=12, cap0="fan", cap1="fan").auto_sharp(50), "cap", name="cap")
    return [{"name": "tip", "pos": (0, 0.56, 0)}, {"name": "mid", "pos": (0, yc, 0)}]


def build_dagger(b: M.Builder) -> list[dict]:
    """Leaf dagger (web default w_dagger: steel blade, iron bar guard, leather grip, iron pommel); bright edge
    bevels are their own role so the poison / shadow / abyssal tiers make them glow. Blade 29 cm, overall 47."""
    y0, ytip = 0.070, 0.362
    ts = [0.0, 0.22, 0.45, 0.66, 0.84, 1.0]
    ws = [0.0200, 0.0238, 0.0230, 0.0190, 0.0118, 0.0015]

    def w(y):
        return smooth_interp(ts, ws, (y - y0) / (ytip - y0))

    def th(y):
        t = (y - y0) / (ytip - y0)
        return max(0.0028, 0.0078 - 0.0042 * t) * (1.0 if t < 0.84 else max(0.45, 1 - (t - 0.84) * 3.0))
    bev = (0.50, 0.27)
    rows = [(y, w(y), th(y)) for y in [y0 + (ytip - y0) * i / 14 for i in range(15)]]
    b.add(blade_solid(rows, ytip + 0.003, bevel=bev, edge_sub=True), "blade", name="blade",
          sub_regions={2: "edge"})
    gid = b.add(M.tube([(0, 0.061, z) for z in (-0.054, -0.03, 0.0, 0.03, 0.054)],
                       [(0.0090, 0.0105), (0.0095, 0.0110), (0.0100, 0.0115), (0.0095, 0.0110), (0.0090, 0.0105)],
                       sides=10, exp=3.0, up=(1, 0, 0)), "guard", name="guard", hull_pad=True)
    for sg in (1, -1):
        b.add(IK.ell((0.0115, 0.0115, 0.0115), (0, 0.061, sg * 0.057), sides=8, rings=6), "guard", name="finial",
              outline=False, covered_by=gid)
    grip = b.add(M.tube([(0, -0.058, 0), (0, 0.0, 0), (0, 0.054, 0)], [0.0150, 0.0160, 0.0148], sides=12),
                 "grip", name="grip", hull_pad=True)
    b.add(helix(-0.050, 0.047, 0.0155, 3.0, 0.0078, 0.0028), "wrap", name="grip_wrap", outline=False,
          covered_by=grip)
    pom = M.lathe([(0.016, -0.060), (0.021, -0.066), (0.023, -0.075), (0.021, -0.084), (0.015, -0.090),
                   (0.013, -0.091)], sides=14, cap0="fan", cap1="fan")
    pid = b.add(pom.rotate("x", 180).rotate("x", 90), "pommel", name="pommel", hull_pad=True)
    b.add(cabochon(0.0115, 0.0045).rotate("x", 90).translate((0, -0.0895, 0)), "gem", name="pommel_gem",
          outline=False, covered_by=pid)
    return [{"name": "tip", "pos": (0, ytip, 0)}, {"name": "mid", "pos": (0, (y0 + ytip) / 2, 0)}]


# Bows stand in the off hand the way the strapped shield's handle does (limbs along ±Z = up in the socket frame,
# the back of the bow toward +Y = out of the fist, the string toward the archer, −Y): in every class's READY / combat
# clip weapon_l keeps +Z roughly up, so the bow stands upright in front of the forearm. (The rogue's clip-only
# flash-bow is modelled along ±Y because Cast_Multishot aims weapon_l for it.)
BOW_S = Vector((0.0, -1.0, 0.0))          # toward the archer (string side)
BOW_UP = Vector((0.0, 0.0, 1.0))          # upper limb


def build_bow(b: M.Builder) -> list[dict]:
    """Recurve bow in ``weapon_l`` space: grip in the fist, limbs along ±Z, string toward −Y. Web bowSpec: limbs
    tapering to recurved tips, wrapped grip with ring bands, a band at mid-limb, tip caps, a lit grain line on the
    back of each limb; 92 cm tip to tip, brace ≈ 10 cm."""
    S, Y = BOW_S, BOW_UP
    L = 0.46

    def P(y: float) -> Vector:
        f = abs(y) / L
        return Y * y + S * (0.122 * f * f - 0.026 * f ** 4)
    for sgn, nm in ((1.0, "limb_top"), (-1.0, "limb_bot")):
        n = 22
        ys = [sgn * L * i / n for i in range(n + 1)]
        path = [P(y) for y in ys]
        sc = [((0.0215 - 0.0105 * abs(y) / L) / 2 + 0.0005, (0.0330 - 0.0165 * abs(y) / L) / 2) for y in ys]
        lid = b.add(M.sweep(path, M.superellipse(1, 1, 10, 2.6), scales=sc, up=S, dome0=0.0, dome1=0.003),
                    "wood", name=nm, hull_pad=True)
        line = [P(y) - S * (sc[i][0] - 0.0004) for i, y in enumerate(ys) if 0.14 < abs(y) / L < 0.90]
        b.add(M.strap(line, 0.0062, 0.0016, up=[-S] * len(line)), "grain", name="grain", outline=False,
              covered_by=lid)
        tip = P(sgn * L * 0.985)
        b.add(IK.ell((0.0110, 0.0110, 0.0165), tip, sides=10, rings=6), "tip", name="tip_cap", hull_pad=True)
        yy = sgn * L * 0.52
        d, w = sc[int(round(0.52 * n))]
        b.add(M.tube([P(yy - 0.010), P(yy + 0.010)], [(d + 0.0022, w + 0.0022)] * 2, sides=10, up=S, dome0=0.0,
                     dome1=0.0), "band", name="band", outline=False, covered_by=lid)
    grip = b.add(M.tube([P(-0.064), P(0.0), P(0.064)], [(0.0150, 0.0200), (0.0160, 0.0210), (0.0150, 0.0200)],
                        sides=12, up=S, dome0=0.0, dome1=0.0), "wrap", name="bow_grip", hull_pad=True)
    for yy in (-0.070, 0.070):
        b.add(M.tube([P(yy - 0.006), P(yy + 0.006)], [(0.0170, 0.0220)] * 2, sides=12, up=S, dome0=0.0, dome1=0.0),
              "band", name="riser_band", outline=False, covered_by=grip)
    top, bot = P(L * 0.985) + S * 0.006, P(-L * 0.985) + S * 0.006
    b.add(M.tube([top, (top + bot) / 2, bot], [0.0024, 0.0026, 0.0024], sides=6, dome0=0.0, dome1=0.0),
          "string", name="string", outline=False)
    rest = (top + bot) / 2
    return [{"name": "string_top", "pos": tuple(top)}, {"name": "string_bottom", "pos": tuple(bot)},
            {"name": "nock", "pos": tuple(rest + Y * 0.010)}, {"name": "arrow_rest", "pos": tuple(P(0.07) + Vector((-0.02, 0, 0)))},
            {"name": "tip", "pos": tuple(P(L))}, {"name": "mid", "pos": tuple(P(L * 0.5))}]


def build_staff(b: M.Builder) -> list[dict]:
    """Staff (web staffSpec: gnarled shaft with a grip wrap and an iron butt; head = three claws cradling an orb —
    oak twigs, gold arcane claw, rune iron, elder twigs, horn, abyssal spikes by tier). Held like the mage's staff:
    the grip 40 cm above the butt, the orb 81 cm above the grip, 1.27 m overall."""
    ys = [-0.395, -0.30, -0.15, 0.0, 0.15, 0.30, 0.42, 0.47, 0.52, 0.64, 0.712]
    wob = [0.0, 0.0018, -0.0022, 0.0, 0.0020, -0.0025, 0.0015, 0.0020, 0.0, -0.0012, 0.0]
    rad = [0.0175, 0.0180, 0.0182, 0.0186, 0.0178, 0.0172, 0.0168, 0.0205, 0.0166, 0.0160, 0.0165]
    pts = [(wob[i], y, wob[(i + 3) % len(wob)] * 0.7) for i, y in enumerate(ys)]
    sh = b.add(M.tube(pts, rad, sides=10), "shaft", name="shaft", hull_pad=True)
    line = [Vector((pts[i][0] - 0.0005, ys[i], pts[i][2] + rad[i] - 0.0004)) for i in range(1, len(ys) - 2)]
    b.add(M.strap(line, 0.0050, 0.0016, up=[(0, 0, 1)] * len(line)), "grain", name="grain", outline=False,
          covered_by=sh)
    b.add(M.tube([(0, -0.068, 0), (0, 0.108, 0)], [0.0205, 0.0205], sides=12, dome0=0.0, dome1=0.0), "wrap",
          name="grip_sleeve", outline=False, covered_by=sh)
    b.add(helix(-0.064, 0.104, 0.0205, 5.0, 0.0105, 0.0026, phase=1.1), "wrap", name="grip_wrap", outline=False,
          covered_by=sh)
    b.add(IK.lathe_y([(0.006, -0.430), (0.0165, -0.412), (0.0195, -0.392), (0.0182, -0.380)],
                     sides=10, cap0="fan", cap1=None).auto_sharp(45), "butt", name="butt")
    for yy in (0.652, 0.694):
        b.add(IK.ring_y(yy, 0.0205, 0.0115, 0.006, sides=12), "collar", name="collar", outline=False, covered_by=sh)
    yo, ro = 0.815, 0.058
    for k in range(3):
        a = math.radians(90 + 120 * k)
        c, s = math.cos(a), math.sin(a)
        prong = [Vector((0.013 * c, 0.700, 0.013 * s)), Vector((0.040 * c, 0.735, 0.040 * s)),
                 Vector((0.067 * c, 0.782, 0.067 * s)), Vector((0.071 * c, 0.834, 0.071 * s)),
                 Vector((0.055 * c, 0.880, 0.055 * s)), Vector((0.032 * c, 0.899, 0.032 * s))]
        b.add(M.tube(prong, [0.0125, 0.0118, 0.0105, 0.0088, 0.0068, 0.0046], sides=8, dome1=0.004), "claw",
              name="claw", hull_pad=True)
    b.add(IK.ell((ro, ro, ro), (0, yo, 0), sides=16, rings=10), "orb", name="orb", hull_pad=True)
    return [{"name": "crystal", "pos": (0, yo, 0)}, {"name": "tip", "pos": (0, yo + ro, 0)},
            {"name": "mid", "pos": (0, 0.36, 0)}, {"name": "foot", "pos": (0, -0.432, 0)}]


def build_wand(b: M.Builder) -> list[dict]:
    """Wand (web wandSpec: violet wood rod, two gold bands, a cyan marquise crystal in a three-prong setting)."""
    sh = b.add(IK.lathe_y([(0.0105, -0.116), (0.0150, -0.104), (0.0140, -0.090), (0.0125, -0.04),
                           (0.0118, 0.06), (0.0100, 0.17), (0.0094, 0.214), (0.0125, 0.222)],
                          sides=10, cap0="fan", cap1="fan"), "shaft", name="shaft", hull_pad=True)
    for yy, r in ((-0.095, 0.0158), (0.070, 0.0135), (0.205, 0.0118)):
        b.add(IK.ring_y(yy, r, 0.0125, 0.005, sides=12), "band", name="band", outline=False, covered_by=sh)
    b.add(IK.lathe_y([(0.008, -0.123), (0.012, -0.115), (0.0118, -0.108)], sides=10,
                     cap0="fan", cap1=None), "cap", name="cap", outline=False, covered_by=sh)
    cr = M.loft_z([(-0.040, 0.0006, 0.0006), (-0.012, 0.0200, 0.0155), (0.0, 0.0220, 0.0170),
                   (0.012, 0.0200, 0.0155), (0.058, 0.0006, 0.0006)], sides=8, cap0="fan", cap1="fan")
    cr.rotate("x", -90).translate((0, 0.256, 0))
    cid = b.add(cr.flat(), "crystal", name="crystal", hull_pad=True)
    for k in range(3):
        a = math.radians(30 + 120 * k)
        c, s_ = math.cos(a), math.sin(a)
        pr = [Vector((0.010 * c, 0.214, 0.010 * s_)), Vector((0.0195 * c, 0.234, 0.0195 * s_)),
              Vector((0.0165 * c, 0.254, 0.0165 * s_))]
        b.add(M.tube(pr, [0.0044, 0.0038, 0.0028], sides=6, dome1=0.002), "band", name="prong", outline=False,
              covered_by=cid)
    return [{"name": "crystal", "pos": (0, 0.256, 0)}, {"name": "tip", "pos": (0, 0.314, 0)},
            {"name": "mid", "pos": (0, 0.10, 0)}]


SH_W, SH_H = 0.42, 0.56
SH_R = 0.46               # face curvature
SH_FACE_Y = 0.118         # face in front of the grip (as the warrior's: fist + forearm clear behind it)
SH_CX, SH_CZ = -0.040, -0.015


def heater(scale: float = 1.0) -> list[tuple[float, float]]:
    """Web heaterPath (w 32, h 80 → the chunkier generic heater) as a CCW (x across, z up) outline, 42 × 56 cm."""
    src = [(-1.0, 0.43), (-0.5, 0.48), (0.0, 0.50), (0.5, 0.48), (1.0, 0.43), (1.0, 0.13), (0.96, -0.08),
           (0.82, -0.27), (0.58, -0.40), (0.30, -0.47), (0.0, -0.52), (-0.30, -0.47), (-0.58, -0.40), (-0.82, -0.27),
           (-0.96, -0.08), (-1.0, 0.13)]
    sm = C.smooth_closed(src, 3)
    xs, zs = [p[0] for p in sm], [p[1] for p in sm]
    kx = SH_W / (max(xs) - min(xs)) * scale
    kz = SH_H / (max(zs) - min(zs)) * scale
    return C.ccw([(x * kx, z * kz) for x, z in sm])


def bend(co: Vector) -> Vector:
    a = co.x / SH_R
    r = SH_R + co.y
    return Vector((r * math.sin(a) + SH_CX, r * math.cos(a) - SH_R + SH_FACE_Y, co.z + SH_CZ))


def build_shield(b: M.Builder) -> list[dict]:
    """Heater shield (web shieldSpec default 'iron': rim, face field, a vertical band (+ a cross bar for the tower /
    kite / rune tiers), a domed boss, four rivets, plank seams that only the wooden tier paints). Back: boards,
    two straps and the handle the fist closes on (as SM_Hero_Warrior_Shield, so every hero's off-hand pose fits)."""
    out = heater()
    field = C.offset_closed(out, -0.030)
    slab = C.slab2d(out, -0.026, 0.021, rings=4)
    slab.deform(bend)
    core = C.slab2d(C.offset_closed(out, -0.006), -0.019, 0.002, rings=4)
    core.deform(bend)
    body = b.add(core.auto_sharp(70), "back", name="boards", hull=slab, hull_pad=True)
    fld = C.slab2d(field, -0.002, 0.0100, rings=5)
    fld.deform(bend)
    b.add(fld.auto_sharp(70), "face", name="field", outline=False, covered_by=body)
    inner = C.offset_closed(field, 0.002)
    rim = C.thick_patch([[Vector((x, 0.021, z)) for x, z in out], [Vector((x, 0.021, z)) for x, z in inner]],
                        [[Vector((x, -0.006, z)) for x, z in out], [Vector((x, -0.006, z)) for x, z in inner]],
                        wrap_u=True)
    rim.deform(bend)
    b.add(rim.auto_sharp(50), "rim", name="rim", outline=False, covered_by=body)
    bo, bi = C.offset_closed(out, -0.001), C.offset_closed(out, -0.030)
    brim = C.thick_patch([[Vector((x, -0.006, z)) for x, z in bo], [Vector((x, -0.006, z)) for x, z in bi]],
                         [[Vector((x, -0.026, z)) for x, z in bo], [Vector((x, -0.026, z)) for x, z in bi]],
                         wrap_u=True)
    brim.deform(bend)
    b.add(brim.auto_sharp(50), "rim", name="back_rim", outline=False, covered_by=body)
    zs = [p[1] for p in field]
    ztop, zbot = max(zs), min(zs)

    def half_w(z: float) -> float:       # field half-width at height z (the heater is symmetric)
        best = 0.0
        for (x0, z0), (x1, z1) in zip(field, field[1:] + field[:1]):
            if (z0 - z) * (z1 - z) <= 0 and z0 != z1:
                best = max(best, abs(x0 + (x1 - x0) * (z - z0) / (z1 - z0)))
        return best

    def band(pts_xz, half: float, lift: float, name: str, region: str):
        rows_o, rows_i = [], []
        for (x, z), d in pts_xz:
            rows_o.append([Vector((x + dx * half, 0.0100 + lift, z + dz * half)) for dx, dz in ((-d[1], d[0]), (d[1], -d[0]))])
            rows_i.append([Vector((x + dx * half, 0.0040, z + dz * half)) for dx, dz in ((-d[1], d[0]), (d[1], -d[0]))])
        p = C.thick_patch(rows_o, rows_i)
        p.deform(bend)
        b.add(p.auto_sharp(50), region, name=name, outline=False, covered_by=body)
    nv = 14
    vpts = [((0.0, ztop - 0.004 - (ztop - zbot - 0.012) * i / nv), (0.0, -1.0)) for i in range(nv + 1)]
    band(vpts, 0.034, 0.0032, "band_v", "band_v")
    zh = 0.050
    hw = half_w(zh) - 0.004
    hpts = [((-hw + 2 * hw * i / 10, zh), (1.0, 0.0)) for i in range(11)]
    band(hpts, 0.024, 0.0026, "band_h", "band_h")
    # plank seams (painted; only the wooden tier gives them a colour of their own)
    for xs_ in (-0.105, 0.105):
        sp = [((xs_, z), (0.0, -1.0)) for z in np.linspace(ztop - 0.01, -0.20 if abs(xs_) > 0.1 else zbot, 9)]
        sp = [((x, z), d) for (x, z), d in sp if half_w(z) > abs(x) + 0.012]
        if len(sp) >= 2:
            band(sp, 0.0026, 0.0009, "seam", "seam")
    # boss and rivets
    boss = M.lathe([(0.046, 0.0), (0.044, 0.008), (0.036, 0.020), (0.022, 0.030), (0.008, 0.034), (0.0005, 0.035)],
                   sides=18, cap0="fan", cap1=None).rotate("x", -90)
    boss.translate((0.0, 0.0105, zh))
    boss.deform(bend)
    b.add(boss.auto_sharp(40), "boss", name="boss", outline=False, covered_by=body)
    for x, z in ((-0.13, 0.19), (0.13, 0.19), (-0.115, -0.09), (0.115, -0.09)):
        r = cabochon(0.0095, 0.0065, sides=8).rotate("x", -90).translate((x, 0.0105, z))
        r.deform(bend)
        b.add(r, "rivet", name="rivet", outline=False, covered_by=body)
    for zz in (0.110, -0.110):
        st = M.box((0.30, 0.010, 0.032), (0.0, -0.022, zz), bevel=0.003)
        st.deform(bend)
        b.add(st, "strap", name="strap", outline=False, covered_by=body)
    yb = SH_FACE_Y - 0.028
    hdl = M.tube([(0, yb, 0.075), (0, 0.010, 0.050), (0, -0.004, 0.0), (0, 0.010, -0.050), (0, yb, -0.075)],
                 [0.011, 0.012, 0.012, 0.012, 0.011], sides=8)
    b.add(hdl, "strap", name="handle")
    c = bend(Vector((0.0, 0.0105, zh)))
    return [{"name": "center", "pos": tuple(bend(Vector((0.0, 0.0, 0.0))))}, {"name": "emblem", "pos": tuple(c)}]


BUILDERS = {"sword": build_sword, "axe": build_axe, "mace": build_mace, "dagger": build_dagger, "bow": build_bow,
            "staff": build_staff, "wand": build_wand, "shield": build_shield}


# ── palette + build ─────────────────────────────────────────────────────────────────────────────────────
def palettes() -> tuple[Palette, dict[str, dict[str, Region]], list]:
    """The ``Weapons`` palette (T1) with every family's roles + the tier atlases (``IK.AtlasVariant``)."""
    pal = Palette(FAMILY_PAL)
    tabs = {f: {v: outline.ink_regions(r, "weapon") for v, r in t.items()} for f, t in WT.tables().items()}
    base = IK.plan_roles(tabs, "T1")
    for fam, regs in base.items():
        for role, r in regs.items():
            pal.add(f"{fam}.{role}", r)
    variants = []
    for v in ("Worn",) + IK.TIERS[1:]:
        variants.append(IK.AtlasVariant(pal, v, {f"{fam}.{role}": tabs[fam][v][role]
                                                 for fam in tabs for role in tabs[fam][v]}))
    return pal, base, variants


def build(fam: str, pal: Palette, base: dict) -> tuple[bpy.types.Object, list[dict]]:
    b = M.Builder(ASSET[fam], pal, prefix=fam)
    b.regions(**base[fam])
    socks = BUILDERS[fam](b)
    obj = asset.finish_mesh(b, pal, "weapon")
    obj.name = ASSET[fam]
    return obj, socks


def variant_of(variants, v: str):
    return next(x for x in variants if x.variant == v)


# ── previews ────────────────────────────────────────────────────────────────────────────────────────────
STUDIO = {   # (world matrix of the item for the "flat" and "edge" views, label)
    "default": [(lambda: Matrix.Rotation(math.radians(135), 4, "Z") @ Matrix.Rotation(math.radians(90), 4, "X"), "FLAT"),
                (lambda: Matrix.Rotation(math.radians(45), 4, "Z") @ Matrix.Rotation(math.radians(90), 4, "X"), "EDGE")],
    "shield": [(lambda: Matrix.Rotation(math.radians(45), 4, "Z"), "FACE"),
               (lambda: Matrix.Rotation(math.radians(225), 4, "Z"), "BACK")],
    "bow": [(lambda: Matrix.Rotation(math.radians(135), 4, "Z"), "SIDE"),
            (lambda: Matrix.Rotation(math.radians(170), 4, "Z"), "3/4")],
}
# the axe bit and mace flanges read best turned a little off the flat
STUDIO["axe"] = [(lambda: Matrix.Rotation(math.radians(135), 4, "Z") @ Matrix.Rotation(math.radians(90), 4, "X"), "FLAT"),
                 (lambda: Matrix.Rotation(math.radians(100), 4, "Z") @ Matrix.Rotation(math.radians(90), 4, "X"), "3/4")]
STUDIO["mace"] = STUDIO["axe"]


def detail_view(fam: str):
    """(matrix, point filter, label) for the third studio cell: the hilt / head close-up."""
    if fam in ("sword", "dagger"):
        d = Vector((1.0, -1.0, 1.1)).normalized()
        return R_frame(d), (lambda p, d=d: (p @ np.array(d)) < (0.13 if fam == "sword" else 0.09)), "HILT"
    if fam in ("axe", "mace", "staff", "wand"):
        d = Vector((0.6, -0.6, 1.0)).normalized()
        ymin = {"axe": 0.36, "mace": 0.36, "staff": 0.62, "wand": 0.17}[fam]
        return R_frame(d), (lambda p, d=d, y=ymin: (p @ np.array(d)) > y), "HEAD"
    if fam == "bow":
        return Matrix.Rotation(math.radians(150), 4, "Z"), (lambda p: np.abs(p[:, 2]) < 0.12), "GRIP"
    return Matrix.Rotation(math.radians(70), 4, "Z"), (lambda p: p[:, 2] > -1), "FACE 3/4"


def R_frame(d: Vector) -> Matrix:
    from kit import rig as R
    return R.frame((0, 0, 0), d, (1.0, 1.0, 0.0))


def studio_set(fam: str, obj, label_tier: str = "T1") -> list[np.ndarray]:
    cells = []
    with IK.only_visible([obj]):
        for mfn, lab in STUDIO.get(fam, STUDIO["default"]):
            IK.detach(obj, mfn())
            bpy.context.view_layer.update()
            cells.append(IK.studio([obj], label=f"{fam.upper()} {lab}"))
        m, filt, lab = detail_view(fam)
        IK.detach(obj, m)
        bpy.context.view_layer.update()
        pts = review.mesh_points([obj])
        sel = pts[filt(pts)]
        cells.append(IK.studio([obj], pts=sel if len(sel) > 8 else pts, fill=0.85, label=f"{fam.upper()} {lab}"))
    IK.detach(obj)
    return cells


def tier_sheet(fam: str, obj, pal, variants) -> np.ndarray:
    mfn, _ = STUDIO.get(fam, STUDIO["default"])[0]
    order = [("T1", pal)] + [(v.variant, v) for v in variants]
    order = [order[0], order[1]] + order[2:] if fam == "sword" else [o for o in order if o[0] != "Worn"]
    cells = []
    res = (190, 300) if fam not in ("shield",) else (210, 250)
    with IK.only_visible([obj]):
        IK.detach(obj, mfn())
        bpy.context.view_layer.update()
        pts = review.mesh_points([obj])
        for v, p in order:
            IK.set_toon(obj, p)
            img = IK.studio([obj], res=res, pts=pts, fill=0.92, label=f"{v} {WT.TIER_NAMES[v].upper()}")
            cells.append(img)
    IK.set_toon(obj, pal)
    obj.data.materials[1] = bpy.data.materials[shading.OUTLINE_MATERIAL]
    IK.detach(obj)
    return review.grid(cells, len(cells))


class Heroes:
    """The three heroes re-imported from their shipped FBX, READY pose; one shown at a time at the origin."""

    def __init__(self, clips=("Idle",)):
        self.h = {}
        for cls in HEROES:
            arm, body = IK.import_hero(cls)
            self.h[cls] = (arm, body)
        self.show(None)

    def show(self, cls: str | None):
        for k, (arm, body) in self.h.items():
            body.hide_render = k != cls

    def arm(self, cls):
        return self.h[cls][0]

    def body(self, cls):
        return self.h[cls][1]


def contact_frame(cls: str, clip: str = "Attack01") -> int:
    """The clip's Contact / Release frame from the shipped manifest (60 fps)."""
    man = json.loads(paths.manifest_path().read_text())
    for a in man["assets"][f"SK_Hero_{cls}"]["anims"]:
        if a["name"] == clip:
            ms = a.get("contactMs", a.get("releaseMs", a["lengthMs"] / 2))
            return int(round(ms * 0.06))
    return 0


POSES = (("Idle", None, "READY"), ("Attack01", "contact", "ATTACK01 CONTACT"))
CELL = (330, 330)


def inhand(fam: str, obj, heroes: Heroes, out_dir: Path):
    """W1 1080p 1:1 cells: the weapon on each hero's socket in the READY pose and at the Attack01 contact (se),
    ink-gated together with the hero (hero class 3.5 px, gate 3.0 px). Also the lowest point of the weapon in each
    pose (``inHand`` QA: generic meshes ride clips authored for each class's own weapon)."""
    cells, reports, qa = [], [], {}
    wide = first = None
    obj.hide_render = False
    for cls in HEROES:
        arm, body = heroes.arm(cls), heroes.body(cls)
        heroes.show(cls)
        IK.attach(obj, arm, SOCKET[fam])
        gc = IK.GameCells(ASSET[fam], arm, [body, obj], IK.HERO_HEIGHT[cls], "hero", blob_radius=0.36,
                          out_dir=out_dir)
        for clip, beat, lab in POSES:
            IK.hero_pose(arm, cls, clip, contact_frame(cls, clip) if beat else 0)
            gc.rv.facing("se")
            low = IK.min_z([obj])
            qa[f"{cls}.{clip}"] = round(low * 100.0, 1)
            img, st = gc.cell(f"{cls} {lab}", facing="se", min_px=CELL, label=False)
            cell = IK.pad_to(img, CELL[1], CELL[0])
            review.draw_text(cell, 3, 3, f"{cls} {lab}".upper())
            review.draw_text(cell, 3, 12, gc.rv.ink_label(st), rgb=(255, 220, 140))
            review.draw_text(cell, 3, 21, f"WEAPON LOWEST {low * 100:.0f}CM", rgb=(200, 230, 255) if low > -0.005
                             else (255, 90, 90))
            cells.append(cell)
            if first is None:
                first = (img, st, gc)
        if cls == "Warrior":
            IK.hero_pose(arm, cls, "Idle", 0)
            wide = gc.wide(ASSET[fam], text=f"{ASSET[fam]} ON SK_HERO_WARRIOR  W1 640X360")
        reports += gc.reports
        IK.hero_pose(arm, cls, "Idle", 0)
        arm.rotation_euler = (0, 0, 0)
    heroes.show(None)
    IK.detach(obj)
    obj.hide_render = True
    v = review.ink_verdict(reports)
    img0, st0, gc0 = first
    return cells, v, (wide, gc0.inkmap(img0, st0)), qa


# ── ship ────────────────────────────────────────────────────────────────────────────────────────────────
def manifest_extra(fam: str, variants) -> dict:
    bases = [b for b in IK.weapon_bases() if b.get("weaponType") == fam]
    tiers = {"T1": f"MI_AF_Toon_{FAMILY_PAL}"}
    for v in variants:
        tiers[v.variant] = v.material_name
    used = sorted({IK.weapon_tier(b) for b in bases}, key=lambda t: ("Worn",) + IK.TIERS)
    return {
        "weaponType": fam, "attachSocket": SOCKET[fam], "attachKind": "hand", "heldBy": [],
        "tiers": tiers, "tierNames": {k: WT.TIER_NAMES[k] for k in tiers},
        "bases": {b["id"]: IK.weapon_tier(b) for b in bases},
        "tierRule": "item base levelReq >= 1 T1 · 6 T2 · 15 T3 · 20 T4 · 28 T5 · 35 T6; w_rusty_sword Worn",
        "note": ("generic family mesh for every hero (I6 / R4): attach to attachSocket with no extra transform; swap "
                 "material slot 0 to tiers[bases[<equipped base id>]] (MI over T_AF_Palette_Weapons_<Tier>); slot 1 "
                 "stays M_AF_Outline (weapon ink 3.5 px)" + ("; no item base uses this family yet (core WeaponType "
                                                              "enum)" if not bases else "")),
        "tiersUsed": used,
    }


def ship(fams: list[str], previews: bool = True, verify: bool = True) -> dict:
    t0 = time.time()
    scene.reset()
    pal, base, variants = palettes()
    pal.save()
    for v in variants:
        v.save()
    objs = {}
    for fam in fams:
        objs[fam] = build(fam, pal, base)
        objs[fam][0].hide_render = True
    entries = {}
    for fam in fams:
        obj, socks = objs[fam]
        e = IK.ship_static_item(obj, ASSET[fam], CATEGORY, pal, "weapon", socks, extra=manifest_extra(fam, variants),
                                palettes=variants)
        entries[fam] = e
        print(f"[weapons] {ASSET[fam]}: toon {e['triangles']['toon']} hull {e['triangles']['outline']} tris, "
              f"bounds {e['boundsCm']}")
    result = {"assets": {ASSET[f]: entries[f]["triangles"] for f in fams}, "ink": {}}
    if previews:
        heroes = Heroes()
        upd = {}
        for fam in fams:
            obj, _ = objs[fam]
            out = paths.preview_dir(ASSET[fam])
            sheet = review.grid(studio_set(fam, obj), 3)
            files = [IK.save(out, "closeup.png", sheet)]
            files.append(IK.save(out, "tiers.png", tier_sheet(fam, obj, pal, variants)))
            cells, v, (wide, imap), qa = inhand(fam, obj, heroes, out)
            files.append(IK.save(out, "game_inhand.png", review.grid(cells, 6, pad=2)))
            files.append(IK.save(out, "game.png", wide))
            files.append(IK.save(out, "game_inkgate.png", imap))
            print(f"[ink] {ASSET[fam]}: {v['cells']} cells, min rim {v['minFracRim'] * 100:.1f} %, dark "
                  f"{v['minFracDark'] * 100:.1f} %, median {v['medianRimPx']} px -> {'PASS' if v['pass'] else 'FAIL'}")
            for f in v["failures"]:
                print("   ", f)
            result["ink"][ASSET[fam]] = v["pass"]
            upd[ASSET[fam]] = {"previews": [str(p.relative_to(paths.preview_root())) for p in files],
                               "inkGate": IK.ink_entry(v),
                               "inHandLowestCm": qa}
        IK.update_entries(upd)
        lineup(objs, heroes)
    if verify:
        fails = []
        for fam in fams:
            obj, socks = objs[fam]
            fails += IK.verify_fbx(ASSET[fam], CATEGORY, sum(p.loop_total - 2 for p in obj.data.polygons),
                                   [s["name"] for s in socks], FAMILY_PAL)
        print("[verify] " + ("OK" if not fails else "\n  ".join(["FAIL"] + fails)))
        result["verify"] = fails
    print(f"[weapons] done in {time.time() - t0:.0f}s")
    return result


def lineup(objs: dict, heroes: Heroes) -> None:
    """``Previews/weapons_lineup/lineup.png``: every family (T1) upright beside the warrior for scale, W1 camera
    direction, studio background."""
    out = paths.preview_root() / "weapons_lineup"
    heroes.show("Warrior")
    arm, body = heroes.arm("Warrior"), heroes.body("Warrior")
    arm.rotation_euler = (0, 0, math.radians(review.FACINGS["se"]))
    order = ["sword", "axe", "mace", "dagger", "staff", "wand", "bow", "shield"]
    keep = [body]
    x = 0.55
    for fam in order:
        if fam not in objs:
            continue
        o = objs[fam][0]
        if fam == "shield":
            m = Matrix.Translation((x + 0.05, -x - 0.05, 0.30)) @ Matrix.Rotation(math.radians(45), 4, "Z")
        elif fam == "bow":
            m = Matrix.Translation((x, -x, 0.49)) @ STUDIO["bow"][0][0]()
        else:
            lo = min(v.co.y for v in o.data.vertices)
            m = Matrix.Translation((x, -x, -lo + 0.02)) @ STUDIO["default"][0][0]()
        IK.detach(o, m)
        keep.append(o)
        x += 0.36
    bpy.context.view_layer.update()
    with IK.only_visible(keep):
        img = IK.studio(keep, res=(1200, 420), pitch=-14.0, ink_px=1.8, fill=0.94,
                        label="SM_WEAPON_* (T1) BESIDE SK_HERO_WARRIOR 1.76 M")
    IK.save(out, "lineup.png", img)
    for fam in order:
        if fam in objs:
            IK.detach(objs[fam][0])
    heroes.show(None)
    arm.rotation_euler = (0, 0, 0)


def quick(fams: list[str]) -> None:
    """Mesh iteration: studio + tiers + in-hand cells → scratch (no export, no manifest)."""
    scene.reset()
    pal, base, variants = palettes()
    pal.save()
    for v in variants:
        v.save()
    heroes = Heroes()
    for fam in fams:
        obj, _ = build(fam, pal, base)
        obj.hide_render = True
        out = IK.SCRATCH / fam
        print(f"[quick] {fam}: toon {M.tri_count(obj, 0)} hull {M.tri_count(obj, 1)}")
        IK.save(out, "closeup.png", review.grid(studio_set(fam, obj), 3))
        IK.save(out, "tiers.png", tier_sheet(fam, obj, pal, variants))
        cells, v, (wide, imap), qa = inhand(fam, obj, heroes, out)
        IK.save(out, "game_inhand.png", review.grid(cells, 6, pad=2))
        print(f"[quick] {fam} lowest cm {qa}")
        IK.save(out, "game_inkgate.png", imap)
        print(f"[quick] {fam} ink {'PASS' if v['pass'] else 'FAIL'} min rim {v['minFracRim']:.3f} "
              f"dark {v['minFracDark']:.3f} {v['failures'][:3]}")


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="")
    ap.add_argument("--quick", default="")
    ap.add_argument("--no-previews", action="store_true")
    ap.add_argument("--no-verify", action="store_true")
    a = ap.parse_args(argv)
    if a.quick:
        quick([f for f in a.quick.split(",") if f])
        return 0
    fams = [f for f in (a.only.split(",") if a.only else WT.FAMILIES) if f]
    r = ship(fams, previews=not a.no_previews, verify=not a.no_verify)
    bad = [k for k, ok in r["ink"].items() if not ok] + (r.get("verify") or [])
    return 1 if bad else 0


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    sys.exit(main(argv))

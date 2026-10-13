"""Generic equipment weapons — one ``SM_Weapon_<Family>`` per weapon base family (DECISIONS I6 / R4, spec §3.6, §9.1).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/items/weapons.py            # ship all
    ... weapons.py --only sword,axe        # ship a subset (manifest entries of the others are kept)
    ... weapons.py --quick sword           # mesh iteration: studio + tiers + in-hand cells → scratch

Families = the core's ``WeaponType`` (sword · axe · mace · dagger · bow · staff · wand · shield; ``Data/item_bases.json``
has bases for all but mace and wand). Every mesh is authored in its socket's bone space (kit README §4: grip centre
at the origin, blade / shaft along +Y, +Z up; shields: face +Y, up +Z like ``SM_Hero_Warrior_Shield``; bows: limbs
along ±Z, string toward −Y), painted on the ``Weapons`` palette by *roles* so the tier atlases (``weapon_tiers.py``:
T1 iron · Worn · T2 … T6 abyssal, ``T_AF_Palette_Weapons_<Tier>``) re-skin it. The manifest entry lists ``tiers``
(tier → material instance) and ``bases`` (item base id → tier) so the UE side can pick the MI for the equipped base.

The meshes ride clips authored for each class's own weapon, so each (class, family) has **grips** (``weapon_grips.py``,
table ``weapon_grips.json``; manifest ``grips``: a socket-relative default + per-clip overrides, blended over
``gripBlendMs``) and the ship runs the **whole-clip in-hand QA** at those grips (every frame of every hero clip:
ground, item × body / head / the class's other default item) and exits 1 on a failure, like the warrior's ship. The
bow is a main-hand base held in the left hand: ``occupiesOffhand``.

Previews in ``Art/Previews/SM_Weapon_<Family>/``: ``closeup.png`` (studio views), ``tiers.png`` (every tier atlas),
``game_inhand_<Class>.png`` (W1 camera, 1080p 1:1: READY, a Run frame, the Attack01-03 contacts and the Cast01
release, at the shipped grips with the class's default attachments, ink-gated; only the classes the runtime shows the
generic mesh on), ``game.png`` (the 640×360 frame), ``game_inkgate.png``; ``Previews/weapons_lineup``.
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
import weapon_grips as WG  # noqa: E402
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

    def grid(ts, sign, A=A, E=E, beard=(0.030, 0.028), side=-1.0, k=1.0):
        rows = []
        for i in range(15):
            sv = i / 14
            a, e = A(sv), E(sv)
            row = []
            for t in ts:
                u = a[0] + (e[0] - a[0]) * t
                v = a[1] + (e[1] - a[1]) * t + math.sin(math.pi * t) * (beard[0] * (1 - sv) - beard[1] * sv)
                row.append(Vector((sign * th(t) * k, v, side * u)))
            rows.append(row)
        return rows
    t_main, t_edge = [0.0, 0.12, 0.26, 0.40, 0.54, 0.68, 0.80], [0.80, 0.90, 1.0]
    full = C.thick_patch(grid(t_main + t_edge[1:], 1), grid(t_main + t_edge[1:], -1))
    bid = b.add(C.thick_patch(grid(t_main, 1), grid(t_main, -1)).auto_sharp(38), "head", name="bit", hull=full,
                hull_pad=True)
    b.add(C.thick_patch(grid(t_edge, 1), grid(t_edge, -1)).auto_sharp(38), "edge", name="bit_edge", outline=False,
          covered_by=bid)

    # back blade (+Z): the web drawAxe's smaller crescent (≈ 0.6 × the main bit), so the head reads as a
    # double-bit war axe from either side; its own bright bevel band shares the ``edge`` role
    def A2(s):
        return (0.021, 0.488 + 0.100 * s)

    def E2(s):
        b0, c, b1 = (0.108, 0.452), (0.172, 0.546), (0.112, 0.640)
        return tuple((1 - s) ** 2 * b0[k] + 2 * s * (1 - s) * c[k] + s * s * b1[k] for k in range(2))
    kw = dict(A=A2, E=E2, beard=(0.020, 0.020), side=1.0, k=0.9)
    full2 = C.thick_patch(grid(t_main + t_edge[1:], 1, **kw), grid(t_main + t_edge[1:], -1, **kw))
    bk = b.add(C.thick_patch(grid(t_main, 1, **kw), grid(t_main, -1, **kw)).auto_sharp(38), "head", name="back_bit",
               hull=full2, hull_pad=True)
    b.add(C.thick_patch(grid(t_edge, 1, **kw), grid(t_edge, -1, **kw)).auto_sharp(38), "edge", name="back_edge",
          outline=False, covered_by=bk)
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
    # collar: closed revolve (art review 9: open lathe ends let the hull show through as a dark notch)
    b.add(IK.lathe_y([(0.0180, 0.380), (0.0245, 0.386), (0.0250, 0.398), (0.0205, 0.404), (0.0215, 0.418),
                      (0.0240, 0.436)], sides=12, cap0="fan", cap1="fan").auto_sharp(45), "collar", name="collar")
    # web drawMace: an iron ball with triangular flanges fanning out of it and a gold boss. 3D: the ball ringed by
    # a gold band, 13 blade-spikes (rhombic section, flat in the meridian plane, so from the side each reads as
    # the web's triangle and from the camera as a spike): 6 on the band, 6 at +42° (offset 30°), one crowning the
    # gold top boss
    yc, rb = 0.486, 0.0625          # head 20 % larger than the first pass (art review 9: a grey blob at 1080p)
    ctr = Vector((0.0, yc, 0.0))
    head = IK.ell((rb, rb * 0.96, rb), tuple(ctr), sides=14, rings=9)
    hid = b.add(head, "head", name="head_ball", hull_pad=True)
    b.add(IK.ring_y(yc, rb + 0.0045, 0.022, 0.010, sides=18), "cap", name="band", outline=False, covered_by=hid)

    def spike(d: Vector, length: float, wm: float, wt: float) -> M.Part:
        d = d.normalized()
        ref = Vector((0, 1, 0)) if abs(d.y) < 0.95 else Vector((0, 0, 1))
        t1 = (ref - d * ref.dot(d)).normalized()          # along the meridian
        t2 = d.cross(t1).normalized()                    # along the azimuth
        c = ctr + d * (rb * 0.62)
        bm = bmesh.new()
        ring = [bm.verts.new(c + t1 * wm), bm.verts.new(c + t2 * wt), bm.verts.new(c - t1 * wm),
                bm.verts.new(c - t2 * wt)]
        apex = bm.verts.new(ctr + d * (rb + length))
        for i in range(4):
            bm.faces.new((ring[i], ring[(i + 1) % 4], apex))
        bm.faces.new(list(reversed(ring)))
        bm.normal_update()
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        return M.Part(bm).flat()
    for ring_el, n, off, ln in ((0.0, 6, 0.0, 0.038), (46.0, 5, 36.0, 0.032)):
        for k in range(n):
            az = math.radians(360.0 * k / n + off + 15.0)
            el = math.radians(ring_el)
            d = Vector((math.cos(el) * math.cos(az), math.sin(el), math.cos(el) * math.sin(az)))
            b.add(spike(d, ln, 0.0250, 0.0180), "flange", name="spike", hull_pad=True)
    b.add(IK.lathe_y([(0.024, yc + rb * 0.80), (0.028, yc + rb * 0.92), (0.024, yc + rb * 1.06),
                      (0.014, yc + rb * 1.16), (0.0008, yc + rb * 1.20)], sides=12, cap0="fan", cap1="fan")
          .auto_sharp(50), "cap", name="cap", hull_pad=True)
    b.add(spike(Vector((0, 1, 0)), 0.050, 0.0140, 0.0140), "flange", name="spike_top", hull_pad=True)
    return [{"name": "tip", "pos": (0, yc + rb + 0.050, 0)}, {"name": "mid", "pos": (0, yc, 0)}]


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


# Bows are modelled in ``weapon_l`` space like the strapped shield's handle (limbs along ±Z, the back of the bow toward
# +Y = out of the fist, the string toward the archer, −Y); the per-class grip table (``weapon_grips.json``, manifest
# ``grips``) turns the bow in each hero's hand so its D-profile faces the W1 camera. (The rogue's clip-only flash-bow is
# modelled along ±Y because Cast_Multishot aims weapon_l for it.)
BOW_S = Vector((0.0, -1.0, 0.0))          # toward the archer (string side)
BOW_UP = Vector((0.0, 0.0, 1.0))          # upper limb
BOW_L = 0.46                              # half length (92 cm tip to tip)
BOW_B, BOW_R = 0.215, 0.065               # web bowSpec bulge 13 / 84: a deep D, brace 15 cm, tips recurving a little


def bow_path(y: float) -> Vector:
    f = abs(y) / BOW_L
    return BOW_UP * y + BOW_S * (BOW_B * f * f - BOW_R * f ** 6)


def build_bow(b: M.Builder) -> list[dict]:
    """Recurve bow in ``weapon_l`` space: grip in the fist, limbs along ±Z, string toward −Y. Web bowSpec: limbs
    tapering to recurved tips, wrapped grip with ring bands, a band at mid-limb, tip caps, a lit grain line on the
    back of each limb; 92 cm tip to tip, brace 15 cm (art review: the D must read at the W1 distance — a 10 cm brace
    and a 2.4 mm string read as a straight stick). The string is a light no-hull cord, 1.3 cm across in the bow's
    plane (≥ 1.5 px at 1080p when the D faces the camera)."""
    S, Y, L = BOW_S, BOW_UP, BOW_L
    P = bow_path
    for sgn, nm in ((1.0, "limb_top"), (-1.0, "limb_bot")):
        n = 22
        ys = [sgn * L * i / n for i in range(n + 1)]
        path = [P(y) for y in ys]
        # (thickness in the bow's plane, width across it): near-round and chunkier than a real self bow, so the
        # limbs keep ≈ 3–4 px + ink each side with the D facing the camera
        sc = [((0.0360 - 0.0170 * abs(y) / L) / 2, (0.0400 - 0.0180 * abs(y) / L) / 2) for y in ys]
        lid = b.add(M.sweep(path, M.superellipse(1, 1, 10, 2.6), scales=sc, up=S, dome0=0.0, dome1=0.003),
                    "wood", name=nm, hull_pad=True)
        line = [P(y) - S * (sc[i][0] - 0.0004) for i, y in enumerate(ys) if 0.14 < abs(y) / L < 0.90]
        b.add(M.strap(line, 0.0062, 0.0016, up=[-S] * len(line)), "grain", name="grain", outline=False,
              covered_by=lid)
        tip = P(sgn * L * 0.985)
        b.add(IK.ell((0.0135, 0.0135, 0.0195), tip, sides=10, rings=6), "tip", name="tip_cap", hull_pad=True)
        yy = sgn * L * 0.52
        d, w = sc[int(round(0.52 * n))]
        b.add(M.tube([P(yy - 0.010), P(yy + 0.010)], [(d + 0.0022, w + 0.0022)] * 2, sides=10, up=S, dome0=0.0,
                     dome1=0.0), "band", name="band", outline=False, covered_by=lid)
    b.add(M.tube([P(-0.064), P(0.0), P(0.064)], [(0.0185, 0.0245), (0.0195, 0.0255), (0.0185, 0.0245)],
                 sides=12, up=S, dome0=0.0, dome1=0.0), "wrap", name="bow_grip")
    # riser bands: each its own thin hull (art review 12: padding the grip proxy round them grew a tent)
    for yy in (-0.070, 0.070):
        b.add(M.tube([P(yy - 0.006), P(yy + 0.006)], [(0.0205, 0.0265)] * 2, sides=12, up=S, dome0=0.0, dome1=0.0),
              "band", name="riser_band")
    top, bot = P(L * 0.985) + S * 0.006, P(-L * 0.985) + S * 0.006
    # the string: an elliptic cord, wide (1.3 cm) in the bow's plane, thin (0.7 cm) across it
    X = S.cross(Y).normalized()
    pts = [top + (bot - top) * (i / 6) for i in range(7)]
    b.add(M.tube(pts, [(0.0035, 0.0065)] * 7, sides=8, up=X, dome0=0.0, dome1=0.0), "string", name="string",
          outline=False)
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
    """The three heroes re-imported from their shipped FBX with their **default attachments** (warrior sword +
    shield, mage staff + crystal, rogue daggers + quiver: what the runtime keeps beside a generic weapon); one shown
    at a time at the origin."""

    def __init__(self):
        self.h, self.att = {}, {}
        for cls in HEROES:
            arm, body = IK.import_hero(cls)
            self.h[cls] = (arm, body)
            self.att[cls] = [(a, sk, self._import_att(a, sk, arm)) for a, sk in WG.DEFAULTS[cls]]
            if cls == "Mage":       # the crystal rides the staff's ``crystal`` socket (manifest itemAttachments)
                staff = self.att[cls][0][2]
                cr = self._import_att("SM_Hero_Mage_Crystal", None, None)
                cr.parent = staff
                cr.matrix_parent_inverse = Matrix.Identity(4)
                cr.matrix_basis = Matrix.Translation((0.0, 0.7336, 0.0))
                self.att[cls].append(("SM_Hero_Mage_Crystal", "weapon_r", cr))
        self.show(None)

    @staticmethod
    def _import_att(asset_name: str, socket: str | None, arm):
        before = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(paths.category_dir("Weapons") / f"{asset_name}.fbx"))
        new = [o for o in bpy.data.objects if o not in before]
        o = next(x for x in new if x.type == "MESH")
        for x in new:
            if x is not o:
                bpy.data.objects.remove(x)
        o.name = f"ATT_{asset_name}"
        IK.set_toon(o, IK.ReadOnlyPalette("Heroes"))
        o["af_outline_width"] = outline.WIDTH_CM["weapon"] / 100.0
        o["af_outline_class"] = "weapon"
        if socket == "quiver":
            IK.attach(o, arm, "pelvis")
            o.matrix_basis = Matrix.Translation(WG.QUIVER_LOCAL)
        elif socket is not None:
            IK.attach(o, arm, socket)
        return o

    def show(self, cls: str | None, fam: str | None = None, socket: str | None = None) -> list:
        """Show ``cls`` (body + the default attachments the runtime keeps with ``fam`` on ``socket``); returns the
        visible attachment objects."""
        vis = []
        for k, (arm, body) in self.h.items():
            body.hide_render = k != cls
            for a, sk, o in self.att[k]:
                keep = k == cls and fam is not None and sk != (socket or SOCKET[fam]) and not (
                    fam == "bow" and sk in ("weapon_r", "weapon_l"))
                o.hide_render = not keep
                if keep:
                    vis.append(o)
        return vis

    def arm(self, cls):
        return self.h[cls][0]

    def body(self, cls):
        return self.h[cls][1]


def beat_frame(cls: str, clip: str) -> int:
    """The clip's Contact / Release frame from the shipped manifest (60 fps)."""
    man = json.loads(paths.manifest_path().read_text())
    for a in man["assets"][f"SK_Hero_{cls}"]["anims"]:
        if a["name"] == clip:
            ms = a.get("contactMs", a.get("releaseMs", a["lengthMs"] / 2))
            return int(round(ms * 0.06))
    return 0


def clip_frames(cls: str, clip: str) -> int:
    man = json.loads(paths.manifest_path().read_text())
    return next(a["frames"] for a in man["assets"][f"SK_Hero_{cls}"]["anims"] if a["name"] == clip)


# in-hand cells (art review: READY + a Run frame + every strike's contact / release, at the shipped grips)
POSES = (("Idle", "ready", "READY"), ("Run", "run", "RUN"), ("Attack01", "beat", "A01 CONTACT"),
         ("Attack02", "beat", "A02 CONTACT"), ("Attack03", "beat", "A03 CONTACT"), ("Cast01", "beat", "CAST01"))
CELL = (330, 330)


def pose_frame(cls: str, clip: str, beat: str) -> int:
    if beat == "beat":
        return beat_frame(cls, clip)
    if beat == "run":
        return clip_frames(cls, clip) // 4          # the near foot's push-off: the weapon swung furthest
    return 0


def line_mask(obj, part: str = "string"):
    """(hidden copy of ``obj`` keeping only ``part``'s toon faces, callable(border) → bool mask of its coverage
    dilated 2 px). The ink gate leaves the bow's string out: a light no-hull line by design (art review: ≥ 1.5 px, no
    ink), so its edges carry no rim on purpose."""
    names = json.loads(obj["af_parts"])
    ids = {i for i, n in enumerate(names) if n == part}
    me = obj.data.copy()
    bm = bmesh.new()
    bm.from_mesh(me)
    lay = bm.faces.layers.int.get("af_part")
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f[lay] not in ids or f.material_index != 0], context="FACES")
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(f"{obj.name}_{part}_only", me)
    bpy.context.scene.collection.objects.link(o)
    o.hide_render = True

    def fn(border):
        o.matrix_world = obj.matrix_world.copy()
        if obj.hide_render:
            return None
        with IK.only_visible([o]):
            body, _ = review.coverage_pass([o], border)
        m = body > 0.2
        for _ in range(2):
            pm = np.pad(m, 1, constant_values=False)
            m = m | pm[:-2, 1:-1] | pm[2:, 1:-1] | pm[1:-1, :-2] | pm[1:-1, 2:]
        return m
    return o, fn


def inhand(fam: str, obj, heroes: Heroes, out_dir: Path, table: dict, qa_low: dict | None = None):
    """W1 1080p 1:1 cells (``game_inhand_<Class>.png``) for every class the runtime shows this generic mesh on:
    READY, a Run frame and the Attack01-03 contacts + the Cast01 release, the weapon on its socket at the shipped
    grip of each clip, the class's default attachments beside it, ink-gated together with the hero (weapon class
    3.5 px, gate 3.0 px). Off-hand items (shield, bow) are shown from sw (the holding hand toward the camera)."""
    files, reports = [], []
    wide = first = None
    obj.hide_render = False
    for cls in HEROES:
        if not WG.shown(cls, fam):
            continue
        sock = WG.socket_for(table, cls, fam)
        # off-hand items from sw (the holding hand toward the camera), a shield slung on the back from ne
        face = "sw" if sock == "weapon_l" else ("se" if sock in WG.HAND_SOCKETS else "ne")
        arm, body = heroes.arm(cls), heroes.body(cls)
        vis = heroes.show(cls, fam, sock)
        IK.attach(obj, arm, sock)
        gc = IK.GameCells(ASSET[fam], arm, [body, obj] + vis, IK.HERO_HEIGHT[cls], "weapon", blob_radius=0.36,
                          out_dir=out_dir)
        if fam == "bow":
            line_obj, gc.rv.ink_ignore = line_mask(obj)
        cells = []
        for clip, beat, lab in POSES:
            fr = pose_frame(cls, clip, beat)
            IK.hero_pose(arm, cls, clip, fr)
            obj.matrix_basis = Matrix(WG.grip(table, cls, fam, clip, fr).tolist())
            hw = WG.hide_window(table, cls, fam, clip)
            hidden = bool(hw and hw[0] <= fr <= hw[1])        # the runtime hides the mesh in this window
            obj.hide_render = hidden
            bpy.context.view_layer.update()
            gc.rv.facing(face)
            low = IK.min_z([obj])
            img, st = gc.cell(f"{cls} {lab} {face}", facing=face, min_px=CELL, label=False)
            cell = IK.pad_to(img, CELL[1], CELL[0])
            review.draw_text(cell, 3, 3, f"{cls} {lab} F{fr} {face}".upper())
            review.draw_text(cell, 3, 12, gc.rv.ink_label(st), rgb=(255, 220, 140))
            g = WG.grip_label(table, cls, fam, clip) + (" HIDDEN" if hidden else "")
            review.draw_text(cell, 3, 21, f"GRIP {g.upper()}  LOW {low * 100:.0f}CM",
                             rgb=(200, 230, 255) if low > -0.005 else (255, 90, 90))
            obj.hide_render = False
            cells.append(cell)
            if first is None:
                first = (img, st, gc)
        if wide is None:
            IK.hero_pose(arm, cls, "Idle", 0)
            obj.matrix_basis = Matrix(WG.grip(table, cls, fam, "Idle", 0).tolist())
            wide = gc.wide(ASSET[fam], facing=face,
                           text=f"{ASSET[fam]} ON SK_HERO_{cls.upper()} (READY, {face.upper()})  W1 640X360")
        files.append(IK.save(out_dir, f"game_inhand_{cls}.png", review.grid(cells, len(cells), pad=2)))
        reports += gc.reports
        if fam == "bow":
            bpy.data.objects.remove(line_obj)
        IK.hero_pose(arm, cls, "Idle", 0)
        arm.rotation_euler = (0, 0, 0)
    heroes.show(None)
    IK.detach(obj)
    obj.hide_render = True
    v = review.ink_verdict(reports)
    img0, st0, gc0 = first
    return files, v, (wide, gc0.inkmap(img0, st0))


# ── ship ────────────────────────────────────────────────────────────────────────────────────────────────
def clip_names() -> dict[str, list[str]]:
    man = json.loads(paths.manifest_path().read_text())
    return {cls: [a["name"] for a in man["assets"][f"SK_Hero_{cls}"]["anims"]] for cls in HEROES}


def manifest_extra(fam: str, variants, table: dict, qa: dict | None = None) -> dict:
    bases = [b for b in IK.weapon_bases() if b.get("weaponType") == fam]
    tiers = {"T1": f"MI_AF_Toon_{FAMILY_PAL}"}
    for v in variants:
        tiers[v.variant] = v.material_name
    order = ("Worn",) + IK.TIERS
    used = sorted({IK.weapon_tier(b) for b in bases}, key=order.index)
    e = {
        "weaponType": fam, "attachSocket": SOCKET[fam], "attachKind": "hand", "heldBy": [],
        "tiers": tiers, "tierNames": {k: WT.TIER_NAMES[k] for k in tiers},
        "bases": {b["id"]: IK.weapon_tier(b) for b in bases},
        "tierRule": "item base levelReq >= 1 T1 · 6 T2 · 15 T3 · 20 T4 · 28 T5 · 35 T6; w_rusty_sword Worn",
        "tiersUsed": used,
        # per hero class: the socket-relative grip (carry) + per-clip overrides (strike / dodge / death)
        "grips": WG.manifest_grips(table, fam, clip_names()),
        "gripBlendMs": WG.BLEND_MS,
        "note": ("generic family mesh (I6 / R4) for every hero whose class has no own SM_Hero_* weapon of this type "
                 "(grips[cls].shown). On attach to attachSocket set the component's relative transform to "
                 "grips[<class>].default (locCm / rotDeg = FRotator, quat = FQuat XYZW, socket space); when a clip "
                 "starts, blend it over gripBlendMs to grips[<class>].clips[<clip>] if listed, else back to the "
                 "default. A clip entry with 'keys' is a keyed grip track ({ms, locCm, rotDeg, quat} at clip "
                 "time; slerp / lerp between keys, held before the first and after the last; sample at the "
                 "montage / sequence position, so play rate scales it); grips[<class>].socket overrides "
                 "attachSocket (slung: carried on that spine bone, not in the hand); hideMs {clip: [ms0, ms1]} "
                 "hides the mesh in that window. Swap material slot 0 to tiers[bases[<equipped base id>]] (MI over "
                 "T_AF_Palette_Weapons_<Tier>); slot 1 stays M_AF_Outline (weapon ink 3.5 px)"
                 + ("; no item base uses this family yet (core WeaponType enum)" if not bases else "")),
    }
    if fam == "bow":
        e["occupiesOffhand"] = True
        e["note"] += ("; occupiesOffhand: the bow is a main-hand ('weapon' slot) base held in the left hand — hide "
                      "the class default off-hand (warrior shield, rogue off dagger) and show no main-hand mesh "
                      "while it is equipped")
    if qa is not None:
        e["inHandQA"] = qa
    return e


def qa_records(res: dict, fam: str) -> dict:
    """Manifest ``inHandQA`` for one family: per class the whole-clip worst values + the limits."""
    out = {"limits": WG.LIMITS, "frames": "every frame of every SK_Hero_<Class> clip (shipped FBX)", "classes": {}}
    for cls, fams in res["classes"].items():
        clips = fams.get(fam)
        if not clips:
            continue
        out["classes"][cls] = {
            "shown": WG.shown(cls, fam), "pass": all(WG.GripQA.passes(w) for w in clips.values()),
            "lowestCm": round(min(w["lowCm"] for w in clips.values()), 1),
            "lowestClip": min(clips, key=lambda k: clips[k]["lowCm"]),
            "bodyPairs": max(w["body"] for w in clips.values()), "clothPairs": max(w["cloth"] for w in clips.values()),
            "headPairs": max(w["head"] for w in clips.values()), "otherItemPairs": max(w["other"] for w in clips.values()),
            "faceFrames": sum(w.get("faceFrames", 0) for w in clips.values())}
        clr = [w["clearCm"] for w in clips.values() if "clearCm" in w]
        if clr:       # bows: limbs + string off the rigid body (WG.CLEAR)
            out["classes"][cls]["minClearCm"] = round(min(clr), 1)
    return out


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
    table = WG.load_table()
    # whole-clip in-hand QA at the shipped grips (art review: failing the ship like the warrior's QA does)
    heroes = Heroes()
    posed = {cls: WG.Posed(cls, heroes.arm(cls), heroes.body(cls)) for cls in HEROES}
    qa = WG.qa_all(table, {f: WG.Item.from_object(f, objs[f][0]) for f in fams}, posed)
    print(WG.qa_table(qa))
    for f in qa["info"]:
        print("  [qa] info", f)
    for f in qa["failures"]:
        print("  [qa] FAIL", f)
    entries = {}
    for fam in fams:
        obj, socks = objs[fam]
        e = IK.ship_static_item(obj, ASSET[fam], CATEGORY, pal, "weapon", socks,
                                extra=manifest_extra(fam, variants, table, qa_records(qa, fam)), palettes=variants)
        entries[fam] = e
        print(f"[weapons] {ASSET[fam]}: toon {e['triangles']['toon']} hull {e['triangles']['outline']} tris, "
              f"bounds {e['boundsCm']}")
    result = {"assets": {ASSET[f]: entries[f]["triangles"] for f in fams}, "ink": {}, "qa": qa["failures"]}
    if previews:
        upd = {}
        for fam in fams:
            obj, _ = objs[fam]
            out = paths.preview_dir(ASSET[fam])
            for old in out.glob("game_inhand*.png"):
                old.unlink()
            sheet = review.grid(studio_set(fam, obj), 3)
            files = [IK.save(out, "closeup.png", sheet)]
            files.append(IK.save(out, "tiers.png", tier_sheet(fam, obj, pal, variants)))
            inh, v, (wide, imap) = inhand(fam, obj, heroes, out, table)
            files += inh
            files.append(IK.save(out, "game.png", wide))
            files.append(IK.save(out, "game_inkgate.png", imap))
            print(f"[ink] {ASSET[fam]}: {v['cells']} cells, min rim {v['minFracRim'] * 100:.1f} %, dark "
                  f"{v['minFracDark'] * 100:.1f} %, median {v['medianRimPx']} px -> {'PASS' if v['pass'] else 'FAIL'}")
            for f in v["failures"]:
                print("   ", f)
            result["ink"][ASSET[fam]] = v["pass"]
            upd[ASSET[fam]] = {"previews": [str(p.relative_to(paths.preview_root())) for p in files],
                               "inkGate": IK.ink_entry(v)}
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
    right = Vector((-1.0, -1.0, 0.0)).normalized()      # screen right for the W1 yaw (Blender axes)
    x = 0.62
    for fam in order:
        if fam not in objs:
            continue
        o = objs[fam][0]
        p = right * x
        if fam == "shield":
            m = Matrix.Translation((p.x, p.y, 0.32)) @ Matrix.Rotation(math.radians(45), 4, "Z")
        elif fam == "bow":
            m = Matrix.Translation((p.x, p.y, 0.49)) @ Matrix.Rotation(math.radians(135), 4, "Z")
        else:
            lo = min(v.co.y for v in o.data.vertices)
            m = (Matrix.Translation((p.x, p.y, -lo + 0.02)) @ Matrix.Rotation(math.radians(135), 4, "Z")
                 @ Matrix.Rotation(math.radians(90), 4, "X"))
        IK.detach(o, m)
        keep.append(o)
        x += 0.40 if fam not in ("shield",) else 0.5
    bpy.context.view_layer.update()
    with IK.only_visible(keep):
        img = IK.studio(keep, res=(1200, 420), pitch=-14.0, ink_px=1.8, fill=0.94,
                        label="SM_WEAPON_(FAMILY) T1 BESIDE SK_HERO_WARRIOR 1.76 M")
    IK.save(out, "lineup.png", img)
    for fam in order:
        if fam in objs:
            IK.detach(objs[fam][0])
    heroes.show(None)
    arm.rotation_euler = (0, 0, 0)


def quick(fams: list[str]) -> None:
    """Mesh iteration: studio + tiers + in-hand cells (at the current grips) → scratch (no export, no manifest)."""
    scene.reset()
    pal, base, variants = palettes()
    pal.save()
    for v in variants:
        v.save()
    heroes = Heroes()
    table = WG.load_table()
    for fam in fams:
        obj, _ = build(fam, pal, base)
        obj.hide_render = True
        out = IK.SCRATCH / fam
        print(f"[quick] {fam}: toon {M.tri_count(obj, 0)} hull {M.tri_count(obj, 1)}")
        IK.save(out, "closeup.png", review.grid(studio_set(fam, obj), 3))
        IK.save(out, "tiers.png", tier_sheet(fam, obj, pal, variants))
        files, v, (wide, imap) = inhand(fam, obj, heroes, out, table)
        IK.save(out, "game_inkgate.png", imap)
        print(f"[quick] {fam} ink {'PASS' if v['pass'] else 'FAIL'} min rim {v['minFracRim']:.3f} "
              f"dark {v['minFracDark']:.3f} {v['failures'][:3]}")


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="")
    ap.add_argument("--quick", default="")
    ap.add_argument("--no-previews", action="store_true")
    ap.add_argument("--no-verify", action="store_true")
    ap.add_argument("--lineup", action="store_true", help="only re-render Previews/weapons_lineup (no export)")
    a = ap.parse_args(argv)
    if a.lineup:
        scene.reset()
        pal, base, _ = palettes()
        objs = {f: build(f, pal, base) for f in WT.FAMILIES}
        lineup(objs, Heroes())
        return 0
    if a.quick:
        quick([f for f in a.quick.split(",") if f])
        return 0
    fams = [f for f in (a.only.split(",") if a.only else WT.FAMILIES) if f]
    r = ship(fams, previews=not a.no_previews, verify=not a.no_verify)
    bad = [k for k, ok in r["ink"].items() if not ok] + (r.get("verify") or []) + r["qa"]
    return 1 if bad else 0


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    sys.exit(main(argv))

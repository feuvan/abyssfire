"""渊火骑士 Abyssfire Knight — ``SK_Hero_Warrior`` (spec art-inventory-ch1.md §3.1, §3.4, §3.5; DECISIONS R2/R4/R5).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/heroes/warrior.py [--quick] [--no-previews]

Builds the plate knight (great helm with the ember T-visor and a crimson crest, layered pauldrons, cuirass with the
chest ember sigil, mail skirt, crimson tabard and two-sided cape with gold hems), the ``SKEL_Human`` rig with cape /
tabard / plume chains, proximity skin weights and the baked outline hull; the broadsword and heater shield as
separate ``Weapons/`` static meshes on ``weapon_r`` / ``weapon_l``; every clip of spec §3.4 + DECISIONS R5
(Idle, Walk, Run, Attack01-03, Cast01-02, Cast_Whirlwind, Cast_Charge, Hurt, HurtAdd, Dodge, Death, Portrait).
Exports ``Art/Export/Characters/SK_Hero_Warrior.fbx`` (+ ``SK_Hero_Warrior_LOD1.fbx``) + ``A_Hero_Warrior_<Clip>.fbx``,
``Art/Export/Weapons/SM_Hero_Warrior_{Sword,Shield}.fbx``, manifest entries, and the review set in
``Art/Previews/hero_warrior/``.

Web source: ``src/graphics/sprites/players/PlayerWarrior.ts`` (palette :56-66, proportions :137-142, helm rings
:365-372, cuirass :155-176, cape :315-342, poses :558-737). Web rig units → metres with ``U`` (3.01 cm × fit).
"""
from __future__ import annotations

import json
import math
import sys
import time
from dataclasses import replace
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))

import bpy  # noqa: E402  (before bmesh: the bpy module registers it)
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Quaternion, Vector  # noqa: E402

import kit  # noqa: E402
from kit import anim, asset, export, mesh as M, outline, paths, pngio, review, rig as R, scene, shading  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402
from kit.rig import Bind, HumanoidSpec  # noqa: E402

import common as C  # noqa: E402
from common import BP, K, V, Profile  # noqa: E402

ASSET = "SK_Hero_Warrior"
TOKEN = "Hero_Warrior"
SWORD = "SM_Hero_Warrior_Sword"
SHIELD = "SM_Hero_Warrior_Shield"
HEIGHT = 1.76
U0 = 0.0301                       # web warrior rig unit (spec §0.1)
# Camera-aware proportion tune (review: at the W1 pitch −50° the body foreshortens and the helm does not, so the
# web's ≈ 3.5-head read became ≈ 2.4): helm loft × 0.92 about the head centre, the freed crown height given to
# the torso (+0.3 u) and legs (+0.15 u each). Crown stays 176 cm (R2); model space now ≈ 4.1 heads.
HELM_K = 0.92
TORSO_ADD, LEG_ADD = 0.30, 0.15

# ── palette (spec §3.1; (s, l) = tone shadow / light) ──────────────────────────────────────────────────
STEEL = Region("#9AA6BA", .42, .45)
IRON = Region("#4B5366")
REGIONS = dict(
    steel=STEEL,
    steel_hi=Region("#C8C9C7", .42, .40),          # breastplate ridge / knuckle plates (steel light tone)
    steel_line=STEEL.line_region(),                # seams
    # painted line art (art review 6, spec §3.1 / §1.3): flat strips that follow the plate, so they band with it
    steel_seam=STEEL.shade_region(),               # breastplate side seams, back ridge (web STEEL.shade strokes)
    iron_seam=IRON.shade_region(),                 # mail-skirt rows (web IRON.shade strokes)
    iron=IRON,
    iron_dark=Region("#363C4B"),                   # shield back rim, sabaton lames
    gold=Region("#D9A640", .42, .50),
    crimson=Region("#A82230"),
    crimson_in=Region("#62131D", .42, .15),
    crimson_line=Region("#A82230").line_region(0.25),  # cape fold creases (web CRIMSON.shade strokes)
    leather=Region("#5E3A22"),
    leather_dark=Region("#5E3A22").line_region(0.3),
    visor=Region("#0C0A12", .30, .10),
    # visor ember (art review 6: the focal glow must pop at 1080p): a 4.2 cm ember inside the slit, graded from
    # the #FF8A2A falloff through #FFB45C to a #FFD08A core (web ember line rgb(255,150–210,60) + glow #FF8A2A)
    ember=Region("#FF8A2A", .0, .0, e=1.0),
    ember_mid=Region("#FFB45C", .0, .0, e=1.0),
    ember_hot=Region("#FFD08A", .0, .0, e=1.0),
    sigil=Region("#FF8A2A", .0, .0, e=0.9),        # chest ember sigil rgba(255,138,42,.9)
)
SWORD_REGIONS = dict(
    # blade shadowAmt .15 (art review 6): with the spec's .30 a flat turned from the key light dropped to a dark iron
    # bar (#5A5E6E); .15 keeps the shade band mid-light steel, the bevelled edges keep a lit facet
    blade=Region("#C9D3E2", .15, .60),
    fuller=Region("#465064", .30, .20),            # fuller line rgba(70,80,100,.8)
    gold=Region("#D9A640", .42, .50),
    leather=Region("#5E3A22"),
    gem=Region("#FF7A26", .0, .0, e=1.0),          # pommel gem
)
SHIELD_REGIONS = dict(
    gold=Region("#D9A640", .42, .50),
    crimson=Region("#A82230"),
    core=Region("#FFCF6B", .30, .50),              # emblem hot core
    iron_dark=Region("#363C4B"),
    leather=Region("#5E3A22"),
    leather_dark=Region("#5E3A22").line_region(0.3),
)


# hull colour = mix(#120C18 ink, line(c), o): o = 0.4 → 60 % ink, the web's heavy ink silhouette (R3 allows the
# darkened local colour; spec §1.3 asks ink for heroes) — the hull still tints toward crimson / gold / steel
HULL_INK_O = 0.4
REGIONS, SWORD_REGIONS, SHIELD_REGIONS = ({k: replace(r, o=HULL_INK_O) for k, r in d.items()}
                                          for d in (REGIONS, SWORD_REGIONS, SHIELD_REGIONS))


# ── rig ────────────────────────────────────────────────────────────────────────────────────────────────
class Dims:
    """Rest landmarks (metres) shared by the mesh, the chains and the poses."""

    def __init__(self):
        base = HumanoidSpec(name=ASSET, skeleton="SKEL_Human", thigh=(12.5 + LEG_ADD) * U0,
                            shin=(12 + LEG_ADD) * U0, ankle=2.6 * U0, torso=(16.5 + TORSO_ADD) * U0, neck=7.2 * U0,
                            head=15.4 * HELM_K * U0, hip_half=0.100,
                            shoulder_half=5.6 * U0, upper_arm=10 * U0, fore_arm=9.5 * U0, hand=0.112,
                            foot_len=0.27)
        self.k = HEIGHT / base.crown_height()
        self.spec = base.fit_height(HEIGHT)
        s = self.spec
        self.U = U0 * self.k
        self.hip_z = s.ankle + s.shin + s.thigh
        self.pz = self.hip_z + s.pelvis_offset
        self.nz = self.pz + s.torso
        self.hc = self.nz + s.neck          # head centre (helm centre)
        self.ts = (16.5 + TORSO_ADD) / 16.5  # cuirass stretch along the longer torso


D = Dims()
U = D.U


def u(n: float) -> float:
    return n * U


def uh(n: float) -> float:
    """Helm-relative web units (the helm loft and everything on it is scaled by ``HELM_K``)."""
    return n * U * HELM_K


def helm_profile() -> Profile:
    hc = D.hc
    rings = [(hc - uh(7.2), uh(4.3), uh(4.4), -uh(1.3)), (hc - uh(6.3), uh(5.2), uh(5.5), -uh(1.1)),
             (hc - uh(5.2), uh(5.7), uh(6.2), -uh(0.9)), (hc - uh(3.4), uh(6.0), uh(6.7), -uh(0.6)),
             (hc - uh(1.5), uh(6.15), uh(7.0), -uh(0.3)), (hc + uh(1.0), uh(6.15), uh(6.95), -uh(0.1))]
    # elliptic dome above h 3 (web rings h 3 → 8.2)
    for i in range(0, 9):
        f = i / 8.0
        h = 3.0 + f * 5.45
        k = math.sqrt(max(0.0, 1 - f ** 2.2)) if i < 8 else 0.12
        rings.append((hc + uh(h), uh(6.05) * k, uh(6.85) * k, 0.0))
    return Profile(rings, exp=2.15)


def VISOR_Z() -> float:
    """Centre of the horizontal eye slit (the ``visor`` socket / glow anchor)."""
    return D.hc + uh(0.10)


# Eye slit (art reviews 5 / 6): a dark #0C0A12 slit filled by a **4.2 cm ember** — #FF8A2A falloff → #FFB45C →
# #FFD08A core (1.4 cm), tapering toward the slit ends — so at 1080p 1:1 it reads as an ember-lit T-visor (the
# 1.5 cm line of review 5 read as a thin gold line). 0.8 cm of dark lip above and below keep it a slit, not a
# second gold band; ≥ 3 cm of steel separate it from the brow band. Emissive (bloom) + the ``visorGlow`` card.
VISOR_SLIT_H = 0.058
VISOR_EMBER_H = 0.042
VISOR_MID_H = 0.026
VISOR_CORE_H = 0.014
BROW_Z, BROW_H = 3.00, 1.30   # brow band centre / height (helm units, ``uh``)


def visor_ember_z() -> float:
    return VISOR_Z()


def cuirass_profile() -> Profile:
    pz = D.pz
    return Profile([(pz + u(0.4) * D.ts, u(4.9), u(3.75), 0.0), (pz + u(1.2) * D.ts, u(5.0), u(3.8), 0.0),
                    (pz + u(3.4) * D.ts, u(4.75), u(3.55), -u(0.05)), (pz + u(7.0) * D.ts, u(5.25), u(4.05), -u(0.5)),
                    (pz + u(11.5) * D.ts, u(6.3), u(4.7), -u(0.8)), (pz + u(14.4) * D.ts, u(6.55), u(4.45), -u(0.35)),
                    (pz + u(16.0) * D.ts, u(5.3), u(3.6), -u(0.1)), (pz + u(17.1) * D.ts, u(3.6), u(2.9), 0.0)], exp=2.35)


def skirt_profile() -> Profile:
    pz = D.pz
    return Profile([(pz + u(2.6), u(5.0), u(3.8), 0.0), (pz + u(0.0), u(5.45), u(4.2), 0.0),
                    (pz - u(1.5), u(5.8), u(4.5), 0.0), (pz - u(3.0), u(6.1), u(4.8), u(0.1)),
                    (pz - u(4.2), u(6.35), u(5.05), u(0.2))], exp=2.3)


# pleat depth at the hem (art review 6: 3.4 cm pleats + rigid chains broke the cape into stacked boards in fast
# motion; with the smooth chain weights of ``cape_weights`` 2.0 cm keeps the folds readable without separate planks)
CAPE_PLEAT = 0.020
# cloth hull proxies (cape, tabards) end in a pinched "lens" rim this far beyond the cloth edge, so the smoothed
# normals fan round every edge. 7.5 mm (0.9 px) until the outline WPO became a screen-space dilation (art review 6):
# a push along N needed it to ink a sheet seen at a grazing angle; the screen-space ink does not, and 7.5 mm made
# the cape / tabard edges read ~0.9 px heavier than the rest of the silhouette
CAPE_LENS = 0.002
CAPE_LEN = 0.735
CAPE_CHAIN_UU = 0.72          # lateral position of the cape_l / cape_r chains (uu)


def cape_frame():
    """Cape surface S(uu, k, d): uu ∈ [−1, 1] across (+ = left), k ∈ [0, 1] top → hem, d = offset along the
    outward (away from the body) normal. Web capeGeometry: 25 u long, half-width 8 → 10.6 u, U-section curl."""
    ztop = D.nz - 0.028
    # centre line (z, y) draped over the shoulder blades, then hanging (idle flow ≈ 6° back)
    ctl = [(ztop, 0.118), (ztop - 0.10, 0.150), (ztop - 0.25, 0.168), (ztop - 0.45, 0.185),
           (ztop - 0.62, 0.200), (ztop - 0.735, 0.210)]
    zs = [c[0] for c in ctl][::-1]
    ys = [c[1] for c in ctl][::-1]
    L = 0.735

    def S(uu: float, k: float, d: float = 0.0) -> Vector:
        z = ztop - L * k
        y = C._hermite(zs, ys, z)
        w = u(7.4) + u(2.2) * k
        curl = u(3.0) * (1 - k) ** 2 + u(1.6) + u(1.8) * k
        fold = (0.005 + CAPE_PLEAT * k) * math.cos(3 * math.pi * uu)
        x = uu * w
        yy = y - curl * uu * uu + fold
        zz = z - 0.02 * uu * uu * (1 - k) ** 3 + 0.032 * (1 - uu * uu) * (1 - k) ** 6
        if k > 0.999:
            zz += 0.012 * math.sin(2.4 * math.pi * uu)
        p = Vector((x, yy, zz))
        if d:
            e = 1e-3
            du = S(min(1.0, uu + e), k) - S(max(-1.0, uu - e), k)
            k2 = min(1.0, k + e)
            k1 = max(0.0, k - e)
            dk = S(uu, k2) - S(uu, k1)
            n = dk.cross(du)
            if n.y < 0:
                n = -n
            p += n.normalized() * d
        return p
    return S


CAPE = cape_frame()


def build_spec() -> HumanoidSpec:
    spec = D.spec
    # cape chains (centre + edges), tabard front/back, plume (spec §2.4 names)
    cape_c = [CAPE(0.0, k) + Vector((0, -0.004, 0)) for k in (0.0, 0.25, 0.5, 0.75, 1.0)]
    cape_l = [CAPE(0.72, k) for k in (0.0, 1 / 3, 2 / 3, 1.0)]
    cape_r = [CAPE(-0.72, k) for k in (0.0, 1 / 3, 2 / 3, 1.0)]
    ztab = D.pz - 0.005
    tab_f = [Vector((0, -0.142, ztab)), Vector((0, -0.160, ztab - 0.22)), Vector((0, -0.176, ztab - 0.455))]
    tab_b = [Vector((0, 0.150, ztab)), Vector((0, 0.170, ztab - 0.19)), Vector((0, 0.186, ztab - 0.375))]
    hc = D.hc
    pl = [Vector((0, -uh(1.0), hc + uh(8.4))), Vector((0, uh(4.6), hc + uh(10.2))), Vector((0, uh(9.6), hc + uh(8.4))),
          Vector((0, uh(14.4), hc + uh(4.6)))]
    spec.chains = [("cape_c", "spine_03", cape_c), ("cape_l", "spine_03", cape_l), ("cape_r", "spine_03", cape_r),
                   ("tabard_f", "pelvis", tab_f), ("tabard_b", "pelvis", tab_b), ("plume", "head", pl)]
    return spec


# Trims without a hull of their own (bands, seams, rivets, hems) stand at most this proud of the part whose hull
# inks them (art review 6: a 7 mm brow band, 1.6 cm rivets and an 11 mm pauldron rim ate up to 1.3 px of the
# 3.5 px silhouette ink at 1080p wherever they reached the outline)
TRIM_PROUD = 0.004


# ── mesh helpers ───────────────────────────────────────────────────────────────────────────────────────
def profile_mesh(prof: Profile, zs, sides: int, disp=None, exp: float | None = None, cap0: str | None = "fan",
                 cap1: str | None = "fan", dome0: float = 0.0, dome1: float = 0.0, a_offset: float = 0.0,
                 extra_angles=()) -> M.Part:
    """Loft through ``prof`` at heights ``zs`` (bottom → top) with an optional radial displacement
    ``disp(z, a) → metres`` (grooves, keels, mail ridges)."""
    bm = bmesh.new()
    rings = []
    angs = sorted(set([round((a_offset + 360.0 * i / sides) % 360.0, 4) for i in range(sides)]
                      + [round(a % 360.0, 4) for a in extra_angles]))
    sides = len(angs)
    dl = bm.verts.layers.int.new("af_disp")
    for z in zs:
        ring = []
        for a in angs:
            d = disp(z, ((a + 180.0) % 360.0) - 180.0) if disp else 0.0
            v = bm.verts.new(tuple(prof.point(z, a, d, exp)))
            v[dl] = 1 if d < -1e-6 else 0
            ring.append(v)
        rings.append(ring)
    # angles increase toward +X from the front: (front → left) is counter-clockwise seen from above
    for r0, r1 in zip(rings, rings[1:]):
        for i in range(sides):
            j = (i + 1) % sides
            bm.faces.new((r0[i], r0[j], r1[j], r1[i]))
    for cap, ring, dome, sgn, z in ((cap0, rings[0], dome0, -1, zs[0]), (cap1, rings[-1], dome1, 1, zs[-1])):
        if cap is None:
            continue
        rx, ry, cy, cx = prof.at(z)
        c = bm.verts.new((cx, cy, z + sgn * dome))
        for i in range(sides):
            j = (i + 1) % sides
            bm.faces.new((ring[i], ring[j], c) if sgn < 0 else (ring[j], ring[i], c))
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return M.Part(bm).smooth(True)


def _sheet(grid) -> M.Part:
    """Single-sided quad sheet from a rows × cols grid of points (faces wound so +normal = row × col order)."""
    bm = bmesh.new()
    vs = [[bm.verts.new(tuple(p)) for p in row] for row in grid]
    for r in range(len(vs) - 1):
        for c in range(len(vs[0]) - 1):
            bm.faces.new((vs[r][c], vs[r][c + 1], vs[r + 1][c + 1], vs[r + 1][c]))
    bm.normal_update()
    return M.Part(bm).smooth(True)


def _outward(part: M.Part, center_fn) -> M.Part:
    """Flip a sheet whose faces point toward ``center_fn(face centre)`` (the axis of the loft it lies on)."""
    f = next(iter(part.bm.faces))
    c = f.calc_center_median()
    if f.normal.dot(c - center_fn(c)) < 0:
        bmesh.ops.reverse_faces(part.bm, faces=list(part.bm.faces))
        part.bm.normal_update()
    return part


def painted_strip(prof: Profile, a: float, z0: float, z1: float, width: float, lift, steps: int = 10) -> M.Part:
    """A flat painted line on a loft: a single-sided sheet ``width`` metres across at angle ``a`` from ``z0`` to
    ``z1``, ``lift(z, a)`` metres off the profile (follow a displacement such as the keel). Its normal is the
    plate's, so the line takes the same toon band as the plate under it (web strokes); no hull, no back faces."""
    grid = []
    for i in range(steps + 1):
        z = z0 + (z1 - z0) * i / steps
        per_deg = (prof.point(z, a + 0.5) - prof.point(z, a - 0.5)).length      # arc length of 1° here
        da = width / 2 / max(per_deg, 1e-6)
        grid.append([prof.point(z, a + da * t, lift(z, a + da * t)) for t in (-1.0, 0.0, 1.0)])

    def axis(c):
        rx, ry, cy, cx = prof.at(c.z)
        return Vector((cx, cy, c.z))
    return _outward(_sheet(grid), axis)


def band_sheet(prof: Profile, z: float, height: float, out: float, a0: float, a1: float, segs: int = 20) -> M.Part:
    """Single-sided band on a loft between z ± height/2 and angles a0..a1, ``out`` metres off the surface."""
    grid = [[prof.point(z + dz, a0 + (a1 - a0) * i / segs, out) for i in range(segs + 1)]
            for dz in (-height / 2, 0.0, height / 2)]

    def axis(c):
        rx, ry, cy, cx = prof.at(c.z)
        return Vector((cx, cy, c.z))
    return _outward(_sheet(grid), axis)


def ell(r, center, sides=16, rings=9, exp=2.0, exp_v=2.0) -> M.Part:
    """kit ellipsoid. Use an **even** ``rings`` for any part (or hull proxy) whose equator can form the silhouette:
    odd counts have no equator ring, so the widest ring is 2–5 % inside the radius — a hull proxy built that way
    sat millimetres inside its part and thinned the ink (art review 6)."""
    return M.ellipsoid(r, center, sides=sides, rings=rings, exp=exp, exp_v=exp_v)


def limb_frame(a: Vector, b: Vector, side_hint: Vector = Vector((0, -1, 0))) -> Matrix:
    return R.frame(a, b - a, side_hint)


# ── body ───────────────────────────────────────────────────────────────────────────────────────────────
# LOD1 (spec §1.9: hero 7 k): the same generator with every part collapse-decimated, and the details that sit
# under the readability floor at LOD1 distances left out
LOD1_RATIO = 0.50
LOD1_SKIP = ("brow_rivet", "nape_rivet", "pauldron_rivet", "knee_rivet", "breath", "plume_strand", "seam_f",
             "seam_b", "cape_crease", "buckle_hole", "buckle_tongue", "sabaton_lame",
             "shin_ridge", "clasp", "couter_wing", "knee_wing", "rere_lame", "thumb", "knuckles")


def build_body(pal: Palette, rig: R.Rig, lod: int = 0) -> bpy.types.Object:
    J = rig.joints
    if lod:
        b = M.Builder(ASSET, pal, prefix="warrior", lod_ratio=LOD1_RATIO,
                      lod_skip=lambda n: n.rstrip("_lr").startswith(LOD1_SKIP))
    else:
        b = M.Builder(ASSET, pal, prefix="warrior")
    b.regions(**REGIONS)
    spine = ("pelvis", "spine_01", "spine_02", "spine_03")
    hc = D.hc

    # ── helm: lofted great helm with a T-visor groove, ember line, brow band, crest seam, breaths, rivets
    hp = helm_profile()
    # visor (review: the ember must read from the −50° camera): slit raised + taller, shallow recess so its
    # upper wall hides little of the floor, a 4 cm ember line with a hot core, brow band less proud
    slit_z, slit_h, slit_a = VISOR_Z(), VISOR_SLIT_H, 54.0
    bar_a, bar_z0 = 6.0, hc - uh(4.7)
    depth = -0.0055

    def helm_disp(z, a):
        if abs(z - slit_z) < slit_h / 2 and abs(a) < slit_a:
            return depth
        if bar_z0 < z < slit_z and abs(a) < bar_a:
            return depth
        return 0.0
    eps = 0.0012
    zs = sorted(set([round(z, 5) for z in np.linspace(hp.z0, hp.z1, 15)] + [
        slit_z - slit_h / 2 - eps, slit_z - slit_h / 2 + eps, slit_z + slit_h / 2 - eps, slit_z + slit_h / 2 + eps,
        bar_z0 - eps, bar_z0 + eps]))
    cols = [sg * (a0 + da) for sg in (1, -1) for a0 in (slit_a, bar_a) for da in (-0.5, 0.5)]
    helm = profile_mesh(hp, zs, 40, disp=helm_disp, cap1="fan", dome1=0.004, cap0="fan", dome0=-0.02,
                        extra_angles=cols).auto_sharp(60)
    shell, groove = C.split_faces(helm, lambda bm, f: any(v[bm.verts.layers.int["af_disp"]] for v in f.verts))
    # hull: the plain helm loft at lower resolution (the slit columns / rows add nothing to the silhouette)
    helm_hull = profile_mesh(hp, list(np.linspace(hp.z0, hp.z1, 11)), 28, cap1="fan", dome1=0.004, cap0="fan",
                             dome0=-0.02)
    # (art review 6, ink gate) the hull proxy is padded at build time until it encloses the helm and every trim on
    # it (brow band, seams, rivets, breaths): nothing on the helm stands proud of the silhouette ink
    helm_id = b.add(shell, "steel", Bind.rigid("head"), name="helm", hull=helm_hull, hull_pad=True)
    b.add(groove, "steel", Bind.rigid("head"), name="helm_visor", outline=False)   # concave: no hull
    # visor slit floors (dark) + the ember line glowing in the horizontal slit (web: rgb(255,150-210,60))
    b.add(C.surface_band(hp, slit_z, slit_h * 0.995, depth + 0.0015, -slit_a - 0.4, slit_a + 0.4, segs=20,
                         out0=-0.016), "visor", Bind.rigid("head"), name="visor_slit", outline=False)
    b.add(C.surface_strip(hp, 0.0, bar_z0 - 0.001, slit_z, math.radians(bar_a) * 2 * uh(7.0) + 0.002,
                          depth + 0.0015, steps=6, out0=-0.016), "visor", Bind.rigid("head"), name="visor_bar",
          outline=False)
    ez = visor_ember_z()
    for nm, reg, hgt, a, lift in (("visor_ember", "ember", VISOR_EMBER_H, 47, 0.0030),
                                  ("visor_ember_mid", "ember_mid", VISOR_MID_H, 40, 0.0037),
                                  ("visor_core", "ember_hot", VISOR_CORE_H, 31, 0.0044)):
        b.add(band_sheet(hp, ez, hgt, depth + lift, -a, a, segs=16), reg, Bind.rigid("head"), name=nm,
              outline=False)
    # gold brow band (web h 2.6, 1.9 u line; raised a little and narrowed so a steel gap separates it from the
    # slit at game size) + steel rivets on it
    b.add(C.surface_band(hp, hc + uh(BROW_Z), uh(BROW_H), TRIM_PROUD, segs=40), "gold", Bind.rigid("head"),
          name="browband", outline=False, covered_by=helm_id)
    for a in (-58, -30, 30, 58):
        b.add(C.surface_dot(hp, hc + uh(BROW_Z), a, 0.0075, 0.0085, sides=6), "steel_hi", Bind.rigid("head"),
              name="brow_rivet", outline=False, covered_by=helm_id)
    # crest seam front + back (raised ridge running over the crown), nape rivets
    b.add(C.surface_strip(hp, 0.0, hc + uh(3.5), hp.z1 - 0.004, 0.016, TRIM_PROUD, steps=8, half_round=True),
          "steel", Bind.rigid("head"), name="seam_f", outline=False, covered_by=helm_id)
    b.add(C.surface_strip(hp, 180.0, hc - uh(6.6), hp.z1 - 0.004, 0.016, TRIM_PROUD, steps=12, half_round=True),
          "steel", Bind.rigid("head"), name="seam_b", outline=False, covered_by=helm_id)
    for z, a in ((hc - uh(1.5), 150), (hc - uh(1.5), -150), (hc - uh(4.0), 152), (hc - uh(4.0), -152)):
        b.add(C.surface_dot(hp, z, a, 0.009, 0.006, sides=6), "steel_hi", Bind.rigid("head"), name="nape_rivet",
              outline=False, covered_by=helm_id)
    # breaths: three small slots on each cheek
    for sg in (1, -1):
        for i in range(3):
            z = hc - uh(3.1) - i * uh(0.75)
            a0 = sg * (52 + i * 5)
            b.add(C.surface_band(hp, z, uh(0.38), 0.002, min(a0, a0 + sg * 13), max(a0, a0 + sg * 13), segs=3,
                                 out0=-0.003), "visor", Bind.rigid("head"), name="breath", outline=False,
                  covered_by=helm_id)
    # plume holder + horsehair crest streaming back (web viewPlume blob, ×1.15 fwd / ×1.08 up)
    top = hp.point(hp.z1 - 0.005, 0.0)
    holder = M.lathe([(0.026, -0.03), (0.024, 0.0), (0.020, 0.022), (0.012, 0.03)], sides=10)
    b.add(holder.rotate("x", -14).translate((0, top.y - uh(0.6), hp.z1 - 0.012)), "gold", Bind.rigid("head"),
          name="plume_holder")
    crest, cpts, chh, cww = plume_crest(hc)
    pbind = Bind.blend("head", "plume_01", "plume_02", "plume_03", falloff=3.0)
    plume_id = b.add(crest, "crimson", pbind, name="plume", hull_pad=True)
    for st in plume_strands(cpts, chh, cww):
        b.add(st, "crimson_in", pbind, name="plume_strand", outline=False, covered_by=plume_id)

    # ── gorget + cuirass (+ keel, ridge, seams, fauld band, ember sigil)
    b.add(M.loft_z([(D.nz - 0.04, u(4.7), u(3.5)), (D.nz, u(4.2), u(3.1)), (D.nz + u(1.6), u(3.4), u(2.5)),
                    (D.nz + u(2.4), u(3.0), u(2.2))], sides=24, exp=2.2), "iron",
          Bind.blend("spine_03", "neck_01"), name="gorget")
    cp = cuirass_profile()
    chest0, chest1 = D.pz + u(5.0) * D.ts, D.pz + u(15.5) * D.ts

    def keel(z, a):
        if chest0 < z < chest1:
            w = math.sin(math.pi * (z - chest0) / (chest1 - chest0)) ** 0.7
            return 0.010 * w * max(0.0, math.cos(math.radians(a))) ** 10
        return 0.0
    zs = list(np.linspace(cp.z0, cp.z1, 16))
    cuirass_id = b.add(profile_mesh(cp, zs, 32, disp=keel, cap0="fan", cap1="fan", dome1=0.01), "steel",
          Bind.blend(*spine), name="cuirass",
          hull=profile_mesh(cp, list(np.linspace(cp.z0, cp.z1, 10)), 24, disp=keel, cap0="fan", cap1="fan",
                            dome1=0.01), hull_pad=True)
    # painted line art (art review 6, web viewTorso): the breastplate ridge in the steel light tone, the back ridge
    # and the side seams (where the front and back plates meet) in the shade tone — flat strips ≥ 1.25 cm (1.5 px at
    # 1080p) lying on the plate (keel included), so each line bands with the plate under it
    def on_plate(z, a):
        return keel(z, a) + 0.0022
    b.add(painted_strip(cp, 0.0, D.pz + u(5.6) * D.ts, D.pz + u(15.0) * D.ts, 0.017, on_plate, steps=14),
          "steel_hi", Bind.blend(*spine), name="ridge", outline=False, covered_by=cuirass_id)
    b.add(painted_strip(cp, 180.0, D.pz + u(5.0) * D.ts, D.pz + u(15.0) * D.ts, 0.013, on_plate, steps=10),
          "steel_seam", Bind.blend(*spine), name="ridge_back", outline=False, covered_by=cuirass_id)
    for a in (64.0, -64.0):
        b.add(painted_strip(cp, a, D.pz + u(4.4) * D.ts, D.pz + u(13.8) * D.ts, 0.013, on_plate, steps=10),
              "steel_seam", Bind.blend(*spine), name="side_seam", outline=False, covered_by=cuirass_id)
    b.add(C.surface_band(cp, D.pz + u(4.4), u(0.85), 0.0045, segs=40), "gold", Bind.blend(*spine), name="fauld",
          outline=False, covered_by=cuirass_id)
    # chest ember sigil (web: flame on the breastplate, slightly to the near side)
    flame = C.smooth_closed([(0.0, 0.034), (0.013, 0.008), (0.018, -0.012), (0.006, -0.032), (-0.004, -0.030),
                             (-0.015, -0.014), (-0.012, 0.004), (-0.004, 0.0)], 4)
    sz = D.pz + u(10.4) * D.ts
    sp = cp.point(sz, -8.0, 0.016)
    sn = cp.normal(sz, -8.0)
    sig = C.plate2d(flame, 0.006).transform(R.frame(sp, Vector((0, 0, 1)), sn))
    # plate2d extrudes along local Z; frame maps local Y → up, Z → surface normal
    b.add(sig, "sigil", Bind.rigid("spine_02"), name="sigil", outline=False, covered_by=cuirass_id)

    # ── mail skirt, belt + buckle, tabards
    sk = skirt_profile()
    row_h = u(1.3)
    z_top = D.pz + u(2.4)

    def mail(z, a):
        f = ((z_top - z) / row_h) % 1.0
        return 0.006 * f ** 1.5
    zs = []
    z = z_top
    while z > sk.z0 + 1e-4:
        zs += [z, z - row_h * 0.08, z - row_h * 0.5]
        z -= row_h
    zs = sorted(set([round(v, 5) for v in zs if v > sk.z0] + [sk.z0]))
    # mail rows painted in the iron shade tone (art review 6, web: IRON.shade strokes every 1.3 u): the lower
    # 1.3 cm of each row (its overlapping lip) is a separate shade-tone face set on the same loft
    line_h = 0.013
    zs = sorted(set(zs + [round(z - row_h + line_h, 5) for z in np.arange(z_top, sk.z0, -row_h)
                          if z - row_h + line_h > sk.z0]))
    skirt_bind = Bind.blend("pelvis", "thigh_l", "thigh_r", smooth=0.06)

    def mail_line(bm, f) -> bool:
        zc = f.calc_center_median().z
        if f.normal.z < -0.7 or zc > z_top:                 # caps
            return False
        fr = ((z_top - zc) / row_h) % 1.0
        return fr > 1.0 - line_h / row_h
    rows_part, lines_part = C.split_faces(profile_mesh(sk, zs, 28, disp=mail, cap0="fan", cap1="fan"), mail_line)
    # one smooth hull over the mail rows, padded to enclose the rows, their shade lines and the belt
    skirt_id = b.add(rows_part, "iron", skirt_bind, name="mailskirt",
                     hull=profile_mesh(sk, list(np.linspace(sk.z0, max(zs), 6)), 20, disp=lambda z, a: 0.003,
                                       cap0="fan", cap1="fan"), hull_pad=True)
    b.add(lines_part, "iron_seam", skirt_bind, name="mail_rows", outline=False, covered_by=skirt_id)
    # belt + buckle (inside the skirt's padded hull)
    bp_ = Profile([(D.pz - 0.03, u(5.2), u(4.0)), (D.pz + 0.04, u(5.15), u(3.95))], exp=2.3)
    bz = D.pz + u(1.5)
    b.add(C.surface_band(bp_, bz, u(2.0), 0.016, segs=40, out0=-0.002), "leather", Bind.rigid("pelvis"),
          name="belt", outline=False, covered_by=skirt_id)
    ba = -20.0
    bpnt = bp_.point(bz, ba, 0.017)
    bn = bp_.normal(bz, ba)
    fr = R.frame(bpnt, Vector((0, 0, 1)), bn)
    buckle = M.box((u(3.0), u(3.2), 0.016), (0, 0, 0.004), bevel=0.004).transform(fr)
    buckle_id = b.add(buckle, "gold", Bind.rigid("pelvis"), name="buckle", hull_pad=True)
    b.add(M.box((u(1.7), u(1.9), 0.012), (0, 0, 0.0075), bevel=0.002).transform(fr), "leather_dark",
          Bind.rigid("pelvis"), name="buckle_hole", outline=False, covered_by=buckle_id)
    b.add(M.box((0.006, u(2.0), 0.01), (0, 0, 0.012), bevel=0.002).transform(fr), "gold",
          Bind.rigid("pelvis"), name="buckle_tongue", outline=False, covered_by=buckle_id)

    # front tabard: pointed V hem, two-sided (crimson / lining), gold V trim
    add_tabard(b, front=True)
    add_tabard(b, front=False)

    # ── cape (two-sided, folds, gold hem)
    add_cape(b)

    # ── arms + pauldrons
    for side, sx in (("l", 1.0), ("r", -1.0)):
        S, E, W = J[f"shoulder_{side}"], J[f"elbow_{side}"], J[f"wrist_{side}"]
        ua, la, hb = f"upperarm_{side}", f"lowerarm_{side}", f"hand_{side}"
        cl = f"clavicle_{side}"
        dua = (E - S).normalized()
        b.add(M.tube([S - dua * 0.02, E], [0.084, 0.074], sides=16), "iron", Bind.blend(cl, ua, la), name=f"ua_{side}")
        rere_id = b.add(M.tube([S + dua * 0.10, S + dua * 0.25, E - dua * 0.02], [0.089, 0.085, 0.080], sides=16,
                               dome0=0.0, dome1=0.0), "steel", Bind.rigid(ua), name=f"rerebrace_{side}", hull_pad=True)
        lame = C.surface_band(Profile([(-0.02, 0.0895, 0.0895), (0.02, 0.0885, 0.0885)]), 0.0, 0.012, 0.004, segs=24)
        b.add(lame.transform(_axis_frame(S + dua * 0.245, dua)), "iron", Bind.rigid(ua), name=f"rere_lame_{side}",
              outline=False, covered_by=rere_id)
        # couter + wing
        b.add(ell((0.066, 0.066, 0.066), E, sides=14, rings=8), "iron", Bind.blend(ua, la), name=f"couter_{side}",
              hull=ell((0.066, 0.066, 0.066), E, sides=10, rings=6), hull_pad=True)
        wing = ell((0.016, 0.052, 0.058), (0, 0, 0), sides=10, rings=6)
        out = Vector((sx, 0.25, 0)).normalized()
        b.add(wing.transform(Matrix.Translation(E + out * 0.058) @ _rot_to(Vector((1, 0, 0)), out)), "steel",
              Bind.blend(ua, la), name=f"couter_wing_{side}")
        dla = (W - E).normalized()
        b.add(M.tube([E + dla * 0.035, W - dla * 0.03], [0.079, 0.069], sides=16, dome0=0.0), "steel",
              Bind.blend(ua, la, hb, falloff=6.0), name=f"vambrace_{side}")
        # gauntlet cuff (bell) + fist
        cuff = M.loft_z([(-0.040, 0.066, 0.066), (-0.006, 0.073, 0.073), (0.016, 0.083, 0.081), (0.022, 0.081, 0.079)],
                        sides=16, cap0="fan", cap1="fan")
        b.add(cuff.transform(_axis_frame(W - dla * 0.012, dla)), "steel", Bind.blend(la, hb, falloff=6.0),
              name=f"cuff_{side}")
        add_fist(b, rig, side)
        add_pauldron(b, rig, side)

    # ── legs
    for side, sx in (("l", 1.0), ("r", -1.0)):
        Hp, Kn, A = J[f"hip_{side}"], J[f"knee_{side}"], J[f"ankle_{side}"]
        th, ca, ft, bl = f"thigh_{side}", f"calf_{side}", f"foot_{side}", f"ball_{side}"
        top = Hp + Vector((0, 0, 0.05))
        d = (Kn - top).normalized()
        b.add(M.tube([top, Kn], [(0.106, 0.096), (0.090, 0.082)], sides=18, up=(0, -1, 0)), "iron",
              Bind.blend("pelvis", th, ca), name=f"thigh_{side}")
        b.add(M.tube([top + d * 0.09, top + d * 0.25, Kn - d * 0.07], [(0.112, 0.100), (0.108, 0.097),
                                                                       (0.097, 0.087)],
                     sides=18, up=(0, -1, 0), dome0=0.0, dome1=0.0), "steel", Bind.rigid(th), name=f"cuisse_{side}")
        dk = (A - Kn).normalized()
        greave_id = b.add(M.tube([Kn + dk * 0.02, A + Vector((0, 0, 0.05))], [(0.090, 0.084), (0.070, 0.066)],
                                 sides=18, up=(0, -1, 0)), "steel", Bind.blend(th, ca, ft, falloff=6.0),
                          name=f"greave_{side}", hull_pad=True)
        b.add(C.surface_strip(_greave_profile(Kn, A), 0.0, A.z + 0.07, Kn.z - 0.05, 0.012, TRIM_PROUD, steps=5,
                              half_round=True), "steel_hi", Bind.blend(ca, ft, falloff=6.0), name=f"shin_ridge_{side}",
              outline=False, covered_by=greave_id)
        # knee cop + side wing + gold rivet
        kc = Kn + Vector((0, -0.05, 0.005))
        kcop_id = b.add(ell((0.074, 0.056, 0.076), kc, sides=14, rings=8, exp_v=2.2), "steel", Bind.blend(th, ca),
                        name=f"kneecop_{side}", hull_pad=True)
        b.add(ell((0.016, 0.055, 0.06), Kn + Vector((sx * 0.084, -0.012, 0.0)), sides=10, rings=6), "steel",
              Bind.blend(th, ca), name=f"knee_wing_{side}")
        b.add(ell((0.013, 0.010, 0.013), kc + Vector((0, -0.055, 0.004)), sides=6, rings=4), "gold",
              Bind.blend(th, ca), name=f"knee_rivet_{side}", outline=False, covered_by=kcop_id)
        # sabaton: oversized armoured boot with lames, toe cap on the ball bone
        boot = M.boot(D.spec.foot_len * 1.0, 0.142, 0.118, shaft=0.035, toe_up=0.018)
        boot_id = b.add(boot.translate((A.x, 0, 0)), "steel", Bind.blend(ca, ft, bl, falloff=5.0),
                        name=f"sabaton_{side}", hull_pad=True)
        for i, yy in enumerate((-0.050, -0.098)):
            hw, hh, cz = _boot_section(yy, D.spec.foot_len, 0.142, 0.118, 0.018)
            ring = M.sweep([(A.x, yy + 0.007, cz), (A.x, yy - 0.007, cz)], M.superellipse(1, 1, 12, 3.0),
                           scales=[(hh * 1.035, hw * 1.035)] * 2, up=(0, 0, 1), cap0="ngon", cap1="ngon")
            ring.deform(lambda co: Vector((co.x, co.y, max(co.z, 0.002))))
            b.add(ring.auto_sharp(50), "iron_dark", Bind.blend(ft, bl, falloff=5.0), name=f"sabaton_lame{i}_{side}",
                  outline=False, covered_by=boot_id)
    global LAST_PARTS
    LAST_PARTS = list(b.parts)
    names = [p["name"] for p in b.parts]
    body = asset.finish_mesh(b, pal, "hero", rig=rig, grounding_height=HEIGHT,
                             name=f"{ASSET}_LOD{lod}" if lod else None)
    body["af_parts"] = json.dumps(names)          # af_part index → part name (solvers, QA)
    body["af_lod"] = lod
    return body


LAST_PARTS: list = []


def part_report(parts) -> str:
    agg: dict = {}
    for p in parts:
        k = p["name"].rstrip("_lr").rstrip("0123456789")
        agg[k] = agg.get(k, 0) + p["tris"]
    return ", ".join(f"{k} {v}" for k, v in sorted(agg.items(), key=lambda kv: -kv[1]))


def plume_crest(hc: float) -> M.Part:
    """One swept crest with a lens section and a lobed (feathered) upper contour; spine and heights follow
    the web plume blob (fwd/up in rig units from the helm centre)."""
    spine = [(1.4, 8.1), (-0.6, 9.0), (-3.2, 9.3), (-6.0, 8.8), (-8.8, 7.5), (-11.4, 5.7), (-13.4, 4.2), (-14.6, 3.3)]
    half_h = [0.9, 1.45, 1.75, 1.8, 1.65, 1.35, 0.9, 0.25]
    half_w = [0.75, 0.95, 1.05, 1.0, 0.9, 0.72, 0.5, 0.15]
    path = [Vector((0.0, -uh(f) * 1.1, hc + uh(h))) for f, h in spine]
    n = 24
    # resample the spine (Catmull-Rom) for a smooth crest
    pts, hh, ww = [], [], []
    for i in range(n + 1):
        t = i / n * (len(path) - 1)
        j = min(int(t), len(path) - 2)
        f = t - j
        p0, p1, p2, p3 = path[max(j - 1, 0)], path[j], path[j + 1], path[min(j + 2, len(path) - 1)]
        f2, f3 = f * f, f * f * f
        q = 0.5 * ((2 * p1) + (-p0 + p2) * f + (2 * p0 - 5 * p1 + 4 * p2 - p3) * f2 + (-p0 + 3 * p1 - 3 * p2 + p3) * f3)
        lobe = 1.0 + 0.16 * max(0.0, math.sin(t / (len(path) - 1) * math.pi * 3.2)) ** 2
        pts.append(q)
        hh.append(uh(half_h[j] + (half_h[j + 1] - half_h[j]) * f) * lobe)
        ww.append(uh(half_w[j] + (half_w[j + 1] - half_w[j]) * f))
    prof = M.superellipse(1.0, 1.0, 12, 1.8)
    # sweep profile x → N (= +Z up for this path), y → lateral
    crest = M.sweep(pts, prof, scales=[(h, w) for h, w in zip(hh, ww)], up=(0, 0, 1), dome0=0.006, dome1=0.004)
    return crest, pts, hh, ww


def plume_strands(pts, hh, ww) -> list[M.Part]:
    """Horsehair strand grooves along both flanks of the crest (dark lining colour, no hull)."""
    out = []
    n = len(pts)
    for side in (1.0, -1.0):
        for j, (hf, s0, s1) in enumerate(((0.30, 2, n - 3), (-0.28, 3, n - 5))):
            path, rad = [], []
            for i in range(s0, s1):
                t = pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]
                t.normalize()
                lat = Vector((1, 0, 0))
                up = t.cross(lat).normalized()
                if up.z < 0:
                    up = -up
                y = hf * 1.0
                wscale = math.sqrt(max(0.0, 1 - y * y)) ** (2 / 1.8)
                path.append(pts[i] + up * (hh[i] * y) + lat * (side * ww[i] * wscale * 0.97))
                f = (i - s0) / max(1, s1 - s0 - 1)
                rad.append(0.0042 * math.sin(math.pi * min(1.0, 0.15 + 0.85 * f)) + 0.0012)
            out.append(M.tube(path, rad, sides=4, dome0=0.0, dome1=0.0))
    return out


def _greave_profile(Kn: Vector, A: Vector) -> Profile:
    return Profile([(A.z + 0.05, 0.068, 0.072, A.y, A.x), (Kn.z - 0.02, 0.084, 0.090, Kn.y, Kn.x)], exp=2.0)


def _boot_section(y: float, L: float, W: float, H: float, toe_up: float) -> tuple[float, float, float]:
    """(half-width, half-height, centre z) of kit.mesh.boot's section at ``y`` (same table as the kit)."""
    st = [(0.22 * L, 0.36 * W, 0.40 * H, 0.42 * H), (0.10 * L, 0.46 * W, 0.52 * H, 0.52 * H),
          (-0.15 * L, 0.50 * W, 0.46 * H, 0.46 * H), (-0.45 * L, 0.52 * W, 0.36 * H, 0.36 * H + toe_up * 0.3),
          (-0.68 * L, 0.46 * W, 0.28 * H, 0.28 * H + toe_up)]
    ys = [r[0] for r in st][::-1]
    return tuple(C._hermite(ys, [r[i] for r in st][::-1], y) for i in (1, 2, 3))


def _axis_frame(origin: Vector, axis: Vector) -> Matrix:
    """Frame whose +Z is ``axis`` (for loft_z parts placed along a limb)."""
    z = axis.normalized()
    x = z.orthogonal().normalized()
    y = z.cross(x)
    m = Matrix.Identity(4)
    for i in range(3):
        m[i][0], m[i][1], m[i][2], m[i][3] = x[i], y[i], z[i], origin[i]
    return m


def _rot_to(a: Vector, b: Vector) -> Matrix:
    return a.rotation_difference(b).to_matrix().to_4x4()


def add_fist(b: M.Builder, rig: R.Rig, side: str) -> None:
    J = rig.joints
    W, T = J[f"wrist_{side}"], J[f"handtip_{side}"]
    G = J[f"grip_{side}"]
    hd = (T - W).normalized()
    grip = Vector((0, -1, 0))                    # weapon bone +Y in rest
    palm = (Vector((0, 0, 0)) - G)
    palm.y = 0.0
    palm = (palm - hd * palm.dot(hd)).normalized()
    fr = Matrix.Identity(4)
    zx = grip - hd * grip.dot(hd)
    zx.normalize()
    xx = hd.cross(zx)
    for i in range(3):
        fr[i][0], fr[i][1], fr[i][2], fr[i][3] = xx[i], hd[i], zx[i], G[i]
    # fist: chunky rounded box around the grip (web fist r 2.8 × 2.6 u)
    fist = ell((0.063, 0.068, 0.069), (0, 0.014, 0), sides=14, rings=8, exp=2.8, exp_v=2.4)
    # one padded hull over the fist, the knuckle plate and the thumb (both stood ~2 cm proud of a fist-only hull:
    # the sword hand lost 2.5 px of its ink at 1080p)
    fist_id = b.add(fist.transform(fr), "iron", Bind.rigid(f"hand_{side}"), name=f"fist_{side}",
                    hull=ell((0.063, 0.068, 0.069), (0, 0.014, 0), sides=10, rings=6, exp=2.8, exp_v=2.4).transform(fr),
                    hull_pad=True)
    # knuckle plate on the back of the fingers (steel light) + thumb bump over the grip
    dorsal = -palm
    kp = ell((0.020, 0.044, 0.054), (0, 0, 0), sides=12, rings=6, exp=2.6)
    m = Matrix.Translation(G + dorsal * 0.046 + hd * 0.012) @ _basis(dorsal, hd)
    b.add(kp.transform(m), "steel_hi", Bind.rigid(f"hand_{side}"), name=f"knuckles_{side}", outline=False,
          covered_by=fist_id)
    th = M.capsule(G + zx * 0.05 + hd * -0.018 + palm * 0.02, G + zx * 0.062 + hd * 0.03 + palm * 0.0, 0.022, 0.018,
                   sides=8)
    b.add(th, "iron", Bind.rigid(f"hand_{side}"), name=f"thumb_{side}", outline=False, covered_by=fist_id)


def _basis(x_axis: Vector, y_axis: Vector) -> Matrix:
    x = x_axis.normalized()
    y = (y_axis - x * y_axis.dot(x)).normalized()
    z = x.cross(y)
    m = Matrix.Identity(4)
    for i in range(3):
        m[i][0], m[i][1], m[i][2] = x[i], y[i], z[i]
    return m


def add_pauldron(b: M.Builder, rig: R.Rig, side: str) -> None:
    J = rig.joints
    S = J[f"shoulder_{side}"]
    sx = 1.0 if side == "l" else -1.0
    cl, ua = f"clavicle_{side}", f"upperarm_{side}"

    def place(part: M.Part, center: Vector, tilt: float) -> M.Part:
        return part.rotate("y", -tilt * sx).translate(center)
    # web: three lames, top biggest (5.6 × 3.6 u), stacked 2.4 u apart, gold edge on the top one
    top_c = S + Vector((sx * 0.050, 0.004, 0.052))
    pid = b.add(place(ell((0.128, 0.132, 0.094), (0, 0, 0), sides=20, rings=10, exp=2.15, exp_v=2.3), top_c, 20),
                "steel", Bind.rigid(cl), name=f"pauldron_{side}",
                hull=place(ell((0.128, 0.132, 0.094), (0, 0, 0), sides=20, rings=8, exp=2.15, exp_v=2.3), top_c, 20),
                hull_pad=True)
    rim = C.surface_band(Profile([(-0.03, 0.128, 0.132), (0.03, 0.128, 0.132)], exp=2.15), -0.008, 0.022, 0.0015,
                         segs=24, out0=-0.006)
    # (flush, 1.5 mm proud: the rim runs round the pauldron's equator = its silhouette, where a proud band ate the ink)
    b.add(place(rim, top_c, 20), "gold", Bind.rigid(cl), name=f"pauldron_rim_{side}", outline=False, covered_by=pid)
    c2, c3 = S + Vector((sx * 0.084, 0.0, -0.010)), S + Vector((sx * 0.112, 0.0, -0.062))
    b.add(place(ell((0.112, 0.118, 0.058), (0, 0, 0), sides=18, rings=8, exp=2.15), c2, 33), "iron",
          Bind.blend(cl, ua), name=f"lame2_{side}",
          hull=place(ell((0.112, 0.118, 0.058), (0, 0, 0), sides=12, rings=6, exp=2.15), c2, 33), hull_pad=True)
    b.add(place(ell((0.096, 0.102, 0.050), (0, 0, 0), sides=16, rings=8, exp=2.15), c3, 44), "iron",
          Bind.rigid(ua), name=f"lame3_{side}",
          hull=place(ell((0.096, 0.102, 0.050), (0, 0, 0), sides=12, rings=6, exp=2.15), c3, 44), hull_pad=True)
    # rivet at the top
    b.add(ell((0.010, 0.010, 0.006), top_c + Vector((sx * 0.02, -0.07, 0.055)), sides=6, rings=4), "gold",
          Bind.rigid(cl), name=f"pauldron_rivet_{side}", outline=False, covered_by=pid)


def add_tabard(b: M.Builder, front: bool) -> None:
    """Front (crimson, 14.5 u, gold V trim) / back (lining colour, 12.5 u) tabard panel hanging from the belt."""
    ztop = D.pz - 0.002
    L = u(14.5) if front else u(12.5)
    half = u(3.4) if front else u(3.6)
    sgn = -1.0 if front else 1.0               # −Y front, +Y back
    y0 = sgn * (0.128 if front else 0.140)
    rows, cols = 9, 8
    chain = "tabard_f" if front else "tabard_b"

    def P(uu: float, k: float, d: float) -> Vector:
        z = ztop - k * (L - u(1.5)) - (u(3.0) * (1 - abs(uu)) * k ** 3 if front else u(1.6) * (1 - abs(uu)) * k ** 3)
        x = uu * (half + u(0.8) * k)
        y = y0 + sgn * (0.030 * k + 0.012 * k * k) - sgn * 0.035 * uu * uu * (1 - 0.5 * k) + sgn * d
        return Vector((x, y, z))
    t = 0.006
    outer = [[P(-1 + 2 * c / cols, r / rows, t) for c in range(cols + 1)] for r in range(rows + 1)]
    inner = [[P(-1 + 2 * c / cols, r / rows, -t) for c in range(cols + 1)] for r in range(rows + 1)]
    part = C.thick_patch(outer, inner, sub_tags=True)
    face, back = ("crimson", "crimson_in") if front else ("crimson_in", "crimson_in")
    # hull proxy with lens edges along the sides and the hem (see ``add_cape``)
    du, dk = CAPE_LENS / (half + u(0.4)), CAPE_LENS / L
    hu = [(-1 - du, 0.0), (-1 - 0.6 * du, 0.8)] + [(-1 + 2 * c / cols, 1.0) for c in range(cols + 1)] + \
        [(1 + 0.6 * du, 0.8), (1 + du, 0.0)]
    hk = [(r / rows, 1.0) for r in range(rows + 1)] + [(1 + 0.6 * dk, 0.8), (1 + dk, 0.0)]
    hull = C.thick_patch([[P(x, k, t * fx * fk + 1e-4) for x, fx in hu] for k, fk in hk],
                         [[P(x, k, -t * fx * fk - 1e-4) for x, fx in hu] for k, fk in hk])
    tid = b.add(part, face, Bind.blend("pelvis", f"{chain}_01", f"{chain}_02", falloff=3.0), name=chain,
                sub_regions={1: face, 2: back}, hull=hull, hull_pad=True)
    if front:
        # gold V trim parallel to the pointed hem (web: from 8 % up the side edges to just above the point)
        pts = []
        for i in range(13):
            uu = -0.86 + 1.72 * i / 12
            k = 0.86 + 0.12 * (1 - abs(uu)) ** 1.0
            pts.append((uu, k))
        o2 = [[P(uu, k - 0.026, t + 0.002) for uu, k in pts], [P(uu, k + 0.026, t + 0.002) for uu, k in pts]]
        i2 = [[P(uu, k - 0.026, t - 0.001) for uu, k in pts], [P(uu, k + 0.026, t - 0.001) for uu, k in pts]]
        b.add(C.thick_patch(o2, i2), "gold", Bind.blend("pelvis", f"{chain}_01", f"{chain}_02", falloff=3.0),
              name="tabard_trim", outline=False, covered_by=tid)


CAPE_BONES = ["spine_03"] + [f"cape_{c}_{i:02d}" for c, n in (("c", 4), ("l", 3), ("r", 3)) for i in range(1, n + 1)]


def _smooth01(x):
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3 - 2 * x)


def cape_weights(co: np.ndarray) -> np.ndarray:
    """Skin weights of every cape part from its surface parameters (art review 6). Laterally the three chains blend
    smoothly (smoothstep from the centre chain at uu 0 to an edge chain at uu ±0.72), so the U section bends as one
    sheet instead of three boards; along each chain the weight passes from bone to bone between bone centres (the
    sheet curls progressively); the yoke (top 14 %) stays on spine_03 where the cape tucks under the pauldrons.
    ``co`` bind-pose metres (N×3) → N × len(CAPE_BONES)."""
    ztop = D.nz - 0.028
    k = np.clip((ztop - co[:, 2]) / CAPE_LEN, 0.0, 1.0)
    half = u(7.4) + u(2.2) * k
    uu = np.clip(co[:, 0] / half, -1.0, 1.0)
    side = _smooth01(np.abs(uu) / CAPE_CHAIN_UU)
    lat = {"c": 1.0 - side, "l": side * (uu > 0), "r": side * (uu <= 0)}
    yoke = 1.0 - _smooth01(k / 0.14)
    W = np.zeros((len(co), len(CAPE_BONES)))
    W[:, 0] = yoke
    for c, n in (("c", 4), ("l", 3), ("r", 3)):
        q = np.clip(k * n - 0.5, 0.0, n - 1.0)          # bone-centre coordinate
        i0 = np.floor(q).astype(int)
        i0 = np.minimum(i0, n - 2)
        f = q - i0
        for i in range(n):
            wi = np.where(i0 == i, 1.0 - f, 0.0) + np.where(i0 + 1 == i, f, 0.0)
            W[:, CAPE_BONES.index(f"cape_{c}_{i + 1:02d}")] += (1.0 - yoke) * lat[c] * wi
    return W


def add_cape(b: M.Builder) -> None:
    rows, cols = 16, 18
    t = 0.0075
    ks = [r / rows for r in range(rows + 1)]
    us = [-1 + 2 * c / cols for c in range(cols + 1)]
    outer = [[CAPE(x, k, t) for x in us] for k in ks]
    inner = [[CAPE(x, k, -t) for x in us] for k in ks]
    bind = Bind.custom(CAPE_BONES, cape_weights)
    # hull proxy: half the rows (the cape is smooth down its length) and **lens edges**: the side edges and the
    # hem taper to a pinched rim ``CAPE_LENS`` beyond the cloth, so the smoothed normals fan through 180° round
    # every edge; padded (``hull_pad``) over the hem, the crease strips and the clasps' rows
    du, dk = CAPE_LENS / (u(7.4) + u(2.2) * 0.5), CAPE_LENS / CAPE_LEN
    hu = [(-1 - du, 0.0), (-1 - 0.6 * du, 0.8)] + [(x, 1.0) for x in us] + [(1 + 0.6 * du, 0.8), (1 + du, 0.0)]
    hk = [(k, 1.0) for k in ks[::2]] + [(1 + 0.6 * dk, 0.8), (1 + dk, 0.0)]
    hull = C.thick_patch([[CAPE(x, k, t * max(fx * fk, 0.0) + 1e-4) for x, fx in hu] for k, fk in hk],
                         [[CAPE(x, k, -t * max(fx * fk, 0.0) - 1e-4) for x, fx in hu] for k, fk in hk])
    cape_id = b.add(C.thick_patch(outer, inner, sub_tags=True), "crimson", bind, name="cape",
                    sub_regions={1: "crimson", 2: "crimson_in"}, hull=hull, hull_pad=True)
    # two fold creases down the cape (web: CRIMSON.shade strokes in the fold valleys, u = ±1/3)
    for uc in (-1 / 3, 1 / 3):
        ck = [0.12 + 0.83 * i / 12 for i in range(13)]
        hw = 0.012 / max(0.2, 0.237 + 0.065)
        o3 = [[CAPE(uc - hw, k, t + 0.0018), CAPE(uc + hw, k, t + 0.0018)] for k in ck]
        i3 = [[CAPE(uc - hw, k, t - 0.0015), CAPE(uc + hw, k, t - 0.0015)] for k in ck]
        b.add(C.thick_patch(o3, i3), "crimson_line", bind, name="cape_crease", outline=False, covered_by=cape_id)
    # gold hem
    k0 = 0.955
    hk = [k0, (k0 + 1) / 2, 1.0]
    o2 = [[CAPE(x, k, t + 0.0022) for x in us] for k in hk]
    i2 = [[CAPE(x, k, -t - 0.0022) for x in us] for k in hk]
    b.add(C.thick_patch(o2, i2), "gold", bind, name="cape_hem", outline=False, covered_by=cape_id)
    # cape clasps: gold discs at the shoulder fronts (where the cape pins under the pauldrons); 2 cm proud of the
    # cloth, so they carry their own small hull (ink ring) instead of standing outside the cape's ink
    for sg in (1, -1):
        p = CAPE(sg * 0.93, 0.02, t + 0.0025)
        b.add(ell((0.022, 0.007, 0.022), p, sides=8, rings=5), "gold", Bind.rigid("spine_03"), name="clasp")


# ── weapons ────────────────────────────────────────────────────────────────────────────────────────────
SWORD_BLADE0, SWORD_TIP = 0.105, 0.905   # blade from the guard to the tip: 80 cm (spec: 27 u incl. tip)
SWORD_GUARD_HALF = 0.140                 # crossguard 28 cm over the finials (spec 9.6 u)
SWORD_POMMEL_END = -0.137


def sword_half_thick(y: float, w: float) -> float:
    """Half-thickness of the blade's diamond section at ``y`` (ridge to flat), ``w`` = half-width there:
    2.7 cm at the guard, 2.0 cm where the point starts, never below 0.8 cm — edge-on the blade (+ its 1.3 cm hull)
    stays above the 4 cm readability floor in rolls and dodges."""
    y1 = SWORD_TIP - 0.135
    if y <= y1:
        return 0.0135 - 0.0035 * (y - SWORD_BLADE0) / (y1 - SWORD_BLADE0)
    return max(0.004, 0.0100 * (w / 0.0345) ** 0.6)


def blade_half_width(y: float) -> float:
    y1 = SWORD_TIP - 0.135
    if y <= y1:
        w = 0.0425 - 0.008 * (y - SWORD_BLADE0) / (y1 - SWORD_BLADE0)
    else:
        g = (y - y1) / (SWORD_TIP - y1)
        w = 0.0345 * math.sqrt(max(0.0, 1 - g ** 1.6))
    return max(w, 0.0015)


BLADE_BEVEL = (0.50, 0.17)    # edge bevel: rises to 50 % of the half-thickness over 17 % of the half-width
BLADE_HOLLOW = 0.68           # the hollow facet break (x as a fraction of the half-thickness)


def blade_section(w: float, th: float) -> list[tuple[float, float]]:
    """Closed (x, z) section: edge, bevel, hollow, ridge, … (x = flat normal, z = edge direction)."""
    bx, bz = th * BLADE_BEVEL[0], w * BLADE_BEVEL[1]
    hz = (w - bz) * 0.5
    half = [(0.0, w), (bx, w - bz), (th * BLADE_HOLLOW, hz), (th, 0.0), (th * BLADE_HOLLOW, -hz), (bx, -(w - bz)),
            (0.0, -w)]
    return half + [(-x, z) for x, z in half[-2:0:-1]]


def blade_surface_x(w: float, th: float, z: float) -> float:
    """Flat-side surface |x| of the section at edge coordinate ``z`` (for trims riding the flat)."""
    pts = blade_section(w, th)[:7]
    az = abs(z)
    for (x0, z0), (x1, z1) in zip(pts[3::-1], pts[2::-1]):     # ridge → hollow → bevel → edge
        if z0 <= az <= z1:
            return x0 + (x1 - x0) * (az - z0) / max(z1 - z0, 1e-9)
    return 0.0


def build_sword(pal: Palette) -> tuple[bpy.types.Object, list]:
    """Broadsword in weapon-bone space: grip centre at the origin, blade along +Y, edges ±Z, flats ±X
    (web: blade 27 u ≈ 80 cm incl. tip, down-swept crossguard 9.6 u ≈ 28 cm, leather grip 4.2 u, round gold pommel +
    gem; the gem is an ember cabochon in the pommel end). The blade has a diamond section with a central ridge
    (dark fuller line along it) so it keeps a readable thickness edge-on."""
    b = M.Builder(SWORD, pal, prefix="warrior_sword")
    b.regions(**SWORD_REGIONS)
    y0, ytip = SWORD_BLADE0, SWORD_TIP
    y1 = ytip - 0.135
    # blade (art review 6): hollow-ground diamond — on each flat two facets meet at the central (fuller) ridge
    # (≈ 16° and ≈ 5° off the blade plane, the hollow), and a ≈ 40° bevel runs along each edge: whichever way the
    # flat turns from the key light, a facet (usually an edge bevel) stays lit, so the blade never reads as one
    # dark slab. Width 3 u → 2.4 u, point over the last 4.5 u.
    rows = []
    n = 16
    for i in range(n + 1):
        f = i / n
        y = y0 + (ytip - y0) * f
        rows.append((y, blade_half_width(y), sword_half_thick(y, blade_half_width(y))))
    bm = bmesh.new()
    ring_vs = []
    for y, w, th in rows:
        ring_vs.append([bm.verts.new((x, y, z)) for x, z in blade_section(w, th)])
    m = len(ring_vs[0])
    for r0, r1 in zip(ring_vs, ring_vs[1:]):
        for i in range(m):
            j = (i + 1) % m
            bm.faces.new((r0[i], r1[i], r1[j], r0[j]))
    bm.faces.new(list(reversed(ring_vs[0])))
    tip = bm.verts.new((0, ytip + 0.004, 0))
    for i in range(m):
        bm.faces.new((ring_vs[-1][i], tip, ring_vs[-1][(i + 1) % m]))
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bl = M.Part(bm)
    for e in bl.bm.edges:                       # crisp bevels and ridge, the hollow facets blend
        e.smooth = True
    bl.auto_sharp(24)
    b.add(bl, "blade")
    # fuller: dark line riding the ridge on both flats (web 0.45 u line, 70 % of the blade)
    for sg in (1, -1):
        ys = [y0 + 0.03 + 0.56 * i / 10 for i in range(11)]
        o3, i3 = [], []
        for y in ys:
            w = blade_half_width(y)
            th = sword_half_thick(y, w)
            row_o, row_i = [], []
            for z in (-0.0055, 0.0, 0.0055):
                xs = blade_surface_x(w, th, z)
                row_o.append(Vector((sg * (xs + 0.0009), y, z)))
                row_i.append(Vector((sg * (xs - 0.0025), y, z)))
            o3.append(row_o)
            i3.append(row_i)
        b.add(C.thick_patch(o3, i3), "fuller", outline=False)
    # down-swept crossguard (gold): from the centre block the quillons droop 14° toward the grip, the tips
    # curling on a little further (web: the guard bends down to the hand), finials at ±12.4 cm → 28 cm overall
    gz = [-0.118, -0.090, -0.058, -0.026, 0.0, 0.026, 0.058, 0.090, 0.118]

    def guard_y(z: float) -> float:
        a = abs(z)
        return 0.080 - max(0.0, a - 0.022) * math.tan(math.radians(14.0)) - 0.008 * (a / 0.118) ** 3
    guard = M.tube([(0.0, guard_y(z), z) for z in gz], [0.013, 0.015, 0.017, 0.019, 0.021, 0.019, 0.017, 0.015, 0.013],
                   sides=10, exp=2.6, up=(1, 0, 0))
    b.add(guard, "gold")
    b.add(M.box((0.036, 0.040, 0.058), (0, 0.090, 0.0), bevel=0.008), "gold")
    zf = SWORD_GUARD_HALF - 0.016
    for sg in (1, -1):
        b.add(ell((0.016, 0.016, 0.016), (0, guard_y(zf) - 0.003, sg * zf), sides=10, rings=6), "gold")
    # grip (leather, wrapped look via two raised rings)
    grip = b.add(M.tube([(0, -0.085, 0), (0, 0.072, 0)], [0.019, 0.0175], sides=12), "leather", name="grip",
                 hull_pad=True)
    for yy in (-0.05, 0.0, 0.045):
        b.add(M.tube([(0, yy - 0.005, 0), (0, yy + 0.005, 0)], [0.0205, 0.0205], sides=12), "leather",
              name="grip_ring", outline=False, covered_by=grip)
    # round gold pommel along the grip axis with an ember cabochon set in its end (faces down the grip, so it
    # shows from the high camera whenever the blade points forward-down)
    pom = M.lathe([(0.021, -0.087), (0.029, -0.096), (0.033, -0.111), (0.031, -0.124), (0.024, -0.134),
                   (0.021, -0.137)], sides=16, cap0="fan", cap1="fan")
    pommel = b.add(pom.rotate("x", 180).rotate("x", 90), "gold", name="pommel",   # lathe z → item −Y (z < 0 → y < 0)
                   hull_pad=True)
    b.add(M.tube([(0, -0.092, 0), (0, -0.082, 0)], [0.022, 0.022], sides=12), "gold")
    cab = M.lathe([(0.0185, 0.0), (0.0175, 0.004), (0.013, 0.009), (0.006, 0.0115), (0.0008, 0.012)], sides=14,
                  cap0="fan", cap1=None)
    b.add(cab.rotate("x", 90).translate((0, -0.1335, 0)), "gem", name="pommel_gem", outline=False, covered_by=pommel)
    obj = asset.finish_mesh(b, pal, "weapon")
    obj.name = SWORD
    return obj, [{"name": "tip", "pos": (0, ytip, 0)}, {"name": "mid", "pos": (0, (y0 + ytip) / 2, 0)},
                 {"name": "guard", "pos": (0, y0, 0)}]


SHIELD_W, SHIELD_H = 0.36, 0.60      # spec §3.1 heater 13.6 × 20 u ≈ 36 × 60 cm (width / height 0.6)


def heater_outline(scale: float = 1.0) -> list[tuple[float, float]]:
    """Web heater (13.6 × 20 u) in shield coordinates (x across, z up), metres, CCW, scaled to the spec's
    36 × 60 cm (measured on the smoothed outline; the face is then bent, see ``_bend_shield``)."""
    src = [(-6.8, -8.6), (0.0, -9.8), (6.8, -8.6), (6.7, -3.0), (6.6, 0.5), (5.4, 3.8), (3.6, 6.6), (1.6, 8.8),
           (0.0, 10.2), (-1.6, 8.8), (-3.6, 6.6), (-5.4, 3.8), (-6.6, 0.5), (-6.7, -3.0)]
    sm = C.smooth_closed([(x, -y) for x, y in src], 3)
    xs, zs = [p[0] for p in sm], [p[1] for p in sm]
    kx = SHIELD_W / (max(xs) - min(xs)) * scale
    kz = SHIELD_H / (max(zs) - min(zs)) * scale
    return C.ccw([(x * kx, z * kz) for x, z in sm])


SHIELD_R = 0.42          # curvature radius (convex face)
SHIELD_FACE_Y = 0.118     # face distance in front of the grip (fist + forearm clear behind it)
SHIELD_CX, SHIELD_CZ = -0.045, -0.020


def _bend_shield(co: Vector) -> Vector:
    """Flat shield coords (x across, y = depth +out, z up) → cylindrically curved, convex toward +Y."""
    x, y, z = co.x, co.y, co.z
    a = x / SHIELD_R
    r = SHIELD_R + y
    return Vector((r * math.sin(a) + SHIELD_CX, r * math.cos(a) - SHIELD_R + SHIELD_FACE_Y, z + SHIELD_CZ))


def build_shield(pal: Palette) -> bpy.types.Object:
    """Heater shield in weapon_l space: grip at the origin, +Y = face normal (out), +Z = up; face curved
    (convex), gold rim, crimson field, gold flame emblem with the hot core; back: iron rim, leather boards,
    straps and the handle the fist closes on (web viewShield)."""
    b = M.Builder(SHIELD, pal, prefix="warrior_shield")
    b.regions(**SHIELD_REGIONS)
    out = heater_outline()
    field = C.offset_closed(out, -0.026)
    # leather board body (behind the field) and the crimson field
    core = C.slab2d(C.offset_closed(out, -0.006), -0.018, 0.002, rings=4)
    core.deform(_bend_shield)
    # the silhouette hull of the whole shield: the heater slab rim-to-rim, padded over the emblem and its core
    # (they stood 5–8 mm proud of the rims edge-on); the rims keep their own hulls for the rim / field line
    slab = C.slab2d(out, -0.026, 0.019, rings=4)
    slab.deform(_bend_shield)
    body_id = b.add(core.auto_sharp(70), "leather", name="boards", hull=slab, hull_pad=True)
    fld = C.slab2d(field, -0.002, 0.0105, rings=4)
    fld.deform(_bend_shield)
    b.add(fld.auto_sharp(70), "crimson", name="field", outline=False, covered_by=body_id)
    # raised gold rim: frame between the outline and the field edge
    inner = C.offset_closed(field, 0.002)
    rim = C.thick_patch([[Vector((x, 0.019, z)) for x, z in out], [Vector((x, 0.019, z)) for x, z in inner]],
                        [[Vector((x, -0.006, z)) for x, z in out], [Vector((x, -0.006, z)) for x, z in inner]],
                        wrap_u=True)
    rim.deform(_bend_shield)
    b.add(rim.auto_sharp(50), "gold", name="rim")
    # iron back rim
    bo, bi = C.offset_closed(out, -0.001), C.offset_closed(out, -0.032)
    brim = C.thick_patch([[Vector((x, -0.006, z)) for x, z in bo], [Vector((x, -0.006, z)) for x, z in bi]],
                         [[Vector((x, -0.026, z)) for x, z in bo], [Vector((x, -0.026, z)) for x, z in bi]],
                         wrap_u=True)
    brim.deform(_bend_shield)
    b.add(brim.auto_sharp(50), "iron_dark", name="back_rim")
    # gold flame emblem + hot core (web emblem path: tongue flicking up the left side)
    fsrc = [(0.0, -5.4), (2.0, -3.4), (3.2, -0.8), (2.7, 2.4), (1.6, 4.7), (0.0, 5.6), (-1.6, 4.7), (-2.6, 2.4),
            (-2.9, -0.2), (-2.0, -1.8), (-0.9, -2.6), (-0.8, -0.6), (-0.1, 0.5), (0.6, -0.6), (0.7, -2.6)]
    ek = U0 * 0.98                       # emblem scaled with the narrower field (≈ 0.57 of its width, as the web)
    flame = C.ccw(C.smooth_closed([(x * ek, -y * ek + 0.004) for x, y in fsrc], 4))
    em = C.plate2d(flame, 0.010)
    em.rotate("x", 90).translate((0, 0.0145, 0))
    em.deform(_bend_shield)
    b.add(em.auto_sharp(60), "gold", name="emblem", outline=False, covered_by=body_id)
    # hot core: an inner flame tongue (web: core ellipse at (0.2, 2.8)) rising from the bulb along the main
    # tongue, its tip just right of the notch
    csrc = [(0.15, 4.5), (1.35, 3.6), (1.55, 2.4), (1.25, 1.1), (1.05, -0.3), (0.55, 0.9), (-0.35, 1.9),
            (-0.75, 3.0), (-0.55, 4.0)]
    core = C.ccw(C.smooth_closed([(x * ek, -y * ek + 0.004) for x, y in csrc], 4))
    cr = C.plate2d(core, 0.008)
    cr.rotate("x", 90).translate((0, 0.0195, 0))
    cr.deform(_bend_shield)
    b.add(cr.auto_sharp(60), "core", name="emblem_core", outline=False, covered_by=body_id)
    # back: two leather straps across the boards + the handle loop the fist closes on (at the origin)
    for zz in (0.085, -0.120):
        st = M.box((0.27, 0.010, 0.032), (0.0, -0.022, zz), bevel=0.003)
        st.deform(_bend_shield)
        b.add(st, "leather_dark", name="strap", outline=False, covered_by=body_id)
    yb = SHIELD_FACE_Y - 0.026
    hdl = M.tube([(0, yb, 0.075), (0, 0.010, 0.050), (0, -0.004, 0.0), (0, 0.010, -0.050), (0, yb, -0.075)],
                 [0.011, 0.012, 0.012, 0.012, 0.011], sides=8)
    b.add(hdl, "leather_dark", name="handle")
    obj = asset.finish_mesh(b, pal, "weapon")
    obj.name = SHIELD
    return obj


# ── quick preview (mesh iteration) ─────────────────────────────────────────────────────────────────────
def build_all():
    scene.reset()
    pal = Palette("Heroes")
    spec = build_spec()
    rig = R.build_humanoid(spec)
    R.add_socket(rig, "visor", "head", helm_profile().point(VISOR_Z(), 0.0, 0.0))
    body = build_body(pal, rig)
    sword, sockets = build_sword(pal)
    shield = build_shield(pal)
    return pal, rig, body, sword, shield, sockets


SCRATCH = Path("/tmp/claude-0/-home-user-abyssfire/b3225f99-25a3-5b78-996f-643be8685889/scratchpad/warrior")


def quick(out: Path, facing: str = "se") -> None:
    """Mesh iteration: build, READY pose, close-up / turnaround / game crop into ``out``."""
    import warrior_clips as WC
    t0 = time.time()
    pal, rig, body, sword, shield, sockets = build_all()
    print(f"[build] body {M.tri_count(body, 0)} + hull {M.tri_count(body, 1)} tris; sword {M.tri_count(sword, 0)}; "
          f"shield {M.tri_count(shield, 0)}; bones {len(rig.obj.data.bones)}; {time.time() - t0:.1f}s")
    print("[weights]", {k: v for k, v in R.weight_stats(body).items() if k != "groups"})
    print("[parts]", part_report(LAST_PARTS))
    A = WC.WarriorAnims(rig, D)
    anim.apply_pose(A.solve(A.ready()))
    R.attach_to_bone(sword, rig, "weapon_r")
    R.attach_to_bone(shield, rig, "weapon_l")
    rv = review.Review(ASSET, rig.obj, [body, sword, shield], HEIGHT, blob_radius=0.39, out_dir=out,
                       game_outline_px=outline.SCREEN_PX_1080["hero"])
    rv.closeup(facing)
    rv.turnaround()
    rv.game_view()
    print(f"[quick] {time.time() - t0:.1f}s -> {out}")


def anim_preview(out: Path, names: list[str], facing: str = "se", frames: int = 8) -> None:
    """Clip iteration: bake the named clips (all when empty) and render contact sheets into ``out``."""
    import warrior_clips as WC
    t0 = time.time()
    pal, rig, body, sword, shield, sockets = build_all()
    A = WC.WarriorAnims(rig, D)
    clips = [c for c in A.all_clips() if not names or c.name in names]
    acts = []
    for c in clips:
        t1 = time.time()
        acts.append(anim.bake(rig, c, TOKEN))
        print(f"[bake] {c.name} {c.frames}f {time.time() - t1:.1f}s")
    R.attach_to_bone(sword, rig, "weapon_r")
    R.attach_to_bone(shield, rig, "weapon_l")
    rv = review.Review(ASSET, rig.obj, [body, sword, shield], HEIGHT, blob_radius=0.39, out_dir=out)
    for c, a in zip(clips, acts):
        rv.contact_sheet(rig, [(c, a)], frames=frames, facing=facing, per_clip_files=False)
        (out / "anims.png").rename(out / f"anim_{c.name}_{facing}.png")
    print(f"[anims] {time.time() - t0:.1f}s -> {out}")


def frame_preview(out: Path, specs: list[str], facings=("se", "side", "back")) -> None:
    facings = tuple(next((a.split("=")[1].split(",") for a in sys.argv if a.startswith("--facings=")), facings))
    """Debug: render ``Clip:frame`` poses from several facings at a readable size into one strip."""
    import warrior_clips as WC
    pal, rig, body, sword, shield, sockets = build_all()
    A = WC.WarriorAnims(rig, D)
    clips = {c.name: c for c in A.all_clips()}
    R.attach_to_bone(sword, rig, "weapon_r")
    R.attach_to_bone(shield, rig, "weapon_l")
    rv = review.Review(ASSET, rig.obj, [body, sword, shield], HEIGHT, blob_radius=0.39, out_dir=out)
    cells = []
    for spec in specs:
        name, fr = spec.split(":")
        act = anim.bake(rig, clips[name], TOKEN)
        anim.assign(rig, act)
        anim.set_frame(int(fr))
        for fc in facings:
            rv.facing(fc)
            res = (300, 360)
            review.setup_render(res, 8)
            pts = review.mesh_points([body, sword, shield])
            review.frame_points(pts, res, fov_h=26, pitch_ue=-30 if fc != "top" else -50, fill=0.92)
            review.set_outline_px([body, sword, shield], 2.0, Vector(pts.mean(0)))
            img = review.render_array()
            review.draw_text(img, 4, 4, f"{name} F{fr} {fc}")
            cells.append(img)
    pngio.write_png(out / "frames.png", review.grid(cells, len(facings)))


if __name__ == "__main__":
    if "--frames" in sys.argv:
        i = sys.argv.index("--frames")
        frame_preview(SCRATCH / "frames", [a for a in sys.argv[i + 1:] if ":" in a])
    elif "--quick" in sys.argv:
        quick(SCRATCH / "quick")
    elif "--anims" in sys.argv:
        i = sys.argv.index("--anims")
        rest = [a for a in sys.argv[i + 1:] if not a.startswith("--")]
        fac = next((a.split("=")[1] for a in sys.argv if a.startswith("--facing=")), "se")
        anim_preview(SCRATCH / "anims", rest, facing=fac)
    else:
        import warrior_ship
        sys.exit(warrior_ship.main(sys.argv))

"""影刃 Shadowblade — ``SK_Hero_Rogue`` (spec art-inventory-ch1.md §3.3, §3.4, §3.5; DECISIONS R2/R4/R5/R11).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/heroes/rogue.py          # ship
    #   --no-previews / --no-verify            skip the review renders / the FBX re-import check
    #   --quick [--facing=se] [--pose=ready]   mesh iteration: posed close-up, turnaround, game crop → scratch
    #   --anims [Clip …] [--facing=se] [--qa]  bake + contact sheets → scratch
    #   --frames Clip:frame … [--facings=se,side,back]   large debug stills → scratch
    #   --game Clip:frame:facing …             native 1080p game crops (ink-gated) → scratch

The hooded shadowblade in teal and oiled leather, the smallest hero, crouched with two blades held wide: a teal
hood (tighter than the mage's, its dark face opening showing a skin band across the eyes and a dark cloth mask over
the nose and mouth, two amber eyes), a short dark-teal liripipe; a high-cut dark-teal capelet with a brass clasp; a
leather jerkin with a stitched seam, a dark bandolier from the right shoulder to the left hip carrying four throwing
knives (bright steel, brass pommels); a dark belt with a brass buckle and a hip pouch; eight split dark-leather
tassets; wrapped arms with leather spaulders and bracers (brass ring), fingerless dark-banded hands; wrapped legs with
leather cross-straps on the shins, knee pads and soft boots; two long scarf tails (dark teal 57 cm, teal 47 cm)
streaming from a knot at the back of the neck.

Rig: ``SKEL_Human`` + chains ``scarf_a_01..05`` / ``scarf_b_01..05`` (spine_03) and ``hood_tip_01..02`` (head); the
tassets ride the pelvis and thighs through their skin weights. Items (``Weapons/``): ``SM_Hero_Rogue_Dagger`` on
``weapon_r`` and its mirror ``SM_Hero_Rogue_Dagger_L`` on ``weapon_l`` (curved leaf blades with the poison edge
line), the flash-bow ``SM_Hero_Rogue_Bow`` (swapped into the off hand for ``Cast_Multishot`` only) and the hip quiver
``SM_Hero_Rogue_Quiver`` (body socket ``quiver`` on the pelvis). Clips: ``rogue_clips.py``; ship / review:
``hero_ship.py`` with ``rogue_ship.HERO``.

Web source: ``src/graphics/sprites/players/PlayerRogue.ts`` (palette :54-66, dagger :91-121, proportions :123-128,
jerkin :130-262, scarf :264-292, hood :294-370, arms :373-386, poses :422-588). Web rig units → metres with ``U``.
"""
from __future__ import annotations

import json
import math
import sys
from dataclasses import replace
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import asset, mesh as M, rig as R, scene  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402
from kit.rig import Bind, HumanoidSpec  # noqa: E402

import common as C  # noqa: E402
from common import Profile  # noqa: E402
import hero_base as HB  # noqa: E402
from hero_base import ell, profile_mesh, painted_strip  # noqa: E402

ASSET = "SK_Hero_Rogue"
TOKEN = "Hero_Rogue"
DAGGER = "SM_Hero_Rogue_Dagger"
DAGGER_L = "SM_Hero_Rogue_Dagger_L"
BOW = "SM_Hero_Rogue_Bow"
QUIVER = "SM_Hero_Rogue_Quiver"
HEIGHT = 1.62
U0 = 0.0293                       # web rogue rig unit (spec §0.1)
# hood: the web 3/4 view draws the rings × 0.88 (HOOD_K :300); the camera-aware tune (warrior HELM_K, mage HOOD_K)
# scales the whole hood × CAM_K about the head centre and gives the freed crown height to the torso and legs
RING_K = 0.88
CAM_K = 0.95
TORSO_ADD, LEG_ADD = 0.4, 0.25

# ── palette (spec §3.3; (s, l) = tone shadow / light) ──────────────────────────────────────────────────
CLOAK = Region("#2F7064", .42, .30)
CLOAK_FAR = Region("#1D463F", .42, .20)
LEATHER = Region("#70492F", .42, .35)
LEATHER_DARK = Region("#4C3121", .42, .20)
WRAP = Region("#3D3D4A")
MASK = Region("#1F2B2E", .42, .25)
BRASS = Region("#C0953E", .42, .50)
# steel: shadowAmt .15 (the warrior's blade rule: with .30 a flat turned from the key light drops to dark iron)
STEEL = Region("#CFD8E4", .15, .60)
REGIONS = dict(
    hood=CLOAK,                                    # hood, scarf tail B
    hood_seam=CLOAK.shade_region(),                # hood back seam (web CLOAK.shade stroke)
    cloak=CLOAK_FAR,                               # capelet, scarf tail A, hood tip
    cloak_edge=CLOAK.light_region(),               # capelet hem line (web CLOAK.light stroke)
    leather=LEATHER,                               # jerkin, bracers, spaulders, knee pads, boots, pouch
    leather_seam=LEATHER.shade_region(),           # stitched seam, spaulder seam (web LEATHER.shade)
    leather_dark=LEATHER_DARK,                     # tassets, belt, pouch flap, boot cuffs, shin straps
    wrap=WRAP,                                     # arm / leg wraps
    wrap_line=WRAP.shade_region(),                 # wrap turns
    mask=MASK,                                     # face mask, bandolier, glove bands
    mask_light=MASK.light_region(),                # bandolier highlight edge (web MASK.light)
    skin=Region("#D9A47C", .30, .25),
    brass=BRASS,
    steel=STEEL,
    hood_in=Region("#0E1416", .30, .08),           # hood opening (web #0e1416)
    eye=Region("#FFD36E", .0, .0, e=1.0),          # amber eyes (+ the runtime eye glow card #FFC24A)
)
DAGGER_REGIONS = dict(
    steel=STEEL,
    # poison edge line rgba(120,255,170,.75) (FX #6DFFA0): set a third of the way in from the curved edge and kept at
    # e .6 so its bloom does not lift the blade's silhouette ink (ink gate: dark rim)
    poison=Region("#78FFAA", .0, .0, e=0.6),
    brass=BRASS,
    grip=LEATHER_DARK,
    grip_wrap=LEATHER,
)
BOW_REGIONS = dict(
    limb=LEATHER_DARK,                             # dark oiled limbs (the rogue's dark leather)
    limb_line=LEATHER.light_region(),              # the limbs' belly lamination line
    brass=BRASS,
    grip=CLOAK,                                    # teal grip wrap
    nock=Region("#9DFFC8", .0, .0, e=0.8),         # the energy-string nocks (string = runtime FX, #9DFFC8)
)
QUIVER_REGIONS = dict(
    leather=LEATHER,
    leather_dark=LEATHER_DARK,
    brass=BRASS,
    shaft=Region("#5A3A26", .42, .25),
    fletch=CLOAK,
    fletch_b=CLOAK_FAR,
)

HULL_INK_O = 0.4                  # hull colour = 60 % ink #120C18 + 40 % the region's line tone (the warrior's)
REGIONS, DAGGER_REGIONS, BOW_REGIONS, QUIVER_REGIONS = (
    {k: replace(r, o=HULL_INK_O) for k, r in d.items()} for d in (REGIONS, DAGGER_REGIONS, BOW_REGIONS,
                                                                    QUIVER_REGIONS))

# ── dimensions ─────────────────────────────────────────────────────────────────────────────────────────
HOOD_TOP = 9.25                   # web RHOOD top ring (8) drawn up into the peak
TORSO_LEN = 15.0                  # web torso (h of the neck ring above the hip joints)


class Dims:
    def __init__(self):
        base = HumanoidSpec(name=ASSET, skeleton="SKEL_Human", thigh=(12 + LEG_ADD) * U0,
                            shin=(12 + LEG_ADD) * U0, ankle=2.2 * U0, torso=(TORSO_LEN + TORSO_ADD) * U0,
                            neck=6.6 * U0, head=2 * (HOOD_TOP + 0.3) * CAM_K * U0, hip_half=0.078,
                            shoulder_half=5.6 * U0, upper_arm=9 * U0, fore_arm=8.5 * U0, hand=0.095,
                            foot_len=0.235)
        self.k = HEIGHT / base.crown_height()
        self.spec = base.fit_height(HEIGHT)
        s = self.spec
        self.U = U0 * self.k
        self.hip_z = s.ankle + s.shin + s.thigh
        self.pz = self.hip_z + s.pelvis_offset
        self.nz = self.pz + s.torso
        self.hc = self.nz + s.neck
        # web torso heights are measured from the hip joints (the web root) to the neck ring
        self.hs = (self.nz - self.hip_z) / TORSO_LEN


D = Dims()
U = D.U


def u(n: float) -> float:
    return n * U


def uh(n: float) -> float:
    """Hood heights / offsets (web units about the head centre) × the camera tune."""
    return n * U * CAM_K


def ur(n: float) -> float:
    """Hood ring radii: web × 0.88 (3/4 view) × the camera tune."""
    return n * U * CAM_K * RING_K


def th(h: float) -> float:
    """Web torso height (units above the hip joints, the web root) → z."""
    return D.hip_z + h * D.hs


BELT_H = 2.5                      # web belt at h 1.4, raised 1.1 u to the waist (the tassets hinge under it)
BAND_BOT_H = 3.7                  # the bandolier's low end at the left hip, above the belt


# ── profiles ───────────────────────────────────────────────────────────────────────────────────────────
def hood_profile() -> Profile:
    """Web RHOOD_RINGS (h, a front/back, b side, f forward) × 0.88, Catmull-Rom between them, closed by a soft
    peak tipped back."""
    hc = D.hc
    # the web's rounded cowl, its crown drawn up into a peak tipped back (the web 3/4 sprite's pointed hood)
    rings = [(-5.2, 5.0, 5.6, -0.5), (-4.5, 6.1, 6.15, -0.25), (-3.6, 6.8, 6.4, 0.0), (-1.2, 7.1, 6.55, -0.1),
             (1.2, 7.2, 6.5, -0.2), (3.7, 6.5, 5.85, -0.35), (6.0, 5.0, 4.7, -0.65), (7.6, 2.9, 2.7, -1.05),
             (8.6, 1.25, 1.15, -1.45), (9.25, 0.3, 0.3, -1.75)]
    return Profile([(hc + uh(h), ur(b), ur(a), uh(-f)) for h, a, b, f in rings], exp=2.0)


def jerkin_profile() -> Profile:
    """Web jerkinRings (len + 0.6 → 0.4) continued under the tassets."""
    L = TORSO_LEN
    rings = [(-2.4, 3.65, 4.95, 0.0), (0.4, 3.5, 4.8, 0.0), (3.0, 3.2, 4.4, 0.0), (5.8, 3.45, 4.75, 0.2),
             (L - 6.5, 3.8, 5.2, 0.4), (L - 4.0, 3.85, 5.5, 0.25), (L - 2.0, 3.7, 5.6, 0.0), (L - 0.6, 3.1, 4.5, 0.0),
             (L + 0.6, 2.5, 3.5, 0.0)]
    return Profile([(th(h), u(b), u(a), -u(f)) for h, a, b, f in rings], exp=2.3)


def capelet_profile() -> Profile:
    """Web capelet (len + 1.2 → len − 2.9), widened over the shoulder joints (the 3D arms start inside it)."""
    L = TORSO_LEN
    rings = [(L - 3.0, 4.55, 6.75, 0.1), (L - 2.7, 4.6, 6.8, 0.1), (L - 1.5, 4.6, 6.75, 0.05),
             (L - 0.2, 4.25, 6.2, 0.0), (L + 0.7, 3.45, 4.95, 0.0), (L + 1.3, 2.7, 3.7, 0.0)]
    return Profile([(th(h), u(b), u(a), -u(f)) for h, a, b, f in rings], exp=2.2)


def tasset_profile() -> Profile:
    """Web TASSET_RINGS (h 2.2 → −3.6) raised 0.8 u with the belt (the panels hinge above the raised thighs),
    flared a little for the 3D thighs."""
    rings = [(3.0, 3.6, 4.95, 0.0), (0.8, 4.1, 5.45, 0.0), (-0.8, 4.45, 5.8, 0.05), (-3.4, 4.8, 6.2, 0.2)]
    return Profile([(th(h), u(b), u(a), -u(f)) for h, a, b, f in rings], exp=2.3)


# ── rig spec ───────────────────────────────────────────────────────────────────────────────────────────
SCARF_A = 152.0                   # knot angle (deg, 0 front, +90 left): web phi π − 0.4, mirrored to the left of
#                                   the spine (the quiver rides the right back hip)
SCARF_LEN = (19.55, 16.15)        # tails A / B (web 23 / 19 u × 0.85, ≈ 57 / 47 cm)
SCARF_W = (1.6, 1.45)             # half-widths at the knot (web 1.6 − 0.12 per link)


def scarf_knot() -> Vector:
    return jerkin_profile().point(th(TORSO_LEN + 0.15), SCARF_A, u(1.45))


def _back_clear(z: float, a: float) -> float:
    """Radius (from the body axis, at angle ``a``) the scarf must clear at height ``z``: capelet, jerkin and tassets."""
    out = 0.0
    for prof in (capelet_profile(), jerkin_profile(), tasset_profile()):
        if prof.z0 <= z <= prof.z1:
            p = prof.point(z, a)
            out = max(out, math.hypot(p.x, p.y))
    if abs(z - th(BELT_H)) < u(1.1):                    # the belt stands 0.4 u proud
        out += u(0.4) + 0.004
    return out


def scarf_points(which: int) -> list[Vector]:
    """Rest chain points of tail A (0) / B (1): from below the knot over the capelet's back, then plumb."""
    k = scarf_knot()
    L = u(SCARF_LEN[which])
    dx = (u(0.9), -u(0.6))[which]
    z0 = k.z - (u(0.7), u(1.6))[which]
    a = SCARF_A
    # polyline: start under the knot, follow the clearance radius + 1.4 cm down to the capelet hem, then hang plumb
    poly = []
    zz = z0
    r_hem = 0.0
    while zz > z0 - L * 1.2:
        rc = _back_clear(zz, a) + 0.022
        r_hem = max(r_hem, rc)              # never back inward: the tail hangs plumb off the capelet's hem
        r = r_hem
        dirv = Vector((math.sin(math.radians(a)), -math.cos(math.radians(a)), 0.0))
        p = dirv * r
        poly.append(Vector((p.x + dx, p.y, zz)))
        zz -= 0.01
    # resample at arc lengths 0, L/5 … L
    cum = [0.0]
    for p0, p1 in zip(poly, poly[1:]):
        cum.append(cum[-1] + (p1 - p0).length)
    out = []
    for i in range(6):
        s = L * i / 5
        j = next((n for n in range(1, len(cum)) if cum[n] >= s), len(cum) - 1)
        f = (s - cum[j - 1]) / max(cum[j] - cum[j - 1], 1e-9)
        out.append(poly[j - 1].lerp(poly[j], min(1.0, f)))
    return out


def build_spec() -> HumanoidSpec:
    spec = D.spec
    hc = D.hc
    chains = [("scarf_a", "spine_03", scarf_points(0)), ("scarf_b", "spine_03", scarf_points(1)),
              ("hood_tip", "head", [Vector((0, uh(5.3), hc + uh(0.6))), Vector((0, uh(7.6), hc - uh(3.0))),
                                    Vector((0, uh(9.2), hc - uh(6.6)))])]
    # one hinge bone per split tasset (from under the belt to the hem at the panel's centre): ``rogue_clips``
    # swings each panel out just enough to clear the posed thighs
    for i, (a0, a1) in enumerate(tasset_panels()):
        chains.append((f"tasset_{i}", "pelvis", list(tasset_hinge(i))))
    spec.chains = chains
    return spec


# LOD1 (spec §1.9 hero 7 k): every part decimated, sub-readability details left out
LOD1_RATIO = 0.4
LOD1_SKIP = ("stitch", "wrap_line", "hood_seam", "strap_hi", "nose", "hood_floor", "slit", "buckle_bar",
             "pouch_stud", "spaulder_seam", "neck")


# ── body ───────────────────────────────────────────────────────────────────────────────────────────────
RIG: list = [None]
LAST_PARTS: list = []


def build_body(pal: Palette, rig: R.Rig, lod: int = 0) -> bpy.types.Object:
    RIG[0] = rig
    if lod:
        b = M.Builder(ASSET, pal, prefix="rogue", lod_ratio=LOD1_RATIO,
                      lod_skip=lambda n: n.rstrip("_lr").rstrip("0123456789").startswith(LOD1_SKIP))
    else:
        b = M.Builder(ASSET, pal, prefix="rogue")
    b.regions(**REGIONS)
    add_hood(b)
    add_torso(b)
    add_tassets(b)
    add_scarf(b)
    for side in ("l", "r"):
        add_arm(b, rig, side)
        add_leg(b, rig, side)
    global LAST_PARTS
    LAST_PARTS = list(b.parts)
    names = [p["name"] for p in b.parts]
    body = asset.finish_mesh(b, pal, "hero", rig=rig, grounding_height=HEIGHT,
                             name=f"{ASSET}_LOD{lod}" if lod else f"{ASSET}_Mesh")
    body["af_parts"] = json.dumps(names)
    body["af_lod"] = lod
    return body


# face opening (web: dark oval h −5.2 … 4.8, ±1 rad round the hood front; trimmed so the hood keeps its cheeks)
OPEN_Z0, OPEN_Z1, OPEN_A = -4.7, 4.4, 50.0
OPEN_DEPTH = 0.042
BAND_TOP, BAND_BOT = 1.9, -0.35   # web skin band across the eyes (h 1.9 … −0.4); the mask below
EYE_H = 0.9


def open_edge(a: float) -> tuple[float, float] | None:
    x = a / OPEN_A
    if abs(x) >= 1.0:
        return None
    zc, hz = (OPEN_Z0 + OPEN_Z1) / 2, (OPEN_Z1 - OPEN_Z0) / 2
    w = math.sqrt(1 - x * x) ** 0.8
    return zc - hz * w, zc + hz * w


def face_r_at(z: float) -> float:
    return hood_profile().at(z)[1]


def add_hood(b: M.Builder) -> None:
    hc = D.hc
    hp = hood_profile()

    def disp(z, a):
        e = open_edge(a)
        if e is None:
            return 0.0
        h = (z - hc) / (U * CAM_K)
        return -OPEN_DEPTH if e[0] < h < e[1] else 0.0
    eps = 0.0012
    # density (budget, spec §1.9): 16 rings + a ring pair either side of the opening's edge at 8 angles; 24 sides
    # + 15 columns across the opening
    zs = [z for z in np.linspace(hp.z0, hp.z1, 16)]
    cols = [OPEN_A * k / 7.0 * 0.998 for k in range(-7, 8)]
    rows = []
    for i in range(1, 10):
        a = OPEN_A * (-1 + 2 * i / 10)
        e = open_edge(a)
        if e:
            rows += [hc + uh(e[0]) - eps, hc + uh(e[0]) + eps, hc + uh(e[1]) - eps, hc + uh(e[1]) + eps]
    zs = sorted(set([round(z, 5) for z in zs + rows if hp.z0 <= z <= hp.z1]))
    hood = profile_mesh(hp, zs, 24, disp=disp, cap0=None, cap1="fan", dome1=uh(0.25), extra_angles=cols)
    shell, recess = C.split_faces(hood, lambda bm, f: any(v[bm.verts.layers.int["af_disp"]] for v in f.verts))
    hull = profile_mesh(hp, list(np.linspace(hp.z0, hp.z1, 16)), 28, cap0="fan", cap1="fan", dome0=-0.03,
                        dome1=uh(0.25))
    hood_id = b.add(shell, "hood", Bind.rigid("head"), name="hood", hull=hull, hull_pad=True)
    b.add(recess, "hood_in", Bind.rigid("head"), name="hood_open", outline=False)
    b.add(M.loft_z([(hp.z0 + 0.004, ur(4.6), ur(5.3), uh(0.4), 0.0), (hp.z0 + 0.03, ur(3.8), ur(4.4), uh(0.2), 0.0)],
                   sides=24, cap0="fan", cap1="fan"), "hood_in", Bind.rigid("head"), name="hood_floor",
          outline=False, covered_by=hood_id)
    # the opening's turned edge (dark-teal lining roll, lying on the hood just outside the opening): lets the oval
    # read against the hood from the high camera
    ring, ups = [], []
    n = 48
    zc, hz = (OPEN_Z0 + OPEN_Z1) / 2, (OPEN_Z1 - OPEN_Z0) / 2
    for i in range(n):
        t = 2 * math.pi * i / n
        x = math.sin(t)
        a = OPEN_A * 1.035 * x
        h = zc + hz * 1.03 * math.copysign((1 - x * x) ** 0.4, math.cos(t))
        z = hc + uh(h)
        ring.append(hp.point(z, a, 0.0016))
        ups.append(hp.normal(z, a))
    rim = M.sweep(ring, M.superellipse(1, 1, 6, 2.0), scales=[(0.0024, 0.0085)] * n, closed_path=True,
                  cap0=None, cap1=None, up=ups)
    b.add(rim, "cloak", Bind.rigid("head"), name="hood_rim", outline=False, covered_by=hood_id)
    b.add(painted_strip(hp, 180.0, hc - uh(4.6), hc + uh(7.6), 0.010, lambda z, a: 0.0016, steps=14),
          "hood_seam", Bind.rigid("head"), name="hood_seam", outline=False, covered_by=hood_id)
    add_face(b, hood_id)
    # hood tip: a short dark-teal point drooping down the back (web tip blob: from h 2 at the back, base ±0.5 rad
    # at h −4.6, tip 9.6 u behind at h −6.8)
    tip_pts = HB.catmull([Vector((0, ur(5.6), hc + uh(1.6))), Vector((0, ur(7.3), hc - uh(1.3))),
                          Vector((0, uh(8.6), hc - uh(4.4))), Vector((0, uh(9.5), hc - uh(6.5))),
                          Vector((0, uh(9.7), hc - uh(7.2)))], 10)
    radii = [(uh(1.9) * (1 - f) ** 0.8 + 0.005, uh(2.8) * (1 - f) ** 0.6 + 0.004)
             for f in [i / 10 for i in range(11)]]
    tip = M.sweep(tip_pts, M.superellipse(1, 1, 10, 2.0), scales=radii, up=(0, 1, 0), dome0=0.0, dome1=0.005)
    b.add(tip, "cloak", Bind.blend("head", "hood_tip_01", "hood_tip_02", falloff=3.0), name="hood_tip",
          hull_pad=True)


def add_face(b: M.Builder, hood_id: int) -> None:
    """Face in the opening: forehead in the hood's dark, a skin band across the eyes, a dark cloth mask over the
    nose and mouth (web band / mask polygons), two amber eyes."""
    hc = D.hc
    fz = hc + uh(-0.4)
    front = face_r_at(fz)
    depth = 0.05
    cy = -(front - 0.012) + depth                       # face front 1.2 cm behind the hood surface
    face = ell((uh(4.2), depth, uh(4.5)), (0, cy, fz), sides=20, rings=12, exp=2.1)
    zt, zb = hc + uh(BAND_TOP), hc + uh(BAND_BOT)
    rest, top = C.split_faces(face, lambda bm, f: f.calc_center_median().z > zt)
    mask, band = C.split_faces(rest, lambda bm, f: f.calc_center_median().z > zb)
    bind = Bind.rigid("head")
    b.add(top, "hood_in", bind, name="face_shade", outline=False, covered_by=hood_id)
    b.add(band, "skin", bind, name="face", outline=False, covered_by=hood_id)
    b.add(mask, "mask", bind, name="mask", outline=False, covered_by=hood_id)
    # the mask over the nose bridge: a blunt ridge that catches the light below the eyes
    nz = hc + uh(-1.3)
    fr = face_r_at(nz) - 0.012
    nose = ell((0.017, 0.016, 0.026), (0, -fr - 0.002, nz), sides=10, rings=6)
    b.add(nose, "mask", bind, name="nose", outline=False, covered_by=hood_id)
    # amber eyes: slanted slits in the skin band (web 1.5 × 0.9 u at phi ±0.35 about the face)
    for sx, (ex, yy, ez) in zip((1.0, -1.0), eye_points(depth)):
        eye = ell((0.027, 0.006, 0.0115), (0, 0, 0), sides=12, rings=6)
        eye.rotate("y", -sx * 12.0)
        eye.rotate("z", sx * 18.0)
        b.add(eye.translate((ex, yy, ez)), "eye", bind, name="eye", outline=False, covered_by=hood_id)


FACE_DEPTH = 0.05                 # add_face's face ellipsoid depth (eye_points() default)


def eye_points(depth: float | None = None) -> list[Vector]:
    """Rest centres of the two amber eyes (left, right), 3.2 mm proud of the face. The portrait composites the web's
    per-eye glow on them (``hero_ship.render_portrait``)."""
    depth = FACE_DEPTH if depth is None else depth
    ez = D.hc + uh(EYE_H)
    fr = face_r_at(ez) - 0.012
    out = []
    for sx in (1.0, -1.0):
        ex = sx * uh(1.95)
        yy = -fr + depth * (1 - math.sqrt(max(0.0, 1 - (ex / uh(4.2)) ** 2))) - 0.0032
        out.append(Vector((ex, yy, ez)))
    return out


def eyes_point() -> Vector:
    return Vector((0, -(face_r_at(D.hc + uh(EYE_H)) - 0.012) - 0.004, D.hc + uh(EYE_H)))


SPINE = ("pelvis", "spine_01", "spine_02", "spine_03")


def add_torso(b: M.Builder) -> None:
    jp = jerkin_profile()
    zs = list(np.linspace(jp.z0, jp.z1, 18))
    bind = Bind.blend(*SPINE, smooth=0.03)
    tid = b.add(profile_mesh(jp, zs, 30, cap0="fan", cap1="fan", dome1=0.008), "leather", bind, name="jerkin",
                hull=profile_mesh(jp, list(np.linspace(jp.z0, jp.z1, 9)), 22, cap0="fan", cap1="fan", dome1=0.008),
                hull_pad=True)
    # stitched seam down the front (web: dashed LEATHER.shade line at phi 0.25 → slightly right of centre)
    sa = -14.0
    h = TORSO_LEN - 3.2
    while h > 1.2:
        b.add(painted_strip(jp, sa, th(h - 0.75), th(h), 0.0055, lambda z, a: 0.0012, steps=2), "leather_seam", bind,
              name="stitch", outline=False, covered_by=tid)
        h -= 1.5
    # neck (under the hood): the scarf's wrap round the throat
    nb = Bind.blend("spine_03", "neck_01", falloff=4.0)
    b.add(M.loft_z([(D.nz - u(0.6), u(2.6), u(2.5), 0, 0), (D.nz + u(1.0), u(2.55), u(2.45), 0, 0),
                    (D.hc - uh(3.2), u(2.2), u(2.1), 0, 0)], sides=18, cap0="fan", cap1="fan"),
          "mask", nb, name="neck", outline=False, covered_by=tid)
    add_bandolier(b, jp, bind, tid)
    add_belt(b, jp)
    add_capelet(b)


def add_capelet(b: M.Builder) -> None:
    cp = capelet_profile()
    zs = list(np.linspace(cp.z0, cp.z1, 11))
    cbind = HB.blend4(RIG[0], "spine_02", "spine_03", "clavicle_l", "clavicle_r", "neck_01", falloff=5.0, smooth=0.02)
    cid = b.add(profile_mesh(cp, zs, 36, cap0="fan", cap1="fan", dome0=-0.03), "cloak", cbind, name="capelet",
                hull=profile_mesh(cp, list(np.linspace(cp.z0, cp.z1, 7)), 24, cap0="fan", cap1="fan", dome0=-0.03),
                hull_pad=True)
    # the light hem line (web CLOAK.light stroke 0.7 u, just above the hem)
    b.add(C.surface_band(cp, cp.z0 + u(0.75), u(0.6), 0.0022, segs=48, out0=-0.002), "cloak_edge", cbind,
          name="capelet_edge", outline=False, covered_by=cid)
    # brass clasp at the front, right of centre (web phi 0.3, len − 0.4)
    cz = th(TORSO_LEN - 0.5)
    cpt = cp.point(cz, -17.0, 0.0)
    cn = cp.normal(cz, -17.0)
    fr = R.frame(cpt, Vector((0, 0, 1)), cn)
    clasp = M.lathe([(u(1.15), -0.003), (u(1.15), 0.003), (u(0.85), 0.0065), (u(0.35), 0.0078)], sides=16)
    b.add(clasp.transform(fr @ Matrix.Rotation(math.radians(-90), 4, "X")), "brass", cbind, name="clasp",
          outline=False, covered_by=cid)


def _bandolier_path(jp: Profile, n: int = 72):
    """Closed strap path: right shoulder (len − 0.2, −90°) → across the chest to the left hip (2.8, +52°) → round
    the left side → up the back (+128° → −90°). Web strap: phi π/2 → −0.9 (front) / π + 0.9 (back)."""
    h_top, h_bot = TORSO_LEN - 0.2, BAND_BOT_H
    pts = []
    for i in range(n):
        s = i / n
        if s < 0.42:                                   # front run
            k = s / 0.42
            a, h = -90.0 + 142.0 * k, h_top + (h_bot - h_top) * k
        elif s < 0.58:                                 # left side, along the hip
            k = (s - 0.42) / 0.16
            a, h = 52.0 + 76.0 * k, h_bot - 0.25 * math.sin(math.pi * k)
        else:                                          # back run
            k = (s - 0.58) / 0.42
            a, h = 128.0 + 142.0 * k, h_bot + (h_top - h_bot) * k
        pts.append((a, h))
    return pts


def add_bandolier(b: M.Builder, jp: Profile, bind, tid: int) -> None:
    """Dark strap (web MASK, 2.8 u) from the right shoulder across the chest to the left hip, closed round the
    back; four throwing knives on the front run below the capelet (bright steel blades up, brass pommels)."""
    path = _bandolier_path(jp)
    lift = u(0.25)
    pts = [jp.point(th(h), a, lift) for a, h in path]
    ups = [jp.normal(th(h), a) for a, h in path]
    strap = M.strap(pts, u(2.3), 0.0045, up=ups, closed=True)
    sid = b.add(strap, "mask", bind, name="bandolier", hull_pad=True)
    # highlight edge along the upper side of the front run (web MASK.light 0.4 u offset up-left)
    hi = []
    for (a, h), p, n in zip(path, pts, ups):
        if -80.0 < a < 50.0 and h > 3.4:
            hi.append((p, n, a, h))
    if len(hi) > 3:
        hp, hr = [], []
        for p, n, a, h in hi:
            q = jp.point(th(h + 0.95), a, lift + 0.0026)
            hp.append(q)
            hr.append(0.0016)
        b.add(M.tube(hp, hr, sides=4, dome0=0.0, dome1=0.0), "mask_light", bind, name="strap_hi", outline=False,
              covered_by=sid)
    # throwing knives (web: k = 0.3 + 0.14 i along the front run; h = len − 0.2 + (2.2 − len + 0.2) k)
    for i in range(4):
        k = 0.3 + i * 0.14
        a = -90.0 + 142.0 * k
        h = (TORSO_LEN - 0.2) + (BAND_BOT_H - (TORSO_LEN - 0.2)) * k
        z = th(h)
        c = jp.point(z, a, lift + 0.0048)
        n = jp.normal(z, a)
        up = Vector((0, 0, 1)) - n * n.z
        up.normalize()
        fr = R.frame(c, up, n)                    # frame: +Y = up the chest, +Z = out of the surface
        blade = C.plate2d([(-u(0.72), u(0.2)), (u(0.72), u(0.2)), (u(0.72), u(2.9)), (0.0, u(3.9)),
                           (-u(0.72), u(2.9))], 0.0032, bevel=0.0009)
        blade.transform(Matrix.Translation((0, 0, 0.0035)))
        b.add(blade.transform(fr), "steel", bind, name="knife", hull_pad=True)
        loop = M.box((u(1.9), u(1.15), 0.007), (0, u(0.05), 0.0012), bevel=0.0015)
        b.add(loop.transform(fr), "mask", bind, name="knife_loop", hull_pad=True)
        pom = M.box((u(1.25), u(0.95), 0.0075), (0, -u(1.0), 0.0032), bevel=0.0022)
        b.add(pom.transform(fr), "brass", bind, name="knife_pommel", hull_pad=True)


def add_belt(b: M.Builder, jp: Profile) -> None:
    bind = Bind.blend("pelvis", "spine_01", smooth=0.04)
    zb = th(BELT_H)
    # the belt sits over the tasset tops (they start under it): a ring on the larger of the two profiles
    tp = tasset_profile()
    belt_prof = Profile([(z, max(jp.at(z)[0], tp.at(z)[0]) + 0.004, max(jp.at(z)[1], tp.at(z)[1]) + 0.004)
                         for z in np.linspace(zb - u(1.4), zb + u(1.4), 7)], exp=2.3)
    belt_id = b.add(C.surface_band(belt_prof, zb, u(2.0), u(0.4), segs=48, out0=-0.002), "leather_dark", bind,
                    name="belt", hull_pad=True)
    # brass buckle (web phi 0.3: right of centre, 2.2 × 3 u): frame + bar
    a = -17.0
    c = belt_prof.point(zb, a, u(0.4))
    n = belt_prof.normal(zb, a)
    fr = R.frame(c, Vector((0, 0, 1)), n)
    w, h, t = u(2.2), u(2.6), u(0.42)
    parts = [M.box((w, t, 0.006), (0, h / 2 - t / 2, 0.003), bevel=0.0015),
             M.box((w, t, 0.006), (0, -h / 2 + t / 2, 0.003), bevel=0.0015),
             M.box((t, h, 0.006), (w / 2 - t / 2, 0, 0.003), bevel=0.0015),
             M.box((t, h, 0.006), (-w / 2 + t / 2, 0, 0.003), bevel=0.0015)]
    frame = parts[0]
    for p in parts[1:]:
        frame.merge(p)
    b.add(frame.transform(fr), "brass", bind, name="buckle", hull_pad=True)
    bar = M.box((u(0.32), h * 0.86, 0.004), (u(0.15), 0, 0.0035), bevel=0.001)
    b.add(bar.transform(fr), "brass", bind, name="buckle_bar", outline=False, covered_by=belt_id)
    add_pouch(b, belt_prof, zb)


POUCH_PANEL = 2


def add_pouch(b: M.Builder, bp: Profile, zb: float) -> None:
    """Leather hip pouch on the left side behind the hip (web phi −1.9), hanging from the belt, a dark flap and a
    brass stud."""
    a = 112.0
    z = zb - u(1.6)
    c = bp.point(zb, a, 0.0)
    n = Vector((c.x, c.y, 0)).normalized()
    tp = tasset_profile()
    r_out = math.hypot(*tuple(tp.point(z, a))[:2])
    cen = Vector((0, 0, z)) + n * (r_out + u(1.05))
    side = n.cross(Vector((0, 0, 1))).normalized()
    fr = Matrix.Identity(4)
    for i in range(3):
        fr[i][0], fr[i][1], fr[i][2], fr[i][3] = side[i], n[i], (0, 0, 1)[i], cen[i]
    body = M.box((u(3.4), u(1.9), u(3.6)), (0, 0, 0), bevel=0.012, segments=2)
    bind = Bind.rigid(f"tasset_{POUCH_PANEL}_01")          # rides the panel it hangs on
    pid = b.add(body.transform(fr), "leather", bind, name="pouch", hull_pad=True)
    flap = M.box((u(3.55), u(2.05), u(1.5)), (0, u(0.06), u(1.15)), bevel=0.008, segments=2)
    b.add(flap.transform(fr), "leather_dark", bind, name="pouch_flap", outline=False, covered_by=pid)
    stud = ell((u(0.45), u(0.25), u(0.45)), (0, u(1.08), u(0.45)), sides=10, rings=6)
    b.add(stud.transform(fr), "brass", bind, name="pouch_stud", outline=False, covered_by=pid)


# ── tassets ────────────────────────────────────────────────────────────────────────────────────────────
TASSET_SLITS = [-11.5 + 45.0 * i for i in range(8)]       # web slits at phi k π/4 + 0.2
TASSET_TOP, TASSET_HEM = 2.6, -3.4                        # under the belt → the hem (web 2.2 → −3.6)
TASSET_HINGE = 2.4                                        # the panels' hinge line (under the belt): above the
#                                                           thigh's top surface when the thigh is raised level
TASSET_GAP = 2.2                                          # deg: half the slit between panels


def tasset_panels() -> list[tuple[float, float]]:
    out = []
    for i in range(8):
        a0 = TASSET_SLITS[i] + TASSET_GAP
        a1 = TASSET_SLITS[(i + 1) % 8] + (360.0 if i == 7 else 0.0) - TASSET_GAP
        out.append((a0, a1))
    return out


def tasset_hinge(i: int) -> tuple[Vector, Vector]:
    """Rest hinge bone of panel ``i``: from the hinge line at the panel's centre angle down to its hem."""
    a0, a1 = tasset_panels()[i]
    ac = (a0 + a1) / 2
    tp = tasset_profile()
    return tp.point(th(TASSET_HINGE), ac, 0.0), tp.point(th(TASSET_HEM), ac, 0.0)


def tasset_bind(i: int) -> Bind:
    """The panel above the hinge line blends into the pelvis (it tucks under the belt); below it rides its hinge
    bone."""
    bone = f"tasset_{i}_01"
    z0, zh = th(TASSET_TOP), th(TASSET_HINGE)

    def fn(co):
        w = HB.smooth01((z0 - co[:, 2]) / max(z0 - zh, 1e-6) * 0.8 + 0.2)
        W = np.zeros((len(co), 2))
        W[:, 0] = 1.0 - w
        W[:, 1] = w
        return W
    return Bind.custom(["pelvis", bone], fn)


def add_tassets(b: M.Builder) -> None:
    """Eight split dark-leather tassets (web: one flared skirt with eight slits) — separate panels, each inked."""
    tp = tasset_profile()
    z0, z1 = th(TASSET_TOP), th(TASSET_HEM)
    thick = 0.006
    for i, (a0, a1) in enumerate(tasset_panels()):
        rows = [z0 + (z1 - z0) * j / 6 for j in range(7)]
        cols = [a0 + (a1 - a0) * c / 6 for c in range(7)]

        def P(z, a, d, a0=a0, a1=a1):
            # each flap bows out a little at its middle (a stiff leather plate) and rounds its lower corners
            f = (a - a0) / (a1 - a0)
            k = (z0 - z) / (z0 - z1)
            bow = 0.004 * math.sin(math.pi * f) * k
            zz = z + (0.010 * (abs(2 * f - 1) ** 4) * k ** 3)
            return tp.point(zz, a, d + bow)
        outer = [[P(z, a, thick / 2) for a in cols] for z in rows]
        inner = [[P(z, a, -thick / 2) for a in cols] for z in rows]
        hr, hc_ = rows[::2], cols[::2]
        hull = C.thick_patch([[P(z, a, thick / 2) for a in hc_] for z in hr],
                             [[P(z, a, -thick / 2) for a in hc_] for z in hr])
        b.add(C.thick_patch(outer, inner, sub_tags=True), "leather_dark", tasset_bind(i), name="tasset",
              sub_regions={1: "leather_dark", 2: "leather_dark"}, hull=hull, hull_pad=True)


# ── scarf ──────────────────────────────────────────────────────────────────────────────────────────────
def add_scarf(b: M.Builder) -> None:
    """Knot at the back of the neck (left of the spine) and two long tails: A dark teal 57 cm, B teal 47 cm
    (web viewScarf: 1.6 u wide tapering 0.12 per link, a notched end)."""
    k = scarf_knot()
    n = Vector((k.x, k.y, 0)).normalized()
    knot = ell((u(1.75), u(1.25), u(1.55)), k + n * u(0.2), sides=12, rings=8)
    b.add(knot, "cloak", Bind.rigid("spine_03"), name="scarf_knot", hull_pad=True)
    for which, reg in ((0, "cloak"), (1, "hood")):
        pts = scarf_points(which)
        path = HB.catmull(pts, 20)
        nm = ("scarf_a", "scarf_b")[which]
        bones = [f"{nm}_{i:02d}" for i in range(1, 6)]
        outer, inner = [], []
        hz = len(path)
        w0 = u(SCARF_W[which])
        for i, p in enumerate(path):
            f = i / (hz - 1)
            w = w0 * (1 - 0.45 * f)
            out_n = Vector((p.x, p.y, 0)).normalized()
            tang = out_n.cross(Vector((0, 0, 1))).normalized()
            row_o, row_i = [], []
            for s in (-1.0, -0.5, 0.0, 0.5, 1.0):
                q = p + tang * (s * w) + out_n * (-0.003 * (1 - s * s))
                if f > 0.96:                                  # notched (swallowtail) end
                    q.z += u(1.1) * (1 - abs(s)) ** 1.3 - u(0.25)
                row_o.append(q + out_n * 0.003)
                row_i.append(q - out_n * 0.003)
            outer.append(row_o)
            inner.append(row_i)
        tail = C.thick_patch(outer, inner, sub_tags=True)

        def wfn(co, pts=pts):
            kk = np.clip((pts[0].z - co[:, 2]) / (pts[0].z - pts[-1].z), 0.0, 1.0)
            yoke = 1.0 - HB.smooth01(kk / 0.06)
            return HB.top4(HB.chain_weights(kk, 5, yoke))
        b.add(tail, reg, Bind.custom(["spine_03"] + bones, wfn), name=f"scarf_tail{which}",
              sub_regions={1: reg, 2: "cloak"}, hull_pad=True)


# ── limbs ──────────────────────────────────────────────────────────────────────────────────────────────
def add_arm(b: M.Builder, rig: R.Rig, side: str) -> None:
    """Wrapped upper arm with a leather spaulder; leather bracer (10–85 % of the forearm) with a brass ring."""
    J = rig.joints
    S, E, W = J[f"shoulder_{side}"], J[f"elbow_{side}"], J[f"wrist_{side}"]
    ua, la, cl = f"upperarm_{side}", f"lowerarm_{side}", f"clavicle_{side}"
    dua = (E - S).normalized()
    dla = (W - E).normalized()
    Lu, Lf = (E - S).length, (W - E).length
    ap = [(S - dua * 0.02).lerp(E + dua * 0.012, i / 5) for i in range(6)]
    arm_id = b.add(M.tube(ap, [u(2.1) + (u(1.95) - u(2.1)) * i / 5 for i in range(6)], sides=16), "wrap",
                   Bind.blend(cl, ua, la, falloff=5.0), name=f"arm_{side}", hull_pad=True)
    # wrap turns (diagonal WRAP.shade lines round the upper arm)
    fr_u = HB.axis_frame_x(S, dua, Vector((0, 0, 1)))
    for j in range(2):
        f0 = 0.45 + 0.22 * j
        pts = []
        for c in range(11):
            t = 2 * math.pi * c / 10
            f = f0 + 0.06 * math.cos(t)
            r = u(2.1) + (u(1.95) - u(2.1)) * f + 0.0011
            pts.append(fr_u @ Vector((r * math.cos(t), r * math.sin(t), f * Lu)))
        b.add(M.tube(pts, [0.0018] * 11, sides=4, dome0=0.0, dome1=0.0), "wrap_line",
              Bind.blend(cl, ua, la, falloff=5.0), name="wrap_line", outline=False, covered_by=arm_id)
    # spaulder: a rounded leather pad over the top / outside of the upper arm (web ellipse 3.4 × 2.6 u at 22 %)
    sx = 1.0 if side == "l" else -1.0
    out = Vector((sx, 0, 0.55)).normalized()
    out = (out - dua * out.dot(dua)).normalized()
    pc = S + dua * (0.30 * Lu) + out * u(0.55)
    pad = ell((u(2.5), u(2.85), u(2.3)), (0, 0, 0), sides=16, rings=10, exp=2.2)
    fr = Matrix.Identity(4)
    side_v = dua.cross(out).normalized()
    for i in range(3):
        fr[i][0], fr[i][1], fr[i][2], fr[i][3] = side_v[i], dua[i], out[i], pc[i]
    pid = b.add(pad.transform(fr), "leather", Bind.blend(ua, cl, falloff=6.0), name=f"spaulder_{side}",
                hull_pad=True)
    seam = []
    for c in range(9):
        t = -0.9 + 1.8 * c / 8
        seam.append(pc + dua * (u(1.1) * t) + out * (u(2.3) + 0.0012) - side_v * u(0.3))
    b.add(M.tube(seam, [0.0016] * 9, sides=4, dome0=0.0, dome1=0.0), "leather_seam",
          Bind.blend(ua, cl, falloff=6.0), name="spaulder_seam", outline=False, covered_by=pid)
    # forearm wrap under the bracer
    b.add(M.tube([E - dla * 0.012, W - dla * 0.004], [u(1.72), u(1.5)], sides=14), "wrap",
          Bind.blend(la, f"hand_{side}", falloff=8.0), name=f"forearm_{side}")
    # leather bracer 10 → 85 %, lipped ends
    fs = [0.08, 0.13, 0.30, 0.52, 0.72, 0.80, 0.83]
    rs = [u(1.95), u(2.18), u(2.14), u(2.04), u(1.96), u(2.06), u(1.9)]
    bid = b.add(M.tube([E + dla * (f * Lf) for f in fs], rs, sides=16, dome0=0.0, dome1=0.0), "leather",
                Bind.rigid(la), name=f"bracer_{side}", hull_pad=True)
    # brass ring at the middle (web arc r 2.2 at 50 %)
    fr_f = HB.axis_frame_x(E, dla, Vector((0, 0, 1)))
    ring = [fr_f @ Vector(((u(2.1) + 0.0012) * math.cos(2 * math.pi * c / 16),
                           (u(2.1) + 0.0012) * math.sin(2 * math.pi * c / 16), 0.5 * Lf)) for c in range(16)]
    b.add(M.sweep(ring, M.superellipse(1, 1, 4, 2.0, phase=0.785), scales=[(0.0032, 0.0045)] * 16, closed_path=True,
                  cap0=None, cap1=None), "brass", Bind.rigid(la), name="bracer_ring", outline=False, covered_by=bid)
    add_hand(b, rig, side)


def add_hand(b: M.Builder, rig: R.Rig, side: str) -> None:
    """Both fists closed round a dagger grip (skin #D9A47C) with the dark glove band across the knuckles."""
    J = rig.joints
    W, T, G = J[f"wrist_{side}"], J[f"handtip_{side}"], J[f"grip_{side}"]
    hd = (T - W).normalized()
    hbn = f"hand_{side}"
    wrist = M.tube([W - hd * 0.028, W + hd * 0.014], [0.028, 0.031], sides=12)
    b.add(wrist, "skin", Bind.blend(f"lowerarm_{side}", hbn, falloff=6.0), name=f"wrist_{side}")
    grip = Vector((0, -1, 0))
    zx = (grip - hd * grip.dot(hd)).normalized()
    xx = hd.cross(zx)
    fr = Matrix.Identity(4)
    for i in range(3):
        fr[i][0], fr[i][1], fr[i][2], fr[i][3] = xx[i], hd[i], zx[i], G[i]
    fist = ell((0.040, 0.047, 0.043), (0, 0.005, 0), sides=14, rings=8, exp=2.5, exp_v=2.3)
    fid = b.add(fist.transform(fr), "skin", Bind.rigid(hbn), name=f"hand_{side}", hull_pad=True)
    thumb = M.capsule((0.026, -0.011, 0.022), (0.013, 0.024, 0.035), 0.014, 0.011, sides=8)
    b.add(thumb.transform(fr), "skin", Bind.rigid(hbn), name=f"thumb_{side}", outline=False, covered_by=fid)
    band = ell((0.0425, 0.0105, 0.0455), (0, -0.004, 0), sides=14, rings=6, exp=2.5)
    b.add(band.transform(fr), "mask", Bind.rigid(hbn), name=f"glove_band_{side}", outline=False, covered_by=fid)


def add_leg(b: M.Builder, rig: R.Rig, side: str) -> None:
    """Wrapped leg (thigh + shin), leather knee pad, three dark cross-straps on the shin, soft leather boot."""
    J = rig.joints
    Hp, Kn, A = J[f"hip_{side}"], J[f"knee_{side}"], J[f"ankle_{side}"]
    th_, ca, ft, bl = f"thigh_{side}", f"calf_{side}", f"foot_{side}", f"ball_{side}"
    top = Hp + Vector((0, 0, -0.005))
    lbind = Bind.blend("pelvis", th_, ca, ft, falloff=5.0)
    # densely ringed (the knee bends smoothly; the tasset / scarf solvers sample its surface)
    A2 = A + Vector((0, 0, 0.065))
    lp = [top.lerp(Kn, i / 7) for i in range(7)] + [Kn.lerp(A2, i / 6) for i in range(7)]
    lr = [0.056, 0.059] + [0.059 + (0.052 - 0.059) * i / 5 for i in range(1, 6)] + \
        [0.052 + (0.040 - 0.052) * i / 6 for i in range(7)]
    leg_id = b.add(M.tube(lp, lr, sides=16), "wrap", lbind, name=f"leg_{side}",
                   hull=M.tube(lp[::3] + [lp[-1]], lr[::3] + [lr[-1]], sides=12), hull_pad=True)
    # wrap turns on the thigh
    dth = (Kn - top).normalized()
    Lt = (Kn - top).length
    fr_t = HB.axis_frame_x(top, dth, Vector((0, -1, 0)))
    for j in range(2):
        f0 = 0.34 + 0.26 * j
        pts = []
        for c in range(13):
            t = 2 * math.pi * c / 12
            f = f0 + 0.05 * math.cos(t)
            r = 0.061 + (0.052 - 0.061) * f + 0.0011
            pts.append(fr_t @ Vector((r * math.cos(t), r * math.sin(t), f * Lt)))
        b.add(M.tube(pts, [0.002] * 13, sides=4, dome0=0.0, dome1=0.0), "wrap_line", lbind, name="wrap_line",
              outline=False, covered_by=leg_id)
    # knee pad
    sx = 1.0 if side == "l" else -1.0
    kp = ell((0.047, 0.026, 0.050), (0, 0, 0), sides=14, rings=8, exp=2.3)
    kp.rotate("x", -8.0)
    b.add(kp.translate(Kn + Vector((sx * 0.004, -0.040, 0.004))), "leather", Bind.blend(th_, ca, falloff=6.0),
          name=f"kneepad_{side}", hull_pad=True)
    # shin cross-straps (web: three diagonal LEATHER.shade strokes at 20 / 45 / 70 %)
    dsh = (A - Kn).normalized()
    Ls = (A - Kn).length
    fr_s = HB.axis_frame_x(Kn, dsh, Vector((0, -1, 0)))
    for j in range(3):
        f0 = 0.25 + 0.22 * j
        pts = []
        for c in range(13):
            t = 2 * math.pi * c / 12
            f = f0 + 0.075 * math.cos(t)
            r = 0.052 + (0.040 - 0.052) * (f * Ls + 0.0) / (Ls + 0.065) + 0.0014
            pts.append(fr_s @ Vector((r * math.cos(t), r * math.sin(t), f * Ls)))
        b.add(M.sweep(pts[:-1], M.superellipse(1, 1, 4, 2.0, phase=0.785), scales=[(0.0022, 0.0055)] * 12, closed_path=True,
                      cap0=None, cap1=None), "leather_dark", lbind, name="shin_strap", outline=False,
              covered_by=leg_id)
    boot = M.boot(D.spec.foot_len, 0.094, 0.084, shaft=0.07, toe_up=0.03)
    boot_id = b.add(boot.translate((A.x, 0, 0)), "leather", Bind.blend(ca, ft, bl, falloff=5.0), name=f"boot_{side}",
                    hull_pad=True)
    cuff = M.loft_z([(0.118, 0.049, 0.047), (0.142, 0.055, 0.053), (0.152, 0.053, 0.051)], sides=14, cap0=None,
                    cap1="fan")
    b.add(cuff.translate((A.x, 0.004, 0)), "leather_dark", Bind.blend(ca, ft, falloff=5.0), name=f"boot_cuff_{side}")
    del boot_id


# ── items ──────────────────────────────────────────────────────────────────────────────────────────────
def grip_axes(rig: R.Rig, side: str) -> Vector:
    """The forearm direction in the item (weapon bone) frame under ``common.to_pose``'s hammer grip: (a, 0, c)
    — a blade's width axis (edge toward the knuckles), a bow's plane (string toward the elbow)."""
    a, c = C._grip_coeffs(rig, side)
    return Vector((a, 0.0, c)).normalized()


BLADE0, BLADE1 = 2.8, 14.2        # web blade from −1.6 to −13 (grip centre at web +1.2 → item y)


def _bez(p0, p1, p2, t):
    return ((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0],
            (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1])


def _edge_x(p0, p1, p2, y):
    """x of a quadratic Bézier edge at height y (monotonic in y)."""
    lo, hi = 0.0, 1.0
    for _ in range(40):
        m = (lo + hi) / 2
        if _bez(p0, p1, p2, m)[1] < y:
            lo = m
        else:
            hi = m
    return _bez(p0, p1, p2, (lo + hi) / 2)[0]


def build_dagger(pal: Palette, width_axis: Vector, mirror: bool, name: str) -> tuple[bpy.types.Object, list]:
    """Curved leaf-shaped dagger (web dagger :91-121; blade 11.4 u ≈ 33 cm, brass guard and pommel, dark grip) in
    weapon-bone space: the fist at the origin, +Y = the blade; the blade's width lies along the hammer grip's
    forearm axis (``width_axis``), so the edges face the knuckles and the elbow. ``mirror``: the left hand's copy
    (the curve mirrored)."""
    b = M.Builder(name, pal, prefix="rogue_dagger" + ("_l" if mirror else ""))
    b.regions(**DAGGER_REGIONS)
    sgn = -1.0 if mirror else 1.0
    # canonical frame (X width, Y blade, Z flat normal) → the grip frame: X onto the forearm axis
    w = width_axis.normalized()
    z = w.cross(Vector((0, 1, 0)))
    rot = Matrix.Identity(4)
    for i in range(3):
        rot[i][0], rot[i][1], rot[i][2] = w[i], (0, 1, 0)[i], z[i]

    def add(part, *a, **k):
        return b.add(part.transform(rot), *a, **k)
    L0, L1 = (-1.2 * sgn, BLADE0), (0.6 * sgn, BLADE1)
    cl, cr = (-2.4 * sgn, 9.0), (1.6 * sgn, 8.35)
    R0 = (1.2 * sgn, BLADE0)
    ys = [BLADE0 + (BLADE1 - BLADE0) * (1 - (1 - i / 16) ** 1.15) for i in range(17)]
    ys[-1] = BLADE1 - 0.05
    cols = 9

    def surf(x, y, xl, xr, side_):
        hw = abs(xr - xl) / 2
        xm = (xl + xr) / 2
        q = (x - xm) / max(hw, 1e-6)
        t = (0.42 - 0.24 * (y - BLADE0) / (BLADE1 - BLADE0)) * u(1.0)
        return side_ * (t * max(0.0, 1 - q * q) ** 0.75 + 0.0007)
    top, bot = [], []
    for y in ys:
        xa = _edge_x(L0, cl, L1, y)
        xb = _edge_x(R0, cr, L1, y)
        xl, xr = min(xa, xb), max(xa, xb)
        if xr - xl < 0.12:
            xm = (xl + xr) / 2
            xl, xr = xm - 0.06, xm + 0.06
        rt, rb = [], []
        for c in range(cols):
            x = xl + (xr - xl) * c / (cols - 1)
            rt.append(Vector((u(x), u(y), surf(x, y, xl, xr, 1.0))))
            rb.append(Vector((u(x), u(y), surf(x, y, xl, xr, -1.0))))
        top.append(rt)
        bot.append(rb)
    blade = C.thick_patch(top, bot)
    bid = add(blade, "steel", name="blade", hull_pad=True)
    # poison edge line just inside the curved edge (web: 0.45 u stroke from (−1.1, −2.2) to the tip), both faces
    for side_ in (1.0, -1.0):
        rows = []
        for y in [BLADE0 + 0.6 + (BLADE1 - 0.7 - BLADE0 - 0.6) * i / 14 for i in range(15)]:
            xa = _edge_x(L0, cl, L1, y)
            xb = _edge_x(R0, cr, L1, y)
            xl, xr = (xa, xb) if not mirror else (xb, xa)
            row = []
            for f in (0.24, 0.33):
                x = xl + (xr - xl) * f
                xx0, xx1 = min(xa, xb), max(xa, xb)
                row.append(Vector((u(x), u(y), surf(x, y, xx0, xx1, side_) + side_ * 0.0005)))
            rows.append(row)
        sheet = HB.sheet_grid(rows)
        if side_ < 0:
            import bmesh
            bmesh.ops.reverse_faces(sheet.bm, faces=list(sheet.bm.faces))
        add(sheet, "poison", name="poison_edge", outline=False, covered_by=bid)
    # guard (web ±2.8 u): +2.1 / −1.25 u in 3D — its quillons lie along the forearm axis under the hammer grip
    # (+x toward the knuckles) and the web width put the back quillon into the bracer
    guard = C.plate2d([(-u(0.9), u(1.8)), (u(1.6), u(1.8)), (u(2.1), u(3.0)), (-u(1.25), u(3.0))], u(1.0),
                      bevel=0.003)
    add(guard, "brass", name="guard", hull_pad=True)
    gid = add(M.tube([(0, -u(2.0), 0), (0, u(1.9), 0)], [u(0.78), u(0.8)], sides=10, dome0=0.0, dome1=0.0),
                "grip", name="grip", hull_pad=True)
    for yy in (-u(1.2), 0.0, u(1.2)):
        ringp = M.tube([(0, yy - 0.0025, 0), (0, yy + 0.0025, 0)], [u(0.84), u(0.84)], sides=10, dome0=0.0,
                       dome1=0.0)
        add(ringp, "grip_wrap", name="grip_wrap", outline=False, covered_by=gid)
    add(ell((u(1.1), u(1.0), u(1.1)), (0, -u(2.65), 0), sides=12, rings=8), "brass", name="pommel", hull_pad=True)
    obj = asset.finish_mesh(b, pal, "weapon")
    obj.name = name
    tip = rot @ Vector((u(0.6 * sgn), u(BLADE1), 0))
    mid = rot @ Vector((u(-0.3 * sgn), u((BLADE0 + BLADE1) / 2), 0))
    return obj, [{"name": "tip", "pos": tuple(tip)}, {"name": "mid", "pos": tuple(mid)}]


BOW_HALF = 15.0                   # web units: tip-to-tip ≈ 88 cm (a short recurve, spec §3.3 NEW optional)


def bow_curve(f: float) -> float:
    """Limb centre-line offset toward the archer (units) at ``f`` = |y| / half length: the braced C, the tips
    recurving back toward the target."""
    ks = [0.0, 0.12, 0.3, 0.5, 0.72, 0.86, 0.95, 1.0]
    vs = [0.0, 0.05, 0.45, 1.45, 3.05, 3.95, 3.8, 3.3]
    return C._hermite(ks, vs, f)


def build_bow(pal: Palette, forearm: Vector) -> tuple[bpy.types.Object, list]:
    """Short recurve flash-bow in ``weapon_l`` space: the grip in the fist, +Y = the upper limb, the string side
    toward the elbow (−``forearm``). Dark leather-laminated limbs, a teal grip wrap, brass riser rings and tip caps,
    glowing string nocks (the string itself is runtime FX: a beam string_top → the drawing hand → string_bottom)."""
    b = M.Builder(BOW, pal, prefix="rogue_bow")
    b.regions(**BOW_REGIONS)
    S = (-forearm).normalized()                       # toward the archer
    Y = Vector((0, 1, 0))
    Lb = Y.cross(S).normalized()                      # limb width axis
    L = BOW_HALF

    def P(y):
        f = abs(y) / L
        return Y * u(y) + S * u(bow_curve(f))
    n = 26
    for sgn, nm in ((1.0, "limb_top"), (-1.0, "limb_bot")):
        ys = [sgn * L * (i / n) ** 1.0 for i in range(n + 1)]
        path = [P(y) for y in ys]
        sc = []
        for y in ys:
            f = abs(y) / L
            depth = u(1.05) * (1 - HB.smooth01((f - 0.08) / 0.18)) + u(0.62 - 0.26 * f) * HB.smooth01((f - 0.08) / 0.18)
            width = u(1.0) * (1 - HB.smooth01((f - 0.1) / 0.2)) + u(1.15 - 0.6 * f) * HB.smooth01((f - 0.1) / 0.2)
            sc.append((depth, width))
        limb = M.sweep(path, M.superellipse(1, 1, 10, 2.4), scales=sc, up=S, dome0=0.0, dome1=u(0.35))
        lid = b.add(limb, "limb", name=nm, hull_pad=True)
        # lamination line along the belly (archer side)
        line = [P(y) + S * (sc[i][0] + 0.0008) for i, y in enumerate(ys) if 0.2 < abs(y) / L < 0.93]
        b.add(M.tube(line, [0.0018] * len(line), sides=4, dome0=0.0, dome1=0.0), "limb_line", name="limb_line",
              outline=False, covered_by=lid)
        # brass tip cap + the glowing nock
        tip = P(sgn * L * 0.985)
        b.add(ell((u(0.62), u(0.85), u(0.62)), tip, sides=10, rings=6), "brass", name="tip_cap", hull_pad=True)
        b.add(ell((u(0.32), u(0.32), u(0.32)), tip + S * u(0.45), sides=8, rings=6), "nock", name="nock",
              hull_pad=True)
    # riser: teal grip wrap + brass rings
    gid = b.add(M.tube([P(-2.0) - Y * 0.0, P(2.0)], [(u(1.18), u(1.08)), (u(1.18), u(1.08))], sides=12, up=S,
                       dome0=0.0, dome1=0.0), "grip", name="bow_grip", hull_pad=True)
    for yy in (-2.2, 2.2):
        b.add(M.tube([P(yy) - Y * u(0.25), P(yy) + Y * u(0.25)], [(u(1.32), u(1.2))] * 2, sides=12, up=S,
                     dome0=0.0, dome1=0.0), "brass", name="riser_ring", hull_pad=True)
    del gid
    obj = asset.finish_mesh(b, pal, "weapon")
    obj.name = BOW
    top, bot = P(L * 0.985) + S * u(0.45), P(-L * 0.985) + S * u(0.45)
    rest = (top + bot) / 2
    socks = [{"name": "string_top", "pos": tuple(top)}, {"name": "string_bottom", "pos": tuple(bot)},
             {"name": "nock", "pos": tuple(rest + Y * u(1.6))},
             {"name": "arrow_rest", "pos": tuple(P(2.4) - S * u(0.9))},
             {"name": "tip", "pos": tuple(P(L))}, {"name": "mid", "pos": tuple(P(L * 0.5))}]
    obj["af_sockets"] = json.dumps([{"name": x["name"], "pos": list(x["pos"])} for x in socks])
    return obj, socks


QUIVER_AXIS = (-0.60, 0.30, 0.74)  # up the quiver: up, out to the right, back (opening behind the right hip)
QUIVER_LEN = 9.5                  # units (≈ 27 cm body) + the fletchings


def quiver_frame() -> tuple[Vector, Vector, Vector, Vector, Vector]:
    """Rest geometry (armature space): bottom-end centre (resting on the belt just right of the spine), the attach /
    socket point (the strap, 35 % up), the axis up the quiver, its side and outward (away from the back) axes."""
    ax = Vector(QUIVER_AXIS).normalized()
    z = th(BELT_H - 0.2)
    a = -172.0
    jp, tp = jerkin_profile(), tasset_profile()
    r = max(math.hypot(*tuple(jp.point(z, a))[:2]), math.hypot(*tuple(tp.point(z, a))[:2])) + u(0.4) + 0.004
    d = Vector((math.sin(math.radians(a)), -math.cos(math.radians(a)), 0.0))
    B = d * (r + u(1.3) + 0.010) + Vector((0, 0, z))
    side = ax.cross(Vector((0, 1, 0))).normalized()
    out = side.cross(ax).normalized()
    if out.y < 0:
        side, out = -side, -out
    Q = B + ax * (0.35 * u(QUIVER_LEN))
    return B, Q, ax, side, out


def build_quiver(pal: Palette, rig: R.Rig) -> tuple[bpy.types.Object, list, Matrix]:
    """Hip quiver on the ``quiver`` socket (pelvis): dark leather tube with leather bands, a brass rim and five
    teal-fletched arrows. Authored in the socket frame (= the pelvis bone frame at the socket point, the UE
    socket convention: attach with no extra rotation)."""
    b = M.Builder(QUIVER, pal, prefix="rogue_quiver")
    b.regions(**QUIVER_REGIONS)
    B, Q, ax, side, out = quiver_frame()
    # parts are authored in armature rest space round Q and moved into the socket frame (the pelvis bone's rest
    # rotation at Q) as they are added
    Rb = rig.obj.data.bones["pelvis"].matrix_local.to_3x3()
    to_sock = (Matrix.Translation(Q) @ Rb.to_4x4()).inverted()

    def add(part, *a, **k):
        return b.add(part.transform(to_sock), *a, **k)
    Lq = u(QUIVER_LEN)
    bot = B
    topc = B + ax * Lq
    # world-space parts around Q, moved into the socket frame at the end
    rr = [u(1.2), u(1.32), u(1.42), u(1.48)]
    pts = [bot + ax * (Lq * f) for f in (0.0, 0.25, 0.75, 1.0)]
    qid = add(M.tube(pts, [(r, r * 0.86) for r in rr], sides=14, up=out, dome0=u(0.5), dome1=0.0, cap1="fan"),
                "leather_dark", name="quiver", hull_pad=True)
    for f, w in ((0.12, 0.7), (0.55, 1.6)):
        c = bot + ax * (Lq * f)
        r = u(1.3 + 0.25 * f) + 0.0022
        band = M.tube([c - ax * (u(w) / 2), c + ax * (u(w) / 2)], [(r, r * 0.86)] * 2, sides=14, up=out, dome0=0.0,
                      dome1=0.0)
        add(band, "leather", name="quiver_band", outline=False, covered_by=qid)
    rim = M.tube([topc - ax * u(0.45), topc + ax * u(0.1)], [(u(1.6), u(1.6) * 0.86)] * 2, sides=14, up=out,
                 dome0=0.0, dome1=0.0)
    add(rim, "brass", name="quiver_rim", hull_pad=True)
    # strap loop to the belt
    add(M.box((u(1.0), u(2.6), 0.006), (0, 0, 0), bevel=0.002).transform(
        Matrix.Translation(Q - out * u(1.25)) @ HB.basis(side, ax).to_4x4()), "leather", name="quiver_strap",
        hull_pad=True)
    # five arrows: shafts out of the opening, three-vane fletchings
    rnd = [(0.0, 0.0, 3.2), (0.55, 0.35, 2.7), (-0.55, 0.3, 2.9), (0.3, -0.5, 2.4), (-0.35, -0.45, 2.75)]
    for i, (dx, dz, h) in enumerate(rnd):
        base = topc + side * u(dx) + out * u(dz) * 0.86
        end = base + ax * u(h) + side * u(dx * 0.15)
        add(M.tube([base - ax * u(1.0), end], [u(0.2), u(0.2)], sides=6, dome0=0.0, dome1=0.0), "shaft",
              name="arrow_shaft", hull_pad=True)
        reg = "fletch" if i % 2 == 0 else "fletch_b"
        d = (end - base).normalized()
        for k in range(3):
            ang = 2 * math.pi * k / 3 + i * 0.7
            v = side * math.cos(ang) + out * math.sin(ang)
            v = (v - d * v.dot(d)).normalized()
            vane = C.plate2d([(0.0, -u(1.9)), (u(0.62), -u(1.6)), (u(0.72), -u(0.15)), (0.0, 0.0)], 0.0022)
            vane.transform(Matrix.Translation(end - d * u(0.25)) @ HB.basis(v, d).to_4x4())
            add(vane, reg, name="fletch", hull_pad=True)
    obj = asset.finish_mesh(b, pal, "weapon")
    obj.name = QUIVER
    socks = [{"name": "arrow_draw", "pos": tuple(to_sock @ (topc + ax * u(2.5)))}]
    return obj, socks, Q


# ── assembly ───────────────────────────────────────────────────────────────────────────────────────────
def build_all(lod1: bool = False):
    scene.reset()
    pal = Palette("Heroes")
    spec = build_spec()
    rig = R.build_humanoid(spec)
    R.add_socket(rig, "eyes", "head", eyes_point())
    body = build_body(pal, rig)
    wr, wl = grip_axes(rig, "r"), grip_axes(rig, "l")
    mirror_ok = abs(wr.x + wl.x) < 1e-3 and abs(wr.z - wl.z) < 1e-3
    dag, ds = build_dagger(pal, wr, False, DAGGER)
    dag_l, dls = build_dagger(pal, wl if not mirror_ok else Vector((-wr.x, 0, wr.z)), True, DAGGER_L)
    bow, bs = build_bow(pal, wl)
    quiver, qs, Q = build_quiver(pal, rig)
    R.add_socket(rig, "quiver", "pelvis", Q)
    out = dict(pal=pal, rig=rig, body=body, items={DAGGER: dag, DAGGER_L: dag_l, BOW: bow, QUIVER: quiver},
               sockets={DAGGER: ds, DAGGER_L: dls, BOW: bs, QUIVER: qs})
    if lod1:
        out["body1"] = build_body(pal, rig, lod=1)
    return out


def attach_items(rig, items) -> None:
    R.attach_to_bone(items[DAGGER], rig, "weapon_r")
    R.attach_to_bone(items[DAGGER_L], rig, "weapon_l")
    R.attach_to_bone(items[BOW], rig, "weapon_l")
    bone, m = HB.socket_local(rig, "quiver")
    HB.attach_at(items[QUIVER], rig, bone, m)


def part_report(parts) -> str:
    agg: dict = {}
    for p in parts:
        k = p["name"].rstrip("_lr").rstrip("0123456789")
        agg[k] = agg.get(k, 0) + p["tris"]
    return ", ".join(f"{k} {v}" for k, v in sorted(agg.items(), key=lambda kv: -kv[1]))


SCRATCH = Path("/tmp/claude-0/-home-user-abyssfire/b3225f99-25a3-5b78-996f-643be8685889/scratchpad/heroes2/rogue")


def hero_def():
    import rogue_ship
    return rogue_ship.HERO


if __name__ == "__main__":
    import hero_ship
    sys.exit(hero_ship.cli(hero_def(), sys.argv, SCRATCH))

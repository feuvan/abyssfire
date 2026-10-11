"""渊火术士 Abyssfire Arcanist — ``SK_Hero_Mage`` (spec art-inventory-ch1.md §3.2, §3.4, §3.5; DECISIONS R2/R4/R5/R11).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/heroes/mage.py          # ship
    #   --no-previews / --no-verify            skip the review renders / the FBX re-import check
    #   --quick [--facing=se] [--pose=ready]   mesh iteration: posed close-up, turnaround, game crop → scratch
    #   --anims [Clip …] [--facing=se]         bake + contact sheets → scratch
    #   --frames Clip:frame … [--facings=se,side,back]   large debug stills → scratch

The hooded arcanist: an indigo robe with gold trim; a deep rounded hood whose face opening (gold-rimmed) shows a
face in the hood's shadow — forehead dark, cheeks and nose lit, two glowing eyes — and a silver pointed beard
spilling down the chest; the mantle-coloured liripipe hanging down the back; a dark mantle over the shoulders with a
gold edge and a violet clasp gem; a crimson sash with a knot and two tails; bell sleeves with gold cuffs; a skirt
flaring from the waist round both feet, its front opening showing the lavender lining with three gold rune
diamonds; a spell tome hanging on a cord at the right hip; leggings and boots under the hem. The ``SKEL_Human`` rig
carries the robe skirt / sash / hood-tip / beard / tome chains; the gnarled staff (``SM_Hero_Mage_Staff``, two-pronged
claw) and the floating arcane crystal (``SM_Hero_Mage_Crystal``, on the staff's ``crystal`` socket) are separate
``Weapons/`` meshes; every clip of spec §3.4 + DECISIONS R5 (signatures Cast_Blizzard, Cast_Meteor) is in
``mage_clips.py``; ``hero_ship.py`` bakes, gates, exports and reviews.

Web source: ``src/graphics/sprites/players/PlayerMage.ts`` (palette :56-71, proportions :151-156, robe :158-269, hood
:323-418, staff :103-144, poses :472-628). Web rig units → metres with ``U`` (2.96 cm × fit).
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
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import asset, mesh as M, rig as R, scene  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402
from kit.rig import Bind, HumanoidSpec  # noqa: E402

import common as C  # noqa: E402
from common import Profile  # noqa: E402
import hero_base as HB  # noqa: E402
from hero_base import ell, profile_mesh, painted_strip  # noqa: E402

ASSET = "SK_Hero_Mage"
TOKEN = "Hero_Mage"
STAFF = "SM_Hero_Mage_Staff"
CRYSTAL = "SM_Hero_Mage_Crystal"
HEIGHT = 1.71
U0 = 0.0296                       # web mage rig unit (spec §0.1)
# camera-aware proportion tune (the warrior's HELM_K): at the W1 pitch −50° the body foreshortens and the hood does
# not — the hood loft is scaled about the head centre and the freed crown height given to the torso and the legs
HOOD_K = 0.90
TORSO_ADD, LEG_ADD = 0.45, 0.20

# ── palette (spec §3.2; (s, l) = tone shadow / light) ──────────────────────────────────────────────────
ROBE = Region("#4A3690", .42, .30)
MANTLE = Region("#2F2360", .42, .20)
SKIN = Region("#E6B791", .30, .25)
REGIONS = dict(
    robe=ROBE,
    robe_seam=ROBE.shade_region(),                 # back seam / fold strokes (web ROBE.shade)
    mantle=MANTLE,                                 # mantle, hood tip, sleeve / skirt lining (spec: ROBE_FAR region)
    lining=Region("#8A74D8", .42, .35),
    trim=Region("#DCB24C", .42, .50),
    rune=Region("#FFE6A0", .20, .55),              # rune diamonds (web TRIM.light fill)
    sash=Region("#A0283C"),
    sash_b=Region("#6E1A2A"),
    skin=SKIN,
    # forehead in the hood's shadow (web: a rgba(26,16,48,.6) brow stroke over the skin): the skin's shade tone
    skin_shade=Region("#86596A", .30, .10),
    hood_in=Region("#1A1030", .30, .08),           # hood opening interior
    # silver beard: shadowAmt .16 (the blade rule): seen from the high camera most of the beard faces down-right of
    # the key light, and the spec's .30 shade read as a grey cloth bib instead of a white beard
    beard=Region("#D9DBE6", .16, .45),
    beard_line=Region("#A8ACC2", .20, .25),        # beard strand grooves
    leggings=Region("#33264F"),
    boot=Region("#3A2A24"),
    tome=Region("#6A2B24"),
    pages=Region("#F0E6C8", .30, .30),
    eye=Region("#E8FBFF", .0, .0, e=1.0),          # glowing eyes (emissive + the runtime eye glow card #9FE6FF)
    gem=Region("#B48CFF", .0, .0, e=0.9),          # mantle clasp gem
)
STAFF_REGIONS = dict(
    wood=Region("#74492A", .42, .30),
    wood_dark=Region("#74492A").line_region(0.3),  # bark grooves
    trim=Region("#DCB24C", .42, .50),
)
CRYSTAL_REGIONS = dict(
    crystal=Region("#9FE6FF", .25, .60, e=0.45),   # arcane crystal: shaded facets that still glow (bloom)
    crystal_hot=Region("#E8FBFF", .0, .0, e=1.0),  # the white highlight sliver (web rgba(255,255,255,.7–1))
)

HULL_INK_O = 0.4                  # hull colour = 60 % ink #120C18 + 40 % the region's line tone (the warrior's)
REGIONS, STAFF_REGIONS, CRYSTAL_REGIONS = ({k: replace(r, o=HULL_INK_O) for k, r in d.items()}
                                           for d in (REGIONS, STAFF_REGIONS, CRYSTAL_REGIONS))


# ── dimensions ─────────────────────────────────────────────────────────────────────────────────────────
HOOD_TOP = 8.8                    # web HOOD_RINGS top (units above the head centre)


class Dims:
    def __init__(self):
        base = HumanoidSpec(name=ASSET, skeleton="SKEL_Human", thigh=(12 + LEG_ADD) * U0,
                            shin=(12 + LEG_ADD) * U0, ankle=2.0 * U0, torso=(16 + TORSO_ADD) * U0, neck=7.0 * U0,
                            head=2 * (HOOD_TOP + 0.5) * HOOD_K * U0, hip_half=0.082, shoulder_half=5.4 * U0,
                            upper_arm=9 * U0, fore_arm=8.5 * U0, hand=0.10, foot_len=0.25)
        self.k = HEIGHT / base.crown_height()
        self.spec = base.fit_height(HEIGHT)
        s = self.spec
        self.U = U0 * self.k
        self.hip_z = s.ankle + s.shin + s.thigh
        self.pz = self.hip_z + s.pelvis_offset
        self.nz = self.pz + s.torso
        self.hc = self.nz + s.neck
        self.ts = (16 + TORSO_ADD) / 16


D = Dims()
U = D.U


def u(n: float) -> float:
    return n * U


def uh(n: float) -> float:
    return n * U * HOOD_K


def th(h: float) -> float:
    """Web torso height (units above the pelvis: the robe rings) → z (the torso is stretched by ``ts``)."""
    return D.pz + u(h) * D.ts


# ── profiles ───────────────────────────────────────────────────────────────────────────────────────────
def hood_profile() -> Profile:
    """Web HOOD_RINGS (h, a front/back, b side, f forward) × HOOD_K, Catmull-Rom between them, closed by a soft
    peak tipped back (web top ring r 1.6 u, 0.8 u behind)."""
    hc = D.hc
    rings = [(-7.4, 5.2, 6.4, -0.6), (-6.2, 6.3, 6.75, -0.45), (-4.2, 7.0, 6.85, -0.2), (-1.4, 7.45, 6.95, -0.2),
             (1.5, 7.6, 6.9, -0.3), (4.4, 7.0, 6.45, -0.45), (6.8, 5.8, 5.6, -0.6), (8.1, 3.6, 3.5, -0.75),
             (8.8, 1.6, 1.6, -0.85), (9.25, 0.35, 0.35, -0.95)]
    return Profile([(hc + uh(h), uh(b), uh(a), uh(-f)) for h, a, b, f in rings], exp=2.0)


def robe_profile() -> Profile:
    """Robe torso (web robeRings: h above the pelvis) continued down over the hips under the sash."""
    rings = [(-3.2, 4.25, 5.85, 0.0), (0.0, 4.0, 5.7, 0.0), (2.5, 3.8, 5.4, 0.0), (6.0, 3.95, 5.6, 0.15),
             (9.0, 4.1, 5.9, 0.4), (12.0, 4.2, 6.1, 0.3), (14.0, 4.1, 6.2, 0.1), (15.6, 3.4, 5.2, 0.0),
             (16.6, 2.8, 4.0, 0.0)]
    return Profile([(th(h), u(b), u(a), -u(f)) for h, a, b, f in rings], exp=2.25)


def mantle_profile() -> Profile:
    """Web mantleRings (len + 1.4 → len − 5) + a rounded shoulder: a capelet over the shoulders."""
    L = 16.0
    rings = [(L - 5.2, 5.55, 7.85, 0.2), (L - 4.6, 5.5, 7.8, 0.2), (L - 2.6, 5.35, 7.6, 0.1), (L - 0.8, 4.95, 7.15, 0.0),
             (L + 0.4, 3.9, 5.4, 0.0), (L + 1.4, 3.05, 4.1, 0.0)]
    return Profile([(th(h), u(b), u(a), -u(f)) for h, a, b, f in rings], exp=2.2)


SKIRT_TOP_H = 1.6                 # web units above the pelvis (under the sash)
SKIRT_HEM_Z = 3.4                 # hem height above the ground (web: 3.4 u)
SKIRT_THICK = 0.008


def skirt_section(k: float) -> tuple[float, float, float, float]:
    """Skirt ring at ``k`` (0 waist → 1 hem): (z, side radius, front/back radius, forward offset y). Web
    skirtSection with the READY feet spread (9 u front/back: the hem a = 5.5 + 0.45 × spread)."""
    z0, z1 = th(SKIRT_TOP_H), u(SKIRT_HEM_Z)
    z = z0 + (z1 - z0) * k
    ks = [0.0, 0.16, 0.55, 1.0]
    rx = [5.75, 6.4, 7.0, 7.75]
    ry = [4.3, 5.35, 6.6, 8.2]
    return z, u(C._hermite(ks, rx, k)), u(C._hermite(ks, ry, k)), 0.0


def skirt_point(k: float, a: float, d: float = 0.0) -> Vector:
    """Skirt surface at ``k`` and angle ``a`` (deg: 0 front, +90 left) pushed ``d`` along the outward normal."""
    z, rx, ry, cy = skirt_section(k)
    e = 2.15
    t = math.radians(a - 90.0)
    c, s = math.cos(t), math.sin(t)
    p = Vector((rx * math.copysign(abs(c) ** (2.0 / e), c), cy + ry * math.copysign(abs(s) ** (2.0 / e), s), z))
    if d:
        e1 = 1e-3
        ta = skirt_point(k, a + 0.5) - skirt_point(k, a - 0.5)
        tk = skirt_point(min(1.0, k + e1), a) - skirt_point(max(0.0, k - e1), a)
        n = tk.cross(ta)
        if n.dot(Vector((p.x, p.y, 0.0))) < 0:
            n = -n
        p += n.normalized() * d
    return p


# ── rig spec ───────────────────────────────────────────────────────────────────────────────────────────
SASH_KNOT_A = -151.0              # web phi π − 0.5 (back, toward the near = right side)
SASH_H = 2.0                      # web: sash round the waist at h 2
TOME_A = -92.0                    # web phi 1.05 (right hip, front of the side; 3D: on the side, clear of the staff)


def sash_knot() -> Vector:
    return robe_profile().point(th(SASH_H), SASH_KNOT_A, u(0.75))


def sash_tail_points(which: int) -> list[Vector]:
    """Chain points of sash tail A (0) / B (1): from the knot down the back of the skirt, 13 u (web clothChain)."""
    kp = sash_knot()
    off = (-0.012, 0.018)[which]
    a = SASH_KNOT_A + (7.0 if which else -5.0)
    L = u(13.0) * (1.0, 0.86)[which]
    pts = []
    for i in range(4):
        f = i / 3
        z = kp.z - 0.014 - L * f
        k = (th(SKIRT_TOP_H) - z) / (th(SKIRT_TOP_H) - u(SKIRT_HEM_Z))
        s = skirt_point(max(0.0, min(1.0, k)), a, 0.024 + 0.006 * f)
        pts.append(Vector((s.x + off * (1 - f), s.y, z)))
    return pts


def tome_hook() -> Vector:
    return robe_profile().point(th(1.0), TOME_A, u(0.9))


def build_spec() -> HumanoidSpec:
    spec = D.spec
    chains = []
    for nm, a in (("skirt_f", 0.0), ("skirt_b", 180.0), ("skirt_l", 90.0), ("skirt_r", -90.0)):
        chains.append((nm, "pelvis", [skirt_point(k, a, 0.0) for k in (0.10, 0.52, 1.0)]))
    chains.append(("sash_a", "pelvis", sash_tail_points(0)))
    chains.append(("sash_b", "pelvis", sash_tail_points(1)))
    hc = D.hc
    chains.append(("hood_tip", "head", [Vector((0, uh(5.6), hc + uh(3.6))), Vector((0, uh(8.6), hc - uh(1.8))),
                                         Vector((0, uh(10.4), hc - uh(7.6)))]))
    chains.append(("beard", "head", [Vector((0, -uh(6.0), hc - uh(2.2))), Vector((0, -uh(6.6), hc - uh(6.4))),
                                      Vector((0, -uh(6.2), hc - uh(11.0)))]))
    hk = tome_hook()
    out = Vector((hk.x, hk.y, 0)).normalized()
    chains.append(("tome", "pelvis", [hk, hk + out * 0.012 + Vector((0, 0, -u(9.0)))]))
    # bell-sleeve sag bones (NEW chain, mage only): the flared half of each bell droops toward world down when the
    # arm is raised, instead of standing open toward the camera
    for side, sx in (("l", 1.0), ("r", -1.0)):
        E, W = _rest_elbow_wrist(spec, sx)
        dla = (W - E).normalized()
        L = (W - E).length
        chains.append((f"sleeve_{side}", f"lowerarm_{side}", [E + dla * 0.30 * L, W + dla * 0.02]))
    spec.chains = chains
    return spec


def _rest_elbow_wrist(spec: HumanoidSpec, sx: float) -> tuple[Vector, Vector]:
    """Rest elbow / wrist of the kit's A-pose (``kit.rig.build_humanoid``: arms ``arm_down`` below horizontal,
    elbow pre-bend forward)."""
    hip_z = spec.ankle + spec.shin + spec.thigh
    pz = hip_z + spec.pelvis_offset
    N = Vector((0, 0, pz + spec.torso))
    up3 = Vector((0, 0, 1))
    S = N - up3 * 0.10 * spec.torso + Vector((sx * spec.shoulder_half, 0, 0))
    a = math.radians(spec.arm_down)
    dA = Vector((sx * math.cos(a), 0, -math.sin(a)))
    hinge = dA.cross(Vector((0, -1, 0)))
    E = S + dA * spec.upper_arm
    dF = Matrix.Rotation(math.radians(spec.elbow_prebend), 3, hinge) @ dA
    return E, E + dF * spec.fore_arm


# LOD1 (spec §1.9 hero 7 k): every part decimated, sub-readability details left out
LOD1_RATIO = 0.5
LOD1_SKIP = ("rune", "robe_seam", "skirt_fold", "beard_line", "tome_corner", "tome_clasp", "boot_cuff",
             "hood_seam", "clasp_gem", "nose", "hood_floor", "tome_pages")


# ── body ───────────────────────────────────────────────────────────────────────────────────────────────
RIG: list = [None]


def build_body(pal: Palette, rig: R.Rig, lod: int = 0) -> bpy.types.Object:
    RIG[0] = rig
    if lod:
        b = M.Builder(ASSET, pal, prefix="mage", lod_ratio=LOD1_RATIO,
                      lod_skip=lambda n: n.rstrip("_lr").rstrip("0123456789").startswith(LOD1_SKIP))
    else:
        b = M.Builder(ASSET, pal, prefix="mage")
    b.regions(**REGIONS)
    add_hood(b)
    add_torso(b)
    add_skirt(b)
    add_sash(b)
    add_tome(b)
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


LAST_PARTS: list = []

# face opening (web: a dark oval h −6.1 … 5.5, ±1.05 rad round the hood front; trimmed in 3D so the hood keeps a
# cheek either side), in hood units about the head centre
OPEN_Z0, OPEN_Z1, OPEN_A = -5.9, 5.2, 52.0
OPEN_DEPTH = 0.046
EYE_H = 1.0                        # web eyes at h 1.1
BROW_H = 1.75                      # forehead in the hood's shadow above this (web brow stroke at h 2.6)


def open_edge(a: float) -> tuple[float, float] | None:
    """Vertical extent (hood units) of the face opening at angle ``a`` (deg), or None outside it."""
    x = a / OPEN_A
    if abs(x) >= 1.0:
        return None
    zc, hz = (OPEN_Z0 + OPEN_Z1) / 2, (OPEN_Z1 - OPEN_Z0) / 2
    w = math.sqrt(1 - x * x) ** 0.8
    return zc - hz * w, zc + hz * w


def face_r_at(z: float) -> float:
    """Front radius of the hood at height ``z`` (for placing the face in the opening)."""
    return hood_profile().at(z)[1]


def add_hood(b: M.Builder) -> None:
    hc = D.hc
    hp = hood_profile()

    def disp(z, a):
        e = open_edge(a)
        if e is None:
            return 0.0
        h = (z - hc) / (U * HOOD_K)
        return -OPEN_DEPTH if e[0] < h < e[1] else 0.0
    eps = 0.0012
    zs = [z for z in np.linspace(hp.z0, hp.z1, 24)]
    cols = [OPEN_A * k / 9.0 * 0.998 for k in range(-9, 10)]
    rows = []
    for i in range(1, 14):
        a = OPEN_A * (-1 + 2 * i / 14)
        e = open_edge(a)
        if e:
            rows += [hc + uh(e[0]) - eps, hc + uh(e[0]) + eps, hc + uh(e[1]) - eps, hc + uh(e[1]) + eps]
    zs = sorted(set([round(z, 5) for z in zs + rows if hp.z0 <= z <= hp.z1]))
    hood = profile_mesh(hp, zs, 40, disp=disp, cap0=None, cap1="fan", dome1=uh(0.25), extra_angles=cols)
    shell, recess = C.split_faces(hood, lambda bm, f: any(v[bm.verts.layers.int["af_disp"]] for v in f.verts))
    hull = profile_mesh(hp, list(np.linspace(hp.z0, hp.z1, 16)), 28, cap0="fan", cap1="fan", dome0=-0.03,
                        dome1=uh(0.25))
    hood_id = b.add(shell, "robe", Bind.rigid("head"), name="hood", hull=hull, hull_pad=True)
    b.add(recess, "hood_in", Bind.rigid("head"), name="hood_open", outline=False)
    # inner neck closure (the hood's lower rim is open; this hides the inside from the high camera)
    b.add(M.loft_z([(hp.z0 + 0.004, uh(5.0), uh(6.0), 0, uh(0.5)), (hp.z0 + 0.03, uh(4.0), uh(5.0), 0, uh(0.3))],
                   sides=24, cap0="fan", cap1="fan"), "hood_in", Bind.rigid("head"), name="hood_floor",
          outline=False, covered_by=hood_id)
    # gold rim round the opening (web: TRIM line 0.8 u), lying on the hood surface just outside the edge
    ring, ups = [], []
    n = 64
    zc, hz = (OPEN_Z0 + OPEN_Z1) / 2, (OPEN_Z1 - OPEN_Z0) / 2
    for i in range(n):
        t = 2 * math.pi * i / n
        x = math.sin(t)
        a = OPEN_A * 1.035 * x
        h = zc + hz * 1.03 * math.copysign((1 - x * x) ** 0.4, math.cos(t))
        z = hc + uh(h)
        ring.append(hp.point(z, a, 0.0018))
        ups.append(hp.normal(z, a))
    rim = M.sweep(ring, M.superellipse(1, 1, 6, 2.0), scales=[(0.0026, 0.0092)] * n, closed_path=True,
                  cap0=None, cap1=None, up=ups)
    b.add(rim, "trim", Bind.rigid("head"), name="hood_rim", outline=False, covered_by=hood_id)
    # back seam (web ROBE.shade stroke from the peak down the back)
    b.add(painted_strip(hp, 180.0, hc - uh(6.4), hc + uh(8.4), 0.011, lambda z, a: 0.0016, steps=14),
          "robe_seam", Bind.rigid("head"), name="hood_seam", outline=False, covered_by=hood_id)
    add_face(b, hood_id)
    add_beard(b)
    # hood tip: the mantle-coloured liripipe hanging down the back (web hoodTip: from h 3 at the back to 11 u
    # behind at h −7.6)
    tip_pts = HB.catmull([Vector((0, uh(5.2), hc + uh(4.4))), Vector((0, uh(7.3), hc + uh(0.9))),
                          Vector((0, uh(9.0), hc - uh(3.3))), Vector((0, uh(10.3), hc - uh(7.0))),
                          Vector((0, uh(10.6), hc - uh(8.4)))], 10)
    radii = [(uh(2.4) * (1 - f) ** 0.75 + 0.006, uh(2.9) * (1 - f) ** 0.6 + 0.005)
             for f in [i / 10 for i in range(11)]]
    tip = M.sweep(tip_pts, M.superellipse(1, 1, 10, 2.0), scales=radii, up=(0, 1, 0), dome0=0.0, dome1=0.006)
    b.add(tip, "mantle", Bind.blend("head", "hood_tip_01", "hood_tip_02", falloff=3.0), name="hood_tip",
          hull_pad=True)


def add_face(b: M.Builder, hood_id: int) -> None:
    """Face in the opening: cheeks / nose catch the light, the forehead in the hood's shadow, glowing eyes."""
    hc = D.hc
    fz = hc + uh(-0.3)
    front = face_r_at(fz)
    depth = 0.052
    cy = -(front - 0.013) + depth                      # face front 1.3 cm behind the hood surface
    face = ell((uh(4.3), depth, uh(4.4)), (0, cy, fz), sides=20, rings=12, exp=2.1)
    zb = hc + uh(BROW_H)
    lit, shade = C.split_faces(face, lambda bm, f: f.calc_center_median().z > zb)
    bind = Bind.rigid("head")
    fid = b.add(lit, "skin", bind, name="face", outline=False, covered_by=hood_id)
    b.add(shade, "skin_shade", bind, name="face_shade", outline=False, covered_by=hood_id)
    # nose: a blunt wedge under the eyes catching the key light
    nz = hc + uh(-0.9)
    fr = face_r_at(nz) - 0.013
    nose = ell((0.015, 0.018, 0.024), (0, -fr - 0.004, nz), sides=10, rings=6)
    b.add(nose, "skin", bind, name="nose", outline=False, covered_by=hood_id)
    # glowing eyes: slanted almond slits in the brow shadow (web 1.6 × 0.9 u at phi ±0.36 about the face)
    ez = hc + uh(EYE_H)
    fr = face_r_at(ez) - 0.013
    for sx in (1.0, -1.0):
        ex = sx * uh(2.15)
        yy = -fr + depth * (1 - math.sqrt(max(0.0, 1 - (ex / uh(4.3)) ** 2))) - 0.0035
        eye = ell((0.026, 0.0065, 0.0125), (0, 0, 0), sides=12, rings=6)
        eye.rotate("y", -sx * 10.0)
        eye.rotate("z", sx * 18.0)
        b.add(eye.translate((ex, yy, ez)), "eye", bind, name="eye", outline=False, covered_by=hood_id)


def add_beard(b: M.Builder) -> None:
    """Pointed silver beard: a lens-section loft from the cheeks over the hood's lower rim down the chest to the
    tip (web beard blob: 7 u wide under the cheeks, tip 10.6 u below the head centre), three strand grooves."""
    hc = D.hc
    spine = HB.catmull([Vector((0, -face_r_at(hc - uh(1.9)) + 0.026, hc - uh(1.9))),
                        Vector((0, -uh(7.25), hc - uh(4.4))), Vector((0, -uh(7.2), hc - uh(7.4))),
                        Vector((0, -uh(6.4), hc - uh(11.0)))], 14)
    n = len(spine)
    widths, depths = [], []
    for i in range(n):
        f = i / (n - 1)
        widths.append(uh(3.55) * (1 - f) ** 0.55 * (1 + 0.10 * math.sin(math.pi * min(1.0, f * 2.2))) + 0.004)
        depths.append(0.022 + 0.014 * math.sin(math.pi * min(1.0, 0.25 + f * 0.9)) * (1 - f) ** 0.3)
    prof = M.superellipse(1, 1, 16, 2.2)
    beard = M.sweep(spine, prof, scales=[(d, w) for d, w in zip(depths, widths)], up=(0, -1, 0), dome0=0.004,
                    dome1=0.003)
    bind = Bind.blend("head", "beard_01", "beard_02", falloff=3.0)
    bid = b.add(beard, "beard", bind, name="beard", hull_pad=True)
    for sx in (-0.45, 0.0, 0.45):
        pts, rad = [], []
        for i in range(2, n - 3):
            p = spine[i]
            w = widths[i]
            f = i / (n - 1)
            x = sx * w * (1 - 0.35 * f)
            dz = math.sqrt(max(0.0, 1 - (x / max(w, 1e-4)) ** 2)) ** (2 / 2.2)
            pts.append(p + Vector((x, -depths[i] * dz * 0.97, 0)))
            rad.append(0.0024 * (1 - f) + 0.0012)
        b.add(M.tube(pts, rad, sides=4, dome0=0.0, dome1=0.0), "beard_line", bind, name="beard_line",
              outline=False, covered_by=bid)


def add_torso(b: M.Builder) -> None:
    spine = ("pelvis", "spine_01", "spine_02", "spine_03")
    rp = robe_profile()
    zs = list(np.linspace(rp.z0, rp.z1, 16))
    torso_id = b.add(profile_mesh(rp, zs, 30, cap0="fan", cap1="fan", dome1=0.01), "robe",
                     Bind.blend(*spine, smooth=0.03), name="robe",
                     hull=profile_mesh(rp, list(np.linspace(rp.z0, rp.z1, 9)), 22, cap0="fan", cap1="fan",
                                       dome1=0.01), hull_pad=True)
    # front closure: gold edge down the chest at the web's phi 0.3 (toward the right), from the mantle to the sash
    b.add(painted_strip(rp, -17.0, th(2.6), th(11.6), 0.013, lambda z, a: 0.002, steps=10), "trim",
          Bind.blend(*spine, smooth=0.03), name="closure", outline=False, covered_by=torso_id)
    # mantle over the shoulders (web mantleRings), gold edge, clasp + violet gem
    mp = mantle_profile()
    mzs = list(np.linspace(mp.z0, mp.z1, 11))
    mbind = HB.blend4(RIG[0], "spine_02", "spine_03", "clavicle_l", "clavicle_r", "neck_01", falloff=5.0, smooth=0.02)
    mantle_id = b.add(profile_mesh(mp, mzs, 36, cap0="fan", cap1="fan", dome0=-0.03), "mantle", mbind, name="mantle",
                      hull=profile_mesh(mp, list(np.linspace(mp.z0, mp.z1, 7)), 24, cap0="fan", cap1="fan",
                                        dome0=-0.03), hull_pad=True)
    b.add(C.surface_band(mp, mp.z0 + u(0.62), u(0.85), 0.0028, segs=48, out0=-0.002), "trim", mbind,
          name="mantle_trim", outline=False, covered_by=mantle_id)
    cz = th(16.0 - 2.6)
    cpt = mp.point(cz, -17.0, 0.0)
    cn = mp.normal(cz, -17.0)
    fr = R.frame(cpt, Vector((0, 0, 1)), cn)
    clasp = M.lathe([(u(1.35), -0.003), (u(1.35), 0.0035), (u(1.05), 0.007), (u(0.55), 0.0085)], sides=16)
    b.add(clasp.transform(fr @ Matrix.Rotation(math.radians(-90), 4, "X")), "trim", mbind,
          name="clasp", outline=False, covered_by=mantle_id)
    gem = M.gem(u(0.62), 0.007, 0.002, sides=6)
    b.add(gem.transform(fr @ Matrix.Rotation(math.radians(-90), 4, "X") @ Matrix.Translation((0, 0, 0.0075))),
          "gem", mbind, name="clasp_gem", outline=False, covered_by=mantle_id)


def add_skirt(b: M.Builder) -> None:
    rows = [i / 16 for i in range(17)]
    shell, hull = HB.shell_ring(lambda k, uu, d: skirt_point(k, uu * 360.0 - 180.0, d), rows, 44, SKIRT_THICK,
                                lens=0.002 / 0.75)
    bind = Bind.custom(SKIRT_BONES, skirt_weights)
    sid = b.add(shell, "robe", bind, name="skirt", sub_regions={1: "robe", 2: "mantle"}, hull=hull, hull_pad=True)
    t = SKIRT_THICK / 2
    # front opening (web: lining panel between phi 0.12 and 0.55 → toward the right, gold edges, 3 rune diamonds)
    a0, a1 = -33.0, -7.0
    ks = [0.02 + 0.98 * i / 14 for i in range(15)]
    panel = [[skirt_point(k, a0 + (a1 - a0) * j / 6, t + 0.0012) for j in range(7)] for k in ks]
    b.add(HB.outward(HB.sheet_grid(panel), lambda c: Vector((0, 0, c.z))), "lining", bind, name="lining",
          outline=False, covered_by=sid)
    for a in (a0, a1):
        edge = [[skirt_point(k, a + da, t + 0.0019) for da in (-1.2, 0.0, 1.2)] for k in ks]
        b.add(HB.outward(HB.sheet_grid(edge), lambda c: Vector((0, 0, c.z))), "trim", bind, name="skirt_edge",
              outline=False, covered_by=sid)
    am = (a0 + a1) / 2
    for i in range(3):
        k = 0.26 + i * 0.24
        c = skirt_point(k, am, t + 0.0024)
        n = (skirt_point(k, am, t + 0.01) - c).normalized()
        fr = R.frame(c, Vector((0, 0, 1)), n)
        s = 1.0 + 0.12 * i
        dia = C.plate2d([(0.0, -u(1.3) * s), (u(0.85) * s, 0.0), (0.0, u(1.3) * s), (-u(0.85) * s, 0.0)], 0.0016)
        b.add(dia.transform(fr), "rune", bind, name="rune", outline=False, covered_by=sid)
    # gold hem (web: TRIM line round the hem, 0.9 u)
    hk = [0.95, 0.975, 1.0]
    o2 = [[skirt_point(k, -180.0 + 360.0 * c / 44, t + 0.0016) for c in range(44)] for k in hk]
    i2 = [[skirt_point(k, -180.0 + 360.0 * c / 44, -t - 0.0016) for c in range(44)] for k in hk]
    b.add(C.thick_patch(o2, i2, wrap_u=True), "trim", bind, name="hem", outline=False, covered_by=sid)
    # back seam + two folds (web ROBE.shade strokes)
    for a, k0, nm in ((180.0, 0.08, "robe_seam"), (180.0 - 46.0, 0.36, "skirt_fold"),
                      (180.0 + 40.0, 0.36, "skirt_fold"), (78.0, 0.42, "skirt_fold"), (-108.0, 0.42, "skirt_fold")):
        strip = [[skirt_point(k, a + da, t + 0.0012) for da in (-0.9, 0.0, 0.9)]
                 for k in [k0 + (0.93 - k0) * i / 10 for i in range(11)]]
        b.add(HB.outward(HB.sheet_grid(strip), lambda c: Vector((0, 0, c.z))), "robe_seam", bind, name=nm,
              outline=False, covered_by=sid)


SKIRT_BONES = ["pelvis", "thigh_l", "thigh_r"] + [f"skirt_{c}_{i:02d}" for c in "fblr" for i in (1, 2)]
SKIRT_ANG = {"f": 0.0, "l": 90.0, "b": 180.0, "r": -90.0}


def skirt_weights(co: np.ndarray) -> np.ndarray:
    """Skirt skin weights from its surface parameters: round the skirt the four chains blend by angle (smoothstep
    between neighbours, so the cone bends as one sheet), down it bone → bone between bone centres; the waist band
    (top 13 %) stays on the pelvis, with a touch of each thigh on its own side near the hip."""
    z0, z1 = th(SKIRT_TOP_H), u(SKIRT_HEM_Z)
    k = np.clip((z0 - co[:, 2]) / (z0 - z1), 0.0, 1.0)
    a = np.degrees(np.arctan2(co[:, 0], -co[:, 1]))          # 0 front, +90 left
    yoke = 1.0 - HB.smooth01(k / 0.13)
    W = np.zeros((len(co), len(SKIRT_BONES)))
    thigh = 0.18 * np.exp(-((k - 0.10) / 0.08) ** 2)
    W[:, 0] = yoke * (1 - thigh)
    W[:, 1] = yoke * thigh * (co[:, 0] > 0)
    W[:, 2] = yoke * thigh * (co[:, 0] <= 0)
    lat = {}
    for c, ac in SKIRT_ANG.items():
        d = np.abs((a - ac + 180.0) % 360.0 - 180.0)
        lat[c] = HB.smooth01(1.0 - d / 90.0)
    tot = sum(lat.values())
    q = np.clip(k * 2 - 0.5, 0.0, 1.0)                        # bone-centre coordinate (2 bones)
    for c in "fblr":
        wc = (1.0 - yoke) * lat[c] / np.maximum(tot, 1e-6)
        W[:, SKIRT_BONES.index(f"skirt_{c}_01")] += wc * (1.0 - q)
        W[:, SKIRT_BONES.index(f"skirt_{c}_02")] += wc * q
    return HB.top4(W)


def add_sash(b: M.Builder) -> None:
    rp = robe_profile()
    z = th(SASH_H)
    spine = ("pelvis", "spine_01")
    sash_id = b.add(C.surface_band(rp, z, u(2.6), u(0.6), segs=48, out0=-0.002), "sash",
                    Bind.blend(*spine, smooth=0.04), name="sash", hull_pad=True)
    # knot at the back (right of the spine) and the two tails (web clothChain 13 u, widths 1.2 → 1.8 u)
    kp = sash_knot()
    n = rp.normal(z, SASH_KNOT_A).normalized()
    knot = ell((u(1.6), u(1.05), u(1.5)), kp + n * u(0.3), sides=12, rings=8)
    b.add(knot, "sash", Bind.rigid("pelvis"), name="sash_knot", hull_pad=True)
    for which, reg in ((0, "sash"), (1, "sash_b")):
        pts = sash_tail_points(which)
        path = HB.catmull(pts, 9)
        nm = ("sash_a", "sash_b")[which]
        bones = [f"{nm}_01", f"{nm}_02", f"{nm}_03"]
        outer, inner = [], []
        hz = len(path)
        for i, p in enumerate(path):
            f = i / (hz - 1)
            w = u(1.25 + 0.6 * f)
            out_n = Vector((p.x, p.y, 0)).normalized()
            tang = out_n.cross(Vector((0, 0, 1))).normalized()
            row_o, row_i = [], []
            for s in (-1.0, -0.4, 0.4, 1.0):
                q = p + tang * (s * w) + out_n * (-0.004 * (1 - s * s))
                if f > 0.97:                                 # forked (swallowtail) end
                    q.z += 0.02 * (1 - abs(s))
                row_o.append(q + out_n * 0.0032)
                row_i.append(q - out_n * 0.0032)
            outer.append(row_o)
            inner.append(row_i)
        tail = C.thick_patch(outer, inner, sub_tags=True)

        def wfn(co, pts=pts):
            k = np.clip((pts[0].z - co[:, 2]) / (pts[0].z - pts[-1].z), 0.0, 1.0)
            yoke = 1.0 - HB.smooth01(k / 0.08)
            return HB.top4(HB.chain_weights(k, 3, yoke))
        b.add(tail, reg, Bind.custom(["pelvis"] + bones, wfn), name=f"sash_tail{which}",
              sub_regions={1: reg, 2: "sash_b"}, hull_pad=True)


def add_tome(b: M.Builder) -> None:
    """Spell tome on a cord at the right hip (web viewTome: 4.8 × 7 u, gold corners and clasp, pages edge)."""
    hk = tome_hook()
    out = Vector((hk.x, hk.y, 0)).normalized()
    side = out.cross(Vector((0, 0, 1))).normalized()
    rbind = Bind.rigid("tome_01")
    cbind = Bind.blend("pelvis", "tome_01", falloff=6.0)
    cord = HB.curve_tube([hk + out * 0.004 + Vector((0, 0, 0.004)), hk + out * 0.010 + Vector((0, 0, -u(1.4))),
                          hk + out * 0.014 + Vector((0, 0, -u(2.7)))], [0.0042, 0.0042, 0.0042], sides=6)
    b.add(cord, "trim", cbind, name="tome_cord")
    c = hk + out * 0.030 + Vector((0, 0, -u(2.6) - u(3.5)))
    w, h, d = u(2.4), u(3.5), u(0.85)
    fr = Matrix.Identity(4)
    for i in range(3):
        fr[i][0], fr[i][1], fr[i][2], fr[i][3] = side[i], out[i], (0, 0, 1)[i], c[i]
    cover = M.box((2 * w, 2 * d, 2 * h), (0, 0, 0), bevel=0.004)
    tid = b.add(cover.transform(fr), "tome", rbind, name="tome", hull_pad=True)
    # page block: inside the covers, showing only along the fore-edge (+x) where it stands 2.5 mm out
    pages = M.box((2 * w - 0.004, 2 * d - 0.007, 2 * h - 0.012), (0.0045, 0.0, 0.0), bevel=0.001)
    b.add(pages.transform(fr), "pages", rbind, name="tome_pages", outline=False, covered_by=tid)
    for sx in (-1, 1):
        for sz in (-1, 1):
            cor = M.box((u(1.3), 2 * d + 0.0035, u(1.3)), (sx * (w - u(0.6)), 0, sz * (h - u(0.6))), bevel=0.0015)
            b.add(cor.transform(fr), "trim", rbind, name="tome_corner", outline=False, covered_by=tid)
    clasp = M.cylinder(u(1.1), u(1.1), -0.002, 0.0035, sides=14).rotate("x", -90).translate((0, d + 0.0005, 0))
    b.add(clasp.transform(fr), "trim", rbind, name="tome_clasp", outline=False, covered_by=tid)


def add_arm(b: M.Builder, rig: R.Rig, side: str) -> None:
    J = rig.joints
    S, E, W = J[f"shoulder_{side}"], J[f"elbow_{side}"], J[f"wrist_{side}"]
    ua, la = f"upperarm_{side}", f"lowerarm_{side}"
    cl = f"clavicle_{side}"
    dua = (E - S).normalized()
    dla = (W - E).normalized()
    b.add(M.tube([S - dua * 0.035, E + dua * 0.012], [u(3.0), u(2.65)], sides=16), "robe",
          Bind.blend(cl, ua, la, falloff=5.0), name=f"sleeve_{side}")
    # bell sleeve: from the elbow flaring to 4.2 u at 86 % of the forearm (web sleeveArm), gold cuff, dark inside
    L = (W - E).length
    sx = 1.0 if side == "l" else -1.0

    def bell(k: float, uu: float, d: float) -> Vector:
        z = -0.025 + k * (L * 0.84 + 0.025)
        r = u(2.65) + (u(3.75) - u(2.65)) * (k ** 1.5)
        t = 2 * math.pi * uu
        # the cuff hangs lower on the under side (frame +X = down at rest; web: the bell's lower corner drops 1.2–1.6 u)
        drop = 0.030 * k ** 2 * max(0.0, math.cos(t)) ** 1.5
        p = Vector((r * math.cos(t), r * math.sin(t), z + drop))
        n = Vector((math.cos(t), math.sin(t), -0.42 * k)).normalized()
        return p + n * d
    fr = HB.axis_frame_x(E, dla, Vector((0, 0, -1)))
    rows = [i / 10 for i in range(11)]
    shell, hull = HB.shell_ring(lambda k, uu, d: fr @ bell(k, uu, d), rows, 24, 0.007, lens=0.002 / (L * 0.84))
    sb = f"sleeve_{side}_01"

    def bell_w(co, E=E, dla=dla, L=L):
        # along the forearm: the bell's root rides the forearm, its flared end the sag bone (``sleeve_*_01``)
        x = (np.asarray(co) - np.array(E)) @ np.array(dla) / L
        f = HB.smooth01((x - 0.18) / 0.42)
        W = np.zeros((len(co), 3))
        W[:, 0] = np.clip(0.25 - x, 0.0, 0.25) * (1 - f)          # a touch of the upper arm at the elbow
        W[:, 1] = (1 - f) - W[:, 0]
        W[:, 2] = f
        return HB.top4(W)
    bbind = Bind.custom([ua, la, sb], bell_w)
    bid = b.add(shell, "robe", bbind, name=f"bell_{side}", sub_regions={1: "robe", 2: "mantle"}, hull=hull,
                hull_pad=True)
    # the under-sleeve along the forearm (dark lining tone): what shows when the bell sags down a raised arm
    b.add(M.tube([E + dla * 0.02, W - dla * 0.02], [u(1.45), u(1.2)], sides=12, dome0=0.0, dome1=0.0), "mantle",
          Bind.blend(la, f"hand_{side}", falloff=8.0), name=f"forearm_{side}")
    cuff_o = [[fr @ bell(k, c / 28, 0.0042) for c in range(28)] for k in (0.90, 0.95, 1.0)]
    cuff_i = [[fr @ bell(k, c / 28, -0.0042) for c in range(28)] for k in (0.90, 0.95, 1.0)]
    b.add(C.thick_patch(cuff_o, cuff_i, wrap_u=True), "trim", bbind, name=f"cuff_band_{side}",
          outline=False, covered_by=bid)
    add_hand(b, rig, side)


def add_hand(b: M.Builder, rig: R.Rig, side: str) -> None:
    """Right: a fist closed round the staff grip; left: an open mitten (spell gestures). Skin #E6B791."""
    J = rig.joints
    W, T, G = J[f"wrist_{side}"], J[f"handtip_{side}"], J[f"grip_{side}"]
    hd = (T - W).normalized()
    hbn = f"hand_{side}"
    wrist = M.tube([W - hd * 0.03, W + hd * 0.016], [0.031, 0.034], sides=12)
    b.add(wrist, "skin", Bind.blend(f"lowerarm_{side}", hbn, falloff=6.0), name=f"wrist_{side}")
    if side == "r":
        grip = Vector((0, -1, 0))
        zx = (grip - hd * grip.dot(hd)).normalized()
        xx = hd.cross(zx)
        fr = Matrix.Identity(4)
        for i in range(3):
            fr[i][0], fr[i][1], fr[i][2], fr[i][3] = xx[i], hd[i], zx[i], G[i]
        fist = ell((0.043, 0.050, 0.046), (0, 0.006, 0), sides=14, rings=8, exp=2.5, exp_v=2.3)
        fid = b.add(fist.transform(fr), "skin", Bind.rigid(hbn), name=f"hand_{side}", hull_pad=True)
        thumb = M.capsule((0.028, -0.012, 0.024), (0.014, 0.026, 0.038), 0.015, 0.012, sides=8)
        b.add(thumb.transform(fr), "skin", Bind.rigid(hbn), name=f"thumb_{side}", outline=False, covered_by=fid)
    else:
        mit = M.mitten(0.112, 0.080, 0.040, side="l", curl_deg=16.0)
        fr = R.frame(W - hd * 0.012, hd, Vector((0, -1, 0)))
        b.add(mit.transform(fr), "skin", Bind.rigid(hbn), name=f"hand_{side}", hull_pad=True)


def add_leg(b: M.Builder, rig: R.Rig, side: str) -> None:
    J = rig.joints
    Hp, Kn, A = J[f"hip_{side}"], J[f"knee_{side}"], J[f"ankle_{side}"]
    th_, ca, ft, bl = f"thigh_{side}", f"calf_{side}", f"foot_{side}", f"ball_{side}"
    # leggings: the thigh is hidden by the robe; it starts below the hip joint so a flexed thigh never pierces the
    # skirt's pinned waist
    top = Hp + Vector((0, 0, -0.05))
    b.add(M.tube([top, Kn, A + Vector((0, 0, 0.06))], [0.054, 0.050, 0.043], sides=14), "leggings",
          Bind.blend("pelvis", th_, ca, ft, falloff=5.0), name=f"leg_{side}")
    boot = M.boot(D.spec.foot_len, 0.100, 0.090, shaft=0.07, toe_up=0.028)
    boot_id = b.add(boot.translate((A.x, 0, 0)), "boot", Bind.blend(ca, ft, bl, falloff=5.0), name=f"boot_{side}",
                    hull_pad=True)
    cuff = M.loft_z([(0.125, 0.051, 0.049), (0.150, 0.057, 0.055), (0.160, 0.055, 0.053)], sides=14, cap0=None,
                    cap1="fan")
    b.add(cuff.translate((A.x, 0.004, 0)), "boot", Bind.blend(ca, ft, falloff=5.0), name=f"boot_cuff_{side}")


# ── staff + crystal ────────────────────────────────────────────────────────────────────────────────────
STAFF_UP, STAFF_DOWN = 22.0, 17.0           # web units above / below the grip
CRYSTAL_AT = STAFF_UP + 3.4                 # crystal centre above the grip (web)


def staff_axis(y: float) -> Vector:
    """Gnarled shaft centre line (staff space: +Y up the staff, X across, Z the staff's face): a gentle S bend
    (web quadratic shaft) with a slight twist out of plane."""
    yu = y / U
    f = (yu + STAFF_DOWN) / (STAFF_UP + STAFF_DOWN)
    x = -u(0.35) * math.sin(math.pi * f) + u(0.18) * math.sin(2.3 * math.pi * f)
    z = u(0.14) * math.sin(1.6 * math.pi * f)
    return Vector((x, y, z))


def build_staff(pal: Palette) -> tuple[bpy.types.Object, list]:
    """Gnarled staff in weapon-bone space (grip at the origin, +Y = up the staff toward the crystal; web: 22 u up,
    17 u down, gold bands at 19 / 4.5 / −15.6 u, a two-pronged claw head cradling the crystal)."""
    b = M.Builder(STAFF, pal, prefix="mage_staff")
    b.regions(**STAFF_REGIONS)
    y0, y1 = -u(STAFF_DOWN), u(STAFF_UP - 2.6)
    n = 30
    ys = [y0 + (y1 - y0) * i / n for i in range(n + 1)]
    knots = (-11.5, -3.0, 7.0, 13.5)

    def radius(y):
        yu = y / U
        r = u(0.98) - u(0.14) * (yu + STAFF_DOWN) / (STAFF_UP + STAFF_DOWN)
        for kz in knots:
            r += u(0.34) * math.exp(-((yu - kz) / 0.9) ** 2)
        return r
    pts = [staff_axis(y) for y in ys]
    rad = [(radius(y), radius(y) * 0.92) for y in ys]
    shaft = M.sweep(pts, M.superellipse(1, 1, 10, 2.0, phase=0.3), scales=rad, up=(0, 0, 1), dome0=u(0.35),
                    dome1=0.0, twist_deg=40.0)
    sid = b.add(shaft, "wood", name="shaft", hull_pad=True)
    for ph in (0.0, math.pi):
        gp, gr = [], []
        for i in range(2, n - 1):
            y = ys[i]
            a = ph + 2.2 * (y - y0) / (y1 - y0) * math.pi
            r = radius(y) * 0.97
            gp.append(staff_axis(y) + Vector((math.cos(a) * r, 0.0, math.sin(a) * r)))
            gr.append(0.0028)
        b.add(M.tube(gp, gr, sides=4, dome0=0.0, dome1=0.0), "wood_dark", name="bark", outline=False,
              covered_by=sid)
    # claw head: two prongs from the top bulb, bowing out round the crystal and hooking in over it (web claw)
    top = staff_axis(y1)
    bulb = ell((u(1.3), u(1.6), u(1.25)), top + Vector((0, u(0.4), 0)), sides=12, rings=8)
    b.add(bulb, "wood", name="claw_bulb", hull_pad=True)
    for sx in (1.0, -1.0):
        ctl = [top + Vector((sx * u(0.4), u(0.6), 0)), top + Vector((sx * u(3.3), u(3.4), u(0.25))),
               top + Vector((sx * u(4.5), u(6.5), u(0.3))), top + Vector((sx * u(3.6), u(9.4), u(0.1))),
               top + Vector((sx * u(1.9), u(10.9), -u(0.1)))]
        path = HB.catmull(ctl, 12)
        rr = [u(1.0) * (1 - 0.70 * (i / 12) ** 1.2) for i in range(13)]
        prong = M.sweep(path, M.superellipse(1, 1, 8, 2.0), scales=[(r, r * 0.85) for r in rr], up=(0, 0, 1),
                        dome0=0.0, dome1=u(0.3))
        b.add(prong, "wood", name="prong", hull_pad=True)
    for yy, h, r in ((y1 - u(0.2), u(1.1), 1.30), (u(4.5), u(0.9), 1.28), (-u(15.6), u(1.0), 1.26),
                     (y0 + u(0.45), u(0.7), 1.18)):
        c = staff_axis(yy)
        rb = radius(yy) * r
        band = M.tube([c - Vector((0, h / 2, 0)), c + Vector((0, h / 2, 0))], [rb, rb], sides=12, dome0=0.0, dome1=0.0)
        b.add(band, "trim", name="staff_band", outline=False, covered_by=sid)
    obj = asset.finish_mesh(b, pal, "weapon")
    obj.name = STAFF
    cy = u(CRYSTAL_AT)
    # sockets (spec §2.4 / §3.7): crystal (the SM_Hero_Mage_Crystal attach point), tip (= the crystal: the web trail
    # sweeps the crystal path), mid (10 u up the staff: the web trail base), foot (staff butt, ground taps)
    return obj, [{"name": "crystal", "pos": (0, cy, 0)}, {"name": "tip", "pos": (0, cy, 0)},
                 {"name": "mid", "pos": (0, u(10.0), 0)}, {"name": "foot", "pos": (0, y0, 0)}]


def build_crystal(pal: Palette) -> bpy.types.Object:
    """Floating arcane crystal (web diamond 4.4 u up / 3.6 u down / 2.3 u wide) in its own space (centre at the
    origin, +Y up the staff): a faceted hexagonal bipyramid with a girdle and a white-hot highlight sliver."""
    b = M.Builder(CRYSTAL, pal, prefix="mage_crystal")
    b.regions(**CRYSTAL_REGIONS)
    up, dn, r = u(4.4), u(3.6), u(2.3)
    bm = bmesh.new()
    n = 6
    ring_hi = [bm.verts.new((r * math.cos(2 * math.pi * i / n), u(0.35), r * math.sin(2 * math.pi * i / n)))
               for i in range(n)]
    ring_lo = [bm.verts.new((r * 0.96 * math.cos(2 * math.pi * (i + 0.5) / n), -u(0.25),
                             r * 0.96 * math.sin(2 * math.pi * (i + 0.5) / n))) for i in range(n)]
    top = bm.verts.new((0, up, 0))
    bot = bm.verts.new((0, -dn, 0))
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((ring_hi[j], ring_hi[i], top))
        bm.faces.new((ring_lo[i], ring_lo[j], bot))
        bm.faces.new((ring_hi[i], ring_hi[j], ring_lo[i]))
        bm.faces.new((ring_lo[i], ring_hi[j], ring_lo[j]))
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    cid = b.add(M.Part(bm).flat(), "crystal", name="crystal", hull_pad=True)
    # highlight sliver on the upper-left facet (web white sliver)
    a = math.radians(150.0)
    p0 = Vector((r * 0.45 * math.cos(a), u(2.2), r * 0.45 * math.sin(a)))
    sl = ell((u(0.28), u(1.25), u(0.18)), (0, 0, 0), sides=6, rings=4)
    sl.rotate("z", 22.0)
    n_out = Vector((math.cos(a), 0.42, math.sin(a))).normalized()
    sl.transform(Matrix.Translation(p0 + n_out * u(0.12)) @ HB.rot_to(Vector((0, 0, 1)), n_out))
    b.add(sl, "crystal_hot", name="crystal_hot", outline=False, covered_by=cid)
    obj = asset.finish_mesh(b, pal, "weapon")
    obj.name = CRYSTAL
    return obj


# ── assembly ───────────────────────────────────────────────────────────────────────────────────────────
def crystal_local() -> Matrix:
    """Crystal pose on ``weapon_r`` (= the staff's ``crystal`` socket): the bone frame + CRYSTAL_AT along +Y."""
    return Matrix.Translation((0, u(CRYSTAL_AT), 0))


def eyes_point() -> Vector:
    return Vector((0, -(face_r_at(D.hc + uh(EYE_H)) - 0.013) - 0.004, D.hc + uh(EYE_H)))


def build_all(lod1: bool = False):
    scene.reset()
    pal = Palette("Heroes")
    spec = build_spec()
    rig = R.build_humanoid(spec)
    R.add_socket(rig, "eyes", "head", eyes_point())
    body = build_body(pal, rig)
    staff, sockets = build_staff(pal)
    crystal = build_crystal(pal)
    out = dict(pal=pal, rig=rig, body=body, items={STAFF: staff, CRYSTAL: crystal}, sockets={STAFF: sockets})
    if lod1:
        out["body1"] = build_body(pal, rig, lod=1)
    return out


def attach_items(rig, items) -> None:
    R.attach_to_bone(items[STAFF], rig, "weapon_r")
    HB.attach_at(items[CRYSTAL], rig, "weapon_r", crystal_local())


def part_report(parts) -> str:
    agg: dict = {}
    for p in parts:
        k = p["name"].rstrip("_lr").rstrip("0123456789")
        agg[k] = agg.get(k, 0) + p["tris"]
    return ", ".join(f"{k} {v}" for k, v in sorted(agg.items(), key=lambda kv: -kv[1]))


SCRATCH = Path("/tmp/claude-0/-home-user-abyssfire/b3225f99-25a3-5b78-996f-643be8685889/scratchpad/heroes2/mage")


def hero_def():
    import mage_ship
    return mage_ship.HERO


if __name__ == "__main__":
    import hero_ship
    sys.exit(hero_ship.cli(hero_def(), sys.argv, SCRATCH))

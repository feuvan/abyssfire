"""焚旗元帅·瓦德拉斯 Lord-Marshal Valdras — ``SK_Mon_Valdras``, the Chapter-1 act boss (dark remake monsters.md §2.6,
chapter1.md §11; implementation-plan.md A7).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/valdras.py
        (no flag) ship · --quick [dir] · --anims [Clip …] [--facing=se] · --portrait · --no-previews · --no-verify

A new mesh on ``SKEL_Human``: the crusade's Lord-Marshal, a head and a half taller than the hero (2.10 m to the bare
crown, 2.13 m to the helm; ×1.4 the hero's bulk: a 0.82 m pauldron span) in **blackened plate** (``valdras.plate``
#2E2F33, edges #4A4C52) whose seams leak pale **breach-violet** (``valdras.seam`` #B02FD0 round ``valdras.seam_core``
#F0E0FF cores — the seized fire inside him; never ember): the gorget under the helm, the waist, the pauldron lames,
the couters and poleyns, three cracks in the breastplate and the rupture in the backplate. A **great helm** with a
burned plume (a ragged charred crest) and a **single glowing visor slit** (``valdras.marker`` #E8D8FF, the family
marker; the runtime ``visorGlow`` card on the ``visor`` socket); no face, no crown, no skull, no throne.

**The crusade's scorched standard has grown through his spine**: a charred pole (``valdras.pole``) bursts out of the
backplate between the shoulder blades (a torn rupture leaking violet, roots of the dark wood spread over the plate),
rises 0.35 m above the helm and carries a crossbar, a blackened open-hand finial and the banner (``valdras.standard``
#5E1012 charred to ``valdras.standard_char`` #1A1210 at the edges, tattered tails, burn holes, the scorched open palm
on both faces). The banner hangs **behind** the pole (the pole's plane is a wall it never swings through), so from the
W1 camera it reads as a crimson flag above his head: the silhouette is "the banner that will not fall". Bones:
``standard_01..02`` (spine_03 → the pole: one rigid piece on ``standard_01`` pivoting at the rupture, tipping back
and to his left as one piece when the standard **droops** in Death) and
``flag_01..03`` (the banner cloth under the crossbar: gravity + a looping wind it should not have, ``flare`` in Roar).

The **oath-sword** ``SM_MonWeapon_OathSword`` (a 1.56 m two-hander, ``valdras.sword`` #6A6D73, worn edges) on
``weapon_r``: its grip has **melted into his gauntlet** — molten iron (``valdras.gauntlet``) flows from the fist over
the cross and down the grip, cracked with violet. Torn tabard remnants (``valdras.cloth`` #4A1A1A) hang under the
faulds front and back (``tabard_f/b`` chains, the warrior's / Unburied names).

Clips (``valdras_clips.py``): Idle (at attention: the sword planted point-down, both hands on the hilt), Walk (a
march, the sword at shoulder arms), Attack01 (the two-hit combo, Contact 650 / Contact2 1000), Attack02 (Banner
Sweep, Contact 700), Cast01 (Oath-Brand slam, Contact 900), Roar (phase change: the standard flares), Lunge (Contact
800), Hurt, Death (kneels, sword point-down, the standard droops), Rise (Death reversed: his spawn), Stun.
Portrait ``T_UI_Portrait_Valdras``. Ink: the **boss** class (3.5 px, gate 3.0 px).
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "march", HERE.parent / "items", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Quaternion, Vector  # noqa: E402

from kit import mesh as M, outline, rig as R, scene, shading  # noqa: E402
from kit.palette import Palette  # noqa: E402
from kit.paths import scratch_root  # noqa: E402
from kit.rig import Bind, HumanoidSpec  # noqa: E402

import common as C  # noqa: E402
from common import Profile  # noqa: E402
import goblins as GB  # noqa: E402
import hero_base as HB  # noqa: E402
import mgeo as G  # noqa: E402
from mgeo import lerp, smooth01  # noqa: E402
import unburied as UB  # noqa: E402

ASSET = "SK_Mon_Valdras"
TOKEN = "Mon_Valdras"
PREVIEW = "mon_valdras"
GAME_IDS = ["monster_valdras"]
DISPLAY = {"zh-CN": "焚旗元帅·瓦德拉斯", "en": "Lord-Marshal Valdras"}
FAMILY = UB.FAMILY                # Monsters_March
PREFIX = "valdras"
OUTLINE_CLASS = "boss"            # spec §1.3: bosses 3.5 px (gate 3.0 px) — the Chapter-1 act boss
SWORD = "SM_MonWeapon_OathSword"
PORTRAIT = "T_UI_Portrait_Valdras"
MARKER = ("valdras.marker",)      # the visor slit (manifest ``marker``; darkgate ≥ 2 px)
VISOR_GLOW = "#E8D8FF"            # = valdras.visor (never ember)
SEAM_HEX = "#B02FD0"

HEIGHT = 2.10                     # bare crown (the helm tops out at 2.13 m): a head and a half over the 1.76 m hero
REF_SPEED = 48 * 0.03             # monsters.md speed 48 → 1.44 m/s (the Walk ref speed)
LOD0_MAX, LOD1_MAX = 11000, 4000  # monsters.md §2.6: a boss exception above the family's 9 k
BLOB = 0.46
CROP_H = 2.55                     # review framing height (game cells): body + the standard above the helm

SWATCHES = ("valdras.plate", "valdras.plate_edge", "valdras.plate_line", "valdras.seam", "valdras.seam_core",
            "valdras.visor", "valdras.marker", "valdras.plume", "valdras.plume_ash", "valdras.standard",
            "valdras.standard_char", "valdras.pole", "valdras.sword", "valdras.sword_edge", "valdras.gauntlet",
            "valdras.cloth")

# A7 appends (the registry stays append-only; ``march_palettes`` keeps the A1 swatches): the blackened plate with a
# lower light band than A1's ``blackmetal`` preset (.42): at the W1 pitch the camera sees mostly key-lit tops, and
# with .42 the Lord-Marshal read as a pale grey knight (lit plate L* 54); .30 keeps it black (lit L* 45, still +23 L*
# over the darkest march mud, the darkgate lit rule) and leaves the worn plate edges (``valdras.plate_edge``) as
# the highlights.
A7_SWATCHES = {"valdras.blackplate": ("#2E2F33", 0.55, 0.30, 0.0),
               # the banner's crimson with a narrower light band: the "dark" preset (.36) lit the flared banner
               # (Roar, the sweep's follow-through) a dusty salmon #986259; .25 keeps it brick crimson #864943,
               # still +16.5 L* over the darkest march ground (the A1 lit-band rule, 15)
               "valdras.banner": ("#5E1012", 0.52, 0.25, 0.0),
               # the tabard remnants likewise (#8B685E salmon-brown in the walk's flap → #775049, +16.5 L*)
               "valdras.tabard": ("#4A1A1A", 0.52, 0.25, 0.0),
               # ── review round 2 (appended; the swatches above stay in the registry, unused) ──────────────────
               # the plate's base split from its edge: a cool near-black mass (base L* 14, shade .62 → L* 3) whose
               # light band is a narrow neutral-cool sheen (L* 53: .44 of the warm light over a blue-black base),
               # +21 over the march grass / stone (L* 32) and +34 over the mud; at the W1 camera the light band is
               # ~1/5 of the figure (the key-lit tops), the base band the mass
               "valdras.plate_black": ("#1C2438", 0.62, 0.44, 0.0),
               # the key-lit bevels (lame rims, keel, ridges, haute-piece rims, the standard's iron): darker than
               # the A1 plate_edge in the base band (L* 21 instead of 32: no grey lines over the black mass), a
               # brighter sheen in the light band (L* 65)
               "valdras.plate_rim": ("#2A3246", 0.48, 0.56, 0.0),
               # brick crimson that stays crimson in the light band (Roar's flare, the back views): .25 over #5E1012
               # lit a dusty rose #864943 (a* 25); #701614 at .18 lights #8A3E37 (L* 36, a* 32), +16.7 over the mud
               "valdras.banner_brick": ("#701614", 0.52, 0.18, 0.0),
               "valdras.tabard_brick": ("#621A16", 0.52, 0.19, 0.0),     # lit #80433A (L* 36, a* 25)
               # char that stays char: the "dark" preset (.36) lit #1A1210 a pale brown #6C6357 (L* 43) and the
               # burnt tongues vanished in the light band; .08 keeps them #2C2420 (L* 15)
               "valdras.char": ("#1A1210", 0.55, 0.08, 0.0),
               # the burned plume: char-black horsehair (#241C1C at .36 lit a brown tuft #736A5F); ash tips stay
               # valdras.plume_ash
               "valdras.plume_char": ("#1A1414", 0.55, 0.12, 0.0),
               # ── review round 3 (appended) ─────────────────────────────────────────────────────────────────
               # the tabard's light band still read dusty in the back views (#7F433A, L* 36, a* 25 in Death NE
               # F86 / F120: the warm light at .19 greys a dull crimson): a redder base at .12 lights #7F312A
               # (L* 33, a* 34) — brick crimson, the banner's family (#8A3E37, a* 31)
               "valdras.tabard_crimson": ("#6E1814", 0.52, 0.12, 0.0),
               # the portrait slit's outer lip (portrait mesh only): a deep violet between the dark frame and the
               # breach-violet lip, so the slit grades frame → deep violet → violet → pale → hot core
               "valdras.slit_deep": ("#4A1466", 0.0, 0.0, 1.0)}
A7_REGIONS = {"plate": "valdras.plate_black", "plate_edge": "valdras.plate_rim", "standard": "valdras.banner_brick",
              "cloth": "valdras.tabard_crimson", "standard_char": "valdras.char", "plume": "valdras.plume_char",
              "slit_deep": "valdras.slit_deep"}


# ── proportions (body space: X left, −Y forward, Z up; metres) ─────────────────────────────────────────────────
TORSO_RINGS = [   # (z above the pelvis joint, rx, ry, cy): a barrel-chested cuirass, the waist pinched by the faulds
    (-0.035, 0.192, 0.150, 0.000), (0.030, 0.196, 0.150, 0.000), (0.130, 0.208, 0.156, -0.006),
    (0.250, 0.230, 0.168, -0.014), (0.370, 0.250, 0.180, -0.018), (0.470, 0.258, 0.177, -0.012),
    (0.555, 0.240, 0.160, -0.002), (0.615, 0.186, 0.130, 0.004), (0.650, 0.120, 0.100, 0.004)]
HELM_RINGS = [    # (z above the head centre, rx, ry, cy): a barrel great helm, flat crown, slight bucket flare
    (-0.152, 0.128, 0.142, -0.006), (-0.120, 0.121, 0.136, -0.006), (-0.070, 0.119, 0.134, -0.008),
    (-0.010, 0.121, 0.137, -0.008), (0.050, 0.121, 0.136, -0.006), (0.105, 0.117, 0.131, -0.004),
    (0.145, 0.106, 0.120, -0.002), (0.168, 0.086, 0.097, 0.000), (0.180, 0.052, 0.060, 0.000)]
HELM_EXP = 2.5
VISOR_Z, VISOR_H, VISOR_A = 0.030, 0.040, 47.0     # slit centre above hc, glow height, half angle
VISOR_FRAME_H = 0.062                              # the dark frame round the glow
BROW_Z = 0.082                                     # the brow band over the slit

# the standard (rest, body space): the pole bursts out of the back at ENTRY and runs **straight** (one rigid piece:
# review round 2 — the old knee bend read as a rubber hose when the standard drooped) to TOP; ``standard_01`` spans
# ENTRY → KNEE (a point on the line), ``standard_02`` KNEE → TOP and never turns against it. The top was lowered
# 32 cm (2.80 → 2.48: the finial at ~2.65 m instead of 2.97, monsters.md "≈ 2.4 m with the standard")
POLE_R = 0.032
STD_ENTRY = (0.30, 0.214)         # (z above the pelvis joint, y) where the pole leaves the backplate
STD_TOP_Z, STD_TOP_Y = 2.48, 0.400
STD_KNEE_T = 0.62                 # standard_01's tail along ENTRY → TOP
CROSS_Z = 2.34                    # crossbar height (rest)
CROSS_HALF = 0.36
FLAG_OFF = 0.050                  # the banner hangs this far behind the pole axis (+Y)
FLAG_W, FLAG_L = 0.66, 0.76
_FLAG_K = FLAG_L / 0.84
FLAG_TAILS6 = tuple(v * _FLAG_K for v in (0.84, 0.71, 0.88, 0.64, 0.86, 0.73, 0.82))   # tattered hem: tooth tips /
#                                                                                       notches (left → right)
FLAG_TAILS = tuple(v for a, b in zip(FLAG_TAILS6, FLAG_TAILS6[1:]) for v in (a, 0.5 * (a + b))) + FLAG_TAILS6[-1:]
FLAG_FOLD = 0.040
CLOTH_MATERIAL = "MI_AF_Toon_Valdras_Cloth"   # slot 2: the banner + tabard faces, the rim overridden (see cloth_slot)
CLOTH_RIM_HEX, CLOTH_RIM_ALPHA = "#8E3A34", 0.38
CLOTH_PARTS = ("banner", "banner_trim", "banner_sigil", "banner_burn", "tabard_skirt", "tabard_hem")
FLAG_PARTS = ("banner", "banner_trim", "banner_sigil", "banner_burn")
TABARD_L = {"tabard_f": 0.44, "tabard_b": 0.54}   # torn remnants under the faulds (front shorter: the kneel)
POLE_PARTS = ("pole", "pole_collar", "crossbar", "crossbar_knob", "finial", "pole_ring")

# the oath-sword (weapon_r space: the right fist at the origin, +Y along the blade, +Z the leading edge)
SW_POMMEL_Y, SW_GRIP0, SW_GRIP1 = -0.345, -0.315, 0.050
SW_GUARD_Y = 0.078
SW_BLADE0, SW_TIP = 0.100, 1.215
SW_LEFT_Y = -0.205                # the left fist on the grip (two-handed holds)
SW_POMMEL_HOLD = -0.300           # the left hand resting on the pommel (at attention)


# ── builder (exact Monsters_March values; LOD1: decimated hull proxies) ────────────────────────────────────────
LOD1_RATIO = 0.42
LOD1_SKIP = ("rivet", "breath", "crack", "crack_core", "root", "plume_ash", "banner_burn", "lame_rim", "trim",
             "knuckles", "wing_line", "keel", "sabaton_lame", "seam_minor", "pole_ring", "banner_sigil", "tasset_rim",
             "fauld_rim", "haute_rim", "seam_joint", "seam_shoulder", "ridge")


def builder(pal: Palette, lod: int = 0, ratio: float = LOD1_RATIO, hull_ratio: float = 1.0) -> UB.MBuilder:
    if not lod:
        return UB.MBuilder(ASSET, pal, prefix=PREFIX)
    return UB.MBuilder(ASSET, pal, prefix=PREFIX, lod_ratio=ratio, lod_skip=lambda n: GB.lod_base(n) in LOD1_SKIP,
                       hull_ratio=hull_ratio)


def band2(prof: Profile, z: float, height: float, out: float, a0: float = -180.0, a1: float = 180.0,
          segs: int = 20, z_of_a=None) -> M.Part:
    """A single-sided painted band on a loft (two rows: half the triangles of ``hero_base.band_sheet``)."""
    grid = [[prof.point((z_of_a(a0 + (a1 - a0) * i / segs) if z_of_a else z) + dz, a0 + (a1 - a0) * i / segs, out)
             for i in range(segs + 1)] for dz in (-height / 2, height / 2)]
    return HB.outward(HB.sheet_grid(grid), HB._axis_of(prof))


def lens_band(prof: Profile, z: float, height: float, out: float, half_a: float, tip: float = 0.3,
              segs: int = 24, z_of_a=None) -> M.Part:
    """``hero_base.band_sheet`` whose height tapers toward both ends (a lens: full ``height`` at the middle,
    ``tip`` × it at ±``half_a``) — the portrait slit's graded bands."""
    def h_at(a):
        t = min(1.0, abs(a) / half_a)
        return height * (tip + (1.0 - tip) * (1.0 - t ** 2.2) ** 0.8)
    angs = [-half_a + 2.0 * half_a * i / segs for i in range(segs + 1)]
    grid = [[prof.point((z_of_a(a) if z_of_a else z) + k * h_at(a) / 2, a, out) for a in angs]
            for k in (-1.0, 0.0, 1.0)]
    return HB.outward(HB.sheet_grid(grid), HB._axis_of(prof))


def use_a7(b) -> None:
    """Map the region keys ``plate`` / ``plate_edge`` / ``standard`` / ``cloth`` / ``standard_char`` / ``plume`` onto
    the appended A7 swatches (``A7_REGIONS``; appended to the Monsters_March registry once, then shared)."""
    from kit.palette import Region
    import march_palettes as MP
    for key, nm in A7_REGIONS.items():
        hx, s_, l_, e_ = A7_SWATCHES[nm]
        b.swatch[key] = b.palette.add(nm, Region(hx, s_, l_, e_, MP.O_MON))


def add_own_hull(b, part, *a, **kw) -> int:
    """``kit.mesh.Builder.add`` (not ``LodHullBuilder.add``): the LOD1 hull proxy is the decimated part itself, not
    a further ``hull_ratio`` decimation — thin cloth panels and the fauld rings lost their ink edge (3.8 px proud)."""
    return M.Builder.add(b, part, *a, **kw)


def exact(b, part, *a, **kw) -> int:
    """``Builder.add`` without the LOD decimation (the visor slit, seams: their shape is the identity)."""
    return UB.GB_add_exact(b, part, *a, **kw)


SEAM_W = 0.028                    # a joint seam's width along the limb: ≥ 2 px at 1080p on a vertical limb (1.2 px/cm
#                                   at the W1 distance × cos 50° foreshortening)
SEAM_SIDES = 8


def seam_ring(b, c: Vector, axis: Vector, radius, bind, covered_by: int, width: float = SEAM_W,
              name: str = "seam_joint", up=None) -> int:
    """A violet joint seam (review round 2: every plate join leaks the seized fire): an open band ``width`` along
    ``axis`` round the limb at ``c``, ``radius`` (or (rx, ry) with the limb tube's ``up``) 2-3 mm over the plate
    it lies on (emissive ``valdras.seam``; LOD0 only)."""
    ax = axis.normalized()
    ring = M.tube([c - ax * (width / 2), c + ax * (width / 2)], [radius, radius], sides=SEAM_SIDES, cap0=None,
                  cap1=None, up=up)
    return exact(b, ring, "seam", bind, name=name, outline=False, covered_by=covered_by)


class _Surf:
    """``mgeo`` surface adapter for a Profile (u = absolute z)."""

    def __init__(self, prof: Profile, lift: float = 0.0):
        self.p, self.lift = prof, lift

    def point(self, u, a, out=0.0):
        return self.p.point(u, a, out + self.lift)

    def normal(self, u, a):
        return self.p.normal(u, a)


# ── the kit ────────────────────────────────────────────────────────────────────────────────────────────────────
class ValdrasKit:
    """Rig + body of the Lord-Marshal (``SKEL_Human`` + ``tabard_f/b``, ``standard_01..02``, ``flag_01..03``)."""

    def __init__(self, asset: str = ASSET):
        self.asset = asset
        base = HumanoidSpec(name=asset, skeleton="SKEL_Human", thigh=0.45, shin=0.43, ankle=0.08, torso=0.53,
                            neck=0.13, head=0.25, hip_half=0.105, shoulder_half=0.205, upper_arm=0.31,
                            fore_arm=0.29, hand=0.11, foot_len=0.29, arm_down=62.0, elbow_prebend=12.0,
                            knee_prebend=3.0, face=False)
        self.spec = base.fit_height(HEIGHT)
        s = self.spec
        self.hip_z = s.ankle + s.shin + s.thigh
        self.pz = self.hip_z + s.pelvis_offset
        self.nz = self.pz + s.torso
        self.hc = self.nz + s.neck
        self.torso = Profile([(self.pz + z, rx, ry, cy) for z, rx, ry, cy in TORSO_RINGS], exp=2.35)
        self.helm = Profile([(self.hc + z, rx, ry, cy) for z, rx, ry, cy in HELM_RINGS], exp=HELM_EXP)
        self.spec = _with_chains(self.spec, self.chains())
        self.rig = R.build_humanoid(self.spec)
        self.J = self.rig.joints
        self.U = 0.0316 * HEIGHT / 1.76
        self.w = UB.SimpleWeb(self.spec)
        self.add_sockets()

    # ── rig ────────────────────────────────────────────────────────────────────────────────────────────
    def std_points(self) -> list[Vector]:
        z0, y0 = STD_ENTRY
        a, t = Vector((0.0, y0, self.pz + z0)), Vector((0.0, STD_TOP_Y, STD_TOP_Z))
        return [a, a.lerp(t, STD_KNEE_T), t]

    def pole_at(self, z: float) -> Vector:
        """Rest pole axis at height ``z`` (piecewise linear through the entry, the knee and the top)."""
        p = self.std_points()
        for a, b in zip(p, p[1:]):
            if z <= b.z or b is p[-1]:
                t = (z - a.z) / (b.z - a.z)
                return a.lerp(b, t)
        return p[-1]

    def flag_top(self) -> Vector:
        c = self.pole_at(CROSS_Z)
        return Vector((0.0, c.y + FLAG_OFF, CROSS_Z - 0.020))

    def tabard_top(self, sg: float) -> Vector:
        z = self.pz + 0.000
        rx, ry, cy, cx = self.torso.at(z)
        return Vector((0.0, cy - sg * (ry + 0.030), z))

    def chains(self) -> list:
        out = []
        for nm, sg in (("tabard_f", 1.0), ("tabard_b", -1.0)):
            L = TABARD_L[nm]
            t = self.tabard_top(sg)
            out.append((nm, "pelvis", [t, t + Vector((0, 0, -0.45 * L)), t + Vector((0, 0, -L))]))
        out.append(("standard", "spine_03", self.std_points()))
        t = self.flag_top()
        out.append(("flag", "standard_02", [t + Vector((0, 0, -FLAG_L * k / 3.0)) for k in range(4)]))
        return out

    def visor_point(self) -> Vector:
        return self.helm.point(self.hc + VISOR_Z, 0.0, 0.004)

    def add_sockets(self) -> None:
        rig = self.rig
        R.add_socket(rig, "visor", "head", self.visor_point())
        top = self.pole_at(STD_TOP_Z)
        R.add_socket(rig, "standard_top", "standard_02", top + Vector((0, 0, 0.06)))
        ft = self.flag_top()
        R.add_socket(rig, "banner", "flag_02", ft + Vector((0, 0.004, -0.5 * FLAG_L)))
        z0, y0 = STD_ENTRY
        R.add_socket(rig, "standard_root", "spine_03", Vector((0.0, y0 - 0.01, self.pz + z0)))
        # fx_overhead (nameplates, labels) over the helm; the story boss opts out of the elite marker (manifest
        # ``eliteMarker: "none"``: the visor slit is his marker) — if one must ever show, ``fx_elite`` holds it
        # forward of and high over the helm (36 cm / 70 cm from the head centre: >= 6.6 px clear of helm, plume, pole
        # and banner in se / sw / ne / nw at the W1 camera, 1080p — lower it projects onto the plume in the front
        # facings, higher onto the finial)
        for so in rig.sockets:
            if so["name"] == "fx_overhead":
                so["bone"] = "head"
                so["pos"] = Vector((0.0, 0.0, self.hc + 0.45))
        R.add_socket(rig, "fx_elite", "head", Vector((0.0, -0.36, self.hc + 0.70)))

    # ── helpers ──────────────────────────────────────────────────────────────────────────────────────────
    def tz(self, z: float) -> float:
        return self.pz + z

    def spine_bind(self) -> Bind:
        return Bind.blend("pelvis", "spine_01", "spine_02", "spine_03", smooth=0.03)

    def chest_bind(self) -> Bind:
        return Bind.blend("spine_01", "spine_02", "spine_03", smooth=0.03)

    # ── build ──────────────────────────────────────────────────────────────────────────────────────────
    def build(self, pal: Palette, lod: int = 0, ratio: float = LOD1_RATIO, hull_ratio: float = 1.0,
              name: str | None = None):
        from kit import asset
        self.lod = lod
        b = builder(pal, lod, ratio, hull_ratio)
        b.use(*SWATCHES)
        use_a7(b)
        self.b = b
        ids = {}
        ids["helm"] = self.add_helm(b)
        self.add_gorget(b, ids)
        ids["cuirass"] = self.add_cuirass(b)
        self.add_faulds(b, ids)
        self.add_tabard(b)
        for side in ("l", "r"):
            self.add_arm(b, side)
            self.add_pauldron(b, side)
            self.add_leg(b, side)
        self.add_standard(b, ids)
        self.add_banner(b)
        names = [p["name"] for p in b.parts]
        body = asset.finish_mesh(b, pal, OUTLINE_CLASS, rig=self.rig, grounding_height=HEIGHT,
                                 name=name or (f"{self.asset}_LOD{lod}" if lod else f"{self.asset}_Mesh"))
        body["af_parts"] = json.dumps(names)
        body["af_lod"] = lod
        cloth_slot(body, pal, names)
        return body

    # ── great helm: barrel, flat crown, the single visor slit, a burned plume ──────────────────────────────
    def add_helm(self, b) -> int:
        hp = self.helm
        hb = Bind.rigid("head")
        lod = getattr(self, "lod", 0)
        zs = [self.hc + z for z in np.linspace(HELM_RINGS[0][0], HELM_RINGS[-1][0], 11)]
        shell = HB.profile_mesh(hp, zs, 20 if not lod else 16, cap0="fan", cap1="fan", dome0=-0.03, dome1=0.006)
        proxy = HB.profile_mesh(hp, [self.hc + z for z in np.linspace(HELM_RINGS[0][0], HELM_RINGS[-1][0], 8)], 18,
                                cap0="fan", cap1="fan", dome0=-0.03, dome1=0.006)
        hid = b.add(shell.auto_sharp(70), "plate", hb, name="helm", hull=proxy, hull_pad=True)
        zv = self.hc + VISOR_Z
        dip = (lambda a: zv - 0.004 * (min(1.0, abs(a) / VISOR_A)) ** 2)
        if getattr(self, "portrait_slit", False):
            # the portrait's slit (review round 2: the 4 cm game slit, a 3-px marker at the W1 camera, read as a
            # flat white slab across the helm in the 512² bust): a third of the height, graded — the dark frame,
            # breach-violet lips, the pale marker, a hot white core line — under a soft halo card. Round 3: the
            # nested bands still ran full length with square ends (an LED bar): each is now a tapered lens, and
            # the hotter the band the shorter it runs, so the slit burns white only at its middle and fades
            # through pale and violet to deep-violet points at the ends
            exact(b, band2(hp, zv, 0.034, 0.0016, -VISOR_A - 6.0, VISOR_A + 6.0, segs=16),
                  "plate_line", hb, name="visor_frame", outline=False, covered_by=hid)
            b.use("breach.pinpoint")
            for nm, reg, h_, out, da, tip in (("visor_lip", "slit_deep", 0.0225, 0.0020, 3.5, 0.40),
                                              ("visor_lip", "seam", 0.0165, 0.0024, 1.0, 0.34),
                                              ("visor_lip", "breach.pinpoint", 0.0115, 0.0028, -5.0, 0.30),
                                              ("visor", "marker", 0.0072, 0.0032, -14.0, 0.26),
                                              ("visor", "seam_core", 0.0030, 0.0037, -25.0, 0.22)):
                exact(b, lens_band(hp, zv, h_, out, VISOR_A + da, tip=tip, segs=24, z_of_a=dip),
                      reg, hb, name=nm, outline=False, covered_by=hid)
        else:
            # the dark frame and the glowing slit (painted on the plate: never hidden by its own recess from above)
            exact(b, band2(hp, zv, VISOR_FRAME_H, 0.0016, -VISOR_A - 7.0, VISOR_A + 7.0, segs=16),
                  "plate_line", hb, name="visor_frame", outline=False, covered_by=hid)
            # breach-violet lips round the slit (the seams' language: violet rim, pale core); sub-pixel at the game
            # camera (a violet edge tint)
            exact(b, HB.band_sheet(hp, zv, VISOR_H + 0.014, 0.0023, -VISOR_A - 3.0, VISOR_A + 3.0, segs=18,
                                   z_of_a=dip),
                  "seam", hb, name="visor_lip", outline=False, covered_by=hid)
            exact(b, HB.band_sheet(hp, zv, VISOR_H, 0.0030, -VISOR_A, VISOR_A, segs=18,
                                   z_of_a=lambda a: zv - 0.004 * (abs(a) / VISOR_A) ** 2),
                  "marker", hb, name="visor", outline=False, covered_by=hid)
        # brow band over the slit (flush, plate edge), the bevel round the crown, a ridge front → back
        b.add(band2(hp, self.hc + BROW_Z, 0.020, 0.0020, segs=24), "plate_edge", hb,
              name="brow", outline=False, covered_by=hid)
        b.add(band2(hp, self.hc + 0.150, 0.016, 0.0018, segs=20), "plate_edge", hb,
              name="lame_rim", outline=False, covered_by=hid)
        b.add(band2(hp, self.hc - 0.140, 0.020, 0.0020, segs=20), "plate_edge", hb,
              name="lame_rim", outline=False, covered_by=hid)
        b.add(C.surface_strip(hp, 0.0, self.hc + 0.100, self.hc + 0.168, 0.020, 0.0030, steps=4, half_round=True),
              "plate_edge", hb, name="ridge", outline=False, covered_by=hid)
        b.add(C.surface_strip(hp, 0.0, self.hc - 0.135, self.hc - 0.012, 0.022, 0.0035, steps=5, half_round=True),
              "plate_edge", hb, name="ridge", outline=False, covered_by=hid)
        # breaths: a cross of small holes on the right cheek plate
        for k, (dz, da) in enumerate(((-0.042, -40.0), (-0.062, -40.0), (-0.062, -50.0))):
            exact(b, C.surface_dot(hp, self.hc + dz, da, 0.0075, 0.0015, sides=4), "plate_line", hb,
                  name="breath", outline=False, covered_by=hid)
        # rivets round the brow band
        for a in (-120.0, -70.0, 70.0, 120.0):
            b.add(C.surface_dot(hp, self.hc + BROW_Z, a, 0.0075, 0.004, sides=4), "plate_edge", hb, name="rivet",
                  outline=False, covered_by=hid)
        self.add_plume(b, hid)
        return hid

    def add_plume(self, b, hid: int) -> None:
        """The burned plume (review round 2): a **fanned crest** of charred horsehair — eleven tapered locks rising
        from a holder on the crown and fanning from upright at the brow to swept back over the nape, splaying
        sideways toward their tips so the crest reads as a ragged black fan from the front as well as the side;
        char-black (``valdras.plume_char``, lit L* 20), the burnt ends ashen (``valdras.plume_ash``)."""
        hb = Bind.rigid("head")
        top = self.hc + HELM_RINGS[-1][0]
        rng = scene.rng(self.asset, "plume")
        lod = getattr(self, "lod", 0)
        n = 11
        first = None
        for k in range(n):
            f = k / (n - 1)
            ang = math.radians(-20.0 + 72.0 * f + rng.uniform(-4.0, 4.0))     # from the brow (forward) to the nape
            ln = (0.110 + 0.090 * math.sin(math.pi * f ** 0.75)) * (1.0 - 0.30 * f * f) * rng.uniform(0.84, 1.08)
            side = (1.0 if k % 2 else -1.0) * (0.010 + 0.030 * f) + rng.uniform(-0.006, 0.006)
            y0 = -0.040 + 0.085 * f
            root = Vector((0.004 * (1.0 if k % 2 else -1.0), y0, top - 0.010 - max(0.0, y0 - 0.030) * 0.6))
            d = Vector((side, math.sin(ang), math.cos(ang))).normalized()
            droop = Vector((0.0, 0.06 * f, -0.18 * f * f))                      # the back locks arc down
            if lod and k % 2:              # LOD1: six of the eleven locks (the fan's outline; same random draws)
                continue
            pts = [root + (d * (ln * s) + droop * (ln * s * s)) for s in (0.0, 0.34, 0.68, 1.0)]
            rad = [0.016, 0.013, 0.009, 0.0035]
            lock = M.tube(pts, rad, sides=4, dome0=0.006, dome1=0.002, phase=0.5)
            i_ = b.add(lock, "plume", hb, name="plume", hull_pad=True)
            first = i_ if first is None else first
            # the burnt end: ash over the outer third
            ash = M.tube([pts[2].lerp(pts[3], 0.10), pts[3]], [rad[2] * 0.95 + 0.0011, rad[3] + 0.0011],
                         sides=4, dome0=0.0, dome1=0.002, phase=0.5)
            b.add(ash, "plume_ash", hb, name="plume_ash", outline=False, covered_by=i_)
        # the holder: a dark socket on the crown under the crest
        b.add(HB.ell((0.026, 0.060, 0.012), (0.0, 0.012, top - 0.002), sides=8, rings=4), "plate_edge", hb,
              name="plume_holder", outline=False, covered_by=hid)

    # ── gorget + the violet seam under the helm ──────────────────────────────────────────────────────────
    def gorget_profile(self) -> Profile:
        nz = self.nz
        return Profile([(nz - 0.060, 0.205, 0.150, 0.004), (nz - 0.020, 0.175, 0.132, 0.004),
                        (nz + 0.025, 0.130, 0.112, 0.002), (nz + 0.062, 0.112, 0.104, 0.000),
                        (nz + 0.082, 0.108, 0.101, 0.000)], exp=2.3)

    def add_gorget(self, b, ids: dict) -> None:
        gp = self.gorget_profile()
        gb = Bind.blend("spine_03", "neck_01", falloff=3.0)
        zs = list(np.linspace(gp.z0, gp.z1, 6))
        gid = b.add(HB.profile_mesh(gp, zs, 22, cap0=None, cap1="fan"), "plate", gb, name="gorget", hull_pad=True)
        b.add(band2(gp, gp.z0 + 0.012, 0.016, 0.0022, segs=20), "plate_edge", gb,
              name="lame_rim", outline=False, covered_by=gid)
        # the seams: the gap between gorget and helm leaks violet (a glowing ring round the neck), and the
        # gorget's lower lame over the breastplate
        exact(b, band2(gp, self.nz + 0.052, 0.028, 0.0016, segs=20), "seam", gb,
              name="seam_neck", outline=False, covered_by=gid)
        exact(b, band2(gp, gp.z0 + 0.034, 0.024, 0.0026, segs=20), "seam", gb,
              name="seam_neck", outline=False, covered_by=gid)

    # ── cuirass: barrel breastplate, keel, violet cracks, the waist seam; backplate rupture ─────────────────
    def add_cuirass(self, b) -> int:
        tp = self.torso
        lod = getattr(self, "lod", 0)
        z0, z1 = self.tz(TORSO_RINGS[0][0]), self.tz(TORSO_RINGS[-1][0])
        c0, c1 = self.tz(0.10), self.tz(0.56)

        def keel(z, a):
            if c0 < z < c1:
                w = math.sin(math.pi * (z - c0) / (c1 - c0)) ** 0.7
                return 0.012 * w * max(0.0, math.cos(math.radians(a))) ** 10
            return 0.0
        zs = list(np.linspace(z0, z1, 12))
        shell = HB.profile_mesh(tp, zs, 24 if not lod else 18, disp=keel, cap0="fan", cap1="fan", dome1=0.006)
        hull = HB.profile_mesh(tp, list(np.linspace(z0, z1, 9)), 20, disp=keel, cap0="fan", cap1="fan", dome1=0.006)
        cb = self.spine_bind()
        cid = b.add(shell, "plate", cb, name="cuirass", hull=hull, hull_pad=True)

        def on_plate(z, a):
            return keel(z, a) + 0.0022
        # the keel: the breastplate's two halves meet in a violet seam (its crest), a narrow bevel either side
        for a in (-7.5, 7.5):
            b.add(HB.painted_strip(tp, a, self.tz(0.11), self.tz(0.55), 0.010, on_plate, steps=10), "plate_edge",
                  cb, name="keel", outline=False, covered_by=cid)
        exact(b, HB.painted_strip(tp, 0.0, self.tz(0.10), self.tz(0.56), 0.022,
                                  lambda z, a: keel(z, a) + 0.0030, steps=10), "seam", cb,
              name="seam_keel", outline=False, covered_by=cid)
        exact(b, HB.painted_strip(tp, 0.0, self.tz(0.13), self.tz(0.53), 0.007,
                                  lambda z, a: keel(z, a) + 0.0040, steps=8), "seam_core", cb,
              name="seam_keel", outline=False, covered_by=cid)
        for a in (68.0, -68.0, 180.0):
            b.add(HB.painted_strip(tp, a, self.tz(0.06), self.tz(0.54), 0.012, on_plate, steps=8), "plate_line", cb,
                  name="keel", outline=False, covered_by=cid)
        # the upper breastplate edge (a rolled rim under the gorget)
        b.add(band2(tp, self.tz(0.585), 0.020, 0.0028, segs=24), "plate_edge", cb,
              name="lame_rim", outline=False, covered_by=cid)
        # violet cracks in the breastplate: the seized fire inside (seam + hot core), off the keel on both sides
        surf = _Surf(tp)
        cracks = [[(0.43, -10.0), (0.39, -20.0), (0.35, -17.0), (0.30, -27.0), (0.26, -24.0)],
                  [(0.47, 14.0), (0.44, 26.0), (0.40, 30.0), (0.37, 41.0)],
                  [(0.24, -5.0), (0.20, 6.0), (0.17, 2.0)]]
        for k, path in enumerate(cracks):
            pth = [(self.tz(h), a) for h, a in path]
            exact(b, G.surface_paint(surf, pth, 0.022, 0.0030, steps=5, taper=0.5), "seam", cb,
                  name="crack", outline=False, covered_by=cid)
            exact(b, G.surface_paint(surf, pth, 0.008, 0.0042, steps=5, taper=0.55), "seam_core", cb,
                  name="crack_core", outline=False, covered_by=cid)
        # the waist seam (cuirass → faulds)
        exact(b, band2(tp, self.tz(0.012), 0.026, 0.0016, segs=24), "seam", cb,
              name="seam_waist", outline=False, covered_by=cid)
        # the rupture where the standard bursts out of the backplate: torn lips, violet ring, roots
        self.add_rupture(b, cid)
        return cid

    def add_rupture(self, b, cid: int) -> None:
        tp = self.torso
        sb = Bind.blend("spine_02", "spine_03", falloff=4.0)
        z0, y0 = STD_ENTRY
        ze = self.tz(z0)
        surf = _Surf(tp)
        # violet ring round the pole's exit + a hot core ring
        ring = [(ze + 0.055 * math.cos(t), 180.0 + 18.0 * math.sin(t)) for t in np.linspace(0, 2 * math.pi, 13)]
        exact(b, G.surface_paint(surf, ring, 0.020, 0.0024, steps=12, taper=0.0), "seam", sb, name="seam_back",
              outline=False, covered_by=cid)
        exact(b, G.surface_paint(surf, ring, 0.007, 0.0036, steps=12, taper=0.0), "seam_core", sb,
              name="crack_core", outline=False, covered_by=cid)
        # torn plate lips bent outward round the wound (dark edge)
        for t in np.linspace(0.3, 2 * math.pi + 0.3, 6, endpoint=False):
            p = tp.point(ze + 0.075 * math.cos(t), 180.0 + 24.0 * math.sin(t), 0.004)
            n = tp.normal(ze, 180.0)
            d = Vector((math.sin(t) * -1.0, 0.0, math.cos(t))).normalized()
            lip = M.tube([p, p + d * 0.030 + n * 0.012], [0.010, 0.003], sides=4, dome0=0.0, dome1=0.002)
            b.add(lip, "plate_edge", sb, name="crack", outline=False, covered_by=cid)
        # roots of the standard spread over the backplate (the pole grew in), a few cracks of violet between
        rng = scene.rng(self.asset, "roots")
        for k, (h1, a1) in enumerate(((0.10, 160.0), (0.12, 205.0), (0.20, 140.0), (0.46, 150.0), (0.48, 214.0))):
            pth = [(ze + (self.tz(h1) - ze) * f, 180.0 + (a1 - 180.0) * f + rng.uniform(-3, 3))
                   for f in (0.0, 0.35, 0.7, 1.0)]
            b.add(G.surface_paint(surf, pth, 0.014, 0.0040, steps=5, taper=0.8), "pole", sb, name="root",
                  outline=False, covered_by=cid)

    # ── faulds + tassets ───────────────────────────────────────────────────────────────────────────────
    def fauld_profile(self) -> Profile:
        pz = self.pz
        return Profile([(pz + 0.030, 0.200, 0.156, 0.0), (pz - 0.030, 0.212, 0.164, 0.0),
                        (pz - 0.090, 0.226, 0.176, 0.0), (pz - 0.150, 0.240, 0.188, 0.0),
                        (pz - 0.175, 0.246, 0.193, 0.0)], exp=2.4)

    def add_faulds(self, b, ids: dict) -> None:
        fp = self.fauld_profile()
        pb = Bind.blend("pelvis", "spine_01", falloff=5.0)
        rows = ((0.020, -0.050), (-0.040, -0.105), (-0.095, -0.160))
        for k, (zt, zb) in enumerate(rows):
            out = 0.004 + 0.006 * k
            outer, inner = [], []
            for z in (self.pz + zt, self.pz + zb):
                ring_o, ring_i = [], []
                for a in G.angles(20):
                    ring_o.append(fp.point(z, a, out))
                    ring_i.append(fp.point(z, a, out - 0.010))
                outer.append(ring_o)
                inner.append(ring_i)
            part = C.thick_patch(outer, inner, wrap_u=True)
            fid = add_own_hull(b, part, "plate", pb, name="fauld", hull_pad=True)
            b.add(band2(fp, self.pz + zb + 0.008, 0.012, out + 0.0012, segs=20),
                  "plate_edge", pb, name="fauld_rim", outline=False, covered_by=fid)
            if k:      # the join under the lame above: violet in the gap
                zt_up = rows[k - 1][1]
                exact(b, band2(fp, self.pz + zt_up - 0.011, 0.020, out + 0.0016, segs=20), "seam", pb,
                      name="seam_joint", outline=False, covered_by=fid)
        # tassets: two curved plates hanging from the faulds over the front of the thighs, following the thighs
        for side, sx in (("l", 1.0), ("r", -1.0)):
            thigh = f"thigh_{side}"
            ztop, zbot = self.pz - 0.130, self.pz - 0.360

            def wfn(co, ztop=ztop, zbot=zbot):
                k = np.clip((ztop - co[:, 2]) / (ztop - zbot), 0.0, 1.0)
                wt = 0.15 + 0.80 * smooth01_np(k)
                return np.stack([1.0 - wt, wt], axis=1)
            bind = Bind.custom(("pelvis", thigh), wfn)
            hx = self.J[f"hip_{side}"].x
            outer, inner = [], []
            for z in np.linspace(ztop, zbot, 4):
                f = (ztop - z) / (ztop - zbot)
                r = 0.138 + 0.010 * f
                row_o, row_i = [], []
                for a in np.linspace(-62.0, 62.0, 7):
                    t = math.radians(a)
                    p = Vector((hx + sx * 0.012 + r * math.sin(t), -0.010 - r * math.cos(t) - 0.010 * f, z))
                    row_o.append(p)
                    row_i.append(p + Vector((-math.sin(t), math.cos(t), 0.0)) * 0.010)
                outer.append(row_o)
                inner.append(row_i)
            tid = b.add(C.thick_patch(outer, inner), "plate", bind, name=f"tasset_{side}", hull_pad=True)
            # rim along the lower edge
            pts = []
            for a in np.linspace(-58.0, 58.0, 6):
                t = math.radians(a)
                r = 0.148 + 0.0015
                pts.append(Vector((hx + sx * 0.012 + r * math.sin(t), -0.010 - r * math.cos(t) - 0.010, zbot + 0.012)))
            b.add(M.tube(pts, [0.006] * len(pts), sides=4, dome0=0.0, dome1=0.0), "plate_edge", bind,
                  name="tasset_rim", outline=False, covered_by=tid)

    # ── torn tabard remnants (front / back panels under the faulds) ──────────────────────────────────────
    def panel_grid(self, sg: float, L: float, w_top: float, w_bot: float, k0: float, k1: float, lift: float,
                   cols: int, rows: int, seed: str):
        t = self.tabard_top(sg)
        rng = scene.rng(self.asset, "tabard", seed)
        jit = [rng.uniform(-1.0, 1.0) for _ in range(cols + 1)]
        tear = []
        for c in range(cols + 1):
            u = c / cols
            tear.append(0.16 * max(0.0, 1.0 - abs(u - 0.30) / 0.10) + 0.11 * max(0.0, 1.0 - abs(u - 0.78) / 0.09))
        grid = []
        for r in range(rows + 1):
            k = lerp(k0, k1, r / rows)
            w = lerp(w_top, w_bot, k)
            row = []
            for c in range(cols + 1):
                u = c / cols - 0.5
                hem = L * (1.0 - tear[c] - 0.05 * (0.5 + 0.5 * jit[c]))
                z = t.z - hem * k
                bow = 0.020 * (1.0 - (2 * u) ** 2) * (0.4 + 0.6 * k)
                fold = 0.010 * math.sin(2 * math.pi * (u * 2.0 + 0.2)) * smooth01(k / 0.4)
                y = t.y - sg * (bow + fold + lift) - sg * 0.05 * k * k
                row.append(Vector((u * w, y, z)))
            grid.append(row)
        return grid

    def add_tabard(self, b) -> None:
        for sg, nm in ((1.0, "tabard_f"), (-1.0, "tabard_b")):
            L = TABARD_L[nm]
            t = self.tabard_top(sg)
            bind = GB.chain_bind(t, t + Vector((0, 0, -L)), ("pelvis", f"{nm}_01", f"{nm}_02"))
            cols, rows, th = 8, 4, 0.006
            w0, w1 = 0.24, 0.27
            o = self.panel_grid(sg, L, w0, w1, 0.0, 1.0, th / 2, cols, rows, nm)
            i = self.panel_grid(sg, L, w0, w1, 0.0, 1.0, -th / 2, cols, rows, nm)
            pid = add_own_hull(b, C.thick_patch(o, i, sub_tags=True), "cloth", bind, name="tabard_skirt",
                        sub_regions={1: "cloth", 2: "cloth", 0: "standard_char"}, hull_pad=True)
            for side in (1.0, -1.0):
                band = self.panel_grid(sg, L, w0, w1, 0.82, 1.0, side * (th / 2 + 0.0011), cols, 2, nm)
                part = HB.sheet_grid(band)
                f = next(iter(part.bm.faces))
                if f.normal.dot(Vector((0, -sg * side, 0))) < 0:
                    bmesh.ops.reverse_faces(part.bm, faces=list(part.bm.faces))
                    part.bm.normal_update()
                b.add(part, "standard_char", bind, name="tabard_hem", outline=False, covered_by=pid)

    # ── pauldrons: a big cop, two lames, a haute-piece guarding the neck, violet between the lames ──────────
    def add_pauldron(self, b, side: str) -> None:
        """A broad, flat-topped cop (superellipsoid: a shelf over the shoulder, a hard rim), two stepped lames down
        the upper arm, violet in the gap under the cop, and a tall haute-piece flange standing beside the neck."""
        J = self.J
        S = J[f"shoulder_{side}"]
        sx = 1.0 if side == "l" else -1.0
        cl, ua = f"clavicle_{side}", f"upperarm_{side}"
        lod = getattr(self, "lod", 0)

        def place(part, center, tilt):
            return part.rotate("y", -tilt * sx).translate(center)
        top_c = S + Vector((sx * 0.060, 0.004, 0.064))
        cop = (0.178, 0.172, 0.088)
        pb = Bind.blend(cl, ua, falloff=6.0)
        pid = b.add(place(HB.ell(cop, (0, 0, 0), sides=20 if not lod else 14, rings=8, exp=2.4, exp_v=3.2), top_c, 20),
                    "plate", pb, name=f"pauldron_{side}",
                    hull=place(HB.ell(cop, (0, 0, 0), sides=14, rings=6, exp=2.4, exp_v=3.2), top_c, 20), hull_pad=True)
        rim = band2(Profile([(-0.03, cop[0], cop[1]), (0.03, cop[0], cop[1])], exp=2.4), -0.012, 0.018, 0.0016, segs=20)
        b.add(place(rim, top_c, 20), "plate_edge", pb, name="lame_rim", outline=False, covered_by=pid)
        c2, c3 = S + Vector((sx * 0.100, 0.0, -0.016)), S + Vector((sx * 0.128, 0.0, -0.084))
        l2 = (0.152, 0.150, 0.058)
        l3 = (0.128, 0.128, 0.050)
        lid2 = b.add(place(HB.ell(l2, (0, 0, 0), sides=16 if not lod else 12, rings=6, exp=2.4, exp_v=3.0), c2, 32),
                     "plate", Bind.blend(cl, ua), name=f"lame2_{side}",
                     hull=place(HB.ell(l2, (0, 0, 0), sides=12, rings=6, exp=2.4, exp_v=3.0), c2, 32), hull_pad=True)
        b.add(place(HB.ell(l3, (0, 0, 0), sides=14 if not lod else 10, rings=6, exp=2.4, exp_v=3.0), c3, 44), "plate",
              Bind.rigid(ua), name=f"lame3_{side}",
              hull=place(HB.ell(l3, (0, 0, 0), sides=10, rings=6, exp=2.4, exp_v=3.0), c3, 44), hull_pad=True)
        # violet in the gap under the cop (on the second lame's upper face)
        # violet in the joins: round each lame's girth just under the plate above it (the lame's own surface at
        # that height: superellipsoid factor (1 − |z/c|³)^⅓, so the band sits on it, not inside it)
        for l_, cc, tilt, zl, bnd in ((l2, c2, 32, -0.010, Bind.blend(cl, ua)), (l3, c3, 44, -0.006, Bind.rigid(ua))):
            fz = (1.0 - abs(zl / l_[2]) ** 3.0) ** (1.0 / 3.0)
            pr = Profile([(-0.03, l_[0] * fz, l_[1] * fz), (0.03, l_[0] * fz, l_[1] * fz)], exp=2.4)
            exact(b, place(band2(pr, zl, 0.024, 0.0010, segs=16), cc, tilt), "seam", bnd, name="seam_shoulder",
                  outline=False, covered_by=lid2)
        # haute-piece: a tall flange along the cop's inner edge, front to back, beside the neck (swept, rolled rim)
        hx = S.x - sx * 0.030
        ol = [(-0.120, 0.0), (0.120, 0.0), (0.110, 0.050), (0.070, 0.098), (0.010, 0.118), (-0.060, 0.104),
              (-0.110, 0.060)]
        hp = M.plate(C.ccw(ol), 0.014, bevel=0.003)
        # plate in XY (thick Z) → stand it in the YZ plane (X = the thickness), its long edge front → back
        m = (Matrix.Translation((hx, 0.010, S.z + 0.118)) @ Matrix.Rotation(math.radians(-16.0 * sx), 4, "Y")
             @ Matrix.Rotation(math.radians(90.0), 4, "Y") @ Matrix.Rotation(math.radians(90.0), 4, "Z"))
        hp.transform(m)
        hb_ = Bind.blend(cl, "spine_03", falloff=6.0)
        hid = b.add(hp, "plate", hb_, name=f"haute_{side}", hull_pad=True)
        edge = [m @ Vector((y, z, 0.0)) for y, z in ((-0.112, 0.058), (-0.060, 0.102), (0.010, 0.116), (0.068, 0.096),
                                                    (0.108, 0.050))]
        b.add(M.tube(edge, [0.0065] * len(edge), sides=4, dome0=0.0, dome1=0.0), "plate_edge", hb_,
              name="haute_rim", outline=False, covered_by=hid)
        for d in (Vector((0.0, -0.10, 0.06)), Vector((0.0, 0.10, 0.06))):
            b.add(HB.ell((0.011, 0.011, 0.006), top_c + Vector((sx * 0.05, d.y, d.z)), sides=5, rings=4),
                  "plate_edge", Bind.rigid(cl), name="rivet", outline=False, covered_by=pid)

    # ── arms: rerebrace, couter + wing, vambrace, bell cuff, plated fist ─────────────────────────────────
    def add_arm(self, b, side: str) -> None:
        J = self.J
        S, E, W = J[f"shoulder_{side}"], J[f"elbow_{side}"], J[f"wrist_{side}"]
        ua, la, hb, cl = f"upperarm_{side}", f"lowerarm_{side}", f"hand_{side}", f"clavicle_{side}"
        sx = 1.0 if side == "l" else -1.0
        lod = getattr(self, "lod", 0)
        sides = 14 if not lod else 10
        dua = (E - S).normalized()
        dla = (W - E).normalized()
        b.add(M.tube([S - dua * 0.02, E], [0.078, 0.068], sides=sides), "plate_line", Bind.blend(cl, ua, la),
              name=f"ua_{side}")
        rid = b.add(M.tube([S + dua * 0.10, S + dua * 0.24, E - dua * 0.03], [0.083, 0.080, 0.074], sides=sides,
                           dome0=0.0, dome1=0.0), "plate", Bind.rigid(ua), name=f"rerebrace_{side}", hull_pad=True)
        b.add(M.tube([S + dua * 0.235, S + dua * 0.250], [0.0845, 0.0845], sides=sides, cap0=None, cap1=None),
              "plate_edge", Bind.rigid(ua), name="lame_rim", outline=False, covered_by=rid)
        # couter with a fan-shaped wing, violet glow in the joint
        cop = HB.ell((0.072, 0.072, 0.072), E, sides=10, rings=6)
        cid = b.add(cop, "plate", Bind.blend(ua, la), name=f"couter_{side}",
                    hull=HB.ell((0.070, 0.070, 0.070), E, sides=10, rings=6), hull_pad=True)
        out = Vector((sx, 0.30, 0.0)).normalized()
        wing = HB.ell((0.016, 0.060, 0.064), (0, 0, 0), sides=10, rings=6)
        b.add(wing.transform(Matrix.Translation(E + out * 0.060) @ HB.rot_to(Vector((1, 0, 0)), out)), "plate",
              Bind.blend(ua, la), name=f"couter_wing_{side}")
        # the couter's joins: violet either side of the cop (the old ring sat inside the ball, invisible)
        seam_ring(b, E - dua * 0.072, dua, 0.0775, Bind.rigid(ua), rid)
        seam_ring(b, E + dla * 0.072, dla, 0.0760, Bind.blend(ua, la, hb, falloff=6.0), cid)
        b.add(M.tube([E + dla * 0.040, W - dla * 0.040], [0.074, 0.064], sides=sides, dome0=0.0), "plate",
              Bind.blend(ua, la, hb, falloff=6.0), name=f"vambrace_{side}")
        cuff = M.loft_z([(-0.050, 0.068, 0.068), (-0.012, 0.076, 0.076), (0.016, 0.090, 0.088), (0.024, 0.088, 0.086)],
                        sides=sides, cap0="fan", cap1="fan")
        cfid = b.add(cuff.transform(HB.axis_frame(W - dla * 0.016, dla)), "gauntlet", Bind.blend(la, hb, falloff=6.0),
                     name=f"cuff_{side}", hull_pad=True)
        seam_ring(b, W - dla * 0.074, dla, 0.0680, Bind.blend(la, hb, falloff=6.0), cfid, width=0.024)
        self.add_fist(b, side)

    def add_fist(self, b, side: str) -> None:
        J = self.J
        W, T = J[f"wrist_{side}"], J[f"handtip_{side}"]
        Gp = J[f"grip_{side}"]
        hd = (T - W).normalized()
        grip = Vector((0, -1, 0))
        zx = (grip - hd * grip.dot(hd)).normalized()
        xx = hd.cross(zx)
        fr = Matrix.Identity(4)
        for i in range(3):
            fr[i][0], fr[i][1], fr[i][2], fr[i][3] = xx[i], hd[i], zx[i], Gp[i]
        hb = f"hand_{side}"
        fist = HB.ell((0.066, 0.070, 0.070), (0, 0.012, 0), sides=10, rings=6, exp=2.8, exp_v=2.4)
        fid = b.add(fist.transform(fr), "gauntlet", Bind.rigid(hb), name=f"fist_{side}",
                    hull=HB.ell((0.064, 0.068, 0.068), (0, 0.012, 0), sides=10, rings=6, exp=2.8, exp_v=2.4).transform(fr),
                    hull_pad=True)
        palm = Vector((0, 0, 0)) - Gp
        palm.y = 0.0
        palm = (palm - hd * palm.dot(hd)).normalized()
        dorsal = -palm
        kp = HB.ell((0.020, 0.046, 0.054), (0, 0, 0), sides=8, rings=5, exp=2.6)
        m = Matrix.Translation(Gp + dorsal * 0.047 + hd * 0.012) @ HB.basis(dorsal, hd)
        b.add(kp.transform(m), "plate", Bind.rigid(hb), name="knuckles", outline=False, covered_by=fid)
        sg = 1.0 if side == "r" else -1.0
        th = M.capsule(Gp + zx * 0.052 + hd * -0.018 + palm * 0.02, Gp + zx * 0.064 + hd * 0.03, 0.022, 0.018, sides=7)
        b.add(th, "gauntlet", Bind.rigid(hb), name=f"thumb_{side}", outline=False, covered_by=fid)
        del sg

    # ── legs: cuisse, poleyn + wing (violet in the joint), greave, sabaton ───────────────────────────────
    def add_leg(self, b, side: str) -> None:
        J = self.J
        Hp, Kn, A = J[f"hip_{side}"], J[f"knee_{side}"], J[f"ankle_{side}"]
        th_, ca, ft, bl = f"thigh_{side}", f"calf_{side}", f"foot_{side}", f"ball_{side}"
        sx = 1.0 if side == "l" else -1.0
        lod = getattr(self, "lod", 0)
        sides = 16 if not lod else 12
        top = Hp + Vector((0, 0, 0.03))
        d = (Kn - top).normalized()
        b.add(M.tube([top, Kn], [(0.108, 0.100), (0.086, 0.080)], sides=sides, up=(0, -1, 0)), "plate_line",
              Bind.blend("pelvis", th_, ca), name=f"thigh_{side}")
        csid = b.add(M.tube([top + d * 0.10, top + d * 0.28, Kn - d * 0.08],
                            [(0.114, 0.106), (0.108, 0.100), (0.094, 0.088)], sides=sides, up=(0, -1, 0), dome0=0.0,
                            dome1=0.0), "plate", Bind.rigid(th_), name=f"cuisse_{side}", hull_pad=True)
        seam_ring(b, top + d * 0.114, d, (0.1165, 0.1085), Bind.rigid(th_), csid, up=(0, -1, 0))   # under the tasset
        seam_ring(b, Kn - d * 0.092, d, (0.0975, 0.0915), Bind.rigid(th_), csid, up=(0, -1, 0))  # cuisse → poleyn
        dk = (A - Kn).normalized()
        gid = b.add(M.tube([Kn + dk * 0.02, A + Vector((0, 0, 0.06))], [(0.090, 0.084), (0.070, 0.066)], sides=sides,
                           up=(0, -1, 0)), "plate", Bind.blend(th_, ca, ft, falloff=6.0), name=f"greave_{side}",
                    hull_pad=True)
        b.add(M.tube([Kn + dk * 0.18, A + Vector((0, -0.002, 0.10))], [0.006, 0.005], sides=4, dome0=0.0, dome1=0.0)
              .translate((0.0, -0.088, 0.0)), "plate_edge", Bind.blend(ca, ft, falloff=6.0), name="keel",
              outline=False, covered_by=gid)
        kc = Kn + Vector((0, -0.052, 0.006))
        kid = b.add(HB.ell((0.078, 0.060, 0.082), kc, sides=10, rings=6, exp_v=2.2), "plate", Bind.blend(th_, ca),
                    name=f"poleyn_{side}", hull_pad=True)
        b.add(HB.ell((0.018, 0.058, 0.064), Kn + Vector((sx * 0.088, -0.012, 0.0)), sides=10, rings=6), "plate",
              Bind.blend(th_, ca), name=f"poleyn_wing_{side}")
        seam_ring(b, Kn + dk * 0.074, dk, (0.0905, 0.0845), Bind.blend(th_, ca, ft, falloff=6.0), gid,
                  up=(0, -1, 0))                                                        # poleyn → greave
        seam_ring(b, A + Vector((0, 0, 0.092)), dk, (0.0740, 0.0700), Bind.blend(ca, ft, falloff=5.0), gid,
                  width=0.024, up=(0, -1, 0))                                           # ankle
        boot = M.boot(self.spec.foot_len * 1.0, 0.160, 0.128, shaft=0.040, toe_up=0.020)
        bid = b.add(boot.translate((A.x, 0, 0)), "plate", Bind.blend(ca, ft, bl, falloff=5.0), name=f"sabaton_{side}",
                    hull_pad=True)
        for i, yy in enumerate((-0.070, -0.135)):
            L = self.spec.foot_len
            hw, hh, cz = _boot_section(yy, L, 0.160, 0.128, 0.020)
            ring = M.sweep([(A.x, yy + 0.007, cz), (A.x, yy - 0.007, cz)], M.superellipse(1, 1, 10, 3.0),
                           scales=[(hh * 1.035, hw * 1.035)] * 2, up=(0, 0, 1), cap0="ngon", cap1="ngon")
            ring.deform(lambda co: Vector((co.x, co.y, max(co.z, 0.002))))
            b.add(ring.auto_sharp(50), "plate_edge", Bind.blend(ft, bl, falloff=5.0), name="sabaton_lame",
                  outline=False, covered_by=bid)

    # ── the standard: the pole grown through the spine, crossbar, finial ─────────────────────────────────
    def pole_bind(self) -> Bind:
        """The shaft is one rigid piece on ``standard_01`` (review round 2: no weight on ``standard_02``, so the
        pole can never bend along its length); only the root buried in the back rides ``spine_03``, blended over
        5 cm at the rupture — where the standard pivots when it holds itself upright or droops."""
        a = self.std_points()[0]

        def fn(co):
            below = np.clip((a.z + 0.02 - co[:, 2]) / 0.05, 0.0, 1.0)      # the buried root rides the spine
            below = below * below * (3.0 - 2.0 * below)
            return np.stack([below, 1.0 - below], axis=1)
        return Bind.custom(("spine_03", "standard_01"), fn)

    def add_standard(self, b, ids: dict) -> None:
        lod = getattr(self, "lod", 0)
        p = self.std_points()
        base = p[0] + (p[0] - p[1]).normalized() * 0.10            # the root end buried in the back
        axis = [base, p[0], p[0].lerp(p[1], 0.5), p[1], p[1].lerp(p[2], 0.5), p[2]]
        rad = [POLE_R * 0.92, POLE_R, POLE_R * 0.98, POLE_R * 0.95, POLE_R * 0.92, POLE_R * 0.86]
        pid = b.add(M.tube(axis, rad, sides=10 if not lod else 8, dome0=0.01, dome1=0.008), "pole", self.pole_bind(),
                    name="pole", hull_pad=True)
        # charred knots / iron rings bound round the pole
        for z, r in ((1.84, 0.006), (2.12, 0.006)):
            c = self.pole_at(z)
            ring = M.tube([c + Vector((0, 0, -0.012)), c + Vector((0, 0, 0.012))], [POLE_R + r] * 2, sides=10,
                          cap0=None, cap1=None)
            b.add(ring, "plate_edge", self.pole_bind(), name="pole_ring", outline=False, covered_by=pid)
        sb2 = Bind.rigid("standard_02")
        # crossbar on an iron collar, behind the pole (the banner hangs behind it)
        c = self.pole_at(CROSS_Z)
        yb = c.y + FLAG_OFF - 0.012
        b.add(M.tube([c + Vector((0, -0.002, -0.03)), c + Vector((0, 0, 0.03))], [POLE_R + 0.007] * 2, sides=10,
                     dome0=0.004, dome1=0.004), "plate_edge", sb2, name="pole_collar", hull_pad=True)
        b.add(M.box((0.020, FLAG_OFF + 0.010, 0.040), (0.0, (c.y + yb) / 2, CROSS_Z), bevel=0.003), "plate_edge", sb2,
              name="pole_collar", outline=False, covered_by=pid)
        bar = M.tube([(-CROSS_HALF, yb, CROSS_Z), (0.0, yb, CROSS_Z + 0.010), (CROSS_HALF, yb, CROSS_Z)],
                     [0.015, 0.016, 0.015], sides=6, dome0=0.006, dome1=0.006)
        b.add(bar, "pole", sb2, name="crossbar", hull_pad=True)
        for sg in (1.0, -1.0):
            knob = HB.ell((0.022, 0.022, 0.022), (sg * (CROSS_HALF + 0.012), yb, CROSS_Z), sides=6, rings=4)
            b.add(knob, "plate_edge", sb2, name="crossbar_knob", hull_pad=True)
        # the finial: the church's open hand, blackened iron, fingers splayed, one broken off
        top = self.pole_at(STD_TOP_Z)
        d = (p[2] - p[1]).normalized()
        fr = R.frame(top, d, Vector((0, -1, 0)))           # +Y up the pole, +Z out of the palm (forward)
        coll = M.tube([(0, -0.07, 0), (0, -0.01, 0)], [POLE_R + 0.008, POLE_R + 0.005], sides=8, dome0=0.003,
                      dome1=0.003)
        b.add(coll.transform(fr), "plate_edge", sb2, name="finial", hull_pad=True)
        palm = M.box((0.092, 0.084, 0.016), (0, 0.040, 0), bevel=0.0)
        b.add(palm.transform(fr), "plate_edge", sb2, name="finial", hull_pad=True)
        for k, (dx, ln, ang) in enumerate(((-0.033, 0.058, 10.0), (-0.011, 0.074, 3.0), (0.011, 0.036, -2.0),
                                            (0.033, 0.060, -9.0))):
            fg = M.box((0.017, ln, 0.014), (0, ln / 2, 0), bevel=0.0)
            fg.transform(Matrix.Translation((dx, 0.080, 0)) @ Matrix.Rotation(math.radians(ang), 4, "Z"))
            b.add(fg.transform(fr), "plate_edge", sb2, name="finial", hull_pad=True)
        thm = M.box((0.017, 0.052, 0.014), (0, 0.026, 0), bevel=0.0)
        thm.transform(Matrix.Translation((0.050, 0.030, 0)) @ Matrix.Rotation(math.radians(-48.0), 4, "Z"))
        b.add(thm.transform(fr), "plate_edge", sb2, name="finial", hull_pad=True)
        # a violet glint in the palm of the finial (the seized fire reaches the top of the standard)
        exact(b, HB.ell((0.020, 0.020, 0.004), (0.0, 0.040, 0.0092), sides=8, rings=4).transform(fr), "seam", sb2,
              name="seam_minor", outline=False, covered_by=pid)

    # ── the banner: cloth in the skeletal mesh (flag_01..03 under the crossbar) ──────────────────────────
    def add_banner(self, b) -> None:
        lod = getattr(self, "lod", 0)
        top = self.flag_top()
        L = FLAG_L
        bones = ("flag_01", "flag_02", "flag_03")
        d = Vector((0, 0, -L))
        L2 = d.length_squared

        def fn(co):
            t = np.clip(((co - np.array(top)) @ np.array(d)) / L2, 0.0, 1.3)
            w3 = _smooth(t, 0.58, 0.86)
            w1 = 1.0 - _smooth(t, 0.20, 0.48)
            w2 = np.clip(1.0 - w1 - w3, 0.0, 1.0)
            return np.stack([w1, w2, w3], axis=1)
        bind = Bind.custom(bones, fn)
        cols, rows = len(FLAG_TAILS) - 1, 6
        th = 0.006

        def P(x, y_down, off):        # banner point: x across, y_down below the top, off along +Y (behind)
            return Vector((x, top.y + off, top.z - y_down))

        def grid(off):
            out = []
            for r in range(rows + 1):
                row = []
                for c in range(cols + 1):
                    u = c / cols
                    x = -FLAG_W / 2 + FLAG_W * u
                    yd = FLAG_TAILS[c] * r / rows
                    row.append(P(x, yd, flag_fold(u, r / rows) + off))
                out.append(row)
            return out
        part = C.thick_patch(grid(th / 2), grid(-th / 2), sub_tags=True)
        pid = b.add(part, "standard", bind, name="banner", sub_regions={1: "standard", 2: "standard",
                                                                       0: "standard_char"}, hull_pad=True)

        def rip(x, yd):
            u = min(1.0, max(0.0, (x + FLAG_W / 2) / FLAG_W))
            tail = float(np.interp(u, np.linspace(0.0, 1.0, cols + 1), FLAG_TAILS))
            return flag_fold(u, min(1.0, max(0.0, yd / tail)))

        class Face:
            def __init__(self, sg, lift):
                self.sg, self.lift = sg, lift

            def point(self, u, a, out=0.0):
                return P(a, u, rip(a, u) + self.sg * (self.lift + out))

            def normal(self, u, a):
                return Vector((0.0, self.sg, 0.0))
        for sg in (1.0, -1.0):
            face = Face(sg, th / 2 + 0.0012)
            # charred band along both sides and the top (the cloth burnt at its edges)
            for c in (0, cols):
                x = (-FLAG_W / 2 + FLAG_W * c / cols) * 0.965
                pts = [face.point(FLAG_TAILS[c] * k, x) for k in (0.03, 0.25, 0.5, 0.72, 0.92)]
                b.add(M.tube(pts, [0.0065] * len(pts), sides=3, dome0=0.0, dome1=0.0), "standard_char", bind,
                      name="banner_trim", outline=False, covered_by=pid)
            pts = [face.point(0.014, x) for x in np.linspace(-0.29, 0.29, 5)]
            b.add(M.tube(pts, [0.0085] * len(pts), sides=3, dome0=0.0, dome1=0.0), "standard_char", bind,
                  name="banner_trim", outline=False, covered_by=pid)
            if lod:
                continue
            face = Face(sg, th / 2 + 0.0016)
            # the scorched open palm (the crusade's sigil burnt black). The back face (sg +1, seen from behind)
            # carries it centred; on the front face (sg −1, the hero's usual view) the pole stands in front of
            # the banner's centre line, so the palm sits beside it — 15 cm to his right, 85 % — the mirror image
            # of the back face's palm, as a mark burnt through the cloth reads from the other side
            yc = 0.31 * _FLAG_K
            cx, k = (0.0, 1.0) if sg > 0 else (-0.150, 0.85)

            def X(a, cx=cx, k=k):
                return cx + a * k

            def U(u, k=k):
                return yc + (u - yc) * k
            b.add(G.surface_patch(face, U(yc - 0.065), U(yc + 0.060), X(-0.095), X(0.095), 0.0, nu=3, na=4,
                                  shape=lambda tu, ta: 0.80 + 0.20 * math.sin(math.pi * tu)),
                  "standard_char", bind, name="banner_sigil", outline=False, covered_by=pid)
            for dx, ln in ((-0.071, 0.118), (-0.024, 0.150), (0.024, 0.142), (0.071, 0.112)):
                b.add(G.surface_paint(face, [(U(yc - 0.050), X(dx)), (U(yc - 0.050 - ln * 0.5), X(dx * 1.06)),
                                             (U(yc - 0.050 - ln), X(dx * 1.12))], 0.034 * k, 0.0, steps=3, taper=0.25),
                      "standard_char", bind, name="banner_sigil", outline=False, covered_by=pid)
            b.add(G.surface_paint(face, [(U(yc + 0.035), X(-0.088)), (U(yc - 0.008), X(-0.130)),
                                         (U(yc - 0.060), X(-0.158))], 0.032 * k, 0.0,
                                  steps=3, taper=0.3), "standard_char", bind, name="banner_sigil", outline=False,
                  covered_by=pid)
            # burns creeping in from the edges (tapered dark tongues: valdras.char stays char in the light band)
            for path in (((0.10, 0.325), (0.15, 0.27), (0.20, 0.25)), ((0.30, 0.325), (0.33, 0.29)),
                         ((0.52, -0.325), (0.57, -0.27), (0.63, -0.255)), ((0.08, -0.325), (0.11, -0.29))):
                b.add(G.surface_paint(face, [(u * _FLAG_K, a) for u, a in path], 0.044, 0.0, steps=3, taper=0.85),
                      "standard_char", bind, name="banner_burn", outline=False, covered_by=pid)


def cloth_material(pal: Palette):
    """Slot 2 of the body: the palette toon with the rim overridden (``RimOverride`` 1, a deep crimson #8E3A34 at .38 — the
    kit's cream rim at 55 % turned the dark crimson cloth pale pink wherever the banner or the tabard was seen at a
    grazing angle; the light band is the palette's). Parent ``M_AF_Toon`` (the cinderbloat crust's convention)."""
    base = shading.toon_material(pal)
    m = bpy.data.materials.get(CLOTH_MATERIAL)
    if m is not None:
        return m
    m = base.copy()
    m.name = CLOTH_MATERIAL
    grp = next(n for n in m.node_tree.nodes if n.type == "GROUP")
    grp.node_tree = shading.rim_override_group(CLOTH_RIM_HEX, CLOTH_RIM_ALPHA)
    m["af_parent"] = "M_AF_Toon"
    m["af_params"] = json.dumps({"RimOverride": 1.0, "RimOverrideColor": CLOTH_RIM_HEX,
                                 "RimOverrideAlpha": CLOTH_RIM_ALPHA})
    m["af_toon_slot"] = True
    return m


def cloth_slot(body, pal: Palette, names: list) -> None:
    """Move the cloth parts' toon faces (banner, tabard remnants) to slot 2 (``cloth_material``)."""
    me = body.data
    fp = np.zeros(len(me.polygons), np.int32)
    me.attributes["af_part"].data.foreach_get("value", fp)
    mi = np.zeros(len(me.polygons), np.int32)
    me.polygons.foreach_get("material_index", mi)
    ids = np.array([i for i, n in enumerate(names) if GB.lod_base(n) in CLOTH_PARTS], np.int32)
    sel = np.isin(fp, ids) & (mi == 0)
    me.materials.append(cloth_material(pal))
    mi = np.where(sel, 2, mi)
    me.polygons.foreach_set("material_index", mi)
    me.update()


def flag_fold(u: float, r: float) -> float:
    """Behind-side offset of the banner at (u across, r down): three vertical folds — the middle one bellying away
    from the pole, two troughs either side — pinned flat at the crossbar and deepening toward the hem, so the toon
    bands break along fold lines and a grazing view never lights the whole panel at once."""
    return -FLAG_FOLD * math.sin(3.0 * math.pi * u) * r ** 0.7


def _smooth(x, a, b):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def smooth01_np(x):
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3 - 2 * x)


def _boot_section(y: float, L: float, W: float, H: float, toe_up: float) -> tuple[float, float, float]:
    st = [(0.22 * L, 0.36 * W, 0.40 * H, 0.42 * H), (0.10 * L, 0.46 * W, 0.52 * H, 0.52 * H),
          (-0.15 * L, 0.50 * W, 0.46 * H, 0.46 * H), (-0.45 * L, 0.52 * W, 0.36 * H, 0.36 * H + toe_up * 0.3),
          (-0.68 * L, 0.46 * W, 0.28 * H, 0.28 * H + toe_up)]
    ys = [s[0] for s in st][::-1]
    return tuple(float(np.interp(y, ys, [s[i] for s in st][::-1])) for i in (1, 2, 3))


def _with_chains(spec: HumanoidSpec, chains: list) -> HumanoidSpec:
    from dataclasses import replace
    return replace(spec, chains=chains)


# ── the oath-sword (weapon_r space) ────────────────────────────────────────────────────────────────────────────
SW_HALF0, SW_HALF80 = 0.048, 0.034   # review round 2: 9.6 cm at the ricasso (was 8.0), tapering to 6.8 cm at 80 %
#                                      (was 6.4): the 1:30 blade read as a skewer at the game camera


def sword_half_width(y: float) -> float:
    if y <= SW_BLADE0:
        return SW_HALF0
    t = (y - SW_BLADE0) / (SW_TIP - SW_BLADE0)
    if t < 0.80:
        return SW_HALF0 - (SW_HALF0 - SW_HALF80) * t / 0.80
    g = (t - 0.80) / 0.20
    return max(SW_HALF80 * math.sqrt(max(0.0, 1.0 - g ** 1.5)), 0.004)


def sword_half_thick(y: float) -> float:
    w = sword_half_width(y)
    return max(0.0040, 0.0120 * (w / SW_HALF0) ** 0.7)


def build_sword(pal: Palette) -> tuple[bpy.types.Object, list]:
    """The oath-sword: a broad two-hander (1.11 m blade, hollow-ground, a dark fuller, worn bright edges with two
    chips), a heavy down-swept cross, a long leather grip, a wheel pommel — and the right gauntlet **melted into the
    grip**: molten iron (``valdras.gauntlet``) welded over the cross and the ricasso, run down the grip, dripping off
    the quillons, cracked with breach-violet."""
    import weapons as WP
    b = UB.MBuilder(SWORD, pal, prefix=PREFIX)
    b.use(*SWATCHES)
    ys = sorted(set([SW_BLADE0 + (SW_TIP - SW_BLADE0) * i / 20 for i in range(21)] +
                    [c + d for c in (0.42, 0.78) for d in (-0.02, -0.007, 0.0, 0.007, 0.02)]))
    rows = [(y, sword_half_width(y), sword_half_thick(y)) for y in ys if y < SW_TIP - 0.006]
    blade = WP.blade_solid(rows, SW_TIP - 0.010, edge_sub=True)
    chips = {1: [(0.42, 0.010, 0.020)], -1: [(0.78, 0.008, 0.018)]}

    def chip(co):
        if co.y < SW_BLADE0 + 0.02 or abs(co.z) < 1e-5:
            return co
        sgz = 1 if co.z > 0 else -1
        k = 1.0
        for c, depth, half in chips[sgz]:
            dd = abs(co.y - c)
            if dd < half:
                k = min(k, 1.0 - (depth / max(sword_half_width(co.y), 1e-4)) * (1.0 - dd / half))
        return Vector((co.x, co.y, co.z * k))
    blade.deform(chip)
    bid = b.add(blade, "sword", name="blade", sub_regions={2: "sword_edge"}, hull_pad=True)
    b.add(WP.ridge_paint(SW_BLADE0 + 0.03, SW_BLADE0 + 0.70, sword_half_width, sword_half_thick, 0.007),
          "plate_line", name="fuller", outline=False, covered_by=bid)
    # the cross: heavy, down-swept quillons (along ±Z, the edges' plane), flared ends
    gz = np.linspace(-0.165, 0.165, 9)
    guard = M.tube([(0, SW_GUARD_Y + 0.034 * (abs(z) / 0.165) ** 2, z) for z in gz],
                   [(0.013, 0.016) if abs(z) < 0.15 else (0.016, 0.019) for z in gz], sides=8, exp=3.0, up=(1, 0, 0))
    b.add(guard, "plate_edge", name="guard", hull_pad=True)
    for sg in (1.0, -1.0):
        b.add(HB.ell((0.019, 0.021, 0.019), (0.0, SW_GUARD_Y + 0.036, sg * 0.170), sides=8, rings=4), "plate_edge",
              name="guard_knob", hull_pad=True)
    # grip (leather, long) + wheel pommel
    gys = (SW_GRIP0, (SW_GRIP0 + SW_GRIP1) / 2, SW_GRIP1)
    grs = (0.0175, 0.0190, 0.0180)
    gid = b.add(M.tube([(0, y, 0) for y in gys], list(grs), sides=10), "plate_line", name="grip", hull_pad=True)
    b.add(WP.helix(SW_GRIP0 + 0.01, SW_GRIP1 - 0.02, 0.019, 4.0, 0.010, 0.0022, per_turn=9,
                   r_fn=lambda y: float(np.interp(y, gys, grs)), sides=10), "pole", name="wrap", outline=False,
          covered_by=gid)
    wheel = M.lathe([(0.022, -0.016), (0.034, -0.012), (0.037, 0.0), (0.034, 0.012), (0.022, 0.016)], sides=14,
                    cap0="fan", cap1="fan")
    wheel.rotate("y", 90).translate((0, SW_POMMEL_Y, 0))
    b.add(wheel.auto_sharp(40), "plate_edge", name="pommel", hull_pad=True)
    # the melt (review round 2: a mass that reads at the game camera): the right gauntlet's iron run into one lump
    # with the cross and the grip — a collar welded over the fist's top and the cross's centre, gobbets fused onto
    # both quillons near the centre, a tongue run down each flat of the ricasso, a lump under the fist down the grip
    # — cracked with violet (melt seams, emissive) where the metal is still hot inside
    rng = scene.rng(SWORD, "melt")
    lumps = [((0.0, 0.046, 0.0), (0.056, 0.064, 0.060)),          # the collar over fist + cross
             ((0.0, 0.098, 0.010), (0.036, 0.046, 0.046)),         # over the cross's centre / ricasso root
             ((0.006, 0.090, -0.064), (0.030, 0.034, 0.040)),      # fused onto the quillons
             ((-0.006, 0.086, 0.068), (0.028, 0.032, 0.036)),
             ((0.0, -0.064, 0.004), (0.036, 0.048, 0.038)),        # down the grip under the fist
             ((0.0, 0.150, -0.004), (0.024, 0.048, 0.030))]        # up the ricasso
    mid = None
    for k, (c, r) in enumerate(lumps):
        e = HB.ell(r, c, sides=10, rings=6)
        e.deform(lambda co, k=k: co + Vector((rng.uniform(-0.003, 0.003), 0, rng.uniform(-0.003, 0.003))))
        i_ = b.add(e, "gauntlet", name="melt", hull_pad=True)
        mid = i_ if mid is None else mid
    # the melt runs on down the ricasso (both flats) and drips off one quillon toward the blade
    for sgx, z0, ln in ((1.0, 0.018, 0.150), (-1.0, -0.014, 0.120)):
        base = Vector((sgx * 0.012, 0.110, z0))
        drip = M.tube([base, base + Vector((sgx * 0.001, ln * 0.55, 0.004)), base + Vector((0.0, ln, 0.0))],
                      [0.016, 0.012, 0.005], sides=6, dome0=0.005, dome1=0.003)
        b.add(drip, "gauntlet", name="melt_drip", hull_pad=True)
    for sgz in (1.0, -1.0):
        base = Vector((0.0, SW_GUARD_Y + 0.034 * (0.12 / 0.165) ** 2 + 0.010, sgz * 0.118))
        drip = M.tube([base, base + Vector((0.0, 0.034, 0.004 * sgz)), base + Vector((0.0, 0.062, 0.006 * sgz))],
                      [0.013, 0.009, 0.004], sides=6, dome0=0.004, dome1=0.003)
        b.add(drip, "gauntlet", name="melt_drip", hull_pad=True)
    # violet melt seams over the collar and the quillon gobbets (both faces), ~1 cm wide
    for sgx in (1.0, -1.0):
        crack = M.tube([(sgx * 0.050, 0.020, -0.040), (sgx * 0.058, 0.050, -0.006), (sgx * 0.052, 0.082, 0.030),
                        (sgx * 0.034, 0.110, 0.050)], [0.0050, 0.0058, 0.0052, 0.0040], sides=5, dome0=0.0,
                       dome1=0.0)
        exact(b, crack, "seam", name="melt_seam", outline=False, covered_by=mid)
        crack2 = M.tube([(sgx * 0.024, 0.074, -0.090), (sgx * 0.030, 0.092, -0.062), (sgx * 0.022, 0.112, -0.040)],
                        [0.0042, 0.0050, 0.0036], sides=5, dome0=0.0, dome1=0.0)
        exact(b, crack2, "seam", name="melt_seam", outline=False, covered_by=mid)
        crack3 = M.tube([(sgx * 0.034, -0.090, 0.010), (sgx * 0.038, -0.060, -0.012), (sgx * 0.030, -0.030, 0.006)],
                        [0.0036, 0.0044, 0.0036], sides=5, dome0=0.0, dome1=0.0)
        exact(b, crack3, "seam", name="melt_seam", outline=False, covered_by=mid)
    obj = finish_item_boss(b, pal, SWORD)          # the boss class hull (1.8 cm): the act boss's held weapon, as the chief's cleaver
    obj["af_outline_class"] = OUTLINE_CLASS
    socks = [{"name": "tip", "pos": (0.0, SW_TIP, 0.0)}, {"name": "mid", "pos": (0.0, (SW_BLADE0 + SW_TIP) / 2, 0.0)},
             {"name": "guard", "pos": (0.0, SW_GUARD_Y, 0.0)}, {"name": "pommel", "pos": (0.0, SW_POMMEL_Y, 0.0)}]
    return obj, socks


def finish_item_boss(b, pal, name):
    obj = b.build(name)
    mt = shading.toon_material(pal)
    outline.bake_hull(obj, outline.WIDTH_CM[OUTLINE_CLASS] / 100.0, mt, shading.outline_material(pal))
    for msg in outline.hull_color_report(obj, pal, OUTLINE_CLASS):
        print(f"[ink] WARNING {name}: {msg}")
    obj["af_outline_class"] = OUTLINE_CLASS
    return obj


# ── runtime glow cards (review) ─────────────────────────────────────────────────────────────────────────────────
VISOR_FX = {"socket": "visor", "color": VISOR_GLOW, "radiusCm": 9.0, "alpha": 0.55, "alphaPerFx": 0.2,
            "coreColor": "#F4ECFF", "coreRadiusCm": 3.0, "coreAlpha": 0.6, "blend": "additive",
            "falloff": "web glow", "facingFade": "clamp(2 * dot(visorFwd, toCamera) + 0.3, 0, 1)",
            "offAtDeathFraction": 0.72,
            "note": "the single visor slit (monsters.md §2.6: #E8D8FF; the family marker swatch valdras.marker); "
                    "never ember. The UE layer reads it as the hero's visorGlow card (socket visor on head)"}


def visor_glows(rig, scale_fn=None):
    """Runtime ``visorGlow`` card at the posed ``visor`` socket, faded by how much the helm faces the camera."""
    hb = rig.obj.data.bones["head"]

    def fn(cam, res):
        from kit import review
        pb = rig.obj.pose.bones["head"]
        fwd = (rig.obj.matrix_world.to_3x3() @ pb.matrix.to_3x3() @ hb.matrix_local.to_3x3().inverted()
               @ Vector((0, -1, 0))).normalized()
        so = next(x for x in rig.sockets if x["name"] == "visor")
        b = rig.obj.data.bones[so["bone"]]
        wp = rig.obj.matrix_world @ (pb.matrix @ (b.matrix_local.inverted() @ Vector(so["pos"])))
        vis = max(0.0, min(1.0, 2.0 * fwd.dot((cam.matrix_world.translation - wp).normalized()) + 0.3))
        if scale_fn is not None:
            vis *= scale_fn()
        px, py = review.project_px(cam, wp, res)
        wpp = outline.pixel_world_size(cam, wp, res[0])
        return [dict(center_px=(px, py), radius_px=VISOR_FX["radiusCm"] / 100.0 / wpp, color=VISOR_GLOW,
                     alpha=VISOR_FX["alpha"] * vis, core=VISOR_FX["coreColor"],
                     core_frac=VISOR_FX["coreRadiusCm"] / VISOR_FX["radiusCm"], core_alpha=VISOR_FX["coreAlpha"] * vis)]
    return fn


# ── build ──────────────────────────────────────────────────────────────────────────────────────────────────────
def toon_tris(obj) -> int:
    """Every toon-lit slot (0 + the cloth slot 2), as ``mship.toon_tris`` and the manifest budget count them."""
    return sum(M.tri_count(obj, i) for i in range(len(obj.data.materials)) if i != 1)


def build_lod1(kit: ValdrasKit, pal: Palette):
    ratio = LOD1_RATIO
    body1 = kit.build(pal, lod=1, ratio=ratio, hull_ratio=0.75)
    n = toon_tris(body1)
    for _ in range(6):
        if n <= LOD1_MAX - 60:
            break
        me = body1.data
        bpy.data.objects.remove(body1, do_unlink=True)
        bpy.data.meshes.remove(me)
        ratio *= (LOD1_MAX - 160) / n
        body1 = kit.build(pal, lod=1, ratio=ratio, hull_ratio=0.75)
        n = toon_tris(body1)
    print(f"[lod1] {kit.asset}: {n} toon tris at ratio {ratio:.3f}")
    kit.lod = 0
    return body1


def build_all(lod1: bool = False):
    scene.reset()
    pal = Palette(FAMILY)
    kit = ValdrasKit()
    body = kit.build(pal)
    body1 = build_lod1(kit, pal) if lod1 else None
    sword, socks = build_sword(pal)
    return dict(pal=pal, kit=kit, body=body, body1=body1, sword=sword, sword_socks=socks)


def part_report(body) -> str:
    names = json.loads(body["af_parts"])
    me = body.data
    pa = me.attributes.get("af_part")
    mi = np.zeros(len(me.polygons), np.int32)
    me.polygons.foreach_get("material_index", mi)
    idx = np.zeros(len(me.polygons), np.int32)
    pa.data.foreach_get("value", idx)
    agg = {}
    for i, m in zip(idx, mi):
        if m != 0:
            continue
        nm = GB.lod_base(names[i]) if i < len(names) else "?"
        agg[nm] = agg.get(nm, 0) + 1
    tot = sum(agg.values())
    rows = sorted(agg.items(), key=lambda kv: -kv[1])
    return f"toon faces {tot}: " + ", ".join(f"{k} {v}" for k, v in rows[:40])


def quick(out: Path, pose: str | None = None) -> None:
    import valdras_clips as VC
    B = build_all()
    kit = B["kit"]
    rig = kit.rig
    A = VC.anims(kit)
    R.attach_to_bone(B["sword"], rig, "weapon_r")
    A.bind_meshes(B["body"], {"weapon_r": B["sword"]})
    print(part_report(B["body"]))
    print(f"[build] toon {M.tri_count(B['body'], 0)} hull {M.tri_count(B['body'], 1)}; sword "
          f"{M.tri_count(B['sword'], 0)} + {M.tri_count(B['sword'], 1)}")
    bp = None
    if pose == "march":
        bp = VC.march_pose(A)
    UB.dev_review(ASSET, rig, [B["body"], B["sword"]], A, out, CROP_H, pose=bp, glows=visor_glows(rig), blob=BLOB,
                  px=outline.SCREEN_PX_1080[OUTLINE_CLASS])


def anim_quick(out: Path, names, facing: str = "se") -> None:
    import mship
    import valdras_clips as VC
    B = build_all()
    kit = B["kit"]
    A = VC.anims(kit)
    R.attach_to_bone(B["sword"], kit.rig, "weapon_r")
    A.bind_meshes(B["body"], {"weapon_r": B["sword"]})
    cs = [c for c in VC.clips(A) if not names or c.name in names]
    UB.march_stage()
    mship.anim_review(kit.rig, [B["body"]], [(B["sword"], "weapon_r")], cs, TOKEN, out, CROP_H, OUTLINE_CLASS,
                      blob_radius=BLOB, glows=visor_glows(kit.rig), facing=facing)


def dev_frames(out: Path, specs: list[str]) -> None:
    """Dev: game-camera 1080p cells (``frames_game*.png``, ink measured) + close-ups (``poses_close_*.png``) of
    ``Clip:frame[:facing]`` specs → ``out``."""
    import mship
    import valdras_clips as VC
    B = build_all()
    kit = B["kit"]
    A = VC.anims(kit)
    R.attach_to_bone(B["sword"], kit.rig, "weapon_r")
    A.bind_meshes(B["body"], {"weapon_r": B["sword"]})
    keys = []
    for sp in specs:
        bits = sp.split(":")
        keys.append((bits[0], int(bits[1]), bits[2] if len(bits) > 2 else "se"))
    want = {k[0] for k in keys} | {"Idle"}
    cs = [c for c in VC.clips(A) if c.name in want]
    actions = [mship.bake_clip(kit.rig, c, TOKEN) for c in cs]
    UB.march_stage()
    out.mkdir(parents=True, exist_ok=True)
    item = mship.Item(B["sword"], SWORD, "weapon_r")
    ms = mship.MonsterShip(asset=ASSET, token=TOKEN, preview_dir="", pal=B["pal"], A=A, body=B["body"], items=[item],
                           clips=cs, outline_class=OUTLINE_CLASS, height=CROP_H, glows=visor_glows(kit.rig),
                           close_keys=keys, native_sheets=True)
    rv = mship.MReview(ASSET, kit.rig.obj, [B["body"], B["sword"]], CROP_H, blob_radius=BLOB, out_dir=out,
                       game_outline_px=mship.ink_px(ms), outline_class=OUTLINE_CLASS, glows=visor_glows(kit.rig))
    rv.gate = mship.gate_for(OUTLINE_CLASS, mship.ink_px(ms))
    rv.ground_occludes = True
    rv.native_prefixes = ("frames_",)
    for k in range(0, len(keys), 4):
        mship.game_sheet(ms, rv, keys[k:k + 4], f"frames_game_{chr(97 + k // 4)}.png", actions, cols=4)
    mship.closeup_poses(ms, rv, actions)
    for p_, sz in rv.written:
        print(f"  {p_.name} {sz // 1024} KB")
    for st in rv.ink_reports:
        print(f"  [ink] {st.get('tag')}: rim {st.get('fracRim', 0) * 100:.1f} %")


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    scratch = scratch_root("a7", "valdras")
    if "--qa" in args:
        import mship
        import valdras_ship as VS
        from hero_base import PairQA
        ms = VS.qa_setup(lod1=False)
        names = [a for a in args[args.index("--qa") + 1:] if not a.startswith("--")]
        if names:
            ms.clips = [c for c in ms.clips if c.name in names]
        actions = [mship.bake_clip(ms.A.rig, c, TOKEN) for c in ms.clips]
        for it in ms.items:
            R.attach_to_bone(it.obj, ms.A.rig, it.socket)
        from hero_base import PairQA as PQ
        q = PQ(ms.A.rig, ms.body, {it.asset: it.obj for it in ms.items}, json.loads(ms.body["af_parts"]),
               {"body": "*", **ms.qa_groups}, ms.qa_pairs, {it.asset: it.grip for it in ms.items if it.grip})
        q.debug = "--debug" in args
        rep = q.run(ms.clips, actions)
        print(PairQA.table(rep))
        for f in PairQA.verdict(rep, ms.qa_limits, ms.qa_clip_limits):
            print("  QA FAIL " + f)
        if q.debug:
            for cn, r in rep.items():
                for fr in r["frames"]:
                    if fr.get("parts"):
                        print(cn, fr["frame"], fr["parts"])
    elif "--portrait" in args:
        # dev: the portrait alone → scratch (set AF_EXPORT_ROOT to a scratch dir: portrait_shot writes Portraits/)
        import mship
        import valdras_ship as VS
        UB.march_stage()
        ms = VS.qa_setup(lod1=False)
        out = scratch / "portrait"
        out.mkdir(parents=True, exist_ok=True)
        for it in mship.held(ms):
            R.attach_to_bone(it.obj, ms.A.rig, it.socket)
        rv = mship.MReview(ASSET, ms.A.rig.obj, [ms.body] + [it.obj for it in ms.items], CROP_H, blob_radius=BLOB,
                           out_dir=out, game_outline_px=mship.ink_px(ms), outline_class=OUTLINE_CLASS)
        VS.portrait(ms, rv, [])
        print("[portrait]", [str(p_) for p_, _ in rv.written])
    elif "--frames" in args:
        specs = [a for a in args[args.index("--frames") + 1:] if not a.startswith("--")]
        dev_frames(scratch / "frames", specs)
    elif "--anims" in args:
        names = [a for a in args[args.index("--anims") + 1:] if not a.startswith("--")]
        fac = next((a.split("=")[1] for a in args if a.startswith("--facing=")), "se")
        anim_quick(scratch / "anims", names, fac)
    elif "--quick" in args:
        k = args.index("--quick")
        out = Path(args[k + 1]) if len(args) > k + 1 and not args[k + 1].startswith("--") else scratch
        pose = next((a.split("=", 1)[1] for a in args if a.startswith("--pose=")), None)
        quick(out, pose)
    else:
        import valdras_ship as VS
        sys.exit(VS.main(args))

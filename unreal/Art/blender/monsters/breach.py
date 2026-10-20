"""Breach-blood (渊血) — dark-remake A5: ``SK_Mon_Breachclot`` 渊血凝块 and ``SK_Mon_Cinderbloat`` 烬胀囊 on
``SKEL_Slime`` (Docs/dark-remake/monsters.md §2.1, §5; DECISIONS R12, DR5). README §23.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/breach.py [clot|bloat|all]
        [--quick [Clip frame]] [--game] [--anims [Clip …] [--facing=se]] [--qa] [--no-previews] [--no-verify]

monsters.md §2.1: *breach-blood leaves a tar puddle, not a corpse*. Both re-themes keep the slime's rig (the 16-bone
``SKEL_Slime``: same names and hierarchy, so both meshes import onto the shipped skeleton) and its clip machinery
(the web ``SlimePose`` model: squash / stretch as bone scale, lean, hop, the pseudopod); ``slime.py`` is only read
(profile, helpers). Nothing cute is left:

* **Breachclot** (fodder, the slime's 0.43 × 0.88 m dome, lumped a little): a **translucent tar shell pushed almost
  to black** (``clot.pitch`` #160609, a crimson-black; slot 2 ``MI_AF_Toon_Slime_Breachclot`` on ``M_AF_Toon_Slime``:
  ``BodyOpacity`` .62, R12 alpha blend, back faces culled) with a **breach-violet fresnel rim** (``RimOverride`` 1,
  ``RimOverrideColor`` #B02FD0 — written as sRGB floats, see ``RIM_PARAM_NOTE`` — ``RimOverrideAlpha`` 1.0: slot
  params; the previews render exactly that rim) and a **wet highlight** (``clot.gloss``: an oval and a dot on the
  back shoulder). The cartoon eyes are gone; inside, where the bone and the bubbles were: a **dented kettle helm**
  (``clot.helm``) lying on its side so the W1 camera sees its profile, a medial comb over its crown and a dent beside
  it, **two crossbow bolt-heads** (``clot.bolt``, broad wedges) suspended in the tar, and **four violet sparks**
  drifting round them (``clot.marker`` #D08CFF, emissive — the family marker). An opaque, unshaded **backing bowl** (``clot.pitchdepth``) is what the blend
  resolves against.
  Clips are the slime's (Idle, Walk hop, Attack01 slam, Hurt, Death melt, Stun); the sparks stand in for the eyes:
  they dim on a hit, swell and gather to the front on the slam's wind-up, gutter out in Death, and the helm and bolts
  settle flat in the tar puddle.
* **Cinderbloat** (exploder, the clot × 1.35, × 1.12 taller: swollen): an **opaque cracked crust of ash-dusted
  char** in plates (``bloat.ashchar`` #2E2C34: an ash-grey light band, a charcoal base band, a near-black shade; the
  radius dips along every crack, so the plates bulge between them) with dark cracks (``bloat.crack`` #1A0A0A)
  radiating from a **torn vent** at the crown (a dark Y-shaped tear with uneven torn lips, violet only as a sliver on
  the inner lip of its long arm), ring
  cracks cutting the plates, **breach-violet fissures** in the primaries and the upper ring (``bloat.marker``
  #B02FD0 emissive — never ember) and the same breach-violet rim as the clot (slot 2
  ``MI_AF_Toon_Cinderbloat_Crust`` on ``M_AF_Toon``, ``RimOverride`` .65); ash flakes drifting off it are a runtime
  emitter (manifest ``fx.ashFlakes``, A12). **Death is the fuse** (900 ms): the crust swells and shudders while
  glowing violet seams break out of the cracks (``core`` bone, ``FX_Fuse`` 0 ms), the fuse light swells (manifest
  ``fx.fuse.glow``, rendered in the previews as the runtime glow card), ``FX_Burst`` at 800 ms (A12's violet-black
  ``death.burst`` + ash ring): the crust collapses in one frame — every fissure, ridge and the vent out with it — and
  six dark char shards land on the scorched puddle. The fuse needs deathDuration 900 ms at play rate 1, no death
  throw and no body fade before the burst (``FUSE_RUNTIME``).

Bone use (the 16 SKEL_Slime names are kept; manifest ``boneUse``): clot ``bone_prop`` = the helm, ``bubble_01/02`` =
the bolt-heads, ``eye_l/r`` + ``xeye_l/r`` = the four sparks (``core`` / ``lid_l/r`` unweighted); bloat ``core`` =
the fuse ridges, ``bone_prop`` / ``bubble_01/02`` / ``eye_l/r`` / ``xeye_l`` = the burst's six char shards
(``xeye_r`` / ``lid_l/r`` unweighted).

Every colour is a ``Monsters_March`` swatch: A1's (``march/march_palettes.py``) plus the A5 appends
(``A5_SWATCHES``; the registry is append-only, so art review 2's values are new names); :class:`MarchBuilder` maps
region keys onto swatch names *and* values. No ember anywhere (DR5). Ink: the monster class (1.5 cm baked) with the Chapter-1 character
request of 3.5 px (gate 3.0 px), like the slime.
"""
from __future__ import annotations

import json
import math
import sys
from dataclasses import dataclass, field, replace
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "march", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Quaternion, Vector  # noqa: E402

from kit import anim, color as KC, mesh as M, outline, rig as R, scene, shading  # noqa: E402
from kit.anim import Clip  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402
from kit.paths import scratch_root  # noqa: E402
from kit.rig import Bind  # noqa: E402

import march_palettes as MP  # noqa: E402
import mgeo as G  # noqa: E402
import slime as SL  # noqa: E402   (read-only reuse: profile, dome helpers, the bowl tricks)

FAMILY = "Monsters_March"
SKELETON = SL.SKELETON                      # "SKEL_Slime": same 16 bone names / hierarchy as SK_Mon_Slime
OUTLINE_CLASS = "monster"
OUTLINE_PX = 3.5
OUTLINE_PX_NOTE = ("Chapter-1 character gate (dark rim >= 3 px on >= 90 % of the 1080p silhouette edges): as the slime, "
                   "the dome's lower contour (lifted off the terrain by the overhanging bulge) keeps < 3 px on part of "
                   "the silhouette at the monster class's 3.0 px; 3.5 px, as every Chapter-1 character ships, holds "
                   "it (gate 3.0 px)")
# monsters.md §1.2 / DR5: enemy wind-up tints are the telegraph pair (bone fill → crimson for elites), never ember
TELEGRAPH = {"normal": "#FFE8DC", "elite": "#D11F2E"}
BREACH, BREACH_PIN, BREACH_CORE = MP.BREACH, MP.BREACH_PIN, MP.BREACH_CORE
# review stage: the march's ash grass (A1 MARCH_GROUND "0", L* 31), as the other Wave-1 monster previews
STAGE_HEX = MP.MARCH_GROUND["0"][1]
H0, W0 = SL.H, SL.W                          # the slime's dome: 0.43 m tall, 0.44 m half-width
UW, UH = SL.UW, SL.UH                        # web px → m (ground / upright)

MARCH: dict[str, Region] = {n: r for n, r, _ in MP.family_regions(FAMILY)}
# A5's appended swatches (the registry is append-only; A1's colour script does not list them, so
# ``march_palettes --check`` does not see them). Art review of the A1 values on these bodies, and darkgate:
# * ``clot.tar`` = ``clot.shell``'s #2A0E14 with no light band (A1's ``tar`` preset: lightAmt .45). Seen through the
#   .62 blend the A1 band put half the dome in a pinkish grey (≈ #654E4A, L* 36) that read as matte plastic and sat
#   at the march mud's value; a .15 band (a dark wine) still landed 3 L* from the mud (darkgate lit ΔL* 3.0). With
#   no band the tar reads by negative contrast — L* ≈ 8, 14 below the mud, 23 below the ash grass — and the dome is
#   modelled by the violet fresnel rim and the wet glints, as a black gloss should be.
# * ``clot.gloss`` #D8CCE8 (flat): the wet highlight, a pale violet-white (no A1 swatch is a cool highlight that is
#   not emissive; an emissive one would bloom into a fourth spark).
# * ``bloat.char`` = ``bloat.crust``'s #3A1A12 with lightAmt .30 (A1: .36): at .36 on the smooth first dome the lit
#   half read as a bread roll; with the plates, cracks and violet rim .30 reads as fired crust and clears the mud by
#   19 L* (at .20 the lit plates sat 11 from the mud and 2 from the ash grass).
# * ``clot.depth`` #1D0B1A, emissive 1 (unshaded): the backing bowl, the tar's depth seen through the shell. Unshaded
#   so no light band or rim ever lands on it — lit (``clot.inner``), its steep walls caught the kit's warm rim at
#   low pitches and, in the squash / stretch poses, even with custom normals pointed down (Blender keeps custom
#   normals in each face's frame and turned them with the stretched walls). Its value renders exactly as
#   ``clot.inner``'s shade tone #261022 (lin × 1.6 = the toon's emissive boost); bloom source 0.02, nothing glows.
#
# Art review 2 (darkgate on A10's real march terrain, dirt L*50 19.5; the ash grass L* 32.3) appended four more —
# ``clot.tar`` / ``clot.depth`` / ``bloat.char`` stay registered (append-only) but no mesh samples them any more:
# * ``clot.pitch`` #160609 (no light band) + ``clot.pitchdepth`` #110509 (emissive 1, the backing bowl): the tar
#   pushed almost to black. At ``clot.tar``'s L* 8 the lit side sat 11.4 below the dirt (gate 15) and the front half
#   of the dome read dark plum on dark brown, held apart only by the ink; at L* ≈ 3 it reads black on brown, by
#   negative contrast (signed ΔL* ≤ −15 on the dirt), still a crimson-black where the violet rim meets it. A light band
#   (A1's .45, or any lightAmt) puts the warm-light mix on it — the matte pinkish plastic the first review rejected.
# * ``bloat.ashchar`` #2E2C34 (s .52, l .38): the crust as **ash-dusted char**. ``bloat.char``'s lit band (#755B4D,
#   a warm rose-taupe L* 41 over half the body, a soft-looking terminator) read as a bread roll / truffle / fleshy
#   sac and sat 10 L* above the ash grass. The palette formula lights every swatch toward the warm cream
#   (light = mix(c, #FFF4D6, l)), so no lightAmt of a red-brown char base lands on a neutral ash; a near-black
#   charcoal base with a cool cast does: light band #7D7872 (L* 50.7, C* 4.3 — clears the dirt by 31, the grass by
#   18), base band #2E2C34 (L* 18.5, charcoal), shade #13111A (near-black) — the crisp toon terminator between ash and
#   char. ``bloat.charcrust`` (#3A1A12, l .12) and ``bloat.ashcrust`` (#46454A) were registered by the first try of
#   this pass — char on the flanks and the seams, ash on the up-facing faces, split per face — whose 7 cm faces drew
#   the ash / char boundary as stair-steps; registered, unused.
A5_SWATCHES: dict[str, Region] = {
    "clot.tar": Region("#2A0E14", .45, 0.0, 0.0, MP.O_MON),
    "clot.gloss": Region("#D8CCE8", 0.0, 0.0, 0.0, MP.O_MON),
    "bloat.char": Region("#3A1A12", .52, .30, 0.0, MP.O_MON),
    "clot.depth": Region("#1D0B1A", 0.0, 0.0, 1.0, MP.O_MON),
    "clot.pitch": Region("#160609", .45, 0.0, 0.0, MP.O_MON),
    "clot.pitchdepth": Region("#110509", 0.0, 0.0, 1.0, MP.O_MON),
    "bloat.charcrust": Region("#3A1A12", .52, .12, 0.0, MP.O_MON),
    "bloat.ashcrust": Region("#46454A", .50, .30, 0.0, MP.O_MON),
    "bloat.ashchar": Region("#2E2C34", .52, .38, 0.0, MP.O_MON),
}
SWATCHES: dict[str, Region] = {**MARCH, **A5_SWATCHES}

CLOT_ALIAS = dict(shell="clot.pitch", inner="clot.pitchdepth", helm="clot.helm", bolt="clot.bolt",
                  spark="clot.marker", sheen="clot.gloss")
BLOAT_ALIAS = dict(crust="bloat.ashchar", crack="bloat.crack", fissure="bloat.marker")


@dataclass
class Kind:
    key: str
    asset: str
    token: str
    game_id: str
    preview_dir: str
    k: float                                 # scale against the slime's dome
    display: dict
    alias: dict
    ref_speed: float                         # m/s of the Walk clip (play rate = v / ref)
    lump: float                              # dome lump amplitude (fraction of the radius)
    sides: int
    marker: tuple
    lod1_ratio: float
    lod1_skip: tuple
    material: str = ""                       # slot-2 material instance (clot: the blended shell; bloat: the crust)
    tall: float = 1.0                        # height × this over the slime's proportions (the bloat swells up)

    @property
    def H(self) -> float:
        return H0 * self.k * self.tall

    @property
    def W(self) -> float:
        return W0 * self.k


CLOT = Kind("clot", "SK_Mon_Breachclot", "Mon_Breachclot", "monster_breachclot", "mon_breachclot", 1.0,
            {"zh-CN": "渊血凝块", "en": "Breachclot"}, CLOT_ALIAS, 1.20, 0.05, 36, ("clot.marker",), 0.45,
            ("bolt", "sheen"), material="MI_AF_Toon_Slime_Breachclot")
BLOAT = Kind("bloat", "SK_Mon_Cinderbloat", "Mon_Cinderbloat", "monster_cinderbloat", "mon_cinderbloat", 1.35,
             {"zh-CN": "烬胀囊", "en": "Cinderbloat"}, BLOAT_ALIAS, 0.90, 0.05, 48, ("bloat.marker",), 1.0,
             ("branch", "ring_fissure"),
             material="MI_AF_Toon_Cinderbloat_Crust", tall=1.12)
BLOAT_LOD1_SIDES = 24                        # LOD1: the crust rebuilt at 24 columns, no extra rows and no grooves,
#   nothing decimated (decimating the crust raised it over the crack / fissure strips in places: dashed fissures)
KINDS = {"clot": CLOT, "bloat": BLOAT}

BODY_OPACITY = 0.62                          # monsters.md 2.1 (R12 blend, as the slime)
RIM_ALPHA = 1.00                             # the clot shell's violet fresnel band (the kit's warm rim is .55; art review 2:
#   .70 → 1.0: the band composites at L* ≈ 38 through the .62 shell instead of 27, so the lit rim clears the dirt by
#   18 as well — 91 % of the lit side clears it instead of 73 %; a crisp violet crescent on the key-lit side)
BLOAT_RIM_ALPHA = 0.65                       # … the bloat crust's (slot 2, M_AF_Toon RimOverride)


# ── palette ──────────────────────────────────────────────────────────────────────────────────────────────
class MarchBuilder(M.Builder):
    """``kit.mesh.Builder`` whose region keys are aliases of registered ``Monsters_March`` swatches (the pattern
    of ``living.MarchBuilder`` / ``gnawers.MarchBuilder``): ``alias`` maps the keys (``shell``, ``helm`` …) onto
    swatch names (``clot.shell`` …) registered with their A1 values (``Palette.add``: same name and value → the
    same index), so no build appends or moves a swatch."""

    def __init__(self, asset: str, palette: Palette, alias: dict, prefix: str, **kw):
        super().__init__(asset, palette, prefix=prefix, **kw)
        self.alias = dict(alias)
        for key, name in self.alias.items():
            if name not in SWATCHES:
                raise KeyError(f"{asset}: region {key!r} → {name!r} is not a {FAMILY} swatch (A1 or A5_SWATCHES)")

    def region(self, key: str, region: Region | None = None) -> int:
        name = self.alias[key]
        idx = self.palette.add(name, SWATCHES[name])
        self.swatch[key] = idx
        return idx

    def declare(self) -> None:
        for key in self.alias:
            self.region(key)


def march_stage() -> None:
    """Review stage on the march ash grass (every ``kit.review.Review`` built afterwards; the kit is not edited —
    the module's ``stage`` is wrapped with another default ground, as ``gnawers.march_stage``)."""
    from kit import review
    if getattr(review.stage, "_af_march", False):
        return
    orig = review.stage

    def stage(radius: float = 30.0, blob_radius: float = 0.39, ground_hex: str = STAGE_HEX, grid: bool = True):
        return orig(radius, blob_radius, STAGE_HEX, grid)
    stage._af_march = True
    review.stage = stage


# ── the dome ─────────────────────────────────────────────────────────────────────────────────────────────
def _sstep(x: float, e0: float, e1: float) -> float:
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


class Dome:
    """The breach-blood body surface: the slime's lathe profile (``slime.radius_at``) × ``k`` with its radius
    modulated by low lumps (``lump``: fraction of the radius; zero on the sole and the overhanging bulge so the
    slime's ink-safe base contour stays). Parameters as ``mgeo`` surfaces: ``u`` = height fraction 0..1 of
    ``H0 · k``, ``a`` = azimuth in degrees (0 = front −Y, +90 = the monster's left +X)."""

    def __init__(self, k: float, lump: float, phase: float = 0.0, tall: float = 1.0, grooves=(),
                 groove_depth: float = 0.0, groove_w: float = 0.04):
        self.k, self.lump, self.phase, self.tall = k, lump, phase, tall
        self.Hk = H0 * k * tall
        # crack grooves: polylines in (u, a) sampled densely; the radius dips by ``groove_depth`` (fraction) along
        # them with a Gaussian falloff of ``groove_w`` m (the crust's plates bulge between the cracks)
        self.groove_depth, self.groove_w = groove_depth, groove_w
        self._gpts = []
        if grooves and groove_depth > 0:
            import common as C
            for path in grooves:
                us = [q[0] for q in path]
                as_ = [q[1] for q in path]
                ts = [i / (len(path) - 1) for i in range(len(path))]
                n = 6 * (len(path) - 1)
                self._gpts.extend((C._hermite(ts, us, j / n), C._hermite(ts, as_, j / n)) for j in range(n + 1))
        self._gu = np.array([q[0] for q in self._gpts]) if self._gpts else None
        self._ga = np.array([q[1] for q in self._gpts]) if self._gpts else None

    def groove(self, a: float, u: float) -> float:
        if self._gu is None:
            return 0.0
        r0 = SL.radius_at(u * H0) * self.k
        da = (self._ga - a + 180.0) % 360.0 - 180.0
        d2 = ((self._gu - u) * self.Hk) ** 2 + (np.radians(da) * max(r0, 0.05)) ** 2
        return self.groove_depth * float(np.exp(-d2.min() / (self.groove_w ** 2))) * _sstep(u, 0.18, 0.32)

    def lumps(self, a: float, u: float) -> float:
        if self.lump <= 0.0:
            return 0.0
        ar = math.radians(a)
        ph = self.phase
        v = (0.50 * math.sin(3 * ar + 0.7 + 2.1 * u + ph) + 0.35 * math.sin(5 * ar - 1.3 - 3.3 * u)
             + 0.25 * math.sin(2 * ar + 2.2 + ph) * math.cos(4.1 * u)
             + 0.30 * math.sin(7 * ar + 1.9 + 5.1 * u) * math.sin(2.2 * math.pi * u))
        return self.lump * _sstep(u, 0.30, 0.55) * v

    def radius(self, u: float, a: float) -> float:
        return SL.radius_at(u * H0) * self.k * (1.0 + self.lumps(a, u) - self.groove(a, u))

    def _p(self, u: float, a: float) -> Vector:
        r = self.radius(u, a)
        ar = math.radians(a)
        return Vector((r * math.sin(ar), -r * math.cos(ar), u * self.Hk))

    def normal(self, u: float, a: float) -> Vector:
        u = min(u, 0.985)
        du, da = 0.004, 1.5
        pu = self._p(min(u + du, 0.999), a) - self._p(max(u - du, 0.0), a)
        pa = self._p(u, a + da) - self._p(u, a - da)
        n = pa.cross(pu)
        c = self._p(u, a)
        if n.dot(Vector((c.x, c.y, 0.0))) + n.z * 0.2 < 0:
            n = -n
        return n.normalized() if n.length > 1e-9 else Vector((0, 0, 1))

    def point(self, u: float, a: float, out: float = 0.0) -> Vector:
        p = self._p(u, a)
        return p + self.normal(u, a) * out if out else p

    def u_at_radius(self, r: float) -> float:
        """Height fraction where the (unlumped) profile has radius ``r`` on the upper dome (bisection)."""
        lo, hi = 0.3, 1.0
        for _ in range(40):
            mid = 0.5 * (lo + hi)
            if SL.radius_at(mid * H0) * self.k > r:
                lo = mid
            else:
                hi = mid
        return 0.5 * (lo + hi)

    def bowl_mesh(self, sides: int, inset: float, top: float, inset_low: float = 0.006) -> M.Part:
        """The inner backing bowl (the slime's R12 trick on this dome): a floor ≈ 6 mm up, then the shell's own
        (lumped) profile ``inset`` inside it up to height ``top`` (m at k = 1), open at the top, every face turned up /
        inward. The steep near wall faces away from the W1 camera: UE culls it, the preview's two-sided toon draws it
        — unshaded (``clot.pitchdepth``, emissive) both draw the same flat tar (no light band, no Fresnel rim).
        Low down (z ≤ 6 cm, the overhanging flare) the bowl hugs the shell ``inset_low`` inside it: with the 2 cm
        inset the rays through the near flare reached the ground under the bowl's wall (a pale sliver along the
        lower contour of the stretched poses and the puddle)."""
        zs = [z for z in (0.006, 0.02, 0.04, 0.06, 0.09, 0.12, 0.15, 0.18, 0.205, 0.235, 0.26, 0.28, 0.30)
              if z <= top + 1e-6]
        bm = bmesh.new()
        c = bm.verts.new((0.0, 0.0, zs[0] * self.k))
        rings = []
        for z in zs:
            u = z / H0
            ring = []
            for j in range(sides):
                a = 360.0 * j / sides
                ins = inset_low + (inset - inset_low) * _sstep(z, 0.06, 0.12)
                r = max(self.radius(u, a) - ins * self.k, 0.02)
                ar = math.radians(a)
                ring.append(bm.verts.new((r * math.sin(ar), -r * math.cos(ar), z * self.k)))
            rings.append(ring)
        for j in range(sides):
            bm.faces.new((c, rings[0][j], rings[0][(j + 1) % sides]))
        for r0, r1 in zip(rings, rings[1:]):
            for j in range(sides):
                jj = (j + 1) % sides
                bm.faces.new((r0[j], r0[jj], r1[jj], r1[j]))
        part = M.Part(bm).smooth(True)
        SL._face_up(part)
        return part

    def mesh(self, sides: int, extra_rows: bool = False) -> M.Part:
        """Closed shell: the slime profile's rows (bottom sole fan … apex fan) on ``sides`` columns
        (``extra_rows``: a row between each pair above the bulge, for the crust's grooves)."""
        us = [z / H0 for _, z in SL.PROFILE]
        if extra_rows:
            us = sorted(set(us) | {0.5 * (a + b) for a, b in zip(us, us[1:]) if 0.25 <= a and b <= 0.97})
        bm = bmesh.new()
        rings = []
        for u in us:
            rings.append([bm.verts.new(tuple(self._p(u, 360.0 * j / sides))) for j in range(sides)])
        for r0, r1 in zip(rings, rings[1:]):
            for j in range(sides):
                jj = (j + 1) % sides
                bm.faces.new((r0[j], r0[jj], r1[jj], r1[j]))
        bot = bm.verts.new((0.0, 0.0, 0.0))
        for j in range(sides):
            bm.faces.new((rings[0][(j + 1) % sides], rings[0][j], bot))
        top = bm.verts.new((0.0, 0.0, self.Hk))
        for j in range(sides):
            bm.faces.new((rings[-1][j], rings[-1][(j + 1) % sides], top))
        bm.normal_update()
        bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
        return M.Part(bm).smooth(True)


def shell_bind(k: float, tall: float = 1.0) -> Bind:
    """The slime's body weights (``slime.shell_bind``) at scale ``k``: base / mid / top by height (smooth bells)
    and the pseudopod on the front-bottom quarter."""
    names = ("body_base", "body_mid", "body_top", "pod_01", "pod_02")
    Hk = H0 * k * tall

    def sm(x, e0, e1):
        t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
        return t * t * (3 - 2 * t)

    def fn(co):
        x, y, z = co[:, 0], co[:, 1], co[:, 2]
        kz = np.clip(z / Hk, 0, 1)
        wb = 1.0 - sm(kz, 0.05, 0.42)
        wt = sm(kz, 0.42, 0.85)
        wm = np.clip(1.0 - wb - wt, 0, 1)
        r = np.hypot(x, y)
        cosf = np.where(r > 1e-6, -y / np.maximum(r, 1e-6), 0.0)
        front = np.clip((cosf - 0.25) / 0.75, 0, 1) ** 1.5
        low = 1.0 - sm(kz, 0.0, 0.42)
        pod = front * low * 0.95
        outer = sm(r, 0.28 * k, 0.42 * k)
        w = np.stack([wb * (1 - pod), wm * (1 - pod), wt * (1 - pod), pod * (1 - outer), pod * outer], axis=1)
        return w + 1e-6
    return Bind.custom(names, fn)


def dome_strip(surf: Dome, path, width: float, lift: float, steps: int = 8, taper: float = 0.0,
               taper0: float = 0.0) -> M.Part:
    """``mgeo.surface_paint`` with both ends tapered (``taper0`` the start, ``taper`` the end): cracks and fissures
    that open from a hairline and close to one."""
    us = [p[0] for p in path]
    as_ = [p[1] for p in path]
    import common as C
    ts = [i / (len(path) - 1) for i in range(len(path))]
    cen = [(C._hermite(ts, us, j / steps), C._hermite(ts, as_, j / steps)) for j in range(steps + 1)]
    pts = [surf.point(u, a) for u, a in cen]
    bm = bmesh.new()
    rows = []
    for j, (u, a) in enumerate(cen):
        t = j / steps
        tg = (pts[min(j + 1, steps)] - pts[max(j - 1, 0)]).normalized()
        n = surf.normal(u, a)
        w = width * 0.5 * (1.0 - taper * t) * (1.0 - taper0 * (1.0 - t))
        side = tg.cross(n).normalized() * max(w, width * 0.05)
        p = pts[j] + n * lift
        rows.append((bm.verts.new(tuple(p - side)), bm.verts.new(tuple(p + side)), n))
    for (a0, a1, n0), (b0, b1, _) in zip(rows, rows[1:]):
        f = bm.faces.new((a0, b0, b1, a1))
        f.normal_update()
        if f.normal.dot(n0) < 0:
            f.normal_flip()
    bm.normal_update()
    return M.Part(bm).smooth(True)


# ── rig ──────────────────────────────────────────────────────────────────────────────────────────────────
# clot inner items (body space, k = 1). Art review 2: seen from the W1 pitch an upright helm was a grey disc with
# concentric brim / crown rings and a spark over its crown in most se Idle frames — one grey eyeball with a violet
# pupil. The helm now lies **on its side** (crown axis tipped 60° toward the monster's left, +X), centred in the
# dome's screen footprint, so the brim edge and the crown's dented profile are its silhouette in every front view;
# the four sparks sit round it, ≥ 18 cm (screen) from the crown centre in the se / sw / front views (they drift
# ± 2 cm), the bolt-heads low on the flanks. The tip is toward the back (+Y), 59° from vertical: that axis is
# square to both front 3/4 views of the W1 camera (the cross product of the se and sw view directions, 50° down),
# so se, sw and front all see the helm in profile (tipped toward a flank, one of se / sw looked down its crown).
HELM_HOME = Vector((-0.03, 0.02, 0.19))
HELM_SCALE = 1.15                           # 28 cm across the brim (art review: at 24 cm a grey pebble at the W1 camera)
HELM_TIP_DEG = 59.0                         # crown axis tipped from vertical toward the back (the helm on its side)
HELM_AXIS = Vector((0.06, math.sin(math.radians(HELM_TIP_DEG)), math.cos(math.radians(HELM_TIP_DEG)))).normalized()
HELM_SPIN = 25.0                            # deg about the crown axis before the tip (where the crumpled brim lands)
BOLTS = {"bubble_01": (Vector((0.17, -0.25, 0.10)), Vector((-0.20, 0.88, 0.42)).normalized()),
         "bubble_02": (Vector((-0.27, -0.05, 0.12)), Vector((0.25, -0.85, 0.46)).normalized())}
SPARKS = {"eye_l": Vector((0.24, 0.02, 0.20)), "eye_r": Vector((-0.16, -0.26, 0.13)),
          "xeye_l": Vector((0.04, -0.30, 0.15)), "xeye_r": Vector((-0.22, 0.17, 0.25))}
SPARK_R = 0.019                                         # 3.4 × 4.4 cm embers: ≥ 2 px blocks at the W1 camera


@dataclass
class BreachSpec:
    name: str
    k: float
    skeleton: str = SKELETON
    tall: float = 1.0                        # the bloat swells up (Kind.tall): its crown is H0 · k · tall

    def crown_height(self) -> float:
        return H0 * self.k * self.tall


def build_rig(kind: Kind) -> R.Rig:
    """``SKEL_Slime``'s 16 bones (names, hierarchy and roll as ``slime.build_rig``) at scale ``k``; the inner
    bones sit where this monster uses them (clot: helm, bolts, sparks)."""
    k = kind.k
    scene.ensure_object_mode()
    data = bpy.data.armatures.new(SKELETON)
    obj = bpy.data.objects.new(kind.asset, data)
    scene.link(obj)
    data.display_type = "STICK"
    scene.select_only([obj])
    bpy.ops.object.mode_set(mode="EDIT")
    eb = data.edit_bones
    J = {}

    def bone(name, head, tail, parent="root", x_axis=(1, 0, 0)):
        b = eb.new(name)
        b.head, b.tail = Vector(head), Vector(tail)
        if parent:
            b.parent = eb[parent]
        y = (b.tail - b.head).normalized()
        x = Vector(x_axis)
        x = (x - y * x.dot(y)).normalized()
        b.align_roll(x.cross(y))
        b.use_deform = True
        J[name] = Vector(head)
        return b

    def v(x, y, z):
        return Vector((x * k, y * k, z * k * kind.tall))
    bone("root", (0, 0, 0), v(0, 0, 0.05), parent=None)
    bone("body_base", (0, 0, 0.0), v(0, 0, 0.1))
    bone("body_mid", v(0, 0, 0.18), v(0, 0, 0.27))
    bone("body_top", v(0, 0, 0.34), v(0, 0, 0.43))
    bone("pod_01", v(0, -0.25, 0.05), v(0, -0.33, 0.05))
    bone("pod_02", v(0, -0.37, 0.04), v(0, -0.45, 0.04))
    if kind.key == "clot":
        for nm, p in SPARKS.items():
            bone(nm, p, p + Vector((0, -0.04, 0)))
        for s, sg in (("l", 1), ("r", -1)):      # unused by the clot (kept for the shared skeleton)
            lid = SL.surface_point(sg * (SL.EYE_LAT + 0.004), SL.EYE_Z + SL.LID_DZ, -0.004)
            bone(f"lid_{s}", lid, lid + Vector((0, -0.04, 0)))
        bone("core", (0, 0.04, 0.16), (0, 0.04, 0.26))
        bone("bone_prop", HELM_HOME, HELM_HOME + Vector((0, 0, 0.08)))
        for nm, (p, d) in BOLTS.items():
            bone(nm, p, p + Vector((0, 0, 0.06)))
    else:
        # six inner bones carry the burst's char shards (SHARDS), placed deep in the crust where the shards are
        # modelled; xeye_r and the lids stay unused (kept for the shared skeleton)
        homes = {sh[0]: shard_home(i) for i, sh in enumerate(SHARDS)}
        for s, sg in (("l", 1), ("r", -1)):
            e = SL.surface_point(sg * SL.EYE_LAT, SL.EYE_Z, -SL.EYE_SINK) * k
            h = homes.get(f"eye_{s}", e)
            bone(f"eye_{s}", h, h + Vector((0, 0, 0.05)))
            h = homes.get(f"xeye_{s}", e + Vector((0, -0.03, 0)))
            bone(f"xeye_{s}", h, h + Vector((0, 0, 0.05)))
            lid = SL.surface_point(sg * (SL.EYE_LAT + 0.004), SL.EYE_Z + SL.LID_DZ, -0.004) * k
            bone(f"lid_{s}", lid, lid + Vector((0, -0.04, 0)))
        bone("core", (0, 0, 0), v(0, 0, 0.1))  # the fuse ridges scale about the ground centre (see BloatRidges)
        for nm in ("bone_prop", "bubble_01", "bubble_02"):
            bone(nm, homes[nm], homes[nm] + Vector((0, 0, 0.05)))
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in obj.pose.bones:
        pb.rotation_mode = "QUATERNION"
    obj["af_skeleton"] = SKELETON
    H = kind.H
    sockets = [
        {"name": "fx_feet", "bone": "root", "pos": Vector((0, 0, 0))},
        {"name": "fx_chest", "bone": "body_mid", "pos": Vector((0, 0, 0.55 * H))},
        {"name": "fx_head", "bone": "body_top", "pos": Vector((0, 0, 0.8 * H))},
        {"name": "fx_overhead", "bone": "body_top", "pos": Vector((0, 0, H + 0.25))},
        {"name": "fx_pod", "bone": "pod_02", "pos": J["pod_02"]},
    ]
    if kind.key == "clot":
        sockets.append({"name": "fx_core", "bone": "core", "pos": J["core"] + Vector((0, 0, 0.03))})
    else:
        sockets.append({"name": "fx_vent", "bone": "body_top", "pos": Vector((0, 0, H + 0.01))})
    return R.Rig(obj, BreachSpec(kind.asset, k, tall=kind.tall), J, sockets)


# ── the clot's inner items ───────────────────────────────────────────────────────────────────────────────
HELM_CROWN = [(0.080, 0.0), (0.083, 0.010), (0.081, 0.024), (0.072, 0.038), (0.054, 0.050), (0.028, 0.057),
              (0.008, 0.059)]
HELM_BRIM = (0.074, 0.006, 0.122, -0.012, 0.0065)      # r_in, z_in, r_out, z_out, thickness


def ring_band(r_in: float, z_in: float, r_out: float, z_out: float, t: float, sides: int,
              lift_fn=None, rout_fn=None) -> M.Part:
    """A closed conical washer (a helm brim): top, bottom and outer edge; ``lift_fn(az_rad) → dz`` bends the
    outer edge (a crumpled brim), ``rout_fn(az_rad) → dr`` cuts it (a torn notch)."""
    bm = bmesh.new()
    rows = []
    for j in range(sides):
        a = 2 * math.pi * j / sides
        c, s = math.cos(a), math.sin(a)
        dz = lift_fn(a) if lift_fn else 0.0
        ro = r_out + (rout_fn(a) if rout_fn else 0.0)
        rows.append((bm.verts.new((r_in * c, r_in * s, z_in)), bm.verts.new((ro * c, ro * s, z_out + dz)),
                     bm.verts.new((ro * c, ro * s, z_out + dz - t)), bm.verts.new((r_in * c, r_in * s, z_in - t))))
    for j in range(sides):
        a0, a1 = rows[j], rows[(j + 1) % sides]
        for i in range(4):
            ii = (i + 1) % 4
            bm.faces.new((a0[i], a1[i], a1[ii], a0[ii]))
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    return M.Part(bm).auto_sharp(50)


def _helm_rot() -> Matrix:
    """3 × 3 rest rotation of the helm: spun ``HELM_SPIN`` about its crown axis, then tipped onto ``HELM_AXIS``."""
    q = Vector((0, 0, 1)).rotation_difference(HELM_AXIS)
    return q.to_matrix() @ Matrix.Rotation(math.radians(HELM_SPIN), 3, "Z")


def helm_up_az() -> float:
    """The helm's local azimuth (rad, about its crown axis) that faces world up once it lies on its side: the dent
    and the brim notch go there, so the W1 camera (50° down) sees both."""
    lu = _helm_rot().inverted() @ Vector((0, 0, 1))
    return math.atan2(lu.y, lu.x)


HELM_DENT = (0.040, 0.018, 0.034)           # dent centre height on the crown, depth, radius (m, before × HELM_SCALE)
HELM_NOTCH = (0.032, 0.30)                  # torn brim notch: depth (m) and half-width (rad)
HELM_DENT_OFF = -0.95                       # the dent's azimuth off the comb (rad): beside it, on the up flank
HELM_COMB = (0.010, 0.010)                  # the medial comb over the crown: height, half-width (m, before × HELM_SCALE)


def kettle_helm(sides: int = 20) -> M.Part:
    """A dented chapel-de-fer: a low round crown and a broad drooping brim bent up on one side (local: crown axis
    +Z, origin at the brim root). 24 cm across (the clot scales it × HELM_SCALE) — the crusade's foot soldiers wore them
    (monsters.md §2.1). Art review 2: the helm lies on its side, so its up-facing flank (``helm_up_az``) carries a
    raised **medial comb** (``HELM_COMB``, on one lathe column: a crisp crease from brim to brim over the apex — the
    crest of the crown's profile in se / sw / front, and in the back views, which look down the crown, a lit / shade
    split across the disc: a helmet's top, not a grey eyeball with a catchlight), a **flattened dent** beside it (a
    slightly concave facet: a crisp shading plane at the W1 camera, a sword or a cart wheel) and the brim a **torn V
    notch** that breaks the brim's edge in silhouette."""
    crown = M.lathe(HELM_CROWN, sides=sides, cap0="fan", cap1="fan")
    comb_az = round(helm_up_az() / (2 * math.pi / sides)) * (2 * math.pi / sides)   # on a lathe column: one crease
    az = comb_az + HELM_DENT_OFF
    zc, depth, rad = HELM_DENT
    rc = float(np.interp(zc, [p[1] for p in HELM_CROWN], [p[0] for p in HELM_CROWN]))
    c_out = Vector((math.cos(az) * rc, math.sin(az) * rc, zc))
    n_c = Vector((math.cos(az), math.sin(az), 0.35)).normalized()
    plane = c_out - n_c * depth

    def dent(co: Vector) -> Vector:
        lat = (co - c_out) - n_c * (co - c_out).dot(n_c)
        w = _sstep(1.0 - lat.length / rad, 0.0, 0.45)
        if w <= 0.0:
            return co
        s = (co - plane).dot(n_c)
        cup = 0.006 * max(0.0, 1.0 - (lat.length / rad) ** 2)
        return co - n_c * (max(s, 0.0) * w + cup * w)
    crown.deform(dent)
    ch, cw = HELM_COMB
    cdir = Vector((math.cos(comb_az), math.sin(comb_az), 0.0))
    zmax = HELM_CROWN[-1][1]

    def comb(co: Vector) -> Vector:
        d = abs(co.x * -cdir.y + co.y * cdir.x)                     # distance from the comb's plane
        # full on the flanks, half at the apex (the helm settles brim-down in the melted puddle: Death's last frames)
        w = math.exp(-(d / cw) ** 2) * _sstep(co.z, 0.004, 0.022) * (1.0 - 0.5 * _sstep(co.z, 0.040, zmax))
        if w < 1e-3:
            return co
        out = Vector((co.x, co.y, 0.0))
        out = (out.normalized() if out.length > 1e-6 else Vector()) * (1.0 - co.z / zmax) + Vector((0, 0, 1)) * (
            co.z / zmax)
        return co + out.normalized() * ch * w
    crown.deform(comb)
    rb = HELM_BRIM

    def crumple(a: float) -> float:
        da = math.atan2(math.sin(a - math.radians(-120)), math.cos(a - math.radians(-120)))
        return 0.016 * math.exp(-(da / 0.5) ** 2)

    def notch(a: float) -> float:
        da = abs(math.atan2(math.sin(a - comb_az - 0.55), math.cos(a - comb_az - 0.55)))
        return -HELM_NOTCH[0] * max(0.0, 1.0 - da / HELM_NOTCH[1])
    brim = ring_band(rb[0], rb[1], rb[2], rb[3], rb[4], sides, crumple, notch)
    crown.merge(brim)
    return crown


def helm_tilt() -> Matrix:
    """Rest orientation of the suspended helm (crown axis ``HELM_AXIS``: on its side) about its home."""
    return Matrix.Translation(HELM_HOME) @ _helm_rot().to_4x4()


BOLT_HEAD = (0.056, 0.019, 0.0095)           # m: head length, half-width, half-thickness (a 3.8 cm broad wedge)


def bolt_head() -> M.Part:
    """A crossbow quarrel's iron head on its socket and a snapped shaft stub, along +Y (tip at +Y), ≈ 10 cm.
    Art review 2: the 1.9 cm needle head was a 1 px grey hairline at 1080p — now a short broad wedge (3.2 cm wide,
    flat: its broad face up, local +Z, ≈ 4 px at the W1 camera) on a thick socket and a short stub."""
    L, hw, ht = BOLT_HEAD
    head = M.sweep([(0, 0.0, 0), (0, 0.45 * L, 0), (0, L, 0)], M.superellipse(1, 1, 4, 1.0, 0.0),
                   scales=[(ht, hw), (ht * 0.85, hw * 0.95), (0.0008, 0.0012)], up=(0, 0, 1), dome1=0.0)
    sock = M.tube([(0, -0.020, 0), (0, 0.002, 0)], [0.0068, 0.0085], sides=6, dome0=0.0, dome1=0.0)
    stub = M.tube([(0, -0.048, 0), (0, -0.019, 0)], [0.0055, 0.0060], sides=5, dome0=0.0, dome1=0.0)
    head.merge(sock)
    head.merge(stub)
    return head.auto_sharp(40)


def bolt_frame(nm: str) -> Matrix:
    p, d = BOLTS[nm]
    return G.frame_z(p, d, Vector((0, 0, 1))) @ Matrix.Rotation(math.radians(-90), 4, "X")


def spark() -> M.Part:
    return M.ellipsoid((SPARK_R, SPARK_R, SPARK_R * 1.3), (0, 0, 0), sides=6, rings=4)


# ── the bloat's crust pattern (u = height fraction, a = azimuth deg) ────────────────────────────────────
VENT_R, VENT_RING_R = 0.0425, 0.063         # (× k) the violet vent at the crown and the dark torn rim round it
# Art review 2: the vent was a symmetric five-point magenta star — a cute star badge at 1080p that also clashed with
# the R8 stun stars (SM_FX_StunStar). Its first replacement (an elongated slit, a violet inner lip ≥ 5 mm all round,
# dark lips round both) bloomed into a closed violet ring inside a dark almond at 1080p: an eye. Now a **torn Y**
# (``_vent_r``): a dark core split into three uneven arms (VENT_ARMS: azimuth deg, reach, half-width deg; the long one
# 6.7 cm), four uneven dark lips torn outward round it (VENT_LIPS: azimuth deg, reach × VENT_RING_R, half-width deg; the
# long ones on the arms), and breach-violet only as a **sliver on the inner lip** of the long arm (``vent_lip_strip``:
# along one flank, VENT_LIP_SPAN of the arm, tapered at both ends) — the glow deep in the tear catching one torn wall.
# At 1080p it reads as a short violet seam in a dark rupture, like the fissures, never a ring, a star or a pupil.
VENT_CORE = 0.22                              # × VENT_RING_R: the core's radius between the arms
VENT_ARMS = [(197.0, 2.60, 15.0), (57.0, 1.20, 16.0), (-15.0, 0.70, 19.0)]
VENT_LIPS = [(197.0, 0.75, 11.0), (57.0, 0.55, 15.0), (-15.0, 0.35, 18.0), (275.0, 0.25, 26.0)]
VENT_LIP_SPAN, VENT_LIP_W, VENT_LIP_SIDE = (0.10, 0.95), 0.017, -1    # the violet sliver: fraction of the long arm's
#   reach, width (m × k), the flank (−1: toward lower azimuth)
PRIMARY = [   # five cracks from the vent's rim down the sides; the violet fissures glow in their upper part
    [(0.86, -15), (0.80, -9), (0.68, -18), (0.56, -11), (0.44, -19), (0.33, -13)],
    [(0.86, 57), (0.80, 63), (0.68, 54), (0.55, 61), (0.43, 52), (0.31, 58)],
    [(0.86, 127), (0.79, 119), (0.66, 128), (0.53, 120), (0.40, 127)],
    [(0.86, 197), (0.80, 203), (0.67, 193), (0.53, 201), (0.41, 194)],
    [(0.86, 270), (0.80, 263), (0.67, 272), (0.54, 265), (0.42, 271), (0.31, 264)],
]
BRANCHES = [
    [(0.66, -17), (0.55, -38), (0.44, -52), (0.35, -58)],
    [(0.50, -13), (0.42, 6), (0.33, 19)],
    [(0.64, 55), (0.52, 80), (0.41, 93)],
    [(0.61, 125), (0.49, 149), (0.39, 160)],
    [(0.63, 195), (0.51, 222), (0.41, 236)],
    [(0.63, 270), (0.50, 296), (0.40, 309)],
]
RING = [   # secondary cracks between the primaries: they cut the crust into plates
    [(0.66, -12), (0.62, 10), (0.66, 33), (0.61, 56)],
    [(0.47, 59), (0.44, 90), (0.48, 126)],
    [(0.64, 127), (0.67, 161), (0.62, 196)],
    [(0.45, 198), (0.48, 234), (0.43, 269)],
    [(0.62, 268), (0.66, 298), (0.61, 326), (0.66, 345)],
]
RING_FISSURES = (0, 2, 4)                    # the upper ring cracks glow too: a violet web round the crown
FISSURE_TO = 0.38                            # fissures glow in the primaries from the vent down to u = this
CRACK_W, FISSURE_W = 0.038, 0.024            # m at k = 1 (× k)
# the crust's plates: the radius dips GROOVE (fraction) along every crack, Gaussian falloff GROOVE_W m (× k) —
# the plates bulge between the cracks and the toon bands break up plate by plate (a smooth dome read as a bun)
GROOVE, GROOVE_W = 0.10, 0.028
RIDGE_W, RIDGE_PEAK, RIDGE_FOOT = 0.022, -0.007, -0.019   # m (× k): the fuse wedge under the crust at rest
RIDGE_RISE = 0.08                           # fuse: the ridge bone scales 1 + this about the ground centre, the
#   wedges break out of the grooves as glowing seams (emissive: they need no ink of their own; the ship's ink gate
#   leaves edges at bright emissive accents out, and the hull is not padded round the risen state)
RIDGE_HIDE = 0.55                           # … and to this when no fuse is lit (deep inside the crust)


def _primary_fissure(path):
    return [(u, a) for u, a in path if u >= FISSURE_TO - 1e-6]


def _vent_dang(th: float, az: float) -> float:
    return math.atan2(math.sin(th - math.radians(az)), math.cos(th - math.radians(az)))


def _vent_r(th: float, which: str) -> float:
    """Radius (× ``VENT_RING_R``) of the torn vent's boundaries at azimuth ``th`` (rad): ``core`` the dark tear's
    edge (``VENT_CORE`` + the three ``VENT_ARMS`` + a jagged edge), ``lip`` the dark torn lips' outer edge
    (``VENT_LIPS`` + a jagged edge)."""
    arms = sum(amp * math.exp(-(math.degrees(_vent_dang(th, az)) / hw) ** 2) for az, amp, hw in VENT_ARMS)
    core = VENT_CORE * (1.0 + arms) * (1.0 + 0.10 * math.sin(5 * th + 0.3) + 0.05 * math.sin(8 * th + 1.9))
    if which == "core":
        return core
    lips = sum(amp * math.exp(-(math.degrees(_vent_dang(th, az)) / hw) ** 2) for az, amp, hw in VENT_LIPS)
    return core + (0.24 + lips) * (1.0 + 0.10 * math.sin(7 * th + 0.7) + 0.05 * math.sin(11 * th + 2.3))


def vent_band(surf: Dome, k: float, inner: str | None, outer: str, sides: int = 36) -> M.Part:
    """One band of the torn crown vent on the crust (u from ``Dome.u_at_radius``): the annulus between the
    boundaries ``inner`` and ``outer`` of ``_vent_r`` (× ``VENT_RING_R`` × k), or the fan inside ``outer`` when
    ``inner`` is None. Faces turned up; draped by the caller."""
    R = VENT_RING_R * k
    bm = bmesh.new()
    ths = [2 * math.pi * j / sides for j in range(sides)]

    def ring(which):
        return [bm.verts.new(tuple(surf.point(surf.u_at_radius(R * _vent_r(th, which)), math.degrees(th))))
                for th in ths]
    ro = ring(outer)
    m = len(ths)
    if inner is None:
        c = bm.verts.new(tuple(surf.point(1.0, 0.0)))
        for j in range(m):
            bm.faces.new((c, ro[j], ro[(j + 1) % m]))
    else:
        ri = ring(inner)
        for j in range(m):
            jj = (j + 1) % m
            bm.faces.new((ri[j], ro[j], ro[jj], ri[jj]))
    bm.normal_update()
    for f in bm.faces:
        if f.normal.z < 0:
            f.normal_flip()
    bm.normal_update()
    return M.Part(bm).smooth(True)


def vent_lip_path(k: float, n: int = 7) -> list:
    """(r × ``VENT_RING_R``, azimuth deg) points of the violet sliver: along one flank (``VENT_LIP_SIDE``) of the
    vent's long arm (``VENT_ARMS[0]``), ``VENT_LIP_SPAN`` of its reach, inset half its width inside the core's edge."""
    az0, amp, hw = VENT_ARMS[0]
    R = VENT_RING_R * k
    inset = 0.5 * VENT_LIP_W * k / R
    r_tip = VENT_CORE * (1.0 + amp)
    pts = []
    for i in range(n):
        r = VENT_CORE + (r_tip - VENT_CORE) * (VENT_LIP_SPAN[0] + (VENT_LIP_SPAN[1] - VENT_LIP_SPAN[0]) * i / (n - 1))
        lo, hi = az0 + VENT_LIP_SIDE * 3.0 * hw, az0          # the core grows toward the arm's axis
        for _ in range(40):
            mid = 0.5 * (lo + hi)
            if _vent_r(math.radians(mid), "core") < r + inset:
                lo = mid
            else:
                hi = mid
        pts.append((r, 0.5 * (lo + hi)))
    return pts


def vent_lip_strip(surf: Dome, k: float) -> M.Part:
    """The vent's violet inner lip: a tapered strip on the crust along ``vent_lip_path`` (draped by the caller)."""
    R = VENT_RING_R * k
    path = [(surf.u_at_radius(R * r), a) for r, a in vent_lip_path(k)]
    return dome_strip(surf, path, VENT_LIP_W * k, 0.0, steps=8, taper=0.85, taper0=0.6)


# the burst's leftovers (art review 2: the crust used to splat into a 1.6 × disc that kept its lit band, glowing
# fissures and the vent — a squashed pancake). At FX_Burst the crust collapses to nothing in one frame and six low,
# dark char shards are thrown out onto the scorched puddle (``bloat.crack``, near-black in every band), on the six
# bones the bloat leaves unused. Modelled lying flat round SHARD_HOME (deep inside the crust), scaled to ~0 there in
# every pose until the burst. (bone, outline seed, target azimuth deg, target radius m, yaw deg, size m)
SHARD_HOME = Vector((0.0, 0.0, 0.18))
SHARDS = [("bone_prop", 0, 18.0, 0.40, 30.0, 0.130), ("bubble_01", 1, 84.0, 0.30, -50.0, 0.100),
          ("bubble_02", 2, 141.0, 0.47, 75.0, 0.115), ("eye_l", 3, 205.0, 0.36, 10.0, 0.135),
          ("eye_r", 4, 262.0, 0.48, -20.0, 0.095), ("xeye_l", 5, 318.0, 0.27, 55.0, 0.110)]
SHARD_T = 0.032                               # m: shard thickness


def shard_home(i: int) -> Vector:
    a = 2 * math.pi * i / len(SHARDS)
    return SHARD_HOME + Vector((0.05 * math.cos(a), 0.05 * math.sin(a), 0.0))


def shard(seed: int, size: float) -> M.Part:
    """An irregular low char slab (a 6-gon, ``SHARD_T`` thick, lying in XY) about its origin."""
    rng = scene.rng("breach.shard", seed)
    pts = []
    for i in range(6):
        th = 2 * math.pi * i / 6 + rng.uniform(-0.25, 0.25)
        r = 0.5 * size * rng.uniform(0.65, 1.0) * (1.0 if i % 2 else 0.8)
        pts.append((r * math.cos(th), r * 0.75 * math.sin(th)))
    return M.plate(pts, SHARD_T)


def ridge(surf: Dome, path, k: float, steps: int = 8) -> M.Part:
    """The fuse wedge along a fissure: a triangular ridge (peak ``RIDGE_PEAK`` below the crust, feet
    ``RIDGE_FOOT`` below, half-width ``RIDGE_W``) — scaled out by the ``core`` bone it rises through the crust,
    the exposed band widening as it goes (the crust bulging along the fissures, monsters.md 2.1)."""
    import common as C
    us = [p[0] for p in path]
    as_ = [p[1] for p in path]
    ts = [i / (len(path) - 1) for i in range(len(path))]
    cen = [(C._hermite(ts, us, j / steps), C._hermite(ts, as_, j / steps)) for j in range(steps + 1)]
    pts = [surf.point(u, a) for u, a in cen]
    bm = bmesh.new()
    rows = []
    for j, (u, a) in enumerate(cen):
        t = j / steps
        tg = (pts[min(j + 1, steps)] - pts[max(j - 1, 0)]).normalized()
        n = surf.normal(u, a)
        side = tg.cross(n).normalized()
        taper = 0.35 + 0.65 * math.sin(math.pi * min(1.0, 0.15 + 0.85 * t)) if j else 0.35
        w = RIDGE_W * k * taper
        rows.append((bm.verts.new(tuple(pts[j] - side * w + n * RIDGE_FOOT * k)),
                     bm.verts.new(tuple(pts[j] + n * RIDGE_PEAK * k)),
                     bm.verts.new(tuple(pts[j] + side * w + n * RIDGE_FOOT * k)), n))
    for (l0, c0, r0, n0), (l1, c1, r1, _) in zip(rows, rows[1:]):
        for quad in ((l0, l1, c1, c0), (c0, c1, r1, r0)):
            f = bm.faces.new(quad)
            f.normal_update()
            if f.normal.dot(n0) < 0:
                f.normal_flip()
    bm.normal_update()
    return M.Part(bm).smooth(True)


# ── materials ────────────────────────────────────────────────────────────────────────────────────────────
def shell_material(pal: Palette, name: str, opacity: float, rim_hex: str, rim_alpha: float):
    """Preview of the shell slot (parent ``M_AF_Toon_Slime``): the palette toon with the violet rim override,
    alpha-blended at ``opacity`` (R12), back faces culled as in UE. The manifest slot carries the same params."""
    base = shading.toon_material(pal)
    m = bpy.data.materials.get(name)
    if m is not None:
        bpy.data.materials.remove(m)
    m = base.copy()
    m.name = name
    nt = m.node_tree
    grp = next(n for n in nt.nodes if n.type == "GROUP")
    grp.node_tree = shading.rim_override_group(rim_hex, rim_alpha)
    em = next(n for n in nt.nodes if n.type == "EMISSION")
    out = next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL")
    tr = nt.nodes.new("ShaderNodeBsdfTransparent")
    mix = nt.nodes.new("ShaderNodeMixShader")
    mix.inputs["Fac"].default_value = opacity
    nt.links.new(tr.outputs[0], mix.inputs[1])
    nt.links.new(em.outputs[0], mix.inputs[2])
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    cull = nt.nodes.new("ShaderNodeMixShader")
    tr2 = nt.nodes.new("ShaderNodeBsdfTransparent")
    nt.links.new(geo.outputs["Backfacing"], cull.inputs["Fac"])
    nt.links.new(mix.outputs[0], cull.inputs[1])
    nt.links.new(tr2.outputs[0], cull.inputs[2])
    nt.links.new(cull.outputs[0], out.inputs["Surface"])
    m["af_parent"] = "M_AF_Toon_Slime"
    m["af_params"] = json.dumps(shell_params(opacity, rim_hex, rim_alpha))
    m["af_toon_slot"] = True
    return m


def rim_material(pal: Palette, name: str, rim_hex: str, rim_alpha: float):
    """Preview of an opaque toon slot (parent ``M_AF_Toon``) whose instance overrides the rim (``RimOverride`` 1,
    ``RimOverrideColor`` / ``RimOverrideAlpha``; the master's shadow-catcher toon carries the same banded Fresnel):
    the palette toon with the rim colour swapped, nothing else changed."""
    base = shading.toon_material(pal)
    m = bpy.data.materials.get(name)
    if m is not None:
        bpy.data.materials.remove(m)
    m = base.copy()
    m.name = name
    grp = next(n for n in m.node_tree.nodes if n.type == "GROUP")
    grp.node_tree = shading.rim_override_group(rim_hex, rim_alpha)
    m["af_parent"] = "M_AF_Toon"
    m["af_params"] = json.dumps({"RimOverride": 1.0, "RimOverrideColor": rim_param(rim_hex),
                                 "RimOverrideAlpha": rim_alpha})
    m["af_toon_slot"] = True
    return m


# Art review 2, the importer workaround (remove when the importer is fixed): ``Scripts/abyss_content/manifest.
# _param_value`` sends every "#RRGGBB" slot param through ``hex_to_linear``, but the toon HLSL declares
# ``RimOverrideColor`` sRGB-encoded (``hlsl.py`` TOON_INPUTS; ``ue/materials.py`` defaults it to sRGB floats), so a hex
# rim imported as the linear floats of its own value — #B02FD0 → (.434, .028, .632) read as sRGB ≈ #6F07A1, ~15 L*
# darker, the clot's main separator on dark ground. A ``[r, g, b]`` list passes through ``_param_value`` unchanged, so
# the breach slots carry the **sRGB-encoded floats** (hex / 255) in a list: UE then renders exactly the previews' rim.
RIM_PARAM_NOTE = ("RimOverrideColor is written as sRGB-encoded floats [r, g, b] (= hex / 255), not '#RRGGBB': the "
                  "importer (Scripts/abyss_content/manifest._param_value) converts a hex param to linear, but the toon "
                  "HLSL reads RimOverrideColor as sRGB (hlsl.py TOON_INPUTS), so a hex rim imports ~15 L* darker; a "
                  "list passes through unchanged. Workaround until the importer treats sRGB shader inputs as sRGB "
                  "(Art/blender/README.md §23) — then write the hex again")


def rim_param(rim_hex: str) -> list:
    """``RimOverrideColor`` as the importer must receive it today: sRGB-encoded floats in a list (``RIM_PARAM_NOTE``)."""
    return [round(v / 255.0, 4) for v in KC.hex_to_rgb(rim_hex)]


def shell_params(opacity: float, rim_hex: str, rim_alpha: float) -> dict:
    return {"BodyOpacity": opacity, "RimOverride": 1.0, "RimOverrideColor": rim_param(rim_hex),
            "RimOverrideAlpha": rim_alpha}


# ── bodies ───────────────────────────────────────────────────────────────────────────────────────────────
def builder(kind: Kind, pal: Palette, lod: int) -> MarchBuilder:
    if lod:
        b = MarchBuilder(kind.asset, pal, kind.alias, prefix=kind.key, lod_ratio=kind.lod1_ratio,
                         lod_skip=lambda n: any(n.startswith(s) for s in kind.lod1_skip))
    else:
        b = MarchBuilder(kind.asset, pal, kind.alias, prefix=kind.key)
    b.declare()
    return b


# the wet glint: a short bean + a dot near the specular point (the key light / W1 view half-vector) of the facings
# that look at the hero — normal elevation ≈ 61°, az −42° front, −90° se, +3° sw — on the light side of it, in the
# key light band in front / se / sw (ndl .85-.99): it reads as gloss on the black tar
# the wet highlight: two glints (u, az, half-axes, rotation about the normal) — an oval and a dot, the classic cartoon
# gloss. (A crescent inside the upper-left contour, the slime's answer, read well facing the hero but swung to the near
# side when the clot turned away: a white bar under the helm, a grin; at the true specular point it floated mid-dome
# like a worm.) Art review 2: near the crown (u .93 / .88, az −160 / −120) they sat on the helm's crown in the back
# views (ne / nw / back look down it) — a catchlight that made it a grey eyeball. Lower on the back shoulder (u .66,
# az 196 / 171) they are visible in all eight facings, clear the helm in every one (≥ 2 cm in back, ≥ 8 cm in ne / nw,
# ≥ 20 cm in se / sw / front) and sit upper-left — the key-lit side, on the violet rim — in se. (The right flank,
# az −90, cleared the back views too but touched the helm in se: one grey-white shape at 1080p.)
GLINTS = [(0.66, 196.0, 0.050, 0.026, 100.0), (0.67, 171.0, 0.016, 0.016, 0.0)]
BOWL_IN, BOWL_TOP = 0.02, 0.09            # the backing bowl: inside the shell up to 9 cm (the overhanging flare); above
#   it the rays meet the ink hull's far side (near-black), as dark as the bowl


def build_clot(pal: Palette, rig: R.Rig, lod: int = 0):
    from kit import asset
    kind = CLOT
    b = builder(kind, pal, lod)
    surf = Dome(1.0, kind.lump, phase=0.4)
    sid = b.add(surf.mesh(kind.sides), "shell", shell_bind(1.0), name="shell", hull_pad=True)
    bd = shell_bind(1.0)
    # the wet highlight (opaque, over the shell)
    for (u, a, rx, ry, rot) in GLINTS:
        c = surf.point(u, a, 0.0028)
        m = G.frame_z(c, surf.normal(u, a), Vector((0, 0, 1))) @ Matrix.Rotation(math.radians(rot), 4, "Z")
        b.add(M.ellipsoid((rx, ry, 0.004), (0, 0, 0), sides=10, rings=3).transform(m), "sheen", bd, name="sheen",
              outline=False, covered_by=sid)
    # the backing bowl (the slime's R12 trick): opaque, unshaded tar (``clot.pitchdepth``) that the .62 shell resolves
    # against, faces up / inward. Above it the rays through the dome meet the
    # far side of the ink hull (near-black) — together the body stays black-crimson with nothing pale behind it. (An
    # inward-facing inner dome on the shell slot, the first attempt, stacked two blended layers — 14 % of the ground
    # through, Cycles grain, and in UE two translucent sections of one draw in triangle order, unsorted.)
    inner_id = b.add(surf.bowl_mesh(24, BOWL_IN, BOWL_TOP), "inner", bd, name="inner", outline=False, covered_by=sid)
    # the dented kettle helm and two crossbow bolt-heads, suspended
    b.add(kettle_helm(20 if not lod else 12).transform(helm_tilt() @ Matrix.Scale(HELM_SCALE, 4)), "helm",
          Bind.rigid("bone_prop"), name="helm", outline=False)
    for nm in BOLTS:
        b.add(bolt_head().transform(bolt_frame(nm)), "bolt", Bind.rigid(nm), name="bolt", outline=False)
    for nm, p in SPARKS.items():
        b.add(spark().translate(p), "spark", Bind.rigid(nm), name="spark", outline=False)
    body = asset.finish_mesh(b, pal, OUTLINE_CLASS, rig=rig, grounding_height=H0,
                             name=f"{kind.asset}_LOD{lod}" if lod else None)
    body["af_lod"] = lod
    # the shell's toon faces → slot 2, the translucent shell (R12) with the violet rim override
    me = body.data
    fp = np.zeros(len(me.polygons), np.int32)
    me.attributes["af_part"].data.foreach_get("value", fp)
    mi = np.zeros(len(me.polygons), np.int32)
    me.polygons.foreach_get("material_index", mi)
    mi = np.where((fp == sid) & (mi == 0), 2, mi)
    mat = bpy.data.materials.get(kind.material)
    if mat is None:
        mat = shell_material(pal, kind.material, BODY_OPACITY, BREACH, RIM_ALPHA)
    me.materials.append(mat)
    me.polygons.foreach_set("material_index", mi)
    me.update()
    return body, sid


LIFT_CRACK, LIFT_FISSURE, LIFT_VENT = 0.0012, 0.0024, 0.0024   # m (× k) off the built crust: cracks < fissures
#   (draped on the mesh itself, a millimetre per layer is enough; the vent's three bands do not overlap, one lift)
# the crust's ink hull proxy (art review 2): the crust's own grooved copy (0.76 × the toon triangles) let the decals
# in the grooves stand up to 6.8 mm proud of it — padding a proxy that dips into every groove does not converge
# there. The proxy is the crust rebuilt on its own 48 columns without the extra rows, its grooves HULL_GROOVE as
# deep: the decals lie well inside it, and it stays a groove-hugging silhouette (the ink never bridges a crack).
HULL_SIDES, HULL_GROOVE = 48, 0.5


class Drape:
    """Lays a surface decal (crack, fissure, vent band) on the crust **as built**: every vertex moves to its
    nearest point on the crust mesh plus ``lift`` along that face's normal. The decals are generated on the analytic
    surface; the crust's 48 columns sample its 3 cm grooves coarsely, and the chords between samples stood over the
    strips laid in the groove bottoms (dashed fissures in the previews).

    Art review 2 (the torn vent): the vent's lip band has 10 cm radial quads on the curved crown, and their middles
    sank under the crust (a hole in the long arm). A decal draped with ``clear=True`` gets ``CLEAR_PASSES`` clearance
    passes: points along every edge and the centroid of each face are measured along the decal's own normal against
    the crust (≥ ``CLEAR_FRAC`` × lift above it) and against the decals registered under it (``under=True``: the
    vent's dark bands under its violet lip; ≥ ``CLEAR_OVER`` above them), and the face's corners are raised by the
    worst deficit (2-4 mm at the crown, inside the padded hull proxy: ``covered_by``). The cracks and fissures are not
    cleared: a two-vertex strip 5 cm wide bridging a 4 cm groove sinks up to 12 mm at a face centre, and lifting it
    that far floats it off the crust — following the groove needs a third vertex across every strip (~250 tris over
    the 2.9 k budget); the close-ups show them dashed in places, the game camera barely.
    """

    CLEAR_PASSES, CLEAR_FRAC, CLEAR_OVER, OVER_REACH, PROBE = 4, 0.7, 0.0006, 0.004, 0.02
    SAMPLES = (0.25, 0.5, 0.75)

    def __init__(self, crust: M.Part):
        from mathutils.bvhtree import BVHTree
        crust.bm.normal_update()
        self.tree = BVHTree.FromBMesh(crust.bm)
        self._uv, self._uf, self._under = [], [], None
        self.raised = []                      # (lift, largest raise) per decal, m — reported by the ship

    def _under_tree(self):
        from mathutils.bvhtree import BVHTree
        if self._under is None and self._uf:
            self._under = BVHTree.FromPolygons(self._uv, self._uf)
        return self._under

    @staticmethod
    def _height(tree, p: Vector, n: Vector, reach: float, two_sided: bool = False):
        """Signed height of ``p`` over the surface of ``tree`` along the decal's own normal ``n`` (the nearest point
        would sit on a groove wall, whose normal never sees the decal rise): a ray up from ``p`` that leaves the
        solid through an outward face → under it by that distance; else a ray down that meets an outward face → over
        it. ``two_sided`` (open decal strips): any face met going down within ``reach`` is under ``p``; any met going
        up is over it. None when nothing is within ``reach``."""
        hit = tree.ray_cast(p, n, reach)
        if hit[0] is not None and (two_sided or hit[1].dot(n) > 0):
            return -hit[3]
        hit = tree.ray_cast(p, -n, reach)
        if hit[0] is not None and (two_sided or hit[1].dot(n) > 0):
            return hit[3]
        return None

    def __call__(self, part: M.Part, lift: float, under: bool = False, clear: bool = False) -> M.Part:
        bm = part.bm
        bm.verts.index_update()
        normals = {}
        for v in bm.verts:
            hit = self.tree.find_nearest(v.co)
            if hit[0] is not None:
                v.co = hit[0] + hit[1].normalized() * lift
                normals[v.index] = hit[1].normalized()
        ut = self._under_tree()
        need = self.CLEAR_FRAC * lift
        total = {}
        up = self.PROBE
        for _ in range(self.CLEAR_PASSES if clear else 0):
            raise_by = {}
            for f in bm.faces:
                vs = [v.co.copy() for v in f.verts]
                n = sum((normals.get(v.index, Vector((0, 0, 1))) for v in f.verts), Vector()).normalized()
                pts = [sum(vs, Vector()) / len(vs)]
                for i in range(len(vs)):
                    a, b = vs[i], vs[(i + 1) % len(vs)]
                    pts.extend(a.lerp(b, t) for t in self.SAMPLES)
                deficit = 0.0
                for p in pts:
                    h = self._height(self.tree, p, n, up)
                    if h is not None:
                        deficit = max(deficit, need - h)
                    if ut is not None:
                        h = self._height(ut, p, n, self.OVER_REACH, two_sided=True)
                        if h is not None:
                            deficit = max(deficit, self.CLEAR_OVER - h)
                if deficit > 1e-6:
                    for v in f.verts:
                        raise_by[v.index] = max(raise_by.get(v.index, 0.0), deficit)
            if not raise_by:
                break
            for v in bm.verts:
                d = raise_by.get(v.index)
                if d:
                    v.co = v.co + normals.get(v.index, Vector((0, 0, 1))) * d
                    total[v.index] = total.get(v.index, 0.0) + d
        self.raised.append((lift, max(total.values(), default=0.0)))
        bm.normal_update()
        if under:
            base = len(self._uv)
            self._uv.extend(tuple(v.co) for v in bm.verts)
            self._uf.extend([base + v.index for v in f.verts] for f in bm.faces)
            self._under = None
        return part


def build_bloat(pal: Palette, rig: R.Rig, lod: int = 0):
    from kit import asset
    kind = BLOAT
    k = kind.k
    b = builder(kind, pal, lod)
    # LOD1 drops the grooves (its 24 columns cannot follow a 3 cm groove)
    grooves = PRIMARY + RING + BRANCHES
    surf = Dome(k, kind.lump, phase=1.7, tall=kind.tall, grooves=grooves,
                groove_depth=0.0 if lod else GROOVE, groove_w=GROOVE_W * k)
    bd = shell_bind(k, kind.tall)
    crust = surf.mesh(kind.sides if not lod else BLOAT_LOD1_SIDES, extra_rows=not lod)
    drape = Drape(crust)
    if not lod:
        hull = Dome(k, kind.lump, phase=1.7, tall=kind.tall, grooves=grooves, groove_depth=GROOVE * HULL_GROOVE,
                    groove_w=GROOVE_W * k).mesh(HULL_SIDES)
    else:       # LOD1: its own crust, inflated past the decals' lift (padding alone left them 2.5 mm proud)
        hull = surf.mesh(BLOAT_LOD1_SIDES)
        hull.bm.normal_update()
        for v in hull.bm.verts:
            v.co = v.co + v.normal * ((LIFT_FISSURE + 0.0012) * k)
    sid = b.add(crust, "crust", bd, name="crust", hull=hull, hull_pad=True)
    # cracks radiating from the vent, with branches and the ring cracks between them; the violet fissures glow in
    # the primaries' upper part and in the upper ring cracks. LOD1 (art review 2) keeps the primaries, their
    # fissures and the vent: the branches and the ring cracks / fissures ("branch", "ring_fissure") are skipped.
    for path in PRIMARY:
        b.add(drape(dome_strip(surf, path, CRACK_W * k, 0.0, steps=10, taper=0.85, taper0=0.0), LIFT_CRACK * k),
              "crack", bd, name="crack", outline=False, covered_by=sid)
        fp = _primary_fissure(path)
        b.add(drape(dome_strip(surf, fp, FISSURE_W * k, 0.0, steps=8, taper=0.9, taper0=0.0), LIFT_FISSURE * k),
              "fissure", bd, name="fissure", outline=False, covered_by=sid)
    for path in BRANCHES:
        b.add(drape(dome_strip(surf, path, CRACK_W * 0.8 * k, 0.0, steps=6, taper=0.9, taper0=0.5), LIFT_CRACK * k),
              "crack", bd, name="branch", outline=False, covered_by=sid)
    for i, path in enumerate(RING):
        b.add(drape(dome_strip(surf, path, CRACK_W * 0.85 * k, 0.0, steps=8, taper=0.6, taper0=0.6), LIFT_CRACK * k),
              "crack", bd, name="branch", outline=False, covered_by=sid)
        if i in RING_FISSURES:
            b.add(drape(dome_strip(surf, path, FISSURE_W * 0.7 * k, 0.0, steps=8, taper=0.8, taper0=0.8),
                        LIFT_FISSURE * k), "fissure", bd, name="ring_fissure", outline=False, covered_by=sid)
    # the torn crown vent: dark torn lips round a dark Y-shaped core (two bands that do not overlap), and the violet
    #   inner lip — a sliver along one flank of the long arm, over the core
    for inner, outer, nm in (("core", "lip", "vent_lip"), (None, "core", "vent_core")):
        b.add(drape(vent_band(surf, k, inner, outer), LIFT_VENT * k, under=True, clear=True), "crack", bd, name=nm,
              outline=False, covered_by=sid)
    b.add(drape(vent_lip_strip(surf, k), LIFT_VENT * k, clear=True), "fissure", bd, name="vent", outline=False,
          covered_by=sid)
    # the fuse ridges (hidden deep in the crust until Death; the core bone raises them out of the grooves)
    for path in PRIMARY:
        b.add(ridge(surf, _primary_fissure(path), k), "fissure", Bind.rigid("core"), name="ridge", outline=False)
    # the burst's char shards (hidden in the body until FX_Burst; BreachAnims._bloat_inner throws them out)
    # (each with a smooth copy of itself as its ink hull proxy: baked off the slab's split corner normals, the hull
    # opened at the corners and the tumbling shards lost their ink there — 87 % of the edge ≥ 3 px at Death F51)
    for i, (bone, seed, *_r, size) in enumerate(SHARDS):
        b.add(shard(seed, size).translate(shard_home(i)), "crack", Bind.rigid(bone), name="shard",
              hull=shard(seed, size).translate(shard_home(i)).smooth(True), hull_pad=True)
    body = asset.finish_mesh(b, pal, OUTLINE_CLASS, rig=rig, grounding_height=kind.H,
                             name=f"{kind.asset}_LOD{lod}" if lod else None)
    body["af_lod"] = lod
    # the crust's toon faces → slot 2: the palette toon with the breach-violet rim (M_AF_Toon,
    # RimOverride 1; the clot's shell rim) — the kit's cream rim put a pale beige crescent on the dark crust
    me = body.data
    fp = np.zeros(len(me.polygons), np.int32)
    me.attributes["af_part"].data.foreach_get("value", fp)
    mi = np.zeros(len(me.polygons), np.int32)
    me.polygons.foreach_get("material_index", mi)
    mi = np.where((fp == sid) & (mi == 0), 2, mi)
    mat = bpy.data.materials.get(kind.material) or rim_material(pal, kind.material, BREACH, BLOAT_RIM_ALPHA)
    me.materials.append(mat)
    me.polygons.foreach_set("material_index", mi)
    me.update()
    return body, sid


def build_all(kind: Kind):
    scene.reset()
    march_stage()
    pal = Palette(FAMILY)
    rig = build_rig(kind)
    body, sid = (build_clot if kind.key == "clot" else build_bloat)(pal, rig)
    return pal, rig, body


# ── pose model (the web SlimePose of slime.py + the fuse) → bone basis matrices ──────────────────────────
@dataclass
class BP:
    x: float = 0.0          # forward offset (m)
    hop: float = 0.0        # height off the ground (m)
    sx: float = 1.0
    sy: float = 1.0
    lean: float = 0.0       # top leans forward (+) or back (−)
    eyes: float = 1.0       # clot: spark brightness / size (1 alive … 0 guttered out)
    reach: float = 0.0      # pseudopod 0..1
    melt: float = 0.0
    fx: float = 0.0         # attack intensity: the sparks swell and gather to the front
    yaw: float = 0.0        # deg, a little body turn (3D life)
    daze: float = 0.0       # stunned: the sparks slow and sink
    settle: float = 0.0     # clot Death: the helm and the bolts lie down in the puddle
    fuse: float = 0.0       # bloat Death: the ridges rise through the crust
    tilt: float = 0.0       # top leans to the monster's left (+) or right (−): the off-axis sag of Stun
    burst: float = 0.0      # bloat Death after FX_Burst: 0 = shards hidden in the body … 1 = lying on the ground

    def lerp(self, o: "BP", t: float) -> "BP":
        return BP(*[a + (b - a) * t for a, b in zip(self.__dict__.values(), o.__dict__.values())])


REST = BP()


def P(**kw) -> BP:
    return replace(REST, **kw)


def sample(keys, t: float) -> BP:
    """Web ``samplePoseTrack``: keys [(at, ease, BP)], easing of the destination key (``slime.sample``)."""
    if t <= keys[0][0]:
        return keys[0][2]
    for (a0, _, p0), (a1, e1, p1) in zip(keys, keys[1:]):
        if a0 <= t <= a1:
            return p0.lerp(p1, anim.ease((t - a0) / max(a1 - a0, 1e-9), e1))
    return keys[-1][2]


def _helm_flat_rot() -> Quaternion:
    """The rotation that turns the tilted helm upright (crown axis ``HELM_AXIS`` → +Z): it lies brim-down in the
    puddle."""
    return HELM_AXIS.rotation_difference(Vector((0, 0, 1)))


class BreachAnims:
    """``slime.SlimeAnims`` for both breach-blood bodies at scale ``k``: the same body / pseudopod model (bone
    scale squash and stretch, a height-proportional lean shift, hop) with every distance × ``k``; the eyes, brows,
    X-eyes and bubbles are replaced by this family's inner motion (clot: drifting sparks, a slowly tumbling helm
    and bolt-heads; bloat: the fuse ridges)."""

    HELM_FLOOR = 0.025                           # helm origin height lying flat: the brim (crumpled, × 1.15) clears the sole, the crown (and its comb) stays under the puddle top
    BOLT_FLOOR = 0.012

    def __init__(self, rig: R.Rig, kind: Kind):
        self.rig, self.kind, self.k = rig, kind, kind.k
        self.H = kind.H
        self.rest = {b.name: b.matrix_local.copy() for b in rig.obj.data.bones}
        self.u, self.v = UW * kind.k, UH * kind.k
        self.helm_flat = _helm_flat_rot()

    # body ------------------------------------------------------------------------------------------
    def body_x(self, p: BP, kz: float) -> Matrix:
        """Armature-space map of the body at height fraction ``kz`` (``slime.SlimeAnims.body_x`` × k)."""
        sy = p.sy * (1.0 - 0.5 * p.melt)
        S = Matrix.Diagonal((p.sx, p.sx, sy, 1.0))
        shift = Vector((p.tilt * self.H * sy * kz * 1.2, -(p.x + p.lean * self.H * sy * kz * 1.2), p.hop))
        return Matrix.Translation(shift) @ Matrix.Rotation(math.radians(p.yaw), 4, "Z") @ S

    def basis(self, p: BP, t_ms: float, loop_ms: float) -> dict:
        out = {}
        rest = self.rest

        def put(name, X):
            Mr = rest[name]
            out[name] = Mr.inverted() @ X @ Mr
        for nm in ("body_base", "body_mid", "body_top"):
            put(nm, self.body_x(p, rest[nm].translation.z / self.H))
        for nm, d in (("pod_01", 0.09 * self.k), ("pod_02", 7.0 * self.u)):
            X = Matrix.Translation(Vector((0.0, -p.reach * d, -p.reach * 0.012 * self.k))) @ self.body_x(p, 0.0)
            S = Matrix.Diagonal((1.0 + 0.25 * p.reach, 1.0 + 0.6 * p.reach, 1.0 - 0.35 * p.reach, 1.0))
            c = rest[nm].translation
            put(nm, X @ Matrix.Translation(c) @ S @ Matrix.Translation(-c))
        if self.kind.key == "clot":
            self._clot_inner(put, p, t_ms, loop_ms)
        else:
            self._bloat_inner(put, p)
        return out

    # clot ------------------------------------------------------------------------------------------
    def _rigid(self, put, name: str, p: BP, home: Vector, rot: Quaternion, floor: float, settle_rot: Quaternion,
               lift: Vector = Vector(), turn_by: float = 1.0):
        """A rigid inner item: it rides the body (``body_x`` of its home, no squash of its own), turns by ``rot``
        and, as ``settle`` → 1, lies down (``settle_rot``, complete at ``settle`` = ``turn_by``) at ``floor`` height
        in the flattened puddle. The helm turns by .6: tipped on its side, its brim reached under the sole while it
        sank and turned at the same pace."""
        pa = self.body_x(p, home.z / self.H) @ (home + lift)
        st = _sstep(p.settle, 0.0, 1.0)
        sr = _sstep(p.settle, 0.0, turn_by)
        if st > 0:
            flat = Vector((pa.x * 0.55, pa.y * 0.55, floor))
            pa = pa.lerp(flat, st)
        ident = Quaternion((1, 0, 0), 0.0)
        # settling, the slosh / tumble dies out with it: the item comes to rest flat in the puddle
        q = Quaternion((0, 0, 1), math.radians(p.yaw)) @ ident.slerp(settle_rot, sr) @ rot.slerp(ident, sr)
        put(name, Matrix.Translation(pa) @ q.to_matrix().to_4x4() @ Matrix.Translation(-home))

    def _clot_inner(self, put, p: BP, t_ms: float, loop_ms: float):
        rest = self.rest
        ph = 2 * math.pi * ((t_ms / loop_ms) % 1.0)
        put("core", self.body_x(p, rest["core"].translation.z / self.H))
        # the helm: a slow tumble (one cycle per loop), sloshing with the lean; lies down in Death
        slosh = Quaternion((1, 0, 0), math.radians(-14.0 * p.lean)) @ Quaternion(
            Vector((0.6, 0.8, 0.0)).normalized(), math.radians(5.0 * math.sin(ph) * (1 - p.daze * 0.6)))
        drift = Vector((0.0, 0.0, 0.008 * math.sin(ph + 0.6)))
        self._rigid(put, "bone_prop", p, HELM_HOME, slosh, self.HELM_FLOOR, self.helm_flat, drift, turn_by=0.6)
        for i, (nm, (home, d)) in enumerate(BOLTS.items()):
            spin = Quaternion(d, math.radians(10.0 * math.sin(ph + 1.7 * i))) @ Quaternion(
                (0, 0, 1), math.radians(6.0 * math.sin(ph * 1.0 + i)))
            flat_d = Vector((d.x, d.y, 0.0)).normalized()
            lie = d.rotation_difference(flat_d)
            dz = Vector((0.0, 0.0, 0.010 * math.sin(ph + 2.3 * i + 1.0)))
            self._rigid(put, nm, p, home, spin, self.BOLT_FLOOR, lie, dz)
        # sparks: drifting loops round the core, a twinkle; dim on a hit (eyes), swell and gather to the front on
        # the slam's wind-up (fx), slow and sink when stunned (daze), gutter out in Death
        for i, (nm, home) in enumerate(SPARKS.items()):
            a = rest[nm].translation
            q = ph * (1.0 - 0.6 * p.daze) + i * 1.57
            loc = home + Vector((0.022 * math.sin(q), 0.016 * math.sin(2 * q + 0.5 * i), 0.018 * math.sin(q + 1.1)))
            loc += Vector((0.0, -0.05, -0.02)) * max(0.0, p.fx) + Vector((0.0, 0.0, -0.05)) * p.daze
            tw = 0.82 + 0.22 * math.sin(2 * ph + 2.4 * i)
            s = max(0.001, tw * max(0.0, min(1.2, p.eyes)) * (1.0 + 0.45 * max(0.0, p.fx)) * (1.0 - 0.35 * p.daze))
            pa = self.body_x(p, loc.z / self.H) @ loc
            Rz = Matrix.Rotation(math.radians(p.yaw + 40.0 * math.sin(q)), 4, "Z")
            put(nm, Matrix.Translation(pa) @ Rz @ Matrix.Diagonal((s, s, s, 1.0)) @ Matrix.Translation(-a))

    # bloat -----------------------------------------------------------------------------------------
    def _bloat_inner(self, put, p: BP):
        """The fuse ridges: modelled under the crust, scaled about the ground centre. Hidden (``RIDGE_HIDE``) unless
        the fuse burns; then (Death keys keep lean 0, so ``body_x`` is one affine for every height and the rigid
        ``core`` follows the skinned crust exactly) they grow back under the crust and rise through it."""
        f = p.fuse
        s = RIDGE_HIDE + (1.0 - RIDGE_HIDE) * _sstep(f, 0.0, 0.12) + RIDGE_RISE * _sstep(f, 0.10, 0.70)
        put("core", self.body_x(p, 0.5) @ Matrix.Diagonal((s, s, s, 1.0)))
        # the burst's char shards: collapsed to ~0 inside the body (riding it) until FX_Burst, then thrown out on a
        # low arc, tumbling, onto the puddle — flat, ``yaw``, at ``radius`` from the centre by the clip's end
        b = p.burst
        for i, (bone, _seed, az, rad, yaw, _size) in enumerate(SHARDS):
            home = shard_home(i)
            if b <= 0.0:
                X = self.body_x(p, home.z / self.H) @ Matrix.Translation(home) @ Matrix.Diagonal(
                    (1e-3, 1e-3, 1e-3, 1.0)) @ Matrix.Translation(-home)
                put(bone, X)
                continue
            ar = math.radians(az)
            target = Vector((rad * math.sin(ar), -rad * math.cos(ar), 0.5 * SHARD_T + 0.004))
            e = 1.0 - (1.0 - min(b, 1.0)) ** 2
            pos = Vector((0.0, 0.0, 0.10)).lerp(target, e) + Vector((0.0, 0.0, 0.16 * math.sin(math.pi * min(b, 1.0))))
            sc = max(1e-3, _sstep(b, 0.0, 0.35))
            axis = Vector((math.cos(ar + 1.0), math.sin(ar + 1.0), 0.4)).normalized()
            q = Quaternion((0, 0, 1), math.radians(yaw)) @ Quaternion(axis, math.radians(
                260.0 * (1.0 - min(b, 1.0)) ** 2))         # the tumble dies out before they land
            put(bone, Matrix.Translation(pos) @ q.to_matrix().to_4x4() @ Matrix.Diagonal((sc, sc, sc, 1.0))
                @ Matrix.Translation(-home))

    # clips -----------------------------------------------------------------------------------------
    def clip(self, name, length_ms, fn, loop=False, notifies=None, ref_speed=None, loop_ms=667.0, key_ms=None):
        c = Clip(name, length_ms, loop=loop, notifies=notifies or {}, ref_speed=ref_speed)
        c.basis_fn = lambda t: self.basis(fn(t), t, loop_ms)
        c.sampler = lambda t: None
        c.key_poses_ms = key_ms or [length_ms * i / 5 for i in range(6)]
        return c

    def is_bloat(self) -> bool:
        return self.kind.key == "bloat"

    def idle(self):
        if self.is_bloat():
            L = 1200.0

            def fn(t):          # heavy wheezing breath: a slow swell with a catch at the top (two pulses)
                ph = 2 * math.pi * t / L
                br = math.sin(ph) + 0.25 * math.sin(2 * ph + 0.6)
                return P(sx=1 + 0.03 * br, sy=1 - 0.035 * br, lean=0.03 * math.sin(ph + 0.8),
                         yaw=2.0 * math.sin(ph))
            return self.clip("Idle", L, fn, loop=True, loop_ms=L)

        def fn(t):
            ph = 2 * math.pi * t / 667.0
            return P(sx=1 + math.sin(ph) * 0.06, sy=1 - math.sin(ph) * 0.07, lean=math.sin(ph + 0.8) * 0.05,
                     yaw=3.0 * math.sin(ph * 0.5))
        return self.clip("Idle", 667.0, fn, loop=True)

    def walk(self):
        """One hop per cycle with no sliding (R9): planted (t ≥ .82 and t < .08) the body moves back at exactly
        the ground speed, airborne it lunges forward. Clot = the slime's hop (600 ms, 1.20 m/s); the bloat lurches
        (1000 ms, 0.90 m/s, half the hop, smaller squash)."""
        v = self.v
        if self.is_bloat():
            L, ref, land = 1000.0, self.kind.ref_speed, 850.0
            keys = [(0.0, "smooth", P(sx=1.12, sy=0.86)),
                    (0.2, "out", P(sx=0.92, sy=1.10, hop=3.5 * v, lean=0.08)),
                    (0.45, "smooth", P(sx=0.95, sy=1.06, hop=5.0 * v, lean=0.04)),
                    (0.7, "in", P(sx=1.0, sy=1.02, hop=2.0 * v)),
                    (0.85, "smooth", P(sx=1.16, sy=0.82)),
                    (1.0, "smooth", P(sx=1.12, sy=0.86))]
            kms = [0.0, 200.0, 450.0, 700.0, 850.0, 1000.0]
        else:
            L, ref, land = 600.0, self.kind.ref_speed, 510.0
            keys = [(0.0, "smooth", P(sx=1.18, sy=0.8)),
                    (0.2, "out", P(sx=0.86, sy=1.18, hop=6 * v, lean=0.12)),
                    (0.45, "smooth", P(sx=0.92, sy=1.1, hop=9 * v, lean=0.06)),
                    (0.7, "in", P(sx=1.0, sy=1.02, hop=3 * v)),
                    (0.85, "smooth", P(sx=1.25, sy=0.74)),
                    (1.0, "smooth", P(sx=1.18, sy=0.8))]
            kms = [0.0, 120.0, 270.0, 420.0, 510.0, 600.0]
        c0, c1 = 0.82, 1.08
        A = ref * L / 1000.0 * (c1 - c0) / 2.0

        def xoff(t):
            u = t if t >= c0 else t + 1.0
            if u <= c1:
                return A - 2 * A * (u - c0) / (c1 - c0)
            f = (t - (c1 - 1.0)) / (c0 - (c1 - 1.0))
            e = f * f * (3 - 2 * f)
            return -A + 2 * A * e

        def fn(t_ms):
            t = (t_ms / L) % 1.0
            p = sample(keys, t)
            return replace(p, x=xoff(t), yaw=2.0 * math.sin(2 * math.pi * t))
        return self.clip("Walk", L, fn, loop=True, notifies={"FX_Land": land}, ref_speed=ref, loop_ms=L, key_ms=kms)

    def attack_timing(self) -> tuple[float, float, float]:
        """(length, Contact, WindupEnd) ms: the clot's is the slime's slam; the bloat's slower (WindupEnd = 62 %)."""
        return (750.0, 400.0, 248.0) if self.is_bloat() else (500.0, SL.CONTACT_MS, SL.WINDUP_END_MS)

    def attack(self):
        u, v = self.u, self.v
        L, cms, wms = self.attack_timing()
        # the wind-up rears 1.2 cm (× k) off the ground (art review 2, ink gate): planted, the stretched dome's
        # bottom contour met its contact line and the ground hid the ink under it (91.6 % of the edge ≥ 3 px at
        # F5 se); reared, 100 %
        keys = [(0.0, "smooth", REST),
                (0.33, "out", P(x=-3 * u, sx=0.85, sy=1.26, lean=-0.3, fx=0.3, hop=0.012 * self.k)),
                (0.67, "in", P(x=4 * u, sx=0.95, sy=1.1, hop=6 * v, lean=0.35, reach=0.4, fx=0.7)),
                (1.0, "linear", P(x=5 * u, sx=1.3, sy=0.72, lean=0.25, reach=1.0, fx=1.0)),
                (1.32, "out", P(x=4.6 * u, sx=1.22, sy=0.8, lean=0.18, reach=0.85, fx=0.9)),
                (1.65, "smooth", P(x=1.5 * u, sx=0.95, sy=1.06, lean=-0.04, reach=0.2, fx=0.5)),
                (L / cms, "smooth", REST)]
        if self.is_bloat():             # heavier: a shallower hop, the squash held longer
            keys[2] = (0.67, "in", P(x=4 * u, sx=0.97, sy=1.08, hop=3.5 * v, lean=0.3, reach=0.4, fx=0.7))

        def fn(t_ms):
            return sample(keys, t_ms / cms)
        km = sorted({0.0, round(cms / 3, 1), round(2 * cms / 3, 1), cms, round(1.32 * cms, 1), round(1.65 * cms, 1),
                     L})
        return self.clip("Attack01", L, fn, notifies={"WindupEnd": wms, "Contact": cms}, key_ms=km)

    def hurt(self):
        u = self.u
        L = 260.0 if self.is_bloat() else 200.0
        keys = [(0.0, "smooth", P(x=-3 * u, sx=1.35, sy=0.68, lean=-0.25, eyes=0.35)),
                (0.5, "smooth", P(x=-1.5 * u, sx=0.9, sy=1.12, lean=0.1, eyes=0.7)),
                (1.0, "smooth", P(x=-0.5 * u, sx=1.02, sy=0.98, eyes=0.95))]
        if self.is_bloat():
            keys = [(0.0, "smooth", P(x=-2 * u, sx=1.22, sy=0.8, lean=-0.18)),
                    (0.5, "smooth", P(x=-1 * u, sx=0.95, sy=1.07, lean=0.06)),
                    (1.0, "smooth", P(x=-0.4 * u, sx=1.01, sy=0.99))]

        def fn(t_ms):
            return sample(keys, t_ms / L)
        return self.clip("Hurt", L, fn, key_ms=[0.0, L / 2, L])

    def death(self):
        u = self.u
        if self.is_bloat():
            return self._fuse()
        # the slime's melt, a little thicker (tar is viscous; the helm lies in it), the sparks gutter out and the
        # helm and bolts settle flat
        keys = [(0.0, "smooth", P(x=-2 * u, sx=1.35, sy=0.7, lean=-0.2, eyes=0.3)),
                (0.33, "smooth", P(x=-2 * u, sx=0.85, sy=1.25, eyes=0.0, settle=0.15)),
                (0.67, "in", P(x=-2 * u, sx=1.6, sy=0.55, eyes=0.0, melt=0.5, settle=0.7)),
                (1.0, "out", P(x=-1 * u, sx=1.60, sy=0.50, eyes=0.0, melt=1.0, settle=1.0))]

        def fn(t_ms):
            return sample(keys, t_ms / 500.0)
        c = self.clip("Death", 500.0, fn, key_ms=[0.0, 165.0, 335.0, 500.0])
        c.manifest_extra = {"puddleDecal": {"color": "#2A0E14", "alpha": 0.7, "radiusFactor": 1.1, "driesMs": 3000,
                                            "note": "monsters.md 1.5 / 2.1: breach-blood leaves a tar puddle that "
                                                    "dries in 3 s, not a corpse"}}
        return c

    FUSE_MS, DEATH_MS = 800.0, 900.0
    COLLAPSE = 0.001                             # the crust's scale from FX_Burst on (collapsed about the ground centre:
    #   sub-pixel; at .01 a 1 px un-inked speck stayed at the puddle's centre)

    def _fuse(self):
        """Death = the fuse (monsters.md 2.1 [W2]: bone-scale Death clip, 800 ms, under the crimson ring decal):
        the crust gasps, swells and shudders while the fuse ridges rise along the fissures; at ``FX_Burst`` (800 ms,
        A12's violet-black ``death.burst`` + ash ring) the crust **collapses** — every body bone to ``COLLAPSE``
        about the ground centre within one frame, so each fissure, ridge and the vent go out with it — and six low
        dark char shards are thrown onto the scorched puddle (art review 2: the crust used to spread into a 1.6 ×
        disc with its lit band, glowing fissures and vent: a squashed pancake that never visibly burst). Lean stays 0
        throughout (the rigid ridge bone follows the crust exactly). The timing needs the runtime of
        ``FUSE_RUNTIME`` (manifest ``fx.fuse.runtime`` / the clip's ``runtime``)."""
        F, fr, c0 = self.FUSE_MS, 1000.0 / scene.FPS, self.COLLAPSE
        keys = [(0.0, "smooth", P()),
                (80.0, "out", P(sx=0.94, sy=1.08, fuse=0.10)),
                (300.0, "smooth", P(sx=1.06, sy=1.08, fuse=0.42)),
                (600.0, "in", P(sx=1.16, sy=1.18, fuse=0.80)),
                (F - fr, "in", P(sx=1.24, sy=1.25, fuse=1.0)),
                (F, "linear", P(sx=c0, sy=c0, fuse=1.0, burst=0.12)),
                (F + 50.0, "out", P(sx=c0, sy=c0, fuse=1.0, burst=0.75)),
                (self.DEATH_MS, "smooth", P(sx=c0, sy=c0, fuse=1.0, burst=1.0))]

        def fn(t_ms):
            p = sample(keys, t_ms)
            if t_ms < F:      # the shudder grows with the fuse
                a = p.fuse
                p = replace(p, x=0.010 * self.k * a * math.sin(2 * math.pi * t_ms / 46.0),
                            yaw=4.0 * a * math.sin(2 * math.pi * t_ms / 71.0 + 0.4))
            return p
        c = self.clip("Death", self.DEATH_MS, fn, notifies={"FX_Fuse": 0.0, "FX_Burst": F},
                      key_ms=[0.0, 80.0, 300.0, 600.0, round(F - fr, 1), F, F + 50.0, self.DEATH_MS])
        c.manifest_extra = {"usage": "the fuse: play on death; the burst VFX fires on FX_Burst (A12 death.burst "
                                     "recoloured violet-black + ash ring), the ring decal fills to FX_Burst; at "
                                     "FX_Burst the crust (every fissure, ridge and the vent with it) collapses to "
                                     "nothing in one frame and six dark char shards land on the puddle",
                            "runtime": FUSE_RUNTIME,
                            "puddleDecal": {"color": "#1A0A0A", "alpha": 0.6, "radiusFactor": 1.2, "driesMs": 3000,
                                            "note": "scorched crust, no corpse (monsters.md 1.5); the shards lie "
                                                    "within 0.55 m of the centre, inside the decal"}}
        return c

    STUN_TILT = (0.36, 0.13)                # Stun: the held sideways sag (tilt) and its wobble about that hold

    def stun(self):
        """Stunned: a **lopsided sag** (art review 2: the old loops — a few % of squash and a centred sway — were
        almost static at the game camera, so the stun read rested on the overhead stars alone). The body slumps
        toward its left and *holds* there — ``tilt`` never crosses zero (≈ 15 cm at the clot's crown, 19 cm at the
        bloat's; art review 2's first pass held 11 / 14 cm, still a subtle shear from the 50° camera) — squashed lower
        than Idle, while the top circles slowly off-axis (tilt and lean in quadrature, an uneven wave) and the yaw
        drifts. The clot's sparks slow, dim and sink; the bloat's wheeze stops."""
        L = 700.0 if self.is_bloat() else 600.0
        amp = 0.8 if self.is_bloat() else 1.0
        t0, t1 = self.STUN_TILT

        def fn(t_ms):
            ph = 2 * math.pi * t_ms / L
            w = math.sin(ph) + 0.3 * math.sin(2 * ph + 0.4)          # uneven: a slow lean-in, a quicker sag back
            return P(sx=1.08 + 0.03 * math.sin(2 * ph + 0.5), sy=0.85 - 0.035 * math.sin(2 * ph + 0.5),
                     x=0.016 * self.k * math.cos(ph), tilt=amp * (t0 + t1 * w), lean=0.16 * amp * math.cos(ph),
                     yaw=7.0 * amp * math.sin(ph + 1.0), eyes=0.5 + 0.1 * math.sin(ph + 1.2), daze=1.0)
        c = self.clip("Stun", L, fn, loop=True, loop_ms=L, key_ms=[0.0, L / 4, L / 2, 3 * L / 4, L])
        c.manifest_extra = {"usage": "stunned (C5 stun skills / R8 stun stars over fx_overhead): loops until the "
                                     "stun ends — a lopsided sag held to the monster's left, the top circling "
                                     "slowly off-axis (reads as stunned without the stars)"}
        return c

    def all_clips(self):
        return [self.idle(), self.walk(), self.attack(), self.hurt(), self.death(), self.stun()]


# ── manifest extras ──────────────────────────────────────────────────────────────────────────────────────
# Art review 2, the fuse's runtime hand-off (core / UE lanes): AAbyssCharacterActor::BeginDeath plays the Death clip at
# clamp(clipLength / deathDuration, .5, 2) — at the core's default deathDuration of 500 ms that is rate 1.8, so
# FX_Burst would land at 444 ms, under the ≥ 800 ms fuse rule (monsters.md 1.3) — and with a directional killing blow
# it throws, hops and spins the body and starts the body fade at 0.25 × duration (or after the throw), so the fuse
# would be faded out before it bursts.
FUSE_RUNTIME = {
    "deathDurationMs": 900, "playRate": 1.0, "deathThrow": False, "deathHop": False, "deathSpin": False,
    "bodyFadeNotBeforeMs": 900,
    "note": ("the cinderbloat's Death clip is the fuse and must play at rate 1: deathDuration 900 ms (BeginDeath "
             "rate = clamp(clipLength / deathDuration, .5, 2); the core default 500 ms gives 1.8 and FX_Burst at "
             "444 ms, under the >= 800 ms fuse rule, monsters.md 1.3), no death throw / hop / spin (it stands under "
             "its ring decal), and no body fade before FX_Burst — fade the shards out with the puddle after the clip "
             "ends (BeginDeath fades from 0.25 x duration or after the throw today). Owner: core (monster "
             "deathDurationMs) + UE (AAbyssCharacterActor::BeginDeath)"),
}

SLIME_BONES = ("root", "body_base", "body_mid", "body_top", "pod_01", "pod_02", "eye_l", "eye_r", "lid_l", "lid_r",
               "xeye_l", "xeye_r", "core", "bone_prop", "bubble_01", "bubble_02")
BONE_USE = {
    "clot": {"core": "unused (kept for SKEL_Slime)", "bone_prop": "the dented kettle helm (rigid; settles flat in "
             "Death)", "bubble_01": "crossbow bolt-head", "bubble_02": "crossbow bolt-head",
             "eye_l": "violet spark", "eye_r": "violet spark", "xeye_l": "violet spark", "xeye_r": "violet spark",
             "lid_l": "unused (kept for SKEL_Slime)", "lid_r": "unused (kept for SKEL_Slime)"},
    "bloat": {"core": f"the fuse ridges (scaled about the ground centre: hidden at {RIDGE_HIDE:g}, rise to "
                      f"{1 + RIDGE_RISE:g} in Death)",
              **{sh[0]: "a burst char shard (rigid; scaled to ~0 inside the body until FX_Burst, then thrown onto "
                        "the puddle in Death)" for sh in SHARDS},
              "xeye_r": "unused", "lid_l": "unused", "lid_r": "unused"},
}


def fx_for(kind: Kind, BA: BreachAnims) -> dict:
    L, cms, wms = BA.attack_timing()
    if kind.key == "clot":
        return {"breachRim": {"color": BREACH, "alpha": RIM_ALPHA,
                              "material": f"{kind.material} (M_AF_Toon_Slime RimOverride*, slot 2 params)",
                              "paramNote": RIM_PARAM_NOTE},
                "bodyOpacity": BODY_OPACITY,
                "puddle": {"color": "#2A0E14", "alpha": 0.7, "radiusFactorOfWidth": 1.1, "fromClip": "Death",
                           "driesMs": 3000},
                "blobShadow": {"radiusFactorOfWidth": 1.05, "liftShrink": "hop × 1.5 (web)"},
                "telegraph": {**TELEGRAPH, "untilMs": wms},
                "land": {"notify": "FX_Land", "atMs": 510},
                "sparks": {"bones": ["eye_l", "eye_r", "xeye_l", "xeye_r"], "color": BREACH_PIN,
                           "note": "the family marker: emissive palette swatch clot.marker, no glow card"}}
    return {"breachRim": {"color": BREACH, "alpha": BLOAT_RIM_ALPHA,
                          "material": f"{kind.material} (M_AF_Toon RimOverride*, slot 2 params: the crust)",
                          "paramNote": RIM_PARAM_NOTE},
            "telegraph": {**TELEGRAPH, "untilMs": wms},
            "land": {"notify": "FX_Land", "atMs": 850},
            "fuse": {"clip": "Death", "startNotify": "FX_Fuse", "burstNotify": "FX_Burst",
                     "burstMs": BA.FUSE_MS, "runtime": FUSE_RUNTIME,
                     "collapse": {"atMs": BA.FUSE_MS, "bodyScale": BA.COLLAPSE, "shards": len(SHARDS),
                                  "note": "at FX_Burst every body bone scales to ~0 about the ground centre within "
                                          "one frame (the fissures, ridges and vent go out with the crust); six dark "
                                          "char shards (bloat.crack) land on the puddle by the clip's end"},
                     "ringDecal": {"rim": "#D11F2E", "fill": "#FFE8DC", "radiusCm": 220, "fillsUntilMs": BA.FUSE_MS,
                                   "note": "monsters.md 2.1 [W2]: r2.2 crimson ring decal (A12 / the hazard style)"},
                     "glow": {"socket": "fx_chest", "color": BREACH, "coreColor": BREACH_PIN,
                              "radiusCm": round(FUSE_GLOW_R * 100), "alphaFrom": FUSE_GLOW_A[0],
                              "alphaTo": FUSE_GLOW_A[1], "coreAlphaTo": 0.5, "blend": "additive",
                              "note": "the fuse light swells over FX_Fuse → FX_Burst, off at the burst"},
                     "burst": {"recipe": "notify.FX_Burst", "fallback": "death.burst",
                               "colors": [BREACH, BREACH_CORE], "ashRing": "#8A8480",
                               "owner": "A12 (violet-black flame, ash ring, no gore; never ember): the burst is the "
                                        "FX_Burst notify's recipe; death.burst is only the generic kill fallback"}},
            "ashFlakes": {"socket": "fx_vent", "color": "#8A8480", "ratePerS": 3.0, "riseCmS": 22, "lifeMs": 1500,
                          "owner": "A12 (runtime particles: ash flakes drift off the crust)"},
            "puddle": {"color": "#1A0A0A", "alpha": 0.6, "radiusFactorOfWidth": 1.2, "fromClip": "Death",
                       "driesMs": 3000},
            "blobShadow": {"radiusFactorOfWidth": 1.05, "liftShrink": "hop × 1.5 (web)"}}


def marker_extra(kind: Kind) -> dict:
    what = ("violet pinpoint sparks #D08CFF (emissive 1.0) drifting inside the translucent shell; the shell's "
            "breach-violet fresnel rim #B02FD0 (material) carries the family read") if kind.key == "clot" else \
        ("breach-violet fissures, the upper ring cracks and the torn crown vent #B02FD0 (emissive .8; never ember); "
         "the crust's breach-violet rim (material) carries the family read")
    return {"marker": {"swatches": list(kind.marker), "family": "Breach-blood", "kind": what}}


def remake_extra(kind: Kind, BA: BreachAnims) -> dict:
    L, cms, wms = BA.attack_timing()
    if kind.key == "clot":
        notes = ("SKEL_Slime re-theme of the slime: squash / stretch as bone scale keys (body_*), pseudopod pod_*; "
                 "the inner bones carry the helm, two bolt-heads and four sparks (boneUse). Shell = slot 2 "
                 f"{kind.material} (M_AF_Toon_Slime: BodyOpacity .62 alpha blend, RimOverride violet 1.0, R12; "
                 "RimOverrideColor as sRGB floats, fx.breachRim.paramNote); the helm (on its side), bolts, sparks, "
                 "the wet highlight and the tar backing bowl (clot.pitchdepth: unshaded, emissive 1 at a value that "
                 "renders as tar) are opaque toon (slot 0). Walk is a no-slide hop: play rate = v / 1.20 m/s. Death "
                 "melts into a tar puddle (no corpse) with the helm lying in it; Stun loops (a lopsided sag; the "
                 "sparks slow and sink).")
        style = "slam"
    else:
        notes = ("SKEL_Slime variant of the clot at x1.35 (x1.12 taller: swollen): opaque cracked crust of ash-dusted "
                 f"char in plates (slot 2 {kind.material}: M_AF_Toon, RimOverride violet; RimOverrideColor as sRGB "
                 "floats, fx.breachRim.paramNote), violet fissures, ring cracks and a torn vent (violet only on its "
                 "inner lip). Walk is a no-slide lurch: play rate = v / 0.90 m/s. Attack01 a heavy slam (Contact "
                 f"{cms:g} ms). Death is the fuse: FX_Fuse 0 ms, ridges rise along the fissures (core bone), "
                 f"FX_Burst {BA.FUSE_MS:g} ms (A12 burst): the crust collapses in one frame and six char shards land "
                 "on the scorched puddle. The fuse needs deathDuration 900 ms (rate 1), no death throw / hop / spin "
                 "and no body fade before FX_Burst (fx.fuse.runtime). [W2] the burst damage / decal / monster x2 "
                 "rules are the core's (monsters.md 2.1).")
        style = "slam"
    return {"refSpeedCmS": round(kind.ref_speed * 100.0, 1), "attackStyle": style,
            "footprintRadiusCm": round(kind.W * 100, 1),
            "boneUse": BONE_USE[kind.key],
            "remake": {"family": "breach", "palette": FAMILY, "voiceFamily": "Slime", "contactMs": cms,
                       "spec": "Docs/dark-remake/monsters.md 2.1"},
            "notes": notes}


# ── preview helpers ──────────────────────────────────────────────────────────────────────────────────────
def _socket_world(rig: R.Rig, name: str) -> Vector:
    e = next(x for x in rig.sockets if x["name"] == name)
    b = rig.obj.data.bones[e["bone"]]
    pb = rig.obj.pose.bones[e["bone"]]
    return rig.obj.matrix_world @ (pb.matrix @ (b.matrix_local.inverted() @ Vector(e["pos"])))


FUSE_GLOW_R = 0.45                           # m: the fuse light swells inside the crust (a 75 cm card washed the whole
FUSE_GLOW_A = (0.12, 0.65)                   # silhouette violet and lit the ink: the ship's dark-ink check failed)


def fuse_glows(rig: R.Rig):
    """The runtime fuse glow card (manifest ``fx.fuse.glow``) while a Death action plays, before ``FX_Burst``."""
    def fn(cam, res):
        from kit import review
        ad = rig.obj.animation_data
        act = ad.action if ad is not None else None
        if act is None or not act.name.endswith("_Death"):
            return []
        t = scene.frame_to_ms(bpy.context.scene.frame_current)
        if t >= BreachAnims.FUSE_MS:
            return []
        a = _sstep(t, 0.0, BreachAnims.FUSE_MS)
        wp = _socket_world(rig, "fx_chest")
        px, py = review.project_px(cam, wp, res)
        wpp = outline.pixel_world_size(cam, wp, res[0])
        return [dict(center_px=(px, py), radius_px=FUSE_GLOW_R / wpp, color=BREACH,
                     alpha=FUSE_GLOW_A[0] + (FUSE_GLOW_A[1] - FUSE_GLOW_A[0]) * a,
                     core=BREACH_PIN, core_frac=0.3, core_alpha=0.5 * a)]
    return fn


class _A:
    """Duck-typed anims holder for ``mship`` (rig + clips)."""

    def __init__(self, rig):
        self.rig = rig


# ── QA: the inner items stay inside the shell / the fuse ridges stay under the crust ──────────────────────
def containment_qa(kind: Kind, rig: R.Rig, body, clips, actions) -> dict:
    """Per frame of every clip, intersecting triangle pairs between the shell (clot: slot 2; bloat: the crust's
    toon faces) and the inner items (clot: helm, bolts, sparks, core) or the fuse ridges (bloat; allowed only in
    Death, where they rise through the crust on purpose), plus the shell's lowest point (``mship``'s ground QA
    looks at slot 0 only). ``mship``'s PairQA groups take toon faces only, so this runs beside it."""
    from qa import Checker
    names = json.loads(body["af_parts"])
    me = body.data
    fp = np.zeros(len(me.polygons), np.int32)
    me.attributes["af_part"].data.foreach_get("value", fp)
    mi = np.zeros(len(me.polygons), np.int32)
    me.polygons.foreach_get("material_index", mi)
    pn = np.array(names + ["?"], dtype=object)[np.clip(fp, -1, len(names) - 1)]
    shell_name, slot = ("shell", 2) if kind.key == "clot" else ("crust", 2 if kind.material else 0)
    shell = np.nonzero((pn == shell_name) & (mi == slot))[0]
    inner = ("helm", "bolt", "spark", "inner") if kind.key == "clot" else ("ridge", "shard")
    groups = {g: np.nonzero((pn == g) & (mi == 0))[0] for g in inner}
    rep = {}
    saved = rig.obj.rotation_euler.copy()
    rig.obj.rotation_euler = (0, 0, 0)
    for c, a in zip(clips, actions):
        anim.assign(rig, a)
        worst = {g: 0 for g in inner}
        frames = {g: [] for g in inner}
        low = 1e9
        for fr in range(0, c.frames + 1):
            anim.set_frame(fr)
            gb = Checker._geom(body)
            ts, zs = Checker._tree(gb, shell)
            low = min(low, zs)
            for g, faces in groups.items():
                tg, _ = Checker._tree(gb, faces)
                n = Checker._pairs(tg, ts)
                if n:
                    frames[g].append(fr)
                worst[g] = max(worst[g], n)
        rep[c.name] = {"pairs": worst, "frames": {g: f[:10] for g, f in frames.items() if f},
                       "shellLowCm": round(low * 100.0, 2)}
    rig.obj.rotation_euler = saved
    anim.set_frame(0)
    fails = []
    for clip, r in rep.items():
        for g, n in r["pairs"].items():
            if n and not (kind.key == "bloat" and clip == "Death"):
                fails.append(f"{clip}: {g} × {shell_name} {n} pairs (frames {r['frames'].get(g)})")
        if r["shellLowCm"] < -0.5:
            fails.append(f"{clip}: {shell_name} {r['shellLowCm']} cm under the ground")
    return {"pass": not fails, "failures": fails, "clips": rep}


def print_containment(res: dict) -> None:
    print("\n[qa] containment (intersecting pairs with the shell / crust; lowest shell point cm)")
    for clip, r in res["clips"].items():
        print(f"  {clip:9s} " + "  ".join(f"{g} {n}" for g, n in r["pairs"].items()) + f"  low {r['shellLowCm']}")
    for f in res["failures"]:
        print("  QA FAIL " + f)


# ── game keys ────────────────────────────────────────────────────────────────────────────────────────────
GAME_KEYS = {
    "clot": [("Idle", 0, "se"), ("Walk", 7, "se"), ("Walk", 16, "se"), ("Walk", 31, "se"), ("Attack01", 5, "se"),
             ("Attack01", 15, "se"), ("Hurt", 0, "se"), ("Death", 20, "se"), ("Death", 30, "se"), ("Stun", 0, "se"),
             ("Stun", 18, "sw"), ("Idle", 20, "sw")],
    "bloat": [("Idle", 0, "se"), ("Walk", 12, "se"), ("Walk", 27, "se"), ("Walk", 51, "se"), ("Attack01", 10, "se"),
              ("Attack01", 24, "se"), ("Hurt", 0, "se"), ("Death", 18, "se"), ("Death", 36, "se"),
              ("Death", 47, "se"), ("Death", 51, "se"), ("Death", 54, "se"), ("Stun", 0, "se"), ("Stun", 21, "sw")],
}
CLOSE_KEYS = {
    "clot": [("Idle", 0, "front"), ("Walk", 12, "side"), ("Attack01", 5, "se"), ("Attack01", 15, "se"),
             ("Attack01", 15, "side"), ("Hurt", 0, "se"), ("Death", 10, "se"), ("Death", 20, "se"),
             ("Death", 30, "se"), ("Stun", 18, "front")],
    "bloat": [("Idle", 0, "front"), ("Walk", 27, "side"), ("Attack01", 10, "se"), ("Attack01", 24, "se"),
              ("Attack01", 24, "side"), ("Hurt", 0, "se"), ("Death", 30, "se"), ("Death", 47, "se"),
              ("Death", 51, "se"), ("Death", 54, "se"), ("Stun", 21, "front")],
}


def contact_frame(BA: BreachAnims) -> int:
    return scene.ms_to_frame(BA.attack_timing()[1])


# ── dev modes ────────────────────────────────────────────────────────────────────────────────────────────
def _review(kind: Kind, rig, body, out: Path):
    import mship
    rv = mship.MReview(kind.asset, rig.obj, [body], kind.H, blob_radius=kind.W * 1.05, out_dir=out,
                       game_outline_px=OUTLINE_PX, outline_class=OUTLINE_CLASS,
                       glows=fuse_glows(rig) if kind.key == "bloat" else None)
    rv.gate = mship.gate_for(OUTLINE_CLASS, OUTLINE_PX)
    rv.ground_occludes = True
    rv.extra_body_slots = True
    return rv


def quick(kind: Kind, out: Path, clip: str | None = None, frame: int = 0) -> None:
    import mship
    pal, rig, body = build_all(kind)
    BA = BreachAnims(rig, kind)
    out.mkdir(parents=True, exist_ok=True)
    c = next(c for c in BA.all_clips() if c.name == (clip or "Idle"))
    anim.bake_basis(rig, c, kind.token, c.basis_fn)
    anim.set_frame(frame)
    rv = _review(kind, rig, body, out)
    rv.closeup("se")
    (out / "closeup.png").rename(out / "closeup_se.png")
    rv.closeup("front", pitch=-30.0)
    (out / "closeup.png").rename(out / "closeup_front.png")
    rv.closeup("ne", pitch=-30.0)
    (out / "closeup.png").rename(out / "closeup_ne.png")
    rv.turnaround()
    rv.game_view()
    for p, s in rv.written:
        print(f"  {p.name} {s // 1024} KB")
    print(f"[budget] {kind.asset} toon tris {mship.toon_tris(body)}")


def game_quick(kind: Kind, out: Path) -> None:
    """Every clip baked; the game-camera pose and facing sheets (ink-gated) → ``out``."""
    import mship
    ms = make_ship(kind)
    ms.lod1.hide_render = True                 # (as mship.ship: only LOD0 in the previews)
    out.mkdir(parents=True, exist_ok=True)
    actions = [mship.bake_clip(ms.A.rig, c, ms.token) for c in ms.clips]
    rv = _review(kind, ms.A.rig, ms.body, out)
    mship.game_sheet(ms, rv, ms.game_keys, "game_poses.png", actions)
    facing_keys = [(n, f, fac) for fac in ("front", "sw", "ne", "back") for n, f in (("Idle", 0), ("Walk", 9))]
    mship.game_sheet(ms, rv, facing_keys, "game_facings.png", actions)
    mship.closeup_poses(ms, rv, actions)
    for p, s in rv.written:
        print(f"  {p.name} {s // 1024} KB")
    res = containment_qa(kind, ms.A.rig, ms.body, ms.clips, actions)
    print_containment(res)


def anim_quick(kind: Kind, out: Path, names, facing: str = "se") -> None:
    import mship
    pal, rig, body = build_all(kind)
    BA = BreachAnims(rig, kind)
    cs = [c for c in BA.all_clips() if not names or c.name in names]
    mship.anim_review(rig, [body], [], cs, kind.token, out, kind.H, OUTLINE_CLASS, blob_radius=kind.W * 1.05,
                      glows=fuse_glows(rig) if kind.key == "bloat" else None, facing=facing, px=OUTLINE_PX)


def qa_only(kind: Kind) -> int:
    import mship
    pal, rig, body = build_all(kind)
    BA = BreachAnims(rig, kind)
    cs = BA.all_clips()
    actions = [mship.bake_clip(rig, c, kind.token) for c in cs]
    res = containment_qa(kind, rig, body, cs, actions)
    print_containment(res)
    return 0 if res["pass"] else 1


# ── ship ─────────────────────────────────────────────────────────────────────────────────────────────────
CROP_HW = {"clot": 0.06, "bloat": 0.08}       # game-sheet cell half widths (fraction of 1920)


def _wide_cells() -> None:
    """``mship.game_sheet`` sizes its cells from the height (≥ 0.05 × 1920 a side): the clot's puddle and the
    bloat's slam / puddle (2.2 m wide at sx 1.82) ran off the cell and the cut edge counted as missing ink. Cells of
    a ship that sets ``crop_hw`` use it; ``mship`` is not edited, every other asset keeps its cells."""
    import mship
    if getattr(mship._crop_hw, "_af_breach", False):
        return
    orig = mship._crop_hw

    def crop_hw(ms):
        return getattr(ms, "crop_hw", None) or orig(ms)
    crop_hw._af_breach = True
    mship._crop_hw = crop_hw


def make_ship(kind: Kind):
    """The ``mship.MonsterShip`` of a breach-blood monster (built, clips authored; nothing exported)."""
    import mship
    pal, rig, body = build_all(kind)
    body1, _ = (build_clot if kind.key == "clot" else build_bloat)(pal, rig, lod=1)
    BA = BreachAnims(rig, kind)
    cs = BA.all_clips()
    cf = contact_frame(BA)
    ms = mship.MonsterShip(
        asset=kind.asset, token=kind.token, preview_dir=kind.preview_dir, pal=pal, A=_A(rig), body=body, items=[],
        clips=cs, outline_class=OUTLINE_CLASS, game_ids=[kind.game_id], blob_radius=round(kind.W * 1.05, 3),
        height=kind.H, display=kind.display, fx=fx_for(kind, BA),
        glows=fuse_glows(rig) if kind.key == "bloat" else None,
        qa_limits={"ground": -0.5},
        game_keys=GAME_KEYS[kind.key],
        contact_keys=[("Attack01", cf, f) for f in ("se", "sw", "ne", "nw")],
        close_keys=CLOSE_KEYS[kind.key],
        back_clips=("Attack01", "Death"),
        skeleton_bones_required=SLIME_BONES,
        extra={**remake_extra(kind, BA), **marker_extra(kind)},
        lod_note=(f"{kind.display['en']} budget 2.9k / LOD1 1.5k toon tris (monsters.md 5: as the slime; the "
                  f"translucent shell slot 2 included), <= 16 bones (SKEL_Slime)."),
        lod1=body1,
        outline_px=OUTLINE_PX,
        outline_px_note=OUTLINE_PX_NOTE,
        native_sheets=True,
    )
    ms.breach = BA
    ms.crop_hw = CROP_HW[kind.key]
    _wide_cells()
    return ms


def ship(kind: Kind, previews: bool = True, verify: bool = True) -> int:
    """Containment QA first (mship's verify step re-imports into an empty scene), then ``mship.ship``."""
    import mship
    ms = make_ship(kind)
    actions = [mship.bake_clip(ms.A.rig, c, ms.token) for c in ms.clips]
    cq = containment_qa(kind, ms.A.rig, ms.body, ms.clips, actions)
    print_containment(cq)
    res = mship.ship(ms, previews=previews, verify=verify)
    rc = mship.main_result(res)
    return rc or (0 if cq["pass"] else 1)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    pos = [a for a in args if not a.startswith("--")]
    which = pos[0] if pos and pos[0] in ("clot", "bloat", "all") else "all"
    rest = pos[1:] if pos and pos[0] in ("clot", "bloat", "all") else pos
    kinds = [CLOT, BLOAT] if which == "all" else [KINDS[which]]
    rc = 0
    for kd in kinds:
        scratch = scratch_root("a5", kd.preview_dir)
        if "--quick" in args:
            quick(kd, scratch, rest[0] if rest else None, int(rest[1]) if len(rest) > 1 else 0)
        elif "--game" in args:
            game_quick(kd, scratch / "game")
        elif "--anims" in args:
            fac = next((a.split("=")[1] for a in args if a.startswith("--facing=")), "se")
            anim_quick(kd, scratch / "anims", rest, fac)
        elif "--qa" in args:
            rc = max(rc, qa_only(kd))
        else:
            rc = max(rc, ship(kd, previews="--no-previews" not in args, verify="--no-verify" not in args))
    sys.exit(rc)

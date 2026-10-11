"""Townsfolk kit — the modular NPC body generator on ``SKEL_Human`` (spec art-inventory-ch1.md §5.1, DECISIONS R2).

A :class:`Look` (the web ``FolkLook``, ``src/graphics/sprites/rig/Townsfolk.ts:54-81``) + a build becomes one merged
skeletal mesh ``SK_NPC_<Id>``; every garment / hair / hat / beard is its own part generator here, so later chapters
(~30 NPC looks) add rows to ``looks.py``, not mechanisms:

* **builds** ``slim / normal / stout / dwarf`` (web ``BUILDS :87-112``: bone lengths, shoulder / hip half widths, girth,
  belly) → :class:`Dims` (a ``HumanoidSpec`` fitted to the normalised height, R2: 1.70–1.76 m, never taller than
  the warrior);
* **head** — the shared head (web ``HEAD_RINGS :559-567``, × ``HEAD_K`` 0.92: the W1 pitch −50° foreshortens the
  body, not the head — the warrior's ``HELM_K``) with modelled cartoon features: almond eye whites with pupils and
  catch-lights, brows in the hair tone, a bulbous nose, a smiling mouth line, ears with an inner bowl, cheek blush;
* **hair** ``short · long · bald · ponytail · bun · braid`` (a thick cap clipped by the web ``HAIRLINE`` — sideburns,
  the notch over the ears, the nape — + combed strands; bald = the horseshoe fringe; ponytail / long hang on a
  ``hair_tail`` chain);
* **beards** ``stubble · short · full · long · braided · mustache`` (a jaw shell from the sideburns round the chin +
  a hanging lens mass on a ``beard`` chain, the braid's gold band, a handlebar mustache);
* **hats** ``cap`` (seamed dome, visor, accent badge) · ``headband`` (knot + two tails) · ``widebrim`` (crown with a
  pinched crease, accent band, a brim tipped up at the front so the face reads from the W1 camera);
* **tops** ``tunic · vest · robe · coat`` (torso from the web ``torsoRings``; vest = shirt panel + trimmed edges; robe /
  coat = a two-sided skirt on the four ``skirt_*`` chains, placket / opening + hem trims, folds) with sleeves (bare
  arms = skin + a tunic shoulder cap);
* **layers** belt + gold buckle, apron (bib on the chest + a panel hanging to the knee, neck straps, pocket), mantle
  (capelet + clasp) or scarf (neck wrap + a tail on ``scarf_a``), travel pack (box, flap, bedroll, straps, on
  ``pack_01``), spectacles, key ring;
* hands: an open mitten, or a fist round the grip when the hand holds a prop (props are separate
  ``SM_NPCKit_Prop_*`` meshes on ``weapon_r`` / ``weapon_l``: ``props.py``).

Body space (= armature space): X = character left, −Y = forward, Z = up, metres. Profile angles ``a`` in degrees:
0 = front, +90 = character left, 180 = back. Web units (``u``) are the NPC rig unit (3.19 cm, spec §0.1) × the fit.
The web draws the 3/4 view with the face turned ~0.2 rad toward the camera; the 3D head is symmetric (render-only).
"""
from __future__ import annotations

import math
import sys
from dataclasses import dataclass, field, replace
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bmesh  # noqa: E402
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import asset, color, mesh as M, outline, rig as R  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402
from kit.rig import Bind, HumanoidSpec  # noqa: E402

import common as C  # noqa: E402
from common import Profile  # noqa: E402
import hero_base as HB  # noqa: E402
from hero_base import ell, profile_mesh  # noqa: E402

U0 = 0.0319                 # NPC rig unit, upright cm/unit 3.19 (spec §0.1: frameH 120, scale 1 → 1.25 px/unit)
HEAD_K = 0.92               # camera-aware head scale (the warrior's HELM_K)
TORSO_ADD, LEG_ADD = 0.3, 0.15
OUTLINE_CLASS = "npc"
FAMILY = "NPC"


# ── builds (web BUILDS, Townsfolk.ts:87-112) ────────────────────────────────────────────────────────────
@dataclass(frozen=True)
class Build:
    thigh: float
    shin: float
    upper_arm: float
    fore_arm: float
    torso: float
    neck: float
    ankle: float
    sh: float          # shoulder half width (view shW)
    hip: float         # hip half width (view hipW)
    girth: float
    belly: float
    height: float      # normalised crown height (m), R2


BUILDS = {
    "slim": Build(12, 12, 9.5, 9, 16, 7.2, 2.1, 4.9, 2.2, 0.88, 0.0, 1.74),
    "normal": Build(12, 11.5, 9.5, 9, 16.5, 7.2, 2.2, 5.5, 2.5, 1.0, 0.0, 1.74),
    "stout": Build(11.5, 11, 9.5, 9, 16.5, 7.0, 2.2, 6.4, 3.0, 1.2, 1.6, 1.72),
    "dwarf": Build(8.5, 8, 8.5, 8, 15, 6.8, 2.0, 6.6, 3.0, 1.25, 1.4, 1.48),
}

HEAD_TOP = 7.6              # web HEAD_RINGS top (units above the head centre)
# web HEAD_RINGS (h, a front/back, b side, f forward); 3D: a touch rounder (side × 1.03, depth × 0.96)
HEAD_RINGS = [(7.6, 1.3, 1.2, 0.0), (6.4, 4.4, 4.0, -0.3), (3.6, 6.2, 5.5, -0.2), (0.4, 6.6, 5.8, 0.0),
              (-2.6, 6.1, 5.3, 0.4), (-4.8, 4.6, 4.2, 0.9), (-6.3, 2.3, 2.2, 1.5)]
# web HAIRLINE as (|angle from the front| deg, h): brow, temple, sideburn, the notch over the ear, the nape
HAIRLINE = [(0, 3.6), (28, 3.45), (45, 3.0), (57, 2.3), (63, 0.6), (66, -0.9), (72, -0.9), (75, 1.15), (99, 1.15),
            (107, -2.3), (126, -4.4), (150, -5.0), (180, -5.1)]
EYE_H, EYE_A = 0.25, 22.0
EAR_H, EAR_A = -0.45, 82.0


@dataclass
class Look:
    """Web ``FolkLook`` (hex strings). ``hair`` / ``beard`` = (hex, style); ``hat`` = (kind, hex, accent | None);
    ``top`` = (hex, kind, trim | None)."""
    skin: str
    build: str = "normal"
    height: float | None = None
    hair: tuple | None = None
    beard: tuple | None = None
    hat: tuple | None = None
    top: tuple = ("#888888", "tunic", None)
    sleeves: str | None = None
    apron: str | None = None
    belt: str = "#4A2F1C"
    legs: str = "#4A3A2A"
    boots: str = "#2A1A0A"
    mantle: str | None = None
    scarf: bool = False            # the mantle drawn as a scarf (neck wrap + tail)
    pack: str | None = None
    pack_scale: float = 1.0
    specs: bool = False
    keyring: bool = False
    eyes: str = "#2A1C14"
    item: str = "none"             # main hand (weapon_r)
    off: str = "none"              # off hand (weapon_l)
    brow_k: float = 1.0            # bushier brows (elder)
    hat_tilt: float = 0.0          # deg the hat is pushed back (the W1 camera looks down on the brim)
    brim_k: float = 1.0            # wide-brim radius scale
    visor_k: float = 1.0           # cap visor reach scale (W1: a shorter peak keeps the eyes out of its shadow)
    lids: float = 0.0              # upper lids over the eye whites (0 open … 0.5 half-lidded)


def lerp(a, b, t):
    return a + (b - a) * t


def smooth01(x: float) -> float:
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


def piecewise(table, x: float) -> float:
    xs = [p[0] for p in table]
    if x <= xs[0]:
        return table[0][1]
    for (x0, y0), (x1, y1) in zip(table, table[1:]):
        if x <= x1:
            return lerp(y0, y1, (x - x0) / max(x1 - x0, 1e-9))
    return table[-1][1]


def mix_hex(a: str, b: str, t: float) -> str:
    return color.rgb_to_hex(color.mix(color.hex_to_rgb(a), color.hex_to_rgb(b), t))


def scale_hex(a: str, k: float) -> str:
    c = color.hex_to_rgb(a)
    return color.rgb_to_hex(tuple(max(0.0, min(255.0, x * k)) for x in c))


GOLD = Region("#D9A640", .42, .50)
LEATHER = Region("#6A4228")
BEDROLL = Region("#8A6A4A")
METAL = Region("#9AA3B2", .42, .50)
METAL_DARK = Region("#5A606E")


def regions_for(look: Look) -> dict:
    """Material regions of a look (spec §5.1 tone params: skin (s .30, l .25), hair / beard (l .35), hats (l .32),
    accents / trims (l .45), mantle (l .35)). Outline mix = the NPC ink class (60 % ink)."""
    skin = Region(look.skin, .30, .25)
    top_hex, kind, trim = look.top
    sleeves = look.sleeves or top_hex
    r = dict(
        skin=skin,
        skin_shade=Region(color.rgb_to_hex(skin.tone()["shade"]), .25, .15),
        blush=Region(mix_hex(look.skin, "#D25A46", 0.20), .30, .25),
        eye_white=Region("#FBF6EA", .22, .20),
        pupil=Region(look.eyes, .20, .10),
        eye_hi=Region("#FFFFFF", .0, .0, e=0.5),
        mouth=Region(mix_hex(color.rgb_to_hex(skin.tone()["line"]), "#7A2A2A", 0.35), .2, .1),
        top=Region(top_hex),
        top_shade=Region(top_hex).shade_region(),
        sleeve=skin if sleeves.lower() == look.skin.lower() else Region(sleeves),
        legs=Region(look.legs),
        boots=Region(look.boots),
        boot_line=Region(look.boots).shade_region(.3, .1),
        belt=Region(look.belt),
        gold=GOLD,
        leather=LEATHER,
    )
    if trim:
        r["trim"] = Region(trim, .42, .45)
    if look.hair:
        r["hair"] = Region(look.hair[0], .42, .35)
        r["hair_line"] = Region(look.hair[0], .42, .35).shade_region(.28, .12)
    if look.beard:
        bs = .2 if color.hex_to_rgb(look.beard[0])[0] > 180 else .42      # pale beards: the blade rule (mage)
        r["beard"] = Region(look.beard[0], bs, .40 if bs < .3 else .35)
        r["beard_line"] = Region(look.beard[0], .42, .35).shade_region(.28, .12)
        if look.beard[1] == "stubble":
            r["stubble"] = Region(mix_hex(look.skin, "#282020", 0.30), .30, .2)
    bro = (look.hair or look.beard or (None,))[0]
    r["brow"] = (Region(bro, .42, .35).shade_region(.28, .1) if bro else r["skin_shade"])
    if look.hat:
        hk, hx, acc = look.hat
        r["hat"] = Region(hx, .42, .50 if hk == "helm" else .32)
        r["hat_line"] = Region(hx).shade_region(.28, .12)
        if acc:
            r["hat_accent"] = Region(acc, .42, .45)
    if look.apron:
        r["apron"] = Region(look.apron)
        r["apron_line"] = Region(look.apron).shade_region(.28, .12)
    if look.mantle:
        r["mantle"] = Region(look.mantle, .42, .35)
        r["mantle_line"] = Region(look.mantle, .42, .35).shade_region(.28, .12)
    if look.pack:
        r["pack"] = Region(look.pack)
        r["pack_line"] = Region(look.pack).shade_region(.28, .12)
        r["bedroll"] = BEDROLL
        r["metal_dark"] = METAL_DARK
    if look.specs:
        r["specs"] = Region("#A8D6F2", .30, .45)
    return outline.ink_regions(r, OUTLINE_CLASS)


# ── dimensions ──────────────────────────────────────────────────────────────────────────────────────────
class Dims:
    """Rig proportions of a build fitted to the normalised height; ``U`` metres per web unit (upright), ``HK`` the
    head unit (× HEAD_K), landmark heights for the part generators (before the rig exists)."""

    def __init__(self, build: str, height: float | None = None, name: str = "SK_NPC"):
        b = BUILDS[build]
        self.b = b
        self.g = b.girth
        H = height or b.height
        base = HumanoidSpec(name=name, skeleton="SKEL_Human", thigh=(b.thigh + LEG_ADD) * U0,
                            shin=(b.shin + LEG_ADD) * U0, ankle=b.ankle * U0, torso=(b.torso + TORSO_ADD) * U0,
                            neck=b.neck * U0, head=2 * HEAD_TOP * HEAD_K * U0, hip_half=b.hip * U0,
                            shoulder_half=b.sh * U0, upper_arm=b.upper_arm * U0, fore_arm=b.fore_arm * U0,
                            hand=0.105 * (1 + 0.5 * (b.girth - 1)), foot_len=0.27 * (1 + 0.3 * (b.girth - 1)))
        self.k = H / base.crown_height()
        self.spec = base.fit_height(H)
        s = self.spec
        self.height = H
        self.U = U0 * self.k
        self.HK = self.U * HEAD_K
        self.hip_z = s.ankle + s.shin + s.thigh
        self.pz = self.hip_z + s.pelvis_offset
        self.nz = self.pz + s.torso
        self.hc = self.nz + s.neck
        self.len_u = s.torso / self.U            # torso length in (fitted) web units


# ── the generator ───────────────────────────────────────────────────────────────────────────────────────
class Folk:
    """Builds ``SK_NPC_<Id>``: ``spec()`` (rig with the look's chains) then ``build(pal, rig, lod)``."""

    def __init__(self, look: Look, asset_name: str, prefix: str):
        self.look = look
        self.asset = asset_name
        self.prefix = prefix
        self.D = Dims(look.build, look.height, asset_name)
        D = self.D
        self.U, self.HK, self.g = D.U, D.HK, D.g
        self.belly = D.b.belly
        self.hc = D.hc
        g, bel = self.g, self.belly
        L = D.len_u
        self.L = L
        hs = (1.03, 0.96)
        self.head = Profile([(self.hc + h * self.HK, b * self.HK * hs[0], a * self.HK * hs[1], -f * self.HK)
                             for h, a, b, f in HEAD_RINGS], exp=2.0)
        # web torsoRings (h above the pelvis, a front/back, b side, f forward)
        tr = [(L + 0.8, 2.2 * g, 2.9 * g, 0.0), (L - 1.2, 3.6 * g, 5.3 * g, 0.0), (L - 5.5, 3.9 * g, 5.4 * g, 0.3),
              (L * 0.4, 3.6 * g + bel, 5.0 * g + bel * 0.4, bel * 0.9), (1.2, 3.8 * g + bel * 0.4, 5.2 * g, bel * 0.4),
              (-2.4, 4.1 * g, 5.6 * g, 0.0)]
        self.torso_rings = tr
        self.torso = Profile([(self.th(h), b * self.U, a * self.U, -f * self.U) for h, a, b, f in tr], exp=2.25)
        kind = look.top[1]
        self.skirted = kind in ("robe", "coat")
        self.long_skirt = kind == "robe"
        self.hair_thick = {"wild": 1.3, "long": 0.85, "short": 0.6}.get(look.hair[1], 0.5) if look.hair else 0.0
        if look.hair and look.hair[1] == "bald":
            self.hair_thick = 0.0
        self._skirt_profile()
        self.parts_meta: dict = {}

    # unit helpers
    def u(self, n: float) -> float:
        return n * self.U

    def th(self, h: float) -> float:
        """Torso height (web units above the pelvis) → z."""
        return self.D.pz + h * self.U

    def hz(self, h: float) -> float:
        return self.hc + h * self.HK

    def hp(self, h: float, a: float, lift: float = 0.0) -> Vector:
        """Head surface point at head height ``h`` (head units), angle ``a`` (deg), ``lift`` head units out."""
        return self.head.point(self.hz(h), a, lift * self.HK)

    def hn(self, h: float, a: float) -> Vector:
        return self.head.normal(self.hz(h), a)

    def tp(self, h: float, a: float, lift: float = 0.0) -> Vector:
        return self.torso.point(self.th(h), a, lift * self.U)

    def hairline(self, a: float) -> float:
        return piecewise(HAIRLINE, abs(((a + 180.0) % 360.0) - 180.0))

    # ── skirt (robe / coat; web viewSkirt :244-281) ─────────────────────────────────────────────────────
    SKIRT_TOP_H = 0.5          # web units above the pelvis (just under the belt)
    SPREAD = 6.0               # 3D READY stance: feet ~19 cm apart front / back (web 8.5)

    def _skirt_profile(self) -> None:
        """A-line skirt: waist (under the belt) → hips → a straight flare to the hem. Robe hem 4.2 u above the ground
        (the boot toes show — web 2.8 u, raised so the figure keeps its feet at the high W1 camera), coat hem 3.8 u
        below the knee (web: knee + 4 u)."""
        if not self.skirted:
            self.skirt = None
            return
        g, U = self.g, self.U
        D = self.D
        long = self.long_skirt
        bel = self.belly
        knee_z = D.spec.ankle + D.spec.shin
        hem_z = 4.2 * U if long else max(knee_z - 3.8 * U, 6.0 * U)
        self.hem_z = hem_z
        z0 = self.th(self.SKIRT_TOP_H)
        rx0, ry0, cy0, _ = self.torso.at(z0)
        zh = self.th(-2.6)
        rxh, ryh, cyh, _ = self.torso.at(self.th(-2.4))
        hip_x = max(rxh + 0.35 * U, 5.9 * g * U)
        hip_y = ryh + 0.35 * U
        hem_x = (6.9 if long else 6.6) * g * U
        hem_y = (6.1 if long else 5.6) * g * U + 0.25 * bel * U
        zm = lerp(zh, hem_z, 0.5)
        rings = [(z0, rx0 + 0.25 * U, ry0 + 0.25 * U, cy0),
                 (zh, hip_x, hip_y, cyh * 0.8),
                 (zm, lerp(hip_x, hem_x, 0.52), lerp(hip_y, hem_y, 0.52), cyh * 0.4),
                 (hem_z, hem_x, hem_y, 0.0)]
        self.skirt_z0, self.skirt_z1 = z0, hem_z
        self.skirt = Profile([(z, rx, ry, cy) for z, rx, ry, cy in rings], exp=2.15)

    def skirt_point(self, k: float, a: float, d: float = 0.0) -> Vector:
        z = lerp(self.skirt_z0, self.skirt_z1, k)
        return self.skirt.point(z, a, d)

    # ── rig ─────────────────────────────────────────────────────────────────────────────────────────────
    def spec(self) -> HumanoidSpec:
        s = replace(self.D.spec)
        s.name = self.asset
        chains = []
        lk = self.look
        if self.skirted:
            for nm, a in (("skirt_f", 0.0), ("skirt_b", 180.0), ("skirt_l", 90.0), ("skirt_r", -90.0)):
                chains.append((nm, "pelvis", [self.skirt_point(k, a, 0.0) for k in (0.10, 0.52, 1.0)]))
        if lk.hair and lk.hair[1] in ("ponytail", "braid", "long"):
            chains.append(("hair_tail", "head", self.hair_tail_points()))
        if lk.beard and lk.beard[1] in ("long", "braided", "full"):
            chains.append(("beard", "head", self.beard_chain_points()))
        if lk.pack:
            top, bot = self.pack_axis()
            chains.append(("pack", "spine_03", [top, bot]))
        if lk.scarf and lk.mantle:
            chains.append(("scarf_a", "spine_03", self.scarf_tail_points()))
        s.chains = chains
        return s

    # chain geometry (shared by the rig and the meshes)
    def hair_tail_points(self) -> list[Vector]:
        st = self.look.hair[1]
        if st == "long":
            top = self.hp(0.6, 180.0, 0.3)
            n = 13.0
            return [top, top + Vector((0, 0.010, -n * 0.5 * self.HK)), top + Vector((0, 0.022, -n * self.HK))]
        root = self.hp(1.2, 180.0, self.hair_thick + 0.4)
        n = 12.0 if st == "braid" else 10.5
        return [root, root + Vector((0, 0.035, -n * 0.38 * self.HK)), root + Vector((0, 0.05, -n * 0.72 * self.HK)),
                root + Vector((0, 0.055, -n * self.HK))]

    def beard_len(self) -> float:
        return {"short": 0.8, "full": 3.4, "long": 9.0, "braided": 7.0}.get(self.look.beard[1], 0.0)

    def beard_chain_points(self) -> list[Vector]:
        top = self.hp(-4.6, 0.0, 0.9)
        n = self.beard_len() + (2.4 if self.look.beard[1] == "braided" else 0.0)
        p1 = self.hp(-6.1, 0.0, 1.3) + Vector((0, 0, -0.35 * n * self.HK))
        p2 = self.hp(-6.1, 0.0, 1.0) + Vector((0, 0.01, -(n + 0.4) * self.HK))
        return [top, p1, p2]

    def pack_dims(self) -> tuple[float, float, float]:
        k = self.look.pack_scale
        return 4.8 * self.g * self.U * k, 2.6 * self.U * k ** 0.8, 6.4 * self.U * k

    def pack_axis(self) -> tuple[Vector, Vector]:
        W, Dd, H = self.pack_dims()
        L = self.L
        zc = self.th(L * 0.52) + (H - 6.4 * self.U) * 0.25
        rx_, ry_, cy_, _ = self.torso.at(zc)
        back = cy_ + ry_                                          # the back surface y
        cy = back + Dd + 0.4 * self.U
        return Vector((0, cy, zc + H * 0.95)), Vector((0, cy, zc - H * 0.9))

    def scarf_tail_points(self) -> list[Vector]:
        a = 34.0
        z0 = self.th(self.L - 0.6)
        p0 = self.torso.point(z0, a, 1.6 * self.U)
        n = self.torso.normal(z0, a)
        out = Vector((n.x, n.y, 0)).normalized()
        return [p0, p0 + out * 0.012 + Vector((0, 0, -0.10)), p0 + out * 0.03 + Vector((0, 0, -0.21)),
                p0 + out * 0.05 + Vector((0.004, 0, -0.30))]

    # ── build ───────────────────────────────────────────────────────────────────────────────────────────
    LOD1_SKIP = ("eye_hi", "blush", "brow_hair", "strand", "seam", "fold", "pocket", "stitch", "rivet",
                 "beard_line", "lace", "strap_buckle", "badge_ring", "crease", "lid")

    LOD1_RATIO = 0.55

    def build(self, pal: Palette, rig: R.Rig, lod: int = 0, lod_ratio: float | None = None) -> bpy.types.Object:
        import json
        self.rig = rig
        self.lod = lod
        if lod:
            b = M.Builder(self.asset, pal, prefix=self.prefix, lod_ratio=lod_ratio or self.LOD1_RATIO,
                          lod_skip=lambda n: n.rstrip("_lr").rstrip("0123456789").startswith(self.LOD1_SKIP))
        else:
            b = M.Builder(self.asset, pal, prefix=self.prefix)
        b.regions(**regions_for(self.look))
        self.b = b
        ids = {}
        ids["skull"] = self.add_head(b)
        self.add_hair(b, ids["skull"])
        self.add_beard(b, ids["skull"])
        self.add_hat(b, ids["skull"])
        if self.look.specs:
            self.add_specs(b, ids["skull"])
        ids["torso"] = self.add_torso(b)
        if self.skirted:
            ids["skirt"] = self.add_skirt(b)
        self.add_belt(b, ids)
        if self.look.apron:
            self.add_apron(b)
        if self.look.mantle:
            if self.look.scarf:
                self.add_scarf(b)
            else:
                self.add_mantle(b)
        if self.look.pack:
            self.add_pack(b)
        if self.look.keyring:
            self.add_keyring(b, ids)
        for side in ("l", "r"):
            self.add_arm(b, rig, side)
            self.add_leg(b, rig, side)
        names = [p["name"] for p in b.parts]
        body = asset.finish_mesh(b, pal, OUTLINE_CLASS, rig=rig, grounding_height=self.D.height,
                                 name=f"{self.asset}_LOD{lod}" if lod else f"{self.asset}_Mesh")
        body["af_parts"] = json.dumps(names)
        body["af_lod"] = lod
        return body

    # ── modular kit parts (spec §5 / §15: SK_NPCKit_*) ──────────────────────────────────────────────────
    KIT_GROUPS = ("head", "hair", "beard", "hat", "specs", "top", "body", "apron", "mantle", "pack", "keyring")

    def build_kit_part(self, pal: Palette, rig: R.Rig, group: str, name: str) -> bpy.types.Object | None:
        """One part group of this look as its own skinned mesh on the look's rig (same generators as the merged
        mesh; parts that were covered by the skull's padded hull are simply no-hull trims here). Groups: head (skull,
        face, ears, neck) · hair · beard · hat · specs · top (torso, skirt, sleeves / bare arms, belt) · body
        (hands, legs, boots) · apron · mantle (or scarf) · pack · keyring."""
        import json
        self.rig = rig
        lk = self.look
        b = M.Builder(name, pal, prefix=self.prefix)
        b.regions(**regions_for(lk))
        self.b = b
        if group == "head":
            self.add_head(b)
        elif group == "hair" and lk.hair:
            self.add_hair(b, None)
        elif group == "beard" and lk.beard:
            self.add_beard(b, None)
        elif group == "hat" and lk.hat:
            self.add_hat(b, None)
        elif group == "specs" and lk.specs:
            self.add_specs(b, None)
        elif group == "top":
            ids = {"torso": self.add_torso(b)}
            if self.skirted:
                ids["skirt"] = self.add_skirt(b)
            self.add_belt(b, ids)
            for side in ("l", "r"):
                self.add_arm(b, rig, side, hand=False)
        elif group == "body":
            for side in ("l", "r"):
                self.add_hand(b, rig, side)
                self.add_leg(b, rig, side)
        elif group == "apron" and lk.apron:
            self.add_apron(b)
        elif group == "mantle" and lk.mantle:
            (self.add_scarf if lk.scarf else self.add_mantle)(b)
        elif group == "pack" and lk.pack:
            self.add_pack(b)
        elif group == "keyring" and lk.keyring:
            self.add_keyring(b, {})
        if not b.parts:
            return None
        names = [p["name"] for p in b.parts]
        obj = asset.finish_mesh(b, pal, OUTLINE_CLASS, rig=rig, grounding_height=self.D.height, name=name)
        obj["af_parts"] = json.dumps(names)
        obj["af_kit_group"] = group
        return obj

    # ── head ────────────────────────────────────────────────────────────────────────────────────────────
    def add_head(self, b: M.Builder) -> int:
        hp = self.head
        zs = [self.hz(h) for h in np.linspace(-6.3, HEAD_TOP, 18)]
        skull = profile_mesh(hp, zs, 24, cap0="fan", cap1="fan", dome0=0.35 * self.HK, dome1=0.32 * self.HK)
        proxy = profile_mesh(hp, [self.hz(h) for h in np.linspace(-6.3, HEAD_TOP, 12)], 20, cap0="fan", cap1="fan",
                             dome0=0.35 * self.HK, dome1=0.32 * self.HK)
        sid = b.add(skull, "skin", Bind.rigid("head"), name="skull", hull=proxy, hull_pad=True)
        self.add_face(b, sid)
        # neck (web: the head sits on the torso's collar ring; a short neck between collar and jaw)
        J = self.rig.joints
        n0 = J["neck"] + Vector((0, 0.006, -0.03))
        n1 = Vector((0, 0.004, self.hz(-3.2)))
        r0 = 2.35 * self.g ** 0.8 * self.U
        neck = M.tube([n0, n0.lerp(n1, 0.5), n1], [r0, r0 * 0.97, r0 * 0.95], sides=14)
        b.add(neck, "skin", Bind.blend("spine_03", "neck_01", "head", falloff=3.0), name="neck")
        return sid

    @staticmethod
    def _add_exact(b, part, *args, **kw) -> int:
        """``Builder.add`` without the LOD decimation (tiny parts whose shape is their identity: eyes)."""
        r = b.lod_ratio
        b.lod_ratio = 1.0
        try:
            return b.add(part, *args, **kw)
        finally:
            b.lod_ratio = r

    def _eye_frame(self, h: float, a: float, lift: float) -> Matrix:
        p = self.hp(h, a, lift)
        n = self.hn(h, a)
        return R.frame(p, n, (0, 0, 1))         # +Y = surface normal, +Z ≈ up, X = Y × Z

    def add_face(self, b: M.Builder, sid: int) -> None:
        lk = self.look
        HK = self.HK
        bind = Bind.rigid("head")
        for sg in (1.0, -1.0):
            a = sg * EYE_A
            fr = self._eye_frame(EYE_H, a, -0.08)
            # the eyes are never decimated (a collapsed white sinks under the skin and leaves a dark pupil blob in
            # LOD1): LOD1 builds them at a lower ring count instead
            es, er = (12, 6) if not getattr(self, "lod", 0) else (10, 4)
            white = ell((0.92 * HK, 0.44 * HK, 0.86 * HK), (0, 0, 0), sides=es, rings=er)
            self._add_exact(b, white.transform(fr), "eye_white", bind, name="eye_white", outline=False,
                            covered_by=sid)
            # pupil: toward the nose a little (the gaze converges on whoever stands in front), catch-light up-out
            px = sg * 0.10 * HK            # local +X points to the nose for sg = +1 (left eye)
            pup = ell((0.50 * HK, 0.22 * HK, 0.62 * HK), (px, 0.30 * HK, -0.04 * HK), sides=es, rings=er)
            self._add_exact(b, pup.transform(fr), "pupil", bind, name="pupil", outline=False, covered_by=sid)
            hi = ell((0.17 * HK, 0.08 * HK, 0.17 * HK), (px - sg * 0.17 * HK, 0.48 * HK, 0.22 * HK), sides=8, rings=4)
            b.add(hi.transform(fr), "eye_hi", bind, name="eye_hi", outline=False, covered_by=sid)
            if lk.lids > 0:
                # heavy upper lid (skin) over the top of the white: the elder's half-lidded calm
                lid = ell((1.0 * HK, 0.50 * HK, 0.62 * HK), (0, 0.02 * HK, (0.86 - lk.lids * 1.3 + 0.62) * HK),
                          sides=12, rings=6)
                b.add(lid.transform(fr), "skin", bind, name="lid", outline=False, covered_by=sid)
            # brow (web: a stroke in the hair shade, inner end higher)
            bk = lk.brow_k
            pts, rad = [], []
            for i in range(7):
                t = i / 6
                aa = sg * lerp(8.0, 37.0, t)
                hh = lerp(2.45, 2.0, t) + 0.25 * math.sin(math.pi * t)
                pts.append(self.hp(hh, aa, 0.22 + 0.06 * bk))
                w = (0.36 + 0.14 * math.sin(math.pi * min(1.0, t * 1.3))) * bk
                rad.append((0.17 * HK * bk ** 0.5, w * HK * (1 - 0.35 * t)))
            ups = [tuple(self.hn(2.3, sg * 20.0))] * len(pts)
            brow = M.sweep(pts, M.superellipse(1, 1, 8, 2.2), scales=rad, up=ups[0], dome0=0.1 * HK, dome1=0.1 * HK)
            self._add_exact(b, brow, "brow", bind, name="brow", outline=False, covered_by=sid)
            # ear + inner bowl (covered by the padded skull hull: inked where it forms the silhouette)
            fe = self._eye_frame(EAR_H, sg * EAR_A, 0.30)
            fe = fe @ Matrix.Rotation(math.radians(-sg * 12.0), 4, "Y")     # tops tipped back
            ear = ell((1.15 * HK, 0.45 * HK, 1.75 * HK), (0, 0, 0), sides=12, rings=8)
            b.add(ear.transform(fe), "skin", bind, name="ear", outline=False, covered_by=sid)
            bowl = ell((0.70 * HK, 0.22 * HK, 1.12 * HK), (sg * 0.10 * HK, 0.28 * HK, -0.05 * HK), sides=10, rings=6)
            b.add(bowl.transform(fe), "skin_shade", bind, name="ear_bowl", outline=False, covered_by=sid)
            # cheek blush (web rgba(210,90,70,.16) ellipse under the eye)
            fb = self._eye_frame(-2.0, sg * 37.0, -0.20)
            blush = ell((1.25 * HK, 0.24 * HK, 0.78 * HK), (0, 0, 0), sides=10, rings=4)
            b.add(blush.transform(fb), "blush", bind, name="blush", outline=False, covered_by=sid)
        # nose: a bulbous wedge standing off the face (web: root h −0.6, tip h −1.9 lifted 1.1 u)
        path = [self.hp(-0.35, 0.0, -0.15), self.hp(-1.0, 0.0, 0.55), self.hp(-1.75, 0.0, 1.05),
                self.hp(-2.25, 0.0, 0.85), self.hp(-2.55, 0.0, 0.25)]
        nose = M.tube(path, [0.50 * HK, 0.62 * HK, 0.86 * HK, 0.74 * HK, 0.42 * HK], sides=12,
                      up=tuple(self.hn(-1.5, 0.0).cross(Vector((1, 0, 0)))), dome0=0.0, dome1=0.25 * HK)
        b.add(nose, "skin", bind, name="nose", outline=False, covered_by=sid)
        for sg in (1.0, -1.0):
            wing = ell((0.55 * HK, 0.45 * HK, 0.48 * HK), self.hp(-2.15, sg * 9.0, 0.35), sides=10, rings=6)
            b.add(wing, "skin", bind, name="nose_wing", outline=False, covered_by=sid)
        # mouth: a smiling line (web quadratic, the middle 0.15 u lower)
        if not (lk.beard and lk.beard[1] in ("full", "long", "braided")):
            pts = []
            for i in range(9):
                t = -1 + 2 * i / 8
                pts.append(self.hp(-3.55 - 0.32 * (1 - t * t) + 0.12 * t * t, 19.0 * t, 0.06))
            ups = tuple(self.hn(-3.6, 0.0))
            mouth = M.sweep(pts, M.superellipse(1, 1, 6, 2.0), scales=[(0.16 * HK, 0.22 * HK)] * len(pts),
                            up=ups, dome0=0.05 * HK, dome1=0.05 * HK)
            self._add_exact(b, mouth, "mouth", bind, name="mouth", outline=False, covered_by=sid)

    # ── hair ────────────────────────────────────────────────────────────────────────────────────────────
    def head_cap(self, lo_fn, thick_fn, top_h: float = HEAD_TOP, cols: int = 44, rows: int = 8,
                 sunk: float = 0.12, a0: float = -180.0, a1: float = 180.0, inner: bool = True) -> M.Part:
        """A thick shell over the head from ``lo_fn(a)`` (head units) up to the crown, ``thick_fn(s, a)`` head units
        proud (s 0 at the lower edge … 1 at the crown), ``sunk`` below the skin inside. Closed solid; ``inner=False``
        drops the shell's inside (sunk in the skull, never seen — the hull comes from a closed proxy) and keeps the
        thick lower edge."""
        keep_inner = inner            # (the name is reused for the inner ring rows below)
        bm = bmesh.new()
        hidden = []
        full = abs((a1 - a0) - 360.0) < 1e-6
        n = cols if full else cols + 1
        angs = [a0 + (a1 - a0) * i / cols for i in range(n)]
        outer, inner = [], []
        for r in range(rows + 1):
            s = r / rows
            ro, ri = [], []
            for a in angs:
                h = lerp(lo_fn(a), top_h, s)
                ro.append(self.hp(h, a, thick_fn(s, a)))
                ri.append(self.hp(h, a, -sunk))
            outer.append(ro)
            inner.append(ri)
        vo = [[bm.verts.new(tuple(p)) for p in row] for row in outer]
        vi = [[bm.verts.new(tuple(p)) for p in row] for row in inner]
        uc = n if full else n - 1
        for r in range(rows):
            for c in range(uc):
                c1 = (c + 1) % n
                bm.faces.new((vo[r][c], vo[r][c1], vo[r + 1][c1], vo[r + 1][c]))
                hidden.append(bm.faces.new((vi[r][c], vi[r + 1][c], vi[r + 1][c1], vi[r][c1])))
        for c in range(uc):
            c1 = (c + 1) % n
            bm.faces.new((vo[0][c], vi[0][c], vi[0][c1], vo[0][c1]))
        if not full:
            for r in range(rows):
                bm.faces.new((vo[r][0], vo[r + 1][0], vi[r + 1][0], vi[r][0]))
                bm.faces.new((vo[r][-1], vi[r][-1], vi[r + 1][-1], vo[r + 1][-1]))
            for c in range(uc):
                bm.faces.new((vo[-1][c], vo[-1][c + 1], vi[-1][c + 1], vi[-1][c]))
        else:
            ztop = self.hz(top_h)
            po = bm.verts.new((0, self.head.at(ztop)[2], ztop + thick_fn(1.0, 0.0) * self.HK + 0.2 * self.HK))
            pi = bm.verts.new((0, self.head.at(ztop)[2], ztop - sunk * self.HK))
            for c in range(uc):
                c1 = (c + 1) % n
                bm.faces.new((vo[-1][c], vo[-1][c1], po))
                hidden.append(bm.faces.new((vi[-1][c1], vi[-1][c], pi)))
        bm.normal_update()
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        if not keep_inner:
            bmesh.ops.delete(bm, geom=hidden, context="FACES_ONLY")
            bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
        return M.Part(bm).smooth(True)

    def add_hair(self, b: M.Builder, sid: int) -> None:
        lk = self.look
        if not lk.hair:
            return
        style = lk.hair[1]
        bind = Bind.rigid("head")
        HK = self.HK
        if style == "bald":
            # horseshoe fringe round the back of the head (web: h −2.6 … 1.2 behind the ears)
            def lo(a):
                d = abs(((a + 180) % 360) - 180)
                return -1.4 - 0.9 * smooth01((d - 90) / 60)

            def hi_(a):
                d = abs(((a + 180) % 360) - 180)
                return 1.9 + 1.3 * smooth01((d - 85) / 80)

            def thick(s, a):
                d = abs(((a + 180) % 360) - 180)
                edge = smooth01(min(d - 74.0, 1e9) / 10.0)
                return 0.42 * math.sin(math.pi * min(1.0, max(0.0, s)) * 0.5 + 0.5 * math.pi * 0.5) * edge + 0.06
            # build as a partial ring a ∈ [76°, 284°] (the back), lower / upper edges
            cols = 40
            bm = bmesh.new()
            angs = [76.0 + (208.0 * i / cols) for i in range(cols + 1)]
            rows = 6
            vo, vi = [], []
            for r in range(rows + 1):
                s = r / rows
                ro, ri = [], []
                for a in angs:
                    h = lerp(lo(a), hi_(a), s)
                    t = 0.5 * math.sin(math.pi * s)
                    end = smooth01(min(a - 76.0, 284.0 - a) / 14.0)
                    clump = 1.0 + 0.25 * math.sin(math.radians(a) * 9.0)
                    ro.append(bm.verts.new(tuple(self.hp(h, a, (0.12 + 0.95 * t) * end * clump + 0.06))))
                    ri.append(bm.verts.new(tuple(self.hp(h, a, -0.12))))
                vo.append(ro)
                vi.append(ri)
            for r in range(rows):
                for c in range(cols):
                    bm.faces.new((vo[r][c], vo[r][c + 1], vo[r + 1][c + 1], vo[r + 1][c]))
                    bm.faces.new((vi[r][c], vi[r + 1][c], vi[r + 1][c + 1], vi[r][c + 1]))
            for c in range(cols):
                bm.faces.new((vo[0][c], vi[0][c], vi[0][c + 1], vo[0][c + 1]))
                bm.faces.new((vo[-1][c], vo[-1][c + 1], vi[-1][c + 1], vi[-1][c]))
            for r in range(rows):
                bm.faces.new((vo[r][0], vo[r + 1][0], vi[r + 1][0], vi[r][0]))
                bm.faces.new((vo[r][-1], vi[r][-1], vi[r + 1][-1], vo[r + 1][-1]))
            bm.normal_update()
            bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
            b.add(M.Part(bm).smooth(True), "hair", bind, name="hair_fringe", hull_pad=True)
            return
        th = self.hair_thick

        sleek = style in ("ponytail", "braid", "bun")

        def thick(s, a):
            d = abs(((a + 180) % 360) - 180)
            vol = 1.0 + 0.25 * smooth01((d - 60) / 100)       # fuller at the back
            clump = 1.0 + (0.12 if sleek else 0.30) * math.sin(math.radians(a) * 11.0 + 0.6) * (1 - s ** 2)
            full = th * vol * clump * ((0.05 if sleek else 0.10) + 0.90 * smooth01(s / (0.65 if sleek else 0.5))) + 0.04
            # brow, temples and sideburns: the hairline grows out of the skin (buried edge). A thick lower wall there
            # faces away from the W1 camera and its hull band lay over the cheek as a dark wedge (review: merchant)
            face = 1.0 - smooth01((d - 95.0) / 40.0)
            return lerp(lerp(full, -0.06, face), full, smooth01(s / 0.25))

        def lo(a):
            # scalloped hairline: locks at the brow and temples (web: strands combed from the brow); pulled-back
            # styles (ponytail, bun) show the forehead
            d = abs(((a + 180) % 360) - 180)
            k = 1.0 - smooth01((d - 70.0) / 30.0)
            if sleek:
                return self.hairline(a) + 0.55 * (1.0 - smooth01((d - 30.0) / 40.0))
            return self.hairline(a) - 0.45 * k * abs(math.sin(math.radians(a) * 7.0))
        top = HEAD_TOP
        if lk.hat and lk.hat[0] in ("cap", "widebrim"):
            # hidden under the dome / crown above this; a cap pushed back on the head bares the front of the hair
            top = min(HEAD_TOP, 4.6 + 0.13 * lk.hat_tilt) if lk.hat[0] == "cap" else 5.4
        # open shells are dropped at LOD0 only: collapse-decimating an open rim drags the hairline over the eyes
        # The hair is inked by the skull's padded hull (one head silhouette), not by a hull of its own: a separate
        # hair shell's hull is pushed sideways in screen space (the WPO ink) along the hairline at the temples and
        # sideburns, where the skin beside it recedes from the camera, and showed as a dark wedge from the ear to
        # the eye (review: merchant, lost traveller). Kit parts (no skull) keep their own proxy hull.
        lod = getattr(self, "lod", 0)
        cap = self.head_cap(lo, thick, top_h=top, cols=22 if lod else 34, rows=4 if lod else 6, inner=False)
        add = self._add_exact if lod else (lambda bb, *a_, **kw: bb.add(*a_, **kw))
        if sid is None:
            proxy = self.head_cap(lo, thick, top_h=top, cols=24, rows=4)
            cid = add(b, cap, "hair", bind, name="hair_cap", hull=proxy, hull_pad=True)
        else:
            add(b, cap, "hair", bind, name="hair_cap", outline=False, covered_by=sid)
            cid = sid
        if (sleek or style == "long") and not (lk.hat and lk.hat[0] in ("widebrim", "cap")):
            # centre parting (web: the strands part on the brow)
            pts = [self.hp(lerp(self.hairline(0.0) + 0.1, HEAD_TOP - 1.4, t), 0.0, thick(lerp(0.05, 0.75, t), 0.0)
                           - 0.02) for t in np.linspace(0, 1, 7)]
            b.add(M.tube(pts, [0.14 * HK] * 7, sides=4, dome0=0.0, dome1=0.0), "hair_line", bind, name="strand",
                  outline=False, covered_by=cid)
        # combed strands from the brow back over the crown (web: three shade strokes)
        # (under a wide brim the front hair is hidden: no strands — they poked through the brim; under a cap they
        # stop below its band)
        strands = (() if (lk.hat and lk.hat[0] == "widebrim") else
                   (-48.0, -24.0, 6.0, 34.0, 58.0) if not lk.hat else (-24.0, 6.0, 34.0))
        h_end = HEAD_TOP - 0.4 if not lk.hat else min(HEAD_TOP - 0.4, top - 0.5)
        for k, a0 in enumerate(strands):
            pts = []
            for i in range(9):
                t = i / 8
                h = lerp(self.hairline(a0) + 0.5, h_end, t)
                a = a0 + 30.0 * t * (1 if a0 >= 0 else -1) * 0.5 + 8.0 * t
                pts.append(self.hp(h, a, thick(lerp(0.15, 0.95, t), a) + 0.02))
            ups = tuple(self.hn(4.0, a0))
            strand = M.sweep(pts, M.superellipse(1, 1, 4, 2.0),
                             scales=[(0.08 * HK, 0.22 * HK * (1 - 0.6 * abs(2 * i / 8 - 1))) for i in range(9)],
                             up=ups, dome0=0.0, dome1=0.0)
            b.add(strand, "hair_line", bind, name="strand", outline=False, covered_by=cid)
        if style == "bun":
            c = self.hp(5.1, 180.0, th + 1.9)
            bun = ell((2.55 * HK, 2.35 * HK, 2.45 * HK), c, sides=16, rings=10)
            bun_id = b.add(bun, "hair", bind, name="hair_bun", hull_pad=True)
            n = (c - Vector((0, self.head.at(self.hz(5.1))[2], self.hz(5.1)))).normalized()
            wrap = M.tube([c - n * 1.65 * HK, c - n * 1.2 * HK], [1.9 * HK, 1.95 * HK], sides=14, dome0=0.0, dome1=0.0)
            b.add(wrap, "hair_line", bind, name="bun_wrap", outline=False, covered_by=bun_id)
        if style in ("ponytail", "braid"):
            pts = self.hair_tail_points()
            path = HB.catmull(pts, 10)
            n = len(path)
            rad = [(2.0 - 1.1 * (i / (n - 1)) ** 1.2) * HK * (0.85 if style == "braid" else 1.0) for i in range(n)]
            tail = M.tube(path, rad, sides=12, dome0=0.0, dome1=0.5 * HK)
            tb = HB.blend4(self.rig, "head", "hair_tail_01", "hair_tail_02", "hair_tail_03", falloff=3.0)
            tid = b.add(tail, "hair", tb, name="hair_tail", hull_pad=True)
            tie = M.tube([path[1] - (path[2] - path[1]) * 0.3, path[1] + (path[2] - path[1]) * 0.3],
                         [rad[1] * 1.12, rad[1] * 1.12], sides=12, dome0=0.0, dome1=0.0)
            b.add(tie, "leather", tb, name="hair_tie", outline=False, covered_by=tid)
        if style == "long":
            self._long_hair(b, cid, thick)

    def _long_hair(self, b: M.Builder, cid: int, thick) -> None:
        """Web viewHairBack 'long': a mass hanging 13 u down the back from the head's back half + a lock in front
        of each shoulder (web: the near one; 3D both)."""
        HK = self.HK
        pts = self.hair_tail_points()
        top, mid, bot = pts
        rows = 8
        cols = 12
        outer, inner = [], []
        back_y = self.torso.at(self.th(self.L - 2.0))[1] + 0.6 * self.U
        for r in range(rows + 1):
            s = r / rows
            ro, ri = [], []
            for c in range(cols + 1):
                a = 180.0 + lerp(-118.0, 118.0, c / cols)
                head_pt = self.hp(lerp(0.6, -5.6, min(1.0, s * 1.6)), a, thick(0.5, a) * (1 - 0.3 * s))
                w = math.sin(math.radians(a - 180.0))
                z = lerp(top.z, bot.z, s)
                lat = w * lerp(5.6, 4.2, s) * HK
                y_lo = max(back_y, head_pt.y * 0.0 + back_y) + (1 - abs(w)) * 0.6 * HK
                flat = Vector((lat, y_lo + 0.4 * HK * (1 - abs(w)), z))
                k = smooth01((s - 0.12) / 0.5)
                p = head_pt.lerp(flat, k)
                p.z = lerp(head_pt.z, z, k) if s > 0 else head_pt.z
                ro.append(p)
                nrm = Vector((p.x, p.y - self.head.at(self.hc)[2], 0)).normalized()
                ri.append(p - nrm * (0.9 * HK * (1 - 0.5 * s) + 0.15 * HK))
            outer.append(ro)
            inner.append(ri)
        mass = C.thick_patch(outer, inner)

        def wfn(co, top=top, bot=bot):
            k = np.clip((top.z - co[:, 2]) / max(top.z - bot.z, 1e-6), 0.0, 1.0)
            y = 1.0 - HB.smooth01(k / 0.25)
            return HB.top4(HB.chain_weights(k, 2, y))
        b.add(mass, "hair", Bind.custom(["head", "hair_tail_01", "hair_tail_02"], wfn), name="hair_back",
              hull_pad=True)
        # front locks over the shoulders
        for sg in (1.0, -1.0):
            a = sg * 74.0
            p0 = self.hp(1.4, a, self.hair_thick * 0.55)
            n0 = self.hn(1.4, a)
            p1 = p0 + Vector((sg * 0.15 * HK, 0, -3.0 * HK)) + n0 * 0.2 * HK      # lies on the head, then falls
            p2 = p1 + Vector((sg * 0.6 * HK, -0.4 * HK, -4.0 * HK))
            p3 = p2 + Vector((sg * 0.2 * HK, -0.3 * HK, -3.0 * HK))
            path = HB.catmull([p0, p1, p2, p3], 8)
            rad = [(lerp(1.15, 0.45, (i / 8) ** 1.3) * HK, lerp(0.75, 0.35, i / 8) * HK) for i in range(9)]
            lock = M.sweep(path, M.superellipse(1, 1, 10, 2.0), scales=rad, up=(sg, 0, 0), dome0=0.0,
                           dome1=0.3 * HK)
            b.add(lock, "hair", Bind.blend("head", "neck_01", falloff=6.0), name="hair_lock", hull_pad=True)

    # ── beard ───────────────────────────────────────────────────────────────────────────────────────────
    def beard_top(self, a: float) -> float:
        """Upper edge of the beard / stubble on the face (head units): sideburn at the ear, cheek, under the mouth."""
        d = abs(a)
        return piecewise([(0, -3.05), (14, -3.0), (30, -2.4), (50, -1.6), (70, -0.6), (84, 0.2)], d)

    def beard_bot(self, a: float) -> float:
        d = abs(a)
        return piecewise([(0, -6.25), (30, -6.0), (60, -5.0), (84, -3.6)], d)

    def add_beard(self, b: M.Builder, sid: int) -> None:
        lk = self.look
        if not lk.beard:
            return
        style = lk.beard[1]
        HK = self.HK
        bind = Bind.rigid("head")
        if style == "stubble":
            patch = self._jaw_shell(0.07, 0.07, sunk=0.05, cols=14, rows=3)
            b.add(patch, "stubble", bind, name="stubble", outline=False, covered_by=sid)
            return
        if style == "mustache":
            self._mustache(b, sid)
            return
        L = self.beard_len()
        thick = {"short": 0.75, "full": 1.0}.get(style, 0.95)
        jaw = self._jaw_shell(thick * 0.7, thick * 1.25, sunk=0.1)
        proxy = self._jaw_shell(thick * 0.7, thick * 1.25, sunk=0.1, cols=12, rows=3)
        has_chain = style in ("long", "braided", "full")
        bbind = (HB.blend4(self.rig, "head", "beard_01", "beard_02", falloff=3.0) if has_chain else bind)
        jid = b.add(jaw, "beard", bind, name="beard_jaw", hull=proxy, hull_pad=True)
        if L > 1.0:
            # hanging lens mass from the chin (web: the beard blob hangs len below the chin; mage beard section)
            # the spine starts inside the jaw (the mass grows out of the jaw shell — no shelf under the mustache)
            top = self.hp(-4.0, 0.0, -0.4)
            chin = self.hp(-6.0, 0.0, 1.05)
            tip = chin + Vector((0, 0.010, -L * HK))
            mid = chin.lerp(tip, 0.5) + Vector((0, -0.010, 0))
            spine = HB.catmull([top, chin, mid, tip], 14)
            n = len(spine)
            widths, depths = [], []
            for i in range(n):
                f = i / (n - 1)
                # jaw-wide where it leaves the face, bushiest just under the chin, tapering to the tip
                widths.append(piecewise([(0.0, 4.0), (0.18, 5.0), (0.45, 4.3), (0.75, 2.6), (1.0, 0.7)], f) * HK)
                depths.append(piecewise([(0.0, 0.9), (0.18, 1.9), (0.5, 1.8), (0.8, 1.2), (1.0, 0.5)], f) * HK
                              + 0.004)
            mass = M.sweep(spine, M.superellipse(1, 1, 16, 2.2), scales=[(d, w) for d, w in zip(depths, widths)],
                           up=(0, -1, 0), dome0=0.004, dome1=0.003)
            mid_id = b.add(mass, "beard", bbind, name="beard_mass", hull_pad=True)
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
                b.add(M.tube(pts, rad, sides=4, dome0=0.0, dome1=0.0), "beard_line", bbind, name="beard_line",
                      outline=False, covered_by=mid_id)
            if style == "braided":
                # the braid: three lobes down from the tip + a gold band (web: capsule 1.5 → 1 u + GOLD band)
                q0 = tip + Vector((0, -0.004, 0.6 * HK))
                q1 = tip + Vector((0, 0.004, -2.8 * HK))
                for i in range(3):
                    f0, f1 = i / 3, (i + 1) / 3
                    a, c_ = q0.lerp(q1, f0), q0.lerp(q1, f1)
                    r = lerp(1.25, 0.8, f0) * HK
                    lob = ell((r, r * 0.85, (c_ - a).length * 0.62), a.lerp(c_, 0.5), sides=12, rings=8)
                    b.add(lob, "beard", bbind, name="braid", hull_pad=True)
                band = M.tube([q0 + Vector((0, 0, 0.25 * HK)), q0 - Vector((0, 0, 0.55 * HK))],
                              [1.45 * HK, 1.4 * HK], sides=12, dome0=0.0, dome1=0.0)
                b.add(band, "gold", bbind, name="braid_band", hull_pad=True)
        self._mustache(b, sid)

    def _jaw_shell(self, lift_edge: float, lift_mid: float, sunk: float = 0.1, cols: int = 20,
                   rows: int = 5) -> M.Part:
        """Shell over the lower face between ``beard_top`` and ``beard_bot`` from ear to ear (± 86°), with a mouth
        notch kept clear; ``lift_*`` head units proud (edge → middle rows)."""
        angs = [lerp(-86.0, 86.0, i / cols) for i in range(cols + 1)]
        outer, inner = [], []
        for r in range(rows + 1):
            s = r / rows
            ro, ri = [], []
            for a in angs:
                h = lerp(self.beard_top(a), self.beard_bot(a), s)
                edge = math.sin(math.pi * s) ** 0.6
                endc = smooth01((86.0 - abs(a)) / 10.0)
                ro.append(self.hp(h, a, lerp(lift_edge * 0.4, lift_mid, edge) * (0.3 + 0.7 * endc)))
                ri.append(self.hp(h, a, -sunk))
            outer.append(ro)
            inner.append(ri)
        return C.thick_patch(outer, inner)

    def _mustache(self, b: M.Builder, sid: int) -> None:
        """Handlebar mustache: two lobes from under the nose out and down past the mouth corners."""
        HK = self.HK
        bind = Bind.rigid("head")
        for sg in (1.0, -1.0):
            pts = [self.hp(-2.62, sg * 1.0, 0.30), self.hp(-2.78, sg * 9.0, 0.34), self.hp(-3.08, sg * 17.0, 0.28),
                   self.hp(-3.45, sg * 23.0, 0.20), self.hp(-3.75, sg * 26.0, 0.14)]
            path = HB.catmull(pts, 8)
            rad = [(lerp(0.50, 0.20, (i / 8) ** 1.1) * HK, lerp(1.05, 0.34, (i / 8) ** 1.2) * HK) for i in range(9)]
            ups = [tuple(self.hn(-3.0, sg * 12.0))]
            must = M.sweep(path, M.superellipse(1, 1, 8, 2.0), scales=rad, up=ups[0], dome0=0.25 * HK,
                           dome1=0.12 * HK)
            self._add_exact(b, must, "beard", bind, name="mustache", outline=False, covered_by=sid)

    # ── hats ────────────────────────────────────────────────────────────────────────────────────────────
    def add_hat(self, b: M.Builder, sid: int) -> None:
        lk = self.look
        if not lk.hat:
            return
        if lk.hat_tilt:
            # the hat pushed back on the head: rotate every hat part about the back of the head band
            piv = Vector((0.0, self.head.at(self.hz(3.0))[1] * 0.6, self.hz(3.0)))
            m = (Matrix.Translation(piv) @ Matrix.Rotation(math.radians(-lk.hat_tilt), 4, "X")
                 @ Matrix.Translation(-piv))
            add0 = b.add

            class _B:
                swatch = b.swatch

                def add(self_, part, *a, **kw):
                    part.transform(m)
                    if kw.get("hull") is not None:
                        kw["hull"].transform(m)
                    return add0(part, *a, **kw)
            b = _B()
        kind = lk.hat[0]
        bind = Bind.rigid("head")
        HK = self.HK
        acc = "hat_accent" if lk.hat[2] else "gold"
        over = self.hair_thick + 0.32
        if kind == "cap":
            # web cap: dome to h 2.6 + a visor jutting forward + three seams + the accent badge on the brow
            def lo(a):
                return 2.4 + 0.5 * smooth01((abs(a) - 120) / 60)

            def thick(s, a):
                return over + 0.25 * math.sin(math.pi * s) + 0.3 * (1 - s) ** 3
            # (the dome's inside is never seen from above: dropped; the ink comes from a closed low proxy)
            dome = self.head_cap(lo, thick, top_h=HEAD_TOP, cols=44, rows=7, sunk=-over + 0.05,
                                 inner=bool(getattr(self, "lod", 0)))
            dproxy = self.head_cap(lo, thick, top_h=HEAD_TOP, cols=28, rows=5, sunk=-over + 0.05)
            did = b.add(dome, "hat", bind, name="cap_dome", hull=dproxy, hull_pad=True)
            # visor: a curved plate from the band forward (web radius + cos φ · 3.6)
            grid_o, grid_i = [], []
            for r in range(4):
                s = r / 3
                ro, ri = [], []
                for i in range(13):
                    a = lerp(-72.0, 72.0, i / 12)
                    reach = 3.4 * lk.visor_k * math.cos(math.radians(a)) ** 0.7 * s
                    p = self.hp(2.45, a, over + 0.1 + reach)
                    p.z -= (0.35 * s * s) * HK
                    ro.append(p + Vector((0, 0, 0.13 * HK)))
                    ri.append(p - Vector((0, 0, 0.13 * HK)))
                grid_o.append(ro)
                grid_i.append(ri)
            b.add(C.thick_patch(grid_o, grid_i), "hat", bind, name="cap_visor", hull_pad=True)
            for a in (-60.0, 0.0, 60.0, 180.0):
                pts = [self.hp(lerp(lo(a) + 0.3, HEAD_TOP - 0.2, t), a, thick(lerp(0.05, 0.95, t), a) + 0.03)
                       for t in np.linspace(0, 1, 8)]
                seam = M.tube(pts, [0.12 * HK] * len(pts), sides=5, dome0=0.0, dome1=0.0)
                b.add(seam, "hat_line", bind, name="seam", outline=False, covered_by=did)
            button = ell((0.7 * HK, 0.7 * HK, 0.35 * HK), self.hp(HEAD_TOP, 0.0, thick(1.0, 0) + 0.1), sides=10,
                         rings=6)
            b.add(button, "hat", bind, name="cap_button", outline=False, covered_by=did)
            if lk.hat[2]:
                fr = self._eye_frame(4.5, 0.0, thick(0.3, 0.0) + 0.05)
                ring = M.cylinder(1.45 * HK, 1.45 * HK, -0.05 * HK, 0.28 * HK, sides=16).rotate("x", -90)
                b.add(ring.transform(fr), "metal_dark" if "metal_dark" in b.swatch else "hat_line", bind,
                      name="badge_ring", outline=False, covered_by=did)
                coin = M.cylinder(1.0 * HK, 0.95 * HK, 0.1 * HK, 0.48 * HK, sides=16).rotate("x", -90)
                b.add(coin.transform(fr), acc, bind, name="badge", outline=False, covered_by=did)
        elif kind == "headband":
            prof = Profile([(self.hz(h), rx + (over - 0.1) * HK, ry + (over - 0.1) * HK, cy, cx)
                            for h, (rx, ry, cy, cx) in ((h, self.head.at(self.hz(h))) for h in
                                                        np.linspace(1.6, 4.0, 7))], exp=2.0)
            band = C.surface_band(prof, self.hz(2.75), 1.55 * HK, 0.32 * HK, segs=48, out0=-0.2 * HK)
            # inked by the skull's padded hull (as the hair): its own hull banded the forehead under it
            if sid is None:
                bid = b.add(band, "hat", bind, name="headband", hull_pad=True)
            else:
                b.add(band, "hat", bind, name="headband", outline=False, covered_by=sid)
            # knot at the back and two tails (web: knot at φ π − 0.5, tails 6 / 7.5 u)
            kn = prof.point(self.hz(2.75), 160.0, 0.5 * HK)
            b.add(ell((1.15 * HK, 0.85 * HK, 1.05 * HK), kn, sides=12, rings=8), "hat", bind, name="hb_knot",
                  hull_pad=True)
            for k, (dl, dd) in enumerate(((0.9, 5.6), (-0.8, 6.8))):
                p0 = kn + Vector((0, 0.004, -0.3 * HK))
                p1 = p0 + Vector((dl * HK * 0.6, 0.9 * HK, -dd * 0.5 * HK))
                p2 = p0 + Vector((dl * HK, 1.4 * HK, -dd * HK))
                path = HB.catmull([p0, p1, p2], 6)
                tail = M.sweep(path, M.superellipse(1, 1, 8, 2.6),
                               scales=[(0.22 * HK, lerp(0.75, 0.55, i / 6) * HK) for i in range(7)],
                               up=(0, 1, 0), dome0=0.0, dome1=0.1 * HK)
                b.add(tail, "hat", Bind.blend("head", "neck_01", falloff=5.0), name="hb_tail", hull_pad=True)
        elif kind == "widebrim":
            hb = 3.8
            # crown: web rings (hb+6.4, 3.6, 3.4), (hb+5.4, 5.2, 5.0), (hb+1, 6.4, 6.0), f −0.3 — kept over the hair
            rings = []
            for h, a, bb in ((hb + 0.1, 6.5, 6.05), (hb + 1.0, 6.45, 6.0), (hb + 3.2, 6.0, 5.6), (hb + 5.4, 5.2, 5.0),
                             (hb + 6.2, 3.8, 3.6), (hb + 6.45, 1.6, 1.5)):
                hz = min(h, HEAD_TOP)
                rx_h, ry_h, cy_h, _ = self.head.at(self.hz(hz))
                rx = max(bb * HK * 1.05, rx_h + over * HK) if h < HEAD_TOP else bb * HK * 1.05
                ry = max(a * HK, ry_h + over * HK) if h < HEAD_TOP else a * HK
                rings.append((self.hz(h), rx, ry, -0.3 * HK + cy_h * 0.5))
            cp = Profile(rings, exp=2.1)

            def dent(z, a):         # the pinched crease on top (web stroke) as a real dent
                hh = (z - self.hc) / HK - hb
                if hh < 4.6:
                    return 0.0
                return -0.55 * HK * smooth01((hh - 4.6) / 1.4) * math.cos(math.radians(a)) ** 2
            zs = [self.hz(hb + 0.1 + (6.45 - 0.1) * i / 10) for i in range(11)]
            crown = profile_mesh(cp, zs, 28, disp=dent, cap0="fan", cap1="fan", dome1=0.15 * HK)
            hull = profile_mesh(cp, zs[::2] + [zs[-1]], 24, cap0="fan", cap1="fan", dome1=0.15 * HK)
            crid = b.add(crown, "hat", bind, name="brim_crown", hull=hull, hull_pad=True)
            b.add(C.surface_band(cp, self.hz(hb + 1.75), 1.35 * HK, 0.18 * HK, segs=32, out0=-0.05 * HK), acc,
                  bind, name="brim_band", outline=False, covered_by=crid)
            # crease line along the dent
            pts = [cp.point(self.hz(hb + 6.2), a, -0.5 * HK) for a in (-70, -35, 0, 35, 70)]
            # brim: an annulus at the crown's base, tipped up at the front (web: + cos φ · 0.9 u) and curled at the
            # rim; outer radius 10.6 / 9.8 u (× 0.94 for the W1 camera: the face stays readable under it)
            R0x, R0y = rings[0][1] - 0.2 * HK, rings[0][2] - 0.2 * HK
            R1x, R1y = 9.8 * HK * 0.94 * lk.brim_k, 10.6 * HK * 0.94 * lk.brim_k
            cyb = rings[0][3]
            tip = 15.0 if self.look.item != "sword" else 10.0
            ro, ri = [], []
            nseg = 36
            for k in range(4):
                s = k / 3
                o_row, i_row = [], []
                for i in range(nseg):
                    a = 360.0 * i / nseg
                    ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
                    rx, ry = lerp(R0x, R1x, s), lerp(R0y, R1y, s)
                    x, y = rx * sa, cyb - ry * ca
                    lift_front = math.tan(math.radians(tip)) * max(0.0, -y + cyb) * s
                    curl = 0.55 * HK * s ** 3 * (0.6 + 0.4 * abs(sa))
                    z = self.hz(hb + 0.25) + lift_front * 0.85 + curl - 0.25 * HK * s * (1 - abs(ca))
                    th_ = lerp(0.42, 0.26, s) * HK
                    o_row.append(Vector((x, y, z + th_ / 2)))
                    i_row.append(Vector((x, y, z - th_ / 2)))
                ro.append(o_row)
                ri.append(i_row)
            brim = C.thick_patch(ro, ri, wrap_u=True)
            b.add(brim, "hat", bind, name="brim", hull_pad=True)
            del pts

    # ── spectacles (web viewSpecs: two round rims, bridge, arms to the ears) ────────────────────────────
    def add_specs(self, b: M.Builder, sid: int) -> None:
        HK = self.HK
        bind = Bind.rigid("head")
        centres = []
        for sg in (1.0, -1.0):
            fr = self._eye_frame(EYE_H + 0.05, sg * EYE_A, 0.62)
            ring = M.sweep([fr @ Vector((1.55 * HK * math.cos(t), 0.0, 1.45 * HK * math.sin(t)))
                            for t in np.linspace(0, 2 * math.pi, 24, endpoint=False)],
                           M.superellipse(1, 1, 6, 2.0), scales=[(0.17 * HK, 0.24 * HK)] * 24, closed_path=True,
                           up=tuple(fr.to_3x3() @ Vector((0, 1, 0))), cap0=None, cap1=None)
            b.add(ring, "specs", bind, name="specs_rim", outline=False, covered_by=sid)
            centres.append(fr)
            # arm back to the ear
            p0 = self.hp(EYE_H + 0.15, sg * (EYE_A + 26), 0.5)
            p1 = self.hp(EYE_H + 0.3, sg * (EAR_A - 6), 0.45)
            arm = M.tube([p0, p0.lerp(p1, 0.5), p1], [0.15 * HK] * 3, sides=6, dome0=0.0, dome1=0.05 * HK)
            b.add(arm, "specs", bind, name="specs_arm", outline=False, covered_by=sid)
        a = self.hp(EYE_H + 0.35, 7.0, 0.95)
        c = self.hp(EYE_H + 0.35, -7.0, 0.95)
        mid = self.hp(EYE_H + 0.55, 0.0, 1.15)
        bridge = M.tube([a, mid, c], [0.15 * HK] * 3, sides=6, dome0=0.0, dome1=0.0)
        b.add(bridge, "specs", bind, name="specs_bridge", outline=False, covered_by=sid)

    # ── torso ───────────────────────────────────────────────────────────────────────────────────────────
    def spine_bind(self) -> Bind:
        return Bind.blend("pelvis", "spine_01", "spine_02", "spine_03", smooth=0.03)

    def add_torso(self, b: M.Builder) -> int:
        lk = self.look
        tp = self.torso
        zs = list(np.linspace(tp.z0, tp.z1, 13))
        shell = profile_mesh(tp, zs, 24, cap0="fan", cap1="fan", dome1=0.008)
        hull = profile_mesh(tp, list(np.linspace(tp.z0, tp.z1, 10)), 24, cap0="fan", cap1="fan", dome1=0.008)
        sb = self.spine_bind()
        tid = b.add(shell, "top", sb, name="torso", hull=hull, hull_pad=True)
        L = self.L
        kind = lk.top[1]
        trim = "trim" if lk.top[2] else "top_shade"
        if kind == "vest":
            # shirt down the front under the open vest (sleeve colour), trimmed vest edges
            grid = []
            for i in range(11):
                h = lerp(L + 0.5, -2.2, i / 10)
                w = 25.0 + 4.0 * math.sin(math.pi * i / 10) - 5.0 * (i / 10)
                grid.append([tp.point(self.th(h), a, 0.0012 + 0.05 * self.U) for a in np.linspace(-w, w, 7)])
            b.add(HB.outward(HB.sheet_grid(grid), lambda c: Vector((0, tp.at(c.z)[2], c.z))), "sleeve", sb,
                  name="shirt_panel", outline=False, covered_by=tid)
            for sg in (1.0, -1.0):
                pts = []
                for i in range(11):
                    h = lerp(L + 0.5, -2.2, i / 10)
                    w = 25.0 + 4.0 * math.sin(math.pi * i / 10) - 5.0 * (i / 10)
                    pts.append(self.th(h))
                strip = HB.painted_strip(tp, 0.0, self.th(-2.2), self.th(L + 0.5), 0.9 * self.U,
                                         lambda z, a: 0.0018 + 0.1 * self.U, steps=10,
                                         a_of_z=lambda z, sg=sg: sg * (25.0 + 4.0 * math.sin(
                                             math.pi * (self.th(L + 0.5) - z) / (self.th(L + 0.5) - self.th(-2.2)))
                                             - 5.0 * (self.th(L + 0.5) - z) / (self.th(L + 0.5) - self.th(-2.2))))
                b.add(strip, trim, sb, name="vest_edge", outline=False, covered_by=tid)
            # three buttons down the shirt
            for i in range(3):
                h = L - 3.0 - i * 3.6
                b.add(C.surface_dot(tp, self.th(h), 0.0, 0.45 * self.U, 0.3 * self.U, sides=8), "gold", sb,
                      name="button", outline=False, covered_by=tid)
        else:
            # collar V and the placket / trim down the front (web: trim 0.9 u, else the shade 0.6 u)
            wv = (1.45 if kind == "coat" else 0.9) if lk.top[2] else 0.65     # a coat's trim = flat lapels
            # the opening under the V (shirt / lining in the top's shade): the V reads as a neckline, not as a cord
            # lying on the cloth (portrait review)
            vin = []
            for t in np.linspace(0.0, 1.0, 7):
                w = max(30.0 * (1.0 - t) - 2.0, 1.2)
                vin.append([tp.point(self.th(lerp(L + 0.35, L - 3.4, t)), a, 0.0012) for a in np.linspace(-w, w, 5)])
            b.add(HB.outward(HB.sheet_grid(vin), lambda c: Vector((0, tp.at(c.z)[2], c.z))),
                  "sleeve" if (lk.sleeves and lk.sleeves.lower() not in (lk.skin.lower(), lk.top[0].lower()))
                  else "top_shade", sb, name="neck_opening", outline=False, covered_by=tid)
            for sg in (1.0, -1.0):
                pts = [tp.point(self.th(lerp(L + 0.35, L - 3.4, t)), sg * lerp(30.0, 0.0, t) ** 1.0, 0.0018)
                       for t in np.linspace(0, 1, 8)]
                ups = [tuple(tp.normal(p.z, sg * 15.0)) for p in pts]
                v = M.sweep(pts, M.rect_profile(0.0035, wv * self.U), up=ups[3], cap0="ngon", cap1="ngon")
                b.add(v, trim, sb, name="collar", outline=False, covered_by=tid)
            if lk.top[2]:
                b.add(HB.painted_strip(tp, 0.0, self.th(-2.3), self.th(L - 3.4), 0.85 * self.U,
                                       lambda z, a: 0.0018, steps=10), "trim", sb, name="placket", outline=False,
                      covered_by=tid)
            if kind == "tunic" and lk.top[2]:
                b.add(C.surface_band(tp, self.th(-1.9), 0.8 * self.U, 0.0022, segs=40, out0=-0.001), "trim", sb,
                      name="tunic_hem", outline=False, covered_by=tid)
        # side folds under the arms (web: shade strokes at φ 1.35)
        for sg in (1.0, -1.0):
            b.add(HB.painted_strip(tp, sg * 78.0, self.th(0.5), self.th(L - 4.0), 0.5 * self.U,
                                   lambda z, a: 0.0015, steps=8), "top_shade", sb, name="fold", outline=False,
                  covered_by=tid)
        return tid

    # ── skirt (robe / coat) ─────────────────────────────────────────────────────────────────────────────
    SKIRT_THICK = 0.009

    def skirt_bones(self) -> list[str]:
        return ["pelvis", "thigh_l", "thigh_r"] + [f"skirt_{c}_{i:02d}" for c in "fblr" for i in (1, 2)]

    def skirt_weights(self, co: np.ndarray) -> np.ndarray:
        """The mage's skirt weights: round the skirt the four chains blend by angle, down it bone → bone; the waist
        band stays on the pelvis with a touch of each thigh near the hip."""
        z0, z1 = self.skirt_z0, self.skirt_z1
        bones = self.skirt_bones()
        k = np.clip((z0 - co[:, 2]) / (z0 - z1), 0.0, 1.0)
        a = np.degrees(np.arctan2(co[:, 0], -co[:, 1]))
        yoke = 1.0 - HB.smooth01(k / 0.13)
        W = np.zeros((len(co), len(bones)))
        thigh = 0.18 * np.exp(-((k - 0.10) / 0.08) ** 2)
        W[:, 0] = yoke * (1 - thigh)
        W[:, 1] = yoke * thigh * (co[:, 0] > 0)
        W[:, 2] = yoke * thigh * (co[:, 0] <= 0)
        lat = {}
        for c, ac in (("f", 0.0), ("l", 90.0), ("b", 180.0), ("r", -90.0)):
            d = np.abs((a - ac + 180.0) % 360.0 - 180.0)
            lat[c] = HB.smooth01(1.0 - d / 90.0)
        tot = sum(lat.values())
        q = np.clip(k * 2 - 0.5, 0.0, 1.0)
        for c in "fblr":
            wc = (1.0 - yoke) * lat[c] / np.maximum(tot, 1e-6)
            W[:, bones.index(f"skirt_{c}_01")] += wc * (1.0 - q)
            W[:, bones.index(f"skirt_{c}_02")] += wc * q
        return HB.top4(W)

    def add_skirt(self, b: M.Builder) -> int:
        lk = self.look
        rows = [i / 9 for i in range(10)]
        shell, hull = HB.shell_ring(lambda k, uu, d: self.skirt_point(k, uu * 360.0 - 180.0, d), rows, 24,
                                    self.SKIRT_THICK, lens=0.002 / 0.75)
        bind = Bind.custom(self.skirt_bones(), self.skirt_weights)
        inside = "top_shade"
        sid = b.add(shell, "top", bind, name="skirt", sub_regions={1: "top", 2: inside}, hull=hull, hull_pad=True)
        t = self.SKIRT_THICK / 2
        ks = [0.02 + 0.98 * i / 14 for i in range(15)]
        trim = "trim" if lk.top[2] else "top_shade"
        # front opening (coat) / placket (robe)
        edge = [[self.skirt_point(k, da, t + 0.0016) for da in (-1.6, 0.0, 1.6)] for k in ks]
        b.add(HB.outward(HB.sheet_grid(edge), lambda c: Vector((0, 0, c.z))), trim, bind, name="skirt_edge",
              outline=False, covered_by=sid)
        if lk.top[2]:
            hk = [0.955, 0.978, 1.0]
            o2 = [[self.skirt_point(k, -180.0 + 360.0 * c / 24, t + 0.0016) for c in range(24)] for k in hk]
            i2 = [[self.skirt_point(k, -180.0 + 360.0 * c / 24, -t - 0.0016) for c in range(24)] for k in hk]
            b.add(C.thick_patch(o2, i2, wrap_u=True), "trim", bind, name="hem", outline=False, covered_by=sid)
        for a, k0 in ((29.0, 0.12), (-29.0, 0.12), (64.0, 0.25), (-64.0, 0.25), (150.0, 0.2), (-150.0, 0.2)):
            strip = [[self.skirt_point(k, a + da, t + 0.0012) for da in (-0.9, 0.0, 0.9)]
                     for k in [k0 + (0.94 - k0) * i / 10 for i in range(11)]]
            b.add(HB.outward(HB.sheet_grid(strip), lambda c: Vector((0, 0, c.z))), "top_shade", bind, name="fold",
                  outline=False, covered_by=sid)
        return sid

    # ── belt ────────────────────────────────────────────────────────────────────────────────────────────
    def belt_profile(self) -> Profile:
        rings = []
        for h in np.linspace(-0.6, 2.6, 7):
            z = self.th(h)
            rx, ry, cy, cx = self.torso.at(z)
            if self.skirted and z <= self.skirt_z0 + 0.02:
                srx, sry, scy, _ = self.skirt.at(min(z, self.skirt_z0))
                rx, ry = max(rx, srx + self.SKIRT_THICK), max(ry, sry + self.SKIRT_THICK)
            if self.look.apron:
                ry += 0.35 * self.U
                rx += 0.12 * self.U
            rings.append((z, rx, ry, cy, cx))
        return Profile(rings, exp=2.25)

    def add_belt(self, b: M.Builder, ids: dict) -> None:
        bp = self.belt_profile()
        z = self.th(1.1)
        sb = self.spine_bind() if not self.skirted else Bind.blend("pelvis", "spine_01", smooth=0.04)
        belt = C.surface_band(bp, z, 1.75 * self.U, 0.42 * self.U, segs=32, out0=-0.2 * self.U)
        bid = b.add(belt, "belt", sb, name="belt", hull_pad=True)
        self.belt_id = bid
        # gold buckle (web 2.4 × 2.6 u square, a touch to the near side)
        p = bp.point(z, -6.0, 0.42 * self.U)
        n = bp.normal(z, -6.0)
        fr = R.frame(p, n, (0, 0, 1))
        frame = M.box((2.5 * self.U, 0.32 * self.U, 2.3 * self.U), (0, 0.10 * self.U, 0), bevel=0.0025)
        b.add(frame.transform(fr), "gold", sb, name="buckle", outline=False, covered_by=bid)
        inner = M.box((1.3 * self.U, 0.34 * self.U, 1.15 * self.U), (0, 0.16 * self.U, 0), bevel=0.0015)
        b.add(inner.transform(fr), "belt", sb, name="buckle_in", outline=False, covered_by=bid)

    # ── apron (web viewApron :361-391) ──────────────────────────────────────────────────────────────────
    def add_apron(self, b: M.Builder) -> None:
        tp = self.torso
        L = self.L
        U = self.U
        sb = self.spine_bind()
        lift = 0.35 * U
        th_ = 0.006
        # bib: chest (h L − 3.2, ±30°) widening to the belt (h 1, ±54°)
        rows = 9
        outer, inner = [], []
        for r in range(rows + 1):
            s = r / rows
            h = lerp(L - 3.2, 0.6, s)
            w = lerp(30.0, 56.0, s ** 1.3)
            ro, ri = [], []
            for a in np.linspace(-w, w, 9):
                ro.append(tp.point(self.th(h), a, lift + th_))
                ri.append(tp.point(self.th(h), a, lift))
            outer.append(ro)
            inner.append(ri)
        bib = C.thick_patch(outer, inner, sub_tags=True)
        aid = b.add(bib, "apron", sb, name="apron_bib", sub_regions={1: "apron", 2: "apron_line"}, hull_pad=True)
        # skirt panel: from the belt line down to the knee, hanging plumb in front of the thighs, slight flare
        bp = self.belt_profile()
        zb = self.th(0.9)
        drop = 11.5 * U * (1.0 + 0.1 * (self.g - 1))
        J = self.rig.joints
        knee_y = J["knee_l"].y - 3.6 * self.g * U
        cols = 11
        rows = 8
        outer, inner = [], []
        top = [bp.point(zb, a, 0.15 * U) for a in np.linspace(-58.0, 58.0, cols)]
        front_y = min(p.y for p in top)
        for r in range(rows + 1):
            s = r / rows
            ro, ri = [], []
            for c, p0 in enumerate(top):
                u = c / (cols - 1) * 2 - 1
                y = lerp(p0.y, min(front_y * 0.55 + p0.y * 0.45, knee_y - 0.012), smooth01(s / 0.45))
                y -= 0.010 * s * s * (1 - u * u)
                x = p0.x * (1 + 0.10 * s)
                z = zb - drop * s + 0.25 * U * (1 - u * u) * s ** 3
                ro.append(Vector((x, y - th_ * 0.5, z)))
                ri.append(Vector((x, y + th_ * 0.5, z)))
            outer.append(ro)
            inner.append(ri)
        panel = C.thick_patch(outer, inner, sub_tags=True)
        z_top = zb

        def wfn(co, z_top=z_top, drop=drop):
            k = np.clip((z_top - co[:, 2]) / drop, 0.0, 1.0)
            side = np.clip(co[:, 0] / (5.0 * self.g * U) * 0.5 + 0.5, 0.0, 1.0)
            leg = 0.45 * HB.smooth01(k / 0.9)
            W = np.zeros((len(co), 3))
            W[:, 0] = 1.0 - leg
            W[:, 1] = leg * side
            W[:, 2] = leg * (1.0 - side)
            return HB.top4(W)
        pb = Bind.custom(["pelvis", "thigh_l", "thigh_r"], wfn)
        pid = b.add(panel, "apron", pb, name="apron_panel", sub_regions={1: "apron", 2: "apron_line"},
                    hull_pad=True)
        # pocket (web strokeRect on the lower apron)
        k0, k1 = 0.18, 0.48
        pk = []
        for r in range(3):
            s = lerp(k0, k1, r / 2)
            pk.append([outer[int(round(s * rows))][c] + Vector((0, -0.0022, 0)) for c in (3, 4, 5, 6, 7)])
        b.add(HB.outward(HB.sheet_grid(pk), lambda c: c + Vector((0, 1, 0))), "apron_line", pb, name="pocket",
              outline=False, covered_by=pid)
        # neck straps from the bib's top corners over the shoulders to the back of the neck
        for sg in (1.0, -1.0):
            p0 = tp.point(self.th(L - 3.3), sg * 27.0, lift + th_ * 0.5)
            p1 = tp.point(self.th(L - 0.2), sg * 48.0, 0.3 * U)
            p2 = tp.point(self.th(L + 0.7), sg * 95.0, 0.15 * U)
            p3 = tp.point(self.th(L + 0.3), sg * 150.0, 0.15 * U)
            path = HB.catmull([p0, p1, p2, p3], 8)
            ups = [tuple(tp.normal(p1.z, sg * 60.0))] * len(path)
            strap = M.sweep(path, M.rect_profile(0.0042, 0.9 * U), up=ups[0], cap0="ngon", cap1="ngon")
            b.add(strap, "apron_line", Bind.blend("spine_02", "spine_03", "neck_01", falloff=4.0), name="apron_strap",
                  hull_pad=True)
        del aid

    # ── mantle / scarf (web mantle rings :408-421) ──────────────────────────────────────────────────────
    def mantle_profile(self) -> Profile:
        g, bel, L = self.g, self.belly, self.L
        rings = [(L + 1.7, 2.9 * g, 3.7 * g, 0.0), (L + 0.4, 4.3 * g, 5.9 * g, 0.0), (L - 0.6, 4.95 * g, 6.75 * g, 0.0),
                 (L - 2.6, 5.15 * g + 0.2 * bel, 7.05 * g, 0.1), (L - 5.0, 5.3 * g + 0.4 * bel, 7.2 * g, 0.3)]
        return Profile([(self.th(h), b * self.U, a * self.U, -f * self.U) for h, a, b, f in rings], exp=2.2)

    def add_mantle(self, b: M.Builder) -> None:
        mp = self.mantle_profile()
        zs = list(np.linspace(mp.z0, mp.z1, 9))
        mb = HB.blend4(self.rig, "spine_02", "spine_03", "clavicle_l", "clavicle_r", "neck_01", falloff=5.0,
                       smooth=0.02)
        mid = b.add(profile_mesh(mp, zs, 30, cap0="fan", cap1="fan", dome0=-0.03), "mantle", mb, name="mantle",
                    hull=profile_mesh(mp, list(np.linspace(mp.z0, mp.z1, 7)), 26, cap0="fan", cap1="fan",
                                      dome0=-0.03), hull_pad=True)
        b.add(C.surface_band(mp, mp.z0 + 0.6 * self.U, 0.85 * self.U, 0.0026, segs=32, out0=-0.002),
              "trim" if self.look.top[2] else "mantle_line", mb, name="mantle_edge", outline=False, covered_by=mid)
        if self.look.top[2]:
            cz = self.th(self.L - 0.9)
            p = mp.point(cz, 0.0, 0.0)
            n = mp.normal(cz, 0.0)
            fr = R.frame(p, n, (0, 0, 1))
            clasp = M.cylinder(1.25 * self.U, 1.1 * self.U, -0.003, 0.45 * self.U, sides=16).rotate("x", -90)
            b.add(clasp.transform(fr), "gold", mb, name="clasp", outline=False, covered_by=mid)

    def add_scarf(self, b: M.Builder) -> None:
        """The wanderer's red scarf (web mantle #9A2A2A): a fat wrap round the collar + a tail down the chest."""
        U, L = self.U, self.L
        tp = self.torso
        z = self.th(L + 0.2)
        ring = []
        for i in range(24):
            a = 360.0 * i / 24
            z_i = z + 0.6 * U * math.cos(math.radians(a))          # lower at the back, higher at the front
            ring.append(tp.point(z_i, a, 0.9 * U))
        wrap = M.sweep(ring, M.superellipse(1, 1, 8, 2.0), scales=[(1.25 * U, 1.05 * U)] * 24, closed_path=True,
                       up=(0, 0, 1), cap0=None, cap1=None)
        mb = HB.blend4(self.rig, "spine_03", "neck_01", "clavicle_l", "clavicle_r", falloff=5.0, smooth=0.02)
        wid = b.add(wrap, "mantle", mb, name="scarf_wrap", hull_pad=True)
        knot = ell((1.3 * U, 0.9 * U, 1.1 * U), tp.point(self.th(L - 0.6), 34.0, 1.9 * U), sides=12, rings=8)
        b.add(knot, "mantle", mb, name="scarf_knot", hull_pad=True)
        pts = self.scarf_tail_points()
        path = HB.catmull(pts, 10)
        outer, inner = [], []
        for i, p in enumerate(path):
            f = i / (len(path) - 1)
            n = tp.normal(max(min(p.z, tp.z1), tp.z0), 34.0)
            out = Vector((n.x, n.y, 0)).normalized()
            tang = out.cross(Vector((0, 0, 1))).normalized()
            w = lerp(1.1, 1.5, f) * U
            row_o = [p + tang * (s * w) + out * 0.0035 for s in (-1.0, -0.33, 0.33, 1.0)]
            row_i = [p + tang * (s * w) - out * 0.0035 for s in (-1.0, -0.33, 0.33, 1.0)]
            if f > 0.97:
                row_o = [q + Vector((0, 0, 0.012 * (1 - abs(s)))) for q, s in zip(row_o, (-1, -.33, .33, 1))]
                row_i = [q + Vector((0, 0, 0.012 * (1 - abs(s)))) for q, s in zip(row_i, (-1, -.33, .33, 1))]
            outer.append(row_o)
            inner.append(row_i)
        tail = C.thick_patch(outer, inner, sub_tags=True)

        def wfn(co, top=pts[0], bot=pts[-1]):
            k = np.clip((top.z - co[:, 2]) / (top.z - bot.z), 0.0, 1.0)
            y = 1.0 - HB.smooth01(k / 0.1)
            return HB.top4(HB.chain_weights(k, 3, y))
        b.add(tail, "mantle", Bind.custom(["spine_03", "scarf_a_01", "scarf_a_02", "scarf_a_03"], wfn),
              name="scarf_tail", sub_regions={1: "mantle", 2: "mantle_line"}, hull_pad=True)
        del wid

    # ── travel pack (web viewPack :395-449) ─────────────────────────────────────────────────────────────
    def add_pack(self, b: M.Builder) -> None:
        U = self.U
        W, Dd, H = self.pack_dims()
        top, bot = self.pack_axis()
        c = top.lerp(bot, 0.5)
        k = self.look.pack_scale
        pb = Bind.rigid("pack_01")
        box = M.box((2 * W, 2 * Dd, 2 * H), c, bevel=0.022 * k ** 0.5, segments=1)
        pid = b.add(box, "pack", pb, name="pack", hull_pad=True)
        # flap over the top third of the back face (leather) + a buckle strap
        flap = M.box((2 * W + 0.006, 0.012, H * 0.8), c + Vector((0, Dd + 0.004, H * 0.62)), bevel=0.005)
        b.add(flap, "leather", pb, name="pack_flap", outline=False, covered_by=pid)
        b.add(M.box((0.9 * U, 0.016, H * 0.55), c + Vector((0, Dd + 0.008, H * 0.2)), bevel=0.002), "pack_line",
              pb, name="strap_buckle", outline=False, covered_by=pid)
        b.add(M.box((1.0 * U, 0.02, 0.9 * U), c + Vector((0, Dd + 0.011, H * 0.0)), bevel=0.002), "gold", pb,
              name="strap_buckle", outline=False, covered_by=pid)
        # side pocket on the left
        b.add(M.box((0.016 + 0.4 * U, Dd * 1.3, H * 0.75), c + Vector((W + 0.2 * U, 0.004, -H * 0.3)),
                    bevel=0.006), "pack_line", pb, name="pack_pocket", hull_pad=True)
        # bedroll across the top (web capsule r 1.9 u, the width + 0.6 u each side, two leather ties)
        rr = 2.0 * U * k ** 0.6
        zr = c.z + H + rr * 0.85
        a = Vector((-(W + 0.8 * U), c.y - 0.15 * U, zr))
        e = Vector((W + 0.8 * U, c.y - 0.15 * U, zr))
        roll = M.capsule(a, e, rr, rr, sides=14, cap_rings=2, mid=3, up=(0, 0, 1))
        rid = b.add(roll, "bedroll", pb, name="bedroll", hull_pad=True)
        for f in (0.3, 0.7):
            q = a.lerp(e, f)
            tie = M.tube([q - Vector((0.6 * U, 0, 0)), q + Vector((0.6 * U, 0, 0))], [rr * 1.08, rr * 1.08],
                         sides=14, dome0=0.0, dome1=0.0)
            b.add(tie, "leather", pb, name="lace", outline=False, covered_by=rid)
        if k > 1.15:
            # the peddler's extras: a cooking pot hanging off the right side, a rolled rug under the bedroll
            pot_c = c + Vector((-(W + 1.5 * U), 0.002, -H * 0.15))
            pot = M.lathe([(1.4 * U, -1.2 * U), (1.75 * U, -0.6 * U), (1.85 * U, 0.6 * U), (1.95 * U, 1.0 * U)],
                          sides=14, cap0="fan", cap1="fan").translate(pot_c)
            b.add(pot, "metal_dark", pb, name="pack_pot", hull_pad=True)
            rug_a = Vector((-(W + 0.3 * U), c.y + Dd * 0.2, c.z - H - 1.2 * U))
            rug_e = Vector((W + 0.3 * U, c.y + Dd * 0.2, c.z - H - 1.2 * U))
            b.add(M.capsule(rug_a, rug_e, 1.25 * U, 1.25 * U, sides=12, cap_rings=1, mid=2, up=(0, 0, 1)),
                  "pack_line", pb, name="pack_rug", hull_pad=True)
        # shoulder straps (web: two leather strokes over the shoulders, φ −0.5 / 0.95)
        tp = self.torso
        L = self.L
        for sg in (1.0, -1.0):
            pts = [tp.point(self.th(L - 8.5), sg * 30.0, 0.35 * U), tp.point(self.th(L - 4.0), sg * 36.0, 0.42 * U),
                   tp.point(self.th(L - 0.4), sg * 50.0, 0.40 * U), tp.point(self.th(L + 0.6), sg * 105.0, 0.30 * U),
                   tp.point(self.th(L - 1.5), sg * 150.0, 0.45 * U)]
            path = HB.catmull(pts, 10)
            ups = [tuple(tp.normal(max(min(p.z, tp.z1), tp.z0), sg * 70.0)) for p in path]
            strap = M.sweep(path, M.rect_profile(0.006, 1.2 * U), up=ups[4], cap0="ngon", cap1="ngon")
            b.add(strap, "leather", Bind.blend("spine_02", "spine_03", "clavicle_l" if sg > 0 else "clavicle_r",
                                               falloff=4.0), name="pack_strap", hull_pad=True)

    # ── key ring at the hip (stash keeper) ──────────────────────────────────────────────────────────────
    def add_keyring(self, b: M.Builder, ids: dict) -> None:
        U = self.U
        bp = self.belt_profile()
        z = self.th(0.5)
        p = bp.point(z, -62.0, 0.9 * U)
        n = bp.normal(z, -62.0)
        fr = R.frame(p, Vector((0, 0, -1)), n)
        ring = M.sweep([fr @ Vector((1.2 * U * math.cos(t), 1.2 * U * math.sin(t) + 1.2 * U, 0))
                        for t in np.linspace(0, 2 * math.pi, 16, endpoint=False)],
                       M.superellipse(1, 1, 6, 2.0), scales=[(0.28 * U, 0.28 * U)] * 16, closed_path=True,
                       up=tuple(n), cap0=None, cap1=None)
        kb = Bind.blend("pelvis", "thigh_r", falloff=6.0)
        rid = b.add(ring, "gold", kb, name="keyring", hull_pad=True)
        for k, (dx, ln) in enumerate(((-0.5, 4.8), (0.6, 4.0))):
            top = fr @ Vector((dx * U, 2.2 * U, 0.0))
            bot = top + Vector((0, 0, -ln * U))
            shaft = M.tube([top, bot], [0.32 * U, 0.32 * U], sides=6, dome0=0.0, dome1=0.0)
            kid = b.add(shaft, "gold", kb, name="key", hull_pad=True)
            bit = M.box((0.25 * U, 1.3 * U, 0.9 * U), bot + n * 0.6 * U + Vector((0, 0, 0.6 * U)), bevel=0.001)
            b.add(bit, "gold", kb, name="key", outline=False, covered_by=kid)
            bow = ell((0.95 * U, 0.35 * U, 0.95 * U), top + Vector((0, 0, 0.0)), sides=10, rings=6)
            b.add(bow, "gold", kb, name="key", outline=False, covered_by=rid)

    # ── arms / hands / legs ─────────────────────────────────────────────────────────────────────────────
    def add_arm(self, b: M.Builder, rig: R.Rig, side: str, hand: bool = True) -> None:
        J = rig.joints
        g = self.g
        U = self.U
        S, E, W = J[f"shoulder_{side}"], J[f"elbow_{side}"], J[f"wrist_{side}"]
        ua, la, cl = f"upperarm_{side}", f"lowerarm_{side}", f"clavicle_{side}"
        dua = (E - S).normalized()
        dla = (W - E).normalized()
        bare = self.look.sleeves and self.look.sleeves.lower() == self.look.skin.lower()
        k = 0.92
        r_s, r_e, r_w = 2.9 * g * U * k, 2.5 * g * U * k, 2.15 * g * U * k
        pts = [S - dua * 0.045, S.lerp(E, 0.5), E, E.lerp(W, 0.5), W - dla * 0.01]
        rads = [r_s, (r_s + r_e) / 2 * 1.02, r_e, (r_e + r_w) / 2, r_w]
        arm = M.tube(pts, rads, sides=16, dome0=r_s * 0.6, dome1=r_w * 0.4)
        bind = Bind.blend(cl, ua, la, f"hand_{side}", falloff=5.0)
        aid = b.add(arm, "sleeve", bind, name=f"arm_{side}", hull_pad=True)
        if bare:
            # the sleeveless tunic's shoulder cap (web: the near arm starts in the top colour)
            cap = M.tube([S - dua * 0.05, S + dua * 0.035, S + dua * 0.075],
                         [r_s * 1.08, r_s * 1.07, r_s * 1.03], sides=16, dome0=r_s * 0.5, dome1=0.0)
            b.add(cap, "top", Bind.blend(cl, ua, falloff=5.0), name=f"sleeve_cap_{side}", hull_pad=True)
        elif self.skirted or self.look.top[1] == "coat":
            # turned-back cuff at the wrist
            cuff = M.tube([W - dla * 0.045, W - dla * 0.004], [r_w * 1.13, r_w * 1.16], sides=16, dome0=0.0,
                          dome1=0.0)
            b.add(cuff, "trim" if self.look.top[2] else "top_shade", Bind.blend(la, f"hand_{side}", falloff=6.0),
                  name=f"cuff_{side}", outline=False, covered_by=aid)
        if hand:
            self.add_hand(b, rig, side)

    def holds(self, side: str) -> str:
        return self.look.item if side == "r" else self.look.off

    def add_hand(self, b: M.Builder, rig: R.Rig, side: str) -> None:
        J = rig.joints
        W, T, G = J[f"wrist_{side}"], J[f"handtip_{side}"], J[f"grip_{side}"]
        hd = (T - W).normalized()
        hbn = f"hand_{side}"
        sz = (1.0 + 0.45 * (self.g - 1.0)) * 1.05
        wr = M.tube([W - hd * 0.03, W + hd * 0.016], [0.032 * sz, 0.034 * sz], sides=12)
        b.add(wr, "skin", Bind.blend(f"lowerarm_{side}", hbn, falloff=6.0), name=f"wrist_{side}")
        if self.holds(side) != "none":
            grip = Vector((0, -1, 0))
            zx = (grip - hd * grip.dot(hd)).normalized()
            xx = hd.cross(zx)
            fr = Matrix.Identity(4)
            for i in range(3):
                fr[i][0], fr[i][1], fr[i][2], fr[i][3] = xx[i], hd[i], zx[i], G[i]
            fist = ell((0.044 * sz, 0.050 * sz, 0.047 * sz), (0, 0.006, 0), sides=14, rings=8, exp=2.5, exp_v=2.3)
            fid = b.add(fist.transform(fr), "skin", Bind.rigid(hbn), name=f"hand_{side}", hull_pad=True)
            sg = 1.0 if side == "r" else -1.0
            thumb = M.capsule((sg * 0.028 * sz, -0.012 * sz, 0.024 * sz), (sg * 0.014 * sz, 0.026 * sz, 0.038 * sz),
                              0.015 * sz, 0.012 * sz, sides=8)
            b.add(thumb.transform(fr), "skin", Bind.rigid(hbn), name=f"thumb_{side}", outline=False, covered_by=fid)
        else:
            mit = M.mitten(0.112 * sz, 0.082 * sz, 0.042 * sz, side=side, curl_deg=18.0)
            fr = R.frame(W - hd * 0.012, hd, Vector((0, -1, 0)))
            b.add(mit.transform(fr), "skin", Bind.rigid(hbn), name=f"hand_{side}", hull_pad=True)

    def add_leg(self, b: M.Builder, rig: R.Rig, side: str) -> None:
        J = rig.joints
        g = self.g
        U = self.U
        Hp, Kn, A = J[f"hip_{side}"], J[f"knee_{side}"], J[f"ankle_{side}"]
        th_, ca, ft, bl = f"thigh_{side}", f"calf_{side}", f"foot_{side}", f"ball_{side}"
        k = 0.92
        top = Hp + Vector((0, 0, -0.03 if not self.skirted else -0.06))
        pts = [top, top.lerp(Kn, 0.5), Kn, Kn.lerp(A, 0.5), A + Vector((0, 0, 0.05))]
        rads = [3.3 * g * U * k, 3.05 * g * U * k, 2.8 * g * U * k, 2.45 * g * U * k, 2.2 * g * U * k]
        if self.skirted:          # hidden under the skirt: slimmer thighs keep the cloth envelope near the waist calm
            rads[:3] = [2.55 * g * U * k, 2.45 * g * U * k, 2.4 * g * U * k]
        b.add(M.tube(pts, rads, sides=14), "legs", Bind.blend("pelvis", th_, ca, ft, falloff=5.0), name=f"leg_{side}")
        # boot: the web shaft from mid-shin (r 2.6 → 2.4 g u) + the chunky foot
        ftl = self.D.spec.foot_len
        bw = 2.35 * g ** 0.7 * U * 2 * k
        boot = M.boot(ftl, bw, 0.092 * (1 + 0.2 * (g - 1)), shaft=0.0, toe_up=0.022)
        bid = b.add(boot.translate((A.x, 0, 0)), "boots", Bind.blend(ca, ft, bl, falloff=5.0), name=f"boot_{side}",
                    hull_pad=True)
        z_top = A.z + 0.5 * (Kn.z - A.z)
        shaft = M.loft_z([(0.035, 2.45 * g * U * k + 0.004, 2.3 * g * U * k + 0.004, 0, 0.006),
                          (z_top - 0.02, 2.62 * g * U * k + 0.004, 2.5 * g * U * k + 0.004, 0, 0.004),
                          (z_top + 0.004, 2.75 * g * U * k + 0.006, 2.6 * g * U * k + 0.006, 0, 0.004)],
                         sides=16, cap0="fan", cap1="fan")
        sid = b.add(shaft.translate((A.x, 0, 0)), "boots", Bind.blend(ca, ft, falloff=6.0), name=f"boot_shaft_{side}",
                    hull_pad=True)
        cuff = M.loft_z([(z_top - 0.026, 2.78 * g * U * k + 0.007, 2.64 * g * U * k + 0.007, 0, 0.004),
                         (z_top + 0.006, 2.86 * g * U * k + 0.008, 2.72 * g * U * k + 0.008, 0, 0.004)],
                        sides=16, cap0=None, cap1=None)
        b.add(cuff.translate((A.x, 0, 0)), "boot_line", Bind.blend(ca, ft, falloff=6.0), name=f"boot_cuff_{side}",
              outline=False, covered_by=sid)
        del bid

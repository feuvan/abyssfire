"""The Living kit (赤誓会 / 逃兵) — humanoid march monsters on ``SKEL_Human`` (dark remake A3; monsters.md §2.3, §5).

    (library: ``oath_zealot.py`` and ``deserter.py`` build their monsters with it)

monsters.md §2.3: *the same kit as the Unburied with living palettes and no glow, so the player sees that the dead
and the living wore the same tabard*. The kit is the townsfolk body (``npcs/folk.py``: R2 heights, the shared head,
torso / skirt / belt / pack / arms / hands / legs, the cloth chains and their clips) turned into a monster:

* **proportions away from cute** (§5): a smaller head (``head_k`` 0.80 of the NPC head unit instead of 0.92),
  longer limbs (``limb_k``), builds fitted to 1.72–1.76 m (the hero is never shorter, R2);
* **a monster face**: no eye whites, pupils, catch-lights or blush — narrow dark eyes (``living.eye_dark``: the
  Living do not glow) under a scowling brow, a straight nose, a hard mouth line, hollow cheeks;
* **hoods** (a cloth shell over the skull with a brim that overhangs the brow, a cowl down the neck into a shoulder
  capelet) and the deserters' **face scarf**;
* **sleeves**: full (gambeson, a leather bracer on the forearm) or torn off at the elbow over **bare forearms**
  (the zealot: the **oath-scar**, a fist crushing a flame, burned crimson into the skin, ``zealot.scar`` e .25);
* skirts: the zealot's **ragged robe** over a **mail hem**, the deserters' quilted gambeson cut at mid-thigh;
  the rope belt, quilting, the paler patch of a **torn-off insignia**, and the family marker of the deserters, a
  **crimson rag** armband (``deserter.marker``) — the Living's markers are crimson cloth, never a glow (§1.2).

Every colour is a ``Monsters_March`` swatch registered by A1 (``march/march_palettes.py``): the folk part
generators keep their region keys (``skin``, ``top``, ``legs``…) and :class:`MarchBuilder` maps each key onto a
registered swatch *name and value* (``LivingLook.palette``), so a build adds no swatch and can never move one.
Ink: the monster class (1.5 cm baked) with the per-asset 3.5 px request of every Chapter-1 character.
"""
from __future__ import annotations

import math
import sys
from dataclasses import dataclass, field, replace
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "npcs", HERE.parent / "march", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import asset, mesh as M, outline, rig as R  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402
from kit.rig import Bind, HumanoidSpec  # noqa: E402

import common as C  # noqa: E402
from common import Profile  # noqa: E402
import folk as F  # noqa: E402
import goblins as GB  # noqa: E402
import hero_base as HB  # noqa: E402
from hero_base import ell, profile_mesh  # noqa: E402
import march_palettes as MP  # noqa: E402

FAMILY = "Monsters_March"
OUTLINE_CLASS = "monster"
OUTLINE_PX = 3.5
OUTLINE_PX_NOTE = ("Chapter-1 character gate (dark rim >= 3 px on >= 90 % of the 1080p silhouette edges): the monster "
                   "class's 3.0 px keeps hood brims, sleeves, rag tails and weapon hafts < 3 px on part of the "
                   "silhouette; 3.5 px, as the goblins, NPCs and heroes ship, holds it (gate 3.0 px); the held "
                   "weapons and the hunt attachments ink with it")

# every registered Monsters_March swatch: name → Region (exact hex + tone params + ink mix, A1)
MARCH: dict[str, Region] = {n: r for n, r, _ in MP.family_regions(FAMILY)}


class MarchBuilder(GB.LodHullBuilder):
    """``kit.mesh.Builder`` whose region keys are aliases of registered ``Monsters_March`` swatches: ``alias`` maps
    the part generators' keys (``skin``, ``top`` …) onto swatch names (``living.skin``, ``zealot.robe`` …) and the
    swatch is registered with its A1 value, so no build ever appends or moves a swatch (``Palette.add``: same name
    and value → the same index). On the goblins' :class:`goblins.LodHullBuilder`: at LOD1 (``hull_ratio`` < 1)
    every outlined part's hull is baked from a proxy decimated that much further (hull ≤ 0.75 × toon)."""

    def __init__(self, asset_name: str, palette: Palette, alias: dict, **kw):
        super().__init__(asset_name, palette, prefix="living", **kw)
        self.alias = dict(alias)
        for key, name in self.alias.items():
            if name not in MARCH:
                raise KeyError(f"{asset_name}: region {key!r} → {name!r} is not a registered {FAMILY} swatch "
                               f"(march/march_palettes.py)")

    def region(self, key: str, region: Region | None = None) -> int:
        name = self.alias[key]
        idx = self.palette.add(name, MARCH[name])
        self.swatch[key] = idx
        return idx

    def declare(self) -> None:
        for key in self.alias:
            self.region(key)


def swatch_names(alias: dict) -> list[str]:
    return sorted(set(alias.values()))


# ── look ─────────────────────────────────────────────────────────────────────────────────────────────────
@dataclass
class LivingLook(F.Look):
    """``folk.Look`` + the Living's pieces. ``palette``: region key → ``Monsters_March`` swatch name (every key the
    parts use). ``body``: a ``folk.Build`` (bone lengths / widths / girth / height) instead of a named build."""
    palette: dict = field(default_factory=dict)
    body: F.Build | None = None
    head_k: float = 0.60               # head unit × the width unit U (NPC: 0.92; r0 Living 0.76-0.80)
    limb_k: float = 1.0                # arms and shins × the Unburied's
    limb_r: float = 0.80               # arm / leg / boot radii × the townsfolk's (lanky: the Unburied's thin limbs)
    hip_w: float = 5.3                 # skirt half width over the hips (units × girth; folk 5.9)
    hood: bool = False
    hood_brim: float = 2.6             # head h of the brim over the brow (lower = deeper)
    hood_side: float = 56.0            # |angle| where the hood's edge passes the cheek (smaller = closer round the face)
    hood_peak: float = 0.7             # the crown drawn back toward the point (head units × .35)
    hood_tail: float = 0.0             # length of the soft cloth point behind the crown (head units; 0 = none)
    cowl_folds: bool = False           # dark creases down the cowl and capelet
    hood_seam: bool = False            # a dark centre seam from the brim over the crown to the nape
    coif: bool = False                 # a mail coif framing the face under the hood (+ the cowl's mail throat)
    face_shadow: bool = True           # the hood's shadow painted over brow and eyes
    hem_dark: float = 0.0              # share of the skirt above the hem darkened (soot, mud, the scorched edge)
    face_scarf: bool = False           # a cloth wrap over the nose and mouth (deserters)
    sleeve: str = "full"               # full | elbow (torn off over bare forearms)
    bracer: bool = False               # leather bracer on full sleeves
    scar: bool = False                 # the oath-scar on both forearms (bare forearms)
    mail_hem: bool = False             # mail below the robe hem
    robe_k: float = 0.80               # the cloth's own hem (share of the skirt) when a mail hem shows below it
    hem_k: float | None = None         # coat hem height: 0 = the knee … 1 = the hip, < 0 below the knee (None: folk)
    hem_w: tuple = (6.3, 5.4)          # hem half width / depth (units × girth) of a chosen-height coat
    rag_hem: float = 0.0               # sawtooth depth of the hem (share of the skirt)
    rope_belt: bool = False
    quilt: bool = False                # gambeson quilting
    insignia: bool = False             # the paler patch where the crusade insignia was torn off
    armband: str | None = None         # side of the crimson rag armband (the deserters' family marker)
    belt_rag: bool = False             # a crimson rag tucked in the belt at the left hip
    brow_scowl: float = 1.0            # inner brow drop (head units)
    face_hollow: bool = True           # hollow cheeks


# ── dimensions ───────────────────────────────────────────────────────────────────────────────────────────
# The Unburied kit's bone lengths (``unburied.UnburiedKit``: realistic proportions, 1.76 m to the bare crown, head bone
# 0.27 m = 15.6 % of the height, thigh + shin 49 %): monsters.md §2.3 "the same kit as the Unburied", §5 "proportions
# away from cute (smaller heads, longer limbs)". Art review (Living r1, item 1): on the townsfolk proportions the Living
# read as stocky toy soldiers beside the lanky footman (head ~30 % of the figure at the W1 camera, thigh + shin 43 %).
UNBURIED_BODY = dict(thigh=0.43, shin=0.42, ankle=0.072, torso=0.48, neck=0.165, head=0.27, hip_half=0.092,
                     shoulder_half=0.172, upper_arm=0.30, fore_arm=0.275, hand=0.10, foot_len=0.27)
UNBURIED_HEIGHT = 1.76
WIDTH_HEAD_K = 0.78            # the folk width unit is fitted as the r0 Living were (their cloth widths stay as tuned)
HIP_REF, SH_REF = 2.8, 5.5     # folk ``Build.hip`` / ``.sh`` that reproduce the Unburied hip / shoulder half widths


class LivingDims(F.Dims):
    """Rig proportions of a Living body: the **Unburied bone lengths** (``UNBURIED_BODY`` scaled to the height; hip and
    shoulder half widths by the build's ``hip`` / ``sh``) with the folk head ring set at ``head_k`` × the width unit
    ``U`` (the townsfolk build fitted to the height, as the r0 Living: torso rings, skirt, pack and cloth keep their
    widths). ``limb_k`` stretches the shins and arms further (1.0 = the Unburied's)."""

    def __init__(self, b: F.Build, height: float | None, name: str, head_k: float, limb_k: float):
        self.b = b
        self.g = b.girth
        H = height or b.height
        U0 = F.U0
        folk = HumanoidSpec(name=name, skeleton="SKEL_Human", thigh=(b.thigh + F.LEG_ADD) * U0,
                            shin=(b.shin + F.LEG_ADD) * U0, ankle=b.ankle * U0, torso=(b.torso + F.TORSO_ADD) * U0,
                            neck=b.neck * U0 * 0.95, head=2 * F.HEAD_TOP * WIDTH_HEAD_K * U0, hip_half=b.hip * U0,
                            shoulder_half=b.sh * U0, upper_arm=b.upper_arm * U0, fore_arm=b.fore_arm * U0)
        U = U0 * H / folk.crown_height()
        HK = U * head_k
        s = H / UNBURIED_HEIGHT
        ub = UNBURIED_BODY
        base = HumanoidSpec(name=name, skeleton="SKEL_Human", thigh=ub["thigh"] * s, shin=ub["shin"] * s * limb_k,
                            ankle=ub["ankle"] * s, torso=ub["torso"] * s, neck=ub["neck"] * s,
                            head=2 * F.HEAD_TOP * HK, hip_half=ub["hip_half"] * s * b.hip / HIP_REF,
                            shoulder_half=ub["shoulder_half"] * s * b.sh / SH_REF,
                            upper_arm=ub["upper_arm"] * s * limb_k, fore_arm=ub["fore_arm"] * s * limb_k,
                            hand=ub["hand"] * s * (1 + 0.5 * (b.girth - 1)),
                            foot_len=ub["foot_len"] * s * (1 + 0.3 * (b.girth - 1)))
        self.k = H / base.crown_height()
        self.spec = base.fit_height(H)
        sp = self.spec
        self.height = H
        self.U = U * self.k
        self.HK = HK * self.k
        self.hip_z = sp.ankle + sp.shin + sp.thigh
        self.pz = self.hip_z + sp.pelvis_offset
        self.nz = self.pz + sp.torso
        self.hc = self.nz + sp.neck
        self.len_u = sp.torso / self.U


def smooth01(x):
    return F.smooth01(x)


def lerp(a, b, t):
    return a + (b - a) * t


def adist(a: float) -> float:
    """|angle| from the front, 0..180."""
    return abs(((a + 180.0) % 360.0) - 180.0)


# ── the generator ────────────────────────────────────────────────────────────────────────────────────────
class LivingFolk(F.Folk):
    """``folk.Folk`` re-dressed as a march monster (see the module docstring). ``build`` → one merged ``SK_Mon_*``
    mesh on its own ``SKEL_Human`` rig (``spec()``: the folk chains — skirt, coat drivers, pack, scarf)."""

    LOD1_SKIP = F.Folk.LOD1_SKIP + ("quilt", "insignia_thread", "scar_bar", "mail_row", "cheek", "rope_twist",
                                    "brow_hair", "hood_seam", "tatte", "colla", "neck_opening", "wrist",
                                    "armband_tail", "scar_ba", "drum_cord", "drum_dent", "helm_ridge",
                                    "helm_dent", "drum_hook")
    HIDDEN_UNDER_HOOD = ("colla", "neck_opening")      # folk's V-neck trims under the cowl and capelet
    LOD1_RATIO = 0.5
    LOD1_HULL_RATIO = 0.8           # LOD1 hulls from proxies decimated this much further (goblins.LodHullBuilder)

    def __init__(self, look: LivingLook, asset_name: str):
        self.look = look
        self.asset = asset_name
        self.prefix = "living"
        b = look.body or F.BUILDS[look.build]
        self.D = LivingDims(b, look.height, asset_name, look.head_k, look.limb_k)
        D = self.D
        self.U, self.HK, self.g = D.U, D.HK, D.g
        self.belly = D.b.belly
        self.hc = D.hc
        g, bel = self.g, self.belly
        L = D.len_u
        self.L = L
        hs = (1.0, 0.98)
        self.head = Profile([(self.hc + h * self.HK, bb * self.HK * hs[0], a * self.HK * hs[1], -f * self.HK)
                             for h, a, bb, f in F.HEAD_RINGS], exp=2.0)
        tr = [(L + 0.8, 2.2 * g, 2.9 * g, 0.0), (L - 1.2, 3.6 * g, 5.3 * g, 0.0), (L - 5.5, 3.9 * g, 5.4 * g, 0.3),
              (L * 0.4, 3.5 * g + bel, 4.8 * g + bel * 0.4, bel * 0.9), (1.2, 3.7 * g + bel * 0.4, min(5.0, look.hip_w - 0.2) * g, bel * 0.4),
              (-2.4, 4.0 * g, (look.hip_w - 0.4) * g, 0.0)]            # narrow hips (art review r1: hip_w 5.3, folk 5.9)
        self.torso_rings = tr
        self.torso = Profile([(self.th(h), bb * self.U, a * self.U, -f * self.U) for h, a, bb, f in tr], exp=2.25)
        kind = look.top[1]
        self.skirted = kind in ("robe", "coat")
        self.long_skirt = kind == "robe"
        self.hair_thick = 0.0 if (look.hood or not look.hair) else 0.6
        self._skirt_profile()
        self.parts_meta = {}

    # ── skirt: the folk coat with a chosen hem height ──────────────────────────────────────────────────
    def _skirt_profile(self) -> None:
        super()._skirt_profile()
        lk = self.look
        if not self.skirted or lk.hem_k is None:
            return
        U, g = self.U, self.g
        D = self.D
        knee_z = D.spec.ankle + D.spec.shin
        hem_z = lerp(knee_z, D.hip_z, lk.hem_k)
        self.hem_z = hem_z
        z0 = self.th(self.SKIRT_TOP_H)
        rx0, ry0, cy0, _ = self.torso.at(z0)
        zh = self.th(-2.6)
        rxh, ryh, cyh, _ = self.torso.at(self.th(-2.4))
        hip_x = max(rxh + 0.3 * U, lk.hip_w * g * U)
        hip_y = ryh + 0.35 * U
        hem_x, hem_y = lk.hem_w[0] * g * U, lk.hem_w[1] * g * U
        zh = min(zh, lerp(z0, hem_z, 0.45))
        zm = lerp(zh, hem_z, 0.5)
        rings = [(z0, rx0 + 0.25 * U, ry0 + 0.25 * U, cy0), (zh, hip_x, hip_y, cyh * 0.8),
                 (zm, lerp(hip_x, hem_x, 0.52), lerp(hip_y, hem_y, 0.52), cyh * 0.4), (hem_z, hem_x, hem_y, 0.0)]
        self.skirt_z0, self.skirt_z1 = z0, hem_z
        self.skirt = Profile(rings, exp=2.15)

    # ── build ───────────────────────────────────────────────────────────────────────────────────────────
    HIDDEN_UNDER_HELM = ("strand", "ea")              # hair strands on the crown, ears (ear_l → "ea") under the brim

    def builder(self, pal: Palette, lod: int, lod_ratio: float | None) -> MarchBuilder:
        hidden = self.HIDDEN_UNDER_HOOD if self.look.hood else (self.HIDDEN_UNDER_HELM if getattr(self, "helm", False)
                                                                 else ())
        if lod:
            skip = self.LOD1_SKIP + hidden
            return MarchBuilder(self.asset, pal, self.look.palette, lod_ratio=lod_ratio or self.LOD1_RATIO,
                                lod_skip=lambda n: n.rstrip("_lr").rstrip("0123456789").startswith(skip),
                                hull_ratio=self.LOD1_HULL_RATIO)
        if hidden:
            return MarchBuilder(self.asset, pal, self.look.palette,
                                lod_skip=lambda n: n.rstrip("_lr").rstrip("0123456789").startswith(hidden))
        return MarchBuilder(self.asset, pal, self.look.palette)

    def build(self, pal: Palette, rig: R.Rig, lod: int = 0, lod_ratio: float | None = None,
              extra=None) -> bpy.types.Object:
        """The merged mesh; ``extra(self, b, ids)`` adds the monster's own parts (drum, pack straps …)."""
        import json
        self.rig = rig
        self.lod = lod
        b = self.builder(pal, lod, lod_ratio)
        b.declare()
        self.b = b
        lk = self.look
        ids = {}
        ids["skull"] = self.add_head(b)
        if lk.hood:
            ids["hood"] = self.add_hood(b, ids["skull"])
        elif lk.hair:
            self.add_hair(b, ids["skull"])
        if lk.hood and lk.face_shadow and not lk.face_scarf:
            self.add_face_shadow(b, ids["skull"])
        if lk.face_scarf:
            self.add_face_scarf(b, ids["skull"])
        ids["torso"] = self.add_torso(b)
        if self.skirted:
            ids["skirt"] = self.add_skirt(b)
        if lk.rope_belt:
            self.add_rope_belt(b, ids)
        else:
            self.add_belt(b, ids)
        if lk.quilt:
            self.add_quilting(b, ids)
        if lk.insignia:
            self.add_insignia(b, ids)
        if lk.pack:
            self.add_pack(b)
        if lk.belt_rag:
            self.add_belt_rag(b)
        for side in ("l", "r"):
            self.add_arm(b, rig, side)
            self.add_leg(b, rig, side)
        if lk.armband:
            self.add_armband(b, rig, lk.armband)
        if extra is not None:
            extra(self, b, ids)
        names = [p["name"] for p in b.parts]
        body = asset.finish_mesh(b, pal, OUTLINE_CLASS, rig=rig, grounding_height=self.D.height,
                                 name=f"{self.asset}_LOD{lod}" if lod else f"{self.asset}_Mesh")
        body["af_parts"] = json.dumps(names)
        body["af_lod"] = lod
        return body

    # ── head ────────────────────────────────────────────────────────────────────────────────────────────
    def add_head(self, b: M.Builder) -> int:
        hp = self.head
        lod = getattr(self, "lod", 0)
        # under a hood only the face shows: 10 rings × 12 (the r1 budget: the hood's tail, edge and folds)
        zs = [self.hz(h) for h in np.linspace(-6.3, F.HEAD_TOP, (10 if self.look.hood else 14) if not lod else 9)]
        skull = profile_mesh(hp, zs, (12 if self.look.hood else 18) if not lod else 12, cap0="fan", cap1="fan",
                             dome0=0.35 * self.HK,
                             dome1=0.32 * self.HK)
        # the hull proxy: 16 × 9 at LOD0, 12 × 7 at LOD1 (the skull is an exact part: its proxy is not decimated
        # with the LOD ratio, and a 360-triangle head hull held the LOD1 hull at 1.0 × toon)
        proxy = profile_mesh(hp, [self.hz(h) for h in np.linspace(-6.3, F.HEAD_TOP, 9 if not lod else 7)],
                             16 if not lod else 12, cap0="fan", cap1="fan", dome0=0.35 * self.HK, dome1=0.32 * self.HK)
        sid = self._add_exact(b, skull, "skin", Bind.rigid("head"), name="skull", hull=proxy, hull_pad=True)
        self.add_face(b, sid)
        J = self.rig.joints
        n0 = J["neck"] + Vector((0, 0.006, -0.03))
        n1 = Vector((0, 0.004, self.hz(-3.2)))
        r0 = 2.2 * self.g ** 0.8 * self.U
        neck = M.tube([n0, n0.lerp(n1, 0.5), n1], [r0, r0 * 0.97, r0 * 0.95], sides=12)
        b.add(neck, "skin", Bind.blend("spine_03", "neck_01", "head", falloff=3.0), name="neck")
        return sid

    def add_face(self, b: M.Builder, sid: int) -> None:
        """A hard, adult face: sunk dark eyes (no whites, no pupils, no catch-lights), heavy brows slanting down to
        the nose, a straight nose, a hard mouth line, hollow cheeks. Faces read at the W1 pitch by their value
        blocks (brow line, eye shadow, mouth) — 2-4 px each at 1080p."""
        lk = self.look
        HK = self.HK
        bind = Bind.rigid("head")
        lod = getattr(self, "lod", 0)
        for sg in (1.0, -1.0):
            a = sg * F.EYE_A
            fr = self._eye_frame(F.EYE_H, a, -0.06)
            if not (lk.hood and lk.face_shadow) or lk.face_scarf:
                # the socket: a shade lens round the eye (sunk eyes; under a hood the face shadow does it)
                sock = ell((1.20 * HK, 0.30 * HK, 0.78 * HK), (0, 0.02 * HK, 0.05 * HK), sides=8, rings=4)
                self._add_exact(b, sock.transform(fr), "skin_shade", bind, name="eye_socket", outline=False,
                                covered_by=sid)
            eye = ell((0.78 * HK, 0.30 * HK, 0.30 * HK), (0, 0.10 * HK, 0.0), sides=10 if not lod else 6,
                      rings=4)
            self._add_exact(b, eye.transform(fr @ Matrix.Rotation(math.radians(-sg * 8.0), 4, "Y")), "eye", bind,
                            name="eye", outline=False, covered_by=sid)
            # brow: a thick slab from the nose root (low) out to the temple (high): a scowl
            pts, rad = [], []
            nb = 6 if not lod else 4
            for i in range(nb):
                t = i / (nb - 1)
                aa = sg * lerp(7.0, 36.0, t)
                hh = lerp(1.55 - 0.35 * (lk.brow_scowl - 1.0), 2.25, t ** 0.8)
                pts.append(self.hp(hh, aa, 0.26))
                rad.append((0.24 * HK, (0.48 - 0.16 * t) * HK))
            brow = M.sweep(pts, M.superellipse(1, 1, 6 if not lod else 5, 2.2), scales=rad, up=tuple(self.hn(2.0, sg * 20.0)),
                           dome0=0.1 * HK, dome1=0.1 * HK)
            self._add_exact(b, brow, "brow", bind, name="brow", outline=False, covered_by=sid)
            if not lk.hood:
                fe = self._eye_frame(F.EAR_H, sg * F.EAR_A, 0.25)
                fe = fe @ Matrix.Rotation(math.radians(-sg * 10.0), 4, "Y")
                ear = ell((0.95 * HK, 0.42 * HK, 1.5 * HK), (0, 0, 0), sides=10, rings=6)
                b.add(ear.transform(fe), "skin", bind, name="ear", outline=False, covered_by=sid)
            if lk.face_hollow and not lk.face_scarf:
                # hollow cheek: a shade stroke under the cheekbone, down toward the jaw
                pts = [self.hp(-1.0, sg * 40.0, 0.03), self.hp(-2.2, sg * 37.0, 0.03), self.hp(-3.4, sg * 33.0, 0.03)]
                cheek = M.sweep(pts, M.superellipse(1, 1, 6, 2.0), scales=[(0.06 * HK, 0.55 * HK)] * 3,
                                up=tuple(self.hn(-2.0, sg * 37.0)), dome0=0.0, dome1=0.0)
                b.add(cheek, "skin_shade", bind, name="cheek", outline=False, covered_by=sid)
        if lk.face_scarf:
            return
        # nose: straight and narrow, the bridge from between the brows
        path = [self.hp(0.9, 0.0, -0.05), self.hp(-0.3, 0.0, 0.45), self.hp(-1.4, 0.0, 0.85), self.hp(-2.15, 0.0, 0.70),
                self.hp(-2.45, 0.0, 0.15)]
        nose = M.tube(path, [0.36 * HK, 0.40 * HK, 0.52 * HK, 0.50 * HK, 0.32 * HK], sides=10,
                      up=tuple(self.hn(-1.0, 0.0).cross(Vector((1, 0, 0)))), dome0=0.0, dome1=0.2 * HK)
        b.add(nose, "skin", bind, name="nose", outline=False, covered_by=sid)
        # mouth: a hard line, the corners turned down
        pts = []
        nm_ = 7 if not lod else 5
        for i in range(nm_):
            t = -1 + 2 * i / (nm_ - 1)
            pts.append(self.hp(-3.50 - 0.30 * t * t, 15.0 * t, 0.06))
        mouth = M.sweep(pts, M.superellipse(1, 1, 6 if not lod else 4, 2.0), scales=[(0.13 * HK, 0.20 * HK)] * len(pts),
                        up=tuple(self.hn(-3.6, 0.0)), dome0=0.05 * HK, dome1=0.05 * HK)
        self._add_exact(b, mouth, "mouth", bind, name="mouth", outline=False, covered_by=sid)

    # ── hood + mail coif + cowl + capelet ───────────────────────────────────────────────────────────────
    # art review r1 (item 1 / 7): the r0 hood (0.55 + brim .5 + crest .3 head units off a 0.8 head) read as a round steel
    # helmet ~30 % of the figure; the shell is thinner, the crest gone, and the dome is broken by a cloth point
    # behind the crown (``hood_tail``), a lighter rolled edge round the face opening and folds down the cowl
    HOOD_THICK = 0.36          # head units of cloth off the skull
    HOOD_LOW = -5.8            # head h of the hood's lower edge at the sides / back (into the cowl)
    COIF_THICK = 0.26          # the mail coif under the hood

    def hood_lo(self, a: float) -> float:
        """Lower edge of the hood shell (head h): the brim over the brow at the front, down past the cheek to
        the jaw at ``hood_side`` and the nape behind."""
        lk = self.look
        d = adist(a)
        s0, s1 = lk.hood_side - 30.0, lk.hood_side + 12.0
        return lerp(lk.hood_brim, self.HOOD_LOW, smooth01((d - s0) / (s1 - s0)))

    def hood_thick(self, s: float, a: float) -> float:
        """Cloth off the skull (head units): a folded brim round the face opening, the crown drawn back toward the
        point (``hood_tail``), loose cloth low at the back."""
        lk = self.look
        d = adist(a)
        brim = 0.36 * (1.0 - smooth01(s / 0.20)) * (1.0 - smooth01((d - 25.0) / 35.0))
        peak = lk.hood_peak * 0.9 * smooth01((s - 0.5) / 0.5) ** 1.5 * smooth01((d - 110.0) / 50.0)
        drape = 0.42 * smooth01((d - 75.0) / 60.0) * (1.0 - smooth01((s - 0.30) / 0.45))
        return self.HOOD_THICK + brim + peak + drape

    def hood_rim(self, a: float) -> Vector:
        """The face opening's edge (rest head space) at angle ``a`` (|a| <= hood_side + 8): on the shell's outer
        surface just above its lower edge (seen from the W1 camera above the brim)."""
        return self.hp(self.hood_lo(a) + 0.25, a, self.hood_thick(0.03, a) - 0.05)

    def add_hood(self, b: M.Builder, sid: int) -> int:
        lod = getattr(self, "lod", 0)
        cols, rows = (26, 7) if not lod else (20, 5)
        shell = self.head_cap(self.hood_lo, self.hood_thick, top_h=F.HEAD_TOP, cols=cols, rows=rows, sunk=0.4,
                              inner=False)
        # closed hull proxy, coarser than the shell (padded round it: its chords never cut the ink)
        pc, pr = (20, 5) if not lod else (14, 4)
        proxy = self.head_cap(self.hood_lo, lambda s, a: self.hood_thick(s, a) + 0.04, top_h=F.HEAD_TOP, cols=pc,
                              rows=pr, sunk=0.4)
        hid = self._add_exact(b, shell, "hood", Bind.rigid("head"), name="hood", hull=proxy, hull_pad=True)
        side = self.look.hood_side
        angs = [a for a in np.linspace(-(side + 8.0), side + 8.0, 9 if not lod else 7)]
        if "hood_edge" in self.b.alias:
            # a lighter cloth edge rolled round the face opening (the wool's undyed lining turned out): frames the
            # face in the W1 view so the head reads as a hood, not a dome
            pts = [self.hood_rim(a) for a in angs]
            ups = [tuple(self.hn(self.hood_lo(a), a)) for a in angs]
            edge = M.sweep(pts, M.superellipse(1, 1, 4, 2.0),
                           scales=[(0.55 * self.HK, 0.55 * self.HK)] * len(pts), up=ups[len(ups) // 2],
                           dome0=0.25 * self.HK, dome1=0.25 * self.HK)
            self._add_exact(b, edge, "hood_edge", Bind.rigid("head"), name="hood_edge", outline=False,
                            covered_by=hid)
        else:
            # the brim's folded edge: a darker line just inside the face opening
            pts = []
            for i in range(13):
                a = -90.0 + 180.0 * i / 12
                if adist(a) > side + 8.0:
                    continue
                pts.append(self.hp(self.hood_lo(a) + 0.30, a, self.hood_thick(0.0, a) - 0.02))
            if len(pts) > 3:
                edge = M.sweep(pts, M.superellipse(1, 1, 4, 2.0), scales=[(0.08 * self.HK, 0.22 * self.HK)] * len(pts),
                               up=(0, 0, 1), dome0=0.0, dome1=0.0)
                b.add(edge, "hood_line", Bind.rigid("head"), name="hood_seam", outline=False, covered_by=hid)
        if self.look.hood_seam:
            # the hood's centre seam from the brim over the crown to the point: two cloth panels, not a dome
            hs = [self.hp(lerp(self.look.hood_brim + 0.4, F.HEAD_TOP, t), 0.0 if t < 1.0 else 0.0,
                          self.hood_thick(t, 0.0) + 0.02) for t in np.linspace(0.0, 1.0, 5)]
            hs += [self.hp(lerp(F.HEAD_TOP, -2.0, t), 180.0, self.hood_thick(1.0 - t * 0.9, 180.0) + 0.02)
                   for t in np.linspace(0.25, 1.0, 4)]
            seam = M.sweep(hs, M.rect_profile(0.0012, 0.75 * self.HK), up=(1, 0, 0), cap0="ngon", cap1="ngon")
            b.add(seam, "hood_line", Bind.rigid("head"), name="hood_seam", outline=False, covered_by=hid)
        if self.look.hood_tail > 0.0:
            self.add_hood_tail(b)
        if self.look.coif:
            self.add_coif(b, sid)
        self.add_cowl(b)
        return hid

    def add_hood_tail(self, b: M.Builder) -> int:
        """The hood's soft point (a short liripipe): from the back of the crown, back and down onto the cowl — the
        cloth silhouette that breaks the dome at the W1 camera (a helmet has none)."""
        HK = self.HK
        lod = getattr(self, "lod", 0)
        ln = self.look.hood_tail
        th0 = self.hood_thick(0.8, 180.0)
        root = self.hp(F.HEAD_TOP - 3.0, 180.0, th0 - 0.9)
        back, down = Vector((0.0, 1.0, 0.0)), Vector((0.0, 0.0, -1.0))
        pts = [root, root + back * 0.32 * ln * HK + down * 0.10 * ln * HK,
               root + back * 0.55 * ln * HK + down * 0.42 * ln * HK,
               root + back * 0.62 * ln * HK + down * 0.80 * ln * HK,
               root + back * 0.58 * ln * HK + down * 1.10 * ln * HK]
        rads = [(2.4 * HK, 2.0 * HK), (1.9 * HK, 1.5 * HK), (1.35 * HK, 1.05 * HK), (0.85 * HK, 0.7 * HK),
                (0.35 * HK, 0.35 * HK)]
        tail = M.sweep(pts, M.superellipse(1, 1, 6, 2.2), scales=rads, up=(1, 0, 0),
                       dome0=0.0, dome1=0.3 * HK)
        return self._add_exact(b, tail, "hood", Bind.rigid("head"), name="hood_tail", hull_pad=True)

    def coif_lo(self, a: float) -> float:
        d = adist(a)
        return lerp(2.05, -6.3, smooth01((d - 30.0) / 16.0))

    def add_coif(self, b: M.Builder, sid: int) -> int:
        """Mail coif under the hood: a grey frame round the face (the zealots wear their robes over mail)."""
        lod = getattr(self, "lod", 0)
        shell = self.head_cap(self.coif_lo, lambda s, a: self.COIF_THICK, top_h=F.HEAD_TOP - 0.5,
                              cols=24 if not lod else 16, rows=4 if not lod else 3, sunk=0.3, inner=False)
        cid = self._add_exact(b, shell, "mail", Bind.rigid("head"), name="coif", outline=False, covered_by=sid)
        return cid

    def add_face_shadow(self, b: M.Builder, sid: int) -> None:
        """The hood's shadow over the brow and eyes (cel shading casts none): a shade-tone mask from the brow to
        the cheekbones, the eyes and brows on it."""
        HK = self.HK
        lod = getattr(self, "lod", 0)
        rows, cols = (4, 12) if not lod else (2, 6)
        grid = []
        for r in range(rows + 1):
            s = r / rows
            row = []
            for c in range(cols + 1):
                a = -44.0 + 88.0 * c / cols
                d = adist(a)
                top = 2.4 - 0.4 * smooth01((d - 25.0) / 20.0)
                bot = -0.55 - 0.45 * (d / 44.0) ** 2 + 0.25 * math.exp(-(a / 8.0) ** 2)
                row.append(self.hp(lerp(top, bot, s), a, 0.035))
            grid.append(row)
        sh = HB.outward(HB.sheet_grid(grid), lambda c: Vector((0, self.head.at(c.z)[2], c.z)))
        self._add_exact(b, sh, "skin_shade", Bind.rigid("head"), name="face_shadow", outline=False, covered_by=sid)

    def cowl_top(self, a: float) -> Vector:
        """The cowl's upper edge round the neck: the hood's lower edge at the sides / back, under the chin in
        front (the face opening closes there), never inside the neck."""
        d = adist(a)
        w = smooth01((d - 25.0) / 55.0)
        h = lerp(-6.2, self.HOOD_LOW + 0.5, w)
        lift = lerp(0.5, self.hood_thick(0.0, a) + 0.05, w)
        p = self.hp(h, a, lift)
        rn = 2.2 * self.g ** 0.8 * self.U + 0.014
        v = Vector((p.x, p.y - 0.004, 0.0))
        if v.length < rn:
            p = Vector((0.0, 0.004, p.z)) + v.normalized() * rn
        return p

    def capelet_profile(self) -> Profile:
        g, bel, L = self.g, self.belly, self.L
        rings = [(L + 1.4, 2.9 * g, 3.5 * g, 0.0), (L + 0.2, 4.3 * g, 5.6 * g, 0.0),
                 (L - 1.0, 4.8 * g, 6.3 * g, 0.0), (L - 3.4, 4.95 * g + 0.2 * bel, 6.55 * g, 0.1)]
        return Profile([(self.th(h), bb * self.U, a * self.U, -f * self.U) for h, a, bb, f in rings], exp=2.2)

    COWL_MAIL = 40.0           # |angle| of the coif's throat panel in the cowl (mail) when the look has a coif

    def add_cowl(self, b: M.Builder) -> int:
        """Cloth from the hood's lower edge down the neck (closing under the chin; the coif's mail throat in
        front when the look has a coif) and out over the shoulders as a capelet with a ragged hem."""
        lod = getattr(self, "lod", 0)
        cp = self.capelet_profile()
        cols = 20 if not lod else 14
        rag = 1.0 * self.U
        th = 0.007
        bind = HB.blend4(self.rig, "head", "neck_01", "spine_03", "clavicle_l", "clavicle_r", falloff=5.0,
                         smooth=0.02)

        def neck_pt(r, a, d):
            top = self.cowl_top(a)
            bot = cp.point(cp.z1, a, 0.002)
            p = top.lerp(bot, r)
            n = Vector((p.x, p.y - self.torso.at(min(max(p.z, self.torso.z0), self.torso.z1))[2], 0.0))
            n = n.normalized() if n.length > 1e-6 else Vector((0, -1, 0))
            return p + n * d

        def patch(a0, a1, n, region, name, wrap=False):
            angs = [a0 + (a1 - a0) * c / n for c in range(n if wrap else n + 1)]
            outer = [[neck_pt(r, a, th * 0.5) for a in angs] for r in (0.0, 0.5, 1.0)]
            inner = [[neck_pt(r, a, -th * 0.5) for a in angs] for r in (0.0, 0.5, 1.0)]
            return b.add(C.thick_patch(outer, inner, wrap_u=wrap), region, bind, name=name, hull_pad=True)
        m = self.COWL_MAIL
        if self.look.coif:
            patch(-m, m, 8, "mail", "cowl_throat")
            patch(m, 360.0 - m, cols - 6, "hood", "cowl")
        else:
            patch(-180.0, 180.0, cols, "hood", "cowl", wrap=True)
        angs = [-180.0 + 360.0 * c / cols for c in range(cols)]

        def capz(s, a, c):
            z = lerp(cp.z1, cp.z0, s)
            if s >= 1.0 - 1e-6:
                z += rag * (0.7, -0.5, 0.2, -0.3)[c % 4]
            return z
        outer, inner = [], []
        for s in (0.0, 0.35, 0.7, 1.0):
            outer.append([cp.point(capz(s, a, c), a, 0.002 + th * 0.5) for c, a in enumerate(angs)])
            inner.append([cp.point(capz(s, a, c), a, 0.002 - th * 0.5) for c, a in enumerate(angs)])
        cid = b.add(C.thick_patch(outer, inner, wrap_u=True), "hood", bind, name="capelet", hull_pad=True)
        if self.look.cowl_folds and not lod:
            # cowl folds (art review r1, item 7): dark creases from under the hood down the cowl and over the
            # capelet — slack cloth round the neck, not a helmet's rim
            for a, ln in ((-64.0, 1.0), (-30.0, 0.8), (30.0, 0.9), (66.0, 1.0)):    # the camera-side folds
                if self.look.coif and abs(a) < self.COWL_MAIL + 4.0:
                    continue
                rs = [0.08, 0.5, 1.0]
                path = [neck_pt(r, a + 3.0 * r, th * 0.5 + 0.0011) for r in rs]
                path += [cp.point(lerp(cp.z1, cp.z0, k * ln), a + 3.0 + 4.0 * k, 0.002 + th * 0.5 + 0.0011)
                         for k in (0.4, 0.8)]
                ups = []
                for q in path:
                    nn = Vector((q.x, q.y - self.torso.at(min(max(q.z, self.torso.z0), self.torso.z1))[2], 0.0))
                    ups.append(nn.normalized() if nn.length > 1e-6 else Vector((0, -1, 0)))
                fold = M.sweep(path, M.rect_profile(0.0012, 0.5 * self.U), up=tuple(ups[2]), cap0="ngon",
                               cap1="ngon", scales=[(1.0, 0.6), (1.0, 1.0), (1.0, 0.9), (1.0, 0.7), (1.0, 0.3)])
                b.add(fold, "hood_line", bind, name="cowl_fold", outline=False, covered_by=cid)
        return cid

    # ── face scarf (deserters) ──────────────────────────────────────────────────────────────────────────
    def add_face_scarf(self, b: M.Builder, sid: int) -> int:
        """A cloth wrap from under the eyes over the nose and mouth, round to the hood (or the nape)."""
        HK = self.HK
        lod = getattr(self, "lod", 0)
        cols = 12 if not lod else 10
        a0, a1 = -100.0, 100.0
        rows = 3
        outer, inner = [], []
        for r in range(rows + 1):
            s = r / rows
            ro, ri = [], []
            for c in range(cols + 1):
                a = lerp(a0, a1, c / cols)
                d = adist(a)
                top = -0.9 - 0.5 * smooth01((d - 20.0) / 40.0)       # over the nose bridge, lower at the sides
                h = lerp(top, -6.6, s)
                nose = 0.9 * math.exp(-(a / 16.0) ** 2) * math.exp(-((h + 1.6) / 1.4) ** 2)
                lift = 0.42 + nose + 0.15 * s
                ro.append(self.hp(h, a, lift))
                ri.append(self.hp(h, a, lift - 0.5))
            outer.append(ro)
            inner.append(ri)
        wrap = C.thick_patch(outer, inner)
        fid = b.add(wrap, "hood", Bind.rigid("head"), name="face_scarf", hull_pad=True)
        # a fold line across it
        pts = [self.hp(-3.2 - 0.4 * (a / 70.0) ** 2, a, 0.62) for a in np.linspace(-70.0, 70.0, 7)]
        fold = M.sweep(pts, M.superellipse(1, 1, 4, 2.0), scales=[(0.08 * HK, 0.22 * HK)] * len(pts),
                       up=tuple(self.hn(-3.2, 0.0)), dome0=0.0, dome1=0.0)
        b.add(fold, "hood_line", Bind.rigid("head"), name="hood_seam", outline=False, covered_by=fid)
        return fid

    # ── skirt: ragged robe, mail hem ────────────────────────────────────────────────────────────────────
    def add_skirt(self, b: M.Builder) -> int:
        lk = self.look
        lod = getattr(self, "lod", 0)
        cols = 18 if not lod else 14
        end = lk.robe_k if lk.mail_hem else 1.0
        nrow = 7 if not lod else 5
        rows = [end * i / nrow for i in range(nrow + 1)]
        jag = lk.rag_hem

        def pt(k, uu, d):
            if jag and k >= rows[-1] - 1e-6:
                c = int(round(uu * cols)) % cols
                k = k + jag * ((0.9, -0.3, 0.5, -0.1, 0.7, -0.4)[c % 6])
            return self.skirt_point(k, uu * 360.0 - 180.0, d)
        # every row in the hull (the skirt chains bend between rows), 14 / 10 columns (padded round the shell)
        shell, hull = HB.shell_ring(pt, rows, cols, self.SKIRT_THICK, lens=0.002 / 0.75, hull_row_step=1,
                                    hull_cols=14 if not lod else 10)
        bind = Bind.custom(self.skirt_bones(), self.skirt_weights)
        sid = b.add(shell, "top", bind, name="skirt", sub_regions={1: "top", 2: "top_shade"}, hull=hull,
                    hull_pad=True)
        t = self.SKIRT_THICK / 2
        if lk.hem_dark > 0:
            # the scorched hem: a charred band over the lowest rows (outside only), its top edge ragged, its lower
            # edge on the rags of the hem
            grid = []
            for i in range(3):
                row = []
                for c in range(cols + 1):
                    uu = c / cols
                    k = rows[-1] - lk.hem_dark * (1.0 - i / 2) * (1.0 + 0.35 * math.sin(c * 2.3)) if i < 2 else rows[-1]
                    row.append(pt(k, uu % 1.0, t + 0.0011) if i == 2 else
                               self.skirt_point(k, uu * 360.0 - 180.0, t + 0.0011))
                grid.append(row)
            band = HB.outward(HB.sheet_grid(grid), lambda c: Vector((0, 0, c.z)))
            b.add(band, "hem", bind, name="hem_band", outline=False, covered_by=sid)
        for a, k0 in ((29.0, 0.12), (-29.0, 0.12), (150.0, 0.2), (-150.0, 0.2)):
            strip = [[self.skirt_point(k, a + da, t + 0.0012) for da in (-0.9, 0.0, 0.9)]
                     for k in [k0 + (end * 0.96 - k0) * i / 6 for i in range(7)]]
            b.add(HB.outward(HB.sheet_grid(strip), lambda c: Vector((0, 0, c.z))), "top_shade", bind, name="fold",
                  outline=False, covered_by=sid)
        if lk.mail_hem:
            mrows = [end - 0.06 + (1.0 - end + 0.06) * i / 4 for i in range(5)]
            mail, mhull = HB.shell_ring(lambda k, uu, d: self.skirt_point(k, uu * 360.0 - 180.0, d - 0.004),
                                        mrows, cols, 0.006, lens=0.002 / 0.75, hull_row_step=1)
            mid = b.add(mail, "mail", bind, name="mail_hem", sub_regions={1: "mail", 2: "mail_line"}, hull=mhull,
                        hull_pad=True)
            for k in (end + 0.05, end + 0.11, end + 0.17):
                if k > 0.985:
                    continue
                ring = [[self.skirt_point(kk, -180.0 + 360.0 * c / cols, 0.0004) for c in range(cols + 1)]
                        for kk in (k - 0.008, k + 0.008)]
                band = HB.outward(HB.sheet_grid(ring), lambda c: Vector((0, 0, c.z)))
                b.add(band, "mail_line", bind, name="mail_row", outline=False, covered_by=mid)
        return sid

    # ── belts ───────────────────────────────────────────────────────────────────────────────────────────
    def add_rope_belt(self, b: M.Builder, ids: dict) -> None:
        """A rope cinched twice round the robe, knotted at the left hip, two ends hanging on the skirt."""
        bp = self.belt_profile()
        U = self.U
        sb = Bind.blend("pelvis", "spine_01", smooth=0.04) if self.skirted else self.spine_bind()
        lod = getattr(self, "lod", 0)
        z = self.th(1.0)
        n = 14 if not lod else 12
        rid = None
        for dz in (0.45 * U, -0.45 * U):
            ring = [bp.point(z + dz + 0.25 * U * math.sin(math.radians(a * 2.0)), a, 0.36 * U)
                    for a in np.linspace(-180.0, 180.0, n, endpoint=False)]
            rope = M.sweep(ring, M.superellipse(1, 1, 4, 2.0), scales=[(0.38 * U, 0.38 * U)] * n,
                           closed_path=True, up=(0, 0, 1), cap0=None, cap1=None)
            pid = b.add(rope, "rope", sb, name="rope_belt", hull_pad=True)
            rid = pid if rid is None else rid
        ka = 58.0
        knot = ell((1.1 * U, 0.9 * U, 1.0 * U), bp.point(z, ka, 1.3 * U), sides=10, rings=6)
        b.add(knot, "rope", sb, name="rope_knot", hull_pad=True)
        self.belt_id = rid
        if not self.skirted:
            return
        # the ends hang on the skirt's side (skinned with it, so the thigh drivers carry them)
        bind = Bind.custom(self.skirt_bones(), self.skirt_weights)
        for da, ln in ((-5.0, 0.30), (6.0, 0.22)):
            pts = [self.skirt_point(k, ka + da, self.SKIRT_THICK + 0.006) for k in np.linspace(0.03, ln, 5)]
            end = M.tube(pts, [0.42 * U] * 4 + [0.5 * U], sides=6, dome0=0.0, dome1=0.4 * U)
            b.add(end, "rope", bind, name="rope_end", hull_pad=True)

    # ── gambeson quilting, torn insignia ────────────────────────────────────────────────────────────────
    def add_quilting(self, b: M.Builder, ids: dict) -> None:
        tp, L, U = self.torso, self.L, self.U
        sb = self.spine_bind()
        tid = ids["torso"]
        for a in (-140.0, -66.0, -36.0, 36.0, 66.0, 140.0):
            b.add(HB.painted_strip(tp, a, self.th(0.0), self.th(L - 1.8), 0.32 * U, lambda z, aa: 0.0015, steps=5),
                  "quilt", sb, name="quilt", outline=False, covered_by=tid)
        for h in (L * 0.3, L * 0.62):
            b.add(HB.band_sheet(tp, self.th(h), 0.3 * U, 0.0015, -180.0, 180.0, segs=16), "quilt", sb, name="quilt",
                  outline=False, covered_by=tid)
        if self.skirted and "skirt" in ids:
            bind = Bind.custom(self.skirt_bones(), self.skirt_weights)
            t = self.SKIRT_THICK / 2
            for a in (-120.0, -60.0, 60.0, 120.0, 180.0):
                strip = [[self.skirt_point(k, a + da, t + 0.0012) for da in (-0.7, 0.0, 0.7)]
                         for k in [0.08 + 0.86 * i / 3 for i in range(4)]]
                b.add(HB.outward(HB.sheet_grid(strip), lambda c: Vector((0, 0, c.z))), "quilt", bind, name="quilt",
                      outline=False, covered_by=ids["skirt"])

    INSIGNIA_A = 26.0          # chest angle of the torn-off insignia (character left: the far side in se)

    def add_insignia(self, b: M.Builder, ids: dict) -> None:
        """The paler, frayed patch on the chest where the crusade's sigil was ripped off."""
        tp, L, U = self.torso, self.L, self.U
        sb = self.spine_bind()
        a0 = -8.0
        rows = 5
        grid = []
        for r in range(rows + 1):
            h = lerp(L - 2.6, L - 7.6, r / rows)
            jit = 1.4 * math.sin(r * 2.1)
            grid.append([tp.point(self.th(h), a, 0.0019) for a in np.linspace(a0 - 13.0 + jit, a0 + 13.0 - jit, 5)])
        b.add(HB.outward(HB.sheet_grid(grid), lambda c: Vector((0, tp.at(c.z)[2], c.z))), "insignia", sb,
              name="insignia", outline=False, covered_by=ids["torso"])
        for k, (ha, hb_, da) in enumerate(((L - 2.4, L - 4.0, -14.0), (L - 5.5, L - 7.9, 12.5), (L - 7.7, L - 8.6, -6.0))):
            b.add(HB.painted_strip(tp, a0 + da, self.th(hb_), self.th(ha), 0.22 * U, lambda z, aa: 0.0024, steps=3),
                  "quilt", sb, name="insignia_thread", outline=False, covered_by=ids["torso"])

    # ── marker cloth ────────────────────────────────────────────────────────────────────────────────────
    def add_armband(self, b: M.Builder, rig: R.Rig, side: str) -> int:
        """The deserters' family marker: a crimson rag knotted round the upper arm (a torn strip of the crusade
        tabard), its two short tails hanging on the outside of the arm."""
        J = rig.joints
        S, E = J[f"shoulder_{side}"], J[f"elbow_{side}"]
        d = (E - S).normalized()
        sg = 1.0 if side == "l" else -1.0
        r = self.arm_radius(0.55) + 0.009
        c = S.lerp(E, 0.55)
        band = M.tube([c - d * 0.03, c + d * 0.03], [r, r * 0.98], sides=14, dome0=0.0, dome1=0.0)
        bone = f"upperarm_{side}"
        aid = b.add(band, "marker", Bind.rigid(bone), name="armband", hull_pad=True)
        out = Vector((sg, 0.25, 0.0)).normalized()
        out = (out - d * out.dot(d)).normalized()
        k0 = c + out * (r + 0.006)
        knot = ell((0.016, 0.012, 0.014), k0, sides=6, rings=4)
        b.add(knot, "marker", Bind.rigid(bone), name="armband_knot", hull_pad=True)
        for tw, ln in ((-0.012, 0.055), (0.012, 0.045)):
            side_v = d.cross(out).normalized()
            pts = [k0 + side_v * tw * 0.3, k0 + side_v * tw + d * ln * 0.5 + out * 0.008, k0 + side_v * tw * 1.4 + d * ln + out * 0.004]
            tail = M.sweep(pts, M.rect_profile(0.004, 0.020), up=tuple(out), cap0="ngon", cap1="ngon")
            b.add(tail, "marker", Bind.blend(bone, f"lowerarm_{side}", falloff=6.0), name="armband_tail",
                  hull_pad=True)
        return aid

    def add_belt_rag(self, b: M.Builder) -> None:
        """A second crimson rag tucked under the belt at the left hip, hanging on the skirt."""
        U = self.U
        a = 72.0
        if self.skirted:
            bind = Bind.custom(self.skirt_bones(), self.skirt_weights)
            pts = [self.skirt_point(k, a, self.SKIRT_THICK + 0.004) for k in np.linspace(0.0, 0.62, 5)]
        else:
            bp = self.belt_profile()
            bind = Bind.blend("pelvis", "thigh_l", falloff=5.0)
            pts = [bp.point(self.th(0.8) - i * 0.04, a, 0.5 * U) for i in range(5)]
        rows_o, rows_i = [], []
        for i, p in enumerate(pts):
            f = i / (len(pts) - 1)
            nrm = Vector((p.x, p.y, 0.0)).normalized()
            tang = Vector((0, 0, 1)).cross(nrm).normalized()
            w = lerp(1.5, 1.9, f) * U
            jag = (0.0, 0.0, 0.0, 0.0, 0.012)[i]
            ro = [p + tang * (s * w) + nrm * 0.003 + Vector((0, 0, jag * (1 if s > 0 else -0.5)))
                  for s in (-1.0, 0.0, 1.0)]
            ri = [q - nrm * 0.006 for q in ro]
            rows_o.append(ro)
            rows_i.append(ri)
        rag = C.thick_patch(rows_o, rows_i)
        b.add(rag, "marker", bind, name="belt_rag", hull_pad=True)

    # ── arms ────────────────────────────────────────────────────────────────────────────────────────────
    def arm_radius(self, t: float) -> float:
        """Sleeve radius along the arm (0 shoulder … 1 wrist)."""
        g, U = self.g, self.U
        k = 0.90 * self.look.limb_r
        r_s, r_e, r_w = 2.8 * g * U * k, 2.4 * g * U * k, 2.05 * g * U * k
        return lerp(r_s, r_e, t / 0.5) if t < 0.5 else lerp(r_e, r_w, (t - 0.5) / 0.5)

    def add_belt(self, b: M.Builder, ids: dict) -> None:
        """folk's belt (an iron buckle via the ``gold`` alias) at 22 segments."""
        bp = self.belt_profile()
        z = self.th(1.1)
        sb = self.spine_bind() if not self.skirted else Bind.blend("pelvis", "spine_01", smooth=0.04)
        belt = C.surface_band(bp, z, 1.75 * self.U, 0.42 * self.U, segs=22, out0=-0.2 * self.U)
        bid = b.add(belt, "belt", sb, name="belt", hull_pad=True)
        self.belt_id = bid
        p = bp.point(z, -6.0, 0.42 * self.U)
        n = bp.normal(z, -6.0)
        fr = R.frame(p, n, (0, 0, 1))
        frame = M.box((2.5 * self.U, 0.32 * self.U, 2.3 * self.U), (0, 0.10 * self.U, 0), bevel=0.0025)
        b.add(frame.transform(fr), "gold", sb, name="buckle", outline=False, covered_by=bid)
        inner = M.box((1.3 * self.U, 0.34 * self.U, 1.15 * self.U), (0, 0.16 * self.U, 0), bevel=0.0015)
        b.add(inner.transform(fr), "belt", sb, name="buckle_in", outline=False, covered_by=bid)

    def add_full_sleeve(self, b: M.Builder, rig: R.Rig, side: str) -> None:
        """folk's sleeve (elbow loops either side of the hinge) at 12 sides; a cuff only without a bracer."""
        J = rig.joints
        g, U = self.g, self.U
        S, E, W = J[f"shoulder_{side}"], J[f"elbow_{side}"], J[f"wrist_{side}"]
        ua, la, cl = f"upperarm_{side}", f"lowerarm_{side}", f"clavicle_{side}"
        dua = (E - S).normalized()
        dla = (W - E).normalized()
        k = 0.92 * self.look.limb_r
        r_s, r_e, r_w = 2.9 * g * U * k, 2.5 * g * U * k, 2.15 * g * U * k
        pts = [S - dua * 0.045, S.lerp(E, 0.45), S.lerp(E, 0.8), E, E.lerp(W, 0.2), E.lerp(W, 0.55), W - dla * 0.01]
        rads = [r_s, lerp(r_s, r_e, 0.45) * 1.02, lerp(r_s, r_e, 0.8) * 1.01, r_e, lerp(r_e, r_w, 0.2),
                lerp(r_e, r_w, 0.55), r_w]
        lod = getattr(self, "lod", 0)
        arm = M.tube(pts, rads, sides=10 if lod else 12, dome0=r_s * 0.6, dome1=r_w * 0.4)
        bind = Bind.blend(cl, ua, la, f"hand_{side}", falloff=5.0)
        add = self._add_exact if lod else (lambda bb, *a_, **kw: bb.add(*a_, **kw))
        aid = add(b, arm, "sleeve", bind, name=f"arm_{side}", hull_pad=True)
        if not self.look.bracer:
            cuff = M.tube([W - dla * 0.045, W - dla * 0.004], [r_w * 1.13, r_w * 1.16], sides=10 if lod else 12,
                          dome0=0.0, dome1=0.0)
            add(b, cuff, "top_shade", Bind.blend(la, f"hand_{side}", falloff=6.0), name=f"cuff_{side}", outline=False,
                covered_by=aid)

    def add_arm(self, b: M.Builder, rig: R.Rig, side: str, hand: bool = True) -> None:
        lk = self.look
        if lk.sleeve != "elbow":
            self.add_full_sleeve(b, rig, side)
            if lk.bracer:
                self.add_bracer(b, rig, side)
            if hand:
                self.add_hand(b, rig, side)
            return
        # torn-off sleeve to the elbow over a bare forearm
        J = rig.joints
        g, U = self.g, self.U
        S, E, W = J[f"shoulder_{side}"], J[f"elbow_{side}"], J[f"wrist_{side}"]
        ua, la, cl = f"upperarm_{side}", f"lowerarm_{side}", f"clavicle_{side}"
        dua = (E - S).normalized()
        dla = (W - E).normalized()
        lod = getattr(self, "lod", 0)
        k = 0.90 * lk.limb_r
        r_s, r_e, r_w = 2.8 * g * U * k, 2.35 * g * U * k, 1.95 * g * U * k
        # forearm (skin) from inside the sleeve to the wrist; elbow loops either side of the hinge
        fpts = [E - dua * 0.06, E - dua * 0.02, E, E.lerp(W, 0.25), E.lerp(W, 0.6), W - dla * 0.01]
        frad = [r_e * 0.95, r_e * 0.98, r_e, lerp(r_e, r_w, 0.25) * 1.04, lerp(r_e, r_w, 0.6), r_w]
        fore = M.tube(fpts, frad, sides=8 if lod else 12, dome0=r_e * 0.5, dome1=r_w * 0.4)
        add = self._add_exact if lod else (lambda bb, *a_, **kw: bb.add(*a_, **kw))
        fid = add(b, fore, "skin", Bind.blend(ua, la, f"hand_{side}", falloff=5.0), name=f"forearm_{side}",
                  hull_pad=True)
        # the sleeve: shoulder → just past the elbow, flaring to a torn cuff
        spts = [S - dua * 0.045, S.lerp(E, 0.45), S.lerp(E, 0.8), E, E + dla * 0.035]
        srad = [r_s, lerp(r_s, r_e, 0.45) * 1.06, lerp(r_s, r_e, 0.8) * 1.14, r_e * 1.30, r_e * 1.42]
        sleeve = M.tube(spts, srad, sides=12 if not lod else 9, dome0=r_s * 0.6, dome1=0.0)
        sid = add(b, sleeve, "sleeve", Bind.blend(cl, ua, la, falloff=5.0), name=f"arm_{side}", hull_pad=True)
        # tatters hanging off the cuff (the sleeve was torn, not cut)
        up = Vector((0, 0, 1))
        ref = (up - dla * up.dot(dla)).normalized()
        side_v = dla.cross(ref).normalized()
        cuff_c = E + dla * 0.035
        for i, (ang, ln) in enumerate(((20.0, 0.045), (150.0, 0.05), (260.0, 0.04))):
            th_ = math.radians(ang)
            radial = ref * math.cos(th_) + side_v * math.sin(th_)
            p0 = cuff_c + radial * (r_e * 1.40)
            p1 = p0 + dla * ln + radial * 0.004
            tat = M.sweep([p0 - dla * 0.006, p0.lerp(p1, 0.5), p1], M.rect_profile(0.003, 0.022),
                          scales=[(1.0, 1.0), (1.0, 0.8), (1.0, 0.25)], up=tuple(radial), cap0="ngon", cap1="ngon")
            b.add(tat, "sleeve", Bind.blend(ua, la, falloff=5.0), name=f"tatter_{side}", outline=False,
                  covered_by=sid)
        if lk.scar:
            self.add_scar(b, rig, side, fid)
        if hand:
            self.add_hand(b, rig, side)

    def add_bracer(self, b: M.Builder, rig: R.Rig, side: str) -> None:
        J = rig.joints
        E, W = J[f"elbow_{side}"], J[f"wrist_{side}"]
        dla = (W - E).normalized()
        r0, r1 = self.arm_radius(0.68) * 1.12, self.arm_radius(0.98) * 1.18
        pts = [E.lerp(W, 0.40), E.lerp(W, 0.7), W - dla * 0.012]
        br = M.tube(pts, [r0, lerp(r0, r1, 0.5), r1], sides=10, dome0=0.0, dome1=0.0)
        bid = b.add(br, "leather", Bind.blend(f"lowerarm_{side}", f"hand_{side}", falloff=6.0),
                    name=f"bracer_{side}", hull_pad=True)
        for t in (0.62,):
            lace = M.tube([E.lerp(W, t) - dla * 0.004, E.lerp(W, t) + dla * 0.004],
                          [lerp(r0, r1, (t - 0.4) / 0.6) * 1.06] * 2, sides=8, dome0=0.0, dome1=0.0)
            b.add(lace, "leather_dark", Bind.rigid(f"lowerarm_{side}"), name="lace", outline=False, covered_by=bid)

    SCAR_AT = 0.52             # along the forearm (elbow 0 … wrist 1)

    def add_scar(self, b: M.Builder, rig: R.Rig, side: str, fid: int) -> None:
        """The oath-scar on the back of the forearm: a brand, not a wound (art review r1: the r0 wavy flame read as
        bleeding blotches against story-bible's "ash, not blood"). A clean oval of burned skin (``scar_edge``, a
        dark seared rim), inside it a crimson flame (``zealot.scar``, emissive .25: a teardrop with a pointed tip)
        and the dark bar of the fist crushing it (the church's open palm cradling a flame, twisted)."""
        J = rig.joints
        E, W = J[f"elbow_{side}"], J[f"wrist_{side}"]
        dla = (W - E).normalized()
        sg = 1.0 if side == "l" else -1.0
        # the back of the forearm in the A-pose: outward and a little forward / up (seen from the W1 camera)
        out = Vector((sg * 0.9, -0.35, 0.45))
        out = (out - dla * out.dot(dla)).normalized()
        tang = dla.cross(out).normalized()
        r = self.arm_radius(self.SCAR_AT) * (0.94 / 0.90) + 0.0010
        c = E.lerp(W, self.SCAR_AT)
        bind = Bind.blend(f"lowerarm_{side}", f"hand_{side}", falloff=6.0)
        lod = getattr(self, "lod", 0)
        edge_r = "scar_edge" if "scar_edge" in self.b.alias else "skin_shade"

        def on_arm(u, v, lift=0.0):          # u along the arm (m), v round it (m)
            ang = v / r
            return c + dla * u + (out * math.cos(ang) + tang * math.sin(ang)) * (r + lift)

        def axis(p):
            return c + dla * (p - c).dot(dla)
        # the seared oval: an annulus (u half-axis 2.6 cm, v 1.9 cm, 0.45 cm wide)
        n = 16 if not lod else 10
        ring = []
        for k in range(n + 1):
            t = 2 * math.pi * k / n
            ring.append([on_arm(au * math.cos(t), av * math.sin(t), 0.0002) for au, av in ((0.026, 0.019),
                                                                                          (0.0215, 0.0145))])
        ring = [list(x) for x in zip(*ring)]
        b.add(HB.outward(HB.sheet_grid(ring), axis), edge_r, bind, name="scar_edge", outline=False, covered_by=fid)
        # the flame: a teardrop from a round base (toward the elbow) to a point (toward the wrist)
        rows = []
        nr = 7 if not lod else 4
        for i in range(nr):
            t = i / (nr - 1)
            u = -0.015 + 0.033 * t
            w = 0.0105 * math.sin(math.pi * min(1.0, 0.18 + 0.82 * t)) ** 0.55 * (1.0 - t ** 3) + 0.0008
            rows.append([on_arm(u, v, 0.0004) for v in (-w, -w * 0.5, 0.0, w * 0.5, w)])
        b.add(HB.outward(HB.sheet_grid(rows), axis), "scar", bind, name="scar", outline=False, covered_by=fid)
        # the fist's bar across the flame's waist
        bar = [[on_arm(u, v, 0.0007) for v in np.linspace(-0.0145, 0.0145, 5)] for u in (-0.0035, 0.0030)]
        b.add(HB.outward(HB.sheet_grid(bar), axis), edge_r, bind, name="scar_bar", outline=False, covered_by=fid)

    # ── hands: rough fists ──────────────────────────────────────────────────────────────────────────────
    def add_hand(self, b: M.Builder, rig: R.Rig, side: str) -> None:
        super().add_hand(b, rig, side)

    # ── legs: the folk leg slimmed to the Unburied's (art review r1: chunky boots, short legs) ────────────
    def add_leg(self, b: M.Builder, rig: R.Rig, side: str) -> None:
        """``folk.Folk.add_leg`` (same part names: the QA groups ``leg_*`` / ``boot_*``) with the radii × ``limb_r``,
        a narrower, lower boot and its shaft to 42 % of the shin (the Unburied's)."""
        J = rig.joints
        g, U = self.g, self.U
        Hp, Kn, A = J[f"hip_{side}"], J[f"knee_{side}"], J[f"ankle_{side}"]
        th_, ca, ft, bl = f"thigh_{side}", f"calf_{side}", f"foot_{side}", f"ball_{side}"
        k = 0.92 * self.look.limb_r
        top = Hp + Vector((0, 0, -0.03 if not self.skirted else -0.06))
        pts = [top, top.lerp(Kn, 0.5), Kn, Kn.lerp(A, 0.5), A + Vector((0, 0, 0.05))]
        rads = [3.3 * g * U * k, 3.05 * g * U * k, 2.8 * g * U * k, 2.45 * g * U * k, 2.2 * g * U * k]
        if self.skirted:
            rads[:3] = [2.55 * g * U * k, 2.45 * g * U * k, 2.4 * g * U * k]
        lod = getattr(self, "lod", 0)
        b.add(M.tube(pts, rads, sides=12 if not lod else 10), "legs", Bind.blend("pelvis", th_, ca, ft, falloff=5.0),
              name=f"leg_{side}")
        ftl = self.D.spec.foot_len
        bw = 2.15 * g ** 0.7 * U * 2 * 0.92                  # ≈ 11 cm (the Unburied's; folk 12)
        boot = M.boot(ftl, bw, 0.084 * (1 + 0.2 * (g - 1)), shaft=0.0, toe_up=0.018)
        b.add(boot.translate((A.x, 0, 0)), "boots", Bind.blend(ca, ft, bl, falloff=5.0), name=f"boot_{side}",
              hull_pad=True)
        z_top = A.z + 0.42 * (Kn.z - A.z)
        r0, r1 = 2.45 * g * U * k + 0.004, 2.65 * g * U * k + 0.005
        shaft = M.loft_z([(0.035, r0, r0 * 0.94, 0, 0.006), (z_top - 0.02, r1 * 0.96, r1 * 0.92, 0, 0.004),
                          (z_top + 0.004, r1, r1 * 0.95, 0, 0.004)], sides=14 if not lod else 10, cap0="fan",
                         cap1="fan")
        sid = b.add(shaft.translate((A.x, 0, 0)), "boots", Bind.blend(ca, ft, falloff=6.0), name=f"boot_shaft_{side}",
                    hull_pad=True)
        cuff = M.loft_z([(z_top - 0.024, r1 + 0.003, r1 * 0.95 + 0.003, 0, 0.004),
                         (z_top + 0.006, r1 + 0.005, r1 * 0.95 + 0.005, 0, 0.004)], sides=14 if not lod else 10,
                        cap0=None, cap1=None)
        b.add(cuff.translate((A.x, 0, 0)), "boot_line", Bind.blend(ca, ft, falloff=6.0), name=f"boot_cuff_{side}",
              outline=False, covered_by=sid)


def build_rig(folk: LivingFolk, extra_chains=()) -> R.Rig:
    """``SKEL_Human`` with the look's folk chains (+ ``extra_chains``) and the monster sockets (``fx_eye_l/r`` for
    the portrait, ``eyes``)."""
    sp = folk.spec()
    sp.chains = list(sp.chains) + list(extra_chains)
    rig = R.build_humanoid(sp)
    for sg, nm in ((1.0, "fx_eye_l"), (-1.0, "fx_eye_r")):
        R.add_socket(rig, nm, "head", folk.hp(F.EYE_H, sg * F.EYE_A, 0.3))
    return rig

"""The Gravegnawers (啃冢者) — ``SKEL_Goblin`` re-skins for the dark remake (A4; monsters.md §2.4, §5).

    (library: ``gravegnawer.py`` and ``bonegorger.py`` build their monsters with it)

monsters.md §2.4: *carrion things drawn by the battle: hairless, ash-grey, long-armed, needle-toothed. They eat what
the bell would toll for. They are beasts, not breach-touched: bone-yellow eyes.* The kit subclasses
:class:`goblins.GoblinKit` (``goblins.py`` is not edited) and keeps ``SKEL_Goblin`` — the same bone names, chains and
clip machinery (``mclips.GoblinAnims``) as the shipped goblins — but replaces the cute goblin read (§5: smaller heads,
longer limbs, no pupils, hunched silhouettes):

* **head** — a small low cranium on a scrawny forward-thrust neck, a **long muzzle** (upper snout on ``head``, the
  lower jaw a separate solid on the ``jaw`` bone, hinged under the eye so the whole jaw drops when it bites) lined with
  **needle teeth** that overlap the lips even with the mouth shut, a near-black maw between the jaws, a dark nose pad,
  deep-set **bone-yellow eyeshine** (``*.eye``, emissive .35 — the family marker; no pupils) under a low brow, and the
  goblin ears **cut to torn stubs**;
* **body** — an emaciated torso (pinched waist, ribcage, painted rib strokes with ``mgeo.surface_paint``, a knobbed
  spine ridge and shoulder blades the high W1 camera sees), arms + 15 % with knobbly elbows and long-clawed hands
  (``gnawer.claw``), thin legs and long clawed feet; the bruiser variant (``bonegorger``) is bloated instead: a sagging
  belly, mangy tufts, an oversized underbite jaw;
* **trophies** — scraps of the crusade tabard (``gnawer.trophy``) tied on: a ragged loin scrap on a rope belt and a
  rag knotted round the upper arm.

Every colour is a registered ``Monsters_March`` swatch (A1, ``march/march_palettes.py``): :class:`MarchBuilder` maps
the part generators' region keys onto swatch *names and values*, so a build never appends or moves a swatch.
Ink: the monster class (1.5 cm baked) with the Chapter-1 character request of 3.5 px (gate 3.0 px).

:class:`GnawerAnims` (on ``mclips.GoblinAnims``): the attack with an authored contact (the gnawer's quick bite
≈ 300 ms, the bonegorger's long wind-up ≈ 700 ms; ``WindupEnd`` = 62 % of it), a jaw that opens on the wind-up and
snaps shut on the contact, the knuckle-drag idle (sniffing, the free fist on the ground) and the low, fast walk.
"""
from __future__ import annotations

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
from mathutils import Matrix, Vector  # noqa: E402

from kit import mesh as M, outline, rig as R  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402
from kit.rig import Bind, HumanoidSpec  # noqa: E402

import common as C  # noqa: E402
from common import BP, K, V  # noqa: E402
import goblins as GB  # noqa: E402
import mgeo as G  # noqa: E402
import march_palettes as MP  # noqa: E402
from mclips import GoblinAnims  # noqa: E402

FAMILY = "Monsters_March"
OUTLINE_CLASS = "monster"
OUTLINE_PX = 3.5
OUTLINE_PX_NOTE = ("Chapter-1 character gate (dark rim >= 3 px on >= 90 % of the 1080p silhouette edges): the monster "
                   "class's 3.0 px keeps claws, ear stubs, needle teeth and the weapon shaft < 3 px on part of the "
                   "silhouette; 3.5 px, as every Chapter-1 character ships, holds it (gate 3.0 px); the held weapon "
                   "and the hunt attachments ink with it")
# monsters.md §1.2 / DR5: enemy wind-up tints are the telegraph pair (bone fill → crimson for elites), never ember
TELEGRAPH = {"normal": "#FFE8DC", "elite": "#D11F2E"}
TRAIL = "#E8E0CC"                 # bone-white weapon trail (outside the ember band)
# review stage: the march's ash grass (A1 MARCH_GROUND proposal "0", L* 31) instead of the plains green, so every
# preview judges the dark palettes on the ground they were tuned for (darkgate measures the darkest mud separately)
STAGE_HEX = MP.MARCH_GROUND["0"][1]

# every registered Monsters_March swatch: name → Region (exact hex + tone params + ink mix, A1)
MARCH: dict[str, Region] = {n: r for n, r, _ in MP.family_regions(FAMILY)}

# region key (as the part generators use them) → Monsters_March swatch name
GNAWER_ALIAS = dict(
    skin="gnawer.hide", skin_dark="gnawer.hide_dark", skin_line="gnawer.hide_line", belly="gnawer.belly",
    ear="gnawer.ear_stub", ear_in="gnawer.hide_dark", claw="gnawer.claw", teeth="gnawer.teeth",
    mouth="gnawer.maw", gum="gnawer.gum", nose="gnawer.hide_line", eye="gnawer.eye",
    cloth="gnawer.trophy", rope="gnawer.rope", rope_line="gnawer.stick",
)
BONEGORGER_ALIAS = dict(GNAWER_ALIAS, skin="bonegorger.hide", skin_dark="bonegorger.hide_dark",
                        belly="bonegorger.belly", tuft="bonegorger.tuft", eye="bonegorger.eye")


class MarchBuilder(GB.LodHullBuilder):
    """``kit.mesh.Builder`` whose region keys are aliases of registered ``Monsters_March`` swatches (the Living's
    pattern, ``living.MarchBuilder``): ``alias`` maps the part generators' keys (``skin``, ``claw`` …) onto swatch
    names (``gnawer.hide`` …) and the swatch is registered with its A1 value, so no build appends or moves a swatch
    (``Palette.add``: same name and value → the same index). On :class:`goblins.LodHullBuilder`: at LOD1
    (``hull_ratio`` < 1) every outlined part's hull is baked from a proxy decimated that much further."""

    def __init__(self, asset_name: str, palette: Palette, alias: dict, prefix: str = "gnawer", **kw):
        super().__init__(asset_name, palette, prefix=prefix, **kw)
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


def march_builder(asset: str, pal: Palette, alias: dict, prefix: str, lod: int = 0, skip: tuple = (),
                  ratio: float = 0.6, hull_ratio: float = 1.0) -> MarchBuilder:
    """LOD0, or LOD1 (``ratio`` + ``goblins.LOD1_SKIP`` + ``skip``; ``hull_ratio`` < 1: hulls from proxies decimated
    that much further) — ``goblins.builder`` with march swatches."""
    if not lod:
        b = MarchBuilder(asset, pal, alias, prefix=prefix)
    else:
        # the long hooked claws are a gnawer identity cue (monsters.md 2.4): LOD1 keeps them (the goblins skip theirs)
        names = (set(GB.LOD1_SKIP) - set(LOD1_KEEP)) | set(skip)
        b = MarchBuilder(asset, pal, alias, prefix=prefix, lod_ratio=ratio,
                         lod_skip=lambda n: GB.lod_base(n) in names, hull_ratio=hull_ratio)
    b.declare()
    return b


LOD1_KEEP = ("claw", "toe_claw")


def swatch_names(alias: dict) -> list[str]:
    return sorted(set(alias.values()))


def march_stage() -> None:
    """Review stage on the march ash grass (every ``kit.review.Review`` built afterwards; nothing in the kit is
    changed — the module's ``stage`` is wrapped with another default ground)."""
    from kit import review
    if getattr(review.stage, "_af_march", False):
        return
    orig = review.stage

    def stage(radius: float = 30.0, blob_radius: float = 0.39, ground_hex: str = STAGE_HEX, grid: bool = True):
        return orig(radius, blob_radius, STAGE_HEX, grid)
    stage._af_march = True
    review.stage = stage


# ── shape ─────────────────────────────────────────────────────────────────────────────────────────────────
# cranium rings (head units: h up the head axis, back / front along it, lateral radius factor): small and low — the
# goblin's big brain-case is the cute read monsters.md §5 removes
CRANIUM = [(3.9, -1.5, 0.8, 0.95), (3.5, -3.8, 2.6, 0.95), (2.7, -5.2, 3.8, 0.92), (1.6, -5.8, 4.4, 0.88),
           (0.3, -5.8, 4.6, 0.84), (-0.9, -5.2, 4.4, 0.80), (-1.9, -4.2, 3.8, 0.74), (-2.7, -2.6, 2.6, 0.64),
           (-3.1, -1.0, 1.0, 0.52)]


@dataclass
class Shape:
    """Per-monster head / body shape (head units for the head, rig units for the body)."""
    cranium: list = field(default_factory=lambda: list(CRANIUM))
    # upper muzzle: (h, f, half width, half height) from inside the face to the nose tip
    muzzle: tuple = ((1.7, 2.6, 3.0, 2.3), (1.2, 5.6, 2.6, 2.0), (0.6, 8.4, 2.05, 1.6), (-0.1, 10.9, 1.45, 1.2),
                     (-0.55, 12.4, 0.95, 0.85))
    # lower jaw: (h, f, half width, half height) from the hinge to the chin tip
    jaw: tuple = ((-1.0, -0.2, 2.6, 1.45), (-2.0, 3.0, 2.45, 1.35), (-2.55, 6.8, 1.95, 1.1), (-2.55, 9.9, 1.35, 0.9),
                  (-2.25, 11.5, 0.8, 0.6))
    jaw_hinge: tuple = (-0.9, 0.0)            # (h, f) of the jaw bone's head
    jaw_tip: tuple = (-2.4, 11.6)
    teeth_up: tuple = (3.9, 5.6, 7.2, 8.7, 10.1, 11.3)        # f of the upper needles (each side)
    teeth_dn: tuple = (4.7, 6.4, 8.0, 9.5, 10.8)               # f of the lower needles (each side)
    tooth_len: float = 1.55                    # head units
    tooth_r: float = 0.3
    tooth_len_dn: float | None = None          # lower row (None: .85 × tooth_len); the bonegorger's underbite tusks
    teeth_dn_k: tuple | None = None            # per lower tooth length factors (None: the gentle taper)
    tooth_r_dn: float | None = None
    eye: tuple = (2.55, 4.0, 2.15)            # (h, f, lat) of the eye centre
    eye_r: tuple = (2.0, 1.45, 0.85)          # half extents (across, up, deep), head units: the eyeshine is the
    #                                           family marker and reads ≥ 2-3 px at the W1 camera (at 1.65 × 1.15 the
    #                                           darkest-tile gate measured exactly 2 px)
    ear: tuple = (2.6, 2.9)                    # torn stub length, width (head units)
    head_tilt: float | None = 0.12             # rad from vertical of the head's modelling frame (None: along the neck)
    head_drop: float = 0.0                     # rig units the head centre sits below the neck's end
    arm_down: float = 62.0                     # rest A-pose arm angle (deg below horizontal)
    grime_arm: float = 0.42                    # forearm share (elbow → wrist) above the grave grime line
    grime_leg: float = 0.30                    # shin share (knee → ankle) above it
    ribs: int = 5                              # painted rib strokes per side (0: none)
    spine_knobs: int = 7
    tufts: int = 0                             # mangy tufts (bonegorger)
    belly_k: float = 0.0                       # sagging belly (bonegorger)
    neck_k: float = 1.0                        # neck girth
    fist_k: float = 1.0                        # hand size (the fist, knuckles, claws, thumb)
    loin_len: float = 0.85                     # loin scrap length × the loin chain
    loin_w: tuple = (3.0, 2.3)                 # loin scrap width at the belt / hem (u)
    loin_out: float = 0.4                      # u the scrap hangs out in front of the belt


def grime_split(part: M.Part, a: Vector, b: Vector, frac: float) -> tuple[M.Part, M.Part]:
    """Split a limb tube at ``frac`` along ``a`` → ``b`` with a ragged edge: (clean part, grimed part below)."""
    a, b = Vector(a), Vector(b)
    d = b - a
    L2 = max(d.length_squared, 1e-9)
    side = d.normalized().orthogonal().normalized()
    side2 = d.normalized().cross(side)

    def pred(bm, f):
        c = f.calc_center_median()
        t = (c - a).dot(d) / L2
        r = c - a - d * ((c - a).dot(d) / L2)
        ang = math.atan2(r.dot(side2), r.dot(side))
        return t > frac + 0.07 * math.sin(3.0 * ang) + 0.04 * math.sin(7.0 * ang + 1.3)
    return C.split_faces(part, pred)


# ── kit ───────────────────────────────────────────────────────────────────────────────────────────────────
OVERHEAD_K = 1.57      # fx_overhead height / the head centre's rest height (≈ 1.20 m gravegnawer, 1.45 m bonegorger)


class GnawerKit(GB.GoblinKit):
    """``goblins.GoblinKit`` re-shaped into a carrion beast (see the module docstring). ``shape`` holds the head and
    body knobs that differ between the gravegnawer and the bonegorger."""

    def __init__(self, asset: str, prefix: str, web: GB.Web, look: GB.Look, shape: Shape):
        """``goblins.GoblinKit.__init__`` with one change: the head is modelled in its own frame (``Shape.head_tilt``
        from vertical) instead of along the neck. A carrion beast's neck thrusts forward off a near-horizontal back
        (rig ``rest_lean + rest_head`` ≈ 66°) while its skull stays level, muzzle forward — on the goblin frame the
        face would look at the ground."""
        self.shape = shape
        self.asset, self.prefix, self.w, self.look = asset, prefix, web, look
        U = web.U
        self.U = U
        self.spec = HumanoidSpec(
            name=asset, skeleton=GB.SKELETON, thigh=web.thigh * U, shin=web.shin * U, ankle=web.ankle * U,
            torso=web.torso * U, neck=web.neck * U, head=14.0 * web.head_k * U, hip_half=web.hip * U,
            shoulder_half=web.shoulder * U, upper_arm=web.upper_arm * U, fore_arm=web.fore_arm * U,
            hand=4.4 * U, foot_len=web.foot_len * U, arm_down=shape.arm_down, elbow_prebend=12.0, knee_prebend=8.0,
            rest_lean=web.lean, rest_head=web.head_tilt, face=True)
        self.rig = R.build_humanoid(self.spec)
        self.J = self.rig.joints
        self.tilt = shape.head_tilt if shape.head_tilt is not None else web.lean + web.head_tilt
        self.HC = self.J["head_center"].copy() + Vector((0.0, 0.0, U * shape.head_drop))
        self.Hm = Matrix.Translation(self.HC) @ Matrix.Rotation(self.tilt, 4, "X")
        self.head = G.ProfileLoft(self.skull_profile(), self.Hm)
        self.torso = self.torso_loft()
        self._place_face_bones()
        self._place_overhead()
        self._add_chains()

    # ── head frame ────────────────────────────────────────────────────────────────────────────────────────
    def skull_profile(self) -> C.Profile:
        rings = []
        for h, back, front, wk in self.shape.cranium:
            d = (front - back) / 2
            c = (front + back) / 2
            rings.append((self.hk(h), self.hk(d * wk), self.hk(d), -self.hk(c)))
        return C.Profile(rings, exp=2.2)

    def eye_center(self, sg: int) -> Vector:
        h, f, lat = self.shape.eye
        return self.hp(h, f, sg * lat)

    def eye_dir(self, sg: int) -> Vector:
        """Where an eye looks: forward, out to its side and up (the W1 camera looks down 50°)."""
        return self.hdir(1.15, 1.0, sg * 0.75)

    def _place_face_bones(self) -> None:
        """Eye bones on the eyes (fx_eye sockets); the jaw hinged under the eye, pointing along the lower jaw to the
        chin tip (the whole lower jaw drops when it bites)."""
        arm = self.rig.obj
        from kit import scene
        scene.select_only([arm])
        bpy.ops.object.mode_set(mode="EDIT")
        eb = arm.data.edit_bones
        for s, sg in (("l", 1), ("r", -1)):
            e = self.eye_center(sg)
            b = eb[f"eye_{s}"]
            b.head, b.tail = e, e + self.eye_dir(sg) * 0.03
            b.align_roll(self.hdir(1, 0))
            self.J[f"eye_{s}"] = e
        jb = eb["jaw"]
        jb.head = self.hp(*self.shape.jaw_hinge)
        jb.tail = self.hp(*self.shape.jaw_tip)
        jb.align_roll(Vector((1, 0, 0)).cross((jb.tail - jb.head).normalized()))
        bpy.ops.object.mode_set(mode="OBJECT")
        for so in self.rig.sockets:
            if so["name"] in ("fx_eye_l", "fx_eye_r"):
                so["pos"] = self.J["eye_" + so["name"][-1]]

    def _place_overhead(self) -> None:
        """``fx_overhead`` (the elite marker, nameplates; review round 2 of the A8 marker): on ``root``, centred over
        the feet and ``OVERHEAD_K`` × the head centre's rest height up — not on the head bone, whose forward-thrust,
        nodding head carried the marker into the head and back silhouettes at the W1 camera. Measured on the shipped
        Idle (``elite_sigil.py --gaps``): ≥ 8 px clear of the gravegnawer / bonegorger in se, sw, ne and nw for the
        W2 sigil, ≥ 16 px for the W1 crown (gate 4 px)."""
        for so in self.rig.sockets:
            if so["name"] == "fx_overhead":
                so["bone"] = "root"
                so["pos"] = Vector((0.0, 0.0, OVERHEAD_K * self.J["head_center"].z))

    # ear stubs: the chain bones sit on the stubs (short; the stub hangs on ear_*_01)
    def ear_base(self, sg: int) -> Vector:
        return self.hp(2.5, -2.8, sg * 3.9)

    def ear_mid(self, sg: int) -> Vector:
        return self.hp(3.4, -4.2, sg * 5.2)

    def ear_tip(self, sg: int) -> Vector:
        return self.hp(4.0, -5.2, sg * 6.0)

    # ── head parts ────────────────────────────────────────────────────────────────────────────────────────
    def add_head(self, b: M.Builder, ears: bool = True, face: bool = True) -> dict:
        """Cranium, upper muzzle + nose pad, lower jaw (on ``jaw``), maw, needle teeth, eyes, brow, ear stubs,
        neck."""
        hk = self.hk
        sh = self.shape
        head = Bind.rigid("head")
        jaw = Bind.rigid("jaw")
        zs = sorted({hk(h) for h, *_ in sh.cranium} | {hk(h) for h in np.linspace(sh.cranium[-1][0],
                                                                                      sh.cranium[0][0], 9)})
        skull = self.head.loft(zs, sides=22, cap0="fan", cap1="fan", dome0=hk(0.3), dome1=hk(0.1))
        sid = b.add(skull, "skin", head, name="skull", hull_pad=True)
        ids = {"skull": sid}
        # upper muzzle (snout): a flattened tapering sweep from inside the face to the nose; its underside is the
        # dark roof of the mouth (seen when the jaw drops)
        up = self.hdir(1, 0)
        pts = [self.hp(h, f) for h, f, _, _ in sh.muzzle]
        sc = [(hk(hh), hk(hw)) for _, _, hw, hh in sh.muzzle]
        snout = M.sweep(pts, M.superellipse(1, 1, 12, 2.5), scales=sc, up=tuple(up), dome0=0.0, dome1=hk(0.35))
        Hinv = self.Hm.inverted()
        roof_h = lambda f: np.interp(f, [m[1] for m in sh.muzzle], [m[0] - m[3] * 0.55 for m in sh.muzzle])
        snout, roof = C.split_faces(snout, lambda bm, f: self._local(f.calc_center_median(), Hinv)[0]
                                    < roof_h(self._local(f.calc_center_median(), Hinv)[1]) and
                                    f.normal.dot(-up) > 0.55)
        mid = b.add(snout, "skin", head, name="muzzle", hull_pad=True)
        b.add(roof, "mouth", head, name="muzzle_roof", outline=False, covered_by=mid)
        ids["muzzle"] = mid
        # the nose pad at the tip and two nostril slits
        h, f = sh.muzzle[-1][0], sh.muzzle[-1][1]
        nose = M.ellipsoid((hk(1.05), hk(0.85), hk(0.62)), (0, 0, 0), sides=8, rings=5)
        nose.transform(Matrix.Translation(self.hp(h + 0.35, f + 0.15)) @ self.Hm.to_3x3().to_4x4())
        b.add(nose, "nose", head, name="nose", outline=False, covered_by=mid)
        # lower jaw on the jaw bone (top face = the dark floor of the mouth)
        jpts = [self.hp(h, f) for h, f, _, _ in sh.jaw]
        jsc = [(hk(hh), hk(hw)) for _, _, hw, hh in sh.jaw]
        jawp = M.sweep(jpts, M.superellipse(1, 1, 12, 2.4), scales=jsc, up=tuple(up), dome0=hk(0.4),
                       dome1=hk(0.3))
        floor_h = lambda f: np.interp(f, [m[1] for m in sh.jaw], [m[0] + m[3] * 0.45 for m in sh.jaw])
        jawp, floor = C.split_faces(jawp, lambda bm, f: self._local(f.calc_center_median(), Hinv)[0]
                                    > floor_h(self._local(f.calc_center_median(), Hinv)[1]) and
                                    f.normal.dot(up) > 0.55 and self._local(f.calc_center_median(), Hinv)[1] > 1.5)
        jid = b.add(jawp, "skin", jaw, name="jaw", hull_pad=True)
        b.add(floor, "mouth", jaw, name="jaw_floor", outline=False, covered_by=jid)
        ids["jaw"] = jid
        # maw: a near-black wedge between the jaws (its top rides the head, its bottom the jaw), so an open bite
        # shows a dark throat, never the far side of the head
        self.add_maw(b, mid)
        self.add_teeth(b, head, jaw, mid, jid)
        if face:
            self._add_face(b, ids, sid)
        if ears:
            for s, sg in (("l", 1), ("r", -1)):
                ids[f"ear_{s}"] = self.add_ear(b, s, sg)
        J = self.J
        n0 = J["neck"] + Vector((0, 0.004, -0.02))
        n1 = self.hp(-1.6, -2.4)
        nm = n0.lerp(n1, 0.5) + Vector((0, 0, self.u(0.4)))
        nk = self.shape.neck_k
        neck = M.tube([n0, nm, n1], [self.u(2.3 * nk), self.u(1.75 * nk), self.u(2.0 * nk)], sides=10)
        b.add(neck, "skin", Bind.blend("spine_03", "neck_01", "head", falloff=3.0), name="neck")
        return ids

    def _local(self, co, Hinv):
        """Armature point → (h, f) head units."""
        lc = Hinv @ Vector(co)
        hu = self.hk(1.0)
        return lc.z / hu, -lc.y / hu

    def mouth_lines(self, fs):
        """(roof h, floor h) of the mouth at forward positions ``fs`` (head units)."""
        sh = self.shape
        roof = np.interp(fs, [m[1] for m in sh.muzzle], [m[0] - m[3] * 0.5 for m in sh.muzzle])
        floor = np.interp(fs, [m[1] for m in sh.jaw], [m[0] + m[3] * 0.3 for m in sh.jaw])
        return roof, floor

    def add_maw(self, b: M.Builder, mid: int) -> None:
        hk = self.hk
        sh = self.shape
        up = self.hdir(1, 0)
        Hinv = self.Hm.inverted()
        fs = np.array([0.8, 3.5, 6.5, 9.2])
        roof, floor = self.mouth_lines(fs)
        wid = np.interp(fs, [m[1] for m in sh.jaw], [m[2] * 0.72 for m in sh.jaw])
        pts = [self.hp((r + fl) * 0.5, f) for r, fl, f in zip(roof, floor, fs)]
        hh = [hk(max(0.3, (r - fl) * 0.5 + 0.15)) for r, fl in zip(roof, floor)]
        maw = M.sweep(pts, M.superellipse(1, 1, 8, 2.2), scales=[(h_, hk(w)) for h_, w in zip(hh, wid)],
                      up=tuple(up), dome0=hk(0.2), dome1=hk(0.2))
        hu = hk(1.0)
        mid_h = (roof + floor) * 0.5

        def w(co):
            loc = (np.c_[co, np.ones(len(co))] @ np.array(Hinv).T)[:, :3] / hu
            h, f = loc[:, 2], -loc[:, 1]
            wj = 1.0 / (1.0 + np.exp((h - np.interp(f, fs, mid_h)) * 6.0))     # below the mouth line → the jaw
            return np.stack([1.0 - wj, wj], axis=1)
        b.add(maw, "mouth", Bind.custom(("head", "jaw"), w), name="maw", outline=False, covered_by=mid)

    def add_teeth(self, b: M.Builder, head: Bind, jaw: Bind, mid: int, jid: int) -> None:
        """Needle teeth: the upper row hangs from the snout's lip past the lower jaw, the lower row stands up past
        the snout — they overlap the lips with the mouth shut (the read from the W1 camera: a pale saw along the
        muzzle)."""
        hk = self.hk
        sh = self.shape
        up = self.hdir(1, 0)
        mf = [m[1] for m in sh.muzzle]
        jf = [m[1] for m in sh.jaw]
        for sg in (1, -1):
            for i, f in enumerate(sh.teeth_up):
                h = float(np.interp(f, mf, [m[0] - m[3] * 0.62 for m in sh.muzzle]))
                lat = float(np.interp(f, mf, [m[2] * 0.82 for m in sh.muzzle]))
                base = self.hp(h + 0.25, f, sg * lat)
                L = hk(sh.tooth_len * (1.0 - 0.18 * abs(i - 2) / 3))
                tip = base - up * L + self.hdir(0, 0.15, sg * 0.25) * hk(0.25)
                b.add(G.cone(base, tip, hk(sh.tooth_r), sides=4), "teeth", head, name="tooth", outline=False,
                      covered_by=mid)
            for i, f in enumerate(sh.teeth_dn):
                h = float(np.interp(f, jf, [m[0] + m[3] * 0.62 for m in sh.jaw]))
                lat = float(np.interp(f, jf, [m[2] * 0.86 for m in sh.jaw]))
                base = self.hp(h - 0.25, f, sg * lat)
                kk = sh.teeth_dn_k[i] if sh.teeth_dn_k else (1.0 - 0.15 * abs(i - 2) / 3)
                L = hk((sh.tooth_len_dn or sh.tooth_len * 0.85) * kk)
                tip = base + up * L + self.hdir(0, 0.1, sg * 0.3) * hk(0.25)
                b.add(G.cone(base, tip, hk(sh.tooth_r_dn or sh.tooth_r * 0.95), sides=4), "teeth", jaw,
                      name="tooth_low", outline=False, covered_by=jid)

    def _add_face(self, b: M.Builder, ids: dict, skull_id: int) -> None:
        """Deep-set bone-yellow eyeshine (the family marker; no pupils) under a low brow ridge."""
        hk = self.hk
        sh = self.shape
        for s, sg in (("l", 1), ("r", -1)):
            ec = self.eye_center(sg)
            n = self.eye_dir(sg)
            m = G.frame_z(ec, n, self.hdir(1, 0))
            ex, ey, ez = sh.eye_r
            eye = M.ellipsoid((hk(ex), hk(ey), hk(ez)), (0, 0, 0), sides=10, rings=6)
            # almond: the outer corner lifted (a slant that reads feral, not cute)
            eye.transform(m @ Matrix.Rotation(math.radians(sg * 14.0), 4, "Z"))
            b.add(eye, "eye", Bind.rigid(f"eye_{s}"), name=f"eye_{s}", outline=False, covered_by=skull_id)
        ids["brow"] = b.add(self._closed_brow(), "skin", Bind.rigid("head"), name="brow", hull_pad=True)

    def _closed_brow(self) -> M.Part:
        """A low brow ridge over both eyes, dipping to the snout between them (a scowl)."""
        hk = self.hk
        h0, f0, lat0 = self.shape.eye
        pts, rad = [], []
        for t in np.linspace(-1.0, 1.0, 9):
            lat = t * (lat0 + 1.5)
            dip = 0.55 * (1.0 - abs(t)) ** 1.5
            pts.append(self.hp(h0 + 1.05 - dip - 0.35 * abs(t), f0 + 0.15 - 0.9 * abs(t) ** 2, lat))
            rad.append((hk(0.62 * (1 - 0.45 * abs(t))), hk(0.8 * (1 - 0.4 * abs(t)))))
        return M.sweep(pts, M.superellipse(1, 1, 8, 2.4), scales=rad, up=tuple(self.hdir(1, 0.3)), dome0=hk(0.25),
                       dome1=hk(0.25))

    def add_ear(self, b: M.Builder, s: str, sg: int) -> int:
        """A torn ear stub: a short ragged leaf on ``ear_*_01`` (the goblin's big ear cut off at a third)."""
        hk = self.hk
        L, W = self.shape.ear
        base = self.ear_base(sg)
        d = (self.ear_mid(sg) - base).normalized()
        face = self.hdir(0.2, 0.6, sg * 1.0).normalized()
        m = R.frame(base - d * hk(0.6), d, -face)
        ear = G.leaf_solid(hk(L + 0.6), hk(W), hk(0.55), cup=0.3, bend=0.12, twist_deg=-sg * 6.0, rows=6,
                           prof=[(0.0, 0.62), (0.22, 0.95), (0.45, 0.92), (0.58, 0.55), (0.68, 0.8), (0.82, 0.5),
                                 (0.9, 0.62), (1.0, 0.0)],
                           inner=(0.15, 0.8, 0.6))
        ear.transform(m)
        return b.add(ear, "ear", Bind.rigid(f"ear_{s}_01"), name=f"ear_{s}", sub_regions={1: "ear_in"},
                     hull_pad=True)

    # ── torso ─────────────────────────────────────────────────────────────────────────────────────────────
    def torso_rings(self):
        """Emaciated: a pinched waist under a ribcage (the goblin's pot belly is gone); ``belly_k`` sags it again
        for the bloated bruiser."""
        k = self.w.bulk
        u = self.u
        bk = self.shape.belly_k
        return [(-0.16, u(3.4 * k), u(3.1 * k), u(0.0)),
                (-0.05, u(4.3 * k), u(3.7 * k), u(0.15 + 0.4 * bk)),
                (0.10, u(4.25 * k * (1 + 0.28 * bk)), u(3.4 * k * (1 + 0.62 * bk)), u(-0.2 + 1.9 * bk)),
                (0.26, u(4.05 * k * (1 + 0.36 * bk)), u(3.3 * k * (1 + 0.78 * bk)), u(-0.4 + 2.5 * bk)),
                (0.42, u(4.6 * k * (1 + 0.24 * bk)), u(4.0 * k * (1 + 0.5 * bk)), u(0.0 + 1.6 * bk)),
                (0.58, u(5.15 * k), u(4.55 * k), u(0.4 + 0.3 * bk)),
                (0.72, u(5.25 * k), u(4.55 * k), u(0.35)),
                (0.86, u(4.9 * k), u(3.9 * k), u(-0.05)),
                (0.96, u(3.8 * k), u(3.0 * k), u(-0.35)),
                (1.05, u(2.35), u(2.2), u(-0.3))]

    def add_torso(self, b: M.Builder, trophies: bool = True, belt: bool = True) -> dict:
        t = self.torso
        u = self.u
        sh = self.shape
        spine = Bind.blend("pelvis", "spine_01", "spine_02", "spine_03", falloff=3.0, smooth=0.02)
        body = t.loft(-0.16, 1.05, ss=list(np.linspace(-0.16, 1.05, 15)), sides=24, dome0=u(0.6), cap1=None)
        rest, bel = G.split_by(body, lambda s, a, c: 0.08 < s < 0.56 and abs(a) < 46
                               and ((s - 0.32) / 0.24) ** 2 + (a / 46.0) ** 2 < 1.0)
        tid = b.add(rest, "skin", spine, name="torso",
                    hull=t.loft(-0.16, 1.05, ss=list(np.linspace(-0.16, 1.05, 10)), sides=18, dome0=u(0.6),
                                cap1="fan"), hull_pad=True)
        b.add(bel, "belly", spine, name="belly", outline=False, covered_by=tid)
        ids = {"torso": tid}
        # rib strokes: arcs round each flank from under the breast to the back (painted, single-sided)
        for i in range(sh.ribs):
            s0 = 0.70 - 0.075 * i
            for sg in (1, -1):
                path = [(s0 + 0.035, sg * 28.0), (s0 + 0.01, sg * 62.0), (s0 - 0.02, sg * 98.0),
                        (s0 - 0.01, sg * 128.0)]
                st = G.surface_paint(t, path, u(0.62 - 0.05 * i), u(0.12), steps=4, taper=0.6)
                b.add(st, "skin_line", spine, name="rib", outline=False, covered_by=tid)
        # spine ridge: vertebra knobs down the back (the high camera sees the back of a hunched beast)
        for i in range(sh.spine_knobs):
            s_ = 0.98 - 0.85 * i / max(1, sh.spine_knobs - 1)
            r = u(0.95 - 0.25 * i / max(1, sh.spine_knobs - 1))
            b.add(G.surface_dot(t, s_, 180.0, r, r * 0.85, low=True), "skin", spine, name="spine_knob",
                  outline=False, covered_by=tid)
        # shoulder blades jutting under the hide
        for sg in (1, -1):
            c = t.point(0.80, sg * 148.0, -u(0.4))
            nb = t.normal(0.80, sg * 148.0)
            sb = M.ellipsoid((u(2.3), u(2.9), u(1.2)), (0, 0, 0), sides=8, rings=5)
            sb.transform(G.frame_z(c, nb, Vector((0, 0, 1))))
            b.add(sb, "skin", Bind.blend("spine_03", f"clavicle_{'l' if sg > 0 else 'r'}", falloff=3.0),
                  name="blade", outline=False, covered_by=tid)
        if belt:
            ids["belt"] = self.add_rope_belt(b, tid, s0=0.07, width_u=1.3, thick_u=0.55)
        if trophies:
            self.add_loin_scrap(b)
        for s, sg in (("l", 1), ("r", -1)):
            S = self.J[f"shoulder_{s}"]
            d = M.ellipsoid((u(1.85), u(1.9), u(1.85)), S + Vector((-sg * u(0.2), 0.0, u(0.1))), sides=8, rings=6)
            b.add(d, "skin", Bind.blend(f"clavicle_{s}", f"upperarm_{s}", "spine_03", falloff=4.0),
                  name=f"deltoid_{s}")
        return ids

    def add_loin_scrap(self, b: M.Builder) -> None:
        """A ragged scrap of crusade tabard hanging from the rope belt (front only: a trophy, not clothing)."""
        u = self.u
        top, mid, bot = self.loin_points(1)
        L = (bot - top).length
        sh = self.shape
        fl = G.flap(u(sh.loin_w[0]), u(sh.loin_w[1]), L * sh.loin_len, thick=0.006, cols=5, rows=5, notch=u(1.6),
                    tails=2, ragged=u(0.7), seed=23, bulge=u(0.25), flare=u(0.05))
        fwd = self.torso.normal(0.06, 0.0)
        fl.transform(Matrix.Translation(top + fwd * u(sh.loin_out)) @ Matrix.Rotation(math.radians(-8.0), 4, "Z"))
        b.add(fl, "cloth", GB.chain_bind(top, bot, ("pelvis", "loin_f_01", "loin_f_02")), name="loin_f",
              sub_regions={1: "cloth", 2: "cloth"}, hull_pad=True)

    def add_arm_rag(self, b: M.Builder, s: str = "l") -> None:
        """A tabard rag knotted round the upper arm, its two ends hanging (the second trophy)."""
        u = self.u
        J = self.J
        S, E = J[f"shoulder_{s}"], J[f"elbow_{s}"]
        c0 = S.lerp(E, 0.55)
        d = (E - S).normalized()
        band = M.tube([c0 - d * u(0.7), c0 + d * u(0.7)], [u(1.85), u(1.75)], sides=10, dome0=0.0, dome1=0.0)
        bind = Bind.rigid(f"upperarm_{s}")
        rid = b.add(band, "cloth", bind, name=f"arm_rag_{s}", hull_pad=True)
        sg = 1 if s == "l" else -1
        out = Vector((sg, 0.0, 0.0))
        k0 = c0 + out * u(1.6) + Vector((0, -u(0.4), 0))
        for j, (dl, dz) in enumerate(((0.9, -3.4), (-0.4, -2.6))):
            tip = k0 + d * u(dz * 0.6) + Vector((0, u(dl), 0)) + out * u(0.6)
            tail = G.chain_strip([k0, k0.lerp(tip, 0.5) + out * u(0.4), tip], [u(1.3), u(1.0), u(0.6)],
                                 d.cross(out).normalized(), out, u(0.3), tails=2, seed=31 + j, cols=3, per_seg=1)
            # inked on their own (covered by the band, its padded proxy ballooned 2.4 cm round the hanging ends)
            b.add(tail, "cloth", bind, name=f"arm_rag_tail_{s}", sub_regions={1: "cloth", 2: "cloth"},
                  hull_pad=True)
        del rid

    def add_rope_belt(self, b: M.Builder, tid: int, s0: float = 0.10, region: str = "rope",
                      stroke: str | None = "rope_line", width_u: float = 1.6, thick_u: float = 0.7) -> int:
        """The goblin rope belt (``goblins.GoblinKit.add_rope_belt``: band, twist strokes, a knot at the front-left,
        two hanging ends) on the thin waist — the knot and the ends ink on their own hulls (covered by the torso,
        whose padded proxy bulged ≈ 4 cm round the hanging ends)."""
        t = self.torso
        u = self.u
        band = G.surface_ring(t, lambda a: s0 + 0.012 * math.cos(math.radians(a)), u(width_u), u(thick_u),
                              segs=24)
        bind = Bind.blend("pelvis", "spine_01", falloff=3.0)
        bid = b.add(band, region, bind, name="belt", outline=False, covered_by=tid)
        if stroke:
            for k in range(16):
                a = -180 + k * 22.5 + 4
                st = G.surface_paint(t, [(s0 - 0.045, a - 5), (s0 + 0.045, a + 7)], u(0.46), u(thick_u) + 0.0015,
                                     steps=2)
                b.add(st, stroke, bind, name="belt_twist", outline=False, covered_by=tid)
        knot = M.ellipsoid((u(1.2), u(0.9), u(1.0)), t.point(s0, 28.0, u(thick_u) + u(0.4)), sides=8, rings=5)
        b.add(knot, region, bind, name="belt_knot", hull_pad=True)
        for da in (22.0, 34.0):
            p0 = t.point(s0, da, u(thick_u) + u(0.3))
            end = M.tube([p0, p0 + Vector((0, -u(0.5), -u(2.4))), p0 + Vector((0, -u(0.7), -u(3.6)))],
                         [u(0.42), u(0.38), u(0.32)], sides=6)
            b.add(end, region, bind, name="belt_end")
        return bid

    # ── limbs ─────────────────────────────────────────────────────────────────────────────────────────────
    def add_arm(self, b: M.Builder, s: str, sg: int, r_scale: float = 0.82) -> None:
        """Long scrawny arm (+ 15 % in the web numbers), a knobbly elbow and the clawed hand."""
        J = self.J
        u = self.u
        S, E, W = J[f"shoulder_{s}"], J[f"elbow_{s}"], J[f"wrist_{s}"]
        k = r_scale
        pts = [S + (S - E).normalized() * u(0.4), S.lerp(E, 0.45), E, E.lerp(W, 0.35), E.lerp(W, 0.75), W]
        rad = [u(2.2 * k), u(1.85 * k), u(1.7 * k), u(1.75 * k), u(1.45 * k), u(1.3 * k)]
        arm = M.tube(pts, rad, sides=9, dome0=u(0.3), dome1=u(0.3))
        bind = Bind.blend(f"clavicle_{s}", f"upperarm_{s}", f"lowerarm_{s}", f"hand_{s}", falloff=4.0, smooth=0.008)
        # grave grime: the forearm below a ragged line is caked dark (they dig), the upper arm stays ash-pale
        up_, grime = grime_split(arm, E, W, self.shape.grime_arm)
        aid = b.add(up_, "skin", bind, name=f"arm_{s}", hull=M.tube(pts, rad, sides=9, dome0=u(0.3), dome1=u(0.3)),
                    hull_pad=True)
        b.add(grime, "skin_dark", bind, name=f"arm_grime_{s}", outline=False, covered_by=aid)
        eb = M.ellipsoid((u(1.75 * k), u(1.9 * k), u(1.75 * k)), E + (E - S).normalized() * u(0.25)
                         + Vector((sg * u(0.2), u(0.55), 0)), sides=8, rings=5)
        b.add(eb, "skin", Bind.rigid(f"lowerarm_{s}"), name=f"elbow_{s}")
        self.add_fist(b, s, sg)

    def add_fist(self, b: M.Builder, s: str, sg: int) -> int:
        """A narrow knuckled hand curled round the grip / the ground, three long hooked claws off the knuckles and a
        clawed thumb."""
        J = self.J
        fk = self.shape.fist_k
        u = lambda n: self.u(n * fk)
        W, T = J[f"wrist_{s}"], J[f"handtip_{s}"]
        hb = self.rig.obj.data.bones[f"hand_{s}"]
        X = hb.matrix_local.to_3x3() @ Vector((1, 0, 0))
        d = (T - W).normalized()
        palm = X.cross(d).normalized()
        if palm.dot(Vector((0, -1, 0))) < 0:
            palm = -palm
        m = R.frame(W, d, palm)
        fist = M.ellipsoid((u(1.7), u(2.25), u(1.6)), (0, u(1.9), u(0.3)), sides=9, rings=6)
        fist.transform(m)
        hb_bind = Bind.blend(f"hand_{s}", f"fingers_01_{s}", f"lowerarm_{s}", falloff=5.0)
        fid = b.add(fist, "skin_dark", hb_bind, name=f"fist_{s}", hull_pad=True)
        for i, lat in enumerate((-1.0, 0.0, 1.0)):
            kn = m @ Vector((lat * u(0.85), u(3.2), u(1.3)))
            b.add(M.ellipsoid((u(0.55), u(0.55), u(0.5)), kn, sides=6, rings=4), "skin_dark", hb_bind,
                  name=f"knuckle_{s}", outline=False, covered_by=fid)
            # hooked claw: base on the knuckle, curving forward and down past the fingers
            c0 = m @ Vector((lat * u(0.8), u(3.7), u(1.0)))
            c1 = m @ Vector((lat * u(0.95), u(5.0), u(0.35)))
            c2 = m @ Vector((lat * u(1.05), u(5.6), u(-0.9)))
            claw = M.tube([c0, c1, c2], [u(0.42), u(0.3), u(0.06)], sides=5, dome0=u(0.1), dome1=0.0)
            # each claw inks on its own hull (covered by the fist, the fist's padded proxy grew 4.6 cm into a dark
            # mitten round the three hooks)
            b.add(claw, "claw", hb_bind, name=f"claw_{s}")
        th = M.capsule(m @ Vector((-sg * u(1.3), u(1.0), u(1.1))), m @ Vector((-sg * u(0.8), u(2.5), u(1.8))),
                       u(0.62), u(0.5), sides=7, cap_rings=1, mid=0)
        b.add(th, "skin_dark", hb_bind, name=f"thumb_{s}", outline=False, covered_by=fid)
        return fid

    def add_leg(self, b: M.Builder, s: str, sg: int, r_scale: float = 0.85, top_up: float = 1.4,
                top_r: float = 2.4) -> None:
        J = self.J
        u = self.u
        k = r_scale
        Hp, Kn, A = J[f"hip_{s}"], J[f"knee_{s}"], J[f"ankle_{s}"]
        top = Hp + Vector((0, 0, u(top_up)))
        pts = [top, Hp.lerp(Kn, 0.45), Kn, Kn.lerp(A, 0.45), A.lerp(Kn, 0.08)]
        rad = [u(top_r * k), u(2.15 * k), u(1.75 * k), u(1.55 * k), u(1.2 * k)]
        leg = M.tube(pts, rad, sides=8, dome0=u(0.8), dome1=u(0.2))
        bind = Bind.blend("pelvis", f"thigh_{s}", f"calf_{s}", f"foot_{s}", falloff=4.0, smooth=0.008)
        up_, grime = grime_split(leg, Kn, A, self.shape.grime_leg)
        lid = b.add(up_, "skin", bind, name=f"leg_{s}", hull=M.tube(pts, rad, sides=8, dome0=u(0.8), dome1=u(0.2)),
                    hull_pad=True)
        b.add(grime, "skin_dark", bind, name=f"leg_grime_{s}", outline=False, covered_by=lid)
        kn = M.ellipsoid((u(1.9 * k), u(1.75 * k), u(1.9 * k)), Kn + Vector((0, -u(0.45), u(0.1))), sides=8, rings=5)
        b.add(kn, "skin", Bind.rigid(f"calf_{s}"), name=f"knee_{s}")
        self.add_foot(b, s, sg)

    def add_foot(self, b: M.Builder, s: str, sg: int) -> int:
        """The long goblin foot with three hooked toe claws (``claw``)."""
        J = self.J
        u = self.u
        A = J[f"ankle_{s}"]
        L = self.spec.foot_len
        x = A.x
        st = [(0.24 * L, u(1.0), u(1.05), u(1.1)), (0.10 * L, u(1.45), u(1.45), u(1.4)),
              (-0.12 * L, u(1.6), u(1.15), u(1.15)), (-0.36 * L, u(1.55), u(0.92), u(0.98)),
              (-0.56 * L, u(1.25), u(0.78), u(1.08)), (-0.70 * L, u(0.85), u(0.62), u(1.3))]
        path = [Vector((x, y, cz)) for y, _, _, cz in st]
        foot = M.sweep(path, M.superellipse(1, 1, 10, 2.6), scales=[(hh, hw) for _, hw, hh, _ in st], up=(0, 0, 1),
                       dome0=u(0.6), dome1=u(0.4))
        foot.deform(lambda co: Vector((co.x, co.y, max(co.z, 0.0))))
        fb = Bind.blend(f"calf_{s}", f"foot_{s}", f"ball_{s}", falloff=5.0)
        fid = b.add(foot.smooth(True), "skin_dark", fb, name=f"foot_{s}", hull_pad=True)
        for lat in (-0.8, 0.0, 0.8):
            c0 = Vector((x + lat * u(0.8), -0.64 * L + abs(lat) * u(0.7), u(1.45)))
            c1 = c0 + Vector((lat * u(0.25), -u(1.0), u(-0.25)))
            c2 = c1 + Vector((lat * u(0.15), -u(0.6), -u(0.75)))
            claw = M.tube([c0, c1, c2], [u(0.42), u(0.3), u(0.06)], sides=5, dome0=u(0.1), dome1=0.0)
            claw.deform(lambda co: Vector((co.x, co.y, max(co.z, 0.002))))
            b.add(claw, "claw", Bind.blend(f"foot_{s}", f"ball_{s}", falloff=5.0), name=f"toe_claw_{s}")
        return fid

    def add_tufts(self, b: M.Builder, tid: int) -> None:
        """Mangy tufts (``tuft``): ragged clumps on the shoulders, the back and the forearms' outer edge."""
        t = self.torso
        u = self.u
        spine = Bind.blend("pelvis", "spine_01", "spine_02", "spine_03", falloff=3.0, smooth=0.02)
        rng = np.random.default_rng(int.from_bytes(self.asset.encode()[:4], "little"))
        spots = [(0.93, 120.0), (0.93, -120.0), (0.86, 160.0), (0.86, -165.0), (0.70, 150.0), (0.62, -140.0),
                 (0.97, 70.0), (0.96, -75.0), (0.48, 168.0)][:self.shape.tufts]
        for k, (s_, a) in enumerate(spots):
            for j in range(3):
                da = (j - 1) * 9.0 + float(rng.uniform(-3, 3))
                path = [(s_ + 0.02, a + da), (s_ - 0.05, a + da * 1.3 + 4.0), (s_ - 0.11, a + da * 1.5 + 2.0)]
                st = G.surface_paint(t, path, u(1.15), u(0.1), steps=3, taper=0.85)
                b.add(st, "tuft", spine, name="tuft", outline=False, covered_by=tid)


# ── clips ─────────────────────────────────────────────────────────────────────────────────────────────────
class GnawerAnims(GoblinAnims):
    """``mclips.GoblinAnims`` with an authored contact time, a biting jaw, the knuckle-drag idle and the loincloth
    envelope sized to the gnawer's knees. ``contact_ms`` / ``attack_ms``: the attack's contact beat and length
    (``WindupEnd`` = 62 % of the contact, spec §1.8)."""

    FLAP_THIGH_R = 3.0
    contact_ms = 300.0
    attack_ms = 600.0
    windup_frac = 0.62

    def flaps(self, p, bp, **kw) -> None:
        kw.setdefault("thigh_r", self.U * self.FLAP_THIGH_R)
        super().flaps(p, bp, names=(("loin_f", 1.0),), **kw)

    death_roll = 1.0              # side the carcass rolls onto (+1: its left)
    death_spear = 128.0           # weapon angle from vertical (deg, + forward) of the dropped weapon, pre-roll
    # lying feet (u: lat, fwd, up in the unrolled body frame): the hind legs half stretched (drawn up to the belly
    # the thighs trapped the loin scrap between them and the belly: cloth × leg QA)
    death_feet = ((-5.0, 4.0, -3.0), (4.5, 6.5, -1.5))

    def u3(self, lat: float, fwd: float, up: float) -> Vector:
        return V(lat * self.U, fwd * self.U, up * self.U)

    def _with_ready(self, fn, hand_l_lift: float = 0.0, hand_r_lift: float = 0.0):
        """Run a ``GoblinAnims`` clip factory on READY with the hands lifted (u): the knuckle-drag READY rests the
        free fist on the ground, so a clip that lowers the hands from READY (walk swing, stun sag) needs it up."""
        if not (hand_l_lift or hand_r_lift):
            return fn()
        saved = self._ready
        R0 = self.ready()
        self._ready = R0.but(hand_l=R0.hand_l + V(0, 0, hand_l_lift * self.U),
                             hand_r=R0.hand_r + V(0, 0, hand_r_lift * self.U))
        try:
            return fn()
        finally:
            self._ready = saved

    walk_hand_l_lift = 0.0        # u the free hand rides above its READY place in Walk / Stun (knuckle-drag READY)
    stun_hand_l_lift = 0.0
    walk_hand_r_lift = 0.0        # u the weapon hand rides above READY in Walk (a dragged weapon clears the ground)

    def walk(self, name: str = "Walk", extra=None):
        return self._with_ready(lambda: super(GnawerAnims, self).walk(name, extra), self.walk_hand_l_lift,
                                self.walk_hand_r_lift)

    def stun(self, *a, **kw):
        return self._with_ready(lambda: super(GnawerAnims, self).stun(*a, **kw), self.stun_hand_l_lift)

    def death_keys(self) -> list:
        """Recoil → the legs fold and the chest pitches into the ground → rolled onto one side, knees drawn up, the
        jaw slack, the weapon arm flung forward (a carcass, not a sleeper; W2: the husk keeps this pose)."""
        if "death_keys" in self.overrides:
            return self.overrides["death_keys"]()
        R0 = self.ready()
        rec = self.recoil()
        rx, ry = self.web_ready["root"]
        u3 = self.u3
        rl = self.death_roll
        buckle = rec.but(pelvis=self.web_pelvis(rx + 0.5, ry + 4.0), lean=14.0, head_pitch=18.0, head_roll=8.0 * rl,
                         twist=R0.twist - 4.0, foot_r=R0.foot_r, foot_l=R0.foot_l, knee_l=V(0.6, 1, 0),
                         knee_r=V(-0.6, 1, 0), hand_r=u3(-10.5, 10.0, 8.0), elbow_r=V(-0.9, 0.2, -0.3),
                         **self._wkw(R0, 112.0, lat=0.0), hand_l=u3(8.0, 11.0, 5.5), elbow_l=V(0.9, 0.2, -0.3),
                         wrist_l=(0.0, 0.0, 0.0), clav_l=(-4.0, 0.0), clav_r=(-4.0, 0.0), flow=0.5, jaw=20.0)
        tip = buckle.but(pelvis=self.web_pelvis(rx + 1.5, ry + 7.0), lean=24.0, head_pitch=22.0, head_roll=16.0 * rl,
                         hand_r=u3(-10.0, 13.0, 5.0), **self._wkw(R0, 120.0, lat=0.2), hand_l=u3(8.5, 12.0, 4.0),
                         spin=rl * 34.0, spin_axis=V(0, 1, 0), flow=0.6, jaw=26.0)
        lying = R0.but(pelvis=self.web_pelvis(rx + 1.5, ry + 8.0), lean=26.0, head_pitch=14.0, head_roll=26.0 * rl,
                       head_yaw=-10.0 * rl, twist=-6.0, hip_yaw=0.0, hip_pitch=10.0,
                       foot_r=u3(*self.death_feet[0]), foot_l=u3(*self.death_feet[1]), foot_r_rot=(30.0, 0, 20),
                       foot_l_rot=(30.0, 0, 20), knee_l=V(0.4, 1, 0.2), knee_r=V(-0.4, 1, 0.2),
                       hand_r=u3(-9.0, 17.0, 6.0), elbow_r=V(-0.8, 0.2, -0.6), **self._wkw(R0, self.death_spear, lat=0.3),
                       hand_l=u3(6.0, 13.0, 3.0), elbow_l=V(0.6, 0.2, -0.8), wrist_l=(20.0, 0.0, 0.0),
                       clav_l=(-6.0, 0.0), clav_r=(-6.0, 0.0), spin=rl * 80.0, spin_axis=V(0, 1, 0), flow=0.05,
                       jaw=24.0)
        return [rec, buckle, tip, lying]

    def death(self, length: float = 600.0):
        rec, buckle, tip, lying = self.death_keys()
        piv = lambda bp: bp.but(pivot=self.J["pelvis"] + bp.pelvis + V(0, 0, -0.02))
        keys = [K(0.0, piv(rec)), K(0.3, piv(buckle), "out"), K(0.62, piv(tip), "in"), K(1.0, piv(lying), "out")]
        c = self.clip("Death", length, keys, length, post=self.grounded(snap=True))
        c.key_poses_ms = [0.0, round(0.3 * length, 1), round(0.62 * length, 1), length]
        return c

    def attack(self, name: str = "Attack01", length: float | None = None, notifies: dict | None = None,
               recover: BP | None = None, contact: float | None = None, keys_fn=None):
        """READY → wind (⅓ of the contact, ease-out) → mid (⅔, ease-in) → HIT = Contact → follow-through → back to
        READY by ``length`` (the goblin attack's key spacing, stretched to ``contact``)."""
        cms = contact or self.contact_ms
        length = length or self.attack_ms
        R0 = self.ready()
        wind, mid, hit = (keys_fn or self.attack_keys)()
        follow = self.overrides.get("follow", lambda h: h)(hit)
        back = self.overrides.get("recover", lambda: None)()
        f_at = min(cms + 0.28 * (length - cms), cms * 1.35)
        keys = [K(0.0, R0), K(1 / 3, wind, "out"), K(2 / 3, mid, "in"), K(1.0, hit, "linear"),
                K(f_at / cms, follow, "out")]
        if back is not None:
            keys.append(K((f_at + 0.45 * (length - f_at)) / cms, back, "smooth"))
        keys.append(K(length / cms, recover or R0, "smooth"))
        nt = {"WindupEnd": round(self.windup_frac * cms, 1), "Contact": cms}
        nt.update(notifies or {})
        c = self.clip(name, length, keys, cms, notifies=nt)
        c.key_poses_ms = sorted({0.0, round(cms / 3, 1), round(2 * cms / 3, 1), cms, round(f_at, 1), length})
        return c


# ── manifest extras ───────────────────────────────────────────────────────────────────────────────────────
def marker_extra(names) -> dict:
    return {"marker": {"swatches": list(names), "family": "the Gravegnawers",
                       "kind": "bone-yellow eyeshine #EDE6B0 (emissive .35; beasts, not breach-touched: monsters.md "
                               "1.2)"}}


def remake_extra(role: str, contact_ms: float, ref_speed: float, notes: str) -> dict:
    return {"refSpeedCmS": round(ref_speed * 100.0, 1), "attackStyle": role,
            "remake": {"family": "gnawers", "palette": FAMILY, "voiceFamily": "Gnawer", "contactMs": contact_ms,
                       "spec": "Docs/dark-remake/monsters.md 2.4"},
            "notes": notes}

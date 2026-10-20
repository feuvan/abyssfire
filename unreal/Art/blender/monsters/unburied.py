"""不葬军 The Unburied — the human monster kit on ``SKEL_Human`` (dark remake, monsters.md §2.2; package A2 of
Docs/dark-remake/implementation-plan.md §5).

The crusade's dead in ash mode: skin like burnt clay, ash-grey and finely cracked (``unburied.skin`` #8C8F86, painted
shadow #4C4F4A, crack strokes #2A2C29), no open wounds and no exposed bone; a torn crimson tabard (#6B1F1F, tarnished
trim #B49A5E, the church's open-palm sigil scorched, charred hem) over a quilted gambeson (#6E6758) with mail shreds
(#5A5D63) on the upper arms and a mail skirt; broken arrow shafts in the back; ash dusting on the shoulders; **violet
pinpoint eyes** (#D08CFF, the family marker) in dark sockets. Every colour is a ``Monsters_March`` swatch registered by
``march/march_palettes.py`` (``MBuilder.use`` registers the exact registry values, so the kit never adds a swatch).

:class:`UnburiedKit` builds the rig (``HumanoidSpec`` on ``SKEL_Human``, realistic proportions with a moderately enlarged
head — monsters.md §5 "proportions away from cute": smaller heads, longer limbs; 1.76 m to the bare crown, R2) + the
shared body. The rest pose is upright (clean bind pose); the **hunch** (lean 12°, the broken-neck head tilt 15°, one
shoulder dropped) is authored in the READY pose (``uclips.py``). Members add their gear through :class:`ULook` flags
(quiver, pavise, coif, hip sword) and their own items:

* head looks per instance (monsters.md: "kettle helm / bare scalp / hood") are **attachments on the ``head`` bone**
  (``SM_Mon_Unburied_Att_KettleHelm``, ``SM_Mon_Unburied_Att_Hood``; the base mesh is the bare cracked scalp), so the
  runtime can pick one per instance (manifest ``looks``); the default ``attachments`` wear the kettle helm;
* held items ``SM_MonWeapon_*`` (the notched arming sword and the burial detail's shovel here; the crossbow, tower
  shield, mace and standard pole in the members' scripts) in their weapon bone's space (grip at the origin, +Y along
  the blade / shaft, +Z up — the kit's weapon-socket convention).

Chains: ``tabard_f_01..02`` / ``tabard_b_01..02`` (the tabard skirt panels below the belt, envelope-solved against
the legs in ``uclips``) + whatever a member adds (the standard-bearer's ``banner_*`` on ``weapon_r``).
"""
from __future__ import annotations

import math
import sys
from dataclasses import dataclass, field, replace
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "march", HERE.parent / "items", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import mesh as M, outline, rig as R, scene, shading  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402
from kit.rig import Bind, HumanoidSpec  # noqa: E402

import common as C  # noqa: E402
from common import Profile  # noqa: E402
import goblins as GB  # noqa: E402
import hero_base as HB  # noqa: E402
import mgeo as G  # noqa: E402
from mgeo import lerp, smooth01  # noqa: E402

FAMILY = "Monsters_March"
SKELETON = "SKEL_Human"
PREFIX = "unburied"
OUTLINE_CLASS = "monster"
OUTLINE_PX = 3.5
OUTLINE_PX_NOTE = ("per-asset request (as the goblin family and every Chapter-1 character): the Unburied are thin "
                   "and dark on the march ground — arrow shafts, sword / pole, fingers and the torn tabard hem kept "
                   "< 3 px at the monster class's 3.0 px; 3.5 px holds >= 3 px on >= 90 % of every gated cell (gate "
                   "3.0 px, the Chapter-1 character gate and the darkest-tile rim rule); held items and head "
                   "attachments ink with it")
HEIGHT = 1.76                    # bare crown (R2: the hero is never shorter than a townsperson; warrior 1.76 m)
HELM = "SM_Mon_Unburied_Att_KettleHelm"
HOOD = "SM_Mon_Unburied_Att_Hood"
SWORD = "SM_MonWeapon_Sword"
SHOVEL = "SM_MonWeapon_Shovel"
MARKER = ("unburied.eye",)       # family marker swatches (manifest ``marker``; darkgate ≥ 2 px)
EYE_GLOW = "#D08CFF"             # = breach.pinpoint (the runtime eye cards; never ember)
MEMBERS = ("SK_Mon_RisenFootman", "SK_Mon_RisenArbalest", "SK_Mon_RisenShieldbearer", "SK_Mon_DeadStandardBearer")

# ── crimson cloth (art review r2): the kit's cream Fresnel rim (#FFECC8 at .55) stacked on the cloth preset's light
# band turned the dark crimson tabards, the banner, the pavise and the tower shield a pale salmon / peach wherever a
# flat panel was seen at a grazing angle (README §25 records the same defect on Valdras). Those faces take a second
# toon slot whose instance overrides the rim with a deep crimson (the MI_AF_Toon_Valdras_Cloth parameters).
CLOTH_MATERIAL = "MI_AF_Toon_Unburied_Cloth"
CLOTH_RIM_HEX, CLOTH_RIM_ALPHA = "#8E3A34", 0.38     # = MI_AF_Toon_Valdras_Cloth (sRGB, README §23 importer note)
CLOTH_NOTE = (f"slot 2 {CLOTH_MATERIAL}: the palette toon (M_AF_Toon) with RimOverride 1, RimOverrideColor "
              f"{CLOTH_RIM_HEX}, RimOverrideAlpha {CLOTH_RIM_ALPHA} on the crimson cloth (tabard + its paint; the "
              "banner; the pavise) — the cream rim lit the dark crimson pale salmon at grazing angles (the same "
              "parameters as MI_AF_Toon_Valdras_Cloth); RimOverrideColor is sRGB (README §23's importer note)")
# swatches appended to the Monsters_March registry by this family (append-only: registered once, then shared)
APPENDED = {
    # the banner's crimson with a narrower light band: the cloth preset (.30) lit the banner a dusty salmon
    # (#995B54 before the rim); .25 keeps it brick crimson (#924A4A, L* ≈ 41, +19 over the darkest march mud)
    "standard.banner": ("#6E1A1C", 0.52, 0.25, 0.0),
    # the hood look's wool: darker than the legs' shared ``unburied.hood`` (#4A4642, the dark preset's .36 band lit
    # the cowl's crown L* 55, paler than the face it frames); #3A3633 at .22 reads as a dark cowl (base L* 23, lit
    # L* 40: +18 over the darkest march mud, the A1 lit-band rule 15) with the grey face and violet eyes inside
    "unburied.cowl": ("#3A3633", 0.52, 0.22, 0.0),
    # the pavise's and the tower shield's crimson boards with the banner's narrower band: a flat board turned up to
    # the key light (the pavise on the folded corpse, the shield laid face-up) lit to L* 46-49 with the cloth .30;
    # .22 keeps them brick (#7E524A L* 40 / #97524A L* 44: +18 / +22 over the darkest march mud)
    "arbalest.pavise_field": ("#5A2422", 0.52, 0.22, 0.0),
    "towershield.face": ("#7A2422", 0.52, 0.22, 0.0),
    # art review r3: the tabard (chest panels + the skirt panels on tabard_f/b) with the boards' narrower band. The
    # rim override (r2) cannot reach it: the shared ``unburied.tabard`` (#6B1F1F, cloth .30) lights to #975F56 itself
    # (L* 46, a* 22: a dusty salmon) — the skirt panels turned up to the key light read pale on 20-35 % of the crimson
    # in the sw facings. .22 → #8C4E47 (L* 40, a* 25: brick, beside the banner's #92504A; +21 over the darkest march
    # mud). The Unburied only (region ``tabard`` → this swatch in UnburiedKit.regions); ``unburied.tabard`` stays
    # registered for the drum hoop and the Living's scarf lines
    "unburied.tabard_cloth": ("#6B1F1F", 0.52, 0.22, 0.0),
}

# ── review ground: the march ash grass (march_palettes.MARCH_GROUND "0", L* 31) instead of the plains green ──────────
MARCH_GROUND_HEX = "#4C4940"


def march_stage(ground_hex: str = MARCH_GROUND_HEX) -> None:
    """Every review stage of this process on the march ground (``kit.review.stage`` default colour): the Unburied
    are judged where they stand in game. The darkest-tile gate (darkgate.py) measures the darkest march tile."""
    from kit import review
    if getattr(review.stage, "_march", False):
        return
    orig = review.stage

    def stage(radius: float = 30.0, blob_radius: float = 0.39, ground_hex_: str = ground_hex, grid: bool = True,
              **kw):
        return orig(radius=radius, blob_radius=blob_radius, ground_hex=kw.get("ground_hex", ground_hex_), grid=grid)
    stage._march = True
    review.stage = stage


def dev_review(asset: str, rig, meshes, A, out: Path, height: float, pose=None, glows=None, blob: float = 0.30,
               px: float = OUTLINE_PX) -> None:
    """Mesh iteration (``--quick``): READY close-ups se / front / back, turnaround and the 1080p game crop on the
    march ground, only ``meshes`` visible (attachments already parented)."""
    import mship
    from kit import review
    march_stage()
    keep = set(meshes)
    for o in bpy.data.objects:
        if o.type == "MESH" and o not in keep and o.name != "AF_Stage":
            o.hide_render = True
    out.mkdir(parents=True, exist_ok=True)
    mship.apply_bp(A, pose if pose is not None else A.ready())
    rv = review.Review(asset, rig.obj, list(meshes), height, blob_radius=blob, out_dir=out, game_outline_px=px,
                       outline_class=OUTLINE_CLASS, glows=glows)
    rv.gate = mship.gate_for(OUTLINE_CLASS, px)
    rv.ground_occludes = True
    for fac, pitch, nm in (("se", None, "se"), ("front", -30.0, "front"), ("back", -30.0, "back")):
        if pitch is None:
            rv.closeup(fac)
        else:
            rv.closeup(fac, pitch=pitch)
        (out / "closeup.png").rename(out / f"closeup_{nm}.png")
    rv.turnaround()
    rv.game_view()
    for p, s_ in rv.written:
        print(f"  {p.name} {s_ // 1024} KB")


# ── palette (exact Monsters_March registry values) ─────────────────────────────────────────────────────────────
_REGS: dict | None = None


def march_regions() -> dict:
    """{swatch name: Region} of every ``Monsters_March`` swatch (``march_palettes.family_regions``): parts declared
    through :meth:`MBuilder.use` register these exact values, so they land on the A1 swatches (no new index)."""
    global _REGS
    if _REGS is None:
        import march_palettes as MP
        _REGS = {n: r for n, r, _ in MP.family_regions(FAMILY)}
    return _REGS


class MBuilder(GB.LodHullBuilder):
    """``kit.mesh.Builder`` (LOD1: :class:`goblins.LodHullBuilder`) whose regions are ``Monsters_March`` swatch
    names: ``use("unburied.skin", "crossbow.iron")`` → region keys ``skin`` (the builder's own prefix is dropped)
    and ``crossbow.iron`` (other prefixes keep the full name), registered with the registry's exact values."""

    def use(self, *names: str) -> None:
        regs = march_regions()
        for full in names:
            if full not in regs:
                raise KeyError(f"{full} is not a Monsters_March swatch (march/march_palettes.py)")
            key = full.split(".", 1)[1] if full.startswith(self.prefix + ".") else full
            self.swatch[key] = self.palette.add(full, regs[full])

    def alias(self, **keys: str) -> None:
        """Region keys of a shared generator (``items/weapons.build_mace``: haft, head, …) → swatch names."""
        regs = march_regions()
        for key, full in keys.items():
            self.swatch[key] = self.palette.add(full, regs[full])

    def use_appended(self, *names: str) -> None:
        """Swatches this family appends to the registry (:data:`APPENDED`); region key = the full name."""
        import march_palettes as MP
        for full in names:
            hx, s_, l_, e_ = APPENDED[full]
            self.swatch[full] = self.palette.add(full, Region(hx, s_, l_, e_, MP.O_MON))


def cloth_material(pal: Palette) -> bpy.types.Material:
    """The crimson-cloth toon slot: the palette toon with ``RimOverride`` 1, ``RimOverrideColor`` #8E3A34,
    ``RimOverrideAlpha`` .38 (parent ``M_AF_Toon``; the light / shade bands are the palette's)."""
    import json
    m = bpy.data.materials.get(CLOTH_MATERIAL)
    if m is not None:
        return m
    m = shading.toon_material(pal).copy()
    m.name = CLOTH_MATERIAL
    grp = next(n for n in m.node_tree.nodes if n.type == "GROUP")
    grp.node_tree = shading.rim_override_group(CLOTH_RIM_HEX, CLOTH_RIM_ALPHA)
    m["af_parent"] = "M_AF_Toon"
    m["af_params"] = json.dumps({"RimOverride": 1.0, "RimOverrideColor": CLOTH_RIM_HEX,
                                 "RimOverrideAlpha": CLOTH_RIM_ALPHA})
    m["af_toon_slot"] = True
    return m


def cloth_slot(obj, pal: Palette, part_ids) -> int:
    """Move the toon faces (slot 0) of parts ``part_ids`` (``af_part``) to slot 2 (:func:`cloth_material`); the
    baked hull stays on slot 1. Whole parts only: a part split between slots 0 and 2 shares vertices across the
    seam, and the kit's preview outline (``outline`` Set Position on material index == 1, interpolated to the
    points: 0 / 2 averages to 1) pushes that seam out over the ink. Returns the faces moved (0: no slot added)."""
    me = obj.data
    fp = np.zeros(len(me.polygons), np.int32)
    me.attributes["af_part"].data.foreach_get("value", fp)
    mi = np.zeros(len(me.polygons), np.int32)
    me.polygons.foreach_get("material_index", mi)
    sel = np.isin(fp, np.array(sorted(part_ids), np.int32)) & (mi == 0)
    if not sel.any():
        return 0
    assert len(me.materials) == 2, f"{obj.name}: cloth slot after toon + outline only"
    me.materials.append(cloth_material(pal))
    me.polygons.foreach_set("material_index", np.where(sel, 2, mi).astype(np.int32))
    me.update()
    return int(sel.sum())


def part_ids(b, names) -> set:
    """Builder part ids whose (LOD-base) name is in ``names``."""
    return {i for i, p in enumerate(b.parts) if GB.lod_base(p["name"]) in set(names)}


def item_slot_extra(obj, pal: Palette, cls: str = "monster") -> dict:
    """Manifest fields of a static mesh with the cloth slot (``export.static_entry`` lists toon + outline only and
    counts slot 0): ``materialSlots`` (toon, outline, cloth) + ``triangles`` (toon = every toon-lit slot, as
    ``mship.tri_record``). Passed in ``Item.extra`` (it overrides the generated fields)."""
    import json
    from kit import mesh as M_
    width = float(obj.get("af_outline_width", 0.0))
    slots = [{"index": 0, "name": pal.material_name, "parent": "M_AF_Toon", "palette": pal.family},
             {"index": 1, "name": "M_AF_Outline", "parent": "M_AF_Outline", "palette": pal.family, "class": cls,
              "widthCm": round(width * 100.0, 2), "outlinePx1080": outline.SCREEN_PX_1080.get(cls, 0.0)}]
    mats = obj.data.materials
    for i in range(2, len(mats)):
        e = {"index": i, "name": mats[i].name, "parent": mats[i].get("af_parent", "M_AF_Toon"), "palette": pal.family}
        if "af_params" in mats[i]:
            e["params"] = json.loads(mats[i]["af_params"])
        slots.append(e)
    tris = {"toon": sum(M_.tri_count(obj, i) for i in range(len(mats)) if i != 1), "outline": M_.tri_count(obj, 1)}
    if len(mats) > 2:
        tris["bySlot"] = {m.name: M_.tri_count(obj, i) for i, m in enumerate(mats)}
    return {"materialSlots": slots, "triangles": tris}


LOD1_RATIO = 0.42
LOD1_SKIP = ("crack", "mail_row", "quilt", "fletch", "ash", "sigil_finger", "stitch", "nostril", "rivet", "dent",
             "lace", "cheek", "ear", "buckle_pin", "wrap", "notch", "brow", "knuckle", "buckle")


def builder(asset: str, pal: Palette, prefix: str, lod: int = 0, skip: tuple = (), ratio: float = LOD1_RATIO,
            hull_ratio: float = 1.0) -> MBuilder:
    if not lod:
        return MBuilder(asset, pal, prefix=prefix)
    names = set(LOD1_SKIP) | set(skip)
    return MBuilder(asset, pal, prefix=prefix, lod_ratio=ratio, lod_skip=lambda n: GB.lod_base(n) in names,
                    hull_ratio=hull_ratio)


BODY_SWATCHES = ("unburied.skin", "unburied.skin_shadow", "unburied.skin_line", "unburied.ash", "unburied.eye",
                 "unburied.socket", "unburied.mouth", "unburied.tabard", "unburied.tabard_char", "unburied.trim",
                 "unburied.sigil", "unburied.mail", "unburied.mail_line", "unburied.gambeson",
                 "unburied.gambeson_line", "unburied.hood", "unburied.leather", "unburied.leather_dark",
                 "unburied.wood", "unburied.fletch", "unburied.helm", "unburied.helm_dent", "unburied.steel",
                 "unburied.steel_edge", "unburied.rope")


# ── look ───────────────────────────────────────────────────────────────────────────────────────────────────────
@dataclass
class ULook:
    """One member's body options (the kit's shared body + member gear flags)."""
    arrows: tuple = ((0.70, 158.0, 18.0, 0.30, True), (0.52, 204.0, -14.0, 0.24, False),
                     (0.80, 196.0, 6.0, 0.19, True))   # (torso h, angle, yaw, length m, fletched) broken shafts
    tabard_len: float = 0.40     # skirt panel below the belt (m)
    coif: bool = False           # mail coif over the head and shoulders (shieldbearer)
    quiver: bool = False         # bolt quiver at the right hip (arbalester)
    pavise: bool = False         # pavise slung on the back (arbalester)
    hip_sword: bool = False      # sheathed sword at the left hip (standard-bearer)
    hand_r: str = "fist"         # fist (holds weapon_r) | open
    hand_l: str = "open"         # open (the shambler's reaching hand) | fist (holds weapon_l)
    mail_sleeves: bool = True
    sash: bool = False           # (pitwarden, W3)
    eye_scale: float = 1.0       # socket + violet pinpoint size (the coif's face window hides part of them)
    extra: dict = field(default_factory=dict)       # "swatches": member regions · "lod1_skip": extra LOD1-only drops


# ── the kit ────────────────────────────────────────────────────────────────────────────────────────────────────
TORSO_RINGS = [   # (h along pelvis → neck base, rx side, ry front/back, cy (− forward)): a padded gambeson torso
    (-0.17, 0.148, 0.110, 0.000), (0.00, 0.150, 0.108, 0.000), (0.22, 0.146, 0.106, -0.004),
    (0.46, 0.154, 0.112, -0.008), (0.66, 0.166, 0.118, -0.006), (0.84, 0.170, 0.114, 0.004),
    (0.95, 0.138, 0.096, 0.010), (1.02, 0.084, 0.072, 0.012), (1.07, 0.058, 0.056, 0.012)]
HEAD_RINGS = [    # (z above the head centre, rx, ry, cy): a gaunt human head, ~15 % over life size for the camera
    (0.128, 0.012, 0.014, 0.004), (0.118, 0.046, 0.058, 0.004), (0.098, 0.070, 0.084, 0.004),
    (0.066, 0.084, 0.098, 0.004), (0.034, 0.088, 0.102, 0.002), (0.004, 0.086, 0.100, -0.002),
    (-0.026, 0.081, 0.094, -0.006), (-0.054, 0.069, 0.086, -0.012), (-0.082, 0.060, 0.076, -0.020),
    (-0.104, 0.044, 0.058, -0.032), (-0.118, 0.026, 0.036, -0.044), (-0.126, 0.010, 0.016, -0.048)]
EYE_Z, EYE_A = -0.004, 30.0      # eye centre (head z, angle from the front)
TABARD_LIFT = 0.007              # the tabard's outer face over the gambeson
TABARD_W = 52.0                  # half angle of the front / back panels on the chest


class UnburiedKit:
    """Rig + shared body of one Unburied (or Living: the same kit with living swatches) mesh."""

    def __init__(self, asset: str, look: ULook | None = None, height: float = HEIGHT, prefix: str = PREFIX,
                 extra_chains=None):
        self.asset, self.prefix = asset, prefix
        self.look = look or ULook()
        base = HumanoidSpec(name=asset, skeleton=SKELETON, thigh=0.43, shin=0.42, ankle=0.072, torso=0.48,
                            neck=0.165, head=0.27, hip_half=0.092, shoulder_half=0.172, upper_arm=0.30,
                            fore_arm=0.275, hand=0.10, foot_len=0.27, arm_down=58.0, elbow_prebend=10.0,
                            knee_prebend=3.0, face=True)
        self.spec = base.fit_height(height)
        s = self.spec
        self.hip_z = s.ankle + s.shin + s.thigh
        self.pz = self.hip_z + s.pelvis_offset
        self.nz = self.pz + s.torso
        self.hc = self.nz + s.neck
        self.torso = Profile([(self.th(h), rx, ry, cy) for h, rx, ry, cy in TORSO_RINGS], exp=2.3)
        self.head = Profile([(self.hc + z, rx, ry, cy) for z, rx, ry, cy in HEAD_RINGS], exp=2.1)
        chains = self.chains() + list(extra_chains(self) if extra_chains else [])
        self.spec = replace(self.spec, chains=chains)
        self.rig = R.build_humanoid(self.spec)
        self.J = self.rig.joints
        self._place_face_bones()
        self.U = 0.0316          # rig unit for GoblinAnims-style offsets (≈ the NPC unit at this height)
        self.w = SimpleWeb(self.spec)

    # ── units ──────────────────────────────────────────────────────────────────────────────────────────
    def th(self, h: float) -> float:
        """Torso fraction (0 pelvis joint … 1 neck base) → z."""
        return self.pz + h * (self.nz - self.pz)

    def tp(self, h: float, a: float, out: float = 0.0) -> Vector:
        return self.torso.point(self.th(h), a, out)

    def tn(self, h: float, a: float) -> Vector:
        return self.torso.normal(self.th(h), a)

    def hp(self, z: float, a: float, out: float = 0.0) -> Vector:
        """Head surface at ``z`` above the head centre."""
        return self.head.point(self.hc + z, a, out)

    def hn(self, z: float, a: float) -> Vector:
        return self.head.normal(self.hc + z, a)

    # ── rig ────────────────────────────────────────────────────────────────────────────────────────────
    def tabard_top(self, sg: float) -> Vector:
        """Top centre of a tabard skirt panel (front sg = 1, back −1), just under the belt."""
        z = self.th(-0.02)
        rx, ry, cy, cx = self.torso.at(z)
        return Vector((0.0, cy - sg * (ry + TABARD_LIFT + 0.012), z))

    def chains(self) -> list:
        out = []
        L = self.look.tabard_len
        for nm, sg in (("tabard_f", 1.0), ("tabard_b", -1.0)):
            t = self.tabard_top(sg)
            out.append((nm, "pelvis", [t, t + Vector((0, 0, -0.48 * L)), t + Vector((0, 0, -L))]))
        return out

    def _place_face_bones(self) -> None:
        arm = self.rig.obj
        scene.select_only([arm])
        bpy.ops.object.mode_set(mode="EDIT")
        eb = arm.data.edit_bones
        for s, sg in (("l", 1), ("r", -1)):
            e = self.eye_center(sg)
            b = eb[f"eye_{s}"]
            b.head, b.tail = e, e + Vector((0, -0.03, 0))
            b.align_roll(Vector((0, 0, 1)))
            self.J[f"eye_{s}"] = e
        jb = eb["jaw"]
        jb.head = Vector((0, 0.01, self.hc - 0.035))
        jb.tail = self.hp(-0.112, 0.0)
        jb.align_roll(Vector((1, 0, 0)).cross((jb.tail - jb.head).normalized()))
        bpy.ops.object.mode_set(mode="OBJECT")
        for so in self.rig.sockets:
            if so["name"] in ("fx_eye_l", "fx_eye_r"):
                so["pos"] = self.J["eye_" + so["name"][-1]]

    def eye_center(self, sg: int) -> Vector:
        return self.hp(EYE_Z, sg * EYE_A, -0.004)

    # ── build ──────────────────────────────────────────────────────────────────────────────────────────
    def regions(self, b: MBuilder) -> None:
        b.use(*BODY_SWATCHES)
        b.use_appended("unburied.tabard_cloth")
        b.swatch["tabard"] = b.swatch["unburied.tabard_cloth"]     # the tabard's narrower light band (APPENDED, r3)
        lk = self.look
        if lk.quiver:
            b.use("arbalest.quiver", "crossbow.bolt")
        if lk.pavise:
            b.use("arbalest.pavise", "arbalest.pavise_rim", "towershield.ash", "towershield.sigil")
            b.use_appended("arbalest.pavise_field")
        for name in lk.extra.get("swatches", ()):
            b.use(name)

    def build(self, pal: Palette, lod: int = 0, ratio: float = LOD1_RATIO, extra=None, name: str | None = None,
              hull_ratio: float = 1.0):
        """The merged skeletal mesh (LOD0, or LOD1 at ``ratio``); ``extra(kit, b)`` adds the member's own parts."""
        import json
        from kit import asset
        self.lod = lod
        b = builder(self.asset, pal, self.prefix, lod, skip=tuple(self.look.extra.get("lod1_skip", ())), ratio=ratio,
                    hull_ratio=hull_ratio)
        self.regions(b)
        self.b = b
        self.cloth_ids = set()           # parts on the crimson-cloth slot (tabard + its paint, pavise; members add)
        ids = {}
        ids["skull"] = self.add_head(b)
        self.add_neck(b)
        ids["torso"] = self.add_torso(b)
        n0 = len(b.parts)
        self.add_tabard(b, ids)
        self.cloth_ids.update(range(n0, len(b.parts)))
        self.add_belt(b, ids)
        self.add_mail_skirt(b)
        for side in ("l", "r"):
            self.add_arm(b, side)
            self.add_leg(b, side)
        self.add_arrows(b, ids)
        if self.look.coif:
            self.add_coif(b, ids)
        if self.look.quiver:
            self.add_quiver(b)
        if self.look.pavise:
            self.add_pavise(b)
        if self.look.hip_sword:
            self.add_hip_sword(b)
        if extra is not None:
            extra(self, b)
        names = [p["name"] for p in b.parts]
        body = asset.finish_mesh(b, pal, OUTLINE_CLASS, rig=self.rig, grounding_height=HEIGHT,
                                 name=name or (f"{self.asset}_LOD{lod}" if lod else f"{self.asset}_Mesh"))
        body["af_parts"] = json.dumps(names)
        body["af_lod"] = lod
        cloth_slot(body, pal, self.cloth_ids)
        return body

    # ── head: gaunt ash skull-cap (bare scalp), dark sockets with violet pinpoints, slack mouth, cracks ──────
    def head_cut(self, z: float, a: float) -> bool:
        """Skin under the coif (dropped from the toon mesh; the hull is baked from the closed proxy): everything
        outside the coif's face window plus a 1.5-2 cm margin. ``z`` above the head centre, ``a`` degrees from the
        front (+ = the left)."""
        if not self.look.coif:
            return False
        return z > 0.068 or z < -0.122 or abs(a) > 56.0

    def head_za(self, co) -> tuple[float, float]:
        cy = self.head.at(self.hc)[2]
        return co[2] - self.hc, math.degrees(math.atan2(co[0], -(co[1] - cy)))

    def add_head(self, b: MBuilder) -> int:
        hp = self.head
        zs = [self.hc + z for z in np.linspace(-0.124, 0.126, 11)]
        extra = [-44.0, -30.0, -16.0, 16.0, 30.0, 44.0]
        skull = HB.profile_mesh(hp, zs, 14, cap0="fan", cap1="fan", dome0=0.004, dome1=0.006, extra_angles=extra)
        proxy = HB.profile_mesh(hp, [self.hc + z for z in np.linspace(-0.124, 0.126, 9)], 14, cap0="fan",
                                cap1="fan", dome0=0.004, dome1=0.006)
        bind = Bind.rigid("head")
        if self.look.coif:
            bm = skull.bm
            gone = [f for f in bm.faces if all(self.head_cut(*self.head_za(v.co)) for v in f.verts)]
            bmesh.ops.delete(bm, geom=gone, context="FACES")
            bm.normal_update()
        sid = b.add(skull, "skin", bind, name="skull", hull=proxy, hull_pad=True)
        self.add_face(b, sid)
        return sid

    def add_face(self, b: MBuilder, sid: int) -> None:
        bind = Bind.rigid("head")
        lod = getattr(self, "lod", 0)
        for sg in (1.0, -1.0):
            a = sg * EYE_A
            p = self.hp(EYE_Z, a)
            n = self.hn(EYE_Z, a)
            fr = R.frame(p, n, (0, 0, 1))                     # +Y out of the face, +Z up
            # the socket: a dark pit sunk into the face (reads as a hollow at the W1 camera)
            k = self.look.eye_scale
            pit = HB.ell((0.028 * k, 0.010, 0.0215 * k), (sg * -0.002, -0.0035, -0.001), sides=10, rings=4)
            GB_add_exact(b, pit.transform(fr), "socket", bind, name="socket", outline=False, covered_by=sid)
            # the pinpoint: a violet glowing bead in the pit (family marker; never decimated)
            pin = HB.ell((0.0165 * k, 0.0072, 0.0148 * k), (sg * -0.001, 0.0054, 0.0000), sides=8, rings=4)
            GB_add_exact(b, pin.transform(fr), "eye", bind, name="eye", outline=False, covered_by=sid)
            # heavy brow ridge over the pit (the socket's shadow edge)
            pts = [self.hp(0.030 + 0.006 * math.sin(math.pi * t), sg * lerp(8.0, 50.0, t), 0.004)
                   for t in np.linspace(0, 1, 6)]
            brow = M.sweep(pts, M.superellipse(1, 1, 6, 2.2), scales=[(0.008, 0.010)] * len(pts),
                           up=tuple(self.hn(0.03, sg * 25.0)), dome0=0.004, dome1=0.004)
            b.add(brow, "skin", bind, name="brow", outline=False, covered_by=sid)
            # cheek hollow (painted shadow under the cheekbone)
            b.add(G.surface_patch(_HeadSurf(self), -0.064, -0.036, sg * 30.0, sg * 58.0, 0.0012, nu=2, na=3,
                                  shape=lambda tu, ta: 0.55 + 0.45 * math.sin(math.pi * tu)),
                  "skin_shadow", bind, name="cheek", outline=False, covered_by=sid)
            # ear (small, flat to the skull)
            fe = R.frame(self.hp(-0.004, sg * 88.0, -0.004), self.hn(-0.004, sg * 88.0), (0, 0, 1))
            ear = HB.ell((0.026, 0.010, 0.032), (0, 0.004, 0), sides=6, rings=4)
            if not self.head_cut(-0.004, 88.0):
                b.add(ear.transform(fe), "skin", bind, name="ear", outline=False, covered_by=sid)
        # nose: worn, flattened wedge
        path = [self.hp(-0.004, 0.0, -0.004), self.hp(-0.024, 0.0, 0.010), self.hp(-0.040, 0.0, 0.014),
                self.hp(-0.048, 0.0, 0.006)]
        nose = M.tube(path, [0.009, 0.011, 0.013, 0.009], sides=8, up=tuple(self.hn(-0.03, 0.0).cross(Vector((1, 0, 0)))),
                      dome0=0.0, dome1=0.006)
        b.add(nose, "skin", bind, name="nose", outline=False, covered_by=sid)
        # slack mouth: a dark slit, a little open (no teeth, no bone)
        mfr = R.frame(self.hp(-0.078, 0.0), self.hn(-0.078, 0.0), (0, 0, 1))
        mouth = HB.ell((0.024, 0.006, 0.0085), (0, -0.002, 0), sides=10, rings=4)
        GB_add_exact(b, mouth.transform(mfr), "mouth", bind, name="mouth", outline=False, covered_by=sid)
        surf = _HeadSurf(self)
        # chin shadow under the mouth, temple hollows
        b.add(G.surface_patch(surf, -0.112, -0.094, -18.0, 18.0, 0.0010, nu=2, na=4,
                              shape=lambda tu, ta: 0.4 + 0.6 * math.sin(math.pi * tu)), "skin_shadow", bind,
              name="cheek", outline=False, covered_by=sid)
        # cracks: fine dark strokes over the scalp, brow and cheek (painted, single-sided)
        rng = scene.rng(self.asset, "cracks")
        cracks = [[(0.118, -20.0), (0.100, -6.0), (0.080, 4.0), (0.060, 0.0)],
                  [(0.112, 40.0), (0.092, 52.0), (0.070, 48.0), (0.052, 60.0)],
                  [(0.105, 150.0), (0.085, 162.0), (0.060, 150.0)],
                  [(0.100, -120.0), (0.078, -108.0), (0.050, -118.0), (0.030, -104.0)],
                  [(-0.020, -52.0), (-0.036, -42.0), (-0.050, -48.0)],
                  [(0.058, -28.0), (0.046, -36.0), (0.040, -24.0)],
                  [(0.010, 62.0), (-0.016, 70.0), (-0.040, 64.0)]]
        for path in cracks:
            jit = [(z + rng.uniform(-0.002, 0.002), a + rng.uniform(-2.0, 2.0)) for z, a in path]
            if all(self.head_cut(z, a) for z, a in path):
                continue
            st = G.surface_paint(surf, jit, 0.0034, 0.0011, steps=5, taper=0.7)
            b.add(st, "skin_line", bind, name="crack", outline=False, covered_by=sid)
        # ash dusting on the crown
        if not self.head_cut(0.10, 160.0):
            b.add(G.surface_patch(surf, 0.090, 0.124, 100.0, 220.0, 0.0012, nu=2, na=5,
                                  shape=lambda tu, ta: 0.6 + 0.4 * math.sin(math.pi * ta)), "ash", bind, name="ash",
                  outline=False, covered_by=sid)

    def add_neck(self, b: MBuilder) -> None:
        n0 = Vector((0, 0.012, self.nz - 0.04))
        n1 = Vector((0, 0.010, self.hc - 0.075))
        neck = M.tube([n0, n0.lerp(n1, 0.5), n1], [0.048, 0.043, 0.046], sides=12)
        b.add(neck, "skin", Bind.blend("spine_03", "neck_01", "head", falloff=3.0), name="neck")
        # tendons / throat shadow
        b.add(M.tube([n0 + Vector((0, -0.04, 0.03)), n1 + Vector((0, -0.036, 0.005))], [0.006, 0.006], sides=5),
              "skin_shadow", Bind.blend("spine_03", "neck_01", "head", falloff=3.0), name="cheek", outline=False)

    # ── torso: quilted gambeson ─────────────────────────────────────────────────────────────────────────
    def spine_bind(self) -> Bind:
        return Bind.blend("pelvis", "spine_01", "spine_02", "spine_03", smooth=0.03)

    def add_torso(self, b: MBuilder) -> int:
        tp = self.torso
        shell = HB.profile_mesh(tp, list(np.linspace(tp.z0, tp.z1, 12)), 20, cap0="fan", cap1="fan", dome1=0.006)
        hull = HB.profile_mesh(tp, list(np.linspace(tp.z0, tp.z1, 9)), 16, cap0="fan", cap1="fan", dome1=0.006)
        sb = self.spine_bind()
        tid = b.add(shell, "gambeson", sb, name="torso", hull=hull, hull_pad=True)
        # quilting under the arms (the only gambeson the tabard leaves bare on the body)
        for sg in (1.0, -1.0):
            for da in (-9.0, 9.0):
                b.add(HB.painted_strip(tp, sg * (90.0 + da), self.th(0.10), self.th(0.80), 0.004,
                                       lambda z, a: 0.0014, steps=6), "gambeson_line", sb, name="quilt",
                      outline=False, covered_by=tid)
        return tid

    # ── tabard: torn crusade tabard, scorched sigil, charred hem ─────────────────────────────────────────
    def tabard_w(self, h: float) -> float:
        """Half angle of the front / back panel at torso height ``h`` (they meet over the shoulders)."""
        return TABARD_W + 38.0 * smooth01((h - 0.86) / 0.12)

    def add_tabard(self, b: MBuilder, ids: dict) -> None:
        tp = self.torso
        sb = self.spine_bind()
        hs = list(np.linspace(0.06, 1.02, 9))
        for c, nm in ((0.0, "tabard_front"), (180.0, "tabard_back")):
            outer, inner = [], []
            for h in hs:
                w = self.tabard_w(h)
                angs = np.linspace(c - w, c + w, 9)
                outer.append([tp.point(self.th(h), a, TABARD_LIFT) for a in angs])
                inner.append([tp.point(self.th(h), a, -0.004) for a in angs])
            part = C.thick_patch(outer, inner)
            pid = b.add(part, "tabard", sb, name=nm, hull_pad=True)
            ids[nm] = pid
            # tarnished trim down both edges and round the neck
            for sg in (1.0, -1.0):
                strip = HB.painted_strip(tp, 0.0, self.th(0.07), self.th(0.86), 0.010,
                                         lambda z, a: TABARD_LIFT + 0.0012, steps=5,
                                         a_of_z=lambda z, sg=sg, c=c: c + sg * (TABARD_W - 2.5))
                b.add(strip, "trim", sb, name="tabard_trim", outline=False, covered_by=pid)
            # scorched, torn edge patches (char) along the lower edge of the panel at the belt and the sides
            for k, (h0, h1, a0, a1) in enumerate(((0.30, 0.52, c + TABARD_W - 14.0, c + TABARD_W - 2.0),
                                                  (0.60, 0.74, c - TABARD_W + 2.0, c - TABARD_W + 13.0))):
                b.add(G.surface_patch(_TorsoSurf(self), h0, h1, a0, a1, TABARD_LIFT + 0.0010, nu=2, na=2,
                                      shape=lambda tu, ta: 0.35 + 0.65 * math.sin(math.pi * tu)),
                      "tabard_char", sb, name="tabard_scorch", outline=False, covered_by=pid)
        # neckline trim
        nl = C.surface_band(Profile([(z, rx + TABARD_LIFT, ry + TABARD_LIFT, cy, cx) for z, (rx, ry, cy, cx) in
                                     ((self.th(h), tp.at(self.th(h))) for h in np.linspace(0.96, 1.06, 5))],
                                    exp=2.3), self.th(1.012), 0.014, 0.003, segs=18, out0=-0.002)
        b.add(nl, "trim", sb, name="tabard_trim", outline=False, covered_by=ids["tabard_front"])
        self.add_sigil(b, ids["tabard_front"])
        self.add_tabard_skirts(b)
        # ash dusting on the shoulders (over the tabard)
        for sg in (1.0, -1.0):
            b.add(G.surface_patch(_TorsoSurf(self), 0.90, 1.0, sg * 58.0, sg * 118.0, TABARD_LIFT + 0.0016, nu=2,
                                  na=4, shape=lambda tu, ta: 0.55 + 0.45 * math.sin(math.pi * ta)),
                  "ash", sb, name="ash", outline=False, covered_by=ids["tabard_front"])

    def add_sigil(self, b: MBuilder, pid: int) -> None:
        """The church's open palm, upright on the chest (palm, four fingers, the thumb out to the side), scorched
        dark into the crimson: ~11 cm tall, so it reads as a hand at the W1 camera (≈ 10 px)."""
        surf = _TorsoSurf(self)
        sb = self.spine_bind()
        lift = TABARD_LIFT + 0.0014
        h0, h1, ac, half = 0.50, 0.605, -3.0, 9.0
        b.add(G.surface_patch(surf, h0, h1, ac - half, ac + half, lift, nu=2, na=4,
                              shape=lambda tu, ta: 0.78 + 0.22 * math.sin(math.pi * tu)), "sigil", sb, name="sigil",
              outline=False, covered_by=pid)
        for k, da in enumerate((-6.6, -2.2, 2.2, 6.6)):
            ln = (0.10, 0.118, 0.112, 0.092)[k]
            b.add(G.surface_paint(surf, [(h1 - 0.01, ac + da), (h1 + ln * 0.5, ac + da * 1.04), (h1 + ln, ac + da * 1.08)],
                                  0.0115, lift, steps=3, taper=0.25), "sigil", sb, name="sigil_finger", outline=False,
                  covered_by=pid)
        b.add(G.surface_paint(surf, [(h0 + 0.02, ac + half - 1.0), (h0 + 0.06, ac + half + 6.0),
                                     (h0 + 0.10, ac + half + 10.0)], 0.0115, lift, steps=3, taper=0.3), "sigil", sb,
              name="sigil_finger", outline=False, covered_by=pid)

    def panel_grid(self, sg: float, w_top: float, w_bot: float, L: float, k0: float, k1: float, lift: float,
                   cols: int, rows: int, seed: str, ragged: float = 0.035):
        """Points of a tabard skirt panel (front sg 1 / back −1) from row k0 to k1 (0 belt … 1 hem), ``lift``
        metres off its mid-plane toward the outside: a gently bowed sheet with a torn, uneven hem."""
        t = self.tabard_top(sg)
        rng = scene.rng(self.asset, "hem", seed)
        jit = [rng.uniform(-1.0, 1.0) for _ in range(cols + 1)]
        tear = [0.0] * (cols + 1)
        for c in range(cols + 1):     # two deeper tears
            u = c / cols
            tear[c] = 0.09 * max(0.0, 1.0 - abs(u - 0.27) / 0.09) + 0.07 * max(0.0, 1.0 - abs(u - 0.74) / 0.08)
        grid = []
        for r in range(rows + 1):
            k = lerp(k0, k1, r / rows)
            w = lerp(w_top, w_bot, k)
            row = []
            for c in range(cols + 1):
                u = c / cols - 0.5
                hem = L * (1.0 - tear[c] * 1.0 - ragged * (0.5 + 0.5 * jit[c]))
                z = t.z - hem * k
                bow = 0.022 * (1.0 - (2 * u) ** 2) * (0.4 + 0.6 * k)         # bowed over the thighs
                fold = 0.009 * math.sin(2 * math.pi * (u * 2.5 + 0.15)) * smooth01(k / 0.35)   # 2½ soft folds
                y = t.y - sg * (bow + fold + lift) - sg * 0.04 * k * k
                row.append(Vector((u * w, y, z)))
            grid.append(row)
        return grid

    def add_tabard_skirts(self, b: MBuilder) -> None:
        L = self.look.tabard_len
        for sg, nm in ((1.0, "tabard_f"), (-1.0, "tabard_b")):
            t = self.tabard_top(sg)
            bones = ("pelvis", f"{nm}_01", f"{nm}_02")
            bind = GB.chain_bind(t, t + Vector((0, 0, -L)), bones)
            cols, rows = 10, 5
            w0, w1 = 0.25, 0.30
            th = 0.006
            o = self.panel_grid(sg, w0, w1, L, 0.0, 1.0, th / 2, cols, rows, nm)
            i = self.panel_grid(sg, w0, w1, L, 0.0, 1.0, -th / 2, cols, rows, nm)
            part = C.thick_patch(o, i, sub_tags=True)
            pid = b.add(part, "tabard", bind, name="tabard_skirt", sub_regions={1: "tabard", 2: "tabard", 0: "tabard_char"},
                        hull_pad=True)
            # charred hem band on both faces
            for side in (1.0, -1.0):
                band = self.panel_grid(sg, w0, w1, L, 0.86, 1.0, side * (th / 2 + 0.0011), cols, 2, nm)
                part = HB.sheet_grid(band)
                f = next(iter(part.bm.faces))
                c0 = f.calc_center_median()
                want = Vector((0, -sg * side, 0))
                if f.normal.dot(want) < 0:
                    bmesh.ops.reverse_faces(part.bm, faces=list(part.bm.faces))
                    part.bm.normal_update()
                b.add(part, "tabard_char", bind, name="tabard_hem", outline=False, covered_by=pid)
            # the trim along the panel's sides (front only: the back is the plain field)
            if sg > 0:
                for c in (0, cols):
                    pts = [self.panel_grid(sg, w0, w1, L, k, k, th / 2 + 0.0012, cols, 1, nm)[0][c]
                           for k in np.linspace(0.0, 0.84, 6)]
                    strip = M.tube(pts, [0.005] * len(pts), sides=4)
                    b.add(strip, "trim", bind, name="tabard_trim", outline=False, covered_by=pid)

    # ── belt ────────────────────────────────────────────────────────────────────────────────────────────
    def add_belt(self, b: MBuilder, ids: dict) -> None:
        tp = self.torso
        z = self.th(0.035)
        rings = []
        for h in np.linspace(-0.06, 0.13, 5):
            zz = self.th(h)
            rx, ry, cy, cx = tp.at(zz)
            rings.append((zz, rx + TABARD_LIFT, ry + TABARD_LIFT, cy, cx))
        bp = Profile(rings, exp=2.3)
        sb = Bind.blend("pelvis", "spine_01", smooth=0.04)
        belt = C.surface_band(bp, z, 0.052, 0.012, segs=22, out0=-0.006)
        bid = b.add(belt, "leather", sb, name="belt", hull_pad=True)
        # iron buckle (a little to the near side)
        p = bp.point(z, -8.0, 0.012)
        n = bp.normal(z, -8.0)
        fr = R.frame(p, n, (0, 0, 1))
        frame = M.box((0.052, 0.010, 0.050), (0, 0.003, 0), bevel=0.0)
        b.add(frame.transform(fr), "helm_dent", sb, name="buckle", outline=False, covered_by=bid)
        inner = M.box((0.028, 0.011, 0.024), (0, 0.006, 0), bevel=0.0)
        b.add(inner.transform(fr), "leather_dark", sb, name="buckle_pin", outline=False, covered_by=bid)
        # loose strap end hanging from the buckle
        s0 = bp.point(z - 0.012, 4.0, 0.013)
        strap = M.strap([s0, s0 + Vector((0.006, -0.008, -0.06)), s0 + Vector((0.01, -0.012, -0.11))], 0.030, 0.005,
                        up=(0, -1, 0.1))
        b.add(strap, "leather", Bind.blend("pelvis", "tabard_f_01", falloff=3.0), name="belt_end", hull_pad=True)

    # ── mail skirt: two halves riding the thighs (split front and back under the tabard panels) ────────────
    def mail_skirt_profile(self) -> Profile:
        tp = self.torso
        z0 = self.th(0.03)
        rx0, ry0, cy0, _ = tp.at(z0)
        z1 = self.hip_z - 0.22
        return Profile([(z0, rx0 + 0.004, ry0 + 0.000, cy0), (self.th(-0.10), rx0 + 0.020, ry0 + 0.004, 0.0),
                        (lerp(self.th(-0.10), z1, 0.5), rx0 + 0.040, ry0 + 0.006, 0.0),
                        (z1, rx0 + 0.054, ry0 + 0.008, 0.0)], exp=2.6)

    def add_mail_skirt(self, b: MBuilder) -> None:
        sp = self.mail_skirt_profile()
        z0, z1 = sp.z1, sp.z0
        for side, sg in (("l", 1.0), ("r", -1.0)):
            a0, a1 = (12.0, 168.0) if sg > 0 else (-168.0, -12.0)
            hip = self.J[f"hip_{side}"]
            knee = self.J[f"knee_{side}"]
            thigh = f"thigh_{side}"

            def wfn(co, thigh=thigh, hip=hip):
                k = np.clip((z0 - co[:, 2]) / (z0 - z1), 0.0, 1.0)
                wt = 0.85 * smooth01_np(k / 0.85)
                return np.stack([1.0 - wt, wt], axis=1)
            bind = Bind.custom(("pelvis", thigh), wfn)
            rows = 4
            outer, inner = [], []
            for r in range(rows + 1):
                z = lerp(z0, z1, r / rows)
                angs = np.linspace(a0, a1, 8)
                outer.append([sp.point(z, a, 0.004) for a in angs])
                inner.append([sp.point(z, a, -0.004) for a in angs])
            part = C.thick_patch(outer, inner)
            pid = b.add(part, "mail", bind, name="mail_skirt", hull_pad=True)
            # mail rows (painted rings) + a ragged dark hem
            for zz in (lerp(z0, z1, 0.42), lerp(z0, z1, 0.78)):
                grid = [[sp.point(zz + dz, a, 0.0052) for a in np.linspace(a0 + 3, a1 - 3, 8)] for dz in (-0.003, 0.003)]
                st = HB.outward(HB.sheet_grid(grid), lambda c: Vector((0, 0, c.z)))
                b.add(st, "mail_line", bind, name="mail_row", outline=False, covered_by=pid)

    # ── arms: mail sleeve to the elbow, gambeson forearm, bare ash hand ───────────────────────────────────
    def add_arm(self, b: MBuilder, side: str) -> None:
        J = self.J
        S, E, W = J[f"shoulder_{side}"], J[f"elbow_{side}"], J[f"wrist_{side}"]
        ua, la, cl = f"upperarm_{side}", f"lowerarm_{side}", f"clavicle_{side}"
        dua = (E - S).normalized()
        dla = (W - E).normalized()
        lod = getattr(self, "lod", 0)
        sides = 10 if lod else 14
        bind = Bind.blend(cl, ua, la, f"hand_{side}", falloff=5.0)
        # gambeson arm (shoulder → wrist) under the mail sleeve: elbow loops either side of the hinge; gaunt forearm
        pts = [S - dua * 0.03, S.lerp(E, 0.5), S.lerp(E, 0.85), E, E.lerp(W, 0.2), E.lerp(W, 0.6), W - dla * 0.012]
        rads = [0.050, 0.046, 0.043, 0.042, 0.040, 0.036, 0.032]
        arm = M.tube(pts, rads, sides=sides, dome0=0.03, dome1=0.02)
        aid = GB_add_exact(b, arm, "gambeson", bind, name=f"arm_{side}", hull_pad=True) if lod else \
            b.add(arm, "gambeson", bind, name=f"arm_{side}", hull_pad=True)
        if self.look.mail_sleeves:
            # mail shreds: a short loose sleeve over the shoulder and upper arm, its hem torn into tongues
            sl_bind = Bind.blend(cl, ua, falloff=5.0)
            sleeve = ragged_tube(S - dua * 0.05 + Vector((0, 0, 0.010)), dua, 0.20, [0.062, 0.060, 0.057],
                                 sides, scene.rng(self.asset, "sleeve", side), jag=0.055)
            sid = b.add(sleeve, "mail", sl_bind, name=f"mail_sleeve_{side}", hull_pad=True)
            for t in (0.42, 0.72):
                c = S - dua * 0.05 + dua * (0.20 * t)
                r = lerp(0.061, 0.058, t) + 0.0012
                ring = M.tube([c - dua * 0.0025, c + dua * 0.0025], [r, r], sides=sides, cap0=None, cap1=None)
                b.add(ring, "mail_line", sl_bind, name="mail_row", outline=False, covered_by=sid)
        # wrist cuff (leather)
        cuff = M.tube([W - dla * 0.05, W - dla * 0.012], [0.039, 0.038], sides=sides, cap0=None, cap1=None)
        b.add(cuff, "leather", Bind.blend(la, f"hand_{side}", falloff=6.0), name=f"cuff_{side}", outline=False,
              covered_by=aid)
        self.add_hand(b, side)

    def add_hand(self, b: MBuilder, side: str) -> None:
        J = self.J
        W, T, G_ = J[f"wrist_{side}"], J[f"handtip_{side}"], J[f"grip_{side}"]
        hd = (T - W).normalized()
        hbn = f"hand_{side}"
        b.add(M.tube([W - hd * 0.025, W + hd * 0.014], [0.030, 0.032], sides=10), "skin",
              Bind.blend(f"lowerarm_{side}", hbn, falloff=6.0), name=f"wrist_{side}")
        kind = self.look.hand_r if side == "r" else self.look.hand_l
        if kind == "fist":
            grip = Vector((0, -1, 0))
            zx = (grip - hd * grip.dot(hd)).normalized()
            xx = hd.cross(zx)
            fr = Matrix.Identity(4)
            for i in range(3):
                fr[i][0], fr[i][1], fr[i][2], fr[i][3] = xx[i], hd[i], zx[i], G_[i]
            fist = HB.ell((0.040, 0.047, 0.044), (0, 0.006, 0), sides=12, rings=8, exp=2.5, exp_v=2.3)
            fid = b.add(fist.transform(fr), "skin", Bind.rigid(hbn), name=f"fist_{side}", hull_pad=True)
            sg = 1.0 if side == "r" else -1.0
            thumb = M.capsule((sg * 0.026, -0.012, 0.022), (sg * 0.013, 0.024, 0.035), 0.013, 0.011, sides=7)
            b.add(thumb.transform(fr), "skin", Bind.rigid(hbn), name=f"thumb_{side}", outline=False, covered_by=fid)
            # knuckle shadow line
            kn = M.tube([(-0.034, 0.030, 0.030), (0.0, 0.034, 0.036), (0.034, 0.030, 0.030)], [0.004] * 3, sides=4)
            b.add(kn.transform(fr), "skin_shadow", Bind.rigid(hbn), name="knuckle", outline=False, covered_by=fid)
        else:
            # gaunt open hand: long fused fingers curled into a claw
            mit = M.mitten(0.125, 0.074, 0.030, side=side, curl_deg=34.0)
            fr = R.frame(W - hd * 0.010, hd, Vector((0, -1, 0)))
            hid = b.add(mit.transform(fr), "skin", Bind.rigid(hbn), name=f"hand_{side}", hull_pad=True)
            # finger grooves (painted) on the back of the hand
            for k in (-1.0, 0.0, 1.0):
                g = M.tube([(k * 0.014, 0.060, 0.0), (k * 0.016, 0.098, -0.004)], [0.0024, 0.002], sides=4)
                b.add(g.transform(fr @ Matrix.Translation((0, 0, 0.0))), "skin_shadow", Bind.rigid(hbn),
                      name="knuckle", outline=False, covered_by=hid)

    # ── legs: dark hose, worn boots ──────────────────────────────────────────────────────────────────────
    def add_leg(self, b: MBuilder, side: str) -> None:
        J = self.J
        Hp, Kn, A = J[f"hip_{side}"], J[f"knee_{side}"], J[f"ankle_{side}"]
        th_, ca, ft, bl = f"thigh_{side}", f"calf_{side}", f"foot_{side}", f"ball_{side}"
        top = Hp + Vector((0, 0, -0.04))
        pts = [top, top.lerp(Kn, 0.5), Kn, Kn.lerp(A, 0.45), A + Vector((0, 0, 0.06))]
        rads = [0.074, 0.066, 0.056, 0.050, 0.042]
        lod = getattr(self, "lod", 0)
        b.add(M.tube(pts, rads, sides=10 if lod else 12), "hood", Bind.blend("pelvis", th_, ca, ft, falloff=5.0),
              name=f"leg_{side}")
        # knee pad (quilted)
        kp = HB.ell((0.050, 0.030, 0.045), tuple(Kn + Vector((0, -0.036, 0.008))), sides=10, rings=6)
        b.add(kp, "gambeson", Bind.blend(th_, ca, falloff=6.0), name=f"knee_{side}", hull_pad=True)
        ftl = self.spec.foot_len
        bw = 0.110
        boot = M.boot(ftl, bw, 0.088, shaft=0.0, toe_up=0.016)
        bid = b.add(boot.translate((A.x, 0, 0)), "leather", Bind.blend(ca, ft, bl, falloff=5.0), name=f"boot_{side}",
                    hull_pad=True)
        z_top = A.z + 0.42 * (Kn.z - A.z)
        shaft = M.loft_z([(0.035, 0.052, 0.050, 0, 0.006), (z_top - 0.02, 0.056, 0.054, 0, 0.004),
                          (z_top + 0.004, 0.060, 0.058, 0, 0.004)], sides=12 if not lod else 10, cap0="fan",
                         cap1="fan")
        sid = b.add(shaft.translate((A.x, 0, 0)), "leather", Bind.blend(ca, ft, falloff=6.0), name=f"boot_shaft_{side}",
                    hull_pad=True)
        cuff = M.loft_z([(z_top - 0.03, 0.062, 0.060, 0, 0.004), (z_top + 0.008, 0.066, 0.064, 0, 0.004)],
                        sides=12, cap0=None, cap1=None)
        b.add(cuff.translate((A.x, 0, 0)), "leather_dark", Bind.blend(ca, ft, falloff=6.0), name=f"boot_cuff_{side}",
              outline=False, covered_by=sid)
        sole = M.loft_z([(0.0, bw * 0.5, ftl * 0.5, 0, -0.23 * ftl), (0.012, bw * 0.52, ftl * 0.51, 0, -0.23 * ftl)],
                        sides=12, exp=3.0, cap0="fan", cap1="fan")
        b.add(sole.translate((A.x, 0, 0)), "leather_dark", Bind.blend(ft, bl, falloff=6.0), name=f"sole_{side}",
              outline=False, covered_by=bid)

    # ── broken arrow shafts in the back ──────────────────────────────────────────────────────────────────
    def add_arrows(self, b: MBuilder, ids: dict) -> None:
        sb = Bind.rigid("spine_03")
        for k, (h, a, yaw, ln, fletched) in enumerate(self.look.arrows):
            bone = "spine_03" if h > 0.62 else "spine_02"
            p = self.tp(h, a, TABARD_LIFT - 0.02)
            n = self.tn(h, a)
            d = (n + Vector((0, 0, 0.75))).normalized()
            d = Matrix.Rotation(math.radians(yaw), 3, "Z") @ d
            q = p + d * ln
            shaft = M.tube([p, p.lerp(q, 0.5), q], [0.0058, 0.0056, 0.0050], sides=6, dome0=0.003, dome1=0.003)
            aid = b.add(shaft, "wood", Bind.rigid(bone), name="arrow", hull_pad=True)
            # splintered end (a darker jagged tip) or the fletching
            if fletched:
                side = d.orthogonal().normalized()
                for j in range(3):
                    s = Matrix.Rotation(math.radians(120.0 * j), 3, d) @ side
                    v0 = q - d * 0.085
                    vane = _tri_strip([v0, v0 + d * 0.04 + s * 0.016, q - d * 0.006 + s * 0.012, q - d * 0.004],
                                      s.cross(d))
                    b.add(vane, "fletch", Bind.rigid(bone), name="fletch", outline=False, covered_by=aid)
            else:
                b.add(M.tube([q - d * 0.012, q + d * 0.004], [0.0062, 0.002], sides=5), "leather_dark",
                      Bind.rigid(bone), name="arrow_end", outline=False, covered_by=aid)

    # ── member gear ──────────────────────────────────────────────────────────────────────────────────────
    def add_coif(self, b: MBuilder, ids: dict) -> None:
        """Mail coif: a hood of mail over the skull, open at the face, its cape over the collar and shoulders."""
        hp = self.head
        bind = Bind.blend("head", "neck_01", "spine_03", falloff=3.5)

        def head_r(z, a, out):
            return hp.point(self.hc + z, a, out)
        rows = []
        zs = list(np.linspace(-0.13, 0.135, 8))
        for z in zs:
            row = []
            for a in G.angles(COIF_SEGS):
                fa = abs(((a + 180.0) % 360.0) - 180.0)
                if z < -0.09:
                    out = 0.012 + (-0.09 - z) * 0.6
                else:
                    out = 0.012
                row.append(head_r(min(z, 0.126), a, out) + Vector((0, 0, max(0.0, z - 0.126))))
            rows.append(row)
        # open face (cut faces in the front window)
        self._coif_window = (-0.105, 0.052, 46.0)
        part = _ring_grid(rows, cut=lambda r, c: (-0.105 < zs[r] < 0.052)
                          and _angdist(G.angles(COIF_SEGS)[c], 0.0) < 46.0)
        cid = b.add(part, "mail", bind, name="coif", hull_pad=True)
        # cape of mail on the shoulders (a flared collar)
        z0, z1 = self.nz + 0.02, self.nz - 0.07
        tp = self.torso
        outer, inner = [], []
        for z in np.linspace(z0, z1, 4):
            rx, ry, cy, cx = tp.at(z)
            grow = (z0 - z) / (z0 - z1)
            pr = Profile([(z - 0.01, rx + 0.02 + grow * 0.03, ry + 0.02 + grow * 0.02, cy, 0.0),
                          (z + 0.01, rx + 0.02 + grow * 0.03, ry + 0.02 + grow * 0.02, cy, 0.0)], exp=2.3)
            outer.append([pr.point(z, a, 0.004) for a in G.angles(COIF_SEGS)])
            inner.append([pr.point(z, a, -0.004) for a in G.angles(COIF_SEGS)])
        cape = C.thick_patch(outer, inner, wrap_u=True)
        pid = b.add(cape, "mail", Bind.blend("neck_01", "spine_03", falloff=3.5), name="coif_cape", hull_pad=True)
        for z in (self.nz - 0.03,):
            rx, ry, cy, cx = tp.at(z)
            grow = (z0 - z) / (z0 - z1)
            pr = Profile([(z - 0.01, rx + 0.025 + grow * 0.03, ry + 0.025 + grow * 0.02, cy, 0.0),
                          (z + 0.01, rx + 0.025 + grow * 0.03, ry + 0.025 + grow * 0.02, cy, 0.0)], exp=2.3)
            ring = C.surface_band(pr, z, 0.005, 0.0016, segs=COIF_SEGS, out0=-0.001)
            b.add(ring, "mail_line", Bind.blend("neck_01", "spine_03", falloff=3.5), name="mail_row", outline=False,
                  covered_by=pid)

    def add_quiver(self, b: MBuilder) -> None:
        """Bolt quiver at the right hip: a leather tube angled back, bolt fletching above its mouth."""
        J = self.J
        top = Vector((-0.20, 0.06, self.th(0.08)))
        bot = Vector((-0.17, 0.12, self.th(-0.62)))
        bind = Bind.blend("pelvis", "thigh_r", falloff=6.0) if False else Bind.rigid("pelvis")
        q = M.tube([bot, bot.lerp(top, 0.5), top], [0.040, 0.044, 0.046], sides=10, dome0=0.01, cap1=None)
        qid = b.add(q, "arbalest.quiver", bind, name="quiver", hull_pad=True)
        rim = M.tube([top - (top - bot).normalized() * 0.012, top + (top - bot).normalized() * 0.004],
                     [0.049, 0.049], sides=10, cap0=None, cap1=None)
        b.add(rim, "leather_dark", bind, name="quiver_rim", outline=False, covered_by=qid)
        d = (top - bot).normalized()
        for k, (dx, dy) in enumerate(((0.012, 0.0), (-0.012, 0.010), (0.0, -0.014), (-0.004, 0.018))):
            p0 = top - d * 0.03 + Vector((dx, dy, 0))
            p1 = p0 + d * (0.08 + 0.01 * (k % 2))
            bolt = M.tube([p0, p1], [0.005, 0.005], sides=5, dome0=None, dome1=0.003)
            b.add(bolt, "crossbow.bolt", bind, name="quiver_bolt")
        strap = M.strap([Vector((-0.17, -0.02, self.th(0.07))), Vector((-0.20, 0.05, self.th(0.06)))], 0.02, 0.004,
                        up=(-1, 0, 0))
        b.add(strap, "leather_dark", bind, name="quiver_strap", outline=False, covered_by=qid)

    def add_pavise(self, b: MBuilder) -> None:
        """A pavise slung on the back: a tall faded-crimson board with a dark iron rim and a central ridge, hung
        canted on its strap (``PAVISE_YAW`` about the spine: the board rides on the right shoulder blade, its left
        edge off the back); the arbalester's Death rolls the fold onto the flank (``risen_arbalest.DEATH_ROLL``, art
        review r3), so the board lands on its edge — the field off the key light, its iron rim up — instead of a flat
        lit slab on the corpse. The board is on the crimson-cloth slot (no cream rim)."""
        n0 = len(b.parts)
        bind = Bind.rigid("spine_03")
        zc = self.th(0.30) - 0.10               # slung low: its top stays under the helm brim when the head tips back
        _, ry, cy, _ = self.torso.at(self.th(0.45))
        y0 = cy + ry + TABARD_LIFT + 0.07
        H, W = 0.70, 0.40
        outline2 = [(-W / 2, -H / 2), (W / 2, -H / 2), (W / 2 * 0.94, H / 2), (0.0, H / 2 + 0.04),
                    (-W / 2 * 0.94, H / 2)]
        # canted on the strap: turned PAVISE_YAW about the spine round the edge that stays on the back
        sg = 1.0 if PAVISE_YAW >= 0.0 else -1.0
        piv = Vector((-sg * W / 2, y0 - 0.012, 0.0))
        cant = Matrix.Translation(piv) @ Matrix.Rotation(math.radians(PAVISE_YAW), 4, "Z") @ Matrix.Translation(-piv)
        plate = M.plate(C.ccw(outline2), 0.024, bevel=0.006)
        # plate lies in XY (thickness Z): stand it up facing +Y (back), tilt top away from the back
        m = cant @ Matrix.Translation((0, y0, zc + 0.06)) @ Matrix.Rotation(math.radians(-8.0), 4, "X") @ \
            Matrix.Rotation(math.radians(90.0), 4, "X")
        plate.transform(m)
        pid = b.add(plate, "arbalest.pavise_field", bind, name="pavise", hull_pad=True)
        ridge = M.box((0.05, 0.016, H * 0.96), (0, 0, 0), bevel=0.006)
        ridge.transform(cant @ Matrix.Translation((0, y0 + 0.016, zc + 0.06)) @
                        Matrix.Rotation(math.radians(-8.0), 4, "X"))
        b.add(ridge, "arbalest.pavise_rim", bind, name="pavise_ridge", outline=False, covered_by=pid)
        rim_pts = [Vector((x, 0, z)) for x, z in outline2] + [Vector((outline2[0][0], 0, outline2[0][1]))]
        mm = cant @ Matrix.Translation((0, y0 + 0.013, zc + 0.06)) @ Matrix.Rotation(math.radians(-8.0), 4, "X")
        rim = M.tube([mm @ p for p in rim_pts], [0.008] * len(rim_pts), sides=5)
        b.add(rim, "arbalest.pavise_rim", bind, name="pavise_rim", outline=False, covered_by=pid)
        # the crusade's open palm scorched on the board, ash smears from the top edge
        palm = M.box((0.085, 0.002, 0.075), (0, 0, 0), bevel=0.0)
        palm.transform(mm @ Matrix.Translation((0.0, 0.0006, 0.06)))
        b.add(palm, "towershield.sigil", bind, name="sigil", outline=False, covered_by=pid)
        for k, dx in enumerate((-0.030, -0.010, 0.010, 0.030)):
            ln = (0.060, 0.075, 0.070, 0.056)[k]
            f = M.box((0.014, 0.002, ln), (0, 0, 0), bevel=0.0)
            f.transform(mm @ Matrix.Translation((dx, 0.0006, 0.10 + ln / 2)))
            b.add(f, "towershield.sigil", bind, name="sigil_finger", outline=False, covered_by=pid)
        th_ = M.box((0.014, 0.002, 0.055), (0, 0, 0), bevel=0.0)
        th_.transform(mm @ Matrix.Translation((0.052, 0.0006, 0.06)) @ Matrix.Rotation(math.radians(-38.0), 4, "Y"))
        b.add(th_, "towershield.sigil", bind, name="sigil_finger", outline=False, covered_by=pid)
        for x, z0, ln in ((-0.12, 0.30, 0.20), (0.13, 0.32, 0.14)):
            st = M.box((0.024, 0.002, ln), (0, 0, 0), bevel=0.0)
            st.transform(mm @ Matrix.Translation((x, 0.0008, z0 - ln / 2)))
            b.add(st, "towershield.ash", bind, name="ash", outline=False, covered_by=pid)
        self.cloth_ids.update(range(n0, len(b.parts)))      # the board and its paint: the crimson-cloth slot
        # carry strap over the right shoulder (lies on the tabard: no hull of its own)
        s0 = self.tp(0.90, 155.0, TABARD_LIFT + 0.006)
        s1 = self.tp(0.98, -100.0, TABARD_LIFT + 0.004)
        s2 = self.tp(0.62, -30.0, TABARD_LIFT + 0.008)
        s3 = self.tp(0.20, 30.0, TABARD_LIFT + 0.008)
        strap = M.strap([s0, s1, s2, s3], 0.032, 0.004, up=[tuple(self.tn(h, a)) for h, a in
                                                             ((0.9, 155.0), (0.98, -100.0), (0.62, -30.0), (0.2, 30.0))])
        b.add(strap, "leather_dark", self.spine_bind(), name="pavise_strap", hull_pad=True)

    def add_hip_sword(self, b: MBuilder) -> None:
        """A sheathed sword at the left hip (the standard-bearer): leather scabbard, dark iron cross and pommel."""
        bind = Bind.rigid("pelvis")
        top = Vector((0.19, -0.03, self.th(0.02)))
        d = Vector((0.08, 0.55, -0.83)).normalized()
        tip = top + d * 0.70
        sc = M.tube([top, top.lerp(tip, 0.5), tip], [0.022, 0.020, 0.012], sides=8, dome0=None, dome1=0.008)
        sid = b.add(sc, "leather", bind, name="scabbard", hull_pad=True)
        chape = M.tube([tip - d * 0.06, tip], [0.016, 0.011], sides=8, dome0=None, dome1=0.006)
        b.add(chape, "helm_dent", bind, name="scabbard_chape", outline=False, covered_by=sid)
        g0 = top - d * 0.008
        side = d.cross(Vector((1, 0, 0))).normalized()
        guard = M.tube([g0 - side * 0.09, g0, g0 + side * 0.09], [0.010, 0.012, 0.010], sides=6)
        b.add(guard, "helm_dent", bind, name="hip_guard")
        grip = M.tube([g0, g0 - d * 0.10], [0.014, 0.013], sides=6)
        b.add(grip, "leather_dark", bind, name="hip_grip")
        pom = HB.ell((0.022, 0.022, 0.022), tuple(g0 - d * 0.118), sides=8, rings=6)
        b.add(pom, "helm_dent", bind, name="hip_pommel")


# ── small geometry helpers ─────────────────────────────────────────────────────────────────────────────────────
class SimpleWeb:
    """The ``GoblinAnims`` reach numbers (``kit.w.upper_arm + fore_arm`` in rig units)."""

    def __init__(self, spec: HumanoidSpec):
        self.upper_arm = spec.upper_arm / 0.0316
        self.fore_arm = spec.fore_arm / 0.0316


class _HeadSurf:
    """``mgeo`` surface adapter for the head profile (u = z above the head centre)."""

    def __init__(self, kit: UnburiedKit):
        self.k = kit

    def point(self, u, a, out=0.0):
        return self.k.hp(u, a, out)

    def normal(self, u, a):
        return self.k.hn(u, a)


class _TorsoSurf:
    """``mgeo`` surface adapter for the torso (u = torso fraction h)."""

    def __init__(self, kit: UnburiedKit):
        self.k = kit

    def point(self, u, a, out=0.0):
        return self.k.tp(u, a, out)

    def normal(self, u, a):
        return self.k.tn(u, a)


def ragged_tube(top: Vector, axis: Vector, length: float, radii, sides: int, rng, jag: float = 0.05,
                rows: int = 3) -> M.Part:
    """An open-ended sleeve down ``axis`` from ``top`` (domed shut at the top) whose hem is torn into uneven tongues
    (each column's length shortened by up to ``jag``): mail shreds, rags."""
    ax = Vector(axis).normalized()
    side = ax.orthogonal().normalized()
    fwd = ax.cross(side)
    cuts = [jag * (0.5 + 0.5 * math.sin(2.6 * c + 0.7)) * rng.uniform(0.35, 1.0) for c in range(sides)]
    bm = bmesh.new()
    rings = []
    for r in range(rows + 1):
        t = r / rows
        rad = radii[min(len(radii) - 1, int(round(t * (len(radii) - 1))))]
        ring = []
        for c in range(sides):
            a = 2 * math.pi * c / sides
            ln = length - cuts[c] * t
            ring.append(bm.verts.new(top + ax * (ln * t if r < rows else ln) + (side * math.cos(a) + fwd * math.sin(a)) * rad))
        rings.append(ring)
    for r0, r1 in zip(rings, rings[1:]):
        for c in range(sides):
            c1 = (c + 1) % sides
            bm.faces.new((r0[c], r0[c1], r1[c1], r1[c]))
    cap = bm.verts.new(top - ax * radii[0] * 0.55)
    for c in range(sides):
        bm.faces.new((rings[0][(c + 1) % sides], rings[0][c], cap))
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return M.Part(bm).smooth(True)


def GB_add_exact(b, part, *args, **kw) -> int:
    """``Builder.add`` without the LOD decimation (the eyes, the sockets, the mouth: their shape is the identity)."""
    r = b.lod_ratio
    b.lod_ratio = 1.0
    try:
        return b.add(part, *args, **kw)
    finally:
        b.lod_ratio = r


def smooth01_np(x):
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3 - 2 * x)


def _angdist(a: float, b: float) -> float:
    return abs(((a - b) + 180.0) % 360.0 - 180.0)


def _tri_strip(pts, normal: Vector) -> M.Part:
    """A flat single-sided quad (4 points) facing ``normal`` (fletching vanes)."""
    bm = bmesh.new()
    vs = [bm.verts.new(tuple(p)) for p in pts]
    f = bm.faces.new(vs)
    bm.normal_update()
    if f.normal.dot(normal) < 0:
        f.normal_flip()
    # two-sided: a back face so the vane reads from both sides
    vs2 = [bm.verts.new(tuple(p)) for p in reversed(pts)]
    f2 = bm.faces.new(vs2)
    bm.normal_update()
    return M.Part(bm)


COIF_SEGS = 16                    # coif / cape columns (the face window cut spans ±46°)
PAVISE_YAW = 18.0                 # deg the slung pavise is canted about the spine (+: its left edge off the back)


def _ring_grid(rows, cut=None) -> M.Part:
    """Single-sided closed-ring surface from rows of points (columns wrap), faces where ``cut(r, c)`` dropped."""
    bm = bmesh.new()
    vs = [[bm.verts.new(tuple(p)) for p in row] for row in rows]
    n = len(rows[0])
    for r in range(len(rows) - 1):
        for c in range(n):
            if cut is not None and cut(r, c):
                continue
            c1 = (c + 1) % n
            bm.faces.new((vs[r][c], vs[r + 1][c], vs[r + 1][c1], vs[r][c1]))
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bmesh.ops.solidify(bm, geom=list(bm.faces), thickness=0.006)
    bm.normal_update()
    return M.Part(bm).smooth(True)


# ── head attachments (bone-local SMs on ``head``) ────────────────────────────────────────────────────────────────
# LOD1 of the head looks and held items (art review r2: they kept full detail on mobile-low, ~6 k triangles with the
# 2.9 k LOD1 body): the same generators at a collapse ratio, the paint-only details dropped; shipped as
# ``<asset>_LOD1.fbx`` with a manifest ``lods`` record (the static-mesh convention of the environment kit)
ATT_LOD1_RATIO = 0.45
ATT_LOD1_SKIP = ("dent", "ash", "wrap", "binding", "fuller", "helm_band", "notch", "sigil_finger", "scorch", "strap")


def att_builder(asset: str, pal: Palette, lod: int = 0, prefix: str = PREFIX) -> MBuilder:
    """``MBuilder`` of a head look / held item: LOD0, or LOD1 (:data:`ATT_LOD1_RATIO`, :data:`ATT_LOD1_SKIP`, hull
    proxies decimated further like the bodies' LOD1)."""
    if not lod:
        return MBuilder(asset, pal, prefix=prefix)
    return MBuilder(asset, pal, prefix=prefix, lod_ratio=ATT_LOD1_RATIO,
                    lod_skip=lambda n: GB.lod_base(n) in ATT_LOD1_SKIP, hull_ratio=0.75)


def _finish_attachment(b: MBuilder, pal: Palette, name: str, rig: R.Rig, bone: str) -> bpy.types.Object:
    """Build in armature space → the bone's rest frame at its head (= the object frame under ``attach_to_bone``,
    the UE socket convention), toon + baked hull at the monster class width."""
    obj = b.build(name)
    inv = rig.obj.data.bones[bone].matrix_local.inverted()
    obj.data.transform(inv)
    obj.data.update()
    mt = shading.toon_material(pal)
    outline.bake_hull(obj, outline.WIDTH_CM[OUTLINE_CLASS] / 100.0, mt, shading.outline_material(pal))
    for msg in outline.hull_color_report(obj, pal, OUTLINE_CLASS):
        print(f"[ink] WARNING {name}: {msg}")
    obj["af_outline_class"] = OUTLINE_CLASS
    return obj


HELM_TILT = 16.0          # deg the kettle helm sits pushed back (the W1 camera looks down over the brim at the eyes)


def build_kettle_helm(pal: Palette, kit: UnburiedKit, lod: int = 0) -> bpy.types.Object:
    """The crusade's kettle hat: a rounded iron dome with a low comb, a rim band and a wide brim that slopes down
    at the sides and back but is short and nearly level in front (W1 pitch −50°: the eyes stay visible), dents,
    ash dusting on the crown. Pushed back ``HELM_TILT`` on the head."""
    b = att_builder(HELM, pal, lod)
    b.use("unburied.helm", "unburied.helm_dent", "unburied.ash")
    hc = kit.hc
    piv = Vector((0, 0.02, hc + 0.02))
    tilt = Matrix.Translation(piv) @ Matrix.Rotation(math.radians(-HELM_TILT), 4, "X") @ Matrix.Translation(-piv)
    bind = Bind.rigid("root")
    zb = 0.044                                   # brim line (head z) at the front; it dips to the back
    rings = []
    for z, grow in ((zb - 0.006, 0.016), (0.07, 0.018), (0.10, 0.017), (0.122, 0.014), (0.138, 0.0)):
        rx, ry, cy, cx = kit.head.at(hc + min(z, 0.126))
        if z >= 0.138:
            rx, ry = 0.010, 0.012
        rings.append((hc + z, rx + grow + 0.004, ry + grow + 0.004, cy, cx))
    dome_p = Profile(rings, exp=2.1)
    zs = [hc + z for z in (zb - 0.006, 0.058, 0.075, 0.092, 0.108, 0.122, 0.132, 0.138)]
    dome = HB.profile_mesh(dome_p, zs, 20, cap0="fan", cap1="fan", dome1=0.006)
    did = b.add(dome.transform(tilt), "helm", bind, name="helm_dome", hull_pad=True)
    # rim band at the base of the dome
    band = C.surface_band(dome_p, hc + zb + 0.004, 0.016, 0.004, segs=24, out0=-0.002)
    b.add(band.transform(tilt), "helm_dent", bind, name="helm_band", outline=False, covered_by=did)
    # comb ridge front → back
    pts = [dome_p.point(hc + z, a, 0.0) for z, a in ((0.075, 0.0), (0.118, 0.0), (0.137, 90.0), (0.118, 180.0),
                                                      (0.075, 180.0))]
    pts[2] = Vector((0, rings[-1][3], hc + 0.142))
    comb = M.tube(pts, [0.006, 0.008, 0.008, 0.008, 0.006], sides=5)
    b.add(comb.transform(tilt), "helm_dent", bind, name="helm_comb", outline=False, covered_by=did)
    # brim: a two-sided ring from the dome base out, width and droop by angle (short and level at the front)
    rx0, ry0, cy0, cx0 = dome_p.at(hc + zb - 0.006)
    angs = G.angles(28)
    outer_top, outer_bot = [], []
    for k in (0.0, 0.5, 1.0):
        top, bot = [], []
        for a in angs:
            fa = _angdist(a, 0.0) / 180.0                         # 0 front … 1 back
            wid = lerp(0.024, 0.058, smooth01(fa * 1.6))
            drop = lerp(0.004, 0.026, smooth01(fa * 1.4))
            t = math.radians(a - 90.0)
            dx, dy = math.cos(t), math.sin(t)
            r_in_x, r_in_y = rx0 * 0.98, ry0 * 0.98
            base = Vector((cx0 + r_in_x * dx, cy0 + r_in_y * dy, hc + zb - 0.004))
            outdir = Vector((dx, dy, 0)).normalized()
            p = base + outdir * (wid * k) + Vector((0, 0, -drop * k * k))
            top.append(p + Vector((0, 0, 0.0035)))
            bot.append(p - Vector((0, 0, 0.0035)))
        outer_top.append(top)
        outer_bot.append(bot)
    brim = C.thick_patch(outer_top, outer_bot, wrap_u=True)
    bid = b.add(brim.transform(tilt), "helm", bind, name="helm_brim", hull_pad=True)
    # brim edge (rolled rim, darker)
    edge = [outer_top[-1][i].lerp(outer_bot[-1][i], 0.5) for i in range(len(angs))]
    edge.append(edge[0])
    rim = M.tube([tilt @ p for p in edge], [0.0055] * len(edge), sides=5)
    b.add(rim, "helm_dent", bind, name="helm_rim", outline=False, covered_by=bid)
    # dents and ash on the crown
    for z, a, r in ((0.11, 40.0, 0.013), (0.09, -70.0, 0.011), (0.12, 150.0, 0.012)):
        p, n = dome_p.point(hc + z, a, 0.0006), dome_p.normal(hc + z, a)
        dent = HB.ell((r, 0.0025, r * 0.8), (0, 0, 0), sides=8, rings=4).transform(R.frame(p, n, (0, 0, 1)))
        b.add(dent.transform(tilt), "helm_dent", bind, name="dent", outline=False, covered_by=did)

    class _DS:
        def point(self, u, a, out=0.0):
            return dome_p.point(hc + u, a, out)

        def normal(self, u, a):
            return dome_p.normal(hc + u, a)
    ash = G.surface_patch(_DS(), 0.112, 0.136, 60.0, 230.0, 0.0012, nu=2, na=5,
                          shape=lambda tu, ta: 0.5 + 0.5 * math.sin(math.pi * ta))
    b.add(ash.transform(tilt), "ash", bind, name="ash", outline=False, covered_by=did)
    return _finish_attachment(b, pal, HELM + ("_LOD1" if lod else ""), kit.rig, "head")


# the hood's face window (head z above the head centre, degrees from the front): straight sides at ±HOOD_WIN_A from
# the hood's open bottom up to HOOD_WIN_SPRING, then an elliptical arch to HOOD_WIN_TOP (the brow stays under the cowl
# but the W1 camera, 50° above, still sees the sockets and the violet pinpoints inside it)
HOOD_WIN_A, HOOD_WIN_SPRING, HOOD_WIN_TOP = 45.0, 0.004, 0.060
HOOD_TOP = 0.128
HOOD_ROWS = 9
HOOD_RECESS = -0.004             # the closed hull proxy dips this far under the skin inside the face window


def hood_window_a(z: float, grow: float = 0.0) -> float:
    """Half-width (deg) of the face window at head z (0 above its arch); ``grow`` widens it (the frayed edge)."""
    a0, top = HOOD_WIN_A + grow, HOOD_WIN_TOP + grow * 0.0016
    if z <= HOOD_WIN_SPRING:
        return a0
    t = (z - HOOD_WIN_SPRING) / (top - HOOD_WIN_SPRING)
    return 0.0 if t >= 1.0 else a0 * math.sqrt(1.0 - t * t)


def _hood_zb(a: float) -> float:
    """The hood's bottom edge: −0.086 at the front (the jaw) rising to −0.040 behind (the nape: the head tips back
    against the collar)."""
    return lerp(-0.086, -0.040, smooth01((_angdist(a, 0.0) - 70.0) / 80.0))


def _hood_out(z: float, a: float) -> float:
    fa = _angdist(a, 0.0)
    return 0.016 + 0.008 * smooth01((fa - 60.0) / 100.0) + 0.006 * smooth01((-0.06 - z) / 0.04)


def _hood_point(kit: UnburiedKit, z: float, a: float, out: float) -> Vector:
    p = kit.hp(min(z, 0.124), a, out)
    if z > 0.124:
        p.z += z - 0.124
    return p


class _HoodSurf:
    """``mgeo`` surface adapter on the hood's outer face (u = head z, ``out`` over the hood)."""

    def __init__(self, kit: UnburiedKit):
        self.k = kit

    def point(self, u, a, out=0.0):
        return _hood_point(self.k, u, a, _hood_out(u, a) + out)

    def normal(self, u, a):
        return self.k.hn(min(u, 0.124), a)


def _window_curve(grow: float = 0.0, n_side: int = 4, n_arch: int = 14) -> list:
    """(z, a) points of the face window's edge: up the right side (−a), over the arch, down the left side."""
    a0 = HOOD_WIN_A + grow
    zb = _hood_zb(a0)
    top = HOOD_WIN_TOP + grow * 0.0016
    pts = [(lerp(zb, HOOD_WIN_SPRING, k / n_side), -a0) for k in range(n_side)]
    for k in range(n_arch + 1):
        t = math.pi * k / n_arch
        pts.append((HOOD_WIN_SPRING + (top - HOOD_WIN_SPRING) * math.sin(t), -a0 * math.cos(t)))
    pts += [(lerp(HOOD_WIN_SPRING, zb, k / n_side), a0) for k in range(1, n_side + 1)]
    return pts


def _snap_to_window(z: float, a: float, curve) -> tuple[float, float]:
    """Nearest point of the window edge (metric: 0.095 m per radian round the face)."""
    r = 0.095
    best, bd = (z, a), 1e9
    for (z0, a0), (z1, a1) in zip(curve, curve[1:]):
        for t in np.linspace(0.0, 1.0, 9):
            zz, aa = lerp(z0, z1, t), lerp(a0, a1, t)
            d = (zz - z) ** 2 + (math.radians(aa - a) * r) ** 2
            if d < bd:
                bd, best = d, (zz, aa)
    return best


def hood_shell(kit: UnburiedKit, proxy: bool = False) -> M.Part:
    """The hood's loft (rows from the bottom edge to the crown, 24 columns + a fan cap on the crown).

    ``proxy=False``: the toon shell — the face window's cells cut, the vertices on the cut snapped onto the window
    edge (no stepped corners), single-sided then solidified 6 mm. ``proxy=True``: the **closed** hull proxy — the
    same loft with every cell kept and a bottom cap, the vertices inside the window sunk ``HOOD_RECESS`` under the
    skin (art review r2: the open shell's hull was padded round the cut and grew 10-39 cm faces across the face
    window — black wedges beside the head at the W1 camera; this proxy follows the hood and dips under the face, so
    the ink is the hood's own silhouette and the face keeps the skull's ink)."""
    angs = G.angles(24)
    nc = len(angs)
    grid = [[(lerp(_hood_zb(a), HOOD_TOP, i / HOOD_ROWS), a) for a in angs] for i in range(HOOD_ROWS + 1)]

    def inside(z, a, grow=0.0):
        return abs(((a + 180.0) % 360.0) - 180.0) < hood_window_a(z, grow) - 1e-6

    cut = [[inside(sum(grid[i + di][(j + dj) % nc][0] for di in (0, 1) for dj in (0, 1)) / 4.0,
                   angs[j] + 0.5 * ((angs[(j + 1) % nc] - angs[j]) % 360.0))
            for j in range(nc)] for i in range(HOOD_ROWS)]
    curve = _window_curve()
    used, edge = set(), set()
    for i in range(HOOD_ROWS):
        for j in range(nc):
            corners = [(i, j), (i + 1, j), (i + 1, (j + 1) % nc), (i, (j + 1) % nc)]
            if cut[i][j]:
                edge.update(corners)
            else:
                used.update(corners)
    edge &= used
    bm = bmesh.new()
    vs = {}
    for i in range(HOOD_ROWS + 1):
        for j in range(nc):
            z, a = grid[i][j]
            if (i, j) in edge:
                z, a = _snap_to_window(z, a, curve)
                out = _hood_out(z, a)
            elif proxy and inside(z, a):
                out = HOOD_RECESS
            elif not proxy and (i, j) not in used:
                continue
            else:
                out = _hood_out(z, a)
            vs[(i, j)] = bm.verts.new(tuple(_hood_point(kit, z, a, out)))
    for i in range(HOOD_ROWS):
        for j in range(nc):
            if cut[i][j] and not proxy:
                continue
            bm.faces.new((vs[(i, j)], vs[(i, (j + 1) % nc)], vs[(i + 1, (j + 1) % nc)], vs[(i + 1, j)]))
    # crown cap (a low dome), and on the proxy the bottom cap
    top = [vs[(HOOD_ROWS, j)] for j in range(nc)]
    ctop = sum((v.co for v in top), Vector()) / nc + Vector((0, 0, 0.006))
    c = bm.verts.new(tuple(ctop))
    for j in range(nc):
        bm.faces.new((top[j], top[(j + 1) % nc], c))
    if proxy:
        bot = [vs[(0, j)] for j in range(nc)]
        cb = bm.verts.new(tuple(sum((v.co for v in bot), Vector()) / nc + Vector((0, 0, 0.012))))
        for j in range(nc):
            bm.faces.new((bot[(j + 1) % nc], bot[j], cb))
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    # outward (an open sheet's recalc may pick either side): solidify then grows the wool inward, under the face
    # the proxy, the frayed edge and the ash are measured from
    hcen = Vector((0.0, kit.head.at(kit.hc)[2], kit.hc))
    if sum(f.normal.dot(f.calc_center_median() - hcen) for f in bm.faces) < 0.0:
        bmesh.ops.reverse_faces(bm, faces=list(bm.faces))
        bm.normal_update()
    if not proxy:
        bmesh.ops.solidify(bm, geom=list(bm.faces), thickness=0.006)
        bm.normal_update()
    return M.Part(bm).smooth(True)


def build_hood(pal: Palette, kit: UnburiedKit, lod: int = 0) -> bpy.types.Object:
    """A dark wool hood over the scalp, ears and nape, an arched face window (the grey face and the violet
    pinpoints inside the cowl), a frayed charred edge round the window, a short peak at the back. Its outline hull is
    baked from a **closed** proxy (:func:`hood_shell` ``proxy=True``, the standard pad), never from the open shell."""
    b = att_builder(HOOD, pal, lod)
    b.use("unburied.tabard_char", "unburied.ash")
    b.use_appended("unburied.cowl")
    b.swatch["hood"] = b.swatch["unburied.cowl"]
    bind = Bind.rigid("root")
    hid = b.add(hood_shell(kit), "hood", bind, name="hood", hull=hood_shell(kit, proxy=True), hull_pad=True)
    # the peak at the back
    p0 = _hood_point(kit, 0.10, 180.0, _hood_out(0.10, 180.0) - 0.002)
    peak = M.tube([p0, p0 + Vector((0, 0.028, 0.004)), p0 + Vector((0, 0.045, -0.012))], [0.028, 0.018, 0.006],
                  sides=6)
    b.add(peak, "hood", bind, name="hood_peak", hull_pad=True)
    # the window's frayed edge: a charred hem riding the hood's lip just outside the cut (it hides the cut wall)
    surf = _HoodSurf(kit)
    edge = [surf.point(z, a, 0.001) for z, a in _window_curve(grow=3.2, n_side=3, n_arch=10)]
    b.add(M.tube(edge, [0.0055] * len(edge), sides=5, dome0=0.003, dome1=0.003), "tabard_char", bind,
          name="hood_edge", outline=False, covered_by=hid)
    ash = G.surface_patch(surf, 0.098, 0.124, 70.0, 220.0, 0.0012, nu=2, na=5,
                          shape=lambda tu, ta: 0.5 + 0.5 * math.sin(math.pi * ta))
    b.add(ash, "ash", bind, name="ash", outline=False, covered_by=hid)
    return _finish_attachment(b, pal, HOOD + ("_LOD1" if lod else ""), kit.rig, "head")


# ── held items (weapon-bone space: grip at the origin, +Y blade / shaft, +Z up) ───────────────────────────────
def _finish_item(b: MBuilder, pal: Palette, name: str) -> bpy.types.Object:
    obj = b.build(name)
    mt = shading.toon_material(pal)
    outline.bake_hull(obj, outline.WIDTH_CM[OUTLINE_CLASS] / 100.0, mt, shading.outline_material(pal))
    for msg in outline.hull_color_report(obj, pal, OUTLINE_CLASS):
        print(f"[ink] WARNING {name}: {msg}")
    obj["af_outline_class"] = OUTLINE_CLASS
    return obj


SWORD_TIP = 0.82


def build_sword(pal: Palette, lod: int = 0) -> tuple[bpy.types.Object, list]:
    """The notched arming sword (the hero ``SM_Weapon_Sword`` T1 shapes re-used: hollow-ground blade section,
    cross guard, wrapped grip, wheel pommel) in the Unburied's dull steel: a 72 cm blade with chipped, notched edges
    and a blunted tip, a plain dark-iron cross, a disc pommel. Trimmed for the W1 camera (art review r2: 1.73 k
    triangles, near the hero sword's 2.1 k): 12 blade rows + 3 per notch (the notches still bite the silhouette at
    1080p), an 8-sided grip and wrap, a 10-sided pommel."""
    import weapons as WP
    b = att_builder(SWORD, pal, lod)
    b.use("unburied.steel", "unburied.steel_edge", "unburied.helm_dent", "unburied.leather",
          "unburied.leather_dark")
    y0, ytip = 0.092, SWORD_TIP
    y1 = ytip - 0.11

    def w(y):
        if y <= y1:
            return 0.034 - 0.006 * (y - y0) / (y1 - y0)
        g = (y - y1) / (ytip - y1)
        return max(0.028 * math.sqrt(max(0.0, 1 - g ** 1.6)), 0.0035)

    def th(y):
        if y <= y1:
            return 0.0110 - 0.0025 * (y - y0) / (y1 - y0)
        return max(0.0040, 0.0085 * (w(y) / 0.028) ** 0.6)
    ys = sorted(set([y0 + (ytip - y0) * i / 12 for i in range(13)] +
                    [c + d for c in (0.27, 0.43, 0.60) for d in (-0.012, 0.0, 0.012)]))
    rows = [(y, w(y), th(y)) for y in ys if y < ytip - 0.004]
    blade = WP.blade_solid(rows, ytip - 0.012, edge_sub=True)
    notches = {1: [(0.27, 0.010, 0.016), (0.60, 0.007, 0.014)], -1: [(0.43, 0.011, 0.018)]}

    def notch(co):
        if co.y < y0 + 0.02 or abs(co.z) < 1e-5:
            return co
        sgz = 1 if co.z > 0 else -1
        k = 1.0
        for c, depth, half in notches[sgz]:
            d = abs(co.y - c)
            if d < half:
                ww = max(w(co.y), 1e-4)
                k = min(k, 1.0 - (depth / ww) * (1.0 - d / half))
        return Vector((co.x, co.y, co.z * k))
    blade.deform(notch)
    bid = b.add(blade, "steel", name="blade", sub_regions={2: "steel_edge"}, hull_pad=True)
    b.add(WP.ridge_paint(y0 + 0.03, y0 + 0.40, w, th, 0.005), "helm_dent", name="fuller", outline=False,
          covered_by=bid)
    gz = [-0.095, -0.05, 0.0, 0.05, 0.095]
    gid = b.add(M.tube([(0, 0.084 + 0.006 * (abs(z) / 0.095) ** 2, z) for z in gz],
                       [(0.009, 0.011), (0.010, 0.012), (0.011, 0.013), (0.010, 0.012), (0.009, 0.011)], sides=6,
                       exp=3.0, up=(1, 0, 0)), "helm_dent", name="guard", hull_pad=True)
    gys, grs = (-0.085, 0.0, 0.074), (0.0165, 0.0175, 0.0160)
    grip = b.add(M.tube([(0, y, 0) for y in gys], list(grs), sides=8), "leather", name="grip", hull_pad=True)
    b.add(WP.helix(-0.076, 0.066, 0.017, 3.6, 0.008, 0.0022, r_fn=lambda y: float(np.interp(y, gys, grs)), sides=8),
          "leather_dark", name="wrap", outline=False, covered_by=grip)
    wheel = M.lathe([(0.020, -0.011), (0.028, -0.006), (0.028, 0.006), (0.020, 0.011)], sides=10,
                    cap0="fan", cap1="fan")
    wheel.rotate("y", 90).translate((0, -0.112, 0))
    b.add(wheel.auto_sharp(40), "helm_dent", name="pommel", hull_pad=True)
    obj = _finish_item(b, pal, SWORD + ("_LOD1" if lod else ""))
    socks = [{"name": "tip", "pos": (0.0, ytip, 0.0)}, {"name": "mid", "pos": (0.0, (y0 + ytip) / 2, 0.0)},
             {"name": "guard", "pos": (0.0, y0, 0.0)}]
    del gid
    return obj, socks


SHOVEL_TIP = 0.84


def build_shovel(pal: Palette, lod: int = 0) -> tuple[bpy.types.Object, list]:
    """The burial detail's spade (monsters.md: one in three carries a shovel instead of a sword): a short ash-wood
    haft gripped at its knobbed end, an iron socket with a rope binding, a dished, rounded spade blade of dull
    iron, its edge worn bright. As long as the sword (84 cm) and held the same way, so every footman clip carries
    either item. Grip at the origin, blade along +Y."""
    b = att_builder(SHOVEL, pal, lod)
    b.use("unburied.wood", "unburied.helm", "unburied.steel_edge", "unburied.rope", "unburied.helm_dent")
    y_butt, y_neck = -0.060, 0.50
    haft = M.tube([(0, y_butt, 0), (0, 0.12, 0), (0, y_neck, 0)], [0.0165, 0.0175, 0.0160], sides=8, dome0=0.006,
                  dome1=0.004)
    hid = b.add(haft, "wood", name="haft", hull_pad=True)
    knob = HB.ell((0.022, 0.020, 0.022), (0, y_butt - 0.010, 0), sides=8, rings=4)
    b.add(knob, "wood", name="knob")
    sock = M.tube([(0, y_neck - 0.03, 0), (0, y_neck + 0.06, 0)], [0.020, 0.025], sides=8, dome0=0.0, dome1=0.0)
    b.add(sock, "helm_dent", name="shovel_socket")
    ol = [(-0.095, 0.0), (0.095, 0.0), (0.102, 0.11), (0.078, 0.200), (0.0, 0.232), (-0.078, 0.200),
          (-0.102, 0.11)]
    blade = M.plate(C.ccw(ol), 0.008, bevel=0.002)
    blade.transform(Matrix.Translation((0, y_neck + 0.05, 0)))
    blade.deform(lambda co: Vector((co.x, co.y, co.z + 0.018 * (co.x / 0.10) ** 2)))
    bl = b.add(blade, "helm", name="spade", hull_pad=True)
    edge = [Vector((x, y_neck + 0.05 + y, 0.018 * (x / 0.10) ** 2)) for x, y in
            ((0.100, 0.12), (0.078, 0.198), (0.0, 0.229), (-0.078, 0.198), (-0.100, 0.12))]
    b.add(M.tube(edge, [0.0045] * 5, sides=4), "steel_edge", name="spade_edge", outline=False, covered_by=bl)
    for k in range(3):
        y = y_neck - 0.075 + k * 0.016
        ring = M.tube([(0, y - 0.006, 0), (0, y + 0.006, 0)], [0.0195, 0.0195], sides=8, cap0=None, cap1=None)
        b.add(ring, "rope", name="binding", outline=False, covered_by=hid)
    obj = _finish_item(b, pal, SHOVEL + ("_LOD1" if lod else ""))
    socks = [{"name": "blade_tip", "pos": (0.0, y_neck + 0.05 + 0.23, 0.0)},
             {"name": "blade_mid", "pos": (0.0, y_neck + 0.05 + 0.11, 0.0)}]
    return obj, socks


def looks_manifest(weapons: list[dict], default_weapon: str) -> dict:
    """Manifest ``looks``: the per-instance look table (monsters.md §2.2 "variant looks per instance"). The runtime
    keeps ``attachments`` (the default look) until it picks per instance (UE presentation, U-lane): head =
    ``head[h % 3]``, weapon by weight with an independent hash. Each look lists the attachments it swaps in."""
    return {
        "rule": "per instance: head = head[hash(instance, 'head') % len(head)]; weapon = weighted pick with "
                "hash(instance, 'weapon'); the entry's attachments are the default look (kettle helm + "
                f"{default_weapon})",
        "head": [{"id": "kettle_helm", "attachments": [{"object": HELM, "socket": "head"}]},
                 {"id": "bare_scalp", "attachments": []},
                 {"id": "hood", "attachments": [{"object": HOOD, "socket": "head"}]}],
        "weapon": weapons,
        "status": "art ships every look; per-instance selection pending in the UE presentation layer (until then "
                  "every instance wears the default attachments)",
    }


def eye_glows(rig):
    """Runtime violet pinpoint cards over both eyes (manifest ``fx.eyeGlow``; breach.pinpoint, never ember), faded
    by how much the face looks at the camera."""
    hb = rig.obj.data.bones["head"]

    def fn(cam, res):
        from kit import review
        out = []
        pb = rig.obj.pose.bones["head"]
        fwd = (rig.obj.matrix_world.to_3x3() @ pb.matrix.to_3x3() @ hb.matrix_local.to_3x3().inverted()
               @ Vector((0, -1, 0))).normalized()
        for s in ("l", "r"):
            e = next(x for x in rig.sockets if x["name"] == f"fx_eye_{s}")
            b = rig.obj.data.bones[e["bone"]]
            pbe = rig.obj.pose.bones[e["bone"]]
            wp = rig.obj.matrix_world @ (pbe.matrix @ (b.matrix_local.inverted() @ Vector(e["pos"])))
            vis = max(0.0, min(1.0, 2.0 * fwd.dot((cam.matrix_world.translation - wp).normalized()) + 0.5))
            px, py = review.project_px(cam, wp, res)
            wpp = outline.pixel_world_size(cam, wp, res[0])
            out.append(dict(center_px=(px, py), radius_px=EYE_FX["radiusCm"] / 100.0 / wpp, color=EYE_GLOW,
                            alpha=EYE_FX["alpha"] * vis, core="#F4E8FF", core_frac=0.3, core_alpha=0.45 * vis))
        return out
    return fn


EYE_FX = {"sockets": ["fx_eye_l", "fx_eye_r"], "color": EYE_GLOW, "radiusCm": 4.2, "alpha": 0.55,
          "coreColor": "#F4E8FF", "coreAlpha": 0.45, "blend": "additive", "falloff": "web glow",
          "offInDeath": True, "facingFade": "clamp(2 * dot(headFwd, toCamera) + 0.5, 0, 1)",
          "note": "breach-violet pinpoints (monsters.md §1.2: Unburied eyes #D08CFF); never ember"}


# ── ship (shared by the four members: risen_footman / _arbalest / _shieldbearer / dead_standard_bearer) ─────────
HEAD_PARTS = ("skull", "brow", "ear", "eye", "socket", "mouth", "nose", "cheek", "crack", "ash", "coif")
LOOK_KEYS = [("Idle", 0, "se"), ("Attack01", None, "se")]


def budget_record(ms, lod0_max: int, lod1_max: int) -> dict:
    """``budget`` of the manifest entry (monsters.md §2.2: LOD0 ≤ 7 k, LOD1 ≤ 3 k triangles; the standard-bearer
    7.5 k): the toon surface of the body; the baked hull is budgeted separately (≤ 0.75 × toon via padded proxies and
    no-hull trims, the goblin family's rule); held items and head attachments are their own meshes."""
    import mship
    from kit import mesh as M
    rec = {
        "spec": f"monsters.md §2.2 Unburied: LOD0 <= {lod0_max} toon triangles, LOD1 <= {lod1_max}; hull <= 0.75 x "
                "toon; held items / head looks are separate static meshes",
        "lod0": mship.tri_record(ms.body),
        "items": {it.asset: {"toon": mship.toon_tris(it.obj), "outline": M.tri_count(it.obj, 1),
                             "default": it.variant is None} for it in ms.items},
        "weightedBones": len({g.name for g in ms.body.vertex_groups}),
        "pass": mship.toon_tris(ms.body) <= lod0_max and (ms.lod1 is None or mship.toon_tris(ms.lod1) <= lod1_max),
    }
    if ms.lod1 is not None:
        rec["lod1"] = mship.tri_record(ms.lod1)
    return rec


def looks_preview(looks: list, keys=None):
    """``looks.png``: every per-instance look of the member (manifest ``looks``) at the W1 camera, native 1080p,
    each cell measured by the ink gate. ``looks`` = [(label, [Item objects shown], [Item objects hidden])]."""
    def fn(ms, rv, actions):
        import mship
        from kit import anim, review
        rig = ms.A.rig
        acts = dict(zip([c.name for c in ms.clips], actions))
        base = list(rv.meshes)
        held = [mship.shown(it) for it in mship.held(ms)]
        cells = []
        ks = keys or [("Idle", 0, "se"), ("Attack01", scene.ms_to_frame(ms.A.contact_ms), "se")]
        try:
            for label, show, hide in looks:
                for o in show:
                    o.hide_render = False
                for o in hide:
                    o.hide_render = True
                rv.meshes[:] = [m for m in base if m not in hide] + [o for o in show if o not in base]
                for clip, fr, fac in ks:
                    rv.facing(fac)
                    anim.assign(rig, acts[clip])
                    anim.set_frame(fr)
                    review.setup_render((1920, 1080), 8)
                    cam = review.game_camera()
                    zc = 0.80
                    rv.game_outline(1080, Vector((0, 0, zc)))
                    c = review.project(cam, Vector((0, 0, zc)))
                    hw = mship._crop_hw(ms)
                    hh = hw * 1920 / 1080
                    box = (c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh)
                    img, st = rv.game_shot(box, measure=True, tag=f"looks.png {label} {clip} F{fr} {fac}")
                    review.draw_text(img, 3, 3, f"{label.upper()} {clip} F{fr}")
                    review.draw_text(img, 3, img.shape[0] - 10, rv.ink_label(st), rgb=(255, 220, 140))
                    cells.append(img)
                for o in show:
                    if o not in held:
                        o.hide_render = True
                for o in hide:
                    if o in held:
                        o.hide_render = False
        finally:
            rv.meshes[:] = base
            for o in held:
                o.hide_render = False
            rv.facing("se")
        rv._save("looks.png", review.grid(cells, 2 * len(ks)))
        anim.assign(rig, acts["Idle"])
        anim.set_frame(0)
    return fn


def head_item(obj, asset: str, variant: str | None) -> "object":
    import mship
    return mship.Item(obj, asset, "head", variant=variant, category="Monsters",
                      extra={"look": "head", "attach": "bone socket 'head', identity transform (weapon-socket "
                                                       "convention)"})


# ── ship wrapper: attachment LOD1s, per-look budget totals, shared attachments listed for every member ──────────
def export_item_lod1(obj, asset: str, category: str) -> dict:
    """``<asset>_LOD1.fbx`` (item space) + its manifest ``lods[]`` record (the environment kit's static-mesh
    convention: Blender's FBX exporter cannot write an ``FbxLODGroup``)."""
    import mship
    from kit import export, mesh as M_, paths
    fbx = export.export_static_mesh(obj, f"{asset}_LOD1", category)
    return {"index": 1, "asset": f"{asset}_LOD1", "fbx": paths.rel_to_export(fbx), "screenSize": 0.05,
            "screenSizeNote": "the mobile-low mesh (static mesh MinLOD / LOD bias 1, worn with the holder's LOD1) and "
                              "the far fallback; desktop draws LOD0 at every W1 zoom",
            "import": f"same palette UVs and material slots as {asset}; import as LOD index 1 of {asset} "
                      "(StaticMeshEditorSubsystem.import_lod / Interchange LOD import)",
            "triangles": {"toon": mship.toon_tris(obj), "outline": M_.tri_count(obj, 1)}}


def look_totals(ms, lod1_tris: dict) -> dict:
    """Toon triangles of every per-instance look (manifest ``looks``: head × weapon) at LOD0 and LOD1: the body +
    its head look + held items (an item without a LOD1 counts its LOD0)."""
    import mship
    tri0 = {it.asset: mship.toon_tris(it.obj) for it in ms.items}
    body0 = mship.toon_tris(ms.body)
    body1 = mship.toon_tris(ms.lod1) if ms.lod1 is not None else body0
    lk = ms.extra.get("looks") or {}
    out = {}
    for h in lk.get("head", []):
        for w in lk.get("weapon", []):
            objs = [a["object"] for a in h["attachments"] + w["attachments"]]
            out[f"{h['id']}+{w['id']}"] = {"lod0": body0 + sum(tri0.get(o, 0) for o in objs),
                                           "lod1": body1 + sum(lod1_tris.get(o, tri0.get(o, 0)) for o in objs)}
    return out


TRUECOLOUR_SHEETS = ("material_ab", "lod1", "looks", "closeup_", "poses_close", "turnaround")


def ship_member(ms, lod1_items: dict, previews: bool = True, verify: bool = True) -> int:
    """``mship.ship`` for an Unburied member, plus (art review r2):

    * ``lod1_items`` {item asset: () → LOD1 object in item space}: each LOD1 exported beside its item and listed in
      the item's manifest ``lods``; the per-look totals (body + head look + held items, LOD0 / LOD1) recorded in
      ``budget.looks`` against the brief;
    * the shared attachments (head looks, the sword…) list **every** member that wears them: ``heldBy`` and
      ``previews`` are the union of what the manifest holds and this ship (``mship`` writes ``[holder]`` and this
      ship's previews, so the last member shipped used to own the list)."""
    import mship
    from kit import export
    man = export.Manifest()
    prev = {it.asset: dict(man.data["assets"].get(it.asset) or {}) for it in ms.items}
    lod1_tris = {}
    for it in ms.items:
        it.extra["heldBy"] = sorted(set(prev[it.asset].get("heldBy") or []) | {ms.asset})
        make = lod1_items.get(it.asset)
        if make is None:
            continue
        o1 = make()
        rec = export_item_lod1(o1, it.asset, it.category)
        it.extra["lods"] = [rec]
        lod1_tris[it.asset] = rec["triangles"]["toon"]
        me = o1.data
        bpy.data.objects.remove(o1, do_unlink=True)
        bpy.data.meshes.remove(me)
    if len(ms.body.data.materials) > 2:
        ms.extra.setdefault("materialNote", CLOTH_NOTE)
    b = ms.extra.setdefault("budget", {})
    b["looks"] = look_totals(ms, lod1_tris)
    b["looksNote"] = ("toon triangles drawn per per-instance look (manifest looks: head x weapon): body + head look + "
                      "held items; LOD1 = the body's LOD1 with each attachment's LOD1 (lods) — the 7 k / 3 k brief "
                      "(standard-bearer 7.5 k) budgets the body; the looks add the separate static meshes")
    orig_save = mship.MReview._save

    def save_truecolour(rv, name, img):
        """The close-up sheets truecolour (``pngio.write_png_native``, tiled under the budget) instead of the
        budget writer's 256-colour fallback: quantised, the few violet eye pixels snapped to the nearest bright
        palette entry — the label cream #F0ECDC (art review r2: 'cream eye discs' in material_ab / lod1 / looks)."""
        if any(name.startswith(p) for p in TRUECOLOUR_SHEETS) and \
                not any(name.startswith(p) for p in rv.native_prefixes):
            from kit import pngio
            try:
                written = pngio.write_png_native(rv.out / name, img)
            except pngio.NativePreviewError as e:
                print(f"[preview] {name}: {e} — budget writer")
            else:
                rv.written.extend(written)
                return written[0][0]
        return orig_save(rv, name, img)
    mship.MReview._save = save_truecolour
    try:
        res = mship.ship(ms, previews=previews, verify=verify)
    finally:
        mship.MReview._save = orig_save
    if previews:
        man = export.Manifest()
        for it in ms.items:
            e = man.data["assets"].get(it.asset)
            if e is None:
                continue
            old = list(prev[it.asset].get("previews") or [])
            new = list(e.get("previews") or [])
            if it.socket == "head":                      # the head looks are shown on every member's looks sheet
                new.append(f"{ms.preview_dir}/looks.png")
            e["previews"] = list(dict.fromkeys(sorted(set(old) | set(new))))
            e["heldBy"] = sorted(set(prev[it.asset].get("heldBy") or []) | set(e.get("heldBy") or []) | {ms.asset})
        man.save()
    rc = mship.main_result(res)
    print(f"[budget] looks: " + ", ".join(f"{k} {v['lod0']}/{v['lod1']}" for k, v in b["looks"].items()))
    return rc

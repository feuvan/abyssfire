"""March townsfolk kit — the A9 re-dress of the folk kit for 焚境 · The Burning March (Docs/dark-remake/chapter1.md §3,
implementation-plan.md §5 A9).

:class:`MarchFolk` is :class:`folk.Folk` with the march's identity pieces as extra part generators (``MarchLook.dress``),
built on the same body, head, garments and rig, so the eleven march looks reuse every mechanism the plains NPCs were
reviewed on (skirt chains + coat drivers, hulls, LOD1 rebuilds). Colours come only from the A1 colour script
(``march/march_palettes.py``): each look's ``folk.regions_for`` regions (registered under ``npc.march.<snake>``) and
the kit swatches ``npckit.march.<part>`` (``KIT``), re-registered under the look's prefix with identical values, so a
part never takes a new atlas index unless its colour is new (``NEW`` lists the few that are).

Dress pieces (``MarchLook.dress``):

* ``halfplate``  — the Captain's dented half-plate: layered spaulders with dents and a dark rim, vambraces on the
  forearms (the tabard covers the breastplate); ``gorget`` — two steel lames round the neck;
* ``tabard``     — the crusade tabard's front and back panels hanging from the belt to mid-thigh, a brass-edged hem,
  and ``sigil_cut`` — the church sigil cut out of its chest: a ragged window onto the plate under it;
* ``burn_scar``  — a burn scar down the right side of the neck and jaw (the near side in se and in the portrait);
* ``hood``       — a hat kind: a deep cloth hood over a short cowl (Corran), the face opening rolled;
* ``gambeson`` / ``insignia_torn`` — quilting rows on the deserter's coat; the darker unfaded patch on the left
  sleeve where the company badge was torn off, thread stubs at its edge;
* ``rope_belt``  — a cord cincture with a knot and two hanging ends instead of the leather belt (chaplain, pilgrim);
* ``ash_brow``   — the chaplain's smear of ash on the brow;
* ``name_tags``  — Hald's two sons' bronze name-tags on a cord round his neck;
* ``tally_cord`` — Vira's knotted ration-tally cord at the left hip;
* ``rolled_sleeves`` / ``gloves`` / ``apron_stains`` — Marl: sleeves rolled above the elbow, stained leather
  gloves, stains on the apron bib;
* ``feather_collar`` / ``slung_boots`` — Mag Crowe's crow-feather collar (flat vane feathers with a centre rib,
  two overlapping rows fanned out past the shoulders over a short capelet, black on black, a cool sheen band on the lit
  side's vanes only) and a pair of boots tied at the shafts on her left shoulder: one on the chest in profile (the L
  of shaft and foot, a dark outsole and heel), one on the back;
* ``relic_tray`` — the Relic Peddler's tray of reliquary wares on black cloth (bone fragments bound in cord, a finger
  bone in a brass-wire cage, wax-sealed ossuary boxes, a knuckle on a cord; no skull), tilted to the buyer;
* ``wrist_rope`` — the Bound Pilgrim's rope coils at both wrists with cut ends; ``pilgrim_cloak`` — her dusty
  cloak to mid-back, open in front, the hood bunched at the neck;
* ``earth_stains`` — Old Bern's coat caked with grave earth at the hem, splashed to the thighs;
* ``threadbare`` / ``tonsure`` — Brother Aldwin's cassock with a ragged hem and cuffs and two sewn patches; his
  thick grizzled monk's fringe;
* ``crypt_keys`` — Tamsin's big ring of crypt keys at the left hip.

Face (``MarchLook``): ``expr`` (``EXPRESSIONS``), ``jaw_k`` (a narrower jaw and chin), ``lashes``, ``lip`` (a cupid's
bow), ``nose_k``, ``crop`` (``"pixie"``: the Captain's side-swept crop, longer at the sides and nape). Old Bern's mesh
also carries his barrow's wheel, rigid on the spinning bone ``barrow_wheel_01`` (``march_props.wheel_parts``).

Body space as ``folk``: X = character left, −Y = forward, Z = up, metres; profile angles ``a`` 0 = front, +90 = the
character's left. Kit groups for ``build_kit_part``: ``halfplate``, ``gorget``, ``hood`` (= the hat group of a hood
look), plus every ``folk`` group (the chaplain's ``top`` is the cassock with its rope cincture).
"""
from __future__ import annotations

import json
import math
import sys
from dataclasses import dataclass
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "march", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bmesh  # noqa: E402
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import asset, mesh as M, outline, rig as R  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402
from kit.rig import Bind  # noqa: E402

import common as C  # noqa: E402
from common import Profile  # noqa: E402
import hero_base as HB  # noqa: E402
from hero_base import ell, profile_mesh  # noqa: E402

import folk as F  # noqa: E402
from folk import Folk, Look, lerp, smooth01, HEAD_TOP  # noqa: E402
import march_palettes as MP  # noqa: E402

INK_O = outline.INK_MIX[F.OUTLINE_CLASS]


def _kit_regions() -> dict:
    """``npckit.march.<part>`` swatches (A1 ``NPC_KIT``) as Regions with the NPC ink mix (the registered values)."""
    return {part: Region(hx, s, l, e, INK_O) for part, hx, s, l, e, _note in MP.NPC_KIT}


KIT = _kit_regions()
# the few new colours of the A9 re-dress (appended to the NPC registry; everything else reuses an A1 / plains value):
# the dents and rim of the half-plate are its own shade tones, the charred stump of Hald's anvil (march_props)
NEW = {
    "halfplate_line": KIT["halfplate"].shade_region(.30, .12),
}


@dataclass
class MarchLook(Look):
    """``folk.Look`` + the march dress pieces (module docstring) + a face (``EXPRESSIONS``: the plains folk all smile;
    the march's survivors are tired, grim, wary)."""
    dress: tuple = ()
    expr: str = "kind"
    jaw_k: float = 1.0            # < 1 narrows the jaw and chin (a feminine taper inside the shared head)
    lashes: bool = False          # an upper-lid lash line with a small flick at the outer corner
    lip: bool = False             # a defined upper lip (a cupid's bow above the mouth line)
    nose_k: float = 1.0           # nose size (the Captain's a touch finer)
    crop: str = "back"            # the cropped cut: "back" (swept straight back) | "pixie" (a side-swept fringe,
                                  # longer at the sides and the nape — not a bowl)


# face expressions: brow line (inner height, outer height, arch) in head units (folk: 2.45, 2.0, .25 — the kind,
# slightly worried plains brow), mouth (centre h, corner rise, half-angle deg, left-corner extra rise), cheek blush
EXPRESSIONS = {
    "kind": dict(brow=(2.45, 2.0, 0.25), mouth=(-3.87, 0.44, 19.0, 0.0), blush=True),
    "flat": dict(brow=(2.30, 2.12, 0.18), mouth=(-3.66, 0.04, 16.0, 0.0), blush=False),
    "grim": dict(brow=(2.02, 2.34, 0.16), mouth=(-3.56, -0.20, 16.0, 0.0), blush=False),
    "weary": dict(brow=(2.72, 1.98, 0.10), mouth=(-3.64, -0.06, 15.0, 0.0), blush=False),
    "haunted": dict(brow=(2.62, 2.02, 0.12), mouth=(-3.58, -0.14, 15.0, 0.0), blush=False),
    "fear": dict(brow=(2.95, 2.10, 0.06), mouth=(-3.72, -0.04, 10.0, 0.0), blush=False),
    "sly": dict(brow=(2.20, 2.20, 0.22), mouth=(-3.70, 0.06, 17.0, 0.30), blush=False),
    # the Captain: the grim line kept (inner end low, outer high) on a thinner, higher, arched brow
    "grim_f": dict(brow=(2.30, 2.64, 0.34), mouth=(-3.62, -0.14, 14.0, 0.0), blush=False),
}


class MarchFolk(Folk):
    """``folk.Folk`` with the march dress pieces (module docstring). Same rig, same garments."""

    def __init__(self, look: MarchLook, asset_name: str, prefix: str):
        super().__init__(look, asset_name, prefix)
        self.dress = set(getattr(look, "dress", ()) or ())
        if look.hair and look.hair[1] == "crop":
            self.hair_thick = 0.30
        jk = getattr(look, "jaw_k", 1.0)
        if jk != 1.0:
            # the jaw and chin rings narrowed (side and depth), the cheekbones kept: a tapering, narrower chin
            rings = []
            for h, a, b_, f in F.HEAD_RINGS:
                k = 1.0 if h > -1.0 else lerp(1.0, jk, smooth01((-1.0 - h) / 3.8)) * (0.94 if h < -6.0 else 1.0)
                ka = 1.0 if h > -3.0 else lerp(1.0, 0.5 + 0.5 * jk, smooth01((-3.0 - h) / 3.3))
                rings.append((h, a * ka, b_ * k, f))
            hs = (1.03, 0.96)
            self.head = Profile([(self.hc + h * self.HK, b_ * self.HK * hs[0], a * self.HK * hs[1], -f * self.HK)
                                 for h, a, b_, f in rings], exp=2.0)

    def spec(self):
        """``folk.Folk.spec`` + Old Bern's barrow bones: ``barrow_01`` (head at the wheel's axle, pointing back at
        him: the barrow's pitch, the socket ``barrow``) and its child ``barrow_wheel_01`` (along the axle: the
        wheel's spin), both unweighted but for the wheel built into his mesh."""
        s = super().spec()
        if self.look.item == "barrow":
            import march_props as MPR
            ax = MPR.barrow_axle()
            s.chains = list(s.chains) + [("barrow", "root", [ax, ax + Vector((0.0, 0.25, 0.0))]),
                                         ("barrow_wheel", "barrow_01", [ax, ax + Vector((0.10, 0.0, 0.0))])]
        return s

    # LOD1 drops the dress pieces' small detail too (the relic tray's knobs / lids / corks, stains, the tag cord)
    # (a name must not end in "l" / "r": the skip test strips a side suffix with rstrip("_lr"))
    LOD1_SKIP = Folk.LOD1_SKIP + ("relic_knob", "relic_lid", "relic_cork", "relic_wrap", "relic_wire",
                                 "relic_wax", "lash", "lip_shade", "sigil_thread", "sigil_scratch", "sigil_dent",
                                 "feather_rib", "fray", "patch_stitch", "stain", "tag_cord", "boot_lace",
                                 "burn_scar_neck", "burn_scar_skin", "burn_scar_fleck")

    def has(self, *names: str) -> bool:
        return any(n in self.dress for n in names)

    def holds(self, side: str) -> str:
        # Bern grips the barrow's two handles: both hands are fists
        if self.look.item == "barrow":
            return "barrow"
        return super().holds(side)

    # ── regions ─────────────────────────────────────────────────────────────────────────────────────────
    def dress_regions(self) -> dict:
        """Extra regions this look's dress pieces paint with (kit swatches re-registered under the look's prefix:
        same values → same atlas index)."""
        r = {}
        k = KIT
        if self.has("halfplate", "sigil_cut"):
            r.update(halfplate=k["halfplate"], halfplate_dark=k["halfplate_dark"], halfplate_line=NEW["halfplate_line"])
        if self.has("gorget"):
            r.update(gorget=k["gorget"], halfplate_dark=k["halfplate_dark"])
        if self.has("burn_scar"):
            r["burn_scar"] = k["burn_scar"]
        if self.has("ash_brow"):
            r["ash_brow"] = k["ash_brow"]
        if self.has("name_tags", "tally_cord", "wrist_rope", "slung_boots", "relic_tray"):
            r["cord"] = k["cord"]
        if self.has("name_tags"):
            r["brass"] = k["crypt_keys"]
        if self.has("gloves", "apron_stains", "earth_stains"):
            r["stain"] = k["stain"]
        if self.has("slung_boots"):
            r["slung_boots"] = k["slung_boots"]
            r["slung_boots_line"] = Region(k["slung_boots"].hex).shade_region(.3, .1)      # = folk boot_line
            r["boot_tan"] = F.BEDROLL          # worn tan leather (an existing swatch): apart from the purse's brown
        if self.has("relic_tray"):
            r.update(tray=k["tray"], relic=k["relic"], tray_dark=Region("#7A5230", .42, .30).shade_region(.30, .12),
                     lining=k["lantern_frame"], stain=k["stain"])
        if self.has("tonsure"):
            r["fringe"] = k["ash_brow"]              # the ash-grey of his brow smear (an existing swatch)
        if self.has("crypt_keys"):
            r["brass"] = k["crypt_keys"]
        if self.look.item == "barrow":
            r.update(wheel_wood=k["barrow_wood"], wheel_dark=Region("#7A5230", .42, .30).shade_region(.30, .12),
                     wheel_iron=k["halfplate_dark"])
        if self.has("feather_collar"):
            r["feather_sheen"] = F.METAL_DARK
            r["feather_rib"] = F.METAL_DARK
        return outline.ink_regions(r, F.OUTLINE_CLASS)

    # ── build (folk.Folk.build + the dress pieces) ──────────────────────────────────────────────────────
    def build(self, pal: Palette, rig: R.Rig, lod: int = 0, lod_ratio: float | None = None) -> bpy.types.Object:
        self.rig = rig
        self.lod = lod
        if lod:
            b = M.Builder(self.asset, pal, prefix=self.prefix, lod_ratio=lod_ratio or self.LOD1_RATIO,
                          lod_skip=lambda n: n.rstrip("_lr").rstrip("0123456789").startswith(self.LOD1_SKIP))
        else:
            b = M.Builder(self.asset, pal, prefix=self.prefix)
        b.regions(**F.regions_for(self.look))
        b.regions(**self.dress_regions())
        self.b = b
        ids = self.add_all(b, rig)
        names = [p["name"] for p in b.parts]
        body = asset.finish_mesh(b, pal, F.OUTLINE_CLASS, rig=rig, grounding_height=self.D.height,
                                 name=f"{self.asset}_LOD{lod}" if lod else f"{self.asset}_Mesh")
        body["af_parts"] = json.dumps(names)
        body["af_lod"] = lod
        del ids
        return body

    def add_all(self, b: M.Builder, rig: R.Rig) -> dict:
        ids = {}
        ids["skull"] = self.add_head(b)
        self.add_hair(b, ids["skull"])
        self.add_beard(b, ids["skull"])
        self.add_hat(b, ids["skull"])
        if self.look.specs:
            self.add_specs(b, ids["skull"])
        self.add_face_dress(b, ids["skull"])
        ids["torso"] = self.add_torso(b)
        if self.skirted:
            ids["skirt"] = self.add_skirt(b)
        self.add_belt(b, ids)
        if self.look.apron:
            self.add_apron(b)
        if self.look.mantle:
            if self.look.scarf:
                self.add_scarf(b)
            elif self.has("pilgrim_cloak"):
                self.add_pilgrim_cloak(b)
            elif not self.has("feather_collar"):
                self.add_mantle(b)
        if self.look.pack:
            self.add_pack(b)
        if self.look.keyring:
            self.add_keyring(b, ids)
        for side in ("l", "r"):
            self.add_arm(b, rig, side)
            self.add_leg(b, rig, side)
        self.add_dress(b, rig, ids)
        return ids

    KIT_GROUPS = Folk.KIT_GROUPS + ("halfplate", "gorget", "hood", "collar", "tray")

    def build_kit_part(self, pal: Palette, rig: R.Rig, group: str, name: str) -> bpy.types.Object | None:
        """``folk.Folk.build_kit_part`` + the march groups: ``halfplate`` (spaulders + vambraces), ``gorget``,
        ``hood`` (the hat group of a hood look), ``collar`` (the feather collar), ``tray`` (the relic tray)."""
        if group not in ("halfplate", "gorget", "hood", "collar", "tray", "top"):
            return super().build_kit_part(pal, rig, group, name)
        self.rig = rig
        self.lod = 0
        b = M.Builder(name, pal, prefix=self.prefix)
        b.regions(**F.regions_for(self.look))
        b.regions(**self.dress_regions())
        self.b = b
        if group == "halfplate":
            for side in ("l", "r"):
                self.add_spaulder(b, rig, side)
                self.add_vambrace(b, rig, side, None)
        elif group == "gorget":
            self.add_gorget(b)
        elif group == "hood":
            self.add_hat(b, None)
        elif group == "collar":
            self.add_feather_collar(b)
        elif group == "tray":
            self.add_relic_tray(b)
        elif group == "top":
            ids = {"torso": self.add_torso(b)}
            if self.skirted:
                ids["skirt"] = self.add_skirt(b)
            self.add_belt(b, ids)
            for side in ("l", "r"):
                self.add_arm(b, rig, side, hand=False)
        if not b.parts:
            return None
        names = [p["name"] for p in b.parts]
        obj = asset.finish_mesh(b, pal, F.OUTLINE_CLASS, rig=rig, grounding_height=self.D.height, name=name)
        obj["af_parts"] = json.dumps(names)
        obj["af_kit_group"] = group
        return obj

    # ── helpers ─────────────────────────────────────────────────────────────────────────────────────────
    @staticmethod
    def part_id(b: M.Builder, name: str) -> int | None:
        """Id of the last part called ``name`` that carries a padded hull (``covered_by`` target)."""
        for i in range(len(b.parts) - 1, -1, -1):
            if b.parts[i]["name"] == name and i in b._pending:
                return i
        return None

    def _exact(self, b, part, *a, **kw) -> int:
        return (self._add_exact if getattr(self, "lod", 0) else (lambda bb, *a_, **kw_: bb.add(*a_, **kw_)))(
            b, part, *a, **kw)

    def head_patch(self, h0: float, h1: float, a0: float, a1: float, lift: float, rows: int = 4, cols: int = 6,
                   edge=None) -> M.Part:
        """A thin painted patch on the head surface (head units h, degrees a), ``lift`` head units proud;
        ``edge(u, v) → 0..1`` shrinks the patch toward its centre (ragged smears)."""
        outer, inner = [], []
        hc, ac = (h0 + h1) / 2, (a0 + a1) / 2
        for r in range(rows + 1):
            v = r / rows
            ro, ri = [], []
            for c in range(cols + 1):
                u = c / cols
                k = edge(u, v) if edge else 1.0
                h = lerp(hc, lerp(h0, h1, v), k)
                a = lerp(ac, lerp(a0, a1, u), k)
                ro.append(self.hp(h, a, lift + 0.05))
                ri.append(self.hp(h, a, lift - 0.06))
            outer.append(ro)
            inner.append(ri)
        return C.thick_patch(outer, inner)

    # ── face: the march expressions (folk.add_face with the brow / mouth / blush of ``EXPRESSIONS``) ─────────
    def add_face(self, b: M.Builder, sid: int) -> None:
        lk = self.look
        ex = EXPRESSIONS[getattr(lk, "expr", "kind")]
        HK = self.HK
        bind = Bind.rigid("head")
        bi, bo, barch = ex["brow"]
        for sg in (1.0, -1.0):
            a = sg * F.EYE_A
            fr = self._eye_frame(F.EYE_H, a, -0.08)
            es, er = (12, 6) if not getattr(self, "lod", 0) else (10, 4)
            white = ell((0.92 * HK, 0.44 * HK, 0.86 * HK), (0, 0, 0), sides=es, rings=er)
            self._add_exact(b, white.transform(fr), "eye_white", bind, name="eye_white", outline=False,
                            covered_by=sid)
            px = sg * 0.10 * HK
            pup = ell((0.50 * HK, 0.22 * HK, 0.62 * HK), (px, 0.30 * HK, -0.04 * HK), sides=es, rings=er)
            self._add_exact(b, pup.transform(fr), "pupil", bind, name="pupil", outline=False, covered_by=sid)
            hi = ell((0.17 * HK, 0.08 * HK, 0.17 * HK), (px - sg * 0.17 * HK, 0.48 * HK, 0.22 * HK), sides=8, rings=4)
            b.add(hi.transform(fr), "eye_hi", bind, name="eye_hi", outline=False, covered_by=sid)
            if lk.lids > 0:
                lid = ell((1.0 * HK, 0.50 * HK, 0.62 * HK), (0, 0.02 * HK, (0.86 - lk.lids * 1.3 + 0.62) * HK),
                          sides=12, rings=6)
                b.add(lid.transform(fr), "skin", bind, name="lid", outline=False, covered_by=sid)
            bk = lk.brow_k
            raise_ = 0.22 if (getattr(lk, "expr", "") == "sly" and sg > 0) else 0.0
            pts, rad = [], []
            for i in range(7):
                t = i / 6
                aa = sg * lerp(8.0, 37.0, t)
                hh = lerp(bi, bo, t) + barch * math.sin(math.pi * t) + raise_
                pts.append(self.hp(hh, aa, 0.22 + 0.06 * bk))
                w = (0.36 + 0.14 * math.sin(math.pi * min(1.0, t * 1.3))) * bk
                rad.append((0.17 * HK * bk ** 0.5, w * HK * (1 - 0.35 * t)))
            brow = M.sweep(pts, M.superellipse(1, 1, 8, 2.2), scales=rad, up=tuple(self.hn(2.3, sg * 20.0)),
                           dome0=0.1 * HK, dome1=0.1 * HK)
            self._add_exact(b, brow, "brow", bind, name="brow", outline=False, covered_by=sid)
            if getattr(lk, "lashes", False):
                # the upper lash line along the top of the eye white, thickening outward, a small flick up at the
                # outer corner (the feminine read at the portrait / closeup scale; sub-pixel in game)
                pts_l, rad_l = [], []
                for i in range(7):
                    th = math.radians(lerp(160.0, 18.0, i / 6))
                    pts_l.append(fr @ Vector((0.99 * math.cos(th) * HK, 0.16 * HK, (0.90 * math.sin(th) + 0.02) * HK)))
                    rad_l.append((0.10 * HK, (0.10 + 0.10 * i / 6) * HK))
                if abs(pts_l[-1].x) < abs(pts_l[0].x):          # outer corner last
                    pts_l.reverse()
                    rad_l.reverse()
                o = pts_l[-1]
                pts_l.append(o + (o - pts_l[-2]).normalized() * 0.28 * HK + Vector((0, 0, 0.22 * HK)))
                rad_l.append((0.07 * HK, 0.08 * HK))
                lash = M.sweep(pts_l, M.superellipse(1, 1, 4, 2.0), scales=rad_l, up=tuple(self.hn(F.EYE_H, a)),
                               dome0=0.04 * HK, dome1=0.03 * HK)
                self._add_exact(b, lash, "pupil", bind, name="lash", outline=False, covered_by=sid)
            if not (lk.hat and lk.hat[0] == "hood"):            # ears under a hood are not built
                fe = self._eye_frame(F.EAR_H, sg * F.EAR_A, 0.30)
                fe = fe @ Matrix.Rotation(math.radians(-sg * 12.0), 4, "Y")
                ear = ell((1.15 * HK, 0.45 * HK, 1.75 * HK), (0, 0, 0), sides=12, rings=8)
                b.add(ear.transform(fe), "skin", bind, name="ear", outline=False, covered_by=sid)
                bowl = ell((0.70 * HK, 0.22 * HK, 1.12 * HK), (sg * 0.10 * HK, 0.28 * HK, -0.05 * HK), sides=10,
                           rings=6)
                b.add(bowl.transform(fe), "skin_shade", bind, name="ear_bowl", outline=False, covered_by=sid)
            if ex["blush"]:
                fb = self._eye_frame(-2.0, sg * 37.0, -0.20)
                blush = ell((1.25 * HK, 0.24 * HK, 0.78 * HK), (0, 0, 0), sides=10, rings=4)
                b.add(blush.transform(fb), "blush", bind, name="blush", outline=False, covered_by=sid)
        path = [self.hp(-0.35, 0.0, -0.15), self.hp(-1.0, 0.0, 0.55), self.hp(-1.75, 0.0, 1.05),
                self.hp(-2.25, 0.0, 0.85), self.hp(-2.55, 0.0, 0.25)]
        nk = getattr(lk, "nose_k", 1.0)
        nose = M.tube(path, [0.50 * HK * nk, 0.62 * HK * nk, 0.86 * HK * nk, 0.74 * HK * nk, 0.42 * HK * nk], sides=12,
                      up=tuple(self.hn(-1.5, 0.0).cross(Vector((1, 0, 0)))), dome0=0.0, dome1=0.25 * HK)
        b.add(nose, "skin", bind, name="nose", outline=False, covered_by=sid)
        for sg in (1.0, -1.0):
            wing = ell((0.55 * HK, 0.45 * HK, 0.48 * HK), self.hp(-2.15, sg * 9.0, 0.35), sides=10, rings=6)
            b.add(wing, "skin", bind, name="nose_wing", outline=False, covered_by=sid)
        if not (lk.beard and lk.beard[1] in ("full", "long", "braided")):
            mc, rise, half, left = ex["mouth"]
            pts = []
            for i in range(9):
                t = -1 + 2 * i / 8
                hh = mc + rise * t * t + (left * t * t if t > 0 else 0.0)
                pts.append(self.hp(hh, half * t, 0.06))
            mouth = M.sweep(pts, M.superellipse(1, 1, 6, 2.0), scales=[(0.16 * HK, 0.22 * HK)] * len(pts),
                            up=tuple(self.hn(-3.6, 0.0)), dome0=0.05 * HK, dome1=0.05 * HK)
            self._add_exact(b, mouth, "mouth", bind, name="mouth", outline=False, covered_by=sid)
            if getattr(lk, "lip", False):
                # a defined upper lip: a cupid's bow over the mouth line (peaks either side of the philtrum, a dip
                # at the centre), in the rosier skin tone; a short shadow under the lower lip
                up_pts, up_rad = [], []
                for i in range(11):
                    t = -1 + 2 * i / 10
                    bow = 0.30 * math.exp(-((abs(t) - 0.36) / 0.20) ** 2) - 0.10 * math.exp(-(t / 0.12) ** 2)
                    hh = mc + rise * t * t + 0.22 + bow * (1 - abs(t)) + 0.05
                    up_pts.append(self.hp(hh, half * 0.92 * t, 0.05))
                    up_rad.append((0.10 * HK, (0.20 - 0.12 * abs(t) ** 2) * HK))
                lip = M.sweep(up_pts, M.superellipse(1, 1, 6, 2.0), scales=up_rad, up=tuple(self.hn(-3.4, 0.0)),
                              dome0=0.04 * HK, dome1=0.04 * HK)
                self._add_exact(b, lip, "blush", bind, name="lip", outline=False, covered_by=sid)

    # ── hair: the cropped style (thin, swept back, the forehead bare) ───────────────────────────────────────
    def add_hair(self, b: M.Builder, sid: int) -> None:
        lk = self.look
        if lk.hat and lk.hat[0] == "hood":
            return              # hidden under the hood (its face opening starts above the hairline)
        if lk.hair and lk.hair[1] == "bald" and self.has("tonsure"):
            return self.add_tonsure(b, sid)
        if lk.hair and lk.hair[1] == "crop" and getattr(lk, "crop", "back") == "pixie":
            return self.add_pixie(b, sid)
        if not lk.hair or lk.hair[1] != "crop":
            return super().add_hair(b, sid)
        bind = Bind.rigid("head")
        HK = self.HK
        th = 0.30

        def lo(a):
            d = abs(((a + 180) % 360) - 180)
            front = 0.65 * (1.0 - smooth01((d - 22.0) / 45.0)) + 0.55 * math.exp(-((d - 36.0) / 11.0) ** 2)
            return self.hairline(a) + front

        def thick(s, a):
            d = abs(((a + 180) % 360) - 180)
            full = th * (1.0 + 0.35 * smooth01((d - 90.0) / 80.0)) * (0.1 + 0.9 * smooth01(s / 0.45)) + 0.03
            face = 1.0 - smooth01((d - 95.0) / 40.0)
            return lerp(lerp(full, -0.06, face), full, smooth01(s / 0.25))
        lod = getattr(self, "lod", 0)
        top = F.HEAD_TOP
        if lk.hat and lk.hat[0] == "hood":
            top = F.HEAD_TOP
        cap = self.head_cap(lo, thick, top_h=top, cols=22 if lod else 34, rows=4 if lod else 6, inner=False)
        add = self._add_exact if lod else (lambda bb, *a_, **kw: bb.add(*a_, **kw))
        if sid is None:
            proxy = self.head_cap(lo, thick, top_h=top, cols=24, rows=4)
            cid = add(b, cap, "hair", bind, name="hair_cap", hull=proxy, hull_pad=True)
        else:
            add(b, cap, "hair", bind, name="hair_cap", outline=False, covered_by=sid)
            cid = sid
        # a few combed strokes swept straight back
        for a0 in (-40.0, -14.0, 12.0, 38.0):
            pts = []
            for i in range(8):
                t = i / 7
                h = lerp(lo(a0) + 0.35, F.HEAD_TOP - 0.6, t)
                a = a0 * (1.0 + 0.6 * t)
                pts.append(self.hp(h, a, thick(lerp(0.2, 0.95, t), a) + 0.02))
            strand = M.sweep(pts, M.superellipse(1, 1, 4, 2.0),
                             scales=[(0.06 * HK, 0.18 * HK * (1 - 0.6 * abs(2 * i / 7 - 1))) for i in range(8)],
                             up=tuple(self.hn(4.0, a0)), dome0=0.0, dome1=0.0)
            b.add(strand, "hair_line", bind, name="strand", outline=False, covered_by=cid)

    def add_pixie(self, b: M.Builder, sid: int) -> None:
        """The Captain's crop (review: the swept-back cap read as a man's bowl cut): a side-swept fringe falling
        lower on her right, pointed locks in front of the ears down to the cheekbone, the cut longer over the ears
        and at the nape (a tapered point), a feathered lower edge; more volume at the crown."""
        bind = Bind.rigid("head")
        HK = self.HK
        th = 0.36

        def lo(a):
            d = abs(((a + 180) % 360) - 180)
            notch = 0.22 * abs(math.sin(math.radians(a) * 9.0))           # feathered, not a hard line
            if d <= 52.0:
                # the fringe sweeps from high on her left (+a) to low over the right temple (−a)
                f = 3.30 + 0.62 * (a / 52.0)
                return lerp(f, self.hairline(a) - 0.2, smooth01((d - 40.0) / 12.0)) - notch
            if d <= 100.0:
                # over and in front of the ears: down to the cheekbone in front, the ear top covered
                k = smooth01((d - 52.0) / 14.0)
                side = lerp(self.hairline(a) - 0.2, -0.6, k)
                return side - 0.9 * math.exp(-((d - 64.0) / 8.0) ** 2) - notch * 0.6
            # the back: longer at the nape, tapering to a point at the centre
            return lerp(-0.6, -5.4, smooth01((d - 100.0) / 45.0)) - 0.9 * smooth01((d - 150.0) / 30.0) - notch * 0.5

        def thick(s, a):
            d = abs(((a + 180) % 360) - 180)
            full = th * (1.0 + 0.45 * smooth01((d - 80.0) / 90.0)) * (0.15 + 0.85 * smooth01(s / 0.40)) + 0.04
            full *= 1.0 + 0.25 * math.sin(math.pi * s)                    # volume at the crown
            # the sides fall over the tops of the ears (the bare ears read boyish)
            full += 0.55 * math.exp(-((d - 84.0) / 13.0) ** 2) * (1.0 - smooth01(s / 0.55))
            face = 1.0 - smooth01((d - 95.0) / 40.0)
            return lerp(lerp(full, 0.02, face * 0.6), full, smooth01(s / 0.22))
        lod = getattr(self, "lod", 0)
        cap = self.head_cap(lo, thick, top_h=F.HEAD_TOP, cols=24 if lod else 32, rows=4 if lod else 5, inner=False)
        add = self._add_exact if lod else (lambda bb, *a_, **kw: bb.add(*a_, **kw))
        if sid is None:
            proxy = self.head_cap(lo, thick, top_h=F.HEAD_TOP, cols=24, rows=4)
            cid = add(b, cap, "hair", bind, name="hair_cap", hull=proxy, hull_pad=True)
        else:
            add(b, cap, "hair", bind, name="hair_cap", outline=False, covered_by=sid)
            cid = sid
        # strands: the fringe swept across the brow toward her right, the rest combed back over the crown
        for a0, sweep in ((30.0, -1.0), (6.0, -1.0), (-40.0, 0.0), (58.0, 0.3)):
            pts = []
            for i in range(6):
                t = i / 5
                if sweep < 0:
                    h = lerp(F.HEAD_TOP - 2.2, lo(a0 - 30.0 * t) + 0.35, t)
                    a = a0 - 34.0 * t
                else:
                    h = lerp(lo(a0) + 0.4, F.HEAD_TOP - 0.8, t)
                    a = a0 * (1.0 + sweep * t)
                pts.append(self.hp(h, a, thick(lerp(0.9, 0.1, t) if sweep < 0 else lerp(0.15, 0.9, t), a) + 0.02))
            b.add(M.sweep(pts, M.superellipse(1, 1, 4, 2.0),
                          scales=[(0.06 * HK, 0.20 * HK * (1 - 0.6 * abs(2 * i / 5 - 1))) for i in range(6)],
                          up=tuple(self.hn(4.0, a0)), dome0=0.0, dome1=0.0), "hair_line", bind, name="strand",
                  outline=False, covered_by=cid)

    def add_tonsure(self, b: M.Builder, sid: int) -> None:
        """Brother Aldwin's monk's fringe (review: barely read from the front): a thick grizzled ring from temple to
        temple round the back of the head, its top edge high enough to show over the ears from the front; the base
        in the ash grey of his brow smear (an existing swatch: the plains brown at that size read as a shadow), the
        brown of his hair in combed strands through it."""
        HK = self.HK
        bind = Bind.rigid("head")

        def lo(a):
            d = abs(((a + 180) % 360) - 180)
            return -1.0 - 1.2 * smooth01((d - 80) / 70) - 0.35 * abs(math.sin(math.radians(a) * 8.0))

        def hi_(a):
            d = abs(((a + 180) % 360) - 180)
            return 2.6 + 1.0 * smooth01((d - 70) / 80)
        a0, a1 = 52.0, 308.0
        cols, rows = (20, 4) if getattr(self, "lod", 0) else (30, 5)
        bm = bmesh.new()
        angs = [a0 + (a1 - a0) * i / cols for i in range(cols + 1)]
        vo, vi = [], []
        for r in range(rows + 1):
            s = r / rows
            ro, ri = [], []
            for a in angs:
                h = lerp(lo(a), hi_(a), s)
                t = math.sin(math.pi * s) ** 0.7
                end = smooth01(min(a - a0, a1 - a) / 16.0)
                clump = 1.0 + 0.22 * math.sin(math.radians(a) * 11.0)
                ro.append(bm.verts.new(tuple(self.hp(h, a, (0.20 + 0.95 * t) * end * clump + 0.07))))
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
        fid = b.add(M.Part(bm).smooth(True), "fringe", bind, name="hair_fringe", hull_pad=True)
        for k, a in enumerate(np.linspace(a0 + 10.0, a1 - 10.0, 10)):
            t0 = 0.25 + 0.2 * (k % 3) / 2
            pts = [self.hp(lerp(lo(a), hi_(a), lerp(t0, 0.85, u)), a + 3.0 * u,
                           (0.20 + 0.95 * math.sin(math.pi * lerp(t0, 0.85, u)) ** 0.7) * 1.0 + 0.09)
                   for u in np.linspace(0, 1, 4)]
            b.add(M.tube(pts, [0.11 * HK] * 4, sides=3, dome0=0.0, dome1=0.0), "hair", bind, name="strand",
                  outline=False, covered_by=fid)

    # ── face dress: ash brow, burn scar ─────────────────────────────────────────────────────────────────
    def add_face_dress(self, b: M.Builder, sid: int | None) -> None:
        bind = Bind.rigid("head")
        if self.has("ash_brow"):
            # a thumb's smear of ash high on the brow: overlapping blots, heaviest left of centre, thinning to the
            # right (not a band: a band across the forehead read as a unibrow)
            kw = {"covered_by": sid} if sid is not None else {}
            HK = self.HK
            for k, (h, ang, rx, rz, tilt) in enumerate(((4.15, 3.0, 1.45, 0.62, -8.0), (4.0, 15.0, 0.75, 0.42, 14.0))):
                # flattened ovals lying on the brow (frame +Y = the head's normal): a soft thumb-smear
                fr = self._eye_frame(h, ang, 0.03 + 0.015 * k) @ Matrix.Rotation(math.radians(tilt), 4, "Y")
                blot = ell((rx * HK, 0.05 * HK, rz * HK), (0, 0, 0), sides=14, rings=4)
                b.add(blot.transform(fr), "ash_brow", bind, name="ash_brow", outline=False, **kw)
        if self.has("burn_scar"):
            # puckered burn from under the right ear down the jaw onto the neck (the near side in se and in the
            # portrait): a ragged streak, narrow at the cheek and widening down the jaw (a round patch read as a
            # blush), mottled with three islands of whole skin
            def edge(u, v):
                # v = 0 at the jaw, 1 at the cheek; a rough, mottled boundary (review: a smooth edge on the jaw read
                # partly as a shadow): two noise octaves, deeper bites toward the cheek
                taper = 0.55 + 0.45 * (1.0 - v)
                n1 = math.cos(6.0 * u + 1.3 * v) * math.sin(5.0 * v + 0.7)
                n2 = math.sin(17.0 * u + 3.1) * math.cos(13.0 * v + 1.7)
                return taper * (0.70 + 0.20 * n1 + 0.14 * n2)
            kw = {"covered_by": sid} if sid is not None else {}
            rows, cols = (7, 6) if getattr(self, "lod", 0) else (8, 7)
            b.add(self.head_patch(-6.4, -1.2, -88.0, -46.0, 0.05, rows=rows, cols=cols, edge=edge), "burn_scar", bind,
                  name="burn_scar", outline=False, **kw)
            for h, a, rh, ra in ((-4.9, -70.0, 0.34, 4.0), (-3.4, -62.0, 0.26, 3.2), (-5.6, -58.0, 0.22, 3.0)):
                b.add(self.head_patch(h - rh, h + rh, a - ra, a + ra, 0.075, rows=2, cols=3,
                                      edge=lambda u, v: 0.6 + 0.4 * math.sin(math.pi * u) * math.sin(math.pi * v)),
                      "skin", bind, name="burn_scar_skin", outline=False, **kw)
            # satellite flecks along the boundary: the edge breaks up into spots instead of a line
            for h, a, rh, ra in ((-1.0, -55.0, 0.22, 2.4), (-1.6, -84.0, 0.20, 2.2), (-6.6, -60.0, 0.20, 2.6)):
                b.add(self.head_patch(h - rh, h + rh, a - ra, a + ra, 0.05, rows=2, cols=3,
                                      edge=lambda u, v: 0.55 + 0.45 * math.sin(math.pi * u) * math.sin(math.pi * v)),
                      "burn_scar", bind, name="burn_scar_fleck", outline=False, **kw)
            # the scar continues down the neck to the gorget
            J = self.rig.joints
            n0 = J["neck"] + Vector((0, 0.006, -0.03))
            n1 = Vector((0, 0.004, self.hz(-3.2)))
            r0 = 2.35 * self.g ** 0.8 * self.U
            pts, outs = [], []
            for t in np.linspace(0.15, 0.95, 5):
                c = n0.lerp(n1, t)
                a = math.radians(-72.0)
                d = Vector((math.sin(a), -math.cos(a), 0.0))
                pts.append(c + d * (r0 * 0.97 + 0.0012))
                outs.append(d)
            scar = M.sweep(pts, M.superellipse(1, 1, 6, 2.0), scales=[(0.0016, 0.012), (0.0016, 0.015),
                                                                       (0.0016, 0.013), (0.0016, 0.011),
                                                                       (0.0016, 0.008)],
                           up=tuple(outs[2]), dome0=0.004, dome1=0.004)
            b.add(scar, "burn_scar", Bind.blend("spine_03", "neck_01", "head", falloff=3.0), name="burn_scar_neck",
                  outline=False)

    # ── torso: a tabard has no placket (the cut sigil is the chest's accent); its trim stays at the hem ─────
    def add_torso(self, b: M.Builder) -> int:
        if not self.has("tabard"):
            return super().add_torso(b)
        lk = self.look
        top = lk.top
        lk.top = (top[0], top[1], None)
        try:
            tid = super().add_torso(b)
        finally:
            lk.top = top
        if top[2]:
            b.add(C.surface_band(self.torso, self.th(-1.9), 0.8 * self.U, 0.0022, segs=40, out0=-0.001), "trim",
                  self.spine_bind(), name="tunic_hem", outline=False, covered_by=tid)
        return tid

    # ── hats: the hood ──────────────────────────────────────────────────────────────────────────────────
    HOOD_FRONT_H = 3.25          # face-opening top (head units): over the hairline (3.6), the brows (2.45) clear
    HOOD_SIDE_H = -4.8           # cheek / jaw line at the opening's sides
    HOOD_BACK_H = -6.0           # round the back of the head into the cowl

    def hood_lo(self, a: float) -> float:
        d = abs(((a + 180.0) % 360.0) - 180.0)
        if d <= 38.0:
            return self.HOOD_FRONT_H
        if d <= 78.0:
            return lerp(self.HOOD_FRONT_H, self.HOOD_SIDE_H, smooth01((d - 38.0) / 40.0))
        return lerp(self.HOOD_SIDE_H, self.HOOD_BACK_H, smooth01((d - 78.0) / 50.0))

    def hood_thick(self, s: float, a: float) -> float:
        d = abs(((a + 180.0) % 360.0) - 180.0)
        base = self.hair_thick + 0.55
        back = 0.7 * smooth01((d - 70.0) / 100.0)                 # the cloth bunches behind the head
        crown = 0.25 * math.sin(math.pi * s)
        tip = 0.9 * math.exp(-((s - 0.78) / 0.16) ** 2) * smooth01((d - 115.0) / 55.0)   # the soft point at the back
        roll = 0.25 * (1.0 - smooth01(s / 0.15)) * (1.0 - smooth01((d - 60.0) / 30.0))   # the rolled face opening
        h = lerp(self.hood_lo(a), HEAD_TOP, s)
        ear = 0.75 * math.exp(-((d - 84.0) / 20.0) ** 2) * math.exp(-((h + 0.4) / 2.6) ** 2)   # room for the ears
        return base + back + crown + tip + roll + ear

    def add_hat(self, b: M.Builder, sid: int | None) -> None:
        lk = self.look
        if not lk.hat or lk.hat[0] != "hood":
            return super().add_hat(b, sid)
        bind = Bind.rigid("head")
        HK = self.HK
        lod = getattr(self, "lod", 0)
        cap = self.head_cap(self.hood_lo, self.hood_thick, top_h=HEAD_TOP, cols=26 if lod else 40,
                            rows=6 if lod else 8, sunk=0.1, inner=bool(lod))
        proxy = self.head_cap(self.hood_lo, lambda s, a: self.hood_thick(s, a) + 0.05, top_h=HEAD_TOP, cols=28,
                              rows=6, sunk=0.1)
        hid = self._exact(b, cap, "hat", bind, name="hood", hull=proxy, hull_pad=True)
        # the rolled edge of the face opening (a fat cord in the hood's shade)
        pts = []
        for a in np.linspace(-92.0, 92.0, 17):
            pts.append(self.hp(self.hood_lo(a) + 0.15, a, self.hood_thick(0.0, a) - 0.25))
        rim = M.tube(pts, [0.42 * HK] * len(pts), sides=6, dome0=0.2 * HK, dome1=0.2 * HK)
        b.add(rim, "hat_line", bind, name="hood_rim", outline=False, covered_by=hid)
        # two soft creases from the crown down the sides
        for a in (66.0, -66.0):
            path = [self.hp(lerp(self.hood_lo(a) + 1.0, HEAD_TOP - 1.0, t), a + 8.0 * t,
                            self.hood_thick(lerp(0.12, 0.85, t), a) + 0.03) for t in np.linspace(0, 1, 7)]
            b.add(M.tube(path, [0.13 * HK] * len(path), sides=5, dome0=0.0, dome1=0.0), "hat_line", bind,
                  name="crease", outline=False, covered_by=hid)
        self.add_cowl(b)

    def cowl_point(self, u: float, a: float, out: float) -> Vector:
        """The hood's cowl: from round the jaw (u 0, on the hood) to over the shoulders (u 1, the mantle profile)."""
        mp = self.mantle_profile()
        z_top = self.hz(-4.4)
        z_mid = self.th(self.L + 0.6)
        z_bot = self.th(self.L - 2.9)
        if u <= 0.4:
            k = u / 0.4
            p0 = self.hp(-4.4, a, self.hood_thick(0.1, a) + out / self.HK - 0.05)
            p1 = mp.point(z_mid, a, 0.55 * self.U + out)
            return p0.lerp(p1, smooth01(k) * 0.6 + 0.4 * k)
        k = (u - 0.4) / 0.6
        z = lerp(z_mid, z_bot, k)
        del z_top
        return mp.point(z, a, lerp(0.55, 0.45, k) * self.U + out)

    def add_cowl(self, b: M.Builder) -> None:
        lod = getattr(self, "lod", 0)
        def grid(rows, cols):
            outer, inner = [], []
            for r in range(rows + 1):
                u = r / rows
                a0 = lerp(46.0, 16.0, smooth01(u))
                ro, ri = [], []
                for c in range(cols + 1):
                    t = c / cols
                    a = lerp(a0, 360.0 - a0, t)
                    a = a - 360.0 if a > 180.0 else a
                    ro.append(self.cowl_point(u, a, 0.006))
                    ri.append(self.cowl_point(u, a, -0.002))
                outer.append(ro)
                inner.append(ri)
            return outer, inner
        rows = 6 if lod else 8
        cols = 16 if lod else 22
        outer, inner = [], []
        for r in range(rows + 1):
            u = r / rows
            a0 = lerp(46.0, 16.0, smooth01(u))                    # the front opening narrows toward the chest
            ro, ri = [], []
            for c in range(cols + 1):
                t = c / cols
                a = lerp(a0, 360.0 - a0, t)
                a = a - 360.0 if a > 180.0 else a
                ro.append(self.cowl_point(u, a, 0.006))
                ri.append(self.cowl_point(u, a, -0.002))
            outer.append(ro)
            inner.append(ri)
        part = C.thick_patch(outer, inner, sub_tags=True)
        ho, hi_ = grid(5, 14)
        hull = C.thick_patch(ho, hi_)
        mb = HB.blend4(self.rig, "spine_03", "neck_01", "clavicle_l", "clavicle_r", falloff=5.0, smooth=0.02)
        z_head = self.hz(-4.6)
        z_neck = self.th(self.L + 0.2)
        head_w = lambda co: np.clip((co[:, 2] - z_neck) / max(z_head - z_neck, 1e-6), 0.0, 1.0)  # noqa: E731
        bones = ["spine_03", "neck_01", "clavicle_l", "clavicle_r", "head"]

        def wfn(co):
            hw = HB.smooth01(head_w(co)) * 0.85
            body = mb.fn(co) if mb.kind == "custom" else None
            if body is None:
                # proximity among the four body bones (as Bind.blend)
                J = self.rig.joints
                segs = [(J["spine_03"], J["neck"]), (J["neck"], J["neck"] + Vector((0, 0, 0.08))),
                        (J["neck"], J["shoulder_l"]), (J["neck"], J["shoulder_r"])]
                a_ = np.array([s[0] for s in segs])
                b_ = np.array([s[1] for s in segs])
                d = R._segment_dist(co, a_, b_) + 0.02
                w = 1.0 / d ** 5.0
                body = w / w.sum(1, keepdims=True)
            W = np.zeros((len(co), 5))
            W[:, :4] = body * (1.0 - hw)[:, None]
            W[:, 4] = hw
            return HB.top4(W)
        cid = b.add(part, "hat", Bind.custom(bones, wfn), name="cowl", sub_regions={1: "hat", 2: "hat_line", 0: "hat"},
                    hull=hull, hull_pad=True)
        del cid

    # ── belt: the rope cincture ─────────────────────────────────────────────────────────────────────────
    def add_belt(self, b: M.Builder, ids: dict) -> None:
        if not self.has("rope_belt"):
            return super().add_belt(b, ids)
        U = self.U
        bp = self.belt_profile()
        z = self.th(1.0)
        sb = self.spine_bind() if not self.skirted else Bind.blend("pelvis", "spine_01", smooth=0.04)
        ring = [bp.point(z + 0.18 * U * math.sin(math.radians(a) * 2.0), a, 0.35 * U)
                for a in np.linspace(-180.0, 180.0, 36, endpoint=False)]
        cord = M.sweep(ring, M.superellipse(1, 1, 8, 2.0), scales=[(0.55 * U, 0.55 * U)] * 36, closed_path=True,
                       up=(0, 0, 1), cap0=None, cap1=None)
        bid = b.add(cord, "belt", sb, name="belt", hull_pad=True)
        self.belt_id = bid
        # the twist: short dark strokes round the cord
        for a in np.linspace(-170.0, 170.0, 18):
            p = bp.point(z + 0.18 * U * math.sin(math.radians(a) * 2.0), a, 0.35 * U + 0.55 * U * 0.8)
            n = bp.normal(z, a)
            fr = R.frame(p, n, (0, 0, 1)) @ Matrix.Rotation(math.radians(35.0), 4, "Y")
            b.add(M.box((0.25 * U, 0.12 * U, 1.0 * U), (0, 0, 0), bevel=0.0006).transform(fr), "belt_line"
                  if "belt_line" in b.swatch else "leather", sb, name="stitch", outline=False, covered_by=bid)
        # knot at the front left and two hanging ends with frayed tips (rigid on the pelvis → thigh)
        a_k = 24.0
        kp = bp.point(z - 0.2 * U, a_k, 0.9 * U)
        b.add(ell((0.95 * U, 0.8 * U, 0.9 * U), kp, sides=10, rings=6), "belt", sb, name="rope_knot", hull_pad=True)
        n = bp.normal(z, a_k)
        out = Vector((n.x, n.y, 0.0)).normalized()
        for k, (dl, ln) in enumerate(((0.9, 10.5), (-0.5, 13.0))):
            side = out.cross(Vector((0, 0, 1))).normalized()
            p0 = kp + Vector((0, 0, -0.6 * U)) + side * dl * 0.4 * U
            pts = [p0 + out * (0.25 * U * math.sin(math.pi * t)) + side * dl * 0.5 * U * t
                   + Vector((0, 0, -ln * U * t)) for t in np.linspace(0, 1, 6)]
            if self.skirted:
                # follow the robe's flare (the ends lie on the cloth, never inside it)
                fixed = []
                for p in pts:
                    zz = max(min(p.z, self.skirt_z0), self.skirt_z1)
                    kk = (self.skirt_z0 - zz) / max(self.skirt_z0 - self.skirt_z1, 1e-6)
                    ap = math.degrees(math.atan2(p.x, -p.y))
                    sp = self.skirt_point(kk, ap, self.SKIRT_THICK + 0.55 * U)
                    q = Vector((p.x, p.y, p.z))
                    if q.xy.length < sp.xy.length:
                        q.x, q.y = sp.x, sp.y
                    fixed.append(q)
                pts = fixed
            end = M.tube(pts, [0.45 * U] * 5 + [0.55 * U], sides=8, dome0=0.0, dome1=0.25 * U)
            wb = Bind.blend("pelvis", "thigh_l", falloff=6.0) if not self.skirted else Bind.custom(
                self.skirt_bones(), self.skirt_weights)
            b.add(end, "belt", wb, name="rope_end", hull_pad=True)

    # ── arms: rolled sleeves, gloves ────────────────────────────────────────────────────────────────────
    def _add_rolled_arm(self, b: M.Builder, rig: R.Rig, side: str, hand: bool = True) -> None:
        if not self.has("rolled_sleeves"):
            super().add_arm(b, rig, side, hand=False)
            if hand:
                self.add_hand(b, rig, side)
            return
        J = rig.joints
        g, U = self.g, self.U
        S, E, W = J[f"shoulder_{side}"], J[f"elbow_{side}"], J[f"wrist_{side}"]
        ua, la, cl = f"upperarm_{side}", f"lowerarm_{side}", f"clavicle_{side}"
        dua = (E - S).normalized()
        dla = (W - E).normalized()
        k = 0.92
        r_s, r_e, r_w = 2.9 * g * U * k, 2.5 * g * U * k, 2.15 * g * U * k
        bind = Bind.blend(cl, ua, la, f"hand_{side}", falloff=5.0)
        lod = getattr(self, "lod", 0)
        # the bare forearm (skin) runs up inside the sleeve; the sleeve stops in a roll just above the elbow
        fpts = [S.lerp(E, 0.55), S.lerp(E, 0.8), E, E.lerp(W, 0.2), E.lerp(W, 0.55), W - dla * 0.01]
        frad = [r_e * 0.95, r_e * 0.96, r_e * 0.95, lerp(r_e, r_w, 0.2) * 0.95, lerp(r_e, r_w, 0.55) * 0.95,
                r_w * 0.95]
        fid = self._exact(b, M.tube(fpts, frad, sides=12 if lod else 16, dome0=0.0, dome1=r_w * 0.4), "skin", bind,
                          name=f"forearm_{side}", hull_pad=True)
        del fid
        upts = [S - dua * 0.045, S.lerp(E, 0.45), S.lerp(E, 0.78)]
        urad = [r_s, lerp(r_s, r_e, 0.45) * 1.04, lerp(r_s, r_e, 0.78) * 1.06]
        aid = self._exact(b, M.tube(upts, urad, sides=12 if lod else 16, dome0=r_s * 0.6, dome1=0.0), "sleeve", bind,
                          name=f"arm_{side}", hull_pad=True)
        c0 = S.lerp(E, 0.74)
        c1 = S.lerp(E, 0.92)
        roll = M.tube([c0, c0.lerp(c1, 0.5), c1], [r_e * 1.22, r_e * 1.30, r_e * 1.16], sides=12 if lod else 16,
                      dome0=r_e * 0.3, dome1=r_e * 0.35)
        rid = self._exact(b, roll, "sleeve", bind, name=f"sleeve_roll_{side}", hull_pad=True)
        b.add(M.tube([c0.lerp(c1, 0.45), c0.lerp(c1, 0.6)], [r_e * 1.315, r_e * 1.31], sides=16, dome0=0.0,
                     dome1=0.0), "top_shade", bind, name=f"roll_fold_{side}", outline=False, covered_by=rid)
        del aid
        if hand:
            self.add_hand(b, rig, side)

    def add_hand(self, b: M.Builder, rig: R.Rig, side: str) -> None:
        if not self.has("gloves"):
            return super().add_hand(b, rig, side)
        # folk.add_hand in glove leather (the A1 'leather' swatch) with a turned cuff and stains on the fingers
        sw = b.swatch
        skin = sw["skin"]
        sw["skin"] = sw["leather"]
        n0 = len(b.parts)
        try:
            super().add_hand(b, rig, side)
        finally:
            sw["skin"] = skin
        J = rig.joints
        W, T = J[f"wrist_{side}"], J[f"handtip_{side}"]
        hd = (T - W).normalized()
        hbn = f"hand_{side}"
        sz = (1.0 + 0.45 * (self.g - 1.0)) * 1.05
        cuff = M.tube([W - hd * 0.035, W + hd * 0.006], [0.040 * sz, 0.042 * sz], sides=12, dome0=0.0, dome1=0.0)
        b.add(cuff, "leather", Bind.blend(f"lowerarm_{side}", hbn, falloff=6.0), name=f"glove_cuff_{side}",
              hull_pad=True)
        hid = next((i for i in range(n0, len(b.parts)) if b.parts[i]["name"] == f"hand_{side}" and i in b._pending),
                   None)
        if hid is not None:
            sg = 1.0 if side == "l" else -1.0
            for k, (f, o, r) in enumerate(((0.62, 0.9, 0.016), (0.40, -0.6, 0.012))):
                c = W + hd * (0.03 + f * 0.06) + Vector((sg * 0.005 * o, -0.034 * sz, 0.004 * o))
                b.add(ell((r, 0.004, r * 0.8), c, sides=8, rings=4), "stain", Bind.rigid(hbn), name="stain",
                      outline=False, covered_by=hid)

    # ── dress pieces on the body ────────────────────────────────────────────────────────────────────────
    def add_dress(self, b: M.Builder, rig: R.Rig, ids: dict) -> None:
        if self.has("tabard"):
            self.add_tabard(b)
        if self.has("sigil_cut"):
            self.add_sigil_cut(b, ids.get("torso"))
        if self.has("halfplate"):
            for side in ("l", "r"):
                self.add_spaulder(b, rig, side)
                self.add_vambrace(b, rig, side, None)
        if self.has("gorget"):
            self.add_gorget(b)
        if self.has("gambeson"):
            self.add_quilting(b, ids.get("torso"))
        if self.has("insignia_torn"):
            self.add_torn_insignia(b, rig)
        if self.has("name_tags"):
            self.add_name_tags(b)
        if self.has("tally_cord"):
            self.add_tally_cord(b)
        if self.has("apron_stains"):
            self.add_apron_stains(b)
        if self.has("feather_collar"):
            self.add_feather_collar(b)
        if self.has("slung_boots"):
            self.add_slung_boots(b)
        if self.has("relic_tray"):
            self.add_relic_tray(b)
        if self.has("wrist_rope"):
            for side in ("l", "r"):
                self.add_wrist_rope(b, rig, side)
        if self.has("earth_stains"):
            self.add_earth_stains(b, ids.get("skirt"))
        if self.look.item == "barrow":
            self.add_barrow_wheel(b)

    # half-plate ---------------------------------------------------------------------------------------------
    def add_spaulder(self, b: M.Builder, rig: R.Rig, side: str) -> None:
        """A dented steel spaulder: a domed cap over the shoulder and two lames down the upper arm, each lame's
        lower edge a dark rim; dents as darker dimples (the plate is grey, the tabard over it crimson)."""
        J = rig.joints
        g, U = self.g, self.U
        S, E = J[f"shoulder_{side}"], J[f"elbow_{side}"]
        sg = 1.0 if side == "l" else -1.0
        dua = (E - S).normalized()
        k = 0.92
        r_s = 2.9 * g * U * k
        bind = Bind.blend(f"clavicle_{side}", f"upperarm_{side}", falloff=6.0)
        # cap: a lathe dome whose axis runs from the arm up-out over the shoulder
        ax = (-dua + Vector((sg * 0.35, 0.0, 0.0))).normalized()
        c = S + ax * (0.2 * r_s) + Vector((sg * 0.006, 0.0, 0.0))
        R0 = r_s * 1.62
        H = r_s * 1.05
        prof = [(0.001, H), (R0 * 0.45, H * 0.93), (R0 * 0.78, H * 0.68), (R0 * 0.96, H * 0.32), (R0 * 1.0, 0.0),
                (R0 * 0.97, -0.25 * H), (R0 * 0.80, -0.30 * H)]
        dome = M.lathe(prof, sides=20, cap0="fan", cap1="fan")
        fr = HB.axis_frame(c, ax)
        dome.transform(fr @ Matrix.Diagonal((1.0, 0.92, 1.0, 1.0)))
        did = b.add(dome, "halfplate", bind, name=f"spaulder_{side}", hull_pad=True)
        # rim at the cap's lower edge
        rim = [fr @ Vector((math.cos(t) * R0 * 1.0, math.sin(t) * R0 * 0.92, -0.02 * H))
               for t in np.linspace(0, 2 * math.pi, 16, endpoint=False)]
        b.add(M.sweep(rim, M.superellipse(1, 1, 4, 2.0), scales=[(0.0035, 0.006)] * 16, closed_path=True,
                      up=tuple(ax), cap0=None, cap1=None), "halfplate_line", bind, name="rivet", outline=False,
              covered_by=did)
        # two lames down the upper arm, each smaller and tucked under the one above
        lid = None
        for i, (f, rr, ln) in enumerate(((0.36, 1.30, 0.050), (0.56, 1.22, 0.042))):
            p0 = S + dua * (f * (E - S).length)
            lame = M.tube([p0 - dua * ln * 0.5, p0 + dua * ln * 0.5], [r_s * rr, r_s * (rr - 0.06)], sides=18,
                          dome0=0.0, dome1=0.0)
            lid = b.add(lame, "halfplate", Bind.blend(f"upperarm_{side}", f"clavicle_{side}", falloff=8.0),
                        name=f"lame_{side}", hull_pad=True)
            b.add(M.tube([p0 + dua * ln * 0.38, p0 + dua * ln * 0.5], [r_s * (rr - 0.045), r_s * (rr - 0.05)],
                         sides=12, dome0=0.0, dome1=0.0), "halfplate_line",
                  Bind.blend(f"upperarm_{side}", f"clavicle_{side}", falloff=8.0), name="rivet", outline=False,
                  covered_by=lid)
        # dents: shallow dark dimples on the cap (deterministic places)
        for k_, (u, v) in enumerate(((0.35, 0.8), (0.62, -1.9), (0.5, 2.6))):
            q = fr @ Vector((math.cos(v) * R0 * u, math.sin(v) * R0 * u * 0.92, 0.0))
            n = (q - c).normalized()
            hgt = math.sqrt(max(0.0, 1.0 - u * u)) * H * 0.95
            p = c + (fr.to_3x3() @ Vector((math.cos(v) * R0 * u, math.sin(v) * R0 * u * 0.92, hgt)))
            n = (p - c).normalized()
            dent = ell((0.010 + 0.003 * k_, 0.0022, 0.008), (0, 0, 0), sides=6, rings=3)
            dent.transform(R.frame(p + n * 0.0004, n, ax))
            b.add(dent, "halfplate_dark", bind, name="rivet", outline=False, covered_by=did)

    def add_vambrace(self, b: M.Builder, rig: R.Rig, side: str, arm_id) -> None:
        J = rig.joints
        g, U = self.g, self.U
        E, W = J[f"elbow_{side}"], J[f"wrist_{side}"]
        dla = (W - E).normalized()
        k = 0.92
        r_e, r_w = 2.5 * g * U * k, 2.15 * g * U * k
        p0, p1 = E.lerp(W, 0.30), W - dla * 0.022
        bind = Bind.blend(f"lowerarm_{side}", f"hand_{side}", falloff=8.0)
        vb = M.tube([p0, p0.lerp(p1, 0.5), p1], [lerp(r_e, r_w, 0.3) * 1.28, lerp(r_e, r_w, 0.65) * 1.24,
                                                 r_w * 1.26], sides=16, dome0=0.0, dome1=0.0)
        vid = b.add(vb, "halfplate", bind, name=f"vambrace_{side}", hull_pad=True)
        for t in (0.04, 0.96):
            q = p0.lerp(p1, t)
            r = lerp(lerp(r_e, r_w, 0.3) * 1.28, r_w * 1.26, t) + 0.0012
            b.add(M.tube([q - dla * 0.004, q + dla * 0.004], [r, r], sides=12, dome0=0.0, dome1=0.0),
                  "halfplate_line", bind, name="rivet", outline=False, covered_by=vid)
        del arm_id

    def add_gorget(self, b: M.Builder) -> None:
        """Two steel lames round the base of the neck over the tabard's collar (#9AA3B2, a dark rim each)."""
        tp = self.torso
        L, U = self.L, self.U
        J = self.rig.joints
        nb = Bind.blend("spine_03", "neck_01", falloff=6.0, smooth=0.02)
        # lower lame: a collar flaring over the top of the chest and shoulders
        rings = []
        for h, out in ((L - 1.6, 0.55), (L - 0.4, 0.75), (L + 0.6, 0.65), (L + 1.2, 0.40)):
            z = self.th(h)
            rx, ry, cy, cx = tp.at(z)
            rings.append((z, rx + out * U, ry + out * U, cy, cx))
        prof = Profile(rings, exp=2.3)
        zs = [prof.z0 + (prof.z1 - prof.z0) * i / 4 for i in range(5)]
        # a closed band: the outer profile + a thin inner one
        inner_rings = [(z, rx - 0.006, ry - 0.006, cy, cx) for z, rx, ry, cy, cx in rings]
        lo_part = self._band(prof, Profile(inner_rings, exp=2.3), zs, 22)
        lid = b.add(lo_part, "gorget", nb, name="gorget", hull_pad=True)
        b.add(C.surface_band(prof, prof.z0 + 0.004, 0.006, 0.0014, segs=22, out0=-0.001), "halfplate_dark", nb,
              name="rivet", outline=False, covered_by=lid)
        # upper lame round the neck itself
        n0 = J["neck"] + Vector((0, 0.006, -0.012))
        r0 = 2.35 * self.g ** 0.8 * U
        up_rings = [(n0.z - 0.004, r0 * 1.42, r0 * 1.32, n0.y), (n0.z + 0.020, r0 * 1.30, r0 * 1.22, n0.y),
                    (n0.z + 0.034, r0 * 1.20, r0 * 1.13, n0.y)]
        up = Profile(up_rings, exp=2.2)
        up_in = Profile([(z, rx - 0.005, ry - 0.005, cy) for z, rx, ry, cy in up_rings], exp=2.2)
        uzs = [up.z0 + (up.z1 - up.z0) * i / 3 for i in range(4)]
        uid = b.add(self._band(up, up_in, uzs, 18), "gorget", Bind.blend("neck_01", "spine_03", falloff=6.0),
                    name="gorget_neck", hull_pad=True)
        b.add(C.surface_band(up, up.z0 + 0.003, 0.005, 0.0012, segs=18, out0=-0.001), "halfplate_dark",
              Bind.blend("neck_01", "spine_03", falloff=6.0), name="rivet", outline=False, covered_by=uid)

    @staticmethod
    def _band(outer: Profile, inner: Profile, zs, sides: int) -> M.Part:
        oo = [[outer.point(z, -180.0 + 360.0 * c / sides) for c in range(sides)] for z in zs]
        ii = [[inner.point(z, -180.0 + 360.0 * c / sides) for c in range(sides)] for z in zs]
        return C.thick_patch(oo, ii, wrap_u=True)

    # tabard ---------------------------------------------------------------------------------------------------
    def add_tabard(self, b: M.Builder) -> None:
        """Front and back panels from the belt to mid-thigh (the crusade tabard over the plate), open at the sides;
        brass-edged hems. Weighted like the apron panel (pelvis → thighs)."""
        U = self.U
        bp = self.belt_profile()
        zb = self.th(0.8)
        drop = 12.0 * U
        th_ = 0.006
        J = self.rig.joints
        knee_y = J["knee_l"].y
        for back in (False, True):
            cols, rows = 9, 7
            a_c = 180.0 if back else 0.0
            top = [bp.point(zb, a_c + a, 0.25 * U) for a in np.linspace(-44.0, 44.0, cols)]
            outer, inner = [], []
            for r in range(rows + 1):
                s = r / rows
                ro, ri = [], []
                for c, p0 in enumerate(top):
                    u = c / (cols - 1) * 2 - 1
                    y0 = p0.y
                    # hang plumb with a slight flare away from the thighs
                    sgn = 1.0 if back else -1.0
                    y = y0 + sgn * (0.012 * smooth01(s / 0.5) + 0.006 * s * s * (1 - u * u))
                    if not back:
                        y = min(y, knee_y - 3.4 * self.g * U * s)
                    x = p0.x * (1 + 0.08 * s)
                    z = zb - drop * s
                    ro.append(Vector((x, y - sgn * -th_ * 0.5, z)))
                    ri.append(Vector((x, y + sgn * -th_ * 0.5, z)))
                outer.append(ro)
                inner.append(ri)
            panel = C.thick_patch(outer, inner, sub_tags=True)

            def wfn(co, z_top=zb, drop=drop):
                k = np.clip((z_top - co[:, 2]) / drop, 0.0, 1.0)
                side = np.clip(co[:, 0] / (5.0 * self.g * U) * 0.5 + 0.5, 0.0, 1.0)
                leg = 0.5 * HB.smooth01(k / 0.9)
                W = np.zeros((len(co), 3))
                W[:, 0] = 1.0 - leg
                W[:, 1] = leg * side
                W[:, 2] = leg * (1.0 - side)
                return HB.top4(W)
            pb = Bind.custom(["pelvis", "thigh_l", "thigh_r"], wfn)
            pid = b.add(panel, "top", pb, name="tabard_panel", sub_regions={1: "top", 2: "top_shade", 0: "top"},
                        hull_pad=True)
            # brass hem across the bottom
            sgn = 1.0 if back else -1.0
            hem_o = [[outer[rows][c] + Vector((0, sgn * 0.0014, dz)) for c in range(cols)] for dz in (0.0, 0.9 * U)]
            hem_i = [[inner[rows][c] + Vector((0, -sgn * 0.0014, dz)) for c in range(cols)] for dz in (0.0, 0.9 * U)]
            b.add(C.thick_patch(hem_o, hem_i), "trim" if "trim" in b.swatch else "top_shade", pb, name="tabard_hem",
                  outline=False, covered_by=pid)

    def add_sigil_cut(self, b: M.Builder, tid: int | None) -> None:
        """The church sigil cut out of the tabard's chest: a ragged window onto the plate under it — the flame-in-a-
        ring shape survives only as the hole. It reads as a hole, not a badge (review: a dark rim round grey plate
        read as a shield sewn on): the cloth's cut edge shows its thickness as a dark inner bevel, light threads fray
        from it into the hole, the cloth's edge shades the plate's top inside, and the exposed plate is dented and
        scratched."""
        tp = self.torso
        L, U = self.L, self.U
        zc = self.th(L - 5.4)
        sb = self.spine_bind()
        rng = np.random.default_rng(7)
        n = 22

        def ring(rad_u):
            pts = []
            for i in range(n):
                t = 2 * math.pi * i / n
                r = rad_u * (1.0 + 0.11 * rng.standard_normal())
                # the flame tongue: the window reaches up at the top
                r *= 1.0 + 0.55 * math.exp(-((t - math.pi / 2) / 0.35) ** 2)
                pts.append((r * math.cos(t), r * math.sin(t)))
            return pts
        edge = ring(3.1)

        def on_chest(x_u, y_u, lift):
            z = zc + y_u * U
            rx, ry, cy, cx = tp.at(z)
            a = math.degrees(x_u * U / max(rx, 1e-3))
            return tp.point(z, -a, lift)

        def band(f0, f1, lift, thick, mask=None):
            """A ring band between ``f0`` and ``f1`` of the edge outline (``mask(t)`` → 0..1 thins it)."""
            outer, inner = [], []
            for f in (f0, f1):
                ro, ri = [], []
                for i, (x, y) in enumerate(edge):
                    t = 2 * math.pi * i / n
                    k = 1.0 if mask is None else mask(t)
                    ff = lerp(f1, f, k)
                    ro.append(on_chest(x * ff, y * ff, lift + thick))
                    ri.append(on_chest(x * ff, y * ff, lift))
                outer.append(ro)
                inner.append(ri)
            return C.thick_patch(outer, inner, wrap_u=True)

        def fan(f, lift, thick):
            outer, inner = [], []
            for k in (0.06, 1.0):
                outer.append([on_chest(x * f * k, y * f * k, lift + thick) for x, y in edge])
                inner.append([on_chest(x * f * k, y * f * k, lift) for x, y in edge])
            return C.thick_patch(outer, inner, wrap_u=True)
        kw = {"covered_by": tid} if tid is not None else {}
        # the exposed plate (recessed: it lies under the cloth's surface plane)
        b.add(fan(0.98, 0.0012, 0.0008), "halfplate", sb, name="sigil_window", outline=False, **kw)
        # the cloth's edge thrown over the plate's top inside the hole (a crescent, widest at the top)
        b.add(band(0.70, 0.98, 0.0021, 0.0004, mask=lambda t: max(0.0, math.sin(t)) ** 1.5),
              "halfplate_dark", sb, name="sigil_shadow", outline=False, **kw)
        # the cut edge of the cloth: its thickness as a dark bevel just inside the tabard's surface
        b.add(band(0.98, 1.10, 0.0026, 0.0008), "top_shade", sb, name="sigil_bevel", outline=False, **kw)
        # frayed light threads from the cut edge into the hole
        for i in range(1, n, 3):
            x, y = edge[i]
            ln = 0.16 + 0.12 * ((i * 7) % 5) / 4
            tw = 0.25 * math.sin(i * 1.7)
            p0 = on_chest(x * 1.06, y * 1.06, 0.0034)
            p1 = on_chest(x * (1.06 - ln) + tw, y * (1.06 - ln), 0.0034)
            b.add(M.tube([p0, p1], [0.0011, 0.0007], sides=3, dome0=0.0, dome1=0.0), "hair", sb,
                  name="sigil_thread", outline=False, **kw)
        # the plate: the breastplate's lame edge across the window, a dent and a bright scratch
        pts = [on_chest(x, -0.5, 0.0024) for x in np.linspace(-1.9, 1.9, 7)]
        b.add(M.tube(pts, [0.0016] * 7, sides=5, dome0=0.0, dome1=0.0), "halfplate_line", sb, name="seam",
              outline=False, **kw)
        dz = on_chest(0.75, 0.55, 0.0024)
        nrm = tp.normal(zc + 0.55 * U, -math.degrees(0.75 * U / max(tp.at(zc)[0], 1e-3)))
        fr = R.frame(dz, nrm, (0, 0, 1)) @ Matrix.Rotation(math.radians(25.0), 4, "Y")
        b.add(ell((0.55 * U, 0.0010, 0.36 * U), (0, 0, 0), sides=10, rings=4).transform(fr), "halfplate_dark", sb,
              name="sigil_dent", outline=False, **kw)
        b.add(ell((0.42 * U, 0.0012, 0.08 * U), (0.05 * U, 0.0002, 0.30 * U), sides=8, rings=3).transform(fr),
              "gorget", sb, name="sigil_dent", outline=False, **kw)
        sc = [on_chest(lerp(-1.3, 0.2, u), lerp(0.9, -1.2, u) + 0.12 * math.sin(u * 7.0), 0.0025)
              for u in np.linspace(0, 1, 6)]
        b.add(M.tube(sc, [0.0010] * 6, sides=4, dome0=0.0, dome1=0.0), "gorget", sb, name="sigil_scratch",
              outline=False, **kw)

    # deserter -------------------------------------------------------------------------------------------------
    def add_quilting(self, b: M.Builder, tid: int | None) -> None:
        """Gambeson quilting: horizontal rows across the coat's chest and back (top shade lines)."""
        tp = self.torso
        L, U = self.L, self.U
        sb = self.spine_bind()
        kw = {"covered_by": tid} if tid is not None else {}
        for h in np.arange(L - 3.0, 0.5, -3.0):
            for a0, a1 in ((-64.0, -8.0), (8.0, 64.0)):
                band = C.surface_band(tp, self.th(h), 0.22 * U, 0.0016, a0=a0, a1=a1, segs=6, out0=-0.0008)
                b.add(band, "top_shade", sb, name="seam", outline=False, **kw)

    def add_torn_insignia(self, b: M.Builder, rig: R.Rig) -> None:
        """The unfaded patch on the left sleeve where the badge was torn off (darker cloth), thread stubs round it."""
        J = rig.joints
        side = "l"
        g, U = self.g, self.U
        S, E = J[f"shoulder_{side}"], J[f"elbow_{side}"]
        dua = (E - S).normalized()
        k = 0.92
        r_s, r_e = 2.9 * g * U * k, 2.5 * g * U * k
        c = S.lerp(E, 0.33)
        r = lerp(r_s, r_e, 0.33) * 1.02
        out = Vector((1.0, -0.25, 0.0))
        out = (out - dua * out.dot(dua)).normalized()
        side_v = dua.cross(out).normalized()
        bind = Bind.blend(f"clavicle_{side}", f"upperarm_{side}", falloff=6.0)
        aid = self.part_id(b, f"arm_{side}")
        # shield-shaped patch wrapped round the sleeve
        rows, cols = 4, 6
        outer, inner = [], []
        for i in range(rows + 1):
            v = i / rows
            w = 1.0 - 0.45 * v ** 2
            ro, ri = [], []
            for j in range(cols + 1):
                u = (j / cols * 2 - 1) * w
                ang = u * 0.62
                d = out * math.cos(ang) + side_v * math.sin(ang)
                p = c + dua * (v - 0.45) * 0.075
                ro.append(p + d * (r + 0.0022))
                ri.append(p + d * (r + 0.0005))
            outer.append(ro)
            inner.append(ri)
        kw = {"covered_by": aid} if aid is not None else {}
        b.add(C.thick_patch(outer, inner), "top_shade", bind, name="insignia_patch", outline=False, **kw)
        for k_, (u, v) in enumerate(((-0.8, 0.0), (0.1, -0.02), (0.75, 0.05), (-0.5, 0.75), (0.6, 0.7))):
            ang = u * 0.62
            d = out * math.cos(ang) + side_v * math.sin(ang)
            p = c + dua * (v - 0.45) * 0.075 + d * (r + 0.002)
            b.add(M.tube([p, p + d * 0.003 + dua * -0.006], [0.0011, 0.0009], sides=4, dome0=0.0, dome1=0.0), "cord"
                  if "cord" in b.swatch else "top_shade", bind, name="stitch", outline=False, **kw)

    # hald, vira -----------------------------------------------------------------------------------------------
    def add_name_tags(self, b: M.Builder) -> None:
        """Two bronze name-tags on a cord round Hald's neck, resting on the chest above the apron bib."""
        tp = self.torso
        L, U = self.L, self.U
        nb = Bind.blend("spine_02", "spine_03", "neck_01", falloff=4.0)
        # the tags (and the cord's V over the bib) sit on the apron bib: its padded hull covers them, so where they
        # reach the silhouette (sw / ne views) they keep the ink; the rest of the loop lies on the shirt
        bib = next((i for i, q in enumerate(b.parts) if q["name"] == "apron_bib" and i in b._pending), None)
        kw = {"covered_by": bib} if bib is not None else {}

        def cord_pt(d):                                   # d = degrees from the V's low point (a ≈ −31°)
            a = ((d - 31.0 + 180.0) % 360.0) - 180.0
            dd = abs(d)
            h = L + 0.9 - 0.4 * smooth01((90.0 - abs(a)) / 90.0) - 4.9 * smooth01((40.0 - dd) / 40.0)
            # on the bib: over it; round the neck: on the shirt (higher, it floated off the hull of the shoulders)
            k = smooth01((60.0 - dd) / 30.0)
            h -= 0.3 * (1.0 - k)
            lift = lerp(0.03 * U + 0.0005, 0.55 * U + 0.002, k) + (0.35 * U + 0.006) * smooth01((50.0 - dd) / 30.0) * (
                1.0 if self.look.apron else 0.0)
            return tp.point(self.th(h), a, lift)

        front = [cord_pt(d) for d in np.linspace(-60.0, 60.0, 9)]
        segs = [(front, kw, "tag_cord")] + [([cord_pt(d) for d in np.linspace(d0, d0 + 60.0, 5)], {}, f"tag_cord_{d0:.0f}x")
                                            for d0 in (60.0, 120.0, 180.0, 240.0)]
        for path, k, nm in segs:
            cord = M.sweep(path, M.superellipse(1, 1, 6, 2.0), scales=[(0.0028, 0.0028)] * len(path), up=(0, 0, 1),
                           cap0=None, cap1=None)
            b.add(cord, "cord", nb, name=nm, outline=False, **k)
        # beside the braided beard on the near (right) side: under it they were hidden from every view
        for k, (a, dz) in enumerate(((-27.0, 0.0), (-37.0, -0.55))):
            z = self.th(L - 4.6 + dz)
            p = tp.point(z, a, 0.55 * U + 0.006 + ((0.35 * U + 0.006) if self.look.apron else 0.0))
            n = tp.normal(z, a)
            fr = R.frame(p, n, (0, 0, 1)) @ Matrix.Rotation(math.radians(8.0 - 16.0 * k), 4, "Y")
            tag = M.box((1.15 * U, 0.003, 1.65 * U), (0, 0, 0), bevel=0.0015)
            b.add(tag.transform(fr), "brass", nb, name="name_tag", outline=False, **kw)

    def add_tally_cord(self, b: M.Builder) -> None:
        """Vira's ration-tally: a knotted cord tucked under the belt at the left hip, hanging to mid-thigh."""
        U = self.U
        bp = self.belt_profile()
        a = 66.0
        z0 = self.th(1.6)
        n = bp.normal(z0, a)
        out = Vector((n.x, n.y, 0.0)).normalized()
        p0 = bp.point(z0, a, 0.45 * U)
        pts = [p0 + out * (0.006 * math.sin(math.pi * t)) + Vector((0, 0, -0.20 * t)) for t in np.linspace(0, 1, 7)]
        kb = Bind.blend("pelvis", "thigh_l", falloff=6.0)
        cid = b.add(M.tube(pts, [0.0032] * 7, sides=6, dome0=0.0, dome1=0.002), "cord", kb, name="tally_cord",
                    hull_pad=True)
        for t in (0.25, 0.38, 0.51, 0.64, 0.80, 0.93):
            q = pts[0].lerp(pts[-1], t) + out * (0.006 * math.sin(math.pi * t))
            b.add(ell((0.0058, 0.0058, 0.0050), q, sides=8, rings=5), "cord", kb, name="knot", outline=False,
                  covered_by=cid)

    # marl -----------------------------------------------------------------------------------------------------
    def add_apron_stains(self, b: M.Builder) -> None:
        tp = self.torso
        L, U = self.L, self.U
        sb = self.spine_bind()
        aid = self.part_id(b, "apron_bib")
        kw = {"covered_by": aid} if aid is not None else {}
        lift = 0.35 * U + 0.006 + 0.0012
        for k, (h, a, r) in enumerate(((L - 6.5, 14.0, 1.0), (L - 9.0, -20.0, 0.75), (L - 11.5, 4.0, 1.25))):
            z = self.th(h)
            p = tp.point(z, a, lift)
            n = tp.normal(z, a)
            blot = ell((r * 1.15 * U, 0.0012, r * 0.8 * U), (0, 0, 0), sides=10, rings=4)
            blot.transform(R.frame(p, n, (0, 0, 1)) @ Matrix.Rotation(math.radians(20.0 * k - 15.0), 4, "Y"))
            b.add(blot, "stain", sb, name="stain", outline=False, **kw)

    # mag crowe ------------------------------------------------------------------------------------------------
    # the feather collar (review: a ruff of round quills with pale tips read as a grey beard / icicles): flat vane
    # cards with a centre rib, longer and fewer, overlapping downward and fanned out past the shoulders; black on
    # black, a cool grey sheen band only along the lit (right, in se) side's vanes, no pale tips
    COLLAR_ROWS = (
        # (torso height h above the shoulder line L, lift (U), count, length (m), width (m), droop deg below level at
        #  the front / shoulder /
        #  back, angular span from the front (deg))
        (1.1, 0.95, 8, 0.150, 0.030, (62.0, 28.0, 58.0), 40.0),
        (-0.4, 0.70, 10, 0.205, 0.036, (66.0, 34.0, 64.0), 34.0),
    )

    def _vane(self, root: Vector, dirv: Vector, nrm: Vector, length: float, width: float, curl: float,
              lod: int = 0):
        """(outer, inner) grids of one flat feather: narrow at the root, widest at ~40 %, a pointed tip; slightly
        cupped across and curling away from the body toward the tip."""
        side = dirv.cross(nrm).normalized()
        outer, inner = [], []
        rows, cols = (5, 2) if not lod else (3, 2)
        for r in range(rows + 1):
            sv = r / rows
            wv = width * (0.28 + 0.72 * smooth01(sv / 0.38)) * (1.0 - smooth01((sv - 0.55) / 0.45)) ** 0.75 + 0.0015
            ro, ri = [], []
            for c in range(cols + 1):
                u = c / cols * 2 - 1
                p = root + dirv * (length * sv) + side * (u * wv) + nrm * (curl * length * sv * sv
                                                                         - 0.18 * wv * u * u)
                ro.append(p + nrm * 0.0013)
                ri.append(p - nrm * 0.0013)
            outer.append(ro)
            inner.append(ri)
        return outer, inner, side

    def add_feather_collar(self, b: M.Builder) -> None:
        """Mag's crow-feather collar over a short black capelet (module docstring; ``COLLAR_ROWS``)."""
        mp = self.mantle_profile()
        U = self.U
        mb = HB.blend4(self.rig, "spine_02", "spine_03", "clavicle_l", "clavicle_r", "neck_01", falloff=5.0,
                       smooth=0.02)
        zs = list(np.linspace(self.th(self.L - 2.2), mp.z1, 6))
        cape = profile_mesh(mp, zs, 26, cap0="fan", cap1="fan", dome0=-0.03)
        b.add(cape, "mantle", mb, name="mantle", hull=profile_mesh(mp, zs[::2] + [zs[-1]], 20, cap0="fan",
                                                                   cap1="fan", dome0=-0.03), hull_pad=True)
        lod = getattr(self, "lod", 0)
        rng = np.random.default_rng(11)
        for ri, (h, lift, n, ln, wd, droop, span0) in enumerate(self.COLLAR_ROWS[::-1]):
            # the lower (longer) row first: the upper row lies over it
            z = self.th(self.L + h)
            cnt = n if not lod else max(5, n * 2 // 3)
            for i in range(cnt):
                # round the shoulders and the back, the throat and upper chest left open
                a = lerp(span0, 360.0 - span0, (i + 0.5 * (ri % 2)) / max(cnt - 1 + 0.5 * (ri % 2), 1))
                a = a - 360.0 if a > 180.0 else a
                d = abs(a)
                if self.has("slung_boots") and 20.0 < a < 70.0:
                    continue                 # the boots' lace and the chest boot push the feathers aside here
                p0 = mp.point(z, a, lift * U)
                nm_ = mp.normal(z, a)
                outd = Vector((nm_.x, nm_.y, 0.0)).normalized()
                # droop: steep at the front and the back, shallow over the shoulders (they fan out past them)
                k_sh = math.exp(-((d - 95.0) / 35.0) ** 2)
                dr = lerp(droop[0] if d < 95.0 else droop[2], droop[1], k_sh)
                dr += 6.0 * (rng.random() - 0.5)
                dirv = (outd * math.cos(math.radians(dr)) + Vector((0, 0, -math.sin(math.radians(dr))))).normalized()
                # the vane's face toward the camera side (out and up from the body)
                nrm = (outd * math.sin(math.radians(dr)) + Vector((0, 0, math.cos(math.radians(dr))))).normalized()
                L_ = ln * (0.88 + 0.24 * rng.random()) * (1.0 + 0.18 * k_sh)
                wv_ = wd * (0.9 + 0.2 * rng.random())
                outer, inner, side = self._vane(p0, dirv, nrm, L_, wv_, 0.10, lod)
                ho, hi_ = self._vane(p0, dirv, nrm, L_, wv_, 0.10, 1)[:2]
                vid = b.add(C.thick_patch(outer, inner), "mantle", mb, name="feather", hull=C.thick_patch(ho, hi_),
                            hull_pad=True)
                # the centre rib (thin: a closeup / portrait detail)
                lit = -170.0 < a < -15.0
                if lit:
                    rib = [p0 + dirv * (L_ * f) + nrm * (0.10 * L_ * f * f + 0.0016) for f in (0.04, 0.45, 0.86)]
                    b.add(M.tube(rib, [0.0011] * 3, sides=3, dome0=0.0, dome1=0.0), "feather_rib", mb,
                          name="feather_rib", outline=False, covered_by=vid)
                    # the cool sheen band along the upper edge of the lit side's vanes (never the tip)
                    band_o, band_i = [], []
                    for f in np.linspace(0.12, 0.70, 4):
                        wv = wd * (0.28 + 0.72 * smooth01(f / 0.38)) * (1.0 - smooth01((f - 0.55) / 0.45)) ** 0.75
                        base = p0 + dirv * (L_ * f) + nrm * (0.10 * L_ * f * f)
                        row_o, row_i = [], []
                        for u in (0.45, 0.88):
                            q = base + side * (u * wv) - nrm * (0.18 * wv * u * u)
                            row_o.append(q + nrm * 0.0021)
                            row_i.append(q + nrm * 0.0013)
                        band_o.append(row_o)
                        band_i.append(row_i)
                    b.add(C.thick_patch(band_o, band_i), "feather_sheen", mb, name="feather_sheen", outline=False,
                          covered_by=vid)

    SLUNG_BOOTS = ((28.0, -0.6, 95.0, -40.0), (128.0, -1.0, 172.0, -16.0))   # h relative to the shoulder line L
    SOLE_K = 0.32

    def add_slung_boots(self, b: M.Builder) -> None:
        """A pair of dead men's boots tied together at the shafts on top of the left shoulder (review: one hung like
        a tan goblet): the front one hangs on the chest in profile — the shaft down from the knot, the foot turned
        across the chest, its sole tipped toward the camera — so the L of shaft and foot, the dark outsole band and
        the heel read; the other hangs on the back the same way. Worn tan leather (the shared bedroll value) over
        dark soles (her boots' leather)."""
        tp = self.torso
        L, U = self.L, self.U
        bind = HB.blend4(self.rig, "spine_02", "spine_03", "clavicle_l", "neck_01", falloff=5.0, smooth=0.02)
        knot = tp.point(self.th(L + 1.1), 84.0, 1.6 * U)
        b.add(ell((0.018, 0.016, 0.014), knot, sides=8, rings=5), "cord", bind, name="boot_knot", hull_pad=True)
        bl, bw, bh, shaft = 0.24, 0.090, 0.072, 0.10
        # (angle round the torso, shaft-top height, the toe's aim, roll about the toe axis toward the viewer)
        BOOTS = self.SLUNG_BOOTS
        # the chest boot high on the left of the chest (her hair is braided down her back: the front locks hid it), the
        # foot turned across the chest above her working hands
        mp = self.mantle_profile()

        def over(z, a, out):
            """A point ``out`` metres off the outermost garment there (the capelet over the torso)."""
            p = tp.point(z, a, out)
            if min(mp.z0, mp.z1) <= z <= max(mp.z0, mp.z1):
                q = mp.point(z, a, out + 0.012)
                if q.xy.length > p.xy.length:
                    p = q
            return p
        for k, (a_top, h_top, toward, tilt) in enumerate(BOOTS):
            z_top = self.th(L + h_top)
            top = over(z_top, a_top, 1.2 * U + bw * 0.5)
            n = tp.normal(z_top, a_top)
            outn = Vector((n.x, n.y, 0.0)).normalized()
            # the foot points along the body surface toward ``toward`` (the midline front / back)
            aim = over(z_top - 0.18, toward, 1.2 * U + bw * 0.5) - over(z_top - 0.18, a_top, 1.2 * U + bw * 0.5)
            toe = (aim - outn * aim.dot(outn)).normalized()
            up = Vector((0.0, 0.0, 1.0))
            toe = (toe - up * toe.dot(up)).normalized()
            # boot local: toe −Y, up +Z, X = Y × Z (Y = heel direction)
            yv = -toe
            xv = yv.cross(up).normalized()
            m = Matrix.Identity(4)
            for i in range(3):
                m[i][0], m[i][1], m[i][2] = xv[i], yv[i], up[i]
            ankle = top - up * (0.60 * bh + shaft + 0.012)
            m.translation = ankle
            # the sole tipped toward the viewer: rolled about the toe axis
            m = Matrix.Translation(ankle) @ Matrix.Rotation(math.radians(tilt), 4, toe) @ Matrix.Translation(-ankle) @ m
            boot = M.boot(bl, bw, bh, shaft=shaft, toe_up=0.018, sides=8)
            sid = b.add(boot.transform(m), "boot_tan", bind, name="slung_boot", hull_pad=True)
            # the outsole: a dark slab under the foot, the heel block deeper
            sole = M.boot(bl * 1.03, bw * 1.06, bh, shaft=0.0, toe_up=0.018, sides=8)
            sole.deform(lambda co: Vector((co.x, co.y, -0.006 + co.z * self.SOLE_K)))
            b.add(sole.transform(m), "boots", bind, name="slung_sole", outline=False, covered_by=sid)
            heel = M.box((bw * 0.78, bl * 0.24, 0.026), (0.0, 0.17 * bl, -0.006), bevel=0.003)
            b.add(heel.transform(m), "boots", bind, name="slung_sole", hull_pad=True)
            # the turned-down top of the shaft and the lace up to the knot
            ct = m @ Vector((0.0, 0.03 * bl, 0.62 * bh + shaft))
            b.add(M.tube([m @ Vector((0.0, 0.03 * bl, 0.62 * bh + shaft - 0.018)), ct + up * 0.004],
                         [0.50 * bw * 1.04, 0.52 * bw * 1.04], sides=8, dome0=0.0, dome1=0.0), "slung_boots_line",
                  bind, name="boot_cuff", outline=False, covered_by=sid)
            lace = M.tube(HB.catmull([ct, ct.lerp(knot, 0.5) + outn * 0.01, knot], 4), [0.0030] * 9, sides=6,
                          dome0=0.0, dome1=0.0)
            b.add(lace, "cord", bind, name="boot_lace", hull_pad=True)

    # relic peddler --------------------------------------------------------------------------------------------
    TRAY = dict(w=0.40, d=0.23, h=0.035, fwd=0.035, z_k=0.26, tilt=16.0)   # size (m), gap before the belly,
    # height (× L), tilt (deg, the front edge down: shown to the buyer — and to the camera; level, its floor lay at a
    # grazing angle to the closeup / portrait cameras and their rim light washed it out behind the relics)

    def tray_tilt(self) -> Matrix:
        """The tray's tilt about its back edge (rest body space)."""
        c, _ = self.tray_frame()
        T = self.TRAY
        pivot = c + Vector((0.0, T["d"] / 2, 0.0))
        return (Matrix.Translation(pivot) @ Matrix.Rotation(math.radians(T["tilt"]), 4, "X")
                @ Matrix.Translation(-pivot))

    def tray_frame(self) -> tuple[Vector, float]:
        """(centre of the tray's floor, front y of the belly) in rest body space."""
        F_ = self
        tp = self.torso
        T = self.TRAY
        z = self.th(self.L * T["z_k"])
        rx, ry, cy, cx = tp.at(z)
        front = cy - ry
        if self.look.belt:
            bp = self.belt_profile()
            rxb, ryb, cyb, _ = bp.at(min(max(z, bp.z0), bp.z1))
            front = min(front, cyb - ryb)
        c = Vector((0.0, front - T["fwd"] - T["d"] / 2, z))
        del F_
        return c, front

    def add_relic_tray(self, b: M.Builder) -> None:
        """A shallow wooden tray before the belly on a strap round the neck, its floor crowded with bone-white
        relics: knuckle bones, a finger bone, a tiny reliquary box, a stoppered phial."""
        U = self.U
        T = self.TRAY
        c, front = self.tray_frame()
        bind = Bind.blend("spine_01", "spine_02", falloff=6.0, smooth=0.03)
        w, d, h = T["w"], T["d"], T["h"]
        tilt = self.tray_tilt()
        b0 = b

        class _Tilted:
            """The builder with every tray part tilted first (the strap below is not)."""
            def add(self_, part, *a, **kw):
                part.transform(tilt)
                return b0.add(part, *a, **kw)

            def __getattr__(self_, k):
                return getattr(b0, k)
        b = _Tilted()
        floor = M.box((w, d, 0.012), c + Vector((0, 0, 0.006)), bevel=0.0)
        fid = b.add(floor, "tray", bind, name="tray", hull_pad=True)
        for (sx, sy, lx, ly) in ((0, -1, w, 0.012), (0, 1, w, 0.012), (-1, 0, 0.012, d), (1, 0, 0.012, d)):
            pos = c + Vector((sx * (w / 2 - 0.006), sy * (d / 2 - 0.006), 0.012 + h / 2))
            b.add(M.box((lx, ly, h), pos, bevel=0.0), "tray", bind, name="tray_rim", hull_pad=True)
        # a black cloth lining (review: bone-white relics on the lit tan floor were faint): the relics on it
        b.add(M.box((w - 0.02, d - 0.02, 0.003), c + Vector((0, 0, 0.0135)), bevel=0.001), "lining", bind,
              name="tray_floor", outline=False, covered_by=fid)
        # reliquary wares, not cartoon bones (review: double-knob dog-bones undercut the grief): bone fragments bound
        # in cloth / wire, a finger bone in a little cage of brass wire, two wax-sealed ossuary boxes, a knuckle on a
        # cord; no skull, no skeleton (story-bible §12). Sizes up for 1080p.
        z0 = c.z + 0.015
        lod = self.lod
        sd = 4 if lod else 6

        def at(x, y, yaw):
            p = c + Vector((x, y, 0.0))
            p.z = z0
            return p, Vector((math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), 0.0))

        def bundle(x, y, yaw, ln, r, wraps, cross=False):
            p, dv = at(x, y, yaw)
            q = p + Vector((0, 0, r))
            a_, e_ = q - dv * ln / 2, q + dv * ln / 2
            b.add(M.capsule(a_, e_, r, r * 0.85, sides=sd, cap_rings=1, mid=1, up=(0, 0, 1)), "relic", bind,
                  name="relic", outline=False, covered_by=fid)
            if cross and not lod:            # LOD1: the crossed pair is one bundle (budget)
                dv2 = Vector((-dv.y, dv.x, 0.0))
                q2 = q + Vector((0, 0, r * 0.9))
                b.add(M.capsule(q2 - dv2 * ln * 0.32, q2 + dv2 * ln * 0.32, r * 0.8, r * 0.7, sides=sd,
                                cap_rings=1, mid=1, up=(0, 0, 1)), "relic", bind, name="relic", outline=False,
                      covered_by=fid)
            for f in wraps:
                m_ = a_.lerp(e_, f)
                side = Vector((-dv.y, dv.x, 0.0))
                ring = [m_ + (side * math.cos(t) + Vector((0, 0, 1)) * math.sin(t)) * (r * 1.08 + 0.0015)
                        + dv * (0.003 * math.sin(t)) for t in np.linspace(0, 2 * math.pi, 6, endpoint=False)]
                b.add(M.sweep(ring, M.superellipse(1, 1, 3, 2.0), scales=[(0.0026, 0.0040)] * 6, closed_path=True,
                              up=tuple(dv), cap0=None, cap1=None), "cord", bind, name="relic_wrap", outline=False,
                      covered_by=fid)

        # 1-2: bone fragments bound in cord (one a crossed pair)
        bundle(-0.115, -0.030, 14.0, 0.090, 0.0105, (0.28, 0.72))
        bundle(0.020, 0.052, -28.0, 0.080, 0.0095, (0.50,), cross=True)
        # 3: the finger bone in a cage of brass wire (end caps + three wires)
        p, dv = at(0.105, -0.045, 4.0)
        q = p + Vector((0, 0, 0.014))
        a_, e_ = q - dv * 0.040, q + dv * 0.040
        b.add(M.capsule(a_ + dv * 0.006, e_ - dv * 0.006, 0.0070, 0.0062, sides=sd, cap_rings=1, mid=1, up=(0, 0, 1)),
              "relic", bind, name="relic", outline=False, covered_by=fid)
        for qq in ((a_, e_) if not lod else ()):        # LOD1: the brass end caps go (budget)
            b.add(M.tube([qq - dv * 0.004, qq + dv * 0.004], [0.0135, 0.0135], sides=sd, dome0=0.002, dome1=0.002),
                  "trim", bind, name="relic", outline=False, covered_by=fid)
        side = Vector((-dv.y, dv.x, 0.0))
        for t in (0.5, 2.6, 4.7):
            off = (side * math.cos(t) + Vector((0, 0, 1)) * math.sin(t)) * 0.0125
            b.add(M.tube([a_ + off, e_ + off], [0.0012, 0.0012], sides=4, dome0=0.0, dome1=0.0), "trim", bind,
                  name="relic_wire", outline=False, covered_by=fid)
        # 4-5: wax-sealed ossuary boxes, tied with cord, a dark wax seal on the knot
        for (x, y, yaw, sx, sy) in ((-0.060, 0.058, 10.0, 0.050, 0.034), (0.150, 0.040, -14.0, 0.042, 0.030)):
            p, dv = at(x, y, yaw)
            rot = Matrix.Translation(p) @ Matrix.Rotation(math.radians(yaw), 4, "Z") @ Matrix.Translation(-p)
            hgt = 0.028
            b.add(M.box((sx, sy, hgt), p + Vector((0, 0, hgt / 2)), bevel=0.0 if lod else 0.002).transform(rot),
                  "relic", bind,
                  name="relic", outline=False, covered_by=fid)
            for bx, by in ((sx + 0.002, 0.0036), (0.0036, sy + 0.002)):
                b.add(M.box((bx, by, hgt + 0.002), p + Vector((0, 0, hgt / 2)), bevel=0.0).transform(rot), "cord",
                      bind, name="relic_wrap", outline=False, covered_by=fid)
            b.add(ell((0.0085, 0.0085, 0.0035), p + Vector((0, 0, hgt + 0.002)), sides=6, rings=3), "stain", bind,
                  name="relic_wax", outline=False, covered_by=fid)
        # 6: a knuckle bone on a cord loop
        p, dv = at(-0.020, -0.062, 0.0)
        b.add(ell((0.016, 0.012, 0.011), p + Vector((0, 0, 0.011)), sides=sd, rings=4), "relic", bind,
              name="relic", outline=False, covered_by=fid)
        loop = [p + Vector((0.016 + 0.028 * (1 - math.cos(t)) * 0.5, 0.022 * math.sin(t), 0.004))
                for t in np.linspace(0.0, 2 * math.pi, 8)]
        b.add(M.tube(loop, [0.0024] * 8, sides=3, dome0=0.0, dome1=0.0), "cord", bind, name="relic_wrap",
              outline=False, covered_by=fid)
        b = b0
        # the neck strap: from the tray's back corners up over the shoulders (cord), round the back of the neck
        tp = self.torso
        L = self.L
        for sg in (1.0, -1.0):
            p0 = tilt @ (c + Vector((sg * (w / 2 - 0.02), d / 2 - 0.01, 0.012 + h)))
            p1 = tp.point(self.th(L - 4.0), sg * 32.0, 1.6 * U)
            p2 = tp.point(self.th(L + 0.4), sg * 48.0, 0.9 * U)
            p3 = tp.point(self.th(L + 0.9), sg * 110.0, 0.5 * U)
            p4 = tp.point(self.th(L + 0.7), sg * 179.0, 0.5 * U)
            path = HB.catmull([p0, p1, p2, p3, p4], 3 if self.lod else 6)
            sb = Bind.custom(["spine_01", "spine_02", "spine_03", "neck_01"], self._strap_w(c.z, self.th(L + 0.8)))
            b.add(M.tube(path, [0.0036] * len(path), sides=4, dome0=0.0, dome1=0.0), "cord", sb, name="tray_strap",
                  hull_pad=True)
        del front

    @staticmethod
    def _strap_w(z0: float, z1: float):
        def fn(co):
            t = np.clip((co[:, 2] - z0) / max(z1 - z0, 1e-6), 0.0, 1.0)
            W = np.zeros((len(co), 4))
            W[:, 0] = (1 - t) ** 2
            W[:, 1] = 2 * t * (1 - t) * 0.6
            W[:, 2] = 2 * t * (1 - t) * 0.4 + t * t * 0.7
            W[:, 3] = t * t * 0.3
            return HB.top4(W)
        return fn

    # pilgrim --------------------------------------------------------------------------------------------------
    def add_wrist_rope(self, b: M.Builder, rig: R.Rig, side: str) -> None:
        """Rope coiled round the forearm above the wrist, a cut end hanging from the last coil."""
        J = rig.joints
        g, U = self.g, self.U
        E, W = J[f"elbow_{side}"], J[f"wrist_{side}"]
        dla = (W - E).normalized()
        k = 0.92
        r_e, r_w = 2.5 * g * U * k, 2.15 * g * U * k
        bind = Bind.blend(f"lowerarm_{side}", f"hand_{side}", falloff=8.0)
        perp = dla.orthogonal().normalized()
        perp2 = dla.cross(perp).normalized()
        last = None
        rid = None
        for i, f in enumerate((0.70, 0.79, 0.88)):
            c = E.lerp(W, f)
            r = lerp(r_e, r_w, f) * 1.07 + 0.0035
            pts = [c + (perp * math.cos(t) + perp2 * math.sin(t)) * r + dla * (0.004 * math.sin(t))
                   for t in np.linspace(0, 2 * math.pi, 14, endpoint=False)]
            coil = M.sweep(pts, M.superellipse(1, 1, 6, 2.0), scales=[(0.0036, 0.0036)] * 14, closed_path=True,
                           up=tuple(dla), cap0=None, cap1=None)
            rid = b.add(coil, "cord", bind, name="wrist_rope", hull_pad=True)
            last = (c, r)
        c, r = last
        down = Vector((0, 0, -1))
        outv = (down - dla * down.dot(dla)).normalized() if abs(down.dot(dla)) < 0.95 else perp
        p0 = c + outv * r
        end = [p0, p0 + outv * 0.012 + dla * 0.012, p0 + outv * 0.030 + dla * 0.020, p0 + outv * 0.050 + dla * 0.022]
        eid = b.add(M.tube(end, [0.0036, 0.0036, 0.0034, 0.0042], sides=6, dome0=0.0, dome1=0.003), "cord", bind,
                    name="rope_end", hull_pad=True)
        del rid, eid

    # bern -----------------------------------------------------------------------------------------------------
    def add_earth_stains(self, b: M.Builder, sid: int | None) -> None:
        """Grave earth on Old Bern's coat (review: the A1 maroon read as wine red, not an earth-stained coat — the
        colour is A1's, so the stains take more of it): the hem caked dark all round with a ragged top edge, mud
        splashed up the front and sides to the thighs in his trousers' earth brown, smears where the handles rub."""
        if not self.skirted:
            return
        bind = Bind.custom(self.skirt_bones(), self.skirt_weights)
        t = self.SKIRT_THICK / 2
        kw = {"covered_by": sid} if sid is not None else {}

        def patch(a, k0, w, hgt, jag_k, region, lift, cols=8, rows=3, seed=0.0):
            outer, inner = [], []
            for r in range(rows + 1):
                v = r / rows
                ro, ri = [], []
                for c in range(cols + 1):
                    u = c / cols * 2 - 1
                    jag = 0.75 + 0.25 * math.sin(5.0 * u + seed) * (1.0 - v) + 0.12 * math.sin(13.0 * u + 2 * seed)
                    kk = min(0.995, k0 + hgt * (v * jag) * jag_k if hgt < 0 else k0 + hgt * v * jag)
                    aa = a + u * w * (0.6 + 0.4 * v)
                    ro.append(self.skirt_point(kk, aa, t + lift + 0.0010))
                    ri.append(self.skirt_point(kk, aa, t + lift))
                outer.append(ro)
                inner.append(ri)
            b.add(C.thick_patch(outer, inner), region, bind, name="stain", outline=False, **kw)
        # the caked hem, all round (six overlapping bands, top edge ragged)
        for i, a in enumerate(range(-150, 180, 60)):
            patch(float(a), 0.72 + 0.04 * math.sin(i * 1.9), 34.0, 0.29, 1.0, "stain", 0.0006, cols=6, rows=2,
                  seed=i * 1.3)
        # splashes up the front and the sides in the trousers' earth brown
        for i, (a, k0, w, hgt) in enumerate(((-14.0, 0.40, 13.0, 0.26), (24.0, 0.46, 11.0, 0.22),
                                              (-58.0, 0.48, 10.0, 0.22), (64.0, 0.42, 12.0, 0.25),
                                              (150.0, 0.50, 16.0, 0.22), (-120.0, 0.55, 10.0, 0.18),
                                              (2.0, 0.20, 7.0, 0.14))):
            patch(a, k0, w, hgt, 1.0, "legs", 0.0009, cols=4, rows=2, seed=2.0 + i)

    def add_barrow_wheel(self, b: M.Builder) -> None:
        """The barrow's wheel, built into Bern's mesh rigid on ``barrow_wheel_01`` (``march_props.wheel_parts``):
        the bone spins it at stride speed in Walk / Run; the rest of the barrow is the rigid prop on ``barrow``."""
        import march_props as MPR
        ax = MPR.barrow_axle()
        bind = Bind.rigid("barrow_wheel_01")
        reg = {"iron": "wheel_iron", "wood": "wheel_wood", "wood_dark": "wheel_dark"}
        wid = None
        for nm, part, region, hull in MPR.wheel_parts(getattr(self, "lod", 0)):
            part.translate(ax)
            if hull:
                wid = b.add(part, reg[region], bind, name=nm, hull_pad=True)
            else:
                b.add(part, reg[region], bind, name=nm, outline=False, covered_by=wid)

    # chaplain ---------------------------------------------------------------------------------------------------
    def add_skirt(self, b: M.Builder) -> int:
        sid = super().add_skirt(b)
        if self.has("threadbare"):
            self.add_threadbare(b, sid)
        return sid

    def add_threadbare(self, b: M.Builder, sid: int) -> None:
        """Brother Aldwin's threadbare cassock (review: plain, smooth black): a frayed hem — ragged tabs hanging
        below it and torn notches in it — and two patches in the cassock's shade tone sewn on with the rope's tan."""
        bind = Bind.custom(self.skirt_bones(), self.skirt_weights)
        t = self.SKIRT_THICK / 2
        rng = np.random.default_rng(17)
        # ragged tabs below the hem
        for i, a in enumerate(np.linspace(-165.0, 165.0, 10)):
            a = float(a + 8.0 * (rng.random() - 0.5))
            w = 3.5 + 3.0 * rng.random()
            ln = 0.035 + 0.04 * rng.random()
            outer, inner = [], []
            for r in range(3):
                v = r / 2
                ro, ri = [], []
                for c in range(3):
                    u = c / 2 * 2 - 1
                    ww = w * (1.0 - 0.75 * v) * (1 if c != 1 else 1)
                    kk = 0.985 + ln / max(self.skirt_z0 - self.skirt_z1, 1e-3) * v * (1.0 - 0.35 * abs(u))
                    ro.append(self.skirt_point(kk, a + u * ww, t + 0.0004))
                    ri.append(self.skirt_point(kk, a + u * ww, -t - 0.0004))
                outer.append(ro)
                inner.append(ri)
            b.add(C.thick_patch(outer, inner), "top", bind, name="fray", hull_pad=True)
        # torn notches in the hem (dark), between the tabs
        for a in np.linspace(-155.0, 165.0, 9):
            outer, inner = [], []
            for r in range(3):
                v = r / 2
                ro, ri = [], []
                for u in (-1.0, 0.0, 1.0):
                    kk = lerp(0.93, 1.0, v) if u == 0.0 else lerp(0.975, 1.0, v)
                    ww = 2.2 * v
                    ro.append(self.skirt_point(kk, float(a) + u * ww, t + 0.0016))
                    ri.append(self.skirt_point(kk, float(a) + u * ww, t + 0.0006))
                outer.append(ro)
                inner.append(ri)
            b.add(C.thick_patch(outer, inner), "top_shade", bind, name="fray_notch", outline=False, covered_by=sid)
        # two sewn patches: the front right of the skirt (clear of the rope's ends), the left side below the hip
        for a, k, wa, hk in ((-34.0, 0.52, 17.0, 0.17), (72.0, 0.30, 14.0, 0.15)):
            outer, inner = [], []
            for r in range(4):
                v = r / 3
                ro, ri = [], []
                for c in range(5):
                    u = c / 4 * 2 - 1
                    kk = k + (v - 0.5) * hk
                    ro.append(self.skirt_point(kk, a + u * wa * 0.5, t + 0.0022))
                    ri.append(self.skirt_point(kk, a + u * wa * 0.5, t + 0.0008))
                outer.append(ro)
                inner.append(ri)
            b.add(C.thick_patch(outer, inner), "top_shade", bind, name="patch", outline=False, covered_by=sid)
            # running stitches round its border
            for j in range(10):
                f = j / 10
                if f < 0.25:
                    u, v = -1 + 8 * f, 0.0
                elif f < 0.5:
                    u, v = 1.0, (f - 0.25) * 4
                elif f < 0.75:
                    u, v = 1 - 8 * (f - 0.5), 1.0
                else:
                    u, v = -1.0, 1 - (f - 0.75) * 4
                kk = k + (v - 0.5) * hk * 0.92
                aa = a + u * wa * 0.46
                p0 = self.skirt_point(kk - 0.006, aa, t + 0.0026)
                p1 = self.skirt_point(kk + 0.006, aa + 0.6, t + 0.0026)
                b.add(M.tube([p0, p1], [0.0012, 0.0012], sides=3, dome0=0.0, dome1=0.0), "belt", bind,
                      name="patch_stitch", outline=False, covered_by=sid)

    def add_arm(self, b: M.Builder, rig: R.Rig, side: str, hand: bool = True) -> None:
        if not self.has("threadbare"):
            return self._add_arm_march(b, rig, side, hand)
        self._add_arm_march(b, rig, side, hand)
        # frayed cuff: ragged tabs round the sleeve end and a torn notch (review: nothing threadbare about it)
        J = rig.joints
        g, U = self.g, self.U
        E, W = J[f"elbow_{side}"], J[f"wrist_{side}"]
        dla = (W - E).normalized()
        r_w = 2.15 * g * U * 0.92 * 1.16
        perp = dla.orthogonal().normalized()
        perp2 = dla.cross(perp).normalized()
        bind = Bind.blend(f"lowerarm_{side}", f"hand_{side}", falloff=6.0)
        aid = self.part_id(b, f"arm_{side}")
        for k, th0 in enumerate((0.3, 1.5, 2.6, 3.9, 5.2)):
            outer, inner = [], []
            for r in range(3):
                v = r / 2
                ro, ri = [], []
                for u in (-1.0, 0.0, 1.0):
                    th = th0 + u * 0.22 * (1.0 - 0.7 * v)
                    d = perp * math.cos(th) + perp2 * math.sin(th)
                    c = W - dla * 0.004 + dla * (0.010 + 0.006 * (k % 2)) * v
                    ro.append(c + d * (r_w + 0.0016))
                    ri.append(c + d * (r_w - 0.0016))
                outer.append(ro)
                inner.append(ri)
            b.add(C.thick_patch(outer, inner), "top", bind, name="fray", hull_pad=True)
        del aid

    def _add_arm_march(self, b, rig, side, hand):
        """The march sleeves (rolled sleeves / gloves) or the plains arm."""
        if not self.has("rolled_sleeves"):
            Folk.add_arm(self, b, rig, side, hand=False)
            if hand:
                self.add_hand(b, rig, side)
            return
        return self._add_rolled_arm(b, rig, side, hand)

    # tamsin -----------------------------------------------------------------------------------------------------
    def add_keyring(self, b: M.Builder, ids: dict) -> None:
        if not self.has("crypt_keys"):
            return super().add_keyring(b, ids)
        # the gravewarden's crypt keys (review: small at the game camera): a big ring at the LEFT hip (her spade
        # hand is the right), three long iron-age keys with heavy bows and bits, each with its own unpadded hull
        U = self.U
        bp = self.belt_profile()
        a = 60.0
        z = self.th(0.5)
        n = bp.normal(z, a)
        n = Vector((n.x, n.y, 0.0)).normalized()
        t = Vector((0.0, 0.0, 1.0)).cross(n).normalized()
        down = Vector((0.0, 0.0, -1.0))
        kb = Bind.blend("pelvis", "thigh_l", falloff=6.0)
        p0 = bp.point(self.th(1.4), a, 0.30 * U)
        p1 = bp.point(self.th(0.2), a, 0.55 * U)
        p2 = p1 + down * 1.4 * U + n * 0.35 * U
        tab = M.sweep(HB.catmull([p0, p1, p2], 4), M.rect_profile(0.0050, 1.3 * U), up=tuple(n), cap0="ngon",
                      cap1="ngon")
        b.add(tab, "leather", kb, name="key_tab")
        yaw = math.radians(50.0)
        w = (t * math.cos(yaw) + n * math.sin(yaw)).normalized()
        rr = 1.9 * U
        top = p2 + down * 0.15 * U
        c = top + down * rr
        ring = M.sweep([c + (w * math.sin(th) - down * math.cos(th)) * rr
                        for th in np.linspace(0, 2 * math.pi, 18, endpoint=False)],
                       M.superellipse(1, 1, 6, 2.0), scales=[(0.30 * U, 0.30 * U)] * 18, closed_path=True,
                       up=tuple(w.cross(down)), cap0=None, cap1=None)
        b.add(ring, "brass", kb, name="keyring")
        for k, (fan, ln) in enumerate(((-26.0, 6.0), (0.0, 6.8), (24.0, 5.4))):
            th = math.radians(200.0 + 70.0 * k)          # hung round the ring's lower half
            hang = c + (w * math.sin(th) - down * math.cos(th)) * rr
            d = (down * math.cos(math.radians(fan)) + t * math.sin(math.radians(fan))).normalized()
            bow_c = hang + d * 0.95 * U
            bow = M.sweep([bow_c + (d.cross(n).normalized() * math.sin(q) + d * math.cos(q)) * 0.85 * U
                           for q in np.linspace(0, 2 * math.pi, 12, endpoint=False)], M.superellipse(1, 1, 6, 2.0),
                          scales=[(0.26 * U, 0.30 * U)] * 12, closed_path=True, up=tuple(n), cap0=None, cap1=None)
            b.add(bow, "brass", kb, name="key_bow")
            s0 = bow_c + d * 0.80 * U
            s1 = s0 + d * ln * U
            b.add(M.tube([s0, s1], [0.30 * U, 0.27 * U], sides=6, dome0=0.0, dome1=0.0), "brass", kb,
                  name="key_shaft")
            side = d.cross(n).normalized() * (1.0 if k != 1 else -1.0)
            for j, (f, wd) in enumerate(((0.92, 1.5), (0.80, 1.05))):
                bit_c = s0.lerp(s1, f) + side * (0.55 * wd * U)
                bit = M.box((wd * U, 0.55 * U, 0.30 * U), Vector((0, 0, 0)), bevel=0.0012)
                bit.transform(R.frame(bit_c, d, n) @ Matrix.Rotation(math.radians(90.0), 4, "Y"))
                b.add(bit, "brass_dark", kb, name="key_bit")

    # pilgrim ----------------------------------------------------------------------------------------------------
    def add_pilgrim_cloak(self, b: M.Builder) -> None:
        """The Bound Pilgrim's dusty cloak (review: a short olive capelet read as a green bib): over the shoulders
        and down the back to mid-back, open in front, its hood pushed back and bunched round the back of the
        neck."""
        mp = self.mantle_profile()
        L, U = self.L, self.U
        bind = HB.blend4(self.rig, "spine_01", "spine_02", "spine_03", "clavicle_l", "clavicle_r", falloff=5.0,
                         smooth=0.02)
        z_lo_ring = min(mp.z0, mp.z1)

        def hem_h(a):
            d = abs(((a + 180.0) % 360.0) - 180.0)
            # the open front edges hang as long as the sides (review 11, round 2: front panels ending at the collar
            # still read as a bib from the game camera), the back to mid-back
            hh = lerp(L - 5.8, L - 6.6, smooth01((d - 50.0) / 40.0))
            return lerp(hh, L - 10.0, smooth01((d - 95.0) / 70.0))

        def pt(h, a, out):
            z = self.th(h)
            zz = max(z, z_lo_ring)
            p = mp.point(zz, a, out)
            if z < zz:
                # below the profile the cloth hangs plumb, drawing in a touch at the back
                p = Vector((p.x * (1.0 - 0.10 * (zz - z) / 0.3), p.y - 0.02 * (zz - z) / 0.3, z))
            return p
        lod = getattr(self, "lod", 0)
        cols, rows = (18, 4) if lod else (26, 6)
        a0, a1 = 50.0, 310.0              # open in front: the robe shows between the edges
        h_top = L + 1.4
        outer, inner = [], []
        for r in range(rows + 1):
            v = r / rows
            ro, ri = [], []
            for c in range(cols + 1):
                a = lerp(a0, a1, c / cols)
                a = a - 360.0 if a > 180.0 else a
                h = lerp(h_top, hem_h(a), v)
                fold = 0.18 * U * math.sin(math.radians(a) * 7.0) * smooth01(v / 0.5)
                ro.append(pt(h, a, 0.55 * U + 0.006 + fold))
                ri.append(pt(h, a, 0.55 * U + fold))
            outer.append(ro)
            inner.append(ri)
        cid = b.add(C.thick_patch(outer, inner, sub_tags=True), "mantle", bind, name="mantle",
                    sub_regions={1: "mantle", 2: "mantle_line", 0: "mantle"}, hull_pad=True)
        # the frayed, dusty hem: a darker band along the bottom edge
        band_o = [[pt(lerp(h_top, hem_h(lerp(a0, a1, c / cols) - (360.0 if lerp(a0, a1, c / cols) > 180 else 0)), vv),
                      lerp(a0, a1, c / cols) - (360.0 if lerp(a0, a1, c / cols) > 180 else 0), 0.55 * U + 0.0072)
                   for c in range(cols + 1)] for vv in (0.90, 1.0)]
        band_i = [[q - (q - Vector((0, 0, q.z))).normalized() * 0.0010 for q in row] for row in band_o]
        b.add(C.thick_patch(band_o, band_i), "mantle_line", bind, name="mantle_edge", outline=False, covered_by=cid)
        # the hood, pushed back and bunched round the back of the neck: a fat soft roll with two folds
        J = self.rig.joints
        nz = self.th(L + 0.9)
        pts, rads = [], []
        for a in np.linspace(-118.0, 118.0, 11):
            aa = 180.0 + float(a)
            aa = aa - 360.0 if aa > 180.0 else aa
            k = 1.0 - abs(a) / 118.0
            pts.append(mp.point(nz + 0.014 * k, aa, (1.1 + 1.5 * k) * U))
            rads.append((1.1 + 1.5 * k) * U)
        hood = M.tube(pts, rads, sides=8, dome0=0.6 * U, dome1=0.6 * U)
        hid = b.add(hood, "mantle", Bind.blend("spine_03", "neck_01", "clavicle_l", "clavicle_r", falloff=5.0),
                    name="hood_bunch", hull_pad=True)
        for a in (-40.0, 40.0):
            aa = 180.0 + a
            aa = aa - 360.0 if aa > 180.0 else aa
            c0 = mp.point(nz + 0.02, aa, 2.4 * U)
            b.add(M.tube([c0 + Vector((0, 0, 0.02)), c0 - Vector((0, 0, 0.025))], [0.15 * U, 0.15 * U], sides=4,
                         dome0=0.0, dome1=0.0), "mantle_line",
                  Bind.blend("spine_03", "neck_01", falloff=5.0), name="crease", outline=False, covered_by=hid)
        del J

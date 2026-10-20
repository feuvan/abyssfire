"""尸牧婆 the Corpseherd — ``SK_Mon_Corpseherd`` (dark remake A6; monsters.md §2.5, chapter1.md §10, story-bible §5).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/corpseherd.py
        [--quick] [--anims Clip … --facing=se] [--portrait] [--no-previews] [--no-verify]

*A washerwoman from the crusade's baggage train whose two sons died on the field. She keeps the dead walking in
circles with a bell tied to a crook, leading them "home".* The Chapter-1 mini-boss (raiser, M3, the Sheepfold) as a
**re-dress of the goblin shaman** (monsters.md §5: ``goblin_shaman.py`` through this file; that file is not edited):
the shaman's ``SKEL_Goblin`` rig (same bone set: the unused union chains are re-placed, never added), its skirt
envelope (``goblin_shaman.ShamanAnims``, extended in :class:`HerdAnims`) and clip machinery, dressed in a human
palette. Size: monsters.md's "×1.3 (≈ 1.21 m)" is built into the mesh at ×1.5 the shaman's rig unit (``SCALE``;
manifest ``scale`` 1.0 + ``scaleNote``: the core sets no visual scale) so that the **stooped READY hood top** — the
height she is seen at — measures ≈ 1.20 m (manifest heightCm, the rig's unstooped crown, 129.7). At ×1.3 she measured
heightCm 112 / hood top 104 cm and read child-sized under the hood next to the 1.76 m footman (art review r1); ×1.4
(heightCm 121) still left the hood top at 112 cm — the review's "≈ ×1.4" read heightCm as the stooped height. The
file's metre constants are authored at the ×1.3 design size and scaled by ``GROW`` (the crook and the bell keep
theirs). Nothing of the goblin shows:

* **head** — no skull, ears or face are built: a **deep cowl** (``corpseherd.hood`` #2A2624; a round crown — a peak
  over a red skirt read as an executioner — the brow overhanging the face by ≈ 4 cm, the lower rows flaring onto the
  shoulders, painted cloth creases so the lit crown never reads as a helmet from the W1 camera) over a near-black
  void (``corpseherd.face_void``) in which only the **violet pinpoint eyes** (``corpseherd.eye`` #D08CFF, the family
  marker, emissive) burn; a **widow's veil** (:class:`VeilSurf`, in the hood's darker value ``corpseherd.hood``)
  hanging from the bridge of the nose like a cloth: three soft vertical folds deepening toward a ragged hem with a
  few longer tatters, its top in the brow's shadow, draped onto the chin's point (``corpseherd.chin``) and falling past
  it to the hood's lower lip (art review r1: a short veil over a dark gap and a grey crescent read as a visor over an
  open mouth — the chin now reads only through the cloth);
* **body** — an old woman's stoop: the rest hunch + ``READY_LEAN``, the neck carried forward (``head_tilt`` −.20) so
  the hood hangs in front of a **dowager's hump** (``HUMP``, under the shawl); a narrow, flat trunk (no goblin
  pot-belly or deltoids); a **shawl** of rags (``corpseherd.rags``) snug over the shoulders, high at the sides (the
  arms come out under it), a point down the back with a crusade-banner patch (crimson, tarnished trim, the scorched
  open palm); a **long skirt patched from the banners**: rag panels and three irregular crimson gores
  (``corpseherd.skirt`` #5E1012; ``GORES`` — se shows the front + right, sw the front + left), soft folds deepening
  toward the hem, a trailing back hem, an ash-dark scorched hem band; a rope belt; worn shoes; thin legs (hidden);
* **hands** — long-fingered, ashen (``corpseherd.hands`` #A89A8C): the right hand wrapped round the crook, the free
  left hand held out over the flock, its long fingers drooping (``curl_l`` on ``fingers_01_l``: the hush in Taunt
  brings it up to the veil, the palm to her lips);
* **the crook** ``SM_MonWeapon_Crook`` — a shepherd's crook of pale ash-wood (``crook.wood`` #D9CEB4) rising above her
  hood (≈ 1.4 m at the hook), knotted, the grip wound with cord, rolled in the fist so the hook opens outward
  (``CROOK_ROLL``); the **cracked bronze handbell** (``crook.bell`` #8A6A3A, ×1.45 so it reads at the W1 camera; a crack
  and a chipped lip, verdigris) hangs on a cord from the hook's end — part of the skeletal mesh on
  ``totem_feather_01`` (a child of ``weapon_r``), a pendulum the clips swing (``bell`` / ``bell_side``); socket
  ``fx_bell`` (its mouth) is where the bell-toll bolt and the ``FX_bell_ring`` ripple start; a torn banner rag is
  knotted on the shaft (``totem_feather_02``).

Colour: only registered A1 ``Monsters_March`` swatches (``corpseherd.*``, ``crook.*`` + shared ``unburied.*`` /
``clapper.*``) through :class:`gnawers.MarchBuilder`, so a build never appends or moves a swatch. Marker: the violet
eyes (+ runtime ``fx.eyeGlow`` cards, with the Corpseherd's own ``EYE_FADE`` and depth-tested, so a turned-away hood
shows no violet glint over its crown); no ember anywhere (the bronze is hue 36°, sat .58). Ink: the monster class with
the Chapter-1 character request of 3.5 px (gate 3 px). Portrait: :func:`portrait` (framed round the eyes; the eyes
drawn as small #D08CFF points with an open violet halo).

Clips (speed 45 → 1.35 m/s; attack every 1300 ms, range 3): Idle 1600 (breathing, a faint rocking, the bell swaying),
Walk 560 (a short shuffle under the long skirt, the crook carried upright), **Attack01** 1000 (the bell-toll bolt
[W1]: the crook hauled back, then swung out at the target so the bell is flung forward and tolls — ``WindupEnd`` 310,
``Contact`` = ``Release`` = ``FX_BellToll`` 500), **Cast01** 1300 (召羊 Call the Flock [W2]: she rises out of her stoop,
the crook raised high and shaken, the free hand over the flock — ``FX_bell_ring`` + ``Release`` 650), **Taunt** 2000
(the pre-fight lines, a seamless loop: swaying from hip to hip as if lulling a child, the long hand brought up to the
veil, palm to her lips — hush — for a third of the loop), Hurt 300, Death 1400 (she sinks onto her knees, the skirt
pooling on the floor, and folds forward over her knees, the hood bowed low over her right hand; the crook tips over
and slides through her fist as it falls, so it lies along her right side with the fist closed under the hook and the
bell on the ground against her knuckles — ``cs_rf_herd``: "her hand still on the bell"), Stun 700. Cloth × limb QA (``QA_PAIRS``, limit 0 on every frame): skirt × legs, shawl × forearms / cuffs / hands,
hood × the free hand, crook × body, crook × bell, ground.
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "march", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import mesh as M, outline, rig as R, scene, shading  # noqa: E402
from kit.paths import scratch_root  # noqa: E402
from kit.palette import Palette  # noqa: E402
from kit.rig import Bind  # noqa: E402

import common as C  # noqa: E402
from common import BP, K, V  # noqa: E402
import gnawers as GN  # noqa: E402
import goblins as GB  # noqa: E402
import goblin_shaman as GS  # noqa: E402
import mgeo as G  # noqa: E402
import unburied as UB  # noqa: E402

ASSET = "SK_Mon_Corpseherd"
TOKEN = "Mon_Corpseherd"
CROOK = "SM_MonWeapon_Crook"
PREFIX = "corpseherd"
GAME_IDS = ["monster_miniboss_corpseherd", "miniboss_corpseherd"]    # spriteKey, id (Data/monsters.json)
PORTRAIT = "T_UI_Portrait_Corpseherd"
DISPLAY = {"zh-CN": "尸牧婆", "en": "the Corpseherd"}
SCALE = 1.5                                    # the shaman's unit × 1.5, built into the mesh (manifest scale 1.0):
                                               # monsters.md §2.5 asks ×1.3 "(≈ 1.21 m)" — measured on the stooped
                                               # READY hood top: 104 cm at ×1.3 (child-sized under the hood next to
                                               # the 176 cm footman, art review r1), 112.4 at ×1.4, 120.4 at ×1.5
DESIGN_SCALE = 1.3                             # the metre constants below (hood, veil, skirt, shawl, free hand,
GROW = SCALE / DESIGN_SCALE                    # shoes, every pose target) are authored at ×1.3 and scaled by GROW;
                                               # the crook and the bell (an item) keep their size
U = GS.WEB.U * SCALE                           # m per rig unit
BLOB_R = 0.30 * GROW                           # web contact shadow radius (m)
WEB = GB.Web(U=U, thigh=9.0, shin=9.4, upper_arm=8.8, fore_arm=9.0, torso=13.5, neck=5.0, ankle=1.6, hip=2.4,
             shoulder=3.9, lean=0.42, head_tilt=-0.20, root=(46.0, 73.5), bulk=0.92, head_k=0.6, foot_len=7.0,
             grin=0.0)
LOOK = GB.Look()                               # unused (the march swatches come from ALIAS)
HEIGHT = 1.21                                  # framing height ≈ the hood top of the stooped READY (measured
                                               # 120.4 cm; manifest heightCm = the rig's unstooped crown, 129.7)
REF_SPEED = 45 * 0.03                          # monsters.md §2.0: speed 45 (tiles/s = speed × 0.03)
OUTLINE_CLASS = GN.OUTLINE_CLASS
OUTLINE_PX = GN.OUTLINE_PX
OUTLINE_PX_NOTE = ("Chapter-1 character gate (dark rim >= 3 px on >= 90 % of the 1080p silhouette edges): the "
                   "monster class's 3.0 px keeps the crook's shaft and hook, the bell cord and the long fingers < 3 px "
                   "on part of the silhouette; 3.5 px, as every Chapter-1 character ships, holds it (gate 3.0 px); "
                   "the crook inks with it")
MARKER = ("corpseherd.eye",)
LOD0_BUDGET, LOD1_BUDGET = 9000, 3400          # monsters.md §5 (the shaman's budget)

# region key → registered Monsters_March swatch (A1, march/march_palettes.py)
ALIAS = dict(
    skin="corpseherd.rags", belly="corpseherd.rags",          # kit generators' keys: the bodice, the stockings
    rags="corpseherd.rags", rags_in="corpseherd.hood", crease="corpseherd.face_void", hood="corpseherd.hood",
    hood_edge="corpseherd.veil", veil="corpseherd.hood", banner="corpseherd.skirt",
    hands="corpseherd.hands", chin="corpseherd.chin", void="corpseherd.face_void", eye="corpseherd.eye",
    char="unburied.tabard_char", sigil="unburied.sigil", trim="unburied.trim", rope="unburied.rope",
    rope_line="unburied.gambeson_line", stitch="unburied.gambeson_line", shoe="unburied.leather_dark",
    bronze="crook.bell", bronze_dark="clapper.bronze_dark", patina="clapper.patina", cord="crook.cord",
)
CROOK_ALIAS = dict(wood="crook.wood", wood_line="crook.wood_line", cord="crook.cord", knot="crook.wood_line")

# ── the crook (weapon_r space, m: grip at the origin, +Y up the shaft, +Z = forward when held upright) ───────
CROOK_DOWN = 0.42              # grip → butt (the grip sits ≈ 7 cm under the wrist target)
CROOK_UP = 0.70                # grip → the hook's spring
HOOK_R = 0.066                 # hook radius (the curl opens forward, +Z)
HOOK_END = 222.0               # deg round the hook circle where the curl ends (0 = its spring off the shaft)
SHAFT_R = (0.0135, 0.0118)     # shaft radius at the butt / at the hook
GRIP_R = 0.0142                # shaft radius under the fist (the cord wrap)
BELL_CORD = 0.035              # cord from the hook's end to the bell's handle loop
BELL_K = 1.45                  # handbell scale (art review: at 8 cm the bell was a 6 px speck at the W1 camera)
BELL_H, BELL_R = 0.082 * BELL_K, 0.041 * BELL_K  # handbell height (crown → lip) and lip radius


def hook_point(phi_deg: float, r: float = HOOK_R) -> Vector:
    """Point on the hook's centre line, ``phi`` degrees round from its spring off the shaft top."""
    p = math.radians(phi_deg)
    return Vector((0.0, CROOK_UP + r * math.sin(p), r - r * math.cos(p)))


BELL_TIE = hook_point(HOOK_END) + Vector((0.0, -0.006, 0.004))   # the cord's knot under the hook's end


# ── head (head frame, m: lat + left, fwd, up from the head centre) ──────────────────────────────────────────
# hood rows: (up, lateral radius, front radius, back radius, half-angle of the face opening in deg). A deep, soft
# cowl: a round crown (no peak: a pointed hood over a red skirt reads as an executioner), the brow overhanging the
# face by ≈ 4 cm so the inside stays black, and the lower rows flaring out onto the shoulders (the cowl falls into
# the shawl).
HOOD_ROWS = [(-0.182, 0.130, 0.104, 0.122, 14.0), (-0.150, 0.112, 0.111, 0.118, 28.0),
             (-0.102, 0.097, 0.117, 0.114, 56.0), (-0.058, 0.096, 0.121, 0.113, 60.0),
             (-0.014, 0.096, 0.124, 0.112, 60.0), (0.022, 0.094, 0.125, 0.110, 55.0),
             (0.048, 0.090, 0.119, 0.106, 40.0), (0.068, 0.083, 0.111, 0.100, 0.0),
             (0.084, 0.072, 0.101, 0.090, 0.0), (0.097, 0.057, 0.081, 0.076, 0.0),
             (0.106, 0.038, 0.056, 0.056, 0.0), (0.112, 0.019, 0.029, 0.031, 0.0),
             (0.114, 0.0015, 0.002, 0.003, 0.0)]
HOOD_THICK = 0.011
HOOD_EXP = 2.3                 # superellipse exponent of the hood rows (a soft box: cloth over a skull)
VOID_C, VOID_R = (0.0, -0.006, -0.022), (0.076, 0.088, 0.100)    # the dark inside (centre, radii)
EYE = (0.028, 0.004)           # (lat, up) of each eye on the void's front
EYE_R = (0.019, 0.0155, 0.0075)  # eye half extents (across, up, deep): the marker reads as a 3 px block at the W1
                               # camera past the hood's brow (darkgate: .0125 across gave 1 px, .0145 up 2 px)
EYE_OUT = 0.009                # eye centre in front of the void's surface (m): out from under the brow's shadow
EYE_DROOP = 12.0               # deg the outer corners droop (grief, not anger)
VEIL_TOP = -0.012              # veil top edge (under the eyes)
VEIL_HEM = (-0.178, -0.165)    # the hem at the centre (past the chin, at the hood's lower lip) / at the sides (tucked
                               # into the hood's cheeks)
VEIL_SPAN = 58.0               # deg round the face either side (tucked under the hood's cheeks)
VEIL_FOLDS = (40.0, 0.0095)    # (deg per fold round the face: three across the front, depth m at the hem)
VEIL_VALLEYS = ((-16.0, 0.07, 0.95), (24.0, 0.12, 0.78))   # fold valleys darkened by a painted line (deg, f from, to)
VEIL_ON_CHIN = 0.0012          # m the veil stands off the chin's point (resting on it)
VEIL_LEAN = 0.012              # m the curtain leans in toward the face from its top edge to the hem (sides)
VEIL_TONGUES = (2, 5, 15, 18)  # hem columns (of 21) with a longer tatter
CHIN = ((0.0, 0.066, -0.130), (0.0110, 0.0115, 0.0105))        # thin grey chin (centre, radii): the jaw's point
                                                                # under the veil (a wide one read as a grin)


class CorpseherdKit(GB.GoblinKit):
    """``goblins.GoblinKit`` (the shaman's rig) dressed as the Corpseherd: no goblin head is built; the hood, void,
    eyes, veil and chin live in the head frame (``hm``), the bell and the crook rag hang on ``totem_feather_*``."""

    @staticmethod
    def torso_hide(s: float, a: float) -> bool:
        """Bodice under the shawl (above its hem) and under the skirt's waist."""
        back = abs(a) > 110.0
        return s > (0.48 if back else 0.64) or s < 0.07

    def torso_rings(self):
        """A narrow, flat-chested old woman's trunk (the goblin's pot-belly removed): (s, lat, depth, centre fwd)."""
        u = self.u
        return [(-0.16, u(3.7), u(3.3), u(0.0)), (-0.05, u(4.7), u(4.2), u(0.2)), (0.10, u(5.0), u(4.5), u(0.3)),
                (0.28, u(4.6), u(4.2), u(0.4)), (0.45, u(4.4), u(4.0), u(0.3)), (0.62, u(4.5), u(4.0), u(0.1)),
                (0.78, u(4.6), u(3.9), u(-0.3)), (0.90, u(4.3), u(3.5), u(-0.6)), (0.98, u(3.5), u(2.8), u(-0.6)),
                (1.05, u(2.3), u(2.1), u(-0.3))]

    # ── head frame ─────────────────────────────────────────────────────────────────────────────────────
    def hm(self, lat: float, fwd: float, up: float) -> Vector:
        """Head-frame point (the head constants are authored at the ×1.3 design size: scaled by ``GROW``)."""
        return self.Hm @ (Vector((lat, -fwd, up)) * GROW)

    def hdirm(self, lat: float, fwd: float, up: float) -> Vector:
        return (self.Hm.to_3x3() @ Vector((lat, -fwd, up))).normalized()

    def eye_center(self, sg: int) -> Vector:
        lat, up = EYE
        return self.hm(sg * lat, self.void_front(sg * lat, up) + EYE_OUT, up)

    @staticmethod
    def void_front(lat: float, up: float) -> float:
        """Forward extent of the dark void at (lat, up) (head frame)."""
        cx, cy, cz = VOID_C
        rx, ry, rz = VOID_R
        k = 1.0 - ((lat - cx) / rx) ** 2 - ((up - cz) / rz) ** 2
        return cy + ry * math.sqrt(max(k, 0.0))

    # ── chains ─────────────────────────────────────────────────────────────────────────────────────────
    def loin_points(self, sg: int):
        """The skirt's front / back chains: from the waist to the hem (the long skirt; the goblin's are 8 u)."""
        a = 0.0 if sg > 0 else 180.0
        top = self.torso.point(0.06, a, self.u(0.9)) + Vector((0, 0, -self.u(0.6)))
        bot = Vector((0.0, top.y - sg * 0.06 * GROW, SKIRT_HEM_Z))
        return top, top.lerp(bot, 0.5) + Vector((0, -sg * 0.02 * GROW, 0)), bot

    def _add_chains(self) -> None:
        """``GoblinKit._add_chains`` with the Corpseherd's placements: ears and headdress tucked inside the hood
        (no weights), the long skirt chains, the pelt chain down the back (no weights), the bell (``totem_feather_01``)
        hanging from the hook's end and the crook rag (``totem_feather_02``) from under the hook."""
        sp = []
        for s, sg in (("l", 1), ("r", -1)):
            b0 = self.hm(sg * 0.03, -0.01, 0.02)
            sp += [(f"ear_{s}_01", b0, b0 + self.hdirm(sg, 0, 0.3) * 0.02, "head"),
                   (f"ear_{s}_02", b0 + self.hdirm(sg, 0, 0.3) * 0.02, b0 + self.hdirm(sg, 0, 0.3) * 0.04,
                    f"ear_{s}_01")]
        for fb, sg, nm in (("f", 1, "loin_f"), ("b", -1, "loin_b")):
            top, mid, bot = self.loin_points(sg)
            sp += [(f"{nm}_01", top, mid, "pelvis"), (f"{nm}_02", mid, bot, f"{nm}_01")]
        t = self.torso
        p0, p1 = t.point(0.92, 180, self.u(1.4)), t.point(0.62, 180, self.u(2.6))
        p2, p3 = t.point(0.36, 180, self.u(3.4)), t.point(0.12, 180, self.u(3.9))
        sp += [("pelt_01", p0, p1, "spine_03"), ("pelt_02", p1, p2, "pelt_01"), ("pelt_03", p2, p3, "pelt_02")]
        for i in range(4):
            r0 = self.hm(-0.03 + 0.02 * i, -0.02, 0.03)
            sp.append((f"headdress_{i + 1:02d}", r0, r0 + self.hdirm(0, 0, 1) * 0.03, "head"))
        wm = self.rig.obj.data.bones["weapon_r"].matrix_local
        tie = wm @ BELL_TIE
        sp.append(("totem_feather_01", tie, tie + Vector((0.0, 0.0, -0.07)), "weapon_r"))
        rag = wm @ RAG_TIE
        sp.append(("totem_feather_02", rag, rag + Vector((0.0, 0.0, -0.07)), "weapon_r"))
        self._add_bones(sp)

    # ── legs: thin stockinged legs under the long skirt (no goblin knee caps: they only poked through it) ──
    def add_leg(self, b: M.Builder, s: str, sg: int, r_scale: float = 0.72, top_up: float = 0.5,
                top_r: float = 2.0) -> int:
        J = self.J
        u = self.u
        k = r_scale
        Hp, Kn, A = J[f"hip_{s}"], J[f"knee_{s}"], J[f"ankle_{s}"]
        top = Hp + Vector((0, 0, u(top_up)))
        pts = [top, Hp.lerp(Kn, 0.45), Kn, Kn.lerp(A, 0.4), A.lerp(Kn, 0.08)]
        rad = [u(top_r * k), u(2.2 * k), u(1.8 * k), u(1.75 * k), u(1.3 * k)]
        leg = M.tube(pts, rad, sides=8, dome0=u(0.8), dome1=u(0.2))
        bind = Bind.blend("pelvis", f"thigh_{s}", f"calf_{s}", f"foot_{s}", falloff=4.0, smooth=0.008)
        lid = b.add(leg, "skin", bind, name=f"leg_{s}")
        self.add_foot(b, s, sg)
        return lid

    # ── feet: worn shoes instead of the goblin's long bare feet ─────────────────────────────────────────
    def add_foot(self, b: M.Builder, s: str, sg: int) -> int:
        J = self.J
        A = J[f"ankle_{s}"]
        L = self.spec.foot_len
        x = A.x
        # (y, half-width, half-height, centre z): heel, instep, ball, toe — a worn, rounded shoe
        st = [(0.22 * L, 0.023, 0.028, 0.032), (0.05 * L, 0.031, 0.034, 0.036), (-0.20 * L, 0.033, 0.025, 0.027),
              (-0.42 * L, 0.030, 0.021, 0.023), (-0.58 * L, 0.020, 0.015, 0.021)]
        st = [(y, hw * GROW, hh * GROW, cz * GROW) for y, hw, hh, cz in st]
        path = [Vector((x, y, cz)) for y, _, _, cz in st]
        foot = M.sweep(path, M.superellipse(1, 1, 10, 2.6), scales=[(hh, hw) for _, hw, hh, _ in st], up=(0, 0, 1),
                       dome0=0.012, dome1=0.012)
        foot.deform(lambda co: Vector((co.x, co.y, max(co.z, 0.0))))
        fb = Bind.blend(f"calf_{s}", f"foot_{s}", f"ball_{s}", falloff=5.0)
        return b.add(foot.smooth(True), "shoe", fb, name=f"foot_{s}", hull_pad=True)


SKIRT_HEM_Z = 0.072 * GROW     # m above the ground: an ankle-length skirt (the shoe toes show under it; the
                               # READY / Walk crouch lowers the hips ≈ 4 cm)
TRAIN = (0.014 * GROW, 0.035 * GROW)     # the back hem drags: (lower, further back) m at the back centre — an old woman's skirts
RAG_TIE = Vector((0.016, CROOK_UP - 0.22, -0.004))    # the crook rag's knot, on the shaft under the hook


# ── surfaces ────────────────────────────────────────────────────────────────────────────────────────────
class SkirtSurf:
    """The long skirt as a surface (u = 0 waist … 1 hem, a = angle: 0 front, +90 left), with soft vertical folds
    that deepen toward the hem (``FOLDS``): the toon bands catch them, so the skirt reads as cloth, not a keg."""

    def __init__(self, kit: CorpseherdKit, top_z: float, folds: bool = True):
        self.kit, self.top_z, self.folds = kit, top_z, folds
        self.P = kit.J["pelvis"]

    def ring(self, f: float):
        """(lateral, depth, centre fwd) radii at height fraction ``f``."""
        fs = [r[0] for r in SKIRT_RINGS]
        return tuple(float(np.interp(f, fs, [r[i] for r in SKIRT_RINGS])) for i in (1, 2, 3))

    def z(self, f: float, a: float = 0.0) -> float:
        back = max(0.0, -math.cos(math.radians(a))) ** 1.5
        return self.top_z + (SKIRT_HEM_Z - TRAIN[0] * back - self.top_z) * f

    def fold(self, f: float, a: float) -> float:
        if not self.folds:
            return 1.0
        n, amp, ph = FOLDS
        k = GB._smooth_w(f, 0.08, 0.9)
        return 1.0 + amp * k * math.sin(math.radians(a) * n + ph + 0.6 * math.sin(math.radians(a) * 2.0))

    def point(self, f: float, a: float, out: float = 0.0) -> Vector:
        lat, dep, cf = self.ring(f)
        k = self.fold(f, a)
        ra = math.radians(a)
        back = max(0.0, -math.cos(ra)) ** 1.5
        c = Vector((0.0, self.P.y - cf + TRAIN[1] * f * f * back, self.z(f, a)))
        p = c + Vector((lat * k * math.sin(ra), -dep * k * math.cos(ra), 0.0))
        return p + self.normal(f, a) * out if out else p

    def normal(self, f: float, a: float) -> Vector:
        e = 1e-3
        ta = self.point(f, a + 0.5) - self.point(f, a - 0.5)
        tf = self.point(min(1.0, f + e), a) - self.point(max(0.0, f - e), a)
        n = tf.cross(ta)
        if n.length < 1e-9:
            return Vector((math.sin(math.radians(a)), -math.cos(math.radians(a)), 0.0))
        n.normalize()
        ra = math.radians(a)
        if n.dot(Vector((math.sin(ra), -math.cos(ra), 0.0))) < 0:
            n = -n
        return n


# (f, lateral, depth, centre fwd) of the skirt (m): gathered at the waist over the hips, then a long A-line to the
# hem (the shaman's skirt envelope keeps the legs inside it)
SKIRT_RINGS = [(f, la * GROW, de * GROW, cf * GROW) for f, la, de, cf in (
    (0.0, 0.132, 0.122, 0.008), (0.10, 0.146, 0.136, 0.008), (0.32, 0.160, 0.151, 0.006),
    (0.62, 0.178, 0.170, 0.004), (1.0, 0.198, 0.190, 0.002))]
FOLDS = (9, 0.07, 0.7)        # (folds round the skirt, radius amplitude at the hem, phase)
# the patchwork (deg, 0 front, +90 left): the crusade-banner gores sewn between the rag panels, irregular widths so
# it never reads as stripes; se shows the front + the right side, sw the front + the left side
GORES = ((20.0, 66.0), (-78.0, -34.0), (138.0, 196.0))


def skirt_bind(kit: CorpseherdKit, top_z: float) -> Bind:
    """Long-skirt weights: the waist on the pelvis; below it the front / back quarters down the ``loin_f`` /
    ``loin_b`` chains (the skirt envelope of ``HerdAnims`` turns them out round the legs and over the floor); the
    sides on the pelvis with a share of the nearer thigh (``SIDE_THIGH``), handing over toward the hem to the thigh
    and the shin (``SIDE_CALF``): a stride sways the side hem, and kneeling (Death) the side hem stays with the shins
    on the floor instead of sinking through it with the hips."""
    names = ("pelvis", "loin_f_01", "loin_f_02", "loin_b_01", "loin_b_02", "thigh_l", "thigh_r", "calf_l", "calf_r")
    bot_z = SKIRT_HEM_Z
    P = kit.J["pelvis"]

    def fn(co):
        x, y, z = co[:, 0], co[:, 1] - P.y, co[:, 2]
        t = np.clip((top_z - z) / max(top_z - bot_z, 1e-6), 0.0, 1.0)
        ang = np.arctan2(x, -y)
        fr = np.clip(np.cos(ang), 0, None) ** 2.0
        bk = np.clip(-np.cos(ang), 0, None) ** 2.0
        sl = np.clip(np.sin(ang), 0, None) ** 2.0
        sr = np.clip(-np.sin(ang), 0, None) ** 2.0
        top = 1.0 - GB._smooth_w(t, 0.0, 0.22)
        low = 1.0 - top
        lk = GB._smooth_w(t, 0.30, 0.75)
        h = GB._smooth_w(t, 0.50, 1.0)                     # toward the hem the sides leave the pelvis for the legs
        pel_s = (1.0 - SIDE_THIGH) * (1.0 - h)
        thi_s = SIDE_THIGH * (1.0 - h) + (1.0 - SIDE_CALF) * h
        cal_s = SIDE_CALF * h
        w = np.stack([top + low * (sl + sr) * pel_s + 1e-4, low * fr * (1 - lk), low * fr * lk, low * bk * (1 - lk),
                      low * bk * lk, low * sl * thi_s, low * sr * thi_s, low * sl * cal_s, low * sr * cal_s], axis=1)
        return w / w.sum(axis=1, keepdims=True)
    return Bind.custom(names, fn)


SIDE_THIGH = 0.45
SIDE_CALF = 0.30


# ── the body ────────────────────────────────────────────────────────────────────────────────────────────
def _thick_rows(outer, inner, wrap: bool = False, sub: bool = True) -> M.Part:
    return C.thick_patch(outer, inner, wrap_u=wrap, sub_tags=sub)


def _hood_xy(rl: float, rf: float, rb: float, a: float, exp: float = HOOD_EXP) -> tuple[float, float]:
    """(lateral, forward) of angle ``a`` (0 front, +90 left) on a hood row."""
    ra = math.radians(a)
    sa, ca = math.sin(ra), math.cos(ra)
    lat = rl * math.copysign(abs(sa) ** (2.0 / exp), sa)
    fr = (rf if ca > 0 else rb) * math.copysign(abs(ca) ** (2.0 / exp), ca)
    return lat, fr


class HoodSurf:
    """The hood's outer surface (u = up in the head frame, a = angle: 0 front, +90 left) for painted creases."""

    def __init__(self, kit: CorpseherdKit):
        self.kit = kit
        self.ups = [r[0] for r in HOOD_ROWS]

    def point(self, u: float, a: float, out: float = 0.0) -> Vector:
        rl = float(np.interp(u, self.ups, [r[1] for r in HOOD_ROWS]))
        rf = float(np.interp(u, self.ups, [r[2] for r in HOOD_ROWS]))
        rb = float(np.interp(u, self.ups, [r[3] for r in HOOD_ROWS]))
        lat, fr = _hood_xy(rl, rf, rb, a)
        p = self.kit.hm(lat, fr, u)
        return p + self.normal(u, a) * out if out else p

    def normal(self, u: float, a: float) -> Vector:
        ta = self.point(u, a + 0.5) - self.point(u, a - 0.5)
        tu = self.point(u + 0.002, a) - self.point(u - 0.002, a)
        n = ta.cross(tu)
        if n.length < 1e-9:
            return self.kit.hdirm(0, 0, 1)
        n.normalize()
        c = self.kit.hm(0.0, 0.0, u)
        if n.dot(self.point(u, a) - c) < 0:
            n = -n
        return n


def hood_rows(kit: CorpseherdKit, cols: int = 22, thick: float = HOOD_THICK):
    """(outer, inner) point grids of the hood: one row per ``HOOD_ROWS`` entry, the columns spread round the back
    from one side of the face opening to the other (above the brow the two edge columns meet in front)."""
    outer, inner = [], []
    n = len(HOOD_ROWS)
    for i, (up, rl, rf, rb, th) in enumerate(HOOD_ROWS):
        th = max(th, 0.0)
        ro, ri = [], []
        for j in range(cols + 1):
            a = th + (360.0 - 2.0 * th) * j / cols          # + = left, round the back (180) to the right side
            lat, fr = _hood_xy(rl, rf, rb, a)
            lat_i, fr_i = _hood_xy(max(rl - thick, 0.003), max(rf - thick, 0.003), max(rb - thick, 0.003), a)
            ro.append(kit.hm(lat, fr, up))
            ri.append(kit.hm(lat_i, fr_i, up - (thick * 0.7 if i == n - 1 else 0.0)))
        outer.append(ro)
        inner.append(ri)
    return outer, inner


class VeilSurf:
    """The widow's veil as a surface (f = 0 its top edge under the eyes … 1 the hem, a = angle round the face: 0
    front, + left), head frame: it hangs off the face like a curtain from just in front of the void (following the
    void's curve it tucked under the face and left a black mouth-like gap), three soft folds (``VEIL_FOLDS``)
    deepening toward the hem; at the centre it drapes from the bridge of the nose onto the chin's point
    (``VEIL_ON_CHIN``) and falls straight past it to the hood's lower lip (``VEIL_HEM``) — no black opening is left
    under it (the dark gap between a short veil and the hood's lit lower rim read as an open mouth, art review r1)."""

    def __init__(self, kit: "CorpseherdKit"):
        self.kit = kit
        self.r0 = kit.void_front(0.0, VEIL_TOP) + 0.010
        self.chin_front = CHIN[0][1] + CHIN[1][1]       # the chin's point (its front-most)

    @staticmethod
    def hem_up(a: float) -> float:
        k = min(1.0, abs(a) / 28.0)
        k = k * k * (3.0 - 2.0 * k)
        return VEIL_HEM[0] + (VEIL_HEM[1] - VEIL_HEM[0]) * k

    def hf(self, f: float, a: float, out: float = 0.0, dz: float = 0.0) -> tuple[float, float, float]:
        """(lat, fwd, up) in the (×1.3 design) head frame."""
        ra = math.radians(a)
        # the top edge drapes from the bridge of the nose (a straight edge read as a visor slit)
        drape = (0.004 - 0.022 * (abs(a) / VEIL_SPAN) ** 1.4) * max(0.0, 1.0 - f * 7.0)
        up = VEIL_TOP + (self.hem_up(a) - VEIL_TOP) * f + drape + dz
        n, amp = VEIL_FOLDS
        fold = amp * (0.3 + 0.7 * f ** 0.9) * math.cos(math.radians((a - 4.0) * 360.0 / n) + 0.35 * math.sin(ra * 3.0))
        centre = 1.0 - min(1.0, abs(a) / 30.0)
        centre = centre * centre * (3.0 - 2.0 * centre)
        k = min(1.0, max(0.0, (VEIL_TOP - up) / (VEIL_TOP - CHIN[0][2])))    # 0 top edge … 1 the chin's height
        k = k * k * (3.0 - 2.0 * k)
        fold *= 1.0 - 0.6 * centre * k                  # flatter where it lies on the chin
        back = (self.r0 - (self.chin_front + VEIL_ON_CHIN)) * k * centre
        lean = VEIL_LEAN * (1.0 - (1.0 - min(f, 1.0)) ** 2)                    # leaning in, most under the eyes:
        r_fwd = self.r0 - lean * (1.0 - centre * k) - back + fold               # turned down, out of the key light
        r_lat = 0.076 + 0.006 * f + 0.4 * fold
        lat = r_lat * math.sin(ra)
        fwd = r_fwd * math.cos(ra) ** 0.8
        nl, nf = math.sin(ra), math.cos(ra)
        return lat + nl * out, fwd + nf * out, up

    def point(self, f: float, a: float, out: float = 0.0, dz: float = 0.0) -> Vector:
        return self.kit.hm(*self.hf(f, a, out, dz))

    def normal(self, f: float, a: float) -> Vector:
        ra = math.radians(a)
        return self.kit.hdirm(math.sin(ra), math.cos(ra), 0.0)


def veil_tatter(j: int, a: float) -> float:
    """Ragged hem offset (m, + up): small notches, a few longer tatters toward the sides; at the centre (|a| < 12°)
    only a slight unevenness (the hem rests on the hood's lower lip there)."""
    base = 0.004 * (0.5 + 0.5 * math.sin(j * 2.1 + 0.4)) * (1 if j % 2 else -0.6)
    if abs(a) < 12.0:
        return 0.4 * base
    tongue = -0.011 if j in VEIL_TONGUES else 0.0
    return base + tongue


def add_head(b: M.Builder, kit: CorpseherdKit, lod: int = 0) -> int:
    """Hood (+ its rolled brim), the void, the eyes, the veil and the chin — all rigid on ``head``."""
    head = Bind.rigid("head")
    cols = 24 if not lod else 14
    outer, inner = hood_rows(kit, cols)
    hood = _thick_rows(outer, inner)
    # hull: the closed outer shell (its front over the face opening faces the camera, so the inverted hull draws no
    # ink inside the opening; the inner surface's own hull would have inked the face)
    ups = [r[0] for r in HOOD_ROWS]

    def hull_fn(u, a):
        rl = float(np.interp(u, ups, [r[1] for r in HOOD_ROWS]))
        rf = float(np.interp(u, ups, [r[2] for r in HOOD_ROWS]))
        rb = float(np.interp(u, ups, [r[3] for r in HOOD_ROWS]))
        lat, fr = _hood_xy(rl, rf, rb, a)
        return kit.hm(lat, fr, u)
    hull = G.grid_loft(hull_fn, ups, G.angles(18 if not lod else 12), cap0="fan", cap1="fan")
    hid = b.add(hood, "hood", head, name="hood", sub_regions={1: "hood", 2: "void", 0: "hood"}, hull=hull,
                hull_pad=True)
    # the rolled brim along the face opening (left edge up, over the brow, down the right edge)
    rim = []
    opening = [i for i in range(len(HOOD_ROWS)) if HOOD_ROWS[i][4] > 0.0]
    for col in (0, -1):
        rim.append([outer[i][col].lerp(inner[i][col], 0.5) for i in opening])
    top_i = opening[-1] + 1
    top = outer[top_i][0].lerp(inner[top_i][0], 0.5)
    path = rim[0] + [top] + list(reversed(rim[1]))
    brim = M.tube(path, [0.0080 * GROW] * len(path), sides=8 if not lod else 6, dome0=0.004, dome1=0.004)
    b.add(brim, "hood_edge", head, name="hood_brim", outline=False, covered_by=hid)
    # cloth creases (art review: from the W1 camera the smooth lit crown read as a helmet): a seam over the crown and
    # down the back, two drape folds down each side, painted in the void's black
    if not lod:
        hs = HoodSurf(kit)
        for path in ([(0.074, 0.0), (0.096, 0.0), (0.112, 0.0)], [(0.112, 180.0), (0.06, 180.0), (-0.02, 182.0),
                                                                  (-0.12, 178.0)],
                     [(0.09, 118.0), (0.02, 124.0), (-0.08, 128.0), (-0.16, 134.0)],
                     [(0.09, -118.0), (0.02, -124.0), (-0.08, -128.0), (-0.16, -134.0)],
                     [(0.04, 82.0), (-0.06, 88.0), (-0.165, 92.0)], [(0.04, -82.0), (-0.06, -88.0), (-0.165, -92.0)]):
            b.add(G.surface_paint(hs, path, 0.0055, 0.0012, steps=4, taper=0.5), "crease", head, name="hood_crease",
                  outline=False, covered_by=hid)
    # the dark void (no face is built: the inside of the hood is black but for the eyes)
    vc = kit.hm(*VOID_C)
    void = M.ellipsoid(tuple(r * GROW for r in VOID_R), (0, 0, 0), sides=14 if not lod else 10, rings=8 if not lod else 6)
    void.transform(Matrix.Translation(vc) @ kit.Hm.to_3x3().to_4x4())
    b.add(void, "void", head, name="void", outline=False, covered_by=hid)
    # violet pinpoint eyes on the void (the family marker; exact at every LOD)
    for s, sg in (("l", 1), ("r", -1)):
        ec = kit.eye_center(sg)
        n = kit.hdirm(0.15 * sg, 1.0, 0.05)
        m = G.frame_z(ec, n, kit.hdirm(0, 0, 1)) @ Matrix.Rotation(math.radians(-sg * EYE_DROOP), 4, "Z")
        eye = M.ellipsoid(tuple(r * GROW for r in EYE_R), (0, 0, 0), sides=10, rings=6).transform(m)
        b.add(eye, "eye", Bind.rigid(f"eye_{s}"), name=f"eye_{s}", outline=False, covered_by=hid)
    # the widow's veil (art review r1: the curved plate read as a grey visor with a centre crease, and the dark gap
    # under its hem over the chin as an open mouth): a hanging cloth in the hood's darker value, three soft vertical
    # folds deepening toward a ragged hem with a few longer tatters, falling past the jaw at the sides and drawn
    # back onto the chin at the centre, so only the chin's point shows under it — no gap above it
    vs = VeilSurf(kit)
    angs = list(np.linspace(-VEIL_SPAN, VEIL_SPAN, 21 if not lod else 9))
    nr = 7 if not lod else 3
    vrows_o, vrows_i = [], []
    for k in range(nr + 1):
        f = k / nr
        ro, ri = [], []
        for j, a in enumerate(angs):
            dz = 0.0
            if k == nr:
                dz = veil_tatter(round(j * 20 / (len(angs) - 1)), a)
            th = 0.25 + 0.75 * min(1.0, f * 3.0)          # thin at the top edge (the inner side: the outer stays smooth)
            ro.append(vs.point(f, a, 0.002, dz))
            ri.append(vs.point(f, a, -0.003 * th, dz))
        vrows_o.append(ro)
        vrows_i.append(ri)
    veil = _thick_rows(vrows_o, vrows_i)
    b.add(veil, "veil", head, name="veil", outline=False, covered_by=hid)
    # the top edge in the brow's shadow (painted, the void's black): lit, it read as a mask's rim under the eyes
    b.add(G.surface_paint(vs, [(0.045, a) for a in np.linspace(-VEIL_SPAN, VEIL_SPAN, 9 if not lod else 5)], 0.024,
                          0.0045, steps=16 if not lod else 6), "crease", head, name="veil_edge", outline=False,
          covered_by=hid)
    if not lod:                                        # the folds' valleys darkened (painted, the void's black)
        for a0, f0, f1 in VEIL_VALLEYS:
            b.add(G.surface_paint(vs, [(f0, a0 * 0.6), ((f0 + f1) * 0.5, a0 * 0.85), (f1, a0)], 0.0060,
                                  0.0030, steps=8, taper=0.85), "crease", head, name="veil_crease", outline=False, covered_by=hid)
    # the thin grey chin under the veil's hem
    cc, cr = CHIN
    chin = M.ellipsoid(tuple(r * GROW for r in cr), (0, 0, 0), sides=8, rings=6)
    chin.transform(Matrix.Translation(kit.hm(*cc)) @ (kit.Hm.to_3x3() @ Matrix.Rotation(math.radians(-12.0), 3,
                                                                                         "X")).to_4x4())
    b.add(chin, "chin", head, name="chin", outline=False, covered_by=hid)
    return hid


def add_bodice(b: M.Builder, kit: CorpseherdKit, lod: int = 0) -> int:
    """The trunk under the shawl and the skirt (``GoblinKit.add_torso`` without the goblin's deltoids and belly):
    only the bodice between the skirt's waist and the shawl's hem is drawn; the hull is the closed loft."""
    t = kit.torso
    u = kit.u
    spine = Bind.blend("pelvis", "spine_01", "spine_02", "spine_03", falloff=3.0, smooth=0.02)
    body = t.loft(-0.16, 1.05, ss=list(np.linspace(-0.16, 1.05, 14 if not lod else 9)), sides=22 if not lod else 14,
                  dome0=u(0.6), cap1=None)
    body, gone = G.split_by(body, lambda s, a, c: kit.torso_hide(s, a))
    gone.bm.free()
    return b.add(body, "skin", spine, name="torso",
                 hull=t.loft(-0.16, 1.05, ss=list(np.linspace(-0.16, 1.05, 10)), sides=16, dome0=u(0.6),
                             cap1="fan"), hull_pad=True)


# shawl rings on the spine curve (s, lateral, depth, centre fwd), m: snug over the narrow shoulders and the upper
# arms' roots (no goblin pauldrons), the cowl's foot at the top
SHAWL_RINGS = [(s, la * GROW, de * GROW, cf * GROW) for s, la, de, cf in (
    (0.30, 0.122, 0.112, 0.004), (0.50, 0.128, 0.114, 0.000), (0.66, 0.140, 0.112, -0.004),
    (0.80, 0.152, 0.106, -0.010), (0.92, 0.138, 0.096, -0.016), (1.02, 0.108, 0.082, -0.014),
    (1.10, 0.080, 0.070, -0.010))]
HUMP = (0.84, 0.058 * GROW, 0.16)   # dowager's hump under the shawl: (s, height m, s-width) on the back (a = 180)
SHAWL_OUT = 0.010 * GROW       # shawl outer surface off the ring surface (m); inner = SHAWL_IN
SHAWL_IN = -0.006 * GROW


def shawl_out(s: float, a: float) -> float:
    """Extra offset of the shawl: the hump on the upper back (falls off round the sides)."""
    hs, hh, hw = HUMP
    k = math.exp(-((s - hs) / hw) ** 2) * max(0.0, -math.cos(math.radians(a))) ** 1.6
    return hh * k


def add_shawl(b: M.Builder, kit: CorpseherdKit, lod: int = 0) -> int:
    """The shawl of rags over the hunched shoulders: hem s ≈ .60 in front (the bodice shows under it), a point
    down the back (s ≈ .30) over the dowager's hump, ragged; a crusade-banner patch (crimson, tarnished trim, the
    scorched open palm) sewn on the back, smaller patches on the shoulder and breast."""
    shw = G.SpineLoft(kit.spine_points(), SHAWL_RINGS, exp=2.3)
    cols = 30 if not lod else 18
    angs = G.angles(cols)
    rows = 11 if not lod else 5     # dense enough that the hump's chords stay under the painted patch / sigil

    def hem_s(a: float) -> float:
        aa = abs(a)
        if aa <= 90.0:                          # front → side: rises over the shoulder cap (the arm comes out under)
            return 0.62 + 0.16 * math.sin(math.radians(aa)) ** 2
        return 0.78 - 0.48 * ((aa - 90.0) / 90.0) ** 1.4   # side → the point down the back

    outer, inner = [], []
    for r in range(rows):
        f = r / (rows - 1)
        ro, ri = [], []
        for i, a in enumerate(angs):
            s0 = hem_s(a)
            s_ = s0 + (1.10 - s0) * f
            if r == 0:                                      # ragged hem: soft tatters, a few longer tongues
                s_ -= 0.028 * (0.5 + 0.5 * math.sin(i * 2.7)) * (1 if i % 3 else 0.2)
            hump = shawl_out(s_, a)
            ro.append(shw.point(s_, a, SHAWL_OUT + hump))
            ri.append(shw.point(s_, a, SHAWL_IN + hump))
        outer.append(ro)
        inner.append(ri)
    shawl = _thick_rows(outer, inner, wrap=True)
    sbind = kit.blend4("spine_03", "neck_01", "clavicle_l", "clavicle_r", "upperarm_l", "upperarm_r",
                       falloff=3.0, smooth=0.06)
    sid = b.add(shawl, "rags", sbind, name="shawl", sub_regions={1: "rags", 2: "rags_in", 0: "rags"},
                hull_pad=True)

    class _Surf:
        def point(self, s, a, out=0.0):
            return shw.point(s, a, SHAWL_OUT + shawl_out(s, a) + out)

        def normal(self, s, a):
            return shw.normal(s, a)
    so = _Surf()
    # drape folds from the neck over the shoulders and down the back (painted, the void's black)
    if not lod:
        for path in ([(1.04, 52.0), (0.92, 58.0), (0.76, 62.0)], [(1.04, -52.0), (0.92, -58.0), (0.76, -62.0)],
                     [(1.00, 134.0), (0.86, 141.0), (0.66, 146.0), (0.50, 150.0)],
                     [(1.00, -134.0), (0.86, -141.0), (0.66, -146.0), (0.50, -150.0)],
                     [(0.98, 20.0), (0.82, 16.0), (0.68, 12.0)]):
            b.add(G.surface_paint(so, path, 0.006, 0.0006, steps=4, taper=0.6), "crease", sbind,
                  name="shawl_crease", outline=False, covered_by=sid)
    # the banner patch on the back: crimson field, tarnished trim, the scorched palm
    sp = 0.70
    b.add(G.surface_patch(so, sp - 0.13, sp + 0.11, 158.0, 204.0, 0.0012, nu=3, na=4), "banner", sbind,
          name="shawl_patch", outline=False, covered_by=sid)
    if not lod:
        for uu in (sp - 0.13, sp + 0.11):
            b.add(G.surface_paint(so, [(uu, 156.0), (uu, 181.0), (uu, 206.0)], 0.009, 0.0022, steps=4), "trim",
                  sbind, name="shawl_trim", outline=False, covered_by=sid)
    palm = [(sp - 0.06, 178.0), (sp + 0.02, 181.0), (sp + 0.06, 184.0)]
    b.add(G.surface_paint(so, palm, 0.024, 0.0008, steps=6, taper=0.3), "sigil", sbind, name="shawl_sigil",
          outline=False, covered_by=sid)
    for fa in (172.0, 177.0, 182.0, 187.0):                 # the palm's fingers
        b.add(G.surface_paint(so, [(sp + 0.04, fa + (fa - 180.0) * 0.4), (sp + 0.10, fa + (fa - 180.0) * 0.9)],
                              0.011, 0.0012, steps=2, taper=0.5), "sigil", sbind, name="shawl_sigil",
              outline=False, covered_by=sid)
    # smaller crimson banner patches on the left breast and the right shoulder (sewn in with dark stitches)
    for (s0, s1, a0, a1) in ((0.66, 0.79, 24.0, 50.0), (0.84, 0.96, -84.0, -60.0)):
        b.add(G.surface_patch(so, s0, s1, a0, a1, 0.0012, nu=2, na=3), "banner", sbind, name="shawl_patch",
              outline=False, covered_by=sid)
    return sid


class GN_Off:
    """A surface pushed ``off`` along its normals (``mgeo`` surface helpers)."""

    def __init__(self, surf, off):
        self.s, self.off = surf, off

    def point(self, u, a, out=0.0):
        return self.s.point(u, a, self.off + out)

    def normal(self, u, a):
        return self.s.normal(u, a)


def _in_gore(a: float) -> bool:
    for a0, a1 in GORES:
        aa = a if a >= a0 - 1e-6 or a1 <= 180.0 else a + 360.0
        if a0 <= aa <= a1:
            return True
    return False


def add_skirt(b: M.Builder, kit: CorpseherdKit, lod: int = 0) -> int:
    """The long skirt patched from the crusade's banners: rag panels (dark) and crimson banner gores sewn between
    them, soft folds, a tarnished-trim seam on each gore's edge, an ash-dark scorched hem with a ragged edge."""
    t = kit.torso
    top_z = t.point(0.10, 0.0).z
    surf = SkirtSurf(kit, top_z)
    edges = [a for g in GORES for a in g]
    edges = [((a + 180.0) % 360.0) - 180.0 for a in edges]
    cols = 32 if not lod else 18
    angs = G.angles(cols, extra=edges)
    fs = [0.0, 0.10, 0.25, 0.45, 0.65, 0.82, 1.0] if not lod else [0.0, 0.2, 0.55, 1.0]
    outer, inner = [], []
    for k, f in enumerate(fs):
        ro, ri = [], []
        for i, a in enumerate(angs):
            ff = f
            if k == len(fs) - 1:                            # ragged hem
                ff -= 0.030 * (0.5 + 0.5 * math.sin(i * 2.3)) * (1 if i % 2 else 0.4)
            n = surf.normal(ff, a)
            p = surf.point(ff, a)
            ro.append(p + n * 0.004)
            ri.append(p - n * 0.006)
        outer.append(ro)
        inner.append(ri)
    skirt = _thick_rows(outer, inner, wrap=True)
    bind = skirt_bind(kit, top_z)
    # hull proxy: the folds' outer envelope (no folds: radius × the fold crest) — a smooth closed shell that stays
    # outside the folded skirt when decimated (LOD1: the decimated fold crests stood 1-1.5 cm out of a decimated
    # copy of the skirt itself)
    env = SkirtSurf(kit, top_z, folds=False)
    n_amp = FOLDS[1]
    h_angs = G.angles(20 if not lod else 14)
    h_fs = [0.0, 0.25, 0.5, 0.75, 1.0]
    h_out, h_in = [], []
    for f in h_fs:
        k = 1.0 + n_amp * GB._smooth_w(f, 0.08, 0.9)
        ro, ri = [], []
        for a in h_angs:
            c = env.point(f, a)
            ctr = Vector((0.0, env.P.y - env.ring(f)[2], c.z))
            radial = (c - ctr) * k
            nrm = env.normal(f, a)
            ro.append(ctr + radial + nrm * 0.0015)
            ri.append(ctr + radial - nrm * 0.008)
        h_out.append(ro)
        h_in.append(ri)
    hull = _thick_rows(h_out, h_in, wrap=True, sub=False)
    P = kit.J["pelvis"]

    def ang_of(f) -> float:
        c = f.calc_center_median()
        return math.degrees(math.atan2(c.x, -(c.y - P.y)))
    hem_cut = SKIRT_HEM_Z + 0.075 * GROW
    upper, band = C.split_faces(skirt, lambda bm, f: f.calc_center_median().z < hem_cut)
    rags, gores = C.split_faces(upper, lambda bm, f: _in_gore(ang_of(f)))
    sid = b.add(rags, "rags", bind, name="skirt", sub_regions={1: "rags", 2: "char", 0: "rags"}, hull=hull,
                hull_pad=True)
    b.add(gores, "banner", bind, name="skirt_gore", sub_regions={1: "banner", 2: "char", 0: "banner"},
          outline=False, covered_by=sid)
    b.add(band, "char", bind, name="skirt_hem", outline=False, covered_by=sid)
    so = GN_Off(surf, 0.0052)
    if not lod:
        # tarnished trim down one edge of each gore (the banners' borders), dark stitches on the others
        for a0, a1 in GORES:
            if a0 > 90.0:      # the banner's tarnished border survives on the back gore only (on the front gores a
                               # bright vertical line read as a seam / zip at the W1 camera)
                b.add(G.surface_paint(so, [(0.03, a0 + 2.0), (0.5, a0 + 2.6), (0.86, a0 + 1.6)], 0.008, 0.0009,
                                      steps=5), "trim", bind, name="skirt_trim", outline=False, covered_by=sid)
            b.add(G.surface_paint(so, [(0.03, a1 - 1.5), (0.5, a1 - 1.0), (0.86, a1 - 2.0)], 0.004, 0.0009, steps=5),
                  "stitch", bind, name="skirt_seam", outline=False, covered_by=sid)
        # a rag patch sewn over one banner gore, a banner patch on a rag panel
        b.add(G.surface_patch(so, 0.40, 0.58, 30.0, 52.0, 0.0010, nu=2, na=3), "rags", bind, name="skirt_patch",
              outline=False, covered_by=sid)
        b.add(G.surface_patch(so, 0.22, 0.38, -132.0, -110.0, 0.0010, nu=2, na=3), "banner", bind,
              name="skirt_patch", outline=False, covered_by=sid)
    return sid


def add_arm(b: M.Builder, kit: CorpseherdKit, s: str, sg: int, lod: int = 0) -> int:
    """A thin sleeve of rags (shoulder → a flared, ragged cuff over the wrist): an old woman's arm, not the goblin's
    muscle."""
    J = kit.J
    u = kit.u
    S, E, W = J[f"shoulder_{s}"], J[f"elbow_{s}"], J[f"wrist_{s}"]
    pts = [S + (S - E).normalized() * u(0.4), S.lerp(E, 0.5), E, E.lerp(W, 0.5), W.lerp(E, 0.12)]
    rad = [u(1.50), u(1.40), u(1.30), u(1.30), u(1.45)]
    arm = M.tube(pts, rad, sides=10 if not lod else 7, dome0=u(0.3), dome1=u(0.2))
    bind = Bind.blend(f"clavicle_{s}", f"upperarm_{s}", f"lowerarm_{s}", falloff=4.0, smooth=0.008)
    aid = b.add(arm, "rags", bind, name=f"arm_{s}")
    # the cuff: a short flared ragged tube round the wrist (rigid on the forearm)
    d = (W - E).normalized()
    c0, c1 = W - d * u(1.6), W - d * u(0.1 if sg > 0 else 0.9)       # the crook hand's cuff kept off the shaft
    cuff = M.tube([c0, c0.lerp(c1, 0.5), c1], [u(1.42), u(1.60), u(1.78)], sides=10 if not lod else 7, dome0=0.0,
                  dome1=0.0)
    b.add(cuff, "rags", Bind.rigid(f"lowerarm_{s}"), name=f"cuff_{s}", hull_pad=True)
    return aid


def _finger(path, r0: float, r1: float, sides: int) -> M.Part:
    n = len(path)
    rad = [r0 + (r1 - r0) * i / (n - 1) for i in range(n)]
    return M.tube(path, rad, sides=sides, dome0=r0 * 0.5, dome1=r1 * 0.9)


def add_grip_hand(b: M.Builder, kit: CorpseherdKit, lod: int = 0) -> int:
    """The right hand round the crook's grip: a narrow palm against the shaft, four long fingers wrapped round it
    (index toward the hook), the thumb over them — built round the shaft's rest axis (``weapon_r`` +Y through
    ``grip_r``), so every pose keeps the fingers on the wood."""
    J = kit.J
    bones = kit.rig.obj.data.bones
    Gp, W = J["grip_r"], J["wrist_r"]
    A = (bones["weapon_r"].matrix_local.to_3x3() @ Vector((0, 1, 0))).normalized()
    uu = W - Gp
    uu -= A * uu.dot(A)
    uu.normalize()
    vv = A.cross(uu).normalized()
    rs = GRIP_R
    hb = Bind.rigid("hand_r")
    sides = 6 if not lod else 4

    def ring_pt(along: float, phi: float, r: float) -> Vector:
        p = math.radians(phi)
        return Gp + A * along + (uu * math.cos(p) + vv * math.sin(p)) * r
    # palm + back of the hand: from the wrist to the shaft's near side
    pc = Gp + uu * (rs + 0.016) + vv * 0.004
    palm = M.ellipsoid((0.020, 0.013, 0.026), (0, 0, 0), sides=8, rings=6)
    palm.transform(Matrix.Translation(pc) @ Matrix((vv, uu, A)).transposed().to_4x4())
    pid = b.add(palm, "hands", hb, name="hand_r", hull_pad=True)
    rf = 0.0058
    for k, along in enumerate((0.017, 0.006, -0.005, -0.015)):
        R0 = rs + rf + 0.0008
        ln = (0.94, 1.0, 0.96, 0.82)[k]
        end = 60.0 + 205.0 * ln
        pth = [Gp + A * along + uu * (rs + 0.010) + vv * 0.014]
        for phi in np.linspace(60.0, end, 6):
            pth.append(ring_pt(along, phi, R0))
        b.add(_finger(pth, rf, rf * 0.72, sides), "hands", hb, name="finger_r", outline=False, covered_by=pid)
    # the thumb: from the palm's other edge over the curled fingers
    tp = [Gp + A * 0.010 + uu * (rs + 0.014) - vv * 0.012, ring_pt(0.020, -40.0, rs + 0.007),
          ring_pt(0.022, -95.0, rs + 0.008), ring_pt(0.020, -135.0, rs + 0.010)]
    b.add(_finger(tp, 0.0068, 0.0052, sides), "hands", hb, name="thumb_r", outline=False, covered_by=pid)
    return pid


def add_free_hand(b: M.Builder, kit: CorpseherdKit, lod: int = 0) -> int:
    """The free left hand: a narrow palm on ``hand_l`` and four long, half-curled fingers on ``fingers_01_l``
    (curled further / straightened by the clips), the thumb on ``thumb_01_l``."""
    J = kit.J
    W, Kn = J["wrist_l"], J["knuckles_l"]
    hbone = kit.rig.obj.data.bones["hand_l"]
    X = hbone.matrix_local.to_3x3() @ Vector((1, 0, 0))      # curl axis
    d = (Kn - W).normalized()
    palm_n = X.cross(d).normalized()
    if palm_n.dot(Vector((0, -1, 0))) < 0:
        palm_n = -palm_n
    lat = palm_n.cross(d).normalized()
    sides = 6 if not lod else 4
    g = GROW                                            # authored at the ×1.3 design size
    pc = W.lerp(Kn, 0.55)
    palm = M.ellipsoid((0.021 * g, 0.028 * g, 0.011 * g), (0, 0, 0), sides=8, rings=6)
    palm.transform(Matrix.Translation(pc) @ Matrix((lat, d, palm_n)).transposed().to_4x4())
    pid = b.add(palm, "hands", Bind.rigid("hand_l"), name="hand_l", hull_pad=True)
    fb = Bind.rigid("fingers_01_l")
    rf = 0.0056 * g
    for k, (off, ln, spread) in enumerate(((0.013, 0.074, 7.0), (0.0045, 0.082, 2.0), (-0.0045, 0.078, -2.5),
                                           (-0.013, 0.064, -8.0))):
        base = Kn + lat * off * g - d * 0.004 * g
        dd = (d + lat * math.tan(math.radians(spread))).normalized()
        pth = [base]
        ang = 0.0
        p = base
        seg = ln * g / 4
        for j in range(4):
            ang += (14.0, 22.0, 26.0, 24.0)[j]
            r = math.radians(ang)
            step = dd * math.cos(r) + palm_n * math.sin(r)
            p = p + step * seg
            pth.append(p)
        b.add(_finger(pth, rf, rf * 0.68, sides), "hands", fb, name="finger_l", outline=False, covered_by=pid)
    tb = Bind.rigid("thumb_01_l")
    t0 = W.lerp(Kn, 0.35) + palm_n * 0.008 * g + lat * 0.016 * g
    tdir = (d * 0.7 + palm_n * 0.45 + lat * 0.35).normalized()
    tp = [t0, t0 + tdir * 0.022 * g, t0 + tdir * 0.040 * g + palm_n * 0.004 * g]
    b.add(_finger(tp, 0.0068 * g, 0.0052 * g, sides), "hands", tb, name="thumb_l", outline=False, covered_by=pid)
    return pid


def add_bell(b: M.Builder, kit: CorpseherdKit, lod: int = 0) -> int:
    """The cracked bronze handbell on its cord from the hook's end (``totem_feather_01``, rest: hanging straight
    down): handle loop, crown, waist, flared lip with a chipped notch, the dark mouth, a clapper, the crack and
    verdigris streaks."""
    bones = kit.rig.obj.data.bones
    bone = bones["totem_feather_01"]
    tie = bone.head_local.copy()
    dn = (bone.tail_local - bone.head_local).normalized()
    bb = Bind.rigid("totem_feather_01")
    cord = M.tube([tie, tie + dn * (BELL_CORD * 0.5), tie + dn * BELL_CORD], [0.0028, 0.0026, 0.0028],
                  sides=5, dome0=0.002, dome1=0.0)
    top = tie + dn * BELL_CORD
    # handle loop (a small ring the cord threads)
    loop = []
    for a in np.linspace(0.0, 2 * math.pi, 9)[:-1]:
        loop.append(top + dn * (0.008 - 0.008 * math.cos(a)) + Vector((0.008 * math.sin(a), 0.0, 0.0)))
    ring = M.sweep(loop, M.superellipse(1, 1, 5, 2.0), scales=[0.0028] * len(loop), closed_path=True)
    # the bell (lathe along −dn): outer profile then the inner wall back up → a shell open at the mouth
    H, Rb = BELL_H, BELL_R
    prof_o = [(0.0001, 0.0), (0.012, 0.002), (0.019, 0.010), (0.021, 0.024), (0.022, 0.040), (0.026, 0.056),
              (0.034, 0.070), (Rb, 0.080), (Rb + 0.001, H)]
    prof_i = [(Rb - 0.004, H), (Rb - 0.006, 0.078), (0.029, 0.068), (0.021, 0.054), (0.0175, 0.040),
              (0.016, 0.022), (0.0001, 0.016)]
    prof_o = [(r * BELL_K, z * BELL_K) for r, z in prof_o[:-2]] + [(Rb, 0.080 * BELL_K), (Rb + 0.001, H)]
    prof_i = [(Rb - 0.004, H), (Rb - 0.006, 0.078 * BELL_K)] + [(r * BELL_K, z * BELL_K) for r, z in prof_i[2:]]
    prof = prof_o + prof_i
    sides = 14 if not lod else 9
    shell = M.lathe([(r, -z) for r, z in prof], sides=sides, cap0=None, cap1=None)
    frame = R.frame(top + dn * 0.016, -dn, Vector((1, 0, 0)))          # lathe +Z → frame Y = up the bell
    shell.transform(frame @ Matrix.Rotation(math.radians(-90), 4, "X"))
    # chipped lip: pull the lip vertices in one 40° sector up into a notch
    ax = (-dn).normalized()
    c_lip = top + dn * (0.016 + H)
    ref = Vector((1.0, 0.0, 0.0))
    ref = (ref - ax * ref.dot(ax)).normalized()
    ref2 = ax.cross(ref)

    def notch(co):
        rel = co - c_lip
        h = rel.dot(ax)                                   # height above the lip (m, + up the bell)
        if h > 0.016:
            return co
        q = rel - ax * rel.dot(ax)
        ang = math.degrees(math.atan2(q.dot(ref2), q.dot(ref)))
        k = max(0.0, 1.0 - abs(ang - 35.0) / 22.0)
        return co + ax * (0.012 * k * (1.0 - h / 0.016))
    shell.deform(notch)
    # mouth inside: dark (faces whose normal points toward the bell's axis)
    shell_hull = shell.copy()
    outside, inside = C.split_faces(shell, lambda bm_, f: (f.calc_center_median() - (
        top + dn * (0.016 + H * 0.5))).normalized().dot(f.normal) < -0.2)
    bid = b.add(outside, "bronze", bb, name="bell", hull=shell_hull, hull_pad=True)
    b.add(inside, "bronze_dark", bb, name="bell_mouth", outline=False, covered_by=bid)
    b.add(cord, "cord", bb, name="bell_cord", hull_pad=True)
    b.add(ring, "bronze_dark", bb, name="bell_loop", outline=False, covered_by=bid)
    # clapper inside the mouth
    clap = M.ellipsoid((0.0085 * BELL_K,) * 3, top + dn * (0.016 + H * 0.80), sides=6, rings=4)
    b.add(clap, "bronze_dark", bb, name="bell_clapper", outline=False, covered_by=bid)
    # the crack (from the notch up the waist) and verdigris streaks: painted on the bell's surface
    class _BellSurf:
        def point(self, hz, a, out=0.0):
            r = float(np.interp(hz, [z for _, z in prof_o], [r for r, _ in prof_o]))
            ra = math.radians(a)
            return top + dn * (0.016 + hz) + (ref * math.cos(ra) + ref2 * math.sin(ra)) * (r + out)

        def normal(self, hz, a):
            ra = math.radians(a)
            n = ref * math.cos(ra) + ref2 * math.sin(ra)
            return (n + dn * 0.25).normalized()
    bs = _BellSurf()
    if not lod:
        b.add(G.surface_paint(bs, [(z * BELL_K, a_) for z, a_ in ((0.077, 36.0), (0.064, 40.0), (0.052, 33.0),
                                                                 (0.040, 37.0))], 0.0040, 0.0009,
                              steps=4, taper=0.6), "bronze_dark", bb, name="bell_crack", outline=False, covered_by=bid)
        for a in (-60.0, 150.0, 205.0):
            b.add(G.surface_paint(bs, [(0.012 * BELL_K, a), (0.040 * BELL_K, a + 4.0), (0.066 * BELL_K, a - 3.0)],
                                  0.008, 0.0008, steps=3,
                                  taper=0.7), "patina", bb, name="bell_patina", outline=False, covered_by=bid)
    return bid


def add_crook_rag(b: M.Builder, kit: CorpseherdKit, lod: int = 0) -> int:
    """A strip torn from a crusade banner, knotted on the crook under the hook (``totem_feather_02``)."""
    bone = kit.rig.obj.data.bones["totem_feather_02"]
    h0 = bone.head_local.copy()
    fl = G.flap(0.024, 0.018, 0.080, thick=0.004, cols=2, rows=4 if not lod else 2, notch=0.02, tails=2,
                ragged=0.004, seed=3)
    # hanging along −Z (world, rest), the strip's face toward the character's left (+X)
    fl.transform(Matrix.Translation(h0) @ Matrix.Rotation(math.radians(90.0), 4, "Z"))
    return b.add(fl, "banner", Bind.rigid("totem_feather_02"), name="crook_rag", sub_regions={1: "banner",
                                                                                          2: "char", 0: "banner"},
                 hull_pad=True)


LOD1_RATIO = 0.65
LOD1_HULL = 0.75
LOD1_SKIP = ("skirt_seam", "skirt_patch", "shawl_trim", "hood_crease", "shawl_crease", "veil_crease", "bell_crack",
             "bell_patina", "bell_loop", "bell_clapper", "hood_brim", "thumb", "cuff")


def add_belt(b: M.Builder, kit: CorpseherdKit, sid: int, lod: int = 0) -> int:
    """A rope cord round the skirt's waist, knotted at the front-left with two short hanging ends (each inks on its
    own hull: covered by the skirt, its padded proxy would balloon round them)."""
    top_z = kit.torso.point(0.10, 0.0).z
    surf = GN_Off(SkirtSurf(kit, top_z, folds=False), 0.004)
    band = G.surface_ring(surf, lambda a: 0.035, 0.016, 0.006, segs=24 if not lod else 14)
    bind = Bind.blend("pelvis", "spine_01", falloff=3.0)
    bid = b.add(band, "rope", bind, name="belt", outline=False, covered_by=sid)
    if not lod:
        for k in range(14):
            a = -180 + k * (360.0 / 14) + 4
            st = G.surface_paint(surf, [(0.005, a - 5), (0.065, a + 7)], 0.005, 0.0075, steps=2)
            b.add(st, "rope_line", bind, name="belt_twist", outline=False, covered_by=sid)
    kc = surf.point(0.035, 26.0, 0.010)
    knot = M.ellipsoid((0.016, 0.012, 0.014), kc, sides=8, rings=5)
    b.add(knot, "rope", bind, name="belt_knot", hull_pad=True)
    for da, ln in ((20.0, 0.075), (31.0, 0.055)):
        p0 = surf.point(0.035, da, 0.012)
        end = M.tube([p0, p0 + Vector((0, -0.010, -0.045)), p0 + Vector((0, -0.016, -ln))],
                     [0.0058, 0.0052, 0.0044], sides=6)
        b.add(end, "rope", bind, name="belt_end")
    return bid


def build_body(pal: Palette, kit: CorpseherdKit, lod: int = 0, ratio: float = LOD1_RATIO) -> bpy.types.Object:
    from kit import asset
    b = GN.march_builder(ASSET, pal, ALIAS, PREFIX, lod, LOD1_SKIP, ratio, hull_ratio=LOD1_HULL if lod else 1.0)
    add_head(b, kit, lod)
    add_bodice(b, kit, lod)
    add_shawl(b, kit, lod)
    sid = add_skirt(b, kit, lod)
    add_belt(b, kit, sid, lod)
    for s, sg in (("l", 1), ("r", -1)):
        add_arm(b, kit, s, sg, lod)
        kit.add_leg(b, s, sg, r_scale=0.50, top_up=0.5, top_r=2.0)
    add_grip_hand(b, kit, lod)
    add_free_hand(b, kit, lod)
    add_bell(b, kit, lod)
    add_crook_rag(b, kit, lod)
    body = asset.finish_mesh(b, pal, OUTLINE_CLASS, rig=kit.rig, grounding_height=HEIGHT,
                             name=f"{ASSET}_LOD{lod}" if lod else None)
    body["af_lod"] = lod
    return body


# ── the crook ───────────────────────────────────────────────────────────────────────────────────────────
def build_crook(pal: Palette) -> bpy.types.Object:
    """``SM_MonWeapon_Crook``: a shepherd's crook of pale ash-wood (grip at the origin, +Y up the shaft, the hook
    curling forward over +Z): a knotted, slightly crooked shaft, the grip wound with cord, the hook with its end
    turned out, a cord knot under the hook (where the rag and the bell's cord are tied); grain strokes."""
    b = GN.MarchBuilder(CROOK, pal, CROOK_ALIAS, prefix="corpseherd_crook")
    b.declare()
    rig = Bind.rigid("root")
    rng = scene.rng(CROOK, "shaft")
    ys = np.linspace(-CROOK_DOWN, CROOK_UP, 14)
    pts = [Vector((0.004 * math.sin(y * 7.0 + 0.3) + rng.uniform(-0.0015, 0.0015), y,
                   0.003 * math.cos(y * 5.0))) for y in ys]
    pts[-1] = Vector((0.0, CROOK_UP, 0.0))
    rad = [SHAFT_R[0] + (SHAFT_R[1] - SHAFT_R[0]) * (y + CROOK_DOWN) / (CROOK_DOWN + CROOK_UP) for y in ys]
    # the hook: on from the shaft top round the circle, the end flaring out
    hk = [hook_point(p) for p in np.linspace(14.0, HOOK_END, 12)]
    end = hook_point(HOOK_END) + (hook_point(HOOK_END) - hook_point(HOOK_END - 12.0)).normalized() * 0.010
    hk.append(end + Vector((0.0, -0.004, 0.006)))
    hr = [SHAFT_R[1] * (1.0 - 0.18 * i / (len(hk) - 1)) for i in range(len(hk))]
    path = pts + hk
    radii = rad + hr
    shaft = M.tube(path, radii, sides=9, up=(1, 0, 0), dome0=0.006, dome1=0.006)
    sid = b.add(shaft, "wood", rig, name="shaft", hull_pad=True)
    # knots
    for y, side in ((-0.62 * CROOK_DOWN, 1.0), (0.26 * CROOK_UP, -1.0), (0.60 * CROOK_UP, 1.0)):
        k = M.ellipsoid((0.006, 0.012, 0.006), (side * 0.010, y, 0.004 * side), sides=6, rings=4)
        b.add(k, "knot", rig, name="knot", outline=False, covered_by=sid)
    # grip wrap (cord) round the fist's place
    for y0 in np.linspace(-0.045, 0.040, 7):
        w = M.tube([Vector((0, y0 - 0.0045, 0)), Vector((0, y0 + 0.0045, 0))], [GRIP_R, GRIP_R], sides=9,
                   dome0=0.0, dome1=0.0)
        b.add(w, "cord", rig, name="wrap", outline=False, covered_by=sid)
    # cord knot under the hook (the rag) and at the hook's end (the bell)
    for c, r in ((Vector((0.0, RAG_TIE.y, 0.0)), 0.0155), (hook_point(HOOK_END - 6.0), 0.0125)):
        nb = M.ellipsoid((r, 0.008, r), c, sides=8, rings=4)
        b.add(nb, "cord", rig, name="knot_cord", outline=False, covered_by=sid)
    # grain strokes along the shaft
    class _ShaftSurf:
        def point(self, y, a, out=0.0):
            k = (y + CROOK_DOWN) / (CROOK_DOWN + CROOK_UP)
            r = SHAFT_R[0] + (SHAFT_R[1] - SHAFT_R[0]) * k + out
            ra = math.radians(a)
            return Vector((r * math.sin(ra), y, r * math.cos(ra)))

        def normal(self, y, a):
            ra = math.radians(a)
            return Vector((math.sin(ra), 0.0, math.cos(ra)))
    ss = _ShaftSurf()
    dn, up = CROOK_DOWN, CROOK_UP                   # grain kept on the shaft (fractions of its two halves)
    for a, y0, y1 in ((30.0, -0.88 * dn, -0.20 * dn), (150.0, -0.78 * dn, -0.14 * dn), (-90.0, 0.12 * up, 0.72 * up),
                      (95.0, 0.15 * up, 0.52 * up), (-150.0, -0.66 * dn, -0.18 * dn)):
        b.add(G.surface_paint(ss, [(y0, a), ((y0 + y1) / 2, a + 6.0), (y1, a - 4.0)], 0.0028, 0.0006, steps=4,
                              taper=0.6), "wood_line", rig, name="grain", outline=False, covered_by=sid)
    obj = b.build(CROOK)
    mt = shading.toon_material(pal)
    outline.bake_hull(obj, outline.WIDTH_CM[OUTLINE_CLASS] / 100.0, mt, shading.outline_material(pal))
    for m in outline.hull_color_report(obj, pal, OUTLINE_CLASS):
        print(f"[ink] WARNING {CROOK}: {m}")
    obj["af_outline_class"] = OUTLINE_CLASS
    return obj


def crook_grip(co) -> bool:
    return -0.05 < co.y < 0.05


def build_all():
    scene.reset()
    pal = Palette(GN.FAMILY)
    kit = CorpseherdKit(ASSET, PREFIX, WEB, LOOK)
    eye_sockets(kit)
    body = build_body(pal, kit)
    crook = build_crook(pal)
    return pal, kit, body, crook


def eye_sockets(kit: CorpseherdKit) -> None:
    """``fx_eye_l/r`` on the eyes, ``fx_bell`` at the bell's mouth (the bolt and the ring ripple)."""
    for s, sg in (("l", 1), ("r", -1)):
        R.add_socket(kit.rig, f"fx_eye_{s}", "head", kit.eye_center(sg))
    bone = kit.rig.obj.data.bones["totem_feather_01"]
    dn = (bone.tail_local - bone.head_local).normalized()
    R.add_socket(kit.rig, "fx_bell", "totem_feather_01", bone.head_local + dn * (BELL_CORD + 0.016 + BELL_H))


# ── poses ───────────────────────────────────────────────────────────────────────────────────────────────
def V(lat: float = 0.0, fwd: float = 0.0, up: float = 0.0) -> Vector:
    """Body-space vector of the pose code, authored at the ×1.3 design size: ``common.V`` × ``GROW`` (every IK
    target and offset below scales with the figure; a pole / aim direction does not care about its length).
    Values derived from the rig (``A.web_*``, ``A.AZ``, the spec) are already at size: they go through ``C.V``."""
    return C.V(lat, fwd, up) * GROW


def u3(A, lat: float, fwd: float, up: float) -> Vector:
    return C.V(lat * A.U, fwd * A.U, up * A.U)


WEB_READY = dict(root=(47.0, 72.5), footN=50.5, footF=44.5, lean=0.42, head=-0.20)
READY_HAND_R = (-0.29, 0.02, 0.52)          # m (lat, fwd, up): the crook hand at the right side, a relaxed arm
READY_CROOK = (3.0, 0.04)                   # crook angle from vertical (deg, + forward), lateral tilt (+ = the
                                            # top toward her: the butt stands out clear of the skirt)
READY_HAND_L = (0.13, 0.21, 0.60)           # the free hand held out over the flock, long fingers drooping
CROOK_ROLL = 90.0                           # the crook rolled in the fist: the hook opens outward, away from
                                            # her body, so the bell hangs free in every facing (art review)
READY_LEAN = 16.0                           # the old woman's stoop on top of the rest hunch (deg)


def ready(A) -> BP:
    """READY: stooped (the rest hunch + ``READY_LEAN``), knees soft, the crook upright at the right side (its butt
    just off the ground), the free hand clutching the shawl at the breast, the face lifted out of the hood toward
    the target. Elbows hang down and back (an old woman's arms, not the goblin's flared wings)."""
    wd, wu = C.blade(READY_CROOK[0], lat=READY_CROOK[1])
    P = A.web_pelvis(*WEB_READY["root"])
    return BP(
        pelvis=P, hip_yaw=-4.0, twist=4.0, head_pitch=-40.0, lean=READY_LEAN,
        foot_r=A.web_foot(WEB_READY["footN"], -0.075 * GROW), foot_l=A.web_foot(WEB_READY["footF"], 0.08 * GROW),
        foot_r_rot=(0.0, 0.0, 10.0), foot_l_rot=(0.0, 0.0, 16.0), knee_l=V(0.3, 1, 0), knee_r=V(-0.3, 1, 0),
        hand_r=V(*READY_HAND_R), elbow_r=V(-0.35, -0.55, -0.75), wpn=wd, wpn_up=wu, wrist_r=(0.0, CROOK_ROLL, 0.0),
        hand_l=V(*READY_HAND_L), elbow_l=V(0.5, -0.3, -0.8), wrist_l=(-20.0, 0.0, 0.0), curl_l=35.0,
        flow=0.1)


C.BP_DEFAULTS.setdefault("curl_l", 0.0)          # free hand finger curl (deg about the knuckles, + = closed)
C.BP_DEFAULTS.setdefault("bell", 0.0)            # extra bell swing (deg, + = forward) on top of the pendulum
C.BP_DEFAULTS.setdefault("bell_side", 0.0)       # bell swing sideways (deg, + = toward her left)
C.BP_DEFAULTS.setdefault("slide", 0.0)           # m the crook has slid through the fist toward its hook (Death)


def herd_cloth(p, bp: BP, A) -> None:
    """Bell pendulum (``totem_feather_01``: hangs in the world, swaying with the wave, the flow and ``bell``),
    the crook rag (``totem_feather_02``) and the free hand's finger curl."""
    sw = math.sin(bp.wave + 0.8)
    a = math.radians(5.0 * sw + 14.0 * bp.flow + bp.bell)
    lat = math.radians(3.0 * math.sin(bp.wave * 0.5 + 0.3) + bp.bell_side)
    d = Vector((math.sin(lat), -math.sin(a), -math.cos(a) * math.cos(lat))).normalized()
    p.aim("totem_feather_01", d, Vector((1, 0, 0)))
    a2 = math.radians(8.0 * math.sin(bp.wave + 1.7) + 24.0 * bp.flow + 0.4 * bp.bell)
    d2 = Vector((-0.12, math.sin(a2) * 0.5, -math.cos(a2))).normalized()
    p.aim("totem_feather_02", d2, Vector((1, 0, 0)))
    p.rot("fingers_01_l", pitch=bp.curl_l)
    if bp.slide:                    # the crook slid through the fist toward its hook (Death): weapon_r moved down
        p.move("weapon_r", -bp.wpn.normalized() * bp.slide)        # its own axis (the crook and the bell follow)


class HerdAnims(GS.ShamanAnims):
    """``goblin_shaman.ShamanAnims`` (the skirt envelope on the real meshes) with the Corpseherd's cloth. The
    envelope tests every skirt part (the rag panels ``skirt``, the banner gores ``skirt_gore`` and the scorched hem
    ``skirt_hem``: the shaman's skirt was one part) against the legs ``leg_*``."""

    SKIRT_PARTS = ("skirt", "skirt_gore", "skirt_hem")

    def bind_meshes(self, body, items=None, foot_parts=None) -> None:
        GS.GoblinAnims.bind_meshes(self, body, items, foot_parts)
        names = json.loads(body["af_parts"])
        kit = self.kit
        sk = C.SkinPoints(self.rig).add_mesh(body, names, lambda n: n in self.SKIRT_PARTS)
        lg = C.SkinPoints(self.rig).add_mesh(body, names, lambda n: n.startswith(("leg_", "knee_")))
        top_z = kit.torso.point(0.10, 0.0).z
        yc = self.J["pelvis"].y - kit.u(0.4)
        rest = np.array(self.rig.obj.data.bones["pelvis"].matrix_local)
        self.skirt = (sk, lg, top_z, yc, rest)

    GROUND_CLEAR = 0.016         # m: the chain-carried skirt kept above the floor (kneeling, the panels pool out)
    CHAIN_SECTOR = 62.0          # deg: the legs the chains answer for (front / back quarters); at the sides the skirt
                                 # follows the thighs and shins itself (``skirt_bind``)

    def skirt_excess(self, mats) -> dict:
        """``ShamanAnims.skirt_excess`` restricted to the leg points in front of / behind the legs
        (``CHAIN_SECTOR``): a side deficit (the thigh's root swinging at the hip) cannot be helped by turning the
        front / back chain, and counting it froze the front chain while a knee pressed through the front gore."""
        sk, lg, top_z, yc, rest = self.skirt
        T = rest @ np.linalg.inv(np.array(mats["pelvis"]))

        def cyl(x):
            x = (np.c_[x, np.ones(len(x))] @ T.T)[:, :3]
            dx, dy = x[:, 0], x[:, 1] - yc
            return x[:, 2], np.arctan2(dx, -dy), np.hypot(dx, dy)
        zs, ts, rs = cyl(sk.posed(mats))
        zl, tl, rl = cyl(lg.posed(mats))
        sel = (zl < top_z - 0.15 * (top_z - zs.min())) & (zl > zs.min())
        out = {"loin_f": -1.0, "loin_b": -1.0}
        if not sel.any():
            return out
        zl, tl, rl = zl[sel], tl[sel], rl[sel]
        da = np.abs((ts[None, :] - tl[:, None] + np.pi) % (2 * np.pi) - np.pi)
        near_a = da < self.SKIRT_WIN[0]
        hem = np.where(near_a, zs[None, :], np.inf).min(axis=1)
        near = near_a & (np.abs(zs[None, :] - zl[:, None]) < self.SKIRT_WIN[1])
        rmin = np.where(near, rs[None, :], np.inf).min(axis=1)
        ok = np.isfinite(rmin) & (zl > hem + 0.004)
        ex = np.where(ok, rl + self.SKIRT_CLEAR - rmin, -1.0)
        sector = math.radians(self.CHAIN_SECTOR)
        for nm, m in (("loin_f", np.abs(tl) < sector), ("loin_b", np.abs(tl) > math.pi - sector)):
            if m.any():
                out[nm] = float(ex[m].max())
        return out

    def flaps(self, p, bp, **kw) -> None:
        """The shaman's skirt envelope (a chain turns out while a leg in its sector presses through the skirt) plus the
        floor: a chain whose skirt would go under the floor (kneeling) keeps turning out until it lies on it — the
        front and back panels pool out over the ground."""
        GS.GoblinAnims.flaps(self, p, bp, **kw)
        if self.skirt is None or kw.get("extra"):
            return
        from kit import anim as kanim
        bones = self.rig.obj.data.bones

        sk = self.skirt[0]
        if not hasattr(self, "_chain_masks"):            # skirt points carried mostly by each chain (rest weights)
            self._chain_masks = {}
            for nm in ("loin_f", "loin_b"):
                ids = [sk.bidx[f"{nm}_01"], sk.bidx[f"{nm}_02"]]
                share = (np.isin(sk.idx, ids) * sk.w).sum(axis=1)
                self._chain_masks[nm] = share > 0.2

        def measure():
            mats = kanim.evaluate(p, return_matrices=True)
            ex = self.skirt_excess(mats)
            z = sk.posed(mats)[:, 2]
            gd = {nm: self.GROUND_CLEAR - float(z[m].min()) if m.any() else -1.0
                  for nm, m in self._chain_masks.items()}
            return ex, gd
        # no freezing (the shaman's solver gave up on a chain after one step that did not help; restricted to the
        # chains' sectors every deficit yields to turning, but not always within the first 3°)
        extra = {"loin_f": 0.0, "loin_b": 0.0}
        ex, gd = measure()
        for _ in range(int(self.SKIRT_MAX_EXTRA / self.SKIRT_STEP)):
            bump = [nm for nm in extra if ex[nm] > 0.0 or gd[nm] > 0.0]
            if not bump:
                break
            for nm in bump:
                extra[nm] += self.SKIRT_STEP
            GS.GoblinAnims.flaps(self, p, bp, extra=extra, **kw)
            ex, gd = measure()


def anims(kit) -> HerdAnims:
    A = HerdAnims(kit, REF_SPEED, WALK, ready, extra_cloth=herd_cloth, style="cast", contact_wpn=40.0,
                  web_ready=WEB_READY)
    install_poses(A)
    return A


# Walk (speed 45 → 1.35 m/s): a short, quick shuffle under the long skirt — the crook carried upright and swung a
# little with each step, the free hand kept clutching the shawl (no arm swing), the stoop deepened
WALK = dict(cycle=560.0, duty=0.56, lift=0.026 * GROW, bob=0.010 * GROW, lean=4.0, arm=0.0, crouch=-0.015 * GROW,
            twist=4.0, hand_swing=0.0)

# Attack01 (the bell-toll bolt, [W1] ranged 3.0): authored contact = release of the bolt from the bell's mouth
ATTACK_MS, CONTACT_MS = 1000.0, 500.0
WINDUP_END_MS = round(0.62 * CONTACT_MS, 1)
CAST_MS, RING_MS = 1300.0, 650.0          # Cast01 (召羊 Call the Flock [W2]): the big ring
TAUNT_MS = 2000.0                         # Taunt (the pre-fight lines; seamless loop while they are held)
IDLE_MS = 1600.0


def install_poses(A) -> None:
    """Attack01 keys (the crook drawn back, then swung forward so the bell tolls at the target), the hurt recoil
    and the death keys."""
    def attack_keys():
        R0 = A.ready()
        # wind: the crook hauled back and up, the bell lagging forward; she straightens out of her stoop
        wind = R0.but(lean=R0.lean - 7.0, twist=R0.twist - 12.0, hip_yaw=-8.0, head_pitch=R0.head_pitch + 4.0,
                      hand_r=V(-0.30, -0.02, 0.70), elbow_r=V(-0.6, -0.3, -0.7), **A._wkw(R0, -24.0, lat=0.06),
                      hand_l=V(0.12, 0.23, 0.60), curl_l=50.0, bell=26.0, flow=0.2)
        mid = R0.but(lean=R0.lean - 2.0, twist=R0.twist - 2.0, head_pitch=R0.head_pitch - 2.0,
                     hand_r=V(-0.29, 0.09, 0.75), elbow_r=V(-0.6, -0.3, -0.7), **A._wkw(R0, 10.0, lat=0.04),
                     hand_l=V(0.12, 0.23, 0.61), bell=-34.0, flow=0.3)
        # contact: the crook thrust out at the target, the hook and bell out front at chest height, the bell
        # flung forward (the toll); the free hand opens off the breast toward the flock
        hit = R0.but(lean=R0.lean + 9.0, twist=R0.twist + 12.0, hip_yaw=4.0, head_pitch=R0.head_pitch - 8.0,
                     hand_r=V(-0.27, 0.25, 0.74), elbow_r=V(-0.5, -0.1, -0.8), **A._wkw(R0, 32.0, lat=0.02),
                     foot_r=A.web_foot(WEB_READY["footN"] + 1.5, R0.foot_r.x),
                     hand_l=V(0.09, 0.20, 0.64), curl_l=20.0, bell=58.0, flow=0.35)
        return [A.look(wind, 0.6), A.look(mid, 0.6), A.look(hit, 0.6)]

    def recoil():
        R0 = A.ready()
        return R0.but(lean=R0.lean - 16.0, head_pitch=R0.head_pitch - 14.0, head_yaw=10.0, twist=R0.twist - 8.0,
                      pelvis=R0.pelvis + V(0.0, -0.025, -0.012), hand_r=V(-0.26, -0.03, 0.66),
                      **A._wkw(R0, -14.0, lat=0.0), hand_l=V(0.13, 0.20, 0.64), curl_l=30.0, bell=-32.0,
                      flow=0.45)

    A.overrides.update(attack_keys=attack_keys, recoil=recoil)


def attack(A):
    """Attack01 1000 ms (the bell-toll bolt [W1]: ``Contact`` = ``Release`` = ``FX_BellToll`` at 500, the bolt
    leaves ``fx_bell``; ``WindupEnd`` 310): READY → wind (300, out) → mid (420, in) → contact (500, linear) →
    follow (650: the bell swings back) → READY (1000)."""
    R0 = A.ready()
    wind, mid, hit = A.attack_keys()
    follow = hit.but(hand_r=hit.hand_r + V(0.0, 0.01, 0.01), **A._wkw(R0, 26.0, lat=0.02), bell=-20.0,
                     curl_l=35.0)
    back = R0.but(lean=R0.lean + 3.0, hand_r=R0.hand_r + V(0.0, 0.04, 0.0), **A._wkw(R0, 14.0, lat=0.03),
                  bell=14.0)
    S = ATTACK_MS
    keys = [K(0.0, R0), K(300.0 / S, wind, "out"), K(420.0 / S, mid, "in"), K(CONTACT_MS / S, hit, "linear"),
            K(650.0 / S, follow, "out"), K(820.0 / S, back, "smooth"), K(1.0, R0, "smooth")]
    nt = {"WindupEnd": WINDUP_END_MS, "Contact": CONTACT_MS, "Release": CONTACT_MS, "FX_BellToll": CONTACT_MS}
    c = A.clip("Attack01", S, keys, S, notifies=nt)
    c.key_poses_ms = [0.0, 300.0, 420.0, CONTACT_MS, 650.0, 820.0, S]
    c.manifest_extra = {"ranged": True, "attackRange": 3.0, "contactMs": CONTACT_MS,
                        "projectile": "the bell-toll bolt: a breach-violet ripple (#B02FD0) from fx_bell (the bell's "
                                      "mouth) at Release; on hit slow 20 % for 2 s (monsters.md 2.5)"}
    return c


def cast(A):
    """Cast01 1300 ms (召羊 Call the Flock [W2]): the crook raised high overhead as she rises out of her stoop, the
    free hand lifted palm-down over the flock, the crook shaken so the bell swings and rings — the big ring
    ``FX_bell_ring`` (+ ``Release``: the raise marker) at 650 — then lowered to READY."""
    R0 = A.ready()
    up = R0.but(lean=R0.lean - 15.0, head_pitch=R0.head_pitch - 6.0, twist=R0.twist - 4.0,
                pelvis=R0.pelvis + V(0.0, 0.0, 0.015),
                hand_r=V(-0.27, 0.12, 1.00), elbow_r=V(-0.8, -0.2, -0.5), **A._wkw(R0, 4.0, lat=0.10),
                hand_l=V(0.17, 0.23, 0.84), elbow_l=V(0.6, -0.2, -0.7), curl_l=8.0, clav_r=(12.0, 0.0),
                clav_l=(8.0, 0.0), bell=-10.0, flow=0.25)
    S = CAST_MS
    keys = [K(0.0, R0), K(380.0 / S, up, "out"), K(900.0 / S, up.but(bell=0.0), "linear"), K(1.0, R0, "smooth")]

    def layer(bp, t):
        bp = A.waver(S)(bp, t)
        if 380.0 <= t <= 900.0:                     # the shake: the crook rocks, the bell swings and rings
            k = math.sin(math.pi * (t - 380.0) / 520.0)
            ph = (t - 380.0) * 2.0 * math.pi / 260.0
            th = 4.0 + 9.0 * k * math.sin(ph)
            d, u_ = C.blade(th, lat=0.10)
            bp = bp.but(wpn=d, wpn_up=u_, bell=bp.bell + 26.0 * k * math.sin(ph - 1.2),
                        hand_r=bp.hand_r + V(0.0, 0.012 * k * math.sin(ph), 0.0))
        return bp
    nt = {"FX_bell_ring": RING_MS, "Release": RING_MS}
    c = A.clip("Cast01", S, keys, S, notifies=nt, layer=layer)
    c.key_poses_ms = [0.0, 380.0, RING_MS, 900.0, S]
    c.manifest_extra = {"usage": "[W2] 召羊 Call the Flock (chapter1.md 10): Cast01 rings the bell; FX_bell_ring = "
                                 "the breach-violet ripple from fx_bell (A12 recipe; the runtime resolves a notify "
                                 "as notify.<name>)", "skill": "corpseherd_call_flock"}
    return c


def taunt(A):
    """Taunt 2000 ms (the three pre-fight lines, under line 1 "Hush... don't startle my flock."; a seamless loop
    while they are held): stooped over the planted crook she sways from hip to hip as if lulling a child in her arms
    — the hips, the hooded head and the bell on the crook all rock (``TAUNT_SWAY``: big enough to read at the W1
    camera, where the old ±4° rocking was Idle) — and the long left hand comes up to the veil, the palm turned to her
    lips (the hush, ``TAUNT_LIPS``), holds there for a third of the loop, then sinks out over the flock palm-down and
    comes back."""
    R0 = A.ready()
    base = R0.but(lean=R0.lean - 2.0, head_pitch=R0.head_pitch - 6.0, elbow_l=V(0.8, -0.1, -0.6), clav_l=(8.0, 0.0),
                  flow=0.1)
    S = TAUNT_MS
    side, hip, head, follow, bell = TAUNT_SWAY
    lips, lips_wr, lips_curl = TAUNT_LIPS
    flock, flock_wr, flock_curl = TAUNT_FLOCK

    def s(t):
        u = t / S
        ph = 2.0 * math.pi * u
        rock = math.sin(ph)
        # the hand at the lips around u = .5 (held ≈ a third of the loop), out over the flock around u = 0
        k = min(1.0, max(0.0, (0.5 - 0.5 * math.cos(ph) - 0.22) / 0.66))
        w = k * k * (3.0 - 2.0 * k)
        hand = flock.lerp(lips, w) + V(follow * rock, 0.0, 0.006 * math.sin(2 * ph))
        wr = tuple(a + (b - a) * w for a, b in zip(flock_wr, lips_wr))
        bp = base.but(side=side * rock, hip_roll=-0.35 * side * rock, twist=base.twist + 4.0 * rock,
                      head_roll=head * math.sin(ph - 0.35), lean=base.lean + 1.5 * math.sin(2 * ph),
                      head_pitch=base.head_pitch + 3.0 * w,
                      pelvis=base.pelvis + V(hip * rock, 0.0, -0.010 * rock * rock),
                      hand_l=hand, wrist_l=wr, curl_l=flock_curl + (lips_curl - flock_curl) * w,
                      hand_r=base.hand_r + V(0.35 * hip * rock, 0.0, 0.004 * math.sin(2 * ph)),
                      bell=6.0 * math.sin(ph + 0.9), bell_side=_bell_side(bell * math.sin(ph - 0.7)), wave=ph,
                      sway=0.35 * rock)
        return bp
    c = C.body_clip(A.rig, "Taunt", S, s, cloth=A.cloth, loop=True, post=A.plant)
    c.key_poses_ms = [0.0, S * 0.25, S * 0.5, S * 0.75, S]
    c.manifest_extra = {"usage": "pre-fight dialogue of miniboss_corpseherd (chapter1.md 10: hold her until the "
                                 "three lines are dismissed; line 1 'Hush... don't startle my flock.'): the lulling "
                                 "sway, the hand to the veil (the hush) around 1000 ms; seamless loop"}
    return c


# Taunt: (spine roll deg, pelvis sway m, head roll deg, free hand follow m, bell side-swing deg) — the lulling sway;
# the hush at the lips (wrist target ×1.3 design m, wrist_l (pitch, roll, yaw), finger curl) under the eyes, in front
# of the veil; the hand out over the flock (palm down, the long fingers drooping)
TAUNT_SWAY = (10.0, 0.024, 12.0, 0.07, 16.0)


def _bell_side(a: float) -> float:
    """The bell's sideways swing: free outward (her right, < 0: the hook opens that way), a third inward (+), where
    the shaft stands 5 cm inside the bell's rim."""
    return a if a < 0.0 else a / 3.0
TAUNT_LIPS = (V(0.015, 0.31, 0.80), (-70.0, 0.0, 10.0), -40.0)
TAUNT_FLOCK = (V(0.15, 0.33, 0.62), (-15.0, 0.0, 0.0), 30.0)


def idle(A):
    """Idle 1600 ms loop: slow breathing, a faint rocking from foot to foot, the bell swaying on its cord."""
    R0 = A.ready()
    S = IDLE_MS

    def s(t):
        ph = 2.0 * math.pi * t / S
        b = math.sin(ph)
        return R0.but(pelvis=R0.pelvis + V(0.004 * math.sin(ph * 0.5 * 2), 0.0, 0.004 * b), lean=R0.lean - 1.0 * b,
                      side=1.6 * math.sin(ph + 0.6), head_pitch=R0.head_pitch + 1.5 * math.sin(ph - 0.6),
                      head_roll=2.5 * math.sin(ph + 0.4), hand_r=R0.hand_r + V(0.0, 0.0, 0.004 * b),
                      hand_l=R0.hand_l + V(0.0, 0.0, 0.004 * math.sin(ph - 0.5)), bell=4.0 * math.sin(ph + 1.4),
                      wave=ph, sway=0.15 * math.sin(ph + 1.0))
    return C.body_clip(A.rig, "Idle", S, s, cloth=A.cloth, loop=True, post=A.plant)


def walk_hand(bp: BP, t: float, sw: float) -> BP:
    """Walk: the free hand stays at the breast (the kit walk swings it); the crook swings with the steps."""
    R0 = READY_CACHE["bp"]
    th = READY_CROOK[0] + 7.0 * sw
    d, u_ = C.blade(th, lat=READY_CROOK[1])
    return bp.but(hand_l=R0.hand_l + V(0.0, 0.0, 0.004 * abs(sw)), curl_l=R0.curl_l, wpn=d, wpn_up=u_,
                  hand_r=bp.hand_r + V(0.0, 0.02 * sw, 0.01 * abs(sw)), bell=bp.bell - 12.0 * sw)


READY_CACHE: dict = {}


def death(A):
    """Death 1400 ms: the blow straightens her (recoil), the knees give and she sinks onto her knees, the skirt
    pooling round her on the floor (the panels laid out on the ground by ``HerdAnims.flaps``, the sides following the
    shins), then she folds forward over her knees, the hood bowed low over her right hand; the crook tips over to her
    right front and slides through her loosening fist as it falls (``slide``: ``weapon_r`` moved down its own axis), so
    it comes to rest along her right side with the fist closed under the hook and the bell on the ground against her
    knuckles (``cs_rf_herd``: "her hand still on the bell" — she can ring it once more; "the dead around her sit
    down"). Art review r1: the old end left her hand on the shaft and the bell a metre away at the hook."""
    R0 = A.ready()
    rec = A.recoil()
    D = DEATH_KNEEL                                    # hips down: kneeling upright, the shins flat on the floor
    shin = A.rig.spec.shin
    kneel_feet = dict(foot_l=C.V(0.075 * GROW, -shin * 0.92, A.AZ + 0.012 * GROW),
                      foot_r=C.V(-0.07 * GROW, -shin * 0.92, A.AZ + 0.012 * GROW),
                      foot_l_rot=(-70.0, 0.0, 6.0), foot_r_rot=(-70.0, 0.0, 6.0), knee_l=V(0.2, 1, -0.3),
                      knee_r=V(-0.2, 1, -0.3))
    buckle = rec.but(pelvis=R0.pelvis + V(0.0, 0.02, -D * 0.45), lean=R0.lean + 6.0, head_pitch=R0.head_pitch + 4.0,
                     head_roll=6.0, knee_l=V(0.3, 1, 0), knee_r=V(-0.3, 1, 0), hand_r=V(-0.33, 0.06, 0.48),
                     elbow_r=V(-0.4, -0.5, -0.7), **A._wkw(R0, 12.0, lat=0.06), hand_l=V(0.10, 0.18, 0.52),
                     curl_l=50.0, bell=-20.0, flow=0.4)
    kneel = buckle.but(pelvis=R0.pelvis + V(0.0, 0.03, -D), lean=R0.lean + 10.0, head_pitch=R0.head_pitch + 10.0,
                       head_roll=8.0, **kneel_feet, hand_r=V(-0.35, 0.12, 0.36), **A._wkw(R0, 30.0, lat=-0.04),
                       hand_l=V(0.10, 0.20, 0.36), curl_l=55.0, bell=25.0, flow=0.35)
    # the last third (cs_rf_herd: "her hand still on the bell" — she rings it once more after the fall): folding
    # forward over her knees, the crook tips over to her right front and slides through her loosening fist as it
    # falls (``slide``), so the fist closes under the hook; it comes to rest along her right side, the hook and the
    # bell beside her bowed hood, the bell lying against her knuckles
    slump = kneel.but(lean=R0.lean + 34.0, head_pitch=R0.head_pitch + 22.0, head_roll=10.0,
                      hand_r=V(-0.36, 0.22, 0.20), **A._wkw(R0, DEATH_SLUMP_CROOK, lat=0.06),
                      slide=DEATH_SLIDE * 0.5, hand_l=V(0.09, 0.26, 0.16), curl_l=40.0, bell=-40.0,
                      bell_side=-20.0, flow=0.25)
    rest = slump.but(lean=R0.lean + 44.0, head_pitch=R0.head_pitch + 30.0, head_roll=12.0,
                     hand_r=V(*DEATH_HAND_R), **A._wkw(R0, 90.0, lat=DEATH_CROOK_LAT), slide=DEATH_SLIDE,
                     hand_l=V(0.08, 0.28, 0.05), wrist_l=(-10.0, 0.0, 0.0), curl_l=60.0, bell=DEATH_BELL[0],
                     bell_side=DEATH_BELL[1], flow=0.05)
    S = 1400.0
    keys = [K(0.0, rec), K(160.0 / S, rec, "out"), K(420.0 / S, buckle, "in"), K(620.0 / S, kneel, "out"),
            K(1000.0 / S, slump, "in"), K(1.0, rest, "out")]
    g = A.grounded(snap=True)          # twice: lifted, the floor solver relaxes the chains and the hem settles again
    c = A.clip("Death", S, keys, S, post=lambda bp, t: g(g(bp, t), t))
    c.key_poses_ms = [0.0, 160.0, 420.0, 620.0, 1000.0, S]
    return c


DEATH_KNEEL = 0.215                # m the hips sink (kneeling upright: thigh + knee ≈ the hip height)
DEATH_SLIDE = 0.60                 # m the crook slides through the fist (item metres: the grip → just under the hook)
DEATH_SLUMP_CROOK = 66.0           # deg from vertical while it tips over (slump, 1000 ms)
DEATH_CROOK_LAT = 0.12             # the shaft points forward and a little inward: the butt lies out past her right side
DEATH_HAND_R = (-0.33, 0.30, 0.06)  # the fist on the floor beside the bowed hood (×1.3 design metres)
DEATH_BELL = (-88.0, 0.0)          # (bell, bell_side): the bell laid back along the shaft, under the knuckles


def stun(A):
    """Stun 700 ms loop: dazed, leaning on the crook, the head lolling inside the hood."""
    R0 = A.ready()
    S = 700.0

    def s(t):
        ph = 2.0 * math.pi * t / S
        c_, sn = math.cos(ph), math.sin(ph)
        return R0.but(pelvis=R0.pelvis + V(0.006 * c_, 0.01, -0.02 + 0.006 * sn), lean=R0.lean + 6.0 + 2.0 * sn,
                      side=4.0 * c_, head_pitch=R0.head_pitch + 14.0 + 5.0 * sn, head_roll=12.0 * c_,
                      head_yaw=8.0 * sn, hand_r=R0.hand_r + V(0.0, 0.01, -0.005), **A._wkw(R0, 9.0 + 3.0 * sn,
                                                                                          lat=0.03),
                      hand_l=R0.hand_l + V(0.0, -0.01, -0.05), curl_l=40.0, bell=8.0 * c_, flow=0.15, wave=ph,
                      sway=0.4 * c_)
    return C.body_clip(A.rig, "Stun", S, s, cloth=A.cloth, loop=True, post=A.plant)


def hurt(A):
    """Hurt 300 ms: recoil (the bell flung back) → partial recovery → READY."""
    R0 = A.ready()
    rec = A.recoil()
    part = R0.but(lean=R0.lean - 6.0, head_pitch=R0.head_pitch - 5.0, hand_r=R0.hand_r + V(0, -0.01, 0.02),
                  **A._wkw(R0, -4.0, lat=0.02), bell=18.0, flow=0.25)
    S = 300.0
    keys = [K(0.0, rec), K(0.45, part, "out"), K(1.0, R0, "smooth")]
    c = A.clip("Hurt", S, keys, S)
    c.key_poses_ms = [0.0, 135.0, S]
    return c


def clips(A) -> list:
    READY_CACHE["bp"] = A.ready()
    cs = [idle(A), A.walk(extra=walk_hand), attack(A), cast(A), hurt(A), death(A), taunt(A), stun(A)]
    for c in cs:                     # weapon_r's location is keyed in every clip (zero but in Death's slide): an unkeyed
        c.key_locations = ("weapon_r",)      # channel kept Death's 60 cm in the clips exported / previewed after it
    return cs


# ── manifest / review ──────────────────────────────────────────────────────────────────────────────────────
GRIP_PARTS = ("hand_r", "finger_r", "thumb_r", "bell", "bell_mouth", "bell_cord", "bell_loop", "bell_clapper",
              "crook_rag", "bell_crack", "bell_patina")
# cloth × limb gate: the skirt against the legs, the shawl against the forearms / cuffs / hands (the upper-arm
# tubes root under the shawl by construction), the hood against the free hand (the hush)
QA_GROUPS = {"skirt": ["skirt", "skirt_gore", "skirt_hem"], "legs": ["leg"], "shawl": ["shawl"],
             "forearms": ["cuff", "hand_l", "finger_l", "thumb_l", "hand_r", "finger_r", "thumb_r"],
             "hood": ["hood", "veil"], "freehand": ["hand_l", "finger_l", "thumb_l"],
             "bell": ["bell"]}
QA_PAIRS = {"skirtLeg": ("skirt", "legs"), "shawlArm": ("shawl", "forearms"), "hoodHand": ("hood", "freehand")}


def crook_grip(co) -> bool:
    """The fist round the grip (item-local; the QA baseline)."""
    return -0.05 < co.y < 0.05


# ── the eye cards (fx.eyeGlow) ──────────────────────────────────────────────────────────────────────────
EYE_FADE = (3.0, 0.40)          # facingFade = clamp(k · (dot(headFwd, toCamera) − d0), 0, 1): the eyes sit deep in the
                                # hood, so the card is gone by the time the face turns side-on (the Unburied's
                                # 2·dot + .5 kept half a card in the ne / nw views — a violet haze over the hood's
                                # crown, art review r1); full from ≈ 70° off the camera inward
EYE_FADE_EXPR = f"clamp({EYE_FADE[0]:g} * (dot(headFwd, toCamera) - {EYE_FADE[1]:g}), 0, 1)"


def _head_fwd(rig) -> Vector:
    hb = rig.obj.data.bones["head"]
    pb = rig.obj.pose.bones["head"]
    return (rig.obj.matrix_world.to_3x3() @ pb.matrix.to_3x3() @ hb.matrix_local.to_3x3().inverted()
            @ Vector((0, -1, 0))).normalized()


def _socket_world(rig, name: str) -> Vector:
    so = next(x for x in rig.sockets if x["name"] == name)
    b = rig.obj.data.bones[so["bone"]]
    pb = rig.obj.pose.bones[so["bone"]]
    return rig.obj.matrix_world @ (pb.matrix @ (b.matrix_local.inverted() @ Vector(so["pos"])))


def eye_seen(cam_pos: Vector, wp: Vector, body, names: list, tries: int = 6) -> bool:
    """True when the first toon surface on the ray camera → eye is the eye itself (the baked ink hull is passed
    through): the hood's brow, crown or cheek between them hides the card, as the runtime's depth-tested card."""
    dg = bpy.context.evaluated_depsgraph_get()
    ev = body.evaluated_get(dg).data
    part = ev.attributes.get("af_part")
    d = wp - cam_pos
    dist = d.length
    d.normalize()
    o = cam_pos.copy()
    for _ in range(tries):
        hit, loc, _n, idx, obj, _m = bpy.context.scene.ray_cast(dg, o, d, distance=dist - (o - cam_pos).length)
        if not hit:
            return True
        if (loc - wp).length < 0.02 * GROW:
            return True
        if obj is not None and obj.original != body and obj.original.hide_render:
            o = loc + d * 1e-4                                  # not drawn (the LOD1 body, review helpers)
            continue
        if obj is not None and obj.original == body and idx < len(ev.polygons):
            if ev.polygons[idx].material_index == 1:            # the ink hull: look through it
                o = loc + d * 1e-4
                continue
            if part is not None and names[part.data[idx].value].startswith("eye"):
                return True
        return False
    return True


def eye_glows(rig, body=None):
    """Review glow cards over the eyes (manifest ``fx.eyeGlow``: breach.pinpoint #D08CFF, never ember): faded by
    ``EYE_FADE`` and hidden where the hood stands between the camera and the eye (``eye_seen``)."""
    names = json.loads(body["af_parts"]) if body is not None else None

    def fn(cam, res):
        from kit import review
        out = []
        fwd = _head_fwd(rig)
        cp = cam.matrix_world.translation
        for s in ("l", "r"):
            wp = _socket_world(rig, f"fx_eye_{s}")
            vis = max(0.0, min(1.0, EYE_FADE[0] * (fwd.dot((cp - wp).normalized()) - EYE_FADE[1])))
            if vis > 0.0 and body is not None and not eye_seen(cp, wp, body, names):
                vis = 0.0
            px, py = review.project_px(cam, wp, res)
            wpp = outline.pixel_world_size(cam, wp, res[0])
            out.append(dict(center_px=(px, py), radius_px=UB.EYE_FX["radiusCm"] / 100.0 / wpp, color=UB.EYE_GLOW,
                            alpha=UB.EYE_FX["alpha"] * vis, core="#F4E8FF", core_frac=0.3, core_alpha=0.45 * vis))
        return out
    return fn


def bell_glows(rig, body=None):
    """Review glow cards: the violet eye pinpoints (``fx.eyeGlow``)."""
    return eye_glows(rig, body)


FX = {"eyeGlow": dict(UB.EYE_FX, facingFade=EYE_FADE_EXPR, depthTest=True,
                     note="breach-violet pinpoints in the hood's void (monsters.md 1.2: Corpseherd eyes #D08CFF); never "
                          "ember. facingFade is the Corpseherd's own (stricter than the Unburied's: the eyes sit deep "
                          "in the hood, so a side-on or turned-away face shows no card) and the card is depth-tested: "
                          "the hood's crown and brow hide it (no violet glint over the crown in the ne / nw views)"),
      "bellToll": {"notify": "FX_BellToll", "atMs": CONTACT_MS, "socket": "fx_bell", "color": "#B02FD0",
                   "coreColor": "#1A0820", "note": "the bolt (derived.projectileColor #B02FD0) leaves the bell's "
                                                   "mouth on Release"},
      "bellRing": {"notify": "FX_bell_ring", "atMs": RING_MS, "socket": "fx_bell", "recipe": "notify.FX_bell_ring",
                   "color": "#B02FD0", "note": "monsters.md 2.5 names the recipe notify.bell_ring; the runtime "
                                                "(UAbyssVfxSystem::PlayNotify) resolves notify.<notify name> = "
                                                "notify.FX_bell_ring — A12 authors it under that id (or an alias)"},
      "telegraph": {**GN.TELEGRAPH, "untilMs": WINDUP_END_MS},
      "eliteCrown": {"socket": "fx_overhead", "asset": "SM_FX_EliteCrown", "themedId": "elite_crown@march"}}


def portrait_pose(A) -> BP:
    """Portrait pose (``T_UI_Portrait_Corpseherd``: hood, veil, eyes, bell): the crook drawn in beside the hood so the
    bell hangs by her face, the head lifted out of the hood toward the viewer so the violet eyes read over the
    veil."""
    R0 = A.ready()
    return R0.but(head_pitch=R0.head_pitch - 10.0, head_yaw=-12.0, lean=R0.lean - 8.0,
                  hand_r=V(*PORTRAIT_HAND_R), elbow_r=V(-0.6, -0.3, -0.7), **A._wkw(R0, PORTRAIT_CROOK[0],
                                                                                   lat=PORTRAIT_CROOK[1]),
                  hand_l=V(0.08, 0.17, 0.68), curl_l=65.0, bell=0.0, flow=0.1)


PORTRAIT_HAND_R = (-0.25, 0.14, 0.50)   # the crook hand (×1.3 design m): low enough that the bell hangs beside the hood
PORTRAIT_CROOK = (12.0, 0.06)           # the crook leant in toward her right shoulder
PORTRAIT_YAW, PORTRAIT_PITCH = -118.0, -10.0
PORTRAIT_FRAME = (0.15, 0.30, 0.25, 0.035)    # m above / below the eyes, either side of them, and the shift toward the
                                        # bell: the eyes on the upper third, near the centre (the old bust framing — every vertex within 40 cm — gave the crook
                                        # and the bell the upper-left half and put the face low on the right)
PORTRAIT_EYE_K = 0.45                   # the eyes drawn at 45 % in the bust (the in-game marker keeps its size): burning
                                        # points in the void, not lenses
PORTRAIT_HALO = {"radiusCm": 2.2, "alpha": 0.75, "hole": 0.30}   # a violet halo round each eye, open at its centre: no
                                        # white core (the screen-blended core turned the eyes lavender-pink, #FEB1FF)


def _violet_halo(img: np.ndarray, center: tuple, radius_px: float, hex_color: str, alpha: float,
                 hole: float) -> None:
    """A soft violet halo (screen blend, quadratic falloff) that leaves the eye's own colour untouched inside
    ``hole`` × the radius (``review.composite_glow`` without its hot core)."""
    from kit import color as kcolor
    h, w = img.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    r = np.hypot(xx + 0.5 - center[0], yy + 0.5 - center[1]) / max(radius_px, 1e-3)
    ring = np.clip((r - hole * 0.6) / (hole * 0.8), 0.0, 1.0)
    g = (alpha * np.clip(1.0 - r, 0.0, 1.0) ** 2 * ring)[:, :, None]
    c = np.array(kcolor.hex_to_rgb(hex_color), np.float32) / 255.0
    rgb = img[:, :, :3].astype(np.float32) / 255.0
    a = img[:, :, 3:4].astype(np.float32) / 255.0
    lit = 1 - (1 - rgb) * (1 - c * g)
    out_a = a + g * (1 - a)
    rgb = (lit * a + c * g * (1 - a)) / np.maximum(out_a, 1e-6)
    img[:, :, 3] = (out_a[:, :, 0] * 255 + 0.5).astype(np.uint8)
    img[:, :, :3] = (rgb.clip(0, 1) * 255 + 0.5).astype(np.uint8)


class _PortraitEyes:
    """Portrait-only eye look: the eye vertices scaled to ``PORTRAIT_EYE_K`` about their centre in the body mesh and
    the toon's emissive boost off (the eye renders its swatch #D08CFF instead of the overbright #FFAEFF the game's
    bloom starts from); restored on exit."""

    def __init__(self, body):
        self.body = body

    def __enter__(self):
        me = self.body.data
        names = json.loads(self.body["af_parts"])
        nf = len(me.polygons)
        fp = np.zeros(nf, np.int32)
        me.attributes["af_part"].data.foreach_get("value", fp)
        co = np.zeros(len(me.vertices) * 3, np.float32)
        me.vertices.foreach_get("co", co)
        self.co = co.copy()
        co = co.reshape(-1, 3)
        for nm in ("eye_l", "eye_r"):
            if nm not in names:
                continue
            fids = np.nonzero(fp == names.index(nm))[0]
            vids = sorted({v for i in fids for v in me.polygons[int(i)].vertices})
            c = co[vids].mean(0)
            co[vids] = c + (co[vids] - c) * PORTRAIT_EYE_K
        me.vertices.foreach_set("co", co.ravel())
        me.update()
        self.nodes = []
        for ng in list(bpy.data.node_groups) + [m.node_tree for m in bpy.data.materials if m.node_tree]:
            for nd in ng.nodes:
                if (nd.bl_idname == "ShaderNodeMath" and nd.operation == "MULTIPLY_ADD"
                        and abs(nd.inputs[1].default_value - shading.EMISSIVE_BOOST) < 1e-6
                        and abs(nd.inputs[2].default_value - 1.0) < 1e-6):
                    self.nodes.append(nd)
                    nd.inputs[1].default_value = 0.0
        bpy.context.view_layer.update()
        return self

    def __exit__(self, *exc):
        me = self.body.data
        me.vertices.foreach_set("co", self.co)
        me.update()
        for nd in self.nodes:
            nd.inputs[1].default_value = shading.EMISSIVE_BOOST
        bpy.context.view_layer.update()
        return False


def portrait(ms, rv, actions) -> str:
    """``T_UI_Portrait_Corpseherd`` (``mship.portrait_shot``'s render: 512² RGBA, toon + screen-space ink, no bloom)
    with the Corpseherd's own framing and eyes: the camera fits a box round the eyes (``PORTRAIT_FRAME``: hood and
    eyes near the upper-third centre, the bell beside the hood), the eyes drawn as small #D08CFF points
    (``_PortraitEyes``) with a violet halo open at its centre (``_violet_halo``)."""
    import mship
    from kit import anim, paths, pngio, review
    rig = ms.A.rig
    mship.apply_bp(ms.A, portrait_pose(ms.A))
    bpy.context.view_layer.update()
    rv.facing(PORTRAIT_YAW)
    res = (512, 512)
    review.hide_stage(True)
    review.setup_render(res, 16)
    vis = [m for m in [ms.body] + [mship.shown(it) for it in mship.held(ms)] if not m.hide_render]
    eyes = [_socket_world(rig, "fx_eye_l"), _socket_world(rig, "fx_eye_r")]
    ec = (eyes[0] + eyes[1]) * 0.5
    f = np.array(review.cam_dir(shading.CAM_YAW_UE, PORTRAIT_PITCH))
    r = np.cross(f, (0, 0, 1.0))
    r /= np.linalg.norm(r)
    u = np.cross(r, f)
    up_, dn_, side, toward_bell = PORTRAIT_FRAME
    c = np.array(ec) - r * toward_bell
    box = np.array([c + u * up_, c - u * dn_, c + r * side, c - r * side, c + u * up_ + r * side,
                    c + u * up_ - r * side, c - u * dn_ + r * side, c - u * dn_ - r * side])
    cam = review.frame_points(box, res, fov_h=24, pitch_ue=PORTRAIT_PITCH, fill=1.0, include_ground=False)
    for m in vis:
        outline.remove_preview(m)
    with _PortraitEyes(ms.body):
        rgba = review.ink_render(vis, res, silhouette_px=mship.PORTRAIT_INK_PX, interior_px=2.0, depth_step=0.018)
    fwd = _head_fwd(rig)
    for wp in eyes:
        vis_k = max(0.0, min(1.0, EYE_FADE[0] * (fwd.dot((cam.matrix_world.translation - wp).normalized())
                                                 - EYE_FADE[1])))
        cx, cy = review.project_px(cam, wp, res)
        rad = PORTRAIT_HALO["radiusCm"] / 100.0 / outline.pixel_world_size(cam, wp, res[0])
        _violet_halo(rgba, (cx, cy), rad, UB.EYE_GLOW, PORTRAIT_HALO["alpha"] * vis_k, PORTRAIT_HALO["hole"])
    outp = paths.export_root() / "Portraits" / f"{PORTRAIT}.png"
    outp.parent.mkdir(parents=True, exist_ok=True)
    pngio.write_png(outp, rgba)
    review.hide_stage(False)
    bg = np.array([59, 53, 70], np.float32)
    a = rgba[:, :, 3:4].astype(np.float32) / 255.0
    comp = (rgba[:, :, :3].astype(np.float32) * a + bg * (1 - a) + 0.5).astype(np.uint8)
    review.draw_text(comp, 4, 4, PORTRAIT.upper())
    rv._save("portrait.png", comp)
    cx, cy = review.project_px(cam, ec, res)
    x0 = int(min(max(cx - 128, 0), 256))
    y0 = int(min(max(cy - 96, 0), 256))
    z = comp[y0:y0 + 256, x0:x0 + 256].repeat(2, 0).repeat(2, 1)
    review.draw_text(z, 4, 4, "PORTRAIT 2X ZOOM")
    rv._save("portrait_zoom2x.png", z)
    anim.assign(rig, None)
    anim.clear_pose(rig)
    rv.facing("se")
    print(f"[portrait] {outp}")
    return f"Portraits/{PORTRAIT}.png"


def build_lod1(pal, kit):
    """LOD1 within the 3.4 k budget (the ratio scaled down until the decimated parts fit)."""
    import mship
    ratio = LOD1_RATIO
    for _ in range(6):
        body1 = build_body(pal, kit, lod=1, ratio=ratio)
        n = mship.toon_tris(body1)
        if n <= LOD1_BUDGET:
            break
        me = body1.data
        bpy.data.objects.remove(body1, do_unlink=True)
        bpy.data.meshes.remove(me)
        ratio *= (LOD1_BUDGET - 100) / n
    print(f"[lod1] {ASSET}: {n} toon tris at ratio {ratio:.3f}")
    return body1


def make_ship():
    import living_ship as LS
    import mship
    GN.march_stage()
    pal, kit, body, crook = build_all()
    body1 = build_lod1(pal, kit)
    A = anims(kit)
    R.attach_to_bone(crook, kit.rig, "weapon_r")
    A.bind_meshes(body, {"weapon_r": crook})
    cs = clips(A)
    items = [mship.Item(crook, CROOK, "weapon_r", "staff",
                        sockets=[{"name": "tip", "pos": tuple(hook_point(180.0))},
                                 {"name": "mid", "pos": (0.0, 0.25, 0.0)},
                                 {"name": "bell_tie", "pos": tuple(BELL_TIE)}],
                        extra={"lengthCm": round((CROOK_DOWN + CROOK_UP + 2 * HOOK_R) * 100.0, 1),
                               "note": "monsters.md 2.5: the totem becomes a shepherd's crook of pale ash-wood "
                                       "(#D9CEB4); the cracked bronze handbell (#8A6A3A) hangs from the hook's end "
                                       "on a cord as part of SK_Mon_Corpseherd (totem_feather_01 under weapon_r: it "
                                       "swings as a pendulum); socket fx_bell = its mouth"},
                        grip=crook_grip)]
    groups = {"body_nf": mship.parts_except(body, GRIP_PARTS), **QA_GROUPS}
    pairs = {"crookBody": ("item:" + CROOK, "body_nf"), "crookBell": ("item:" + CROOK, "bell"), **QA_PAIRS}
    ms = mship.MonsterShip(
        asset=ASSET, token=TOKEN, preview_dir="mon_corpseherd", pal=pal, A=A, body=body, items=items, clips=cs,
        outline_class=OUTLINE_CLASS, game_ids=GAME_IDS, blob_radius=BLOB_R, height=HEIGHT, lod1=body1,
        display=DISPLAY, fx=FX, glows=bell_glows(kit.rig, body),
        qa_groups=groups, qa_pairs=pairs, qa_limits={"ground": -0.5, **{k: 0 for k in pairs}},
        outline_px=OUTLINE_PX, outline_px_note=OUTLINE_PX_NOTE, portrait=portrait, native_sheets=True,
        extra_previews=lambda ms_, rv, acts: LS.corpse_preview(ms_, rv, acts),
        game_keys=[("Idle", 0, "se"), ("Walk", 8, "se"), ("Attack01", 18, "se"), ("Attack01", 30, "se"),
                   ("Cast01", 39, "se"), ("Taunt", 60, "se"), ("Hurt", 0, "se"), ("Death", 84, "se")],
        contact_keys=[("Attack01", 30, f) for f in ("se", "sw", "ne", "nw")],
        close_keys=[("Attack01", 18, "se"), ("Attack01", 30, "se"), ("Attack01", 30, "side"), ("Cast01", 39, "se"),
                    ("Cast01", 39, "front"), ("Taunt", 60, "front"), ("Hurt", 0, "se"), ("Death", 34, "se"),
                    ("Death", 84, "se"), ("Stun", 10, "front")],
        back_clips=("Attack01", "Cast01", "Death"),
        skeleton_bones_required=("root", "pelvis", "head", "jaw", "weapon_r", "weapon_l", "ear_l_02", "loin_f_02",
                                 "pelt_03", "headdress_04", "totem_feather_01", "totem_feather_02", "ik_foot_l",
                                 "ik_hand_gun"),
        lod_note=f"LOD0 budget {LOD0_BUDGET} / LOD1 {LOD1_BUDGET} toon tris (monsters.md 5: the shaman's budget).",
        extra={"refSpeedCmS": round(REF_SPEED * 100.0, 1), "attackStyle": "cast", "ranged": True, "elite": True,
               "miniBoss": True, "scale": 1.0,
               "scaleNote": "monsters.md x1.3 is built into the mesh; the core sets no visual scale. monsters.md 2.5 "
                            "asks x1.3 '(about 1.21 m)': modelled at x1.5 on the goblin shaman's rig unit so the "
                            "stooped READY hood top, the height she is seen at, measures about 120 cm (heightCm 129.7 "
                            "is the rig's unstooped crown). At x1.3 the hood top was 104 cm, child-sized under the "
                            "hood next to the 1.76 m footman (art review r1); x1.4 gave 112 cm. Art scale 1.0 x "
                            "VisualScale 1.0: walk play rate = v / refSpeedCmS with no extra size factor, and the "
                            "baked 1.5 cm ink is the gated 3.5 px",
               "remake": {"family": "unburied", "kit": "monsters/corpseherd.py", "palette": GN.FAMILY,
                          "contactMs": CONTACT_MS,
                          "spec": "Docs/dark-remake/monsters.md 2.5 (family unburied: 2.0), chapter1.md 10",
                          "source": "re-dress of goblin_shaman.py (SKEL_Goblin, the shaman's skirt envelope and cast "
                                    "clip machinery), x1.5 the shaman's rig unit built into the mesh"},
               "marker": {"swatches": list(MARKER), "family": "the Unburied (the Corpseherd)",
                          "kind": "violet pinpoint eyes #D08CFF in the hood's void (emissive; monsters.md 1.2)"},
               "notes": "Clips in place; the core moves the actor (play rate = v / ref). Attack01 releases the "
                        "bell-toll bolt from fx_bell (the handbell's mouth) on Contact 500 (FX_BellToll, Release). "
                        "Cast01 = [W2] Call the Flock (FX_bell_ring 650). Taunt loops under the pre-fight lines. "
                        "Death: she sinks to her knees and folds forward over them, the hood bowed low over her right "
                        "hand; the crook slides through her fist as it falls (weapon_r keys a translation down its "
                        "axis: attach the crook to the weapon_r bone) and lies along her right side, the fist closed "
                        "under the hook and the bell on the ground against her knuckles (cs_rf_herd: her hand still "
                        "on the bell). Taunt: the lulling sway and the hush (the hand at the veil around 1000 ms). The "
                        "family marker is the violet eyes; no ember anywhere (the bell bronze #8A6A3A is hue 36, sat "
                        ".58)."},
    )
    return ms


def ship(previews: bool = True, verify: bool = True) -> int:
    import mship
    ms = make_ship()
    t0, h0 = mship.toon_tris(ms.body), M.tri_count(ms.body, 1)
    t1, h1 = mship.toon_tris(ms.lod1), M.tri_count(ms.lod1, 1)
    print(f"[budget] {ASSET}: LOD0 {t0} toon (<= {LOD0_BUDGET}) + {h0} hull ({h0 / max(1, t0):.2f} x), "
          f"LOD1 {t1} (<= {LOD1_BUDGET}) + {h1} hull ({h1 / max(1, t1):.2f} x)")
    res = mship.ship(ms, previews=previews, verify=verify)
    return mship.main_result(res)


# ── review / ship ───────────────────────────────────────────────────────────────────────────────────────
def quick(out: Path) -> None:
    import mship
    GN.march_stage()
    pal, kit, body, crook = build_all()
    A = anims(kit)
    R.attach_to_bone(crook, kit.rig, "weapon_r")
    A.bind_meshes(body, {"weapon_r": crook})
    mship.quick_review(ASSET, kit.rig, [body], [(crook, "weapon_r")], A, out, HEIGHT, OUTLINE_CLASS,
                       blob_radius=BLOB_R, glows=bell_glows(kit.rig, body), px=OUTLINE_PX)
    print(f"[budget] LOD0 {mship.toon_tris(body)} toon + {M.tri_count(body, 1)} hull; crook "
          f"{M.tri_count(crook, 0)} + {M.tri_count(crook, 1)}")


def anim_quick(out: Path, names, facing: str = "se") -> None:
    import mship
    GN.march_stage()
    pal, kit, body, crook = build_all()
    A = anims(kit)
    R.attach_to_bone(crook, kit.rig, "weapon_r")
    A.bind_meshes(body, {"weapon_r": crook})
    cs = [c for c in clips(A) if not names or c.name in names]
    mship.anim_review(kit.rig, [body], [(crook, "weapon_r")], cs, TOKEN, out, HEIGHT, OUTLINE_CLASS,
                      blob_radius=BLOB_R, facing=facing, px=OUTLINE_PX)


def portrait_quick(out: Path) -> None:
    """Portrait iteration: the portrait + its 2× zoom → ``out`` (and Art/Export/Portraits)."""
    import mship
    GN.march_stage()
    pal, kit, body, crook = build_all()
    A = anims(kit)
    R.attach_to_bone(crook, kit.rig, "weapon_r")
    A.bind_meshes(body, {"weapon_r": crook})
    ms = mship.MonsterShip(asset=ASSET, token=TOKEN, preview_dir="x", pal=pal, A=A, body=body,
                           items=[mship.Item(crook, CROOK, "weapon_r", "staff")], clips=[],
                           outline_class=OUTLINE_CLASS, game_ids=[], blob_radius=BLOB_R, height=HEIGHT)
    out.mkdir(parents=True, exist_ok=True)
    rv = mship.MReview(ASSET, kit.rig.obj, [body, crook], HEIGHT, blob_radius=BLOB_R, out_dir=out,
                       game_outline_px=OUTLINE_PX, outline_class=OUTLINE_CLASS)
    portrait(ms, rv, [])


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    scratch = scratch_root("a6", "corpseherd")
    if "--quick" in args:
        quick(scratch / "quick")
    elif "--anims" in args:
        names = [a for a in args[args.index("--anims") + 1:] if not a.startswith("--")]
        fac = next((a.split("=")[1] for a in args if a.startswith("--facing=")), "se")
        anim_quick(scratch / "anims", names, fac)
    elif "--portrait" in args:
        portrait_quick(scratch / "portrait")
    else:
        sys.exit(ship(previews="--no-previews" not in args, verify="--no-verify" not in args))

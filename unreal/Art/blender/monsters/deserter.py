"""逃兵 Deserter + 军鼓手 Deserter Drummer + the hunts 拾银者·德雷克 / 断誓者·哈克 (dark remake A3; monsters.md §2.3).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/deserter.py [deserter|drummer|all]
        [--quick] [--anims [Clip …]] [--facing=se] [--no-previews] [--no-verify]

* ``SK_Mon_Deserter`` + ``SM_MonWeapon_Hatchet`` (``monster_deserter``; L6 skirmisher, speed 56 → 1.68 m/s, Contact
  ≈ 450 ms): drab gambeson with the insignia torn off (``deserter.gambeson`` #4E5040, quilted, the paler patch
  ``deserter.insignia_scar``), a hood and a scarf over the face (``deserter.hood`` #2F2B26), a looted pack, leather
  (#5E4630: belt, bracers, boots), a hatchet; the family marker: **crimson cloth** — a rag of the crusade tabard
  knotted round the right upper arm and a second one tucked in the belt (``deserter.marker`` #7A1C1E, no glow).
* The hunt leaders are this mesh at ×1.25 with the elite marker and their attachments (manifest ``variants``):
  ``hunt_dreck`` 拾银者·德雷克 Dreck the Silverpicker — ``SM_Mon_Deserter_Att_TagChain`` (a bandolier of silver
  name-tags cut off the dead, across the chest, ``dreck.tags``) + ``SM_Mon_Deserter_Att_LootSack`` (the loot sack at
  the front-left hip);
  ``hunt_harke`` 断誓者·哈克 Harke the Oathless — ``SM_Mon_Deserter_Att_RedScarf`` (``harke.scarf`` #A3221E, the only
  bright red on a deserter: his marker) + ``SM_Mon_Deserter_Att_Gorget`` (a captain's gorget, ``harke.gorget``).
* ``SK_Mon_DeserterDrummer`` + ``SM_MonWeapon_DrumStick`` ×2 (``monster_deserter_drummer``; sustain, speed 50 →
  1.5 m/s): the same gambeson, a dented kettle helm instead of the hood (the face shows: the one deserter who still
  keeps the beat) with a crimson rag tied round its foot (his marker beside the armband: the camera always sees
  it), a battered company drum on a belly harness slung from the right shoulder (pale head tipped to the camera,
  crimson hoops: the crusade's colours), sticks in both hands; ``Cast01`` = drumming (a 1 s loop, four beats,
  ``FX_DrumBeat``; the W2 channel plays it 2 s).

Built with the Living kit (``living.py``, ``living_clips.py``) on ``SKEL_Human``.
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "npcs", HERE.parent / "march", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import anim, mesh as M, outline, rig as R, scene, shading  # noqa: E402
from kit.paths import scratch_root  # noqa: E402
from kit.palette import Palette  # noqa: E402
from kit.rig import Bind  # noqa: E402

import common as C  # noqa: E402
from common import BP, K, V  # noqa: E402
import folk as F  # noqa: E402
import hero_base as HB  # noqa: E402
import living as LV  # noqa: E402
from living_clips import LivingAnims  # noqa: E402

ASSET = "SK_Mon_Deserter"
TOKEN = "Mon_Deserter"
DRUMMER = "SK_Mon_DeserterDrummer"
DRUMMER_TOKEN = "Mon_DeserterDrummer"
HATCHET = "SM_MonWeapon_Hatchet"
STICK = "SM_MonWeapon_DrumStick"
STICK_L = "SM_MonWeapon_DrumStick_L"      # build-time copy for the left hand (QA, previews); ships as STICK twice
BANDOLIER = "SM_Mon_Deserter_Att_TagChain"
LOOTSACK = "SM_Mon_Deserter_Att_LootSack"
RED_SCARF = "SM_Mon_Deserter_Att_RedScarf"
GORGET = "SM_Mon_Deserter_Att_Gorget"
HEIGHT = 1.73
DRUMMER_HEIGHT = 1.72
HUNT_SCALE = 1.25
MARKER = ("deserter.marker", "harke.marker")

ALIAS = dict(
    skin="living.skin_b", skin_shade="living.skin_shade", mouth="living.mouth", eye="living.eye_dark",
    brow="living.hair", hair="living.hair", hair_line="unburied.leather_dark", stubble="living.skin_shade",
    top="deserter.gambeson", top_shade="deserter.gambeson_line", sleeve="deserter.gambeson",
    trim="deserter.gambeson_line", quilt="deserter.gambeson_line", hem="deserter.gambeson_line",
    hood="deserter.hood", hood_line="deserter.gambeson_line", hood_edge="unburied.gambeson",
    legs="deserter.hood", boots="deserter.leather", boot_line="unburied.leather_dark",
    belt="deserter.leather", leather="deserter.leather", leather_dark="unburied.leather_dark",
    gold="unburied.helm_dent", metal_dark="unburied.helm_dent",
    pack="deserter.pack", pack_line="deserter.gambeson_line", bedroll="unburied.gambeson",
    insignia="deserter.insignia_scar", marker="deserter.marker",
    helm="unburied.helm_dent", helm_dent="unburied.mail_line",
    drum="drum.shell", drum_head="drum.head", drum_hoop="drum.hoop", drum_cord="drum.cord",
)
BODY = F.Build(thigh=12.5, shin=12, upper_arm=9.5, fore_arm=9, torso=17.5, neck=7.0, ankle=2.2, sh=5.3, hip=2.3,
               girth=0.94, belly=0.0, height=HEIGHT)
# art review r1 (re-check of item 1): head_k .56 → .53 (head share 22.7 → ~21.5 %, the brief's 20-22 %), the coat cut
# higher and narrower (hem_k .34 → .40, hem 5.1 × 4.7 → 4.8 × 4.5: more thigh, less bell) and a smaller pack (.95 → .80:
# the lump over the right shoulder in the se view) — the Living stand beside the lanky footman, not as toy soldiers
LOOK = LV.LivingLook(
    skin="#9A7460", body=BODY, height=HEIGHT, head_k=0.53, top=("#4E5040", "coat", None), palette=ALIAS,
    hood=True, hood_brim=2.75, hood_side=62.0, hood_peak=1.2, hood_tail=4.5, cowl_folds=True, hood_seam=True, face_scarf=True,
    sleeve="full", bracer=True, limb_r=0.72, hip_w=5.0, hem_k=0.40, hem_w=(4.8, 4.5), hem_dark=0.12, rag_hem=0.035, quilt=True, insignia=True, armband="r",
    belt_rag=True, pack="#5A4E3C", pack_scale=0.80, brow_scowl=1.3,
)
DRUMMER_BODY = F.Build(thigh=12.0, shin=11.5, upper_arm=9.5, fore_arm=9, torso=17.5, neck=7.0, ankle=2.2, sh=5.7,
                       hip=2.5, girth=1.06, belly=0.6, height=DRUMMER_HEIGHT)
# (r1 re-check: head_k .57 → .54 under the kettle helm, the coat .34 → .38 / 5.4 × 5.0 → 5.1 × 4.7, as the deserter)
DRUMMER_LOOK = LV.LivingLook(
    skin="#9A7460", body=DRUMMER_BODY, height=DRUMMER_HEIGHT, head_k=0.54, top=("#4E5040", "coat", None),
    palette=ALIAS, hair=("#2E2620", "short"), beard=("#2E2620", "stubble"), hood=False, face_scarf=False,
    sleeve="full", bracer=True, limb_r=0.74, hem_k=0.38, hem_w=(5.1, 4.7), hip_w=5.2, hem_dark=0.12, rag_hem=0.035, quilt=True,
    insignia=True, armband="r", belt_rag=False, brow_scowl=1.0, item="none",
)

DISPLAY = {"zh-CN": "逃兵", "en": "Deserter"}
DRUMMER_DISPLAY = {"zh-CN": "军鼓手", "en": "Deserter Drummer"}


# ── the deserters' folk: + kettle helm, drum, drum strap ──────────────────────────────────────────────────
class DeserterFolk(LV.LivingFolk):
    def __init__(self, look, asset_name, helm: bool = False, drum: bool = False):
        self.helm = helm
        self.drum = drum
        super().__init__(look, asset_name)

    def build(self, pal, rig, lod=0, lod_ratio=None, extra=None):
        def more(f, b, ids):
            lk = f.look
            if lk.beard:
                f.add_beard(b, ids["skull"])
            if f.helm:
                f.add_helm(b, ids["skull"])
            if f.drum:
                f.add_drum(b, ids)
            if extra is not None:
                extra(f, b, ids)
        return super().build(pal, rig, lod=lod, lod_ratio=lod_ratio, extra=more)

    def add_hair(self, b: M.Builder, sid: int) -> None:
        """Under the helm only a band of hair shows below the brim: the folk cap is built at its LOD1 density
        (22 × 4 instead of 34 × 6, −230 triangles; the strands are skipped under the helm)."""
        if not (self.helm and not getattr(self, "lod", 0)):
            return super().add_hair(b, sid)
        self.lod = 1
        try:
            super().add_hair(b, sid)
        finally:
            self.lod = 0

    # kettle helm (the crusade's issue: a round dome, a broad brim tipped down) — dented, dull iron
    def add_helm(self, b: M.Builder, sid: int) -> int:
        HK = self.HK
        lod = getattr(self, "lod", 0)
        hc = self.hc
        rx, ry, cy, _ = self.head.at(self.hz(2.0))
        r0 = rx + (self.hair_thick + 0.30) * HK
        sq = ry / rx
        z_brim = self.hz(2.7)
        top = self.hz(F.HEAD_TOP) + (self.hair_thick + 0.75) * HK
        # a chapel-de-fer: a tall dome rising to a blunt point (a ridge front to back), a sloping brim
        prof = [(r0 * 1.22, z_brim - 0.018), (r0 * 1.25, z_brim - 0.012), (r0 * 1.21, z_brim - 0.005),
                (r0 * 1.03, z_brim + 0.008), (r0 * 1.01, z_brim + 0.030), (r0 * 0.96, lerp(z_brim, top, 0.45)),
                (r0 * 0.74, lerp(z_brim, top, 0.76)), (r0 * 0.36, lerp(z_brim, top, 0.93)), (r0 * 0.05, top)]
        helm = M.lathe(prof, sides=16 if not lod else 12, squash_y=sq, cap0="fan", cap1="fan")
        helm.translate((0.0, cy, 0.0))
        # the brim tipped down at the sides and back a little (dented), level in front so the face shows
        bm = helm.bm
        for v in bm.verts:
            d = v.co - Vector((0.0, cy, z_brim))
            rad = math.hypot(d.x, d.y / max(sq, 1e-3))
            if rad > r0 * 1.10:
                a = math.degrees(math.atan2(v.co.x, -(v.co.y - cy)))
                k = (rad - r0 * 1.10) / (r0 * 0.33)
                v.co.z -= k * (0.010 + 0.016 * LV.smooth01((LV.adist(a) - 50.0) / 80.0))
        hid = self._add_exact(b, helm, "helm", Bind.rigid("head"), name="helm", hull_pad=True)
        # the family marker: a strip of the crusade's crimson tabard tied round the dome's foot (the one place a
        # top-down camera always sees: the armband hides behind the drum arm), knotted at the back left
        band = M.lathe([(r0 * 1.05, z_brim + 0.007), (r0 * 1.055, z_brim + 0.019), (r0 * 1.04, z_brim + 0.032)],
                       sides=16 if not lod else 12, squash_y=sq, cap0=None, cap1=None).translate((0.0, cy, 0.0))
        b.add(band, "marker", Bind.rigid("head"), name="helm_rag", outline=False, covered_by=hid)
        ka = math.radians(150.0)
        kc = Vector((r0 * 1.06 * math.sin(ka), cy - r0 * 1.06 * sq * math.cos(ka), z_brim + 0.020))
        knot = M.ellipsoid((0.014, 0.011, 0.013), tuple(kc), sides=8, rings=4)
        b.add(knot, "marker", Bind.rigid("head"), name="helm_rag_knot", outline=False, covered_by=hid)
        ridge = [Vector((0.0, cy + sq * r0 * 0.98 * math.cos(math.radians(t)), z_brim + 0.03 + (top - z_brim - 0.03)
                         * math.sin(math.radians(t)) ** 1.6)) for t in np.linspace(12.0, 168.0, 9)]
        ridge = [p + Vector((0.0, 0.0, 0.004)) for p in ridge]
        b.add(M.sweep(ridge, M.superellipse(1, 1, 4, 2.0), scales=[(0.0045, 0.006)] * len(ridge), up=(1, 0, 0),
                      cap0="ngon", cap1="ngon"), "helm_dent", Bind.rigid("head"), name="helm_ridge", outline=False,
              covered_by=hid)
        dent = M.ellipsoid((0.028, 0.006, 0.020), (r0 * 0.62, cy - r0 * 0.55 * sq, lerp(z_brim, top, 0.62)),
                           sides=8, rings=4)
        b.add(dent, "helm_dent", Bind.rigid("head"), name="helm_dent", outline=False, covered_by=hid)
        return hid

    # the company drum carried on a belly harness (just left of centre, forward of the skirt), strap from the right
    # shoulder, hook at the belt
    DRUM_R = 0.135
    DRUM_D = 0.17
    DRUM_TILT = 26.0           # deg the head is tipped forward (toward the camera: the pale head reads)

    def drum_frame(self) -> Matrix:
        """Drum frame (rest body space): origin at the centre of the top head, +Z its axis (up, tipped toward the
        front-right), forward of the belly."""
        D = self.D
        c = Vector((0.06, -0.345, D.hip_z + 0.17))
        ax = Vector((0.0, 0.0, 1.0))
        ax = (Matrix.Rotation(math.radians(-self.DRUM_TILT), 3, "X") @ ax)          # top toward −Y (forward)
        ax = (Matrix.Rotation(math.radians(-12.0), 3, "Z") @ ax).normalized()        # … and toward the right
        return R.frame(c, ax, (0, -1, 0)) @ Matrix.Rotation(math.radians(-90.0), 4, "X")

    def drum_head_point(self) -> Vector:
        return self.drum_frame() @ Vector((0.0, 0.0, 0.0))

    def drum_axis(self) -> Vector:
        return (self.drum_frame().to_3x3() @ Vector((0.0, 0.0, 1.0))).normalized()

    def add_drum(self, b: M.Builder, ids: dict) -> None:
        lod = getattr(self, "lod", 0)
        fr = self.drum_frame()
        r, d = self.DRUM_R, self.DRUM_D
        sides = 14 if not lod else 10
        bind = Bind.blend("pelvis", "spine_01", falloff=8.0)
        shell = M.lathe([(r, -d), (r * 1.01, -d * 0.5), (r, 0.0)], sides=sides, cap0="fan", cap1="fan")
        sid = b.add(shell.transform(fr), "drum", bind, name="drum", hull_pad=True)
        head = M.cylinder(r * 0.94, r * 0.94, -0.002, 0.004, sides=sides)
        b.add(head.transform(fr), "drum_head", bind, name="drum_head", outline=False, covered_by=sid)
        for z in (0.0, -d):
            # hoops: closed rings (a flat band round the rim), inked by the shell's padded hull (an open ring's own
            # hull came out thin at 1080p; a capped lathe covered the drum head)
            ring = [Vector((math.cos(2 * math.pi * k / sides) * r * 1.035, math.sin(2 * math.pi * k / sides) * r * 1.035,
                            z - 0.002)) for k in range(sides)]
            hoop = M.sweep(ring, M.rect_profile(0.012, 0.026), closed_path=True, up=(0, 0, 1), cap0=None, cap1=None)
            b.add(hoop.transform(fr), "drum_hoop", bind, name="drum_hoop", outline=False, covered_by=sid)
        # zigzag tension cords between the hoops
        n = 5
        for i in range(n * 2):
            a0 = 2 * math.pi * i / (n * 2)
            a1 = 2 * math.pi * (i + 1) / (n * 2)
            z0, z1 = (-0.015, -d + 0.015) if i % 2 == 0 else (-d + 0.015, -0.015)
            p0 = Vector((math.cos(a0) * r * 1.03, math.sin(a0) * r * 1.03, z0))
            p1 = Vector((math.cos(a1) * r * 1.03, math.sin(a1) * r * 1.03, z1))
            cord = M.tube([tuple(fr @ p0), tuple(fr @ p1)], [0.0035, 0.0035], sides=4, dome0=0.0, dome1=0.0)
            b.add(cord, "drum_cord", bind, name="drum_cord", outline=False, covered_by=sid)
        # dents in the shell (battered)
        for a, z in ((0.6, -0.08), (4.1, -0.06)):
            p = Vector((math.cos(a) * r * 1.0, math.sin(a) * r * 1.0, z))
            dent = M.ellipsoid((0.018, 0.018, 0.005), (0, 0, 0), sides=6, rings=4)
            dent.transform(fr @ R.frame(p, Vector((math.cos(a), math.sin(a), 0.0)), (0, 0, 1)))
            b.add(dent, "drum_cord", bind, name="drum_dent", outline=False, covered_by=sid)
        # the strap: the drum's hook up across the chest over the right shoulder, down the back to the belt
        tp, L, U = self.torso, self.L, self.U
        pts = [tp.point(self.th(L * 0.30), 60.0, 0.30 * U), tp.point(self.th(L * 0.55), 25.0, 0.42 * U),
               tp.point(self.th(L * 0.78), -12.0, 0.45 * U), tp.point(self.th(L - 1.6), -42.0, 0.45 * U),
               tp.point(self.th(L + 0.2), -80.0, 0.42 * U), tp.point(self.th(L - 2.5), -130.0, 0.42 * U),
               tp.point(self.th(L * 0.55), -165.0, 0.42 * U), tp.point(self.th(L * 0.30), 150.0, 0.38 * U)]
        path = HB.catmull(pts, 3)
        ups = [tuple(tp.normal(max(min(p.z, tp.z1), tp.z0), math.degrees(math.atan2(p.x, -p.y)))) for p in path]
        strap = M.sweep(path, M.rect_profile(0.006, 1.6 * U), up=ups[len(ups) // 2], cap0="ngon", cap1="ngon")
        b.add(strap, "leather", Bind.blend("spine_01", "spine_02", "spine_03", "clavicle_r", falloff=4.0),
              name="drum_strap", hull_pad=True)
        hook = M.tube([tuple(fr @ Vector((r * 0.9, 0.0, 0.01))), tuple(pts[0])], [0.006, 0.006], sides=5,
                      dome0=0.0, dome1=0.0)
        b.add(hook, "leather", bind, name="drum_hook", hull_pad=True)


def lerp(a, b, t):
    return a + (b - a) * t


# ── held items ─────────────────────────────────────────────────────────────────────────────────────────
ITEM_ALIAS = dict(haft="unburied.wood", haft_dark="unburied.leather_dark", steel="unburied.steel",
                  edge="unburied.steel_edge", grip="deserter.leather", iron="unburied.helm_dent",
                  stick="drum.shell", knob="drum.cord")


def _finish_item(b: LV.MarchBuilder, pal: Palette, name: str) -> bpy.types.Object:
    obj = b.build(name)
    mt = shading.toon_material(pal)
    outline.bake_hull(obj, outline.WIDTH_CM[LV.OUTLINE_CLASS] / 100.0, mt, shading.outline_material(pal))
    for m in outline.hull_color_report(obj, pal, LV.OUTLINE_CLASS):
        print(f"[ink] WARNING {name}: {m}")
    obj["af_outline_class"] = LV.OUTLINE_CLASS
    return obj


HATCHET_TOP = 0.42


def build_hatchet(pal: Palette) -> bpy.types.Object:
    """A woodsman's hatchet carried off by a deserter: a 50 cm haft (leather grip, a split near the head bound with
    cord), a bearded iron head, its edge notched. Grip at the origin, haft along +Y, the edge toward +Z (the swing
    direction: ``C.blade`` up = where the tip goes)."""
    b = LV.MarchBuilder(HATCHET, pal, ITEM_ALIAS)
    b.declare()
    rig = Bind.rigid("root")
    haft = M.tube([(0, -0.13, 0), (0, 0.0, 0.002), (0, 0.22, 0.006), (0, HATCHET_TOP + 0.03, 0.0)],
                  [0.0155, 0.0145, 0.0135, 0.014], sides=8, dome0=0.006, dome1=0.004)
    hid = b.add(haft, "haft", rig, name="haft", hull_pad=True)
    for k in range(5):
        y = -0.06 + k * 0.024
        ring = M.tube([(0, y - 0.009, 0), (0, y + 0.009, 0)], [0.0168] * 2, sides=8, dome0=0.0, dome1=0.0)
        b.add(ring, "grip" if k % 2 == 0 else "haft_dark", rig, name="grip", outline=False, covered_by=hid)
    # bearded head: eye round the haft, the cheek widening toward the edge, the beard hooked down (toward −Y)
    y0 = HATCHET_TOP - 0.02
    eye = M.box((0.028, 0.056, 0.036), (0.0, y0, 0.0), bevel=0.004)
    b.add(eye, "iron", rig, name="eye", hull_pad=True)
    outline2d = [(-0.022, 0.012), (0.022, 0.012), (0.030, 0.045), (0.050, 0.085), (0.064, 0.122), (0.038, 0.130),
                 (-0.012, 0.130), (-0.064, 0.126), (-0.084, 0.106), (-0.060, 0.088), (-0.030, 0.045)]
    # (x = along the haft (+Y), z = out from it (+Z)): the edge at z ≈ 0.10, the beard hooking down toward −Y
    bm = bmesh.new()
    th = 0.0075
    top, bot = [], []
    for u, w in outline2d:
        tz = th * (1.0 - 0.75 * min(1.0, max(0.0, (w - 0.09) / 0.04)))
        top.append(bm.verts.new((tz, y0 + u, w)))
        bot.append(bm.verts.new((-tz, y0 + u, w)))
    n = len(outline2d)
    bm.faces.new(top)
    bm.faces.new(list(reversed(bot)))
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((top[j], top[i], bot[i], bot[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    cheek = M.Part(bm).flat()
    cid = b.add(cheek, "steel", rig, name="cheek", hull_pad=True)
    # the bright edge along the bit, with two notches
    edge = []
    for i in range(9):
        t = i / 8
        u = lerp(-0.076, 0.058, t)
        w = 0.126 + 0.004 * math.sin(math.pi * t) - (0.007 if i in (3, 6) else 0.0)
        edge.append((0.0, y0 + u, w))
    eg = M.sweep(edge, M.superellipse(1, 1, 4, 2.0), scales=[(0.0035, 0.004)] * len(edge), up=(1, 0, 0),
                 cap0="ngon", cap1="ngon")
    b.add(eg, "edge", rig, name="bit", outline=False, covered_by=cid)
    # cord binding below the head (the haft split)
    for k in range(3):
        y = y0 - 0.045 - k * 0.012
        ring = M.tube([(0, y - 0.004, 0), (0, y + 0.004, 0)], [0.0162] * 2, sides=8, dome0=0.0, dome1=0.0)
        b.add(ring, "haft_dark", rig, name="binding", outline=False, covered_by=hid)
    return _finish_item(b, pal, HATCHET)


def hatchet_grip(co) -> bool:
    return -0.075 < co.y < 0.06 and math.hypot(co.x, co.z) < 0.03


STICK_LEN = 0.30


def build_stick(pal: Palette) -> bpy.types.Object:
    """A drumstick (35 cm, a round knob): grip at the origin (4.5 cm from the butt), +Y to the tip."""
    b = LV.MarchBuilder(STICK, pal, ITEM_ALIAS)
    b.declare()
    rig = Bind.rigid("root")
    st = M.tube([(0, -0.045, 0), (0, 0.10, 0), (0, STICK_LEN - 0.02, 0)], [0.0115, 0.0105, 0.0075], sides=8,
                dome0=0.005, dome1=0.0)
    sid = b.add(st, "stick", rig, name="stick", hull_pad=True)
    knob = M.ellipsoid((0.0125, 0.016, 0.0125), (0, STICK_LEN - 0.006, 0), sides=8, rings=6)
    b.add(knob, "knob", rig, name="knob", hull_pad=True)
    del sid
    return _finish_item(b, pal, STICK)


def stick_grip(co) -> bool:
    return -0.05 < co.y < 0.05


# ── hunt attachments (bone-local; manifest variants) ─────────────────────────────────────────────────────
ATT_ALIAS = dict(tags="dreck.tags", chain="dreck.chain", sack="dreck.sack", rope="unburied.rope",
                 sack_line="deserter.gambeson_line", scarf="harke.scarf", scarf_line="unburied.tabard",
                 gorget="harke.gorget", gorget_dark="unburied.helm_dent", leather="deserter.leather")


def _finish_att(b, pal, name, rig, bone) -> bpy.types.Object:
    obj = b.build(name)
    inv = rig.obj.data.bones[bone].matrix_local.inverted()
    obj.data.transform(inv)
    obj.data.update()
    mt = shading.toon_material(pal)
    outline.bake_hull(obj, outline.WIDTH_CM[LV.OUTLINE_CLASS] / 100.0, mt, shading.outline_material(pal))
    for m in outline.hull_color_report(obj, pal, LV.OUTLINE_CLASS):
        print(f"[ink] WARNING {name}: {m}")
    obj["af_outline_class"] = LV.OUTLINE_CLASS
    return obj


def _chest_pt(folk, h: float, a: float, out: float) -> Vector:
    """A point ``out`` metres proud of the outermost cloth at torso height ``h`` (folk units above the pelvis), angle
    ``a``: the capelet where it covers the torso, else the gambeson torso."""
    cp, tp = folk.capelet_profile(), folk.torso
    z = folk.th(h)
    best = tp.point(z, a, out + 0.014)            # over the pack's shoulder straps (1.2 cm + 6 mm on the torso)
    if cp.z0 <= z <= cp.z1:
        q = cp.point(z, a, out + 0.003)           # over the capelet's shell (its outer face 5.5 mm off the profile)
        cy = tp.at(min(max(z, tp.z0), tp.z1))[2]
        if math.hypot(q.x, q.y - cy) > math.hypot(best.x, best.y - cy):
            best = q
    return best


# art review r1 (item 5): the r0 necklace of tags read as a 1-2 px grey arc under the chin and the loot sack hung only
# on the back of the pack: at the game camera Dreck was a 1.25x deserter with a crown. The tags now ride a diagonal
# bandolier across the chest (bigger, tilted up to the key light) and the sack hangs at the front-left hip.
BANDOLIER_PATH = ((58.0, -1.0), (42.0, -2.4), (24.0, -4.0), (4.0, -5.6), (-12.0, -7.0), (-22.0, -8.0),
                  (-29.0, -8.7))           # (angle, folk height below the neck base): left shoulder → right ribs
TAG_SIZE = (0.043, 0.0045, 0.055)        # width, thickness, height (m) — r1 re-check: 3.6 × 4.6 cm read as 2-3 px grey
TAG_COUNT = 9                            # specks under the capelet in the se view; nine larger tags (5-7 px at x1.25)


def _outer_surface(folk, body, parts=("torso", "capelet", "cowl", "pack_strap", "quilt", "insignia", "strap_buckle")):
    """f(h, a) → (point on the outermost cloth along the ray from the torso axis at folk height ``h``, angle ``a``,
    the ray's direction, its origin on the axis): the rest mesh's toon faces of ``parts`` (the pack's shoulder straps stand up to 3 cm off
    the gambeson where they turn round the chest)."""
    import bmesh
    import json
    from mathutils.bvhtree import BVHTree
    names = json.loads(body["af_parts"])
    bm = bmesh.new()
    bm.from_mesh(body.data)
    lay = bm.faces.layers.int.get("af_part")
    drop = [f for f in bm.faces if f.material_index == 1 or not names[f[lay]].startswith(parts)]
    bmesh.ops.delete(bm, geom=drop, context="FACES")
    tree = BVHTree.FromBMesh(bm)

    def f(h, a):
        z = folk.th(h)
        cy = folk.torso.at(min(max(z, folk.torso.z0), folk.torso.z1))[2]
        o = Vector((0.0, cy, z))
        d = Vector((math.sin(math.radians(a)), -math.cos(math.radians(a)), 0.0))
        r, start = 0.0, o.copy()
        for _ in range(8):
            hit = tree.ray_cast(start, d, 0.6)
            if hit[0] is None:
                break
            r = (hit[0] - o).dot(d)
            start = hit[0] + d * 1e-4
        return o + d * r, d, o
    return f


def build_tagchain(pal, folk, rig, body=None) -> bpy.types.Object:
    """Dreck's bandolier of silver name-tags cut off the dead (the Tag-Thief, S3): a leather strap from the left
    shoulder diagonally across the chest to the right ribs (over the capelet and the pack's shoulder straps), nine
    large tags riveted along it, tilted up toward the key light so they catch it — the brightest metal on any deserter
    (``dreck.tags``). Bone ``spine_03`` (the strap stays on the chest, where spine_03 carries the torso)."""
    b = LV.MarchBuilder(BANDOLIER, pal, ATT_ALIAS)
    b.declare()
    L, U = folk.L, folk.U
    bind = Bind.rigid("root")
    body = body if body is not None else bpy.data.objects[f"{folk.asset}_Mesh"]
    outer = _outer_surface(folk, body)
    # (angle, height) resampled along the diagonal; every sample just over the outermost cloth across the strap's
    # width (its own ray, and its neighbours' ±9° projected onto it)
    ah = HB.catmull([Vector((a, dh, 0.0)) for a, dh in BANDOLIER_PATH], 30)
    path, ups = [], []
    for q in ah:
        p0, d0, axis = outer(L + q.y, q.x)
        reach = (p0 - axis).dot(d0)
        for da in (-12.0, -6.0, 0.0, 6.0, 12.0):
            for dh in (-0.8, 0.0, 0.8):                 # ±2.4 cm: the strap's 4 cm width runs across the diagonal
                if da or dh:
                    pt, _, _ = outer(L + q.y + dh, q.x + da)
                    reach = max(reach, (pt - axis).dot(d0))
        path.append(axis + d0 * (reach + 0.016))
        ups.append(d0)
    # per-sample up = the cloth's normal: the strap lies flat on the chest (one up vector stood its edges into it)
    strap = M.sweep(path, M.rect_profile(0.007, 0.040), up=[tuple(u) for u in ups], cap0="ngon", cap1="ngon")
    sid = b.add(strap, "leather", bind, name="bandolier", hull_pad=True)
    rng = scene.rng(BANDOLIER, "tags")
    cum = [0.0]
    for p, q in zip(path[:-1], path[1:]):
        cum.append(cum[-1] + (q - p).length)
    tw, tt, th = TAG_SIZE
    k = 0
    s_pos = np.linspace(cum[-1] * 0.16, cum[-1] * 0.95, TAG_COUNT)
    for sp in s_pos:
        i = max(0, min(len(path) - 2, int(np.searchsorted(cum, sp)) - 1))
        f = (sp - cum[i]) / max(1e-6, cum[i + 1] - cum[i])
        c = path[i].lerp(path[i + 1], f)
        n = ups[i].lerp(ups[i + 1], f).normalized()
        along = (path[i + 1] - path[i]).normalized()
        # riveted flat on the strap (alternately a little above / below its line, bigger than the strap is wide),
        # their faces turned up toward the camera and the key light so the metal catches it
        down = (Vector((0, 0, -1)) - n * Vector((0, 0, -1)).dot(n)).normalized()
        face = (n + Vector((0.0, 0.0, 0.45))).normalized()
        cc = c + down * (0.006 if k % 2 else -0.004) + n * (0.0045 + tt)
        tag = M.box((tw, tt, th), (0, 0, 0), bevel=0.002)
        fr = R.frame(cc, face, (0, 0, 1)) @ Matrix.Rotation(math.radians(rng.uniform(-12, 12)), 4, "Y")
        # no ink of its own (a 3 cm tag under a 2.9 cm game-camera ink turned into a dark bump): the strap's padded
        # hull encloses the tags, so the bright metal reads against the dark leather inside one outline
        b.add(tag.transform(fr), "tags", bind, name="tag", outline=False, covered_by=sid)
        rivet = M.ellipsoid((0.0045, 0.003, 0.0045), tuple(c + n * 0.0045), sides=6, rings=4)
        b.add(rivet, "chain", bind, name="link", outline=False, covered_by=sid)
        del along
        k += 1
    return _finish_att(b, pal, BANDOLIER, rig, "spine_03")


SACK_A = 34.0                              # the hip sack's angle on the belt: front-left, inside the free hand's swing
SACK_BONE = "coat_th_l_01"                 # the left coat driver: the sack swings forward with the coat before the
                                           # thigh (the driver copies only the forward swing), never into it


def build_lootsack(pal, folk, rig) -> bpy.types.Object:
    """Dreck's loot: a bulging burlap sack tied to the belt at the front-left hip, hanging high on the coat (bone
    ``coat_th_l_01``, the coat's left thigh driver: it swings forward with the cloth before the thigh): the camera's
    se and front views see it; the free hand swings outside it, the hatchet hand on the other side."""
    b = LV.MarchBuilder(LOOTSACK, pal, ATT_ALIAS)
    b.declare()
    bp = folk.belt_profile()
    z_belt = folk.th(1.1)
    n = bp.normal(z_belt, SACK_A)
    n = Vector((n.x, n.y, 0.0)).normalized()
    top = bp.point(z_belt, SACK_A, 0.010)
    # just under the hip joint (the coat driver's hinge: above it a forward swing would turn the sack into the
    # belly), clear of the coat's flare by 3 cm over its whole height
    zc = folk.D.hip_z - 0.085
    cy = folk.torso.at(zc)[2]
    reach = 0.0
    for z in np.linspace(zc - 0.08, zc + 0.06, 8):            # the sack's height range
        k = (folk.skirt_z0 - z) / (folk.skirt_z0 - folk.skirt_z1)
        q = folk.skirt_point(min(1.0, max(0.0, k)), SACK_A, folk.SKIRT_THICK) if k > 0.0 else bp.point(z, SACK_A, 0.0)
        reach = max(reach, (Vector((q.x, q.y - cy, 0.0))).dot(n))
    # (r1 re-check: 6 → 9 cm off the coat — the shorter, narrower coat's front panel swings into the sack in Walk)
    c = Vector((0.0, cy, zc)) + n * (reach + 0.090 + 0.072)
    bind = Bind.rigid("root")
    rng = scene.rng(LOOTSACK, "lumps")
    tang = Vector((0, 0, 1)).cross(n).normalized()

    def disp(z, a):
        return 0.005 * math.sin(math.radians(a) * 3.0 + z * 40.0) + 0.003 * rng.uniform(-1, 1)
    rings = [(c.z - 0.078, 0.064, 0.042), (c.z - 0.052, 0.084, 0.056), (c.z - 0.005, 0.090, 0.060),
             (c.z + 0.040, 0.076, 0.050), (c.z + 0.066, 0.040, 0.028), (c.z + 0.078, 0.020, 0.016)]
    prof = C.Profile([(z, rx, ry, 0.0, 0.0) for z, rx, ry in rings], exp=2.0)
    sack = HB.profile_mesh(prof, list(np.linspace(prof.z0, prof.z1, 9)), 14, disp=disp, cap0="fan", cap1="fan",
                           dome0=0.01)
    yaw = math.atan2(n.x, -n.y)
    place = Matrix.Translation((c.x, c.y, 0.0)) @ Matrix.Rotation(-yaw, 4, "Z")
    sid = b.add(sack.transform(place), "sack", bind, name="sack", hull_pad=True)
    tie = M.tube([(c.x, c.y, c.z + 0.058), (c.x, c.y, c.z + 0.072)], [0.024, 0.022], sides=10, dome0=0.0, dome1=0.0)
    b.add(tie, "rope", bind, name="sack_tie", outline=False, covered_by=sid)
    # its neck tied off in a twist of rope (no cord up to the belt: above the hinge it would swing into the belt)
    knot = M.ellipsoid((0.020, 0.016, 0.014), (c.x, c.y, c.z + 0.084), sides=8, rings=4)
    b.add(knot, "rope", bind, name="lash", hull_pad=True)
    patch = M.box((0.045, 0.004, 0.04), (0.0, 0.0, 0.0), bevel=0.001)
    pc = c + n * 0.056 + tang * 0.02 + Vector((0, 0, -0.02))
    b.add(patch.transform(R.frame(pc, n, (0, 0, 1))), "sack_line", bind, name="patch", outline=False, covered_by=sid)
    return _finish_att(b, pal, LOOTSACK, rig, SACK_BONE)


def build_red_scarf(pal, folk, rig) -> bpy.types.Object:
    """Harke's red scarf (the plains hunt's red cap re-made): wound over his face scarf from the nose bridge to
    under the chin across the hood's opening, knotted at the right cheek, one tail on the capelet — the only bright
    red on a deserter (``harke.scarf`` #A3221E: his family marker). Bone ``head``."""
    b = LV.MarchBuilder(RED_SCARF, pal, ATT_ALIAS)
    b.declare()
    bind = Bind.rigid("root")
    HK = folk.HK
    rows, cols = 3, 16
    outer, inner = [], []
    for r in range(rows + 1):
        s = r / rows
        ro, ri = [], []
        for c in range(cols + 1):
            a = lerp(-54.0, 54.0, c / cols)
            d = LV.adist(a)
            top = -1.25 - 0.6 * LV.smooth01((d - 25.0) / 30.0)
            h = lerp(top, -6.2, s)
            nose = 0.9 * math.exp(-(a / 16.0) ** 2) * math.exp(-((h + 1.6) / 1.4) ** 2)
            lift = 1.05 + nose + 0.25 * s                # over the hood's rolled edge (r1)
            ro.append(folk.hp(h, a, lift))
            ri.append(folk.hp(h, a, lift - 0.36))
        outer.append(ro)
        inner.append(ri)
    wrap = C.thick_patch(outer, inner)
    wid = b.add(wrap, "scarf", bind, name="red_scarf", hull_pad=True)
    for h in (-3.3, -5.3):
        pts = [folk.hp(h - 0.3 * (a / 55.0) ** 2, a, 1.33 + 0.9 * math.exp(-(a / 16.0) ** 2) *
                       math.exp(-((h + 1.6) / 1.4) ** 2)) for a in np.linspace(-50.0, 50.0, 7)]
        fold = M.sweep(pts, M.superellipse(1, 1, 4, 2.0), scales=[(0.07 * HK, 0.18 * HK)] * len(pts),
                       up=tuple(folk.hn(h, 0.0)), dome0=0.0, dome1=0.0)
        b.add(fold, "scarf_line", bind, name="scarf_fold", outline=False, covered_by=wid)
    knot_c = folk.hp(-5.0, -40.0, 1.65)
    knot = M.ellipsoid((0.020, 0.017, 0.019), tuple(knot_c), sides=8, rings=6)
    b.add(knot, "scarf", bind, name="scarf_knot", hull_pad=True)
    out = folk.hn(-5.0, -40.0)
    p0 = knot_c + Vector((0, 0, -0.012)) + out * 0.004
    pts = [p0, p0 + Vector((-0.010, -0.012, -0.030)), p0 + Vector((-0.018, -0.020, -0.058))]
    tail = M.sweep(pts, M.rect_profile(0.005, 0.030), scales=[(1.0, 1.0), (1.0, 0.9), (1.0, 0.65)],
                   up=tuple(out), cap0="ngon", cap1="ngon")
    b.add(tail, "scarf", bind, name="scarf_tail", hull_pad=True)
    return _finish_att(b, pal, RED_SCARF, rig, "head")


def build_gorget(pal, folk, rig) -> bpy.types.Object:
    """Harke's captain's gorget: a brass-trimmed crescent plate on the upper chest, over the capelet (bone
    ``spine_03``)."""
    b = LV.MarchBuilder(GORGET, pal, ATT_ALIAS)
    b.declare()
    bind = Bind.rigid("root")
    cp = folk.capelet_profile()
    L = folk.L
    outer, inner = [], []
    rows, cols = 3, 14
    # art review r1 (item 6): the r0 plate (L − 0.4, 3.6 u deep) sat under the red scarf in every view; it now hangs
    # lower and larger — a brass crescent below the scarf at the game camera
    A = 50.0
    top_h = L - 1.5

    def h_of(a, s):
        w = 1.0 - (abs(a) / A) ** 2
        return lerp(top_h, top_h - (4.0 * w + 0.7), s)
    for r in range(rows + 1):
        s = r / rows
        outer.append([cp.point(folk.th(h_of(lerp(-A, A, c / cols), s)), lerp(-A, A, c / cols), 0.016)
                      for c in range(cols + 1)])
        inner.append([cp.point(folk.th(h_of(lerp(-A, A, c / cols), s)), lerp(-A, A, c / cols), 0.009)
                      for c in range(cols + 1)])
    plate = C.thick_patch(outer, inner)
    pid = b.add(plate, "gorget", bind, name="gorget", hull_pad=True)
    rim = [cp.point(folk.th(h_of(a, 1.0) + 0.15), a, 0.0175) for a in np.linspace(-A + 2.0, A - 2.0, 13)]
    b.add(M.sweep(rim, M.superellipse(1, 1, 4, 2.0), scales=[(0.0025, 0.0045)] * len(rim), up=(0, -1, 0),
                  cap0="ngon", cap1="ngon"), "gorget_dark", bind, name="gorget_rim", outline=False, covered_by=pid)
    boss = M.ellipsoid((0.017, 0.006, 0.017), tuple(cp.point(folk.th(top_h - 2.0), 0.0, 0.021)), sides=8, rings=4)
    b.add(boss, "gorget_dark", bind, name="gorget_boss", outline=False, covered_by=pid)
    return _finish_att(b, pal, GORGET, rig, "spine_03")


VARIANTS = {
    "hunt_dreck": {
        "displayName": {"zh-CN": "拾银者·德雷克", "en": "Dreck the Silverpicker"}, "scale": HUNT_SCALE, "elite": True,
        "attachments": [{"object": BANDOLIER, "socket": "spine_03"}, {"object": LOOTSACK, "socket": SACK_BONE}],
        "marker": {"swatches": ["deserter.marker"]},
        "note": "quest S3 hunt (monsters.md 2.3; drops mat_tag_pouch): SK_Mon_Deserter x1.25 + a bandolier of silver "
                "name-tags across the chest (dreck.tags) + the loot sack at the front-left hip + the elite marker"},
    "hunt_harke": {
        "displayName": {"zh-CN": "断誓者·哈克", "en": "Harke the Oathless"}, "scale": HUNT_SCALE, "elite": True,
        "attachments": [{"object": RED_SCARF, "socket": "head"}, {"object": GORGET, "socket": "spine_03"}],
        "marker": {"swatches": ["harke.marker"]},
        "note": "quest S4 q_rf_oathless hunt: SK_Mon_Deserter x1.25 + the red scarf (harke.scarf #A3221E, the only "
                "bright red on a deserter: his family marker) + a captain's gorget + the elite marker"},
}


# ── clips ──────────────────────────────────────────────────────────────────────────────────────────────
class DeserterAnims(LivingAnims):
    REF_SPEED = 56 * 0.03
    WALK_MS = 860.0
    WALK = dict(duty=0.60, lift=0.07, bob=0.016, lean=9.0, crouch=-0.04, stance=0.11, hip_pitch=3.0)
    IDLE_MS = 1300.0
    ATTACK_MS = 800.0
    CONTACT_MS = 450.0

    # art review r1 (item 8): the r0 READY held the hatchet across the belly end-on (a dark pistol-like lump in the
    # most-seen pose); it now hangs low at the right side, the head down and forward, its cheek turned to the camera
    READY_AXE = (150.0, -0.32, -62.0)          # C.blade (theta, lat, roll) of the hanging hatchet

    def make_ready(self) -> BP:
        """Wary skirmisher: crouched and hunched, the hood low, eyes up over the scarf; the hatchet hanging low at
        the right side (head down and forward, the axe head in profile to the W1 camera); the left hand open
        before the chest."""
        J = self.J
        th, lat, roll = self.READY_AXE
        return BP(
            pelvis=V(0, 0.02, -0.06), hip_pitch=5.0, hip_yaw=-10.0, lean=14.0, twist=8.0, side=0.0,
            head_pitch=-14.0, head_yaw=-6.0,
            foot_l=J["ankle_l"] + V(0.03, 0.10, 0), foot_r=J["ankle_r"] + V(-0.04, -0.09, 0),
            foot_l_rot=(0, 0, 14), foot_r_rot=(0, 0, 24), knee_l=V(0.3, 1, 0), knee_r=V(-0.35, 1, 0),
            hand_r=self.side_pt("r", 0.10, 0.10, -0.50), elbow_r=V(-0.7, 0.3, -0.4),
            **self.wkw(th, lat=lat, roll=roll),
            hand_l=self.side_pt("l", 0.0, 0.26, -0.30), elbow_l=V(0.45, -0.4, -0.7), wrist_l=(-10.0, 0.0, -35.0),
            flow=0.12)                         # (r1 re-check: the left elbow in at the side, not akimbo — a narrower top)

    def idle_layer(self, bp: BP, t: float, ph: float) -> BP:
        # shifting weight, the hanging hatchet swinging a little from the loose wrist
        th, lat, roll = self.READY_AXE
        return bp.but(**self.wkw(th + 5.0 * math.sin(ph), lat=lat, roll=roll), hip_yaw=bp.hip_yaw + 3.0 * math.sin(ph),
                      head_yaw=bp.head_yaw + 9.0 * math.sin(ph * 0.5))

    def carry(self, bp: BP, sw: float, ph: float) -> BP:
        R0 = self.ready()
        return bp.but(hand_r=self.side_pt("r", 0.13, 0.12 + 0.10 * sw, -0.50), elbow_r=V(-0.8, -0.2, -0.5),
                      **self.wkw(118.0 - 20.0 * sw, lat=-0.25),
                      hand_l=self.side_pt("l", 0.10, 0.10 - 0.14 * sw, -0.48 + 0.04 * max(0.0, -sw)),
                      elbow_l=V(0.7, -0.3, -0.6), wrist_l=R0.wrist_l, head_pitch=-12.0)

    def attack_keys(self) -> list:
        R0 = self.ready()
        J = self.J
        wind = R0.but(pelvis=V(0, -0.01, -0.05), lean=4.0, twist=-26.0, hip_yaw=-18.0, head_pitch=-16.0,
                      head_yaw=10.0, hand_r=self.side_pt("r", 0.22, -0.06, 0.28), elbow_r=V(-1.0, 0.2, 0.3),
                      **self.wkw(-40.0, lat=-0.3),
                      hand_l=self.side_pt("l", 0.18, 0.34, -0.10), elbow_l=V(0.8, -0.2, -0.5), clav_r=(14.0, -6.0),
                      flow=0.25)
        mid = R0.but(pelvis=V(0, 0.03, -0.06), lean=12.0, twist=-6.0, hip_yaw=-8.0, head_pitch=-18.0,
                     hand_r=self.side_pt("r", 0.38, 0.12, 0.18), elbow_r=V(-1.0, 0.1, 0.0), **self.wkw(35.0, lat=-0.35),
                     hand_l=self.side_pt("l", 0.12, 0.22, -0.28), clav_r=(12.0, 4.0), flow=0.35)
        hit = R0.but(pelvis=V(0, 0.07, -0.09), lean=22.0, hip_pitch=7.0, twist=16.0, hip_yaw=6.0, head_pitch=-26.0,
                     head_yaw=-8.0, foot_l=J["ankle_l"] + V(0.05, 0.18, 0), foot_r=J["ankle_r"] + V(-0.06, -0.11, 0),
                     hand_r=self.side_pt("r", 0.10, 0.50, -0.30), elbow_r=V(-1.0, -0.2, -0.3),
                     **self.wkw(110.0, lat=0.15),
                     hand_l=self.side_pt("l", 0.14, -0.02, -0.42), elbow_l=V(0.7, 0.3, -0.5), clav_r=(-4.0, 10.0),
                     flow=0.5)
        # the follow-through ends before the belly, not across it (Dreck's bandolier rides the lower chest)
        follow = hit.but(lean=24.0, twist=20.0, hand_r=self.side_pt("r", 0.02, 0.50, -0.46),
                         elbow_r=V(-1.0, -0.1, -0.2), **self.wkw(148.0, lat=0.24), flow=0.4)
        back = R0.but(lean=16.0, twist=6.0, hand_r=self.side_pt("r", 0.26, 0.32, -0.36), elbow_r=V(-1.0, -0.1, -0.3),
                      **self.wkw(70.0, lat=-0.35), flow=0.25)
        return [(250.0, wind, "out"), (370.0, mid, "in"), (450.0, hit, "linear"), (560.0, follow, "out"),
                (680.0, back, "smooth")]

    def death_items(self) -> dict:
        up = Vector((0.0, 1.0, 0.0))
        J = self.J
        # the knees buckle: the hatchet hand drops out to the side, the haft hanging (its butt clear of the forearm)
        return {"hit": self.wkw(140.0, lat=-0.45),
                "kneel": {**self.wkw(172.0, lat=-0.40), "hand_r": J["hip_r"] + V(-0.30, 0.20, 0.02),
                          "elbow_r": V(-0.9, -0.3, -0.1)},
                "topple": {"wpn": Vector((-0.75, 0.0, 0.66)), "wpn_up": up},
                "down": {"wpn": Vector((-0.80, 0.0, 0.60)), "wpn_up": up}}

    def stun_items(self, bp: BP, ph: float) -> dict:
        # dazed: the free hand drops to the side (off the chest: Harke's gorget), the hatchet hangs
        return {**self.wkw(150.0 + 6.0 * math.sin(ph), lat=-0.3),
                "hand_l": self.hang_hand("l", 0.10, 0.06, 0.84) + V(0, 0, 0.4 * self.U * math.sin(ph)),
                "elbow_l": V(0.8, -0.4, -0.4)}


class DrummerAnims(LivingAnims):
    """The drummer: both hands hold sticks (two hammer grips); the drum rides the belly harness."""
    REF_SPEED = 50 * 0.03
    WALK_MS = 960.0
    WALK = dict(duty=0.60, lift=0.065, bob=0.016, lean=5.0, crouch=-0.03, stance=0.11, hip_pitch=2.0)
    IDLE_MS = 1400.0
    ATTACK_MS = 850.0
    CONTACT_MS = 480.0
    CAST_MS = 1000.0
    GRIPS_LIVING = ("r", "l")
    BEATS = (("r", 125.0), ("l", 375.0), ("r", 625.0), ("l", 875.0))

    def __init__(self, rig, folk, items):
        super().__init__(rig, folk, items)
        self.head_pt_drum = folk.drum_head_point()
        self.drum_ax = folk.drum_axis()

    def pelvis_m(self, bp: BP):
        p = C.to_pose(self.rig, bp.but(hand_l=None, hand_r=None), None, grips=())
        m = anim.evaluate(p, return_matrices=True)["pelvis"]
        return m @ self.bones["pelvis"].matrix_local.inverted()

    def strike(self, side: str, up: float, bp: BP, out: float = 0.0, fwd: float = 0.0) -> dict:
        """Hand + stick so the stick's knob touches the drum head (``up`` = 0) or hangs ``up`` m above it — the drum
        carried by the posed pelvis."""
        sg = 1.0 if side == "l" else -1.0
        pm = self.pelvis_m(bp)
        ax = (pm.to_3x3() @ self.drum_ax).normalized()
        tip = pm @ self.head_pt_drum + ax * (0.018 + up) + Vector((sg * 0.035 + out, -0.01 - fwd, 0.0))
        # the stick slopes from the hand (above the drum, in front of the belly) forward-down onto the head, in
        # from the drummer's side
        d = (Vector((-sg * 0.40, -0.42, -0.80 + 0.30 * up / 0.15))).normalized()
        hand = tip - d * (STICK_LEN - 0.006)
        upv = d.cross(Vector((1, 0, 0))).normalized()
        if upv.length < 0.1:
            upv = Vector((0, 0, 1))
        key = "wpn" if side == "r" else "off"
        return {f"hand_{side}": hand, key: d, f"{key}_up": upv}

    # ── art review r1 (item 2): the r0 READY held both sticks over the drum, so Idle and the drumming (Cast01, the
    # W2 "kill the drummer first" tell) shared one silhouette and the drumming was wrist flicks. The drummer now
    # rests with the sticks lowered at his sides; Cast01 drums in big strokes (sticks to shoulder height, elbows
    # out, a knee / torso bob on every beat); Attack01 is a forward overhand blow that ends at the target ──
    def rest_hands(self, swing: float = 0.0, tap: float = 0.0) -> dict:
        """Hands hanging at the sides, the sticks lowered along the thighs, tips down and out (``swing`` +1 = the
        left hand forward, the walk's counter-swing; ``tap`` lifts the right stick's tip a little)."""
        return dict(hand_r=self.hang_hand("r", 0.09, 0.03 - 0.07 * swing, 0.86), elbow_r=V(-0.5, -0.6, -0.4),
                    **self.wkw(172.0 - 5.0 * swing - 6.0 * tap, lat=-0.44),
                    hand_l=self.hang_hand("l", 0.12, 0.03 + 0.07 * swing, 0.84), elbow_l=V(0.5, -0.6, -0.4),
                    **self.wkw(172.0 + 5.0 * swing, lat=0.44, key="off"))

    def make_ready(self) -> BP:
        J = self.J
        bp = BP(
            pelvis=V(0, 0.01, -0.03), hip_pitch=2.0, hip_yaw=-6.0, lean=6.0, twist=-4.0, head_pitch=-8.0,
            head_yaw=-6.0, foot_l=J["ankle_l"] + V(0.03, 0.05, 0), foot_r=J["ankle_r"] + V(-0.03, -0.07, 0),
            foot_l_rot=(0, 0, 12), foot_r_rot=(0, 0, 18), knee_l=V(0.3, 1, 0), knee_r=V(-0.3, 1, 0), flow=0.1)
        return bp.but(**self.rest_hands())

    def idle_layer(self, bp: BP, t: float, ph: float) -> BP:
        # at rest: the right stick idly tapping the thigh, the head turning slowly
        tap = max(0.0, math.sin(ph * 2.0)) ** 2
        return bp.but(**self.rest_hands(tap=tap), head_yaw=bp.head_yaw + 6.0 * math.sin(ph * 0.5))

    def carry(self, bp: BP, sw: float, ph: float) -> BP:
        # marching with the sticks lowered: the arms counter-swing at the sides, the drum riding the hips
        bp = bp.but(head_pitch=-6.0, twist=bp.twist * 0.5)
        return bp.but(**self.rest_hands(swing=0.8 * sw))

    STROKE_TOP = (0.13, 0.10, 0.04)        # side_pt (out, fwd, up) of the hand at the top of a stroke

    def stroke(self, side: str, k: float, bp: BP) -> dict:
        """One drum stroke at lift ``k``: 0 = the knob on the drum head (``strike``), 1 = the top — the hand at
        shoulder height out to the side, the elbow out, the stick cocked up and back over the shoulder. The stick
        turns through pointing forward on the way (a wrist + forearm arc, never through the body)."""
        sg = 1.0 if side == "l" else -1.0
        key = "wpn" if side == "r" else "off"
        c = self.strike(side, 0.0, bp)
        h0, d0 = c[f"hand_{side}"], c[key].normalized()
        h1 = self.side_pt(side, *self.STROKE_TOP)
        dm = V(sg * 0.15, 1.0, 0.10).normalized()
        d1 = V(sg * 0.30, -0.35, 0.90).normalized()
        d = d0.slerp(dm, min(1.0, 2.0 * k)) if k < 0.5 else dm.slerp(d1, 2.0 * k - 1.0)
        h = h0.lerp(h1, k) + V(sg * 0.06 * math.sin(math.pi * k), 0.0, 0.03 * math.sin(math.pi * k))
        up = d.cross(Vector((1.0, 0.0, 0.0)))
        up = up.normalized() if up.length > 1e-3 else Vector((0.0, 0.0, 1.0))
        return {f"hand_{side}": h, key: d, f"{key}_up": up, f"elbow_{side}": V(sg * (0.7 + 0.5 * k), -0.2, -0.5 + 0.6 * k),
                f"clav_{side}": (10.0 * k, 4.0 * k)}

    def attack_keys(self) -> list:
        """A forward overhand blow with the right stick: raised up behind the shoulder, brought over and down at the
        target (Contact 480: the arm reaching forward-right above the shoulder, the stick sloping down at the target's
        head ~0.75 m out, clear of the drum), the left hand swinging back at the side."""
        R0 = self.ready()
        J = self.J
        rest_l = dict(hand_l=self.hang_hand("l", 0.10, -0.06, 0.84), elbow_l=V(0.5, -0.6, -0.4),
                      **self.wkw(168.0, lat=0.35, key="off"))
        lift = R0.but(hand_r=self.side_pt("r", 0.14, 0.12, 0.08), elbow_r=V(-1.0, -0.1, -0.2),
                      **self.wkw(-5.0, lat=-0.25), clav_r=(8.0, 0.0), **rest_l)
        wind = R0.but(pelvis=V(0, -0.02, -0.04), lean=0.0, twist=-24.0, hip_yaw=-14.0, head_pitch=-12.0,
                      hand_r=self.side_pt("r", 0.12, -0.10, 0.30), elbow_r=V(-0.9, 0.0, 0.4),
                      **self.wkw(-48.0, lat=-0.25), clav_r=(16.0, -6.0), flow=0.25, **rest_l)
        mid = R0.but(lean=10.0, twist=-6.0, head_pitch=-14.0, hand_r=self.side_pt("r", 0.08, 0.24, 0.28),
                     elbow_r=V(-0.9, -0.2, 0.3), **self.wkw(25.0, lat=-0.10), clav_r=(12.0, 6.0), flow=0.35, **rest_l)
        # Contact: the arm extended forward and out to the right above shoulder height, the stick sloping down-forward
        # at the target's head / shoulder (~0.75 m out) — on the far side of the right hand from the drum, which
        # hangs front-left. Art review r1 (re-check): the r1 contact kept the hand in front of the belly with the
        # stick level, so at the W1 camera (se: forward runs down the screen) the stick still crossed the drum's
        # front and the blow read as another beat; out 0.28 m with no torso twist it stays clear of the drum in
        # every facing (the side / sw views show the full forward reach)
        hit = R0.but(pelvis=V(0, 0.08, -0.08), lean=20.0, hip_pitch=7.0, twist=-6.0, hip_yaw=-8.0, head_pitch=-14.0,
                     head_yaw=-2.0, foot_l=J["ankle_l"] + V(0.03, 0.16, 0), foot_r=J["ankle_r"] + V(-0.03, -0.12, 0),
                     hand_r=self.side_pt("r", 0.28, 0.42, 0.22), elbow_r=V(-1.0, 0.0, 0.3),
                     **self.wkw(108.0, lat=-0.55), clav_r=(6.0, 14.0), flow=0.5,
                     **{**rest_l, "hand_l": self.hang_hand("l", 0.10, -0.16, 0.82)})
        # the follow-through carries on down the right side (never back across the drum)
        follow = hit.but(lean=22.0, hand_r=self.side_pt("r", 0.30, 0.38, -0.06), **self.wkw(126.0, lat=-0.65),
                         flow=0.4)
        back = R0.but(lean=10.0, hand_r=self.side_pt("r", 0.10, 0.24, -0.40), elbow_r=V(-0.7, -0.4, -0.4),
                      **self.wkw(160.0, lat=-0.30), flow=0.25)
        return [(90.0, lift, "out"), (250.0, wind, "smooth"), (380.0, mid, "in"), (480.0, hit, "linear"),
                (590.0, follow, "out"), (720.0, back, "smooth")]

    # art review r1 (item 11): face down the drummer hid his drum under him (his corpse read as a deserter's); the
    # fall rolls him onto his right side, the drum's pale head turned up beside the body
    DEATH_ROLL = -85.0

    def death_items(self) -> dict:
        up = Vector((0.0, 1.0, 0.0))
        return {"hit": {**self.wkw(150.0, lat=-0.35), **self.wkw(150.0, lat=0.35, key="off")},
                "kneel": {**self.wkw(120.0, lat=-0.5), **self.wkw(120.0, lat=0.5, key="off")},
                "topple": {"wpn": Vector((-0.75, 0.0, 0.66)), "wpn_up": up, "off": Vector((0.75, 0.0, 0.66)),
                           "off_up": up},
                "down": {"wpn": Vector((-0.30, -0.20, 0.93)).normalized(), "wpn_up": up,
                         "off": Vector((0.8, 0.0, 0.6)), "off_up": up,
                         "hand_r": V(-0.16, -0.06, 1.42), "elbow_r": V(-0.6, -0.5, 0.2),
                         "hand_l": V(0.42, -0.18, 0.92), "elbow_l": V(0.9, -0.4, 0.0)}}

    def stun_items(self, bp: BP, ph: float) -> dict:
        # dazed, the sticks hang from limp hands at the sides, pointing down and out (never up at the face)
        U = self.U
        return {"hand_r": self.hang_hand("r", 0.12, 0.10, 0.80) + V(0, 0, 0.5 * U * math.cos(ph)),
                "hand_l": self.hang_hand("l", 0.12, 0.10, 0.80) + V(0, 0, 0.5 * U * math.sin(ph)),
                "elbow_r": V(-0.8, -0.4, -0.4), "elbow_l": V(0.8, -0.4, -0.4),
                **self.wkw(165.0 + 6.0 * math.sin(ph), lat=-0.5), **self.wkw(165.0, lat=0.5, key="off")}

    def recoil(self) -> BP:
        bp = super().recoil()
        return bp.but(**self.rest_hands(swing=-0.4))

    def cast(self):
        """``Cast01`` 1000 loop — the march beat in big strokes: right, left, right, left; each stick rebounds off
        the head, rises to shoulder height with the elbow out (the shoulder lifting with it: an elbow pump the back
        facings see), hangs cocked while the other stick falls, and drives down onto the head; the knees and torso
        bob on every beat (``FX_DrumBeat`` at each contact; ``Release`` on the first: the W2 channel starts)."""
        R0 = self.ready()
        L = self.CAST_MS
        J = self.J
        period = L / 2.0

        def lift(side, t_ms):
            hits = [ms for s_, ms in self.BEATS if s_ == side]
            tau = min(((t_ms - h) % L) % period for h in hits) / period     # 0 at this hand's contact
            if tau < 0.30:
                return HB.smooth01(tau / 0.30)
            if tau < 0.62:
                return 1.0
            return 1.0 - ((tau - 0.62) / 0.38) ** 2

        def pulse(t_ms):
            d = min(min(abs(t_ms - ms), L - abs(t_ms - ms)) for _, ms in self.BEATS)
            return math.exp(-(d / 55.0) ** 2)

        def s(t_ms: float) -> BP:
            ph = 2 * math.pi * (t_ms - self.BEATS[0][1]) / period
            pu = pulse(t_ms)
            bp = R0.but(pelvis=R0.pelvis + V(0, 0.01, -0.035 * pu - 0.01), lean=R0.lean + 4.0 + 5.0 * pu,
                        head_pitch=-4.0 + 7.0 * pu, twist=R0.twist - 7.0 * math.cos(ph), hip_yaw=R0.hip_yaw
                        - 3.0 * math.cos(ph), foot_l=J["ankle_l"] + V(0.04, 0.05, 0),
                        foot_r=J["ankle_r"] + V(-0.04, -0.06, 0), knee_l=V(0.35, 1, 0), knee_r=V(-0.35, 1, 0),
                        flow=0.15 + 0.1 * pu, wave=ph)
            return bp.but(**self.stroke("l", lift("l", t_ms), bp), **self.stroke("r", lift("r", t_ms), bp))
        nt = {"Release": self.BEATS[0][1]}
        for i, (_, ms) in enumerate(self.BEATS):
            nt[f"FX_DrumBeat_{i + 1}"] = ms
        c = self.proc_clip("Cast01", L, s, loop=True, notifies=nt)
        c.key_poses_ms = [0.0, 125.0, 250.0, 375.0, 500.0, 625.0, 750.0, 875.0]
        c.manifest_extra = {"beats": [{"hand": h, "ms": ms} for h, ms in self.BEATS],
                            "note": "loop; the W2 drum channel (2 s every 8 s) plays it twice; FX_DrumBeat_<n> = each "
                                    "stick contact (the drum's dull thud + a dust puff off the head, socket fx_drum). "
                                    "Big strokes (sticks to shoulder height, elbows out, a knee / torso bob per beat) "
                                    "against the Idle's lowered sticks: the drumming silhouette is the W2 tell"}
        return c

    def monster_clips(self) -> list:
        return super().monster_clips() + [self.cast()]


# ── review: the drumming tell against the rest pose (art review r1, item 2) ──────────────────────────────
CAST_IDLE_FACINGS = ("se", "side", "ne", "back", "nw", "side_l", "sw", "front")


def cast_idle_preview(ms, rv, actions) -> dict:
    """``cast_vs_idle.png``: per facing (8 rows) Idle F0, Idle at half its loop, then Cast01 at its four beats and
    the four tops between them, native 1080p game-camera crops; each row labelled with the changed-pixel counts
    against Idle F0 (|Δ| > 25 in any channel): Idle's own motion (max over 8 frames) and the drumming's minimum —
    the r1 rule: the drumming must differ from Idle by well over Idle's own motion in every facing."""
    from kit import review
    rig = ms.A.rig
    acts = dict(zip([c.name for c in ms.clips], actions))
    clips = {c.name: c for c in ms.clips}
    review.setup_render((1920, 1080), 8)
    cam = review.game_camera()
    zc = Vector((0.0, 0.0, ms.height * 0.5))
    c = review.project(cam, zc)
    hw = 0.052
    hh = hw * 1920 / 1080
    box = (c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh)
    rows, out = [], {}

    def shot(clip, f):
        anim.assign(rig, acts[clip])
        anim.set_frame(f)
        bpy.context.view_layer.update()
        rv.game_outline(1080, zc)
        return review.render_array(box)
    for fac in CAST_IDLE_FACINGS:
        rv.facing(fac)
        L_i, L_c = clips["Idle"].frames, clips["Cast01"].frames
        idle = [shot("Idle", round(i * L_i / 8)) for i in range(8)]
        cast = [shot("Cast01", round(i * L_c / 8)) for i in range(8)]
        ref = idle[0].astype(int)

        def changed(im):
            return int((np.abs(im.astype(int) - ref).max(2) > 25).sum())
        own = max(changed(im) for im in idle[1:])
        drum = min(changed(im) for im in cast)
        out[fac] = {"idleOwnMax": own, "castMin": drum, "ratio": round(drum / max(1, own), 2)}
        cells = [idle[0], idle[4]] + cast
        for k, im in enumerate(cells):
            review.draw_text(im, 2, 2, ("IDLE F0", "IDLE MID")[k] if k < 2 else f"CAST {round((k - 2) * 125)}MS")
        review.draw_text(cells[0], 2, cells[0].shape[0] - 18, fac.upper())
        review.draw_text(cells[0], 2, cells[0].shape[0] - 9, f"{drum}/{own} X{drum / max(1, own):.1f}",
                         rgb=(255, 220, 140))
        rows.append(np.concatenate(cells, 1))
        print(f"[cast] {fac:7s} Idle's own motion max {own:5d} px | Cast01 vs Idle F0 min {drum:5d} px "
              f"(x{drum / max(1, own):.2f})")
    rv._save("cast_vs_idle.png", np.concatenate(rows, 0))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)
    rv.facing("se")
    return out


# ── build ──────────────────────────────────────────────────────────────────────────────────────────────
def build_deserter():
    scene.reset()
    pal = Palette(LV.FAMILY)
    folk = DeserterFolk(LOOK, ASSET)
    rig = LV.build_rig(folk)
    body = folk.build(pal, rig)
    hatchet = build_hatchet(pal)
    return pal, folk, rig, body, hatchet


def build_drummer():
    scene.reset()
    pal = Palette(LV.FAMILY)
    folk = DeserterFolk(DRUMMER_LOOK, DRUMMER, helm=True, drum=True)
    rig = LV.build_rig(folk)
    # art review r1 (item 3): fx.drumBeat named the mesh part 'drum_head'; the dust puff needs a real socket
    R.add_socket(rig, "fx_drum", "pelvis", folk.drum_head_point() + folk.drum_axis() * 0.01)
    body = folk.build(pal, rig)
    stick_r = build_stick(pal)
    stick_l = build_stick(pal)
    stick_l.name = STICK_L
    return pal, folk, rig, body, stick_r, stick_l


def build_all():
    """(pal, folk, rig, body) of the deserter (dbg_lod)."""
    return build_deserter()


def deserter_anims(folk, rig, hatchet):
    return DeserterAnims(rig, folk, {hatchet.name: (hatchet, "weapon_r")})


def drummer_anims(folk, rig, sr, sl):
    return DrummerAnims(rig, folk, {sr.name: (sr, "weapon_r"), sl.name: (sl, "weapon_l")})


# ── dev ────────────────────────────────────────────────────────────────────────────────────────────────
def quick(which: str, out: Path) -> None:
    import mship
    if which == "drummer":
        pal, folk, rig, body, sr, sl = build_drummer()
        A = drummer_anims(folk, rig, sr, sl)
        mship.quick_review(DRUMMER, rig, [body], [(sr, "weapon_r"), (sl, "weapon_l")], A, out / "drummer",
                           DRUMMER_HEIGHT, LV.OUTLINE_CLASS, px=LV.OUTLINE_PX)
        return
    pal, folk, rig, body, hatchet = build_deserter()
    A = deserter_anims(folk, rig, hatchet)
    atts = [(build_tagchain(pal, folk, rig), "spine_03"), (build_lootsack(pal, folk, rig), SACK_BONE)] \
        if which == "dreck" else ([(build_red_scarf(pal, folk, rig), "head"), (build_gorget(pal, folk, rig), "spine_03")]
                                  if which == "harke" else [])
    mship.quick_review(ASSET, rig, [body] + [o for o, _ in atts], [(hatchet, "weapon_r")] + atts, A, out / which, HEIGHT,
                       LV.OUTLINE_CLASS, px=LV.OUTLINE_PX)


def anim_quick(which: str, out: Path, names, facing: str = "se") -> None:
    import mship
    if which == "drummer":
        pal, folk, rig, body, sr, sl = build_drummer()
        R.attach_to_bone(sr, rig, "weapon_r")
        R.attach_to_bone(sl, rig, "weapon_l")
        A = drummer_anims(folk, rig, sr, sl)
        att, token, h = [(sr, "weapon_r"), (sl, "weapon_l")], DRUMMER_TOKEN, DRUMMER_HEIGHT
    else:
        pal, folk, rig, body, hatchet = build_deserter()
        R.attach_to_bone(hatchet, rig, "weapon_r")
        A = deserter_anims(folk, rig, hatchet)
        att, token, h = [(hatchet, "weapon_r")], TOKEN, HEIGHT
    cs = [c for c in A.monster_clips() if not names or c.name in names]
    mship.anim_review(rig, [body], att, cs, token, out / which, h, LV.OUTLINE_CLASS, facing=facing, px=LV.OUTLINE_PX)


# ── ship ───────────────────────────────────────────────────────────────────────────────────────────────
GRIP_R = ("hand_r", "thumb_r", "wrist_r", "bracer_r")
GRIP_L = ("hand_l", "thumb_l", "wrist_l", "bracer_l")
NOTES = ("Clips in place; the core moves the actor (play rate = v / ref). Death folds forward face down (W2: the ash "
         "husk takes its pose). Stun (C5) loops.")


def ship_deserter(previews: bool = True, verify: bool = True) -> int:
    import living_ship as LS
    import mship
    LS.march_stage()
    pal, folk, rig, body, hatchet = build_deserter()
    body1 = LS.build_lod1(folk, pal, rig)
    R.attach_to_bone(hatchet, rig, "weapon_r")
    A = deserter_anims(folk, rig, hatchet)
    atts = [(build_tagchain(pal, folk, rig), BANDOLIER, "spine_03", "hunt_dreck"),
            (build_lootsack(pal, folk, rig), LOOTSACK, SACK_BONE, "hunt_dreck"),
            (build_red_scarf(pal, folk, rig), RED_SCARF, "head", "hunt_harke"),
            (build_gorget(pal, folk, rig), GORGET, "spine_03", "hunt_harke")]
    # one Death for the mesh and its variants: the fallen body rests on the hunt leaders' chest attachments
    A.floor_items = tuple((o, bone) for o, _, bone, _ in atts)
    cs = A.monster_clips()
    note = {"variantScale": HUNT_SCALE, "attach": "bone socket, identity transform (weapon-socket convention)"}
    items = [mship.Item(hatchet, HATCHET, "weapon_r", "axe",
                        sockets=[{"name": "tip", "pos": (0.0, HATCHET_TOP, 0.12)},
                                 {"name": "mid", "pos": (0.0, HATCHET_TOP * 0.55, 0.0)}],
                        extra={"lengthCm": round((HATCHET_TOP + 0.16) * 100.0, 1)}, grip=hatchet_grip)]
    for o, asset_name, bone, vid in atts:
        extra = dict(note, glint={"region": "tags", "color": "#D8DCE0"}) if asset_name == BANDOLIER else dict(note)
        items.append(mship.Item(o, asset_name, bone, variant=vid, category="Monsters", extra=extra))
    groups = LS.qa_groups(body, GRIP_R)
    names = LS.part_names(body)
    groups.update({
        "body_nohead": mship.parts_except(body, ("skull", "hood", "face_scarf", "hood_seam", "hood_edge", "hood_tail",
                                                 "brow", "eye", "eye_socket", "cowl", "capelet", "cowl_fold")),
        "body_nochest": mship.parts_except(body, ("torso", "cowl", "capelet", "insignia", "insignia_thread", "quilt",
                                                  "collar", "neck_opening", "neck", "cowl_fold")),
        "body_nopack": mship.parts_except(body, ("pack", "pack_flap", "pack_pocket", "bedroll", "pack_strap", "lace",
                                                 "strap_buckle")),
    })
    del names
    att_pairs = {"bandolierBody": ("item:" + BANDOLIER, "body_nochest"), "sackBody": ("item:" + LOOTSACK, "body_nf"),
                 "scarfBody": ("item:" + RED_SCARF, "body_nohead"), "gorgetBody": ("item:" + GORGET, "body_nochest"),
                 "sackAxe": ("item:" + LOOTSACK, "item:" + HATCHET), "bandolierAxe": ("item:" + BANDOLIER, "item:" + HATCHET)}
    ms = mship.MonsterShip(
        asset=ASSET, token=TOKEN, preview_dir="mon_deserter", pal=pal, A=A, body=body, items=items, clips=cs,
        outline_class=LV.OUTLINE_CLASS, game_ids=["monster_deserter", *VARIANTS], blob_radius=0.30, height=HEIGHT,
        lod1=body1, display=DISPLAY,
        fx={"telegraph": {**LS.TELEGRAPH, "untilMs": round(A.WINDUP_FRAC * A.CONTACT_MS, 1)},
            "attackTrail": {"color": LS.TRAIL, "alpha": 0.45, "sockets": ["tip", "mid"], "item": HATCHET},
            "blobShadow": LS.DEATH_SHADOW},
        qa_groups=groups,
        qa_pairs={"axeBody": ("item:" + HATCHET, "body_nf"), **att_pairs},
        qa_limits={"ground": -0.5, "axeBody": 0, "coatLegs": 0, **{k: 0 for k in att_pairs}},
        qa_clip_limits={"Death": {"coatLegs": 1e9, "coatLegsMm": 25.0}},
        outline_px=LV.OUTLINE_PX, outline_px_note=LV.OUTLINE_PX_NOTE,
        variants=VARIANTS, extra_previews=lambda ms_, rv, acts: (LS.corpse_preview(ms_, rv, acts),
                                                                  LS.hunt_previews(ms_, rv, acts, VARIANTS)),
        variant_previews=("hunt_leaders.png", "hunt_close.png"),
        game_keys=[("Idle", 0, "se"), ("Walk", 13, "se"), ("Attack01", 15, "se"), ("Attack01", 27, "se"),
                   ("Attack01", 27, "sw"), ("Hurt", 4, "se"), ("Death", 30, "se"), ("Stun", 10, "se")],
        contact_keys=[("Attack01", 27, "se"), ("Attack01", 27, "sw"), ("Attack01", 27, "front"),
                      ("Attack01", 27, "ne")],
        close_keys=[("Attack01", 8, "se"), ("Attack01", 17, "se"), ("Attack01", 27, "se"), ("Attack01", 27, "side"),
                    ("Hurt", 4, "se"), ("Death", 22, "se"), ("Death", 36, "se"), ("Death", 54, "se"),
                    ("Stun", 10, "front"), ("Walk", 13, "side")],
        skeleton_bones_required=("root", "pelvis", "head", "weapon_r", "weapon_l", "skirt_f_02", "coat_th_l_01",
                                 "pack_01", "ik_foot_l", "ik_hand_gun"),
        lod_note=f"LOD0 budget {LS.LOD0_BUDGET} / LOD1 {LS.LOD1_BUDGET} toon tris (monsters.md 2.2, the Unburied kit "
                 "the Living share).",
        extra={**LS.remake_extra("axe_overhand", A.CONTACT_MS, A.REF_SPEED,
                                 NOTES + " [W2] morale: flees 5 s when the pack leader dies (the Walk clip at a run "
                                         "rate, facing away) unless a drummer drums."),
               **LS.marker_extra(MARKER)},
    )
    LS.report_budget(ms)
    LS.install_coat_qa()
    res = mship.ship(ms, previews=previews, verify=verify)
    return mship.main_result(res)


def ship_drummer(previews: bool = True, verify: bool = True) -> int:
    import living_ship as LS
    import mship
    LS.march_stage()
    LS.items_sheet_without((STICK_L,))
    pal, folk, rig, body, sr, sl = build_drummer()
    body1 = LS.build_lod1(folk, pal, rig)
    R.attach_to_bone(sr, rig, "weapon_r")
    R.attach_to_bone(sl, rig, "weapon_l")
    A = drummer_anims(folk, rig, sr, sl)
    cs = A.monster_clips()
    tip = (0.0, STICK_LEN - 0.006, 0.0)
    socks = [{"name": "tip", "pos": tip}, {"name": "mid", "pos": (0.0, 0.12, 0.0)}]
    # the left stick is a build-time copy (QA and previews need both hands' items): it ships as STICK on weapon_l —
    # its FBX / manifest entry are removed after the ship and the drummer's attachments list STICK twice
    items = [mship.Item(sl, STICK_L, "weapon_l", "club", sockets=socks, grip=stick_grip),
             mship.Item(sr, STICK, "weapon_r", "club", sockets=socks,
                        extra={"lengthCm": round((STICK_LEN + 0.045) * 100.0, 1),
                               "pair": "attached twice: weapon_r and weapon_l (the drummer's two sticks)"},
                        grip=stick_grip)]
    groups = LS.qa_groups(body, GRIP_R + GRIP_L)
    groups["drum"] = [n for n in LS.part_names(body) if n.startswith("drum") and n not in ("drum_strap", "drum_hook")]
    groups["body_nodrum"] = mship.parts_except(body, tuple(groups["drum"]) + GRIP_R + GRIP_L + (
        "belt", "buckle", "buckle_in", "drum_hook"))
    ms = mship.MonsterShip(
        asset=DRUMMER, token=DRUMMER_TOKEN, preview_dir="mon_deserter_drummer", pal=pal, A=A, body=body, items=items,
        clips=cs, outline_class=LV.OUTLINE_CLASS, game_ids=["monster_deserter_drummer"], blob_radius=0.30,
        height=DRUMMER_HEIGHT, lod1=body1, display=DRUMMER_DISPLAY,
        fx={"telegraph": {**LS.TELEGRAPH, "untilMs": round(A.WINDUP_FRAC * A.CONTACT_MS, 1)},
            "attackTrail": {"color": LS.TRAIL, "alpha": 0.4, "sockets": ["tip", "mid"], "item": STICK},
            "drumBeat": {"notifies": [f"FX_DrumBeat_{i + 1}" for i in range(len(A.BEATS))], "socket": "fx_drum",
                         "dust": "#8A8070", "note": "a puff of dust off the drum head at each beat (A12 recipe), at "
                                                     "the skeleton socket fx_drum (pelvis bone, the centre of the "
                                                     "drum's head 1 cm proud); no glow: the Living carry no light"},
            "blobShadow": LS.DEATH_SHADOW},
        qa_groups=groups,
        qa_pairs={"stickBody": ("item:" + STICK, "body_nf"), "stickLBody": ("item:" + STICK_L, "body_nf"),
                  "drumLegs": ("drum", "legs"), "drumBody": ("drum", "body_nodrum")},
        qa_limits={"ground": -0.5, "stickBody": 0, "stickLBody": 0, "drumLegs": 0, "drumBody": 0, "coatLegs": 0},
        qa_clip_limits={"Death": {"coatLegs": 1e9, "coatLegsMm": 25.0, "drumLegs": 1e9, "drumBody": 1e9}},
        outline_px=LV.OUTLINE_PX, outline_px_note=LV.OUTLINE_PX_NOTE,
        extra_previews=lambda ms_, rv, acts: (LS.corpse_preview(ms_, rv, acts), cast_idle_preview(ms_, rv, acts)),
        game_keys=[("Idle", 0, "se"), ("Walk", 14, "se"), ("Attack01", 16, "se"), ("Attack01", 29, "se"),
                   ("Cast01", 8, "se"), ("Cast01", 23, "sw"), ("Hurt", 4, "se"), ("Stun", 10, "se")],
        contact_keys=[("Attack01", 29, "se"), ("Attack01", 29, "sw"), ("Cast01", 8, "front"), ("Cast01", 38, "ne")],
        close_keys=[("Attack01", 16, "se"), ("Attack01", 29, "se"), ("Cast01", 8, "se"), ("Cast01", 23, "se"),
                    ("Hurt", 4, "se"), ("Death", 22, "se"), ("Death", 36, "se"), ("Death", 54, "se"),
                    ("Stun", 10, "front"), ("Walk", 14, "side")],
        back_clips=("Attack01", "Cast01", "Death"),
        skeleton_bones_required=("root", "pelvis", "head", "weapon_r", "weapon_l", "skirt_f_02", "coat_th_l_01",
                                 "ik_foot_l", "ik_hand_gun"),
        lod_note=f"LOD0 budget {LS.LOD0_BUDGET} / LOD1 {LS.LOD1_BUDGET} toon tris (monsters.md 2.2).",
        extra={**LS.remake_extra("club_overhead", A.CONTACT_MS, A.REF_SPEED,
                                 NOTES + " Cast01 = drumming (loop; the W2 channel: 2 s every 8 s, deserters within 8 "
                                         "tiles cannot be feared and attack 10 % faster)."),
               **LS.marker_extra(("deserter.marker",))},
    )
    LS.report_budget(ms)
    LS.install_coat_qa()
    # the left stick is a build-time copy: never exported (no second FBX / manifest entry), never re-imported by the
    # verification (two items with the same sockets collide in one scene); QA and previews see it as a held item
    ship_item0, verify0 = mship.ship_item, mship.verify_export
    mship.ship_item = lambda it, pal_, holder, ms_=None: ({} if it.asset == STICK_L else ship_item0(it, pal_, holder, ms_))

    def verify_wo_left(ms_, sk, src, entry):
        keep = ms_.items
        ms_.items = [it for it in keep if it.asset != STICK_L]
        try:
            return verify0(ms_, sk, src, entry)
        finally:
            ms_.items = keep
    mship.verify_export = verify_wo_left
    try:
        res = mship.ship(ms, previews=previews, verify=verify)
    finally:
        mship.ship_item, mship.verify_export = ship_item0, verify0
    # the left stick is the same SM on weapon_l
    from kit import export, paths
    man = export.Manifest()
    e = man.data["assets"].get(DRUMMER)
    if e is not None:
        e["attachments"] = [{"object": STICK, "socket": "weapon_r"}, {"object": STICK, "socket": "weapon_l"}]
        man.set_asset(DRUMMER, e)
    if STICK_L in man.data["assets"]:
        man.delete_asset(STICK_L) if hasattr(man, "delete_asset") else man.data["assets"].pop(STICK_L)
    man.save()
    fbx = paths.category_dir("Weapons") / f"{STICK_L}.fbx"
    if fbx.exists():
        fbx.unlink()
    return mship.main_result(res)


def main(which: str, previews: bool = True, verify: bool = True) -> int:
    rc = 0
    if which in ("deserter", "all", "dreck", "harke"):
        rc |= ship_deserter(previews, verify)
    if which in ("drummer", "all"):
        rc |= ship_drummer(previews, verify)
    return rc


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    which = next((a for a in args if a in ("deserter", "drummer", "dreck", "harke", "all")), "deserter")
    scratch = scratch_root("living", "deserter")
    if "--quick" in args:
        quick(which, scratch)
    elif "--anims" in args:
        names = [a for a in args[args.index("--anims") + 1:] if not a.startswith("--") and a not in
                 ("deserter", "drummer", "all")]
        fac = next((a.split("=")[1] for a in args if a.startswith("--facing=")), "se")
        anim_quick(which, scratch / "anims", names, fac)
    else:
        sys.exit(main(which, previews="--no-previews" not in args, verify="--no-verify" not in args))

"""掘坑官 the Pitwarden — ``SK_Mon_Pitwarden`` (dark remake Wave 3, monsters.md §2.2 ``sub_pitwarden``; chapter1.md
§9.2 the Pit of the Faithful, room four "the Pitwarden's Cut"; implementation-plan.md A13 / work package 3C).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/pitwarden.py
        (no flag) ship · --quick [dir] · --anims [Clip …] [--facing=se] · --portrait · --no-previews · --no-verify

The Unburied officer who dug the pits before the battle (the deserter's journal, ``lore_rf_02``, pays off): a footman
of the kit (``unburied.UnburiedKit``: the same body, tabard, mail and violet pinpoint eyes) bare-headed — his own helm
is in the wall with the others — with what monsters.md asks for:

* an **officer's sash** (``pitwarden.sash``, tarnished edges ``unburied.trim``) from the right shoulder to the left
  hip, front and back, a knot on the belt and two tails that hang and stream (cloth in the skeletal mesh: the chain
  ``sash_01..02`` under ``pelvis``);
* a **gorget** (``pitwarden.gorget``: a brass collar and a crescent bib over the tabard's neck, a dark rim, a boss);
* a **long spade** (``SM_MonWeapon_PitSpade``, new held item on ``weapon_r``; ``pitwarden.spade`` / ``pitwarden.haft``):
  1.99 m, a long trenching blade with foot treads, an iron socket with langets, a shod butt. He stands warden over his
  pit with it: stood off his right side like a staff of office, the haft canted outward (``CARRY_*``: the blade
  clears his bare head in all eight facings, and the elite sigil sits over the head, not over the spade).

Behaviour (monsters.md 2.0 / 2.2): L8, 420 HP, speed 40 (1.20 m/s = Walk ref speed), range 1.8, attack 1500 ms,
SubDungeonMiniBoss with two shieldbearers. Clips and the beats the core needs (``CONTACT`` below):

| Clip | ms | Beats (notifies) | Use |
|---|---|---|---|
| ``Attack01`` | 1250 | WindupEnd 384, **Contact 620** | the basic blow: the spade heaved up in both fists and chopped down edge-first |
| ``Attack02`` | 1400 | WindupEnd 434, **Contact 700** | 铲扫 the spade sweep (cone 100 deg, r 2.2): coiled to his right, swept flat across the front |
| ``Cast01`` | 1600 | Plant 330 (``FX_PitDig``), WindupEnd 496, **Contact 800** (``FX_PitCollapse``) | 掘 Dig (every 12 s): the spade turned over and driven in ahead (Plant), his boot on the tread, then levered back: the 4-tile line collapses at Contact |
| ``Idle_Order`` | 3000 | ``FX_PitDig`` 900 | his last order: he digs a pit that is already full, and looks into it |
| ``Idle`` / ``Walk`` / ``Hurt`` / ``Death`` / ``Rise`` / ``Stun`` | 1600 / 1000 / 320 / 1000 / 1300 / 700 | FootL / FootR; RiseUp | the kit's, standing warden (the spade canted off his right side). Death: the knees give, he hangs on the planted spade, kneels and folds at its foot, the fist still on the haft; Rise reverses it |

Budget (monsters.md §5): LOD0 <= 7 k / LOD1 <= 3 k toon triangles (the arrows of the rank and file are left off: he
did not die in the line). Ink 3.5 px (the Unburied's per-asset request). Portrait ``T_UI_Portrait_Pitwarden``.
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Quaternion, Vector  # noqa: E402

from kit import anim, mesh as M, rig as R, scene  # noqa: E402
from kit.palette import Palette  # noqa: E402
from kit.paths import scratch_root  # noqa: E402
from kit.rig import Bind  # noqa: E402

import common as C  # noqa: E402
from common import BP, K, V  # noqa: E402
import goblins as GB  # noqa: E402
import hero_base as HB  # noqa: E402
import mgeo as G  # noqa: E402
import unburied as UB  # noqa: E402
import risen_footman as RF  # noqa: E402
from uclips import UnburiedAnims, WINDUP_OF_CONTACT  # noqa: E402

ASSET = "SK_Mon_Pitwarden"
TOKEN = "Mon_Pitwarden"
PREVIEW = "mon_pitwarden"
GAME_IDS = ["monster_sub_pitwarden"]
DISPLAY = {"zh-CN": "掘坑官", "en": "the Pitwarden"}
SPADE = "SM_MonWeapon_PitSpade"
PORTRAIT = "T_UI_Portrait_Pitwarden"
REF_SPEED = 40 * 0.03                 # monsters.md speed 40 -> 1.20 tiles/s = m/s
HEIGHT = 1.80
BLOB = 0.34
LOD0_MAX, LOD1_MAX = 7000, 3000
CROP_H = 2.45                         # review framing height: the blade's tip stands 2.04 m over the ground at
                                      # rest and passes 2.4 m in the chop's wind-up

# ── beats (ms) the core's abilities and the UE play rate key on ────────────────────────────────────────────────
CONTACT_MS, ATTACK_MS = 620.0, 1250.0                  # Attack01
SWEEP_CONTACT, SWEEP_MS = 700.0, 1400.0                # Attack02 (monsters.md: spade sweep, 700 ms)
DIG_PLANT, DIG_CONTACT, DIG_MS = 330.0, 800.0, 1600.0  # Cast01 (monsters.md: the line collapses after 800 ms)
ORDER_MS, ORDER_DIG = 3000.0, 900.0
CONTACT = {"Attack01": CONTACT_MS, "Attack02": SWEEP_CONTACT, "Cast01": DIG_CONTACT}
WALK = dict(RF.WALK, cycle=1000.0, lift=0.085, lean=4.0)
LOOK = UB.ULook(arrows=(), sash=True,
                extra={"swatches": ("pitwarden.sash", "pitwarden.gorget"),
                       "lod1_skip": ("gorget_rim", "gorget_boss", "sash_fringe")})

# Swatch this member appends to the Monsters_March registry (append-only). Art review r1 (3C): A1's ``pitwarden.sash``
# (#5A1A1C, cloth) is the tabard's own crimson (#6B1F1F) a step darker — at the W1 camera the band could not be told
# from the cloth under it (dL* ~4), so the officer's mark did not exist. The sash is tarnished cloth-of-gold: an ochre
# below the gorget's brass (hue 41 deg, sat .51: outside the ember band), with scorched edges (``unburied.tabard_char``);
# ``pitwarden.sash`` stays as its crimson lining (the tails' inner faces, the knot's wrap).
APPENDED = {"pitwarden.sash_gold": ("#7E6A3E", 0.52, 0.24, 0.0)}

# ── the spade in weapon_r space (the fist at the origin, +Y toward the blade, +Z the blade's face normal) ───────
BUTT_Y, NECK_Y = -0.88, 0.72
BLADE_Y0, BLADE_L, BLADE_HW = 0.76, 0.345, 0.098
TIP_Y = BLADE_Y0 + BLADE_L
HAFT_R = 0.019
TWO_HAND = (-0.40, 0.0, 0.0)          # the left fist lower on the haft in the two-handed clips
BUTT_AT = V(-0.37, 0.14, 0.025)       # where the shod butt stands (body space): by the right foot
# The carry (READY, Idle, Walk, Hurt, Stun): the spade stood off his right side like a staff of office, its shod butt
# a hand over the ground outside the right foot, the haft canted outward — art review r1 (3C): borne upright beside
# the head (the fist at the rib, the blade over the crown) the blade covered a third of his bare head facing SE / E
# and the elite sigil, solved clear of it, floated over the spade instead of over him. The blade now stands 62-70 cm
# out from the head's axis: beside the head in the 3/4 views, over the upper arm when the spade is camera-side.
CARRY_BUTT = V(-0.27, 0.14, 0.10)
CARRY_LEAN = (-0.22, 0.02)            # (lateral, forward) per unit up: 12.5 degrees outward


# ── the spade ──────────────────────────────────────────────────────────────────────────────────────────────────
def build_spade(pal: Palette, lod: int = 0) -> tuple[bpy.types.Object, list]:
    """The Pitwarden's long spade: an ash-dark haft (``pitwarden.haft``) with two leather grip bands, an iron shoe on
    the butt, an iron socket with two langets riveted down the haft, a long narrow trenching blade of dull iron
    (``pitwarden.spade``), dished, its shoulders rolled into foot treads, the worn edge bright, grave earth caked
    on the lower face."""
    b = UB.att_builder(SPADE, pal, lod)
    b.use("pitwarden.spade", "pitwarden.haft", "unburied.steel_edge", "unburied.helm_dent", "unburied.leather_dark",
          "unburied.skin_line")
    haft = M.tube([(0, BUTT_Y + 0.07, 0), (0, -0.2, 0), (0, NECK_Y, 0)], [HAFT_R, HAFT_R, HAFT_R * 0.92],
                  sides=8, dome0=None, dome1=0.004)
    hid = b.add(haft, "pitwarden.haft", name="haft", hull_pad=True)
    shoe = M.tube([(0, BUTT_Y, 0), (0, BUTT_Y + 0.04, 0), (0, BUTT_Y + 0.10, 0)],
                  [0.011, HAFT_R + 0.003, HAFT_R + 0.002], sides=8, dome0=0.004, dome1=None)
    b.add(shoe, "helm_dent", name="spade_shoe", hull_pad=True)
    for y0, y1 in ((-0.11, 0.10), (TWO_HAND[0] - 0.10, TWO_HAND[0] + 0.10)):
        band = M.tube([(0, y0, 0), (0, y1, 0)], [HAFT_R + 0.002] * 2, sides=8, cap0=None, cap1=None)
        b.add(band, "leather_dark", name="spade_grip", outline=False, covered_by=hid)
    sock = M.tube([(0, NECK_Y - 0.06, 0), (0, NECK_Y + 0.02, 0), (0, BLADE_Y0 + 0.03, 0)],
                  [HAFT_R + 0.004, HAFT_R + 0.007, HAFT_R + 0.010], sides=8, dome0=0.0, dome1=0.0)
    b.add(sock, "helm_dent", name="spade_socket", hull_pad=True)
    for sz in (1.0, -1.0):                                   # langets down the haft, front and back
        lg = M.box((0.016, 0.20, 0.006), (0, NECK_Y - 0.14, sz * (HAFT_R + 0.001)), bevel=0.0)
        b.add(lg, "helm_dent", name="langet", outline=False, covered_by=hid)
    w = BLADE_HW
    ol = [(-w, 0.0), (w, 0.0), (w * 1.03, 0.07), (w * 0.93, 0.21), (w * 0.60, 0.305), (0.0, BLADE_L),
          (-w * 0.60, 0.305), (-w * 0.93, 0.21), (-w * 1.03, 0.07)]
    blade = M.plate(C.ccw(ol), 0.009, bevel=0.002)
    blade.transform(Matrix.Translation((0, BLADE_Y0, 0)))
    dish = lambda x: 0.020 * (x / w) ** 2                    # noqa: E731
    blade.deform(lambda co: Vector((co.x, co.y, co.z + dish(co.x))))
    bl = b.add(blade, "pitwarden.spade", name="spade", hull_pad=True)
    for sg in (1.0, -1.0):                                   # the rolled foot treads on the shoulders
        tr = M.tube([(sg * 0.026, BLADE_Y0 + 0.004, dish(0.026) + 0.004), (sg * w * 0.62, BLADE_Y0 + 0.002,
                                                                           dish(w * 0.62) + 0.006),
                     (sg * w, BLADE_Y0 + 0.004, dish(w) + 0.004)], [0.0085, 0.0095, 0.0085], sides=5, dome0=0.003,
                    dome1=0.003)
        b.add(tr, "helm_dent", name="tread", outline=False, covered_by=bl)
    edge = [Vector((x, BLADE_Y0 + y, dish(x))) for x, y in
            ((w * 0.93, 0.21), (w * 0.60, 0.303), (0.0, BLADE_L - 0.003), (-w * 0.60, 0.303), (-w * 0.93, 0.21))]
    b.add(M.tube(edge, [0.0046] * 5, sides=4), "steel_edge", name="spade_edge", outline=False,
          covered_by=bl)
    if not lod:
        # grave earth caked on the face toward the tip (both faces), a dark smear up the middle
        for sz in (1.0, -1.0):
            pts = [(-w * 0.55, 0.20), (w * 0.5, 0.215), (w * 0.52, 0.275), (0.0, 0.325), (-w * 0.5, 0.28)]
            clod = M.plate(C.ccw(pts), 0.002)
            clod.transform(Matrix.Translation((0, BLADE_Y0, sz * 0.0058)))
            clod.deform(lambda co: Vector((co.x, co.y, co.z + dish(co.x))))
            b.add(clod, "skin_line", name="earth", outline=False, covered_by=bl)
    obj = UB._finish_item(b, pal, SPADE + ("_LOD1" if lod else ""))
    socks = [{"name": "blade_tip", "pos": (0.0, TIP_Y, 0.0)}, {"name": "blade_mid", "pos": (0.0, BLADE_Y0 + 0.16, 0.0)},
             {"name": "tread", "pos": (0.0, BLADE_Y0, 0.0)}, {"name": "mid", "pos": (0.0, 0.30, 0.0)},
             {"name": "butt", "pos": (0.0, BUTT_Y, 0.0)}]
    return obj, socks


# ── the officer's gear on the kit body ─────────────────────────────────────────────────────────────────────────
SASH_W = 0.100                        # the band's width (m): ~10 px at the W1 camera
SASH_TRIM = 0.014
SASH_PATH = [(0.095, 80.0), (0.30, 54.0), (0.55, 20.0), (0.78, -14.0), (0.95, -50.0), (1.012, -90.0),
             (0.95, -130.0), (0.78, -166.0), (0.55, -200.0), (0.30, -234.0), (0.095, -260.0)]
SASH_STEPS = 24
KNOT_A = 106.0                        # the knot on the belt at the left hip (degrees from the front, + = left)
TAIL_L = (0.36, 0.28)
TAIL_W = 0.062
SASH_CLEAR = 0.014                    # the tails clear the posed leg / mail skirt by this (m)
SASH_PARTS = ("sash", "sash_knot", "sash_tail", "sash_fringe")
GORGET_PARTS = ("gorget", "gorget_bib", "gorget_rim", "gorget_boss")


def knot_at(kit) -> Vector:
    z = kit.th(0.045)
    return kit.torso.point(z, KNOT_A, UB.TABARD_LIFT + 0.030)


def sash_chain(kit) -> list:
    t = knot_at(kit) + Vector((0.012, 0.0, -0.02))
    L = TAIL_L[0]
    return [("sash", "pelvis", [t, t + Vector((0.0, 0.0, -0.5 * L)), t + Vector((0.0, 0.0, -L))])]


def _lift(kit, h: float, a: float) -> float:
    """The band's lift off the torso surface at (h, a): over the tabard panels (front and back, their half angle
    ``tabard_w``) it rides the cloth; on the bare gambeson under the arm it comes down onto it."""
    w = kit.tabard_w(h)
    aa = (a + 180.0) % 360.0 - 180.0
    d = min(abs(aa), abs(abs(aa) - 180.0))                  # degrees from the nearer panel's centre line
    over = 1.0 - G.smooth01((d - (w - 2.0)) / 10.0)
    return 0.0020 + over * UB.TABARD_LIFT


def sash_band(kit, k0: int = 0, k1: int | None = None) -> M.Part:
    """The sash (steps ``k0`` .. ``k1`` of its path) as a painted band on the torso (``af_sub`` 1 = the field, 2 = the tarnished edges): centre line
    :data:`SASH_PATH` (Catmull-Rom in the surface's (h, angle) space), each cross-section laid along the surface (its
    side points are found on the surface itself, not on the tangent plane: a 9 cm band over the ribs would dip into
    the tabard by its sagitta)."""
    us = [p[0] for p in SASH_PATH]
    as_ = [p[1] for p in SASH_PATH]
    ts = [i / (len(SASH_PATH) - 1) for i in range(len(SASH_PATH))]
    cen = [(C._hermite(ts, us, k / SASH_STEPS), C._hermite(ts, as_, k / SASH_STEPS)) for k in range(SASH_STEPS + 1)]
    P = lambda h, a: kit.tp(h, a, _lift(kit, h, a))          # noqa: E731
    offs = (-SASH_W / 2, -SASH_W / 2 + SASH_TRIM, SASH_W / 2 - SASH_TRIM, SASH_W / 2)
    bm = bmesh.new()
    sub = bm.faces.layers.int.new("af_sub")
    rows = []
    for k, (h, a) in enumerate(cen):
        h0, a0 = cen[max(k - 1, 0)]
        h1, a1 = cen[min(k + 1, SASH_STEPS)]
        tg = (P(h1, a1) - P(h0, a0)).normalized()
        n = kit.tn(h, a)
        side = tg.cross(n).normalized()
        # the surface's own axes at (h, a): solve the (dh, da) that moves a point `s` metres along `side`
        eh = (P(h + 0.004, a) - P(h - 0.004, a)) / 0.008
        ea = (P(h, a + 0.5) - P(h, a - 0.5)) / 1.0
        m = np.array([[eh.dot(eh), eh.dot(ea)], [eh.dot(ea), ea.dot(ea)]])
        row = []
        for s in offs:
            dh, da = np.linalg.solve(m, np.array([eh.dot(side) * s, ea.dot(side) * s]))
            row.append(bm.verts.new(tuple(P(h + float(dh), a + float(da)))))
        rows.append((row, n))
    k1 = SASH_STEPS if k1 is None else k1
    for (r0, n0), (r1, _) in list(zip(rows, rows[1:]))[k0:k1]:
        for c in range(3):
            f = bm.faces.new((r0[c], r1[c], r1[c + 1], r0[c + 1]))
            f.normal_update()
            if f.normal.dot(n0) < 0:
                f.normal_flip()
            f[sub] = 1 if c == 1 else 2
    for v in [v for v in bm.verts if not v.link_faces]:
        bm.verts.remove(v)
    bm.normal_update()
    return M.Part(bm).smooth(True)


def tail_panel(top: Vector, length: float, yaw: float, lean: float) -> M.Part:
    """One hanging tail of the knot: a narrow two-sided panel ``length`` long from ``top``, fanned ``yaw`` degrees
    about the vertical and ``lean`` outward, flared a little toward a notched end (``af_sub`` 1 / 2 the faces, 0
    the edges: the tarnished trim)."""
    rows, th = 3, 0.006
    rz = Matrix.Rotation(math.radians(yaw), 3, "Z")

    def grid(off):
        out = []
        for r in range(rows + 1):
            k = r / rows
            w = TAIL_W * (0.72 + 0.42 * k)
            row = []
            for c in range(3):
                u = c / 2 - 0.5
                notch = 0.035 * (1.0 - abs(2 * u)) * (k == 1.0)        # a swallow-tail end
                p = Vector((lean * k + off, u * w, -length * k + notch))
                row.append(top + rz @ p)
            out.append(row)
        return out
    return C.thick_patch(grid(th / 2), grid(-th / 2), sub_tags=True)


def add_officer(kit, b) -> None:
    """The officer's sash (band, knot, two tails on the ``sash`` chain) and the gorget, over the kit body."""
    lod = getattr(kit, "lod", 0)
    sb = kit.spine_bind()
    tab = sorted(UB.part_ids(b, ("tabard_front",)))[0]
    tab_b = sorted(UB.part_ids(b, ("tabard_back",)))[0]
    # the band (painted: it lies on the body, the body's hull inks the silhouette)
    import march_palettes as MP
    from kit.palette import Region
    for full, (hx, s_, l_, e_) in APPENDED.items():
        b.swatch[full] = b.palette.add(full, Region(hx, s_, l_, e_, MP.O_MON))
    gold = "pitwarden.sash_gold"
    half = SASH_STEPS // 2                                   # the shoulder's top: front panel / back panel
    for k0, k1, cov in ((0, half, tab), (half, SASH_STEPS, tab_b)):
        b.add(sash_band(kit, k0, k1), gold, sb, name="sash", outline=False, covered_by=cov,
              sub_regions={1: gold, 2: "tabard_char"})
    # the knot on the belt (its wrap shows the crimson lining)
    kp = knot_at(kit)
    kb = Bind.blend("pelvis", "spine_01", smooth=0.04)
    knot = HB.ell((0.050, 0.040, 0.046), tuple(kp), sides=7, rings=4)
    kid = b.add(knot, gold, kb, name="sash_knot", hull_pad=True)
    wrap = M.tube([kp + Vector((0.0, -0.036, 0.0)), kp + Vector((0.026, 0.0, 0.004)), kp + Vector((0.0, 0.036, 0.0))],
                  [0.012, 0.013, 0.012], sides=5)
    b.add(wrap, "pitwarden.sash", kb, name="sash_fringe", outline=False, covered_by=kid)
    # the tails (cloth: sash_01..02): gold outside, the crimson lining inside, scorched edges
    top = knot_at(kit) + Vector((0.012, 0.0, -0.02))
    bind = GB.chain_bind(top, top + Vector((0, 0, -TAIL_L[0])), ("pelvis", "sash_01", "sash_02"))
    for k, (L, yaw, lean) in enumerate(((TAIL_L[0], -13.0, 0.030), (TAIL_L[1], 17.0, 0.046))):
        part = tail_panel(top + Vector((0.007 * k, 0.0, 0.0)), L, yaw, lean)
        b.add(part, gold, bind, name="sash_tail",
              sub_regions={1: gold, 2: "pitwarden.sash", 0: "tabard_char"}, hull_pad=True)
    # the gorget: a collar round the neck's base, a crescent bib over the tabard's neck, a dark rolled rim, a boss
    gb = Bind.rigid("spine_03")
    surf = _Lifted(kit, UB.TABARD_LIFT + 0.001)
    u_c = lambda a: 1.022 - 0.022 * math.cos(math.radians(a))          # noqa: E731  (the collar dips at the throat)
    collar = G.surface_ring(surf, u_c, 0.034, 0.012, out0=0.0, segs=12 if lod else 14)
    gid = b.add(collar, "pitwarden.gorget", gb, name="gorget", hull_pad=True)
    A_BIB = 50.0

    def bot(a):
        return 0.815 + 0.150 * (abs(a) / A_BIB) ** 1.7

    cols, rws = (6, 2) if lod else (8, 2)
    outer, inner = [], []
    for r in range(rws + 1):
        ro, ri = [], []
        for c in range(cols + 1):
            a = -A_BIB + 2 * A_BIB * c / cols
            h = G.lerp(0.992, bot(a), r / rws)
            ro.append(kit.tp(h, a, UB.TABARD_LIFT + 0.012))
            ri.append(kit.tp(h, a, UB.TABARD_LIFT + 0.0005))
        outer.append(ro)
        inner.append(ri)
    bid = b.add(C.thick_patch(outer, inner), "pitwarden.gorget", gb, name="gorget_bib", hull_pad=True)
    path = [(bot(a) + 0.012, a) for a in np.linspace(-A_BIB + 3.0, A_BIB - 3.0, 7)]
    b.add(G.surface_paint(_Lifted(kit, UB.TABARD_LIFT + 0.0132), path, 0.011, 0.0, steps=8), "helm_dent", gb,
          name="gorget_rim", outline=False, covered_by=bid)
    b.add(G.surface_dot(_Lifted(kit, UB.TABARD_LIFT + 0.012), 0.90, 0.0, 0.016, 0.010, low=True), "trim", gb,
          name="gorget_boss", outline=False, covered_by=bid)
    del gid


class _Lifted:
    """``mgeo`` surface adapter: the torso surface ``lift`` metres out (the tabard's face and what rides on it)."""

    def __init__(self, kit, lift: float):
        self.k, self.lift = kit, lift

    def point(self, u, a, out=0.0):
        return self.k.tp(u, a, self.lift + out)

    def normal(self, u, a):
        return self.k.tn(u, a)


# ── poses ──────────────────────────────────────────────────────────────────────────────────────────────────────
FACE = V(-0.30, 1.0, 0.0)             # the blade's face looks forward-right (the W1 camera's side) at rest


def spade(lat: float, fwd: float, up: float, face=None) -> dict:
    """The spade's direction (+Y, toward the blade) and the blade's face normal as close to ``face`` as it allows."""
    d = V(lat, fwd, up).normalized()
    f = Vector(face if face is not None else FACE)
    f = f - d * f.dot(d)
    if f.length < 1e-4:
        f = V(0.0, 0.0, 1.0) - d * d.z
    return {"wpn": d, "wpn_up": f.normalized()}


def planted(lat: float, fwd: float, up: float, face=None) -> dict:
    """The spade standing on its shod butt at :data:`BUTT_AT`, leaning (lat, fwd, up): the fist where the haft's
    grip then is (0.88 m up the haft)."""
    kw = spade(lat, fwd, up, face)
    return dict(kw, hand_r=BUTT_AT + kw["wpn"] * (-BUTT_Y))


def level(A, bp: BP) -> BP:
    """(kept for callers) The blade's face is set in :func:`warden_cloth`: the spade is turned about its own round
    haft inside the fist, the fist keeps its natural hammer grip. Art review r1 (3C): rolling the *fist* about the
    haft to turn the blade (the standard-bearer's ``level``) folded the hand back over the wrist for the 100-160
    degree turns the chop, the dig and the fold ask for, and ran the haft through the cuff and forearm."""
    return bp


def carried(lat: float = 0.0, fwd: float = 0.0, lift: float = 0.0, face=None) -> dict:
    """The carry: the spade canted :data:`CARRY_LEAN` (+ ``lat`` / ``fwd``) with its butt at :data:`CARRY_BUTT`
    (+ ``lift``), the fist where the haft's grip then is."""
    kw = spade(CARRY_LEAN[0] + lat, CARRY_LEAN[1] + fwd, 1.0, face)
    return dict(kw, hand_r=CARRY_BUTT + V(0.0, 0.0, lift) + kw["wpn"] * (-BUTT_Y))


def ready(A: UnburiedAnims) -> BP:
    """READY, standing warden: hunched like the rank and file (lean 10, the head on the broken neck), the spade
    stood off his right side like a staff of office (:func:`carried`): the arm let down and out to the grip, the
    shod butt a hand above the ground outside the right foot, the haft canted 12.5 degrees outward so the blade
    stands clear of his bare head in every facing (its face to the front); the left hand hangs."""
    J = A.J
    return BP(pelvis=V(0.0, -0.02, -0.045), lean=10.0, hip_pitch=3.0, side=-2.0, twist=-4.0, hip_yaw=-3.0,
              head_pitch=-32.0, head_roll=13.0, head_yaw=-6.0,
              foot_l=J["ankle_l"] + V(0.02, 0.06, 0.0), foot_r=J["ankle_r"] + V(-0.04, -0.06, 0.0),
              foot_l_rot=(0.0, 0.0, 8.0), foot_r_rot=(12.0, 0.0, -14.0), knee_l=V(0.25, 1, 0), knee_r=V(-0.25, 1, 0),
              elbow_r=V(-0.9, -0.4, -0.2), **carried(),
              hand_l=V(0.31, 0.17, 0.83), elbow_l=V(0.8, -0.2, -0.4), clav_l=(-8.0, 0.0), clav_r=(4.0, 0.0),
              flow=0.1)


def keys(A: UnburiedAnims) -> dict:
    """Attack01, the chop: the spade heaved up and back over the right shoulder in both fists (the wind-up tint
    reads), then brought down edge-first onto the target with a lurch onto the left foot (Contact 620 ms)."""
    R0 = A.ready()
    J = A.J
    edge = V(1.0, 0.0, 0.15)                                 # the blade's face sideways: its edge leads the chop
    # (the haft's butt stays outside the right thigh all the way over: the blade tips a little to his left going up,
    # the fists stay wide of the hip coming down — QA: the butt cut the leg on the way through)
    wind = R0.but(lean=2.0, twist=-14.0, hip_yaw=-6.0, head_pitch=-24.0, hand_r=V(-0.33, 0.10, 1.42),
                  elbow_r=V(-0.9, 0.1, -0.2), **spade(0.05, -0.22, 1.0, edge), clav_r=(14.0, -4.0), flow=0.3)
    hold = wind.but(hand_r=V(-0.33, 0.06, 1.48), **spade(0.07, -0.36, 1.0, edge), lean=-1.0, twist=-18.0)
    hit = R0.but(pelvis=R0.pelvis + V(0, 0.10, -0.07), lean=24.0, hip_pitch=6.0, twist=12.0, hip_yaw=5.0,
                 head_pitch=-40.0, hand_r=V(-0.31, 0.50, 1.02), elbow_r=V(-0.9, -0.2, -0.4),
                 **spade(0.17, 1.0, -0.16, edge), foot_l=J["ankle_l"] + V(0.03, 0.30, 0.0), clav_r=(2.0, 8.0),
                 flow=0.45)
    follow = hit.but(hand_r=V(-0.31, 0.50, 0.95), **spade(0.17, 1.0, -0.30, edge), lean=27.0)
    recover = R0.but(pelvis=R0.pelvis + V(0, 0.05, -0.04), lean=15.0, hand_r=V(-0.30, 0.30, 1.06),
                     **spade(0.0, 0.55, 0.85), foot_l=J["ankle_l"] + V(0.03, 0.16, 0.0))
    up = planted(0.02, 0.10, 1.0)
    kn = planted(-0.04, 0.22, 1.0)
    fo = planted(-0.06, 0.20, 0.97)
    return dict(wind=wind, hold=hold, hit=hit, follow=follow, recover=recover, lift_ms=250.0, follow_ms=130.0,
                recover_ms=340.0,
                # dying at the foot of the planted spade: the butt never leaves its place, the fist stays on the haft
                buckle_hand_r=up["hand_r"], buckle_elbow_r=V(-0.9, -0.2, -0.3), buckle_wpn=up["wpn"],
                buckle_wpn_up=up["wpn_up"],
                kneel_hand_r=kn["hand_r"], kneel_elbow_r=V(-0.9, 0.0, -0.4), kneel_wpn=kn["wpn"],
                kneel_wpn_up=kn["wpn_up"],
                fold_hand_r=fo["hand_r"], fold_elbow_r=V(-0.9, 0.0, -0.4), fold_wpn=fo["wpn"],
                fold_wpn_up=fo["wpn_up"])


def sweep_poses(A: UnburiedAnims) -> dict:
    """The spade sweep's poses: coiled to his right with the spade drawn back level at the hip, its face up; the
    blade crossing the cone's centre line at Contact; run out to his left; hauled upright."""
    R0 = A.ready()
    J = A.J
    up = V(0.0, 0.0, 1.0)
    pre = R0.but(lean=8.0, twist=-16.0, hand_r=V(-0.38, 0.20, 1.06), **spade(-0.45, -0.10, 0.80, up), flow=0.25)
    wind = R0.but(pelvis=R0.pelvis + V(-0.02, -0.03, -0.05), lean=8.0, twist=-36.0, hip_yaw=-14.0, head_yaw=18.0,
                  head_pitch=-28.0, hand_r=V(-0.44, 0.02, 0.98), elbow_r=V(-0.9, -0.5, -0.2),
                  **spade(-0.80, -0.55, 0.16, up), clav_r=(4.0, -10.0), flow=0.3)
    hold = wind.but(twist=-42.0, hand_r=V(-0.46, -0.02, 0.98), **spade(-0.62, -0.78, 0.18, up))
    hit = R0.but(pelvis=R0.pelvis + V(0.02, 0.09, -0.08), lean=18.0, hip_pitch=4.0, twist=8.0, hip_yaw=4.0,
                 head_pitch=-36.0, hand_r=V(-0.12, 0.54, 0.98), elbow_r=V(-0.8, -0.3, -0.5),
                 **spade(0.16, 1.0, -0.06, up), foot_l=J["ankle_l"] + V(0.04, 0.22, 0.0), clav_r=(2.0, 12.0),
                 flow=0.5)
    follow = hit.but(twist=34.0, hip_yaw=12.0, hand_r=V(0.14, 0.46, 0.98), **spade(0.92, 0.42, -0.02, up),
                     lean=16.0, flow=0.5)
    recover = R0.but(pelvis=R0.pelvis + V(0, 0.04, -0.04), lean=13.0, twist=10.0, hand_r=V(-0.14, 0.34, 1.06),
                     **spade(0.30, 0.55, 0.80), foot_l=J["ankle_l"] + V(0.03, 0.12, 0.0))
    return dict(pre=pre, wind=wind, hold=hold, hit=hit, follow=follow, recover=recover)


def sweep(A: UnburiedAnims):
    """Attack02 1400 ms, 铲扫 the spade sweep (cone 100 deg, r 2.2 tiles): he coils to his right, the spade drawn
    back level at the hip, its face up (WindupEnd 434), then sweeps it flat across the whole front — the blade
    passes the centre of the cone at Contact 700 — runs out to the left and hauls it upright again."""
    R0 = A.ready()
    S = sweep_poses(A)
    pre, wind, hold, hit, follow, recover = (S[n] for n in ("pre", "wind", "hold", "hit", "follow", "recover"))
    L, c_ms = SWEEP_MS, SWEEP_CONTACT
    w_ms = round(WINDUP_OF_CONTACT * c_ms, 1)
    ks = [K(0.0, R0), K(120.0 / L, pre, "out"), K(260.0 / L, wind, "smooth"), K(w_ms / L, hold, "smooth"),
          K(c_ms / L, hit, "in"), K((c_ms + 130.0) / L, follow, "out"), K((c_ms + 380.0) / L, recover, "smooth"),
          K(1.0, R0, "smooth")]
    c = A.clip("Attack02", L, ks, L, notifies={"WindupEnd": w_ms, "Contact": c_ms})
    c.key_poses_ms = sorted({round(k.t * L, 1) for k in ks})
    c.manifest_extra = {"usage": "[W3] 铲扫 the spade sweep (monsters.md sub_pitwarden: cone 100 deg, r 2.2, 700 ms; "
                                 "the crimson cone decal fills to full at Contact)", "skill": "spade_sweep"}
    return c


DIG_LAT = -0.36                       # the dig's line: outside the right shoulder and hip (the haft clears the body)
DIG_HAND = -0.30                      # the left fist on the haft in the dig (above the right one: the blade is down)


def dig_poses(A: UnburiedAnims) -> dict:
    """The dig: the spade turned over (blade forward, then down) at his right side, driven into the ground 0.85 m
    ahead, his right boot stamped on the blade's inner tread, then levered back toward him."""
    R0 = A.ready()
    J = A.J
    fwd = V(0.0, 1.0, 0.25)                                 # the blade's face ahead: a spade is driven face-on
    lift = R0.but(lean=6.0, head_pitch=-26.0, hand_r=V(-0.32, 0.22, 1.26), elbow_r=V(-0.9, -0.1, -0.4),
                  **spade(0.0, 0.80, 0.62, V(0.0, -0.4, 1.0)), clav_r=(8.0, 0.0), flow=0.25)
    over = R0.but(lean=4.0, head_pitch=-18.0, hand_r=V(-0.34, 0.30, 1.40), elbow_r=V(-0.9, 0.0, -0.3),
                  **spade(0.0, 0.92, -0.40, V(0.0, 0.4, 1.0)), clav_r=(14.0, 2.0), flow=0.3)
    plant = R0.but(pelvis=R0.pelvis + V(-0.02, 0.07, -0.10), lean=22.0, hip_pitch=5.0, twist=-16.0, hip_yaw=-6.0,
                   head_pitch=-14.0, head_roll=8.0, head_yaw=-14.0, hand_r=V(DIG_LAT, 0.38, 0.95),
                   elbow_r=V(-0.9, -0.2, -0.3), **spade(0.02, 0.36, -0.94, fwd),
                   foot_l=J["ankle_l"] + V(0.03, 0.14, 0.0), clav_r=(0.0, 8.0), flow=0.4)
    d = spade(0.02, 0.40, -0.92, fwd)["wpn"]
    hand = V(DIG_LAT, 0.41, 0.91)
    tread = hand + d * BLADE_Y0                              # the blade's shoulders, in body space
    stamp = plant.but(pelvis=R0.pelvis + V(-0.05, 0.12, -0.12), lean=26.0, hand_r=hand,
                      **spade(0.02, 0.40, -0.92, fwd),
                      # the boot on the inner tread: its ball over the shoulder, the ankle 10 cm behind and 7 cm up
                      foot_r=Vector((tread.x + 0.088, tread.y + 0.10, tread.z + 0.098)),
                      foot_r_rot=(4.0, 0.0, -2.0), knee_r=V(0.10, 1, 0.1), head_pitch=-8.0)
    lever = plant.but(pelvis=R0.pelvis + V(-0.02, 0.02, -0.10), lean=14.0, twist=-14.0, hand_r=V(DIG_LAT, 0.16, 0.84),
                      **spade(0.02, 0.58, -0.82, fwd), foot_r=J["ankle_r"] + V(-0.02, 0.12, 0.0),
                      foot_r_rot=(0.0, 0.0, -10.0), head_pitch=-20.0, flow=0.45)
    out = R0.but(pelvis=R0.pelvis + V(0, 0.02, -0.03), lean=10.0, twist=-8.0, hand_r=V(-0.34, 0.26, 1.08),
                 **spade(0.0, 0.92, -0.10, V(0.0, 0.3, 1.0)), flow=0.3)
    back = R0.but(lean=8.0, hand_r=V(-0.34, 0.20, 1.16), **spade(0.0, 0.62, 0.78, V(0.0, -0.4, 1.0)), flow=0.25)
    return dict(lift=lift, over=over, plant=plant, stamp=stamp, lever=lever, out=out, back=back)


def dig(A: UnburiedAnims):
    """Cast01 1600 ms, 掘 Dig: he turns the spade over and drives it into the ground ahead (``Plant`` 330 ms,
    ``FX_PitDig``: the crimson line shows from the windup's start), stamps on the tread, throws his weight on the
    haft and levers it back at ``Contact`` 800 ms — the 4-tile line collapses (``FX_PitCollapse``; the damage and the
    30 % slow are the core's) — then wrenches the spade out and stands warden again."""
    R0 = A.ready()
    P = dig_poses(A)
    L = DIG_MS
    w_ms = round(WINDUP_OF_CONTACT * DIG_CONTACT, 1)
    ks = [K(0.0, R0), K(140.0 / L, P["lift"], "out"), K(240.0 / L, P["over"], "smooth"),
          K(DIG_PLANT / L, P["plant"], "in"), K(560.0 / L, P["stamp"], "out"),
          K(DIG_CONTACT / L, P["lever"], "in"), K(980.0 / L, P["lever"].but(lean=12.0), "smooth"),
          K(1180.0 / L, P["out"], "smooth"), K(1380.0 / L, P["back"], "smooth"), K(1.0, R0, "smooth")]
    c = A.one_shot("Cast01", L, ks, notifies={"Plant": DIG_PLANT, "FX_PitDig": DIG_PLANT, "WindupEnd": w_ms,
                                              "Contact": DIG_CONTACT, "FX_PitCollapse": DIG_CONTACT},
                   usage="[W3] 掘 Dig (monsters.md sub_pitwarden: every 12 s a crimson line of 4 tiles collapses "
                         "800 ms after he drives the spade in, slow 30 %): Plant = the blade enters the ground, "
                         "Contact = the collapse (the telegraph fills to full then)")
    c.key_poses_ms = sorted({round(k.t * L, 1) for k in ks})
    c.manifest_extra["skill"] = "dig"
    return c


def idle_order(A: UnburiedAnims):
    """Idle_Order 3000 ms (one-shot idle variant, monsters.md "they keep their last order"): he digs a pit that is
    already full — drives the spade in, stamps, levers a spadeful that is not there — and looks down into it."""
    R0 = A.ready()
    P = dig_poses(A)
    L = ORDER_MS
    look = P["lever"].but(lean=20.0, head_pitch=6.0, head_roll=16.0, head_yaw=10.0)
    ks = [K(0.0, R0), K(420.0 / L, P["lift"], "smooth"), K(680.0 / L, P["over"], "smooth"),
          K(ORDER_DIG / L, P["plant"], "in"), K(1250.0 / L, P["stamp"], "out"), K(1600.0 / L, P["lever"], "smooth"),
          K(1900.0 / L, look, "smooth"), K(2250.0 / L, look.but(head_yaw=-6.0), "smooth"),
          K(2520.0 / L, P["out"], "smooth"), K(2780.0 / L, P["back"], "smooth"), K(1.0, R0, "smooth")]
    c = A.one_shot("Idle_Order", L, ks, notifies={"FX_PitDig": ORDER_DIG},
                   usage="idle variant (monsters.md 'they keep their last order'): he digs a pit that is already "
                         "full and looks into it; one-shot from READY back to READY")
    c.key_poses_ms = sorted({round(k.t * L, 1) for k in ks})
    return c


def portrait_pose(A: UnburiedAnims) -> BP:
    """Portrait: the warden looking up from under his brow, the gorget and the sash's shoulder in the frame."""
    R0 = A.ready()
    return level(A, R0.but(lean=6.0, head_pitch=-20.0, head_roll=9.0, head_yaw=-16.0, twist=-8.0, flow=0.2))


class WardenAnims(UnburiedAnims):
    """Every frame: the left hand on the haft in the two-handed clips (blended in and out at their ends); the
    blade's face and the sash tails are solved in ``warden_cloth``."""
    two_hand_clips = ("Attack01", "Attack02", "Cast01", "Idle_Order")
    HAND_IN, HAND_OUT = 130.0, 240.0

    def body_clip(self, name, length, sampler, loop=False, notifies=None, ref_speed=None, post=None):
        inner = post if post is not None else self.plant

        def post2(bp, t, _inner=inner):
            bp = level(self, bp)
            if name in self.two_hand_clips:
                two = self.second_hand(bp, DIG_HAND if name in ("Cast01", "Idle_Order") else TWO_HAND[0])
                wgt = max(0.0, min(1.0, t / self.HAND_IN, (length - t) / self.HAND_OUT))
                wgt = wgt * wgt * (3.0 - 2.0 * wgt)
                bp = bp.but(hand_l=Vector(bp.hand_l).lerp(Vector(two.hand_l), wgt),
                            elbow_l=V(0.7, 0.2, -0.7) if wgt > 0.5 else bp.elbow_l)
            return _inner(bp, t)
        return HB.hero_clip(self.rig, name, length, sampler, cloth=self.cloth, loop=loop, notifies=notifies,
                            ref_speed=ref_speed, post=post2, grips=self.grips)

    def walk(self, name: str = "Walk", extra=None):
        """The kit's dragging walk with the spade carried as he stands with it (:func:`carried`), lifted a hand
        higher and swinging a little with the stride."""
        def carry(bp, t, sw):
            kw = carried(0.02 * sw, 0.05 * sw, 0.06)
            return bp.but(**dict(kw, hand_r=kw["hand_r"] + (Vector(bp.hand_r) - Vector(self.ready().hand_r))))
        return super().walk(name, extra=carry)


def warden_cloth(p, bp: BP, A) -> None:
    """The spade turned about its own haft so the blade's face looks along the pose's ``wpn_up`` (the fist keeps
    the natural hammer grip ``to_pose`` gave it: a round haft turns in the hand), then the sash tails."""
    p.aim("weapon_r", Vector(bp.wpn), Vector(bp.wpn_up))
    sash_cloth(p, bp, A)


def sash_cloth(p, bp: BP, A) -> None:
    """The sash tails (``sash_01..02``): hanging from the knot under gravity, pushed out off the hip, streaming
    back with the pose's flow and swaying with its gait; each link is turned outward just enough to clear the posed
    left leg, knee pad, boot and mail skirt (the kit's vertex envelope ``A.env``, as its tabard panels do), and
    when the hip comes down (the kneel, the fold) the links swing out so the tails lie on the ground instead of
    passing through it."""
    mats = anim.evaluate(p, return_matrices=True)
    top = mats["sash_01"].translation
    bones = A.rig.obj.data.bones
    L1, L2 = bones["sash_01"].length, bones["sash_02"].length
    pel = mats["pelvis"].to_3x3()
    out_w = (pel @ Vector((1.0, 0.0, 0.0)))
    out_w = Vector((out_w.x, out_w.y, 0.0))
    out_w = out_w.normalized() if out_w.length > 1e-4 else Vector((1.0, 0.0, 0.0))
    back = Vector((0.0, 1.0, 0.0))
    fwd_w = Vector((-out_w.y, out_w.x, 0.0))                 # along the hip, in the ground plane
    yaw_h = math.atan2(out_w.y, out_w.x)                     # the hip's heading (rest: +X = 0)
    pts = A.env.posed(mats) if getattr(A, "env", None) is not None else None
    ph = bp.wave
    head = top.copy()
    for k, (nm, L) in enumerate((("sash_01", L1), ("sash_02", L2 + 0.02))):
        a_out = math.radians(11.0 + 5.0 * k + 10.0 * abs(bp.sway) + 6.0 * math.sin(ph - 0.9 * k) * min(1.0, bp.flow * 3.0))
        if pts is not None:                                  # clear the leg: the obstacle that asks for the most
            rel = pts - np.array(head)
            along = rel @ np.array(fwd_w)
            down = -rel[:, 2]
            out = rel @ np.array(out_w)
            dist = np.hypot(down, out)
            sel = (np.abs(along) < TAIL_W * 0.75) & (down > 0.0) & (dist < L + SASH_CLEAR) & (dist > 1e-4)
            if sel.any():
                need = np.arctan2(out[sel], down[sel]) + np.arcsin(np.minimum(0.999, SASH_CLEAR / dist[sel]))
                a_out = max(a_out, float(need.max()))
        a_out = min(a_out, math.radians(80.0))
        a_back = math.radians((4.0 + 6.0 * k) + bp.flow * (22.0 + 10.0 * k))
        d = (Vector((0.0, 0.0, -1.0)) * math.cos(a_out) + out_w * math.sin(a_out) + back * math.tan(a_back) *
             math.cos(a_out)).normalized()
        floor = 0.012
        if head.z + d.z * L < floor:                         # lay the link out along the ground, away from the hip
            dz = max(-1.0, min(0.0, (floor - head.z) / L))
            flat = Vector((d.x, d.y, 0.0))
            flat = flat.normalized() if flat.length > 1e-4 else out_w
            d = (flat * math.sqrt(max(0.0, 1.0 - dz * dz)) + Vector((0.0, 0.0, dz))).normalized()
        # the link keeps its rest roll about its own axis, turned with the hip's heading (the panels' faces stay
        # outward): the rest Z axis carried by the heading, made square to the link
        zr = Matrix.Rotation(yaw_h, 3, "Z") @ (bones[nm].matrix_local.to_3x3() @ Vector((0.0, 0.0, 1.0)))
        up = zr - d * zr.dot(d)
        p.aim(nm, d, up.normalized() if up.length > 1e-4 else out_w)
        head = head + d * (L1 if k == 0 else L2)


def anims(kit) -> UnburiedAnims:
    return WardenAnims(kit, REF_SPEED, WALK, ready, CONTACT_MS, ATTACK_MS, keys, extra_cloth=warden_cloth)


def clips(A: UnburiedAnims) -> list:
    return [A.idle(), idle_order(A), A.walk(), A.attack(), sweep(A), dig(A), A.hurt(), A.death(1000.0),
            A.rise(1300.0), A.stun()]


# ── build ──────────────────────────────────────────────────────────────────────────────────────────────────────
def make_kit():
    """The kit with the sash chain under ``pelvis`` (the chain's rest points need the built torso profile, which
    the kit has before its rig: ``extra_chains`` is called from the constructor)."""
    scene.reset()
    return UB.UnburiedKit(ASSET, LOOK, height=HEIGHT, extra_chains=sash_chain)


def build_body(kit, pal, lod: int = 0, ratio: float = UB.LOD1_RATIO):
    if lod:
        return kit.build(pal, lod=1, ratio=ratio, hull_ratio=0.75, extra=add_officer)
    return kit.build(pal, extra=add_officer)


def build_lod1(kit, pal):
    from mship import toon_tris
    ratio = UB.LOD1_RATIO
    body1 = build_body(kit, pal, 1, ratio)
    n = toon_tris(body1)
    for _ in range(6):
        if n <= LOD1_MAX:
            break
        me = body1.data
        bpy.data.objects.remove(body1, do_unlink=True)
        bpy.data.meshes.remove(me)
        ratio *= (LOD1_MAX - 120) / n
        body1 = build_body(kit, pal, 1, ratio)
        n = toon_tris(body1)
    print(f"[lod1] {kit.asset}: {n} toon tris at ratio {ratio:.3f}")
    return body1


def build_all(lod1: bool = False):
    kit = make_kit()
    pal = Palette(UB.FAMILY)
    body = build_body(kit, pal)
    body1 = build_lod1(kit, pal) if lod1 else None
    spade_obj, socks = build_spade(pal)
    return dict(pal=pal, kit=kit, body=body, body1=body1, spade=spade_obj, spade_socks=socks)


def dev_items(B, key: str) -> list:
    return [(B["spade"], "weapon_r")]


def quick(out: Path) -> None:
    B = build_all()
    A = anims(B["kit"])
    rig = B["kit"].rig
    R.attach_to_bone(B["spade"], rig, "weapon_r")
    A.bind_meshes(B["body"], {"weapon_r": B["spade"]})
    from mship import toon_tris
    print(f"[tris] body {toon_tris(B['body'])} toon / {M.tri_count(B['body'], 1)} hull; spade "
          f"{toon_tris(B['spade'])} / {M.tri_count(B['spade'], 1)}")
    UB.dev_review(ASSET, rig, [B["body"], B["spade"]], A, out, CROP_H, pose=level(A, A.ready()),
                  glows=RF.eye_glows(rig), blob=BLOB)


def anim_quick(out: Path, names, facing: str = "se") -> None:
    import mship
    B = build_all()
    A = anims(B["kit"])
    R.attach_to_bone(B["spade"], B["kit"].rig, "weapon_r")
    A.bind_meshes(B["body"], {"weapon_r": B["spade"]})
    cs = [c for c in clips(A) if not names or c.name in names]
    UB.march_stage()
    for o in bpy.data.objects:
        if o.type == "MESH" and o not in (B["body"], B["spade"]):
            o.hide_render = True
    mship.anim_review(B["kit"].rig, [B["body"]], [(B["spade"], "weapon_r")], cs, TOKEN, out, CROP_H,
                      UB.OUTLINE_CLASS, blob_radius=BLOB, facing=facing, px=UB.OUTLINE_PX)


# ── ship ───────────────────────────────────────────────────────────────────────────────────────────────────────
GRIP_PARTS = ("fist_r", "thumb_r", "knuckle", "wrist_r", "hand_l", "wrist_l", "fist_l", "thumb_l")
QA_GROUPS = dict(RF.QA_GROUPS)
QA_PAIRS = dict(RF.QA_PAIRS)
EYE_PORTRAIT = {"radiusCm": 3.4, "color": UB.EYE_GLOW, "alpha": 0.6, "coreFrac": 0.3}


def spade_grip(co) -> bool:
    """Spade faces in the fists: the right round the upper grip band, the left anywhere down to the lower one."""
    near = abs(co.x) < 0.04 and abs(co.z) < 0.04
    return near and -0.51 < co.y < 0.11


def portrait(ms, rv, actions) -> str:
    """``T_UI_Portrait_Pitwarden`` (512² RGBA, head and shoulders, toon + screen-space ink, the violet eye cards)."""
    import mship
    return mship.portrait_shot(ms, rv, portrait_pose(ms.A), PORTRAIT, eye_glow=EYE_PORTRAIT, yaw=-112.0,
                               bust_below_head=0.46, bust_radius=0.46)


def reach(A, clip_pose: BP, local) -> list:
    """(forward, right, up) cm from the root of an item point in a pose (the VFX offset convention)."""
    p = A.item_point(level(A, clip_pose), "weapon_r", local)
    return [round(-p.y * 100.0, 1), round(-p.x * 100.0, 1), round(p.z * 100.0, 1)]


def qa_setup(lod1: bool = False):
    import mship
    B = build_all(lod1=lod1)
    kit, body = B["kit"], B["body"]
    rig = kit.rig
    A = anims(kit)
    R.attach_to_bone(B["spade"], rig, "weapon_r")
    A.bind_meshes(body, {"weapon_r": B["spade"]})
    cs = clips(A)
    items = [mship.Item(B["spade"], SPADE, "weapon_r", "spade", sockets=B["spade_socks"],
                        extra={"lengthCm": round((TIP_Y - BUTT_Y) * 100.0, 1), "look": "weapon",
                               "note": "monsters.md sub_pitwarden: a long spade (new held item); grip at the upper "
                                       "leather band, 0.88 m above the shod butt"},
                        grip=spade_grip, preview_up="weapon_flat")]
    f = scene.ms_to_frame
    k, P = keys(A), dig_poses(A)
    tip, mid = (0.0, TIP_Y, 0.0), (0.0, BLADE_Y0 + 0.16, 0.0)
    plant_at = reach(A, A.second_hand(P["plant"], DIG_HAND), tip)
    fx = {"eyeGlow": RF.EYE_FX,
          "telegraph": {"normal": "#FFE8DC", "elite": "#D11F2E", "untilMs": A.windup_ms,
                        "note": "dark remake: bone fill / crimson rim (monsters.md §1.2)"},
          "attackTrail": {"color": "#C8C4BC", "alpha": 0.4, "sockets": ["blade_tip", "blade_mid"], "item": SPADE},
          "contactReach": {"attackRangeTiles": 1.8, "bladeTipCm": reach(A, A.second_hand(k["hit"], *TWO_HAND), tip),
                           "bladeMidCm": reach(A, A.second_hand(k["hit"], *TWO_HAND), mid),
                           "note": "Attack01 Contact 620: the blade's edge comes down ~1.4 m ahead of the root at "
                                   "chest height (forward, right, up cm) — the hit point of a hero at the 1.8-tile "
                                   "range"},
          "sweep": {"clip": "Attack02", "skill": "spade_sweep", "windupEndMs": round(WINDUP_OF_CONTACT * SWEEP_CONTACT, 1),
                    "contactMs": SWEEP_CONTACT, "shape": {"kind": "cone", "arcDeg": 100, "radiusTiles": 2.2},
                    "bladeTipCm": reach(A, A.second_hand(sweep_hit(A), *TWO_HAND), tip),
                    "note": "[W3] the blade crosses the cone's centre line at Contact, sweeping from his right to "
                            "his left; the cone decal is the core's telegraph"},
          "dig": {"clip": "Cast01", "skill": "dig", "plantMs": DIG_PLANT, "contactMs": DIG_CONTACT,
                  "shape": {"kind": "line", "lengthTiles": 4, "widthTiles": 1},
                  "plantAtCm": plant_at, "item": SPADE, "socket": "blade_tip",
                  "recipes": {"plant": "notify.FX_PitDig", "collapse": "notify.FX_PitCollapse"},
                  "note": "[W3] Plant: the blade enters the ground at plantAtCm (forward, right, up cm; the tip "
                          "8-10 cm under the surface: the terrain hides it); Contact: the 4-tile line collapses "
                          "(notify.FX_PitCollapse runs dust and clods down the line from the actor, Blow = his "
                          "facing; the crimson line decal, the damage and the 30 % slow are the core's)"}}
    body_nf = mship.parts_except(body, GRIP_PARTS + SASH_PARTS)
    body_ns = mship.parts_except(body, SASH_PARTS)
    contact_f, windup_f = f(CONTACT_MS), f(A.windup_ms)
    weapons = [{"id": "spade", "weight": 1, "attachments": [{"object": SPADE, "socket": "weapon_r"}],
                "idleOrder": "Idle_Order"}]
    ms = mship.MonsterShip(
        asset=ASSET, token=TOKEN, preview_dir=PREVIEW, pal=B["pal"], A=A, body=body, items=items, clips=cs,
        outline_class=UB.OUTLINE_CLASS, game_ids=GAME_IDS, blob_radius=BLOB, height=CROP_H, lod1=B["body1"],
        display=DISPLAY, fx=fx, glows=RF.eye_glows(rig),
        qa_groups={"body_nf": body_nf, "body_ns": body_ns, "sash_tails": ["sash_tail"], **QA_GROUPS},
        qa_pairs={"spadeBody": ("item:" + SPADE, "body_nf"), "sashBody": ("sash_tails", "body_ns"), **QA_PAIRS},
        qa_limits={"ground": -0.5, "spadeBody": 0, "sashBody": 0, **{k_: 0 for k_ in QA_PAIRS}},
        # the dug-in blade stands in the ground (the terrain hides it); the boot stands on its tread
        qa_clip_limits={"Cast01": {"ground": -16.0, "spadeBody": 10 ** 6},
                        "Idle_Order": {"ground": -16.0, "spadeBody": 10 ** 6}},
        outline_px=UB.OUTLINE_PX, outline_px_note=UB.OUTLINE_PX_NOTE, native_sheets=True, portrait=portrait,
        game_keys=[("Idle", 0, "se"), ("Walk", 10, "se"), ("Attack01", windup_f, "se"), ("Attack01", contact_f, "se"),
                   ("Attack02", f(434.0), "se"), ("Attack02", f(SWEEP_CONTACT), "se")],
        contact_keys=[("Cast01", f(DIG_PLANT), "se"), ("Cast01", f(DIG_CONTACT), "se"), ("Attack02", f(SWEEP_CONTACT), "sw"),
                      ("Hurt", 6, "se"), ("Death", 59, "se"), ("Stun", 20, "se")],
        close_keys=[("Attack01", windup_f, "se"), ("Attack01", contact_f, "se"), ("Attack02", f(434.0), "se"),
                    ("Attack02", f(SWEEP_CONTACT), "se"), ("Attack02", f(SWEEP_CONTACT + 130.0), "front"),
                    ("Cast01", f(DIG_PLANT), "se"), ("Cast01", f(560.0), "se"), ("Cast01", f(DIG_CONTACT), "side"),
                    ("Death", 36, "se"), ("Death", 59, "se"), ("Rise", 55, "se"), ("Idle_Order", 114, "front")],
        back_clips=("Attack01", "Attack02", "Cast01", "Death"),
        skeleton_bones_required=("root", "pelvis", "head", "jaw", "eye_l", "weapon_r", "weapon_l", "tabard_f_02",
                                 "tabard_b_02", "ik_foot_l", "ik_hand_gun", "sash_01", "sash_02"),
        extra_previews=mship.w2_sheet([("Idle_Order", f(ORDER_DIG), "se"), ("Idle_Order", f(1900.0), "se"),
                                       ("Rise", 55, "se"), ("Walk", 40, "sw")], "game_w3.png"),
        lod_note="LOD1: the same kit at a collapse ratio iterated to <= 3 k toon (eyes, sockets and mouth built "
                 "exact); the sash keeps its band, knot and tails, the gorget its collar and bib (the rim, the boss "
                 "and the knot's wrap are LOD0 only).",
        extra={"refSpeedCmS": round(REF_SPEED * 100.0, 1), "attackStyle": "overhead", "family": "unburied",
               "role": "subDungeonMiniBoss",
               "marker": {"swatches": list(UB.MARKER), "note": "violet pinpoint eyes (monsters.md §1.2)"},
               "eliteMarker": {"asset": "SM_FX_EliteSigil", "socket": "fx_elite", "gameId": "elite_sigil",
                               "note": "a sub-dungeon mini-boss is elite (affixes rolled with the parent zone's "
                                       "count): the W2 sigil rides fx_elite (elite_sigil.py --solve-elite), the W1 "
                                       "crown fx_overhead; no boss marker of his own (Valdras alone opts out)"},
               "looks": {"rule": "one look: bare cracked scalp (his helm is in the Helm Wall with the others), the "
                                 "sash, the gorget, the spade; no per-instance variants",
                         "head": [{"id": "bare_scalp", "attachments": []}], "weapon": weapons},
               "abilities": {"spade_sweep": {"clip": "Attack02", "contactMs": SWEEP_CONTACT},
                             "dig": {"clip": "Cast01", "plantMs": DIG_PLANT, "contactMs": DIG_CONTACT}},
               "skeletonNote": "SKEL_Human + sash_01..02 under pelvis (merged into the shared skeleton on import: "
                               "the other Unburied leave them unweighted)",
               "notes": "Sub-dungeon mini-boss of the Pit of the Faithful (chapter1.md §9.2, room four). Clips in "
                        "place (play rate = v / ref). Attack01 chops (Contact 620); Attack02 = the spade sweep "
                        "(Contact 700); Cast01 = Dig (Plant 330, Contact 800). Idle_Order is a one-shot idle "
                        "variant (he digs). Death: he hangs on the planted spade, kneels and folds at its foot, the "
                        "fist still on the haft (the spade dissolves with the husk); Rise = Death reversed. Stun "
                        "loops. The sash tails are cloth in the SK (sash_* chain): gravity, flow and gait sway "
                        "baked into every clip."},
    )
    ms.extra["budget"] = UB.budget_record(ms, LOD0_MAX, LOD1_MAX)
    pal = B["pal"]
    ms.lod1_items = {SPADE: lambda: build_spade(pal, 1)[0]}
    return ms


def sweep_hit(A) -> BP:
    """The sweep's Contact pose, for the manifest's reach numbers."""
    return sweep_poses(A)["hit"]


def ship(previews: bool = True, verify: bool = True) -> int:
    UB.march_stage()
    ms = qa_setup(lod1=True)
    return UB.ship_member(ms, ms.lod1_items, previews=previews, verify=verify)


def portrait_quick(out: Path) -> None:
    import mship
    from kit import review
    UB.march_stage()
    ms = qa_setup()
    out.mkdir(parents=True, exist_ok=True)
    rv = mship.MReview(ASSET, ms.A.rig.obj, [ms.body, ms.items[0].obj], CROP_H, blob_radius=BLOB, out_dir=out,
                       game_outline_px=UB.OUTLINE_PX, outline_class=UB.OUTLINE_CLASS, glows=ms.glows)
    del review
    portrait(ms, rv, [])


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    scratch = scratch_root("mq", "pitwarden")
    if "--quick" in args:
        k_ = args.index("--quick")
        quick(Path(args[k_ + 1]) if len(args) > k_ + 1 and not args[k_ + 1].startswith("--") else scratch)
    elif "--anims" in args:
        names_ = [a for a in args[args.index("--anims") + 1:] if not a.startswith("--")]
        fac = next((a.split("=")[1] for a in args if a.startswith("--facing=")), "se")
        anim_quick(scratch / "anims", names_, fac)
    elif "--portrait" in args:
        portrait_quick(scratch / "portrait")
    else:
        sys.exit(ship(previews="--no-previews" not in args, verify="--no-verify" not in args))

"""亡旗手 Dead Standard-Bearer — ``SK_Mon_DeadStandardBearer`` (dark remake monsters.md §2.2, Unburied kit variant;
implementation-plan.md A2).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/dead_standard_bearer.py
        (no flag) ship · --quick [dir] · --no-previews · --no-verify

The priority target and the tallest silhouette of the family: a dead crusade standard-bearer with a sword sheathed at
the left hip and a 3 m banner pole (``SM_MonWeapon_StandardPole``, new held item on ``weapon_r``: an ash-dark pole
shod in iron, a crossbar 1.82 m above the fist, a tarnished open-hand finial). The tattered crimson banner hangs from
the crossbar and is **cloth in the skeletal mesh**: a three-bone chain ``banner_01..03`` parented to ``weapon_r`` (the
skeleton gains it under the weapon bone), turned about the crossbar every frame — it hangs under gravity whatever the
pole does and snaps in a wind it should not have (a looping flutter on the clip clock), the scorched open palm on both
faces.

Behaviour (monsters.md): L7, speed 30 (0.90 m/s), range 1.6, attack interval 1700 — Attack01 brings the standard
crashing down on the target with both hands (Contact 650 ms, WindupEnd 403); [W2] Muster (Cast01: plants the pole,
raises a corpse — ``Plant`` 450 ms, ``Release`` 900 ms; the breach-violet marker ring is core VFX) and the +15 %
damage aura. Idle_Order: dips the standard in salute to a commander who is not there. Death: he kneels and folds at
the foot of the standard, the dead hand still on it — the pole stays planted, the banner still moving; Rise reverses.
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
import numpy as np  # noqa: E402
from mathutils import Matrix, Quaternion, Vector  # noqa: E402

from kit import mesh as M, outline, rig as R, scene, shading  # noqa: E402
from kit.palette import Palette  # noqa: E402
from kit.paths import scratch_root  # noqa: E402
from kit.rig import Bind  # noqa: E402

import common as C  # noqa: E402
from common import BP, K, V  # noqa: E402
import hero_base as HB  # noqa: E402
import mgeo as G  # noqa: E402
import unburied as UB  # noqa: E402
import risen_footman as RF  # noqa: E402
from uclips import UnburiedAnims  # noqa: E402

ASSET = "SK_Mon_DeadStandardBearer"
TOKEN = "Mon_DeadStandardBearer"
PREVIEW = "mon_dead_standard_bearer"
GAME_IDS = ["monster_dead_standard_bearer"]
DISPLAY = {"zh-CN": "亡旗手", "en": "Dead Standard-Bearer"}
POLE = "SM_MonWeapon_StandardPole"
REF_SPEED = 30 * 0.03
CONTACT_MS = 650.0
ATTACK_MS = 1300.0
CAST_MS = 1500.0
PLANT_MS, RELEASE_MS = 450.0, 900.0
WALK = dict(RF.WALK, cycle=1240.0)
HEIGHT = 1.78
LOOK = UB.ULook(hip_sword=True, arrows=((0.70, 158.0, 18.0, 0.30, True), (0.80, 196.0, 6.0, 0.19, True)),
                extra={"swatches": ("standard.cloth", "standard.char"),
                       "lod1_skip": ("banner_trim", "scabbard_chape")})   # LOD1 under 3 k: trims are < 1 px there
BLOB = 0.34
LOD0_MAX, LOD1_MAX = 7500, 3000
CROP_H = 2.7                      # review framing height (game cells): the 3.3 m standard fits a 1080p cell of 2.7 m

# the pole in weapon_r space (the fist at the origin, +Y up the pole, +Z the banner's face normal)
FOOT_Y, CROSS_Y, TOP_Y = -0.85, 1.82, 2.02
POLE_R = 0.021
CROSS_HALF = 0.31
BANNER_Z = 0.036                  # the banner (and its crossbar) hang in front of the pole
BANNER_W, BANNER_L = 0.58, 0.74
BANNER_TOP = CROSS_Y - 0.026
TAILS6 = (0.74, 0.65, 0.80, 0.59, 0.78, 0.67, 0.73)   # tattered hem: tooth tips / notches (left → right)
TAILS = tuple(v for a, b in zip(TAILS6, TAILS6[1:]) for v in (a, 0.5 * (a + b))) + TAILS6[-1:]   # per column (12)
FOLD_A = 0.020                    # depth of the banner's three vertical folds at the hem (m)
BANNER_PARTS = ("banner", "banner_trim", "banner_sigil")
FLUTTER_MS = 620.0                # one snap of the wind
BANNER_MIN_OPEN = math.radians(2.5)   # each link at least this far out to the face side of the pole's plane
FACE = V(-0.35, 1.0, 0.0)         # the banner faces forward-right (the W1 camera's side)


# ── the pole ───────────────────────────────────────────────────────────────────────────────────────────────────
def build_pole(pal: Palette) -> tuple[bpy.types.Object, list]:
    """The banner pole in weapon_r space: an ash-dark shaft (−0.85 … +1.95 m), an iron shoe, a leather grip band,
    the crossbar on a short iron bracket in front of the shaft, a tarnished open-hand finial."""
    b = UB.MBuilder(POLE, pal, prefix="standard")
    b.use("standard.pole", "standard.finial", "unburied.helm_dent", "unburied.leather_dark")
    shaft = M.tube([(0, FOOT_Y + 0.10, 0), (0, 0.6, 0), (0, TOP_Y - 0.06, 0)], [POLE_R, POLE_R, POLE_R * 0.92],
                   sides=8, dome0=None, dome1=0.006)
    sid = b.add(shaft, "pole", name="pole", hull_pad=True)
    shoe = M.tube([(0, FOOT_Y, 0), (0, FOOT_Y + 0.05, 0), (0, FOOT_Y + 0.12, 0)], [0.012, POLE_R + 0.003,
                                                                                    POLE_R + 0.002],
                  sides=8, dome0=0.004, dome1=None)
    b.add(shoe, "unburied.helm_dent", name="pole_shoe", hull_pad=True)
    band = M.tube([(0, -0.12, 0), (0, 0.10, 0)], [POLE_R + 0.002] * 2, sides=8, cap0=None, cap1=None)
    b.add(band, "unburied.leather_dark", name="pole_grip", outline=False, covered_by=sid)
    # the crossbar on its bracket (in front of the shaft: the banner hangs clear of the pole)
    bracket = M.box((0.016, 0.03, BANNER_Z + 0.01), (0, CROSS_Y, BANNER_Z / 2), bevel=0.0)
    b.add(bracket, "unburied.helm_dent", name="pole_bracket", outline=False, covered_by=sid)
    bar = M.tube([(-CROSS_HALF, CROSS_Y, BANNER_Z), (0, CROSS_Y + 0.008, BANNER_Z), (CROSS_HALF, CROSS_Y, BANNER_Z)],
                 [0.013, 0.014, 0.013], sides=6, dome0=0.006, dome1=0.006)
    b.add(bar, "pole", name="crossbar", hull_pad=True)
    for sg in (1.0, -1.0):
        knob = HB.ell((0.020, 0.020, 0.020), (sg * (CROSS_HALF + 0.012), CROSS_Y, BANNER_Z), sides=6, rings=4)
        b.add(knob, "finial", name="crossbar_knob", hull_pad=True)
    # the finial: a flat open hand of tarnished brass on a collar, palm toward the banner's face
    collar = M.tube([(0, TOP_Y - 0.08, 0), (0, TOP_Y - 0.02, 0)], [POLE_R + 0.006, POLE_R + 0.004], sides=8,
                    dome0=0.003, dome1=0.003)
    b.add(collar, "finial", name="finial_collar", hull_pad=True)
    palm = M.box((0.085, 0.080, 0.014), (0, TOP_Y + 0.030, 0), bevel=0.004)
    pid = b.add(palm, "finial", name="finial", hull_pad=True)
    for k, (dx, ln) in enumerate(((-0.030, 0.060), (-0.010, 0.074), (0.010, 0.070), (0.030, 0.056))):
        f = M.box((0.016, ln, 0.012), (dx, TOP_Y + 0.070 + ln / 2, 0), bevel=0.003)
        b.add(f, "finial", name="finial", hull_pad=True)
    th = M.box((0.016, 0.050, 0.012), (0, 0, 0), bevel=0.003)
    th.transform(Matrix.Translation((0.050, TOP_Y + 0.040, 0)) @ Matrix.Rotation(math.radians(-40.0), 4, "Z"))
    b.add(th, "finial", name="finial", hull_pad=True)
    del pid
    obj = UB._finish_item(b, pal, POLE)
    socks = [{"name": "finial", "pos": (0.0, TOP_Y + 0.10, 0.0)},
             {"name": "crossbar_l", "pos": (CROSS_HALF, CROSS_Y, BANNER_Z)},
             {"name": "crossbar_r", "pos": (-CROSS_HALF, CROSS_Y, BANNER_Z)},
             {"name": "mid", "pos": (0.0, 0.70, 0.0)},
             {"name": "foot", "pos": (0.0, FOOT_Y, 0.0)}]
    return obj, socks


# ── the banner (cloth in the skeletal mesh) ────────────────────────────────────────────────────────────────────
def item_to_rest(G: Vector):
    """weapon_r space → the armature's rest space (the rest weapon bone: head at the grip, +Y forward = −Y world,
    +Z up, +X = −X world)."""
    return lambda x, y, z: Vector((G.x - x, G.y - y, G.z + z))


def banner_chain(G: Vector) -> list:
    w = item_to_rest(G)
    ys = [BANNER_TOP - BANNER_L * k / 3.0 for k in range(4)]
    return [("banner", "weapon_r", [w(0.0, y, BANNER_Z) for y in ys])]


def _smooth(x, a, b):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def fold(u: float, r: float) -> float:
    """Face-side offset of the banner cloth at (u across 0..1, r down 0..1): three vertical folds — a ridge down
    the middle (over the pole, the sigil on its crest) between two troughs — flat where the crossbar pins it and
    deepening toward the hem. Folds, not a gentle ripple: the toon light / shade bands then break along fold lines
    instead of drifting across the panel as a soft blob when the terminator crosses it."""
    return -FOLD_A * math.sin(3.0 * math.pi * u) * r ** 0.8


def add_banner(kit, b) -> None:
    """The banner panel (two-sided, charred edges, three folds, tattered tails), the tarnished trim down its sides
    and along the top, the scorched open palm on both faces; weighted along ``banner_01..03``. The field is the
    appended ``standard.banner`` (light band .25) on the crimson-cloth slot (``UB.CLOTH_MATERIAL``: the rim
    overridden): art review r2 — the cream rim on the .30 light band washed the banner a pale salmon (#BE8E7B,
    L* 63) when he faced SW / NE."""
    n0 = len(b.parts)
    b.use_appended("standard.banner")
    w = item_to_rest(kit.grip_r)
    lod = getattr(kit, "lod", 0)
    top, L = w(0.0, BANNER_TOP, BANNER_Z), BANNER_L
    bones = ("banner_01", "banner_02", "banner_03")
    d = (w(0.0, BANNER_TOP - L, BANNER_Z) - top)
    L2 = d.length_squared

    def fn(co):
        t = np.clip(((co - np.array(top)) @ np.array(d)) / L2, 0.0, 1.3)
        w3 = _smooth(t, 0.58, 0.86)
        w1 = 1.0 - _smooth(t, 0.20, 0.48)
        w2 = np.clip(1.0 - w1 - w3, 0.0, 1.0)
        return np.stack([w1, w2, w3], axis=1)
    bind = Bind.custom(bones, fn)
    cols, rows = len(TAILS) - 1, 5
    th = 0.005

    def grid(off):
        out = []
        for r in range(rows + 1):
            row = []
            for c in range(cols + 1):
                u = c / cols
                x = -BANNER_W / 2 + BANNER_W * u
                tail = TAILS[c]
                y = BANNER_TOP - tail * r / rows
                row.append(w(x, y, BANNER_Z + fold(u, r / rows) + off))
            out.append(row)
        return out
    part = C.thick_patch(grid(th / 2), grid(-th / 2), sub_tags=True)
    pid = b.add(part, "standard.banner", bind, name="banner",
                sub_regions={1: "standard.banner", 2: "standard.banner", 0: "standard.char"}, hull_pad=True)

    def rip(x: float, y: float) -> float:
        """The panel's folds (as ``grid``) at item (x, y): the trim and the sigil ride the cloth exactly."""
        u = min(1.0, max(0.0, (x + BANNER_W / 2) / BANNER_W))
        tail = float(np.interp(u, np.linspace(0.0, 1.0, cols + 1), TAILS))
        return fold(u, min(1.0, max(0.0, (BANNER_TOP - y) / tail)))

    for sg in (1.0, -1.0):
        face = _BannerFace(w, rip, sg, th / 2 + 0.0012)
        # tarnished trim down both sides and along the top (front and back)
        for c in (0, cols):
            x = -BANNER_W / 2 + BANNER_W * c / cols
            pts = [face.point(BANNER_TOP - TAILS[c] * k, x * 0.985) for k in (0.02, 0.25, 0.5, 0.72, 0.92)]
            b.add(M.tube(pts, [0.0055] * len(pts), sides=3, dome0=0.0, dome1=0.0), "trim", bind,
                  name="banner_trim", outline=False, covered_by=pid)
        pts = [face.point(BANNER_TOP - 0.012, x) for x in (-0.27, -0.09, 0.09, 0.27)]
        b.add(M.tube(pts, [0.009] * len(pts), sides=3, dome0=0.0, dome1=0.0), "trim", bind, name="banner_trim",
              outline=False, covered_by=pid)
        if lod:
            continue
        # the scorched open palm (the crusade's sigil, burnt black) painted on the cloth, a little above the middle
        face = _BannerFace(w, rip, sg, th / 2 + 0.0016)
        yc = BANNER_TOP - 0.38
        b.add(G.surface_patch(face, yc - 0.055, yc + 0.05, -0.06, 0.06, 0.0, nu=3, na=4,
                              shape=lambda tu, ta: 0.82 + 0.18 * math.sin(math.pi * tu)),
              "standard.char", bind, name="banner_sigil", outline=False, covered_by=pid)
        for dx, ln in ((-0.045, 0.080), (-0.015, 0.100), (0.015, 0.094), (0.045, 0.074)):
            b.add(G.surface_paint(face, [(yc + 0.04, dx), (yc + 0.04 + ln * 0.5, dx * 1.05),
                                         (yc + 0.04 + ln, dx * 1.1)], 0.022, 0.0, steps=3, taper=0.25),
                  "standard.char", bind, name="banner_sigil", outline=False, covered_by=pid)
        b.add(G.surface_paint(face, [(yc - 0.03, -0.055), (yc + 0.0, -0.085), (yc + 0.04, -0.105)], 0.022, 0.0,
                              steps=3, taper=0.3), "standard.char", bind, name="banner_sigil", outline=False,
              covered_by=pid)
    kit.cloth_ids.update(range(n0, len(b.parts)))


class _BannerFace:
    """``mgeo`` surface adapter for one face of the banner (u = item y down the cloth, a = item x across):
    the rippled panel ``lift`` metres off its mid-plane on side ``sg``, in the armature's rest space."""

    def __init__(self, w, rip, sg: float, lift: float):
        self.w, self.rip, self.sg, self.lift = w, rip, sg, lift

    def point(self, u, a, out=0.0):
        return self.w(a, u, BANNER_Z + self.rip(a, u) + self.sg * (self.lift + out))

    def normal(self, u, a):
        return Vector((0.0, 0.0, self.sg))


# ── poses ──────────────────────────────────────────────────────────────────────────────────────────────────────
def pole_dir(lat: float, fwd: float, up: float, face=None):
    """Pole direction (+Y) and the banner's face normal (item +Z) as close to ``face`` as the direction allows."""
    d = V(lat, fwd, up).normalized()
    f = Vector(face if face is not None else FACE)
    f = f - d * f.dot(d)
    if f.length < 1e-4:
        f = V(0.0, 0.0, 1.0) - d * d.z
    return {"wpn": d, "wpn_up": f.normalized()}


def level(A, bp: BP) -> BP:
    """Roll the right fist about the pole so the banner faces ``wpn_up`` (the hammer grip would roll it with the
    forearm); the short way round (keys interpolate the roll)."""
    m = A.mats(bp)["weapon_r"].to_3x3()
    d = Vector(bp.wpn).normalized()
    z = m @ Vector((0, 0, 1))
    zp = (z - d * z.dot(d)).normalized()
    want = Vector(bp.wpn_up)
    wp_ = (want - d * want.dot(d)).normalized()
    ang = math.degrees(math.atan2(d.dot(zp.cross(wp_)), zp.dot(wp_)))
    p, r, y = bp.wrist_r
    return bp.but(wrist_r=(p, (r + ang + 180.0) % 360.0 - 180.0, y))


def ready(A: UnburiedAnims) -> BP:
    """READY: hunched like the footman, the standard borne upright at the right side (the fist at the lowest rib, the
    iron shoe a forearm above the ground), the banner high over the helm — clear of the face at the W1 camera — and
    facing the camera's side; the left hand hangs."""
    J = A.J
    return BP(pelvis=V(0.0, -0.02, -0.045), lean=11.0, hip_pitch=3.0, side=-2.0, twist=-4.0, hip_yaw=-3.0,
              head_pitch=-34.0, head_roll=15.0, head_yaw=-6.0,
              foot_l=J["ankle_l"] + V(0.02, 0.06, 0.0), foot_r=J["ankle_r"] + V(-0.04, -0.06, 0.0),
              foot_l_rot=(0.0, 0.0, 8.0), foot_r_rot=(14.0, 0.0, -14.0), knee_l=V(0.25, 1, 0), knee_r=V(-0.25, 1, 0),
              hand_r=V(-0.30, 0.13, 1.10), elbow_r=V(-0.9, -0.2, -0.5), **pole_dir(0.0, -0.04, 1.0),
              hand_l=V(0.27, 0.08, 0.80), elbow_l=V(0.8, -0.2, -0.4), clav_l=(-8.0, 0.0), clav_r=(4.0, 0.0),
              flow=0.1)


TWO_HAND = (-0.40, 0.0, -0.035)   # the left fist lower on the pole in Attack01
# Attack01 at Contact (650 ms, measured on the baked clip; forward / up from the root, cm): the crossbar lands on the
# head of a hero at the 1.6-tile range; the banner hangs from it down his back (manifest fx.contactReach)
CONTACT_REACH = {"crossbarCm": [186, 191], "bannerForwardCm": [166, 192], "bannerUpCm": [116, 208],
                 "attackRangeTiles": 1.6,
                 "note": "Attack01 Contact: the crossbar (sockets crossbar_l / crossbar_r) comes down 1.86 m in front "
                         "of the root at 1.91 m — the crown of a hero standing at the 1.6-tile range — with the banner "
                         "draped behind him; the shaft runs from the fists (0.22 m out, 1.1 m up) to it. Place the hit "
                         "and the telegraph on the crossbar point, not further out"}
DEATH_ROLL = 22.0                 # head roll added in the death fold (away from the raised right arm)


def keys(A: UnburiedAnims) -> dict:
    """The standard brought crashing down: both fists on the pole, it is heaved back over the shoulder (the banner
    swinging behind), then toppled forward like a felled tree onto the target — the crossbar comes down on its head
    at Contact (``CONTACT_REACH``: ≈ 1.85 m out at the 1.6-tile range, the banner draped behind it) — and hauled
    upright again."""
    R0 = A.ready()
    J = A.J
    wind = R0.but(lean=3.0, twist=8.0, hip_yaw=4.0, head_pitch=-26.0, hand_r=V(-0.30, 0.16, 1.22),
                  elbow_r=V(-0.9, 0.0, -0.4), **pole_dir(0.05, -0.30, 1.0), elbow_l=V(0.6, 0.1, -0.8),
                  clav_r=(10.0, 0.0), flow=0.25)
    hold = wind.but(hand_r=V(-0.30, 0.12, 1.30), **pole_dir(0.06, -0.42, 1.0), lean=1.0)
    # Contact (art review r2): the topple stops with the pole 26° above level and the fists drawn in (fwd .26), so
    # the crossbar comes down on a hero at his 1.6-tile range — crossbar ≈ 1.85 m out, ≈ 1.9 m up (the crown of the
    # target's head), the banner draped down his back — instead of 2.2 m out with the shaft through him
    hit = R0.but(pelvis=R0.pelvis + V(0, 0.10, -0.06), lean=24.0, hip_pitch=6.0, twist=-6.0, hip_yaw=-6.0,
                 head_pitch=-46.0, hand_r=V(-0.28, 0.22, 1.12), elbow_r=V(-0.9, -0.4, -0.2),
                 **pole_dir(0.10, 1.0, 0.49), elbow_l=V(0.6, -0.2, -0.8), foot_l=J["ankle_l"] + V(0.03, 0.30, 0.0),
                 clav_r=(4.0, 6.0), flow=0.45)
    follow = hit.but(hand_r=V(-0.28, 0.23, 1.08), **pole_dir(0.10, 1.0, 0.41), lean=26.0)
    recover = R0.but(pelvis=R0.pelvis + V(0, 0.05, -0.04), lean=15.0, hand_r=V(-0.28, 0.24, 1.06),
                     **pole_dir(-0.04, 0.45, 1.0), foot_l=J["ankle_l"] + V(0.03, 0.16, 0.0))
    upright = pole_dir(-0.03, 0.05, 1.0)
    return dict(wind=wind, hold=hold, hit=hit, follow=follow, recover=recover, lift_ms=260.0, follow_ms=140.0,
                recover_ms=330.0,
                # dying at the foot of the standard: the pole stays planted, the dead hand on it
                buckle_hand_r=V(-0.31, 0.16, 0.90), buckle_elbow_r=V(-0.9, -0.2, -0.3),
                **{"buckle_" + k: v for k, v in upright.items()},
                kneel_hand_r=V(-0.38, 0.26, 0.86), kneel_elbow_r=V(-0.9, 0.0, -0.4),
                **{"kneel_" + k: v for k, v in upright.items()},
                fold_hand_r=V(-0.42, 0.34, 0.86), fold_elbow_r=V(-0.9, 0.0, -0.4),
                **{"fold_" + k: v for k, v in pole_dir(-0.03, 0.08, 1.0).items()},
                death_fix=lambda bp: level(A, bp))


def idle_order(A: UnburiedAnims):
    """Idle_Order 2400 ms: dips the standard in salute to a commander who is not there — the pole lowered forward
    to 50°, the head bowed, held, then hauled upright with a snap of the banner."""
    R0 = A.ready()
    dip = R0.but(lean=17.0, head_pitch=-14.0, head_roll=8.0, hand_r=V(-0.27, 0.30, 1.04), elbow_r=V(-0.9, -0.1, -0.3),
                 **pole_dir(-0.08, 0.80, 0.62), flow=0.2)
    low = dip.but(**pole_dir(-0.08, 0.86, 0.52), head_pitch=-6.0)
    keys_ = [K(0.0, R0), K(650 / 2400, dip, "out"), K(1200 / 2400, low, "smooth"), K(1650 / 2400, dip, "smooth"),
             K(2050 / 2400, R0.but(head_pitch=R0.head_pitch - 6.0), "out"), K(1.0, R0, "smooth")]
    return A.one_shot("Idle_Order", 2400.0, keys_, notifies={"FX_BannerSnap": 2050.0},
                      usage="idle variant (monsters.md 'they keep their last order'): dips the standard in salute; "
                            "one-shot from READY back to READY")


def cast(A: UnburiedAnims):
    """Cast01 1500 ms (Muster): lifts the standard and drives its iron shoe into the ground (``Plant``), throws the
    left hand up, palm open, the head back (``Release``: the raise), holds, and settles."""
    R0 = A.ready()
    lift = R0.but(lean=6.0, head_pitch=-30.0, hand_r=V(-0.31, 0.10, 1.24), elbow_r=V(-0.9, -0.1, -0.4),
                  **pole_dir(0.0, -0.02, 1.0), hand_l=V(0.25, 0.14, 0.92), flow=0.25)
    # planted at his right side, the top tipped back a little: the banner stands behind the raised face
    plant = R0.but(pelvis=R0.pelvis + V(0, 0.02, -0.06), lean=16.0, head_pitch=-36.0, hand_r=V(-0.31, 0.04, 0.87),
                   elbow_r=V(-0.9, -0.2, -0.3), **pole_dir(0.0, -0.08, 1.0), hand_l=V(0.25, 0.16, 0.84),
                   foot_r=A.J["ankle_r"] + V(-0.05, 0.02, 0.0), flow=0.35)
    call = plant.but(pelvis=R0.pelvis + V(0, 0.0, -0.04), lean=4.0, head_pitch=-50.0, head_roll=8.0,
                     hand_l=V(0.20, 0.40, 1.62), elbow_l=V(0.9, -0.2, -0.3), clav_l=(16.0, 4.0), flow=0.4)
    held = call.but(hand_l=V(0.21, 0.42, 1.66), head_pitch=-53.0)
    keys_ = [K(0.0, R0), K(250 / CAST_MS, lift, "out"), K(PLANT_MS / CAST_MS, plant, "in"),
             K(720 / CAST_MS, call, "out"), K(RELEASE_MS / CAST_MS, held, "smooth"),
             K(1180 / CAST_MS, plant.but(hand_l=V(0.25, 0.20, 1.00)), "smooth"), K(1.0, R0, "smooth")]
    c = A.one_shot("Cast01", CAST_MS, keys_, notifies={"Plant": PLANT_MS, "Release": RELEASE_MS},
                   usage="[W2] Muster: plants the pole and raises a corpse within 6 tiles (the breach-violet marker "
                         "ring, 900 ms, is core VFX at Release)")
    c.key_poses_ms = [0.0, 250.0, PLANT_MS, 720.0, RELEASE_MS, 1180.0, CAST_MS]
    return c


class StandardAnims(UnburiedAnims):
    """Every frame: the banner turned to face its side (``level``), the left fist on the pole in Attack01 (blended
    in and out at the clip's ends), the banner chain solved in ``banner_cloth``."""
    two_hand = TWO_HAND
    two_hand_clips = ("Attack01",)
    HAND_IN, HAND_OUT = 140.0, 260.0

    def death_keys(self) -> list:
        """The kit's fold with the broken-neck tilt deepened (``DEATH_ROLL``): the right fist stays up on the planted
        pole, so the head (and the kettle helm's brim) lolls over to the side away from that arm."""
        rec, buckle, kneel, fold = super().death_keys()
        r0 = self.ready().head_roll
        return [rec, buckle, kneel.but(head_roll=r0 + 0.6 * DEATH_ROLL), fold.but(head_roll=r0 + DEATH_ROLL)]

    def body_clip(self, name, length, sampler, loop=False, notifies=None, ref_speed=None, post=None):
        inner = post if post is not None else self.plant

        def post2(bp, t, _inner=inner):
            bp = level(self, bp)
            if name in self.two_hand_clips:
                two = self.second_hand(bp, *self.two_hand)
                wgt = min(1.0, t / self.HAND_IN, (length - t) / self.HAND_OUT)
                wgt = max(0.0, wgt)
                wgt = wgt * wgt * (3.0 - 2.0 * wgt)
                bp = bp.but(hand_l=Vector(bp.hand_l).lerp(Vector(two.hand_l), wgt),
                            elbow_l=two.elbow_l if wgt > 0.5 else bp.elbow_l)
            return _inner(bp, t)
        return HB.hero_clip(self.rig, name, length, sampler, cloth=self.cloth, loop=loop, notifies=notifies,
                            ref_speed=ref_speed, post=post2, grips=self.grips)


_REST = {}


def banner_cloth(p, bp: BP, A) -> None:
    """The banner chain: each link turned about the crossbar axis (its only freedom) — toward world-down by gravity
    (the pole may tilt; the panel hangs), then a wind it should not have: a travelling flutter on the clip clock
    (whole cycles per loop) that streams the hem back from the face."""
    rig = A.rig
    names = ("banner_01", "banner_02", "banner_03")
    if not _REST.get(id(rig)):
        bones = rig.obj.data.bones
        _REST[id(rig)] = {n: (bones[n].matrix_local.to_3x3().copy()) for n in names}
        _REST[id(rig)]["weapon_r"] = bones["weapon_r"].matrix_local.to_3x3().copy()
    rest = _REST[id(rig)]
    dq = p.abs.get("weapon_r", Quaternion())
    wr = dq.to_matrix() @ rest["weapon_r"]
    xw = (wr @ Vector((1, 0, 0))).normalized()           # the crossbar axis
    hang = -(wr @ Vector((0, 1, 0))).normalized()        # down the pole
    down = Vector((0, 0, -1))
    dproj = down - xw * down.dot(xw)
    g = 0.0
    if dproj.length > 1e-4:
        dproj.normalize()
        g = math.atan2(xw.dot(hang.cross(dproj)), hang.dot(dproj))
    clk = C.ClothClock
    t, L = (clk.t, clk.length) if clk.sampler is not None and clk.length > 0 else (0.0, FLUTTER_MS)
    n = max(1, round(L / FLUTTER_MS))
    ph = 2.0 * math.pi * n * t / L
    face = (wr @ Vector((0, 0, 1))).normalized()
    # the wind streams the hem out to the face side (the banner never wraps the pole behind it)
    sgn = 1.0 if face.dot(xw.cross(hang)) >= 0.0 else -1.0
    amp = 0.6 + 0.4 * min(1.0, bp.flow * 2.0)
    for k, nm in enumerate(names):
        wave = math.radians(9.0 + 3.0 * k) * amp * math.sin(ph - 1.1 * k)
        lean = math.radians(7.0 + 5.0 * k)
        acc_k = g + sgn * (lean + wave)
        # the panel hangs in front of the pole: when gravity (a pole tipped toward the face side) or the wind would
        # swing it back through the shaft, it rests against the pole instead (never behind the pole's plane)
        acc_k = sgn * max(sgn * acc_k, BANNER_MIN_OPEN)
        q = Quaternion(xw, acc_k)
        rmat = q.to_matrix() @ dq.to_matrix() @ rest[nm]
        p.aim(nm, rmat @ Vector((0, 1, 0)), rmat @ Vector((0, 0, 1)))


def anims(kit) -> UnburiedAnims:
    return StandardAnims(kit, REF_SPEED, WALK, ready, CONTACT_MS, ATTACK_MS, keys, extra_cloth=banner_cloth)


def clips(A: UnburiedAnims) -> list:
    return [A.idle(), idle_order(A), A.walk(), A.attack(), cast(A), A.hurt(), A.death(), A.rise(), A.stun()]


# ── build ──────────────────────────────────────────────────────────────────────────────────────────────────────
def make_kit():
    """The kit with the banner chain under ``weapon_r``: the chain's rest points need the rest grip, so a plain kit
    is built first to read it."""
    scene.reset()
    G = UB.UnburiedKit(ASSET, LOOK, height=HEIGHT).J["grip_r"].copy()
    scene.reset()
    kit = UB.UnburiedKit(ASSET, LOOK, height=HEIGHT, extra_chains=lambda k: banner_chain(G))
    kit.grip_r = G
    return kit


def mship_toon(obj) -> int:
    from mship import toon_tris
    return toon_tris(obj)                # every toon-lit slot (the crimson-cloth slot included)


def build_lod1(kit, pal):
    ratio = UB.LOD1_RATIO
    body1 = kit.build(pal, lod=1, ratio=ratio, hull_ratio=0.75, extra=add_banner)
    n = mship_toon(body1)
    for _ in range(6):
        if n <= LOD1_MAX:
            break
        me = body1.data
        bpy.data.objects.remove(body1, do_unlink=True)
        bpy.data.meshes.remove(me)
        ratio *= (LOD1_MAX - 120) / n
        body1 = kit.build(pal, lod=1, ratio=ratio, hull_ratio=0.75, extra=add_banner)
        n = mship_toon(body1)
    print(f"[lod1] {kit.asset}: {n} toon tris at ratio {ratio:.3f}")
    return body1


def build_all(lod1: bool = False):
    kit = make_kit()
    pal = Palette(UB.FAMILY)
    body = kit.build(pal, extra=add_banner)
    body1 = build_lod1(kit, pal) if lod1 else None
    helm = UB.build_kettle_helm(pal, kit)
    hood = UB.build_hood(pal, kit)
    pole, socks = build_pole(pal)
    return dict(pal=pal, kit=kit, body=body, body1=body1, helm=helm, hood=hood, pole=pole, pole_socks=socks)


def dev_items(B, key: str) -> list:
    head = {"hood": B["hood"], "bare": None}.get(key, B["helm"])
    return [(B["pole"], "weapon_r")] + ([(head, "head")] if head is not None else [])


def quick(out: Path) -> None:
    B = build_all()
    A = anims(B["kit"])
    rig = B["kit"].rig
    R.attach_to_bone(B["pole"], rig, "weapon_r")
    R.attach_to_bone(B["helm"], rig, "head")
    A.bind_meshes(B["body"], {"weapon_r": B["pole"]})
    UB.dev_review(ASSET, rig, [B["body"], B["pole"], B["helm"]], A, out, 3.0, glows=RF.eye_glows(rig), blob=BLOB)


# ── ship ───────────────────────────────────────────────────────────────────────────────────────────────────────
GRIP_PARTS = ("fist_r", "thumb_r", "knuckle", "wrist_r", "hand_l", "wrist_l", "fist_l", "thumb_l")
QA_GROUPS = dict(RF.QA_GROUPS)
QA_PAIRS = dict(RF.QA_PAIRS)


def pole_grip(co) -> bool:
    """Pole faces in the fists: the right round the grip band, the left lower on the shaft (Attack01)."""
    return (-0.10 < co.y < 0.10 and abs(co.x) < 0.04 and abs(co.z) < 0.04) or \
        (TWO_HAND[0] - 0.09 < co.y < TWO_HAND[0] + 0.09 and abs(co.x) < 0.04 and abs(co.z) < 0.04)


def qa_setup(lod1: bool = False):
    import mship
    B = build_all(lod1=lod1)
    kit, body = B["kit"], B["body"]
    rig = kit.rig
    A = anims(kit)
    R.attach_to_bone(B["pole"], rig, "weapon_r")
    R.attach_to_bone(B["helm"], rig, "head")
    A.bind_meshes(body, {"weapon_r": B["pole"]})
    cs = clips(A)
    items = [mship.Item(B["pole"], POLE, "weapon_r", "standard", sockets=B["pole_socks"],
                        extra={"lengthCm": round((TOP_Y + 0.17 - FOOT_Y) * 100.0, 1),
                               "note": "monsters.md §2.2: new held item SM_MonWeapon_StandardPole; the banner itself is "
                                       "cloth in the skeletal mesh (banner_01..03 under weapon_r)"},
                        grip=pole_grip, preview_up="weapon_flat"),
             UB.head_item(B["helm"], UB.HELM, None),
             UB.head_item(B["hood"], UB.HOOD, "look_hood")]
    contact_f = scene.ms_to_frame(CONTACT_MS)
    windup_f = scene.ms_to_frame(A.windup_ms)
    release_f = scene.ms_to_frame(RELEASE_MS)
    body_nf = mship.parts_except(body, GRIP_PARTS + BANNER_PARTS)
    body_nb = mship.parts_except(body, BANNER_PARTS)
    body_nohead = mship.parts_except(body, UB.HEAD_PARTS + ("neck",) + BANNER_PARTS)
    weapons = [{"id": "standard", "weight": 1, "attachments": [{"object": POLE, "socket": "weapon_r"}],
                "idleOrder": "Idle_Order"}]
    ms = mship.MonsterShip(
        asset=ASSET, token=TOKEN, preview_dir=PREVIEW, pal=B["pal"], A=A, body=body, items=items, clips=cs,
        outline_class=UB.OUTLINE_CLASS, game_ids=GAME_IDS, blob_radius=BLOB, height=CROP_H, lod1=B["body1"],
        display=DISPLAY, fx={"eyeGlow": RF.EYE_FX,
                             "telegraph": {"normal": "#FFE8DC", "elite": "#D11F2E", "untilMs": A.windup_ms,
                                           "note": "dark remake: bone fill / crimson rim (monsters.md §1.2)"},
                             "attackTrail": {"color": "#C8C4BC", "alpha": 0.35, "sockets": ["finial", "crossbar_r"],
                                             "item": POLE},
                             "contactReach": CONTACT_REACH,
                             "muster": {"clip": "Cast01", "socket": "foot", "item": POLE, "plantMs": PLANT_MS,
                                        "releaseMs": RELEASE_MS,
                                        "note": "[W2] Muster: the breach-violet marker ring (900 ms) and the raise "
                                                "are core VFX; the crimson aura halo is core data"}},
        glows=RF.eye_glows(rig),
        qa_groups={"body_nf": body_nf, "body_nb": body_nb, "body_nohead": body_nohead, "banner": list(BANNER_PARTS),
                   **QA_GROUPS},
        qa_pairs={"poleBody": ("item:" + POLE, "body_nf"), "bannerBody": ("banner", "body_nb"),
                  "poleBanner": ("item:" + POLE, "banner"),
                  "helmBody": ("item:" + UB.HELM, "body_nohead"), "hoodBody": ("item:" + UB.HOOD, "body_nohead"),
                  "helmBanner": ("item:" + UB.HELM, "banner"), **QA_PAIRS},
        qa_limits={"ground": -0.5, "poleBody": 0, "bannerBody": 0, "poleBanner": 0, "helmBody": 0, "hoodBody": 0,
                   "helmBanner": 0, **{k: 0 for k in QA_PAIRS}},
        outline_px=UB.OUTLINE_PX, outline_px_note=UB.OUTLINE_PX_NOTE, native_sheets=True,
        # the standard's big 1080p cells: two native (tiled) sheets of six instead of one downscaled sheet of twelve
        game_keys=[("Idle", 0, "se"), ("Walk", 10, "se"), ("Attack01", windup_f, "se"), ("Attack01", contact_f, "se"),
                   ("Attack01", contact_f, "sw"), ("Cast01", release_f, "se")],
        contact_keys=[("Hurt", 6, "se"), ("Death", 54, "se"), ("Rise", 50, "se"), ("Stun", 20, "se"),
                      ("Idle_Order", 72, "se"), ("Walk", 40, "sw")],
        close_keys=[("Attack01", windup_f, "se"), ("Attack01", contact_f, "se"), ("Attack01", contact_f, "side"),
                    ("Cast01", scene.ms_to_frame(PLANT_MS), "se"), ("Cast01", release_f, "se"), ("Hurt", 6, "se"),
                    ("Death", 31, "se"), ("Death", 54, "se"), ("Rise", 50, "se"), ("Idle_Order", 72, "front")],
        back_clips=("Attack01", "Cast01", "Death"),
        skeleton_bones_required=("root", "pelvis", "head", "jaw", "eye_l", "weapon_r", "weapon_l", "tabard_f_02",
                                 "tabard_b_02", "ik_foot_l", "ik_hand_gun", "banner_01", "banner_03"),
        extra_previews=UB.looks_preview([("helm", [], []), ("bare scalp", [], [B["helm"]]),
                                         ("hood", [B["hood"]], [B["helm"]])]),
        variant_previews=("looks.png",),
        lod_note="LOD1: the same kit at a collapse ratio iterated to <= 3 k toon (eyes, sockets and mouth built exact); "
                 "the banner keeps its panel and trim, the painted palm is LOD0 only.",
        extra={"refSpeedCmS": round(REF_SPEED * 100.0, 1), "attackStyle": "overhead", "family": "unburied",
               "marker": {"swatches": list(UB.MARKER), "note": "violet pinpoint eyes (monsters.md §1.2)"},
               "looks": UB.looks_manifest(weapons, "standard"),
               "skeletonNote": "SKEL_Human + banner_01..03 under weapon_r (merged into the shared skeleton on import: "
                               "the other Unburied leave them unweighted)",
               "notes": "Summoner / aura champion: the tallest silhouette (3.3 m to the finial). The banner is cloth "
                        "in the SK (banner_* chain): gravity + a looping flutter baked into every clip. Attack01 topples "
                        "the standard onto the target (Contact 650). Cast01 = Muster (Plant 450, Release 900). "
                        "Idle_Order is a one-shot idle variant (dips the standard). Death: kneels and folds at the foot "
                        "of the planted standard, the hand still on it; Rise = Death reversed. Stun loops."},
    )
    ms.extra["budget"] = UB.budget_record(ms, LOD0_MAX, LOD1_MAX)
    pal = B["pal"]
    ms.lod1_items = {UB.HELM: lambda: UB.build_kettle_helm(pal, kit, 1), UB.HOOD: lambda: UB.build_hood(pal, kit, 1)}
    return ms


def ship(previews: bool = True, verify: bool = True) -> int:
    UB.march_stage()
    ms = qa_setup(lod1=True)
    return UB.ship_member(ms, ms.lod1_items, previews=previews, verify=verify)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    scratch = scratch_root("mq", "dead_standard_bearer")
    if "--quick" in args:
        k = args.index("--quick")
        quick(Path(args[k + 1]) if len(args) > k + 1 and not args[k + 1].startswith("--") else scratch)
    else:
        sys.exit(ship(previews="--no-previews" not in args, verify="--no-verify" not in args))

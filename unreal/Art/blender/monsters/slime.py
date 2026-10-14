"""绿色史莱姆 Green Slime — ``SK_Mon_Slime`` on ``SKEL_Slime`` (spec art-inventory-ch1.md §4.1, §2.4, §10; R2, R12).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/slime.py
        [--quick] [--anims Clip …] [--no-previews] [--no-verify]

A glossy jiggling jelly dome (0.43 m tall × 0.88 m wide, round footprint) with a dark jelly core, a half-digested
bone and rising bubbles inside, big glaring eyes, a pseudopod for the slam.

* **Look (R12, spec §14 Q14: inner meshes + dithered shell)**: the shell is its own material slot
  ``MI_AF_Toon_Slime`` (parent ``M_AF_Toon_Slime``: masked, Bayer-dithered ``BodyOpacity`` **0.62** + the web jelly
  rim rgba(230,255,220,.6)); the core, bone, bubbles, eyes, brows, the sheen and an **inner backing bowl** are opaque
  palette toon (slot 0); one ink hull round the whole jelly (slot 1). The previews render the TAA-resolved dither
  (the shell mixed with what lies behind it, back faces culled like UE). At 0.62 the inside shows at 38 %. The
  backing bowl (flat body green #5FCF5A, no bands, no grounding; a floor + the shell's profile 2 cm inside it up to
  z ≈ 0.2) is what the dither resolves against where the core is not: the body lands on the web's #5FCF5A lime
  (light band ≈ #91D980) and only the core ellipse darkens — the deep ``#0B4019`` core resolves to ≈ the web's core
  tint (rgba(28,110,48,.55) over the body). (Art review: over the grass and the blob shadow the body resolved to a
  dull #52AC4B–#64B35F glassy dome.) The bone (16 cm, tilted −29° like the web's −0.5 rad) sits in front of the core
  on the right-front (the side the ``se`` game view sees) and the four bubbles (2.0–3.2 cm) rise in front of it.
* **Ink (art review)**: a per-asset 3.5 px hull request (``OUTLINE_PX``, manifest ``outlinePx1080``; gate 3.0 px) and
  a dome whose bulge **overhangs a small sole** (a jelly drop's contact angle): the near contour sits at z ≈ 6–7 cm and
  the hull's underside stays above the terrain, so the bold closed ink loop of the web survives at the base (the
  flat 36 cm sole buried the lower ink — a third of the silhouette was < 3 px).
* **Sheen**: a thin crescent streak high on the back-right of the dome + a specular dot (``SHEEN_*``): in the front,
  se and sw facings it lies in the screen's upper-left light band, clearly smaller than an eye (the old ellipse on
  the front-right sat beside the eyes in se and read as a third eye).
* **Eyes**: big and high on the dome (``EYE_*``: inside the front third of the 3/4 silhouette); the rim's back half is
  body green (seen from ne / back / nw the eye bumps read as jelly-covered eyes, not dark horns); at death the eye
  (rim, sclera, pupil, glint) collapses completely and eye-sized X bars (bowed to the dome) scale in.
* **Rig (SKEL_Slime, 16 bones, all children of ``root``)**: ``body_base / body_mid / body_top`` carry the web pose
  (``sx, sy`` squash/stretch as non-uniform **bone scale**, ``lean`` as a height-proportional shift, ``hop``),
  ``pod_01..02`` the pseudopod reach, ``eye_l/r`` (openness = vertical scale), ``lid_l/r`` (the angry brows,
  scaled in when attacking), ``xeye_l/r`` (X-ed out eyes at death), ``core``, ``bone_prop``, ``bubble_01..02``
  (rising bubbles, scaled out when they pop). Clips are keyed as bone basis matrices (``kit.anim.bake_basis``).
* **Clips** (web ``Slime.ts`` :46-72; ref speed 1.20 m/s): Idle 667 · Walk (hop) 600 with no sliding — the base is
  planted while the body squashes (FX_Land 510) · Attack01 body slam 500 (WindupEnd 155, Contact 250) · Hurt 200 ·
  Death 500 (melts into a puddle; the runtime adds the puddle decal and the fade) · Stun 600 loop (NEW, the goblins'
  C5 / R8 stun loop: a dazed wobble and lean sway, the eyes half shut under drooping lids — the brow bars laid
  across them — rolling against each other).
"""
from __future__ import annotations

import json
import math
import sys
from dataclasses import dataclass, field, replace
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE.parent / "heroes"))
sys.path.insert(0, str(HERE))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import anim, mesh as M, outline, rig as R, scene, shading  # noqa: E402
from kit.paths import scratch_root  # noqa: E402
from kit.anim import Clip  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402
from kit.rig import Bind  # noqa: E402

import mgeo as G  # noqa: E402

ASSET = "SK_Mon_Slime"
TOKEN = "Mon_Slime"
SKELETON = "SKEL_Slime"
FAMILY = "Monsters_Plains"
PREFIX = "slime"
OUTLINE_CLASS = "monster"
W = 0.44                    # dome half-width (web W 20 px × 2.21 cm)
H = 0.43                    # dome height (web H 17 px × 2.552 cm)
UW = 0.0221                 # web px → m, ground
UH = 0.02552                # web px → m, upright
REF_SPEED = 1.20
BODY_OPACITY = 0.62          # dithered shell (R12): the core, bone and bubbles show at 38 %
OUTLINE_PX = 3.5             # per-asset hull request (art review: at the class's 3.0 px the dome's lower contour
                             # kept < 3 px on a third of the silhouette; the web's bold closed ink loop is identity)
CONTACT_MS = 250.0
WINDUP_END_MS = 155.0

REGIONS = outline.ink_regions(dict(
    body=Region("#5FCF5A", .35, .45),
    core=Region("#0B4019", .30, .0),       # under the 0.62 shell ≈ the web core rgba(28,110,48,.55) over the body
    bone=Region("#F6F0DE", .25, .45),
    bubble=Region("#E6FFDC", .10, .35),
    sclera=Region("#F7FFF0", .20, .25),
    eye_rim=Region("#1D4A24", .30, .10),
    pupil=Region("#10200F", .30, .10),
    glint=Region("#FFFFFF", .05, .20),
    brow=Region("#1D4A24", .30, .10),
    xeye=Region("#123018", .30, .10),
    sheen=Region("#C8F2BE", .10, .25),     # web white α .55 over the body (≈ #B7E9B5), a touch lighter
    inner=Region("#5FCF5A", .0, .0),       # the backing bowl: flat body green (no bands, no grounding)
), OUTLINE_CLASS)

# dome profile (radius, z) bottom → top: a small sole, the bulge overhanging it (a jelly drop's contact angle),
# a soft dome. The overhang lifts the near silhouette contour to z ≈ 5.5 cm (normal 40° below horizontal = the W1
# grazing line): the 3.5 px ink below it (≈ 1.9 cm drop at the default distance, 2.4 cm at max zoom-out) stays
# above the terrain. (Art review: the old flat 36 cm sole put the contour at z ≈ 2 cm and the terrain swallowed the
# lower ink — the dome's bottom thinned into the blob shadow; the landing squash sy .74 still keeps it at 4 cm.)
PROFILE = [(0.20, 0.0), (0.27, 0.007), (0.32, 0.02), (0.36, 0.036), (0.395, 0.055), (0.423, 0.078),
           (0.440, 0.105), (0.444, 0.13), (0.433, 0.165), (0.41, 0.2), (0.38, 0.235), (0.34, 0.28), (0.26, 0.345),
           (0.16, 0.395), (0.075, 0.421), (0.03, 0.428)]
PROFILE_PLOT = PROFILE + [(0.0, 0.43)]          # (the mesh closes the top with a fan: no degenerate pole ring)


def radius_at(z: float) -> float:
    zs = [p[1] for p in PROFILE_PLOT]
    rs = [p[0] for p in PROFILE_PLOT]
    return float(np.interp(z, zs, rs))


def surface_point(lat: float, z: float, out: float = 0.0) -> Vector:
    """Front surface point of the dome at lateral offset ``lat`` and height ``z`` (+ ``out`` along the normal)."""
    r = radius_at(z)
    y = -math.sqrt(max(r * r - lat * lat, 1e-6))
    p = Vector((lat, y, z))
    dz = 1e-3
    dr = (radius_at(z + dz) - radius_at(z - dz)) / (2 * dz)
    n = Vector((lat / r, y / r, -dr)).normalized()
    return p + n * out


def dome_point(az: float, z: float, out: float = 0.0) -> Vector:
    """Dome surface at azimuth ``az`` (deg: 0 = front −Y, +90 = the slime's left +X, ±180 = back) and height
    ``z`` (+ ``out`` along the normal)."""
    r = radius_at(z)
    a = math.radians(az)
    return Vector((r * math.sin(a), -r * math.cos(a), z)) + dome_normal(az, z) * out


def dome_normal(az: float, z: float) -> Vector:
    a = math.radians(az)
    dz = 1e-3
    dr = (radius_at(z + dz) - radius_at(z - dz)) / (2 * dz)
    return Vector((math.sin(a), -math.cos(a), -dr)).normalized()


def sheen_streak(az: float, z: float, length: float, width: float, steps: int = 8) -> M.Part:
    """A single-sided crescent strip along the dome's latitude at (``az``, ``z``): ``length`` m of arc, ``width``
    m across at the middle tapering to points, bowed a little toward the top (the reflected sky band)."""
    import bmesh
    r = radius_at(z)
    span = math.degrees(length / max(r, 1e-3))
    bm = bmesh.new()
    rows = []
    for k in range(steps + 1):
        t = k / steps
        a = az - span / 2 + span * t
        zc = z + 0.010 * math.sin(math.pi * t)                     # bowed toward the top
        w = width * max(0.08, math.sin(math.pi * t) ** 0.8) * 0.5
        c = dome_point(a, zc, 0.0025)
        n = dome_normal(a, zc)
        up = (dome_point(a, zc + 0.01) - dome_point(a, zc - 0.01)).normalized()
        rows.append((bm.verts.new(tuple(c - up * w)), bm.verts.new(tuple(c + up * w)), n))
    for (a0, a1, n0), (b0, b1, _) in zip(rows, rows[1:]):
        f = bm.faces.new((a0, b0, b1, a1))
        f.normal_update()
        if f.normal.dot(n0) < 0:
            f.normal_flip()
    bm.normal_update()
    return M.Part(bm).smooth(True)


def _face_up(part: M.Part) -> None:
    """Orient the inner bowl's faces up / inward (toward the W1 camera at every azimuth)."""
    import bmesh
    bm = part.bm
    bm.normal_update()
    for f in bm.faces:
        c = f.calc_center_median()
        inward = Vector((-c.x, -c.y, 0.0))
        want = Vector((0, 0, 1)) + (inward.normalized() * 0.5 if inward.length > 1e-6 else Vector())
        if f.normal.dot(want) < 0:
            f.normal_flip()
    bm.normal_update()


@dataclass
class SlimeSpec:
    name: str = ASSET
    skeleton: str = SKELETON

    def crown_height(self) -> float:
        return H


EYE_LAT, EYE_Z, EYE_R = 0.098, 0.288, 0.091      # art review: up the dome and 14 % bigger — at se the eyes stay
                                                # inside the front third of the 3/4 silhouette, big and glaring
EYE_SINK = 0.016            # eye bone below the jelly surface
EYE_TILT = 25.0             # deg the eyes face down from the surface normal (their tops tuck into the dome)
EYE_YS = 1.12               # taller eyes: the tilt foreshortens them from the W1 camera
FX_REST_K = 0.02            # the X-eyes and angry brows are modelled collapsed to 2 % about their bones (a clean,
                            # neutral bind pose in UE); the clips scale them in by 1 / FX_REST_K
XEYE_LEN = 2.05             # X bar length (× EYE_R): the X spans the eye like the web's
XEYE_DEG = 50.0             # bar angle from the eye's horizontal (the eye is EYE_YS taller than wide)
XEYE_BEND_R = 0.30          # m: the X bars bow to this radius (≈ the dome's curvature at the eyes)
BUBBLE_PHASE = 0.2          # bubble loop phase offset: both bubbles are up and full-size on Idle F0
SHEEN_AZ, SHEEN_Z = -125.0, 0.36    # crescent streak: back-right, high (upper-left light band in front / se / sw)
SHEEN_LEN, SHEEN_W = 0.16, 0.032    # m of arc, m across (an eye is 0.18 × 0.23 m)
SHEEN_DOT_DAZ, SHEEN_DOT_DZ = 14.0, 0.026   # specular dot beside the streak's upper end
# inner backing bowl (r, z) bottom → top: a floor over the sole, then the shell's own profile 2 cm inside it up to
# z ≈ 0.2 — rays through the near lower jelly land on the floor / lower wall (front-facing below 50°), rays through
# the top on the far wall (front-facing); the steep near parts hug the shell (UE culls them, the preview draws them
# — the same green either way)
BOWL_IN = 0.02
BOWL_Z = (0.006, 0.04, 0.09, 0.15, 0.205)


LID_DZ = 0.092              # brow / lid bone above the eye centre (just over the rim's top)
DAZE_LID_DEG = 30.0         # Stun: the brow bar rolled from the angry slant to a sleepy droop (outer end down)
DAZE_LID_OUT = 0.047        # Stun: m along the eye axis from the eye bone (in front of the sclera and pupil)
DAZE_LID_UP = 0.022         # Stun: m above the eye centre (the upper third of the half-shut eye)
DAZE_LID_SCALE = 1.45       # Stun: the lid bar is a heavy line, longer and thicker than the angry brow
XEYE_OUT = 0.026            # X bars' centre along the eye normal from the eye bone (rest)
XEYE_LIFT = 0.016           # posed: X centre above the squashed jelly surface (half the bar depth + a hair)


def eye_normal(sg: int) -> Vector:
    """The eye's outward axis: the dome normal at the eye tipped ``EYE_TILT`` down (tops tuck into the dome)."""
    n = (surface_point(sg * EYE_LAT, EYE_Z, 0.01) - surface_point(sg * EYE_LAT, EYE_Z, 0.0)).normalized()
    side = n.cross(Vector((0, 0, 1))).normalized()
    return Matrix.Rotation(-math.radians(EYE_TILT), 3, side) @ n


def _tag_back(part: M.Part, z0: float) -> None:
    """Face layer ``af_sub`` = 1 on the faces behind local ``z0`` (the part's local +Z faces out of the eye)."""
    bm = part.bm
    lay = bm.faces.layers.int.get("af_sub") or bm.faces.layers.int.new("af_sub")
    for f in bm.faces:
        f[lay] = 1 if f.calc_center_median().z < z0 else 0


def collapsed(head: Vector) -> Matrix:
    return Matrix.Translation(head) @ Matrix.Diagonal((FX_REST_K, FX_REST_K, FX_REST_K, 1.0)) @ \
        Matrix.Translation(-head)


def build_rig() -> R.Rig:
    scene.ensure_object_mode()
    data = bpy.data.armatures.new(SKELETON)
    obj = bpy.data.objects.new(ASSET, data)
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
    bone("root", (0, 0, 0), (0, 0, 0.05), parent=None)
    bone("body_base", (0, 0, 0.0), (0, 0, 0.1))
    bone("body_mid", (0, 0, 0.18), (0, 0, 0.27))
    bone("body_top", (0, 0, 0.34), (0, 0, 0.43))
    bone("pod_01", (0, -0.25, 0.05), (0, -0.33, 0.05))
    bone("pod_02", (0, -0.37, 0.04), (0, -0.45, 0.04))
    for s, sg in (("l", 1), ("r", -1)):
        e = surface_point(sg * EYE_LAT, EYE_Z, -EYE_SINK)
        bone(f"eye_{s}", e, e + Vector((0, -0.04, 0)))
        bone(f"xeye_{s}", e + Vector((0, -0.03, 0)), e + Vector((0, -0.07, 0)))
        lid = surface_point(sg * (EYE_LAT + 0.004), EYE_Z + LID_DZ, -0.004)
        bone(f"lid_{s}", lid, lid + Vector((0, -0.04, 0)))
    bone("core", (0, 0.04, 0.16), (0, 0.04, 0.26))
    bone("bone_prop", (-0.19, -0.19, 0.12), (-0.19, -0.19, 0.20))     # right-front, in front of the core
    bone("bubble_01", (-0.17, -0.24, 0.06), (-0.17, -0.24, 0.12))
    bone("bubble_02", (0.06, -0.27, 0.07), (0.06, -0.27, 0.13))
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in obj.pose.bones:
        pb.rotation_mode = "QUATERNION"
    obj["af_skeleton"] = SKELETON
    sockets = [
        {"name": "fx_feet", "bone": "root", "pos": Vector((0, 0, 0))},
        {"name": "fx_chest", "bone": "body_mid", "pos": Vector((0, 0, 0.55 * H))},
        {"name": "fx_head", "bone": "body_top", "pos": Vector((0, 0, 0.8 * H))},
        {"name": "fx_overhead", "bone": "body_top", "pos": Vector((0, 0, H + 0.25))},
        {"name": "fx_eye_l", "bone": "eye_l", "pos": J["eye_l"]},
        {"name": "fx_eye_r", "bone": "eye_r", "pos": J["eye_r"]},
        {"name": "fx_pod", "bone": "pod_02", "pos": J["pod_02"]},
    ]
    return R.Rig(obj, SlimeSpec(), J, sockets)


# ── mesh ───────────────────────────────────────────────────────────────────────────────────────────────
def _smooth(x, e0, e1):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def shell_bind() -> Bind:
    """Body weights by height (base / mid / top, smooth bells) and the pseudopod on the front-bottom quarter."""
    names = ("body_base", "body_mid", "body_top", "pod_01", "pod_02")

    def fn(co):
        x, y, z = co[:, 0], co[:, 1], co[:, 2]
        k = np.clip(z / H, 0, 1)
        wb = 1.0 - _smooth(k, 0.05, 0.42)
        wt = _smooth(k, 0.42, 0.85)
        wm = np.clip(1.0 - wb - wt, 0, 1)
        r = np.hypot(x, y)
        cosf = np.where(r > 1e-6, -y / np.maximum(r, 1e-6), 0.0)            # 1 = straight ahead
        front = np.clip((cosf - 0.25) / 0.75, 0, 1) ** 1.5
        low = 1.0 - _smooth(k, 0.0, 0.42)
        pod = front * low * 0.95
        outer = _smooth(r, 0.28, 0.42)
        w = np.stack([wb * (1 - pod), wm * (1 - pod), wt * (1 - pod), pod * (1 - outer), pod * outer], axis=1)
        return w + 1e-6
    return Bind.custom(names, fn)


# LOD1 (spec §1.9 slime 1.5 k): decimated parts, the rising bubbles left out (mobile low / far zoom-out)
LOD1_RATIO = 0.43
LOD1_SKIP = ("bubble",)


def build_body(pal: Palette, rig: R.Rig, lod: int = 0):
    from kit import asset
    if lod:
        b = M.Builder(ASSET, pal, prefix=PREFIX, lod_ratio=LOD1_RATIO, lod_skip=lambda n: n in LOD1_SKIP)
    else:
        b = M.Builder(ASSET, pal, prefix=PREFIX)
    b.regions(**REGIONS)
    shell = M.lathe(PROFILE, sides=36, cap0="fan", cap1="fan")
    sid = b.add(shell, "body", shell_bind(), name="shell", hull_pad=True)
    # eyes: dark rim, sclera, pupil looking slightly down-in (glaring), glint
    for s, sg in (("l", 1), ("r", -1)):
        e = rig.joints[f"eye_{s}"]
        n = eye_normal(sg)
        m = G.frame_z(e, n, Vector((0, 0, 1)))
        rim = M.ellipsoid((EYE_R * 0.95, EYE_R * 1.1 * EYE_YS, 0.028), (0, 0, -0.001), sides=12, rings=6)
        _tag_back(rim, -0.006)        # the back half is jelly: seen from behind the eyes are green bumps, not horns
        rim.transform(m)
        b.add(rim, "eye_rim", Bind.rigid(f"eye_{s}"), name=f"eye_rim_{s}", sub_regions={1: "body"}, outline=False,
              covered_by=sid)
        scl = M.ellipsoid((EYE_R * 0.85, EYE_R * EYE_YS, 0.030), (0, 0, 0.006), sides=12, rings=6).transform(m)
        b.add(scl, "sclera", Bind.rigid(f"eye_{s}"), name=f"sclera_{s}", outline=False, covered_by=sid)
        pup = M.ellipsoid((EYE_R * 0.42, EYE_R * 0.52 * EYE_YS, 0.012), (-sg * EYE_R * 0.1, -EYE_R * 0.08, 0.031),
                          sides=10, rings=5).transform(m)
        b.add(pup, "pupil", Bind.rigid(f"eye_{s}"), name=f"pupil_{s}", outline=False, covered_by=sid)
        gl = M.ellipsoid((EYE_R * 0.13, EYE_R * 0.13, 0.006), (sg * EYE_R * 0.08, EYE_R * 0.22, 0.041), sides=8,
                         rings=4).transform(m)
        b.add(gl, "glint", Bind.rigid(f"eye_{s}"), name=f"glint_{s}", outline=False, covered_by=sid)
        # X-ed out eye (death): two crossed bars spanning the eye (the web's X is eye-sized: ±0.6 r both ways at a
        # 0.9 u stroke), scaled in by xeye_* while the eye itself collapses
        for ang in (XEYE_DEG, -XEYE_DEG):
            # bowed to the dome's curvature (a straight bar's ends stood ~3 cm proud of the shell and the padded ink
            # grew dark bumps round them on the stretched death frames once the eyes moved up the dome)
            hl = EYE_R * XEYE_LEN / 2
            sag = hl * hl / (2 * XEYE_BEND_R)
            bar = M.tube([Vector((-hl, 0, -sag)), Vector((0, 0, 0)), Vector((hl, 0, -sag))],
                         [(0.012, 0.008)] * 3, sides=4, phase=math.pi / 4, up=(0, 1, 0), dome0=0.0, dome1=0.0)
            bar.transform(m @ Matrix.Translation((0, 0, XEYE_OUT)) @ Matrix.Rotation(math.radians(ang), 4, "Z"))
            b.cover(sid, bar)                   # the full-size bars sit inside the shell's padded ink
            bar.transform(collapsed(rig.joints[f"xeye_{s}"]))
            b.add(bar, "xeye", Bind.rigid(f"xeye_{s}"), name=f"xeye_{s}", outline=False)
        # angry brow (attack): a slanted bar above the eye, lower toward the middle (scaled in by lid_*)
        lid = rig.joints[f"lid_{s}"]
        a0 = lid + Vector((sg * 0.05, 0, 0.01))
        a1 = lid + Vector((-sg * 0.045, 0, -0.022))
        # laid on the dome (each point on the surface + 4 mm): a straight bar's ends stood proud near the dome's top
        bpts = [surface_point(q.x, q.z, 0.004) for q in (a0, a0.lerp(a1, 0.5), a1)]
        brow = M.tube(bpts, [0.012, 0.013, 0.014], sides=6, dome0=0.006, dome1=0.006)
        b.cover(sid, brow)
        brow.transform(collapsed(lid))
        b.add(brow, "brow", Bind.rigid(f"lid_{s}"), name=f"brow_{s}", outline=False)
    # glossy sheen: a thin crescent streak high on the dome's back-right + a specular dot (art review: the old
    # ellipse on the front-right landed beside the eyes in se at eye size and value — a "third eye"). At SHEEN_AZ the
    # streak sits in the screen's upper-left light band in the front, se and sw views (the facings that look at the
    # hero), pale green-white like the web's white α .55 over the body, clearly smaller than an eye.
    bd = shell_bind()
    b.add(sheen_streak(SHEEN_AZ, SHEEN_Z, SHEEN_LEN, SHEEN_W), "sheen", bd, name="sheen", outline=False,
          covered_by=sid)
    c = dome_point(SHEEN_AZ + SHEEN_DOT_DAZ, SHEEN_Z + SHEEN_DOT_DZ, 0.0025)
    m = G.frame_z(c, dome_normal(SHEEN_AZ + SHEEN_DOT_DAZ, SHEEN_Z + SHEEN_DOT_DZ), Vector((0, 0, 1)))
    b.add(M.ellipsoid((0.012, 0.012, 0.005), (0, 0, 0), sides=8, rings=3).transform(m), "glint", bd,
          name="sheen", outline=False, covered_by=sid)
    # inner backing (R12 look, art review: the 0.62 shell over the dark core and the grass resolved to a dull
    # #52AC4B–#64B35F glassy dome): a body-green bowl inside the jelly — floor + a wall rising at 45° (front-facing
    # from the W1 pitch at every azimuth, so UE's culling and the preview agree) — is what the dither mixes with
    # where the core is not: the body resolves to the web's #5FCF5A lime and only the core ellipse darkens (≈ the web
    # core rgba(28,110,48,.55) over the body). The core, bone and bubbles stand in front of it.
    bowl = M.lathe([(0.12, BOWL_Z[0])] + [(radius_at(z) - BOWL_IN, z) for z in BOWL_Z], sides=16, cap0="fan",
                   cap1=None)
    _face_up(bowl)
    bowl_id = b.add(bowl, "inner", bd, name="inner_bowl", outline=False, covered_by=sid)
    # inside: the dark jelly core (two merged lobes), the half-digested bone, bubbles
    core = M.ellipsoid((0.27, 0.24, 0.17), (0.0, 0.03, 0.19), sides=14, rings=8)
    b.add(core, "core", Bind.rigid("core"), name="core", outline=False)
    lobe = M.ellipsoid((0.15, 0.13, 0.11), (-0.09, -0.04, 0.12), sides=10, rings=6)
    b.add(lobe, "core", Bind.rigid("core"), name="core", outline=False)
    bp = rig.joints["bone_prop"]
    # in the plane facing the se game view (the camera sees the slime's front-right there), tilted 29° like the web
    tilt = Matrix.Translation(bp) @ Matrix.Rotation(math.radians(-45), 4, "Z") @ Matrix.Rotation(math.radians(-29), 4, "Y")
    # the web bone: 7 × 1.4 u shaft with two round knobs at each end (≈ 16 × 3 cm), tilted −0.5 rad
    shaft = M.capsule((-0.075, 0, 0), (0.075, 0, 0), 0.016, 0.016, sides=6, cap_rings=1).transform(tilt)
    b.add(shaft, "bone", Bind.rigid("bone_prop"), name="bone_prop", outline=False)
    for x in (-0.082, 0.082):
        for dz in (-0.018, 0.018):
            k = M.ellipsoid((0.024, 0.024, 0.024), (x, 0, dz), sides=6, rings=4).transform(tilt)
            b.add(k, "bone", Bind.rigid("bone_prop"), name="bone_prop", outline=False)
    for nm, offs in (("bubble_01", ((0, 0, 0, 0.034), (0.06, 0.03, 0.055, 0.024))),
                     ("bubble_02", ((0, 0, 0, 0.03), (-0.07, 0.02, 0.06, 0.022)))):
        c0 = rig.joints[nm]
        for dx, dy, dz, r in offs:
            bb = M.ellipsoid((r, r, r), c0 + Vector((dx, dy, dz)), sides=6, rings=4)
            b.add(bb, "bubble", Bind.rigid(nm), name="bubble", outline=False)
    body = asset.finish_mesh(b, pal, OUTLINE_CLASS, rig=rig, grounding_height=H,
                             name=f"{ASSET}_LOD{lod}" if lod else None)
    body["af_lod"] = lod
    _no_grounding(body, bowl_id)
    # the shell's toon faces → slot 2, the dithered jelly material (R12)
    me = body.data
    fp = np.zeros(len(me.polygons), np.int32)
    me.attributes["af_part"].data.foreach_get("value", fp)
    mi = np.zeros(len(me.polygons), np.int32)
    me.polygons.foreach_get("material_index", mi)
    mi = np.where((fp == sid) & (mi == 0), 2, mi)
    me.materials.append(bpy.data.materials.get("MI_AF_Toon_Slime") or dither_material(pal, BODY_OPACITY))
    me.polygons.foreach_set("material_index", mi)
    me.update()
    return body, sid


def _no_grounding(body, part_id: int) -> None:
    """AF_Data.r = 0 on one part's vertices (the backing bowl lies at z ≈ 0, where the grounding term would darken
    it 22 % toward #0A0818: it stands for the jelly's own colour seen through the shell, not a surface near the
    ground)."""
    me = body.data
    fp = np.zeros(len(me.polygons), np.int32)
    me.attributes["af_part"].data.foreach_get("value", fp)
    vs = set()
    for poly, pid in zip(me.polygons, fp.tolist()):
        if pid == part_id:
            vs.update(poly.vertices)
    att = me.color_attributes["AF_Data"]
    for v in vs:
        c = list(att.data[v].color)
        c[0] = 0.0
        att.data[v].color = c
    me.update()


def dither_material(pal: Palette, opacity: float) -> bpy.types.Material:
    """Preview of ``MI_AF_Toon_Slime`` (parent ``M_AF_Toon_Slime``): the palette toon, dithered to ``opacity`` —
    rendered as the TAA-resolved mix with what lies behind; back faces are culled as in UE (one-sided masked)."""
    base = shading.toon_material(pal)
    m = base.copy()
    m.name = "MI_AF_Toon_Slime"
    nt = m.node_tree
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
    m["af_params"] = json.dumps({"BodyOpacity": opacity})
    m["af_toon_slot"] = True
    return m


# ── pose model (web SlimePose) → bone basis matrices ───────────────────────────────────────────────────
@dataclass
class SP:
    x: float = 0.0          # forward offset (m)
    hop: float = 0.0        # height off the ground (m)
    sx: float = 1.0
    sy: float = 1.0
    lean: float = 0.0       # top leans forward (+) or back (−)
    eyes: float = 1.0       # 0 X-ed out … 1 open
    reach: float = 0.0      # pseudopod 0..1
    melt: float = 0.0
    fx: float = 0.0         # attack intensity (angry brows)
    yaw: float = 0.0        # deg, a little body turn (3D life)
    daze: float = 0.0       # stunned: the brows become heavy drooping lids sitting on the half-shut eyes
    eye_roll: float = 0.0   # deg, the eyes (and their pupils) rolled about their axes — the woozy wobble

    def lerp(self, o: "SP", t: float) -> "SP":
        return SP(*[a + (b - a) * t for a, b in zip(self.__dict__.values(), o.__dict__.values())])


REST = SP()


def P(**kw) -> SP:
    return replace(REST, **kw)


def sample(keys, t: float) -> SP:
    """Web ``samplePoseTrack``: keys [(at, ease, SP)], easing of the destination key."""
    if t <= keys[0][0]:
        return keys[0][2]
    for (a0, _, p0), (a1, e1, p1) in zip(keys, keys[1:]):
        if a0 <= t <= a1:
            return p0.lerp(p1, anim.ease((t - a0) / max(a1 - a0, 1e-9), e1))
    return keys[-1][2]


class SlimeAnims:
    def __init__(self, rig: R.Rig):
        self.rig = rig
        self.rest = {b.name: b.matrix_local.copy() for b in rig.obj.data.bones}

    def body_x(self, p: SP, k: float) -> Matrix:
        """Armature-space map of the body at height fraction ``k``: scale (sx, sx, sy·(1 − melt/2)) about the
        ground point, the lean shift (web x += lean·h·k·1.2), hop and forward offset, a small yaw."""
        sy = p.sy * (1.0 - 0.5 * p.melt)
        S = Matrix.Diagonal((p.sx, p.sx, sy, 1.0))
        shift = Vector((0.0, -(p.x + p.lean * H * sy * k * 1.2), p.hop))
        return Matrix.Translation(shift) @ Matrix.Rotation(math.radians(p.yaw), 4, "Z") @ S

    def basis(self, p: SP, t_ms: float, loop_ms: float) -> dict:
        out = {}
        rest = self.rest

        def put(name, X):
            Mr = rest[name]
            out[name] = Mr.inverted() @ X @ Mr
        for nm in ("body_base", "body_mid", "body_top", "core", "bone_prop"):
            k = rest[nm].translation.z / H
            put(nm, self.body_x(p, k))
        # pseudopod: the front-bottom reaches forward (web: x += reach·7 px at the base, y += reach·2)
        for nm, d in (("pod_01", 0.09), ("pod_02", 7.0 * UW)):
            X = Matrix.Translation(Vector((0.0, -p.reach * d, -p.reach * 0.012))) @ self.body_x(p, 0.0)
            S = Matrix.Diagonal((1.0 + 0.25 * p.reach, 1.0 + 0.6 * p.reach, 1.0 - 0.35 * p.reach, 1.0))
            c = rest[nm].translation
            put(nm, X @ Matrix.Translation(c) @ S @ Matrix.Translation(-c))
        # eyes ride the surface (no squash of their own), open = vertical scale; brows / X-eyes scale in
        xe = max(0.0, min(1.0, (0.12 - p.eyes) / 0.1))
        open_ = 0.35 + 0.65 * max(p.eyes, 0.0)
        gone = max(0.001, 1.0 - xe)             # the whole eye (rim, sclera, pupil, glint) shrinks out under the X
        brow = max(0.001, min(1.0, (p.fx - 0.2) / 0.25), min(1.0, p.daze))
        Rz = Matrix.Rotation(math.radians(p.yaw), 4, "Z")
        sy_eff = p.sy * (1.0 - 0.5 * p.melt)
        for s, sg in (("l", 1), ("r", -1)):
            n0 = eye_normal(sg)
            for nm, sc in ((f"eye_{s}", (gone, gone, max(0.001, open_ * gone))),
                           (f"lid_{s}", (brow / FX_REST_K,) * 3)):
                a = rest[nm].translation
                k = a.z / H
                pa = self.body_x(p, k) @ a
                D = Matrix.Diagonal((sc[0], sc[1], sc[2], 1.0))
                if nm.startswith("eye_"):
                    Rr = Matrix.Rotation(math.radians(sg * p.eye_roll), 4, n0)
                    put(nm, Matrix.Translation(pa) @ Rz @ D @ Rr @ Matrix.Translation(-a))
                    continue
                if p.daze < 0.5:
                    put(nm, Matrix.Translation(pa) @ Rz @ D @ Matrix.Translation(-a))
                    continue
                # dazed: the brow bar becomes a heavy drooping upper lid lying across the half-shut eye (outer end
                # down), in front of the sclera, its line a third of the way down the squashed eye
                e = rest[f"eye_{s}"].translation
                pc = self.body_x(p, e.z / H) @ (e + n0 * DAZE_LID_OUT + Vector((0.0, 0.0, DAZE_LID_UP)))
                Rd = Matrix.Rotation(math.radians(sg * DAZE_LID_DEG), 4, -n0)
                Sd = Matrix.Diagonal((sc[0] * DAZE_LID_SCALE,) * 3 + (1.0,))
                put(nm, Matrix.Translation(pc) @ Rz @ Rd @ Sd @ Matrix.Translation(-a))
            # X-eyes lie on the squashed jelly: turned from the eye axis to the posed surface normal (S^-T n) and
            # centred just above the posed surface point — flattened into the death puddle they stay whole X's
            # instead of sinking half into the shell
            a = rest[f"xeye_{s}"].translation
            n0 = eye_normal(sg)
            s0 = surface_point(sg * EYE_LAT, EYE_Z, 0.0)
            c_rest = rest[f"eye_{s}"].translation + n0 * XEYE_OUT
            ns = Vector((n0.x / p.sx, n0.y / p.sx, n0.z / max(sy_eff, 1e-3))).normalized()
            Rn = n0.rotation_difference(ns).to_matrix().to_4x4()
            surf = self.body_x(p, s0.z / H) @ s0
            target = surf + (Rz.to_3x3() @ ns) * XEYE_LIFT
            R = Rz @ Rn
            pa = target - R.to_3x3() @ (c_rest - a)
            D = Matrix.Diagonal((max(0.001, xe) / FX_REST_K,) * 3 + (1.0,))
            put(f"xeye_{s}", Matrix.Translation(pa) @ R @ D @ Matrix.Translation(-a))
        # bubbles rise inside and pop near the top (web: 4 bubbles, k = (i/4 + t) % 1)
        for i, nm in enumerate(("bubble_01", "bubble_02")):
            a = rest[nm].translation
            ph = ((t_ms / loop_ms) + i * 0.5 + BUBBLE_PHASE) % 1.0
            z = a.z + ph * (0.62 * H - a.z) * 1.0
            pop = max(0.001, min(1.0, (0.92 - ph) / 0.12)) * min(1.0, 0.3 + ph / 0.15)
            pop = max(0.001, pop * (1.0 - min(1.0, 1.6 * p.melt)))      # the puddle has no rising bubbles
            pa = self.body_x(p, z / H) @ Vector((a.x, a.y, z))
            pa.z = max(pa.z, 0.036 * pop)                                  # never under the floor (squashed body)
            D = Matrix.Diagonal((pop, pop, pop, 1.0))
            put(nm, Matrix.Translation(pa) @ D @ Matrix.Translation(-a))
        return out

    def clip(self, name, length_ms, fn, loop=False, notifies=None, ref_speed=None, loop_ms=667.0, key_ms=None):
        c = Clip(name, length_ms, loop=loop, notifies=notifies or {}, ref_speed=ref_speed)
        c.basis_fn = lambda t: self.basis(fn(t), t, loop_ms)
        c.sampler = lambda t: None
        c.key_poses_ms = key_ms or [length_ms * i / 5 for i in range(6)]
        return c

    # web tracks (Slime.ts :46-72), t = ms / span
    def idle(self):
        def fn(t):
            ph = 2 * math.pi * t / 667.0
            return P(sx=1 + math.sin(ph) * 0.06, sy=1 - math.sin(ph) * 0.07, lean=math.sin(ph + 0.8) * 0.05,
                     yaw=3.0 * math.sin(ph * 0.5))
        return self.clip("Idle", 667.0, fn, loop=True)

    WALK = [(0.0, "smooth", P(sx=1.18, sy=0.8)),
            (0.2, "out", P(sx=0.86, sy=1.18, hop=6 * UH, lean=0.12)),
            (0.45, "smooth", P(sx=0.92, sy=1.1, hop=9 * UH, lean=0.06)),
            (0.7, "in", P(sx=1.0, sy=1.02, hop=3 * UH)),
            (0.85, "smooth", P(sx=1.25, sy=0.74)),
            (1.0, "smooth", P(sx=1.18, sy=0.8))]

    def walk(self):
        """One hop per 600 ms at 1.20 m/s (0.72 m). No sliding (R9): while the sole is on the ground (t ≥ .82 and
        t < .08) the body moves back at exactly the ground speed; in the air it lunges forward (web keys give the
        squash / hop)."""
        L = 600.0
        c0, c1 = 0.82, 1.08                 # contact window (wraps)
        A = REF_SPEED * L / 1000.0 * (c1 - c0) / 2.0

        def xoff(t):
            u = t if t >= c0 else t + 1.0
            if u <= c1:                                   # planted: +A → −A linearly
                return A - 2 * A * (u - c0) / (c1 - c0)
            f = (t - (c1 - 1.0)) / (c0 - (c1 - 1.0))      # airborne: −A → +A
            e = f * f * (3 - 2 * f)
            return -A + 2 * A * e

        def fn(t_ms):
            t = (t_ms / L) % 1.0
            p = sample(self.WALK, t)
            return replace(p, x=xoff(t), yaw=2.0 * math.sin(2 * math.pi * t))
        return self.clip("Walk", L, fn, loop=True, notifies={"FX_Land": 510.0}, ref_speed=REF_SPEED, loop_ms=600.0,
                         key_ms=[0.0, 120.0, 270.0, 420.0, 510.0, 600.0])

    def attack(self):
        keys = [(0.0, "smooth", REST),
                (0.33, "out", P(x=-3 * UW, sx=0.85, sy=1.26, lean=-0.3, fx=0.3)),     # web .8 / 1.3: held at .85 so the
                # skinned hull keeps the 2.5 px ink floor on the narrowed sides (the WPO subtracts the full baked width)
                (0.67, "in", P(x=4 * UW, sx=0.95, sy=1.1, hop=6 * UH, lean=0.35, reach=0.4, fx=0.7)),
                (1.0, "linear", P(x=5 * UW, sx=1.3, sy=0.72, lean=0.25, reach=1.0, fx=1.0)),
                (1.32, "out", P(x=4.6 * UW, sx=1.22, sy=0.8, lean=0.18, reach=0.85, fx=0.9)),
                (1.65, "smooth", P(x=1.5 * UW, sx=0.95, sy=1.06, lean=-0.04, reach=0.2, fx=0.5)),
                (2.0, "smooth", REST)]

        def fn(t_ms):
            return sample(keys, t_ms / CONTACT_MS)
        return self.clip("Attack01", 500.0, fn, notifies={"WindupEnd": WINDUP_END_MS, "Contact": CONTACT_MS},
                         key_ms=[0.0, 83.3, 166.7, 250.0, 330.0, 412.0, 500.0])

    def hurt(self):
        keys = [(0.0, "smooth", P(x=-3 * UW, sx=1.35, sy=0.68, lean=-0.25, eyes=0.35)),
                (0.5, "smooth", P(x=-1.5 * UW, sx=0.9, sy=1.12, lean=0.1, eyes=0.7)),
                (1.0, "smooth", P(x=-0.5 * UW, sx=1.02, sy=0.98, eyes=0.95))]

        def fn(t_ms):
            return sample(keys, t_ms / 200.0)
        return self.clip("Hurt", 200.0, fn, key_ms=[0.0, 100.0, 200.0])

    def death(self):
        keys = [(0.0, "smooth", P(x=-2 * UW, sx=1.35, sy=0.7, lean=-0.2, eyes=0.3)),
                (0.33, "smooth", P(x=-2 * UW, sx=0.85, sy=1.25, eyes=0.0)),
                (0.67, "in", P(x=-2 * UW, sx=1.6, sy=0.5, eyes=0.0, melt=0.5)),
                (1.0, "out", P(x=-1 * UW, sx=1.65, sy=0.3, eyes=0.0, melt=1.0))]

        def fn(t_ms):
            return sample(keys, t_ms / 500.0)
        c = self.clip("Death", 500.0, fn, key_ms=[0.0, 165.0, 335.0, 500.0])
        c.manifest_extra = {"puddleDecal": {"color": "#5FCF5A", "alpha": 0.35, "radiusFactor": 1.1,
                                            "note": "web fx: rgba(95,207,90,0.35·melt) ellipse r = 1.1·W·sx"}}
        return c

    def stun(self):
        """Stun 600 ms loop (NEW, the goblins' C5 / R8 stun loop for the slime): a dazed wobble — the body slumps
        (sy .93) and sways in a slow lean circle with a jelly wobble at twice the rate, the eyes half shut under
        drooping lids, rolling against each other (the pupils circle), a lazy yaw. Loops seamlessly."""
        L = 600.0

        def fn(t_ms):
            ph = 2 * math.pi * t_ms / L
            return P(sx=1.04 + 0.035 * math.sin(2 * ph), sy=0.93 - 0.04 * math.sin(2 * ph), x=0.012 * math.cos(ph),
                     lean=0.11 * math.sin(ph), yaw=7.0 * math.cos(ph), eyes=0.17 + 0.04 * math.sin(ph + 1.2),
                     daze=1.0, eye_roll=18.0 * math.sin(ph))
        c = self.clip("Stun", L, fn, loop=True, loop_ms=L, key_ms=[0.0, 150.0, 300.0, 450.0, 600.0])
        c.manifest_extra = {"usage": "stunned (C5 stun skills / R8 stun stars over fx_overhead): loops until the "
                                     "stun ends"}
        return c

    def all_clips(self):
        return [self.idle(), self.walk(), self.attack(), self.hurt(), self.death(), self.stun()]


# ── build / review / ship ──────────────────────────────────────────────────────────────────────────────
def build_all():
    scene.reset()
    pal = Palette(FAMILY)
    rig = build_rig()
    body, sid = build_body(pal, rig)
    return pal, rig, body


FX = {"jellyRim": {"color": "#E6FFDC", "alpha": 0.6, "material": "M_AF_Toon_Slime RimOverride"},
      "bodyOpacity": BODY_OPACITY,
      "puddle": {"color": "#5FCF5A", "alpha": 0.35, "radiusFactorOfWidth": 1.1, "fromClip": "Death"},
      "blobShadow": {"radiusFactorOfWidth": 1.05, "liftShrink": "hop × 1.5 (web)"},
      "telegraph": {"normal": "#FFC4B0", "elite": "#FF7A5C", "untilMs": 155},
      "land": {"notify": "FX_Land", "atMs": 510}}


class _A:
    """Duck-typed anims holder for ``mship`` (rig + clips)."""

    def __init__(self, rig):
        self.rig = rig


def quick(out: Path, clip: str | None = None, frame: int = 0) -> None:
    from kit import review
    pal, rig, body = build_all()
    S = SlimeAnims(rig)
    out.mkdir(parents=True, exist_ok=True)
    if clip:
        c = next(c for c in S.all_clips() if c.name == clip)
        a = anim.bake_basis(rig, c, TOKEN, c.basis_fn)
        anim.set_frame(frame)
    import mship
    rv = mship.MReview(ASSET, rig.obj, [body], H, blob_radius=W * 1.05, out_dir=out, game_outline_px=OUTLINE_PX,
                       outline_class=OUTLINE_CLASS)
    rv.gate = mship.gate_for(OUTLINE_CLASS, OUTLINE_PX)
    rv.ground_occludes = True
    rv.extra_body_slots = True
    rv.closeup("se", pitch=-35.0)
    (out / "closeup.png").rename(out / "closeup_se.png")
    rv.closeup("front", pitch=-30.0)
    (out / "closeup.png").rename(out / "closeup_front.png")
    rv.turnaround()
    rv.game_view()
    for p, s in rv.written:
        print(f"  {p.name} {s // 1024} KB")


def anim_quick(out: Path, names) -> None:
    import mship
    pal, rig, body = build_all()
    S = SlimeAnims(rig)
    cs = [c for c in S.all_clips() if not names or c.name in names]
    mship.anim_review(rig, [body], [], cs, TOKEN, out, H, OUTLINE_CLASS, blob_radius=W * 1.05, px=OUTLINE_PX)


def ship(previews: bool = True, verify: bool = True) -> int:
    import mship
    pal, rig, body = build_all()
    body1, _ = build_body(pal, rig, lod=1)
    S = SlimeAnims(rig)
    cs = S.all_clips()
    ms = mship.MonsterShip(
        asset=ASSET, token=TOKEN, preview_dir="mon_slime", pal=pal, A=_A(rig), body=body, items=[], clips=cs,
        outline_class=OUTLINE_CLASS, game_ids=["monster_slime"], blob_radius=round(W * 1.05, 3), height=H,
        display={"zh-CN": "绿色史莱姆", "en": "Green Slime"}, fx=FX,
        qa_limits={"ground": -0.5},
        game_keys=[("Idle", 0, "se"), ("Walk", 7, "se"), ("Walk", 16, "se"), ("Walk", 31, "se"),
                   ("Attack01", 5, "se"), ("Attack01", 15, "se"), ("Hurt", 0, "se"), ("Death", 30, "se"),
                   ("Stun", 0, "se"), ("Stun", 18, "sw")],
        close_keys=[("Idle", 0, "front"), ("Walk", 12, "side"), ("Attack01", 5, "se"), ("Attack01", 15, "se"),
                    ("Attack01", 15, "side"), ("Hurt", 0, "se"), ("Death", 10, "se"), ("Death", 30, "se"),
                    ("Stun", 9, "front"), ("Stun", 27, "se")],
        back_clips=("Attack01", "Death"),
        skeleton_bones_required=("root", "body_base", "body_mid", "body_top", "pod_01", "pod_02", "eye_l", "eye_r",
                                 "lid_l", "lid_r", "xeye_l", "xeye_r", "core", "bone_prop", "bubble_01",
                                 "bubble_02"),
        extra={"refSpeedCmS": REF_SPEED * 100.0, "attackStyle": "slam", "footprintRadiusCm": round(W * 100, 1),
               "notes": "SKEL_Slime: squash / stretch as bone scale keys (body_*), pseudopod pod_*, eye openness, "
                        "angry brows (lid_*) and X-eyes (xeye_*) scaled in, bubbles rising and popping. Shell = "
                        "slot 2 MI_AF_Toon_Slime (M_AF_Toon_Slime, dithered BodyOpacity, jelly rim, R12); the core, "
                        "bone, bubbles, eyes, the sheen streak and the inner backing bowl (flat body green, what the "
                        "dither resolves against: body #5FCF5A, darker only over the core) are opaque toon (slot 0). "
                        "Walk is a no-slide hop: play rate = v / 1.20 m/s. Stun loops (dazed: lids droop, eyes roll)."},
        lod_note="Slime budget 2-3k toon tris (shell slot 2 included), <= 16 bones (SKEL_Slime has 16).",
        lod1=body1,
        outline_px=OUTLINE_PX,
        outline_px_note="art review: at the monster class's 3.0 px the dome's lower contour (lifted off the "
                        "terrain by the overhanging bulge) kept < 3 px on a third of the silhouette; 3.5 px (as "
                        "the boss) gives the web's bold closed ink loop, gated at 3.0 px",
    )
    res = mship.ship(ms, previews=previews, verify=verify)
    return mship.main_result(res)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    scratch = scratch_root("mq", "slime")
    if "--quick" in args:
        k = args.index("--quick")
        rest = [a for a in args[k + 1:] if not a.startswith("--")]
        quick(scratch, rest[0] if rest else None, int(rest[1]) if len(rest) > 1 else 0)
    elif "--anims" in args:
        names = [a for a in args[args.index("--anims") + 1:] if not a.startswith("--")]
        anim_quick(scratch / "anims", names)
    else:
        sys.exit(ship(previews="--no-previews" not in args, verify="--no-verify" not in args))

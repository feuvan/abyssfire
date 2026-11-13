"""食月犬 the Moon-hound — ``SK_Mon_MoonHound`` on ``SKEL_Quadruped`` (Chapter 2 绳林 / the Hanging Wood, package
4R-1; Docs/dark-remake/chapter2.md §4.4 ``moon_hound``, §17.1-17.2): the first member of the quadruped rig family.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/moon_hound.py
        (no flag) ship · --quick [pose] · --anims [Clip …] [--facing=se] · --portrait · --no-previews · --no-verify
        --poses Clip@ms … [--facing=side] [--pitch=-12] [--name=poses]   (dev: clip moments on one sheet, one scale)
        --gait (the no-slide report of Walk / Run and exit)

The Moon-Bitten's pack hunter: a gaunt, long-legged hound, 0.75 m at the withers and 1.30 m nose to rump — a deep
narrow chest, the waist tucked up to nothing, the ribs showing as paint strokes, a long narrow muzzle carried **low**
(under the back line) and a **straight** tail. Silver-grey coat gone to slate (``moonhound.coat`` #6E7478, shadow
#32363A, belly #9AA0A0, a darker saddle along the spine), a torn leather temple collar with a tarnished Well-Ring
(#8A9088). Family marker: **silver-white eye-shine, not emissive** (``moonbitten.eye`` #E6ECF5 in a near-black
socket) — beasts and the long-turned, not the seal's dead. No gore: maw and nose near-black, teeth dull bone.

Read at the W1 camera (the brief's hardest case: a low dark body on dark ground): the silhouette is a **horizontal**
one — back line, tucked waist, four thin legs, a low wedge head, a level tail — where every Chapter 1 figure is an
upright; the pale belly line and the lit back separate it from the root earth, the eyes are the two brightest
pixels of it.

Behaviour (chapter2.md §4.0 / §4.4): L15, speed 125 = 3.75 m/s (faster than the hero's 3.6: ``Run`` ref speed),
range 1.3, attack 850 ms, ``packAggro`` 8. Clips and the beats the core needs:

| Clip | ms | Beats (notifies) | Use |
|---|---|---|---|
| ``Idle`` | 1600 loop | — | the stand: breathing, the low head quartering |
| ``Idle_Sniff`` | 2400 | — | idle variant: nose along the roots (READY → READY) |
| ``Walk`` | 700 loop | FootFL 0, FootBR 0, FootFR 350, FootBL 350 | the trot (ref 1.30 m/s): a slowed hound |
| ``Run`` | 460 loop | FootBL 0, FootBR 46, FootFL 211.6, FootFR 266.8 | the gallop (ref **3.75 m/s** = the data speed) |
| ``Attack01`` | 800 | WindupEnd 198.4, **Contact 320** | the bite: one contact, one damage roll |
| ``Pounce`` | 1000 | WindupEnd 403, Leap 380 (``FX_PounceLeap``), **Contact 650** (``FX_PounceLand``) | 扑 Pounce (line 4 × 1.2, dash): leaves the ground at 380, strikes at 650 |
| ``Hurt`` | 280 | — | |
| ``Death`` | 900 | — | rolls onto its side; the last frame is the corpse (remains: the runtime greys it to a husk) |
| ``Stun`` | 700 loop | — | |

No ``Rise``: the Moon-Bitten leave remains, nothing raises them (Volgan's phase summons spawn at their anchors).
Budget (§17.1): LOD0 ≤ 5 k / LOD1 ≤ 2.5 k toon triangles. Ink 3.5 px (the per-asset request of every dark-remake
monster, gated at 3 px). Portrait ``T_UI_Portrait_MoonHound``.
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "hangwood", HERE.parent / "march", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import anim, mesh as M, outline, rig as R, scene, skel_quadruped as SQ  # noqa: E402
from kit.palette import Palette  # noqa: E402
from kit.paths import scratch_root  # noqa: E402
from kit.rig import Bind  # noqa: E402

import hangwood_palettes as HP  # noqa: E402
import mgeo as G  # noqa: E402
from qclips import QP, QuadAnims  # noqa: E402

ASSET = "SK_Mon_MoonHound"
TOKEN = "Mon_MoonHound"
PREVIEW = "mon_moon_hound"
GAME_IDS = ["monster_moon_hound"]
DISPLAY = {"zh-CN": "食月犬", "en": "Moon-hound"}
PORTRAIT = "T_UI_Portrait_MoonHound"
FAMILY = "Monsters_Hangwood"
OUTLINE_CLASS = "monster"
OUTLINE_PX = 3.5
OUTLINE_PX_NOTE = ("per-asset request (as every dark-remake monster): thin legs, ear tips, the tail and the jaw keep "
                   "< 3 px on too many edges at the class's 3.0 px; gated at 3.0 px")
RUN_SPEED = 125 * 0.03                # chapter2.md §4.0 speed 125 → 3.75 tiles/s = m/s
WALK_SPEED = 1.30                     # the trot's ref speed (what a 0.55 m foreleg covers in half of 700 ms)
WITHERS = 0.75
BLOB = 0.40
CROP_H = 1.45                         # review framing (a 1.7 m long animal: the game cells are sized from this)
LOD0_MAX, LOD1_MAX = 5000, 2500
LOD1_HULL_RATIO = 0.80
LOD1_SKIP = ("claw", "tooth_side", "rib", "lip", "ear_in")
MARKER = ("moonbitten.eye",)
SWATCHES = ("moonhound.coat", "moonhound.coat_shadow", "moonhound.belly", "moonhound.back", "moonhound.ear_inner",
            "moonhound.collar", "moonhound.collar_edge", "moonhound.ring", "moonbitten.eye", "moonbitten.eye_rim",
            "moonbitten.maw", "moonbitten.teeth", "moonbitten.claw")
EYE_R = (0.027, 0.0215, 0.008)        # the eye-shine disc (m) under its lid: a ~6 × 3 px slit at the W1 camera


def hangwood_stage() -> None:
    """Every review stage of this process on the hangwood root earth (the darkest walkable tile's value)."""
    from kit import review
    if getattr(review.stage, "_hangwood", False):
        return
    orig = review.stage

    def stage(radius: float = 30.0, blob_radius: float = BLOB, ground_hex_: str = HP.STAGE_GROUND_HEX,
              grid: bool = True, **kw):
        return orig(radius=radius, blob_radius=blob_radius, ground_hex=kw.get("ground_hex", ground_hex_), grid=grid)
    stage._hangwood = True
    review.stage = stage


# ── the body ───────────────────────────────────────────────────────────────────────────────────────────────
# trunk stations rump → forechest: (y, top z, bottom z, half width)
TRUNK = [(0.505, 0.655, 0.575, 0.042), (0.420, 0.708, 0.500, 0.088), (0.250, 0.724, 0.556, 0.054),
         (0.090, 0.736, 0.508, 0.074), (-0.080, 0.746, 0.432, 0.100), (-0.220, 0.750, 0.402, 0.106),
         (-0.335, 0.738, 0.450, 0.088), (-0.410, 0.716, 0.535, 0.058)]


def torso_surface() -> G.SpineLoft:
    """The trunk as a ``SpineLoft`` running rump → forechest: angle 0 = the belly line, ±180 = the back line, +90 =
    the animal's left; s = chord fraction along the centre line."""
    pts = [Vector((0.0, y, 0.5 * (zt + zb))) for y, zt, zb, _ in TRUNK]
    d = [0.0]
    for a, b in zip(pts, pts[1:]):
        d.append(d[-1] + (b - a).length)
    rings = [(di / d[-1], hw, 0.5 * (zt - zb)) for di, (_, zt, zb, hw) in zip(d, TRUNK)]
    return G.SpineLoft(pts, rings, exp=2.35, fwd_dir=Vector((0, 0, -1)))


def _tag_bands(part: M.Part, bands) -> M.Part:
    """Face layer ``af_sub`` on a ``grid_loft`` part from its mean ``af_a``: ``bands`` = [(sub, predicate(|a|, u))]."""
    bm = part.bm
    sub = bm.faces.layers.int.get("af_sub") or bm.faces.layers.int.new("af_sub")
    la = bm.verts.layers.float["af_a"]
    lu = bm.verts.layers.float["af_u"]
    for f in bm.faces:
        sx = sum(math.sin(math.radians(v[la])) for v in f.verts)
        cx = sum(math.cos(math.radians(v[la])) for v in f.verts)
        a = abs(math.degrees(math.atan2(sx, cx)))
        u = sum(v[lu] for v in f.verts) / len(f.verts)
        f[sub] = next((s for s, pred in bands if pred(a, u)), 0)
    return part


def leg_tube(pts, radii, sides: int) -> M.Part:
    """A limb through ``pts`` with (lateral, fore-aft) radii, each segment split once (joints deform on a ring)."""
    P, Rr = [Vector(pts[0])], [radii[0]]
    for (a, ra), (b, rb) in zip(zip(pts, radii), zip(pts[1:], radii[1:])):
        a, b = Vector(a), Vector(b)
        P += [a.lerp(b, 0.5), b]
        Rr += [tuple((x + y) * 0.5 for x, y in zip(ra, rb)), rb]
    return M.tube(P, Rr, sides=sides, up=(1, 0, 0), dome0=0.0, dome1=0.0)


def build_body(pal: Palette, rig: R.Rig, lod: int = 0, ratio: float = 1.0) -> bpy.types.Object:
    """LOD0, or LOD1: the same generator with fewer sides / rings on every loft (``hi``), the sub-readability parts
    skipped (:data:`LOD1_SKIP`), a further collapse ``ratio`` only if that is still over budget, and every hull
    baked from a proxy decimated ``LOD1_HULL_RATIO`` × further (``goblins.LodHullBuilder``: hull ≤ 0.75 × toon)."""
    from kit import asset
    J = rig.joints
    if lod:
        import goblins as GB
        b = GB.LodHullBuilder(ASSET, pal, prefix="moonhound", lod_ratio=ratio,
                              lod_skip=lambda n: n.startswith(LOD1_SKIP), hull_ratio=LOD1_HULL_RATIO)
    else:
        b = M.Builder(ASSET, pal, prefix="moonhound")
    for full in SWATCHES:                                    # registered names and values (hangwood_palettes)
        b.swatch[full.split(".", 1)[1]] = b.swatch[full] = pal.add(full, HP.region(full))
    hi = not lod

    # trunk: coat, the pale belly line, the slate saddle
    T = torso_surface()
    trunk = T.loft(0.0, 1.0, rings=17 if hi else 12, sides=18 if hi else 12, dome0=0.02, dome1=0.03)
    _tag_bands(trunk, [(1, lambda a, u: a < 46.0), (2, lambda a, u: a > 138.0 and u > 0.06)])
    trunk_bind = Bind.blend(*SQ.SPINE, falloff=4.0, smooth=0.05)
    tid = b.add(trunk, "coat", trunk_bind, name="trunk", sub_regions={1: "belly", 2: "back"}, hull_pad=True)
    # ribs: five strokes a side, slanting back as they come down; the flank hollow before the thigh
    for sg in (1.0, -1.0):
        for k_ in range(5):
            s0 = 0.50 + 0.062 * k_
            path = [(s0 + 0.012, sg * 150.0), (s0, sg * 118.0), (s0 - 0.022, sg * 84.0), (s0 - 0.040, sg * 62.0)]
            b.add(G.surface_paint(T, path, 0.017, 0.0022, steps=6, taper=0.45), "coat_shadow", trunk_bind,
                  name="rib", outline=False, covered_by=tid)
        path = [(0.36, sg * 128.0), (0.345, sg * 98.0), (0.36, sg * 66.0)]
        b.add(G.surface_paint(T, path, 0.050, 0.0022, steps=5, taper=0.3), "coat_shadow", trunk_bind, name="rib",
              outline=False, covered_by=tid)

    # neck (carried low and forward), throat pale underneath
    nb, nh = J["neck_01"], J["head"]
    neck_pts = [Vector((0, nb.y + 0.045, 0.640)), Vector((0, nb.y - 0.045, 0.652)), Vector((0, J["neck_02"].y, 0.660)),
                Vector((0, nh.y + 0.015, 0.662)), Vector((0, nh.y - 0.050, 0.660))]
    neck_r = [(0.098, 0.066), (0.086, 0.058), (0.070, 0.050), (0.060, 0.046), (0.050, 0.042)]
    neck = M.tube(neck_pts, neck_r, sides=12 if hi else 8, up=(0, 0, 1), dome0=0.0, dome1=0.0)
    neck_bind = Bind.blend("spine_04", "neck_01", "neck_02", "head", falloff=4.0, smooth=0.035)
    nid = b.add(neck, "coat", neck_bind, name="neck", hull_pad=True)

    # head: a low flat skull, a long narrow muzzle, the lower jaw its own solid on ``jaw``
    hb = Bind.rigid("head")
    sk_c = Vector((0, nh.y - 0.050, 0.668))
    skull = M.ellipsoid((0.061, 0.074, 0.047), tuple(sk_c), sides=12 if hi else 8, rings=7 if hi else 5, exp=2.3)
    hid = b.add(skull, "coat", hb, name="skull", hull_pad=True)
    nose = J["nose"]
    mz = [Vector((0, sk_c.y - 0.030, 0.652)), Vector((0, sk_c.y - 0.095, 0.636)), Vector((0, nose.y + 0.045, 0.622)),
          Vector((0, nose.y + 0.006, 0.613))]
    muzzle = M.tube(mz, [(0.037, 0.044), (0.030, 0.034), (0.024, 0.027), (0.019, 0.021)], sides=10 if hi else 7,
                    up=(0, 0, 1), dome0=0.0, dome1=0.012)
    mid = b.add(muzzle, "coat", hb, name="muzzle", hull_pad=True)
    b.add(M.ellipsoid((0.017, 0.016, 0.014), (0, nose.y + 0.002, 0.622), sides=7, rings=4), "maw", hb, name="nose",
          outline=False, covered_by=mid)
    jb = Bind.rigid("jaw")
    jw, ch = J["jaw"], J["chin"]
    jaw_pts = [Vector((0, jw.y + 0.010, jw.z + 0.004)), Vector((0, jw.lerp(ch, 0.5).y, jw.lerp(ch, 0.5).z + 0.002)),
               Vector((0, ch.y, ch.z + 0.004))]
    jaw = M.tube(jaw_pts, [(0.019, 0.033), (0.015, 0.024), (0.011, 0.016)], sides=8 if hi else 6, up=(0, 0, 1),
                 dome0=0.0, dome1=0.008)
    jid = b.add(jaw, "belly", jb, name="jaw", hull_pad=True)
    # the lips' dark line down each side of the muzzle (the snarl), teeth
    for sg in (1.0, -1.0):
        lip = M.tube([Vector((sg * 0.036, sk_c.y - 0.040, 0.626)), Vector((sg * 0.030, sk_c.y - 0.100, 0.612)),
                      Vector((sg * 0.020, nose.y + 0.030, 0.600))], [(0.008, 0.004)] * 3, sides=4, up=(0, 0, 1))
        b.add(lip, "maw", hb, name="lip", outline=False, covered_by=mid)
        # upper fang and two side teeth (down), lower fang (up, on the jaw)
        fx_, fy = sg * 0.015, nose.y + 0.038
        b.add(G.cone(Vector((fx_, fy, 0.604)), Vector((fx_, fy - 0.003, 0.572)), 0.0065, sides=4), "teeth", hb,
              name="fang", outline=False, covered_by=mid)
        for j_ in range(2):
            y_ = nose.y + 0.075 + 0.036 * j_
            x_ = sg * (0.022 + 0.005 * j_)
            b.add(G.cone(Vector((x_, y_, 0.610 + 0.004 * j_)), Vector((x_, y_, 0.592 + 0.004 * j_)), 0.0055, sides=4),
                  "teeth", hb, name="tooth_side", outline=False, covered_by=mid)
        ly = ch.y + 0.022
        b.add(G.cone(Vector((sg * 0.012, ly, ch.z + 0.012)), Vector((sg * 0.012, ly + 0.002, ch.z + 0.040)), 0.0055,
                     sides=4), "teeth", jb, name="fang", outline=False, covered_by=jid)
    # eyes: silver-white shine in a near-black socket, high and forward on the skull (seen from above and ahead)
    for s, sg in SQ.SIDES:
        e = J[f"eye_{s}"]
        n = Vector((sg * 0.62, -0.40, 0.70)).normalized()
        m = G.frame_z(e + n * 0.006, n, Vector((0, -1, 0.25)))
        rim = M.ellipsoid((EYE_R[1] + 0.008, EYE_R[0] + 0.008, 0.006), (0, 0, -0.003), sides=10 if hi else 8, rings=4)
        b.add(rim.transform(m), "eye_rim", hb, name="eye_rim", outline=False, covered_by=hid)
        eye = M.ellipsoid((EYE_R[1], EYE_R[0], EYE_R[2]), (0, 0, 0.002), sides=10 if hi else 8, rings=4)
        b.add(eye.transform(m), "eye", hb, name="eye", outline=False, covered_by=hid)
        # the heavy lid: a slate wedge over the eye's upper half, slanted down toward the muzzle (the scowl; the
        # shine below it is a slit, not a disc)
        lid = M.ellipsoid((EYE_R[1] * 0.95, EYE_R[0] * 1.25, 0.006), (0, 0, 0.0), sides=8, rings=4)
        lid.transform(m @ Matrix.Translation((sg * EYE_R[1] * 1.02, EYE_R[0] * 0.10, 0.0045))
                      @ Matrix.Rotation(math.radians(sg * 17.0), 4, "Z"))
        b.add(lid, "back", hb, name="brow", outline=False, covered_by=hid)
    # ears: laid half back, one torn short
    for s, sg in SQ.SIDES:
        e0, e1 = J[f"ear_{s}"], J[f"ear_tip_{s}"]
        tip = e0.lerp(e1, 1.0 if s == "l" else 0.74)
        up = Vector((sg * 0.55, -0.8, 0.1))
        ear = M.tube([e0 + Vector((0, 0.004, -0.014)), e0.lerp(tip, 0.45), tip],
                     [(0.040, 0.012), (0.030, 0.009), (0.006 if s == "l" else 0.016, 0.004)], sides=6, up=tuple(up))
        eid = b.add(ear, "back", Bind.blend("head", f"ear_{s}", falloff=6.0), name="ear", hull_pad=True)
        inn = M.tube([e0.lerp(tip, 0.12), e0.lerp(tip, 0.5), e0.lerp(tip, 0.86)], [(0.024, 0.004), (0.017, 0.003),
                                                                                  (0.005, 0.002)], sides=4,
                     up=tuple(up))
        inn.translate(Vector((sg * 0.006, -0.008, 0.0)))
        b.add(inn, "ear_inner", Bind.rigid(f"ear_{s}"), name="ear_in", outline=False, covered_by=eid)

    # legs: thin and long; the shoulder and the thigh are flat muscle masses on the trunk's side
    sd = 9 if hi else 6
    for s, sg in SQ.SIDES:
        sh, el, wr, fb = (J[f"{n}_{s}"] for n in ("shoulder", "elbow", "wrist", "fore_ball"))
        fore_b = Bind.blend(f"foreleg_01_{s}", f"foreleg_02_{s}", f"foreleg_03_{s}", falloff=5.0, smooth=0.012)
        blade = M.ellipsoid((0.038, 0.085, 0.125), tuple(sh + Vector((sg * 0.006, 0.035, 0.045))), sides=8, rings=6)
        blade.rotate("x", -24.0, pivot=tuple(sh))
        b.add(blade, "coat", Bind.blend("spine_04", f"foreleg_01_{s}", falloff=3.0, smooth=0.05), name="shoulder",
              hull_pad=True)
        fore = leg_tube([sh + Vector((0, 0.01, 0.03)), el, wr, fb + Vector((0, 0.004, 0.006))],
                        [(0.038, 0.054), (0.031, 0.038), (0.023, 0.026), (0.021, 0.024)], sd)
        b.add(fore, "coat", fore_b, name="foreleg", hull_pad=True)
        hp, st, hk, hbl = (J[f"{n}_{s}"] for n in ("hip", "stifle", "hock", "hind_ball"))
        hind_b = Bind.blend(f"hindleg_01_{s}", f"hindleg_02_{s}", f"hindleg_03_{s}", falloff=5.0, smooth=0.012)
        thigh = leg_tube([hp + Vector((sg * -0.012, 0.02, 0.03)), hp.lerp(st, 0.55) + Vector((0, 0.02, 0.0)), st],
                         [(0.044, 0.105), (0.040, 0.082), (0.030, 0.044)], sd)
        b.add(thigh, "coat", Bind.blend("pelvis", f"hindleg_01_{s}", f"hindleg_02_{s}", falloff=4.0, smooth=0.03),
              name="thigh", hull_pad=True)
        hind = leg_tube([st + Vector((0, -0.01, 0.02)), hk, hbl + Vector((0, 0.004, 0.006))],
                        [(0.029, 0.042), (0.021, 0.027), (0.019, 0.022)], sd)
        b.add(hind, "coat", hind_b, name="hindleg", hull_pad=True)
        # painted shadow down the back of the hock (reads the zigzag), paws, claws
        for kind, ball, tip_n in (("fore", fb, "fore_tip"), ("hind", hbl, "hind_tip")):
            tb = Bind.rigid(f"{kind}toe_{s}")
            tp = J[f"{tip_n}_{s}"]
            paw = M.ellipsoid((0.031, 0.052, 0.024), (ball.x, ball.lerp(tp, 0.42).y, 0.0245), sides=8 if hi else 6,
                              rings=5 if hi else 4, exp=2.4)
            pid = b.add(paw, "coat", tb, name="paw", hull_pad=True)
            for c_ in (-1, 0, 1):
                c0 = Vector((ball.x + c_ * 0.016, tp.y + 0.012 + abs(c_) * 0.008, 0.016))
                b.add(G.cone(c0, c0 + Vector((c_ * 0.003, -0.022, -0.010)), 0.006, sides=4), "claw", tb, name="claw",
                      outline=False, covered_by=pid)

    # tail: straight, thin
    tl = [J[n] for n in SQ.TAIL] + [J["tail_tip"]]
    tail_pts = [tl[0] + Vector((0, -0.05, 0.004))] + [p + Vector((0, 0, 0.0)) for p in tl]
    tail = M.tube(tail_pts, [0.037, 0.033, 0.025, 0.017, 0.008], sides=8 if hi else 6, dome0=0.0)
    b.add(tail, "back", Bind.blend("pelvis", *SQ.TAIL, falloff=5.0, smooth=0.02), name="tail", hull_pad=True)

    # the temple collar: a torn leather band, a hanging end, the tarnished Well-Ring under the throat
    cb = Bind.rigid("neck_01")
    t = (neck_pts[2] - neck_pts[1]).normalized()
    c0 = neck_pts[1].lerp(neck_pts[2], 0.55)
    band = M.tube([c0 - t * 0.026, c0 + t * 0.026], [(0.088, 0.062), (0.084, 0.059)], sides=12 if hi else 8,
                  up=(0, 0, 1), dome0=0.0, dome1=0.0)
    cid = b.add(band, "collar", cb, name="collar", hull_pad=True)
    end = M.box((0.022, 0.007, 0.085), (0.0, 0.0, -0.040), bevel=0.0)
    end.transform(Matrix.Translation(c0 + Vector((0.045, -0.004, -0.062))) @ Matrix.Rotation(math.radians(-24), 4, "Y"))
    b.add(end, "collar", cb, name="collar_end", hull_pad=True)
    # the Well-Ring: lashed on the collar over the left shoulder, lying on the neck (seen from above: the game
    # camera never sees under the throat; the ``collar`` socket, where a chain hangs, stays under it)
    rn = Vector((0.62, 0.0, 0.78)).normalized()
    ring_c = c0 + Vector((0.062 * rn.x, -0.004, 0.092 * rn.z)) + rn * 0.006
    ru, rv_ = t.copy(), rn.cross(t).normalized()
    rr, n_seg = 0.030, 10 if hi else 7
    circ = [ring_c + ru * (rr * math.cos(2 * math.pi * i / n_seg)) + rv_ * (rr * math.sin(2 * math.pi * i / n_seg))
            for i in range(n_seg)]
    ring = M.sweep(circ, M.superellipse(0.0075, 0.0075, 5), closed_path=True, up=tuple(rn))
    b.add(ring, "ring", cb, name="well_ring", hull_pad=True)
    del cid
    del nid

    body = asset.finish_mesh(b, pal, OUTLINE_CLASS, rig=rig, grounding_height=WITHERS,
                             name=f"{ASSET}_LOD{lod}" if lod else None)
    body["af_lod"] = lod
    return body


# ── poses / clips ──────────────────────────────────────────────────────────────────────────────────────────
def ready() -> QP:
    """READY, the stand: the head low and a little to one side, the weight even, the tail straight out."""
    return QP(hip=(0.0, 0.0, -0.012), bend=5.0, neck=(4.0, 7.0), head=(-2.0, 4.0, 0.0), ears=(-6.0, -6.0),
              tail=(0.0, -4.0, 0.0),
              fl=(0.050, 0.03, 0.0), fr=(-0.030, -0.015, 0.0), bl=(0.030, -0.02, 0.0), br=(-0.030, 0.02, 0.0))


def ground_probes(rig: R.Rig) -> tuple:
    """The body's volume for the clips' ground clamp (``QuadAnims.probes``): the trunk's stations as ellipses on
    the nearest spine bone, the shoulder blades and thighs, skull, muzzle, leg joints, paws and tail."""
    J = rig.joints
    out = []
    spine = [(n, rig.head(n).y, rig.tail(n).y) for n in SQ.SPINE]
    for y, zt, zb, hw in TRUNK:
        bone = min(spine, key=lambda b: abs(0.5 * (b[1] + b[2]) - y))[0]
        out.append((bone, (0.0, y, 0.5 * (zt + zb)), hw, 0.5 * (zt - zb)))
    for s, sg in SQ.SIDES:
        sh, hp = J[f"shoulder_{s}"], J[f"hip_{s}"]
        out.append(("spine_04", (sh.x + sg * 0.006, sh.y + 0.035, sh.z + 0.045), 0.038, 0.11))
        out.append((f"hindleg_01_{s}", tuple(hp.lerp(J[f"stifle_{s}"], 0.5)), 0.044, 0.09))
        for bone, nm, r in ((f"foreleg_02_{s}", "elbow", 0.036), (f"foreleg_03_{s}", "wrist", 0.026),
                            (f"foretoe_{s}", "fore_ball", 0.034), (f"foretoe_{s}", "fore_tip", 0.026),
                            (f"hindleg_02_{s}", "stifle", 0.040), (f"hindleg_03_{s}", "hock", 0.028),
                            (f"hindtoe_{s}", "hind_ball", 0.034), (f"hindtoe_{s}", "hind_tip", 0.026)):
            out.append((bone, tuple(J[f"{nm}_{s}"]), r, r))
        out.append((f"ear_{s}", tuple(J[f"ear_tip_{s}"]), 0.012, 0.012))
    nh = J["head"]
    out += [("head", (0.0, nh.y - 0.050, 0.668), 0.061, 0.047), ("head", (0.0, J["nose"].y + 0.01, 0.618), 0.022, 0.022),
            ("jaw", tuple(J["chin"]), 0.017, 0.013), ("neck_01", (0.0, J["neck_02"].y, 0.655), 0.060, 0.085),
            ("tail_01", tuple(J["tail_02"]), 0.033, 0.033), ("tail_02", tuple(J["tail_03"]), 0.025, 0.025),
            ("tail_03", tuple(J["tail_tip"]), 0.010, 0.010)]
    return tuple(out)


def anims(rig: R.Rig) -> QuadAnims:
    A = QuadAnims(rig, ready(), run_speed=RUN_SPEED, walk_speed=WALK_SPEED)
    A.probes = ground_probes(rig)
    return A


def clips(A: QuadAnims) -> list:
    return [A.idle(1600.0), A.idle_sniff(2400.0), A.walk(700.0), A.run(460.0), A.attack(800.0, 320.0),
            A.pounce(1000.0, 380.0, 650.0), A.hurt(280.0), A.death(900.0), A.stun(700.0)]


def portrait_pose(A: QuadAnims) -> QP:
    """Portrait: the head turned to the viewer over the shoulder line, lifted a little, the lip up."""
    return A.ready().but(neck=(-14.0, 18.0), head=(-4.0, 16.0, -6.0), jaw=10.0, ears=(-18.0, -30.0))


# ── build / review / ship ──────────────────────────────────────────────────────────────────────────────────
def build_rig() -> R.Rig:
    return SQ.build_quadruped(SQ.QuadrupedSpec(name=ASSET, withers=WITHERS))


def build_all(lod1: bool = False) -> dict:
    scene.reset()
    hangwood_stage()
    pal = Palette(FAMILY)
    rig = build_rig()
    body = build_body(pal, rig)
    body1 = build_lod1(pal, rig) if lod1 else None
    return dict(pal=pal, rig=rig, body=body, body1=body1)


def build_lod1(pal, rig):
    from mship import toon_tris
    ratio = 1.0
    body1 = build_body(pal, rig, 1, ratio)
    n = toon_tris(body1)
    for _ in range(6):
        if n <= LOD1_MAX:
            break
        me = body1.data
        bpy.data.objects.remove(body1, do_unlink=True)
        bpy.data.meshes.remove(me)
        ratio *= (LOD1_MAX - 100) / n
        body1 = build_body(pal, rig, 1, ratio)
        n = toon_tris(body1)
    print(f"[lod1] {ASSET}: {n} toon tris at ratio {ratio:.3f}")
    return body1


def _review(B, out: Path):
    import mship
    rv = mship.MReview(ASSET, B["rig"].obj, [B["body"]], CROP_H, blob_radius=BLOB, out_dir=out,
                       game_outline_px=OUTLINE_PX, outline_class=OUTLINE_CLASS)
    rv.gate = mship.gate_for(OUTLINE_CLASS, OUTLINE_PX)
    rv.ground_occludes = True
    return rv


def quick(out: Path, pose: str | None = None) -> None:
    """Mesh iteration: close-ups (se, side, front, ne), the turnaround and the 1080p game crop → scratch. ``pose``:
    a key pose name (bite, crouch, lie …) instead of READY."""
    from mship import toon_tris
    B = build_all()
    A = anims(B["rig"])
    out.mkdir(parents=True, exist_ok=True)
    q = A.ready()
    if pose:
        named = {**A.bite_poses(), **A.death_poses()}
        q = named[pose]
    A.apply(q)
    print(f"[tris] body {toon_tris(B['body'])} toon / {M.tri_count(B['body'], 1)} hull; bones "
          f"{len(B['rig'].obj.data.bones)}; reach error {A.reach_error * 1000:.1f} mm")
    rv = _review(B, out)
    for fac, pitch in (("se", -35.0), ("side", -12.0), ("front", -30.0), ("ne", -30.0)):
        rv.closeup(fac, pitch=pitch)
        (out / "closeup.png").rename(out / f"closeup_{fac}.png")
    rv.turnaround()
    rv.game_view()
    for p, s in rv.written:
        print(f"  {p.name} {s // 1024} KB")


def gait_report(A: QuadAnims, cs) -> dict:
    out = {}
    for c in cs:
        if getattr(c, "duty_of", None):
            out[c.name] = round(A.slide_report(c) * 1000.0, 2)
    return out


def anim_quick(out: Path, names, facing: str = "se") -> None:
    import mship
    B = build_all()
    A = anims(B["rig"])
    cs = clips(A) + [A.howl()]
    print("[gait] largest planted-paw slide (mm):", gait_report(A, cs))
    cs = [c for c in cs if not names or c.name in names]
    mship.anim_review(B["rig"], [B["body"]], [], cs, TOKEN, out, CROP_H, OUTLINE_CLASS, blob_radius=BLOB,
                      facing=facing, px=OUTLINE_PX)


def pose_sheet(out: Path, specs, facing: str = "side", pitch: float = -12.0, cell=(300, 250), cols: int = 5,
               name: str = "poses") -> None:
    """Dev: one sheet of clip moments at one scale. ``specs`` = ["Clip@ms", …] (or a key pose name)."""
    from kit import review
    B = build_all()
    A = anims(B["rig"])
    cs = {c.name: c for c in clips(A) + [A.howl()]}
    named = {**A.bite_poses(), **A.death_poses()}
    qs = []
    for sp in specs:
        if "@" in sp:
            n, ms_ = sp.split("@")
            qs.append((sp, cs[n].pose_fn(float(ms_))))
        else:
            qs.append((sp, named[sp]))
    out.mkdir(parents=True, exist_ok=True)
    rv = _review(B, out)
    rv.facing(facing)
    pts = []
    for _, q in qs:
        A.apply(q)
        pts.append(review.mesh_points([B["body"]], step=4))
    pts = np.concatenate(pts)
    review.setup_render(cell, 6)
    review.frame_points(pts, cell, fov_h=24.0, pitch_ue=pitch, fill=0.94)
    rv.ink(at=Vector(pts.mean(0)))
    cells = []
    for label, q in qs:
        A.apply(q)
        img = review.render_array()
        review.draw_text(img, 3, 3, label.upper())
        cells.append(img)
    while len(cells) % cols:
        cells.append(np.full_like(cells[0], 24))
    from kit import pngio
    pngio.write_png(out / f"{name}_{facing}.png", review.grid(cells, cols))
    print(f"  {out / (name + '_' + facing + '.png')}")


class _Ship:
    """The anims holder ``mship`` asks for (``.rig``)."""

    def __init__(self, A):
        self.rig, self.A = A.rig, A


def portrait(ms, rv, actions) -> str:
    """``T_UI_Portrait_MoonHound`` (512² RGBA, head and neck, toon + screen-space ink; no eye cards: the eye-shine
    is not emissive). ``mship.portrait_shot`` with the pose applied by the quadruped solver."""
    import mship
    A = ms.A.A
    orig = mship.apply_bp
    mship.apply_bp = lambda _a, bp: A.apply(bp)
    try:
        return mship.portrait_shot(ms, rv, portrait_pose(A), PORTRAIT, eye_glow=None, yaw=-140.0,
                                   bust_below_head=0.17, bust_radius=0.30, pitch=-16.0)
    finally:
        mship.apply_bp = orig


def qa_setup(lod1: bool = False):
    import mship
    B = build_all(lod1=lod1)
    rig, body = B["rig"], B["body"]
    A = anims(rig)
    cs = clips(A)
    slide = gait_report(A, cs)
    print("[gait] largest planted-paw slide (mm):", slide)
    assert all(v <= 2.0 for v in slide.values()), f"a planted paw slides (> 2 mm): {slide}"
    f = scene.ms_to_frame
    attack = next(c for c in cs if c.name == "Attack01")
    pounce = next(c for c in cs if c.name == "Pounce")
    cn = {c.name: {d["name"]: d["ms"] for d in c.notify_list()} for c in cs}
    A.solve(attack.pose_fn(cn["Attack01"]["Contact"]))
    bite_at = _socket_cm(A, attack.pose_fn(cn["Attack01"]["Contact"]), "bite")
    pounce_at = _socket_cm(A, pounce.pose_fn(cn["Pounce"]["Contact"]), "bite")
    fx = {"telegraph": {"normal": "#FFE8DC", "elite": "#D11F2E", "untilMs": cn["Attack01"]["WindupEnd"],
                        "note": "dark remake: bone fill / crimson rim (monsters.md §1.2)"},
          "eyeShine": {"color": HP.MOON_SILVER, "emissive": 0.0, "swatch": "moonbitten.eye",
                       "sockets": ["fx_eye_l", "fx_eye_r"],
                       "note": "the Moon-Bitten's marker (chapter2.md §4.1): silver-white, NOT emissive — no eye "
                               "card, no bloom in game; the sockets are for portraits and a later variant"},
          "contactReach": {"attackRangeTiles": 1.3, "biteCm": bite_at,
                           "note": "Attack01 Contact 320: the jaws shut here (forward, right, up cm from the root)"},
          "pounce": {"clip": "Pounce", "skill": "pounce", "leapMs": cn["Pounce"]["Leap"],
                     "contactMs": cn["Pounce"]["Contact"], "shape": {"kind": "line", "lengthTiles": 4, "widthTiles": 1.2},
                     "dash": True, "biteCm": pounce_at,
                     "recipes": {"leap": "notify.FX_PounceLeap", "land": "notify.FX_PounceLand"},
                     "note": "in place: the core dashes the actor to the line's end; airborne from Leap to Contact "
                             "(the body's arc peaks ~33 cm over the stand)"},
          "blobShadow": {"radiusCm": round(BLOB * 100.0, 1), "note": "a long animal under a round blob: 40 cm covers "
                                                                     "the chest and the loin; the legs' contact is "
                                                                     "the ink"}}
    game_keys = [("Idle", 0, "se"), ("Walk", 10, "se"), ("Run", 6, "se"), ("Run", 20, "se"),
                 ("Attack01", f(cn["Attack01"]["WindupEnd"]), "se"), ("Attack01", f(320.0), "se"),
                 ("Pounce", f(380.0 * 0.84), "se"), ("Pounce", f(520.0), "se")]
    contact_keys = [("Pounce", f(650.0), "se"), ("Pounce", f(650.0), "sw"), ("Idle_Sniff", f(1000.0), "se"),
                    ("Hurt", f(60.0), "se"), ("Death", f(430.0), "se"), ("Death", 54, "se"), ("Stun", 20, "se"),
                    ("Run", 12, "side")]
    close_keys = [("Idle", 0, "side"), ("Walk", 10, "side"), ("Run", 2, "side"), ("Run", 16, "side"),
                  ("Attack01", f(cn["Attack01"]["WindupEnd"]), "se"), ("Attack01", f(320.0), "se"),
                  ("Pounce", f(320.0), "side"), ("Pounce", f(520.0), "side"), ("Pounce", f(650.0), "se"),
                  ("Death", f(430.0), "se"), ("Death", 54, "se"), ("Idle_Sniff", f(1000.0), "se"),
                  ("Hurt", f(60.0), "se"), ("Stun", 20, "front"), ("Death", 54, "front")]
    ms = mship.MonsterShip(
        asset=ASSET, token=TOKEN, preview_dir=PREVIEW, pal=B["pal"], A=_Ship(A), body=body, items=[], clips=cs,
        outline_class=OUTLINE_CLASS, game_ids=GAME_IDS, blob_radius=BLOB, height=CROP_H, lod1=B["body1"],
        display=DISPLAY, fx=fx,
        qa_limits={"ground": -0.5},
        outline_px=OUTLINE_PX, outline_px_note=OUTLINE_PX_NOTE, native_sheets=True, portrait=portrait,
        game_keys=game_keys, contact_keys=contact_keys, close_keys=close_keys,
        back_clips=("Attack01", "Pounce", "Death"),
        skeleton_bones_required=SQ.CORE_BONES + SQ.IK_BONES,
        lod_note="LOD1: the same generator with fewer sides and rings per loft (<= 2.5 k toon; eyes, collar and ring "
                 "kept; claws, side teeth, ribs, the lip lines and the inner ears are LOD0 "
                 "only).",
        extra={"refSpeedCmS": round(RUN_SPEED * 100.0, 1), "attackStyle": "bite", "family": "moonbitten",
               "role": "packHunter",
               "rig": {"family": "quadruped", "skeleton": SQ.SKELETON, "views": "two (a 3-D mesh: the actor turns)",
                       "animCategoryData": "beast",
                       "note": "the core's AnimRig (monster animCategory, anim_timing presets) has no 'quadruped': "
                               "'beast' is the existing preset that fits (hurt / death durations); the UE anim map "
                               "resolves this asset's clips by name (Pounce is new), not by category"},
               "locomotion": {"Walk": {"refSpeedCmS": round(WALK_SPEED * 100.0, 1), "gait": "trot",
                                       "footNotifies": ["FootFL", "FootFR", "FootBL", "FootBR"]},
                              "Run": {"refSpeedCmS": round(RUN_SPEED * 100.0, 1), "gait": "gallop",
                                      "footNotifies": ["FootFL", "FootFR", "FootBL", "FootBR"]},
                              "slideMm": slide,
                              "rule": "UE picks the clip whose ref speed is nearer the actor's (log scale) and plays "
                                      "it at v / ref: the hound at its data speed gallops at rate 1.0, a slowed one "
                                      "(< 2.2 m/s) trots"},
               "marker": {"family": "the Moon-Bitten", "swatches": list(MARKER),
                          "kind": "silver-white eye-shine #E6ECF5, non-emissive (chapter2.md §4.1)"},
               "abilities": {"pounce": {"clip": "Pounce", "leapMs": cn["Pounce"]["Leap"],
                                        "contactMs": cn["Pounce"]["Contact"], "windupEndMs": cn["Pounce"]["WindupEnd"],
                                        "dash": True,
                                        "travelMs": [cn["Pounce"]["Leap"], cn["Pounce"]["Contact"]],
                                        "travelNote": "the clip is airborne from Leap to Contact and stands from "
                                                      "Contact on (the fores planted where they strike): the "
                                                      "actor has to cover the dash line inside travelMs. The UE "
                                                      "charge slide today starts at the core's "
                                                      "EvEntityTeleported{Dash} (= Contact) and eases over 200 ms: "
                                                      "played that way the hound leaps on the spot and then skates "
                                                      "down the line on planted paws (HANDOFF 4U: slide the "
                                                      "visual from Leap to Contact along the telegraph's line, or "
                                                      "ask the core for the dash's start)"}},
               "remake": {"family": "moonbitten", "palette": FAMILY, "contactMs": cn["Attack01"]["Contact"],
                          "spec": "Docs/dark-remake/chapter2.md 4.4 moon_hound", "voiceFamily": "Hound"},
               "eliteMarker": {"asset": "SM_FX_EliteSigil", "socket": "fx_elite", "gameId": "elite_sigil",
                               "note": "the W2 sigil rides fx_elite (on root: it does not swing with the head), the "
                                       "W1 crown / nameplate / stun stars fx_overhead"},
               "variantsPlanned": {"hound_packleader": "scale 1.25, heavier shoulders, a grey saddle, a scarred "
                                                       "muzzle; + Cast01 (qclips.howl 1400 / Contact 1000)",
                                   "volgan": "SKEL_Quadruped at actor scale 2.2 with his own mesh and clips "
                                             "(qclips takes every length and beat as an argument)"},
               "skeletonNote": "SKEL_Quadruped (kit/skel_quadruped.py): 36 bones — root, pelvis + spine_01..04, "
                               "neck_01..02, head, jaw, ear_l/r, tail_01..03, foreleg_01..03 + foretoe and "
                               "hindleg_01..03 + hindtoe per side, ik_foot_root + ik_foot_fl/fr/bl/br (no weights)",
               "notes": "Clips in place; the core moves the actor (play rate = v / ref). Attack01 is one bite (the "
                        "jaws shut on Contact 320). Pounce = 扑 (crouch, Leap 380, Contact 650; the core dashes the "
                        "actor). Idle_Sniff is a one-shot idle variant. Death rolls onto its side and holds: the "
                        "corpse (remains). Stun loops. No Rise (nothing raises the Moon-Bitten). Eye-shine is a "
                        "palette swatch without emissive: it does not bloom."},
    )
    ms.extra["budget"] = _budget(ms)
    return ms


def _budget(ms) -> dict:
    import mship
    rec = {"spec": f"chapter2.md §17.1 moon_hound: LOD0 <= {LOD0_MAX} toon triangles, LOD1 <= {LOD1_MAX}; hull <= "
                   "0.75 x toon",
           "lod0": mship.tri_record(ms.body), "weightedBones": len({g.name for g in ms.body.vertex_groups}),
           "bones": len(ms.A.rig.obj.data.bones)}
    ok = mship.toon_tris(ms.body) <= LOD0_MAX and M.tri_count(ms.body, 1) <= 0.75 * mship.toon_tris(ms.body)
    if ms.lod1 is not None:
        rec["lod1"] = mship.tri_record(ms.lod1)
        ok = ok and mship.toon_tris(ms.lod1) <= LOD1_MAX and \
            M.tri_count(ms.lod1, 1) <= 0.75 * mship.toon_tris(ms.lod1)
    rec["pass"] = bool(ok)
    return rec


def _socket_cm(A: QuadAnims, q: QP, name: str) -> list:
    """(forward, right, up) cm from the root of a socket in a pose (the VFX offset convention)."""
    posed = A.solve(q)
    so = next(s for s in A.rig.sockets if s["name"] == name)
    p = posed[so["bone"]] @ (A.rest[so["bone"]].inverted() @ Vector(so["pos"]))
    return [round(-p.y * 100.0, 1), round(-p.x * 100.0, 1), round(p.z * 100.0, 1)]


def ship(previews: bool = True, verify: bool = True) -> int:
    import mship
    ms = qa_setup(lod1=True)
    assert ms.extra["budget"]["pass"], f"budget: {json.dumps(ms.extra['budget'])}"
    res = mship.ship(ms, previews=previews, verify=verify)
    return mship.main_result(res)


def portrait_quick(out: Path) -> None:
    ms = qa_setup()
    out.mkdir(parents=True, exist_ok=True)
    rv = _review(dict(rig=ms.A.rig, body=ms.body), out)
    portrait(ms, rv, [])
    print("  portrait written to", out, "(and Art/Export/Portraits)")


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    scratch = scratch_root("mq", "moon_hound")
    if "--quick" in args:
        k_ = args.index("--quick")
        rest = [a for a in args[k_ + 1:] if not a.startswith("--")]
        quick(scratch, rest[0] if rest else None)
    elif "--anims" in args:
        names_ = [a for a in args[args.index("--anims") + 1:] if not a.startswith("--")]
        fac = next((a.split("=")[1] for a in args if a.startswith("--facing=")), "se")
        anim_quick(scratch / "anims", names_, fac)
    elif "--poses" in args:
        specs_ = [a for a in args[args.index("--poses") + 1:] if not a.startswith("--")]
        fac = next((a.split("=")[1] for a in args if a.startswith("--facing=")), "side")
        pit = float(next((a.split("=")[1] for a in args if a.startswith("--pitch=")), "-12"))
        nm = next((a.split("=")[1] for a in args if a.startswith("--name=")), "poses")
        pose_sheet(scratch / "poses", specs_, fac, pit, name=nm)
    elif "--gait" in args:
        B_ = build_all()
        A_ = anims(B_["rig"])
        print("[gait] largest planted-paw slide (mm):", gait_report(A_, clips(A_)))
    elif "--portrait" in args:
        portrait_quick(scratch / "portrait")
    else:
        sys.exit(ship(previews="--no-previews" not in args, verify="--no-verify" not in args))

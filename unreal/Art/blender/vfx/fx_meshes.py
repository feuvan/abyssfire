"""FX meshes ``SM_FX_*`` for the pooled ISM particle runtime (spec art-inventory-ch1.md §8.4, DECISIONS P6, R8).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/vfx/fx_meshes.py [names…]
    #   --quick     look sheet of every mesh (close 3/4 + 1080p W1 crop with the warrior) → scratch, no export
    #   --no-previews

Every mesh is drawn by UE with ``M_AF_FX_Mesh`` (``fxkit``: one tint per particle, toon bands, ink hull, value
channel AF_Data.a, Bayer-dithered fade), so these are the *solid* toon pieces of the effects; light, smoke and
energy are the ``T_FX_*`` sprites (``fx_textures.py``). Axes: the mesh's forward is **Blender −Y = UE +Y**
(velocity layers align it with the flight; ground / upright layers yaw it, ``alignToBlow`` → along the blow).
Sizes are authored at the drawn size (``boundsDiameterCm`` in the manifest = what a recipe's ``sizeCm`` scales).

| Mesh | Used by (vfx_recipes.json) | Orient | Notes |
|---|---|---|---|
| SM_FX_Arrow | proj.arrow, multishot, piercing_arrow, arrow_rain | velocity | broadhead, 3 vanes; values head 1, shaft .6 |
| SM_FX_PoisonArrow | proj.poison_arrow | velocity | barbed head + venom bulb + drips |
| SM_FX_IceArrow | proj.ice_arrow | velocity | faceted crystal javelin (flat facets band per face) |
| SM_FX_IceShard | blizzard, ice_arrow.hit, freeze | velocity | hex bipyramid |
| SM_FX_Fireball | proj.fireball (+ meteor trail) | velocity | flat-lit flame-licked core (unshaded cel fire) |
| SM_FX_SpiritBolt | proj.pet_bolt, proj.monster_bolt (shadow) | velocity | flat-lit wisp with a curling tail |
| SM_FX_Rock_A / _B | debris (charge, stomp, traps, meteor) | billboard (tumbles) | faceted chips |
| SM_FX_MeteorRock | skill.meteor | velocity | faceted boulder, dark crack seams |
| SM_FX_HexRing | skill.shield_wall | ground | 8 hex frames on a 0.8 m ring (two-tone) |
| SM_FX_HexDome | skill.iron_fortress | ground | 6 + 6 hex frames, the upper ring leaning in |
| SM_FX_FrostNova | skill.ice_armor | ground | 9 + 9 crystals erupting outward |
| SM_FX_FrostCage | skill.freeze | ground | 7 tall shards leaning over the target |
| SM_FX_FrostCrust | status.freeze (R8) | ground | crystal ring at the feet |
| SM_FX_StunStar | status.stun (R8) | upright | puffy 5-point star facing the W1 camera at rotation 0 |
| SM_FX_ChainLink / SM_FX_Chain | skill.chain_trap | ground (dir out) | one link / 7 links from the pivot along +Y (UE) |
| SM_FX_SlashArc_Warrior / _Rogue / _Mage | slash, bleed_strike, backstab, hit.basic.<class> | ground (+ spin) | horizontal crescent ribbons round the pivot |
| SM_FX_WhirlBand | skill.whirlwind | ground (+ spin) | 1.25-turn rising blade ribbon |
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))

import bpy  # noqa: E402
import bmesh  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import outline, paths, pngio, review, scene, shading  # noqa: E402
from kit import mesh as M  # noqa: E402
import fxkit as K  # noqa: E402
from fxkit import FxBuilder  # noqa: E402


# ── geometry helpers ───────────────────────────────────────────────────────────────────────────────────
def bipyramid(length_front: float, length_back: float, radius: float, sides: int = 6, mid_len: float = 0.0,
              axis: str = "-y") -> M.Part:
    """Faceted crystal along −Y (tip at −length_front, back point at +length_back), ``mid_len`` prism between."""
    path = [(0, -length_front, 0), (0, -mid_len / 2, 0), (0, mid_len / 2, 0), (0, length_back, 0)] if mid_len > 0 \
        else [(0, -length_front, 0), (0, 0, 0), (0, length_back, 0)]
    sc = [0.0, 1.0, 1.0, 0.0] if mid_len > 0 else [0.0, 1.0, 0.0]
    prof = M.superellipse(radius, radius, sides, 2.0, math.pi / sides)
    p = M.sweep(path, prof, scales=[max(s, 1e-4) for s in sc], up=(0, 0, 1), cap0=None, cap1=None)
    bm = p.bm
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
    # close the degenerate tips: collapse rings with zero scale already merged by remove_doubles
    holes = [e for e in bm.edges if e.is_boundary]
    if holes:
        bmesh.ops.holes_fill(bm, edges=holes, sides=0)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return p.flat()


def crystal(height: float, radius: float, tip: float, sides: int = 6, base_sink: float = 0.02) -> M.Part:
    """Upright crystal: hex prism from z=−base_sink to ``height`` and a pointed tip ``tip`` above (flat facets)."""
    rings = [(-base_sink, radius, radius), (height, radius * 0.92, radius * 0.92)]
    p = M.loft_z(rings, sides, 2.0, cap0="ngon", cap1=None)
    bm = p.bm
    top = [v for v in bm.verts if abs(v.co.z - height) < 1e-6]
    c = bm.verts.new((0, 0, height + tip))
    ring = sorted(top, key=lambda v: math.atan2(v.co.y, v.co.x))
    for i in range(len(ring)):
        bm.faces.new((ring[i], ring[(i + 1) % len(ring)], c))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return p.flat()


def place(p: M.Part, origin, z_dir, y_hint=(0, 0, 1)) -> M.Part:
    """Rotate a part modelled along +Z so +Z → ``z_dir`` and translate to ``origin``."""
    z = Vector(z_dir).normalized()
    y = Vector(y_hint)
    x = y.cross(z)
    if x.length < 1e-6:
        x = Vector((1, 0, 0)).cross(z)
    x.normalize()
    y = z.cross(x)
    m = Matrix((x, y, z)).transposed().to_4x4()
    m.translation = Vector(origin)
    return p.transform(m)


def ribbon_solid(stations: list[tuple[Vector, Vector, Vector, float, float]]) -> M.Part:
    """Closed lens-section ribbon: per station (centre, radial dir, up dir, width, thickness) → 4 verts (inner,
    top, outer, bottom); capped ends collapse to the centre (sharp tips)."""
    bm = bmesh.new()
    rings = []
    for c, rad, up, w, t in stations:
        w = max(w, 1e-4)
        t = max(t, 1e-4)
        rings.append([bm.verts.new(c - rad * w * 0.5), bm.verts.new(c + up * t * 0.5),
                      bm.verts.new(c + rad * w * 0.5), bm.verts.new(c - up * t * 0.5)])
    for a, b in zip(rings[:-1], rings[1:]):
        for i in range(4):
            j = (i + 1) % 4
            bm.faces.new((a[i], a[j], b[j], b[i]))
    for ring, rev in ((rings[0], True), (rings[-1], False)):
        bm.faces.new(list(reversed(ring)) if rev else ring)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return M.Part(bm).auto_sharp(50)


def arc_stations(R: float, phi0: float, phi1: float, n: int, width, thick, z=lambda s: 0.0, r_off=lambda s: 0.0,
                 lo: float = 0.0, hi: float = 1.0):
    """Stations of a horizontal arc round the origin (φ from −Y, + toward +X), radial band [lo, hi] of the width."""
    out = []
    for i in range(n + 1):
        s = i / n
        phi = phi0 + (phi1 - phi0) * s
        rad = Vector((math.sin(phi), -math.cos(phi), 0.0))
        w = width(s)
        r_c = R + r_off(s) + (lo + hi - 1.0) * 0.5 * w
        c = rad * r_c + Vector((0, 0, z(s)))
        out.append((c, rad, Vector((0, 0, 1)), w * (hi - lo), thick(s)))
    return out


def hex_frame(r_out: float, width: float, thick: float) -> M.Part:
    """Hexagonal frame in the XZ plane (normal −Y), outer radius ``r_out`` (pointy top)."""
    rc = r_out - width / 2
    path = [(math.cos(math.pi / 2 + k * math.pi / 3) * rc, 0.0, math.sin(math.pi / 2 + k * math.pi / 3) * rc)
            for k in range(6)]
    prof = M.rect_profile(thick, width)
    p = M.sweep(path, prof, up=(0, -1, 0), closed_path=True)
    return p.flat()


def star_outline(r_out: float, r_in: float, n: int = 5, rot: float = math.pi / 2):
    pts = []
    for i in range(2 * n):
        a = rot + i * math.pi / n
        r = r_out if i % 2 == 0 else r_in
        pts.append((math.cos(a) * r, math.sin(a) * r))
    return pts


# ── meshes ─────────────────────────────────────────────────────────────────────────────────────────────
def build_arrow(kind: str = "steel"):
    name = "SM_FX_Arrow" if kind == "steel" else "SM_FX_PoisonArrow"
    b = FxBuilder(name, {"head": 1.0, "shaft": 0.6, "fletch": 0.9, "nock": 0.42, "wrap": 0.4, "venom": 0.96})
    shaft = M.tube([(0, 0.232, 0), (0, -0.165, 0)], [0.0115, 0.0115], sides=8)
    sid = b.add(shaft, "shaft", name="shaft", hull_pad=True)
    b.add(M.tube([(0, 0.252, 0), (0, 0.228, 0)], [0.0145, 0.0135], sides=8), "nock", name="nock")
    for y in (0.095, -0.135):
        b.add(M.tube([(0, y + 0.012, 0), (0, y - 0.012, 0)], [0.0125, 0.0125], sides=8, dome0=0, dome1=0),
              "wrap", name=f"wrap{y}", outline=False, covered_by=sid)
    vane = [(0.0, 0.222), (0.0, 0.112), (0.01, 0.102), (0.036, 0.14), (0.04, 0.214), (0.03, 0.232)]
    for k in range(3):
        v = M.plate([(z, y) for z, y in vane], 0.006, bevel=0.0015)        # XY plate: x = height, y = along
        v.transform(Matrix(((0, 0, 1, 0), (0, 1, 0, 0), (1, 0, 0, 0), (0, 0, 0, 1))))   # x → z (stand up)
        v.translate((0, 0, 0.0095))
        v.rotate("y", 120.0 * k)
        b.add(v, "fletch", name=f"vane{k}")
    if kind == "steel":
        head = M.plate([(0.0, -0.262), (0.044, -0.19), (0.047, -0.15), (0.013, -0.168), (0.013, -0.148),
                        (-0.013, -0.148), (-0.013, -0.168), (-0.047, -0.15), (-0.044, -0.19)], 0.014, bevel=0.004)
        b.add(head, "head", name="head")
        b.add(M.tube([(0, -0.14, 0), (0, -0.17, 0)], [0.0145, 0.011], sides=8), "head", name="ferrule")
    else:
        head = M.plate([(0.0, -0.262), (0.024, -0.215), (0.03, -0.196), (0.012, -0.205), (0.012, -0.19),
                        (-0.012, -0.19), (-0.012, -0.205), (-0.03, -0.196), (-0.024, -0.215)], 0.011, bevel=0.003)
        b.add(head, "head", name="head")
        b.add(M.ellipsoid((0.026, 0.036, 0.026), (0, -0.168, 0), sides=10, rings=7), "venom", name="bulb")
        for dx, dy, s in ((0.008, -0.172, 1.0), (-0.012, -0.152, 0.75)):
            drop = M.ellipsoid((0.008 * s, 0.008 * s, 0.014 * s), (dx, dy, -0.034 * s), sides=8, rings=5)
            b.add(drop, "venom", name=f"drip{dy}")
    obj = K.finish(b, 1.2, default_tint="#EEF3FB" if kind == "steel" else "#9AEC40")
    return obj, {"orient": "velocity", "pivot": "shaft centre (tip 25 cm ahead along UE +Y)",
                 "defaultTint": obj["af_default_tint"], "intensity": 1.0,
                 "recipes": ["proj.arrow", "proj.multishot", "proj.piercing_arrow", "ground.arrow_rain"]
                 if kind == "steel" else ["proj.poison_arrow"]}


def build_ice_arrow():
    b = FxBuilder("SM_FX_IceArrow", {"head": 1.0, "shaft": 0.84, "fin": 0.92})
    head = bipyramid(0.28 - 0.12, 0.07, 0.036, 6).translate((0, -0.12, 0))
    b.add(head, "head", name="head")
    shaft = M.loft_z([(0.0, 0.013, 0.013), (0.27, 0.011, 0.011)], 6, 2.0, cap0="ngon", cap1="ngon").flat()
    shaft.transform(Matrix(((1, 0, 0, 0), (0, 0, 1, 0), (0, -1, 0, 0), (0, 0, 0, 1))))   # +Z → −Y
    shaft.translate((0, -0.05, 0))
    b.add(shaft, "shaft", name="shaft")
    for k in range(3):
        fin = M.plate([(0.0, 0.25), (0.0, 0.14), (0.016, 0.13), (0.05, 0.205), (0.044, 0.25)], 0.008, bevel=0.0)
        fin.transform(Matrix(((0, 0, 1, 0), (0, 1, 0, 0), (1, 0, 0, 0), (0, 0, 0, 1))))
        fin.translate((0, 0, 0.008))
        fin.rotate("y", 120.0 * k + 60.0)
        b.add(fin.flat(), "fin", name=f"fin{k}")
    obj = K.finish(b, 1.5, default_tint="#CFF6FF", intensity=1.2)
    return obj, {"orient": "velocity", "pivot": "centre", "defaultTint": "#CFF6FF", "intensity": 1.2,
                 "recipes": ["proj.ice_arrow"]}


def build_ice_shard():
    b = FxBuilder("SM_FX_IceShard", {"shard": 1.0, "chip": 0.9})
    b.add(bipyramid(0.15, 0.08, 0.034, 6, mid_len=0.04), "shard", name="shard")
    chip = bipyramid(0.05, 0.03, 0.014, 5).rotate("z", 25).translate((0.026, 0.035, 0.004))
    b.add(chip, "chip", name="chip")
    obj = K.finish(b, 1.5, default_tint="#DDF8FF", intensity=1.1)
    return obj, {"orient": "velocity", "pivot": "widest point", "defaultTint": "#DDF8FF", "intensity": 1.1,
                 "recipes": ["skill.blizzard", "proj.ice_arrow.hit", "skill.freeze"]}


def flame_lick(start: Vector, back: float, swirl: float, r0: float, length: float, seed: int) -> M.Part:
    pts, rads = [], []
    n = 7
    for i in range(n + 1):
        s = i / n
        ang = swirl * s * 1.6
        off = Vector((math.cos(ang), 0, math.sin(ang))) * (r0 * 1.6 * s * s)
        p = start + Vector((start.x * 0.25 * s, back * length * s, start.z * 0.25 * s)) + off
        pts.append(p)
        rads.append(max(r0 * (1 - s) ** 1.1, 0.002))
    return M.tube(pts, rads, sides=7, dome0=0.0, dome1=0.0)


def hot_cap(radii, center, y_cut: float, grow: float = 1.03, sides: int = 14, rings: int = 9) -> M.Part:
    """The leading shell of an ellipsoid (faces in front of ``y_cut``, forward = −Y), ``grow`` × the radii so it
    rides 3–4 mm over the body: an inner value region with no ink of its own (the body's hull frames it)."""
    part = M.ellipsoid(tuple(r * grow for r in radii), center, sides=sides, rings=rings)
    bm = part.bm
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.calc_center_median().y > y_cut], context="FACES")
    return part


def build_fireball():
    # value ramp under an HDR tint (× 2.4): the cap clips to pale yellow and blooms, the body lands orange-yellow,
    # the licks orange → red-orange — the web's pre-coloured cel flame head, as one instance colour
    b = FxBuilder("SM_FX_Fireball", {"cap": 1.0, "core": 0.8, "lick": 0.64, "lick2": 0.55})
    radii, centre = (0.115, 0.125, 0.115), (0, -0.06, 0)
    b.add(M.ellipsoid(radii, centre, sides=14, rings=9), "core", name="core")
    b.add(hot_cap(radii, centre, -0.105), "cap", name="cap", outline=False)
    rnd = scene.rng("SM_FX_Fireball", "licks")
    for k in range(7):
        a = k / 7 * math.tau + rnd.uniform(-0.2, 0.2)
        start = Vector((math.cos(a) * 0.07, 0.0, math.sin(a) * 0.07))
        lick = flame_lick(start, 1.0, rnd.choice((-1, 1)) * rnd.uniform(1.4, 2.2), rnd.uniform(0.05, 0.066),
                          rnd.uniform(0.2, 0.3) if k % 2 else rnd.uniform(0.26, 0.36), k)
        b.add(lick, "lick" if k % 2 == 0 else "lick2", name=f"lick{k}")
    obj = K.finish(b, 1.0, flat_lit=True, default_tint="#FF9A3A", intensity=2.4)
    return obj, {"orient": "velocity", "pivot": "core centre (licks trail behind)", "defaultTint": "#FF9A3A",
                 "intensity": 2.4, "tintNote": "flat-lit + HDR: the leading cap (value 1) clips to pale yellow and "
                 "blooms, the body (.8) reads orange-yellow, the licks (.64 / .55) orange to red-orange",
                 "flatLit": True, "recipes": ["proj.fireball"],
                 "note": "normals all +Z: the toon maths lands the surface in the light band = unshaded cel fire; "
                         "glow / comet / embers are sprites around it"}


def build_spirit_bolt():
    b = FxBuilder("SM_FX_SpiritBolt", {"cap": 1.0, "head": 0.82, "tail": 0.66, "wisp": 0.56})
    radii, centre = (0.07, 0.085, 0.07), (0, -0.07, 0)
    b.add(M.ellipsoid(radii, centre, sides=12, rings=8), "head", name="head")
    b.add(hot_cap(radii, centre, -0.1, sides=12, rings=8), "cap", name="cap", outline=False)
    pts, rads = [], []
    for i in range(10):
        s = i / 9
        pts.append(Vector((math.sin(s * 5.0) * 0.045 * s, -0.04 + s * 0.30, math.cos(s * 5.0) * 0.03 * s - 0.01 * s)))
        rads.append(max(0.058 * (1 - s) ** 1.2, 0.002))
    b.add(M.tube(pts, rads, sides=8, dome0=0, dome1=0), "tail", name="tail")
    for k, (dx, dz) in enumerate(((0.05, 0.03), (-0.045, -0.02))):
        pts2 = [Vector((dx * 0.6, -0.04, dz * 0.6)), Vector((dx, 0.06, dz)), Vector((dx * 1.3, 0.16, dz * 1.4))]
        b.add(M.tube(pts2, [0.022, 0.014, 0.002], sides=6, dome0=0, dome1=0), "wisp", name=f"wisp{k}")
    obj = K.finish(b, 1.0, flat_lit=True, default_tint="#7FD2FF", intensity=1.9)
    return obj, {"orient": "velocity", "pivot": "head centre", "defaultTint": "#7FD2FF", "intensity": 1.9,
                 "tintNote": "the leading cap (value 1) clips toward white, head .82 / tail .66 / wisps .56 step "
                 "down through the tint", "flatLit": True, "recipes": ["proj.pet_bolt", "proj.monster_bolt"]}


def jitter_sphere(r: float, subdiv: int, seed, amp: float = 0.25, squash: float = 0.78) -> M.Part:
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=r)
    rnd = scene.rng(*seed)
    for v in bm.verts:
        k = 1.0 + rnd.uniform(-amp, amp)
        v.co = Vector((v.co.x * k, v.co.y * k, v.co.z * k * squash))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return M.Part(bm).flat()


def build_rock(variant: str):
    b = FxBuilder(f"SM_FX_Rock_{variant}", {"rock": 1.0})
    seed = ("SM_FX_Rock", variant)
    p = jitter_sphere(0.055 if variant == "A" else 0.05, 1, seed, 0.28, 0.72 if variant == "A" else 0.86)
    if variant == "B":
        p.scale((1.25, 0.85, 1.0))
    b.add(p, "rock", name="rock")
    obj = K.finish(b, 1.4, default_tint="#8A7358")
    return obj, {"orient": "billboard (tumbling rotation)", "pivot": "centre", "defaultTint": "#8A7358",
                 "intensity": 1.0, "recipes": ["debris preset (charge, war_stomp, explosive_trap, unyielding, meteor)"]}


def build_meteor():
    b = FxBuilder("SM_FX_MeteorRock", {"rock": 1.0})
    p = jitter_sphere(0.26, 2, ("SM_FX_MeteorRock",), 0.16, 0.92)
    b.add(p, "rock", name="rock")
    obj = K.finish(b, 1.4, default_tint="#8A6A5A")
    # molten seams: vertices near three crack planes get a low value (dark seams; the glow is the sprites')
    me = obj.data
    afd = me.color_attributes["AF_Data"]
    nv = len(me.vertices)
    data = np.zeros(nv * 4, np.float32)
    afd.data.foreach_get("color", data)
    data = data.reshape(nv, 4)
    co = np.zeros(nv * 3, np.float32)
    me.vertices.foreach_get("co", co)
    co = co.reshape(nv, 3)
    planes = [np.array(v) / np.linalg.norm(v) for v in ((1, 0.3, 0.4), (-0.2, 1, 0.5), (0.5, -0.6, 1))]
    body = data[:, 3] > 0.5
    for n in planes:
        d = np.abs(co @ n)
        data[body & (d < 0.028), 3] = 0.3
    afd.data.foreach_set("color", data.ravel())
    return obj, {"orient": "velocity", "pivot": "centre", "defaultTint": "#8A6A5A", "intensity": 1.0,
                 "recipes": ["skill.meteor"], "note": "dark crack seams (value .3) — the lava glow is Comet / Glow / "
                                                       "flame sprites"}


def build_hex_ring():
    b = FxBuilder("SM_FX_HexRing", {"rim": 1.0, "inner": 0.8})
    R = 0.80
    for k in range(8):
        a = k * math.pi / 4                          # UE angles 0, 45, … (orbit glow sprites use the same)
        for part, region in ((hex_frame(0.235, 0.056, 0.045), "rim"), (hex_frame(0.185, 0.024, 0.032), "inner")):
            part.rotate("x", -12.0)                   # top leans in (faces tilt up toward the camera)
            # frame normal −Y → outward radial (Blender: UE (cos a, sin a) → (cos a, −sin a))
            yaw = math.degrees(math.atan2(-math.sin(a), math.cos(a))) + 90.0
            part.rotate("z", yaw)
            part.translate((math.cos(a) * R, -math.sin(a) * R, 0.0))
            b.add(part, region, name=f"{region}{k}")
    obj = K.finish(b, 1.2, default_tint="#FFE27A", intensity=1.0)
    return obj, {"orient": "ground", "pivot": "ring centre at plate mid-height (layer height: chest)",
                 "defaultTint": "#FFE27A", "intensity": 1.0, "recipes": ["skill.shield_wall"],
                 "note": "frames at UE yaw 0/45/…: the orbit Hex glow sprites (count 8, r 80 cm) sit in them; "
                         "hollow so the hero stays visible"}


def build_hex_dome():
    b = FxBuilder("SM_FX_HexDome", {"low": 0.84, "high": 1.0, "lowin": 0.7, "highin": 0.85})
    for ring, (R, z, tilt, n, off, size, reg) in enumerate(((0.89, 0.0, -10.0, 6, 0.0, 0.26, "low"),
                                                           (0.62, 0.46, -45.0, 6, 30.0, 0.23, "high"))):
        for k in range(n):
            a = math.radians(off + k * 360.0 / n)
            for part, region in ((hex_frame(size, 0.05, 0.04), reg),
                                 (hex_frame(size - 0.045, 0.022, 0.03), reg + "in")):
                part.rotate("x", tilt)
                yaw = math.degrees(math.atan2(-math.sin(a), math.cos(a))) + 90.0
                part.rotate("z", yaw)
                part.translate((math.cos(a) * R, -math.sin(a) * R, z))
                b.add(part, region, name=f"{region}{ring}_{k}")
    obj = K.finish(b, 1.2, default_tint="#9FB4D4", intensity=1.0)
    return obj, {"orient": "ground", "pivot": "lower ring centre at plate mid-height (layer height: chest)",
                 "defaultTint": "#9FB4D4", "intensity": 1.0, "recipes": ["skill.iron_fortress"],
                 "note": "lower ring value .84 (web #7F97BB under #9FB4D4)"}


def build_frost_nova():
    b = FxBuilder("SM_FX_FrostNova", {"big": 1.0, "small": 0.88})
    rnd = scene.rng("SM_FX_FrostNova")
    for k in range(9):
        a = k / 9 * math.tau + rnd.uniform(-0.12, 0.12)
        h = rnd.uniform(0.26, 0.42)
        c = crystal(h, rnd.uniform(0.05, 0.065), rnd.uniform(0.09, 0.13), 6)
        c.rotate("z", rnd.uniform(0, 60))
        out = Vector((math.cos(a), -math.sin(a), 0))
        lean = math.radians(rnd.uniform(18, 30))
        place(c, out * 0.5, out * math.sin(lean) + Vector((0, 0, math.cos(lean))))
        b.add(c, "big", name=f"big{k}")
        a2 = a + math.pi / 9
        out2 = Vector((math.cos(a2), -math.sin(a2), 0))
        c2 = crystal(rnd.uniform(0.12, 0.18), rnd.uniform(0.03, 0.04), 0.06, 5)
        lean2 = math.radians(rnd.uniform(35, 48))
        place(c2, out2 * rnd.uniform(0.38, 0.6), out2 * math.sin(lean2) + Vector((0, 0, math.cos(lean2))))
        b.add(c2, "small", name=f"small{k}")
    obj = K.finish(b, 1.2, default_tint="#CFF6FF", intensity=1.15)
    return obj, {"orient": "ground", "pivot": "ground centre", "defaultTint": "#CFF6FF", "intensity": 1.15,
                 "recipes": ["skill.ice_armor"], "note": "pops in with sizePow 6 then shatters into Flake motes"}


def build_frost_cage():
    b = FxBuilder("SM_FX_FrostCage", {"a": 1.0, "b": 0.88})
    rnd = scene.rng("SM_FX_FrostCage")
    for k in range(7):
        a = k / 7 * math.tau + 0.2
        base = Vector((math.cos(a), -math.sin(a), 0)) * 0.46
        tip = Vector((math.cos(a + 0.5), -math.sin(a + 0.5), 0)) * 0.1 + Vector((0, 0, rnd.uniform(1.25, 1.45)))
        d = tip - base
        c = crystal(d.length - 0.16, rnd.uniform(0.05, 0.065), 0.16, 6)
        c.rotate("z", rnd.uniform(0, 60))
        place(c, base, d)
        b.add(c, "a" if k % 2 == 0 else "b", name=f"shard{k}")
    obj = K.finish(b, 1.3, default_tint="#CFF6FF", intensity=1.15)
    return obj, {"orient": "ground", "pivot": "ground centre under the target", "defaultTint": "#CFF6FF",
                 "intensity": 1.15, "recipes": ["skill.freeze"]}


def build_frost_crust():
    b = FxBuilder("SM_FX_FrostCrust", {"a": 1.0, "b": 0.86})
    rnd = scene.rng("SM_FX_FrostCrust")
    for k in range(8):
        a = k / 8 * math.tau + rnd.uniform(-0.25, 0.25)
        out = Vector((math.cos(a), -math.sin(a), 0))
        c = crystal(rnd.uniform(0.07, 0.15), rnd.uniform(0.028, 0.04), rnd.uniform(0.045, 0.07), 5)
        c.rotate("z", rnd.uniform(0, 72))
        lean = math.radians(rnd.uniform(15, 40))
        place(c, out * rnd.uniform(0.24, 0.36), out * math.sin(lean) + Vector((0, 0, math.cos(lean))))
        b.add(c, "a" if k % 2 == 0 else "b", name=f"c{k}")
    obj = K.finish(b, 1.45, default_tint="#BFEFFF", intensity=1.1)
    return obj, {"orient": "ground", "pivot": "ground centre (attached to the frozen monster)",
                 "defaultTint": "#BFEFFF", "intensity": 1.1, "recipes": ["status.freeze"]}


def build_stun_star():
    b = FxBuilder("SM_FX_StunStar", {"star": 1.0, "core": 1.0})
    st = M.plate(star_outline(0.085, 0.042), 0.034, bevel=0.012, segments=2)
    b.add(st, "star", name="star")
    # face the W1 camera at rotation 0 (yaw fixed, W1): the plate normal (+Z) → toward the camera
    obj = K.finish(b, 0.9, default_tint="#FFE27A", intensity=1.25)
    f = review.cam_dir()
    to_cam = (-f).normalized()
    rot = Vector((0, 0, 1)).rotation_difference(to_cam).to_matrix().to_4x4()
    obj.data.transform(rot)
    obj.data.update()
    return obj, {"orient": "upright (yaw only; rotation 0 faces the W1 camera)", "pivot": "centre",
                 "defaultTint": "#FFE27A", "intensity": 1.25, "recipes": ["status.stun"]}


def chain_link(length=0.1, width=0.055, wire=0.011) -> M.Part:
    n = 12
    path = []
    for i in range(n):
        a = i / n * math.tau
        x = math.cos(a) * width / 2
        y = math.sin(a) * length / 2
        y += math.copysign(0.0, y)
        path.append((x, y, 0.0))
    prof = M.superellipse(wire, wire, 6, 2.0)
    return M.sweep(path, prof, up=(0, 0, 1), closed_path=True)


def build_chain(single: bool):
    name = "SM_FX_ChainLink" if single else "SM_FX_Chain"
    b = FxBuilder(name, {"link": 1.0, "link2": 0.82})
    if single:
        b.add(chain_link(), "link", name="link")
    else:
        for k in range(7):
            l_ = chain_link()
            if k % 2:
                l_.rotate("y", 90)
            l_.translate((0, -0.04 - k * 0.075, 0))
            b.add(l_, "link" if k % 2 == 0 else "link2", name=f"link{k}")
    obj = K.finish(b, 0.9, default_tint="#A8B4C4")
    return obj, {"orient": "ground (dir out: yaw along the lash)", "pivot": "chain start (links run along UE +Y)"
                 if not single else "centre", "defaultTint": "#A8B4C4", "intensity": 1.0,
                 "recipes": ["skill.chain_trap"]}


def build_slash_arc(cls: str):
    name = f"SM_FX_SlashArc_{cls}"
    if cls == "Warrior":
        b = FxBuilder(name, {"edge": 1.0, "body": 0.74})
        R, span, n = 1.0, math.radians(150), 40
        width = lambda s: 0.27 * math.sin(math.pi * s) ** 0.75
        thick = lambda s: 0.03 * math.sin(math.pi * s) ** 0.5 + 0.004
        z = lambda s: (s - 0.5) * 0.24
        proxy = ribbon_solid(arc_stations(R, -span / 2, span / 2, n, width, thick, z))
        b.add(ribbon_solid(arc_stations(R, -span / 2, span / 2, n, width, thick, z, lo=0.0, hi=0.62)), "body",
              name="body", hull=proxy)
        b.add(ribbon_solid(arc_stations(R, -span / 2, span / 2, n, width, thick, z, lo=0.6, hi=1.0)), "edge",
              name="edge", outline=False)
        tint = "#F4AC28"
    elif cls == "Rogue":
        b = FxBuilder(name, {"edge": 1.0, "body": 0.82})
        span, n = math.radians(124), 34
        for k, (R, wmax, reg) in enumerate(((1.06, 0.075, "edge"), (0.9, 0.06, "body"))):
            width = (lambda wm: (lambda s: wm * math.sin(math.pi * s) ** 0.7))(wmax)
            thick = lambda s: 0.02 * math.sin(math.pi * s) ** 0.5 + 0.003
            z = (lambda kk: (lambda s: (s - 0.5) * 0.36 + kk * 0.02))(k)
            b.add(ribbon_solid(arc_stations(R, -span / 2 + k * 0.06, span / 2 + k * 0.06, n, width, thick, z)),
                  reg, name=f"blade{k}")
        tint = "#B478FF"
    else:   # Mage: a broad staff sweep with arcane sparkles riding it
        b = FxBuilder(name, {"edge": 1.0, "body": 0.8, "spark": 1.0})
        R, span, n = 0.95, math.radians(140), 36
        width = lambda s: 0.2 * math.sin(math.pi * s) ** 0.6
        thick = lambda s: 0.05 * math.sin(math.pi * s) ** 0.5 + 0.004
        z = lambda s: 0.0
        proxy = ribbon_solid(arc_stations(R, -span / 2, span / 2, n, width, thick, z))
        b.add(ribbon_solid(arc_stations(R, -span / 2, span / 2, n, width, thick, z, lo=0.0, hi=0.55)), "body",
              name="body", hull=proxy)
        b.add(ribbon_solid(arc_stations(R, -span / 2, span / 2, n, width, thick, z, lo=0.53, hi=1.0)), "edge",
              name="edge", outline=False)
        for k in range(5):
            s = 0.18 + 0.16 * k
            phi = -span / 2 + span * s
            rad = Vector((math.sin(phi), -math.cos(phi), 0))
            sz = 0.035 + 0.025 * math.sin(math.pi * s)
            stp = M.plate(star_outline(sz, sz * 0.32, 4, 0.0), 0.016, bevel=0.004)
            stp.translate(tuple(rad * (R - 0.16) + Vector((0, 0, 0.05))))
            b.add(stp, "spark", name=f"spark{k}")
        tint = "#D8B4FF"
    obj = K.finish(b, 1.2, flat_lit=True, default_tint=tint, intensity=1.25)
    return obj, {"orient": "ground (alignToBlow: the crescent's centre points along the blow; spinDegS sweeps it)",
                 "pivot": "the swing centre (the caster / the target chest)", "defaultTint": tint, "intensity": 1.25,
                 "flatLit": True, "class": cls.lower(),
                 "recipes": {"Warrior": ["skill.slash", "skill.bleed_strike", "hit.basic.warrior"],
                             "Rogue": ["skill.backstab", "hit.basic.rogue"], "Mage": ["hit.basic.mage"]}[cls]}


def build_whirl_band():
    """A rising spiral blade trail: thin tail near the body, widening to a bright blade head at radius 1."""
    b = FxBuilder("SM_FX_WhirlBand", {"edge": 1.0, "body": 0.72})
    turns, n = 1.25, 90
    span = turns * math.tau
    width = lambda s: 0.025 + 0.26 * s ** 1.6 * (1.0 if s < 0.95 else max(0.0, 1 - (s - 0.95) / 0.05) ** 0.6)
    thick = lambda s: 0.03 * min(1.0, s * 3) + 0.003
    z = lambda s: -0.16 + 0.34 * s + 0.04 * math.sin(s * math.tau * 1.25)
    r_off = lambda s: -0.5 * (1 - s) ** 1.4                       # spiral: radius 0.5 (tail) → 1.0 (head)
    proxy = ribbon_solid(arc_stations(1.0, 0.0, -span, n, width, thick, z, r_off))
    b.add(ribbon_solid(arc_stations(1.0, 0.0, -span, n, width, thick, z, r_off, lo=0.0, hi=0.6)), "body",
          name="body", hull=proxy)
    b.add(ribbon_solid(arc_stations(1.0, 0.0, -span, n, width, thick, z, r_off, lo=0.58, hi=1.0)), "edge",
          name="edge", outline=False)
    obj = K.finish(b, 1.2, flat_lit=True, default_tint="#AABBD4", intensity=1.25)
    return obj, {"orient": "ground (+ spinDegS ≈ 1500: the band whirls)", "pivot": "the caster's centre, chest height",
                 "defaultTint": "#AABBD4", "intensity": 1.25, "flatLit": True, "recipes": ["skill.whirlwind",
                                                                                          "skill.rampage"]}


BUILDERS = {
    "SM_FX_Arrow": lambda: build_arrow("steel"),
    "SM_FX_PoisonArrow": lambda: build_arrow("poison"),
    "SM_FX_IceArrow": build_ice_arrow,
    "SM_FX_IceShard": build_ice_shard,
    "SM_FX_Fireball": build_fireball,
    "SM_FX_SpiritBolt": build_spirit_bolt,
    "SM_FX_Rock_A": lambda: build_rock("A"),
    "SM_FX_Rock_B": lambda: build_rock("B"),
    "SM_FX_MeteorRock": build_meteor,
    "SM_FX_HexRing": build_hex_ring,
    "SM_FX_HexDome": build_hex_dome,
    "SM_FX_FrostNova": build_frost_nova,
    "SM_FX_FrostCage": build_frost_cage,
    "SM_FX_FrostCrust": build_frost_crust,
    "SM_FX_StunStar": build_stun_star,
    "SM_FX_ChainLink": lambda: build_chain(True),
    "SM_FX_Chain": lambda: build_chain(False),
    "SM_FX_SlashArc_Warrior": lambda: build_slash_arc("Warrior"),
    "SM_FX_SlashArc_Rogue": lambda: build_slash_arc("Rogue"),
    "SM_FX_SlashArc_Mage": lambda: build_slash_arc("Mage"),
    "SM_FX_WhirlBand": build_whirl_band,
}

# in-game context for the preview: (where it sits relative to the hero / target, height m, extra tints)
CONTEXT = {
    "SM_FX_Arrow": ("flight", 1.0, ["#EEF3FB", "#FFE27A"]),
    "SM_FX_PoisonArrow": ("flight", 1.0, ["#9AEC40"]),
    "SM_FX_IceArrow": ("flight", 1.0, ["#CFF6FF"]),
    "SM_FX_IceShard": ("fall", 1.2, ["#DDF8FF"]),
    "SM_FX_Fireball": ("flight", 1.0, ["#FF5A00"]),
    "SM_FX_SpiritBolt": ("flight", 1.0, ["#9CE0FF", "#B478FF"]),
    "SM_FX_Rock_A": ("debris", 0.3, ["#8A7358", "#8A8E98"]),
    "SM_FX_Rock_B": ("debris", 0.3, ["#8A7358", "#8A8E98"]),
    "SM_FX_MeteorRock": ("fall", 1.6, ["#8A6A5A"]),
    "SM_FX_HexRing": ("hero", 0.95, ["#FFE27A"]),
    "SM_FX_HexDome": ("hero", 0.95, ["#9FB4D4"]),
    "SM_FX_FrostNova": ("hero", 0.0, ["#CFF6FF"]),
    "SM_FX_FrostCage": ("target", 0.0, ["#CFF6FF"]),
    "SM_FX_FrostCrust": ("target", 0.0, ["#BFEFFF"]),
    "SM_FX_StunStar": ("overhead", 1.15, ["#FFE27A"]),
    "SM_FX_ChainLink": ("debris", 0.4, ["#A8B4C4"]),
    "SM_FX_Chain": ("ground", 0.15, ["#A8B4C4", "#FFE27A"]),
    "SM_FX_SlashArc_Warrior": ("hero", 1.0, ["#F4AC28", "#E8342C"]),
    "SM_FX_SlashArc_Rogue": ("target", 0.55, ["#B478FF", "#7030C0"]),
    "SM_FX_SlashArc_Mage": ("target", 0.55, ["#D8B4FF"]),
    "SM_FX_WhirlBand": ("hero", 1.0, ["#AABBD4"]),
}


# ── previews ───────────────────────────────────────────────────────────────────────────────────────────
GATE_PX = 1.0


def fx_gate(obj) -> dict:
    """Ink gate of an FX mesh: the baked hull alone (M_AF_FX_Mesh has no outline WPO) must give ≥ baked − 0.5 px
    at 1080p (FX ink ≈ 1.1–1.7 px: the web's cel props carry ~1 px line art) on ≥ 90 % of the silhouette edges,
    with an ink pixel (lum < 40) within 3 px."""
    px = float(obj["af_outline_width"]) * 100.0 / K.PX_CM
    return {"class": "fx", "minPx": round(max(0.6, px - 0.5), 2), "minFrac": 0.9, "darkLum": 40.0,
            "darkWithinPx": 3, "maxHullLumP95": 40.0}


RIGHT = Vector((-0.70711, -0.70711, 0.0))       # screen-right on the ground under the W1 camera (Blender axes)
INTO = Vector((0.70711, -0.70711, 0.0))         # screen-up on the ground (away from the camera)
HERO_AT = Vector((0.0, 0.0, 0.0))
TARGET_AT = RIGHT * 1.7 + INTO * 0.12


def yaw_to(d: Vector) -> float:
    """Blender Z rotation that turns a −Y-facing mesh toward the ground direction ``d``."""
    return math.atan2(d.x, -d.y)


def scale_scene():
    """The warrior (facing the goblin) and a goblin target for scale, both with their shipped ink."""
    blow = (TARGET_AT - HERO_AT).normalized()
    w = K.import_warrior("Idle", 0, at=tuple(HERO_AT), yaw_deg=math.degrees(yaw_to(blow)))
    g = K.import_goblin("Idle", 0, at=tuple(TARGET_AT), yaw_deg=math.degrees(yaw_to(-blow)))
    chars = (w[1] if w else []) + (g[1] if g else [])
    for m in chars:
        review.set_outline_px([m], outline.SCREEN_PX_1080[m.get("af_outline_class", "monster")])
    return chars


def context_shot(obj, name: str) -> np.ndarray:
    """Native 1080p W1 crop: the mesh where it lives in the game, next to the warrior and a goblin target, in
    its default tint and the recipes' other tints."""
    kind, h, tints = CONTEXT[name]
    review.stage(radius=40.0, blob_radius=0.0)
    chars = scale_scene()
    blow = (TARGET_AT - HERO_AT).normalized()
    review.setup_render((1920, 1080), 8)
    mid = (HERO_AT + TARGET_AT) * 0.5
    cam = review.game_camera((mid.x, mid.y))
    c = review.project(cam, Vector((mid.x, mid.y, 0.75)))
    hw, hh = 520 / 1920 / 2, 360 / 1080 / 2
    box = (c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh)
    cells = []
    for i, t in enumerate(tints):
        o = obj
        o.rotation_euler = (0, 0, 0)
        if kind == "flight":
            o.location = tuple(HERO_AT.lerp(TARGET_AT, 0.45) + Vector((0, 0, h)) + INTO * 0.0)
            o.rotation_euler = (0, 0, yaw_to(blow))
        elif kind == "fall":
            o.location = tuple(TARGET_AT + INTO * 0.35 + Vector((0, 0, h)))
            o.rotation_euler = (math.radians(55), 0, yaw_to(blow))
        elif kind == "debris":
            o.location = tuple(TARGET_AT - RIGHT * 0.45 + Vector((0, 0, h)))
            o.rotation_euler = (0.5, 0.8, 0.3)
        elif kind == "hero":
            o.location = tuple(HERO_AT + Vector((0, 0, h)))
            o.rotation_euler = (0, 0, yaw_to(blow))
        elif kind == "target":
            o.location = tuple(TARGET_AT + Vector((0, 0, h)))
            o.rotation_euler = (0, 0, yaw_to(blow))
        elif kind == "overhead":
            o.location = tuple(TARGET_AT + Vector((0.12, 0.18, h)))
        elif kind == "ground":
            o.location = tuple(HERO_AT + RIGHT * 0.4 + Vector((0, 0, h)))
            o.rotation_euler = (0, 0, yaw_to(blow))
        bpy.context.view_layer.update()
        K.set_fx_look(o, K.tint_linear(t, obj["af_intensity"]), 1.0)
        img = review.render_array(box)
        review.draw_text(img, 4, 4, f"{name}  W1 1080P 1:1  TINT {t}" if i == 0 else f"TINT {t}")
        cells.append(img)
    obj.location = (0, 0, 0)
    obj.rotation_euler = (0, 0, 0)
    for m in chars:
        m.hide_render = True
    K.preview_mats(obj)
    return review.grid(cells, len(cells), pad=2)


def previews(obj, name: str) -> tuple[list[str], dict]:
    out_dir = paths.preview_dir(name)
    out_dir.mkdir(parents=True, exist_ok=True)
    for f in out_dir.glob("*.png"):
        f.unlink()
    lo, hi = scene.world_bounds([obj])
    rv = review.Review(name, obj, [obj], max(hi.z - lo.z, 0.15), blob_radius=0.0, game_outline_px=None,
                       outline_class="decor")
    rv.bloom = False
    rv.gate = fx_gate(obj)
    K.preview_mats(obj)
    # FX meshes float at their layer height: lift the preview copy off the floor (hull under z = 0 is not ink)
    z0 = -lo.z + (0.4 if hi.z - lo.z < 0.5 else 0.05)
    obj.location = (0, 0, z0)
    bpy.context.view_layer.update()
    rv.game_view("front")
    rv.closeup("se", res=(420, 420), pitch=-30.0)
    rv.turnaround(cell=(200, 200))
    v = rv.ink_verdict()
    obj.location = (0, 0, 0)
    ctx = context_shot(obj, name)
    p = out_dir / "game_context.png"
    pngio.write_png_budget(p, ctx)
    rv.written.append((p, p.stat().st_size))
    ink = {k: v[k] for k in ("pass", "cells", "minFracRim", "minFracDark", "medianRimPx", "maxHullLumP95", "gate")}
    print(f"[ink] {name}: rim {v['minFracRim'] * 100:.1f} % >= {rv.gate['minPx']} px, dark "
          f"{v['minFracDark'] * 100:.1f} %, median {v['medianRimPx']} px -> {'PASS' if v['pass'] else 'FAIL'}")
    rel = [str(pp.relative_to(paths.preview_root().parent)) for pp, _ in rv.written]
    return rel, ink


def quick(names) -> None:
    """Look sheet: every mesh at a close 3/4 + in context → scratch."""
    K.SCRATCH.mkdir(parents=True, exist_ok=True)
    cells = []
    for name in names:
        scene.reset()
        obj, fx = BUILDERS[name]()
        K.preview_mats(obj)
        review.stage(radius=10.0, blob_radius=0.0)
        lo, hi = scene.world_bounds([obj])
        obj.location = (0, 0, -lo.z + 0.05)
        bpy.context.view_layer.update()
        review.setup_render((300, 300), 8)
        pts = review.mesh_points([obj])
        review.frame_points(pts, (300, 300), fov_h=30, pitch_ue=-30, fill=0.85)
        a = review.render_array()
        review.draw_text(a, 3, 3, name.replace("SM_FX_", ""))
        obj.location = (0, 0, 0)
        cells.append(a)
        ctx = context_shot(obj, name)
        cells.append(ctx[:300, :min(ctx.shape[1], 440)])
    sheet = review.grid([np.pad(c, ((0, 300 - c.shape[0]), (0, 440 - c.shape[1]), (0, 0))) for c in cells], 4, pad=2)
    p = K.SCRATCH / "meshes_quick.png"
    pngio.write_png_budget(p, sheet, 900 * 1024)
    print(p)


def main(argv) -> int:
    names = [a for a in argv if not a.startswith("--")] or list(BUILDERS)
    if "--quick" in argv:
        quick(names)
        return 0
    fails = []
    for name in names:
        scene.reset()
        obj, fx = BUILDERS[name]()
        fx["values"] = json.loads(obj["af_values"])
        prev, ink = ([], None)
        if "--no-previews" not in argv:
            prev, ink = previews(obj, name)
            if not ink["pass"]:
                fails.append(name)
        entry = K.ship(obj, fx, prev, ink)
        entry_fx = entry["fx"]
        print(f"{name}: {entry['triangles']} tris, diameter {entry['boundsDiameterCm']} cm, {entry_fx['orient']}")
    if fails:
        print("INK GATE FAILED: " + ", ".join(fails))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

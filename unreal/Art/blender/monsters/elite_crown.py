"""Elite marker — ``SM_FX_EliteCrown`` (spec art-inventory-ch1.md §4.6; web ``effects/Pickups.ts:50-67``).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/elite_crown.py [--quick]

A small floating gold crown (≈ 31 cm wide at the points, the web's 16 px), tilted back 38° so the camera-yawed crown is seen
nearly front-on from the W1 camera, over every elite (the chief, the shaman, the hunt leaders): the web / spec
three-point crown (``Pickups.ts:50-67``) — a front arc whose top edge is the web polygon (a tall centre point and
two side points with straight edges), a low plain band round the back (never seen: the crown always faces the
camera's yaw), low-poly ball tips in the gold light tone, a dark gold rule across the front and a diamond-cut ruby
``#FF3A4A`` (emissive glint) on a dark ``#7A1020`` setting. ≈ 540 toon + 340 hull tris. Pivot = the lowest point of
the tilted base; it rides ``fx_overhead`` with a slow bob and sway and always faces the camera's yaw (UE side,
manifest ``motion``); the warm glow ``#FFD040`` α .4 is a runtime card (``fx.glow``). Monster-class ink (the weight
of the elite it marks; a 2 px line tone lost its dark core at this size).
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE.parent / "heroes"))
sys.path.insert(0, str(HERE))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import mesh as M, outline, review, scene, shading  # noqa: E402
from kit.paths import scratch_root  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402
from kit.rig import Bind  # noqa: E402

import common as C  # noqa: E402

ASSET = "SM_FX_EliteCrown"
FAMILY = "Monsters_Plains"
OUTLINE_CLASS = "monster"          # same ink weight as the elite it floats over (a 2 px line tone lost its
                                   # dark core at 35 cm / 1080p)
R0 = 0.14                   # band radius at the base
FLARE = 0.016               # the band flares out this much toward its top edge (per 0.12 m of height)
TH = 0.015                  # band thickness
BACK_H = 0.034              # the low back band (the crown always faces the camera's yaw: the back is never seen)
GOLD = Region("#F2B72E", .35, .50)
REGIONS = outline.ink_regions(dict(
    gold=GOLD, gold_light=GOLD.light_region(.25, .30), gold_rule=GOLD.shade_region(.30, .15),
    gem=Region("#FF3A4A", .25, .40, e=0.3), gem_rim=Region("#7A1020", .30, .15),   # e .3: its bloom ate the ink below
), "monster")
TILT_DEG = 38.0
# the web's front-view crown (Pickups.ts:50-67) as the top-edge profile of the front arc (deg from the front,
# height m): the tall centre point, two valleys, two side points, down to the low back band at ±72°
PROFILE = [(0.0, 0.122), (12.5, 0.088), (25.0, 0.058), (37.5, 0.082), (50.0, 0.104), (61.0, 0.066),
           (72.0, BACK_H)]
POINTS = [(0.0, 0.122), (50.0, 0.104), (-50.0, 0.104)]          # ball tips (angle, height)
BACK_STEP = 18.0            # deg between the back band's columns (low, plain)

FX = {"glow": {"color": "#FFD040", "alpha": 0.4, "radiusCm": 23.0, "blend": "additive", "falloff": "web glow",
               "placement": "behind the crown (depth-tested card a little further from the camera; the web draws the "
                            "glow before the crown)"},
      "motion": {"bobCm": 3.0, "bobPeriodMs": 1600, "swayDeg": 20.0, "swayPeriodMs": 3200, "faceCameraYaw": True,
                 "bakedTiltDeg": 38.0,
                 "note": "spec §4.6: slow bob and spin, unlit + outline, always faces the camera's yaw. The mesh is "
                         "tilted back 38° about its base so the W1 camera (pitch -50) sees it nearly front-on; the "
                         "'spin' is a gentle ±20° sway about the camera-facing yaw (a full spin would show the tilt)"},
      "socket": "fx_overhead", "absoluteScale": True,
      "scaleNote": "the crown keeps its 35 cm on scaled elites (hunt leaders x1.25): attach with absolute scale"}


def top_h(a: float) -> float:
    """Top edge height of the crown at angle ``a`` (deg, 0 = front): the front arc's three points (``PROFILE``,
    linear between the keys: straight-edged points like the web polygon), the low back band behind ±72°."""
    d = abs(((a + 180.0) % 360.0) - 180.0)
    if d >= PROFILE[-1][0]:
        return BACK_H
    for (a0, h0), (a1, h1) in zip(PROFILE, PROFILE[1:]):
        if a0 <= d <= a1:
            return h0 + (h1 - h0) * (d - a0) / (a1 - a0)
    return BACK_H


def _ring_dir(a: float) -> Vector:
    c, s = math.cos(math.radians(a - 90.0)), math.sin(math.radians(a - 90.0))
    return Vector((c, s, 0.0))                                # a = 0 → −Y (front)


def _rad(z: float) -> float:
    return R0 + FLARE * (z / 0.12)


def build_crown(pal: Palette) -> bpy.types.Object:
    """Art review rebuild: the web / spec three-point crown (Pickups.ts:50-67, §4.6) — a front arc whose top edge
    is the web polygon (tall centre point, two side points, straight edges), a low plain band round the back, low-
    poly ball tips, a dark rule across the front and a diamond-cut ruby. One band row (the faces are flat between the
    profile keys), ≈ 540 toon + 340 hull tris (was a 5-point ring: 4.8 k toon + 4.1 k hull tris for a 35 cm marker);
    the band and the balls are their own hulls, the rule and the ruby are interior details of the camera-facing
    front."""
    b = M.Builder(ASSET, pal, prefix="elite_crown")
    b.regions(**REGIONS)
    rig = Bind.rigid("root")
    front = sorted({a for a, _ in PROFILE} | {-a for a, _ in PROFILE})
    back = list(np.arange(PROFILE[-1][0] + BACK_STEP, 360.0 - PROFILE[-1][0] - 1e-6, BACK_STEP))
    back = [a if a <= 180.0 else a - 360.0 for a in back]
    angs = sorted(front + back, key=lambda a: (a + 360.0) % 360.0)
    outer, inner = [], []
    for zf in (0.0, 1.0):
        ro, ri = [], []
        for a in angs:
            z = top_h(a) * zf
            d = _ring_dir(a)
            ro.append(d * (_rad(z) + TH * 0.5) + Vector((0, 0, z)))
            ri.append(d * (_rad(z) - TH * 0.5) + Vector((0, 0, z)))
        outer.append(ro)
        inner.append(ri)
    band = C.thick_patch(outer, inner, wrap_u=True)
    bid = b.add(band, "gold", rig, name="band")
    # dark rule across the front (web line at gy − 2), a painted strip on the outer face
    zr0, zr1 = 0.020, 0.027
    ra = [a for a in np.linspace(-66.0, 66.0, 23)]
    ro_, ri_ = [], []
    for zz in (zr0, zr1):
        ro_.append([_ring_dir(a) * (_rad(zz) + TH * 0.5 + 0.0012) + Vector((0, 0, zz)) for a in ra])
        ri_.append([_ring_dir(a) * (_rad(zz) + TH * 0.5 - 0.0004) + Vector((0, 0, zz)) for a in ra])
    b.add(C.thick_patch(ro_, ri_), "gold_rule", rig, name="rule", outline=False)
    # ball tips on the three points (low poly: 8 × 4)
    for pa, ph in POINTS:
        p = _ring_dir(pa) * _rad(ph) + Vector((0, 0, ph + 0.011))
        b.add(M.ellipsoid((0.019, 0.019, 0.019), p, sides=8, rings=4), "gold_light", rig, name="ball")
    # diamond-cut ruby on the front of the band (web: a rhombus over the rule) — four facets to an apex, on a dark
    # rhombic setting
    zc, hh, hw, out = 0.063, 0.024, 0.018, 0.009     # above the rule (web: the gem sits over gy − 2)
    nrm = _ring_dir(0.0)
    base = nrm * (_rad(zc) + TH * 0.5) + Vector((0.0, 0.0, zc))
    up, side = Vector((0, 0, 1)), Vector((1, 0, 0))

    def rhomb(scale, lift):
        c0 = base + nrm * lift
        return [c0 + up * hh * scale, c0 + side * hw * scale, c0 - up * hh * scale, c0 - side * hw * scale]
    rim = rhomb(1.28, 0.0012)
    rim_back = rhomb(1.28, -0.003)
    b.add(C.thick_patch([[rim[0], rim[1]], [rim[3], rim[2]]], [[rim_back[0], rim_back[1]], [rim_back[3], rim_back[2]]]),
          "gem_rim", rig, name="gem_rim", outline=False)
    g = rhomb(1.0, 0.0025)
    apex = base + nrm * (0.0025 + out)
    import bmesh
    bm = bmesh.new()
    vs = [bm.verts.new(tuple(p)) for p in g]
    va = bm.verts.new(tuple(apex))
    vb = [bm.verts.new(tuple(p - nrm * 0.004)) for p in g]
    for i in range(4):
        bm.faces.new((vs[i], vs[(i + 1) % 4], va))
        bm.faces.new((vs[(i + 1) % 4], vs[i], vb[i], vb[(i + 1) % 4]))
    bm.faces.new(list(reversed(vb)))
    gem = M.Part(bm).flat()
    # interior details of the camera-facing front (inside the band's silhouette from the yaw-locked view): no hull
    # and no hull padding (a padded band hull grew a dark bib under the front)
    b.add(gem, "gem", rig, name="gem", outline=False)
    obj = b.build(ASSET, grounding_height=None)
    # tilted back about the base so the yaw-facing crown is seen almost front-on from the W1 camera (pitch −50°):
    # the web's front-view three-point crown, not a ring seen from above
    obj.data.transform(Matrix.Rotation(math.radians(-TILT_DEG), 4, "X"))
    zmin = min(v.co.z for v in obj.data.vertices)
    obj.data.transform(Matrix.Translation((0.0, 0.0, -zmin)))      # pivot = the lowest point (sits on the socket)
    obj.data.update()
    mt = shading.toon_material(pal)
    outline.bake_hull(obj, outline.WIDTH_CM[OUTLINE_CLASS] / 100.0, mt, shading.outline_material(pal))
    for msg in outline.hull_color_report(obj, pal, OUTLINE_CLASS):
        print(f"[ink] WARNING {ASSET}: {msg}")
    obj["af_outline_class"] = OUTLINE_CLASS
    return obj


def over_monsters(pal: Palette, out_dir: Path) -> None:
    """``crown_over_monsters.png``: the crown at ``fx_overhead`` (camera-yawed) over the elites — the chief, the
    shaman and the goblin at hunt-leader scale 1.25 — native 1080p crops at the W1 camera with the glow card."""
    import goblin as GOB
    import goblin_chief as GC
    import goblin_shaman as GS
    cells = []
    for mod, scale in ((GC, 1.0), (GS, 1.0), (GOB, 1.25)):
        scene.reset()
        pal2 = Palette(FAMILY)
        res = mod.build_all()
        _, kit, body, item = res[0], res[1], res[2], res[3]
        from kit import rig as R
        R.attach_to_bone(item, kit.rig, "weapon_r")
        A = mod.anims(kit)
        import mship
        mship.apply_bp(A, A.ready())
        kit.rig.obj.scale = (scale, scale, scale)
        crown = build_crown(pal2)
        rv = review.Review(ASSET, kit.rig.obj, [body, item, crown], 1.4, blob_radius=0.25, out_dir=out_dir,
                           game_outline_px=outline.SCREEN_PX_1080["monster"], outline_class="monster",
                           glows=crown_glow(crown))
        rv.ground_occludes = True
        rv.facing("se")
        bpy.context.view_layer.update()
        review.setup_render((1920, 1080), 8)
        cam = review.game_camera()
        wp = place_on(crown, kit.rig, cam)
        review.set_outline_px([body, item], outline.SCREEN_PX_1080["monster"], Vector((0, 0, 0.6)))
        review.set_outline_px([crown], outline.SCREEN_PX_1080[OUTLINE_CLASS], wp)
        c = review.project(cam, (Vector((0, 0, 0)) + wp) * 0.5)
        hw, hh = 0.07, 0.07 * 1920 / 1080
        img, _ = rv.game_shot((c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh))
        review.draw_text(img, 3, 3, f"{kit.asset} x{scale:g} + CROWN")
        cells.append(img)
    out_dir.mkdir(parents=True, exist_ok=True)
    from kit import pngio
    p = out_dir / "crown_over_monsters.png"
    pngio.write_png_budget(p, review.grid(cells, len(cells)))
    print(f"  {p.name}")


def overhead(rig) -> Vector:
    """World position of the rig's ``fx_overhead`` socket in its current pose / transform."""
    so = next(x for x in rig.sockets if x["name"] == "fx_overhead")
    bn = rig.obj.data.bones[so["bone"]]
    pb = rig.obj.pose.bones[so["bone"]]
    return rig.obj.matrix_world @ (pb.matrix @ (bn.matrix_local.inverted() @ Vector(so["pos"])))


def place_on(crown, rig, cam) -> Vector:
    """The runtime placement: the crown's base on ``fx_overhead``, its front (−Y) turned to the camera's yaw,
    unscaled by the elite's actor scale (manifest ``fx.absoluteScale``). Returns the socket position."""
    bpy.context.view_layer.update()
    wp = overhead(rig)
    dv = cam.matrix_world.translation - wp
    yaw = math.atan2(dv.x, -dv.y)
    crown.parent = None
    crown.matrix_world = Matrix.Translation(wp) @ Matrix.Rotation(yaw, 4, "Z")
    bpy.context.view_layer.update()
    return wp


def halo_view(rv, crown, out: Path) -> None:
    """``game_halo.png``: the 1080p 1:1 crop with the runtime glow as the web draws it — a halo **behind** the
    crown (web: glow first, then the crown; UE: a depth-tested additive card behind the opaque mesh)."""
    from kit import pngio
    review.setup_render((1920, 1080), 8)
    rv.facing("front")
    cam = review.game_camera()
    box = rv.game_crop_box(cam)
    rv.game_outline(1080)
    raw = review.render_array(box)
    b, h = review.coverage_pass(rv.meshes, box, ground=True)
    glow = review.glow_card(raw, *(_glow_args(cam, crown, box)))
    a = np.clip(b + h, 0, 1)[:, :, None]
    img = (raw.astype(np.float32) * a + glow.astype(np.float32) * (1 - a) + 0.5).astype(np.uint8)
    review.draw_text(img, 4, 4, "1080P 1:1 + HALO BEHIND (RUNTIME CARD)")
    rv._save("game_halo.png", img)


def _glow_args(cam, crown, box):
    c = crown.matrix_world @ Vector((0, 0, 0.07))
    px, py = review.project_px(cam, c, (1920, 1080))
    x0, y0 = review.border_origin(box, (1920, 1080))
    wpp = outline.pixel_world_size(cam, c, 1920)
    return ((px - x0, py - y0), FX["glow"]["radiusCm"] / 100.0 / wpp, FX["glow"]["color"], FX["glow"]["alpha"])


def crown_glow(crown):
    def fn(cam, res):
        c = crown.matrix_world @ Vector((0, 0, 0.06))
        px, py = review.project_px(cam, c, res)
        wpp = outline.pixel_world_size(cam, c, res[0])
        return [dict(center_px=(px, py), radius_px=FX["glow"]["radiusCm"] / 100.0 / wpp, color=FX["glow"]["color"],
                     alpha=FX["glow"]["alpha"], core=None, core_alpha=0.0)]
    return fn


def ship(previews: bool = True) -> int:
    """FBX + manifest entry + the review set **camera-facing** (the crown always faces the camera's yaw: the game
    views use facing ``front``), ink-gated like ``kit.asset.ship_static``, + ``crown_over_monsters.png``."""
    from kit import export, paths
    scene.reset()
    pal = Palette(FAMILY)
    obj = build_crown(pal)
    pal.save()
    print(f"[build] {ASSET}: {M.tri_count(obj, 0)} toon + {M.tri_count(obj, 1)} hull tris")
    sockets = [{"name": "base", "pos": (0.0, 0.0, 0.0)}]
    fbx = export.export_static_mesh(obj, ASSET, "VFX", sockets)
    man = export.Manifest()
    man.set_palette(pal)
    entry = export.static_entry(obj, fbx, "VFX", pal, OUTLINE_CLASS, ["elite_crown"], sockets,
                                extra={"attachSocket": "fx_overhead", "fx": FX, "widthCm": round(2 * (_rad(PROFILE[0][1]) + TH * 0.5) * 100, 1),
                                       "displayName": {"zh-CN": "精英王冠", "en": "Elite crown"}, "previews": []})
    entry["materialSlots"][1]["outlineMix"] = outline.INK_MIX[OUTLINE_CLASS]
    man.set_asset(ASSET, entry)
    man.save()
    ok = True
    if previews:
        out = paths.preview_dir(ASSET)
        out.mkdir(parents=True, exist_ok=True)
        for f in out.glob("*.png"):
            f.unlink()
        carrier = bpy.data.objects.new("AF_CrownCarrier", None)
        scene.link(carrier)
        obj.parent = carrier
        rv = review.Review(ASSET, carrier, [obj], 0.16, blob_radius=0.0, out_dir=out,
                           game_outline_px=outline.SCREEN_PX_1080[OUTLINE_CLASS], outline_class=OUTLINE_CLASS)
        rv.ground_occludes = True
        rv.game_view(facing="front")
        halo_view(rv, obj, out)
        rv.closeup("front", pitch=-30.0)
        rv.turnaround()
        v = rv.ink_verdict()
        print(f"[ink] {ASSET}: {v['cells']} cells, min rim {v['minFracRim'] * 100:.1f} %, min dark "
              f"{v['minFracDark'] * 100:.1f} % -> {'PASS' if v['pass'] else 'FAIL'}")
        for f in v["failures"]:
            print("  INK FAIL " + f)
        ok = v["pass"]
        man = export.Manifest()
        man.data["assets"][ASSET]["inkGate"] = {k: v[k] for k in ("pass", "cells", "minFracRim", "minFracDark",
                                                                   "medianRimPx", "maxHullLumP95", "gate")}
        man.save()
        over_monsters(pal, out)
        man = export.Manifest()
        man.data["assets"][ASSET]["previews"] = [f"{ASSET}/{p.name}" for p in sorted(out.glob("*.png"))]
        man.save()
    return 0 if ok else 1


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    if "--quick" in args:
        scene.reset()
        pal = Palette(FAMILY)
        obj = build_crown(pal)
        out = scratch_root("mq", "crown")
        carrier = bpy.data.objects.new("AF_CrownCarrier", None)
        scene.link(carrier)
        obj.parent = carrier
        rv = review.Review(ASSET, carrier, [obj], 0.16, blob_radius=0.0, out_dir=out,
                           game_outline_px=outline.SCREEN_PX_1080[OUTLINE_CLASS], outline_class=OUTLINE_CLASS,
                           glows=crown_glow(obj))
        rv.ground_occludes = True
        rv.closeup("front", pitch=-30.0)
        rv.turnaround()
        rv.game_view(facing="front")
        print([p.name for p, _ in rv.written])
    else:
        sys.exit(ship(previews="--no-previews" not in args))

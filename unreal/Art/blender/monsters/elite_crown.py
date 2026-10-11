"""Elite marker — ``SM_FX_EliteCrown`` (spec art-inventory-ch1.md §4.6; web ``effects/Pickups.ts:50-67``).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/monsters/elite_crown.py [--quick]

A small floating gold crown (≈ 35 cm wide, the web's 16 px), tilted back 38° so the camera-yawed crown is seen
nearly front-on from the W1 camera, over every elite (the chief, the shaman, the hunt
leaders): a flared gold band with five points — the front one tallest, so the camera-yawed crown reads as the web's
three-point silhouette — ball tips in the gold light tone, a dark gold rule round the band and a red cabochon
``#FF3A4A`` (emissive glint, rim ``#7A1020``) at the front. Pivot = the centre of the band's base; it rides
``fx_overhead`` with a slow bob and spin and always faces the camera's yaw (UE side, manifest ``motion``); the warm
glow ``#FFD040`` α .4 is a runtime card (``fx.glow``). Monster-class ink (the weight of the elite it marks; a 2 px line tone lost its dark core at this size).
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
from kit.palette import Palette, Region  # noqa: E402
from kit.rig import Bind  # noqa: E402

import common as C  # noqa: E402

ASSET = "SM_FX_EliteCrown"
FAMILY = "Monsters_Plains"
OUTLINE_CLASS = "monster"          # same ink weight as the elite it floats over (a 2 px line tone lost its
                                   # dark core at 35 cm / 1080p)
R0 = 0.14                   # band radius at the base
BAND_H = 0.045
GOLD = Region("#F2B72E", .35, .50)
REGIONS = outline.ink_regions(dict(
    gold=GOLD, gold_light=GOLD.light_region(.25, .30), gold_rule=GOLD.shade_region(.30, .15),
    gem=Region("#FF3A4A", .25, .40, e=0.6), gem_rim=Region("#7A1020", .30, .15),
), "monster")
TILT_DEG = 38.0
POINTS = [(0.0, 0.118), (72.0, 0.092), (-72.0, 0.092), (144.0, 0.08), (-144.0, 0.08)]   # (angle from front, height)
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
    """Top edge height of the crown at angle ``a`` (deg, 0 = front): the band + five spikes."""
    h = BAND_H
    for pa, ph in POINTS:
        d = abs(((a - pa + 180.0) % 360.0) - 180.0)
        w = 30.0
        if d < w:
            k = 1.0 - d / w
            h = max(h, BAND_H + (ph - BAND_H) * k ** 1.35)
    return h


def build_crown(pal: Palette) -> bpy.types.Object:
    b = M.Builder(ASSET, pal, prefix="elite_crown")
    b.regions(**REGIONS)
    rig = Bind.rigid("root")
    angs = list(np.linspace(-180.0, 180.0, 121))[:-1]        # 3° steps: every point angle is a column
    rows = 7
    outer, inner = [], []
    th = 0.016
    for r in range(rows):
        f = r / (rows - 1)
        ro, ri = [], []
        for a in angs:
            z = top_h(a) * f
            rad = R0 + 0.02 * (z / 0.12)                      # flares out toward the points
            c, s = math.cos(math.radians(a - 90.0)), math.sin(math.radians(a - 90.0))
            d = Vector((c, s, 0.0))                           # a = 0 → −Y (front)
            ro.append(d * (rad + th * 0.5) + Vector((0, 0, z)))
            ri.append(d * (rad - th * 0.5) + Vector((0, 0, z)))
        outer.append(ro)
        inner.append(ri)
    band = C.thick_patch(outer, inner, wrap_u=True)
    bid = b.add(band, "gold", rig, name="band")
    # dark rule round the band (web line at gy − 2) and a raised base ring
    for z0, z1, reg in ((0.026, 0.032, "gold_rule"),):
        rr = []
        for zz in (z0, z1):
            row_o, row_i = [], []
            for a in np.linspace(-180, 180, 73)[:-1]:
                c, s = math.cos(math.radians(a - 90.0)), math.sin(math.radians(a - 90.0))
                d = Vector((c, s, 0.0))
                rad = R0 + 0.02 * (zz / 0.12)
                row_o.append(d * (rad + th * 0.5 + 0.0007) + Vector((0, 0, zz)))
                row_i.append(d * (rad + th * 0.5 - 0.001) + Vector((0, 0, zz)))
            rr.append((row_o, row_i))
        b.add(C.thick_patch([rr[0][0], rr[1][0]], [rr[0][1], rr[1][1]], wrap_u=True), reg, rig, name="rule",
              outline=False)
    # ball tips
    for pa, ph in POINTS:
        c, s = math.cos(math.radians(pa - 90.0)), math.sin(math.radians(pa - 90.0))
        rad = R0 + 0.02 * (ph / 0.12)
        p = Vector((c * rad, s * rad, ph + 0.012))
        b.add(M.ellipsoid((0.021, 0.021, 0.021), p, sides=12, rings=7), "gold_light", rig, name="ball", hull_pad=True)
    # the red cabochon on the front of the band (+ a dark rim)
    front = Vector((0.0, -(R0 + 0.0035), 0.03))
    m = Matrix.Translation(front) @ Matrix.Rotation(math.radians(90), 4, "X")
    rim = M.ellipsoid((0.024, 0.029, 0.007), (0, 0, 0), sides=14, rings=6).transform(m)
    b.add(rim, "gem_rim", rig, name="gem_rim", outline=False)
    gem = M.gem(0.02, 0.012, 0.004, sides=8).transform(m @ Matrix.Translation((0, 0, 0.004)) @ Matrix.Diagonal((1, 1.2, 1, 1)))
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
                                extra={"attachSocket": "fx_overhead", "fx": FX, "widthCm": round(2 * (R0 + 0.035) * 100, 1),
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
        out = Path("/tmp/claude-0/-home-user-abyssfire/b3225f99-25a3-5b78-996f-643be8685889/scratchpad/mq/crown")
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

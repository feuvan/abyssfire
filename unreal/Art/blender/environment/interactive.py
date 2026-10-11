"""Interactive props and world markers (spec art-inventory-ch1.md §6.8; web ``TreasureChest.ts``, ``GoldPile.ts``,
``EventProps.ts``, ``ExitPortal.ts``, ``QuestWorld.ts`` markers; DECISIONS W7 sealed chapter-2 gate, R7).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/interactive.py [names…]
        [--sheet] [--no-previews]

| Asset | game ids | look / behaviour |
|---|---|---|
| ``SK_Prop_TreasureChest`` | ``decor_treasure_chest``, ``treasure_chest`` | ≈ 0.65 × 0.5 m chest ``#8A4E2A`` with brass straps / rim ``#E0AA3A``, lock ``#F0C050`` (keyhole ``#3A1E0C``), an ember-gold seam ``#FFE080`` (focal accent), gold + a ``#FF5A7A`` gem inside; skeleton ``SKEL_Prop_Chest`` (``root``, ``lid``), clips ``Idle`` (1000 ms loop, a restless rattle) and ``Open`` (400 ms, notify ``ChestOpen`` → NS_World_ChestOpen) |
| ``SM_Prop_GoldPile`` | ``decor_gold_pile``, ``gold_pile`` | coin mound ≈ 0.53 × 0.28 m ``#F0B83A`` with spilled coins and a gem |
| ``SM_Prop_Event_RunePillar`` | ``decor_event_puzzle_rune_pillar`` | base ``#5A6664`` 0.7 × 0.2 m, tapered pillar ``#6F7C7A`` to 1.48 m, zig-zag rune ``#2A9AD8`` / core ``#DFF8FF`` |
| ``SM_Prop_ExitPortal`` | ``exit_portal`` | 2.6 m elven stone frame round a teal vortex disc (``#3FE0A0`` → ``#1A6A8A``, swirl strokes), two 1.2 m rune stones ``#6A7A8A``, a ground ring of rune marks; NS_World_ExitPortal adds the motion |
| ``SM_Prop_ExitGate_Sealed`` | ``exit_gate_sealed``, ``exit_sealed`` | W7: the same frame barricaded — nailed planks, an iron chain and padlock across a dormant, crimson-sealed vortex ``#6A2230``, a warning plank; "第二章即将开放" is UI |
| ``SM_FX_ClueMagnifier`` | ``clue_mark``, ``fx_clue`` | floating magnifying glass: brass rim ``#E0B04A``, lens ``#E9FBFF`` → ``#7FC6E0`` + glint, handle ``#7A4A22`` |
| ``SM_FX_GuideArrow`` | ``guide_arrow`` | flat gold chevron ``#FFF2B8`` / ``#FFCF4A`` / ``#D88A1A``, ink ``#2A1606``, points along the mesh forward (−Y) |
| ``SM_Prop_Gather_EmeraldHerb`` | ``gather_herb`` | ≈ 30 cm sprig of 翡翠草药: leaves ``#5FB84A`` with glowing veins ``#9FE07A`` |
"""
from __future__ import annotations

import math
import random
import sys
from pathlib import Path
from types import SimpleNamespace

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import envkit as E  # noqa: E402
import foliage as F  # noqa: E402
import story as ST  # noqa: E402
import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import export, outline, paths, review, scene, shading  # noqa: E402
from kit import mesh as M  # noqa: E402
from kit.palette import Palette, Region  # noqa: E402

CAT = "Props"


def _rx(deg):
    return Matrix.Rotation(math.radians(deg), 4, "X")


def _ry(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Y")


def _rz(deg):
    return Matrix.Rotation(math.radians(deg), 4, "Z")


REGIONS = dict(
    wood=Region("#8A4E2A", .40, .32),
    wood_line=Region("#8A4E2A").shade_region(.30, .12),
    wood_in=Region("#3A1E10", .30, .10),
    brass=Region("#E0AA3A", .42, .50),
    lock=Region("#F0C050", .40, .50),
    keyhole=Region("#3A1E0C", .30, .10),
    seam=Region("#FFE080", .0, .0, e=0.85),
    gold=Region("#F0B83A", .35, .50),
    gold_hi=Region("#FFE89A", .25, .40),
    gem=Region("#FF5A7A", .30, .40, e=0.35),
    gem_core=Region("#FFD0DC", .0, .0, e=0.8),
    pillar_base=Region("#5A6664", .45, .35),
    pillar=Region("#6F7C7A", .42, .35),
    rune_blue=Region("#2A9AD8", .0, .0, e=1.0),
    rune_core=Region("#DFF8FF", .0, .0, e=1.0),
    frame=Region("#8E96A0", .45, .38),
    frame_lt=Region("#A2AAB2", .45, .40),
    frame_line=Region("#8E96A0").line_region(),
    runestone=Region("#6A7A8A", .42, .35),
    vortex_deep=Region("#10304A", .20, .10, e=0.55),
    vortex_mid=Region("#1A6A8A", .10, .10, e=0.9),
    vortex=Region("#3FE0A0", .0, .0, e=1.0),
    vortex_core=Region("#F0FFF8", .0, .0, e=1.0),
    swirl=Region("#DCFFF0", .0, .0, e=1.0),
    swirl_b=Region("#80F0FF", .0, .0, e=1.0),
    ring_mark=Region("#D8FFF0", .0, .0, e=1.0),
    sealed_deep=Region("#2A1018", .25, .10, e=0.25),
    sealed=Region("#6A2230", .20, .15, e=0.45),
    sealed_glyph=Region("#C04050", .0, .0, e=0.7),
    plank=Region("#8A6440", .42, .32),
    plank_line=Region("#8A6440").shade_region(.30, .12),
    iron=Region("#4A4E5A", .42, .45),
    nail=Region("#2E2A30", .40, .20),
    sign_paint=Region("#C8322E", .40, .30),
    brass_rim=Region("#E0B04A", .42, .50),
    lens=Region("#E9FBFF", .20, .25),
    lens_out=Region("#7FC6E0", .30, .30),
    glint=Region("#FFFFFF", .0, .0, e=0.6),
    handle=Region("#7A4A22", .42, .30),
    arrow_hi=Region("#FFF2B8", .25, .30),
    arrow=Region("#FFCF4A", .38, .40),
    arrow_dk=Region("#D88A1A", .40, .35),
    leaf=Region("#5FB84A", .40, .32),
    vein=Region("#9FE07A", .0, .0, e=0.9),
    stem=Region("#3F7A33", .40, .25),
    soil=Region("#7A5A3A", .42, .30),
    stone=Region("#7A7E88", .42, .35),
    dark=Region("#3F7A33", .40, .14),
    light=Region("#8CC24F", .38, .12),
)


def new(pal, name):
    b = M.Builder(name, pal, prefix="interact")
    b.regions(**REGIONS)
    return b, scene.rng(name)


def paint(b, region, pts, normal, width, pid, lift=0.002, thick=0.003):
    pts = [Vector(p) for p in pts]
    b.add(E.ribbon(pts, [Vector(normal)] * len(pts), width, lift=lift, thick=thick), region, name=region,
          outline=False, covered_by=pid)


# ── treasure chest (skeletal) ───────────────────────────────────────────────────────────────────────────
CW, CD, CH = 0.62, 0.44, 0.30          # body width (X), depth (Y), height
LID_R = CD / 2                          # barrel lid radius
HINGE = Vector((0, CD / 2, CH))         # back top edge


def build_chest(pal: Palette):
    """Returns (mesh, binds {part id: 'root' | 'lid'})."""
    name = "SK_Prop_TreasureChest"
    b, rnd = new(pal, name)
    lid_parts = []
    body = M.box((CW, CD, CH), (0, 0, CH / 2), bevel=0.02)
    bid = b.add(body, "wood", name="body", hull_pad=True)
    # dark interior (visible when open) + the treasure: gold heap + coins + gem
    b.add(M.box((CW - 0.07, CD - 0.07, 0.02), (0, 0, CH + 0.0005), bevel=0.004), "wood_in", name="inside",
          outline=False, covered_by=bid)
    heap = M.ellipsoid((CW * 0.4, CD * 0.36, 0.07), (0, 0, CH), sides=12, rings=6)
    b.add(heap, "gold", name="heap", outline=False, covered_by=bid)
    for q in range(7):
        c = Vector((rnd.uniform(-0.2, 0.2), rnd.uniform(-0.12, 0.12), CH + 0.04 + rnd.uniform(0, 0.02)))
        coin = M.cylinder(0.035, 0.035, -0.004, 0.004, sides=10)
        coin.transform(_rx(rnd.uniform(-25, 25)) @ _ry(rnd.uniform(-25, 25))).translate(c)
        b.add(coin, "gold_hi", name="coin", outline=False)
    gem = M.gem(0.04, 0.05, 0.035, sides=6).translate((0.05, -0.02, CH + 0.07))
    b.add(gem, "gem", name="gem", outline=False)
    # plank seams (front/back/sides), brass corner straps + rim band, lock plate
    for z in (CH * 0.36, CH * 0.7):
        paint(b, "wood_line", [(-CW / 2 + 0.03, -CD / 2, z), (0, -CD / 2, z), (CW / 2 - 0.03, -CD / 2, z)], (0, -1, 0),
              0.012, bid)
        paint(b, "wood_line", [(-CW / 2 + 0.03, CD / 2, z), (0, CD / 2, z), (CW / 2 - 0.03, CD / 2, z)], (0, 1, 0),
              0.012, bid)
        for s in (-1, 1):
            paint(b, "wood_line", [(s * CW / 2, -CD / 2 + 0.03, z), (s * CW / 2, 0, z), (s * CW / 2, CD / 2 - 0.03, z)],
                  (s, 0, 0), 0.012, bid)
    rim = M.belt(CW / 2 - 0.004, CD / 2 - 0.004, CH - 0.05, 0.045, 0.012, sides=20, exp=9)
    b.add(rim, "brass", name="rim", outline=False, covered_by=bid)
    for sx in (-1, 1):
        for sy in (-1, 1):
            st = M.box((0.05, 0.05, CH - 0.06), (sx * (CW / 2 - 0.016), sy * (CD / 2 - 0.016), CH / 2 - 0.02),
                       bevel=0.006)
            b.add(st, "brass", name="strap", outline=False, covered_by=bid)
    plate = M.box((0.11, 0.02, 0.12), (0, -CD / 2 - 0.006, CH - 0.07), bevel=0.008)
    b.add(plate, "lock", name="lock", outline=False, covered_by=bid)
    hole = M.ellipsoid((0.013, 0.006, 0.024), (0, -CD / 2 - 0.016, CH - 0.08), sides=6, rings=3)
    b.add(hole, "keyhole", name="keyhole", outline=False, covered_by=bid)
    # barrel lid (half cylinder along X, hinged at the back top edge) + bands + the glowing seam
    rings = []
    for x in (-CW / 2, CW / 2):
        ring = []
        for i in range(9):
            a = math.pi * i / 8
            ring.append(Vector((x, -math.cos(a) * LID_R, CH + math.sin(a) * LID_R * 0.72)))
        rings.append(ring)
    lid = E.ring_loft(rings, cap0="ngon", cap1="ngon", smooth=True)
    lid.auto_sharp(50)
    lid_id = b.add(lid, "wood", name="lid", hull_pad=True)
    lid_parts.append(lid_id)
    for x in (-CW / 2 + 0.07, CW / 2 - 0.07, 0.0):
        band = []
        for i in range(9):
            a = math.pi * i / 8
            band.append(Vector((x, -math.cos(a) * (LID_R + 0.008), CH + math.sin(a) * (LID_R + 0.008) * 0.72)))
        nrm = [Vector((0, -math.cos(math.pi * i / 8), math.sin(math.pi * i / 8))) for i in range(9)]
        rb = E.ribbon(band, nrm, 0.05 if x else 0.04, lift=0.0, thick=0.008)
        lid_parts.append(b.add(rb, "brass", name="lid_band", outline=False, covered_by=lid_id))
    seam = M.box((CW - 0.12, 0.016, 0.018), (0, -CD / 2 - 0.006, CH - 0.006), bevel=0.004)
    b.add(seam, "seam", name="seam", outline=False)
    hasp = M.box((0.06, 0.02, 0.1), (0, -LID_R - 0.004, CH + 0.02), bevel=0.006)
    lid_parts.append(b.add(hasp, "lock", name="hasp", outline=False, covered_by=lid_id))
    obj = E.finish(b, pal, "interactive", ground_h=0.4, name=name)
    return obj, b, lid_parts


def chest_rig(obj, b, lid_parts):
    """Two-bone skeleton ``root`` → ``lid`` (head on the hinge, pointing forward: local X = the hinge axis); the
    lid's parts (and their hull copies) are weighted to ``lid``, everything else to ``root``."""
    arm_data = bpy.data.armatures.new("SKEL_Prop_Chest")
    arm = bpy.data.objects.new("SK_Prop_TreasureChest_Rig", arm_data)
    scene.link(arm)
    scene.select_only([arm], active=arm)
    bpy.ops.object.mode_set(mode="EDIT")
    root = arm_data.edit_bones.new("root")
    root.head, root.tail = (0, 0, 0), (0, 0.15, 0)
    root.roll = 0.0
    lid = arm_data.edit_bones.new("lid")
    lid.head, lid.tail = HINGE, HINGE + Vector((0, -0.2, 0))
    lid.roll = 0.0
    lid.parent = root
    bpy.ops.object.mode_set(mode="OBJECT")
    for bn in arm_data.bones:
        bn.use_deform = True
    me = obj.data
    import numpy as np
    pid = np.zeros(len(me.polygons), np.int32)
    me.attributes["af_part"].data.foreach_get("value", pid)
    hull_src = {}
    for k, p in enumerate(b.parts):        # hull proxies of lid parts move with the lid
        if p.get("hullOnly"):
            base = p["name"][:-5]
            hull_src[k] = base
    names = [p["name"] for p in b.parts]
    lid_ids = set(lid_parts) | {k for k, base in hull_src.items() if any(names[i] == base for i in lid_parts)}
    on_lid = set()
    for poly in me.polygons:
        if int(pid[poly.index]) in lid_ids:
            on_lid.update(poly.vertices)
    vg_root = obj.vertex_groups.new(name="root")
    vg_lid = obj.vertex_groups.new(name="lid")
    lid_v = sorted(on_lid)
    root_v = [v.index for v in me.vertices if v.index not in on_lid]
    vg_lid.add(lid_v, 1.0, "REPLACE")
    vg_root.add(root_v, 1.0, "REPLACE")
    obj.parent = arm
    mod = obj.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    return arm


CHEST_CLIPS = {
    # name: (length ms, loop, keys [(ms, lid angle deg, ease)], notifies)
    "Idle": (1000.0, True, [(0, 0.0), (90, 2.4), (170, 0.0), (260, 1.4), (340, 0.0), (1000, 0.0)], {}),
    "Open": (400.0, False, [(0, 0.0), (60, 4.0), (230, -88.0), (320, -112.0), (400, -105.0)], {"ChestOpen": 120.0}),
}


def _ease(t):
    return t * t * (3 - 2 * t)


def chest_action(arm, clip: str):
    length, loop, keys, notifies = CHEST_CLIPS[clip]
    act = bpy.data.actions.new(f"A_Prop_TreasureChest_{clip}")
    ad = arm.animation_data or arm.animation_data_create()
    ad.action = act
    pb = arm.pose.bones["lid"]
    pb.rotation_mode = "QUATERNION"
    n = scene.ms_to_frame(length)
    for f in range(n + 1):
        ms = scene.frame_to_ms(f)
        for (t0, a0), (t1, a1) in zip(keys, keys[1:]):
            if t0 <= ms <= t1:
                ang = a0 + (a1 - a0) * _ease((ms - t0) / max(t1 - t0, 1e-6))
                break
        else:
            ang = keys[-1][1]
        # the lid bone points forward (−Y): positive rotation about its local X lifts the front edge
        pb.rotation_quaternion = Matrix.Rotation(math.radians(-ang), 4, "X").to_quaternion()
        pb.keyframe_insert("rotation_quaternion", frame=f)
        for bname in ("root",):
            rb = arm.pose.bones[bname]
            rb.rotation_mode = "QUATERNION"
            rb.keyframe_insert("rotation_quaternion", frame=f)
            rb.keyframe_insert("location", frame=f)
        pb.keyframe_insert("location", frame=f)
    act.frame_range = (0, n)
    clip_ns = SimpleNamespace(name=clip, length_ms=length, frames=n, loop=loop, notifies=dict(notifies),
                              additive=False, ref_speed=None,
                              notify_list=lambda nf=notifies: sorted([{"name": k, "ms": float(v)} for k, v in nf.items()],
                                                                     key=lambda d: d["ms"]))
    return act, clip_ns


def ship_chest(pal: Palette, previews: bool = True):
    name = "SK_Prop_TreasureChest"
    obj, b, lid_parts = build_chest(pal)
    pal.save()
    arm = chest_rig(obj, b, lid_parts)
    rig = SimpleNamespace(obj=arm)
    acts = []
    for clip in CHEST_CLIPS:
        acts.append(chest_action(arm, clip))
    # verify the lid opens upward: the lid's front vertex rises at the Open end frame
    from kit import anim
    anim.assign(rig, acts[1][0])
    anim.set_frame(acts[1][1].frames)
    lo, hi = scene.world_bounds([obj])
    print(f"[chest] open bounds z {lo.z:.3f}..{hi.z:.3f} (closed top {CH + LID_R * 0.72:.3f})")
    anim.assign(rig, None)
    for pb in arm.pose.bones:
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.location = (0, 0, 0)
    sk = export.export_skeletal_mesh(rig, [obj], name, CAT)
    anim_entries = []
    for act, clip in acts:
        fbx = export.export_animation(rig, act, CAT)
        anim_entries.append(export.clip_entry(clip, act, fbx))
    man = E.EnvManifest()
    man.set_palette(pal)
    width = float(obj.get("af_outline_width", 0.0))
    entry = {
        "kind": "SkeletalMesh", "category": CAT, "fbx": paths.rel_to_export(sk),
        "gameIds": ["decor_treasure_chest", "treasure_chest"], "skeleton": "SKEL_Prop_Chest", "scale": 1.0,
        "heightCm": round((CH + LID_R * 0.72) * 100.0, 1), "boundsCm": export.bounds_ue([obj]),
        "bones": len(arm.data.bones),
        "triangles": {"toon": E.tris(obj, 0), "outline": E.tris(obj, 1)},
        "materialSlots": [
            {"index": 0, "name": pal.material_name, "parent": "M_AF_Toon", "palette": pal.family},
            {"index": 1, "name": shading.OUTLINE_MATERIAL, "parent": "M_AF_Outline", "palette": pal.family,
             "class": "interactive", "widthCm": round(width * 100.0, 2),
             "outlinePx1080": outline.SCREEN_PX_1080["interactive"], "outlineMix": outline.INK_MIX["interactive"]}],
        "sockets": [{"name": "fx_open", "bone": "root", "relLocCm": [0.0, 0.0, round((CH + 0.05) * 100, 1)],
                     "relRotDeg": [0.0, 0.0, 0.0], "restLocCm": [0.0, 0.0, round((CH + 0.05) * 100, 1)]},
                    {"name": "fx_seam", "bone": "root", "relLocCm": [0.0, round(CD / 2 * 100 + 1, 1),
                                                                     round(CH * 100, 1)],
                     "relRotDeg": [0.0, 0.0, 0.0], "restLocCm": [0.0, round(CD / 2 * 100 + 1, 1), round(CH * 100, 1)]}],
        "anims": anim_entries, "footprintTiles": [1, 1], "blocking": False, "blobShadowRadiusCm": 42.0,
        "family": "environment.interactive", "displayName": "TreasureChest",
        "note": "hidden-area reward / random-event cache (spec 6.8): Idle = restless rattle (the seam glow pulse "
                "0.55 +- 0.25 is the material's emissive x runtime CPD), Open = lid up 105 deg in 400 ms; "
                "ChestOpen notify -> NS_World_ChestOpen (light shaft, sparkles) at socket fx_open",
    }
    if previews:
        rv = review.Review(name, arm, [obj], 0.62, blob_radius=0.42,
                           game_outline_px=outline.SCREEN_PX_1080["interactive"], outline_class="interactive")
        rv.ground_occludes = True
        rv.game_view("front")
        rv.closeup("front")
        rv.turnaround()
        rv.contact_sheet(rig, [(c, a) for a, c in acts], frames=8, per_clip_files=True)
        entry["previews"] = [str(Path(p).relative_to(rv.out.parent.parent)) for p, _ in rv.written]
        v = rv.ink_verdict()
        entry["inkGate"] = {k: v[k] for k in ("pass", "cells", "minFracRim", "minFracDark", "medianRimPx",
                                              "maxHullLumP95", "gate")}
        E.RESULTS.append({"asset": name, "fbx": str(sk), "toonTris": E.tris(obj, 0), "hullTris": E.tris(obj, 1),
                          "ink": {k: v[k] for k in ("pass", "minFracRim", "minFracDark", "medianRimPx",
                                                    "maxHullLumP95")}, "inkFailures": v["failures"]})
    man.set_asset(name, entry)
    man.save()
    return obj


# ── gold pile ───────────────────────────────────────────────────────────────────────────────────────────
def build_goldpile(pal: Palette):
    name = "SM_Prop_GoldPile"
    b, rnd = new(pal, name)
    mound = M.ellipsoid((0.27, 0.21, 0.16), (0, 0, 0.02), sides=14, rings=7)
    for v in mound.bm.verts:
        v.co.z = max(v.co.z, -0.01)
    mid = b.add(mound, "gold", name="mound", hull_pad=True)
    # coins lying on the mound (lit discs) and spilled round it
    for q in range(16):
        a = rnd.uniform(0, math.tau)
        e = rnd.uniform(0.15, 1.2)
        d = Vector((math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e)))
        p = Vector((d.x * 0.27, d.y * 0.21, 0.02 + d.z * 0.16))
        n = Vector((d.x / 0.27, d.y / 0.21, d.z / 0.16)).normalized()
        coin = M.cylinder(0.042, 0.042, -0.005, 0.005, sides=10)
        coin.transform(Vector((0, 0, 1)).rotation_difference(n.lerp(Vector((0, 0, 1)), 0.4)).to_matrix().to_4x4())
        coin.translate(p + n * 0.004)
        b.add(coin, "gold_hi" if q % 3 else "gold", name="coin", outline=False, covered_by=mid)
    for q in range(6):
        a = rnd.uniform(0, math.tau)
        rr = rnd.uniform(0.3, 0.4)
        coin = M.cylinder(0.042, 0.042, 0.0, 0.01, sides=10)
        coin.transform(_rx(rnd.uniform(-12, 12))).translate((math.cos(a) * rr, math.sin(a) * rr * 0.8, 0.0))
        b.add(coin, "gold", name="spill")
    b.add(M.gem(0.045, 0.06, 0.04, sides=6).translate((0.06, -0.05, 0.17)), "gem", name="gem")
    return E.finish(b, pal, "interactive", ground_h=0.25, name=name)


# ── puzzle rune pillar ──────────────────────────────────────────────────────────────────────────────────
def build_runepillar(pal: Palette):
    name = "SM_Prop_Event_RunePillar"
    b, rnd = new(pal, name)
    ST.stone(b, "pillar_base", (0.7, 0.62, 0.2), (0, 0, 0.1), rnd, tilt=0.5, name="base")
    rings = []
    for z, w in ((0.19, 0.2), (1.28, 0.15)):
        rings.append([Vector((x * w, y * w * 0.9, z)) for x, y in ((-1, -1), (1, -1), (1, 1), (-1, 1))])
    p = E.ring_loft(rings, cap0="ngon", cap1=None, smooth=False)
    top = [v for v in p.bm.verts if v.co.z > 1.0]
    top.sort(key=lambda v: math.atan2(v.co.y, v.co.x))
    apex = p.bm.verts.new((0, 0, 1.48))
    for i in range(4):
        p.bm.faces.new((top[i], top[(i + 1) % 4], apex))
    p.recalc_normals()
    p.auto_sharp(30)
    pid = b.add(p, "pillar", name="pillar", hull_pad=True)
    # zig-zag rune down the front (−Y) face + hot core (focal accent)
    zz = [(0.0, 1.15), (0.08, 1.0), (-0.07, 0.85), (0.07, 0.68), (-0.06, 0.52), (0.0, 0.38)]
    pts = []
    for x, z in zz:
        w = 0.2 + (0.15 - 0.2) * (z - 0.19) / (1.28 - 0.19)
        pts.append(Vector((x * w / 0.2, -w * 0.9 - 0.002, z)))
    nrm = Vector((0, -1, 0.04)).normalized()
    b.add(E.ribbon(pts, [nrm] * len(pts), 0.038, lift=0.0, thick=0.004), "rune_blue", name="rune", outline=False,
          covered_by=pid)
    b.add(E.ribbon(pts, [nrm] * len(pts), 0.012, lift=0.004, thick=0.002), "rune_core", name="rune_core",
          outline=False, covered_by=pid)
    for q in range(3):
        a = rnd.uniform(0, math.tau)
        shape = E.RockShape((0.09, 0.08, 0.06), (name, q), bumps=4, amp=0.08, top_cut=0.3)
        b.add(E.rock(shape, subdiv=1, facet=24.0).translate((math.cos(a) * 0.48, math.sin(a) * 0.42, 0)),
              "pillar_base", name="rubble", hull_pad=True)
    return E.finish(b, pal, "interactive", ground_h=0.9, name=name)


# ── exit portal / sealed gate ───────────────────────────────────────────────────────────────────────────
ARCH_W, ARCH_H = 1.0, 2.35        # inner half width, inner height of the frame


def portal_frame(b: M.Builder, rnd: random.Random, name: str):
    """Elven stone frame (two coursed pillars + a voussoir arch, keystone) ≈ 2.6 m, opening along X, faces −Y."""
    for s in (-1, 1):
        z = 0.0
        k = 0
        while z < 1.75:
            h = 0.3 if k % 2 == 0 else 0.26
            ST.stone(b, "frame" if k % 2 else "frame_lt", (0.34, 0.4, h - 0.01),
                     (s * (ARCH_W + 0.17), 0, z + h / 2), rnd, tilt=0.8, name="pillar")
            z += h
            k += 1
    # arch: voussoirs on a semicircle springing at z = 1.75
    R0, R1, zc = ARCH_W, ARCH_W + 0.34, 1.75
    n = 7
    for i in range(n):
        a0 = math.pi * i / n
        a1 = math.pi * (i + 1) / n - 0.012
        rings = []
        for a in (a0, (a0 + a1) / 2, a1):
            ca, sa = math.cos(a), math.sin(a)
            rings.append([Vector((ca * R0, -0.2, zc + sa * R0 * 0.62)), Vector((ca * R1, -0.2, zc + sa * R1 * 0.62)),
                          Vector((ca * R1, 0.2, zc + sa * R1 * 0.62)), Vector((ca * R0, 0.2, zc + sa * R0 * 0.62))])
        part = E.ring_loft(rings, cap0="ngon", cap1="ngon", smooth=False).auto_sharp(40)
        b.add(part, "frame_lt" if i == n // 2 else "frame", name="voussoir", hull_pad=True)
    for s in (-1, 1):
        ST.stone(b, "frame", (0.46, 0.5, 0.12), (s * (ARCH_W + 0.17), 0, 0.06), rnd, tilt=0.3, name="plinth")


def vortex(b: M.Builder, sealed: bool):
    """The standing oval vortex disc filling the opening (two-sided): deep rim → mid → core, with four swirl
    strokes (web ExitPortal swirl arcs); sealed = a dormant crimson seal with a cracked ward glyph."""
    cz = 1.25
    rx, rz = ARCH_W - 0.04, 1.2
    layers = ((1.0, "sealed_deep" if sealed else "vortex_deep"), (0.86, "sealed" if sealed else "vortex_mid"),
              (0.55, "sealed" if sealed else "vortex"), (0.22, "sealed" if sealed else "vortex_core"))
    for k, (f, reg) in enumerate(layers):
        disc = M.cylinder(1.0, 1.0, -0.012 - 0.004 * k, 0.012 + 0.004 * k, sides=28)
        disc.scale((rx * f, rz * f * (1.0 if f > 0.3 else 1.4), 1.0))
        disc.transform(_rx(90)).translate((0, 0, cz))
        b.add(disc, reg, name="vortex", outline=False)
    if not sealed:
        for i in range(4):
            a0 = i * math.pi / 2
            pts = []
            for k in range(13):
                t = k / 12
                rr = 0.92 * (1 - t * 0.85)
                a = a0 + t * math.pi * 1.6
                pts.append(Vector((math.cos(a) * rx * rr, 0, cz + math.sin(a) * rz * rr)))
            for side in (-1, 1):
                q = [p + Vector((0, side * 0.03, 0)) for p in pts]
                b.add(E.ribbon(q, [Vector((0, side, 0))] * len(q), [0.06 - 0.03 * t for t in
                                                                     (k / 12 for k in range(13))], lift=0.0,
                               thick=0.004), "swirl" if i % 2 else "swirl_b", name="swirl", outline=False)
    else:
        for side in (-1, 1):
            ring = [Vector((math.cos(a) * rx * 0.62, side * 0.032, cz + math.sin(a) * rz * 0.62)) for a in
                    (math.tau * i / 24 for i in range(25))]
            b.add(E.ribbon(ring, [Vector((0, side, 0))] * len(ring), 0.035, lift=0.0, thick=0.004), "sealed_glyph",
                  name="ward", outline=False)
            for a in (math.radians(60), math.radians(240)):
                p0 = Vector((math.cos(a) * rx * 0.62, side * 0.034, cz + math.sin(a) * rz * 0.62))
                p1 = Vector((math.cos(a + math.pi) * rx * 0.62, side * 0.034, cz + math.sin(a + math.pi) * rz * 0.62))
                b.add(E.ribbon([p0, (p0 + p1) / 2, p1], [Vector((0, side, 0))] * 3, 0.03, lift=0.0, thick=0.004),
                      "sealed_glyph", name="ward_x", outline=False)


def rune_stones(b: M.Builder, rnd: random.Random, glow_region: str):
    for s in (-1, 1):
        x = s * 1.85
        rings = []
        for z, w, d in ((0.0, 0.17, 0.13), (0.95, 0.14, 0.11), (1.12, 0.07, 0.07)):
            rings.append([Vector((x + math.cos(a) * w + (0.04 * s if z > 1 else 0), -0.15 + math.sin(a) * d, z))
                          for a in (math.tau * i / 6 for i in range(6))])
        st = E.ring_loft(rings, cap0="ngon", cap1="fan", smooth=False).auto_sharp(30)
        sid = b.add(st, "runestone", name="runestone", hull_pad=True)
        c = Vector((x, -0.15 - 0.125, 0.62))
        pts = [c + Vector((0, 0, 0.12)), c + Vector((0.035 * s, 0, 0.02)), c + Vector((-0.02 * s, 0, -0.05)),
               c + Vector((0, 0, -0.13))]
        b.add(E.ribbon(pts, [Vector((0, -1, 0))] * 4, 0.035, lift=0.0, thick=0.004), glow_region, name="rune",
              outline=False, covered_by=sid)


def ground_ring(b: M.Builder, region: str):
    for i in range(12):
        a = math.tau * i / 12
        c = Vector((math.cos(a) * 1.25, -0.9 + math.sin(a) * 0.62, 0.006))
        m = M.box((0.07, 0.11, 0.012), (0, 0, 0), bevel=0.003)
        m.transform(_rz(math.degrees(a) + 90)).translate(c)
        b.add(m, region, name="ring_mark", outline=False)


def build_portal(pal: Palette):
    name = "SM_Prop_ExitPortal"
    b, rnd = new(pal, name)
    portal_frame(b, rnd, name)
    vortex(b, sealed=False)
    rune_stones(b, rnd, "vortex")
    ground_ring(b, "ring_mark")
    shape = E.RockShape((0.14, 0.12, 0.1), (name, "rock"), bumps=4, amp=0.08, top_cut=0.3)
    b.add(E.rock(shape, subdiv=1, facet=24.0).translate((1.45, -0.7, 0)), "runestone", name="rock", hull_pad=True)
    F.tuft(b, Vector((-1.5, -0.6, 0)), 0.24, 8, rnd, segs=2, inked=True)
    return E.finish(b, pal, "interactive", ground_h=1.6, name=name)


def build_sealed(pal: Palette):
    name = "SM_Prop_ExitGate_Sealed"
    b, rnd = new(pal, name)
    portal_frame(b, rnd, name)
    vortex(b, sealed=True)
    rune_stones(b, rnd, "sealed_glyph")
    # barricade: three heavy planks nailed across the opening (front, −Y), two crossed
    planks = [((-0.95, 0.55), (0.95, 0.75), 0.22), ((-0.95, 1.55), (0.95, 1.35), 0.2), ((-0.85, 0.25), (0.8, 2.0), 0.2),
              ((0.85, 0.3), (-0.8, 1.95), 0.18)]
    for k, ((x0, z0), (x1, z1), w) in enumerate(planks):
        a = Vector((x0, -0.27 - 0.05 * (k >= 2), z0))
        e = Vector((x1, -0.27 - 0.05 * (k >= 2), z1))
        L = (e - a).length + 0.18
        pl = M.box((L, 0.05, w), (0, 0, 0), bevel=0.012)
        ang = math.degrees(math.atan2(e.z - a.z, e.x - a.x))
        pl.transform(_ry(-ang)).translate((a + e) / 2)
        pid = b.add(pl, "plank", name="plank", hull_pad=True)
        d = (e - a).normalized()
        for t in (-0.42, 0.42):
            c = (a + e) / 2 + d * (L / 2 * t * 2)
            b.add(M.cylinder(0.022, 0.022, 0.0, 0.01, sides=6).transform(_rx(90)).translate(c + Vector((0, -0.026, 0))),
                  "nail", name="nail", outline=False, covered_by=pid)
        paint(b, "plank_line", [(a + e) / 2 - d * L * 0.4 + Vector((0, -0.026, w * 0.15)),
                                (a + e) / 2 + Vector((0, -0.026, w * 0.15)),
                                (a + e) / 2 + d * L * 0.4 + Vector((0, -0.026, w * 0.15))], (0, -1, 0), 0.012, pid)
    # iron chain across the arch + padlock
    links = []
    for i in range(11):
        t = i / 10
        x = -0.95 + 1.9 * t
        z = 1.05 - 0.22 * math.sin(math.pi * t)
        links.append(Vector((x, -0.38, z)))
    for i, c in enumerate(links):
        ring = M.tube([c + Vector((math.cos(a) * 0.06, 0, math.sin(a) * 0.035)) for a in
                       (math.tau * k / 8 for k in range(9))], [0.012] * 9, sides=5, cap0=None, cap1=None)
        if i % 2:
            ring.transform(Matrix.Translation(c) @ _rz(90) @ Matrix.Translation(-c))
        b.add(ring, "iron", name="chain")
    lock = M.box((0.13, 0.06, 0.15), links[5] + Vector((0, -0.02, -0.12)), bevel=0.02)
    lid = b.add(lock, "iron", name="padlock", hull_pad=True)
    shackle = M.tube([links[5] + Vector((-0.04, -0.02, -0.06)), links[5] + Vector((-0.04, -0.02, 0.0)),
                      links[5] + Vector((0.04, -0.02, 0.0)), links[5] + Vector((0.04, -0.02, -0.06))], [0.013] * 4,
                     sides=6)
    b.add(shackle, "iron", name="shackle")
    # a red-painted warning plank on a stake beside the gate
    post = M.box((0.07, 0.07, 1.05), (-1.62, -0.75, 0.5), bevel=0.01)
    b.add(post, "plank", name="sign_post")
    board = M.box((0.62, 0.04, 0.28), (-1.62, -0.8, 0.88), bevel=0.01)
    board.transform(Matrix.Translation((-1.62, -0.8, 0.88)) @ _ry(-6) @ Matrix.Translation((1.62, 0.8, -0.88)))
    sid = b.add(board, "plank", name="sign", hull_pad=True)
    paint(b, "sign_paint", [(-1.82, -0.823, 0.95), (-1.62, -0.823, 0.82), (-1.42, -0.823, 0.95)], (0, -1, 0), 0.045, sid)
    paint(b, "sign_paint", [(-1.82, -0.823, 0.8), (-1.62, -0.823, 0.93), (-1.42, -0.823, 0.8)], (0, -1, 0), 0.045, sid)
    F.tuft(b, Vector((1.5, -0.55, 0)), 0.24, 8, rnd, segs=2, inked=True)
    return E.finish(b, pal, "interactive", ground_h=1.6, name=name)


# ── markers ─────────────────────────────────────────────────────────────────────────────────────────────
def build_magnifier(pal: Palette):
    name = "SM_FX_ClueMagnifier"
    b, rnd = new(pal, name)
    # lens plane tilted back 40° (reads from the high W1 camera at any yaw); floats 0.55 m up (the Pickup bob /
    # rock of AAbyssPropActor runs about the ground pivot)
    mtx = Matrix.Translation((0, 0, 0.55)) @ _rx(-40)

    def T(p):
        return p.transform(mtx)
    R = 0.15
    rim = M.tube([Vector((math.cos(a) * R, 0, math.sin(a) * R)) for a in (math.tau * k / 20 for k in range(21))],
                 [0.028] * 21, sides=8, cap0=None, cap1=None)
    b.add(T(rim), "brass_rim", name="rim")
    lens = M.cylinder(R * 0.98, R * 0.98, -0.012, 0.012, sides=20)
    lens.transform(_rx(90))
    b.add(T(lens), "lens_out", name="lens")
    inner = M.cylinder(R * 0.62, R * 0.62, -0.016, 0.016, sides=18)
    inner.transform(_rx(90)).translate((-0.025, 0, 0.025))
    b.add(T(inner), "lens", name="lens_in", outline=False)
    for side in (-1, 1):
        gl = E.ribbon([Vector((-0.09, side * 0.02, 0.03)), Vector((-0.06, side * 0.02, 0.08)),
                       Vector((-0.02, side * 0.02, 0.105))], [Vector((0, side, 0))] * 3, 0.022, lift=0.0, thick=0.003)
        b.add(T(gl), "glint", name="glint", outline=False)
    d = Vector((0.62, 0, -0.78)).normalized()
    a = d * (R + 0.02)
    ferrule = M.tube([a, a + d * 0.05], [0.032, 0.032], sides=8)
    b.add(T(ferrule), "brass_rim", name="ferrule")
    handle = M.tube([a + d * 0.05, a + d * 0.2, a + d * 0.27], [0.03, 0.034, 0.03], sides=8)
    b.add(T(handle), "handle", name="handle")
    return E.finish(b, pal, "interactive", ground_h=None, name=name)


def build_arrow(pal: Palette):
    name = "SM_FX_GuideArrow"
    b, rnd = new(pal, name)
    # chevron lying flat, tip toward −Y (the mesh forward)
    outer = [(0.0, -0.2), (0.17, 0.02), (0.1, 0.1), (0.0, -0.03), (-0.1, 0.1), (-0.17, 0.02)]
    body = M.plate(outer[::-1] if False else outer, 0.045, bevel=0.01)
    body.translate((0, 0, 0.03))
    bid = b.add(body, "arrow", name="chevron", hull_pad=True)
    hi = [(0.0, -0.17), (0.075, -0.075), (0.0, -0.06), (-0.075, -0.075)]
    top = M.plate(hi, 0.008, bevel=0.002).translate((0, 0, 0.056))
    b.add(top, "arrow_hi", name="hi", outline=False, covered_by=bid)
    for s in (-1, 1):
        back = [(s * 0.1, 0.1), (s * 0.17, 0.02), (s * 0.14, 0.02), (s * 0.085, 0.072)]
        if s < 0:
            back = back[::-1]
        bk = M.plate(back if s > 0 else back, 0.008, bevel=0.002).translate((0, 0, 0.056))
        b.add(bk, "arrow_dk", name="back", outline=False, covered_by=bid)
    obj = E.finish(b, pal, "interactive", ground_h=None, name=name)
    return obj


def build_herb(pal: Palette):
    name = "SM_Prop_Gather_EmeraldHerb"
    b, rnd = new(pal, name)
    mound = M.ellipsoid((0.12, 0.1, 0.03), (0, 0, 0.0), sides=10, rings=4)
    b.add(mound, "soil", name="soil")
    stem = M.tube([Vector((0, 0, 0.0)), Vector((0.01, 0.0, 0.12)), Vector((-0.01, 0.01, 0.27))], [0.012, 0.01, 0.006],
                  sides=6)
    b.add(stem, "stem", name="stem")
    for i in range(7):
        a = i * 2.39 + rnd.uniform(-0.2, 0.2)
        z = 0.04 + i * 0.033
        L = 0.16 - i * 0.012
        d = Vector((math.cos(a), math.sin(a), 0.55 + i * 0.08)).normalized()
        base = Vector((0, 0, z))
        side = d.cross(Vector((0, 0, 1))).normalized()
        lf = E.blade(base, d, L, 0.075 - i * 0.004, bend=0.18, side=side, segs=3, thick=0.008)
        lid = b.add(lf, "leaf", name="leaf", hull_pad=True)
        # glowing midrib vein along the leaf's upper face
        out = d.cross(side).normalized()
        if out.z < 0:
            out = -out
        horiz = Vector((d.x, d.y, 0)).normalized()
        pts = []
        for k in range(4):
            t = 0.1 + 0.75 * k / 3
            p = base + d * (L * t) + horiz * (0.18 * L * t * t) - Vector((0, 0, 0.18 * L * t * t * 0.6))
            pts.append(p)
        b.add(E.ribbon(pts, [out] * 4, [0.014, 0.012, 0.009, 0.005], lift=0.006, thick=0.002), "vein", name="vein",
              outline=False, covered_by=lid)
    bud = M.ellipsoid((0.025, 0.025, 0.035), (-0.01, 0.01, 0.29), sides=8, rings=4)
    b.add(bud, "vein", name="bud")
    return E.finish(b, pal, "interactive", ground_h=0.2, name=name)


ASSETS: dict = {
    "SM_Prop_GoldPile": (build_goldpile, dict(game_ids=["decor_gold_pile", "gold_pile"], footprint=[1, 1],
                                              blocking=False, blob=0.36)),
    "SM_Prop_Event_RunePillar": (build_runepillar, dict(game_ids=["decor_event_puzzle_rune_pillar"],
                                                        footprint=[1, 1], blocking=False, blob=0.45)),
    "SM_Prop_ExitPortal": (build_portal, dict(game_ids=["exit_portal"], footprint=[1, 1], blocking=False, blob=1.3,
                                              sockets=[{"name": "fx_vortex", "pos": (0, 0, 1.25)},
                                                       {"name": "fx_ring", "pos": (0, -0.9, 0.02)}])),
    "SM_Prop_ExitGate_Sealed": (build_sealed, dict(game_ids=["exit_gate_sealed", "exit_sealed"], footprint=[1, 1],
                                                   blocking=True, blob=1.3,
                                                   sockets=[{"name": "fx_vortex", "pos": (0, 0, 1.25)}])),
    "SM_FX_ClueMagnifier": (build_magnifier, dict(game_ids=["clue_mark", "fx_clue"], blob=0.25, cat="VFX")),
    "SM_FX_GuideArrow": (build_arrow, dict(game_ids=["guide_arrow"], blob=0.01, ground=False, cat="VFX",
                                           extra_kw={"meshYawOffsetDeg": -90.0})),
    "SM_Prop_Gather_EmeraldHerb": (build_herb, dict(game_ids=["gather_herb"], footprint=[1, 1], blocking=False,
                                                    blob=0.15)),
}
NOTES = {
    "SM_Prop_ExitPortal": "open zone exit (later chapters; W7 seals the Chapter-1 exit): faces into the map (-Y); "
                          "NS_World_ExitPortal (rotating swirl, motes, ground ring pulse) + teal light at fx_vortex",
    "SM_Prop_ExitGate_Sealed": "W7: the twilight_forest exit (119,60) shows this sealed gate; the '第二章即将开放' message "
                               "is UI (zone.exit.sealedChapter2); exit.sealed recipe at fx_vortex",
    "SM_FX_ClueMagnifier": "quest clue marker (QuestWorld investigate_clue): floats 0.5 m up, bobs 6 px and rocks +-8 "
                           "deg (1100 ms yoyo) over NS_World_ClueRing; pivot = lens centre",
    "SM_FX_GuideArrow": "quest guide chevron on a 1.1 m ring round the hero, 30 cm up; tip = mesh forward (-Y, UE +Y "
                        "before meshYawOffsetDeg); CPD Tint (9..12) recolours it cream/white for turn-ins",
    "SM_Prop_Gather_EmeraldHerb": "gather node of q_herb_gathering (翡翠草药): bob + gold glow ring are runtime FX",
    "SM_Prop_GoldPile": "hidden-area reward (110,107): glint = runtime glow card",
    "SM_Prop_Event_RunePillar": "random event environmental_puzzle (plains): glow #72D8FF r 18 px = runtime card",
}


def main(argv: list[str]) -> int:
    sheet = "--sheet" in argv
    previews = "--no-previews" not in argv
    names = [a for a in argv if not a.startswith("--")] or ["SK_Prop_TreasureChest"] + list(ASSETS)
    all_names = ["SK_Prop_TreasureChest"] + list(ASSETS)
    names = [n for n in all_names if any(n == a or a in n for a in names)]
    scene.reset()
    pal = Palette(E.PROPS)
    if sheet:
        objs = []
        for n in names:
            if n == "SK_Prop_TreasureChest":
                objs.append(build_chest(pal)[0])
            else:
                objs.append(ASSETS[n][0](pal))
        pal.save()
        E.sheet(objs, "sheet_interactive_" + "_".join(n.split("_")[-1][:6] for n in names)[:80], cols=3)
        return 0
    for n in names:
        scene.reset()
        pal = Palette(E.PROPS)
        if n == "SK_Prop_TreasureChest":
            ship_chest(pal, previews)
            continue
        fn, kw = ASSETS[n]
        kw = dict(kw)
        cat = kw.pop("cat", CAT)
        extra = {"displayName": n.split("_", 2)[-1], "family": "environment.interactive"}
        extra.update(kw.pop("extra_kw", {}))
        if n in NOTES:
            extra["note"] = NOTES[n]
        obj = fn(pal)
        pal.save()
        E.ship(obj, n, cat, pal, obj["af_outline_class"], extra=extra, previews=previews, close_facing="front", **kw)
    return E.summary()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

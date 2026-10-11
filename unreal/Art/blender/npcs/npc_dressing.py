"""NPC props, world dressing and the family lineup (spec art-inventory-ch1.md §5.1 props, §5.2 anvil scenery, §5.4 /
§9.4 quest markers, DECISIONS R2 heights).

* ``ship_props``    — every ``SM_NPCKit_Prop_*`` (Art/Export/NPCs, NPC palette, NPC ink) with its sockets and the NPCs
  that hold it (``heldBy``), review sheet ``Previews/npc_props/props.png``;
* ``ship_dressing`` — ``SM_Prop_Camp_AnvilStump`` (Art/Export/Props, socket ``strike``; placed by the blacksmith's
  manifest ``scenery`` offset) and the overhead quest markers ``SM_UI_QuestMarker_Excl / _Quest / _QuestGrey``
  (Art/Export/NPCs; pivot at the glyph's foot, mounted on ``fx_overhead``), each with the kit's static review set and
  ink gate;
* ``lineup``        — ``Previews/npc_family/lineup_game.png``: every NPC at the W1 camera, native 1080p pixels, side by
  side at one scale with the warrior (``SK_Hero_Warrior.fbx`` re-imported, flat silhouette) for the R2 height check,
  and ``lineup_close.png`` (front 3/4 close-ups at one scale).
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "monsters", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import anim, asset, export, outline, paths, review, scene, shading  # noqa: E402
from kit.palette import Palette  # noqa: E402

import folk as F  # noqa: E402
import looks as LK  # noqa: E402
import props as P  # noqa: E402


def _held_by() -> dict:
    out: dict = {}
    for n in LK.NPCS:
        for kind, bone in ((n.look.item, "weapon_r"), (n.look.off, "weapon_l")):
            if kind == "none":
                continue
            k = "herbs" if (kind == "pouch" and n.id == "PlainsHerbalist") else kind
            out.setdefault(P.PROPS[k], []).append((n.asset, bone))
    return out


def ship_props() -> int:
    import mship
    scene.reset()
    pal = Palette(F.FAMILY)
    held = _held_by()
    objs = []
    for kind, name in P.PROPS.items():
        obj, socks = P.build_prop(kind, pal)
        hb = held.get(name, [])
        sock = hb[0][1] if hb else ("weapon_r" if kind in ("hammer", "staff", "sword", "herbs") else "weapon_l")
        fbx = export.export_static_mesh(obj, name, "NPCs", socks)
        man = export.Manifest()
        man.set_palette(pal)
        e = export.static_entry(obj, fbx, "NPCs", pal, "npc", sockets=socks,
                                extra={"attachSocket": sock, "heldBy": sorted({a for a, _ in hb}),
                                       "weaponType": f"npc_{kind}", "previews": ["npc_props/props.png"],
                                       "note": "weapon-bone space: grip at the origin, +Y along the item, +Z the "
                                               "leading face; attaches to the socket with an identity transform"})
        e["materialSlots"][1]["outlineMix"] = outline.INK_MIX["npc"]
        man.set_asset(name, e)
        man.save()
        objs.append((name, obj))
        print(f"[prop] {name}: {sum(len(p.vertices) - 2 for p in obj.data.polygons if p.material_index == 0)} toon "
              f"tris, sockets {[s['name'] for s in socks]}")
    pal.save()
    _props_sheet(objs)
    return 0


PROP_VIEW_YAWS = ((240, "A"), (60, "B"))      # the leading face 3/4 front, then 3/4 back (tongs open)


def _props_sheet(objs) -> None:
    out = paths.preview_root() / "npc_props"
    out.mkdir(parents=True, exist_ok=True)
    review.stage()
    review.hide_stage(True)
    cells = []
    res = (220, 300)
    for o in [o for _, o in objs]:
        o.hide_render = True
    for name, o in objs:
        o.hide_render = False
        for yaw, side in PROP_VIEW_YAWS:
            # +Y (along the item) up — hanging bags the other way round, neck up, as they hang from the fist
            up = -90 if name.endswith("Pouch") else 90
            o.matrix_world = Matrix.Rotation(math.radians(yaw), 4, "Z") @ Matrix.Rotation(math.radians(up), 4, "X")
            bpy.context.view_layer.update()
            review.setup_render(res, 8, bg_hex="#3B3546")
            pts = review.mesh_points([o])
            review.frame_points(pts, res, fov_h=24, pitch_ue=-15, fill=0.86, include_ground=False)
            review.set_outline_px([o], 1.8, Vector(pts.mean(0)))
            img = review.render_array()
            review.draw_text(img, 3, 3, f"{name[len('SM_NPCKit_Prop_'):].upper()} {side}")
            cells.append(img)
        o.hide_render = True
    p = out / "props.png"
    from kit import pngio
    pngio.write_png_budget(p, review.grid(cells, 6))
    print(f"  {p}")


def _ship_grounded_static(name: str, category: str, obj, pal, cls: str, game_ids, sockets, footprint, blocking,
                          height: float, blob: float, extra: dict) -> dict | None:
    """``asset.ship_static`` for a prop standing on the terrain: the same export / manifest / review set, with
    ``Review.ground_occludes`` (the hull under its foot is hidden by the ground in UE — it is not measured as missing
    ink, exactly as for the characters' soles)."""
    fbx = export.export_static_mesh(obj, name, category, sockets)
    man = export.Manifest()
    man.set_palette(pal)
    entry = export.static_entry(obj, fbx, category, pal, cls, game_ids, sockets, footprint, blocking, extra)
    rv = review.Review(name, obj, [obj], height, blob_radius=blob, game_outline_px=outline.SCREEN_PX_1080[cls],
                       outline_class=cls)
    rv.ground_occludes = True
    rv.game_view()
    rv.closeup()
    rv.turnaround()
    entry["previews"] = [str(Path(p).relative_to(rv.out.parent.parent)) for p, _ in rv.written]
    v = asset._record_ink(rv, entry, cls)
    man.set_asset(name, entry)
    man.save()
    return v


def ship_dressing() -> int:
    rc = 0
    scene.reset()
    pal = Palette(F.FAMILY)
    obj, socks = P.build_anvil(pal)
    v = _ship_grounded_static(
        P.ANVIL, "Props", obj, pal, "npc", ["npc_scenery_anvil"], socks, [1, 1], False, 0.62, 0.22,
        {"usedBy": ["SK_NPC_Blacksmith"], "displayName": {"zh": "铁砧", "en": "Anvil"},
         "outlineNote": "NPC scenery, not camp decor: spawned with the smith (SK_NPC_Blacksmith.scenery) and struck "
                        "by his Work_Hammer loop, so it is inked with the NPC class (3 px, 60 % ink) like him",
         "note": "stump + anvil scenery beside the blacksmith (spec §5.2): placed by SK_NPC_Blacksmith.scenery; "
                 "socket `strike` = where FX_HammerStrike sparks (NS_NPC_AnvilSparks) spawn"})
    print(f"[dressing] {P.ANVIL}: ink {v['pass'] if v else None}")
    rc |= 0 if (v or {}).get("pass", True) else 1
    rc |= ship_markers()
    return rc


MARKER_IDS = {"SM_UI_QuestMarker_Excl": "quest_marker_available", "SM_UI_QuestMarker_Quest": "quest_marker_complete",
              "SM_UI_QuestMarker_QuestGrey": "quest_marker_progress"}
MARKER_LIFT = 0.06          # above fx_overhead (the glyph's foot; the bob rises from here)


def _face_camera(obj, at: Vector, cam) -> None:
    """The runtime placement of an overhead marker: its foot at ``at``, its face (−Y) turned to the camera — yaw
    and pitch (a full billboard: the W1 camera looks down 50°, a yaw-only glyph would read squashed to 64 %)."""
    d = (cam.matrix_world.translation - at).normalized()
    yaw = math.atan2(d.x, -d.y)
    pitch = math.asin(max(-1.0, min(1.0, d.z)))
    obj.parent = None
    obj.matrix_world = (Matrix.Translation(at) @ Matrix.Rotation(yaw, 4, "Z")
                        @ Matrix.Rotation(-pitch, 4, "X"))
    bpy.context.view_layer.update()


def ship_markers() -> int:
    """``SM_UI_QuestMarker_*`` (spec §5.4 / §9.4): FBX + manifest per glyph; review ``Previews/npc_markers/``:
    ``markers_over_npc.png`` — each marker on the elder's ``fx_overhead`` at the W1 camera, native 1080p, the NPC ink
    on both; the marker cells are ink-gated (marker alone, same placement) — and ``markers_close.png``."""
    import npc_ship as S
    from kit import pngio
    rc = 0
    out = paths.preview_root() / "npc_markers"
    out.mkdir(parents=True, exist_ok=True)
    for p in out.glob("*.png"):
        p.unlink()
    npc = LK.find("QuestElder")
    B = S.build(npc)
    A = S.anims(B)
    S.attach(B)
    anim.apply_pose(A.solve(A.ready()))
    pal = B.pal
    cells, close, verdicts = [], [], {}
    man = export.Manifest()
    for name in P.MARKERS:
        m = P.build_marker(name, pal)
        fbx = export.export_static_mesh(m, name, "NPCs", [])
        entry = export.static_entry(m, fbx, "NPCs", pal, "npc", [MARKER_IDS[name]], [], extra={
            "attachSocket": "fx_overhead", "offsetCm": [0.0, 0.0, MARKER_LIFT * 100.0],
            "billboard": "camera (yaw + pitch: the face, -Y, turned to the camera)",
            "bob": {"amplitudeCm": 4.0, "periodMs": 1400.0},
            "note": "overhead quest marker (spec §5.4 / §9.4): '!' available, '?' turn-in #F1C40F, grey '?' #888888 "
                    "in progress; pivot at the glyph's foot",
            "previews": ["npc_markers/markers_over_npc.png", "npc_markers/markers_close.png"]})
        rv = review.Review(name, B.rig.obj, meshes_of_with(B, m), B.folk.D.height, blob_radius=npc.blob_radius,
                           out_dir=out, game_outline_px=outline.SCREEN_PX_1080["npc"], outline_class="npc",
                           glows=S.glows_for(B))
        rv.ground_occludes = True
        rv.facing("se")
        review.setup_render((1920, 1080), 8)
        cam = review.game_camera()
        top = S._world_socket(B.rig, "fx_overhead") + Vector((0, 0, MARKER_LIFT))
        _face_camera(m, top, cam)
        rv.game_outline(1080, Vector((0, 0, 1.2)))
        c = review.project(cam, (top + Vector((0, 0, 0.34))) * 0.5)
        hw, hh = 0.062, 0.13
        box = (c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh)
        img, _ = rv.game_shot(box)
        review.draw_text(img, 3, 3, name[len("SM_UI_QuestMarker_"):].upper() + " ON ELDER")
        cells.append(img)
        # the gated cell: the marker alone at the same placement (its own silhouette)
        hidden = [o for o in rv.meshes if o is not m]
        for o in hidden:
            o.hide_render = True
        rv.meshes = [m]
        mc = review.project(cam, m.matrix_world @ Vector((0, 0, 0.18)))
        mb = (mc[0] - 0.034, mc[1] - 0.05, mc[0] + 0.034, mc[1] + 0.05)
        img2, st = rv.game_shot(mb, measure=True, tag=f"{name} 1080p")
        review.draw_text(img2, 3, 3, "1080P")
        review.draw_text(img2, 3, img2.shape[0] - 10, rv.ink_label(st), rgb=(255, 220, 140))
        close.append(img2)
        for o in hidden:
            o.hide_render = False
        v = asset._record_ink(rv, entry, "npc")
        verdicts[name] = v
        man.set_asset(name, entry)
        rc |= 0 if v["pass"] else 1
        m.hide_render = True
    man.set_palette(pal)
    pal.save()
    man.save()
    pngio.write_png_budget(out / "markers_over_npc.png", review.grid(cells, len(cells), pad=2))
    pngio.write_png_budget(out / "markers_close.png", review.grid(close, len(close), pad=2))
    for nm, v in verdicts.items():
        print(f"[dressing] {nm}: ink {'PASS' if v['pass'] else 'FAIL'} rim {v['minFracRim'] * 100:.1f} % dark "
              f"{v['minFracDark'] * 100:.1f} %")
    return rc


def meshes_of_with(B, extra) -> list:
    import npc_ship as S
    return S.meshes_of(B) + [extra]


# ── lineup ──────────────────────────────────────────────────────────────────────────────────────────────
def _warrior_cell(box_of):
    """The warrior as UE receives it — the shipped ``SK_Hero_Warrior`` + sword / shield FBXs re-imported, driven by
    the re-imported ``A_Hero_Warrior_Idle`` (frame 0), Heroes palette toon + hull, the hero ink width — the R2 height
    reference at the same camera and scale as the NPC cells."""
    from kit import rig as R
    from kit.palette import Palette as Pal
    scene.reset()
    ch, wp = paths.category_dir("Characters"), paths.category_dir("Weapons")
    sk, idle = ch / "SK_Hero_Warrior.fbx", ch / "A_Hero_Warrior_Idle.fbx"
    if not (sk.exists() and idle.exists()):
        return None
    bpy.ops.import_scene.fbx(filepath=str(sk))
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    me = next(o for o in bpy.data.objects if o.type == "MESH")
    seen = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(idle))
    arm2 = next(o for o in bpy.data.objects if o not in seen and o.type == "ARMATURE")
    for pb in arm.pose.bones:
        c = pb.constraints.new("COPY_TRANSFORMS")
        c.target, c.subtarget = arm2, pb.name
        c.target_space = c.owner_space = "POSE"
    arm2.hide_render = True
    act = arm2.animation_data.action if arm2.animation_data else None
    bpy.context.scene.frame_set(int(act.frame_range[0]) if act else 0)
    pal = Pal("Heroes")
    mt, mo = shading.toon_material(pal), shading.outline_material(pal)
    meshes = [me]
    me.data.materials[0], me.data.materials[1] = mt, mo
    me["af_outline_width"] = outline.WIDTH_CM["hero"] / 100.0
    for name, bone in (("SM_Hero_Warrior_Sword", "weapon_r"), ("SM_Hero_Warrior_Shield", "weapon_l")):
        f = wp / f"{name}.fbx"
        if not f.exists():
            continue
        seen = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(f))
        new = [o for o in bpy.data.objects if o not in seen]
        o = next(x for x in new if x.type == "MESH" and not x.name.startswith(("SOCKET_", "UCX_")))
        for x in new:
            if x is not o:
                bpy.data.objects.remove(x)
        o.data.materials[0], o.data.materials[1] = mt, mo
        o["af_outline_width"] = outline.WIDTH_CM["weapon"] / 100.0
        R.attach_to_bone(o, R.Rig(arm, None, {}, []), bone)
        meshes.append(o)
    bpy.context.view_layer.update()
    rv = review.Review("lineup_warrior", arm, meshes, 1.76, blob_radius=0.39,
                       game_outline_px=outline.SCREEN_PX_1080["hero"], outline_class="hero")
    rv.ground_occludes = True
    rv.facing("se")
    review.setup_render((1920, 1080), 8)
    cam = review.game_camera()
    rv.game_outline(1080, Vector((0, 0, 0.9)))
    img, st = rv.game_shot(box_of(cam), measure=True, tag="lineup warrior")
    review.draw_text(img, 3, 3, "WARRIOR (REF, FBX)")
    review.draw_text(img, 3, img.shape[0] - 10, f"1.76M {rv.ink_label(st)}", rgb=(255, 220, 140))
    return img


def lineup() -> int:
    import npc_ship as S
    from kit import pngio
    out = paths.preview_root() / "npc_family"
    out.mkdir(parents=True, exist_ok=True)
    cells, close = [], []
    hw = 0.06
    hh = hw * 1920 / 1080 * 0.82            # ≈ 210 × 190 px of native 1080p: the whole figure, hat and props

    def box_of(cam):
        c = review.project(cam, Vector((0, 0, 0.92)))
        return (c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh)
    heights = []
    for n in LK.NPCS:
        B = S.build(n)
        A = S.anims(B)
        S.attach(B)
        clip = A.work()
        act = anim.bake(B.rig, clip, n.token)
        anim.assign(B.rig, act)
        anim.set_frame(0)
        rv = S.make_review(B, out)
        rv.facing("se")
        review.setup_render((1920, 1080), 8)
        cam = review.game_camera()
        rv.game_outline(1080, Vector((0, 0, 0.8)))
        img, st = rv.game_shot(box_of(cam), measure=True, tag=f"lineup {n.id}")
        review.draw_text(img, 3, 3, n.id.upper()[:18])
        review.draw_text(img, 3, img.shape[0] - 10, f"{B.folk.D.height:.2f}M {rv.ink_label(st)}", rgb=(255, 220, 140))
        cells.append(img)
        heights.append((n.id, B.folk.D.height))
        anim.assign(B.rig, None)
        anim.apply_pose(A.solve(A.ready()))
        rv.facing("front")
        review.setup_render((200, 320), 8)
        review.frame_bounds(Vector((-0.55, -0.55, 0.0)), Vector((0.55, 0.55, 2.05)), (200, 320), fov_h=22,
                            pitch_ue=-28, fill=0.98)
        rv.ink()
        im2 = review.render_array()
        review.draw_text(im2, 3, 3, n.id.upper()[:20])
        close.append(im2)
    w = _warrior_cell(box_of)
    if w is not None:
        cells.append(w)
    pngio.write_png_budget(out / "lineup_game.png", review.grid(cells, 5, pad=2))
    pngio.write_png_budget(out / "lineup_close.png", review.grid(close, 9, pad=2))
    for nm, h in heights:
        print(f"  {nm:26s} crown {h:.2f} m")
    print(f"  {out / 'lineup_game.png'}\n  {out / 'lineup_close.png'}")
    return 0

"""March NPC props, dressing, kit parts and the family lineup (A9; run through ``march_ship.py``).

* ``ship_props``    — ``SM_NPCKit_Prop_{Handbell,Lantern,Ledger,Vial,Barrow}`` (Art/Export/NPCs, NPC palette, NPC ink)
  with their sockets and holders (``heldBy``); the barrow is exported in the root bone's frame (it hangs on the root
  socket ``barrow``); review sheet ``Previews/npc_props_march/props.png``;
* ``ship_dressing`` — ``SM_Prop_March_AnvilStump`` (Art/Export/Props, game id ``npc_scenery_anvil@march``; placed by
  ``SK_NPC_Blacksmith_March.scenery``) with the kit's static review set on the march ground;
* ``ship_kit``      — the march kit parts ``SK_NPCKit_{HalfPlate,Gorget,Top_Cassock,Hat_Hood,FeatherCollar,RelicTray}``
  (each the merged look's own generator on that look's rig, as ``npc_kit_parts``) + ``Previews/npc_kit_march/``;
* ``lineup``        — ``Previews/npc_family_march/lineup_game.png`` (the eleven at the W1 camera, native 1080p, the
  march ground, the warrior for the R2 height check) and ``lineup_close.png``.

Props and kit parts are never drawn alone: their ``inkGate`` is recorded from the merged NPCs that hold / wear them
(``npc_dressing.held_ink_gate``) — ship the NPCs first.
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "monsters", HERE.parent / "march", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

from kit import anim, export, mesh as M, outline, paths, review, rig as R, scene, theme  # noqa: E402
from kit.palette import Palette  # noqa: E402

import folk as F  # noqa: E402
import npc_ship as S  # noqa: E402
import npc_dressing as ND  # noqa: E402
import march_folk as MF  # noqa: E402
import march_looks as ML  # noqa: E402
import march_props as MPR  # noqa: E402

PROP_SOCKET = {"handbell": "weapon_r", "vial": "weapon_r", "lantern": "weapon_l", "ledger": "weapon_l",
               "barrow": "barrow", "spade": "weapon_r", "sword_notched": "weapon_r"}


def _held_by() -> dict:
    out: dict = {}
    for n in ML.NPCS:
        for kind in (n.look.item, n.look.off):
            if kind in MPR.MARCH_PROPS:
                out.setdefault(MPR.MARCH_PROPS[kind], set()).add(n.asset)
    return {k: sorted(v) for k, v in out.items()}


def _root_rig():
    """Old Bern's SKEL_Human rig, for the barrow bone's frame (the barrow is exported in ``barrow_01``'s frame)."""
    npc = ML.find("EscortTravelingMerchant_March")
    folk = MF.MarchFolk(npc.look, npc.asset, prefix=npc.prefix)
    return R.build_humanoid(folk.spec())


def ship_props() -> int:
    scene.reset()
    pal = Palette(F.FAMILY)
    held = _held_by()
    objs = []
    rig = _root_rig()
    global ROOT_M
    ROOT_M = rig.obj.data.bones["barrow_01"].matrix_local.copy()
    for kind, name in MPR.MARCH_PROPS.items():
        obj, socks = MPR.build_prop(kind, pal)
        if kind == "barrow":
            import march_ship as MS
            socks = MS.to_bone_space(obj, rig, "barrow_01", socks)
        hb = held.get(name, [])
        fbx = export.export_static_mesh(obj, name, "NPCs", socks)
        man = export.Manifest()
        man.set_palette(pal)
        note = ("weapon-bone space: grip at the origin, +Y along the item, +Z the leading face; attaches to the socket "
                "with an identity transform")
        if kind == "barrow":
            note = ("barrow_01-bone space (socket `barrow` on Bern's bone barrow_01, head at the wheel's axle, "
                    "identity): modelled in body space and re-expressed in that frame, its grips (sockets handle_l / "
                    "handle_r) under the escort's fists; rests on its two legs and the wheel, which is not in this "
                    "mesh: it is built into SK_NPC_EscortTravelingMerchant_March on the spinning bone "
                    "barrow_wheel_01 (Walk / Run pitch barrow_01 up about the axle and spin the wheel)")
        e = export.static_entry(obj, fbx, "NPCs", pal, "npc", sockets=socks,
                                extra={"attachSocket": PROP_SOCKET[kind], "heldBy": hb, "weaponType": f"npc_{kind}",
                                       "previews": ["npc_props_march/props.png"], "note": note,
                                       "march": {"theme": "march", "package": "A9"}})
        e["materialSlots"][1]["outlineMix"] = outline.INK_MIX["npc"]
        S.ink_slot(e["materialSlots"][1])
        e["inkGate"] = ND.held_ink_gate(hb, man, "a held prop" if kind != "barrow" else "the escort's barrow")
        man.set_asset(name, e)
        man.save()
        objs.append((name, obj, kind))
        print(f"[prop] {name}: {M.tri_count(obj, 0)} + {M.tri_count(obj, 1)} tris, sockets "
              f"{[s['name'] for s in socks]}, held by {hb}")
    pal.save()
    # the sheet shows the barrow with its wheel (Bern's mesh carries it in game)
    sheet = []
    for name, obj, kind in objs:
        if kind == "barrow":
            import march_ship as MS
            o2, s2 = MPR.build_barrow(pal, wheel=True)
            MS.to_bone_space(o2, rig, "barrow_01", s2)
            obj.hide_render = True
            sheet.append((name, o2, kind))
        else:
            sheet.append((name, obj, kind))
    files = _props_sheet(sheet)
    man = export.Manifest()
    for name, _, _ in objs:
        man.data["assets"][name]["previews"] = [f"npc_props_march/{p.name}" for p in files]
    man.save()
    return 0


ROOT_M = Matrix.Identity(4)          # the root bone's rest frame (set by ship_props: the barrow is exported in it)


def _props_sheet(objs) -> list:
    out = paths.preview_root() / "npc_props_march"
    out.mkdir(parents=True, exist_ok=True)
    for p in out.glob("*.png"):
        p.unlink()
    review.stage()
    review.hide_stage(True)
    cells = []
    res = (220, 300)
    for _, o, _ in objs:
        o.hide_render = True
    for name, o, kind in objs:
        o.hide_render = False
        for yaw, side in ND.PROP_VIEW_YAWS:
            if kind == "barrow":
                o.matrix_world = Matrix.Rotation(math.radians(yaw + 180.0), 4, "Z") @ ROOT_M
                pitch = -30
            else:
                up = -90 if kind == "lantern" else 90       # hanging props neck up, as they hang from the fist
                o.matrix_world = Matrix.Rotation(math.radians(yaw), 4, "Z") @ Matrix.Rotation(math.radians(up), 4, "X")
                pitch = -15
            bpy.context.view_layer.update()
            review.setup_render(res, 8, bg_hex="#3B3546")
            pts = review.mesh_points([o])
            review.frame_points(pts, res, fov_h=24, pitch_ue=pitch, fill=0.86, include_ground=False)
            review.set_outline_px([o], 1.8, Vector(pts.mean(0)))
            img = review.render_array()
            review.draw_text(img, 3, 3, f"{name.split('_Prop_')[-1].upper()} {side}")
            cells.append(img)
        o.hide_render = True
    written = S.save_native(out / "props.png", review.grid(cells, 4))
    for p, _ in written:
        print(f"  {p}")
    return [p for p, _ in written]


def ship_dressing() -> int:
    scene.reset()
    pal = Palette(F.FAMILY)
    obj, socks = MPR.build_anvil(pal)
    v = ND._ship_grounded_static(
        MPR.ANVIL, "Props", obj, pal, "npc", theme.themed_ids(["npc_scenery_anvil"], "march"), socks, [1, 1], False,
        0.62, 0.22,
        {"usedBy": ["SK_NPC_Blacksmith_March"], "displayName": {"zh": "焦砧", "en": "Charred Anvil"},
         "outlineNote": "NPC scenery, not camp decor: spawned with the smith (SK_NPC_Blacksmith_March.scenery) and "
                        "struck by his Work_Hammer loop, so it is inked with the NPC class like him",
         "march": {"theme": "march", "package": "A9", "redresses": "SM_Prop_Camp_AnvilStump"},
         "note": "the plains anvil stump charred black (same footprint and strike socket); placed by "
                 "SK_NPC_Blacksmith_March.scenery; socket `strike` = where FX_HammerStrike sparks spawn"})
    pal.save()
    print(f"[dressing] {MPR.ANVIL}: ink {v['pass'] if v else None}")
    return 0 if (v or {}).get("pass", True) else 1


# (kit asset, march look id, group)
KIT = [
    ("SK_NPCKit_HalfPlate", "QuestElder_March", "halfplate"),
    ("SK_NPCKit_Gorget", "QuestElder_March", "gorget"),
    ("SK_NPCKit_Top_Cassock", "KnellChaplain", "top"),
    ("SK_NPCKit_Hat_Hood", "PlainsWanderer_March", "hood"),
    ("SK_NPCKit_FeatherCollar", "CrowTrader", "collar"),
    ("SK_NPCKit_RelicTray", "WanderingMerchant_March", "tray"),
]


def ship_kit() -> int:
    out = paths.preview_root() / "npc_kit_march"
    out.mkdir(parents=True, exist_ok=True)
    for p in out.glob("*.png"):
        p.unlink()
    cells = []
    by_look: dict = {}
    for name, look, group in KIT:
        by_look.setdefault(look, []).append((name, group))
    man = export.Manifest()
    for look, parts in by_look.items():
        npc = ML.find(look)
        B = S.build(npc)
        B.body.hide_render = True
        for o, _ in B.items.values():
            o.hide_render = True
        for o, *_ in B.scenery:
            o.hide_render = True
        A = S.anims(B)
        for name, group in parts:
            obj = B.folk.build_kit_part(B.pal, B.rig, group, name)
            if obj is None:
                print(f"[kit] {name}: empty, skipped")
                continue
            hull = M.tri_count(obj, 1)
            if not hull:
                obj.data.materials.pop(index=1)
            fbx = export.export_skeletal_mesh(B.rig, [obj], name, "NPCs")
            e = export.skeletal_entry(B.rig, obj, fbx, "NPCs", B.pal, "npc", [], [], extra={
                "kitPart": group, "fromLook": npc.asset, "build": npc.look.build,
                "assembly": "leader-pose component on a SKEL_Human NPC; hats and the hood are rigid on `head` (the "
                            "hood's cowl blends neck / clavicles), the other parts are bind-pose specific to their build",
                "march": {"theme": "march", "package": "A9"},
                "previews": ["npc_kit_march/kit_parts.png"]})
            e["inkGate"] = ND.held_ink_gate([npc.asset], man, "a kit part (the merged look's generator)")
            if hull:
                e["materialSlots"][1]["outlineMix"] = outline.INK_MIX["npc"]
                S.ink_slot(e["materialSlots"][1])
            else:
                del e["materialSlots"][1:]
            man.set_asset(name, e)
            print(f"[kit] {name}: {M.tri_count(obj, 0)} + {hull} tris ({group} of {npc.asset})")
            R0 = A.ready()
            if group == "top":
                # a garment cell, not a pose cell (review: posed with the bell and the chest hand the cassock read as
                # a giant black glove): from the front, the arms hanging at rest
                R0 = R0.but(hand_l=A.hang_hand("l"), hand_r=A.hang_hand("r"), elbow_l=Vector((0.6, 0.8, -0.2)),
                            elbow_r=Vector((-0.6, 0.8, -0.2)), wrist_l=(0, 0, 0), wrist_r=(0, 0, 0), lean=0.0,
                            head_pitch=0.0)
            anim.apply_pose(A.solve(R0))
            rv = review.Review(name, B.rig.obj, [obj], 1.8, blob_radius=0.0, out_dir=out, game_outline_px=S.PX,
                               outline_class="npc")
            review.hide_stage(True)
            rv.facing("front" if group == "top" else "se")
            res = (200, 240)
            review.setup_render(res, 8, bg_hex="#3B3546")
            pts = review.mesh_points([obj])
            review.frame_points(pts, res, fov_h=24, pitch_ue=-25, fill=0.86, include_ground=False)
            rv.ink(at=Vector(pts.mean(0)))
            img = review.render_array()
            review.draw_text(img, 3, 3, name[len("SK_NPCKit_"):].upper())
            cells.append(img)
            obj.hide_render = True
            anim.clear_pose(B.rig)
        B.pal.save()
        man.set_palette(B.pal)
    files = [q for q, _ in S.save_native(out / "kit_parts.png", review.grid(cells, 6, pad=2))]
    for name, _, _ in KIT:
        e = man.data["assets"].get(name)
        if e is not None:
            e["previews"] = [f"npc_kit_march/{q.name}" for q in files]
    man.save()
    for q in files:
        print(f"  {q}")
    return 0


def lineup() -> int:
    out = paths.preview_root() / "npc_family_march"
    out.mkdir(parents=True, exist_ok=True)
    for p in out.glob("*.png"):
        p.unlink()
    cells, close = [], []
    hw = 0.06
    hh = hw * 1920 / 1080 * 0.82
    WIDE = 1.35                                     # room for Bern's barrow across the cell

    def box_of(cam, wide: float = WIDE, objs=None):
        c = review.project(cam, Vector((0, 0, 0.92)))
        if objs:
            # centre on the screen bounds of these meshes (Bern + his barrow, 1.9 m long: centred on him, the load
            # ran out of the cell)
            ps = [review.project(cam, o.matrix_world @ Vector(k)) for o in objs for k in o.bound_box]
            c = ((min(p[0] for p in ps) + max(p[0] for p in ps)) / 2, (min(p[1] for p in ps) + max(p[1] for p in ps)) / 2)
        return (c[0] - hw * wide, c[1] - hh, c[0] + hw * wide, c[1] + hh)
    heights = []
    for n in ML.NPCS:
        B = S.build(n)
        A = S.anims(B)
        S.attach(B)
        clip = A.work()
        act = anim.bake(B.rig, clip, n.token)
        anim.assign(B.rig, act)
        anim.set_frame(0)
        rv = S.make_review(B, out)
        objs = None
        if n.look.item == "barrow":
            # Bern + his 1.9 m barrow do not fit a figure cell in se (the load runs down-screen past his feet): in
            # profile the barrow lies across the cell, centred on their screen bounds
            rv.facing("side")
            objs = [o for o in rv.meshes if not o.hide_render]
        else:
            rv.facing("se")
        review.setup_render((1920, 1080), 8)
        cam = review.game_camera()
        rv.game_outline(1080, Vector((0, 0, 0.8)))
        img, st = rv.game_shot(box_of(cam, WIDE, objs=objs), measure=True, tag=f"lineup {n.id}")
        review.draw_text(img, 3, 3, n.display["en"].upper()[:22])
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
        review.draw_text(im2, 3, 3, n.display["en"].upper()[:24])
        close.append(im2)
    w = ND._warrior_cell(box_of)
    if w is not None:
        cells.append(w)
    S.save_native(out / "lineup_game.png", review.grid(cells, 6, pad=2))
    S.save_native(out / "lineup_close.png", review.grid(close, 6, pad=2))
    for nm, h in heights:
        print(f"  {nm:34s} crown {h:.2f} m")
    print(f"  {out}")
    return 0

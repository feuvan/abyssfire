"""Modular townsfolk kit parts ``SK_NPCKit_*`` (spec art-inventory-ch1.md §5 "the kit parts stay available for later
chapters", §15 checklist: bodies slim / normal / stout, head, hair, beards, hats, tops, apron, mantle, pack).

Each part is the very generator the merged ``SK_NPC_<Id>`` meshes use (``folk.Folk.build_kit_part``), exported on the
rig (``SKEL_Human``, build-specific bind pose) of the Chapter-1 look that wears it, in that look's colours. In UE a
look can be assembled from them with leader-pose components on any ``SKEL_Human`` NPC (head / hair / beard / hat
parts are rigid on ``head``; tops, apron, mantle, pack are build-specific). Assembled, the parts of one look
reproduce its merged mesh exactly (no duplicated layers: the top owns the arms, the body the hands, legs and boots).

Review: ``Previews/npc_kit/kit_parts_a.png`` / ``_b.png`` (every part on its own, READY pose of its look, the shipped ink).
"""
from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
for _p in (HERE.parent, HERE.parent / "heroes", HERE.parent / "monsters", HERE):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector  # noqa: E402

from kit import anim, export, mesh as M, outline, paths, pngio, review  # noqa: E402

import looks as LK  # noqa: E402

# (kit asset, look id, group)
KIT = [
    ("SK_NPCKit_Head", "Merchant", "head"),
    ("SK_NPCKit_Body_Slim", "PlainsHerbalist", "body"),
    ("SK_NPCKit_Body_Normal", "WanderingMerchant", "body"),
    ("SK_NPCKit_Body_Stout", "Merchant", "body"),
    ("SK_NPCKit_Hair_Bald", "Blacksmith", "hair"),
    ("SK_NPCKit_Hair_Short", "Merchant", "hair"),
    ("SK_NPCKit_Hair_Long", "QuestElder", "hair"),
    ("SK_NPCKit_Hair_Bun", "PlainsHerbalist", "hair"),
    ("SK_NPCKit_Hair_Ponytail", "RescueLostTraveler", "hair"),
    ("SK_NPCKit_Beard_Braided", "Blacksmith", "beard"),
    ("SK_NPCKit_Beard_Mustache", "Merchant", "beard"),
    ("SK_NPCKit_Beard_Long", "QuestElder", "beard"),
    ("SK_NPCKit_Beard_Stubble", "PlainsWanderer", "beard"),
    ("SK_NPCKit_Beard_Short", "WanderingMerchant", "beard"),
    ("SK_NPCKit_Hat_Cap", "Merchant", "hat"),
    ("SK_NPCKit_Hat_Headband", "PlainsHerbalist", "hat"),
    ("SK_NPCKit_Hat_WideBrim", "PlainsWanderer", "hat"),
    ("SK_NPCKit_Top_Tunic", "Blacksmith", "top"),
    ("SK_NPCKit_Top_Vest", "Merchant", "top"),
    ("SK_NPCKit_Top_Robe", "QuestElder", "top"),
    ("SK_NPCKit_Top_Coat", "PlainsWanderer", "top"),
    ("SK_NPCKit_Apron", "Blacksmith", "apron"),
    ("SK_NPCKit_Mantle", "QuestElder", "mantle"),
    ("SK_NPCKit_Scarf", "PlainsWanderer", "mantle"),
    ("SK_NPCKit_Pack", "WanderingMerchant", "pack"),
    ("SK_NPCKit_Specs", "StashKeeper", "specs"),
    ("SK_NPCKit_KeyRing", "StashKeeper", "keyring"),
]


def ship_kit() -> int:
    import npc_ship as S
    out = paths.preview_root() / "npc_kit"
    out.mkdir(parents=True, exist_ok=True)
    cells = []
    by_look: dict = {}
    for name, look, group in KIT:
        by_look.setdefault(look, []).append((name, group))
    man = export.Manifest()
    for look, parts in by_look.items():
        npc = LK.find(look)
        B = S.build(npc)                       # scene reset + the look's rig and merged mesh (hidden below)
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
            fbx = export.export_skeletal_mesh(B.rig, [obj], name, "NPCs")
            e = export.skeletal_entry(B.rig, obj, fbx, "NPCs", B.pal, "npc", [], [], extra={
                "kitPart": group, "fromLook": npc.asset, "build": npc.look.build,
                "assembly": "leader-pose component on a SKEL_Human NPC; head / hair / beard / hat / specs are rigid on "
                            "`head`, the other parts are bind-pose specific to their build",
                "previews": ["npc_kit/kit_parts_a.png", "npc_kit/kit_parts_b.png"]})
            e["materialSlots"][1]["outlineMix"] = outline.INK_MIX["npc"]
            man.set_asset(name, e)
            print(f"[kit] {name}: {M.tri_count(obj, 0)} + {M.tri_count(obj, 1)} tris ({group} of {npc.asset})")
            # review cell in the look's READY pose
            anim.apply_pose(A.solve(A.ready()))
            rv = review.Review(name, B.rig.obj, [obj], 1.8, blob_radius=0.0, out_dir=out,
                               game_outline_px=outline.SCREEN_PX_1080["npc"], outline_class="npc")
            review.hide_stage(True)
            rv.facing("se")
            res = (180, 220)
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
    man.save()
    for i, (lo, hi) in enumerate(((0, 14), (14, len(cells)))):
        p = out / f"kit_parts_{'ab'[i]}.png"
        pngio.write_png_budget(p, review.grid(cells[lo:hi], 7, pad=2))
        print(f"  {p}")
    return 0

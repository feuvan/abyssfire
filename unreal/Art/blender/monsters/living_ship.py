"""Ship helpers of the Living (dark remake A3): LOD1 to the kit budget, the shared QA groups, the manifest extras
(family marker, the remake's wind-up tints) and the hunt-leader previews — on top of ``mship`` (bake, FBX + LOD1,
manifest merge, per-frame QA, ink gate, re-import check, the review set).

Budgets (monsters.md §2.2, the Unburied kit the Living share): LOD0 ≤ 7 k, LOD1 ≤ 3 k toon triangles; hull ≤ 0.75 ×
toon (the goblin family's rule).
"""
from __future__ import annotations

import json
import math

import bpy
import numpy as np
from mathutils import Vector

from kit import anim, mesh as M, review

import living as LV
import mship

LOD0_BUDGET = 7000
LOD1_BUDGET = 3000
# monsters.md §1.2 / DR5: enemy wind-up tints are the telegraph pair (bone fill → crimson for elites), never ember
TELEGRAPH = {"normal": "#FFE8DC", "elite": "#D11F2E"}
TRAIL = "#E8E0CC"             # bone-white weapon trail (the goblins' value; outside the ember band)
# art review r1 (item 11): in game_corpse the blob shadow stayed at the actor's root and floated off the fallen body
DEATH_SHADOW = {
    "deathFollowBone": "pelvis", "deathFollowMs": [380, 760], "deathFadeAlpha": 0.35,
    "note": "Death: the body folds forward and lies ~0.8-1.0 m in front of the actor's root (the drummer on his side). "
            "Move the blob shadow under the pelvis bone's ground projection (XY, Z stays on the ground) between the "
            "kneel (380 ms) and the face-down pose (760 ms) and keep it there; a runtime that cannot follow a bone "
            "fades it to deathFadeAlpha over the same span instead (the fallen body's contact shadow is the floor ink)"}


def march_stage() -> None:
    """Every review stage of the ship on the march ground (the Unburied's ``unburied.march_stage``): the Living are
    judged where they stand in game and beside the Unburied previews."""
    import unburied as UB
    UB.march_stage()


def items_sheet_without(names) -> None:
    """``mship.items_sheet`` leaving out build-time copies (``names``) that are never exported (the drummer's left
    stick: art review r1 item 12, reviewers took the ``_L`` cells for a shipped asset)."""
    base = getattr(mship.items_sheet, "_af_base", mship.items_sheet)

    def items_sheet(ms, rv):
        keep = ms.items
        ms.items = [it for it in keep if it.asset not in names]
        try:
            return base(ms, rv)
        finally:
            ms.items = keep
    items_sheet._af_base = base
    mship.items_sheet = items_sheet


def build_lod1(folk, pal, rig, budget: int = LOD1_BUDGET, extra=None):
    """LOD1 within ``budget`` toon triangles: built at the kit ratio, rebuilt with the ratio scaled to the budget
    while the exact parts (face, sleeves) keep it above."""
    ratio = folk.LOD1_RATIO
    body1 = folk.build(pal, rig, lod=1, extra=extra)
    n0 = n = mship.toon_tris(body1)
    for _ in range(8):
        if n <= budget:
            break
        me = body1.data
        bpy.data.objects.remove(body1, do_unlink=True)
        bpy.data.meshes.remove(me)
        ratio *= (budget - 120) / n
        body1 = folk.build(pal, rig, lod=1, lod_ratio=ratio, extra=extra)
        n = mship.toon_tris(body1)
    if n != n0:
        print(f"[lod1] {folk.asset}: {n0} toon tris at {folk.LOD1_RATIO} -> {n} at {ratio:.3f}")
    return body1


def part_names(body) -> list[str]:
    return sorted({n for n in json.loads(body["af_parts"]) if not n.endswith("_hull")})


def qa_groups(body, grip_parts=("hand_r", "thumb_r", "wrist_r", "forearm_r")) -> dict:
    names = part_names(body)
    skirt = [n for n in names if n.startswith(("skirt", "hem_band", "fold", "rope_end", "belt_rag", "quilt"))]
    legs = [n for n in names if n.startswith(("leg_", "boot_"))]
    return {"body_nf": mship.parts_except(body, grip_parts), "skirt": skirt, "legs": legs}


def marker_extra(names) -> dict:
    return {"marker": {"swatches": list(names), "family": "the Living",
                       "kind": "crimson cloth (no glow, monsters.md 1.2)"}}


def remake_extra(role: str, contact_ms: float, ref_speed: float, notes: str) -> dict:
    return {"refSpeedCmS": round(ref_speed * 100.0, 1), "attackStyle": role,
            "remake": {"family": "living", "palette": LV.FAMILY, "voiceFamily": "Human",
                       "contactMs": contact_ms, "spec": "Docs/dark-remake/monsters.md 2.3"},
            "notes": notes}


def install_coat_qa() -> None:
    """``mship.qa_run`` with the NPCs' cloth rule for skirted bodies: ``npc_ship.ToonGroundQA`` (items' ground on
    their toon surface) + ``npc_ship.CoatLegQA`` (``coatLegs``: trouser / boot vertices that show through the robe
    or coat; a triangle-pair test counts the legs leaving under the hem as hits)."""
    import npc_ship as NS

    def qa_run(ms, actions):
        names = json.loads(ms.body["af_parts"])
        items = {it.asset: it.obj for it in ms.items if it.obj.parent is not None}
        grips = {it.asset: it.grip for it in ms.items if it.grip is not None}
        groups = {"body": "*", **ms.qa_groups}
        q = NS.ToonGroundQA(ms.A.rig, ms.body, items, names, groups, dict(ms.qa_pairs), grips)
        F = getattr(ms.A, "F", None)
        if F is not None and F.skirted:
            q.coat = NS.CoatLegQA(ms.A.rig, ms.body, names, F.skirt_z0, F.skirt_z1)
        q.debug = True                         # the parts behind every hit (printed for the failing frames)
        rep = q.run(ms.clips, actions)
        for clip, r in rep.items():
            hits = {}
            for f in r["frames"]:
                for metric, parts in f.get("parts", {}).items():
                    for nm, n in parts.items():
                        hits.setdefault(metric, {}).setdefault(nm, 0)
                        hits[metric][nm] += n
            for metric, parts in hits.items():
                top = sorted(parts.items(), key=lambda x: -x[1])[:6]
                print(f"  [qa parts] {clip} {metric}: " + ", ".join(f"{k} {v}" for k, v in top))
        return rep
    mship.qa_run = qa_run


def corpse_preview(ms, rv, actions, clip: str = "Death", facings=("se", "sw", "ne")) -> None:
    """``game_corpse.png``: the last Death frame (face down, the body lying ~1 m forward of the actor) at the W1
    camera, native 1080p, each cell centred on the fallen body and ink-gated (``mship.game_sheet`` crops round the
    actor's origin and cut the corpse)."""
    from kit import shading
    rig = ms.A.rig
    acts = dict(zip([c.name for c in ms.clips], actions))
    c = next(c for c in ms.clips if c.name == clip)
    fr = c.frames
    meshes = [ms.body] + [mship.shown(it) for it in mship.held(ms)]
    cells = []
    review.setup_render((1920, 1080), 8)
    hw = 0.085
    for fac in facings:
        rv.facing(fac)
        anim.assign(rig, acts[clip])
        anim.set_frame(fr)
        bpy.context.view_layer.update()
        pts = review.mesh_points(meshes, step=4)
        ctr = Vector(pts.mean(0))
        cam = review.game_camera()
        rv.game_outline(1080, ctr)
        p = review.project(cam, ctr)
        hh = hw * 1920 / 1080
        box = (p[0] - hw, p[1] - hh, p[0] + hw, p[1] + hh)
        with mship.FxOff(ms, rv):
            img, st = rv.game_shot(box, measure=True, tag=f"game_corpse.png {clip} F{fr} {fac}")
        review.draw_text(img, 3, 3, f"{clip} F{fr} {fac} (CENTRED ON THE BODY)")
        review.draw_text(img, 3, img.shape[0] - 10, rv.ink_label(st), rgb=(255, 220, 140))
        cells.append(img)
    rv._save("game_corpse.png", review.grid(cells, len(cells)))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)
    rv.facing("se")


def report_budget(ms) -> None:
    t0 = mship.toon_tris(ms.body)
    h0 = M.tri_count(ms.body, 1)
    t1 = mship.toon_tris(ms.lod1) if ms.lod1 is not None else 0
    print(f"[budget] {ms.asset}: LOD0 {t0} toon (<= {LOD0_BUDGET}) + {h0} hull ({h0 / max(1, t0):.2f} x), "
          f"LOD1 {t1} (<= {LOD1_BUDGET})")


# ── hunt leaders (variants): previews with the elite marker ──────────────────────────────────────────────
def hunt_previews(ms, rv, actions, variants: dict, keys=(("Idle", 0, "se"), ("Attack01", 25, "sw"))) -> None:
    """``hunt_leaders.png``: every hunt variant ×scale with its attachments and the W1 march elite marker
    ``SM_FX_EliteCrown_March`` (``elite_crown@march``: ``elite_sigil.build_crown_march``, the shipped crown's
    generator, its triangle count checked against the manifest entry; tarnished iron, a crimson stone, the runtime's
    crimson glow card ``elite_sigil.crown_fx``) on ``fx_overhead`` at the W1 camera, native 1080p, the ink measured
    without the crown (the crown ships its own gate); ``hunt_close.png``: READY close-ups se / front / back with
    the same crown. (Art review r1 item 4: the r0 sheets drew a plains-glow crown and a re-paletted copy.)"""
    import elite_crown as EC
    import elite_sigil as ES
    from kit import export
    rig = ms.A.rig
    acts = dict(zip([c.name for c in ms.clips], actions))
    crown = ES.build_crown_march(ms.pal)
    shipped = (export.Manifest().data["assets"].get(ES.CROWN) or {}).get("triangles", {}).get("toon")
    built = M.tri_count(crown, 0)
    crown_tag = ES.CROWN.replace("SM_FX_", "").upper() + ("" if shipped in (None, built) else " (MISMATCH)")
    print(f"[hunt] crown {ES.CROWN}: {built} toon tris (manifest {shipped})")
    glow = ES.glow_cards(crown, ES.crown_fx())
    crown.hide_render = True
    base, glows0 = list(rv.meshes), rv.glows
    cells, close = [], []
    try:
        for vid, var in variants.items():
            objs = [it.obj for it in ms.items if it.variant == vid]
            for o in objs:
                o.hide_render = False
            sc = var["scale"]
            rig.obj.scale = (sc,) * 3
            label = var["displayName"]["en"].upper()
            for clip, fr, fac in keys:
                rv.meshes[:] = base + objs
                rv.glows = None
                rv.facing(fac)
                anim.assign(rig, acts[clip])
                anim.set_frame(fr)
                review.setup_render((1920, 1080), 8)
                cam = review.game_camera()
                zc = ms.height * 0.5 * sc
                rv.game_outline(1080, Vector((0, 0, zc)))
                c = review.project(cam, Vector((0, 0, zc)))
                hw = 0.075 * sc
                hh = hw * 1920 / 1080
                box = (c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh)
                with mship.FxOff(ms, rv):
                    _, st = rv.game_shot(box, measure=True, tag=f"hunt_leaders.png {vid} {clip} F{fr} {fac}")
                EC.place_on(crown, rig, cam)
                crown.hide_render = False
                rv.meshes.append(crown)
                rv.glows = glow
                rv.game_outline(1080, Vector((0, 0, zc)))
                img, _ = rv.game_shot(box, measure=False)
                crown.hide_render = True
                review.draw_text(img, 3, 3, f"{label} X{sc:g} {clip} F{fr} {fac}")
                review.draw_text(img, 3, 12, crown_tag, rgb=(200, 200, 200))
                review.draw_text(img, 3, img.shape[0] - 10, rv.ink_label(st), rgb=(255, 220, 140))
                cells.append(img)
            anim.assign(rig, acts["Idle"])
            anim.set_frame(0)
            res = (256, 320)
            for fac, pitch in (("se", -35.0), ("front", -30.0), ("back", -35.0)):
                rv.facing(fac)
                rv.meshes[:] = base + objs
                review.setup_render(res, 8)
                EC.place_on(crown, rig, review.game_camera())
                crown.hide_render = False
                rv.meshes.append(crown)
                pts = review.mesh_points(rv.meshes)
                cam = review.frame_points(pts, res, fov_h=26, pitch_ue=pitch, fill=0.92)
                EC.place_on(crown, rig, cam)
                rv.ink(at=Vector(pts.mean(0)))
                img = review.render_array()
                crown.hide_render = True
                review.draw_text(img, 3, 3, f"{label} {fac.upper()}")
                review.draw_text(img, 3, 12, crown_tag, rgb=(200, 200, 200))
                close.append(img)
            for o in objs:
                o.hide_render = True
    finally:
        rig.obj.scale = (1.0, 1.0, 1.0)
        rv.meshes[:] = base
        rv.glows = glows0
        crown.hide_render = True
        rv.facing("se")
    rv._save("hunt_leaders.png", review.grid(cells, len(keys)))
    rv._save("hunt_close.png", review.grid(close, 3))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)

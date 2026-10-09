"""Ship the warrior: bake every clip, export FBX (SK + A_ per clip + weapon SMs), write manifest entries,
verify the exported files by re-importing them, and render the review set into ``Art/Previews/hero_warrior/``.

Called by ``warrior.py`` (no flags). ``--no-previews`` skips the renders, ``--no-verify`` the re-import check.
"""
from __future__ import annotations

import json
import math
import time
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Vector

from kit import anim, export, mesh as M, outline, paths, pngio, review, rig as R, scene, shading

import common as C
import qa
import warrior as W
import warrior_clips as WC

PREVIEW_DIR_NAME = "hero_warrior"
LOD1_SCREEN_SIZE = 0.15      # (was 0.3: LOD1 would have drawn at every gameplay distance, see the manifest note)
# spec §3 general + §3.1 FX attachments (for the UE module; colours are the web's)
FX = {
    "classColor": "#D0473A", "impactColor": "#FFD98A", "spiritColor": "#FFB45C",
    # art review 6: the focal glow must pop at 1080p 1:1 — a 10 cm additive card (web r 3.2 u ≈ 9.6 cm) with the
    # web light falloff, #FF8A2A with a #FFD08A hot core, over the 4.2 cm emissive ember (+ bloom on the emissive)
    "visorGlow": {"socket": "visor", "color": "#FF8A2A", "radiusCm": 10.0, "alpha": 0.75, "alphaPerFx": 0.25,
                  "coreColor": "#FFD08A", "coreRadiusCm": 3.5, "coreAlpha": 0.9, "blend": "additive",
                  "falloff": [list(x) for x in review.GLOW_STOPS], "facingFade": "clamp(2 * dot(visorFwd, toCamera) + 0.3, 0, 1)",
                  "offAtDeathFraction": 0.6},
    "bloom": {"source": "palette P.b emissive regions", "lobesSigmaPx1080": [list(x) for x in review.BLOOM_LOBES],
              "strength": review.BLOOM_STRENGTH, "note": "desktop / high mobile; the review previews approximate it"},
    "attackTrail": {"color": "#DFE8FF", "alpha": 0.55, "sockets": ["tip", "mid"], "samples": 7, "sampleMs": 13},
    "castTrail": {"color": "#FF8A2A", "alpha": 0.55, "sockets": ["tip", "mid"]},
    "castBladeGlow": {"color": "#FF8A2A", "radiusCm": 33, "alpha": 0.55, "tipColor": "#FFD08A",
                      "tipRadiusCm": 18, "tipAlpha": 0.8, "embers": {"count": 6, "color": "#FFB060"}},
    "heroHalo": {"color": "#FFEEDD", "radiusCm": 200},
}


def ship(previews: bool = True, verify: bool = True) -> dict:
    t0 = time.time()
    pal, rig, body, sword, shield, sockets = W.build_all()
    print(f"[build] body {M.tri_count(body, 0)} toon + {M.tri_count(body, 1)} hull tris; sword "
          f"{M.tri_count(sword, 0)}+{M.tri_count(sword, 1)}; shield {M.tri_count(shield, 0)}+{M.tri_count(shield, 1)}; "
          f"bones {len(rig.obj.data.bones)}")
    st = R.weight_stats(body)
    print(f"[weights] max {st['maxInfluences']} influences, {st['unweighted']} unweighted, "
          f"{st['notNormalised']} not normalised")
    assert st["unweighted"] == 0 and st["maxInfluences"] <= 4
    # LOD1 (spec §1.9 hero 7 k): same generator, decimated parts, sub-readability details left out
    body1 = W.build_body(pal, rig, lod=1)
    st1 = R.weight_stats(body1)
    assert st1["unweighted"] == 0 and st1["maxInfluences"] <= 4
    print(f"[build] LOD1 {M.tri_count(body1, 0)} toon + {M.tri_count(body1, 1)} hull tris")
    # weapons (exported unattached, grip at the origin)
    shield_sockets = [{"name": "center", "pos": (W.SHIELD_CX, W.SHIELD_FACE_Y + 0.02, W.SHIELD_CZ)},
                      {"name": "emblem", "pos": (W.SHIELD_CX, W.SHIELD_FACE_Y + 0.025, W.SHIELD_CZ - 0.02)}]
    sw = C.ship_weapon(sword, W.SWORD, pal, sockets, "weapon_r", "sword",
                       extra={"heldBy": [W.ASSET],
                              "lengthCm": round((W.SWORD_TIP - W.SWORD_POMMEL_END) * 100.0, 1),
                              "bladeCm": round((W.SWORD_TIP - W.SWORD_BLADE0) * 100.0, 1),
                              "guardCm": round(W.SWORD_GUARD_HALF * 200.0, 1)})
    sh = C.ship_weapon(shield, W.SHIELD, pal, shield_sockets, "weapon_l", "shield",
                       extra={"heldBy": [W.ASSET], "faceAxis": "+Y (bone forward)",
                              "sizeCm": [round(W.SHIELD_W * 100.0, 1), round(W.SHIELD_H * 100.0, 1)]})
    # clips
    A = WC.WarriorAnims(rig, W.D)
    clips = A.all_clips()
    actions = []
    for c in clips:
        actions.append(anim.bake(rig, c, W.TOKEN))
    sk = export.export_skeletal_mesh(rig, [body], W.ASSET, "Characters")
    sk1 = export.export_skeletal_mesh(rig, [body1], f"{W.ASSET}_LOD1", "Characters")
    body1.hide_render = True              # previews draw LOD0 unless a sheet shows LOD1 on purpose
    entries = []
    for c, a in zip(clips, actions):
        fbx = export.export_animation(rig, a, "Characters")
        e = export.clip_entry(c, a, fbx)
        if c.name == "HurtAdd":
            e["additiveBase"] = {"type": "LocalAnimFrame", "anim": a.name, "frame": 0,
                                 "note": "Local-space additive; base = its own frame 0 (the READY pose)"}
        if c.name == "Portrait":
            e["staticPose"] = True
        if c.name.startswith("Cast_"):
            e["signature"] = True
            e["skill"] = {"Cast_Whirlwind": "whirlwind", "Cast_Charge": "charge"}[c.name]
        if c.name == "Cast02":
            e["skills"] = ["shield_wall", "iron_fortress", "taunt_roar", "frenzy", "vengeful_wrath", "war_stomp"]
        entries.append(e)
    man = export.Manifest()
    man.set_palette(pal)
    entry = export.skeletal_entry(rig, body, sk, "Characters", pal, "hero", entries, ["player_warrior"],
                                  blob_radius_cm=39.0,
                                  extra={"displayName": {"zh-CN": "渊火骑士", "en": "Abyssfire Knight"},
                                         "fx": FX, "previews": [],
                                         "notes": "Dodge clip is in place; the UE module lerps a mesh offset "
                                                  "(1.8 tiles). Cast_Charge is in place; the core dashes the actor "
                                                  "(C4)."})
    entry["attachments"] = [{"object": W.SWORD, "socket": "weapon_r"}, {"object": W.SHIELD, "socket": "weapon_l"}]
    entry["lods"] = [{
        "index": 1, "asset": f"{W.ASSET}_LOD1", "fbx": paths.rel_to_export(sk1), "screenSize": LOD1_SCREEN_SIZE,
        "screenSizeNote": "UE screen size of the hero at the W1 camera is 0.21 (default distance) to 0.28 (max zoom-in): "
                          "LOD0 is the gameplay mesh (what the 1080p previews show); LOD1 only below 0.15 (far zoom-out, "
                          "small UI/cutscene shots)",
        "triangles": {"toon": M.tri_count(body1, 0), "outline": M.tri_count(body1, 1)},
        "import": "same SKEL_Human armature and bind pose; import as LOD index 1 of SK_Hero_Warrior "
                  "(USkeletalMeshEditorSubsystem::ImportLOD / Interchange LOD import) — Blender's FBX exporter "
                  "cannot write an FbxLODGroup, so the LOD ships as its own file"}]
    entry["budget"] = budget_record(body, body1, sword, shield)
    entry["materialSlots"][1]["outlineMix"] = W.HULL_INK_O     # palette P.a of every warrior region (60 % ink)
    man.set_asset(W.ASSET, entry)
    man.save()
    print(f"[export] {sk.name} + {len(entries)} clips + 2 weapons ({time.time() - t0:.1f}s)")
    result = {"sk": str(sk), "lod1": str(sk1), "anims": [e["fbx"] for e in entries],
              "weapons": [sw["fbx"], sh["fbx"]], "manifest": str(man.path)}
    # per-frame geometry gate (art review 2 / 7 / 10): weapons vs body, cape vs armour, ground — before anything
    # is rendered or re-imported
    R.attach_to_bone(sword, rig, "weapon_r")
    R.attach_to_bone(shield, rig, "weapon_l")
    rep = qa_checker(rig, body, sword, shield, pal).run(clips, actions)
    print("\n[qa] per-clip worst values (ground cm; overlaps = intersecting triangle pairs)")
    print(qa.Checker.table(rep))
    qa_fails = qa.Checker.verdict(rep, QA_LIMITS, QA_CLIP_LIMITS) + head_verdict(rep)
    for f in qa_fails:
        print("  QA FAIL " + f)
    result["qa"] = {"pass": not qa_fails, "failures": qa_fails,
                    "worst": {k: v["worst"] for k, v in rep.items()}}
    if verify:
        src = record(rig, clips, actions)
    if previews:
        out = paths.preview_root() / PREVIEW_DIR_NAME
        result["previews"], result["ink"] = render_previews(out, pal, rig, body, sword, shield, A, clips, actions, body1)
        man = export.Manifest()
        man.data["assets"][W.ASSET]["previews"] = [f"{PREVIEW_DIR_NAME}/{p.name}" for p in sorted(out.glob("*.png"))]
        for n in (W.SWORD, W.SHIELD):
            man.data["assets"][n]["previews"] = [f"{PREVIEW_DIR_NAME}/weapons.png"]
        man.data["assets"][W.ASSET]["portrait"] = f"Portraits/{PORTRAIT}.png"
        ink = result["ink"]
        man.data["assets"][W.ASSET]["inkGate"] = {
            k: ink[k] for k in ("pass", "cells", "minFracRim", "minFracDark", "medianRimPx", "maxHullLumP95", "gate")}
        man.save()
    if verify:
        result["verify"] = verify_export(Path(sk), clips, src, entry, Path(sk1))
    print(f"[ship] done in {time.time() - t0:.1f}s")
    return result


def ink_verdict(reports: list) -> dict:
    """The kit's ink gate (``review.ink_verdict``): every gated 1080p game-camera cell needs a rim of
    ≥ outlinePx1080 − 0.5 px (hero: 3 px) on ≥ 90 % of its silhouette edge pixels, a dark pixel near ≥ 90 % of
    them and hull lum p95 ≤ 40."""
    return review.ink_verdict(reports)


# ── per-frame QA (heroes/qa.py) ─────────────────────────────────────────────────────────────────────────
ARMOUR = ("cuirass", "ridge", "side_seam", "gorget", "pauldron", "pauldron_rim", "pauldron_rivet", "lame2", "lame3",
          "rerebrace", "rere_lame", "ua", "couter", "couter_wing", "vambrace", "cuff", "fist", "thumb", "knuckles",
          "helm", "helm_visor", "seam_b", "nape_rivet", "belt", "mailskirt", "fauld", "buckle")
# Baseline: 0 weapon intersections and no part more than 5 mm under the floor anywhere. The cape count allows the
# modelled yoke (the cape's top tucks under the pauldrons at rest: ~10 pairs in Idle); a tumbling roll and a fall
# wrap the cloth round the curled body where three chains cannot follow every plate (documented, reviewed in the
# back-view sheets); the run's trailing pommel brushes the flaring side edge of the cape.
QA_LIMITS = {"groundBody": -0.5, "groundR": -0.5, "groundL": -0.5, "swordShield": 0, "swordBody": 0,
             "shieldBody": 0, "capeArmour": 30}
QA_CLIP_LIMITS = {"Dodge": {"capeArmour": 140}, "Death": {"capeArmour": 130}, "Run": {"swordBody": 20}}
# attack / cast contact frames: the visor stays on the strike line (art review 3)
QA_HEAD = {"clips": ("Attack01", "Attack02", "Attack03", "Cast01"), "maxYaw": 12.0, "maxDown": 10.0}


def qa_checker(rig, body, sword, shield, pal) -> qa.Checker:
    ztop = W.D.nz - 0.028

    def interior(c) -> bool:          # the cape's side edges curl round the arms by design
        k = max(0.0, min(1.0, (ztop - c.z) / 0.735))
        return abs(c.x) <= 0.8 * (W.u(7.4) + W.u(2.2) * k)
    g = qa.Groups(armour=ARMOUR, hand_r=("fist_r", "thumb_r", "knuckles_r", "cuff_r"),
                  hand_l=("fist_l", "thumb_l", "knuckles_l", "cuff_l", "vambrace_l"),
                  yoke_rows=ztop - 0.06, cape_interior=interior, cape_skin=(pal.index("warrior.crimson"),),
                  # grip zones: the sword's grip, guard block and pommel sit in the fist; the shield's handle loop
                  # and straps hold the fist and forearm
                  item_r_grip=lambda co: co.y < 0.115,
                  item_l_grip=lambda co: co.length < 0.10 or (abs(co.x) < 0.025 and co.y < 0.095
                                                               and abs(co.z) < 0.09))
    return qa.Checker(rig, body, sword, shield, json.loads(body["af_parts"]), g)


def head_verdict(rep: dict) -> list[str]:
    out = []
    for clip in QA_HEAD["clips"]:
        for f in rep.get(clip, {}).get("frames", []):
            if "headYaw" not in f:
                continue
            if abs(f["headYaw"]) > QA_HEAD["maxYaw"] or f["headPitch"] < -QA_HEAD["maxDown"]:
                out.append(f"{clip} F{f['frame']}: visor {f['headYaw']:+.0f}° off the strike line, "
                           f"{f['headPitch']:+.0f}° pitch (limits ±{QA_HEAD['maxYaw']:.0f}°, ≥ −{QA_HEAD['maxDown']:.0f}°)")
    return out


def budget_record(body, body1, sword, shield) -> dict:
    """Spec §1.9 hero budget (LOD0 14–18 k, LOD1 7 k) and the explicit hull decision for the manifest."""
    t = M.tri_count
    weapons = {"toon": t(sword, 0) + t(shield, 0), "outline": t(sword, 1) + t(shield, 1)}
    return {
        "spec": "art-inventory-ch1 §1.9 hero: LOD0 14-18k, LOD1 7k triangles, <= 75 bones per section",
        "lod0": {"toon": t(body, 0), "outline": t(body, 1)},
        "lod1": {"toon": t(body1, 0), "outline": t(body1, 1)},
        "weapons": weapons,
        "drawnLod0": t(body, 0) + t(body, 1) + weapons["toon"] + weapons["outline"],
        "decision": "The §1.9 figures budget the toon surface. The baked outline hull (P7) is a second, cheap draw "
                    "(same skinning, unlit one-colour pixel shader, front faces culled) budgeted separately at "
                    "<= 0.5x the toon triangles: hidden or small parts (rivets, seams, mail rows, inner lames, "
                    "bands inside a neighbour's hull) get no hull or a low-poly hull proxy (Builder.add(hull=...)).",
    }


# ── previews ────────────────────────────────────────────────────────────────────────────────────────────
BACK_CLIPS = ("Attack01", "Attack02", "Attack03", "Cast01", "Cast02", "Cast_Whirlwind", "Cast_Charge", "Dodge",
              "Death")
BACK_FACINGS = ("ne", "back", "nw")


def back_sheets(rv, rig, meshes, clips, actions, cell=(168, 204)) -> None:
    """Art review 10: every attack / cast (+ dodge, death) from behind at the game pitch — rows ne / back / nw,
    columns = the clip's key poses (+ the contact / release frame, boxed). ``anim_back_<Clip>.png``."""
    acts = dict(zip([c.name for c in clips], actions))
    for c in clips:
        if c.name not in BACK_CLIPS:
            continue
        anim.assign(rig, acts[c.name])
        notif = {scene.ms_to_frame(d["ms"]): d for d in c.notify_list() if d["name"] in ("Contact", "Release")}
        keys = getattr(c, "key_poses_ms", None) or [c.length_ms * i / 7 for i in range(8)]
        frames = sorted({min(c.frames, scene.ms_to_frame(ms)) for ms in keys} | set(notif))
        pts = []
        for fc in BACK_FACINGS:
            rv.facing(fc)
            for fr in frames:
                anim.set_frame(fr)
                pts.append(review.mesh_points(meshes, step=4))
        pts = np.concatenate(pts)
        review.setup_render(cell, 4)
        rows = []
        for fc in BACK_FACINGS:
            rv.facing(fc)
            review.frame_points(pts, cell, fov_h=24.0, pitch_ue=shading.CAM_PITCH_UE, fill=0.95)
            review.set_outline_px(meshes, 1.6, Vector(pts.mean(0)))
            cells = []
            for fr in frames:
                anim.set_frame(fr)
                img = review.render_array()
                review.draw_text(img, 3, 3, f"{fc} F{fr}")
                if fr in notif:
                    review.border(img)
                    review.draw_text(img, 3, 12, notif[fr]["name"], rgb=(255, 170, 70))
                cells.append(img)
            rows.append(review.grid(cells, len(cells), pad=2))
        sheet = np.vstack(rows)
        label = np.zeros((14, sheet.shape[1], 3), np.uint8)
        label[:, :] = (24, 20, 30)
        review.draw_text(label, 4, 4, f"{c.name}  BACK VIEWS NE / BACK / NW  PITCH -50  KEY POSES")
        rv._save(f"anim_back_{c.name}.png", np.vstack([label, sheet]))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)
    rv.facing("se")


def render_previews(out: Path, pal, rig, body, sword, shield, A, clips, actions, body1=None) -> list:
    out.mkdir(parents=True, exist_ok=True)
    for p in out.glob("*.png"):
        p.unlink()
    R.attach_to_bone(sword, rig, "weapon_r")
    R.attach_to_bone(shield, rig, "weapon_l")
    meshes = [body, sword, shield]
    rv = review.Review(W.ASSET, rig.obj, meshes, W.HEIGHT, blob_radius=0.39, out_dir=out,
                       game_outline_px=outline.SCREEN_PX_1080["hero"], glows=visor_glows(rig))
    idle = dict(zip([c.name for c in clips], actions))["Idle"]
    anim.assign(rig, idle)
    anim.set_frame(0)
    rv.game_view()
    rv.closeup()
    rv.turnaround()
    rv.material_ab()
    # contact sheets: one full-resolution file per clip (a stacked overview cannot stay legible in 200 KB)
    for c, a in zip(clips, actions):
        n = 2 if c.name == "Portrait" else 8
        rv.contact_sheet(rig, [(c, a)], frames=n)
        (out / "anims.png").rename(out / f"anim_{c.name}.png")
    game_poses(rv, rig, meshes, clips, actions)
    closeup_poses(rv, rig, meshes, clips, actions)
    back_sheets(rv, rig, meshes, clips, actions)
    render_portrait(rv, rig, meshes, clips, actions)
    visor_glow_check(rv, rig, meshes, clips, actions)
    if body1 is not None:
        lod_sheet(rv, rig, body, body1, sword, shield, clips, actions)
    weapons_sheet(rv, sword, shield)
    rv.facing("se")
    for p, size in rv.written:
        print(f"  {p.name:28s} {size // 1024:4d} KB")
    ink = ink_verdict(rv.ink_reports)
    print(f"\n[ink] {ink['cells']} gated 1080p cells: min rim≥3px {ink['minFracRim'] * 100:.1f} %, "
          f"min dark {ink['minFracDark'] * 100:.1f} %, median rim {ink['medianRimPx']} px; baked hull only: "
          + ", ".join(f"{b['tag']} median {b['rimPxMedian']} px" for b in ink["baked"]))
    for f in ink["failures"]:
        print("  INK FAIL " + f)
    return [str(p) for p, _ in rv.written], ink


GAME_KEYS = [("Idle", 0, "se"), ("Run", 8, "se"), ("Attack01", 9, "se"), ("Attack01", 18, "se"),
             ("Cast01", 20, "se"), ("Cast_Whirlwind", 20, "sw"), ("Cast_Charge", 20, "se"), ("Death", 45, "se")]
# art review 3: the four strikes must not freeze on one silhouette — every contact / release at game size, both
# front 3/4 views
CONTACT_KEYS = [(n, f, fac) for fac in ("se", "sw") for n, f in (("Attack01", 18), ("Attack02", 18),
                                                                ("Attack03", 18), ("Cast01", 20))]


# ink gate coverage (art review 6): the idle and run silhouettes from the other camera-relative facings too
FACING_KEYS = [(n, f, fac) for fac in ("front", "sw", "ne", "back") for n, f in (("Idle", 0), ("Run", 8))]


def game_poses(rv, rig, meshes, clips, actions) -> None:
    """Key frames at true in-game size: W1 camera, default distance, native 1080p pixels (crop)."""
    game_sheet(rv, rig, clips, actions, GAME_KEYS, "game_poses.png")
    game_sheet(rv, rig, clips, actions, CONTACT_KEYS, "game_contacts.png")
    game_sheet(rv, rig, clips, actions, FACING_KEYS, "game_facings.png")


def game_sheet(rv, rig, clips, actions, keys, fname) -> None:
    """Native 1080p crops at the W1 camera with the shipped look (UE ink width, bloom, visor glow card); every
    cell's ink rim is measured (``rv.ink_reports``, the ship's ink gate) and printed under its label."""
    acts = dict(zip([c.name for c in clips], actions))
    lens = {c.name: c.frames for c in clips}
    cells = []
    review.setup_render((1920, 1080), 8)
    for name, fr, fac in keys:
        rv.facing(fac)
        anim.assign(rig, acts[name])
        anim.set_frame(fr)
        rv.glow_scale = 0.0 if name == "Death" and fr >= FX["visorGlow"]["offAtDeathFraction"] * lens[name] else 1.0
        cam = review.game_camera()
        rv.game_outline(1080, Vector((0, 0, 0.75)))
        c = review.project(cam, Vector((0, 0, 0.75)))
        hw, hh = 0.075, 0.075 * 1920 / 1080
        img, st = rv.game_shot((c[0] - hw, c[1] - hh, c[0] + hw, c[1] + hh), measure=True,
                               tag=f"{fname} {name} F{fr} {fac}")
        review.draw_text(img, 3, 3, f"{name} F{fr}")
        review.draw_text(img, 3, img.shape[0] - 10, rv.ink_label(st), rgb=(255, 220, 140))
        cells.append(img)
    rv.glow_scale = 1.0
    rv._save(fname, review.grid(cells, 4))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)


def visor_glows(rig):
    """``Review.glows``: the runtime ``visorGlow`` card at the posed ``visor`` socket (manifest size / colours), faded
    by how much the visor faces the camera like the web ``H.vis`` (and by ``rv.glow_scale``, 0 late in Death)."""
    vg = FX["visorGlow"]
    hb = rig.obj.data.bones["head"]

    def fn(cam, res):
        vp = visor_world(rig)
        pb = rig.obj.pose.bones["head"]
        n = (rig.obj.matrix_world.to_3x3() @ pb.matrix.to_3x3() @ hb.matrix_local.to_3x3().inverted()
             @ Vector((0, -1, 0))).normalized()
        vis = max(0.0, min(1.0, 2.0 * n.dot((cam.matrix_world.translation - vp).normalized()) + 0.3))
        px, py = review.project_px(cam, vp, res)
        wpp = outline.pixel_world_size(cam, vp, res[0])
        return [dict(center_px=(px, py), radius_px=vg["radiusCm"] / 100.0 / wpp, color=vg["color"],
                     alpha=vg["alpha"] * vis, core=vg["coreColor"], core_frac=vg["coreRadiusCm"] / vg["radiusCm"],
                     core_alpha=vg["coreAlpha"] * vis)]
    return fn


CLOSE_KEYS = [("Attack01", 18, "se"), ("Attack02", 18, "se"), ("Attack03", 18, "se"), ("Cast01", 20, "se"),
              ("Cast02", 20, "se"), ("Cast_Whirlwind", 16, "se"), ("Cast_Charge", 20, "side"), ("Dodge", 6, "side"),
              ("Hurt", 0, "se"), ("Death", 45, "front")]


def closeup_poses(rv, rig, meshes, clips, actions) -> None:
    """Contact / release / key poses large enough to judge intersections and silhouettes."""
    acts = dict(zip([c.name for c in clips], actions))
    cells = []
    res = (256, 320)
    for name, fr, fac in CLOSE_KEYS:
        rv.facing(fac)
        anim.assign(rig, acts[name])
        anim.set_frame(fr)
        review.setup_render(res, 8)
        pts = review.mesh_points(meshes)
        review.frame_points(pts, res, fov_h=26, pitch_ue=-35, fill=0.92)
        review.set_outline_px(meshes, 1.8, Vector(pts.mean(0)))
        img = review.render_array()
        clip = next(c for c in clips if c.name == name)
        mark = ""
        for d in clip.notify_list():
            if scene.ms_to_frame(d["ms"]) == fr:
                mark = f" {d['name'].upper()}"
        review.draw_text(img, 3, 3, f"{name} F{fr}{mark}")
        cells.append(img)
    rv._save("poses_attack_cast.png", review.grid(cells[:5], 5))
    rv._save("poses_signature_hurt.png", review.grid(cells[5:], 5))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)


PORTRAIT = "T_UI_Portrait_Hero_Warrior"


def visor_world(rig) -> Vector:
    """Posed world position of the ``visor`` socket (head-bone relative)."""
    s = next(x for x in rig.sockets if x["name"] == "visor")
    b = rig.obj.data.bones[s["bone"]]
    pb = rig.obj.pose.bones[s["bone"]]
    return rig.obj.matrix_world @ (pb.matrix @ (b.matrix_local.inverted() @ Vector(s["pos"])))


PORTRAIT_INK_PX = 4.5          # silhouette ink at 512² (spec §1.3 hero ink, web: 1.1 rig units on a 96-unit sprite)


def render_portrait(rv, rig, meshes, clips, actions) -> Path:
    """Spec §9.5 / R11: 512² RGBA bust portrait, 3/4 front, transparent background, from the Portrait pose →
    ``Art/Export/Portraits/T_UI_Portrait_Hero_Warrior.png`` (+ a preview over the studio colour).

    Offline look: toon shading + **screen-space ink** (``review.ink_render``: constant 4.5 px #120C18 silhouette,
    2 px contour lines where plates overlap) instead of the baked hull, which at 1:1 left saw-teeth along the
    pauldron lames and slivers at grazing angles; and the runtime ``visorGlow`` FX (no bloom on a UI texture)
    composited at its manifest size/colour over the ember visor."""
    acts = dict(zip([c.name for c in clips], actions))
    anim.assign(rig, acts["Portrait"])
    anim.set_frame(0)
    rv.facing(-110.0)
    res = (512, 512)
    review.hide_stage(True)
    review.setup_render(res, 16)
    pts = review.mesh_points(meshes)
    bust = pts[pts[:, 2] > 1.02]
    cam = review.frame_points(bust, res, fov_h=24, pitch_ue=-9, fill=0.96, include_ground=False)
    for m in meshes:
        outline.remove_preview(m)
    rgba = review.ink_render(meshes, res, silhouette_px=PORTRAIT_INK_PX, interior_px=2.0, depth_step=0.018)
    vg = FX["visorGlow"]
    vp = visor_world(rig)
    cx, cy = review.project_px(cam, vp, res)
    rad = vg["radiusCm"] / 100.0 / outline.pixel_world_size(cam, vp, res[0])
    review.composite_glow(rgba, (cx, cy), rad, vg["color"], 0.62, core=0.32)
    out = paths.export_root() / "Portraits" / f"{PORTRAIT}.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    pngio.write_png(out, rgba)
    review.hide_stage(False)
    bg = np.array([59, 53, 70], np.float32)
    a = rgba[:, :, 3:4].astype(np.float32) / 255.0
    comp = (rgba[:, :, :3].astype(np.float32) * a + bg * (1 - a) + 0.5).astype(np.uint8)
    review.draw_text(comp, 4, 4, PORTRAIT)
    rv._save("portrait.png", comp)
    # 2x zoom of the busiest region (pauldron, crossguard, gauntlet, visor) for the 1:1 defect check
    z = comp[150:406, 40:296].repeat(2, 0).repeat(2, 1)
    review.draw_text(z, 4, 4, "PORTRAIT 2X ZOOM")
    rv._save("portrait_zoom2x.png", z)
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)
    rv.facing("se")
    return out


def _game_crop(cx: float, cy: float, hw: float = 0.075) -> tuple[tuple[float, float, float, float], tuple[int, int]]:
    """1080p border box around a projected point (normalised) + its top-left pixel offset."""
    hh = hw * 1920 / 1080
    box = (cx - hw, cy - hh, cx + hw, cy + hh)
    return box, (int(round((cx - hw) * 1920)), int(round((1 - (cy + hh)) * 1080)))


def visor_glow_check(rv, rig, meshes, clips, actions) -> None:
    """Readability of the warrior's focal accent at 1080p (art review 6): native 1080p crops at the W1 camera with
    the UE ink width, each as the bare render (no post) and with the shipped post — emissive bloom + the runtime
    ``visorGlow`` card (manifest colour / radius / core, faded by how much the visor faces the camera)."""
    acts = dict(zip([c.name for c in clips], actions))
    cells = []
    review.setup_render((1920, 1080), 8)
    for clip, fr, fac in (("Idle", 0, "se"), ("Idle", 0, "front"), ("Idle", 0, "sw"), ("Run", 8, "front")):
        rv.facing(fac)
        anim.assign(rig, acts[clip])
        anim.set_frame(fr)
        cam = review.game_camera()
        rv.game_outline(1080)
        cx, cy = review.project(cam, Vector((0, 0, 0.95)))
        box, _ = _game_crop(cx, cy)
        img = review.render_array(box)
        lit = rv.post(img, cam, (1920, 1080), box)
        d = np.abs(lit.astype(np.int32) - img.astype(np.int32)).sum(2)
        print(f"  [visorGlow] {clip} {fac}: {int((d > 6).sum())} px changed, max ΔRGB {int(d.max())}")
        for im, tag in ((img, " NO POST"), (lit, " +BLOOM +VISORGLOW")):
            im = im.copy()
            review.draw_text(im, 3, 3, f"{clip} {fac}{tag}")
            cells.append(im)
    rv._save("game_visorglow.png", review.grid(cells, 4))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)
    rv.facing("se")


def lod_sheet(rv, rig, body, body1, sword, shield, clips, actions) -> None:
    """LOD0 vs LOD1: front-3/4 close-ups and native 1080p game crops (idle)."""
    acts = dict(zip([c.name for c in clips], actions))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)
    rv.facing("se")
    cells = []
    res = (300, 380)
    for show, hide, tag in ((body, body1, "LOD0"), (body1, body, "LOD1")):
        show.hide_render, hide.hide_render = False, True
        meshes = [show, sword, shield]
        review.setup_render(res, 8)
        pts = review.mesh_points(meshes)
        review.frame_points(pts, res, fov_h=28.0, pitch_ue=-22.0, fill=0.9)
        review.set_outline_px(meshes, 2.2, Vector(pts.mean(0)))
        img = review.render_array()
        review.draw_text(img, 3, 3, f"{tag} {M.tri_count(show, 0)}+{M.tri_count(show, 1)} TRIS")
        cells.append(img)
    for show, hide, tag in ((body, body1, "LOD0"), (body1, body, "LOD1")):
        show.hide_render, hide.hide_render = False, True
        review.setup_render((1920, 1080), 8)
        cam = review.game_camera()
        review.set_outline_px([show, sword, shield], outline.SCREEN_PX_1080["hero"], Vector((0, 0, 0.9)))
        cx, cy = review.project(cam, Vector((0, 0, 0.85)))
        box, _ = _game_crop(cx, cy, hw=0.07)
        crop = review.render_array(box)[:res[1], :res[0]]
        img = np.zeros((res[1], res[0], 3), np.uint8)
        img[:, :] = crop[0, 0]
        y0, x0 = (res[1] - crop.shape[0]) // 2, (res[0] - crop.shape[1]) // 2
        img[y0:y0 + crop.shape[0], x0:x0 + crop.shape[1]] = crop
        review.draw_text(img, 3, 3, f"{tag} 1080P 1:1")
        cells.append(img)
    body.hide_render, body1.hide_render = False, True
    rv._save("lod1.png", review.grid(cells, 4))


def weapons_sheet(rv, sword, shield) -> None:
    """The two weapon meshes on their own (front / back of the shield, both flats of the sword)."""
    cells = []
    res = (300, 360)
    saved = {}
    for o in (sword, shield):
        saved[o.name] = (o.parent, o.parent_type, o.parent_bone, o.matrix_parent_inverse.copy(), o.matrix_basis.copy())
    hidden = [o for o in bpy.data.objects if o.type == "MESH" and o not in (sword, shield)]
    for o in hidden:
        o.hide_render = True
    review.hide_stage(True)
    for o in (sword, shield):
        o.parent = None
        o.matrix_world = Matrix.Identity(4)
    # camera (W1 yaw) looks from Blender (−X, +Y): yaw the item so its flat / face normal points there
    for obj, views in ((sword, ((135, "SWORD FLAT"), (45, "SWORD EDGE"))), (shield, ((45, "SHIELD FACE"), (225, "SHIELD BACK")))):
        other = shield if obj is sword else sword
        other.hide_render = True
        obj.hide_render = False
        for yaw, label in views:
            if obj is sword:
                obj.matrix_world = Matrix.Rotation(math.radians(yaw), 4, "Z") @ Matrix.Rotation(math.radians(90), 4, "X")
            else:
                obj.matrix_world = Matrix.Rotation(math.radians(yaw), 4, "Z")
            review.setup_render(res, 8, bg_hex="#3B3546")
            pts = review.mesh_points([obj])
            review.frame_points(pts, res, fov_h=24, pitch_ue=-12, fill=0.9, include_ground=False)
            review.set_outline_px([obj], 1.6, Vector(pts.mean(0)))
            img = review.render_array()
            review.draw_text(img, 3, 3, label)
            cells.append(img)
    # hilt close-up: pommel end toward the camera (the ember cabochon), guard sweeping toward the blade
    shield.hide_render = True
    sword.hide_render = False
    d = Vector((1.0, -1.0, 1.1)).normalized()            # blade away from the W1 camera and up
    sword.matrix_world = R.frame((0, 0, 0), d, (1.0, 1.0, 0.0))
    bpy.context.view_layer.update()
    review.setup_render(res, 8, bg_hex="#3B3546")
    pts = review.mesh_points([sword])
    hilt = pts[(pts @ np.array(d)) < 0.17]
    review.frame_points(hilt, res, fov_h=24, pitch_ue=-12, fill=0.85, include_ground=False)
    review.set_outline_px([sword], 1.6, Vector(hilt.mean(0)))
    img = review.render_array()
    review.draw_text(img, 3, 3, "SWORD HILT")
    cells.append(img)
    rv._save("weapons.png", review.grid(cells, 5))
    for o in hidden:
        o.hide_render = False
    sword.hide_render = shield.hide_render = False
    review.hide_stage(False)
    for o in (sword, shield):
        par, ptype, pbone, mpi, mb = saved[o.name]
        o.parent, o.parent_type, o.parent_bone = par, ptype, pbone
        o.matrix_parent_inverse = mpi
        o.matrix_basis = mb


# ── verification (re-import what UE receives) ───────────────────────────────────────────────────────────
def record(rig, clips, actions) -> dict:
    rec = {}
    for c, a in zip(clips, actions):
        anim.assign(rig, a)
        for f in sorted({0, c.frames // 2, c.frames}):
            anim.set_frame(f)
            rec[(c.name, f)] = {pb.name: pb.matrix.copy() for pb in rig.obj.pose.bones}
    anim.assign(rig, None)
    anim.clear_pose(rig)
    return rec


def verify_export(sk_fbx: Path, clips, src: dict, entry: dict, lod_fbx: Path | None = None) -> dict:
    fails = []

    def check(ok, msg):
        print(("  ok   " if ok else "  FAIL ") + msg)
        if not ok:
            fails.append(msg)
    print("\n[verify] re-import into an empty scene")
    scene.reset()
    bpy.ops.import_scene.fbx(filepath=str(sk_fbx))
    arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    check(len(arms) == 1 and len(meshes) == 1, f"one armature + one mesh ({len(arms)}, {len(meshes)})")
    arm, me = arms[0], meshes[0]
    bones = {b.name for b in arm.data.bones}
    check(len(bones) == entry["bones"], f"{len(bones)} bones == {entry['bones']}")
    for need in ("root", "pelvis", "head", "weapon_r", "weapon_l", "cape_c_04", "cape_l_03", "tabard_f_02",
                 "plume_03", "ik_foot_l", "ik_hand_gun"):
        check(need in bones, f"bone {need}")
    check("Armature" not in bones, "no extra root bone")
    mats = [m.name for m in me.data.materials]
    check(mats == ["MI_AF_Toon_Heroes", "M_AF_Outline"], f"material slots {mats}")
    st = R.weight_stats(me)
    check(st["unweighted"] == 0 and st["maxInfluences"] <= 4, f"weights ({st['maxInfluences']} max, "
                                                                f"{st['unweighted']} unweighted)")
    lo, hi = scene.world_bounds([me])
    # crest top ≈ 1.84 m (spec §1.6) + the 1.8 cm hull; the sabaton hull dips 1.8 cm under the soles
    check(1.82 < hi.z < 1.89 and -0.025 < lo.z < 0.0, f"bind-pose bounds z {lo.z:.3f} .. {hi.z:.3f} m incl. hull")
    check("AF_Data" in me.data.color_attributes and me.data.has_custom_normals, "AF_Data colours + custom normals")
    worst_mm, worst_deg = 0.0, 0.0
    for c in clips:
        f = paths.category_dir("Characters") / f"A_{W.TOKEN}_{c.name}.fbx"
        before = set(bpy.data.actions)
        objs = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(f))
        new = [a for a in bpy.data.actions if a not in before]
        if len(new) != 1:
            check(False, f"{f.name}: one action")
            continue
        a = new[0]
        n = int(round(a.frame_range[1] - a.frame_range[0]))
        check(n == max(1, c.frames), f"{f.name}: {n} frames == {c.frames}")
        arm2 = next(o for o in bpy.data.objects if o not in objs and o.type == "ARMATURE")
        for (cn, fr), mats_ in src.items():
            if cn != c.name:
                continue
            bpy.context.scene.frame_set(int(a.frame_range[0]) + fr)
            for bn, m in mats_.items():
                pb = arm2.pose.bones.get(bn)
                if pb is None:
                    continue
                worst_mm = max(worst_mm, (pb.matrix.translation - m.translation).length * 1000)
                ang = m.to_quaternion().rotation_difference(pb.matrix.to_quaternion()).angle
                worst_deg = max(worst_deg, math.degrees(min(ang, 2 * math.pi - ang)))
        bpy.data.objects.remove(arm2)
    check(worst_mm < 1.0 and worst_deg < 0.1, f"re-imported clips match the source (max {worst_mm:.3f} mm, "
                                              f"{worst_deg:.4f} deg)")
    if lod_fbx is not None:
        objs = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(lod_fbx))
        new = [o for o in bpy.data.objects if o not in objs]
        a1 = [o for o in new if o.type == "ARMATURE"]
        m1 = [o for o in new if o.type == "MESH"]
        check(len(a1) == 1 and len(m1) == 1, f"LOD1: one armature + one mesh ({len(a1)}, {len(m1)})")
        if a1 and m1:
            check({b.name for b in a1[0].data.bones} == bones, "LOD1: same bone set as LOD0")
            mats1 = [m.name.split(".")[0] for m in m1[0].data.materials]
            check(mats1 == ["MI_AF_Toon_Heroes", "M_AF_Outline"], f"LOD1: material slots {mats1}")
            st1 = R.weight_stats(m1[0])
            check(st1["unweighted"] == 0 and st1["maxInfluences"] <= 4, "LOD1: weights")
            want = entry["lods"][0]["triangles"]
            got = {"toon": M.tri_count(m1[0], 0), "outline": M.tri_count(m1[0], 1)}
            check(got == want, f"LOD1: triangles {got} == manifest {want}")
            check(m1[0].data.has_custom_normals, "LOD1: custom normals")
    for n in (W.SWORD, W.SHIELD):
        objs = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(paths.category_dir("Weapons") / f"{n}.fbx"))
        new = [o for o in bpy.data.objects if o not in objs]
        names = sorted(o.name for o in new)
        mesh = next(o for o in new if o.type == "MESH")
        mn = [m.name.split(".")[0] for m in mesh.data.materials]
        check(mn == ["MI_AF_Toon_Heroes", "M_AF_Outline"], f"{n}: material slots {mn}")
        check(any(x.startswith("SOCKET_") for x in names), f"{n}: sockets {[x for x in names if x.startswith('SOCKET_')]}")
    print(f"[verify] {'PASS' if not fails else 'FAIL'} ({len(fails)} failures)")
    return {"pass": not fails, "failures": fails}


def main(argv) -> int:
    res = ship(previews="--no-previews" not in argv, verify="--no-verify" not in argv)
    out = {k: v for k, v in res.items() if k not in ("previews", "qa", "ink")}
    if res.get("ink"):
        out["ink"] = {k: v for k, v in res["ink"].items() if k not in ("baked",)}
    out["qa"] = {k: v for k, v in res["qa"].items() if k != "worst"}
    print(json.dumps(out, indent=1)[:3000])
    ok = res.get("verify", {"pass": True})["pass"] and res["qa"]["pass"]
    ink = res.get("ink")
    if ink is not None and not ink["pass"]:
        print("\nINK GATE FAILED (review.INK_GATE, see game_inkgate.png):\n  " + "\n  ".join(ink["failures"]))
        ok = False
    return 0 if ok else 1

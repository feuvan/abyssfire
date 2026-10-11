"""Ship / review / dev tooling shared by the mage and the rogue (``mage.py``, ``rogue.py``); the warrior keeps its own
``warrior_ship.py`` (the reference this module generalises).

A hero is described by a :class:`HeroDef` (``mage_ship.HERO``, ``rogue_ship.HERO``): asset names, the generator module
(``build_all`` / ``attach_items``), the clip factory, held items, FX block, QA groups and limits, and which frames the
review sheets show. ``cli(hero, argv, scratch)``:

* no flag — **ship**: bake every clip, per-frame geometry QA (``hero_base.PairQA``: ground, item × body, cloth × body,
  visor/eyes on the strike line at contacts), FBX export (SK + LOD1 + one ``A_`` per clip + the item SMs), manifest
  entries (merged), re-import verification, the review set in ``Art/Previews/<preview_dir>/`` with the **ink gate**
  on every game-camera cell (exit 1 on a QA, ink or verify failure); ``--no-previews`` / ``--no-verify``;
* ``--quick [--facing=se] [--pose=ready]`` — mesh iteration into the scratch directory;
* ``--anims [Clip …] [--facing=se]`` — bake + contact sheets into the scratch directory;
* ``--frames Clip:frame … [--facings=se,side,back]`` — large debug stills;
* ``--game [Clip:frame:facing …]`` — native 1080p game-camera crops (ink-gated) into the scratch directory.

Item visibility per clip (the rogue's flash-bow replaces the off-hand dagger during the archery cast) is applied by
a frame-change handler (``HeroDef.visibility``), so every review sheet and the QA see exactly what the UE module
shows when it honours the clip's ``FX_BowShow`` / ``FX_BowHide`` notifies.
"""
from __future__ import annotations

import json
import math
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable

import bpy
import numpy as np
from mathutils import Matrix, Vector

from kit import anim, export, mesh as M, outline, paths, pngio, review, rig as R, scene, shading

import common as C
import hero_base as HB


@dataclass
class Item:
    """A held / worn item exported as its own ``SM_`` (``Weapons/``)."""
    name: str
    socket: str                        # weapon_r / weapon_l (hand), a parent item's socket, or a body socket
    weapon_type: str
    kind: str = "hand"                 # hand | item (child of ``parent``'s socket) | body | swap (clip-only)
    parent: str | None = None
    extra: dict = field(default_factory=dict)


@dataclass
class HeroDef:
    asset: str
    token: str
    game_id: str
    display_name: dict
    preview_dir: str
    module: object                     # build_all(lod1) / attach_items(rig, items) / D / HEIGHT / part_report
    anims: Callable                    # (rig, D) → HeroAnims
    items: list
    blob_radius: float
    fx: dict
    eye_socket: str
    eye_glow: dict                     # runtime eye card: color, radiusCm, alpha, coreColor, coreRadiusCm, coreAlpha
    clip_meta: Callable                # (clip, entry) → None (signature / skills / notes)
    qa_groups: dict
    qa_pairs: dict
    qa_limits: dict
    qa_clip_limits: dict
    qa_head: dict                      # {"clips": (...), "maxYaw": deg, "maxDown": deg}
    item_grips: dict                   # {item: fn(co_item_local) → bool} faces inside the hand
    game_keys: list                    # [(clip, frame | 'beat', facing)]
    contact_keys: list
    facing_keys: list
    close_keys: list
    back_clips: tuple
    portrait_pose: str = "Portrait"
    portrait_bust_z: float = 1.0
    portrait_zoom: tuple = (150, 406, 40, 296)
    portrait_yaw: float = -110.0
    verify_bones: tuple = ()
    verify_top: tuple = (1.70, 1.80)
    visibility: Callable | None = None  # (clip name, ms) → {item name: visible}
    preview_extra: Callable | None = None   # (rv, rig, meshes, clips, actions, items) → None (hero sheets)
    notes: str = ""
    lod1_screen_size: float = 0.15

    @property
    def portrait_name(self) -> str:
        return f"T_UI_Portrait_{self.token}"


# ── item visibility (frame-change handler) ──────────────────────────────────────────────────────────────
_VIS: dict = {}


def _vis_handler(sc, *_):
    hd, rig = _VIS.get("hd"), _VIS.get("rig")
    if hd is None or hd.visibility is None or rig is None:
        return
    ad = rig.obj.animation_data
    act = ad.action if ad else None
    clip = json.loads(act["af_clip"])["name"] if act is not None and "af_clip" in act else None
    vis = hd.visibility(clip, scene.frame_to_ms(sc.frame_current))
    for name, on in vis.items():
        ob = bpy.data.objects.get(name)
        if ob is not None and ob.hide_render == on:
            ob.hide_render = not on
    hook = _VIS.get("hook")
    if hook is not None:
        hook(clip, scene.frame_to_ms(sc.frame_current))


def install_visibility(hd: HeroDef, rig, hook: Callable | None = None) -> None:
    _VIS.update(hd=hd, rig=rig, hook=hook)
    hs = bpy.app.handlers.frame_change_post
    if _vis_handler not in hs:
        hs.append(_vis_handler)


def eye_glows(hd: HeroDef, rig):
    """``Review.glows``: the runtime eye card at the posed ``eyes`` socket (manifest ``fx.visorGlow`` size / colours),
    faded by how much the face looks at the camera (UE: clamp(2·dot(fwd, toCamera) + 0.3, 0, 1))."""
    vg = hd.eye_glow
    hb = rig.obj.data.bones["head"]

    def fn(cam, res):
        vp = socket_world(rig, hd.eye_socket)
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


def socket_world(rig, name: str) -> Vector:
    s = next(x for x in rig.sockets if x["name"] == name)
    b = rig.obj.data.bones[s["bone"]]
    pb = rig.obj.pose.bones[s["bone"]]
    return rig.obj.matrix_world @ (pb.matrix @ (b.matrix_local.inverted() @ Vector(s["pos"])))


def item_objects(items: dict, hd: HeroDef) -> list:
    return [items[i.name] for i in hd.items if i.name in items]


# ── ship ────────────────────────────────────────────────────────────────────────────────────────────────
def ship(hd: HeroDef, previews: bool = True, verify: bool = True) -> dict:
    t0 = time.time()
    G = hd.module
    built = G.build_all(lod1=True)
    pal, rig, body, items, sockets, body1 = (built[k] for k in ("pal", "rig", "body", "items", "sockets", "body1"))
    print(f"[build] body {M.tri_count(body, 0)} toon + {M.tri_count(body, 1)} hull tris; bones "
          f"{len(rig.obj.data.bones)}; " + "; ".join(f"{n} {M.tri_count(o, 0)}+{M.tri_count(o, 1)}"
                                                      for n, o in items.items()))
    for b_ in (body, body1):
        st = R.weight_stats(b_)
        print(f"[weights] {b_.name}: max {st['maxInfluences']} influences, {st['unweighted']} unweighted")
        assert st["unweighted"] == 0 and st["maxInfluences"] <= 4
    print(f"[build] LOD1 {M.tri_count(body1, 0)} toon + {M.tri_count(body1, 1)} hull tris")
    # items (exported unattached, grip / attach point at the origin)
    shipped = {}
    for it in hd.items:
        obj = items[it.name]
        extra = {"heldBy": [hd.asset], "attachKind": it.kind, **it.extra}
        if it.parent:
            extra["parentItem"] = it.parent
        shipped[it.name] = C.ship_weapon(obj, it.name, pal, sockets.get(it.name, []), it.socket, it.weapon_type,
                                         extra=extra)
    # clips
    A = hd.anims(rig, G.D)
    clips = A.all_clips()
    actions = []
    for c in clips:
        t1 = time.time()
        actions.append(anim.bake(rig, c, hd.token))
        print(f"[bake] {c.name} {c.frames}f {time.time() - t1:.1f}s")
    sk = export.export_skeletal_mesh(rig, [body], hd.asset, "Characters")
    sk1 = export.export_skeletal_mesh(rig, [body1], f"{hd.asset}_LOD1", "Characters")
    body1.hide_render = True
    entries = []
    for c, a in zip(clips, actions):
        fbx = export.export_animation(rig, a, "Characters")
        e = export.clip_entry(c, a, fbx)
        if c.name == "HurtAdd":
            e["additiveBase"] = {"type": "LocalAnimFrame", "anim": a.name, "frame": 0,
                                 "note": "Local-space additive; base = its own frame 0 (the READY pose)"}
        if c.name == "Portrait":
            e["staticPose"] = True
        hd.clip_meta(c, e)
        entries.append(e)
    man = export.Manifest()
    man.set_palette(pal)
    entry = export.skeletal_entry(rig, body, sk, "Characters", pal, "hero", entries, [hd.game_id],
                                  blob_radius_cm=round(hd.blob_radius * 100.0, 1),
                                  extra={"displayName": hd.display_name, "fx": hd.fx, "previews": [],
                                         "notes": hd.notes})
    # character attachments = the hand items the UE actor puts on weapon_r / weapon_l (AAbyssCharacterActor reads
    # only these); child items (the mage crystal on the staff), body items (the rogue's quiver) and clip-only
    # items (the flash-bow) are listed separately so the actor's main / off-hand logic is not confused
    entry["attachments"] = [{"object": i.name, "socket": i.socket} for i in hd.items if i.kind == "hand"]
    others = [{"object": i.name, "socket": i.socket, "kind": i.kind, **({"parentItem": i.parent} if i.parent else {}),
               **({k: v for k, v in i.extra.items() if k in ("showNotify", "hideNotify", "replaces", "clips")})}
              for i in hd.items if i.kind != "hand"]
    if others:
        entry["itemAttachments"] = others
    entry["lods"] = [{
        "index": 1, "asset": f"{hd.asset}_LOD1", "fbx": paths.rel_to_export(sk1), "screenSize": hd.lod1_screen_size,
        "screenSizeNote": "LOD0 is the gameplay mesh at every W1 zoom (screen size 0.2-0.28); LOD1 only below 0.15 "
                          "(far zoom-out, small UI / cutscene shots)",
        "triangles": {"toon": M.tri_count(body1, 0), "outline": M.tri_count(body1, 1)},
        "import": f"same SKEL_Human armature and bind pose; import as LOD index 1 of {hd.asset} "
                  "(USkeletalMeshEditorSubsystem::ImportLOD / Interchange LOD import)"}]
    entry["budget"] = budget_record(body, body1, item_objects(items, hd))
    entry["materialSlots"][1]["outlineMix"] = 0.4
    man.set_asset(hd.asset, entry)
    man.save()
    print(f"[export] {sk.name} + {len(entries)} clips + {len(shipped)} items ({time.time() - t0:.1f}s)")
    result = {"sk": str(sk), "lod1": str(sk1), "anims": [e["fbx"] for e in entries],
              "items": [v["fbx"] for v in shipped.values()], "manifest": str(man.path)}
    G.attach_items(rig, items)
    install_visibility(hd, rig, getattr(A, "preview_hook", None))
    rep = qa_run(hd, rig, body, items, clips, actions)
    print("\n[qa] per-clip worst values (ground cm; overlaps = intersecting triangle pairs)")
    print(HB.PairQA.table(rep))
    fails = HB.PairQA.verdict(rep, hd.qa_limits, hd.qa_clip_limits) + head_verdict(hd, rep)
    for f in fails:
        print("  QA FAIL " + f)
    result["qa"] = {"pass": not fails, "failures": fails, "worst": {k: v["worst"] for k, v in rep.items()}}
    if verify:
        src = record(rig, clips, actions)
    if previews:
        out = paths.preview_root() / hd.preview_dir
        result["previews"], result["ink"] = render_previews(hd, out, pal, rig, body, items, A, clips, actions, body1)
        man = export.Manifest()
        man.data["assets"][hd.asset]["previews"] = [f"{hd.preview_dir}/{p.name}" for p in sorted(out.glob("*.png"))]
        for it in hd.items:
            man.data["assets"][it.name]["previews"] = [f"{hd.preview_dir}/items.png"]
        man.data["assets"][hd.asset]["portrait"] = f"Portraits/{hd.portrait_name}.png"
        ink = result["ink"]
        man.data["assets"][hd.asset]["inkGate"] = {
            k: ink[k] for k in ("pass", "cells", "minFracRim", "minFracDark", "medianRimPx", "maxHullLumP95", "gate")}
        man.data["assets"][hd.asset]["qa"] = {"pass": result["qa"]["pass"], "failures": result["qa"]["failures"]}
        man.save()
    if verify:
        result["verify"] = verify_export(hd, Path(sk), clips, src, entry, Path(sk1))
    print(f"[ship] done in {time.time() - t0:.1f}s")
    return result


def budget_record(body, body1, objs) -> dict:
    t = M.tri_count
    items = {"toon": sum(t(o, 0) for o in objs), "outline": sum(t(o, 1) for o in objs)}
    return {
        "spec": "art-inventory-ch1 §1.9 hero: LOD0 14-18k, LOD1 7k triangles, <= 75 bones per section",
        "lod0": {"toon": t(body, 0), "outline": t(body, 1)},
        "lod1": {"toon": t(body1, 0), "outline": t(body1, 1)},
        "items": items,
        "drawnLod0": t(body, 0) + t(body, 1) + items["toon"] + items["outline"],
        "decision": "The §1.9 figures budget the toon surface; the baked outline hull (P7) is a second, cheap draw "
                    "(same skinning, unlit one-colour pixel shader) budgeted at <= 0.5x the toon triangles via "
                    "low-poly hull proxies and no-hull trims.",
    }


# ── QA ──────────────────────────────────────────────────────────────────────────────────────────────────
def qa_run(hd: HeroDef, rig, body, items, clips, actions) -> dict:
    names = json.loads(body["af_parts"])
    q = HB.PairQA(rig, body, {i.name: items[i.name] for i in hd.items}, names, hd.qa_groups, hd.qa_pairs,
                  grips=hd.item_grips)
    q.debug = "--qa-parts" in __import__("sys").argv
    rep = q.run(clips, actions)
    # face direction on the contact / release frames (the eyes / visor stay on the strike line)
    acts = dict(zip([c.name for c in clips], actions))
    saved = rig.obj.rotation_euler.copy()
    rig.obj.rotation_euler = (0, 0, 0)
    for c in clips:
        if c.name not in hd.qa_head.get("clips", ()):
            continue
        anim.assign(rig, acts[c.name])
        for d in c.notify_list():
            if d["name"] not in ("Contact", "Release"):
                continue
            fr = scene.ms_to_frame(d["ms"])
            anim.set_frame(fr)
            h = head_dir(rig)
            for f in rep[c.name]["frames"]:
                if f["frame"] == fr:
                    f.update(h)
    rig.obj.rotation_euler = saved
    anim.set_frame(0)
    return rep


def head_dir(rig) -> dict:
    pb = rig.obj.pose.bones["head"]
    b = rig.obj.data.bones["head"]
    n = (rig.obj.matrix_world.to_3x3() @ pb.matrix.to_3x3() @ b.matrix_local.to_3x3().inverted()
         @ Vector((0, -1, 0))).normalized()
    return {"headYaw": math.degrees(math.atan2(n.x, -n.y)), "headPitch": math.degrees(math.asin(max(-1, min(1, n.z))))}


def head_verdict(hd: HeroDef, rep: dict) -> list[str]:
    out = []
    for clip in hd.qa_head.get("clips", ()):
        for f in rep.get(clip, {}).get("frames", []):
            if "headYaw" not in f:
                continue
            if abs(f["headYaw"]) > hd.qa_head["maxYaw"] or f["headPitch"] < -hd.qa_head["maxDown"]:
                out.append(f"{clip} F{f['frame']}: face {f['headYaw']:+.0f}° off the strike line, "
                           f"{f['headPitch']:+.0f}° pitch (limits ±{hd.qa_head['maxYaw']:.0f}°, "
                           f"≥ −{hd.qa_head['maxDown']:.0f}°)")
    return out


# ── previews ────────────────────────────────────────────────────────────────────────────────────────────
def _frame_of(c, fr) -> int:
    if fr == "beat":
        for d in c.notify_list():
            if d["name"] in ("Contact", "Release"):
                return scene.ms_to_frame(d["ms"])
        return c.frames // 2
    return int(fr)


def make_review(hd: HeroDef, rig, meshes, out: Path) -> review.Review:
    return review.Review(hd.asset, rig.obj, meshes, hd.module.HEIGHT, blob_radius=hd.blob_radius, out_dir=out,
                         game_outline_px=outline.SCREEN_PX_1080["hero"], glows=eye_glows(hd, rig))


def render_previews(hd: HeroDef, out: Path, pal, rig, body, items, A, clips, actions, body1=None):
    out.mkdir(parents=True, exist_ok=True)
    for p in out.glob("*.png"):
        p.unlink()
    meshes = [body] + item_objects(items, hd)
    rv = make_review(hd, rig, meshes, out)
    acts = dict(zip([c.name for c in clips], actions))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)
    rv.game_view()
    rv.closeup()
    rv.turnaround()
    rv.material_ab()
    for c, a in zip(clips, actions):
        n = 2 if c.name == "Portrait" else 8
        rv.contact_sheet(rig, [(c, a)], frames=n)
        (out / "anims.png").rename(out / f"anim_{c.name}.png")
    game_sheet(rv, rig, clips, actions, hd.game_keys, "game_poses.png", hd)
    game_sheet(rv, rig, clips, actions, hd.contact_keys, "game_contacts.png", hd)
    game_sheet(rv, rig, clips, actions, hd.facing_keys, "game_facings.png", hd)
    closeup_poses(rv, rig, meshes, clips, actions, hd.close_keys)
    back_sheets(rv, rig, meshes, clips, actions, hd.back_clips)
    render_portrait(hd, rv, rig, meshes, clips, actions)
    eye_glow_check(hd, rv, rig, clips, actions)
    if body1 is not None:
        lod_sheet(hd, rv, rig, body, body1, items, clips, actions)
    items_sheet(hd, rv, items)
    if hd.preview_extra is not None:
        hd.preview_extra(rv, rig, meshes, clips, actions, items)
    rv.facing("se")
    for p, size in rv.written:
        print(f"  {p.name:28s} {size // 1024:4d} KB")
    ink = review.ink_verdict(rv.ink_reports)
    print(f"\n[ink] {ink['cells']} gated 1080p cells: min rim≥3px {ink['minFracRim'] * 100:.1f} %, "
          f"min dark {ink['minFracDark'] * 100:.1f} %, median rim {ink['medianRimPx']} px; baked hull only: "
          + ", ".join(f"{b['tag']} median {b['rimPxMedian']} px" for b in ink["baked"]))
    for f in ink["failures"]:
        print("  INK FAIL " + f)
    return [str(p) for p, _ in rv.written], ink


def game_sheet(rv, rig, clips, actions, keys, fname, hd: HeroDef, cols: int = 4) -> None:
    """Native 1080p crops at the W1 camera with the shipped look (UE ink width, bloom, eye glow card); every cell's
    ink rim is measured (``rv.ink_reports``, the ship's ink gate) and printed under its label."""
    acts = dict(zip([c.name for c in clips], actions))
    cl = {c.name: c for c in clips}
    cells = []
    review.setup_render((1920, 1080), 8)
    for name, fr, fac in keys:
        c = cl[name]
        fr = _frame_of(c, fr)
        rv.facing(fac)
        anim.assign(rig, acts[name])
        anim.set_frame(fr)
        off = hd.fx.get("visorGlow", {}).get("offAtDeathFraction", 1.0)
        rv.glow_scale = 0.0 if name == "Death" and fr >= off * c.frames else 1.0
        cam = review.game_camera()
        rv.game_outline(1080, Vector((0, 0, 0.75)))
        cc = review.project(cam, Vector((0, 0, 0.75)))
        hw, hh = 0.075, 0.075 * 1920 / 1080
        img, st = rv.game_shot((cc[0] - hw, cc[1] - hh, cc[0] + hw, cc[1] + hh), measure=True,
                               tag=f"{fname} {name} F{fr} {fac}")
        review.draw_text(img, 3, 3, f"{name} F{fr} {fac}")
        review.draw_text(img, 3, img.shape[0] - 10, rv.ink_label(st), rgb=(255, 220, 140))
        cells.append(img)
    rv.glow_scale = 1.0
    rv._save(fname, review.grid(cells, cols))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)


def closeup_poses(rv, rig, meshes, clips, actions, keys) -> None:
    acts = dict(zip([c.name for c in clips], actions))
    cl = {c.name: c for c in clips}
    cells = []
    res = (256, 320)
    for name, fr, fac in keys:
        c = cl[name]
        fr = _frame_of(c, fr)
        rv.facing(fac)
        anim.assign(rig, acts[name])
        anim.set_frame(fr)
        review.setup_render(res, 8)
        vis = [m for m in meshes if not m.hide_render]
        pts = review.mesh_points(vis)
        review.frame_points(pts, res, fov_h=26, pitch_ue=-35, fill=0.92)
        review.set_outline_px(meshes, 1.8, Vector(pts.mean(0)))
        img = review.render_array()
        mark = ""
        for d in c.notify_list():
            if scene.ms_to_frame(d["ms"]) == fr:
                mark = f" {d['name'].upper()}"
        review.draw_text(img, 3, 3, f"{name} F{fr}{mark}")
        cells.append(img)
    half = (len(cells) + 1) // 2
    rv._save("poses_attack_cast.png", review.grid(cells[:half], half))
    rv._save("poses_signature_hurt.png", review.grid(cells[half:], half))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)


BACK_FACINGS = ("ne", "back", "nw")


def back_sheets(rv, rig, meshes, clips, actions, names, cell=(168, 204)) -> None:
    """Every attack / cast (+ dodge, death) from behind at the game pitch — rows ne / back / nw, columns = the clip's
    key poses (+ the contact / release frame, boxed). ``anim_back_<Clip>.png``."""
    acts = dict(zip([c.name for c in clips], actions))
    for c in clips:
        if c.name not in names:
            continue
        anim.assign(rig, acts[c.name])
        notif = {scene.ms_to_frame(d["ms"]): d for d in c.notify_list() if d["name"] in ("Contact", "Release")}
        keys = getattr(c, "key_poses_ms", None) or [c.length_ms * i / 7 for i in range(8)]
        frames = sorted({min(c.frames, scene.ms_to_frame(ms)) for ms in keys} | set(notif))
        if len(frames) > 9:
            keep = set(notif)
            step = (len(frames) - 1) / 8.0
            frames = sorted({frames[int(round(i * step))] for i in range(9)} | keep)
        pts = []
        for fc in BACK_FACINGS:
            rv.facing(fc)
            for fr in frames:
                anim.set_frame(fr)
                pts.append(review.mesh_points([m for m in meshes if not m.hide_render], step=4))
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


PORTRAIT_INK_PX = 4.5


def render_portrait(hd: HeroDef, rv, rig, meshes, clips, actions) -> Path:
    """Spec §9.5 / R11: 512² RGBA bust portrait, 3/4 front, transparent background, from the Portrait pose →
    ``Art/Export/Portraits/T_UI_Portrait_<Hero>.png`` (+ a preview over the studio colour): toon shading +
    screen-space ink (``review.ink_render``) and the runtime eye glow composited (no bloom on a UI texture)."""
    acts = dict(zip([c.name for c in clips], actions))
    anim.assign(rig, acts[hd.portrait_pose])
    anim.set_frame(0)
    rv.facing(hd.portrait_yaw)
    res = (512, 512)
    review.hide_stage(True)
    review.setup_render(res, 16)
    vis = [m for m in meshes if not m.hide_render]
    pts = review.mesh_points(vis)
    bust = pts[pts[:, 2] > hd.portrait_bust_z]
    cam = review.frame_points(bust, res, fov_h=24, pitch_ue=-9, fill=0.96, include_ground=False)
    for m in meshes:
        outline.remove_preview(m)
    rgba = review.ink_render(vis, res, silhouette_px=PORTRAIT_INK_PX, interior_px=2.0, depth_step=0.018)
    vg = hd.eye_glow
    vp = socket_world(rig, hd.eye_socket)
    cx, cy = review.project_px(cam, vp, res)
    rad = vg["radiusCm"] / 100.0 / outline.pixel_world_size(cam, vp, res[0])
    review.composite_glow(rgba, (cx, cy), rad, vg["color"], 0.55, core=0.30)
    outp = paths.export_root() / "Portraits" / f"{hd.portrait_name}.png"
    outp.parent.mkdir(parents=True, exist_ok=True)
    pngio.write_png(outp, rgba)
    review.hide_stage(False)
    bg = np.array([59, 53, 70], np.float32)
    a = rgba[:, :, 3:4].astype(np.float32) / 255.0
    comp = (rgba[:, :, :3].astype(np.float32) * a + bg * (1 - a) + 0.5).astype(np.uint8)
    review.draw_text(comp, 4, 4, hd.portrait_name)
    rv._save("portrait.png", comp)
    y0, y1, x0, x1 = hd.portrait_zoom
    z = comp[y0:y1, x0:x1].repeat(2, 0).repeat(2, 1)
    review.draw_text(z, 4, 4, "PORTRAIT 2X ZOOM")
    rv._save("portrait_zoom2x.png", z)
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)
    rv.facing("se")
    return outp


def _game_crop(cx: float, cy: float, hw: float = 0.075):
    hh = hw * 1920 / 1080
    return (cx - hw, cy - hh, cx + hw, cy + hh)


def eye_glow_check(hd: HeroDef, rv, rig, clips, actions) -> None:
    """The focal eye glow at native 1080p: bare render vs the shipped post (bloom + runtime eye card)."""
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
        box = _game_crop(cx, cy)
        img = review.render_array(box)
        lit = rv.post(img, cam, (1920, 1080), box)
        for im, tag in ((img, " NO POST"), (lit, " +BLOOM +EYEGLOW")):
            im = im.copy()
            review.draw_text(im, 3, 3, f"{clip} {fac}{tag}")
            cells.append(im)
    rv._save("game_eyeglow.png", review.grid(cells, 4))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)
    rv.facing("se")


def lod_sheet(hd: HeroDef, rv, rig, body, body1, items, clips, actions) -> None:
    acts = dict(zip([c.name for c in clips], actions))
    anim.assign(rig, acts["Idle"])
    anim.set_frame(0)
    rv.facing("se")
    its = [o for o in item_objects(items, hd) if not o.hide_render]
    cells = []
    res = (300, 380)
    for show, hide, tag in ((body, body1, "LOD0"), (body1, body, "LOD1")):
        show.hide_render, hide.hide_render = False, True
        meshes = [show] + its
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
        review.set_outline_px([show] + its, outline.SCREEN_PX_1080["hero"], Vector((0, 0, 0.9)))
        cx, cy = review.project(cam, Vector((0, 0, 0.85)))
        crop = review.render_array(_game_crop(cx, cy, hw=0.07))[:res[1], :res[0]]
        img = np.zeros((res[1], res[0], 3), np.uint8)
        img[:, :] = crop[0, 0]
        y0, x0 = (res[1] - crop.shape[0]) // 2, (res[0] - crop.shape[1]) // 2
        img[y0:y0 + crop.shape[0], x0:x0 + crop.shape[1]] = crop
        review.draw_text(img, 3, 3, f"{tag} 1080P 1:1")
        cells.append(img)
    body.hide_render, body1.hide_render = False, True
    rv._save("lod1.png", review.grid(cells, 4))


def items_sheet(hd: HeroDef, rv, items) -> None:
    """Every item mesh on its own (two views each, studio background) + the hero's ``items_views`` close-ups."""
    objs = item_objects(items, hd)
    cells = []
    res = (300, 360)
    saved = {o.name: (o.parent, o.parent_type, o.parent_bone, o.matrix_parent_inverse.copy(), o.matrix_basis.copy(),
                      o.hide_render) for o in objs}
    hidden = [o for o in bpy.data.objects if o.type == "MESH" and o not in objs and not o.hide_render]
    for o in hidden:
        o.hide_render = True
    review.hide_stage(True)
    views = getattr(hd.module, "ITEM_VIEWS", {})
    for o in objs:
        o.parent = None
        o.matrix_world = Matrix.Identity(4)
    for o in objs:
        for oo in objs:
            oo.hide_render = oo is not o
        for label, mw in views.get(o.name, [("ITEM", Matrix.Identity(4))]):
            o.matrix_world = mw
            bpy.context.view_layer.update()
            review.setup_render(res, 8, bg_hex="#3B3546")
            pts = review.mesh_points([o])
            review.frame_points(pts, res, fov_h=24, pitch_ue=-12, fill=0.88, include_ground=False)
            review.set_outline_px([o], 1.6, Vector(pts.mean(0)))
            img = review.render_array()
            review.draw_text(img, 3, 3, label)
            cells.append(img)
    rv._save("items.png", review.grid(cells, min(5, len(cells))))
    for o in hidden:
        o.hide_render = False
    review.hide_stage(False)
    for o in objs:
        par, ptype, pbone, mpi, mb, hr = saved[o.name]
        o.parent, o.parent_type, o.parent_bone = par, ptype, pbone
        o.matrix_parent_inverse = mpi
        o.matrix_basis = mb
        o.hide_render = hr


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


def verify_export(hd: HeroDef, sk_fbx: Path, clips, src: dict, entry: dict, lod_fbx: Path | None = None) -> dict:
    fails = []

    def check(ok, msg):
        print(("  ok   " if ok else "  FAIL ") + msg)
        if not ok:
            fails.append(msg)
    print("\n[verify] re-import into an empty scene")
    if _vis_handler in bpy.app.handlers.frame_change_post:
        bpy.app.handlers.frame_change_post.remove(_vis_handler)
    scene.reset()
    bpy.ops.import_scene.fbx(filepath=str(sk_fbx))
    arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    check(len(arms) == 1 and len(meshes) == 1, f"one armature + one mesh ({len(arms)}, {len(meshes)})")
    arm, me = arms[0], meshes[0]
    bones = {b.name for b in arm.data.bones}
    check(len(bones) == entry["bones"], f"{len(bones)} bones == {entry['bones']}")
    for need in ("root", "pelvis", "head", "weapon_r", "weapon_l", "ik_foot_l", "ik_hand_gun") + tuple(hd.verify_bones):
        check(need in bones, f"bone {need}")
    check("Armature" not in bones, "no extra root bone")
    mats = [m.name for m in me.data.materials]
    check(mats == ["MI_AF_Toon_Heroes", "M_AF_Outline"], f"material slots {mats}")
    st = R.weight_stats(me)
    check(st["unweighted"] == 0 and st["maxInfluences"] <= 4, f"weights ({st['maxInfluences']} max, "
                                                                f"{st['unweighted']} unweighted)")
    lo, hi = scene.world_bounds([me])
    check(hd.verify_top[0] < hi.z < hd.verify_top[1] and -0.025 < lo.z < 0.0,
          f"bind-pose bounds z {lo.z:.3f} .. {hi.z:.3f} m incl. hull")
    check("AF_Data" in me.data.color_attributes and me.data.has_custom_normals, "AF_Data colours + custom normals")
    worst_mm, worst_deg = 0.0, 0.0
    for c in clips:
        f = paths.category_dir("Characters") / f"A_{hd.token}_{c.name}.fbx"
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
    for it in hd.items:
        objs = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(paths.category_dir("Weapons") / f"{it.name}.fbx"))
        new = [o for o in bpy.data.objects if o not in objs]
        names = sorted(o.name for o in new)
        mesh = next(o for o in new if o.type == "MESH")
        mn = [m.name.split(".")[0] for m in mesh.data.materials]
        check(mn == ["MI_AF_Toon_Heroes", "M_AF_Outline"], f"{it.name}: material slots {mn}")
        socks = [x for x in names if x.startswith("SOCKET_")]
        check(bool(socks), f"{it.name}: sockets {socks}")
    print(f"[verify] {'PASS' if not fails else 'FAIL'} ({len(fails)} failures)")
    return {"pass": not fails, "failures": fails}


# ── dev tooling ─────────────────────────────────────────────────────────────────────────────────────────
def _arg(argv, key, default=None):
    return next((a.split("=", 1)[1] for a in argv if a.startswith(f"--{key}=")), default)


def quick(hd: HeroDef, out: Path, facing: str = "se", pose: str = "ready") -> None:
    t0 = time.time()
    G = hd.module
    built = G.build_all()
    rig, body, items = built["rig"], built["body"], built["items"]
    print(f"[build] body {M.tri_count(body, 0)} + hull {M.tri_count(body, 1)} tris; "
          + "; ".join(f"{n} {M.tri_count(o, 0)}" for n, o in items.items())
          + f"; bones {len(rig.obj.data.bones)}; {time.time() - t0:.1f}s")
    print("[weights]", {k: v for k, v in R.weight_stats(body).items() if k != "groups"})
    print("[parts]", G.part_report(G.LAST_PARTS))
    A = hd.anims(rig, G.D)
    bp = A.ready() if pose == "ready" else getattr(A, pose)()
    anim.apply_pose(A.solve(bp))
    G.attach_items(rig, items)
    meshes = [body] + item_objects(items, hd)
    vis = hd.visibility(None, 0.0) if hd.visibility else {}
    for n, on in vis.items():
        if n in items:
            items[n].hide_render = not on
    out.mkdir(parents=True, exist_ok=True)
    rv = make_review(hd, rig, meshes, out)
    rv.closeup(facing)
    rv.turnaround()
    rv.game_view()
    print(f"[quick] {time.time() - t0:.1f}s -> {out}")


def anim_preview(hd: HeroDef, out: Path, names: list, facing: str = "se", frames: int = 8) -> None:
    t0 = time.time()
    G = hd.module
    built = G.build_all()
    rig, body, items = built["rig"], built["body"], built["items"]
    A = hd.anims(rig, G.D)
    clips = [c for c in A.all_clips() if not names or c.name in names]
    acts = []
    for c in clips:
        t1 = time.time()
        acts.append(anim.bake(rig, c, hd.token))
        print(f"[bake] {c.name} {c.frames}f {time.time() - t1:.1f}s")
    G.attach_items(rig, items)
    install_visibility(hd, rig, getattr(A, "preview_hook", None))
    out.mkdir(parents=True, exist_ok=True)
    rv = make_review(hd, rig, [body] + item_objects(items, hd), out)
    for c, a in zip(clips, acts):
        rv.contact_sheet(rig, [(c, a)], frames=frames, facing=facing)
        (out / "anims.png").rename(out / f"anim_{c.name}_{facing}.png")
    if "--qa" in __import__("sys").argv:
        rep = qa_run(hd, rig, body, items, clips, acts)
        print(HB.PairQA.table(rep))
        if "--qa-parts" in __import__("sys").argv:
            for cn, r in rep.items():
                agg = {}
                for f in r["frames"]:
                    for m, hit in f.get("parts", {}).items():
                        for n, v in hit.items():
                            agg.setdefault(m, {}).setdefault(n, 0)
                            agg[m][n] += v
                print(f"  parts {cn}: " + "; ".join(f"{m}: " + ", ".join(f"{n} {v}" for n, v in
                                                     sorted(h.items(), key=lambda kv: -kv[1])[:6])
                                                     for m, h in agg.items()))
        for f in HB.PairQA.verdict(rep, hd.qa_limits, hd.qa_clip_limits) + head_verdict(hd, rep):
            print("  QA FAIL " + f)
    print(f"[anims] {time.time() - t0:.1f}s -> {out}")


def frame_preview(hd: HeroDef, out: Path, specs: list, facings=("se", "side", "back"), res=(300, 360)) -> None:
    G = hd.module
    built = G.build_all()
    rig, body, items = built["rig"], built["body"], built["items"]
    A = hd.anims(rig, G.D)
    want = {s.split(":")[0] for s in specs}
    clips = {c.name: c for c in A.all_clips() if c.name in want}
    G.attach_items(rig, items)
    install_visibility(hd, rig, getattr(A, "preview_hook", None))
    meshes = [body] + item_objects(items, hd)
    out.mkdir(parents=True, exist_ok=True)
    rv = make_review(hd, rig, meshes, out)
    cells = []
    acts = {}
    for spec in specs:
        name, fr = spec.split(":")[:2]
        if name not in acts:
            acts[name] = anim.bake(rig, clips[name], hd.token)
        anim.assign(rig, acts[name])
        anim.set_frame(int(fr))
        for fc in facings:
            name, _, pitch = fc.partition("@")
            rv.facing(name)
            review.setup_render(res, 8)
            pts = review.mesh_points([m for m in meshes if not m.hide_render])
            review.frame_points(pts, res, fov_h=26, pitch_ue=float(pitch) if pitch else -30.0, fill=0.92)
            review.set_outline_px(meshes, 2.0, Vector(pts.mean(0)))
            img = review.render_array()
            review.draw_text(img, 4, 4, f"{name} F{fr} {fc}")
            cells.append(img)
    pngio.write_png(out / "frames.png", review.grid(cells, len(facings)))
    print(f"[frames] -> {out / 'frames.png'}")


def game_preview(hd: HeroDef, out: Path, specs: list) -> None:
    """Native 1080p game crops of ``Clip:frame:facing`` (ink-gated, labelled) → ``game.png`` in ``out``."""
    G = hd.module
    built = G.build_all()
    rig, body, items = built["rig"], built["body"], built["items"]
    A = hd.anims(rig, G.D)
    keys = []
    for s in specs:
        p = s.split(":")
        keys.append((p[0], p[1] if p[1] == "beat" else int(p[1]), p[2] if len(p) > 2 else "se"))
    want = {k[0] for k in keys} | {"Idle"}
    cl = [c for c in A.all_clips() if c.name in want]
    acts = [anim.bake(rig, c, hd.token) for c in cl]
    G.attach_items(rig, items)
    install_visibility(hd, rig, getattr(A, "preview_hook", None))
    out.mkdir(parents=True, exist_ok=True)
    rv = make_review(hd, rig, [body] + item_objects(items, hd), out)
    game_sheet(rv, rig, cl, acts, keys, "game.png", hd)
    for r in rv.ink_reports:
        print(f"  {r['tag']}: rim {r['fracRim'] * 100:.1f} % dark {r.get('fracDark', 0) * 100:.1f} % "
              f"median {r['rimPxMedian']} px -> {'ok' if review.ink_gate_ok(r) else 'FAIL'}")


def cli(hd: HeroDef, argv: list, scratch: Path) -> int:
    if "--quick" in argv:
        quick(hd, scratch / "quick", _arg(argv, "facing", "se"), _arg(argv, "pose", "ready"))
        return 0
    if "--anims" in argv:
        i = argv.index("--anims")
        rest = [a for a in argv[i + 1:] if not a.startswith("--")]
        anim_preview(hd, scratch / "anims", rest, facing=_arg(argv, "facing", "se"))
        return 0
    if "--frames" in argv:
        i = argv.index("--frames")
        fac = tuple((_arg(argv, "facings") or "se,side,back").split(","))
        frame_preview(hd, scratch / "frames", [a for a in argv[i + 1:] if ":" in a and not a.startswith("--")], fac)
        return 0
    if "--game" in argv:
        i = argv.index("--game")
        game_preview(hd, scratch / "game", [a for a in argv[i + 1:] if ":" in a and not a.startswith("--")])
        return 0
    res = ship(hd, previews="--no-previews" not in argv, verify="--no-verify" not in argv)
    out = {k: v for k, v in res.items() if k not in ("previews", "qa", "ink")}
    if res.get("ink"):
        out["ink"] = {k: v for k, v in res["ink"].items() if k not in ("baked",)}
    out["qa"] = {k: v for k, v in res["qa"].items() if k != "worst"}
    print(json.dumps(out, indent=1)[:4000])
    ok = res.get("verify", {"pass": True})["pass"] and res["qa"]["pass"]
    ink = res.get("ink")
    if ink is not None and not ink["pass"]:
        print("\nINK GATE FAILED (review.INK_GATE, see game_inkgate.png):\n  " + "\n  ".join(ink["failures"]))
        ok = False
    return 0 if ok else 1

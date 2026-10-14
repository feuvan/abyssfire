"""Chapter-1 coherence lineups at the W1 game camera — ``Art/Previews/ch1_coherence/``.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/coherence/lineup.py
        [--characters] [--props]          (default: both)

Every asset is re-imported from ``Art/Export`` (``cohkit``) and drawn side by side on the shipped plains terrain
(tile grid, web contact shadows) at the game camera (yaw 45°, pitch −50°, FOV 35°, default distance), native
1080p pixels, each mesh with its shipped screen ink and the emissive bloom:

* ``lineup_characters*.png`` — front row: the 3 heroes (Idle F0, held items, rogue quiver, mage crystal), the
  ley-sprite companion (DECISIONS Q3), the goblin chief (boss ink), the shaman (mini-boss), the goblin, both
  quest-hunt leaders (``variants``: ×1.25, attachments, elite crown on ``fx_overhead``) and the slime; back row:
  every Chapter-1 NPC look (IdleNPC F0), the blacksmith with his anvil stump, the ley sprite's two evolved looks. ``lineup_characters_inkgate*.png``
  marks every silhouette edge pixel (green = rim ≥ class gate, red = thinner).
* ``lineup_props_<group>*.png`` (interactive, ground loot, camp kit + walls, story, goblin camp, foliage + rocks) — each
  row with the warrior (and a goblin) for scale; ``…_inkgate*.png``.

Measured per figure / prop on its own coverage (``cohkit.ink_gate_group``): the ink gate of its class, the median
rim, the CIELAB chroma C*ab (mean, 75th percentile; the HSV saturation is kept for reference) and value spread of its
toon pixels, the lit / shade split (upper-left vs lower-right half of the silhouette: the key light), the posed top of
every character vs its exported rest top (FBX scale) and its manifest ``heightCm``. Family aggregates and the
cross-family rules (spec §1.3 ink per class, §1.5 saturation hierarchy by chroma, key-light side, R2 scale) go to
``coherence/lineup_metrics.json`` (read by ``coherence/checklist.py``).
Exit 1 when an ink gate or a rule fails.
"""
from __future__ import annotations

import json
import math
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import cohkit as K  # noqa: E402
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector  # noqa: E402

from kit import outline, paths, pngio, review, scene  # noqa: E402

OUT = paths.preview_root() / "ch1_coherence"
METRICS = HERE / "lineup_metrics.json"
RES = (1920, 1080)

FRONT = [  # (asset, label, variant)
    ("SK_Hero_Warrior", "WARRIOR", None), ("SK_Hero_Mage", "MAGE", None), ("SK_Hero_Rogue", "ROGUE", None),
    ("SK_Pet_Sprite", "LEY SPRITE", None),
    ("SK_Mon_GoblinChief", "CHIEF", None), ("SK_Mon_GoblinShaman", "SHAMAN", None), ("SK_Mon_Goblin", "GOBLIN", None),
    ("SK_Mon_Goblin", "REDCAP GRUK", "hunt_redcap_gruk"), ("SK_Mon_Goblin", "SNEEK", "hunt_pendant_thief"),
    ("SK_Mon_Slime", "SLIME", None),
]
BACK = [
    ("SK_NPC_Blacksmith", "BLACKSMITH"), ("SK_NPC_Merchant", "MERCHANT"), ("SK_NPC_QuestElder", "ELDER"),
    ("SK_NPC_StashKeeper", "STASH"), ("SK_NPC_PlainsHerbalist", "HERBALIST"), ("SK_NPC_PlainsWanderer", "WANDERER"),
    ("SK_NPC_EscortTravelingMerchant", "ESCORT"), ("SK_NPC_RescueLostTraveler", "LOST TRAVELER"),
    ("SK_NPC_WanderingMerchant", "WANDERING M."),
    ("SK_Pet_Sprite_E1", "SPRITE E1"), ("SK_Pet_Sprite_E2", "SPRITE E2"),      # the companion's evolved looks
]
FAMILY_OF = {"hero": "heroes", "boss": "monsters", "monster": "monsters", "npc": "npcs"}

PROP_GROUPS = {   # name → (spacing m, [(asset, label)])
    "interactive": (1.0, [
        ("SK_Prop_TreasureChest", "CHEST"), ("SM_Prop_GoldPile", "GOLD PILE"), ("SM_Prop_Event_RunePillar", "PILLAR"),
        ("SM_Prop_Lore_AncientTablet", "TABLET"), ("SM_Prop_Lore_TornJournal", "JOURNAL"),
        ("SM_Prop_Lore_CrystalShard", "CRYSTAL"), ("SM_Prop_Lore_CarvedStone", "CARVED"),
        ("SM_Prop_Gather_EmeraldHerb", "HERB"), ("SM_Loot_Bag", "BAG"), ("SM_Pickup_PotionHP", "POTION"),
        ("SM_Pickup_Gold_M", "GOLD"), ("SM_Pickup_Quest_Pendant", "PENDANT"), ("SM_Prop_Camp_Hearthstone", "HEARTH")]),
    "camp": (1.0, [
        ("SM_Prop_Camp_Tent", "TENT"), ("SM_Prop_Camp_Campfire", "CAMPFIRE"), ("SM_Prop_Camp_Well", "WELL"),
        ("SM_Prop_Camp_Banner", "BANNER"), ("SM_Prop_Camp_Torch", "TORCH"), ("SM_Prop_Camp_Barrel", "BARREL"),
        ("SM_Prop_Camp_Crate", "CRATE"), ("SM_Prop_Camp_AnvilStump", "ANVIL"), ("SM_Palisade_Plains_Stake_A", "STAKE"),
        ("SM_Palisade_Plains_GatePost", "GATEPOST")]),
    "story": (1.0, [
        ("SM_Prop_Story_ElvenRuins", "RUINS"), ("SM_Prop_Story_GoblinTotem", "WAR TOTEM"),
        ("SM_Prop_Story_BrokenAltar", "ALTAR"), ("SM_Prop_Story_DryWell", "DRY WELL"),
        ("SM_Prop_ExitGate_Sealed", "SEALED GATE")]),
    "goblin": (1.0, [
        ("SM_Prop_Goblin_Hut_A", "HUT A"), ("SM_Prop_Goblin_Hut_B", "HUT B"), ("SM_Prop_Goblin_FirePit", "FIRE PIT"),
        ("SM_Prop_Goblin_BoneTotem", "BONE TOTEM"), ("SM_Prop_Goblin_SpearRack", "SPEARS"),
        ("SM_Prop_Goblin_Crates", "CRATES")]),
    "loot": (0.6, [
        ("SM_Loot_Bag", "BAG"), ("SM_Pickup_Gold_S", "GOLD S"), ("SM_Pickup_Gold_M", "M"), ("SM_Pickup_Gold_L", "L"),
        ("SM_Pickup_PotionHP_S", "HP S"), ("SM_Pickup_PotionHP", "HP"), ("SM_Pickup_PotionHP_L", "HP L"),
        ("SM_Pickup_PotionMP_S", "MP S"), ("SM_Pickup_PotionMP", "MP"), ("SM_Pickup_Antidote", "ANTI"),
        ("SM_Pickup_Scroll_ID", "ID"), ("SM_Pickup_Scroll_TP", "TP"), ("SM_Pickup_Gem_Ruby_1", "RUBY"),
        ("SM_Pickup_Gem_Sapphire_2", "SAPH"), ("SM_Pickup_Gem_Emerald_3", "EMER"), ("SM_Pickup_Gem_Topaz_2", "TOPAZ"),
        ("SM_Pickup_Gem_Diamond_5", "DIAM"), ("SM_Pickup_LeyFruit", "FRUIT"), ("SM_Pickup_Quest_Herb", "HERB"),
        ("SM_Pickup_Quest_Gel", "GEL"), ("SM_Pickup_Quest_Pendant", "PEND")]),
    "foliage": (1.0, [
        ("SM_Foliage_Plains_Oak_A", "OAK"), ("SM_Foliage_Plains_FruitTree_A", "FRUIT TREE"),
        ("SM_Foliage_Plains_BerryBush_A", "BUSH"), ("SM_Foliage_Plains_RedMushrooms_A", "MUSHROOMS"),
        ("SM_Foliage_Plains_FlowerPatch_A", "FLOWERS"), ("SM_Foliage_Plains_GrassTuft_A", "GRASS"),
        ("SM_Rock_Plains_Cluster_A", "ROCKS"), ("SM_Rock_Plains_MossBoulder_A", "BOULDER"),
        ("SM_Outcrop_Plains_V0", "OUTCROP")]),
}


def screen_axes(cam) -> tuple[Vector, Vector]:
    """(screen-right, screen-up) directions on the ground plane for the camera."""
    r = cam.matrix_world.to_3x3() @ Vector((1, 0, 0))
    r.z = 0
    r.normalize()
    f = cam.matrix_world.to_3x3() @ Vector((0, 0, -1))
    u = Vector((f.x, f.y, 0.0)).normalized()
    return r, u


def tiles_window(c0: int, r0: int, c1: int, r1: int, dirt=()):
    m = np.zeros((r1 - r0, c1 - c0), np.int32)
    for (c, r) in dirt:
        if c0 <= c < c1 and r0 <= r < r1:
            m[r - r0, c - c0] = 1
    return m


def stats(st: dict, label: str, cls: str) -> dict:
    body, rgb = st["body"], st["rgb"]
    solid = body > 0.98
    out = {"label": label, "class": cls, "inkOk": bool(st["ok"]), "fracRim": st["fracRim"],
           "rimPxMedian": st["rimPxMedian"], "fracDark": st.get("fracDark", 0.0),
           "hullLumP95": st.get("hullLumP95", 0.0), "minPx": st["gate"]["minPx"], "edges": st["edges"],
           **({"ignoredEmissive": st["ignoredEmissive"]} if st.get("ignoredEmissive") else {})}
    if solid.sum() > 30:
        px = rgb[solid]
        s = K.hsv_sat(px)
        lm = K.lum(px)
        out["satMean"] = round(float(s.mean()), 3)
        out["satP75"] = round(float(np.percentile(s, 75)), 3)
        c = K.lab_chroma(px)
        out["chromaMean"] = round(float(c.mean()), 1)
        out["chromaP75"] = round(float(np.percentile(c, 75)), 1)
        out["lumP10"], out["lumP90"] = round(float(np.percentile(lm, 10)), 1), round(float(np.percentile(lm, 90)), 1)
        ys, xs = np.nonzero(solid)
        cy, cx = ys.mean(), xs.mean()
        ul = ((xs - cx) + (ys - cy)) < 0                  # upper-left half of the silhouette (the key light's side)
        lmap = K.lum(rgb)[ys, xs]
        out["litUL"] = round(float(lmap[ul].mean() - lmap[~ul].mean()), 1) if ul.any() and (~ul).any() else 0.0
    return out


def write(name: str, img: np.ndarray, label: str) -> list:
    def ann(t, i, k):
        review.draw_text(t, 6, 6, label + (f"  TILE {i + 1}/{k}" if k > 1 else ""))
    out = pngio.write_png_native(OUT / name, img, annotate=ann)
    for p, size in out:
        print(f"  {p}  {size // 1024} KB")
    return out


def shoot(groups: list, cam, name: str, label: str, ground_dirt=(), shadows=(), pad=(36, 44)) -> list[dict]:
    """Render the frame round every group, gate each group on its own coverage, label, write the image and its
    ink-gate map. ``groups`` = [(label, meshes, class, record extras)]. Returns the per-group stats."""
    every = [m for _, ms, _, _ in groups for m in ms]
    pts = review.mesh_points(every, step=8)
    c0, c1 = int(pts[:, 0].min()) - 10, int(pts[:, 0].max()) + 11
    r0, r1 = int(-pts[:, 1].max()) - 10, int(-pts[:, 1].min()) + 11
    K.ground(tiles_window(c0, r0, c1, r1, ground_dirt), (c0, r0), ppt=48, shadows=shadows, name=f"g_{name}")
    bpy.context.view_layer.update()
    box = K.projected_box(cam, every, RES, pad_px=pad)
    if box[0] <= 0.0 or box[1] <= 0.0 or box[2] >= 1.0 or box[3] >= 1.0:
        raise RuntimeError(f"{name}: the lineup does not fit the 1080p frame {box} — tighten the layout")
    t0 = time.time()
    img, raw = K.render(every, box)
    print(f"  [{name}] render {time.time() - t0:.1f} s  {img.shape[1]}x{img.shape[0]}")
    ink = img.copy()
    recs = []
    for lab, ms, cls, extra in groups:
        if cls == "none":                                  # ground cover: no hull by design (spec §1.3)
            st = K.ink_gate_group(ms, every, box, img, raw, "decor", cam)
            rec = stats(st, lab, "none")
            rec["inkOk"] = True
            for k in ("fracRim", "rimPxMedian", "fracDark", "hullLumP95", "minPx", "edges"):
                rec.pop(k, None)
        else:
            st = K.ink_gate_group(ms, every, box, img, raw, cls, cam, min_px=extra.get("gatePx"),
                                  emissive=K.LAST_EMISSIVE.get("em"))
            rec = stats(st, lab, cls)
            ys, xs = st["edge_yx"]
            ox, oy = st["offset"]
            ok = st["edge_ok"]
            ink[ys[ok] + oy, xs[ok] + ox] = (60, 230, 90)
            ink[ys[~ok] + oy, xs[~ok] + ox] = (240, 60, 60)
        rec.update(extra)
        recs.append(rec)
        print(f"  [ink] {lab:20s} {rec['class']:11s} rim {rec.get('fracRim', 1) * 100:5.1f}% median "
              f"{rec.get('rimPxMedian', 0):.2f}px dark {rec.get('fracDark', 1) * 100:5.1f}% hull p95 "
              f"{rec.get('hullLumP95', 0):4.0f}  sat {rec.get('satMean', 0):.2f}/{rec.get('satP75', 0):.2f}  "
              f"C* {rec.get('chromaP75', 0):.0f}  lit "
              f"{rec.get('litUL', 0):+.0f}  -> {'PASS' if rec['inkOk'] else 'FAIL'}")
    x0, y0 = review.border_origin(box, RES)
    for (lab, ms, cls, extra), rec in zip(groups, recs):
        if extra.get("nolabel"):
            continue
        anchor = extra.get("anchor")
        p = review.project_px(cam, anchor if anchor is not None else Vector(review.mesh_points(ms, step=8).mean(0)), RES)
        tx, ty = int(p[0] - x0 - len(lab) * 3), int(p[1] - y0 + 12)
        review.draw_text(img, max(2, tx), min(img.shape[0] - 20, max(2, ty)), lab)
        if "fracRim" in rec:
            tag = f"RIM {rec['fracRim'] * 100:.0f}%"
            review.draw_text(img, max(2, int(p[0] - x0 - len(tag) * 3)), min(img.shape[0] - 10, max(12, ty + 9)), tag,
                             rgb=(160, 230, 160) if rec["inkOk"] else (240, 90, 90))
    write(f"{name}.png", img, label)
    write(f"{name}_inkgate.png", ink, "INK GATE (GREEN = RIM >= CLASS GATE, RED = THINNER)")
    for rec in recs:
        rec.pop("anchor", None)
        rec.pop("nolabel", None)
        rec.pop("gatePx", None)
    return recs


def characters(results: dict) -> bool:
    K.reset()
    review.setup_render(RES, 8)
    cam = review.game_camera((0.0, 0.0))
    r, u = screen_axes(cam)
    se = review.FACINGS["se"]
    front = [f for f in FRONT if f[0] in K.manifest()["assets"]]
    figs = []
    for i, (asset, label, var) in enumerate(front):
        at = r * ((i - (len(front) - 1) / 2) * 1.12) - u * 1.55
        figs.append(K.import_character(asset, at, se, variant=var, label=label, cam=cam))
    nfront = len(figs)
    back = [b for b in BACK if b[0] in K.manifest()["assets"]]
    for i, (asset, label) in enumerate(back):
        at = r * ((i - (len(back) - 1) / 2) * 1.12) + u * 1.75
        figs.append(K.import_character(asset, at, se, label=label, scenery=(asset == "SK_NPC_Blacksmith")))
    lane = [(int(round(f.root.location.x)) + dc, int(round(-f.root.location.y)) + dr)
            for f in figs[:nfront] for dc in (-1, 0, 1) for dr in (-1, 0, 1)]
    shadows = [(f.root.location.x, -f.root.location.y,
                K.entry(f.asset).get("blobShadowRadiusCm", 30) / 100.0 * f.root.scale[0], 0.45) for f in figs]
    groups = []
    for f in figs:
        fam = "pets" if f.asset.startswith("SK_Pet_") else FAMILY_OF.get(f.cls, "other")
        e = K.entry(f.asset)
        rest_top = e["boundsCm"]["max"][2] * f.root.scale[0] if e.get("boundsCm") else None
        groups.append((f.label, f.meshes, f.cls, {"asset": f.asset, "family": fam, "heightCm": round(f.height_m * 100, 1),
                                                   "topCm": round(f.top_cm(), 1), "anchor": f.root.location.copy(),
                                                   "px": f.px, "gatePx": K.CHAR_GATE_PX,
                                                   **({"restTopCm": round(rest_top, 1)} if rest_top else {})}))
        for x in f.extra:
            if x.name.startswith("COH_SM_FX_EliteCrown"):
                groups.append((f.label + " CROWN", [x], "monster", {"asset": "SM_FX_EliteCrown", "family": "monsters",
                                                                    "px": float(x.get("af_px1080", 3.0)),
                                                                    "nolabel": True}))
            elif "AnvilStump" in x.name:
                groups.append((f.label + " ANVIL", [x], K.slot_info(K.entry("SM_Prop_Camp_AnvilStump"))[1],
                               {"asset": "SM_Prop_Camp_AnvilStump", "family": "npcs", "nolabel": True,
                                "px": K.slot_info(K.entry("SM_Prop_Camp_AnvilStump"))[2]}))
    recs = shoot(groups, cam, "lineup_characters",
                 "CH1 CHARACTERS  W1 CAMERA DEFAULT DISTANCE  NATIVE 1080P 1:1  SHIPPED FBX + INK + BLOOM",
                 ground_dirt=lane, shadows=shadows)
    results["characters"] = recs
    return all(x["inkOk"] for x in recs)


def props(results: dict, only=None) -> bool:
    ok = True
    results.setdefault("props", [])
    for gname, (sp, items) in PROP_GROUPS.items():
        if only and gname not in only:
            continue
        results["props"] = [p for p in results["props"] if p.get("group") != gname]
        K.reset()
        review.setup_render(RES, 8)
        cam = review.game_camera((0.0, 0.0))
        r, u = screen_axes(cam)
        face = review.FACINGS["front"] + 15.0
        items = [x for x in items if x[0] in K.manifest()["assets"]]
        n = len(items)
        groups, shadows = [], []
        # pack along screen-right by each item's footprint (no two silhouettes overlap on screen: the ink gate's
        # dark / hull-colour checks read the final image)
        half = []
        for asset, _ in items:
            b = K.entry(asset)["boundsCm"]
            ext = max(b["max"][0] - b["min"][0], b["max"][1] - b["min"][1]) / 100.0
            half.append(max(0.18, 0.5 * ext * 0.85))
        xs, x = [], 0.0
        for i, hw in enumerate(half):
            if i:
                x += half[i - 1] + hw + 0.22 * sp
            xs.append(x)
        mid = (xs[0] + xs[-1]) / 2
        xs = [v - mid for v in xs]
        for (asset, label), x in zip(items, xs):
            at = r * x - u * 0.2
            e = K.entry(asset)
            if e["kind"] == "SkeletalMesh":
                f = K.import_character(asset, at, face, label=label)
                ms, root = f.meshes, f.root
            else:
                o = K.import_static(asset)
                o.location = at
                o.rotation_euler = (0, 0, math.radians(face))
                ms, root = [o], o
            fam, cls, px, _ = K.slot_info(e)
            cat = "ground" if cls == "none" else cls
            groups.append((label, ms, cls, {"asset": asset, "family": fam, "cat": cat, "group": gname, "px": px,
                                            **({"gatePx": K.CHAR_GATE_PX} if e["kind"] == "SkeletalMesh"
                                               and cls in K.CHAR_CLASSES else {}),
                                            "anchor": root.location.copy()}))
            if cls != "none":
                rad = e.get("blobShadowRadiusCm")
                if rad is None:
                    b = e["boundsCm"]
                    rad = 0.42 * max(b["max"][0] - b["min"][0], b["max"][1] - b["min"][1])
                shadows.append((root.location.x, -root.location.y, min(rad / 100.0, 1.3), 0.40))
        w = K.import_character("SK_Hero_Warrior", r * (xs[0] - half[0] - 0.75) + u * 0.35, review.FACINGS["se"],
                               label="WARRIOR")
        groups.append(("WARRIOR", w.meshes, w.cls, {"asset": w.asset, "family": "heroes", "group": gname, "px": w.px,
                                                     "gatePx": K.CHAR_GATE_PX, "scaleRef": True,
                                                     "anchor": w.root.location.copy()}))
        shadows.append((w.root.location.x, -w.root.location.y, 0.39, 0.45))
        if gname in ("story", "goblin", "foliage"):
            g = K.import_character("SK_Mon_Goblin", r * (xs[-1] + half[-1] + 0.6) + u * 0.35, review.FACINGS["se"],
                                   label="GOBLIN")
            groups.append(("GOBLIN", g.meshes, g.cls, {"asset": g.asset, "family": "monsters", "group": gname,
                                                       "px": g.px, "gatePx": K.CHAR_GATE_PX, "scaleRef": True,
                                                       "anchor": g.root.location.copy()}))
            shadows.append((g.root.location.x, -g.root.location.y, 0.2, 0.45))
        recs = shoot(groups, cam, f"lineup_props_{gname}",
                     f"CH1 {gname.upper()}  W1 CAMERA DEFAULT DISTANCE  NATIVE 1080P 1:1  SHIPPED FBX + INK + BLOOM",
                     shadows=shadows)
        results["props"] += [x for x in recs if not x.get("scaleRef")]
        ok = ok and all(x["inkOk"] for x in recs)
    return ok


def family_rules(results: dict) -> list:
    """Cross-family aggregates → rules for the checklist."""
    rules = []
    chars = [c for c in results.get("characters", []) if "CROWN" not in c["label"] and "ANVIL" not in c["label"]]
    props_ = results.get("props", [])

    def med(xs):
        xs = [x for x in xs if x is not None]
        return float(np.median(xs)) if xs else 0.0
    # colourfulness of the identity colour regions: 75th percentile of the toon pixels' CIELAB chroma C*ab (steel,
    # skin and bone pull the mean down on every family alike). Not HSV saturation: (max − min) / max rates the camp's
    # dark wood and canvas as saturated as the warrior's crimson (both ≈ .55), which is not what §1.5 means.
    fam_sat = {}
    for fam in ("heroes", "monsters", "npcs", "pets"):
        xs = [c.get("chromaP75") for c in chars if c["family"] == fam]
        if xs:
            fam_sat[fam] = med(xs)
    for cat in ("interactive", "decor", "ground"):
        xs = [p.get("chromaP75") for p in props_ if p.get("cat") == cat]
        if xs:
            fam_sat[cat] = med(xs)
    detail = ", ".join(f"{k} {v:.1f}" for k, v in fam_sat.items())
    for a, b in (("heroes", "npcs"), ("monsters", "npcs"), ("heroes", "decor"), ("interactive", "decor")):
        if a in fam_sat and b in fam_sat:
            rules.append({"rule": "§1.5 saturation hierarchy", "subject": f"{a} ≥ {b}",
                          "ok": fam_sat[a] >= fam_sat[b] - 2.0, "detail": f"median p75 CIELAB chroma C*: {detail}"})
    # measured ink per class: median rim within 0.8 px of the shipped request (the slot's outlinePx1080: the class
    # width, Props_Plains decor 1.4, a documented per-asset request such as the 3.5 px characters)
    every = results.get("characters", []) + props_
    for cls in ("hero", "boss", "monster", "small_monster", "npc", "interactive", "decor"):
        rs = [c for c in every if c.get("class") == cls and "rimPxMedian" in c]
        if not rs:
            continue
        m = med([c["rimPxMedian"] for c in rs])
        want = med([c.get("px") or (1.4 if cls == "decor" else outline.SCREEN_PX_1080[cls]) for c in rs])
        rules.append({"rule": "§1.3 measured ink", "subject": cls, "ok": abs(m - want) <= 0.8,
                      "detail": f"median rim {m:.2f} px over {len(rs)} meshes (shipped request {want:g} px at 1080p, "
                                f"class {1.4 if cls == 'decor' else outline.SCREEN_PX_1080[cls]:g} px)"})
    # Chapter-1 character gate: every character figure (heroes, monsters, NPCs, the companion) keeps a dark rim
    # >= CHAR_GATE_PX on >= 90 % of its silhouette edges at 1080p
    cg = [c for c in results.get("characters", []) if c.get("class") in K.CHAR_CLASSES and "CROWN" not in c["label"]]
    if cg:
        bad = [f"{c['label']} {c.get('fracRim', 0) * 100:.0f} %" for c in cg if not c.get("inkOk")]
        rules.append({"rule": "character ink gate", "subject": f">= {K.CHAR_GATE_PX:g} px", "ok": not bad,
                      "detail": f"{len(cg) - len(bad)}/{len(cg)} figures keep a rim >= {K.CHAR_GATE_PX:g} px on "
                                f">= 90 % of their 1080p silhouette edges (min "
                                f"{min(c.get('fracRim', 0) for c in cg) * 100:.1f} %)" +
                                (f"; below: {', '.join(bad)}" if bad else "")})
    for fam in ("heroes", "monsters", "npcs", "pets"):
        xs = [c.get("litUL") for c in chars if c["family"] == fam and c.get("litUL") is not None]
        if xs:
            rules.append({"rule": "key light side", "subject": fam, "ok": med(xs) > 0,
                          "detail": f"median lum(upper-left) − lum(lower-right) {med(xs):+.1f} over {len(xs)}"})
    xs = [p.get("litUL") for p in props_ if p.get("litUL") is not None]
    if xs:
        rules.append({"rule": "key light side", "subject": "props", "ok": med(xs) > 0,
                      "detail": f"median {med(xs):+.1f} over {len(xs)}"})
    # FBX re-import scale: the posed top (Idle) against the exported rest bounds (× the variant scale) — the R2
    # heights themselves are the checklist's manifest check (heightCm = body height; feathers / horns / sprout stand
    # above it, listed here for information)
    bad, n, above = [], 0, []
    for c in chars:
        ref = c.get("restTopCm")
        if c.get("topCm") and ref:
            n += 1
            k = c["topCm"] / ref
            if not 0.85 <= k <= 1.15:
                bad.append(f"{c['label']} {c['topCm']:.0f}/{ref:.0f} cm")
            if c.get("heightCm") and c["topCm"] > c["heightCm"] * 1.08:
                above.append(f"{c['label']} {c['topCm']:.0f} (body {c['heightCm']:.0f})")
    if n:
        rules.append({"rule": "R2 scale (FBX re-import)", "subject": "characters", "ok": not bad,
                      "detail": f"posed top within 15 % of the exported rest top on {n - len(bad)}/{n}" +
                                (f"; off: {', '.join(bad)}" if bad else "") +
                                (f"; headgear above the R2 body height: {', '.join(above)}" if above else "")})
    gates = [c for c in every if not c.get("inkOk", True)]
    rules.append({"rule": "lineup ink gate", "subject": "all", "ok": not gates,
                  "detail": f"{len(every)} meshes gated at 1080p" +
                            (f"; failed: {', '.join(c['label'] for c in gates)}" if gates else "")})
    return rules


def main(argv: list[str]) -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    want_c = "--props" not in argv or "--characters" in argv
    want_p = "--characters" not in argv or "--props" in argv
    only = [a for a in argv if not a.startswith("--")]
    results = json.loads(METRICS.read_text()).get("results", {}) if METRICS.is_file() else {}
    ok = True
    if want_c:
        ok = characters(results) and ok
    if want_p:
        ok = props(results, only) and ok
    rules = family_rules(results)
    for rr in rules:
        print(f"  [rule] {'OK ' if rr['ok'] else 'GAP'} {rr['rule']:26s} {rr['subject']:22s} {rr['detail']}")
    METRICS.write_text(json.dumps({"results": results, "rules": rules}, indent=1, ensure_ascii=False) + "\n")
    print(f"  {METRICS}")
    return 0 if ok and all(rr["ok"] for rr in rules) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

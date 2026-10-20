"""Chapter-1 coherence lineups at the W1 game camera — ``Art/Previews/ch1_coherence/``.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/coherence/lineup.py
        [--characters] [--props]          (default: both)
        [--march] [group …]               the dark-remake Chapter 1 (焚境 · The Burning March) lineups instead

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

``--march`` (Wave 1 coherence pass, implementation-plan.md §5 "family lineup and coherence sheets"): the same shots
under the presentation theme ``march`` (``cohkit.set_theme``: ``<id>@march`` first, the march terrain set, the march
elite crown) into ``Previews/ch1_coherence_march/`` and ``coherence/lineup_metrics_march.json``:

* ``lineup_march_characters*.png`` — front: the 3 heroes, the Lanternwisp, Lord-Marshal Valdras, the Corpseherd and the
  four Unburied; back: the eleven march NPCs (Hald with his charred anvil stump, Old Bern last in profile so his barrow
  runs clear of the row);
* ``lineup_march_monsters*.png`` — front: the warrior (scale), the Living (zealot, deserter, Dreck, Harke, the
  drummer), the gravegnawer, the breachclot and the cinderbloat; back: the bonegorger, Hungerbell and the Lanternwisp's
  evolved looks (every hunt under ``elite_crown@march`` on ``fx_overhead``);
* ``lineup_march_props_<group>*.png`` — the march camp kit, story decor + lore + the east-road barricade, the three
  set-piece groups (Ashwheat, the Sheepfold and the Breach Rim, Knellwatch), foliage + rocks and the interactive /
  quest props, each with the warrior and a risen footman for scale.

The march metrics add the dark remake's colour rules (DR5, monsters.md §1.2) measured on the rendered emissive pass:
no **enemy emissive in the ember band** (hue 15-35°, sat > .6, value ≥ .35), no **friendly emissive in breach-violet**
(within 30° of #B02FD0 at sat > .5; the Lanternwisp and the heroes stay warm), and the **value hierarchy** (every actor
family's lit p90 above the decor's: the ground and props stay below the actors).
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


# ── the march (dark remake Chapter 1) ──────────────────────────────────────────────────────────────────
OUT_MARCH = paths.preview_root() / "ch1_coherence_march"
METRICS_MARCH = HERE / "lineup_metrics_march.json"
MARCH_PAGES = {   # name → (label, front [(asset, label, variant, facing)], back [...])
    "characters": ("CH1 MARCH CHARACTERS: HEROES, LANTERNWISP, BOSS, MINI-BOSS, THE UNBURIED / THE MARCH NPCS", [
        ("SK_Hero_Warrior", "WARRIOR", None, "se"), ("SK_Hero_Mage", "MAGE", None, "se"),
        ("SK_Hero_Rogue", "ROGUE", None, "se"), ("SK_Pet_Lanternwisp", "LANTERNWISP", None, "se"),
        ("SK_Mon_Valdras", "VALDRAS", None, "se"), ("SK_Mon_Corpseherd", "CORPSEHERD", None, "se"),
        ("SK_Mon_RisenFootman", "FOOTMAN", None, "se"), ("SK_Mon_RisenArbalest", "ARBALESTER", None, "se"),
        ("SK_Mon_RisenShieldbearer", "SHIELDBEARER", None, "se"),
        ("SK_Mon_DeadStandardBearer", "STD-BEARER", None, "se")], [
        ("SK_NPC_QuestElder_March", "CAPTAIN", None, "se"), ("SK_NPC_KnellChaplain", "ALDWIN", None, "se"),
        ("SK_NPC_Blacksmith_March", "HALD", None, "se"), ("SK_NPC_Merchant_March", "VIRA", None, "se"),
        ("SK_NPC_StashKeeper_March", "TAMSIN", None, "se"), ("SK_NPC_PlainsHerbalist_March", "MARL", None, "se"),
        ("SK_NPC_PlainsWanderer_March", "CORRAN", None, "se"), ("SK_NPC_CrowTrader", "MAG", None, "se"),
        ("SK_NPC_RescueLostTraveler_March", "PILGRIM", None, "se"),
        ("SK_NPC_WanderingMerchant_March", "PEDDLER", None, "se"),
        ("SK_NPC_EscortTravelingMerchant_March", "BERN", None, "side")]),
    "monsters": ("CH1 MARCH MONSTERS: THE LIVING, HUNTS (ELITE CROWN@MARCH), GNAWERS, BREACH-BLOOD", [
        ("SK_Hero_Warrior", "WARRIOR", None, "se"), ("SK_Mon_OathZealot", "ZEALOT", None, "se"),
        ("SK_Mon_Deserter", "DESERTER", None, "se"), ("SK_Mon_Deserter", "DRECK", "hunt_dreck", "se"),
        ("SK_Mon_Deserter", "HARKE", "hunt_harke", "se"), ("SK_Mon_DeserterDrummer", "DRUMMER", None, "se"),
        ("SK_Mon_Gravegnawer", "GRAVEGNAWER", None, "se"), ("SK_Mon_Breachclot", "BREACHCLOT", None, "se"),
        ("SK_Mon_Cinderbloat", "CINDERBLOAT", None, "se")], [
        ("SK_Mon_Bonegorger", "BONEGORGER", None, "se"), ("SK_Mon_Bonegorger", "HUNGERBELL", "hunt_hungerbell", "se"),
        ("SK_Pet_Lanternwisp_E1", "WISP E1", None, "se"), ("SK_Pet_Lanternwisp_E2", "WISP E2", None, "se")]),
}
MARCH_PROP_GROUPS = {   # name → (spacing, [(asset, label)]); each row ≤ ~11 m so it fits the 1080p frame
    "camp": (1.0, [
        ("SM_Prop_March_BellFrame", "BELL FRAME"), ("SM_Prop_March_Hearth", "HEARTH"),
        ("SM_Prop_March_HearthDressing", "HEARTH DRESSING"), ("SM_Prop_March_Well", "WELL"),
        ("SM_Prop_March_KnellwatchBanner", "BANNER"), ("SM_Prop_March_Brazier", "BRAZIER")]),
    "camp_kit": (1.0, [
        ("SM_Prop_March_RationSacks", "SACKS"), ("SM_Prop_March_SupplyCrate", "CRATE"),
        ("SM_Prop_March_AnvilStump", "ANVIL"), ("SM_Palisade_March_Pew_A", "PEW A"),
        ("SM_Palisade_March_Pew_B", "PEW B"), ("SM_Palisade_March_Pew_C", "PEW C"),
        ("SM_Palisade_March_GatePost", "GATEPOST")]),
    "story": (1.0, [
        ("SM_Prop_March_BrokenBelfry", "BELFRY"), ("SM_Prop_March_RedBanner", "RED BANNER"),
        ("SM_Prop_March_PenitentsAltar", "ALTAR"), ("SM_Prop_March_OrderPost", "ORDER POST"),
        ("SM_Prop_March_TornJournal", "JOURNAL"), ("SM_Prop_March_SealShard", "SEAL SHARD"),
        ("SM_Prop_March_CatechismStone", "CATECHISM"), ("SM_Prop_March_HandbellPost", "HANDBELLS")]),
    "ashwheat": (1.0, [
        ("SM_Prop_March_AshwheatCottage_A", "COTTAGE A"), ("SM_Prop_March_AshwheatCottage_B", "COTTAGE B"),
        ("SM_Prop_March_BurnedBridge", "BRIDGE")]),
    "sheepfold_east": (1.0, [
        ("SM_Prop_March_SheepfoldWagon_A", "WAGON A"), ("SM_Prop_March_SheepfoldWagon_B", "WAGON B"),
        ("SM_Prop_March_WagonBarricade", "EAST ROAD BARRICADE")]),
    "breach_rim": (1.0, [
        ("SM_Prop_March_BreachDais", "DAIS"), ("SM_Prop_March_RitualCircle", "CIRCLE")]),
    "banners_stakes": (1.0, [
        ("SM_Prop_March_RingBanner_A", "RING BANNER A"), ("SM_Prop_March_RingBanner_B", "RING BANNER B"),
        ("SM_Prop_March_HelmetStake_A", "HELMET STAKE A"), ("SM_Prop_March_HelmetStake_B", "HELMET STAKE B")]),
    "knellwatch": (1.0, [
        ("SM_Prop_March_BoneyardGate", "BONEYARD GATE"), ("SM_Prop_March_NameWall", "NAME WALL"),
        ("SM_Prop_March_PitMouthSealed", "PIT MOUTH")]),
    "foliage": (1.0, [
        ("SM_Foliage_March_DeadOak_A", "DEAD OAK"), ("SM_Foliage_March_CharredOrchard_A", "ORCHARD"),
        ("SM_Foliage_March_Briar_A", "BRIAR"), ("SM_Foliage_March_AshCap_A", "ASH-CAP"),
        ("SM_Foliage_March_AshPoppy_A", "POPPIES"), ("SM_Foliage_March_AshGrass_A", "ASH GRASS"),
        ("SM_Foliage_March_Reeds_A", "REEDS")]),
    "rocks": (1.0, [
        ("SM_Rock_March_BrokenStone_A", "STONE A"), ("SM_Rock_March_BrokenStone_B", "B"),
        ("SM_Rock_March_BrokenStone_C", "C"), ("SM_Rock_March_AshBoulder_A", "BOULDER"),
        ("SM_Outcrop_March_V0", "OUTCROP V0"), ("SM_Outcrop_March_V3", "V3")]),
    "interactive": (0.8, [
        ("SK_Prop_March_Strongbox", "STRONGBOX"), ("SM_Prop_Gather_Barrowmoss", "BARROWMOSS"),
        ("SM_Pickup_Quest_Ichor", "ICHOR"), ("SM_Pickup_Quest_Moss", "MOSS"), ("SM_Pickup_Quest_Tags", "TAGS"),
        ("SM_Loot_Bag", "BAG"), ("SM_Pickup_Gold_M", "GOLD"), ("SM_Pickup_PotionHP", "POTION"),
        ("SM_Prop_GoldPile", "GOLD PILE"), ("SM_Prop_Camp_Hearthstone", "HEARTHSTONE"),
        ("SM_FX_EliteCrown_March", "CROWN"), ("SM_FX_EliteSigil", "SIGIL")]),
}
SCALE_REF_MARCH = "SK_Mon_RisenFootman"
BREACH_SOURCED = ("BreachDais", "RitualCircle", "PitMouth", "EliteCrown", "EliteSigil",
                  "Quest_Ichor")     # mat_breach_ichor: the clots' black blood (monsters.md §2.8) glows as they do
EMBER_HUE = (15.0, 35.0)          # DR5 ember band (hue °, sat > .6, value ≥ .35: hue_audit's floor, R22)
EMBER_SAT, EMBER_VAL = 0.6, 0.35
VIOLET_HUE, VIOLET_DEG, VIOLET_SAT = 288.0, 30.0, 0.5     # breach-violet #B02FD0 ± 30° at sat > .5


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


def stats(st: dict, label: str, cls: str, em: np.ndarray | None = None) -> dict:
    body, rgb = st["body"], st["rgb"]
    solid = body > 0.98
    if em is not None:            # emissive accents (a lantern's flame, eyes) are not lit by the key: leave them out
        ox, oy = st["offset"]
        H, W = body.shape
        es = np.zeros((H, W), np.float32)
        e = em[oy:oy + H, ox:ox + W, :3].max(-1)
        es[:e.shape[0], :e.shape[1]] = e
        solid = solid & (es <= K.EMISSIVE_EDGE_MIN)
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


def emissive_hues(st: dict, em: np.ndarray | None) -> dict:
    """The colours of a group's own emissive pixels (the frame's emissive pass inside its coverage, above the
    ``cohkit.EMISSIVE_EDGE_MIN`` bloom level): how many fall in the DR5 ember band and how many in breach-violet
    (hue / saturation of each pixel's normalised sRGB colour; brightness is the emitter's, not the colour's)."""
    if em is None:
        return {}
    ox, oy = st["offset"]
    body = st["body"]
    H, W = body.shape
    e = np.zeros((H, W, 3), np.float32)
    es = em[oy:oy + H, ox:ox + W, :3]
    e[:es.shape[0], :es.shape[1]] = es
    on = (e.max(-1) > K.EMISSIVE_EDGE_MIN) & (body > 0.5)
    n = int(on.sum())
    if not n:
        return {"emissivePx": 0}
    c = e[on] / np.maximum(e[on].max(-1, keepdims=True), 1e-6)            # chromaticity, max channel 1
    c = np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1 / 2.4) - 0.055)
    mx, mn = c.max(-1), c.min(-1)
    sat = np.where(mx > 1e-4, (mx - mn) / np.maximum(mx, 1e-4), 0.0)
    r, g, b = c[:, 0], c[:, 1], c[:, 2]
    d = np.maximum(mx - mn, 1e-6)
    hue = np.where(mx == r, ((g - b) / d) % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) * 60.0
    ember = (hue >= EMBER_HUE[0]) & (hue <= EMBER_HUE[1]) & (sat > EMBER_SAT) & (mx >= EMBER_VAL)
    dv = np.abs((hue - VIOLET_HUE + 180.0) % 360.0 - 180.0)
    violet = (dv <= VIOLET_DEG) & (sat > VIOLET_SAT)
    return {"emissivePx": n, "emberPx": int(ember.sum()), "violetPx": int(violet.sum()),
            "emissiveHueP50": round(float(np.median(hue)), 1)}


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
            rec = stats(st, lab, cls, K.LAST_EMISSIVE.get("em"))
            rec.update(emissive_hues(st, K.LAST_EMISSIVE.get("em")))
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


def row_offsets(rows: list, base: float = 1.12) -> list[float]:
    """Screen-right offsets (m) of a row packed by footprint: each figure takes at least ``base`` / 2 either side, a
    wide one (a banner, a standard, a barrow) its bounds' half-extent so no two silhouettes meet."""
    half = []
    for asset, _l, var, facing in rows:
        e = K.entry(asset)
        b = e.get("boundsCm")
        sc = ((e.get("variants") or {}).get(var) or {}).get("scale", 1.0) if var else 1.0
        ext = max(b["max"][0] - b["min"][0], b["max"][1] - b["min"][1]) / 100.0 * sc if b else 0.0
        if facing == "side":                         # in profile: its length runs along the row
            ext *= 1.05
        half.append(max(base / 2, 0.5 * ext * 0.78))
    xs, x = [], 0.0
    for i, hw in enumerate(half):
        if i:
            x += half[i - 1] + hw + 0.06
        xs.append(x)
    mid = (xs[0] - half[0] + xs[-1] + half[-1]) / 2 if xs else 0.0
    return [v - mid for v in xs]


def key_light(figs: list, recs: list, cam) -> None:
    """Per character, the side the key light comes from, measured on the toon's own light terms (``darkgate``'s light
    pass: h = N·L × .5 + .5 per pixel, albedo-free): the screen direction from the centroid of the shade band
    (h < ``T_SHADE``) to the centroid of the light band (h ≥ ``T_LIGHT``). ``keyLightOk`` when it points into the
    upper-left half-plane (the one ``L_KEY``); the albedo split ``litUL`` (upper-left − lower-right luma) stays a
    diagnostic — a dark tail or cloak on one side (the Lanternwisp's soot wisps) moves it without any light change."""
    import darkgate as DG
    from kit import shading
    every = [m for f in figs for m in f.all()]
    by_label = {r["label"]: r for r in recs}
    for f in figs:
        rec = by_label.get(f.label)
        if rec is None:
            continue
        box = K.projected_box(cam, f.meshes, RES, pad_px=(14, 14))
        h, _ = DG.light_pass(f, every, box)
        cov = h > 0.02
        lit, shade = cov & (h >= shading.T_LIGHT), cov & (h < shading.T_SHADE)
        if lit.sum() < 20 or shade.sum() < 20:
            continue
        ly, lx = np.nonzero(lit)
        sy, sx = np.nonzero(shade)
        dx, dy = float(lx.mean() - sx.mean()), float(ly.mean() - sy.mean())   # image y grows downward
        rec["keyDirDeg"] = round(math.degrees(math.atan2(-dy, dx)), 1)          # 0 = screen right, 90 = up
        rec["keyLightOk"] = (dx + dy) < 0.0


def characters(results: dict, page: str | None = None) -> bool:
    K.reset()
    review.setup_render(RES, 8)
    cam = review.game_camera((0.0, 0.0))
    r, u = screen_axes(cam)
    se = review.FACINGS["se"]
    man = K.manifest()["assets"]
    if page is None:
        front = [(a, lab, var, "se") for a, lab, var in FRONT if a in man]
        back = [(a, lab, None, "se") for a, lab in BACK if a in man]
        fx, bx = [(i - (len(front) - 1) / 2) * 1.12 for i in range(len(front))], \
            [(i - (len(back) - 1) / 2) * 1.12 for i in range(len(back))]
        name, label = "lineup_characters", ("CH1 CHARACTERS  W1 CAMERA DEFAULT DISTANCE  NATIVE 1080P 1:1  SHIPPED FBX "
                                            "+ INK + BLOOM")
    else:
        plabel, front, back = MARCH_PAGES[page]
        front = [f for f in front if f[0] in man]
        back = [b for b in back if b[0] in man]
        fx, bx = row_offsets(front), row_offsets(back)
        name, label = f"lineup_march_{page}", f"{plabel}  MARCH TERRAIN  W1 CAMERA  NATIVE 1080P 1:1"
    figs = []
    for (asset, lab, var, facing), x in zip(front, fx):
        at = r * x - u * 1.55
        figs.append(K.import_character(asset, at, review.FACINGS[facing], variant=var, label=lab, cam=cam))
    nfront = len(figs)
    for (asset, lab, var, facing), x in zip(back, bx):
        at = r * x + u * 1.75
        figs.append(K.import_character(asset, at, review.FACINGS[facing], variant=var, label=lab, cam=cam,
                                       scenery=asset.startswith("SK_NPC_Blacksmith")))
    lane = [(int(round(f.root.location.x)) + dc, int(round(-f.root.location.y)) + dr)
            for f in figs[:nfront] for dc in (-1, 0, 1) for dr in (-1, 0, 1)]
    shadows = [(f.root.location.x, -f.root.location.y,
                K.entry(f.asset).get("blobShadowRadiusCm", 30) / 100.0 * f.root.scale[0], 0.45) for f in figs]
    groups = []
    for f in figs:
        fam = "pets" if f.asset.startswith("SK_Pet_") else FAMILY_OF.get(f.cls, "other")
        e = K.entry(f.asset)
        rest_top = e["boundsCm"]["max"][2] * f.root.scale[0] if e.get("boundsCm") else None
        extra = {}
        if e.get("boundsCm"):             # bounding-box diagonals: a carried pole / banner re-posed from its bind pose
            b = e["boundsCm"]             # keeps its diagonal; an FBX scale error does not
            extra["restDiagCm"] = round(math.dist(b["max"], b["min"]) * f.root.scale[0], 1)
            pts = review.mesh_points(f.meshes[: f.n_body], step=4)
            extra["posedDiagCm"] = round(float(np.linalg.norm(pts.max(0) - pts.min(0))) * 100.0, 1)
        groups.append((f.label, f.meshes, f.cls, {"asset": f.asset, "family": fam, "heightCm": round(f.height_m * 100, 1),
                                                   "topCm": round(f.top_cm(), 1), "anchor": f.root.location.copy(),
                                                   "px": f.px, "gatePx": K.CHAR_GATE_PX, **extra,
                                                   **({"restTopCm": round(rest_top, 1)} if rest_top else {})}))
        for x in f.extra:
            if x.name.startswith("COH_SM_FX_EliteCrown"):
                crown = x.name[4:].split(".")[0]
                groups.append((f.label + " CROWN", [x], "monster", {"asset": crown, "family": "monsters",
                                                                    "px": float(x.get("af_px1080", 3.0)),
                                                                    "nolabel": True}))
            elif "AnvilStump" in x.name:
                stump = x.name[4:].split(".")[0]
                groups.append((f.label + " ANVIL", [x], K.slot_info(K.entry(stump))[1],
                               {"asset": stump, "family": "npcs", "nolabel": True,
                                "px": K.slot_info(K.entry(stump))[2]}))
    recs = shoot(groups, cam, name, label, ground_dirt=lane, shadows=shadows)
    key_light(figs, recs, cam)
    results["characters" if page is None else f"characters_{page}"] = recs
    return all(x["inkOk"] for x in recs)


def props(results: dict, only=None, march: bool = False) -> bool:
    ok = True
    results.setdefault("props", [])
    groups_ = MARCH_PROP_GROUPS if march else PROP_GROUPS
    for gname, (sp, items) in groups_.items():
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
        w = K.import_character("SK_Hero_Warrior", r * (xs[0] - half[0] - (1.0 if march else 0.75)) + u * 0.35,
                               review.FACINGS["se"], label="WARRIOR")
        groups.append(("WARRIOR", w.meshes, w.cls, {"asset": w.asset, "family": "heroes", "group": gname, "px": w.px,
                                                     "gatePx": K.CHAR_GATE_PX, "scaleRef": True,
                                                     "anchor": w.root.location.copy()}))
        shadows.append((w.root.location.x, -w.root.location.y, 0.39, 0.45))
        if march or gname in ("story", "goblin", "foliage"):
            ref, rlab = (SCALE_REF_MARCH, "FOOTMAN") if march else ("SK_Mon_Goblin", "GOBLIN")
            g = K.import_character(ref, r * (xs[-1] + half[-1] + 0.6) + u * 0.35, review.FACINGS["se"], label=rlab)
            groups.append((rlab, g.meshes, g.cls, {"asset": g.asset, "family": "monsters", "group": gname,
                                                   "px": g.px, "gatePx": K.CHAR_GATE_PX, "scaleRef": True,
                                                   "anchor": g.root.location.copy()}))
            shadows.append((g.root.location.x, -g.root.location.y,
                            K.entry(ref).get("blobShadowRadiusCm", 20) / 100.0, 0.45))
        if march:
            recs = shoot(groups, cam, f"lineup_march_props_{gname}",
                         f"CH1 MARCH {gname.upper().replace('_', ' ')}  MARCH TERRAIN  W1 CAMERA  NATIVE 1080P 1:1  "
                         "SHIPPED FBX + INK + BLOOM", shadows=shadows)
        else:
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
        fc = [c for c in chars if c["family"] == fam]
        kd = [c for c in fc if "keyLightOk" in c]
        xs = [c.get("litUL") for c in fc if c.get("litUL") is not None]
        if kd:                      # measured on the light pass (albedo-free): the family is lit from the upper left
            bad = [f"{c['label']} {c['keyDirDeg']:.0f}°" for c in kd if not c["keyLightOk"]]
            # the median screen direction (angles unwrapped round 135°) must lie in the upper-left half-plane; single
            # figures can point elsewhere when one large flat plane dominates a band (a banner turned from the key)
            m = med([((c["keyDirDeg"] - 135.0 + 180.0) % 360.0) - 180.0 + 135.0 for c in kd])
            rules.append({"rule": "key light side", "subject": fam, "ok": 45.0 < m < 225.0,
                          "detail": f"light-band centroid up-left of the shade band on {len(kd) - len(bad)}/{len(kd)} "
                                    f"(screen direction median {m:.0f}°, 135° = upper left)" +
                                    (f"; single figures where a large plane turned from the key dominates its shade "
                                     f"band: {', '.join(bad)}" if bad else "") +
                                    (f"; albedo split median {med(xs):+.1f}" if xs else "")})
        elif xs:
            rules.append({"rule": "key light side", "subject": fam, "ok": med(xs) > 0,
                          "detail": f"median lum(upper-left) − lum(lower-right) {med(xs):+.1f} over {len(xs)}"})
    xs = [p.get("litUL") for p in props_ if p.get("litUL") is not None]
    if xs:
        rules.append({"rule": "key light side", "subject": "props", "ok": med(xs) > 0,
                      "detail": f"median {med(xs):+.1f} over {len(xs)}"})
    # FBX re-import scale: the posed top (Idle) against the exported rest bounds (× the variant scale) — the R2
    # heights themselves are the checklist's manifest check (heightCm = body height; feathers / horns / sprout stand
    # above it, listed here for information)
    bad, n, above, reposed = [], 0, [], []
    for c in chars:
        ref = c.get("restTopCm")
        if c.get("topCm") and ref:
            n += 1
            k = c["topCm"] / ref
            if not 0.85 <= k <= 1.15:
                kd = (c.get("posedDiagCm") or 0) / max(c.get("restDiagCm") or 1e-6, 1e-6)
                if 0.85 <= kd <= 1.15:     # the bind pose lays a long prop down (the standard-bearer's banner)
                    reposed.append(f"{c['label']} top {c['topCm']:.0f}/{ref:.0f} cm, bounds diagonal "
                                   f"{c['posedDiagCm']:.0f}/{c['restDiagCm']:.0f} cm")
                else:
                    bad.append(f"{c['label']} {c['topCm']:.0f}/{ref:.0f} cm")
            if c.get("heightCm") and c["topCm"] > c["heightCm"] * 1.08:
                above.append(f"{c['label']} {c['topCm']:.0f} (body {c['heightCm']:.0f})")
    if n:
        rules.append({"rule": "R2 scale (FBX re-import)", "subject": "characters", "ok": not bad,
                      "detail": f"posed top within 15 % of the exported rest top on {n - len(bad)}/{n}" +
                                (f"; off: {', '.join(bad)}" if bad else "") +
                                (f"; re-posed from the bind pose (diagonal within 15 %): {', '.join(reposed)}"
                                 if reposed else "") +
                                (f"; headgear above the R2 body height: {', '.join(above)}" if above else "")})
    gates = [c for c in every if not c.get("inkOk", True)]
    rules.append({"rule": "lineup ink gate", "subject": "all", "ok": not gates,
                  "detail": f"{len(every)} meshes gated at 1080p" +
                            (f"; failed: {', '.join(c['label'] for c in gates)}" if gates else "")})
    return rules


def march_rules(results: dict) -> list:
    """The dark remake's colour discipline on the rendered lineup (DR5, monsters.md §1.2) + the value hierarchy."""
    rules = []
    chars = [c for k, v in results.items() if k.startswith("characters") for c in v]
    props_ = results.get("props", [])
    every = chars + props_
    hostile = [c for c in chars if c.get("family") == "monsters" and "CROWN" not in c["label"]]
    hostile += [c for c in chars if "CROWN" in c["label"]]
    friendly = [c for c in chars if c.get("family") in ("heroes", "npcs", "pets")]
    bad = [f"{c['label']} {c['emberPx']} px" for c in hostile if c.get("emberPx", 0) > 0]
    rules.append({"rule": "DR5 no enemy ember", "subject": "monster emissives", "ok": not bad,
                  "detail": f"{len(hostile)} hostile figures / markers: emissive pixels in the ember band (hue 15-35°, "
                            f"sat > .6, value >= .35) " + (f"on {', '.join(bad)}" if bad else "none") +
                            f"; emissive px total {sum(c.get('emissivePx', 0) for c in hostile)}"})
    bad = [f"{c['label']} {c['violetPx']} px" for c in friendly if c.get("violetPx", 0) > 0]
    rules.append({"rule": "DR5 violet is the enemy's", "subject": "hero / NPC / pet emissives", "ok": not bad,
                  "detail": f"{len(friendly)} friendly figures: emissive pixels within 30° of breach-violet #B02FD0 "
                            f"at sat > .5 " + (f"on {', '.join(bad)}" if bad else "none")})
    bad = [f"{p['label']} {p['violetPx']} px" for p in props_ if p.get("violetPx", 0) > 0
           and not any(k in p.get("asset", "") for k in BREACH_SOURCED)]
    seen = sorted({p["label"] for p in props_ if p.get("violetPx", 0) > 0} - {b.split(" ")[0] for b in bad})
    rules.append({"rule": "DR5 violet is the enemy's", "subject": "props outside the breach", "ok": not bad,
                  "detail": "breach-violet emissives only on breach-sourced props (the dais, ritual circles, the Pit "
                            "mouth, the elite markers, the breach-blood ichor)" +
                            (f": {', '.join(seen)}" if seen else "") + (f"; also on {', '.join(bad)}" if bad else "")})
    ember_props = sorted({p["label"] for p in props_ if p.get("emberPx", 0) > 0})
    rules.append({"rule": "DR5 reserved warm light", "subject": "props", "ok": all(
        any(k in p.get("asset", "") for k in ("Hearth", "AnvilStump", "BellFrame", "Strongbox", "SealShard",
                                              "HandbellPost", "Loot", "Pickup", "GoldPile", "Hearthstone"))
        for p in props_ if p.get("emberPx", 0) > 0),
        "detail": "ember emissives only on refuges, interaction and loot (hearth, forge, bell-frame lantern, "
                  "strongbox, seal shard, handbells, loot): " + (", ".join(ember_props) or "none")})

    def p90(xs):
        xs = [x for x in xs if x is not None]
        return float(np.median(xs)) if xs else None
    fam_v = {}
    for fam in ("heroes", "monsters", "npcs", "pets"):
        v = p90([c.get("lumP90") for c in chars if c.get("family") == fam and "CROWN" not in c["label"]
                 and "ANVIL" not in c["label"]])
        if v is not None:
            fam_v[fam] = v
    dv = p90([p.get("lumP90") for p in props_ if p.get("cat") == "decor"])
    if dv is not None:
        low = [f for f, v in fam_v.items() if v < dv]
        rules.append({"rule": "value hierarchy", "subject": "actors above decor", "ok": not low,
                      "detail": "median lit p90 luma: " + ", ".join(f"{k} {v:.0f}" for k, v in fam_v.items()) +
                                f"; decor {dv:.0f}" + (f"; below the decor: {', '.join(low)}" if low else "")})
    return rules


def main(argv: list[str]) -> int:
    global OUT, METRICS
    march = "--march" in argv
    if march:
        OUT, METRICS = OUT_MARCH, METRICS_MARCH
        K.set_theme("march")
    OUT.mkdir(parents=True, exist_ok=True)
    want_c = "--props" not in argv or "--characters" in argv
    want_p = "--characters" not in argv or "--props" in argv
    only = [a for a in argv if not a.startswith("--")]
    results = json.loads(METRICS.read_text()).get("results", {}) if METRICS.is_file() else {}
    ok = True
    if want_c:
        if march:
            for page in MARCH_PAGES:
                if only and page not in only:
                    continue
                ok = characters(results, page) and ok
        else:
            ok = characters(results) and ok
    if want_p:
        ok = props(results, [o for o in only if o not in MARCH_PAGES], march) and ok
    if march:                                   # family_rules reads results["characters"]: every march page
        merged = dict(results)
        merged["characters"] = [c for k, v in results.items() if k.startswith("characters_") for c in v]
        rules = family_rules(merged) + march_rules(results)
    else:
        rules = family_rules(results)
    for rr in rules:
        print(f"  [rule] {'OK ' if rr['ok'] else 'GAP'} {rr['rule']:26s} {rr['subject']:22s} {rr['detail']}")
    METRICS.write_text(json.dumps({"results": results, "rules": rules}, indent=1, ensure_ascii=False) + "\n")
    print(f"  {METRICS}")
    return 0 if ok and all(rr["ok"] for rr in rules) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

"""Chapter-1 coherence vignettes: the camp and the goblin camp composed from the shipped environment kit with the
shipped characters placed — ``Art/Previews/ch1_coherence/vignette_{camp,goblin_camp}*.png``.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/coherence/vignettes.py [camp] [goblin]
    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/coherence/vignettes.py --march
        [knellwatch] [red_field] [ashwheat]

Everything is re-imported from ``Art/Export`` (``cohkit``) and placed the way the runtime places it:

* **Camp** (``emerald_plains`` camp 0 at (15,15), DECISIONS W11): the MapGen camp stamp (11 × 11 camp ground, palisade
  on the gate row except the 2-tile gate at x −1 / 0 and on both sides up to row +3, ``MapGen.cpp``), the palisade
  stakes exactly as ``AAbyssZoneActor::BuildWalls`` (centre + 0.2 / 0.4 toward every camp-wall neighbour, variant and
  yaw from ``tileHash(c·5+i, r, 17)``), the gate posts at x −1.5 / +0.5, every ``abyss/world/CampLayout.h`` prop
  (well / barrels / crates facing the fire, the rest facing the camera; no tents), the four camp NPCs on slots 0–3
  (blacksmith with his anvil stump, merchant, elder, stash keeper), the three heroes and the ley sprite inside the gate,
  oaks / fruit trees / bushes / grass outside.
* **Goblin camp** at (40,52) (spec §6.9, R7; the ``q_explore_goblin_camp`` objective and the ``cs_ep_whisper`` focus):
  the two hide huts, the fire pit, bone totems, a spear rack and crates on trampled dirt, goblins round the fire, the
  shaman at the pit, the chief by the big hut, a red-cap hunt leader; the rogue and the ley sprite sneaking in.

Each scene: ``vignette_<name>.png`` = the whole frame as a native 640 × 360 render of the W1 camera (ink and bloom at
360p), ``vignette_<name>_1080crop*.png`` = native 1080p 1:1 crops (truecolour tiles, ``pngio.write_png_native``).

**The march** (dark remake Chapter 1, 焚境 · The Burning March; ``--march`` or the scene names ``knellwatch``,
``red_field``, ``ashwheat``) → ``Art/Previews/ch1_coherence_march/`` under the presentation theme ``march``
(``cohkit.set_theme``: every id resolves ``<id>@march`` first, the march terrain set, the march elite crown), labels
in a reserved band above each image (``envkit.write_banded``):

* **Knellwatch** (camp 1 (15,15), chapter1.md §2): the camp stamp, ``palisade_plains@march`` pews and the
  ``palisade_gate_plains@march`` posts laid like ``BuildWalls``, every ``CampLayout.h`` prop resolved under the theme
  (the hearth, the well, Knellwatch's banners, the braziers, sacks and crates), the U4 set pieces where their handoff
  puts them (``setpiece_hearth_dressing`` on the hearth's tile and yaw; ``setpiece_bell_frame`` beyond the fire, its
  door toward the hearth, ≥ 2 tiles from every NPC slot; ``setpiece_name_wall`` on the inner west palisade between its
  torches — proposals, U4 owns the placement), camp 1's five NPCs on slots 0-4 in the data's order (Hald with his
  charred anvil, Vira, the Captain, Aldwin, Tamsin), the three heroes and the Lanternwisp inside the gate, dead oaks,
  charred orchard trees, briars, ash grass and poppies outside;
* **The Red Field** (chapter1.md §1): the Red Banner on its mound of shields (``story_rf_red_banner`` (40,70),
  ``decor_war_banner@march``) and the Dead Ranks' shield wall (spawn (30,80): three shieldbearers, the standard-bearer
  and two footmen, the W2 mix) on the same screen row, 14 tiles apart, helmets on spears along the approach, the
  warrior and the rogue between them, gravegnawers at a carcass line, dead oaks, briars, broken stone and ash grass;
* **Ashwheat** (chapter1.md §1 / M3, (40,52) r8): the burned bridge over its dry ditch (38,48), the cottages with their
  candles (``setpiece_ashwheat_cottage``, set-piece layer: a proposal), charred orchard trees, ash poppies, the mage and
  the Lanternwisp coming down into the ditch, two gravegnawers prowling the far bank.
"""
from __future__ import annotations

import json
import math
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import cohkit as K  # noqa: E402
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector  # noqa: E402

from kit import paths, pngio, review  # noqa: E402

import envkit as E  # noqa: E402

OUT = paths.preview_root() / "ch1_coherence"
CAMERA_YAW = 45.0
MESH_YAW = K.MESH_YAW
FACE_CAM = CAMERA_YAW + 180.0 + MESH_YAW          # actor yaw (UE) of a prop facing the camera, mesh yaw included


class Scene:
    def __init__(self):
        self.objs: list = []                 # every placed mesh
        self.shadows: list = []              # (col, row, radius m, alpha)
        self.figs: list = []

    def prop(self, name: str, col: float, row: float, yaw_ue: float = FACE_CAM, scale: float = 1.0, shadow=True):
        o = K.import_static(name)
        o.location = E.tile_to_blender(col, row, 0.0)
        o.rotation_euler = (0, 0, E.ue_yaw_to_blender(yaw_ue))
        o.scale = (scale,) * 3
        self.objs.append(o)
        if shadow and not any(k in name for k in ("Grass", "Flower", "Lily")):
            e = K.entry(name)
            rad = e.get("blobShadowRadiusCm")
            if rad is None:
                b = e["boundsCm"]
                rad = 0.42 * max(b["max"][0] - b["min"][0], b["max"][1] - b["min"][1])
            self.shadows.append((col, row, min(rad / 100.0, 1.3) * scale, 0.40))
        return o

    def variant(self, gid: str, h: int) -> str:
        """The runtime's variant pick under the current presentation theme (``<gid>@<theme>`` first)."""
        names = K.variant_names(gid)
        if not names:
            raise KeyError(f"no asset for game id {gid!r} (theme {K.THEME})")
        return names[h % len(names)]

    def char(self, asset: str, col: float, row: float, facing_deg: float, clip=None, frame=0, variant=None,
             scenery=False, cam=None):
        f = K.import_character(asset, E.tile_to_blender(col, row, 0.0), facing_deg, clip=clip, frame=frame,
                               variant=variant, scenery=scenery, cam=cam)
        self.figs.append(f)
        self.objs += f.all()
        self.shadows.append((col, row, K.entry(asset).get("blobShadowRadiusCm", 30) / 100.0 * f.root.scale[0], 0.45))
        return f


def face_towards(col, row, tc, tr) -> float:
    """Blender Z rotation (deg) for a character (modelled facing −Y) at (col, row) to face tile (tc, tr)."""
    p, q = E.tile_to_blender(col, row), E.tile_to_blender(tc, tr)
    d = q - p
    return math.degrees(math.atan2(d.x, -d.y))


def render_scene(sc: Scene, focus_tile, name: str, label: str, crops) -> None:
    bpy.context.view_layer.update()
    focus = E.tile_to_blender(*focus_tile)
    review.setup_render((640, 360), 8)
    cam = review.game_camera((focus.x, focus.y))
    small, _ = K.render(sc.objs, None, res=(640, 360))

    def lab(t, i, k):
        review.draw_text(t, 6, 6, label + "  NATIVE 640X360" + (f"  {i + 1}/{k}" if k > 1 else ""))
    for p, s in pngio.write_png_native(OUT / f"{name}.png", small, annotate=lab):
        print(f"  {p}  {s // 1024} KB")
    review.setup_render((1920, 1080), 8)
    cam = review.game_camera((focus.x, focus.y))
    for (ct, w_px, h_px, clabel, cname) in crops:
        x, y = review.project_px(cam, E.tile_to_blender(ct[0], ct[1], 0.5), (1920, 1080))
        x0 = min(max(x - w_px / 2, 0), 1920 - w_px)
        y0 = min(max(y - h_px / 2, 0), 1080 - h_px)
        box = (x0 / 1920, 1 - (y0 + h_px) / 1080, (x0 + w_px) / 1920, 1 - y0 / 1080)
        img, _ = K.render(sc.objs, box)

        def lab2(t, i, k, clabel=clabel):
            review.draw_text(t, 6, 6, clabel + "  NATIVE 1080P 1:1" + (f"  TILE {i + 1}/{k}" if k > 1 else ""))
        for p, s in pngio.write_png_native(OUT / cname, img, annotate=lab2):
            print(f"  {p}  {s // 1024} KB")


# ── camp ────────────────────────────────────────────────────────────────────────────────────────────────
CAMP = (15, 15)
CAMP_LAYOUT_H = paths.ART_ROOT.parent / "Source/AbyssCore/Public/abyss/world/CampLayout.h"
HERO_NEAR_TILES = 3.0           # QuestWorld heroNear: a camp NPC turns to a hero within 3 tiles


def camp_layout() -> tuple[list, list]:
    """(``kCampNpcSlots``, ``kCampProps``) parsed from ``abyss/world/CampLayout.h`` — the runtime's own table (camp
    layout v2, DECISIONS W11), never a copy: slots [(x, y)], props [(type, x, y, blocking, facesCentre)]."""
    src = CAMP_LAYOUT_H.read_text()
    num = r"(-?[0-9]+(?:\.[0-9]*)?)"
    body = src.split("kCampNpcSlots{{", 1)[1].split("}};", 1)[0]
    slots = [(float(a), float(b)) for a, b in re.findall(r"\{\s*" + num + r",\s*" + num + r"\s*\}", body)]
    body = src.split("kCampProps{{", 1)[1].split("}};", 1)[0]
    props = [(t, float(x), float(y), bl == "true", fc == "true") for t, x, y, bl, fc in re.findall(
        r'\{"([a-z_]+)",\s*' + num + r",\s*" + num + r",\s*(true|false),\s*(true|false)\}", body)]
    if len(slots) < 4 or len(props) < 10:
        raise RuntimeError(f"CampLayout.h parse: {len(slots)} slots, {len(props)} props")
    return slots, props


def camp_npc_facing(slot: int, slots: list) -> tuple[float, float]:
    """``abyss::camp::CampNpcFacing``: every slot looks at the campfire (tile-space unit vector; the camp has no tents,
    DECISIONS W11)."""
    x, y = slots[slot % len(slots)]
    n = math.hypot(x, y)
    return (-x / n, -y / n)


def round_to_tile(v: float) -> int:
    """``abyss::RoundToTile`` (JS Math.round: floor(x + 0.5))."""
    return int(math.floor(v + 0.5))


def camp_tiles(c0, r0, c1, r1):
    cx, cy = CAMP
    m = np.zeros((r1 - r0, c1 - c0), np.int32)

    def put(c, r, v):
        if c0 <= c < c1 and r0 <= r < r1:
            m[r - r0, c - c0] = v
    for dr in range(-5, 6):
        for dc in range(-5, 6):
            put(cx + dc, cy + dr, 5)
    for dc in range(-5, 6):
        if dc not in (-1, 0):
            put(cx + dc, cy - 5, 6)
    for dr in range(-5, 4):
        put(cx - 5, cy + dr, 6)
        put(cx + 5, cy + dr, 6)
    for k in range(1, 9):                                    # the path leaving the gate (toward the camera)
        for dc in (-1, 0):
            put(cx + dc + (1 if k > 5 else 0) * (k - 5) // 2, cy - 5 - k, 1)
    return m


def camp() -> None:
    K.reset()
    sc = Scene()
    cx, cy = CAMP
    c0, r0, c1, r1 = cx - 16, cy - 18, cx + 17, cy + 15
    tiles = camp_tiles(c0, r0, c1, r1)
    review.setup_render((1920, 1080), 8)
    cam = review.game_camera(tuple(E.tile_to_blender(cx, cy - 1.0))[:2])
    # palisade (BuildWalls)
    def wall(c, r):
        return c0 <= c < c1 and r0 <= r < r1 and tiles[r - r0, c - c0] == 6
    for r in range(r0, r1):
        for c in range(c0, c1):
            if not wall(c, r):
                continue
            spots = [(c, r)]
            for dc, dr in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                if wall(c + dc, r + dr):
                    spots += [(c + dc * 0.2, r + dr * 0.2), (c + dc * 0.4, r + dr * 0.4)]
            for i, (sx, sy) in enumerate(spots):
                h = E.tile_hash(c * 5 + i, r, 17)
                sc.prop(sc.variant("palisade_plains", h), sx, sy, float(h % 360), shadow=False)
    for gx in (cx - 1.5, cx + 0.5):
        sc.prop("SM_Palisade_Plains_GatePost", gx, cy - 5, MESH_YAW + 90.0)
    # camp props (CampLayout v2, parsed from CampLayout.h; MapGen CampProps + AAbyssZoneActor::BuildCamp: sub-tile
    # centre, the well / barrels / crates turned to the camp centre, the rest to the camera, variant by
    # TileHash(RoundToTile(centre), 23))
    slots, props = camp_layout()
    for typ, x, y, _bl, faces in props:
        name = sc.variant(f"camp_{typ}", E.tile_hash(round_to_tile(cx + x), round_to_tile(cy + y), 23))
        yaw = (math.degrees(math.atan2(-y, -x)) + MESH_YAW) if faces else FACE_CAM
        sc.prop(name, cx + x, cy + y, yaw)
    # the party just inside the gate (gap x −1.5 … +0.5 on row −5), turned to the camera; the ley sprite by the rogue
    # (one screen row clear of the gate posts and banners: screen right = (−col + row)/√2, down = −(col + row)/√2)
    party = [("SK_Hero_Warrior", (-0.03, -3.22), "se"), ("SK_Hero_Mage", (-0.53, -2.72), "se"),
             ("SK_Hero_Rogue", (0.46, -3.71), "front"), ("SK_Pet_Sprite", (-0.21, -3.89), "se")]
    for asset, (dc, dr), facing in party:
        if asset in K.manifest()["assets"]:
            sc.char(asset, cx + dc, cy + dr, review.FACINGS[facing], clip="Idle", frame=0)
    # camp NPCs on slots 0-3 (Zone.cpp: npcs[i] on kCampNpcSlots[i]), facing CampNpcFacing(slot) — or the nearest hero
    # within 3 tiles (QuestWorld heroNear)
    heroes = [(dc, dr) for a, (dc, dr), _ in party if a.startswith("SK_Hero_")]
    for i, (asset, scen) in enumerate((("SK_NPC_Blacksmith", True), ("SK_NPC_Merchant", False),
                                       ("SK_NPC_QuestElder", False), ("SK_NPC_StashKeeper", False))):
        sx, sy = slots[i]
        near = min(heroes, key=lambda h: math.hypot(h[0] - sx, h[1] - sy))
        if math.hypot(near[0] - sx, near[1] - sy) <= HERO_NEAR_TILES:
            fc = face_towards(cx + sx, cy + sy, cx + near[0], cy + near[1])
        else:
            fx, fy = camp_npc_facing(i, slots)
            fc = face_towards(cx + sx, cy + sy, cx + sx + fx, cy + sy + fy)
        sc.char(asset, cx + sx, cy + sy, fc, clip=None, scenery=scen)
    # dressing outside the palisade (decor game ids: variants by tile hash, jitter / scale like MapGen)
    scatter = [("tree", -7.2, -3.5), ("tree", 7.4, -2.6), ("tree_round", -7.0, 2.4), ("tree", 6.8, 3.8),
               ("tree_round", 8.6, -6.4), ("bush", -6.6, -6.2), ("bush", 6.2, -7.0), ("bush", -8.4, 0.6),
               ("rock", 4.2, -8.2), ("rock", -4.6, -8.6), ("mushroom_red", -6.2, -1.2), ("boulder", 9.0, 1.0)]
    for gid, dx, dy in scatter:
        h = E.tile_hash(int((cx + dx) * 10), int((cy + dy) * 10), 5)
        sc.prop(sc.variant(gid, h), cx + dx, cy + dy, float(h % 360), scale=0.92 + 0.16 * ((h >> 11) % 100) / 100)
    rng = np.random.default_rng(7)
    for k in range(60):
        dx, dy = rng.uniform(-11, 11), rng.uniform(-12, 9)
        if abs(dx) < 6 and -6 < dy < 6:
            continue
        gid = "grass" if k % 3 else "flower"
        h = E.tile_hash(int((cx + dx) * 10), int((cy + dy) * 10), 7)
        sc.prop(sc.variant(gid, h), cx + dx, cy + dy, float(h % 360), scale=0.9 + 0.2 * ((h >> 5) % 100) / 100)
    K.ground(tiles, (c0, r0), ppt=64, shadows=sc.shadows, name="g_camp")
    render_scene(sc, (cx, cy - 0.5), "vignette_camp",
                 "CH1 CAMP (CAMPLAYOUT V2)  W1 CAMERA  HEROES + LEY SPRITE AT THE GATE",
                 [((cx + 0.5, cy + 1.2), 760, 470, "CAMP: FIRE, WELL, NPC SLOTS 0-3", "vignette_camp_1080crop.png"),
                  ((cx + 0.24, cy - 2.93), 640, 420, "CAMP GATE: HEROES + LEY SPRITE", "vignette_camp_gate_1080crop.png")])


# ── goblin camp ─────────────────────────────────────────────────────────────────────────────────────────
GCAMP = (40, 52)


def goblin_camp() -> None:
    K.reset()
    sc = Scene()
    gx, gy = GCAMP
    c0, r0, c1, r1 = gx - 15, gy - 15, gx + 16, gy + 14
    tiles = np.zeros((r1 - r0, c1 - c0), np.int32)
    yy, xx = np.mgrid[r0:r1, c0:c1]
    d = np.hypot(xx - gx, (yy - gy) * 1.1)
    noise = np.sin(xx * 1.7 + yy * 0.6) * 0.6 + np.cos(yy * 1.3 - xx * 0.4) * 0.5
    tiles[d + noise < 4.6] = 1                               # trampled dirt round the fire
    tiles[(np.abs(xx - gx + 0.0) < 1.0) & (yy < gy - 3) & (yy > gy - 12)] = 1   # the trail out (toward the camera)
    review.setup_render((1920, 1080), 8)
    cam = review.game_camera(tuple(E.tile_to_blender(gx, gy))[:2])
    fire = (gx, gy)
    sc.prop("SM_Prop_Goblin_FirePit", gx, gy, FACE_CAM)
    sc.prop("SM_Prop_Goblin_Hut_A", gx + 2.8, gy + 2.2, math.degrees(math.atan2(-2.2, -2.8)) + MESH_YAW)
    sc.prop("SM_Prop_Goblin_Hut_B", gx - 3.0, gy + 2.6, math.degrees(math.atan2(-2.6, 3.0)) + MESH_YAW)
    sc.prop("SM_Prop_Goblin_Hut_B", gx + 4.2, gy - 1.6, math.degrees(math.atan2(1.6, -4.2)) + MESH_YAW, scale=0.9)
    sc.prop("SM_Prop_Goblin_BoneTotem", gx - 1.6, gy - 3.4, FACE_CAM)
    sc.prop("SM_Prop_Goblin_BoneTotem", gx + 1.9, gy - 3.2, FACE_CAM + 20)
    sc.prop("SM_Prop_Goblin_SpearRack", gx - 3.6, gy - 0.6, FACE_CAM - 30)
    sc.prop("SM_Prop_Goblin_Crates", gx + 0.4, gy + 3.6, FACE_CAM + 160)
    sc.prop("SM_Prop_Goblin_Crates", gx - 4.4, gy + 0.9, FACE_CAM + 70, scale=0.9)
    # goblins round the fire (Idle / Attack poses), the shaman at the pit (Taunt), the chief by the big hut
    sc.char("SK_Mon_GoblinShaman", gx + 0.2, gy + 1.3, face_towards(gx + 0.2, gy + 1.3, *fire), clip="Taunt", frame=24)
    sc.char("SK_Mon_GoblinChief", gx + 2.0, gy + 0.6, face_towards(gx + 2.0, gy + 0.6, gx - 1.0, gy - 4.0),
            clip="Idle", frame=10)
    for (dx, dy, clip, fr) in ((-1.4, 0.4, "Idle", 0), (1.2, -1.3, "Idle", 20), (-0.9, -1.6, "Attack01", 9),
                               (-2.4, 1.6, "Idle", 30)):
        sc.char("SK_Mon_Goblin", gx + dx, gy + dy, face_towards(gx + dx, gy + dy, *fire), clip=clip, frame=fr)
    # the trail in (screen: −row = down-left): the red-cap on the trail facing the rogue, a slime by the totem, the
    # rogue walking up with the ley sprite at her side
    rogue = (gx - 0.2, gy - 5.6)
    sc.char("SK_Mon_Goblin", gx + 0.9, gy - 3.4, face_towards(gx + 0.9, gy - 3.4, *rogue), variant="hunt_redcap_gruk",
            cam=cam)
    sc.char("SK_Mon_Slime", gx - 2.8, gy - 3.6, face_towards(gx - 2.8, gy - 3.6, *rogue), clip="Idle", frame=5)
    sc.char("SK_Hero_Rogue", *rogue, face_towards(*rogue, gx + 0.9, gy - 3.4), clip="Walk", frame=12)
    if "SK_Pet_Sprite" in K.manifest()["assets"]:
        sc.char("SK_Pet_Sprite", gx - 1.0, gy - 5.2, face_towards(gx - 1.0, gy - 5.2, gx + 0.9, gy - 3.4), clip="Idle",
                frame=0)
    scatter = [("tree", -6.4, 3.0), ("tree", 6.6, 4.0), ("tree_round", -7.2, -3.2), ("tree", 7.6, -4.2),
               ("bush", -5.6, -5.2), ("bush", 5.4, -6.2), ("bush", 2.4, 6.0), ("rock", -3.0, 5.4),
               ("rock", 4.6, 2.0), ("boulder", -7.6, 0.6), ("mushroom_red", 3.4, -5.6)]
    for gid, dx, dy in scatter:
        h = E.tile_hash(int((gx + dx) * 10), int((gy + dy) * 10), 5)
        sc.prop(sc.variant(gid, h), gx + dx, gy + dy, float(h % 360), scale=0.92 + 0.16 * ((h >> 11) % 100) / 100)
    rng = np.random.default_rng(11)
    for k in range(50):
        dx, dy = rng.uniform(-10, 10), rng.uniform(-10, 8)
        if math.hypot(dx, dy * 1.1) < 5.0 or (abs(dx) < 1.2 and dy < -3):
            continue
        gid = "grass" if k % 4 else "flower"
        h = E.tile_hash(int((gx + dx) * 10), int((gy + dy) * 10), 7)
        sc.prop(sc.variant(gid, h), gx + dx, gy + dy, float(h % 360), scale=0.9 + 0.2 * ((h >> 5) % 100) / 100)
    K.ground(tiles, (c0, r0), ppt=64, shadows=sc.shadows, name="g_goblin")
    render_scene(sc, (gx, gy - 0.8), "vignette_goblin_camp",
                 "CH1 GOBLIN CAMP (40,52)  W1 CAMERA  SHAMAN AT THE PIT, CHIEF, RED-CAP, ROGUE + LEY SPRITE",
                 [((gx + 0.6, gy + 0.6), 760, 470, "GOBLIN CAMP: PIT, HUTS, SHAMAN, CHIEF", "vignette_goblin_camp_1080crop.png"),
                  ((gx - 0.35, gy - 4.05), 760, 540, "TRAIL IN: TOTEMS, RED-CAP, SLIME, ROGUE + LEY SPRITE",
                   "vignette_goblin_camp_trail_1080crop.png")])


# ── the march (dark remake Chapter 1) ──────────────────────────────────────────────────────────────────
OUT_MARCH = paths.preview_root() / "ch1_coherence_march"
MARCH_SCENES = ("knellwatch", "red_field", "ashwheat")


def render_scene_march(sc: Scene, focus_tile, name: str, label: str, crops) -> list:
    """``render_scene`` into ``ch1_coherence_march/`` with the labels in a reserved band above each image."""
    bpy.context.view_layer.update()
    focus = E.tile_to_blender(*focus_tile)
    review.setup_render((640, 360), 8)
    review.game_camera((focus.x, focus.y))
    small, _ = K.render(sc.objs, None, res=(640, 360))
    out = []
    for p, s_ in E.write_banded(OUT_MARCH / f"{name}.png", small, label + "  NATIVE 640X360  NO ZONE GRADE"):
        print(f"  {p}  {s_ // 1024} KB")
        out.append(p)
    review.setup_render((1920, 1080), 8)
    cam = review.game_camera((focus.x, focus.y))
    for (ct, w_px, h_px, clabel, cname) in crops:
        x, y = review.project_px(cam, E.tile_to_blender(ct[0], ct[1], 0.5), (1920, 1080))
        x0 = min(max(x - w_px / 2, 0), 1920 - w_px)
        y0 = min(max(y - h_px / 2, 0), 1080 - h_px)
        box = (x0 / 1920, 1 - (y0 + h_px) / 1080, (x0 + w_px) / 1920, 1 - y0 / 1080)
        img, _ = K.render(sc.objs, box)
        for p, s_ in E.write_banded(OUT_MARCH / cname, img, clabel + "  NATIVE 1080P 1:1"):
            print(f"  {p}  {s_ // 1024} KB")
            out.append(p)
    return out


def _decor(sc: Scene, items, salt: int = 5):
    """Decor game ids at tiles, variant / yaw / scale by tile hash as MapGen + the zone actor pick them."""
    for gid, c, r in items:
        h = E.tile_hash(int(c * 10), int(r * 10), salt)
        sc.prop(sc.variant(gid, h), c, r, float(h % 360), scale=0.92 + 0.16 * ((h >> 11) % 100) / 100)


def _cover(sc: Scene, rng, n: int, area, keep_out, grass_every: int = 3):
    """Ground cover (``grass`` / ``flower``) scattered in ``area`` (c0, c1, r0, r1) outside ``keep_out(c, r)``."""
    c0, c1, r0, r1 = area
    for k in range(n):
        c, r = rng.uniform(c0, c1), rng.uniform(r0, r1)
        if keep_out(c, r):
            continue
        gid = "grass" if k % grass_every else "flower"
        h = E.tile_hash(int(c * 10), int(r * 10), 7)
        sc.prop(sc.variant(gid, h), c, r, float(h % 360), scale=0.9 + 0.2 * ((h >> 5) % 100) / 100)


def knellwatch() -> None:
    """Knellwatch, camp 1 (15,15), under the march theme (chapter1.md §2)."""
    K.reset()
    sc = Scene()
    cx, cy = CAMP
    c0, r0, c1, r1 = cx - 16, cy - 18, cx + 17, cy + 15
    tiles = camp_tiles(c0, r0, c1, r1)
    review.setup_render((1920, 1080), 8)
    cam = review.game_camera(tuple(E.tile_to_blender(cx, cy - 1.0))[:2])

    def wall(c, r):
        return c0 <= c < c1 and r0 <= r < r1 and tiles[r - r0, c - c0] == 6
    for r in range(r0, r1):                                   # BuildWalls: pews (palisade_plains@march)
        for c in range(c0, c1):
            if not wall(c, r):
                continue
            spots = [(c, r)]
            for dc, dr in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                if wall(c + dc, r + dr):
                    spots += [(c + dc * 0.2, r + dr * 0.2), (c + dc * 0.4, r + dr * 0.4)]
            for i, (sx, sy) in enumerate(spots):
                h = E.tile_hash(c * 5 + i, r, 17)
                sc.prop(sc.variant("palisade_plains", h), sx, sy, float(h % 360), shadow=False)
    gate = sc.variant("palisade_gate_plains", 0)
    for gx in (cx - 1.5, cx + 0.5):
        sc.prop(gate, gx, cy - 5, MESH_YAW + 90.0)
    slots, props = camp_layout()
    hearth_yaw = FACE_CAM
    for typ, x, y, _bl, faces in props:
        name = sc.variant(f"camp_{typ}", E.tile_hash(round_to_tile(cx + x), round_to_tile(cy + y), 23))
        yaw = (math.degrees(math.atan2(-y, -x)) + MESH_YAW) if faces else FACE_CAM
        sc.prop(name, cx + x, cy + y, yaw)
        if typ == "campfire":
            hearth_yaw = yaw
    # U4 set pieces (proposals; implementation-plan.md §4.4): the hearth dressing on the hearth's tile and yaw, the
    # bell frame beyond the fire (door toward the hearth, >= 2 tiles from every slot), the Name Wall on the inner west
    # palisade between its torches, facing into the camp
    sc.prop(sc.variant("setpiece_hearth_dressing", 0), cx, cy, hearth_yaw)
    bf = (0.0, 3.9)
    assert min(math.hypot(bf[0] - sx, bf[1] - sy) for sx, sy in slots[:5]) >= 2.0
    sc.prop(sc.variant("setpiece_bell_frame", 0), cx + bf[0], cy + bf[1],
            math.degrees(math.atan2(-bf[1], -bf[0])) + MESH_YAW)
    sc.prop(sc.variant("setpiece_name_wall", 0), cx - 4.45, cy + 0.32, math.degrees(math.atan2(0.0, 1.0)) + MESH_YAW)
    party = [("SK_Hero_Warrior", (-0.03, -3.22), "se"), ("SK_Hero_Mage", (-0.53, -2.72), "se"),
             ("SK_Hero_Rogue", (0.46, -3.71), "front"), ("SK_Pet_Lanternwisp", (-0.21, -3.89), "se")]
    for asset, (dc, dr), facing in party:
        if asset in K.manifest()["assets"]:
            sc.char(asset, cx + dc, cy + dr, review.FACINGS[facing], clip="Idle", frame=0)
    heroes = [(dc, dr) for a, (dc, dr), _ in party if a.startswith("SK_Hero_")]
    camp1 = next(c for c in json.loads((paths.UNREAL_ROOT / "Data" / "maps.json").read_text())["maps"]
                 ["emerald_plains"]["camps"] if (c["col"], c["row"]) == CAMP)
    npcs = json.loads((paths.UNREAL_ROOT / "Data" / "npcs.json").read_text())["npcs"]
    for i, nid in enumerate(camp1["npcs"]):
        sp = (npcs.get(nid) or {}).get("spriteId")
        asset = next(iter(K.variant_names(f"npc_{sp}" if sp else f"npc_{nid}")), None)
        if asset is None:
            raise KeyError(f"camp NPC {nid}: no npc_{nid}@march / npc_{nid} asset")
        sx, sy = slots[i % len(slots)]
        near = min(heroes, key=lambda h: math.hypot(h[0] - sx, h[1] - sy))
        if math.hypot(near[0] - sx, near[1] - sy) <= HERO_NEAR_TILES:
            fc = face_towards(cx + sx, cy + sy, cx + near[0], cy + near[1])
        else:
            fx, fy = camp_npc_facing(i, slots)
            fc = face_towards(cx + sx, cy + sy, cx + sx + fx, cy + sy + fy)
        sc.char(asset, cx + sx, cy + sy, fc, clip=None, scenery=asset.startswith("SK_NPC_Blacksmith"))
    _decor(sc, [("tree", cx - 7.2, cy - 3.5), ("tree", cx + 7.4, cy - 2.6), ("tree_round", cx - 7.0, cy + 2.4),
                ("tree", cx + 6.8, cy + 3.8), ("tree_round", cx + 8.6, cy - 6.4), ("bush", cx - 6.6, cy - 6.2),
                ("bush", cx + 6.2, cy - 7.0), ("bush", cx - 8.4, cy + 0.6), ("rock", cx + 4.2, cy - 8.2),
                ("rock", cx - 4.6, cy - 8.6), ("mushroom_red", cx - 6.2, cy - 1.2), ("boulder", cx + 9.0, cy + 1.0)])
    _cover(sc, np.random.default_rng(7), 60, (cx - 11, cx + 11, cy - 12, cy + 9),
           lambda c, r: abs(c - cx) < 6 and -6 < r - cy < 6)
    K.ground(tiles, (c0, r0), ppt=64, shadows=sc.shadows, name="g_knellwatch")
    render_scene_march(sc, (cx, cy - 0.5), "vignette_knellwatch",
                       "KNELLWATCH (CAMP 1, 15,15)  MARCH THEME  W1 CAMERA  HEROES + LANTERNWISP AT THE GATE",
                       [((cx + 0.3, cy + 1.4), 820, 500, "KNELLWATCH: HEARTH, BELL FRAME, WELL, NAME WALL, NPC SLOTS 0-4",
                         "vignette_knellwatch_1080crop.png"),
                        ((cx + 0.24, cy - 2.93), 640, 420, "KNELLWATCH GATE: PEWS, GATE POSTS, HEROES + LANTERNWISP",
                         "vignette_knellwatch_gate_1080crop.png")])


def _field_tiles(c0, r0, c1, r1, dirt_fn) -> np.ndarray:
    tiles = np.zeros((r1 - r0, c1 - c0), np.int32)
    yy, xx = np.mgrid[r0:r1, c0:c1]
    tiles[dirt_fn(xx.astype(float), yy.astype(float))] = 1
    return tiles


RED_FIELD = (36.2, 74.2)             # between the Red Banner (40,70) and the Dead Ranks (30,80): one screen row


def red_field() -> None:
    """The Red Field: the Red Banner (40,70) and the Dead Ranks (30,80) at the W1 camera (chapter1.md §1, §12)."""
    K.reset()
    sc = Scene()
    fc_, fr_ = RED_FIELD
    c0, r0, c1, r1 = int(fc_) - 17, int(fr_) - 17, int(fc_) + 18, int(fr_) + 18
    noise = lambda x, y: np.sin(x * 1.3 + y * 0.7) * 0.7 + np.cos(y * 1.1 - x * 0.5) * 0.6  # noqa: E731
    tiles = _field_tiles(c0, r0, c1, r1, lambda x, y: (np.hypot(x - 32.4, y - 78.4) + noise(x, y) < 3.6)
                         | (np.hypot(x - 40.0, y - 70.0) + noise(x, y) < 2.2)
                         | ((np.abs((x - y) - (-40.0)) < 1.0) & (x > 33) & (x < 40)))    # the trampled line between
    review.setup_render((1920, 1080), 8)
    cam = review.game_camera(tuple(E.tile_to_blender(fc_, fr_))[:2])
    # the Red Banner on its mound of shields, every shield toward Knellwatch (15,15)
    sc.prop(sc.variant("decor_war_banner", 0), 40.0, 70.0, FACE_CAM)
    # helmets on spears along the approach (set piece setpiece_helmet_stake)
    for k, (c, r) in enumerate(((37.6, 73.4), (35.9, 75.6), (38.9, 75.9))):
        sc.prop(sc.variant("setpiece_helmet_stake", k), c, r, FACE_CAM + (k - 1) * 25.0)
    # the Dead Ranks (the near edge of the spawn area round (30,80)): the shield wall that never broke, facing the
    # way the heroes come (from the banner's side)
    hero_at = (36.6, 74.0)
    wall_line = [(33.0, 77.2), (33.9, 78.2), (32.1, 76.3)]
    for c, r in wall_line:
        sc.char("SK_Mon_RisenShieldbearer", c, r, face_towards(c, r, *hero_at), clip="Idle", frame=0)
    sc.char("SK_Mon_DeadStandardBearer", 32.0, 78.4, face_towards(32.0, 78.4, *hero_at), clip="Idle", frame=0)
    for (c, r, fr) in ((30.9, 77.3, 6), (33.2, 79.6, 18)):
        sc.char("SK_Mon_RisenFootman", c, r, face_towards(c, r, *hero_at), clip="Walk", frame=fr)
    # the heroes between the banner and the wall
    sc.char("SK_Hero_Warrior", *hero_at, face_towards(*hero_at, 33.0, 77.2), clip="Attack01", frame=8)
    sc.char("SK_Hero_Rogue", 37.8, 73.4, face_towards(37.8, 73.4, 33.0, 77.2), clip="Idle", frame=0)
    # gravegnawers worrying the dead below the banner mound
    for (c, r, fr) in ((38.6, 70.7, 0), (38.4, 72.0, 14)):        # screen right of the mound, on its near side
        sc.char("SK_Mon_Gravegnawer", c, r, face_towards(c, r, 40.0, 70.0), clip="Idle", frame=fr)
    _decor(sc, [("tree", 27.8, 71.6), ("tree", 44.2, 77.4), ("tree", 33.4, 67.6), ("tree_round", 46.0, 72.0),
                ("bush", 34.0, 80.4), ("bush", 43.0, 68.2), ("bush", 28.2, 75.0), ("rock", 36.4, 79.2),
                ("rock", 39.6, 66.8), ("rock", 29.6, 83.0), ("boulder", 45.6, 81.4), ("mushroom_red", 37.8, 82.4),
                ("rock", 42.6, 75.2)])
    _cover(sc, np.random.default_rng(13), 90, (fc_ - 12, fc_ + 12, fr_ - 12, fr_ + 12),
           lambda c, r: (math.hypot(c - 32.4, r - 78.0) < 3.0 or math.hypot(c - 40.0, r - 70.0) < 1.6
                         or math.hypot(c - hero_at[0], r - hero_at[1]) < 1.2
                         or math.hypot(c - 38.5, r - 71.3) < 1.2), grass_every=4)
    K.ground(tiles, (c0, r0), ppt=64, shadows=sc.shadows, name="g_red_field")
    render_scene_march(sc, (fc_, fr_), "vignette_red_field",
                       "THE RED FIELD: THE RED BANNER (40,70) + THE DEAD RANKS (30,80)  MARCH THEME  W1 CAMERA",
                       [((39.2, 71.8), 820, 500, "THE RED BANNER: MOUND OF SHIELDS, HELMET STAKES, GRAVEGNAWERS",
                         "vignette_red_field_banner_1080crop.png"),
                        ((34.2, 76.6), 820, 520, "THE DEAD RANKS: SHIELD WALL, STANDARD-BEARER, FOOTMEN VS WARRIOR",
                         "vignette_red_field_ranks_1080crop.png")])


ASHWHEAT = (40, 52)
BRIDGE = (38, 48)


def ashwheat() -> None:
    """Ashwheat (40,52) r8 and its burned bridge (38,48) at the W1 camera (chapter1.md §1, M3)."""
    K.reset()
    sc = Scene()
    ac, ar = ASHWHEAT
    fc_, fr_ = 39.0, 50.0
    c0, r0, c1, r1 = int(fc_) - 17, int(fr_) - 17, int(fc_) + 18, int(fr_) + 18
    noise = lambda x, y: np.sin(x * 1.6 + y * 0.5) * 0.6 + np.cos(y * 1.2 - x * 0.7) * 0.5  # noqa: E731
    tiles = _field_tiles(c0, r0, c1, r1, lambda x, y: (np.hypot(x - ac, (y - ar) * 1.1) + noise(x, y) < 5.2)
                         | ((np.abs((x - y) - (BRIDGE[0] - BRIDGE[1])) < 1.0) & (y < BRIDGE[1] + 2) & (y > BRIDGE[1] - 7)))
    review.setup_render((1920, 1080), 8)
    cam = review.game_camera(tuple(E.tile_to_blender(fc_, fr_))[:2])
    # the burned bridge over its dry ditch (set pieces: the bridge and its ground layer on one tile and yaw)
    byaw = FACE_CAM + 45.0
    sc.prop(sc.variant("setpiece_burned_bridge_ditch", 0), *BRIDGE, byaw, shadow=False)
    sc.prop(sc.variant("setpiece_burned_bridge", 0), *BRIDGE, byaw)
    # the hamlet: cottages round the green, doors barred from inside, candles in every window
    for k, (dc, dr, yaw) in enumerate(((-2.6, 1.0, FACE_CAM + 20.0), (2.2, 2.6, FACE_CAM - 10.0),
                                       (3.4, -1.2, FACE_CAM - 40.0))):
        sc.prop(sc.variant("setpiece_ashwheat_cottage", k), ac + dc, ar + dr, yaw)
    # the mage and the Lanternwisp coming down the near bank; two gravegnawers on the far side of the ditch
    mage = (39.0, 50.3)
    sc.char("SK_Hero_Mage", *mage, face_towards(*mage, *BRIDGE), clip="Walk", frame=10)
    sc.char("SK_Pet_Lanternwisp", 39.9, 50.1, face_towards(39.9, 50.1, *BRIDGE), clip="Idle", frame=0)
    for (c, r, fr) in ((35.8, 48.5, 0), (36.7, 47.3, 20)):
        sc.char("SK_Mon_Gravegnawer", c, r, face_towards(c, r, *mage), clip="Idle", frame=fr)
    _decor(sc, [("tree_round", 44.6, 50.2), ("tree_round", 43.4, 55.8), ("tree_round", 35.6, 53.6),
                ("tree", 33.2, 46.6), ("tree", 45.8, 45.6), ("bush", 41.8, 47.6), ("bush", 34.6, 50.2),
                ("rock", 40.6, 46.4), ("rock", 36.8, 55.6), ("mushroom_red", 42.6, 49.0), ("boulder", 31.8, 52.2)])
    _cover(sc, np.random.default_rng(29), 80, (fc_ - 12, fc_ + 12, fr_ - 12, fr_ + 12),
           lambda c, r: (math.hypot(c - ac, (r - ar) * 1.1) < 4.6 or math.hypot(c - BRIDGE[0], r - BRIDGE[1]) < 3.2
                         or math.hypot(c - mage[0], r - mage[1]) < 1.0), grass_every=2)
    K.ground(tiles, (c0, r0), ppt=64, shadows=sc.shadows, name="g_ashwheat")
    render_scene_march(sc, (fc_, fr_), "vignette_ashwheat",
                       "ASHWHEAT (40,52) + THE BURNED BRIDGE (38,48)  MARCH THEME  W1 CAMERA",
                       [((40.4, 52.0), 820, 500, "ASHWHEAT: COTTAGES (CANDLES, DOORS BARRED), CHARRED ORCHARD",
                         "vignette_ashwheat_1080crop.png"),
                        ((37.6, 48.8), 820, 500, "THE BURNED BRIDGE: DRY DITCH, MAGE + LANTERNWISP, GRAVEGNAWERS",
                         "vignette_ashwheat_bridge_1080crop.png")])


def main(argv: list[str]) -> int:
    which = [a for a in argv if not a.startswith("--")]
    march = "--march" in argv or any(w in MARCH_SCENES for w in which)
    if march:
        K.set_theme("march")
        OUT_MARCH.mkdir(parents=True, exist_ok=True)
        for w in [w for w in which if w in MARCH_SCENES] or list(MARCH_SCENES):
            globals()[w]()
        return 0
    OUT.mkdir(parents=True, exist_ok=True)
    which = which or ["camp", "goblin"]
    if "camp" in which:
        camp()
    if "goblin" in which:
        goblin_camp()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

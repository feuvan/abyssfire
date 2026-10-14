"""Chapter-1 coherence vignettes: the camp and the goblin camp composed from the shipped environment kit with the
shipped characters placed — ``Art/Previews/ch1_coherence/vignette_{camp,goblin_camp}*.png``.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/coherence/vignettes.py [camp] [goblin]

Everything is re-imported from ``Art/Export`` (``cohkit``) and placed the way the runtime places it:

* **Camp** (``emerald_plains`` camp 0 at (15,15), DECISIONS W11): the MapGen camp stamp (11 × 11 camp ground, palisade
  on the gate row except the 2-tile gate at x −1 / 0 and on both sides up to row +3, ``MapGen.cpp``), the palisade
  stakes exactly as ``AAbyssZoneActor::BuildWalls`` (centre + 0.2 / 0.4 toward every camp-wall neighbour, variant and
  yaw from ``tileHash(c·5+i, r, 17)``), the gate posts at x −1.5 / +0.5, every ``abyss/world/CampLayout.h`` prop
  (tents / well / barrels / crates facing the fire, the rest facing the camera), the four camp NPCs on slots 0–3
  (blacksmith with his anvil stump, merchant, elder, stash keeper), the three heroes and the ley sprite inside the gate,
  oaks / fruit trees / bushes / grass outside.
* **Goblin camp** at (40,52) (spec §6.9, R7; the ``q_explore_goblin_camp`` objective and the ``cs_ep_whisper`` focus):
  the two hide huts, the fire pit, bone totems, a spear rack and crates on trampled dirt, goblins round the fire, the
  shaman at the pit, the chief by the big hut, a red-cap hunt leader; the rogue and the ley sprite sneaking in.

Each scene: ``vignette_<name>.png`` = the whole frame as a native 640 × 360 render of the W1 camera (ink and bloom at
360p), ``vignette_<name>_1080crop*.png`` = native 1080p 1:1 crops (truecolour tiles, ``pngio.write_png_native``).
"""
from __future__ import annotations

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
        names = sorted(n for n, e in K.manifest()["assets"].items() if gid in (e.get("gameIds") or []))
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
    layout v2, DECISIONS W11), never a copy: slots [(x, y)], props [(type, x, y, facesCentre, blocking, ownerSlot)]."""
    src = CAMP_LAYOUT_H.read_text()
    num = r"(-?[0-9]+(?:\.[0-9]*)?)"
    body = src.split("kCampNpcSlots{{", 1)[1].split("}};", 1)[0]
    slots = [(float(a), float(b)) for a, b in re.findall(r"\{\s*" + num + r",\s*" + num + r"\s*\}", body)]
    body = src.split("kCampProps{{", 1)[1].split("}};", 1)[0]
    props = [(t, float(x), float(y), fc == "true", bl == "true", int(o)) for t, x, y, fc, bl, o in re.findall(
        r'\{"([a-z_]+)",\s*' + num + r",\s*" + num + r",\s*(true|false),\s*(true|false),\s*(-?[0-9]+)\}", body)]
    if len(slots) < 4 or len(props) < 10:
        raise RuntimeError(f"CampLayout.h parse: {len(slots)} slots, {len(props)} props")
    return slots, props


def camp_npc_facing(slot: int, slots: list, props: list) -> tuple[float, float]:
    """``abyss::camp::CampNpcFacing``: a tent owner looks out of its door (the tent's front, toward the centre), every
    other slot looks at the campfire (tile-space unit vector)."""
    for (_t, x, y, _fc, _bl, owner) in props:
        if owner >= 0 and owner == slot:
            n = math.hypot(x, y)
            return (-x / n, -y / n)
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
    # centre, tents / well / barrels / crates turned to the camp centre, the rest to the camera, variant by
    # TileHash(RoundToTile(centre), 23))
    slots, props = camp_layout()
    for typ, x, y, faces, _bl, _own in props:
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
            fx, fy = camp_npc_facing(i, slots, props)
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
                 [((cx + 0.5, cy + 1.2), 760, 470, "CAMP: FIRE, TENTS, NPC SLOTS 0-3", "vignette_camp_1080crop.png"),
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


def main(argv: list[str]) -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    which = [a for a in argv if not a.startswith("--")] or ["camp", "goblin"]
    if "camp" in which:
        camp()
    if "goblin" in which:
        goblin_camp()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

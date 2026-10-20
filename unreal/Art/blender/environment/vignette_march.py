"""The march vignette: Knellwatch and the Red Field's edge composed from the shipped march meshes on the march terrain at
the W1 game camera (dark remake A10; chapter1.md §2, §15). The plains vignette (``vignette.py``) with the presentation
theme ``march``: every placement resolves ``<id>@march`` first (``kit.theme.resolve``), exactly as U1 will.

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/vignette_march.py [--check]

Everything is re-imported from ``Art/Export`` (FBX + shipped palettes + manifest ink), placed like ``AAbyssZoneActor``
places it (the plains map of ``vignette.build_map``: camp + palisade ring + 2-tile gate, a dirt road, a pond, a wall
ridge, a flagstone patch), on the composited ``terrain.march`` set. The camp: the hearth at the centre, the bell frame
in a tent slot, the well, Knellwatch's banners, braziers, sacks and crates, the pew palisade and its gate posts, the
Captain (``npc_quest_elder@march``) by the hearth. Outside: dead oaks, charred orchard trees, briars, ash grass and
poppies, ash-caps, broken stone, an ash boulder, the outcrop ridge, reeds in the black pond, the three story
decorations, and on the road the warrior facing three march monsters (a risen footman, a gravegnawer, a Red-Oath
zealot) — the readability check of the whole palette at once.

Writes ``Art/Previews/env_vignette_march/``: ``vignette.png`` (native 640 × 360), ``vignette_road_1080crop.png`` (the
hero and the monsters on the mud road), ``vignette_camp_1080crop.png`` (hearth, bell frame, well, banners, palisade),
``vignette_field_1080crop.png`` (pond, reeds, story decor, trees, ridge) — native 1080p, truecolour, scale 1.
"""
from __future__ import annotations

import math
import sys
from pathlib import Path
from types import SimpleNamespace

HERE = Path(__file__).resolve().parent
BLENDER = HERE.parent
for _p in (HERE, BLENDER, BLENDER / "coherence"):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import envkit_march as EM  # noqa: E402
import envkit as E  # noqa: E402
import terrain as T  # noqa: E402
import terrain_march as TM  # noqa: E402
import vignette as V  # noqa: E402
import bpy  # noqa: E402
import numpy as np  # noqa: E402

from kit import outline, paths, pngio, review, scene, theme  # noqa: E402

OUT = paths.preview_root() / "env_vignette_march"
THEME = "march"


def themed_variant(shelf: V.Shelf, game_id: str, h: int) -> str:
    """The runtime pick under the presentation theme: the assets listing ``<id>@march`` (else ``<id>``), sorted by
    name, index h % n (``vignette.variant`` on the themed id)."""
    tid = theme.themed(game_id, THEME)
    if any(tid in e.get("gameIds", []) for e in shelf.man["assets"].values()):
        return V.variant(shelf, tid, h)
    return V.variant(shelf, game_id, h)


def dress(shelf: V.Shelf, m: np.ndarray) -> None:
    tile = V.tile
    for r in range(V.R0, V.R1):
        for c in range(V.C0, V.C1):
            t = tile(m, c, r)
            if t == 4:
                h = E.tile_hash(c, r, 3)
                shelf.put(themed_variant(shelf, "outcrop_plains", h), c, r, float((h >> 8) % 360))
            elif t == 6:
                spots = [(c, r)]
                for dc, dr in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    if tile(m, c + dc, r + dr) == 6:
                        spots += [(c + dc * 0.2, r + dr * 0.2), (c + dc * 0.4, r + dr * 0.4)]
                for i, (sc, sr) in enumerate(spots):
                    h = E.tile_hash(c * 5 + i, r, 17)
                    shelf.put(themed_variant(shelf, "palisade_plains", h), sc, sr, float(h % 360))
            elif t == 3 and E.tile_hash(c, r, 13) % 3 == 0:
                h = E.tile_hash(c, r, 13)
                shelf.put(themed_variant(shelf, "lily_pad_plains", h), c + ((h >> 4) % 100 / 100 - 0.5) * 0.4,
                          r + ((h >> 9) % 100 / 100 - 0.5) * 0.4, float(h % 360), z=0.005)
    gate = themed_variant(shelf, "palisade_gate_plains", 0)
    for c in (8.5, 10.5):
        shelf.put(gate, c, 11, V.MESH_YAW + 90.0)
    centre = (10.5, 14.0)

    def facing(c, r):
        return math.degrees(math.atan2(centre[1] - r, centre[0] - c)) + V.MESH_YAW
    face_cam = V.CAMERA_YAW + 180.0 + V.MESH_YAW

    def pick(gid):
        return themed_variant(shelf, gid, 0)
    shelf.put(pick("camp_campfire"), 10.5, 14.0, face_cam)
    # the hearth's stools and blanket line (set piece setpiece_hearth_dressing: same tile and yaw, U4 places it)
    if "SM_Prop_March_HearthDressing" in shelf.man["assets"]:
        shelf.put("SM_Prop_March_HearthDressing", 10.5, 14.0, face_cam)
    shelf.put(pick("camp_tent"), 8.9, 15.7, V.clamp_to_camera(facing(8.9, 15.7)))
    shelf.put(pick("camp_well"), 12.8, 12.6, facing(12.8, 12.6))
    shelf.put(pick("camp_banner"), 8.4, 12.4, face_cam)
    shelf.put(pick("camp_banner"), 11.4, 12.2, face_cam)
    for c, r in ((8.0, 10.3), (11.0, 10.3), (13.3, 16.2)):
        shelf.put(pick("camp_torch"), c, r, face_cam)
    shelf.put(pick("camp_barrel"), 13.4, 14.4, facing(13.4, 14.4))
    shelf.put(pick("camp_barrel"), 13.1, 15.2, facing(13.1, 15.2) + 40)
    shelf.put(pick("camp_crate"), 8.0, 13.4, facing(8.0, 13.4))
    scatter = [
        ("tree", 4.6, 9.2), ("tree", 3.4, 12.6), ("tree", 16.6, 12.4), ("tree", 20.6, 9.8), ("tree_round", 15.2, 9.1),
        ("tree_round", 4.2, 4.0), ("tree", 2.0, 6.8),
        ("bush", 5.6, 7.0), ("bush", 12.0, 8.6), ("bush", 17.4, 7.2), ("bush", 2.6, 2.4), ("bush", 15.8, 14.6),
        ("boulder", 10.2, 0.8), ("rock", 6.2, 3.0), ("rock", 11.2, 7.4), ("rock", 16.8, 1.8), ("rock", 3.8, 8.0),
        ("mushroom_red", 5.4, 10.6), ("mushroom_red", 13.6, 8.2),
    ]
    for gid, c, r in scatter:
        h = E.tile_hash(int(c * 10), int(r * 10), 5)
        shelf.put(themed_variant(shelf, gid, h), c, r, float(h % 360),
                  scale=0.92 + 0.16 * ((h >> 11) % 100) / 100)
    rng = scene.rng("vignette_march", "groundcover")
    for k in range(70):
        c, r = rng.uniform(0, 20), rng.uniform(-3, 17)
        if any(tile(m, int(round(c + dc)), int(round(r + dr))) != 0 for dc in (-0.45, 0.0, 0.45)
               for dr in (-0.45, 0.0, 0.45)):
            continue
        gid = "grass" if k % 3 else "flower"
        h = E.tile_hash(int(c * 10), int(r * 10), 7)
        shelf.put(themed_variant(shelf, gid, h), c, r, float(h % 360), scale=0.9 + 0.2 * ((h >> 5) % 100) / 100)
    # the three story decorations (face the camera), on dry ground in the frame
    shelf.put(themed_variant(shelf, "decor_ruins", 0), 11.4, 2.6, face_cam)
    shelf.put(themed_variant(shelf, "decor_war_banner", 0), 2.6, 0.2, face_cam)
    shelf.put(themed_variant(shelf, "decor_broken_altar", 0), 17.6, 15.6, face_cam)
    # the reeds take the lily pads' water slot (``vignette.check_dry`` excepts pads by name)
    placed = shelf.placed
    shelf.placed = [p for p in placed if "Reeds" not in p[0]]
    try:
        V.check_dry(shelf, m)
    finally:
        shelf.placed = placed


def figures(cam=None) -> list:
    """The warrior on the road facing three march monsters, the Captain by the hearth (re-imported, posed Idle)."""
    import cohkit as K
    out = []
    # an arc in front of the warrior, apart on screen (u = col − row across, v = col + row up the frame)
    for asset, c, r, yaw in (("SK_Mon_RisenFootman", 8.9, 5.7, 150.0), ("SK_Mon_Gravegnawer", 10.5, 6.1, 200.0),
                             ("SK_Mon_OathZealot", 8.8, 4.2, 170.0), ("SK_NPC_QuestElder_March", 11.6, 13.2, -60.0)):
        if asset not in K.manifest().get("assets", {}):
            print(f"  [vignette] {asset} not shipped: skipped")
            continue
        f = K.import_character(asset, E.tile_to_blender(c, r), yaw, cam=cam)
        for mm in f.all():
            mm["af_px1080"] = mm.get("af_px1080", f.px)
        out.append(f)
    return out


def render_at(meshes, focus, res):
    review.setup_render(res, 8)
    cam = review.game_camera((focus.x, focus.y))
    k = res[1] / 1080.0
    for o in meshes:
        if "af_outline_width" not in o:
            continue
        cls = o.get("af_outline_class", "decor")
        if "af_px1080" in o:
            px = float(o["af_px1080"])
        elif cls in ("hero", "weapon"):
            px = outline.SCREEN_PX_1080[cls]
        else:
            px = E.ink_px(o.get("af_family", ""), cls)
        review.set_outline_px([o], px * k)
    img = review.render_array()
    saved = {}
    for o in meshes:
        if o.type == "MESH":
            saved.setdefault(o.data.name, (o.data, list(o.data.materials)))
    gmesh = bpy.data.objects["vignette_ground"].data
    gmats = list(gmesh.materials)
    gmesh.materials[0] = V._black_emission()
    try:
        em = review.emissive_pass(meshes)
    finally:
        gmesh.materials[0] = gmats[0]
        for data, mats in saved.values():
            for i, mt in enumerate(mats):
                data.materials[i] = mt
    return cam, review.bloom(img, em, res[1])


def main(argv: list[str]) -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    scene.reset()
    m = V.build_map()
    shelf = V.Shelf()
    dress(shelf, m)
    if "--check" in argv:
        print("  dressing OK:", len(shelf.placed), "placed")
        return 0
    # the ground: the vignette's composite on the march terrain set
    saved_t = V.T
    V.T = SimpleNamespace(load_textures=TM.load_textures, composite=TM.composite, Layers=T.Layers)
    try:
        V.ground(shelf, m)
    finally:
        V.T = saved_t
    w = E.import_warrior(at=tuple(E.tile_to_blender(8.8, 7.0))[:2] + (0.0,), yaw_deg=-60.0)
    meshes = [o for *_, o in shelf.placed]
    if w is not None:
        meshes += w[1]
    figs = figures()
    for f in figs:
        meshes += f.all()
    focus = E.tile_to_blender(10.2, 7.6)
    cam_s, small = render_at(meshes, focus, (640, 360))

    # labels in a reserved band above the render (env art review: the headers overprinted the props' top edge)
    E.write_banded(OUT / "vignette.png", small, "MARCH VIGNETTE (KNELLWATCH + THE FIELD)  W1 CAMERA  NATIVE 640X360  "
                   "NO ZONE GRADE")
    cam, full = render_at(meshes, focus, (1920, 1080))

    def crop(col, row, w_px, h_px, label, name):
        x, y = review.project_px(cam, E.tile_to_blender(col, row, 0.5), (1920, 1080))
        x0 = int(min(max(x - w_px / 2, 0), 1920 - w_px))
        y0 = int(min(max(y - h_px / 2, 0), 1080 - h_px))
        c = full[y0:y0 + h_px, x0:x0 + w_px].copy()
        out = E.write_banded(OUT / name, c, label)
        for p, size in out:
            print(f"  {p} {size // 1024} KB")
    crop(9.0, 5.8, 760, 460, "THE ROAD: WARRIOR VS RISEN FOOTMAN, GRAVEGNAWER, RED-OATH ZEALOT  NATIVE 1080P 1:1",
         "vignette_road_1080crop.png")
    crop(11.0, 14.0, 760, 460, "KNELLWATCH: HEARTH, BELL FRAME, WELL, BANNERS, PEW PALISADE  NATIVE 1080P 1:1",
         "vignette_camp_1080crop.png")
    crop(12.4, 3.6, 760, 460, "THE FIELD: BLACK POND + REEDS, BROKEN BELFRY, DEAD TREES, ASH RIDGE  NATIVE 1080P 1:1",
         "vignette_field_1080crop.png")
    V.hero_check(full, cam, w)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

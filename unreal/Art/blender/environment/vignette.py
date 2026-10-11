"""Emerald Plains vignette: the shipped environment family composed on the tile grid at the W1 game camera, the
warrior for scale (spec art-inventory-ch1.md §6; DECISIONS W1, R1, W5).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/vignette.py

Everything comes from ``Art/Export``: every static mesh is **re-imported from its FBX** (what UE receives) with the
toon / outline preview materials read from the shipped palette PNGs, its ink class and family from the manifest
entry, and placed the way the runtime places it (``AAbyssZoneActor``: outcrop variant = tileHash(c, r, 3) % 6,
palisade stakes at the centre and 0.2 / 0.4 toward every camp-wall neighbour, camp props facing the camp centre,
lily pads on water tiles). The ground is the shipped terrain texture set composited with the exact transition
rules (``terrain.composite``) + the web contact shadows under the props. The hero is ``SK_Hero_Warrior`` with its
Idle clip.

Writes ``Art/Previews/env_vignette/``: ``vignette.png`` (the whole 1920×1080 frame at half size), ``vignette_1080crop.png``
(native 1080p pixels round the hero and the camp gate), ``vignette_pond.png`` (the pond, ridge and grove corner,
native pixels) and ``vignette_footprints.png`` (W5: the blocking footprints of every placed prop painted on the
tile grid from the manifest ``footprintTiles`` / ``blocking``).
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import envkit as E  # noqa: E402
import terrain as T  # noqa: E402
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector  # noqa: E402

from kit import outline, paths, pngio, review, scene  # noqa: E402

OUT = paths.preview_root() / "env_vignette"
PPT = 80                                  # ground texture pixels per tile (≈ 120 screen px per tile at 1080p)
C0, R0, C1, R1 = -3, -6, 24, 21           # composited ground window (tiles, inclusive-exclusive)


# ── map ─────────────────────────────────────────────────────────────────────────────────────────────────
def build_map() -> np.ndarray:
    """Tile ids over [R0, R1) × [C0, C1): grass, a camp (camp ground + palisade ring with a 2-tile gate), the dirt
    path leaving the gate, a pond, a moss-boulder wall ridge and a stone patch."""
    m = np.zeros((R1 - R0, C1 - C0), np.int32)

    def put(c, r, v):
        if C0 <= c < C1 and R0 <= r < R1:
            m[r - R0, c - C0] = v
    # camp: ground cols 7..14, rows 11..17, palisade ring, gate on the near (row 11) side at cols 9..10
    for c in range(7, 15):
        for r in range(11, 18):
            put(c, r, 5)
    for c in range(7, 15):
        for r in (11, 17):
            put(c, r, 6)
    for r in range(11, 18):
        put(7, r, 6)
        put(14, r, 6)
    for c in (9, 10):
        put(c, 11, 5)
    # path from the gate toward the viewer, wandering
    path = [(9, 10), (10, 10), (9, 9), (10, 9), (9, 8), (8, 7), (9, 7), (8, 6), (8, 5), (7, 4), (8, 4), (7, 3),
            (7, 2), (6, 1), (7, 1), (6, 0), (6, -1), (5, -2), (6, -2)]
    for c, r in path:
        put(c, r, 1)
    # pond (screen left)
    for c in range(13, 17):
        for r in range(3, 7):
            put(c, r, 3)
    for c, r in ((12, 4), (12, 5), (17, 4), (14, 2), (15, 2), (14, 7)):
        put(c, r, 3)
    # wall ridge along the far left (moss boulders + hedges), a stone patch near it
    for r in range(-2, 11):
        put(19, r, 4)
    for c in (18, 17):
        put(c, 10, 4)
    put(18, 9, 4)
    for c in range(16, 19):
        for r in range(-1, 1):
            put(c, r, 2)
    return m


def tile(m, c, r):
    if C0 <= c < C1 and R0 <= r < R1:
        return int(m[r - R0, c - C0])
    return 0


# ── assets from the export (FBX + manifest) ─────────────────────────────────────────────────────────────
class Shelf:
    """Shipped static meshes, imported once each and instanced (linked duplicates)."""

    def __init__(self):
        self.man = json.loads(paths.manifest_path().read_text())
        self.src: dict[str, bpy.types.Object] = {}
        self.mats: dict[str, tuple] = {}
        self.placed: list[tuple] = []          # (asset, col, row, yaw, obj)

    def entry(self, name: str) -> dict:
        return self.man["assets"][name]

    def _materials(self, family: str):
        if family not in self.mats:
            self.mats[family] = E.toon_material_files(family)
        return self.mats[family]

    def source(self, name: str):
        if name in self.src:
            return self.src[name]
        e = self.entry(name)
        before = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(paths.export_root() / e["fbx"]))
        new = [o for o in bpy.data.objects if o not in before]
        mesh = next(o for o in new if o.type == "MESH")
        for o in new:
            if o is not mesh:
                bpy.data.objects.remove(o)
        mesh.parent = None
        mesh.location = (0, 0, 0)
        mesh.rotation_euler = (0, 0, 0)
        mesh.scale = (1, 1, 1)
        slots = e["materialSlots"]
        fam = slots[0]["palette"]
        mt, mo = self._materials(fam)
        E._set_slots(mesh, mt, mo)
        if len(slots) > 1:
            mesh["af_outline_width"] = slots[1]["widthCm"] / 100.0
            mesh["af_outline_class"] = slots[1]["class"]
        else:
            while len(mesh.data.materials) > 1:
                mesh.data.materials.pop()
        mesh["af_family"] = fam
        mesh.hide_render = True
        self.src[name] = mesh
        return mesh

    def put(self, name: str, col: float, row: float, yaw_ue: float = 0.0, z: float = 0.0, scale: float = 1.0):
        src = self.source(name)
        o = src.copy()
        scene.link(o)
        o.hide_render = False
        o.location = E.tile_to_blender(col, row, z)
        o.rotation_euler = (0, 0, E.ue_yaw_to_blender(yaw_ue))
        o.scale = (scale, scale, scale)
        for k in ("af_outline_width", "af_outline_class", "af_family"):
            if k in src:
                o[k] = src[k]
        self.placed.append((name, col, row, yaw_ue, o))
        return o


def variant(shelf: Shelf, game_id: str, h: int) -> str:
    """The runtime pick: the assets listing ``game_id`` in manifest key order, index h % n."""
    names = sorted(n for n, e in shelf.man["assets"].items() if game_id in e.get("gameIds", []))
    return names[h % len(names)]


# ── dressing ────────────────────────────────────────────────────────────────────────────────────────────
CAMERA_YAW = 45.0
MESH_YAW = -90.0          # AAbyssZoneActor::MeshYawOffsetDeg (mesh forward +Y → actor forward +X)


def dress(shelf: Shelf, m: np.ndarray) -> None:
    # walls: outcrops on wall tiles (variant + yaw from tileHash), palisade stakes on camp-wall tiles
    for r in range(R0, R1):
        for c in range(C0, C1):
            t = tile(m, c, r)
            if t == 4:
                h = E.tile_hash(c, r, 3)
                shelf.put(variant(shelf, "outcrop_plains", h), c, r, float((h >> 8) % 360))
            elif t == 6:
                spots = [(c, r)]
                for dc, dr in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    if tile(m, c + dc, r + dr) == 6:
                        spots += [(c + dc * 0.2, r + dr * 0.2), (c + dc * 0.4, r + dr * 0.4)]
                for i, (sc, sr) in enumerate(spots):
                    h = E.tile_hash(c * 5 + i, r, 17)
                    shelf.put(variant(shelf, "palisade_plains", h), sc, sr, float(h % 360))
            elif t == 3 and E.tile_hash(c, r, 13) % 3 == 0:
                h = E.tile_hash(c, r, 13)
                shelf.put(variant(shelf, "lily_pad_plains", h), c + ((h >> 4) % 100 / 100 - 0.5) * 0.4,
                          r + ((h >> 9) % 100 / 100 - 0.5) * 0.4, float(h % 360), z=0.005)
    # gate posts either side of the 2-tile gate (BuildWalls: camp.x - 1.5 / + 0.5 on the gate row)
    for c in (8.5, 10.5):
        shelf.put("SM_Palisade_Plains_GatePost", c, 11, MESH_YAW + 90.0)
    # camp kit (faces the camp centre: tent, well, barrel, crate; the rest face the camera)
    centre = (10.5, 14.0)

    def facing(c, r):
        return math.degrees(math.atan2(centre[1] - r, centre[0] - c)) + MESH_YAW
    face_cam = CAMERA_YAW + 180.0 + MESH_YAW
    shelf.put("SM_Prop_Camp_Campfire", 10.5, 14.0, face_cam)
    shelf.put("SM_Prop_Camp_Tent", 8.6, 15.6, facing(8.6, 15.6))
    shelf.put("SM_Prop_Camp_Tent", 12.6, 15.8, facing(12.6, 15.8))
    shelf.put("SM_Prop_Camp_Well", 12.8, 12.6, facing(12.8, 12.6))
    shelf.put("SM_Prop_Camp_Banner", 8.4, 12.4, face_cam)
    shelf.put("SM_Prop_Camp_Banner", 11.4, 12.2, face_cam)
    shelf.put("SM_Prop_Camp_Torch", 8.0, 10.3, face_cam)
    shelf.put("SM_Prop_Camp_Torch", 11.0, 10.3, face_cam)
    shelf.put("SM_Prop_Camp_Torch", 9.4, 13.6, face_cam)
    shelf.put("SM_Prop_Camp_Barrel", 13.4, 14.4, facing(13.4, 14.4))
    shelf.put("SM_Prop_Camp_Barrel", 13.2, 15.0, facing(13.2, 15.0) + 40)
    shelf.put("SM_Prop_Camp_Crate", 8.0, 13.6, facing(8.0, 13.6))
    shelf.put("SM_Prop_Camp_Crate", 8.1, 14.4, facing(8.1, 14.4) + 25)
    # grove + open scatter (decor game ids, jitter like world-map-nav §15.5)
    scatter = [
        ("tree", 4.6, 9.2), ("tree", 3.4, 12.6), ("tree", 16.6, 12.4), ("tree", 20.6, 9.8), ("tree_round", 15.2, 9.1),
        ("tree_round", 4.2, 4.0), ("tree", 2.0, 6.8),
        ("bush", 5.6, 7.0), ("bush", 12.0, 8.6), ("bush", 17.4, 7.2), ("bush", 2.6, 2.4), ("bush", 15.8, 14.6),
        ("boulder", 11.6, 2.0), ("rock", 6.2, 3.0), ("rock", 11.2, 7.4), ("rock", 16.8, 1.8), ("rock", 3.8, 8.0),
        ("mushroom_red", 5.4, 10.6), ("mushroom_red", 13.6, 8.2),
    ]
    for gid, c, r in scatter:
        h = E.tile_hash(int(c * 10), int(r * 10), 5)
        shelf.put(variant(shelf, gid, h), c, r, float(h % 360), scale=0.92 + 0.16 * ((h >> 11) % 100) / 100)
    rng = scene.rng("vignette", "groundcover")
    for k in range(70):
        c, r = rng.uniform(0, 20), rng.uniform(-3, 17)
        t = tile(m, int(round(c)), int(round(r)))
        if t not in (0,):
            continue
        gid = "grass" if k % 3 else "flower"
        h = E.tile_hash(int(c * 10), int(r * 10), 7)
        shelf.put(variant(shelf, gid, h), c, r, float(h % 360), scale=0.9 + 0.2 * ((h >> 5) % 100) / 100)
    # story / lore / interactive accents along the walk
    shelf.put("SM_Prop_Lore_AncientTablet", 6.4, 6.0, CAMERA_YAW + 180.0 + MESH_YAW + 20)
    shelf.put("SM_Prop_Story_ElvenRuins", 17.6, 3.4, CAMERA_YAW + 180.0 + MESH_YAW)


# ── ground ──────────────────────────────────────────────────────────────────────────────────────────────
BLOB = {"tree": 0.55, "tree_round": 0.45, "outcrop": 0.7, "Tent": 1.1, "Well": 0.7, "boulder": 0.6}


def ground(shelf: Shelf, m: np.ndarray) -> Path:
    tex = T.load_textures()
    layers = T.Layers(tex["tex"], tex["details"])
    img = T.composite(m, layers, ppt=PPT, origin=(C0, R0)).astype(np.float32)
    # web contact shadows under every placed prop (blobShadowRadiusCm or the manifest footprint), a true circle
    H, W = img.shape[:2]
    ys, xs = np.mgrid[0:H, 0:W]
    for name, c, r, _, o in shelf.placed:
        e = shelf.entry(name)
        if "Grass" in name or "Flower" in name or "Lily" in name:
            continue
        rad = e.get("blobShadowRadiusCm")
        if rad is None:
            bmin, bmax = e["boundsCm"]["min"], e["boundsCm"]["max"]
            rad = 0.42 * max(bmax[0] - bmin[0], bmax[1] - bmin[1])
        rad = min(rad / 100.0, 1.3) * PPT
        cx, cy = (c - C0 + 0.5) * PPT, (r - R0 + 0.5) * PPT
        x0, x1 = int(max(cx - rad, 0)), int(min(cx + rad + 1, W))
        y0, y1 = int(max(cy - rad, 0)), int(min(cy + rad + 1, H))
        if x0 >= x1 or y0 >= y1:
            continue
        d = np.hypot(xs[y0:y1, x0:x1] - cx, ys[y0:y1, x0:x1] - cy) / rad
        a = np.where(d < 0.65, 0.42 - 0.16 * d / 0.65, 0.26 * (1 - (d - 0.65) / 0.35))
        a = np.clip(a, 0, 1) * (d < 1)
        img[y0:y1, x0:x1] = img[y0:y1, x0:x1] * (1 - a[..., None]) + np.array([8, 6, 16]) * a[..., None]
    p = E.SCRATCH / "vignette_ground.png"
    E.SCRATCH.mkdir(parents=True, exist_ok=True)
    pngio.write_png(p, img.clip(0, 255).astype(np.uint8))
    me = bpy.data.meshes.new("vignette_ground")
    x0, x1 = C0 - 0.5, C1 - 0.5
    y0, y1 = -(R0 - 0.5), -(R1 - 0.5)
    me.from_pydata([(x0, y0, 0), (x1, y0, 0), (x1, y1, 0), (x0, y1, 0)], [], [(0, 1, 2, 3)])
    uv = me.uv_layers.new(name="UVMap")
    for li, (u, v) in enumerate(((0, 1), (1, 1), (1, 0), (0, 0))):
        uv.data[li].uv = (u, v)
    ob = bpy.data.objects.new("vignette_ground", me)
    scene.link(ob)
    mat = bpy.data.materials.new("vignette_ground")
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    tx = nt.nodes.new("ShaderNodeTexImage")
    tx.image = bpy.data.images.load(str(p))
    tx.interpolation = "Linear"
    em = nt.nodes.new("ShaderNodeEmission")
    o = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(tx.outputs["Color"], em.inputs["Color"])
    nt.links.new(em.outputs[0], o.inputs["Surface"])
    me.materials.append(mat)
    return p


def footprints(shelf: Shelf, m: np.ndarray) -> Path:
    """W5 check: the tiles each placed prop blocks (manifest ``blocking`` + ``footprintTiles`` round its anchor
    tile), painted over a flat map of the tile ids."""
    pal = {0: (116, 162, 71), 1: (192, 154, 96), 2: (169, 162, 145), 3: (63, 147, 184), 4: (60, 70, 50),
           5: (184, 148, 104), 6: (110, 80, 50)}
    k = 16
    img = np.zeros(((R1 - R0) * k, (C1 - C0) * k, 3), np.uint8)
    for r in range(R0, R1):
        for c in range(C0, C1):
            img[(r - R0) * k:(r - R0 + 1) * k, (c - C0) * k:(c - C0 + 1) * k] = pal[tile(m, c, r)]
    img[::k, :] = (40, 40, 40)
    img[:, ::k] = (40, 40, 40)
    for name, c, r, _, o in shelf.placed:
        e = shelf.entry(name)
        if not e.get("blocking"):
            continue
        fw, fh = e.get("footprintTiles", [1, 1])
        ac, ar = int(round(c)), int(round(r))
        for dr in range(fh):
            for dc in range(fw):
                cc, rr = ac + dc - (fw - 1) // 2, ar + dr - (fh - 1) // 2
                if C0 <= cc < C1 and R0 <= rr < R1:
                    y, x = (rr - R0) * k, (cc - C0) * k
                    img[y + 3:y + k - 2, x + 3:x + k - 2] = (200, 40, 40)
    img = img.repeat(2, 0).repeat(2, 1)
    review.draw_text(img, 4, 4, "W5 BLOCKING FOOTPRINTS (RED) OF THE PLACED PROPS ON THE TILE GRID")
    p = OUT / "vignette_footprints.png"
    pngio.write_png_budget(p, img)
    return p


# ── render ──────────────────────────────────────────────────────────────────────────────────────────────
def main(argv: list[str]) -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    scene.reset()
    m = build_map()
    shelf = Shelf()
    dress(shelf, m)
    ground(shelf, m)
    footprints(shelf, m)
    hero_c, hero_r = 8.8, 8.2
    w = E.import_warrior(at=tuple(E.tile_to_blender(hero_c, hero_r))[:2] + (0.0,), yaw_deg=-60.0)
    meshes = [o for *_, o in shelf.placed]
    if w is not None:
        meshes += w[1]
    review.setup_render((1920, 1080), 8)
    focus = E.tile_to_blender(10.2, 7.6)
    cam = review.game_camera((focus.x, focus.y))
    for o in meshes:
        if "af_outline_width" not in o:
            continue
        cls = o.get("af_outline_class", "decor")
        px = outline.SCREEN_PX_1080[cls] if cls in ("hero", "weapon") else E.ink_px(o.get("af_family", ""), cls)
        review.set_outline_px([o], px)
    full = review.render_array()
    full = review.bloom(full, review.emissive_pass(meshes), 1080)
    half = pngio.downscale(full, 0.5)
    review.draw_text(half, 6, 6, "EMERALD PLAINS VIGNETTE  W1 GAME CAMERA DEFAULT DISTANCE  1080P FRAME AT 1/2")
    pngio.write_png_budget(OUT / "vignette.png", half)

    def crop(col, row, w_px, h_px, label, name):
        x, y = review.project_px(cam, E.tile_to_blender(col, row, 0.5), (1920, 1080))
        x0 = int(min(max(x - w_px / 2, 0), 1920 - w_px))
        y0 = int(min(max(y - h_px / 2, 0), 1080 - h_px))
        c = full[y0:y0 + h_px, x0:x0 + w_px].copy()
        review.draw_text(c, 6, 6, label)
        pngio.write_png_budget(OUT / name, c)
        print(f"  {OUT / name}")
    crop(9.4, 9.6, 720, 440, "CAMP GATE + HERO  NATIVE 1080P 1:1", "vignette_1080crop.png")
    crop(15.4, 4.6, 720, 440, "POND, RIDGE, RUINS  NATIVE 1080P 1:1", "vignette_pond.png")
    crop(11.4, 14.6, 720, 440, "CAMP: TENTS, WELL, CAMPFIRE, BANNERS  NATIVE 1080P 1:1", "vignette_camp.png")
    print(f"  {OUT / 'vignette.png'}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

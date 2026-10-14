"""Emerald Plains vignette: the shipped environment family composed on the tile grid at the W1 game camera, the
warrior for scale (spec art-inventory-ch1.md §6; DECISIONS W1, R1, W5).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/vignette.py

Everything comes from ``Art/Export``: every static mesh is **re-imported from its FBX** (what UE receives) with the
toon / outline preview materials read from the shipped palette PNGs, its ink class and family from the manifest
entry, and placed the way the runtime places it (``AAbyssZoneActor``: outcrop variant = tileHash(c, r, 3) % 6,
palisade stakes at the centre and 0.2 / 0.4 toward every camp-wall neighbour, camp props facing the camp centre —
tents clamped to a door within 60° of screen-down —, lily pads on water tiles; ``check_dry`` keeps every other prop's
base off water and wall tiles). The ground is the shipped terrain texture set composited with the exact transition
rules (``terrain.composite``) + the web contact shadows under the props. The hero is ``SK_Hero_Warrior`` with its
Idle clip.

Writes ``Art/Previews/env_vignette/``: ``vignette.png`` (the whole frame as a native 640 × 360 render of the same camera),
``vignette_1080crop.png`` (native 1080p pixels round the hero and the camp gate), ``vignette_pond.png`` (the pond and the
elven ruins on its near shore), ``vignette_camp.png`` and ``vignette_footprints.png`` (W5: the blocking footprints of every
placed prop painted on the tile grid from the manifest ``footprintTiles`` / ``blocking``). Every image is truecolour at
scale 1 (``pngio.write_png_native``: busy crops are split into labelled ``_2`` … tiles, each read back and compared
pixel-exact); ``hero_check`` asserts the warrior's crimson and ink survive; ``ground_check`` asserts plain grass
renders at its painted terrain colour (the ground plane is blacked out of the bloom source, art review 10).
"""
from __future__ import annotations

import json
import math
import os
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
    # tents: door toward the camp centre, clamped to within TENT_DOOR_MAX_DEG of screen-down (art review 10: a tent
    # standing broadside to the fixed camera reads as a flat brown shed wall — no A-frame, no dark door)
    shelf.put("SM_Prop_Camp_Tent", 8.6, 15.6, clamp_to_camera(facing(8.6, 15.6)))
    shelf.put("SM_Prop_Camp_Tent", 12.6, 15.8, clamp_to_camera(facing(12.6, 15.8)))
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
        ("boulder", 10.2, 0.8), ("rock", 6.2, 3.0), ("rock", 11.2, 7.4), ("rock", 16.8, 1.8), ("rock", 3.8, 8.0),
        ("mushroom_red", 5.4, 10.6), ("mushroom_red", 13.6, 8.2),
    ]
    for gid, c, r in scatter:
        h = E.tile_hash(int(c * 10), int(r * 10), 5)
        shelf.put(variant(shelf, gid, h), c, r, float(h % 360), scale=0.92 + 0.16 * ((h >> 11) % 100) / 100)
    rng = scene.rng("vignette", "groundcover")
    for k in range(70):
        c, r = rng.uniform(0, 20), rng.uniform(-3, 17)
        # plain grass under the whole tuft (art review 10: a tuft centred on grass beside the pond reached into the
        # water tile next to it)
        if any(tile(m, int(round(c + dc)), int(round(r + dr))) != 0 for dc in (-0.45, 0.0, 0.45)
               for dr in (-0.45, 0.0, 0.45)):
            continue
        gid = "grass" if k % 3 else "flower"
        h = E.tile_hash(int(c * 10), int(r * 10), 7)
        shelf.put(variant(shelf, gid, h), c, r, float(h % 360), scale=0.9 + 0.2 * ((h >> 5) % 100) / 100)
    # story / lore / interactive accents along the walk
    shelf.put("SM_Prop_Lore_AncientTablet", 6.4, 6.0, CAMERA_YAW + 180.0 + MESH_YAW + 20)
    # the ruins stand on dry grass below the pond's near shore, inside the frame (art review 10: at (17.6, 3.4) the
    # 2 x 1 footprint reached pond tile (17, 4) — its front column stood in the water with lily pads clipping its
    # base; the reviewer's (18.6, 2.4) is a ridge wall tile and outside the frame); the boulder moved to make room
    shelf.put("SM_Prop_Story_ElvenRuins", RUINS_AT[0], RUINS_AT[1], CAMERA_YAW + 180.0 + MESH_YAW)
    check_dry(shelf, m)


RUINS_AT = tuple(float(v) for v in os.environ.get("AF_RUINS_AT", "11.4,2.6").split(","))
TENT_DOOR_MAX_DEG = 60.0


def clamp_to_camera(yaw_ue: float) -> float:
    """A camp prop's UE yaw with its door (mesh front) turned to within ``TENT_DOOR_MAX_DEG`` of screen-down (the
    camera's ground direction, door azimuth CAMERA_YAW + 180)."""
    door = yaw_ue - MESH_YAW
    down = CAMERA_YAW + 180.0
    d = (door - down + 180.0) % 360.0 - 180.0
    d = max(-TENT_DOOR_MAX_DEG, min(TENT_DOOR_MAX_DEG, d))
    return down + d + MESH_YAW


def check_dry(shelf: Shelf, m: np.ndarray) -> None:
    """Decor footprints stay off the water and the wall ridge (art review 10): no toon vertex below 30 cm of a placed
    prop (lily pads excepted; outcrops on their own wall tiles) may stand over a water or a wall tile."""
    bad = []
    for name, c, r, _, o in shelf.placed:
        if "Lily" in name:
            continue
        me = o.data
        mw = o.matrix_world
        mi = np.zeros(len(me.polygons), np.int32)
        me.polygons.foreach_get("material_index", mi)
        vs = {v for p in me.polygons if mi[p.index] == 0 for v in p.vertices}
        wet = 0
        forbid = (3,) if "Outcrop" in name else (3, 4)
        for vi in vs:
            co = me.vertices[vi].co
            if co.z > 0.3:
                continue
            w = mw @ co
            if tile(m, int(round(w.x)), int(round(-w.y))) in forbid:
                wet += 1
        if wet:
            bad.append(f"{name} at ({c:g}, {r:g}): {wet} base vertices over water / wall tiles")
    if bad:
        raise RuntimeError("vignette dressing: props standing in the pond or the ridge\n  " + "\n  ".join(bad))


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
    tile), painted over a flat map of the tile ids. Half-tile placements (the gate posts at camp.x − 1.5 / + 0.5)
    are skipped: they are not blocking and rounding them would paint the gate shut."""
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
        if abs(c - round(c)) == 0.5 or abs(r - round(r)) == 0.5:
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
    pngio.write_png_native(p, img)
    return p


def hero_check(full: np.ndarray, cam, w) -> None:
    """Art review 9 check on the native frame: the warrior's crimson (cape, plume, shield #A82230 family) is still
    red — not the brown of a 256-colour fallback — and his ink is dark."""
    if w is None:
        return
    root, meshes = w
    pts = review.mesh_points(meshes, step=4)
    xy = [review.project_px(cam, Vector(p), (1920, 1080)) for p in pts]
    x0, x1 = int(max(min(x for x, _ in xy) - 4, 0)), int(min(max(x for x, _ in xy) + 4, 1920))
    y0, y1 = int(max(min(y for _, y in xy) - 4, 0)), int(min(max(y for _, y in xy) + 4, 1080))
    reg = full[y0:y1, x0:x1].astype(np.int32)
    r, g, b = reg[..., 0], reg[..., 1], reg[..., 2]
    crimson = (r > 110) & (r > 2.0 * g) & (r > 1.7 * b)
    lum = 0.299 * r + 0.587 * g + 0.114 * b
    ink = lum < 40
    print(f"  [hero] crimson px {int(crimson.sum())} (mean {reg[crimson].mean(0).round().tolist() if crimson.any() else '-'}), "
          f"ink px {int(ink.sum())} of {reg.shape[0] * reg.shape[1]}")
    if crimson.sum() < 80 or ink.sum() < 80:
        raise RuntimeError("vignette: the warrior's crimson / ink did not survive in the native frame")


def _black_emission():
    mat = bpy.data.materials.get("AF_Vignette_GroundBlack")
    if mat is None:
        mat = bpy.data.materials.new("AF_Vignette_GroundBlack")
        mat.use_nodes = True
        nt = mat.node_tree
        nt.nodes.clear()
        em = nt.nodes.new("ShaderNodeEmission")
        em.inputs["Color"].default_value = (0.0, 0.0, 0.0, 1.0)
        out = nt.nodes.new("ShaderNodeOutputMaterial")
        nt.links.new(em.outputs[0], out.inputs["Surface"])
    return mat


def ground_check(img: np.ndarray, cam, res, shelf: Shelf, m: np.ndarray, gimg: np.ndarray, what: str) -> None:
    """Art review 10: the ground reaches the frame at its painted colour. On the five plain grass tiles farthest from
    every placed prop (3 x 3 grass round each, inside the frame), 7 x 7 ground points are projected into the native
    render and compared with the composited terrain texels under them: the best tile must agree within 3/255 and
    the median within 4/255 (a prop's own bloom may still reach one tile; the ground plane in the bloom source put
    every tile ≈ 30/255 too bright)."""
    pts = [(o.location.x, -o.location.y) for *_, o in shelf.placed]
    cands = []
    for r in range(R0 + 1, R1 - 1):
        for c in range(C0 + 1, C1 - 1):
            if any(tile(m, c + dc, r + dr) != 0 for dc in (-1, 0, 1) for dr in (-1, 0, 1)):
                continue
            x, y = review.project_px(cam, E.tile_to_blender(c, r), res)
            if not (0.1 * res[0] < x < 0.9 * res[0] and 0.1 * res[1] < y < 0.9 * res[1]):
                continue
            cands.append((min(math.hypot(c - px, r - py) for px, py in pts), c, r))
    cands.sort(reverse=True)
    if not cands:
        raise RuntimeError(f"ground_check {what}: no plain grass tile in the frame")
    errs = []
    for d, c, r in cands[:5]:
        ren, tex = [], []
        for i in range(7):
            for j in range(7):
                gc, gr = c + (i - 3) * 0.08, r + (j - 3) * 0.08
                x, y = review.project_px(cam, E.tile_to_blender(gc, gr), res)
                ren.append(img[int(y), int(x), :3].astype(np.float32))
                tx, ty = (gc - C0 + 0.5) * PPT, (gr - R0 + 0.5) * PPT
                tex.append(gimg[int(ty), int(tx)])
        mr, mt = np.mean(ren, 0), np.mean(tex, 0)
        err = float(np.abs(mr - mt).max())
        errs.append(err)
        print(f"  [ground] {what}: grass tile ({c}, {r}) ({d:.1f} tiles from the nearest prop): render "
              f"{mr.round(1).tolist()} vs terrain {mt.round(1).tolist()} -> max |d| {err:.2f}/255")
    if min(errs) > 3.0 or float(np.median(errs)) > 4.0:
        raise RuntimeError(f"ground_check {what}: the ground renders off its painted colour (best {min(errs):.1f}, "
                           f"median {float(np.median(errs)):.1f}/255 — bloom / exposure on the ground plane?)")


# ── render ──────────────────────────────────────────────────────────────────────────────────────────────
def main(argv: list[str]) -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    scene.reset()
    m = build_map()
    shelf = Shelf()
    dress(shelf, m)
    if "--check" in argv:                      # dressing checks only (no render)
        print("  dressing OK:", len(shelf.placed), "placed")
        return 0
    gpath = ground(shelf, m)
    gimg = pngio.read_png(gpath)[:, :, :3].astype(np.float32)
    footprints(shelf, m)
    hero_c, hero_r = 8.8, 8.2
    w = E.import_warrior(at=tuple(E.tile_to_blender(hero_c, hero_r))[:2] + (0.0,), yaw_deg=-60.0)
    meshes = [o for *_, o in shelf.placed]
    if w is not None:
        meshes += w[1]
    focus = E.tile_to_blender(10.2, 7.6)

    def render_at(res):
        review.setup_render(res, 8)
        cam = review.game_camera((focus.x, focus.y))
        k = res[1] / 1080.0
        for o in meshes:
            if "af_outline_width" not in o:
                continue
            cls = o.get("af_outline_class", "decor")
            px = outline.SCREEN_PX_1080[cls] if cls in ("hero", "weapon") else E.ink_px(o.get("af_family", ""), cls)
            review.set_outline_px([o], px * k)
        img = review.render_array()
        # emissive_pass swaps slot materials per object; the placed copies share their mesh data, so the swap of
        # one copy is saved as the "original" of the next — restore every mesh's own materials afterwards
        saved = {}
        for o in meshes:
            saved.setdefault(o.data.name, (o.data, list(o.data.materials)))
        # the vignette's own ground is an emission plane, not the kit's AF_Stage (which emissive_pass hides): black it
        # out for the bloom source but keep it occluding (art review 10: it bloomed at +0.8x its own linear value —
        # grass (116,162,71) rendered (145,204,92), water went neon, ink next to grass lightened)
        gmesh = bpy.data.objects["vignette_ground"].data
        gmats = list(gmesh.materials)
        gmesh.materials[0] = _black_emission()
        try:
            em = review.emissive_pass(meshes)
        finally:
            gmesh.materials[0] = gmats[0]
            for data, mats in saved.values():
                for i, mt in enumerate(mats):
                    data.materials[i] = mt
        return cam, review.bloom(img, em, res[1])
    # overview: a native 640 × 360 render of the same camera (ink and bloom at 360p, like every game.png) — never a
    # resampled 1080p frame (art review 9: the 1/2 downscale + 256-colour fallback turned the crimson cape brown)
    cam_s, small = render_at((640, 360))
    ground_check(small, cam_s, (640, 360), shelf, m, gimg, "vignette.png")

    def overview_label(t, i, k):
        review.draw_text(t, 6, 6, "EMERALD PLAINS VIGNETTE  W1 CAMERA DEFAULT DISTANCE  NATIVE 640X360 RENDER"
                         + (f"  {i + 1}/{k}" if k > 1 else ""))
    written = pngio.write_png_native(OUT / "vignette.png", small, annotate=overview_label)
    cam, full = render_at((1920, 1080))
    ground_check(full, cam, (1920, 1080), shelf, m, gimg, "1080p frame")

    def crop(col, row, w_px, h_px, label, name):
        """Native 1080p pixels (scale 1, truecolour); split into labelled tiles when one file would pass 200 KB."""
        x, y = review.project_px(cam, E.tile_to_blender(col, row, 0.5), (1920, 1080))
        x0 = int(min(max(x - w_px / 2, 0), 1920 - w_px))
        y0 = int(min(max(y - h_px / 2, 0), 1080 - h_px))
        c = full[y0:y0 + h_px, x0:x0 + w_px].copy()

        def lab(t, i, k):
            review.draw_text(t, 6, 6, label + (f"  TILE {i + 1}/{k}" if k > 1 else ""))
        out = pngio.write_png_native(OUT / name, c, annotate=lab)
        back = np.concatenate([pngio.read_png(p)[:, :, :3] for p, _ in out], axis=1 if len(out) > 1 else 0)
        body = np.ones(c.shape[:2], bool)
        body[:20, :] = False                  # the label rows differ (each tile is labelled)
        if back.shape != c.shape or not np.array_equal(back[body], c[body]):
            raise RuntimeError(f"{name}: the stored pixels differ from the native render")
        for p, size in out:
            print(f"  {p} {size // 1024} KB (truecolour, scale 1, pixel-exact)")
        return out
    print(f"  {written[0][0]}")
    crop(9.4, 9.6, 720, 440, "CAMP GATE + HERO  NATIVE 1080P 1:1", "vignette_1080crop.png")
    crop(12.4, 2.8, 720, 440, "POND AND ELVEN RUINS  NATIVE 1080P 1:1", "vignette_pond.png")
    crop(11.4, 14.6, 720, 440, "CAMP: TENTS, WELL, CAMPFIRE, BANNERS  NATIVE 1080P 1:1", "vignette_camp.png")
    hero_check(full, cam, w)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

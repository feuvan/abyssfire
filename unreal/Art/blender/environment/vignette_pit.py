"""The Pit vignettes: 信者之坑 / the Pit of the Faithful room by room, composed from the **shipped** crypt kit, the Pit
terrain and the shipped monsters at the W1 game camera, **lit by the hero's light radius** (dark remake Wave 3, A13 /
3C; chapter1.md §9.2: "the hero's light radius is the main light").

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/vignette_pit.py
        [room …] [--check] [--flat] [--whole]

Everything is re-imported from ``Art/Export`` (FBX + shipped palettes + manifest ink and slot params, ``cohkit``) and
placed the way U6 will place it:

* a wall block on every wall tile — ``outcrop_plains@pit`` (the variants sorted by name, ``tileHash(c, r, 3) % n``),
  yaw **snapped to 90°** (``(hash >> 8) % 4 * 90``: the blocks abut face to face); no block within one tile of the exit
  (``BuildWalls`` ``NearExit``): the Ash Steps (``exit_portal@pit``) stand there;
* the wall overlays (pier, helm niches, lamps, tag curtains / strands, the Flame Niche) on the wall tile's own centre,
  turned to the floor tile they face: ``yaw = atan2(−dc, dr)`` for a floor neighbour at (dc, dr) (:func:`face_yaw`);
* the floor: ``terrain.pit`` composited over the room's tile types (flagstone 2, ash 0, dug earth 1, paving 5).

**The light radius** (``LIGHT``; the recipe U6 implements: ``vfx_recipes.json`` *pit* section) is emulated per pixel
from a world-position pass: frame × (ambient + Σ lights), a light = colour × (1 − d / r)^p on the ground distance from
its centre; emissive pixels (eyes, the breach seep, lamp flames) keep their own light and bloom. Uncleansed: a cold
ambient of ~¼ and the hero's warm radius. Cleansed (``rf_pit_cleansed``): the ambient warms and lifts, every lit lamp
adds its pool, embers rise from the floor (drawn as additive sprites: ``prop.pit_cleanse.embers``).

Rooms (``Art/Previews/env_vignette_pit/<room>.png``, native 1080p crops cut into truecolour strips, each with a
``_flat`` twin without the light radius — the kit's own values — and a ``<room>[_flat]_whole.png``, the whole frame
in one file at half size for the composition): ``ash_steps``, ``helm_wall``, ``tag_cellar``, ``pitwardens_cut``,
``flame_niche``, ``flame_niche_cleansed``, and ``pit_mouth_open`` (the surface entrance on the march terrain,
daylight: no radius).
``--whole`` rebuilds the whole-frame sheets from the strips on disk (no render).
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
BLENDER = HERE.parent
for _p in (HERE, BLENDER, BLENDER / "coherence"):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

import envkit as E  # noqa: E402
import cohkit as K  # noqa: E402
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector  # noqa: E402

from kit import paths, review, scene  # noqa: E402

OUT = paths.preview_root() / "env_vignette_pit"
RES = (1920, 1080)
SIZE = 26                                   # the test maps: SIZE × SIZE tiles, wall mass with a room cut out
APEX = (17, 17)                             # the room's far corner: walls at col ≥ 17 (left arm, in the shade band)
                                            # and row ≥ 17 (right arm, key-lit); the floor runs open toward the camera
# The light radius (the recipe U6 implements; values in tiles / linear multipliers)
LIGHT = {
    "ambient": (0.21, 0.23, 0.30),          # uncleansed: cold, a quarter of the kit's values
    "hero": {"color": (1.00, 0.93, 0.80), "radius": 5.6, "power": 1.5, "gain": 0.95},
    "ambientCleansed": (0.52, 0.44, 0.36),  # rf_pit_cleansed: the grade warms and lifts
    "lamp": {"color": (1.00, 0.66, 0.36), "radius": 2.9, "power": 1.4, "gain": 0.85},
    "ceil": 1.12,
}
EMBER = ((255, 138, 60), (255, 196, 110))   # hearth.ember / lantern.glow (the cleanse's embers)


def face_yaw(dc: float, dr: float) -> float:
    """UE yaw of an overlay / piece whose front (mesh −Y) must look at the floor tile (dc, dr) away."""
    return math.degrees(math.atan2(-dc, dr))


def look(c: float, r: float, tc: float, tr: float) -> float:
    """Blender yaw of a figure at (c, r) facing tile (tc, tr)."""
    return math.degrees(math.atan2(tc - c, tr - r))


def wall_yaw(c: int, r: int) -> float:
    return float(((E.tile_hash(c, r, 3) >> 8) % 4) * 90)


class Kit:
    """Shipped statics, imported once (``cohkit.import_static``: family materials, slot params, ink) and instanced."""

    def __init__(self):
        self.src: dict[str, object] = {}
        self.placed: list[tuple] = []

    def put(self, name: str, c: float, r: float, yaw_ue: float = 0.0, z: float = 0.0, scale: float = 1.0):
        src = self.src.get(name)
        if src is None:
            src = self.src[name] = K.import_static(name)
            src.hide_render = True
        o = src.copy()
        scene.link(o)
        o.hide_render = False
        o.location = E.tile_to_blender(c, r, z)
        o.rotation_euler = (0, 0, E.ue_yaw_to_blender(yaw_ue))
        o.scale = (scale, scale, scale)
        for k in ("af_outline_width", "af_outline_class", "af_px1080", "af_family"):
            if k in src:
                o[k] = src[k]
        self.placed.append((name, c, r, yaw_ue, o))
        return o

    def meshes(self) -> list:
        return [o for *_, o in self.placed]


class Room:
    """One vignette: the tile map (wall mass 4 everywhere, the room painted in), the placements, the cast."""

    def __init__(self, key: str, title: str, focus=(12.6, 12.6), crop=(1360, 820), theme: str = "pit"):
        self.key, self.title, self.focus, self.crop, self.theme = key, title, focus, crop, theme
        self.m = np.full((SIZE, SIZE), 4, np.int32)
        self.m[:APEX[1], :APEX[0]] = 2                       # the room: flagstone, open toward the camera
        self.exit: tuple[int, int] | None = None
        self.pieces: list[tuple] = []                        # (asset, c, r, yaw_ue, z)
        self.cast: list[tuple] = []                          # (asset, c, r, yaw_blender, clip, frame)
        self.hero: tuple | None = None                       # (c, r, yaw_blender, clip, frame)
        self.lamps: list[tuple] = []                         # (c, r, dc, dr): wall tile + the floor it faces
        self.cleansed = False
        self.embers = 0
        self.lights_off = False                              # the surface: daylight, no radius
        self.skip_walls: set[tuple[int, int]] = set()

    def paint(self, c0, r0, c1, r1, t) -> None:
        self.m[r0:r1 + 1, c0:c1 + 1] = t

    def overlay(self, asset: str, c: int, r: int, dc: int, dr: int, along: float = 0.0) -> None:
        """On wall tile (c, r), facing the floor at (dc, dr); ``along`` = offset along the wall (m)."""
        self.pieces.append((asset, c + along * abs(dr), r + along * abs(dc), face_yaw(dc, dr), 0.0))

    def lamp(self, c: int, r: int, dc: int, dr: int) -> None:
        self.lamps.append((c, r, dc, dr))

    def mob(self, asset, c, r, to, clip=None, frame=0) -> None:
        self.cast.append((asset, c, r, look(c, r, *to), clip, frame))


# ── the rooms ───────────────────────────────────────────────────────────────────────────────────────────
A, R_ = APEX
RIGHT = (0, -1)       # the right arm (row ≥ 17): its faces look toward −row (key-lit, screen-left)
LEFT = (-1, 0)        # the left arm (col ≥ 17): its faces look toward −col (shade band, screen-right)


def corner(room: Room) -> None:
    room.overlay("SM_Pit_WallPier", A, R_, -1, -1)           # the inner corner (any yaw: the pier is four-sided)
    room.pieces[-1] = ("SM_Pit_WallPier", A, R_, 0.0, 0.0)


def ash_steps() -> Room:
    rm = Room("ash_steps", "THE ASH STEPS 灰阶: THE STAIR (EXIT_PORTAL@PIT), ASH DRIFTS, 4 BREACHCLOTS",
              focus=(12.3, 12.9))
    rm.exit = (12, R_ - 1)
    rm.paint(9, 13, 15, 16, 0)                               # ash over the flags round the stair's foot
    rm.paint(11, 11, 13, 12, 0)
    rm.pieces.append(("SM_Pit_AshSteps", 12, R_ - 1, face_yaw(0, -1), 0.0))
    rm.pieces += [("SM_Pit_AshDrift_A", 9.6, 15.4, 40.0, 0.0), ("SM_Pit_AshDrift_B", 14.5, 15.3, 200.0, 0.0),
                  ("SM_Pit_AshDrift_A", 13.6, 12.6, 150.0, 0.0), ("SM_Pit_AshDrift_B", 10.4, 12.2, 80.0, 0.0)]
    corner(rm)
    rm.lamp(8, R_, *RIGHT)
    rm.lamp(A, 13, *LEFT)
    rm.overlay("SM_Pit_WallPier", A, 9, *LEFT)
    rm.hero = (12.0, 14.6, look(12.0, 14.6, 10.5, 11.5), "Idle", 0)
    for c, r in ((9.2, 11.3), (11.0, 10.2), (13.1, 10.9), (8.8, 13.4)):
        rm.mob("SK_Mon_Breachclot", c, r, (12.0, 14.6))
    return rm


def helm_wall() -> Room:
    rm = Room("helm_wall", "THE HELM WALL 盔墙: KETTLE HELMS IN THE NICHES, FACING UP, 6 FOOTMEN AT ATTENTION",
              focus=(12.0, 13.4))
    for k, c in enumerate(range(8, 16)):
        rm.overlay("SM_Pit_HelmNiche_" + "AB"[k % 2], c, R_, *RIGHT)
    rm.overlay("SM_Pit_WallPier", 7, R_, *RIGHT)
    corner(rm)
    rm.lamp(6, R_, *RIGHT)
    rm.lamp(A, 14, *LEFT)
    rm.lamp(A, 10, *LEFT)
    rm.overlay("SM_Pit_HelmNiche_A", A, 12, *LEFT)
    rm.overlay("SM_Pit_HelmNiche_B", A, 16, *LEFT)
    for k in range(6):                                       # the roll call: a rank facing out from the wall
        c = 9.0 + 1.15 * k
        rm.mob("SK_Mon_RisenFootman", c, 15.2, (c, 10.0), "Idle", (k * 7) % 20)
    rm.hero = (11.2, 11.3, look(11.2, 11.3, 11.6, 15.2), "Idle", 0)
    return rm


def tag_cellar() -> Room:
    rm = Room("tag_cellar", "THE TAG CELLAR 名牌窖: TAG CURTAINS, RACKS, STRANDS; 2 ARBALESTS + 4 GNAWERS",
              focus=(12.2, 12.8))
    for c in range(7, A):
        rm.overlay("SM_Pit_TagCurtain", c, R_, *RIGHT)
    for r in range(9, R_):                                   # the left arm: single strands, instanced with offsets
        for k in range(7):
            h = E.tile_hash(r * 7 + k, 3, 11)
            rm.overlay("SM_Pit_TagStrand_" + "ABC"[h % 3], A, r, *LEFT, along=-0.42 + 0.14 * k)
    corner(rm)
    rm.lamp(6, R_, *RIGHT)
    rm.lamp(A, 8, *LEFT)
    for c, r, yaw in ((9.5, 14.4, 0.0), (13.0, 14.4, 0.0), (9.5, 11.6, 0.0), (13.2, 11.5, 0.0)):
        rm.pieces.append(("SM_Pit_TagRack", c, r, yaw, 0.0))
    rm.hero = (11.3, 9.4, look(11.3, 9.4, 11.6, 13.0), "Idle", 0)
    rm.mob("SK_Mon_RisenArbalest", 14.6, 15.2, (11.3, 9.4))
    rm.mob("SK_Mon_RisenArbalest", 7.6, 15.4, (11.3, 9.4))
    for c, r in ((11.3, 13.0), (12.9, 12.9), (8.6, 10.2), (14.6, 9.9)):
        rm.mob("SK_Mon_Gravegnawer", c, r, (11.3, 9.4))
    return rm


def pitwardens_cut() -> Room:
    rm = Room("pitwardens_cut", "THE PITWARDEN'S CUT 掘坑官之室: DUG EARTH, THE PITWARDEN + 2 SHIELDBEARERS",
              focus=(12.0, 12.4))
    rm.paint(8, 9, 15, 16, 1)                                # the cut: dug earth
    rm.paint(8, 9, 9, 10, 2)
    rm.paint(14, 9, 15, 9, 2)
    corner(rm)
    rm.lamp(9, R_, *RIGHT)
    rm.lamp(14, R_, *RIGHT)
    rm.lamp(A, 12, *LEFT)
    rm.overlay("SM_Pit_WallPier", A, 8, *LEFT)
    rm.pieces += [("SM_Pit_AshDrift_B", 15.4, 15.6, 120.0, 0.0)]
    rm.hero = (10.2, 10.6, look(10.2, 10.6, 12.6, 13.8), "Idle", 0)
    rm.mob("SK_Mon_Pitwarden", 12.6, 13.8, (10.2, 10.6))
    rm.mob("SK_Mon_RisenShieldbearer", 10.4, 14.3, (10.2, 10.6))
    rm.mob("SK_Mon_RisenShieldbearer", 14.4, 12.3, (10.2, 10.6))
    return rm


def flame_niche(cleansed: bool = False) -> Room:
    rm = Room("flame_niche_cleansed" if cleansed else "flame_niche",
              ("THE FLAME NICHE 火龛 CLEANSED (RF_PIT_CLEANSED): LAMPS LIT, EMBERS RISING, THE GRADE WARMED"
               if cleansed else "THE FLAME NICHE 火龛: THE EMPTY FLAME, FIVE BURN MARKS, UNLIT LAMPS"),
              focus=(12.2, 13.2))
    rm.paint(9, 12, 15, 16, 5)                               # the niche's finer paving
    rm.overlay("SM_Pit_FlameNiche", 12, R_, *RIGHT)
    corner(rm)
    rm.lamp(10, R_, *RIGHT)
    rm.lamp(14, R_, *RIGHT)
    rm.lamp(7, R_, *RIGHT)
    rm.lamp(A, 14, *LEFT)
    rm.lamp(A, 10, *LEFT)
    rm.overlay("SM_Pit_WallPier", A, 7, *LEFT)
    rm.hero = (12.5, 14.0, look(12.5, 14.0, 12.0, 17.0), "Idle", 0)
    rm.cleansed = cleansed
    rm.embers = 90 if cleansed else 0
    return rm


def pit_mouth_open() -> Room:
    rm = Room("pit_mouth_open", "THE SURFACE: THE PIT MOUTH UNSEALED (SETPIECE_PIT_MOUTH_OPEN) AT (17,26), MARCH "
              "TERRAIN, DAYLIGHT", focus=(12.3, 12.3), crop=(1100, 700), theme="march")
    rm.m[:, :] = 0
    rm.paint(9, 9, 15, 15, 1)
    rm.pieces.append(("SM_Prop_March_PitMouthOpen", 12, 12, 0.0, 0.0))
    rm.hero = (10.2, 13.6, look(10.2, 13.6, 12.0, 12.0), "Idle", 0)
    rm.lights_off = True
    return rm


ROOMS = {"ash_steps": ash_steps, "helm_wall": helm_wall, "tag_cellar": tag_cellar, "pitwardens_cut": pitwardens_cut,
         "flame_niche": flame_niche, "flame_niche_cleansed": lambda: flame_niche(True),
         "pit_mouth_open": pit_mouth_open}


# ── build ───────────────────────────────────────────────────────────────────────────────────────────────
def near_floor(m: np.ndarray, c: int, r: int, reach: int = 3) -> bool:
    r0, r1, c0, c1 = max(r - reach, 0), min(r + reach + 1, m.shape[0]), max(c - reach, 0), min(c + reach + 1, m.shape[1])
    return bool((m[r0:r1, c0:c1] != 4).any())


def dress(kit: Kit, rm: Room) -> list[tuple]:
    """Place the room; returns the lit lamps' light points [(col, row)] (tile space) for the light pass."""
    walls = sorted(n for n, e in K.manifest()["assets"].items() if "outcrop_plains@pit" in (e.get("gameIds") or []))
    if rm.theme == "pit":
        if not walls:
            raise SystemExit("no asset lists outcrop_plains@pit: ship environment/crypt_pit.py first")
        for r in range(SIZE):
            for c in range(SIZE):
                if rm.m[r, c] != 4 or not near_floor(rm.m, c, r):
                    continue
                if rm.exit and abs(rm.exit[0] - c) <= 1 and abs(rm.exit[1] - r) <= 1:
                    continue                                 # BuildWalls NearExit: the stair stands there
                kit.put(walls[E.tile_hash(c, r, 3) % len(walls)], c, r, wall_yaw(c, r))
    for asset, c, r, yaw, z in rm.pieces:
        kit.put(asset, c, r, yaw, z)
    pools = []
    for c, r, dc, dr in rm.lamps:
        kit.put("SM_Pit_NicheLamp_Lit" if rm.cleansed else "SM_Pit_NicheLamp", c, r, face_yaw(dc, dr))
        if rm.cleansed:
            pools.append((c + dc * 1.25, r + dr * 1.25, c + dc * 0.56, r + dr * 0.56))
    return pools


def cast(rm: Room):
    figs, shadows, hero = [], [], None
    for asset, c, r, yaw, clip, frame in rm.cast:
        f = K.import_character(asset, E.tile_to_blender(c, r), yaw, clip=clip, frame=frame)
        for mm in f.all():
            mm["af_px1080"] = mm.get("af_px1080", f.px)
        figs.append(f)
        e = K.entry(asset)
        shadows.append((c, r, float(e.get("blobShadowRadiusCm", 34.0)) / 100.0, 0.45))
    if rm.hero:
        c, r, yaw, clip, frame = rm.hero
        hero = E.import_warrior(at=tuple(E.tile_to_blender(c, r))[:2] + (0.0,), yaw_deg=yaw, clip=clip, frame=frame)
        shadows.append((c, r, 0.36, 0.45))
    return figs, hero, shadows


def light_material(lights) -> bpy.types.Material:
    """Emission = Σ colour × gain × clamp(1 − d / r)^p, d = the ground distance of the shaded point from the light."""
    m = bpy.data.materials.new("AF_PitLightPass")
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    flat = nt.nodes.new("ShaderNodeVectorMath")
    flat.operation = "MULTIPLY"
    flat.inputs[1].default_value = (1.0, 1.0, 0.0)
    nt.links.new(geo.outputs["Position"], flat.inputs[0])
    acc = None
    for (x, y, radius, power, gain, col) in lights:
        d = nt.nodes.new("ShaderNodeVectorMath")
        d.operation = "DISTANCE"
        d.inputs[1].default_value = (x, y, 0.0)
        nt.links.new(flat.outputs["Vector"], d.inputs[0])
        t = nt.nodes.new("ShaderNodeMapRange")
        t.clamp = True
        t.inputs["From Min"].default_value = 0.0
        t.inputs["From Max"].default_value = radius
        t.inputs["To Min"].default_value = 1.0
        t.inputs["To Max"].default_value = 0.0
        nt.links.new(d.outputs["Value"], t.inputs["Value"])
        pw = nt.nodes.new("ShaderNodeMath")
        pw.operation = "POWER"
        pw.inputs[1].default_value = power
        nt.links.new(t.outputs["Result"], pw.inputs[0])
        sc = nt.nodes.new("ShaderNodeVectorMath")
        sc.operation = "SCALE"
        sc.inputs[0].default_value = tuple(c * gain for c in col)
        nt.links.new(pw.outputs["Value"], sc.inputs["Scale"])
        if acc is None:
            acc = sc
        else:
            add = nt.nodes.new("ShaderNodeVectorMath")
            add.operation = "ADD"
            nt.links.new(acc.outputs["Vector"], add.inputs[0])
            nt.links.new(sc.outputs["Vector"], add.inputs[1])
            acc = add
    em = nt.nodes.new("ShaderNodeEmission")
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    if acc is not None:
        nt.links.new(acc.outputs["Vector"], em.inputs["Color"])
    else:
        em.inputs["Color"].default_value = (0, 0, 0, 1)
    nt.links.new(em.outputs[0], out.inputs["Surface"])
    return m


def light_pass(lights) -> np.ndarray:
    vl = bpy.context.view_layer
    mat = light_material(lights)
    vl.material_override = mat
    try:
        with review._World((0.0, 0.0, 0.0)):
            arr = review.render_array(exr=True)
    finally:
        vl.material_override = None
        bpy.data.materials.remove(mat)
    return np.clip(arr, 0.0, 4.0)


def render(rm: Room, flat: bool = False) -> tuple[np.ndarray, object]:
    K.reset()
    K.set_theme(rm.theme)
    kit = Kit()
    pools = dress(kit, rm)
    review.setup_render(RES, 8)
    cam = review.game_camera(tuple(E.tile_to_blender(*rm.focus))[:2])
    figs, hero, shadows = cast(rm)
    K.ground(rm.m, (0, 0), ppt=72, shadows=shadows, name="pit_ground_" + rm.key)
    meshes = kit.meshes()
    for f in figs:
        meshes += f.all()
    if hero is not None:
        from kit import outline
        for mm in hero[1]:
            mm["af_px1080"] = outline.SCREEN_PX_1080[mm.get("af_outline_class", "hero")]
        meshes += hero[1]
    K.apply_ink(meshes, 1.0)
    raw = review.render_array()
    saved = {}
    for o in meshes:
        if o.type == "MESH":
            saved.setdefault(o.data.name, (o.data, list(o.data.materials)))
    em = review.emissive_pass(meshes)
    for data, ms in saved.values():
        for i, mt in enumerate(ms):
            data.materials[i] = mt
    lin = review._srgb_to_lin(raw.astype(np.float32) / 255.0)
    if not (flat or rm.lights_off):
        lights = []
        if rm.hero:
            h = LIGHT["hero"]
            p = E.tile_to_blender(rm.hero[0], rm.hero[1])
            lights.append((p.x, p.y, h["radius"], h["power"], h["gain"], h["color"]))
        la = LIGHT["lamp"]
        for (c, r, *_s) in pools:
            p = E.tile_to_blender(c, r)
            lights.append((p.x, p.y, la["radius"], la["power"], la["gain"], la["color"]))
        mask = light_pass(lights)
        amb = np.array(LIGHT["ambientCleansed"] if rm.cleansed else LIGHT["ambient"], np.float32)
        L = np.minimum(amb[None, None, :] + mask, LIGHT["ceil"])
        ew = np.clip(em.max(axis=2) / 0.25, 0.0, 1.0)[:, :, None]          # emissive pixels keep their own light
        lin = lin * L * (1.0 - ew) + lin * ew
    img = (review._lin_to_srgb(lin) * 255.0 + 0.5).astype(np.uint8)
    img = review.bloom(img, em, RES[1])
    if rm.exit:                                              # exit.portal@pit: a faint cold daylight in the passage
        x, y = review.project_px(cam, E.tile_to_blender(rm.exit[0], rm.exit[1] + 0.62, 0.95), RES)
        img = review.glow_card(img, (x, y), 60 * RES[1] / 1080.0, "#AEB6C6", 0.16)
    if rm.cleansed and not flat:
        k = RES[1] / 1080.0
        for (_c, _r, fc, fr) in pools:                       # the flame's glow sprite (prop.pit_lamp.flame)
            x, y = review.project_px(cam, E.tile_to_blender(fc, fr, 1.02), RES)
            img = review.glow_card(img, (x, y), 46 * k, "#FF8A3C", 0.55, core_hex="#FFD9A0", core_alpha=0.5)
        rng = scene.rng("vignette_pit", rm.key, "embers")
        for _ in range(rm.embers):                           # prop.pit_cleanse.embers: rising from the floor
            c, r = rng.uniform(6.5, 16.4), rng.uniform(8.0, 16.4)
            z = rng.uniform(0.05, 1.7) ** 1.3
            x, y = review.project_px(cam, E.tile_to_blender(c, r, z), RES)
            a = (1.0 - z / 2.4) * rng.uniform(0.5, 1.0)
            img = review.glow_card(img, (x, y), rng.uniform(5.0, 10.0) * k, "#FF8A3C", 0.9 * a,
                                   core_hex="#FFC46E", core_alpha=0.9 * a)
    return img, cam


def crop_of(img: np.ndarray, cam, rm: Room) -> np.ndarray:
    x, y = review.project_px(cam, E.tile_to_blender(rm.focus[0], rm.focus[1], 0.9), RES)
    w, h = rm.crop
    x0 = int(min(max(x - w / 2, 0), RES[0] - w))
    y0 = int(min(max(y - h / 2, 0), RES[1] - h))
    return img[y0:y0 + h, x0:x0 + w].copy()


WHOLE_W = 680                               # the whole-frame sheet's width (px)


def median_cut(img: np.ndarray, n: int = 256) -> np.ndarray:
    """``img`` (H × W × 3 uint8) on at most ``n`` colours by median cut (boxes split across their widest channel,
    widest box first, weighted a little by population; each box drawn in its mean colour). ``pngio``'s own palette
    is the most frequent colours: in a room that is nine tenths dark stone it gave the crimson tabards, the violet
    eyes and the lamp flames to the nearest grey."""
    h, w, _ = img.shape
    flat = img.reshape(-1, 3).astype(np.int32)

    def box(ix):
        px = flat[ix]
        rng = px.max(0) - px.min(0)
        ch = int(rng.argmax())
        return (float(rng[ch]) * len(ix) ** 0.25, ix, ch)
    boxes = [box(np.arange(len(flat)))]
    while len(boxes) < n:
        k = max(range(len(boxes)), key=lambda i: boxes[i][0])
        score, ix, ch = boxes[k]
        if score <= 0.0 or len(ix) < 2:
            break
        v = flat[ix, ch]
        med = np.median(v)
        lo = ix[v <= med]
        hi = ix[v > med]
        if not len(hi):                                      # the median is the top value: cut under it
            lo, hi = ix[v < med], ix[v >= med]
        if not len(lo) or not len(hi):
            boxes[k] = (0.0, ix, ch)
            continue
        boxes[k] = box(lo)
        boxes.append(box(hi))
    out = np.empty_like(flat)
    for _, ix, _ in boxes:
        out[ix] = np.rint(flat[ix].mean(0)).astype(np.int32)
    return out.reshape(h, w, 3).astype(np.uint8)


def write_whole(path: Path, crop: np.ndarray, label: str) -> tuple[Path, int]:
    """The room's whole composition in one file: the native crop box-filtered down to :data:`WHOLE_W` px under a
    label band (art review r1: the native crops are cut into five to seven vertical strips to stay truecolour
    under the preview budget, so no single file showed a room). Truecolour when it fits the budget, else on 256
    median-cut colours. For the composition only — values, ink and reads are judged on the native strips."""
    from kit import pngio
    k = WHOLE_W / crop.shape[1]
    small = pngio.downscale(crop[:, :, :3], k) if k < 1.0 else crop[:, :, :3]
    band = E.label_band(small.shape[1])
    review.draw_text(band, 4, 4, f"{label}  WHOLE FRAME X{k:.2f} (COMPOSITION ONLY)")
    img = np.vstack([band, small])
    size = pngio.write_png(path, img)
    if size > pngio.PREVIEW_MAX_BYTES:
        size = pngio.write_png(path, median_cut(img), palette=True)
    if size > pngio.PREVIEW_MAX_BYTES:
        raise RuntimeError(f"{path.name}: {size} bytes on 256 colours (budget {pngio.PREVIEW_MAX_BYTES})")
    return path, size


def stitched(stem: str) -> np.ndarray | None:
    """The native crop of ``<stem>.png`` put back together from its strips on disk (``write_banded`` cuts side by
    side and gives every strip the label band), or None when the room has not been rendered."""
    from kit import pngio
    tiles = [OUT / f"{stem}.png"]
    if not tiles[0].exists():
        return None
    k = 2
    while (OUT / f"{stem}_{k}.png").exists():
        tiles.append(OUT / f"{stem}_{k}.png")
        k += 1
    return np.hstack([pngio.read_png(t)[E.HEADER_BAND:, :, :3] for t in tiles])


def main(argv: list[str]) -> int:
    flags = {a for a in argv if a.startswith("--")}
    sel = [a for a in argv if not a.startswith("--")] or list(ROOMS)
    OUT.mkdir(parents=True, exist_ok=True)
    if "--whole" in flags:                                   # the whole-frame sheets again, from the strips on disk
        for key in sel:
            for stem, label in ((key, key.upper()), (key + "_flat", key.upper() + " FLAT")):
                c = stitched(stem)
                if c is not None:
                    pth, size = write_whole(OUT / f"{stem}_whole.png", c, label)
                    print(f"  {pth}  {size // 1024} KB")
        return 0
    for key in sel:
        rm = ROOMS[key]()
        if "--check" in flags:
            K.reset()
            K.set_theme(rm.theme)
            kit = Kit()
            dress(kit, rm)
            print(f"  {key}: {len(kit.placed)} pieces placed, {len(rm.cast)} monsters")
            continue
        variants = [False] if rm.lights_off else ([True] if "--flat" in flags else [False, True])
        for flat in variants:
            img, cam = render(rm, flat)
            c = crop_of(img, cam, rm)
            tail = ("  KIT VALUES, NO LIGHT RADIUS" if flat else
                    ("  DAYLIGHT" if rm.lights_off else "  HERO LIGHT RADIUS %.1f TILES, AMBIENT %s" % (
                        LIGHT["hero"]["radius"], "WARM .52/.44/.36 + LAMP POOLS" if rm.cleansed else "COLD .21/.23/.30")))
            stem = f"{rm.key}{'_flat' if flat and not rm.lights_off else ''}"
            for pth, size in E.write_banded(OUT / f"{stem}.png", c, rm.title.encode("ascii", "ignore").decode()
                                            + "  W1 1080P 1:1" + tail):
                print(f"  {pth}  {size // 1024} KB")
            pth, size = write_whole(OUT / f"{stem}_whole.png", c, rm.key.upper() + (" FLAT" if flat else ""))
            print(f"  {pth}  {size // 1024} KB")
    K.set_theme(None)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

"""Terrain style ``march``: the painted ground of 赤幡原 / The Red Field (dark remake A10; chapter1.md §15 "trampled mud,
ash-grey dead grass, scorched patches, wheel ruts, still black water with an oily sheen").

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/terrain_march.py
        [--no-previews] [--textures-only]

The plains painter (``terrain.py``, the web ``GroundPainter`` in tile space) run on ``Data/terrain_styles.json
themes.march`` (authored by the core lane with A1's reference ground: grass ``#4C4940`` L* 31, dirt ``#3A332D`` L* 22,
stone ``#55524C``, water ``#121418``, camp ``#47423C``), plus the march's **art extras** painted over the data layers
(the runtime samples only the textures, so the extras need no schema):

| Layer | Extras |
|---|---|
| grass (0) | ash flecks — pale ``#8A867A`` specks drifted on the dead grass (falling ash settles) |
| dirt (1) | **wheel ruts**: short paired grooves ``#2A231F`` with a lit lip ``#4A4138`` (cart tracks trampled into the mud, 1–1.6 tiles, varied angles so a road of dirt tiles does not stripe), hoof / boot scuffs |
| stone (2) | ash in the mortar joints (``#6A665C`` dusting on the slab edges) |
| camp (5) | charred board ends ``#26221F`` and ash drifts (packed ash round the hearth) |
| water (3) | the oily sheen: the data's ``wave`` flecks ``#3A3550`` (α .4) + slow iridescent smears ``#3A3E48``; **water override** (env art review): base ``#262A30`` (L* 17, the data's ``#121418`` read as a dead hole), patches ``#22262B`` / ``#2C3036``, shallows ``#30343A``, a faint grey sheen band ``#4E5258`` at the shore (``artExtras.waterOverride``; the runtime water reads ``Data/terrain_styles.json``: handed off to the core lane) |

Writes ``Art/Export/Terrain/``: ``T_Terrain_March_Grass/Dirt/Stone/Camp_BC``, ``T_Water_March_BC`` (1024², one 3 × 3
period, sRGB), ``T_Terrain_March_Details_BC`` (4 × 4 per-tile detail atlas, column 4 empty: the water props are the
``lily_pad_plains@march`` reed / scum meshes), ``T_Water_March_Mask_M`` (512² linear: R oily-sheen flecks, G foam
breakup, B patch mask); the lattice noise is theme-independent: the march section points at
``T_Terrain_Plains_Noise_M``. The UE inputs go to ``manifest.json → terrain.march`` (same schema as
``terrain.plains``; ``envkit.EnvManifest`` merges per key, the plains section is kept).

Previews (``Art/Previews/terrain_march/``): ``layers.png``, ``details.png``, ``transitions.png`` (the plains test map
composited with the march rules at the W1 camera with the warrior), ``water.png`` (the black pond and its oily edge).
"""
from __future__ import annotations

import copy
import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import numpy as np  # noqa: E402

import envkit as E  # noqa: E402
import terrain as T  # noqa: E402
from kit import paths, pngio  # noqa: E402

THEME = "march"
DATA = json.loads((paths.UNREAL_ROOT / "Data" / "terrain_styles.json").read_text())["themes"][THEME]
# Art overrides on the data style (painting only; the manifest records them): the data's second grass patch layer
# (#575247 / #2E2B27, r 4-10 px, 3 per tile) painted 27 near-black blobs per period — read as holes in the ground at
# 1080p and swallowed the shaded side of dark monsters (the darkgate's problem); the scorch keeps its place and shape
# at a mid-dark #3A3631 and two per tile.
ART_OVERRIDES = [
    ("0", 1, {"colors": [0x575247, 0x3A3631], "count": 2}),
]
# env art review (march): the data's tar-black water (#121418, L* 6) made every pond a dead hole with no surface read,
# and the reed slicks sat on it as slabs. The painted water is lifted to L* 17 (#262A30; its two patch tones a step
# either side), the shallows to L* 21.5 and the shore band to a faint grey sheen (#4E5258) — still black water next to
# the L* 22 mud, but a surface. Recorded under ``artExtras.waterOverride``; the runtime water surface (M_AF_Water)
# reads its colours from Data/terrain_styles.json (core lane): the same values are handed off for themes.march.
WATER_OVERRIDE = {"base": 0x262A30, "patch": [0x22262B, 0x2C3036],
                  "liquid": {"shallow": 0x30343A, "foam": 0x4E5258}}
MARCH = copy.deepcopy(DATA)
for _k, _i, _ov in ART_OVERRIDES:
    MARCH["ground"][_k]["layers"][_i].update(_ov)
MARCH["ground"]["3"]["base"] = WATER_OVERRIDE["base"]
next(l for l in MARCH["ground"]["3"]["layers"] if l["kind"] == "patch")["colors"] = list(WATER_OVERRIDE["patch"])
MARCH["ground"]["3"]["liquid"].update(WATER_OVERRIDE["liquid"])
PLAINS = T.STYLES
OUT = T.OUT
TEXTURES = {"0": "T_Terrain_March_Grass_BC", "1": "T_Terrain_March_Dirt_BC", "2": "T_Terrain_March_Stone_BC",
            "3": "T_Water_March_BC", "5": "T_Terrain_March_Camp_BC"}
DETAILS = "T_Terrain_March_Details_BC"
WATER_MASK = "T_Water_March_Mask_M"
NOISE = "T_Terrain_Plains_Noise_M"          # theme-independent lattice noise (AF_Terrain maths)
PREVIEW_DIR = "terrain_march"
SHEEN = 0x3A3E48                             # oily sheen on the black water (march_palettes MARCH_GROUND note)


class march_style:
    """Context: ``terrain.STYLES`` = the march style (every painter / ``composite`` of ``terrain.py`` reads it)."""

    def __enter__(self):
        T.STYLES = MARCH
        return MARCH

    def __exit__(self, *exc):
        T.STYLES = PLAINS
        return False


# ── art extras (painted over the data layers) ──────────────────────────────────────────────────────────
def _rand(seed: int, n: int = 12):
    r = T.mulberry32(seed)
    return r, [r() for _ in range(n)]


def extra_ash_flecks(cv: T.Canvas, seed: int, count: int = 44, color: int = 0x8A867A, light: int = 0xA8A294):
    """Falling ash settled on the dead grass: small pale specks, a few elongated (drifted)."""
    r = T.mulberry32(seed)
    for _ in range(count):
        u, v = r() * T.PERIOD, r() * T.PERIOD
        s = (0.5 + r() * 0.8) * T.SM
        rot = r() * math.pi
        c = color if r() < 0.7 else light
        cv.paint(u, v, s * 3, T.sdf_ellipse(s * (1.0 + r() * 1.4), s * 0.6, rot), T.rgb(c), 0.55 + r() * 0.3)


def extra_ruts(cv: T.Canvas, seed: int, groove: int = 0x2A231F, lip: int = 0x4A4138, mud: int = 0x332D28):
    """Wheel ruts: three short pairs of parallel grooves (cart tracks pressed into the mud, track 0.38 tile),
    1-1.6 tiles long, gently curved, at varied angles (a straight period-3 stripe would tile into rails along a
    road); each groove = a darker trough, a lit lip on its upper-left side (the key light) and a trampled band
    between the pair."""
    r = T.mulberry32(seed)
    for k in range(3):
        u0, v0 = (0.4 + k * 1.0 + r() * 0.6) % T.PERIOD, (0.3 + k * 0.95 + r() * 0.7) % T.PERIOD
        ang = (math.pi / 4) + (r() - 0.5) * 1.3 + (math.pi / 2 if k == 1 else 0.0)
        L = 1.0 + r() * 0.6
        bend = (r() - 0.5) * 0.35
        d = np.array([math.cos(ang), math.sin(ang)])
        nrm = np.array([-d[1], d[0]])
        gauge = 0.19
        base = np.array([0.0, 0.0])

        def path(off):
            a = base - d * L / 2 + nrm * off
            b_ = base + d * L / 2 + nrm * off
            c = (a + b_) / 2 + nrm * bend * L
            return T.quad_curve(a, c, b_, 9)
        # trampled band between the wheels (barely darker than the mud)
        band = path(0.0)
        cv.paint(u0, v0, L, T.sdf_stroke(band, [0.16 * (1 - abs(i / 8 - 0.5)) + 0.05 for i in range(9)]),
                 T.rgb(mud), 0.55)
        for side in (-1, 1):
            p = path(side * gauge)
            taper = [0.015 + 0.012 * math.sin(math.pi * i / 8) for i in range(9)]
            lift = np.array([-0.012, -0.016])          # the lit lip sits up-left of the trough (screen key light)
            cv.paint(u0, v0, L, T.sdf_stroke([q + lift for q in p], [t * 0.75 for t in taper]), T.rgb(lip), 0.6)
            cv.paint(u0, v0, L, T.sdf_stroke(p, taper), T.rgb(groove), 0.75)


def extra_scuffs(cv: T.Canvas, seed: int, color: int = 0x2E2824, count: int = 10):
    """Hoof and boot scuffs: small dark crescents trodden into the mud."""
    r = T.mulberry32(seed)
    for _ in range(count):
        u, v = r() * T.PERIOD, r() * T.PERIOD
        s = (1.2 + r() * 0.8) * T.SM
        rot = r() * math.tau
        cv.paint(u, v, s * 3, T.sdf_ellipse(s * 1.3, s * 0.75, rot), T.rgb(color), 0.6)
        cv.paint(u, v, s * 3, T.sdf_ellipse(s * 0.9, s * 0.45, rot, s * 0.25, -s * 0.15), T.rgb(0x3A332D), 0.8)


def extra_mortar_ash(cv: T.Canvas, seed: int, color: int = 0x6A665C, count: int = 26):
    """Ash dusted on the flagstones: pale smudges, mostly where it collects along the joints."""
    r = T.mulberry32(seed)
    for _ in range(count):
        u, v = r() * T.PERIOD, r() * T.PERIOD
        s = (2.0 + r() * 3.0) * T.SM
        cv.paint(u, v, s * 2, T.sdf_ellipse(s * (1.6 + r()), s * 0.5, r() * math.pi), T.rgb(color), 0.28)


def extra_char(cv: T.Canvas, seed: int, char: int = 0x26221F, ash: int = 0x5E5A52, count: int = 7):
    """Charred board ends and ash drifts on the packed camp ground."""
    r = T.mulberry32(seed)
    for _ in range(count):
        u, v = r() * T.PERIOD, r() * T.PERIOD
        L = (6 + r() * 7) * T.SM
        ang = r() * math.pi
        a = np.array([math.cos(ang), math.sin(ang)]) * L / 2
        cv.paint(u, v, L, T.sdf_stroke([-a, a], [1.0 * T.SM, 0.8 * T.SM]), T.rgb(char), 0.85)
    for _ in range(count):
        u, v = r() * T.PERIOD, r() * T.PERIOD
        s = (4 + r() * 5) * T.SM
        cv.paint(u, v, s * 2, T.sdf_blob(s, [r() for _ in range(12)], 0.7), T.rgb(ash), 0.35)


def extra_sheen(cv: T.Canvas, seed: int, color: int = SHEEN, count: int = 5):
    """Slow oily smears on the black water (the wave flecks of the data stay the brightest marks)."""
    r = T.mulberry32(seed)
    for _ in range(count):
        u, v = r() * T.PERIOD, r() * T.PERIOD
        L = (14 + r() * 12) * T.PX_TILE
        ang = math.pi / 4 + (r() - 0.5) * 0.8
        a = np.array([math.cos(ang), math.sin(ang)]) * L / 2
        c = np.array([-a[1], a[0]]) * 0.25
        pts = T.quad_curve(-a, c, a, 8)
        cv.paint(u, v, L, T.sdf_stroke(pts, [0.02 + 0.03 * math.sin(math.pi * i / 7) for i in range(8)]),
                 T.rgb(color), 0.45)


EXTRAS = {
    "0": lambda cv, s: extra_ash_flecks(cv, s),
    "1": lambda cv, s: (extra_scuffs(cv, s + 1), extra_ruts(cv, s)),
    "2": lambda cv, s: extra_mortar_ash(cv, s),
    "3": lambda cv, s: extra_sheen(cv, s),
    "5": lambda cv, s: extra_char(cv, s),
}


def paint_material(key: str, res: int = T.RES) -> np.ndarray:
    """``terrain.paint_material`` on the march style + the march extras (deterministic per key)."""
    st = MARCH["ground"][key]
    cv = T.Canvas(res, T.PERIOD, T.rgb(st["base"]))
    seed = 1013 + int(key) * 7907
    for i, layer in enumerate(st["layers"]):
        if layer["kind"] == "slabs":
            T.paint_slabs(cv, layer, st["base"], seed + i * 7919)
            continue
        T.paint_layer(cv, layer, T.features(layer, seed + i * 7919))
    EXTRAS[key](cv, 0xA10 * 31 + int(key) * 977)
    return cv.u8()


def build_textures(res: int = T.RES) -> dict:
    OUT.mkdir(parents=True, exist_ok=True)
    tex = {}
    with march_style():
        for key, name in TEXTURES.items():
            tex[key] = paint_material(key, res)
            pngio.write_png(OUT / f"{name}.png", tex[key])
            print(f"  {OUT / (name + '.png')}")
        det = T.paint_details(res)
        mask = T.paint_water_mask()
    pngio.write_png(OUT / f"{DETAILS}.png", det)
    pngio.write_png(OUT / f"{WATER_MASK}.png", mask)
    print(f"  {OUT / (DETAILS + '.png')}\n  {OUT / (WATER_MASK + '.png')}")
    return {"tex": tex, "details": det}


def load_textures() -> dict:
    tex = {k: pngio.read_png(OUT / f"{n}.png")[:, :, :3] for k, n in TEXTURES.items()}
    return {"tex": tex, "details": pngio.read_png(OUT / f"{DETAILS}.png")}


def composite(tiles: np.ndarray, layers: "T.Layers", ppt: int = 48, origin=(0, 0)) -> np.ndarray:
    """``terrain.composite`` with the march ranks, lips and liquid colours."""
    with march_style():
        return T.composite(tiles, layers, ppt=ppt, origin=origin)


# ── manifest ────────────────────────────────────────────────────────────────────────────────────────────
def manifest_section() -> dict:
    """``terrain.march``: the plains schema (``terrain.manifest_section``) with the march textures and colours."""
    with march_style():
        saved = dict(T.TEXTURES)
        T.TEXTURES.clear()
        T.TEXTURES.update(TEXTURES)
        try:
            sec = copy.deepcopy(T.manifest_section()["plains"])
        finally:
            T.TEXTURES.clear()
            T.TEXTURES.update(saved)
    g = MARCH["ground"]
    hexs = lambda c: "#%06X" % c  # noqa: E731
    wave = next(l for l in g["3"]["layers"] if l["kind"] == "wave")
    sec["source"] = ("Data/terrain_styles.json themes.march (core lane, chapter1.md §15; the runtime Tile<i>* vectors) "
                     "+ the march art extras painted into the textures (terrain_march.py)")
    sec["theme"] = THEME
    sec["zones"] = ["emerald_plains"]
    sec["details"]["texture"] = DETAILS
    sec["details"]["file"] = f"Terrain/{DETAILS}.png"
    sec["details"]["cells"] = sec["details"]["cells"].replace(
        "instanced SM_Foliage_Plains_LilyPad_A/B meshes only", "instanced lily_pad_plains@march meshes (dead reeds, "
        "oily scum: SM_Foliage_March_Reeds_A/B)")
    sec["noise"]["texture"] = NOISE
    sec["noise"]["file"] = f"Terrain/{NOISE}.png"
    sec["noise"]["shared"] = "theme-independent (the AF_Terrain lattice / patch fields): the plains noise texture"
    w = sec["water"]
    w["texture"] = TEXTURES["3"]
    w["wave"] = hexs(wave["color"])
    w["waveAlpha"] = wave.get("alpha", 0.7)
    w["sheen"] = hexs(SHEEN)
    w["mask"]["texture"] = WATER_MASK
    w["mask"]["file"] = f"Terrain/{WATER_MASK}.png"
    w["mask"]["channels"]["r"] = (f"oily sheen flecks ({hexs(wave['color'])} x {wave.get('alpha', 0.7)}), scroll UV0/3 "
                                  f"+ t*(.012,.008): still water (a quarter of the plains drift)")
    w["mask"]["channels"]["b"] = "patch mask (" + "/".join(hexs(c) for c in next(
        l for l in g["3"]["layers"] if l["kind"] == "patch")["colors"]) + ")"
    w["lilyPads"] = ("lily_pad_plains@march: SM_Foliage_March_Reeds_A/B (dead reeds, oily scum) on the same ~1 in 6 "
                     "water tiles")
    w["note"] = "still black water with an oily sheen (chapter1.md §15): no bright foam, the bank is wet mud"
    sec["artExtras"] = {
        "grass": "ash flecks #8A867A / #A8A294 (falling ash settled on the dead grass)",
        "dirt": "wheel ruts (paired grooves #2A231F, lit lip #4A4138, trampled band #332D28; 1-1.6 tiles at varied "
                "angles) and hoof / boot scuffs",
        "stone": "ash dusting #6A665C on the flagstones",
        "camp": "charred board ends #26221F and ash drifts #5E5A52",
        "water": f"slow oily smears {hexs(SHEEN)} under the data's sheen flecks",
        "overrides": [{"layer": f"ground.{k}.layers[{i}]", "set": {kk: (["#%06X" % c for c in vv] if kk == "colors"
                                                                         else vv) for kk, vv in ov.items()}}
                      for k, i, ov in ART_OVERRIDES],
        "overridesWhy": "the data's near-black #2E2B27 scorch patches (27 per period) read as holes at 1080p and hid "
                        "the shaded side of dark monsters; same place and shape at #3A3631, two per tile",
        "rule": "painted into the BC textures only (the runtime samples textures; Data/terrain_styles.json is the "
                "core lane's and stays the colour source of every data layer)",
        "waterOverride": {
            "set": {"ground.3.base": hexs(WATER_OVERRIDE["base"]),
                    "ground.3.layers[patch].colors": [hexs(c) for c in WATER_OVERRIDE["patch"]],
                    "ground.3.liquid.shallow": hexs(WATER_OVERRIDE["liquid"]["shallow"]),
                    "ground.3.liquid.foam": hexs(WATER_OVERRIDE["liquid"]["foam"])},
            "was": {"ground.3.base": hexs(DATA["ground"]["3"]["base"]),
                    "ground.3.layers[patch].colors": [hexs(c) for c in next(
                        l for l in DATA["ground"]["3"]["layers"] if l["kind"] == "patch")["colors"]],
                    "ground.3.liquid.shallow": hexs(DATA["ground"]["3"]["liquid"]["shallow"]),
                    "ground.3.liquid.foam": hexs(DATA["ground"]["3"]["liquid"]["foam"])},
            "why": "env art review: the tar-black water (L* 6) read as a dead hole; lifted to L* 17 with L* 21.5 "
                   "shallows and a faint grey sheen band (#4E5258) at the shore",
            "runtime": "HANDOFF core lane: M_AF_Water / the terrain's WaterBase/Shallow/Foam come from "
                       "Data/terrain_styles.json themes.march.ground.3 — set these values there (the painted "
                       "T_Water_March_BC and the previews already use them)"}}
    sec["previews"] = [f"Previews/{PREVIEW_DIR}/{n}.png" for n in ("layers", "details", "transitions", "water")]
    sec["params"] = {f"Tile{k}{f}": v for k in ("0", "1", "2", "3", "5") for f, v in
                     (("Base", hexs(g[k]["base"])), ("Rank", g[k]["rank"]))}
    if "lip" in g["0"]:
        sec["params"]["Tile0Lip"] = hexs(g["0"]["lip"])
    return {THEME: sec}


# ── previews ────────────────────────────────────────────────────────────────────────────────────────────
def test_map() -> np.ndarray:
    """The plains test map (camp + palisade corner, path, pond, stone, walls) plus a second dirt road."""
    m = T.test_map()
    for c in range(1, 9):
        m[8 + (c > 5), c] = 1 if m[8 + (c > 5), c] != 3 else 3
    return m


def render_ground(img: np.ndarray, shape, out: Path) -> None:
    """The composited march test map at the W1 camera (default distance, native 1080p crops) with the warrior on the
    camp ground (``terrain.render_ground`` with march labels)."""
    import bpy
    from mathutils import Vector
    from kit import outline, review, scene
    scene.reset()
    rows, cols = shape
    E.SCRATCH.mkdir(parents=True, exist_ok=True)
    p = E.SCRATCH / "terrain_march_ground.png"
    pngio.write_png(p, img)
    me = bpy.data.meshes.new("ground")
    x0, x1 = -0.5, cols - 0.5
    y0, y1 = 0.5, -(rows - 0.5)
    me.from_pydata([(x0, y0, 0), (x1, y0, 0), (x1, y1, 0), (x0, y1, 0)], [], [(0, 1, 2, 3)])
    uv = me.uv_layers.new(name="UVMap")
    for li, (u, v) in enumerate(((0, 1), (1, 1), (1, 0), (0, 0))):
        uv.data[li].uv = (u, v)
    ob = bpy.data.objects.new("ground", me)
    scene.link(ob)
    mat = bpy.data.materials.new("ground")
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
    w = E.import_warrior(at=(3.5, -3.0, 0), yaw_deg=-135)
    review.setup_render((1920, 1080), 8)
    cam = review.game_camera((cols / 2 - 0.5, -(rows / 2 - 0.5)))
    if w:
        for mm in w[1]:
            review.set_outline_px([mm], outline.SCREEN_PX_1080[mm.get("af_outline_class", "hero")])
    full = review.render_array()

    def crop_at(col, row, w_, h_):
        x, y = review.project_px(cam, Vector((col, -row, 0.0)), (1920, 1080))
        xa = int(min(max(x - w_ / 2, 0), 1920 - w_))
        ya = int(min(max(y - h_ / 2, 0), 1080 - h_))
        return full[ya:ya + h_, xa:xa + w_].copy()

    def labelled(text):
        def lab(t, i, k):
            review.draw_text(t, 6, 6, text + (f"  TILE {i + 1}/{k}" if k > 1 else ""))
        return lab
    tr = crop_at(cols / 2 - 0.5, rows / 2 - 0.5, 1040, 600)
    pngio.write_png_native(out / "transitions.png", tr, annotate=labelled(
        "TERRAIN MARCH TEST MAP: CAMP ASH, MUD ROADS + RUTS, BLACK POND, FLAGSTONE, WALL GROUND  W1 1080P 1:1"))
    wa = crop_at(4.0, 9.2, 640, 400)
    pngio.write_png_native(out / "water.png", wa, annotate=labelled(
        "BLACK WATER: OILY SHEEN, WET-MUD BANK, SCUM EDGE  1080P 1:1"))
    print(f"  {out / 'transitions.png'}")


def previews(tex: dict, out: Path | None = None) -> None:
    import envkit  # noqa: F401
    from kit import review
    out = out or paths.preview_root() / PREVIEW_DIR
    out.mkdir(parents=True, exist_ok=True)
    cells = []
    for key in ("0", "1", "2", "5", "3"):
        t = np.tile(tex["tex"][key], (2, 2, 1))[::4, ::4].copy()
        review.draw_text(t, 4, 4, f"{TEXTURES[key]}  2X2 PERIODS (6 TILES)  TEXELS AT 1/4")
        cells.append(t)
    E.save_native(out / "layers.png", review.grid(cells, 3, pad=2))
    det = tex["details"]
    bg = np.zeros_like(det[:, :, :3])
    bg[:, :] = (40, 36, 34)
    a = det[:, :, 3:4] / 255.0
    comp = (det[:, :, :3] * a + bg * (1 - a)).astype(np.uint8)[::2, ::2].copy()
    review.draw_text(comp, 4, 4, "MARCH DETAILS AT 1/2: ROWS GRASS DIRT STONE CAMP / COLS VARIANT 1-3 (COL 4 EMPTY)")
    E.save_native(out / "details.png", comp)
    layers = T.Layers(tex["tex"], tex["details"])
    m = test_map()
    img = composite(m, layers, ppt=64)
    pngio.write_png(E.SCRATCH / "terrain_march_composite.png", img)
    render_ground(img, m.shape, out)


def main(argv: list[str]) -> int:
    if "--scratch" in argv:                 # iteration: textures + previews into scratch, nothing shipped
        global OUT
        OUT = E.SCRATCH / "terrain_march"
        tex = build_textures()
        previews(tex, E.SCRATCH / "terrain_march")
        return 0
    tex = build_textures()
    if "--textures-only" in argv:
        return 0
    man = E.EnvManifest()
    man.data.setdefault("terrain", {}).update(manifest_section())
    man.save()
    print(f"  manifest terrain.{THEME} written")
    if "--no-previews" not in argv:
        previews(tex)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

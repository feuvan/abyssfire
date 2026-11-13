"""Terrain style ``hangwood``: the ground of 绳林 / the Hanging Wood (Chapter 2, package 4R-1 — the first cut, made so
the darkest-tile gate has the theme's own ground to stand the Moon-Bitten on; chapter2.md §15 "Terrain style
``hangwood``: leaf mould, root ridges, wet stone, black pond water with a silver-green sheen"; §4.4 "the rig must pass
the darkest-tile gate on the ``hangwood`` root tile").

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/terrain_hangwood.py
        [--no-previews] [--textures-only] [--scratch]

The plains painter (``terrain.py``) run on :data:`HANGWOOD_STYLE` — the schema of ``Data/terrain_styles.json
themes.<name>``. **The style is authored here** (the art lane's proposal, recorded under
``terrain.hangwood.dataStyle``); the runtime's per-tile colour vectors come from ``Data/terrain_styles.json``, so the
content lane copies it to ``GameData`` as ``themes.hangwood`` (HANDOFF 4A). The generator theme of the zone stays
``forest``; the paint keys keep the map's tile types:

| Key (tile type) | Material | Look | L* |
|---|---|---|---|
| 0 (grass) | ``leafmould`` | wet dead leaves over black soil: dull ochre / rust leaf flecks, pale fungus specks | ~24 |
| 1 (dirt) | ``rootearth`` | root-ribbed black earth: long root ridges with a lit lip, hollows — **the darkest walkable tile** (the "root tile": darkgate's ground) | ~19-20 |
| 2 (stone) | ``wetstone`` | wet flags and outcrop: cool grey slabs, moss in the joints | ~26 |
| 5 (camp) | ``yard`` | the mill yard: trodden earth, chaff | ~26 |
| 3 (water) | ``pond`` | black pond water, a silver-green sheen | ~5 |

**How dark.** As the Pit's floor (``terrain_pit.py``): the values are set by the darkest-tile gate, not by taste. The
root earth sits at L* ~19-20, the level of the march mud and the Pit's dug earth — a near-black body (the fenbound's
tarnish-black, a moon-tarnish core #0C1614) still separates from it by being *darker* (ΔL* ≥ 15 needs the ground
above ~18.5) while the slate hound's lit side clears it by 30+. The wood's darkness in game is the zone mood (§15:
ambient .34, vignette .62, light scale .36), not the texture.

Writes ``Art/Export/Terrain/``: ``T_Terrain_Hangwood_LeafMould/RootEarth/WetStone/Yard_BC``, ``T_Water_Hangwood_BC``
(1024², one 3 × 3 period, sRGB), ``T_Terrain_Hangwood_Details_BC``, ``T_Water_Hangwood_Mask_M``; the lattice noise is
the plains texture. ``manifest.json → terrain.hangwood`` has the plains schema. Previews
(``Art/Previews/terrain_hangwood/``): ``layers.png``, ``details.png``, ``transitions.png``. 4R-3 owns the style from
here (the camp kit, foliage and set pieces are judged on it).
"""
from __future__ import annotations

import copy
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import numpy as np  # noqa: E402

import envkit as E  # noqa: E402
import terrain as T  # noqa: E402
import terrain_march as TM  # noqa: E402
from kit import paths, pngio  # noqa: E402

THEME = "hangwood"
NAMES = {"0": "leafmould", "1": "rootearth", "2": "wetstone", "3": "pond", "5": "yard"}
HANGWOOD_STYLE = {
    "ground": {
        "0": {"base": 0x39382D, "rank": 4, "lip": 0x4A4838,
              "layers": [{"kind": "patch", "count": 4, "colors": [0x3D3B2F, 0x333229, 0x403E2E], "r": [14, 28]},
                         {"kind": "patch", "count": 3, "colors": [0x2C2B24, 0x44412E], "r": [5, 10]},
                         {"kind": "leaf", "count": 16, "colors": [0x5A4A2C, 0x4E3A26, 0x5C5434, 0x443A2A],
                          "size": [2.2, 3.6]},
                         {"kind": "pebble", "count": 3, "color": 0x4A4A40, "shade": 0x24241E, "light": 0x5E5E52,
                          "size": [1.0, 1.7]}],
              "details": ["stones", "bigTuft", "stones"], "accents": [0xA8A894, 0x8C8C78]},
        "1": {"base": 0x39332C, "rank": 1,
              "layers": [{"kind": "patch", "count": 4, "colors": [0x352F29, 0x3E3831, 0x322D27], "r": [10, 22]},
                         {"kind": "crack", "count": 2, "color": 0x1C1815, "light": 0x443C33, "len": [8, 14],
                          "width": 2},
                         {"kind": "leaf", "count": 5, "colors": [0x4A3C28, 0x433627], "size": [2.0, 3.0]},
                         {"kind": "pebble", "count": 5, "color": 0x3E3830, "shade": 0x201C19, "light": 0x4E463C,
                          "size": [1.0, 2.0]}],
              "details": ["stones", "crackStar", "stones"], "accents": [0x4E463C]},
        "2": {"base": 0x3D4242, "rank": 2,
              "layers": [{"kind": "slabs", "cells": 2, "mortar": 0x1E2320, "mortarW": 0.04, "jitter": 0.09,
                          "bevelLight": 0x4E5454, "bevelShade": 0x2E3232},
                         {"kind": "patch", "count": 4, "colors": [0x363C3A, 0x444A48], "r": [4, 9]}],
              "details": ["crackStar", "stones", "crackStar"], "accents": [0x4E5A48]},
        "3": {"base": 0x0E1414, "rank": 0,
              "layers": [{"kind": "patch", "count": 3, "colors": [0x0B1010, 0x121A19], "r": [14, 26]},
                         {"kind": "wave", "count": 4, "color": 0x4A6A60, "len": [6, 11], "alpha": 0.40}],
              "details": ["stones"], "accents": [0x2A3A34],
              "liquid": {"shallow": 0x162020, "foam": 0x2E3A38, "bank": 0x1C1A16}},
        "5": {"base": 0x443D33, "rank": 2,
              "layers": [{"kind": "patch", "count": 4, "colors": [0x3E382F, 0x4A4337, 0x40392E], "r": [10, 20]},
                         {"kind": "pebble", "count": 6, "color": 0x504A3E, "shade": 0x28241F, "light": 0x645C4E,
                          "size": [1.0, 1.9]},
                         {"kind": "crack", "count": 1, "color": 0x26221D, "light": 0x524A3E, "len": [7, 12],
                          "width": 2}],
              "details": ["straw", "stones", "straw"], "accents": [0x8A7E5A, 0x6E6448]},
    },
    "outcrop": {"kind": "rootKnuckle", "rock": 0x444848, "cap": 0x2E3A30, "accent": 0x5A6A58, "extra": 0x363A38,
                "shadow": 0x0C0E0E},
    "palisade": {"kind": "wattle", "wood": 0x3E342A, "band": 0x5A4E3E, "accent": 0x8A7E62},
}
PLAINS = T.STYLES
OUT = T.OUT
TEXTURES = {"0": "T_Terrain_Hangwood_LeafMould_BC", "1": "T_Terrain_Hangwood_RootEarth_BC",
            "2": "T_Terrain_Hangwood_WetStone_BC", "3": "T_Water_Hangwood_BC", "5": "T_Terrain_Hangwood_Yard_BC"}
DETAILS = "T_Terrain_Hangwood_Details_BC"
WATER_MASK = "T_Water_Hangwood_Mask_M"
NOISE = "T_Terrain_Plains_Noise_M"
PREVIEW_DIR = "terrain_hangwood"


class hangwood_style:
    """Context: ``terrain.STYLES`` = the hangwood style (every painter / ``composite`` of ``terrain.py`` reads it)."""

    def __enter__(self):
        T.STYLES = HANGWOOD_STYLE
        return HANGWOOD_STYLE

    def __exit__(self, *exc):
        T.STYLES = PLAINS
        return False


# ── art extras (painted over the style's layers; the runtime samples the textures) ──────────────────────────
def extra_roots(cv: T.Canvas, seed: int, wood: int = 0x3C352D, lip: int = 0x574C40, shade: int = 0x191613,
                count: int = 7):
    """Root ridges breaking the surface: long sinuous strokes that fork once, a hard shadow on the lower-right side
    and a lit lip on the upper-left (the key light), thicker where they leave the ground. The motif of the tile the
    chapter calls the root tile; kept low in contrast (ΔL* ≈ ±8 round the base) so the tile's median stays put."""
    r = T.mulberry32(seed)
    for _ in range(count):
        u0, v0 = r() * T.PERIOD, r() * T.PERIOD
        ang = r() * math.tau
        L = 0.55 + r() * 0.55
        n = 7
        pts, a = [np.zeros(2)], ang
        for k in range(n):
            a += (r() - 0.5) * 0.9
            pts.append(pts[-1] + np.array([math.cos(a), math.sin(a)]) * L / n)
        w0 = 0.028 + r() * 0.018
        widths = [w0 * (1.0 - 0.75 * abs(k / n - 0.4)) for k in range(n + 1)]
        sh = np.array([0.012, 0.016])
        lt = np.array([-0.007, -0.009])
        cv.paint(u0, v0, L * 1.4, T.sdf_stroke([p + sh for p in pts], [w * 1.15 for w in widths]), T.rgb(shade), 0.75)
        cv.paint(u0, v0, L * 1.4, T.sdf_stroke(pts, widths), T.rgb(wood), 0.95)
        cv.paint(u0, v0, L * 1.4, T.sdf_stroke([p + lt for p in pts], [w * 0.38 for w in widths]), T.rgb(lip), 0.8)
        if r() < 0.7:                                        # one fork
            k0 = 2 + int(r() * 3)
            b, fa = [pts[k0]], a + (1.0 if r() < 0.5 else -1.0) * (0.7 + r() * 0.5)
            for k in range(4):
                fa += (r() - 0.5) * 0.7
                b.append(b[-1] + np.array([math.cos(fa), math.sin(fa)]) * L * 0.09)
            bw = [widths[k0] * 0.7 * (1.0 - 0.2 * k) for k in range(5)]
            cv.paint(u0, v0, L * 1.4, T.sdf_stroke([p + sh for p in b], [w * 1.15 for w in bw]), T.rgb(shade), 0.7)
            cv.paint(u0, v0, L * 1.4, T.sdf_stroke(b, bw), T.rgb(wood), 0.95)
            cv.paint(u0, v0, L * 1.4, T.sdf_stroke([p + lt for p in b], [w * 0.38 for w in bw]), T.rgb(lip), 0.75)


def extra_fungus(cv: T.Canvas, seed: int, color: int = 0x9C9C88, count: int = 12):
    """Pale fungus specks in twos and threes (chapter2.md §15 "pale fungus"): the only light motif of the floor."""
    r = T.mulberry32(seed)
    for _ in range(count):
        u, v = r() * T.PERIOD, r() * T.PERIOD
        for _k in range(2 + int(r() * 2)):
            s = (0.9 + r() * 0.9) * T.SM
            cv.paint(u + (r() - 0.5) * 0.09, v + (r() - 0.5) * 0.09, s * 2, T.sdf_ellipse(s, s * 0.8, r() * 3.0),
                     T.rgb(color), 0.85)


def extra_moss(cv: T.Canvas, seed: int, color: int = 0x34402F, count: int = 22):
    """Moss in the joints and corners of the wet stone: small soft blobs."""
    r = T.mulberry32(seed)
    for _ in range(count):
        u, v = r() * T.PERIOD, r() * T.PERIOD
        s = (3 + r() * 5) * T.SM
        cv.paint(u, v, s * 2, T.sdf_blob(s, [r() for _ in range(12)], 0.8), T.rgb(color), 0.55)


EXTRAS = {
    "0": lambda cv, s: (extra_roots(cv, s + 11, count=2), extra_fungus(cv, s)),
    "1": lambda cv, s: (TM.extra_scuffs(cv, s + 1, color=0x25211D, count=6), extra_roots(cv, s),
                        extra_fungus(cv, s + 5, color=0x7E7E6C, count=4)),
    "2": lambda cv, s: (extra_moss(cv, s), TM.extra_sheen(cv, s + 3, color=0x525A58, count=4)),
    "3": lambda cv, s: TM.extra_sheen(cv, s, color=0x2E4A42, count=5),
    "5": lambda cv, s: TM.extra_scuffs(cv, s, color=0x2E2924, count=9),
}


def paint_material(key: str, res: int = T.RES) -> np.ndarray:
    st = HANGWOOD_STYLE["ground"][key]
    cv = T.Canvas(res, T.PERIOD, T.rgb(st["base"]))
    seed = 5521 + int(key) * 7907
    for i, layer in enumerate(st["layers"]):
        if layer["kind"] == "slabs":
            T.paint_slabs(cv, layer, st["base"], seed + i * 7919)
            continue
        T.paint_layer(cv, layer, T.features(layer, seed + i * 7919))
    EXTRAS[key](cv, 0xB17 * 31 + int(key) * 977)
    return cv.u8()


def build_textures(res: int = T.RES, out: Path | None = None) -> dict:
    out = out or OUT
    out.mkdir(parents=True, exist_ok=True)
    tex = {}
    with hangwood_style():
        for key, name in TEXTURES.items():
            tex[key] = paint_material(key, res)
            pngio.write_png(out / f"{name}.png", tex[key])
            print(f"  {out / (name + '.png')}")
        det = T.paint_details(res)
        mask = T.paint_water_mask()
    pngio.write_png(out / f"{DETAILS}.png", det)
    pngio.write_png(out / f"{WATER_MASK}.png", mask)
    return {"tex": tex, "details": det}


def load_textures(out: Path | None = None) -> dict:
    out = out or OUT
    tex = {k: pngio.read_png(out / f"{n}.png")[:, :, :3] for k, n in TEXTURES.items()}
    return {"tex": tex, "details": pngio.read_png(out / f"{DETAILS}.png")}


def composite(tiles: np.ndarray, layers: "T.Layers", ppt: int = 48, origin=(0, 0)) -> np.ndarray:
    """``terrain.composite`` with the hangwood ranks, lips and liquid colours."""
    with hangwood_style():
        return T.composite(tiles, layers, ppt=ppt, origin=origin)


# ── manifest ────────────────────────────────────────────────────────────────────────────────────────────
def _hex_style(o):
    """The style with 0xRRGGBB ints as ``#RRGGBB`` (review / handoff copy)."""
    if isinstance(o, dict):
        return {k: _hex_style(v) for k, v in o.items()}
    if isinstance(o, list):
        return [_hex_style(v) for v in o]
    if isinstance(o, int) and not isinstance(o, bool) and o > 4096:
        return "#%06X" % o
    return o


def manifest_section() -> dict:
    """``terrain.hangwood``: the plains schema (``terrain.manifest_section``) with the hangwood textures and colours."""
    with hangwood_style():
        saved = dict(T.TEXTURES)
        T.TEXTURES.clear()
        T.TEXTURES.update(TEXTURES)
        try:
            sec = copy.deepcopy(T.manifest_section()["plains"])
        finally:
            T.TEXTURES.clear()
            T.TEXTURES.update(saved)
    g = HANGWOOD_STYLE["ground"]
    hexs = lambda c: "#%06X" % c  # noqa: E731
    wave = next(l for l in g["3"]["layers"] if l["kind"] == "wave")
    for k, lay in sec["layers"].items():
        lay["tile"] = lay["name"]                           # the map's tile type this key paints
        lay["name"] = NAMES[k]
    sec["source"] = ("environment/terrain_hangwood.py HANGWOOD_STYLE (the art lane's style: chapter2.md 4R-1, a first "
                     "cut for the darkest-tile gate; 4R-3 owns it from here). HANDOFF content lane (4A): copy "
                     "dataStyle to GameData as terrain_styles themes.hangwood (the runtime Tile<i>* vectors, ranks, "
                     "lips and liquid colours come from Data/terrain_styles.json)")
    sec["theme"] = THEME
    sec["zones"] = ["twilight_forest"]
    sec["dataStyle"] = _hex_style(HANGWOOD_STYLE)
    sec["details"]["texture"] = DETAILS
    sec["details"]["file"] = f"Terrain/{DETAILS}.png"
    sec["details"]["cells"] = ("4 x 4 cells of one tile: row 0 leafmould, 1 rootearth, 2 wetstone, 3 yard (paint 5); "
                               "column = variant - 1 (variants 1..3); column 3 (pond) is transparent")
    sec["noise"]["texture"] = NOISE
    sec["noise"]["file"] = f"Terrain/{NOISE}.png"
    sec["noise"]["shared"] = "theme-independent (the AF_Terrain lattice / patch fields): the plains noise texture"
    w = sec["water"]
    w["texture"] = TEXTURES["3"]
    w["wave"] = hexs(wave["color"])
    w["waveAlpha"] = wave.get("alpha", 0.7)
    w["mask"]["texture"] = WATER_MASK
    w["mask"]["file"] = f"Terrain/{WATER_MASK}.png"
    w["mask"]["channels"]["r"] = (f"silver-green sheen ({hexs(wave['color'])} x {wave.get('alpha', 0.7)}), scroll "
                                  f"UV0/3 + t*(.012,.008): still water")
    w["lilyPads"] = "none in this cut (4R-3 decides: the forest theme's lily pad ids resolve to <id>@hangwood first)"
    w["note"] = "black pond water with a silver-green sheen (chapter2.md §15)"
    sec["materials"] = {
        "0": "leafmould: wet dead leaves over black soil; also what wall tiles paint (rank 4 wins the edge)",
        "1": "rootearth: root-ribbed black earth — the darkest walkable tile (the 'root tile' of chapter2.md §4.4; "
             "kept at L* ~19-20: a near-black body must stay >= 15 L* darker than any tile it stands on, and the "
             "slate hound's lit side clears it by 30+)",
        "2": "wetstone: wet flags and outcrop, moss in the joints",
        "5": "yard: the mill yard's trodden earth and chaff",
        "3": "pond: black water"}
    sec["artExtras"] = {
        "leafmould": "dead-leaf flecks (the style's leaf layer), two root ridges, pale fungus specks #9C9C88",
        "rootearth": "root ridges (wood #3C352D, lit lip #574C40, shadow #191613), scuffs, a few fungus specks",
        "wetstone": "moss #34402F, a wet sheen #525A58",
        "yard": "scuffs",
        "rule": "painted into the BC textures only (the runtime samples textures)"}
    sec["previews"] = [f"Previews/{PREVIEW_DIR}/{n}.png" for n in ("layers", "details", "transitions")]
    sec["params"] = {f"Tile{k}{f}": v for k in ("0", "1", "2", "3", "5") for f, v in
                     (("Base", hexs(g[k]["base"])), ("Rank", g[k]["rank"]))}
    sec["params"]["Tile0Lip"] = hexs(g["0"]["lip"])
    return {THEME: sec}


# ── previews ────────────────────────────────────────────────────────────────────────────────────────────
def test_map() -> np.ndarray:
    """A 14 × 12 test clearing: leaf mould with a run of root earth through it, a wet-stone ruin corner, the mill
    yard, a pond; wall tiles (4) where the oaks' trunks and root knuckles stand."""
    m = np.zeros((12, 14), np.int32)
    m[0, :] = 4
    m[:, 0] = 4
    m[3:9, 3:8] = 1                                          # the root earth (the Wolf Run's floor)
    m[5:7, 8:11] = 1
    m[1:4, 9:13] = 2                                         # a ruin's flags
    m[8:11, 9:13] = 5                                        # a yard
    m[9:11, 1:4] = 3                                         # a pond
    m[4, 5] = 4                                              # a trunk
    return m


def render_ground(img: np.ndarray, shape, out: Path) -> None:
    """The composited test clearing at the W1 camera (native 1080p crop) with the warrior on the root earth; wall
    tiles are drawn as flat black tops (trunks and root knuckles stand there in game: 4R-3)."""
    import bpy
    from mathutils import Vector
    from kit import outline, review, scene
    scene.reset()
    rows, cols = shape
    E.SCRATCH.mkdir(parents=True, exist_ok=True)
    p = E.SCRATCH / "terrain_hangwood_ground.png"
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
    w = E.import_warrior(at=(5.0, -6.0, 0), yaw_deg=-135)
    review.setup_render((1920, 1080), 8)
    cam = review.game_camera((cols / 2 - 0.5, -(rows / 2 - 0.5)))
    if w:
        for mm in w[1]:
            review.set_outline_px([mm], outline.SCREEN_PX_1080[mm.get("af_outline_class", "hero")])
    full = review.render_array()
    x, y = review.project_px(cam, Vector((cols / 2 - 0.5, -(rows / 2 - 0.5), 0.0)), (1920, 1080))
    w_, h_ = 1100, 640
    xa = int(min(max(x - w_ / 2, 0), 1920 - w_))
    ya = int(min(max(y - h_ / 2, 0), 1080 - h_))
    tr = full[ya:ya + h_, xa:xa + w_].copy()
    for pth, s_ in E.write_banded(out / "transitions.png", tr,
                                  "TERRAIN HANGWOOD TEST CLEARING: LEAF MOULD, ROOT EARTH, WET STONE, YARD, POND  "
                                  "W1 1080P 1:1  NO ZONE MOOD"):
        print(f"  {pth}  {s_ // 1024} KB")


def previews(tex: dict, out: Path | None = None) -> None:
    from kit import review
    out = out or paths.preview_root() / PREVIEW_DIR
    out.mkdir(parents=True, exist_ok=True)
    cells = []
    for key in ("1", "0", "2", "5", "3"):
        t = np.tile(tex["tex"][key], (2, 2, 1))[::4, ::4].copy()
        review.draw_text(t, 4, 4, f"{TEXTURES[key]}  ({NAMES[key].upper()})  2X2 PERIODS  TEXELS AT 1/4")
        cells.append(t)
    E.save_native(out / "layers.png", review.grid(cells, 3, pad=2))
    det = tex["details"]
    bg = np.zeros_like(det[:, :, :3])
    bg[:, :] = (47, 44, 41)
    a = det[:, :, 3:4] / 255.0
    comp = (det[:, :, :3] * a + bg * (1 - a)).astype(np.uint8)[::2, ::2].copy()
    review.draw_text(comp, 4, 4, "HANGWOOD DETAILS AT 1/2: ROWS LEAFMOULD ROOTEARTH WETSTONE YARD / COLS VARIANT 1-3")
    E.save_native(out / "details.png", comp)
    layers = T.Layers(tex["tex"], tex["details"])
    m = test_map()
    img = composite(m, layers, ppt=64)
    # wall tiles: flat black (trunks / root knuckles stand there in game)
    for r in range(m.shape[0]):
        for c in range(m.shape[1]):
            if m[r, c] == 4:
                img[r * 64:(r + 1) * 64, c * 64:(c + 1) * 64] = (19, 17, 16)
    render_ground(img, m.shape, out)


def stats() -> dict:
    """Median L* of each walkable material (an 8 x 8 composited field), as darkgate measures the ground."""
    tex = load_textures()
    layers = T.Layers(tex["tex"], tex["details"])
    out = {}
    for k in ("0", "1", "2", "5"):
        img = composite(np.full((8, 8), int(k), np.int32), layers, ppt=48).reshape(-1, 3).astype(np.float32) / 255.0
        lin = np.where(img <= 0.04045, img / 12.92, ((img + 0.055) / 1.055) ** 2.4)
        y = lin @ np.array([0.2126, 0.7152, 0.0722], np.float32)
        L = 116.0 * np.where(y > 216 / 24389, np.cbrt(y), (24389 / 27 * y + 16) / 116) - 16.0
        out[NAMES[k]] = round(float(np.median(L)), 1)
    return out


def main(argv: list[str]) -> int:
    if "--scratch" in argv:
        d = E.SCRATCH / "terrain_hangwood"
        tex = build_textures(out=d)
        previews(tex, d)
        return 0
    tex = build_textures()
    print("  median L* per material:", stats())
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

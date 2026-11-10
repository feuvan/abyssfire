"""Terrain style ``pit``: the floor of 信者之坑 / the Pit of the Faithful (dark remake Wave 3, A13 / 3C; chapter1.md
§9.2: "the hero's light radius is the main light" — the darkest ground of the chapter).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/environment/terrain_pit.py
        [--no-previews] [--textures-only] [--scratch]

The plains painter (``terrain.py``, the web ``GroundPainter`` in tile space) run on :data:`PIT_STYLE` — the same
schema as ``Data/terrain_styles.json themes.<name>``. **The style is authored here** (the art lane's proposal: the
manifest records it under ``terrain.pit.dataStyle``); the runtime's per-tile colour vectors come from
``Data/terrain_styles.json``, so the content lane copies it to ``GameData`` as ``themes.pit`` (HANDOFF, see the
manifest section's ``source``). The paint keys keep the map's tile types; in a crypt they mean:

| Key (tile type) | Pit material | Look | L* |
|---|---|---|---|
| 0 (grass) | ``ash`` | ash drifted deep over the flags: pale-on-dark patches, flecks (the Ash Steps; and what wall tiles paint: it spills a hand's width out from every wall's foot) | 24 |
| 1 (dirt) | ``earth`` | dug grave earth: spade cuts with a lit lip, clods (the Pitwarden's Cut) — **the darkest walkable tile** (darkgate's ground) | 20 |
| 2 (stone) | ``flagstone`` | big crypt flags, black joints, ash in the joints, soot (the default floor) | 22 |
| 5 (camp) | ``paving`` | smaller dressed paving, ash dusted (the Flame Niche) | 24 |
| 3 (water) | ``seep`` | still black water with a faint breach sheen (unused by the first map; the set stays complete) | 5 |

**How dark.** The floor's values are set by the darkest-tile gate, not by taste: the breach clot is a black tar body
(lit side L* 3.2) that separates from the ground by being *darker* than it, so a walkable tile below L* ~18.5 hides it
(ΔL* < 15; the first earth, L* 12.1, failed it at ΔL* 8.9). The darkest tile sits at the march dirt's level (L* ~20);
the Pit's darkness in game is the grade — a quarter ambient outside the hero's light radius (``vfx_recipes.json``
``dark.pit``) — not the texture.

Writes ``Art/Export/Terrain/``: ``T_Terrain_Pit_Ash/Earth/Flagstone/Paving_BC``, ``T_Water_Pit_BC`` (1024², one 3 × 3
period, sRGB), ``T_Terrain_Pit_Details_BC`` (4 × 4 per-tile detail atlas), ``T_Water_Pit_Mask_M``; the lattice noise is
the plains texture. ``manifest.json → terrain.pit`` has the plains schema. Previews (``Art/Previews/terrain_pit/``):
``layers.png``, ``details.png``, ``transitions.png`` (a test crypt at the W1 camera).
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

THEME = "pit"
NAMES = {"0": "ash", "1": "earth", "2": "flagstone", "3": "seep", "5": "paving"}
PIT_STYLE = {
    "ground": {
        "0": {"base": 0x3A3733, "rank": 4, "lip": 0x4A4640,
              "layers": [{"kind": "patch", "count": 4, "colors": [0x403D38, 0x35322F, 0x44413C], "r": [14, 28]},
                         {"kind": "patch", "count": 2, "colors": [0x4C4842, 0x312E2B], "r": [4, 8]},
                         {"kind": "pebble", "count": 5, "color": 0x4E4A44, "shade": 0x26231F, "light": 0x625D55,
                          "size": [1.0, 1.9]}],
              "details": ["stones", "crackStar", "stones"], "accents": [0x6A655C, 0x57534C]},
        "1": {"base": 0x36312C, "rank": 1,
              "layers": [{"kind": "patch", "count": 4, "colors": [0x312C28, 0x3B3530], "r": [10, 22]},
                         {"kind": "crack", "count": 2, "color": 0x1E1A17, "light": 0x463E36, "len": [8, 14],
                          "width": 2},
                         {"kind": "pebble", "count": 8, "color": 0x403830, "shade": 0x221E1B, "light": 0x50473D,
                          "size": [1.1, 2.2]}],
              "details": ["stones", "crackStar", "stones"], "accents": [0x50473D]},
        "2": {"base": 0x363330, "rank": 2,
              "layers": [{"kind": "slabs", "cells": 2, "mortar": 0x1C1A19, "mortarW": 0.035, "jitter": 0.085,
                          "bevelLight": 0x46423E, "bevelShade": 0x2A2826},
                         {"kind": "patch", "count": 3, "colors": [0x403D39], "r": [3, 7]}],
              "details": ["crackStar", "stones", "crackStar"], "accents": [0x403D39]},
        "3": {"base": 0x101114, "rank": 0,
              "layers": [{"kind": "patch", "count": 3, "colors": [0x0D0E11, 0x15161A], "r": [14, 26]},
                         {"kind": "wave", "count": 3, "color": 0x2E2440, "len": [6, 11], "alpha": 0.35}],
              "details": ["stones"], "accents": [0x1E1A24],
              "liquid": {"shallow": 0x17191D, "foam": 0x2A2C31, "bank": 0x1A1715}},
        "5": {"base": 0x3B3834, "rank": 2,
              "layers": [{"kind": "slabs", "cells": 3, "mortar": 0x211F1D, "mortarW": 0.03, "jitter": 0.06,
                          "bevelLight": 0x4B4843, "bevelShade": 0x2F2D2A},
                         {"kind": "patch", "count": 3, "colors": [0x45423D], "r": [3, 7]}],
              "details": ["crackStar", "stones"], "accents": [0x45423D]},
    },
    "outcrop": {"kind": "cryptWall", "rock": 0x4A4642, "cap": 0x131110, "accent": 0x8A6E44, "extra": 0x36332F,
                "shadow": 0x0C0A0C},
    "palisade": {"kind": "darkStakes", "wood": 0x2A221C, "band": 0x4A4238, "accent": 0x5E1012},
}
PLAINS = T.STYLES
OUT = T.OUT
TEXTURES = {"0": "T_Terrain_Pit_Ash_BC", "1": "T_Terrain_Pit_Earth_BC", "2": "T_Terrain_Pit_Flagstone_BC",
            "3": "T_Water_Pit_BC", "5": "T_Terrain_Pit_Paving_BC"}
DETAILS = "T_Terrain_Pit_Details_BC"
WATER_MASK = "T_Water_Pit_Mask_M"
NOISE = "T_Terrain_Plains_Noise_M"
PREVIEW_DIR = "terrain_pit"


class pit_style:
    """Context: ``terrain.STYLES`` = the pit style (every painter / ``composite`` of ``terrain.py`` reads it)."""

    def __enter__(self):
        T.STYLES = PIT_STYLE
        return PIT_STYLE

    def __exit__(self, *exc):
        T.STYLES = PLAINS
        return False


# ── art extras (painted over the style's layers; the runtime samples the textures) ──────────────────────────
def extra_spade_cuts(cv: T.Canvas, seed: int, groove: int = 0x1A1614, lip: int = 0x4A4037, count: int = 9):
    """The Pitwarden's work: short straight spade cuts in the dug earth, in rows of two or three — a black slot,
    a lit lip on its upper-left side (the key light), a clod thrown beside some."""
    r = T.mulberry32(seed)
    for k in range(count):
        u0, v0 = r() * T.PERIOD, r() * T.PERIOD
        ang = (math.pi / 4) + (r() - 0.5) * 2.2
        d = np.array([math.cos(ang), math.sin(ang)])
        nrm = np.array([-d[1], d[0]])
        n = 2 + int(r() * 2)
        for j in range(n):
            off = nrm * (j - (n - 1) / 2) * 0.16 + d * (r() - 0.5) * 0.06
            L = 0.15 + r() * 0.07
            a, b_ = off - d * L / 2, off + d * L / 2
            lift = np.array([-0.012, -0.016])
            cv.paint(u0, v0, 0.6, T.sdf_stroke([a + lift, b_ + lift], [0.014, 0.014]), T.rgb(lip), 0.7)
            cv.paint(u0, v0, 0.6, T.sdf_stroke([a, b_], [0.019, 0.016]), T.rgb(groove), 0.9)


def extra_soot(cv: T.Canvas, seed: int, color: int = 0x23211F, count: int = 5):
    """Soot shadows burned onto the flags (the Pit was a burn pit): soft dark blobs."""
    r = T.mulberry32(seed)
    for _ in range(count):
        u, v = r() * T.PERIOD, r() * T.PERIOD
        s = (9 + r() * 9) * T.SM
        cv.paint(u, v, s * 2, T.sdf_blob(s, [r() for _ in range(12)], 0.75), T.rgb(color), 0.30)


EXTRAS = {
    "0": lambda cv, s: TM.extra_ash_flecks(cv, s, count=70, color=0x6A655C, light=0x8A8478),
    "1": lambda cv, s: (TM.extra_scuffs(cv, s + 1, color=0x25211D, count=7), extra_spade_cuts(cv, s)),
    "2": lambda cv, s: (extra_soot(cv, s + 3), TM.extra_mortar_ash(cv, s, color=0x504C46, count=34),
                        TM.extra_ash_flecks(cv, s + 7, count=26, color=0x5C5851, light=0x6E6960)),
    "3": lambda cv, s: TM.extra_sheen(cv, s, color=0x241C30, count=4),
    "5": lambda cv, s: (TM.extra_mortar_ash(cv, s, color=0x4E4A44, count=30),
                        TM.extra_ash_flecks(cv, s + 5, count=30, color=0x5E5A52, light=0x77726A)),
}


def paint_material(key: str, res: int = T.RES) -> np.ndarray:
    st = PIT_STYLE["ground"][key]
    cv = T.Canvas(res, T.PERIOD, T.rgb(st["base"]))
    seed = 4409 + int(key) * 7907
    for i, layer in enumerate(st["layers"]):
        if layer["kind"] == "slabs":
            T.paint_slabs(cv, layer, st["base"], seed + i * 7919)
            continue
        T.paint_layer(cv, layer, T.features(layer, seed + i * 7919))
    EXTRAS[key](cv, 0xA13 * 31 + int(key) * 977)
    return cv.u8()


def build_textures(res: int = T.RES, out: Path | None = None) -> dict:
    out = out or OUT
    out.mkdir(parents=True, exist_ok=True)
    tex = {}
    with pit_style():
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
    """``terrain.composite`` with the pit ranks, lips and liquid colours."""
    with pit_style():
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
    """``terrain.pit``: the plains schema (``terrain.manifest_section``) with the pit textures and colours."""
    with pit_style():
        saved = dict(T.TEXTURES)
        T.TEXTURES.clear()
        T.TEXTURES.update(TEXTURES)
        try:
            sec = copy.deepcopy(T.manifest_section()["plains"])
        finally:
            T.TEXTURES.clear()
            T.TEXTURES.update(saved)
    g = PIT_STYLE["ground"]
    hexs = lambda c: "#%06X" % c  # noqa: E731
    wave = next(l for l in g["3"]["layers"] if l["kind"] == "wave")
    for k, lay in sec["layers"].items():
        lay["tile"] = lay["name"]                           # the map's tile type this key paints
        lay["name"] = NAMES[k]
    sec["source"] = ("environment/terrain_pit.py PIT_STYLE (the art lane's style: A13). HANDOFF content lane: copy "
                     "dataStyle to GameData as terrain_styles themes.pit (the runtime Tile<i>* vectors, ranks, lips "
                     "and liquid colours come from Data/terrain_styles.json)")
    sec["theme"] = THEME
    sec["zones"] = ["sub_rf_pit"]
    sec["dataStyle"] = _hex_style(PIT_STYLE)
    sec["details"]["texture"] = DETAILS
    sec["details"]["file"] = f"Terrain/{DETAILS}.png"
    sec["details"]["cells"] = ("4 x 4 cells of one tile: row 0 ash, 1 earth, 2 flagstone, 3 paving (paint 5); column "
                               "= variant - 1 (variants 1..3); column 3 (seep) is transparent")
    sec["noise"]["texture"] = NOISE
    sec["noise"]["file"] = f"Terrain/{NOISE}.png"
    sec["noise"]["shared"] = "theme-independent (the AF_Terrain lattice / patch fields): the plains noise texture"
    w = sec["water"]
    w["texture"] = TEXTURES["3"]
    w["wave"] = hexs(wave["color"])
    w["waveAlpha"] = wave.get("alpha", 0.7)
    w["mask"]["texture"] = WATER_MASK
    w["mask"]["file"] = f"Terrain/{WATER_MASK}.png"
    w["mask"]["channels"]["r"] = (f"breach sheen flecks ({hexs(wave['color'])} x {wave.get('alpha', 0.7)}), scroll "
                                  f"UV0/3 + t*(.012,.008): still water")
    w["lilyPads"] = "none (no water props in the Pit)"
    w["note"] = "still black seep water with a faint breach sheen; not used by the first map (the set stays complete)"
    sec["materials"] = {
        "0": "ash: ash drifted over the flags; also what wall tiles paint, so it spills ~0.15 tile out from every "
             "wall's foot (the transition rule: rank 4 wins the edge)",
        "1": "earth: dug grave earth, spade cuts — the Pitwarden's Cut; the darkest walkable tile (kept at L* ~20: "
             "the black breach clot must stay >= 15 L* darker than any tile it stands on)",
        "2": "flagstone: the crypt's default floor",
        "5": "paving: the Flame Niche's finer paving",
        "3": "seep: black water (unused)"}
    sec["artExtras"] = {
        "ash": "ash flecks #6A655C / #8A8478",
        "earth": "spade cuts (black slots #1A1614 in rows of 2-3, lit lip #4A4037) and scuffs",
        "flagstone": "soot shadows #23211F, ash in the joints #504C46, flecks",
        "paving": "ash in the joints #4E4A44, flecks",
        "rule": "painted into the BC textures only (the runtime samples textures)"}
    sec["previews"] = [f"Previews/{PREVIEW_DIR}/{n}.png" for n in ("layers", "details", "transitions")]
    sec["params"] = {f"Tile{k}{f}": v for k in ("0", "1", "2", "3", "5") for f, v in
                     (("Base", hexs(g[k]["base"])), ("Rank", g[k]["rank"]))}
    sec["params"]["Tile0Lip"] = hexs(g["0"]["lip"])
    return {THEME: sec}


# ── previews ────────────────────────────────────────────────────────────────────────────────────────────
def test_map() -> np.ndarray:
    """A 14 × 12 test crypt: flagstone rooms in wall mass (4), an ash-choked corner, a cut of dug earth, a paved
    apse."""
    m = np.full((12, 14), 4, np.int32)
    m[1:6, 1:8] = 2                                          # a hall
    m[1:3, 1:4] = 0                                          # ash drifted in its corner
    m[6:8, 4:6] = 2                                          # a passage
    m[8:11, 2:10] = 1                                        # the cut: dug earth
    m[8:11, 2:4] = 2
    m[2:5, 9:13] = 5                                         # a paved apse
    m[3, 8] = 2
    return m


def render_ground(img: np.ndarray, shape, out: Path) -> None:
    """The composited test crypt at the W1 camera (native 1080p crop) with the warrior on the flags; wall tiles are
    drawn as flat black tops (the kit's blocks stand there in game: ``vignette_pit.py`` shows them)."""
    import bpy
    from mathutils import Vector
    from kit import outline, review, scene
    scene.reset()
    rows, cols = shape
    E.SCRATCH.mkdir(parents=True, exist_ok=True)
    p = E.SCRATCH / "terrain_pit_ground.png"
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
    w = E.import_warrior(at=(4.0, -3.5, 0), yaw_deg=-135)
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
                                  "TERRAIN PIT TEST CRYPT: FLAGSTONE HALL, ASH CORNER, DUG EARTH, PAVED APSE, WALL "
                                  "GROUND (ASH)  W1 1080P 1:1  NO GRADE, NO LIGHT RADIUS"):
        print(f"  {pth}  {s_ // 1024} KB")


def previews(tex: dict, out: Path | None = None) -> None:
    from kit import review
    out = out or paths.preview_root() / PREVIEW_DIR
    out.mkdir(parents=True, exist_ok=True)
    cells = []
    for key in ("2", "0", "1", "5", "3"):
        t = np.tile(tex["tex"][key], (2, 2, 1))[::4, ::4].copy()
        review.draw_text(t, 4, 4, f"{TEXTURES[key]}  ({NAMES[key].upper()})  2X2 PERIODS  TEXELS AT 1/4")
        cells.append(t)
    E.save_native(out / "layers.png", review.grid(cells, 3, pad=2))
    det = tex["details"]
    bg = np.zeros_like(det[:, :, :3])
    bg[:, :] = (47, 44, 41)
    a = det[:, :, 3:4] / 255.0
    comp = (det[:, :, :3] * a + bg * (1 - a)).astype(np.uint8)[::2, ::2].copy()
    review.draw_text(comp, 4, 4, "PIT DETAILS AT 1/2: ROWS ASH EARTH FLAGSTONE PAVING / COLS VARIANT 1-3")
    E.save_native(out / "details.png", comp)
    layers = T.Layers(tex["tex"], tex["details"])
    m = test_map()
    img = composite(m, layers, ppt=64)
    # wall tiles: the black earth of the kit's block tops (so the test map reads as rooms)
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
        d = E.SCRATCH / "terrain_pit"
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

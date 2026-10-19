"""Darkest-tile readability gate (dark remake: implementation-plan.md A0 / work package 0C, monsters.md §1.2).

    EGL_PLATFORM=surfaceless /opt/venvs/blender/bin/python unreal/Art/blender/coherence/darkgate.py
        [assets…]                      only these manifest assets (default: every shipped monster)
        [--theme=<t>]                  terrain set (manifest ``terrain.<t>``); default ``march`` once it is exported
                                       (A10), ``plains`` until then
        [--tile=darkest|<name>]        ground material: the darkest walkable one (default) or grass / dirt / stone /
                                       camp (or the theme's own layer names)
        [--ground-hex=#RRGGBB]         a flat ground colour instead of a terrain set (stress test, e.g. before the
                                       march terrain exists)
        [--facing=se] [--clip=Idle] [--frame=0] [--no-variants] [--per-page=6]
        [--out=<dir>] [--report=<json>] [--no-report]

Every monster the game ships (manifest ``SkeletalMesh`` entries in ``Monsters/``; each quest-hunt ``variants`` entry as
its own figure: scale, attachments, elite crown) is re-imported from ``Art/Export`` exactly as the coherence lineups do
(``cohkit``: shipped FBX posed by its exported clip, family palette atlases, the UE screen-constant ink of its slot,
emissive bloom) and stood on a field of one terrain tile at the **W1 game camera** (yaw 45°, pitch −50°, FOV 35°, the
default distance), web contact shadows under the feet, native 1080p. Each figure is measured on its own coverage:

| Metric | Gate | How |
|---|---|---|
| ``rim`` | dark ink ≥ 3 px on ≥ 90 % of the silhouette edge pixels (+ the kit's dark / hull-colour checks) | ``cohkit.ink_gate_group`` at ``RIM_PX`` (= the Chapter-1 character gate ``CHAR_GATE_PX``) |
| ``lit`` | ΔL* ≥ 15 between the lit side and the ground | the lit side = the figure's solid toon pixels in the key light's **light band or rim light** (a light pass evaluates the toon terms of ``kit.shading`` per pixel: h = N·L × .5 + .5 ≥ ``T_LIGHT``, or rim weight ≥ .5; marker and emissive pixels left out; fewer than 30 such pixels: every not-shade pixel); ΔL* = the median over those pixels of \\|L* − L*ground\\| (CIELAB L*, D65, final frame; L*ground = the median of the ground field: the composited tile before the contact shadows), i.e. half the lit side clears the ground by ΔL*. The signed region median (``signedDeltaL``) is reported beside it |
| ``marker`` | ≥ 2 px | the family marker's palette swatches (eyes, crimson cloth) rendered as a coverage mask with the rest of the figure occluding them: the largest solid k × k block (coverage ≥ .5) of the visible marker |

Definitions pending confirmation (Docs/DECISIONS.md R21): ``rim`` is the ink hull's width and does not look at the
ground (on a near-black tile the ink cannot separate the figure; the lit side has to), so the report adds, ungated, the
ink's and the marker's L* distance from the ground (``rim.inkDeltaL`` / ``fracInkClear``, ``marker.deltaL``,
``marker.emissivePixels``) for the in-game review; ``lit`` gates the per-pixel median, with the region median
(``signedDeltaL``) beside it. A PASS within ``EDGE_*`` of a threshold is listed in ``atEdge`` (no headroom for a darker
grade or ground).

The marker swatches come from the manifest entry (``"marker": {"swatches": [...]}`` — the hook the new monster
generators fill), else ``MARKERS`` below, else every swatch on the mesh whose name ends in ``.eye`` / ``.eyes`` /
``.sclera`` / ``.marker``. A figure without a marker fails (monsters.md §1.2: every hostile carries exactly one).

Ground: the darkest walkable material (grass, dirt, stone, camp; never water) by the median L* of a composited 8 × 8
field of it (``environment/terrain.composite`` with the theme's layer and detail textures from ``manifest.terrain``),
so the march set is picked up as soon as A10 exports it. The zone grade (``zone_moods`` P14, 55 % of the mood) is not
applied: it acts on the whole frame and is reviewed in game (implementation-plan.md *mac-look*).

Outputs (``Art/Previews/ch1_dark/``):

* ``darkgate_<theme>_<tile>.png`` — the lineup at 1:1 with each figure's verdict (``RIM`` fraction, ``DL`` lit-side
  ΔL*, ``MK`` marker block px); ``darkgate_<theme>_<tile>_map.png`` — the gate map: silhouette edge pixels green / red
  (ink ≥ / < 3 px), the lit-side pixels washed yellow, visible marker pixels cyan (red when < 2 px);
  ``…_p<N>`` when the roster needs more than one frame;
* ``darkgate_report.json`` — one run per ground key (``plains:grass``, later ``march:<tile>``, ``hex:<RRGGBB>``):
  thresholds, the ground's per-material L*, every figure's metrics and verdict. The first run on today's assets
  (``plains:grass``) is the baseline.

Exit 1 when a figure fails a metric.
"""
from __future__ import annotations

import json
import re
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import cohkit as K  # noqa: E402  (sets up the kit / environment import paths)
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector  # noqa: E402

import terrain as T  # noqa: E402
from kit import palette, paths, pngio, review, scene, shading  # noqa: E402

OUT = paths.preview_root() / "ch1_dark"
REPORT = "darkgate_report.json"
RES = (1920, 1080)
RIM_PX = K.CHAR_GATE_PX          # monsters.md §1.2 "rim ≥ 3 px" (the kit's Chapter-1 character gate, 3.0 px at 1080p)
RIM_FRAC = 0.9                   # … on ≥ 90 % of the silhouette edge pixels (outline.GATE_FRAC)
LIT_DL = 15.0                    # lit-side ΔL* against the ground
MARKER_PX = 2                    # marker accent ≥ 2 px: a solid 2 × 2 block at native 1080p
SPACING = 1.15                   # m between figures along screen-right
WALKABLE = ("0", "1", "2", "5")  # terrain paint keys a monster can stand on (3 = water)
MARKERS = {                      # asset → its family marker swatches (today's roster; new assets: manifest "marker")
    "SK_Mon_Goblin": ("goblin.eye",),
    "SK_Mon_GoblinChief": ("chief.eye",),
    "SK_Mon_GoblinShaman": ("shaman.ember",),     # its eyes: the spirit embers in the bone mask's eye holes
    "SK_Mon_Slime": ("slime.sclera",),
}
MARKER_NAME = re.compile(r"\.(eye|eyes|sclera|marker)$")
MONSTER_CLASSES = ("monster", "boss", "small_monster")
RIM_LIGHT_ON = 0.5               # rim-light weight (0..1 of RIM_ALPHA) from which a pixel counts as rim-lit
MIN_LIT_PX = 30                  # fewer light-band / rim pixels than this: the not-shade bands stand in
# A pass this close to a threshold is flagged ``atEdge`` (report + log): no headroom for a darker grade or ground.
EDGE_DL = 1.5                    # lit ΔL* within 1.5 of LIT_DL (10 %)
EDGE_RIM_FRAC = 0.03             # rim fraction within 3 points of RIM_FRAC
EDGE_MARKER_PX = 0               # marker block exactly MARKER_PX


# ── colour ──────────────────────────────────────────────────────────────────────────────────────────────
def lab_l(rgb: np.ndarray) -> np.ndarray:
    """CIELAB L* (D65) of sRGB uint8 pixels."""
    a = rgb[..., :3].astype(np.float32) / 255.0
    lin = np.where(a <= 0.04045, a / 12.92, ((a + 0.055) / 1.055) ** 2.4)
    y = lin @ np.array([0.2126, 0.7152, 0.0722], np.float32)
    f = np.where(y > 216 / 24389, np.cbrt(y), (24389 / 27 * y + 16) / 116)
    return 116.0 * f - 16.0


def hex_rgb(h: str) -> np.ndarray:
    s = h.strip().lstrip("#")
    if not re.fullmatch(r"[0-9A-Fa-f]{6}", s):
        raise SystemExit(f"--ground-hex wants #RRGGBB, got {h!r}")
    return np.array([int(s[i:i + 2], 16) for i in (0, 2, 4)], np.float32)


# ── roster ──────────────────────────────────────────────────────────────────────────────────────────────
def _label(name: str) -> str:
    """A display name in the review font's glyphs (A-Z 0-9 . - _ : / ( ) + # = %): the part before a ' · ' title."""
    name = name.split("·")[0].strip().upper()
    return re.sub(r"[^A-Z0-9 .\-_:/()+#=%]", "", name) or "?"


def roster(only: list[str], variants: bool = True) -> list[tuple[str, str | None, str]]:
    """[(asset, variant, label)] of every shipped monster (+ its hunt variants), manifest order."""
    out = []
    for name, e in K.manifest()["assets"].items():
        if e.get("kind") != "SkeletalMesh" or e.get("category") != "Monsters":
            continue
        if only and name not in only:
            continue
        if K.slot_info(e)[1] not in MONSTER_CLASSES:
            continue
        out.append((name, None, _label((e.get("displayName") or {}).get("en") or name.replace("SK_Mon_", ""))))
        if variants:
            for vid, v in sorted((e.get("variants") or {}).items()):
                out.append((name, vid, _label((v.get("displayName") or {}).get("en") or vid)))
    missing = [a for a in only if a not in K.manifest()["assets"]]
    if missing:
        raise SystemExit(f"not in the manifest: {', '.join(missing)}")
    return out


# ── ground ──────────────────────────────────────────────────────────────────────────────────────────────
def default_theme() -> str:
    terr = K.manifest().get("terrain", {})
    return "march" if "march" in terr else "plains"


def theme_layers(theme: str) -> tuple[T.Layers, dict[str, str]]:
    """The composite inputs of a terrain set from ``manifest.terrain.<theme>`` + {paint key: material name}."""
    sec = K.manifest().get("terrain", {}).get(theme)
    if sec is None:
        raise SystemExit(f"no terrain set {theme!r} in the manifest (have: {', '.join(K.manifest().get('terrain', {}))})")
    root = paths.export_root()
    tex, names = {}, {}
    for key, lay in sec["layers"].items():
        tex[key] = pngio.read_png(root / lay["file"])[:, :, :3]
        names[key] = lay.get("name", key)
    det = pngio.read_png(root / sec["details"]["file"])
    return T.Layers(tex, det), names


def field_image(layers: T.Layers, key: str, rows: int, cols: int, origin=(0, 0), ppt: int = 48) -> np.ndarray:
    """A uniform field of one material (per-tile interior details included) as the runtime terrain draws it."""
    return T.composite(np.full((rows, cols), int(key), np.int32), layers, ppt=ppt, origin=origin)


def ground_stats(img: np.ndarray) -> dict:
    L = lab_l(img.reshape(-1, 3))
    return {"L50": round(float(np.median(L)), 1), "L10": round(float(np.percentile(L, 10)), 1),
            "L90": round(float(np.percentile(L, 90)), 1),
            "meanRgb": "#%02X%02X%02X" % tuple(int(round(c)) for c in img.reshape(-1, 3).mean(0))}


class Ground:
    """The ground a run stands on: a terrain material of a theme, or a flat colour."""

    def __init__(self, theme: str | None, tile: str, hex_color: str | None):
        self.hex = hex_color
        self.materials: dict = {}
        if hex_color:
            self.rgb = hex_rgb(hex_color)
            self.theme, self.key, self.name = "hex", None, hex_color.lstrip("#").upper()
            self.stats = ground_stats(np.broadcast_to(self.rgb.astype(np.uint8), (4, 4, 3)).copy())
            self.selected_by = "--ground-hex (flat colour)"
            return
        self.theme = theme or default_theme()
        self.layers, names = theme_layers(self.theme)
        for key in WALKABLE:
            if key in names:
                self.materials[names[key]] = dict(key=key, **ground_stats(field_image(self.layers, key, 8, 8)))
        if tile == "darkest":
            name = min(self.materials, key=lambda n: (self.materials[n]["L50"], self.materials[n]["key"]))
            self.selected_by = "darkest walkable material by median L* of an 8 x 8 composited field"
        elif tile in self.materials:
            name = tile
            self.selected_by = f"--tile={tile}"
        else:
            raise SystemExit(f"--tile={tile}: the {self.theme} set has {', '.join(self.materials)}")
        self.name, self.key = name, self.materials[name]["key"]
        self.stats = {k: v for k, v in self.materials[name].items() if k != "key"}

    @property
    def run_key(self) -> str:
        return f"{self.theme}:{self.name}"

    @property
    def file_tag(self) -> str:
        return f"{self.theme}_{self.name}".lower()

    def image(self, rows: int, cols: int, origin, ppt: int) -> np.ndarray:
        if self.hex:
            return np.broadcast_to(self.rgb.astype(np.uint8), (rows * ppt, cols * ppt, 3)).copy()
        return field_image(self.layers, self.key, rows, cols, origin, ppt)


def ground_plane(img: np.ndarray, origin, shape, ppt: int, shadows=(), name: str = "dg_ground"):
    """``img`` (rows × cols tiles at ``ppt``) as the emission ground plane ``AF_Stage`` (the name the review passes
    treat as the stage) + the web contact shadows ``[(col, row, radius_m, alpha)]`` (``cohkit.ground``'s falloff)."""
    img = img.astype(np.float32)
    H, W = img.shape[:2]
    c0, r0 = origin
    for (c, r, rad, alpha) in shadows:
        rp = rad * ppt
        cx, cy = (c - c0 + 0.5) * ppt, (r - r0 + 0.5) * ppt
        x0, x1 = int(max(cx - rp, 0)), int(min(cx + rp + 1, W))
        y0, y1 = int(max(cy - rp, 0)), int(min(cy + rp + 1, H))
        if x0 >= x1 or y0 >= y1:
            continue
        ys, xs = np.mgrid[y0:y1, x0:x1]
        d = np.hypot(xs - cx, ys - cy) / rp
        a = np.where(d < 0.65, alpha - (alpha - 0.26 * alpha / 0.45) * d / 0.65,
                     (0.26 * alpha / 0.45) * (1 - (d - 0.65) / 0.35))
        a = np.clip(a, 0, 1) * (d < 1)
        img[y0:y1, x0:x1] = img[y0:y1, x0:x1] * (1 - a[..., None]) + np.array([8, 6, 16]) * a[..., None]
    K.SCRATCH.mkdir(parents=True, exist_ok=True)
    p = K.SCRATCH / f"{name}.png"
    pngio.write_png(p, img.clip(0, 255).astype(np.uint8))
    rows, cols = shape
    me = bpy.data.meshes.new(name)
    x0, x1 = c0 - 0.5, c0 + cols - 0.5
    y0, y1 = -(r0 - 0.5), -(r0 + rows - 0.5)
    me.from_pydata([(x0, y0, 0), (x1, y0, 0), (x1, y1, 0), (x0, y1, 0)], [], [(0, 1, 2, 3)])
    uv = me.uv_layers.new(name="UVMap")
    for li, (u, v) in enumerate(((0, 1), (1, 1), (1, 0), (0, 0))):
        uv.data[li].uv = (u, v)
    old = bpy.data.objects.get("AF_Stage")
    if old is not None:
        bpy.data.objects.remove(old)
    ob = bpy.data.objects.new("AF_Stage", me)
    scene.link(ob)
    mat = bpy.data.materials.new(name)
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
    return ob


# ── markers ─────────────────────────────────────────────────────────────────────────────────────────────
_REGISTRY: dict[str, dict[str, int]] = {}


def registry(family: str) -> dict[str, int]:
    """{swatch name: atlas index} of a palette family (``palettes/<Family>.json``, read-only)."""
    if family not in _REGISTRY:
        p = paths.palette_root() / f"{family}.json"
        names: dict[str, int] = {}
        if p.is_file():
            for sw in json.loads(p.read_text())["swatches"]:
                if not sw.get("free"):
                    for n in sw["names"]:
                        names[n] = sw["i"]
        _REGISTRY[family] = names
    return _REGISTRY[family]


def face_swatches(mesh) -> np.ndarray:
    """Atlas swatch index of every slot-0 / shell face (UVs collapse onto the swatch centres, palette.bake_uvs)."""
    me = mesh.data
    nf = len(me.polygons)
    if nf == 0 or not me.uv_layers:
        return np.zeros(0, np.int32)
    uvl = me.uv_layers.active or me.uv_layers[0]
    uv = np.zeros(len(me.loops) * 2, np.float32)
    uvl.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    ls = np.zeros(nf, np.int32)
    me.polygons.foreach_get("loop_start", ls)
    mi = np.zeros(nf, np.int32)
    me.polygons.foreach_get("material_index", mi)
    u, v = uv[ls, 0], uv[ls, 1]
    g = palette.GRID
    idx = (np.clip(np.floor((1.0 - v) * g), 0, g - 1) * g + np.clip(np.floor(u * g), 0, g - 1)).astype(np.int32)
    return idx[mi != 1]


def marker_spec(fig: K.Figure) -> dict:
    """{"swatches": names, "indices": atlas indices, "source": where the names came from} for a figure."""
    e = K.entry(fig.asset)
    fam = fig.family
    reg = registry(fam)
    used = set()
    for m in fig.meshes:                                  # body + attachments drawn from the same atlas
        if m.get("af_family", fam) == fam:
            used.update(int(i) for i in np.unique(face_swatches(m)))
    names, source = None, None
    mk = e.get("marker")
    if isinstance(mk, dict) and mk.get("swatches"):
        names, source = list(mk["swatches"]), "manifest marker"
    elif fig.asset in MARKERS:
        names, source = list(MARKERS[fig.asset]), "darkgate.MARKERS"
    else:
        names = sorted(n for n, i in reg.items() if i in used and MARKER_NAME.search(n))
        source = "swatch names on the mesh (*.eye / *.sclera / *.marker)"
    idx = sorted({reg[n] for n in names if n in reg})
    unknown = [n for n in names if n not in reg]
    return {"swatches": names, "indices": idx, "source": source, "family": fam,
            "onMesh": [i for i in idx if i in used], **({"unknown": unknown} if unknown else {})}


def _mask_material(indices: list[int], tag: str):
    """Emission 1 on the marker swatches, 0 elsewhere: the swatch index is recomputed from the UV (``Palette.uv``:
    faces sit on their swatch centre) and compared with each marker index — pure nodes (a generated lookup image
    is regenerated blank by Cycles)."""
    m = bpy.data.materials.new(f"AF_DG_Marker_{tag}")
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    b = shading.NB(nt)
    u, v, _ = b.sep(b.node("ShaderNodeTexCoord").outputs["UV"])
    col = b.math("FLOOR", b.math("MULTIPLY", u, float(palette.GRID)))
    row = b.math("FLOOR", b.math("MULTIPLY", b.math("SUBTRACT", 1.0, v), float(palette.GRID)))
    idx = b.math("MULTIPLY_ADD", row, float(palette.GRID), col)
    hit = 0.0
    for i in indices:
        hit = b.math("ADD", hit, b.math("COMPARE", idx, float(i), 0.5))
    em = b.node("ShaderNodeEmission")
    b._set(em.inputs["Color"], b.comb(*([b.math("MINIMUM", hit, 1.0)] * 3)) if indices else (0.0, 0.0, 0.0))
    out = b.node("ShaderNodeOutputMaterial")
    b.l.new(em.outputs[0], out.inputs["Surface"])
    return m


def _light_material():
    """The toon's key-light terms per pixel (``kit.shading.build_toon_group``): R = h = N·L × .5 + .5 (bands: shade
    below ``T_SHADE``, light above ``T_LIGHT``), G = the rim-light weight (banded Fresnel × lit mask, 0..1 of
    ``RIM_ALPHA``)."""
    m = bpy.data.materials.get("AF_DG_Light")
    if m is not None:
        return m
    m = bpy.data.materials.new("AF_DG_Light")
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    b = shading.NB(nt)
    geo = b.node("ShaderNodeNewGeometry")
    n, v = geo.outputs["Normal"], geo.outputs["Incoming"]
    ndl = b.vmath("DOT_PRODUCT", n, b.const_vec(shading.L_KEY))
    h = b.math("MULTIPLY_ADD", ndl, 0.5, 0.5)
    ndv = b.math("MAXIMUM", b.vmath("DOT_PRODUCT", n, v), 0.0)
    fres = b.math("POWER", b.math("SUBTRACT", 1.0, ndv), shading.RIM_POWER)
    rim = b.math("MULTIPLY", b.smooth(fres, shading.RIM_T0, shading.RIM_T1),
                 b.smooth(ndl, 0.0, shading.RIM_MASK_EDGE))
    em = b.node("ShaderNodeEmission")
    b._set(em.inputs["Color"], b.comb(h, rim, 0.0))
    out = b.node("ShaderNodeOutputMaterial")
    b.l.new(em.outputs[0], out.inputs["Surface"])
    return m


def _clear_material():
    m = bpy.data.materials.get("AF_DG_Clear")
    if m is None:
        m = bpy.data.materials.new("AF_DG_Clear")
        m.use_nodes = True
        nt = m.node_tree
        nt.nodes.clear()
        nt.links.new(nt.nodes.new("ShaderNodeBsdfTransparent").outputs[0],
                     nt.nodes.new("ShaderNodeOutputMaterial").inputs["Surface"])
    return m


def figure_pass(fig: K.Figure, every: list, box, body_mat, shell_mat=None, samples: int = 8) -> np.ndarray:
    """A data render (EXR, linear) of one figure over ``box``: ``body_mat(mesh)`` on the toon slot of its meshes
    (``None`` = black), black on the ink hulls and on its extras (the elite crown occludes), ``shell_mat`` on
    dithered shell slots (≥ 2; ``None`` = left out), every other object and the stage hidden, black world."""
    black = review._flat_emission("AF_DG_Black", (0, 0, 0))
    black_hull = review._flat_emission("AF_DG_BlackHull", (0, 0, 0), hull=True)
    mine = [m for m in fig.all() if m.type == "MESH"]
    others = [o for o in every if o not in mine and not o.hide_render]
    for o in others:
        o.hide_render = True
    saved = []
    for m in mine:
        mats = m.data.materials
        saved.append((m, [mats[i] for i in range(len(mats))]))
        body = (body_mat(m) if m in fig.meshes else None) or black
        for i in range(len(mats)):
            mats[i] = body if i == 0 else black_hull if i == 1 else (shell_mat or _clear_material())
    review.hide_stage(True)
    sc = bpy.context.scene
    keep = sc.cycles.samples
    sc.cycles.samples = samples
    try:
        with review._World((0.0, 0.0, 0.0)):
            arr = review.render_array(box, exr=True)
    finally:
        sc.cycles.samples = keep
        for m, ms in saved:
            for i, mt in enumerate(ms):
                m.data.materials[i] = mt
        for o in others:
            o.hide_render = False
        review.hide_stage(False)
    return arr


def marker_pass(fig: K.Figure, every: list, spec: dict, box) -> np.ndarray:
    """Coverage of the figure's visible marker swatches over ``box`` (the figure's ink-gate box): its other regions,
    held items, attachments and crown occlude in black; dithered shells are left out (the eyes show through)."""
    mask_m = _mask_material(spec["indices"], f"{fig.asset}_{id(fig) % 100000}")
    arr = figure_pass(fig, every, box, lambda m: mask_m if m.get("af_family") == fig.family else None)
    return np.clip(arr[:, :, 0], 0, 1)


def light_pass(fig: K.Figure, every: list, box) -> tuple[np.ndarray, np.ndarray]:
    """(h, rim weight) of the toon key light over ``box`` (shells included: they are lit like the body)."""
    lm = _light_material()
    arr = figure_pass(fig, every, box, lambda m: lm, shell_mat=lm)
    return arr[:, :, 0], np.clip(arr[:, :, 1], 0, 1)


def _fit(a: np.ndarray, shape) -> np.ndarray:
    """``a`` cropped / zero-padded to ``shape`` (border renders can differ by a pixel at the frame edge)."""
    H, W = shape
    out = np.zeros((H, W) + a.shape[2:], a.dtype)
    h, w = min(H, a.shape[0]), min(W, a.shape[1])
    out[:h, :w] = a[:h, :w]
    return out


def largest_block(mask: np.ndarray) -> int:
    """Side of the largest all-true square in a bool mask (dynamic programming over its bounding box)."""
    if not mask.any():
        return 0
    ys, xs = np.nonzero(mask)
    sub = mask[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    h, w = sub.shape
    dp = np.zeros((h + 1, w + 1), np.int32)
    best = 0
    for y in range(h):
        row = sub[y]
        for x in range(w):
            if row[x]:
                v = min(dp[y, x + 1], dp[y + 1, x], dp[y, x]) + 1
                dp[y + 1, x + 1] = v
                if v > best:
                    best = v
    return int(best)


# ── measuring ───────────────────────────────────────────────────────────────────────────────────────────
def screen_axes(cam) -> tuple[Vector, Vector]:
    """(screen-right, screen-up) directions on the ground plane for the camera."""
    r = cam.matrix_world.to_3x3() @ Vector((1, 0, 0))
    r.z = 0
    r.normalize()
    f = cam.matrix_world.to_3x3() @ Vector((0, 0, -1))
    return r, Vector((f.x, f.y, 0.0)).normalized()


def measure(st: dict, mk: np.ndarray, light: tuple[np.ndarray, np.ndarray], glow: np.ndarray, spec: dict,
            ground_l: float) -> dict:
    """The three metrics of one figure from its ink-gate record ``st`` (coverage + final-image crop), its marker
    coverage, its light pass and its emissive mask (all over the same box)."""
    body, hull, rgb = st["body"], st["hull"], st["rgb"]
    shape = body.shape
    mk, glow = _fit(mk, shape), _fit(glow, shape)
    h, rimw = _fit(light[0], shape), _fit(light[1], shape)
    rec = {"rim": {"ok": bool(st["ok"]), "fracRim": st["fracRim"], "rimPxMedian": st["rimPxMedian"],
                   "rimPxP10": st.get("rimPxP10", 0.0), "fracDark": st.get("fracDark", 0.0),
                   "hullLumP95": st.get("hullLumP95", 0.0), "edges": st["edges"], "minPx": st["gate"]["minPx"],
                   "minFrac": st["gate"]["minFrac"],
                   **({"ignoredEmissive": st["ignoredEmissive"]} if st.get("ignoredEmissive") else {})}}
    L = lab_l(rgb)
    solid = (body > 0.98) & (mk < 0.02) & ~glow           # the body's own toon pixels: no marker, no emissive accent
    lit = solid & ((h >= shading.T_LIGHT) | (rimw >= RIM_LIGHT_ON))
    band = "light band + rim light"
    if lit.sum() < MIN_LIT_PX:
        lit = solid & (h >= shading.T_SHADE)
        band = "not-shade bands (fewer than %d light-band / rim pixels)" % MIN_LIT_PX
    shade = solid & (h < shading.T_SHADE)
    ink = (hull > 0.98) & (body < 0.02)
    # Diagnostics against the ground (not gated; implementation-plan.md 0C / DECISIONS R21): the rim gate is the ink hull's
    # width, which a near-black ground cannot see — how far the ink's own lightness sits from the ground's.
    if ink.any():
        ink_dl = np.abs(L[ink] - ground_l)
        rec["rim"].update(inkL=round(float(np.median(L[ink])), 1), inkDeltaL=round(float(np.median(ink_dl)), 1),
                          fracInkClear=round(float((ink_dl >= LIT_DL).mean()), 3))
    if lit.any():
        lit_l = float(np.median(L[lit]))
        # the gate value: the median of the per-pixel |ΔL*| (half the lit side clears the ground by at least this).
        # The signed median of the region (litL − groundL) is kept as a diagnostic: on a lit side with parts both
        # brighter and darker than the ground (a red cap on a pale goblin) it lands near the ground's L* although
        # no part of the figure sits there.
        dl = float(np.median(np.abs(L[lit] - ground_l)))
        rec["lit"] = {"ok": dl >= LIT_DL, "deltaL": round(dl, 1), "signedDeltaL": round(lit_l - ground_l, 1),
                      "litL": round(lit_l, 1), "groundL": round(ground_l, 1), "band": band,
                      "pixels": int(lit.sum()),
                      "litP10": round(float(np.percentile(L[lit], 10)), 1),
                      "litP90": round(float(np.percentile(L[lit], 90)), 1),
                      "fracLitClear": round(float((np.abs(L[lit] - ground_l) >= LIT_DL).mean()), 3),
                      "shadeL": round(float(np.median(L[shade])), 1) if shade.any() else None,
                      "inkL": round(float(np.median(L[ink])), 1) if ink.any() else None}
    else:
        rec["lit"] = {"ok": False, "deltaL": 0.0, "groundL": round(ground_l, 1), "pixels": 0,
                      "note": "no lit body pixels"}
    vis = mk >= 0.5
    blk = largest_block(vis)
    rec["marker"] = {"ok": blk >= MARKER_PX and bool(spec["indices"]), "blockPx": blk, "pixels": int(vis.sum()),
                     "swatches": spec["swatches"], "source": spec["source"]}
    if not spec["indices"]:
        rec["marker"]["note"] = "no marker swatch resolved"
    elif not spec["onMesh"]:
        rec["marker"]["note"] = "marker swatches not on the mesh"
    elif not vis.any():
        rec["marker"]["note"] = "marker hidden at this facing"
    if vis.any():
        rec["marker"]["medianRgb"] = "#%02X%02X%02X" % tuple(int(round(c)) for c in np.median(rgb[vis], 0))
        rec["marker"]["medianL"] = round(float(np.median(L[vis])), 1)
        # Diagnostic (not gated): the marker's lightness against the ground; emissive marker pixels (a glow that reads on
        # any ground) are counted apart.
        rec["marker"]["deltaL"] = round(float(np.median(np.abs(L[vis] - ground_l))), 1)
        rec["marker"]["emissivePixels"] = int((vis & glow).sum())
    rec["ok"] = rec["rim"]["ok"] and rec["lit"]["ok"] and rec["marker"]["ok"]
    edge = []
    if rec["rim"]["ok"] and rec["rim"]["fracRim"] < RIM_FRAC + EDGE_RIM_FRAC:
        edge.append("rim")
    if rec["lit"]["ok"] and rec["lit"]["deltaL"] < LIT_DL + EDGE_DL:
        edge.append("lit")
    if rec["marker"]["ok"] and rec["marker"]["blockPx"] <= MARKER_PX + EDGE_MARKER_PX:
        edge.append("marker")
    rec["atEdge"] = edge
    rec["_lit"], rec["_marker"] = lit, vis
    return rec


# ── one frame ───────────────────────────────────────────────────────────────────────────────────────────
def shoot(page: list, ground: Ground, facing: str, clip: str | None, frame: int, tag: str, out: Path) -> list[dict]:
    K.reset()
    review.setup_render(RES, 8)
    cam = review.game_camera((0.0, 0.0))
    r, _ = screen_axes(cam)
    yaw = review.FACINGS[facing]
    figs = []
    for i, (asset, var, label) in enumerate(page):
        at = r * ((i - (len(page) - 1) / 2) * SPACING)
        figs.append(K.import_character(asset, at, yaw, clip=clip, frame=frame, variant=var, label=label, cam=cam))
    every = [m for f in figs for m in f.all()]
    pts = review.mesh_points(every, step=8)
    c0, c1 = int(pts[:, 0].min()) - 10, int(pts[:, 0].max()) + 11
    r0, r1 = int(-pts[:, 1].max()) - 10, int(-pts[:, 1].min()) + 11
    ppt = 48
    shadows = [(f.root.location.x, -f.root.location.y,
                K.entry(f.asset).get("blobShadowRadiusCm", 30) / 100.0 * f.root.scale[0], 0.45) for f in figs]
    ground_plane(ground.image(r1 - r0, c1 - c0, (c0, r0), ppt), (c0, r0), (r1 - r0, c1 - c0), ppt, shadows,
                 name=f"dg_{tag}")
    bpy.context.view_layer.update()
    box = K.projected_box(cam, every, RES, pad_px=(36, 72))
    if box[0] <= 0.0 or box[1] <= 0.0 or box[2] >= 1.0 or box[3] >= 1.0:
        raise RuntimeError(f"{tag}: the lineup does not fit the 1080p frame {box}; lower --per-page")
    t0 = time.time()
    img, raw = K.render(every, box)
    print(f"  [{tag}] render {time.time() - t0:.1f} s  {img.shape[1]}x{img.shape[0]}")
    gmap = img.copy()
    recs = []
    for f, (_, var, _) in zip(figs, page):
        st = K.ink_gate_group(f.meshes, every, box, img, raw, f.cls, cam, min_px=RIM_PX,
                              emissive=K.LAST_EMISSIVE.get("em"))
        spec = marker_spec(f)
        mk = marker_pass(f, every, spec, st["box"]) if spec["indices"] else np.zeros_like(st["body"])
        ox, oy = st["offset"]
        H, W = st["body"].shape
        em = K.LAST_EMISSIVE.get("em")
        glow = np.zeros((H, W), bool)
        if em is not None:
            es = em[oy:oy + H, ox:ox + W, :3].max(-1)
            glow[:es.shape[0], :es.shape[1]] = es > K.EMISSIVE_EDGE_MIN
        rec = measure(st, mk, light_pass(f, every, st["box"]), glow, spec, ground.stats["L50"])
        sub = gmap[oy:oy + H, ox:ox + W]
        lit, vis = rec.pop("_lit"), rec.pop("_marker")
        h2, w2 = min(H, sub.shape[0]), min(W, sub.shape[1])
        lit, vis = lit[:h2, :w2], vis[:h2, :w2]
        s2 = sub[:h2, :w2].astype(np.float32)
        s2[lit] = s2[lit] * 0.45 + np.array([255, 220, 60]) * 0.55
        s2[vis] = (60, 235, 235) if rec["marker"]["ok"] else (255, 40, 40)
        sub[:h2, :w2] = s2.astype(np.uint8)
        ys, xs = st["edge_yx"]
        ok = st["edge_ok"]
        gmap[ys[ok] + oy, xs[ok] + ox] = (60, 230, 90)
        gmap[ys[~ok] + oy, xs[~ok] + ox] = (240, 60, 60)
        e = K.entry(f.asset)
        rec = {"asset": f.asset, "variant": var,
               "label": f.label, "class": f.cls, "outlinePx1080": f.px, "facing": facing,
               "clip": clip or next((a["name"] for a in e.get("anims", []) if a["name"] == "Idle"), "first"),
               "frame": frame, "marker": rec["marker"], "rim": rec["rim"], "lit": rec["lit"], "ok": rec["ok"],
               "atEdge": rec["atEdge"],
               "_anchor": f.root.location.copy(), "_bottom": oy + int(np.nonzero(st["body"].max(1) > 0.5)[0].max())
               if (st["body"] > 0.5).any() else oy + H}
        print(f"  [gate] {f.label:22s} rim {rec['rim']['fracRim'] * 100:5.1f}% (median {rec['rim']['rimPxMedian']:.2f}px)"
              f"  lit L* {rec['lit'].get('litL', 0):5.1f} vs ground {ground.stats['L50']:5.1f} -> dL "
              f"{rec['lit'].get('deltaL', 0):5.1f} (signed {rec['lit'].get('signedDeltaL', 0):+5.1f})"
              f"  marker {rec['marker']['blockPx']}px ({rec['marker']['pixels']} px)"
              f"  -> {'PASS' if rec['ok'] else 'FAIL'}"
              + (f" (at the edge: {', '.join(rec['atEdge'])})" if rec["atEdge"] else ""))
        recs.append(rec)
    x0, y0 = review.border_origin(box, RES)
    for im in (img, gmap):
        for rec in recs:
            p = review.project_px(cam, rec["_anchor"], RES)
            lines = [(rec["label"], (240, 236, 220)),
                     (f"RIM {rec['rim']['fracRim'] * 100:.0f}%", (160, 230, 160) if rec["rim"]["ok"] else (240, 90, 90)),
                     (f"DL {rec['lit'].get('deltaL', 0):.1f}", (160, 230, 160) if rec["lit"]["ok"] else (240, 90, 90)),
                     (f"MK {rec['marker']['blockPx']}PX", (160, 230, 160) if rec["marker"]["ok"] else (240, 90, 90)),
                     ("PASS" if rec["ok"] else "FAIL", (160, 230, 160) if rec["ok"] else (240, 90, 90))]
            for k, (txt, col) in enumerate(lines):
                tx, ty = int(p[0] - x0 - len(txt) * 3), max(int(p[1] - y0 + 14), rec["_bottom"] + 8) + 9 * k
                review.draw_text(im, max(2, tx), min(im.shape[0] - 9, max(2, ty)), txt, rgb=col)
    head = (f"DARKEST-TILE GATE  {ground.run_key.upper()} L {ground.stats['L50']:.0f}  W1 CAMERA 1080P "
            f"{facing.upper()}")
    write(out / f"{tag}.png", img, head)
    write(out / f"{tag}_map.png", gmap,
          "GATE MAP: EDGE GREEN INK GE 3PX / RED  YELLOW LIT SIDE  CYAN MARKER")
    for rec in recs:
        rec.pop("_anchor", None)
        rec.pop("_bottom", None)
    return recs


def write(path: Path, img: np.ndarray, label: str) -> None:
    def ann(t, i, k):
        review.draw_text(t, 6, 6, label + (f"  TILE {i + 1}/{k}" if k > 1 else ""))
    for p, size in pngio.write_png_native(path, img, annotate=ann):
        print(f"  {p}  {size // 1024} KB")


# ── main ────────────────────────────────────────────────────────────────────────────────────────────────
def parse(argv: list[str]) -> dict:
    opt = {"theme": None, "tile": "darkest", "ground-hex": None, "facing": "se", "clip": None, "frame": "0",
           "per-page": "6", "out": None, "report": None}
    flags, only = set(), []
    for a in argv:
        if a.startswith("--") and "=" in a:
            k, v = a[2:].split("=", 1)
            if k not in opt:
                raise SystemExit(f"unknown option --{k}")
            opt[k] = v
        elif a.startswith("--"):
            flags.add(a[2:])
        else:
            only.append(a)
    unknown = flags - {"no-variants", "no-report"}
    if unknown:
        raise SystemExit(f"unknown flag(s): {', '.join(sorted(unknown))}")
    if opt["facing"] not in review.FACINGS:
        raise SystemExit(f"--facing: one of {', '.join(review.FACINGS)}")
    opt.update(only=only, variants="no-variants" not in flags, write_report="no-report" not in flags)
    return opt


def main(argv: list[str]) -> int:
    o = parse(argv)
    out = Path(o["out"]).resolve() if o["out"] else OUT
    out.mkdir(parents=True, exist_ok=True)
    report_path = Path(o["report"]).resolve() if o["report"] else out / REPORT
    ground = Ground(o["theme"], o["tile"], o["ground-hex"])
    print(f"  ground {ground.run_key} ({ground.selected_by}): L* {ground.stats['L50']} "
          f"(p10 {ground.stats['L10']}, p90 {ground.stats['L90']})")
    for n, s in ground.materials.items():
        print(f"    {n:8s} L* {s['L50']:5.1f}  mean {s['meanRgb']}")
    figs = roster(o["only"], o["variants"])
    if not figs:
        raise SystemExit("no monster to gate")
    per = max(1, int(o["per-page"]))
    pages = [figs[i:i + per] for i in range(0, len(figs), per)]
    recs = []
    for k, page in enumerate(pages):
        tag = f"darkgate_{ground.file_tag}"
        if o["only"]:                                       # a partial roster never overwrites the full lineup
            tag += "_" + "_".join(a.replace("SK_Mon_", "") for a in o["only"])[:48]
        tag += (f"_p{k + 1}" if len(pages) > 1 else "") + (f"_{o['facing']}" if o["facing"] != "se" else "")
        recs += shoot(page, ground, o["facing"], o["clip"], int(o["frame"]), tag, out)
    ok = all(r["ok"] for r in recs)
    run = {"ground": {"theme": ground.theme, "tile": ground.name, "selectedBy": ground.selected_by, **ground.stats,
                      **({"materials": ground.materials} if ground.materials else {}),
                      **({"note": "the march terrain set (A10) is not exported yet: today's darkest walkable "
                                  "plains material stands in for it"}
                         if ground.theme == "plains" and "march" not in K.manifest().get("terrain", {}) else {})},
           "camera": {"yawUE": 45.0, "pitchUE": -50.0, "fovH": 35.0, "distance": "default (16 x 12 tiles)",
                      "res": list(RES)},
           "facing": o["facing"], "pages": len(pages), "pass": ok,
           "failed": [r["label"] for r in recs if not r["ok"]], "figures": recs}
    edge = [f"{r['label']} ({', '.join(r['atEdge'])})" for r in recs if r["ok"] and r["atEdge"]]
    run["atEdge"] = edge
    print(f"  darkest-tile gate on {ground.run_key}: {sum(r['ok'] for r in recs)}/{len(recs)} pass"
          + (f"; at the edge: {'; '.join(edge)}" if edge else ""))
    if o["write_report"]:
        doc = json.loads(report_path.read_text()) if report_path.is_file() else {}
        doc.update({"tool": "Art/blender/coherence/darkgate.py",
                    "spec": "Docs/dark-remake/implementation-plan.md A0 / 0C; monsters.md 1.2",
                    "thresholds": {"rimPx": RIM_PX, "rimFrac": RIM_FRAC, "litDeltaL": LIT_DL, "markerPx": MARKER_PX},
                    "edgeMargins": {"litDeltaL": EDGE_DL, "rimFrac": EDGE_RIM_FRAC, "markerPx": EDGE_MARKER_PX,
                                    "note": "a PASS within these of its threshold is listed in atEdge"},
                    "status": "metric definitions pending confirmation (Docs/DECISIONS.md R21); the plains:grass run "
                              "is the Phase 0 baseline, not an acceptance pass",
                    "metrics": {"rim": "cohkit.ink_gate_group at rimPx: visible ink width on every silhouette edge "
                                       "pixel at native 1080p, gate = rimFrac of them (+ the kit's dark / hull "
                                       "colour checks)",
                                "lit": "median over the figure's solid toon pixels in the key light's light band "
                                       "(h = N.L * .5 + .5 >= T_LIGHT) or rim light (weight >= .5), marker and "
                                       "emissive pixels excluded, of |CIELAB L* - median L* of the ground field| "
                                       ">= litDeltaL (signedDeltaL: the region's median L* minus the ground's, a "
                                       "diagnostic)",
                                "marker": "largest solid k x k block (coverage >= .5) of the visible family-marker "
                                          "swatches, the rest of the figure occluding, >= markerPx",
                                "diagnostics": "not gated: rim.inkL / inkDeltaL / fracInkClear (the ink's L* against "
                                               "the ground: the rim gate is ground-independent), lit.signedDeltaL / "
                                               "fracLitClear, marker.medianL / deltaL / emissivePixels"}})
        key = ground.run_key + ("" if o["facing"] == "se" else f"@{o['facing']}")
        prev = doc.setdefault("runs", {}).get(key)
        if o["only"] and prev:                             # a partial roster updates its figures in the run
            mine = {(r["asset"], r["variant"]) for r in recs}
            run["figures"] = [r for r in prev.get("figures", []) if (r["asset"], r["variant"]) not in mine] + recs
            run["pass"] = all(r["ok"] for r in run["figures"])
            run["failed"] = [r["label"] for r in run["figures"] if not r["ok"]]
            run["pages"] = prev.get("pages", run["pages"])
        doc["runs"][key] = run
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(doc, indent=1, ensure_ascii=False) + "\n")
        print(f"  {report_path}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

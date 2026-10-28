"""Art manifest (unreal/Art/Export/manifest.json, Art/blender/README.md 6) -> a validated, deterministic import plan.

No `unreal` import: the planner runs in plain CPython (tests, `build_content.py --plan`) and inside the editor.

Contract with the runtime (Source/Abyssfire/Public/World/WorldContract.md 2.1, AbyssArtManifest.h):
* an asset exported as Art/Export/<Dir>/<Name>.fbx is imported to /Game/Abyssfire/<Dir>/<Name>; <Dir> = the directory part
  of the manifest `fbx` field, else the asset `category`; clips use their own `fbx` directory;
* palettes -> /Game/Abyssfire/Textures/<T_AF_Palette_*>; portraits / icons / UI glyphs -> /Game/Abyssfire/UI/{Portraits,Icons};
  FX sprites (T_FX_*) -> /Game/Abyssfire/FX/Textures;
* material slot 0 = the family toon instance (MI_AF_Toon_<Family>), slot 1 = the outline hull (M_AF_Outline, made an
  instance per palette and outline class: MI_AF_Outline_<Family>_<Class>);
* the manifest `terrain.<theme>` ground layers + detail atlas -> /Game/Abyssfire/Textures, bound on
  MI_AF_Terrain_<Theme> (parent M_AF_Terrain_Painted), which AAbyssZoneActor::BuildTerrain loads for that theme;
* art-pass look parameters (DECISIONS P11 revised, R16, R19): every M_AF_Toon / M_AF_Toon_Foliage instance gets its
  family's CastShadowStrength, ground cover and the painted ground of a graded theme their saturation / value, and the
  plan always holds MI_AF_SelectRim (plan_look_parameters, SELECT_RIM_DEFAULTS);
* the enemy telegraph (dark remake U2 / 2G): manifest `materials.M_AF_Telegraph` -> the constants M_AF_Telegraph bakes
  as parameter defaults (telegraph_contract) and its two textures (T_FX_TelegraphRim / _Fill), planned even without the
  texture scan;
* the ash husk (dark remake U3 / 2G stage C): manifest `materials.M_AF_AshDissolve` -> the colours, edge width and noise
  tile M_AF_Toon_Ash / M_AF_Outline_Ash bake as parameter defaults (ash_contract) and the noise (T_FX_AshNoise), planned
  even without the texture scan;
* Ashnumb on the hero (dark remake K6 / 2G stage H): Export/VFX/vfx_recipes.json `dark.materials.AshnumbTint` -> the
  colours, band softness, strength and edge width M_AF_Toon_Ashnumb bakes as parameter defaults (ashnumb_contract).
"""
from __future__ import annotations

import json
import math
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable

from . import paths

SUPPORTED_SCHEMA = 1
_NAME_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_]*$")
_HEX_RE = re.compile(r"^#?([0-9A-Fa-f]{6})$")

# README 2 / manifest `shading` defaults (used when a manifest omits a field).
DEFAULT_OUTLINE_PX = {"hero": 3.5, "boss": 3.5, "weapon": 3.5, "npc": 3.0, "monster": 3.0, "small_monster": 3.0,
                      "interactive": 2.0, "decor": 1.2, "none": 0.0}
DEFAULT_BAKED_CM = {"hero": 1.8, "boss": 1.8, "npc": 1.6, "monster": 1.5, "small_monster": 1.4, "interactive": 1.3,
                    "weapon": 1.3, "decor": 1.2, "none": 0.0}

# CastShadowStrength k of the shadow-catcher toon (DECISIONS P11 revised): how far a surface drops toward its shade band
# in a cast sun shadow (shade + (1 - k)(lit - shade)), by palette family (the palette name's first `_` token).
# Characters keep 30 % of their light in a tree's shade (readable), loot 20 %, props and ground cover none; canopies
# (foliage other than the `_Ground` cover) 40 %, so self-shadowed crowns keep their colour.
CAST_SHADOW_BY_FAMILY = {"Heroes": 0.7, "Monsters": 0.7, "NPC": 0.7, "Pets": 0.7, "Weapons": 0.7, "Pickups": 0.8,
                         "Props": 1.0, "Foliage": 0.6}
GROUND_COVER_SUFFIX = "_Ground"            # MI_AF_Toon_Foliage_<Theme>_Ground: grass tufts, flowers
GROUND_COVER_CAST_SHADOW = 1.0
# DECISIONS R19 (ground value): display-space (saturation, value) of the painted ground (MI_AF_Terrain_<Theme>
# GroundSaturation / GroundValue) and its ground cover (BaseSaturation / BaseValue), by theme; other themes as painted.
GROUND_GRADE_BY_THEME = {"plains": (0.85, 0.93)}
# Selection rim (DECISIONS R16): M_AF_SelectRim's defaults = the hero's rim; MI_AF_SelectRim carries them and the
# runtime derives the target / hover rims from it (SELECT_RIM_STYLES: colour, px beyond the ink at 720 rows).
SELECT_RIM_DEFAULTS: dict[str, Any] = {"RimColor": "#FFF4DC", "RimPx720": 2.0, "RimDepthPushCm": 30.0,
                                       "OutlinePx1080": 3.5, "BakedWidthCm": 1.8, "BodyMask": 0.0}
SELECT_RIM_STYLES = {"hero": ("#FFF4DC", 2.0), "target": ("#FF5A3C", 1.5), "hover": ("#FFB03C", 1.5)}


@dataclass
class TelegraphContract:
    """M_AF_Telegraph's constants (manifest materials.M_AF_Telegraph; Art/blender/vfx/fx_recipes_dark.py TELEGRAPH).

    The defaults are the contract as shipped (the runtime's AbyssTelegraphRules::DefaultContract keeps the same copy).
    Per decal and per frame values (shape, quad, style rim widths / inner rims, progress, alphas, flash) are custom
    primitive data the runtime writes; only what is the same for every decal is baked here."""
    fill_color: str = "#FFE8DC"
    rim_color: str = "#D11F2E"
    front_color: str = "#FFF4EE"
    flash_rim_color: str = "#FF3A48"   # contact.look: "the rim to #FF3A48"
    wash_color: str = "#D11F2E"        # styles.arena.outerWash.color
    fill_grain: float = 0.3
    fill_tile_cm: float = 160.0
    front_width_cm: float = 5.0
    rim_tile_cm: float = 120.0
    wash_fade_in_cm: float = 60.0      # styles.arena.outerWash.fadeInCm
    rim_texture: str = "T_FX_TelegraphRim"
    fill_texture: str = "T_FX_TelegraphFill"
    present: bool = False              # the manifest carries the contract (else these defaults)


def telegraph_contract(data: dict[str, Any], problems: list[Problem] | None = None) -> TelegraphContract:
    """The contract's material constants; a malformed colour is an error, a missing field keeps the default."""
    t = TelegraphContract()
    raw = (data.get("materials") or {}).get(paths.M_TELEGRAPH)
    if not isinstance(raw, dict):
        return t
    t.present = True
    params = raw.get("params") or {}

    def colour(value: Any, default: str, subject: str) -> str:
        if value is None:
            return default
        text = str(value)
        if not _HEX_RE.match(text):
            if problems is not None:
                problems.append(Problem("error", f"materials {paths.M_TELEGRAPH}", f"{subject}: not a #RRGGBB colour"))
            return default
        return text if text.startswith("#") else f"#{text}"

    t.fill_color = colour(params.get("FillColor"), t.fill_color, "params.FillColor")
    t.rim_color = colour(params.get("RimColor"), t.rim_color, "params.RimColor")
    t.front_color = colour(params.get("FrontColor"), t.front_color, "params.FrontColor")
    for attr, key in (("fill_grain", "FillGrain"), ("fill_tile_cm", "FillTileCm"), ("front_width_cm", "FrontWidthCm"),
                      ("rim_tile_cm", "RimTileCm")):
        value = params.get(key)
        if isinstance(value, (int, float)):
            setattr(t, attr, float(value))
    look = str((raw.get("contact") or {}).get("look") or "")
    flash = re.search(r"rim to (#[0-9A-Fa-f]{6})", look)
    if flash:
        t.flash_rim_color = flash.group(1)
    wash = ((raw.get("styles") or {}).get("arena") or {}).get("outerWash") or {}
    t.wash_color = colour(wash.get("color"), t.wash_color, "styles.arena.outerWash.color")
    if isinstance(wash.get("fadeInCm"), (int, float)):
        t.wash_fade_in_cm = float(wash["fadeInCm"])
    textures = raw.get("textures") or {}
    for attr, key in (("rim_texture", "Rim"), ("fill_texture", "Fill")):
        name = (textures.get(key) or {}).get("texture")
        if isinstance(name, str) and _NAME_RE.match(name):
            setattr(t, attr, name)
    return t

@dataclass
class AshContract:
    """M_AF_AshDissolve's constants (manifest materials.M_AF_AshDissolve; Art/blender/vfx/fx_recipes_dark.py
    ASH_DISSOLVE). The defaults are the contract as shipped (the runtime's AbyssCorpseRules::FAshContract keeps the same
    copy); Grey / Dissolve / HuskTopCm are the runtime's per-body scalars, the timeline is the runtime's."""
    ash_color: str = "#6E6A62"
    ash_shade: str = "#3E3B37"
    edge_color: str = "#B8B2A6"
    edge_width: float = 0.06
    noise_tile_cm: float = 60.0        # textures.Noise.map: "object-space XZ / 60 cm"
    noise_texture: str = "T_FX_AshNoise"
    present: bool = False              # the manifest carries the contract (else these defaults)


def ash_contract(data: dict[str, Any], problems: list[Problem] | None = None) -> AshContract:
    """The contract's material constants; a malformed colour is an error, a missing field keeps the default."""
    a = AshContract()
    raw = (data.get("materials") or {}).get("M_AF_AshDissolve")
    if not isinstance(raw, dict):
        return a
    a.present = True
    params = raw.get("params") or {}
    for attr, key in (("ash_color", "AshColor"), ("ash_shade", "AshShade"), ("edge_color", "EdgeColor")):
        value = params.get(key)
        if value is None:
            continue
        text = str(value)
        if not _HEX_RE.match(text):
            if problems is not None:
                problems.append(Problem("error", "materials M_AF_AshDissolve", f"params.{key}: not a #RRGGBB colour"))
            continue
        setattr(a, attr, text if text.startswith("#") else f"#{text}")
    edge = params.get("EdgeWidth")
    if isinstance(edge, (int, float)) and 0.0 <= float(edge) <= 1.0:
        a.edge_width = float(edge)
    noise = (raw.get("textures") or {}).get("Noise") or {}
    name = noise.get("texture")
    if isinstance(name, str) and _NAME_RE.match(name):
        a.noise_texture = name
    tile = re.search(r"([0-9]+(?:\.[0-9]+)?) cm", str(noise.get("map") or ""))
    if tile and float(tile.group(1)) > 0.0:
        a.noise_tile_cm = float(tile.group(1))
    return a


@dataclass
class AshnumbContract:
    """M_AF_Toon_Ashnumb's constants (Art/Export/VFX/vfx_recipes.json dark.materials.AshnumbTint; Art/blender/vfx/
    fx_recipes_dark.py ASHNUMB, the band mock vfx/ashnumb_band.py). The defaults are the contract as shipped (the
    runtime's AbyssWorldStateRules::FAshnumbTint keeps the same copy). The band's top (min(BandTopBase + stacks x
    BandTopPerStack, MaxBandTop)) and the hero's height are the runtime's per-hero scalars BandTop / BandHeightCm."""
    ash_color: str = "#8A8480"
    ash_shade: str = "#4C4944"
    edge_color: str = "#B8B2A6"
    edge_width: float = 0.04
    band_softness: float = 0.06
    strength: float = 0.85
    present: bool = False              # the recipes carry the contract (else these defaults)


def load_vfx_recipes(path: Path | None = None) -> dict[str, Any]:
    """The art lane's vfx_recipes.json ({} when it is missing or unreadable: every contract keeps its defaults)."""
    path = path or paths.VFX_RECIPES_PATH
    try:
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, ValueError):
        return {}
    return data if isinstance(data, dict) else {}


def ashnumb_contract(recipes: dict[str, Any], problems: list[Problem] | None = None) -> AshnumbContract:
    """The contract's material constants; a malformed colour is an error, a missing or out-of-range number keeps the
    default."""
    a = AshnumbContract()
    raw = (((recipes.get("dark") or {}).get("materials") or {}).get("AshnumbTint"))
    if not isinstance(raw, dict):
        return a
    a.present = True
    params = raw.get("params") or {}
    for attr, key in (("ash_color", "AshColor"), ("ash_shade", "AshShade"), ("edge_color", "EdgeColor")):
        value = params.get(key)
        if value is None:
            continue
        text = str(value)
        if not _HEX_RE.match(text):
            if problems is not None:
                problems.append(Problem("error", "vfx_recipes AshnumbTint", f"params.{key}: not a #RRGGBB colour"))
            continue
        setattr(a, attr, text if text.startswith("#") else f"#{text}")
    for attr, key in (("edge_width", "EdgeWidth"), ("band_softness", "BandSoftness"), ("strength", "Strength")):
        value = params.get(key)
        if isinstance(value, (int, float)) and not isinstance(value, bool) and 0.0 <= float(value) <= 1.0:
            setattr(a, attr, float(value))
    return a


# Additive base pose: manifest `additiveBase.type` -> unreal.AdditiveBasePoseType member.
ADDITIVE_BASE_TYPES = {
    "LocalAnimFrame": "ABPT_LOCAL_ANIM_FRAME",
    "AnimFrame": "ABPT_ANIM_FRAME",
    "AnimScaled": "ABPT_ANIM_SCALED",
    "RefPose": "ABPT_REF_POSE",
}


class ManifestError(RuntimeError):
    pass


# ---------------------------------------------------------------------------------------------------------------------
# Colour helpers (sRGB hex <-> floats); shared with hlsl.py
# ---------------------------------------------------------------------------------------------------------------------
def hex_to_rgb255(value: str) -> tuple[float, float, float]:
    m = _HEX_RE.match(value.strip())
    if not m:
        raise ManifestError(f"not a #RRGGBB colour: {value!r}")
    h = m.group(1)
    return float(int(h[0:2], 16)), float(int(h[2:4], 16)), float(int(h[4:6], 16))


def srgb_to_linear1(c: float) -> float:
    c = min(max(c, 0.0), 1.0)
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def linear_to_srgb1(c: float) -> float:
    c = min(max(c, 0.0), 1.0)
    return c * 12.92 if c <= 0.0031308 else 1.055 * c ** (1.0 / 2.4) - 0.055


def hex_to_linear(value: str) -> tuple[float, float, float]:
    r, g, b = hex_to_rgb255(value)
    return srgb_to_linear1(r / 255.0), srgb_to_linear1(g / 255.0), srgb_to_linear1(b / 255.0)


# ---------------------------------------------------------------------------------------------------------------------
# Shading contract (manifest `shading`, Art/blender/README.md 2)
# ---------------------------------------------------------------------------------------------------------------------
@dataclass
class Shading:
    t_shade: float = 0.42
    t_light: float = 0.86
    band_width: float = 0.02
    cool_shade: tuple[float, float, float] = (30.0, 20.0, 60.0)        # sRGB 0..255
    cool_shade_mix: float = 0.35
    warm_light: tuple[float, float, float] = (255.0, 244.0, 214.0)
    line_tint: tuple[float, float, float] = (20.0, 10.0, 30.0)
    line_tint_mix: float = 0.5
    line_darken: float = 0.55
    ground_color: str = "#0A0818"
    ground_alpha: float = 0.22
    rim_color: str = "#FFECC8"
    rim_alpha: float = 0.55
    rim_band: tuple[float, float] = (0.56 ** 4, 0.62 ** 4)
    rim_power: float = 4.0
    rim_mask_edge: float = 0.05
    emissive_boost: float = 0.6
    ink: str = "#120C18"
    key_light: tuple[float, float, float] = (0.40558, -0.40558, 0.819152)   # unit vector towards the light, UE axes
    outline_eps: float = 0.25
    outline_px: dict[str, float] = field(default_factory=lambda: dict(DEFAULT_OUTLINE_PX))
    baked_width_cm: dict[str, float] = field(default_factory=lambda: dict(DEFAULT_BAKED_CM))
    camera_fov_h: float = 35.0

    @staticmethod
    def from_manifest(data: dict[str, Any]) -> "Shading":
        s = Shading()
        sh = data.get("shading") or {}
        s.t_shade = float(sh.get("tShade", s.t_shade))
        s.t_light = float(sh.get("tLight", s.t_light))
        s.band_width = float(sh.get("bandWidth", s.band_width))
        tc = sh.get("toneConstants") or {}
        s.cool_shade = _vec3(tc.get("coolShade"), s.cool_shade)
        s.cool_shade_mix = float(tc.get("coolShadeMix", s.cool_shade_mix))
        s.warm_light = _vec3(tc.get("warmLight"), s.warm_light)
        s.line_tint = _vec3(tc.get("lineTint"), s.line_tint)
        s.line_tint_mix = float(tc.get("lineTintMix", s.line_tint_mix))
        s.line_darken = float(tc.get("lineDarken", s.line_darken))
        gr = sh.get("grounding") or {}
        s.ground_color = str(gr.get("color", s.ground_color))
        s.ground_alpha = float(gr.get("alpha", s.ground_alpha))
        rim = sh.get("rim") or {}
        s.rim_color = str(rim.get("color", s.rim_color))
        s.rim_alpha = float(rim.get("alpha", s.rim_alpha))
        band = rim.get("band")
        if isinstance(band, list) and len(band) == 2:
            s.rim_band = (float(band[0]), float(band[1]))
        s.rim_power = float(rim.get("power", s.rim_power))
        s.rim_mask_edge = float(rim.get("maskEdge", s.rim_mask_edge))
        s.emissive_boost = float(sh.get("emissiveBoost", s.emissive_boost))
        s.ink = str(sh.get("ink", s.ink))
        key = (sh.get("keyLight") or {}).get("dirToLightUE")
        if isinstance(key, list) and len(key) == 3:
            k = [float(x) for x in key]
            n = math.sqrt(sum(x * x for x in k)) or 1.0
            s.key_light = (k[0] / n, k[1] / n, k[2] / n)
        outline = sh.get("outline") or {}
        s.outline_px.update({str(k): float(v) for k, v in (outline.get("outlinePx1080ByClass") or {}).items()})
        s.baked_width_cm.update({str(k): float(v) for k, v in (outline.get("bakedWidthCm") or {}).items()})
        color = outline.get("color") or {}
        s.ink = str(color.get("ink", s.ink))
        s.line_tint = _vec3(color.get("lineTint"), s.line_tint)
        s.line_tint_mix = float(color.get("lineTintMix", s.line_tint_mix))
        s.line_darken = float(color.get("lineDarken", s.line_darken))
        s.outline_eps = float((outline.get("wpo") or {}).get("eps", s.outline_eps))
        s.camera_fov_h = float((sh.get("camera") or {}).get("fovH", s.camera_fov_h))
        # Validate the colours early (fail loudly, README 11.1).
        for value in (s.ground_color, s.rim_color, s.ink):
            hex_to_rgb255(value)
        return s


def _vec3(value: Any, default: tuple[float, float, float]) -> tuple[float, float, float]:
    if isinstance(value, list) and len(value) == 3:
        return float(value[0]), float(value[1]), float(value[2])
    return default


# ---------------------------------------------------------------------------------------------------------------------
# Plan data
# ---------------------------------------------------------------------------------------------------------------------
@dataclass
class Palette:
    family: str
    material: str
    parent: str
    bc_name: str
    bc_file: Path
    p_name: str
    p_file: Path


@dataclass
class MaterialSlot:
    index: int
    name: str
    parent: str
    palette: str
    outline_class: str = ""
    width_cm: float | None = None
    px1080: float | None = None
    params: dict[str, Any] = field(default_factory=dict)

    @property
    def is_outline(self) -> bool:
        return self.parent in paths.OUTLINE_MASTERS or self.name.startswith(paths.M_OUTLINE)


@dataclass
class Clip:
    name: str
    asset: str
    fbx: Path
    folder: str
    length_ms: float
    frames: int
    fps: int
    loop: bool
    additive: bool = False
    additive_base_type: str = ""     # unreal.AdditiveBasePoseType member name
    additive_base_anim: str = ""
    additive_base_frame: int = 0

    @property
    def content_path(self) -> str:
        return f"{paths.CONTENT_ROOT}/{self.folder}/{self.asset}"


@dataclass
class Socket:
    name: str
    bone: str
    loc: tuple[float, float, float]
    rot: tuple[float, float, float]


@dataclass
class Lod:
    index: int
    asset: str
    fbx: Path
    screen_size: float


@dataclass
class Asset:
    name: str
    kind: str                    # "SkeletalMesh" | "StaticMesh"
    category: str
    folder: str
    fbx: Path
    skeleton: str = ""
    slots: list[MaterialSlot] = field(default_factory=list)
    clips: list[Clip] = field(default_factory=list)
    sockets: list[Socket] = field(default_factory=list)
    lods: list[Lod] = field(default_factory=list)
    height_cm: float = 0.0
    empty_slots: list[MaterialSlot] = field(default_factory=list)   # manifest slots without triangles (not imported)

    @property
    def skeletal(self) -> bool:
        return self.kind == "SkeletalMesh"

    @property
    def content_path(self) -> str:
        return f"{paths.CONTENT_ROOT}/{self.folder}/{self.name}"

    @property
    def toon_parent(self) -> str:
        for slot in self.slots:
            if not slot.is_outline:
                return slot.parent
        return paths.M_TOON


@dataclass
class TextureEntry:
    name: str
    source: Path
    folder: str
    kind: str      # palette_bc | palette_p | ui_icon | ui_portrait | fx | world | normal | mask | data
    srgb: bool

    @property
    def content_path(self) -> str:
        return f"{self.folder}/{self.name}"


@dataclass
class InstancePlan:
    name: str
    parent: str
    palette: str = ""
    scalars: dict[str, float] = field(default_factory=dict)
    vectors: dict[str, tuple[float, float, float, float]] = field(default_factory=dict)
    textures: dict[str, str] = field(default_factory=dict)       # parameter -> texture asset name
    used_by: list[str] = field(default_factory=list)

    def signature(self) -> tuple:
        return (self.parent, self.palette, tuple(sorted(self.scalars.items())), tuple(sorted(self.vectors.items())),
                tuple(sorted(self.textures.items())))


@dataclass
class Problem:
    level: str      # "error" | "warning" | "note"
    subject: str
    message: str

    def __str__(self) -> str:
        return f"[{self.level}] {self.subject}: {self.message}"


@dataclass
class Plan:
    manifest_path: Path
    export_dir: Path
    shading: Shading
    palettes: dict[str, Palette]
    assets: list[Asset]
    skeleton_families: dict[str, list[str]]          # skeleton -> skeletal asset names (first one creates it)
    textures: list[TextureEntry]
    instances: dict[str, InstancePlan]
    slot_materials: dict[str, dict[int, str]]         # asset -> slot index -> material / instance name
    problems: list[Problem] = field(default_factory=list)
    skipped: list[str] = field(default_factory=list)  # assets dropped by the family filter or errors
    telegraph: TelegraphContract = field(default_factory=TelegraphContract)   # M_AF_Telegraph's constants
    ash: AshContract = field(default_factory=AshContract)                     # M_AF_Toon_Ash / _Outline_Ash's constants
    ashnumb: AshnumbContract = field(default_factory=AshnumbContract)         # M_AF_Toon_Ashnumb's constants

    def texture(self, name: str) -> TextureEntry | None:
        for t in self.textures:
            if t.name == name:
                return t
        return None

    @property
    def errors(self) -> list[Problem]:
        return [p for p in self.problems if p.level == "error"]

    def asset(self, name: str) -> Asset | None:
        for a in self.assets:
            if a.name == name:
                return a
        return None

    def summary(self) -> dict[str, Any]:
        skeletal = [a for a in self.assets if a.skeletal]
        static = [a for a in self.assets if not a.skeletal]
        return {
            "manifest": str(self.manifest_path),
            "palettes": sorted(self.palettes),
            "skeletalMeshes": [a.name for a in skeletal],
            "staticMeshes": [a.name for a in static],
            "skeletons": {k: list(v) for k, v in sorted(self.skeleton_families.items())},
            "animations": sum(len(a.clips) for a in skeletal),
            "lods": sum(len(a.lods) for a in skeletal),
            "textures": len(self.textures),
            "materialInstances": sorted(self.instances),
            "skipped": list(self.skipped),
            "problems": [str(p) for p in self.problems],
        }


# ---------------------------------------------------------------------------------------------------------------------
# Loading
# ---------------------------------------------------------------------------------------------------------------------
def load_manifest(path: Path) -> dict[str, Any]:
    try:
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
    except FileNotFoundError as e:
        raise ManifestError(f"no art manifest at {path} (run the Blender exporters, Art/blender/README.md)") from e
    except json.JSONDecodeError as e:
        raise ManifestError(f"{path}: invalid JSON: {e}") from e
    if not isinstance(data, dict):
        raise ManifestError(f"{path}: the manifest must be a JSON object")
    if data.get("schemaVersion") != SUPPORTED_SCHEMA:
        raise ManifestError(f"{path}: unsupported schemaVersion {data.get('schemaVersion')!r} "
                            f"(this build understands {SUPPORTED_SCHEMA})")
    return data


def runtime_copy_problem(manifest_path: Path, copy_path: Path) -> Problem | None:
    """Data/assets.json is the exporter's copy of the art manifest, and the game reads it before the art file
    (UAbyssAssetLibrary, the core DataStore's decor footprints). A copy that lags the export hides every asset added
    since at runtime without an error (the first playtest loaded 17 of 198 assets: no foliage, props, NPCs or loot
    bags), although this build imports them all. None when the copy matches or does not exist (the game then reads
    the art file)."""
    if not copy_path.is_file():
        return None
    subject = f"{copy_path.parent.name}/{copy_path.name}"
    try:
        with open(copy_path, encoding="utf-8") as f:
            envelope = json.load(f)
        with open(manifest_path, encoding="utf-8") as f:
            art = json.load(f)
    except (OSError, json.JSONDecodeError) as e:
        return Problem("warning", subject, f"could not compare with {manifest_path}: {e}")
    inner = envelope.get("manifest") if isinstance(envelope, dict) else None
    if inner == art:
        return None

    def count(m: Any) -> int:
        return len(m.get("assets") or {}) if isinstance(m, dict) else 0

    return Problem("warning", subject,
                   f"stale copy of {manifest_path.name} ({count(inner)} vs {count(art)} assets): the game reads the "
                   "copy, so assets missing from it never show; re-export from the repo root with "
                   "`node unreal/Tools/export-data/run.mjs`")


def _folder_of(fbx: str, category: str) -> str:
    rel = fbx.replace("\\", "/").strip("/")
    if "/" in rel:
        return rel.rsplit("/", 1)[0]
    return category or "Misc"


def _pascal(token: str) -> str:
    return "".join(part[:1].upper() + part[1:] for part in re.split(r"[^A-Za-z0-9]+", token) if part)


def _width_token(value: float) -> str:
    return f"{value:g}".replace(".", "p").replace("-", "m")


def outline_instance_name(palette: str, outline_class: str, foliage: bool, px: float | None = None,
                          baked: float | None = None) -> str:
    """One outline instance per palette + class; a slot that overrides the class widths gets its own suffix
    (_Px<px> / _Cm<baked>, '.' -> 'p'), so two widths never fight over one instance."""
    base = "MI_AF_OutlineFoliage" if foliage else "MI_AF_Outline"
    name = f"{base}_{palette}_{_pascal(outline_class) or 'Default'}"
    if px is not None:
        name += f"_Px{_width_token(px)}"
    if baked is not None:
        name += f"_Cm{_width_token(baked)}"
    return name


def _param_value(name: str, value: Any, subject: str, problems: list[Problem]
                 ) -> tuple[str, float | tuple[float, float, float, float]] | None:
    """Manifest slot `params`: number -> scalar; [r,g,b(,a)] (linear) or "#RRGGBB" (sRGB) -> vector."""
    if isinstance(value, bool):
        return "scalar", 1.0 if value else 0.0
    if isinstance(value, (int, float)):
        return "scalar", float(value)
    if isinstance(value, str):
        try:
            r, g, b = hex_to_linear(value)
        except ManifestError as e:
            problems.append(Problem("warning", subject, f"param {name}: {e}"))
            return None
        return "vector", (r, g, b, 1.0)
    if isinstance(value, list) and len(value) in (3, 4) and all(isinstance(x, (int, float)) for x in value):
        v = [float(x) for x in value] + ([1.0] if len(value) == 3 else [])
        return "vector", (v[0], v[1], v[2], v[3])
    problems.append(Problem("warning", subject, f"param {name}: unsupported value {value!r}"))
    return None


def _slot_triangles(tri: Any, slot: MaterialSlot) -> int | None:
    """Triangles a manifest `triangles` record ({toon, outline[, bySlot]}) gives one slot; None when it does not say."""
    if not isinstance(tri, dict):
        return None
    by_slot = tri.get("bySlot")
    if isinstance(by_slot, dict) and isinstance(by_slot.get(slot.name), (int, float)):
        return int(by_slot[slot.name])
    if slot.is_outline and isinstance(tri.get("outline"), (int, float)):
        return int(tri["outline"])      # the total over the outline slots: 0 = every outline slot is empty
    return None


def _empty_outline_slot(raw: dict[str, Any], slot: MaterialSlot) -> bool:
    """An outline slot whose hull has no triangles in LOD0 nor in any LOD (e.g. an NPC kit moustache too thin for a
    hull): Interchange drops a material slot no face uses, so the build must not expect it."""
    if not slot.is_outline:
        return False
    counts = [_slot_triangles(raw.get("triangles"), slot)]
    counts += [_slot_triangles(lod.get("triangles"), slot) for lod in raw.get("lods") or [] if isinstance(lod, dict)]
    return all(c == 0 for c in counts)


FBX_KTIME_PER_SECOND = 46186158000


def fbx_anim_stack_seconds(path: Path) -> tuple[float, float] | None:
    """(LocalStart, LocalStop) of the animation stack of a binary FBX in seconds; None when unreadable.

    Reads the AnimationStack's Properties70 records `P: "LocalStop", "KTime", "Time", "", <int64 ticks>` directly (the
    last occurrence: the Definitions template that precedes the objects holds the defaults). A record left at its
    template default (0) is not written."""
    try:
        data = path.read_bytes()
    except OSError:
        return None
    if not data.startswith(b"Kaydara FBX Binary"):
        return None
    span: list[float] = []
    for key in (b"LocalStart", b"LocalStop"):
        at = data.rfind(b"S" + len(key).to_bytes(4, "little") + key)
        if at < 0:
            return None
        pos = at + 5 + len(key)
        for _ in range(3):              # "KTime", "Time", ""
            if data[pos:pos + 1] != b"S":
                return None
            pos += 5 + int.from_bytes(data[pos + 1:pos + 5], "little")
        if data[pos:pos + 1] != b"L":
            return None
        span.append(int.from_bytes(data[pos + 1:pos + 9], "little", signed=True) / FBX_KTIME_PER_SECOND)
    return span[0], span[1]


def _matches_family(asset: Asset, families: set[str]) -> bool:
    if not families:
        return True
    keys = {asset.category.lower(), asset.folder.lower(), asset.name.lower()}
    keys |= {s.palette.lower() for s in asset.slots}
    for fam in families:
        f = fam.lower()
        if f in keys or any(f in k for k in keys):
            return True
        # "heroes" also matches SK_Hero_* (plural forms on the command line)
        if f.endswith("es") and any(f[:-2] in k for k in keys):
            return True
        if f.endswith("s") and any(f[:-1] in k for k in keys):
            return True
    return False


def build_plan(manifest_path: Path | None = None, export_dir: Path | None = None,
               families: Iterable[str] = (), scan_textures: bool = True) -> Plan:
    manifest_path = manifest_path or paths.MANIFEST_PATH
    export_dir = export_dir or manifest_path.parent
    data = load_manifest(manifest_path)
    shading = Shading.from_manifest(data)
    problems: list[Problem] = []
    fam_filter = {f.strip() for f in families if f.strip()}

    # ---- palettes ----
    palettes: dict[str, Palette] = {}
    for family, pal in sorted((data.get("palettes") or {}).items()):
        bc = pal.get("baseColor") or {}
        pp = pal.get("params") or {}
        try:
            palette = Palette(
                family=family,
                material=str(pal.get("material") or f"MI_AF_Toon_{family}"),
                parent=str(pal.get("parent") or paths.M_TOON),
                bc_name=str(bc["name"]),
                bc_file=export_dir / str(bc["file"]),
                p_name=str(pp["name"]),
                p_file=export_dir / str(pp["file"]),
            )
        except KeyError as e:
            problems.append(Problem("error", f"palette {family}", f"missing field {e}"))
            continue
        for f in (palette.bc_file, palette.p_file):
            if not f.is_file():
                problems.append(Problem("error", f"palette {family}", f"texture file missing: {f}"))
        if bc.get("srgb") is False:
            problems.append(Problem("warning", f"palette {family}", "baseColor.srgb is false; importing as sRGB anyway "
                                    "(M_AF_Toon samples it as Color and re-encodes)"))
        palettes[family] = palette

    # ---- assets ----
    assets: list[Asset] = []
    skipped: list[str] = []
    seen: dict[str, str] = {}
    for name, raw in sorted((data.get("assets") or {}).items()):
        kind = str(raw.get("kind", ""))
        subject = name
        if kind not in ("SkeletalMesh", "StaticMesh"):
            problems.append(Problem("warning", subject, f"unknown kind {kind!r}: skipped"))
            skipped.append(name)
            continue
        if not _NAME_RE.match(name):
            problems.append(Problem("error", subject, "not a valid UE asset name"))
            skipped.append(name)
            continue
        fbx_rel = str(raw.get("fbx") or "")
        if not fbx_rel:
            problems.append(Problem("error", subject, "no `fbx` field"))
            skipped.append(name)
            continue
        category = str(raw.get("category") or "")
        folder = _folder_of(fbx_rel, category)
        fbx = export_dir / fbx_rel
        expected_prefix = "SK_" if kind == "SkeletalMesh" else "SM_"
        if not name.startswith(expected_prefix):
            problems.append(Problem("warning", subject, f"{kind} names should start with {expected_prefix}"))
        if Path(fbx_rel).stem != name:
            problems.append(Problem("warning", subject, f"fbx file name {Path(fbx_rel).name} differs from the asset name "
                                    "(Interchange names the asset after the source; the build renames it)"))
        asset = Asset(name=name, kind=kind, category=category, folder=folder, fbx=fbx,
                      skeleton=str(raw.get("skeleton") or ""), height_cm=float(raw.get("heightCm") or 0.0))
        if not fbx.is_file():
            problems.append(Problem("error", subject, f"FBX missing: {fbx}"))
            skipped.append(name)
            continue
        if asset.skeletal and not asset.skeleton:
            asset.skeleton = f"SKEL_{name[3:]}"
            problems.append(Problem("warning", subject, f"no skeleton named: using {asset.skeleton}"))

        # material slots
        for slot_raw in sorted(raw.get("materialSlots") or [], key=lambda s: int(s.get("index", 0))):
            slot = MaterialSlot(
                index=int(slot_raw.get("index", 0)),
                name=str(slot_raw.get("name") or ""),
                parent=str(slot_raw.get("parent") or ""),
                palette=str(slot_raw.get("palette") or ""),
                outline_class=str(slot_raw.get("class") or ""),
                width_cm=float(slot_raw["widthCm"]) if slot_raw.get("widthCm") is not None else None,
                px1080=float(slot_raw["outlinePx1080"]) if slot_raw.get("outlinePx1080") is not None else None,
                params=dict(slot_raw.get("params") or {}),
            )
            if slot.palette and slot.palette not in palettes:
                problems.append(Problem("error", subject, f"slot {slot.index} uses unknown palette {slot.palette!r}"))
            if _empty_outline_slot(raw, slot):
                asset.empty_slots.append(slot)
                problems.append(Problem("note", subject, f"slot {slot.index} ({slot.name}) has no triangles (manifest "
                                        "`triangles`): not planned, Interchange imports no such slot"))
                continue
            asset.slots.append(slot)
        if not asset.slots:
            problems.append(Problem("warning", subject, "no material slots in the manifest (engine default material)"))

        # sockets
        for sock in raw.get("sockets") or []:
            loc = sock.get("relLocCm") if asset.skeletal else sock.get("locCm")
            rot = sock.get("relRotDeg") if asset.skeletal else sock.get("rotDeg")
            asset.sockets.append(Socket(
                name=str(sock.get("name") or ""),
                bone=str(sock.get("bone") or ""),
                loc=_vec3(loc, (0.0, 0.0, 0.0)),
                rot=_vec3(rot, (0.0, 0.0, 0.0)),
            ))
            if not asset.sockets[-1].name or (asset.skeletal and not asset.sockets[-1].bone):
                problems.append(Problem("warning", subject, f"socket without name / bone: {sock!r}"))
                asset.sockets.pop()

        # clips and LODs (skeletal only)
        if asset.skeletal:
            clip_names: set[str] = set()
            for clip_raw in raw.get("anims") or []:
                cname = str(clip_raw.get("asset") or "")
                cfbx_rel = str(clip_raw.get("fbx") or "")
                if not cname or not cfbx_rel:
                    problems.append(Problem("error", subject, f"clip without asset / fbx: {clip_raw.get('name')}"))
                    continue
                if cname in clip_names:
                    problems.append(Problem("error", subject, f"duplicate clip {cname}"))
                    continue
                clip_names.add(cname)
                cfbx = export_dir / cfbx_rel
                if not cfbx.is_file():
                    problems.append(Problem("error", subject, f"clip FBX missing: {cfbx}"))
                    continue
                clip = Clip(
                    name=str(clip_raw.get("name") or cname),
                    asset=cname,
                    fbx=cfbx,
                    folder=_folder_of(cfbx_rel, folder),
                    length_ms=float(clip_raw.get("lengthMs") or 0.0),
                    frames=int(clip_raw.get("frames") or 0),
                    fps=int(clip_raw.get("fps") or 60),
                    loop=bool(clip_raw.get("loop", False)),
                    additive=bool(clip_raw.get("additive", False)),
                )
                if not cname.startswith("A_"):
                    problems.append(Problem("warning", subject, f"clip {cname} should start with A_"))
                # The import takes frames 0..frames explicitly (ue/meshes.py set_anim_range): the FBX must agree.
                span = fbx_anim_stack_seconds(cfbx)
                if span is not None and clip.frames > 0 and (abs(span[0]) * clip.fps > 0.01 or abs(
                        (span[1] - span[0]) * clip.fps - clip.frames) > 0.01):
                    problems.append(Problem("warning", subject, f"clip {cname}: the FBX animation stack spans frames "
                                            f"{span[0] * clip.fps:.2f}..{span[1] * clip.fps:.2f} at {clip.fps} fps, "
                                            f"the manifest {clip.frames} frames (imported as frames 0..{clip.frames})"))
                if clip.additive:
                    base = clip_raw.get("additiveBase") or {}
                    btype = str(base.get("type") or "LocalAnimFrame")
                    if btype not in ADDITIVE_BASE_TYPES:
                        problems.append(Problem("warning", subject, f"clip {cname}: unknown additive base {btype!r}, "
                                                "using LocalAnimFrame"))
                        btype = "LocalAnimFrame"
                    clip.additive_base_type = ADDITIVE_BASE_TYPES[btype]
                    clip.additive_base_anim = str(base.get("anim") or "")
                    clip.additive_base_frame = int(base.get("frame") or 0)
                asset.clips.append(clip)
            for lod_raw in raw.get("lods") or []:
                lfbx_rel = str(lod_raw.get("fbx") or "")
                lfbx = export_dir / lfbx_rel
                if not lfbx_rel or not lfbx.is_file():
                    problems.append(Problem("warning", subject, f"LOD FBX missing: {lfbx} (LOD skipped)"))
                    continue
                asset.lods.append(Lod(index=int(lod_raw.get("index") or 1), asset=str(lod_raw.get("asset") or ""),
                                      fbx=lfbx, screen_size=float(lod_raw.get("screenSize") or 0.3)))
            asset.lods.sort(key=lambda lod: lod.index)

        if name in seen:
            problems.append(Problem("error", subject, f"duplicate asset name (also in {seen[name]})"))
            skipped.append(name)
            continue
        seen[name] = folder

        if not _matches_family(asset, fam_filter):
            skipped.append(name)
            continue
        assets.append(asset)

    # W5: the core bakes a decoration's blocking footprint from the FIRST asset listing its game id (manifest key order),
    # the UE shows a hash-picked variant among all of them (AAbyssZoneActor::BuildDecorations): every asset sharing a
    # game id must agree on footprintTiles and blocking, or collision and mesh diverge.
    by_game_id: dict[str, list[tuple[str, Any, Any]]] = {}
    for name, raw in (data.get("assets") or {}).items():
        for gid in raw.get("gameIds") or []:
            by_game_id.setdefault(str(gid), []).append((name, raw.get("footprintTiles"), raw.get("blocking")))
    for gid, owners in sorted(by_game_id.items()):
        shapes = {(json.dumps(fp), json.dumps(bl)) for _, fp, bl in owners}
        if len(shapes) > 1:
            detail = ", ".join(f"{n} (footprintTiles {fp}, blocking {bl})" for n, fp, bl in owners)
            problems.append(Problem("error", f"game id {gid}", f"variants disagree on footprint / blocking: {detail}"))

    # Clip names must be unique across assets too (one content folder per category).
    clip_owner: dict[str, str] = {}
    for asset in assets:
        for clip in asset.clips:
            other = clip_owner.get(clip.asset)
            if other and other != asset.name:
                problems.append(Problem("error", asset.name, f"clip {clip.asset} is also listed by {other}"))
            clip_owner[clip.asset] = asset.name

    # ---- skeleton families (sorted asset order: deterministic creator) ----
    families_map: dict[str, list[str]] = {}
    for asset in assets:
        if asset.skeletal:
            families_map.setdefault(asset.skeleton, []).append(asset.name)

    # ---- material instances and slot assignment ----
    instances: dict[str, InstancePlan] = {}
    slot_materials: dict[str, dict[int, str]] = {}

    def add_instance(plan: InstancePlan, subject: str) -> str:
        existing = instances.get(plan.name)
        if existing is None:
            plan.used_by.append(subject)
            instances[plan.name] = plan
        elif existing.signature() != plan.signature():
            problems.append(Problem("error", subject, f"material instance {plan.name} is defined twice with different "
                                    f"parents / parameters ({existing.used_by[0]} vs {subject})"))
        elif subject not in existing.used_by:
            existing.used_by.append(subject)
        return plan.name

    def palette_textures(family: str) -> dict[str, str]:
        pal = palettes.get(family)
        if pal is None:
            return {}
        return {"PaletteBC": pal.bc_name, "PaletteP": pal.p_name}

    for family, pal in palettes.items():
        parent = pal.parent if pal.parent in paths.TOON_MASTERS else paths.M_TOON
        if parent != pal.parent:
            problems.append(Problem("warning", f"palette {family}", f"unknown parent {pal.parent!r}: using {parent}"))
        add_instance(InstancePlan(pal.material, parent, family, textures=palette_textures(family)), f"palette {family}")

    for asset in assets:
        mapping: dict[int, str] = {}
        foliage = asset.toon_parent == paths.M_TOON_FOLIAGE
        for slot in asset.slots:
            subject = f"{asset.name} slot {slot.index}"
            extra_s: dict[str, float] = {}
            extra_v: dict[str, tuple[float, float, float, float]] = {}
            for pname, pvalue in sorted(slot.params.items()):
                parsed = _param_value(pname, pvalue, subject, problems)
                if parsed is None:
                    continue
                if parsed[0] == "scalar":
                    extra_s[pname] = parsed[1]  # type: ignore[assignment]
                else:
                    extra_v[pname] = parsed[1]  # type: ignore[assignment]
            if slot.is_outline:
                cls = slot.outline_class or "decor"
                px = slot.px1080 if slot.px1080 is not None else shading.outline_px.get(cls)
                baked = slot.width_cm if slot.width_cm is not None else shading.baked_width_cm.get(cls)
                if px is None or baked is None:
                    problems.append(Problem("warning", subject, f"outline class {cls!r} has no width: using decor"))
                    px = px if px is not None else shading.outline_px.get("decor", 1.2)
                    baked = baked if baked is not None else shading.baked_width_cm.get("decor", 1.2)
                parent = paths.M_OUTLINE_FOLIAGE if foliage else paths.M_OUTLINE
                scalars = {"OutlinePx1080": float(px), "BakedWidthCm": float(baked)}
                scalars.update(extra_s)
                class_px, class_baked = shading.outline_px.get(cls), shading.baked_width_cm.get(cls)
                name = outline_instance_name(
                    slot.palette or "Default", cls, foliage,
                    px=float(px) if class_px is None or abs(float(px) - class_px) > 1e-6 else None,
                    baked=float(baked) if class_baked is None or abs(float(baked) - class_baked) > 1e-6 else None)
                mapping[slot.index] = add_instance(
                    InstancePlan(name, parent, slot.palette, scalars=scalars, vectors=extra_v,
                                 textures=palette_textures(slot.palette)), subject)
                continue
            # toon-family slot
            if slot.name.startswith("M_") and not slot.name.startswith("MI_"):
                # a master used directly (e.g. M_AF_Ghost on an FX prop)
                mapping[slot.index] = slot.name
                if extra_s or extra_v:
                    problems.append(Problem("warning", subject, "params on a master slot are ignored"))
                continue
            parent = slot.parent
            known_parents = paths.TOON_MASTERS + (paths.M_FX_MESH, paths.M_GHOST)
            if parent not in known_parents:
                problems.append(Problem("warning", subject, f"unknown parent {parent!r}: using {paths.M_TOON}"))
                parent = paths.M_TOON
            name = slot.name if slot.name.startswith("MI_") else f"MI_AF_Toon_{slot.palette or asset.name}"
            pal = palettes.get(slot.palette)
            if pal is not None and name == pal.material and parent == (pal.parent if pal.parent in paths.TOON_MASTERS
                                                                       else paths.M_TOON) and not extra_s and not extra_v:
                mapping[slot.index] = add_instance(
                    InstancePlan(name, parent, slot.palette, textures=palette_textures(slot.palette)), subject)
            else:
                mapping[slot.index] = add_instance(
                    InstancePlan(name, parent, slot.palette, scalars=extra_s, vectors=extra_v,
                                 textures=palette_textures(slot.palette)), subject)
        slot_materials[asset.name] = mapping

    # ---- textures ----
    telegraph = telegraph_contract(data, problems)
    ash = ash_contract(data, problems)
    # the hero's Ashnumb band (the art lane's VFX contract, next to the manifest's Export/VFX)
    ashnumb = ashnumb_contract(load_vfx_recipes(export_dir / "VFX" / "vfx_recipes.json"), problems)
    textures = plan_textures(export_dir, palettes, data, problems, scan=scan_textures)
    # ---- painted ground: MI_AF_Terrain_<Theme> per manifest terrain section (needs the planned textures) ----
    for terrain_instance, subject in plan_terrain_instances(data, textures, problems):
        add_instance(terrain_instance, subject)
    # ---- utility instances and the art-pass look parameters ----
    add_instance(select_rim_instance(), "selection rim")
    plan_look_parameters(instances)

    return Plan(manifest_path=manifest_path, export_dir=export_dir, shading=shading, palettes=palettes, assets=assets,
                skeleton_families=families_map, textures=textures, instances=instances, slot_materials=slot_materials,
                problems=problems, skipped=skipped, telegraph=telegraph, ash=ash, ashnumb=ashnumb)


def classify_texture(name: str) -> tuple[str, str, bool]:
    """Texture asset name -> (kind, content folder, sRGB) by the naming convention (art-inventory-ch1.md 2.2)."""
    if name.startswith("T_AF_Palette_"):
        if name.endswith("_P"):
            return "palette_p", paths.TEXTURES_DIR, False
        return "palette_bc", paths.TEXTURES_DIR, True
    if name.startswith("T_UI_Portrait_") or name.startswith("T_UI_Emblem_"):
        return "ui_portrait", paths.UI_PORTRAITS_DIR, True
    if name.startswith("T_UI_"):
        return "ui_icon", paths.UI_ICONS_DIR, True
    if name.startswith("T_FX_"):
        return "fx", paths.FX_TEXTURES_DIR, False
    if name.endswith("_N"):
        return "normal", paths.TEXTURES_DIR, False
    if name.endswith(("_M", "_P", "_E")):
        return "mask", paths.TEXTURES_DIR, False
    if name.endswith("_Details_BC"):
        # straight-alpha motif atlas (T_Terrain_<Theme>_Details_BC): small coloured motifs on transparent texels
        return "atlas", paths.TEXTURES_DIR, True
    return "world", paths.TEXTURES_DIR, True


# Directories under Art/Export scanned for PNGs (in this order; first occurrence of a name wins).
TEXTURE_SCAN_DIRS = ("Textures", "Portraits", "Icons", "UI", "VFX", "Terrain", "Props", "Foliage", "Pickups")
# Files that are packaging inputs or test artefacts, not game textures (Docs/CI.md 8: the installer icon source).
# T_AF_Palette_Test_* belong to the art kit smoke test (Art/blender/tests), not to the game.
TEXTURE_EXCLUDE_PREFIXES = ("T_UI_AppIcon", "T_AF_Palette_Test_")


def plan_textures(export_dir: Path, palettes: dict[str, Palette], data: dict[str, Any], problems: list[Problem],
                  scan: bool = True) -> list[TextureEntry]:
    entries: dict[str, TextureEntry] = {}

    def add(path: Path, subject: str, required: bool) -> None:
        name = path.stem
        if not path.is_file():
            if required:
                problems.append(Problem("error", subject, f"texture missing: {path}"))
            return
        if not _NAME_RE.match(name):
            problems.append(Problem("warning", subject, f"{path.name}: not a valid UE asset name (skipped)"))
            return
        if name.startswith(TEXTURE_EXCLUDE_PREFIXES):
            return
        existing = entries.get(name)
        if existing is not None:
            if existing.source.resolve() != path.resolve():
                problems.append(Problem("warning", subject, f"{name} exists twice ({existing.source} and {path}); "
                                        "keeping the first"))
            return
        kind, folder, srgb = classify_texture(name)
        entries[name] = TextureEntry(name=name, source=path, folder=folder, kind=kind, srgb=srgb)

    for family, pal in sorted(palettes.items()):
        add(pal.bc_file, f"palette {family}", True)
        add(pal.p_file, f"palette {family}", True)
    for name, raw in sorted((data.get("assets") or {}).items()):
        portrait = raw.get("portrait")
        if portrait:
            add(export_dir / str(portrait), name, True)
        for key in ("icon", "icons"):
            value = raw.get(key)
            for rel in (value if isinstance(value, list) else [value] if value else []):
                add(export_dir / str(rel), name, True)
    # M_AF_Telegraph's rim strip and fill grain (Art/Export/VFX), planned even without the scan: the master bakes them as
    # its texture defaults; a missing one leaves the decal on the generated glow (warned)
    contract = telegraph_contract(data)
    if contract.present:
        for name in (contract.rim_texture, contract.fill_texture):
            path = export_dir / "VFX" / f"{name}.png"
            if not path.is_file():
                problems.append(Problem("warning", f"materials {paths.M_TELEGRAPH}",
                                        f"{path.name} missing: the decal draws on the default glow texture"))
            add(path, f"materials {paths.M_TELEGRAPH}", False)
    # the ash husk's noise (Art/Export/VFX), planned even without the scan: M_AF_Toon_Ash / M_AF_Outline_Ash bake it as
    # their texture default; a missing one leaves the dissolve on the default glow (warned)
    ash = ash_contract(data)
    if ash.present:
        path = export_dir / "VFX" / f"{ash.noise_texture}.png"
        if not path.is_file():
            problems.append(Problem("warning", "materials M_AF_AshDissolve",
                                    f"{path.name} missing: the ash dissolve samples the default glow texture"))
        add(path, "materials M_AF_AshDissolve", False)
    # the painted ground's layers and detail atlas (plan_terrain_instances binds them; a missing one is reported there)
    for theme, section in sorted((data.get("terrain") or {}).items()):
        if not isinstance(section, dict):
            continue
        for rel in terrain_files(section):
            add(export_dir / rel, f"terrain {theme}", False)
    if scan:
        for sub in TEXTURE_SCAN_DIRS:
            root = export_dir / sub
            if not root.is_dir():
                continue
            for png in sorted(root.rglob("*.png")):
                if png.name.startswith("."):
                    continue
                add(png, f"Art/Export/{sub}", False)
    return [entries[k] for k in sorted(entries)]


# ---------------------------------------------------------------------------------------------------------------------
# Painted ground (manifest `terrain.<theme>`, Art/blender/environment/terrain.py; WorldContract 3.1)
# ---------------------------------------------------------------------------------------------------------------------
def terrain_files(section: dict[str, Any]) -> list[str]:
    """Export-relative files of one theme's painted ground: the layer per paint index and the detail atlas."""
    out: list[str] = []
    layers = section.get("layers")
    for tile in paths.TERRAIN_PAINT_TILES:
        layer = layers.get(str(tile)) if isinstance(layers, dict) else None
        if isinstance(layer, dict) and layer.get("file"):
            out.append(str(layer["file"]))
    details = section.get("details")
    if isinstance(details, dict) and details.get("file"):
        out.append(str(details["file"]))
    return out


def plan_terrain_instances(data: dict[str, Any], textures: list[TextureEntry], problems: list[Problem]
                           ) -> list[tuple[InstancePlan, str]]:
    """MI_AF_Terrain_<Theme> (parent M_AF_Terrain_Painted) for every manifest theme whose five ground layers and detail
    atlas are all in the texture plan. A theme with a texture missing gets no instance: its zones keep the procedural
    M_AF_Terrain (the runtime falls back when the instance does not exist)."""
    have = {t.name for t in textures}
    out: list[tuple[InstancePlan, str]] = []
    for theme, section in sorted((data.get("terrain") or {}).items()):
        if not isinstance(section, dict):
            continue
        subject = f"terrain {theme}"
        name = paths.terrain_instance_name(str(theme))
        if not _NAME_RE.match(name):
            problems.append(Problem("warning", subject, f"{name}: not a valid UE asset name (skipped)"))
            continue
        period = section.get("periodTiles", paths.TERRAIN_PERIOD_TILES)
        if not isinstance(period, (int, float)) or abs(float(period) - paths.TERRAIN_PERIOD_TILES) > 1e-6:
            problems.append(Problem("warning", subject, f"periodTiles {period!r}: {paths.M_TERRAIN_PAINTED} samples "
                                    f"the layers every {paths.TERRAIN_PERIOD_TILES} tiles; {name} not built"))
            continue
        bound: dict[str, str] = {}
        missing: list[str] = []
        layers = section.get("layers")
        for tile in paths.TERRAIN_PAINT_TILES:
            layer = layers.get(str(tile)) if isinstance(layers, dict) else None
            tex = str(layer.get("texture") or "") if isinstance(layer, dict) else ""
            if tex and tex in have:
                bound[paths.terrain_layer_param(tile)] = tex
            else:
                missing.append(tex or f"layer {tile}")
        details = section.get("details")
        tex = str(details.get("texture") or "") if isinstance(details, dict) else ""
        if tex and tex in have:
            bound[paths.TERRAIN_DETAILS_PARAM] = tex
        else:
            missing.append(tex or "details")
        if missing:
            problems.append(Problem("warning", subject, f"painted ground texture(s) missing ({', '.join(missing)}): "
                                    f"{name} not built, the theme keeps the procedural {paths.M_TERRAIN}"))
            continue
        out.append((InstancePlan(name, paths.M_TERRAIN_PAINTED, textures=bound), subject))
    return out


# ---------------------------------------------------------------------------------------------------------------------
# Art-pass look parameters (DECISIONS P11 revised, R16, R19)
# ---------------------------------------------------------------------------------------------------------------------
def _family(inst: InstancePlan) -> str:
    """Palette family token: 'Monsters_Plains' -> 'Monsters' (else from the instance name MI_AF_Toon_<Family>...)."""
    key = inst.palette or inst.name.removeprefix("MI_AF_Toon_")
    return key.split("_", 1)[0]


def _theme(inst: InstancePlan) -> str:
    """Theme token of a themed palette: 'Foliage_Plains' -> 'plains' ('' without one)."""
    parts = (inst.palette or inst.name.removeprefix("MI_AF_Toon_")).split("_")
    return parts[1].lower() if len(parts) > 1 else ""


def cast_shadow_strength(inst: InstancePlan) -> float | None:
    """The instance's CastShadowStrength (None: unknown family, the master default 1 applies)."""
    family = _family(inst)
    if family == "Foliage" and inst.name.endswith(GROUND_COVER_SUFFIX):
        return GROUND_COVER_CAST_SHADOW
    return CAST_SHADOW_BY_FAMILY.get(family)


def plan_look_parameters(instances: dict[str, InstancePlan]) -> None:
    """Adds the art-pass scalars to the planned instances (a value the manifest slot `params` set wins):
    CastShadowStrength on every M_AF_Toon / M_AF_Toon_Foliage instance, BaseSaturation / BaseValue on the graded themes'
    ground cover, GroundSaturation / GroundValue on their painted ground."""
    for inst in instances.values():
        if inst.parent in (paths.M_TOON, paths.M_TOON_FOLIAGE):
            k = cast_shadow_strength(inst)
            if k is not None:
                inst.scalars.setdefault("CastShadowStrength", k)
            grade = GROUND_GRADE_BY_THEME.get(_theme(inst))
            if grade is not None and inst.name.endswith(GROUND_COVER_SUFFIX):
                inst.scalars.setdefault("BaseSaturation", grade[0])
                inst.scalars.setdefault("BaseValue", grade[1])
        elif inst.parent == paths.M_TERRAIN_PAINTED:
            theme = inst.name.removeprefix("MI_AF_Terrain_").lower()
            grade = GROUND_GRADE_BY_THEME.get(theme)
            if grade is not None:
                inst.scalars.setdefault("GroundSaturation", grade[0])
                inst.scalars.setdefault("GroundValue", grade[1])


def select_rim_instance() -> InstancePlan:
    """MI_AF_SelectRim: M_AF_SelectRim with the hero rim (the runtime's dynamic instances start from it). BodyMask stays
    the master's 0 (the runtime sets 1 on the weapon copies' body slot)."""
    d = SELECT_RIM_DEFAULTS
    r, g, b = hex_to_linear(str(d["RimColor"]))
    scalars = {k: float(v) for k, v in d.items() if k not in ("RimColor", "BodyMask")}
    return InstancePlan(paths.MI_SELECT_RIM, paths.M_SELECT_RIM, scalars=scalars, vectors={"RimColor": (r, g, b, 1.0)})

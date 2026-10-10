"""Art manifest (unreal/Art/Export/manifest.json, Art/blender/README.md 6) -> a validated, deterministic import plan.

No `unreal` import: the planner runs in plain CPython (tests, `build_content.py --plan`) and inside the editor.

Contract with the runtime (Source/Abyssfire/Public/World/WorldContract.md 2.1, AbyssArtManifest.h):
* an asset exported as Art/Export/<Dir>/<Name>.fbx is imported to /Game/Abyssfire/<Dir>/<Name>; <Dir> = the directory part
  of the manifest `fbx` field, else the asset `category`; clips use their own `fbx` directory;
* palettes -> /Game/Abyssfire/Textures/<T_AF_Palette_*>; portraits / icons / UI glyphs -> /Game/Abyssfire/UI/{Portraits,Icons};
  FX sprites (T_FX_*) -> /Game/Abyssfire/FX/Textures;
* material slot 0 = the family toon instance (MI_AF_Toon_<Family>), slot 1 = the outline hull (M_AF_Outline, made an
  instance per palette and outline class: MI_AF_Outline_<Family>_<Class>).
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
    level: str      # "error" | "warning"
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


def _folder_of(fbx: str, category: str) -> str:
    rel = fbx.replace("\\", "/").strip("/")
    if "/" in rel:
        return rel.rsplit("/", 1)[0]
    return category or "Misc"


def _pascal(token: str) -> str:
    return "".join(part[:1].upper() + part[1:] for part in re.split(r"[^A-Za-z0-9]+", token) if part)


def outline_instance_name(palette: str, outline_class: str, foliage: bool) -> str:
    base = "MI_AF_OutlineFoliage" if foliage else "MI_AF_Outline"
    return f"{base}_{palette}_{_pascal(outline_class) or 'Default'}"


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
                name = outline_instance_name(slot.palette or "Default", cls, foliage)
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
    textures = plan_textures(export_dir, palettes, data, problems, scan=scan_textures)

    return Plan(manifest_path=manifest_path, export_dir=export_dir, shading=shading, palettes=palettes, assets=assets,
                skeleton_families=families_map, textures=textures, instances=instances, slot_materials=slot_materials,
                problems=problems, skipped=skipped)


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

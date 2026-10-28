"""Repository paths, content folders and the names the runtime looks up (Source/Abyssfire/Public/World/WorldContract.md).

Nothing here imports `unreal`, so the planner and the tests run in a plain CPython 3.11.
"""
from __future__ import annotations

import os
from pathlib import Path

# unreal/Scripts/abyss_content/paths.py -> unreal/
UNREAL_DIR = Path(__file__).resolve().parent.parent.parent
REPO_DIR = UNREAL_DIR.parent
EXPORT_DIR = UNREAL_DIR / "Art" / "Export"
MANIFEST_PATH = EXPORT_DIR / "manifest.json"
# The exporter's copy the game reads first (UAbyssAssetLibrary, the core DataStore): {"manifest": <MANIFEST_PATH>}.
RUNTIME_MANIFEST_COPY = UNREAL_DIR / "Data" / "assets.json"
AUDIO_IMPORT_SCRIPT = UNREAL_DIR / "Audio" / "ue" / "import_audio.py"
CONTENT_DIR = UNREAL_DIR / "Content"            # /Game on disk
# Generated, never committed (unreal/.gitignore ignores Saved/).
WORK_DIR = UNREAL_DIR / "Saved" / "ContentBuild"
GENERATED_DIR = WORK_DIR / "Generated"
DEFAULT_REPORT_JSON = WORK_DIR / "build_report.json"
DEFAULT_REPORT_MD = WORK_DIR / "build_report.md"

# ---------------------------------------------------------------------------------------------------------------------
# Content folders (/Game/Abyssfire/...). Manifest assets go to /Game/Abyssfire/<dir of their fbx> (WorldContract 2.1).
# ---------------------------------------------------------------------------------------------------------------------
CONTENT_ROOT = "/Game/Abyssfire"
MATERIALS_DIR = f"{CONTENT_ROOT}/Materials"              # masters + MPC (UAbyssAssetLibrary::LoadMaterial folder 1)
INSTANCES_DIR = f"{CONTENT_ROOT}/Materials/Instances"    # per palette / outline-class instances (folder 2)
FX_MATERIALS_DIR = f"{CONTENT_ROOT}/Materials/FX"        # VFX / utility masters (folder 3)
TEXTURES_DIR = f"{CONTENT_ROOT}/Textures"                # palettes and other world textures (LoadTexture)
DEFAULT_TEXTURES_DIR = f"{CONTENT_ROOT}/Textures/Defaults"
FX_DIR = f"{CONTENT_ROOT}/FX"                            # SM_FX_Quad (LoadContentMesh("FX", ...))
FX_TEXTURES_DIR = f"{CONTENT_ROOT}/FX/Textures"          # T_FX_<Sprite> (LoadFxTexture folder 1)
UI_ICONS_DIR = f"{CONTENT_ROOT}/UI/Icons"                # UAbyssUiSubsystem texture folders
UI_PORTRAITS_DIR = f"{CONTENT_ROOT}/UI/Portraits"
SKELETONS_DIR = f"{CONTENT_ROOT}/Skeletons"
MAPS_DIR = f"{CONTENT_ROOT}/Maps"
MAIN_MAP = f"{MAPS_DIR}/L_Main"
AUDIO_ROOT = f"{CONTENT_ROOT}/Audio"

MPC_NAME = "MPC_AF_Lighting"

# Master materials (WorldContract 3). Names are looked up by the C++ (UAbyssAssetLibrary::LoadMaterial).
M_TOON = "M_AF_Toon"
M_TOON_FOLIAGE = "M_AF_Toon_Foliage"
M_TOON_SLIME = "M_AF_Toon_Slime"
# Translucent twins of M_AF_Toon / M_AF_Toon_Foliage for the occlusion fade ghosts (runtime only: no manifest slot
# names them; AAbyssZoneActor loads them by name).
M_TOON_FADE = "M_AF_Toon_Fade"
M_TOON_FOLIAGE_FADE = "M_AF_Toon_Foliage_Fade"
M_OUTLINE = "M_AF_Outline"
M_OUTLINE_FOLIAGE = "M_AF_Outline_Foliage"
M_TERRAIN = "M_AF_Terrain"
M_TERRAIN_PAINTED = "M_AF_Terrain_Painted"
M_WATER = "M_AF_Water"
M_FX_ADDITIVE = "M_AF_FX_Additive"
M_FX_TRANSLUCENT = "M_AF_FX_Translucent"
M_FX_MESH = "M_AF_FX_Mesh"
M_GHOST = "M_AF_Ghost"
M_LIGHT_POOL = "M_AF_LightPool"
M_TARGET_RING = "M_AF_TargetRing"
M_AFFIX_AURA = "M_AF_AffixAura"
M_BLOB_SHADOW = "M_AF_BlobShadow"
M_PP_GRADE = "M_AF_PP_Grade"            # still built, no longer bound (DECISIONS P14)
M_PP_AO = "M_AF_PP_AOComposite"         # light SSAO composite, desktop balanced / high (DECISIONS P14)
# Selection rim (DECISIONS R16): the hull-only follower of the hero / target / hovered monster. The runtime loads the
# instance (hero defaults) and makes per-use dynamic instances of it (AAbyssCharacterActor::SetSelectionRim).
M_SELECT_RIM = "M_AF_SelectRim"
MI_SELECT_RIM = "MI_AF_SelectRim"
# The enemy ground telegraph (dark remake U2 / 2G; manifest materials.M_AF_Telegraph): one translucent unlit master on
# SM_FX_Quad, every per-decal value custom primitive data (UAbyssTelegraphSystem, AbyssTelegraphRules.h Cpd).
M_TELEGRAPH = "M_AF_Telegraph"
# The ash husk (dark remake U3 / 2G stage C; manifest materials.M_AF_AshDissolve): runtime-only twins of M_AF_Toon /
# M_AF_Outline a dead body's slots swap to (AAbyssCharacterActor::SetAshLook: dynamic instances that copy the slot's
# parameters, with the scalars Grey / Dissolve / HuskTopCm); no manifest slot names them.
M_TOON_ASH = "M_AF_Toon_Ash"
M_OUTLINE_ASH = "M_AF_Outline_Ash"

TOON_MASTERS = (M_TOON, M_TOON_FOLIAGE, M_TOON_SLIME)
OUTLINE_MASTERS = (M_OUTLINE, M_OUTLINE_FOLIAGE)

# Painted ground (WorldContract 3.1, manifest `terrain.<theme>`): MI_AF_Terrain_<Theme> is M_AF_Terrain_Painted with the
# theme's art layers bound; AAbyssZoneActor::BuildTerrain uses it when it exists, else the procedural M_AF_Terrain.
TERRAIN_PAINT_TILES = (0, 1, 2, 3, 5)     # paint indices with a ground style (terrain_styles.json `ground` keys)
TERRAIN_PERIOD_TILES = 3                  # the layer textures repeat every 3 tiles (web PERIOD; manifest periodTiles)
TERRAIN_DETAILS_PARAM = "Details"         # the per-tile detail atlas (4 x 4 cells)


def terrain_layer_param(tile: int) -> str:
    """Texture parameter of paint index `tile`'s ground layer on M_AF_Terrain_Painted."""
    return f"Tile{tile}Layer"


def terrain_instance_name(theme: str) -> str:
    """'plains' -> 'MI_AF_Terrain_Plains' (the runtime capitalises the zone theme the same way)."""
    return f"MI_AF_Terrain_{theme[:1].upper()}{theme[1:]}"

# Generated utility textures (pngio.default_textures): defaults of the texture parameters, so every master compiles
# before the art exists, and the VFX glow used when T_FX_Glow is missing.
T_DEFAULT_WHITE = "T_AF_Default_White"
T_DEFAULT_PALETTE_P = "T_AF_Default_PaletteP"
T_DEFAULT_TILE_IDS = "T_AF_Default_TileIds"
T_DEFAULT_GLOW = "T_AF_FX_DefaultGlow"

SM_FX_QUAD = "SM_FX_Quad"

# Asset metadata tags written on imported assets (idempotent re-runs).
TAG_SOURCE_SHA = "AbyssSourceSha256"
TAG_BUILD = "AbyssContentBuild"
# Bump when import settings change in a way that requires re-importing unchanged sources.
IMPORT_SETTINGS_VERSION = "abyss-import-3"


def object_path(package_path: str) -> str:
    """'/Game/A/B' -> '/Game/A/B.B'."""
    name = package_path.rsplit("/", 1)[-1]
    return f"{package_path}.{name}"


def content_path(folder: str, name: str) -> str:
    return f"{folder.rstrip('/')}/{name}"


def report_json_path() -> Path:
    """CI exports AF_CONTENT_REPORT (Docs/CI.md 8); otherwise Saved/ContentBuild/build_report.json."""
    env = os.environ.get("AF_CONTENT_REPORT", "").strip()
    return Path(env) if env else DEFAULT_REPORT_JSON

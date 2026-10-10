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
M_OUTLINE = "M_AF_Outline"
M_OUTLINE_FOLIAGE = "M_AF_Outline_Foliage"
M_TERRAIN = "M_AF_Terrain"
M_WATER = "M_AF_Water"
M_FX_ADDITIVE = "M_AF_FX_Additive"
M_FX_TRANSLUCENT = "M_AF_FX_Translucent"
M_FX_MESH = "M_AF_FX_Mesh"
M_GHOST = "M_AF_Ghost"
M_LIGHT_POOL = "M_AF_LightPool"
M_TARGET_RING = "M_AF_TargetRing"
M_AFFIX_AURA = "M_AF_AffixAura"
M_BLOB_SHADOW = "M_AF_BlobShadow"
M_PP_GRADE = "M_AF_PP_Grade"

TOON_MASTERS = (M_TOON, M_TOON_FOLIAGE, M_TOON_SLIME)
OUTLINE_MASTERS = (M_OUTLINE, M_OUTLINE_FOLIAGE)

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

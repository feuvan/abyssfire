"""Repository paths used by the art kit.

Every generator writes inside ``unreal/Art``:

* ``Art/Export/<Category>/<Asset>.fbx`` + ``Art/Export/manifest.json`` + ``Art/Export/Textures/``
* ``Art/Previews/<Asset>/*.png`` (review renders, small PNG)
* ``Art/blender/palettes/<Family>.json`` (persistent swatch registry, committed)

Environment overrides (used by the smoke test so it never touches committed output):
``AF_EXPORT_ROOT``, ``AF_PREVIEW_ROOT``, ``AF_PALETTE_ROOT``.
"""
from __future__ import annotations

import os
from pathlib import Path

KIT_DIR = Path(__file__).resolve().parent
BLENDER_DIR = KIT_DIR.parent            # unreal/Art/blender
ART_ROOT = BLENDER_DIR.parent           # unreal/Art
UNREAL_ROOT = ART_ROOT.parent           # unreal
REPO_ROOT = UNREAL_ROOT.parent


def _env_path(name: str, default: Path) -> Path:
    v = os.environ.get(name)
    return Path(v).resolve() if v else default


def export_root() -> Path:
    return _env_path("AF_EXPORT_ROOT", ART_ROOT / "Export")


def preview_root() -> Path:
    return _env_path("AF_PREVIEW_ROOT", ART_ROOT / "Previews")


def palette_root() -> Path:
    return _env_path("AF_PALETTE_ROOT", BLENDER_DIR / "palettes")


def manifest_path() -> Path:
    return export_root() / "manifest.json"


def texture_dir() -> Path:
    return export_root() / "Textures"


# Export categories (ARCHITECTURE.md §5).
CATEGORIES = (
    "Characters", "Monsters", "NPCs", "Weapons", "Props", "Terrain", "Foliage", "VFX", "Pickups",
)


def category_dir(category: str) -> Path:
    if category not in CATEGORIES:
        raise ValueError(f"unknown export category {category!r}; expected one of {CATEGORIES}")
    return export_root() / category


def preview_dir(asset: str) -> Path:
    return preview_root() / asset


def rel_to_export(p: Path) -> str:
    """Path relative to the export root, with forward slashes (for the manifest)."""
    return Path(p).resolve().relative_to(export_root()).as_posix()

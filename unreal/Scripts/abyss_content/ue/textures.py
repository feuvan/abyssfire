"""Texture imports with per-kind settings (ue58-platform.md 11.3, Art/blender/README.md 2 "Palette atlas").

| kind        | folder              | sRGB | compression              | filter  | mips | group     |
|-------------|---------------------|------|--------------------------|---------|------|-----------|
| palette_bc  | Textures            | yes  | UserInterface2D (RGBA8)  | nearest | none | Pixels2D  |
| palette_p   | Textures            | no   | VectorDisplacement (RGBA8)| nearest | none | Pixels2D  |
| data        | Textures/Defaults   | no   | VectorDisplacement       | nearest | none | Pixels2D  |
| ui_icon     | UI/Icons            | yes  | UserInterface2D          | default | none | UI        |
| ui_portrait | UI/Portraits        | yes  | UserInterface2D          | default | none | UI        |
| fx          | FX/Textures         | no   | Default (BC / ASTC)      | default | yes  | Effects   |
| world       | Textures            | yes  | Default                  | default | yes  | World     |
| normal      | Textures            | no   | Normalmap                | default | yes  | WorldNormalMap |
| mask        | Textures            | no   | Masks                    | default | yes  | World     |

Palettes are exact swatch atlases: uncompressed and unfiltered on every platform (16 px swatches survive no block
compression or bilinear blending), never streamed. The sampler types the masters expect follow from these settings
(sRGB + default compression -> Color, linear -> LinearColor); a palette imported differently would not bind.
"""
from __future__ import annotations

from typing import Any

import unreal

from .. import paths, pngio
from ..manifest import Plan, TextureEntry
from ..report import BuildReport
from .core import BuildError, Editor, enum, fingerprint, set_props


def settings_for(kind: str) -> dict[str, Any]:
    exact = {
        "filter": enum("TextureFilter", "TF_NEAREST"),
        "mip_gen_settings": enum("TextureMipGenSettings", "TMGS_NO_MIPMAPS"),
        "lod_group": enum("TextureGroup", "TEXTUREGROUP_PIXELS2D"),
        "never_stream": True,
        "address_x": enum("TextureAddress", "TA_CLAMP"),
        "address_y": enum("TextureAddress", "TA_CLAMP"),
    }
    if kind == "palette_bc":
        return {"srgb": True, "compression_settings": enum("TextureCompressionSettings", "TC_EDITOR_ICON"), **exact}
    if kind in ("palette_p", "data"):
        return {"srgb": False, "compression_settings": enum("TextureCompressionSettings", "TC_VECTOR_DISPLACEMENTMAP"),
                **exact}
    if kind in ("ui_icon", "ui_portrait"):
        return {
            "srgb": True,
            "compression_settings": enum("TextureCompressionSettings", "TC_EDITOR_ICON"),
            "mip_gen_settings": enum("TextureMipGenSettings", "TMGS_NO_MIPMAPS"),
            "lod_group": enum("TextureGroup", "TEXTUREGROUP_UI"),
            "never_stream": True,
        }
    if kind == "fx":
        return {
            "srgb": False,
            "compression_settings": enum("TextureCompressionSettings", "TC_DEFAULT"),
            "mip_gen_settings": enum("TextureMipGenSettings", "TMGS_FROM_TEXTURE_GROUP"),
            "lod_group": enum("TextureGroup", "TEXTUREGROUP_EFFECTS"),
            "address_x": enum("TextureAddress", "TA_CLAMP"),
            "address_y": enum("TextureAddress", "TA_CLAMP"),
        }
    if kind == "normal":
        return {"srgb": False, "compression_settings": enum("TextureCompressionSettings", "TC_NORMALMAP"),
                "lod_group": enum("TextureGroup", "TEXTUREGROUP_WORLD_NORMAL_MAP")}
    if kind == "mask":
        return {"srgb": False, "compression_settings": enum("TextureCompressionSettings", "TC_MASKS"),
                "lod_group": enum("TextureGroup", "TEXTUREGROUP_WORLD")}
    return {"srgb": True, "compression_settings": enum("TextureCompressionSettings", "TC_DEFAULT"),
            "lod_group": enum("TextureGroup", "TEXTUREGROUP_WORLD")}


def import_texture(ed: Editor, entry: TextureEntry, report: BuildReport, force: bool) -> Any:
    target = entry.content_path
    fp = fingerprint([entry.source], salt=f"texture:{entry.kind}")
    existing = ed.load(target)
    if existing is not None and not isinstance(existing, unreal.Texture2D):
        raise BuildError(f"{target} exists but is a {type(existing).__name__}")
    action = "unchanged"
    tex = existing
    if existing is None or force or ed.get_tag(existing, paths.TAG_SOURCE_SHA) != fp:
        objects = ed.import_file(entry.source, entry.folder)
        tex = next((o for o in objects if isinstance(o, unreal.Texture2D)), None)
        if tex is None:
            raise BuildError(f"{entry.source.name} did not produce a texture")
        if tex.get_name() != entry.name:
            raise BuildError(f"{entry.source.name} imported as {tex.get_name()} (expected {entry.name})")
        ed.set_tag(tex, paths.TAG_SOURCE_SHA, fp)
        action = "imported" if existing is None else "reimported"
    notes: list[str] = []
    changed = set_props(tex, settings_for(entry.kind), entry.name, required=True, notes=notes)
    try:
        size = (int(tex.blueprint_get_size_x()), int(tex.blueprint_get_size_y()))
    except Exception:  # noqa: BLE001
        size = None
    if action != "unchanged" or changed:
        ed.save(tex, force=True)
    if action == "unchanged" and changed:
        action = "updated"
    report.asset(target, "Texture2D", action, kind=entry.kind, size=list(size) if size else None)
    return tex


def write_generated(report: BuildReport) -> list[TextureEntry]:
    """The content build's own utility textures (pngio.default_textures) as import entries."""
    entries = []
    for g in pngio.default_textures(paths.DEFAULT_TEXTURES_DIR, paths.FX_TEXTURES_DIR):
        path = paths.GENERATED_DIR / g.file_name
        if pngio.write_png(path, g.width, g.height, g.rgba):
            report.note(f"generated {path.name}")
        entries.append(TextureEntry(name=g.name, source=path, folder=g.folder, kind=g.kind, srgb=g.srgb))
    return entries


def import_all(ed: Editor, plan: Plan, report: BuildReport, force: bool) -> None:
    entries = write_generated(report) + list(plan.textures)
    with unreal.ScopedSlowTask(len(entries), "Importing Abyssfire textures") as slow:
        slow.make_dialog(True)
        for entry in entries:
            if slow.should_cancel():
                raise BuildError("cancelled")
            slow.enter_progress_frame(1, entry.name)
            try:
                import_texture(ed, entry, report, force)
            except BuildError as e:
                report.error(f"{entry.name}: {e}")
            except Exception as e:  # noqa: BLE001
                report.error(f"{entry.name}: unexpected {type(e).__name__}: {e}")

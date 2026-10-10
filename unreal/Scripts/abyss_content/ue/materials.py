"""MPC_AF_Lighting, the master materials, the manifest's material instances and their assignment to mesh slots.

Contract: Source/Abyssfire/Public/World/WorldContract.md 3 (names, parameters, custom primitive data / per-instance data
layouts) and Art/blender/README.md 2 (the toon / outline look). Platform rules: ue58-platform.md 6.4-6.7, 11.4
(Substrate off: classic root pins; Unlit + custom lighting; every material compiles for SM5 and ES3.1 / Metal mobile).
"""
from __future__ import annotations

import json
import math
from dataclasses import dataclass
from typing import Any, Callable

import unreal

from .. import hlsl, paths
from ..manifest import Asset, InstancePlan, Plan, Shading, hex_to_linear, hex_to_rgb255, srgb_to_linear1
from ..report import BuildReport
from .core import BuildError, Editor, linear_color, set_props
from .graph import C, E, Graph

TAG_MATERIAL = "AbyssMaterialFingerprint"
MPC_PATH = paths.object_path(f"{paths.MATERIALS_DIR}/{paths.MPC_NAME}")
HIGHLIGHT_HEX = "#FFE7A0"     # interactable hover / story focus (warm gold, art-direction quality "legendary" family)


def _tex(name: str) -> str:
    return paths.object_path(f"{paths.DEFAULT_TEXTURES_DIR}/{name}")


@dataclass
class Ctx:
    shading: Shading
    mpc: str
    tex_white: str
    tex_palette_p: str
    tex_tile_ids: str
    tex_glow: str
    highlight: tuple[float, float, float]
    rim_srgb: tuple[float, float, float]
    terrain_defaults: dict[str, tuple[float, float, float]]
    terrain_ranks: dict[int, float]


# ---------------------------------------------------------------------------------------------------------------------
# MPC_AF_Lighting (ue58-platform.md 6.4; WorldContract 3.5; written every lighting interval by UAbyssWorldBuilder)
# ---------------------------------------------------------------------------------------------------------------------
def mpc_parameters(sh: Shading) -> tuple[dict[str, tuple[float, float, float, float]], dict[str, float]]:
    k = sh.key_light
    rim = tuple(x / 255.0 for x in hex_to_rgb255(sh.rim_color))
    fill = 0.55  # abyss.GroundAmbient default x (0.92, 0.95, 1.08) (AbyssZoneActor.cpp)
    vectors = {
        "SunDir": (k[0], k[1], k[2], 0.0),
        "KeyLightDir": (k[0], k[1], k[2], 0.0),
        "SunColor": (1.0, 1.0, 1.0, 1.0),
        "Ambient": (fill * 0.92, fill * 0.95, fill * 1.08, 1.0),
        "RimColor": (rim[0], rim[1], rim[2], 1.0),
    }
    tan_y = math.tan(math.radians(sh.camera_fov_h) / 2.0) * 9.0 / 16.0
    scalars = {"CameraTanHalfFovY": tan_y, "TimeSec": 0.0}
    return vectors, scalars


def ensure_mpc(ed: Editor, sh: Shading, report: BuildReport) -> None:
    mpc, created = ed.create(paths.MPC_NAME, paths.MATERIALS_DIR, unreal.MaterialParameterCollection,
                             unreal.MaterialParameterCollectionFactoryNew())
    vectors, scalars = mpc_parameters(sh)

    def current_vectors() -> dict[str, tuple[float, ...]]:
        out = {}
        for p in mpc.get_editor_property("vector_parameters") or []:
            c = p.get_editor_property("default_value")
            out[str(p.get_editor_property("parameter_name"))] = (c.r, c.g, c.b, c.a)
        return out

    def current_scalars() -> dict[str, float]:
        return {str(p.get_editor_property("parameter_name")): float(p.get_editor_property("default_value"))
                for p in mpc.get_editor_property("scalar_parameters") or []}

    def close(a: dict, b: dict) -> bool:
        if set(a) != set(b):
            return False
        for key in a:
            va = a[key] if isinstance(a[key], tuple) else (a[key],)
            vb = b[key] if isinstance(b[key], tuple) else (b[key],)
            if any(abs(x - y) > 1e-5 for x, y in zip(va, vb)):
                return False
        return True

    changed = False
    if not close(current_vectors(), vectors):
        # Keep existing parameter entries (and their GUIDs) where the name matches: only defaults change.
        existing = {str(p.get_editor_property("parameter_name")): p for p in mpc.get_editor_property("vector_parameters") or []}
        arr = []
        for name, value in vectors.items():
            p = existing.get(name) or unreal.CollectionVectorParameter()
            p.set_editor_property("parameter_name", name)
            p.set_editor_property("default_value", linear_color(value))
            arr.append(p)
        mpc.set_editor_property("vector_parameters", arr)
        changed = True
    if not close(current_scalars(), scalars):
        existing = {str(p.get_editor_property("parameter_name")): p for p in mpc.get_editor_property("scalar_parameters") or []}
        arr = []
        for name, value in scalars.items():
            p = existing.get(name) or unreal.CollectionScalarParameter()
            p.set_editor_property("parameter_name", name)
            p.set_editor_property("default_value", float(value))
            arr.append(p)
        mpc.set_editor_property("scalar_parameters", arr)
        changed = True
    if created or changed:
        ed.save(mpc, force=True)
    report.asset(f"{paths.MATERIALS_DIR}/{paths.MPC_NAME}", "MaterialParameterCollection",
                 "created" if created else ("updated" if changed else "unchanged"))


# ---------------------------------------------------------------------------------------------------------------------
# Master materials
# ---------------------------------------------------------------------------------------------------------------------
def _surface(blend: str, shading_model: str = "MSM_UNLIT", two_sided: bool = False, **extra: Any) -> dict[str, Any]:
    s: dict[str, Any] = {
        "material_domain": E("MaterialDomain", "MD_SURFACE"),
        "shading_model": E("MaterialShadingModel", shading_model),
        "blend_mode": E("BlendMode", blend),
        "two_sided": two_sided,
        "used_with_static_lighting": False,
    }
    s.update(extra)
    return s


TOON_SETTINGS = _surface("BLEND_MASKED", opacity_mask_clip_value=0.5, used_with_skeletal_mesh=True,
                         used_with_instanced_static_meshes=True)
OUTLINE_SETTINGS = dict(TOON_SETTINGS)
TERRAIN_SETTINGS = _surface("BLEND_OPAQUE", shading_model="MSM_DEFAULT_LIT",
                            float_precision_mode=E("MaterialFloatPrecisionMode", "MFPM_FULL"))
WATER_SETTINGS = _surface("BLEND_TRANSLUCENT", float_precision_mode=E("MaterialFloatPrecisionMode", "MFPM_FULL"))
FX_ADD_SETTINGS = _surface("BLEND_ADDITIVE", two_sided=True, used_with_instanced_static_meshes=True)
FX_TRANS_SETTINGS = _surface("BLEND_TRANSLUCENT", two_sided=True, used_with_instanced_static_meshes=True)
FX_MESH_SETTINGS = _surface("BLEND_MASKED", opacity_mask_clip_value=0.5, used_with_instanced_static_meshes=True)
GHOST_SETTINGS = _surface("BLEND_ADDITIVE", used_with_skeletal_mesh=True, used_with_instanced_static_meshes=True)
QUAD_ADD_SETTINGS = _surface("BLEND_ADDITIVE", two_sided=True, used_with_instanced_static_meshes=True)
BLOB_SETTINGS = _surface("BLEND_TRANSLUCENT", two_sided=True)
PP_SETTINGS = {
    "material_domain": E("MaterialDomain", "MD_POST_PROCESS"),
    "blendable_location": E("BlendableLocation", "BL_SCENE_COLOR_AFTER_TONEMAPPING"),
}


def _feedback(g: Graph) -> dict[str, Any]:
    """Custom primitive data parameters (WorldContract 3.3, AbyssCpd)."""
    grp = "Feedback (custom primitive data)"
    return {
        "HitFlash": g.scalar("HitFlash", 0.0, cpd=0, group=grp),
        "PainTint": g.vector("PainTint", (1.0, 1.0, 1.0, 0.0), cpd=1, group=grp),
        "Telegraph": g.vector("Telegraph", (1.0, 1.0, 1.0, 0.0), cpd=5, group=grp),
        "StatusTint": g.vector("StatusTint", (1.0, 1.0, 1.0, 0.0), cpd=9, group=grp),
        "Fade": g.scalar("Fade", 0.0, cpd=13, group=grp),
        "Ghost": g.scalar("Ghost", 0.0, cpd=14, group=grp),
        "Highlight": g.scalar("Highlight", 0.0, cpd=15, group=grp),
    }


def _instance_fade_highlight(g: Graph) -> Any:
    """ISM per-instance data (WorldContract 3.4: 0 Fade, 2 Highlight) handed to the pixel shader."""
    return g.interpolate(g.append(g.per_instance(0, 0.0), g.per_instance(2, 0.0)))


def _wind(g: Graph, ctx: Ctx) -> Any:
    return g.custom(hlsl.wind_node(), {
        "LP": g.node("MaterialExpressionPreSkinnedPosition"),
        "Rand": g.per_instance(1, 0.5),
        "Time": g.mpc(ctx.mpc, "TimeSec"),
        "WindDir": g.vector("WindDirection", (1.0, 0.35, 0.0, 0.0), group="Wind"),
        "Amp": g.scalar("WindAmplitudeCm", 4.0, group="Wind"),
        "Height": g.scalar("WindHeightCm", 300.0, group="Wind"),
        "Speed": g.scalar("WindSpeed", 1.3, group="Wind"),
    })


def build_toon(g: Graph, ctx: Ctx, variant: str) -> None:
    sh = ctx.shading
    slime = variant == "slime"
    bc = g.texture("PaletteBC", ctx.tex_white, "SAMPLERTYPE_COLOR")
    pp = g.texture("PaletteP", ctx.tex_palette_p, "SAMPLERTYPE_LINEAR_COLOR")
    fb = _feedback(g)
    inst = _instance_fade_highlight(g)
    vc = g.vertex_color()
    # Slime (DECISIONS R12): masked dithered toon with the web's jelly rim rgba(230,255,220,.6) (Slime.ts:234).
    rim_rgb = (230.0 / 255.0, 1.0, 220.0 / 255.0) if slime else ctx.rim_srgb
    toon = g.custom(hlsl.toon_node(sh), {
        "Base": bc["RGB"], "P": pp["RGBA"],
        "N": g.node("MaterialExpressionVertexNormalWS"),
        "V": g.node("MaterialExpressionCameraVectorWS"),
        "L": g.mpc(ctx.mpc, "KeyLightDir"),
        "SunCol": g.mpc(ctx.mpc, "SunColor"),
        "RimCol": g.mpc(ctx.mpc, "RimColor"),
        "Ground": vc["R"],
        "GroundEnable": g.scalar("GroundingEnable", 1.0, group="Look"),
        "SunTint": g.scalar("SunTint", 1.0, group="Look"),
        "RimOverride": g.scalar("RimOverride", 1.0 if slime else 0.0, group="Look"),
        "RimOverrideColor": g.vector("RimOverrideColor", (*rim_rgb, 1.0), group="Look"),
        "RimOverrideAlpha": g.scalar("RimOverrideAlpha", 0.6 if slime else sh.rim_alpha, group="Look"),
        "HitFlash": fb["HitFlash"],
        "PainTint": fb["PainTint"], "PainAmt": fb["PainTint"]["A"],
        "Telegraph": fb["Telegraph"], "TelegraphAmt": fb["Telegraph"]["A"],
        "StatusTint": fb["StatusTint"], "StatusAmt": fb["StatusTint"]["A"],
        "Ghost": fb["Ghost"],
        "Highlight": fb["Highlight"],
        "Inst": inst,
        "HighlightColor": g.vector("HighlightColor", (*ctx.highlight, 1.0), group="Look"),
    })
    mask = g.custom(hlsl.dither_node(), {
        "Fade": fb["Fade"], "Inst": inst,
        "BodyOpacity": g.scalar("BodyOpacity", 0.9 if slime else 1.0, group="Look"),
    })
    g.output(toon, "MP_EMISSIVE_COLOR")
    g.output(mask, "MP_OPACITY_MASK")
    if variant == "foliage":
        g.output(_wind(g, ctx), "MP_WORLD_POSITION_OFFSET")


def build_outline(g: Graph, ctx: Ctx, foliage: bool) -> None:
    sh = ctx.shading
    bc = g.texture("PaletteBC", ctx.tex_white, "SAMPLERTYPE_COLOR")
    pp = g.texture("PaletteP", ctx.tex_palette_p, "SAMPLERTYPE_LINEAR_COLOR")
    grp = "Feedback (custom primitive data)"
    hit = g.scalar("HitFlash", 0.0, cpd=0, group=grp)
    fade = g.scalar("Fade", 0.0, cpd=13, group=grp)
    highlight = g.scalar("Highlight", 0.0, cpd=15, group=grp)
    inst = _instance_fade_highlight(g)
    ink = g.custom(hlsl.ink_node(sh), {
        "Base": bc["RGB"], "P": pp["RGBA"], "Highlight": highlight, "Inst": inst, "HitFlash": hit,
        "HighlightColor": g.vector("HighlightColor", (*ctx.highlight, 1.0), group="Look"),
    })
    mask = g.custom(hlsl.dither_node(), {"Fade": fade, "Inst": inst,
                                         "BodyOpacity": g.scalar("BodyOpacity", 1.0, group="Look")})
    pos = g.node("MaterialExpressionWorldPosition",
                 world_position_shader_offset=E("WorldPositionIncludedOffsets", "WPT_CAMERA_RELATIVE_NO_OFFSETS"))
    forward = g.node("MaterialExpressionTransform",
                     transform_source_type=E("MaterialVectorCoordTransformSource", "TRANSFORMSOURCE_VIEW"),
                     transform_type=E("MaterialVectorCoordTransform", "TRANSFORM_WORLD"))
    g.connect(g.const3((0.0, 0.0, 1.0)), forward, "")
    tan = g.node("MaterialExpressionViewProperty",
                 property_=E("MaterialExposedViewProperty", "MEVP_TAN_HALF_FIELD_OF_VIEW"))
    wpo = g.custom(hlsl.outline_wpo_node(sh), {
        "Pos": pos, "N": g.node("MaterialExpressionVertexNormalWS"), "F": forward, "T": tan,
        "Px": g.scalar("OutlinePx1080", 3.0, group="Outline"),
        "Baked": g.scalar("BakedWidthCm", 1.5, group="Outline"),
        "Enable": g.scalar("OutlineScreenSpace", 1.0, group="Outline"),
    })
    if foliage:
        wind = _wind(g, ctx)
        total = g.plus(wpo, wind)
        low = wind
    else:
        total = wpo
        low = g.const3((0.0, 0.0, 0.0))
    g.output(ink, "MP_EMISSIVE_COLOR")
    g.output(mask, "MP_OPACITY_MASK")
    # Mobile low (material quality Low): the baked hull alone (README 2 "Mobile low (no WPO)").
    g.output(g.quality(total, low), "MP_WORLD_POSITION_OFFSET")


def build_terrain(g: Graph, ctx: Ctx) -> None:
    vc = g.vertex_color()
    wires: dict[str, Any] = {
        "UV": g.texcoord(0), "Wall": vc["G"], "Skirt": vc["B"],
        "TileIds": g.texture_object("TileIds", ctx.tex_tile_ids, "SAMPLERTYPE_LINEAR_COLOR"),
        "MapCols": g.scalar("MapCols", 120.0, group="Map"), "MapRows": g.scalar("MapRows", 120.0, group="Map"),
    }
    for t in hlsl.TERRAIN_TILES:
        for f in hlsl.TERRAIN_FIELDS:
            name = f"Tile{t}{f}"
            wires[name] = g.vector(name, (*ctx.terrain_defaults[name], 1.0), group=f"Tile {t}")
        wires[f"Tile{t}Rank"] = g.scalar(f"Tile{t}Rank", ctx.terrain_ranks.get(t, 0.0), group=f"Tile {t}")
    for name in ("WaterShallow", "WaterFoam", "WaterBank"):
        wires[name] = g.vector(name, (*ctx.terrain_defaults[name], 1.0), group="Water")
    detailed = g.custom(hlsl.terrain_node(True), wires)
    simple = g.custom(hlsl.terrain_node(False), wires)
    albedo = g.quality(detailed, simple)
    ambient = g.custom(hlsl.terrain_emissive_node(), {"Albedo": albedo, "Amb": g.mpc(ctx.mpc, "Ambient")})
    g.output(albedo, "MP_BASE_COLOR")
    g.output(ambient, "MP_EMISSIVE_COLOR")
    g.output(g.const(0.0), "MP_SPECULAR")
    g.output(g.const(1.0), "MP_ROUGHNESS")


def build_water(g: Graph, ctx: Ctx) -> None:
    nodes = hlsl.water_nodes()
    vc = g.vertex_color()
    uv = g.texcoord(0)
    time = g.mpc(ctx.mpc, "TimeSec")
    color = g.custom(nodes["color"], {
        "Shore": vc["R"], "Depth": vc["G"], "UV": uv, "Time": time,
        "WaterBase": g.vector("WaterBase", (*ctx.terrain_defaults["WaterBase"], 1.0), group="Water"),
        "WaterShallow": g.vector("WaterShallow", (*ctx.terrain_defaults["WaterShallow"], 1.0), group="Water"),
        "WaterFoam": g.vector("WaterFoam", (*ctx.terrain_defaults["WaterFoam"], 1.0), group="Water"),
        "WaterWave": g.vector("WaterWave", (*ctx.terrain_defaults["WaterWave"], 1.0), group="Water"),
        "SunCol": g.mpc(ctx.mpc, "SunColor"), "Amb": g.mpc(ctx.mpc, "Ambient"), "L": g.mpc(ctx.mpc, "KeyLightDir"),
        "SunScale": g.scalar("SunScale", 0.55, group="Water"),
    })
    opacity = g.custom(nodes["opacity"], {"Shore": vc["R"], "Depth": vc["G"], "UV": uv, "Time": time})
    g.output(color, "MP_EMISSIVE_COLOR")
    g.output(opacity, "MP_OPACITY")


def _fx_instance_color(g: Graph) -> Any:
    """VFX instance data 0..3 (RGBA, WorldContract 3.7 / AbyssVfxIcd); white opaque outside instancing."""
    rgba = g.append(g.append(g.append(g.per_instance(0, 1.0), g.per_instance(1, 1.0)), g.per_instance(2, 1.0)),
                    g.per_instance(3, 1.0))
    return g.interpolate(rgba)


def build_fx_sprite(g: Graph, ctx: Ctx, additive: bool) -> None:
    nodes = hlsl.fx_nodes()
    col = _fx_instance_color(g)
    var = g.interpolate(g.append(g.per_instance(4, 0.0), g.per_instance(5, 0.0)))
    uv = g.custom(nodes["uv"], {"UV": g.texcoord(0), "Var": var, "Cells": g.scalar("Cells", 1.0, group="Sprite")})
    tex = g.texture("Sprite", ctx.tex_glow, "SAMPLERTYPE_LINEAR_COLOR", uv=uv)
    # Soft intersection with the ground / bodies (desktop and higher mobile tiers only).
    soft = g.quality(g.node("MaterialExpressionDepthFade", fade_distance_default=8.0, opacity_default=1.0),
                     g.const(1.0))
    if additive:
        g.output(g.custom(nodes["add"], {"Tex": tex["RGBA"], "Col": col, "Soft": soft}), "MP_EMISSIVE_COLOR")
    else:
        g.output(g.custom(nodes["trans_color"], {"Tex": tex["RGBA"], "Col": col}), "MP_EMISSIVE_COLOR")
        g.output(g.custom(nodes["trans_alpha"], {"Tex": tex["RGBA"], "Col": col, "Soft": soft}), "MP_OPACITY")


def build_fx_mesh(g: Graph, ctx: Ctx) -> None:
    col = _fx_instance_color(g)
    vc = g.vertex_color()
    color = g.custom(hlsl.fx_mesh_node(ctx.shading), {
        "Col": col, "N": g.node("MaterialExpressionVertexNormalWS"), "L": g.mpc(ctx.mpc, "KeyLightDir"),
        "Body": vc["A"]})
    g.output(color, "MP_EMISSIVE_COLOR")
    g.output(g.custom(hlsl.fx_mesh_mask_node(), {"Col": col}), "MP_OPACITY_MASK")


def build_ghost(g: Graph, ctx: Ctx) -> None:
    grp = "Feedback (custom primitive data)"
    vc = g.vertex_color()
    color = g.custom(hlsl.ghost_node(), {
        "Tint": g.vector("Tint", (1.0, 1.0, 1.0, 1.0), cpd=9, group=grp),
        "Ghost": g.scalar("Ghost", 0.0, cpd=14, group=grp),
        "Fade": g.scalar("Fade", 0.0, cpd=13, group=grp),
        "N": g.node("MaterialExpressionVertexNormalWS"),
        "V": g.node("MaterialExpressionCameraVectorWS"),
        "Body": vc["A"],
    })
    g.output(color, "MP_EMISSIVE_COLOR")


def build_quad(g: Graph, ctx: Ctx, kind: str) -> None:
    tint = g.vector("Tint", (1.0, 1.0, 1.0, 1.0), cpd=9, group="Feedback (custom primitive data)")
    color = g.custom(hlsl.quad_nodes()[kind], {"UV": g.texcoord(0), "Tint": tint, "Alpha": tint["A"]})
    g.output(color, "MP_EMISSIVE_COLOR")


def build_blob(g: Graph, ctx: Ctx) -> None:
    fade = g.scalar("Fade", 0.0, cpd=13, group="Feedback (custom primitive data)")
    alpha = g.custom(hlsl.blob_node(), {"UV": g.texcoord(0), "Fade": fade})
    g.output(g.const3(hlsl.BLOB_COLOR_LINEAR), "MP_EMISSIVE_COLOR")
    g.output(alpha, "MP_OPACITY")


def build_grade(g: Graph, ctx: Ctx) -> None:
    scene = g.node("MaterialExpressionSceneTexture", scene_texture_id=E("SceneTextureId", "PPI_POST_PROCESS_INPUT0"))
    screen = g.node("MaterialExpressionScreenPosition")
    color = g.custom(hlsl.grade_node(), {
        "Scene": scene["Color"], "UV": screen["ViewportUV"], "Time": g.mpc(ctx.mpc, "TimeSec"),
        "Ambient": g.vector("Ambient", (1.0, 1.0, 1.0, 1.0), group="Mood"),
        "AmbientAlpha": g.scalar("AmbientAlpha", 0.0, group="Mood"),
        "Haze": g.vector("Haze", (0.0, 0.0, 0.0, 1.0), group="Mood"),
        "HazeAlpha": g.scalar("HazeAlpha", 0.0, group="Mood"),
        "Saturation": g.scalar("Saturation", 1.0, group="Mood"),
        "Contrast": g.scalar("Contrast", 1.0, group="Mood"),
        "Lift": g.vector("Lift", (0.0, 0.0, 0.0, 0.0), group="Mood"),
        "Gain": g.vector("Gain", (0.0, 0.0, 0.0, 0.0), group="Mood"),
    })
    g.output(color, "MP_EMISSIVE_COLOR")


@dataclass(frozen=True)
class Master:
    name: str
    folder: str
    settings: dict[str, Any]
    build: Callable[[Graph, Ctx], None]


MASTERS: tuple[Master, ...] = (
    Master(paths.M_TOON, paths.MATERIALS_DIR, TOON_SETTINGS, lambda g, c: build_toon(g, c, "toon")),
    Master(paths.M_TOON_FOLIAGE, paths.MATERIALS_DIR, TOON_SETTINGS, lambda g, c: build_toon(g, c, "foliage")),
    Master(paths.M_TOON_SLIME, paths.MATERIALS_DIR, TOON_SETTINGS, lambda g, c: build_toon(g, c, "slime")),
    Master(paths.M_OUTLINE, paths.MATERIALS_DIR, OUTLINE_SETTINGS, lambda g, c: build_outline(g, c, False)),
    Master(paths.M_OUTLINE_FOLIAGE, paths.MATERIALS_DIR, OUTLINE_SETTINGS, lambda g, c: build_outline(g, c, True)),
    Master(paths.M_TERRAIN, paths.MATERIALS_DIR, TERRAIN_SETTINGS, build_terrain),
    Master(paths.M_WATER, paths.MATERIALS_DIR, WATER_SETTINGS, build_water),
    Master(paths.M_PP_GRADE, paths.MATERIALS_DIR, PP_SETTINGS, build_grade),
    Master(paths.M_FX_ADDITIVE, paths.FX_MATERIALS_DIR, FX_ADD_SETTINGS, lambda g, c: build_fx_sprite(g, c, True)),
    Master(paths.M_FX_TRANSLUCENT, paths.FX_MATERIALS_DIR, FX_TRANS_SETTINGS, lambda g, c: build_fx_sprite(g, c, False)),
    Master(paths.M_FX_MESH, paths.FX_MATERIALS_DIR, FX_MESH_SETTINGS, build_fx_mesh),
    Master(paths.M_GHOST, paths.FX_MATERIALS_DIR, GHOST_SETTINGS, build_ghost),
    Master(paths.M_LIGHT_POOL, paths.FX_MATERIALS_DIR, QUAD_ADD_SETTINGS, lambda g, c: build_quad(g, c, "light_pool")),
    Master(paths.M_TARGET_RING, paths.FX_MATERIALS_DIR, QUAD_ADD_SETTINGS, lambda g, c: build_quad(g, c, "target_ring")),
    Master(paths.M_AFFIX_AURA, paths.FX_MATERIALS_DIR, QUAD_ADD_SETTINGS, lambda g, c: build_quad(g, c, "affix_aura")),
    Master(paths.M_BLOB_SHADOW, paths.FX_MATERIALS_DIR, BLOB_SETTINGS, build_blob),
)


def master_path(name: str) -> str | None:
    for m in MASTERS:
        if m.name == name:
            return f"{m.folder}/{m.name}"
    return None


# ---------------------------------------------------------------------------------------------------------------------
# Context (terrain defaults from Data/terrain_styles.json, plains theme)
# ---------------------------------------------------------------------------------------------------------------------
_PLAINS_FALLBACK = {   # sRGB hex (art-inventory-ch1.md 6.1)
    0: ("#74A247", "#6C9A43", "#7EAB4E", "#9CC65C", "#F6D65A", 4),
    1: ("#C09A60", "#B8925A", "#C8A36A", None, "#E0C890", 1),
    2: ("#A9A291", "#86A052", "#86A052", None, "#86A052", 2),
    3: ("#3F93B8", "#3B8AB0", "#459AC0", None, "#6FAE4A", 0),
    5: ("#B89468", "#B08D62", "#C09C70", None, "#E2CB84", 2),
}
_WATER_FALLBACK = {"WaterBase": "#3F93B8", "WaterShallow": "#6CC3CF", "WaterFoam": "#EEF9F2", "WaterBank": "#8C7448",
                   "WaterWave": "#A9E2EE"}


def _int_rgb_linear(value: Any, fallback_hex: str) -> tuple[float, float, float]:
    if isinstance(value, int):
        return tuple(srgb_to_linear1(((value >> s) & 255) / 255.0) for s in (16, 8, 0))  # type: ignore[return-value]
    if isinstance(value, str):
        try:
            return hex_to_linear(value)
        except Exception:  # noqa: BLE001
            pass
    return hex_to_linear(fallback_hex)


def _lip_default(base_lin: tuple[float, float, float]) -> tuple[float, float, float]:
    """C++ fallback when a style has no lip: base mixed 25 % toward #FFF2D6 (AbyssZoneActor::ApplyTerrainStyle)."""
    warm = (1.0, 0.88, 0.67)
    return tuple(b + (w - b) * 0.25 for b, w in zip(base_lin, warm))  # type: ignore[return-value]


def terrain_defaults(report: BuildReport) -> tuple[dict[str, tuple[float, float, float]], dict[int, float]]:
    styles: dict[str, Any] = {}
    path = paths.UNREAL_DIR / "Data" / "terrain_styles.json"
    try:
        with open(path, encoding="utf-8") as f:
            styles = ((json.load(f).get("themes") or {}).get("plains") or {}).get("ground") or {}
    except (OSError, ValueError) as e:
        report.warning(f"terrain defaults: {path} not readable ({e}); using the art-inventory values")
    out: dict[str, tuple[float, float, float]] = {}
    ranks: dict[int, float] = {}
    for t in hlsl.TERRAIN_TILES:
        fb = _PLAINS_FALLBACK[t]
        st = styles.get(str(t)) or {}
        base = _int_rgb_linear(st.get("base"), fb[0])
        layers = st.get("layers") or []
        colors = (layers[0].get("colors") if layers and isinstance(layers[0], dict) else None) or []
        pa = _int_rgb_linear(colors[0] if len(colors) > 0 else None, fb[1])
        pb = _int_rgb_linear(colors[1] if len(colors) > 1 else None, fb[2])
        lip = _int_rgb_linear(st.get("lip"), fb[3]) if (st.get("lip") is not None or fb[3]) else _lip_default(base)
        accents = st.get("accents") or []
        acc = _int_rgb_linear(accents[0] if accents else None, fb[4])
        out.update({f"Tile{t}Base": base, f"Tile{t}PatchA": pa, f"Tile{t}PatchB": pb, f"Tile{t}Lip": lip,
                    f"Tile{t}Accent": acc})
        ranks[t] = float(st.get("rank", fb[5]))
    liquid = (styles.get(str(hlsl.TERRAIN_LIQUID_TILE)) or {}).get("liquid") or {}
    wave = None
    for layer in (styles.get(str(hlsl.TERRAIN_LIQUID_TILE)) or {}).get("layers") or []:
        if isinstance(layer, dict) and layer.get("kind") == "wave":
            wave = layer.get("color")
    out["WaterBase"] = out[f"Tile{hlsl.TERRAIN_LIQUID_TILE}Base"]
    out["WaterShallow"] = _int_rgb_linear(liquid.get("shallow"), _WATER_FALLBACK["WaterShallow"])
    out["WaterFoam"] = _int_rgb_linear(liquid.get("foam"), _WATER_FALLBACK["WaterFoam"])
    out["WaterBank"] = _int_rgb_linear(liquid.get("bank"), _WATER_FALLBACK["WaterBank"])
    out["WaterWave"] = _int_rgb_linear(wave, _WATER_FALLBACK["WaterWave"])
    return out, ranks


def make_context(plan: Plan, report: BuildReport) -> Ctx:
    sh = plan.shading
    defaults, ranks = terrain_defaults(report)
    return Ctx(
        shading=sh,
        mpc=MPC_PATH,
        tex_white=_tex(paths.T_DEFAULT_WHITE),
        tex_palette_p=_tex(paths.T_DEFAULT_PALETTE_P),
        tex_tile_ids=_tex(paths.T_DEFAULT_TILE_IDS),
        tex_glow=paths.object_path(f"{paths.FX_TEXTURES_DIR}/{paths.T_DEFAULT_GLOW}"),
        highlight=hex_to_linear(HIGHLIGHT_HEX),
        rim_srgb=tuple(x / 255.0 for x in hex_to_rgb255(sh.rim_color)),  # type: ignore[arg-type]
        terrain_defaults=defaults,
        terrain_ranks=ranks,
    )


# ---------------------------------------------------------------------------------------------------------------------
# Building masters
# ---------------------------------------------------------------------------------------------------------------------
def build_master(ed: Editor, master: Master, ctx: Ctx, report: BuildReport, force: bool) -> unreal.Material:
    dry = Graph()
    master.build(dry, ctx)
    fp = dry.fingerprint(master.settings)
    mat, created = ed.create(master.name, master.folder, unreal.Material, unreal.MaterialFactoryNew())
    path = f"{master.folder}/{master.name}"
    if not created and not force and ed.get_tag(mat, TAG_MATERIAL) == fp:
        report.asset(path, "Material", "unchanged")
        return mat
    ed.mel.delete_all_material_expressions(mat)
    # Classic root pins only (Substrate is off, DECISIONS P11): set the material attributes before building the graph.
    resolved = {k: (v.resolve() if isinstance(v, (E, C)) else v) for k, v in master.settings.items()}
    set_props(mat, resolved, subject=path)
    graph = Graph(ed, mat)
    master.build(graph, ctx)
    ed.mel.layout_material_expressions(mat)
    ed.mel.recompile_material(mat)
    ed.set_tag(mat, TAG_MATERIAL, fp)
    ed.save(mat, force=True)
    report.asset(path, "Material", "created" if created else "rebuilt", expressions=len(graph.trace))
    return mat


def build_masters(ed: Editor, plan: Plan, report: BuildReport, force: bool) -> dict[str, unreal.Material]:
    ctx = make_context(plan, report)
    built: dict[str, unreal.Material] = {}
    for master in MASTERS:
        try:
            built[master.name] = build_master(ed, master, ctx, report, force)
        except BuildError as e:
            report.error(f"{master.name}: {e}")
        except Exception as e:  # noqa: BLE001 - keep building the others; the report fails the run
            report.error(f"{master.name}: unexpected {type(e).__name__}: {e}")
    return built


def check_material_stats(ed: Editor, report: BuildReport) -> None:
    """Instruction counts of every master (0 pixel instructions = the shader did not compile: see the log)."""
    for master in MASTERS:
        path = f"{master.folder}/{master.name}"
        mat = ed.load(path)
        if mat is None:
            continue
        try:
            st = ed.mel.get_statistics(mat)
            vs = int(st.get_editor_property("num_vertex_shader_instructions"))
            ps = int(st.get_editor_property("num_pixel_shader_instructions"))
            samplers = int(st.get_editor_property("num_samplers"))
        except Exception as e:  # noqa: BLE001
            report.materials[master.name] = {"status": f"stats unavailable ({e})"}
            continue
        status = "ok" if ps > 0 else "NO SHADER (compile error? open it in the editor)"
        report.materials[master.name] = {"status": status, "vs": vs, "ps": ps, "samplers": samplers}
        if ps <= 0:
            report.warning(f"{master.name}: no compiled pixel shader for the editor platform; check the log for "
                           "'Failed to compile Material' and the Custom node HLSL")


# ---------------------------------------------------------------------------------------------------------------------
# Instances
# ---------------------------------------------------------------------------------------------------------------------
def material_object_path(name: str) -> str:
    """Instance or master name -> object path (instances folder first)."""
    mp = master_path(name)
    if mp is not None:
        return paths.object_path(mp)
    return paths.object_path(f"{paths.INSTANCES_DIR}/{name}")


def apply_instance(ed: Editor, inst: InstancePlan, report: BuildReport) -> None:
    parent_path = master_path(inst.parent)
    if parent_path is None:
        raise BuildError(f"{inst.name}: unknown parent {inst.parent}")
    parent = ed.load(parent_path)
    if parent is None:
        raise BuildError(f"{inst.name}: parent {parent_path} missing (master build failed?)")
    mi, created = ed.create(inst.name, paths.INSTANCES_DIR, unreal.MaterialInstanceConstant,
                            unreal.MaterialInstanceConstantFactoryNew())
    mel = ed.mel
    changed = created
    if mi.get_editor_property("parent") != parent:
        mel.set_material_instance_parent(mi, parent)
        changed = True
    for param, tex_name in sorted(inst.textures.items()):
        tex = ed.load(f"{paths.TEXTURES_DIR}/{tex_name}")
        if tex is None:
            raise BuildError(f"{inst.name}: texture {tex_name} missing (texture import failed?)")
        if mel.get_material_instance_texture_parameter_value(mi, param) != tex:
            if not mel.set_material_instance_texture_parameter_value(mi, param, tex):
                raise BuildError(f"{inst.name}: parent has no texture parameter {param}")
            changed = True
    for param, value in sorted(inst.scalars.items()):
        if abs(float(mel.get_material_instance_scalar_parameter_value(mi, param)) - float(value)) > 1e-6:
            if not mel.set_material_instance_scalar_parameter_value(mi, param, float(value)):
                raise BuildError(f"{inst.name}: parent has no scalar parameter {param}")
            changed = True
    for param, value in sorted(inst.vectors.items()):
        cur = mel.get_material_instance_vector_parameter_value(mi, param)
        if any(abs(a - b) > 1e-6 for a, b in zip((cur.r, cur.g, cur.b, cur.a), value)):
            if not mel.set_material_instance_vector_parameter_value(mi, param, linear_color(value)):
                raise BuildError(f"{inst.name}: parent has no vector parameter {param}")
            changed = True
    if changed:
        mel.update_material_instance(mi)
        ed.save(mi, force=True)
    report.asset(f"{paths.INSTANCES_DIR}/{inst.name}", "MaterialInstanceConstant",
                 "created" if created else ("updated" if changed else "unchanged"), parent=inst.parent)


def build_instances(ed: Editor, plan: Plan, report: BuildReport) -> None:
    for name in sorted(plan.instances):
        try:
            apply_instance(ed, plan.instances[name], report)
        except BuildError as e:
            report.error(str(e))


# ---------------------------------------------------------------------------------------------------------------------
# Slot assignment (+ the outline hull never casts a shadow: its WPO is screen-space, wrong from the light's view)
# ---------------------------------------------------------------------------------------------------------------------
def _imported_slot_names(asset: Asset, mesh: Any) -> list[str]:
    if asset.skeletal:
        return [str(e.get_editor_property("material_slot_name")) for e in mesh.get_editor_property("materials") or []]
    return [str(e.get_editor_property("material_slot_name")) for e in mesh.get_editor_property("static_materials") or []]


def slot_index_map(asset: Asset, imported: list[str], report: BuildReport) -> dict[int, int]:
    """Manifest slot index -> imported slot index. Slots are matched by name (the FBX material names are the manifest
    slot names); the manifest index is used only when a name is missing or ambiguous (the order Interchange gives the
    slots is not guaranteed)."""
    result: dict[int, int] = {}
    for slot in asset.slots:
        if slot.name and imported.count(slot.name) == 1:
            result[slot.index] = imported.index(slot.name)
            continue
        if slot.index < len(imported):
            result[slot.index] = slot.index
            report.warning(f"{asset.name}: slot {slot.name!r} not found by name among {imported}; using index "
                           f"{slot.index}")
        else:
            report.error(f"{asset.name}: the imported mesh has {len(imported)} material slot(s), manifest slot "
                         f"{slot.index} ({slot.name}) missing")
    return result


def assign_slots(ed: Editor, asset: Asset, mesh: Any, mapping: dict[int, str], report: BuildReport) -> None:
    imported = _imported_slot_names(asset, mesh)
    to_mesh = slot_index_map(asset, imported, report)
    outline_slots = {to_mesh[s.index] for s in asset.slots if s.is_outline and s.index in to_mesh}
    materials: dict[int, Any] = {}
    for idx, mat_name in mapping.items():
        if idx not in to_mesh:
            continue
        mat = unreal.load_asset(material_object_path(mat_name))
        if mat is None:
            report.error(f"{asset.name}: material {mat_name} missing for slot {idx}")
            continue
        materials[to_mesh[idx]] = mat
    changed = False
    if asset.skeletal:
        arr = list(mesh.get_editor_property("materials") or [])
        for idx, mat in sorted(materials.items()):
            entry = arr[idx]
            if entry.get_editor_property("material_interface") != mat:
                entry.set_editor_property("material_interface", mat)
                arr[idx] = entry
                changed = True
        if changed:
            mesh.set_editor_property("materials", arr)
        if ed.skm is not None and outline_slots:
            for lod in range(int(ed.skm.get_lod_count(mesh))):
                for section in range(int(ed.skm.get_num_sections(mesh, lod))):
                    slot = int(ed.skm.get_lod_material_slot(mesh, lod, section))
                    if slot in outline_slots and ed.skm.get_section_cast_shadow(mesh, lod, section):
                        ed.skm.set_section_cast_shadow(mesh, lod, section, False)
                        changed = True
    else:
        for idx, mat in sorted(materials.items()):
            if mesh.get_material(idx) != mat:
                mesh.set_material(idx, mat)
                changed = True
        if ed.smes is not None and outline_slots:
            for lod in range(int(ed.smes.get_lod_count(mesh))):
                for section in range(int(mesh.get_num_sections(lod))):
                    slot = int(ed.smes.get_lod_material_slot(mesh, lod, section))
                    if slot in outline_slots and ed.smes.is_section_cast_shadow_enabled(mesh, lod, section):
                        ed.smes.enable_section_cast_shadow(mesh, False, lod, section)
                        changed = True
    if changed:
        ed.save(mesh, force=True)
    report.count("assigned" if changed else "assignment unchanged")

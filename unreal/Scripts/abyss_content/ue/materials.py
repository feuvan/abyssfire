"""MPC_AF_Lighting, the master materials, the manifest's material instances and their assignment to mesh slots.

Contract: Source/Abyssfire/Public/World/WorldContract.md 3 (names, parameters, custom primitive data / per-instance data
layouts) and Art/blender/README.md 2 (the toon / outline look). Platform rules: ue58-platform.md 6.4-6.7, 11.4
(Substrate off: classic root pins; every material compiles for SM5 and ES3.1 / Metal mobile). Lighting model: DECISIONS
P11 (revised, art pass): M_AF_Toon / M_AF_Toon_Foliage are a "shadow-catcher lit toon" (DefaultLit: the unlit toon
colour split into Emissive + a BaseColor the sun lights with N = KeyLightDir, so it only darkens in cast shadows);
everything else that is a toon (slime, fade ghosts), the ink, the selection rim and the FX stay Unlit.
"""
from __future__ import annotations

import dataclasses
import json
import math
from dataclasses import dataclass
from typing import Any, Callable

import unreal

from .. import hlsl, paths
from ..manifest import (SELECT_RIM_DEFAULTS, AshContract, AshnumbContract, Asset, InstancePlan, Plan, Shading,
                        TelegraphContract, hex_to_linear, hex_to_rgb255, srgb_to_linear1)
from ..report import BuildReport
from .core import BuildError, Editor, linear_color, set_props, shaders_disabled
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
    # M_AF_Telegraph (dark remake U2): the contract's constants and its two textures (the generated glow when the art's
    # textures are not in the plan; a warning at plan time)
    telegraph: TelegraphContract = dataclasses.field(default_factory=TelegraphContract)
    tex_telegraph_rim: str = ""
    tex_telegraph_fill: str = ""
    # M_AF_Toon_Ash / M_AF_Outline_Ash (dark remake U3): the contract's constants and its noise (the generated glow when
    # the art's texture is not in the plan; a warning at plan time)
    ash: AshContract = dataclasses.field(default_factory=AshContract)
    tex_ash_noise: str = ""
    # M_AF_Toon_Ashnumb (dark remake K6 / 2G stage H): the AshnumbTint contract's constants
    ashnumb: AshnumbContract = dataclasses.field(default_factory=AshnumbContract)


# ---------------------------------------------------------------------------------------------------------------------
# MPC_AF_Lighting (ue58-platform.md 6.4; WorldContract 3.5). Writers: UAbyssWorldBuilder every lighting interval (sun,
# key light, ambient, rim, camera, time, SunScale, InkScale); UAbyssVfxSystem the toon light slots Light<i>PosR /
# Light<i>Col (DECISIONS P11 revised). A writer skips a parameter the collection lacks.
# ---------------------------------------------------------------------------------------------------------------------
INK_SCALE_DEFAULT = 1.5     # DECISIONS R3 (revised): OutlinePx1080 values read as px per 720 rows (abyss.InkScale)
SUN_SCALE_DEFAULT = 0.55    # abyss.SunScale: the lit ground's sun strength (the light itself is pi lux, P11 revised)


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
    for i in range(hlsl.TOON_LIGHT_SLOTS):
        vectors[light_slot_pos(i)] = (0.0, 0.0, 0.0, 0.0)     # xyz absolute world cm, w radius cm
        vectors[light_slot_color(i)] = (0.0, 0.0, 0.0, 0.0)   # rgb premultiplied intensity, a > 0 = slot on
    tan_y = math.tan(math.radians(sh.camera_fov_h) / 2.0) * 9.0 / 16.0
    scalars = {"CameraTanHalfFovY": tan_y, "TimeSec": 0.0, "SunScale": SUN_SCALE_DEFAULT, "InkScale": INK_SCALE_DEFAULT}
    return vectors, scalars


def light_slot_pos(i: int) -> str:
    return f"Light{i}PosR"


def light_slot_color(i: int) -> str:
    return f"Light{i}Col"


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


# M_AF_Toon / M_AF_Toon_Foliage (DECISIONS P11 revised): DefaultLit, the normal input in world space (= MPC KeyLightDir);
# masked, dithered fade.
TOON_SETTINGS = _surface("BLEND_MASKED", shading_model="MSM_DEFAULT_LIT", opacity_mask_clip_value=0.5,
                         used_with_skeletal_mesh=True, used_with_instanced_static_meshes=True,
                         tangent_space_normal=False)
# The slime shell (DECISIONS R12, revised after the second Mac playtest): alpha-blended unlit translucency, back faces
# culled. A fixed Bayer screen door only resolves under a temporal AA that varies it, which no tier has (mobile MSAA,
# desktop FXAA / TSR with a per-pixel-fixed pattern); the blend is what the slime previews render (shell mixed with
# the opaque core, bone, eyes and ink hull behind it). Opaque sections (slot 0 toon, slot 1 ink hull) stay in the base
# pass and write depth; the hull still casts the slime's sun shadow. Sorting (translucent sort priority): blob shadows
# (-10) and water (-2) first, the shell at the component's default 0, VFX after it (1 / 2).
SLIME_SETTINGS = _surface("BLEND_TRANSLUCENT", used_with_skeletal_mesh=True, used_with_instanced_static_meshes=True)
# The enemy ground telegraph (dark remake U2, M_AF_Telegraph): unlit alpha-blended translucency on SM_FX_Quad (no depth
# write, no depth fade; the component's sort priority -1 puts it over blob shadows / prop pools / water, under the VFX),
# two-sided like every quad material.
TELEGRAPH_SETTINGS = _surface("BLEND_TRANSLUCENT", two_sided=True)
# Occlusion fade ghosts (DECISIONS R1): on tiers without temporal AA (desktop low FXAA, mobile MSAA) a faded tree / rock /
# wall instance is hidden in its masked ISM and drawn by a translucent copy (AAbyssZoneActor, abyss.FadeGhost) with
# these twins of M_AF_Toon / M_AF_Toon_Foliage: the same toon colour and wind, alpha 1 - PerInstanceCustomData[0],
# back faces culled, no depth write (sort priority 0 on the component, like the slime shell). Instanced dressing only.
FADE_SETTINGS = _surface("BLEND_TRANSLUCENT", used_with_instanced_static_meshes=True)
OUTLINE_SETTINGS = _surface("BLEND_MASKED", opacity_mask_clip_value=0.5, used_with_skeletal_mesh=True,
                            used_with_instanced_static_meshes=True)
# The selection rim (DECISIONS R16): a hull-only follower of a skeletal body (leader pose) and its weapons (static
# meshes need no usage flag); unlit, masked, back faces culled like the ink hull.
SELECT_RIM_SETTINGS = _surface("BLEND_MASKED", opacity_mask_clip_value=0.5, used_with_skeletal_mesh=True)
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
# Light AO (DECISIONS P14): linear scene colour before the tone curve, after depth of field.
PP_AO_SETTINGS = {
    "material_domain": E("MaterialDomain", "MD_POST_PROCESS"),
    "blendable_location": E("BlendableLocation", "BL_SCENE_COLOR_AFTER_DOF"),
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


def _pre_skinned_position_ps(g: Graph) -> Any:
    """The pre-skinned (object-space) position for a pixel-shader input. UE's HLSL translator compiles
    PreSkinnedPosition in the vertex shader only (FHLSLMaterialTranslator::PreSkinnedPosition: "only available in the
    vertex shader, pass through custom interpolators if needed"), so a colour / opacity graph takes it through a
    VertexInterpolator (2G review H-M1). Linear across a triangle, so the per-pixel value is exact. Only WPO graphs
    (_wind) may wire it directly; tests/test_flow.py checks every master for it."""
    return g.interpolate(g.node("MaterialExpressionPreSkinnedPosition"))


ASH_GROUP = "Ash (per body: AAbyssCharacterActor::SetAshLook)"


def _ash_mask(g: Graph, ctx: Ctx) -> Any:
    """AF_AshMask (hlsl.ash_mask_node): keep 0 / 1 and the Edge band, from the per-body scalars Dissolve and HuskTopCm,
    the pre-skinned (object-space) position, the height over the actor's feet and T_FX_AshNoise."""
    pos = g.node("MaterialExpressionWorldPosition",
                 world_position_shader_offset=E("WorldPositionIncludedOffsets", "WPT_CAMERA_RELATIVE"))
    feet = g.node("MaterialExpressionActorPositionWS", origin_type=E("PositionOrigin", "CAMERA_RELATIVE"))
    return g.custom(hlsl.ash_mask_node(), {
        "LP": _pre_skinned_position_ps(g),
        "Pw": pos,
        "Feet": feet,
        "Dissolve": g.scalar("Dissolve", 0.0, group=ASH_GROUP),
        "TopCm": g.scalar("HuskTopCm", 100.0, group=ASH_GROUP),
        "TileCm": g.scalar("NoiseTileCm", ctx.ash.noise_tile_cm, group=ASH_GROUP),
        "EdgeWidth": g.scalar("EdgeWidth", ctx.ash.edge_width, group=ASH_GROUP),
        "Noise": g.texture_object("Noise", ctx.tex_ash_noise or ctx.tex_glow, "SAMPLERTYPE_LINEAR_COLOR"),
    })


ASHNUMB_GROUP = "Ashnumb (the hero: AAbyssCharacterActor::UpdateAshnumbBand)"
# The hero heights the band measures (manifest heightCm: warrior 176, mage 171, rogue 162); the runtime sets each hero's.
ASHNUMB_DEFAULT_HEIGHT_CM = 176.0


def _ashnumb_band(g: Graph, ctx: Ctx, toon: Any, wires: dict[str, Any]) -> Any:
    """AF_AshnumbBand (hlsl.ashnumb_band_node) after the split: per-hero scalars BandTop / BandHeightCm, the contract's
    look as parameter defaults, the split's hit flash / pain / telegraph / status inputs re-applied to the ash."""
    a = ctx.ashnumb
    return g.custom(hlsl.ashnumb_band_node(ctx.shading), {
        "Emit": toon, "Lit": toon["Lit"], "SunCol": wires["SunCol"],
        "LP": _pre_skinned_position_ps(g),
        "N": wires["N"], "L": wires["L"],
        "BandTop": g.scalar("BandTop", 0.0, group=ASHNUMB_GROUP),
        "HeightCm": g.scalar("BandHeightCm", ASHNUMB_DEFAULT_HEIGHT_CM, group=ASHNUMB_GROUP),
        "Softness": g.scalar("BandSoftness", a.band_softness, group=ASHNUMB_GROUP),
        "Strength": g.scalar("BandStrength", a.strength, group=ASHNUMB_GROUP),
        "EdgeWidth": g.scalar("BandEdgeWidth", a.edge_width, group=ASHNUMB_GROUP),
        "AshColor": g.vector("BandAshColor", (*hex_to_linear(a.ash_color), 1.0), group=ASHNUMB_GROUP),
        "AshShade": g.vector("BandAshShade", (*hex_to_linear(a.ash_shade), 1.0), group=ASHNUMB_GROUP),
        "EdgeColor": g.vector("BandEdgeColor", (*hex_to_linear(a.edge_color), 1.0), group=ASHNUMB_GROUP),
        # the split's feedback (the same custom-primitive-data parameters): the band sits under it (2G review H-m1)
        **{name: wires[name] for name in hlsl.ASHNUMB_FEEDBACK_INPUTS},
    })


def build_toon(g: Graph, ctx: Ctx, variant: str, ash: bool = False, band: bool = False) -> None:
    """variant: toon | foliage (masked, dithered fade; shadow-catcher lit, DECISIONS P11 revised) | slime | fade |
    fade_foliage (translucent, blended fade; unlit AF_Toon). ash (variant toon only): M_AF_Toon_Ash, the same graph with
    the ash dissolve after the split (_ash_mask, hlsl.ash_grey_node); band (variant toon only): M_AF_Toon_Ashnumb, the
    same graph with the Ashnumb band after the split (_ashnumb_band); without either the graph is exactly M_AF_Toon's."""
    sh = ctx.shading
    slime = variant == "slime"
    blended = variant in ("slime", "fade", "fade_foliage")
    foliage = variant in ("foliage", "fade_foliage")
    bc = g.texture("PaletteBC", ctx.tex_white, "SAMPLERTYPE_COLOR")
    pp = g.texture("PaletteP", ctx.tex_palette_p, "SAMPLERTYPE_LINEAR_COLOR")
    fb = _feedback(g)
    inst = _instance_fade_highlight(g)
    vc = g.vertex_color()
    # Slime (DECISIONS R12): translucent toon shell with the web's jelly rim rgba(230,255,220,.6) (Slime.ts:234).
    rim_rgb = (230.0 / 255.0, 1.0, 220.0 / 255.0) if slime else ctx.rim_srgb
    wires: dict[str, Any] = {
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
        # DECISIONS R19: ground cover (MI_AF_Toon_Foliage_<Theme>_Ground) compressed like the terrain
        "BaseSaturation": g.scalar("BaseSaturation", 1.0, group="Ground grade"),
        "BaseValue": g.scalar("BaseValue", 1.0, group="Ground grade"),
    }
    if blended:
        toon = g.custom(hlsl.toon_node(sh), wires)
        # SLIME_SETTINGS / FADE_SETTINGS: blended opacity (BodyOpacity faded by Fade / the instance fade) instead of the
        # dithered mask. The fade copy shows the unshadowed toon colour (a cast shadow pops off while it fades: accepted).
        body = g.scalar("BodyOpacity", 0.9 if slime else 1.0, group="Look")
        g.output(toon, "MP_EMISSIVE_COLOR")
        g.output(g.custom(hlsl.fade_opacity_node(), {"Fade": fb["Fade"], "Inst": inst, "BodyOpacity": body}),
                 "MP_OPACITY")
        if foliage:
            g.output(_wind(g, ctx), "MP_WORLD_POSITION_OFFSET")
        return
    # Shadow-catcher lit toon: Emissive = Emit, BaseColor = Lit (the AF_ToonSplit additional output), Normal = the key
    # light, no specular: the sun (pi lux x SunColor along -KeyLightDir) adds Lit x SunColor x its shadow term.
    wires["Pw"] = g.node("MaterialExpressionWorldPosition")     # absolute world position (cm), like the slot positions
    wires["CastShadowStrength"] = g.scalar("CastShadowStrength", 1.0, group="Look")
    for i in range(hlsl.TOON_LIGHT_SLOTS):
        wires[f"L{i}"] = g.mpc(ctx.mpc, light_slot_pos(i))
    for i in range(hlsl.TOON_LIGHT_SLOTS):
        wires[f"C{i}"] = g.mpc(ctx.mpc, light_slot_color(i))
    toon = g.custom(hlsl.toon_split_node(sh), wires)
    body = g.scalar("BodyOpacity", 1.0, group="Look")
    mask = g.custom(hlsl.dither_node(), {"Fade": fb["Fade"], "Inst": inst, "BodyOpacity": body})
    emit: Any = toon
    lit_out: Any = toon["Lit"]
    keep: Any = None
    if ash:
        keep = _ash_mask(g, ctx)
        grey = g.custom(hlsl.ash_grey_node(), {
            "Emit": toon, "Lit": toon["Lit"], "SunCol": wires["SunCol"],
            "Grey": g.scalar("Grey", 0.0, group=ASH_GROUP),
            "AshColor": g.vector("AshColor", (*hex_to_linear(ctx.ash.ash_color), 1.0), group=ASH_GROUP),
            "AshShade": g.vector("AshShade", (*hex_to_linear(ctx.ash.ash_shade), 1.0), group=ASH_GROUP),
            "EdgeColor": g.vector("EdgeColor", (*hex_to_linear(ctx.ash.edge_color), 1.0), group=ASH_GROUP),
            "Edge": keep["Edge"],
        })
        emit, lit_out = grey, grey["AshLit"]
        mask = g.mul(mask, keep)
    if band:
        ashnumb = _ashnumb_band(g, ctx, toon, wires)
        emit, lit_out = ashnumb, ashnumb["BandLit"]
    g.output(emit, "MP_EMISSIVE_COLOR")
    g.output(lit_out, "MP_BASE_COLOR")
    g.output(g.rgb(g.mpc(ctx.mpc, "KeyLightDir")), "MP_NORMAL")
    g.output(g.const(0.0), "MP_SPECULAR")
    g.output(g.const(0.0), "MP_METALLIC")
    g.output(g.const(1.0), "MP_ROUGHNESS")
    # Shadow pass (ShadowPassSwitch): the death / prop fade and BodyOpacity only. The occlusion fade of instanced
    # dressing keeps its full sun shadow, also while the instance is hidden behind its translucent ghost (R1).
    shadow = g.custom(hlsl.dither_shadow_node(), {"Fade": fb["Fade"], "BodyOpacity": body})
    if keep is not None:
        shadow = g.mul(shadow, keep)   # a dropped plate casts no shadow either
    switch = g.node("MaterialExpressionShadowReplace")
    g.connect(mask, switch, "Default")
    g.connect(shadow, switch, "Shadow")
    g.output(switch, "MP_OPACITY_MASK")
    if foliage:
        g.output(_wind(g, ctx), "MP_WORLD_POSITION_OFFSET")


def _view_inputs(g: Graph) -> tuple[Any, Any, Any]:
    """Per-frame view inputs of the screen-space outline (camera-relative position without material offsets, camera
    forward in world space, tan of the half FOV). Kept per frame, not in the MPC: the hull and the rim never cast a
    shadow (assign_slots / the runtime turn their shadow casting off), so the WPO need not match between passes, and
    an MPC copy would lag one lighting interval behind the camera."""
    pos = g.node("MaterialExpressionWorldPosition",
                 world_position_shader_offset=E("WorldPositionIncludedOffsets", "WPT_CAMERA_RELATIVE_NO_OFFSETS"))
    forward = g.node("MaterialExpressionTransform",
                     transform_source_type=E("MaterialVectorCoordTransformSource", "TRANSFORMSOURCE_VIEW"),
                     transform_type=E("MaterialVectorCoordTransform", "TRANSFORM_WORLD"))
    g.connect(g.const3((0.0, 0.0, 1.0)), forward, "")
    tan = g.node("MaterialExpressionViewProperty",
                 property_=E("MaterialExposedViewProperty", "MEVP_TAN_HALF_FIELD_OF_VIEW"))
    return pos, forward, tan


def build_outline(g: Graph, ctx: Ctx, foliage: bool, ash: bool = False) -> None:
    """M_AF_Outline / M_AF_Outline_Foliage; ash: M_AF_Outline_Ash, the hull dropping the same plates as the body
    (_ash_mask: no ink floats over a dissolved part); without it the graph is exactly M_AF_Outline's."""
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
    if ash:
        mask = g.mul(mask, _ash_mask(g, ctx))
    pos, forward, tan = _view_inputs(g)
    wpo = g.custom(hlsl.outline_wpo_node(sh), {
        "Pos": pos, "N": g.node("MaterialExpressionVertexNormalWS"), "F": forward, "T": tan,
        "Px": g.scalar("OutlinePx1080", 3.0, group="Outline"),
        "Baked": g.scalar("BakedWidthCm", 1.5, group="Outline"),
        "Enable": g.scalar("OutlineScreenSpace", 1.0, group="Outline"),
        "InkScale": g.mpc(ctx.mpc, "InkScale"),
        "ExtraPx1080": g.const(0.0),
    })
    total = g.plus(wpo, _wind(g, ctx)) if foliage else wpo
    g.output(ink, "MP_EMISSIVE_COLOR")
    g.output(mask, "MP_OPACITY_MASK")
    # Every quality tier (DECISIONS R3 revised: the mobile-low baked-only fallback is gone); OutlineScreenSpace 0 is
    # the kill switch back to the baked hull.
    g.output(total, "MP_WORLD_POSITION_OFFSET")


def build_select_rim(g: Graph, ctx: Ctx) -> None:
    """M_AF_SelectRim (DECISIONS R16): the ink hull dilated RimPx720 px (at 720 rows) beyond the ink and pushed up to
    RimDepthPushCm along the view ray (less near the feet: never below the actor's ground plane), so it shows only
    outside the whole figure's silhouette and still closes round the boots; dithers with the body's Fade (CPD 13);
    BodyMask 1 hides the slot (the weapon copies' body slot)."""
    sh = ctx.shading
    d = SELECT_RIM_DEFAULTS
    grp = "Selection rim"
    color = g.vector("RimColor", (*hex_to_linear(d["RimColor"]), 1.0), group=grp)
    mask = g.custom(hlsl.select_rim_mask_node(), {
        "Fade": g.scalar("Fade", 0.0, cpd=13, group="Feedback (custom primitive data)"),
        "BodyOpacity": g.scalar("BodyOpacity", 1.0, group="Look"),
        "BodyMask": g.scalar("BodyMask", d["BodyMask"], group=grp),
    })
    pos, forward, tan = _view_inputs(g)
    wpo = g.custom(hlsl.outline_wpo_node(sh), {
        "Pos": pos, "N": g.node("MaterialExpressionVertexNormalWS"), "F": forward, "T": tan,
        "Px": g.scalar("OutlinePx1080", d["OutlinePx1080"], group="Outline"),
        "Baked": g.scalar("BakedWidthCm", d["BakedWidthCm"], group="Outline"),
        "Enable": g.scalar("OutlineScreenSpace", 1.0, group="Outline"),
        "InkScale": g.mpc(ctx.mpc, "InkScale"),
        "ExtraPx1080": g.scale(g.scalar("RimPx720", d["RimPx720"], group=grp), 1080.0 / 720.0),
    })
    # the owner's ground plane (the actor stands on it): the push is clamped above it near the feet (hlsl.view_push_node)
    feet = g.node("MaterialExpressionActorPositionWS", origin_type=E("PositionOrigin", "CAMERA_RELATIVE"))
    push = g.custom(hlsl.view_push_node(), {
        "Pos": pos, "Off": wpo, "Push": g.scalar("RimDepthPushCm", d["RimDepthPushCm"], group=grp), "Feet": feet})
    g.output(color, "MP_EMISSIVE_COLOR")
    g.output(mask, "MP_OPACITY_MASK")
    g.output(push, "MP_WORLD_POSITION_OFFSET")


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
    _ground_grade(g, wires)
    detailed = g.custom(hlsl.terrain_node(True), wires)
    simple = g.custom(hlsl.terrain_node(False), wires)
    _terrain_outputs(g, ctx, g.quality(detailed, simple))


def _ground_grade(g: Graph, wires: dict[str, Any]) -> None:
    """DECISIONS R19: saturation / value of the composited ground (MI_AF_Terrain_<Theme>; 1 / 1 = as painted)."""
    for name in ("GroundSaturation", "GroundValue"):
        wires[name] = g.scalar(name, 1.0, group="Ground grade")


def _terrain_outputs(g: Graph, ctx: Ctx, albedo: Any) -> None:
    """Lit ground: BaseColor = albedo x MPC SunScale under the pi-lux sun (DECISIONS P11 revised: the sun is no longer
    scaled, so the toon's shadow-catcher sees SunColor exactly; the ground keeps abyss.SunScale through the MPC),
    plus the ambient fill (MPC Ambient) as emissive."""
    ambient = g.custom(hlsl.terrain_emissive_node(), {"Albedo": albedo, "Amb": g.mpc(ctx.mpc, "Ambient")})
    g.output(g.mul(albedo, g.mpc(ctx.mpc, "SunScale")), "MP_BASE_COLOR")
    g.output(ambient, "MP_EMISSIVE_COLOR")
    g.output(g.const(0.0), "MP_SPECULAR")
    g.output(g.const(1.0), "MP_ROUGHNESS")


def build_terrain_painted(g: Graph, ctx: Ctx) -> None:
    """M_AF_Terrain_Painted: the M_AF_Terrain tile mask over the art's painted ground (manifest terrain.<theme>).

    The five layer textures (`Tile<i>Layer`, one 3 x 3-tile period each) are sampled once at UV0 / 3 outside the Custom
    node with the shared wrap sampler (no sampler slot per layer) and handed in as linear colour; the node picks the
    winner's / runner-up's and composites the per-tile `Details` atlas (its own clamped sampler, explicit gradients).
    Bound per theme on MI_AF_Terrain_<Theme> (manifest.plan_terrain_instances); the defaults only keep it compiling."""
    vc = g.vertex_color()
    wires: dict[str, Any] = {
        "UV": g.texcoord(0), "Wall": vc["G"], "Skirt": vc["B"],
        "TileIds": g.texture_object("TileIds", ctx.tex_tile_ids, "SAMPLERTYPE_LINEAR_COLOR"),
        "MapCols": g.scalar("MapCols", 120.0, group="Map"), "MapRows": g.scalar("MapRows", 120.0, group="Map"),
    }
    layer_uv = g.texcoord(0, tiling=1.0 / paths.TERRAIN_PERIOD_TILES)
    for t in hlsl.TERRAIN_TILES:
        layer = g.texture(paths.terrain_layer_param(t), ctx.tex_white, "SAMPLERTYPE_COLOR", uv=layer_uv,
                          sampler_source="SSM_WRAP_WORLD_GROUP_SETTINGS")
        wires[f"Tile{t}Layer"] = layer["RGB"]
        wires[f"Tile{t}Lip"] = g.vector(f"Tile{t}Lip", (*ctx.terrain_defaults[f"Tile{t}Lip"], 1.0), group=f"Tile {t}")
        wires[f"Tile{t}Rank"] = g.scalar(f"Tile{t}Rank", ctx.terrain_ranks.get(t, 0.0), group=f"Tile {t}")
    for name in ("WaterShallow", "WaterFoam", "WaterBank"):
        wires[name] = g.vector(name, (*ctx.terrain_defaults[name], 1.0), group="Water")
    wires["Details"] = g.texture_object(paths.TERRAIN_DETAILS_PARAM, ctx.tex_white, "SAMPLERTYPE_COLOR")
    _ground_grade(g, wires)
    detailed = g.custom(hlsl.terrain_painted_node(True), wires)
    simple = g.custom(hlsl.terrain_painted_node(False), wires)
    _terrain_outputs(g, ctx, g.quality(detailed, simple))


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
        # the same sun strength as the lit ground (MPC SunScale = abyss.SunScale)
        "SunScale": g.mpc(ctx.mpc, "SunScale"),
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


def build_telegraph(g: Graph, ctx: Ctx) -> None:
    """M_AF_Telegraph (dark remake U2 / 2G; DECISIONS DR5, DR16): the crimson rim + bone fill that sweeps to the contact,
    the hazard zone's rim, the arena's outward rim and wash (hlsl.telegraph_node). Per decal / per frame: five float4
    custom primitive data groups (hlsl.TELEGRAPH_CPD, the runtime's AbyssTelegraphRules::Cpd); the contract's constants
    as parameter defaults (manifest materials.M_AF_Telegraph); the rim strip and the fill grain as texture objects."""
    t = ctx.telegraph
    grp = "Telegraph (custom primitive data)"
    look = "Telegraph"
    cpd = hlsl.TELEGRAPH_CPD
    wires: dict[str, Any] = {
        "UV": g.texcoord(0),
        "Pw": g.node("MaterialExpressionWorldPosition"),
        # editor-preview defaults: a 1 m circle, the normal rim, half filled. Each group is read as a float4 (`.a`): a
        # vector parameter's output 0 is RGB only, so its A is appended (Graph.float4; test_flow checks every Custom
        # input's width).
        "ShapeP": g.float4(g.vector("ShapeP", (0.0, 100.0, 0.0, 26.0), cpd=cpd["ShapeP"], group=grp)),
        "QuadP": g.float4(g.vector("QuadP", (208.0, 208.0, 0.0, 0.0), cpd=cpd["QuadP"], group=grp)),
        "InnerP": g.float4(g.vector("InnerP", (0.0, 0.0, 0.0, 0.0), cpd=cpd["InnerP"], group=grp)),
        "ModeP": g.float4(g.vector("ModeP", (1.0, 0.0, 0.0, 0.5), cpd=cpd["ModeP"], group=grp)),
        "AlphaP": g.float4(g.vector("AlphaP", (0.145, 0.85, 0.95, 0.0), cpd=cpd["AlphaP"], group=grp)),
        "FillColor": g.vector("FillColor", (*hex_to_linear(t.fill_color), 1.0), group=look),
        "RimColor": g.vector("RimColor", (*hex_to_linear(t.rim_color), 1.0), group=look),
        "FrontColor": g.vector("FrontColor", (*hex_to_linear(t.front_color), 1.0), group=look),
        "FlashRimColor": g.vector("FlashRimColor", (*hex_to_linear(t.flash_rim_color), 1.0), group=look),
        "WashColor": g.vector("WashColor", (*hex_to_linear(t.wash_color), 1.0), group=look),
        "FillGrain": g.scalar("FillGrain", t.fill_grain, group=look),
        "FillTileCm": g.scalar("FillTileCm", t.fill_tile_cm, group=look),
        "FrontWidthCm": g.scalar("FrontWidthCm", t.front_width_cm, group=look),
        "RimTileCm": g.scalar("RimTileCm", t.rim_tile_cm, group=look),
        "WashFadeInCm": g.scalar("WashFadeInCm", t.wash_fade_in_cm, group=look),
        "Rim": g.texture_object("Rim", ctx.tex_telegraph_rim or ctx.tex_glow, "SAMPLERTYPE_LINEAR_COLOR"),
        "Fill": g.texture_object("Fill", ctx.tex_telegraph_fill or ctx.tex_glow, "SAMPLERTYPE_LINEAR_COLOR"),
    }
    decal = g.custom(hlsl.telegraph_node(), wires)
    g.output(g.rgb(decal), "MP_EMISSIVE_COLOR")
    g.output(g.alpha(decal), "MP_OPACITY")


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


def build_ao_composite(g: Graph, ctx: Ctx) -> None:
    """M_AF_PP_AOComposite (DECISIONS P14): SceneColor x lerp(1, SSAO, AOStrength) on the visible ground, before the
    tone curve. Desktop balanced / high only (AAbyssZoneActor binds it where SSAO runs and makes the terrain alone
    render custom depth); M_AF_PP_Grade stays buildable but unbound."""
    scene = g.node("MaterialExpressionSceneTexture", scene_texture_id=E("SceneTextureId", "PPI_POST_PROCESS_INPUT0"))
    ao = g.node("MaterialExpressionSceneTexture", scene_texture_id=E("SceneTextureId", "PPI_AMBIENT_OCCLUSION"))
    depth = g.node("MaterialExpressionSceneTexture", scene_texture_id=E("SceneTextureId", "PPI_SCENE_DEPTH"))
    ground = g.node("MaterialExpressionSceneTexture", scene_texture_id=E("SceneTextureId", "PPI_CUSTOM_DEPTH"))
    color = g.custom(hlsl.ao_composite_node(), {
        "Scene": scene["Color"], "AO": ao["Color"], "AOStrength": g.scalar("AOStrength", 0.25, group="AO"),
        "SceneDepth": depth["Color"], "GroundDepth": ground["Color"]})
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
    Master(paths.M_TOON_SLIME, paths.MATERIALS_DIR, SLIME_SETTINGS, lambda g, c: build_toon(g, c, "slime")),
    Master(paths.M_TOON_FADE, paths.MATERIALS_DIR, FADE_SETTINGS, lambda g, c: build_toon(g, c, "fade")),
    Master(paths.M_TOON_FOLIAGE_FADE, paths.MATERIALS_DIR, FADE_SETTINGS,
           lambda g, c: build_toon(g, c, "fade_foliage")),
    Master(paths.M_OUTLINE, paths.MATERIALS_DIR, OUTLINE_SETTINGS, lambda g, c: build_outline(g, c, False)),
    # dark remake U3 (2G stage C): the ash husk's twins (runtime-only; same settings as their originals)
    Master(paths.M_TOON_ASH, paths.MATERIALS_DIR, TOON_SETTINGS, lambda g, c: build_toon(g, c, "toon", ash=True)),
    Master(paths.M_OUTLINE_ASH, paths.MATERIALS_DIR, OUTLINE_SETTINGS,
           lambda g, c: build_outline(g, c, False, ash=True)),
    # dark remake K6 (2G stage H): the hero's Ashnumb band twin (runtime-only; M_AF_Toon's settings)
    Master(paths.M_TOON_ASHNUMB, paths.MATERIALS_DIR, TOON_SETTINGS, lambda g, c: build_toon(g, c, "toon", band=True)),
    Master(paths.M_OUTLINE_FOLIAGE, paths.MATERIALS_DIR, OUTLINE_SETTINGS, lambda g, c: build_outline(g, c, True)),
    Master(paths.M_SELECT_RIM, paths.MATERIALS_DIR, SELECT_RIM_SETTINGS, build_select_rim),
    Master(paths.M_TERRAIN, paths.MATERIALS_DIR, TERRAIN_SETTINGS, build_terrain),
    Master(paths.M_TERRAIN_PAINTED, paths.MATERIALS_DIR, TERRAIN_SETTINGS, build_terrain_painted),
    Master(paths.M_WATER, paths.MATERIALS_DIR, WATER_SETTINGS, build_water),
    Master(paths.M_PP_GRADE, paths.MATERIALS_DIR, PP_SETTINGS, build_grade),
    Master(paths.M_PP_AO, paths.MATERIALS_DIR, PP_AO_SETTINGS, build_ao_composite),
    Master(paths.M_FX_ADDITIVE, paths.FX_MATERIALS_DIR, FX_ADD_SETTINGS, lambda g, c: build_fx_sprite(g, c, True)),
    Master(paths.M_FX_TRANSLUCENT, paths.FX_MATERIALS_DIR, FX_TRANS_SETTINGS, lambda g, c: build_fx_sprite(g, c, False)),
    Master(paths.M_FX_MESH, paths.FX_MATERIALS_DIR, FX_MESH_SETTINGS, build_fx_mesh),
    Master(paths.M_GHOST, paths.FX_MATERIALS_DIR, GHOST_SETTINGS, build_ghost),
    Master(paths.M_LIGHT_POOL, paths.FX_MATERIALS_DIR, QUAD_ADD_SETTINGS, lambda g, c: build_quad(g, c, "light_pool")),
    Master(paths.M_TARGET_RING, paths.FX_MATERIALS_DIR, QUAD_ADD_SETTINGS, lambda g, c: build_quad(g, c, "target_ring")),
    Master(paths.M_AFFIX_AURA, paths.FX_MATERIALS_DIR, QUAD_ADD_SETTINGS, lambda g, c: build_quad(g, c, "affix_aura")),
    Master(paths.M_BLOB_SHADOW, paths.FX_MATERIALS_DIR, BLOB_SETTINGS, build_blob),
    Master(paths.M_TELEGRAPH, paths.FX_MATERIALS_DIR, TELEGRAPH_SETTINGS, build_telegraph),
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


def _planned_texture(plan: Plan, name: str) -> str:
    """Object path of a planned texture ("" when the plan does not import it)."""
    entry = plan.texture(name)
    return paths.object_path(entry.content_path) if entry is not None else ""


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
        telegraph=plan.telegraph,
        tex_telegraph_rim=_planned_texture(plan, plan.telegraph.rim_texture),
        tex_telegraph_fill=_planned_texture(plan, plan.telegraph.fill_texture),
        ash=plan.ash,
        tex_ash_noise=_planned_texture(plan, plan.ash.noise_texture),
        ashnumb=plan.ashnumb,
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
        check_texture_parameters(ed, mat, dry.texture_params, path, report)
        report.asset(path, "Material", "unchanged")
        return mat
    ed.mel.delete_all_material_expressions(mat)
    # Classic root pins only (Substrate is off, DECISIONS P11): set the material attributes before building the graph.
    resolved = {k: (v.resolve() if isinstance(v, (E, C)) else v) for k, v in master.settings.items()}
    set_props(mat, resolved, subject=path)
    graph = Graph(ed, mat)
    master.build(graph, ctx)
    for note in graph.compat:
        report.compat_note(note, path)
    ed.mel.layout_material_expressions(mat)
    ed.mel.recompile_material(mat)
    check_texture_parameters(ed, mat, graph.texture_params, path, report)
    ed.set_tag(mat, TAG_MATERIAL, fp)
    ed.save(mat, force=True)
    report.asset(path, "Material", "created" if created else "rebuilt", expressions=len(graph.trace))
    return mat


def check_texture_parameters(ed: Editor, mat: unreal.Material, expected: list[str], path: str,
                             report: BuildReport) -> None:
    """Every texture parameter the graph created must be exposed to instances (the palettes are bound per instance)."""
    if not expected:
        return
    try:
        exposed = sorted({str(n) for n in ed.mel.get_texture_parameter_names(mat)})
    except Exception as e:  # noqa: BLE001 - diagnostics only
        report.note(f"{path}: texture parameter check unavailable ({type(e).__name__}: {e})")
        return
    missing = sorted(set(expected) - set(exposed))
    if missing:
        report.warning(f"{path}: texture parameter(s) {missing} not exposed after the rebuild (exposed: {exposed}); "
                       "instances cannot bind them")


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


NOT_COMPILED = "not compiled (NullRHI commandlet; run build_content.sh --shaders)"


def check_material_stats(ed: Editor, report: BuildReport, require_shaders: bool = False) -> None:
    """Compile errors (C++ helper) and instruction counts of every master for the editor's shader platform.

    A -run=pythonscript commandlet renders with the NullRHI unless started with -AllowCommandletRendering: it compiles
    no shader and every count reads 0, so the masters are listed as not compiled, with one note. require_shaders
    (--shaders) makes that, and a master without a pixel shader, an error."""
    if shaders_disabled():
        for master in MASTERS:
            if ed.load(f"{master.folder}/{master.name}") is not None:
                report.materials[master.name] = {"status": NOT_COMPILED}
        report.note("shaders not compiled in this session (a -run=pythonscript commandlet uses the NullRHI without "
                    "-AllowCommandletRendering): material statistics and compile errors not read; "
                    "`build_content.sh --shaders` compiles every master and reports them. It also fills the DDC the "
                    "uncooked -game run reads: without it that run compiles the masters on demand, waiting at each "
                    "zone entry and hiding zone dressing until its shaders are ready (abyss.WaitForCompilation, "
                    "abyss.DecorShaderGate; Source/Abyssfire/Public/World/WorldContract.md section 6), and a mesh whose "
                    "master is not ready meanwhile draws with the engine default material (lit grey, black away from "
                    "the sun)")
        if require_shaders:
            report.error("--shaders: this editor session compiles no shaders (NullRHI); start it with "
                         "-AllowCommandletRendering (build_content.sh --shaders adds it)")
        return
    compile_errors = ed.helper("get_material_compile_errors")
    if compile_errors is None:
        report.note("shader compile errors not read: AbyssContentBuildLibrary.get_material_compile_errors unavailable "
                    "(stale editor binary)")
    for master in MASTERS:
        path = f"{master.folder}/{master.name}"
        mat = ed.load(path)
        if mat is None:
            continue
        # The helper waits for the material's compilation, so the statistics below see the finished shader map.
        errors = [str(e) for e in compile_errors(mat)] if compile_errors is not None else []
        for err in errors:
            report.error(f"{master.name}: shader compile error: {err}")
        try:
            st = ed.mel.get_statistics(mat)
            vs = int(st.get_editor_property("num_vertex_shader_instructions"))
            ps = int(st.get_editor_property("num_pixel_shader_instructions"))
            samplers = int(st.get_editor_property("num_samplers"))
        except Exception as e:  # noqa: BLE001
            report.materials[master.name] = {"status": f"stats unavailable ({e})", "errors": len(errors)}
            continue
        if errors:
            status = f"COMPILE ERRORS ({len(errors)})"
        else:
            status = "ok" if ps > 0 else "NO SHADER (compile error? open it in the editor)"
        report.materials[master.name] = {"status": status, "vs": vs, "ps": ps, "samplers": samplers}
        if ps <= 0 and not errors:
            (report.error if require_shaders else report.warning)(
                f"{master.name}: no compiled pixel shader for the editor platform; check the log for "
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


SETTER_RETURNED_FALSE = ("MaterialEditingLibrary.set_material_instance_*_parameter_value returned False but applied "
                         "the value (UE 5.8); accepted after reading it back")


def _close(a: float, b: float) -> bool:
    return abs(float(a) - float(b)) <= 1e-6 + 1e-6 * abs(float(b))     # parameters are stored as float32


def _read_param(ed: Editor, mi: Any, kind: str, param: str) -> Any:
    if kind == "texture":
        return ed.mel.get_material_instance_texture_parameter_value(mi, param)
    if kind == "scalar":
        return float(ed.mel.get_material_instance_scalar_parameter_value(mi, param))
    c = ed.mel.get_material_instance_vector_parameter_value(mi, param)
    return (float(c.r), float(c.g), float(c.b), float(c.a))


def _param_matches(kind: str, got: Any, want: Any) -> bool:
    if kind == "texture":
        if got is None or want is None:
            return got is want
        return got == want or got.get_path_name() == want.get_path_name()
    if kind == "scalar":
        return _close(got, want)
    return all(_close(a, b) for a, b in zip(got, want))


def _engine_value(kind: str, want: Any) -> Any:
    return linear_color(want) if kind == "vector" else float(want) if kind == "scalar" else want


def _describe(value: Any) -> str:
    if hasattr(value, "get_path_name"):
        return value.get_path_name()
    if isinstance(value, tuple):
        return "(" + ", ".join(f"{v:.4g}" for v in value) + ")"
    return repr(value)


def _parent_names(ed: Editor, parent: Any, kind: str) -> list[str]:
    """The parent's parameters of this kind as MaterialEditingLibrary lists them (raises when it cannot)."""
    if kind == "texture":
        names = ed.mel.get_texture_parameter_names(parent)
    elif kind == "scalar":
        names = ed.mel.get_scalar_parameter_names(parent)
    else:
        names = ed.mel.get_vector_parameter_names(parent)
    return sorted(str(n) for n in names)


def _parent_param_names(ed: Editor, parent: Any, kind: str) -> str:
    """Diagnostics: the parent's parameters of this kind as MaterialEditingLibrary (and, for textures, the C++ helper)
    list them."""
    try:
        text = f"editor={_parent_names(ed, parent, kind)}"
    except Exception as e:  # noqa: BLE001 - diagnostics only
        text = f"editor=unreadable ({type(e).__name__})"
    if kind == "texture":
        infos = ed.helper("get_texture_parameter_infos")
        text += f" infos={sorted(str(n) for n in infos(parent))}" if infos is not None else " infos=helper unavailable"
    return text


def set_instance_parameter(ed: Editor, mi: Any, parent: Any, inst_name: str, kind: str, param: str, want: Any,
                           report: BuildReport) -> bool:
    """Gives a material instance one parameter override (kind: texture | scalar | vector); True when it changed.

    The value read back decides, not the setter's bool: UE 5.8's MaterialEditingLibrary set_material_instance_*_
    parameter_value apply the value but return False (one compatibility note per build). A value that did not stick goes
    through the C++ helper (AbyssContentBuildLibrary.set_material_instance_<kind>_parameter); BuildError when that is
    unavailable or does not apply it either. A parameter the parent does not declare is a BuildError up front: an
    instance (and the helper's Set*ParameterValueEditorOnly) keeps an override for any name, which then reads back as
    applied but does nothing."""
    try:
        declared = param in _parent_names(ed, parent, kind)
    except Exception:  # noqa: BLE001 - names unreadable: the read-back below still decides
        declared = True
    if not declared:
        raise BuildError(f"parent has no {kind} parameter {param} (parent {kind} params: "
                         f"{_parent_param_names(ed, parent, kind)})")
    if _param_matches(kind, _read_param(ed, mi, kind, param), want):
        return False
    if kind == "texture":
        returned = ed.mel.set_material_instance_texture_parameter_value(mi, param, want)
    elif kind == "scalar":
        returned = ed.mel.set_material_instance_scalar_parameter_value(mi, param, float(want))
    else:
        returned = ed.mel.set_material_instance_vector_parameter_value(mi, param, linear_color(want))
    got = _read_param(ed, mi, kind, param)
    if _param_matches(kind, got, want):
        if not returned:
            report.compat_note(SETTER_RETURNED_FALSE, inst_name)
        return True
    helper = ed.helper(f"set_material_instance_{kind}_parameter")
    if helper is None:
        fallback = "helper unavailable (stale editor binary)"
    else:
        helper(mi, param, _engine_value(kind, want))
        got = _read_param(ed, mi, kind, param)
        if _param_matches(kind, got, want):
            report.warning(f"{inst_name}: MaterialEditingLibrary did not apply {kind} parameter {param}; set through "
                           f"the C++ helper (parent {kind} params: {_parent_param_names(ed, parent, kind)})")
            return True
        fallback = "the C++ helper did not apply it either"
    raise BuildError(f"{kind} parameter {param} not applied (MaterialEditingLibrary returned {bool(returned)}, read "
                     f"back {_describe(got)}, wanted {_describe(want)}; {fallback}; parent {kind} params: "
                     f"{_parent_param_names(ed, parent, kind)})")


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
    wanted: list[tuple[str, str, Any]] = []
    for param, tex_name in sorted(inst.textures.items()):
        tex = ed.load(f"{paths.TEXTURES_DIR}/{tex_name}")
        if tex is None:
            raise BuildError(f"{inst.name}: texture {tex_name} missing (texture import failed?)")
        wanted.append(("texture", param, tex))
    wanted += [("scalar", param, float(value)) for param, value in sorted(inst.scalars.items())]
    wanted += [("vector", param, tuple(value)) for param, value in sorted(inst.vectors.items())]
    failures: list[str] = []
    for kind, param, value in wanted:
        try:
            changed |= set_instance_parameter(ed, mi, parent, inst.name, kind, param, value, report)
        except BuildError as e:
            failures.append(str(e))
    if changed:
        # Saved whatever the setters returned (and with the parameters that did apply when another one failed).
        mel.update_material_instance(mi)
        ed.save(mi, force=True)
    report.asset(f"{paths.INSTANCES_DIR}/{inst.name}", "MaterialInstanceConstant",
                 "created" if created else ("updated" if changed else "unchanged"), parent=inst.parent)
    if failures:
        raise BuildError(f"{inst.name}: " + "; ".join(failures))


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

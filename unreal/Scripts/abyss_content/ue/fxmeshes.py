"""SM_FX_Quad (WorldContract 4): the 100 x 100 cm quad of every VFX sprite, blob shadow, light pool and target ring.

Layout: XY plane centred on the origin, normal +Z, UV (0,0) at (-50, +50) and (1,1) at (+50, -50) (texture up = local
+Y, right = local +X). Built from a mesh description (exact UVs, no source file); when that API is unavailable the
engine plane (/Engine/BasicShapes/Plane, the same layout) is used as is. The art may ship its own SM_FX_Quad through the
manifest, which then wins.
"""
from __future__ import annotations

from typing import Any

import unreal

from .. import paths
from ..manifest import Plan
from ..report import BuildReport
from .core import BuildError, Editor

QUAD_VERSION = "SM_FX_Quad v1: 100x100 cm XY plane, +Z, UV(0,0) at (-50,+50)"
ENGINE_PLANE = "/Engine/BasicShapes/Plane"
CORNERS = ((-50.0, 50.0), (50.0, 50.0), (50.0, -50.0), (-50.0, -50.0))
UVS = ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))


def _build_geometry(mesh: Any) -> None:
    desc = unreal.StaticMesh.create_static_mesh_description(mesh)
    group = desc.create_polygon_group()
    desc.set_polygon_group_material_slot_name(group, "Sprite")
    instances = []
    for (x, y), (u, v) in zip(CORNERS, UVS):
        vertex = desc.create_vertex()
        desc.set_vertex_position(vertex, unreal.Vector(x, y, 0.0))
        inst = desc.create_vertex_instance(vertex)
        desc.set_vertex_instance_uv(inst, unreal.Vector2D(u, v), 0)
        instances.append(inst)
    # Two triangles; the quad materials are two-sided, so the winding only decides the stored normal.
    desc.create_triangle(group, [instances[0], instances[1], instances[2]])
    desc.create_triangle(group, [instances[0], instances[2], instances[3]])
    mesh.build_from_static_mesh_descriptions([desc], False, False)


def ensure_quad(ed: Editor, plan: Plan, report: BuildReport) -> None:
    if plan.asset(paths.SM_FX_QUAD) is not None:
        report.note(f"{paths.SM_FX_QUAD} comes from the art manifest")
        return
    target = f"{paths.FX_DIR}/{paths.SM_FX_QUAD}"
    mesh = ed.load(target)
    if mesh is not None and ed.get_tag(mesh, paths.TAG_BUILD) == QUAD_VERSION:
        report.asset(target, "StaticMesh", "unchanged")
        return
    ed.ensure_dir(paths.FX_DIR)
    created = mesh is None
    if mesh is None:
        mesh = ed.assets.duplicate_asset(ENGINE_PLANE, target)
        if mesh is None:
            raise BuildError(f"could not duplicate {ENGINE_PLANE} to {target}")
    source = "mesh description"
    try:
        _build_geometry(mesh)
    except Exception as e:  # noqa: BLE001 - the engine plane has the same layout
        source = f"engine plane ({e})"
        report.warning(f"{paths.SM_FX_QUAD}: mesh description build failed, keeping the engine plane geometry ({e})")
    box = mesh.get_bounding_box()
    lo, hi = box.get_editor_property("min"), box.get_editor_property("max")
    if abs(lo.x + 50) > 0.5 or abs(lo.y + 50) > 0.5 or abs(hi.x - 50) > 0.5 or abs(hi.y - 50) > 0.5 \
            or abs(lo.z) > 0.5 or abs(hi.z) > 0.5:
        report.error(f"{paths.SM_FX_QUAD}: bounds ({lo.x:.1f},{lo.y:.1f},{lo.z:.1f})-({hi.x:.1f},{hi.y:.1f},{hi.z:.1f}) "
                     "are not the 100 x 100 cm XY quad")
    fx_additive = ed.load(f"{paths.FX_MATERIALS_DIR}/{paths.M_FX_ADDITIVE}")
    if fx_additive is not None:
        mesh.set_material(0, fx_additive)   # no engine-content material reference in the cooked quad
    if ed.smes is not None:
        try:
            if int(ed.smes.get_simple_collision_count(mesh)) > 0:
                ed.smes.remove_collisions(mesh)
        except Exception:  # noqa: BLE001
            pass
    ed.set_tag(mesh, paths.TAG_BUILD, QUAD_VERSION)
    ed.save(mesh, force=True)
    report.asset(target, "StaticMesh", "created" if created else "rebuilt", source=source)

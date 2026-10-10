"""Skeletal meshes (shared skeletons, LODs, sockets), animations and static meshes from Art/Export (ue58-platform.md 11.3).

Interchange through synchronous AssetImportTasks with an explicit InterchangeGenericAssetsPipeline (the project's default
pipeline stack is replaced, so editor defaults never leak in):
* import normals as authored (the outline hull's custom outward normals, Art/blender/README.md 2), vertex colours
  replaced from the file (AF_Data), no materials / textures (built from the manifest), no physics assets, no Nanite,
  no lightmap UVs, sockets from SOCKET_ empties (static meshes);
* the first skeletal mesh of a skeleton family creates the skeleton, which is moved to /Game/Abyssfire/Skeletons/<SKEL_x>;
  the others and every animation import onto it;
* idempotent: an asset whose sources and import settings are unchanged (fingerprint tag) is not re-imported.
"""
from __future__ import annotations

from typing import Any

import unreal

from .. import paths
from ..manifest import Asset, Clip, Plan
from ..report import BuildReport
from .core import BuildError, Editor, enum, fingerprint, set_props


# ---------------------------------------------------------------------------------------------------------------------
# Pipelines
# ---------------------------------------------------------------------------------------------------------------------
def make_pipeline(kind: str, skeleton: Any = None, fps: int = 60, notes: list[str] | None = None) -> Any:
    """kind: 'skeletal' | 'static' | 'anim'."""
    if kind not in ("skeletal", "static", "anim"):
        raise BuildError(f"unknown pipeline kind {kind}")
    p = unreal.InterchangeGenericAssetsPipeline()
    soft: list[str] = notes if notes is not None else []
    set_props(p, {"use_source_name_for_asset": True, "asset_type_sub_folders": False,
                  "scene_name_sub_folder": False}, "assets pipeline", required=False, notes=soft)

    common = p.get_editor_property("common_meshes_properties")
    force_type = {"skeletal": "IFMT_SKELETAL_MESH", "static": "IFMT_STATIC_MESH", "anim": "IFMT_NONE"}[kind]
    set_props(common, {
        "force_all_mesh_as_type": enum("InterchangeForceMeshType", force_type),
        "vertex_color_import_option": enum("InterchangeVertexColorImportOption", "IVCIO_REPLACE"),
        "recompute_normals": False,
    }, "common meshes")
    set_props(common, {
        "auto_detect_mesh_type": False,
        "import_lods": True,
        "bake_meshes": True,
        "keep_sections_separate": False,
        "import_sockets": True,
        "recompute_tangents": True,
        "use_mikk_t_space": True,
        "compute_weighted_normals": False,
        "use_high_precision_tangent_basis": False,
        "use_full_precision_u_vs": False,
    }, "common meshes", required=False, notes=soft)

    skm = p.get_editor_property("common_skeletal_meshes_and_animations_properties")
    set_props(skm, {"import_only_animations": kind == "anim"}, "skeletal/animation common")
    if skeleton is not None:
        set_props(skm, {"skeleton": skeleton}, "skeletal/animation common")
    elif kind == "anim":
        raise BuildError("animations import onto an existing skeleton")
    set_props(skm, {"use_t0_as_ref_pose": False, "convert_statics_with_morph_targets_to_skeletals": False},
              "skeletal/animation common", required=False, notes=soft)

    mesh = p.get_editor_property("mesh_pipeline")
    set_props(mesh, {
        "import_skeletal_meshes": kind == "skeletal",
        "import_static_meshes": kind == "static",
        "create_physics_asset": False,
    }, "mesh pipeline")
    set_props(mesh, {
        "import_morph_targets": False,
        "import_vertex_attributes": False,
        "update_skeleton_reference_pose": False,
        "use_high_precision_skin_weights": False,
        "combine_static_meshes": True,
        "collision": False,
        "import_collision": False,
        "generate_lightmap_u_vs": False,
        "build_nanite": False,
        "build_reversed_index_buffer": False,
        "auto_compute_lod_screen_sizes": True,
        "import_geometry_caches": False,
    }, "mesh pipeline", required=False, notes=soft)

    anim = p.get_editor_property("animation_pipeline")
    set_props(anim, {"import_animations": kind == "anim"}, "animation pipeline")
    set_props(anim, {
        "import_bone_tracks": True,
        "animation_range": enum("InterchangeAnimationRange", "TIMELINE"),
        "custom_bone_animation_sample_rate": int(fps),
        "use30_hz_to_bake_bone_animation": False,
        "snap_to_closest_frame_boundary": False,
        "import_custom_attribute": False,
    }, "animation pipeline", required=False, notes=soft)

    mat = p.get_editor_property("material_pipeline")
    set_props(mat, {"import_materials": False}, "material pipeline")
    try:
        tex = mat.get_editor_property("texture_pipeline")
        set_props(tex, {"import_textures": False}, "texture pipeline", required=False, notes=soft)
    except Exception as e:  # noqa: BLE001
        soft.append(f"texture pipeline not reachable ({e})")

    stack = unreal.InterchangePipelineStackOverride()
    stack.add_pipeline(p)
    return stack


# ---------------------------------------------------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------------------------------------------------
def _package(obj: Any) -> str:
    return obj.get_path_name().split(".", 1)[0]


def _take(objects: list[Any], cls: type, expected_name: str) -> Any:
    found = [o for o in objects if isinstance(o, cls)]
    if not found:
        return None
    for o in found:
        if o.get_name() == expected_name:
            return o
    return found[0]


def _move_to(ed: Editor, obj: Any, target_package: str, report: BuildReport) -> Any:
    """Moves an imported asset to its contract path (Interchange names assets after the source)."""
    current = _package(obj)
    if current == target_package:
        return obj
    if ed.exists(target_package):
        stale = ed.load(target_package)
        if stale is not None and stale != obj:
            if not ed.delete(target_package):
                raise BuildError(f"{target_package} exists and cannot be replaced by {current}")
    ed.rename(current, target_package)
    report.note(f"moved {current} -> {target_package}")
    moved = ed.load(target_package)
    if moved is None:
        raise BuildError(f"{target_package} missing after the move")
    _drop_redirector(ed, current, report)
    return moved


def _drop_redirector(ed: Editor, old_package: str, report: BuildReport) -> None:
    if not ed.exists(old_package):
        return
    obj = ed.load(old_package)
    if obj is None or type(obj).__name__ == "ObjectRedirector":
        try:
            ed.delete(old_package)
        except Exception as e:  # noqa: BLE001
            report.warning(f"redirector {old_package} left behind ({e}); run Fix Up Redirectors in the editor")


def _discard_extras(ed: Editor, objects: list[Any], keep: list[Any], report: BuildReport) -> None:
    """Deletes side assets an import created that the contract does not want (physics assets, materials, textures)."""
    keep_paths = {k.get_path_name() for k in keep if k is not None}
    for o in objects:
        if o is None or o.get_path_name() in keep_paths:
            continue
        if isinstance(o, (unreal.PhysicsAsset, unreal.MaterialInterface, unreal.Texture)):
            pkg = _package(o)
            if pkg.startswith(paths.CONTENT_ROOT) and ed.delete(pkg):
                report.note(f"deleted import side asset {pkg}")


def _skeleton_path(name: str) -> str:
    return f"{paths.SKELETONS_DIR}/{name}"


# ---------------------------------------------------------------------------------------------------------------------
# Skeletal meshes
# ---------------------------------------------------------------------------------------------------------------------
def import_skeletal(ed: Editor, asset: Asset, report: BuildReport, force: bool) -> Any:
    target = asset.content_path
    skel_target = _skeleton_path(asset.skeleton)
    skeleton = ed.load(skel_target)
    fp = fingerprint([asset.fbx] + [lod.fbx for lod in asset.lods], salt=f"skeletal:{asset.skeleton}")
    existing = ed.load(target)
    if (existing is not None and not force and ed.get_tag(existing, paths.TAG_SOURCE_SHA) == fp
            and skeleton is not None and existing.get_editor_property("skeleton") == skeleton):
        report.asset(target, "SkeletalMesh", "unchanged", skeleton=asset.skeleton)
        return existing

    notes: list[str] = []
    objects = ed.import_file(asset.fbx, f"{paths.CONTENT_ROOT}/{asset.folder}", make_pipeline("skeletal", skeleton,
                                                                                              notes=notes))
    for n in notes:
        report.note(f"{asset.name}: {n}")
    mesh = _take(objects, unreal.SkeletalMesh, asset.name)
    if mesh is None:
        raise BuildError(f"{asset.fbx.name} did not produce a skeletal mesh (got "
                         f"{[type(o).__name__ for o in objects]})")
    mesh = _move_to(ed, mesh, target, report)
    skel = mesh.get_editor_property("skeleton")
    if skel is None:
        raise BuildError(f"{asset.name}: imported without a skeleton")
    if skeleton is None:
        # First mesh of the family created the skeleton: give it the contract name.
        ed.ensure_dir(paths.SKELETONS_DIR)
        if _package(skel) != skel_target:
            # Renamed before anything is saved: nothing on disk references the new skeleton yet, so no redirector.
            old = _package(skel)
            ed.rename(old, skel_target)
            report.note(f"skeleton {old} -> {skel_target}")
            skel = ed.load(skel_target)
            ed.save(mesh, force=True)
            _drop_redirector(ed, old, report)
        ed.save(skel, force=True)
        report.asset(skel_target, "Skeleton", "created")
    elif skel != skeleton:
        raise BuildError(f"{asset.name}: Interchange bound the mesh to {skel.get_path_name()} instead of {skel_target}; "
                         "the armature does not match the family skeleton (bone names / hierarchy)")
    _discard_extras(ed, objects, [mesh, skel], report)

    import_lods(ed, asset, mesh, report)
    ed.set_tag(mesh, paths.TAG_SOURCE_SHA, fp)
    ed.set_tag(mesh, paths.TAG_BUILD, f"Art/Export/{asset.folder}/{asset.fbx.name}")
    ed.save(mesh, force=True)
    report.asset(target, "SkeletalMesh", "imported" if existing is None else "reimported", skeleton=asset.skeleton)
    return mesh


def import_lods(ed: Editor, asset: Asset, mesh: Any, report: BuildReport) -> None:
    if not asset.lods:
        return
    if ed.skm is None:
        report.warning(f"{asset.name}: SkeletalMeshEditorSubsystem unavailable, LODs not imported")
        return
    for lod in asset.lods:
        try:
            got = int(ed.skm.import_lod(mesh, lod.index, str(lod.fbx)))
        except Exception as e:  # noqa: BLE001
            report.warning(f"{asset.name}: LOD{lod.index} import failed ({e})")
            continue
        ed.wait_for_interchange()
        if got != lod.index:
            report.warning(f"{asset.name}: LOD{lod.index} import returned {got} (LOD left to the engine reduction)")
    count = int(ed.skm.get_lod_count(mesh))
    infos = list(mesh.get_editor_property("lod_info") or [])
    sizes = {0: 1.0}
    sizes.update({lod.index: lod.screen_size for lod in asset.lods})
    changed = False
    for idx, info in enumerate(infos):
        if idx in sizes:
            current = info.get_editor_property("screen_size")
            if abs(float(current.get_editor_property("default")) - sizes[idx]) > 1e-4:
                info.set_editor_property("screen_size", unreal.PerPlatformFloat(sizes[idx]))
                infos[idx] = info
                changed = True
    if changed:
        mesh.set_editor_property("lod_info", infos)
    report.verification.append(f"{asset.name}: {count} LOD(s) (manifest: {1 + len(asset.lods)})")
    if count < 1 + len(asset.lods):
        report.warning(f"{asset.name}: expected {1 + len(asset.lods)} LODs, the mesh has {count}")


def apply_skeletal_sockets(ed: Editor, asset: Asset, mesh: Any, report: BuildReport) -> None:
    """Bone-relative mesh sockets from the manifest (fx_feet, fx_chest, fx_head, fx_overhead, fx_hand_l/r, visor).

    SkeletalMeshSocket.socket_name / bone_name are read-only to Python: the socket object is created here, named by the
    C++ helper UAbyssContentBuildLibrary::ConfigureSkeletalMeshSocket and added with SkeletalMesh.add_socket. Without
    the helper the runtime falls back to fractions of the mesh height (WorldContract 2.3)."""
    if not asset.sockets:
        return
    lib = ed.content_lib
    if lib is None or not hasattr(lib, "configure_skeletal_mesh_socket"):
        report.warning(f"{asset.name}: {len(asset.sockets)} socket(s) not created: AbyssContentBuildLibrary missing "
                       "(build the AbyssfireEditor target first)")
        return
    changed = False
    for s in asset.sockets:
        loc = unreal.Vector(*s.loc)
        rot = unreal.Rotator(roll=s.rot[0], pitch=s.rot[1], yaw=s.rot[2])
        try:
            sock, _ = mesh.find_socket_and_index(s.name)
        except Exception:  # noqa: BLE001
            sock = None
        if sock is None:
            sock = unreal.SkeletalMeshSocket(mesh)
            if not lib.configure_skeletal_mesh_socket(mesh, sock, s.name, s.bone):
                report.error(f"{asset.name}: socket {s.name}: bone {s.bone} is not in the mesh")
                continue
            sock.set_editor_property("relative_location", loc)
            sock.set_editor_property("relative_rotation", rot)
            mesh.add_socket(sock, False)
            changed = True
            continue
        if str(sock.get_editor_property("bone_name")) != s.bone:
            if not lib.configure_skeletal_mesh_socket(mesh, sock, s.name, s.bone):
                report.error(f"{asset.name}: socket {s.name}: bone {s.bone} is not in the mesh")
                continue
            changed = True
        changed |= set_props(sock, {"relative_location": loc, "relative_rotation": rot}, f"{asset.name}.{s.name}")
    if changed:
        ed.save(mesh, force=True)
    found = []
    for s in asset.sockets:
        try:
            sock, _ = mesh.find_socket_and_index(s.name)
        except Exception:  # noqa: BLE001
            sock = None
        if sock is None:
            report.error(f"{asset.name}: socket {s.name} missing after the build")
        else:
            found.append(s.name)
    report.verification.append(f"{asset.name}: sockets {', '.join(found) or 'none'}")


# ---------------------------------------------------------------------------------------------------------------------
# Animations
# ---------------------------------------------------------------------------------------------------------------------
def _clip_order(clips: list[Clip]) -> list[Clip]:
    """Non-additive clips first (additive clips may reference another clip as their base pose)."""
    return sorted(clips, key=lambda c: (c.additive, c.asset))


def import_clip(ed: Editor, asset: Asset, clip: Clip, skeleton: Any, report: BuildReport, force: bool) -> Any:
    target = clip.content_path
    fp = fingerprint([clip.fbx], salt=f"anim:{asset.skeleton}:{clip.fps}")
    existing = ed.load(target)
    if existing is not None and not force and ed.get_tag(existing, paths.TAG_SOURCE_SHA) == fp \
            and existing.get_editor_property("skeleton") == skeleton:
        seq = existing
        action = "unchanged"
    else:
        notes: list[str] = []
        objects = ed.import_file(clip.fbx, f"{paths.CONTENT_ROOT}/{clip.folder}",
                                 make_pipeline("anim", skeleton, clip.fps, notes=notes))
        seq = _take(objects, unreal.AnimSequence, clip.asset)
        if seq is None:
            raise BuildError(f"{clip.fbx.name} did not produce an animation (got {[type(o).__name__ for o in objects]})")
        seq = _move_to(ed, seq, target, report)
        _discard_extras(ed, objects, [seq, skeleton], report)
        stray = [o for o in objects if isinstance(o, unreal.Skeleton) and o != skeleton]
        if stray:
            raise BuildError(f"{clip.asset}: the import created a new skeleton ({stray[0].get_path_name()}); the clip "
                             f"armature does not match {asset.skeleton}")
        ed.set_tag(seq, paths.TAG_SOURCE_SHA, fp)
        action = "imported" if existing is None else "reimported"
    changed = apply_clip_settings(ed, clip, seq, report)
    if action != "unchanged" or changed:
        ed.save(seq, force=True)
    verify_clip(clip, seq, report)
    report.asset(target, "AnimSequence", action if not (changed and action == "unchanged") else "updated",
                 lengthMs=clip.length_ms)
    return seq


def apply_clip_settings(ed: Editor, clip: Clip, seq: Any, report: BuildReport) -> bool:
    notes: list[str] = []
    props: dict[str, Any] = {"enable_root_motion": False}
    changed = set_props(seq, props, clip.asset)
    changed |= set_props(seq, {"loop": bool(clip.loop)}, clip.asset, required=False, notes=notes)
    if clip.additive:
        additive: dict[str, Any] = {
            "additive_anim_type": enum("AdditiveAnimationType", "AAT_LOCAL_SPACE_BASE"),
            "ref_pose_type": enum("AdditiveBasePoseType", clip.additive_base_type or "ABPT_LOCAL_ANIM_FRAME"),
            "ref_frame_index": int(clip.additive_base_frame),
        }
        if clip.additive_base_type in ("ABPT_ANIM_FRAME", "ABPT_ANIM_SCALED"):
            if not clip.additive_base_anim:
                raise BuildError(f"{clip.asset}: additive base {clip.additive_base_type} needs a base animation")
            base = ed.load(f"{paths.CONTENT_ROOT}/{clip.folder}/{clip.additive_base_anim}")
            if base is None:
                raise BuildError(f"{clip.asset}: additive base animation {clip.additive_base_anim} missing")
            additive["ref_pose_seq"] = base
        changed |= set_props(seq, additive, clip.asset)
    for n in notes:
        report.note(n)
    return changed


def verify_clip(clip: Clip, seq: Any, report: BuildReport) -> None:
    try:
        length = float(seq.get_editor_property("sequence_length"))
    except Exception:  # noqa: BLE001
        length = float(unreal.AnimationLibrary.get_sequence_length(seq))
    want = clip.length_ms / 1000.0
    frame = 1.0 / max(clip.fps, 1)
    if want > 0 and abs(length - want) > frame * 1.01:
        report.warning(f"{clip.asset}: {length * 1000:.1f} ms imported, manifest {clip.length_ms:.1f} ms "
                       "(the runtime times contacts from the manifest)")
    try:
        frames = int(unreal.AnimationLibrary.get_num_frames(seq))
        if clip.frames and abs(frames - clip.frames) > 1:
            report.warning(f"{clip.asset}: {frames} frames imported, manifest {clip.frames}")
    except Exception:  # noqa: BLE001
        pass


# ---------------------------------------------------------------------------------------------------------------------
# Static meshes
# ---------------------------------------------------------------------------------------------------------------------
def import_static(ed: Editor, asset: Asset, report: BuildReport, force: bool) -> Any:
    target = asset.content_path
    fp = fingerprint([asset.fbx], salt="static")
    existing = ed.load(target)
    if existing is not None and not force and ed.get_tag(existing, paths.TAG_SOURCE_SHA) == fp:
        report.asset(target, "StaticMesh", "unchanged")
        return existing
    notes: list[str] = []
    objects = ed.import_file(asset.fbx, f"{paths.CONTENT_ROOT}/{asset.folder}", make_pipeline("static", notes=notes))
    for n in notes:
        report.note(f"{asset.name}: {n}")
    mesh = _take(objects, unreal.StaticMesh, asset.name)
    if mesh is None:
        raise BuildError(f"{asset.fbx.name} did not produce a static mesh (got {[type(o).__name__ for o in objects]})")
    extra_meshes = [o for o in objects if isinstance(o, unreal.StaticMesh) and o != mesh]
    if extra_meshes:
        report.warning(f"{asset.name}: the FBX produced {len(extra_meshes) + 1} static meshes; keeping {mesh.get_name()}")
    mesh = _move_to(ed, mesh, target, report)
    _discard_extras(ed, objects, [mesh], report)
    if ed.smes is not None:
        try:
            if int(ed.smes.get_simple_collision_count(mesh)) > 0:
                ed.smes.remove_collisions(mesh)    # no physics in the game (ue58-platform.md 13)
        except Exception as e:  # noqa: BLE001
            report.note(f"{asset.name}: collision cleanup skipped ({e})")
    ed.set_tag(mesh, paths.TAG_SOURCE_SHA, fp)
    ed.save(mesh, force=True)
    report.asset(target, "StaticMesh", "imported" if existing is None else "reimported")
    return mesh


def apply_static_sockets(ed: Editor, asset: Asset, mesh: Any, report: BuildReport) -> None:
    """SOCKET_ empties import as sockets; any manifest socket missing (or off by > 1 mm) is (re)written."""
    if not asset.sockets:
        return
    changed = False
    for s in asset.sockets:
        loc = unreal.Vector(*s.loc)
        rot = unreal.Rotator(roll=s.rot[0], pitch=s.rot[1], yaw=s.rot[2])
        sock = mesh.find_socket(s.name)
        if sock is None:
            sock = unreal.StaticMeshSocket(mesh)
            sock.set_editor_property("socket_name", s.name)
            sock.set_editor_property("relative_location", loc)
            sock.set_editor_property("relative_rotation", rot)
            mesh.add_socket(sock)
            report.note(f"{asset.name}: socket {s.name} added from the manifest")
            changed = True
            continue
        cur = sock.get_editor_property("relative_location")
        if max(abs(cur.x - loc.x), abs(cur.y - loc.y), abs(cur.z - loc.z)) > 0.1:
            report.warning(f"{asset.name}: socket {s.name} at ({cur.x:.1f}, {cur.y:.1f}, {cur.z:.1f}), manifest "
                           f"({loc.x:.1f}, {loc.y:.1f}, {loc.z:.1f}): using the manifest")
            sock.set_editor_property("relative_location", loc)
            sock.set_editor_property("relative_rotation", rot)
            changed = True
    if changed:
        ed.save(mesh, force=True)
    report.verification.append(f"{asset.name}: sockets {', '.join(s.name for s in asset.sockets)}")


# ---------------------------------------------------------------------------------------------------------------------
# Steps
# ---------------------------------------------------------------------------------------------------------------------
def import_all_skeletal(ed: Editor, plan: Plan, report: BuildReport, force: bool) -> dict[str, Any]:
    meshes: dict[str, Any] = {}
    for skeleton_name, names in sorted(plan.skeleton_families.items()):
        for name in names:
            asset = plan.asset(name)
            if asset is None:
                continue
            try:
                mesh = import_skeletal(ed, asset, report, force)
                apply_skeletal_sockets(ed, asset, mesh, report)
                meshes[name] = mesh
            except BuildError as e:
                report.error(f"{name}: {e}")
            except Exception as e:  # noqa: BLE001
                report.error(f"{name}: unexpected {type(e).__name__}: {e}")
    return meshes


def import_all_clips(ed: Editor, plan: Plan, report: BuildReport, force: bool) -> None:
    for asset in plan.assets:
        if not asset.skeletal or not asset.clips:
            continue
        skeleton = ed.load(_skeleton_path(asset.skeleton))
        if skeleton is None:
            report.error(f"{asset.name}: skeleton {asset.skeleton} missing, {len(asset.clips)} clip(s) skipped")
            continue
        with unreal.ScopedSlowTask(len(asset.clips), f"Animations of {asset.name}") as slow:
            slow.make_dialog(True)
            for clip in _clip_order(asset.clips):
                if slow.should_cancel():
                    raise BuildError("cancelled")
                slow.enter_progress_frame(1, clip.asset)
                try:
                    import_clip(ed, asset, clip, skeleton, report, force)
                except BuildError as e:
                    report.error(f"{clip.asset}: {e}")
                except Exception as e:  # noqa: BLE001
                    report.error(f"{clip.asset}: unexpected {type(e).__name__}: {e}")


def import_all_static(ed: Editor, plan: Plan, report: BuildReport, force: bool) -> dict[str, Any]:
    meshes: dict[str, Any] = {}
    for asset in plan.assets:
        if asset.skeletal:
            continue
        try:
            mesh = import_static(ed, asset, report, force)
            apply_static_sockets(ed, asset, mesh, report)
            meshes[asset.name] = mesh
        except BuildError as e:
            report.error(f"{asset.name}: {e}")
        except Exception as e:  # noqa: BLE001
            report.error(f"{asset.name}: unexpected {type(e).__name__}: {e}")
    return meshes

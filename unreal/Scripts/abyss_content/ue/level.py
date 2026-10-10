"""L_Main (DECISIONS P9, ue58-platform.md 10.4 / 11.7): the one persistent map. It holds nothing but its world settings;
the Abyssfire module spawns light, fog, post-process, camera and every zone at runtime. Plus a check of the project
settings that point at it (Config/DefaultEngine.ini, owned by the backbone).
"""
from __future__ import annotations

from typing import Any

import unreal

from .. import paths
from ..report import BuildReport
from .core import BuildError, Editor, set_props

WORLD_SETTINGS = {
    "kill_z": -10000.0,
    "enable_world_bounds_checks": False,
    "force_no_precomputed_lighting": True,
    "enable_navigation_system": False,
    "enable_ai_system": False,
    "precompute_visibility": False,
}
# Actors a blank map may legitimately hold (UE creates the world settings / default brush itself).
ALLOWED_ACTOR_CLASSES = {"WorldSettings", "Brush", "DefaultPhysicsVolume", "LevelBounds", "GameplayDebuggerCategoryReplicator",
                         "AbstractNavData"}


def _soft_path(value: Any) -> str:
    try:
        text = value.export_text()
    except Exception:  # noqa: BLE001
        text = str(value)
    return str(text).strip().strip('"').strip("'")


def ensure_main_map(ed: Editor, report: BuildReport) -> None:
    path = paths.MAIN_MAP
    if ed.levels is None:
        raise BuildError("LevelEditorSubsystem unavailable")
    ed.ensure_dir(paths.MAPS_DIR)
    created = False
    if ed.exists(path):
        if not ed.levels.load_level(path):
            raise BuildError(f"could not load {path}")
    else:
        ok = False
        try:
            ok = bool(ed.levels.new_level(path, False))
        except Exception as e:  # noqa: BLE001
            report.note(f"LevelEditorSubsystem.new_level failed ({e}); trying EditorLoadingAndSavingUtils")
        if not ok:
            world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
            if world is None or not unreal.EditorLoadingAndSavingUtils.save_map(world, path):
                raise BuildError(f"could not create {path}")
        created = True
    world = ed.unreal_editor.get_editor_world() if ed.unreal_editor is not None else None
    if world is None:
        raise BuildError("no editor world after loading L_Main")
    notes: list[str] = []
    changed = set_props(world.get_world_settings(), WORLD_SETTINGS, "L_Main world settings", required=False,
                        notes=notes)
    for n in notes:
        report.note(n)
    if ed.actors is not None:
        actors = list(ed.actors.get_all_level_actors() or [])
        classes = sorted({a.get_class().get_name() for a in actors})
        unexpected = [c for c in classes if c not in ALLOWED_ACTOR_CLASSES]
        report.verification.append(f"L_Main actors: {len(actors)} ({', '.join(classes) or 'none'})")
        if unexpected:
            report.warning(f"L_Main holds placed actors ({', '.join(unexpected)}); the game spawns everything at "
                           "runtime (P9) — remove them unless deliberate")
    if created or changed:
        if not ed.levels.save_current_level():
            raise BuildError(f"could not save {path}")
    umap = paths.CONTENT_DIR / "Abyssfire" / "Maps" / "L_Main.umap"
    if not umap.is_file():
        report.error(f"{umap} was not written")
    report.asset(path, "World", "created" if created else ("updated" if changed else "unchanged"))


def check_project_settings(report: BuildReport) -> None:
    """Default maps / game mode / game instance (Config/DefaultEngine.ini) and the rendering switches the materials
    depend on. Reported, not rewritten: Config/ belongs to the backbone."""
    expected_map = paths.object_path(paths.MAIN_MAP)
    try:
        gms = unreal.GameMapsSettings.get_game_maps_settings()
        for prop in ("game_default_map", "editor_startup_map"):
            value = _soft_path(gms.get_editor_property(prop))
            if value != expected_map:
                report.warning(f"GameMapsSettings.{prop} is {value!r}, expected {expected_map!r} "
                               "(Config/DefaultEngine.ini [/Script/EngineSettings.GameMapsSettings])")
            else:
                report.verification.append(f"GameMapsSettings.{prop} = {value}")
        for prop, want in (("global_default_game_mode", "/Script/Abyssfire.AbyssGameMode"),
                           ("game_instance_class", "/Script/Abyssfire.AbyssGameInstance")):
            value = _soft_path(gms.get_editor_property(prop))
            if value != want:
                report.warning(f"GameMapsSettings.{prop} is {value!r}, expected {want!r}")
    except Exception as e:  # noqa: BLE001
        report.warning(f"GameMapsSettings not readable ({e})")
    for cvar, want in (("r.Substrate", False), ("r.Mobile.ShadingPath", None)):
        try:
            if want is None:
                report.verification.append(f"{cvar} = {unreal.SystemLibrary.get_console_variable_int_value(cvar)}")
                continue
            value = bool(unreal.SystemLibrary.get_console_variable_bool_value(cvar))
            if value != want:
                report.error(f"{cvar} is {value}; the materials are built for the classic (non-Substrate) root "
                             "(DECISIONS P11, Config/DefaultEngine.ini)")
            else:
                report.verification.append(f"{cvar} = {value}")
        except Exception as e:  # noqa: BLE001
            report.note(f"{cvar} not readable ({e})")

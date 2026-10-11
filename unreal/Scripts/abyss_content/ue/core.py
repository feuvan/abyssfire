"""Thin, loud wrapper around the editor scripting API used by every build step (ue58-platform.md 11.1).

Every property goes through `set_editor_property` (fires PostEditChangeProperty); a wrong property or enum name raises
BuildError with the asset and property in the message. Names were checked against the UE 5.6 Python API stubs; 5.8.3
changes surface here first, with a clear message.
"""
from __future__ import annotations

import hashlib
import math
import os
import platform
from pathlib import Path
from typing import Any, Iterable

import unreal

from .. import paths

PREFIX = "[build_content]"

# Every AbyssContentBuildLibrary function the scripts call (Python names of the UFUNCTIONs in
# Source/Abyssfire/Public/ContentBuild/AbyssContentBuildLibrary.h). Editor.helper() only hands out these; an editor
# binary built before one of them was added is reported once at the start of the build (stale_editor_warning).
CONTENT_LIB_FUNCTIONS = (
    "configure_skeletal_mesh_socket", "mesh_has_bone", "finish_compilation",
    "set_skeletal_mesh_lod_screen_size", "get_skeletal_mesh_lod_screen_size",
    "set_material_instance_texture_parameter", "set_material_instance_scalar_parameter",
    "set_material_instance_vector_parameter", "get_texture_parameter_infos", "get_material_compile_errors",
)
# AF_CONTENT_NULLRHI=1 / 0 overrides the NullRHI detection (shaders_disabled).
NULLRHI_ENV = "AF_CONTENT_NULLRHI"


class BuildError(RuntimeError):
    pass


def log(msg: str) -> None:
    unreal.log(f"{PREFIX} {msg}")


def warn(msg: str) -> None:
    unreal.log_warning(f"{PREFIX} {msg}")


def error(msg: str) -> None:
    unreal.log_error(f"{PREFIX} {msg}")


def enum(enum_name: str, member: str) -> Any:
    owner = getattr(unreal, enum_name, None)
    if owner is None or not hasattr(owner, member):
        raise BuildError(f"unreal.{enum_name}.{member} does not exist in this engine version")
    return getattr(owner, member)


def has_enum(enum_name: str, member: str) -> bool:
    owner = getattr(unreal, enum_name, None)
    return owner is not None and hasattr(owner, member)


def linear_color(rgba: Iterable[float]) -> unreal.LinearColor:
    v = list(rgba) + [1.0] * 4
    return unreal.LinearColor(float(v[0]), float(v[1]), float(v[2]), float(v[3]))


def file_sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def fingerprint(files: Iterable[Path], salt: str = "") -> str:
    """Identity of an import: source bytes + import settings version (+ caller salt)."""
    h = hashlib.sha256()
    h.update(paths.IMPORT_SETTINGS_VERSION.encode())
    h.update(salt.encode())
    for f in files:
        h.update(f.name.encode())
        h.update(file_sha256(f).encode())
    return h.hexdigest()


def _same(a: Any, b: Any) -> bool:
    if isinstance(a, float) or isinstance(b, float):
        try:
            return math.isclose(float(a), float(b), rel_tol=1e-6, abs_tol=1e-6)
        except (TypeError, ValueError):
            return False
    try:
        return bool(a == b)
    except Exception:  # noqa: BLE001 - some wrapped types do not compare
        return False


def set_props(obj: unreal.Object, props: dict[str, Any], subject: str = "", required: bool = True,
              notes: list[str] | None = None) -> bool:
    """set_editor_property for each entry that differs. Returns True when anything changed."""
    changed = False
    who = subject or obj.get_name()
    for name, value in props.items():
        try:
            current = obj.get_editor_property(name)
        except Exception as e:  # noqa: BLE001
            if required:
                raise BuildError(f"{who}: no property {name!r} ({e})") from e
            if notes is not None:
                notes.append(f"{who}: property {name} not available ({e})")
            continue
        if _same(current, value):
            continue
        try:
            obj.set_editor_property(name, value)
            changed = True
        except Exception as e:  # noqa: BLE001
            if required:
                raise BuildError(f"{who}: cannot set {name} = {value!r} ({e})") from e
            if notes is not None:
                notes.append(f"{who}: {name} not set ({e})")
    return changed


class Editor:
    """Editor subsystems used by the build, resolved once."""

    def __init__(self) -> None:
        self.assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
        if self.assets is None:
            raise BuildError("EditorAssetSubsystem unavailable: run inside the editor (-run=pythonscript)")
        self.tools = unreal.AssetToolsHelpers.get_asset_tools()
        self.mel = unreal.MaterialEditingLibrary
        self.skm = self._subsystem("SkeletalMeshEditorSubsystem")
        self.smes = self._subsystem("StaticMeshEditorSubsystem")
        self.levels = self._subsystem("LevelEditorSubsystem")
        self.actors = self._subsystem("EditorActorSubsystem")
        self.unreal_editor = self._subsystem("UnrealEditorSubsystem")
        # Optional C++ helpers (Source/Abyssfire/Public/ContentBuild/AbyssContentBuildLibrary.h); None when the editor
        # target was built without them, and missing_helpers lists what an older editor binary lacks.
        self.content_lib = getattr(unreal, "AbyssContentBuildLibrary", None)
        self.missing_helpers = [f for f in CONTENT_LIB_FUNCTIONS
                                if self.content_lib is None or not hasattr(self.content_lib, f)]
        self.interchange = None
        manager_cls = getattr(unreal, "InterchangeManager", None)
        if manager_cls is not None:
            try:
                self.interchange = manager_cls.get_interchange_manager_scripted()
            except Exception:  # noqa: BLE001
                self.interchange = None

    def helper(self, name: str) -> Any:
        """AbyssContentBuildLibrary.<name>, or None when this editor binary lacks it (reported once at the start)."""
        if name not in CONTENT_LIB_FUNCTIONS:
            raise BuildError(f"AbyssContentBuildLibrary.{name} is not listed in core.CONTENT_LIB_FUNCTIONS")
        return getattr(self.content_lib, name, None) if self.content_lib is not None else None

    @staticmethod
    def _subsystem(name: str) -> Any:
        cls = getattr(unreal, name, None)
        if cls is None:
            return None
        try:
            return unreal.get_editor_subsystem(cls)
        except Exception:  # noqa: BLE001
            return None

    # ---- assets ----
    def exists(self, path: str) -> bool:
        return bool(self.assets.does_asset_exist(path))

    def load(self, path: str) -> Any:
        return self.assets.load_asset(path) if self.exists(path) else None

    def ensure_dir(self, folder: str) -> None:
        if not self.assets.does_directory_exist(folder):
            self.assets.make_directory(folder)

    def save(self, obj: unreal.Object, force: bool = False) -> None:
        if not self.assets.save_loaded_asset(obj, not force):
            # save_loaded_asset returns False when nothing was dirty (only_if_is_dirty) too; a forced save must succeed.
            if force:
                raise BuildError(f"could not save {obj.get_path_name()}")

    def delete(self, path: str) -> bool:
        return bool(self.assets.delete_asset(path)) if self.exists(path) else False

    def rename(self, src: str, dst: str) -> None:
        if not self.assets.rename_asset(src, dst):
            raise BuildError(f"could not rename {src} -> {dst}")

    def get_tag(self, obj: unreal.Object, tag: str) -> str:
        try:
            return str(self.assets.get_metadata_tag(obj, tag) or "")
        except Exception:  # noqa: BLE001
            return ""

    def set_tag(self, obj: unreal.Object, tag: str, value: str) -> None:
        if self.get_tag(obj, tag) != value:
            self.assets.set_metadata_tag(obj, tag, value)

    def create(self, name: str, folder: str, cls: type, factory: Any) -> tuple[Any, bool]:
        """Existing asset of the class, or a new one. Returns (asset, created)."""
        path = paths.content_path(folder, name)
        if self.exists(path):
            obj = self.assets.load_asset(path)
            if obj is None or not isinstance(obj, cls):
                raise BuildError(f"{path} exists but is a {type(obj).__name__ if obj else 'unloadable asset'}, "
                                 f"not a {cls.__name__}: delete it and re-run")
            return obj, False
        self.ensure_dir(folder)
        obj = self.tools.create_asset(name, folder, cls, factory)
        if obj is None:
            raise BuildError(f"could not create {cls.__name__} {path}")
        return obj, True

    # ---- imports ----
    def import_file(self, source: Path, folder: str, options: Any = None, factory: Any = None) -> list[Any]:
        """Synchronous AssetImportTask (Interchange for FBX / PNG, ue58-platform.md 11.3). Returns the imported objects."""
        if not source.is_file():
            raise BuildError(f"source missing: {source}")
        self.ensure_dir(folder)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", folder)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        for prop, value in (("replace_existing_settings", True), ("async_", False)):
            try:
                task.set_editor_property(prop, value)
            except Exception:  # noqa: BLE001 - property absent in this engine version
                pass
        if options is not None:
            task.set_editor_property("options", options)
        if factory is not None:
            task.set_editor_property("factory", factory)
        self.tools.import_asset_tasks([task])
        self.wait_for_interchange()
        objects: list[Any] = []
        for p in list(task.get_editor_property("imported_object_paths") or []):
            obj = unreal.load_object(None, str(p)) if "." in str(p) else self.assets.load_asset(str(p))
            if obj is not None and obj not in objects:
                objects.append(obj)
        if not objects:
            try:
                objects = [o for o in (task.get_objects() or []) if o is not None]
            except Exception:  # noqa: BLE001
                objects = []
        if not objects:
            raise BuildError(f"import produced no asset: {source} -> {folder} (see the Interchange messages in the log)")
        return objects

    def wait_for_interchange(self) -> None:
        if self.interchange is not None:
            try:
                self.interchange.wait_until_all_tasks_done(False)
            except Exception:  # noqa: BLE001
                pass

    def finish_compilation(self) -> bool:
        """Blocks until async mesh / texture / shader builds finish (C++ helper); False when the helper is missing."""
        self.wait_for_interchange()
        finish = self.helper("finish_compilation")
        if finish is not None:
            finish()
            return True
        return False


def rebuild_command() -> str:
    """The command that rebuilds this project's editor target (AbyssCore + Abyssfire, including the C++ helper)."""
    project = paths.UNREAL_DIR / "Abyssfire.uproject"
    try:
        engine = str(unreal.Paths.convert_relative_path_to_full(unreal.Paths.engine_dir())).rstrip("/\\")
    except Exception:  # noqa: BLE001 - only names the path in a message
        engine = "$UE_ROOT/Engine"
    system = platform.system()
    if system == "Windows":
        return (f'"{engine}\\Build\\BatchFiles\\Build.bat" AbyssfireEditor Win64 Development -project="{project}" '
                "-waitmutex")
    host = "Mac" if system == "Darwin" else "Linux"
    return (f'"{engine}/Build/BatchFiles/{host}/Build.sh" AbyssfireEditor {host} Development -project="{project}" '
            "-waitmutex")


def stale_editor_warning(ed: Editor) -> str | None:
    """One warning for an editor binary that lacks C++ helpers the scripts call (built before they were added, e.g. run
    with SKIP_EDITOR_BUILD=1 after a pull), naming them and the rebuild command; None when everything is there."""
    if not ed.missing_helpers:
        return None
    if ed.content_lib is None:
        what = "AbyssContentBuildLibrary (C++ helper) is not loaded at all"
    else:
        what = ("the loaded AbyssfireEditor binary predates these AbyssContentBuildLibrary functions: "
                + ", ".join(ed.missing_helpers))
    return (f"STALE EDITOR BINARY: {what}. The build goes on without them (LOD screen sizes, skeletal sockets, "
            "material parameter fallbacks, shader waits and compile-error checks that need them are skipped; one note "
            f"each in 'Engine compatibility'). Rebuild the editor target: {rebuild_command()} (build_content.sh does "
            "it unless SKIP_EDITOR_BUILD is set and the binary is newer than Source/)")


def shaders_disabled() -> bool:
    """True when this editor session compiles no shaders, so material statistics read 0: a commandlet (-run=...) uses
    the NullRHI unless it was started with -AllowCommandletRendering (build_content.sh --shaders), and -nullrhi forces
    it. AF_CONTENT_NULLRHI=1 / 0 overrides the detection."""
    override = os.environ.get(NULLRHI_ENV, "").strip().lower()
    if override in ("1", "true", "yes", "on"):
        return True
    if override in ("0", "false", "no", "off"):
        return False
    try:
        cmd = str(unreal.SystemLibrary.get_command_line()).lower()
    except Exception:  # noqa: BLE001 - unknown: assume a rendering editor (statistics are read and judged)
        return False
    if "-nullrhi" in cmd:
        return True
    return "-run=" in cmd and "-allowcommandletrendering" not in cmd

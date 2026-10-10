"""In-memory stand-in for the editor's `unreal` module: just enough behaviour for tests/test_flow.py to run the whole
content build (every step, twice) and catch flow / logic errors in abyss_content.ue without Unreal.

It does not validate engine API names (tests/test_api_names.py does that against a generated stub) and it is
deliberately permissive about property names. Imports are simulated from the file names: *.png -> Texture2D,
SK_*.fbx -> SkeletalMesh (+ a skeleton unless one is passed), A_*.fbx -> AnimSequence, SM_*.fbx -> StaticMesh,
*.ogg -> SoundWave.
"""
from __future__ import annotations

import copy
import json
import types
from pathlib import Path
from typing import Any

MOD = types.ModuleType("unreal")
LOG: list[tuple[str, str]] = []
REGISTRY: dict[str, "Object"] = {}       # package path -> asset
DIRS: set[str] = set()
STATE: dict[str, Any] = {"content_dir": None, "unreal_dir": None, "anim_meta": {}, "imports": 0, "compiles": 0}


def reset(content_dir: Path, unreal_dir: Path, anim_meta: dict[str, tuple[float, int]]) -> None:
    REGISTRY.clear()
    DIRS.clear()
    LOG.clear()
    STATE.update(content_dir=content_dir, unreal_dir=unreal_dir, anim_meta=anim_meta, imports=0, compiles=0)


def _pkg(path: str) -> str:
    path = str(path)
    if "." in path.rsplit("/", 1)[-1]:
        path = path.rsplit(".", 1)[0]
    return path.rstrip("/")


# ---------------------------------------------------------------------------------------------------------------------
# Value types
# ---------------------------------------------------------------------------------------------------------------------
class _Struct:
    _fields: tuple[str, ...] = ()

    def get_editor_property(self, name: str) -> Any:
        return getattr(self, name)

    def set_editor_property(self, name: str, value: Any) -> None:
        setattr(self, name, value)

    def __eq__(self, other: Any) -> bool:
        return type(self) is type(other) and all(getattr(self, f) == getattr(other, f) for f in self._fields)

    def __hash__(self) -> int:
        return hash(tuple(getattr(self, f) for f in self._fields))

    def __repr__(self) -> str:
        return f"{type(self).__name__}({', '.join(f'{f}={getattr(self, f)!r}' for f in self._fields)})"


class LinearColor(_Struct):
    _fields = ("r", "g", "b", "a")

    def __init__(self, r: float = 0.0, g: float = 0.0, b: float = 0.0, a: float = 1.0) -> None:
        self.r, self.g, self.b, self.a = float(r), float(g), float(b), float(a)


class Vector(_Struct):
    _fields = ("x", "y", "z")

    def __init__(self, x: float = 0.0, y: float = 0.0, z: float = 0.0) -> None:
        self.x, self.y, self.z = float(x), float(y), float(z)


class Vector2D(_Struct):
    _fields = ("x", "y")

    def __init__(self, x: float = 0.0, y: float = 0.0) -> None:
        self.x, self.y = float(x), float(y)


class Rotator(_Struct):
    _fields = ("roll", "pitch", "yaw")

    def __init__(self, roll: float = 0.0, pitch: float = 0.0, yaw: float = 0.0) -> None:
        self.roll, self.pitch, self.yaw = float(roll), float(pitch), float(yaw)


class Box(_Struct):
    _fields = ("min", "max")

    def __init__(self, lo: Vector, hi: Vector) -> None:
        self.min, self.max = lo, hi


class PerPlatformFloat(_Struct):
    _fields = ("default",)

    def __init__(self, default: float = 0.0) -> None:
        self.default = float(default)


class SoftObjectPath(_Struct):
    _fields = ("path",)

    def __init__(self, path: str = "") -> None:
        self.path = path

    def export_text(self) -> str:
        return self.path


# ---------------------------------------------------------------------------------------------------------------------
# Objects
# ---------------------------------------------------------------------------------------------------------------------
class Object:
    _defaults: dict[str, Any] = {}

    def __init__(self, outer: Any = None, name: str = "") -> None:
        self._props: dict[str, Any] = {}
        self._name = name or type(self).__name__
        self._package = ""
        self._outer = outer
        self._tags: dict[str, str] = {}

    def get_editor_property(self, name: str) -> Any:
        if name in self._props:
            return self._props[name]
        for cls in type(self).__mro__:
            d = cls.__dict__.get("_defaults", {})
            if name in d:
                value = copy.deepcopy(d[name])
                self._props[name] = value
                return value
        return None

    def set_editor_property(self, name: str, value: Any) -> None:
        self._props[name] = value

    def get_name(self) -> str:
        return self._name

    def get_path_name(self) -> str:
        return f"{self._package}.{self._name}" if self._package else f"/Transient/{self._name}"

    def get_class(self) -> Any:
        return types.SimpleNamespace(get_name=lambda: type(self).__name__)

    def __repr__(self) -> str:
        return f"<{type(self).__name__} {self.get_path_name()}>"


def register(obj: Object, package: str) -> Object:
    package = _pkg(package)
    obj._package = package
    obj._name = package.rsplit("/", 1)[-1]
    REGISTRY[package] = obj
    DIRS.add(package.rsplit("/", 1)[0])
    return obj


class MaterialInterface(Object):
    pass


class Material(MaterialInterface):
    def __init__(self, *a: Any, **k: Any) -> None:
        super().__init__(*a, **k)
        self._expressions: list[Any] = []
        self._outputs: dict[str, Any] = {}


class MaterialInstanceConstant(MaterialInterface):
    def __init__(self, *a: Any, **k: Any) -> None:
        super().__init__(*a, **k)
        self._params: dict[str, Any] = {}


class MaterialParameterCollection(Object):
    _defaults = {"vector_parameters": [], "scalar_parameters": []}


class CollectionVectorParameter(Object):
    _defaults = {"parameter_name": "", "default_value": LinearColor(0, 0, 0, 0)}


class CollectionScalarParameter(Object):
    _defaults = {"parameter_name": "", "default_value": 0.0}


class CustomInput(Object):
    _defaults = {"input_name": ""}


class Texture(Object):
    pass


class Texture2D(Texture):
    def blueprint_get_size_x(self) -> int:
        return 256

    def blueprint_get_size_y(self) -> int:
        return 256


class Skeleton(Object):
    pass


class PhysicsAsset(Object):
    pass


class SoundWave(Object):
    _defaults = {"duration": 1.0, "num_channels": 1, "looping": False}


class World(Object):
    def __init__(self, *a: Any, **k: Any) -> None:
        super().__init__(*a, **k)
        self._settings = Object(name="WorldSettings")

    def get_world_settings(self) -> Object:
        return self._settings


class SkeletalMaterial(Object):
    _defaults = {"material_slot_name": "", "material_interface": None}


class StaticMaterial(Object):
    _defaults = {"material_slot_name": "", "material_interface": None}


class SkeletalMeshLODInfo(Object):
    _defaults = {"screen_size": PerPlatformFloat(1.0)}


class SkeletalMeshSocket(Object):
    _defaults = {"socket_name": "None", "bone_name": "None", "relative_location": Vector(),
                 "relative_rotation": Rotator()}


class StaticMeshSocket(Object):
    _defaults = {"socket_name": "None", "relative_location": Vector(), "relative_rotation": Rotator()}


class SkeletalMesh(Object):
    BONES = {"root", "pelvis", "spine_01", "spine_02", "spine_03", "neck_01", "head", "hand_l", "hand_r", "weapon_l",
             "weapon_r"}

    def __init__(self, *a: Any, **k: Any) -> None:
        super().__init__(*a, **k)
        self._sockets: list[SkeletalMeshSocket] = []
        self._shadow: dict[tuple[int, int], bool] = {}

    def find_socket_and_index(self, name: str) -> tuple[Any, int]:
        for i, s in enumerate(self._sockets):
            if str(s.get_editor_property("socket_name")) == str(name):
                return s, i
        return None, -1

    def add_socket(self, socket: SkeletalMeshSocket, add_to_skeleton: bool = False) -> None:
        self._sockets.append(socket)


class StaticMesh(Object):
    def __init__(self, *a: Any, **k: Any) -> None:
        super().__init__(*a, **k)
        self._materials: list[Any] = [None, None]
        self._sockets: list[StaticMeshSocket] = []
        self._bounds = (Vector(-50, -50, 0), Vector(50, 50, 0))
        self._shadow: dict[tuple[int, int], bool] = {}

    def set_material(self, index: int, material: Any) -> None:
        while len(self._materials) <= index:
            self._materials.append(None)
        self._materials[index] = material

    def get_editor_property(self, name: str) -> Any:
        if name == "static_materials":
            # Interchange slot order is not guaranteed: the fake reverses it to exercise the by-name matching.
            names = ["M_AF_Outline", "MI_AF_Toon_Heroes"] if len(self._materials) == 2 else ["Sprite"]
            out = []
            for n in names[:len(self._materials)]:
                m = StaticMaterial()
                m._props["material_slot_name"] = n
                out.append(m)
            return out
        return super().get_editor_property(name)

    def get_material(self, index: int) -> Any:
        return self._materials[index] if index < len(self._materials) else None

    def get_num_sections(self, lod: int) -> int:
        return len(self._materials)

    def find_socket(self, name: str) -> Any:
        for s in self._sockets:
            if str(s.get_editor_property("socket_name")) == str(name):
                return s
        return None

    def add_socket(self, socket: StaticMeshSocket) -> None:
        self._sockets.append(socket)

    def get_bounding_box(self) -> Box:
        return Box(*self._bounds)

    @classmethod
    def create_static_mesh_description(cls, outer: Any = None) -> "StaticMeshDescription":
        return StaticMeshDescription()

    def build_from_static_mesh_descriptions(self, descs: list[Any], simple_collision: bool = False,
                                            fast_build: bool = True) -> None:
        pts = [p for d in descs for p in d.positions.values()]
        self._bounds = (Vector(min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)),
                        Vector(max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
        self._materials = [None]


class _Id:
    def __init__(self, value: int) -> None:
        self.id_value = value


class StaticMeshDescription(Object):
    def __init__(self) -> None:
        super().__init__()
        self.positions: dict[int, Vector] = {}
        self.n = 0

    def _new(self) -> _Id:
        self.n += 1
        return _Id(self.n)

    def create_polygon_group(self) -> _Id:
        return self._new()

    def set_polygon_group_material_slot_name(self, group: _Id, name: str) -> None:
        pass

    def create_vertex(self) -> _Id:
        return self._new()

    def set_vertex_position(self, vid: _Id, pos: Vector) -> None:
        self.positions[vid.id_value] = pos

    def create_vertex_instance(self, vid: _Id) -> _Id:
        return self._new()

    def set_vertex_instance_uv(self, vi: _Id, uv: Vector2D, index: int = 0) -> None:
        pass

    def create_triangle(self, group: _Id, instances: list[_Id]) -> tuple[_Id, list[_Id]]:
        assert len(instances) == 3
        return self._new(), []


class AnimSequence(Object):
    _defaults = {"loop": False, "enable_root_motion": False, "additive_anim_type": "AdditiveAnimationType.AAT_NONE",
                 "ref_pose_type": "AdditiveBasePoseType.ABPT_REF_POSE", "ref_frame_index": 0, "ref_pose_seq": None}


class AssetImportTask(Object):
    _defaults = {"imported_object_paths": []}

    def get_objects(self) -> list[Any]:
        return [REGISTRY[_pkg(p)] for p in self.get_editor_property("imported_object_paths")]


class _Pipeline(Object):
    pass


class InterchangeGenericAssetsPipeline(_Pipeline):
    def __init__(self, *a: Any, **k: Any) -> None:
        super().__init__(*a, **k)
        for sub in ("common_meshes_properties", "common_skeletal_meshes_and_animations_properties", "mesh_pipeline",
                    "animation_pipeline", "material_pipeline"):
            self._props[sub] = _Pipeline(name=sub)
        self._props["material_pipeline"]._props["texture_pipeline"] = _Pipeline(name="texture_pipeline")


class InterchangePipelineStackOverride(Object):
    def __init__(self, *a: Any, **k: Any) -> None:
        super().__init__(*a, **k)
        self.pipelines: list[Any] = []

    def add_pipeline(self, p: Any) -> None:
        self.pipelines.append(p)


# ---------------------------------------------------------------------------------------------------------------------
# Material expressions
# ---------------------------------------------------------------------------------------------------------------------
EXPRESSION_INPUTS = {
    "MaterialExpressionTextureSampleParameter2D": ["UVs", "Tex", "Apply View MipBias"],
    "MaterialExpressionAppendVector": ["A", "B"],
    "MaterialExpressionAdd": ["A", "B"],
    "MaterialExpressionQualitySwitch": ["Default", "Low", "High", "Medium", "Epic"],
    "MaterialExpressionVertexInterpolator": ["VS"],
    "MaterialExpressionTransform": [""],
    "MaterialExpressionDepthFade": ["Opacity", "FadeDistance"],
}


class MaterialExpression(Object):
    def input_names(self) -> list[str]:
        cls = type(self).__name__
        if cls == "MaterialExpressionCustom":
            return [str(i.get_editor_property("input_name")) for i in (self.get_editor_property("inputs") or [])]
        return list(EXPRESSION_INPUTS.get(cls, []))


class MaterialEditingLibrary:
    @staticmethod
    def delete_all_material_expressions(material: Material) -> None:
        material._expressions.clear()
        material._outputs.clear()

    @staticmethod
    def create_material_expression(material: Material, cls: type, x: int = 0, y: int = 0) -> Any:
        expr = cls(material)
        material._expressions.append(expr)
        return expr

    @staticmethod
    def get_material_expression_input_names(expr: MaterialExpression) -> list[str]:
        return expr.input_names()

    @staticmethod
    def connect_material_expressions(src: Any, out: str, dst: Any, inp: str) -> bool:
        if inp not in dst.input_names():
            return False
        dst._props.setdefault("_links", {})[inp] = (src, out)
        return True

    @staticmethod
    def connect_material_property(src: Any, out: str, prop: Any) -> bool:
        src._outer._outputs[str(prop)] = src
        return True

    @staticmethod
    def layout_material_expressions(material: Material) -> None:
        pass

    @staticmethod
    def recompile_material(material: Material) -> None:
        STATE["compiles"] += 1

    @staticmethod
    def set_material_instance_parent(mi: MaterialInstanceConstant, parent: Any) -> None:
        mi.set_editor_property("parent", parent)

    @staticmethod
    def get_material_instance_texture_parameter_value(mi: Any, name: str) -> Any:
        return mi._params.get(("t", name))

    @staticmethod
    def set_material_instance_texture_parameter_value(mi: Any, name: str, value: Any) -> bool:
        mi._params[("t", name)] = value
        return True

    @staticmethod
    def get_material_instance_scalar_parameter_value(mi: Any, name: str) -> float:
        return mi._params.get(("s", name), 0.0)

    @staticmethod
    def set_material_instance_scalar_parameter_value(mi: Any, name: str, value: float) -> bool:
        mi._params[("s", name)] = value
        return True

    @staticmethod
    def get_material_instance_vector_parameter_value(mi: Any, name: str) -> LinearColor:
        return mi._params.get(("v", name), LinearColor(0, 0, 0, 0))

    @staticmethod
    def set_material_instance_vector_parameter_value(mi: Any, name: str, value: LinearColor) -> bool:
        mi._params[("v", name)] = value
        return True

    @staticmethod
    def update_material_instance(mi: Any) -> None:
        pass

    @staticmethod
    def get_statistics(material: Any) -> Object:
        st = Object()
        for k in ("num_vertex_shader_instructions", "num_pixel_shader_instructions", "num_samplers"):
            st._props[k] = 42
        return st


# ---------------------------------------------------------------------------------------------------------------------
# Subsystems / libraries
# ---------------------------------------------------------------------------------------------------------------------
class EditorAssetSubsystem:
    def does_asset_exist(self, path: str) -> bool:
        return _pkg(path) in REGISTRY

    def load_asset(self, path: str) -> Any:
        return REGISTRY.get(_pkg(path))

    def does_directory_exist(self, path: str) -> bool:
        return path.rstrip("/") in DIRS

    def make_directory(self, path: str) -> bool:
        DIRS.add(path.rstrip("/"))
        return True

    def save_loaded_asset(self, obj: Any, only_if_is_dirty: bool = True) -> bool:
        return True

    def save_directory(self, path: str, only_if_is_dirty: bool = True, recursive: bool = True) -> bool:
        return True

    def rename_asset(self, src: str, dst: str) -> bool:
        obj = REGISTRY.pop(_pkg(src), None)
        if obj is None or _pkg(dst) in REGISTRY:
            return False
        register(obj, dst)
        return True

    def delete_asset(self, path: str) -> bool:
        return REGISTRY.pop(_pkg(path), None) is not None

    def get_metadata_tag(self, obj: Object, tag: str) -> str:
        return obj._tags.get(tag, "")

    def set_metadata_tag(self, obj: Object, tag: str, value: str) -> None:
        obj._tags[tag] = value

    def list_assets(self, directory: str, recursive: bool = True, include_folder: bool = False) -> list[str]:
        return [p for p in REGISTRY if p.startswith(directory.rstrip("/") + "/")]

    def duplicate_asset(self, src: str, dst: str) -> Any:
        if src != "/Engine/BasicShapes/Plane":
            return None
        return register(StaticMesh(), dst)


class AssetTools:
    def create_asset(self, name: str, folder: str, cls: type, factory: Any) -> Any:
        return register(cls(), f"{folder}/{name}")

    def import_asset_tasks(self, tasks: list[AssetImportTask]) -> None:
        for task in tasks:
            STATE["imports"] += 1
            src = Path(task.get_editor_property("filename"))
            dest = task.get_editor_property("destination_path")
            stem = src.stem
            created: list[Object] = []
            options = task.get_editor_property("options")
            pipeline = options.pipelines[0] if options is not None else None
            if src.suffix.lower() == ".png":
                created.append(register(REGISTRY.get(f"{dest}/{stem}") or Texture2D(), f"{dest}/{stem}"))
            elif src.suffix.lower() == ".ogg":
                created.append(register(REGISTRY.get(f"{dest}/{stem}") or SoundWave(), f"{dest}/{stem}"))
            elif src.suffix.lower() == ".fbx":
                common = pipeline.get_editor_property("common_skeletal_meshes_and_animations_properties")
                mesh_p = pipeline.get_editor_property("mesh_pipeline")
                skeleton = common.get_editor_property("skeleton")
                if common.get_editor_property("import_only_animations"):
                    seq = REGISTRY.get(f"{dest}/{stem}") or AnimSequence()
                    register(seq, f"{dest}/{stem}")
                    seq._props["skeleton"] = skeleton
                    length_ms, frames = STATE["anim_meta"].get(stem, (1000.0, 60))
                    seq._props["sequence_length"] = length_ms / 1000.0
                    seq._frames = frames
                    created.append(seq)
                elif mesh_p.get_editor_property("import_skeletal_meshes"):
                    mesh = REGISTRY.get(f"{dest}/{stem}") or SkeletalMesh()
                    register(mesh, f"{dest}/{stem}")
                    if skeleton is None:
                        skeleton = register(Skeleton(), f"{dest}/{stem}_Skeleton")
                        created.append(skeleton)
                    mesh._props["skeleton"] = skeleton
                    mesh._props["materials"] = [SkeletalMaterial(), SkeletalMaterial()]
                    mesh._props["materials"][0]._props["material_slot_name"] = "MI_AF_Toon_Heroes"
                    mesh._props["materials"][1]._props["material_slot_name"] = "M_AF_Outline"
                    mesh._props["lod_info"] = [SkeletalMeshLODInfo()]
                    created.insert(0, mesh)
                elif mesh_p.get_editor_property("import_static_meshes"):
                    mesh = REGISTRY.get(f"{dest}/{stem}") or StaticMesh()
                    register(mesh, f"{dest}/{stem}")
                    created.append(mesh)
            task.set_editor_property("imported_object_paths", [o.get_path_name() for o in created])


class SkeletalMeshEditorSubsystem:
    def import_lod(self, mesh: SkeletalMesh, index: int, filename: str) -> int:
        infos = mesh.get_editor_property("lod_info")
        while len(infos) <= index:
            infos.append(SkeletalMeshLODInfo())
        return index

    def get_lod_count(self, mesh: SkeletalMesh) -> int:
        return len(mesh.get_editor_property("lod_info") or [])

    def get_num_sections(self, mesh: SkeletalMesh, lod: int) -> int:
        return 2

    def get_lod_material_slot(self, mesh: SkeletalMesh, lod: int, section: int) -> int:
        return section

    def get_section_cast_shadow(self, mesh: SkeletalMesh, lod: int, section: int) -> bool:
        return mesh._shadow.get((lod, section), True)

    def set_section_cast_shadow(self, mesh: SkeletalMesh, lod: int, section: int, value: bool) -> bool:
        mesh._shadow[(lod, section)] = value
        return True


class StaticMeshEditorSubsystem:
    def get_number_materials(self, mesh: StaticMesh) -> int:
        return len(mesh._materials)

    def get_lod_count(self, mesh: StaticMesh) -> int:
        return 1

    def get_lod_material_slot(self, mesh: StaticMesh, lod: int, section: int) -> int:
        return section

    def is_section_cast_shadow_enabled(self, mesh: StaticMesh, lod: int, section: int) -> bool:
        return mesh._shadow.get((lod, section), True)

    def enable_section_cast_shadow(self, mesh: StaticMesh, value: bool, lod: int, section: int) -> None:
        mesh._shadow[(lod, section)] = value

    def get_simple_collision_count(self, mesh: StaticMesh) -> int:
        return 0

    def remove_collisions(self, mesh: StaticMesh) -> bool:
        return True


class LevelEditorSubsystem:
    def __init__(self) -> None:
        self.current: World | None = None

    def new_level(self, path: str, partitioned: bool = False) -> bool:
        self.current = register(World(), path)  # type: ignore[assignment]
        self._write(path)
        return True

    def load_level(self, path: str) -> bool:
        self.current = REGISTRY.get(_pkg(path))  # type: ignore[assignment]
        return self.current is not None

    def save_current_level(self) -> bool:
        if self.current is None:
            return False
        self._write(self.current._package)
        return True

    @staticmethod
    def _write(package: str) -> None:
        rel = package[len("/Game/"):]
        f = Path(STATE["content_dir"]) / (rel + ".umap")
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_bytes(b"fake umap")


class WorldSettings(Object):
    pass


class EditorActorSubsystem:
    def get_all_level_actors(self) -> list[Any]:
        return [WorldSettings()]


class UnrealEditorSubsystem:
    def get_editor_world(self) -> Any:
        return SUBSYSTEMS["LevelEditorSubsystem"].current


SUBSYSTEMS: dict[str, Any] = {
    "EditorAssetSubsystem": EditorAssetSubsystem(),
    "SkeletalMeshEditorSubsystem": SkeletalMeshEditorSubsystem(),
    "StaticMeshEditorSubsystem": StaticMeshEditorSubsystem(),
    "LevelEditorSubsystem": LevelEditorSubsystem(),
    "EditorActorSubsystem": EditorActorSubsystem(),
    "UnrealEditorSubsystem": UnrealEditorSubsystem(),
}


def get_editor_subsystem(cls: type) -> Any:
    return SUBSYSTEMS.get(cls.__name__)


class AssetToolsHelpers:
    _tools = AssetTools()

    @classmethod
    def get_asset_tools(cls) -> AssetTools:
        return cls._tools


class InterchangeManager:
    @classmethod
    def get_interchange_manager_scripted(cls) -> "InterchangeManager":
        return cls()

    def wait_until_all_tasks_done(self, cancel: bool) -> None:
        pass


class SystemLibrary:
    @staticmethod
    def get_engine_version() -> str:
        return "5.8.3-fake"

    @staticmethod
    def get_console_variable_bool_value(name: str) -> bool:
        return False

    @staticmethod
    def get_console_variable_int_value(name: str) -> int:
        return 0


class GameMapsSettings(Object):
    @classmethod
    def get_game_maps_settings(cls) -> "GameMapsSettings":
        s = cls()
        s._props.update(
            game_default_map=SoftObjectPath("/Game/Abyssfire/Maps/L_Main.L_Main"),
            editor_startup_map=SoftObjectPath("/Game/Abyssfire/Maps/L_Main.L_Main"),
            global_default_game_mode=SoftObjectPath("/Script/Abyssfire.AbyssGameMode"),
            game_instance_class=SoftObjectPath("/Script/Abyssfire.AbyssGameInstance"),
        )
        return s


class EditorLoadingAndSavingUtils:
    @staticmethod
    def new_blank_map(save_existing: bool) -> Any:
        return None

    @staticmethod
    def save_map(world: Any, path: str) -> bool:
        return False


class AnimationLibrary:
    @staticmethod
    def get_num_frames(seq: AnimSequence) -> int:
        return getattr(seq, "_frames", 0)

    @staticmethod
    def get_sequence_length(seq: AnimSequence) -> float:
        return float(seq.get_editor_property("sequence_length") or 0.0)


class AbyssContentBuildLibrary:
    @staticmethod
    def configure_skeletal_mesh_socket(mesh: SkeletalMesh, socket: SkeletalMeshSocket, name: str, bone: str) -> bool:
        if bone not in SkeletalMesh.BONES:
            return False
        socket.set_editor_property("socket_name", name)
        socket.set_editor_property("bone_name", bone)
        return True

    @staticmethod
    def mesh_has_bone(mesh: SkeletalMesh, bone: str) -> bool:
        return bone in SkeletalMesh.BONES

    @staticmethod
    def finish_compilation() -> None:
        pass


class ScopedSlowTask:
    def __init__(self, work: float, desc: str = "", enabled: bool = True) -> None:
        pass

    def __enter__(self) -> "ScopedSlowTask":
        return self

    def __exit__(self, *exc: Any) -> bool:
        return False

    def make_dialog(self, can_cancel: bool = False) -> None:
        pass

    def should_cancel(self) -> bool:
        return False

    def enter_progress_frame(self, work: float = 1.0, desc: str = "") -> None:
        pass


class Paths:
    @staticmethod
    def project_dir() -> str:
        return str(STATE["unreal_dir"]) + "/"

    @staticmethod
    def convert_relative_path_to_full(path: str, base: str = "") -> str:
        return path


def load_asset(path: str, type: Any = None, follow_redirectors: bool = True) -> Any:  # noqa: A002
    return REGISTRY.get(_pkg(path))


def load_object(outer: Any, name: str, type: Any = None, follow_redirectors: bool = True) -> Any:  # noqa: A002
    return REGISTRY.get(_pkg(name))


def log(msg: Any) -> None:
    LOG.append(("log", str(msg)))


def log_warning(msg: Any) -> None:
    LOG.append(("warning", str(msg)))


def log_error(msg: Any) -> None:
    LOG.append(("error", str(msg)))


class _EnumMeta(type):
    def __getattr__(cls, name: str) -> str:
        if name.startswith("__"):
            raise AttributeError(name)
        return f"{cls.__name__}.{name}"


class _Factory(Object):
    pass


_EXPLICIT = {k: v for k, v in dict(globals()).items() if not k.startswith("_") and k not in (
    "annotations", "copy", "json", "types", "Path", "Any", "MOD", "LOG", "REGISTRY", "DIRS", "STATE", "reset",
    "register", "SUBSYSTEMS", "EXPRESSION_INPUTS")}
_DYNAMIC: dict[str, Any] = {}


def _module_getattr(name: str) -> Any:
    if name in _DYNAMIC:
        return _DYNAMIC[name]
    if name.startswith("__"):
        raise AttributeError(name)
    if name.startswith("MaterialExpression"):
        cls: Any = type(name, (MaterialExpression,), {})
    elif name.endswith("Factory") or name.endswith("FactoryNew"):
        cls = type(name, (_Factory,), {})
    else:
        cls = _EnumMeta(name, (), {})
    _DYNAMIC[name] = cls
    return cls


for _k, _v in _EXPLICIT.items():
    setattr(MOD, _k, _v)
MOD.__getattr__ = _module_getattr  # type: ignore[attr-defined]
MOD.Object = Object
MOD.MaterialInterface = MaterialInterface
_ = json  # keep the import for callers that inspect the module

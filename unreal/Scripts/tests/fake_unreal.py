"""In-memory stand-in for the editor's `unreal` module: just enough behaviour for tests/test_flow.py to run the whole
content build (every step, twice) and catch flow / logic errors in abyss_content.ue without Unreal.

It does not validate engine API names (tests/test_api_names.py does that against a generated stub) and it is
deliberately permissive about property names. Imports are simulated from the file names: *.png -> Texture2D,
SK_*.fbx -> SkeletalMesh (+ a skeleton unless one is passed), A_*.fbx -> AnimSequence, SM_*.fbx -> StaticMesh,
*.ogg -> SoundWave.

It emulates UE 5.8 where the build depends on it: the MaterialEditingLibrary instance-parameter setters apply the value
but return False; the command line is a -run=pythonscript commandlet without -AllowCommandletRendering (NullRHI: no
shader statistics); the Interchange properties 5.8 deprecated or removed raise. Switches (reset() restores 5.8):
STATE["mel_refuse"] (parameters MaterialEditingLibrary does not apply), STATE["command_line"], STATE["compile_errors"],
STATE["interchange_missing"] (INTERCHANGE_58 / INTERCHANGE_56), STATE["custom_output_pins"] (False: Custom nodes keep a
single output pin whatever `additional_outputs` says) and use_content_library("current" | "stale" | "none").
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
COMMANDLET_LINE = '"/p/Abyssfire.uproject" -run=pythonscript -script="build_content.py" -unattended -nosplash -stdout'

# Interchange properties an engine version does not expose: "<pipeline part>.<property>" -> the error reading it raises.
INTERCHANGE_58 = {
    "common_meshes_properties.auto_detect_mesh_type":
        "is deprecated: bAutoDetectMeshType is deprecated. "
        "Use bConvertStaticsWithAnimatedTransformToSkeletals instead.",
    "common_skeletal_meshes_and_animations_properties.convert_statics_with_morph_targets_to_skeletals":
        "is deprecated: bConvertStaticsWithMorphTargetsToSkeletals is deprecated. Use "
        "UInterchangeGenericCommonMeshesProperties::bConvertStaticsWithMorphTargetsToSkeletals instead.",
    "mesh_pipeline.combine_static_meshes":
        "is deprecated: bCombineStaticMeshes is no longer used. Use CombineStaticMeshesBehavior",
    "mesh_pipeline.import_collision": "is deprecated: Use Collision instead.",
    "animation_pipeline.snap_to_closest_frame_boundary": "Failed to find property",
}
INTERCHANGE_56 = {
    "common_meshes_properties.convert_statics_with_animated_transform_to_skeletals": "Failed to find property",
    "common_meshes_properties.convert_statics_with_morph_targets_to_skeletals": "Failed to find property",
    "mesh_pipeline.combine_static_meshes_behavior": "Failed to find property",
    "mesh_pipeline.import_collision": "is deprecated: Use Collision instead.",
}


def reset(content_dir: Path, unreal_dir: Path, anim_meta: dict[str, tuple[float, int]]) -> None:
    REGISTRY.clear()
    DIRS.clear()
    LOG.clear()
    STATE.update(content_dir=content_dir, unreal_dir=unreal_dir, anim_meta=anim_meta, imports=0, compiles=0,
                 mel_refuse=set(), command_line=COMMANDLET_LINE, compile_errors={},
                 interchange_missing=dict(INTERCHANGE_58), saved=[], pipelines={}, custom_output_pins=True)
    use_content_library("current")


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


class Int32Interval(_Struct):
    _fields = ("min", "max")

    def __init__(self, min: int = 0, max: int = 0) -> None:  # noqa: A002 - the engine's keyword names
        self.min, self.max = int(min), int(max)


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
        self._output_pins: dict[str, str] = {}     # material property -> the source's output pin name


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


class CustomOutput(Object):
    _defaults = {"output_name": "", "output_type": None}


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


def fbx_node_names(path: Path, node_class: str) -> list[str]:
    """Names of the FBX binary nodes of one class ("Model" = bones and meshes, "Material"), in file order.

    A binary FBX stores each node name as an `S` string record `<name>\\x00\\x01<class>` behind a little-endian length.
    """
    try:
        data = path.read_bytes()
    except OSError:
        return []
    tag = b"\x00\x01" + node_class.encode()
    out: list[str] = []
    at = data.find(tag)
    while at >= 0:
        for n in range(1, min(at, 128) - 4):
            start = at - n
            if data[start - 5] == ord("S") and int.from_bytes(data[start - 4:start], "little") == n + len(tag):
                name = data[start:at].decode("utf-8", "replace")
                if name not in out:
                    out.append(name)
                break
        at = data.find(tag, at + len(tag))
    return out


class SkeletalMesh(Object):
    # Fallback for meshes whose source FBX cannot be read (each import reads the real bone names from the file).
    BONES = {"root", "pelvis", "spine_01", "spine_02", "spine_03", "neck_01", "head", "hand_l", "hand_r", "weapon_l",
             "weapon_r"}

    def __init__(self, *a: Any, **k: Any) -> None:
        super().__init__(*a, **k)
        self._sockets: list[SkeletalMeshSocket] = []
        self._shadow: dict[tuple[int, int], bool] = {}
        self._bones: set[str] = set(SkeletalMesh.BONES)

    def has_bone(self, bone: str) -> bool:
        return str(bone) in self._bones

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
        self._slot_names: list[str] = ["MI_AF_Toon_Heroes", "M_AF_Outline"]
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
            names = list(reversed(self._slot_names)) if len(self._materials) == len(self._slot_names) else ["Sprite"]
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


class _CombineBehavior:
    """Stand-in for the enum behind 5.8's InterchangeGenericMeshPipeline.combine_static_meshes_behavior."""

    def __init__(self, name: str) -> None:
        self.name = name

    def __repr__(self) -> str:
        return f"CombineBehavior.{self.name}"


_CombineBehavior.DONT_COMBINE = _CombineBehavior("DONT_COMBINE")  # type: ignore[attr-defined]
_CombineBehavior.COMBINE_ALL = _CombineBehavior("COMBINE_ALL")    # type: ignore[attr-defined]


class _Pipeline(Object):
    _defaults = {"combine_static_meshes_behavior": _CombineBehavior.DONT_COMBINE}  # type: ignore[attr-defined]

    def _check(self, name: str) -> None:
        msg = STATE.get("interchange_missing", {}).get(f"{self._name}.{name}")
        if msg is not None:
            raise RuntimeError(f"Property '{name}' on '{self._name}' {msg}")

    def get_editor_property(self, name: str) -> Any:
        self._check(name)
        return super().get_editor_property(name)

    def set_editor_property(self, name: str, value: Any) -> None:
        self._check(name)
        super().set_editor_property(name, value)


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
    "MaterialExpressionMultiply": ["A", "B"],
    "MaterialExpressionComponentMask": [""],
    "MaterialExpressionQualitySwitch": ["Default", "Low", "High", "Medium", "Epic"],
    "MaterialExpressionVertexInterpolator": ["VS"],
    "MaterialExpressionTransform": [""],
    "MaterialExpressionDepthFade": ["Opacity", "FadeDistance"],
    "MaterialExpressionShadowReplace": ["Default", "Shadow"],
}


# Output widths (float components) of the expressions the build wires into Custom nodes, as the engine compiles them:
# output 0 of a vector parameter / texture sample / vertex colour / Constant3Vector is RGB (mask 1,1,1,0), the named
# channel pins are one wide and only "RGBA" is four (UMaterialExpressionVectorParameter / TextureSample outputs). The
# Custom node declares each input with the width its wire compiles to, so a float4 input fed from an RGB pin is a
# float3 in the shader (`.a` fails to compile). None = not modelled (output_width).
_RGB_FIRST = {"MaterialExpressionVectorParameter", "MaterialExpressionTextureSampleParameter2D",
              "MaterialExpressionTextureSample", "MaterialExpressionVertexColor", "MaterialExpressionConstant3Vector"}
_FIXED_WIDTH = {
    "MaterialExpressionScalarParameter": 1, "MaterialExpressionConstant": 1, "MaterialExpressionPerInstanceCustomData": 1,
    "MaterialExpressionTime": 1, "MaterialExpressionDepthFade": 1, "MaterialExpressionDitherTemporalAA": 1,
    "MaterialExpressionTextureCoordinate": 2, "MaterialExpressionWorldPosition": 3,
    "MaterialExpressionVertexNormalWS": 3, "MaterialExpressionPixelNormalWS": 3, "MaterialExpressionCameraVectorWS": 3,
    "MaterialExpressionObjectPositionWS": 3, "MaterialExpressionConstant4Vector": 4,
}
_CUSTOM_WIDTH = {"CMOT_FLOAT1": 1, "CMOT_FLOAT2": 2, "CMOT_FLOAT3": 3, "CMOT_FLOAT4": 4}


def output_width(expr: Any, out: str) -> int | None:
    """The width an output of `expr` compiles to (see _RGB_FIRST); None when the fake does not model it."""
    cls = type(expr).__name__
    links = expr._props.get("_links", {})
    if cls in _RGB_FIRST:
        name = (out or "").upper()
        if name in ("", "RGB"):
            return 3
        if name in ("R", "G", "B", "A"):
            return 1
        if name == "RGBA":
            return 4
        return None
    if cls in _FIXED_WIDTH:
        return _FIXED_WIDTH[cls]
    if cls == "MaterialExpressionComponentMask":
        return sum(1 for c in "rgba" if expr.get_editor_property(c))
    if cls == "MaterialExpressionAppendVector":
        a, b = (output_width(*links[k]) if k in links else None for k in ("A", "B"))
        return a + b if a is not None and b is not None else None
    if cls == "MaterialExpressionVertexInterpolator":
        return output_width(*links["VS"]) if "VS" in links else None
    if cls == "MaterialExpressionCustom":
        if out and out.lower() not in ("return", "output"):
            return None   # an additional output
        return _CUSTOM_WIDTH.get(str(expr.get_editor_property("output_type")).rsplit(".", 1)[-1])
    return None


class MaterialExpression(Object):
    def input_names(self) -> list[str]:
        cls = type(self).__name__
        if cls == "MaterialExpressionCustom":
            return [str(i.get_editor_property("input_name")) for i in (self.get_editor_property("inputs") or [])]
        return list(EXPRESSION_INPUTS.get(cls, []))

    def has_output(self, name: str) -> bool:
        """Custom nodes: UMaterialExpressionCustom::RebuildOutputs (main pin "" / "return", one pin per additional
        output); other expressions accept any name (not modelled)."""
        if type(self).__name__ != "MaterialExpressionCustom" or not name:
            return True
        extra = [str(o.get_editor_property("output_name")) for o in (self.get_editor_property("additional_outputs")
                                                                       or [])]
        if not extra or not STATE.get("custom_output_pins", True):
            return False
        return name.lower() in [n.lower() for n in ["return"] + extra]


class MaterialEditingLibrary:
    @staticmethod
    def delete_all_material_expressions(material: Material) -> None:
        material._expressions.clear()
        material._outputs.clear()
        material._output_pins.clear()

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
        if inp not in dst.input_names() or not src.has_output(out):
            return False
        dst._props.setdefault("_links", {})[inp] = (src, out)
        return True

    @staticmethod
    def connect_material_property(src: Any, out: str, prop: Any) -> bool:
        if not src.has_output(out):
            return False
        src._outer._outputs[str(prop)] = src
        src._outer._output_pins[str(prop)] = out
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
    def _set_param(mi: Any, key: tuple[str, str], value: Any) -> bool:
        # UE 5.8: the value is applied but the call returns False. STATE["mel_refuse"]: names it does not apply at all.
        if key[1] not in STATE.get("mel_refuse", set()):
            mi._params[key] = value
        return False

    @staticmethod
    def set_material_instance_texture_parameter_value(mi: Any, name: str, value: Any) -> bool:
        return MaterialEditingLibrary._set_param(mi, ("t", str(name)), value)

    @staticmethod
    def get_material_instance_scalar_parameter_value(mi: Any, name: str) -> float:
        return mi._params.get(("s", name), 0.0)

    @staticmethod
    def set_material_instance_scalar_parameter_value(mi: Any, name: str, value: float) -> bool:
        return MaterialEditingLibrary._set_param(mi, ("s", str(name)), value)

    @staticmethod
    def get_material_instance_vector_parameter_value(mi: Any, name: str) -> LinearColor:
        return mi._params.get(("v", name), LinearColor(0, 0, 0, 0))

    @staticmethod
    def set_material_instance_vector_parameter_value(mi: Any, name: str, value: LinearColor) -> bool:
        return MaterialEditingLibrary._set_param(mi, ("v", str(name)), value)

    @staticmethod
    def update_material_instance(mi: Any) -> None:
        pass

    @staticmethod
    def _parameter_names(material: Any, prefix: str) -> list[str]:
        names: list[str] = []
        for expr in getattr(material, "_expressions", []):
            if type(expr).__name__.startswith(prefix):
                name = str(expr.get_editor_property("parameter_name") or "")
                if name and name not in names:
                    names.append(name)
        return names

    @staticmethod
    def get_texture_parameter_names(material: Any) -> list[str]:
        return MaterialEditingLibrary._parameter_names(material, "MaterialExpressionTexture")

    @staticmethod
    def get_scalar_parameter_names(material: Any) -> list[str]:
        return MaterialEditingLibrary._parameter_names(material, "MaterialExpressionScalarParameter")

    @staticmethod
    def get_vector_parameter_names(material: Any) -> list[str]:
        return MaterialEditingLibrary._parameter_names(material, "MaterialExpressionVectorParameter")

    @staticmethod
    def get_statistics(material: Any) -> Object:
        # A commandlet without -AllowCommandletRendering runs the NullRHI: nothing is compiled, every count is 0.
        cmd = str(STATE.get("command_line", "")).lower()
        compiled = "-run=" not in cmd or "-allowcommandletrendering" in cmd
        st = Object()
        for k in ("num_vertex_shader_instructions", "num_pixel_shader_instructions", "num_samplers"):
            st._props[k] = 42 if compiled else 0
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
        STATE.setdefault("saved", []).append(obj._package)
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
                STATE.setdefault("pipelines", {})[stem] = pipeline
                common = pipeline.get_editor_property("common_skeletal_meshes_and_animations_properties")
                mesh_p = pipeline.get_editor_property("mesh_pipeline")
                skeleton = common.get_editor_property("skeleton")
                if common.get_editor_property("import_only_animations"):
                    seq = REGISTRY.get(f"{dest}/{stem}") or AnimSequence()
                    register(seq, f"{dest}/{stem}")
                    seq._props["skeleton"] = skeleton
                    length_ms, frames = STATE["anim_meta"].get(stem, (1000.0, 60))
                    anim_p = pipeline.get_editor_property("animation_pipeline")
                    if anim_p.get_editor_property("animation_range") == "InterchangeAnimationRange.SET_RANGE":
                        rng = anim_p.get_editor_property("frame_import_range")
                        rate = anim_p.get_editor_property("custom_bone_animation_sample_rate") or 30
                        frames = rng.max - rng.min
                        length_ms = frames * 1000.0 / rate
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
                    # Interchange names each slot after the FBX material, in file order.
                    slot_names = fbx_node_names(src, "Material") or ["MI_AF_Toon_Heroes", "M_AF_Outline"]
                    mesh._props["materials"] = []
                    for slot_name in slot_names:
                        slot = SkeletalMaterial()
                        slot._props["material_slot_name"] = slot_name
                        mesh._props["materials"].append(slot)
                    mesh._bones = set(fbx_node_names(src, "Model")) or set(SkeletalMesh.BONES)
                    mesh._props["lod_info"] = [SkeletalMeshLODInfo()]
                    created.insert(0, mesh)
                elif mesh_p.get_editor_property("import_static_meshes"):
                    mesh = REGISTRY.get(f"{dest}/{stem}") or StaticMesh()
                    register(mesh, f"{dest}/{stem}")
                    slot_names = fbx_node_names(src, "Material")
                    if slot_names:
                        mesh._slot_names = slot_names
                        mesh._materials = [None] * len(slot_names)
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
    def get_command_line() -> str:
        return str(STATE.get("command_line", COMMANDLET_LINE))

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
        if not mesh.has_bone(bone):
            return False
        socket.set_editor_property("socket_name", name)
        socket.set_editor_property("bone_name", bone)
        return True

    @staticmethod
    def mesh_has_bone(mesh: SkeletalMesh, bone: str) -> bool:
        return mesh.has_bone(bone)

    @staticmethod
    def set_skeletal_mesh_lod_screen_size(mesh: SkeletalMesh, lod: int, screen_size: float) -> bool:
        infos = mesh._props.get("lod_info") or []
        if not 0 <= lod < len(infos):
            return False
        infos[lod]._props["screen_size"] = PerPlatformFloat(screen_size)
        return True

    @staticmethod
    def get_skeletal_mesh_lod_screen_size(mesh: SkeletalMesh, lod: int) -> float:
        infos = mesh._props.get("lod_info") or []
        if not 0 <= lod < len(infos):
            return -1.0
        return float(infos[lod].get_editor_property("screen_size").default)

    @staticmethod
    def set_material_instance_texture_parameter(mi: Any, name: str, value: Any) -> bool:
        mi._params[("t", str(name))] = value
        return True

    @staticmethod
    def set_material_instance_scalar_parameter(mi: Any, name: str, value: float) -> bool:
        mi._params[("s", str(name))] = float(value)
        return True

    @staticmethod
    def set_material_instance_vector_parameter(mi: Any, name: str, value: LinearColor) -> bool:
        mi._params[("v", str(name))] = value
        return True

    @staticmethod
    def get_texture_parameter_infos(material: Any) -> list[str]:
        return MaterialEditingLibrary.get_texture_parameter_names(material)

    @staticmethod
    def get_material_compile_errors(material: Any) -> list[str]:
        return list(STATE.get("compile_errors", {}).get(material.get_name(), []))

    @staticmethod
    def finish_compilation() -> None:
        pass


# The helper library of an editor binary built before 6a657c8 (no LOD screen sizes, no parameter fallbacks).
STALE_LIBRARY_FUNCTIONS = ("configure_skeletal_mesh_socket", "mesh_has_bone", "finish_compilation")


def use_content_library(mode: str) -> None:
    """unreal.AbyssContentBuildLibrary: "current" (every helper), "stale" (an old editor binary) or "none"."""
    if mode == "current":
        lib: Any = AbyssContentBuildLibrary
    elif mode == "stale":
        lib = type("AbyssContentBuildLibrary", (), {n: AbyssContentBuildLibrary.__dict__[n]
                                                    for n in STALE_LIBRARY_FUNCTIONS})
    else:
        lib = None
    MOD.AbyssContentBuildLibrary = lib


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
    def engine_dir() -> str:
        return "/Users/Shared/Epic Games/UE_5.8/Engine/"

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
    "register", "SUBSYSTEMS", "EXPRESSION_INPUTS", "COMMANDLET_LINE", "INTERCHANGE_58", "INTERCHANGE_56",
    "STALE_LIBRARY_FUNCTIONS", "use_content_library")}
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

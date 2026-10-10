"""Declarative material graph builder on top of MaterialEditingLibrary (ue58-platform.md 11.4).

A material is described by a Python function that calls the Graph API. The function runs twice:
1. a dry run (no material) records a textual trace -> the material's fingerprint, compared with the fingerprint tag of
   the existing asset (unchanged graph + settings = no rebuild, no shader recompile);
2. the real run deletes every expression of the material and rebuilds it.

Connections are checked: an unknown input / output name raises BuildError listing the real pin names
(MaterialEditingLibrary.get_material_expression_input_names), so an engine-version rename fails loudly.
"""
from __future__ import annotations

import hashlib
from dataclasses import dataclass
from typing import Any

import unreal

from .. import hlsl
from .core import BuildError, Editor, enum, linear_color


@dataclass(frozen=True)
class E:
    """Enum reference resolved at build time: E("BlendMode", "BLEND_MASKED")."""
    enum: str
    member: str

    def resolve(self) -> Any:
        return enum(self.enum, self.member)

    def __repr__(self) -> str:
        return f"{self.enum}.{self.member}"


@dataclass(frozen=True)
class A:
    """Asset reference resolved at build time (object or package path)."""
    path: str

    def resolve(self) -> Any:
        obj = unreal.load_asset(self.path)
        if obj is None:
            raise BuildError(f"asset not found: {self.path}")
        return obj

    def __repr__(self) -> str:
        return f"A({self.path})"


@dataclass(frozen=True)
class C:
    """Linear colour."""
    r: float
    g: float
    b: float
    a: float = 1.0

    def resolve(self) -> unreal.LinearColor:
        return linear_color((self.r, self.g, self.b, self.a))

    def __repr__(self) -> str:
        return f"C({self.r:.6g},{self.g:.6g},{self.b:.6g},{self.a:.6g})"


def resolve(value: Any) -> Any:
    if isinstance(value, (E, A, C)):
        return value.resolve()
    return value


class Node:
    def __init__(self, graph: "Graph", idx: int, cls: str, expr: Any) -> None:
        self.graph = graph
        self.idx = idx
        self.cls = cls
        self.expr = expr

    def __getitem__(self, output: str) -> "Pin":
        return Pin(self, output)


@dataclass(frozen=True)
class Pin:
    node: Node
    output: str = ""


Wire = Node | Pin

# Output names tried for an unnamed request (output 0 is unnamed on most expressions; some name it: the Custom node
# "Output", the vertex interpolator "PS", the view property "Property"). A named request (RGB, R, A, Color, ...) never
# falls back to another output: that could silently connect the wrong channel.
_OUTPUT_FALLBACKS = ("", "Output", "Result", "PS", "Property")


class Graph:
    def __init__(self, editor: Editor | None = None, material: Any = None) -> None:
        self.ed = editor
        self.material = material
        self.dry = material is None
        self.trace: list[str] = []
        self._count = 0

    # ---- core ----
    def node(self, cls_name: str, **props: Any) -> Node:
        idx = self._count
        self._count += 1
        self.trace.append(f"#{idx} {cls_name} " + ", ".join(f"{k}={props[k]!r}" for k in sorted(props)))
        expr = None
        if not self.dry:
            cls = getattr(unreal, cls_name, None)
            if cls is None:
                raise BuildError(f"unreal.{cls_name} does not exist in this engine version")
            expr = self.ed.mel.create_material_expression(self.material, cls, -400 - 40 * idx, 30 * idx)
            if expr is None:
                raise BuildError(f"{self.material.get_name()}: could not create {cls_name}")
            for key, value in props.items():
                try:
                    expr.set_editor_property(key, resolve(value))
                except Exception as e:  # noqa: BLE001
                    raise BuildError(f"{self.material.get_name()}: {cls_name}.{key} = {value!r}: {e}") from e
        return Node(self, idx, cls_name, expr)

    def connect(self, src: Wire, dst: Node, input_name: str) -> None:
        pin = src if isinstance(src, Pin) else Pin(src, "")
        self.trace.append(f"#{pin.node.idx}.{pin.output} -> #{dst.idx}.{input_name}")
        if self.dry:
            return
        mel = self.ed.mel
        names = [str(n) for n in (mel.get_material_expression_input_names(dst.expr) or [])]
        target = input_name
        if target not in names:
            by_lower = {n.lower(): n for n in names}
            if target.lower() in by_lower:
                target = by_lower[target.lower()]
            elif len(names) == 1 and input_name in ("", "Input", "VS"):
                target = names[0]
            else:
                raise BuildError(f"{self.material.get_name()}: {dst.cls} has no input {input_name!r} (inputs: {names})")
        for out in self._outputs(pin.output):
            if mel.connect_material_expressions(pin.node.expr, out, dst.expr, target):
                return
        raise BuildError(f"{self.material.get_name()}: cannot connect {pin.node.cls}.{pin.output or '<0>'} -> "
                         f"{dst.cls}.{target}")

    def output(self, src: Wire, prop: str) -> None:
        pin = src if isinstance(src, Pin) else Pin(src, "")
        self.trace.append(f"#{pin.node.idx}.{pin.output} -> {prop}")
        if self.dry:
            return
        prop_value = enum("MaterialProperty", prop)
        for out in self._outputs(pin.output):
            if self.ed.mel.connect_material_property(pin.node.expr, out, prop_value):
                return
        raise BuildError(f"{self.material.get_name()}: cannot connect {pin.node.cls}.{pin.output or '<0>'} -> {prop}")

    @staticmethod
    def _outputs(requested: str) -> list[str]:
        outs = [requested]
        if requested:
            outs += [requested.upper(), requested.lower()]
        else:
            outs += list(_OUTPUT_FALLBACKS)
        seen: list[str] = []
        for o in outs:
            if o not in seen:
                seen.append(o)
        return seen

    def fingerprint(self, settings: dict[str, Any]) -> str:
        h = hashlib.sha256()
        for key in sorted(settings):
            h.update(f"{key}={settings[key]!r}\n".encode())
        for line in self.trace:
            h.update(line.encode())
            h.update(b"\n")
        return h.hexdigest()

    # ---- parameters ----
    def scalar(self, name: str, default: float, cpd: int | None = None, group: str = "") -> Node:
        props: dict[str, Any] = {"parameter_name": name, "default_value": float(default)}
        if cpd is not None:
            props.update(use_custom_primitive_data=True, primitive_data_index=int(cpd))
        if group:
            props["group"] = group
        return self.node("MaterialExpressionScalarParameter", **props)

    def vector(self, name: str, rgba: tuple[float, ...], cpd: int | None = None, group: str = "") -> Node:
        v = list(rgba) + [1.0] * (4 - len(rgba))
        props: dict[str, Any] = {"parameter_name": name, "default_value": C(v[0], v[1], v[2], v[3])}
        if cpd is not None:
            props.update(use_custom_primitive_data=True, primitive_data_index=int(cpd))
        if group:
            props["group"] = group
        return self.node("MaterialExpressionVectorParameter", **props)

    def texture(self, name: str, default_texture: str, sampler: str, uv: Wire | None = None) -> Node:
        n = self.node("MaterialExpressionTextureSampleParameter2D", parameter_name=name, texture=A(default_texture),
                      sampler_type=E("MaterialSamplerType", sampler),
                      sampler_source=E("SamplerSourceMode", "SSM_FROM_TEXTURE_ASSET"))
        if uv is not None:
            self.connect(uv, n, "UVs")
        return n

    def texture_object(self, name: str, default_texture: str, sampler: str) -> Node:
        return self.node("MaterialExpressionTextureObjectParameter", parameter_name=name, texture=A(default_texture),
                         sampler_type=E("MaterialSamplerType", sampler))

    def mpc(self, collection_path: str, parameter: str) -> Node:
        return self.node("MaterialExpressionCollectionParameter", collection=A(collection_path), parameter_name=parameter)

    # ---- constants / inputs ----
    def const(self, value: float) -> Node:
        return self.node("MaterialExpressionConstant", r=float(value))

    def const3(self, rgb: tuple[float, float, float]) -> Node:
        return self.node("MaterialExpressionConstant3Vector", constant=C(rgb[0], rgb[1], rgb[2], 1.0))

    def vertex_color(self) -> Node:
        return self.node("MaterialExpressionVertexColor")

    def texcoord(self, index: int = 0) -> Node:
        return self.node("MaterialExpressionTextureCoordinate", coordinate_index=index)

    def per_instance(self, index: int, default: float) -> Node:
        return self.node("MaterialExpressionPerInstanceCustomData", data_index=index, const_default_value=float(default))

    # ---- math / routing ----
    def append(self, a: Wire, b: Wire) -> Node:
        n = self.node("MaterialExpressionAppendVector")
        self.connect(a, n, "A")
        self.connect(b, n, "B")
        return n

    def plus(self, a: Wire, b: Wire) -> Node:
        n = self.node("MaterialExpressionAdd")
        self.connect(a, n, "A")
        self.connect(b, n, "B")
        return n

    def interpolate(self, src: Wire) -> Node:
        """Vertex-shader value handed to the pixel shader (per-instance data is safest read in the VS on every RHI)."""
        n = self.node("MaterialExpressionVertexInterpolator")
        self.connect(src, n, "VS")
        return n

    def quality(self, default: Wire, low: Wire) -> Node:
        n = self.node("MaterialExpressionQualitySwitch")
        self.connect(default, n, "Default")
        self.connect(low, n, "Low")
        return n

    # ---- custom HLSL ----
    _OUTPUT_TYPES = {"float": "CMOT_FLOAT1", "float2": "CMOT_FLOAT2", "float3": "CMOT_FLOAT3", "float4": "CMOT_FLOAT4"}

    def custom(self, spec: hlsl.CustomNode, wires: dict[str, Wire]) -> Node:
        missing = [i for i in spec.input_names if i not in wires]
        extra = [k for k in wires if k not in spec.input_names]
        if missing or extra:
            raise BuildError(f"custom node {spec.key}: missing inputs {missing}, unknown inputs {extra}")
        n = self.node("MaterialExpressionCustom", code=spec.code, description=spec.key,
                      output_type=E("CustomMaterialOutputType", self._OUTPUT_TYPES[spec.output]))
        self.trace.append(f"#{n.idx} inputs {spec.input_names}")
        if not self.dry:
            inputs = []
            for name in spec.input_names:
                ci = unreal.CustomInput()
                ci.set_editor_property("input_name", name)
                inputs.append(ci)
            try:
                n.expr.set_editor_property("inputs", inputs)
            except Exception as e:  # noqa: BLE001
                raise BuildError(f"{self.material.get_name()}: custom node {spec.key}: cannot set inputs ({e})") from e
        for name in spec.input_names:
            self.connect(wires[name], n, name)
        return n

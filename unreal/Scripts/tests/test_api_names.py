"""Static check of every Unreal editor-API name the content build uses, against a generated UE Python stub.

Without Unreal installed the editor-side code cannot run; this test parses abyss_content/ue/*.py and build_content.py
and checks that each `unreal.<Name>`, each subsystem / library method, each editor property name, each enum member and
each material-expression property exists in the stub. Skipped when no stub is available (tests/fetch_ue_stub.py, or
UE_PY_STUB=<path to unreal.py>).
"""
from __future__ import annotations

import ast
import os
import re
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCRIPTS = HERE.parent
STUB_CANDIDATES = [os.environ.get("UE_PY_STUB", ""), str(SCRIPTS / ".cache" / "unreal_stub" / "unreal.py")]

# Editor wrapper attributes (core.Editor) -> stub class.
EDITOR_ATTRS = {
    "assets": "EditorAssetSubsystem", "mel": "MaterialEditingLibrary", "skm": "SkeletalMeshEditorSubsystem",
    "smes": "StaticMeshEditorSubsystem", "levels": "LevelEditorSubsystem", "actors": "EditorActorSubsystem",
    "unreal_editor": "UnrealEditorSubsystem", "tools": "AssetTools", "interchange": "InterchangeManager",
}
# Names that are ours (C++ helper, Python builtins / our own objects), not stub API.
OWN_METHODS = {
    "configure_skeletal_mesh_socket", "mesh_has_bone", "finish_compilation",          # AbyssContentBuildLibrary
}
PY_BUILTIN_METHODS = {"append", "items", "keys", "values", "get", "split", "strip", "lower", "upper", "startswith",
                      "endswith", "replace", "join", "format", "pop", "update", "extend", "copy", "is_file", "is_dir",
                      "read_text", "write_text", "read_bytes", "write_bytes", "mkdir", "with_suffix", "relative_to",
                      "as_posix", "exists", "rsplit", "setdefault", "sort", "index", "count", "add", "note", "error",
                      "warning", "begin", "end", "skip", "asset", "resolve", "fingerprint", "rglob", "glob", "decode",
                      "encode", "hexdigest", "digest",
                      # stdlib / our own plan objects
                      "spec_from_file_location", "module_from_spec", "exec_module", "sha256", "ArgumentParser",
                      "add_argument", "parse_args", "summary"}


def find_stub() -> Path | None:
    for c in STUB_CANDIDATES:
        if c and Path(c).is_file():
            return Path(c)
    return None


class Stub:
    def __init__(self, path: Path) -> None:
        text = path.read_text(encoding="utf-8-sig")
        self.lines = text.split("\n")
        self.classes: dict[str, tuple[int, int]] = {}
        self.bases: dict[str, list[str]] = {}
        starts = []
        for i, line in enumerate(self.lines):
            m = re.match(r"^class (\w+)\((.*?)\):", line)
            if m:
                starts.append((i, m.group(1)))
                self.bases[m.group(1)] = [b.strip() for b in m.group(2).split(",") if b.strip()]
        for (i, name), nxt in zip(starts, starts[1:] + [(len(self.lines), "")]):
            self.classes[name] = (i, nxt[0])
        self.functions = set(re.findall(r"^def (\w+)\(", text, re.M))
        self.all_defs = set(re.findall(r"^\s+def (\w+)\(", text, re.M))
        self.all_props = set(re.findall(r"^\s+- ``(\w+)`` \(", text, re.M))

    def _section(self, cls: str) -> list[str]:
        if cls not in self.classes:
            return []
        a, b = self.classes[cls]
        return self.lines[a:b]

    def class_has(self, cls: str, member: str, kind: str) -> bool:
        seen = set()
        stack = [cls]
        while stack:
            c = stack.pop()
            if c in seen:
                continue
            seen.add(c)
            for line in self._section(c):
                if kind == "def" and re.match(rf"^\s+def {re.escape(member)}\(", line):
                    return True
                if kind == "prop" and re.match(rf"^\s+- ``{re.escape(member)}`` \(", line):
                    return True
            stack += self.bases.get(c, [])
        return False

    def enum_has(self, enum: str, member: str) -> bool:
        return any(re.match(rf"^\s+{re.escape(member)}: {re.escape(enum)} = ", line) for line in self._section(enum))


def sources() -> list[Path]:
    return sorted((SCRIPTS / "abyss_content" / "ue").glob("*.py")) + [SCRIPTS / "build_content.py"]


def _str(node: ast.AST) -> str | None:
    return node.value if isinstance(node, ast.Constant) and isinstance(node.value, str) else None


@unittest.skipIf(find_stub() is None, "no UE Python stub (run tests/fetch_ue_stub.py or set UE_PY_STUB)")
class ApiNameTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.stub = Stub(find_stub())
        cls.trees = {p: ast.parse(p.read_text(encoding="utf-8")) for p in sources()}

    def test_unreal_attributes_exist(self):
        missing = []
        for path, tree in self.trees.items():
            for node in ast.walk(tree):
                if isinstance(node, ast.Attribute) and isinstance(node.value, ast.Name) and node.value.id == "unreal":
                    name = node.attr
                    if name not in self.stub.classes and name not in self.stub.functions:
                        missing.append(f"{path.name}:{node.lineno} unreal.{name}")
        self.assertFalse(missing, "unknown unreal names:\n" + "\n".join(missing))

    def test_subsystem_methods_exist(self):
        missing = []
        for path, tree in self.trees.items():
            for node in ast.walk(tree):
                if not (isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)):
                    continue
                owner = node.func.value
                method = node.func.attr
                if isinstance(owner, ast.Attribute) and owner.attr in EDITOR_ATTRS:
                    cls = EDITOR_ATTRS[owner.attr]
                    if not self.stub.class_has(cls, method, "def"):
                        missing.append(f"{path.name}:{node.lineno} {cls}.{method}")
                elif isinstance(owner, ast.Attribute) and isinstance(owner.value, ast.Name) and owner.value.id == "unreal":
                    cls = owner.attr
                    if cls in self.stub.classes and not self.stub.class_has(cls, method, "def"):
                        missing.append(f"{path.name}:{node.lineno} unreal.{cls}.{method}")
        self.assertFalse(missing, "unknown methods:\n" + "\n".join(missing))

    def test_other_methods_exist_somewhere(self):
        """Weak check for calls on UE objects held in variables: the method must exist in some stub class."""
        missing = []
        for path, tree in self.trees.items():
            for node in ast.walk(tree):
                if not (isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)):
                    continue
                method = node.func.attr
                owner = node.func.value
                if isinstance(owner, ast.Attribute) and owner.attr in EDITOR_ATTRS:
                    continue
                if isinstance(owner, ast.Name) and owner.id in ("unreal", "report", "ed", "g", "graph", "dry", "core",
                                                                 "materials", "meshes", "textures", "level", "audio",
                                                                 "fxmeshes", "paths", "pngio", "hlsl", "args", "json",
                                                                 "os", "sys", "re", "math", "time", "shlex", "slow",
                                                                 "importlib", "traceback", "lib", "spec", "module",
                                                                 "h", "f", "mel", "self"):
                    continue
                if method in PY_BUILTIN_METHODS or method in OWN_METHODS or method.startswith("_"):
                    continue
                if method not in self.stub.all_defs and method not in self.stub.functions:
                    missing.append(f"{path.name}:{node.lineno} .{method}()")
        self.assertFalse(missing, "methods not found in any stub class:\n" + "\n".join(sorted(set(missing))))

    def test_editor_property_names_exist(self):
        missing = []
        for path, tree in self.trees.items():
            for node in ast.walk(tree):
                if not isinstance(node, ast.Call):
                    continue
                names: list[tuple[str, int]] = []
                func = node.func
                fname = func.attr if isinstance(func, ast.Attribute) else (func.id if isinstance(func, ast.Name) else "")
                if fname in ("set_editor_property", "get_editor_property") and node.args:
                    s = _str(node.args[0])
                    if s:
                        names.append((s, node.lineno))
                if fname == "set_props" and len(node.args) >= 2 and isinstance(node.args[1], ast.Dict):
                    names += [(_str(k), node.lineno) for k in node.args[1].keys if _str(k)]
                if fname == "node" and node.args and _str(node.args[0]):
                    cls = _str(node.args[0])
                    for kw in node.keywords:
                        if kw.arg and not self.stub.class_has(cls, kw.arg, "prop"):
                            missing.append(f"{path.name}:{node.lineno} {cls}.{kw.arg}")
                for name, line in names:
                    if name not in self.stub.all_props:
                        missing.append(f"{path.name}:{line} property {name}")
            # dict literals of settings (materials.py *_SETTINGS / WORLD_SETTINGS, textures.settings_for)
            for node in ast.walk(tree):
                if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id == "_surface":
                    for kw in node.keywords:
                        if kw.arg and kw.arg not in self.stub.all_props:
                            missing.append(f"{path.name}:{node.lineno} property {kw.arg}")
        self.assertFalse(missing, "unknown editor properties:\n" + "\n".join(missing))

    def test_settings_dicts(self):
        """Keys of the property dicts handed to set_props: module-level *_SETTINGS and textures.settings_for()."""
        missing = []
        for path, tree in self.trees.items():
            dicts: list[ast.Dict] = []
            for node in tree.body:
                if isinstance(node, ast.Assign) and isinstance(node.value, ast.Dict) and any(
                        isinstance(t, ast.Name) and t.id.endswith("SETTINGS") for t in node.targets):
                    dicts.append(node.value)
                if isinstance(node, ast.FunctionDef) and node.name == "settings_for":
                    dicts += [n for n in ast.walk(node) if isinstance(n, ast.Dict)]
            for d in dicts:
                for k in d.keys:
                    key = _str(k) if k is not None else None
                    if key and key not in self.stub.all_props:
                        missing.append(f"{path.name}:{d.lineno} property {key}")
        self.assertFalse(missing, "unknown editor properties:\n" + "\n".join(missing))

    def test_enum_members_exist(self):
        missing = []
        for path, tree in self.trees.items():
            for node in ast.walk(tree):
                if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id in ("E", "enum") \
                        and len(node.args) == 2 and _str(node.args[0]) and _str(node.args[1]):
                    enum, member = _str(node.args[0]), _str(node.args[1])
                    if not self.stub.enum_has(enum, member):
                        missing.append(f"{path.name}:{node.lineno} {enum}.{member}")
        # members chosen at runtime from data
        from_data = [("AdditiveBasePoseType", m) for m in ("ABPT_LOCAL_ANIM_FRAME", "ABPT_ANIM_FRAME", "ABPT_ANIM_SCALED",
                                                            "ABPT_REF_POSE")]
        from_data += [("InterchangeForceMeshType", m) for m in ("IFMT_SKELETAL_MESH", "IFMT_STATIC_MESH", "IFMT_NONE")]
        from_data += [("CustomMaterialOutputType", m) for m in ("CMOT_FLOAT1", "CMOT_FLOAT2", "CMOT_FLOAT3", "CMOT_FLOAT4")]
        from_data += [("MaterialProperty", m) for m in ("MP_EMISSIVE_COLOR", "MP_OPACITY", "MP_OPACITY_MASK",
                                                        "MP_BASE_COLOR", "MP_SPECULAR", "MP_ROUGHNESS",
                                                        "MP_WORLD_POSITION_OFFSET")]
        from_data += [("MaterialSamplerType", m) for m in ("SAMPLERTYPE_COLOR", "SAMPLERTYPE_LINEAR_COLOR")]
        for enum, member in from_data:
            if not self.stub.enum_has(enum, member):
                missing.append(f"(data) {enum}.{member}")
        self.assertFalse(missing, "unknown enum members:\n" + "\n".join(missing))


if __name__ == "__main__":
    unittest.main()

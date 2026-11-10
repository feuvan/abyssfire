#!/usr/bin/env python3
"""Cross-check the stub SDK against the engine-generated Python stub (unreal.py, UE 5.6).

The Python stub lists everything the engine exposes through reflection: classes (with base class, module and header
file), UFUNCTIONs exposed to Blueprint / Python (snake_case, parameter names), editor properties and enums. The SDK
marks what it claims to be reflected:
    class U... / A...  with UE_STUB_CLASS_BODY(UFoo, UBar)   -> class Foo(Bar) must exist; module and header checked
    struct F...        with UE_STUB_STRUCT_BODY(FFoo)        -> class Foo must exist; header checked
    UFUNCTION(...) before a member function                  -> method foo_bar must exist on the class (or a base);
                                                                parameter names compared
    UPROPERTY(...) before a member                           -> editor property must be documented on the class
    UENUM(...) before an enum                                -> class Foo(EnumBase) must exist; enumerators compared
Members without those markers are "from UE source knowledge" and cannot be checked here.

  check_reflected.py                      check the whole SDK, print mismatches, exit 1 if any error
  check_reflected.py --show Actor         print what the stub knows about a class (methods, properties)
  check_reflected.py --find spawn_sound   search method names
The stub is UE 5.6 and the project targets 5.8: a mismatch is evidence, not proof (README.md, API_UNCERTAIN.md).
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CHECK_DIR = os.path.dirname(HERE)
SDK_DIR = os.path.join(CHECK_DIR, 'sdk')
sys.dont_write_bytecode = True  # no __pycache__ next to the sources
sys.path.insert(0, HERE)
from stubpath import find_stub  # noqa: E402

DEFAULT_STUB = find_stub(os.path.dirname(CHECK_DIR))


def load_stub(path, cache_dir=None):
    """-> {class name: {base, module, file, methods: {name: [params]}, props: [names], enum: [members]}}"""
    stamp = 'v4:%d:%d' % (os.path.getsize(path), int(os.path.getmtime(path)))   # v4: default arguments, parameter / property types
    cache = os.path.join(cache_dir, 'stub_index.json') if cache_dir else None
    if cache and os.path.exists(cache):
        try:
            with open(cache, encoding='utf-8') as f:
                data = json.load(f)
            if data.get('stamp') == stamp:
                return data['classes']
        except (ValueError, KeyError):
            pass
    classes, cur = {}, None
    class_re = re.compile(r'^class (\w+)\((.*?)\):')
    def_re = re.compile(r'^    def (\w+)\((.*)\)(?: -> (.*))?:')
    prop_re = re.compile(r'^    - ``(\w+)`` \((.*?)\):')
    with open(path, encoding='utf-8') as f:
        for line in f:
            m = class_re.match(line)
            if m:
                cur = {'base': m.group(2).split(',')[0].strip(), 'module': None, 'file': None, 'methods': {}, 'props': [], 'prop_types': {}, 'members': []}
                classes[m.group(1)] = cur
                continue
            if cur is None:
                continue
            if line.startswith('    - **Module**:'):
                cur['module'] = line.split(':', 1)[1].strip()
            elif line.startswith('    - **File**:'):
                cur['file'] = line.split(':', 1)[1].strip()
            else:
                m = def_re.match(line)
                if m:
                    params, defaults, types = [], {}, {}
                    for part in split_top(m.group(2)):
                        pname = part.split(':')[0].split('=')[0].strip()
                        if pname in ('self', 'cls'):
                            continue
                        params.append(pname)
                        # "name: type = value": the C++ default argument as UHT recorded it (CPP_Default_<Param>)
                        annotation = part.split(':', 1)[1] if ':' in part else ''
                        if '=' in annotation:
                            defaults[pname] = annotation.split('=', 1)[1].strip()
                        if annotation.split('=', 1)[0].strip():
                            types[pname] = annotation.split('=', 1)[0].strip()
                    cur['methods'][m.group(1)] = {'params': params, 'ret': (m.group(3) or '').strip(), 'defaults': defaults, 'types': types}
                    continue
                m = prop_re.match(line)
                if m:
                    cur['props'].append(m.group(1))
                    cur['prop_types'][m.group(1)] = m.group(2).strip()
                    continue
                m = re.match(r'^    ([A-Z][A-Z0-9_]*): \w+ = ', line)
                if m:
                    cur['members'].append(m.group(1))
    if cache:
        os.makedirs(cache_dir, exist_ok=True)
        with open(cache, 'w', encoding='utf-8') as f:
            json.dump({'stamp': stamp, 'classes': classes}, f)
    return classes


def split_top(text):
    parts, depth, cur = [], 0, []
    for ch in text:
        if ch in '([{<':
            depth += 1
        elif ch in ')]}>':
            depth -= 1
        if ch == ',' and depth == 0:
            parts.append(''.join(cur))
            cur = []
        else:
            cur.append(ch)
    if ''.join(cur).strip():
        parts.append(''.join(cur))
    return parts


def snake(name):
    """UE's PythonScriptPlugin naming (PyGenUtil::PythonizeName): GetActorLocation -> get_actor_location, bIsActive ->
    is_active, SetRelativeScale3D -> set_relative_scale3d, PrestreamMeshLODs -> prestream_mesh_lo_ds. A break goes before
    an upper-case letter that follows a lower-case one, or that follows an upper-case one and precedes a lower-case one;
    digits never start a word."""
    if re.match(r'b[A-Z]', name):
        name = name[1:]
    name = re.sub(r'^K2_', '', name)
    out = []
    for i, ch in enumerate(name):
        if ch == '_':
            out.append('_')
            continue
        prev = name[i - 1] if i else ''
        nxt = name[i + 1] if i + 1 < len(name) else ''
        if ch.isupper() and i and prev != '_' and (prev.islower() or (prev.isupper() and nxt.islower())):
            out.append('_')
        out.append(ch.lower())
    return re.sub(r'__+', '_', ''.join(out))


PY_SCALARS = {'int8': 'int', 'int16': 'int', 'int32': 'int', 'int64': 'int', 'uint8': 'int', 'uint16': 'int', 'uint32': 'int', 'uint64': 'int',
              'int': 'int', 'float': 'float', 'double': 'float', 'bool': 'bool', 'FString': 'str', 'FName': 'Name', 'FText': 'Text', 'UClass': 'Class',
              'FVector2f': 'Vector2f', 'FVector3f': 'Vector3f', 'FVector4f': 'Vector4f'}


SCRIPT_NAMES = {}   # C++ enum name -> Python name, from `UENUM(..., meta = (ScriptName = "X"))` in the SDK


def load_script_names(sdk_dir):
    for root, _dirs, files in os.walk(sdk_dir):
        for name in files:
            if name.endswith('.h'):
                with open(os.path.join(root, name), encoding='utf-8') as f:
                    text = f.read()
                for m in re.finditer(r'\bUENUM\s*\(([^\n]*)\)\s*(?:namespace\s+(\w+)|enum\s+(?:class\s+)?(\w+))', text):
                    script = re.search(r'ScriptName\s*=\s*"(\w+)"', m.group(1))
                    if script:
                        SCRIPT_NAMES[m.group(2) or m.group(3)] = script.group(1)


# property documentation keeps the C++ scalar ("(int32)", "(double)"): compared exactly
PROP_SCALARS = dict(PY_SCALARS, **{k: k for k in ('int8', 'int16', 'int32', 'int64', 'uint8', 'uint16', 'uint32', 'uint64', 'float', 'double')})


def python_type(cpp, scalars=None):
    """The Python (unreal.py) spelling of a C++ parameter type: FVector -> Vector, const TArray<AActor*>& -> Array[Actor],
    TSubclassOf<UFoo> -> Class, EFoo::Type / TEnumAsByte<EFoo> -> Foo. None when the type is not one of these shapes."""
    t = re.sub(r'\b(const|class|struct|enum|typename)\b', ' ', cpp)
    t = re.sub(r'\s+', '', t).rstrip('&*')
    for wrapper in ('TObjectPtr', 'TEnumAsByte', 'TScriptInterface', 'TSoftObjectPtr', 'TWeakObjectPtr'):
        m = re.match(r'^%s<(.*)>$' % wrapper, t)
        if m:
            return python_type(m.group(1), scalars)
    if re.match(r'^(TSubclassOf|TSoftClassPtr)<.*>$', t):
        return 'Class'
    for container, py in (('TArray', 'Array'), ('TSet', 'Set'), ('TMap', 'Map'), ('TArrayView', 'Array')):
        m = re.match(r'^%s<(.*)>$' % container, t)
        if m:
            inner = [python_type(x, scalars) for x in split_top(m.group(1))]
            return None if None in inner else '%s[%s]' % (py, ','.join(inner))
    t = re.sub(r'::Type$', '', t)
    if t in SCRIPT_NAMES:
        return SCRIPT_NAMES[t]
    if t in (scalars or PY_SCALARS):
        return (scalars or PY_SCALARS)[t]
    if re.match(r'^[UAFEI][A-Z]\w*$', t):
        return t[1:]
    return None


def same_type(want, have):
    """Known spelling differences between the two sides: Python types its class parameters `Class` or `type(Class)`,
    and wraps what may be None."""
    have = re.sub(r'^Optional\[(.*)\]$', r'\1', have)
    if want == have:
        return True
    if want == 'Class' and (have.startswith('type(') or have == 'Class'):
        return True
    return want.replace('type(Class)', 'Class') == have.replace('type(Class)', 'Class')


def same_default(cpp, py):
    """A C++ default argument against the Python one: booleans and numbers are compared, anything else (enumerators,
    structs, names, nullptr / None) is accepted as it is spelled differently on the two sides."""
    cpp, py = cpp.strip(), py.strip()
    if py in ('True', 'False'):
        return cpp in ('true', 'false') and (cpp == 'true') == (py == 'True')
    number = re.compile(r'^[-+]?(\d+\.?\d*|\.\d+)([eE][-+]?\d+)?$')
    cpp_number = re.sub(r'(?<=[\d.])[fFuUlL]+$', '', cpp)
    if number.match(py) and number.match(cpp_number):
        return abs(float(py) - float(cpp_number)) <= 1e-6 * max(1.0, abs(float(py)))
    return True


def clean(text):
    text = re.sub(r'/\*.*?\*/', lambda m: re.sub(r'[^\n]', ' ', m.group(0)), text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def match_close(text, i, o, c):
    depth = 0
    for j in range(i, len(text)):
        if text[j] == o:
            depth += 1
        elif text[j] == c:
            depth -= 1
            if depth == 0:
                return j
    return -1


def sdk_module_of(path, modules):
    rel = os.path.relpath(path, SDK_DIR).replace(os.sep, '/')
    best = None
    for name, info in modules.items():
        for root in info.get('roots', []):
            if rel.startswith(root + '/') and (best is None or len(root) > len(best[1])):
                best = (name, root)
    return best[0] if best else None


class Checker:
    def __init__(self, stub, modules):
        self.stub, self.modules = stub, modules
        self.problems = []
        self.checked = {'classes': 0, 'structs': 0, 'functions': 0, 'properties': 0, 'enums': 0}

    def problem(self, level, path, line, message):
        self.problems.append((level, os.path.relpath(path, CHECK_DIR), line, message))

    def find_method(self, cls, name):
        seen = 0
        while cls in self.stub and seen < 40:
            if name in self.stub[cls]['methods']:
                return cls, self.stub[cls]['methods'][name]
            cls = self.stub[cls]['base']
            seen += 1
        return None, None

    def has_prop(self, cls, name):
        seen = 0
        while cls in self.stub and seen < 40:
            if name in self.stub[cls]['props']:
                return True
            cls = self.stub[cls]['base']
            seen += 1
        return False

    def prop_type(self, cls, name):
        seen = 0
        while cls in self.stub and seen < 40:
            if name in self.stub[cls].get('prop_types', {}):
                return self.stub[cls]['prop_types'][name]
            cls = self.stub[cls]['base']
            seen += 1
        return None

    def lookup(self, cpp_name, header, want_enum=False):
        """The stub entry of a C++ type: by its name without the prefix, else (ScriptName renames such as
        UKismetSystemLibrary -> SystemLibrary, ECollisionResponse -> CollisionResponseType) by header file."""
        py = cpp_name[1:] if re.match(r'[UAFEIT][A-Z]', cpp_name) else cpp_name
        entry = self.stub.get(py)
        is_enum = lambda e: e.get('base') == 'EnumBase'
        if entry is not None and is_enum(entry) == want_enum:
            return py, entry
        candidates = [n for n, e in self.stub.items() if e.get('file') == header and is_enum(e) == want_enum
                      and (n in py or py in n)]
        if len(candidates) == 1:
            return candidates[0], self.stub[candidates[0]]
        return py, None

    def check_file(self, path):
        with open(path, encoding='utf-8') as f:
            text = clean(f.read())
        line_of = lambda off: text.count('\n', 0, off) + 1
        module = sdk_module_of(path, self.modules)
        base_name = os.path.basename(path)
        # classes / structs
        for m in re.finditer(r'\b(class|struct)\s+(?:[A-Z_]+_API\s+)?([A-Za-z_]\w*)\s*(?:final\s*)?(:[^{;]*)?\{', text):
            body_open = m.end() - 1
            body_close = match_close(text, body_open, '{', '}')
            if body_close < 0:
                continue
            body = text[body_open + 1:body_close]
            name = m.group(2)
            cm = re.match(r'\s*UE_STUB_CLASS_BODY\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)', body)
            sm = re.match(r'\s*UE_STUB_STRUCT_BODY\s*\(\s*(\w+)\s*\)', body)
            if not cm and not sm:
                continue
            line = line_of(m.start(2))
            py, entry = self.lookup(name, base_name)
            if cm:
                self.checked['classes'] += 1
                if cm.group(1) != name:
                    self.problem('error', path, line, 'UE_STUB_CLASS_BODY(%s, ...) inside class %s' % (cm.group(1), name))
                declared_base = (m.group(3) or '').replace(':', ' ').replace('public', ' ').split(',')[0].strip()
                if declared_base != cm.group(2):
                    self.problem('error', path, line, '%s: UE_STUB_CLASS_BODY names Super %s but the first base is %s' % (name, cm.group(2), declared_base))
            else:
                self.checked['structs'] += 1
            if entry is None:
                # not exposed to Python (no Blueprint-visible member) or newer than the stub: nothing to compare with
                self.problem('note', path, line, '%s: not in the Python stub (not exposed to Python, or newer than 5.6): cannot be cross-checked' % name)
                continue
            if cm and name not in ('UObject',):
                want = cm.group(2)[1:]
                have = entry['base']
                if have not in (want, '_ObjectBase', 'Object' if want == 'Object' else want):
                    self.problem('error', path, line, '%s: the engine base class is %s, the SDK derives it from %s' % (name, have, cm.group(2)))
            if entry.get('file') and entry['file'] != base_name and entry['file'] != 'NoExportTypes.h':
                self.problem('error', path, line, '%s is declared in %s in the engine, the SDK has it in %s' % (name, entry['file'], base_name))
            if entry.get('module') and module and entry['module'] != module:
                self.problem('error', path, line, '%s belongs to engine module %s, the SDK has it under %s' % (name, entry['module'], module))
            # reflected members
            for fm in re.finditer(r'\bUFUNCTION\s*\(', body):
                close = match_close(body, fm.end() - 1, '(', ')')
                decl_end = min(x for x in (body.find(';', close), body.find('{', close)) if x >= 0)
                decl = ' '.join(body[close + 1:decl_end].split())
                po = decl.find('(')
                pc = match_close(decl, po, '(', ')') if po >= 0 else -1
                if po < 0 or pc < 0:
                    self.problem('error', path, line_of(body_open + 1 + fm.start()), '%s: cannot parse the UFUNCTION declaration "%s"' % (name, decl[:80]))
                    continue
                fname = decl[:po].split()[-1].lstrip('*&')
                specifiers = body[fm.end():close]
                if re.search(r'\b(BlueprintSetter|BlueprintGetter|BlueprintInternalUseOnly)\b', specifiers):
                    continue  # exposed as a property / not exposed: no method to compare with
                exposed = re.search(r'\b(BlueprintCallable|BlueprintPure|BlueprintNativeEvent|BlueprintImplementableEvent)\b', specifiers) is not None
                script_name = re.search(r'ScriptName\s*=\s*"?(\w+)', specifiers)
                py_name = snake(script_name.group(1) if script_name else fname)
                self.checked['functions'] += 1
                fline = line_of(body_open + 1 + fm.start())
                owner, method = self.find_method(py, py_name)
                if method is None:
                    if exposed:
                        self.problem('error', path, fline, '%s::%s: no method "%s" on %s (or its bases) in the Python stub' % (name, fname, py_name, py))
                    continue
                cpp_params, cpp_defaults, meta_defaults, cpp_types = [], {}, set(), {}
                for part in split_top(decl[po + 1:pc]):
                    part = re.sub(r'\bUPARAM\s*\([^)]*\)', '', part)
                    default = part.split('=', 1)[1].strip() if '=' in part else None
                    part = part.split('=')[0].strip()
                    if not part or part == 'void':
                        continue
                    pm = re.match(r'(.*?)([A-Za-z_]\w*)$', part)
                    ptype = pm.group(1).strip()
                    is_out = ptype.endswith('&') and not ptype.startswith('const')
                    pname = re.sub(r'^in_(?=.)', '', snake(pm.group(2)))
                    cpp_params.append((pname, is_out))
                    cpp_types[pname] = ptype
                    if default is not None:
                        cpp_defaults[pname] = default
                    if re.search(r'\b%s\s*=' % re.escape(pm.group(2)), specifiers):
                        meta_defaults.add(pname)   # meta = (Param = value): a Blueprint / Python default, not a C++ one
                want = [p for p, is_out in cpp_params if not is_out]
                have = method['params']
                if want != have and [p for p, _ in cpp_params] != have:
                    self.problem('warn', path, fline, '%s::%s: parameters differ: SDK (%s) vs stub %s.%s(%s)' % (name, fname, ', '.join(p for p, _ in cpp_params), owner, py_name, ', '.join(have)))
                else:
                    # default arguments: which parameters have one decides which calls compile; simple values are compared
                    self.checked['defaults'] = self.checked.get('defaults', 0) + 1
                    stub_defaults = method.get('defaults', {})
                    stub_types = method.get('types', {})
                    # the return type, when no output parameter turns the Python result into a tuple
                    if not any(is_out for _, is_out in cpp_params) and method.get('ret'):
                        cpp_ret = re.sub(r'\b(virtual|static|inline|FORCEINLINE|constexpr|[A-Z]+_API)\b', ' ', decl[:po].rsplit(fname, 1)[0]).strip()
                        want_ret = 'None' if cpp_ret == 'void' else python_type(cpp_ret)
                        have_ret = method['ret'].replace(' ', '')
                        if want_ret is not None and not same_type(want_ret, have_ret):
                            self.problem('warn', path, fline, '%s::%s returns %s in the SDK (Python: %s) but %s in the engine (stub %s.%s)' % (
                                name, fname, cpp_ret, want_ret, method['ret'], owner, py_name))
                    for pname in have:
                        # parameter types: what a call may pass. Both sides are reduced to the Python spelling.
                        if pname in stub_types and pname in cpp_types:
                            want_type, have_type = python_type(cpp_types[pname]), stub_types[pname].replace(' ', '')
                            if want_type is not None and want_type != have_type and not same_type(want_type, have_type):
                                self.problem('warn', path, fline, '%s::%s: parameter "%s" is %s in the SDK (Python: %s) but %s in the engine (stub %s.%s)' % (
                                    name, fname, pname, cpp_types[pname], want_type, stub_types[pname], owner, py_name))
                    for pname in have:
                        in_sdk, in_stub = pname in cpp_defaults, pname in stub_defaults
                        if in_sdk != in_stub and pname in meta_defaults and not in_sdk:
                            continue
                        if in_sdk != in_stub:
                            # a default the engine does not have makes calls with too few arguments compile: an error
                            self.problem('error' if in_sdk else 'warn', path, fline, '%s::%s: parameter "%s" %s in the SDK but %s in the engine (stub %s.%s: %s)' % (
                                name, fname, pname, 'has a default argument' if in_sdk else 'has no default argument',
                                'none' if in_sdk else 'the default ' + stub_defaults[pname], owner, py_name, ', '.join(
                                    '%s=%s' % (q, stub_defaults[q]) if q in stub_defaults else q for q in have)))
                        elif in_sdk and not same_default(cpp_defaults[pname], stub_defaults[pname]):
                            self.problem('warn', path, fline, '%s::%s: default of "%s" is %s in the SDK and %s in the engine (stub %s.%s)' % (
                                name, fname, pname, cpp_defaults[pname], stub_defaults[pname], owner, py_name))
            for pm in re.finditer(r'\bUPROPERTY\s*\(', body):
                close = match_close(body, pm.end() - 1, '(', ')')
                decl_end = body.find(';', close)
                decl = ' '.join(body[close + 1:decl_end].split())
                decl = re.sub(r'\s*(=|\{).*$', '', decl)
                is_bitfield = re.search(r'\s*:\s*\d+$', decl) is not None
                decl = re.sub(r'\s*:\s*\d+$', '', decl)
                member = decl.split()[-1].lstrip('*&') if decl.split() else ''
                self.checked['properties'] += 1
                if not re.search(r'\b(Edit\w+|Visible\w+|BlueprintReadOnly|BlueprintReadWrite)\b', body[pm.end():close]):
                    continue  # not visible to the editor / Blueprint: Python does not document it
                if not self.has_prop(py, snake(member)):
                    self.problem('warn', path, line_of(body_open + 1 + pm.start()), '%s::%s: no editor property "%s" documented on %s in the Python stub (not Edit/Visible/Blueprint exposed, or renamed)' % (name, member, snake(member), py))
                    continue
                # the property's type (a bitfield is a bool to reflection)
                cpp_type = decl[:decl.rfind(member)].strip() if member and member in decl else ''
                want_type = 'bool' if is_bitfield else python_type(cpp_type, PROP_SCALARS)
                have_type = (self.prop_type(py, snake(member)) or '').replace(' ', '')
                if want_type is not None and have_type and not same_type(want_type, have_type):
                    self.problem('warn', path, line_of(body_open + 1 + pm.start()), '%s::%s is %s in the SDK (Python: %s) but %s in the engine (stub property %s.%s)' % (
                        name, member, cpp_type + (' : 1' if is_bitfield else ''), want_type, self.prop_type(py, snake(member)), py, snake(member)))
        # enums
        for m in re.finditer(r'\bUENUM\s*\((?:[^()]|\([^()]*\))*\)\s*(?:namespace\s+(\w+)\s*\{\s*)?enum\s+(?:class\s+)?(\w+)\s*(?::\s*[\w:]+\s*)?\{', text):
            name = m.group(1) or m.group(2)
            close = match_close(text, m.end() - 1, '{', '}')
            self.checked['enums'] += 1
            line = line_of(m.start())
            _py, entry = self.lookup(name, base_name, want_enum=True)
            if entry is None:
                self.problem('note', path, line, 'enum %s: not in the Python stub (not exposed to Python, or newer than 5.6): cannot be cross-checked' % name)
                continue
            if entry.get('file') and entry['file'] != base_name:
                self.problem('error', path, line, 'enum %s is declared in %s in the engine, the SDK has it in %s' % (name, entry['file'], base_name))
            have = set(entry['members'])
            missing, matched = [], 0
            for part in split_top(text[m.end():close]):
                ident = re.match(r'\s*(\w+)', re.sub(r'\bUMETA\s*\([^)]*\)', '', part))
                if not ident:
                    continue
                enumerator = ident.group(1)
                if re.search(r'(_MAX|_Max|MAX|Count|_NUM|NumOf\w*)$', enumerator) or 'Hidden' in part:
                    continue
                candidates = {snake(enumerator).upper(), enumerator.upper(), snake(enumerator.split('_', 1)[-1]).upper()}
                if candidates & have:
                    matched += 1
                else:
                    missing.append(enumerator)
            if missing:
                # hidden enumerators (UMETA(Hidden)) are not exposed to Python: with other enumerators matching, that is
                # the likely reason; with none matching the enum itself is suspect
                level = 'note' if matched else 'warn'
                shown = ', '.join(missing[:6]) + (', ... (%d)' % len(missing) if len(missing) > 6 else '')
                self.problem(level, path, line, 'enum %s: %d enumerator(s) not in the Python stub enum (hidden, renamed or newer): %s' % (name, len(missing), shown))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--stub', default=DEFAULT_STUB)
    parser.add_argument('--cache-dir', default=os.environ.get('ABYSS_CC_BUILD', os.path.join(CHECK_DIR, 'build')))
    parser.add_argument('--show', action='append', default=[], help='print the stub entry of a class (Python name, e.g. Actor)')
    parser.add_argument('--find', help='search method names (substring)')
    parser.add_argument('--grep', help='with --show: only methods / properties containing this')
    parser.add_argument('--quiet', action='store_true')
    parser.add_argument('--notes', action='store_true', help='also list the SDK types that are not in the Python stub (cannot be cross-checked)')
    args = parser.parse_args()
    if not os.path.exists(args.stub):
        print('Python stub not found: %s (set ABYSS_UNREAL_STUB); nothing checked' % args.stub)
        return 0
    stub = load_stub(args.stub, args.cache_dir)
    if args.show:
        for name in args.show:
            entry = stub.get(name)
            if not entry:
                print('%s: not in the stub' % name)
                continue
            print('class %s(%s)   module %s   file %s' % (name, entry['base'], entry['module'], entry['file']))
            for method in sorted(entry['methods']):
                if not args.grep or args.grep in method:
                    print('  %s(%s) -> %s' % (method, ', '.join(entry['methods'][method]['params']), entry['methods'][method]['ret']))
            props = [p for p in entry['props'] if not args.grep or args.grep in p]
            if props:
                print('  properties: ' + ', '.join(props))
            if entry['members']:
                print('  members: ' + ', '.join(entry['members']))
        return 0
    if args.find:
        for name in sorted(stub):
            for method in sorted(stub[name]['methods']):
                if args.find in method:
                    print('%s.%s(%s)' % (name, method, ', '.join(stub[name]['methods'][method]['params'])))
        return 0
    with open(os.path.join(SDK_DIR, 'modules.json'), encoding='utf-8') as f:
        modules = json.load(f)['modules']
    load_script_names(SDK_DIR)
    checker = Checker(stub, modules)
    for root, _dirs, files in os.walk(SDK_DIR):
        for name in sorted(files):
            if name.endswith('.h'):
                checker.check_file(os.path.join(root, name))
    order = {'error': 0, 'warn': 1, 'note': 2}
    checker.problems.sort(key=lambda p: (order[p[0]], p[1], p[2]))
    errors = sum(1 for p in checker.problems if p[0] == 'error')
    notes = sum(1 for p in checker.problems if p[0] == 'note')
    if not args.quiet:
        for level, path, line, message in checker.problems:
            if level != 'note' or args.notes:
                print('%s:%d: %s: %s' % (path, line, level, message))
    checker.checked.setdefault('defaults', 0)
    print('check_reflected: %(classes)d classes, %(structs)d structs, %(functions)d UFUNCTIONs (%(defaults)d with their default arguments), %(properties)d UPROPERTYs, %(enums)d enums checked against the Python stub' % checker.checked
          + '; %d errors, %d warnings, %d not in the stub (--notes lists them)' % (errors, len(checker.problems) - errors - notes, notes))
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())

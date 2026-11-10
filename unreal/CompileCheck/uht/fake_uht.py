#!/usr/bin/env python3
"""Fake UnrealHeaderTool for unreal/CompileCheck (no engine needed).

For every module header that uses reflection macros it writes, into --out:
  <Header>.generated.h   the macros UCLASS() / GENERATED_BODY() expand to (<FileId>_<line>_PROLOG, _GENERATED_BODY):
                         Super / ThisClass / StaticClass, the constructors and destructor UHT declares, exec thunks,
                         the I-interface boilerplate; then CURRENT_FILE_ID, exactly like the real generated header;
  <Header>.gen.cpp       what UHT's .gen.cpp makes the compiler instantiate: the header on its own (after the PCH),
                         the generated constructors / destructor (members of incomplete types fail here), the default
                         constructor call (a non-Abstract UCLASS with pure virtuals fails here) and one call thunk per
                         UFUNCTION.
and it LINTS the UHT rules a compiler cannot see (see RULES). Findings go to stdout and --report (JSON); the exit code
is 1 when there is at least one error-level finding.

It is a line / token scanner, not a C++ parser: it understands the declaration shapes the module uses and reports what
it cannot parse (rule uht-parse) instead of guessing.
"""
import argparse
import bisect
import json
import os
import re
import sys

RULES = {
    'generated-include': 'a header with reflection macros includes "<Header>.generated.h", and it is its LAST #include',
    'generated-body': 'every UCLASS / USTRUCT / UINTERFACE body has GENERATED_BODY()',
    'prefix': 'UObject classes are U-prefixed, actors A-prefixed, structs F, enums E, native interfaces I',
    'class-base': 'a UCLASS derives publicly from exactly one UObject class, listed first',
    'specifier': 'UCLASS / UPROPERTY / UFUNCTION / USTRUCT / UENUM specifiers are ones UHT knows',
    'property-type': 'UPROPERTY / UFUNCTION types are types UHT can reflect (no std::, no namespaces, no nested containers)',
    'enum-underlying': 'a UENUM enum class names its underlying type; BlueprintType enums are uint8',
    'preprocessor': 'no reflection macro inside #if other than WITH_EDITOR / WITH_EDITORONLY_DATA',
    'raw-object-pointer': 'UObject pointer members without UPROPERTY are not seen by the garbage collector (reported)',
    'macro-in-cpp': 'reflection macros only work in headers',
    'header-name': 'reflected header file names are unique in the module',
    'interface': 'a UINTERFACE UFoo has a native IFoo with GENERATED_BODY() in the same header',
    'uht-parse': 'the fake UHT could not parse a declaration (extend uht/fake_uht.py)',
}

CLASS_SPECIFIERS = {
    'Abstract', 'Blueprintable', 'NotBlueprintable', 'BlueprintType', 'NotBlueprintType', 'Transient', 'NonTransient',
    'Config', 'DefaultConfig', 'PerObjectConfig', 'GlobalUserConfig', 'ProjectUserConfig', 'ConfigDoNotCheckDefaults',
    'MinimalAPI', 'Const', 'Deprecated', 'Placeable', 'NotPlaceable', 'DefaultToInstanced', 'EditInlineNew',
    'NotEditInlineNew', 'HideDropdown', 'HideCategories', 'ShowCategories', 'AutoExpandCategories',
    'AutoCollapseCategories', 'DontAutoCollapseCategories', 'CollapseCategories', 'DontCollapseCategories',
    'ClassGroup', 'ComponentWrapperClass', 'HideFunctions', 'ShowFunctions', 'Within', 'CustomConstructor',
    'Intrinsic', 'NoExport', 'Optional', 'AdvancedClassDisplay', 'ConversionRoot', 'Experimental', 'EarlyAccessPreview',
    'SparseClassDataType', 'CustomFieldNotify', 'PrioritizeCategories', 'meta', 'DisplayName', 'Interface',
}
STRUCT_SPECIFIERS = {'BlueprintType', 'NoExport', 'Atomic', 'Immutable', 'HasDefaults', 'HasNoOpConstructor',
                     'IsAlwaysAccessible', 'IsCoreType', 'BlueprintInternalUseOnly', 'meta', 'DisplayName'}
ENUM_SPECIFIERS = {'BlueprintType', 'Flags', 'meta', 'DisplayName'}
INTERFACE_SPECIFIERS = {'MinimalAPI', 'Blueprintable', 'NotBlueprintable', 'BlueprintType', 'ConversionRoot', 'meta',
                        'DependsOn'}
PROPERTY_SPECIFIERS = {
    'EditAnywhere', 'EditDefaultsOnly', 'EditInstanceOnly', 'VisibleAnywhere', 'VisibleDefaultsOnly',
    'VisibleInstanceOnly', 'BlueprintReadOnly', 'BlueprintReadWrite', 'BlueprintAssignable', 'BlueprintCallable',
    'BlueprintAuthorityOnly', 'BlueprintGetter', 'BlueprintSetter', 'Getter', 'Setter', 'FieldNotify', 'Category',
    'Transient', 'DuplicateTransient', 'NonPIEDuplicateTransient', 'TextExportTransient', 'SkipSerialization',
    'Config', 'GlobalConfig', 'SaveGame', 'Replicated', 'ReplicatedUsing', 'NotReplicated', 'RepRetry', 'Instanced',
    'Export', 'NoClear', 'EditFixedSize', 'EditInline', 'AdvancedDisplay', 'SimpleDisplay', 'AssetRegistrySearchable',
    'Interp', 'NonTransactional', 'Localized', 'Native', 'meta', 'DisplayName', 'EditConst',
}
FUNCTION_SPECIFIERS = {
    'BlueprintCallable', 'BlueprintPure', 'BlueprintImplementableEvent', 'BlueprintNativeEvent', 'BlueprintGetter',
    'BlueprintSetter', 'BlueprintAuthorityOnly', 'BlueprintCosmetic', 'BlueprintInternalUseOnly', 'CallInEditor',
    'Category', 'Client', 'Server', 'NetMulticast', 'Reliable', 'Unreliable', 'WithValidation', 'Exec', 'SealedEvent',
    'CustomThunk', 'Variadic', 'ServiceRequest', 'ServiceResponse', 'FieldNotify', 'meta', 'DisplayName',
}
ALLOWED_PP = {'WITH_EDITOR', 'WITH_EDITORONLY_DATA'}
REFLECT_RE = re.compile(r'\b(UCLASS|USTRUCT|UENUM|UINTERFACE|UPROPERTY|UFUNCTION|UDELEGATE|GENERATED_BODY|'
                        r'GENERATED_UCLASS_BODY|GENERATED_USTRUCT_BODY|GENERATED_UINTERFACE_BODY|'
                        r'GENERATED_IINTERFACE_BODY|DECLARE_DYNAMIC_\w+)\s*\(')
BUILTIN_TYPES = {'bool', 'int8', 'int16', 'int32', 'int64', 'uint8', 'uint16', 'uint32', 'uint64', 'float', 'double',
                 'FName', 'FString', 'FText', 'void', 'int'}
UNSUPPORTED_WORDS = {'long', 'short', 'unsigned', 'signed', 'char', 'size_t', 'wchar_t', 'TCHAR', 'ANSICHAR', 'auto'}
CONTAINERS = {'TArray', 'TSet', 'TMap'}
WRAPPERS = {'TObjectPtr', 'TWeakObjectPtr', 'TSoftObjectPtr', 'TSoftClassPtr', 'TSubclassOf', 'TLazyObjectPtr',
            'TScriptInterface', 'TEnumAsByte', 'TOptional', 'TFieldPath', 'TNonNullPtr'}
UNREFLECTABLE_TEMPLATES = {'TSharedPtr', 'TSharedRef', 'TWeakPtr', 'TUniquePtr', 'TFunction', 'TFunctionRef',
                           'TUniqueFunction', 'TVariant', 'TTuple', 'TPair', 'TStaticArray', 'TArrayView',
                           'TConstArrayView', 'TAttribute', 'TWeakInterfacePtr', 'TObjectKey', 'TInlineComponentArray'}


class Finding:
    def __init__(self, level, rule, path, line, message):
        self.level, self.rule, self.path, self.line, self.message = level, rule, path, line, message

    def as_dict(self):
        return {'level': self.level, 'rule': self.rule, 'file': self.path, 'line': self.line, 'message': self.message}


def clean_source(text):
    """Blank comments and literal contents, keeping every newline (so offsets map to the original lines)."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            j = text.find('\n', i)
            j = n if j < 0 else j
            out.append(' ' * (j - i))
            i = j
        elif c == '/' and i + 1 < n and text[i + 1] == '*':
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(''.join('\n' if ch == '\n' else ' ' for ch in text[i:j]))
            i = j
        elif c == '"' or c == "'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == '\\' else 1
                if j < n and text[j - 1] == '\n' and text[j - 2] != '\\':
                    break
            j = min(j + 1, n)
            out.append(c + ''.join('\n' if ch == '\n' else ' ' for ch in text[i + 1:j - 1]) + (c if j - i > 1 else ''))
            i = j
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def match_close(text, i, open_ch, close_ch):
    """text[i] == open_ch -> index of the matching close_ch (or -1)."""
    depth = 0
    for j in range(i, len(text)):
        if text[j] == open_ch:
            depth += 1
        elif text[j] == close_ch:
            depth -= 1
            if depth == 0:
                return j
    return -1


def split_top(text, sep=','):
    """Split at separators outside (), <>, {} and []."""
    parts, depth, cur = [], 0, []
    for ch in text:
        if ch in '(<{[':
            depth += 1
        elif ch in ')>}]':
            depth -= 1
        if ch == sep and depth == 0:
            parts.append(''.join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    last = ''.join(cur).strip()
    if last:
        parts.append(last)
    return parts


class Header:
    def __init__(self, path, rel, text):
        self.path, self.rel = path, rel
        self.raw = text
        self.text = clean_source(text)
        self.line_starts = [0]
        for m in re.finditer('\n', self.text):
            self.line_starts.append(m.end())
        self.pp_at_line = self._preprocessor_map()
        self.classes, self.structs, self.enums = [], [], []

    def line_of(self, offset):
        return bisect.bisect_right(self.line_starts, offset)

    def _preprocessor_map(self):
        """line number -> tuple of the #if conditions that enclose it."""
        stack, result = [], {}
        for number, line in enumerate(self.text.split('\n'), 1):
            stripped = line.strip()
            m = re.match(r'#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)', stripped)
            if m:
                kind, cond = m.group(1), m.group(2).strip()
                if kind in ('if', 'ifdef', 'ifndef'):
                    stack.append(('!' if kind == 'ifndef' else '') + cond)
                elif kind == 'elif' and stack:
                    stack[-1] = cond
                elif kind == 'else' and stack:
                    stack[-1] = '!(' + stack[-1] + ')'
                elif kind == 'endif' and stack:
                    stack.pop()
            result[number] = tuple(stack)
        return result


def specifier_names(args):
    names = []
    for part in split_top(args):
        m = re.match(r'([A-Za-z_]\w*)', part)
        if m:
            names.append(m.group(1))
        elif part:
            names.append(part)
    return names


class Uht:
    def __init__(self, module_dir, out_dir, api_macro, stub_index):
        self.module_dir, self.out_dir, self.api = module_dir, out_dir, api_macro
        self.findings = []
        self.headers = []
        self.stub_types = stub_index  # set of reflected engine type names (U/A/F/E prefixed) or None
        self.module_classes, self.module_structs, self.module_enums = {}, {}, {}
        self.module_plain_types = set()
        self.deferred_types = []

    def report(self, level, rule, header, line, message):
        self.findings.append(Finding(level, rule, header.rel if isinstance(header, Header) else header, line, message))

    # ---- scanning -------------------------------------------------------------------------------------------------
    def load(self):
        for root, _dirs, files in os.walk(self.module_dir):
            for name in sorted(files):
                path = os.path.join(root, name)
                rel = os.path.relpath(path, self.module_dir)
                if name.endswith('.h'):
                    with open(path, encoding='utf-8') as f:
                        self.headers.append(Header(path, rel, f.read()))
                elif name.endswith('.cpp'):
                    with open(path, encoding='utf-8') as f:
                        text = clean_source(f.read())
                    for m in REFLECT_RE.finditer(text):
                        if m.group(1).startswith('DECLARE_DYNAMIC'):
                            continue
                        line = text.count('\n', 0, m.start()) + 1
                        self.findings.append(Finding('error', 'macro-in-cpp', rel, line,
                                                     m.group(1) + ' in a .cpp file: UHT only parses headers'))
        self.headers.sort(key=lambda h: h.rel)
        for header in self.headers:
            for m in re.finditer(r'\b(?:struct|class)\s+(?:[A-Z_]+_API\s+)?([A-Za-z_]\w*)\b[^;{()]*\{', header.text):
                self.module_plain_types.add(m.group(1))
            for m in re.finditer(r'\benum\s+(?:class\s+)?([A-Za-z_]\w*)\b[^;{()]*\{', header.text):
                self.module_plain_types.add(m.group(1))
        for header in self.headers:
            self.parse_header(header)
        for header, line, type_text, what in self.deferred_types:
            self.check_type(header, line, type_text, what)

    def parse_header(self, header):
        text = header.text
        macros = []
        for m in REFLECT_RE.finditer(text):
            close = match_close(text, m.end() - 1, '(', ')')
            if close < 0:
                self.report('error', 'uht-parse', header, header.line_of(m.start()), 'unbalanced ' + m.group(1) + '(')
                continue
            line_start = text.rfind('\n', 0, m.start()) + 1
            if text[line_start:m.start()].lstrip().startswith('#'):
                continue  # inside a #define
            macros.append((m.group(1), m.start(), close, text[m.end():close]))
        header.macros = macros
        header.reflected = bool(macros)
        for name, start, close, args in macros:
            line = header.line_of(start)
            bad = [c for c in header.pp_at_line.get(line, ()) if c.strip('!() ') not in ALLOWED_PP and c not in ('0', '!CPP', 'CPP')]
            if bad and not name.startswith('DECLARE_DYNAMIC'):
                self.report('error', 'preprocessor', header, line,
                            '%s inside "#if %s": UHT only evaluates WITH_EDITOR / WITH_EDITORONLY_DATA' % (name, ' && '.join(bad)))
            if name in ('UCLASS', 'UINTERFACE'):
                self.parse_class(header, name, start, close, args)
            elif name == 'USTRUCT':
                self.parse_struct(header, start, close, args)
            elif name == 'UENUM':
                self.parse_enum(header, start, close, args)
        # native interfaces: class IFoo with GENERATED_BODY and a UINTERFACE UFoo in this header
        for cls in [c for c in header.classes if c['kind'] == 'UINTERFACE']:
            iname = 'I' + cls['name'][1:]
            m = re.search(r'\bclass\s+(?:[A-Z_]+_API\s+)?' + re.escape(iname) + r'\b[^;{]*\{', text)
            if not m:
                self.report('error', 'interface', header, cls['line'], 'UINTERFACE %s has no native class %s in this header' % (cls['name'], iname))
                continue
            body_open = m.end() - 1
            body_close = match_close(text, body_open, '{', '}')
            body = text[body_open + 1:body_close]
            g = re.search(r'\bGENERATED_(?:IINTERFACE_)?BODY\s*\(', body)
            if not g:
                self.report('error', 'generated-body', header, header.line_of(m.start()), 'native interface %s has no GENERATED_BODY()' % iname)
                continue
            cls['native'] = {'name': iname, 'body_line': header.line_of(body_open + 1 + g.start())}

    def _class_head(self, header, kind, close):
        text = header.text
        m = re.compile(r'\s*(class|struct)\s+((?:[A-Z_]+_API\s+)?)([A-Za-z_]\w*)\s*(final\s*)?(:[^{;]*)?\{').match(text, close + 1)
        if not m:
            self.report('error', 'uht-parse', header, header.line_of(close), kind + ' is not followed by a class / struct definition')
            return None
        body_open = m.end() - 1
        body_close = match_close(text, body_open, '{', '}')
        if body_close < 0:
            self.report('error', 'uht-parse', header, header.line_of(body_open), 'unbalanced class body')
            return None
        bases = []
        if m.group(5):
            for part in split_top(m.group(5)[1:]):
                words = part.split()
                access = words[0] if words and words[0] in ('public', 'protected', 'private') else None
                bases.append((access, ' '.join(w for w in words if w not in ('public', 'protected', 'private', 'virtual'))))
        return {'keyword': m.group(1), 'api': m.group(2).strip(), 'name': m.group(3), 'bases': bases,
                'body_open': body_open, 'body_close': body_close, 'line': header.line_of(m.start(3))}

    def parse_class(self, header, kind, start, close, args):
        head = self._class_head(header, kind, close)
        if not head:
            return
        specs = specifier_names(args)
        allowed = INTERFACE_SPECIFIERS if kind == 'UINTERFACE' else CLASS_SPECIFIERS
        for s in specs:
            if s not in allowed:
                self.report('error', 'specifier', header, header.line_of(start), 'unknown %s specifier "%s"' % (kind, s))
        cls = dict(head, kind=kind, specs=specs, macro_line=header.line_of(start), header=header,
                   properties=[], functions=[], ctors=[], has_dtor=False, body_line=None, native=None)
        if head['keyword'] != 'class':
            self.report('error', 'class-base', header, cls['line'], '%s %s must be declared with "class"' % (kind, cls['name']))
        if not head['bases']:
            self.report('error', 'class-base', header, cls['line'], '%s %s has no base class (it must derive from a UObject class)' % (kind, cls['name']))
        else:
            access, base = head['bases'][0]
            if access != 'public':
                self.report('error', 'class-base', header, cls['line'], '%s must derive publicly from %s' % (cls['name'], base))
            if not re.match(r'[UA][A-Z]', base):
                self.report('error', 'class-base', header, cls['line'], 'the first base of %s is "%s": the UObject base class must come first' % (cls['name'], base))
            for _access, other in head['bases'][1:]:
                if re.match(r'[UA][A-Z]', other):
                    self.report('error', 'class-base', header, cls['line'], '%s derives from a second UObject class %s' % (cls['name'], other))
            cls['super'] = base
        self.parse_body(header, cls)
        if cls['body_line'] is None:
            self.report('error', 'generated-body', header, cls['line'], '%s %s has no GENERATED_BODY()' % (kind, cls['name']))
        header.classes.append(cls)
        self.module_classes[cls['name']] = cls

    def parse_struct(self, header, start, close, args):
        head = self._class_head(header, 'USTRUCT', close)
        if not head:
            return
        for s in specifier_names(args):
            if s not in STRUCT_SPECIFIERS:
                self.report('error', 'specifier', header, header.line_of(start), 'unknown USTRUCT specifier "%s"' % s)
        st = dict(head, kind='USTRUCT', specs=specifier_names(args), macro_line=header.line_of(start), header=header,
                  properties=[], functions=[], ctors=[], has_dtor=False, body_line=None)
        if head['keyword'] != 'struct':
            self.report('error', 'class-base', header, st['line'], 'USTRUCT %s must be declared with "struct"' % st['name'])
        if not st['name'].startswith('F'):
            self.report('error', 'prefix', header, st['line'], 'struct %s has an invalid Unreal prefix, expecting F%s' % (st['name'], st['name']))
        st['super'] = head['bases'][0][1] if head['bases'] else None
        self.parse_body(header, st)
        if st['body_line'] is None:
            self.report('error', 'generated-body', header, st['line'], 'USTRUCT %s has no GENERATED_BODY()' % st['name'])
        header.structs.append(st)
        self.module_structs[st['name']] = st

    def parse_enum(self, header, start, close, args):
        text = header.text
        specs = specifier_names(args)
        for s in specs:
            if s not in ENUM_SPECIFIERS:
                self.report('error', 'specifier', header, header.line_of(start), 'unknown UENUM specifier "%s"' % s)
        m = re.compile(r'\s*(?:namespace\s+(\w+)\s*\{\s*)?enum\s+(class\s+|struct\s+)?(\w+)\s*(?::\s*([\w:]+)\s*)?\{').match(text, close + 1)
        if not m:
            self.report('error', 'uht-parse', header, header.line_of(close), 'UENUM is not followed by an enum definition')
            return
        name = m.group(1) or m.group(3)
        line = header.line_of(m.start(3))
        scoped, underlying = bool(m.group(2)), m.group(4)
        if not name.startswith('E'):
            self.report('warn', 'prefix', header, line, 'enum %s is not E-prefixed' % name)
        if scoped and not underlying:
            self.report('error', 'enum-underlying', header, line, 'UENUM enum class %s must name its underlying type (": uint8")' % name)
        if 'BlueprintType' in specs and scoped and underlying and underlying != 'uint8':
            self.report('error', 'enum-underlying', header, line, 'BlueprintType enum %s must be uint8 (is %s)' % (name, underlying))
        if underlying and underlying not in ('uint8', 'int8', 'uint16', 'int16', 'uint32', 'int32', 'uint64', 'int64'):
            self.report('error', 'enum-underlying', header, line, 'UENUM %s has unsupported underlying type %s' % (name, underlying))
        enum = {'name': name, 'line': line, 'scoped': scoped, 'underlying': underlying, 'header': header}
        header.enums.append(enum)
        self.module_enums[name] = enum

    def parse_body(self, header, cls):
        """Walk the top-level statements of a class body."""
        text = header.text
        i, end = cls['body_open'] + 1, cls['body_close']
        pending = None  # (macro name, args, line) waiting for its declaration
        stmt_start = i
        while i < end:
            ch = text[i]
            if ch == '#':  # preprocessor line
                i = text.find('\n', i)
                i = end if i < 0 else i + 1
                stmt_start = i
                continue
            if ch == '(':
                i = match_close(text, i, '(', ')') + 1
                if i <= 0:
                    self.report('error', 'uht-parse', header, header.line_of(stmt_start), 'unbalanced ( in the body of ' + cls['name'])
                    return
                head = text[stmt_start:i]
                m = re.match(r'\s*(?:(?:public|protected|private)\s*:\s*)*(UPROPERTY|UFUNCTION|GENERATED_BODY|GENERATED_UCLASS_BODY|GENERATED_USTRUCT_BODY|GENERATED_UINTERFACE_BODY)\s*\(', head, re.S)
                if m and head.rstrip().endswith(')') and head.count('(') == head.count(')') and m.end() - 1 == head.find('('):
                    name = m.group(1)
                    line = header.line_of(stmt_start + m.start(1))
                    macro_args = head[m.end():-1] if head.rstrip() == head else head[m.end():head.rstrip().__len__() - 1]
                    if name.startswith('GENERATED'):
                        cls['body_line'] = line
                        cls['legacy_body'] = name != 'GENERATED_BODY' and name != 'GENERATED_USTRUCT_BODY'
                        # an optional ';' after GENERATED_BODY()
                        j = i
                        while j < end and text[j] in ' \t\r\n':
                            j += 1
                        if j < end and text[j] == ';':
                            i = j + 1
                    else:
                        if pending:
                            self.report('error', 'uht-parse', header, pending[2], pending[0] + ' is not followed by a declaration')
                        pending = (name, macro_args, line)
                    stmt_start = i
                continue
            if ch == '{':
                close = match_close(text, i, '{', '}')
                if close < 0:
                    self.report('error', 'uht-parse', header, header.line_of(i), 'unbalanced { in the body of ' + cls['name'])
                    return
                head = text[stmt_start:i]
                is_function = bool(re.search(r'\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?:final\s*)?(?:->\s*[\w:<>&*\s]+)?(?::[^{]*)?$', head.strip())) \
                    and not re.match(r'\s*(?:(?:public|protected|private)\s*:\s*)*(?:struct|class|enum|union|namespace)\b', head)
                if is_function:
                    self.statement(header, cls, text[stmt_start:i], stmt_start, pending, inline_body=True)
                    pending = None
                    i = close + 1
                    stmt_start = i
                else:
                    i = close + 1  # nested type or brace initialiser: the statement continues to ';'
                continue
            if ch == ';':
                self.statement(header, cls, text[stmt_start:i], stmt_start, pending, inline_body=False)
                pending = None
                i += 1
                stmt_start = i
                continue
            i += 1
        if pending:
            self.report('error', 'uht-parse', header, pending[2], pending[0] + ' is not followed by a declaration')

    def statement(self, header, cls, stmt, offset, pending, inline_body):
        m = re.match(r'(\s*(?:(?:public|protected|private)\s*:\s*)*)', stmt)
        lead = m.end()
        body = stmt[lead:]
        line = header.line_of(offset + lead + (len(body) - len(body.lstrip())))
        decl = ' '.join(body.split())
        if not decl:
            return
        name = cls['name']
        # constructors / destructor
        if re.match(r'(?:virtual\s+)?~' + re.escape(name) + r'\s*\(', decl):
            cls['has_dtor'] = True
            return
        m = re.match(r'(?:explicit\s+)?' + re.escape(name) + r'\s*\((.*)\)', decl)
        if m and not decl.startswith('friend'):
            cls['ctors'].append(m.group(1).strip())
            return
        if pending and pending[0] == 'UFUNCTION':
            self.function(header, cls, decl, pending, line, inline_body)
            return
        if pending and pending[0] == 'UPROPERTY':
            self.property(header, cls, decl, pending, line)
            return
        if re.match(r'(?:friend|using|typedef|static_assert|template|enum|struct|class|union)\b', decl) or '(' in decl.split('=')[0]:
            return
        # a data member without UPROPERTY
        mtype = re.sub(r'\s*=.*$', '', decl)
        mm = re.match(r'(.*?)([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*(?::\s*\d+)?\s*(?:\{.*\})?$', mtype)
        if not mm:
            return
        type_text, member = mm.group(1).strip(), mm.group(2)
        if type_text.startswith('static ') or 'constexpr' in type_text:
            return
        if self.holds_object_pointer(type_text):
            self.report('warn', 'raw-object-pointer', header, line,
                        '%s::%s (%s) holds UObject pointers without UPROPERTY: the garbage collector does not see it'
                        % (name, member, type_text))

    @staticmethod
    def holds_object_pointer(type_text):
        if re.search(r'\bTWeakObjectPtr\b|\bTSoftObjectPtr\b|\bTWeakInterfacePtr\b|\bTObjectKey\b|\bTStrongObjectPtr\b', type_text) \
                and not re.search(r'\bTObjectPtr\b|\b[UA][A-Z]\w*\s*\*', re.sub(r'\b(?:TWeakObjectPtr|TSoftObjectPtr|TWeakInterfacePtr|TObjectKey|TStrongObjectPtr)\s*<[^<>]*>', '', type_text)):
            return False
        return bool(re.search(r'\bTObjectPtr\s*<', type_text) or re.search(r'\b[UA][A-Z]\w*\s*\*', type_text))

    # ---- reflected members ------------------------------------------------------------------------------------------
    def check_type(self, header, line, type_text, what):
        """UHT-reflectable type? Reports and returns False when not."""
        t = ' '.join(type_text.replace('const ', ' ').replace('&', ' ').split())
        t = re.sub(r'\b(?:class|struct|enum)\s+', '', t).strip()
        ok = True
        if 'std::' in t or re.search(r'\b[a-z_]\w*::', t):
            self.report('error', 'property-type', header, line, '%s: "%s" is not a reflected type (UHT does not know namespaces / std types)' % (what, type_text))
            return False
        for word in re.findall(r'[A-Za-z_]\w*', t):
            if word in UNSUPPORTED_WORDS:
                self.report('error', 'property-type', header, line, '%s: "%s" uses "%s" (use the sized Unreal types)' % (what, type_text, word))
                ok = False
            elif word in UNREFLECTABLE_TEMPLATES:
                self.report('error', 'property-type', header, line, '%s: "%s": %s cannot be reflected' % (what, type_text, word))
                ok = False
        m = re.match(r'(\w+)\s*<(.*)>\s*\*?$', t)
        if m and m.group(1) in CONTAINERS:
            inner = split_top(m.group(2))
            if m.group(1) == 'TMap' and len(inner) != 2:
                self.report('error', 'property-type', header, line, '%s: TMap needs exactly a key and a value type ("%s")' % (what, type_text))
                ok = False
            for part in inner:
                if re.match(r'(TArray|TSet|TMap)\s*<', part):
                    self.report('error', 'property-type', header, line, '%s: "%s": containers cannot be nested directly (wrap the inner one in a USTRUCT)' % (what, type_text))
                    ok = False
                elif re.match(r'T(?:Inline|Fixed)Allocator\b', part):
                    self.report('error', 'property-type', header, line, '%s: "%s": UPROPERTY containers cannot use a custom allocator' % (what, type_text))
                    ok = False
                else:
                    ok = self.check_type(header, line, part, what) and ok
            return ok
        if m and m.group(1) in WRAPPERS:
            inner = m.group(2).strip()
            if m.group(1) == 'TObjectPtr' and re.match(r'I[A-Z]', inner):
                self.report('error', 'property-type', header, line, '%s: TObjectPtr<%s>: interfaces are referenced with TScriptInterface' % (what, inner))
                return False
            if m.group(1) in ('TObjectPtr', 'TWeakObjectPtr', 'TSoftObjectPtr', 'TSubclassOf', 'TSoftClassPtr') and not re.match(r'[UA][A-Z]\w*$', inner):
                self.report('error', 'property-type', header, line, '%s: %s<%s>: the argument must be a UObject class' % (what, m.group(1), inner))
                return False
            return self.check_named(header, line, inner, what) and ok
        base = t.rstrip('* ').strip()
        if '<' in base:
            self.report('error', 'property-type', header, line, '%s: "%s": template type UHT cannot reflect' % (what, type_text))
            return False
        return self.check_named(header, line, base, what) and ok

    def check_named(self, header, line, name, what):
        if name in BUILTIN_TYPES or not name:
            return True
        if name in self.module_classes or name in self.module_structs or name in self.module_enums:
            return True
        if name in self.module_plain_types:
            self.report('error', 'property-type', header, line, '%s: %s is declared in this module without UCLASS / USTRUCT / UENUM (UHT: "Unrecognized type")' % (what, name))
            return False
        if self.stub_types is not None and re.match(r'[UAFE][A-Z]', name) and name[1:] not in self.stub_types:
            self.report('warn', 'property-type', header, line, '%s: engine type %s is not in the reflected-type index (Python stub): is it a UCLASS / USTRUCT / UENUM?' % (what, name))
        return True

    def property(self, header, cls, decl, pending, line):
        _name, args, macro_line = pending
        for s in specifier_names(args):
            if s not in PROPERTY_SPECIFIERS:
                self.report('error', 'specifier', header, macro_line, 'unknown UPROPERTY specifier "%s"' % s)
        text = re.sub(r'\s*=.*$', '', decl)
        text = re.sub(r'\s*\{.*\}\s*$', '', text)
        m = re.match(r'(.*?)([A-Za-z_]\w*)\s*(\[[^\]]*\])?\s*(:\s*\d+)?$', text)
        if not m or '(' in text:
            self.report('error', 'uht-parse', header, line, 'UPROPERTY is not followed by a member variable: "%s"' % decl)
            return
        type_text, member = m.group(1).strip(), m.group(2)
        if re.match(r'(static|constexpr|mutable)\b', type_text):
            self.report('error', 'property-type', header, line, 'UPROPERTY %s::%s cannot be %s' % (cls['name'], member, type_text.split()[0]))
        if type_text.endswith('&'):
            self.report('error', 'property-type', header, line, 'UPROPERTY %s::%s cannot be a reference' % (cls['name'], member))
        self.deferred_types.append((header, line, type_text, 'UPROPERTY %s::%s' % (cls['name'], member)))
        cls['properties'].append({'name': member, 'type': type_text, 'line': line, 'specs': specifier_names(args)})

    def function(self, header, cls, decl, pending, line, inline_body):
        _name, args, macro_line = pending
        specs = specifier_names(args)
        for s in specs:
            if s not in FUNCTION_SPECIFIERS:
                self.report('error', 'specifier', header, macro_line, 'unknown UFUNCTION specifier "%s"' % s)
        if decl.startswith('template'):
            self.report('error', 'property-type', header, line, 'a UFUNCTION cannot be a template')
            return
        open_paren = decl.find('(')
        close_paren = match_close(decl, open_paren, '(', ')') if open_paren >= 0 else -1
        if open_paren < 0 or close_paren < 0:
            self.report('error', 'uht-parse', header, line, 'UFUNCTION is not followed by a function declaration: "%s"' % decl)
            return
        head = decl[:open_paren].strip()
        tail = decl[close_paren + 1:]
        words = head.split()
        fname = words[-1].lstrip('*&')
        ret = ' '.join(words[:-1])
        is_static = bool(re.search(r'\bstatic\b', ret))
        is_virtual = bool(re.search(r'\bvirtual\b', ret))
        ret = re.sub(r'\b(static|virtual|inline|FORCEINLINE|constexpr|[A-Z_]+_API)\b', '', ret).strip()
        ret = ret + ('*' * (len(words[-1]) - len(words[-1].lstrip('*'))))
        params = []
        for part in split_top(decl[open_paren + 1:close_paren]):
            if part == 'void':
                continue
            part = re.sub(r'\bUPARAM\s*\([^)]*\)\s*', '', part)
            default = None
            if '=' in part:
                part, default = [x.strip() for x in part.split('=', 1)]
            pm = re.match(r'(.*?)([A-Za-z_]\w*)$', part.strip())
            if not pm or not pm.group(1).strip():
                self.report('error', 'property-type', header, line, 'UFUNCTION %s::%s: parameter "%s" needs a name' % (cls['name'], fname, part))
                continue
            ptype = pm.group(1).strip()
            params.append({'type': ptype, 'name': pm.group(2), 'default': default})
            self.deferred_types.append((header, line, ptype, 'UFUNCTION %s::%s parameter %s' % (cls['name'], fname, pm.group(2))))
        if ret != 'void':
            self.deferred_types.append((header, line, ret, 'UFUNCTION %s::%s return type' % (cls['name'], fname)))
        if any(f['name'] == fname for f in cls['functions']):
            self.report('error', 'property-type', header, line, 'UFUNCTION %s::%s is overloaded: UFUNCTION names must be unique in a class' % (cls['name'], fname))
        if 'BlueprintPure' in specs and ret == 'void' and not any(p['type'].endswith('&') and not p['type'].startswith('const') for p in params):
            self.report('error', 'specifier', header, macro_line, 'BlueprintPure %s::%s returns nothing' % (cls['name'], fname))
        if 'BlueprintImplementableEvent' in specs and inline_body:
            self.report('error', 'specifier', header, macro_line, 'BlueprintImplementableEvent %s::%s must not have a body' % (cls['name'], fname))
        cls['functions'].append({'name': fname, 'ret': ret, 'params': params, 'static': is_static, 'virtual': is_virtual,
                                 'const': bool(re.match(r'\s*const\b', tail)), 'specs': specs, 'line': line})

    # ---- header-level lint ------------------------------------------------------------------------------------------
    def lint_headers(self):
        seen = {}
        for header in self.headers:
            if not header.reflected:
                # raw UObject pointers in non-reflected types are listed as notes
                self.note_plain_pointers(header)
                continue
            base = os.path.basename(header.rel)
            if base in seen:
                self.report('error', 'header-name', header, 1, 'two reflected headers named %s (%s): UHT writes one %s.generated.h' % (base, seen[base], base[:-2]))
            seen[base] = header.rel
            expected = base[:-2] + '.generated.h'
            includes = [(header.line_of(m.start()), m.group(1)) for m in re.finditer(r'^[ \t]*#[ \t]*include[ \t]*[<"]([^>"]*)[>"]', header.raw, re.M)]
            generated = [(l, inc) for l, inc in includes if inc.endswith('.generated.h')]
            if not generated:
                self.report('error', 'generated-include', header, 1, 'missing #include "%s"' % expected)
            else:
                gline, ginc = generated[0]
                if os.path.basename(ginc) != expected or '/' in ginc:
                    self.report('error', 'generated-include', header, gline, 'includes "%s", expected "%s"' % (ginc, expected))
                later = [l for l, inc in includes if l > gline]
                if later:
                    self.report('error', 'generated-include', header, later[0], '#include after "%s": the generated header must be the last include' % expected)
                first_macro = min(header.line_of(start) for _n, start, _c, _a in header.macros)
                if gline > first_macro:
                    self.report('error', 'generated-include', header, gline, '"%s" is included after the first reflection macro (line %d)' % (expected, first_macro))
            for cls in header.classes:
                self.lint_class(header, cls)
            self.note_plain_pointers(header)

    def note_plain_pointers(self, header):
        text = header.text
        reflected_spans = [(c['body_open'], c['body_close']) for c in header.classes + header.structs]
        for m in re.finditer(r'\b(class|struct)\s+(?:[A-Z_]+_API\s+)?([A-Za-z_]\w*)\b(?:\s*final)?\s*(?::[^{;]*)?\{', text):
            body_open = m.end() - 1
            if any(a <= body_open <= b for a, b in reflected_spans):
                continue
            body_close = match_close(text, body_open, '{', '}')
            if body_close < 0:
                continue
            fake = {'name': m.group(2), 'body_open': body_open, 'body_close': body_close, 'properties': [], 'functions': [],
                    'ctors': [], 'has_dtor': False, 'body_line': None}
            before = len(self.findings)
            self.parse_body(header, fake)
            for f in self.findings[before:]:
                if f.rule == 'raw-object-pointer':
                    f.level = 'note'
                    f.message = f.message.replace('the garbage collector does not see it',
                                                  'not a reflected type: keep it weak (TWeakObjectPtr) or owned by a UObject / FGCObject')
            self.findings[before:] = [f for f in self.findings[before:] if f.rule == 'raw-object-pointer']

    def is_actor(self, name, depth=0):
        if depth > 16:
            return False
        if name in self.module_classes:
            return self.is_actor(self.module_classes[name].get('super', ''), depth + 1)
        return name.startswith('A')

    def lint_class(self, header, cls):
        name, sup = cls['name'], cls.get('super', '')
        if cls['kind'] == 'UINTERFACE':
            if not name.startswith('U'):
                self.report('error', 'prefix', header, cls['line'], 'interface class %s has an invalid Unreal prefix, expecting U' % name)
            if sup != 'UInterface' and not (sup in self.module_classes and self.module_classes[sup]['kind'] == 'UINTERFACE'):
                self.report('error', 'class-base', header, cls['line'], 'UINTERFACE %s must derive from UInterface' % name)
            if cls['properties'] or cls['functions']:
                self.report('error', 'interface', header, cls['line'], 'UINTERFACE %s must be empty: declare the functions in I%s' % (name, name[1:]))
            return
        expected = 'A' if self.is_actor(sup) else 'U'
        if not re.match(expected + r'[A-Z]', name):
            self.report('error', 'prefix', header, cls['line'], 'class %s has an invalid Unreal prefix, expecting %s%s' % (name, expected, name.lstrip('UA')))

    # ---- output -----------------------------------------------------------------------------------------------------
    def file_id(self, header):
        return 'FID_' + re.sub(r'[^A-Za-z0-9]', '_', 'Abyssfire/Source/Abyssfire/' + header.rel.replace(os.sep, '/'))

    def write(self):
        os.makedirs(self.out_dir, exist_ok=True)
        wanted = set()
        gen_units = []
        for header in self.headers:
            if not header.reflected:
                continue
            base = os.path.basename(header.rel)[:-2]
            gen_h = os.path.join(self.out_dir, base + '.generated.h')
            gen_cpp = os.path.join(self.out_dir, base + '.gen.cpp')
            self._write_if_changed(gen_h, self.generated_header(header, base))
            self._write_if_changed(gen_cpp, self.generated_cpp(header, base))
            wanted.update((gen_h, gen_cpp))
            gen_units.append({'header': header.rel, 'gen_cpp': gen_cpp})
        for name in os.listdir(self.out_dir):
            path = os.path.join(self.out_dir, name)
            if (name.endswith('.generated.h') or name.endswith('.gen.cpp')) and path not in wanted:
                os.remove(path)
        return gen_units

    @staticmethod
    def _write_if_changed(path, content):
        if os.path.exists(path):
            with open(path, encoding='utf-8') as f:
                if f.read() == content:
                    return
        with open(path, 'w', encoding='utf-8') as f:
            f.write(content)

    def include_path(self, header):
        rel = header.rel.replace(os.sep, '/')
        for prefix in ('Public/', 'Private/', 'Classes/'):
            if rel.startswith(prefix):
                return rel[len(prefix):]
        return rel

    def generated_header(self, header, base):
        fid = self.file_id(header)
        guard = 'ABYSSFIRE_%s_generated_h' % base
        out = ['// Generated by unreal/CompileCheck/uht/fake_uht.py from %s. Do not edit.' % header.rel.replace(os.sep, '/'),
               '// IWYU pragma: private, include "%s"' % self.include_path(header),
               '#include "UObject/ObjectMacros.h"', '#include "UObject/ScriptMacros.h"', '',
               'PRAGMA_DISABLE_DEPRECATION_WARNINGS',
               '#ifdef %s' % guard,
               '#error "%s.generated.h already included, missing \'#pragma once\' in %s.h"' % (base, base),
               '#endif', '#define %s' % guard, '']
        for st in header.structs:
            if st['body_line'] is None:
                continue
            lines = ['\tfriend struct Z_Construct_UScriptStruct_%s_Statics;' % st['name'],
                     '\t%s static class UScriptStruct* StaticStruct();' % self.api]
            if st.get('super'):
                lines.append('\ttypedef %s Super;' % st['super'])
            out.append('#define %s_%d_GENERATED_BODY \\\n%s' % (fid, st['body_line'], ' \\\n'.join(lines)))
            out.append('struct %s;' % st['name'])
            out.append('template <> %s UScriptStruct* StaticStruct<struct %s>();' % (self.api, st['name']))
            out.append('')
        for cls in header.classes:
            out.append('#define %s_%d_PROLOG' % (fid, cls['macro_line']))
            if cls['body_line'] is not None:
                name, sup = cls['name'], cls.get('super', 'UObject')
                lines = ['PRAGMA_DISABLE_DEPRECATION_WARNINGS', 'public:']
                for fn in cls['functions']:
                    lines.append('\tDECLARE_FUNCTION(exec%s);' % fn['name'])
                lines.append('\tUE_STUB_DECLARE_CLASS(%s, %s)' % (name, sup))
                abstract = 'Abstract' in cls['specs'] or cls['kind'] == 'UINTERFACE'
                if not cls['ctors']:
                    lines.append('\t%s(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());' % name)
                    if not abstract:
                        lines.append('\tstatic void __DefaultConstructor(const FObjectInitializer& X) { new ((EInternal*)X.GetObj()) %s(X); }' % name)
                elif not abstract:
                    takes_initializer = any('FObjectInitializer' in c for c in cls['ctors'])
                    lines.append('\tstatic void __DefaultConstructor(const FObjectInitializer& X) { new ((EInternal*)X.GetObj()) %s%s; }'
                                 % (name, '(X)' if takes_initializer and not any(c == '' for c in cls['ctors']) else ''))
                if not cls['has_dtor']:
                    lines.append('\tvirtual ~%s();' % name)
                lines += ['public:' if cls.get('legacy_body') else 'private:', 'PRAGMA_ENABLE_DEPRECATION_WARNINGS']
                out.append('#define %s_%d_GENERATED_BODY%s \\\n%s' % (fid, cls['body_line'], '_LEGACY' if cls.get('legacy_body') else '', ' \\\n'.join(lines)))
                out.append('class %s;' % name)
                out.append('template <> %s UClass* StaticClass<class %s>();' % (self.api, name))
            native = cls.get('native')
            if native:
                lines = ['PRAGMA_DISABLE_DEPRECATION_WARNINGS', 'public:', 'protected:', '\tvirtual ~%s() {}' % native['name'],
                         'public:', '\ttypedef %s UClassType;' % cls['name'], '\ttypedef %s ThisClass;' % native['name'],
                         '\tvirtual UObject* _getUObject() const { return nullptr; }', 'private:',
                         'PRAGMA_ENABLE_DEPRECATION_WARNINGS']
                out.append('#define %s_%d_GENERATED_BODY \\\n%s' % (fid, native['body_line'], ' \\\n'.join(lines)))
            out.append('')
        for enum in header.enums:
            decl = 'enum class %s%s;' % (enum['name'], (' : ' + enum['underlying']) if enum['underlying'] else '') if enum['scoped'] else None
            if decl:
                out.append(decl)
                out.append('template <> %s UEnum* StaticEnum<%s>();' % (self.api, enum['name']))
        out += ['', '#undef CURRENT_FILE_ID', '#define CURRENT_FILE_ID %s' % fid, '', 'PRAGMA_ENABLE_DEPRECATION_WARNINGS', '']
        return '\n'.join(out)

    def generated_cpp(self, header, base):
        out = ['// Generated by unreal/CompileCheck/uht/fake_uht.py from %s. Do not edit.' % header.rel.replace(os.sep, '/'),
               '// The parts of UHT\'s %s.gen.cpp that make the compiler look at the header on its own.' % base,
               '#include "UObject/GeneratedCppIncludes.h"', '#include "%s"' % self.include_path(header), '',
               'PRAGMA_DISABLE_DEPRECATION_WARNINGS', '']
        for cls in header.classes:
            if cls['body_line'] is None:
                continue
            name = cls['name']
            if not cls['ctors']:
                out.append('%s::%s(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}' % (name, name))
            out.append('%s::%s(FVTableHelper& Helper) : Super(Helper) {}' % (name, name))
            if not cls['has_dtor']:
                out.append('%s::~%s() {}' % (name, name))
            for fn in cls['functions']:
                out.append('DEFINE_FUNCTION(%s::exec%s)' % (name, fn['name']))
                out.append('{')
                call_args = []
                for p in fn['params']:
                    out.append('\tstd::remove_cvref_t<%s> Z_Param_%s{};' % (p['type'], p['name']))
                    call_args.append('Z_Param_%s' % p['name'])
                target = ('%s::' % name) if fn['static'] else ('static_cast<%s%s*>(Context)->' % ('const ' if fn['const'] else '', name))
                call = '%s%s(%s)' % (target, fn['name'], ', '.join(call_args))
                if fn['ret'] == 'void':
                    out.append('\t%s;' % call)
                else:
                    out.append('\t*(std::remove_cvref_t<%s>*)Z_Param__Result = %s;' % (fn['ret'], call))
                out.append('\t(void)Stack;')
                out.append('}')
            out.append('')
        out += ['PRAGMA_ENABLE_DEPRECATION_WARNINGS', '']
        return '\n'.join(out)


def load_stub_types(stub_path, cache_path):
    """Names of the reflected engine types (Python stub class names). Cached next to the build output."""
    if not stub_path or not os.path.exists(stub_path):
        return None
    stamp = '%d:%d' % (os.path.getsize(stub_path), int(os.path.getmtime(stub_path)))
    if cache_path and os.path.exists(cache_path):
        try:
            with open(cache_path, encoding='utf-8') as f:
                cached = json.load(f)
            if cached.get('stamp') == stamp:
                return set(cached['types'])
        except (ValueError, KeyError):
            pass
    names = set()
    with open(stub_path, encoding='utf-8') as f:
        for line in f:
            if line.startswith('class '):
                m = re.match(r'class (\w+)', line)
                if m:
                    names.add(m.group(1))
    if cache_path:
        with open(cache_path, 'w', encoding='utf-8') as f:
            json.dump({'stamp': stamp, 'types': sorted(names)}, f)
    return names


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--module', required=True, help='module source directory (contains Public / Private)')
    parser.add_argument('--out', required=True, help='directory for the .generated.h / .gen.cpp files')
    parser.add_argument('--api', default='ABYSSFIRE_API')
    parser.add_argument('--report', help='write the findings as JSON here')
    parser.add_argument('--stub', help='engine Python stub (unreal.py): reflected engine type names for the type lint')
    parser.add_argument('--quiet', action='store_true', help='print only the summary line')
    args = parser.parse_args()

    os.makedirs(args.out, exist_ok=True)
    stub_types = load_stub_types(args.stub, os.path.join(args.out, 'stub_types.json') if args.stub else None)
    uht = Uht(os.path.abspath(args.module), os.path.abspath(args.out), args.api, stub_types)
    uht.load()
    uht.lint_headers()
    gen_units = uht.write()
    order = {'error': 0, 'warn': 1, 'note': 2}
    uht.findings.sort(key=lambda f: (order[f.level], f.path, f.line))
    counts = {level: sum(1 for f in uht.findings if f.level == level) for level in order}
    if not args.quiet:
        for f in uht.findings:
            print('%s:%d: %s: [uht/%s] %s' % (f.path, f.line, f.level, f.rule, f.message))
    print('fake UHT: %d reflected headers, %d classes, %d structs, %d enums; %d errors, %d warnings, %d notes'
          % (len(gen_units), len(uht.module_classes), len(uht.module_structs), len(uht.module_enums),
             counts['error'], counts['warn'], counts['note']))
    if args.report:
        with open(args.report, 'w', encoding='utf-8') as f:
            json.dump({'rules': RULES, 'counts': counts, 'generated': gen_units,
                       'findings': [x.as_dict() for x in uht.findings]}, f, indent=1)
    return 1 if counts['error'] else 0


if __name__ == '__main__':
    sys.exit(main())

#!/usr/bin/env python3
"""AST lints over the whole module: what `clang++ -fsyntax-only`, one translation unit at a time, cannot see.

    python3 tools/link_scan.py                       # every configuration of the last ./run.sh (its build directory)
    python3 tools/link_scan.py --config mac-editor   # one configuration
    ./run.sh                                         # runs it after the compiles (--no-link-scan skips it)

Every module .cpp and every fake-UHT .gen.cpp is parsed with the system libclang (ctypes, no Python package) and the
command line of the compile check; the facts of all translation units of one configuration are then put together, as
the linker (and a unity blob) would. Findings:

  unity-collision   error  the same internal-linkage name (file-scope `static`, anonymous namespace) is defined in two
                           .cpp files of the module. UBT compiles a game module in unity blobs: there the two meet in one
                           translation unit. A redefinition fails the unity build (the unity unit of the compile check
                           reports those); two functions with different parameters do NOT fail: they silently become one
                           overload set, and a call can bind to the other file's helper in a unity build only. The same
                           for two helpers with external linkage that no header declares (same name in the same named
                           namespace, different parameters, two .cpp files).
  undefined         error  a function / static member / extern variable is declared in a module file, used (or virtual)
                           and defined in no translation unit: an undefined symbol at link time.
  duplicate         error  the same non-inline function / variable is defined in two files: a duplicate symbol.
  header-definition error  a non-inline, non-template definition with external linkage in a header: a duplicate symbol
                           as soon as two .cpp files include it (and a redefinition never, thanks to #pragma once).
  core-export       error  (modular configurations: the editor) a non-inline AbyssCore function / variable the module
                           uses is not exported (ABYSS_API): an undefined symbol when AbyssCore is its own library.
  platform-name     error  a global / anonymous-namespace name of the module equals a name of Apple's MacTypes.h /
                           Objective-C headers (Rect, Point, Cell, Size, Ptr, ...): every Apple translation unit is
                           Objective-C++ with those in scope, so the name is ambiguous or redefined on the Mac only.
  lp64-mix          error  one call of min / max / clamp (std:: or FMath::) mixes a std fixed-width 64-bit type
                           (std::int64_t / std::uint64_t: `long` on Linux and Android, `long long` on macOS / iOS) with
                           size_t / ptrdiff_t / long: one type here, two types on the Mac, where the template argument
                           deduction then fails.

The scan cannot see engine symbols (whether an engine function is exported from its module) or the real linker's view
of the C# build files; see README.md, "What a PASS proves, and what it cannot".
"""
import argparse
import concurrent.futures
import ctypes
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)
import compile_check as cc  # noqa: E402
import sdk_usage  # noqa: E402

CXCursor, CXString, CXSourceLocation, VISITOR = sdk_usage.CXCursor, sdk_usage.CXString, sdk_usage.CXSourceLocation, sdk_usage.VISITOR


class CXType(ctypes.Structure):
    _fields_ = [('kind', ctypes.c_int), ('data', ctypes.c_void_p * 2)]


class CXSourceRange(ctypes.Structure):
    _fields_ = [('ptr_data', ctypes.c_void_p * 2), ('begin_int_data', ctypes.c_uint), ('end_int_data', ctypes.c_uint)]


# CXCursorKind (clang-c/Index.h)
K_STRUCT, K_UNION, K_CLASS, K_ENUM, K_FIELD, K_ENUM_CONST, K_FUNCTION, K_VAR = 2, 3, 4, 5, 6, 7, 8, 9
K_TYPEDEF, K_METHOD, K_NAMESPACE, K_CTOR, K_DTOR, K_CONVERSION = 20, 21, 22, 24, 25, 26
K_FUNCTION_TEMPLATE, K_CLASS_TEMPLATE, K_CLASS_PARTIAL, K_ALIAS, K_TU = 30, 31, 32, 36, 350
K_CALL = 103
FUNCTION_KINDS = {K_FUNCTION, K_METHOD, K_CTOR, K_DTOR, K_CONVERSION, K_FUNCTION_TEMPLATE}
TYPE_KINDS = {K_STRUCT, K_UNION, K_CLASS, K_ENUM, K_TYPEDEF, K_ALIAS, K_CLASS_TEMPLATE}
CLASS_KINDS = {K_STRUCT, K_UNION, K_CLASS, K_CLASS_TEMPLATE, K_CLASS_PARTIAL}
KIND_LABEL = {K_STRUCT: 'struct', K_UNION: 'union', K_CLASS: 'class', K_ENUM: 'enum', K_ENUM_CONST: 'enumerator', K_FUNCTION: 'function', K_VAR: 'variable',
              K_TYPEDEF: 'typedef', K_METHOD: 'member function', K_CTOR: 'constructor', K_DTOR: 'destructor', K_CONVERSION: 'conversion function',
              K_FUNCTION_TEMPLATE: 'function template', K_CLASS_TEMPLATE: 'class template', K_ALIAS: 'alias', K_NAMESPACE: 'namespace'}
L_INTERNAL, L_UNIQUE_EXTERNAL, L_EXTERNAL = 2, 3, 4      # CXLinkageKind
V_HIDDEN = 1                                             # CXVisibilityKind
T_ULONG, T_ULONGLONG, T_LONG, T_LONGLONG = 10, 11, 18, 19  # CXTypeKind
REF_KINDS = {45, 47, 49, 50, 101, 102, 103}   # TemplateRef, MemberRef, OverloadedDeclRef, VariableRef, DeclRefExpr, MemberRefExpr, CallExpr

# Names Apple's headers put into every Objective-C++ translation unit (MacTypes.h, objc/objc.h, CoreFoundation basics):
# a module declaration with one of them at global scope, or in an anonymous namespace (unqualified lookup then finds
# both), does not compile on macOS / iOS. Macros of the same headers (YES, NO, nil, MIN, MAX, ...) are checked on every
# identifier token of the module by APPLE_MACROS below.
APPLE_NAMES = {
    'Byte', 'SignedByte', 'SInt8', 'UInt8', 'SInt16', 'UInt16', 'SInt32', 'UInt32', 'SInt64', 'UInt64', 'Fixed', 'Fract', 'UnsignedFixed', 'ShortFixed',
    'Float32', 'Float64', 'Float80', 'Float96', 'Float32Point', 'Ptr', 'Handle', 'Size', 'OSErr', 'OSStatus', 'LogicalAddress', 'PhysicalAddress', 'BytePtr',
    'ByteCount', 'ByteOffset', 'Duration', 'AbsoluteTime', 'OptionBits', 'ItemCount', 'PBVersion', 'ScriptCode', 'LangCode', 'RegionCode', 'FourCharCode',
    'OSType', 'ResType', 'Boolean', 'UniChar', 'UniCharCount', 'UTF32Char', 'UTF16Char', 'UTF8Char', 'Str255', 'Str63', 'Str32', 'Str31', 'Str27', 'Str15',
    'StringPtr', 'StringHandle', 'ProcessSerialNumber', 'Point', 'Rect', 'FixedPoint', 'FixedRect', 'CharParameter', 'Style', 'StyleParameter',
    'StyleField', 'TimeValue', 'TimeScale', 'CompTimeValue', 'TimeBase', 'TimeRecord', 'NumVersion', 'VersRec', 'wide', 'UnsignedWide', 'extended80',
    'extended96', 'Nanoseconds', 'RGBColor', 'Cell', 'Component', 'ComponentInstance', 'Collection', 'Media', 'Track', 'Movie',
    'id', 'Class', 'SEL', 'IMP', 'BOOL', 'Protocol', 'Method', 'Ivar', 'Category',
}
# Object-like / function-like macros of the Apple, Objective-C and C platform headers every Apple or Android translation
# unit sees (objc/objc.h, MacTypes.h, sys/param.h, math.h): an identifier spelled like one is replaced wherever it is.
APPLE_MACROS = {'YES', 'NO', 'nil', 'Nil', 'MIN', 'MAX', 'ABS', 'TRUE', 'FALSE', 'DEBUG', 'DOMAIN', 'SING', 'OVERFLOW', 'UNDERFLOW', 'TLOSS', 'PLOSS',
                'HUGE', 'IBOutlet', 'IBAction', 'NSEC_PER_SEC', 'BIG_ENDIAN', 'LITTLE_ENDIAN', 'BYTE_ORDER', 'MAXPATHLEN', 'NBBY', 'PAGE_SIZE', 'PAGE_MASK',
                'noErr', 'kNilOptions', 'kVariableLengthArray', 'kUnknownType', 'major', 'minor', 'makedev'}
FIXED64 = {'int64_t', 'uint64_t', '__int64_t', '__uint64_t', 'int_least64_t', 'uint_least64_t', 'int_fast64_t', 'uint_fast64_t',
           '__int_least64_t', '__uint_least64_t'}
MIX_CALLEES = {'min', 'max', 'clamp', 'minmax', 'Min', 'Max', 'Clamp', 'Min3', 'Max3'}


def load_libclang():
    lib = sdk_usage.load_libclang()
    if lib is None:
        return None
    for name, res, args in (
            ('clang_getCursorSpelling', CXString, [CXCursor]), ('clang_getCursorUSR', CXString, [CXCursor]),
            ('clang_isCursorDefinition', ctypes.c_uint, [CXCursor]), ('clang_getCursorDefinition', CXCursor, [CXCursor]),
            ('clang_getCursorLinkage', ctypes.c_int, [CXCursor]), ('clang_getCursorVisibility', ctypes.c_int, [CXCursor]),
            ('clang_Cursor_isFunctionInlined', ctypes.c_uint, [CXCursor]), ('clang_CXXMethod_isVirtual', ctypes.c_uint, [CXCursor]),
            ('clang_CXXMethod_isPureVirtual', ctypes.c_uint, [CXCursor]), ('clang_CXXMethod_isDefaulted', ctypes.c_uint, [CXCursor]),
            ('clang_CXXMethod_isDeleted', ctypes.c_uint, [CXCursor]), ('clang_Cursor_isAnonymous', ctypes.c_uint, [CXCursor]),
            ('clang_Cursor_getNumArguments', ctypes.c_int, [CXCursor]), ('clang_Cursor_getArgument', CXCursor, [CXCursor, ctypes.c_uint]),
            ('clang_getCursorType', CXType, [CXCursor]), ('clang_getCanonicalType', CXType, [CXType]),
            ('clang_getTypeDeclaration', CXCursor, [CXType]), ('clang_getTypedefDeclUnderlyingType', CXType, [CXCursor]),
            ('clang_getTypeSpelling', CXString, [CXType]), ('clang_getCursorExtent', CXSourceRange, [CXCursor]),
            ('clang_getRangeStart', CXSourceLocation, [CXSourceRange]), ('clang_getCursorLexicalParent', CXCursor, [CXCursor]),
            ('clang_getDiagnostic', ctypes.c_void_p, [ctypes.c_void_p, ctypes.c_uint]), ('clang_getDiagnosticSeverity', ctypes.c_int, [ctypes.c_void_p]),
            ('clang_getDiagnosticSpelling', CXString, [ctypes.c_void_p]), ('clang_disposeDiagnostic', None, [ctypes.c_void_p]),
            ('clang_EnumDecl_isScoped', ctypes.c_uint, [CXCursor]), ('clang_disposeIndex', None, [ctypes.c_void_p])):
        fn = getattr(lib, name, None)
        if fn is None:
            return None   # a libclang older than 16 (no clang_CXXMethod_isDeleted): the scan is skipped
        fn.restype, fn.argtypes = res, args
    return lib


class Scanner:
    """Facts of one translation unit (see scan)."""

    def __init__(self, lib, roots):
        self.lib = lib
        self.module_dir, self.core_dir, self.uht_dir = roots
        self.files = {}
        self.sources = {}

    def text(self, cxstring):
        value = self.lib.clang_getCString(cxstring)
        out = value.decode('utf-8', 'replace') if value else ''
        self.lib.clang_disposeString(cxstring)
        return out

    def location_of(self, loc):
        f, line = ctypes.c_void_p(), ctypes.c_uint()
        self.lib.clang_getSpellingLocation(loc, ctypes.byref(f), ctypes.byref(line), None, None)
        if not f.value:
            return '', 0
        name = self.files.get(f.value)
        if name is None:
            name = os.path.realpath(self.text(self.lib.clang_getFileName(f)))
            self.files[f.value] = name
        return name, line.value

    def location(self, cursor):
        return self.location_of(self.lib.clang_getCursorLocation(cursor))

    def spelling(self, cursor):
        return self.text(self.lib.clang_getCursorSpelling(cursor))

    def parents(self, cursor):
        """[(kind, name, anonymous)] of the enclosing scopes, innermost first, up to the translation unit."""
        out = []
        lib = self.lib
        cur = lib.clang_getCursorSemanticParent(cursor)
        while not lib.clang_Cursor_isNull(cur) and len(out) < 16:
            kind = lib.clang_getCursorKind(cur)
            if kind == K_TU:
                break
            if kind in CLASS_KINDS and not lib.clang_Cursor_isNull(lib.clang_getSpecializedCursorTemplate(cur)):
                kind = K_CLASS_TEMPLATE   # an instantiation of a class template: its members are templated entities
            out.append((kind, self.spelling(cur), bool(lib.clang_Cursor_isAnonymous(cur)) or (kind == K_NAMESPACE and not self.spelling(cur))))
            cur = lib.clang_getCursorSemanticParent(cur)
        return out

    def generated(self, path, line):
        """A declaration that GENERATED_BODY() expands to (libclang places it on that line of the module header): the
        real UHT defines every one of those in its .gen.cpp, the fake one only what the compile check needs."""
        if 'GENERATED_' in self.source_line(path, line):
            return True
        # a Blueprint event: UHT writes the body of `Foo()` itself (the module only defines Foo_Implementation, if anything)
        above = ' '.join(self.source_line(path, n) for n in range(max(1, line - 3), line))
        return re.search(r'UFUNCTION\s*\([^)]*\bBlueprint(NativeEvent|ImplementableEvent)\b', above) is not None

    def source_line(self, path, line):
        lines = self.sources.get(path)
        if lines is None:
            try:
                with open(path, encoding='utf-8', errors='replace') as f:
                    lines = f.read().split('\n')
            except OSError:
                lines = []
            self.sources[path] = lines
        return lines[line - 1] if 0 < line <= len(lines) else ''

    def declared_inline(self, cursor, path, line):
        """A namespace-scope variable definition: is it spelled `inline` / `constexpr` / `static` / `extern`, or a template?"""
        start, start_line = self.location_of(self.lib.clang_getRangeStart(self.lib.clang_getCursorExtent(cursor)))
        text = ' '.join(self.source_line(path, n) for n in range(max(1, min(start_line or line, line) - 1), line + 1))
        return re.search(r'\b(inline|template|extern|static|thread_local)\b', text) is not None

    def unwrap(self, cursor):
        """The expression under implicit casts / temporaries (libclang: UnexposedExpr with one child)."""
        lib = self.lib
        for _ in range(8):
            if cursor.kind != 100:   # CXCursor_UnexposedExpr
                break
            children = []

            def first(child, _parent, _data, children=children):
                children.append(child)
                return 0   # CXChildVisit_Break
            lib.clang_visitChildren(cursor, VISITOR(first), None)
            if not children:
                break
            cursor = children[0]
        return cursor

    def family(self, cxtype):
        """'fixed64' for a type spelled through std::int64_t / std::uint64_t (long long on macOS / iOS), 'long' for
        size_t / ptrdiff_t / long (long everywhere), None for anything that is not a 64-bit `long` here."""
        lib = self.lib
        canonical = lib.clang_getCanonicalType(cxtype).kind
        if canonical not in (T_LONG, T_ULONG):
            return None
        cur = cxtype
        for _ in range(12):
            # the spelling first: `std::uint64_t` is a using-declaration of ::uint64_t, which has no typedef cursor of its own
            spelled = re.sub(r'\b(const|volatile)\b|[&*\s]', '', self.text(lib.clang_getTypeSpelling(cur))).split('::')[-1]
            if spelled in FIXED64:
                return 'fixed64'
            decl = lib.clang_getTypeDeclaration(cur)
            if lib.clang_Cursor_isNull(decl) or lib.clang_getCursorKind(decl) not in (K_TYPEDEF, K_ALIAS):
                break
            if self.spelling(decl) in FIXED64:
                return 'fixed64'
            cur = lib.clang_getTypedefDeclUnderlyingType(decl)
        return 'long'

    def scan(self, index, args, source):
        lib = self.lib
        argv = (ctypes.c_char_p * len(args))(*[a.encode() for a in args])
        tu = lib.clang_parseTranslationUnit(index, source.encode(), argv, len(args), None, 0, 0)
        facts = {'source': source, 'internal': [], 'decls': {}, 'defs': {}, 'refs': [], 'core': [], 'globals': [], 'mix': [], 'errors': []}
        if not tu:
            facts['errors'].append('libclang could not parse the translation unit')
            return facts
        for i in range(lib.clang_getNumDiagnostics(tu)):
            diag = lib.clang_getDiagnostic(tu, i)
            if lib.clang_getDiagnosticSeverity(diag) >= 3:
                facts['errors'].append(self.text(lib.clang_getDiagnosticSpelling(diag)))
            lib.clang_disposeDiagnostic(diag)
        if facts['errors']:
            lib.clang_disposeTranslationUnit(tu)
            return facts
        self.files = {}
        module_dir, core_dir, uht_dir = self.module_dir, self.core_dir, self.uht_dir
        main = os.path.realpath(source)
        refs = set()
        core_seen = set()

        def is_module(path):
            return path.startswith(module_dir) or path.startswith(uht_dir)

        def visit(cursor, _parent, _data):
            kind = cursor.kind
            if kind in FUNCTION_KINDS or kind == K_VAR or kind in TYPE_KINDS or kind == K_ENUM_CONST:
                path, line = self.location(cursor)
                if is_module(path):
                    parents = self.parents(cursor)
                    local = any(k in FUNCTION_KINDS for k, _n, _a in parents)
                    in_class = bool(parents) and parents[0][0] in CLASS_KINDS
                    name = self.spelling(cursor)
                    if not local and name:
                        linkage = lib.clang_getCursorLinkage(cursor)
                        is_def = bool(lib.clang_isCursorDefinition(cursor))
                        qualified = '::'.join([n if n else '(anonymous)' for _k, n, _a in reversed(parents)] + [name])
                        scope_global = all(k == K_NAMESPACE and a for k, _n, a in parents)   # the global scope or anonymous namespaces only
                        if scope_global and path.startswith(module_dir):
                            facts['globals'].append([name, KIND_LABEL.get(kind, str(kind)), path, line])
                        # an unscoped enumerator is in the scope of its enum's parent: `enum { Rect }` at global scope
                        if (kind == K_ENUM_CONST and all(k == K_NAMESPACE and a for k, _n, a in parents[1:]) and path.startswith(module_dir)
                                and not lib.clang_EnumDecl_isScoped(lib.clang_getCursorSemanticParent(cursor))):
                            facts['globals'].append([name, 'enumerator', path, line])
                        if kind in TYPE_KINDS or kind == K_ENUM_CONST:
                            # a type defined in an anonymous namespace of a .cpp (never a member / local type)
                            if kind in TYPE_KINDS and is_def and not in_class and path == main and any(a for k, _n, a in parents if k == K_NAMESPACE):
                                facts['internal'].append([qualified, KIND_LABEL.get(kind, str(kind)), path, line])
                        elif linkage in (L_INTERNAL, L_UNIQUE_EXTERNAL):
                            if is_def and not in_class and path == main:
                                facts['internal'].append([qualified, KIND_LABEL.get(kind, str(kind)), path, line])
                        elif linkage == L_EXTERNAL and (kind in FUNCTION_KINDS or (kind == K_VAR)):
                            usr = self.text(lib.clang_getCursorUSR(cursor))
                            # an implicit instantiation reaches the consumer as a top-level function of its own: also "templated"
                            templated = (kind == K_FUNCTION_TEMPLATE or any(k in (K_CLASS_TEMPLATE, K_CLASS_PARTIAL) for k, _n, _a in parents)
                                         or (kind != K_VAR and not lib.clang_Cursor_isNull(lib.clang_getSpecializedCursorTemplate(cursor))))
                            if self.generated(path, line):
                                pass
                            elif is_def:
                                if kind == K_VAR:
                                    inline = in_class or self.declared_inline(cursor, path, line)
                                else:
                                    inline = bool(lib.clang_Cursor_isFunctionInlined(cursor)) or bool(lib.clang_CXXMethod_isDefaulted(cursor))
                                    lexical = lib.clang_getCursorLexicalParent(cursor)
                                    if lib.clang_getCursorKind(lexical) in CLASS_KINDS:
                                        inline = True   # defined in the class body
                                # the last flag: a namespace-scope function / variable (not a member) defined in this .cpp
                                facts['defs'].setdefault(usr, [qualified, KIND_LABEL.get(kind, str(kind)), path, line, inline or templated,
                                                               kind in (K_FUNCTION, K_VAR) and not in_class and path == main])
                            elif usr not in facts['decls']:
                                pure = kind in (K_METHOD, K_DTOR, K_CONVERSION) and bool(lib.clang_CXXMethod_isPureVirtual(cursor))
                                gone = kind != K_VAR and (bool(lib.clang_CXXMethod_isDeleted(cursor)) or bool(lib.clang_CXXMethod_isDefaulted(cursor)))
                                if not pure and not gone and not templated:
                                    virtual = kind in (K_METHOD, K_DTOR, K_CONVERSION) and bool(lib.clang_CXXMethod_isVirtual(cursor))
                                    facts['decls'][usr] = [qualified, KIND_LABEL.get(kind, str(kind)), path, line, virtual or kind == K_DTOR]
            elif kind in REF_KINDS:
                use_path, use_line = self.location(cursor)
                if is_module(use_path):
                    target = lib.clang_getCursorReferenced(cursor)
                    if not lib.clang_Cursor_isNull(target):
                        tkind = lib.clang_getCursorKind(target)
                        if tkind in FUNCTION_KINDS or tkind == K_VAR:
                            template = lib.clang_getSpecializedCursorTemplate(target)
                            origin = template if not lib.clang_Cursor_isNull(template) else target
                            decl_path, decl_line = self.location(origin)
                            if is_module(decl_path):
                                refs.add(self.text(lib.clang_getCursorUSR(origin)))
                            elif decl_path.startswith(core_dir) and lib.clang_Cursor_isNull(template) and tkind != K_FUNCTION_TEMPLATE:
                                key = (decl_path, decl_line)
                                if key not in core_seen:
                                    core_seen.add(key)
                                    parents = self.parents(target)
                                    templated = any(k in (K_CLASS_TEMPLATE, K_CLASS_PARTIAL) for k, _n, _a in parents)
                                    local = any(k in FUNCTION_KINDS for k, _n, _a in parents)
                                    definition = lib.clang_getCursorDefinition(target)
                                    inline = (tkind != K_VAR and (bool(lib.clang_Cursor_isFunctionInlined(target)) or bool(lib.clang_CXXMethod_isDefaulted(target))
                                                                  or bool(lib.clang_CXXMethod_isDeleted(target)) or bool(lib.clang_CXXMethod_isPureVirtual(target))))
                                    external = lib.clang_getCursorLinkage(target) == L_EXTERNAL
                                    if external and not templated and not local and not inline and lib.clang_Cursor_isNull(definition):
                                        hidden = lib.clang_getCursorVisibility(target) == V_HIDDEN
                                        name = '::'.join([n for _k, n, _a in reversed(parents) if n] + [self.spelling(target)])
                                        facts['core'].append([name, KIND_LABEL.get(tkind, str(tkind)), decl_path, decl_line, hidden, use_path, use_line])
                    if kind == K_CALL and use_path.startswith(module_dir):
                        count = lib.clang_Cursor_getNumArguments(cursor)
                        if count >= 2 and self.spelling(cursor) in MIX_CALLEES:
                            families = []
                            for i in range(count):
                                # the argument as written: under the implicit conversion to the (already substituted) parameter type
                                arg = self.unwrap(lib.clang_Cursor_getArgument(cursor, i))
                                cxtype = lib.clang_getCursorType(arg)
                                fam = self.family(cxtype)
                                if fam:
                                    families.append((fam, self.text(lib.clang_getTypeSpelling(cxtype))))
                            if len({f for f, _s in families}) > 1:
                                facts['mix'].append([self.spelling(cursor), use_path, use_line, sorted({s for _f, s in families})])
            return 2   # CXChildVisit_Recurse

        callback = VISITOR(visit)
        lib.clang_visitChildren(lib.clang_getTranslationUnitCursor(tu), callback, None)
        lib.clang_disposeTranslationUnit(tu)
        facts['refs'] = sorted(refs)
        return facts


_WORKER = {}


def scan_unit(job):
    """Worker (its own process: libclang state is per process). job: (args, source, roots, cache path, cache key, deps)."""
    args, source, roots, cache, key, deps = job
    if cache and os.path.exists(cache):
        try:
            with open(cache, encoding='utf-8') as f:
                cached = json.load(f)
            if cached.get('key') == key and deps and cc.newest_mtime(deps) <= os.path.getmtime(cache):
                cached['cached'] = True
                return cached
        except (ValueError, OSError):
            pass
    if 'lib' not in _WORKER:
        _WORKER['lib'] = load_libclang()
        _WORKER['index'] = _WORKER['lib'].clang_createIndex(1, 0) if _WORKER['lib'] else None   # 1: skip the declarations of the PCH
    if _WORKER['lib'] is None:
        return {'source': source, 'errors': ['libclang not found'], 'internal': [], 'decls': {}, 'defs': {}, 'refs': [], 'core': [], 'globals': [], 'mix': []}
    facts = Scanner(_WORKER['lib'], roots).scan(_WORKER['index'], args, source)
    facts['key'] = key
    if cache and not facts['errors']:
        with open(cache, 'w', encoding='utf-8') as f:
            json.dump(facts, f)
    return facts


TOKEN_RE = re.compile(r'[A-Za-z_]\w*')


def strip_comments_and_strings(text):
    """C++ source with comments, string and character literals blanked (line structure kept)."""
    out = []
    i, n = 0, len(text)
    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ''
        if ch == '/' and nxt == '/':
            j = text.find('\n', i)
            j = n if j < 0 else j
            i = j
        elif ch == '/' and nxt == '*':
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(''.join(c if c == '\n' else ' ' for c in text[i:j]))
            i = j
        elif ch == 'R' and nxt == '"':
            m = re.match(r'R"([^()\\ ]{0,16})\(', text[i:])
            if not m:
                out.append(ch)
                i += 1
                continue
            end = text.find(')' + m.group(1) + '"', i)
            end = n if end < 0 else end + len(m.group(1)) + 2
            out.append(''.join(c if c == '\n' else ' ' for c in text[i:end]))
            i = end
        elif ch in '"\'':
            if ch == '\'' and i and (text[i - 1].isalnum()) and nxt.isalnum():
                out.append(' ')   # a digit separator (1'000)
                i += 1
                continue
            j = i + 1
            while j < n and text[j] != ch and text[j] != '\n':
                j += 2 if text[j] == '\\' else 1
            out.append(' ' * (min(j, n - 1) - i + 1))
            i = j + 1
        else:
            out.append(ch)
            i += 1
    return ''.join(out)


# an integer conversion with a platform-dependent length modifier: %ld / %lu / %lx (32-bit on Windows, 64-bit elsewhere),
# %zu / %zd / %td / %jd (not what a std::uint64_t is on macOS, where it is `unsigned long long`)
FORMAT_LENGTH_RE = re.compile(r'(?<!%)%[-+#0]*(?:\d+|\*)?(?:\.(?:\d+|\*))?(l(?!l)|z|j|t)[diouxX]')
STRING_RE = re.compile(r'"((?:[^"\\\n]|\\.)*)"')


def macro_name_lint(module_dir, core_dir):
    """Text lints: identifier tokens of the module (and of the core's public headers it compiles) spelled like a platform
    macro, and format strings of the module with a platform-dependent length modifier."""
    findings = []
    for root, _dirs, files in os.walk(module_dir):
        for name in sorted(files):
            if not name.endswith(('.h', '.cpp', '.inl')):
                continue
            path = os.path.join(root, name)
            with open(path, encoding='utf-8', errors='replace') as f:
                for number, line in enumerate(f.read().split('\n'), 1):
                    if '%' not in line or line.lstrip().startswith(('//', '#')):
                        continue
                    for literal in STRING_RE.finditer(line):
                        m = FORMAT_LENGTH_RE.search(literal.group(1))
                        if m:
                            findings.append({'rule': 'lp64-mix', 'level': 'error', 'file': os.path.join('Source/Abyssfire', os.path.relpath(path, module_dir)), 'line': number,
                                             'message': "format '%s': the length modifier '%s' names a type whose size or identity differs between Windows, Linux / "
                                                        'Android and macOS / iOS (a std::uint64_t is `unsigned long` here and `unsigned long long` on the Mac): use %%d '
                                                        'with an int32, or %%lld / %%llu with a `long long` cast' % (m.group(0), m.group(1))})
    for root_dir, label, level in ((module_dir, 'Source/Abyssfire', 'error'), (core_dir, 'Source/AbyssCore/Public', 'warn')):
        for root, _dirs, files in os.walk(root_dir):
            for name in sorted(files):
                if not name.endswith(('.h', '.cpp', '.inl')):
                    continue
                path = os.path.join(root, name)
                with open(path, encoding='utf-8', errors='replace') as f:
                    text = strip_comments_and_strings(f.read())
                for number, line in enumerate(text.split('\n'), 1):
                    if line.lstrip().startswith('#'):
                        continue
                    for token in sorted({m.group(0) for m in TOKEN_RE.finditer(line)} & APPLE_MACROS):
                        m = re.search(r'\b%s\b' % token, line)
                        if m:
                            findings.append({'rule': 'platform-name', 'level': level, 'file': os.path.join(label, os.path.relpath(path, root_dir)), 'line': number,
                                             'message': "identifier '%s' is a macro of the Apple / Objective-C / C platform headers that every Mac, iOS or Android "
                                                        'translation unit sees: it is replaced there (rename it)' % m.group(0)})
    return findings


def aggregate(config, target_type, all_facts, module_dir, uht_dir):
    """Findings of one configuration from the facts of its translation units."""
    findings = []

    def rel(path):
        real_module = os.path.realpath(module_dir)
        if path.startswith(real_module + os.sep):
            return os.path.join('Source', 'Abyssfire', os.path.relpath(path, real_module))
        if path.startswith(os.path.realpath(uht_dir)):
            return os.path.join('uht', os.path.basename(path))
        return os.path.relpath(path, cc.UNREAL_DIR)

    def add(rule, level, path, line, message):
        findings.append({'rule': rule, 'level': level, 'file': rel(path), 'line': line, 'message': message, 'config': config})

    # unity-collision: the same internal-linkage name defined in two .cpp files
    internal = {}
    for facts in all_facts:
        for qualified, kind, path, line in facts['internal']:
            internal.setdefault(qualified, {}).setdefault(path, (kind, line))
    for qualified in sorted(internal):
        places = internal[qualified]
        if len(places) > 1:
            first = sorted(places)[0]
            others = ', '.join('%s:%d' % (rel(p), places[p][1]) for p in sorted(places)[1:])
            add('unity-collision', 'error', first, places[first][1],
                "%s '%s' has internal linkage and is also defined in %s: in a unity blob both are in one translation unit (a redefinition, or one silently "
                'merged overload set); give each a name of its own' % (places[first][0], qualified, others))
    # undefined / duplicate / header-definition
    decls, defs, refs = {}, {}, set()
    for facts in all_facts:
        refs.update(facts['refs'])
        for usr, decl in facts['decls'].items():
            if usr not in decls or (decl[2].endswith('.h') and not decls[usr][2].endswith('.h')):
                decls[usr] = decl
        for usr, definition in facts['defs'].items():
            defs.setdefault(usr, {}).setdefault((definition[2], definition[3]), definition)
    # unity-collision, second form: helpers with external linkage that no header declares (a named "private" namespace
    # instead of an anonymous one), with one name and different parameters in two .cpp files
    helpers = {}
    for usr, places in defs.items():
        declared_in_header = usr in decls and decls[usr][2].endswith(('.h', '.inl'))
        for (path, line), d in places.items():
            if len(d) > 5 and d[5] and not declared_in_header:
                helpers.setdefault(d[0], {}).setdefault(path, (d[1], line, usr))
    for qualified in sorted(helpers):
        places = helpers[qualified]
        if len(places) > 1 and len({u for _k, _l, u in places.values()}) > 1:
            first = sorted(places)[0]
            others = ', '.join('%s:%d' % (rel(p), places[p][1]) for p in sorted(places)[1:])
            add('unity-collision', 'error', first, places[first][1],
                "%s '%s' is declared in no header and is defined with other parameters in %s: in a unity blob the two become one overload set and a call "
                'can bind to the other file\'s helper; give each a name of its own' % (places[first][0], qualified, others))
    for usr in sorted(decls):
        qualified, kind, path, line, needed = decls[usr]
        if usr in defs or path.startswith(os.path.realpath(uht_dir)):
            continue
        if usr in refs or needed:
            why = 'used' if usr in refs else ('virtual: its class cannot be linked' if kind != 'destructor' else 'a destructor: every object of the class needs it')
            add('undefined', 'error', path, line, "%s '%s' is declared here, %s, and defined in no translation unit of the module: an undefined symbol at link time"
                % (kind, qualified, why))
    for usr in sorted(defs):
        places = defs[usr]
        strong = {where: d for where, d in places.items() if not d[4]}
        if len(strong) > 1:
            first = sorted(strong)[0]
            add('duplicate', 'error', first[0], first[1], "%s '%s' is also defined in %s: a duplicate symbol at link time" % (
                strong[first][1], strong[first][0], ', '.join('%s:%d' % (rel(p), l) for p, l in sorted(strong)[1:])))
        for (path, line), d in sorted(strong.items()):
            if path.endswith(('.h', '.inl')) and not path.startswith(os.path.realpath(uht_dir)):
                add('header-definition', 'error', path, line, "%s '%s' is defined in a header without `inline`: a duplicate symbol as soon as two .cpp files include "
                    'the header' % (d[1], d[0]))
    # core-export: only where AbyssCore is its own library
    if target_type == 'Editor':
        seen = set()
        for facts in all_facts:
            for name, kind, decl_path, decl_line, hidden, use_path, use_line in facts['core']:
                if hidden and (decl_path, decl_line) not in seen:
                    seen.add((decl_path, decl_line))
                    shown = os.path.join('AbyssCore', 'Public', os.path.relpath(decl_path, os.path.realpath(cc.CORE_PUBLIC)))
                    add('core-export', 'error', use_path, use_line, "AbyssCore %s '%s' (%s:%d) is used here but not exported (no ABYSS_API): an undefined symbol in the "
                        'modular editor build, where AbyssCore is a library of its own' % (kind, name, shown, decl_line))
    # platform-name: global / anonymous-namespace names
    seen = set()
    for facts in all_facts:
        for name, kind, path, line in facts['globals']:
            if name in APPLE_NAMES and (path, line) not in seen:
                seen.add((path, line))
                add('platform-name', 'error', path, line, "%s '%s' at global scope (or in an anonymous namespace) has the name of a type of Apple's MacTypes.h / "
                    'Objective-C runtime headers, which every Mac / iOS translation unit has in scope: ambiguous or redefined there (rename it or put it in a '
                    'named namespace)' % (kind, name))
    # lp64-mix
    seen = set()
    for facts in all_facts:
        for callee, path, line, types in facts['mix']:
            if (path, line) not in seen:
                seen.add((path, line))
                add('lp64-mix', 'error', path, line, "'%s' mixes %s: one type here (and on Android), two different types on macOS / iOS, where std::int64_t / "
                    'std::uint64_t are `long long` and size_t / long are `long` (cast both sides to one type)' % (callee, ' and '.join("'%s'" % t for t in types)))
    return findings


def scan_config(checker, config, units, jobs, use_cache=True):
    """(findings, statistics, errors) of one configuration. `units`: the translation units of compile_check (module + gen)."""
    _desc, target_type, _defines = cc.CONFIGS[config]
    base = checker.command_base(config)
    pch, _diags = checker.build_pch(config, base)
    args = [a for a in base['cmd'][1:] if a != '-fsyntax-only']
    # what UBT passes for a modular build: only the symbols marked for export are visible to another library
    args += ['-fvisibility=hidden'] if target_type == 'Editor' else []
    if pch and target_type != 'Editor':
        args += ['-include-pch', pch]
    else:
        args += ['-include', base['pch_wrapper']]   # the PCH was built with the default visibility: parse the header again
    ast_dir = os.path.join(base['dir'], 'ast')
    os.makedirs(ast_dir, exist_ok=True)
    roots = (os.path.realpath(cc.MODULE_DIR) + os.sep, os.path.realpath(cc.CORE_PUBLIC) + os.sep, os.path.realpath(checker.uht_dir) + os.sep)
    jobs_list = []
    for unit in units:
        if unit['kind'] not in ('module', 'gen'):
            continue
        name = re.sub(r'[^A-Za-z0-9_.]', '_', unit['rel'])
        dep = os.path.join(base['dir'], 'obj', name + '.d')
        # the dependency file of the compile names what the unit includes outside the PCH; the PCH covers the rest
        deps = (cc.parse_depfile(dep) + ([pch] if pch else [])) if use_cache and os.path.exists(dep) else []
        key = cc.sha(' '.join(args) + '|' + unit['path'] + '|' + '|'.join(roots) + '|' + str(os.path.getmtime(os.path.abspath(__file__))))
        jobs_list.append((args, unit['path'], roots, os.path.join(ast_dir, name + '.json'), key, deps))
    if jobs <= 1:
        all_facts = [scan_unit(job) for job in jobs_list]
    else:
        with concurrent.futures.ProcessPoolExecutor(max_workers=jobs) as pool:
            all_facts = list(pool.map(scan_unit, jobs_list))
    errors = [(f['source'], f['errors'][0]) for f in all_facts if f.get('errors')]
    good = [f for f in all_facts if not f.get('errors')]
    findings = aggregate(config, target_type, good, cc.MODULE_DIR, checker.uht_dir)
    if errors:
        # a definition may sit in a translation unit that does not compile: "undefined" would be a false finding
        findings = [f for f in findings if f['rule'] != 'undefined']
    stats = {'units': len(all_facts), 'cached': sum(1 for f in all_facts if f.get('cached')),
             'internal': len({i[0] for f in good for i in f['internal']}), 'declared': len({u for f in good for u in f['decls']}),
             'defined': len({u for f in good for u in f['defs']}), 'core_symbols': len({(c[2], c[3]) for f in good for c in f['core']}),
             'globals': len({(g[2], g[3]) for f in good for g in f['globals']})}
    return findings, stats, errors


def run_scan(checker, configs, units, jobs, use_cache=True):
    """All configurations; findings that repeat across configurations are merged. Returns (findings, per-config stats, errors)."""
    merged, order, stats, errors = {}, [], {}, []
    for config in configs:
        findings, stats[config], errs = scan_config(checker, config, units, jobs, use_cache)
        errors += [(config, source, message) for source, message in errs]
        for f in findings:
            key = (f['rule'], f['file'], f['line'], f['message'])
            if key not in merged:
                merged[key] = dict(f, configs=[])
                del merged[key]['config']
                order.append(key)
            merged[key]['configs'].append(config)
    macro = macro_name_lint(cc.MODULE_DIR, cc.CORE_PUBLIC)
    for f in macro:
        f['configs'] = list(configs)
    return [merged[k] for k in order] + macro, stats, errors


def print_findings(findings, stats, errors, configs):
    for config in configs:
        s = stats[config]
        print('   %s: %d translation units (%d from cache); %d internal-linkage names, %d declared / %d defined external symbols, %d AbyssCore symbols used, '
              '%d global names' % (config, s['units'], s['cached'], s['internal'], s['declared'], s['defined'], s['core_symbols'], s['globals']))
    for config, source, message in errors[:20]:
        print('   could not scan %s (%s): %s' % (os.path.relpath(source, cc.UNREAL_DIR), config, message[:200]))
    if errors:
        print('   (the undefined-symbol rule is skipped for a configuration with translation units that do not compile)')
    for f in findings:
        where = 'all configs' if len(f['configs']) == len(configs) else ', '.join(f['configs'])
        print('%s:%d: %s: [%s] %s  {%s}' % (f['file'], f['line'], f['level'], f['rule'], f['message'], where))
    counts = {level: sum(1 for f in findings if f['level'] == level) for level in ('error', 'warn')}
    print('link scan: %d errors, %d warnings%s' % (counts['error'], counts['warn'], (', %d translation units could not be scanned' % len(errors)) if errors else ''))
    return counts['error'] == 0 and not errors


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--config', action='append', help='configuration (repeatable; default: all)')
    ap.add_argument('--build-dir', default=os.environ.get('ABYSS_CC_BUILD', os.path.join(cc.CHECK_DIR, 'build')))
    ap.add_argument('--module-dir', default=os.environ.get('ABYSS_CC_MODULE', cc.MODULE_DIR))
    ap.add_argument('--core-public', default=os.environ.get('ABYSS_CC_CORE_PUBLIC', cc.CORE_PUBLIC))
    ap.add_argument('-j', '--jobs', type=int, default=int(os.environ.get('ABYSS_JOBS', '3')))
    ap.add_argument('--force', action='store_true', help='ignore cached facts')
    ap.add_argument('--json', help='also write the findings to this file')
    args = ap.parse_args()
    cc.MODULE_DIR = os.path.abspath(args.module_dir)
    cc.CORE_PUBLIC = os.path.abspath(args.core_public)
    if load_libclang() is None:
        sys.exit('libclang not found (set ABYSS_LIBCLANG to the shared library of the clang that run.sh uses)')
    report = os.path.join(os.path.abspath(args.build_dir), 'uht_report.json')
    if not os.path.exists(report):
        sys.exit('run ./run.sh once first (the generated headers of the fake UHT are needed): no ' + report)
    with open(report, encoding='utf-8') as f:
        generated = json.load(f)['generated']
    ns = argparse.Namespace(build_dir=args.build_dir, cxx=os.environ.get('ABYSS_CC_CXX', 'clang++'), define=[], no_pch=False, force=False, only=[], jobs=args.jobs,
                            stdlib=None, msvc_lint=False)
    checker = cc.Checker(ns)
    configs = args.config or list(cc.CONFIGS)
    findings, stats, errors = run_scan(checker, configs, checker.translation_units(generated), args.jobs, use_cache=not args.force)
    print('== link scan (libclang over every translation unit: unity collisions, undefined / duplicate symbols, exports, platform names)')
    ok = print_findings(findings, stats, errors, configs)
    if args.json:
        with open(args.json, 'w', encoding='utf-8') as f:
            json.dump({'findings': findings, 'stats': stats, 'errors': errors}, f, indent=1)
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())

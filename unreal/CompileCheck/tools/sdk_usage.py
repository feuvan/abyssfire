#!/usr/bin/env python3
"""Lists the stub-SDK declarations a set of module files actually uses: what a PASS of those files rests on.

    python3 tools/sdk_usage.py --only World --only Actors            # declarations used, grouped by SDK header
    python3 tools/sdk_usage.py --only World --baseline Framework ... # only those the baseline files do not use
    python3 tools/sdk_usage.py --only AbyssZoneActor.cpp --uses      # with the module lines that use each one

Each translation unit is parsed with the system libclang (ctypes, no Python package needed) and the command line of the
compile check (configuration mac-editor unless --config). Every reference from a module file to a declaration that
lives in sdk/ is recorded: functions, members, types, enumerators, macros are not (they are expanded before parsing).
Use it when an area turns green: review the listed declarations against the engine (the reflected ones are already
cross-checked by check_reflected.py; the others are source knowledge), and move anything doubtful to API_UNCERTAIN.md.
"""
import argparse
import ctypes
import glob
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)
import compile_check as cc  # noqa: E402


class CXString(ctypes.Structure):
    _fields_ = [('data', ctypes.c_void_p), ('private_flags', ctypes.c_uint)]


class CXCursor(ctypes.Structure):
    _fields_ = [('kind', ctypes.c_int), ('xdata', ctypes.c_int), ('data', ctypes.c_void_p * 3)]


class CXSourceLocation(ctypes.Structure):
    _fields_ = [('ptr_data', ctypes.c_void_p * 2), ('int_data', ctypes.c_uint)]


VISITOR = ctypes.CFUNCTYPE(ctypes.c_int, CXCursor, CXCursor, ctypes.c_void_p)


def load_libclang():
    names = [os.environ.get('ABYSS_LIBCLANG', '')] + sorted(glob.glob('/usr/lib/llvm-*/lib/libclang.so*'), reverse=True) + \
        sorted(glob.glob('/usr/lib/*/libclang-*.so*'), reverse=True) + ['libclang.so', 'libclang.dylib',
                                                                         '/Library/Developer/CommandLineTools/usr/lib/libclang.dylib']
    for name in names:
        if not name:
            continue
        try:
            lib = ctypes.CDLL(name)
        except OSError:
            continue
        lib.clang_createIndex.restype = ctypes.c_void_p
        lib.clang_createIndex.argtypes = [ctypes.c_int, ctypes.c_int]
        lib.clang_parseTranslationUnit.restype = ctypes.c_void_p
        lib.clang_parseTranslationUnit.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_char_p), ctypes.c_int,
                                                   ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint]
        lib.clang_getTranslationUnitCursor.restype = CXCursor
        lib.clang_getTranslationUnitCursor.argtypes = [ctypes.c_void_p]
        lib.clang_visitChildren.restype = ctypes.c_uint
        lib.clang_visitChildren.argtypes = [CXCursor, VISITOR, ctypes.c_void_p]
        lib.clang_getCursorReferenced.restype = CXCursor
        lib.clang_getCursorReferenced.argtypes = [CXCursor]
        lib.clang_Cursor_isNull.restype = ctypes.c_int
        lib.clang_Cursor_isNull.argtypes = [CXCursor]
        lib.clang_getCursorLocation.restype = CXSourceLocation
        lib.clang_getCursorLocation.argtypes = [CXCursor]
        lib.clang_getSpellingLocation.restype = None
        lib.clang_getSpellingLocation.argtypes = [CXSourceLocation, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_uint),
                                                  ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint)]
        lib.clang_getFileName.restype = CXString
        lib.clang_getFileName.argtypes = [ctypes.c_void_p]
        lib.clang_getCString.restype = ctypes.c_char_p
        lib.clang_getCString.argtypes = [CXString]
        lib.clang_disposeString.restype = None
        lib.clang_disposeString.argtypes = [CXString]
        lib.clang_getCursorDisplayName.restype = CXString
        lib.clang_getCursorDisplayName.argtypes = [CXCursor]
        lib.clang_getCursorSemanticParent.restype = CXCursor
        lib.clang_getCursorSemanticParent.argtypes = [CXCursor]
        lib.clang_getCursorKind.restype = ctypes.c_int
        lib.clang_getCursorKind.argtypes = [CXCursor]
        lib.clang_getSpecializedCursorTemplate.restype = CXCursor
        lib.clang_getSpecializedCursorTemplate.argtypes = [CXCursor]
        lib.clang_disposeTranslationUnit.argtypes = [ctypes.c_void_p]
        lib.clang_getNumDiagnostics.restype = ctypes.c_uint
        lib.clang_getNumDiagnostics.argtypes = [ctypes.c_void_p]
        return lib
    return None


KIND_NAMES = {4: 'struct', 5: 'union', 6: 'class', 5001: 'enum', 7: 'enumerator', 8: 'function', 9: 'variable', 6001: 'field', 20: 'typedef', 21: 'method',
              24: 'constructor', 25: 'destructor', 26: 'conversion', 30: 'function template', 31: 'class template', 32: 'class template', 36: 'alias', 10: 'parameter',
              22: 'namespace', 603: 'concept'}
# CXCursorKind values (clang-c/Index.h)
K_STRUCT, K_UNION, K_CLASS, K_ENUM, K_FIELD, K_ENUM_CONST, K_FUNCTION, K_VAR, K_PARM = 2, 3, 4, 5, 6, 7, 8, 9, 10
KIND_LABEL = {2: 'struct', 3: 'union', 4: 'class', 5: 'enum', 6: 'field', 7: 'enumerator', 8: 'function', 9: 'variable', 20: 'typedef', 21: 'method', 22: 'namespace',
              24: 'constructor', 25: 'destructor', 26: 'conversion', 30: 'function template', 31: 'class template', 32: 'partial specialization', 36: 'alias',
              33: 'namespace alias', 604: 'concept'}
REF_KINDS = {43, 45, 46, 47, 49, 50, 101, 102, 103}   # TypeRef, TemplateRef, NamespaceRef, MemberRef, OverloadedDeclRef, VariableRef, DeclRefExpr, MemberRefExpr, CallExpr


class Usage:
    def __init__(self, lib):
        self.lib = lib
        self.index = lib.clang_createIndex(0, 0)
        self.files = {}

    def text(self, cxstring):
        value = self.lib.clang_getCString(cxstring)
        out = value.decode('utf-8', 'replace') if value else ''
        self.lib.clang_disposeString(cxstring)
        return out

    def location(self, cursor):
        f, line = ctypes.c_void_p(), ctypes.c_uint()
        self.lib.clang_getSpellingLocation(self.lib.clang_getCursorLocation(cursor), ctypes.byref(f), ctypes.byref(line), None, None)
        if not f.value:
            return '', 0
        name = self.files.get(f.value)
        if name is None:
            name = os.path.realpath(self.text(self.lib.clang_getFileName(f)))
            self.files[f.value] = name
        return name, line.value

    def qualified(self, cursor):
        parts = []
        depth = 0
        while not self.lib.clang_Cursor_isNull(cursor) and depth < 12:
            kind = self.lib.clang_getCursorKind(cursor)
            if kind == 350:   # the translation unit
                break
            name = self.text(self.lib.clang_getCursorDisplayName(cursor))
            if name:
                parts.append(name)
            cursor = self.lib.clang_getCursorSemanticParent(cursor)
            depth += 1
        return '::'.join(reversed(parts))

    def scan(self, cmd, source, module_dir, sdk_dir, found):
        args = [a for a in cmd[1:] if a not in ('-fsyntax-only',)]
        argv = (ctypes.c_char_p * len(args))(*[a.encode() for a in args])
        tu = self.lib.clang_parseTranslationUnit(self.index, source.encode(), argv, len(args), None, 0, 0)
        if not tu:
            print('libclang could not parse ' + source)
            return
        module_dir = os.path.realpath(module_dir) + os.sep
        sdk_dir = os.path.realpath(sdk_dir) + os.sep
        lib = self.lib

        def visit(cursor, _parent, _data):
            kind = cursor.kind
            if kind in REF_KINDS:
                use_file, use_line = self.location(cursor)
                if use_file.startswith(module_dir):
                    target = lib.clang_getCursorReferenced(cursor)
                    if not lib.clang_Cursor_isNull(target):
                        template = lib.clang_getSpecializedCursorTemplate(target)
                        if not lib.clang_Cursor_isNull(template):
                            target = template
                        decl_file, decl_line = self.location(target)
                        tkind = lib.clang_getCursorKind(target)
                        if decl_file.startswith(sdk_dir) and tkind != K_PARM:
                            key = (os.path.relpath(decl_file, sdk_dir), decl_line, self.qualified(target), KIND_LABEL.get(tkind, str(tkind)))
                            found.setdefault(key, set()).add('%s:%d' % (os.path.relpath(use_file, module_dir), use_line))
            return 2   # CXChildVisit_Recurse

        callback = VISITOR(visit)
        lib.clang_visitChildren(lib.clang_getTranslationUnitCursor(tu), callback, None)
        lib.clang_disposeTranslationUnit(tu)


def collect(usage, only, config, build_dir):
    report = os.path.join(os.path.abspath(build_dir), 'uht_report.json')
    if not os.path.exists(report):
        sys.exit('run ./run.sh once first (the generated headers of the fake UHT are needed): no ' + report)
    # its own directory for the force-included wrapper: next to the one of run.sh clang would pick up that run's
    # precompiled header (PCH.Abyssfire.h.pch), which is stale as soon as an SDK header changes
    ns = argparse.Namespace(build_dir=os.path.join(build_dir, 'usage'), cxx='clang++', define=[], no_pch=True, force=True, only=only, jobs=1)
    checker = cc.Checker(ns)
    checker.uht_dir = os.path.join(os.path.abspath(build_dir), 'uht')
    units = [u for u in checker.translation_units([]) if u['kind'] == 'module']
    base = checker.command_base(config)
    found = {}
    for unit in units:
        usage.scan(base['cmd'] + ['-include', base['pch_wrapper']], unit['path'], cc.MODULE_DIR, cc.SDK_DIR, found)
    return found, [u['rel'] for u in units]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--only', action='append', default=[], help='module files / directories to scan (as for run.sh; default: the whole module)')
    ap.add_argument('--baseline', action='append', default=[], help='files / directories whose SDK use is subtracted (list only what --only adds)')
    ap.add_argument('--config', default='mac-editor')
    ap.add_argument('--build-dir', default=os.environ.get('ABYSS_CC_BUILD', os.path.join(cc.CHECK_DIR, 'build')))
    ap.add_argument('--uses', action='store_true', help='print up to three module lines that use each declaration')
    ap.add_argument('--header', action='append', default=[], help='only declarations in SDK headers whose path contains this text')
    ap.add_argument('--json', help='also write the result to this file')
    args = ap.parse_args()
    lib = load_libclang()
    if lib is None:
        sys.exit('libclang not found (set ABYSS_LIBCLANG to the shared library of the clang that run.sh uses)')
    usage = Usage(lib)
    found, units = collect(usage, args.only, args.config, args.build_dir)
    if args.baseline:
        base_found, _ = collect(usage, args.baseline, args.config, args.build_dir)
        found = {k: v for k, v in found.items() if k not in base_found}
    by_file = {}
    for (path, line, name, kind), uses in found.items():
        if args.header and not any(h in path for h in args.header):
            continue
        by_file.setdefault(path, []).append((line, name, kind, sorted(uses)))
    total = 0
    for path in sorted(by_file):
        print(path)
        for line, name, kind, uses in sorted(by_file[path]):
            total += 1
            print('  %5d  %-18s %s  (%d use%s)' % (line, kind, name, len(uses), '' if len(uses) == 1 else 's'))
            if args.uses:
                for use in uses[:3]:
                    print('           ' + use)
    print('%d SDK declarations in %d headers used by %d translation unit(s)%s' % (
        total, len(by_file), len(units), ' and not by the baseline' if args.baseline else ''))
    if args.json:
        with open(args.json, 'w', encoding='utf-8') as f:
            json.dump([{'header': p, 'line': l, 'name': n, 'kind': k, 'uses': u} for p in sorted(by_file) for l, n, k, u in sorted(by_file[p])], f, indent=1)
    return 0


if __name__ == '__main__':
    sys.exit(main())

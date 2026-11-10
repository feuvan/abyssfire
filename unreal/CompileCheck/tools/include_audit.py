#!/usr/bin/env python3
"""Include-boundary audit: for every SDK declaration a module file uses, how does that file reach the SDK header that
declares it?

    python3 tools/sdk_usage.py --only Private/UI/Panels --json usage.json     # what is used, and where
    python3 tools/include_audit.py usage.json                                 # how each declaring header is reached
    python3 tools/include_audit.py usage.json --edges                         # the SDK include edges those paths rest on

A module file reaches an SDK header
  direct      it, or a module header it includes, has the #include itself (include what you use);
  transitive  only through other SDK headers: the listed chain must exist in the real engine too, or the file compiles
              here and not there (the SDK is meant to include no more than the engine does);
  pch         only through the force-included private PCH (Private/AbyssfirePCH.h): fine while the module keeps that PCH;
  forward     not at all, but the type is forward-declared on the way (a pointer / TSharedPtr member in a header).
The compile check cannot tell whether an SDK include edge is real: this tool lists the edges a PASS rests on so they can
be reviewed against the engine. #if around an #include is ignored (every branch counts).
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CHECK_DIR = os.path.dirname(HERE)
UNREAL_DIR = os.path.dirname(CHECK_DIR)
MODULE_DIR = os.path.join(UNREAL_DIR, 'Source', 'Abyssfire')
CORE_PUBLIC = os.path.join(UNREAL_DIR, 'Source', 'AbyssCore', 'Public')
SDK_DIR = os.path.join(CHECK_DIR, 'sdk')
INCLUDE_RE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*"([^"]+)"', re.M)


def sdk_roots():
    modules = json.load(open(os.path.join(SDK_DIR, 'modules.json'), encoding='utf-8'))['modules']
    return [os.path.join(SDK_DIR, root) for m in modules.values() for root in m['roots']]


class Graph:
    def __init__(self):
        self.roots = [os.path.join(MODULE_DIR, 'Public'), os.path.join(MODULE_DIR, 'Private'), CORE_PUBLIC] + sdk_roots()
        self.cache = {}
        self.texts = {}

    def resolve(self, name, includer):
        for base in [os.path.dirname(includer)] + self.roots:
            path = os.path.normpath(os.path.join(base, name))
            if os.path.isfile(path):
                return path
        return None   # a system header, a generated header, or missing

    def includes(self, path):
        if path not in self.cache:
            try:
                text = open(path, encoding='utf-8', errors='replace').read()
            except OSError:
                text = ''
            self.cache[path] = [p for p in (self.resolve(n, path) for n in INCLUDE_RE.findall(text)) if p]
        return self.cache[path]

    def module_closure(self, path):
        """The file plus the module / core headers it includes, transitively (not through SDK headers)."""
        seen, todo = {path}, [path]
        while todo:
            for inc in self.includes(todo.pop()):
                if inc not in seen and not inc.startswith(SDK_DIR + os.sep):
                    seen.add(inc)
                    todo.append(inc)
        return seen

    def reachable(self, starts):
        seen, todo = set(starts), list(starts)
        while todo:
            for inc in self.includes(todo.pop()):
                if inc not in seen:
                    seen.add(inc)
                    todo.append(inc)
        return seen

    def forward_declared(self, name, files):
        """True when `class Name;` / `struct Name;` (or an opaque enum, or the elaborated `TSharedPtr<class Name>`)
        appears in one of the files."""
        pattern = re.compile(r'\b(?:class|struct|enum(?:\s+class)?)\s+%s\b(?:\s*:\s*[\w:]+)?\s*[;>*&,)]' % re.escape(name))
        for path in files:
            if path not in self.texts:
                try:
                    self.texts[path] = open(path, encoding='utf-8', errors='replace').read()
                except OSError:
                    self.texts[path] = ''
            if pattern.search(self.texts[path]):
                return True
        return False

    def sdk_chain(self, starts, target):
        """Shortest include chain from one of `starts` (SDK headers) to `target` through SDK headers."""
        prev = {s: None for s in starts}
        todo = list(starts)
        while todo:
            nxt = []
            for cur in todo:
                if cur == target:
                    chain = []
                    while cur is not None:
                        chain.append(cur)
                        cur = prev[cur]
                    return chain[::-1]
                for inc in self.includes(cur):
                    if inc.startswith(SDK_DIR + os.sep) and inc not in prev:
                        prev[inc] = cur
                        nxt.append(inc)
            todo = nxt
        return None


def short(path):
    rel = os.path.relpath(path, SDK_DIR)
    for marker in ('/Public/', '/Classes/'):
        if marker in rel:
            return rel.split(marker, 1)[1]
    return rel


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('usage', help='the --json output of tools/sdk_usage.py')
    ap.add_argument('--edges', action='store_true', help='list the distinct SDK include edges the transitive paths use, with the module files behind each')
    ap.add_argument('--pch', action='store_true', help='also list what is reached only through the PCH')
    args = ap.parse_args()
    graph = Graph()
    pch = os.path.join(MODULE_DIR, 'Private', 'AbyssfirePCH.h')
    pch_direct = [p for p in graph.includes(pch) if p.startswith(SDK_DIR + os.sep)]
    for inc in graph.module_closure(pch):
        pch_direct += [p for p in graph.includes(inc) if p.startswith(SDK_DIR + os.sep)]
    # (module file, SDK header) -> example declarations
    needs = {}
    for row in json.load(open(args.usage, encoding='utf-8')):
        if row['kind'] in ('namespace', 'namespace alias'):
            continue   # a namespace is reopened by many headers: the one libclang names says nothing
        header = os.path.join(SDK_DIR, row['header'])
        for use in row['uses']:
            rel = use.rsplit(':', 1)[0]
            needs.setdefault((rel, header), []).append(row['name'])
    counts = {'direct': 0, 'transitive': 0, 'pch': 0, 'forward': 0, 'unreached': 0}
    transitive, by_pch, edges = [], [], {}
    closures, module_files = {}, {}
    for (rel, header), names in sorted(needs.items()):
        path = os.path.join(MODULE_DIR, rel)
        if path not in closures:
            closure = graph.module_closure(path)
            module_files[path] = closure
            closures[path] = sorted({p for f in closure for p in graph.includes(f) if p.startswith(SDK_DIR + os.sep)})
        direct = closures[path]
        if header in direct:
            counts['direct'] += 1
            continue
        chain = graph.sdk_chain(direct, header)
        if chain:
            counts['transitive'] += 1
            transitive.append((rel, header, chain, names))
            for a, b in zip(chain, chain[1:]):
                edges.setdefault((a, b), set()).add(rel)
            continue
        # a type that is only named (pointer, reference, TSharedPtr<T> member) needs no more than a forward declaration
        visible = module_files[path] | graph.reachable(direct)
        bare = {n.split('<')[0].split('::')[-1] for n in names}
        if all(graph.forward_declared(n, visible) for n in bare):
            counts['forward'] += 1
            continue
        chain = graph.sdk_chain(pch_direct, header)
        if chain:
            counts['pch'] += 1
            by_pch.append((rel, header, chain, names))
        else:
            counts['unreached'] += 1
            by_pch.append((rel, header, [], names))
    if args.edges:
        print('SDK include edges the transitive paths rest on (includer -> included: module files that depend on it)')
        for (a, b), files in sorted(edges.items(), key=lambda kv: (short(kv[0][0]), short(kv[0][1]))):
            print('  %s -> %s   (%d file%s)' % (short(a), short(b), len(files), '' if len(files) == 1 else 's'))
    else:
        for rel, header, chain, names in transitive:
            print('%s uses %s (%s%s)' % (rel, short(header), ', '.join(sorted(set(names))[:2]), ', ...' if len(set(names)) > 2 else ''))
            print('     via ' + ' -> '.join(short(c) for c in chain))
    if args.pch or counts['unreached']:
        for rel, header, chain, names in by_pch:
            if chain and not args.pch:
                continue
            print('%s uses %s only through the PCH%s' % (rel, short(header), ': ' + ' -> '.join(short(c) for c in chain) if chain else ' (no include path found)'))
    print('%d (file, SDK header) pairs: %d direct, %d transitive through SDK headers, %d forward-declared only, %d only through the PCH, %d without an include path' % (
        sum(counts.values()), counts['direct'], counts['transitive'], counts['forward'], counts['pch'], counts['unreached']))
    return 0


if __name__ == '__main__':
    sys.exit(main())

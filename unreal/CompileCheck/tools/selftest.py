#!/usr/bin/env python3
"""Proves the compile check bites: plants known faults in a scratch copy of Source/Abyssfire and requires each one to be
reported at the planted place. Run through  ./run.sh --selftest  (uses <build>/selftest, mac-editor only).

A fault that is NOT reported means the stub SDK or the fake UHT has become too permissive: fix that before trusting a
PASS of the real module.
"""
import json
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CHECK_DIR = os.path.dirname(HERE)
MODULE_DIR = os.path.join(os.path.dirname(CHECK_DIR), 'Source', 'Abyssfire')

# name, file, text to find (must occur exactly once), replacement, expected diagnostic (file suffix, message regex), run
FAULTS = [
    {
        'name': 'wrong engine argument type',
        'what': 'APlayerController::ProjectWorldLocationToScreen(FVector, FVector2D&, bool) called with the two vectors swapped',
        'file': 'Private/Input/AbyssPicking.cpp',
        'find': 'Controller.ProjectWorldLocationToScreen(Corner, Projected,',
        'replace': 'Controller.ProjectWorldLocationToScreen(Projected, Corner,',
        'expect_file': 'Private/Input/AbyssPicking.cpp',
        'expect': r'no viable conversion|cannot bind|no matching (member )?function|cannot initialize a parameter',
        'run': 'A',
    },
    {
        'name': 'missing include',
        'what': '#include "Animation/AnimSequence.h" removed: UAnimSequence is only forward-declared where it is used',
        'file': 'Private/Anim/AbyssAnimInstance.cpp',
        'find': '#include "Animation/AnimSequence.h"\n',
        'replace': '',
        'expect_file': 'Private/Anim/AbyssAnimInstance.cpp',
        'expect': r"incomplete type '(const )?UAnimSequence'",
        'run': 'A',
    },
    {
        'name': '.h / .cpp signature mismatch',
        'what': 'AAbyssGameMode::PlayerCanRestart_Implementation defined with AController* where the header declares APlayerController*',
        'file': 'Private/Framework/AbyssGameMode.cpp',
        'find': 'bool AAbyssGameMode::PlayerCanRestart_Implementation(APlayerController* ',
        'replace': 'bool AAbyssGameMode::PlayerCanRestart_Implementation(AController* ',
        'expect_file': 'Private/Framework/AbyssGameMode.cpp',
        'expect': r"out-of-line definition of 'PlayerCanRestart_Implementation' does not match any declaration",
        'run': 'A',
    },
    {
        'name': 'module missing from Build.cs',
        'what': '"EnhancedInput" removed from the dependency list: its headers must no longer be found',
        'file': 'Abyssfire.Build.cs',
        'find': '"EnhancedInput",',
        'replace': '',
        'expect_file': '',
        'expect': r"'(EnhancedInputComponent|EnhancedInputSubsystems|InputAction|InputActionValue|InputMappingContext|InputModifiers)\.h' file not found",
        'run': 'B',
    },
    {
        'name': 'UHT rule: generated header not last',
        'what': 'an include placed after AbyssCameraRig.generated.h',
        'file': 'Public/Camera/AbyssCameraRig.h',
        'find': '#include "AbyssCameraRig.generated.h"\n',
        'replace': '#include "AbyssCameraRig.generated.h"\n#include "Math/Color.h"\n',
        'expect_file': 'Public/Camera/AbyssCameraRig.h',
        'expect': r'generated-include',
        'run': 'B',
        'uht': True,
    },
    {
        'name': 'UHT rule: UPROPERTY inside a platform #if',
        'what': 'a reflected property wrapped in #if PLATFORM_IOS (UHT does not evaluate it: only WITH_EDITOR / WITH_EDITORONLY_DATA)',
        'file': 'Public/Anim/AbyssAnimInstance.h',
        'find': '\tUPROPERTY(Transient)\n\tTObjectPtr<UAnimSequence> AdditiveSeq;\n',
        'replace': '#if PLATFORM_IOS\n\tUPROPERTY(Transient)\n\tTObjectPtr<UAnimSequence> AdditiveSeq;\n#endif\n',
        'expect_file': 'Public/Anim/AbyssAnimInstance.h',
        'expect': r'preprocessor',
        'run': 'B',
        'uht': True,
    },
]
RUNS = {
    'A': ['Private/Input/AbyssPicking.cpp', 'Private/Anim/AbyssAnimInstance.cpp', 'Private/Framework/AbyssGameMode.cpp'],
    'B': ['Private/Input/AbyssInputConfig.cpp', 'Private/Input/AbyssInputSubsystem.cpp'],
}


# tools/stub_bodies.py must find every body-less template declaration: a miss makes module code that instantiates the
# template with an anonymous-namespace or local type fail with "used but not defined", a false MODULE BUG.
BODIES_PROBE = '''// selftest probe for tools/stub_bodies.py (not an SDK header)
#pragma once
template <typename T>
struct TBodiesProbe
{
	bool Has(const T& Item) const requires requires(const T& A, const T& B) { { A == B } -> std::convertible_to<bool>; };
	void Sort() requires requires(const T& A, const T& B) { { A < B } -> std::convertible_to<bool>; };
	template <typename P> int Count(P Pred) const requires std::is_invocable_r_v<bool, P&, const T&>;
	void Next();
	static constexpr int Size() { return sizeof(T); }
	virtual void Pure() = 0;
};
template <typename T> concept CBodiesProbe = requires(const T& A) { A.Num(); };
template <typename... A> requires (sizeof...(A) > 0) && (std::is_integral_v<A> && ...)
void BodiesProbeJoin(A... Args) UE_STUB_BODY
void BodiesProbeNotATemplate(int Value);
template <typename T> void BodiesProbeAfterConstrainedBody(T Value);
template <typename T> requires requires(const T& A) { A.Num(); }
int BodiesProbeHeadRequires(const T& Range);
'''
BODIES_EXPECT = ['Has(', 'Sort(', 'Count(', 'Next(', 'BodiesProbeAfterConstrainedBody(', 'BodiesProbeHeadRequires(']


def run_bodies_probe(build):
    """stub_bodies.py on a header with every declaration shape: --check counts them, a run fixes exactly those lines."""
    root = os.path.join(build, 'selftest', 'bodies')
    if os.path.exists(root):
        shutil.rmtree(root)
    os.makedirs(root)
    header = os.path.join(root, 'BodiesProbe.h')
    open(header, 'w', encoding='utf-8').write(BODIES_PROBE)
    tool = [sys.executable, os.path.join(HERE, 'stub_bodies.py'), '--sdk', root]
    check = subprocess.run(tool + ['--check'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
    found = re.search(r'stub_bodies: (\d+) declaration', check.stdout)
    found = int(found.group(1)) if found else -1
    subprocess.run(tool, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
    fixed = [line for line in open(header, encoding='utf-8').read().split('\n') if line.rstrip().endswith('UE_STUB_BODY')]
    wrong = [e for e in BODIES_EXPECT if sum(1 for line in fixed if e in line) != 1]
    extra = len(fixed) - 1 - len(BODIES_EXPECT)   # one UE_STUB_BODY is in the probe already
    again = subprocess.run(tool + ['--check'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
    ok = check.returncode == 1 and found == len(BODIES_EXPECT) and not wrong and extra == 0 and again.returncode == 0
    detail = '%d of %d body-less template declarations found (requires-expressions, constrained heads, members), then none left' % (found, len(BODIES_EXPECT))
    if not ok:
        detail = 'found %d (expected %d); not given a body: %s; unexpected bodies: %d; second --check exit %d' % (
            found, len(BODIES_EXPECT), ', '.join(wrong) or '-', extra, again.returncode)
    return ok, [('stub_bodies.py on a probe header', 'REPORTED' if ok else 'NOT DETECTED', detail)]


CASES_FILE = os.path.join(CHECK_DIR, 'tests', 'negative_cases.cpp')
CASE_RE = re.compile(r'^#elif CASE == (\d+)\s*//\s*EXPECT(?:\[([\w-]+)\])?:\s*(.+?)\s*$', re.M)


def run_negative_cases(build, jobs, cxx):
    """tests/negative_cases.cpp: code UE rejects must be rejected by the SDK too; the control case must compile."""
    import argparse
    import concurrent.futures
    import compile_check as cc
    ns = argparse.Namespace(build_dir=build, cxx=cxx, define=[], no_pch=False, force=False, only=[], jobs=jobs)
    checker = cc.Checker(ns)
    text = open(CASES_FILE, encoding='utf-8').read()
    cases = [(int(n), cfg or 'mac-editor', rx) for n, cfg, rx in CASE_RE.findall(text)]
    lines = {int(m.group(1)): text.count('\n', 0, m.start()) + 2 for m in CASE_RE.finditer(text)}
    source = text.split('\n')
    bases = {}
    rows = []
    ok = True
    for config in sorted(set(c for _, c, _ in cases) | {'mac-editor'}):
        base = checker.command_base(config)
        pch, diags = checker.build_pch(config, base)
        if not pch:
            return False, [('negative cases', 'CANNOT RUN', 'the precompiled header of %s does not build: %s' % (config, diags[0]['message'] if diags else '?'))]
        bases[config] = base['cmd'] + ['-include-pch', pch]

    def compile_case(number, config):
        code, output = cc.run(bases[config] + ['-DCASE=%d' % number, CASES_FILE])
        return code, cc.parse_diagnostics(output), output

    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, jobs)) as pool:
        control = {config: pool.submit(compile_case, 0, config) for config in bases}
        futures = [(n, cfg, rx, pool.submit(compile_case, n, cfg)) for n, cfg, rx in cases]
        for config, future in sorted(control.items()):
            code, diags, output = future.result()
            if code != 0 or diags:
                ok = False
                first = diags[0] if diags else {'line': 0, 'message': output.strip()[:200]}
                rows.append(('control (correct code), %s' % config, 'REJECTED', 'tests/negative_cases.cpp:%s: %s' % (first['line'], first['message'])))
            else:
                rows.append(('control (correct code), %s' % config, 'COMPILES', 'the correct twin of every case'))
        missed = 0
        for number, config, rx, future in futures:
            code, diags, output = future.result()
            snippet = source[lines[number] - 1].strip() if lines[number] - 1 < len(source) else ''
            hit = None
            for d in diags:
                if d['level'] == 'error' and d['file'].endswith('negative_cases.cpp') and re.search(rx, d['message'] + ' ' + d['option'], re.I):
                    hit = d
                    break
            if hit is None:
                # the error may be reported inside an SDK template with a note pointing at the case
                for d in diags:
                    if d['level'] == 'error' and re.search(rx, d['message'] + ' ' + d['option'], re.I):
                        hit = d
                        break
            if code == 0 or hit is None:
                ok = False
                missed += 1
                why = 'compiles' if code == 0 else 'fails, but not with /%s/: %s' % (rx, (diags[0]['message'] if diags else output.strip())[:160])
                rows.append(('case %d (%s)' % (number, config), 'NOT DETECTED', '%s  -> %s' % (snippet[:110], why)))
        rows.append(('negative cases', 'REJECTED' if missed == 0 else 'NOT DETECTED', '%d of %d cases of invalid UE code are rejected' % (len(cases) - missed, len(cases))))
    return ok, rows


def main(args=None):
    build = os.path.abspath(getattr(args, 'build_dir', None) or os.environ.get('ABYSS_CC_BUILD') or os.path.join(CHECK_DIR, 'build'))
    root = os.path.join(build, 'selftest')
    ok = True
    rows = []
    for run_name, only in sorted(RUNS.items()):
        scratch = os.path.join(root, run_name, 'Abyssfire')
        if os.path.exists(scratch):
            shutil.rmtree(scratch)
        shutil.copytree(MODULE_DIR, scratch)
        faults = [f for f in FAULTS if f['run'] == run_name]
        for fault in faults:
            path = os.path.join(scratch, fault['file'])
            text = open(path, encoding='utf-8').read()
            if text.count(fault['find']) != 1:
                rows.append((fault['name'], 'CANNOT PLANT', '%s: the text to mutate occurs %d times (update tools/selftest.py)' % (fault['file'], text.count(fault['find']))))
                ok = False
                fault['planted'] = False
                continue
            open(path, 'w', encoding='utf-8').write(text.replace(fault['find'], fault['replace']))
            fault['planted'] = True
        cmd = [sys.executable, os.path.join(HERE, 'compile_check.py'), '--module-dir', scratch, '--build-dir', os.path.join(root, run_name, 'build'),
               '--config', 'mac-editor', '--force']
        for o in only:
            cmd += ['--only', o]
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
        report_path = os.path.join(root, run_name, 'build', 'report.json')
        report = json.load(open(report_path, encoding='utf-8')) if os.path.exists(report_path) else {'configs': {}, 'uht': {'findings': []}}
        diags = []
        for cfg in report.get('configs', {}).values():
            for f in cfg['files']:
                diags += f['diagnostics']
        uht_findings = report.get('uht', {}).get('findings', [])
        if proc.returncode == 0:
            rows.append(('run ' + run_name, 'NOT DETECTED', 'the check passed although faults were planted'))
            ok = False
        for fault in faults:
            if not fault.get('planted'):
                continue
            hit = None
            if fault.get('uht'):
                for f in uht_findings:
                    if f.get('level') == 'error' and f.get('file', '').replace(os.sep, '/').endswith(fault['expect_file']) and re.search(fault['expect'], f.get('rule', '') + ' ' + f.get('message', '')):
                        hit = '%s:%s: [uht/%s] %s' % (f['file'], f['line'], f['rule'], f['message'])
                        break
            else:
                for d in diags:
                    if d['level'] == 'error' and d['file'].replace(os.sep, '/').endswith(fault['expect_file']) and re.search(fault['expect'], d['message']):
                        hit = '%s:%d: %s' % (d['file'], d['line'], d['message'])
                        break
            rows.append((fault['name'], 'REPORTED' if hit else 'NOT DETECTED', hit or fault['what']))
            ok = ok and hit is not None
    bodies_ok, bodies_rows = run_bodies_probe(build)
    ok = ok and bodies_ok
    rows += bodies_rows
    neg_ok, neg_rows = run_negative_cases(build, getattr(args, 'jobs', 3) or 3, getattr(args, 'cxx', None) or 'clang++')
    ok = ok and neg_ok
    rows += neg_rows
    print('== selftest: faults planted in a scratch copy of Source/Abyssfire (%s) and tests/negative_cases.cpp' % root)
    for name, status, detail in rows:
        print('%-13s %s' % (status, name))
        print('              %s' % detail[:300])
    print('SELFTEST: %s' % ('PASS (every planted fault and every negative case was reported)' if ok else 'FAIL (the check missed a planted fault: the SDK / fake UHT is too permissive)'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())

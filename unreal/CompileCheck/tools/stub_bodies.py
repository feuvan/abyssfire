#!/usr/bin/env python3
"""Keeps the stub SDK usable with types that have no linkage.

The SDK is declaration-only, but C++ makes it an error to use an undefined function whose type involves a type without
linkage (a lambda, a local struct) and clang warns for internal linkage (anonymous-namespace types). Module code
instantiates engine templates with such types all the time (TArray<FLocal>, AddLambda([]{...}), MakeShared<FPrivate>()).
In the engine every template is defined in its header, so the stub must define them too.

This tool finds, in sdk/**.h, every function template and every member function of a class template that is declared
without a body and ends it with UE_STUB_BODY (HAL/Platform.h: a body that only calls a [[noreturn]] function, valid for
any return type). It is idempotent: run it after adding or editing SDK headers.

    python3 tools/stub_bodies.py            # rewrite in place, print what changed
    python3 tools/stub_bodies.py --check    # exit 1 when something would change (run.sh uses this)

Not touched: non-template code, constexpr / consteval functions (they need a real body if they are evaluated), functions
with a deduced return type and no trailing type, pure virtuals, = default / = delete, friends, deduction guides, macros.

A trailing requires-clause may end in a requires-expression (`... const requires requires(const T& A) { A == A; };`):
the braces of the requires-expression are not a body, the declaration still gets one.
"""
import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SDK = os.path.join(os.path.dirname(HERE), 'sdk')

TAIL_RE = re.compile(
    r'^(\s*(const|volatile|noexcept(\s*\([^;]*?\))?|override|final|&&|&|UE_LIFETIMEBOUND|'
    r'->[^;]*?|requires\b[^;]*))*\s*$', re.S)
SKIP_START_RE = re.compile(r'^(using|typedef|friend|static_assert|extern|enum|class|struct|union|return|namespace|public|private|protected)\b')
MACRO_ID_RE = re.compile(r'^[A-Z][A-Z0-9]*(_[A-Za-z0-9]+)+$')


def mask_source(text):
    """Same-length copy with comments, string / char literals and preprocessor lines blanked."""
    out = list(text)
    i, n = 0, len(text)
    line_start = True
    while i < n:
        c = text[i]
        if line_start and c in ' \t':
            i += 1
            continue
        if line_start and c == '#':
            j = i
            while j < n:
                k = text.find('\n', j)
                if k < 0:
                    k = n
                cont = text[j:k].rstrip().endswith('\\')
                j = k + 1
                if not cont:
                    break
            for q in range(i, min(j, n)):
                if out[q] != '\n':
                    out[q] = ' '
            i = j
            line_start = True
            continue
        line_start = (c == '\n')
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            k = text.find('\n', i)
            if k < 0:
                k = n
            for q in range(i, k):
                out[q] = ' '
            i = k
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            k = text.find('*/', i + 2)
            k = n if k < 0 else k + 2
            for q in range(i, k):
                if out[q] != '\n':
                    out[q] = ' '
            i = k
            continue
        if c in '"\'':
            # a digit separator (1'000) is not a literal
            if c == '\'' and i > 0 and text[i - 1].isdigit():
                i += 1
                continue
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == '\\' else 1
            for q in range(i + 1, min(j, n)):
                if out[q] != '\n':
                    out[q] = ' '
            i = j + 1
            continue
        i += 1
    return ''.join(out)


def strip_template_heads(stmt):
    """Returns (rest, has_template_head, is_full_specialization)."""
    has_head = False
    full_spec = False
    s = stmt
    while True:
        m = re.match(r'\s*template\s*<', s)
        if not m:
            break
        depth = 1
        i = m.end()
        paren = 0
        while i < len(s) and depth > 0:
            ch = s[i]
            if ch in '([{':
                paren += 1
            elif ch in ')]}':
                paren -= 1
            elif paren == 0 and ch == '<':
                depth += 1
            elif paren == 0 and ch == '>':
                depth -= 1
            i += 1
        params = s[m.end():i - 1].strip()
        if params == '':
            full_spec = True
        else:
            has_head = True
        s = s[i:]
        # a requires-clause that belongs to the head
        m2 = re.match(r'\s*requires\b', s)
        if m2:
            # keep it: handled by the group scan (its tail never matches)
            pass
    return s, has_head, full_spec


def top_level_groups(s):
    """Yields (open_index, close_index) of every parenthesised group at bracket depth 0."""
    depth = 0
    start = -1
    for i, ch in enumerate(s):
        if ch in '([{':
            if depth == 0 and ch == '(':
                start = i
            depth += 1
        elif ch in ')]}':
            depth -= 1
            if depth == 0 and ch == ')' and start >= 0:
                yield start, i
                start = -1


def blank_braces(s):
    """Same-length copy with the content of every top-level {...} blanked: the body of a requires-expression holds ';'
    and nested parentheses that are not part of the declarator."""
    out = list(s)
    depth = 0
    for i, ch in enumerate(s):
        if ch == '{':
            depth += 1
            if depth > 1:
                out[i] = ' '
        elif ch == '}':
            depth -= 1
            if depth > 0:
                out[i] = ' '
        elif depth > 0 and ch != '\n':
            out[i] = ' '
    return ''.join(out)


def classify(stmt, at_class_scope):
    """stmt: masked statement text without the final ';'. Returns 'function', or a reason string to skip."""
    rest, has_head, full_spec = strip_template_heads(stmt)
    body = blank_braces(rest.strip())
    if not body or '(' not in body:
        return 'no-parens', has_head
    if SKIP_START_RE.match(body):
        return 'not-a-function', has_head
    if re.search(r'=\s*(0|delete|default)\s*$', body):
        return 'pure-or-defaulted', has_head
    if 'PURE_VIRTUAL' in body or 'UE_STUB_BODY' in body:
        return 'has-body', has_head
    if re.search(r'\b(constexpr|consteval)\b', body):
        return 'constexpr', has_head
    for open_i, close_i in top_level_groups(body):
        tail = body[close_i + 1:]
        if not TAIL_RE.match(tail):
            continue
        before = body[:open_i].rstrip()
        m = re.search(r'(operator\s*(\(\s*\)|\[\s*\]|[^\s\w(]+|\s+[\w:<>,\s\*&]+?)|~?\w+)\s*$', before)
        if not m:
            return 'no-declarator', has_head
        name = m.group(1)
        prefix = before[:m.start()]
        # an initialiser (int32 X = F();) is not a declaration of a function
        if re.search(r'(?<![=!<>+\-*/%&|^])=(?!=)', re.sub(r'operator\s*[^\s\w(]+', 'operator', prefix)):
            return 'initialiser', has_head
        if MACRO_ID_RE.match(name) and not name.startswith('operator'):
            return 'macro', has_head
        if re.search(r'\bauto\b', prefix) and '->' not in tail:
            return 'deduced-return', has_head
        if not at_class_scope and prefix.strip() == '' and '->' in tail:
            return 'deduction-guide', has_head
        return 'function', has_head
    return 'no-function-declarator', has_head


OUT_OF_LINE_RE = re.compile(r'(\w+)\s*(?:<[^{};<>]*(?:<[^{};<>]*>[^{};<>]*)*>)?\s*::\s*(operator\s*(?:\(\s*\)|\[\s*\]|[^\s\w(]+|\s+[\w:<>,\s\*&]+?)|~?\w+)\s*\($')


def out_of_line_name(stmt_before_params):
    """'template <...> Ret Class<T>::Name' -> ('Class', 'Name') for an out-of-line member definition."""
    m = OUT_OF_LINE_RE.search(stmt_before_params.rstrip() + '(')
    if not m:
        return None
    return m.group(1), re.sub(r'\s+', '', m.group(2))


def process(text, path, report, defined=None, collect=None):
    # UE_STUB_BODY is a body: scan it as "{}" (same length, so indices stay valid)
    mask = mask_source(text).replace('UE_STUB_BODY', '{}' + ' ' * (len('UE_STUB_BODY') - 2))
    edits = []  # indices of ';' to replace
    # scope stack entries: (kind, is_template)
    stack = []
    stmt_start = 0
    paren = 0
    i, n = 0, len(mask)

    def in_body():
        return any(k == 'body' for k, _ in stack)

    def template_class_scope():
        return any(k == 'class' and t for k, t in stack)

    def at_class_scope():
        return bool(stack) and stack[-1][0] == 'class'

    names = []  # class names parallel to the 'class' entries of the stack

    while i < n:
        ch = mask[i]
        if ch in '([':
            paren += 1
        elif ch in ')]':
            paren -= 1
        elif ch == '{':
            stmt = mask[stmt_start:i]
            if in_body() or paren > 0:
                stack.append(('body', False))
            else:
                rest, has_head, full_spec = strip_template_heads(stmt)
                head = rest.strip()
                m = re.match(r'^(class|struct|union)\b', head)
                if re.match(r'^(inline\s+)?namespace\b', head) or re.match(r'^extern\s*$', head) or head == 'extern':
                    stack.append(('ns', False))
                    stmt_start = i + 1
                elif re.match(r'^enum\b', head):
                    stack.append(('body', False))
                elif m and '(' not in re.sub(r'<[^{}]*>', '', head):
                    stack.append(('class', has_head or template_class_scope()))
                    mname = re.match(r'^(?:class|struct|union)\s+(?:[A-Z_]+_API\s+)?(\w+)', head)
                    names.append(mname.group(1) if mname else '')
                    stmt_start = i + 1
                else:
                    if collect is not None and '(' in head:
                        # a function body opening at declaration scope: remember out-of-line member definitions
                        for open_i, close_i in top_level_groups(head):
                            found = out_of_line_name(head[:open_i])
                            if found:
                                collect.add(found)
                    stack.append(('body', False))
        elif ch == '}':
            if stack:
                kind, _ = stack.pop()
                if kind == 'class':
                    names.pop()
                if kind in ('ns', 'class'):
                    stmt_start = i + 1
                elif not in_body() and paren == 0:
                    # a block closed at declaration scope: function body (statement over) or an initialiser / requires
                    stmt = mask[stmt_start:i + 1]
                    opening = find_block_open(mask, i)
                    before = mask[stmt_start:opening].rstrip()
                    is_requires = opens_requires_expression(before)
                    is_init = before.endswith(('=', ',', '(', 'return')) or '(' not in before
                    if not is_requires and not is_init:
                        stmt_start = i + 1
        elif ch == ';' and paren == 0 and not in_body():
            stmt = mask[stmt_start:i]
            if stmt.strip().endswith(':'):
                pass
            # access specifiers end with ':' and may prefix the statement
            stmt_clean = re.sub(r'^\s*((public|private|protected)\s*:\s*)+', '', stmt)
            verdict, has_head = classify(stmt_clean, at_class_scope())
            if verdict == 'function' and (has_head or template_class_scope()) and defined and names:
                declared = declarator_name(stmt_clean)
                if declared and (names[-1], declared) in defined:
                    verdict = 'defined-out-of-line'
            if verdict == 'function' and (has_head or template_class_scope()):
                edits.append(i)
            elif verdict in ('constexpr', 'deduced-return') and (has_head or template_class_scope()):
                report.append((path, text.count('\n', 0, i) + 1, verdict, ' '.join(stmt_clean.split())[:140]))
            stmt_start = i + 1
        i += 1
    if not edits:
        return text, 0
    out = []
    last = 0
    for e in edits:
        out.append(text[last:e])
        out.append(' UE_STUB_BODY')
        last = e + 1
    out.append(text[last:])
    return ''.join(out), len(edits)


def declarator_name(stmt):
    rest, _, _ = strip_template_heads(stmt)
    body = blank_braces(rest.strip())
    for open_i, close_i in top_level_groups(body):
        if TAIL_RE.match(body[close_i + 1:]):
            m = re.search(r'(operator\s*(\(\s*\)|\[\s*\]|[^\s\w(]+|\s+[\w:<>,\s\*&]+?)|~?\w+)\s*$', body[:open_i].rstrip())
            return re.sub(r'\s+', '', m.group(1)) if m else None
    return None


def opens_requires_expression(before):
    """True when the '{' that follows `before` opens the body of a requires-expression: `requires {` or
    `requires (parameters) {`. The parameter list must be the group that ends right here: a requires-clause followed by
    a declarator (`requires (A) && (B) void F(int) {`) opens a function body instead."""
    text = before.rstrip()
    if text.endswith(')'):
        depth = 0
        i = len(text) - 1
        while i >= 0:
            if text[i] == ')':
                depth += 1
            elif text[i] == '(':
                depth -= 1
                if depth == 0:
                    break
            i -= 1
        if i < 0:
            return False
        text = text[:i].rstrip()
    return re.search(r'\brequires$', text) is not None


def find_block_open(mask, close_index):
    depth = 0
    i = close_index
    while i >= 0:
        if mask[i] == '}':
            depth += 1
        elif mask[i] == '{':
            depth -= 1
            if depth == 0:
                return i
        i -= 1
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--check', action='store_true', help='do not write; exit 1 when a declaration lacks a body')
    ap.add_argument('--sdk', default=SDK)
    ap.add_argument('--verbose', action='store_true', help='list every declaration that gets a body')
    ap.add_argument('paths', nargs='*', help='only these headers (default: the whole SDK)')
    args = ap.parse_args()
    files = []
    if args.paths:
        files = [os.path.abspath(p) for p in args.paths]
    else:
        for d, _, fs in os.walk(args.sdk):
            files += [os.path.join(d, f) for f in fs if f.endswith('.h')]
    total = 0
    report = []
    # pass 1: members defined out of line somewhere in the SDK keep their in-class declaration
    defined = set()
    for d, _, fs in os.walk(args.sdk):
        for f in fs:
            if f.endswith('.h'):
                process(open(os.path.join(d, f), encoding='utf-8').read(), f, [], collect=defined)
    for path in sorted(files):
        text = open(path, encoding='utf-8').read()
        new, count = process(text, os.path.relpath(path, args.sdk), report, defined=defined)
        if count:
            total += count
            print('%s: %d declaration(s) %s' % (os.path.relpath(path, args.sdk), count, 'lack a body' if args.check else 'given a body'))
            if args.verbose:
                old_lines = text.split('\n')
                for ln, (a, b) in enumerate(zip(old_lines, new.split('\n')), 1):
                    if a != b:
                        print('   %d: %s' % (ln, b.strip()[:150]))
            if not args.check:
                open(path, 'w', encoding='utf-8').write(new)
    if args.verbose:
        for path, line, why, stmt in report:
            print('note: %s:%d: left declaration-only (%s): %s' % (path, line, why, stmt))
    print('stub_bodies: %d declaration(s) %s in %d header(s)' % (total, 'lack a body' if args.check else 'given a body', len(files)))
    return 1 if (args.check and total) else 0


if __name__ == '__main__':
    sys.exit(main())

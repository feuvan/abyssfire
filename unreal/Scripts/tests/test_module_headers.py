"""Static checks of the UE module's public headers that no compile can run here (no Unreal toolchain in CI).

The UE-free presentation rules live in private, header-only `Private/<Area>/Abyss*Rules.h` files (PresentationTests compile
those alone). A public header that names one of their types (`AbyssMusicRules::Stinger`) compiles only where a private
header happened to be included first: the class's own .cpp includes its header first, and UHT's generated .gen.cpp sees
nothing else, so the module fails to build (2G review B2). Every `Abyss<Area>Rules::` a public header names must be
visible from that header: an include of the rules header or a `namespace Abyss<Area>Rules` declaration in it.
"""
from __future__ import annotations

import re
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
MODULE = HERE.parent.parent / "Source" / "Abyssfire"
PUBLIC = MODULE / "Public"

_RULES_USE = re.compile(r"\b(Abyss\w*Rules)::")


def strip_comments(text: str) -> str:
    """C++ text without // and /* */ comments or string literals (kept: line structure)."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            chunk = text[i:n if j < 0 else j + 2]
            out.append("\n" * chunk.count("\n"))
            i = n if j < 0 else j + 2
        elif c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
            out.append('""')
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def unresolved_rules_uses(text: str) -> list[str]:
    """Names of private rules namespaces the header's code uses without including or declaring them."""
    code = strip_comments(text)
    missing = []
    for name in sorted(set(_RULES_USE.findall(code))):
        included = re.search(rf'#\s*include\s*"[^"]*\b{name}\.h"', text) is not None
        declared = re.search(rf"\bnamespace\s+{name}\b", code) is not None
        if not included and not declared:
            missing.append(name)
    return missing


class PublicHeaderTests(unittest.TestCase):
    def test_public_headers_see_the_rules_types_they_name(self):
        headers = sorted(PUBLIC.rglob("*.h"))
        self.assertTrue(headers, f"no public headers under {PUBLIC}")
        bad = []
        for header in headers:
            for name in unresolved_rules_uses(header.read_text(encoding="utf-8")):
                bad.append(f"{header.relative_to(MODULE)}: names {name}:: without including {name}.h or declaring "
                           f"`namespace {name}` (a private rules type in a public header: pass plain values instead)")
        self.assertEqual(bad, [], "\n".join(bad))

    def test_the_check_sees_code_not_comments(self):
        self.assertEqual(unresolved_rules_uses("// AbyssMusicRules::PhaseStinger picks it\nvoid F();"), [])
        self.assertEqual(unresolved_rules_uses("/* AbyssThemeRules::ThemeOf */ void F();"), [])
        self.assertEqual(unresolved_rules_uses('const char* S = "AbyssMusicRules::x";'), [])
        self.assertEqual(unresolved_rules_uses("void Play(const AbyssMusicRules::Stinger& S);"), ["AbyssMusicRules"])
        self.assertEqual(unresolved_rules_uses('#include "Audio/AbyssMusicRules.h"\n'
                                               "void Play(const AbyssMusicRules::Stinger& S);"), [])
        self.assertEqual(unresolved_rules_uses("namespace AbyssMusicRules { struct Stinger; }\n"
                                               "void Play(const AbyssMusicRules::Stinger& S);"), [])


if __name__ == "__main__":
    unittest.main()

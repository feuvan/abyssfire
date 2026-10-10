"""Every i18n key the Abyssfire UE module passes as a literal must exist in the exported string tables.

The UE layer calls `Loc*` / `LocOr*` / `LocArgs*` / `LocalizeOr` / `NameOr` (and the `{ "key", TEXT("English") }` row
tables of the menus) with an English fallback, so a key missing from Data/i18n_*.json does not crash: the zh-CN build
(the default locale) silently shows English instead. This test greps the module for literal keys and fails on any key
missing from Data/i18n_zh-CN.json or Data/i18n_en.json. Port-only strings belong in PORT_STRINGS
(Tools/export-data/src/tables/i18n.ts); re-run the exporter, then Scripts/fonts/build_fonts.py (the CJK subsets only
contain glyphs that occur in Data/*.json).

Keys built at runtime ("sys.eliteAffix.name." + id) are not checked.
"""
from __future__ import annotations

import json
import re
import unittest
from pathlib import Path

UNREAL = Path(__file__).resolve().parents[2]
SOURCE = UNREAL / "Source" / "Abyssfire"
DATA = UNREAL / "Data"

KEY = r'"([a-z][A-Za-z0-9_]*(?:\.[A-Za-z0-9_]+)+)"'
# Key as the first argument of a localisation helper ...
CALL = re.compile(r"\b(?:LocStr|Loc|LocOr|LocOrStr|LocArgs|LocArgsOr|LocArgsOrStr|LocalizeOr|NameOr|Section|AddCategory|HasKey)"
                  r"\(\s*" + KEY)
# ... or a (key, English fallback) pair in a row table / helper call.
PAIR = re.compile(KEY + r"\s*,\s*TEXT\(")


def used_keys() -> dict[str, list[str]]:
    found: dict[str, list[str]] = {}
    for path in sorted(list(SOURCE.rglob("*.cpp")) + list(SOURCE.rglob("*.h"))):
        text = path.read_text(encoding="utf-8")
        for rx in (CALL, PAIR):
            for m in rx.finditer(text):
                key = m.group(1)
                if key.endswith((".json", ".ini", ".png")):
                    continue
                line = text.count("\n", 0, m.start()) + 1
                found.setdefault(key, []).append(f"{path.relative_to(UNREAL)}:{line}")
    return found


def table(locale: str) -> dict[str, str]:
    return json.loads((DATA / f"i18n_{locale}.json").read_text(encoding="utf-8"))["strings"]


class UiI18nKeysTest(unittest.TestCase):
    def test_literal_keys_exist(self) -> None:
        keys = used_keys()
        self.assertGreater(len(keys), 100, "the key scan found almost nothing - did the helper names change?")
        tables = {locale: table(locale) for locale in ("zh-CN", "en")}
        missing = []
        for key, sites in sorted(keys.items()):
            absent = [locale for locale, strings in tables.items() if key not in strings]
            if absent:
                missing.append(f"{key} (missing in {', '.join(absent)}; {sites[0]})")
        self.assertEqual(missing, [], "i18n keys used by the UE layer but not exported:\n  " + "\n  ".join(missing))

    def test_placeholders_match(self) -> None:
        """A UE key's {args} must be the same in zh-CN and en (the UE passes one arg set for both)."""
        zh, en = table("zh-CN"), table("en")
        bad = []
        for key in used_keys():
            if key in zh and key in en:
                a = set(re.findall(r"\{(\w+)\}", zh[key]))
                b = set(re.findall(r"\{(\w+)\}", en[key]))
                if a != b:
                    bad.append(f"{key}: zh {sorted(a)} vs en {sorted(b)}")
        self.assertEqual(bad, [], "\n".join(bad))


if __name__ == "__main__":
    unittest.main()

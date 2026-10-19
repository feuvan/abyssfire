#!/usr/bin/env python3
"""Writes unreal/CoreTests/golden/zh_simplified_only.txt: the Simplified-only characters content_ch1_test's
"Chapter 1 zh-TW" check looks for in the zh-TW strings Chapter 1 shows (final review R4).

Input: OpenCC 1.x's character dictionaries (Apache-2.0, https://github.com/BYVoid/OpenCC, data/dictionary; the
opencc-python-reimplemented wheel ships the same files under opencc/dictionary). They are read as data only.

    python3 -I unreal/Tools/export-data/scripts/zh_simplified_only.py <OpenCC dictionary dir> [out file]

A character is Simplified-only when it is a BMP key of STCharacters.txt (Simplified -> Traditional candidates) whose
candidates, also after TWVariants.txt (OpenCC Traditional -> Taiwan form), never include the character itself, and it is
neither a TSCharacters.txt key (a Traditional character) nor a TWVariants.txt value (a Taiwan form). 着 is added: OpenCC
keeps it as Traditional, Taiwan writes 著 (TWVariants 着 -> 著).
"""
import os
import sys

HEADER = """# Simplified-only characters: zh-CN forms that never appear in Traditional (Taiwan) text, sorted by code point.
# content_ch1_test "Chapter 1 zh-TW" fails when a zh-TW string Chapter 1 shows contains one of them.
#
# Derived from OpenCC 1.x's dictionaries (Apache-2.0; https://github.com/BYVoid/OpenCC, data/dictionary), read as
# data: every BMP entry of STCharacters.txt (Simplified -> Traditional candidates) whose candidates, also after
# TWVariants.txt (OpenCC Traditional -> Taiwan form: 牀 -> 床, 衆 -> 眾, ...), never include the character itself,
# minus the entries that are TSCharacters.txt keys (Traditional characters) or TWVariants.txt values (Taiwan forms:
# 么 for 幺); plus 着, which OpenCC keeps as Traditional but Taiwan writes 著 (TWVariants 着 -> 著). So 后 里 于 余
# 采 尸 只 台 干 面 系 制 (each also a Traditional character: 皇后, 公里, ...) are not listed; 发 军 们 (Traditional
# 發 / 髮, 軍, 們) are.
# Lines starting with '#' are comments; every other non-space character is a member.
"""


def load(path):
    table = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            key, values = line.split("\t")
            table[key] = values.split(" ")
    return table


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    src = sys.argv[1]
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(here, "../../../CoreTests/golden/zh_simplified_only.txt")
    st = load(os.path.join(src, "STCharacters.txt"))
    ts = load(os.path.join(src, "TSCharacters.txt"))
    twv = load(os.path.join(src, "TWVariants.txt"))
    tw_values = {v for vs in twv.values() for v in vs}

    def tw(c):
        return twv.get(c, [c])[0]

    chars = set()
    for k, v in st.items():
        if len(k) != 1 or ord(k) > 0xFFFF:
            continue
        forms = set(v) | {tw(x) for x in v}
        if k in forms or k in ts or k in tw_values:
            continue
        chars.add(k)
    chars.add("着")
    ordered = sorted(chars)
    lines = ["".join(ordered[i:i + 64]) for i in range(0, len(ordered), 64)]
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write(HEADER + "\n".join(lines) + "\n")
    print(f"{len(ordered)} characters -> {os.path.normpath(out)}")


if __name__ == "__main__":
    main()

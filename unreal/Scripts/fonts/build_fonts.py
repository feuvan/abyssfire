#!/usr/bin/env python3
"""Fetch and subset the bundled UI fonts into unreal/Fonts (ue58-platform.md 9.4, save-ui-input.md 8.2).

UE has no system-font fallback in packaged games, so the CJK + Latin fonts are bundled (OFL) and loaded at runtime from
the staged Fonts/ directory (Abyssfire.Build.cs RuntimeDependencies). Full CJK weights are 8-16 MB each; this script keeps
only the glyphs the game can display: every character of every string in unreal/Data/*.json (the exported zh-CN /
zh-TW / en tables and data names), ASCII, Latin-1, general / CJK punctuation, full-width forms and a few UI symbols.

Re-run it whenever the exported string tables change (after Tools/export-data):

    python3 -m pip install --user fonttools          # one-time (pure Python)
    python3 unreal/Scripts/fonts/build_fonts.py      # downloads to unreal/Scripts/fonts/.cache, writes unreal/Fonts
    python3 unreal/Scripts/fonts/build_fonts.py --check   # exit 1 if a glyph used by the data is missing from the subsets

Options: --common adds the GB2312 level-1 hanzi (3755) for future text without a re-run (+~2 MB per CJK weight);
--offline uses only the cache. Downloads are verified against fonts.lock.json (sha256), which --update-lock rewrites.
Stdlib + fontTools only (Python 3.11).
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
UNREAL = HERE.parent.parent
DATA_DIR = UNREAL / "Data"
FONTS_DIR = UNREAL / "Fonts"
CACHE_DIR = HERE / ".cache"
LOCK_FILE = HERE / "fonts.lock.json"

NOTO = "https://raw.githubusercontent.com/notofonts/noto-cjk/main"
GFONTS = "https://raw.githubusercontent.com/google/fonts/main/ofl/cinzel"

# name -> (url, output file, kind). kind: "cff" (OTF), "var-ttf" (variable TTF instanced at wght).
SOURCES = {
    "NotoSansSC-Regular": (f"{NOTO}/Sans/SubsetOTF/SC/NotoSansSC-Regular.otf", "NotoSansSC-Regular.otf", None),
    "NotoSansSC-Bold": (f"{NOTO}/Sans/SubsetOTF/SC/NotoSansSC-Bold.otf", "NotoSansSC-Bold.otf", None),
    "NotoSansTC-Regular": (f"{NOTO}/Sans/SubsetOTF/TC/NotoSansTC-Regular.otf", "NotoSansTC-Regular.otf", None),
    "NotoSansTC-Bold": (f"{NOTO}/Sans/SubsetOTF/TC/NotoSansTC-Bold.otf", "NotoSansTC-Bold.otf", None),
    "NotoSerifSC-Regular": (f"{NOTO}/Serif/SubsetOTF/SC/NotoSerifSC-Regular.otf", "NotoSerifSC-Regular.otf", None),
    "NotoSerifSC-Bold": (f"{NOTO}/Serif/SubsetOTF/SC/NotoSerifSC-Bold.otf", "NotoSerifSC-Bold.otf", None),
    "NotoSerifTC-Regular": (f"{NOTO}/Serif/SubsetOTF/TC/NotoSerifTC-Regular.otf", "NotoSerifTC-Regular.otf", None),
    "NotoSerifTC-Bold": (f"{NOTO}/Serif/SubsetOTF/TC/NotoSerifTC-Bold.otf", "NotoSerifTC-Bold.otf", None),
    "Cinzel-Regular": (f"{GFONTS}/Cinzel%5Bwght%5D.ttf", "Cinzel-Regular.ttf", 400),
    "Cinzel-Bold": (f"{GFONTS}/Cinzel%5Bwght%5D.ttf", "Cinzel-Bold.ttf", 700),
}
LICENSES = {
    "OFL-NotoCJK.txt": f"{NOTO}/Sans/LICENSE",
    "OFL-Cinzel.txt": f"{GFONTS}/OFL.txt",
}

EXTRA_RANGES = [
    (0x0020, 0x007E),  # ASCII
    (0x00A0, 0x00FF),  # Latin-1 supplement
    (0x2010, 0x2027),  # dashes, quotes, ellipsis, bullets
    (0x2030, 0x203E),  # per mille, primes, reference mark
    (0x2190, 0x2199),  # arrows
    (0x2460, 0x2473),  # circled numbers
    (0x25A0, 0x25FF),  # geometric shapes (triangles, circles, diamonds)
    (0x2605, 0x2606),  # stars
    (0x2713, 0x2717),  # check / cross marks
    (0x3000, 0x303F),  # CJK symbols and punctuation
    (0xFF01, 0xFF5E),  # full-width ASCII forms
    (0xFFE0, 0xFFE6),  # full-width signs
]


def collect_strings(value, out: set[str]) -> None:
    if isinstance(value, str):
        out.update(value)
    elif isinstance(value, dict):
        for k, v in value.items():
            out.update(k)
            collect_strings(v, out)
    elif isinstance(value, list):
        for v in value:
            collect_strings(v, out)


def gb2312_level1() -> set[str]:
    chars = set()
    for hi in range(0xB0, 0xD8):  # level-1 hanzi rows 16-55
        for lo in range(0xA1, 0xFF):
            try:
                chars.add(bytes([hi, lo]).decode("gb2312"))
            except UnicodeDecodeError:
                pass
    return chars


def glyph_set(common: bool) -> set[str]:
    chars: set[str] = set()
    for path in sorted(DATA_DIR.glob("*.json")):
        with path.open(encoding="utf-8") as f:
            collect_strings(json.load(f), chars)
    for lo, hi in EXTRA_RANGES:
        chars.update(chr(c) for c in range(lo, hi + 1))
    if common:
        chars |= gb2312_level1()
    return {c for c in chars if c >= " " and c not in "  ﻿"}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fetch(url: str, dest: Path, offline: bool) -> Path:
    if dest.exists():
        return dest
    if offline:
        sys.exit(f"missing cached download {dest} (run without --offline)")
    dest.parent.mkdir(parents=True, exist_ok=True)
    print(f"download {url}")
    tmp = dest.with_suffix(dest.suffix + ".part")
    with urllib.request.urlopen(url, timeout=120) as resp, tmp.open("wb") as f:
        while chunk := resp.read(1 << 20):
            f.write(chunk)
    tmp.replace(dest)
    return dest


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--common", action="store_true", help="also keep the GB2312 level-1 hanzi")
    ap.add_argument("--offline", action="store_true", help="use cached downloads only")
    ap.add_argument("--update-lock", action="store_true", help="rewrite fonts.lock.json from the downloads")
    ap.add_argument("--check", action="store_true", help="only verify that the existing subsets cover the data")
    ap.add_argument("--cache", type=Path, default=CACHE_DIR, help="download cache directory")
    args = ap.parse_args()
    cache_dir: Path = args.cache

    try:
        from fontTools import subset
        from fontTools.ttLib import TTFont
        from fontTools.varLib import instancer
    except ImportError:
        sys.exit("fontTools is required: python3 -m pip install --user fonttools")

    chars = glyph_set(args.common)
    cjk = sorted(c for c in chars if ord(c) >= 0x2E80)
    print(f"{len(chars)} characters ({len(cjk)} CJK) from {DATA_DIR}")

    if args.check:
        missing = 0
        for name in ("NotoSansSC-Regular.otf", "NotoSansTC-Regular.otf"):
            font = TTFont(FONTS_DIR / name)
            cmap = font.getBestCmap()
            lost = [c for c in cjk if ord(c) not in cmap]
            # SC must cover everything. TC only lacks simplified-only characters; the UI's composite fonts fall back to SC
            # for those (the exported zh-TW table still contains some simplified characters).
            if lost:
                print(f"{name}: {len(lost)} data characters not in this font, e.g. {''.join(lost[:40])}")
            if name.startswith("NotoSansSC"):
                missing += len(lost)
        return 1 if missing else 0

    lock = json.loads(LOCK_FILE.read_text()) if LOCK_FILE.exists() else {}
    new_lock = {}
    FONTS_DIR.mkdir(parents=True, exist_ok=True)
    text = "".join(sorted(chars))

    for name, (url, out_name, wght) in SOURCES.items():
        src = fetch(url, cache_dir / url.rsplit("/", 1)[-1].replace("%5B", "[").replace("%5D", "]"), args.offline)
        digest = sha256(src)
        new_lock[url] = digest
        if not args.update_lock and url in lock and lock[url] != digest:
            sys.exit(f"sha256 mismatch for {url}: {digest} != {lock[url]} (upstream changed; review, then --update-lock)")
        font = TTFont(src)
        if wght is not None and "fvar" in font:
            font = instancer.instantiateVariableFont(font, {"wght": wght}, updateFontNames=True)
        opts = subset.Options()
        opts.layout_features = ["*"]
        opts.name_IDs = ["*"]
        opts.name_languages = ["*"]
        opts.notdef_outline = True
        opts.glyph_names = False
        opts.hinting = True
        opts.desubroutinize = False
        sub = subset.Subsetter(opts)
        sub.populate(text=text)
        sub.subset(font)
        out = FONTS_DIR / out_name
        font.save(out)
        print(f"wrote {out.relative_to(UNREAL)} ({out.stat().st_size / 1024:.0f} KB, {len(font.getGlyphOrder())} glyphs)")

    for out_name, url in LICENSES.items():
        src = fetch(url, cache_dir / out_name, args.offline)
        (FONTS_DIR / out_name).write_bytes(src.read_bytes())

    if args.update_lock or not LOCK_FILE.exists():
        LOCK_FILE.write_text(json.dumps(new_lock, indent=2, sort_keys=True) + "\n")
        print(f"wrote {LOCK_FILE.relative_to(UNREAL)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

"""The bundled UI fonts cover every character the exported data shows (1C review, F1-zh-glyphs).

UE has no system-font fallback in a packaged game: a character missing from the bundled subsets (unreal/Fonts, written
by Scripts/fonts/build_fonts.py from the strings in unreal/Data/*.json) draws as a tofu box. The Simplified Chinese
faces are the fallback of every UI face (AbyssUiStyle.cpp: zh-CN / en body and title text, and zh-TW for the simplified
characters the Traditional subsets lack), so each of them must hold every CJK character of the data. This is the
`build_fonts.py --check` rule as a test, extended to the bold and serif SC faces: a content change that adds a new
character fails here until the subsets are rebuilt (`python3 unreal/Scripts/fonts/build_fonts.py`).

Skipped when fontTools is not installed (it is optional on a build machine).
"""
from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path

try:
    from fontTools.ttLib import TTFont  # type: ignore
except ImportError:  # pragma: no cover - the fonts tooling is optional on a build machine
    TTFont = None

UNREAL = Path(__file__).resolve().parents[2]
FONTS = UNREAL / "Fonts"
BUILD_FONTS = UNREAL / "Scripts" / "fonts" / "build_fonts.py"
SC_FACES = ("NotoSansSC-Regular.otf", "NotoSansSC-Bold.otf", "NotoSerifSC-Regular.otf", "NotoSerifSC-Bold.otf")


def _build_fonts_module():
    spec = importlib.util.spec_from_file_location("abyss_build_fonts", BUILD_FONTS)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)  # stdlib only at import time (fontTools is imported inside main)
    return module


@unittest.skipIf(TTFont is None, "fontTools is not installed")
class FontCoverageTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        chars = _build_fonts_module().glyph_set(common=False)
        cls.cjk = sorted(c for c in chars if ord(c) >= 0x2E80)

    def test_data_has_cjk(self) -> None:
        self.assertGreater(len(self.cjk), 1000)

    def test_simplified_faces_cover_every_data_character(self) -> None:
        for name in SC_FACES:
            with self.subTest(font=name):
                font = TTFont(str(FONTS / name))
                try:
                    cmap = font.getBestCmap()
                finally:
                    font.close()
                missing = "".join(c for c in self.cjk if ord(c) not in cmap)
                self.assertEqual(
                    missing, "",
                    f"{name} lacks {len(missing)} characters the data uses ({missing[:40]}...): run "
                    "python3 unreal/Scripts/fonts/build_fonts.py and commit unreal/Fonts")

    def test_chapter1_names_render(self) -> None:
        # The names the 1C review found as tofu boxes (zone, hub, boss, mini-boss, risen footmen, the relic peddler).
        font = TTFont(str(FONTS / "NotoSansSC-Regular.otf"))
        try:
            cmap = font.getBestCmap()
        finally:
            font.close()
        for text in ("赤幡原", "丧钟堡", "焚旗元帅·瓦德拉斯", "守冢人塔姆欣", "复起步卒", "圣骨贩子"):
            with self.subTest(text=text):
                self.assertEqual("".join(c for c in text if ord(c) not in cmap), "")


if __name__ == "__main__":
    unittest.main()

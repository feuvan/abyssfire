"""Chapter 1 strings fit the UE panels they flow into (dark remake 1C U5 text-fit pass, implementation-plan.md 4 U5).

The remake's Chapter 1 text (quest names and objectives, progress lines, labelled reward choices, the sealed entrance,
random-event lines, discoveries, lore and NPC names) is longer than the web's, above all in English. This test measures
every such string with the bundled fonts (unreal/Fonts, fontTools advance widths at the UI's pixel sizes: a web N px
font is Slate size 0.75 N pt = N px at 96 DPI) against the budgets of the panels that show it, in zh-CN and en, at the
desktop and touch layouts. The budgets mirror the Slate code (file:line comments below); a change on either side that
breaks a fit fails here instead of on the Mac.

What it pins (single-line text must fit; wrapping text must stay within a line count that keeps its box readable):
- quest card (SAbyssQuestCardPanel.cpp): giver names in the 420 px frame title, labelled choice captions in their
  columns (AbyssQuestCardRules::ChoiceColumnWidth), progress lines, the accept preview line;
- quest log (SAbyssQuestLogPanel.cpp): the painted list row, the detail title and objective labels;
- quest tracker (SAbyssHudWidgets.cpp): title and objective lines inside the frame padding, the one-line summary;
- toasts and centre banners (SAbyssNotices.cpp): sealed entrance, quest accepted / turned in, quest complete, the
  hidden-area discovery (wrapped at 720 px);
- combat log (SAbyssHudWidgets.cpp SAbyssCombatLog): every Chapter 1 event / system line fits the log's room;
- puzzle (SAbyssConversationPanels.cpp): prompt lines and the answer button (grows, then shrinks its font);
- lore and dialogue frame titles.

Widths are approximate (no hinting): single-line budgets keep a 4 % margin. Wrapping follows
Source/Abyssfire/Private/UI/Core/AbyssTextFitRules.h (Latin words, CJK characters, no closing mark at a line start).
"""
from __future__ import annotations

import json
import re
import unittest
from pathlib import Path

try:
    from fontTools.ttLib import TTFont  # type: ignore
except ImportError:  # pragma: no cover - the fonts tooling is optional on a build machine
    TTFont = None

UNREAL = Path(__file__).resolve().parents[2]
DATA = UNREAL / "Data"
FONTS = UNREAL / "Fonts"
ZONE = "emerald_plains"
MARGIN = 0.96  # single-line budgets: measured width must stay under 96 % of the room

# face -> font files in fallback order (AbyssUiStyle.cpp: zh-CN / en use the SC faces; Cinzel titles fall back to Sans SC)
FACES = {
    ("body", False): ["NotoSansSC-Regular.otf"],
    ("body", True): ["NotoSansSC-Bold.otf"],
    ("title", False): ["Cinzel-Regular.ttf", "NotoSansSC-Regular.otf"],
    ("title", True): ["Cinzel-Bold.ttf", "NotoSansSC-Bold.otf"],
    ("serif", False): ["NotoSerifSC-Regular.otf", "NotoSansSC-Regular.otf"],
    ("serif", True): ["NotoSerifSC-Bold.otf", "NotoSansSC-Bold.otf"],
}
LOCALES = ("zh-CN", "en")

_FONT_CACHE: dict[str, tuple[dict, dict, int, float]] = {}


def _font(name: str):
    """(cmap, advance widths by glyph name, units per em, line height in em) of a bundled font."""
    if name not in _FONT_CACHE:
        font = TTFont(str(FONTS / name))
        try:
            upm = font["head"].unitsPerEm
            hhea = font["hhea"]
            advances = {glyph: metric[0] for glyph, metric in font["hmtx"].metrics.items()}
            _FONT_CACHE[name] = (font.getBestCmap(), advances, upm, (hhea.ascent - hhea.descent) / upm)
        finally:
            font.close()
    return _FONT_CACHE[name]


def width(text: str, face: str, px: float, bold: bool = False) -> float:
    total = 0.0
    for ch in text:
        for name in FACES[(face, bold)]:
            cmap, advances, upm, _ = _font(name)
            glyph = cmap.get(ord(ch))
            if glyph is not None:
                total += advances[glyph] / upm * px
                break
        else:
            total += px  # no glyph: a tofu box about one em wide
    return total


def line_height(px: float) -> float:
    return _font("NotoSansSC-Regular.otf")[3] * px


# ---- line breaking (AbyssTextFitRules.h) ---------------------------------------------------------------------------
_NO_BREAK_BEFORE = set("，。、！？：；）」』】》”’…·．,.;:!?)]}%")
_NO_BREAK_AFTER = set("（「『【《“‘([{")


def _cjk(ch: str) -> bool:
    c = ord(ch)
    return 0x2E80 <= c <= 0x9FFF or 0xF900 <= c <= 0xFAFF or 0xFE30 <= c <= 0xFE4F or 0xFF00 <= c <= 0xFFEF


def _space(ch: str) -> bool:
    return ch in " \t　"


def _can_break_before(text: str, i: int) -> bool:
    prev, cur = text[i - 1], text[i]
    if _space(cur) or cur in _NO_BREAK_BEFORE or prev in _NO_BREAK_AFTER:
        return False
    if _space(prev):
        return True
    return _cjk(prev) or _cjk(cur)


def wrap(text: str, max_width: float, face: str, px: float, bold: bool = False) -> list[str]:
    """Greedy wrap in a UI font, the same rules as AbyssTextFitRules::WrapLines (Slate's own breaker is close to it)."""
    return wrap_with(text, max_width, lambda t: width(t, face, px, bold))


def wrap_with(text: str, max_width: float, measure) -> list[str]:
    lines: list[str] = []
    for para in text.split("\n"):
        start = 0
        n = len(para)
        while start < n:
            while start < n and _space(para[start]):
                start += 1
            if start >= n:
                break
            fit = start
            nxt = n
            for pos in range(start + 1, n + 1):
                if pos < n and not _can_break_before(para, pos):
                    continue
                if measure(para[start:pos].rstrip()) <= max_width:
                    fit = pos
                    continue
                if fit > start:
                    nxt = fit
                else:
                    cut = start + 1
                    while cut < pos and measure(para[start:cut + 1]) <= max_width:
                        cut += 1
                    while cut < pos and para[cut] in _NO_BREAK_BEFORE:
                        cut += 1
                    nxt = cut
                break
            lines.append(para[start:nxt].rstrip())
            start = nxt
    return lines


def fit_font_px(width_at_px: float, px: float, max_width: float, min_px: float) -> float:
    """AbyssTextFitRules::FitFontPx."""
    if width_at_px <= max_width or width_at_px <= 0:
        return px
    return max(min_px, int(px * max_width / width_at_px * 2) / 2)


# ---- Chapter 1 strings ------------------------------------------------------------------------------------------------
def _json(name: str):
    return json.loads((DATA / name).read_text(encoding="utf-8"))


def strings(locale: str) -> dict[str, str]:
    return _json(f"i18n_{locale}.json")["strings"]


def fill(template: str, **args) -> str:
    for key, value in args.items():
        template = template.replace("{" + key + "}", str(value))
    return template


def chapter_quests() -> list[dict]:
    return [q for q in _json("quests.json")["quests"] if q.get("zone") == ZONE]


def chapter_map() -> dict:
    maps = _json("maps.json")["maps"]
    return maps[ZONE] if isinstance(maps, dict) else next(m for m in maps if m["id"] == ZONE)


def chapter_npcs() -> list[str]:
    zone = chapter_map()
    ids: set[str] = set()
    for camp in zone.get("camps", []):
        ids.update(camp.get("npcs", []))
    for npc in zone.get("fieldNpcs", []):
        ids.add(npc if isinstance(npc, str) else (npc.get("npcId") or npc.get("id")))
    return sorted(i for i in ids if i)


def npc_name(s: dict[str, str], npc_id: str) -> str:
    npc = _json("npcs.json")["npcs"].get(npc_id, {})
    return s.get(npc.get("nameKey") or f"data.npc.{npc_id}.name", npc_id)


def quest_name(s: dict[str, str], quest: dict) -> str:
    return s.get(f"data.quest.{quest['id']}.name", quest["name"])


_PLAIN_TYPES = {"kill", "collect", "explore", "talk", "escort"}


def objective_verb(s: dict[str, str], quest: dict, objective: dict) -> str:
    """FAbyssUiContext::ObjectiveTypeLabel."""
    otype = objective["type"]
    if otype in _PLAIN_TYPES:
        return s.get(f"sys.quest.type.{otype}", otype)
    return s.get(f"sys.quest.objType.{otype}") or s.get(f"sys.quest.type.{quest['type']}", otype)


def objective_label(s: dict[str, str], quest: dict, objective: dict) -> str:
    """ObjectiveTypeLabel + ObjectiveTargetLabel, as the card, the log and the tracker join them."""
    key = objective.get("labelKey") or f"data.questTarget.{objective['targetId']}"
    return f"{objective_verb(s, quest, objective)} {s.get(key, objective.get('targetName', ''))}"


def event_lines(s: dict[str, str]) -> dict[str, str]:
    """The Chapter 1 random-event, discovery, sealed and quest lines the combat log shows (worst-case numbers)."""
    out: dict[str, str] = {}
    for key, text in s.items():
        if key.endswith("." + ZONE) and key.startswith(("sys.event.msg.", "zone.event.")) and ".label." not in key:
            out[key] = fill(text, gold=99999, exp=99999, npcName=s.get(f"sys.event.rescue.{ZONE}", ""))
    for area in chapter_map().get("hiddenAreas", []):
        out[f"discovered {area['id']}"] = fill(s["zone.hiddenArea.discovered"],
                                               areaName=s.get(f"data.hiddenArea.{area['id']}.name", area["name"]))
    for entrance in chapter_map().get("subDungeonEntrances", []):
        if entrance.get("sealedTextKey"):
            out[entrance["sealedTextKey"]] = s[entrance["sealedTextKey"]]
    puzzle = {f: s.get(f"sys.event.puzzle.{ZONE}.{f}", "") for f in ("solution", "reward")}
    out["zone.event.puzzle.solved"] = fill(s["zone.event.puzzle.solved"], **puzzle)
    for quest in chapter_quests():
        name = quest_name(s, quest)
        out[f"turnedIn {quest['id']}"] = fill(s["sys.quest.turnedIn"], name=name, exp=99999, gold=99999)
        out[f"completed {quest['id']}"] = fill(s["sys.quest.completed"], name=name)
    return out


@unittest.skipUnless(TTFont is not None and (FONTS / "NotoSansSC-Regular.otf").exists(), "fontTools or the bundled fonts missing")
class Chapter1TextFitTest(unittest.TestCase):
    maxDiff = None

    def check_all(self, problems: list[str]) -> None:
        self.assertEqual(problems, [], "Chapter 1 text that does not fit its panel:\n  " + "\n  ".join(problems))

    def test_quest_card(self) -> None:
        problems = []
        quests = chapter_quests()
        givers = sorted({q["derived"]["giverNpcId"] for q in quests if q.get("derived", {}).get("giverNpcId")})
        for locale in LOCALES:
            s = strings(locale)
            # frame title (AbyssUiWidgets.cpp SAbyssPanelFrame: 420 - 2 x 40 padding - 2 x 54 flourishes)
            for npc in givers:
                w = width(npc_name(s, npc), "title", 18, True)
                if w > 232 * MARGIN:
                    problems.append(f"{locale} card title {npc!r} {w:.0f}px > 232")
            for quest in quests:
                # name (17 px bold at 380) and the progress line (serif 13 px in quotes)
                if len(wrap(quest_name(s, quest), 380, "body", 17, True)) > 2:
                    problems.append(f"{locale} card name {quest['id']} > 2 lines")
                if quest.get("progressKey"):
                    rows = wrap("“" + s[quest["progressKey"]] + "”", 380, "serif", 13)
                    if len(rows) > 3:
                        problems.append(f"{locale} progress {quest['id']} {len(rows)} lines")
                rewards = quest.get("rewards", {})
                choices = rewards.get("choices") or []
                labels = rewards.get("choiceLabels") or []
                if not choices:
                    continue
                # labelled captions: AbyssQuestCardRules::ChoiceColumnWidth(n, 380, slot, 10, 150), 11 / 13 px bold
                for touch, slot, px, sub_px in ((False, 44, 11, 9), (True, 56, 13, 11)):
                    column = max(slot, min(150.0, (380 - 10 * (len(choices) - 1)) / len(choices)))
                    for key, slot_id in zip(labels, choices):
                        if len(wrap(s[key], column, "body", px, True)) > 2:
                            problems.append(f"{locale} {'touch' if touch else 'desktop'} choice {key} > 2 lines at {column:.0f}px")
                        slot_name = s.get(f"sys.questCard.choice.{slot_id}", slot_id)
                        if width(slot_name, "body", sub_px) > column * MARGIN:
                            problems.append(f"{locale} choice slot {slot_id} does not fit {column:.0f}px")
                # accept preview line (11 px at 380): "label · slot / ..."
                parts = [(s[k] + " · " if labels else "") + s.get(f"sys.questCard.choice.{c}", c)
                         for k, c in zip(labels or choices, choices)]
                preview = fill(s["ui.questCard.choicePreview"], slots=" / ".join(parts))
                if len(wrap(preview, 380, "body", 11)) > 3:
                    problems.append(f"{locale} accept preview {quest['id']} > 3 lines")
        self.check_all(problems)

    def test_quest_log(self) -> None:
        problems = []
        for locale in LOCALES:
            s = strings(locale)
            for quest in chapter_quests():
                tag = s["ui.questLog.mainTag" if quest["category"] == "main" else "ui.questLog.sideTag"]
                # painted list row (SAbyssQuestLogPanel.cpp AbyssQuestLog_Row: x 10 in a 250 px row, bold when selected,
                # tracked arrow and done tick around it); no wrap, so it must fit
                row = f"➤ {tag} {quest_name(s, quest)} ✓"
                w = width(row, "body", 12, True)
                if w > 234 * MARGIN:
                    problems.append(f"{locale} log row {quest['id']} {w:.0f}px > 234")
                if len(wrap(quest_name(s, quest), 404, "body", 16, True)) > 2:
                    problems.append(f"{locale} log title {quest['id']} > 2 lines")
                for index, objective in enumerate(quest["objectives"]):
                    label = "○ " + objective_label(s, quest, objective)
                    room = 306 if objective.get("itemKind") or objective["type"] == "collect" else 330
                    if len(wrap(label, room, "body", 12)) > 2:
                        problems.append(f"{locale} log objective {quest['id']}[{index}] > 2 lines")
        self.check_all(problems)

    def test_tracker(self) -> None:
        problems = []
        # SAbyssHudWidgets.cpp SAbyssQuestTracker: width 218 / 330, 9 px padding a side; title 12 / 19 px bold;
        # summary 10 / 16 px (no wrap); objectives 9 / 14 px indented 12 / 16 (+ 19 for an item icon)
        for touch, frame, title_px, summary_px, obj_px, indent in ((False, 218, 12, 10, 9, 12), (True, 330, 19, 16, 14, 16)):
            content = frame - 18
            layout = "touch" if touch else "desktop"
            for locale in LOCALES:
                s = strings(locale)
                done = s["sys.tracker.completed"]
                if width(done, "body", summary_px) > content * MARGIN:
                    problems.append(f"{locale} {layout} tracker summary {done!r}")
                for quest in chapter_quests():
                    tag = s["ui.questTracker.mainTag" if quest["category"] == "main" else "ui.questTracker.sideTag"]
                    title = f"▶ {tag} {quest_name(s, quest)} ✓"
                    if len(wrap(title, content, "body", title_px, True)) > 2:
                        problems.append(f"{locale} {layout} tracker title {quest['id']} > 2 lines")
                    if len(quest["objectives"]) == 1:
                        summary = f"{objective_verb(s, quest, quest['objectives'][0])} 99/99"
                        if width(summary, "body", summary_px) > content * MARGIN:
                            problems.append(f"{locale} {layout} tracker summary {quest['id']}")
                    for index, objective in enumerate(quest["objectives"]):
                        room = content - indent - (19 if objective.get("itemKind") or objective["type"] == "collect" else 0)
                        line = objective_label(s, quest, objective) + " 99/99"
                        if len(wrap(line, room, "body", obj_px)) > 2:
                            problems.append(f"{locale} {layout} tracker objective {quest['id']}[{index}] > 2 lines")
        self.check_all(problems)

    def test_toasts_and_banners(self) -> None:
        problems = []
        zone = chapter_map()
        for locale in LOCALES:
            s = strings(locale)
            # toasts (SAbyssNotices.cpp PaintToasts: one line, 15 px bold, box = text + 36)
            toasts = {e["sealedTextKey"]: s[e["sealedTextKey"]] for e in zone.get("subDungeonEntrances", []) if e.get("sealedTextKey")}
            for quest in chapter_quests():
                name = quest_name(s, quest)
                toasts[f"accepted {quest['id']}"] = fill(s["sys.questCard.accepted"], name=name)
                toasts[f"turnedIn {quest['id']}"] = fill(s["sys.questCard.turnedIn"], name=name)
            for key, text in toasts.items():
                w = width(text, "body", 15, True) + 36
                if w > 720 * MARGIN:
                    problems.append(f"{locale} toast {key} {w:.0f}px > 720")
            # quest complete banner: name 18 px bold, "return to <giver>" 14 px, one line each at 720
            for quest in chapter_quests():
                giver = quest.get("derived", {}).get("giverNpcId")
                if len(wrap(quest_name(s, quest), 720, "body", 18, True)) > 1:
                    problems.append(f"{locale} quest-complete banner {quest['id']} wraps")
                if giver and len(wrap(fill(s["zone.quest.returnTo"], npc=npc_name(s, giver)), 720, "body", 14)) > 1:
                    problems.append(f"{locale} quest-complete hint {quest['id']} wraps")
            # discovery banner: Cinzel 22 px bold wrapped at 720, at most two lines
            for area in zone.get("hiddenAreas", []):
                text = s.get(f"data.hiddenArea.{area['id']}.discovery", area.get("discoveryText", ""))
                rows = wrap(text, 720, "title", 22, True)
                if len(rows) > 2:
                    problems.append(f"{locale} discovery {area['id']} {len(rows)} lines")
            name = s.get(f"data.zone.{ZONE}", ZONE)
            if width(name, "title", 28, True) > 720 * MARGIN:
                problems.append(f"{locale} zone banner name")
        self.check_all(problems)

    def test_combat_log_lines(self) -> None:
        problems = []
        # SAbyssCombatLog::Rebuild: wrap at width - 20; room = height - 22 - 6 framed (desktop 290x150, touch expanded
        # 470x300) or the whole height collapsed (touch 470x120); a row is the font's line height + the outline
        layouts = (("desktop", 12, 2, 270, 122), ("touch collapsed", 19, 3, 450, 120), ("touch expanded", 19, 3, 450, 272))
        for locale in LOCALES:
            s = strings(locale)
            for key, text in event_lines(s).items():
                for layout, px, outline, wrap_w, room in layouts:
                    rows = len(wrap(text, wrap_w, "body", px))
                    height = rows * (line_height(px) + outline)
                    if height > room:  # a newer line taller than the room loses its top rows
                        problems.append(f"{locale} {layout} log {key}: {rows} rows, {height:.0f}px > {room}px")
        self.check_all(problems)

    def test_puzzle_and_frames(self) -> None:
        problems = []
        for locale in LOCALES:
            s = strings(locale)
            prompt = s.get(f"sys.event.puzzle.{ZONE}.prompt", "")
            if len(wrap(prompt, 370, "body", 14)) > 4:
                problems.append(f"{locale} puzzle prompt > 4 lines")
            solution = s.get(f"sys.event.puzzle.{ZONE}.solution", "")
            # SAbyssPuzzlePanel::Refresh: the answer button grows to 380 px (392 content - 12), the label shrinks past it
            for layout, px in (("desktop", 13), ("touch", 16)):
                label = width(solution, "body", px, True)
                button = min(max(label + 24, 200), 380)
                fitted = fit_font_px(label, px, button - 20, 10)
                if fitted < px - 2:
                    problems.append(f"{locale} {layout} puzzle answer shrinks to {fitted}px")
            # lore panel (440 px frame: 252 px title band) and dialogue panel (480 px: 292 px) titles at >= 16 px
            for key, text in s.items():
                if key.startswith("data.lore.lore_rf") and key.endswith(".name"):
                    px = fit_font_px(width(text, "title", 18, True), 18, 252 * MARGIN, 13)
                    if px < 16:
                        problems.append(f"{locale} lore title {key} shrinks to {px}px")
            for npc in chapter_npcs():
                name = npc_name(s, npc)
                if width(name, "title", 18, True) > 292 * MARGIN:
                    problems.append(f"{locale} dialogue title {npc}")
                if fit_font_px(width(name, "title", 18, True), 18, 232 * MARGIN, 13) < 15:
                    problems.append(f"{locale} quest-card title {npc} would shrink below 15 px")
        self.check_all(problems)

    def test_wrap_rules_mirror_the_helper(self) -> None:
        """The Python breaker follows AbyssTextFitRules (unit widths as in textfit_test.cpp)."""
        def unit_wrap(text: str, max_width: float) -> list[str]:
            return wrap_with(text, max_width, lambda t: float(sum(1 if ord(c) < 0x80 else 2 for c in t)))
        self.assertEqual(unit_wrap("aa bb cc", 5), ["aa bb", "cc"])
        self.assertEqual(unit_wrap("abcdefgh", 3), ["abc", "def", "gh"])
        self.assertEqual(unit_wrap("一二。三", 4), ["一", "二。", "三"])
        self.assertEqual(unit_wrap("一“二三”", 4), ["一", "“二", "三”"])
        self.assertEqual(unit_wrap("一，二", 2), ["一，", "二"])
        self.assertEqual(unit_wrap("The Marshal’s Last Order", 13), ["The", "Marshal’s", "Last Order"])


if __name__ == "__main__":
    unittest.main()

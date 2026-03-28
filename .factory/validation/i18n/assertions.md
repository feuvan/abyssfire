# i18n Foundation — Validation Assertions

## i18n Core System

### VAL-FOUND-001: t() returns correct zh-CN string
Call `t('menu.new_game')` with locale set to `zh-CN`. The returned string must be `'新的旅程'` (or the exact zh-CN translation defined in the locale file). Pass if the correct zh-CN string is returned; fail if English fallback or undefined is returned.
Tool: agent-browser
Evidence: Console eval of `t('menu.new_game')` output, screenshot showing zh-CN text rendered

### VAL-FOUND-002: t() returns correct en string
Call `t('menu.new_game')` with locale set to `en`. The returned string must be the English translation (e.g. `'New Journey'` or equivalent). Pass if a non-empty English string is returned; fail if Chinese text or undefined is returned.
Tool: agent-browser
Evidence: Console eval of `t('menu.new_game')` output, screenshot showing English text rendered

### VAL-FOUND-003: t() returns correct zh-TW string
Call `t('menu.new_game')` with locale set to `zh-TW`. The returned string must be the Traditional Chinese translation. Pass if a non-empty zh-TW string is returned that differs from zh-CN where character differences exist; fail if zh-CN or English is returned.
Tool: agent-browser
Evidence: Console eval of `t('menu.new_game')` output, screenshot showing zh-TW text rendered

### VAL-FOUND-004: t() parameter interpolation works
Call `t('menu.continue', { class: 'Warrior', level: 5 })` (or equivalent key with parameters). The returned string must have `'Warrior'` and `'5'` substituted into the template. Pass if parameters are correctly interpolated; fail if raw template placeholders like `{class}` or `{level}` remain visible.
Tool: agent-browser
Evidence: Console eval showing interpolated result string

### VAL-FOUND-005: t() parameter interpolation with zh-CN class names
Call `t('menu.continue', { class: '战士', level: 10 })` with locale `zh-CN`. The returned string must contain `'战士'` and `'10'` properly embedded. Pass if Chinese class name and numeric level are interpolated; fail if placeholders remain or string is malformed.
Tool: agent-browser
Evidence: Console eval showing interpolated zh-CN result

### VAL-FOUND-006: Missing key falls back to en
Set locale to `zh-CN`. Call `t()` with a key that exists only in the `en` locale file (not in `zh-CN`). The returned string must be the English fallback value. Pass if English string is returned; fail if undefined, empty string, or raw key is returned.
Tool: agent-browser
Evidence: Console eval showing fallback string returned for missing zh-CN key

### VAL-FOUND-007: Completely missing key returns key path or safe fallback
Call `t('nonexistent.totally.fake.key')`. The function must not throw and must return either the key path itself as a string or a defined fallback marker. Pass if no exception and a non-undefined value is returned; fail if an exception is thrown or `undefined` is returned.
Tool: agent-browser
Evidence: Console eval showing the return value for a nonexistent key

### VAL-FOUND-008: Locale switching updates t() output immediately
Set locale to `zh-CN`, confirm `t('menu.new_game')` returns Chinese. Switch locale to `en`, confirm `t('menu.new_game')` now returns English. Pass if the returned value changes immediately after switching; fail if the old locale's string persists.
Tool: agent-browser
Evidence: Console eval showing before/after values across locale switch

### VAL-FOUND-009: All three locales (zh-CN, zh-TW, en) are registered
Inspect the i18n system's available locales. The system must report exactly three locales: `zh-CN`, `zh-TW`, `en`. Pass if all three are present; fail if any is missing or extras exist.
Tool: agent-browser
Evidence: Console eval listing available locales

### VAL-FOUND-010: Default locale is zh-CN on first visit
Clear localStorage (or use incognito context). Load the game. The active locale must default to `zh-CN`. Pass if the default locale is `zh-CN`; fail if it defaults to `en` or `zh-TW`.
Tool: agent-browser
Evidence: Console eval of current locale after fresh load, screenshot of menu showing zh-CN text

## Persistence

### VAL-FOUND-011: Language selection persists to localStorage
Switch language to `en` via the language selector. Check `localStorage.getItem('abyssfire_locale')`. The value must be `'en'`. Pass if localStorage contains the correct locale string; fail if the key is missing or value is wrong.
Tool: agent-browser
Evidence: Console eval of localStorage value after language switch

### VAL-FOUND-012: Language restored from localStorage on reload
Set locale to `en`, confirm it is persisted. Reload the page. After reload, the active locale must be `en` and all menu text must display in English. Pass if English is restored; fail if it reverts to zh-CN.
Tool: agent-browser
Evidence: Screenshot of menu after reload showing English text, console eval confirming locale is `en`

### VAL-FOUND-013: Cleared localStorage reverts to default zh-CN
Set locale to `en`, confirm persistence. Clear localStorage entirely (`localStorage.clear()`). Reload the page. The game must default to `zh-CN`. Pass if zh-CN is active after reload; fail if English persists or an error occurs.
Tool: agent-browser
Evidence: Console eval of locale after clear + reload, screenshot showing zh-CN menu

### VAL-FOUND-014: Invalid localStorage locale value handled gracefully
Set `localStorage.setItem('abyssfire_locale', 'xx-INVALID')`. Reload the page. The game must not crash and must fall back to the default locale (`zh-CN`). Pass if game loads with zh-CN; fail if crash, blank screen, or the invalid locale is used.
Tool: agent-browser
Evidence: Screenshot of menu loading normally, console eval confirming fallback locale

## zh-TW Auto-Conversion

### VAL-FOUND-015: zh-TW strings differ from zh-CN where expected
Compare `t('menu.new_game')` in zh-CN vs zh-TW. Characters like 游/遊, 选/選, 战/戰 etc. must be converted. Pass if at least some characters differ between zh-CN and zh-TW for strings containing convertible characters; fail if zh-TW is byte-identical to zh-CN.
Tool: agent-browser
Evidence: Console eval comparing zh-CN and zh-TW strings side by side

### VAL-FOUND-016: zh-TW conversion produces valid Traditional Chinese
Switch to `zh-TW`. Inspect several UI strings (menu buttons, class names, help text). All converted text must be valid Traditional Chinese — no mojibake, no mixed simplified/traditional within a single string. Pass if text reads correctly as Traditional Chinese; fail if garbled or inconsistent.
Tool: agent-browser
Evidence: Screenshot of full menu in zh-TW, manual verification of character correctness

### VAL-FOUND-017: zh-TW auto-converter uses no external library
Inspect the zh-TW conversion code. It must use a character mapping table (object/Map) within the codebase, not an npm package like `opencc` or similar. Pass if conversion is self-contained; fail if external i18n conversion lib is imported.
Tool: agent-browser
Evidence: Code inspection of the converter module, `package.json` dependency check

### VAL-FOUND-018: zh-TW conversion handles characters not in mapping table
Strings containing ASCII, numbers, punctuation, and characters not in the zh-CN→zh-TW mapping must pass through unchanged. E.g. `'Lv.5'` embedded in a zh-TW string must remain `'Lv.5'`. Pass if non-mapped characters are preserved; fail if they are corrupted or removed.
Tool: agent-browser
Evidence: Console eval of a zh-TW string containing mixed CJK and ASCII

## Language Selector

### VAL-MENU-001: Language selector appears in main menu
Load the game. The main menu must display a language selector button/entry alongside existing buttons (New Game, Help, OST, Credits). Pass if a language-related button or selector is visible; fail if no language option exists.
Tool: agent-browser
Evidence: Screenshot of main menu showing language selector button

### VAL-MENU-002: Language selector offers exactly 3 options
Click/activate the language selector. It must present exactly 3 language options: 简体中文 (zh-CN), 繁體中文 (zh-TW), English (en). Pass if all 3 are listed; fail if fewer, more, or incorrectly labeled.
Tool: agent-browser
Evidence: Screenshot showing all 3 language options visible

### VAL-MENU-003: Selecting English changes menu language immediately
Click `English` in the language selector. All menu text must switch to English without requiring a reload or scene restart. Pass if all visible text updates to English immediately; fail if any text remains in Chinese or requires manual refresh.
Tool: agent-browser
Evidence: Screenshot of menu immediately after selecting English

### VAL-MENU-004: Selecting zh-TW changes menu language immediately
Click `繁體中文` in the language selector. All menu text must switch to Traditional Chinese without reload. Pass if text updates immediately to zh-TW; fail if text stays in zh-CN or English.
Tool: agent-browser
Evidence: Screenshot of menu immediately after selecting zh-TW

### VAL-MENU-005: Selecting zh-CN restores original Chinese text
After switching to English, click `简体中文`. All menu text must return to Simplified Chinese, matching the original pre-i18n text. Pass if all text matches expected zh-CN strings; fail if any residual English or zh-TW remains.
Tool: agent-browser
Evidence: Screenshot of menu after switching back to zh-CN

### VAL-MENU-006: Language selector itself updates its label on switch
After switching to English, the language selector button/entry itself must display in English (e.g. "Language" instead of "语言"). Pass if the selector's own label is localized; fail if it remains in the previous language.
Tool: agent-browser
Evidence: Screenshot showing language selector label in English

## MenuScene — Title Area

### VAL-MENU-007: Title "ABYSSFIRE" stays constant across all languages
Switch between all 3 languages. The English title "ABYSSFIRE" must remain unchanged — it is a proper noun/brand. Pass if "ABYSSFIRE" is visible in all 3 locales; fail if it is translated or removed.
Tool: agent-browser
Evidence: Screenshots of title area in zh-CN, zh-TW, en

### VAL-MENU-008: Subtitle "渊 火" changes appropriately per locale
In zh-CN the subtitle should be "渊 火" (or equivalent). In zh-TW it should be "淵 火" (traditional). In English it may be omitted or show a subtitle like "Abyssfire". Pass if subtitle reflects the locale; fail if zh-CN subtitle shows in zh-TW unchanged, or is absent in en without intentional design.
Tool: agent-browser
Evidence: Screenshots of subtitle in all 3 locales

### VAL-MENU-009: Version string format is consistent across locales
Switch between all 3 languages. The version string (e.g. `v0.22.0`) must remain visible and unchanged — version numbers are not localized. Pass if version text is present in all locales; fail if it disappears or changes.
Tool: agent-browser
Evidence: Screenshots showing version string in each locale

## MenuScene — Buttons

### VAL-MENU-010: "Continue Game" button text is localized
Load a game with a save file. Switch to each locale. The continue button must show: zh-CN `继续游戏 - [class] Lv.[level]`, zh-TW `繼續遊戲 - [class] Lv.[level]`, en `Continue - [class] Lv.[level]` (or similar). Pass if text changes per locale with correct class/level interpolation; fail if text is hardcoded to one language.
Tool: agent-browser
Evidence: Screenshots of continue button in all 3 locales

### VAL-MENU-011: "Continue Game" subtitle text is localized
The subtitle under the continue button (currently "继续你的冒险") must change per locale. Pass if subtitle text changes across all 3 locales; fail if it stays in zh-CN.
Tool: agent-browser
Evidence: Screenshots showing subtitle in each locale

### VAL-MENU-012: "New Game" button text is localized
Switch to each locale. The new game button must show: zh-CN `新的旅程`, en `New Journey` (or equivalent), zh-TW traditional equivalent. Pass if text changes; fail if hardcoded.
Tool: agent-browser
Evidence: Screenshots of new game button in all 3 locales

### VAL-MENU-013: "Help" button text is localized
Switch to each locale. The help button must show: zh-CN `游戏控制`, en `Controls` (or equivalent), zh-TW traditional equivalent. Pass if text changes; fail if hardcoded.
Tool: agent-browser
Evidence: Screenshots of help button in all 3 locales

### VAL-MENU-014: "OST" button text is localized
Switch to each locale. The soundtrack button must show: zh-CN `原声音乐`, en `Soundtrack` (or equivalent), zh-TW traditional equivalent. Pass if text changes; fail if hardcoded.
Tool: agent-browser
Evidence: Screenshots of OST button in all 3 locales

### VAL-MENU-015: "Credits" button text is localized
Switch to each locale. The credits button must show: zh-CN `制作名单`, en `Credits` (or equivalent), zh-TW traditional equivalent. Pass if text changes; fail if hardcoded.
Tool: agent-browser
Evidence: Screenshots of credits button in all 3 locales

## MenuScene — Class Selection

### VAL-MENU-016: Class selection title is localized
Click "New Game" to enter class selection. Switch to each locale. The title "选 择 职 业" must translate. Pass if title changes per locale; fail if hardcoded in zh-CN.
Tool: agent-browser
Evidence: Screenshots of class selection title in all 3 locales

### VAL-MENU-017: Class names are localized
In class selection, class names must change per locale: zh-CN `战士 Warrior` / `法师 Mage` / `盗贼 Rogue`, en `Warrior` / `Mage` / `Rogue`, zh-TW traditional equivalents. Pass if class names change with locale; fail if hardcoded.
Tool: agent-browser
Evidence: Screenshots of class list in all 3 locales

### VAL-MENU-018: Class descriptions are localized
Each class description (e.g. zh-CN "钢铁意志,剑盾无双") must translate per locale. Pass if descriptions change; fail if hardcoded in zh-CN.
Tool: agent-browser
Evidence: Screenshots of class descriptions in all 3 locales

### VAL-MENU-019: "Back" button in class selection is localized
The "← 返回" button must translate per locale (e.g. en: "← Back"). Pass if text changes; fail if hardcoded.
Tool: agent-browser
Evidence: Screenshots of back button in all 3 locales

## MenuScene — Help Panel

### VAL-MENU-020: Help panel title is localized
Open the help panel. Switch to each locale. The title "快捷键" must translate (e.g. en: "Keyboard Shortcuts" or "Controls"). Pass if title changes; fail if hardcoded.
Tool: agent-browser
Evidence: Screenshots of help panel title in all 3 locales

### VAL-MENU-021: Help panel category headers are localized
Category headers "移动", "战斗", "界面" must translate per locale. Pass if all category headers change; fail if any remain in zh-CN when locale is en or zh-TW.
Tool: agent-browser
Evidence: Screenshots of help panel categories in all 3 locales

### VAL-MENU-022: Help panel key descriptions are localized
Action descriptions (e.g. "上 / 左 / 下 / 右移动", "使用技能", "背包") must translate per locale. Pass if all descriptions change; fail if any remain hardcoded.
Tool: agent-browser
Evidence: Screenshots of help panel descriptions in all 3 locales

### VAL-MENU-023: Help panel "Back" button is localized
The "← 返回" close button in the help panel must translate. Pass if text changes per locale; fail if hardcoded.
Tool: agent-browser
Evidence: Screenshots of help panel close button in all 3 locales

### VAL-MENU-024: Switching language while help panel is open updates text
Open help panel in zh-CN. Switch language to English (via keyboard shortcut or if selector is accessible). All help panel text must update without closing and reopening the panel. Pass if text updates in-place; fail if panel must be closed and reopened, OR if a design decision intentionally requires panel re-open (document which behavior).
Tool: agent-browser
Evidence: Screenshot of help panel before and after mid-panel language switch

## MenuScene — Jukebox Panel

### VAL-MENU-025: Jukebox panel header is localized
Open the jukebox. The header "ABYSSFIRE OST" may remain (brand name), but the subtitle "原声音乐集 · N 首 · MM:SS" must translate. Pass if subtitle changes per locale; fail if hardcoded in zh-CN.
Tool: agent-browser
Evidence: Screenshots of jukebox header in all 3 locales

### VAL-MENU-026: Jukebox track titles are localized
Track titles like "渊火 · 序章" and "翠绿平原 · 探索" must translate per locale. Pass if track names change in en and zh-TW; fail if always zh-CN.
Tool: agent-browser
Evidence: Screenshots of track list in all 3 locales

### VAL-MENU-027: Jukebox "Back" button is localized
The "← 返回" close button must translate. Pass if text changes per locale; fail if hardcoded.
Tool: agent-browser
Evidence: Screenshots of jukebox close button in all 3 locales

## MenuScene — Credits Panel

### VAL-MENU-028: Credits panel title is localized
Open credits. The title "制作名单" must translate (e.g. en: "Credits"). Pass if title changes per locale; fail if hardcoded.
Tool: agent-browser
Evidence: Screenshots of credits panel title in all 3 locales

### VAL-MENU-029: Credits panel section headers are localized
Section headers "地图美术", "背景音乐", "游戏引擎" must translate per locale. Pass if headers change; fail if hardcoded in zh-CN.
Tool: agent-browser
Evidence: Screenshots of credits section headers in all 3 locales

### VAL-MENU-030: Credits license text and artist names remain unchanged
Artist names (e.g. "Kenney", "DST", "cynicmusic") and license identifiers (e.g. "CC0", "GPL 2.0") are proper nouns/legal identifiers and must NOT be translated. Pass if they remain identical across all locales; fail if they are altered.
Tool: agent-browser
Evidence: Screenshots of credits artist/license text in all 3 locales

### VAL-MENU-031: Credits panel "Back" button is localized
The "← 返回" close button must translate. Pass if text changes per locale; fail if hardcoded.
Tool: agent-browser
Evidence: Screenshots of credits close button in all 3 locales

## MenuScene — Difficulty Selector

### VAL-MENU-032: Difficulty selector title is localized
Open difficulty selector (requires a save with completed difficulty). The title "选 择 难 度" must translate. Pass if title changes per locale; fail if hardcoded.
Tool: agent-browser
Evidence: Screenshots of difficulty title in all 3 locales

### VAL-MENU-033: Difficulty level labels are localized
Difficulty labels from `DIFFICULTY_LABELS` (e.g. "普通", "噩梦", "地狱") must translate per locale. Pass if labels change; fail if hardcoded in zh-CN.
Tool: agent-browser
Evidence: Screenshots of difficulty labels in all 3 locales

### VAL-MENU-034: Difficulty descriptions are localized
Description text like "标准难度", "怪物伤害×1.5, 经验×2" must translate. Pass if descriptions change per locale; fail if hardcoded.
Tool: agent-browser
Evidence: Screenshots of difficulty descriptions in all 3 locales

### VAL-MENU-035: Difficulty "current" badge is localized
The "当前" badge next to the active difficulty must translate (e.g. en: "Current"). Pass if badge changes per locale; fail if hardcoded.
Tool: agent-browser
Evidence: Screenshots of current difficulty badge in all 3 locales

### VAL-MENU-036: Locked difficulty text is localized
The locked difficulty message "未解锁 — 需要通关上一难度" must translate. Pass if text changes per locale; fail if hardcoded.
Tool: agent-browser
Evidence: Screenshots of locked difficulty text in all 3 locales

### VAL-MENU-037: Difficulty "Back" button is localized
The "返回" button must translate. Pass if text changes per locale; fail if hardcoded.
Tool: agent-browser
Evidence: Screenshots of difficulty back button in all 3 locales

## BootScene

### VAL-BOOT-001: Loading text changes with language
Set locale to `en` in localStorage before loading the game. The BootScene loading text (currently "锻造渊火...") must display in English (e.g. "Forging Abyssfire..." or equivalent). Pass if loading text is in English; fail if it shows zh-CN regardless of saved locale.
Tool: agent-browser
Evidence: Screenshot of boot screen with English locale, showing English loading text

### VAL-BOOT-002: Loading text shows zh-CN by default
Clear localStorage. Load the game. The BootScene loading text must show in zh-CN: "锻造渊火..." (or the defined zh-CN translation). Pass if zh-CN text is displayed; fail if English or empty.
Tool: agent-browser
Evidence: Screenshot of boot screen on first visit showing zh-CN text

### VAL-BOOT-003: Ready text changes with language
Set locale to `en`. Load the game and wait for assets to finish loading. The "准备就绪!" text must display in English (e.g. "Ready!"). Pass if ready text is in English; fail if it shows zh-CN.
Tool: agent-browser
Evidence: Screenshot of boot screen after load complete with English locale

### VAL-BOOT-004: BootScene title text is localized
The BootScene title "渊 火" and subtitle "A B Y S S F I R E" must adapt to locale. In zh-TW the title should use Traditional Chinese "淵 火". Pass if title text reflects saved locale; fail if always zh-CN.
Tool: agent-browser
Evidence: Screenshots of boot screen title in all 3 locales

### VAL-BOOT-005: BootScene reads locale from localStorage at boot time
The BootScene must read `abyssfire_locale` from localStorage during preload/create (before MenuScene), not defer to MenuScene. Pass if boot text reflects the persisted locale immediately; fail if boot always shows zh-CN and only MenuScene respects the locale.
Tool: agent-browser
Evidence: Set locale to `en` in localStorage, reload, capture boot screen screenshot showing English text

## Edge Cases

### VAL-FOUND-019: Rapid locale switching does not corrupt UI state
Switch locale rapidly 10+ times between zh-CN, zh-TW, and en. After settling on one locale, all visible text must be consistent in that locale — no mixed-language text. Pass if final state is consistent; fail if any text is from a different locale.
Tool: agent-browser
Evidence: Screenshot after rapid switching showing consistent text in final locale

### VAL-FOUND-020: Locale switch during class selection preserves selection screen
Navigate to class selection screen. Switch locale. The class selection screen must remain visible (not revert to main menu) and all text must update to the new locale. Pass if class selection persists with updated text; fail if screen resets or text is mixed.
Tool: agent-browser
Evidence: Screenshot of class selection after mid-screen locale switch

### VAL-FOUND-021: All locale files have matching key sets (zh-CN ⊇ en keys)
Inspect the locale data files. Every key present in the `en` locale must also exist in `zh-CN`. zh-TW may be auto-generated. Pass if zh-CN has all keys that en has (zh-CN is the primary language); fail if en has keys missing from zh-CN.
Tool: agent-browser
Evidence: Script or console eval comparing key sets across locale files

### VAL-FOUND-022: t() is used consistently — no hardcoded Chinese strings remain in MenuScene
Inspect `MenuScene.ts` source code. All player-facing text strings that were previously hardcoded in Chinese must now go through `t()`. Pass if no raw Chinese string literals remain for player-facing text; fail if any hardcoded Chinese strings exist that should be localized.
Tool: agent-browser
Evidence: Code inspection of MenuScene.ts, grep for Chinese character literals outside of comments

### VAL-FOUND-023: t() is used consistently — no hardcoded Chinese strings remain in BootScene
Inspect `BootScene.ts` source code. All player-facing text strings must go through `t()`. Pass if no raw Chinese string literals remain for player-facing text; fail if any hardcoded Chinese strings exist.
Tool: agent-browser
Evidence: Code inspection of BootScene.ts, grep for Chinese character literals outside of comments

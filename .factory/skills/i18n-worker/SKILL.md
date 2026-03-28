---
name: i18n-worker
description: Implements i18n features — locale modules, string extraction, scene wiring, and translation
---

# i18n Worker

NOTE: Startup and cleanup are handled by `worker-base`. This skill defines the WORK PROCEDURE.

## When to Use This Skill

Features that involve:
- Building or extending the i18n core module (t(), setLocale(), converter)
- Extracting hardcoded strings from scenes/systems into locale files
- Wiring scenes to use t() for all player-facing text
- Adding language selector UI
- Creating or updating locale data (zh-CN, en, zh-TW)

## Required Skills

- `agent-browser` — Used for manual verification of translated UI. Invoke when verifying that language switching works visually in the running game.

## Work Procedure

1. **Read the feature description carefully.** Understand which files to modify, which locale keys to create, and which assertions this feature fulfills.

2. **Read the architecture doc** at `.factory/library/architecture.md` for the i18n module structure and conventions.

3. **Write tests first (TDD).** Create or extend test files under `src/__tests__/`. Tests should verify:
   - t() returns correct values for each locale
   - Parameter interpolation works
   - Fallback to en works for missing keys
   - Locale switching updates output
   Run tests with `npx vitest run` — they should FAIL (red).

4. **Implement the feature.**
   - For **i18n core**: Create/update files in `src/i18n/`. Export `t`, `setLocale`, `getLocale`, `getLocales` from `src/i18n/index.ts`.
   - For **locale data**: Add keys to `src/i18n/locales/zh-CN.ts` and `src/i18n/locales/en.ts`. zh-TW is auto-generated.
   - For **string extraction**: Replace hardcoded strings in scene/system files with `t('namespace.key')` or `t('namespace.key', { param: value })`.
   - For **reactive updates**: When locale changes, text objects need to update. Use EventBus to listen for locale change events. In Phaser, call `.setText(t('key'))` on the text object.
   - For **language selector**: Add UI elements in the scene that call `setLocale()` and trigger re-rendering.

5. **Run tests (green).** `npx vitest run` — all tests must pass.

6. **Run validators.** `npx tsc --noEmit` and `npm run build` must succeed.

7. **Verify no hardcoded Chinese remains** in the files you modified. Run: `grep -n '[\u4e00-\u9fff]' <file>` and confirm only comments or non-UI strings remain.

8. **Manual verification with agent-browser.** Start the dev server (`npm run dev`), navigate to `http://localhost:5173/abyssfire/`, and verify:
   - Switch language via the selector (if available)
   - Confirm text changes in the relevant scene/panel
   - Take screenshots as evidence
   - Check that no Chinese leaks through in English mode
   - Check that zh-TW shows Traditional characters where they differ

9. **Commit** with a descriptive message.

## Key Conventions

- **Namespace keys**: Use dot-separated paths matching the scene/system: `menu.newGame`, `ui.inventory.title`, `zone.enterZone`, `data.monster.goblin`, `sys.inventory.bagFull`
- **Parameter placeholders**: Use `{paramName}` syntax: `t('menu.continue', { class: className, level: lvl })`
- **Fallback**: en is the final fallback. If a key exists in zh-CN but not en, the zh-CN value is returned. If missing from both, return the key path string.
- **zh-TW**: Auto-generated. Never hand-edit `zh-TW.ts` — always regenerate from zh-CN via the converter.
- **Locale persistence**: `localStorage.getItem('abyssfire_locale')` / `localStorage.setItem('abyssfire_locale', locale)`
- **Font**: Ensure `index.html` includes both Noto Sans SC and Noto Sans TC Google Fonts for proper CJK rendering.
- **EventBus integration**: Import `EventBus` and `GameEvents` from `src/utils/EventBus.ts`. Add a `LOCALE_CHANGED` event if not present.

## Example Handoff

```json
{
  "salientSummary": "Built the i18n core module with t(), setLocale(), getLocale(), parameter interpolation, en fallback, and zh-TW auto-converter. Created zh-CN and en locale files with MenuScene keys (~45 keys). Added LOCALE_CHANGED event to EventBus. All 12 unit tests pass, typecheck clean, build succeeds.",
  "whatWasImplemented": "Created src/i18n/index.ts (t function with locale switching, parameter interpolation, en fallback chain), src/i18n/locales/zh-CN.ts (45 MenuScene keys), src/i18n/locales/en.ts (45 English translations), src/i18n/converter.ts (zh-CN→zh-TW character mapping with 800+ mappings), src/i18n/locales/zh-TW.ts (auto-generated). Added LOCALE_CHANGED to GameEvents enum.",
  "whatWasLeftUndone": "",
  "verification": {
    "commandsRun": [
      { "command": "npx vitest run", "exitCode": 0, "observation": "All 2965 tests pass including 12 new i18n tests" },
      { "command": "npx tsc --noEmit", "exitCode": 0, "observation": "Clean typecheck" },
      { "command": "npm run build", "exitCode": 0, "observation": "Build succeeds, 2.2MB bundle" }
    ],
    "interactiveChecks": [
      { "action": "Loaded game at localhost:5173/abyssfire/, opened browser console, called t('menu.newGame') with locale zh-CN", "observed": "Returns '新的旅程'" },
      { "action": "Called setLocale('en'), then t('menu.newGame')", "observed": "Returns 'New Journey'" },
      { "action": "Called setLocale('zh-TW'), then t('menu.newGame')", "observed": "Returns '新的旅程' with Traditional characters where applicable" },
      { "action": "Called t('nonexistent.key')", "observed": "Returns 'nonexistent.key' (key path fallback)" }
    ]
  },
  "tests": {
    "added": [
      { "file": "src/__tests__/i18n.test.ts", "cases": [
        { "name": "t() returns zh-CN string", "verifies": "Correct zh-CN value for known key" },
        { "name": "t() returns en string", "verifies": "Correct en value after setLocale('en')" },
        { "name": "t() parameter interpolation", "verifies": "Placeholders replaced with values" },
        { "name": "t() falls back to en for missing zh-CN key", "verifies": "Missing zh-CN key returns en value" },
        { "name": "t() returns key path for completely missing key", "verifies": "Unknown key returns key string" },
        { "name": "zh-TW conversion produces Traditional characters", "verifies": "Converted string differs from zh-CN" }
      ]}
    ]
  },
  "discoveredIssues": []
}
```

## When to Return to Orchestrator

- The i18n module structure needs to change fundamentally (different key format, different fallback chain)
- A scene has text rendering that can't be updated reactively (would need scene restart)
- Font loading issues prevent CJK characters from rendering correctly
- Existing test failures unrelated to this feature block the test suite

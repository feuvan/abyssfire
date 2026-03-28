# User Testing

## Validation Surface

- **Primary surface**: Browser (Phaser 3 canvas game)
- **URL**: `http://localhost:5173/abyssfire/`
- **Tool**: `agent-browser`
- **Dev server**: Vite on port 5173 (`npm run dev`)

### Testing Approach

1. Start dev server via `.factory/services.yaml`
2. Navigate to the game URL with agent-browser
3. Interact with menu (language selector, panels) via canvas clicks
4. Verify text content changes when switching languages
5. Take screenshots as evidence

### Key Testing Paths

- **MenuScene**: Click language selector → verify all button text changes → open each panel (help, jukebox, credits, class selection, difficulty) → verify panel text changes
- **Gameplay**: Start new game → verify ZoneScene + UIScene text → open panels (inventory, skills, character, quests, homestead, etc.) → verify all text
- **Persistence**: Set language → reload page → verify boot screen and menu use saved language
- **zh-TW**: Select zh-TW → verify Traditional Chinese characters appear correctly

### Limitations

- Phaser renders to canvas, so text extraction requires screenshots + visual inspection (no DOM text queries)
- Some assertions require in-game progression (combat, quests, items) which may need seed data or manual play-through
- Mobile controls require viewport emulation to test

## Validation Concurrency

- **Machine**: 16 GB RAM, 8 CPU cores
- **Dev server**: ~200 MB RAM
- **Per agent-browser instance**: ~300 MB RAM
- **Max concurrent validators**: 5 (5 × 300 MB + 200 MB = 1.7 GB, well within 70% headroom of ~7 GB)

## Flow Validator Guidance: agent-browser

- Use exactly one isolated browser session per subagent and reuse it for all assigned assertions.
- Do not restart or stop the shared Vite server on port `5173`; treat it as shared infrastructure.
- App URL: `http://localhost:5173/abyssfire/`.
- Keep state isolated inside your own browser session. Do not assume another validator's `localStorage` or IndexedDB data exists.
- For locale assertions, it is safe to manipulate `localStorage['abyssfire_locale']` inside your isolated session and reload.
- For save-dependent menu assertions, seed your own IndexedDB autosave inside the browser session. The app exposes Vite source modules at paths like:
  - `await import('/abyssfire/src/i18n/index.ts')`
  - `await import('/abyssfire/src/systems/SaveSystem.ts')`
- Save data is stored in IndexedDB database `AbyssfireDB`, object store `saves`; `SaveSystem.autoSave()` writes the `autosave` record used by MenuScene.
- Capture screenshots for every UI assertion, and record console errors after each major flow.
- If a flow mutates locale/save state significantly, reset your own session state before moving to unrelated assertions.

## Flow Validator Guidance: command-inspection

- Use read-only repo inspection plus local validator commands; do not edit source files or shared app state.
- Suitable for assertions that explicitly require code inspection, locale key parity checks, dependency checks, or grep-based audits.
- Prefer `Read`, `Grep`, `Glob`, and manifest commands over ad hoc shell exploration.
- When checking locale parity, compare `src/i18n/locales/zh-CN.ts` and `src/i18n/locales/en.ts` key sets directly.
- When checking for hardcoded Chinese strings in `MenuScene.ts` or `BootScene.ts`, treat comments as non-player-facing and report only executable/player-facing literals.

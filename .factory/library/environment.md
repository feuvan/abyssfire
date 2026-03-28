# Environment

Environment variables, external dependencies, and setup notes.

**What belongs here:** Required env vars, external API keys/services, dependency quirks, platform-specific notes.
**What does NOT belong here:** Service ports/commands (use `.factory/services.yaml`).

---

## Dependencies

- Node.js (tested with system node)
- npm (package manager)
- No external API keys or services required (pure client-side game)

## Fonts

- Google Fonts: `Noto Sans SC` (Simplified Chinese) and `Noto Sans TC` (Traditional Chinese) must be loaded in `index.html`
- `Cinzel` serif font for title/decorative elements

## Build

- Vite bundler
- TypeScript strict mode
- Output to `dist/`
- Base path: `/abyssfire/` (configured in vite.config.ts)

## Browser Compatibility

- Modern browsers with ES2020+ support
- Canvas rendering (Phaser 3)
- Web Audio API for sound
- IndexedDB (Dexie.js) for save data
- localStorage for language preference

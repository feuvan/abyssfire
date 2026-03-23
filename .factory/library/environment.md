# Environment

Environment variables, external dependencies, and setup notes.

**What belongs here:** Required env vars, external API keys/services, dependency quirks, platform-specific notes.
**What does NOT belong here:** Service ports/commands (use `.factory/services.yaml`).

---

## Dependencies

- **Phaser 3 v3.90+**: Game engine, provides rendering, physics, input, audio
- **TypeScript ~5.9**: Strict mode, compiled via Vite
- **Vite 8**: Build tool and dev server
- **Dexie.js v4.3**: IndexedDB wrapper for save/load
- **Tauri v2.10**: Optional desktop wrapper (do not modify)

## Node Version

- Node.js 22 (see `.github/workflows/deploy.yml`)
- npm for package management

## Build Pipeline

- `npm run build` = `tsc && vite build`
- TypeScript strict mode compilation
- Single chunk output to `dist/`
- Deployed to GitHub Pages via push to `main`

## Audio Assets

- 17 MP3 files in `public/assets/audio/bgm/`
- These are the ONLY external assets — everything else is procedurally generated
- Do not modify or delete these files

## Browser Requirements

- WebGL required for `ColorGradePipeline` post-processing
- Web Audio API for procedural SFX
- AudioContext auto-resume handled on first user gesture

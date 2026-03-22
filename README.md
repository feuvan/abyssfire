# Abyssfire (渊火)

Isometric ARPG web game with a DnD-inspired world, Diablo II-style loot system, and real-time auto-combat. Runs entirely in the browser — no server required.

**[Play Now](https://feuvan.github.io/abyssfire/)**

## Features

- **3 Playable Classes** — Warrior, Mage, Rogue, each with unique skill trees
- **5 Zones** — Progress from Emerald Plains to the Abyss Rift (Lv.1–50)
- **Diablo II Loot** — Normal / Magic / Rare / Legendary / Set items with random affixes
- **Real-time Combat** — Manual control or auto-battle with skill priority configuration
- **Fog of War** — Explore and uncover the map as you go
- **Homestead** — Build structures, collect pets
- **Quests & NPCs** — Shops, dialogue, quest chains
- **Procedural Sprites** — All entity art generated via Canvas 2D, with external PNG override support
- **Persistent Saves** — IndexedDB via Dexie.js, multiple save slots
- **Audio** — Synthesized BGM and SFX via Web Audio API

## Tech Stack

- **Engine**: Phaser 3, TypeScript, Vite
- **Storage**: IndexedDB (Dexie.js)
- **Art**: Procedural Canvas 2D sprite generation
- **Resolution**: 1280x720, isometric tiles 64x32

## Getting Started

```bash
npm install
npm run dev      # dev server at localhost:5173
npm run build    # production build to dist/
```

## Controls

| Key | Action |
|-----|--------|
| `WASD` / Arrow Keys | Move |
| Left Click | Move / attack / interact |
| Right Click / `R` | Return to camp |
| `1-6` | Use skills |
| `TAB` | Toggle auto-combat |
| `I` | Inventory |
| `C` | Character stats |
| `K` | Skill tree |
| `J` | Quest log |
| `M` | Map |
| `H` | Homestead |
| `O` | Audio settings |
| `ESC` | Return to main menu |
| `Ctrl` + `Shift` + `E` | Export generated textures (dev builds only) |

## Credits

- **Tile Art** — [Isometric Landscape](https://kenney.nl/) by Kenney (CC0) · [License](public/assets/tiles/LICENSE-kenney-CC0.txt)
- **BGM** — Various artists from [OpenGameArt.org](https://opengameart.org/) (CC0 / GPL) · [Full credits](public/assets/audio/bgm/CREDITS.md)

## License

All rights reserved.

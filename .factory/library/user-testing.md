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

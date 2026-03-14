# Save/Load System Refactor

## Context

The game autosaves player state to IndexedDB via Dexie.js but **never restores from it**. MenuScene always starts a fresh character at emerald_plains. All SaveSystem load methods (`load`, `loadAutoSave`, `listSaves`) are dead code. Systems have load methods (`QuestSystem.loadProgress`, `AchievementSystem.loadData`) that are never called. When the browser is closed, all progress is lost despite saves existing in IndexedDB.

This refactor wires up the full save→load round-trip so players can resume, and adds a "New Game" option for fresh characters.

## Files to Modify

| File | Changes |
|------|---------|
| `src/scenes/MenuScene.ts` | Two-phase menu: "Continue" (load save) + "New Game" (class select) |
| `src/scenes/ZoneScene.ts` | Accept optional `SaveData` in init, add `restoreFromSave()` method |

No changes needed to: `Player.ts`, `SaveSystem.ts`, `QuestSystem.ts`, `AchievementSystem.ts`, `InventorySystem.ts`, `HomesteadSystem.ts`, `FogOfWarSystem.ts`, `types.ts`

## Step 1: ZoneScene — Accept and Restore Save Data

**File**: `src/scenes/ZoneScene.ts`

### 1a. Add property
```typescript
private _pendingSaveData: SaveData | null = null;
```
Import `SaveData` from `../data/types` (add to existing import).

### 1b. Expand init() data type
```typescript
init(data: { classId: string; mapId: string; saveData?: SaveData }): void {
  this.currentMapId = data.mapId || 'emerald_plains';
  if (!AllMaps[this.currentMapId]) this.currentMapId = 'emerald_plains';  // guard
  this.mapData = AllMaps[this.currentMapId];
  this.campPositions = this.mapData.camps.map(c => ({ col: c.col, row: c.row }));
  this._pendingSaveData = data.saveData ?? null;
}
```

### 1c. Modify create() — add restore after player creation
After the existing system init block (`if (!this.inventorySystem) {...}`) and player creation block, insert a restore call:

```typescript
const isFirstLoad = !this.inventorySystem;
// ... existing system init (lines 79-86) ...
// ... existing player creation (lines 97-112) ...

// ── Restore from save (first load only, not zone transitions) ──
if (isFirstLoad && this._pendingSaveData) {
  this.restoreFromSave(this._pendingSaveData);
  this._pendingSaveData = null;
}

// ... existing pathfinding + fog (lines 114-120) ...
```

The `isFirstLoad` guard ensures zone transitions (which re-call `create()` but already have systems in memory) don't re-apply save data.

### 1d. Add restoreFromSave() method

```typescript
private restoreFromSave(save: SaveData): void {
  // 1. Player stats
  this.player.level = save.player.level;
  this.player.exp = save.player.exp;
  this.player.gold = save.player.gold;
  this.player.stats = { ...save.player.stats };
  this.player.freeStatPoints = save.player.freeStatPoints;
  this.player.freeSkillPoints = save.player.freeSkillPoints;
  this.player.skillLevels = new Map(Object.entries(save.player.skillLevels));
  this.player.recalcDerived();
  this.player.hp = Math.min(save.player.hp, this.player.maxHp);
  this.player.mana = Math.min(save.player.mana, this.player.maxMana);
  this.player.moveTo(save.player.tileCol, save.player.tileRow);

  // 2. Inventory (direct public property assignment)
  this.inventorySystem.inventory = save.inventory ?? [];
  this.inventorySystem.equipment = (save.equipment ?? {}) as any;
  this.inventorySystem.stash = save.stash ?? [];

  // 3. Quests (existing loadProgress method)
  if (save.quests) this.questSystem.loadProgress(save.quests);

  // 4. Homestead (direct assignment, no load method)
  if (save.homestead) {
    this.homesteadSystem.buildings = save.homestead.buildings ?? {};
    this.homesteadSystem.pets = save.homestead.pets ?? [];
    this.homesteadSystem.activePet = save.homestead.activePet ?? null;
  }

  // 5. Achievements (existing loadData method)
  if (save.achievements) this.achievementSystem.loadData(save.achievements);

  // 6. Exploration fog data (restored into fogData, picked up by fog init)
  if (save.exploration) this.fogData = save.exploration;

  // 7. Settings
  this.player.autoCombat = save.settings?.autoCombat ?? false;
  this.autoLootMode = save.settings?.autoLootMode ?? 'off';
  this.difficulty = save.difficulty ?? 'normal';
}
```

**Key ordering**: `recalcDerived()` before hp/mana (so maxHp/maxMana are correct for clamping). `fogData` before fog system init (so it picks up restored data at lines 117-119).

## Step 2: MenuScene — Two-Phase Menu with Save Detection

**File**: `src/scenes/MenuScene.ts`

### 2a. Add imports
```typescript
import { SaveSystem } from '../systems/SaveSystem';
import { AllClasses } from '../data/classes/index';
import type { SaveData } from '../data/types';
```

### 2b. Add properties
```typescript
private menuContainer: Phaser.GameObjects.Container | null = null;
private classContainer: Phaser.GameObjects.Container | null = null;
```

### 2c. Restructure create()
Keep existing background/particles/title rendering (lines 13-66). Replace class selection code (lines 68-114) with:
```typescript
this.checkForSaves();
```

### 2d. Add checkForSaves()
```typescript
private async checkForSaves(): Promise<void> {
  const saveSystem = new SaveSystem();
  const save = await saveSystem.loadAutoSave();
  this.showMainMenu(save ?? null);
}
```

### 2e. Add showMainMenu(save)
```typescript
private showMainMenu(save: SaveData | null): void {
  if (this.menuContainer) { this.menuContainer.destroy(); }
  this.menuContainer = this.add.container(0, 0);

  const cx = GAME_WIDTH / 2;
  let y = save ? 300 : 340;

  if (save) {
    // "Continue" button — shows class name + level
    const classData = AllClasses[save.classId];
    const className = classData?.name ?? save.classId;
    const label = `继续游戏 - ${className} Lv.${save.player.level}`;

    const bg = this.add.rectangle(cx, y, 320, 65, 0x12121e, 0.9)
      .setStrokeStyle(1.5, 0xc0934a, 0.8).setInteractive({ useHandCursor: true });
    bg.on('pointerover', () => { bg.setStrokeStyle(2, 0xc0934a, 1); bg.setFillStyle(0x1a1a2e, 0.95); });
    bg.on('pointerout', () => { bg.setStrokeStyle(1.5, 0xc0934a, 0.8); bg.setFillStyle(0x12121e, 0.9); });
    bg.on('pointerdown', () => this.loadGame(save));
    this.menuContainer.add(bg);

    this.menuContainer.add(this.add.text(cx, y - 6, label, {
      fontSize: '16px', color: '#e8e0d4', fontFamily: '"Cinzel", "Noto Sans SC", serif',
    }).setOrigin(0.5));
    this.menuContainer.add(this.add.text(cx, y + 16, '继续你的冒险', {
      fontSize: '11px', color: '#c0934a', fontFamily: '"Noto Sans SC", sans-serif',
    }).setOrigin(0.5));

    y += 90;
  }

  // "New Game" button
  const newBg = this.add.rectangle(cx, y, 320, 55, 0x12121e, 0.9)
    .setStrokeStyle(1.5, 0x555566, 0.6).setInteractive({ useHandCursor: true });
  newBg.on('pointerover', () => { newBg.setStrokeStyle(2, 0x888899, 1); newBg.setFillStyle(0x1a1a2e, 0.95); });
  newBg.on('pointerout', () => { newBg.setStrokeStyle(1.5, 0x555566, 0.6); newBg.setFillStyle(0x12121e, 0.9); });
  newBg.on('pointerdown', () => {
    this.menuContainer?.destroy(); this.menuContainer = null;
    this.showClassSelection();
  });
  this.menuContainer.add(newBg);
  this.menuContainer.add(this.add.text(cx, y, '新的旅程', {
    fontSize: '16px', color: '#a0907a', fontFamily: '"Cinzel", "Noto Sans SC", serif',
  }).setOrigin(0.5));
}
```

### 2f. Add showClassSelection()
Move existing class selection code (lines 68-114) into this method. Add a "Back" button:
```typescript
private showClassSelection(): void {
  this.classContainer = this.add.container(0, 0);
  const cx = GAME_WIDTH / 2;

  this.classContainer.add(this.add.text(cx, 260, '选 择 职 业', { ... }).setOrigin(0.5));

  // ... existing class buttons rendering (moved from create()) ...

  // Back button
  const backBtn = this.add.text(cx, 570, '← 返回', {
    fontSize: '12px', color: '#888', fontFamily: '"Noto Sans SC", sans-serif',
  }).setOrigin(0.5).setInteractive({ useHandCursor: true });
  backBtn.on('pointerdown', () => {
    this.classContainer?.destroy(); this.classContainer = null;
    this.checkForSaves();
  });
  this.classContainer.add(backBtn);
}
```

### 2g. Add loadGame()
```typescript
private loadGame(save: SaveData): void {
  this.scene.start('ZoneScene', {
    classId: save.classId,
    mapId: save.player.currentMap,
    saveData: save,
  });
}
```

### 2h. startGame() unchanged
```typescript
private startGame(classId: string): void {
  this.scene.start('ZoneScene', { classId, mapId: 'emerald_plains' });
}
```

## Edge Cases & Graceful Degradation

| Scenario | Handling |
|----------|----------|
| No saves exist | `checkForSaves()` returns null -> only "New Game" shown |
| Old save missing `autoLootMode` | `?? 'off'` fallback |
| Old save missing `homestead` | `?? {}` / `?? []` / `?? null` |
| Save references deleted map | Guard in `init()`: fallback to `emerald_plains` |
| Save hp > new maxHp (formula change) | `Math.min(saved.hp, recalculated.maxHp)` |
| Zone transition after load | `_pendingSaveData` is null, `isFirstLoad` is false -> no re-apply |
| `skillLevels` format (Map<->Record) | `new Map(Object.entries(...))` conversion |

## Verification

1. **No saves -- fresh start**: Clear IndexedDB -> open game -> only "New Game" button shown -> select class -> starts at emerald_plains Lv.1
2. **Save -> resume**: Play until Lv.3+ with gear -> close browser -> reopen -> "Continue" button shows class + level -> click -> correct map, level, stats, inventory, gold
3. **New game from existing save**: Click "New Game" instead of "Continue" -> select class -> starts fresh Lv.1 at emerald_plains (old save untouched)
4. **Zone transition after load**: Load save -> walk to zone exit -> transition works, state preserved in memory
5. **Settings persistence**: Enable auto-loot to "magic+", autoCombat on -> save -> reload -> both settings restored
6. **Quest progress**: Accept quests, kill some mobs -> reload -> quest progress intact
7. **TypeScript check**: `npx tsc --noEmit` passes
8. **Build**: `npm run build` succeeds

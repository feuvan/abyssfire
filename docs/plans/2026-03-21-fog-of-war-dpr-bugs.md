# Fog of War DPR Bugs — 2026-03-21

## Background

Commit `6bd6d17` (Mar 20) introduced DPR-aware canvas scaling:
- Game canvas: `GAME_WIDTH * DPR` x `GAME_HEIGHT * DPR` (1280→2560, 720→1440 at DPR=2)
- `TEXTURE_SCALE`: 2 → 3
- Camera zoom remains 1.8

Several systems were not updated accordingly, causing 3 visible fog-of-war bugs.

---

## Bug 1: 左上 1/4 矩形阴影

**Symptom**: A dark rectangle covering the top-left quarter of the screen.

**Root cause**: `LightingSystem` creates an off-screen canvas at `GAME_WIDTH/2 × GAME_HEIGHT/2` (640×360) with `setScale(2)`, producing a 1280×720 overlay. After DPR scaling the game canvas is 2560×1440, so the overlay only covers 1/4 of the viewport.

**File**: `src/systems/LightingSystem.ts` lines 58-59, 76

```typescript
// Before (broken)
this.renderW = Math.ceil(GAME_WIDTH / 2);   // 640
this.renderH = Math.ceil(GAME_HEIGHT / 2);  // 360
this.overlay.setScale(2);                    // → 1280×720, but canvas is 2560×1440
```

**Fix**: Scale renderW/renderH and overlay.setScale by DPR.

---

## Bug 2: 未探索阴影深浅不一（explored vs unexplored alpha mismatch）

**Symptom**: Unexplored fog on the right/bottom side appears lighter than the top-left.

**Root cause**: Compound effect of Bug 1. In the top-left quarter, fog-of-war (alpha 0.85) is multiplied by the lighting overlay → very dark. In the remaining 3/4, no lighting overlay exists → fog renders at raw alpha 0.85, which appears lighter because it's not multiplied by ambient darkness. Fixing Bug 1 resolves this.

Additionally, `LightingSystem.update()` converts world→screen coordinates using `cam.zoom / 2` which assumed the overlay was always at half the game resolution. With DPR this ratio changes.

---

## Bug 3: 区域预先标记为已探索

**Symptom**: Areas the player never visited appear as "explored" (lighter fog instead of opaque).

**Root cause**: Not introduced by Mar 20 changes. The save system persists `fogData` via `autoSave()`. When loading a save, `restoreFromSave()` restores old exploration data. On new-game or zone-enter, `autoSave()` is called at the end of `create()`, so subsequent loads restore that snapshot. This is pre-existing behavior.

---

## Fix Plan

1. **LightingSystem**: Multiply renderW/renderH and overlay scale by DPR
2. **LightingSystem**: Fix world→screen coordinate conversion to account for DPR
3. Verify fog-of-war rendering consistency after lighting fix

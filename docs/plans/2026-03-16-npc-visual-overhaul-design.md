# NPC Visual Overhaul Design

## Goal

Full visual overhaul of all 11 NPCs: richer proportional sprites, state-driven animations, and subtle ambient VFX. Make NPCs feel like living characters in the camp, not colored blocks.

## Decisions

- **Art style**: Proportional / Western RPG (Diablo II feel)
- **Sprite resolution**: 80×120 pixels (up from 48×80), scaled by TEXTURE_SCALE
- **Animation states**: 4-state machine (working, alert, idle, talking)
- **Ambient VFX**: Subtle per-role particle hints
- **All procedurally generated** on canvas (no external assets)

## Sprite Sheet Layout

Each NPC gets a single sprite sheet with 24 frames at 80×120:

| Frames | State   | Description                                |
|--------|---------|--------------------------------------------|
| 0–7    | Working | Role-specific busy animation (loop)        |
| 8–11   | Alert   | Head turn, posture shift toward player     |
| 12–17  | Idle    | Gentle breathing, weight shift (loop)      |
| 18–23  | Talking | Hand gestures, mouth movement (loop)       |

Total sheet dimensions per NPC: 1920×120 (before TEXTURE_SCALE).

## Drawing Improvements

At 80×120 resolution, each frame includes:

- **Body**: Proper torso/hip proportions with gradient shading (light from top-left). Shoulder line, waist taper.
- **Head**: Rounded rect with jaw definition. Eyes with whites, iris, and highlight dot. Eyebrows for expression. Nose, mouth line.
- **Clothing**: Visible collar/neckline, sleeve cuffs, tunic hem. Subtle wrinkle lines. Belt buckle detail.
- **Accessories**: Detailed — hammer head with metallic gradient, staff with glowing orb, coinbag with drawstring, etc.
- **Boots**: Distinct sole, ankle detail.
- **Shading**: Dark edge outlines (1px at scale), subtle gradients on body/limbs for volume.

### Per-State Drawing Differences

- **Working**: Arms in active positions per role (hammer raised/striking, counting coins, reading book, holding staff upright).
- **Alert**: Head rotated slightly toward camera, body straightens, hand pauses mid-action.
- **Idle**: Relaxed stance, arms at sides or loosely holding accessory.
- **Talking**: One hand gestures outward, mouth open in some frames, slight lean forward.

## NPC State Machine

### States

```
                    player leaves range
         ┌──────────────────────────────────┐
         ▼                                  │
    ┌─────────┐   player enters range   ┌───────┐
    │ Working │ ──────────────────────► │ Alert │
    └─────────┘                         └───────┘
         ▲                                  │
         │ dialogue closed                  │ dialogue opened
         │ + player leaves                  ▼
         │                             ┌─────────┐
         └──────────────────────────── │ Talking │
                                       └─────────┘
```

- **Working** (default): Role-specific activity. Frames 0–7, looping.
- **Alert**: Player enters interaction range (3 tiles, matching existing `isNearPlayer` range). Frames 8–11 play once, then hold frame 11. Returns to Working when player leaves range (with ~500ms debounce via `this.scene.time.delayedCall()`).
- **Talking**: Triggered by `NPC_INTERACT` or `SHOP_OPEN` event (matched by `npcId`). Frames 18–23, looping.
- **Idle**: Fallback state. Frames 12–17, looping. Entered only if no Working animation frames are defined for this NPC (which currently never happens — all 11 NPCs have accessories). Defensive only.

### Transition: Talking → Alert → Working

When dialogue/shop closes, the NPC transitions to **Alert** (not directly to Working), since the player is likely still in range. From Alert, the normal range-check logic handles the transition back to Working once the player walks away.

### EventBus Changes Required

| Change | Details |
|--------|---------|
| Add `DIALOGUE_CLOSE` event | New event in `GameEvents` enum. `UIScene.closeDialogue()` emits it. |
| Add `npcId` to `NPC_INTERACT` payload | `ZoneScene` already has `def.id` in scope at the emit site (line 1046). Add `npcId: def.id` to the payload object. |
| `SHOP_OPEN` already has `npcId` | No change needed — already emits `{ npcId: def.id, ... }`. |
| `SHOP_CLOSE` already exists | NPC listens for this to leave Talking state. |
| Stash NPC: `UI_TOGGLE_PANEL` | The stash NPC currently emits `UI_TOGGLE_PANEL` with `{ panel: 'stash' }`, not `NPC_INTERACT`. Add `npcId` to that payload. NPC listens for `UI_TOGGLE_PANEL` (with `panel: 'stash'` and matching `npcId`) as a Talking trigger. Since it's a toggle, the same event fires again on close — NPC tracks open/closed state internally via a flag. Alternatively, the NPC exits Talking when the player moves out of range (the Alert→Working path handles cleanup naturally). |

The NPC matches events by comparing the payload's `npcId` against its own `definition.id`, so only the targeted NPC transitions to Talking.

### Implementation Details

- `NPC` class gets a `state` property: `'working' | 'alert' | 'idle' | 'talking'`.
- `update(playerCol, playerRow)` method called from `ZoneScene` update loop — checks player distance, triggers state transitions.
- EventBus listeners: `NPC_INTERACT` and `SHOP_OPEN` → enter Talking (if `npcId` matches). `DIALOGUE_CLOSE` and `SHOP_CLOSE` → exit Talking to Alert.
- Animation transitions use `sprite.play()` with animation keys registered per state per NPC.
- Alert→Working debounce: `this.scene.time.delayedCall(500, ...)`, cancelled if player re-enters range.

## Ambient VFX

Subtle Phaser particle emitters attached to each NPC container, active only during Working state.

| Role       | Effect       | Details                                                                 |
|------------|-------------|-------------------------------------------------------------------------|
| Blacksmith | Forge sparks | 2-3 tiny orange particles drifting up from hammer area. 1 per 400ms.   |
| Merchant   | Coin glints  | Occasional gold sparkle near coinbag. 1 per 800ms. Quick flash.        |
| Quest giver| Mystic wisps | 1-2 faint arcane motes floating slowly around NPC. Color per NPC theme.|
| Stash      | Purple motes | Dim purple particles from book/hands area. 1 per 1000ms.              |

### VFX Implementation

- Phaser `ParticleEmitter` system (already used for campfire/torches).
- Tiny particle textures (2×2 or 3×3 circles) generated in `BootScene`, or reuse from `SkillEffectSystem`.
- Low `frequency`, short `lifespan`, small `scale`, low `alpha`.
- Emitters are children of NPC container (move with NPC).
- Emitters active only in Working state.

## Files Modified

| File | Changes |
|------|---------|
| `src/graphics/SpriteGenerator.ts` | Rewrite `makeNPCSheet()` — new frame size (80×120), 24 frames, 4 state animations, detailed proportional drawing. Update animation registration to create 4 animation keys per NPC (`_working`, `_alert`, `_idle`, `_talking`). No new fields needed on `NPCConfig` — per-state drawing logic is driven by the existing `accessory` field (which already uniquely determines what "working" looks like per role). |
| `src/entities/NPC.ts` | Add state machine (`state` property, `update()` method, state transitions). Add particle emitters per role. Wire EventBus listeners for `NPC_INTERACT`, `SHOP_OPEN`, `DIALOGUE_CLOSE`, `SHOP_CLOSE`. |
| `src/scenes/ZoneScene.ts` | Call `npc.update(playerCol, playerRow)` in the scene update loop for each NPC. Add `npcId: def.id` to the `NPC_INTERACT` event payload. |
| `src/scenes/UIScene.ts` | Emit `DIALOGUE_CLOSE` event in `closeDialogue()` method. |
| `src/utils/EventBus.ts` | Add `DIALOGUE_CLOSE: 'dialogue:close'` to `GameEvents` enum. |

## NPCConfig Type

No new fields are needed. The existing `NPCConfig` already has `accessory`, `bulky`, `hairStyle`, `cloakColor`, etc. Per-state drawing differences (working pose vs alert pose vs talking gestures) are determined by the `accessory` field:

- `hammer` / `pickaxe` → working = hammering/digging, alert = arm paused mid-swing
- `coinbag` → working = weighing coins, alert = looks up from counting
- `staff` → working = leaning on staff, alert = straightens up
- `sword` → working = patrol stance, alert = hand on hilt
- `lantern` → working = swinging lantern, alert = holds lantern up
- `book` → working = reading, alert = looks up from book
- `scroll` / `none` → working = generic idle gestures, alert = faces player (fallback poses; no current NPC uses these)

## Ambient VFX Per-NPC Variation

All NPCs of the same role share the same VFX type, but quest givers vary their wisp color by NPC theme:

| Quest NPC | Wisp Color |
|-----------|-----------|
| quest_elder | Warm gold (0xb8860b) |
| quest_scout | Forest green (0x4a6a3a) |
| forest_hermit | Moss green (0x5a8a4a) |
| quest_dwarf | Amber (0x8a7a5a) |
| quest_nomad | Desert gold (0xc09a30) |
| quest_warden | Dark crimson (0x6a2a3a) |

The wisp color is derived from each NPC's existing `itemColor` field — no new config needed.

## Constraints

- All sprites remain procedurally generated (no external PNG dependencies).
- Particle effects are subtle — low counts, short lifespans, small sizes.
- Performance: TEXTURE_SCALE is 2. Each NPC sheet = 24 × (80×2) × (120×2) = 3840×240 = ~3.7MB raw RGBA. 11 NPCs = ~40MB total canvas texture memory. This is acceptable for modern browsers (GPU texture budgets are typically 256MB+), but if needed, individual NPC sheets can be generated lazily (only when the NPC's zone is loaded).
- Existing NPC interaction (click to open shop/dialogue/stash) remains unchanged.
- NPCDefinition type in `types.ts` does not need changes (state machine is visual only).

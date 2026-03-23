# Abyssfire Codebase Analysis Report

**Date**: 2026-03-24  
**Total Lines of Code**: ~29,612 TypeScript across ~80+ files  
**Codebase State**: Fully playable, procedurally-generated art, 5 zones, 3 classes

---

## 1. PROJECT INFRASTRUCTURE

### Build & Dependencies
- **Framework**: Phaser 3 v3.90+ (latest), TypeScript ~5.9, Vite 8
- **Storage**: Dexie.js v4.3 (IndexedDB wrapper)
- **Desktop**: Tauri v2.10 (optional native wrapper)
- **Scripts**: `npm run dev` (Vite dev), `npm run build` (tsc + vite build)
- **Resolution**: 1280×720 logical, DPR=2 (2560×1440 actual), FIT scaling
- **Tile size**: 64×32 iso (config), procedural textures at TEXTURE_SCALE=3
- **No test framework**: No unit tests, no linter config, no CI test step

### File Size Breakdown (top files)
| File | Lines |
|------|-------|
| UIScene.ts | 2,216 |
| ZoneScene.ts | 2,179 |
| SpriteGenerator.ts | 1,372 |
| SkillEffectSystem.ts | 1,295 |
| SFXEngine.ts | 999 |
| MusicEngine.ts | 965 |
| MenuScene.ts | 844 |
| all_quests.ts | 742 |
| CharacterAnimator.ts | 746 |
| MapGenerator.ts | 494 |

### TODO/FIXME/HACK Comments
**None found.** The codebase is clean of inline markers.

---

## 2. SCENE ARCHITECTURE

### Scene Flow
`BootScene` → `MenuScene` → `ZoneScene` + `UIScene` (parallel overlay)

### BootScene (~100 lines)
- **Implemented**: Loading screen with animated title, ember particles, progress bar
- **Asset loading**: Commented out — no external PNGs loaded. All sprites procedurally generated
- **Procedural generation**: `SpriteGenerator.generateBootTextures()` creates tiles, `SkillEffectSystem.generateTextures()` creates particle/skill icon textures
- **External art override**: Architecture exists (loaderror handler, asset path resolution) but is dormant

### MenuScene (~844 lines)
- **Implemented**: Title screen with fire glow VFX, class selection (3 classes), continue/new game, help screen with keybindings, OST jukebox with foobar2000-style UI
- **Save integration**: Loads autosave, shows "Continue" button with class/level info
- **Audio**: BGM starts on menu, handles AudioContext resume for browser restrictions

### ZoneScene (~2,179 lines)
- **Core game loop**: Movement, combat, tile rendering, monster AI, NPC interaction, loot, quests
- **Viewport culling**: Only renders visible tiles/decorations (throttled at 100ms)
- **Lighting**: LightingSystem with player halo, campfire/torch lights, flicker effects
- **Weather**: WeatherSystem per zone
- **VFX**: VFXManager (camera shake, hit flash, bloom, vignette), TrailRenderer (weapon trails, ground scorch)
- **Post-processing**: Bloom, vignette, color grading (WebGL only)
- **Mobile controls**: MobileControlsSystem (virtual joystick) — detected via `isMobileDevice()`
- **Zone transitions**: Fade-in/out, preserves player stats across zones, auto-save on entry

### UIScene (~2,216 lines)
- **HUD**: HP/MP globes (Diablo-style spheres), EXP bar, skill bar (6 slots + utility buttons), combat log, quest tracker, minimap, gold display
- **Panels**: Inventory (paginated grid + equipment slots), Shop (split merchant/player), Skill tree (3-column card layout with hover tooltips, synergy indicators), Character stats, Homestead, Quest log, World map, Audio settings
- **Item tooltips**: Full affix display with quality color coding
- **All text**: Chinese (Simplified) throughout

---

## 3. ENTITY SYSTEM

### Player.ts (~371 lines)
- **Stats**: 6 attributes (STR/DEX/VIT/INT/SPI/LCK), derived HP/MP/damage/defense
- **Movement**: Smooth acceleration/deceleration, tile-based with fractional positions, foot dust particles
- **Skills**: Per-class skill list, skill levels (Map), cooldowns (Map), mana costs
- **Combat**: Auto-attack target, attack speed/range/damage, buff system (ActiveBuff[])
- **Animation**: CharacterAnimator with idle/walk/attack/cast/hurt/death states
- **Sprite**: Container-based, uses SpriteGenerator for procedural spritesheet, falls back to colored rectangle
- **Key formulas**:
  - `maxHp = 50 + VIT*10 + (level-1)*15`
  - `maxMana = 30 + SPI*8 + INT*3 + (level-1)*8`
  - `baseDamage = 8 + STR*0.8 + level*2`
  - `defense = 3 + VIT*0.5 + level`
  - `expToNext = floor(100 * 1.15^(level-1))`
  - `manaRegen = 1 + SPI*0.1 /sec`
  - `hpRegen = 0.5 + VIT*0.05 /sec`
- **Level up**: +5 stat points, +1 skill point, full HP/MP restore
- **Auto-combat**: Toggle auto-battle, auto-skill priority list, auto-loot (off/all/magic/rare)

### Monster.ts (~315 lines)
- **AI States**: idle → patrol → chase → attack → dead
- **Patrol**: Random wandering within 2 tiles of spawn, 3s timer
- **Chase**: Triggered by aggro range, leash at 8 tiles from spawn
- **Attack**: Timing handled by ZoneScene combat loop
- **Stats derived from definition**: STR = damage*0.8, DEX = speed*0.1, VIT = hp*0.1
- **Visuals**: HP bar (hidden until damaged), name label, elite crown indicator
- **Knockback**: On damage, brief push-away tween
- **Animation**: CharacterAnimator with category-based configs (humanoid/slime/beast/large/flying/serpentine/demonic)

### NPC.ts (~324 lines)
- **States**: working → alert → idle → talking
- **Proximity detection**: Reacts when player within 3 tiles
- **Types**: blacksmith, merchant, quest, stash — each with unique color, hat, ambient VFX (sparks for blacksmith, gold particles for merchant, wisps for quest)
- **Quest markers**: Dynamic !/? indicators based on quest state
- **Animation**: Uses SpriteGenerator for spritesheet, falls back to procedural drawing

---

## 4. COMBAT SYSTEM

### CombatSystem.ts (~332 lines)
- **Damage formula**: `rawDamage = baseDmg * skillMultiplier * critMultiplier + elementalFlat`
- **After defense**: `max(1, rawDamage - effectiveDefense * 0.5) * (1 - damageReduction)`
- **Elemental resistance**: Reduces non-physical damage by resist% (capped 75%)
- **Dodge**: `DEX * 0.3%` (cap 30%)
- **Crit**: `DEX*0.2% + LCK*0.5% + skillCritBonus + equipCritRate` (cap 75%)
- **Crit damage**: `150% + LCK*1% + equipCritDamage`
- **D2-style skill scaling**: Tiered diminishing returns — levels 1-8 (100%), 9-16 (75%), 17-20 (50%)
- **Synergy system**: Skills boost each other's damage per invested level
- **Equipment stats**: 30+ stat types including life/mana steal, elemental damage, resistances, special effects (death save, crit double strike, free cast, ignore defense)
- **Difficulty multipliers**: Normal (1x), Nightmare (1.5x monster damage, 2x rewards), Hell (2x damage, 3x rewards)

### Status Effects
- **Design doc mentions**: Burn, Freeze, Poison, Bleed, Slow, Stun, Silence
- **Actually implemented**: Only buff-based damage reduction and damage bonus. No DoT system, no CC tracking per monster. Stun duration is defined on skills but not applied to monsters.

---

## 5. LOOT SYSTEM

### LootSystem.ts (~160 lines)
- **Quality tiers**: Normal → Magic (1-2 affixes) → Rare (3-4) → Legendary (fixed) → Set (2-3 + set bonuses)
- **Drop rates**: Base 40% (80% elite) + luck*0.5 bonus
- **Legendary chance**: 0.5% + luck*0.03 + (level>20 ? 1% : 0%)
- **Affix generation**: Random prefix/suffix from pool, slot-restricted, tiered by level
- **Consumables**: HP/MP potions (3 tiers), antidote, TP scroll, ID scroll
- **Gems**: 4 types (ruby/sapphire/emerald/topaz), 3 tiers

### Items Data (bases.ts)
- **Weapons**: 14 weapons (sword, axe, dagger, bow, staff, wand, shield) across level tiers
- **Armor**: 15 armor pieces across 5 slots
- **Accessories**: 4 accessories (rings, amulets)
- **Affixes**: 30 prefixes + 34 suffixes across 5 tiers
- **Legendaries**: 9 legendary items with unique effects
- **Sets**: 5 set definitions (Iron Guardian, Shadow Assassin, Archmage, Wilds Hunter, Abyssfire Oath) with 2/3/4-piece bonuses

### InventorySystem.ts (~311 lines)
- **Capacity**: 100 inventory slots, 80 stash slots
- **Equipment slots**: 10 (helmet, armor, gloves, boots, weapon, offhand, necklace, ring1, ring2, belt)
- **Features**: Equip/unequip, sell, sort, destroy normals, stash transfer, identify (scroll required), use consumables, set bonus tracking
- **Stacking**: Consumables and scrolls stack

---

## 6. SKILL SYSTEM

### Warrior (7 skills, 2 trees)
- **Combat Master** (3 skills): Slash (150% phys), Whirlwind (120% phys AOE), War Stomp (180% phys AOE + stun)
- **Guardian** (3 skills): Shield Wall (50% dmg reduction buff), Taunt Roar (30% def buff AOE), Vengeful Wrath (25% dmg bonus buff)
- **Total**: 6 active + synergies

### Mage (6 skills, 3 trees)
- **Fire** (2 skills): Fireball (180% fire), Meteor (250% fire AOE)
- **Frost** (2 skills): Blizzard (100% ice AOE), Ice Armor (20% reduction buff)
- **Arcane** (2 skills): Chain Lightning (130% lightning AOE), Mana Shield (30% reduction buff)

### Rogue (7 skills, 3 trees)
- **Assassination** (3 skills): Backstab (200% phys +20% crit), Poison Blade (poison buff), Vanish (100% stealth damage buff)
- **Archery** (2 skills): Multi Shot (80% phys AOE), Arrow Rain (60% phys AOE)
- **Traps** (1 skill): Explosive Trap (150% fire AOE)

### Gap Analysis vs Design Doc
- Design doc specifies "8-10 skills per tree, 4 tiers" — current: 2-3 skills per tree, 3 tiers
- Missing skills from design doc: Charge, Lethal Strike, Iron Fortress, Life Regen, Frenzy, Dual Wield Mastery, Bleed Strike, Unyielding, Fire Wall, Combustion, Ice Arrow, Freeze, Teleport, Arcane Torrent, Death Mark, Poison Cloud, Slow Trap, Chain Trap, Piercing Arrow, Poison Arrow

### SkillEffectSystem.ts (~1,295 lines)
- **Fully implemented VFX**: Slash (arc trail), Whirlwind (rotating arcs), Shield Wall (hex shield), Fireball (projectile + explosion), Blizzard (ice shards), Meteor (screen shake + descent), Ice Armor (orbiting crystals), Chain Lightning (zigzag arcs), Backstab (blink + blade flash), Poison Blade (green mist), Multi Shot (fan projectiles), Vanish (smoke bomb), Explosive Trap (ground marker + explosion), Arrow Rain (mass arrows), War Stomp, Taunt Roar, Vengeful Wrath
- **Also**: Monster attack VFX, basic attack slash, procedural skill icons (64×64)

---

## 7. MAP & WORLD SYSTEM

### MapGenerator.ts (~494 lines)
- **FULLY IMPLEMENTED** — generates tiles, collisions, decorations from anchor data
- **Pipeline**: Primary fill → border walls → secondary scatter → wall/obstacle scatter → theme-specific features (mountain ridges) → water bodies (cellular automata) → camp placement (11×11 palisade) → path carving (drunk walk between key points) → decorations
- **Themes**: plains, forest, mountain, desert, abyss — each with unique tile mix, wall density, water config, decoration types
- **Seeded randomness**: Deterministic via LCG
- **All 5 maps use procedural generation** (tiles:[] in map data, MapGenerator.generate() called at import time)

### Zone Data (5 zones, all 80×80 tiles)
| Zone | Level | Monsters | Spawns | Camps | NPCs |
|------|-------|----------|--------|-------|------|
| Emerald Plains | 1-10 | 3 types (slime, goblin, goblin chief) | 10 spawns | 2 camps | 3 NPCs |
| Twilight Forest | 10-20 | 4 types (skeleton, zombie, werewolf, werewolf alpha) | 8 spawns | 2 camps | 4 NPCs |
| Anvil Mountains | 20-30 | 3 types (gargoyle, stone golem, mountain troll) | 8 spawns | 2 camps | 4 NPCs |
| Scorching Desert | 30-40 | 4 types (fire elemental, desert scorpion, sandworm, phoenix) | 8 spawns | 2 camps | 4 NPCs |
| Abyss Rift | 40-50 | 4 types (imp, lesser demon, succubus, demon lord) | 8 spawns | 2 camps | 3 NPCs |

### Quest Content (all_quests.ts — 742 lines)
- **42 total quests** across 5 zones
- Per zone: 5 main line quests + 4-5 side quests
- Types: kill, collect, explore, talk
- Progression chains with prereqQuests
- Story arc: Goblins → Undead/Werewolves → Dwarven ruins → Fire elementals → Demon invasion
- **Quest writing quality**: Each quest has Chinese name, description, clear objectives, appropriate rewards

### Missing from Design Doc
- **Zone 6 (Random Dungeons)**: Not started
- **Races**: Human/Elf/Dwarf/Half-Orc starting stat modifiers — NOT implemented
- **Difficulty modes**: Nightmare/Hell exist in code as multipliers but no unlock flow
- **Death penalty**: Design says "gear drops at death location" — NOT implemented (just respawn at camp)

---

## 8. HOMESTEAD SYSTEM

### HomesteadSystem.ts (~140 lines)
- **FULLY FUNCTIONAL** — not a skeleton
- **6 Buildings**: Herb Garden (potion discount), Training Ground (exp bonus), Gem Workshop (gem bonus), Pet House (pet slots), Warehouse (stash slots), Altar (temp buffs)
- **Upgrade costs**: 5 levels each (100-4000 gold scaling)
- **Pet system**: 5 pets defined (Sprite +EXP, Dragon +ATK, Owl +drops, Cat +crit, Phoenix +HP regen)
- **Pet mechanics**: Feed to gain EXP, level up (max 20), active pet gives passive bonus
- **Integration**: `getTotalBonuses()` feeds into combat/loot calculations
- **UI**: Full homestead panel in UIScene with upgrade buttons, pet display

### Gaps
- Pets are passive only (no visual representation in game world)
- No building visual on homestead (just a menu panel)
- Altar buff mechanic defined but not fully wired (no "use altar" interaction)
- No homestead "zone" or visual scene — just a UI panel

---

## 9. ACHIEVEMENT SYSTEM

### AchievementSystem.ts (~90 lines)
- **FULLY FUNCTIONAL** — not a skeleton
- **12 achievements**: Kill-based (first kill, 100 kills, 500 kills, specific monsters), level-based (10/25/50), exploration (all zones), quest (10 quests), collection (legendary item)
- **Rewards**: Stat bonuses (damage, luck, strength) and titles
- **Integration**: Called on monster kill, level up, zone entry, quest completion
- **Persistence**: Saves/loads progress via save system
- **UI**: Achievement notification in combat log (no dedicated panel yet)

---

## 10. AUDIO SYSTEM

### AudioManager + MusicEngine + SFXEngine (~2,318 lines total)
- **Music**: Per-zone explore/combat/victory tracks, crossfade transitions (2s), combat state switching
- **SFX**: Procedurally generated via Web Audio API oscillators (no sample files)
- **Effects**: Click, hit, crit, dodge, level up, pickup, equip, sell, quest complete, death, heal, fireball, blizzard, lightning, poison, whirlwind, backstab, arrow, trap, portal
- **External BGM**: 17 MP3 files in `public/assets/audio/bgm/` (explore/combat/victory per zone + menu)
- **Volume**: Configurable master/music/SFX volumes, mute support
- **Browser safety**: Handles AudioContext resume on user gesture

---

## 11. VISUAL/ART PIPELINE

### SpriteGenerator.ts (~1,372 lines)
- **Tile generation**: 7 tile types × 4 variants each, with procedural gradients, noise, textures
- **Tile transitions**: Blended edge tiles generated on-the-fly
- **Camp themes**: Per-zone themed camp ground/wall/decoration textures
- **Entity spritesheets**: Generated on demand (not at boot) for players, monsters, NPCs
- **Decoration sprites**: Trees, bushes, flowers, rocks, mushrooms, crystals, cacti, bones, boulders
- **Camp decorations**: Campfire, well, banner, tent, barrel, crate, torch

### Procedural Sprite Files (graphics/sprites/)
- **Players**: 3 files (Warrior, Mage, Rogue) — ~350-380 lines each, full 8-frame spritesheet with idle/walk/attack/cast/hurt/death
- **Monsters**: 17 files — detailed procedural art for each monster type
- **NPCs**: 11 files — unique visual for each NPC
- **Decorations**: 9 files
- **Effects**: LootBag, ExitPortal

### CharacterAnimator.ts (~746 lines)
- **Animation states**: idle, walk, attack, cast, hurt, death
- **Features**: Hit freeze (pause frames), body part decomposition on death, procedural idle breathing, attack anticipation, hurt recoil
- **Per-class configs**: Different timing/effects for warrior/mage/rogue/humanoid/slime/beast/large/flying/serpentine/demonic

### External Assets
- **public/assets/audio/bgm/**: 17 MP3 BGM files (the only real external assets)
- **public/assets/tiles/**: LICENSE file only (Kenney CC0)
- **public/assets/sprites/**: Directory exists, empty (no PNGs)
- **public/assets/effects/**: Empty
- **public/assets/ui/**: Empty

---

## 12. CONTROLS

### Desktop
| Key | Action |
|-----|--------|
| WASD/Arrows | Move (8-directional isometric) |
| Left click | Move to tile / Attack monster / Interact NPC / Pick up loot |
| Right click / R | Town Portal (teleport to camp) |
| 1-6 | Use skills |
| TAB | Toggle auto-combat |
| I | Inventory |
| C | Character stats |
| K | Skill tree |
| J | Quest log |
| M | World map |
| H | Homestead |
| O | Audio settings |
| ESC | Return to menu |
| Ctrl+Shift+E | Export textures (dev only) |

### Mobile
- **MobileControlsSystem.ts** (~306 lines): Virtual joystick (left), skill buttons (right)
- **Detection**: `isMobileDevice()` checks for touch + screen width
- **Status**: Implemented but not extensively tested

---

## 13. SAVE SYSTEM

### SaveSystem.ts (~50 lines) + Dexie DB
- **Storage**: IndexedDB via Dexie.js, database "AbyssfireDB"
- **Auto-save**: On zone change and camp rest, stored as 'autosave'
- **Manual slots**: 3 slots (save_1, save_2, save_3) — infrastructure exists but no UI for manual save slots

### What Gets Persisted (SaveData type)
- Player: level, exp, gold, HP/MP, stats, free points, skill levels, position, current map
- Inventory + Equipment + Stash (full ItemInstance arrays)
- Quest progress (all quest states)
- Exploration data (fog of war per zone)
- Homestead (building levels, pets, active pet)
- Achievements (progress + unlocked)
- Settings (auto-combat, volumes, auto-loot mode)
- Difficulty + completed difficulties

### Save Trigger Points
- Zone entry (auto-save)
- Zone transition (via code in ZoneScene)
- No periodic auto-save timer

---

## 14. ADDITIONAL SYSTEMS

### LightingSystem.ts (~343 lines)
- **Implemented**: Ambient darkness overlay, point lights (player halo, campfires, torches), per-zone ambient colors, flicker effect for fire lights
- **Technique**: Graphics overlay with alpha masking, not true WebGL lighting

### VFXManager.ts (~359 lines)
- **Implemented**: Camera shake, hit flash (white overlay tween), hit sparks (particles), death burst, gold burst, heal burst, skill impact bloom, danger vignette (low HP red edge), camera flash, glow effect, bloom effect

### WeatherSystem.ts
- **Implemented**: Per-zone weather particle effects

### TrailRenderer.ts
- **Implemented**: Weapon slash trails, ground scorch marks (fire/ice/lightning)

### MobileControlsSystem.ts (~306 lines)
- **Implemented**: Virtual joystick with touch zones, skill button overlay

### FogOfWarSystem.ts
- **Referenced** in save data and design doc but actual FogOfWarSystem file exists — appears to be minimal/unused (fog data tracked as exploration in ZoneScene via exploredZones Set)

### ColorGradePipeline.ts
- **Implemented**: WebGL post-processing for painterly color mood per zone

---

## 15. NUMERICAL BALANCE SUMMARY

### Player Progression
- **Level cap**: 50
- **EXP curve**: `100 * 1.15^(level-1)` — level 10 needs ~352 XP, level 25 needs ~2,667, level 50 needs ~83,767
- **Stat points**: +5/level, skill points: +1/level
- **HP at level 50**: ~785 base (no VIT investment), ~2,285 with 100 VIT

### Monster Power Scaling
| Zone | Monster HP | Damage | Defense | EXP | Gold |
|------|-----------|--------|---------|-----|------|
| Plains (1-10) | 30-150 | 5-15 | 2-8 | 10-60 | 1-15 |
| Forest (10-20) | 80-500 | 12-35 | 8-20 | 40-300 | 5-40 |
| Mountains (20-30) | 180-800 | 25-45 | 18-35 | 100-500 | 10-60 |
| Desert (30-40) | 300-2000 | 35-80 | 25-50 | 200-800 | 15-80 |
| Abyss (40-50) | 250-5000 | 30-120 | 20-60 | 150-2000 | 10-200 |

### Item Power Scaling
- **Weapon damage**: 2-8 (lv1 sword) → 25-45 (lv30 demon blade) → 30-55 (lv40 abyssal staff)
- **Armor defense**: 3 (lv1 leather) → 25 (lv30 plate) → 35 (lv40 demon helm)
- **Affix tiers**: T1 (lv1, +1-5) → T5 (lv40, +35-100)

---

## 16. WHAT'S COMPLETELY MISSING

Based on design doc vs codebase:

1. **Status effects/DoT system**: Burn, Freeze, Poison, Bleed, Slow, Stun — defined in design doc, referenced in skills (stunDuration field), but no actual application to monsters
2. **Elite monster affix system**: Design doc describes "Fire Enhanced", "Swift" random affixes — NOT implemented (elites just have higher stats)
3. **Death penalty**: Design doc says gear drops at death location, lose gold — NOT implemented
4. **Gem socketing**: Socket system exists on items but no UI/mechanic to socket gems
5. **Crafting**: Blacksmith only buys/sells, no crafting recipes
6. **Item identify mechanic**: ID scroll exists, identify code exists, but items drop pre-identified
7. **Races**: Human/Elf/Dwarf/Half-Orc starting stat modifiers — NOT implemented
8. **Random dungeons (Zone 6)**: NOT started
9. **Difficulty unlock flow**: Nightmare/Hell multipliers exist but no progression/unlock UI
10. **Tutorial/new player guide**: NOT implemented
11. **Missing ~60% of planned skills per class** (2-3 per tree vs designed 8-10)
12. **Mercenary/follower system**: NOT in design doc or code
13. **Achievement UI panel**: Achievements track and reward but no dedicated panel to view them
14. **Fog of War visual**: Data structure exists but no actual fog rendering
15. **Homestead visual scene**: Just a UI panel, no explorable homestead area

---

## 17. ARCHITECTURE PATTERNS

### Communication
- **EventBus**: Central Phaser EventEmitter with 30+ typed event constants
- **Direct references**: ZoneScene holds direct refs to all systems (inventorySystem, questSystem, etc.)
- **UI refresh**: EventBus 'ui:refresh' event when zone transitions

### System Lifecycle
- **First-load only**: InventorySystem, QuestSystem, HomesteadSystem, AchievementSystem, SaveSystem (survive zone transitions)
- **Per-zone**: CombatSystem, SkillEffectSystem, LightingSystem, VFXManager, WeatherSystem, TrailRenderer, PathfindingSystem (recreated per zone)
- **Singletons**: audioManager (module-level instance)

### Performance Patterns
- Viewport culling for tiles (100ms throttle)
- Cached equipment stats (invalidated on equip change)
- Throttled quest/NPC marker checks (500ms)
- World position pre-computation on zone load
- Procedural texture generation on demand (not all at boot)
- BGM lazy-loading per zone

---

## 18. POTENTIAL BUGS / INCONSISTENCIES

1. **Stun not applied**: Skills define `stunDuration` but monster state machine doesn't handle stun state
2. **Buff effects limited**: Only `damageReduction`, `defenseBonus`, `damageBonus` checked — `stealthDamage`, `poisonDamage` buffs from Rogue skills aren't used in damage calc
3. **Auto-loot mode cycles**: 'off' → 'all' → 'magic' → 'rare' but no visual indicator of current state beyond button text update
4. **Save data version**: Always version 1, no migration logic
5. **Fog of war**: exploredZones is a Set but fogData is Record<string, boolean[][]> in ZoneScene — fogData is declared but never populated with actual per-tile fog
6. **Town portal**: useTownPortal method referenced but implementation is in truncated part of ZoneScene
7. **Mana shield**: Buff stat is 'damageReduction' not actually converting damage to mana cost as described
8. **Taunt**: taunt_roar AOE affects monsters but doesn't actually force aggro (just a defense buff)
9. **Double shot / crit double strike**: Set bonus stats tracked but no combat code checks for them

# Entity Visual Overhaul Design

## Goal

Replace the template-based procedural sprite generation with per-entity custom drawing. Every player class, monster, NPC, decoration, and effect gets a unique visual identity in a realistic dark fantasy style (Diablo II aesthetic). Additionally, fix the asset pipeline so externally loaded PNGs are respected, and add a dev export tool for extracting procedural sprites as PNGs.

## Decisions

- **Approach**: Hybrid — enhanced SpriteGenerator with per-entity drawing modules + texture export tool + external PNG override support
- **Scope**: All entities (3 players, 18 monsters, 11 NPCs, 9 decorations, 2 effects)
- **Art style**: Realistic dark fantasy — gradient shading, anatomical detail, rich materials, dark atmosphere
- **Silhouettes**: Every monster type gets a unique body archetype (no more shared humanoid template)

## Architecture Changes

### A. SpriteGenerator Refactor

Current state: `SpriteGenerator.ts` uses 2 shared drawing functions (`drawHumanoidFrame`, `drawBlobFrame`) with config parameters (colors, sizes, accessories). All humanoid monsters share the same body structure.

New structure: Per-entity drawing modules under `src/graphics/sprites/`. Each module exports a drawing function. `SpriteGenerator` orchestrates but delegates all drawing.

```
src/graphics/
  SpriteGenerator.ts             # Orchestrator (unchanged API surface)
  DrawUtils.ts                   # Shared drawing primitives
  sprites/
    players/
      PlayerWarrior.ts
      PlayerMage.ts
      PlayerRogue.ts
    monsters/
      Slime.ts
      Goblin.ts
      GoblinChief.ts
      Skeleton.ts
      Zombie.ts
      Werewolf.ts
      WerewolfAlpha.ts
      Gargoyle.ts
      StoneGolem.ts
      MountainTroll.ts
      FireElemental.ts
      DesertScorpion.ts
      Sandworm.ts
      Phoenix.ts
      Imp.ts
      LesserDemon.ts
      Succubus.ts
      DemonLord.ts
    npcs/
      Blacksmith.ts
      BlacksmithAdvanced.ts
      Merchant.ts
      MerchantDesert.ts
      Stash.ts
      QuestElder.ts
      QuestScout.ts
      ForestHermit.ts
      QuestDwarf.ts
      QuestNomad.ts
      QuestWarden.ts
    decorations/
      Tree.ts
      Bush.ts
      Rock.ts
      Flower.ts
      Mushroom.ts
      Cactus.ts
      Boulder.ts
      Crystal.ts
      Bones.ts
    effects/
      LootBag.ts
      ExitPortal.ts
  TextureExporter.ts             # Dev export tool
```

Each drawing module implements the `EntityDrawer` interface:

```typescript
interface EntityDrawer {
  readonly key: string;          // Texture key (e.g. 'monster_slime')
  readonly frameW: number;       // Frame width (before TEXTURE_SCALE)
  readonly frameH: number;       // Frame height (before TEXTURE_SCALE)
  readonly totalFrames: number;  // Total frames in sprite sheet

  drawFrame(
    ctx: CanvasRenderingContext2D,
    frame: number,
    action: string,             // 'idle' | 'walk' | 'attack' | 'hurt' | 'death' | 'cast'
    w: number,                  // Scaled frame width
    h: number,                  // Scaled frame height
    utils: DrawUtils,           // Shared drawing utilities
  ): void;
}
```

For decorations and effects (single-frame static textures), `totalFrames` is 1, `action` is always `'idle'`, and `frame` is always 0. The same interface is used for simplicity.

### B. Shared Drawing Utilities (DrawUtils.ts)

Reusable primitives extracted from the current SpriteGenerator plus new ones:

`DrawUtils` is a plain class instantiated once per `SpriteGenerator` run, holding the noise state (`hash2d` seed) and providing stateless drawing helpers. It is passed to each drawer's `drawFrame()` call.

- `gradientFill(ctx, type, colors, bounds)` — radial/linear gradient fills
- `noise2d(x, y)` / `fbm(x, y, octaves)` — existing noise functions
- `applyNoiseToRegion(ctx, region, intensity)` — texture overlay
- `drawLimb(ctx, joints, thickness, color)` — jointed limb with muscle curves
- `drawStoneTexture(ctx, bounds, baseColor)` — cracked stone material
- `drawMetalSurface(ctx, bounds, baseColor)` — metallic shading with specular
- `drawLeatherTexture(ctx, bounds, baseColor)` — worn leather with stitching
- `drawFlameLayer(ctx, bounds, intensity)` — layered flame rendering
- `drawBoneSegment(ctx, start, end, width)` — anatomical bone drawing
- `drawFurTexture(ctx, bounds, baseColor, direction)` — directional fur strokes
- `fillEllipse(ctx, cx, cy, rx, ry)` — existing helper
- `fillCircle(ctx, cx, cy, r)` — existing helper
- `rgb(hex, alpha?)` — existing color conversion

### C. Texture Override Logic

External PNG overrides require two changes:

**1. BootScene update**: Change `this.load.image()` to `this.load.spritesheet()` for all entity assets, with frame dimensions matching each entity's config. This is necessary because entity textures are multi-frame sprite sheets — a single-frame `image` load would lack frame sub-regions and break animations. The NPC load list must also be expanded from 4 generic keys (`npc_blacksmith`, `npc_merchant`, `npc_quest`, `npc_stash`) to all 11 distinct NPC keys.

A frame-size registry (simple map of texture key → `{frameWidth, frameHeight}`) is exported from the drawing modules so BootScene can pass correct dimensions to `this.load.spritesheet()`.

**2. SpriteGenerator skip check**: Fix `makeCharSheet()` and all `generate*()` methods to respect externally loaded spritesheets:

```typescript
private shouldSkipGeneration(key: string): boolean {
  if (!this.scene.textures.exists(key)) return false;
  const tex = this.scene.textures.get(key);
  // HTMLImageElement = loaded from file; HTMLCanvasElement = procedurally generated
  return tex.source[0]?.source instanceof HTMLImageElement;
}
```

Each generate method checks `shouldSkipGeneration(key)` before drawing. This enables incremental replacement: drop a hand-drawn sprite sheet PNG in `public/assets/` and it takes priority over the procedural version.

### D. Texture Export Tool (TextureExporter.ts)

Dev-only utility triggered by `Ctrl+Shift+E` in ZoneScene:

1. Iterates all texture keys matching known entity patterns
2. For each texture, gets the canvas source via `texture.getSourceImage()`
3. Converts to PNG blob via `canvas.toDataURL('image/png')`
4. Downloads individual PNGs with filenames matching the `public/assets/` directory structure (e.g., `sprites-monsters-monster_slime.png`)
5. A simple HTML manifest page opens showing download links for all exported textures

No external dependencies required. Only registered in dev mode (`import.meta.env.DEV`).

## Monster Archetypes

Each monster gets a unique body type. Drawing logic is fully custom per entity.

### Zone 1 — Emerald Plains

| Monster | Archetype | Key Visual Features |
|---------|-----------|-------------------|
| Slime | Ooze | Translucent body with subsurface organs visible, specular highlights, dripping tendrils, internal glow. Idle: gentle pulsing. Walk: squish/stretch. Attack: lunging splat. |
| Goblin | Small Humanoid | Heavy brow ridge, bulbous warty nose, sinewy limbs, leather scraps, nail-studded crude club, wrapped feet. Hunched aggressive posture. |
| Goblin Chief | Small Humanoid+ | Larger, battle scars across face, battered metal crown with inset gem, crude shoulder pauldrons, war axe with notched blade, metal-capped boots. |

### Zone 2 — Twilight Forest

| Monster | Archetype | Key Visual Features |
|---------|-----------|-------------------|
| Skeleton | Undead Bones | Anatomical skull with separate jaw, deep eye sockets with faint soul-fire glow, curved ribcage with individual ribs, visible spine with vertebrae, separate radius/ulna arm bones, rusted sword with pitted blade. |
| Zombie | Undead Flesh | Lopsided head (tilted), exposed skull patch on one side, sunken glowing green eyes (uneven sizes), torn clothing with visible rotting flesh beneath, one arm outstretched (classic zombie pose), stiff asymmetric gait. |
| Werewolf | Beast | Wolf-like skull with protruding muzzle, predatory amber slit-pupil eyes, pointed ears with inner fur detail, muscular defined torso, digitigrade (reverse-knee) legs, curved claws, chest fur pattern. Snarling with visible fangs. |
| Werewolf Alpha | Beast+ | 20% larger than werewolf, darker fur, glowing red eyes, visible battle scars across torso and face, heavier musculature, alpha posture (more upright). |
| Gargoyle | Winged Stone | Weathered grey stone texture with moss patches, bat-like wing membranes with visible veins, curved horns, crouching compact posture, stone cracks with faint orange glow, curled stone tail. |

### Zone 3 — Anvil Mountains

| Monster | Archetype | Key Visual Features |
|---------|-----------|-------------------|
| Stone Golem | Elemental Rock | Angular boulder-like body segments (not smooth), glowing orange cracks between segments, pillar legs, massive asymmetric fists, small boulder head with ember eyes, no visible joints — segments float with energy between them. |
| Mountain Troll | Giant | Massive belly with visible navel, disproportionately small head, tiny beady eyes, prominent underbite with two tusks, enormous arms that reach past knees, short thick legs, grey-green mottled skin, carrying tree-trunk club. |
| Fire Elemental | Elemental Flame | No solid body — layered flame silhouette with outer dark orange, mid bright orange, inner yellow, white-hot core. Face emerges from flames (eyes and mouth as bright spots). Floating embers and spark particles around body. Constant flicker animation. |

### Zone 4 — Scorching Desert

| Monster | Archetype | Key Visual Features |
|---------|-----------|-------------------|
| Desert Scorpion | Arachnid | Wide/low body (landscape orientation), segmented chitinous carapace with sheen, anatomical pincers (chelae) with serrated inner edge, articulated tail segments curving overhead ending in venomous stinger with dripping venom, 8 jointed legs, cluster of beady eyes on cephalothorax. |
| Sandworm | Serpentine | Body emerging from ground at angle, circular mouth with concentric rings of teeth, eyeless head with sensory pits, visible body segments/rings, coils visible behind head above ground, sand particles spraying from emergence point, rough sandy hide texture. |
| Phoenix | Flaming Bird | Majestic bird form, layered flame-feather wings (individual feather shapes visible), ornate head crest, sharp curved beak, blazing eyes, long trailing tail feathers dissolving into fire, ember particles, warm light aura. |

### Zone 5 — Abyss Rift

| Monster | Archetype | Key Visual Features |
|---------|-----------|-------------------|
| Imp | Small Demon | Oversized head relative to body (chibi proportions), small curved horns, bat-like wings (too small to fly — cosmetic), large mischievous amber eyes, wide toothy grin, skinny limbs, spade-tipped tail, reddish skin. |
| Lesser Demon | Demon | Muscular humanoid, curved goat-like horns, fanged mouth, clawed hands with elongated fingers, barbed tail, purple-red skin, defined musculature (pectorals, abs visible), hoofed feet. |
| Succubus | Demon Elegant | Slender feminine form, swept-back curved horns, dark flowing hair, elegant bat wings, alluring glowing pink eyes, dark lips, form-fitting dark clothing, spade-tipped tail, clawed fingertips. More graceful than threatening. |
| Demon Lord | Boss Demon | Massive frame (largest monster), ram-like spiraling horns with ridges, enormous tattered bat wings, burning red eyes with vertical pupils, chest bearing glowing arcane rune/sigil, dark purple-black skin, spiked shoulder pauldrons, hooved digitigrade legs, heavy barbed tail. Subtle dark aura particle effect. |

## Player Classes

Each class gets a distinct silhouette, not just color swaps.

| Class | Build | Visual Identity |
|-------|-------|----------------|
| Warrior | Stocky, broad shoulders | Heavy plate pauldrons, chain mail visible at joints, kite shield with emblem, longsword with crossguard, iron helm with nose guard, thick armored boots. |
| Mage | Lean, tall | Flowing layered robes, pointed wizard hat with star motif, gnarled staff with glowing crystal tip, arcane energy wisps around hands, narrow frame, cloth shoes. |
| Rogue | Athletic, balanced | Leather armor with buckles, hooded cloak draped over one shoulder, dual curved daggers, bandolier with pouches, cloth-wrapped boots, agile stance. |

### Class Animation Differences

- **Warrior**: Heavy footfalls with body bob, shield-forward walk, overhead slash attack, shield-raise hurt
- **Mage**: Gliding walk with minimal bounce, staff-plant idle, channel-cast with both hands raised, recoil hurt
- **Rogue**: Light-footed walk with slight lean, dagger-flip idle, rapid dual-slash attack, dodge-roll hurt

## NPCs

The existing NPC visual overhaul design (`docs/plans/2026-03-16-npc-visual-overhaul-design.md`) is incorporated:

- 4-state animation system (working, alert, idle, talking) — kept as-is
- Ambient VFX (forge sparks, coin glints, mystic wisps) — kept as-is
- Sprite resolution 80x120 with 24 frames — kept as-is

This spec upgrades the drawing quality: each NPC gets a unique face (not just accessories). The blacksmith is grizzled with a square jaw, the elder has deep wrinkles, the desert merchant has a hooked nose and kohl-lined eyes, etc.

## Decorations

| Decoration | Visual Upgrade |
|-----------|---------------|
| Tree | Gnarled trunk with bark texture lines, layered canopy with depth shading, visible roots at base |
| Bush | Leafy clusters with berry accents, ground shadow, slight color variation per leaf clump |
| Rock | Rough stone with lichen spots, surface cracks, varied angular shapes |
| Flower | Distinct petal shapes, stem with small leaves, subtle color variation between instances |
| Mushroom | Cap with spots and visible gills underneath, thick stem, subtle bioluminescent glow for forest zones |
| Cactus | Ribbed surface with needle clusters, occasional flower bloom at top |
| Boulder | Massive angular form, moss-covered top, partially embedded in ground |
| Crystal | Faceted surfaces with light refraction highlights, inner color glow, transparent edges |
| Bones | Scattered skeletal remains, cracked and weathered, partially buried in ground |

## Effects

| Effect | Visual Upgrade |
|--------|---------------|
| Loot Bag | Leather pouch with drawstring, visible stitching, metallic buckle with sheen, bulging shape suggesting contents, ground shadow |
| Exit Portal | Swirling energy vortex with concentric rings, particle sparks at edges, inner glow gradient, pulsing animation |

## Animation Frame Layout

Unchanged from current system. Keeping the same frame counts ensures zero animation system changes.

### Monsters (20 frames)

| Frames | Action | Count |
|--------|--------|-------|
| 0–3 | Idle | 4 |
| 4–9 | Walk | 6 |
| 10–13 | Attack | 4 |
| 14–15 | Hurt | 2 |
| 16–19 | Death | 4 |

### Players (24 frames)

| Frames | Action | Count |
|--------|--------|-------|
| 0–3 | Idle | 4 |
| 4–9 | Walk | 6 |
| 10–13 | Attack | 4 |
| 14–15 | Hurt | 2 |
| 16–19 | Death | 4 |
| 20–23 | Cast | 4 |

### NPCs (24 frames)

| Frames | Action | Count |
|--------|--------|-------|
| 0–7 | Working | 8 |
| 8–11 | Alert | 4 |
| 12–17 | Idle | 6 |
| 18–23 | Talking | 6 |

## Files Modified

| File | Changes |
|------|---------|
| `src/graphics/SpriteGenerator.ts` | Refactor `generatePlayerSheets()`, `generateMonsterSheets()`, `generateNPCSprites()`, `generateDecorations()`, `generateEffects()` to delegate to per-entity drawing modules. Add `shouldSkipGeneration()` check. Remove shared `drawHumanoidFrame()`/`drawBlobFrame()` (logic moves to modules). Keep `generateTiles()`, `generateBlendedTile()`, `generateCampDecorations()`, noise utilities, animation registration, color utilities unchanged. |
| `src/graphics/DrawUtils.ts` | New file. Shared drawing primitives (gradients, materials, limbs, noise). Extracted from SpriteGenerator + new utilities. |
| `src/graphics/sprites/**/*.ts` | 43 new files (3 players + 18 monsters + 11 NPCs + 9 decorations + 2 effects). Per-entity drawing modules implementing `EntityDrawer` interface. |
| `src/graphics/TextureExporter.ts` | New file. Dev-only texture export tool. |
| `src/scenes/ZoneScene.ts` | Register `Ctrl+Shift+E` keybinding for texture export (dev mode only). |
| `src/scenes/BootScene.ts` | Update external asset loading: change `this.load.image()` to `this.load.spritesheet()` for entity textures with correct frame dimensions. Expand NPC load list from 4 generic keys to all 11 distinct NPC keys. Import frame-size registry from drawing modules. |

## Constraints

- All sprites remain procedurally generated at runtime (no mandatory external assets)
- External PNGs in `public/assets/` override procedural generation when present
- Animation frame counts and layout unchanged — zero impact on animation system
- `TEXTURE_SCALE` (currently 2) still applies to all generated sprites
- Existing NPC interaction, combat, and movement systems unchanged
- Performance: canvas generation runs once in BootScene, not per-frame

# Camp Redesign: Diablo II-Style Encampments

## Summary

Upgrade camps from bare 5x5 floor tiles to immersive Diablo II-style encampments with palisade walls, campfire, decorations, zone-themed visuals, and a safe zone that prevents monsters from entering.

## Decisions

- **Boundary**: Hybrid — palisade walls on 3 sides + open entrance + safe zone radius
- **Size**: 11x11 tile footprint (9x9 interior inside walls)
- **Interior elements**: Campfire, wall torches, tents, barrels/crates, banners, well/waypoint
- **Zone theming**: Each zone gets unique wall material, ground tint, and accent colors
- **Camp size is parametric**: Larger camps possible on larger maps in the future

## Camp Layout

```
  W W W G G W W W W W W
  W . . . . . . . . . W
  W . T . . . . . T . W
  W . . . . . . . . . W
  W . . . B . W . . . W
  W . C . . F . . C . W
  W . . . . . . . . . W
  W . . T . . . T . . W
  W . . . . . . . . . W
  T . . . . . . . . . T
  . . . . . . . . . . .

  W = wall tile (collision)
  F = campfire (center)
  T = tent
  B = banner
  C = crates/barrels
  W (interior) = well/waypoint
  G = gate opening (3 tiles wide)
  . = camp ground tile (walkable)
  T (bottom corners) = entrance torches
```

Walls occupy the outer ring on top, left, and right sides. The south side is fully open as the entrance, flanked by torches. The gate opening on the north wall is 2 tiles wide for visual interest (not a functional gate).

## Camp Boundary & Monster Safe Zone

### Walls
- Wall tiles placed on the outer ring of the 11x11 area (top, left, right sides)
- South side open — no wall tiles, just entrance torches
- North wall has a 2-tile gap (decorative gate)
- Walls use `collision = false` (non-walkable) — physically blocks both player and monster movement
- Wall rendering reuses the existing 3D elevated wall tile style, themed per zone

### Safe Zone
- Radius: 7 tiles from camp center (euclidean distance)
- Extends beyond walls to cover the open entrance and surroundings
- Monster behavior when inside safe zone:
  - If in `chase` or `attack` state: immediately set to `idle`, move toward spawn
  - If in `idle` or `patrol` state: patrol target generation rejects tiles inside any safe zone
- Spawn rejection: `spawnMonsters()` skips any spawn tile within a camp's safe zone radius
- Camp centers passed to `Monster` constructor as `readonly` array — cheap distance check, no per-frame lookup

### Edge Cases
- Leashing monsters pass through safe zone briefly to reach spawn — acceptable
- Multiple camps: check distance to all camp centers (max 2 per zone currently)

## Interior Elements

### Campfire (center tile)
- Procedurally generated sprite at exact camp center
- Animated flame: orange/yellow particle tweens (Phaser particles or tween-based)
- Radial glow overlay: additive blend circle, gentle pulse tween
- Player respawn point (existing `respawnAtCamp` already uses camp center)

### Wall Torches (on wall tiles)
- One torch every 2-3 wall segments
- Smaller flame than campfire
- Each casts a small warm glow circle
- Subtle random flicker tween on alpha/scale

### Tents (corners, along walls)
- 2-3 per camp, placed in corners or along wall interiors
- Procedurally drawn as triangular/pitched shapes
- NPCs positioned near their tent (replacing arbitrary offset system)
- Non-collidable decoration

### Barrels & Crates (near walls and NPCs)
- 3-5 clusters per camp
- Small rectangular sprites, stacked groups of 1-3
- Collidable (non-walkable) for pathing interest
- Placed near walls and NPC positions

### Banners (entrance + walls)
- 2 flanking the entrance, 1-2 on walls
- Colored per zone theme
- Subtle swaying tween animation on rotation

### Well / Waypoint (near center)
- Placed 2 tiles from campfire
- Stone circle sprite
- Non-functional for now — positioned for future fast-travel

### Placement Strategy
- All positions computed relative to camp center by `MapGenerator`
- Deterministic layout template per camp size — no random scatter
- Elements have fixed logical positions so camps feel designed

## Zone-Themed Camp Variants

| Zone | Theme | Walls | Ground | Accents |
|------|-------|-------|--------|---------|
| Emerald Plains | `plains` | Wooden palisade (brown) | Warm wood planks | Green/gold banners |
| Twilight Forest | `forest` | Dark wood with moss tint | Mossy stone floor | Purple/silver banners |
| Anvil Mountains | `mountain` | Stone walls (grey) | Cobblestone floor | Red/iron banners |
| Scorching Desert | `desert` | Sandstone + cloth canopies | Sandy tile | Orange/copper banners |
| Abyss Rift | `abyss` | Dark obsidian/corrupted stone | Cracked dark stone | Crimson/black banners |

### Implementation
- `CampTheme` config object per `MapTheme`
- Defines: wall color, ground color, banner tint, torch flame color, tent fabric color
- `SpriteGenerator` uses these when generating camp tiles and decoration sprites
- `drawCamp()` splits into `drawCampGround(theme)` and `drawCampWall(theme)`

## Files Changed

### Modified
- **`src/systems/MapGenerator.ts`** — expanded camp placement: 11x11 layout with walls, ground tiles, decoration positions, themed tiles
- **`src/entities/Monster.ts`** — safe zone proximity check in `update()`, patrol target rejection inside safe zones
- **`src/scenes/ZoneScene.ts`** — pass camp centers to monsters, render camp decorations (campfire particles/glow, torches, tents, crates, banners, well), spawn rejection
- **`src/graphics/SpriteGenerator.ts`** — `drawCampWall(theme)`, `drawCampGround(theme)`, decoration sprite generators (torch, tent, barrel, crate, banner, well, campfire)
- **`src/data/types.ts`** — add `CampTheme` interface, add `safeZoneRadius` to `MapData`
- **`src/data/maps/*.ts`** — verify camp positions have 11x11 clearance, no map data conflicts

### New
- **`src/data/camp-themes.ts`** — `CampTheme` configs keyed by `MapTheme`

### Not Changed
- `Player.ts` — `respawnAtCamp()` already works with camp center coordinates
- `NPC.ts` — NPC placement is driven by `ZoneScene.spawnNPCs()`, not NPC itself
- `UIScene.ts` — no camp-related UI changes
- `CombatSystem.ts` — combat doesn't need to know about safe zones

# World presentation contract

What the world layer (`Source/Abyssfire/{World,Actors,Anim,Vfx,Camera}`) loads, how it addresses content and which
material / data layouts the content build (`Scripts/build_content.py`, the art exporter) must honour. The code never
hard-fails on missing content: a missing asset logs one warning and the thing is skipped (or drawn by a fallback).

Units: 1 tile = 100 uu (cm), X = column, Y = row, Z up. Web px convert with px x 100 / 45 (combat-feel.md 0).

## 1. Who calls what

| Interface | Implemented by | Used by |
| --- | --- | --- |
| `IAbyssWorldView` (BuildZone, ClearZone, SpawnEntity, DespawnEntity, SyncFrame, GetGroundHeight) | `UAbyssWorldBuilder` (world subsystem, registers itself with `UAbyssSimDriver`) | the driver |
| `UAbyssWorldBuilder::RaycastGround` / `PickGround(Controller, ScreenPos, OutWorld, OutTile)` | world | input agent (ray vs terrain height field, no collision) |
| `UAbyssWorldBuilder::SetWorldUi(IAbyssWorldUi*)` (auto-discovered when the UI root object implements `IAbyssWorldUi`) | world | UI agent |
| `IAbyssWorldUi` (AddWorldWidget, RemoveWorldWidget, ClearWorldWidgets, UpdateWorldWidgets, ShowFloatingText) | UI agent | world (nameplates, HP bars, labels, floating combat text) |
| `IAbyssPresenter` on `AAbyssCharacterActor` | world | driver (anim / hit / status / teleport events, hit-stop) |
| `UAbyssVfxSystem::Play / PlayFirst / Stop / PlayNotify`, `OnShake` | world | world builder, anyone wanting a recipe |
| `AAbyssCameraRig` (`AddZoomInput`, `SetZoomFactor`, `ScreenToGroundDirection`, `GetCamera`) via `UAbyssWorldBuilder::GetCameraRig()` | world | input agent (wheel / pinch zoom) |

The world layer only reads the core through the snapshot, the events (`FAbyssEventRouter`) and the data tables; it
contains no gameplay rule. All player-facing text goes through the UI (`IAbyssWorldUi`), never through world code.

## 2. Addressing content

### 2.1 Paths

Every asset lives under `/Game/Abyssfire`. Manifest assets (`Art/Export/manifest.json`, `FAbyssArtManifest`) load from
`/Game/Abyssfire/<Folder>/<Name>.<Name>` where `<Folder>` is the directory part of the asset's `fbx` path relative to the
export root (`Characters/SK_Hero_Warrior.fbx` -> `Characters`), else its `category`. Clips follow the same rule with their
own `fbx`, falling back to the owning asset's folder.

Fixed-name assets (not in the manifest) are searched in these folders, first hit wins:

| Kind | Folders | Names |
| --- | --- | --- |
| Materials, MPC | `Materials`, `Materials/Instances`, `Materials/FX` | `M_AF_*`, `MPC_AF_Lighting` (Materials only) |
| FX meshes | manifest first, then `FX`, `FX/Meshes`, `Pickups`, `Props` | `SM_FX_Quad`, `SM_FX_GuideArrow`, `SM_FX_EliteCrown`, `SM_FX_Arrow`, `SM_FX_HexPlate`, `SM_FX_Bubble`, `SM_FX_Rock_A`, `SM_FX_ClueMagnifier`, `SM_Pickup_GoldCoin`, `SM_Pickup_PotionHP/MP`, `SM_Loot_Bag` |
| FX sprites | `FX/Textures`, `Textures`, `FX` | `T_FX_<Sprite>` (section 5.4) |

### 2.2 Game ids

Manifest assets carry `gameIds`; the world resolves a list of candidate ids and takes the first that exists (variants
of one id are picked with the web tile hash). Resolution chains:

| Thing | Candidates (in order) |
| --- | --- |
| Hero | `player_<artId>`, `<artId>`, `player_<class>` (the core sends `artId` = class name, e.g. `warrior`) |
| Monster | `<artId>` (spriteKey), `monster_<artId>`, `<defId>`, `monster_<defId>` |
| NPC | `npc_<spriteId>`, `<spriteId>` (tower allies), `npc_<npcId>`, `<npcId>`, `<artId>` |
| Pet | `<artId>` (`beast_<id>[_e1/_e2]` after evolution), `beast_<petId>`, `pet_<petId>`, `<petId>` |
| Escort / other characters | `<artId>`, `npc_<artId>`, `<defId>`, `decor_<artId>` |
| Ground item | `loot_bag`, `SM_Loot_Bag` (tinted by quality) |
| Potion drop | `potion_drop_hp` / `potion_drop_mp`, `SM_Pickup_PotionHP` / `SM_Pickup_PotionMP` |
| Soul echo | `player_<class>`, `<class>` (drawn with `M_AF_Ghost`) |
| Quest clue | `<key>`, `clue_mark`, `fx_clue`, `SM_FX_ClueMagnifier` |
| Gather node / quest item | `gather_<itemKind>`, `quest_item_<itemKind>`, `<itemKind>`, `gather_node` |
| Lore pickup | `lore_<spriteType>`, `<spriteType>`, `decor_<spriteType>` |
| Hidden reward | chest: `<key>`, `decor_treasure_chest`, `treasure_chest`; other: `<key>`, `decor_<type>`, `<type>` |
| Treasure cache / puzzle | `decor_treasure_chest` / `decor_event_puzzle_rune_pillar` |
| Defend target | `<artId>`, `decor_<artId>`, `<defId>` (a skeletal asset makes it a character) |
| Wall outcrops | `outcrop_<theme>`, `wall_<theme>`, `outcrop`, `wall` (variant by `tileHash(c, r, 3)`) |
| Camp palisade | `palisade_<theme>`, `palisade`, `camp_wall`; gate posts `palisade_gate_<theme>`, `palisade_gate` |
| Lily pads (1 in 6 water tiles) | `lily_pad_<theme>`, `decor_lily_pad`, `lily_pad` |
| Decorations | `decor_<type>`, `<type>` (core jitter, tall types fade for occlusion) |
| Camp props | `camp_<type>`, `decor_camp_<type>`, `<type>` (tents / well / barrel / crate face the camp centre) |
| Exits | `exit_portal`; sealed (W7): `exit_gate_sealed`, `exit_sealed` |
| Story decorations | `decor_<spriteType>`, `<spriteType>`, `story_<id>` |

`<theme>` is `plains | forest | mountain | desert | abyss`. Weapons attach by `FindWeapon(weaponType, heldBy)`
(`weaponType` = `sword | axe | mace | dagger | bow | staff | wand | shield`, socket = the asset's `attachSocket`).

### 2.3 Skeletons, sockets, clips

Sockets read by the world: `fx_feet`, `fx_chest`, `fx_head`, `fx_overhead`, `fx_hand_r`, `fx_hand_l` (missing sockets
fall back to fractions of the asset height). Clip names used: `Idle`, `IdleNPC`, `Walk`, `Run`, `Hurt` (or additive
`HurtAdd`), `Death`, `Dodge`, `Talk`, `Work`, `Open` (chests), attack / cast / signature clips from the manifest
(`FindClipForSkill`). Anim notifies named `<N>` on clips play recipe `notify.<N>` (else `npc.<N>`).

## 3. Materials

### 3.1 M_AF_Terrain and the tile-id texture

The zone ground is a `UProceduralMeshComponent` (chunked sections, no collision) built by `FAbyssTerrainField`.

* Vertex colours: R = water depth 0..1, G = wall-tile coverage 0..1, B = skirt factor 0 (map) .. 1 (4+ tiles out), A = 1.
* UV0 = tile-space position (col, row); `UV0 / 3` is the web's 3x3-tile pattern period.
* Texture parameter `TileIds`: transient `Cols x Rows` B8G8R8A8, point sampled, clamp, not sRGB. Sample at
  `(floor(UV0) + 0.5) / (MapCols, MapRows)`. Channels: R = paint index (tile type; walls -> `WallPaintTile`, camp walls
  -> 5), G = detail variant `tileHash(c, r, 7) % 12`, B = 255 walkable / 0 blocked, A = raw tile type
  (0 grass, 1 dirt, 2 stone, 3 water, 4 wall, 5 camp).
* Scalars: `MapCols`, `MapRows`, `WallPaintTile`.
* Per paint index `<i>` in `0, 1, 2, 3, 5` (keys of `terrain_styles.json` `themes.<theme>.ground`): vectors
  `Tile<i>Base`, `Tile<i>PatchA`, `Tile<i>PatchB`, `Tile<i>Lip`, `Tile<i>Accent`; scalar `Tile<i>Rank` (blend priority).
* Water colours (also on M_AF_Water): `WaterBase`, `WaterShallow`, `WaterFoam`, `WaterBank`, `WaterWave`.

### 3.2 M_AF_Water

Translucent surface sections at -12 cm over the basins (bed -60 cm). Same `TileIds`, `MapCols`, `MapRows` and water
vectors as 3.1; vertex colour R = depth.

### 3.3 Character / prop custom primitive data (`AbyssCpd`)

Written on every body, weapon and prop mesh component; the toon material (`M_AF_Toon` family assigned by the content
build) reads them as custom primitive data parameters:

| Index | Meaning |
| --- | --- |
| 0 `HitFlash` | 0..1 final colour -> white |
| 1..3 `PainTint` RGB, 4 amount | hero pain tint (multiply) |
| 5..7 `Telegraph` RGB, 8 amount | monster wind-up tint |
| 9..11 `Tint` RGB, 12 amount | status tint (characters) / quality tint (loot) / ghost colour / light-pool colour + alpha |
| 13 `Fade` | 0 opaque .. 1 invisible (dithered opacity mask) |
| 14 `Ghost` | 0..1 ghost look (soul echo, afterimages) |
| 15 `Highlight` | 0..1 hover / story-focus rim |

### 3.4 Instanced dressing (`AbyssDecorIcd`)

Decor / outcrop / palisade / camp ISMs have 3 per-instance custom data floats: 0 `Fade` (occlusion, 0 opaque),
1 `Random` (stable 0..1: wind phase, colour jitter), 2 `Highlight` (story-decoration focus rim). Materials used on
dressing meshes must read `PerInstanceCustomData[0..2]` (and dither on Fade).

### 3.5 MPC_AF_Lighting

Updated every `lightingUpdateIntervalMs`: vectors `SunDir`, `KeyLightDir` (both = direction towards the key light),
`SunColor`, `Ambient`, `RimColor`; scalars `CameraTanHalfFovY`, `TimeSec` (wraps at 3600). Missing parameters are skipped.

### 3.6 Post-process grade

The zone actor owns an unbound post-process: manual exposure (`abyss.ExposureBias`), tone curve 0, no motion blur /
fringe / grain / AO, bloom per tier (`abyss.Bloom`), vignette from the zone mood. When `M_AF_PP_Grade` exists it is added
as a blendable with: vectors `Ambient`, `Haze`, `Lift`, `Gain`; scalars `AmbientAlpha`, `HazeAlpha`, `Saturation`,
`Contrast` (the web mood grade: multiply by Ambient x AmbientAlpha, add Haze x HazeAlpha, saturation / contrast, lift /
gain). Otherwise the engine colour grading approximates it.

### 3.7 Utility materials

| Material | Used on | Inputs |
| --- | --- | --- |
| `M_AF_FX_Additive`, `M_AF_FX_Translucent` | VFX quads (two-sided, unlit) | texture `Sprite`, scalar `Cells`; instance data 0..3 RGBA, 4 Variant, 5 Age (0..1) |
| `M_AF_FX_Mesh` (optional) | VFX meshes | same instance data |
| `M_AF_Ghost` | soul echo, dodge afterimages | CPD Tint (9..12), Ghost (14), Fade (13) |
| `M_AF_LightPool` | ground pools under loot / potions | CPD Tint RGB (9..11) + alpha (12) |
| `M_AF_TargetRing` | target ring quad | CPD Tint RGB + alpha |
| (guide arrow mesh material) | `SM_FX_GuideArrow` | CPD Tint RGB (9..11), TintAmount (12) |
| `M_AF_AffixAura` | elite affix ground aura | CPD Tint RGB + alpha |
| `M_AF_BlobShadow` | blob shadows (mobile low / balanced) | CPD Fade |

## 4. Meshes

`SM_FX_Quad`: 100 x 100 cm plane centred on the origin in the XY plane, normal +Z, UV (0,0) at (-50, +50) and (1,1) at
(+50, -50) (texture up = local +Y, right = local +X). VFX quads scale X by width / 100 and Y by height / 100 and point
local Y at the texture's up (camera up for billboards, the flight direction for velocity quads, the span for beams).
Upright quads stand on their bottom edge. The guide arrow points along the mesh's forward after the manifest
`meshYawOffsetDeg` (same convention as characters).

## 5. VFX recipes

### 5.1 Document

Built-in defaults live in `Private/Vfx/AbyssVfxDefaults.cpp`; the first existing file of `Data/vfx_recipes.json`
(staged) or `Art/Export/VFX/vfx_recipes.json` is merged on top (same id replaces).

```json
{
  "schemaVersion": 1,
  "palettes": { "fire": { "core": "#FFF4C0", "mid": "#FF8A2A", "rim": "#FF3D1F", "dark": "#5A1408" } },
  "sprites": { "Smoke": { "cells": 2 }, "Bolt": { "cells": 4 } },
  "recipes": {
    "skill.fireball": { "palette": "fire", "light": { "radiusCm": 120, "alpha": 0.5, "flicker": true },
      "shake": [120, 0.004], "arcCm": 0, "layers": [ { "emitter": "glow", "at": "target", "sizeCm": 60 } ] }
  },
  "aliases": { "proj.multishot": "proj.arrow" }
}
```

Recipe fields: `palette` (named palette, `element` = from the event's damage type, `event` = derived from the event
colour), `layers[]`, `arcCm` (projectile heads), `light {radiusCm, alpha, flicker}` (fake light pool while it lives),
`shake [ms, intensity]`. Built-in palettes: fire, frost, lightning, poison, shadow, holy, steel, blood, rage, arcane,
nature, earth (element: fire -> fire, ice -> frost, lightning, poison, arcane, physical -> steel).

### 5.2 Layer fields

`emitter` picks a preset (defaults below); every other field overrides it. Ranges are a number or `[min, max]`.

| Field | Values |
| --- | --- |
| `sprite` / `mesh` | sprite name (`T_FX_<sprite>`) / mesh name (falls back to the sprite when missing) |
| `blend` | `additive` (default) / `translucent` |
| `orient` | `billboard`, `ground`, `velocity`, `upright`, `beam` |
| `variant`, `randomVariants` | sprite cell (strip of `cells`), or a random one of N |
| `at` | `origin` (`source`/`caster`), `target`, `point`, `points` (chains), `path` (along origin -> target), `camera` |
| `height` | `ground`/`feet`, `chest`, `head`, `overhead`, `hand`, or a number (cm) |
| `offsetCm` | `[forward, right, up]` in the blow frame |
| `beamTo`, `beamToHeight` | other end of beams |
| `spawnRadiusCm`, `onRing`, `spawnHeightJitterCm`, `unit` | spawn disc; `unit`: `cm`, `ring` (hit ring radius), `radius` (event radius) |
| `attach` | particles follow their anchor |
| `delayMs`, `count` (range or `"sparks"`), `countByRadius`, `rate`, `durationMs` (-1 = until stopped), `staggerMs` | emission |
| `lifeMs` | particle life |
| `speedCmS`, `dir` (`random`, `up`, `down`, `blow`, `back`, `out`, `in`, `flat`, `tangent`), `coneDeg`, `upCmS`, `gravityCmS2`, `drag`, `orbit {r0Cm, r1Cm, degS}` | motion |
| `sizeCm`, `grow`, `sizePow`, `aspect` (width / height), `stretch` | size: quads - height (width = height x aspect); meshes - drawn diameter |
| `alpha [a0, a1]`, `fadeIn`, `alphaPow`, `flicker` | alpha over life |
| `color` (`core`, `mid`, `rim`, `dark`, `event`, `white`, `#RRGGBB`), `altEvery`, `altColor`, `intensity` | colour (HDR x intensity) |
| `rotationDeg`, `spinDegS`, `alignToBlow` | rotation |
| `only` | `big` (crit / kill) or `small` |

Presets: `glow`, `flash`, `glint`, `sparks`, `streak`, `motes`, `flames`, `smoke`, `debris` (`SM_FX_Rock_A`), `ring`,
`shock`, `decal`, `slash`, `swipe`, `beam`, `gather`, `bolt`, `mesh` (values in `AbyssVfxRecipe.cpp ApplyPreset`).

### 5.3 Event bindings (recipe ids)

| Trigger | Recipe ids (first existing wins) |
| --- | --- |
| EvHit (impactBurst) | `hit.impact`; hero basic attack `hit.basic`; melee hit on the hero `hit.claw`; i-frame evade `hit.evade`; single-target elemental skill `hit.mark.fire / ice / other` |
| Monster killed (EvHit.killed) | `death.burst` (#FF4444) + `reward.gold` |
| Escort died | `death.burst` (#E67E22) |
| Hero died | `death.burst` (#CC2222) (respawn stops the hero's status loops) |
| EvLevelUp | `hero.levelup` |
| EvItemPicked / EvPotionPicked | `reward.gold` (8 coins, quality colour) / `reward.mana`, `reward.heal` |
| EvDodgeStarted / EvHeroDash / elite blink | `hero.dodge` / `hero.dash` / `teleport.blink` |
| EvTownPortal | `portal.channel` (loop) then `portal.arrive` |
| EvLootDropped | `loot.drop`; rare `loot.drop.rare`; legendary / set `loot.drop.legendary` (`loot.drop.set`) |
| EvQuestUpdate (progress from a world point) | `quest.pop` |
| EvSkillVfx | `skill.<vfxId>`, `skill.<skillId>`, `skill.default` |
| Projectile launched / flying / ended | `proj.<vfxId>.launch`; head `proj.<vfxId>`, `proj.monster_bolt` / `proj.pet_bolt`, `proj.default`; end `proj.<vfxId>.hit` / `.fizzle` (then by kind, then `proj.default.hit` / `.fizzle`) |
| Ground effect started / triggered / ended | `ground.<vfxId>` / `ground.<vfxId>.trigger` / `ground.<vfxId>.end` (else `ground.default*`) |
| Status in the snapshot mask | loop `status.<burn / freeze / poison / bleed / slow / stun>` while set |
| Anim notify `<N>` | `notify.<N>`, `npc.<N>` |
| Zone dressing (world builder) | `camp.campfire`, `camp.torch`, `exit.portal`, `exit.sealed`, `ambient.<pollen / wisps / dust_motes / sparks / dust>` |
| Props (world builder) | `prop.soul_echo`, `prop.clue`, `prop.gather`, `prop.lore` (lore colour), `prop.chest`, `prop.event` |

### 5.4 Sprites

`T_FX_<Sprite>`: square, linear alpha (or luminance for additive), white-ish so the instance colour tints it. A sprite
with `cells: N` is a horizontal strip of N equal cells, the instance Variant (custom data 4) picks the cell. Names used by
presets and defaults: Glow, Core, Spark, Streak, Ember, Flame, Smoke (2 cells), Rock (2), Ring, Shock, Scorch, Slash,
Beam, Bolt (4), LightPool, Arrow, Bubble, Claw, Coin, Comet, Crack, CrackGlow, Drop, Flake, Frost, Hex, Plus, Puddle,
Rune, ShardFrost, Skull.

## 6. Console variables

`abyss.SunScale` (0.55, sun intensity x pi), `abyss.GroundAmbient` (0.55), `abyss.ExposureBias` (0.263 = log2 1.2),
`abyss.Bloom` (0.6), `abyss.ShakeScale` (camera shake amplitude).

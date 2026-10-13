# Abyssfire Unreal Rebuild — Binding Decisions

These answer every open question raised in `Docs/spec/*.md`. They are **binding** for all code, data and art.
Guiding rule: **keep the web game's numbers and feel (balance, timings, formulas) unless they are bugs; fix bugs; take
the 3D/touch upgrade wherever the web version was limited by being 2D or web.** When a spec lists a "web quirk" with a
recommendation and it is not decided otherwise below, **follow the spec's recommendation**.

Owner of the user-level product decisions: the user (3D oblique camera, no placeholder art, portable C++ core + UE
module, UE 5.8+, macOS first then Windows/Android/iOS with touch UI, Chapter 1 fully playable first).

## P — Platform and engine (ue58-platform.md)

| ID | Decision |
|---|---|
| P1 | Pin **UE 5.8** (`EngineAssociation "5.8"`, tested against 5.8.3+ hotfixes). C++20 everywhere. |
| P2 | Desktop: deferred + TSR (FXAA on low). Mac Apple Silicon Metal SM5; Windows D3D12 SM5. |
| P3 | Mobile: **forward + MSAA 4×** (opt out of 5.8 multi-pass deferred). Android **Vulkan only** (min API 26, target 36); iOS 17+. |
| P4 | UI: **Slate in C++**, ScaleToFit design 1280×720, safe zones. No UMG assets. Touch targets: the **touch HUD controls** (joystick, skill/potion/dodge/portal buttons, Talk/Use) are ≥ 44 pt; panels keep the 720-unit layout with the touch scale and ≥ 32 pt hit areas. A dedicated touch re-layout of panels is a follow-up. |
| P5 | Animation: no Animation Blueprint. Native `UAbyssAnimInstance` + custom proxy (crossfade + additive), timing owned by the core (spec §7 Option A). |
| P6 | VFX: code-driven pooled particles on ISM components + generated meshes/materials; no Niagara assets in milestone 1. |
| P7 | Outline: inverted hull **baked by Blender** into every character/prop mesh (second material slot). |
| P8 | Data: JSON in `unreal/Data`, staged UFS via `RuntimeDependencies`; UE reads bytes, the core parses. |
| P9 | One persistent map `L_Main`; zones built at runtime from map data. |
| P10 | Shadows: CSM (2 cascades desktop, 1 cascade high-tier mobile); blob shadows on low/mid mobile and for small props everywhere. |
| P11 | Substrate off. Toon look = Unlit + custom lighting via a Material Parameter Collection (spec §6.4). Revisit Substrate Toon later. |
| P12 | Bundle ids: `com.feuvan.abyssfire` (iOS/Mac/Android). Apple team id and Play account are filled in by the user (left as config TODO with a clear message). |
| P13 | Gamepad: movement, attack, dodge, skills and pause menu in milestone 1; full UI focus navigation later. |

## S — Simulation core (all specs)

| ID | Decision |
|---|---|
| S1 | **Fixed 60 Hz sim step** in the core, ms as `double`, one `SimClock` that never resets. The UE layer accumulates real time and steps the core 0–4 times per frame (spiral-of-death clamp). Rendering interpolates. |
| S2 | The sim clock **pauses** while the world is frozen (story cinematic, modal pickers, pause menu, dialogue with a quest card). Story/UI animations run on real time. Pending strikes resume after the freeze. (classes-stats-skills §19.1 D13 is binding.) |
| S3 | Seeded, **deterministic** RNG: own PRNG (xoshiro128** seeded by SplitMix), one stream per domain (combat, loot, ai, world, events). Tests inject seeds. Saves store stream states. |
| S4 | Units: **1 tile = 100 uu (1 m)**. Tile (col,row) → world X = col·100, Y = row·100, Z from terrain. Pixel-derived web constants convert with the specs' factors (36 px/tile for projectile & arrow timing, 45 px/tile for VFX size) — one constant table in the core, never duplicated. |
| S5 | **Hero ground speed is uniform**: `moveSpeed / 36` tiles/s (3.333 at 120) for click, hold, keyboard and stick. Keyboard/stick get a 90 ms ramp to full speed and 60 ms stop; click-move starts instantly. |
| S6 | Elite-kill slow motion = global time dilation that scales **both** sim dt and visuals (true slow-mo). |
| S7 | Clock domains in combat: pending monster strikes are core timers on the sim clock; they abort only if the attacker dies or is stunned, not because of a cinematic. |

## C — Combat, classes and skills (classes-stats-skills.md, combat-feel.md)

| ID | Decision |
|---|---|
| C1 | Apply **all** "recommended fix" quirks in classes §19 and combat §21 (mana shield drains mana, buffs/debuffs expire, passives not castable and not on the hotbar, dodged skill hits apply nothing, range checks on targeted buffs, Teleport range clamp to 8 tiles, Shadow Step crit buff works, DoT ticks keep the remainder, gear-inclusive max HP/MP on level-up heal and load, death_mark without target does nothing, auto-battle skips unusable skills). |
| C2 | statGrowth stays **unused** (parity): 5 free stat points + 1 skill point per level. |
| C3 | Hotbar: **explicit, player-editable 6 slots** saved in the save (`hotbar`); new skills auto-fill the first empty slot. |
| C4 | 3D upgrades that keep the numbers: **Charge dashes** to the target (stops at melee range, 0.25 s); **Fire Wall, Arrow Rain and Traps are persistent ground effects** with the same total damage spread over their tick count; **Chain Lightning** jumps target-to-target with 55 ms stagger; Multishot fans in a 50° cone from the hero. |
| C5 | Hero freeze/stun/slow **do** affect movement and attacks (needed for later chapters). |
| C6 | Explicit per-skill / per-monster data fields replace substring rules: `statusRule`, `scorch`, `impactColor`, `onHitStatus`. Owner: classes-stats-skills schema (skills) and monsters-ai schema (monsters). |
| C7 | Click-to-attack stops the approach at attack range. |
| C8 | Mobile: dodge goes in the joystick direction, else the facing direction (never screen-right). Teleport destination: joystick × 6 tiles, else the locked target, else 6 tiles ahead. |
| C9 | Camera shake: same peak amplitude as the web at render scale 1, mapped to the 3D camera, with UE-style exponential decay. |
| C10 | Weapon `baseDamage`/`attackSpeed` and shield `defense` stay **inert in milestone 1** (balance parity), but the tooltip shows them dimmed with "(未生效)". Revisit with a balance pass after playtesting. |
| C11 | Legendary `specialEffect` keys that combat already reads are applied as item stats (killHealPercent, elementalDamagePercent, doubleShot, ignoreDefense, dodgeCounter, damageReduction, cooldownReduction); others are designed later. |
| C12 | Dead-hero rules: Esc and menu actions are blocked during the death window; saving with hp ≤ 0 saves as respawned at the camp. |
| C13 | Floating combat text, kill rewards and quest progress popups (combat-feel §12, quests §5.8): the web draws them as world objects under `ZONE_CAMERA_ZOOM` 1.8, so their px **offsets, rises, drifts, sinks and stack rows are web world px × the live camera scale** (`SAbyssWorldLayer::WebPx` = projected tile diagonal at the hero / 64, ≈ 1.77 at the default framing, follows the zoom); font sizes stay screen px. Rewards fade with the web's `Power2` (= Cubic.Out on y and alpha together), not a quad rise with a quad-in fade, which kept them nearly opaque 3× longer. Port addition (placement, `SAbyssWorldLayer::PlaceFloatGroup`): the label floats one event batch spawns on one anchor, split in arrival order into groups that hold each kind once (a kill's +EXP / +G / +Embers; also status / pickup lines; two kills on one spot in one frame make two groups, the second placed above the first), keep their web layout among themselves and are lifted together, at spawn, until — sampled every 40 ms over the next 1.6 s with the last paint's camera — their ink boxes (measured width × 0.85 em + outline, plus a line height per extra line of a multi-line text; 2 px clearance) clear every other float that is still at least 0.2 opaque, including a hit number still rising through the spot (the killing blow's −30 → −58 px; ≈ 24 web px lift). Labels rise on decelerating curves and start at their fastest, so a group placed clear above an older one never crosses it; lift ≤ 96 web px. Hit numbers keep the spec's position and stacking and are painted after (above) the label floats, so a later number rising through a reward stays readable. (First playtest: unscaled offsets put +EXP on the loot labels; second playtest: cell-keyed reward rows let kills 0.2 tile or > 0.6 s apart overlap.) *Third Mac playtest:* label floats (rewards, status / pickup lines) are regular weight with a 1 px outline (a centred 2 px stroke is 1 px outside the glyph; bold with a 2 px Slate outline closed the counters of a 13 px "+18 EXP"); hit numbers stay bold. Painter text fades its outline with the fill (`MakeText` draws the font outline untinted, which left a black "+18 EXP" behind a faded fill), and a float under alpha .7 cuts the fill out of its outline (`bSeparateFillAlpha`) so the outline does not darken the fading glyph. |

## M — Monsters and AI (monsters-ai.md)

| ID | Decision |
|---|---|
| M1 | Leash: new **returning** state (walk home at normal speed, heal 0.6 × maxHp/s, ignore the hero until home). |
| M2 | A hit from beyond aggro range **provokes** (5 s forced chase that ignores the 1.5× aggro drop). |
| M3 | Respawn jitters around the original anchor (no drift). |
| M4 | Ambush, rescue, defend-wave and hunt monsters **never respawn**. |
| M5 | Goblin Shaman (and miniboss_goblin_shaman) become **ranged casters** with attackRange 3.0 (spirit-fire bolt, projectile timing per spec). |
| M6 | Movement: monsters chase with **A* on the tile grid when line-of-walk is blocked**, plus separation steering and wall sliding; no capsule collision against the hero/NPCs (actors pass through each other, as in the web). |
| M7 | The zone mini-boss spawns **once per zone visit**; `goblin_chief` spawns once per visit and not after its quest is turned in (except as a farmable boss after the chapter is complete, once per visit). |
| M8 | Mini-boss lines and monster name labels move to i18n keys. |
| M9 | `lootTable`/`bossSkills` stay inert data in milestone 1. |
| M10 | Approve the robustness fixes: 4000 ms patrol timeout, 8 placement tries then the anchor. |
| M11 | Monster yaw turn rate 720°/s (render-only). |

## I — Items, loot, inventory (loot-items-inventory.md)

| ID | Decision |
|---|---|
| I1 | Gear magicFind and gear lck **count** toward loot luck, with the web's x0.5/x0.3 coefficients. |
| I2 | Every item drops **identified** (web behaviour); no ID scrolls in milestone 1. |
| I3 | Enforce `levelReq` on equip; no class weapon restrictions. |
| I4 | **Two potion quick slots** (HP, MP) on desktop (keys Q/E) and touch HUD; antidote cleanses poison; the TP scroll item is removed (the portal is free). |
| I5 | Keep the **100-entry list bag**, shown as a grid of cells with sort/filter. |
| I6 | Equipped **weapon and offhand meshes show on the hero** (weapon type → mesh via sockets); armour does not change the mesh in milestone 1. |
| I7 | Add a **stash keeper to the Chapter 1 camp**. |
| I8 | No web-save import. Keep the web's ItemInstance field names anyway (save JSON readability). |
| I9 | Sell prices scale by quality (normal ×1, magic ×1.5, rare ×2.5, legendary ×4, set ×4). |
| I10 | Apply recommended fixes Q4, Q6, Q7, Q13, Q15, Q18, Q19, Q22 (full-bag handling, scrollable lists, ring slot, auto-pickup on arrival). |
| I11 | Dungeon-exclusive legendaries/sets stay out of overworld drops (decided again with the Labyrinth milestone). |

## Q — Quests, story, NPCs (quests-story-ch1.md)

| ID | Decision |
|---|---|
| Q1 | Dialogue-choice rewards are **one-time** per (npcId, nodeId, choiceIndex), saved. |
| Q2 | Fix achievement counting (one count per kill; distinct zones for ach_explore_all). |
| Q3 | **Chapter 1 ships the ley-beast slice**: `pet_sprite` companion (follow, ranged arcane attack, heal, bond/level/evolution data) and the pets panel restricted to owned beasts. Embers are earned and saved but the Ember Tower, its UI and its unlock log lines are hidden until the tower milestone. The hearthstone is not placed in milestone 1. |
| Q4 | Fix `q_find_goblin_chief` quest area to centre on the chief's spawn. |
| Q5 | Hidden-area rewards persist across visits; a chest item overflows to the stash when the bag is full. |
| Q6 | NPC interaction: tap/click → walk to the NPC → talk on arrival; touch also gets a context **Talk/Use** button when within 2.5 tiles. |
| Q7 | Story beats are marked seen when they **finish** (a skipped beat counts as finished). |
| Q8 | Escort destination data stays as is (it is a gameplay point, not the camp); the label text is corrected to not claim it is the southern camp. |

## W — World, navigation, camera (world-map-nav.md)

| ID | Decision |
|---|---|
| W1 | Camera: fixed yaw 45°, **pitch −50°**, perspective FOV 35°, default distance framing ≈ 16 × 12 tiles of ground on 16:9; mouse wheel / pinch zoom between 0.75× and 1.25× of default. Smooth follow with 0.12 s lag. *Confirmed by the user after the second Mac playtest:* yaw 45° stays. The map therefore reads as a 180° rotation of the web's cartToIso view (col+1 runs up-left, the minimap's north points down-left); gameplay is identical, and the terrain motifs, key light and art reviews are authored for this yaw. |
| W2 | Fog of war: none in the 3D view; the **minimap and world map show explored fog** (FogOfWarCore). |
| W3 | Town portal: 1.5 s channel, **cancelled by movement input, damage ≥ 10 % max HP, or death**, goes to the **nearest camp** of the zone; on touch it is a HUD button. |
| W4 | Minimap **rotated to the camera yaw** (north indicator shown). |
| W5 | Tall decorations **block** walking (trees, boulders, tents, walls, wells, statues): the decoration footprint is baked into the walkability grid; small decor (flowers, grass, mushrooms) does not. Golden pathfinding tests are re-baselined. |
| W6 | Random events: reset the movement counter after every roll (designed 3–8 per 5 minutes). |
| W7 | The exit to twilight_forest shows a **sealed gate** with a "第二章即将开放" message in milestone 1. |
| W8 | Walk-then-act for NPCs, exits, loot, chests and lore. Exits fire only on arrival, and only after the hero has been > √6 tiles away from that exit since entering the zone (bounce fix). |
| W9 | Rescue/ambush monsters never respawn (same as M4). |
| W10 | The map generator is ported **bit-exact** (same seed → same grid) so the specs' golden layouts hold; 3D dressing is layered on top deterministically. |
| W11 | **Camp layout v2** (remake departure from world-map-nav §3.4 prop offsets / blocking and quests-story §6.2 NPC slots; the web drew a tent sprite behind the NPC sprite on the same tile, which in 3D put NPCs inside the canvas — playtest: "Z-index is not correct for the camp"). One core table, `abyss/world/CampLayout.h`, places every camp prop at a sub-tile position with an explicit facing (tents, well, barrels and crates turn their front to the fire; the rest face the camera) and holds the six NPC slots; MapGen `CampProps` / `ApplyCampBlockers` / `CollectAnchors`, `ZoneRuntime::EnterZone` and `AAbyssZoneActor::BuildCamps` all read it. **Three tents** set back into the far / side corners (screen top, left, right for the W1 camera), doors to the fire; **NPC slots 0–2 stand on the door axes 0.73–0.76 m in front of the door plane** (offsets (1.875, 2.375), (1.375, −1.375), (−1.875, 2.375)) and idle facing out of the door (the other slots face the fire; the hero within 3 tiles still turns them); slot 3 (−2.75, −0.75) beside a crate; spare slots 4–5 (2.25, 1.0), (−2.375, −2.25). No NPC stands inside or behind a prop for the W1 camera (world_test checks it against `world_constants` yaw / pitch), and no slot's click-pick disc (`npcPickRadiusSq` 3.24) reaches the campfire or the gate / open-side lane centre lines (a click on the fire must walk there, not open the shop). The web's 4th tent is dropped (no 4th full-size tent fits inside the palisade facing the fire). **Blocking:** every blocking prop (3 tents, the well, 2 barrels, 2 crates) blocks its anchor tile plus every tile whose centre lies within its body (tent canvas 2.5 × 1.96 m; well / crate / barrel = manifest bounds) grown by 0.3 m, so a click-path hero, pet or follower standing on any walkable tile centre keeps ≥ 0.3 m from every prop: 33 blocked tiles per camp. The props are placed so that no walkable camp tile is cut off from the fire (world_test asserts it on all 12 camps: a sealed pocket was a dead click target and trapped a hero that corner-cut or teleported in) and a click on any prop tile resolves to a reachable tile; tent 2 and the barrel behind it fill the palisade corner, so a click there walks to the nearest reachable tile, which can be just outside the palisade. The 0.3 m margin holds for path movement (tile centres); keyboard / stick steering only tests the rounded tile of the hero centre (Locomotion), so it can still graze a canvas edge by up to half a tile. Decorations on a camp prop's tile are dropped (ember_tower's hand-placed bush and grass). The MapGen stamp, gate, safe zones and the campfire are unchanged. Tuned for W1 (yaw 45°): re-check if the yaw changes. |

## U — Save, flow, UI, input (save-ui-input.md)

| ID | Decision |
|---|---|
| U1 | **3 character slots**, each autosaving (zone entry, quest turn-in, level-up, every 60 s, on app pause/background), confirm before overwriting/deleting. |
| U2 | Save format **v4** (UE-only fields such as `hotbar`, `potionSlots`, `playTimeMs`, `rng`, `dialogueOnce`), keeping v3 field names otherwise. Saves at `Saved/SaveGames/abyssfire_slot{N}.json` via UE's platform save dir. |
| U3 | JSON in the core: **RapidJSON** (header-only, MIT) vendored under the core's ThirdParty with `RAPIDJSON_NAMESPACE abyss_rapidjson`, exceptions off, own float parser per spec §3.3. |
| U4 | Esc / Back: closes the top panel; with nothing open it opens the **system menu** (Resume / Settings / Save & return to menu / Quit). |
| U5 | Cloud saves out of scope for milestone 1. |
| U6 | Milestone-1 build hides later-milestone entries and keys (Soundtrack, homestead H, mercenary U). Pets panel P is shown once a beast is owned. |
| U7 | Modal panels (dialogue, quest card, shop, stash, mini-boss, lore, confirms, system menu) block gameplay input; HUD-side panels (inventory, character, skills, quest log, map) do not pause the sim on desktop but **do pause on touch**. |
| U8 | Minimap rotated (W4) with explored fog; world map shows all 5 zones, locked ones greyed with their chapter name. |
| U9 | One **Settings** panel on all platforms: master/music/SFX volume, language (zh-CN / en), graphics quality (low/mid/high/auto), touch control scale and opacity, camera shake on/off, damage numbers on/off. Touch gets entries for achievements and settings. |
| U10 | Portal channel cancelled by movement input and death (W3). |
| U11 | Skill slot without icon art (`T_UI_SkillIcon_<id>` missing): a 36 px tile in the web icon backdrop hue (damage-type colour; physical skills by tree) with a monogram (first glyph of a CJK name, else 2 letters) up-left of centre; while cooling the monogram becomes a 10 px top-left corner tag, so the centred seconds (§6.6) never print over it. Touch buttons (5.7.3) use the same monogram; any label-only touch button moves its label to a small tag under the centred countdown while cooling. |

## A — Audio (audio.md)

| ID | Decision |
|---|---|
| A1 | **Offline render** the web synth (SFX + procedural music) with a Python/numpy port here; commit **OGG Vorbis q6** renders (no raw WAV, no LFS) plus the renderer; UE imports them as SoundWaves. |
| A2 | Combat-music off-debounce is a true debounce (cancelled when fighting resumes). The explore track resumes from its position after a fight. |
| A3 | Loudness-normalise all music to −16 LUFS and SFX consistently; default volumes music 0.6, SFX 0.8. |
| A4 | Mild 3D panning (listener on the hero, 25 % spread) for world SFX; UI SFX stay 2D. |
| A5 | Chapter 1 boss music: the procedural `boss_ch1` score. Boss-victory hold 8000 ms. |
| A6 | Use `hit_heavy` for heavy/crit/kill hits and `player_hurt` on damage taken. |
| A7 | Milestone 1 adds a plains **ambience bed**, hero **footsteps** (grass/dirt/stone) and basic monster vocalisations (aggro, hurt, death per family), synthesised by the same renderer. |
| A8 | The GPL "Desert Battle Theme" must be replaced before chapter 4 ships (not in milestone 1). |

## R — Art (art-inventory-ch1.md)

| ID | Decision |
|---|---|
| R1 | Walls and palisades 1.4 m; camera pitch −50° (W1) keeps the hero visible behind them; occluding props fade when they cover the hero. *Revised after the third Mac playtest (pending confirmation)* — the fixed 4 × 4 Bayer fade read as a crosshatch over trees and rocks, and props faded with nobody behind them. **Technique per anti-aliasing:** under TSR / TAA (desktop balanced / high, and any temporal method) the masked toon / ink / FX-mesh fade dithers with the Bayer threshold shifted every jitter sample (`View.TemporalAAParams`; each sample keeps exactly α × 16 cells, each pixel ≈ α of a sequence), which the AA resolves to a smooth α; body and ink hull keep the same pixels. Without temporal AA (desktop low FXAA, mobile MSAA 4× / 2×; `abyss.FadeGhost`, 2 forces it on every tier for testing) a faded dressing instance is hidden in its ISM and drawn by a **translucent copy** in a parallel ISM (`M_AF_Toon_Fade` / `M_AF_Toon_Foliage_Fade`: same toon colour and wind, α = 1 − fade; ink hull hidden, sort priority 0 like the slime shell, after water −2 and before VFX 1 / 2); overlapping canopy lobes blend in triangle order, accepted while the copy is at most ~.8 opaque: it takes over from fade .3 and hands back below .2, so the first ~3 frames of a fade-in and the last ~0.25 s of a fade-out keep the masked instance and its outline with a sparse static dither (≤ 5 of 16 pixels dropped). Character / prop death and expiry fades and FX meshes keep the dither there (static, brief). The shadow pass ignores the dressing's occlusion fade (a faded tree keeps its shadow; the ghost casts none). **Test:** the hero (feet + 30 cm, chest, head) and *living* monsters within 6 tiles (chest); walls / stakes / gates: the target's ray toward the camera through their cylinder (radius + 25 cm); other tall dressing: the target stands behind the prop's centre and its ray enters the prop's bounds ellipsoid (horizontal semi-axis = bounds radius, vertical = half the height, centred at half height): canopies are widest in their upper half over a thin trunk, so a half-spheroid on the ground left goblins half hidden by a crown unfaded; tents, boxier, fade a little early. Low dressing fades only for ground loot (unchanged). |
| R2 | Heights follow art-inventory-ch1.md §1.6 (web proportions, ≈ 4 heads): warrior 1.76 m to the helm crown, mage 1.71 m, rogue 1.62 m; NPCs normalised to 1.70–1.76 m so the hero is never shorter than a townsperson; goblin 0.92 m, chief 1.17 m, shaman 0.95 m, hunt leaders 1.15 m, slime 0.43 × 0.88 m. |
| R3 | Outline: baked inverted hull (P7); world-space width 1.2–1.8 cm by asset size, darkened local colour. |
| R4 | Weapons swap via `weapon_r`/`weapon_l` sockets (I6): one mesh per weapon base type family. |
| R5 | Hero animations: shared idle/run/attack01-03/cast01-02/hurt/death/dodge per class **plus signature montages** for whirlwind, charge, multishot and blizzard/meteor casts; others reuse cast/attack with VFX variation. |
| R6 | The broken altar mesh is canonical for the story decor (the dry well is a separate prop). |
| R7 | Approve the new goblin-camp dressing and per-clue props (§6). |
| R8 | Approve small code-driven status visuals (burn embers, poison bubbles, frost crystals, stun stars) in addition to tints. |
| R9 | Run cycle lengths re-derived from S5 (3.333 m/s): stride matches ground speed, no foot sliding. |
| R10 | Item and skill icons are **rendered from Blender** (3D models / VFX shapes) at 256×256 with the UI frame applied in Slate. |
| R11 | Portraits are offline Blender renders (512×512, bust, three-quarter). |
| R12 | Slime: toon shell with a fresnel (jelly) rim over opaque inner meshes (core, bone, bubbles, eyes) and an opaque ink hull. *Revised after the second Mac playtest (pending confirmation):* the shell is **alpha-blended translucency** (`M_AF_Toon_Slime`, `BodyOpacity` .62 × fade, back faces culled) instead of the masked 4 × 4 Bayer dither, which read as a screen door on every tier (no temporal AA varies a fixed pattern: MSAA on mobile, FXAA on desktop low, TSR); the blend is what the slime previews render. (The other masked fades now shift the pattern per TSR / TAA sample, R1; the shell stays blended on every tier.) Opaque sections keep depth and the hull still casts the shadow; sort priorities: blob shadows −10, water −2, shell 0, VFX 1 / 2. |
| R13 | Generic weapons (I6 / R4) on another class's clips use per-class grips (`SM_Weapon_*` manifest `grips`). *Pending art-direction sign-off (weapons-loot review, fourth look):* the mage's generic **shield is slung on her back** (`spine_03`, `slung`) — her Walk / Run swing the left hand across the robe and no forearm grip clears it (12 of 60 Walk frames at any grip), so from the se 3/4 view she shows most the shield is hidden behind her; the warrior holds a generic **bow in his right hand** (`weapon_r`, like the mage) so every strike carries it and its D faces the camera (on `weapon_l` it dangled edge-on at the hip while an empty fist struck). |

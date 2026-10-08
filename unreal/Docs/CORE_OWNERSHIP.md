# AbyssCore — area ownership

Written by the foundation agent; binding for parallel implementation. Shared files (lead contract) may only get small additive edits, coordinated through this list.

## API map

Root: /home/user/abyssfire/unreal (nothing committed; src/ and Art/ untouched). Public headers live in Source/AbyssCore/Public/abyss/<dir>/; stubs and implementations live in Source/AbyssCore/Private/<dir>/ (one .cpp per header). Every core .cpp includes "abyss/base/Platform.h" first. Namespace is flat `abyss`. Core sources are ASCII-only.

BASE (implemented and tested)
- Platform.h: FP-contract pragmas, kCoreApiVersion.
- Assert.h: ABYSS_ASSERT, ABYSS_UNIMPLEMENTED (logs once per call site), SetAssertHandler.
- Log.h: LogLevel, SetLogSink, LogInfo/Warning/Error.
- Types.h: EntityId, kHeroEntityId, EntityKind, EntityIdAllocator, Vec2, TilePos, TileCircle, RoundToTile, IdIndex.
- Enums.h: ClassId, Difficulty, DamageType, ItemQuality, ItemType, EquipSlot, StatusType, HitWeight, AnimRig, MapTheme, AutoLootMode, LocaleId, Faction. ABYSS_ENUM_STRINGS gives EnumName/ParseEnum/EnumCount.
- Stats.h: Stat (45 EquipStats keys + extras), PrimaryStats, EquipStats, StatBag.
- Math.h: JsRound*, Clamp, DistSq, JsHypot, Imul.
- Units.h: S4/S5 constants and conversions.
- Rng.h: SplitMix64, Rng (xoshiro128**, scriptable), RngStream {Combat, Loot, Ai, World, Events, Quests, Pets}, RngSet with save states.
- SimClock.h: kSimStepMs, FreezeReason bit flags, step accumulator, S6 dilation.
- Timers.h: TimerOwner {Sim, Hero, Combat, Projectiles, Monsters, Items, Quests, Story, Pets, World}; TimerQueue (Schedule/Cancel/CancelIf/PopDue).
- Json.h: JsonValue keeps document order; ParseJson, WriteJson, JsonWriter, own number parser and formatter.
- I18n.h: LocText, MakeLoc, I18nArg with isKey, KeyArg, I18n::T.
- StrUtil.h: string helpers.

DATA (implemented and tested; loads all 50 Data/*.json, 0 errors)
- DataStore.h: LoadAll, Finalize, typed table accessors, Find* helpers.
- One header per table group: SkillData, ClassData, CombatData, ItemData, MonsterData, DialogueData, NpcData, QuestData, StoryData, LoreData, PetData (with homestead), MapData (with world constants, map_gen, random events, moods), AudioData, AssetManifest, UiData.

SIM CONTRACT (lead; facade and loop implemented)
- GameSim.h: GameSim {Create, NewGame, LoadGame, SaveGame, CanSave, BuildSave, Step, Frame, AdvanceRealTime, Submit, Events, ClearEvents, View, WorldFrozen, NowMs, Context}.
- Commands.h: Command = variant of 53 Cmd* structs (move, pointer, combat, world, items, progression, pets, quests, dialogue, story, panels, lifecycle, debug); IsHeroGameplayCommand.
- Events.h: Event = variant of about 68 Ev* structs; EventSink.
- Snapshot.h: Snapshot with Hero/Monster/Npc/Pet/GroundItem/Projectile/GroundEffect/WorldMarker/Zone/BossBar views, prevPos plus interpolationAlpha, and live pointers (Inventory, QuestSystem, Dialogue, Story, Pets...).
- SimContext.h: SimContext (shared services by reference, SimSystems pointers, merged EquipStats `equip`), SimConfig, RNG stream placement rules.
- GameplayBus.h: typed synchronous bus. Messages: MonsterKilledMsg, Hero*Msg, Quest*Msg, ZoneEntered/Exited, SaveRequestMsg, EquipStatsDirtyMsg, ...
- SimTypes.h: PanelId, LogType, AnimAction, InteractKind, StoryBeatKind, PotionSlot, ...
- Session.h: SessionState, PanelState, AbyssRecord, difficulty ladder functions.
- Private/sim: SimImpl.h, GameSim.cpp (D13 step loop, freeze F1/F2, timer dispatch by owner, update order from classes spec section 16), SimWiring.cpp (bus subscriptions, kill pipeline order from monsters spec section 11), SimCommands.cpp, SimZone.cpp (zone entry/exit order from world spec 9.1; equip-stat merge), SimSave.cpp (save 3.4/3.5 order), SimSnapshot.cpp, Session.cpp.

HERO
- Hero.h: Hero (level/exp/gold, stats, derived, resources, HeroLife, position, SkillBook, BuffList, Spirit, ToSave/FromSave).
- Skills.h: scaling functions, SkillBook (levels, invest gates, C3 hotbar, cooldowns).
- Spirit.h: Spirit.
- Buffs.h: BuffList (implemented).

COMBAT
- Damage.h: Combatant, CalculateDamage, proc checks.
- StatusEffects.h: StatusEffectSystem, RollStatusRule.
- HitFeedback.h: ClassifyHit, ComputeAttackTiming/ComputeCastTiming (manifest first, anim_timing fallback), shake helpers, ShakeThrottle.
- CombatInput.h: InputBuffer, DodgeController, ComputeDodgeDestination, CycleTarget.
- SkillTargeting.h: AoE anchor, line/cone/radius targets, teleport/shadow-step/charge destinations.
- Projectiles.h: ProjectileSystem (projectiles and C4 ground effects), travel-time functions.
- Combat.h: CombatSystem (skill requests, dodge, targeting, progression commands, per-step ticks, DamageMonster/DamageHero, KillHero, ResolvePendingDeath, OnMonsterKilled).
- SoulEcho.h: ComputeDeathPenalty, SoulEchoState, SoulEchoSystem.

MONSTERS
- Monster.h: MonsterInstance, MonsterState (with Returning), MonsterRole.
- MonsterDefs.h: difficulty scaling, hunt definitions, base stats, CombineAffixes/ApplyEliteAffixes.
- EliteAffixes.h: RollEliteAffixes, on-hit, teleport, curse.
- MonsterAI.h: UpdateMonsterAI, MonsterMoveToward, ProvokeMonster.
- SpatialGrid.h: implemented in the header.
- Hunts.h: IsHuntDue, HuntsToSpawn, FindWalkableNear.
- MonsterSystem.h: spawn, mini-boss, hunts, ambush, TickAI, ApplyDamage (publishes MonsterKilledMsg exactly once), Heal, queries, save.

ITEMS
- Item.h: ItemInstance, ComputeItemStats, ItemUidGenerator.
- LootGen.h: CreateItem, GenerateLoot, RollQuality, quest reward choices.
- Inventory.h: Inventory (pure bag/equipment/stash/buyback) and InventorySystem (runtime).
- Crafting.h, ItemCompare.h, Shop.h (ShopSystem), GroundLoot.h (GroundLootSystem).

QUESTS, STORY, PETS
- QuestSystem.h: QuestSystem, QuestProgress, NpcMarker, OfferRule.
- QuestGuide.h: ComputeGuideTarget, ResolveGatherSpots.
- QuestWorld.h: NPC interaction, quest card, TurnIn, drops/gather/clues/escort/defend, guide.
- Dialogue.h (DialogueSystem, Q1 dialogueOnce), Achievements.h (AchievementState/System), Lore.h (LoreSystem).
- StoryDirector.h: StoryProgress, StoryDirector (queue, T15 timers, IsCinematic, real-time step player, Advance/Skip, Q7 seen-on-finish).
- PetSystem.h: pet formulas, PetSystem. PetCompanion.h: ChoosePetAction, PetCompanion. Homestead.h: ember/garden functions, HomesteadTower, HomesteadSystem.

WORLD
- Grid.h: TileType, ZoneGrid (implemented).
- MapGen.h: ParkMillerRng (implemented and tested), GenerateMap, BuildZoneGrid (placeholder open grid until the port lands), ApplyCampBlockers.
- Pathfinding.h: Pathfinder.
- Exploration.h: ExplorationGrid (implemented), FogOfWarCore, ExplorationSystem.
- Zone.h: ZoneRuntime (EnterZone implemented; pointer chain, Interact/InteractWith, town portal, exits W7/W8, transitions).
- Locomotion.h: HeroLocomotion, ScreenDirToTile.
- RandomEvents.h: RandomEventRules, RandomEventSystem.

AUDIO
- audio/Audio.h: Sfx* rule functions, MusicDirector, AudioDirector.

SAVE
- SaveData.h: SaveData v4 (SaveHero, SaveSettings, SaveHomestead, SavePets, SaveSoulEcho, SaveRng, hotbar, potionSlots, playTimeMs, dialogueOnce, hiddenRewardsClaimed, itemUidCounter).
- SaveIO.h: MigrateRaw, ParseSave, SerializeSave, FindNearestWalkablePosition (implemented and tested), item/quest/pet JSON helpers, ISaveStorage, SummarizeSave.
- Private/save: SaveSections.h plus per-area SaveHero/SaveItems/SaveQuests/SavePets/SaveIO .cpp files.

TESTS
- CoreTests/tests: one file per area (hero, combat, monsters, items, quests and story, world and audio, pets, save, sim) plus base and data.
- SimHarness.h builds a SimContext over the real data so one runtime system can be tested alone.
- CoreTests/.gitignore ignores the build-* directories.

## Areas

### lead / shared contract (coordinate in review; not one of the 5 areas)

**Files:** Source/AbyssCore/Public/abyss/base/*, Public/abyss/data/DataStore.h, Public/abyss/sim/{GameSim,Commands,Events,Snapshot,SimContext,GameplayBus,SimTypes,Session}.h, Private/base/*, Private/data/{DataStore,JsonReader,Tables}.cpp, Private/sim/*, CoreTests/{CMakeLists.txt,run.sh,tests/{main.cpp,TestUtil.h,SimHarness.h,base_test.cpp,data_test.cpp,sim_test.cpp}}, AbyssCore.Build.cs

Adding a message, event, command, snapshot field, SimSystems pointer or bus subscription means editing a shared file, so do it in one small reviewed change. Wiring order lives in Private/sim/SimWiring.cpp; the per-step order and timer dispatch live in GameSim.cpp; the zone entry/exit order lives in SimZone.cpp; save build/apply order lives in SimSave.cpp. Each area owns its data/<X>Data.h and the matching Private/data/Load<X>.cpp, so new JSON fields are added there and checked in DataStore::Finalize.

### hero+combat

**Files:** Public/abyss/hero/{Hero,Skills,Spirit,Buffs}.h, Public/abyss/combat/{Damage,StatusEffects,HitFeedback,CombatInput,SkillTargeting,Projectiles,Combat,SoulEcho}.h, the matching Private/hero/*.cpp and Private/combat/*.cpp, Private/save/SaveHero.cpp, data/{ClassData,SkillData,CombatData}.h with Private/data/{LoadClasses,LoadCombat}.cpp, CoreTests/tests/{hero_test,combat_test}.cpp

Spec: classes-stats-skills.md and combat-feel.md; follow D13 (timers via TimerOwner::Combat and ::Projectiles; F1/F2 in OnFreezeBegin). RNG: RngStream::Combat, plus Loot for kill gold. Monster swings, hero strikes, skill releases and all hit application live in CombatSystem. Monster damage always goes through MonsterSystem::ApplyDamage, which publishes MonsterKilledMsg, and CombatSystem::OnMonsterKilled is the first kill handler. KillHero, the Dying gate, ResolvePendingDeath, the respawn timer (T12) and the soul-echo penalty belong here. Implement Hero::ToSave/FromSave and the savejson player/settings/soulEcho sections. BuffList is already implemented and shared with monsters. ComputeAttackTiming must read AssetManifest contact/release times first. Stubs to fill: Spirit, Skills formulas and invest rules, CalculateDamage, StatusEffectSystem Apply/Tick (Q21 fix), CombatSystem, ProjectileSystem, SoulEcho.

### monsters

**Files:** Public/abyss/monsters/{Monster,MonsterDefs,EliteAffixes,MonsterAI,SpatialGrid,Hunts,MonsterSystem}.h, Private/monsters/{Monster,MonsterDefs,EliteAffixes,MonsterAI,Hunts,MonsterSystem}.cpp, data/MonsterData.h with Private/data/LoadMonsters.cpp, CoreTests/tests/monsters_test.cpp

Spec: monsters-ai.md and combat-feel.md 14 and 17. RNG: RngStream::Ai (Combat for elite on-hit rolls). Respawn uses TimerOwner::Monsters (T9). ApplyDamage is the only way a monster dies and must publish exactly one MonsterKilledMsg with every field filled (expReward spawn-scaled, affixLootBonus, role, killer/source). MonsterSystem::OnMonsterKilled is the last kill handler (log plus M4/M7/W9 respawn decision). Provide the queries CombatSystem, PetCompanion and QuestWorld rely on (QueryAlive, NearestAlive, NearestAggro, MonsterAtTile, NearestAliveOfDef, Candidates). Spawn order: SpawnZonePopulation, then SpawnMiniBoss, then SpawnDueHunts(false). SpatialGrid is complete and tested. M1 Returning leash and M6 A* need world's Pathfinder through MonsterWorld.findPath. The mini-boss pre-fight dialogue (EvMiniBossDialogue, miniBossDialogueSeen) belongs here.

### items

**Files:** Public/abyss/items/{Item,LootGen,Inventory,Crafting,ItemCompare,Shop,GroundLoot}.h, Private/items/*.cpp, Private/save/SaveItems.cpp, data/ItemData.h with Private/data/LoadItems.cpp, CoreTests/tests/items_test.cpp

Spec: loot-items-inventory.md; decisions I1-I11. RNG: RngStream::Loot. Ground-loot despawn uses TimerOwner::Items (T10). Inventory is the pure container; InventorySystem publishes EquipStatsDirtyMsg on every equipment change and supplies GearStats() for the merged EquipStats. GroundLootSystem::OnMonsterKilled does the ley fruit, generateLoot and potion pickups. Grant() is used by quest, dialogue and hidden-area rewards and overflows to the stash. ShopSystem is opened by QuestWorld NPC routing. Implement WriteItemJson/ReadItemJson with web field names (I8), WriteSave/ReadSave (identified = true) and potion slots (I4). ItemUidGenerator and ComputeItemStats are already implemented; CraftGoldUnit is implemented and tested.

### quests+story+pets

**Files:** Public/abyss/quests/{QuestSystem,QuestGuide,QuestWorld,Dialogue,Achievements,Lore}.h, Public/abyss/story/StoryDirector.h, Public/abyss/pets/{PetSystem,PetCompanion,Homestead}.h, the matching Private/quests|story|pets/*.cpp, Private/save/{SaveQuests,SavePets}.cpp, data/{QuestData,DialogueData,NpcData,StoryData,LoreData,PetData}.h with Private/data/{LoadQuests,LoadPets,LoadMisc}.cpp, CoreTests/tests/{quests_test,pets_test}.cpp

Spec: quests-story-ch1.md (including section 18 pets) and the save-ui-input.md 3.3 normalisations. RNG: RngStream::Quests and RngStream::Pets. Timers: TimerOwner::Quests, ::Story (T15 beat delays on the sim clock) and ::Pets. QuestSystem and PetSystem are pure; QuestWorld follows the turn-in order in quests spec 4.1, where the QuestTurnedInMsg listeners (story, homestead) run before exp and gold are paid. StoryDirector owns IsCinematic(), which GameSim reads for the freeze, and the real-time step player (AdvanceRealTime, Advance, Skip). Mark beats seen when they finish (Q7) and grant pets through StoryBeatFinishedMsg. Achievements publish AchievementUnlockedMsg so equip stats are rebuilt (Q13). Lore owns the hidden-area records and the Q5 claimed rewards. PetCompanion deals damage through CombatSystem::DamageMonster(KillSource::Pet) and fires bolts through ProjectileSystem. Q3 slice: only pet_sprite matters in Chapter 1.

### world

**Files:** Public/abyss/world/{Grid,MapGen,Pathfinding,Exploration,Zone,Locomotion,RandomEvents}.h, Public/abyss/audio/Audio.h, Public/abyss/save/{SaveData,SaveIO}.h, Private/world/*.cpp, Private/audio/Audio.cpp, Private/save/{SaveIO.cpp,SaveSections.h}, data/{MapData,AudioData,AssetManifest,UiData}.h with Private/data/LoadWorld.cpp, CoreTests/tests/{world_test,save_test}.cpp, CoreTests/golden/maps/*

Spec: world-map-nav.md, save-ui-input.md (flow, save, input), audio.md 9.8; decisions W1-W10, S5, U1-U3, A2-A6. RNG: RngStream::World and RngStream::Events. MapGen uses its own ParkMillerRng, which is implemented and tested. Timers: TimerOwner::World; random-event timer kinds must be >= kRandomEventTimerKindBase. First priority: port GenerateMap bit-exact against the golden maps and A* with the web tie-breaking, because BuildZoneGrid is a placeholder open grid until then and Pathfinder::FindPath is a stub. ZoneRuntime owns the pointer chain, interact, the W3 town portal, exits (armed/sealed) and RequestZoneChange/TakePendingTransition; GameSim runs the actual exit and entry sequences. HeroLocomotion owns movement, hold-to-move and S5 speeds, and publishes HeroMoveInputMsg. Also owns SaveIO top level (MigrateRaw v1 to v4, ParseSave, SerializeSave, key order) and AudioDirector/MusicDirector.

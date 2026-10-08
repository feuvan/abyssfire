export interface Stats {
  str: number;
  dex: number;
  vit: number;
  int: number;
  spi: number;
  lck: number;
}

/**
 * Per-level scaling config inspired by Diablo II.
 * Values are per skill level (applied on top of base).
 * D2-style: early levels give more value; diminishing returns at higher levels.
 * Tier brackets: 1-8 (full), 9-16 (75%), 17-20 (50%).
 */
export interface SkillScaling {
  damagePerLevel: number;
  manaCostPerLevel: number;
  cooldownReductionPerLevel?: number;
  aoeRadiusPerLevel?: number;
  buffValuePerLevel?: number;
  buffDurationPerLevel?: number;
}

/** Synergy: another skill that boosts this skill's damage. */
export interface SkillSynergy {
  skillId: string;
  damagePerLevel: number;
}

export interface SkillDefinition {
  id: string;
  name: string;
  nameEn: string;
  description: string;
  tree: string;
  tier: number;
  maxLevel: number;
  manaCost: number;
  cooldown: number;
  range: number;
  damageMultiplier: number;
  damageType: 'physical' | 'fire' | 'ice' | 'lightning' | 'poison' | 'arcane';
  aoe?: boolean;
  aoeRadius?: number;
  buff?: {
    stat: string;
    value: number;
    duration: number;
  };
  icon: string;
  scaling?: SkillScaling;
  synergies?: SkillSynergy[];
  /** Extra crit chance bonus (e.g. backstab +20%) */
  critBonus?: number;
  /** Stun duration in ms (e.g. war_stomp) */
  stunDuration?: number;
}

export interface ClassDefinition {
  id: string;
  name: string;
  nameEn: string;
  description: string;
  baseStats: Stats;
  statGrowth: Stats;
  skills: SkillDefinition[];
}

export type MonsterAnimCategory = 'humanoid' | 'slime' | 'beast' | 'large' | 'flying' | 'serpentine' | 'demonic';

export interface MonsterDefinition {
  id: string;
  name: string;
  level: number;
  hp: number;
  damage: number;
  defense: number;
  speed: number;
  aggroRange: number;
  attackRange: number;
  attackSpeed: number;
  expReward: number;
  goldReward: [number, number];
  spriteKey: string;
  elite?: boolean;
  /** Indicates this monster is a mini-boss (guaranteed enhanced loot). */
  isMiniBoss?: boolean;
  /** For sub-dungeon mini-bosses, indicates they belong to a sub-dungeon (rare+ loot floor). */
  isSubDungeonMiniBoss?: boolean;
  lootTable?: LootEntry[];
  bossSkills?: string[];
  animCategory?: MonsterAnimCategory;
}

export interface LootEntry {
  itemId?: string;
  quality?: ItemQuality;
  dropRate: number;
  levelRange?: [number, number];
}

export type ItemQuality = 'normal' | 'magic' | 'rare' | 'legendary' | 'set';
export type EquipSlot = 'helmet' | 'armor' | 'gloves' | 'boots' | 'weapon' | 'offhand' | 'necklace' | 'ring1' | 'ring2' | 'belt';

export interface ItemBase {
  id: string;
  name: string;
  nameEn: string;
  description: string;
  type: 'weapon' | 'armor' | 'accessory' | 'consumable' | 'gem' | 'material' | 'scroll';
  slot?: EquipSlot;
  icon: string;
  levelReq: number;
  sellPrice: number;
  stackable: boolean;
  maxStack: number;
}

export interface WeaponBase extends ItemBase {
  type: 'weapon';
  slot: 'weapon' | 'offhand';
  baseDamage: [number, number];
  attackSpeed: number;
  weaponType: 'sword' | 'axe' | 'mace' | 'dagger' | 'bow' | 'staff' | 'wand' | 'shield';
  sockets: number;
}

export interface ArmorBase extends ItemBase {
  type: 'armor';
  slot: 'helmet' | 'armor' | 'gloves' | 'boots' | 'belt';
  baseDefense: number;
  sockets: number;
}

export interface AccessoryBase extends ItemBase {
  type: 'accessory';
  slot: 'necklace' | 'ring1' | 'ring2';
}

export interface AffixDefinition {
  id: string;
  name: string;
  nameEn: string;
  type: 'prefix' | 'suffix';
  tier: number;
  stat: string;
  minValue: number;
  maxValue: number;
  levelReq: number;
  allowedSlots?: EquipSlot[];
}

export interface ItemAffix {
  affixId: string;
  name: string;
  stat: string;
  value: number;
}

export interface ItemInstance {
  uid: string;
  baseId: string;
  name: string;
  quality: ItemQuality;
  level: number;
  affixes: ItemAffix[];
  sockets: GemInstance[];
  /** Extra sockets punched by the blacksmith (CraftingSystem), on top of the base's. */
  bonusSockets?: number;
  setId?: string;
  legendaryEffect?: string;
  identified: boolean;
  quantity: number;
  // Computed stats
  stats: Partial<Record<string, number>>;
}

export interface GemInstance {
  gemId: string;
  name: string;
  stat: string;
  value: number;
  tier: number;
}

export interface SetDefinition {
  id: string;
  name: string;
  nameEn: string;
  pieces: string[];
  pieceAffixes?: Record<string, ItemAffix[]>;
  bonuses: { count: number; description: string; stats: Partial<Record<string, number>> }[];
}

export interface LegendaryDefinition {
  id: string;
  baseId: string;
  name: string;
  nameEn: string;
  fixedAffixes: ItemAffix[];
  specialEffect: string;
  specialEffectValue?: number;
  specialEffectDescription: string;
}

export interface TileData {
  walkable: boolean;
  type: 'grass' | 'dirt' | 'stone' | 'water' | 'wall' | 'camp' | 'camp_wall';
}

export type MapTheme = 'plains' | 'forest' | 'mountain' | 'desert' | 'abyss';

export interface MapData {
  id: string;
  name: string;
  cols: number;
  rows: number;
  tiles: number[][];
  collisions: boolean[][];
  spawns: { col: number; row: number; monsterId: string; count: number }[];
  camps: { col: number; row: number; npcs: string[] }[];
  playerStart: { col: number; row: number };
  exits: { col: number; row: number; targetMap: string; targetCol: number; targetRow: number }[];
  levelRange: [number, number];
  bgColor?: string;
  theme?: MapTheme;
  seed?: number;
  decorations?: { col: number; row: number; type: string }[];
  safeZoneRadius?: number;
  petSpawns?: { col: number; row: number; petId: string; chance: number }[];
  /** Hidden areas — not shown on minimap until the player enters the fog-revealed region. */
  hiddenAreas?: HiddenArea[];
  /** Sub-dungeon entrance tiles — interactable objects that load a separate sub-map. */
  subDungeonEntrances?: SubDungeonEntrance[];
  /** Environmental storytelling decorations (ruins, statues, skeletal remains, etc.). */
  storyDecorations?: StoryDecoration[];
  /** Field NPCs that are placed outside camps (standalone positions on the map). */
  fieldNpcs?: { col: number; row: number; npcId: string }[];
}

/** A hidden area within a zone, not shown on minimap until discovered by fog-of-war. */
export interface HiddenArea {
  id: string;
  /** Display name (Chinese). */
  name: string;
  /** Center tile position. */
  col: number;
  row: number;
  /** Radius in tiles defining the hidden area region (legacy, used to derive bounds). */
  radius: number;
  /** Rectangular bounds for fog-of-war discovery check (if not set, derived from col/row ± radius). */
  startCol?: number;
  startRow?: number;
  endCol?: number;
  endRow?: number;
  /** Reward spawns inside the hidden area. */
  rewards: HiddenAreaReward[];
  /** Flavour text shown when discovering the area (Chinese). */
  discoveryText: string;
}

export interface HiddenAreaReward {
  type: 'chest' | 'gold_pile' | 'rare_spawn' | 'lore';
  /** For chest/gold_pile: item quality or gold amount description. */
  value?: string;
  col: number;
  row: number;
}

/** Sub-dungeon entrance definition on the parent map. */
export interface SubDungeonEntrance {
  id: string;
  /** Display name (Chinese). */
  name: string;
  /** Entrance tile position on parent map. */
  col: number;
  row: number;
  /** ID of the sub-dungeon map data to load. */
  targetSubDungeon: string;
}

/** Complete sub-dungeon map data (a mini-map with its own spawns, mini-boss, and exit). */
export interface SubDungeonMapData {
  id: string;
  name: string;
  /** Parent zone map ID. */
  parentZone: string;
  cols: number;
  rows: number;
  theme: MapTheme;
  seed: number;
  spawns: { col: number; row: number; monsterId: string; count: number }[];
  /** The mini-boss of this sub-dungeon. */
  miniBoss: { col: number; row: number; monsterId: string };
  /** Position where the player enters the sub-dungeon. */
  playerStart: { col: number; row: number };
  /** Exit back to parent zone (returns player near the entrance). */
  exit: { col: number; row: number; returnCol: number; returnRow: number };
  levelRange: [number, number];
  bgColor?: string;
}

/** Environmental storytelling decoration placed in a zone. */
export interface StoryDecoration {
  id: string;
  /** Display name (Chinese). */
  name: string;
  /** Description shown on interaction or proximity (Chinese, ≥30 chars). */
  description: string;
  col: number;
  row: number;
  /** Visual sprite type. */
  spriteType: 'ruins' | 'skeletal_remains' | 'ancient_statue' | 'broken_altar' | 'war_banner' | 'charred_tree' | 'collapsed_pillar' | 'ritual_circle' | 'frozen_corpse' | 'sand_buried_structure';
}

export interface CampTheme {
  wallColor: string;
  wallDark: string;
  wallLight: string;
  wallTop: string;
  groundColor: string;
  bannerColor: string;
  bannerDark: string;
  torchFlame: number;
  tentColor: string;
}

export interface QuestDefinition {
  id: string;
  name: string;
  description: string;
  zone: string;
  type: 'kill' | 'collect' | 'explore' | 'talk' | 'escort' | 'defend' | 'investigate' | 'craft';
  category: 'main' | 'side';
  objectives: QuestObjective[];
  rewards: QuestReward;
  prereqQuests?: string[];
  level: number;
  questArea?: { col: number; row: number; radius: number };
  /** Escort quest: NPC to follow player to destination. */
  escortNpc?: { name: string; spriteKey: string; startCol: number; startRow: number; destCol: number; destRow: number };
  /** Defend quest: location/object to protect and enemy wave config. */
  defendTarget?: { name: string; spriteKey: string; col: number; row: number; totalWaves: number };
  /** Investigate quest: clue objects to find. */
  clues?: { id: string; name: string; col: number; row: number }[];
  /** Craft quest phase definitions: collect → craft → deliver. */
  craftPhases?: {
    materials: { itemId: string; name: string; required: number }[];
    craftNpc: string;
    deliverNpc: string;
  };
  /** Whether this quest can be re-accepted after failure. */
  reacceptable?: boolean;
  /** Named monsters this quest puts into the world (bounties, lairs, ambushes). */
  hunts?: QuestHunt[];
}

/**
 * A named quest monster. It spawns only while its quest is open, never
 * respawns, and its kill objective targets `huntId` (so ordinary kills of
 * the base monster do not count and the guide arrow points straight at it).
 */
export interface QuestHunt {
  /** Unique monster id for the kill objective; en name via `data.monster.<huntId>`. */
  huntId: string;
  /** Base monster whose rig, attacks and loot table it borrows. */
  monsterId: string;
  /** Display name (zh-CN). */
  name: string;
  col: number;
  row: number;
  /** Multipliers on the base monster (default hp ×4, damage ×1.5). */
  hpMul?: number;
  dmgMul?: number;
  /** Stay hidden until every objective listed before its kill objective is done (track it down first). */
  revealAfterPrevious?: boolean;
  /** Pack that spawns around it. */
  minions?: { monsterId: string; count: number };
}

export interface QuestObjective {
  type: 'kill' | 'collect' | 'explore' | 'talk' | 'escort' | 'defend_wave' | 'investigate_clue' | 'craft_collect' | 'craft_craft' | 'craft_deliver';
  targetId: string;
  targetName: string;
  required: number;
  current: number;
  location?: { col: number; row: number; radius: number };
  /** Where a collect objective's items come from (default: any monster in the quest zone). */
  source?: QuestItemSource;
  /** Look of the collectible (icon + world node art); see QuestItemIcons. */
  itemKind?: string;
  /** i18n key for the objective text when `targetId` alone is ambiguous (e.g. the same NPC in several deliveries). */
  labelKey?: string;
}

/**
 * Collect / craft_collect objective sources.
 * - `drop`: killing one of `monsters` has `chance` to yield one item.
 * - `gather`: `count` glowing nodes scattered on walkable ground within `area`
 *   (resolved deterministically per quest); walking up gathers one each.
 */
export type QuestItemSource =
  | { kind: 'drop'; monsters: string[]; chance: number }
  | { kind: 'gather'; area: { col: number; row: number; radius: number }; count: number };

/** Equipment reward the player picks at turn-in, generated for their class and level. */
export type QuestRewardChoice = 'weapon' | 'armor' | 'helmet' | 'gloves' | 'boots' | 'belt' | 'jewelry' | 'offhand';

export interface QuestReward {
  exp: number;
  gold: number;
  /** Fixed item base ids (consumables, gems, set pieces). */
  items?: string[];
  petReward?: string;
  /** Homestead currency (余烬) paid on turn-in. */
  embers?: number;
  /** Pick-one equipment rewards (class-appropriate, generated at turn-in). */
  choices?: QuestRewardChoice[];
  /** Quality of the generated choices (default: rare for main quests, magic for side). */
  choiceQuality?: 'magic' | 'rare' | 'legendary';
}

export interface QuestProgress {
  questId: string;
  status: 'available' | 'active' | 'completed' | 'turned_in' | 'failed';
  objectives: { current: number }[];
}

/** A single choice in a dialogue tree node. */
export interface DialogueChoice {
  text: string;
  nextNodeId: string;
  /** Quest ID to trigger (accept) when this choice is selected. */
  questTrigger?: string;
  /** Quest IDs that must be turned_in before this choice appears. */
  prereqQuests?: string[];
  /** Reward given immediately on choosing (e.g., gold, item). */
  reward?: { gold?: number; items?: string[]; exp?: number };
}

/** A single node in a dialogue tree. */
export interface DialogueNode {
  id: string;
  /** NPC text shown to the player. */
  text: string;
  /** Player-selectable choices. If empty/missing, a default "继续" or "离开" is shown. */
  choices?: DialogueChoice[];
  /** If set, auto-advance to this node after displaying text. */
  nextNodeId?: string;
  /** Flag this node as an ending node (closes dialogue). */
  isEnd?: boolean;
}

/** Complete dialogue tree for an NPC. */
export interface DialogueTree {
  /** Root node ID to start the dialogue. */
  startNodeId: string;
  /** All nodes indexed by id. */
  nodes: Record<string, DialogueNode>;
}

export interface NPCDefinition {
  id: string;
  name: string;
  type: 'blacksmith' | 'merchant' | 'quest' | 'stash';
  dialogue: string[];
  shopItems?: string[];
  quests?: string[];
  /** Branching dialogue tree (replaces linear dialogue[] when present). */
  dialogueTree?: DialogueTree;
  /** Borrow another NPC's look (e.g. the tower's allies wear their chapter selves' sprites). */
  spriteId?: string;
}

export interface AchievementDefinition {
  id: string;
  name: string;
  description: string;
  type: 'kill' | 'collect' | 'explore' | 'level' | 'quest';
  targetId?: string;
  required: number;
  reward?: { stat: string; value: number };
  title?: string;
}

/** Ember Tower state (余烬之塔) saved next to the building levels; every field optional for old saves. */
export interface HomesteadSaveExtras {
  /** Homestead currency. */
  embers?: number;
  /** Herb garden: kills counted toward the next yield, and items waiting to be harvested. */
  garden?: { progress: number; stock: Record<string, number> };
  /** Pet expedition from the caravan post (one at a time). */
  expedition?: HomesteadExpedition | null;
  /** Altar blessing (lasts until the next return to the tower or until its time runs out). */
  blessing?: HomesteadBlessing | null;
  /** Where the tower's return portal sends the hero. */
  towerReturn?: { mapId: string; col: number; row: number } | null;
}

export interface HomesteadExpedition {
  petId: string;
  /** Expedition option id (short / long). */
  optionId: string;
  kills: number;
  killsRequired: number;
  /** Play time left (ms); the expedition is back when this or the kill count runs out. */
  remainingMs: number;
}

export interface HomesteadBlessing {
  id: string;
  /** Altar level it was bought at (scales the stats). */
  level: number;
  remainingMs: number;
}

export interface HomesteadBuilding {
  id: string;
  name: string;
  description: string;
  maxLevel: number;
  costPerLevel: { gold: number; embers?: number; materials?: Record<string, number> }[];
  /** Main quest whose turn-in brings this wing's ally to the tower (absent: always open). */
  unlockQuest?: string;
  /** NPC who tends this wing in the tower. */
  allyNpc?: string;
  bonusPerLevel: { stat: string; value: number }[];
}

export interface PetDefinition {
  id: string;
  name: string;
  description: string;
  rarity: 'common' | 'rare' | 'epic';
  bonusStat: string;
  bonusValue: number;
  bonusPerLevel: number;
  maxLevel: number;
  feedItem: string;
}

/** One owned ley-beast (PetSystem). */
export interface PetSaveInstance {
  petId: string;
  level: number;
  exp: number;
  /** Evolution stage: 0, 1 (觉醒), 2 (至尊). */
  evolved: number;
  /** Bond 0–5. */
  bond: number;
  /** Progress towards the next bond level (0–99). */
  bondProgress: number;
}

export interface PetSaveData {
  owned: PetSaveInstance[];
  active: string | null;
}

export type MercenaryType = 'tank' | 'melee' | 'ranged' | 'healer' | 'mage';

export interface MercenaryDefinition {
  type: MercenaryType;
  name: string;
  description: string;
  hireCost: number;
  reviveCost: number;
  baseStats: Stats;
  statGrowth: Stats;
  baseHp: number;
  baseMana: number;
  baseDamage: number;
  baseDefense: number;
  attackRange: number;
  attackSpeed: number;
  aiRole: 'tank' | 'melee_dps' | 'ranged_dps' | 'healer' | 'aoe_mage';
  allowedWeaponTypes: string[];
  allowedArmorSlots: string[];
}

export interface MercenarySaveData {
  type: MercenaryType;
  level: number;
  exp: number;
  hp: number;
  mana: number;
  equipment: {
    weapon?: ItemInstance;
    armor?: ItemInstance;
  };
  alive: boolean;
}

export interface SpiritSaveState {
  value: number;
  resonanceRemainingMs: number;
}

/** What a death left behind (see SoulEcho). */
export interface SoulEchoData {
  mapId: string;
  col: number;
  row: number;
  gold: number;
  exp: number;
}

export interface SaveData {
  id: string;
  version: number;
  timestamp: number;
  classId: string;
  player: {
    level: number;
    exp: number;
    gold: number;
    hp: number;
    maxHp: number;
    mana: number;
    maxMana: number;
    stats: Stats;
    freeStatPoints: number;
    freeSkillPoints: number;
    skillLevels: Record<string, number>;
    /** Class-specific Spirit meter and active Resonance time (save v3+). */
    spirit?: SpiritSaveState;
    tileCol: number;
    tileRow: number;
    currentMap: string;
  };
  inventory: ItemInstance[];
  equipment: Partial<Record<EquipSlot, ItemInstance>>;
  stash: ItemInstance[];
  quests: QuestProgress[];
  exploration: Record<string, boolean[][]>;
  homestead: {
    buildings: Record<string, number>;
    /** Legacy (pre ley-beast) pet list; read for migration, new saves write `pets`. */
    pets?: { petId: string; level: number; exp: number; evolved?: number }[];
    /** Legacy active pet; see `pets`. */
    activePet?: string;
  } & HomesteadSaveExtras;
  /** Ley-beasts (PetSystem). Absent in older saves: migrated from `homestead.pets`. */
  pets?: PetSaveData;
  achievements: Record<string, number>;
  settings: {
    autoCombat: boolean;
    musicVolume: number;
    sfxVolume: number;
    autoLootMode: 'off' | 'all' | 'magic' | 'rare' | 'legendary';
  };
  difficulty: 'normal' | 'nightmare' | 'hell';
  completedDifficulties: string[];
  mercenary?: MercenarySaveData;
  /** Tracks visited dialogue nodes and choices made per NPC. */
  dialogueState?: Record<string, { visitedNodes: string[]; choicesMade: Record<string, string> }>;
  /** Mini-boss IDs whose pre-fight dialogue has been seen (does not repeat). */
  miniBossDialogueSeen?: string[];
  /** Lore collectible IDs that have been discovered. */
  loreCollected?: string[];
  /** Hidden area IDs that have been discovered and had rewards collected. */
  discoveredHiddenAreas?: string[];
  /** Story beats already played (prologue, chapters, cutscenes, boss intros, ending). */
  storySeen?: string[];
  /** Gold/exp dropped at the hero's last death, waiting to be reclaimed. */
  soulEcho?: SoulEchoData | null;
  /** Abyss Labyrinth tier ladder: highest tier unlocked / cleared, best clear time. */
  abyss?: { unlockedTier: number; bestTier: number; bestTimeMs?: number };
}

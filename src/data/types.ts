export interface Stats {
  str: number;
  dex: number;
  vit: number;
  int: number;
  spi: number;
  lck: number;
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
  bonuses: { count: number; description: string; stats: Partial<Record<string, number>> }[];
}

export interface LegendaryDefinition {
  id: string;
  baseId: string;
  name: string;
  nameEn: string;
  fixedAffixes: ItemAffix[];
  specialEffect: string;
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
  type: 'kill' | 'collect' | 'explore' | 'talk';
  category: 'main' | 'side';
  objectives: QuestObjective[];
  rewards: QuestReward;
  prereqQuests?: string[];
  level: number;
  questArea?: { col: number; row: number; radius: number };
}

export interface QuestObjective {
  type: 'kill' | 'collect' | 'explore' | 'talk';
  targetId: string;
  targetName: string;
  required: number;
  current: number;
  location?: { col: number; row: number; radius: number };
}

export interface QuestReward {
  exp: number;
  gold: number;
  items?: string[];
}

export interface QuestProgress {
  questId: string;
  status: 'available' | 'active' | 'completed' | 'turned_in';
  objectives: { current: number }[];
}

export interface NPCDefinition {
  id: string;
  name: string;
  type: 'blacksmith' | 'merchant' | 'quest' | 'stash';
  dialogue: string[];
  shopItems?: string[];
  quests?: string[];
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

export interface HomesteadBuilding {
  id: string;
  name: string;
  description: string;
  maxLevel: number;
  costPerLevel: { gold: number; materials?: Record<string, number> }[];
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
    pets: { petId: string; level: number; exp: number }[];
    activePet?: string;
  };
  achievements: Record<string, number>;
  settings: {
    autoCombat: boolean;
    musicVolume: number;
    sfxVolume: number;
    autoLootMode: 'off' | 'all' | 'magic' | 'rare';
  };
  difficulty: 'normal' | 'nightmare' | 'hell';
  completedDifficulties: string[];
}

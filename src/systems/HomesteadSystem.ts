import { EventBus, GameEvents } from '../utils/EventBus';
import type { HomesteadBuilding, PetDefinition } from '../data/types';
import { t } from '../i18n';

const BUILDINGS: HomesteadBuilding[] = [
  {
    id: 'herb_garden', name: '药草园', description: '自动产出药水材料',
    maxLevel: 5,
    costPerLevel: [{ gold: 100 }, { gold: 250 }, { gold: 500 }, { gold: 1000 }, { gold: 2000 }],
    bonusPerLevel: [{ stat: 'potionDiscount', value: 5 }],
  },
  {
    id: 'training_ground', name: '训练场', description: '佣兵经验加成',
    maxLevel: 5,
    costPerLevel: [{ gold: 150 }, { gold: 350 }, { gold: 700 }, { gold: 1400 }, { gold: 2800 }],
    bonusPerLevel: [{ stat: 'mercExpBonus', value: 5 }],
  },
  {
    id: 'gem_workshop', name: '宝石工坊', description: '合成/升级宝石',
    maxLevel: 5,
    costPerLevel: [{ gold: 200 }, { gold: 450 }, { gold: 900 }, { gold: 1800 }, { gold: 3500 }],
    bonusPerLevel: [{ stat: 'gemBonus', value: 2 }],
  },
  {
    id: 'pet_house', name: '宠物小屋', description: '饲养宠物，提升宠物经验加成',
    maxLevel: 5,
    costPerLevel: [{ gold: 300 }, { gold: 600 }, { gold: 1200 }, { gold: 2000 }, { gold: 3500 }],
    bonusPerLevel: [{ stat: 'petExpBonus', value: 10 }],
  },
  {
    id: 'warehouse', name: '仓库', description: '扩展存储空间',
    maxLevel: 5,
    costPerLevel: [{ gold: 100 }, { gold: 200 }, { gold: 400 }, { gold: 800 }, { gold: 1600 }],
    bonusPerLevel: [{ stat: 'stashSlots', value: 10 }],
  },
  {
    id: 'altar', name: '祭坛', description: '临时Buff',
    maxLevel: 3,
    costPerLevel: [{ gold: 500 }, { gold: 1500 }, { gold: 4000 }],
    bonusPerLevel: [{ stat: 'altarBonus', value: 3 }],
  },
];

/** Evolution thresholds and names for pets. */
export interface PetEvolution {
  level: number;
  nameSuffix: string;
  bonusMultiplier: number;
}

/** Default evolution stages at levels 10 and 20. */
const EVOLUTION_STAGES: PetEvolution[] = [
  { level: 10, nameSuffix: '·觉醒', bonusMultiplier: 1.5 },
  { level: 20, nameSuffix: '·至尊', bonusMultiplier: 2.0 },
];

// 5 original pets (unchanged)
const PETS: PetDefinition[] = [
  { id: 'pet_sprite', name: '小精灵', description: '可爱的精灵，增加经验获取', rarity: 'common', bonusStat: 'expBonus', bonusValue: 3, bonusPerLevel: 0.5, maxLevel: 20, feedItem: 'c_hp_potion_s' },
  { id: 'pet_dragon', name: '小火龙', description: '火龙幼崽，增加攻击力', rarity: 'rare', bonusStat: 'damage', bonusValue: 5, bonusPerLevel: 1, maxLevel: 20, feedItem: 'c_mp_potion_s' },
  { id: 'pet_owl', name: '猫头鹰', description: '智慧的猫头鹰，增加掉宝率', rarity: 'common', bonusStat: 'magicFind', bonusValue: 5, bonusPerLevel: 1, maxLevel: 20, feedItem: 'c_hp_potion_s' },
  { id: 'pet_cat', name: '暗影猫', description: '神秘的黑猫，增加暴击率', rarity: 'rare', bonusStat: 'critRate', bonusValue: 2, bonusPerLevel: 0.3, maxLevel: 20, feedItem: 'c_mp_potion_s' },
  { id: 'pet_phoenix', name: '凤凰雏', description: '凤凰之子，增加生命回复', rarity: 'epic', bonusStat: 'hpRegen', bonusValue: 3, bonusPerLevel: 0.8, maxLevel: 20, feedItem: 'c_hp_potion_m' },
  // 3 new rare/epic pets with unique bonusStat and distinct acquisition
  { id: 'pet_storm_wolf', name: '雷狼', description: '雷暴之子，增加攻击速度（BOSS掉落）', rarity: 'epic', bonusStat: 'attackSpeed', bonusValue: 3, bonusPerLevel: 0.5, maxLevel: 20, feedItem: 'c_mp_potion_m' },
  { id: 'pet_jade_tortoise', name: '玄武龟', description: '远古守护，增加防御力（任务奖励）', rarity: 'epic', bonusStat: 'defense', bonusValue: 4, bonusPerLevel: 0.8, maxLevel: 20, feedItem: 'c_hp_potion_m' },
  { id: 'pet_void_butterfly', name: '虚空蝶', description: '虚空使者，增加法力回复（稀有刷新）', rarity: 'epic', bonusStat: 'manaRegen', bonusValue: 2, bonusPerLevel: 0.6, maxLevel: 20, feedItem: 'c_mp_potion_s' },
];

export interface PetInstance {
  petId: string;
  level: number;
  exp: number;
  evolved: number; // tracks evolution stage count (0, 1, 2)
}

export class HomesteadSystem {
  buildings: Record<string, number> = {};
  pets: PetInstance[] = [];
  activePet: string | null = null;

  /** Last time the active pet attacked (for periodic pet combat). */
  petLastAttackTime = -Infinity;
  /** Pet attack interval in ms (3 seconds). */
  static readonly PET_ATTACK_INTERVAL = 3000;
  /** Base pet damage fraction of player damage. */
  static readonly PET_DAMAGE_BASE_FRACTION = 0.05;
  /** Additional damage fraction per pet level. */
  static readonly PET_DAMAGE_PER_LEVEL_FRACTION = 0.005;
  /** Max pet damage fraction (at level 20: 5% + 20*0.5% = 15%). */
  static readonly PET_DAMAGE_MAX_FRACTION = 0.15;

  getBuildingDef(id: string): HomesteadBuilding | undefined {
    return BUILDINGS.find(b => b.id === id);
  }

  getAllBuildings(): HomesteadBuilding[] { return BUILDINGS; }
  getAllPets(): PetDefinition[] { return PETS; }
  getEvolutionStages(): PetEvolution[] { return EVOLUTION_STAGES; }

  getPetDef(petId: string): PetDefinition | undefined {
    return PETS.find(p => p.id === petId);
  }

  getPetInstance(petId: string): PetInstance | undefined {
    return this.pets.find(p => p.petId === petId);
  }

  getActivePetInstance(): PetInstance | undefined {
    if (!this.activePet) return undefined;
    return this.pets.find(p => p.petId === this.activePet);
  }

  getActivePetDef(): PetDefinition | undefined {
    if (!this.activePet) return undefined;
    return PETS.find(p => p.id === this.activePet);
  }

  getBuildingLevel(id: string): number {
    return this.buildings[id] ?? 0;
  }

  /** Pet EXP bonus from pet_house level (10% per level). */
  getPetExpBonus(): number {
    return 1 + (this.buildings['pet_house'] ?? 0) * 0.1;
  }

  /** @deprecated No capacity limit — returns Infinity for backwards compatibility. */
  getMaxPetSlots(): number {
    return Infinity;
  }

  /** Training ground mercenary exp bonus (percentage). */
  getTrainingGroundBonus(): number {
    return (this.buildings['training_ground'] ?? 0) * 5;
  }

  canUpgrade(id: string, gold: number): boolean {
    const def = this.getBuildingDef(id);
    if (!def) return false;
    const currentLevel = this.getBuildingLevel(id);
    if (currentLevel >= def.maxLevel) return false;
    return gold >= def.costPerLevel[currentLevel].gold;
  }

  upgrade(id: string): number {
    const def = this.getBuildingDef(id);
    if (!def) return 0;
    const currentLevel = this.getBuildingLevel(id);
    if (currentLevel >= def.maxLevel) return 0;
    const cost = def.costPerLevel[currentLevel].gold;
    this.buildings[id] = currentLevel + 1;
    EventBus.emit(GameEvents.HOMESTEAD_UPGRADED, { buildingId: id, level: currentLevel + 1 });
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: t('sys.homestead.buildingUpgrade', { name: def.name, level: currentLevel + 1 }),
      type: 'system',
    });
    return cost;
  }

  /** Get the evolution multiplier for a pet based on its evolved stage. */
  getEvolutionMultiplier(pet: PetInstance): number {
    if (pet.evolved <= 0) return 1.0;
    // Each evolution stage has a specific multiplier
    const stage = EVOLUTION_STAGES[pet.evolved - 1];
    return stage ? stage.bonusMultiplier : 1.0;
  }

  /** Get the display name for a pet including evolution suffix. */
  getPetDisplayName(pet: PetInstance): string {
    const def = this.getPetDef(pet.petId);
    if (!def) return pet.petId;
    if (pet.evolved > 0 && pet.evolved <= EVOLUTION_STAGES.length) {
      return def.name + EVOLUTION_STAGES[pet.evolved - 1].nameSuffix;
    }
    return def.name;
  }

  getTotalBonuses(): Record<string, number> {
    const bonuses: Record<string, number> = {};
    for (const [id, level] of Object.entries(this.buildings)) {
      const def = this.getBuildingDef(id);
      if (!def) continue;
      for (const bonus of def.bonusPerLevel) {
        bonuses[bonus.stat] = (bonuses[bonus.stat] ?? 0) + bonus.value * level;
      }
    }
    // Active pet bonus (with evolution multiplier)
    if (this.activePet) {
      const pet = this.pets.find(p => p.petId === this.activePet);
      if (pet) {
        const def = PETS.find(p => p.id === pet.petId);
        if (def) {
          const evoMult = this.getEvolutionMultiplier(pet);
          const baseBonus = def.bonusValue + def.bonusPerLevel * pet.level;
          bonuses[def.bonusStat] = (bonuses[def.bonusStat] ?? 0) + Math.floor(baseBonus * evoMult);
        }
      }
    }
    return bonuses;
  }

  addPet(petId: string): boolean {
    // Duplicate prevention
    if (this.pets.find(p => p.petId === petId)) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('sys.homestead.petDuplicate'), type: 'system' });
      return false;
    }
    this.pets.push({ petId, level: 1, exp: 0, evolved: 0 });
    const def = PETS.find(p => p.id === petId);
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: t('sys.homestead.petObtained', { name: def?.name ?? petId }),
      type: 'system',
    });
    if (!this.activePet) this.activePet = petId;
    return true;
  }

  feedPet(petId: string): boolean {
    const pet = this.pets.find(p => p.petId === petId);
    if (!pet) return false;
    const def = PETS.find(p => p.id === petId);
    if (!def || pet.level >= def.maxLevel) return false;
    const baseExp = 10;
    pet.exp += Math.floor(baseExp * this.getPetExpBonus());
    const needed = pet.level * 20;
    if (pet.exp >= needed) {
      pet.exp -= needed;
      pet.level++;
      EventBus.emit(GameEvents.LOG_MESSAGE, {
        text: t('sys.homestead.petLevelUp', { name: def.name, level: pet.level }),
        type: 'system',
      });

      // Check for evolution at threshold levels
      this.checkEvolution(pet, def);
    }
    return true;
  }

  /** Check and apply evolution at specific level thresholds. */
  private checkEvolution(pet: PetInstance, def: PetDefinition): void {
    for (let i = 0; i < EVOLUTION_STAGES.length; i++) {
      const stage = EVOLUTION_STAGES[i];
      if (pet.level >= stage.level && pet.evolved <= i) {
        pet.evolved = i + 1;
        const evolvedName = def.name + stage.nameSuffix;
        EventBus.emit(GameEvents.LOG_MESSAGE, {
          text: t('sys.homestead.petEvolved', { name: def.name, evolvedName }),
          type: 'system',
        });
      }
    }
  }

  /** Calculate pet attack damage as fraction of player damage, scaling with pet level. */
  calculatePetDamage(playerDamage: number): number {
    const pet = this.getActivePetInstance();
    if (!pet) return 0;
    const fraction = Math.min(
      HomesteadSystem.PET_DAMAGE_MAX_FRACTION,
      HomesteadSystem.PET_DAMAGE_BASE_FRACTION + pet.level * HomesteadSystem.PET_DAMAGE_PER_LEVEL_FRACTION,
    );
    const evoMult = this.getEvolutionMultiplier(pet);
    const raw = Math.floor(playerDamage * fraction * evoMult);
    return raw > 0 ? Math.max(1, raw) : 0;
  }

  /** Whether the pet can attack this tick (periodic interval check). */
  canPetAttack(time: number): boolean {
    if (!this.activePet) return false;
    return time - this.petLastAttackTime >= HomesteadSystem.PET_ATTACK_INTERVAL;
  }

  /** Record that a pet attack was performed. */
  recordPetAttack(time: number): void {
    this.petLastAttackTime = time;
  }

  setActivePet(petId: string | null): void {
    this.activePet = petId;
  }

  /** Reset all pet/homestead state for a new game. */
  resetState(): void {
    this.buildings = {};
    this.pets = [];
    this.activePet = null;
    this.petLastAttackTime = -Infinity;
  }
}

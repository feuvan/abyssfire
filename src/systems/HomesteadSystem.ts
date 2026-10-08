import { EventBus, GameEvents } from '../utils/EventBus';
import type { HomesteadBuilding } from '../data/types';
import { t } from '../i18n';
import { getBuildingName } from '../i18n/gameAccessors';
import { BUILDINGS } from '../data/homestead';
import { HomesteadTower } from './HomesteadTower';

export class HomesteadSystem {
  buildings: Record<string, number> = {};
  /** Ember Tower state: embers, unlocks, garden, expedition, blessing (see HomesteadTower). */
  readonly tower = new HomesteadTower(this);

  getBuildingDef(id: string): HomesteadBuilding | undefined {
    return BUILDINGS.find(b => b.id === id);
  }

  getAllBuildings(): HomesteadBuilding[] { return BUILDINGS; }

  getBuildingLevel(id: string): number {
    return this.buildings[id] ?? 0;
  }

  /** Training ground mercenary exp bonus (percentage). */
  getTrainingGroundBonus(): number {
    return (this.buildings['training_ground'] ?? 0) * 5;
  }

  /** Upgrade cost for the next level (null when maxed or unknown). */
  getUpgradeCost(id: string): { gold: number; embers: number } | null {
    const def = this.getBuildingDef(id);
    if (!def) return null;
    const lv = this.getBuildingLevel(id);
    if (lv >= def.maxLevel) return null;
    const c = def.costPerLevel[lv];
    return { gold: c.gold, embers: c.embers ?? 0 };
  }

  /** Affordable, not maxed and (for story wings) unlocked. Embers default to the tower's purse. */
  canUpgrade(id: string, gold: number, embers = this.tower.embers): boolean {
    const cost = this.getUpgradeCost(id);
    if (!cost || !this.tower.isBuildingUnlocked(id)) return false;
    return gold >= cost.gold && embers >= cost.embers;
  }

  /** Raise a wing one level, spending its embers; returns the gold cost for the caller to deduct (0 if nothing happened). */
  upgrade(id: string): number {
    const def = this.getBuildingDef(id);
    const cost = this.getUpgradeCost(id);
    if (!def || !cost || !this.tower.isBuildingUnlocked(id) || this.tower.embers < cost.embers) return 0;
    const currentLevel = this.getBuildingLevel(id);
    this.tower.embers -= cost.embers;
    this.buildings[id] = currentLevel + 1;
    EventBus.emit(GameEvents.HOMESTEAD_UPGRADED, { buildingId: id, level: currentLevel + 1 });
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: t('sys.homestead.buildingUpgrade', { name: getBuildingName(def.id, def.name), level: currentLevel + 1 }),
      type: 'system',
    });
    return cost.gold;
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
    return bonuses;
  }

  /** Reset all homestead state for a new game (ley-beasts live in PetSystem). */
  resetState(): void {
    this.buildings = {};
    this.tower.reset();
  }
}

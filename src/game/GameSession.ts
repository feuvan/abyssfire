import { AchievementSystem } from '../systems/AchievementSystem';
import { CombatSystem } from '../systems/CombatSystem';
import { EliteAffixSystem } from '../systems/EliteAffixSystem';
import { HomesteadSystem } from '../systems/HomesteadSystem';
import { InventorySystem } from '../systems/InventorySystem';
import { LootSystem } from '../systems/LootSystem';
import { MercenarySystem } from '../systems/MercenarySystem';
import { QuestSystem } from '../systems/QuestSystem';
import { RandomEventSystem } from '../systems/RandomEventSystem';
import { SaveSystem } from '../systems/SaveSystem';
import { StatusEffectSystem } from '../systems/StatusEffectSystem';
import { AllQuests } from '../data/quests/all_quests';

export interface ZoneRuntime {
  combat: CombatSystem;
  loot: LootSystem;
  statusEffects: StatusEffectSystem;
  eliteAffixes: EliteAffixSystem;
  randomEvents: RandomEventSystem;
}

export class GameSession {
  readonly inventory = new InventorySystem();
  readonly quests = new QuestSystem();
  readonly homestead = new HomesteadSystem();
  readonly achievements = new AchievementSystem();
  readonly saves = new SaveSystem();
  readonly mercenaries = new MercenarySystem();

  constructor() {
    this.quests.registerQuests(AllQuests);
  }

  beginZone(zoneId: string, levelRange: [number, number], safeZoneRadius: number): ZoneRuntime {
    return {
      combat: new CombatSystem(),
      loot: new LootSystem(),
      statusEffects: new StatusEffectSystem(),
      eliteAffixes: new EliteAffixSystem(),
      randomEvents: new RandomEventSystem(
        { zoneId, levelRange },
        { safeZoneRadius },
      ),
    };
  }
}

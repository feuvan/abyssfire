import { AchievementSystem } from '../systems/AchievementSystem';
import { CombatSystem } from '../systems/CombatSystem';
import { EliteAffixSystem } from '../systems/EliteAffixSystem';
import { HomesteadSystem } from '../systems/HomesteadSystem';
import { PetSystem } from '../systems/PetSystem';
import { InventorySystem } from '../systems/InventorySystem';
import { LootSystem } from '../systems/LootSystem';
import { MercenarySystem } from '../systems/MercenarySystem';
import { QuestSystem } from '../systems/QuestSystem';
import { RandomEventSystem } from '../systems/RandomEventSystem';
import { SaveSystem } from '../systems/SaveSystem';
import { StatusEffectSystem } from '../systems/StatusEffectSystem';
import { StoryProgress } from '../systems/StoryProgress';
import { SoulEchoState } from '../systems/SoulEcho';
import type { AbyssRecord } from '../systems/DungeonSystem';
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
  /** Ley-beasts (灵兽); reads the tower's 月井 level through the homestead. */
  readonly pets = new PetSystem();
  readonly achievements = new AchievementSystem();
  readonly saves = new SaveSystem();
  readonly mercenaries = new MercenarySystem();
  readonly story = new StoryProgress();
  readonly soulEcho = new SoulEchoState();
  /** Abyss Labyrinth tier ladder progress. */
  abyss: AbyssRecord = { unlockedTier: 1, bestTier: 0 };

  constructor() {
    this.quests.registerQuests(AllQuests);
    this.pets.setBuildingLevelSource(id => this.homestead.getBuildingLevel(id));
    this.pets.setAwaySource(id => this.homestead.tower.isPetAway(id));
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

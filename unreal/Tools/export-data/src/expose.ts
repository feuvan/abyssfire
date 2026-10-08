/**
 * Module-private bindings of the web game that the exporter reads.
 *
 * run.mjs appends `export { NAME as __expose_NAME }` to each listed module **in memory** while
 * Vite loads it (src/ is never edited). If a name disappears from the TS source the module fails
 * to load and the export stops with an error, so renamed constants cannot be silently missed.
 *
 * The block between the markers is parsed as JSON by run.mjs (keep it valid JSON).
 */
export const EXPOSE: Record<string, string[]> = /*JSON*/{
  "src/systems/SkillProgressionSystem.ts": ["TIER_PLAYER_LEVEL", "TIER_TREE_POINTS"],
  "src/systems/SpiritSystem.ts": ["SPIRIT_PROFILES"],
  "src/systems/CombatSystem.ts": ["tieredScale"],
  "src/systems/CombatInputSystem.ts": ["DEFAULT_DODGE_CONFIG"],
  "src/systems/SkillEffectSystem.ts": ["METEOR_FALL_MS", "CH"],
  "src/systems/EliteAffixSystem.ts": ["ZONE_AFFIX_COUNTS", "ALL_AFFIX_TYPES"],
  "src/systems/SoulEcho.ts": ["GOLD_SHARE", "EXP_SHARE"],
  "src/systems/CraftingSystem.ts": ["AFFIX_RANGE"],
  "src/systems/WeatherSystem.ts": ["ZONE_WEATHER", "THEME_WEATHER"],
  "src/systems/LightingSystem.ts": ["ZONE_THEME_BY_ID"],
  "src/systems/AchievementSystem.ts": ["ACHIEVEMENTS"],
  "src/systems/MapGenerator.ts": ["THEME_CONFIGS", "DECOR_POOLS", "SeededRandom", "groveNoise", "pickWeighted",
    "TILE_GRASS", "TILE_DIRT", "TILE_STONE", "TILE_WATER", "TILE_WALL", "TILE_CAMP", "TILE_CAMP_WALL"],
  "src/data/maps/index.ts": ["externalLandmarks"],
  "src/systems/CharacterAnimator.ts": ["PRESETS"],
  "src/graphics/SpriteGenerator.ts": ["IDLE_COUNT", "WALK_COUNT", "ATK_COUNT", "HURT_COUNT", "DEATH_COUNT", "MONSTER_VIEW_FRAMES"],
  "src/systems/audio/MusicEngine.ts": ["SCORE_TRIM"],
  "src/systems/audio/Composer.ts": ["RHYTHMS_4", "RHYTHMS_3"],
  "src/systems/audio/ScorePlayer.ts": ["BASS_FLOOR"],
  "src/systems/audio/AudioManager.ts": ["DEFAULT_SETTINGS"],
  "src/rendering/RenderQuality.ts": ["PROFILES", "RESOLUTION_BY_QUALITY"],
  "src/scenes/StoryScene.ts": ["MOODS"],
  "src/systems/QuestWorld.ts": ["GATHER_RANGE", "CLUE_RANGE", "GUIDE_NEAR", "KIND_BY_TARGET"],
  "src/systems/StoryDirector.ts": ["BOSS_SIGHT", "BOSS_BAR_RANGE", "FINAL_BOSS"],
  "src/systems/PetCompanion.ts": ["STRAY_SWING_CHANCE", "FOLLOW_SPEED", "DASH_SPEED", "TELEPORT_DIST"],
  "src/systems/InventorySystem.ts": ["QUALITY_ORDER", "TYPE_ORDER"],
  "src/systems/QuestRewards.ts": ["SHIELD_CLASSES"],
  "src/systems/MobileControlsSystem.ts": ["CSS", "MIN_K", "MAX_K", "TOUCH_POINTERS"],
  "src/scenes/ZoneScene.ts": ["GROUND_AOE_SKILLS", "skillImpactColor", "CLASS_IMPACT_COLORS", "CAMPFIRE_RECOVERY_RADIUS",
    "CAMPFIRE_HP_REGEN_MULTIPLIER", "CAMPFIRE_MANA_REGEN_MULTIPLIER", "HOLD_MOVE_REPATH_MS", "ESCORT_CATCH_UP_TILES"]
}/*END*/;

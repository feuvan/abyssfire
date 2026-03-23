---
name: content-worker
description: Creates game content - skills, monsters, quests, NPCs, maps, dialogue data, and VFX definitions
---

# Content Worker

NOTE: Startup and cleanup are handled by `worker-base`. This skill defines the WORK PROCEDURE.

## When to Use This Skill

Features involving:
- Skill definitions and data (class skill trees, damage multipliers, synergies)
- Skill VFX in SkillEffectSystem (particle effects, animations)
- Monster definitions (stats, AI behaviors, elite affixes)
- Quest definitions (objectives, rewards, prerequisite chains)
- NPC definitions and dialogue content
- Map data (spawn points, camps, exits, zone content expansion)
- Lore collectibles and environmental storytelling data

## Required Skills

None. This worker uses standard file tools and Execute for builds.

## Work Procedure

1. **Read the feature description carefully.** Understand what content to create, the expected quantity, and quality requirements.

2. **Study existing content patterns.** Before creating new content:
   - Read existing skill definitions in `src/data/classes/warrior.ts` (or mage/rogue) to match the data structure
   - Read existing monster definitions in `src/data/monsters/` for stat patterns
   - Read existing quest definitions in `src/data/quests/all_quests.ts` for quest structure
   - Read existing VFX in `src/systems/SkillEffectSystem.ts` for the effect pattern
   - Note numerical ranges and scaling conventions

3. **Write tests for new content (where applicable).** Content data often needs validation tests:
   - Skill definitions: test that all required fields exist, synergy IDs reference valid skills, damage multipliers are within expected ranges
   - Map data: test that coordinates are within bounds, exits point to valid maps
   - Quest data: test that prereqQuest IDs reference valid quests, objective types are valid

4. **Create the content.** Follow these guidelines:
   - **Skills**: Each skill needs: id, name (Chinese), nameEn, description (Chinese), tree, tier, damageMultiplier, manaCost, cooldown, range, damageType, synergies, optional aoe/buff/statusEffect fields
   - **Skill VFX**: Add a `case` in `SkillEffectSystem.play()` with a dedicated effect method. Use existing particle textures. Generate a `skill_icon_{id}` in `generateSkillIcons()`
   - **Monsters**: Define stats proportional to zone level range. Elite variants need affix system integration
   - **Quests**: Chinese name, description, clear objectives with type/target/count, rewards (exp/gold/items), prereqQuests chain
   - **Dialogue**: Chinese text, branching trees with choice nodes
   - **All player-facing text MUST be in Simplified Chinese**

5. **Ensure content integrates correctly.** New skills must be importable by ZoneScene/CombatSystem. New quests must be loadable by QuestSystem. New monsters must be spawnable.

6. **Run verification.**
   - `npx vitest run` — all tests pass
   - `npx tsc --noEmit` — no type errors
   - `npm run build` — production build succeeds

7. **Manual verification.** Check in-browser that new content appears (skill tree shows new skills, new quests are offered by NPCs, new monsters spawn correctly).

## Example Handoff

```json
{
  "salientSummary": "Added 8 new Warrior skills: Charge (rush + damage), Lethal Strike (high single-target), Iron Fortress (defense buff), Frenzy (attack speed buff), Bleed Strike (phys DoT), Dual Wield Mastery (passive dual wield bonus), Unyielding (passive low-HP defense), Life Regen (passive HP regen). All have synergies, VFX, and procedural icons. Wrote 12 validation tests. `npm run build` succeeds.",
  "whatWasImplemented": "8 new skill definitions in warrior.ts with complete fields (id, name, nameEn, description, tree, tier, damageMultiplier/buff, manaCost, cooldown, range, damageType, synergies). 8 new VFX cases in SkillEffectSystem.play() with dedicated effect methods. 8 procedural skill icons in generateSkillIcons(). Updated TREE_NAMES and TREE_COLORS for any new tree names.",
  "whatWasLeftUndone": "",
  "verification": {
    "commandsRun": [
      { "command": "npx vitest run", "exitCode": 0, "observation": "12 tests pass - skill definition validation, synergy reference checks" },
      { "command": "npm run build", "exitCode": 0, "observation": "Build succeeds" }
    ],
    "interactiveChecks": [
      { "action": "Opened Warrior skill tree (K key)", "observed": "All 15 skills visible across 3 trees, new icons render, tooltips show Chinese text with synergy info" },
      { "action": "Used Charge in combat", "observed": "Player rushed to target with trail VFX, damage number appeared, mana consumed, cooldown started" }
    ]
  },
  "tests": {
    "added": [
      {
        "file": "src/__tests__/warrior-skills.test.ts",
        "cases": [
          { "name": "all warrior skills have required fields", "verifies": "Skill definition completeness" },
          { "name": "synergy references are valid warrior skill IDs", "verifies": "Synergy integrity" },
          { "name": "damage multipliers within 0-3.0 range", "verifies": "Balance constraints" }
        ]
      }
    ]
  },
  "discoveredIssues": []
}
```

## When to Return to Orchestrator

- Content requires new type definitions not yet added to `types.ts`
- VFX requires new particle textures not generated in BootScene
- Quest requires a new objective type not supported by QuestSystem
- Monster AI requires new behavior states not in Monster.ts
- Dialogue tree structure requires a data format not yet defined

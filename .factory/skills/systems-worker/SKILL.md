---
name: systems-worker
description: Implements core game systems, combat mechanics, AI, pathfinding, save system, and test infrastructure
---

# Systems Worker

NOTE: Startup and cleanup are handled by `worker-base`. This skill defines the WORK PROCEDURE.

## When to Use This Skill

Features involving:
- Core system logic (CombatSystem, PathfindingSystem, StatusEffectSystem, SaveSystem)
- Test infrastructure setup (Vitest configuration, test helpers, mocks)
- Numerical balance (damage formulas, stat curves, economy)
- AI behavior (mercenary combat AI, monster elite affixes)
- Data structure changes (SaveData migration, new types)
- Map system changes (size expansion, generation parameters)

## Required Skills

None. This worker uses standard file tools and Execute for running tests/builds.

## Work Procedure

1. **Read the feature description carefully.** Understand preconditions, expected behavior, and verification steps.

2. **Explore existing code.** Before writing anything, read the relevant source files to understand current patterns, interfaces, and data flow. Pay special attention to:
   - `src/data/types.ts` for type definitions
   - The system file you're modifying
   - How the system integrates with ZoneScene and other systems
   - Existing test patterns (if any exist in `src/__tests__/`)

3. **Write tests FIRST (red).** Create test file in `src/__tests__/{system}.test.ts`. Write failing tests that define the expected behavior from the feature description. Tests must:
   - Import the system module directly (not through Phaser scenes)
   - Mock Phaser dependencies if the module imports Phaser
   - Cover all `expectedBehavior` items from the feature
   - Include edge cases (zero values, max values, negative inputs, empty arrays)

4. **Implement the feature (green).** Write the minimum code to make tests pass. Follow existing patterns:
   - Export interfaces/types from `types.ts` or the system file
   - Use EventBus for cross-system communication
   - Follow existing naming conventions
   - Add Chinese UI text for any player-facing strings

5. **Wire into game loop.** If the feature adds new behavior to the runtime:
   - Update `ZoneScene.ts` to call the new system methods
   - Update `Player.ts` if player stats/behavior changes
   - Update `Monster.ts` if monster behavior changes
   - Emit appropriate EventBus events

6. **Run verification.**
   - `npx vitest run` — all tests pass
   - `npx tsc --noEmit` — no type errors
   - `npm run build` — production build succeeds

7. **Manual sanity check.** If the dev server is running, briefly verify in-browser that the feature doesn't break existing gameplay (load a save, move around, basic combat).

## Example Handoff

```json
{
  "salientSummary": "Implemented StatusEffectSystem with Burn/Freeze/Poison/Bleed/Slow/Stun. Each effect has apply/tick/expire lifecycle with visual indicators. Added diminishing returns for Freeze/Stun. Wrote 24 unit tests covering all effect types plus edge cases (death during DoT, stacking policy, immunity windows). `npx vitest run` passes (24/24), `npm run build` succeeds.",
  "whatWasImplemented": "New StatusEffectSystem class in src/systems/StatusEffectSystem.ts with 6 status effect types. Each effect tracked per-entity with duration, tick interval, and value. Integrated into CombatSystem.calculateDamage() for DoT ticks and into Monster.ts for movement/attack prevention during stun/freeze. Visual indicators added via VFXManager.applyStatusTint(). Diminishing returns: repeated stun/freeze on same target within 5s gets 50% duration, third application grants 3s immunity.",
  "whatWasLeftUndone": "",
  "verification": {
    "commandsRun": [
      { "command": "npx vitest run", "exitCode": 0, "observation": "24 tests passed in src/__tests__/StatusEffectSystem.test.ts" },
      { "command": "npx tsc --noEmit", "exitCode": 0, "observation": "No type errors" },
      { "command": "npm run build", "exitCode": 0, "observation": "Build succeeded, 1.9MB bundle" }
    ],
    "interactiveChecks": [
      { "action": "Loaded game, used War Stomp near goblins", "observed": "Goblins froze in place for ~2s with stun indicator, then resumed movement" },
      { "action": "Cast Fireball on skeleton, observed burn", "observed": "Burn indicator (orange particles) appeared, 3 damage ticks visible in combat log, indicator removed after 6s" }
    ]
  },
  "tests": {
    "added": [
      {
        "file": "src/__tests__/StatusEffectSystem.test.ts",
        "cases": [
          { "name": "applies burn with correct duration and tick rate", "verifies": "Burn lifecycle" },
          { "name": "freeze prevents movement and attack", "verifies": "Freeze immobilization" },
          { "name": "stun diminishing returns on rapid reapplication", "verifies": "Anti-perma-stun" },
          { "name": "status effects clear on entity death", "verifies": "Death cleanup" }
        ]
      }
    ]
  },
  "discoveredIssues": []
}
```

## When to Return to Orchestrator

- Feature requires changes to ZoneScene that would conflict with another in-progress feature
- SaveData migration requires understanding of what other features will add
- Numerical balance depends on data that hasn't been created yet (e.g., new skill multipliers)
- Phaser API behavior is unexpected and blocks the feature
- Test infrastructure setup reveals dependency issues that need architectural decisions

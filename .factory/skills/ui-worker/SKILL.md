---
name: ui-worker
description: Implements UI panels, visual polish, sprite generation improvements, and HUD elements
---

# UI Worker

NOTE: Startup and cleanup are handled by `worker-base`. This skill defines the WORK PROCEDURE.

## When to Use This Skill

Features involving:
- UIScene panels (inventory, skill tree, achievement panel, companion panel, gem socket panel)
- HUD elements (HP/MP globes, skill bar, minimap, quest tracker)
- SpriteGenerator improvements (palette consistency, outline uniformity, sprite detail)
- Visual polish (panel styling, tooltip layout, animation consistency)
- Homestead panel visual upgrades
- Menu/title screen changes

## Required Skills

None. This worker uses standard file tools and Execute for builds.

## Work Procedure

1. **Read the feature description carefully.** Understand what UI to create or modify, styling requirements, and interaction behavior.

2. **Study existing UI patterns.** Before modifying any UI:
   - Read `src/scenes/UIScene.ts` thoroughly — understand panel creation patterns, styling constants, depth layering, mutual exclusion
   - Note the `px()` and `fs()` DPR-aware helper functions — ALL dimensions and font sizes MUST use these
   - Note panel open animation via `animatePanelOpen()`
   - Note color constants: panel backgrounds, border colors, text colors
   - Read `src/graphics/SpriteGenerator.ts` for sprite generation patterns
   - Read `src/graphics/DrawUtils.ts` for shared drawing utilities

3. **Write tests where applicable.** UI logic tests for:
   - Data formatting functions (stat display, tooltip text generation)
   - Panel state management (mutual exclusion, toggle behavior)
   - Not needed for pure rendering code (Phaser graphics calls)

4. **Implement the UI changes.** Follow these strict conventions:
   - **All dimensions via `px()`, all font sizes via `fs()`** — no hardcoded pixel values
   - **Panel background**: Use the standard dark background color+alpha from existing panels
   - **Panel border**: Match existing border color, width, and corner radius
   - **Panel header**: Same height, font, size, color, close button style as other panels
   - **Text**: Use `FONT` constant (Noto Sans SC), Chinese for all player-facing strings
   - **Depth**: Panels at 4000, tooltips at 5000
   - **Mutual exclusion**: New panels must participate in the `UI_TOGGLE_PANEL` event system
   - **Keyboard shortcuts**: Bind in ZoneScene keyboard handler, emit `UI_TOGGLE_PANEL`
   - **Colors**: Gold (#c0934a) for headers/accents, quality colors for items (white/blue/yellow/orange/green)

5. **For sprite generation changes:**
   - Maintain consistent outline technique across all entity types
   - Use `softOutline` with uniform blur parameter or stroke with uniform lineWidth
   - Zone palette: each zone's sprites use hues within 30° of zone's dominant color on HSL wheel
   - Use `DrawUtils.applyDirectionalLighting()` consistently
   - Test that no missing textures appear (no green rectangles)

6. **Run verification.**
   - `npx tsc --noEmit` — no type errors
   - `npm run build` — production build succeeds
   - `npx vitest run` — all tests pass (if tests were added)

7. **Visual verification in browser.** This is critical for UI work:
   - Open the modified panel and verify it renders correctly
   - Check at different screen positions (tooltips near edges)
   - Verify Chinese text is not truncated
   - Compare styling with adjacent panels for consistency
   - Test keyboard shortcuts work

## Example Handoff

```json
{
  "salientSummary": "Created Achievement panel accessible via 'V' hotkey. Shows all 12+ achievements with Chinese names, descriptions, progress bars, locked/unlocked states, and reward info. Panel follows unified styling (dark bg, gold border, standard header). Unlock notification toast added. `npm run build` succeeds.",
  "whatWasImplemented": "New showAchievementPanel() method in UIScene.ts (120 lines). 12 achievement entries rendered with: icon area, Chinese name/description, progress bar (filled proportionally), reward text, locked/unlocked visual states (grey vs gold). Unlock toast: gold-bordered notification at top-center, auto-dismisses in 3s. Keyboard 'V' bound in ZoneScene, emits UI_TOGGLE_PANEL with panel:'achievement'. Mutual exclusion with other panels.",
  "whatWasLeftUndone": "",
  "verification": {
    "commandsRun": [
      { "command": "npx tsc --noEmit", "exitCode": 0, "observation": "No type errors" },
      { "command": "npm run build", "exitCode": 0, "observation": "Build succeeds" }
    ],
    "interactiveChecks": [
      { "action": "Pressed V key to open achievement panel", "observed": "Panel opened with 12 achievements listed, 3 unlocked (green/gold), 9 locked (grey). Progress bars show correct fill. Chinese text throughout." },
      { "action": "Pressed V again", "observed": "Panel closed. Opened inventory (I), then pressed V — inventory closed, achievement panel opened (mutual exclusion works)." },
      { "action": "Triggered a kill achievement", "observed": "Gold toast notification appeared at top center: '成就解锁: 初出茅庐!' — auto-dismissed after 3s." }
    ]
  },
  "tests": {
    "added": []
  },
  "discoveredIssues": []
}
```

## When to Return to Orchestrator

- Panel styling conflicts with another panel being modified by a different feature
- Required data/system API doesn't exist yet (e.g., achievement panel needs AchievementSystem.getAll() which doesn't return needed fields)
- Sprite generation changes would break existing textures in untestable ways
- DPR scaling produces unexpected results on the current display

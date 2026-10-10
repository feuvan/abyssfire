// Built-in VFX recipes: the web compositions of combat feedback, rewards, world dressing and the Chapter-1 skills
// (art-inventory-ch1.md 8.5-8.7, combat-feel.md 11.4, world-map-nav.md 14.2), translated to the recipe schema of
// Public/Vfx/AbyssVfxRecipe.h (px -> cm x 2.22, px/s -> cm/s x 2.22). Data/vfx_recipes.json (the art pipeline's file)
// replaces any of them by id. Each document stays well below the 16 KB MSVC string-literal limit.
//
// Binding of ids to events (UAbyssVfxSystem): see Public/World/WorldContract.md section 5.3.
#include "Vfx/AbyssVfxRecipe.h"

namespace AbyssVfxDefaultsPrivate
{
	// ---- combat feedback (combat-feel.md 11.4, art-inventory-ch1.md 8.5) ------------------------------------------------
	const char* const Combat = R"ABYSSJSON({
"schemaVersion": 1,
"sprites": { "Smoke": { "cells": 2 }, "Bolt": { "cells": 4 }, "Rock": { "cells": 2 }, "Slash": { "cells": 1 } },
"recipes": {
 "hit.impact": { "palette": "event", "layers": [
  { "emitter": "flash", "at": "target", "height": "chest", "offsetCm": [6.7, 0, 0], "unit": "ring", "sizeCm": 0.5, "grow": 2.6, "lifeMs": 100, "color": "white", "only": "small" },
  { "emitter": "flash", "at": "target", "height": "chest", "offsetCm": [6.7, 0, 0], "unit": "ring", "sizeCm": 0.5, "grow": 2.6, "lifeMs": 140, "color": "white", "only": "big" },
  { "emitter": "glow", "at": "target", "height": "chest", "unit": "ring", "sizeCm": 1.2, "grow": 2.17, "lifeMs": 160, "alpha": [0.45, 0], "color": "event", "only": "small" },
  { "emitter": "glow", "at": "target", "height": "chest", "unit": "ring", "sizeCm": 1.2, "grow": 2.17, "lifeMs": 220, "alpha": [0.45, 0], "color": "event", "only": "big" },
  { "emitter": "ring", "at": "target", "unit": "ring", "sizeCm": 0.6, "grow": 4.17, "lifeMs": 240, "alpha": [0.7, 0], "color": "event", "only": "small" },
  { "emitter": "ring", "at": "target", "unit": "ring", "sizeCm": 0.6, "grow": 5.67, "lifeMs": 320, "alpha": [0.7, 0], "color": "event", "only": "big" },
  { "emitter": "sparks", "at": "target", "height": "chest", "count": "sparks", "dir": "blow", "coneDeg": 52, "speedCmS": [355, 622], "lifeMs": [160, 280], "drag": 4, "gravityCmS2": 266, "altEvery": 2, "color": "event", "only": "small" },
  { "emitter": "sparks", "at": "target", "height": "chest", "count": "sparks", "dir": "blow", "coneDeg": 72, "speedCmS": [488, 932], "lifeMs": [160, 280], "drag": 4, "gravityCmS2": 266, "altEvery": 2, "color": "event", "only": "big" },
  { "emitter": "glint", "at": "target", "height": "chest", "sizeCm": 20, "lifeMs": 200, "rotationDeg": 45, "only": "big" }
 ] },
 "hit.basic": { "palette": "steel", "layers": [
  { "emitter": "slash", "at": "target", "height": "chest", "offsetCm": [-13, 0, 0], "sizeCm": 45, "lifeMs": 150, "color": "#AABBD4" },
  { "emitter": "sparks", "at": "target", "height": "chest", "count": 5, "dir": "blow", "coneDeg": 69, "speedCmS": [200, 377], "sizeCm": [9, 13], "color": "#FFF2C0" },
  { "emitter": "streak", "at": "target", "height": "chest", "count": 2, "sizeCm": 60, "aspect": 0.12, "lifeMs": [170, 220], "orient": "billboard", "rotationDeg": [-50, -30], "alignToBlow": true, "color": "#FFFFCC", "alpha": [0.9, 0] }
 ] },
 "hit.claw": { "palette": "blood", "layers": [
  { "sprite": "Claw", "orient": "billboard", "blend": "translucent", "at": "target", "height": "chest", "sizeCm": [90, 90], "grow": 1.25, "sizePow": 3, "lifeMs": 220, "alpha": [1, 0], "rotationDeg": [-20, 20], "speedCmS": 67, "dir": "down", "coneDeg": 0, "color": "#5A0A14" },
  { "sprite": "Claw", "orient": "billboard", "at": "target", "height": "chest", "sizeCm": 80, "grow": 1.15, "lifeMs": 180, "alpha": [1, 0], "rotationDeg": [-20, 20], "color": "#FF5A3A", "intensity": 1.4 },
  { "sprite": "Claw", "orient": "billboard", "at": "target", "height": "chest", "sizeCm": 70, "grow": 1.1, "lifeMs": 120, "alpha": [1, 0], "color": "white", "intensity": 1.5 },
  { "emitter": "sparks", "at": "target", "height": "chest", "count": 5, "dir": "down", "coneDeg": 92, "speedCmS": [133, 289], "sizeCm": [8, 11], "color": "#FF6A4A", "altEvery": 0 },
  { "emitter": "glow", "at": "target", "height": "chest", "sizeCm": 67, "grow": 1.6, "lifeMs": 160, "alpha": [0.5, 0], "color": "#FF3A2A" }
 ] },
 "hit.evade": { "layers": [
  { "emitter": "glint", "at": "target", "height": "chest", "sizeCm": 75, "lifeMs": 200 },
  { "emitter": "sparks", "at": "target", "height": "chest", "count": 6, "color": "#FFE9A0", "altEvery": 0 }
 ] },
 "hit.mark.fire": { "palette": "fire", "layers": [
  { "emitter": "decal", "at": "target", "sizeCm": 71, "lifeMs": 1800, "alpha": [0.6, 0] },
  { "emitter": "glow", "orient": "ground", "at": "target", "height": 4, "sizeCm": 53, "grow": 1.4, "lifeMs": 500, "color": "#FF6A1A" }
 ] },
 "hit.mark.ice": { "palette": "frost", "layers": [
  { "emitter": "decal", "sprite": "Frost", "blend": "additive", "at": "target", "sizeCm": 80, "lifeMs": 1600, "alpha": [0.6, 0], "color": "mid" }
 ] },
 "hit.mark.other": { "palette": "lightning", "layers": [
  { "emitter": "decal", "at": "target", "sizeCm": 62, "lifeMs": 1400, "alpha": [0.6, 0], "color": "#9A9AB8" },
  { "emitter": "decal", "sprite": "CrackGlow", "blend": "additive", "at": "target", "sizeCm": 71, "lifeMs": 360, "alpha": [0.9, 0], "color": "#8C8CFF" }
 ] },
 "death.burst": { "palette": "event", "layers": [
  { "emitter": "flash", "at": "target", "height": "chest", "sizeCm": 49, "grow": 2.4, "lifeMs": 170, "color": "core" },
  { "emitter": "smoke", "at": "target", "height": 20, "count": 7, "spawnRadiusCm": 27, "gravityCmS2": -40, "alpha": [0.8, 0], "color": "#5A5060" },
  { "emitter": "motes", "at": "target", "height": "chest", "count": 8, "gravityCmS2": -133, "color": "event", "altEvery": 2, "altColor": "white" },
  { "emitter": "ring", "at": "target", "sizeCm": 27, "grow": 5, "lifeMs": 360, "alpha": [0.7, 0], "color": "event" },
  { "emitter": "glow", "at": "target", "height": "chest", "delayMs": 80, "sizeCm": 30, "grow": 1.3, "lifeMs": 700, "upCmS": 100, "dir": "up", "coneDeg": 0, "alpha": [0.8, 0], "color": "core" }
 ] },
 "reward.gold": { "palette": "holy", "layers": [
  { "sprite": "Coin", "mesh": "SM_Pickup_GoldCoin", "orient": "billboard", "blend": "translucent", "at": "target", "height": 30, "count": 6, "dir": "up", "coneDeg": 126, "speedCmS": [111, 222], "upCmS": 155, "gravityCmS2": 933, "lifeMs": [420, 580], "sizeCm": 30, "grow": 1, "alpha": [1, 0], "alphaPow": 4, "spinDegS": [-540, 540], "color": "white" },
  { "emitter": "motes", "sprite": "Spark", "at": "target", "height": 40, "count": 5, "gravityCmS2": -67, "color": "mid", "altEvery": 2, "altColor": "core" }
 ] },
 "reward.heal": { "palette": "nature", "layers": [
  { "emitter": "glow", "at": "target", "height": "chest", "sizeCm": 111, "grow": 1.4, "lifeMs": 420, "alpha": [0.55, 0], "color": "rim" },
  { "emitter": "ring", "at": "target", "sizeCm": 27, "grow": 4.67, "lifeMs": 420, "color": "mid" },
  { "emitter": "motes", "sprite": "Plus", "at": "target", "height": "chest", "count": 4, "gravityCmS2": -133, "sizeCm": [12, 18], "color": "mid" },
  { "emitter": "motes", "at": "target", "height": "chest", "count": 10, "gravityCmS2": -155, "color": "core", "altEvery": 2, "altColor": "rim" }
 ] },
 "hero.levelup": { "palette": "holy", "shake": [100, 0.004], "layers": [
  { "emitter": "decal", "sprite": "Rune", "blend": "additive", "at": "target", "sizeCm": 178, "grow": 1.1, "lifeMs": 1100, "alpha": [0.9, 0], "spinDegS": 57, "color": "rim" },
  { "emitter": "beam", "at": "target", "sizeCm": 333, "aspect": 0.29, "grow": 1, "lifeMs": 900, "alpha": [0.85, 0], "color": "mid" },
  { "emitter": "beam", "at": "target", "sizeCm": 289, "aspect": 0.107, "grow": 1, "lifeMs": 700, "alpha": [0.9, 0], "color": "white" },
  { "emitter": "flash", "at": "target", "height": "chest", "sizeCm": 80, "grow": 2.4, "lifeMs": 200, "color": "core" },
  { "emitter": "shock", "at": "target", "sizeCm": 36, "grow": 8.75, "lifeMs": 480, "alpha": [0.6, 0], "color": "mid" },
  { "emitter": "ring", "at": "target", "delayMs": 120, "sizeCm": 44, "grow": 8, "lifeMs": 520, "color": "rim" },
  { "emitter": "motes", "sprite": "Spark", "at": "target", "height": "chest", "count": 16, "spawnRadiusCm": 45, "gravityCmS2": -200, "lifeMs": [700, 1000], "color": "core", "altEvery": 2, "altColor": "mid" },
  { "emitter": "motes", "at": "target", "delayMs": 150, "count": 10, "spawnRadiusCm": 40, "gravityCmS2": -266, "lifeMs": [600, 900], "color": "mid" }
 ] }
}
})ABYSSJSON";

	// ---- hero utility, loot, quests, statuses ---------------------------------------------------------------------------
	const char* const Utility = R"ABYSSJSON({
"schemaVersion": 1,
"recipes": {
 "hero.dodge": { "layers": [
  { "emitter": "smoke", "at": "origin", "height": 8, "count": 5, "spawnRadiusCm": 25, "sizeCm": [22, 34], "lifeMs": [380, 560], "alpha": [0.55, 0], "color": "#C8B89A" }
 ] },
 "hero.dash": { "palette": "holy", "layers": [
  { "emitter": "streak", "at": "path", "height": "chest", "count": 9, "spawnRadiusCm": 30, "speedCmS": 930, "dir": "blow", "coneDeg": 6, "lifeMs": 180, "sizeCm": 12, "color": "mid", "altEvery": 2, "altColor": "white", "delayMs": 0 },
  { "emitter": "smoke", "at": "path", "height": 8, "count": 5, "sizeCm": [22, 32], "lifeMs": [380, 560], "alpha": [0.5, 0], "color": "#C8B89A" }
 ] },
 "teleport.blink": { "palette": "arcane", "layers": [
  { "emitter": "glow", "orient": "upright", "at": "origin", "height": "chest", "sizeCm": 180, "aspect": 0.5, "grow": 0.1, "lifeMs": 200, "color": "mid" },
  { "emitter": "ring", "at": "origin", "sizeCm": 140, "grow": 0.12, "lifeMs": 220, "color": "mid" },
  { "emitter": "flash", "at": "point", "height": "chest", "delayMs": 80, "sizeCm": 66, "grow": 2.4, "color": "core" },
  { "emitter": "sparks", "at": "point", "height": "chest", "delayMs": 80, "count": 8, "color": "mid" }
 ] },
 "portal.channel": { "layers": [
  { "emitter": "ring", "at": "origin", "rate": 2.5, "durationMs": 1500, "attach": true, "sizeCm": 18, "grow": 7, "sizePow": 1.5, "lifeMs": 1200, "alpha": [0.7, 0], "color": "#4488FF" },
  { "emitter": "ring", "at": "origin", "rate": 2.5, "durationMs": 1300, "delayMs": 200, "attach": true, "sizeCm": 18, "grow": 5, "sizePow": 1.5, "lifeMs": 1200, "alpha": [0.5, 0], "color": "#66AAFF" },
  { "emitter": "glow", "orient": "ground", "at": "origin", "height": 3, "rate": 1.5, "durationMs": 1500, "attach": true, "sizeCm": 9, "grow": 15, "sizePow": 1.5, "lifeMs": 1200, "alpha": [0.3, 0], "color": "#2266CC" },
  { "emitter": "motes", "sprite": "Spark", "at": "origin", "rate": 14, "durationMs": 1500, "attach": true, "spawnRadiusCm": 55, "gravityCmS2": -180, "lifeMs": [500, 800], "color": "#9CC8FF" }
 ] },
 "portal.arrive": { "layers": [
  { "emitter": "flash", "at": "point", "height": "chest", "sizeCm": 80, "grow": 2.4, "lifeMs": 220, "color": "#B8D8FF" },
  { "emitter": "ring", "at": "point", "sizeCm": 30, "grow": 6, "lifeMs": 420, "color": "#4488FF" },
  { "emitter": "sparks", "at": "point", "height": "chest", "count": 10, "color": "#66AAFF" }
 ] },
 "loot.drop": { "palette": "event", "layers": [
  { "emitter": "flash", "at": "point", "height": 20, "sizeCm": 44, "grow": 2, "lifeMs": 220, "color": "event" },
  { "emitter": "ring", "at": "point", "sizeCm": 18, "grow": 6, "lifeMs": 420, "color": "event" }
 ] },
 "loot.drop.rare": { "palette": "event", "layers": [
  { "emitter": "beam", "at": "point", "sizeCm": 180, "aspect": 0.14, "grow": 1, "lifeMs": 900, "fadeIn": 0.25, "alpha": [0.55, 0], "color": "event" },
  { "emitter": "flash", "at": "point", "height": 20, "sizeCm": 44, "grow": 2, "lifeMs": 220, "color": "event" },
  { "emitter": "ring", "at": "point", "sizeCm": 18, "grow": 6, "lifeMs": 420, "color": "event" }
 ] },
 "loot.drop.legendary": { "palette": "event", "layers": [
  { "emitter": "beam", "at": "point", "sizeCm": 280, "aspect": 0.12, "grow": 1, "lifeMs": 1200, "fadeIn": 0.2, "alpha": [0.75, 0], "color": "event" },
  { "emitter": "beam", "at": "point", "sizeCm": 220, "aspect": 0.05, "grow": 1, "lifeMs": 1000, "fadeIn": 0.2, "alpha": [0.7, 0], "color": "white" },
  { "emitter": "flash", "at": "point", "height": 20, "sizeCm": 66, "grow": 2, "lifeMs": 220, "color": "event" },
  { "emitter": "ring", "at": "point", "sizeCm": 18, "grow": 9, "lifeMs": 420, "color": "event" },
  { "emitter": "motes", "sprite": "Spark", "at": "point", "count": 10, "spawnRadiusCm": 35, "gravityCmS2": -150, "lifeMs": [700, 1100], "color": "event", "altEvery": 2, "altColor": "white" }
 ] },
 "loot.pickup": { "palette": "holy", "layers": [
  { "emitter": "glint", "at": "point", "height": 25, "sizeCm": 40, "lifeMs": 200 },
  { "emitter": "sparks", "at": "point", "height": 25, "count": 5, "color": "mid" }
 ] },
 "quest.pop": { "layers": [
  { "emitter": "glow", "at": "point", "height": 40, "sizeCm": 45, "grow": 2.4, "lifeMs": 260, "color": "#FFE39A" },
  { "emitter": "sparks", "at": "point", "height": 40, "count": 6, "dir": "flat", "speedCmS": 155, "color": "#FFF1C0", "altEvery": 0 }
 ] },
 "status.burn": { "palette": "fire", "layers": [
  { "emitter": "flames", "at": "target", "height": "chest", "rate": 6, "attach": true, "spawnRadiusCm": 18, "spawnHeightJitterCm": 40, "sizeCm": [16, 26], "lifeMs": [260, 420] },
  { "emitter": "motes", "at": "target", "height": "chest", "rate": 5, "attach": true, "spawnRadiusCm": 22, "gravityCmS2": -160, "color": "rim" }
 ] },
 "status.poison": { "palette": "poison", "layers": [
  { "emitter": "motes", "sprite": "Bubble", "at": "target", "height": "chest", "rate": 5, "attach": true, "spawnRadiusCm": 24, "spawnHeightJitterCm": 40, "gravityCmS2": -90, "speedCmS": [10, 30], "sizeCm": [8, 14], "lifeMs": [600, 900], "color": "#8FE04A", "intensity": 1 }
 ] },
 "status.freeze": { "palette": "frost", "layers": [
  { "emitter": "motes", "sprite": "Flake", "at": "target", "height": "chest", "rate": 6, "attach": true, "spawnRadiusCm": 35, "spawnHeightJitterCm": 50, "gravityCmS2": 40, "speedCmS": [5, 20], "sizeCm": [8, 12], "lifeMs": [700, 1100], "color": "core" },
  { "emitter": "glow", "orient": "ground", "at": "target", "height": 3, "rate": 1.2, "attach": true, "sizeCm": 110, "grow": 1.1, "lifeMs": 900, "fadeIn": 0.3, "alpha": [0.35, 0], "color": "#7FD0FF" }
 ] },
 "status.stun": { "palette": "holy", "layers": [
  { "sprite": "Spark", "orient": "billboard", "at": "target", "height": "overhead", "count": 3, "rate": 1.5, "attach": true, "orbit": { "r0Cm": 26, "r1Cm": 26, "degS": 240 }, "sizeCm": 16, "grow": 1, "lifeMs": 680, "alpha": [1, 1], "fadeIn": 0.1, "color": "core" }
 ] },
 "status.slow": { "palette": "frost", "layers": [
  { "emitter": "ring", "at": "target", "rate": 1.6, "attach": true, "sizeCm": 90, "grow": 0.6, "sizePow": 1, "lifeMs": 700, "fadeIn": 0.3, "alpha": [0.4, 0], "color": "mid" }
 ] },
 "status.bleed": { "palette": "blood", "layers": [
  { "sprite": "Drop", "orient": "billboard", "at": "target", "height": "chest", "rate": 3, "attach": true, "spawnRadiusCm": 15, "gravityCmS2": 600, "sizeCm": [6, 9], "aspect": 0.7, "grow": 1, "lifeMs": [350, 500], "alpha": [1, 0.6], "color": "mid" }
 ] }
}
})ABYSSJSON";

	// ---- world: camps, exits, props, ambience ---------------------------------------------------------------------------
	const char* const World = R"ABYSSJSON({
"schemaVersion": 1,
"recipes": {
 "camp.campfire": { "palette": "fire", "light": { "radiusCm": 300, "alpha": 0.85, "flicker": true }, "layers": [
  { "emitter": "flames", "at": "origin", "height": 8, "rate": 14, "durationMs": -1, "spawnRadiusCm": 14, "sizeCm": [40, 62], "lifeMs": [420, 620], "upCmS": [50, 95], "color": "#FF8800" },
  { "emitter": "flames", "at": "origin", "height": 10, "rate": 10, "durationMs": -1, "spawnRadiusCm": 7, "sizeCm": [24, 36], "lifeMs": [300, 460], "upCmS": [60, 110], "color": "#FFE070" },
  { "emitter": "sparks", "at": "origin", "height": 30, "rate": 5.5, "durationMs": -1, "count": 1, "dir": "up", "coneDeg": 50, "speedCmS": [33, 100], "gravityCmS2": -40, "drag": 0.5, "lifeMs": [400, 900], "color": "#FFDD44", "altEvery": 2, "altColor": "#FF8800" },
  { "emitter": "glow", "at": "origin", "height": 40, "rate": 2.5, "durationMs": -1, "sizeCm": 90, "grow": 1.15, "lifeMs": 600, "fadeIn": 0.3, "alpha": [0.45, 0], "color": "#FF8800" }
 ] },
 "camp.torch": { "palette": "fire", "light": { "radiusCm": 180, "alpha": 0.65, "flicker": true }, "layers": [
  { "emitter": "flames", "at": "origin", "height": 160, "rate": 10, "durationMs": -1, "spawnRadiusCm": 5, "sizeCm": [20, 30], "lifeMs": [300, 460], "upCmS": [45, 85], "color": "#FF6600" },
  { "emitter": "motes", "at": "origin", "height": 175, "rate": 2, "durationMs": -1, "spawnRadiusCm": 6, "gravityCmS2": -120, "lifeMs": [500, 900], "color": "#FFB060" }
 ] },
 "exit.portal": { "palette": "arcane", "light": { "radiusCm": 260, "alpha": 0.6, "flicker": false }, "layers": [
  { "sprite": "Ring", "orient": "ground", "at": "origin", "height": 3, "rate": 1.2, "durationMs": -1, "sizeCm": 115, "grow": 1.05, "lifeMs": 1500, "fadeIn": 0.3, "alpha": [0.55, 0], "spinDegS": 30, "color": "#3FE0A0" },
  { "emitter": "glow", "orient": "upright", "at": "origin", "height": 110, "rate": 3, "durationMs": -1, "sizeCm": 170, "aspect": 0.7, "grow": 1.1, "lifeMs": 900, "fadeIn": 0.3, "alpha": [0.6, 0], "color": "#80F0FF" },
  { "sprite": "Rune", "orient": "upright", "at": "origin", "height": 110, "rate": 1.5, "durationMs": -1, "sizeCm": 150, "grow": 1, "lifeMs": 1400, "fadeIn": 0.4, "alpha": [0.5, 0], "spinDegS": 70, "color": "#DCFFF0" },
  { "emitter": "motes", "sprite": "Spark", "at": "origin", "height": 20, "rate": 6, "durationMs": -1, "spawnRadiusCm": 80, "gravityCmS2": -120, "lifeMs": [900, 1400], "color": "#B0FFE0" }
 ] },
 "exit.sealed": { "palette": "blood", "layers": [
  { "emitter": "glow", "orient": "upright", "at": "origin", "height": 110, "rate": 1.5, "durationMs": -1, "sizeCm": 180, "aspect": 0.7, "grow": 1.05, "lifeMs": 1400, "fadeIn": 0.4, "alpha": [0.55, 0], "color": "#6A2230" },
  { "sprite": "Rune", "orient": "ground", "at": "origin", "height": 3, "rate": 0.8, "durationMs": -1, "sizeCm": 120, "grow": 1, "lifeMs": 2000, "fadeIn": 0.4, "alpha": [0.45, 0], "color": "#A8344A" }
 ] },
 "prop.lore": { "palette": "holy", "layers": [
  { "emitter": "glow", "orient": "ground", "at": "origin", "height": 2, "rate": 1.3, "durationMs": -1, "attach": true, "sizeCm": 100, "grow": 1.2, "lifeMs": 1500, "fadeIn": 0.45, "alpha": [0.6, 0], "color": "event" },
  { "emitter": "motes", "sprite": "Spark", "at": "origin", "height": 30, "rate": 2, "durationMs": -1, "attach": true, "spawnRadiusCm": 30, "gravityCmS2": -60, "lifeMs": [700, 1100], "color": "event" }
 ] },
 "prop.gather": { "palette": "holy", "layers": [
  { "emitter": "glow", "orient": "ground", "at": "origin", "height": 2, "rate": 1.4, "durationMs": -1, "attach": true, "sizeCm": 70, "grow": 1.25, "lifeMs": 900, "fadeIn": 0.4, "alpha": [0.75, 0], "color": "#FFD98A" }
 ] },
 "prop.clue": { "palette": "frost", "layers": [
  { "emitter": "ring", "at": "origin", "rate": 0.9, "durationMs": -1, "attach": true, "sizeCm": 70, "grow": 1.3, "sizePow": 1, "lifeMs": 1100, "alpha": [0.7, 0.25], "color": "#8FE0FF" }
 ] },
 "prop.soul_echo": { "palette": "frost", "layers": [
  { "emitter": "ring", "at": "origin", "rate": 0.7, "durationMs": -1, "attach": true, "sizeCm": 90, "grow": 1.15, "sizePow": 1, "lifeMs": 1400, "fadeIn": 0.3, "alpha": [0.6, 0.25], "color": "#7FD8FF" },
  { "emitter": "motes", "at": "origin", "height": "chest", "rate": 3, "durationMs": -1, "attach": true, "spawnRadiusCm": 25, "gravityCmS2": -80, "color": "#9FE6FF" }
 ] },
 "prop.chest": { "palette": "holy", "layers": [
  { "emitter": "glint", "at": "origin", "height": 45, "rate": 1, "durationMs": -1, "attach": true, "spawnRadiusCm": 20, "sizeCm": 26, "lifeMs": 400, "color": "#FFE080" }
 ] },
 "prop.chest.open": { "palette": "holy", "layers": [
  { "emitter": "beam", "at": "origin", "height": 30, "sizeCm": 66, "aspect": 0.75, "grow": 1, "lifeMs": 900, "fadeIn": 0.2, "alpha": [0.55, 0], "color": "#FFF0B0" },
  { "emitter": "glow", "at": "origin", "height": 40, "sizeCm": 98, "grow": 1.4, "lifeMs": 600, "color": "#FFD870" },
  { "emitter": "motes", "sprite": "Spark", "at": "origin", "height": 40, "count": 10, "gravityCmS2": -150, "color": "core" }
 ] },
 "prop.event": { "palette": "frost", "layers": [
  { "emitter": "glow", "at": "origin", "height": 110, "rate": 1.5, "durationMs": -1, "attach": true, "sizeCm": 80, "grow": 1.2, "lifeMs": 1000, "fadeIn": 0.4, "alpha": [0.5, 0], "color": "#72D8FF" }
 ] },
 "ambient.pollen": { "layers": [
  { "emitter": "motes", "sprite": "Glow", "at": "camera", "rate": 2.4, "durationMs": -1, "spawnRadiusCm": 1500, "spawnHeightJitterCm": 260, "height": 60, "speedCmS": [2, 11], "gravityCmS2": 0, "drag": 0, "lifeMs": [8000, 14000], "sizeCm": [5, 8], "grow": 1.6, "fadeIn": 0.15, "alpha": [0.35, 0], "color": "#FFE8A0", "intensity": 1 }
 ] },
 "ambient.dust": { "layers": [
  { "emitter": "motes", "sprite": "Glow", "at": "camera", "rate": 1.25, "durationMs": -1, "spawnRadiusCm": 1500, "spawnHeightJitterCm": 120, "height": 30, "speedCmS": [4, 18], "dir": "up", "coneDeg": 140, "gravityCmS2": 0, "drag": 0, "lifeMs": [6000, 12000], "sizeCm": [6, 10], "grow": 1.9, "fadeIn": 0.15, "alpha": [0.15, 0], "color": "#88CC88", "intensity": 1 }
 ] },
 "ambient.wisps": { "layers": [
  { "emitter": "motes", "sprite": "Glow", "at": "camera", "rate": 1.2, "durationMs": -1, "spawnRadiusCm": 1500, "spawnHeightJitterCm": 200, "height": 80, "speedCmS": [8, 20], "gravityCmS2": -4, "drag": 0, "lifeMs": [5000, 9000], "sizeCm": [8, 14], "grow": 1.3, "fadeIn": 0.2, "alpha": [0.4, 0], "color": "#9ACBFF", "intensity": 1.2 }
 ] },
 "ambient.dust_motes": { "layers": [
  { "emitter": "motes", "sprite": "Glow", "at": "camera", "rate": 2.4, "durationMs": -1, "spawnRadiusCm": 1500, "spawnHeightJitterCm": 260, "height": 60, "speedCmS": [2, 11], "gravityCmS2": 0, "drag": 0, "lifeMs": [8000, 14000], "sizeCm": [5, 8], "grow": 1.6, "fadeIn": 0.15, "alpha": [0.3, 0], "color": "#DCE8F4", "intensity": 1 }
 ] },
 "ambient.sparks": { "layers": [
  { "emitter": "motes", "sprite": "Ember", "at": "camera", "rate": 3, "durationMs": -1, "spawnRadiusCm": 1500, "spawnHeightJitterCm": 200, "height": 30, "speedCmS": [10, 30], "dir": "up", "coneDeg": 90, "gravityCmS2": -20, "drag": 0, "lifeMs": [3000, 6000], "sizeCm": [5, 9], "fadeIn": 0.1, "alpha": [0.6, 0], "color": "#FF6A3A", "intensity": 1.5 }
 ] },
 "npc.FX_HammerStrike": { "palette": "fire", "layers": [
  { "emitter": "glow", "at": "origin", "height": "hand", "sizeCm": 32, "grow": 1.6, "lifeMs": 150, "alpha": [0.7, 0], "color": "#FFA040" },
  { "emitter": "sparks", "at": "origin", "height": "hand", "count": 6, "dir": "up", "coneDeg": 130, "speedCmS": [80, 160], "lifeMs": [120, 220], "sizeCm": [5, 8], "color": "#FFD070", "altEvery": 0 }
 ] }
}
})ABYSSJSON";

	// ---- projectiles and ground effects (EvProjectile*, EvGroundEffect*) ------------------------------------------------
	const char* const Projectiles = R"ABYSSJSON({
"schemaVersion": 1,
"aliases": { "proj.multishot": "proj.arrow", "proj.multishot.hit": "proj.arrow.hit", "proj.piercing_arrow": "proj.arrow",
 "proj.piercing_arrow.hit": "proj.arrow.hit", "ground.chain_trap": "ground.slow_trap" },
"recipes": {
 "proj.default": { "palette": "event", "layers": [
  { "emitter": "glow", "at": "origin", "rate": 60, "durationMs": -1, "attach": true, "sizeCm": 44, "grow": 0.6, "lifeMs": 90, "alpha": [0.9, 0], "color": "rim" },
  { "sprite": "Comet", "orient": "velocity", "at": "origin", "rate": 60, "durationMs": -1, "attach": true, "sizeCm": 34, "aspect": 0.4, "stretch": 0.05, "grow": 0.8, "lifeMs": 80, "alpha": [0.9, 0], "color": "mid", "intensity": 1.4 },
  { "emitter": "motes", "at": "origin", "rate": 30, "durationMs": -1, "speedCmS": [10, 40], "gravityCmS2": -60, "lifeMs": [200, 380], "color": "rim" }
 ] },
 "proj.default.hit": { "palette": "event", "layers": [
  { "emitter": "flash", "at": "point", "height": "chest", "sizeCm": 60, "grow": 2.4, "lifeMs": 170, "color": "core" },
  { "emitter": "sparks", "at": "point", "height": "chest", "count": 8, "color": "mid" },
  { "emitter": "ring", "at": "point", "sizeCm": 18, "grow": 6, "lifeMs": 320, "color": "rim" }
 ] },
 "proj.default.fizzle": { "palette": "event", "layers": [
  { "emitter": "glow", "at": "point", "height": "chest", "sizeCm": 40, "grow": 1.5, "lifeMs": 200, "alpha": [0.6, 0], "color": "mid" }
 ] },
 "proj.monster_bolt": { "palette": "event", "layers": [
  { "emitter": "glow", "at": "origin", "rate": 60, "durationMs": -1, "attach": true, "sizeCm": 40, "grow": 0.6, "lifeMs": 90, "alpha": [0.9, 0], "color": "event" },
  { "sprite": "Comet", "orient": "velocity", "at": "origin", "rate": 60, "durationMs": -1, "attach": true, "sizeCm": 30, "aspect": 0.4, "stretch": 0.05, "grow": 0.8, "lifeMs": 80, "alpha": [0.9, 0], "color": "core", "intensity": 1.4 },
  { "emitter": "motes", "at": "origin", "rate": 36, "durationMs": -1, "speedCmS": [10, 40], "gravityCmS2": -60, "lifeMs": [220, 380], "color": "event" }
 ] },
 "proj.monster_bolt.hit": { "palette": "event", "layers": [
  { "emitter": "flash", "at": "point", "height": "chest", "sizeCm": 53, "grow": 2.4, "lifeMs": 160, "color": "core" },
  { "emitter": "sparks", "at": "point", "height": "chest", "count": 7, "color": "event" },
  { "emitter": "ring", "at": "point", "sizeCm": 18, "grow": 5, "lifeMs": 300, "color": "event" },
  { "emitter": "smoke", "at": "point", "height": 40, "count": 2, "sizeCm": [20, 28], "color": "#4A3A36" }
 ] },
 "proj.pet_bolt": { "palette": "arcane", "layers": [
  { "emitter": "glow", "at": "origin", "rate": 60, "durationMs": -1, "attach": true, "sizeCm": 30, "grow": 0.6, "lifeMs": 90, "color": "event" },
  { "emitter": "motes", "sprite": "Spark", "at": "origin", "rate": 24, "durationMs": -1, "speedCmS": [5, 25], "gravityCmS2": 0, "lifeMs": [200, 320], "sizeCm": [5, 8], "color": "core" }
 ] },
 "proj.pet_bolt.hit": { "palette": "arcane", "layers": [
  { "emitter": "flash", "at": "point", "height": "chest", "sizeCm": 40, "grow": 2.2, "lifeMs": 150, "color": "core" },
  { "emitter": "sparks", "at": "point", "height": "chest", "count": 5, "color": "event" }
 ] },
 "proj.fireball": { "palette": "fire", "layers": [
  { "emitter": "glow", "at": "origin", "rate": 60, "durationMs": -1, "attach": true, "sizeCm": 70, "grow": 0.6, "lifeMs": 100, "alpha": [0.85, 0], "color": "rim" },
  { "sprite": "Comet", "orient": "velocity", "at": "origin", "rate": 60, "durationMs": -1, "attach": true, "sizeCm": 56, "aspect": 0.45, "stretch": 0.06, "grow": 0.8, "lifeMs": 90, "alpha": [0.95, 0], "color": "#FFA532", "intensity": 1.5 },
  { "emitter": "flash", "at": "origin", "rate": 60, "durationMs": -1, "attach": true, "sizeCm": 22, "grow": 1, "lifeMs": 60, "alpha": [1, 0.5], "color": "core" },
  { "emitter": "flames", "at": "origin", "rate": 40, "durationMs": -1, "spawnRadiusCm": 6, "sizeCm": [12, 18], "lifeMs": [200, 260], "upCmS": [40, 80], "color": "mid" },
  { "emitter": "motes", "at": "origin", "rate": 14, "durationMs": -1, "color": "rim" },
  { "emitter": "smoke", "at": "origin", "rate": 8, "durationMs": -1, "count": 1, "sizeCm": [14, 20], "lifeMs": [400, 600], "alpha": [0.5, 0], "color": "#4A3A36" }
 ] },
 "proj.fireball.launch": { "palette": "fire", "layers": [
  { "emitter": "flash", "at": "origin", "height": "hand", "sizeCm": 44, "grow": 2.4, "lifeMs": 160, "color": "mid" },
  { "emitter": "sparks", "at": "origin", "height": "hand", "count": 4, "color": "mid" }
 ] },
 "proj.fireball.hit": { "palette": "fire", "shake": [90, 0.003], "layers": [
  { "emitter": "flash", "at": "point", "height": "chest", "sizeCm": 75, "grow": 2.4, "lifeMs": 170, "color": "core" },
  { "emitter": "glow", "at": "point", "height": "chest", "sizeCm": 120, "grow": 1.7, "lifeMs": 320, "alpha": [0.6, 0], "color": "rim" },
  { "emitter": "shock", "at": "point", "sizeCm": 36, "grow": 5.75, "lifeMs": 380, "alpha": [0.6, 0], "color": "mid" },
  { "emitter": "flames", "at": "point", "height": 20, "count": 9, "dir": "flat", "speedCmS": [200, 311], "sizeCm": [16, 24], "lifeMs": [300, 420], "color": "mid" },
  { "emitter": "sparks", "at": "point", "height": "chest", "count": 10, "gravityCmS2": 355, "color": "mid" },
  { "emitter": "motes", "at": "point", "height": "chest", "count": 8, "color": "rim" },
  { "emitter": "smoke", "at": "point", "height": 30, "delayMs": 90, "count": 4, "color": "#4A3A36" },
  { "emitter": "decal", "at": "point", "sizeCm": 80, "lifeMs": 1400, "alpha": [0.7, 0] }
 ] },
 "proj.ice_arrow": { "palette": "frost", "layers": [
  { "emitter": "glow", "orient": "velocity", "at": "origin", "rate": 60, "durationMs": -1, "attach": true, "sizeCm": 50, "aspect": 0.5, "grow": 0.6, "lifeMs": 90, "color": "rim" },
  { "sprite": "ShardFrost", "orient": "velocity", "at": "origin", "rate": 60, "durationMs": -1, "attach": true, "sizeCm": 18, "aspect": 0.35, "stretch": 0.02, "grow": 1, "lifeMs": 50, "alpha": [1, 1], "color": "white", "intensity": 1.2 },
  { "emitter": "motes", "sprite": "Flake", "at": "origin", "rate": 30, "durationMs": -1, "speedCmS": [5, 30], "gravityCmS2": 30, "lifeMs": [250, 420], "color": "core" }
 ] },
 "proj.ice_arrow.hit": { "palette": "frost", "layers": [
  { "emitter": "flash", "at": "point", "height": "chest", "sizeCm": 53, "grow": 2.4, "lifeMs": 160, "color": "core" },
  { "sprite": "ShardFrost", "orient": "velocity", "blend": "translucent", "at": "point", "height": "chest", "count": 7, "dir": "back", "coneDeg": 120, "speedCmS": [222, 377], "gravityCmS2": 400, "lifeMs": [300, 450], "sizeCm": 12, "aspect": 0.4, "grow": 1, "alpha": [1, 0], "alphaPow": 3, "color": "white" },
  { "emitter": "ring", "at": "point", "sizeCm": 18, "grow": 6, "lifeMs": 300, "color": "mid" },
  { "emitter": "decal", "sprite": "Frost", "blend": "additive", "at": "point", "sizeCm": 71, "lifeMs": 1000, "alpha": [0.6, 0], "color": "mid" }
 ] },
 "proj.poison_arrow": { "palette": "poison", "layers": [
  { "sprite": "Arrow", "mesh": "SM_FX_Arrow", "orient": "velocity", "at": "origin", "rate": 60, "durationMs": -1, "attach": true, "sizeCm": 50, "aspect": 0.25, "grow": 1, "lifeMs": 40, "alpha": [1, 1], "color": "white" },
  { "sprite": "Comet", "orient": "velocity", "at": "origin", "rate": 60, "durationMs": -1, "attach": true, "sizeCm": 36, "aspect": 0.35, "stretch": 0.05, "grow": 0.8, "lifeMs": 80, "alpha": [0.8, 0], "color": "mid" },
  { "sprite": "Drop", "orient": "billboard", "at": "origin", "rate": 20, "durationMs": -1, "gravityCmS2": 500, "sizeCm": [5, 8], "aspect": 0.7, "grow": 1, "lifeMs": [250, 400], "alpha": [1, 0], "color": "rim" }
 ] },
 "proj.poison_arrow.hit": { "palette": "poison", "layers": [
  { "emitter": "flash", "at": "point", "height": "chest", "sizeCm": 44, "grow": 2.4, "lifeMs": 160, "color": "mid" },
  { "sprite": "Drop", "orient": "billboard", "at": "point", "height": "chest", "count": 8, "dir": "up", "coneDeg": 120, "speedCmS": [150, 260], "gravityCmS2": 844, "sizeCm": [6, 9], "aspect": 0.7, "grow": 1, "lifeMs": [350, 500], "alpha": [1, 0], "color": "rim" },
  { "emitter": "smoke", "at": "point", "height": 30, "count": 3, "color": "#7AC84A", "alpha": [0.6, 0] },
  { "emitter": "decal", "sprite": "Puddle", "at": "point", "sizeCm": 62, "grow": 1.3, "lifeMs": 1200, "alpha": [0.7, 0], "color": "rim" }
 ] },
 "proj.arrow": { "palette": "steel", "layers": [
  { "sprite": "Arrow", "mesh": "SM_FX_Arrow", "orient": "velocity", "at": "origin", "rate": 60, "durationMs": -1, "attach": true, "sizeCm": 50, "aspect": 0.25, "grow": 1, "lifeMs": 40, "alpha": [1, 1], "color": "white" },
  { "sprite": "Comet", "orient": "velocity", "at": "origin", "rate": 60, "durationMs": -1, "attach": true, "sizeCm": 36, "aspect": 0.35, "stretch": 0.05, "grow": 0.8, "lifeMs": 80, "alpha": [0.8, 0], "color": "#EEF3FB" }
 ] },
 "proj.arrow.hit": { "palette": "steel", "layers": [
  { "emitter": "glint", "at": "point", "height": "chest", "sizeCm": 30, "lifeMs": 160 }
 ] },
 "ground.default": { "palette": "element", "layers": [
  { "sprite": "Rune", "orient": "ground", "at": "origin", "height": 3, "unit": "radius", "rate": 1.2, "durationMs": -1, "attach": true, "sizeCm": 2, "grow": 1.05, "lifeMs": 1200, "fadeIn": 0.3, "alpha": [0.5, 0], "spinDegS": 25, "color": "rim" },
  { "emitter": "motes", "at": "origin", "unit": "radius", "rate": 10, "durationMs": -1, "attach": true, "spawnRadiusCm": 1, "gravityCmS2": -90, "color": "mid" }
 ] },
 "ground.default.trigger": { "palette": "element", "layers": [
  { "emitter": "flash", "at": "origin", "height": 30, "sizeCm": 88, "grow": 2.4, "lifeMs": 180, "color": "core" },
  { "emitter": "shock", "at": "origin", "unit": "radius", "sizeCm": 0.3, "grow": 6.7, "lifeMs": 380, "color": "mid" },
  { "emitter": "sparks", "at": "origin", "height": 30, "count": 12, "color": "mid" }
 ] },
 "ground.default.end": { "palette": "element", "layers": [
  { "emitter": "ring", "at": "origin", "unit": "radius", "sizeCm": 1.8, "grow": 1.2, "lifeMs": 400, "alpha": [0.4, 0], "color": "mid" }
 ] },
 "ground.fire_wall": { "palette": "fire", "light": { "radiusCm": 260, "alpha": 0.5, "flicker": true }, "layers": [
  { "emitter": "flames", "at": "origin", "unit": "radius", "rate": 45, "durationMs": -1, "attach": true, "spawnRadiusCm": 1, "onRing": true, "sizeCm": [0.25, 0.4], "lifeMs": [420, 700], "upCmS": [70, 140], "color": "mid" },
  { "emitter": "flames", "at": "origin", "unit": "radius", "rate": 20, "durationMs": -1, "attach": true, "spawnRadiusCm": 0.95, "sizeCm": [0.16, 0.26], "lifeMs": [300, 500], "upCmS": [60, 110], "color": "rim" },
  { "emitter": "motes", "at": "origin", "unit": "radius", "rate": 10, "durationMs": -1, "attach": true, "spawnRadiusCm": 1, "gravityCmS2": -150, "color": "rim" },
  { "emitter": "decal", "at": "origin", "unit": "radius", "rate": 0.7, "durationMs": -1, "attach": true, "sizeCm": 2.3, "lifeMs": 1700, "alpha": [0.45, 0] }
 ] },
 "ground.explosive_trap": { "palette": "fire", "layers": [
  { "sprite": "Rune", "orient": "ground", "at": "origin", "height": 3, "rate": 1.5, "durationMs": -1, "attach": true, "sizeCm": 80, "grow": 1, "lifeMs": 900, "fadeIn": 0.3, "alpha": [0.7, 0], "spinDegS": 120, "color": "rim" }
 ] },
 "ground.explosive_trap.trigger": { "palette": "fire", "shake": [200, 0.009], "layers": [
  { "emitter": "flash", "at": "origin", "height": 40, "sizeCm": 88, "grow": 2.4, "lifeMs": 200, "color": "core" },
  { "emitter": "glow", "at": "origin", "height": 40, "unit": "radius", "sizeCm": 1.6, "grow": 1.4, "lifeMs": 360, "alpha": [0.7, 0], "color": "rim" },
  { "emitter": "shock", "at": "origin", "unit": "radius", "sizeCm": 0.16, "grow": 12.5, "lifeMs": 380, "alpha": [1, 0], "color": "mid" },
  { "emitter": "flames", "at": "origin", "unit": "radius", "count": 12, "spawnRadiusCm": 0.4, "sizeCm": [0.12, 0.2], "lifeMs": [360, 520], "upCmS": [155, 290], "color": "mid" },
  { "emitter": "debris", "at": "origin", "count": 8 },
  { "emitter": "sparks", "at": "origin", "height": 30, "count": 12, "gravityCmS2": 488, "color": "mid" },
  { "emitter": "smoke", "at": "origin", "height": 30, "delayMs": 90, "count": 7, "color": "#4A3A36" },
  { "emitter": "decal", "at": "origin", "unit": "radius", "sizeCm": 1, "lifeMs": 1800, "alpha": [0.7, 0] }
 ] },
 "ground.slow_trap": { "palette": "frost", "layers": [
  { "sprite": "Rune", "orient": "ground", "at": "origin", "height": 3, "rate": 1.5, "durationMs": -1, "attach": true, "sizeCm": 71, "grow": 1, "lifeMs": 900, "fadeIn": 0.3, "alpha": [0.7, 0], "spinDegS": 150, "color": "rim" }
 ] },
 "ground.slow_trap.trigger": { "palette": "frost", "layers": [
  { "emitter": "flash", "at": "origin", "height": 30, "sizeCm": 58, "grow": 2.4, "lifeMs": 180, "color": "mid" },
  { "emitter": "decal", "sprite": "Frost", "blend": "additive", "at": "origin", "unit": "radius", "sizeCm": 1.6, "lifeMs": 1500, "alpha": [0.6, 0], "color": "mid" },
  { "emitter": "ring", "at": "origin", "unit": "radius", "count": 3, "sizeCm": 0.16, "grow": 12.5, "sizePow": 2, "lifeMs": [520, 820], "alpha": [0.75, 0], "color": "rim" },
  { "emitter": "motes", "sprite": "Flake", "at": "origin", "height": 30, "count": 10, "spawnRadiusCm": 60, "gravityCmS2": 40, "color": "core" }
 ] },
 "ground.arrow_rain": { "palette": "steel", "layers": [
  { "sprite": "Arrow", "mesh": "SM_FX_Arrow", "orient": "velocity", "at": "origin", "unit": "radius", "rate": 30, "durationMs": -1, "attach": true, "spawnRadiusCm": 1, "spawnHeightJitterCm": 30, "height": 260, "speedCmS": 1700, "dir": "down", "coneDeg": 10, "lifeMs": 130, "sizeCm": 0.45, "aspect": 0.25, "grow": 1, "alpha": [1, 1], "color": "white" },
  { "emitter": "smoke", "at": "origin", "unit": "radius", "rate": 8, "durationMs": -1, "attach": true, "spawnRadiusCm": 1, "sizeCm": [0.12, 0.18], "lifeMs": [300, 480], "alpha": [0.45, 0], "color": "#C8B89A" }
 ] }
}
})ABYSSJSON";

	// ---- Chapter-1 skills (EvSkillVfx, art-inventory-ch1.md 8.6) --------------------------------------------------------
	const char* const SkillsA = R"ABYSSJSON({
"schemaVersion": 1,
"recipes": {
 "skill.default": { "palette": "element", "layers": [
  { "emitter": "flash", "at": "point", "height": "chest", "sizeCm": 53, "grow": 2.4, "lifeMs": 160, "color": "core" },
  { "emitter": "sparks", "at": "point", "height": "chest", "count": 8, "color": "mid" },
  { "emitter": "ring", "at": "point", "sizeCm": 18, "grow": 6, "lifeMs": 360, "color": "rim" }
 ] },
 "skill.slash": { "palette": "holy", "layers": [
  { "emitter": "swipe", "at": "origin", "height": "chest", "offsetCm": [70, 0, 0], "sizeCm": 110, "lifeMs": 230, "color": "rim" },
  { "emitter": "swipe", "at": "origin", "height": "chest", "offsetCm": [70, 0, 0], "delayMs": 25, "sizeCm": 88, "lifeMs": 200, "color": "#EEF3FB" },
  { "emitter": "flash", "at": "target", "height": "chest", "delayMs": 40, "sizeCm": 36, "grow": 2.4, "lifeMs": 120, "color": "mid" },
  { "emitter": "sparks", "at": "target", "height": "chest", "delayMs": 40, "count": 8, "dir": "blow", "coneDeg": 80, "speedCmS": [244, 488], "color": "mid" },
  { "emitter": "glint", "at": "target", "height": "chest", "delayMs": 40, "sizeCm": 58, "lifeMs": 180 }
 ] },
 "skill.whirlwind": { "palette": "steel", "layers": [
  { "emitter": "swipe", "at": "origin", "height": "chest", "unit": "radius", "count": 3, "sizeCm": 0.62, "lifeMs": 200, "rotationDeg": [0, 360], "spinDegS": 900, "alignToBlow": false, "color": "rim" },
  { "emitter": "swipe", "at": "origin", "height": "chest", "unit": "radius", "delayMs": 30, "count": 2, "sizeCm": 0.48, "lifeMs": 200, "rotationDeg": [0, 360], "spinDegS": -900, "alignToBlow": false, "color": "#F4AC28" },
  { "emitter": "streak", "at": "origin", "height": "chest", "count": 10, "dir": "tangent", "speedCmS": 400, "spawnRadiusCm": 80, "lifeMs": 200, "color": "white" },
  { "emitter": "ring", "at": "origin", "unit": "radius", "sizeCm": 0.8, "grow": 3.25, "lifeMs": 420, "alpha": [0.7, 0], "color": "mid" },
  { "emitter": "shock", "at": "origin", "unit": "radius", "delayMs": 120, "sizeCm": 0.6, "grow": 5, "lifeMs": 480, "alpha": [0.4, 0], "color": "mid" },
  { "emitter": "smoke", "at": "origin", "unit": "radius", "count": 8, "spawnRadiusCm": 1, "onRing": true, "height": 10, "sizeCm": [0.16, 0.24], "alpha": [0.5, 0], "color": "#C8B89A" }
 ] },
 "skill.shield_wall": { "palette": "holy", "layers": [
  { "emitter": "gather", "at": "origin", "count": 10, "lifeMs": 140, "color": "mid" },
  { "emitter": "decal", "sprite": "Rune", "blend": "additive", "at": "origin", "delayMs": 60, "sizeCm": 178, "grow": 1, "lifeMs": 900, "spinDegS": 34, "alpha": [0.8, 0], "color": "rim" },
  { "sprite": "Hex", "mesh": "SM_FX_HexPlate", "orient": "billboard", "blend": "translucent", "at": "origin", "height": "chest", "delayMs": 100, "count": 8, "orbit": { "r0Cm": 80, "r1Cm": 80, "degS": 0 }, "sizeCm": 15, "grow": 5.7, "sizePow": 5, "lifeMs": 780, "alpha": [1, 0], "alphaPow": 4, "color": "mid" },
  { "emitter": "flash", "at": "origin", "height": "chest", "delayMs": 110, "sizeCm": 58, "grow": 2.4, "color": "core" },
  { "emitter": "beam", "at": "origin", "delayMs": 100, "sizeCm": 200, "aspect": 0.44, "grow": 1, "lifeMs": 520, "alpha": [0.4, 0], "color": "rim" },
  { "emitter": "motes", "sprite": "Spark", "at": "origin", "count": 12, "spawnRadiusCm": 60, "gravityCmS2": -90, "color": "core" },
  { "emitter": "ring", "at": "origin", "sizeCm": 71, "grow": 3.5, "lifeMs": 420, "color": "mid" }
 ] },
 "skill.iron_fortress": { "palette": "steel", "layers": [
  { "emitter": "gather", "at": "origin", "count": 10, "lifeMs": 140, "color": "mid" },
  { "emitter": "decal", "sprite": "Rune", "blend": "additive", "at": "origin", "sizeCm": 169, "grow": 1, "lifeMs": 900, "spinDegS": -29, "alpha": [0.8, 0], "color": "rim" },
  { "sprite": "Hex", "mesh": "SM_FX_HexPlate", "orient": "billboard", "blend": "translucent", "at": "origin", "height": "chest", "delayMs": 100, "count": 6, "orbit": { "r0Cm": 89, "r1Cm": 89, "degS": 0 }, "sizeCm": 15, "grow": 5.7, "sizePow": 5, "lifeMs": 800, "alpha": [1, 0], "alphaPow": 4, "color": "#7F97BB" },
  { "sprite": "Hex", "mesh": "SM_FX_HexPlate", "orient": "billboard", "blend": "translucent", "at": "origin", "height": "head", "delayMs": 160, "count": 6, "orbit": { "r0Cm": 67, "r1Cm": 67, "degS": 0 }, "sizeCm": 15, "grow": 5.7, "sizePow": 5, "lifeMs": 800, "alpha": [1, 0], "alphaPow": 4, "color": "#9FB4D4" },
  { "emitter": "flash", "at": "origin", "height": "chest", "delayMs": 170, "sizeCm": 58, "grow": 2.4, "color": "mid" },
  { "emitter": "ring", "at": "origin", "delayMs": 170, "sizeCm": 62, "grow": 3.6, "lifeMs": 420, "color": "mid" },
  { "emitter": "sparks", "at": "origin", "height": "chest", "delayMs": 170, "count": 8, "color": "mid" }
 ] },
 "skill.taunt_roar": { "palette": "rage", "layers": [
  { "emitter": "glow", "at": "origin", "height": "head", "sizeCm": 75, "grow": 2, "lifeMs": 260, "color": "rim" },
  { "emitter": "slash", "orient": "ground", "at": "origin", "height": "chest", "count": 8, "dir": "out", "coneDeg": 0, "speedCmS": 377, "drag": 2, "sizeCm": 25, "grow": 1.9, "lifeMs": 360, "alignToBlow": false, "color": "rim" },
  { "emitter": "slash", "orient": "ground", "at": "origin", "height": "chest", "delayMs": 110, "count": 8, "dir": "out", "coneDeg": 0, "speedCmS": 377, "drag": 2, "sizeCm": 25, "grow": 1.9, "lifeMs": 360, "alignToBlow": false, "color": "mid" },
  { "emitter": "ring", "at": "origin", "unit": "radius", "sizeCm": 0.1, "grow": 20, "lifeMs": 480, "color": "rim" },
  { "emitter": "ring", "at": "origin", "unit": "radius", "delayMs": 110, "sizeCm": 0.07, "grow": 23, "lifeMs": 440, "color": "mid" },
  { "emitter": "sparks", "at": "origin", "height": "chest", "count": 10, "color": "mid" }
 ] },
 "skill.charge": { "palette": "holy", "shake": [150, 0.008], "layers": [
  { "emitter": "streak", "at": "path", "height": "chest", "count": 9, "spawnRadiusCm": 25, "speedCmS": 930, "dir": "blow", "coneDeg": 4, "lifeMs": 200, "sizeCm": 14, "color": "mid", "altEvery": 2, "altColor": "white" },
  { "emitter": "smoke", "at": "path", "height": 8, "count": 5, "sizeCm": [24, 34], "alpha": [0.5, 0], "color": "#C8B89A" },
  { "emitter": "flash", "at": "target", "height": "chest", "delayMs": 70, "sizeCm": 58, "grow": 2.4, "color": "mid" },
  { "emitter": "sparks", "at": "target", "height": "chest", "delayMs": 70, "count": 12, "coneDeg": 103, "dir": "blow", "speedCmS": [311, 577], "color": "mid" },
  { "emitter": "shock", "at": "target", "delayMs": 70, "sizeCm": 36, "grow": 5.75, "lifeMs": 400, "color": "#E8C47A" },
  { "emitter": "glint", "at": "target", "height": "chest", "delayMs": 70, "sizeCm": 89, "lifeMs": 200 },
  { "emitter": "debris", "at": "target", "delayMs": 70, "count": 4 }
 ] },
 "skill.frenzy": { "palette": "rage", "layers": [
  { "emitter": "glow", "at": "origin", "height": "chest", "sizeCm": 133, "grow": 1.6, "lifeMs": 260, "color": "rim" },
  { "emitter": "ring", "at": "origin", "sizeCm": 44, "grow": 4.6, "lifeMs": 320, "color": "rim" },
  { "emitter": "glow", "at": "origin", "height": "chest", "delayMs": 150, "sizeCm": 133, "grow": 1.6, "lifeMs": 260, "color": "rim" },
  { "emitter": "ring", "at": "origin", "delayMs": 150, "sizeCm": 44, "grow": 4.6, "lifeMs": 320, "color": "mid" },
  { "emitter": "flames", "at": "origin", "count": 10, "spawnRadiusCm": 58, "onRing": true, "upCmS": [133, 222], "color": "mid" },
  { "emitter": "sparks", "at": "origin", "height": "chest", "count": 10, "dir": "up", "coneDeg": 70, "color": "mid" },
  { "emitter": "motes", "at": "origin", "height": "chest", "count": 8, "gravityCmS2": -244, "color": "rim" }
 ] },
 "skill.bleed_strike": { "palette": "blood", "layers": [
  { "emitter": "swipe", "at": "origin", "height": "chest", "offsetCm": [70, 0, 0], "sizeCm": 105, "lifeMs": 230, "rotationDeg": 180, "blend": "translucent", "color": "mid" },
  { "emitter": "swipe", "at": "origin", "height": "chest", "offsetCm": [70, 0, 0], "delayMs": 25, "sizeCm": 84, "lifeMs": 200, "rotationDeg": 180, "color": "rim" },
  { "emitter": "flash", "at": "target", "height": "chest", "delayMs": 40, "sizeCm": 40, "grow": 2.4, "color": "mid" },
  { "sprite": "Drop", "orient": "billboard", "at": "target", "height": "chest", "count": 10, "dir": "blow", "coneDeg": 80, "speedCmS": [178, 377], "gravityCmS2": 933, "sizeCm": [6, 9], "aspect": 0.7, "grow": 1, "lifeMs": [300, 450], "alpha": [1, 0], "alphaPow": 3, "color": "mid" },
  { "emitter": "decal", "sprite": "Puddle", "at": "target", "delayMs": 200, "sizeCm": 49, "grow": 1.4, "lifeMs": 1300, "alpha": [0.7, 0], "color": "rim" }
 ] },
 "skill.unyielding": { "palette": "holy", "layers": [
  { "emitter": "decal", "sprite": "Crack", "at": "origin", "sizeCm": 115, "lifeMs": 900, "alpha": [0.6, 0] },
  { "emitter": "shock", "at": "origin", "sizeCm": 44, "grow": 4.4, "lifeMs": 400, "color": "#E8C47A" },
  { "emitter": "debris", "at": "origin", "count": 8, "upCmS": [100, 140], "gravityCmS2": 0, "lifeMs": 620, "orbit": { "r0Cm": 53, "r1Cm": 53, "degS": 180 } },
  { "emitter": "beam", "at": "origin", "sizeCm": 142, "aspect": 0.47, "grow": 1, "lifeMs": 500, "alpha": [0.55, 0], "color": "rim" },
  { "emitter": "glow", "at": "origin", "height": "chest", "sizeCm": 142, "grow": 1.4, "lifeMs": 400, "color": "mid" },
  { "emitter": "glint", "at": "origin", "height": "chest", "sizeCm": 60 }
 ] },
 "skill.life_regen": { "palette": "nature", "layers": [
  { "emitter": "glow", "orient": "ground", "at": "origin", "height": 3, "sizeCm": 115, "grow": 1.2, "lifeMs": 700, "alpha": [0.6, 0], "color": "rim" },
  { "emitter": "ring", "at": "origin", "sizeCm": 36, "grow": 4, "lifeMs": 420, "color": "mid" },
  { "emitter": "motes", "sprite": "Plus", "at": "origin", "height": "chest", "count": 6, "gravityCmS2": -111, "sizeCm": [12, 18], "color": "mid" },
  { "emitter": "motes", "at": "origin", "height": "chest", "count": 10, "gravityCmS2": -155, "color": "core" }
 ] },
 "skill.war_stomp": { "palette": "earth", "shake": [240, 0.011], "layers": [
  { "emitter": "decal", "sprite": "Crack", "at": "origin", "unit": "radius", "sizeCm": 1.1, "lifeMs": 1400, "alpha": [0.7, 0] },
  { "emitter": "decal", "sprite": "CrackGlow", "blend": "additive", "at": "origin", "unit": "radius", "sizeCm": 1.1, "lifeMs": 600, "alpha": [0.9, 0], "color": "#FF9A3A" },
  { "emitter": "shock", "at": "origin", "unit": "radius", "sizeCm": 0.3, "grow": 6.5, "lifeMs": 420, "color": "mid" },
  { "emitter": "ring", "at": "origin", "unit": "radius", "sizeCm": 0.4, "grow": 5, "lifeMs": 460, "color": "rim" },
  { "emitter": "ring", "at": "origin", "unit": "radius", "delayMs": 100, "sizeCm": 0.3, "grow": 6.5, "lifeMs": 520, "color": "mid" },
  { "emitter": "debris", "at": "origin", "count": 10, "spawnRadiusCm": 60 },
  { "emitter": "smoke", "at": "origin", "unit": "radius", "count": 10, "spawnRadiusCm": 0.8, "height": 10, "sizeCm": [0.16, 0.26], "alpha": [0.55, 0], "color": "#C8B89A" },
  { "emitter": "sparks", "at": "origin", "height": 30, "count": 8, "color": "mid" }
 ] }
}
})ABYSSJSON";

	const char* const SkillsB = R"ABYSSJSON({
"schemaVersion": 1,
"recipes": {
 "skill.blizzard": { "palette": "frost", "layers": [
  { "emitter": "decal", "sprite": "Frost", "blend": "additive", "at": "point", "unit": "radius", "sizeCm": 1.7, "grow": 1.08, "lifeMs": 1300, "fadeIn": 0.15, "alpha": [0.75, 0], "color": "mid" },
  { "emitter": "ring", "at": "point", "unit": "radius", "sizeCm": 0.8, "grow": 2.5, "lifeMs": 500, "color": "rim" },
  { "sprite": "ShardFrost", "orient": "velocity", "at": "point", "unit": "radius", "count": 9, "spawnRadiusCm": 0.85, "height": 240, "speedCmS": 1500, "dir": "down", "coneDeg": 25, "lifeMs": [150, 190], "sizeCm": 0.1, "aspect": 0.4, "grow": 1, "alpha": [1, 1], "color": "white" },
  { "sprite": "ShardFrost", "orient": "velocity", "at": "point", "unit": "radius", "delayMs": 180, "count": 9, "spawnRadiusCm": 0.85, "height": 240, "speedCmS": 1500, "dir": "down", "coneDeg": 25, "lifeMs": [150, 190], "sizeCm": 0.1, "aspect": 0.4, "grow": 1, "alpha": [1, 1], "color": "white" },
  { "sprite": "ShardFrost", "orient": "velocity", "at": "point", "unit": "radius", "delayMs": 360, "count": 8, "spawnRadiusCm": 0.85, "height": 240, "speedCmS": 1500, "dir": "down", "coneDeg": 25, "lifeMs": [150, 190], "sizeCm": 0.1, "aspect": 0.4, "grow": 1, "alpha": [1, 1], "color": "white" },
  { "emitter": "motes", "sprite": "Flake", "at": "point", "unit": "radius", "count": 16, "spawnRadiusCm": 0.9, "height": 120, "gravityCmS2": 60, "speedCmS": [10, 40], "lifeMs": [700, 1100], "color": "core" },
  { "emitter": "smoke", "at": "point", "unit": "radius", "delayMs": 200, "count": 6, "spawnRadiusCm": 0.7, "sizeCm": [0.3, 0.4], "alpha": [0.35, 0], "color": "#CFEFFF" }
 ] },
 "skill.ice_armor": { "palette": "frost", "layers": [
  { "emitter": "decal", "sprite": "Frost", "blend": "additive", "at": "origin", "sizeCm": 133, "lifeMs": 900, "alpha": [0.6, 0], "color": "mid" },
  { "sprite": "ShardFrost", "orient": "upright", "at": "origin", "count": 9, "spawnRadiusCm": 50, "onRing": true, "sizeCm": 44, "aspect": 0.5, "grow": 1.4, "sizePow": 6, "lifeMs": 520, "alpha": [1, 0], "alphaPow": 4, "color": "white" },
  { "emitter": "flash", "at": "origin", "height": "chest", "delayMs": 60, "sizeCm": 58, "grow": 2.4, "color": "mid" },
  { "emitter": "ring", "at": "origin", "sizeCm": 44, "grow": 4.2, "lifeMs": 420, "color": "rim" },
  { "emitter": "motes", "sprite": "Flake", "at": "origin", "height": "chest", "delayMs": 420, "count": 8, "color": "core" }
 ] },
 "skill.chain_lightning": { "palette": "lightning", "layers": [
  { "emitter": "flash", "at": "origin", "height": "hand", "sizeCm": 44, "grow": 2.4, "color": "mid" },
  { "emitter": "bolt", "at": "points", "beamTo": "points", "sizeCm": 18, "color": "#CFE0FF" },
  { "emitter": "bolt", "at": "points", "beamTo": "points", "delayMs": 80, "sizeCm": 12, "color": "#8C8CFF" },
  { "emitter": "flash", "at": "points", "height": "chest", "sizeCm": 40, "grow": 2.4, "color": "core" },
  { "emitter": "sparks", "at": "points", "height": "chest", "count": 7, "color": "rim", "altEvery": 2, "altColor": "core" },
  { "emitter": "ring", "at": "points", "sizeCm": 18, "grow": 5.5, "lifeMs": 300, "color": "mid" },
  { "emitter": "glint", "at": "points", "height": "chest", "sizeCm": 40 }
 ] },
 "skill.mana_shield": { "palette": "arcane", "layers": [
  { "emitter": "gather", "sprite": "Spark", "at": "origin", "count": 12, "color": "rim" },
  { "emitter": "decal", "sprite": "Rune", "blend": "additive", "at": "origin", "delayMs": 100, "sizeCm": 151, "grow": 1, "lifeMs": 900, "spinDegS": 52, "alpha": [0.8, 0], "color": "rim" },
  { "sprite": "Bubble", "mesh": "SM_FX_Bubble", "orient": "billboard", "at": "origin", "height": "chest", "delayMs": 120, "sizeCm": 40, "grow": 3.75, "sizePow": 6, "lifeMs": 760, "alpha": [0.6, 0], "alphaPow": 3, "color": "#9A7CFF" },
  { "emitter": "flash", "at": "origin", "height": "chest", "delayMs": 120, "sizeCm": 53, "grow": 2.4, "color": "core" },
  { "sprite": "Spark", "orient": "billboard", "at": "origin", "height": "chest", "delayMs": 120, "count": 8, "orbit": { "r0Cm": 62, "r1Cm": 62, "degS": 200 }, "sizeCm": 12, "grow": 1, "lifeMs": 640, "alpha": [1, 0], "color": "mid" }
 ] },
 "skill.teleport": { "palette": "arcane", "layers": [
  { "emitter": "gather", "sprite": "Spark", "at": "origin", "count": 12, "orbit": { "r0Cm": 75, "r1Cm": 0, "degS": 200 }, "lifeMs": 180, "color": "mid" },
  { "emitter": "glow", "orient": "upright", "at": "origin", "height": "chest", "sizeCm": 240, "aspect": 0.5, "grow": 0.08, "lifeMs": 200, "color": "mid" },
  { "emitter": "ring", "at": "origin", "sizeCm": 142, "grow": 0.12, "lifeMs": 220, "color": "mid" },
  { "emitter": "decal", "sprite": "Rune", "blend": "additive", "at": "origin", "sizeCm": 115, "grow": 1, "lifeMs": 420, "spinDegS": 115, "alpha": [0.8, 0], "color": "rim" },
  { "emitter": "streak", "at": "path", "height": "chest", "count": 6, "speedCmS": 700, "dir": "blow", "coneDeg": 4, "lifeMs": 200, "sizeCm": 14, "color": "mid" },
  { "emitter": "decal", "sprite": "Rune", "blend": "additive", "at": "point", "delayMs": 100, "sizeCm": 133, "grow": 1.15, "lifeMs": 600, "spinDegS": -86, "alpha": [0.8, 0], "color": "mid" },
  { "emitter": "flash", "at": "point", "height": "chest", "delayMs": 100, "sizeCm": 66, "grow": 2.4, "color": "core" },
  { "emitter": "beam", "at": "point", "delayMs": 100, "sizeCm": 155, "aspect": 0.32, "grow": 1, "lifeMs": 360, "alpha": [0.7, 0], "color": "rim" },
  { "emitter": "ring", "at": "point", "delayMs": 100, "sizeCm": 27, "grow": 6.7, "lifeMs": 400, "color": "mid" },
  { "emitter": "sparks", "at": "point", "height": "chest", "delayMs": 100, "count": 10, "color": "mid" },
  { "emitter": "motes", "at": "point", "delayMs": 100, "count": 8, "gravityCmS2": -150, "color": "core" }
 ] },
 "skill.fire_wall": { "palette": "fire", "layers": [
  { "emitter": "decal", "at": "point", "unit": "radius", "sizeCm": 1.84, "lifeMs": 1700, "alpha": [0.6, 0] },
  { "emitter": "shock", "at": "point", "unit": "radius", "sizeCm": 0.48, "grow": 3.3, "lifeMs": 380, "color": "mid" }
 ] },
 "skill.backstab": { "palette": "shadow", "layers": [
  { "emitter": "slash", "at": "target", "height": "chest", "sizeCm": 71, "lifeMs": 200, "rotationDeg": -40, "color": "rim" },
  { "emitter": "slash", "at": "target", "height": "chest", "delayMs": 60, "sizeCm": 71, "lifeMs": 200, "rotationDeg": 40, "color": "mid" },
  { "emitter": "flash", "at": "target", "height": "chest", "delayMs": 30, "sizeCm": 40, "grow": 2.4, "color": "mid" },
  { "emitter": "glint", "at": "target", "height": "chest", "delayMs": 60, "sizeCm": 98, "rotationDeg": 45 },
  { "emitter": "sparks", "at": "target", "height": "chest", "count": 6, "color": "mid" },
  { "sprite": "Drop", "orient": "billboard", "at": "target", "height": "chest", "count": 6, "dir": "blow", "coneDeg": 80, "speedCmS": [178, 377], "gravityCmS2": 933, "sizeCm": [6, 9], "aspect": 0.7, "grow": 1, "lifeMs": [300, 450], "alpha": [1, 0], "color": "#E8342C" },
  { "emitter": "smoke", "at": "target", "height": 40, "count": 2, "color": "#4A3A5A" }
 ] },
 "skill.poison_blade": { "palette": "poison", "layers": [
  { "emitter": "gather", "at": "origin", "count": 8, "color": "mid" },
  { "emitter": "flames", "at": "origin", "delayMs": 110, "count": 12, "spawnRadiusCm": 53, "onRing": true, "color": "mid" },
  { "emitter": "ring", "at": "origin", "sizeCm": 44, "grow": 4.4, "lifeMs": 420, "color": "rim" },
  { "emitter": "glow", "at": "origin", "height": "chest", "sizeCm": 124, "grow": 1.4, "lifeMs": 400, "color": "mid" },
  { "sprite": "Drop", "orient": "billboard", "at": "origin", "height": "hand", "count": 7, "gravityCmS2": 600, "sizeCm": [6, 9], "aspect": 0.7, "grow": 1, "lifeMs": [350, 500], "alpha": [1, 0], "color": "rim" },
  { "emitter": "decal", "sprite": "Puddle", "at": "origin", "delayMs": 250, "sizeCm": 71, "grow": 1.3, "lifeMs": 900, "alpha": [0.7, 0], "color": "rim" }
 ] },
 "skill.multishot": { "palette": "steel", "layers": [
  { "emitter": "flash", "at": "origin", "height": "hand", "sizeCm": 44, "grow": 2.4, "color": "mid" }
 ] },
 "skill.death_mark": { "palette": "shadow", "layers": [
  { "emitter": "decal", "sprite": "Rune", "blend": "additive", "at": "target", "sizeCm": 98, "grow": 1, "lifeMs": 1000, "spinDegS": -69, "alpha": [0.8, 0], "color": "rim" },
  { "emitter": "gather", "at": "target", "count": 10, "color": "mid" },
  { "sprite": "Skull", "orient": "billboard", "at": "target", "height": "overhead", "sizeCm": 64, "grow": 0.42, "sizePow": 6, "lifeMs": 1000, "alpha": [1, 0], "alphaPow": 3, "color": "mid" },
  { "sprite": "Skull", "orient": "billboard", "at": "target", "height": "overhead", "sizeCm": 50, "grow": 1, "lifeMs": 260, "alpha": [1, 0], "color": "white" },
  { "emitter": "ring", "at": "target", "sizeCm": 80, "grow": 0.39, "lifeMs": 360, "color": "rim" },
  { "emitter": "glint", "at": "target", "height": "overhead", "sizeCm": 40 },
  { "emitter": "motes", "at": "target", "height": "chest", "count": 6, "orbit": { "r0Cm": 49, "r1Cm": 49, "degS": 180 }, "gravityCmS2": 0, "color": "mid" }
 ] },
 "skill.poison_cloud": { "palette": "poison", "layers": [
  { "emitter": "decal", "sprite": "Puddle", "at": "point", "unit": "radius", "sizeCm": 1.12, "grow": 1.2, "lifeMs": 1600, "alpha": [0.7, 0], "color": "rim" },
  { "emitter": "glow", "orient": "ground", "at": "point", "height": 4, "unit": "radius", "sizeCm": 1.4, "grow": 1.05, "lifeMs": 1300, "alpha": [0.4, 0], "color": "mid" },
  { "emitter": "smoke", "at": "point", "unit": "radius", "count": 12, "spawnRadiusCm": 0.7, "sizeCm": [0.4, 0.6], "lifeMs": [900, 1300], "alpha": [0.7, 0], "color": "#6AB83A" },
  { "emitter": "smoke", "at": "point", "unit": "radius", "delayMs": 120, "count": 8, "spawnRadiusCm": 0.6, "sizeCm": [0.3, 0.45], "lifeMs": [900, 1300], "alpha": [0.6, 0], "color": "#A8E05A" },
  { "emitter": "motes", "sprite": "Bubble", "at": "point", "unit": "radius", "count": 12, "spawnRadiusCm": 0.7, "gravityCmS2": -90, "speedCmS": [10, 30], "sizeCm": [0.06, 0.1], "lifeMs": [600, 1000], "color": "mid", "intensity": 1 },
  { "emitter": "shock", "at": "point", "unit": "radius", "sizeCm": 0.42, "grow": 4, "lifeMs": 420, "alpha": [0.5, 0], "color": "mid" }
 ] },
 "skill.combustion": { "palette": "fire", "shake": [140, 0.006], "layers": [
  { "emitter": "gather", "at": "target", "count": 8, "lifeMs": 60, "color": "rim" },
  { "emitter": "flash", "at": "target", "height": "chest", "delayMs": 60, "sizeCm": 75, "grow": 2.4, "color": "core" },
  { "emitter": "glow", "at": "target", "height": "chest", "delayMs": 60, "sizeCm": 140, "grow": 1.6, "lifeMs": 320, "alpha": [0.6, 0], "color": "rim" },
  { "emitter": "flames", "at": "target", "delayMs": 60, "count": 10, "spawnRadiusCm": 40, "color": "mid" },
  { "emitter": "shock", "at": "target", "delayMs": 60, "sizeCm": 36, "grow": 6, "lifeMs": 380, "color": "mid" },
  { "emitter": "sparks", "at": "target", "height": "chest", "delayMs": 60, "count": 10, "color": "mid" },
  { "emitter": "smoke", "at": "target", "height": 40, "delayMs": 120, "count": 4, "color": "#4A3A36" },
  { "emitter": "decal", "at": "target", "delayMs": 60, "sizeCm": 90, "lifeMs": 1400, "alpha": [0.7, 0] }
 ] },
 "skill.arcane_torrent": { "palette": "arcane", "layers": [
  { "emitter": "decal", "sprite": "Rune", "blend": "additive", "at": "point", "unit": "radius", "sizeCm": 1.8, "grow": 1, "lifeMs": 900, "spinDegS": 40, "alpha": [0.7, 0], "color": "rim" },
  { "sprite": "Comet", "orient": "velocity", "at": "point", "unit": "radius", "count": 12, "spawnRadiusCm": 0.85, "height": 260, "speedCmS": 1700, "dir": "down", "coneDeg": 15, "lifeMs": 150, "sizeCm": 0.25, "aspect": 0.35, "grow": 1, "alpha": [1, 1], "color": "mid", "intensity": 1.5 },
  { "emitter": "shock", "at": "point", "unit": "radius", "delayMs": 250, "sizeCm": 0.4, "grow": 5, "lifeMs": 420, "color": "mid" },
  { "emitter": "motes", "sprite": "Spark", "at": "point", "unit": "radius", "delayMs": 150, "count": 12, "spawnRadiusCm": 0.8, "gravityCmS2": -120, "color": "core" }
 ] }
}
})ABYSSJSON";

	const char* const AllDocuments[] = { Combat, Utility, World, Projectiles, SkillsA, SkillsB };
}

namespace AbyssVfxDefaults
{
	TConstArrayView<const char*> Documents()
	{
		return TConstArrayView<const char*>(AbyssVfxDefaultsPrivate::AllDocuments);
	}
}

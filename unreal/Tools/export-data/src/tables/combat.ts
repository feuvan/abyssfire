/**
 * Combat constants: hero_formulas, buff_caps, status_effects, spirit_profiles, combat_input,
 * projectile_timing, anim_timing, hit_feedback, elite_affixes, difficulty, soul_echo.
 *
 * Exported TS tables are imported as-is. Numbers that only exist inside code are transcribed with the
 * line they come from and pinned by `assertSource`, and wherever a pure TS function computes them it
 * is called with probe inputs and compared (Player.recalcDerived, expToNextLevel, regen getters,
 * SpiritSystem.gainFromCombat, classifyHit, attackSpeedScale, DifficultySystem.scaleMonster, ...).
 */
import { Player } from '../../../../../src/entities/Player';
import { BUFF_CAPS, emptyEquipStats } from '../../../../../src/systems/CombatSystem';
import * as StatusFx from '../../../../../src/systems/StatusEffectSystem';
import * as SpiritMod from '../../../../../src/systems/SpiritSystem';
import { SpiritSystem } from '../../../../../src/systems/SpiritSystem';
import * as CombatInputMod from '../../../../../src/systems/CombatInputSystem';
import * as SkillEffectMod from '../../../../../src/systems/SkillEffectSystem';
import { HIT_PROFILES, classifyHit, attackSpeedScale, computeImpactDelay } from '../../../../../src/systems/HitFeedback';
import * as AnimatorMod from '../../../../../src/systems/CharacterAnimator';
import { CharacterAnimator } from '../../../../../src/systems/CharacterAnimator';
import * as SpriteGenMod from '../../../../../src/graphics/SpriteGenerator';
import { PLAYER_ACTION_FRAME_COUNTS, PLAYER_ACTION_ORDER, PLAYER_VIEWS } from '../../../../../src/graphics/sprites/types';
import { ELITE_AFFIX_DEFINITIONS } from '../../../../../src/systems/EliteAffixSystem';
import * as EliteMod from '../../../../../src/systems/EliteAffixSystem';
import { DifficultySystem, DIFFICULTY_ORDER } from '../../../../../src/systems/DifficultySystem';
import { LootSystem } from '../../../../../src/systems/LootSystem';
import * as SoulEchoMod from '../../../../../src/systems/SoulEcho';
import * as ZoneSceneMod from '../../../../../src/scenes/ZoneScene';
import { TILE_WIDTH, TILE_HEIGHT } from '../../../../../src/config';
import { assert, assertSource, plain, type TableResult } from '../util';

const exp = <T>(mod: unknown, name: string): T => {
  const v = (mod as Record<string, unknown>)[`__expose_${name}`];
  assert(v !== undefined, `exposed binding ${name} missing`);
  return v as T;
};

const ZS = 'src/scenes/ZoneScene.ts';

// ── hero_formulas.json ─────────────────────────────────────────────────────

function heroFormulas(): TableResult {
  // Player.recalcDerived / regen / exp curve (Player.ts:106-182), probed with a fake `this`.
  const proto = Player.prototype as unknown as Record<string, (...a: unknown[]) => unknown>;
  const derive = (stats: Record<string, number>, level: number, eq?: Record<string, number>) => {
    const self: Record<string, unknown> = { stats, level, spirit: { moveSpeedMultiplier: 1 } };
    proto.recalcDerived.call(self, eq ? { ...emptyEquipStats(), ...eq } : undefined);
    return self as { maxHp: number; maxMana: number; baseDamage: number; defense: number; moveSpeed: number; attackSpeed: number };
  };
  const F = {
    maxHp: { base: 50, perVit: 10, perLevelAbove1: 15 },
    maxMana: { base: 30, perSpi: 8, perInt: 3, perLevelAbove1: 8 },
    baseDamage: { base: 8, perStr: 0.8, perLevel: 2 },
    defense: { base: 3, perVit: 0.5, perLevel: 1 },
  };
  for (const [stats, level] of [[{ str: 12, dex: 8, vit: 10, int: 5, spi: 5, lck: 5 }, 1], [{ str: 4, dex: 6, vit: 6, int: 14, spi: 10, lck: 5 }, 10],
    [{ str: 31, dex: 7, vit: 17, int: 3, spi: 9, lck: 2 }, 23]] as const) {
    const d = derive({ ...stats }, level);
    assert(d.maxHp === F.maxHp.base + stats.vit * F.maxHp.perVit + (level - 1) * F.maxHp.perLevelAbove1, 'maxHp formula');
    assert(d.maxMana === F.maxMana.base + stats.spi * F.maxMana.perSpi + stats.int * F.maxMana.perInt + (level - 1) * F.maxMana.perLevelAbove1, 'maxMana formula');
    assert(d.baseDamage === F.baseDamage.base + stats.str * F.baseDamage.perStr + level * F.baseDamage.perLevel, 'baseDamage formula');
    assert(d.defense === F.defense.base + stats.vit * F.defense.perVit + level * F.defense.perLevel, 'defense formula');
    assert(d.moveSpeed === 120 && d.attackSpeed === 1000, 'base move / attack speed');
  }
  const g = derive({ str: 10, dex: 0, vit: 10, int: 10, spi: 10, lck: 0 }, 5,
    { str: 3, vit: 2, maxHp: 7, maxHpPercent: 10, maxMana: 5, maxManaPercent: 20, moveSpeed: 15, attackSpeed: 90 });
  assert(g.maxHp === Math.floor((50 + 12 * 10 + 4 * 15 + 7) * 1.1), 'gear maxHp');
  assert(g.maxMana === Math.floor((30 + 80 + 30 + 32 + 5) * 1.2), 'gear maxMana');
  assert(g.moveSpeed === Math.floor(120 * 1.15) && g.attackSpeed === 200, 'gear speed / attack interval floor');
  for (let L = 1; L <= 30; L++) assert(proto.expToNextLevel.call({ level: L }) === Math.floor(3 * L * L + 25 * L), 'exp curve');
  assert(proto.getHpRegenPerSecond.call({ stats: { vit: 10 } }) === 0.5 + 10 * 0.05, 'hp regen');
  assert(proto.getManaRegenPerSecond.call({ stats: { spi: 10 } }) === 1 + 10 * 0.1, 'mana regen');
  assertSource('src/entities/Player.ts', 'attackRange: number = 1.5;', 'this.freeStatPoints += 5;', 'this.freeSkillPoints += 1;',
    'aspd = Math.max(200, Math.floor(aspd * (1 - equipStats.attackSpeed / 100)));',
    'this.moveSpeed = Math.floor(spd * this.spirit.moveSpeedMultiplier);');
  // Damage formula constants (CombatSystem.calculateDamage, CombatSystem.ts:229-381).
  assertSource('src/systems/CombatSystem.ts',
    'const dodgeRate = clamp((defender.stats.dex + eqDex) * 0.3, 0, 30);',
    '(attacker.stats.dex + eqAtkDex) * 0.2 + (attacker.stats.lck + eqAtkLck) * 0.5 + skillCritBonus + eqCritRate, 0, 75,',
    'const critMultiplier = isCrit ? 1.5 + (attacker.stats.lck + eqAtkLck) * 0.01 + eqCritDmg / 100 : 1;',
    'baseDmg = attacker.baseDamage + statBonus * 0.5;',
    'baseDmg = attacker.baseDamage + attacker.stats.str * 0.5;');
  assertSource('src/systems/CombatSystem.ts', 'const afterDef = Math.max(1, rawDamage - effectiveDefense * 0.5);',
    'damageReduction = Math.min(damageReduction, BUFF_CAPS.damageReduction);',
    'const outgoingMultiplier = clamp(attacker.outgoingDamageMultiplier ?? 1, 0, 10);', 'return clamp(res, 0, 75);',
    'finalDamage = Math.max(1, Math.floor(finalDamage));');
  // Regen and campfire (ZoneScene.ts:83-86, 1366-1379).
  const campRadius = exp<number>(ZoneSceneMod, 'CAMPFIRE_RECOVERY_RADIUS');
  const campHp = exp<number>(ZoneSceneMod, 'CAMPFIRE_HP_REGEN_MULTIPLIER');
  const campMp = exp<number>(ZoneSceneMod, 'CAMPFIRE_MANA_REGEN_MULTIPLIER');
  assertSource(ZS, 'recovery.hpRegenMultiplier = (recovery.hpRegenMultiplier ?? 1) * 0.5;');
  return {
    file: 'hero_formulas.json',
    source: ['src/entities/Player.ts', 'src/systems/CombatSystem.ts', ZS],
    data: {
      derived: {
        ...F,
        moveSpeed: 120,
        attackIntervalMs: 1000,
        attackIntervalMinMs: 200,
        attackRange: 1.5,
        gear: {
          note: 'with gear: hp=floor((hp+eq.maxHp)*(1+eq.maxHpPercent/100)); mp likewise; move=floor(120*(1+eq.moveSpeed/100)); '
            + 'attack=max(200, floor(1000*(1-eq.attackSpeed/100))); moveSpeed then floor(x*spirit.moveSpeedMultiplier)',
        },
      },
      regen: {
        hpPerSecond: { base: 0.5, perVit: 0.05 },
        manaPerSecond: { base: 1, perSpi: 0.1 },
        campfireRadiusTiles: campRadius,
        campfireHpMultiplier: campHp,
        campfireManaMultiplier: campMp,
        poisonedHpRegenMultiplier: 0.5,
      },
      leveling: {
        expToNext: { a: 3, b: 25, formula: 'floor(a*L*L + b*L)' },
        statPointsPerLevel: 5,
        skillPointsPerLevel: 1,
        levelCap: null,
      },
      damage: {
        dodgePerDex: 0.3, dodgeCapPercent: 30,
        critPerDex: 0.2, critPerLck: 0.5, critCapPercent: 75,
        critMultiplierBase: 1.5, critMultiplierPerLck: 0.01,
        statToDamage: 0.5,
        defenseFactor: 0.5,
        resistCapPercent: 75,
        damageReductionCap: 0.9,
        outgoingMultiplierMin: 0, outgoingMultiplierMax: 10,
        minDamage: 1,
      },
    },
    counts: { formulas: 4 },
  };
}

// ── buff_caps.json / status_effects.json / spirit_profiles.json ─────────────

function buffCaps(): TableResult {
  return {
    file: 'buff_caps.json',
    source: ['src/systems/CombatSystem.ts'],
    data: { caps: plain(BUFF_CAPS), uncapped: ['damageAmplify', 'critBonus', 'slowEffect', 'taunted'] },
    counts: { caps: Object.keys(BUFF_CAPS).length },
  };
}

function statusEffects(skillRules: Record<string, unknown>): TableResult {
  assertSource(ZS,
    "if (spriteKey.includes('fire') || spriteKey.includes('phoenix') || spriteKey.includes('lava') || monsterId.includes('fire') || monsterId.includes('phoenix') || monsterId.includes('lava')) {",
    'if (Math.random() < 0.3) { const burnDamage = Math.floor(damage * 0.2);',
    "this.statusEffects.apply('player', 'burn', Math.max(1, burnDamage), 3000, monster.id, time);",
    "if (spriteKey.includes('poison') || spriteKey.includes('venom') || spriteKey.includes('spider') || monsterId.includes('poison') || monsterId.includes('venom') || monsterId.includes('spider')) {",
    'if (Math.random() < 0.25) { const poisonDamage = Math.floor(damage * 0.15);',
    "if (spriteKey.includes('ice') || spriteKey.includes('frost') || monsterId.includes('ice') || monsterId.includes('frost')) {",
    "if (Math.random() < 0.2) { this.statusEffects.apply('player', 'slow', 30, 3000, monster.id, time);",
    "this.statusEffects.apply('player', 'slow', 30, 2500, monster.id, time);");
  return {
    file: 'status_effects.json',
    source: ['src/systems/StatusEffectSystem.ts', ZS],
    data: {
      types: ['burn', 'freeze', 'poison', 'bleed', 'slow', 'stun'],
      tickIntervalMs: plain(StatusFx.DEFAULT_TICK_INTERVALS),
      diminishing: {
        appliesTo: ['freeze', 'stun'],
        factor: StatusFx.DIMINISH_FACTOR,
        immunityMs: StatusFx.DIMINISH_IMMUNITY_DURATION,
        windowMs: StatusFx.DIMINISH_WINDOW,
      },
      slowMinSpeedFactor: StatusFx.SLOW_MIN_SPEED_FACTOR,
      stacking: { burn: 'stack', bleed: 'stack', poison: 'refreshKeepStronger', slow: 'refreshKeepStronger', freeze: 'replace', stun: 'replace' },
      poisonedHpRegenMultiplier: 0.5,
      // Generator rules for MonsterDefinition.derived.onHitStatus (ZoneScene.applyMonsterStatusEffect, :6204-6237);
      // each rule rolls independently, value from the (scaled) monster damage.
      monsterKeywordRules: [
        { keywords: ['fire', 'phoenix', 'lava'], status: 'burn', chance: 0.3, value: { kind: 'monsterDamage', fraction: 0.2, min: 1 }, durationMs: 3000 },
        { keywords: ['poison', 'venom', 'spider'], status: 'poison', chance: 0.25, value: { kind: 'monsterDamage', fraction: 0.15, min: 1 }, durationMs: 4000 },
        { keywords: ['ice', 'frost'], status: 'slow', chance: 0.2, value: { kind: 'fixed', amount: 30 }, durationMs: 3000 },
      ],
      eliteFrozenOnHit: { status: 'slow', value: 30, durationMs: 2500, chanceFrom: 'eliteAffix.freezeChance' },
      skillRules,
    },
    counts: { types: 6, skillsWithRules: Object.keys(skillRules).length },
  };
}

function spiritProfiles(): TableResult {
  const profiles = exp<Record<string, unknown>>(SpiritMod, 'SPIRIT_PROFILES');
  // gainFromCombat: (base + crit bonus) * (1 + clamp(spi, 0, 200) * 0.015) (SpiritSystem.ts:122-131).
  for (const spi of [0, 10, 200, 500]) {
    const s = new SpiritSystem('mage');
    const r = s.gainFromCombat('hit', spi, true);
    const want = Math.min(100, (7 + 5) * (1 + Math.min(200, spi) * 0.015));
    assert(Math.abs(r.gained - want) < 1e-9, `spirit gain at spi ${spi}`);
  }
  assert(SpiritMod.getSpiritProfile('nobody').id === 'emberheart', 'unknown class → warrior profile');
  return {
    file: 'spirit_profiles.json',
    source: ['src/systems/SpiritSystem.ts'],
    data: { profiles: plain(profiles), fallbackClass: 'warrior', spiGainFactor: 0.015, spiClamp: [0, 200] },
    counts: { profiles: Object.keys(profiles).length },
  };
}

// ── combat_input.json ───────────────────────────────────────────────────────

function combatInput(): TableResult {
  const dodge = exp<{ cooldownMs: number; invulnerabilityMs: number }>(CombatInputMod, 'DEFAULT_DODGE_CONFIG');
  assertSource(ZS,
    'private readonly combatInput = new CombatInputBuffer(180);',
    "const dodgeDistance = this.player.classData.id === 'rogue' ? 2.6 : this.player.classData.id === 'mage' ? 2.25 : 1.8;",
    'for (let distance = dodgeDistance; distance >= 0.5; distance -= 0.25) {',
    'dx = 1; dy = -1;',
    'const dodgePressed = pad.buttons?.[1]?.pressed ?? false;',
    'const targetPressed = pad.buttons?.[4]?.pressed ?? false;',
    'const skillButtonIndices = [0, 2, 3, 5];',
    '14, );',
    'const nearbyAttackers = this.monsterGrid.queryRadius(this.player.tileCol, this.player.tileRow, 12);',
    'const reach = monster.definition.attackRange * 1.35 + 0.5;',
    'const ranged = monster.definition.attackRange > 2.5;',
    '}, 1500);',
    'this.time.delayedCall(15000, () => this.respawnMonster(monster));');
  assertSource('src/systems/SkillEffectSystem.ts', 'const duration = Math.max(200, Math.min(500, dist * 2));');
  const holdMs = exp<number>(ZoneSceneMod, 'HOLD_MOVE_REPATH_MS');
  return {
    file: 'combat_input.json',
    source: ['src/systems/CombatInputSystem.ts', ZS, 'src/systems/SkillEffectSystem.ts'],
    data: {
      inputBufferMs: 180,
      dodge: {
        cooldownMs: dodge.cooldownMs,
        invulnerabilityMs: dodge.invulnerabilityMs,
        distanceTiles: { warrior: 1.8, mage: 2.25, rogue: 2.6 },
        stepTiles: 0.25,
        minTiles: 0.5,
        defaultDirection: [1, -1],
      },
      gamepad: { dodgeButton: 1, targetCycleButton: 4, skillButtons: [0, 2, 3, 5] },
      targetCycleRangeTiles: 14,
      monsterSwingScanRadiusTiles: 12,
      monsterMeleeReach: { mul: 1.35, add: 0.5 },
      monsterRangedThreshold: 2.5,
      monsterProjectileMs: { msPerPx: 2, minMs: 200, maxMs: 500 },
      combatStateOffDebounceMs: 1500,
      monsterRespawnMs: 15000,
      holdMoveRepathMs: holdMs,
    },
    counts: { constants: 12 },
  };
}

// ── projectile_timing.json / anim_timing.json ───────────────────────────────

function animTables(classes: { id: string; skills: { id: string; derived: Record<string, unknown> }[] }[]): TableResult[] {
  const presets = exp<Record<string, Record<string, unknown>>>(AnimatorMod, 'PRESETS');
  const transitions = (CharacterAnimator as unknown as Record<string, Record<string, number>>).TRANSITION_MS;
  assert(transitions && Object.keys(transitions).length > 0, 'CharacterAnimator.TRANSITION_MS');
  const monsterFrames = {
    idle: { frames: exp<number>(SpriteGenMod, 'IDLE_COUNT'), fps: 6, loop: true },
    walk: { frames: exp<number>(SpriteGenMod, 'WALK_COUNT'), fps: 10, loop: true },
    attack: { frames: exp<number>(SpriteGenMod, 'ATK_COUNT'), fps: 12, loop: false },
    hurt: { frames: exp<number>(SpriteGenMod, 'HURT_COUNT'), fps: 10, loop: false },
    death: { frames: exp<number>(SpriteGenMod, 'DEATH_COUNT'), fps: 6, loop: false },
  };
  assertSource('src/graphics/SpriteGenerator.ts',
    "[name('idle'), base + IDLE_START, IDLE_COUNT, 6, -1],", "[name('walk'), base + WALK_START, WALK_COUNT, 10, -1],",
    "[name('attack'), base + ATK_START, ATK_COUNT, 12, 0],", "[name('hurt'), base + HURT_START, HURT_COUNT, 10, 0],",
    "[name('death'), base + DEATH_START, DEATH_COUNT, 6, 0],");
  assertSource('src/systems/CharacterAnimator.ts',
    'const contactFrame = Math.round((anim.frames.length - 1) * this.config.attackContact);',
    'const chargeMs = total * 0.46;', 'const releaseMs = total * 0.2;');
  // Contact / release beats the core schedules (CharacterAnimator.ts:862-867, 934-990; HitFeedback.ts:72-75).
  const heroes = ['warrior', 'mage', 'rogue'];
  const contact: Record<string, unknown> = {};
  for (const [rig, cfg] of Object.entries(presets)) {
    const isHero = heroes.includes(rig);
    const frames = isHero ? PLAYER_ACTION_FRAME_COUNTS.attack : monsterFrames.attack.frames;
    const fps = isHero ? (cfg.attackFrameRate as number) : monsterFrames.attack.fps;
    const contactFrame = Math.round((frames - 1) * (cfg.attackContact as number));
    const contactMs = contactFrame * (1000 / fps);
    contact[rig] = {
      attackFrames: frames, attackFps: fps, contactFrame,
      contactMsAtSpeed1: Math.round(contactMs),
      frameContactMs: contactMs,
      castReleaseMs: isHero ? Math.round((cfg.castDuration as number) * 0.46) : null,
      fullSpeedBelowIntervalMs: (cfg.attackDuration as number) / 0.9,
    };
  }
  // Pin the parity targets quoted by the specs.
  const c = contact as Record<string, { contactMsAtSpeed1: number; castReleaseMs: number | null }>;
  assert(c.warrior.contactMsAtSpeed1 === 308 && c.mage.contactMsAtSpeed1 === 267 && c.rogue.contactMsAtSpeed1 === 222, 'hero contact beats');
  assert(c.warrior.castReleaseMs === 334 && c.mage.castReleaseMs === 230 && c.rogue.castReleaseMs === 246, 'hero cast release');
  assert(c.humanoid.contactMsAtSpeed1 === 250, 'monster contact 250 ms');
  assert(attackSpeedScale(610, 1000) === 1 && attackSpeedScale(610, 100) === 0.35 && attackSpeedScale(610, 600) === 600 * 0.9 / 610, 'attackSpeedScale');

  const projectiles: Record<string, unknown> = {};
  const arrows: Record<string, unknown> = {};
  const aoeDelays: Record<string, number> = {};
  for (const cls of classes) for (const s of cls.skills) {
    if (s.derived.projectile) projectiles[s.id] = s.derived.projectile;
    if (s.derived.arrowDelay) arrows[s.id] = s.derived.arrowDelay;
    if ((s.derived.aoeDelayMs as number) > 0) aoeDelays[s.id] = s.derived.aoeDelayMs as number;
  }
  const chestPx = exp<number>(SkillEffectMod, 'CH');
  assertSource('src/systems/SkillEffectSystem.ts', 'const dist = Phaser.Math.Distance.Between(casterX, casterY - 16, targetX, targetY - 16);');
  return [
    {
      file: 'projectile_timing.json',
      source: ['src/systems/SkillEffectSystem.ts', ZS, 'src/systems/CharacterAnimator.ts', 'src/config.ts'],
      data: {
        isoPx: {
          tileWidth: TILE_WIDTH, tileHeight: TILE_HEIGHT,
          formula: 'sqrt(((dc-dr)*tileWidth/2)^2 + ((dc+dr)*tileHeight/2)^2)',
          note: 'web distances are iso screen px between the two feet points (both raised 16 px, which cancels)',
        },
        portPxPerTile: { projectileAndArrow: 36, vfxSize: 45, decision: 'S4' },
        skillProjectiles: projectiles,
        skillArrowDelay: arrows,
        skillAoeDelayMs: aoeDelays,
        monsterProjectile: { msPerPx: 2, minMs: 200, maxMs: 500 },
        chestHeightPx: chestPx,
        heroBeats: Object.fromEntries(heroes.map(h => [h, { contactMsAtSpeed1: c[h].contactMsAtSpeed1, castReleaseMs: c[h].castReleaseMs }])),
      },
      counts: { projectiles: Object.keys(projectiles).length },
    },
    {
      file: 'anim_timing.json',
      source: ['src/systems/CharacterAnimator.ts', 'src/graphics/SpriteGenerator.ts', 'src/graphics/sprites/types.ts', 'src/systems/HitFeedback.ts'],
      data: {
        presets: plain(presets),
        fallbackPreset: 'humanoid',
        heroSheet: { actionOrder: [...PLAYER_ACTION_ORDER], frameCounts: plain(PLAYER_ACTION_FRAME_COUNTS), views: [...PLAYER_VIEWS] },
        monsterSheet: { actions: monsterFrames, viewFrames: exp<number>(SpriteGenMod, 'MONSTER_VIEW_FRAMES') },
        castPhases: { charge: 0.46, release: 0.2, recover: 0.34 },
        attackPhases: { windupOfContact: 0.62, strikeOfContact: 0.38, recoverMinMs: 70 },
        attackSpeedScale: { intervalFactor: 0.9, min: 0.35, max: 1 },
        legacyImpactDelay: { strikeFactor: 0.6, speedMin: 0.2, speedMax: 1, check: computeImpactDelay(150, 80) },
        transitionsMs: plain(transitions),
        transitionDefaultMs: 80,
        contact,
      },
      counts: { rigs: Object.keys(presets).length, transitions: Object.keys(transitions).length },
    },
  ];
}

// ── hit_feedback.json ───────────────────────────────────────────────────────

function hitFeedback(): TableResult {
  for (const [dmg, hp, w] of [[25, 100, 'heavy'], [24.99, 100, 'normal'], [6, 100, 'normal'], [5.99, 100, 'light'], [5, 0, 'light']] as const) {
    assert(classifyHit({ damage: dmg, maxHp: hp }) === w, `classifyHit ${dmg}/${hp}`);
  }
  assert(classifyHit({ damage: 1, maxHp: 1, isTick: true, killed: true, isCrit: true }) === 'tick', 'tick first');
  assert(classifyHit({ damage: 1, maxHp: 100, killed: true, isCrit: true }) === 'kill', 'kill before crit');
  const classColors = exp<Record<string, number>>(ZoneSceneMod, 'CLASS_IMPACT_COLORS');
  assertSource(ZS, 'const color = CLASS_IMPACT_COLORS[this.player.classData.id] ?? 0xfff2c0;',
    "if (weight === 'kill' && target.definition.elite) { this.vfx.slowMotion(200, 0.4);",
    'monster.animator.triggerHitFreeze(Math.round(HIT_PROFILES[hurtWeight].attackerStopMs * 0.6));',
    'if (this.vfx && hits > 0) this.vfx.cameraShake(100, 0.004 + hits * 0.001);');
  assertSource('src/systems/VFXManager.ts', 'const severity = 1 - (hpRatio / 0.3);',
    'const pulse = Math.sin(this.scene.time.now * 0.005) * 0.05;', 'this.dangerVignette.strength = 0.2 + severity * 0.25 + pulse;',
    'this.dangerVignette.radius = 0.7 + severity * 0.15;', 'this.cameraShake(150, 0.008);',
    'const intensity = Math.max(0.002, Math.min(0.006, ratio * 0.01));',
    'const duration = Math.max(50, Math.min(120, 50 + ratio * 100));');
  return {
    file: 'hit_feedback.json',
    source: ['src/systems/HitFeedback.ts', ZS, 'src/systems/VFXManager.ts'],
    data: {
      profiles: plain(HIT_PROFILES),
      classify: { order: ['tick', 'kill', 'crit', 'ratio'], heavyRatio: 0.25, normalRatio: 0.06 },
      monsterHitAttackerStopFactor: 0.6,
      eliteKillSlowMotion: { durationMs: 200, timeScale: 0.4 },
      classImpactColors: plain(classColors),
      fallbackImpactColor: 0xfff2c0,
      aoeHitShake: { durationMs: 100, base: 0.004, perHit: 0.001 },
      playerHitShake: {
        crit: { durationMs: 150, intensity: 0.008 },
        normal: { intensity: { perRatio: 0.01, min: 0.002, max: 0.006 }, durationMs: { base: 50, perRatio: 100, min: 50, max: 120 } },
      },
      shakeThrottleMs: 100,
      painTint: { color: 0xff6b6b, durationMs: 100 },
      lowHpVignette: { belowHpRatio: 0.3, strength: { base: 0.2, perSeverity: 0.25, pulse: 0.05, pulseRate: 0.005 },
        radius: { base: 0.7, perSeverity: 0.15 }, severity: '1 - hpRatio/0.3' },
    },
    counts: { profiles: Object.keys(HIT_PROFILES).length },
  };
}

// ── elite_affixes.json / difficulty.json / soul_echo.json ───────────────────

function eliteAffixes(): TableResult {
  const order = exp<string[]>(EliteMod, 'ALL_AFFIX_TYPES');
  const zoneCounts = exp<Record<string, [number, number]>>(EliteMod, 'ZONE_AFFIX_COUNTS');
  assertSource('src/systems/EliteAffixSystem.ts', 'const range = ZONE_AFFIX_COUNTS[zoneId] ?? [1, 1];',
    'freezeChance = Math.min(0.5, freezeChance + def.freezeChance);');
  assertSource(ZS, 'if (dSq > 4 && dSq < 225) {', '(Math.random() < 0.5 ? -1 : 1) * (1 + Math.random());',
    'if (time - (curseAffix.lastCurseTickTime ?? 0) > 2000) {');
  return {
    file: 'elite_affixes.json',
    source: ['src/systems/EliteAffixSystem.ts', ZS],
    data: {
      order,
      definitions: plain(ELITE_AFFIX_DEFINITIONS),
      zoneAffixCounts: plain(zoneCounts),
      defaultAffixCount: [1, 1],
      stacking: { multipliers: 'multiply', extras: 'add', freezeChanceCap: 0.5 },
      runtime: {
        teleportWindowDistSq: [4, 225], teleportOffset: [1, 2],
        curseAura: { buffDurationMs: 2000, logIntervalMs: 2000, tag: 'curseAura' },
        frozenOnHit: { status: 'slow', value: 30, durationMs: 2500 },
      },
    },
    counts: { affixes: order.length, zones: Object.keys(zoneCounts).length },
  };
}

function difficulty(): TableResult {
  const loot = (LootSystem as unknown as { getDifficultyLootMods: (d: string) => unknown }).getDifficultyLootMods;
  const multipliers = Object.fromEntries(DIFFICULTY_ORDER.map(d => [d, DifficultySystem.getMultipliers(d)]));
  const lootMods = Object.fromEntries(DIFFICULTY_ORDER.map(d => [d, loot(d)]));
  // scaleMonster rounding: Math.round each stat; gold uses the exp multiplier (DifficultySystem.ts:196-212).
  const probe = { id: 'p', name: 'p', level: 3, hp: 55, damage: 8, defense: 4, speed: 40, aggroRange: 5, attackRange: 1.5,
    attackSpeed: 1200, expReward: 18, goldReward: [3, 6] as [number, number], spriteKey: 'x' };
  const hell = DifficultySystem.scaleMonster(probe, 'hell');
  assert(hell.hp === 110 && hell.damage === 16 && hell.defense === 6 && hell.expReward === 54 && hell.goldReward[0] === 9, 'scaleMonster hell');
  assert(DifficultySystem.scaleMonster(probe, 'normal') === probe, 'normal returns the base def');
  assertSource('src/systems/DifficultySystem.ts', "if (monsterId !== 'demon_lord') return false;", "if (zoneId !== 'abyss_rift') return false;");
  return {
    file: 'difficulty.json',
    source: ['src/systems/DifficultySystem.ts', 'src/systems/LootSystem.ts'],
    data: {
      order: [...DIFFICULTY_ORDER],
      monsterMultipliers: plain(multipliers),
      monsterScaling: { rounding: 'Math.round', goldUses: 'exp', normalUnchanged: true },
      lootMods: plain(lootMods),
      unlock: { bossId: 'demon_lord', zoneId: 'abyss_rift', next: { normal: 'nightmare', nightmare: 'hell' } },
      nameKey: 'sys.difficulty.<id>',
    },
    counts: { difficulties: DIFFICULTY_ORDER.length },
  };
}

function soulEcho(): TableResult {
  return {
    file: 'soul_echo.json',
    source: ['src/systems/SoulEcho.ts'],
    data: {
      minLevel: SoulEchoMod.SOUL_ECHO_MIN_LEVEL,
      goldShare: plain(exp<Record<string, number>>(SoulEchoMod, 'GOLD_SHARE')),
      expShare: plain(exp<Record<string, number>>(SoulEchoMod, 'EXP_SHARE')),
      claimRangeTiles: SoulEchoMod.SOUL_ECHO_RANGE,
    },
    counts: { difficulties: 3 },
  };
}

export function exportCombat(): TableResult[] {
  return [heroFormulas(), buffCaps(), spiritProfiles(), combatInput(), hitFeedback(), eliteAffixes(), difficulty(), soulEcho()];
}

/** Tables that reuse the per-skill derived data (classes.json is built first in skills.ts). */
export async function exportCombatDerived(): Promise<TableResult[]> {
  const { exportSkills } = await import('./skills');
  const classes = exportSkills().find(t => t.file === 'classes.json')!.data.classes as
    { id: string; skills: { id: string; derived: Record<string, unknown> }[] }[];
  const skillRules: Record<string, unknown> = {};
  for (const cls of classes) for (const s of cls.skills) if ((s.derived.statusRule as unknown[]).length) skillRules[s.id] = s.derived.statusRule;
  return [statusEffects(skillRules), ...animTables(classes)];
}

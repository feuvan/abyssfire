/**
 * classes.json, skill_trees.json, skill_rules.json
 *
 * Classes and skills are dumped verbatim from src/data/classes/*.ts (definition order kept: it is
 * the hotbar auto-fill and auto-combat order). Each skill gets a `derived` block (DECISIONS C6) built
 * from the explicit mapping tables below. Every table replaces an id / substring rule of the web
 * code and is cross-checked against that code at export time (probes call the real TS functions;
 * rules inside closures are pinned with source assertions), so a new skill without a mapping, or a
 * changed web rule, stops the export.
 */
import { AllClasses } from '../../../../../src/data/classes';
import type { SkillDefinition } from '../../../../../src/data/types';
import * as SkillProgression from '../../../../../src/systems/SkillProgressionSystem';
import * as CombatSystemMod from '../../../../../src/systems/CombatSystem';
import { SkillEffectSystem } from '../../../../../src/systems/SkillEffectSystem';
import * as SkillEffectMod from '../../../../../src/systems/SkillEffectSystem';
import * as ZoneSceneMod from '../../../../../src/scenes/ZoneScene';
import { ZoneScene } from '../../../../../src/scenes/ZoneScene';
import { assert, assertSource, plain, withRandom, type TableResult } from '../util';

type ExecKind = 'teleport' | 'shadow_step' | 'death_mark' | 'slow_trap' | 'buff' | 'aoe' | 'single';
type StatusType = 'burn' | 'freeze' | 'poison' | 'bleed' | 'slow' | 'stun';
type StatusValue =
  | { kind: 'fixed'; amount: number }
  | { kind: 'damage'; fraction: number; min: number }
  | { kind: 'buffPercent' };
interface StatusRule {
  status: StatusType;
  /** Roll `rand01() < chance` (one RNG draw); null = always, no draw. */
  chance: number | null;
  value: StatusValue;
  /** null = the skill's scaled buff duration (slow_trap). */
  durationMs: number | null;
}

const exposed = <T>(mod: Record<string, unknown>, name: string): T => {
  const v = mod[`__expose_${name}`];
  assert(v !== undefined, `exposed binding ${name} missing`);
  return v as T;
};

// ── Explicit mapping tables (C6) ─────────────────────────────────────────────

/**
 * releaseSkill branch per skill. Replaces the id checks at ZoneScene.ts:2539 (teleport),
 * :2588 (shadow_step && target), :2619 (death_mark && target), :2638 (slow_trap), then
 * `skill.buff` (:2663), `skill.aoe && aoeRadius > 0` (:2701), else single target (:2770).
 */
const EXEC_KIND: Record<string, ExecKind> = {
  // warrior
  slash: 'single', whirlwind: 'aoe', war_stomp: 'aoe', shield_wall: 'buff', taunt_roar: 'buff', vengeful_wrath: 'buff',
  charge: 'single', lethal_strike: 'single', dual_wield_mastery: 'single', iron_fortress: 'buff', unyielding: 'buff',
  life_regen: 'single', frenzy: 'buff', bleed_strike: 'single', rampage: 'aoe',
  // mage
  fireball: 'single', meteor: 'aoe', blizzard: 'aoe', ice_armor: 'buff', chain_lightning: 'aoe', mana_shield: 'buff',
  fire_wall: 'aoe', combustion: 'single', ice_arrow: 'single', freeze: 'single', teleport: 'teleport', arcane_torrent: 'aoe',
  // rogue
  backstab: 'single', poison_blade: 'buff', vanish: 'buff', multishot: 'aoe', arrow_rain: 'aoe', shadow_step: 'shadow_step',
  death_mark: 'death_mark', piercing_arrow: 'aoe', poison_arrow: 'single', explosive_trap: 'aoe', poison_cloud: 'aoe',
  slow_trap: 'slow_trap', chain_trap: 'aoe',
};

/** Passive skills (classes-stats-skills §1.4, Q4 FIX / DECISIONS C1: not castable, not on the hotbar). */
const PASSIVE: Record<string, { kind: 'regen' | 'lowHpProc' | 'dualWield'; [k: string]: unknown }> = {
  // ZoneScene.ts:1372-1379 — +2 HP/s per level, scaled by the campfire/poison regen multiplier.
  life_regen: { kind: 'regen', hpPerSecondPerLevel: 2 },
  // ZoneScene.ts:1381-1394 — pushes its damageReduction buff when hp/maxHp < 0.3 and off cooldown.
  unyielding: { kind: 'lowHpProc', hpRatioBelow: 0.3 },
  // ZoneScene.ts:1396-1410 — weapon + offhand equipped: damageBonus 0.03 × level, 2000 ms, tag dualWieldMastery.
  dual_wield_mastery: { kind: 'dualWield', damageBonusPerLevel: 0.03, buffDurationMs: 2000, tag: 'dualWieldMastery' },
};

/** Ground-anchored AoE (lands on the enemy within range + 1). Replaces GROUND_AOE_SKILLS (ZoneScene.ts:95). */
const GROUND_ANCHORED = ['meteor', 'blizzard', 'fire_wall', 'arcane_torrent', 'arrow_rain', 'poison_cloud'];

/**
 * Projectile flight before a single-target hit lands: `clamp(isoPx * msPerPx, minMs, maxMs)` where isoPx is
 * the iso screen distance caster→target (both raised 16 px). Replaces the switch in
 * SkillEffectSystem.getProjectileTravelMs (SkillEffectSystem.ts:38-48).
 */
const PROJECTILE: Record<string, { minMs: number; maxMs: number; msPerPx: number }> = {
  fireball: { minMs: 300, maxMs: 600, msPerPx: 1.5 },
  ice_arrow: { minMs: 250, maxMs: 500, msPerPx: 1.5 },
  poison_arrow: { minMs: 250, maxMs: 500, msPerPx: 1.5 },
};

/** Whole-AoE impact delay (meteor fall, `METEOR_FALL_MS`, SkillEffectSystem.ts:10,46). */
const AOE_DELAY_MS: Record<string, number> = { meteor: 300 };

/** Per-target arrow delay `min(maxMs, isoPx * msPerPx)` for AoE arrows (ZoneScene.ts:2724-2728). */
const ARROW_DELAY: Record<string, { maxMs: number; msPerPx: number }> = {
  multishot: { maxMs: 260, msPerPx: 1.1 },
  piercing_arrow: { maxMs: 260, msPerPx: 1.1 },
};

/** Line-shaped AoE toward the target (ZoneScene.ts:2708-2710, monstersAlongLine :2321-2333). */
const LINE_TARGET: Record<string, { halfWidthFactor: number }> = { piercing_arrow: { halfWidthFactor: 0.6 } };

/** Released immediately, not on the cast beat (ZoneScene.ts:2518). */
const INSTANT_RELEASE = ['teleport', 'shadow_step'];

/** Damage ×1.5 (floor) against a burning target (ZoneScene.ts:2740-2742, 2777-2779). */
const BONUS_VS_STATUS: Record<string, { status: StatusType; mul: number }> = { combustion: { status: 'burn', mul: 1.5 } };

/** Taunt the monsters within the scaled aoeRadius (ZoneScene.ts:2677-2687). */
const TAUNT_AOE = ['taunt_roar'];

/**
 * Status effects applied after each hit of the AoE / single-target paths (ZoneScene.applySkillStatusEffect,
 * ZoneScene.ts:6244-6291), in evaluation order. Replaces the damageType + id substring rules
 * ('meteor'/'fireball' → 60 % burn, 'freeze'/blizzard → freeze, 'bleed'/'lacerate'/'rend' → bleed).
 * Buff / teleport / shadow_step / death_mark skills never reach that function, so they have none.
 * slow_trap applies its own slow (ZoneScene.ts:2638-2661): value round(buffValue(L)*100), buffDuration(L).
 */
const burn = (chance: number): StatusRule => ({ status: 'burn', chance, value: { kind: 'damage', fraction: 0.15, min: 1 }, durationMs: 3000 });
const POISON: StatusRule = { status: 'poison', chance: null, value: { kind: 'damage', fraction: 0.2, min: 1 }, durationMs: 4000 };
const STATUS_RULE: Record<string, StatusRule[]> = {
  war_stomp: [{ status: 'stun', chance: null, value: { kind: 'fixed', amount: 1 }, durationMs: 2000 }],
  bleed_strike: [{ status: 'bleed', chance: null, value: { kind: 'damage', fraction: 0.25, min: 1 }, durationMs: 5000 }],
  fireball: [burn(0.6)],
  meteor: [burn(0.6)],
  blizzard: [{ status: 'freeze', chance: null, value: { kind: 'fixed', amount: 1 }, durationMs: 2000 }],
  fire_wall: [burn(0.4)],
  combustion: [burn(0.4)],
  ice_arrow: [{ status: 'slow', chance: 0.35, value: { kind: 'fixed', amount: 40 }, durationMs: 3000 }],
  freeze: [{ status: 'freeze', chance: null, value: { kind: 'fixed', amount: 1 }, durationMs: 2000 }],
  poison_arrow: [POISON],
  explosive_trap: [burn(0.4)],
  poison_cloud: [POISON],
  slow_trap: [{ status: 'slow', chance: null, value: { kind: 'buffPercent' }, durationMs: null }],
  chain_trap: [{ status: 'stun', chance: null, value: { kind: 'fixed', amount: 1 }, durationMs: 1000 }],
};

/**
 * Impact-burst colour (0xRRGGBB). Replaces skillImpactColor (ZoneScene.ts:98-105): 'fire'/meteor/combustion →
 * 0xff6600, 'ice'/blizzard/freeze → 0x4488ff, 'lightning' → 0x5dade2, 'poison' → 0x7ed957, arcane_torrent →
 * 0xb07cff, else physical 0xf1c40f / other 0xf39c12.
 */
const IMPACT_COLOR: Record<string, number> = {
  slash: 0xf1c40f, whirlwind: 0xf1c40f, war_stomp: 0xf1c40f, shield_wall: 0xf1c40f, taunt_roar: 0xf1c40f,
  vengeful_wrath: 0xf1c40f, charge: 0xf1c40f, lethal_strike: 0xf1c40f, dual_wield_mastery: 0xf1c40f,
  iron_fortress: 0xf1c40f, unyielding: 0xf1c40f, life_regen: 0xf1c40f, frenzy: 0xf1c40f, bleed_strike: 0xf1c40f,
  rampage: 0xf1c40f,
  fireball: 0xff6600, meteor: 0xff6600, blizzard: 0x4488ff, ice_armor: 0x4488ff, chain_lightning: 0x5dade2,
  mana_shield: 0xf39c12, fire_wall: 0xff6600, combustion: 0xff6600, ice_arrow: 0x4488ff, freeze: 0x4488ff,
  teleport: 0xf39c12, arcane_torrent: 0xb07cff,
  backstab: 0xf1c40f, poison_blade: 0x7ed957, vanish: 0xf1c40f, multishot: 0xf1c40f, arrow_rain: 0xf1c40f,
  shadow_step: 0xf1c40f, death_mark: 0xf1c40f, piercing_arrow: 0xf1c40f, poison_arrow: 0x7ed957,
  explosive_trap: 0xf39c12, poison_cloud: 0x7ed957, slow_trap: 0xf1c40f, chain_trap: 0xf1c40f,
};

/**
 * Ground mark after a single-target hit with `damageType != physical || damageMultiplier > 1.5`
 * (ZoneScene.ts:2788-2793). `scorch` maps by damageType (combat-feel §6.4 recommended fix, DECISIONS C1);
 * `scorchWeb` keeps the web's id rule ('fire'/meteor → fire, 'ice'/blizzard → ice, else lightning — so
 * poison_arrow, combustion, freeze and heavy physical hits drew a lightning mark).
 */
const SCORCH: Record<string, string> = {
  charge: 'physical', lethal_strike: 'physical', bleed_strike: 'physical', backstab: 'physical',
  fireball: 'fire', combustion: 'fire', ice_arrow: 'ice', freeze: 'ice', poison_arrow: 'poison',
};
const SCORCH_WEB: Record<string, string> = {
  charge: 'lightning', lethal_strike: 'lightning', bleed_strike: 'lightning', backstab: 'lightning',
  fireball: 'fire', combustion: 'lightning', ice_arrow: 'ice', freeze: 'lightning', poison_arrow: 'lightning',
};

/**
 * Port additions decided in DECISIONS (not web data). C4: Charge dashes to the target (0.25 s, stops at melee
 * range); Fire Wall, Arrow Rain and the traps become persistent ground effects with the same total damage spread
 * over their ticks (each tick deals 1 / groundTicks of the skill hit); Chain Lightning jumps target-to-target 55 ms
 * apart; Multishot fans in a 50° cone. C1/C8: Teleport clamps to 8 tiles; touch aims joystick × 6 tiles (deadzone
 * 0.2, ZoneScene.ts:2547-2551).
 *
 * Ground effects (SkillData.h GroundTrigger): `periodic` ticks at once, then every groundDurationMs / groundTicks
 * until groundDurationMs; `armed` (traps, as their web descriptions say: "放置陷阱，触发时…") waits up to
 * groundDurationMs for a living monster inside the radius, then fires its ticks and ends. Lifetimes follow the web
 * skill descriptions where they give one (arrow_rain "持续3秒"); fire_wall matches arrow_rain; traps stay armed
 * 10 s (longer than every trap cooldown). PROPOSED port values — tune after the Chapter 1 playtest (C4).
 */
const GROUND = (groundDurationMs: number, groundTicks: number, groundTrigger: 'periodic' | 'armed') =>
  ({ persistentGround: true, groundDurationMs, groundTicks, groundTrigger, decision: 'C4' });
const DECISION_FIELDS: Record<string, Record<string, unknown>> = {
  charge: { dash: { durationMs: 250, stopAtMeleeRange: true }, decision: 'C4' },
  fire_wall: GROUND(3000, 6, 'periodic'),
  arrow_rain: GROUND(3000, 6, 'periodic'),
  explosive_trap: GROUND(10000, 1, 'armed'),
  slow_trap: GROUND(10000, 1, 'armed'),
  chain_trap: GROUND(10000, 1, 'armed'),
  chain_lightning: { chainStaggerMs: 55, decision: 'C4' },
  multishot: { coneDeg: 50, decision: 'C4' },
  teleport: { maxRangeTiles: 8, touchJoystickTiles: 6, touchDeadzone: 0.2, walkableSearchRings: 3, decision: 'C1, C8' },
};

// ── Reference transcriptions of the web rules (used only to cross-check the tables) ──────────

function webExecKind(s: SkillDefinition): ExecKind {
  if (s.id === 'teleport') return 'teleport';
  if (s.id === 'shadow_step') return 'shadow_step'; // `&& target`: without one it falls through (Q8)
  if (s.id === 'death_mark') return 'death_mark';
  if (s.id === 'slow_trap') return 'slow_trap';
  if (s.buff) return 'buff';
  if (s.aoe && (s.aoeRadius ?? 0) > 0) return 'aoe';
  return 'single';
}

function webScorch(s: SkillDefinition): string | null {
  if (!(s.damageType !== 'physical' || s.damageMultiplier > 1.5)) return null;
  return s.id.includes('fire') || s.id === 'meteor' ? 'fire' : s.id.includes('ice') || s.id === 'blizzard' ? 'ice' : 'lightning';
}

/** Calls the real ZoneScene.applySkillStatusEffect with a recording stub and scripted Math.random. */
function probeStatus(skill: SkillDefinition, damage: number, rand: number): { type: string; value: number; duration: number }[] {
  const out: { type: string; value: number; duration: number }[] = [];
  const fake = { statusEffects: { apply: (_id: string, type: string, value: number, duration: number) => { out.push({ type, value, duration }); return duration; } } };
  const target = { id: 'probe', isAlive: () => true };
  const proto = ZoneScene.prototype as unknown as Record<string, (...a: unknown[]) => void>;
  withRandom(new Array(16).fill(rand), () => proto.applySkillStatusEffect.call(fake, target, skill, damage, 0));
  return out;
}

/** Threshold of a `Math.random() < p` roll found by bisection on the real code. */
function probeChance(skill: SkillDefinition): number {
  let lo = 0, hi = 1;
  for (let i = 0; i < 60; i++) {
    const mid = (lo + hi) / 2;
    if (probeStatus(skill, 1000, mid).length > 0) lo = mid; else hi = mid;
  }
  return hi;
}

function checkStatusRule(skill: SkillDefinition, kind: ExecKind, rules: StatusRule[]): void {
  if (kind === 'slow_trap') return; // own branch, pinned by source assertion below
  if (kind !== 'aoe' && kind !== 'single') {
    assert(rules.length === 0, `${skill.id}: ${kind} skills never call applySkillStatusEffect`);
    return;
  }
  const always = probeStatus(skill, 1000, 0);
  const never = probeStatus(skill, 1000, 0.999999999);
  const zeroDmg = probeStatus(skill, 0, 0);
  assert(always.length === rules.length, `${skill.id}: statusRule count ${rules.length} != web ${always.length}`);
  rules.forEach((r, i) => {
    const w = always[i];
    assert(w.type === r.status, `${skill.id}: status ${r.status} != web ${w.type}`);
    if (r.value.kind === 'fixed') assert(w.value === r.value.amount && zeroDmg[i].value === r.value.amount, `${skill.id}: fixed value`);
    if (r.value.kind === 'damage') {
      assert(w.value === Math.max(r.value.min, Math.floor(1000 * r.value.fraction)), `${skill.id}: damage fraction`);
      assert(zeroDmg[i].value === r.value.min, `${skill.id}: min value`);
    }
    assert(w.duration === r.durationMs, `${skill.id}: duration ${r.durationMs} != web ${w.duration}`);
    if (r.chance === null) assert(never.some(n => n.type === r.status), `${skill.id}: ${r.status} should not roll`);
    else {
      assert(!never.some(n => n.type === r.status), `${skill.id}: ${r.status} should roll`);
      const p = probeChance(skill);
      assert(Math.abs(p - r.chance) < 1e-12, `${skill.id}: chance ${r.chance} != web ${p}`);
    }
  });
}

function checkProjectile(skill: SkillDefinition): void {
  const proto = SkillEffectSystem.prototype as unknown as { getProjectileTravelMs: (...a: number[] | string[] | unknown[]) => number };
  const travel = (px: number): number => proto.getProjectileTravelMs.call({}, skill.id, 0, 0, px, 0);
  const p = PROJECTILE[skill.id];
  const fall = AOE_DELAY_MS[skill.id];
  for (const px of [0, 50, 180, 270, 1000]) {
    const want = p ? Math.max(p.minMs, Math.min(p.maxMs, px * p.msPerPx)) : fall ?? 0;
    assert(travel(px) === want, `${skill.id}: projectile travel at ${px}px ${travel(px)} != table ${want}`);
  }
}

// ── Table ───────────────────────────────────────────────────────────────────

export function exportSkills(): TableResult[] {
  const groundSet = ZoneSceneMod as unknown as Record<string, unknown>;
  const GROUND_AOE_SKILLS = exposed<Set<string>>(groundSet, 'GROUND_AOE_SKILLS');
  const skillImpactColor = exposed<(id: string, t: string) => number>(groundSet, 'skillImpactColor');
  assert(METEOR_FALL() === 300, 'METEOR_FALL_MS');

  // Pin the rules transcribed above to the web source.
  assertSource('src/scenes/ZoneScene.ts',
    "if (skillId === 'teleport') {",
    "if (skillId === 'shadow_step' && target) {",
    "if (skillId === 'death_mark' && target) {",
    "if (skillId === 'slow_trap') {",
    'if (skill.aoe && scaledAoeRadius > 0) {',
    "if (skillId === 'teleport' || skillId === 'shadow_step' || releaseDelay <= 0) {",
    'if (skill.buff || skill.aoe || skill.range > 2) {',
    "if (skillId !== 'piercing_arrow' && skillId !== 'multishot') return aoeDelay;",
    'return Math.min(260, d * 1.1);',
    "skillId === 'piercing_arrow' && target ? this.monstersAlongLine(target, skill.range, scaledAoeRadius * 0.6)",
    "if (skillId === 'combustion' && this.statusEffects.hasEffect(t.id, 'burn')) { finalDmg = Math.floor(finalDmg * 1.5);",
    "if (skillId === 'taunt_roar' && skill.aoe && skill.aoeRadius) {",
    "if (this.trails && (skill.damageType !== 'physical' || skill.damageMultiplier > 1.5)) {",
    "const scorchType = skillId.includes('fire') || skillId === 'meteor' ? 'fire' : skillId.includes('ice') || skillId === 'blizzard' ? 'ice' : 'lightning';",
    'const slowValue = skill.buff ? Math.round(getSkillBuffValue(skill, level) * 100) : 40;',
    'const slowDuration = skill.buff ? getSkillBuffDuration(skill, level) : 5000;',
    'const regenBonus = lifeRegenLevel * 2;',
    'if (hpRatio < 0.3 && unyieldingSkill',
    'const bonusValue = dualWieldLevel * 0.03;',
    "this.player.buffs.push({ stat: 'damageBonus', value: bonusValue, duration: 2000, startTime: time, tag: 'dualWieldMastery' });",
    'tile = len > 0.2',
    '? { col: this.player.tileCol + (dir.dx / len) * 6, row: this.player.tileRow + (dir.dy / len) * 6 }',
    'for (let r = 1; r <= 3 && !found; r++) {',
  );

  const allSkills: SkillDefinition[] = [];
  const classes = Object.values(AllClasses).map(cls => ({
    ...plain(cls),
    skills: cls.skills.map(skill => {
      allSkills.push(skill);
      const id = skill.id;
      const kind = EXEC_KIND[id];
      assert(kind, `skill ${id} has no EXEC_KIND mapping`);
      assert(kind === webExecKind(skill), `${id}: execKind ${kind} != web ${webExecKind(skill)}`);
      const statusRule = STATUS_RULE[id] ?? [];
      checkStatusRule(skill, kind, statusRule);
      checkProjectile(skill);
      assert(GROUND_ANCHORED.includes(id) === GROUND_AOE_SKILLS.has(id), `${id}: groundAnchored`);
      assert(IMPACT_COLOR[id] !== undefined, `${id}: no IMPACT_COLOR`);
      assert(IMPACT_COLOR[id] === skillImpactColor(id, skill.damageType), `${id}: impactColor != web skillImpactColor`);
      const web = kind === 'single' ? webScorch(skill) : null;
      assert((SCORCH_WEB[id] ?? null) === web, `${id}: scorchWeb ${SCORCH_WEB[id]} != web ${web}`);
      assert((SCORCH[id] ?? null) === (web ? skill.damageType : null), `${id}: scorch must be damageType when a mark is drawn`);
      assert(!(skill.buff?.stat === 'hp' || id.includes('heal')), `${id}: heal-burst rule now matches a skill — add a field`);
      const animKind = skill.buff || skill.aoe || skill.range > 2 ? 'cast' : 'attack';
      return {
        ...plain(skill),
        derived: {
          execKind: kind,
          passive: id in PASSIVE,
          passiveRule: PASSIVE[id] ?? null,
          groundAnchored: GROUND_ANCHORED.includes(id),
          projectile: PROJECTILE[id] ?? null,
          aoeDelayMs: AOE_DELAY_MS[id] ?? 0,
          arrowDelay: ARROW_DELAY[id] ?? null,
          lineTarget: LINE_TARGET[id] ?? null,
          animKind,
          instantRelease: INSTANT_RELEASE.includes(id),
          requiresTarget: kind === 'single' || kind === 'death_mark' || kind === 'shadow_step',
          rangeCheck: kind === 'single' || kind === 'death_mark' || kind === 'shadow_step',
          statusRule,
          bonusVsStatus: BONUS_VS_STATUS[id] ?? null,
          tauntAoe: TAUNT_AOE.includes(id),
          impactColor: IMPACT_COLOR[id],
          scorch: SCORCH[id] ?? null,
          scorchWeb: SCORCH_WEB[id] ?? null,
          vfxId: id,
          port: DECISION_FIELDS[id] ?? null,
        },
      };
    }),
  }));
  for (const tableName of ['EXEC_KIND', 'IMPACT_COLOR'] as const) {
    const table = tableName === 'EXEC_KIND' ? EXEC_KIND : IMPACT_COLOR;
    for (const id of Object.keys(table)) assert(allSkills.some(s => s.id === id), `${tableName} lists unknown skill ${id}`);
  }
  for (const id of [...Object.keys(STATUS_RULE), ...Object.keys(PASSIVE), ...GROUND_ANCHORED, ...Object.keys(PROJECTILE),
    ...Object.keys(SCORCH), ...Object.keys(DECISION_FIELDS), ...Object.keys(ARROW_DELAY)]) {
    assert(allSkills.some(s => s.id === id), `mapping table lists unknown skill ${id}`);
  }
  assert(allSkills.length === 40, `expected 40 skills, found ${allSkills.length}`);

  // ── skill_trees.json ──
  // Colours: UIScene.ts:2041-2045 (TREE_COLORS) and :2046-2049 (DMG_COLORS); names: data.skillTree.<id>.
  assertSource('src/scenes/UIScene.ts',
    'combat_master: 0xd4a017, guardian: 0xf1c40f, berserker: 0xcc3333,',
    'fire: 0xe74c3c, frost: 0x5dade2, arcane: 0x8e44ad,',
    'assassination: 0x27ae60, archery: 0xcc8844, traps: 0xff6600,',
    'physical: 0xcccccc, fire: 0xff6633, ice: 0x66ccff,',
    'lightning: 0x5dade2, poison: 0x33cc33, arcane: 0xbb77ff,');
  const TREE_COLORS: Record<string, number> = {
    combat_master: 0xd4a017, guardian: 0xf1c40f, berserker: 0xcc3333,
    fire: 0xe74c3c, frost: 0x5dade2, arcane: 0x8e44ad,
    assassination: 0x27ae60, archery: 0xcc8844, traps: 0xff6600,
  };
  const DAMAGE_TYPE_COLORS: Record<string, number> = {
    physical: 0xcccccc, fire: 0xff6633, ice: 0x66ccff, lightning: 0x5dade2, poison: 0x33cc33, arcane: 0xbb77ff,
  };
  const trees: Record<string, unknown>[] = [];
  for (const cls of Object.values(AllClasses)) {
    for (const s of cls.skills) {
      if (trees.some(t => t.id === s.tree)) continue;
      assert(TREE_COLORS[s.tree] !== undefined, `tree ${s.tree} has no colour`);
      trees.push({ id: s.tree, classId: cls.id, tabOrder: trees.filter(t => t.classId === cls.id).length,
        nameKey: `data.skillTree.${s.tree}`, color: TREE_COLORS[s.tree] });
    }
  }

  // ── skill_rules.json ──
  const tierPlayerLevel = exposed<Record<number, number>>(SkillProgression as never, 'TIER_PLAYER_LEVEL');
  const tierTreePoints = exposed<Record<number, number>>(SkillProgression as never, 'TIER_TREE_POINTS');
  const tieredScale = exposed<(per: number, level: number) => number>(CombatSystemMod as never, 'tieredScale');
  assertSource('src/systems/SkillProgressionSystem.ts',
    'TIER_PLAYER_LEVEL[skill.tier] ?? 1 + (skill.tier - 1) * 6;',
    'TIER_TREE_POINTS[skill.tier] ?? Math.max(0, (skill.tier - 1) * 5);',
    'limit = 6,');
  assertSource('src/systems/CombatSystem.ts',
    'if (i <= 8) total += perLevel;', 'else if (i <= 16) total += perLevel * 0.75;', 'else total += perLevel * 0.5;',
    'skill.scaling?.damagePerLevel ?? 0.05', 'skill.scaling?.manaCostPerLevel ?? 0.5', 'skill.scaling?.buffValuePerLevel ?? 0.02',
    'const raw = Math.max(500, Math.floor(base - tieredScale(perLevel, level)));', 'clamp(cdr, 0, 50)');
  // tieredScale weights reproduced from the real function.
  const w = (lv: number) => tieredScale(1, lv) - tieredScale(1, lv - 1);
  assert(w(2) === 1 && w(8) === 1 && w(9) === 0.75 && w(16) === 0.75 && w(17) === 0.5 && w(20) === 0.5, 'tier weights');

  const rules = {
    tierPlayerLevel,
    tierTreePoints,
    // Unlisted tiers: playerLevel = 1 + (tier-1)*6, treePoints = max(0, (tier-1)*5) (SkillProgressionSystem.ts:79-80).
    tierFallback: { playerLevel: { base: 1, perTierAbove1: 6 }, treePoints: { perTierAbove1: 5, min: 0 } },
    starterLevels: { tier1: 1, other: 0 },
    loadoutSize: 6,
    groundAoeSkills: GROUND_ANCHORED,
    scaling: {
      tierWeights: [{ fromLevel: 2, toLevel: 8, weight: 1 }, { fromLevel: 9, toLevel: 16, weight: 0.75 }, { fromLevel: 17, toLevel: null, weight: 0.5 }],
      defaults: { damagePerLevel: 0.05, manaCostPerLevel: 0.5, cooldownReductionPerLevel: 0, aoeRadiusPerLevel: 0,
        buffValuePerLevel: 0.02, buffDurationPerLevel: 0 },
      cooldownFloorMs: 500,
      cooldownReductionCapPercent: 50,
    },
    targeting: { rangeSlackTiles: 1, groundAnchorReachSlackTiles: 1 },
    gamepadSkillButtons: [0, 2, 3, 5],
    passiveSkills: Object.keys(PASSIVE),
  };
  assertSource('src/scenes/ZoneScene.ts', 'const skillButtonIndices = [0, 2, 3, 5];');

  const skillCount = allSkills.length;
  return [
    {
      file: 'classes.json',
      source: ['src/data/classes/index.ts', 'src/data/classes/warrior.ts', 'src/data/classes/mage.ts', 'src/data/classes/rogue.ts',
        'src/scenes/ZoneScene.ts', 'src/systems/SkillEffectSystem.ts'],
      data: { classOrder: Object.keys(AllClasses), classes },
      counts: { classes: classes.length, skills: skillCount },
    },
    {
      file: 'skill_trees.json',
      source: ['src/data/classes/*.ts', 'src/scenes/UIScene.ts', 'src/i18n/locales/zh-CN.ts'],
      data: { trees, damageTypeColors: DAMAGE_TYPE_COLORS },
      counts: { trees: trees.length },
    },
    {
      file: 'skill_rules.json',
      source: ['src/systems/SkillProgressionSystem.ts', 'src/systems/CombatSystem.ts', 'src/scenes/ZoneScene.ts'],
      data: rules,
      counts: { tiers: Object.keys(tierPlayerLevel).length },
    },
  ];
}

function METEOR_FALL(): number {
  return exposed<number>(SkillEffectMod as never, 'METEOR_FALL_MS');
}

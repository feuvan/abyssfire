import type { MonsterDefinition, RemakeCorpse, RemakeRiseAgain } from '../types';

/**
 * Chapter 1 roster of the dark remake (赤幡原 / The Red Field): Docs/dark-remake/monsters.md §2.
 *
 * Four families (monsters.md §1.1): the Unburied (risen crusaders), the Living (Red-Oath zealots, deserters), the
 * Gravegnawers (carrion things) and Breach-blood (clots, exploding bloats). Stats are the §2.0 table; every monster runs
 * on today's core (one Attack01, melee, or a bolt when attackRange > 2.5) until the Wave 2 systems land. `spriteKey` is
 * the art lane's plain remake id (`monster_<id>`, Art/blender/march/march_palettes.py); until the asset ships, the core's
 * art chain (MonsterArtGameId) misses and the anim_timing rig fallback times the swing.
 *
 * Exp (1B review, DECISIONS DR9 *as built*): every regular gives round(0.75 x its §2.0 Exp) and Valdras 110 (half),
 * so a full Chapter 1 ends in the playthrough band (Lv 12-14) that DR9 anchors; monsters.md §2.0 records the change.
 *
 * Order matters: the zone list's first entry is the random-event fallback monster (the breachclot, the weakest). The
 * mini-boss lives in ../miniBosses.ts, the hunts in ../quests/all_quests.ts (`hunts`); Valdras is the story boss
 * (../config.ts, DECISIONS M7). `deserter_drummer` spawns only in the Wave 2 Deserters' Hollow mix (monsters.md §2.3);
 * `rollcall_standard_bearer` (last) only as Valdras's P2 summon.
 *
 * Wave 2 (package 2F, implementation-plan.md *2F outcomes*): every definition caps a hit at 35 % of the hero's maximum
 * life on Normal (monsters.md §1.3, K2 `hitCapHeroMaxHpPct`) and says what its body leaves (§1.5, K3 `corpse`); the
 * Unburied regulars carry the family trait (rise once, 25 %, 3 s, at 40 %, not after a crit or fire), the gnawers eat
 * corpses and wake their pack, the cinderbloat bursts, the arbalester kites, the shieldbearer blocks and holds a wall,
 * the standard-bearer musters and inspires, the deserters break when their leader falls unless the drummer drums, the
 * zealot frenzies, the bonegorger slams (and Hungerbell calls the field), and Valdras fights in three phases (chapter1.md
 * §11). Every ability authors its `windupMs` (the contact; the fairness floors are 600 ms, 700 ms for a boss special),
 * and its clip (Art/Export/manifest.json) plays at the rate that lands its Contact / Release beat there. Telegraphs are
 * the crimson enemy style (DR5).
 */

/** monsters.md §1.3: no single enemy hit above 35 % of the hero's maximum life on Normal (scaled on Nightmare / Hell). */
const HIT_CAP = 35;
/** monsters.md §1.5: Unburied and Living bodies stay as corpses (raised, eaten, burned). */
const CORPSE: RemakeCorpse = { leaves: 'corpse' };
/** monsters.md §2.2: the 不葬 Unburied family trait: a 25 % chance to rise once, 3 s after death, at 40 % life; a fire
 * killing blow, a crit or a burned corpse stops it (K3 `riseAgain`). */
const UNBURIED_TRAIT: RemakeRiseAgain = { chance: 0.25, delayMs: 3000, hpPct: 40, notOnCrit: true };
/** monsters.md §4: breach-blood contact numbs: one Ashnumb stack per landed hit, 6 s (the stacks share the timer). */
const ASHNUMB_ON_HIT = { status: 'ashnumb', chance: 1, value: { kind: 'fixed', amount: 1 }, durationMs: 6000 } as const;
export const EmeraldPlainsMonsters: MonsterDefinition[] = [
  // breachclot: Breach-blood, fodder
  {
    id: 'breachclot',
    name: '渊血凝块',
    level: 1,
    hp: 28,
    damage: 5,
    defense: 2,
    speed: 38,
    aggroRange: 4,
    attackRange: 1.2,
    attackSpeed: 1500,
    expReward: 8,
    goldReward: [2, 4],
    spriteKey: 'monster_breachclot',
    animCategory: 'slime',
    family: 'breach',
    // W2: Ashnumb replaces W1's poison (20 % for 3 s; monsters.md §4).
    onHitStatus: [ASHNUMB_ON_HIT],
    hitCapHeroMaxHpPct: HIT_CAP,
    // It sinks into a tar puddle that dries (presentation): no corpse entry (monsters.md §1.5).
    corpse: { leaves: 'none' },
  },
  // risen_footman: the Unburied, shambler
  {
    id: 'risen_footman',
    name: '复起步卒',
    level: 2,
    hp: 50,
    damage: 7,
    defense: 5,
    speed: 32,
    aggroRange: 4,
    attackRange: 1.5,
    attackSpeed: 1600,
    expReward: 11,
    goldReward: [2, 5],
    spriteKey: 'monster_risen_footman',
    animCategory: 'humanoid',
    family: 'unburied',
    hitCapHeroMaxHpPct: HIT_CAP,
    corpse: CORPSE,
    riseAgain: UNBURIED_TRAIT,
  },
  // gravegnawer: the Gravegnawers, swarmer
  {
    id: 'gravegnawer',
    name: '啃冢者',
    level: 3,
    hp: 42,
    damage: 7,
    defense: 3,
    speed: 66,
    aggroRange: 6,
    attackRange: 1.3,
    attackSpeed: 950,
    expReward: 13,
    goldReward: [1, 4],
    spriteKey: 'monster_gravegnawer',
    animCategory: 'humanoid',
    family: 'gravegnawers',
    hitCapHeroMaxHpPct: HIT_CAP,
    // Provoking one wakes its pack within 6 tiles (monsters.md §2.4, K5).
    packAggro: 6,
    // A carcass the other gnawers ignore; it eats corpses (out of combat or under half life: 2 s, +30 %, the body gone).
    corpse: { leaves: 'remains', consumes: true },
  },
  // cinderbloat: Breach-blood, exploder
  {
    id: 'cinderbloat',
    name: '烬胀囊',
    level: 4,
    hp: 40,
    damage: 4,
    defense: 2,
    speed: 30,
    aggroRange: 5,
    attackRange: 1.2,
    attackSpeed: 1800,
    expReward: 14,
    goldReward: [1, 3],
    spriteKey: 'monster_cinderbloat',
    animCategory: 'slime',
    family: 'breach',
    // A slow tank that burns on hit (30 %, W1) and numbs (breach-blood contact, monsters.md §4); its on-death burst is
    // its real threat.
    onHitStatus: [
      { status: 'burn', chance: 0.3, value: { kind: 'monsterDamage', fraction: 0.2, min: 1 }, durationMs: 3000 },
      ASHNUMB_ON_HIT,
    ],
    hitCapHeroMaxHpPct: HIT_CAP,
    corpse: { leaves: 'none' },
    // monsters.md §2.1: an 800 ms fuse under a crimson ring r2.2, then 18 breach fire, at most 20 % of the hero's life on
    // Normal, x2 against monsters; fire burns the corpses in it.
    onDeath: {
      kind: 'burst', delayMs: 800, shape: { kind: 'circle', radius: 2.2 }, dmg: 18, damageType: 'fire', hitsMonsters: true,
      monsterDmgMul: 2, capHeroMaxHpPct: 20,
    },
  },
  // risen_arbalest: the Unburied, ranged
  {
    id: 'risen_arbalest',
    name: '复起弩手',
    level: 5,
    hp: 46,
    damage: 11,
    defense: 4,
    speed: 34,
    aggroRange: 7,
    attackRange: 6.0,
    attackSpeed: 2200,
    expReward: 18,
    goldReward: [3, 6],
    spriteKey: 'monster_risen_arbalest',
    animCategory: 'humanoid',
    family: 'unburied',
    // The first ranged regular: a bone-white bolt with a breach-violet trail (DR5: never ember).
    projectileColor: 0xb02fd0,
    hitCapHeroMaxHpPct: HIT_CAP,
    // It steps back to 6 tiles when the hero closes within 3 (monsters.md §2.2: the band 3-6, its range 6), then shoots.
    kite: { min: 3, max: 6 },
    corpse: CORPSE,
    riseAgain: UNBURIED_TRAIT,
  },
  // oath_zealot: the Living, frenzy
  {
    id: 'oath_zealot',
    name: '赤誓狂信徒',
    level: 6,
    hp: 90,
    damage: 11,
    defense: 6,
    speed: 52,
    aggroRange: 6,
    attackRange: 1.5,
    attackSpeed: 1100,
    expReward: 23,
    goldReward: [4, 8],
    spriteKey: 'monster_oath_zealot',
    animCategory: 'humanoid',
    family: 'living',
    // The flail bleeds you (15 %).
    onHitStatus: [{ status: 'bleed', chance: 0.15, value: { kind: 'monsterDamage', fraction: 0.25, min: 1 }, durationMs: 5000 }],
    hitCapHeroMaxHpPct: HIT_CAP,
    corpse: CORPSE,
    // monsters.md §2.3: at <= 50 % life it frenzies (a one-row phase table, K4): attack speed x1.35, move x1.2 and +10 %
    // damage taken, at once. 2F review: no enter action - a K4 Roar roots and cancels the windup for its 1.2 s clip, which
    // read as a stagger at the moment it should surge; UE plays the frenzy look (fx.frenzy: the robe whips, the oath-scar
    // flares) on its EvMonsterPhase. Never flees (no morale).
    phases: [{ hpBelowPct: 50, attackSpeedMul: 1.35, speedMul: 1.2, damageTakenMul: 1.1 }],
  },
  // deserter: the Living, skirmisher
  {
    id: 'deserter',
    name: '逃兵',
    level: 6,
    hp: 80,
    damage: 12,
    defense: 7,
    speed: 56,
    aggroRange: 5,
    attackRange: 1.6,
    attackSpeed: 1150,
    expReward: 21,
    goldReward: [6, 12],
    spriteKey: 'monster_deserter',
    animCategory: 'humanoid',
    family: 'living',
    hitCapHeroMaxHpPct: HIT_CAP,
    corpse: CORPSE,
    // monsters.md §2.3: when the pack's leader dies, every deserter within 8 tiles of the body flees for 5 s, unless a
    // living drummer (the Living's sustain: his drum grants fearImmune) stands within 8 tiles. Dreck's and Harke's men
    // are their hunt leader's pack (K5).
    morale: { fleeOnLeaderDeathMs: 5000, fleeRadius: 8, sustainFamily: 'living', sustainRadius: 8 },
  },
  // deserter_drummer: the Living, sustain (no Wave 1 spawn)
  {
    id: 'deserter_drummer',
    name: '军鼓手',
    level: 6,
    hp: 70,
    damage: 8,
    defense: 5,
    speed: 50,
    aggroRange: 6,
    attackRange: 1.5,
    attackSpeed: 1400,
    expReward: 21,
    goldReward: [4, 9],
    spriteKey: 'monster_deserter_drummer',
    animCategory: 'humanoid',
    family: 'living',
    hitCapHeroMaxHpPct: HIT_CAP,
    corpse: CORPSE,
    // monsters.md §2.3: he drums (a 2 s channel every 8 s; Cast01, its 1000 ms loop): the call wakes the Living within 8
    // tiles and makes them fearless for 6 s; while he lives his pack is held (morale `sustainFamily`) and attacks 10 %
    // faster (the aura). Kill order: the drummer, then the leader (the drum ends with him).
    abilities: [
      {
        id: 'war_drum', kind: 'call', clip: 'Cast01', windupMs: 2000, cooldownMs: 8000, initialCooldownMs: 1000, rangeMax: 12,
        aim: 'self', buff: { stat: 'fearImmune', ms: 6000, radius: 8, family: 'living' },
      },
    ],
    aura: { stat: 'attackSpeed', mul: 1.1, radius: 8, family: 'living' },
  },
  // risen_shieldbearer: the Unburied, shield wall
  {
    id: 'risen_shieldbearer',
    name: '复起盾卒',
    level: 7,
    hp: 130,
    damage: 12,
    defense: 14,
    speed: 30,
    aggroRange: 5,
    attackRange: 1.6,
    attackSpeed: 1700,
    expReward: 26,
    goldReward: [3, 7],
    spriteKey: 'monster_risen_shieldbearer',
    animCategory: 'humanoid',
    family: 'unburied',
    hitCapHeroMaxHpPct: HIT_CAP,
    // monsters.md §2.2: the 100 degree front arc takes half (sparks, EvBlock); shieldbearers within 2 tiles share the
    // arc's facing (the line holds). Flanks, ticks and a stunned one get through.
    block: { arcDeg: 100, damageMul: 0.5, wall: 2 },
    corpse: CORPSE,
    riseAgain: UNBURIED_TRAIT,
  },
  // dead_standard_bearer: the Unburied, summoner-aura
  {
    id: 'dead_standard_bearer',
    name: '亡旗手',
    level: 7,
    hp: 140,
    damage: 8,
    defense: 8,
    speed: 30,
    aggroRange: 6,
    attackRange: 1.6,
    attackSpeed: 1700,
    expReward: 30,
    goldReward: [4, 8],
    spriteKey: 'monster_dead_standard_bearer',
    animCategory: 'humanoid',
    family: 'unburied',
    hitCapHeroMaxHpPct: HIT_CAP,
    // monsters.md §2.2 Muster: every 10 s it plants the pole (Cast01, 900 ms = its Release) and one corpse within 6 tiles
    // stands as a footman at 40 % life (a breach-violet marker), at most 2 alive; a cast between swings (K3). Aura: the
    // Unburied within 5 tiles deal +15 % (the crimson halo). The priority target; Valdras's P2 lesson.
    abilities: [
      {
        id: 'muster', kind: 'raise', clip: 'Cast01', windupMs: 900, cooldownMs: 10000, initialCooldownMs: 2000, rangeMax: 12,
        aim: 'corpse',
        summon: {
          monsterId: 'risen_footman', count: 1, at: 'corpse', radius: 6, capAlive: 2, perWindowCap: 1, windowMs: 10000, hpPct: 40,
        },
      },
    ],
    aura: { stat: 'damage', mul: 1.15, radius: 5, family: 'unburied' },
    corpse: CORPSE,
    riseAgain: UNBURIED_TRAIT,
  },
  // bonegorger: the Gravegnawers, bruiser
  {
    id: 'bonegorger',
    name: '饕骸者',
    level: 7,
    hp: 210,
    damage: 17,
    defense: 9,
    speed: 42,
    aggroRange: 6,
    attackRange: 2.0,
    attackSpeed: 1700,
    expReward: 36,
    goldReward: [6, 12],
    spriteKey: 'monster_bonegorger',
    animCategory: 'humanoid',
    family: 'gravegnawers',
    hitCapHeroMaxHpPct: HIT_CAP,
    // monsters.md §2.4 钟锤 Clapper Slam: a circle r2 where the hero stood when it wound up, 800 ms (Attack02's Contact;
    // the bell rings on impact), x1.6, every 6 s.
    abilities: [
      {
        id: 'clapper_slam', kind: 'circle', clip: 'Attack02', windupMs: 800, cooldownMs: 6000, initialCooldownMs: 1500,
        aim: 'heroAtWindup', shape: { radius: 2 }, dmgMul: 1.6,
      },
    ],
    // A carcass (monsters.md §1.5); only the gravegnawers eat.
    corpse: { leaves: 'remains' },
  },
  // valdras: the act boss and story boss (DECISIONS M7, ../config.ts)
  {
    // chapter1.md §11: three phases. P1 死守 Hold the Line (Banner Sweep, Oath-Brand); P2 点卯 Roll Call at 65 % (Roar; he
    // holds the dais at move x0.6 inside a 3-tile tether; a standard-bearer rises at the west ash circle and musters at
    // both; Feed; Breach-seep); P3 不许退 No Retreat under 30 % (Roar; attack speed x1.25, move x1.15, off the dais; Lunge;
    // the banner ring closes from 9 to 5 over 40 s, 4 % max life per second of breach fire outside). Every special
    // winds up >= 700 ms under a crimson decal; no hit above 35 % of the hero's life; no invulnerable phase. Centres
    // default to his authored spawn tile (15,95), the dais (K4), and so does his M1 leash, which covers the ring (a bound
    // fight, 2F review). Numbers: the playthrough gate, per class (2F review: 9000 HP puts every class's median fight in
    // the DR9 band, 30-180 s, for the frame-perfect and the reaction-limited bot alike; 14000 / 32 was tuned on the
    // warrior alone). Damage is the design's 20: at 24 a Brutish Valdras (x1.35) killed the reaction-limited rogue in 10
    // of 18 fights.
    id: 'valdras',
    name: '焚旗元帅·瓦德拉斯',
    level: 9,
    hp: 9000,
    damage: 20,
    defense: 12,
    speed: 48,
    aggroRange: 8,
    attackRange: 2.0,
    attackSpeed: 1300,
    expReward: 110,
    goldReward: [30, 60],
    spriteKey: 'monster_valdras',
    elite: true,
    animCategory: 'humanoid',
    family: 'unburied',
    hitCapHeroMaxHpPct: HIT_CAP,
    // His body is a story beat (cs_rf_marshal_falls): remains, never raised or eaten.
    corpse: { leaves: 'remains' },
    abilities: [
      // 旗扫 Banner Sweep: a 120 degree cone r2.6 toward where the hero stood, 700 ms (Attack02), x1.3 breach fire (his
      // specials burn, monsters.md §2.6: the bodies under it too), every 5 s.
      {
        id: 'banner_sweep', kind: 'cone', clip: 'Attack02', windupMs: 700, cooldownMs: 5000, initialCooldownMs: 2000,
        aim: 'heroAtWindup', shape: { radius: 2.6, arcDeg: 120 }, dmgMul: 1.3, damageType: 'fire', priority: 1,
      },
      // 誓印 Oath-Brand Slam: a circle r1.8 at the hero's position when the windup starts, 900 ms (Cast01, FX_Brand), x1.8
      // breach fire; the brand burns 4 s (x0.15 every 500 ms). Every 8 s.
      {
        id: 'oath_brand', kind: 'circle', clip: 'Cast01', windupMs: 900, cooldownMs: 8000, initialCooldownMs: 4000,
        aim: 'heroAtWindup', shape: { radius: 1.8 }, dmgMul: 1.8, damageType: 'fire', priority: 2,
        hazard: { durationMs: 4000, tickMs: 500, tickDmgMul: 0.15, shape: { kind: 'circle', radius: 1.8 } },
      },
      // 渗裂 Breach-seep (P2): a crack line 8 x 1 from the dais toward the hero after a 700 ms warning, two breach-fire
      // pulses of x0.5 and slow 30 % for 2 s; every 6 s. One line, not three: an ability aims once (2F outcomes). 2F review:
      // the dais pulses, not Valdras - no caster clip, and a ranged special (rangeMax past his 2.4-tile swing reach): it
      // reaches a hero up to 10 tiles away while the P2 tether holds him on the dais.
      {
        id: 'breach_seep', kind: 'hazard', windupMs: 700, cooldownMs: 6000, initialCooldownMs: 1500, rangeMax: 10,
        aim: 'anchor', shape: { kind: 'line', length: 8, width: 1 }, damageType: 'fire', phases: [1], priority: 3,
        hazard: {
          durationMs: 1000, tickMs: 500, tickDmgMul: 0.5, status: { type: 'slow', ms: 2000, value: 30 },
        },
      },
      // 冲锋 Lunge (P3, switched on by the row): a charge - a line 5 x 1.2 toward a hero 2.5-5 tiles away, 800 ms (Lunge),
      // x1.5 breach fire, every 7 s; he rushes down the line after the hit (2F review: a ranged special with a dash).
      {
        id: 'lunge', kind: 'line', clip: 'Lunge', windupMs: 800, cooldownMs: 7000, initialCooldownMs: 1000, rangeMin: 2.5,
        rangeMax: 5, aim: 'heroAtWindup', shape: { length: 5, width: 1.2 }, dmgMul: 1.5, damageType: 'fire', priority: 2,
        dash: true,
      },
    ],
    phases: [
      {
        hpBelowPct: 65, enterAction: 'roar', speedMul: 0.6, leash: { radius: 3 },
        summons: [
          {
            monsterId: 'rollcall_standard_bearer', count: 1, at: 'anchors', anchors: [{ col: 11, row: 91 }], respawnMs: 20000,
            respawns: 1,
          },
        ],
        feed: { radius: 4, healPct: 3, delayMs: 1000 },
        stinger: 'stinger_valdras_roll_call',
      },
      {
        hpBelowPct: 30, enterAction: 'roar', attackSpeedMul: 1.25, speedMul: 1.15, abilitiesOn: ['lunge'],
        arena: { kind: 'ringShrink', fromR: 9, toR: 5, overMs: 40000, dpsPctMaxHp: 4, damageType: 'fire' },
        stinger: 'stinger_valdras_no_retreat',
      },
    ],
  },
  // rollcall_standard_bearer: Valdras's P2 standard-bearer (chapter1.md §11 Roll Call), never a map spawn. It rises at the
  // west ash circle (11,91) as his Raised (it collapses with him; one more rises 20 s after it falls, once); while it
  // lives both ash circles (11,91) and (19,99) raise a footman every 10 s (cap 4 alive, 40 % life; they collapse with it).
  // The standard-bearer's look, numbers, banner aura and family; its muster is the circles', not the corpses'.
  {
    id: 'rollcall_standard_bearer',
    name: '亡旗手',
    level: 7,
    hp: 140,
    damage: 8,
    defense: 8,
    speed: 30,
    aggroRange: 6,
    attackRange: 1.6,
    attackSpeed: 1700,
    expReward: 30,
    goldReward: [4, 8],
    spriteKey: 'monster_dead_standard_bearer',
    animCategory: 'humanoid',
    family: 'unburied',
    hitCapHeroMaxHpPct: HIT_CAP,
    abilities: [
      {
        id: 'roll_call', kind: 'summon', clip: 'Cast01', windupMs: 900, cooldownMs: 10000, initialCooldownMs: 3000, rangeMax: 30,
        aim: 'self',
        summon: {
          monsterId: 'risen_footman', count: 2, at: 'anchors', anchors: [{ col: 11, row: 91 }, { col: 19, row: 99 }], capAlive: 4,
          hpPct: 40,
        },
      },
    ],
    aura: { stat: 'damage', mul: 1.15, radius: 5, family: 'unburied' },
    corpse: CORPSE,
  },
];

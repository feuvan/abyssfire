/**
 * Abyssfire (渊火) — main story script.
 *
 * Story bible: docs/story.md. Every text field is an i18n key; the words live in
 * src/i18n/locales/story.ts (STORY_ZH / STORY_EN). Keys follow `story.<id>.<n>`,
 * where <n> is the 1-based slide/step index.
 */
import type { BossIntro, Chapter, Cutscene, StorySequence, StoryTrigger } from './types';

// ─── Prologue: myth → cataclysm → sealing → the fire stirs → the hero wakes ───
export const PROLOGUE: StorySequence = {
  id: 'prologue',
  slides: [
    { heading: 'story.prologue.1.heading', title: 'story.prologue.1.title', text: 'story.prologue.1.text', mood: 'embers' },
    { heading: 'story.prologue.2.heading', title: 'story.prologue.2.title', text: 'story.prologue.2.text', mood: 'night' },
    { heading: 'story.prologue.3.heading', title: 'story.prologue.3.title', text: 'story.prologue.3.text', mood: 'abyss' },
    { heading: 'story.prologue.4.heading', title: 'story.prologue.4.title', text: 'story.prologue.4.text', mood: 'abyss' },
    { heading: 'story.prologue.5.heading', title: 'story.prologue.5.title', text: 'story.prologue.5.text', mood: 'embers' },
    { heading: 'story.prologue.6.heading', title: 'story.prologue.6.title', text: 'story.prologue.6.text', mood: 'dawn' },
  ],
};

// ─── Chapter cards (first entry into each zone) ───
export const CHAPTERS: Chapter[] = [
  { zoneId: 'emerald_plains', number: 'story.chapter.emerald_plains.number', title: 'story.chapter.emerald_plains.title', subtitle: 'story.chapter.emerald_plains.subtitle', text: 'story.chapter.emerald_plains.text', mood: 'dawn' },
  { zoneId: 'twilight_forest', number: 'story.chapter.twilight_forest.number', title: 'story.chapter.twilight_forest.title', subtitle: 'story.chapter.twilight_forest.subtitle', text: 'story.chapter.twilight_forest.text', mood: 'night' },
  { zoneId: 'anvil_mountains', number: 'story.chapter.anvil_mountains.number', title: 'story.chapter.anvil_mountains.title', subtitle: 'story.chapter.anvil_mountains.subtitle', text: 'story.chapter.anvil_mountains.text', mood: 'forge' },
  { zoneId: 'scorching_desert', number: 'story.chapter.scorching_desert.number', title: 'story.chapter.scorching_desert.title', subtitle: 'story.chapter.scorching_desert.subtitle', text: 'story.chapter.scorching_desert.text', mood: 'sand' },
  { zoneId: 'abyss_rift', number: 'story.chapter.abyss_rift.number', title: 'story.chapter.abyss_rift.title', subtitle: 'story.chapter.abyss_rift.subtitle', text: 'story.chapter.abyss_rift.text', mood: 'abyss' },
];

// ─── In-world cutscenes ───
export const CUTSCENES: Record<string, Cutscene> = {
  // Ch.1 — the elder reads the brand and speaks the prophecy (after q_kill_slimes).
  cs_ep_mark: {
    id: 'cs_ep_mark',
    steps: [
      { kind: 'focus', target: { npc: 'quest_elder' } },
      { kind: 'say', speaker: { npc: 'quest_elder' }, text: 'story.cs_ep_mark.2' },
      { kind: 'focus', target: 'player' },
      { kind: 'narrate', text: 'story.cs_ep_mark.4' },
      { kind: 'say', speaker: { npc: 'quest_elder' }, text: 'story.cs_ep_mark.5' },
      { kind: 'say', speaker: { npc: 'quest_elder' }, text: 'story.cs_ep_mark.6' },
      { kind: 'say', speaker: { npc: 'quest_elder' }, text: 'story.cs_ep_mark.7' },
      { kind: 'say', speaker: 'hero', text: 'story.cs_ep_mark.8' },
      { kind: 'say', speaker: { npc: 'quest_elder' }, text: 'story.cs_ep_mark.9' },
      { kind: 'say', speaker: { npc: 'quest_elder' }, text: 'story.cs_ep_mark.10' },
    ],
  },
  // Ch.1 — the first whisper, in a dream of the goblin camp (after q_explore_goblin_camp).
  cs_ep_whisper: {
    id: 'cs_ep_whisper',
    steps: [
      { kind: 'focus', target: { col: 40, row: 52 }, ms: 1400 },
      { kind: 'narrate', text: 'story.cs_ep_whisper.2' },
      { kind: 'flash', color: 0x7a3cff, ms: 400 },
      { kind: 'whisper', text: 'story.cs_ep_whisper.4' },
      { kind: 'whisper', text: 'story.cs_ep_whisper.5' },
      { kind: 'whisper', text: 'story.cs_ep_whisper.6' },
      { kind: 'focus', target: 'player' },
      { kind: 'say', speaker: 'hero', text: 'story.cs_ep_whisper.8' },
      { kind: 'whisper', text: 'story.cs_ep_whisper.9' },
      { kind: 'shake', intensity: 0.006, ms: 400 },
      { kind: 'narrate', text: 'story.cs_ep_whisper.11' },
    ],
  },
  // Ch.1 — the Seal of Veins steadies; the first flame lights (after q_secure_plains).
  cs_ep_finale: {
    id: 'cs_ep_finale',
    steps: [
      { kind: 'focus', target: 'player' },
      { kind: 'narrate', text: 'story.cs_ep_finale.2' },
      { kind: 'flash', color: 0x9dffb0, ms: 500 },
      { kind: 'narrate', text: 'story.cs_ep_finale.4' },
      { kind: 'focus', target: { npc: 'quest_elder' } },
      { kind: 'say', speaker: { npc: 'quest_elder' }, text: 'story.cs_ep_finale.6' },
      { kind: 'say', speaker: { npc: 'quest_elder' }, text: 'story.cs_ep_finale.7' },
      { kind: 'say', speaker: { npc: 'quest_elder' }, text: 'story.cs_ep_finale.8' },
      { kind: 'whisper', text: 'story.cs_ep_finale.9' },
      { kind: 'narrate', text: 'story.cs_ep_finale.10' },
      { kind: 'narrate', text: 'story.cs_ep_finale.11' },
    ],
  },
  // Ch.2 — the hermit reveals the sixth sage, Ignaroth (after q_talk_hermit).
  cs_tf_hermit: {
    id: 'cs_tf_hermit',
    steps: [
      { kind: 'narrate', text: 'story.cs_tf_hermit.1' },
      { kind: 'focus', target: { npc: 'forest_hermit' }, ms: 1400 },
      { kind: 'say', speaker: { npc: 'forest_hermit' }, text: 'story.cs_tf_hermit.3' },
      { kind: 'say', speaker: { npc: 'forest_hermit' }, text: 'story.cs_tf_hermit.4' },
      { kind: 'say', speaker: { npc: 'forest_hermit' }, text: 'story.cs_tf_hermit.5' },
      { kind: 'say', speaker: { npc: 'forest_hermit' }, text: 'story.cs_tf_hermit.6' },
      { kind: 'say', speaker: { npc: 'forest_hermit' }, text: 'story.cs_tf_hermit.7' },
      { kind: 'say', speaker: { npc: 'forest_hermit' }, text: 'story.cs_tf_hermit.8' },
      { kind: 'focus', target: 'player' },
      { kind: 'whisper', text: 'story.cs_tf_hermit.10' },
      { kind: 'focus', target: { npc: 'quest_scout' } },
      { kind: 'say', speaker: { npc: 'quest_scout' }, text: 'story.cs_tf_hermit.12' },
    ],
  },
  // Ch.2 — the Moon Seal rekindles; moonlight returns (after q_seal_dark_source).
  cs_tf_finale: {
    id: 'cs_tf_finale',
    steps: [
      { kind: 'focus', target: 'player' },
      { kind: 'narrate', text: 'story.cs_tf_finale.2' },
      { kind: 'flash', color: 0xdfe8ff, ms: 600 },
      { kind: 'narrate', text: 'story.cs_tf_finale.4' },
      { kind: 'narrate', text: 'story.cs_tf_finale.5' },
      { kind: 'focus', target: { npc: 'forest_spirit_medium' } },
      { kind: 'say', speaker: { npc: 'forest_spirit_medium' }, text: 'story.cs_tf_finale.7' },
      { kind: 'focus', target: { npc: 'quest_scout' } },
      { kind: 'say', speaker: { npc: 'quest_scout' }, text: 'story.cs_tf_finale.9' },
      { kind: 'whisper', text: 'story.cs_tf_finale.10' },
      { kind: 'narrate', text: 'story.cs_tf_finale.11' },
      { kind: 'narrate', text: 'story.cs_tf_finale.12' },
    ],
  },
  // Ch.3 — the Hammer of Fate is reforged (after q_reforge_artifact).
  cs_am_hammer: {
    id: 'cs_am_hammer',
    steps: [
      { kind: 'focus', target: { npc: 'quest_dwarf' } },
      { kind: 'say', speaker: { npc: 'quest_dwarf' }, text: 'story.cs_am_hammer.2' },
      { kind: 'narrate', text: 'story.cs_am_hammer.3' },
      { kind: 'flash', color: 0xffa040, ms: 500 },
      { kind: 'shake', intensity: 0.004, ms: 300 },
      { kind: 'say', speaker: { npc: 'quest_dwarf' }, text: 'story.cs_am_hammer.6' },
      { kind: 'title', title: 'story.cs_am_hammer.7.title', subtitle: 'story.cs_am_hammer.7.subtitle' },
      { kind: 'narrate', text: 'story.cs_am_hammer.8' },
      { kind: 'say', speaker: { npc: 'quest_dwarf' }, text: 'story.cs_am_hammer.9' },
      { kind: 'whisper', text: 'story.cs_am_hammer.10' },
      { kind: 'say', speaker: { npc: 'quest_dwarf' }, text: 'story.cs_am_hammer.11' },
    ],
  },
  // Ch.3 — the Anvil Seal rings again (after q_kill_stone_guardian).
  cs_am_finale: {
    id: 'cs_am_finale',
    steps: [
      { kind: 'focus', target: 'player' },
      { kind: 'narrate', text: 'story.cs_am_finale.2' },
      { kind: 'narrate', text: 'story.cs_am_finale.3' },
      { kind: 'shake', intensity: 0.008, ms: 500 },
      { kind: 'flash', color: 0xff8a3a, ms: 500 },
      { kind: 'narrate', text: 'story.cs_am_finale.6' },
      { kind: 'focus', target: { npc: 'quest_dwarf' } },
      { kind: 'say', speaker: { npc: 'quest_dwarf' }, text: 'story.cs_am_finale.8' },
      { kind: 'say', speaker: { npc: 'quest_dwarf' }, text: 'story.cs_am_finale.9' },
      { kind: 'whisper', text: 'story.cs_am_finale.10' },
      { kind: 'whisper', text: 'story.cs_am_finale.11' },
      { kind: 'narrate', text: 'story.cs_am_finale.12' },
      { kind: 'narrate', text: 'story.cs_am_finale.13' },
    ],
  },
  // Ch.4 — the oasis vision: the Sun Crown Kingdom and the high priest's betrayal (after q_explore_oasis).
  cs_sd_oasis: {
    id: 'cs_sd_oasis',
    steps: [
      { kind: 'focus', target: { npc: 'quest_nomad' } },
      { kind: 'narrate', text: 'story.cs_sd_oasis.2' },
      { kind: 'focus', target: { col: 45, row: 22 }, ms: 1400 },
      { kind: 'title', title: 'story.cs_sd_oasis.4.title', subtitle: 'story.cs_sd_oasis.4.subtitle' },
      { kind: 'narrate', text: 'story.cs_sd_oasis.5' },
      { kind: 'narrate', text: 'story.cs_sd_oasis.6' },
      { kind: 'narrate', text: 'story.cs_sd_oasis.7' },
      { kind: 'whisper', text: 'story.cs_sd_oasis.8' },
      { kind: 'shake', intensity: 0.01, ms: 700 },
      { kind: 'narrate', text: 'story.cs_sd_oasis.10' },
      { kind: 'narrate', text: 'story.cs_sd_oasis.11' },
      { kind: 'focus', target: { npc: 'quest_nomad' } },
      { kind: 'say', speaker: { npc: 'quest_nomad' }, text: 'story.cs_sd_oasis.13' },
      { kind: 'say', speaker: { npc: 'quest_nomad' }, text: 'story.cs_sd_oasis.14' },
    ],
  },
  // Ch.4 — Helia is reborn; the Sun Seal closes the rift (after q_seal_fire_rift).
  cs_sd_finale: {
    id: 'cs_sd_finale',
    steps: [
      { kind: 'focus', target: 'player' },
      { kind: 'narrate', text: 'story.cs_sd_finale.2' },
      { kind: 'flash', color: 0xffd36a, ms: 600 },
      { kind: 'narrate', text: 'story.cs_sd_finale.4' },
      { kind: 'narrate', text: 'story.cs_sd_finale.5' },
      { kind: 'focus', target: { npc: 'quest_nomad' } },
      { kind: 'say', speaker: { npc: 'quest_nomad' }, text: 'story.cs_sd_finale.7' },
      { kind: 'say', speaker: { npc: 'quest_nomad' }, text: 'story.cs_sd_finale.8' },
      { kind: 'whisper', text: 'story.cs_sd_finale.9' },
      { kind: 'shake', intensity: 0.006, ms: 500 },
      { kind: 'narrate', text: 'story.cs_sd_finale.11' },
      { kind: 'narrate', text: 'story.cs_sd_finale.12' },
    ],
  },
  // Ch.5 — at the gate: Ignaroth names himself; the brand is the key (after q_explore_abyss).
  cs_ar_gate: {
    id: 'cs_ar_gate',
    steps: [
      { kind: 'focus', target: { npc: 'quest_warden' } },
      { kind: 'say', speaker: { npc: 'quest_warden' }, text: 'story.cs_ar_gate.2' },
      { kind: 'focus', target: { col: 38, row: 70 }, ms: 1600 },
      { kind: 'title', title: 'story.cs_ar_gate.4.title', subtitle: 'story.cs_ar_gate.4.subtitle' },
      { kind: 'narrate', text: 'story.cs_ar_gate.5' },
      { kind: 'whisper', text: 'story.cs_ar_gate.6' },
      { kind: 'whisper', text: 'story.cs_ar_gate.7' },
      { kind: 'shake', intensity: 0.01, ms: 600 },
      { kind: 'focus', target: 'player' },
      { kind: 'narrate', text: 'story.cs_ar_gate.10' },
      { kind: 'whisper', text: 'story.cs_ar_gate.11' },
      { kind: 'focus', target: { npc: 'quest_warden' } },
      { kind: 'say', speaker: { npc: 'quest_warden' }, text: 'story.cs_ar_gate.13' },
    ],
  },
  // Ch.5 — the Final Key is forged; the price of the Heartflame (after q_forge_seal).
  cs_ar_seal: {
    id: 'cs_ar_seal',
    steps: [
      { kind: 'focus', target: { npc: 'quest_warden' } },
      { kind: 'say', speaker: { npc: 'quest_warden' }, text: 'story.cs_ar_seal.2' },
      { kind: 'narrate', text: 'story.cs_ar_seal.3' },
      { kind: 'flash', color: 0xc9b8ff, ms: 600 },
      { kind: 'title', title: 'story.cs_ar_seal.5.title', subtitle: 'story.cs_ar_seal.5.subtitle' },
      { kind: 'narrate', text: 'story.cs_ar_seal.6' },
      { kind: 'say', speaker: { npc: 'quest_warden' }, text: 'story.cs_ar_seal.7' },
      { kind: 'say', speaker: { npc: 'quest_warden' }, text: 'story.cs_ar_seal.8' },
      { kind: 'focus', target: 'player' },
      { kind: 'narrate', text: 'story.cs_ar_seal.10' },
      { kind: 'say', speaker: 'hero', text: 'story.cs_ar_seal.11' },
      { kind: 'whisper', text: 'story.cs_ar_seal.12' },
      { kind: 'say', speaker: { npc: 'quest_warden' }, text: 'story.cs_ar_seal.13' },
    ],
  },
  // Finale — Ignaroth falls; the Heartflame is returned; the brand becomes a silver ring (demon_lord killed).
  cs_ar_fall: {
    id: 'cs_ar_fall',
    steps: [
      { kind: 'shake', intensity: 0.014, ms: 900 },
      { kind: 'flash', color: 0xffffff, ms: 500 },
      { kind: 'focus', target: { monster: 'demon_lord' }, ms: 700 },
      { kind: 'narrate', text: 'story.cs_ar_fall.4' },
      { kind: 'say', speaker: { monster: 'demon_lord' }, text: 'story.cs_ar_fall.5' },
      { kind: 'focus', target: 'player' },
      { kind: 'narrate', text: 'story.cs_ar_fall.7' },
      { kind: 'say', speaker: { monster: 'demon_lord' }, text: 'story.cs_ar_fall.8' },
      { kind: 'narrate', text: 'story.cs_ar_fall.9' },
      { kind: 'flash', color: 0xffd88a, ms: 800 },
      { kind: 'narrate', text: 'story.cs_ar_fall.11' },
      { kind: 'say', speaker: { monster: 'demon_lord' }, text: 'story.cs_ar_fall.12' },
      { kind: 'narrate', text: 'story.cs_ar_fall.13' },
      { kind: 'narrate', text: 'story.cs_ar_fall.14' },
    ],
  },
  // Ch.2 — Volgan's cub finds the hero by its father's body (first werewolf_alpha kill; grants pet_storm_wolf).
  cs_tf_moonfang: {
    id: 'cs_tf_moonfang',
    steps: [
      { kind: 'focus', target: 'player' },
      { kind: 'narrate', text: 'story.cs_tf_moonfang.2' },
      { kind: 'narrate', text: 'story.cs_tf_moonfang.3' },
      { kind: 'title', title: 'story.cs_tf_moonfang.4.title', subtitle: 'story.cs_tf_moonfang.4.subtitle' },
      { kind: 'narrate', text: 'story.cs_tf_moonfang.5' },
      { kind: 'say', speaker: 'hero', text: 'story.cs_tf_moonfang.6' },
      { kind: 'narrate', text: 'story.cs_tf_moonfang.7' },
    ],
  },
  // Ch.4 — Helia leaves one ember behind (after the Sun Seal finale; grants pet_phoenix).
  cs_sd_helia: {
    id: 'cs_sd_helia',
    steps: [
      { kind: 'focus', target: 'player' },
      { kind: 'narrate', text: 'story.cs_sd_helia.2' },
      { kind: 'flash', color: 0xffb347, ms: 500 },
      { kind: 'narrate', text: 'story.cs_sd_helia.4' },
      { kind: 'title', title: 'story.cs_sd_helia.5.title', subtitle: 'story.cs_sd_helia.5.subtitle' },
      { kind: 'focus', target: { npc: 'quest_nomad' } },
      { kind: 'say', speaker: { npc: 'quest_nomad' }, text: 'story.cs_sd_helia.7' },
      { kind: 'narrate', text: 'story.cs_sd_helia.8' },
    ],
  },
  // Ch.5 — the warden sends her lamp to light the tower's altar (after q_collect_demon_essence, 以渊为引).
  cs_ar_altar: {
    id: 'cs_ar_altar',
    steps: [
      { kind: 'focus', target: { npc: 'quest_warden' } },
      { kind: 'say', speaker: { npc: 'quest_warden' }, text: 'story.cs_ar_altar.2' },
      { kind: 'say', speaker: { npc: 'quest_warden' }, text: 'story.cs_ar_altar.3' },
      { kind: 'narrate', text: 'story.cs_ar_altar.4' },
      { kind: 'say', speaker: { npc: 'quest_warden' }, text: 'story.cs_ar_altar.5' },
      { kind: 'focus', target: 'player' },
    ],
  },
  // The Ember Tower — first visit home: the elder waits under the tower.
  cs_tower_home: {
    id: 'cs_tower_home',
    steps: [
      { kind: 'focus', target: { col: 24, row: 18 }, ms: 1400 },
      { kind: 'title', title: 'story.cs_tower_home.2.title', subtitle: 'story.cs_tower_home.2.subtitle' },
      { kind: 'narrate', text: 'story.cs_tower_home.3' },
      { kind: 'focus', target: { npc: 'tower_elder' } },
      { kind: 'say', speaker: { npc: 'tower_elder' }, text: 'story.cs_tower_home.5' },
      { kind: 'say', speaker: { npc: 'tower_elder' }, text: 'story.cs_tower_home.6' },
      { kind: 'say', speaker: { npc: 'tower_elder' }, text: 'story.cs_tower_home.7' },
      { kind: 'narrate', text: 'story.cs_tower_home.8' },
      { kind: 'focus', target: 'player' },
    ],
  },
  // Boss intro — goblin_chief.
  cs_boss_goblin_chief: {
    id: 'cs_boss_goblin_chief',
    steps: [
      { kind: 'focus', target: { monster: 'goblin_chief' }, ms: 800 },
      { kind: 'title', title: 'story.boss.goblin_chief.name', subtitle: 'story.boss.goblin_chief.epithet' },
      { kind: 'say', speaker: { monster: 'goblin_chief' }, text: 'story.cs_boss_goblin_chief.3' },
      { kind: 'shake', intensity: 0.006, ms: 400 },
      { kind: 'whisper', text: 'story.cs_boss_goblin_chief.5' },
      { kind: 'focus', target: 'player', ms: 600 },
    ],
  },
  // Boss intro — werewolf_alpha.
  cs_boss_werewolf_alpha: {
    id: 'cs_boss_werewolf_alpha',
    steps: [
      { kind: 'focus', target: { monster: 'werewolf_alpha' }, ms: 800 },
      { kind: 'narrate', text: 'story.cs_boss_werewolf_alpha.2' },
      { kind: 'title', title: 'story.boss.werewolf_alpha.name', subtitle: 'story.boss.werewolf_alpha.epithet' },
      { kind: 'say', speaker: { monster: 'werewolf_alpha' }, text: 'story.cs_boss_werewolf_alpha.4' },
      { kind: 'whisper', text: 'story.cs_boss_werewolf_alpha.5' },
      { kind: 'shake', intensity: 0.006, ms: 400 },
      { kind: 'focus', target: 'player', ms: 600 },
    ],
  },
  // Boss intro — mountain_troll.
  cs_boss_mountain_troll: {
    id: 'cs_boss_mountain_troll',
    steps: [
      { kind: 'focus', target: { monster: 'mountain_troll' }, ms: 800 },
      { kind: 'title', title: 'story.boss.mountain_troll.name', subtitle: 'story.boss.mountain_troll.epithet' },
      { kind: 'say', speaker: { monster: 'mountain_troll' }, text: 'story.cs_boss_mountain_troll.3' },
      { kind: 'shake', intensity: 0.01, ms: 500 },
      { kind: 'whisper', text: 'story.cs_boss_mountain_troll.5' },
      { kind: 'focus', target: 'player', ms: 600 },
    ],
  },
  // Boss intro — phoenix.
  cs_boss_phoenix: {
    id: 'cs_boss_phoenix',
    steps: [
      { kind: 'focus', target: { monster: 'phoenix' }, ms: 900 },
      { kind: 'narrate', text: 'story.cs_boss_phoenix.2' },
      { kind: 'title', title: 'story.boss.phoenix.name', subtitle: 'story.boss.phoenix.epithet' },
      { kind: 'whisper', text: 'story.cs_boss_phoenix.4' },
      { kind: 'narrate', text: 'story.cs_boss_phoenix.5' },
      { kind: 'flash', color: 0xff9a3a, ms: 400 },
      { kind: 'shake', intensity: 0.008, ms: 500 },
      { kind: 'focus', target: 'player', ms: 600 },
    ],
  },
  // Boss intro — demon_lord.
  cs_boss_demon_lord: {
    id: 'cs_boss_demon_lord',
    steps: [
      { kind: 'focus', target: { monster: 'demon_lord' }, ms: 1600 },
      { kind: 'narrate', text: 'story.cs_boss_demon_lord.2' },
      { kind: 'title', title: 'story.boss.demon_lord.name', subtitle: 'story.boss.demon_lord.epithet' },
      { kind: 'say', speaker: { monster: 'demon_lord' }, text: 'story.cs_boss_demon_lord.4' },
      { kind: 'say', speaker: { monster: 'demon_lord' }, text: 'story.cs_boss_demon_lord.5' },
      { kind: 'say', speaker: 'hero', text: 'story.cs_boss_demon_lord.6' },
      { kind: 'say', speaker: { monster: 'demon_lord' }, text: 'story.cs_boss_demon_lord.7' },
      { kind: 'shake', intensity: 0.012, ms: 700 },
      { kind: 'flash', color: 0x7a3cff, ms: 500 },
      { kind: 'focus', target: 'player', ms: 600 },
    ],
  },
  cs_boss_kassanor: {
    id: 'cs_boss_kassanor',
    steps: [
      { kind: 'focus', target: { monster: 'dungeon_abyss_lord' }, ms: 1500 },
      { kind: 'narrate', text: 'story.cs_boss_kassanor.1' },
      { kind: 'title', title: 'story.boss.dungeon_abyss_lord.name', subtitle: 'story.boss.dungeon_abyss_lord.epithet' },
      { kind: 'say', speaker: { monster: 'dungeon_abyss_lord' }, text: 'story.cs_boss_kassanor.2' },
      { kind: 'say', speaker: { monster: 'dungeon_abyss_lord' }, text: 'story.cs_boss_kassanor.3' },
      { kind: 'say', speaker: 'hero', text: 'story.cs_boss_kassanor.4' },
      { kind: 'say', speaker: { monster: 'dungeon_abyss_lord' }, text: 'story.cs_boss_kassanor.5' },
      { kind: 'shake', intensity: 0.01, ms: 600 },
      { kind: 'flash', color: 0x5a2cff, ms: 450 },
      { kind: 'focus', target: 'player', ms: 600 },
    ],
  },
};

// ─── Boss introductions (first sight of each zone boss) ───
export const BOSS_INTROS: BossIntro[] = [
  { monsterId: 'goblin_chief', name: 'story.boss.goblin_chief.name', epithet: 'story.boss.goblin_chief.epithet', cutscene: 'cs_boss_goblin_chief' },
  { monsterId: 'werewolf_alpha', name: 'story.boss.werewolf_alpha.name', epithet: 'story.boss.werewolf_alpha.epithet', cutscene: 'cs_boss_werewolf_alpha' },
  { monsterId: 'mountain_troll', name: 'story.boss.mountain_troll.name', epithet: 'story.boss.mountain_troll.epithet', cutscene: 'cs_boss_mountain_troll' },
  { monsterId: 'phoenix', name: 'story.boss.phoenix.name', epithet: 'story.boss.phoenix.epithet', cutscene: 'cs_boss_phoenix' },
  { monsterId: 'demon_lord', name: 'story.boss.demon_lord.name', epithet: 'story.boss.demon_lord.epithet', cutscene: 'cs_boss_demon_lord' },
  // Post-game: the Abyss Labyrinth's last floor.
  { monsterId: 'dungeon_abyss_lord', name: 'story.boss.dungeon_abyss_lord.name', epithet: 'story.boss.dungeon_abyss_lord.epithet', cutscene: 'cs_boss_kassanor' },
];

// ─── When each beat plays ───
export const STORY_TRIGGERS: StoryTrigger[] = [
  { on: 'quest_turned_in', questId: 'q_kill_slimes', cutscene: 'cs_ep_mark' },
  { on: 'quest_turned_in', questId: 'q_explore_goblin_camp', cutscene: 'cs_ep_whisper' },
  { on: 'quest_turned_in', questId: 'q_secure_plains', cutscene: 'cs_ep_finale' },
  { on: 'quest_turned_in', questId: 'q_talk_hermit', cutscene: 'cs_tf_hermit' },
  { on: 'quest_turned_in', questId: 'q_seal_dark_source', cutscene: 'cs_tf_finale' },
  { on: 'quest_turned_in', questId: 'q_reforge_artifact', cutscene: 'cs_am_hammer' },
  { on: 'quest_turned_in', questId: 'q_kill_stone_guardian', cutscene: 'cs_am_finale' },
  { on: 'quest_turned_in', questId: 'q_explore_oasis', cutscene: 'cs_sd_oasis' },
  { on: 'quest_turned_in', questId: 'q_seal_fire_rift', cutscene: 'cs_sd_finale' },
  { on: 'quest_turned_in', questId: 'q_seal_fire_rift', cutscene: 'cs_sd_helia', grantPet: 'pet_phoenix' },
  { on: 'quest_turned_in', questId: 'q_explore_abyss', cutscene: 'cs_ar_gate' },
  { on: 'quest_turned_in', questId: 'q_collect_demon_essence', cutscene: 'cs_ar_altar' },
  { on: 'quest_turned_in', questId: 'q_forge_seal', cutscene: 'cs_ar_seal' },
  { on: 'monster_killed', monsterId: 'demon_lord', cutscene: 'cs_ar_fall' },
  { on: 'monster_killed', monsterId: 'werewolf_alpha', cutscene: 'cs_tf_moonfang', grantPet: 'pet_storm_wolf' },
  { on: 'zone_entered', zoneId: 'ember_tower', cutscene: 'cs_tower_home' },
];

// ─── Epilogue: each land heals; the fate of the brand ───
export const EPILOGUE: StorySequence = {
  id: 'epilogue',
  slides: [
    { heading: 'story.epilogue.1.heading', title: 'story.epilogue.1.title', text: 'story.epilogue.1.text', mood: 'light' },
    { title: 'story.epilogue.2.title', text: 'story.epilogue.2.text', mood: 'dawn' },
    { title: 'story.epilogue.3.title', text: 'story.epilogue.3.text', mood: 'dawn' },
    { title: 'story.epilogue.4.title', text: 'story.epilogue.4.text', mood: 'light' },
    { title: 'story.epilogue.5.title', text: 'story.epilogue.5.text', mood: 'dawn' },
    { title: 'story.epilogue.6.title', text: 'story.epilogue.6.text', mood: 'light' },
  ],
};

// ─── Credits ───
export const CREDITS: StorySequence = {
  id: 'credits',
  credits: true,
  slides: [
    { title: 'story.credits.1.title', text: 'story.credits.1.text', mood: 'embers' },
    { heading: 'story.credits.2.heading', title: 'story.credits.2.title' },
    { heading: 'story.credits.3.heading', text: 'story.credits.3.text' },
    { heading: 'story.credits.4.heading', text: 'story.credits.4.text' },
    { title: 'story.credits.5.title', mood: 'dawn' },
    { text: 'story.credits.6.text', mood: 'abyss' },
  ],
};

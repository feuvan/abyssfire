/**
 * Abyssfire (渊火) — main story script.
 *
 * Story bible: docs/story.md. Every text field is an i18n key; the words live in
 * src/i18n/locales/story.ts (STORY_ZH / STORY_EN). Keys follow `story.<id>.<n>`,
 * where <n> is the 1-based slide/step index.
 *
 * Dark remake (unreal/Docs/dark-remake/, package 1B part 2): the prologue, the Chapter 1 card, its cutscenes
 * (`cs_rf_*`, `cs_boss_valdras`) and triggers follow chapter1.md §8 and story-bible.md; their words live in
 * unreal/GameData/i18n/ch1Story.ts (the remake strings win over the web's). Chapters 2-5 stay as copied (DECISIONS DR11).
 */
import type { BossIntro, Chapter, Cutscene, StorySequence, StoryTrigger } from './types';

// ─── Prologue (chapter1.md §8): the forge → the burned oath → the whisper → the Ashen Crusade → the Breach → the hero
// wakes in the burn pit. Six slides, title + text (no heading line) ───
export const PROLOGUE: StorySequence = {
  id: 'prologue',
  slides: [
    { title: 'story.prologue.1.title', text: 'story.prologue.1.text', mood: 'embers' },
    { title: 'story.prologue.2.title', text: 'story.prologue.2.text', mood: 'night' },
    { title: 'story.prologue.3.title', text: 'story.prologue.3.text', mood: 'abyss' },
    { title: 'story.prologue.4.title', text: 'story.prologue.4.text', mood: 'embers' },
    { title: 'story.prologue.5.title', text: 'story.prologue.5.text', mood: 'abyss' },
    { title: 'story.prologue.6.title', text: 'story.prologue.6.text', mood: 'night' },
  ],
};

// ─── Chapter cards (first entry into each zone) ───
export const CHAPTERS: Chapter[] = [
  // 第一章 · 灰烬中醒来 · 赤幡原 / Chapter One · Woken from Ash · The Red Field (chapter1.md §8).
  { zoneId: 'emerald_plains', number: 'story.chapter.emerald_plains.number', title: 'story.chapter.emerald_plains.title', subtitle: 'story.chapter.emerald_plains.subtitle', text: 'story.chapter.emerald_plains.text', mood: 'night' },
  { zoneId: 'twilight_forest', number: 'story.chapter.twilight_forest.number', title: 'story.chapter.twilight_forest.title', subtitle: 'story.chapter.twilight_forest.subtitle', text: 'story.chapter.twilight_forest.text', mood: 'night' },
  { zoneId: 'anvil_mountains', number: 'story.chapter.anvil_mountains.number', title: 'story.chapter.anvil_mountains.title', subtitle: 'story.chapter.anvil_mountains.subtitle', text: 'story.chapter.anvil_mountains.text', mood: 'forge' },
  { zoneId: 'scorching_desert', number: 'story.chapter.scorching_desert.number', title: 'story.chapter.scorching_desert.title', subtitle: 'story.chapter.scorching_desert.subtitle', text: 'story.chapter.scorching_desert.text', mood: 'sand' },
  { zoneId: 'abyss_rift', number: 'story.chapter.abyss_rift.number', title: 'story.chapter.abyss_rift.title', subtitle: 'story.chapter.abyss_rift.subtitle', text: 'story.chapter.abyss_rift.text', mood: 'abyss' },
];

// ─── In-world cutscenes ───
export const CUTSCENES: Record<string, Cutscene> = {
  // ─── Chapter 1 (dark remake, chapter1.md §8). Every speaker and focus target stands in emerald_plains (the Captain,
  // Aldwin and Marl in their camps, Valdras at the Breach Rim); the Corpseherd is no map spawn, so cs_rf_herd narrates
  // her and looks at her tile. Aldwin calls the hero 孩子 (child), the whisper's word: the first seed of his fall. ───
  // The Captain rings the bell for the burial detail; Aldwin reads the brand and speaks the prophecy (after
  // q_rf_burial_detail).
  cs_rf_brand: {
    id: 'cs_rf_brand',
    steps: [
      { kind: 'focus', target: { npc: 'quest_elder' } },
      { kind: 'say', speaker: { npc: 'quest_elder' }, text: 'story.cs_rf_brand.2' },
      { kind: 'say', speaker: { npc: 'quest_elder' }, text: 'story.cs_rf_brand.3' },
      { kind: 'shake', intensity: 0.003, ms: 500 },
      { kind: 'narrate', text: 'story.cs_rf_brand.5' },
      { kind: 'focus', target: { npc: 'knell_chaplain' } },
      { kind: 'say', speaker: { npc: 'knell_chaplain' }, text: 'story.cs_rf_brand.7' },
      { kind: 'say', speaker: { npc: 'knell_chaplain' }, text: 'story.cs_rf_brand.8' },
      { kind: 'say', speaker: { npc: 'knell_chaplain' }, text: 'story.cs_rf_brand.9' },
      { kind: 'say', speaker: 'hero', text: 'story.cs_rf_brand.10' },
      { kind: 'say', speaker: { npc: 'quest_elder' }, text: 'story.cs_rf_brand.11' },
    ],
  },
  // The dream of the last battle and the first whisper (after q_rf_ashwheat): the third of the boss's four beats.
  cs_rf_whisper: {
    id: 'cs_rf_whisper',
    steps: [
      { kind: 'flash', color: 0xb02fd0, ms: 400 },
      { kind: 'narrate', text: 'story.cs_rf_whisper.2' },
      { kind: 'focus', target: { col: 15, row: 95 }, ms: 1400 },
      { kind: 'say', speaker: { monster: 'valdras' }, text: 'story.cs_rf_whisper.4' },
      { kind: 'narrate', text: 'story.cs_rf_whisper.5' },
      { kind: 'shake', intensity: 0.006, ms: 400 },
      { kind: 'whisper', text: 'story.cs_rf_whisper.7' },
      { kind: 'whisper', text: 'story.cs_rf_whisper.8' },
      { kind: 'say', speaker: 'hero', text: 'story.cs_rf_whisper.9' },
      { kind: 'whisper', text: 'story.cs_rf_whisper.10' },
      { kind: 'narrate', text: 'story.cs_rf_whisper.11' },
    ],
  },
  // The Corpseherd falls in her wagon ring and the dead sit down (her first kill, the Sheepfold (60,55)).
  cs_rf_herd: {
    id: 'cs_rf_herd',
    steps: [
      { kind: 'focus', target: { col: 60, row: 55 } },
      { kind: 'narrate', text: 'story.cs_rf_herd.2' },
      { kind: 'narrate', text: 'story.cs_rf_herd.3' },
      { kind: 'narrate', text: 'story.cs_rf_herd.4' },
      { kind: 'whisper', text: 'story.cs_rf_herd.5' },
    ],
  },
  // Valdras kneels on the dais and asks who gave the order to retreat (his first kill, the Breach Rim (15,95)).
  cs_rf_marshal_falls: {
    id: 'cs_rf_marshal_falls',
    steps: [
      { kind: 'focus', target: { col: 15, row: 95 } },
      { kind: 'say', speaker: { monster: 'valdras' }, text: 'story.cs_rf_marshal_falls.2' },
      { kind: 'narrate', text: 'story.cs_rf_marshal_falls.3' },
      { kind: 'whisper', text: 'story.cs_rf_marshal_falls.4' },
      { kind: 'wait', ms: 1200 },
    ],
  },
  // The bell rings by itself: the siege of the Boneyard Gate begins (on accepting q_rf_last_toll).
  cs_rf_siege: {
    id: 'cs_rf_siege',
    steps: [
      { kind: 'shake', intensity: 0.005, ms: 500 },
      { kind: 'narrate', text: 'story.cs_rf_siege.2' },
      { kind: 'say', speaker: { npc: 'quest_elder' }, text: 'story.cs_rf_siege.3' },
      { kind: 'focus', target: { col: 10, row: 32 }, ms: 1200 },
      { kind: 'whisper', text: 'story.cs_rf_siege.5' },
    ],
  },
  // Dawn at the gate: the branded palm on the bell, the first flame, every risen dead lies down; Marl leaves for the
  // tower; Aldwin's last line seeds Chapter 2 (after q_rf_last_toll, the chapter-complete quest; config.ts).
  cs_rf_finale: {
    id: 'cs_rf_finale',
    steps: [
      { kind: 'focus', target: { npc: 'quest_elder' } },
      { kind: 'say', speaker: { npc: 'quest_elder' }, text: 'story.cs_rf_finale.2' },
      { kind: 'narrate', text: 'story.cs_rf_finale.3' },
      { kind: 'flash', color: 0x9dffb0, ms: 500 },
      { kind: 'narrate', text: 'story.cs_rf_finale.5' },
      { kind: 'title', title: 'story.cs_rf_finale.6.title', subtitle: 'story.cs_rf_finale.6.subtitle' },
      { kind: 'narrate', text: 'story.cs_rf_finale.7' },
      { kind: 'whisper', text: 'story.cs_rf_finale.8' },
      { kind: 'whisper', text: 'story.cs_rf_finale.9' },
      { kind: 'say', speaker: { npc: 'quest_elder' }, text: 'story.cs_rf_finale.10' },
      { kind: 'focus', target: { npc: 'plains_herbalist' } },
      { kind: 'say', speaker: { npc: 'plains_herbalist' }, text: 'story.cs_rf_finale.12' },
      { kind: 'say', speaker: { npc: 'knell_chaplain' }, text: 'story.cs_rf_finale.13' },
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
  // Boss intro — valdras (dark remake, Docs/dark-remake/chapter1.md §8): the boss scan at 9 tiles. His Roar plays on the
  // focus step, under the title (K1 `anim`); Wave 2 adds the phase Roars.
  cs_boss_valdras: {
    id: 'cs_boss_valdras',
    steps: [
      { kind: 'focus', target: { monster: 'valdras' }, ms: 800, anim: 'roar' },
      { kind: 'title', title: 'story.boss.valdras.name', subtitle: 'story.boss.valdras.epithet' },
      { kind: 'say', speaker: { monster: 'valdras' }, text: 'story.cs_boss_valdras.3' },
      { kind: 'say', speaker: { monster: 'valdras' }, text: 'story.cs_boss_valdras.4' },
      { kind: 'whisper', text: 'story.cs_boss_valdras.5' },
      { kind: 'say', speaker: 'hero', text: 'story.cs_boss_valdras.6' },
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
  { monsterId: 'valdras', name: 'story.boss.valdras.name', epithet: 'story.boss.valdras.epithet', cutscene: 'cs_boss_valdras' },
  { monsterId: 'werewolf_alpha', name: 'story.boss.werewolf_alpha.name', epithet: 'story.boss.werewolf_alpha.epithet', cutscene: 'cs_boss_werewolf_alpha' },
  { monsterId: 'mountain_troll', name: 'story.boss.mountain_troll.name', epithet: 'story.boss.mountain_troll.epithet', cutscene: 'cs_boss_mountain_troll' },
  { monsterId: 'phoenix', name: 'story.boss.phoenix.name', epithet: 'story.boss.phoenix.epithet', cutscene: 'cs_boss_phoenix' },
  { monsterId: 'demon_lord', name: 'story.boss.demon_lord.name', epithet: 'story.boss.demon_lord.epithet', cutscene: 'cs_boss_demon_lord' },
  // Post-game: the Abyss Labyrinth's last floor.
  { monsterId: 'dungeon_abyss_lord', name: 'story.boss.dungeon_abyss_lord.name', epithet: 'story.boss.dungeon_abyss_lord.epithet', cutscene: 'cs_boss_kassanor' },
];

// ─── When each beat plays ───
export const STORY_TRIGGERS: StoryTrigger[] = [
  // Chapter 1 (chapter1.md §8 "Triggers"; cs_rf_pit waits for the Pit, Wave 3).
  { on: 'quest_turned_in', questId: 'q_rf_burial_detail', cutscene: 'cs_rf_brand' },
  { on: 'quest_turned_in', questId: 'q_rf_ashwheat', cutscene: 'cs_rf_whisper' },
  { on: 'quest_accepted', questId: 'q_rf_last_toll', cutscene: 'cs_rf_siege' },
  { on: 'quest_turned_in', questId: 'q_rf_last_toll', cutscene: 'cs_rf_finale' },
  // While M3 / M4 is open: a Corpseherd or Valdras killed before the quest comes back on its accept (DECISIONS M7) and
  // the kill that counts plays the beat.
  { on: 'monster_killed', monsterId: 'miniboss_corpseherd', cutscene: 'cs_rf_herd', whileQuest: 'q_rf_ashwheat' },
  { on: 'monster_killed', monsterId: 'valdras', cutscene: 'cs_rf_marshal_falls', whileQuest: 'q_rf_marshal' },
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

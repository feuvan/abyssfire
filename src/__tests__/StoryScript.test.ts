import { describe, expect, it } from 'vitest';
import { BOSS_INTROS, CHAPTERS, CREDITS, CUTSCENES, EPILOGUE, PROLOGUE, STORY_TRIGGERS } from '../data/story/script';
import type { Cutscene, CutsceneStep } from '../data/story/types';
import { STORY_EN, STORY_ZH } from '../i18n/locales/story';
import { AllMaps } from '../data/maps';
import { AllQuests } from '../data/quests/all_quests';
import { NPCDefinitions } from '../data/npcs';
import { getMonsterDef } from '../data/monsters';

/** Keys the playback code reads directly (not referenced from script.ts). */
const UI_KEYS = ['story.ui.skip', 'story.ui.skipTouch', 'story.speaker.villain', 'story.speaker.hero'];

/** Every string that looks like a story key, found anywhere in the script data. */
function collectKeys(value: unknown, out: Set<string>): Set<string> {
  if (typeof value === 'string') {
    if (value.startsWith('story.')) out.add(value);
  } else if (Array.isArray(value)) {
    for (const v of value) collectKeys(v, out);
  } else if (value && typeof value === 'object') {
    for (const v of Object.values(value)) collectKeys(v, out);
  }
  return out;
}

const referenced = collectKeys([PROLOGUE, CHAPTERS, CUTSCENES, BOSS_INTROS, EPILOGUE, CREDITS], new Set());

function npcsIn(zone: string): Set<string> {
  const map = AllMaps[zone];
  if (!map) return new Set(); // the labyrinth has no NPCs
  return new Set([...map.camps.flatMap(c => c.npcs), ...(map.fieldNpcs ?? []).map(n => n.npcId)]);
}

/** Monsters that only live in the Abyss Labyrinth (Zone 6), whose floors are generated per run. */
const LABYRINTH = 'abyss_labyrinth';
const LABYRINTH_MONSTERS = new Set(['dungeon_abyss_lord', 'dungeon_mid_boss', 'dungeon_shade', 'dungeon_fiend']);

function zonesWithMonster(monsterId: string): string[] {
  if (LABYRINTH_MONSTERS.has(monsterId)) return [LABYRINTH];
  return Object.keys(AllMaps).filter(z => AllMaps[z].spawns.some(s => s.monsterId === monsterId));
}

/** The zone(s) each cutscene can play in, derived from its triggers / boss intro. */
function cutsceneZones(): Map<string, Set<string>> {
  const zones = new Map<string, Set<string>>();
  const add = (cs: string, zone: string) => zones.set(cs, new Set([...(zones.get(cs) ?? []), zone]));
  for (const t of STORY_TRIGGERS) {
    if (t.on === 'monster_killed') {
      for (const z of zonesWithMonster(t.monsterId)) add(t.cutscene, z);
    } else if (t.on === 'zone_entered') {
      add(t.cutscene, t.zoneId);
    } else {
      const quest = AllQuests.find(q => q.id === t.questId);
      if (quest) add(t.cutscene, quest.zone);
    }
  }
  for (const b of BOSS_INTROS) for (const z of zonesWithMonster(b.monsterId)) add(b.cutscene, z);
  return zones;
}

const zh = (key: string) => [...(STORY_ZH[key] ?? '').replace(/\n/g, '')].length;

describe('story script', () => {
  it('references only keys that exist in both locales', () => {
    for (const key of referenced) {
      expect(STORY_ZH[key], `zh ${key}`).toBeTruthy();
      expect(STORY_EN[key], `en ${key}`).toBeTruthy();
    }
  });

  it('has no unused locale keys, and zh/en carry the same keys', () => {
    const allowed = new Set([...referenced, ...UI_KEYS]);
    expect(Object.keys(STORY_ZH).filter(k => !allowed.has(k))).toEqual([]);
    expect(Object.keys(STORY_EN).filter(k => !allowed.has(k))).toEqual([]);
    expect(Object.keys(STORY_EN).sort()).toEqual(Object.keys(STORY_ZH).sort());
    for (const k of UI_KEYS) expect(STORY_ZH[k] && STORY_EN[k], k).toBeTruthy();
  });

  it('has a prologue, five chapters, an epilogue and credits', () => {
    expect(PROLOGUE.id).toBe('prologue');
    expect(PROLOGUE.slides.length).toBeGreaterThanOrEqual(5);
    expect(PROLOGUE.slides.length).toBeLessThanOrEqual(7);
    expect(EPILOGUE.id).toBe('epilogue');
    expect(EPILOGUE.slides.length).toBeGreaterThanOrEqual(5);
    expect(EPILOGUE.slides.length).toBeLessThanOrEqual(7);
    expect(CREDITS.credits).toBe(true);
    expect(CHAPTERS.map(c => c.zoneId)).toEqual(['emerald_plains', 'twilight_forest', 'anvil_mountains', 'scorching_desert', 'abyss_rift']);
    for (const c of CHAPTERS) expect(AllMaps[c.zoneId], c.zoneId).toBeDefined();
  });

  it('keys every cutscene by its id, with 5–14 steps', () => {
    for (const [id, cs] of Object.entries(CUTSCENES)) {
      expect(cs.id).toBe(id);
      expect(cs.steps.length, id).toBeGreaterThanOrEqual(5);
      expect(cs.steps.length, id).toBeLessThanOrEqual(14);
    }
  });

  it('wires every trigger to an existing cutscene and target', () => {
    for (const t of STORY_TRIGGERS) {
      expect(CUTSCENES[t.cutscene], t.cutscene).toBeDefined();
      if (t.on === 'monster_killed') expect(getMonsterDef(t.monsterId), t.monsterId).toBeDefined();
      else if (t.on === 'zone_entered') expect(AllMaps[t.zoneId], t.zoneId).toBeDefined();
      else expect(AllQuests.some(q => q.id === t.questId), t.questId).toBe(true);
    }
  });

  it('introduces each zone boss with a title card', () => {
    const bosses = ['goblin_chief', 'werewolf_alpha', 'mountain_troll', 'phoenix', 'demon_lord', 'dungeon_abyss_lord'];
    expect(BOSS_INTROS.map(b => b.monsterId).sort()).toEqual([...bosses].sort());
    for (const b of BOSS_INTROS) {
      const cs = CUTSCENES[b.cutscene];
      expect(cs, b.cutscene).toBeDefined();
      expect(cs.steps.some(s => s.kind === 'title'), `${b.cutscene} title step`).toBe(true);
      expect(getMonsterDef(b.monsterId), b.monsterId).toBeDefined();
    }
  });

  it('gives every cutscene a zone, and every speaker / focus target stands there', () => {
    const zones = cutsceneZones();
    for (const [id, cs] of Object.entries(CUTSCENES) as [string, Cutscene][]) {
      const csZones = zones.get(id);
      expect(csZones?.size, `${id} has a zone`).toBeGreaterThan(0);
      for (const zone of csZones ?? []) {
        const npcs = npcsIn(zone);
        const check = (ref: string | { npc: string } | { monster: string } | { col: number; row: number }) => {
          if (typeof ref !== 'object') return;
          if ('npc' in ref) {
            expect(NPCDefinitions[ref.npc], ref.npc).toBeDefined();
            expect(npcs.has(ref.npc), `${id}: ${ref.npc} in ${zone}`).toBe(true);
          } else if ('monster' in ref) {
            expect(getMonsterDef(ref.monster), ref.monster).toBeDefined();
            expect(zonesWithMonster(ref.monster), `${id}: ${ref.monster} in ${zone}`).toContain(zone);
          } else {
            const map = AllMaps[zone];
            expect(ref.col >= 0 && ref.col < map.cols && ref.row >= 0 && ref.row < map.rows, `${id}: tile in bounds`).toBe(true);
          }
        };
        for (const step of cs.steps as CutsceneStep[]) {
          if (step.kind === 'say') check(step.speaker);
          if (step.kind === 'focus') check(step.target);
        }
      }
    }
  });

  it('keeps the Chinese text within its length limits', () => {
    const over: string[] = [];
    const limit = (key: string, max: number) => { if (zh(key) > max) over.push(`${key} (${zh(key)} > ${max}): ${STORY_ZH[key]}`); };
    for (const cs of Object.values(CUTSCENES)) {
      for (const s of cs.steps) {
        if (s.kind === 'say' || s.kind === 'whisper') limit(s.text, 60);
        if (s.kind === 'narrate') limit(s.text, 70);
        if (s.kind === 'title') { limit(s.title, 12); limit(s.subtitle, 14); }
      }
    }
    for (const seq of [PROLOGUE, EPILOGUE, CREDITS]) {
      for (const s of seq.slides) {
        if (s.text) limit(s.text, 110);
        if (s.title) limit(s.title, 12);
      }
    }
    for (const c of CHAPTERS) { limit(c.title, 12); limit(c.subtitle, 14); limit(c.text, 110); }
    for (const b of BOSS_INTROS) { limit(b.name, 12); limit(b.epithet, 14); }
    expect(over).toEqual([]);
  });

  it('uses full-width punctuation in Chinese lines', () => {
    const bad = Object.entries(STORY_ZH)
      .filter(([k]) => !k.startsWith('story.credits.') && k !== 'story.ui.skip')
      .filter(([, v]) => /[一-鿿][,.!?;:]|[,.!?;:][一-鿿]/.test(v))
      .map(([k]) => k);
    expect(bad).toEqual([]);
  });
});

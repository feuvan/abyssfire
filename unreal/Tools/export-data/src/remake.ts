/**
 * Remake content plumbing (implementation-plan.md §2): the field schema gate, the override hook for tables embedded in
 * web systems, the `remake` blocks and the redirect self-checks. Remake content itself lives in unreal/GameData/**.
 *
 *  - REMAKE_SCHEMA (main.ts) lists the remake-only features the C++ core can read. `withholdRemakeFields` fails on a
 *    record key that is neither a web field nor a registered remake field (remakeKeys.ts), then strips every registered
 *    remake field whose feature is not listed (a field may be authored before its core package lands);
 *    `gateRemakeValues` fails on a registered value (e.g. objective type `clear_area`) the core cannot read yet.
 *  - `overrideEntries` / `overrideById` apply GameData/world.ts REMAKE_OVERRIDES to one web table, per entry, and record
 *    what changed for the `remake` block of each file the table feeds. Unknown override tables or keys fail the export.
 *  - `noteConfig` records a GameData/config.ts value that differs from the behaviour the exporter hard-coded before the
 *    remake owned its content (`CONFIG_BASELINE`), like an override.
 *  - `remakeBlock(file)` is appended to every written table (and golden fixture) that the remake changed (main.ts);
 *    nothing is added while the remake changes nothing, so the Phase 0 export is byte-identical to the web's.
 *  - `divergedRedirects` compares each redirected GameData module with the hash of its Phase 0 snapshot
 *    (redirects.json `snapshots`), never with the live web file, so a web-only edit changes nothing here.
 */
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { REMAKE_OVERRIDES, type EntryOverride, type RemakeOverrides } from '../../../GameData/world';
import { CHAPTER_CONFIG, type ChapterConfig } from '../../../GameData/config';
import { RECORD_KEYS, DERIVED_ONLY, type LevelKeys } from './remakeKeys';
import { assert, sha256 } from './util';

const WORLD_TS = 'unreal/GameData/world.ts';
const CONFIG_TS = 'unreal/GameData/config.ts';

// ── Schema gate ─────────────────────────────────────────────────────────────

export type RemakeKind = 'monster' | 'quest' | 'map' | 'npc' | 'cutscene' | 'trigger';

/**
 * Remake-only fields (implementation-plan.md §2.6), by record kind. `path` is dotted; `[]` walks an array. A field is
 * written to unreal/Data only once its `feature` is in REMAKE_SCHEMA. Add a row when a package introduces a field.
 */
export const REMAKE_FIELDS: readonly { kind: RemakeKind; path: string; feature: string; wave: 1 | 2 | 3 }[] = [
  { kind: 'monster', path: 'family', feature: 'monster.family', wave: 1 },
  { kind: 'monster', path: 'damageType', feature: 'monster.damageType', wave: 1 },
  { kind: 'monster', path: 'abilities', feature: 'monster.abilities', wave: 2 },
  { kind: 'monster', path: 'onDeath', feature: 'monster.onDeath', wave: 2 },
  { kind: 'monster', path: 'aura', feature: 'monster.aura', wave: 2 },
  { kind: 'monster', path: 'phases', feature: 'monster.phases', wave: 2 },
  { kind: 'monster', path: 'kite', feature: 'monster.kite', wave: 2 },
  { kind: 'monster', path: 'block', feature: 'monster.block', wave: 2 },
  { kind: 'monster', path: 'morale', feature: 'monster.morale', wave: 2 },
  { kind: 'monster', path: 'corpse', feature: 'monster.corpse', wave: 2 },
  { kind: 'monster', path: 'riseAgain', feature: 'monster.riseAgain', wave: 2 },
  { kind: 'monster', path: 'botPriority', feature: 'monster.botPriority', wave: 2 },
  { kind: 'monster', path: 'hitCapHeroMaxHpPct', feature: 'monster.hitCap', wave: 2 },
  { kind: 'quest', path: 'progressKey', feature: 'quest.progressKey', wave: 1 },
  { kind: 'quest', path: 'rewards.choiceLabels', feature: 'quest.choiceLabels', wave: 1 },
  { kind: 'quest', path: 'defendTarget.waves', feature: 'quest.defendWaves', wave: 1 },
  { kind: 'quest', path: 'defendTarget.approachDeg', feature: 'quest.defendApproach', wave: 1 },
  { kind: 'quest', path: 'setsFlags', feature: 'quest.setsFlags', wave: 2 },
  { kind: 'quest', path: 'rewards.permanent', feature: 'quest.permanentRewards', wave: 2 },
  { kind: 'quest', path: 'objectives[].zone', feature: 'quest.objectiveZone', wave: 3 },
  { kind: 'map', path: 'presentationTheme', feature: 'map.presentationTheme', wave: 1 },
  { kind: 'map', path: 'subDungeonEntrances[].sealed', feature: 'map.sealedEntrances', wave: 1 },
  { kind: 'map', path: 'subDungeonEntrances[].sealedTextKey', feature: 'map.sealedEntrances', wave: 1 },
  { kind: 'map', path: 'regions', feature: 'map.regions', wave: 1 },
  { kind: 'map', path: 'spawns[].mix', feature: 'map.spawnMix', wave: 2 },
  { kind: 'map', path: 'spawns[].leader', feature: 'map.spawnMix', wave: 2 },
  { kind: 'map', path: 'spawns[].variants', feature: 'map.spawnVariants', wave: 2 },
  { kind: 'map', path: 'corpseSeeds', feature: 'map.corpseSeeds', wave: 2 },
  { kind: 'npc', path: 'lines', feature: 'npc.lines', wave: 2 },
  { kind: 'npc', path: 'visibleWhen', feature: 'npc.visibleWhen', wave: 2 },
  { kind: 'cutscene', path: 'steps[].anim', feature: 'story.focusAnim', wave: 1 },
  { kind: 'trigger', path: 'whileQuest', feature: 'story.triggerWhileQuest', wave: 1 },
];

/** Remake-only values of existing fields: a record using one before its feature lands fails the export. */
export const REMAKE_VALUES: readonly { kind: RemakeKind; path: string; value: string; feature: string; wave: 1 | 2 | 3 }[] = [
  { kind: 'quest', path: 'objectives[].type', value: 'clear_area', feature: 'quest.clearArea', wave: 3 },
];

const KNOWN_FEATURES = new Set([...REMAKE_FIELDS.map(f => f.feature), ...REMAKE_VALUES.map(v => v.feature)]);
let schema = new Set<string>();
const withheld = new Map<string, { count: number; example: string }>();

/** The level of a REMAKE_FIELDS path ('' for a top-level field, 'rewards' for 'rewards.choiceLabels', ...) and its key. */
function splitPath(path: string): [string, string] {
  const i = path.lastIndexOf('.');
  return i < 0 ? ['', path] : [path.slice(0, i), path.slice(i + 1)];
}

export function configureRemakeSchema(features: readonly string[]): void {
  for (const f of features) assert(KNOWN_FEATURES.has(f), `REMAKE_SCHEMA: unknown feature '${f}' (registered: ${[...KNOWN_FEATURES].join(', ')})`);
  schema = new Set(features);
  withheld.clear();
  // The key gate's lists (remakeKeys.ts, pinned to the types by tsc) and the field registry must agree: every typed
  // remake field is registered (or derived-only), and every registered field sits on a level the gate checks.
  for (const [kind, levels] of Object.entries(RECORD_KEYS) as [RemakeKind, Record<string, LevelKeys>][]) {
    for (const [level, k] of Object.entries(levels)) {
      for (const key of k.remake) {
        const path = level ? `${level}.${key}` : key;
        assert(REMAKE_FIELDS.some(f => f.kind === kind && f.path === path) || (!level && DERIVED_ONLY[kind]?.includes(key)),
          `GameData/types.ts types the ${kind} field '${path}', which REMAKE_FIELDS (remake.ts) does not register: add its row ` +
          '(kind, path, feature, wave) so REMAKE_SCHEMA gates it');
      }
    }
  }
  for (const f of REMAKE_FIELDS) {
    const [level] = splitPath(f.path);
    assert(RECORD_KEYS[f.kind][level], `REMAKE_FIELDS ${f.kind}.${f.path}: level '${level}' has no key list in remakeKeys.ts RECORD_KEYS`);
  }
}

/** Nodes of `obj` at a dotted level ('' = obj; `[]` walks an array); non-objects are skipped. */
function nodesAt(obj: unknown, level: string): Record<string, unknown>[] {
  let nodes: unknown[] = [obj];
  for (const part of level ? level.split('.') : []) {
    const [key, arr] = part.endsWith('[]') ? [part.slice(0, -2), true] : [part, false];
    nodes = nodes.flatMap(n => {
      if (!n || typeof n !== 'object') return [];
      const v = (n as Record<string, unknown>)[key];
      return arr ? (Array.isArray(v) ? v : []) : [v];
    });
  }
  return nodes.filter((n): n is Record<string, unknown> => !!n && typeof n === 'object' && !Array.isArray(n));
}

/**
 * Fails on a record key that is neither a field of the web type nor a remake-only field (typed in GameData/types.ts or
 * registered in REMAKE_FIELDS) at every checked level of `kind` — a misspelt or unregistered field must not reach
 * unreal/Data past the REMAKE_SCHEMA gate.
 */
export function checkRecordKeys(kind: RemakeKind, obj: unknown, where: string): void {
  for (const [level, k] of Object.entries(RECORD_KEYS[kind])) {
    const registered = REMAKE_FIELDS.filter(f => f.kind === kind && splitPath(f.path)[0] === level).map(f => splitPath(f.path)[1]);
    const allowed = new Set([...k.web, ...k.remake, ...registered]);
    for (const node of nodesAt(obj, level)) {
      for (const key of Object.keys(node)) {
        assert(allowed.has(key), `${where}${level ? `.${level}` : ''}: unknown ${kind} field '${key}' — neither a web field nor a ` +
          'registered remake field (typo? a new remake field needs a GameData/types.ts type, a REMAKE_FIELDS row and remakeKeys.ts)');
      }
    }
  }
}

/** Parents of `path` in `obj` (walking `[]` through arrays), with the last key: [[parent, key], ...]. */
function walk(obj: unknown, path: string): [Record<string, unknown>, string][] {
  const parts = path.split('.');
  let nodes: unknown[] = [obj];
  for (let i = 0; i < parts.length - 1; i++) {
    const [key, arr] = parts[i].endsWith('[]') ? [parts[i].slice(0, -2), true] : [parts[i], false];
    nodes = nodes.flatMap(n => {
      if (!n || typeof n !== 'object') return [];
      const v = (n as Record<string, unknown>)[key];
      return arr ? (Array.isArray(v) ? v : []) : [v];
    });
  }
  const last = parts[parts.length - 1];
  return nodes.filter((n): n is Record<string, unknown> => !!n && typeof n === 'object' && !Array.isArray(n)).map(n => [n, last]);
}

/**
 * Checks the record's keys (checkRecordKeys), then deletes the registered remake fields of `kind` that REMAKE_SCHEMA does
 * not list yet (in place; returns `obj`).
 */
export function withholdRemakeFields<T>(kind: RemakeKind, obj: T, where: string): T {
  checkRecordKeys(kind, obj, where);
  for (const f of REMAKE_FIELDS) {
    if (f.kind !== kind || schema.has(f.feature)) continue;
    for (const [parent, key] of walk(obj, f.path)) {
      if (!(key in parent)) continue;
      delete parent[key];
      const w = withheld.get(f.feature) ?? { count: 0, example: `${where}.${f.path}` };
      w.count++;
      withheld.set(f.feature, w);
    }
  }
  return obj;
}

/** Fails if `obj` uses a registered remake-only value whose feature REMAKE_SCHEMA does not list yet. */
export function gateRemakeValues(kind: RemakeKind, obj: unknown, where: string): void {
  for (const v of REMAKE_VALUES) {
    if (v.kind !== kind || schema.has(v.feature)) continue;
    for (const [parent, key] of walk(obj, v.path)) {
      assert(parent[key] !== v.value, `${where}: ${v.path} = '${v.value}' needs REMAKE_SCHEMA feature '${v.feature}' ` +
        `(wave ${v.wave}); the core cannot read it yet`);
    }
  }
}

export function withheldSummary(): string[] {
  return [...withheld].map(([f, w]) => `${f}: ${w.count} field(s) withheld (e.g. ${w.example})`);
}

// ── Override hook ───────────────────────────────────────────────────────────

/** Every override table the exporter applies (dotted path into REMAKE_OVERRIDES). */
export type OverridePath =
  | 'achievements' | 'randomEvents'
  | 'zoneMoods.themeByZone' | 'zoneMoods.moods' | 'zoneMoods.palettes' | 'zoneMoods.weather' | 'zoneMoods.weatherByTheme'
  | 'zoneMoods.campThemes' | 'zoneMoods.fog' | 'zoneMoods.lightScale' | 'zoneMoods.moodWeather' | 'zoneMoods.windupTint'
  | 'eliteAffixes.definitions' | 'eliteAffixes.zoneAffixCounts'
  | 'terrainStyles';

const OVERRIDE_PATHS: readonly OverridePath[] = ['achievements', 'randomEvents', 'zoneMoods.themeByZone', 'zoneMoods.moods',
  'zoneMoods.palettes', 'zoneMoods.weather', 'zoneMoods.weatherByTheme', 'zoneMoods.campThemes', 'zoneMoods.fog',
  'zoneMoods.lightScale', 'zoneMoods.moodWeather', 'zoneMoods.windupTint',
  'eliteAffixes.definitions', 'eliteAffixes.zoneAffixCounts', 'terrainStyles'];
const ENTRY_KEYS = new Set(['replace', 'add', 'remove']);
const applied = new Set<OverridePath>();

interface RemakeRecord { source: Set<string>; modules: Map<string, string>; replaced: Set<string>; added: Set<string>; removed: Set<string>;
  extra: Map<string, unknown> }
const records = new Map<string, RemakeRecord>();

function record(file: string): RemakeRecord {
  let r = records.get(file);
  if (!r) {
    r = { source: new Set(), modules: new Map(), replaced: new Set(), added: new Set(), removed: new Set(), extra: new Map() };
    records.set(file, r);
  }
  return r;
}

function overrideAt(path: OverridePath): EntryOverride<unknown> | undefined {
  let node: unknown = REMAKE_OVERRIDES;
  for (const part of path.split('.')) node = node && typeof node === 'object' ? (node as Record<string, unknown>)[part] : undefined;
  return node as EntryOverride<unknown> | undefined;
}

/** Fails on override tables the exporter does not know (a typo or a stale table name) and on malformed tables. */
export function validateOverrides(): void {
  const visit = (node: unknown, prefix: string): void => {
    assert(node && typeof node === 'object' && !Array.isArray(node), `${WORLD_TS}: REMAKE_OVERRIDES${prefix ? `.${prefix}` : ''} must be an object`);
    for (const [k, v] of Object.entries(node as Record<string, unknown>)) {
      if (v === undefined) continue;
      const path = prefix ? `${prefix}.${k}` : k;
      if ((OVERRIDE_PATHS as readonly string[]).includes(path)) {
        assert(v && typeof v === 'object' && !Array.isArray(v), `${WORLD_TS}: REMAKE_OVERRIDES.${path} must be {replace?, add?, remove?}`);
        for (const key of Object.keys(v)) {
          assert(ENTRY_KEYS.has(key), `${WORLD_TS}: REMAKE_OVERRIDES.${path}.${key}: expected replace / add / remove`);
        }
      } else if (OVERRIDE_PATHS.some(p => p.startsWith(`${path}.`))) {
        visit(v, path);
      } else {
        assert(false, `${WORLD_TS}: REMAKE_OVERRIDES.${path} is not a table the exporter overrides ` +
          `(known: ${OVERRIDE_PATHS.join(', ')}) — typo or stale override`);
      }
    }
  };
  visit(REMAKE_OVERRIDES as RemakeOverrides, '');
  applied.clear();
  records.clear();
}

/** Fails if an override table is present in REMAKE_OVERRIDES but no exporter table applied it. */
export function checkOverridesApplied(): void {
  for (const p of OVERRIDE_PATHS) {
    assert(!overrideAt(p) || applied.has(p), `${WORLD_TS}: REMAKE_OVERRIDES.${p} was never applied by an exporter table`);
  }
}

function noteChange(files: readonly string[], kind: 'replaced' | 'added' | 'removed', name: string): void {
  for (const f of files) {
    const r = record(f);
    r.source.add(WORLD_TS);
    r[kind].add(name);
  }
}

/**
 * `override ?? web` for a table keyed by id (an object). Replaced entries keep their position, added entries are
 * appended, removed entries are dropped. `files` = the Data files this table feeds (their `remake` block records it).
 */
export function overrideEntries<T>(path: OverridePath, files: readonly string[], web: Readonly<Record<string, T>>): Record<string, T> {
  applied.add(path);
  const ov = overrideAt(path) as EntryOverride<T> | undefined;
  if (!ov) return { ...web };
  const where = `${WORLD_TS}: REMAKE_OVERRIDES.${path}`;
  for (const k of Object.keys(ov.replace ?? {})) assert(k in web, `${where}.replace.${k}: no such entry in the web table (typo or stale override)`);
  for (const k of ov.remove ?? []) assert(k in web, `${where}.remove: no such entry '${k}' in the web table (typo or stale override)`);
  for (const k of Object.keys(ov.add ?? {})) assert(!(k in web), `${where}.add.${k}: the web table already has it (use replace)`);
  for (const k of ov.remove ?? []) assert(!(k in (ov.replace ?? {})), `${where}: '${k}' is both replaced and removed`);
  const out: Record<string, T> = {};
  for (const [k, v] of Object.entries(web)) {
    if (ov.remove?.includes(k)) { noteChange(files, 'removed', `${path}.${k}`); continue; }
    if (ov.replace && k in ov.replace) { out[k] = ov.replace[k]; noteChange(files, 'replaced', `${path}.${k}`); continue; }
    out[k] = v;
  }
  for (const [k, v] of Object.entries(ov.add ?? {})) { out[k] = v; noteChange(files, 'added', `${path}.${k}`); }
  return out;
}

/** `override ?? web` for a list of `{id}` records, keyed by id. A replacement may carry a new id (renamed in place). */
export function overrideById<T extends { id: string }>(path: OverridePath, files: readonly string[], web: readonly T[]): T[] {
  const byId = Object.fromEntries(web.map(e => [e.id, e]));
  assert(Object.keys(byId).length === web.length, `${path}: duplicate ids in the web table`);
  const merged = overrideEntries(path, files, byId);
  const where = `${WORLD_TS}: REMAKE_OVERRIDES.${path}`;
  const ov = overrideAt(path) as EntryOverride<T> | undefined;
  for (const [k, v] of Object.entries(ov?.add ?? {})) assert(v.id === k, `${where}.add.${k}: entry id '${v.id}' differs from its key`);
  const out = Object.values(merged);
  const ids = new Set<string>();
  for (const e of out) { assert(!ids.has(e.id), `${where}: duplicate id '${e.id}' after the override`); ids.add(e.id); }
  return out;
}

// ── Chapter config (GameData/config.ts) ─────────────────────────────────────

/**
 * What the exporter hard-coded before the remake owned its content (the web game's chapter: DECISIONS M7 / W7), i.e.
 * the Phase 0 values of GameData/config.ts. A config value that differs is recorded in the `remake` block of every file
 * it feeds (`replaced: ["config.storyBoss"]`). `finalBoss` unset = the web StoryDirector's FINAL_BOSS (the caller passes
 * it).
 */
const CONFIG_BASELINE: Omit<ChapterConfig, 'finalBoss'> = {
  storyBoss: { monsterId: 'goblin_chief', notAfterQuestTurnIn: 'q_find_goblin_chief' },
  chapterCompleteQuest: 'q_secure_plains',
  chapterFinaleCutscene: 'cs_ep_finale',
  sealedGate: { exitTo: 'twilight_forest', messageKey: 'zone.exit.sealedChapter2' },
};

/** Records `config.<key>` in `files`' remake blocks when GameData/config.ts changes it (see CONFIG_BASELINE). */
export function noteConfig(files: readonly string[], key: keyof ChapterConfig, webFinalBoss?: string): void {
  const now = key === 'finalBoss' ? (CHAPTER_CONFIG.finalBoss ?? webFinalBoss) : CHAPTER_CONFIG[key];
  const base = key === 'finalBoss' ? webFinalBoss : CONFIG_BASELINE[key];
  assert(key !== 'finalBoss' || webFinalBoss !== undefined, 'noteConfig(finalBoss) needs the web FINAL_BOSS');
  if (JSON.stringify(now) === JSON.stringify(base)) return;
  for (const f of files) {
    const r = record(f);
    r.source.add(CONFIG_TS);
    r.replaced.add(`config.${key}`);
  }
}

// ── remake blocks ───────────────────────────────────────────────────────────

/** Records that `file` was produced from redirected GameData modules that differ from the web ones they replace. */
export function noteModules(file: string, modules: Readonly<Record<string, string>>): void {
  const r = record(file);
  for (const [web, gd] of Object.entries(modules)) { r.modules.set(web, gd); r.source.add(gd); }
}

/** Any other remake facts a table records (e.g. i18n `swept`); `source` = the GameData file that caused them. */
export function noteExtra(file: string, source: string, key: string, value: unknown): void {
  const r = record(file);
  r.source.add(source);
  r.extra.set(key, value);
}

/** The `remake` block for `file`, or undefined when the remake did not change it. */
export function remakeBlock(file: string): Record<string, unknown> | undefined {
  const r = records.get(file);
  if (!r) return undefined;
  const list = (s: Set<string>) => (s.size ? [...s] : undefined);
  return {
    source: [...r.source].sort(),
    modules: r.modules.size ? Object.fromEntries([...r.modules].sort(([a], [b]) => (a < b ? -1 : a > b ? 1 : 0))) : undefined,
    replaced: list(r.replaced),
    added: list(r.added),
    removed: list(r.removed),
    ...Object.fromEntries(r.extra),
  };
}

export function remakeFiles(): string[] { return [...records.keys()].sort(); }

// ── Redirect provenance ─────────────────────────────────────────────────────

/** Module text with every import / export specifier blanked (a snapshot differs from the web only in those). */
function normalizedModule(repoRoot: string, file: string): string {
  return readFileSync(resolve(repoRoot, file), 'utf8')
    .replace(/(\bfrom\s*)(['"])[^'"]*\2/g, '$1$2$2')
    .replace(/(\bimport\s*\(\s*)(['"])[^'"]*\2/g, '$1$2$2')
    .replace(/(^|\n)(\s*import\s*)(['"])[^'"]*\3/g, '$1$2$3$3');
}

/** sha256 of a module with its import / export specifiers blanked (the redirects.json `snapshots` digest). */
export function snapshotHash(repoRoot: string, file: string): string {
  return sha256(normalizedModule(repoRoot, file));
}

/**
 * Redirected web modules whose GameData replacement is no longer its Phase 0 snapshot (content was remade): the module
 * differs from the digest recorded in redirects.json `snapshots` when it was copied. The live web file is never
 * compared, so a web-only edit of a redirected module changes nothing in the export. A target without a digest fails
 * (the message gives the digest to record when the module is still a snapshot of the web one).
 */
export function divergedRedirects(repoRoot: string, file: string, modules: Readonly<Record<string, string>>,
  snapshots: Readonly<Record<string, string>>): Record<string, string> {
  const out: Record<string, string> = {};
  for (const [web, gd] of Object.entries(modules)) {
    const now = snapshotHash(repoRoot, gd);
    const recorded = snapshots[gd];
    if (!recorded) {
      const same = normalizedModule(repoRoot, web) === normalizedModule(repoRoot, gd);
      assert(false, `${file}: no snapshot digest for ${gd}. ` + (same
        ? `It is still a snapshot of ${web}: add "${gd}": "${now}" to "snapshots".`
        : `It already differs from ${web}: record the digest of the byte-identical copy it started from ` +
          `(copy ${web}, rewrite its web imports to @web/..., then sha256 with the import / export specifiers blanked; ` +
          'run.mjs prints it for a fresh copy).'));
    }
    if (now !== recorded) out[web] = gd;
  }
  return out;
}

/** True when a table `source` entry (a path, or a `dir/*.ts` glob) names `file`. */
export function sourceCites(entry: string, file: string): boolean {
  if (!entry.includes('*')) return entry === file;
  const re = new RegExp(`^${entry.split('*').map(s => s.replace(/[.+?^${}()|[\]\\]/g, '\\$&')).join('[^/]*')}$`);
  return re.test(file);
}

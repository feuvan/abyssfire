/**
 * Exporter driver: runs every table module, writes unreal/Data/*.json (+ index.json) and the golden
 * fixtures, then prints a record-count summary. With --check, writes to a temp dir and reports any
 * difference from the committed files instead.
 *
 * Remake content (unreal/GameData/**, implementation-plan.md §2): run.mjs redirects web content modules to their
 * GameData replacements; this driver checks that every redirect was used and every GameData module was loaded, that
 * every GameData/world.ts override table was applied, gates remake-only fields with REMAKE_SCHEMA and appends a `remake`
 * block to each table (and golden fixture) the remake changed.
 */
import { readFileSync, existsSync, readdirSync } from 'node:fs';
import { resolve, relative, dirname } from 'node:path';
import { setRepoRoot, setRedirects, writeTable, writeText, formatJson, sha256, mulberry32, assert, SCHEMA_VERSION, type Ctx, type TableResult,
  type GoldenFile } from './util';
import { configureRemakeSchema, validateOverrides, checkOverridesApplied, divergedRedirects, sourceCites, noteModules, remakeBlock,
  remakeFiles, withheldSummary } from './remake';

type TableOut = TableResult[] | { tables: TableResult[]; golden: GoldenFile[] };

/**
 * Remake-only features the C++ core can read (feature ids: REMAKE_FIELDS / REMAKE_VALUES in remake.ts). A remake field
 * whose feature is missing here is withheld from unreal/Data (a remake-only value fails the export), so content can be
 * authored before the core package that parses it lands: each core package (K1-K7) adds its features in the change
 * that teaches the loaders to read them. Phase 0: none.
 *
 * K1 content hooks (implementation-plan.md §3, package 1A): quest `progressKey` (LoadQuests.cpp → QuestCardState
 * progress lines), `rewards.choiceLabels` (one key per choice slot → QuestCardEntry::choiceLabels), `defendTarget.waves`
 * / `approachDeg` (MonsterSystem::SpawnDefendWave), map `subDungeonEntrances[].sealed` / `sealedTextKey` (the sealed
 * entrance toast, ZoneRuntime) and the cutscene focus step `anim` (StoryDirector → EvPlayAnim).
 *
 * 1B review fixes: the story trigger `whileQuest` (LoadQuests.cpp → StoryTriggerDef::whileQuest; StoryDirector fires a
 * monster_killed beat only while that quest is open).
 *
 * 1B part 1 (Chapter 1 world data): map `presentationTheme` and `regions` (LoadWorld.cpp → MapDef::presentationTheme /
 * regions, mood themes of zone_moods.json; DataStore checks them; UE U1 resolves art, terrain, camp kit and the region
 * blend from them).
 *
 * K2 monster abilities (package 2A): monster `abilities` (LoadMonsters.cpp → MonsterDef::abilities, shapes checked here
 * first by tables/monsters.ts; CombatSystem runs them), `damageType` (→ MonsterDef::damageType: monster swings, bolts and
 * abilities stop being physical-only) and `hitCapHeroMaxHpPct` (→ MonsterDef::hitCapHeroMaxHpPct, the per-difficulty
 * hit cap). The ability kinds raise / summon / buff / call stay value-gated (REMAKE_VALUES) until K3 / K5.
 * Still withheld: monster `family` (no core reader yet) and every other Wave 2 / 3 feature.
 */
export const REMAKE_SCHEMA: readonly string[] = [
  'quest.progressKey', 'quest.choiceLabels', 'quest.defendWaves', 'quest.defendApproach',
  'map.sealedEntrances', 'map.presentationTheme', 'map.regions',
  'story.focusAnim',
  'story.triggerWhileQuest',
  'monster.abilities', 'monster.damageType', 'monster.hitCap',
];

export interface RunOptions {
  repoRoot: string;
  outRoot: string;
  check: boolean;
  committedRoot: string;
  /** unreal/GameData/redirects.json as run.mjs loaded it, and the web paths its plugin redirected during this run. */
  redirects: { file: string; modules: Readonly<Record<string, string>>; snapshots: Readonly<Record<string, string>>;
    hits: ReadonlySet<string> };
  /** The unreal/GameData modules Vite loaded during this run (run.mjs `load` hook). */
  gameDataLoaded: ReadonlySet<string>;
}

const GAMEDATA = 'unreal/GameData';
/**
 * GameData modules that hold only types (erased at load: `import type`), so the export never loads them. Every other
 * GameData module must be loaded, and these may not export a value (a value would need loading to take effect).
 */
const TYPE_ONLY_MODULES = ['unreal/GameData/types.ts', 'unreal/GameData/story/types.ts', 'unreal/GameData/autopilot/types.ts'];

/** Self-test: every redirect target exists and every redirected web path was loaded (a stale entry fails the export). */
function checkRedirects(opts: RunOptions): void {
  const { file, modules, hits } = opts.redirects;
  for (const [web, gd] of Object.entries(modules)) {
    assert(existsSync(resolve(opts.repoRoot, gd)), `${file}: stale redirect ${web} -> ${gd}: the target does not exist`);
    assert(hits.has(web), `${file}: stale redirect ${web} -> ${gd}: no module imported ${web} during the export ` +
      '(the web module is no longer used, or the entry is a typo); remove or fix the entry');
  }
}

/**
 * Self-test: every unreal/GameData module was loaded by the export. A module nothing loads is content that silently does
 * not ship — a redirects.json entry was dropped (the web module is exported instead) or a new module was never wired
 * (no redirect, no exporter table).
 */
function checkGameDataLoaded(opts: RunOptions): string[] {
  const all = listFiles(resolve(opts.repoRoot, GAMEDATA)).map(f => `${GAMEDATA}/${f.replace(/\\/g, '/')}`)
    .filter(f => /\.(ts|mts|js|mjs)$/.test(f) && !f.endsWith('.d.ts')).sort();
  for (const t of TYPE_ONLY_MODULES) {
    assert(all.includes(t), `main.ts TYPE_ONLY_MODULES: ${t} does not exist (stale entry)`);
    const value = readFileSync(resolve(opts.repoRoot, t), 'utf8').split('\n')
      .find(l => /^\s*export\s+(?!type\b|interface\b)/.test(l));
    assert(!value, `${t} is listed as type-only (main.ts TYPE_ONLY_MODULES) but exports a value: ${value?.trim()}`);
  }
  const redirectedTo = new Map(Object.entries(opts.redirects.modules).map(([web, gd]) => [gd, web]));
  for (const f of all) {
    if (TYPE_ONLY_MODULES.includes(f) || opts.gameDataLoaded.has(f)) continue;
    assert(false, `GameData module ${f} is never loaded during the export, so its content does not ship: ` + (redirectedTo.has(f)
      ? `nothing imports ${redirectedTo.get(f)} any more (stale redirect)`
      : `add its ${opts.redirects.file} entry (the web module it replaces) or the exporter table that reads it`));
  }
  return all.filter(f => opts.gameDataLoaded.has(f));
}

function listFiles(dir: string, base = dir): string[] {
  if (!existsSync(dir)) return [];
  return readdirSync(dir, { withFileTypes: true }).flatMap(e => {
    const p = resolve(dir, e.name);
    return e.isDirectory() ? listFiles(p, base) : [relative(base, p)];
  });
}

export async function run(opts: RunOptions): Promise<boolean> {
  setRepoRoot(opts.repoRoot);
  setRedirects(opts.redirects.modules);
  configureRemakeSchema(REMAKE_SCHEMA);
  validateOverrides();
  const ctx: Ctx = { repoRoot: opts.repoRoot, outRoot: opts.outRoot, check: opts.check, committedRoot: opts.committedRoot };

  // No web module may make the export depend on Math.random (some draw at load time): pin it.
  const savedRandom = Math.random;
  Math.random = mulberry32(0x5eed);
  const tables: TableResult[] = [];
  const golden: GoldenFile[] = [];
  try {
    const modules = [
      () => import('./tables/skills'),
      () => import('./tables/combat'),
      () => import('./tables/items'),
      () => import('./tables/monsters'),
      () => import('./tables/maps'),
      () => import('./tables/world'),
      () => import('./tables/quests'),
      () => import('./tables/story'),
      () => import('./tables/pets'),
      () => import('./tables/audio'),
      () => import('./tables/ui'),
      () => import('./tables/assets'),
      // The bot's routes check their ids against the content tables above (implementation-plan.md §6).
      () => import('./tables/autopilot'),
      // The UE set-piece layer (implementation-plan.md §4 U4): anchors checked against the maps, quests and lore.
      () => import('./tables/setpieces'),
      // Last: the i18n orphan sweep and key parity read every other exported table (the second argument).
      () => import('./tables/i18n'),
    ];
    for (const load of modules) {
      const mod = await load() as Record<string, unknown>;
      for (const [name, fn] of Object.entries(mod)) {
        if (!name.startsWith('export') || typeof fn !== 'function') continue;
        const out = await (fn as (c: Ctx, t: readonly TableResult[]) => Promise<TableOut> | TableOut)(ctx, tables);
        if (Array.isArray(out)) tables.push(...out);
        else { tables.push(...out.tables); golden.push(...out.golden); }
      }
    }
  } finally {
    Math.random = savedRandom;
  }

  // Remake self-tests and provenance (no-ops while GameData equals the web content).
  checkRedirects(opts);
  const loaded = checkGameDataLoaded(opts);
  checkOverridesApplied();
  const diverged = divergedRedirects(opts.repoRoot, opts.redirects.file, opts.redirects.modules, opts.redirects.snapshots);
  // A file is built from a remade module when its `source` (written) or `deps` (provenance only: modules it reads through
  // web code, e.g. the map generator's landmarks) cites the module's web path.
  const cite = (file: string, refs: readonly string[]) => {
    const cited = Object.fromEntries(Object.entries(diverged).filter(([web]) => refs.some(s => sourceCites(s, web))));
    if (Object.keys(cited).length) noteModules(file, cited);
  };
  for (const t of tables) cite(t.file, [...t.source, ...(t.deps ?? [])]);
  for (const g of golden) cite(g.path, g.deps);
  const written = new Set([...tables.map(t => t.file), ...golden.map(g => g.path)]);
  for (const f of remakeFiles()) assert(written.has(f), `remake record for ${f}, which no table or golden fixture wrote`);
  for (const t of tables) {
    const block = remakeBlock(t.file);
    if (!block) continue;
    assert(!('remake' in t.data), `${t.file}: the table already has a 'remake' key`);
    t.data = { ...t.data, remake: block };
  }
  for (const g of golden) {
    const block = remakeBlock(g.path);
    if (!block) continue;
    const doc = JSON.parse(g.text) as Record<string, unknown>;
    assert(!('remake' in doc), `${g.path}: the fixture already has a 'remake' key`);
    g.text = formatJson({ ...doc, remake: block }) + '\n';
  }

  // Write tables + index.
  const seen = new Set<string>();
  const index: Record<string, unknown>[] = [];
  for (const t of tables.sort((a, b) => (a.file < b.file ? -1 : a.file > b.file ? 1 : 0))) {
    if (seen.has(t.file)) throw new Error(`two tables write ${t.file}`);
    seen.add(t.file);
    const text = writeTable(ctx, t);
    index.push({ file: t.file, counts: t.counts, bytes: new TextEncoder().encode(text).length, sha256: sha256(text) });
  }
  for (const g of golden) writeText(resolve(ctx.outRoot, g.path), g.text);
  const indexText = formatJson({
    schemaVersion: SCHEMA_VERSION,
    source: ['unreal/Tools/export-data'],
    generator: 'node unreal/Tools/export-data/run.mjs',
    files: index,
    golden: golden.map(g => ({ path: g.path, sha256: sha256(g.text) })),
  }) + '\n';
  writeText(resolve(ctx.outRoot, 'Data/index.json'), indexText);

  // Summary.
  console.log(`\nExported ${tables.length} tables to ${relative(opts.repoRoot, resolve(ctx.outRoot, 'Data')) || ctx.outRoot}`);
  for (const t of tables) {
    const counts = Object.entries(t.counts).map(([k, v]) => `${k} ${v}`).join(', ');
    console.log(`  ${t.file.padEnd(26)} ${counts}`);
  }
  console.log(`  + ${golden.length} golden fixtures under CoreTests/golden/`);
  const redirected = Object.keys(opts.redirects.modules).length;
  console.log(`  remake: ${redirected} module(s) redirected to unreal/GameData (${Object.keys(diverged).length} remade), ` +
    `REMAKE_SCHEMA [${REMAKE_SCHEMA.join(', ')}], remake blocks in ${remakeFiles().length ? remakeFiles().join(', ') : 'no file'}`);
  for (const w of withheldSummary()) console.log(`  remake: withheld ${w}`);
  console.log(`  remake: GameData modules loaded: ${loaded.map(f => f.slice(GAMEDATA.length + 1)).join(', ')} ` +
    `(type-only: ${TYPE_ONLY_MODULES.map(f => f.slice(GAMEDATA.length + 1)).join(', ')})`);

  if (!opts.check) return true;
  // --check: compare with the committed tree — every committed Data file (but the hand-written README) and every file in
  // the golden fixture folders the export writes, so a table or fixture the export no longer produces shows up too.
  let same = true;
  const posixPath = (f: string) => f.replace(/\\/g, '/');
  const produced = new Set([...listFiles(resolve(ctx.outRoot, 'Data')).map(f => `Data/${posixPath(f)}`), ...golden.map(g => g.path)]);
  const goldenDirs = [...new Set(golden.map(g => dirname(g.path)))];
  const committed = new Set([
    ...listFiles(resolve(opts.committedRoot, 'Data')).map(f => `Data/${posixPath(f)}`).filter(f => f !== 'Data/README.md'),
    ...goldenDirs.flatMap(d => listFiles(resolve(opts.committedRoot, d)).map(f => `${d}/${posixPath(f)}`)),
  ]);
  for (const f of new Set([...produced, ...committed])) {
    const a = resolve(ctx.outRoot, f);
    const b = resolve(opts.committedRoot, f);
    if (!existsSync(a) || !existsSync(b) || readFileSync(a, 'utf8') !== readFileSync(b, 'utf8')) {
      console.log(`  DIFF ${f}`);
      same = false;
    }
  }
  console.log(same ? 'check: committed data is up to date' : 'check: committed data is STALE — re-run the exporter');
  return same;
}

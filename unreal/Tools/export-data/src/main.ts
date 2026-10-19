/**
 * Exporter driver: runs every table module, writes unreal/Data/*.json (+ index.json) and the golden
 * fixtures, then prints a record-count summary. With --check, writes to a temp dir and reports any
 * difference from the committed files instead.
 *
 * Remake content (unreal/GameData/**, implementation-plan.md §2): run.mjs redirects web content modules to their
 * GameData replacements; this driver checks that every redirect was used, that every GameData/world.ts override table
 * was applied, gates remake-only fields with REMAKE_SCHEMA and appends a `remake` block to each table the remake changed.
 */
import { readFileSync, existsSync, readdirSync } from 'node:fs';
import { resolve, relative } from 'node:path';
import { setRepoRoot, setRedirects, writeTable, writeText, formatJson, sha256, mulberry32, assert, SCHEMA_VERSION, type Ctx, type TableResult } from './util';
import { configureRemakeSchema, validateOverrides, checkOverridesApplied, divergedRedirects, sourceCites, noteModules, remakeBlock,
  remakeFiles, withheldSummary } from './remake';

type TableOut = TableResult[] | { tables: TableResult[]; golden: { path: string; text: string }[] };

/**
 * Remake-only features the C++ core can read (feature ids: REMAKE_FIELDS / REMAKE_VALUES in remake.ts). A remake field
 * whose feature is missing here is withheld from unreal/Data (a remake-only value fails the export), so content can be
 * authored before the core package that parses it lands: each core package (K1-K7) adds its features in the change
 * that teaches the loaders to read them. Phase 0: none.
 */
export const REMAKE_SCHEMA: readonly string[] = [];

export interface RunOptions {
  repoRoot: string;
  outRoot: string;
  check: boolean;
  committedRoot: string;
  /** unreal/GameData/redirects.json as run.mjs loaded it, and the web paths its plugin redirected during this run. */
  redirects: { file: string; modules: Readonly<Record<string, string>>; hits: ReadonlySet<string> };
}

/** Self-test: every redirect target exists and every redirected web path was loaded (a stale entry fails the export). */
function checkRedirects(opts: RunOptions): void {
  const { file, modules, hits } = opts.redirects;
  for (const [web, gd] of Object.entries(modules)) {
    assert(existsSync(resolve(opts.repoRoot, gd)), `${file}: stale redirect ${web} -> ${gd}: the target does not exist`);
    assert(hits.has(web), `${file}: stale redirect ${web} -> ${gd}: no module imported ${web} during the export ` +
      '(the web module is no longer used, or the entry is a typo); remove or fix the entry');
  }
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
  const golden: { path: string; text: string }[] = [];
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
  checkOverridesApplied();
  const diverged = divergedRedirects(opts.repoRoot, opts.redirects.modules);
  for (const t of tables) {
    const cited = Object.fromEntries(Object.entries(diverged).filter(([web]) => t.source.some(s => sourceCites(s, web))));
    if (Object.keys(cited).length) noteModules(t.file, cited);
  }
  const written = new Set(tables.map(t => t.file));
  for (const f of remakeFiles()) assert(written.has(f), `remake record for ${f}, which no table wrote`);
  for (const t of tables) {
    const block = remakeBlock(t.file);
    if (!block) continue;
    assert(!('remake' in t.data), `${t.file}: the table already has a 'remake' key`);
    t.data = { ...t.data, remake: block };
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

  if (!opts.check) return true;
  // --check: compare with the committed tree.
  let same = true;
  const produced = new Set([...listFiles(resolve(ctx.outRoot, 'Data')).map(f => `Data/${f}`), ...golden.map(g => g.path)]);
  const committed = new Set(listFiles(resolve(opts.committedRoot, 'Data')).filter(f => f.endsWith('.json')).map(f => `Data/${f}`));
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

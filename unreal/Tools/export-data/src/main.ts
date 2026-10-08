/**
 * Exporter driver: runs every table module, writes unreal/Data/*.json (+ index.json) and the golden
 * fixtures, then prints a record-count summary. With --check, writes to a temp dir and reports any
 * difference from the committed files instead.
 */
import { readFileSync, existsSync, readdirSync } from 'node:fs';
import { resolve, relative } from 'node:path';
import { setRepoRoot, writeTable, writeText, formatJson, sha256, mulberry32, SCHEMA_VERSION, type Ctx, type TableResult } from './util';

type TableOut = TableResult[] | { tables: TableResult[]; golden: { path: string; text: string }[] };

export interface RunOptions {
  repoRoot: string;
  outRoot: string;
  check: boolean;
  committedRoot: string;
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
      () => import('./tables/i18n'),
      () => import('./tables/ui'),
      () => import('./tables/assets'),
    ];
    for (const load of modules) {
      const mod = await load() as Record<string, unknown>;
      for (const [name, fn] of Object.entries(mod)) {
        if (!name.startsWith('export') || typeof fn !== 'function') continue;
        const out = await (fn as (c: Ctx) => Promise<TableOut> | TableOut)(ctx);
        if (Array.isArray(out)) tables.push(...out);
        else { tables.push(...out.tables); golden.push(...out.golden); }
      }
    }
  } finally {
    Math.random = savedRandom;
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

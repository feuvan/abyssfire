// Abyssfire data exporter — entry point.
//
// Loads the web game's TypeScript modules (src/**) directly through Vite's SSR module loader
// (the repo's own toolchain; no tsx / vite-node needed) and writes unreal/Data/*.json plus the
// golden map fixtures in unreal/CoreTests/golden/maps/.
//
//   node unreal/Tools/export-data/run.mjs            (from the repo root, or anywhere)
//   node unreal/Tools/export-data/run.mjs --check    (export to a temp dir and diff against the committed files)
//   node unreal/Tools/export-data/run.mjs --out=DIR  (export into DIR/Data + DIR/CoreTests, e.g. to review a change)
//
// Every run first type-checks unreal/GameData and the exporter (tsc -p tsconfig.json here; a type error fails the
// export). Nothing under src/ is modified. Module-private constants the exporter needs are re-exported
// in memory by the `expose` plugin below (see src/expose.ts for the list). Remake-owned content
// (unreal/GameData/**) replaces web content modules through the `redirect` plugin below
// (unreal/GameData/redirects.json, unreal/GameData/README.md).
import { createServer } from 'vite';
import { fileURLToPath } from 'node:url';
import { dirname, resolve, relative, isAbsolute, posix } from 'node:path';
import { readdirSync, mkdtempSync, readFileSync, existsSync, rmSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { tmpdir } from 'node:os';

const here = dirname(fileURLToPath(import.meta.url));
const repoRoot = resolve(here, '../../..');

// ── Arguments (strict: a typo must not fall back to overwriting the committed data) ──
const USAGE = 'usage: node unreal/Tools/export-data/run.mjs [--check | --out=DIR]';
let check = false;
let outArg;
for (const a of process.argv.slice(2)) {
  if (a === '--check') check = true;
  else if (a.startsWith('--out=')) {
    if (outArg !== undefined) { console.error(`--out given twice\n${USAGE}`); process.exit(2); }
    outArg = a.slice('--out='.length);
  } else if (a === '--help' || a === '-h') { console.log(USAGE); process.exit(0); }
  else { console.error(`unknown argument '${a}' (${a === '--out' ? 'write --out=DIR' : 'see usage'})\n${USAGE}`); process.exit(2); }
}
if (outArg !== undefined && (check || !outArg)) {
  console.error(`--out=DIR needs a directory and cannot be combined with --check\n${USAGE}`);
  process.exit(2);
}
const rel = id => relative(repoRoot, id.split('?')[0]).replace(/\\/g, '/');

/** Same value the game's vite.config.ts injects (`__BGM_MANIFEST__`). */
function bgmManifest() {
  const dir = resolve(repoRoot, 'public/assets/audio/bgm');
  if (!existsSync(dir)) return {};
  return Object.fromEntries(
    readdirSync(dir)
      .filter(f => f.endsWith('.mp3'))
      .sort()
      .map(f => [f.replace(/\.mp3$/, ''), `assets/audio/bgm/${f}`]),
  );
}

// The expose table is plain data in a TS file; parse it with a tiny loader first (it has no imports).
const exposeSrc = readFileSync(resolve(here, 'src/expose.ts'), 'utf8');
const EXPOSE = JSON.parse(exposeSrc.slice(exposeSrc.indexOf('/*JSON*/') + 8, exposeSrc.indexOf('/*END*/')));

// ── Redirects: web content module -> remake module (unreal/GameData/redirects.json) ──
// Checked here before Vite starts (shape, normalised paths, both ends exist, one target per module); main.ts fails the
// export when a redirected web path is never loaded (a stale entry), when a GameData module is never loaded (a missing
// entry) and checks each module's snapshot hash — see checkRedirects() / checkGameDataLoaded() there.
const REDIRECTS_FILE = 'unreal/GameData/redirects.json';
const GAMEDATA = 'unreal/GameData/';
const { REDIRECTS, SNAPSHOTS } = (() => {
  const fail = msg => {
    console.error(`export failed — ${REDIRECTS_FILE}: ${msg}`);
    process.exit(1);
  };
  /** A repo-relative POSIX path in normal form ('a/b.ts': no '.', '..' or empty segment, no backslash). */
  const normal = p => typeof p === 'string' && !p.includes('\\') && !isAbsolute(p) && posix.normalize(p) === p
    && p.split('/').every(s => s && s !== '.' && s !== '..');
  const inside = (dir, p) => !relative(resolve(repoRoot, dir), resolve(repoRoot, p)).startsWith('..');
  let doc;
  try { doc = JSON.parse(readFileSync(resolve(repoRoot, REDIRECTS_FILE), 'utf8')); } catch (e) { fail(`unreadable (${e.message})`); }
  const modules = doc?.modules;
  if (!modules || typeof modules !== 'object' || Array.isArray(modules)) fail('expected {"modules": {"src/...": "unreal/GameData/..."}, "snapshots": {...}}');
  const snapshots = doc.snapshots ?? {};
  if (typeof snapshots !== 'object' || Array.isArray(snapshots)) fail('"snapshots" must be {"unreal/GameData/...": "<sha256>"}');
  const targets = new Set();
  for (const [from, to] of Object.entries(modules)) {
    if (typeof to !== 'string') fail(`${from}: target must be a string`);
    if (!normal(from) || !from.startsWith('src/') || !inside('src', from) || !from.endsWith('.ts')) {
      fail(`${from}: the web path must be a normalised repo-relative .ts path under src/ (no '.', '..' or '//')`);
    }
    if (!normal(to) || !to.startsWith(GAMEDATA) || !inside(GAMEDATA, to) || !to.endsWith('.ts')) {
      fail(`${from} -> ${to}: the target must be a normalised .ts path under ${GAMEDATA} (no '.', '..' or '//')`);
    }
    if (!existsSync(resolve(repoRoot, from))) fail(`stale redirect: web module ${from} does not exist`);
    if (!existsSync(resolve(repoRoot, to))) fail(`stale redirect: ${from} -> ${to}, which does not exist`);
    if (targets.has(to)) fail(`two web modules redirect to ${to}`);
    targets.add(to);
  }
  for (const [gd, sha] of Object.entries(snapshots)) {
    if (!targets.has(gd)) fail(`snapshots.${gd}: not a redirect target (stale entry)`);
    if (typeof sha !== 'string' || !/^[0-9a-f]{64}$/.test(sha)) fail(`snapshots.${gd}: expected a sha256 hex digest`);
  }
  return { REDIRECTS: modules, SNAPSHOTS: snapshots };
})();
const REDIRECT_SOURCES = Object.fromEntries(Object.entries(REDIRECTS).map(([from, to]) => [to, from]));
/** Web paths the export actually loaded through a redirect (main.ts fails on an entry missing here). */
const redirectHits = new Set();
/** unreal/GameData modules Vite loaded during the export (main.ts fails on a GameData module missing here). */
const gameDataLoaded = new Set();
const webRoot = resolve(repoRoot, 'src') + '/';

function redirectPlugin() {
  return {
    name: 'abyss-export:redirect',
    enforce: 'pre',
    async resolveId(source, importer, options) {
      const r = await this.resolve(source, importer, { ...options, skipSelf: true });
      if (!r || r.external) return null;
      const target = rel(r.id);
      const fromGameData = importer && rel(importer).startsWith(GAMEDATA);
      const to = REDIRECTS[target];
      if (!to) {
        // GameData reaches the web only through the @web alias (README.md "Imports"): Vite's alias hands this hook the
        // aliased absolute path; a relative ('../../src/…') or root-relative ('/src/…') specifier fails.
        if (fromGameData && target.startsWith('src/') && !source.replace(/\\/g, '/').startsWith(webRoot)) {
          throw new Error(`GameData module ${rel(importer)} imports web module ${target} as '${source}'; ` +
            `use '@web/${target.slice(4).replace(/\.ts$/, '')}'`);
        }
        return null;
      }
      if (fromGameData) {
        throw new Error(`GameData module ${rel(importer)} imports redirected web module ${target}; import the GameData one (${to})`);
      }
      redirectHits.add(target);
      return resolve(repoRoot, to);
    },
    load(id) {
      const file = rel(id);
      if (file.startsWith(GAMEDATA)) gameDataLoaded.add(file);
      return null;
    },
  };
}

function exposePlugin() {
  return {
    name: 'abyss-export:expose',
    enforce: 'pre',
    transform(code, id) {
      const file = rel(id);
      // An entry for a redirected web module applies to the GameData module that replaces it.
      const names = EXPOSE[file] ?? EXPOSE[REDIRECT_SOURCES[file]];
      if (!names) return null;
      const list = names.map(n => `${n} as __expose_${n}`).join(', ');
      return { code: `${code}\n;export { ${list} };\n`, map: null };
    },
  };
}

// ── Type-check first: GameData records are object literals typed by GameData/types.ts, so tsc catches a misspelt or
// unregistered remake field (and src/remakeKeys.ts pins the record key lists the runtime gate uses to those types).
{
  const tsc = resolve(repoRoot, 'node_modules/typescript/bin/tsc');
  if (!existsSync(tsc)) {
    console.error('export failed — node_modules/typescript is missing: run `npm ci` at the repo root');
    process.exit(1);
  }
  const project = relative(repoRoot, resolve(here, 'tsconfig.json'));
  const r = spawnSync(process.execPath, [tsc, '-p', project, '--pretty', 'false'], { cwd: repoRoot, encoding: 'utf8' });
  if (r.status !== 0) {
    process.stderr.write(`${r.stdout ?? ''}${r.stderr ?? ''}${r.error ? `${r.error.message}\n` : ''}`);
    console.error(`export failed — type errors in unreal/GameData or the exporter (tsc -p ${project})`);
    process.exit(1);
  }
}

const server = await createServer({
  root: repoRoot,
  configFile: false,
  logLevel: 'error',
  appType: 'custom',
  server: { middlewareMode: true, hmr: false, watch: null },
  optimizeDeps: { noDiscovery: true, include: [] },
  // `@web/...` = the web game's src/ (GameData modules import web code through it; tsconfig.json maps it for tsc).
  resolve: { alias: { phaser: resolve(here, 'src/phaser-shim.ts'), '@web': resolve(repoRoot, 'src') } },
  define: { __BGM_MANIFEST__: JSON.stringify(bgmManifest()) },
  plugins: [redirectPlugin(), exposePlugin()],
});

let code = 0;
const outRoot = check ? mkdtempSync(resolve(tmpdir(), 'abyss-export-')) : outArg ? resolve(outArg) : resolve(repoRoot, 'unreal');
try {
  const main = await server.ssrLoadModule('/unreal/Tools/export-data/src/main.ts');
  const ok = await main.run({ repoRoot, outRoot, check, committedRoot: resolve(repoRoot, 'unreal'),
    redirects: { file: REDIRECTS_FILE, modules: REDIRECTS, snapshots: SNAPSHOTS, hits: redirectHits },
    gameDataLoaded });
  if (!ok) code = 1;
} catch (e) {
  console.error(e instanceof Error ? (e.stack ?? e.message) : e);
  code = 1;
} finally {
  if (check) rmSync(outRoot, { recursive: true, force: true });
  await server.close();
}
process.exit(code);

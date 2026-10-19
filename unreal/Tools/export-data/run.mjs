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
// Nothing under src/ is modified. Module-private constants the exporter needs are re-exported
// in memory by the `expose` plugin below (see src/expose.ts for the list). Remake-owned content
// (unreal/GameData/**) replaces web content modules through the `redirect` plugin below
// (unreal/GameData/redirects.json, unreal/GameData/README.md).
import { createServer } from 'vite';
import { fileURLToPath } from 'node:url';
import { dirname, resolve, relative, isAbsolute } from 'node:path';
import { readdirSync, mkdtempSync, readFileSync, existsSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';

const here = dirname(fileURLToPath(import.meta.url));
const repoRoot = resolve(here, '../../..');
const check = process.argv.includes('--check');
const outArg = process.argv.find(a => a.startsWith('--out='))?.slice('--out='.length);
if (outArg !== undefined && (check || !outArg)) {
  console.error('--out=DIR needs a directory and cannot be combined with --check');
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
// Checked here before Vite starts (shape, both ends exist, one target per module); main.ts fails the export when a
// redirected web path is never loaded (a stale entry) — see checkRedirects() there.
const REDIRECTS_FILE = 'unreal/GameData/redirects.json';
const REDIRECTS = (() => {
  const fail = msg => {
    console.error(`export failed — ${REDIRECTS_FILE}: ${msg}`);
    process.exit(1);
  };
  let doc;
  try { doc = JSON.parse(readFileSync(resolve(repoRoot, REDIRECTS_FILE), 'utf8')); } catch (e) { fail(`unreadable (${e.message})`); }
  const modules = doc?.modules;
  if (!modules || typeof modules !== 'object' || Array.isArray(modules)) fail('expected {"modules": {"src/...": "unreal/GameData/..."}}');
  const targets = new Set();
  for (const [from, to] of Object.entries(modules)) {
    if (typeof to !== 'string') fail(`${from}: target must be a string`);
    if (!from.startsWith('src/') || from.includes('\\') || isAbsolute(from)) fail(`${from}: the web path must be repo-relative under src/`);
    if (!to.startsWith('unreal/GameData/') || to.includes('\\')) fail(`${from} -> ${to}: the target must be under unreal/GameData/`);
    if (!existsSync(resolve(repoRoot, from))) fail(`stale redirect: web module ${from} does not exist`);
    if (!existsSync(resolve(repoRoot, to))) fail(`stale redirect: ${from} -> ${to}, which does not exist`);
    if (targets.has(to)) fail(`two web modules redirect to ${to}`);
    targets.add(to);
  }
  return modules;
})();
const REDIRECT_SOURCES = Object.fromEntries(Object.entries(REDIRECTS).map(([from, to]) => [to, from]));
/** Web paths the export actually loaded through a redirect (main.ts fails on an entry missing here). */
const redirectHits = new Set();

function redirectPlugin() {
  return {
    name: 'abyss-export:redirect',
    enforce: 'pre',
    async resolveId(source, importer, options) {
      const r = await this.resolve(source, importer, { ...options, skipSelf: true });
      if (!r || r.external) return null;
      const target = rel(r.id);
      const fromGameData = importer && rel(importer).startsWith('unreal/GameData/');
      const to = REDIRECTS[target];
      if (!to) {
        // GameData reaches the web only through the @web alias (README.md "Imports").
        if (fromGameData && target.startsWith('src/') && source.startsWith('.')) {
          throw new Error(`GameData module ${rel(importer)} imports web module ${target} by a relative path; use '@web/${target.slice(4).replace(/\.ts$/, '')}'`);
        }
        return null;
      }
      if (fromGameData) {
        throw new Error(`GameData module ${rel(importer)} imports redirected web module ${target}; import the GameData one (${to})`);
      }
      redirectHits.add(target);
      return resolve(repoRoot, to);
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
    redirects: { file: REDIRECTS_FILE, modules: REDIRECTS, hits: redirectHits } });
  if (!ok) code = 1;
} catch (e) {
  console.error(e instanceof Error ? (e.stack ?? e.message) : e);
  code = 1;
} finally {
  if (check) rmSync(outRoot, { recursive: true, force: true });
  await server.close();
}
process.exit(code);

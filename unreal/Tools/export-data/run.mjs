// Abyssfire data exporter — entry point.
//
// Loads the web game's TypeScript modules (src/**) directly through Vite's SSR module loader
// (the repo's own toolchain; no tsx / vite-node needed) and writes unreal/Data/*.json plus the
// golden map fixtures in unreal/CoreTests/golden/maps/.
//
//   node unreal/Tools/export-data/run.mjs            (from the repo root, or anywhere)
//   node unreal/Tools/export-data/run.mjs --check    (export to a temp dir and diff against the committed files)
//
// Nothing under src/ is modified. Module-private constants the exporter needs are re-exported
// in memory by the `expose` plugin below (see src/expose.ts for the list).
import { createServer } from 'vite';
import { fileURLToPath } from 'node:url';
import { dirname, resolve, relative } from 'node:path';
import { readdirSync, mkdtempSync, readFileSync, existsSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';

const here = dirname(fileURLToPath(import.meta.url));
const repoRoot = resolve(here, '../../..');
const check = process.argv.includes('--check');

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

function exposePlugin() {
  return {
    name: 'abyss-export:expose',
    enforce: 'pre',
    transform(code, id) {
      const file = relative(repoRoot, id.split('?')[0]).replace(/\\/g, '/');
      const names = EXPOSE[file];
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
  resolve: { alias: { phaser: resolve(here, 'src/phaser-shim.ts') } },
  define: { __BGM_MANIFEST__: JSON.stringify(bgmManifest()) },
  plugins: [exposePlugin()],
});

let code = 0;
try {
  const outRoot = check ? mkdtempSync(resolve(tmpdir(), 'abyss-export-')) : resolve(repoRoot, 'unreal');
  const main = await server.ssrLoadModule('/unreal/Tools/export-data/src/main.ts');
  const ok = await main.run({ repoRoot, outRoot, check, committedRoot: resolve(repoRoot, 'unreal') });
  if (check) rmSync(outRoot, { recursive: true, force: true });
  if (!ok) code = 1;
} catch (e) {
  console.error(e instanceof Error ? (e.stack ?? e.message) : e);
  code = 1;
} finally {
  await server.close();
}
process.exit(code);

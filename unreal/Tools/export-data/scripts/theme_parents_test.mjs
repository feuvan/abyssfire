// Tests of the exporter's theme parent checks (src/themeParents.ts) and of the committed tables:
//   node unreal/Tools/export-data/scripts/theme_parents_test.mjs      (Node 22.18+: it loads the .ts file itself)
// Exit 0 when every case passes; prints the failed cases and exits 1 otherwise.
import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { KIT_THEME_FILE, MAX_THEME_CHAIN, kitParentMismatches, kitThemeParents, themeParentProblems } from '../src/themeParents.ts';

const repoRoot = resolve(dirname(fileURLToPath(import.meta.url)), '../../../..');
let failed = 0;
let cases = 0;
function check(name, ok, detail = '') {
  cases++;
  if (!ok) {
    failed++;
    console.error(`FAIL ${name}${detail ? `: ${detail}` : ''}`);
  }
}
const known = new Set(['plains', 'march', 'pit', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i']);
const problems = parents => themeParentProblems(parents, known, 'themeParents');

// valid tables
check('empty table', problems({}).length === 0);
check('pit -> march', problems({ pit: 'march' }).length === 0);
// unknown themes, either end
check('unknown theme', problems({ crypt: 'march' }).some(p => p.includes("'crypt' is not a terrain style")));
check('unknown parent', problems({ pit: 'marsh' }).some(p => p.includes("parent 'marsh' is not a terrain style")));
check('empty parent', problems({ pit: '' }).some(p => p.includes('must be a theme name')));
check('non-string parent', problems({ pit: 3 }).some(p => p.includes('must be a theme name')));
// cycles: itself, two, three (every member of the loop is reported)
check('self loop', problems({ pit: 'pit' }).some(p => p.includes("loops at 'pit'")));
check('loop of two', problems({ pit: 'march', march: 'pit' }).filter(p => p.includes('loops at')).length === 2);
check('loop of three', problems({ a: 'b', b: 'c', c: 'a' }).filter(p => p.includes('loops at')).length === 3);
check('a tail into a loop', problems({ pit: 'a', a: 'b', b: 'a' }).some(p => p.startsWith('themeParents.pit') && p.includes('loops at')));
// depth: 8 names pass, 9 fail (for the deepest theme only)
const chainOf = names => Object.fromEntries(names.slice(0, -1).map((n, k) => [n, names[k + 1]]));
const eight = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'];
check('MAX_THEME_CHAIN is the module\'s', MAX_THEME_CHAIN === 8);
check('a chain of 8 names', problems(chainOf(eight)).length === 0, problems(chainOf(eight)).join('; '));
const nine = problems(chainOf([...eight, 'i']));
check('a chain of 9 names', nine.length === 1 && nine[0].startsWith('themeParents.a:') && nine[0].includes('names 9 themes'), nine.join('; '));

// the kit's reader
const sample = `
THEMES: dict[str, dict] = {
    "plains": {
        "zones": (),
    },
    "march": {
        "zones": ("emerald_plains",),
    },
    "pit": {
        "zones": ("sub_rf_pit",),
        "parent": "march",          # a comment
    },
}
BASE_THEME = "plains"
`;
const read = kitThemeParents(sample);
check('kit reader: themes', JSON.stringify(read.themes) === JSON.stringify(['plains', 'march', 'pit']));
check('kit reader: parents', JSON.stringify(read.parents) === JSON.stringify({ pit: 'march' }));
const throws = (name, text) => {
  let threw = false;
  try { kitThemeParents(text); } catch { threw = true; }
  check(name, threw);
};
throws('kit reader: no THEMES block', 'BASE_THEME = "plains"\n');
throws('kit reader: unclosed block', 'THEMES = {\n    "pit": {\n        "parent": "march",\n');
throws('kit reader: two parents', 'THEMES = {\n    "pit": {\n        "parent": "march",\n        "parent": "plains",\n    },\n}\n');
check('mismatch: equal', kitParentMismatches({ pit: 'march' }, { pit: 'march' }).length === 0);
check('mismatch: other parent', kitParentMismatches({ pit: 'plains' }, { pit: 'march' }).length === 1);
check('mismatch: missing in the data', kitParentMismatches({}, { pit: 'march' }).length === 1);
check('mismatch: missing in the kit', kitParentMismatches({ pit: 'march', a: 'b' }, { pit: 'march' }).length === 1);

// the committed tables: the data's parents pass the checks and equal the kit's
const styles = JSON.parse(readFileSync(resolve(repoRoot, 'unreal/Data/terrain_styles.json'), 'utf8'));
const kit = kitThemeParents(readFileSync(resolve(repoRoot, KIT_THEME_FILE), 'utf8'));
const shipped = themeParentProblems(styles.themeParents, new Set(Object.keys(styles.themes)), 'terrain_styles.json themeParents');
check('shipped: valid', shipped.length === 0, shipped.join('; '));
check('shipped: pit -> march', styles.themeParents.pit === 'march');
const diff = kitParentMismatches(styles.themeParents, kit.parents);
check('shipped: equals kit/theme.py', diff.length === 0, diff.join('; '));
check('shipped: every kit theme with a parent is a terrain style', Object.keys(kit.parents).every(t => t in styles.themes));

console.log(`theme_parents_test: ${cases - failed} / ${cases} passed`);
process.exit(failed === 0 ? 0 : 1);

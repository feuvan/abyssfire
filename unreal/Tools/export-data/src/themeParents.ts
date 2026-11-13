/**
 * terrain_styles.json `themeParents` (presentation theme -> parent theme; GameData/world.ts
 * REMAKE_OVERRIDES.themeParents): the checks the export runs, kept free of imports so
 * scripts/theme_parents_test.mjs can load this file with Node alone.
 *
 * - both ends are known themes (terrain styles),
 * - no chain loops,
 * - a chain names at most MAX_THEME_CHAIN themes, the theme itself included (the UE module walks no further:
 *   AbyssThemeRules::MaxThemeChain; a longer chain would be cut there and only reported at the zone build),
 * - the table equals the Blender kit's (unreal/Art/blender/kit/theme.py THEMES[...]["parent"]): the art ships its
 *   `<id>@<theme>` variants along the kit's chain, the game looks them up along this one.
 */

/** AbyssThemeRules::MaxThemeChain (Source/Abyssfire/Private/World/AbyssThemeRules.h). */
export const MAX_THEME_CHAIN = 8;

/** The kit's theme module, from the repo root. */
export const KIT_THEME_FILE = 'unreal/Art/blender/kit/theme.py';

/** The problems of a parent table, one sentence each (empty: valid). `where` prefixes each entry's name. */
export function themeParentProblems(parents: Readonly<Record<string, unknown>>, known: ReadonlySet<string>, where: string): string[] {
  const problems: string[] = [];
  for (const [theme, parent] of Object.entries(parents)) {
    const at = `${where}.${theme}`;
    if (typeof parent !== 'string' || parent === '') {
      problems.push(`${at}: the parent must be a theme name`);
      continue;
    }
    if (!known.has(theme)) problems.push(`${at}: '${theme}' is not a terrain style (terrain_styles.json themes)`);
    if (!known.has(parent)) problems.push(`${at}: parent '${parent}' is not a terrain style (terrain_styles.json themes)`);
    const chain = [theme];
    let looped = false;
    for (let t: unknown = parent; typeof t === 'string' && t !== ''; t = parents[t]) {
      if (chain.includes(t)) {
        problems.push(`${at}: the parent chain loops at '${t}' (${chain.join(' -> ')} -> ${t})`);
        looped = true;
        break;
      }
      chain.push(t);
    }
    if (!looped && chain.length > MAX_THEME_CHAIN) {
      problems.push(`${at}: the parent chain names ${chain.length} themes (${chain.join(' -> ')}); the game walks ` +
        `${MAX_THEME_CHAIN} at the most (AbyssThemeRules::MaxThemeChain)`);
    }
  }
  return problems;
}

/**
 * theme -> parent as kit/theme.py's THEMES literal states it. The module is plain data (`"<theme>": {` at one
 * indent level, `"parent": "<theme>",` inside); a THEMES block this reader cannot find is an error, not an empty table.
 */
export function kitThemeParents(source: string): { themes: string[]; parents: Record<string, string> } {
  const lines = source.split(/\r?\n/);
  const start = lines.findIndex(l => /^THEMES\b.*=\s*\{\s*$/.test(l));
  if (start < 0) throw new Error(`${KIT_THEME_FILE}: no 'THEMES = {' block (the exporter reads the kit's theme parents from it)`);
  const themes: string[] = [];
  const parents: Record<string, string> = {};
  let current: string | undefined;
  let closed = false;
  for (let i = start + 1; i < lines.length; i++) {
    const line = lines[i];
    if (/^\}/.test(line)) { closed = true; break; }
    const head = /^ {4}"([A-Za-z0-9_]+)":\s*\{/.exec(line);
    if (head) {
      current = head[1];
      themes.push(current);
      continue;
    }
    const parent = /^\s+"parent":\s*"([A-Za-z0-9_]+)"/.exec(line);
    if (parent) {
      if (current === undefined) throw new Error(`${KIT_THEME_FILE}:${i + 1}: a "parent" outside a theme`);
      if (current in parents) throw new Error(`${KIT_THEME_FILE}:${i + 1}: theme '${current}' has two parents`);
      parents[current] = parent[1];
    }
  }
  if (!closed || themes.length === 0) throw new Error(`${KIT_THEME_FILE}: the THEMES block could not be read`);
  return { themes, parents };
}

/** The differences between the data's table and the kit's (empty: they agree). */
export function kitParentMismatches(data: Readonly<Record<string, unknown>>, kit: Readonly<Record<string, string>>): string[] {
  const problems: string[] = [];
  for (const theme of [...new Set([...Object.keys(data), ...Object.keys(kit)])].sort()) {
    const a = data[theme];
    const b = kit[theme];
    if (a === b) continue;
    problems.push(`theme '${theme}': terrain_styles.json themeParents says ${a === undefined ? 'no parent' : `'${String(a)}'`}, ` +
      `${KIT_THEME_FILE} says ${b === undefined ? 'no parent' : `'${b}'`}`);
  }
  return problems;
}

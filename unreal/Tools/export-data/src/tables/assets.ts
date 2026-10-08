/**
 * assets.json — the art pipeline's manifest (unreal/Art/Export/manifest.json, written by the Blender
 * scripts) re-published next to the other tables so DataStore loads one directory (ARCHITECTURE §5).
 * Skipped (with a note) until the art pipeline has produced it.
 */
import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import type { Ctx, TableResult } from '../util';

export function exportAssets(ctx: Ctx): TableResult[] {
  const path = resolve(ctx.repoRoot, 'unreal/Art/Export/manifest.json');
  if (!existsSync(path)) {
    console.log('  note: unreal/Art/Export/manifest.json not found yet — Data/assets.json not written');
    return [];
  }
  const manifest = JSON.parse(readFileSync(path, 'utf8')) as unknown;
  return [{
    file: 'assets.json',
    source: ['unreal/Art/Export/manifest.json'],
    data: { manifest: manifest as Record<string, unknown> },
    counts: { entries: manifest && typeof manifest === 'object' ? Object.keys(manifest as object).length : 0 },
  }];
}

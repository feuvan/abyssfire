/**
 * set_pieces.json — the UE set-piece layer (implementation-plan.md §2.3 / §4 U4): unreal/GameData/setPieces.ts, written
 * with every optional field filled in (the C++ loader, abyss/data/SetPieceData.h, reads one shape).
 *
 * Content checks here (a stale id fails the export): the zone exists; every anchor resolves on its map (camp number,
 * entrance, region, a quest of the zone with a defend target) and every tile lies on it; ids are unique per zone; rings
 * are sane; `echoes` are echo lore of the zone; socket recipes exist in the art lane's VFX recipes, emitters and cue
 * ids in the audio manifest (when those files exist). The core checks the shape and the map / quest / lore references
 * again (DataStore::Finalize); CoreTests setpieces_test checks the placements against the generated map, the art
 * manifest and the bot's route.
 */
import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { AllMaps } from '../../../../../src/data/maps';
import { AllQuests } from '../../../../../src/data/quests/all_quests';
import { AllLoreEntries } from '../../../../../src/data/loreCollectibles';
import { SET_PIECE_ZONES, type SetPieceAnchor, type SetPieceDef, type SetPieceFacing } from '../../../../GameData/setPieces';
import type { QuestDefinition } from '../../../../GameData/types';
import { assert, plain, type Ctx, type TableResult } from '../util';

interface MapLike {
  cols: number;
  rows: number;
  camps?: readonly { col: number; row: number }[];
  subDungeonEntrances?: readonly { id: string }[];
  regions?: readonly { id: string }[];
}

function readJson(ctx: Ctx, rel: string): Record<string, unknown> | null {
  const path = resolve(ctx.repoRoot, rel);
  if (!existsSync(path)) {
    console.log(`  note: ${rel} not found — set-piece ${rel.includes('VFX') ? 'recipes' : 'audio ids'} not checked`);
    return null;
  }
  return JSON.parse(readFileSync(path, 'utf8')) as Record<string, unknown>;
}

const KINDS_ANCHOR = ['tile', 'camp', 'entrance', 'region', 'defendTarget'] as const;
const KINDS_FACING = ['yaw', 'front', 'camera', 'tile', 'anchor', 'ringCentre'] as const;

export function exportSetPieces(ctx: Ctx): TableResult[] {
  const recipesDoc = readJson(ctx, 'unreal/Art/Export/VFX/vfx_recipes.json');
  const recipes = recipesDoc ? new Set(Object.keys((recipesDoc.recipes ?? {}) as object)) : null;
  const audio = readJson(ctx, 'unreal/Audio/Export/audio_manifest.json');
  const emitters = audio ? new Set(Object.keys(((audio.ambience as Record<string, unknown> | undefined)?.emitters ?? {}) as object)) : null;
  const cueIds = audio ? new Set(Object.keys((audio.cues ?? {}) as object)) : null;
  const quests = new Map((AllQuests as QuestDefinition[]).map(q => [q.id, q]));
  const lore = new Map(AllLoreEntries.map(l => [l.id, l]));

  const zonesOut: Record<string, unknown> = {};
  let pieceCount = 0;
  for (const z of SET_PIECE_ZONES) {
    const map = (AllMaps as Record<string, MapLike | undefined>)[z.zone];
    assert(map, `set pieces: unknown zone '${z.zone}'`);
    assert(!(z.zone in zonesOut), `set pieces: zone '${z.zone}' listed twice`);
    const onMap = (col: number, row: number, where: string) => {
      assert(Number.isFinite(col) && Number.isFinite(row), `${where}: non-finite tile`);
      assert(col >= 0 && row >= 0 && col <= map.cols - 1 && row <= map.rows - 1,
        `${where}: tile [${col}, ${row}] outside ${z.zone} (${map.cols} x ${map.rows})`);
    };
    const ids = new Set<string>();
    const pieces = z.pieces.map((p: SetPieceDef) => {
      const where = `set piece ${z.zone}/${p.id}`;
      assert(/^[a-z0-9_]+$/.test(p.id), `${where}: ids are snake_case ASCII`);
      assert(!ids.has(p.id), `${where}: duplicate id`);
      ids.add(p.id);
      assert(p.area !== '' && /^[a-z0-9_]+$/.test(p.area), `${where}: area is snake_case ASCII`);
      assert(p.gameId !== '', `${where}: empty gameId`);
      const variant = p.variant ?? -1;
      assert(Number.isInteger(variant) && variant >= -1, `${where}: variant is an index (or omitted)`);
      const a: SetPieceAnchor = p.anchor;
      assert((KINDS_ANCHOR as readonly string[]).includes(a.kind), `${where}: unknown anchor kind`);
      switch (a.kind) {
        case 'tile': onMap(a.col, a.row, where); break;
        case 'camp':
          assert(Number.isInteger(a.camp) && a.camp >= 1 && a.camp <= (map.camps?.length ?? 0), `${where}: no camp ${a.camp}`);
          break;
        case 'entrance':
          assert(map.subDungeonEntrances?.some(e => e.id === a.id), `${where}: no entrance '${a.id}' on ${z.zone}`);
          break;
        case 'region': assert(map.regions?.some(r => r.id === a.id), `${where}: no region '${a.id}' on ${z.zone}`); break;
        case 'defendTarget': {
          const q = quests.get(a.quest);
          assert(q && q.zone === z.zone && q.defendTarget, `${where}: '${a.quest}' is no defend quest of ${z.zone}`);
          break;
        }
      }
      const f: SetPieceFacing = p.facing;
      assert((KINDS_FACING as readonly string[]).includes(f.kind), `${where}: unknown facing kind`);
      if (f.kind === 'tile') onMap(f.col, f.row, where);
      if (f.kind === 'yaw' || f.kind === 'front') assert(Number.isFinite(f.deg), `${where}: facing deg`);
      if (f.kind === 'ringCentre') assert(p.ring, `${where}: ringCentre facing needs a ring`);
      const ring = p.ring;
      if (ring) {
        assert(Number.isInteger(ring.count) && ring.count >= 1 && ring.count <= 64, `${where}: ring count 1..64`);
        assert(ring.radius > 0, `${where}: ring radius > 0`);
        for (const k of ring.skip ?? []) assert(Number.isInteger(k) && k >= 0 && k < ring.count, `${where}: ring skip ${k}`);
        for (const v of ring.variants ?? []) assert(Number.isInteger(v) && v >= 0, `${where}: ring variant ${v}`);
      }
      const scale = p.scale ?? 1;
      assert(scale > 0, `${where}: scale > 0`);
      for (const e of p.effects ?? []) {
        assert(e.socket !== '', `${where}: effect without a socket`);
        assert(!recipes || recipes.has(e.recipe), `${where}: unknown VFX recipe '${e.recipe}' (Art/Export/VFX/vfx_recipes.json)`);
      }
      for (const e of p.emitters ?? []) {
        assert(e.socket !== '', `${where}: emitter without a socket`);
        assert(!emitters || emitters.has(e.emitter), `${where}: unknown audio emitter '${e.emitter}' (audio_manifest ambience.emitters)`);
      }
      for (const c of p.cues ?? []) {
        assert(c.socket !== '' && c.cues.length > 0, `${where}: a cue entry needs a socket and cues`);
        assert(!recipes || recipes.has(c.recipe), `${where}: unknown VFX recipe '${c.recipe}'`);
        for (const id of c.cues) assert(!cueIds || cueIds.has(id), `${where}: unknown audio cue '${id}' (audio_manifest cues)`);
      }
      for (const id of p.echoes ?? []) {
        const l = lore.get(id);
        assert(l && l.zone === z.zone && (l.spriteType as string) === 'echo', `${where}: '${id}' is no echo lore of ${z.zone}`);
      }
      assert(p.echoTurnDeg === undefined || (Number.isFinite(p.echoTurnDeg) && (p.echoes ?? []).length > 0),
        `${where}: echoTurnDeg turns the echoes' figures (needs echoes)`);
      assert(p.walkOver === undefined || p.walkOver.trim() !== '', `${where}: walkOver names its reason`);
      return plain({
        id: p.id,
        area: p.area,
        gameId: p.gameId,
        fallbackIds: [...(p.fallbackIds ?? [])],
        variant,
        anchor: { ...a },
        offset: [...(p.offset ?? [0, 0])],
        ring: ring ? {
          count: ring.count, radius: ring.radius, startDeg: ring.startDeg, stepDeg: ring.stepDeg ?? 360 / ring.count,
          skip: [...(ring.skip ?? [])], variants: [...(ring.variants ?? [])],
        } : null,
        facing: { ...f },
        turnDeg: p.turnDeg ?? 0,
        scale,
        partners: [...(p.partners ?? [])],
        effects: (p.effects ?? []).map(e => ({ ...e })),
        emitters: (p.emitters ?? []).map(e => ({ ...e })),
        cues: (p.cues ?? []).map(c => ({ cues: [...c.cues], socket: c.socket, recipe: c.recipe })),
        echoes: [...(p.echoes ?? [])],
        echoTurnDeg: p.echoTurnDeg ?? 0,
        occluder: p.occluder ?? false,
        walkOver: p.walkOver ?? '',
        yieldsToDefendTarget: p.yieldsToDefendTarget ?? false,
        note: p.note ?? '',
      });
    });
    pieceCount += pieces.length;
    zonesOut[z.zone] = { pieces };
  }
  return [{
    file: 'set_pieces.json',
    source: ['unreal/GameData/setPieces.ts'],
    data: { zones: zonesOut },
    counts: { zones: SET_PIECE_ZONES.length, pieces: pieceCount },
  }];
}

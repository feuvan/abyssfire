/**
 * K7 helpers shared by the tables (implementation-plan.md §3 K7; MapData.h SubDungeonDef, QuestData.h
 * QuestObjectiveDef): the sub-dungeons as the remake types them, which of them are hand-authored (the ones the core
 * enters), and the world flags a quest sets by cleansing one (a `clear_area` objective sets its sub-dungeon's
 * `cleansedFlag` while the quest is open - WorldState.cpp WsCleanses).
 */
import { AllSubDungeons } from '../../../../src/data/subDungeons';
import type { SubDungeonMapData } from '../../../GameData/types';

export const subDungeons = (): Readonly<Record<string, SubDungeonMapData>> =>
  AllSubDungeons as unknown as Record<string, SubDungeonMapData>;

/** A sub-dungeon with `tiles` is hand-authored: the zone the hero enters (SubDungeonDef::hasMap). */
export const isHandAuthored = (sd: SubDungeonMapData | undefined): boolean => !!sd && sd.tiles !== undefined;

type ObjectiveLike = { type: string; targetId: string };

/** The world flags `q` sets by cleansing (its clear_area objectives' sub-dungeons' cleansedFlag). */
export function cleanseFlagsOf(q: { objectives: readonly ObjectiveLike[] }): string[] {
  const all = subDungeons();
  return q.objectives.filter(o => o.type === 'clear_area').map(o => all[o.targetId]?.cleansedFlag)
    .filter((f): f is string => typeof f === 'string' && f !== '');
}

/** Every world flag some quest sets: by `setsFlags` on its turn-in (K6) or by cleansing (K7). */
export function flagsSetByQuests(quests: readonly { setsFlags?: string[]; objectives: readonly ObjectiveLike[] }[]): Set<string> {
  return new Set(quests.flatMap(q => [...(q.setsFlags ?? []), ...cleanseFlagsOf(q)]));
}

/**
 * Ley-beast sheet registry: one drawer per pet × evolution stage.
 * `SpriteGenerator.ensurePetSheet` builds the textures; `getPetSheetMeta`
 * tells the follower code how to place and animate the sprite.
 */
import {
  PET_ACTION_ORDER,
  PET_ATTACK_CONTACT,
  PET_CAST_RELEASE_FRAME,
  PET_FRAME_H,
  PET_FRAME_RATES,
  PET_FRAME_W,
  PET_BASE_SCALE,
  PET_GROUND_Y,
  PET_IDS,
  PET_STAGES,
  definePet,
  petSheetKey,
  type PetAction,
  type PetDrawer,
  type PetId,
  type PetSpec,
  type PetStage,
} from './PetKit';
import { SpriteSpec } from './Sprite';
import { OwlSpec } from './Owl';
import { WolfSpec } from './Wolf';
import { CatSpec } from './Cat';
import { TortoiseSpec } from './Tortoise';
import { DragonSpec } from './Dragon';
import { PhoenixChickSpec } from './PhoenixChick';
import { ButterflySpec } from './Butterfly';

export * from './PetKit';

interface PetEntry {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  spec: PetSpec<any>;
  /** Top of the creature above the ground (units, before stage scale; includes hover). */
  height: number;
  /** Body centre height above the ground (units) for hovering pets, 0 for walkers. */
  hover: number;
}

const PETS: Record<PetId, PetEntry> = {
  pet_sprite: { spec: SpriteSpec, height: 42, hover: 22 },
  pet_owl: { spec: OwlSpec, height: 47, hover: 21 },
  pet_storm_wolf: { spec: WolfSpec, height: 37, hover: 0 },
  pet_cat: { spec: CatSpec, height: 34, hover: 0 },
  pet_jade_tortoise: { spec: TortoiseSpec, height: 26, hover: 0 },
  pet_dragon: { spec: DragonSpec, height: 34, hover: 0 },
  pet_phoenix: { spec: PhoenixChickSpec, height: 39, hover: 17 },
  pet_void_butterfly: { spec: ButterflySpec, height: 43, hover: 19 },
};

const drawerCache = new Map<string, PetDrawer>();

export function isPetId(id: string): id is PetId {
  return (PET_IDS as readonly string[]).includes(id);
}

/** Drawer for a pet sheet (null for unknown ids). */
export function getPetDrawer(petId: string, stage: PetStage): PetDrawer | null {
  if (!isPetId(petId)) return null;
  const st = (PET_STAGES.includes(stage) ? stage : 0) as PetStage;
  const key = petSheetKey(petId, st);
  let d = drawerCache.get(key);
  if (!d) {
    d = definePet(PETS[petId].spec, st);
    drawerCache.set(key, d);
  }
  return d;
}

/** Every pet × stage drawer (tests, previews). */
export function allPetDrawers(): PetDrawer[] {
  return PET_IDS.flatMap(id => PET_STAGES.map(st => getPetDrawer(id, st)!));
}

export interface PetSheetMeta {
  key: string;
  /** Frame size in world px (the texture is TEXTURE_SCALE × this; display at 1 / TEXTURE_SCALE). */
  frameW: number;
  frameH: number;
  /** Sprite origin that puts the creature's ground point on the container origin. */
  originX: number;
  originY: number;
  /** Visual height of the creature above the ground (world px), e.g. for name/HP bar placement. */
  heightPx: number;
  /** Hovering pets: height of the body centre above the ground (world px); 0 for walkers. */
  hoverPx: number;
  flyer: boolean;
  /** Use for `AnimConfig.attackContact` (the bite lands on the last attack frame). */
  attackContact: number;
  /** Cast frame on which the ability releases, and the ms until then at the registered rate. */
  castReleaseFrame: number;
  castReleaseMs: number;
  /** Suggested CharacterAnimator preset. */
  animCategory: 'beast' | 'flying';
  /** Registered animation keys (se; ne adds `_ne_`): `<key>_<action>`. */
  actions: readonly PetAction[];
  frameRates: Readonly<Record<PetAction, number>>;
}

/** Placement / animation metadata for a pet sheet (null for unknown ids). */
export function getPetSheetMeta(petId: string, stage: PetStage): PetSheetMeta | null {
  if (!isPetId(petId)) return null;
  const st = (PET_STAGES.includes(stage) ? stage : 0) as PetStage;
  const e = PETS[petId];
  const unitPx = PET_FRAME_H / 96;
  const k = e.spec.scale[st] * PET_BASE_SCALE;
  return {
    key: petSheetKey(petId, st),
    frameW: PET_FRAME_W,
    frameH: PET_FRAME_H,
    originX: 0.5,
    originY: PET_GROUND_Y / 96,
    heightPx: Math.round(e.height * k * unitPx),
    hoverPx: Math.round(e.hover * k * unitPx),
    flyer: e.spec.flyer,
    attackContact: PET_ATTACK_CONTACT,
    castReleaseFrame: PET_CAST_RELEASE_FRAME,
    castReleaseMs: Math.round((PET_CAST_RELEASE_FRAME * 1000) / PET_FRAME_RATES.cast),
    animCategory: e.spec.flyer ? 'flying' : 'beast',
    actions: PET_ACTION_ORDER,
    frameRates: PET_FRAME_RATES,
  };
}

// Static ley-beast portraits (`decor_pet_<petId>`): the stage-0 pet sheet's
// first front-view idle frame, framed as a 40×40 decoration (ground at 36).
// Used as icons and as the fallback follower image.
import type { EntityDrawer } from '../types';
import type { DecorDrawer } from './DecorKit';
import { PET_FRAME_H, PET_FRAME_W, PET_GROUND_Y, PET_IDS, type PetId } from '../pets/PetKit';
import { getPetDrawer } from '../pets';

const W = 40;
const H = 40;
const GROUND = 36;
/** Fit the 56-px pet frame into the 40-px portrait. */
const FIT = 0.82;

function createPetDrawer(petId: PetId): DecorDrawer {
  return {
    key: `decor_pet_${petId}`,
    frameW: W,
    frameH: H,
    totalFrames: 1,
    anchorY: GROUND / H,
    inked: true,
    drawFrame(ctx, _frame, _action, w) {
      const s = w / W;
      const pw = Math.round(PET_FRAME_W * s);
      const ph = Math.round(PET_FRAME_H * s);
      const tmp = document.createElement('canvas');
      tmp.width = pw;
      tmp.height = ph;
      const tctx = tmp.getContext('2d', { willReadFrequently: true });
      const drawer = getPetDrawer(petId, 0);
      if (!tctx || !drawer) return;
      drawer.drawPose(tctx, 'idle', 0, pw, ph, 'se');
      const k = FIT;
      const groundPx = (PET_GROUND_Y / 96) * ph;
      const dx = (W * s) / 2 - (pw * k) / 2;
      const dy = GROUND * s - groundPx * k;
      ctx.drawImage(tmp, dx, dy, pw * k, ph * k);
    },
  };
}

export const PetSpriteDrawers: readonly EntityDrawer[] = PET_IDS.map(createPetDrawer);

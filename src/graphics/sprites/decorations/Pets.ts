import type { EntityDrawer } from '../types';

type PetKind = 'sprite' | 'dragon' | 'owl' | 'cat' | 'phoenix' | 'storm_wolf' | 'jade_tortoise' | 'void_butterfly';

function createPetDrawer(key: string, kind: PetKind): EntityDrawer {
  return {
    key,
    frameW: 40,
    frameH: 40,
    totalFrames: 1,

    drawFrame(ctx, _frame, _action, w, h, utils) {
      const s = w / 40;
      const cx = w / 2;
      const ground = h * 0.9;
      ctx.fillStyle = 'rgba(0,0,0,0.22)';
      utils.fillEllipse(ctx, cx, ground, 11 * s, 2.8 * s);

      if (kind === 'sprite') {
        ctx.fillStyle = utils.rgb(0x78d6a5);
        ctx.beginPath();
        ctx.moveTo(cx, ground - 30 * s);
        ctx.lineTo(cx + 8 * s, ground - 8 * s);
        ctx.lineTo(cx, ground - 3 * s);
        ctx.lineTo(cx - 8 * s, ground - 8 * s);
        ctx.closePath();
        ctx.fill();
        ctx.fillStyle = utils.rgb(0xa7f0c7, 0.8);
        ctx.beginPath();
        ctx.moveTo(cx - 4 * s, ground - 24 * s);
        ctx.quadraticCurveTo(cx - 17 * s, ground - 25 * s, cx - 13 * s, ground - 11 * s);
        ctx.quadraticCurveTo(cx - 7 * s, ground - 15 * s, cx - 3 * s, ground - 17 * s);
        ctx.fill();
        ctx.beginPath();
        ctx.moveTo(cx + 4 * s, ground - 24 * s);
        ctx.quadraticCurveTo(cx + 17 * s, ground - 25 * s, cx + 13 * s, ground - 11 * s);
        ctx.quadraticCurveTo(cx + 7 * s, ground - 15 * s, cx + 3 * s, ground - 17 * s);
        ctx.fill();
        ctx.fillStyle = '#244233';
        utils.fillCircle(ctx, cx - 3 * s, ground - 20 * s, 1.2 * s);
        utils.fillCircle(ctx, cx + 3 * s, ground - 20 * s, 1.2 * s);
      } else if (kind === 'dragon') {
        ctx.fillStyle = utils.rgb(0xd95e32);
        utils.roundRect(ctx, cx - 11 * s, ground - 23 * s, 22 * s, 18 * s, 7 * s);
        ctx.fill();
        ctx.fillStyle = utils.rgb(0xf0a346);
        ctx.beginPath();
        ctx.moveTo(cx - 7 * s, ground - 21 * s);
        ctx.lineTo(cx - 18 * s, ground - 29 * s);
        ctx.lineTo(cx - 13 * s, ground - 14 * s);
        ctx.closePath();
        ctx.fill();
        ctx.beginPath();
        ctx.moveTo(cx + 7 * s, ground - 21 * s);
        ctx.lineTo(cx + 18 * s, ground - 29 * s);
        ctx.lineTo(cx + 13 * s, ground - 14 * s);
        ctx.closePath();
        ctx.fill();
        ctx.fillStyle = utils.rgb(0x703325);
        utils.fillCircle(ctx, cx - 4 * s, ground - 17 * s, 1.4 * s);
        utils.fillCircle(ctx, cx + 4 * s, ground - 17 * s, 1.4 * s);
        ctx.strokeStyle = utils.rgb(0x703325);
        ctx.lineWidth = 1.5 * s;
        ctx.beginPath();
        ctx.moveTo(cx + 10 * s, ground - 10 * s);
        ctx.quadraticCurveTo(cx + 19 * s, ground - 3 * s, cx + 14 * s, ground + 1 * s);
        ctx.stroke();
      } else if (kind === 'owl') {
        ctx.fillStyle = utils.rgb(0x87664e);
        utils.fillEllipse(ctx, cx, ground - 15 * s, 13 * s, 15 * s);
        ctx.fillStyle = utils.rgb(0xd8c7a6);
        utils.fillCircle(ctx, cx - 5 * s, ground - 20 * s, 6 * s);
        utils.fillCircle(ctx, cx + 5 * s, ground - 20 * s, 6 * s);
        ctx.fillStyle = '#242227';
        utils.fillCircle(ctx, cx - 5 * s, ground - 20 * s, 2.2 * s);
        utils.fillCircle(ctx, cx + 5 * s, ground - 20 * s, 2.2 * s);
        ctx.fillStyle = utils.rgb(0xd3a34f);
        ctx.beginPath();
        ctx.moveTo(cx, ground - 17 * s);
        ctx.lineTo(cx - 3 * s, ground - 12 * s);
        ctx.lineTo(cx + 3 * s, ground - 12 * s);
        ctx.closePath();
        ctx.fill();
      } else if (kind === 'cat') {
        ctx.fillStyle = utils.rgb(0x29232e);
        ctx.beginPath();
        ctx.moveTo(cx - 11 * s, ground - 10 * s);
        ctx.lineTo(cx - 9 * s, ground - 29 * s);
        ctx.lineTo(cx - 3 * s, ground - 23 * s);
        ctx.lineTo(cx + 3 * s, ground - 23 * s);
        ctx.lineTo(cx + 9 * s, ground - 29 * s);
        ctx.lineTo(cx + 11 * s, ground - 10 * s);
        ctx.closePath();
        ctx.fill();
        ctx.fillStyle = '#d4a3ff';
        utils.fillCircle(ctx, cx - 4 * s, ground - 18 * s, 1.5 * s);
        utils.fillCircle(ctx, cx + 4 * s, ground - 18 * s, 1.5 * s);
        ctx.strokeStyle = utils.rgb(0x6f4e87);
        ctx.lineWidth = 1.2 * s;
        ctx.beginPath();
        ctx.moveTo(cx + 9 * s, ground - 9 * s);
        ctx.quadraticCurveTo(cx + 20 * s, ground - 7 * s, cx + 13 * s, ground + 1 * s);
        ctx.stroke();
      } else if (kind === 'phoenix') {
        utils.drawFlameLayer(ctx, cx, ground - 4 * s, 18 * s, 32 * s, '#d64b2e', 0.9);
        utils.drawFlameLayer(ctx, cx, ground - 6 * s, 12 * s, 25 * s, '#f28b31', 1.7);
        ctx.fillStyle = '#ffe18b';
        utils.fillEllipse(ctx, cx, ground - 16 * s, 5 * s, 8 * s);
        ctx.fillStyle = '#4b2c31';
        utils.fillCircle(ctx, cx - 2 * s, ground - 18 * s, 1 * s);
        utils.fillCircle(ctx, cx + 2 * s, ground - 18 * s, 1 * s);
      } else if (kind === 'storm_wolf') {
        ctx.fillStyle = utils.rgb(0x527caa);
        ctx.beginPath();
        ctx.moveTo(cx - 14 * s, ground - 8 * s);
        ctx.lineTo(cx - 12 * s, ground - 26 * s);
        ctx.lineTo(cx - 6 * s, ground - 21 * s);
        ctx.lineTo(cx, ground - 31 * s);
        ctx.lineTo(cx + 7 * s, ground - 21 * s);
        ctx.lineTo(cx + 14 * s, ground - 26 * s);
        ctx.lineTo(cx + 14 * s, ground - 8 * s);
        ctx.closePath();
        ctx.fill();
        ctx.fillStyle = '#c3f1ff';
        utils.fillCircle(ctx, cx - 4 * s, ground - 18 * s, 1.7 * s);
        utils.fillCircle(ctx, cx + 4 * s, ground - 18 * s, 1.7 * s);
        ctx.strokeStyle = '#9be8ff';
        ctx.lineWidth = 1.2 * s;
        for (const dx of [-7, 0, 7]) {
          ctx.beginPath();
          ctx.moveTo(cx + dx * s, ground - 4 * s);
          ctx.lineTo(cx + (dx + 3) * s, ground - 12 * s);
          ctx.stroke();
        }
      } else if (kind === 'jade_tortoise') {
        ctx.fillStyle = utils.rgb(0x3d7b5d);
        utils.fillEllipse(ctx, cx, ground - 14 * s, 15 * s, 12 * s);
        ctx.strokeStyle = utils.rgb(0x9dd66e);
        ctx.lineWidth = 1.2 * s;
        ctx.beginPath();
        ctx.ellipse(cx, ground - 14 * s, 10 * s, 8 * s, 0, 0, Math.PI * 2);
        ctx.moveTo(cx - 9 * s, ground - 14 * s); ctx.lineTo(cx + 9 * s, ground - 14 * s);
        ctx.moveTo(cx, ground - 22 * s); ctx.lineTo(cx, ground - 6 * s);
        ctx.stroke();
        ctx.fillStyle = utils.rgb(0x8dcf72);
        utils.fillCircle(ctx, cx + 15 * s, ground - 11 * s, 5 * s);
        ctx.fillStyle = '#213b2f';
        utils.fillCircle(ctx, cx + 17 * s, ground - 12 * s, 1 * s);
      } else {
        ctx.fillStyle = utils.rgb(0x5e3a9a);
        ctx.beginPath();
        ctx.moveTo(cx, ground - 14 * s);
        ctx.quadraticCurveTo(cx - 19 * s, ground - 28 * s, cx - 16 * s, ground - 4 * s);
        ctx.quadraticCurveTo(cx - 7 * s, ground - 3 * s, cx, ground - 14 * s);
        ctx.closePath();
        ctx.fill();
        ctx.beginPath();
        ctx.moveTo(cx, ground - 14 * s);
        ctx.quadraticCurveTo(cx + 19 * s, ground - 28 * s, cx + 16 * s, ground - 4 * s);
        ctx.quadraticCurveTo(cx + 7 * s, ground - 3 * s, cx, ground - 14 * s);
        ctx.closePath();
        ctx.fill();
        ctx.fillStyle = '#d5a9ff';
        utils.fillCircle(ctx, cx, ground - 12 * s, 2.5 * s);
      }
    },
  };
}

export const PetSpriteDrawers: readonly EntityDrawer[] = [
  createPetDrawer('decor_pet_pet_sprite', 'sprite'),
  createPetDrawer('decor_pet_pet_dragon', 'dragon'),
  createPetDrawer('decor_pet_pet_owl', 'owl'),
  createPetDrawer('decor_pet_pet_cat', 'cat'),
  createPetDrawer('decor_pet_pet_phoenix', 'phoenix'),
  createPetDrawer('decor_pet_pet_storm_wolf', 'storm_wolf'),
  createPetDrawer('decor_pet_pet_jade_tortoise', 'jade_tortoise'),
  createPetDrawer('decor_pet_pet_void_butterfly', 'void_butterfly'),
];

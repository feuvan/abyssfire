import Phaser from 'phaser';

export const TILE_WIDTH = 64;
export const TILE_HEIGHT = 32;
export const MAP_COLS = 80;
export const MAP_ROWS = 80;

export const GAME_WIDTH = 1280;
export const GAME_HEIGHT = 720;
export const DPR = 2;

export const TEXTURE_SCALE = 3;

export const gameConfig: Phaser.Types.Core.GameConfig = {
  type: Phaser.AUTO,
  parent: 'game-container',
  width: GAME_WIDTH * DPR,
  height: GAME_HEIGHT * DPR,
  pixelArt: false,
  antialias: true,
  backgroundColor: '#0f0f1a',
  scale: {
    mode: Phaser.Scale.FIT,
    autoCenter: Phaser.Scale.CENTER_BOTH,
  },
  physics: {
    default: 'arcade',
    arcade: {
      debug: false,
    },
  },
};

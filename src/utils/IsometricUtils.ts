import { TILE_WIDTH, TILE_HEIGHT } from '../config';

export function cartToIso(cartX: number, cartY: number): { x: number; y: number } {
  return {
    x: (cartX - cartY) * (TILE_WIDTH / 2),
    y: (cartX + cartY) * (TILE_HEIGHT / 2),
  };
}

export function isoToCart(isoX: number, isoY: number): { x: number; y: number } {
  return {
    x: (isoX / (TILE_WIDTH / 2) + isoY / (TILE_HEIGHT / 2)) / 2,
    y: (isoY / (TILE_HEIGHT / 2) - isoX / (TILE_WIDTH / 2)) / 2,
  };
}

export function tileToWorld(col: number, row: number): { x: number; y: number } {
  return cartToIso(col, row);
}

export function worldToTile(worldX: number, worldY: number): { col: number; row: number } {
  const cart = isoToCart(worldX, worldY);
  return { col: Math.floor(cart.x), row: Math.floor(cart.y) };
}

export function manhattanDistance(x1: number, y1: number, x2: number, y2: number): number {
  return Math.abs(x1 - x2) + Math.abs(y1 - y2);
}

export function euclideanDistance(x1: number, y1: number, x2: number, y2: number): number {
  return Math.sqrt((x1 - x2) ** 2 + (y1 - y2) ** 2);
}

/** Squared Euclidean distance — avoids Math.sqrt. Use with squared thresholds. */
export function distanceSq(x1: number, y1: number, x2: number, y2: number): number {
  const dx = x1 - x2;
  const dy = y1 - y2;
  return dx * dx + dy * dy;
}

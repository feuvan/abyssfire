import type Phaser from 'phaser';

let gameplayRegistration: Promise<void> | null = null;

export function ensureGameplayScenes(game: Phaser.Game): Promise<void> {
  if (gameplayRegistration) return gameplayRegistration;

  gameplayRegistration = Promise.all([
    import('./ZoneScene'),
    import('./UIScene'),
  ]).then(([{ ZoneScene }, { UIScene }]) => {
    game.scene.add('ZoneScene', ZoneScene, false);
    game.scene.add('UIScene', UIScene, false);
  });

  return gameplayRegistration;
}

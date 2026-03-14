import Phaser from 'phaser';
import { gameConfig } from './config';
import { BootScene } from './scenes/BootScene';
import { MenuScene } from './scenes/MenuScene';
import { ZoneScene } from './scenes/ZoneScene';
import { UIScene } from './scenes/UIScene';
import './systems/AudioSystem';

const config: Phaser.Types.Core.GameConfig = {
  ...gameConfig,
  scene: [BootScene, MenuScene, ZoneScene, UIScene],
};

new Phaser.Game(config);

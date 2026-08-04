import Phaser from 'phaser';
import { gameConfig } from './config';
import { BootScene } from './scenes/BootScene';
import { MenuScene } from './scenes/MenuScene';
import './systems/audio/AudioManager';
import { getLocale } from './i18n';
import { initializeFontManager } from './rendering/FontManager';

const config: Phaser.Types.Core.GameConfig = {
  ...gameConfig,
  scene: [BootScene, MenuScene],
};

void initializeFontManager(getLocale()).finally(() => {
  new Phaser.Game(config);
});

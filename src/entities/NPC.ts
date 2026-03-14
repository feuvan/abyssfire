import Phaser from 'phaser';
import { cartToIso } from '../utils/IsometricUtils';
import type { NPCDefinition } from '../data/types';

export class NPC {
  scene: Phaser.Scene;
  sprite: Phaser.GameObjects.Container;
  definition: NPCDefinition;
  tileCol: number;
  tileRow: number;
  nameLabel: Phaser.GameObjects.Text;

  constructor(scene: Phaser.Scene, definition: NPCDefinition, col: number, row: number) {
    this.scene = scene;
    this.definition = definition;
    this.tileCol = col;
    this.tileRow = row;

    const worldPos = cartToIso(col, row);
    this.sprite = scene.add.container(worldPos.x, worldPos.y);
    this.sprite.setDepth(worldPos.y + 80);

    const color = this.getNPCColor();
    // Shadow
    const shadow = scene.add.ellipse(0, 6, 32, 10, 0x000000, 0.3);
    this.sprite.add(shadow);

    // Body
    const body = scene.add.rectangle(0, -20, 28, 38, color);
    body.setStrokeStyle(1.5, Phaser.Display.Color.IntegerToColor(color).darken(20).color);
    this.sprite.add(body);

    // Head
    const headColor = Phaser.Display.Color.IntegerToColor(color).lighten(15).color;
    const head = scene.add.circle(0, -44, 12, 0xffcc80);
    this.sprite.add(head);

    // Hat/identifier
    const hatColor = this.getHatColor();
    const hat = scene.add.rectangle(0, -54, 20, 8, hatColor);
    hat.setStrokeStyle(1, Phaser.Display.Color.IntegerToColor(hatColor).darken(15).color);
    this.sprite.add(hat);

    // Eyes
    const eye1 = scene.add.circle(-4, -46, 1.5, 0x2c3e50);
    const eye2 = scene.add.circle(4, -46, 1.5, 0x2c3e50);
    this.sprite.add(eye1);
    this.sprite.add(eye2);

    // Exclamation mark for quest NPCs
    if (definition.type === 'quest') {
      const marker = scene.add.text(0, -68, '!', {
        fontSize: '18px',
        color: '#f1c40f',
        fontFamily: '"Cinzel", serif',
        fontStyle: 'bold',
        stroke: '#000000',
        strokeThickness: 3,
      }).setOrigin(0.5);
      this.sprite.add(marker);
      // Bounce animation
      scene.tweens.add({
        targets: marker, y: marker.y - 4, duration: 600,
        yoyo: true, repeat: -1, ease: 'Sine.easeInOut',
      });
    }

    // Name label
    this.nameLabel = scene.add.text(0, 12, definition.name, {
      fontSize: '11px',
      color: '#ffffff',
      fontFamily: '"Noto Sans SC", sans-serif',
      stroke: '#000000',
      strokeThickness: 3,
    }).setOrigin(0.5);
    this.sprite.add(this.nameLabel);
  }

  private getNPCColor(): number {
    switch (this.definition.type) {
      case 'blacksmith': return 0x8b4513;
      case 'merchant': return 0x2e86c1;
      case 'quest': return 0xd4a017;
      case 'stash': return 0x7d3c98;
      default: return 0x95a5a6;
    }
  }

  private getHatColor(): number {
    switch (this.definition.type) {
      case 'blacksmith': return 0x6c3483;
      case 'merchant': return 0x1abc9c;
      case 'quest': return 0xe67e22;
      case 'stash': return 0x5b2c6f;
      default: return 0x7f8c8d;
    }
  }

  isNearPlayer(playerCol: number, playerRow: number, range = 2): boolean {
    const dist = Math.sqrt((this.tileCol - playerCol) ** 2 + (this.tileRow - playerRow) ** 2);
    return dist <= range;
  }
}

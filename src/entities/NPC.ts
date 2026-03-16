import Phaser from 'phaser';
import { TEXTURE_SCALE } from '../config';
import { cartToIso } from '../utils/IsometricUtils';
import { EventBus, GameEvents } from '../utils/EventBus';
import type { NPCDefinition } from '../data/types';

type NPCState = 'working' | 'alert' | 'idle' | 'talking';

export class NPC {
  scene: Phaser.Scene;
  sprite: Phaser.GameObjects.Container;
  definition: NPCDefinition;
  tileCol: number;
  tileRow: number;
  nameLabel: Phaser.GameObjects.Text;
  questMarker: Phaser.GameObjects.Text | null = null;
  private questMarkerTween: Phaser.Tweens.Tween | null = null;
  state: NPCState = 'working';
  private npcSprite: Phaser.GameObjects.Sprite | null = null;
  private spriteKey: string = '';
  private alertTimer: Phaser.Time.TimerEvent | null = null;
  private stashOpen = false;
  private emitter: Phaser.GameObjects.Particles.ParticleEmitter | null = null;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private boundListeners: { event: string; fn: (...args: any[]) => void }[] = [];

  constructor(scene: Phaser.Scene, definition: NPCDefinition, col: number, row: number) {
    this.scene = scene;
    this.definition = definition;
    this.tileCol = col;
    this.tileRow = row;

    const worldPos = cartToIso(col, row);
    this.sprite = scene.add.container(worldPos.x, worldPos.y);
    this.sprite.setDepth(worldPos.y + 80);

    // Use animated sprite sheet: try unique npc_<id> first, fall back to npc_<type>
    const uniqueKey = `npc_${definition.id}`;
    const typeKey = `npc_${definition.type}`;
    this.spriteKey = scene.textures.exists(uniqueKey) ? uniqueKey : typeKey;
    if (scene.textures.exists(this.spriteKey)) {
      this.npcSprite = scene.add.sprite(0, -40, this.spriteKey, 0).setScale(1 / TEXTURE_SCALE);
      this.sprite.add(this.npcSprite);
      const workKey = `${this.spriteKey}_working`;
      if (scene.anims.exists(workKey)) {
        this.npcSprite.play(workKey);
      } else {
        const idleKey = `${this.spriteKey}_idle`;
        if (scene.anims.exists(idleKey)) this.npcSprite.play(idleKey);
      }
    } else {
      this.drawProceduralNPC(scene);
    }

    // Quest marker (created for quest NPCs, updated dynamically)
    if (definition.type === 'quest') {
      this.questMarker = scene.add.text(0, -80, '!', {
        fontSize: '18px',
        color: '#f1c40f',
        fontFamily: '"Cinzel", serif',
        fontStyle: 'bold',
        stroke: '#000000',
        strokeThickness: 3,
      }).setOrigin(0.5);
      this.sprite.add(this.questMarker);
      this.questMarkerTween = scene.tweens.add({
        targets: this.questMarker, y: this.questMarker.y - 4, duration: 600,
        yoyo: true, repeat: -1, ease: 'Sine.easeInOut',
      });
    }

    // Name label
    this.nameLabel = scene.add.text(0, 16, definition.name, {
      fontSize: '11px',
      color: '#ffffff',
      fontFamily: '"Noto Sans SC", sans-serif',
      stroke: '#000000',
      strokeThickness: 3,
    }).setOrigin(0.5);
    this.sprite.add(this.nameLabel);

    // Interactive hit area covering the full NPC visual
    const hitZone = scene.add.rectangle(0, -28, 48, 90, 0xffffff, 0);
    hitZone.setInteractive({ useHandCursor: true });
    this.sprite.add(hitZone);
    this.sprite.setSize(48, 90);
    this.sprite.setInteractive(new Phaser.Geom.Rectangle(-24, -76, 48, 100), Phaser.Geom.Rectangle.Contains);

    this.createAmbientVFX();
    this.setupEventListeners();
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

  private drawProceduralNPC(scene: Phaser.Scene): void {
    const color = this.getNPCColor();
    // Shadow
    const shadow = scene.add.ellipse(0, 6, 32, 10, 0x000000, 0.3);
    this.sprite.add(shadow);

    // Body
    const body = scene.add.rectangle(0, -20, 28, 38, color);
    body.setStrokeStyle(1.5, Phaser.Display.Color.IntegerToColor(color).darken(20).color);
    this.sprite.add(body);

    // Head
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
  }

  private createAmbientVFX(): void {
    let particleKey = '';
    let emitX = 0;
    let emitY = -20;
    let config: Phaser.Types.GameObjects.Particles.ParticleEmitterConfig = {};

    switch (this.definition.type) {
      case 'blacksmith':
        particleKey = 'particle_spark';
        emitY = -10;
        config = {
          tint: 0xff8030,
          frequency: 400,
          lifespan: 500,
          speed: { min: 10, max: 30 },
          angle: { min: -120, max: -60 },
          scale: { start: 0.6, end: 0 },
          alpha: { start: 0.9, end: 0 },
          blendMode: Phaser.BlendModes.ADD,
          quantity: 1,
        };
        break;
      case 'merchant':
        particleKey = 'particle_spark';
        emitY = -30;
        config = {
          tint: 0xffd700,
          frequency: 800,
          lifespan: 300,
          speed: { min: 5, max: 15 },
          angle: { min: -150, max: -30 },
          scale: { start: 0.4, end: 0 },
          alpha: { start: 0.7, end: 0 },
          blendMode: Phaser.BlendModes.ADD,
          quantity: 1,
        };
        break;
      case 'quest':
        particleKey = 'particle_circle';
        emitY = -30;
        config = {
          tint: this.getWispColor(),
          frequency: 600,
          lifespan: 1200,
          speed: { min: 3, max: 10 },
          angle: { min: 0, max: 360 },
          scale: { start: 0.5, end: 0 },
          alpha: { start: 0.6, end: 0 },
          blendMode: Phaser.BlendModes.ADD,
          quantity: 1,
        };
        break;
      case 'stash':
        particleKey = 'particle_circle';
        emitY = -20;
        config = {
          tint: 0x8a5ac0,
          frequency: 1000,
          lifespan: 800,
          speed: { min: 5, max: 12 },
          angle: { min: -130, max: -50 },
          scale: { start: 0.4, end: 0 },
          alpha: { start: 0.5, end: 0 },
          blendMode: Phaser.BlendModes.ADD,
          quantity: 1,
        };
        break;
      default:
        return;
    }

    if (!this.scene.textures.exists(particleKey)) return;
    this.emitter = this.scene.add.particles(emitX, emitY, particleKey, config);
    this.sprite.add(this.emitter);
  }

  private getWispColor(): number {
    const colorMap: Record<string, number> = {
      quest_elder: 0xb8860b,
      quest_scout: 0x4a6a3a,
      forest_hermit: 0x5a8a4a,
      quest_dwarf: 0x8a7a5a,
      quest_nomad: 0xc09a30,
      quest_warden: 0x6a2a3a,
    };
    return colorMap[this.definition.id] ?? 0xb8860b;
  }

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private addListener(event: string, fn: (...args: any[]) => void): void {
    EventBus.on(event, fn);
    this.boundListeners.push({ event, fn });
  }

  private setupEventListeners(): void {
    this.addListener(GameEvents.NPC_INTERACT, (data: { npcId?: string }) => {
      if (data.npcId === this.definition.id) this.setState('talking');
    });
    this.addListener(GameEvents.SHOP_OPEN, (data: { npcId?: string }) => {
      if (data.npcId === this.definition.id) this.setState('talking');
    });
    this.addListener(GameEvents.DIALOGUE_CLOSE, () => {
      if (this.state === 'talking') this.setState('alert');
    });
    this.addListener(GameEvents.SHOP_CLOSE, () => {
      if (this.state === 'talking') this.setState('alert');
    });
    this.addListener(GameEvents.UI_TOGGLE_PANEL, (data: { panel: string; npcId?: string }) => {
      if (data.panel === 'stash' && data.npcId === this.definition.id) {
        this.stashOpen = !this.stashOpen;
        this.setState(this.stashOpen ? 'talking' : 'alert');
      }
    });
  }

  private setState(newState: NPCState): void {
    if (this.state === newState) return;
    this.state = newState;
    if (this.alertTimer) { this.alertTimer.destroy(); this.alertTimer = null; }
    if (this.npcSprite) {
      const animKey = `${this.spriteKey}_${newState}`;
      if (this.scene.anims.exists(animKey)) this.npcSprite.play(animKey);
    }
    if (this.emitter) {
      if (newState === 'working') this.emitter.start();
      else this.emitter.stop();
    }
  }

  update(playerCol: number, playerRow: number): void {
    const near = this.isNearPlayer(playerCol, playerRow, 3);
    switch (this.state) {
      case 'working':
      case 'idle':
        if (near) this.setState('alert');
        break;
      case 'alert':
        if (!near) {
          if (!this.alertTimer) {
            this.alertTimer = this.scene.time.delayedCall(500, () => {
              this.alertTimer = null;
              if (this.state === 'alert') this.setState('working');
            });
          }
        } else if (this.alertTimer) {
          this.alertTimer.destroy();
          this.alertTimer = null;
        }
        break;
      case 'talking':
        break;
    }
  }

  destroy(): void {
    for (const { event, fn } of this.boundListeners) {
      EventBus.off(event, fn);
    }
    this.boundListeners = [];
    if (this.alertTimer) { this.alertTimer.destroy(); this.alertTimer = null; }
    if (this.emitter) { this.emitter.destroy(); this.emitter = null; }
    this.sprite.destroy();
  }

  setQuestMarker(text: string, color: string): void {
    if (!this.questMarker) return;
    if (text) {
      this.questMarker.setText(text).setColor(color).setVisible(true);
    } else {
      this.questMarker.setVisible(false);
    }
  }

  isNearPlayer(playerCol: number, playerRow: number, range = 2): boolean {
    const dist = Math.sqrt((this.tileCol - playerCol) ** 2 + (this.tileRow - playerRow) ** 2);
    return dist <= range;
  }
}

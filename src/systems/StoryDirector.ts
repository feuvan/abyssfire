/**
 * StoryDirector — decides when story beats play and stages them in the world.
 *
 * Owned by ZoneScene. Queues the prologue, chapter cards, triggered
 * cutscenes, boss introductions and the ending, plays them one at a time in
 * StoryScene, and while one is running holds the world still (`cinematic`),
 * hides the HUD and steers the camera.
 */
import Phaser from 'phaser';
import { NPCDefinitions } from '../data/npcs';
import {
  BOSS_INTROS, CHAPTERS, CREDITS, CUTSCENES, EPILOGUE, PROLOGUE, STORY_TRIGGERS,
} from '../data/story/script';
import type { BossIntro, FocusTarget, Speaker } from '../data/story/types';
import { t } from '../i18n';
import { getMonsterName, getNpcName } from '../i18n/gameAccessors';
import { StoryScene, type CutsceneHooks } from '../scenes/StoryScene';
import { EventBus, GameEvents } from '../utils/EventBus';
import { tileToWorld } from '../utils/IsometricUtils';
import { audioManager } from './audio/AudioManager';
import type { StoryProgress } from './StoryProgress';

interface HostMonster { definition: { id: string; name: string; spriteKey: string }; tileCol: number; tileRow: number; sprite: { x: number; y: number }; isAlive(): boolean; hp: number; maxHp: number; nameLabel?: Phaser.GameObjects.Text }
interface HostNpc { definition: { id: string; name: string; type: string }; sprite: { x: number; y: number } }

export interface StoryHost {
  scene: Phaser.Scene;
  story: StoryProgress;
  mapId: string;
  player: () => { sprite: Phaser.GameObjects.Container | Phaser.GameObjects.Sprite; tileCol: number; tileRow: number; classData: { id: string } };
  npcs: () => readonly HostNpc[];
  monsters: () => readonly HostMonster[];
  /** Called when the world should freeze/unfreeze (hide HUD, stop input). */
  setCinematic: (on: boolean) => void;
  save: () => void;
  /** Hand the hero a ley-beast (a trigger's `grantPet`), after its cutscene ends. */
  grantPet?: (petId: string) => void;
}

/** Boss intro plays when the boss is this close (tiles). */
const BOSS_SIGHT = 9;
/** Boss bar shows while the boss is this close. */
const BOSS_BAR_RANGE = 14;
const FINAL_BOSS = 'demon_lord';

export interface BossBarState {
  name: string;
  epithet: string;
  hp: () => { hp: number; maxHp: number } | null;
}

export class StoryDirector {
  /** True while a story beat is on screen: the world holds still. */
  cinematic = false;
  private readonly h: StoryHost;
  private readonly queue: { id: string; run: () => Promise<void> }[] = [];
  private running = false;
  private scanTimer = 0;
  private bossBarFor: string | null = null;
  /** Story bosses whose nameplate already shows their proper name. */
  private readonly named = new WeakSet<object>();
  private readonly onTurnedIn = (d: { questId: string }): void => this.fire('quest_turned_in', d.questId);
  private readonly onAccepted = (d: { questId: string }): void => this.fire('quest_accepted', d.questId);

  constructor(host: StoryHost) {
    this.h = host;
    EventBus.on(GameEvents.QUEST_TURNED_IN, this.onTurnedIn);
    EventBus.on(GameEvents.QUEST_ACCEPTED, this.onAccepted);
  }

  /** True while anything story-related is queued or playing. */
  get busy(): boolean {
    return this.running || this.queue.length > 0;
  }

  /**
   * Zone entry: prologue on a brand-new game, then this zone's chapter card
   * the first time it is visited. Returns true if a chapter card will play
   * (so the scene can skip its plain zone banner).
   */
  start(): boolean {
    if (!this.h.story.has('prologue')) {
      this.enqueue('prologue', () => this.sequence(PROLOGUE.id, 'abyss_rift'));
    }
    const chapter = CHAPTERS.find(c => c.zoneId === this.h.mapId);
    const id = `chapter_${this.h.mapId}`;
    if (chapter && !this.h.story.has(id)) {
      this.enqueue(id, async () => {
        this.setCinematic(true);
        try {
          await (await this.stage()).playChapter(chapter);
        } finally {
          this.setCinematic(false);
        }
      });
      this.fire('zone_entered', this.h.mapId);
      return true;
    }
    this.fire('zone_entered', this.h.mapId);
    return false;
  }

  onMonsterKilled(monsterId: string): void {
    this.fire('monster_killed', monsterId);
    if (monsterId === this.bossBarFor) this.setBossBar(null);
    if (monsterId === FINAL_BOSS && !this.h.story.has('epilogue')) {
      this.enqueue('epilogue', async () => {
        await this.sequence(EPILOGUE.id, 'abyss_rift', 'victory');
        await this.sequence(CREDITS.id, 'abyss_rift', 'victory');
      });
    }
  }

  private fire(on: 'quest_turned_in' | 'quest_accepted' | 'monster_killed' | 'zone_entered', key: string): void {
    for (const trig of STORY_TRIGGERS) {
      if (trig.on !== on) continue;
      const match = trig.on === 'monster_killed' ? trig.monsterId === key
        : trig.on === 'zone_entered' ? trig.zoneId === key
        : trig.questId === key;
      if (match) this.enqueueCutscene(trig.cutscene, on === 'monster_killed' ? 0 : on === 'zone_entered' ? 900 : 650, trig.grantPet);
    }
  }

  // ── Queue ───────────────────────────────────────────────────

  private enqueue(id: string, run: () => Promise<void>): void {
    if (this.h.story.has(id) || this.queue.some(q => q.id === id)) return;
    this.queue.push({ id, run });
    void this.pump();
  }

  private enqueueCutscene(id: string, delayMs = 0, grantPet?: string): void {
    const cs = CUTSCENES[id];
    if (!cs) return;
    this.enqueue(id, async () => {
      if (delayMs > 0) await new Promise<void>(res => this.h.scene.time.delayedCall(delayMs, () => res()));
      try {
        await this.cutscene(id);
      } finally {
        if (grantPet) this.h.grantPet?.(grantPet);
      }
    });
  }

  private async pump(): Promise<void> {
    if (this.running) return;
    this.running = true;
    EventBus.emit(GameEvents.STORY_STATE, { active: true });
    try {
      while (this.queue.length > 0) {
        const next = this.queue.shift()!;
        this.h.story.mark(next.id);
        try {
          await next.run();
        } catch (e) {
          // A broken beat must never soft-lock the game.
          console.error('[story]', next.id, e);
        }
      }
    } finally {
      this.running = false;
      this.h.save();
      EventBus.emit(GameEvents.STORY_STATE, { active: false });
    }
  }

  // ── Staging ─────────────────────────────────────────────────

  /** The StoryScene overlay, created on first use. */
  private async stage(): Promise<StoryScene> {
    const mgr = this.h.scene.scene;
    if (!mgr.get('StoryScene')) mgr.add('StoryScene', StoryScene, true);
    else if (!mgr.isActive('StoryScene')) mgr.launch('StoryScene');
    const story = mgr.get('StoryScene') as StoryScene;
    // Wait for create() to have run.
    for (let i = 0; i < 60 && !story.sys.isActive(); i++) {
      await new Promise<void>(res => this.h.scene.time.delayedCall(16, () => res()));
    }
    mgr.bringToTop('StoryScene');
    return story;
  }

  private setCinematic(on: boolean): void {
    this.cinematic = on;
    this.h.setCinematic(on);
  }

  private async sequence(id: string, musicZone: string, musicState: 'explore' | 'victory' = 'explore'): Promise<void> {
    const seq = id === PROLOGUE.id ? PROLOGUE : id === EPILOGUE.id ? EPILOGUE : CREDITS;
    this.setCinematic(true);
    audioManager.playTrack(musicZone, musicState);
    try {
      await (await this.stage()).playSequence(seq);
    } finally {
      audioManager.playTrack(this.h.mapId, 'explore');
      this.setCinematic(false);
    }
  }

  private async cutscene(id: string): Promise<void> {
    const cs = CUTSCENES[id];
    if (!cs) return;
    const cam = this.h.scene.cameras.main;
    this.setCinematic(true);
    try {
      await (await this.stage()).playCutscene(cs, this.hooks());
    } finally {
      // Glide back to the hero and resume following.
      const p = this.h.player().sprite;
      await new Promise<void>(res => cam.pan(p.x, p.y, 450, 'Sine.easeInOut', true, (_c: Phaser.Cameras.Scene2D.Camera, prog: number) => { if (prog >= 1) res(); }));
      cam.startFollow(p, true, 0.08, 0.08);
      this.setCinematic(false);
    }
  }

  private worldOf(target: FocusTarget): { x: number; y: number } | null {
    if (target === 'player') {
      const p = this.h.player().sprite;
      return { x: p.x, y: p.y };
    }
    if ('npc' in target) {
      const npc = this.h.npcs().find(n => n.definition.id === target.npc);
      return npc ? { x: npc.sprite.x, y: npc.sprite.y - 30 } : null;
    }
    if ('monster' in target) {
      const m = this.nearestMonster(target.monster);
      return m ? { x: m.sprite.x, y: m.sprite.y - 30 } : null;
    }
    return tileToWorld(target.col, target.row);
  }

  private nearestMonster(id: string): HostMonster | null {
    const p = this.h.player();
    let best: HostMonster | null = null;
    let bestD = Infinity;
    for (const m of this.h.monsters()) {
      if (m.definition.id !== id || !m.isAlive()) continue;
      const d = (m.tileCol - p.tileCol) ** 2 + (m.tileRow - p.tileRow) ** 2;
      if (d < bestD) { best = m; bestD = d; }
    }
    return best;
  }

  private hooks(): CutsceneHooks {
    const cam = this.h.scene.cameras.main;
    return {
      focus: (target, ms) => new Promise<void>(res => {
        const w = this.worldOf(target);
        if (!w) { res(); return; }
        cam.stopFollow();
        cam.pan(w.x, w.y, ms, 'Sine.easeInOut', true, (_c: Phaser.Cameras.Scene2D.Camera, prog: number) => { if (prog >= 1) res(); });
      }),
      shake: (intensity, ms) => cam.shake(ms, intensity),
      flash: (color, ms) => cam.flash(ms, (color >> 16) & 255, (color >> 8) & 255, color & 255),
      portrait: (speaker) => this.portraitOf(speaker),
      speakerName: (speaker) => this.nameOf(speaker),
    };
  }

  private portraitOf(speaker: Speaker): { key: string; frame: number } | null {
    const textures = this.h.scene.textures;
    if (speaker === 'villain') return null;
    if (speaker === 'hero') return { key: `player_${this.h.player().classData.id}`, frame: 0 };
    if ('npc' in speaker) {
      const def = NPCDefinitions[speaker.npc];
      const unique = `npc_${def?.spriteId ?? speaker.npc}`;
      const key = textures.exists(unique) ? unique : `npc_${def?.type ?? 'quest'}`;
      return textures.exists(key) ? { key, frame: 12 } : null;
    }
    const m = this.h.monsters().find(x => x.definition.id === speaker.monster);
    return m && textures.exists(m.definition.spriteKey) ? { key: m.definition.spriteKey, frame: 0 } : null;
  }

  private nameOf(speaker: Speaker): string {
    if (speaker === 'villain') return t('story.speaker.villain');
    if (speaker === 'hero') return t('story.speaker.hero');
    if ('npc' in speaker) return getNpcName(speaker.npc, NPCDefinitions[speaker.npc]?.name ?? speaker.npc);
    const intro = BOSS_INTROS.find(b => b.monsterId === speaker.monster);
    if (intro) return t(intro.name);
    const m = this.h.monsters().find(x => x.definition.id === speaker.monster);
    return getMonsterName(speaker.monster, m?.definition.name ?? speaker.monster);
  }

  // ── Bosses ──────────────────────────────────────────────────

  private setBossBar(intro: BossIntro | null, monster?: HostMonster): void {
    this.bossBarFor = intro ? intro.monsterId : null;
    const state: BossBarState | null = intro && monster ? {
      name: t(intro.name),
      epithet: t(intro.epithet),
      hp: () => (monster.isAlive() ? { hp: monster.hp, maxHp: monster.maxHp } : null),
    } : null;
    EventBus.emit(GameEvents.BOSS_BAR, state);
  }

  /** Per frame: boss introductions on sight, and the boss bar while one is near. */
  update(delta: number): void {
    this.scanTimer -= delta;
    if (this.scanTimer > 0) return;
    this.scanTimer = 250;
    const p = this.h.player();
    let near: { intro: BossIntro; m: HostMonster; d: number } | null = null;
    for (const intro of BOSS_INTROS) {
      const m = this.nearestMonster(intro.monsterId);
      if (!m) continue;
      const d = Math.hypot(m.tileCol - p.tileCol, m.tileRow - p.tileRow);
      if (!near || d < near.d) near = { intro, m, d };
    }
    // Story bosses wear their proper name, not the generic monster name.
    if (near && near.d <= BOSS_BAR_RANGE && !this.named.has(near.m) && near.m.nameLabel) {
      this.named.add(near.m);
      near.m.nameLabel.setText(t(near.intro.name)).setColor('#ffcf6a').setAlpha(1);
    }
    if (near && near.d <= BOSS_SIGHT && !this.h.story.has(`boss_${near.intro.monsterId}`)) {
      const intro = near.intro;
      this.enqueue(`boss_${intro.monsterId}`, () => this.cutscene(intro.cutscene));
    }
    if (near && near.d <= BOSS_BAR_RANGE) {
      if (this.bossBarFor !== near.intro.monsterId) this.setBossBar(near.intro, near.m);
    } else if (this.bossBarFor) {
      this.setBossBar(null);
    }
  }

  destroy(): void {
    EventBus.off(GameEvents.QUEST_TURNED_IN, this.onTurnedIn);
    EventBus.off(GameEvents.QUEST_ACCEPTED, this.onAccepted);
    if (this.bossBarFor) EventBus.emit(GameEvents.BOSS_BAR, null);
  }
}

# Character Animation System — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add tween-based idle, walk, attack, cast, hurt, and death animations to player and monster sprites so they feel alive instead of sliding rigid bodies.

**Architecture:** A new `CharacterAnimator` class attaches to entity Containers and drives Phaser tweens based on animation state. Config presets per monster category (humanoid, slime, beast, large, flying, serpentine, demonic) control timing/intensity. Player classes get tweaked humanoid configs.

**Tech Stack:** Phaser 3 tweens, TypeScript. No new textures, no test framework.

**Verification:** After each task, run `npm run dev` and confirm the game loads without errors in the browser console. Visual checks noted per task.

---

### Task 1: Add animCategory to types and monster data

**Files:**
- Modify: `src/data/types.ts:43-60` (MonsterDefinition)
- Modify: `src/data/monsters/emerald_plains.ts`
- Modify: `src/data/monsters/twilight_forest.ts`
- Modify: `src/data/monsters/anvil_mountains.ts`
- Modify: `src/data/monsters/scorching_desert.ts`
- Modify: `src/data/monsters/abyss_rift.ts`

**Step 1: Add type and field to MonsterDefinition**

In `src/data/types.ts`, add the type alias before `MonsterDefinition` and add the optional field:

```ts
export type MonsterAnimCategory = 'humanoid' | 'slime' | 'beast' | 'large' | 'flying' | 'serpentine' | 'demonic';
```

Add to `MonsterDefinition` interface (after `bossSkills?`):

```ts
  animCategory?: MonsterAnimCategory;
```

**Step 2: Add category to each monster data file**

`emerald_plains.ts` — add field to each monster object:
- `slime_green`: `animCategory: 'slime'`
- `goblin`: `animCategory: 'humanoid'`
- `goblin_chief`: `animCategory: 'humanoid'`

`twilight_forest.ts`:
- `skeleton`: `animCategory: 'humanoid'`
- `zombie`: `animCategory: 'humanoid'`
- `werewolf`: `animCategory: 'beast'`
- `werewolf_alpha`: `animCategory: 'beast'`

`anvil_mountains.ts`:
- `gargoyle`: `animCategory: 'flying'`
- `stone_golem`: `animCategory: 'large'`
- `mountain_troll`: `animCategory: 'large'`

`scorching_desert.ts`:
- `fire_elemental`: `animCategory: 'flying'`
- `desert_scorpion`: `animCategory: 'beast'`
- `sandworm`: `animCategory: 'serpentine'`
- `phoenix`: `animCategory: 'flying'`

`abyss_rift.ts`:
- `imp`: `animCategory: 'flying'`
- `lesser_demon`: `animCategory: 'demonic'`
- `succubus`: `animCategory: 'demonic'`
- `demon_lord`: `animCategory: 'large'`

**Step 3: Verify**

Run: `npm run dev`
Expected: Game loads, no TypeScript errors.

**Step 4: Commit**

```bash
git add src/data/types.ts src/data/monsters/
git commit -m "feat(anim): add animCategory to MonsterDefinition and all monster data"
```

---

### Task 2: Create CharacterAnimator — core + idle animation

**Files:**
- Create: `src/systems/CharacterAnimator.ts`

**Step 1: Create the file with types, presets, and core class**

Create `src/systems/CharacterAnimator.ts` with this content:

```ts
import Phaser from 'phaser';
import type { MonsterAnimCategory } from '../data/types';

// ── Types ────────────────────────────────────────────────

export type AnimState = 'idle' | 'walk' | 'attack' | 'cast' | 'hurt' | 'death';

export interface AnimConfig {
  idleBobAmount: number;
  idleBobSpeed: number;
  idleScalePulse: number;
  idleSwayX: number;          // horizontal drift for flying types

  walkBobAmount: number;
  walkBobSpeed: number;
  walkTilt: number;
  walkSquash: number;

  attackLunge: number;
  attackDuration: number;
  attackSquash: number;
  attackWindup: number;
  attackShake: boolean;       // camera micro-shake on impact (large types)

  castLean: number;
  castDuration: number;
  castGlow: boolean;

  hurtKnockback: number;
  hurtDuration: number;
  hurtFlash: boolean;

  deathStyle: 'collapse' | 'dissolve' | 'splat';
  deathDuration: number;
}

// ── Preset Configs ───────────────────────────────────────

const BASE_HUMANOID: AnimConfig = {
  idleBobAmount: 2, idleBobSpeed: 1000, idleScalePulse: 0.02, idleSwayX: 0,
  walkBobAmount: 4, walkBobSpeed: 280, walkTilt: 5, walkSquash: 0.05,
  attackLunge: 12, attackDuration: 300, attackSquash: 0.15, attackWindup: 100, attackShake: false,
  castLean: 6, castDuration: 400, castGlow: false,
  hurtKnockback: 8, hurtDuration: 200, hurtFlash: true,
  deathStyle: 'collapse', deathDuration: 600,
};

const PRESETS: Record<string, AnimConfig> = {
  humanoid: { ...BASE_HUMANOID },
  slime: {
    ...BASE_HUMANOID,
    idleBobAmount: 3, idleBobSpeed: 800, idleScalePulse: 0.06,
    walkBobAmount: 6, walkBobSpeed: 320, walkTilt: 2, walkSquash: 0.12,
    attackLunge: 10, attackDuration: 350, attackSquash: 0.2, attackWindup: 120,
    hurtKnockback: 10, hurtDuration: 250,
    deathStyle: 'splat', deathDuration: 500,
  },
  beast: {
    ...BASE_HUMANOID,
    idleBobAmount: 2, idleBobSpeed: 900, idleScalePulse: 0.03,
    walkBobAmount: 5, walkBobSpeed: 220, walkTilt: 8, walkSquash: 0.06,
    attackLunge: 16, attackDuration: 250, attackSquash: 0.12, attackWindup: 80,
    hurtKnockback: 6, hurtDuration: 150,
    deathStyle: 'collapse', deathDuration: 500,
  },
  large: {
    ...BASE_HUMANOID,
    idleBobAmount: 1.5, idleBobSpeed: 1200, idleScalePulse: 0.02,
    walkBobAmount: 3, walkBobSpeed: 400, walkTilt: 3, walkSquash: 0.04,
    attackLunge: 10, attackDuration: 450, attackSquash: 0.18, attackWindup: 150, attackShake: true,
    hurtKnockback: 4, hurtDuration: 300,
    deathStyle: 'collapse', deathDuration: 800,
  },
  flying: {
    ...BASE_HUMANOID,
    idleBobAmount: 4, idleBobSpeed: 700, idleScalePulse: 0.02, idleSwayX: 3,
    walkBobAmount: 5, walkBobSpeed: 250, walkTilt: 6, walkSquash: 0.03,
    attackLunge: 14, attackDuration: 280, attackSquash: 0.1, attackWindup: 80,
    hurtKnockback: 10, hurtDuration: 180,
    deathStyle: 'collapse', deathDuration: 600,
  },
  serpentine: {
    ...BASE_HUMANOID,
    idleBobAmount: 1, idleBobSpeed: 1100, idleScalePulse: 0.03, idleSwayX: 4,
    walkBobAmount: 2, walkBobSpeed: 350, walkTilt: 2, walkSquash: 0.03,
    attackLunge: 14, attackDuration: 350, attackSquash: 0.1, attackWindup: 120,
    hurtKnockback: 6, hurtDuration: 200,
    deathStyle: 'dissolve', deathDuration: 700,
  },
  demonic: {
    ...BASE_HUMANOID,
    idleBobAmount: 2.5, idleBobSpeed: 600, idleScalePulse: 0.04,
    walkBobAmount: 4, walkBobSpeed: 240, walkTilt: 6, walkSquash: 0.05,
    attackLunge: 14, attackDuration: 280, attackSquash: 0.14, attackWindup: 90,
    hurtKnockback: 8, hurtDuration: 180,
    deathStyle: 'dissolve', deathDuration: 600,
  },
  // Player class variants
  warrior: {
    ...BASE_HUMANOID,
    attackLunge: 16, attackSquash: 0.2, attackWindup: 120,
  },
  mage: {
    ...BASE_HUMANOID,
    attackLunge: 6, attackDuration: 250,
    castLean: 8, castDuration: 450, castGlow: true,
  },
  rogue: {
    ...BASE_HUMANOID,
    attackLunge: 12, attackDuration: 200, attackWindup: 60,
    walkTilt: 7, walkBobSpeed: 240,
  },
};

export function getAnimConfig(category: string): AnimConfig {
  return { ...(PRESETS[category] ?? PRESETS.humanoid) };
}

// ── CharacterAnimator ────────────────────────────────────

export class CharacterAnimator {
  private scene: Phaser.Scene;
  private container: Phaser.GameObjects.Container;
  private config: AnimConfig;
  private state: AnimState = 'idle';
  private prevState: AnimState = 'idle';
  private tweens: Phaser.Tweens.Tween[] = [];
  private baseY: number = 0;       // container's "home" Y offset for bobbing
  private animTime: number = 0;    // accumulated time for sine-based anims
  private dead: boolean = false;

  constructor(scene: Phaser.Scene, container: Phaser.GameObjects.Container, config: AnimConfig) {
    this.scene = scene;
    this.container = container;
    this.config = config;
  }

  getState(): AnimState {
    return this.state;
  }

  // ── State transitions ──────────────────────────────────

  setIdle(): void {
    if (this.dead || this.state === 'idle') return;
    this.cancelTweens();
    this.prevState = this.state;
    this.state = 'idle';
    this.resetTransform(150);
  }

  setWalk(): void {
    if (this.dead || this.state === 'walk') return;
    if (this.state === 'attack' || this.state === 'cast' || this.state === 'hurt') return;
    this.cancelTweens();
    this.prevState = this.state;
    this.state = 'walk';
    this.animTime = 0;
  }

  // Called every frame — drives continuous idle/walk animations via sine math
  update(delta: number): void {
    if (this.dead) return;
    this.animTime += delta;

    if (this.state === 'idle') {
      this.updateIdle();
    } else if (this.state === 'walk') {
      this.updateWalk();
    }
  }

  // ── Idle animation (sine-based, per-frame) ─────────────

  private updateIdle(): void {
    const c = this.config;
    const t = this.animTime;
    const phase = (t / c.idleBobSpeed) * Math.PI * 2;

    // Vertical bob
    const bobY = Math.sin(phase) * c.idleBobAmount;
    this.container.y += bobY - this.baseY;
    this.baseY = bobY;

    // Breathing scale pulse
    const pulse = 1 + Math.sin(phase) * c.idleScalePulse;

    if (c.idleSwayX > 0) {
      // Flying: horizontal drift
      const swayX = Math.sin(phase * 0.7) * c.idleSwayX;
      this.container.x += swayX;
      // We don't track baseX — tiny drift is fine, resets on state change
    }

    // Slime special: inverse scaleX/scaleY wobble
    if (c.deathStyle === 'splat') {
      this.container.setScale(1 + Math.sin(phase) * c.idleScalePulse * 1.5, pulse);
    } else {
      this.container.setScale(1, pulse);
    }
  }

  // Placeholder for walk — implemented in Task 3
  private updateWalk(): void {
    // Will be filled in Task 3
    this.updateIdle();
  }

  // ── Tween management ───────────────────────────────────

  private addTween(config: Phaser.Types.Tweens.TweenBuilderConfig): Phaser.Tweens.Tween {
    const tween = this.scene.tweens.add(config);
    this.tweens.push(tween);
    tween.on('complete', () => {
      const idx = this.tweens.indexOf(tween);
      if (idx !== -1) this.tweens.splice(idx, 1);
    });
    return tween;
  }

  private cancelTweens(): void {
    for (const t of this.tweens) {
      t.stop();
      t.destroy();
    }
    this.tweens = [];
  }

  private resetTransform(duration: number): void {
    this.baseY = 0;
    this.animTime = 0;
    this.addTween({
      targets: this.container,
      scaleX: 1,
      scaleY: 1,
      angle: 0,
      duration,
      ease: 'Power2',
    });
  }

  cleanup(): void {
    this.cancelTweens();
    this.dead = true;
  }
}
```

**Step 2: Verify**

Run: `npm run dev`
Expected: Compiles without errors. No visual change yet (not integrated).

**Step 3: Commit**

```bash
git add src/systems/CharacterAnimator.ts
git commit -m "feat(anim): create CharacterAnimator with core class, presets, and idle animation"
```

---

### Task 3: Add walk animation

**Files:**
- Modify: `src/systems/CharacterAnimator.ts`

**Step 1: Replace the `updateWalk` placeholder**

Replace the `updateWalk` method in `CharacterAnimator`:

```ts
  private updateWalk(): void {
    const c = this.config;
    const t = this.animTime;
    const phase = (t / c.walkBobSpeed) * Math.PI * 2;

    // Vertical bounce (abs sine = always positive = bounce feel)
    const bobY = -Math.abs(Math.sin(phase)) * c.walkBobAmount;
    this.container.y += bobY - this.baseY;
    this.baseY = bobY;

    // Tilt left/right
    this.container.angle = Math.sin(phase) * c.walkTilt;

    // Landing squash: when sine crosses zero (landing moments)
    const landPhase = Math.abs(Math.sin(phase));
    if (landPhase < 0.15) {
      this.container.setScale(1 + c.walkSquash, 1 - c.walkSquash);
    } else {
      // Slime: exaggerated stretch on rise
      if (c.deathStyle === 'splat') {
        const stretch = Math.abs(Math.sin(phase)) * c.walkSquash * 2;
        this.container.setScale(1 - stretch, 1 + stretch);
      } else {
        this.container.setScale(1, 1);
      }
    }
  }
```

**Step 2: Verify**

Run: `npm run dev`
Expected: Compiles. No visual change yet.

**Step 3: Commit**

```bash
git add src/systems/CharacterAnimator.ts
git commit -m "feat(anim): add walk animation with bounce, tilt, and squash"
```

---

### Task 4: Add attack and cast animations

**Files:**
- Modify: `src/systems/CharacterAnimator.ts`

**Step 1: Add playAttack method**

Add these methods to `CharacterAnimator` class, after the `setWalk` method:

```ts
  playAttack(targetX: number, targetY: number): void {
    if (this.dead) return;
    this.cancelTweens();
    this.prevState = this.state;
    this.state = 'attack';
    this.baseY = 0;

    const c = this.config;
    const dx = targetX - this.container.x;
    const dy = targetY - this.container.y;
    const dist = Math.sqrt(dx * dx + dy * dy) || 1;
    const nx = dx / dist;
    const ny = dy / dist;
    const angle = Math.atan2(dy, dx) * (180 / Math.PI);

    // Phase 1: Windup — pull back
    this.addTween({
      targets: this.container,
      x: this.container.x - nx * c.attackLunge * 0.3,
      y: this.container.y - ny * c.attackLunge * 0.3,
      scaleY: 0.92,
      angle: angle * 0.1,
      duration: c.attackWindup,
      ease: 'Power2',
      onComplete: () => {
        if (this.dead) return;
        const returnX = this.container.x + nx * c.attackLunge * 0.3;
        const returnY = this.container.y + ny * c.attackLunge * 0.3;

        // Phase 2: Lunge forward
        this.addTween({
          targets: this.container,
          x: returnX + nx * c.attackLunge,
          y: returnY + ny * c.attackLunge,
          scaleY: 1,
          angle: angle * 0.15,
          duration: 100,
          ease: 'Power3',
          onComplete: () => {
            if (this.dead) return;

            // Phase 3: Impact squash
            this.container.setScale(1 + c.attackSquash, 1 - c.attackSquash);
            if (c.attackShake) {
              this.scene.cameras.main.shake(100, 0.005);
            }

            // Phase 4: Recover
            this.addTween({
              targets: this.container,
              x: returnX,
              y: returnY,
              scaleX: 1,
              scaleY: 1,
              angle: 0,
              duration: c.attackDuration - c.attackWindup - 100 - 50,
              ease: 'Power2',
              onComplete: () => {
                if (!this.dead) {
                  this.state = 'idle';
                  this.animTime = 0;
                  this.baseY = 0;
                }
              },
            });
          },
        });
      },
    });
  }

  playCast(): void {
    if (this.dead) return;
    this.cancelTweens();
    this.prevState = this.state;
    this.state = 'cast';
    this.baseY = 0;

    const c = this.config;
    const chargeDur = c.castDuration * 0.4;
    const releaseDur = c.castDuration * 0.3;
    const settleDur = c.castDuration * 0.3;

    // Phase 1: Charge — lean back, scale up
    this.addTween({
      targets: this.container,
      y: this.container.y + c.castLean,
      scaleX: 1.05,
      scaleY: 1.05,
      duration: chargeDur,
      ease: 'Power2',
      onComplete: () => {
        if (this.dead) return;
        const returnY = this.container.y - c.castLean;

        // Tint flash on cast release
        if (c.castGlow) {
          this.tintFlash(0xaaaaff, 120);
        }

        // Phase 2: Release — snap forward
        this.addTween({
          targets: this.container,
          y: returnY - c.castLean * 0.5,
          scaleX: 1,
          scaleY: 1,
          duration: releaseDur,
          ease: 'Power3',
          onComplete: () => {
            if (this.dead) return;

            // Phase 3: Settle
            this.addTween({
              targets: this.container,
              y: returnY,
              scaleX: 1,
              scaleY: 1,
              angle: 0,
              duration: settleDur,
              ease: 'Power1',
              onComplete: () => {
                if (!this.dead) {
                  this.state = 'idle';
                  this.animTime = 0;
                  this.baseY = 0;
                }
              },
            });
          },
        });
      },
    });
  }
```

**Step 2: Add the tintFlash helper (used by cast and later by hurt)**

Add this private method to `CharacterAnimator`:

```ts
  private tintFlash(color: number, duration: number): void {
    this.container.list.forEach(child => {
      if (child instanceof Phaser.GameObjects.Image) {
        (child as Phaser.GameObjects.Image).setTint(color);
      } else if (child instanceof Phaser.GameObjects.Rectangle && (child as Phaser.GameObjects.Rectangle).visible) {
        (child as Phaser.GameObjects.Rectangle).setFillStyle(color);
      }
    });
    this.scene.time.delayedCall(duration, () => {
      this.container.list.forEach(child => {
        if (child instanceof Phaser.GameObjects.Image) {
          (child as Phaser.GameObjects.Image).clearTint();
        }
      });
      // Note: rectangle fill colors are restored by the entity's own logic or not needed
      // since tintFlash is brief and visual-only
    });
  }
```

**Step 3: Verify**

Run: `npm run dev`
Expected: Compiles. No visual change yet.

**Step 4: Commit**

```bash
git add src/systems/CharacterAnimator.ts
git commit -m "feat(anim): add attack and cast animations with windup, lunge, and cast glow"
```

---

### Task 5: Add hurt and death animations

**Files:**
- Modify: `src/systems/CharacterAnimator.ts`

**Step 1: Add playHurt method**

Add after `playCast`:

```ts
  playHurt(sourceX: number, sourceY: number): void {
    if (this.dead) return;
    // Don't interrupt death
    if (this.state === 'death') return;

    const wasState = this.state;
    this.cancelTweens();
    this.state = 'hurt';
    this.baseY = 0;

    const c = this.config;
    const dx = this.container.x - sourceX;
    const dy = this.container.y - sourceY;
    const dist = Math.sqrt(dx * dx + dy * dy) || 1;
    const nx = dx / dist;
    const ny = dy / dist;

    // Red flash
    if (c.hurtFlash) {
      this.tintFlash(0xff4444, 100);
    }

    // Knockback snap
    const startX = this.container.x;
    const startY = this.container.y;
    this.container.x += nx * c.hurtKnockback;
    this.container.y += ny * c.hurtKnockback;
    this.container.setScale(0.9, 1.1);

    // Recover back
    this.addTween({
      targets: this.container,
      x: startX,
      y: startY,
      scaleX: 1,
      scaleY: 1,
      duration: c.hurtDuration,
      ease: 'Power2',
      onComplete: () => {
        if (!this.dead && this.state === 'hurt') {
          this.state = wasState === 'hurt' ? 'idle' : wasState;
          this.animTime = 0;
          this.baseY = 0;
        }
      },
    });
  }
```

**Step 2: Add playDeath method**

Add after `playHurt`:

```ts
  playDeath(onComplete?: () => void): void {
    if (this.dead) return;
    this.cancelTweens();
    this.state = 'death';
    this.dead = true;

    const c = this.config;

    switch (c.deathStyle) {
      case 'collapse':
        this.addTween({
          targets: this.container,
          angle: 90,
          scaleY: 0.2,
          y: this.container.y + 10,
          alpha: 0,
          duration: c.deathDuration,
          ease: 'Power2',
          onComplete: () => onComplete?.(),
        });
        break;

      case 'splat':
        this.addTween({
          targets: this.container,
          scaleX: 2,
          scaleY: 0.1,
          alpha: 0,
          duration: c.deathDuration,
          ease: 'Power3',
          onComplete: () => onComplete?.(),
        });
        break;

      case 'dissolve': {
        // Rapid alpha flicker then shrink and fade
        let flickerCount = 0;
        const flickerTimer = this.scene.time.addEvent({
          delay: 50,
          repeat: 7,
          callback: () => {
            flickerCount++;
            this.container.alpha = flickerCount % 2 === 0 ? 0.8 : 0.2;
          },
        });
        this.scene.time.delayedCall(400, () => {
          flickerTimer.destroy();
          this.addTween({
            targets: this.container,
            scaleX: 0.3,
            scaleY: 0.3,
            alpha: 0,
            duration: c.deathDuration - 400,
            ease: 'Power2',
            onComplete: () => onComplete?.(),
          });
        });
        break;
      }
    }
  }
```

**Step 3: Verify**

Run: `npm run dev`
Expected: Compiles. No visual change yet.

**Step 4: Commit**

```bash
git add src/systems/CharacterAnimator.ts
git commit -m "feat(anim): add hurt and death animations with knockback, collapse, splat, dissolve"
```

---

### Task 6: Integrate into Player.ts

**Files:**
- Modify: `src/entities/Player.ts`

**Step 1: Import and add animator field**

At top of `Player.ts`, add import:

```ts
import { CharacterAnimator, getAnimConfig } from '../systems/CharacterAnimator';
```

Add field to `Player` class (after `buffs`):

```ts
  animator: CharacterAnimator;
```

**Step 2: Create animator in constructor**

At the end of the constructor (after the skill setup loop, before the closing `}`), add:

```ts
    // Animation
    const animCategory = classData.id; // 'warrior', 'mage', 'rogue' — matches preset keys
    this.animator = new CharacterAnimator(scene, this.sprite, getAnimConfig(animCategory));
```

**Step 3: Call animator.update in update()**

In the `update` method, add at the end (after hp regen):

```ts
    // Animate
    this.animator.update(delta);
```

**Step 4: Set walk/idle state in updateMovement**

In `updateMovement`:

At the start of the method, after `if (this.path.length === 0)` block, change it to:

```ts
    if (this.path.length === 0) {
      this.isMoving = false;
      this.animator.setIdle();
      return;
    }
    this.animator.setWalk();
```

**Step 5: Add convenience methods for combat animations**

Add these methods to `Player` class (after `respawnAtCamp`):

```ts
  playAttack(targetX: number, targetY: number): void {
    this.animator.playAttack(targetX, targetY);
  }

  playCast(): void {
    this.animator.playCast();
  }

  playHurt(sourceX: number, sourceY: number): void {
    this.animator.playHurt(sourceX, sourceY);
  }

  playDeath(): void {
    this.animator.playDeath();
  }
```

**Step 6: Call playDeath in die()**

Modify the `die()` method — add `this.playDeath();` as the first line:

```ts
  die(): void {
    this.playDeath();
    EventBus.emit(GameEvents.PLAYER_DIED, {});
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: '你已死亡，将在营地复活...',
      type: 'system',
    });
  }
```

**Step 7: Reset animator on respawn**

In `respawnAtCamp`, add after `this.attackTarget = null;`:

```ts
    this.animator.cleanup();
    this.animator = new CharacterAnimator(this.scene, this.sprite, getAnimConfig(this.classData.id));
```

And reset the container's visual state before creating new animator:

```ts
    this.sprite.setAlpha(1);
    this.sprite.setScale(1);
    this.sprite.setAngle(0);
```

So `respawnAtCamp` becomes:

```ts
  respawnAtCamp(col: number, row: number): void {
    this.hp = this.maxHp;
    this.mana = this.maxMana;
    this.moveTo(col, row);
    this.path = [];
    this.isMoving = false;
    this.attackTarget = null;
    this.sprite.setAlpha(1);
    this.sprite.setScale(1);
    this.sprite.setAngle(0);
    this.animator.cleanup();
    this.animator = new CharacterAnimator(this.scene, this.sprite, getAnimConfig(this.classData.id));
    EventBus.emit(GameEvents.PLAYER_HEALTH_CHANGED, { hp: this.hp, maxHp: this.maxHp });
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: '你在营地复活了。',
      type: 'system',
    });
  }
```

**Step 8: Verify**

Run: `npm run dev`
Expected: Player sprite now bobs gently when idle, bounces/tilts when walking. No attack/cast/hurt animations yet (ZoneScene not wired).

**Step 9: Commit**

```bash
git add src/entities/Player.ts
git commit -m "feat(anim): integrate CharacterAnimator into Player with idle and walk"
```

---

### Task 7: Integrate into Monster.ts

**Files:**
- Modify: `src/entities/Monster.ts`

**Step 1: Import and add animator field**

Add import at top:

```ts
import { CharacterAnimator, getAnimConfig } from '../systems/CharacterAnimator';
```

Add field (after `patrolTimer`):

```ts
  animator: CharacterAnimator;
```

**Step 2: Create animator in constructor**

At the end of the constructor (after the elite crown block, before the closing `}`), add:

```ts
    // Animation
    const animCategory = definition.animCategory ?? 'humanoid';
    this.animator = new CharacterAnimator(scene, this.sprite, getAnimConfig(animCategory));
```

**Step 3: Set animation states in update()**

In the `update` method, add animator.update call and state-driven transitions. After the existing switch statement (before the closing `}` of update), add:

```ts
    // Drive animation states
    if (this.state === 'idle') {
      this.animator.setIdle();
    } else if (this.state === 'patrol' || this.state === 'chase') {
      this.animator.setWalk();
    }
    this.animator.update(delta);
```

**Step 4: Replace takeDamage tint logic with animator**

Replace the entire `takeDamage` method with:

```ts
  takeDamage(amount: number, sourceX?: number, sourceY?: number): void {
    this.hp = Math.max(0, this.hp - amount);
    this.updateHpBar();

    // Animated hurt reaction
    this.animator.playHurt(sourceX ?? this.sprite.x, sourceY ?? (this.sprite.y - 40));

    if (this.hp <= 0) {
      this.die();
    }
  }
```

Note: `takeDamage` now accepts optional sourceX/sourceY for directional knockback. When not provided, it defaults to a position above (generic recoil).

**Step 5: Replace die() with animated death**

Replace the `die()` method:

```ts
  die(): void {
    this.state = 'dead';
    this.animator.playDeath(() => {
      this.sprite.destroy();
    });
  }
```

This replaces the old `setAlpha(0.3)` + `delayedCall(1500)` approach. The sprite is destroyed when the death animation finishes.

**Step 6: Add playAttack convenience method**

Add after `die()`:

```ts
  playAttack(targetX: number, targetY: number): void {
    this.animator.playAttack(targetX, targetY);
  }
```

**Step 7: Verify**

Run: `npm run dev`
Expected: Monsters now bob when idle, bounce when chasing. Slimes have jelly-wobble. Flying types drift. Hit monsters get knockback recoil instead of old tint flash. Dead monsters collapse/splat/dissolve instead of fading.

**Step 8: Commit**

```bash
git add src/entities/Monster.ts
git commit -m "feat(anim): integrate CharacterAnimator into Monster with idle, walk, hurt, and death"
```

---

### Task 8: Wire combat animations in ZoneScene

**Files:**
- Modify: `src/scenes/ZoneScene.ts`

**Step 1: Add player attack animation to auto-attack**

In `handleCombat`, find the player auto-attack section (around line 533-539). After `this.player.lastAttackTime = time;` and before the damage calculation, add the attack animation trigger. The section should become:

```ts
        this.player.lastAttackTime = time;
        this.player.playAttack(target.sprite.x, target.sprite.y);
        const result = this.combatSystem.calculateDamage(this.player.toCombatEntity(), target.toCombatEntity());
```

**Step 2: Pass player position to monster takeDamage**

Change all `target.takeDamage(result.damage)` calls in ZoneScene to pass the player's position for directional knockback. There are 3 locations:

1. In `handleCombat` auto-attack (around line 536):
```ts
        target.takeDamage(result.damage, this.player.sprite.x, this.player.sprite.y);
```

2. In `tryUseSkill` single-target (around line 497):
```ts
      target.takeDamage(result.damage, this.player.sprite.x, this.player.sprite.y);
```

3. In `tryUseSkill` AOE loop (around line 483):
```ts
        t.takeDamage(result.damage, this.player.sprite.x, this.player.sprite.y);
```

**Step 3: Add player cast animation to skill use**

In `tryUseSkill`, after `this.player.useSkill(skillId, time);` (around line 466), add:

```ts
    // Trigger cast or attack animation based on skill type
    if (skill.buff || skill.aoe || skill.range > 2) {
      this.player.playCast();
    } else {
      const target = this.findNearestAliveMonster();
      if (target) {
        this.player.playAttack(target.sprite.x, target.sprite.y);
      } else {
        this.player.playCast();
      }
    }
```

**Step 4: Add monster attack animation**

In `handleCombat`, in the monster attack loop (around line 510-511), after `monster.lastAttackTime = time;`, add:

```ts
        monster.playAttack(this.player.sprite.x, this.player.sprite.y);
```

**Step 5: Add player hurt animation when hit**

In `handleCombat`, after the player takes damage from a monster (around line 518, after `this.player.hp = Math.max(0, ...)`), add:

```ts
          this.player.playHurt(monster.sprite.x, monster.sprite.y);
```

**Step 6: Verify**

Run: `npm run dev`
Expected: Full animation loop working:
- Player bobs idle, bounces when walking
- Player lunges toward target on auto-attack
- Player does cast animation when using skills
- Monsters bob/walk/attack with their category-appropriate animations
- Hit entities recoil away from damage source with red flash
- Dead monsters collapse/splat/dissolve
- Dead player collapses, respawns normally

**Step 7: Commit**

```bash
git add src/scenes/ZoneScene.ts
git commit -m "feat(anim): wire combat animations in ZoneScene for attack, cast, hurt triggers"
```

---

### Task 9: Final verification and tuning

**Files:**
- Possibly tweak: `src/systems/CharacterAnimator.ts` (preset values)

**Step 1: Full playthrough test**

Run: `npm run dev` and test all of these:

1. **Idle**: Stand still — player bobs gently, monsters nearby bob too
2. **Walk**: Click to move — player bounces and tilts, returns to idle
3. **Slime**: Visit Emerald Plains — slimes have jelly wobble idle and squash-stretch walk
4. **Auto-attack**: Get close to a monster — player lunges, target recoils
5. **Skill cast**: Press 1-6 — player does cast lean-back animation
6. **Monster attack**: Let a monster hit you — monster lunges, player recoils with red flash
7. **Monster death**: Kill each type — collapse (humanoid), splat (slime), dissolve (demonic)
8. **Player death**: Die intentionally — collapse animation, then respawn works normally
9. **Zone transition**: Walk to exit portal — animations reset cleanly in new zone
10. **Flying types**: Visit Anvil Mountains for gargoyles — hover/drift idle
11. **Large types**: Find a golem — slow heavy bob, camera shake on attack

**Step 2: Tune if needed**

If any animation feels too subtle or too aggressive, adjust the numeric values in the `PRESETS` object in `CharacterAnimator.ts`. Common tuning:
- `idleBobAmount`: increase if idle feels too static
- `attackLunge`: decrease if entities teleport too far
- `walkTilt`: decrease if walking looks drunk
- `hurtKnockback`: decrease if hurt slides too far

**Step 3: Final commit**

```bash
git add -A
git commit -m "feat(anim): tune animation parameters after playtesting"
```

Only if changes were made. If everything looks good, skip this commit.

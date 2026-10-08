/**
 * story.json — the story script (src/data/story/script.ts) verbatim, the story-scene moods and the
 * StoryDirector / StoryScene timings. All text is i18n keys (src/i18n/locales/story.ts).
 */
import { PROLOGUE, CHAPTERS, CUTSCENES, BOSS_INTROS, STORY_TRIGGERS, EPILOGUE, CREDITS } from '../../../../../src/data/story/script';
import * as StorySceneMod from '../../../../../src/scenes/StoryScene';
import * as StoryDirectorMod from '../../../../../src/systems/StoryDirector';
import { assert, assertSource, plain, type TableResult } from '../util';

const exp = <T>(mod: unknown, name: string): T => {
  const v = (mod as Record<string, unknown>)[`__expose_${name}`];
  assert(v !== undefined, `exposed binding ${name} missing`);
  return v as T;
};

export function exportStory(): TableResult[] {
  for (const trig of STORY_TRIGGERS) assert(CUTSCENES[trig.cutscene], `trigger → unknown cutscene ${trig.cutscene}`);
  for (const bi of BOSS_INTROS) assert(!bi.cutscene || CUTSCENES[bi.cutscene], `boss intro → unknown cutscene ${bi.cutscene}`);
  assertSource('src/systems/StoryDirector.ts', "on === 'monster_killed' ? 0 : on === 'zone_entered' ? 900 : 650");
  assertSource('src/scenes/StoryScene.ts', 'const BAR_H = 78;', 'const speed = 42; // px/s', 'await this.waitOrInput(3800);',
    'private async typeInto(tx: Phaser.GameObjects.Text, full: string, cps = 38): Promise<void> {',
    "case 'focus': await hooks.focus(step.target, step.ms ?? 900); break;");
  assertSource('src/systems/StoryDirector.ts', "cam.pan(p.x, p.y, 450, 'Sine.easeInOut'");
  // Phase lengths of every StoryScene step (quests-story-ch1.md 8.5), pinned to the exact tween / wait lines so the
  // core step player and UE's overlay use the same numbers.
  assertSource('src/scenes/StoryScene.ts',
    // sequence
    'await this.tweenTo(bg, { alpha: 1 }, 900);', 'await this.tweenTo(bg, { alpha: 0 }, 400);', 'await this.tweenTo(bg, { alpha: 1 }, 500);',
    "const r = await Promise.race([this.tweenTo(p, { alpha: 1 }, 900).then(() => 'done' as const), this.waitInput()]);",
    'await this.tweenTo(parts, { alpha: 0 }, 450);', 'await this.tweenTo([bg], { alpha: 0 }, 700);',
    // chapter card
    'await this.tweenTo(shade, { fillAlpha: 0.72 }, 600);', 'await this.tweenTo(num, { alpha: 1 }, 500);',
    "await this.tweenTo(title, { alpha: 1, scale: 1 }, 900, 'Cubic.easeOut');", 'await this.tweenTo(sub, { alpha: 1 }, 600);',
    'await this.tweenTo(body, { alpha: 1 }, 900);', 'await this.tweenTo([num, title, sub, body, lineL, lineR, glow, halo], { alpha: 0 }, 900);',
    'await this.tweenTo(shade, { fillAlpha: 0 }, 700);',
    // narrate
    'await this.tweenTo(shade, { fillAlpha: 0.55 }, 400);', 'await this.tweenTo(tx, { alpha: 1 }, 700);',
    'await this.tweenTo([tx], { alpha: 0 }, 350);', 'await this.tweenTo(shade, { fillAlpha: 0 }, 300);',
    // say
    'await this.tweenTo(objs, { alpha: 1 }, 220);', 'await this.tweenTo(objs, { alpha: 0 }, 160);',
    // whisper
    'await this.tweenTo(veil, { alpha: 1 }, 600);', 'await this.tweenTo([main], { alpha: 1 }, 700);',
    'await this.tweenTo([main, ghostA, ghostB, veil], { alpha: 0 }, 600);',
    // title
    'await this.tweenTo(band, { fillAlpha: 0.6 }, 200);', "await this.tweenTo(name, { alpha: 1, scale: 1 }, 380, 'Back.easeOut');",
    'await this.tweenTo(epi, { alpha: 1 }, 400);', 'await this.waitOrInput(2200);',
    'await this.tweenTo([band, name, epi, slash, halo], { alpha: 0 }, 450);');
  const moods = exp<Record<string, unknown>>(StorySceneMod, 'MOODS');
  return [{
    file: 'story.json',
    source: ['src/data/story/script.ts', 'src/data/story/types.ts', 'src/scenes/StoryScene.ts', 'src/systems/StoryDirector.ts'],
    data: {
      prologue: plain(PROLOGUE),
      chapters: plain(CHAPTERS),
      cutscenes: plain(CUTSCENES),
      bossIntros: plain(BOSS_INTROS),
      triggers: plain(STORY_TRIGGERS),
      epilogue: plain(EPILOGUE),
      credits: plain(CREDITS),
      moods: plain(moods),
      timing: {
        beatDelayMs: { quest_turned_in: 650, quest_accepted: 650, zone_entered: 900, monster_killed: 0 },
        bossSightTiles: exp<number>(StoryDirectorMod, 'BOSS_SIGHT'),
        bossBarRangeTiles: exp<number>(StoryDirectorMod, 'BOSS_BAR_RANGE'),
        finalBoss: exp<string>(StoryDirectorMod, 'FINAL_BOSS'),
        cameraPanMs: 450, letterboxPx: 78, letterboxMs: 450, typewriterCps: 38, chapterHoldMs: 3800,
        focusDefaultMs: 900, creditsPxPerSec: 42,
        // Phase lengths (ms) per step kind (8.5). `in` = reveal before the step waits (input steps) or holds, `out` =
        // the fade after it. Chapter card: intro 600+500+900+600+900 = 3500, hold chapterHoldMs, outro 900+700 = 1600
        // (uncut ≈ 8.9 s). say: the typewriter adds text length / typewriterCps after sayInMs.
        phases: {
          sequence: { backdropInMs: 900, moodOutMs: 400, moodInMs: 500, slidePartInMs: 900, slideOutMs: 450, backdropOutMs: 700 },
          chapter: { introMs: 600 + 500 + 900 + 600 + 900, outroMs: 900 + 700 },
          narrate: { inMs: 400 + 700, outMs: 350 + 300 },
          say: { inMs: 220, outMs: 160 },
          whisper: { inMs: 600 + 700, outMs: 600 },
          title: { inMs: 200 + 380 + 400, holdMs: 2200, outMs: 450 },
        },
        input: 'two-tap rule: the first input during a reveal (typewriter, staggered slide parts, fades) completes it ' +
          'on the UE side; UE sends StoryAdvance only for the input that advances (a waiting step, or a title / chapter ' +
          'hold). StorySkip resolves every remaining wait of the beat except the 450 ms camera return.',
        clock: 'presentation (real time); the sim clock is frozen while a beat plays (DECISIONS S2)',
        markSeen: 'when the beat finishes; a skipped beat counts as finished (DECISIONS Q7)',
      },
    },
    counts: { chapters: CHAPTERS.length, cutscenes: Object.keys(CUTSCENES).length, bossIntros: BOSS_INTROS.length, triggers: STORY_TRIGGERS.length },
  }];
}

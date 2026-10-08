/**
 * Story script schema.
 *
 * All player-facing text is an i18n key (see src/i18n/locales/story.ts);
 * scripts only reference keys so zh-CN and en stay in lockstep.
 */

/** Who is talking in a cutscene line. NPC ids resolve name + portrait from npcs.ts. */
export type Speaker =
  | { npc: string }
  /** The antagonist's voice (purple, distorted). */
  | 'villain'
  /** The player character (silent protagonist: use sparingly, for short reactions). */
  | 'hero'
  /** A monster speaking (boss taunts): name/portrait from the monster sheet. */
  | { monster: string };

/** Where the camera looks. */
export type FocusTarget =
  | 'player'
  | { npc: string }
  | { monster: string }
  | { col: number; row: number };

export type CutsceneStep =
  /** Centered narration over the darkened, letterboxed scene. */
  | { kind: 'narrate'; text: string }
  /** A dialogue line in the bottom box, with speaker name and portrait. */
  | { kind: 'say'; speaker: Speaker; text: string }
  /** The antagonist speaks into the hero's mind: screen dims violet, text trembles. */
  | { kind: 'whisper'; text: string }
  /** Pan the camera to a target (ms defaults to 900). */
  | { kind: 'focus'; target: FocusTarget; ms?: number }
  /** Big title card: boss name + epithet, or a place/relic name. */
  | { kind: 'title'; title: string; subtitle: string }
  /** Screen effects for punctuation. */
  | { kind: 'shake'; intensity?: number; ms?: number }
  | { kind: 'flash'; color?: number; ms?: number }
  /** Pause (ms). */
  | { kind: 'wait'; ms: number };

export interface Cutscene {
  id: string;
  steps: CutsceneStep[];
}

/** A full-screen story sequence (prologue, chapter cards, epilogue, credits). */
export interface StorySlide {
  /** Optional small heading above the text (e.g. '第一章'). */
  heading?: string;
  /** Large title line. */
  title?: string;
  /** Body paragraph(s); '\n' separates lines. */
  text?: string;
  /** Visual mood of the backdrop. */
  mood?: 'embers' | 'dawn' | 'night' | 'forge' | 'sand' | 'abyss' | 'light';
}

export interface StorySequence {
  id: string;
  slides: StorySlide[];
  /** Scroll the slides as credits instead of paging. */
  credits?: boolean;
}

/** Chapter card shown the first time a zone is entered. */
export interface Chapter {
  zoneId: string;
  /** i18n keys */
  number: string;
  title: string;
  subtitle: string;
  /** Two or three lines of narration. */
  text: string;
  mood: NonNullable<StorySlide['mood']>;
}

/** Boss introduction: plays the first time the boss comes into view. */
export interface BossIntro {
  monsterId: string;
  /** Proper name + epithet shown in the title card and the boss bar. */
  name: string;
  epithet: string;
  /** Cutscene played before the fight (should contain a 'title' step and a taunt). */
  cutscene: string;
}

/**
 * When a story beat plays. `grantPet` hands the hero a ley-beast once the
 * cutscene ends (the story's pet beats: 月牙, 赫莉娅之烬).
 */
export type StoryTrigger =
  | { on: 'quest_turned_in'; questId: string; cutscene: string; grantPet?: string }
  | { on: 'quest_accepted'; questId: string; cutscene: string; grantPet?: string }
  | { on: 'monster_killed'; monsterId: string; cutscene: string; grantPet?: string }
  /** First time the hero enters a zone (after its chapter card, if any). */
  | { on: 'zone_entered'; zoneId: string; cutscene: string; grantPet?: string };

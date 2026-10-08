# Port Spec — Audio (BGM, SFX, music engine)

Area owner: audio. Web source of truth: branch `claude/unreal-rebuild`, TypeScript under `src/systems/audio/`
(`AudioManager.ts`, `SFXEngine.ts`, `MusicEngine.ts`, `ScorePlayer.ts`, `Composer.ts`, `AudioLoader.ts`, `types.ts`) plus the
call sites in `src/scenes/*` and `src/systems/StoryDirector.ts`, and the CC0 recordings in `public/assets/audio/bgm/`.
Target: portable C++20 core (`AbyssCore`, no UE types, no exceptions/RTTI) + thin UE5 module, and an **offline Python
audio renderer** (this spec recommends it, §9) that produces the WAV assets UE imports.

This document says **what the web game does today**, precisely enough to rebuild every sound without reading the
TypeScript, and then how to port it. Markers:

* **QUIRK** — web behaviour that is a bug or an artefact; each has a recommendation (KEEP or FIX).
* **NEW** — not in the web game; added for the 3D port (boss music, story stingers, concurrency, …). Everything NEW is
  data-driven and optional for parity.
* **render-only / web-only** — Phaser / browser plumbing with a stated UE equivalent (§12).

Citations are `path:line` at the time of writing. Composer.ts line numbers are its own (not the combined listing).

---

## 0. Summary of the decisions

| Topic | Decision |
|---|---|
| How SFX are produced | Every SFX is **synthesised on the fly** in Web Audio from oscillators, noise, biquads, a waveshaper and ADSR ramps (`SFXEngine.ts`). There are **no SFX files**. 33 cue ids exist: 26 are reachable, 2 are wired only to events that are never emitted (`monster_death`, `panel_close`), 5 are never triggered (§3.4, §4.1). |
| How BGM is produced | Two sources. (a) **CC0 MP3 recordings** for each zone × {explore, combat, victory} (`public/assets/audio/bgm/*.mp3`, credits in `CREDITS.md`). If an MP3 exists for the current `zone_state` it plays **instead of** the procedural score (`MusicEngine.ts:577-581`). (b) A **procedural score** (`ScorePlayer` + `Composer`) used when no MP3 exists: the **title menu**, the **Ember Tower**, **Abyss Labyrinth floors**, and briefly while an MP3 is still downloading. All music passes through a per-zone effects chain (low cut, low shelf, reverb, compressor, limiter). |
| Music states | `explore` / `combat` / `victory`, binary combat flag from `ZoneScene.updateCombatState` (1500 ms off-delay), crossfades 2.0 s (zone) / 1.5 s (state), victory auto-returns after 3000 ms. No intensity layers beyond that; the procedural combat score is faster, denser and adds percussion. |
| Port strategy (§9) | **Offline-render everything to WAV with a Python/numpy port of the synth (recommended)**, import as plain `USoundWave`s, and drive them from a small UE subsystem. Not runtime MetaSounds. The CC0 MP3s are decoded, gapless-trimmed and baked through the same effects chain. The music *rules* (state machine, event → cue mapping) live in the C++ core. |
| Chapter 1 assets (§10) | 6 music assets (menu, plains explore, plains combat, plains victory, boss `boss_ch1` NEW, prologue), 4 story stingers NEW, 33 SFX (16 with 4 noise variants). |

---

## 1. Web architecture

### 1.1 Modules

| Module | Role | Source |
|---|---|---|
| `AudioManager` (singleton `audioManager`) | Owns the `AudioContext`, the two master gains (`musicGain`, `sfxGain`), unlock lifecycle, settings persistence, **all EventBus → sound wiring**, BGM file loading. Imported for side effects in `main.ts:5`. | `src/systems/audio/AudioManager.ts:38-453` |
| `SFXEngine` | `play(ctx, dest, type)`: plays buffer `sfx_<type>` if loaded (never, no SFX files ship), else the procedural recipe. | `SFXEngine.ts:26-84` |
| `MusicEngine` | Zone/state crossfader, effects chain, MP3 buffer layer, procedural score layer, victory stinger. Holds `ZONE_THEMES`, `ZONE_SCORES`. | `MusicEngine.ts:26-675` |
| `ScorePlayer` | Real-time bar-by-bar performer of a `ScoreSpec` (pad, bass, arpeggio, lead phrases, bells, percussion). | `ScorePlayer.ts:45-302` |
| `Composer` | Pure music theory: modes, MIDI↔Hz, diatonic chords, voicing, xorshift RNG, phrase writer. | `Composer.ts:9-177` |
| `AudioLoader` | Map `key → AudioBuffer`, de-duplicated fetch + decode, abort/release. **web-only**. | `AudioLoader.ts:17-106` |
| BGM manifest | Vite build-time define `__BGM_MANIFEST__ = {"<zone>_<state>": "assets/audio/bgm/<file>.mp3"}` from the directory listing. **web-only**. | `vite.config.ts:5-17`, `src/vite-env.d.ts:3` |

### 1.2 Signal graph and gain staging

```
SFX recipe voices ──────────────────────────────────────────────► sfxGain (user SFX vol, default 0.30) ─► destination
                                                                     (no compressor, no limiter on SFX)

Score voices ─► trim 0.4 (SCORE_TRIM) ─┐
MP3 BufferSource (loop) ───────────────┼─► LayerSet.masterGain (crossfade 0→1) ─► MusicEngine.masterGain (1.0)
Victory stinger voices ────────────────┘
   ─► [effects chain of the CURRENT theme]
        inputGain(1) ─► HPF 75 Hz (Q 0.7 dB) ─► lowshelf 180 Hz −4 dB ─┬─► dryGain (1−mix) ─────────────┐
                                                                       └─► preDelay ─► convolver ─► wetGain (mix) ─┤
                                                                                                   compressor ◄─┘
                                                                       compressor ─► limiter ─► musicGain (user BGM vol, default 0.15) ─► destination
```

* Sources: `AudioManager.ts:221-227` (master gains), `MusicEngine.ts:241-323` (chain), `:504-512` (set gain), `:591-595` (trim).
* User volume is applied **after** the chain (`buildEffectsChain(ctx, destination = musicGain)`), so the chain sees the
  pre-volume signal. The port bakes the chain into the assets and applies user volume at runtime (§9.6) — identical.
* `destination` is stereo; mono voices up-mix as L = R = x (Web Audio "speakers" rule, no −3 dB). All SFX are centre-panned
  mono; all music is stereo (the reverb IR is decorrelated L/R).

### 1.3 Gain constants (exact)

| Constant | Value | Source |
|---|---|---|
| `DEFAULT_SETTINGS` | `bgmVolume 0.15, sfxVolume 0.3, bgmMuted false, sfxMuted false` | `AudioManager.ts:27-32` |
| `SCORE_TRIM` | 0.4 (≈ −8 dB) | `MusicEngine.ts:117` |
| SFX layer peaks | 0.04 … 0.5 (see recipes) | `SFXEngine.ts` |
| Effects defaults | compressor −18 dB thr, 6 dB knee, ratio 4, attack 0.003 s, release 0.25 s; limiter −1 dB thr, knee 0, ratio 20, attack 0.001 s, release 0.1 s | `MusicEngine.ts:334-344`, `:298-303` |

---

## 2. Web Audio semantics the renderer must reproduce

The offline renderer (§9.3) is a small emulation of the Web Audio node graph. These are the exact rules the recipes
depend on.

### 2.1 AudioParam automation
* Events: `setValueAtTime(v,T)` (step), `linearRampToValueAtTime(v,T)`, `exponentialRampToValueAtTime(v,T)`. A ramp runs
  from the **previous event's (time, value)** to `(T, v)`; after the last event the value holds.
* Linear: `v(t) = v0 + (v1 − v0)·(t − t0)/(T − t0)`. Exponential: `v(t) = v0·(v1/v0)^((t − t0)/(T − t0))` (both values
  > 0 in every recipe here).
* Before the first event the param has its default (`gain` 1, `frequency` 440 for oscillators / 350 for biquads,
  `detune` 0, `Q` 1). Every recipe either sets values at the voice's start or starts the source at/after the first event,
  so defaults never sound.
* Params are **a-rate**: the computed value = automation value + the sum of any audio connected into the param
  (used for LFO → `frequency` and vibrato → `detune`).
* `setTargetAtTime` is used only in `MusicEngine.stop` (`:428`), which nothing calls. Not needed.

### 2.2 Oscillators
* Types `sine | square | sawtooth | triangle`, band-limited (Web Audio builds them as `PeriodicWave`s; Fourier sine
  coefficients `b_n`): sine `b1 = 1`; square `b_n = (2/(nπ))·(1 − (−1)^n)`; sawtooth `b_n = (−1)^(n+1)·2/(nπ)`;
  triangle `b_n = 8·sin(nπ/2)/(nπ)²`; all `a_n = 0` → every wave starts at phase 0 rising. The table is normalised to a
  peak of 1. Partials above Nyquist are dropped for the current fundamental.
* Effective frequency `f·2^(detune/1200)`; phase accumulates per sample, **negative instantaneous frequency is allowed**
  (lightning recipe dips below 0 Hz — the phase runs backwards).
* `start(T)` / `stop(T)` are hard gates (sample-accurate).

### 2.3 BiquadFilterNode (RBJ cookbook as specified by Web Audio)
`w0 = 2π·f/sr`, `cosw = cos w0`.
* **lowpass / highpass: Q is in dB** — `α = sin w0 / (2·10^(Q/20))`. Default Q = 1 → linear 1.122.
  LP `b = [(1−cosw)/2, 1−cosw, (1−cosw)/2]`, HP `b = [(1+cosw)/2, −(1+cosw), (1+cosw)/2]`, `a = [1+α, −2cosw, 1−α]`.
* **bandpass: Q is linear** — `α = sin w0/(2Q)`, `b = [α, 0, −α]`, `a = [1+α, −2cosw, 1−α]` (0 dB peak). Default Q = 1.
* **lowshelf** (S = 1, Q unused): `A = 10^(G/40)`, `α = sin w0/2·√2`;
  `b0 = A[(A+1) − (A−1)cosw + 2√A α]`, `b1 = 2A[(A−1) − (A+1)cosw]`, `b2 = A[(A+1) − (A−1)cosw − 2√A α]`,
  `a0 = (A+1) + (A−1)cosw + 2√A α`, `a1 = −2[(A−1) + (A+1)cosw]`, `a2 = (A+1) + (A−1)cosw − 2√A α`.
* Time-varying cutoffs: recompute coefficients at least every 128 frames (the Web Audio render quantum); the difference
  from per-sample recomputation is inaudible.

### 2.4 WaveShaperNode
Curve `Float32Array(256)`, `curve[i] = F(i/128 − 1)` (so x spans −1 … 0.9921875). Lookup: `v = (N−1)/2·(x+1)`,
`k = floor(v)`, linear interpolation between `curve[k]` and `curve[k+1]`; `x ≤ −1 → curve[0]`, `x ≥ 1 → curve[N−1]`.
`oversample = 'none'`. The shaper always sits **before** the envelope gain, so its input is the raw ±1 oscillator.

### 2.5 Noise
White noise `Math.random()*2 − 1` (uniform [−1, 1)), a **fresh buffer per burst** (non-deterministic). Port: seeded
uniform noise; every noise-bearing SFX gets 4 rendered variants (§9.5).

### 2.6 ConvolverNode (reverb)
* IR (`MusicEngine.ts:216-238`): 2 channels, `L = ceil(sr·(decay + 0.2))` frames, per channel independently
  `h[i] = (rand·2 − 1) · exp(−(i/sr)·3/decay) · (1 − 0.3·i/L)` (−26 dB at t = decay). A new random IR is generated on
  every music transition (inaudible).
* `normalize = true` (default) — the IR is scaled by
  `scale = 0.00125 / max(√(Σ_ch Σ_i h² / (channels·L)), 0.000125) · (44100 / sr)` (Web Audio spec normalisation;
  ×0.5 only for 4-channel IRs). Then mono input → stereo out (L = x∗h_L, R = x∗h_R); stereo input → L∗h_L, R∗h_R.

### 2.7 DynamicsCompressorNode (Chromium behaviour)
The web chain uses two compressors. The browser implementation (Chromium `DynamicsCompressorKernel`, BSD-licensed WebKit
code) matters for level:
* Static curve: linear below threshold; soft knee from `threshold` to `threshold + knee` with the exponential knee
  `y = T + (1 − e^(−k(x − T)))/k` (linear domain, `k` solved so the slope at the knee end is `1/ratio`); above the knee
  a straight line of slope `1/ratio` in dB.
* **Automatic make-up gain** `= (1 / curve(1.0))^0.6`. For the music compressor (−18/6/4) `k ≈ 13.43`, curve(0 dBFS) =
  −11.59 dB → **make-up +6.95 dB**; static I/O incl. make-up: −30 → −23.05, −18 → −11.05, −12 → −7.63, −6 → −6.13,
  0 → −4.63 dBFS. Limiter (−1/0/20): make-up **+0.57 dB**, i.e. signals below −1.6 dBFS pass +0.57 dB louder.
* Detector = max(|L|, |R|), 6 ms look-ahead (pre-delay), attack/release as given, Chromium's adaptive release curve.
* Port: transliterate the Chromium kernel into the renderer (keeps the licence notice) — this is the only part where a
  generic compressor would change the level by several dB. Acceptance: §9.8.

### 2.8 Helper envelopes used everywhere (`SFXEngine.ts:124-205`)
* **`ADSR(a, d, s, r, peak) @T`** on a gain param: `set 0@T`, `lin peak@T+a`, `exp max(s·peak, 0.001)@T+a+d`,
  `exp 0.001@T+a+d+r`, then holds 0.001 until the source stops. There is **no sustain hold**: release starts as soon as
  decay ends. `s` is a ratio of `peak`. If the source's `stop` comes before `T+a+d+r` the tail is truncated (happens for
  the echo voices of `loot_legendary`, `quest_complete`, `levelup`) — the renderer reproduces this automatically by
  honouring `stop`.
* **`NB(dur, f, type, T, peak = 0.25)`** — noise burst: fresh noise buffer `ceil(sr·dur)` frames, started at `T`,
  stopped at `T+dur` → Biquad(`type`, frequency `f` set at `T`, default Q) → gain `ADSR(0.005, 0.3·dur, 0.3, 0.5·dur, peak) @T`.
* **`TONE(f, wave, dur, g, T)`** — oscillator at constant `f`, gain **constant `g`** (no envelope → hard on/off click),
  `T … T+dur`. QUIRK (clicks): KEEP by default; the renderer may add an optional 2 ms de-click ramp (flag, default off).
* **`CHIME(freqs, step, level)`** (`:990-1005`) — per note `i`: `T = i·step`; sine `f`; gain `set 0@T`, `lin level@T+0.006`,
  `exp 0.001@T+0.32`; span `T … T+0.34`.

---

## 3. Event → SFX wiring (when each sound fires)

All wiring is in `AudioManager.setupEventListeners` (`AudioManager.ts:263-390`) plus direct `audioManager.playSFX` calls.
`playSFX` is a no-op while SFX are muted or the context is not yet unlocked (sounds are **dropped, not queued**,
`:112-118`).

### 3.1 EventBus listeners

| Event (payload) | Sound | Rule | Source |
|---|---|---|---|
| `COMBAT_DAMAGE {isDodged, isCrit, …}` | `miss` / `crit` / `hit` | `isDodged → miss`, else `isCrit → crit`, else `hit` | `:265-273` |
| `PLAYER_LEVEL_UP` | `levelup` | always | `:276-278` |
| `PLAYER_DIED` | `player_death` | always | `:280-282` |
| `DODGE_STARTED` | `dodge` | always | `:284-286` |
| `SPIRIT_RESONANCE_STARTED` | `resonance` | always | `:288-290` |
| `MONSTER_DIED` | `monster_death` | always — **but the event is never emitted** (only `CombatSystem.applyDamage`, `CombatSystem.ts:383-410`, emits it and nothing calls it). QUIRK → **FIX**: emit on every kill (`ZoneScene.onMonsterKilled`, `:3789`); combat-feel spec §9 already recommends it. | `:293-295` |
| `ITEM_PICKED {item.quality}` | `loot_magic` (magic), `loot_rare` (rare), `loot_legendary` (legendary **and set**), else `loot_common` | switch | `:298-314` |
| `SKILL_USED {damageType}` | `skill_fire` (fire), `skill_ice` (ice), `skill_lightning` (lightning), `skill_buff` (arcane, poison), else `skill_melee` (physical / undefined) | fires at **cast commit** (`Player.useSkill`, `Player.ts:386`, called from `ZoneScene.ts:2491`), i.e. before the contact beat | `:317-336` |
| `ZONE_ENTERED {mapId}` | music (§5) | — | `:339-344` |
| `COMBAT_STATE_CHANGED {inCombat}` | music (§5) | — | `:346-349` |
| `QUEST_COMPLETED` | `quest_complete` | all objectives done | `:352-354` |
| `QUEST_ACCEPTED` | `npc_interact` | — | `:356-358` |
| `QUEST_PROGRESS {current, required, targetId, completesQuest}` | `quest_objective` / `quest_progress` / none | `completesQuest → none` (the fanfare covers it); `current ≥ required → quest_objective`; else if `targetId` starts with `mat_` or `clue_` → `quest_progress`; else none (e.g. single kills are silent) | `:360-364` |
| `QUEST_TURNED_IN` | `quest_complete` | — | `:366-368` |
| `NPC_INTERACT` | `npc_interact` | emitted for quest-type NPCs (`ZoneScene.ts:4159`) | `:371-373` |
| `SHOP_OPEN` | `panel_open` | (`ZoneScene.ts:3394`, `:4150`) | `:375-377` |
| `INVENTORY_OPEN` / `INVENTORY_CLOSE` | `panel_open` / `panel_close` | **never emitted** → `panel_close` is unreachable | `:379-385` |
| `UI_TOGGLE_PANEL` | `click` | every hotkey toggle (open **and** close): I, M, K, H, C, J, O, P, U, V (`ZoneScene.ts:2170-2202`), stash NPC (`:4175`), pet HUD button (`UIScene.ts:4971`) | `:387-389` |

### 3.2 COMBAT_DAMAGE emitters (decide `hit`/`crit`/`miss`)

| Emitter | Payload | Sound | Source |
|---|---|---|---|
| Hero basic attack lands (only if `isCrit || damage > 0`), at the contact beat | target monster | `hit` / `crit` | `ZoneScene.ts:2881` |
| Monster swing during the hero's dodge i-frames | `isDodged: true` | `miss` | `:2978` |
| Monster hit on hero (stat-dodge `result.isDodged` emits **nothing** → silent) | hero | `hit` / `crit` | `:3014` |
| Elite "extra fire" on hero | hero | `hit` | `:3033` |
| DoT tick on hero (burn/poison/bleed), each tick | hero | `hit` | `:6118` |
| Abyss labyrinth volatile-corpse burst on hero (later milestone) | hero | `hit` | `:1208` |

DoT ticks on **monsters** and skill damage on monsters emit no `COMBAT_DAMAGE` → silent (the skill cast sound is the
only cue). `HitFeedback` weights (`tick/light/normal/heavy/crit/kill`) are **not** used by audio (`hit_heavy` is unused).

### 3.3 Direct calls

| Call site | Sound | When |
|---|---|---|
| `ZoneScene.ts:1097` | `quest_objective` | labyrinth floor seal opens (later milestone) |
| `ZoneScene.ts:1280` | `resonance` | soul echo reclaimed (death penalty) |
| `ZoneScene.ts:5875` | `zone_transition` | town portal (R) completes, 1500 ms after the cast, with the camera flash. Normal exits/zone changes play **no** sound. |
| `UIScene.ts:978` (toggleInventory), `:4579` (toggleCompanion), `:5042` (toggleAchievement) | `click` | panel toggle — QUIRK: the I/U/V hotkeys also emit `UI_TOGGLE_PANEL` → **two identical clicks at once (+6 dB)**. FIX: one click per toggle. |
| `UIScene.ts:1168` (openShop, not on page refresh), `:1221` (shop tab), `:1257` (buy), `:1295` (buyback), `:1367`/`:1378`/`:1386` (sell), `:1442` (sell confirm), `:1838` (open stash), `:1910`/`:1913`/`:1922`/`:1925` (stash sort / move), `:3044` (openDialogue), `:3104` (openQuestCard), `:3258` (reward choice), `:3439` (openDialogueTree), `:4387` (socket panel), `:5299` (mini-boss dialogue), `:5370` (lore text) | `click` | UI actions. Shop open therefore plays `panel_open` + `click` together; talking to a quest NPC plays `npc_interact` + `click`. KEEP. |
| `UIScene.ts:1459`, `:1656` | `error` | forge: non-equipment put on the anvil / forge action refused |
| `UIScene.ts:1464` | `equip` (placed) or `click` (taken off) | forge anvil slot toggle — **`equip` never plays when equipping gear** |
| `UIScene.ts:1661` | `anvil` | forge action succeeded |
| `UIScene.ts:1668` | `loot_rare` if the item is now rare, else `loot_magic` | 140 ms after a successful forge **upgrade** |

MenuScene buttons, StoryScene (cutscenes, chapter cards, boss title cards), the boss bar, weather, footsteps, monsters'
aggro/attacks and pets make **no sound** in the web game.

### 3.4 Unused cues
`hit_heavy`, `block`, `player_hurt`, `skill_heal`, `potion` are defined but never triggered; `panel_close` and
`monster_death` are wired to events that never fire. Port: render all of them (cheap), wire `monster_death` (FIX), keep the
others unwired unless an open question (§14) decides otherwise.

---

## 4. SFX catalogue (synthesis recipes)

### 4.1 Index

| id | Reachable in web? | Length (s) | Noise layers (→ 4 variants) | Recipe |
|---|---|---|---|---|
| `hit` | yes | 0.20 | yes | §4.3.1 |
| `hit_heavy` | no | 0.35 | yes | §4.3.2 |
| `crit` | yes | 0.35 | yes | §4.3.3 |
| `miss` | yes | 0.20 | no | §4.3.4 |
| `block` | no | 0.20 | yes | §4.3.5 |
| `player_hurt` | no | 0.25 | yes | §4.3.6 |
| `monster_death` | dead event (FIX) | 0.60 | yes | §4.3.7 |
| `player_death` | yes | 1.20 | no | §4.3.8 |
| `dodge` | yes | 0.22 | yes | §4.3.9 |
| `resonance` | yes | 0.61 | yes | §4.3.10 |
| `skill_melee` | yes | 0.35 | yes | §4.4.1 |
| `skill_fire` | yes | 0.50 | yes | §4.4.2 |
| `skill_ice` | yes | 0.45 | yes | §4.4.3 |
| `skill_lightning` | yes | 0.40 | yes | §4.4.4 |
| `skill_heal` | no | 0.55 | yes | §4.4.5 |
| `skill_buff` | yes | 0.50 | no | §4.4.6 |
| `loot_common` | yes | 0.20 | no | §4.5.1 |
| `loot_magic` | yes | 0.30 | no | §4.5.2 |
| `loot_rare` | yes | 0.52 | yes | §4.5.3 |
| `loot_legendary` | yes | 0.79 | no | §4.5.4 |
| `equip` | yes (forge only) | 0.10 | yes | §4.5.5 |
| `potion` | no | 0.25 | no | §4.5.6 |
| `click` | yes | 0.05 | no | §4.6.1 |
| `panel_open` | yes | 0.10 | no | §4.6.2 |
| `panel_close` | no (dead event) | 0.10 | no | §4.6.3 |
| `error` | yes | 0.20 | no | §4.6.4 |
| `zone_transition` | yes | 1.02 | no | §4.7.1 |
| `quest_complete` | yes | 0.75 | no | §4.7.2 |
| `quest_progress` | yes | 0.41 | no | §4.7.3 |
| `quest_objective` | yes | 0.52 | no | §4.7.3 |
| `levelup` | yes | 1.02 | no | §4.7.4 |
| `npc_interact` | yes | 0.19 | no | §4.7.5 |
| `anvil` | yes (forge) | 1.00 | yes | §4.7.6 |

Length = last `stop` time; render length = length + 10 ms of silence.

### 4.2 Notation
Times are seconds from the trigger. `f: set 250@0 exp 80@0.2` = frequency automation. `LP/HP(f, QdB)` and `BP(f, Q)` per
§2.3 (omitted Q = default 1). `shape tanh(3x)` = §2.4 curve `F(x) = tanh(3x)`. `ADSR(...)@T`, `NB(...)`, `TONE(...)`,
`CHIME(...)` per §2.8. `span a–b` = oscillator start/stop. Gains are linear.

### 4.3 Combat

#### 4.3.1 `hit` — metallic slash (`SFXEngine.ts:246-285`)
| # | Source | Processing | Envelope | Span |
|---|---|---|---|---|
| 1 | saw `f: set 250@0 exp 80@0.2` | BP(1200, 2) | ADSR(0.002, 0.04, 0.15, 0.12, 0.25) | 0–0.2 |
| 2 | sine `f: set 100@0 exp 40@0.08` | — | ADSR(0.001, 0.03, 0.1, 0.06, 0.2) | 0–0.1 |
| 3 | square `f: set 2200@0 exp 1800@0.2` | BP(3500, 8) | ADSR(0.001, 0.02, 0.08, 0.15, 0.06) | 0–0.2 |
| 4 | NB(0.08, 2500, highpass, 0, 0.2) | | | |

#### 4.3.2 `hit_heavy` (`:288-330`)
| # | Source | Processing | Envelope | Span |
|---|---|---|---|---|
| 1 | saw `f: set 150@0 exp 40@0.35` | LP(`set 600@0 exp 200@0.35`, QdB 3) | ADSR(0.002, 0.06, 0.25, 0.2, 0.3) | 0–0.35 |
| 2 | square `f: set 120@0 exp 35@0.35` | shape tanh(3x) | ADSR(0.002, 0.08, 0.2, 0.2, 0.12) | 0–0.35 |
| 3 | sine `f: set 80@0 exp 25@0.12` | — | ADSR(0.001, 0.04, 0.1, 0.08, 0.25) | 0–0.15 |
| 4 | NB(0.1, 800, lowpass, 0, 0.25) + NB(0.06, 3000, highpass, 0, 0.12) | | | |

#### 4.3.3 `crit` (`:333-389`)
| # | Source | Processing | Envelope | Span |
|---|---|---|---|---|
| 1 | square `f: set 700@0 exp 100@0.15` | shape tanh(4x) → BP(1800, 1.5) | ADSR(0.001, 0.04, 0.15, 0.2, 0.3) | 0–0.35 |
| 2 | saw `f: set 1100@0 exp 250@0.12` | HP(800) | ADSR(0.001, 0.03, 0.1, 0.15, 0.18) | 0–0.2 |
| 3 | sine `f: set 120@0 exp 30@0.1` | — | ADSR(0.001, 0.03, 0.08, 0.07, 0.25) | 0–0.12 |
| 4 | square `f: set 3200@0 exp 2400@0.35` | BP(4000, 12) | ADSR(0.001, 0.02, 0.06, 0.25, 0.04) | 0–0.35 |
| 5 | NB(0.06, 4000, highpass, 0, 0.3) + NB(0.15, 1500, bandpass, 0.05, 0.08) | | | |

#### 4.3.4 `miss` — whoosh (`:392-404`)
sine `f: set 400@0 exp 150@0.2`; ADSR(0.01, 0.05, 0.3, 0.1, 0.1); span 0–0.2.

#### 4.3.5 `block` (`:407-436`)
| # | Source | Processing | Envelope | Span |
|---|---|---|---|---|
| 1 | square `f: set 350@0 exp 250@0.2` | BP(2000, 8) | ADSR(0.001, 0.03, 0.12, 0.12, 0.25) | 0–0.2 |
| 2 | triangle `f: set 3200@0 exp 2800@0.2` | BP(3500, 12) | ADSR(0.001, 0.02, 0.08, 0.15, 0.06) | 0–0.2 |
| 3 | NB(0.04, 4000, highpass, 0, 0.25) | | | |

#### 4.3.6 `player_hurt` (`:439-470`)
| # | Source | Processing | Envelope | Span |
|---|---|---|---|---|
| 1 | sine `f: set 180@0 exp 50@0.25` | LP(300, QdB 3) | ADSR(0.002, 0.05, 0.2, 0.15, 0.3) | 0–0.25 |
| 2 | saw `f: set 250@0 exp 100@0.25` | shape tanh(2x) | ADSR(0.002, 0.04, 0.15, 0.15, 0.1) | 0–0.25 |
| 3 | NB(0.08, 400, lowpass, 0, 0.22) | | | |

#### 4.3.7 `monster_death` — descending groan (`:473-488`)
saw `f: set 200@0 exp 40@0.6`, ADSR(0.01, 0.1, 0.4, 0.4, 0.25), span 0–0.6; + NB(0.6, 600, lowpass, 0, 0.18).

#### 4.3.8 `player_death` (`:491-517`)
saw `f: set 300@0 exp 40@1.2`, ADSR(0.02, 0.15, 0.5, 0.8, 0.25), span 0–1.2; + saw `f: set 285@0 exp 38@1.2`,
ADSR(0.02, 0.2, 0.4, 0.9, 0.18), span 0–1.2.

#### 4.3.9 `dodge` — airy whoosh (`:212-226`)
triangle `f: set 180@0 exp 920@0.154 exp 420@0.22`, ADSR(0.004, 0.035, 0.18, 0.12, 0.16), span 0–0.22;
+ NB(0.176, 2600, highpass, 0, 0.12).

#### 4.3.10 `resonance` — spirit flare (`:229-243`)
TONE(330, sine, 0.5, 0.09, 0) + TONE(495, sine, 0.5, 0.075, 0.055) + TONE(660, sine, 0.5, 0.06, 0.11);
+ NB(0.28, 3200, bandpass, 0, 0.08).

### 4.4 Skills

#### 4.4.1 `skill_melee` — sword swing (`:524-556`)
| # | Source | Processing | Envelope | Span |
|---|---|---|---|---|
| 1 | triangle `f: set 250@0 lin 700@0.06 exp 150@0.35` | BP(`set 800@0 exp 400@0.35`, 2) | ADSR(0.003, 0.05, 0.2, 0.2, 0.28) | 0–0.35 |
| 2 | saw `f: set 1500@0.05 exp 400@0.15` | BP(2500, 5) | ADSR(0.001, 0.03, 0.1, 0.12, 0.12)@0.05 | 0.05–0.2 |
| 3 | NB(0.15, 1200, bandpass, 0, 0.2) + NB(0.04, 3000, highpass, 0.05, 0.15) | | | |

#### 4.4.2 `skill_fire` (`:559-596`)
| # | Source | Processing | Envelope | Span |
|---|---|---|---|---|
| 1 | saw `f: set 120@0 lin 300@0.1 exp 80@0.5` | LP(`set 300@0 lin 1200@0.15 exp 400@0.5`, QdB 4) | ADSR(0.005, 0.08, 0.3, 0.3, 0.22) | 0–0.5 |
| 2 | saw `f: 400` + LFO square 30 Hz × 200 Hz into `frequency` (span 0–0.5) | BP(2000, 3) | ADSR(0.01, 0.1, 0.2, 0.3, 0.1) | 0–0.5 |
| 3 | NB(0.4, 800, bandpass, 0, 0.25) + NB(0.15, 4000, highpass, 0.05, 0.1) | | | |

#### 4.4.3 `skill_ice` (`:599-634`)
| # | Source | Processing | Envelope | Span |
|---|---|---|---|---|
| 1a/1b | sine, for `f0 ∈ {900, 907}`: `f: set f0@0 lin 1.4·f0@0.12 exp 0.7·f0@0.45` | HP(600) | ADSR(0.005, 0.06, 0.3, 0.25, 0.15) each | 0–0.45 |
| 2 | triangle `f: set 4500@0 exp 3000@0.45` | BP(5000, 15) | ADSR(0.002, 0.04, 0.15, 0.3, 0.04) | 0–0.45 |
| 3 | NB(0.08, 5000, highpass, 0.02, 0.18) + NB(0.27, 3000, highpass, 0.05, 0.08) | | | |

#### 4.4.4 `skill_lightning` (`:637-685`)
| # | Source | Processing | Envelope | Span |
|---|---|---|---|---|
| 1 | square `f: set 1200@0 exp 150@0.4` + LFO saw 60 Hz × 500 Hz into `frequency` (span 0–0.4; frequency goes negative near the end) | shape `sign(x)·√|x|` | ADSR(0.001, 0.03, 0.2, 0.3, 0.2) | 0–0.4 |
| 2 | saw `f: set 3000@0 exp 500@0.1` | HP(2000) | ADSR(0.001, 0.02, 0.05, 0.08, 0.12) | 0–0.15 |
| 3 | sine `f: set 80@0.02 exp 30@0.15` | — | ADSR(0.002, 0.04, 0.1, 0.1, 0.2)@0.02 | 0.02–0.18 |
| 4 | NB(0.08, 6000, highpass, 0, 0.3) + NB(0.15, 2000, bandpass, 0.05, 0.12) | | | |

#### 4.4.5 `skill_heal` (`:688-715`)
For `i, f` in `[400, 500, 600, 800]`, `T = 0.1·i`, span `T–T+0.25`: sine `f` → LP(3f, QdB 2) → ADSR(0.01, 0.04, 0.5,
0.15, 0.15)@T; plus sine `1.005·f` → ADSR(0.015, 0.04, 0.4, 0.15, 0.1)@T. Plus NB(0.3, 4000, highpass, 0.15, 0.04).

#### 4.4.6 `skill_buff` — chord chime (`:718-741`)
For `i, f` in `[523, 659, 784]`, span 0–0.5: triangle `f` → LP(3000, QdB 1) → ADSR(0.01 + 0.01·i, 0.06, 0.4, 0.3, 0.12);
plus sine `2f` → ADSR(0.02, 0.05, 0.3, 0.3, 0.04).

### 4.5 Loot / items

| id | Recipe | Source |
|---|---|---|
| `loot_common` (4.5.1) | sine `f: set 800@0 lin 1200@0.07 exp 600@0.2`, ADSR(0.005, 0.04, 0.3, 0.12, 0.2), span 0–0.2 | `:748-761` |
| `loot_magic` (4.5.2) | sine `f: set 800@0 exp 1200@0.3` ADSR(0.005, 0.05, 0.35, 0.2, 0.18) + triangle `f: set 1000@0 exp 1400@0.3` ADSR(0.005, 0.06, 0.3, 0.2, 0.13); span 0–0.3 | `:764-788` |
| `loot_rare` (4.5.3) | for `i, f` in `[900, 1100, 1350]`: `T = 0.06·i`; sine `f: set f@T exp 1.3f@T+0.24` ADSR(0.008, 0.06, 0.35, 0.25, 0.15)@T, span `T–T+0.4`; + NB(0.4, 5000, highpass, 0, 0.1) | `:791-810` |
| `loot_legendary` (4.5.4) | for `i, f` in `[523, 659, 784, 1047]`: `T = 0.12·i`; sine `f` ADSR(0.008, 0.05, 0.5, 0.18, 0.22)@T span `T–T+0.25`; echo sine `f` ADSR(0.008, 0.05, 0.3, 0.25, 0.1)@T+0.18 span `T+0.18–T+0.43` (truncated tail) | `:813-841` |
| `equip` (4.5.5) | square 400 Hz ADSR(0.002, 0.02, 0.1, 0.07, 0.3) span 0–0.1; + NB(0.1, 3000, highpass, 0, 0.15) | `:844-858` |
| `potion` (4.5.6) | sine 300 Hz + LFO sine 12 Hz × 25 Hz into `frequency`; ADSR(0.01, 0.06, 0.5, 0.15, 0.2); span 0–0.25 | `:861-885` |

### 4.6 UI

| id | Recipe | Source |
|---|---|---|
| `click` | sine 600, ADSR(0.002, 0.01, 0.1, 0.035, 0.12), span 0–0.05 | `:892-903` |
| `panel_open` | sine `f: set 400@0 exp 600@0.1`, ADSR(0.005, 0.02, 0.4, 0.06, 0.15), span 0–0.1 | `:906-918` |
| `panel_close` | sine `f: set 600@0 exp 400@0.1`, same ADSR | `:921-933` |
| `error` | square 200 and square 150, each ADSR(0.003, 0.04, 0.5, 0.13, 0.15), span 0–0.2 | `:936-950` |

### 4.7 World / progression

| id | Recipe | Source |
|---|---|---|
| `zone_transition` (4.7.1) | sine `f: set 200@0 exp 800@0.35 exp 400@0.8` ADSR(0.02, 0.1, 0.5, 0.5, 0.25) span 0–0.8; echo sine `f: set 200@0.22 exp 800@0.57 exp 400@0.86` ADSR(0.02, 0.12, 0.35, 0.45, 0.12)@0.22 span 0.22–1.02 | `:957-986` |
| `quest_complete` (4.7.2) | for `i, f` in `[392, 523, 659, 784]`: `T = 0.11·i`; triangle `f` ADSR(0.008, 0.05, 0.5, 0.18, 0.2)@T span `T–T+0.25`; echo triangle `f` ADSR(0.008, 0.05, 0.3, 0.22, 0.1)@T+0.17 span `T+0.17–T+0.42` | `:1007-1036` |
| `quest_progress` (4.7.3) | CHIME([880, 1175], 0.07, 0.35) | `:78` |
| `quest_objective` (4.7.3) | CHIME([659, 880, 1319], 0.09, 0.5) — the loudest cue (0.5 per partial) | `:79` |
| `levelup` (4.7.4) | for `i, f` in `[261, 329, 392, 523, 659]`: `T = 0.13·i`; sine `f` ADSR(0.01, 0.05, 0.55, 0.2, 0.2)@T span `T–T+0.3`; echo sine `f` ADSR(0.01, 0.06, 0.35, 0.28, 0.1)@T+0.2 span `T+0.2–T+0.5` | `:1039-1068` |
| `npc_interact` (4.7.5) | for `i, f` in `[523, 659]`: `T = 0.04·i`; sine `f` ADSR(0.008, 0.03, 0.4, 0.1, 0.15)@T span `T–T+0.15` | `:1071-1085` |
| `anvil` (4.7.6) | NB(0.05, 3200, bandpass, 0, 0.3); thump sine `f: set 180@0 exp 70@0.12` ADSR(0.002, 0.05, 0.2, 0.1, 0.28) span 0–0.2; partials `[f, peak, dur]` ∈ `[[880,.12,.9],[2376,.07,.7],[4550,.04,.45],[1321,.05,.6]]`: sine `f: set f@0 exp 0.995f@dur` ADSR(0.001, 0.04, 0.5, dur, peak) span `0–dur+0.1` | `:90-114` |

---

## 5. Music director (state machine)

### 5.1 State held by `AudioManager` + `MusicEngine`

| Field | Meaning | Source |
|---|---|---|
| `desiredZone` (init `'menu'`), `desiredState` (init `explore`) | what should play | `AudioManager.ts:54-55` |
| `MusicEngine.currentZone`, `currentState` | what is playing | `MusicEngine.ts:354-355` |
| `activeSet` | the LayerSet fading in / playing | `:359` |
| `transitionTimeouts` | pending destroy timers of fading sets | `:361` |
| `victoryTimer` | 3000 ms auto-return | `:363` |
| `zoneMusicRequestId` | latest buffer-load request (stale loads are ignored) | `AudioManager.ts:51`, `:423`, `:442` |

### 5.2 Inputs

| Input | Effect | Source |
|---|---|---|
| `ZONE_ENTERED {mapId}` (MenuScene emits `'menu'` at `MenuScene.ts:427`, `:1029`; ZoneScene emits the map id on every zone create at `ZoneScene.ts:694`) | `desiredZone = mapId; desiredState = explore; applyDesiredMusic(force=false)` | `AudioManager.ts:339-344` |
| `COMBAT_STATE_CHANGED {inCombat}` | `desiredState = inCombat ? combat : explore; applyDesiredMusic(false)` | `:346-349` |
| `playTrack(zone, state)` — MenuScene create (`'menu','explore'`, `MenuScene.ts:73`), jukebox (`:1014`), story sequences (`StoryDirector.ts:191`, `:195`) | `desiredZone/State = …; unlock().then(applyDesiredMusic(force=true))` | `:167-171` |

`applyDesiredMusic(force)` (`:251-257`): if the context is ready → `setZone(desiredZone, force)` → `setState(desiredState)`
→ `loadZoneMusicBuffers(desiredZone)`.

* `setZone` (`MusicEngine.ts:381-387`): if `!force && zone unchanged` → return; if the zone changed → `currentState = explore`;
  `_transition(2.0 s)`.
* `setState` (`:393-411`): if `state == currentState && activeSet != null` → return; cancel the victory timer;
  `_transition(1.5 s)`; if `state == victory` → after **3000 ms** `setState(explore)`.
* `loadZoneMusicBuffers(zone)` (`AudioManager.ts:422-446`): release every `bgm_*` buffer not of this zone; fetch
  `bgm_<zone>_{explore,combat,victory}` that exist in the manifest; when all settle and this is still the latest request
  → `MusicEngine.refresh()` = `_transition(1.0 s)`.

### 5.3 `_transition(duration)` (`MusicEngine.ts:461-518`)
1. Cancel all pending destroy timers. 2. `old = activeSet`.
3. Resolve the theme: `ZONE_THEMES[zone]`, else `dungeon_floor_*` → `abyss_rift`, `ember_tower` → `emerald_plains`, else none.
   No theme → fade `old` out and play nothing.
4. Rebuild the effects chain for the theme (the old chain is disconnected immediately — the fading set is therefore heard
   through the **new** chain).
5. Build the new LayerSet for `(theme.id, currentState)` (`:563-601`): if buffer `bgm_<theme.id>_<state>` is loaded → loop
   it from position 0; else if state is victory → the procedural victory stinger (§6.7); else a new `ScorePlayer` with a
   random seed (`Math.floor(Math.random()·1e9)`) started at `now + 0.1 s`, scheduled every 100 ms with 400 ms look-ahead.
6. New set gain: `0 → 1` linear over `duration`. Old set: from its current gain → `0.0001` linear over `duration`, then
   (after `ceil(duration·1000) + 100` ms) stop and disconnect all its nodes.

### 5.4 What the player actually hears (effective timings)
Because every `applyDesiredMusic` ends in a `refresh()` (`_transition(1.0)`) one or two microtasks later, the 1.5/2.0 s
"new set" is immediately replaced by a 1.0 s one and the first old set's destroy timer is cancelled:

| Situation | Old music | New music |
|---|---|---|
| Combat on/off in a zone whose MP3s are loaded | fades out linearly over **1.5 s** | the other track **from its start**, linear fade-in over **1.0 s** |
| Zone change | fades out over **2.0 s** | the new zone's procedural score starts (2.0 s ramp) and, when its MP3s finish loading, crossfades (1.0 s) into the MP3 from its start. With a warm HTTP cache the procedural part lasts only a fraction of a second. |
| `playTrack` (forced) | as zone change, even when zone and state are unchanged → the track restarts | — |
| Victory | 1.5 s out | victory track (looping buffer) 1.0 s in; after 3000 ms → explore (1.5 s out / 1.0 s in) |

QUIRKS: (a) the superseded set's destroy timer is cancelled, so a looping MP3 source keeps running at −80 dB forever
(leak) — FIX (a correct crossfader never orphans voices); (b) tracks always restart at 0 when switching explore↔combat —
KEEP for parity (optional NEW: resume the explore track where it left off, open question); (c) the brief procedural
pre-roll while an MP3 downloads — drop (assets are local in UE).

**Port values** (data, `music_config.json`): `zoneFadeOutSec 2.0`, `stateFadeOutSec 1.5`, `fadeInSec 1.0`,
`victoryHoldMs 3000`, curve linear, new track starts at 0.

### 5.5 Combat flag source (`ZoneScene.ts:3193-3211`)
`fighting = any living monster with state 'attack' || (hero.attackTarget is a living monster)`, evaluated every frame.
`fighting && !inCombat` → `inCombat = true`, clear the pending off-timer, emit `{inCombat:true}` immediately.
`!fighting && inCombat` → if no timer: `setTimeout(1500 ms)` → `inCombat = false`, emit `{inCombat:false}`.
QUIRK: the off-timer is **not** cancelled when fighting resumes while it is pending (the clear only happens in the rising
branch, which cannot run while `inCombat` is still true) → if fighting resumes inside the 1.5 s window the music still
flips to explore and back to combat. (`combat-feel.md` §9.6 describes it as cancelled — that is the intended behaviour.)
**FIX** in the core: cancel the pending off-timer on any frame where `fighting` is true. The timer uses wall-clock
`setTimeout` (keeps running during pauses); the core should use the game clock. Cleared on scene shutdown (`:7212`).

### 5.6 Story and menu hooks
* Title menu: `playTrack('menu','explore')` on every MenuScene create (also when returning from a zone) + `ZONE_ENTERED menu`
  (`MenuScene.ts:73`, `:425-437`).
* Prologue (new game, `StoryDirector.ts:83`): `sequence(PROLOGUE, 'abyss_rift')` → `playTrack('abyss_rift','explore')` for the
  whole slide show, then `playTrack(<current map>, 'explore')` (`:188-197`). So the Chapter 1 prologue plays the
  **Abyss Rift explore** recording.
* Epilogue + credits (later milestone, `:106-110`): `playTrack('abyss_rift','victory')` → the victory track for 3000 ms, then
  the MusicEngine auto-returns to Abyss Rift explore while `desiredState` stays `victory`.
* Chapter cards, cutscenes, boss intros, the boss bar and death do **not** touch the music in the web game.
* QUIRK: a `COMBAT_STATE_CHANGED` during a story sequence would re-target `desiredZone` (the sequence's zone). The world is
  frozen during sequences so it does not happen; the port adds an explicit story lock (§9.7).

---

## 6. Procedural music engine

### 6.1 Zone themes — `ZONE_THEMES` (`MusicEngine.ts:26-106`, type `types.ts:12-36`)
Only `id`, `scale` (victory stinger), `mood` and the reverb/compressor overrides are still read; `baseKey`, `tempo`,
`padWaveform`, `padFilterCutoff`, `padLFORate`, `padGain`, `melody*`, `chime*` are leftovers of the old drone engine
(export them anyway, flagged `legacy`).

| id | mood | scale (Hz) | reverb mix / decay / preDelay (resolved) |
|---|---|---|---|
| `emerald_plains` | pastoral | 130.81, 146.83, 164.81, 196.00, 220.00 | 0.22 / 1.2 / 0.015 |
| `twilight_forest` | mysterious | 146.83, 164.81, 174.61, 196.00, 220.00, 246.94, 261.63 | 0.35 / 2.0 / 0.025 |
| `anvil_mountains` | epic | 82.41, 92.50, 98.00, 110.00, 123.47, 130.81, 146.83 | 0.30 / 1.8 / 0.020 |
| `scorching_desert` | exotic | 220.00, 233.08, 261.63, 293.66, 329.63, 349.23, 392.00 | 0.25 / 1.4 / 0.018 |
| `abyss_rift` | dark | 92.50, 103.83, 110.00, 123.47, 138.59, 146.83, 164.81 | 0.38 / 2.5 / 0.030 |
| `menu` | dark | 65.41, 77.78, 87.31, 98.00, 116.54 | 0.22 / 1.8 / 0.015 |

Mood defaults (`reverbParams`, `:205-213`): pastoral 0.22/1.2/0.015, mysterious 0.35/2.0/0.025, epic 0.30/1.8/0.020,
exotic 0.25/1.4/0.018, dark 0.38/2.5/0.030; a theme field overrides the mood value. No theme overrides the compressor.

### 6.2 Effects chain (`MusicEngine.ts:241-346`)
`input(1) → highpass 75 Hz (Q 0.7 dB) → lowshelf 180 Hz (gain −4 dB)`; split: dry `× (1 − mix)`; wet
`→ delay(preDelay) → convolver(IR(decay)) × mix`; sum → compressor(−18 dB, knee 6, ratio 4, attack 0.003, release 0.25)
→ limiter(−1 dB, knee 0, ratio 20, attack 0.001, release 0.1) → `musicGain`. Applies to MP3s **and** procedural music.
If building the chain throws, music goes straight to `musicGain` (never happens in practice).

### 6.3 Composer (`Composer.ts`)
* `MODES` (`:14-22`, semitones): major `0 2 4 5 7 9 11`; minor `0 2 3 5 7 8 10`; dorian `0 2 3 5 7 9 10`;
  phrygian `0 1 3 5 7 8 10`; lydian `0 2 4 6 7 9 11`; harmonicMinor `0 2 3 5 7 8 11`; phrygianDominant `0 1 4 5 7 8 10`.
* `midiToHz(m) = 440·2^((m − 69)/12)` (`:24-26`).
* `degreeToMidi(tonic, mode, d) = tonic + 12·floor(d/7) + steps[((d mod 7) + 7) mod 7]` (`:29-34`).
* `chordDegrees(root, sevenths) = [r, r+2, r+4] (+ r+6 if sevenths)` (`:37-39`).
* `voiceChord(tonic, mode, degrees, low, high)` (`:46-55`): for each degree take its MIDI note, `+12` while `< low`,
  `−12` while `> high`, keep it if `≥ low`; de-duplicate; sort ascending.
* `makeRng(seed)` (`:58-66`) — xorshift32, **must be bit-exact** (golden tests): `s = (seed >>> 0) || 1`; each call:
  `s ^= s << 13 (u32); s ^= s >>> 17; s ^= s << 5 (u32); return s / 2^32`.
* Rhythm cells (`:80-96`, beats, negative = rest): 4/4 `[2,1,1] [1,1,2] [1.5,0.5,2] [1,0.5,0.5,2] [3,1] [-1,1,2] [2,-1,1] [1,1,1,1]`;
  3/4 `[2,1] [1,1,1] [1.5,0.5,1] [3] [-1,1,1]`.

**`writePhrase({tonic, mode, chordRoots, beatsPerBar, low, high, density, rng})`** (`:117-177`) — returns
`NoteEvent{beat, length, midi, velocity}[]`. RNG draws happen in exactly this order:
1. `pool` = `degreeToMidi(d)` for `d = −14 … 27` that lie in `[low, high]` (ascending). Empty → `[]`.
   `chordTonesIn(root)` = pool notes whose pitch class is in the triad of `root`.
   `nearest(x, list)` = element with the smallest `|n − x|`, ties → the earlier (lower) one.
2. `current = nearest((low+high)/2, chordTonesIn(roots[0]) or pool if empty)`; `beatAt = 0`.
3. For each bar `b` with root `r`:
   * `cell = cells[floor(rng()·len)]`; **then** `if rng() > density: cell = first cell with ≤ 2 entries all positive`
     (4/4 → `[3,1]`, 3/4 → `[2,1]`); if last bar: `cell = beatsPerBar == 3 ? [1,2] : [2,2]` (both draws still happen).
   * `tones = chordTonesIn(r)`; `pos = 0`; for each entry `len` (index `i`): `dur = |len|`; if `len > 0`:
     `strong = pos == 0 || (beatsPerBar == 4 && pos == 2)`;
     if `strong && tones` → `next = nearest(current + (rng() < 0.5 ? −1 : +1), tones)`;
     else → `idx = index of nearest(current, pool)`; `leap = rng() < 0.2 ? 2 : 1`; `dir = rng() < 0.5 ? −1 : +1`;
     `next = pool[clamp(idx + dir·leap, 0, len−1)]`;
     if last bar and last entry and tones → `next = nearest(next, tones)`;
     push `{beat: beatAt + pos, length: dur, midi: next, velocity: (strong ? 0.9 : 0.7)·(0.85 + rng()·0.15)}`; `current = next`.
     `pos += dur`.
   * `beatAt += beatsPerBar`.

### 6.4 `ScoreSpec` (`ScorePlayer.ts:20-38`)
```ts
{ tonic: midi; mode: Mode; progression: number[] /*scale-degree roots*/; barsPerChord: number; beatsPerBar: 3|4; tempo: bpm;
  pad:  { wave, gain, cutoff };
  bass: { gain, beats: number[] } | null;           // explore beats; combat always [0,2]
  lead: { wave, gain, low, high, density, vibrato /*cents*/ };
  arp:  { wave, gain, low, high, perBeat: 1|2, decay /*s*/ } | null;
  bell: { gain } | null;
  combatTempo: number }
```

### 6.5 `ZONE_SCORES` (`MusicEngine.ts:119-180`) — export verbatim

| id | tonic / mode | prog · bars/chord · meter | tempo (combat ×) | pad wave/gain/cutoff | bass gain/beats | lead wave/gain/low/high/density/vibrato | arp wave/gain/low/high/perBeat/decay | bell |
|---|---|---|---|---|---|---|---|---|
| `emerald_plains` | 62 D / major | 0 4 5 3 · 2 · 4/4 | 84 (1.3) | triangle/.05/1800 | .05/[0] | sine/.07/69/86/.6/10 | triangle/.035/62/79/2/.5 | .02 |
| `twilight_forest` | 57 A / dorian | 0 3 6 4 · 2 · 3/4 | 76 (1.35) | sine/.055/1400 | .04/[0] | triangle/.055/64/81/.45/6 | sine/.03/64/83/1/1.2 | .025 |
| `anvil_mountains` | 50 D / minor | 0 5 2 6 · 2 · 4/4 | 76 (1.3) | sawtooth/.035/1100 | .055/[0,2] | sawtooth/.045/62/79/.5/8 | triangle/.03/57/74/1/.7 | — |
| `scorching_desert` | 52 E / phrygianDominant | 0 1 0 6 · 2 · 4/4 | 90 (1.25) | triangle/.045/1500 | .05/[0,2] | sawtooth/.04/64/83/.7/14 | sawtooth/.022/59/76/2/.35 | — |
| `abyss_rift` | 48 C / harmonicMinor | 0 5 3 4 · 2 · 4/4 | 66 (1.35) | sawtooth/.035/950 | .045/[0] | triangle/.05/60/77/.35/6 | sine/.028/60/79/1/1.5 | .02 |
| `menu` | 57 A / minor | 0 5 2 6 · 2 · 4/4 | 62 (1.2) | triangle/.06/1300 | .035/[0] | triangle/.055/64/81/.4/5 | sine/.022/64/83/1/1.6 | .02 |

Invariants (`src/__tests__/Composer.test.ts:41-50`): every theme has a score; `lead.low ≥ 55`; `arp.low ≥ 55`;
`progression.length ≥ 3`.

### 6.6 `ScorePlayer` (`ScorePlayer.ts`)
**Clock** (`:56-73`): `beatSec = 60 / (tempo · (combat ? combatTempo : 1))`, `barSec = beatsPerBar·beatSec`, bar 0 at
`startTime`; `scheduleUntil(t)` schedules every bar whose start `< t`. A fresh player starts at bar 0 (so explore always
opens with the intro). Seed: `ScorePlayer(…, seed)`; one `rng = makeRng(seed)` per player.

**Sections** (`:77-86`), `b = bar mod 16`, `cycle = floor(bar/16)`:
* explore: `b < 4` intro; `b < 12` full; `b < 14` thin; `b ∈ {14,15}`: `rest` if `cycle` odd, else thin. → the musical
  macro-loop is **32 bars**, ending in two near-silent bars.
* combat: `b ≥ 14` thin, else full (16-bar loop, no intro, no rest).

**Per bar** (`scheduleBar`, `:93-114`), in this order (this is also the RNG order):
1. `root = progression[floor(bar/barsPerChord) mod len]`. If `bar mod barsPerChord == 0 && sec != rest` → **pad**
   (dur = `barsPerChord·barSec`, level 0.8 in intro else 1).
2. **bass** if `sec == full || (combat && sec == thin)`: beats = combat ? `[0,2]` (those < beatsPerBar) : `bass.beats`;
   each at `t + beat·beatSec`, accent 1 on beat 0 else 0.7.
3. **arpeggio** if `sec ∈ {full, thin}`, level 0.6 in thin.
4. **phrase** if `sec == full && bar even && (combat || rng() < 0.85)` (no draw in combat).
5. **bell** if `bell && sec ∈ {intro, thin} && bar even` at `t + beatSec·(rng() < 0.5 ? 0 : 1)`.
6. **percussion** if combat.

**Voices** — `env(t, peak, a, hold, r)` = `set 0.0001@t, lin peak@t+a, set peak@t+a+hold, exp 0.0001@t+a+hold+r`;
`pluck(t, peak, decay)` = `set 0.0001@t, lin peak@t+0.006, exp 0.0001@t+decay` (`:118-133`). All voices go to the trim (0.4).

| Voice | Exact recipe | Source |
|---|---|---|
| pad | `notes = voiceChord(tonic, mode, chordDegrees(root, sevenths = mode ≠ major), 55, 72)`; LP: `set 0.6·cutoff@t, lin cutoff@t+dur/2, lin 0.7·cutoff@t+dur`, Q 0.5 dB; gain `env(t, pad.gain·level/√(#notes), a = min(1.6, 0.3·dur), hold = max(0.1, dur − a), r = 1.8)`; per note two oscillators `pad.wave` at detune −6 and +6 cents; stop `t + dur + 1.8 + 0.1` | `:146-165` |
| bass | `n = degreeToMidi(root) − 12`, then `+12` while `< 40`, `−12` while `> 52`; LP `set 900@t exp 260@t+0.35` (Q default); `pluck(t, bass.gain·accent, 0.55)`; triangle at `n` + sine at `n+12`; stop `t+0.6` | `:168-183` |
| arpeggio | `tones = voiceChord(tonic, mode, triad(root), arp.low, arp.high)`; `ext = tones + [tones[0]+12]`; `pattern = ext + reverse(ext[1:-1])` (up-down); `perBeat = combat ? 2 : arp.perBeat`; `steps = beatsPerBar·perBeat`; step `i` at `t + i·beatSec/perBeat`: **skip if** `perBeat == 2 && i mod 4 == 3 && rng() < 0.5`; note `pattern[i mod len]`; LP `set 3200@at exp 900@at+decay`; `pluck(at, arp.gain·level·(i mod perBeat == 0 ? 1 : 0.7), decay)`; one `arp.wave` oscillator, stop `at + decay + 0.05` | `:186-210` |
| lead phrase | `writePhrase(tonic, mode, [root(bar), root(bar+1)], beatsPerBar, lead.low, lead.high, density = combat ? min(1, lead.density + 0.25) : lead.density, rng)`; per event: `at = t + beat·beatSec`, `dur = length·beatSec`; LP fixed `6000` if `lead.wave == sine` else `2400` (Q default); `r = min(0.9, 0.8·dur)`; gain `env(at, lead.gain·velocity, 0.04, max(0.05, 0.7·dur), r)`; two `lead.wave` oscillators at detune −4/+4; stop `at + 0.7·dur + r + 0.1`; **vibrato** if `lead.vibrato > 0 && dur > 0.5 s`: sine LFO 5.2 Hz × depth (`set 0@at lin vibrato@at + min(0.6, 0.5·dur)` cents) added to both detunes | `:213-251` |
| bell | `tones = voiceChord(tonic, mode, triad(root), 76, 91)`; `n = tones[floor(rng()·#tones)]`; sine `n` with `pluck(t, bell.gain, 2.6)` stop `t+2.7`; sine `2.76·f(n)` with `pluck(t, 0.3·bell.gain, 1.2)` stop `t+1.3`; no filter | `:253-265` |
| percussion (combat) | noise buffer 0.3 s from `makeRng(7)` (`r·2 − 1`, created once per player, deterministic). For each beat `b`: if `b` even → drum: sine `set 220@at exp 110@at+0.18`, `pluck(at, b == 0 ? 0.09 : 0.06, 0.22)`, stop `at+0.25`; every beat → shaker at `at + 0.5·beatSec`: the noise buffer from offset 0 → HP 6000 (Q default) → `pluck(st, 0.025, 0.08)`, stop `st+0.1` | `:268-301` |

Chord tables for Chapter 1 scores (computed from the rules; use as unit-test vectors):

| score | root deg | pad (MIDI) | arp tones | bell tones | bass (tri / sine) |
|---|---|---|---|---|---|
| `emerald_plains` | 0 | 62 66 69 | 62 66 69 | 78 81 86 | 50 / 62 |
| | 4 | 61 64 69 | 69 73 76 | 76 81 85 | 45 / 57 |
| | 5 | 62 66 71 | 71 74 78 | 78 83 86 | 47 / 59 |
| | 3 | 62 67 71 | 67 71 74 | 79 83 86 | 43 / 55 |
| `menu` | 0 | 57 60 64 67 | 64 69 72 | 76 81 84 | 45 / 57 |
| | 5 | 64 65 69 72 | 65 69 72 | 77 81 84 | 41 / 53 |
| | 2 | 60 64 67 71 | 64 67 72 | 76 79 84 | 48 / 60 |
| | 6 | 62 65 67 71 | 67 71 74 | 79 83 86 | 43 / 55 |
| `abyss_rift` | 0 | 55 59 60 63 | 60 63 67 | 79 84 87 | 48 / 60 |
| | 5 | 56 60 63 67 | 60 63 68 | 80 84 87 | 44 / 56 |
| | 3 | 56 60 63 65 | 60 65 68 | 77 80 84 | 41 / 53 |
| | 4 | 55 59 62 65 | 62 67 71 | 79 83 86 | 43 / 55 |

### 6.7 Procedural victory stinger (`MusicEngine.ts:624-674`)
Used only when no victory MP3 exists for the theme. `notes = last 4 of theme.scale`; note `i` starts at `T = 0.5·i`: sine
`f: set f/2@T lin f@T+0.5`; gain `set 0@T, lin 0.08@T+0.02, exp 0.048@T+0.395, exp 0.0001@T+1.645`; stop `T+2.5`.
(Chapter 1 does not need it: `emerald_plains_victory.mp3` exists.)

### 6.8 Golden vectors (generated from the TypeScript; the Python and C++ ports must match bit-for-bit where marked)
* `makeRng(1)` → `0.0000629502, 0.0157474282, 0.6164041024, 0.0716186350, 0.5584883580, 0.1735741980` (exact u32 → /2³²);
  `makeRng(12345)` → `0.7769387052, 0.3951726963, 0.6557702795, 0.4552956768`; `makeRng(0)` ≡ `makeRng(1)`.
* `writePhrase({tonic 62, major, roots [0,4], 4/4, low 69, high 86, density 0.6, rng makeRng(7)})` →
  `[beat,len,midi,vel]`: `[0,2,78,.8615] [2,1,78,.8309] [3,1,74,.6336] [4,2,73,.8228] [6,2,73,.8161]`.
* `writePhrase({57, dorian, [0,3], 3/4, 64, 81, 0.45, makeRng(3)})` → `[0,2,72,.825] [2,1,71,.6793] [3,1,74,.8786] [4,2,74,.6708]`.
* `ScorePlayer(emerald_plains, explore, seed 42)`, 32 bars: 368 oscillators, 0 noise, 26 lead notes; first leads
  `[beat, midi, vel]` = `[25,78,.6083] [26,79,.8747] [28,79,.8404] [30,79,.8275] [32,78,.8125]`; bells at beats
  `0(81) 8(85) 48(86) 56(83) 64(78) 73(76) 112(78)`.
* `ScorePlayer(emerald_plains, combat, 42)`, 16 bars: 351 oscillators, 64 noise starts, 30 lead notes; first
  `[0,78,.8103] [2,78,.8414] [3,76,.6681] [4,74,.8265] [6,74,.782]`.
* `ScorePlayer(menu, explore, 42)`, 32 bars: 326 oscillators, 24 lead notes; first `[24,71,.7985] [27,72,.6698] [28,71,.782]`;
  bells `0(81) 8(84) 49(76) 57(79) 65(81) 72(84) 113(84)`.
(Method: run `Composer.ts`/`ScorePlayer.ts` under `node --experimental-strip-types` with a mock `BaseAudioContext` that
records `createOscillator/createGain/…` calls — the engine only uses those factory methods and param automation.)

---

## 7. Recorded BGM (CC0)

`public/assets/audio/bgm/` — all 48 kHz stereo 192 kbps MP3, all from OpenGameArt, CC0 (`CREDITS.md`). Measured here
(decoded with miniaudio, before the web effects chain):

| file | track / author | length | peak | LUFS-I | note |
|---|---|---|---|---|---|
| `emerald_plains_explore.mp3` | "GrassLands Theme" — DST | 164.36 s | −1.0 dBFS | −13.1 | Lavc, enc delay 576, padding 1226 |
| `emerald_plains_combat.mp3` | "Battle Theme A" — cynicmusic | 95.85 s | −3.2 | −13.7 | LAME tag unreadable (0x55 filler) → trim by silence; 5864 leading near-silent frames |
| `emerald_plains_victory.mp3` | "Medieval: Victory Theme" — RandomMind | 32.44 s | −3.9 | −15.2 | 0.45 s trailing silence |
| `abyss_rift_explore.mp3` | "Loopable Dungeon Ambience" — JaggedStone | 94.31 s | −1.0 | −14.3 | used by the Chapter 1 prologue |
| `abyss_rift_combat.mp3` | "Battle Theme B" — cynicmusic | 66.10 s | | | later |
| `abyss_rift_victory.mp3`, `anvil_mountains_victory.mp3` | "Victory Fanfare Short" — cynicmusic | 11.90 s | | | later |
| `twilight_forest_explore/combat/victory.mp3` | "Dark Forest Theme" — cynicmusic; "Grizzly Dwarf Battle LOOP" / "… Victory LOOP" — Zane Little Music | 93.12 / 57.65 / 8.28 s | | | later |
| `anvil_mountains_explore/combat.mp3` | "Fantasy Choir 2" — Cesar da Rocha; "Epic Boss Battle" — Juhani Junkala | 126.19 / 123.48 s | | | later |
| `scorching_desert_explore/combat/victory.mp3` | "Desert Theme" — Tarush Singhal; Junkala (reused); RandomMind (reused) | 126.12 / 123.48 / 32.50 s | | | later |

Web loops them with `source.loop = true` and no loop points (`MusicEngine.ts:611-617`), so the MP3 encoder delay/padding
produces a short gap at every wrap (QUIRK → FIX: gapless trim, §9.4). Victory tracks also loop in the web (only ~3–4.5 s is
ever heard); the port plays them as one-shots.

---

## 8. Settings, UI and lifecycle

### 8.1 Settings (`AudioManager.ts:121-159`, `:396-420`)
`AudioSettings {bgmVolume: 0..1, sfxVolume: 0..1, bgmMuted: bool, sfxMuted: bool}` persisted as JSON in `localStorage`
key `abyssfire_audio` on every change; on load each field falls back to its default if missing or of the wrong type
(QUIRK: loaded numbers are not clamped — FIX: clamp to [0,1]). `setMusicVolume/SFXVolume` clamp and apply the gain only
when not muted; `toggle*Mute` sets the gain to 0 or back to the volume. Settings are per device, **not** part of `SaveData`.
`setMusicTempMute(m)` (jukebox pause) sets the music gain without persisting (QUIRK: a later volume change overrides it).

### 8.2 Audio panel (`UIScene.ts:5216-5296`)
Hotkey **O** (`ZoneScene.ts:2189`). Panel 420×170 (logical px), title `ui.audio.title` 音频设置 / Audio Settings; two rows:
`ui.audio.bgm` 背景音乐 / Music (y 60) and `ui.audio.sfx` 音效 / SFX (y 108): label, a 180 px slider (fill `#d4a54a`,
handle `#e8c77a`), `NN%` text, and a mute toggle button labelled `ui.audio.muted` [静音] (`#ff8a72`) /
`ui.audio.unmuted` [开启] (`#a8f090`). Dragging applies and persists continuously. 3D: a UMG settings page with the same
controls (also reachable from the mobile pause menu).

### 8.3 Jukebox (`MenuScene.ts:30-42`, `:804-1062`) — title menu "OST"
11 entries `{titleKey, zoneId, state, duration}` (nominal durations 120–210 s, not the real lengths): menu; each of the 5
zones × {explore, combat}. Play = `playTrack(zone, state)`; a 250 ms timer advances `elapsed` and moves to the next entry at
`duration`; prev (restart if `elapsed > 3 s`), next, play/pause. QUIRKS → FIX: "pause" only mutes (music keeps running);
seeking only moves the counter; durations are nominal. Port: real pause/seek, real lengths from `music_tracks.json`, list
only tracks whose assets are packaged (Chapter 1: menu, plains explore, plains combat; optionally boss and prologue). On
close → back to the title theme.

### 8.4 Lifecycle (web-only)
Lazy `AudioContext` created and resumed on the first `pointerdown`/`keydown` anywhere (capture) or `ensureContext()`
(`AudioManager.ts:188-245`); until then every SFX is dropped and music waits. `document.hidden`/`pagehide` suspends the
context, `visibilitychange`/`pageshow` resumes it (`:78-100`). UE equivalent: §9.7.

---

## 9. Port strategy

### 9.1 Options

| Criterion | A. Offline render (Python/numpy port of the synth) → WAV → `USoundWave` | B. Runtime procedural in UE (MetaSounds / C++ DSP) |
|---|---|---|
| Fidelity to web | Exact recipes, exact score algorithm, exact chain incl. Chromium compressor; verifiable against the TS (§6.8) | MetaSound node semantics differ (biquad Q, oscillator band-limiting, envelopes); the generative score needs a C++ sequencer + per-note graph instancing |
| Runtime cost (iOS/Android) | ~0 (decode only) | 30–60 simultaneous synth voices for the score + every SFX; risky on low-end Android |
| Determinism / tests | Deterministic seeds, CI-checkable (golden vectors, loudness, loop seams); assets reviewable by ear | Hard to test in CMake; graphs are binary `.uasset`s |
| Variation | Fixed loops; mitigated by long multi-cycle renders and 4 noise variants per SFX | Infinite variation (the web's score never repeats) |
| Iteration | Edit JSON/recipe → `render_all.py` (seconds) → reimport | Edit graphs in the editor |
| Tooling | Python 3.11 + numpy/scipy here (PyPI reachable); no UE needed to produce audio | UE editor needed for everything |
| Size | ~10 MB compressed for Chapter 1 music, SFX < 1 MB | negligible |

**Recommendation: A.** The web's only real advantage of runtime synthesis is non-repeating phrases; at 92–248 s per loop
(32–64 bars, several phrase cycles with different RNG draws) that is not audible in play, while A removes all runtime DSP
from mobile, makes audio testable without the editor, and lets the CC0 recordings and the synthesised material share one
mastering chain. The recipe JSON (§11) is engine-agnostic, so a later MetaSound Builder importer remains possible.

### 9.2 Hybrid content rule
* Where the web plays a CC0 recording (every zone explore/combat/victory) → ship that recording, decoded, gapless-trimmed
  and baked through the theme's effects chain. Keeps the sound players know.
* Where the web plays the procedural score (menu; Ember Tower and labyrinth floors in later milestones) → render the score.
* NEW content (boss music, story stingers) → render with the same engine/recipe language (§10.3).

### 9.3 Renderer (`unreal/Tools/audio/`, Python ≥ 3.11, `numpy`, `scipy`, `miniaudio` for MP3 decode)
```
abyss_audio/params.py      AudioParam timeline (set/lin/exp; a-rate inputs)            §2.1
abyss_audio/osc.py         band-limited wavetable oscillators (PeriodicWave coefficients, ≥3 tables/octave,
                           ≥2048-point tables, linear interpolation, signed phase increment)   §2.2
abyss_audio/biquad.py      Web Audio biquads, coefficient update per 128 frames (scipy.signal.lfilter with zi) §2.3
abyss_audio/shaper.py      256-point curve lookup                                       §2.4
abyss_audio/dynamics.py    Chromium DynamicsCompressorKernel port (32-frame divisions; pure Python is fast enough) §2.7
abyss_audio/reverb.py      IR generation + Web Audio normalisation + FFT convolution (scipy.signal.oaconvolve) §2.6
abyss_audio/composer.py    port of Composer.ts (bit-exact xorshift)                     §6.3
abyss_audio/score.py       port of ScorePlayer.ts (offline: schedule all bars up front)  §6.6
abyss_audio/sfx.py         interpreter for data/sfx_recipes.json                        §4, §11
abyss_audio/chain.py       per-theme effects chain                                      §6.2
abyss_audio/loops.py       tail wrap + circular effect processing + crossfade looping    §9.4
abyss_audio/mp3.py         decode (48 kHz float), gapless trim, seam check              §9.4
abyss_audio/master.py      LUFS (BS.1770), true-peak, TPDF dither (seeded), WAV writer
render_all.py              reads data/*.json → writes unreal/RawAudio/{SFX,Music,Stingers}/*.wav + render_manifest.json
tests/                     pytest: golden vectors (§6.8), chord tables (§6.6), envelope maths, loop seams, loudness bounds
```
Sample rate 48 kHz (UE mixer default). Internal float64. Output: SFX mono 24-bit, music/stingers stereo 24-bit (or 16-bit
with seeded TPDF dither if repo size matters). Every random source is seeded from `crc32(assetId) ^ variant`; renders are
byte-reproducible on the same numpy version. `render_manifest.json` records per asset: length, loop start/end (samples),
peak, LUFS-I, seed, source hash.

### 9.4 Loops
* **Procedural score loops**: render the dry score for `N` bars where `N` is a multiple of 32 (explore — the loop ends on the
  `rest` bars of an odd cycle) or 16 (combat), plus the release tails (≤ 1.9 s pad, ≤ 1.0 s lead); **overlap-add the tail
  onto the start**. Then run the effects chain on `[loop, loop]` and keep the second copy (reverb tail and compressor state
  are then circular). Loop points = whole file.
* **CC0 recordings**: decode → trim encoder delay/padding (LAME/Info tag: `enc_delay + 529` at the start,
  `enc_padding − 529` at the end; unreadable tag → trim leading/trailing digital silence < −80 dBFS) → verify the seam
  (|x[0] − x[N−1]| small and no spectral click; else 10 ms equal-power crossfade at the seam) → chain on `[loop, loop]`,
  keep the second copy.
* **Stinger beds** that must loop (whisper): render length + overlap, equal-power crossfade the overlap into the start.

### 9.5 SFX rendering
One WAV per cue; cues with noise layers (§4.1) get **4 variants** (different noise seeds, identical oscillator layers)
played round-robin without immediate repeats. No pitch randomisation (parity). Level = exact web pre-volume level
(no normalisation); the peak is recorded in the manifest.

### 9.6 Levels and buses (UE)
* Assets carry the **exact web pre-volume level**: music already includes the chain's make-up and limiter; SFX are raw.
* Runtime gain = user volume (linear, default BGM 0.15, SFX 0.30) via Sound Classes `SC_Music`, `SC_SFX` (children
  `SC_SFX_Combat`, `SC_SFX_UI`) under `SC_Master`, driven by a `USoundMix` with class overrides
  (`UGameplayStatics::SetSoundMixClassOverride` + `PushSoundMixModifier`); mute = override 0 with the slider value kept.
* Submixes `SMX_Music`, `SMX_SFX` → Master. NEW: a master limiter (−1 dBFS) on the main submix — the web has none on SFX and
  dense fights can clip; it is inaudible otherwise.
* NEW: Sound Concurrency (the web is unlimited): `hit`/`crit`/`miss` max 8 voices each, stop oldest, min retrigger 20 ms;
  `skill_*` max 4 each; `loot_*` max 3; `click` max 2; everything else max 2. DoT ticks spam `hit` — this cap matters.
* Reference levels: the plains recordings measure −13.1 / −13.7 LUFS-I before the chain (§7). The procedural score is
  *estimated* (not yet measured — the renderer's first job is to report it) to sit 8–10 LU below them after the chain: its
  peaks stay around −17…−20 dBFS before the chain, mostly below the compressor threshold, so it only gains the ≈ +7.5 dB
  make-up, while the recordings are compressed down from near 0 dBFS. Parity keeps that difference; see open question 3.

### 9.7 UE runtime (gameplay module)
* `UAbyssAudioSubsystem : UGameInstanceSubsystem` (survives level loads, like the web singleton). Owns: a pool of 3 music
  `UAudioComponent`s (`bIsUISound = true` so music continues while the game is paused/cinematic; non-spatialised), one
  stinger component, SFX playback. Loads `DT_AudioCues`, `DT_MusicTracks` (imported from §11 JSON).
* Consumes **commands from the core** (`MusicDirector`, §9.8) and **SFX ids from the core's `AudioCueMapper`**; it never
  decides game rules.
* Crossfade: `Old->FadeOut(fadeOutSec, 0.f, EAudioFaderCurve::Linear)`; `New->SetSound(Wave)`;
  `New->FadeIn(fadeInSec, 1.f, startTimeSec = 0, EAudioFaderCurve::Linear)`. A component that is fading out when a third
  request arrives is stopped immediately (never orphaned — fixes the web leak).
* SFX: UI/progression cues (`click`, `panel_*`, `error`, `loot_*`, `equip`, `potion`, `quest_*`, `levelup`, `npc_interact`,
  `zone_transition`, `anvil`, `resonance`) → 2D (`PlaySound2D`). Combat cues → NEW mild spatialisation: played at the event's
  world location with an attenuation preset whose inner radius covers the visible play area (no distance loss on screen) and
  stereo panning only; the listener follows the hero (`APlayerController::SetAudioListenerOverride`). Parity mode = 2D for
  everything (open question 4).
* Timing: play on receipt of the core event. Hits are emitted by the core at the contact beat (combat-feel spec), so no extra
  delay; skill cues at cast commit. Keep the platform audio buffer small enough for ≤ 30 ms output latency.
* Asset settings: SFX `Loading Behavior = Retain On Load`, compression ADPCM or PCM (instant start); music/stingers
  `Load On Demand` (stream), platform default compression (Bink Audio / Vorbis) at quality ≈ 80; music `bLooping = true`
  except victory/stingers.
* App lifecycle (replaces the browser unlock/visibility code): no unlock is needed. Desktop: `[Audio]
  UnfocusedVolumeMultiplier=0.0` in `DefaultEngine.ini` (mute when the window is unfocused; the web only mutes when the tab is
  hidden — accepted difference). Mobile: pause music and SFX on `ApplicationWillEnterBackgroundDelegate`, resume on
  `ApplicationHasEnteredForegroundDelegate`. iOS audio session: Ambient category (respects the silent switch, mixes with
  other apps — same as Web Audio in mobile Safari); verify the 5.8 iOS runtime setting during bring-up.
* Settings persistence: `UAbyssAudioSettings` (SaveGame slot `AudioSettings` or `GameUserSettings` ini) with the same four
  fields and defaults; clamp on load.

### 9.8 Core library (C++20, `AbyssCore`, unit-tested with CMake)
```cpp
namespace abyss::audio {
enum class SfxId : uint8_t { Hit, HitHeavy, Crit, Miss, Block, PlayerHurt, MonsterDeath, PlayerDeath, Dodge, Resonance,
  SkillMelee, SkillFire, SkillIce, SkillLightning, SkillHeal, SkillBuff, LootCommon, LootMagic, LootRare, LootLegendary,
  Equip, Potion, Click, PanelOpen, PanelClose, Error, ZoneTransition, QuestComplete, QuestProgress, QuestObjective,
  LevelUp, NpcInteract, Anvil, Count };
std::string_view sfxKey(SfxId);                         // "hit", "skill_fire", … (stable ids used by the data tables)

// §3 rules as pure functions over core events (no UE types):
std::optional<SfxId> sfxForCombatDamage(bool isDodged, bool isCrit);
SfxId sfxForSkill(DamageType);                          // fire/ice/lightning; arcane+poison → SkillBuff; else SkillMelee
SfxId sfxForPickup(ItemQuality);                        // set → LootLegendary
std::optional<SfxId> sfxForQuestProgress(int current, int required, std::string_view targetId, bool completesQuest);

enum class MusicState : uint8_t { Explore, Combat, Victory, Boss /*NEW*/ };
struct MusicCommand { std::string trackKey; float fadeOutSec; float fadeInSec; bool loop; };   // trackKey "" = silence
struct MusicConfig { float zoneFadeOutSec = 2.0f, stateFadeOutSec = 1.5f, fadeInSec = 1.0f; uint32_t victoryHoldMs = 3000; /*+ tables*/ };

std::string_view resolveMusicTheme(std::string_view zoneId); // theme table; dungeon_floor_* → abyss_rift; ember_tower → emerald_plains; else ""

class MusicDirector {
 public:
  explicit MusicDirector(const MusicConfig&);
  void onZoneEntered(std::string_view zoneId);          // desired = (zone, Explore); zone change → zone fade
  void onCombatStateChanged(bool inCombat);             // Explore/Combat; ignored while storyLock or Victory hold
  void onBossEngaged(std::string_view bossDefId);       // NEW: Boss state if a boss track is mapped
  void onBossDisengaged();                              // NEW: back to Combat/Explore from the combat flag
  void onBossDefeated(std::string_view bossDefId);      // NEW: Victory, then Explore after victoryHoldMs
  void playTrack(std::string_view zoneId, MusicState);  // forced (story, jukebox) — restarts even if unchanged
  void setStoryLock(bool);                              // NEW: sequences own the music; combat changes are queued
  void tick(uint64_t nowMs);                            // victory auto-return
  std::optional<MusicCommand> takeCommand();            // UE polls each frame
};
}
```
Track key = `<themeId>_<state>` (the web uses the zone id for the file manifest but the theme id for buffer lookup — QUIRK
that makes the Ember Tower play the procedural plains score; the port always uses the theme id plus an explicit
per-zone override table, §11 `music_tracks.json.zoneOverrides`). Unit tests: every row of §3.1/§3.2, the transition table
of §5.4 (zone → 2.0/1.0, state → 1.5/1.0, victory → explore after 3000 ms, `playTrack` restarts, unknown zone → silence),
theme resolution, story lock, boss precedence (§10.4), and the fixed combat-flag debounce (§5.5).

### 9.9 Validation of the renderer
* Golden vectors §6.8 and chord tables §6.6 (exact).
* Envelope maths: for each recipe, the rendered 5 ms RMS envelope of a noise-free layer matches the analytic ADSR within
  0.2 dB over the first 300 ms.
* Optional browser reference (one-time, any desktop browser): render each SFX and 32 bars of each score with an
  `OfflineAudioContext(2, …, 48000)` using the unchanged TS engines (they only need a `BaseAudioContext`), export WAV, and
  compare: peak ±0.5 dB, RMS ±0.5 dB, 50 ms-frame envelope correlation ≥ 0.98, spectral centroid ±5 % (noise layers compared
  by band energy only). The music chain: integrated loudness ±0.5 LU.
* Loops: no discontinuity at the seam (|Δ| below the local 99th-percentile sample delta), and the chained second copy
  equals a 3-copy render's middle copy to −60 dB.

---

## 10. Chapter 1 asset list

Chapter 1 = title menu, prologue, zone `emerald_plains` (Lv 1–10), boss 碎牙·格罗克 (`goblin_chief`), story beats
`chapter_emerald_plains`, `cs_ep_mark`, `cs_ep_whisper`, `cs_ep_finale`, `cs_boss_goblin_chief` (see `quests-story-ch1.md`).

### 10.1 Music

| Asset | Track key | Source | Length / loop | Chain | Plays when |
|---|---|---|---|---|---|
| `SW_MUS_Menu` | `menu_explore` | procedural `menu` score, explore, seed `menu-1`, **64 bars** | 247.74 s, loop | menu (0.22/1.8/0.015) | title menu, jukebox #1 |
| `SW_MUS_EmeraldPlains_Explore` | `emerald_plains_explore` | CC0 "GrassLands Theme" (DST) | 164.4 s, loop | emerald (0.22/1.2/0.015) | zone, not in combat ("plains day") |
| `SW_MUS_EmeraldPlains_Combat` | `emerald_plains_combat` | CC0 "Battle Theme A" (cynicmusic) | 95.9 s, loop | emerald | combat flag on |
| `SW_MUS_EmeraldPlains_Victory` | `emerald_plains_victory` | CC0 "Medieval: Victory Theme" (RandomMind) | 32.4 s, one-shot | emerald | NEW trigger: `goblin_chief` defeated (§10.4) |
| `SW_MUS_Boss_Ch1` | `boss_ch1` | NEW procedural score `boss_ch1` (§10.3), combat state, **32 bars** | 70.33 s, loop | emerald | NEW: fighting `goblin_chief` |
| `SW_MUS_Prologue` | `abyss_rift_explore` | CC0 "Loopable Dungeon Ambience" (JaggedStone) | 94.3 s, loop | abyss (0.38/2.5/0.030) | prologue sequence (web: `playTrack('abyss_rift','explore')`) |

Later milestones reuse the same pipeline: the other 11 recordings; the procedural `emerald_plains` explore score for the
Ember Tower (64 bars, 182.86 s); the procedural `abyss_rift` explore/combat scores for labyrinth floors; procedural victory
stingers for themes without a victory recording.

### 10.2 Story stingers (all NEW; stereo; one-shot unless noted; routed to `SC_Music` on the stinger component, not crossfaded)

| Asset | Trigger (core event) | Length | Music bed behaviour |
|---|---|---|---|
| `SW_STG_ChapterCard_Dawn` | chapter card shown (`chapter_<zone>` beat; ch1 mood `dawn`) | 8.9 s (matches the card: shade 0.6, number to 1.1, title 1.1–2.0, subtitle 2.0–2.6, body 2.6–3.5, hold to 7.3, fades to 8.9) | duck music −8 dB (0.6 s in, 0.7 s out) |
| `SW_STG_BossIntro` | the `title` step of a boss intro cutscene (`cs_boss_goblin_chief`) | 3.5 s (band 0.2, name/slash 0.38, epithet to 0.78, hold to 2.98, fade 0.45) | duck −10 dB during the title step |
| `SW_STG_Whisper_Loop` | each `whisper` step (`cs_ep_whisper`), loops while shown | 6.0 s loop; fade in 0.6 s / out 0.6 s with the vignette | duck −6 dB |
| `SW_STG_CutsceneIn` | letterbox bars slide in (every cutscene) | 1.2 s | none |

Quest turn-in, level-up and objective fanfares are the existing SFX (`quest_complete`, `levelup`, `quest_objective`).

### 10.3 NEW recipes

**`boss_ch1` ScoreSpec** (same engine, rendered in `combat` state; D phrygian i–♭II–i–♭VII, tribal and menacing):
```json
{ "id": "boss_ch1", "tonic": 50, "mode": "phrygian", "progression": [0, 1, 0, 6], "barsPerChord": 2, "beatsPerBar": 4,
  "tempo": 84, "combatTempo": 1.3,
  "pad":  { "wave": "sawtooth", "gain": 0.04, "cutoff": 1300 },
  "bass": { "gain": 0.06, "beats": [0, 2] },
  "lead": { "wave": "sawtooth", "gain": 0.05, "low": 62, "high": 79, "density": 0.65, "vibrato": 10 },
  "arp":  { "wave": "sawtooth", "gain": 0.025, "low": 57, "high": 74, "perBeat": 2, "decay": 0.3 },
  "bell": null }
```
(109.2 BPM, 32 bars = 70.33 s; satisfies the score invariants.) Fallback if it does not hold up next to the recordings:
the CC0 "Epic Boss Battle" (Junkala) already in the repo (open question 5).

**Stinger recipes** (recipe language of §4, rendered through the emerald chain, then gain-matched so their short-term
loudness max is 3 LU below `SW_MUS_EmeraldPlains_Explore`):
* `SW_STG_ChapterCard_Dawn` (D major): (1) pad A3 D4 F♯4 A4 (MIDI 57 62 66 69), triangle pairs at ±6 cents,
  LP `set 700@0 lin 1800@1.1 lin 1200@7.3`; gain `set 0.0001@0 lin 0.025@1.1 set 0.025@7.3 exp 0.0001@8.9`; span 0–9.0.
  (2) bell arpeggio at the title reveal: score `bell` voice (§6.6, gain 0.05) on MIDI 74, 78, 81, 86 at 1.10, 1.28, 1.46,
  1.64 s. (3) breath: noise → BP(400, 0.7) gain `set 0@0 lin 0.03@1.0 exp 0.001@3.0`. (4) subtitle chime: sine 1174.66 Hz
  `pluck(2.0, 0.03, 1.5)`.
* `SW_STG_BossIntro`: (1) boom: sine `set 70@0 exp 35@0.5` ADSR(0.002, 0.1, 0.3, 0.6, 0.35) + NB(0.25, 300, lowpass, 0, 0.3).
  (2) slash rip synced with the slash bar: saw `set 300@0 exp 2400@0.38` → BP(1800, 2) → ADSR(0.01, 0.2, 0.3, 0.3, 0.12).
  (3) brass cluster D3 A3 E♭4 (MIDI 50 57 63), saw pairs ±8 cents → LP(`set 400@0.3 lin 1500@0.8 lin 600@3.4`), gain
  `set 0.0001@0.25 lin 0.06@0.8 set 0.06@2.98 exp 0.0001@3.5`. (4) two score drums (§6.6 percussion drum, peak 0.09) at
  0.78 and 1.05 s.
* `SW_STG_Whisper_Loop`: noise → BP(`set 900@0 lin 1600@3 lin 900@6`, 4) gain 0.03; sines 311.13 and 329.63 Hz (gain 0.012
  each, beating minor second) + 466.16 Hz (0.008), all with 5 Hz ±15-cent vibrato; render 8 s, crossfade the last 2 s into
  the start → 6 s loop.
* `SW_STG_CutsceneIn`: noise → BP(`set 600@0 exp 1800@0.45`, 1) ADSR(0.05, 0.2, 0.3, 0.2, 0.08) + score bell voice on
  MIDI 62 (gain 0.04).

### 10.4 Chapter 1 music rules (core `MusicDirector`, data-driven)
Priority: story lock > victory hold > boss > combat > explore.
1. Title menu → `menu_explore` (restart on every return to the menu, fade-in 1.0 s).
2. New game: zone entered `emerald_plains` → explore; prologue sequence → `setStoryLock(true)`, `playTrack(abyss_rift,
   Explore)` → at the end `playTrack(emerald_plains, Explore)`, `setStoryLock(false)`; chapter card → stinger + duck.
3. Combat flag (fixed debounce, §5.5) → `emerald_plains_combat` / back to explore (1.5 s out / 1.0 s in).
4. NEW boss: the story director's boss scan reports `goblin_chief` within 14 tiles (boss bar shown) **and** the combat flag
   is on → `onBossEngaged` → `boss_ch1` (state fade). Boss bar cleared without a kill → `onBossDisengaged`. Boss killed →
   `onBossDefeated` → `emerald_plains_victory` one-shot (1.5 s out / 1.0 s in), after `victoryHoldMs` (default 3000;
   recommend 8000 for boss victories so the fanfare phrase completes — open question 6) → explore. Mapping
   `bossMusic: {goblin_chief: "boss_ch1"}`.
5. Cutscenes: no music change except the stingers/ducking of §10.2.

### 10.5 SFX
All 33 cues of §4 (`SW_SFX_<Id>[_01..04]`, mono), 16 with 4 variants → 81 files. Wiring per §3 with the FIXes:
`monster_death` on every kill, one click per panel toggle. Chapter 1 does not use `quest_objective` from the labyrinth seal
or the volatile-burst `hit`, but the cues are shared.

---

## 11. Data to export (JSON, `unreal/Tools/audio/data/` and the UE DataTables)

| File | Content | Source (export script reads the TS module) |
|---|---|---|
| `music_themes.json` | `ZONE_THEMES` rows: `id, mood, scale[], reverbMix?, reverbDecay?, reverbPreDelay?, compressor*?` + `legacy` block (baseKey, tempo, padWaveform, padFilterCutoff, padLFORate, padGain, melody/chime gains) | `MusicEngine.ts:26-106` |
| `music_effects.json` | mood reverb defaults, chain constants (HPF 75/0.7 dB, lowshelf 180/−4, compressor defaults, limiter) | `MusicEngine.ts:205-213`, `:254-303`, `:326-346` |
| `music_scores.json` | `ZONE_SCORES` rows (§6.4 schema) + `SCORE_TRIM` + NEW `boss_ch1` | `MusicEngine.ts:117-180` |
| `composer.json` (or constants in code) | `MODES`, `RHYTHMS_4`, `RHYTHMS_3`, `BASS_FLOOR = 40` | `Composer.ts:14-22`, `:80-96`; `ScorePlayer.ts:43` |
| `sfx_recipes.json` | §4 transcribed: `{id, length, layers:[{source:{kind:"osc", wave, freq:[["set",250,0],["exp",80,0.2]], detune?, lfo?:{wave, rate, depth, target:"frequency"}} \| {kind:"noise", dur, start} \| {kind:"tone"…}, shaper?:"tanh3"\|"tanh4"\|"tanh2"\|"sqrt", filter?:{type, freq: number\|automation, q?}, env:{kind:"adsr", a,d,s,r,peak,start} \| {kind:"chime", level, start} \| {kind:"const", gain} \| {kind:"points", points:[…]}, span:[start, stop]}]}` + NEW stinger recipes | `SFXEngine.ts` (code → hand transcription; review against §4) |
| `music_tracks.json` | `{key, asset, kind:"recording"\|"score"\|"stinger", loop, lengthSec, loopStartSample, loopEndSample, theme, credit:{title, author, license, url}}` + `zoneOverrides` (`ember_tower` → `emerald_plains_explore_score`, `dungeon_floor_*` → `abyss_rift_*_score`) + `bossMusic` | `public/assets/audio/bgm/CREDITS.md`, §10 |
| `music_config.json` | `zoneFadeOutSec 2.0, stateFadeOutSec 1.5, fadeInSec 1.0, victoryHoldMs 3000, combatOffDelayMs 1500`, duck amounts | §5 |
| `audio_cues.json` | per SFX: `{id, assets[], bus:"combat"\|"ui", spatial:"2d"\|"3d", concurrency:{max, rule, minRetriggerMs}}` | §9.6, §9.7 |
| `jukebox_tracks.json` | `{titleKey, trackKey}` (lengths come from `music_tracks.json`) | `MenuScene.ts:30-42` |
| `audio_settings_defaults.json` | `{bgmVolume 0.15, sfxVolume 0.3, bgmMuted false, sfxMuted false}` | `AudioManager.ts:27-32` |
| i18n | `ui.audio.*`, `menu.jukebox.*`, `menu.ost` (zh-CN + en) | `src/i18n/locales/{zh-CN,en}.ts` |

The event → cue rules (§3) live in core code (`AudioCueMapper`), not data.

---

## 12. Web-only / render-only items and their 3D equivalents

| Web | 3D equivalent |
|---|---|
| `AudioContext` lazy creation, gesture unlock, `lifecycle` locked/unlocking/ready/failed (`AudioManager.ts:188-249`) | none — UE audio is live at boot; sounds requested before the menu is up simply play |
| `visibilitychange`/`pagehide`/`pageshow` suspend/resume (`:78-100`) | `UnfocusedVolumeMultiplier` + mobile background delegates (§9.7) |
| `AudioLoader` fetch/decode/abort/release, keep only the current zone's BGM (`AudioLoader.ts`, `AudioManager.ts:422-446`) | UE asset streaming (`Load On Demand`); no manual release |
| `__BGM_MANIFEST__` (vite define) | `music_tracks.json` → `DT_MusicTracks` |
| Per-play Web Audio node graphs (SFX recipes, ScorePlayer, chain) | baked WAVs (§9) |
| `setTimeout` / `setInterval` timers (victory 3000 ms, scheduler 100 ms, combat off-delay 1500 ms) | core game clock (`tick(nowMs)`); scheduler not needed |
| `localStorage` `abyssfire_audio` | SaveGame slot / `GameUserSettings` |
| Phaser audio panel and jukebox UI | UMG |

---

## 13. Quirk list (summary)

| # | Quirk | Decision |
|---|---|---|
| Q1 | `MONSTER_DIED` never emitted → `monster_death` silent | FIX |
| Q2 | I/U/V hotkeys play two clicks at once | FIX |
| Q3 | Combat off-timer not cancelled when fighting resumes (music flips explore→combat inside 1.5 s) | FIX |
| Q4 | Superseded music set never destroyed (looping source leaks at −80 dB) | FIX |
| Q5 | Effective fade-in is 1.0 s (refresh pass), not the 2.0/1.5 s in the code | KEEP the heard values (data) |
| Q6 | Explore/combat tracks restart from 0 on every switch | KEEP (open question 2) |
| Q7 | MP3 loops not gapless | FIX |
| Q8 | Ember Tower: buffer key by theme, manifest by zone → plays the procedural plains score after a brief MP3 flash | FIX via explicit `zoneOverrides` (default: procedural plains score, as heard) |
| Q9 | `TONE` voices have no envelope (clicks) | KEEP (optional de-click flag) |
| Q10 | Jukebox pause = mute, seek = UI only, nominal durations | FIX |
| Q11 | Settings loaded without clamping | FIX |
| Q12 | `hit_heavy`, `block`, `player_hurt`, `skill_heal`, `potion`, `panel_close` unreachable; stat-dodges silent; DoT ticks each play `hit` | KEEP (render the cues; concurrency caps the tick spam) |
| Q13 | Skill sounds fire at cast commit, not at the contact beat | KEEP |

---

## 14. Open questions

1. `combat-feel.md` §9.6 says the combat off-debounce is cancelled when fighting resumes; the code does not do that (§5.5).
   This spec recommends the FIX (true debounce). Confirm so both specs agree.
2. Should the explore track resume where it left off after a fight (NEW) instead of restarting at 0 (web)?
3. Loudness parity: the procedural menu score is estimated 8–10 LU quieter than the zone recordings after the web chain, and the default
   volumes (BGM 0.15, SFX 0.3) are low for phone speakers. Keep exact web levels, or loudness-normalise all music to one
   target (e.g. −16 LUFS pre-volume) and raise the defaults?
4. Combat SFX: keep the web's flat 2D centre mix, or adopt the mild 3D panning of §9.7?
5. Boss music: procedural `boss_ch1` (unique, matches the generated-asset pipeline) or the CC0 "Epic Boss Battle" recording
   (higher production value, but it is also the Chapter 3/4 combat track)?
6. Boss victory hold: web value 3000 ms (designed for the epilogue) or 8000 ms for boss kills?
7. Should heavy hits (HitFeedback weight `heavy`/`kill`) use the existing but unused `hit_heavy` cue, and should the hero's
   damage taken use `player_hurt`? Both would be deviations from the web mix.
8. Ambience beds (plains wind/birds), footsteps and monster vocalisations do not exist in the web game; are they in scope for
   the Chapter 1 milestone or a later polish pass?
9. Repository size: commit rendered WAVs (≈ 200 MB raw 24-bit for Chapter 1 music) via Git LFS, or commit only the renderer
   and regenerate in CI, with only the imported `.uasset`s in LFS?

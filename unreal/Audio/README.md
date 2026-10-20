# unreal/Audio — rendered audio, renderer and UE import

Owner: audio. Spec: `Docs/spec/audio.md` (web behaviour, port strategy §9, Chapter 1 list §10); decisions A1–A8 in
`Docs/DECISIONS.md`. Runtime player: `Source/Abyssfire/{Public,Private}/Audio/` (`UAbyssAudioSystem`,
`FAbyssAudioManifest`).

```
render/               offline Python/numpy port of the web synth (Web Audio emulation, Composer/ScorePlayer,
                      Chromium compressor, effects chains), mastering and OGG export
  render_all.py       renders + levels + exports everything, writes Export/audio_manifest.json
  export_wav.py       optional: Export/*.ogg -> 16-bit WAV (only for tools that refuse OGG)
  abyss_audio/        DSP package (params, osc, biquad, shaper, dynamics, reverb, graph, sfx, composer, score,
                      chain, loops, mp3, master, report) + recipes/ (web SFX, A7 cues, dark_vox AU1 voices,
                      dark_ambience AU2 ambience and bells, dark_music AU3 score interpreter, story stingers)
  data/               port_scores.json (the web-engine boss_ch1 score, A5: the record; AU3 reworked it),
                      dark_scores.json (AU3: the dark remake's scores, stingers and music director table),
                      sfx_recipes.json (every recipe as JSON, generated; AU3's scores stay in dark_scores.json)
  tests/              pytest: Web Audio maths, composer golden vectors, chord tables, loudness, Ogg export, AU1 / AU2
                      recipe invariants and loop seams, AU3 notation, score data, envelopes, loops, director;
                      test_exports.py: the committed Export/ against the manifest (sha256, frames, channels per
                      kind, loudness targets and groups, true peak, loops, references, the package budget)
Export/               committed renders (OGG Vorbis q6, 48 kHz) + audio_manifest.json      (DECISIONS A1)
  SFX/ Footsteps/     mono one-shots (AU2 bells: stereo, but the cracked note and the far toll)
  Music/ Ambience/ Stingers/   stereo (AU2 emitters: mono)
Reports/              waveform + spectrum sheets per kind (*.png; AU3: music_dark.png, level + spectrogram per track) and
                      stats.md (no listening test is possible where
                      the renders are produced: check these)
ue/import_audio.py    editor Python: imports Export/ into /Game/Abyssfire/Audio with the playback settings
```

## Rendering

```sh
python3 -m venv /opt/venvs/audio && /opt/venvs/audio/bin/pip install -r unreal/Audio/render/requirements.txt
/opt/venvs/audio/bin/python unreal/Audio/render/render_all.py --reports      # everything, ~4 min
/opt/venvs/audio/bin/python unreal/Audio/render/render_all.py --only stingers  # sfx vocals bells footsteps ambience stingers music
/opt/venvs/audio/bin/python unreal/Audio/render/render_all.py --manifest-only  # tables changed, audio did not
/opt/venvs/audio/bin/python -m pytest -q unreal/Audio/render/tests
```

* Inputs: `Data/audio_cues.json` (cue ids, asset names, bus, spatial flag, concurrency), `Data/music.json` (zone
  themes, effects chains, scores, composer tables), `render/data/port_scores.json`, the CC0 recordings in
  `public/assets/audio/bgm/` (decoded gapless: LAME delay/padding trimmed, seam checked).
* Deterministic: every noise / score seed is derived from the asset name; the Ogg stream serial number is too, so a
  re-render of unchanged audio is byte-identical. An export whose decoded samples equal the committed file keeps the
  committed file (no binary churn).
* `--later-recordings` also transcodes the other zones' CC0 recordings (later milestones). The GPL "Desert Battle
  Theme" is not in the repository any more (A8): `scorching_desert_combat.mp3` is the CC0 Junkala track.
* Set `PYTHONDONTWRITEBYTECODE=1 NUMBA_CACHE_DIR=<tmp>` to keep caches out of the tree (they are git-ignored anyway).

### What is rendered (Chapter 1, audio.md §10)

| Kind | Assets | Notes |
|---|---|---|
| SFX (web) | 33 cues, `SW_SFX_<Cue>[_01..04]` | exact web recipes; cues with noise layers get 4 noise variants |
| SFX (A7) | `SW_SFX_MonsterAggro_<Family>_0n`, `SW_SFX_MonsterHurt_<Family>_0n`, `SW_SFX_MonsterDeathVocal_<Family>_0n` | families humanoid / slime (today's goblins and slimes); the death vocal layers on top of `monster_death` |
| SFX (AU1) | the same three cues for the dark remake's voice families `undead` (dry rasp, mail, near-words), `human` (breath, effort, chant fragments), `gnawer` (the goblin set a sixth lower, wet chitter), `breach` (the slime set darkened, sizzle); 2 variants each | `recipes/dark_vox.py`; the family list is `port_sfx.FAMILIES` (manifest order); families audio_cues.json does not list yet are appended to the cue rows under the conventional names |
| SFX (AU1 named) | `SW_SFX_VoxCorpseherd{Bell,Home,Hurt}_0n`, `SW_SFX_VoxCorpseherdLullaby` (17.6 s, hummed once: no SFX cue loops, EvSfx has no stop), `SW_SFX_VoxValdras{Order,Hurt,Plate,Standard}_0n`, `SW_SFX_VoxValdras{Roar,RoarP3,Death,Fire}` (one take each: the intro / P2 roar and the P3 roar are separate cues, since UE picks variants at random) | cue ids `vox_<who>_<what>` (`dark_vox.NAMED_CUES`: variants, level, loop (always false), concurrency, when it plays); `voiceFamilies.overrides` says which family cue each replaces. Every AU1 one-shot (family and named) is cut 20 ms after its tail falls 72 dB under the peak (`dark_vox.trim_tail`) |
| Footsteps (A7) | `SW_SFX_Footstep_{Grass,Dirt,Stone}_01..04` | hero steps, surface from the tile under the hero |
| Ambience (A7) | `SW_AMB_Plains` | 36 s seamless loop (wind, grass, birds); the Ember Tower's bed |
| Ambience (AU2) | `SW_AMB_March` (60 s: cold low wind with two long lulls, crows near and far, a far chapel bell), `SW_AMB_BreachRim` (48 s: wind and the breach hum on D2, the bell's hum), `SW_AMB_GnawedFieldRinging` (40 s layer: a clapper with a lip of Ashwheat's bell shaken like a supper bell far off, gnawers answering), mono emitters `SW_AMB_AshwheatKnock_01..03` (19 / 23 / 29 s: knocks, a fist that rattles the bar, a hand dragged down the door; levelled as one group) and `SW_AMB_KnellwatchHearth` (20 s) | `recipes/dark_ambience.py` (`AMBIENCE`: role, level, manifest row with geometry / condition; `EMITTERS`: attenuation, placement). Every layer carries a bus whose mode makes the seam clean by construction: `crossfade` (continuous noise with periodic automation, equal-power fold), `linear` (tones with whole cycles per loop, coherent fold), `wrap` (events overlap-added onto the start; bus low-pass and reverb run circularly) |
| SFX (AU2 bells) | `SW_SFX_BellToll_01..03`, `SW_SFX_BellTollFar_01..02` (mono), `SW_SFX_BellCracked_01..02` (mono), `SW_SFX_BellSelfRing_01..02`, `SW_SFX_BellDawnToll`, `SW_SFX_BellAmbushTell_01..02` (centred); stereo with their reverb tails, cut 60 dB under the peak (kind `bell`) | cue ids `bell_toll` (heard inside Knellwatch), `bell_toll_far` (the dusk tolls from out on the march), `bell_cracked`, `bell_self_ring`, `bell_dawn_toll`, `bell_ambush_tell` (`dark_ambience.BELL_CUES`: variants, level, spatial, concurrency, when it plays). The keepers' bell is a minor-third church bell, nominal D4 (strike note D3, hum D2); the march bed's far bell is a smaller A4 bell so it is never taken for the ambush tell |
| Music | `menu_explore` (procedural, 64 bars), `emerald_plains_{explore,combat,victory}` (CC0), `emerald_plains_{explore,combat}_score` (the web's procedural plains score, 64 / 32 bars: what the Ember Tower plays, Q8), `abyss_rift_explore` (CC0, prologue) | loops are whole-file (tail wrapped / chain run circularly) |
| Music (AU3) | `knellwatch_lute` (`SW_MUS_KnellwatchLute`, 115.7 s: a sparse, slightly detuned plucked lute of our own in D minor), `march_drones` (128 s: low drones in D out of long silences), `red_field` (98.2 s: a slow dead drum and a wordless men's choir), `boss_ch1` (`SW_MUS_BossCh1`, 83.5 s, reworked for Valdras: war drums, field snare, low horns grinding D-Eb-D-C, a chanting choir, tremolo strings, the cracked bell), `siege` (50.5 s: drums from the field, two columns out of step, far horn calls), `valdras_falls` (one-shot, 9.5 s: replaces the boss victory recording) | `data/dark_scores.json` (`tracks`: a small notation, `ITEM[:DUR][!VEL]`, bar lines checked) played by `recipes/dark_music.py` (instruments: lute, choir, drum, snare, brass, strings, drone, glass, the keepers' bell); buses `wrap` (loops) or `oneshot`, each with its own space (no web chain), a 5 Hz DC blocker; every envelope ends at exact zero |
| Stingers | `SW_STG_ChapterCardDawn`, `SW_STG_BossIntro`, `SW_STG_WhisperLoop` (loop), `SW_STG_CutsceneIn` | audio.md §10.3 recipes through the plains chain |
| Stingers (AU3) | `stinger_hunt` (`SW_STG_Hunt`), `stinger_miniboss` (the Corpseherd: women's voices sing the head of her lullaby), `stinger_slain`, `stinger_valdras_roll_call` / `stinger_valdras_no_retreat` (phase changes P2 / P3), `stinger_chapter_night` (`SW_STG_ChapterCardNight`: the night chapter card, 8.9 s timing) | `dark_scores.json` `stingers` (duck, level match, when it plays); choir and percussion |

### Levels (A3)

* Music: −16 LUFS integrated each, true peak ≤ −1 dBTP (circular look-ahead limiter on loops; none needed so far).
  AU3 tracks carry their own target (`loudnessLufs`): the boss −16, the siege −17, Valdras falls −18, the lute and the
  Red Field −19.5, the drones −21 (sparse pieces sit under the beds' level of activity; their silences are gated out).
  An AU3 track's momentary (400 ms) maximum may sit at most 5.5 LU over its target (`MUSIC_MOMENTARY_CEILING_LU`; the
  level takes the lower gain, `levelledBy: momentaryCeiling`): normalising a sparse piece on its gated loudness would
  otherwise blow its loudest chord up past the boss track. It binds on the lute (−20.5 LUFS-I, momentary max −14).
  `levels.musicTargetsByTrack` lists every track's target. This departs from DECISIONS A3 (−16 for all music); the
  amendment is the DECISIONS owner's to record (hand-off).
* Web SFX: one common gain, so the web's relative mix is kept. It is chosen so that the music/SFX balance at the port's
  default volumes (music 0.6, SFX 0.8) equals the web's at its defaults (0.15 / 0.3), with the plains recording as the
  music reference; capped by the loudest SFX true peak (currently +0.41 dB instead of +0.56 dB).
* A7 cues relative to the web `hit` cue (momentary max): footsteps −15 LU, aggro −3, hurt −5, death vocal −4 (every
  voice family). AU1 named voices: Valdras roar −1, order −2, death −3, fire −4, hurt −5, standard −8, plate −10; the
  Corpseherd's bell −4, 回家吧 −5, hurt −5, lullaby −12 (the roar's P3 take −1 too). AU2 bells: dawn toll −1, toll −3,
  self-ring −3, ambush tell −5, cracked note −7, far toll −9 (one gain per cue, so variants keep their differences). A partial run (`--only vocals` /
  `bells`) levels all web SFX as the reference without exporting them, so it reproduces a full run exactly.
* Ambience −30 LUFS integrated (the plains and march beds, the Breach Rim bed); AU2's Gnawed Field layer −34, the
  hearth −33 (`dark_ambience.AMBIENCE`). The knock emitters are sparse (integrated loudness would follow how often a
  door knocks), so they are levelled as one group like the bells: one gain, mean momentary maximum −4.5 LU under
  `hit`; the fist door (02) peaks 1.8 LU under a combat hit, the other two about 4 LU under it. Stingers 3 LU under the plains explore track
  (short-term max; the 1.2 s letterbox whoosh on the momentary max). AU3 stingers: `levelVsExploreLu` (hunt, phase
  changes −2 on the momentary max; mini-boss and night card −3, slain −4 on the short-term max).
* Runtime volume = master × bus (music / SFX) from the user settings, applied by `UAbyssAudioSystem`.

### Package budget (Chapter 1)

audio.md §9.1 estimated ~10 MB of music and < 1 MB of SFX for the web port. The remake adds AU1-AU3 and keeps the CC0
recordings (the jukebox, the Ember Tower, the stinger loudness reference), so the Chapter 1 budget is set here and
enforced by `tests/test_exports.py` (OGG on disk; resident memory under `ue/import_audio.py`'s rules):

| | Now | Budget |
|---|---|---|
| Music OGG | 20.2 MB (AU3 5.9, CC0 recordings 8.2, web procedural scores 6.1) | 22 MB |
| Ambience OGG | 3.4 MB | 4 MB |
| SFX OGG (web, A7, AU1, bells) | 2.2 MB | 2.5 MB |
| Stingers / Footsteps OGG | 1.0 / 0.07 MB | 1.2 / 0.1 MB |
| All OGG | 26.8 MB | 29 MB |
| Retained (Retain On Load, ADPCM 4 bit) | 1.97 MB (web SFX 0.85, vocals 1.07, steps 0.06) | 2.5 MB |

Primed one-shots (bells, the lullaby, stingers) keep only their first chunk resident; music and ambience stream. Before
the review the bells were retained too (4.2 MB of ADPCM for ten rarely played sounds: 6.7 MB in all).
`SW_MUS_EmeraldPlainsVictory` (0.63 MB) is unreachable in Chapter 1 now that `emerald_plains_victory` maps to
`valdras_falls` (no other Chapter 1 zone has a boss victory, and the jukebox does not list it): it stays packaged until
the A5 owner decides to drop it.

## audio_manifest.json (schemaVersion 1)

| Key | Content |
|---|---|
| `assets.<Name>` | `file` (relative to Export/), `ue` (object path), `kind` (sfx / vocal / bell / footstep / ambience / stinger / music), `channels`, `loop`, `loopStartSample`, `loopEndSample` (whole file), `lengthSec`, `gain` (runtime trim, 1), measured `peakDbfs`, `truePeakDbtp`, `rmsDbfs`, `lufs`, `momentaryMaxLufs`, `seamRatio`, `masterGainDb`, `sha256` of the OGG, render metadata (seed, recipe, chain, `sourceFile` / `sourceSha256` of a recording, credit) |
| `cues.<id>` | audio_cues.json row: `assets`, `variants`, `families`, `bus`, `spatial`, `concurrency {max, rule, minRetriggerMs}`, `gain`; `monster_death.vocalLayer`; `monster_aggro` / `monster_hurt` / the vocal layer extended with every rendered voice family (family-major); AU1 named rows `vox_*` and AU2 bell rows `bell_*` (+ `loop` (false: EvSfx has no stop), `use`, `decision`) until audio_cues.json lists them |
| `voiceFamilies` | AU1: `families` (cue order), `aliases` (design family names → voice family), `monsters` (Chapter 1 monster id → voice family, hunt ids and stretch `ash_outrider` too), `silent` (monster → why it plays no family cue: `false_lantern`), `overrides` (monster → {key → named cue}: `monster_aggro` / `monster_hurt` / `monster_death.vocal` replace the family cue; U7 adds `anim.<action>` (the core's AnimAction names: Valdras's roar and plate on attacks, the Corpseherd's handbell on attacks / casts), `miniboss.opened` (her lullaby) and `story.say` (Valdras's boss-intro lines)), `overrideKeys` (what each key means; `dark_vox.OVERRIDE_KEYS`) |
| `plannedCues` | the design's non-vocal monster sounds no AU package renders yet (monsters.md 2 "Sound" rows): `sfx_<monster>_<what>` → {`monsters`, `sound`, `wave` (W1 / W2 / stretch), `spatial`, `bus`, `owner`}; ids for the core to reserve, no assets (`dark_vox.PLANNED_CUES`) |
| `music.<trackKey>` | `asset`, `loop`, `lengthSec`, `gain`, `source` (score / recording / darkScore), `credit {title, author, license, url}`; AU3 rows add `use`, `decision` |
| `menuTrack` | the title theme track key |
| `zoneOverrides` | zone → {core track key → track played instead} (audio.md §11; `ember_tower` plays the procedural plains score; AU3: `emerald_plains` plays `march_drones` for explore **and combat** (released by U7: a zone-overridden request resolving to the track already sounding is a no-op) and `valdras_falls` for the boss victory) |
| `stingers` | chapter card by mood (AU3 adds `night` → `SW_STG_ChapterCardNight`), boss intro, whisper, cutscene-in; duck amounts (dB, in / out s); fades |
| `musicDirector` | AU3 (`decision`), from `dark_scores.json` `director`, read by U7 (`AbyssMusicRules`): `priority` (story > victory > boss > siege > region > zone), `zoneTracks` (explore / combat / victory), `heldOverrides` (only when a state waits for a reader; none since U7), `combat` (keep), `tracks` (key → asset), `regions[]` {`id`, `col`, `row`, `radius`, `exitRadius`, `track` ("" = silence), `while {monsterAlive}`, fades, `source` (design / AU3 proposal), `use`} (breach_rim silence, knellwatch lute, red_field dirge, ashwheat and sheepfold silences), `regionRule` (Euclidean tile distance from the hero's tile, enter at `radius`, leave past `exitRadius`), `bosses.valdras` {`track` boss_ch1, `phaseStingers` {phase index → stinger}, `victory`}, `encounters` (hunts, mini-boss: engaged / slain stingers), `story[]` (chapter card; siege: `track` siege while `quest` q_rf_last_toll is active after its beat, `then` silence until the turn-in, own fades; finale: from `stepIndex` 0 (the 0-based EvStoryStep index of "step 1") silence over 3 s, then 6 s after the beat the lute over 6 s), `stingers.<id>` {`asset`, `lengthSec`, `duck {db, inSec, outSec}`, `use`} |
| `ambience` | `themes` / `zones` (string → asset: the bed per map theme and per zone; AU2: `march` and `emerald_plains` → `SW_AMB_March`, `ember_tower` keeps `SW_AMB_Plains`), fades, cinematic duck; AU2 (`decision`): `regions.<regionId>` {`asset`, `zone`, `mapRegion` (the map's `regions[]` id), `col`, `row`, `radius`, `exitRadius`, `replacesBed`, fades, `use`} (`breach_rim`; no `while`: always), `layers.<id>` {`asset`, `zone`, `col`, `row`, `radius`, `exitRadius`, `while {monsterAlive}`, `untilFlag` (W2), fades, `use`} (`gnawed_field_ringing`), `emitters.<id>` {`assets` (one per placed emitter, neighbours different), `spatial`, `attenuation {innerTiles, falloffTiles}`, `placement` (`area` or `anchor` tile, set piece or camp slot), `use`} (`ashwheat_knock`, `knellwatch_hearth`), `regionRule` (as the music director's), `emitterAttenuationRule` (full volume inside `innerTiles`, linear to silence over `falloffTiles`) |
| `cueTriggers` | U7 (`decision`): `rows[]` of `dark_ambience.BELL_TRIGGERS` + `dark_vox.VOX_TRIGGERS` (a row whose cue is not rendered is left out): {`id`, `cue`, `on` (`storyEnd` {beat}, `storyStep` {beat, index}, `storySlide` {sequence, index}, `randomEvent` {event, zone}, `decorFocus` {decor}, `every` {everySec [lo, hi], zone, whileQuestActive?, within? {col, row, radius}}), `farCue` + `nearRegion` (outside that musicDirector region the far toll plays), `tile` (a 3d cue's position), `count` + `spacingSec` (a burst), `use`}; `kinds`, `rule`. Indices are the core's 0-based EvStoryStep::index (the bell `use` prose counts from 1) |
| `footsteps` | variants per surface, tile type → surface, notify names, fallback stride, timings |
| `spatial` | A4 spread (0.25), play-area half width (800 cm), virtual emitter distance |
| `jukebox` | title-menu soundtrack rows `{titleKey, track}` (packaged tracks only), fades |
| `levels`, `defaults`, `codec` | how the renders were levelled; default volumes |

Every field has a default in `FAbyssAudioManifest::MakeDefaults` (core cue table + naming conventions), so a build
without the manifest still plays; lengths then come from the waves themselves.

## Import into UE

After building `AbyssfireEditor`:

```sh
UnrealEditor-Cmd <repo>/unreal/Abyssfire.uproject -run=pythonscript \
  -script="<repo>/unreal/Audio/ue/import_audio.py" -unattended -nosplash -stdout -FullStdOutLogOutput
```

Content's `Scripts/build_content.py` can call it as a module (`run()`, see its docstring). It imports each OGG
(UE 5 imports .ogg natively) into `/Game/Abyssfire/Audio/<Folder>/<Name>`, skips unchanged sources (SHA-256 in the
asset metadata tag `AbyssSourceSha256`), verifies length and channel count against the manifest and sets:

| Kind | Looping | Loading behaviour | Compression | Virtualisation |
|---|---|---|---|---|
| sfx / vocal / footstep | manifest (no SFX loops) | Retain On Load | ADPCM (instant start) | default |
| the same, longer than 4 s (the lullaby) | manifest | Prime On Load | platform specific, q80 | default |
| bell (1-14 s with their tails) | manifest | Prime On Load (first chunk resident: still starts at once) | platform specific, q80 | default |
| stinger | manifest (whisper loops) | Prime On Load | platform specific, q80 | Play When Silent |
| music | manifest (victory is one-shot) | Load On Demand (stream) | platform specific, q80 | Play When Silent |
| ambience | yes | Load On Demand | platform specific, q70 | Play When Silent |

`--prune` deletes SoundWaves the manifest no longer lists; `--force` re-imports everything; `--dry-run` reports.
No Sound Class / Mix / Concurrency assets are needed: volumes, ducking and concurrency are applied in code.

## Runtime: `UAbyssAudioSystem` (game instance subsystem)

The core decides **what** plays (`AudioDirector` / `MusicDirector` emit `EvMusic`, `EvSfx`); the subsystem decides
**how** (assets from the manifest, fades, concurrency, panning, volumes, lifecycle). It never applies game rules.

* **Music** (`EvMusic`): two crossfading persistent 2D UI-sound components (keep playing while paused / in
  cinematics). Linear fade-out / fade-in with the event's times; a voice still fading when a third request comes is
  stopped at once (FIX Q4). `restart = false` resumes a track where it was last heard (A2: explore after a fight, boss,
  victory); the debounce, boss precedence and the 8 s boss-victory hold (A5) are the core's. The manifest's
  `zoneOverrides` swap a key for the zone in the snapshot (Ember Tower → procedural plains score). The title theme is played
  on `MainMenu` (no session there) and restarts on every return to the menu.
* **AU3 director (U7, `Source/Abyssfire/Private/Audio/AbyssMusicRules.h`)**: EvMusic is no longer played as it
  arrives; it joins a per-frame arbitration with the `musicDirector` layers in their priority (story > victory > boss >
  siege > region > zone). Regions: the hero's tile picks the smallest region that holds it (enter at `radius`, leave
  past `exitRadius`) whose `while` holds (`""` = silence: the Breach Rim while Valdras lives, Ashwheat, the Sheepfold
  while the Corpseherd lives; Knellwatch the lute, the Red Field the dirge). The siege track plays while
  q_rf_last_toll is active (after its beat), then silence until the turn-in; cs_rf_finale holds silence from its first
  step, then the lute 6 s after the beat. Victory and boss requests beat the siege and the regions; a story sequence
  plays the core's track; a cutscene freezes the layers (the core holds its changes too). A zone-overridden request
  that resolves to the track already sounding is a no-op even with `restart = true`, so the zone's combat row maps to
  the drones (the remake has no combat music: a fight keeps whatever plays). Other zones: the core's requests play as
  before. Encounter stingers (`encounters`, `stingers.<id>` duck): `stinger_hunt` on a hunt's first aggro,
  `stinger_miniboss` when the Corpseherd's pre-fight lines close, `stinger_slain` on their death, on their own
  component (music volume) with their own duck. Phase stingers wait for W2's `EvMonsterPhase`.
* **Cue triggers (U7, `AbyssCueTriggerRules.h`; manifest `cueTriggers`)**: the prologue's last slide (ten tolls 2.8 s
  apart, stopped with the prologue), a toll at the end of cs_rf_brand / cs_rf_whisper (`bell_toll` inside the
  knellwatch region, else `bell_toll_far`), the self-ring at cs_rf_siege's step 2 and every 20-30 s while the siege
  lasts, the dawn toll at cs_rf_finale's step 4, the ambush tell when an ambush starts in emerald_plains (the core raises
  the hands at once: no 1.5 s lead), the split bell when the hero reaches story_rf_belfry and every 35-60 s within 10
  tiles of it, the Corpseherd's single ring at cs_rf_herd's step 3. `UAbyssAudioSystem::OnCuePlayed(CueId)` reports
  each (the set-piece layer can ripple the bell frame). The dusk tolls wait for a day clock (none in W1).
* **SFX** (`EvSfx`): per-cue concurrency (max voices, stop oldest, minimum retrigger 20 ms for hit / crit / miss), a
  random variant never repeated twice in a row, the source monster's family for A7 cues (+ the death vocal layer on
  `monster_death`). U7 (`AbyssVoiceRules.h`): the family is `voiceFamilies.monsters[defId]` (the Chapter 1 roster and
  hunt ids), else the design family through `aliases` (once the data exports it), else the animCategory (humanoid /
  slime and every later chapter, as before); `silent` monsters play no family cue; `overrides` swap in the named cues
  (Valdras, the Corpseherd) and add them on beats (`anim.<action>` on EvPlayAnim, `miniboss.opened`, `story.say`); a
  named cue without a manifest row falls back to the family cue. World cues (`spatial`) get the A4 mild panning: the source's lateral offset from the hero across
  the screen × 0.25 spread becomes the azimuth of a virtual emitter 1 m from the engine's audio listener, with no
  distance attenuation; UI cues are 2D.
* **Story** (audio.md §10.2): letterbox whoosh when a cutscene / boss intro begins; chapter-card stinger by the
  card's mood (ducks music −8 dB); boss-intro title stinger (−10 dB during the title step); whisper bed looping while
  whisper steps show (−6 dB). The prologue's music change is the core's (`PlayTrack` under the story lock).
* **Ambience** (A7): bed by zone id, else by map theme (U7: the map's presentation theme when it has one); 2.5 s
  fade-in at a random offset, 1.5 s fade-out on zone exit, −6 dB during cinematics. U7 (`AbyssAmbienceRules.h`): the
  Breach Rim's region bed crossfades with the bed (weights over its fades, the bed scaled by 1 − weight), the Gnawed
  Field's ringing plays over the bed while hunt_hungerbell lives, the emitters loop at their spots (Ashwheat's doors
  from `SetZoneEmitterSpots`, else the area centre; Knellwatch's hearth at camp 1's campfire slot) with full volume
  inside `innerTiles` and linear silence over `falloffTiles`, panned like the A4 world cues.
* **Footsteps** (A7): the hero actor's `FootL` / `FootR` anim notifies (`AAbyssCharacterActor::OnCharacterNotify`)
  with the surface of the tile under the hero; a distance cadence (116.7 cm stride) covers clips without notifies.
* **Volumes**: `FAbyssUserSettings` master × music / SFX (ambience and footsteps on the SFX volume, stingers on the
  music volume), re-read every frame; `PreviewSfxVolume()` gives slider feedback.
* **Lifecycle**: phones / tablets pause everything on deactivate / background and resume on foreground; desktop keeps
  playing (the engine's unfocused volume multiplier applies, see below).
* **Title-menu soundtrack** (audio.md §8.3, FIX Q10): `GetJukeboxTracks()` (packaged rows, real lengths),
  `JukeboxPlay(i)`, real `JukeboxSetPaused` / `JukeboxSeek`, auto-advance, hold at the end of the last row,
  `JukeboxClose()` → title theme. `GetMusicCredits()` lists the CC0 recordings for the credits screen.
* **UI API** (UI agent): `UAbyssAudioSystem::Get(this)->PlayClick()` / `PlayPanelOpen()` / `PlayPanelClose()` /
  `PlayError()` / `PlayUiCue(abyss::SfxId)` — one click per panel toggle (FIX Q2); core-owned panels' cues come from
  the core.
* **Console**: `abyss.Audio.Status`, `abyss.Audio.Music <trackKey> [resume]`, `abyss.Audio.Cue <cueId> [3d]`.

### Platform settings this relies on (backbone-owned files)

* `Config/DefaultEngine.ini`: `[Audio]` `UnfocusedVolumeMultiplier=0.0` — desktop mutes while the window is unfocused
  (the web muted on a hidden tab, audio.md §9.7); raise it (e.g. 0.3) if quiet background play is preferred.
* `Source/Abyssfire/Abyssfire.Build.cs`: stage the manifest with the other runtime data:
  `RuntimeDependencies.Add("$(ProjectDir)/Audio/Export/audio_manifest.json", StagedFileType.UFS);` — without it a
  packaged build runs on the defaults (same assets, lengths read from the waves).
* iOS audio session: UE's default (Solo Ambient, Ambient while another app plays music) respects the silent switch, as
  Web Audio did in mobile Safari; verify on device during bring-up.

## Licences

Procedural music, SFX, stingers, ambience and footsteps are generated by this repository's code. The recordings are CC0
(OpenGameArt; `public/assets/audio/bgm/CREDITS.md`): GrassLands Theme (DST), Battle Theme A (cynicmusic), Medieval:
Victory Theme (RandomMind), Loopable Dungeon Ambience (JaggedStone). The manifest carries each credit.

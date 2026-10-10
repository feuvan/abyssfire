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
                      chain, loops, mp3, master, report) + recipes/ (web SFX, A7 cues, story stingers)
  data/               port_scores.json (boss_ch1 score, A5), sfx_recipes.json (every recipe as JSON, generated)
  tests/              pytest: Web Audio maths, composer golden vectors, chord tables, loudness, Ogg export
Export/               committed renders (OGG Vorbis q6, 48 kHz) + audio_manifest.json      (DECISIONS A1)
  SFX/ Footsteps/     mono one-shots          Music/ Ambience/ Stingers/   stereo
Reports/              waveform + spectrum sheets per kind (*.png) and stats.md (no listening test is possible where
                      the renders are produced: check these)
ue/import_audio.py    editor Python: imports Export/ into /Game/Abyssfire/Audio with the playback settings
```

## Rendering

```sh
python3 -m venv /opt/venvs/audio && /opt/venvs/audio/bin/pip install -r unreal/Audio/render/requirements.txt
/opt/venvs/audio/bin/python unreal/Audio/render/render_all.py --reports      # everything, ~4 min
/opt/venvs/audio/bin/python unreal/Audio/render/render_all.py --only stingers  # sfx vocals footsteps ambience stingers music
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
| SFX (A7) | `SW_SFX_MonsterAggro_<Family>_0n`, `SW_SFX_MonsterHurt_<Family>_0n`, `SW_SFX_MonsterDeathVocal_<Family>_0n` | families humanoid / slime (all Chapter 1 monsters); the death vocal layers on top of `monster_death` |
| Footsteps (A7) | `SW_SFX_Footstep_{Grass,Dirt,Stone}_01..04` | hero steps, surface from the tile under the hero |
| Ambience (A7) | `SW_AMB_Plains` | 36 s seamless loop (wind, grass, birds) |
| Music | `menu_explore` (procedural, 64 bars), `emerald_plains_{explore,combat,victory}` (CC0), `emerald_plains_{explore,combat}_score` (the web's procedural plains score, 64 / 32 bars: what the Ember Tower plays, Q8), `boss_ch1` (procedural, 32 bars, A5), `abyss_rift_explore` (CC0, prologue) | loops are whole-file (tail wrapped / chain run circularly) |
| Stingers | `SW_STG_ChapterCardDawn`, `SW_STG_BossIntro`, `SW_STG_WhisperLoop` (loop), `SW_STG_CutsceneIn` | audio.md §10.3 recipes through the plains chain |

### Levels (A3)

* Music: −16 LUFS integrated each, true peak ≤ −1 dBTP (circular look-ahead limiter on loops; none needed so far).
* Web SFX: one common gain, so the web's relative mix is kept. It is chosen so that the music/SFX balance at the port's
  default volumes (music 0.6, SFX 0.8) equals the web's at its defaults (0.15 / 0.3), with the plains recording as the
  music reference; capped by the loudest SFX true peak (currently +0.41 dB instead of +0.56 dB).
* A7 cues relative to the web `hit` cue (momentary max): footsteps −15 LU, aggro −3, hurt −5, death vocal −4.
* Ambience −30 LUFS. Stingers 3 LU under the plains explore track (short-term max; the 1.2 s letterbox whoosh on the
  momentary max).
* Runtime volume = master × bus (music / SFX) from the user settings, applied by `UAbyssAudioSystem`.

## audio_manifest.json (schemaVersion 1)

| Key | Content |
|---|---|
| `assets.<Name>` | `file` (relative to Export/), `ue` (object path), `kind` (sfx / vocal / footstep / ambience / stinger / music), `channels`, `loop`, `loopStartSample`, `loopEndSample` (whole file), `lengthSec`, `gain` (runtime trim, 1), measured `peakDbfs`, `truePeakDbtp`, `rmsDbfs`, `lufs`, `momentaryMaxLufs`, `seamRatio`, `masterGainDb`, `sha256` of the OGG, render metadata (seed, recipe, chain, `sourceFile` / `sourceSha256` of a recording, credit) |
| `cues.<id>` | audio_cues.json row: `assets`, `variants`, `families`, `bus`, `spatial`, `concurrency {max, rule, minRetriggerMs}`, `gain`; `monster_death.vocalLayer` |
| `music.<trackKey>` | `asset`, `loop`, `lengthSec`, `gain`, `source` (score / recording), `credit {title, author, license, url}` |
| `menuTrack` | the title theme track key |
| `zoneOverrides` | zone → {core track key → track played instead} (audio.md §11; `ember_tower` plays the procedural plains score) |
| `stingers` | chapter card by mood, boss intro, whisper, cutscene-in; duck amounts (dB, in / out s); fades |
| `ambience` | bed per zone and per map theme, fades, cinematic duck |
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
| sfx / vocal / footstep | no | Retain On Load | ADPCM (instant start) | default |
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
* **SFX** (`EvSfx`): per-cue concurrency (max voices, stop oldest, minimum retrigger 20 ms for hit / crit / miss), a
  random variant never repeated twice in a row, the source monster's family for A7 cues (+ the death vocal layer on
  `monster_death`). World cues (`spatial`) get the A4 mild panning: the source's lateral offset from the hero across
  the screen × 0.25 spread becomes the azimuth of a virtual emitter 1 m from the engine's audio listener, with no
  distance attenuation; UI cues are 2D.
* **Story** (audio.md §10.2): letterbox whoosh when a cutscene / boss intro begins; chapter-card stinger by the
  card's mood (ducks music −8 dB); boss-intro title stinger (−10 dB during the title step); whisper bed looping while
  whisper steps show (−6 dB). The prologue's music change is the core's (`PlayTrack` under the story lock).
* **Ambience** (A7): bed by zone id, else by map theme; 2.5 s fade-in at a random offset, 1.5 s fade-out on zone
  exit, −6 dB during cinematics.
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

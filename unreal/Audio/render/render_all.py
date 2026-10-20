#!/usr/bin/env python3
"""Render every Chapter-1 audio asset, master it and export OGG Vorbis q6 + audio_manifest.json (DECISIONS A1, A3,
A5, A7; audio.md 9, 10; dark remake AU1: the voice families and named voices of recipes/dark_vox.py, group `vocals`;
AU2: the beds, region bed, layer and emitters of recipes/dark_ambience.py, group `ambience`, and its bells, group
`bells`; AU3: the scores of data/dark_scores.json played by recipes/dark_music.py, groups `music` (tracks, boss_ch1
reworked for Valdras) and `stingers`, and the music director table).

    /opt/venvs/audio/bin/python unreal/Audio/render/render_all.py            # Chapter 1 (default)
    ... render_all.py --only sfx,stingers                                     # subsets: sfx vocals bells footsteps
                                                                              #          ambience stingers music
    ... render_all.py --later-recordings                                      # + the other zones' CC0 recordings
    ... render_all.py --reports                                               # + waveform / spectrum PNGs
    ... render_all.py --manifest-only                                         # rebuild the manifest only

Outputs (relative to unreal/Audio):
    Export/<Folder>/<Asset>.ogg          SFX (incl. A7 vocals), Footsteps (mono); Ambience, Stingers, Music (stereo); 48 kHz
    Export/audio_manifest.json           cue / track / stinger / ambience / footstep tables for UAbyssAudioSystem
    render/data/sfx_recipes.json         every recipe as engine-agnostic JSON (audio.md 11)
    Reports/*.png, Reports/stats.md      verification (no listening test is possible where this runs)

Levels (A3): music is normalised to -16 LUFS integrated (true peak <= -1 dBTP, circular look-ahead limiting where a
render would exceed it). All web SFX share ONE gain (their relative web mix is kept) chosen so that the music / SFX
balance at the new default volumes (music 0.6, SFX 0.8) equals the web's at its defaults (0.15, 0.3) with the zone
recording as the music reference: g = (0.3 / 0.15) * (0.6 / 0.8) * 10^((-16 - L_web) / 20). The A7 cues are set
relative to the web `hit` cue (AU1's named voices and AU2's bells too, each with its own offset in
dark_vox.NAMED_CUES / dark_ambience.BELL_CUES); stingers 3 LU (short-term max) under the plains explore track (AU3's
at their own offset, `levelVsExploreLu`); the ambience beds at -30 LUFS integrated (AU2's layer and hearth at their
own targets, dark_ambience.AMBIENCE; the sparse knock emitters as one group relative to `hit`, like the bells); AU3's
tracks at their own integrated target (`loudnessLufs`: the boss -16 as A3, the sparse pieces lower), their momentary
maximum at most MUSIC_MOMENTARY_CEILING_LU over it.

A rendered group owns its kind: a partial run drops (and deletes the export of) an asset of that kind the tables no
longer produce, so it leaves what a full run would. No SFX cue loops (EvSfx has no stop).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import pathlib
import sys
import time
import zlib

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from abyss_audio import SAMPLE_RATE as SR  # noqa: E402
from abyss_audio import chain, composer, loops, master, mp3, sfx  # noqa: E402
from abyss_audio.recipes import dark_ambience, dark_music, dark_vox, port_sfx, stingers, web_sfx  # noqa: E402
from abyss_audio.score import ScoreSpec  # noqa: E402

AUDIO_ROOT = HERE.parent
UNREAL = AUDIO_ROOT.parent
REPO = UNREAL.parent
DATA = UNREAL / "Data"
EXPORT = AUDIO_ROOT / "Export"
REPORTS = AUDIO_ROOT / "Reports"
BGM = REPO / "public" / "assets" / "audio" / "bgm"
UE_ROOT = "/Game/Abyssfire/Audio"

MUSIC_LUFS = -16.0
# A track's momentary (400 ms) maximum may sit at most this far over its integrated target: a sparse piece normalised
# on its gated loudness would otherwise blow its loudest chord up past the boss music (the level takes the lower gain).
MUSIC_MOMENTARY_CEILING_LU = 5.5
TRUE_PEAK_DBTP = -1.0
AMBIENCE_LUFS = -30.0
STINGER_UNDER_EXPLORE_LU = 3.0
WEB_VOLUMES = {"music": 0.15, "sfx": 0.3}
PORT_VOLUMES = {"music": 0.6, "sfx": 0.8}
# A7 cue levels relative to the web `hit` cue (momentary max loudness, LU).
A7_RELATIVE_LU = {"footstep": -15.0, "aggro": -3.0, "hurt": -5.0, "death": -4.0}

# Track key -> (asset, source) for Chapter 1 (audio.md 10.1). Key = "<themeId>_<state>" or the boss score id.
CH1_MUSIC = [
    {"key": "menu_explore", "asset": "SW_MUS_MenuExplore", "kind": "score", "score": "menu", "state": "explore",
     "bars": 64, "seed": "menu-1", "theme": "menu", "loop": True},
    {"key": "emerald_plains_explore", "asset": "SW_MUS_EmeraldPlainsExplore", "kind": "recording",
     "file": "emerald_plains_explore.mp3", "theme": "emerald_plains", "loop": True,
     "credit": {"title": "GrassLands Theme", "author": "DST", "license": "CC0",
                "url": "https://opengameart.org/content/grasslands-theme"}},
    {"key": "emerald_plains_combat", "asset": "SW_MUS_EmeraldPlainsCombat", "kind": "recording",
     "file": "emerald_plains_combat.mp3", "theme": "emerald_plains", "loop": True,
     "credit": {"title": "Battle Theme A", "author": "cynicmusic", "license": "CC0",
                "url": "https://opengameart.org/content/battle-theme-a"}},
    {"key": "emerald_plains_victory", "asset": "SW_MUS_EmeraldPlainsVictory", "kind": "recording",
     "file": "emerald_plains_victory.mp3", "theme": "emerald_plains", "loop": False,
     "credit": {"title": "Medieval: Victory Theme", "author": "RandomMind", "license": "CC0",
                "url": "https://opengameart.org/content/medieval-victory-theme"}},
    # boss_ch1 (A5) is reworked for Valdras by AU3: it is a dark score now (data/dark_scores.json, dark_entries); the
    # web-engine spec in port_scores.json is the A5 record and is no longer rendered.
    # The web's procedural plains score (what the Ember Tower plays in the web, QUIRK Q8 / audio.md 11 zoneOverrides:
    # ember_tower -> emerald_plains_explore_score). Explore loops on 64 bars (odd-cycle rest bars), combat on 32.
    {"key": "emerald_plains_explore_score", "asset": "SW_MUS_EmeraldPlainsExploreScore", "kind": "score",
     "score": "emerald_plains", "state": "explore", "bars": 64, "seed": "emerald_plains-explore-1",
     "theme": "emerald_plains", "loop": True},
    {"key": "emerald_plains_combat_score", "asset": "SW_MUS_EmeraldPlainsCombatScore", "kind": "score",
     "score": "emerald_plains", "state": "combat", "bars": 32, "seed": "emerald_plains-combat-1",
     "theme": "emerald_plains", "loop": True},
    {"key": "abyss_rift_explore", "asset": "SW_MUS_AbyssRiftExplore", "kind": "recording",
     "file": "abyss_rift_explore.mp3", "theme": "abyss_rift", "loop": True,
     "credit": {"title": "Loopable Dungeon Ambience", "author": "JaggedStone", "license": "CC0",
                "url": "https://opengameart.org/content/loopable-dungeon-ambience"}},
]

# Later milestones (opt-in with --later-recordings). All CC0 per public/assets/audio/bgm/CREDITS.md; the GPL "Desert
# Battle Theme" (A8) is no longer in the repository: scorching_desert_combat.mp3 is byte-identical to the CC0 Junkala
# track used for anvil_mountains_combat.
LATER_RECORDINGS = [
    ("twilight_forest", "explore", "Dark Forest Theme", "cynicmusic", "https://opengameart.org/content/dark-forest-theme", True),
    ("twilight_forest", "combat", "Grizzly Dwarf Battle LOOP", "Zane Little Music", "https://opengameart.org/content/glizzy-elf-forest-rpg-music-pack", True),
    ("twilight_forest", "victory", "Grizzly Dwarf Battle Victory LOOP", "Zane Little Music", "https://opengameart.org/content/glizzy-elf-forest-rpg-music-pack", False),
    ("anvil_mountains", "explore", "Fantasy Choir 2", "Cesar da Rocha", "https://opengameart.org/content/fantasy-choir-3-orchestral-pieces", True),
    ("anvil_mountains", "combat", "Epic Boss Battle [Seamlessly Looping]", "Juhani Junkala", "https://opengameart.org/content/boss-battle-music", True),
    ("anvil_mountains", "victory", "Victory Fanfare Short", "cynicmusic", "https://opengameart.org/content/victory-fanfare-short", False),
    ("scorching_desert", "explore", "Desert Theme", "Tarush Singhal", "https://opengameart.org/content/desert-theme-0", True),
    ("scorching_desert", "combat", "Epic Boss Battle [Seamlessly Looping]", "Juhani Junkala", "https://opengameart.org/content/boss-battle-music", True),
    ("scorching_desert", "victory", "Medieval: Victory Theme", "RandomMind", "https://opengameart.org/content/medieval-victory-theme", False),
    ("abyss_rift", "combat", "Battle Theme B", "cynicmusic", "https://opengameart.org/content/battle-theme-b-for-rpg", True),
    ("abyss_rift", "victory", "Victory Fanfare Short", "cynicmusic", "https://opengameart.org/content/victory-fanfare-short", False),
]

# zone -> {core track key -> track key played instead} (audio.md 11 music_tracks.json zoneOverrides).
ZONE_OVERRIDES = {
    "ember_tower": {"emerald_plains_explore": "emerald_plains_explore_score",
                    "emerald_plains_combat": "emerald_plains_combat_score"},
}


def zone_overrides(dark: dict) -> dict:
    """ZONE_OVERRIDES + AU3: the remake's zone plays the dark tracks for the core's `<theme>_<state>` keys (the drones
    for explore and combat, Valdras falls for the boss victory). The Ember Tower keeps its plains score. A state listed
    in `director.heldOverrides` stays out until the player can take it (`musicDirector.zoneTracks` keeps it). The combat
    row was held until U7: UAbyssAudioSystem now treats a zone-overridden request that resolves to the track already
    sounding as a no-op even with restart = true, so the combat edge does not restart the drones."""
    d = dark["director"]
    held = d.get("heldOverrides", {})
    out = {z: dict(t) for z, t in ZONE_OVERRIDES.items()}
    out.setdefault(d["zone"], {}).update({f"{d['zone']}_{state}": track for state, track in d["zoneTracks"].items()
                                          if state not in held})
    return out

# Title-menu soundtrack (web JUKEBOX_TRACKS order, MenuScene.ts:30-42): i18n title key -> track key. Rows whose track
# is not rendered (later chapters) are left out of the manifest.
JUKEBOX = [
    ("menu.jukebox.track.menu", "menu_explore"),
    ("menu.jukebox.track.emerald_plains.explore", "emerald_plains_explore"),
    ("menu.jukebox.track.emerald_plains.combat", "emerald_plains_combat"),
    ("menu.jukebox.track.twilight_forest.explore", "twilight_forest_explore"),
    ("menu.jukebox.track.twilight_forest.combat", "twilight_forest_combat"),
    ("menu.jukebox.track.anvil_mountains.explore", "anvil_mountains_explore"),
    ("menu.jukebox.track.anvil_mountains.combat", "anvil_mountains_combat"),
    ("menu.jukebox.track.scorching_desert.explore", "scorching_desert_explore"),
    ("menu.jukebox.track.scorching_desert.combat", "scorching_desert_combat"),
    ("menu.jukebox.track.abyss_rift.explore", "abyss_rift_explore"),
    ("menu.jukebox.track.abyss_rift.combat", "abyss_rift_combat"),
]

def dark_entries(dark: dict) -> list[dict]:
    """AU3 tracks (data/dark_scores.json `tracks`) as music entries: key = the track id."""
    return [{"key": key, "asset": spec["asset"], "kind": "dark", "loop": bool(spec.get("loop", False)),
             "use": spec["use"]} for key, spec in dark["tracks"].items()]


STINGERS = [
    # role, asset, recipe fn, loop
    ("chapterCard.dawn", "SW_STG_ChapterCardDawn", stingers.chapter_card_dawn, False),
    ("bossIntro", "SW_STG_BossIntro", stingers.boss_intro, False),
    ("whisper", "SW_STG_WhisperLoop", stingers.whisper_loop, True),
    ("cutsceneIn", "SW_STG_CutsceneIn", stingers.cutscene_in, False),
]


# render group (--only) -> the asset kind it renders (every asset of that kind).
GROUP_KIND = {"music": "music", "sfx": "sfx", "vocals": "vocal", "bells": "bell", "footsteps": "footstep",
              "ambience": "ambience", "stingers": "stinger"}


def pascal(s: str) -> str:
    return "".join(w[:1].upper() + w[1:] for w in s.replace("-", "_").split("_") if w)


def compact_json(obj, width: int = 118) -> str:
    """Indented JSON where every array / object that fits in `width` columns stays on one line (automation events,
    short layers): the recipe dump stays reviewable."""

    def fmt(o, ind: int, lead: int) -> str:
        flat = json.dumps(o)
        if not isinstance(o, (dict, list)) or not o or ind + lead + len(flat) <= width:
            return flat
        pad = " " * (ind + 1)
        if isinstance(o, dict):
            body = ",\n".join(f"{pad}{json.dumps(k)}: {fmt(v, ind + 1, len(json.dumps(k)) + 2)}" for k, v in o.items())
            return "{\n" + body + "\n" + " " * ind + "}"
        return "[\n" + ",\n".join(pad + fmt(v, ind + 1, 0) for v in o) + "\n" + " " * ind + "]"

    return fmt(obj, 0, 0)


def str_seed(s: str) -> int:
    return zlib.crc32(s.encode("utf-8")) & 0xFFFFFFFF


def log(msg: str) -> None:
    print(msg, flush=True)


class Asset:
    def __init__(self, name: str, folder: str, kind: str, audio: np.ndarray, loop: bool, meta: dict):
        self.name = name
        self.folder = folder
        self.kind = kind
        self.audio = audio
        self.loop = loop
        self.meta = meta
        self.gain_db = 0.0
        self.limiter_gr_db = 0.0
        self.stats: dict = {}

    @property
    def channels(self) -> int:
        return int(self.audio.shape[0])


def fade_tail(x: np.ndarray, sec: float = 0.05) -> np.ndarray:
    n = min(x.shape[1], int(sec * SR))
    if n > 0:
        x = x.copy()
        x[:, -n:] *= np.cos(np.linspace(0, math.pi / 2, n)) ** 2
    return x


# ---------------------------------------------------------------------------------------------------------------------
# Rendering
# ---------------------------------------------------------------------------------------------------------------------
def render_sfx(cue_table: dict, recipes_dump: dict) -> list[Asset]:
    web = web_sfx.recipes()
    out = []
    for cue in cue_table["cues"]:
        cid = cue["id"]
        if cid not in web:
            continue  # A7 cues are rendered by render_vocals
        r = web[cid]
        recipes_dump[cid] = r.to_json()
        assets = cue["assets"]
        if r.has_noise != (len(assets) > 1):
            log(f"  note: {cid}: {len(assets)} asset(s) in audio_cues.json, noise layers: {r.has_noise}")
        for v, name in enumerate(assets, start=1):
            seed = sfx.seed_for(name, v)
            y = sfx.render(r, SR, seed)
            out.append(Asset(name, "SFX", "sfx", y, False,
                             {"cue": cid, "variant": v, "seed": seed, "recipe": cid, "source": r.source}))
    return out


VOCAL_CUES = {"monster_aggro": ("aggro", "MonsterAggro"), "monster_hurt": ("hurt", "MonsterHurt")}
DEATH_VOCAL_VARIANTS = 2


def family_assets(cue: dict) -> tuple[list[str], list[str]]:
    """(families, assets) of an A7 family cue: audio_cues.json's own rows first, then every other voice family of
    port_sfx.FAMILIES (the dark remake's, AU1) under the conventional names, `variants` per family, family-major
    (UAbyssAudioSystem plays the source family's block of `variants` assets)."""
    kind, stem = VOCAL_CUES[cue["id"]]
    families = list(cue.get("families", []))
    assets = list(cue["assets"])
    for family in port_sfx.FAMILIES:
        if family not in families:
            families.append(family)
            assets += [f"SW_SFX_{stem}_{pascal(family)}_{v:02d}" for v in range(1, cue["variants"] + 1)]
    return families, assets


def death_vocal_assets(family: str) -> list[str]:
    return [f"SW_SFX_MonsterDeathVocal_{pascal(family)}_{v:02d}" for v in range(1, DEATH_VOCAL_VARIANTS + 1)]


def named_assets(cue_id: str) -> list[str]:
    """dark_vox.NAMED_CUES / dark_ambience.BELL_CUES: `SW_SFX_<PascalCue>_0n`, or `SW_SFX_<PascalCue>` for a
    single-variant cue."""
    n = (dark_vox.NAMED_CUES.get(cue_id) or dark_ambience.BELL_CUES[cue_id])[1]
    stem = f"SW_SFX_{pascal(cue_id)}"
    return [stem] if n == 1 else [f"{stem}_{v:02d}" for v in range(1, n + 1)]


def render_vocals(cue_table: dict, recipes_dump: dict) -> list[Asset]:
    out = []
    for cue in cue_table["cues"]:
        if cue["id"] not in VOCAL_CUES:
            continue
        kind = VOCAL_CUES[cue["id"]][0]
        for name in family_assets(cue)[1]:
            # SW_SFX_MonsterAggro_Humanoid_01 -> family humanoid, variant 1
            parts = name.split("_")
            family = parts[-2].lower()
            v = int(parts[-1])
            r = port_sfx.VOCALS[(kind, family)](v - 1)
            recipes_dump[r.id] = r.to_json()
            seed = sfx.seed_for(name, v)
            out.append(Asset(name, "SFX", "vocal", vocal_render(r, seed, family), False,
                             {"cue": cue["id"], "family": family, "variant": v, "seed": seed, "recipe": r.id,
                              "a7": kind}))
    # Death vocal layers (played by UE on top of monster_death for the source's family).
    for family in port_sfx.FAMILIES:
        for v, name in enumerate(death_vocal_assets(family), start=1):
            r = port_sfx.VOCALS[("death", family)](v - 1)
            recipes_dump[r.id] = r.to_json()
            seed = sfx.seed_for(name, v)
            out.append(Asset(name, "SFX", "vocal", vocal_render(r, seed, family), False,
                             {"cue": "monster_death", "layer": "vocal", "family": family, "variant": v, "seed": seed,
                              "recipe": r.id, "a7": "death"}))
    # Named voices (AU1: the Corpseherd, Valdras), each cue levelled on its own relative to `hit`. None loops: EvSfx
    # carries no stop, so a looping SFX cue would never end.
    for cue_id, (fn, _n, rel_lu, loop, _conc, _use) in dark_vox.NAMED_CUES.items():
        if loop:
            raise SystemExit(f"{cue_id}: a looping SFX cue cannot be stopped (EvSfx has no stop): render it as a one-shot")
        for v, name in enumerate(named_assets(cue_id), start=1):
            r = fn(v - 1)
            recipes_dump[r.id] = r.to_json()
            seed = sfx.seed_for(name, v)
            meta = {"cue": cue_id, "variant": v, "seed": seed, "recipe": r.id, "a7": "named", "relativeToHitLu": rel_lu}
            out.append(Asset(name, "SFX", "vocal", vocal_render(r, seed, None), False, meta))
    return out


def vocal_render(r: sfx.Recipe, seed: int, family: str | None) -> np.ndarray:
    """A7 vocal one-shot. AU1's (the dark remake's families and the named voices, family None) are cut where the tail
    has fallen dark_vox.TRIM_DB under the peak (no retained memory or concurrency voice spent on inaudible padding);
    the A7 humanoid / slime renders stay exactly as committed."""
    y = sfx.render(r, SR, seed)
    return dark_vox.trim_tail(y, SR) if family is None or family in dark_vox.FAMILIES else y


def render_footsteps(recipes_dump: dict) -> list[Asset]:
    out = []
    for surface, fn in port_sfx.FOOTSTEPS.items():
        for v in range(1, 5):
            name = f"SW_SFX_Footstep_{pascal(surface)}_{v:02d}"
            r = fn(v - 1)
            recipes_dump[r.id] = r.to_json()
            seed = sfx.seed_for(name, v)
            out.append(Asset(name, "Footsteps", "footstep", sfx.render(r, SR, seed), False,
                             {"surface": surface, "variant": v, "seed": seed, "recipe": r.id, "a7": "footstep"}))
    return out


def render_ambience(recipes_dump: dict) -> list[Asset]:
    r = port_sfx.ambience_plains()
    recipes_dump[r.id] = r.to_json()
    name = "SW_AMB_Plains"
    seed = sfx.seed_for(name, 1)
    y = sfx.render(r, SR, seed, extra_sec=0.0)
    y = loops.crossfade_loop(y, int(port_sfx.AMBIENCE_OVERLAP_SEC * SR))
    out = [Asset(name, "Ambience", "ambience", y, True,
                 {"theme": "plains", "seed": seed, "recipe": r.id, "a7": "ambience", "seam": loops.seam_ratio(y)})]
    # AU2: the march bed, the Breach Rim region bed, the Gnawed Field layer, the emitters (whole-file loops, seams
    # clean by construction: dark_ambience.render_loop).
    for name, (fn, level, role, _row) in dark_ambience.AMBIENCE.items():
        t0 = time.time()
        r = fn()
        recipes_dump[r.id] = r.to_json()
        seed = sfx.seed_for(name, 1)
        y = dark_ambience.render_loop(r, SR, seed)
        # An integrated target, or a group levelled with one gain relative to `hit` (the knock emitters).
        lv = ({"levelGroup": level["group"], "relativeToHitLu": float(level["relativeToHitLu"])}
              if isinstance(level, dict) else {"targetLufs": float(level)})
        out.append(Asset(name, "Ambience", "ambience", y, True,
                         {"ambience": role, "seed": seed, "recipe": r.id, **lv,
                          "seamRendered": round(loops.seam_ratio(y), 3), "decision": "AU2"}))
        log(f"  {name}: {y.shape[1] / SR:.1f} s, {y.shape[0]} ch ({time.time() - t0:.1f} s)")
    return out


def render_bells(recipes_dump: dict) -> list[Asset]:
    """AU2 bells (dark_ambience.BELL_CUES): one-shots with their reverb tails, levelled relative to `hit`."""
    out = []
    for cue_id, (fn, _n, rel_lu, _spatial, _conc, _use) in dark_ambience.BELL_CUES.items():
        for v, name in enumerate(named_assets(cue_id), start=1):
            r = fn(v - 1)
            recipes_dump[r.id] = r.to_json()
            seed = sfx.seed_for(name, v)
            y = dark_ambience.render_oneshot(r, SR, seed)
            out.append(Asset(name, "SFX", "bell", y, False,
                             {"cue": cue_id, "variant": v, "seed": seed, "recipe": r.id, "a7": "bell",
                              "relativeToHitLu": rel_lu, "decision": "AU2"}))
    return out


def recipe_end(r: sfx.Recipe) -> float:
    return max([r.length] + [s["stop"] for lay in r.layers for s in lay["sources"]])


def render_stingers(music_json: dict, recipes_dump: dict, dark: dict) -> list[Asset]:
    cfg = chain.config_for_theme(music_json["themes"]["emerald_plains"])
    out = []
    for role, name, fn, loop in STINGERS:
        r = fn()
        recipes_dump[r.id] = r.to_json()
        seed = sfx.seed_for(name, 1)
        rng = np.random.Generator(np.random.PCG64(seed ^ 0x5EED))
        if loop:
            dry = sfx.render(r, SR, seed, extra_sec=0.0)
            dry = loops.crossfade_loop(dry, int((stingers.WHISPER_RENDER_SEC - stingers.WHISPER_LOOP_SEC) * SR))
            y = chain.process_loop(dry, cfg, SR, rng)
        else:
            end = recipe_end(r)
            dry = sfx.render(r, SR, seed, extra_sec=end - r.length + 0.01)
            y = fade_tail(chain.process_oneshot(dry, cfg, SR, rng, tail_sec=cfg.decay))
        out.append(Asset(name, "Stingers", "stinger", y, loop,
                         {"role": role, "seed": seed, "recipe": r.id, "chain": "emerald_plains",
                          "nominalSec": r.length if not loop else stingers.WHISPER_LOOP_SEC}))
    # AU3 stingers (dark_scores.json `stingers`): scores, not recipes (the score file is their engine-agnostic data,
    # so they stay out of sfx_recipes.json); each bus has its own space, no web chain.
    for piece in dark_music.pieces(dark):
        if not piece.stinger:
            continue
        t0 = time.time()
        seed = sfx.seed_for(piece.asset, 1)
        y = dark_music.render(piece, SR, seed)
        out.append(Asset(piece.asset, "Stingers", "stinger", y, False,
                         {"role": piece.key, "seed": seed, "score": piece.key, "nominalSec": round(piece.seconds, 4),
                          "levelVsExploreLu": float(piece.spec["levelVsExploreLu"]),
                          "levelMatch": piece.spec["levelMatch"], "use": piece.spec["use"],
                          "scoreFile": "render/data/dark_scores.json", "decision": "AU3"}))
        log(f"  {piece.asset}: {y.shape[1] / SR:.2f} s ({time.time() - t0:.1f} s)")
    return out


def render_music(music_json: dict, port_scores: dict, entries: list[dict], dark: dict) -> list[Asset]:
    out = []
    themes = music_json["themes"]
    scores = dict(music_json["scores"])
    scores.update(port_scores["scores"])
    dark_pieces = {p.key: p for p in dark_music.pieces(dark) if not p.stinger}
    for e in entries:
        t0 = time.time()
        seed = str_seed(e["asset"])
        if e["kind"] == "dark":
            # AU3: a dark score (no web chain: each bus has its own space), levelled to its own target below.
            piece = dark_pieces[e["key"]]
            y = dark_music.render(piece, SR, seed)
            spec = piece.spec
            meta = {"key": e["key"], "source": "darkScore", "score": e["key"], "seed": seed,
                    "tempo": spec["tempo"], "beatsPerBar": spec["beatsPerBar"], "bars": spec["bars"],
                    "parts": [p["id"] for p in spec["parts"]], "targetLufs": float(spec["loudnessLufs"]),
                    "use": spec["use"], "scoreFile": "render/data/dark_scores.json", "decision": "AU3"}
            if e["loop"]:
                meta["seamRatioRendered"] = round(loops.seam_ratio(y), 3)
            meta["renderedLufs"] = round(master.integrated_lufs(y, SR), 2)
            out.append(Asset(e["asset"], "Music", "music", y.astype(np.float32).astype(np.float64), e["loop"], meta))
            log(f"  {e['asset']}: {y.shape[1] / SR:.2f} s, rendered {meta['renderedLufs']} LUFS "
                f"({time.time() - t0:.1f} s)")
            continue
        cfg = chain.config_for_theme(themes[e["theme"]])
        rng = np.random.Generator(np.random.PCG64(seed))
        meta = {"key": e["key"], "source": e["kind"], "theme": e["theme"], "chain": cfg.to_json()}
        if e["kind"] == "score":
            spec = ScoreSpec.from_json(scores[e["score"]])
            spec.check_invariants()
            score_seed = str_seed(e["seed"])
            y, player = loops.render_score_loop(spec, e["state"], e["bars"], score_seed, SR, cfg, rng)
            meta.update({"score": e["score"], "state": e["state"], "bars": e["bars"], "scoreSeed": e["seed"],
                         "scoreSeedU32": score_seed, "oscillators": player.trace.oscillators,
                         "leadNotes": len(player.trace.leads)})
        else:
            dec = mp3.decode(str(BGM / e["file"]), loop=e["loop"])
            # Source fields are prefixed: `file` / `sha256` of a manifest entry describe the exported OGG.
            meta.update({"sourceFile": f"public/assets/audio/bgm/{e['file']}", "sourceSha256": dec.sha256,
                         "encoder": dec.info.encoder, "encDelay": dec.info.enc_delay,
                         "encPadding": dec.info.enc_padding, "gaplessDecoded": dec.gapless_ok,
                         "trimmedHeadFrames": dec.trimmed_head, "trimmedTailFrames": dec.trimmed_tail,
                         "seamCrossfaded": dec.seam_crossfaded, "credit": e["credit"],
                         "rawLufs": round(master.integrated_lufs(dec.audio, SR), 2)})
            if e["loop"]:
                y = chain.process_loop(dec.audio, cfg, SR, rng)
            else:
                y = fade_tail(chain.process_oneshot(dec.audio, cfg, SR, rng, tail_sec=cfg.decay))
        if e["loop"]:
            meta["seamRatioRendered"] = round(loops.seam_ratio(y), 3)  # before encoding; `seamRatio` = decoded OGG
        meta["webLufs"] = round(master.integrated_lufs(y, SR), 2)
        a = Asset(e["asset"], "Music", "music", y.astype(np.float32).astype(np.float64), e["loop"], meta)
        out.append(a)
        log(f"  {e['asset']}: {y.shape[1] / SR:.2f} s, web chain {meta['webLufs']} LUFS ({time.time() - t0:.1f} s)")
    return out


# ---------------------------------------------------------------------------------------------------------------------
# Levels
# ---------------------------------------------------------------------------------------------------------------------
def apply_gain(a: Asset, gain_db: float, limit: bool = True) -> None:
    a.gain_db = gain_db
    y = a.audio * master.undb(gain_db)
    if limit:
        if a.loop:
            y, gr = master.limit_loop(y, SR, TRUE_PEAK_DBTP)
        else:
            y, gr = master.limit(y, SR, TRUE_PEAK_DBTP)
        a.limiter_gr_db = gr
    a.audio = y


def level_assets(assets: list[Asset], levels: dict) -> None:
    by_kind: dict[str, list[Asset]] = {}
    for a in assets:
        by_kind.setdefault(a.kind, []).append(a)

    # Music: -16 LUFS each (AU3's tracks: their own target), under the momentary ceiling.
    explore_ref = None
    targets = dict(levels.get("musicTargetsByTrack", {}))
    for a in by_kind.get("music", []):
        lufs = a.meta["webLufs"] if "webLufs" in a.meta else a.meta["renderedLufs"]
        target = a.meta.get("targetLufs", MUSIC_LUFS)
        gain_db = target - lufs
        if "targetLufs" in a.meta:  # AU3: the ceiling (the A3 recordings and web scores keep their plain -16)
            ceiling = target + MUSIC_MOMENTARY_CEILING_LU
            over = master.momentary_max_lufs(a.audio, SR) + gain_db - ceiling
            a.meta["momentaryCeilingLufs"] = round(ceiling, 2)
            if over > 0:
                gain_db -= over
                a.meta["levelledBy"] = "momentaryCeiling"
        apply_gain(a, gain_db)
        targets[a.meta["key"]] = target
        if a.meta["key"] == "emerald_plains_explore":
            explore_ref = a
    levels["musicTargetsByTrack"] = dict(sorted(targets.items()))
    levels["musicMomentaryCeilingLu"] = MUSIC_MOMENTARY_CEILING_LU
    ref_web = None
    if explore_ref is not None:
        ref_web = explore_ref.meta["webLufs"]
    levels["musicTargetLufs"] = MUSIC_LUFS
    levels["referenceTrack"] = "emerald_plains_explore"
    if ref_web is None:
        ref_web = levels.get("referenceWebLufs")
    if ref_web is None:
        raise SystemExit("the plains explore track is needed as the loudness reference (render music too)")
    levels["referenceWebLufs"] = ref_web

    # Web SFX: one common gain (A3 "consistently").
    ratio = (WEB_VOLUMES["sfx"] / WEB_VOLUMES["music"]) * (PORT_VOLUMES["music"] / PORT_VOLUMES["sfx"])
    g_db = master.db(ratio) + (MUSIC_LUFS - ref_web)
    sfx_assets = by_kind.get("sfx", [])
    if sfx_assets:
        worst = max(master.db(master.true_peak(a.audio)) for a in sfx_assets)
        headroom = TRUE_PEAK_DBTP - worst
        if g_db > headroom:
            log(f"  SFX common gain {g_db:+.2f} dB capped to {headroom:+.2f} dB by the loudest true peak")
            levels["sfxGainCappedFromDb"] = round(g_db, 2)
            g_db = headroom
        for a in sfx_assets:
            apply_gain(a, g_db, limit=False)
        # Only a run that levelled the SFX records their gain (a partial run keeps the previous values).
        levels["sfxCommonGainDb"] = round(g_db, 3)
        levels["sfxBalanceRatio"] = ratio

    # A7 one-shots relative to the web hit cue (AU2's knock emitters too, as a group).
    hits = [a for a in sfx_assets if a.meta.get("cue") == "hit"]
    hit_groups = [a for a in by_kind.get("ambience", []) if "levelGroup" in a.meta]
    if hit_groups and not hits:
        raise SystemExit("group-levelled ambience needs the web `hit` cue as its reference")
    if hits:
        hit_m = float(np.mean([master.momentary_max_lufs(a.audio, SR) for a in hits]))
        levels["hitMomentaryMaxLufs"] = round(hit_m, 2)
        groups: dict[str, list[Asset]] = {}
        for a in by_kind.get("vocal", []) + by_kind.get("footstep", []) + by_kind.get("bell", []):
            who = a.meta.get("family", a.meta.get("surface", a.meta.get("cue")))
            groups.setdefault(f"{a.meta['a7']}:{who}", []).append(a)
        for a in hit_groups:
            groups.setdefault(f"ambience:{a.meta['levelGroup']}", []).append(a)
        # A partial run (--only vocals) keeps the other groups' rows.
        levels["a7"] = dict(levels.get("a7", {}))
        for key, group in sorted(groups.items()):
            kind = key.split(":")[0]
            rel = group[0].meta.get("relativeToHitLu", A7_RELATIVE_LU.get(kind))
            m = float(np.mean([master.momentary_max_lufs(a.audio, SR) for a in group]))
            gdb = hit_m + rel - m
            for a in group:
                apply_gain(a, gdb)
            levels["a7"][key] = {"relativeToHitLu": rel, "gainDb": round(gdb, 2)}
        levels["a7"] = dict(sorted(levels["a7"].items()))

    # Ambience (AU2's layer and hearth carry their own target; the knock emitters were levelled as a group above).
    for a in by_kind.get("ambience", []):
        if "levelGroup" not in a.meta:
            apply_gain(a, a.meta.get("targetLufs", AMBIENCE_LUFS) - master.integrated_lufs(a.audio, SR))
    levels["ambienceTargetLufs"] = AMBIENCE_LUFS

    # Stingers: short-term max 3 LU under the (normalised) plains explore track (audio.md 10.3). A stinger shorter
    # than the 3 s short-term window (the 1.2 s letterbox whoosh) is matched on the momentary (400 ms) maximum instead,
    # otherwise the silence inside the window would read as headroom and push it far above the music.
    stingers_to_level = by_kind.get("stinger", [])
    if explore_ref is not None:
        explore_audio = explore_ref.audio
    elif stingers_to_level and not {"exploreShortTermMaxLufs", "exploreMomentaryMaxLufs"} <= levels.keys():
        # Partial run (--only stingers): measure the exported, already normalised explore track.
        path = EXPORT / "Music" / "SW_MUS_EmeraldPlainsExplore.ogg"
        if not path.exists():
            raise SystemExit(f"{path} is needed as the stinger loudness reference (render music first)")
        explore_audio, _ = master.read_audio(str(path))
    else:
        explore_audio = None
    if explore_audio is not None:
        levels["exploreShortTermMaxLufs"] = round(master.short_term_max_lufs(explore_audio, SR), 2)
        levels["exploreMomentaryMaxLufs"] = round(master.momentary_max_lufs(explore_audio, SR), 2)
    for a in stingers_to_level:
        # AU3's stingers say how they are matched (`levelMatch`: punchy hits on the momentary maximum, swells on the
        # short-term one); the others by length.
        if a.meta.get("levelMatch", "momentary" if a.meta.get("nominalSec", 3.0) < 3.0 else "shortTerm") == "momentary":
            cur = master.momentary_max_lufs(a.audio, SR)
            ref = levels["exploreMomentaryMaxLufs"]
            a.meta["levelMatch"] = "momentary"
        else:
            cur = master.short_term_max_lufs(a.audio, SR)
            ref = levels["exploreShortTermMaxLufs"]
            a.meta["levelMatch"] = "shortTerm"
        apply_gain(a, (ref + a.meta.get("levelVsExploreLu", -STINGER_UNDER_EXPLORE_LU)) - cur)
    levels["stingerUnderExploreLu"] = STINGER_UNDER_EXPLORE_LU


# ---------------------------------------------------------------------------------------------------------------------
# Export
# ---------------------------------------------------------------------------------------------------------------------
def export(a: Asset) -> dict:
    folder = EXPORT / a.folder
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / f"{a.name}.ogg"
    tmp = folder / f".{a.name}.tmp.ogg"
    master.write_ogg(str(tmp), a.audio, SR, serial=str_seed(a.name))
    dec, sr = master.read_audio(str(tmp))
    # Keep the committed file when it decodes to exactly the same samples (no binary churn in git); otherwise replace.
    if path.exists():
        old, _ = master.read_audio(str(path))
        if old.shape == dec.shape and np.array_equal(old, dec):
            tmp.unlink()
        else:
            os.replace(tmp, path)
    else:
        os.replace(tmp, path)
    if a.loop:
        # Vorbis keeps the exact frame count (granule position): verify, the loop is the whole file.
        assert dec.shape[1] == a.audio.shape[1], f"{a.name}: frame count changed by the encoder"
    st = {
        "frames": int(dec.shape[1]),
        "lengthSec": round(dec.shape[1] / SR, 4),
        "peakDbfs": round(master.db(master.sample_peak(dec)), 2),
        "truePeakDbtp": round(master.db(master.true_peak(dec)), 2),
        "rmsDbfs": round(master.db(float(np.sqrt(np.mean(dec * dec)))), 2),
        "lufs": round(master.integrated_lufs(dec, SR), 2),
        "shortTermMaxLufs": round(master.short_term_max_lufs(dec, SR), 2) if dec.shape[1] >= 3 * SR else None,
        "momentaryMaxLufs": round(master.momentary_max_lufs(dec, SR), 2),
        "bytes": path.stat().st_size,
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
    }
    if a.loop:
        st["seamRatio"] = round(loops.seam_ratio(dec), 3)
    a.stats = st
    entry = {
        "file": f"{a.folder}/{a.name}.ogg",
        "ue": f"{UE_ROOT}/{a.folder}/{a.name}.{a.name}",
        "kind": a.kind,
        "channels": a.channels,
        "sampleRate": SR,
        "loop": a.loop,
        "loopStartSample": 0,
        "loopEndSample": st["frames"] if a.loop else 0,
        "gain": 1.0,
        "masterGainDb": round(a.gain_db, 3),
        "limiterGainReductionDb": round(a.limiter_gr_db, 3),
    }
    entry.update(st)
    clash = (set(a.meta) - {"chain"}) & set(entry)
    if clash:  # metadata must never overwrite what UE / the import script read (file, sha256, loop, ...)
        raise SystemExit(f"{a.name}: metadata keys {sorted(clash)} collide with manifest fields")
    entry.update({k: v for k, v in a.meta.items() if k not in ("chain",)})
    if "chain" in a.meta:
        entry["chain"] = a.meta["chain"]
    return entry


def ambience_table(assets: dict) -> dict:
    """Zone / theme beds (A7 + AU2: the march bed is emerald_plains's and the `march` presentation theme's; the Ember
    Tower keeps the plains bed) and AU2's region beds, layers and emitters, each once its assets are rendered."""
    themes = {"plains": "SW_AMB_Plains"}
    zones = {"emerald_plains": "SW_AMB_Plains", "ember_tower": "SW_AMB_Plains"}
    regions: dict = {}
    layers: dict = {}
    emitters: dict = {}
    geometry = ("col", "row", "radius", "exitRadius")
    for name, (_fn, _level, role, row) in dark_ambience.AMBIENCE.items():
        if name not in assets:
            continue
        if role == "bed":
            themes[row["theme"]] = name
            zones.update({z: name for z in row["zones"]})
        elif role == "region":
            regions[row["region"]] = {"asset": name, "zone": row["zone"], "mapRegion": row["mapRegion"],
                                      **{k: row[k] for k in geometry}, "replacesBed": row["replacesBed"],
                                      "fadeInSec": 3.0, "fadeOutSec": 3.0, "use": row["use"]}
            if "while" in row:
                regions[row["region"]]["while"] = row["while"]
        elif role == "layer":
            layers[row["id"]] = {"asset": name, "zone": row["zone"], **{k: row[k] for k in geometry},
                                 "while": row["while"], "untilFlag": row["untilFlag"], "fadeInSec": 4.0,
                                 "fadeOutSec": 6.0, "use": row["use"]}
        else:
            spec = dark_ambience.EMITTERS[row["id"]]
            e = emitters.setdefault(row["id"], {"assets": [], "spatial": "3d", "attenuation": spec["attenuation"],
                                                "placement": spec["placement"], "use": spec["use"]})
            e["assets"].append(name)
    table = {"themes": themes, "zones": zones, "fadeInSec": 2.5, "fadeOutSec": 1.5, "gain": 1.0,
             "duckInCinematicDb": -6.0}
    if regions or layers or emitters:
        # AU2 (implementation-plan.md 7, U4 / U7): read by the UE lane; the A7 keys above are unchanged. Regions and
        # layers carry their geometry and condition (`regionRule`), emitters their attenuation and placement.
        table.update({"regions": regions, "layers": layers, "emitters": emitters,
                      "regionRule": dark_ambience.REGION_RULE,
                      "emitterAttenuationRule": dark_ambience.EMITTER_ATTENUATION_RULE, "decision": "AU2"})
    return table


def music_director_table(dark: dict, music: dict, assets: dict) -> dict:
    """AU3 (implementation-plan.md 7, U7 / the core's MusicDirector): dark_scores.json `director`, with every track
    key resolved to its asset and every stinger id to its asset, duck and use. Only what is rendered is listed; the
    regions / story / boss rows keep the keys (a row naming a missing track is dropped)."""
    d = dark["director"]
    tracks = {k: music[k]["asset"] for k in dark["tracks"] if k in music}
    stingers_out = {}
    for sid, spec in dark["stingers"].items():
        if spec["asset"] in assets:
            stingers_out[sid] = {"asset": spec["asset"], "lengthSec": assets[spec["asset"]]["lengthSec"],
                                 "duck": spec["duck"], "use": spec["use"]}

    def known(track) -> bool:
        return track == "" or track in tracks

    return {
        "decision": "AU3",
        "source": "unreal/Audio/render/data/dark_scores.json (director)",
        "zone": d["zone"],
        "priority": d["priority"],
        "zoneTracks": {k: v for k, v in d["zoneTracks"].items() if known(v)},
        # States whose zoneTracks row is not in `zoneOverrides` yet (what the reader must land first); none since U7.
        **({"heldOverrides": d["heldOverrides"]} if d.get("heldOverrides") else {}),
        "combat": d["combat"],
        "tracks": tracks,
        "regions": [r for r in d["regions"] if known(r["track"])],
        "regionRule": d["regionRule"],
        "bosses": {b: dict(row, phaseStingers={i: s for i, s in row["phaseStingers"].items() if s in stingers_out})
                   for b, row in d["bosses"].items() if known(row["track"])},
        "encounters": {k: {kk: vv for kk, vv in row.items() if kk == "monsters" or vv in stingers_out}
                       for k, row in d["encounters"].items()},
        "story": d["story"],
        "stingers": stingers_out,
    }


def cue_triggers(cues: dict) -> dict:
    """U7 (implementation-plan.md 4): the structured `use` of the AU2 bells and of the beat-bound AU1 voices."""
    rows = [dict(r) for r in dark_ambience.BELL_TRIGGERS + dark_vox.VOX_TRIGGERS
            if r["cue"] in cues and r.get("farCue", r["cue"]) in cues]
    return {"decision": "U7", "kinds": list(dark_ambience.TRIGGER_KINDS),
            "rule": "story indices are the core's 0-based EvStoryStep::index; `every` never fires while a story beat "
                    "plays and waits one interval first; farCue plays when the hero is outside the musicDirector "
                    "region nearRegion; tile places a 3d cue (else the hero); a burst (count, spacingSec) started by a "
                    "story beat stops when that beat ends",
            "rows": rows}


def build_manifest(assets: dict, levels: dict, cue_table: dict, music_entries: list[dict], dark: dict) -> dict:
    cues = {}
    for cue in cue_table["cues"]:
        cues[cue["id"]] = {
            "assets": cue["assets"],
            "variants": cue["variants"],
            "families": cue.get("families", []),
            "bus": cue["bus"],
            "spatial": cue["spatial"],
            "concurrency": cue["concurrency"],
            "gain": 1.0,
        }
    # Voice families (A7 + AU1): a family joins a cue once all its assets are rendered (a manifest rebuilt from an
    # older render keeps working).
    for cid in VOCAL_CUES:
        if cid in cues:
            row = next(c for c in cue_table["cues"] if c["id"] == cid)
            families, names = family_assets(row)
            per = row["variants"]
            keep = [i for i, f in enumerate(families)
                    if f in row.get("families", []) or all(n in assets for n in names[i * per:(i + 1) * per])]
            cues[cid]["families"] = [families[i] for i in keep]
            cues[cid]["assets"] = [n for i in keep for n in names[i * per:(i + 1) * per]]
    death_families = [f for f in port_sfx.FAMILIES
                      if f in port_sfx.BASE_FAMILIES or all(n in assets for n in death_vocal_assets(f))]
    cues["monster_death"]["vocalLayer"] = {
        "families": death_families,
        "assets": {f: death_vocal_assets(f) for f in death_families},
        "gain": 1.0,
    }
    # AU1 named voices (dark_vox.NAMED_CUES); a row in audio_cues.json, once the core lists the cue, wins.
    for cid, (_fn, n, _rel, loop, conc, use) in dark_vox.NAMED_CUES.items():
        names = named_assets(cid)
        if cid not in cues and all(a in assets for a in names):
            cues[cid] = {"assets": names, "variants": n, "families": [], "bus": "combat", "spatial": "3d",
                         "concurrency": conc, "gain": 1.0, "loop": loop, "use": use, "decision": "AU1"}
    # AU2 bells (dark_ambience.BELL_CUES), the same way.
    for cid, (_fn, n, _rel, spatial, conc, use) in dark_ambience.BELL_CUES.items():
        names = named_assets(cid)
        if cid not in cues and all(a in assets for a in names):
            cues[cid] = {"assets": names, "variants": n, "families": [], "bus": "combat", "spatial": spatial,
                         "concurrency": conc, "gain": 1.0, "loop": False, "use": use, "decision": "AU2"}
    music = {}
    for e in music_entries:
        if e["asset"] in assets:
            a = assets[e["asset"]]
            music[e["key"]] = {"asset": e["asset"], "loop": e["loop"], "lengthSec": a["lengthSec"],
                               "loopStartSample": 0, "loopEndSample": a["loopEndSample"], "gain": 1.0,
                               "source": "darkScore" if e["kind"] == "dark" else e["kind"], "credit": e.get("credit")}
            if e["kind"] == "dark":
                music[e["key"]].update({"use": e["use"], "decision": "AU3"})
    footsteps = {s: [f"SW_SFX_Footstep_{pascal(s)}_{v:02d}" for v in range(1, 5)] for s in port_sfx.FOOTSTEPS}
    return {
        "schemaVersion": 1,
        "generator": "unreal/Audio/render/render_all.py",
        "sampleRate": SR,
        "codec": {"format": "ogg", "codec": "vorbis", "quality": master.VORBIS_QUALITY},
        "ueRoot": UE_ROOT,
        "levels": levels,
        "defaults": {"musicVolume": PORT_VOLUMES["music"], "sfxVolume": PORT_VOLUMES["sfx"], "masterVolume": 1.0},
        "cues": cues,
        "music": music,
        "menuTrack": "menu_explore",
        # audio.md 9.8 / 11 zoneOverrides (QUIRK Q8 FIX, "as heard"): the Ember Tower plays the procedural plains score,
        # not the plains recording its theme resolves to. Applied by UAbyssAudioSystem to the core's EvMusic keys.
        "zoneOverrides": {zone: {k: v for k, v in table.items() if v in music}
                          for zone, table in zone_overrides(dark).items()},
        "stingers": {
            # AU3: the remake's Chapter One card is night (chapter1.md); UE picks the stinger by the card's mood.
            "chapterCard": {"dawn": "SW_STG_ChapterCardDawn",
                            **({"night": "SW_STG_ChapterCardNight"} if "SW_STG_ChapterCardNight" in assets else {})},
            "chapterCardFallback": "SW_STG_ChapterCardDawn",
            "bossIntro": "SW_STG_BossIntro",
            "whisper": "SW_STG_WhisperLoop",
            "cutsceneIn": "SW_STG_CutsceneIn",
            # audio.md 10.2: music ducking while a stinger plays (dB, fade in / out seconds).
            "duck": {
                "chapterCard": {"db": -8.0, "inSec": 0.6, "outSec": 0.7},
                "bossIntro": {"db": -10.0, "inSec": 0.2, "outSec": 0.45},
                "whisper": {"db": -6.0, "inSec": 0.6, "outSec": 0.6},
            },
            "whisperFadeSec": 0.6,
            "chapterCardFadeOutSec": 0.7,
            "bossIntroFadeOutSec": 0.45,
        },
        "ambience": ambience_table(assets),
        "footsteps": {
            "surfaces": footsteps,
            "tileSurface": {"grass": "grass", "dirt": "dirt", "stone": "stone", "camp": "dirt", "water": "dirt",
                            "wall": "stone", "campWall": "dirt"},
            "notifies": ["FootL", "FootR"],
            "fallbackStrideCm": 116.7,
            "notifyTimeoutSec": 0.9,
            "minIntervalSec": 0.18,
            "gain": 1.0,
        },
        "spatial": {"spread": cue_table["spatial"]["spread"], "halfWidthCm": 800.0, "virtualDistanceCm": 100.0},
        # AU1 (implementation-plan.md 7, U7): monster -> voice family for the family cues, the design's family names
        # -> voice family, and the per-monster named cues that replace a family cue (`monster_death.vocal` = the
        # death vocal layer).
        "voiceFamilies": {
            "families": list(cues.get("monster_aggro", {}).get("families", [])),
            "aliases": dark_vox.FAMILY_ALIASES,
            "monsters": dark_vox.MONSTER_FAMILY,
            # Monsters with no throat: no family cue at all (UE would otherwise fall back to the first family).
            "silent": dark_vox.SILENT_MONSTERS,
            "overrides": {m: {k: c for k, c in o.items() if c in cues}
                          for m, o in dark_vox.NAMED_OVERRIDES.items()},
            "overrideKeys": dark_vox.OVERRIDE_KEYS,
        },
        # U7: when the AU2 bells and the beat-bound AU1 voices play (dark_ambience.BELL_TRIGGERS documents the rows;
        # a row whose cue is not rendered is left out).
        "cueTriggers": cue_triggers(cues),
        # The design's non-vocal monster sounds (monsters.md 2) that no AU package renders yet: ids reserved for the
        # core (no assets; UE plays nothing for a cue without a `cues` row). A rendered cue moves to `cues`.
        "plannedCues": {cid: {"monsters": mons, "sound": sound, "wave": wave, "spatial": spatial, "bus": "combat",
                              "owner": dark_vox.PLANNED_OWNER}
                        for cid, (mons, sound, wave, spatial) in dark_vox.PLANNED_CUES.items() if cid not in cues},
        # audio.md 8.3 (FIX Q10): the title menu soundtrack lists only packaged tracks that have a title key; real
        # lengths come from `music`, pause / seek are real.
        # AU3 (implementation-plan.md 7, U7 / the core's MusicDirector): regions (and silences), the zone tracks, the
        # boss and its phase stingers, encounter stingers, story beats.
        "musicDirector": music_director_table(dark, music, assets),
        "jukebox": {
            "tracks": [{"titleKey": key, "track": track} for key, track in JUKEBOX if track in music],
            "fadeOutSec": 1.0,
            "fadeInSec": 0.5,
        },
        "assets": assets,
    }


# ---------------------------------------------------------------------------------------------------------------------
def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--only", default="", help="comma list: sfx,vocals,bells,footsteps,ambience,stingers,music")
    ap.add_argument("--later-recordings", action="store_true", help="also transcode the later chapters' recordings")
    ap.add_argument("--reports", action="store_true", help="write waveform / spectrum PNGs into Reports/")
    ap.add_argument("--manifest-only", action="store_true",
                    help="rebuild audio_manifest.json from the current one (tables / schema changes), render nothing")
    args = ap.parse_args()
    only = {s.strip() for s in args.only.split(",") if s.strip()}
    if only - set(GROUP_KIND):
        raise SystemExit(f"unknown group(s) {sorted(only - set(GROUP_KIND))}; groups: {', '.join(GROUP_KIND)}")

    def want(part: str) -> bool:
        return not only or part in only

    music_json = json.loads((DATA / "music.json").read_text(encoding="utf-8"))
    cue_table = json.loads((DATA / "audio_cues.json").read_text(encoding="utf-8"))
    port_scores = json.loads((HERE / "data" / "port_scores.json").read_text(encoding="utf-8"))
    dark = dark_music.load()  # AU3, beside port_scores.json
    composer.load_tables(music_json["composer"])

    entries = list(CH1_MUSIC) + dark_entries(dark)
    if args.later_recordings:
        for theme, state, title, author, url, loop in LATER_RECORDINGS:
            entries.append({"key": f"{theme}_{state}", "asset": f"SW_MUS_{pascal(theme)}{pascal(state)}",
                            "kind": "recording", "file": f"{theme}_{state}.mp3", "theme": theme, "loop": loop,
                            "credit": {"title": title, "author": author, "license": "CC0", "url": url}})

    manifest_path = EXPORT / "audio_manifest.json"
    previous = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {}
    if args.manifest_only:
        if not previous:
            raise SystemExit(f"{manifest_path} does not exist: render first")
        manifest = build_manifest(dict(sorted(previous["assets"].items())), dict(previous["levels"]), cue_table,
                                  entries, dark)
        manifest_path.write_text(json.dumps(manifest, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
        log(f"manifest rebuilt: {len(manifest['assets'])} assets")
        return 0
    levels: dict = dict(previous.get("levels", {})) if only else {}

    recipes_dump: dict = {}
    assets: list[Asset] = []
    t0 = time.time()
    if want("music"):
        log("music ...")
        assets += render_music(music_json, port_scores, entries, dark)
    if want("sfx"):
        log("sfx ...")
        assets += render_sfx(cue_table, recipes_dump)
    if want("vocals"):
        log("vocals ...")
        assets += render_vocals(cue_table, recipes_dump)
    if want("bells"):
        log("bells ...")
        assets += render_bells(recipes_dump)
    if want("footsteps"):
        log("footsteps ...")
        assets += render_footsteps(recipes_dump)
    if want("ambience"):
        log("ambience ...")
        assets += render_ambience(recipes_dump)
    if want("stingers"):
        log("stingers ...")
        assets += render_stingers(music_json, recipes_dump, dark)
    log(f"rendered {len(assets)} assets in {time.time() - t0:.1f} s; levelling ...")

    if only and ("sfx" not in only) and any(a.kind in ("vocal", "footstep", "bell") or "levelGroup" in a.meta
                                            for a in assets):
        # A7 cues are levelled against `hit`, whose gain is the web SFX common gain, capped by the loudest true peak
        # of ALL web SFX: level every web SFX as the reference (exactly as a full run does), export none of them.
        level_assets(assets + render_sfx(cue_table, {}), levels)
    else:
        level_assets(assets, levels)

    log("exporting ...")
    entries_out = dict(previous.get("assets", {})) if only else {}
    for a in assets:
        entries_out[a.name] = export(a)
    # A rendered group owns its kind: an asset of that kind the tables no longer produce (a renamed or removed cue) is
    # dropped from the manifest and its export deleted, so a partial run leaves what a full run would.
    kinds = {GROUP_KIND[g] for g in only} if only else set(GROUP_KIND.values())
    rendered = {a.name for a in assets}
    for name, e in sorted(previous.get("assets", {}).items()):
        if e.get("kind") in kinds and name not in rendered:
            entries_out.pop(name, None)
            (EXPORT / e["file"]).unlink(missing_ok=True)
            log(f"  removed {e['file']} (no longer rendered)")
    entries_out = dict(sorted(entries_out.items()))

    manifest = build_manifest(entries_out, levels, cue_table, entries, dark)
    EXPORT.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")

    if recipes_dump:
        dump_path = HERE / "data" / "sfx_recipes.json"
        old = json.loads(dump_path.read_text(encoding="utf-8")).get("recipes", {}) if (only and dump_path.exists()) else {}
        old.update(recipes_dump)
        dump_path.write_text(compact_json({"schemaVersion": 1, "generator": "render_all.py", "sampleRate": SR,
                                           "recipes": dict(sorted(old.items()))}) + "\n", encoding="utf-8")

    if args.reports:
        from abyss_audio import report
        report.write_reports(assets, REPORTS, SR, entries_out)
    total = sum(e["bytes"] for e in entries_out.values())
    log(f"done: {len(entries_out)} assets, {total / 1e6:.2f} MB OGG, {time.time() - t0:.1f} s")
    return 0


if __name__ == "__main__":
    sys.exit(main())

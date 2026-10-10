#!/usr/bin/env python3
"""Render every Chapter-1 audio asset, master it and export OGG Vorbis q6 + audio_manifest.json (DECISIONS A1, A3,
A5, A7; audio.md 9, 10).

    /opt/venvs/audio/bin/python unreal/Audio/render/render_all.py            # Chapter 1 (default)
    ... render_all.py --only sfx,stingers                                     # subsets: sfx vocals footsteps ambience
                                                                              #          stingers music
    ... render_all.py --later-recordings                                      # + the other zones' CC0 recordings
    ... render_all.py --reports                                               # + waveform / spectrum PNGs

Outputs (relative to unreal/Audio):
    Export/<Folder>/<Asset>.ogg          SFX, Vocals, Footsteps (mono), Ambience, Stingers, Music (stereo), 48 kHz
    Export/audio_manifest.json           cue / track / stinger / ambience / footstep tables for UAbyssAudioSystem
    render/data/sfx_recipes.json         every recipe as engine-agnostic JSON (audio.md 11)
    Reports/*.png, Reports/stats.md      verification (no listening test is possible where this runs)

Levels (A3): music is normalised to -16 LUFS integrated (true peak <= -1 dBTP, circular look-ahead limiting where a
render would exceed it). All web SFX share ONE gain (their relative web mix is kept) chosen so that the music / SFX
balance at the new default volumes (music 0.6, SFX 0.8) equals the web's at its defaults (0.15, 0.3) with the zone
recording as the music reference: g = (0.3 / 0.15) * (0.6 / 0.8) * 10^((-16 - L_web) / 20). The A7 cues are set
relative to the web `hit` cue; stingers 3 LU (short-term max) under the plains explore track; the ambience bed at
-30 LUFS.
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
from abyss_audio.recipes import port_sfx, stingers, web_sfx  # noqa: E402
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
    {"key": "boss_ch1", "asset": "SW_MUS_BossCh1", "kind": "score", "score": "boss_ch1", "state": "combat",
     "bars": 32, "seed": "boss_ch1-1", "theme": "emerald_plains", "loop": True},
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

STINGERS = [
    # role, asset, recipe fn, loop
    ("chapterCard.dawn", "SW_STG_ChapterCardDawn", stingers.chapter_card_dawn, False),
    ("bossIntro", "SW_STG_BossIntro", stingers.boss_intro, False),
    ("whisper", "SW_STG_WhisperLoop", stingers.whisper_loop, True),
    ("cutsceneIn", "SW_STG_CutsceneIn", stingers.cutscene_in, False),
]


def pascal(s: str) -> str:
    return "".join(w[:1].upper() + w[1:] for w in s.replace("-", "_").split("_") if w)


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


def render_vocals(cue_table: dict, recipes_dump: dict) -> list[Asset]:
    out = []
    kinds = {"monster_aggro": "aggro", "monster_hurt": "hurt"}
    for cue in cue_table["cues"]:
        if cue["id"] not in kinds:
            continue
        kind = kinds[cue["id"]]
        for name in cue["assets"]:
            # SW_SFX_MonsterAggro_Humanoid_01 -> family humanoid, variant 1
            parts = name.split("_")
            family = parts[-2].lower()
            v = int(parts[-1])
            r = port_sfx.VOCALS[(kind, family)](v - 1)
            recipes_dump[r.id] = r.to_json()
            seed = sfx.seed_for(name, v)
            out.append(Asset(name, "SFX", "vocal", sfx.render(r, SR, seed), False,
                             {"cue": cue["id"], "family": family, "variant": v, "seed": seed, "recipe": r.id,
                              "a7": kind}))
    # Death vocal layers (played by UE on top of monster_death for the source's family).
    for family in ("humanoid", "slime"):
        for v in (1, 2):
            name = f"SW_SFX_MonsterDeathVocal_{pascal(family)}_{v:02d}"
            r = port_sfx.VOCALS[("death", family)](v - 1)
            recipes_dump[r.id] = r.to_json()
            seed = sfx.seed_for(name, v)
            out.append(Asset(name, "SFX", "vocal", sfx.render(r, SR, seed), False,
                             {"cue": "monster_death", "layer": "vocal", "family": family, "variant": v, "seed": seed,
                              "recipe": r.id, "a7": "death"}))
    return out


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
    return [Asset(name, "Ambience", "ambience", y, True,
                  {"theme": "plains", "seed": seed, "recipe": r.id, "a7": "ambience", "seam": loops.seam_ratio(y)})]


def recipe_end(r: sfx.Recipe) -> float:
    return max([r.length] + [s["stop"] for lay in r.layers for s in lay["sources"]])


def render_stingers(music_json: dict, recipes_dump: dict) -> list[Asset]:
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
    return out


def render_music(music_json: dict, port_scores: dict, entries: list[dict]) -> list[Asset]:
    out = []
    themes = music_json["themes"]
    scores = dict(music_json["scores"])
    scores.update(port_scores["scores"])
    for e in entries:
        t0 = time.time()
        cfg = chain.config_for_theme(themes[e["theme"]])
        seed = str_seed(e["asset"])
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
            meta.update({"file": f"public/assets/audio/bgm/{e['file']}", "sha256": dec.sha256,
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
            meta["seamRatio"] = round(loops.seam_ratio(y), 3)
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

    # Music: -16 LUFS each.
    explore_ref = None
    for a in by_kind.get("music", []):
        lufs = a.meta["webLufs"]
        apply_gain(a, MUSIC_LUFS - lufs)
        if a.meta["key"] == "emerald_plains_explore":
            explore_ref = a
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
    levels["sfxCommonGainDb"] = round(g_db, 3)
    levels["sfxBalanceRatio"] = ratio

    # A7 one-shots relative to the web hit cue.
    hits = [a for a in sfx_assets if a.meta.get("cue") == "hit"]
    if hits:
        hit_m = float(np.mean([master.momentary_max_lufs(a.audio, SR) for a in hits]))
        levels["hitMomentaryMaxLufs"] = round(hit_m, 2)
        groups: dict[str, list[Asset]] = {}
        for a in by_kind.get("vocal", []) + by_kind.get("footstep", []):
            groups.setdefault(f"{a.meta['a7']}:{a.meta.get('family', a.meta.get('surface'))}", []).append(a)
        levels["a7"] = {}
        for key, group in sorted(groups.items()):
            kind = key.split(":")[0]
            m = float(np.mean([master.momentary_max_lufs(a.audio, SR) for a in group]))
            gdb = hit_m + A7_RELATIVE_LU[kind] - m
            for a in group:
                apply_gain(a, gdb)
            levels["a7"][key] = {"relativeToHitLu": A7_RELATIVE_LU[kind], "gainDb": round(gdb, 2)}

    # Ambience.
    for a in by_kind.get("ambience", []):
        apply_gain(a, AMBIENCE_LUFS - master.integrated_lufs(a.audio, SR))
    levels["ambienceTargetLufs"] = AMBIENCE_LUFS

    # Stingers: short-term max 3 LU under the (normalised) plains explore track (audio.md 10.3). A stinger shorter
    # than the 3 s short-term window (the 1.2 s letterbox whoosh) is matched on the momentary (400 ms) maximum instead,
    # otherwise the silence inside the window would read as headroom and push it far above the music.
    if explore_ref is not None:
        levels["exploreShortTermMaxLufs"] = round(master.short_term_max_lufs(explore_ref.audio, SR), 2)
        levels["exploreMomentaryMaxLufs"] = round(master.momentary_max_lufs(explore_ref.audio, SR), 2)
    for a in by_kind.get("stinger", []):
        if a.meta.get("nominalSec", 3.0) < 3.0:
            cur = master.momentary_max_lufs(a.audio, SR)
            ref = levels["exploreMomentaryMaxLufs"]
            a.meta["levelMatch"] = "momentary"
        else:
            cur = master.short_term_max_lufs(a.audio, SR)
            ref = levels["exploreShortTermMaxLufs"]
            a.meta["levelMatch"] = "shortTerm"
        apply_gain(a, (ref - STINGER_UNDER_EXPLORE_LU) - cur)
    levels["stingerUnderExploreLu"] = STINGER_UNDER_EXPLORE_LU


# ---------------------------------------------------------------------------------------------------------------------
# Export
# ---------------------------------------------------------------------------------------------------------------------
def export(a: Asset) -> dict:
    folder = EXPORT / a.folder
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / f"{a.name}.ogg"
    master.write_ogg(str(path), a.audio, SR)
    dec, sr = master.read_audio(str(path))
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
    entry.update({k: v for k, v in a.meta.items() if k not in ("chain",)})
    if "chain" in a.meta:
        entry["chain"] = a.meta["chain"]
    return entry


def build_manifest(assets: dict, levels: dict, cue_table: dict, music_entries: list[dict]) -> dict:
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
    cues["monster_death"]["vocalLayer"] = {
        "families": ["humanoid", "slime"],
        "assets": {f: [f"SW_SFX_MonsterDeathVocal_{pascal(f)}_{v:02d}" for v in (1, 2)] for f in ("humanoid", "slime")},
        "gain": 1.0,
    }
    music = {}
    for e in music_entries:
        if e["asset"] in assets:
            a = assets[e["asset"]]
            music[e["key"]] = {"asset": e["asset"], "loop": e["loop"], "lengthSec": a["lengthSec"],
                               "loopStartSample": 0, "loopEndSample": a["loopEndSample"], "gain": 1.0,
                               "source": e["kind"], "credit": e.get("credit")}
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
        "stingers": {
            "chapterCard": {"dawn": "SW_STG_ChapterCardDawn"},
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
        "ambience": {
            "themes": {"plains": "SW_AMB_Plains"},
            "zones": {"emerald_plains": "SW_AMB_Plains", "ember_tower": "SW_AMB_Plains"},
            "fadeInSec": 2.5,
            "fadeOutSec": 1.5,
            "gain": 1.0,
            "duckInCinematicDb": -6.0,
        },
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
        "assets": assets,
    }


# ---------------------------------------------------------------------------------------------------------------------
def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--only", default="", help="comma list: sfx,vocals,footsteps,ambience,stingers,music")
    ap.add_argument("--later-recordings", action="store_true", help="also transcode the later chapters' recordings")
    ap.add_argument("--reports", action="store_true", help="write waveform / spectrum PNGs into Reports/")
    args = ap.parse_args()
    only = {s.strip() for s in args.only.split(",") if s.strip()}

    def want(part: str) -> bool:
        return not only or part in only

    music_json = json.loads((DATA / "music.json").read_text(encoding="utf-8"))
    cue_table = json.loads((DATA / "audio_cues.json").read_text(encoding="utf-8"))
    port_scores = json.loads((HERE / "data" / "port_scores.json").read_text(encoding="utf-8"))
    composer.load_tables(music_json["composer"])

    entries = list(CH1_MUSIC)
    if args.later_recordings:
        for theme, state, title, author, url, loop in LATER_RECORDINGS:
            entries.append({"key": f"{theme}_{state}", "asset": f"SW_MUS_{pascal(theme)}{pascal(state)}",
                            "kind": "recording", "file": f"{theme}_{state}.mp3", "theme": theme, "loop": loop,
                            "credit": {"title": title, "author": author, "license": "CC0", "url": url}})

    manifest_path = EXPORT / "audio_manifest.json"
    previous = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {}
    levels: dict = dict(previous.get("levels", {})) if only else {}

    recipes_dump: dict = {}
    assets: list[Asset] = []
    t0 = time.time()
    if want("music"):
        log("music ...")
        assets += render_music(music_json, port_scores, entries)
    if want("sfx"):
        log("sfx ...")
        assets += render_sfx(cue_table, recipes_dump)
    if want("vocals"):
        log("vocals ...")
        assets += render_vocals(cue_table, recipes_dump)
    if want("footsteps"):
        log("footsteps ...")
        assets += render_footsteps(recipes_dump)
    if want("ambience"):
        log("ambience ...")
        assets += render_ambience(recipes_dump)
    if want("stingers"):
        log("stingers ...")
        assets += render_stingers(music_json, recipes_dump)
    log(f"rendered {len(assets)} assets in {time.time() - t0:.1f} s; levelling ...")

    if only and ("sfx" not in only) and any(a.kind in ("vocal", "footstep") for a in assets):
        # A7 cues are levelled against `hit`: render it for the reference without exporting it again.
        ref_hits = [a for a in render_sfx(cue_table, {}) if a.meta["cue"] == "hit"]
        level_assets(assets + ref_hits, levels)
    else:
        level_assets(assets, levels)

    log("exporting ...")
    entries_out = dict(previous.get("assets", {})) if only else {}
    for a in assets:
        entries_out[a.name] = export(a)
    entries_out = dict(sorted(entries_out.items()))

    manifest = build_manifest(entries_out, levels, cue_table, entries)
    EXPORT.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")

    if recipes_dump:
        dump_path = HERE / "data" / "sfx_recipes.json"
        old = json.loads(dump_path.read_text(encoding="utf-8")).get("recipes", {}) if (only and dump_path.exists()) else {}
        old.update(recipes_dump)
        dump_path.write_text(json.dumps({"schemaVersion": 1, "generator": "render_all.py", "sampleRate": SR,
                                         "recipes": dict(sorted(old.items()))}, indent=1) + "\n", encoding="utf-8")

    if args.reports:
        from abyss_audio import report
        report.write_reports(assets, REPORTS, SR)
    total = sum(e["bytes"] for e in entries_out.values())
    log(f"done: {len(entries_out)} assets, {total / 1e6:.2f} MB OGG, {time.time() - t0:.1f} s")
    return 0


if __name__ == "__main__":
    sys.exit(main())

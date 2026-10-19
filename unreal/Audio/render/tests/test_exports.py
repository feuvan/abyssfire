"""The shipped exports (Export/*.ogg + audio_manifest.json), as UE will import them (audio.md 9.9: loudness and loop
checks CI can run). The other tests rebuild recipes and scores in memory; this one reads what is committed:

* every manifest asset is a file whose sha256 / size match, decodes to the manifest's frames and channels at 48 kHz,
  and no OGG in Export is missing from the manifest;
* the channel rule per kind (sfx / vocal / footstep mono; bells as their recipe: stereo except the cracked note and
  the far toll; emitters mono; beds, music, stingers stereo);
* loudness: music at its target (-16, or the AU3 track's own; under the momentary ceiling), ambience at its target,
  every hit-relative group (A7, AU1, AU2) at its offset; true peak <= -1 dBTP; no limiting;
* loops are whole-file (loopEndSample = frames) with a clean seam; no SFX cue loops (EvSfx has no stop);
* every table reference resolves (cues, vocal layer, voice families, zone overrides, music director, ambience,
  stingers, jukebox); the held combat override is not live;
* the Chapter 1 package budget (README "Package budget"): OGG per folder, retained (ADPCM) memory under the import
  rules of ue/import_audio.py.

Decoding every file takes a few seconds; the slowest check (integrated loudness of the music) is limited to music and
ambience."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import pathlib
import sys
import types

import numpy as np
import pytest

from abyss_audio import loops, master
from abyss_audio.recipes import dark_ambience, dark_music, dark_vox

AUDIO = pathlib.Path(__file__).resolve().parents[2]
EXPORT = AUDIO / "Export"
MANIFEST = EXPORT / "audio_manifest.json"
SR = 48000

pytestmark = pytest.mark.skipif(not MANIFEST.exists(), reason="no rendered exports in this checkout")

# Chapter 1 package budget (README "Package budget"; audio.md 9.1 estimated ~10 MB of music and < 1 MB of SFX for the
# web port: the remake adds AU1-AU3 and keeps the CC0 recordings for the jukebox and the Ember Tower).
BUDGET_OGG_MB = {"Music": 22.0, "Ambience": 4.0, "SFX": 2.5, "Stingers": 1.2, "Footsteps": 0.1}
BUDGET_TOTAL_OGG_MB = 29.0
BUDGET_RETAINED_MB = 2.5  # RETAIN_ON_LOAD ADPCM, resident for the whole session (4 bits per sample)


@pytest.fixture(scope="module")
def manifest() -> dict:
    return json.loads(MANIFEST.read_text(encoding="utf-8"))


@pytest.fixture(scope="module")
def decoded(manifest) -> dict:
    out = {}
    for name, e in manifest["assets"].items():
        x, sr = master.read_audio(str(EXPORT / e["file"]))
        out[name] = (x, sr)
    return out


def _import_rules():
    """ue/import_audio.py's settings_for, loaded without the editor (its `unreal` import is stubbed)."""
    spec = importlib.util.spec_from_file_location("import_audio_rules", AUDIO / "ue" / "import_audio.py")
    mod = importlib.util.module_from_spec(spec)
    saved = sys.modules.get("unreal")
    sys.modules["unreal"] = types.ModuleType("unreal")
    try:
        spec.loader.exec_module(mod)
    finally:
        if saved is None:
            del sys.modules["unreal"]
        else:
            sys.modules["unreal"] = saved
    return mod


def test_files_match_the_manifest(manifest, decoded):
    for name, e in manifest["assets"].items():
        path = EXPORT / e["file"]
        data = path.read_bytes()
        assert hashlib.sha256(data).hexdigest() == e["sha256"] and len(data) == e["bytes"], name
        assert e["file"] == f"{path.parent.name}/{name}.ogg" and e["ue"].endswith(f"/{name}.{name}"), name
        x, sr = decoded[name]
        assert sr == SR == e["sampleRate"], name
        assert x.shape == (e["channels"], e["frames"]), (name, x.shape)
    on_disk = {p.relative_to(EXPORT).as_posix() for p in EXPORT.rglob("*.ogg")}
    assert on_disk == {e["file"] for e in manifest["assets"].values()}, "orphan or missing OGG files"


def test_channel_rule_per_kind(manifest):
    bells = {}
    for cue, (fn, n, *_rest) in dark_ambience.BELL_CUES.items():
        for v in range(n):
            bells[cue] = fn(v).channels
    emitters = {name for name, (_fn, _lv, role, _row) in dark_ambience.AMBIENCE.items() if role == "emitter"}
    for name, e in manifest["assets"].items():
        kind, ch = e["kind"], e["channels"]
        if kind in ("sfx", "vocal", "footstep"):
            assert ch == 1, name
        elif kind == "bell":
            assert ch == bells[e["cue"]], name
        elif kind == "ambience":
            assert ch == (1 if name in emitters else 2), name
        else:
            assert kind in ("music", "stinger") and ch == 2, name
    assert bells["bell_cracked"] == 1 and bells["bell_toll_far"] == 1 and bells["bell_toll"] == 2


def test_music_and_ambience_loudness(manifest, decoded):
    for name, e in manifest["assets"].items():
        if e["kind"] not in ("music", "ambience"):
            continue
        x, _ = decoded[name]
        lufs = master.integrated_lufs(x, SR)
        if e["kind"] == "music":
            target = e.get("targetLufs", manifest["levels"]["musicTargetLufs"])
            if "momentaryCeilingLufs" in e:  # AU3: the momentary maximum stays under target + the ceiling
                ceiling = e["momentaryCeilingLufs"]
                assert e["momentaryMaxLufs"] <= ceiling + 0.3, name
                if e.get("levelledBy") == "momentaryCeiling":
                    assert abs(e["momentaryMaxLufs"] - ceiling) <= 0.3 and lufs < target, name
                    continue
            assert abs(lufs - target) <= 0.5, (name, lufs, target)
        elif "targetLufs" in e:
            assert abs(lufs - e["targetLufs"]) <= 0.5, (name, lufs)
    lute = manifest["assets"]["SW_MUS_KnellwatchLute"]
    assert lute["momentaryMaxLufs"] <= -13.5  # the sparse lute never out-shouts the boss track


def test_hit_relative_groups(manifest):
    a = manifest["assets"]
    lv = manifest["levels"]
    hit = lv["hitMomentaryMaxLufs"]
    groups: dict[str, list] = {}
    for name, e in a.items():
        if "a7" in e and e["a7"] != "ambience":  # (the A7 plains bed is levelled on its integrated loudness)
            who = e.get("family", e.get("surface", e.get("cue")))
            groups.setdefault(f"{e['a7']}:{who}", []).append(e)
        elif "levelGroup" in e:
            groups.setdefault(f"ambience:{e['levelGroup']}", []).append(e)
    assert set(groups) == set(lv["a7"]), set(groups) ^ set(lv["a7"])
    for key, rows in groups.items():
        m = float(np.mean([r["momentaryMaxLufs"] for r in rows]))
        assert abs(m - (hit + lv["a7"][key]["relativeToHitLu"])) <= 0.5, (key, m - hit)
    knocks = [a[n]["momentaryMaxLufs"] for n in a if n.startswith("SW_AMB_AshwheatKnock_")]
    assert max(knocks) < hit  # the fist door stays under a combat hit


def test_peaks_and_limiting(manifest):
    for name, e in manifest["assets"].items():
        assert e["truePeakDbtp"] <= -1.0 + 0.05, name
        assert e["limiterGainReductionDb"] == 0.0, name


def test_loops_are_whole_file_and_seamless(manifest, decoded):
    for name, e in manifest["assets"].items():
        if e["loop"]:
            assert e["kind"] in ("music", "ambience", "stinger"), f"{name}: a looping {e['kind']} cannot be stopped"
            assert e["loopStartSample"] == 0 and e["loopEndSample"] == e["frames"], name
            assert loops.seam_ratio(decoded[name][0]) < 1.0, name
        else:
            assert e["loopEndSample"] == 0, name
    for cid, c in manifest["cues"].items():
        assert not c.get("loop", False), cid


def test_references_resolve(manifest):
    a, cues, music = manifest["assets"], manifest["cues"], manifest["music"]
    for cid, c in cues.items():
        assert c["assets"] and set(c["assets"]) <= set(a), cid
        if c["families"]:
            assert len(c["assets"]) == len(c["families"]) * c["variants"], cid
    layer = cues["monster_death"]["vocalLayer"]
    assert all(set(v) <= set(a) for v in layer["assets"].values())
    vf = manifest["voiceFamilies"]
    assert set(vf["monsters"].values()) <= set(vf["families"]) and not set(vf["silent"]) & set(vf["monsters"])
    for m, o in vf["overrides"].items():
        assert m in vf["monsters"] and set(o.values()) <= set(cues), m
    assert not set(manifest["plannedCues"]) & set(cues)
    for key, row in music.items():
        assert row["asset"] in a and a[row["asset"]]["kind"] == "music", key
    for zone, table in manifest["zoneOverrides"].items():
        assert set(table.values()) <= set(music), zone
    md = manifest["musicDirector"]
    assert set(md["tracks"].values()) <= set(a) and all(s["asset"] in a for s in md["stingers"].values())
    for r in md["regions"]:
        assert r["track"] == "" or r["track"] in md["tracks"], r["id"]
    st = manifest["stingers"]
    assert {st["bossIntro"], st["whisper"], st["cutsceneIn"], *st["chapterCard"].values()} <= set(a)
    amb = manifest["ambience"]
    beds = set(amb["themes"].values()) | set(amb["zones"].values()) | {r["asset"] for r in amb["regions"].values()} | \
        {r["asset"] for r in amb["layers"].values()} | {n for e in amb["emitters"].values() for n in e["assets"]}
    assert beds <= set(a) and all(a[n]["kind"] == "ambience" for n in beds)
    assert all(row["track"] in music for row in manifest["jukebox"]["tracks"])
    for s in manifest["footsteps"]["surfaces"].values():
        assert set(s) <= set(a)


def test_held_combat_override_is_not_live(manifest):
    """The core restarts the zone track on the combat edge; until U7 makes a same-track override a no-op, mapping
    combat to the drones would restart them at every fight (dark_scores.json director.heldOverrides)."""
    held = dark_music.load()["director"].get("heldOverrides", {})
    zone = manifest["musicDirector"]["zone"]
    for state in held:
        assert f"{zone}_{state}" not in manifest["zoneOverrides"].get(zone, {}), state
    assert set(manifest["musicDirector"]["heldOverrides"]) == set(held)


def test_named_voice_tails_are_trimmed(manifest, decoded):
    """AU1 one-shots end right after the sound (dark_vox.trim_tail cuts 20 ms after the last sample above -72 dB re
    peak; the encoder can push that last sample a little earlier): under 80 ms below -72 dB (it was up to 260 ms)."""
    au1 = [n for n, e in manifest["assets"].items() if e["kind"] == "vocal" and
           (e.get("a7") == "named" or e.get("family") in dark_vox.FAMILIES)]
    assert len(au1) >= 40
    for name in au1:
        x, _ = decoded[name]
        env = np.max(np.abs(x), axis=0)
        loud = np.nonzero(env > float(env.max()) * 10 ** (dark_vox.TRIM_DB / 20))[0]
        assert x.shape[1] - int(loud[-1]) <= int(0.08 * SR), (name, (x.shape[1] - int(loud[-1])) / SR)


def test_package_budget(manifest):
    rules = _import_rules()
    ogg: dict[str, int] = {}
    retained = 0.0
    for e in manifest["assets"].values():
        folder = e["file"].split("/")[0]
        ogg[folder] = ogg.get(folder, 0) + e["bytes"]
        if rules.settings_for(e)[0] == "RETAIN_ON_LOAD":
            retained += e["frames"] * e["channels"] * 0.5
            assert e["lengthSec"] <= rules.LONG_ONESHOT_SEC, e["file"]
    for folder, cap in BUDGET_OGG_MB.items():
        assert ogg.get(folder, 0) / 1e6 <= cap, (folder, ogg.get(folder, 0) / 1e6)
    assert sum(ogg.values()) / 1e6 <= BUDGET_TOTAL_OGG_MB
    assert retained / 1e6 <= BUDGET_RETAINED_MB, retained / 1e6
    assert rules.settings_for({"kind": "bell", "lengthSec": 1.2})[0] == "PRIME_ON_LOAD"

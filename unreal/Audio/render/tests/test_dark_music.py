"""AU3 music (data/dark_scores.json played by recipes/dark_music.py): the notation, the score data (bars, ranges,
buses, instruments), recipes that end in silence and rebuild identically, loops that wrap seamlessly, the director
table (tracks, stingers, regions, encounters) and its manifest form."""

import json
import math
import random

import numpy as np
import pytest

from abyss_audio import loops, master
from abyss_audio.params import Param
from abyss_audio.recipes import dark_music as dm
from abyss_audio.recipes import dark_vox

SR = 48000
DATA = dm.load()
PIECES = {p.key: p for p in dm.pieces(DATA)}
RECIPES = {k: dm.build(p) for k, p in PIECES.items()}

# Sounding ranges (MIDI) per instrument: the lute's courses D2-D5; the choir from a low bass (oktavist D2) to a
# soprano A5; low brass C2-D4; strings C3-B4; drones A1-D3; glass and bell high.
RANGES = {"lute": (38, 74), "choir": (38, 81), "brass": (36, 62), "strings": (48, 71), "drone": (33, 50),
          "glass": (64, 86), "bell": (62, 62)}


def test_notation():
    assert dm.pitch_midi("C4") == 60 and dm.pitch_midi("D2") == 38 and dm.pitch_midi("Eb4") == 63
    assert dm.pitch_midi("F#3") == 54 and dm.pitch_midi("Bb2") == 46 and dm.pitch_midi("A4") == 69
    toks, length = dm.parse_notes("D2+A2:3!0.5 | r:1 D4 F4:1!0.7 | R:2!0.2>0.9 X:2/3 x:1/3", 3)
    assert length == pytest.approx(9.0)
    assert [t.item for t in toks] == ["D2+A2", "D4", "F4", "R", "X", "x"]
    assert toks[0].pitches == [38, 45] and toks[1].beat == 4.0 and toks[1].vel == 0.8
    assert toks[3].vel == 0.2 and toks[3].vel_to == 0.9 and toks[4].dur == pytest.approx(2 / 3)
    with pytest.raises(ValueError):
        dm.parse_notes("D4:2 | E4", 3)  # bar line inside a bar
    with pytest.raises(ValueError):
        dm.parse_notes("H4:1", 4)
    # A phrase that starts mid-bar checks its bar lines against the bar, not the phrase.
    dm.parse_notes("x:1 | X:4", 4, offset=3.0)


def test_score_file_shape():
    assert DATA["schemaVersion"] == 1 and DATA["decision"] == "AU3"
    assets = [p.asset for p in PIECES.values()]
    assert len(assets) == len(set(assets))
    for key, p in PIECES.items():
        s = p.spec
        assert s["use"] and s["description"] and s["parts"] and s["buses"]
        assert p.asset.startswith("SW_STG_" if p.stinger else "SW_MUS_"), key
        if p.stinger:
            assert key.startswith("stinger_") and not p.loop
            assert -6.0 <= s["levelVsExploreLu"] <= 0.0 and s["levelMatch"] in ("momentary", "shortTerm")
            assert set(s["duck"]) == {"db", "inSec", "outSec"} and s["duck"]["db"] < 0
        else:
            assert -24.0 <= s["loudnessLufs"] <= -16.0
        ids = [part["id"] for part in s["parts"]]
        assert len(ids) == len(set(ids)), key
        for part in s["parts"]:
            assert part["bus"] in s["buses"] and part["instrument"] in dm.INSTRUMENTS
            dm.part_params(part)  # unknown params raise
            assert 0.0 < part["level"] <= 0.5
    # boss_ch1 keeps its track key and asset (the core's bossMusic and the UE table name it).
    assert PIECES["boss_ch1"].asset == "SW_MUS_BossCh1" and PIECES["boss_ch1"].loop


@pytest.mark.parametrize("key", sorted(PIECES))
def test_events_fit_and_sound_in_range(key):
    p = PIECES[key]
    evs = dm.events(p)
    for part in p.spec["parts"]:
        ev = evs[part["id"]]
        assert ev, f"{part['id']}: no events"
        for e in ev:
            assert 0.0 <= e.t < (p.seconds if p.loop else p.seconds + 1e-9)
            if part["instrument"] in ("drum", "snare"):
                assert e.item in dm.HITS or e.item == "R"
                continue
            lo, hi = RANGES[part["instrument"]]
            for m in e.pitches:
                assert lo <= m <= hi, f"{part['id']}: {m} outside {lo}-{hi}"
    # Deterministic: the same events every time.
    again = dm.events(p)
    assert [(e.t, e.item, e.vel) for ev in evs.values() for e in ev] == \
           [(e.t, e.item, e.vel) for ev in again.values() for e in ev]


def test_written_lengths():
    for key, p in PIECES.items():
        if p.loop:
            assert 40.0 <= p.seconds <= 140.0, key
        else:
            assert 3.0 <= p.seconds <= 12.0, key
    # The night chapter card matches the card (8.9 s, like the dawn stinger); the boss victory one-shot fits the
    # 8 s boss-victory hold (A5) but for its tail.
    assert PIECES["stinger_chapter_night"].seconds == pytest.approx(9.0)
    assert PIECES["valdras_falls"].seconds <= 8.0


@pytest.mark.parametrize("key", sorted(RECIPES))
def test_recipe_is_data_and_rebuilds_identically(key):
    r = RECIPES[key]
    d = r.to_json()
    assert json.loads(json.dumps(d)) == d
    assert dm.build(PIECES[key]).to_json() == d
    mode = "wrap" if PIECES[key].loop else "oneshot"
    assert all(spec["mode"] == mode for spec in r.buses.values())
    assert all(lay.get("bus") in r.buses for lay in r.layers)


def _gain_product(chain: list, t: np.ndarray) -> np.ndarray:
    g = np.ones_like(t)
    for proc in chain:
        if proc["type"] == "gain":
            g = g * Param.from_spec(proc["gain"], 1.0).values_at(t)
    return g


@pytest.mark.parametrize("key", sorted(RECIPES))
def test_layers_are_silent_when_their_sources_stop(key):
    """Every note has decayed when its sources stop (a cut is a click) and no automation starts after its layer."""
    r = RECIPES[key]
    for i, lay in enumerate(r.layers):
        start = min(s["start"] for s in lay["sources"])
        stop = max(s["stop"] for s in lay["sources"])
        t = np.linspace(start, stop, 600)
        g = np.abs(_gain_product(lay["chain"], t))
        at_stop = float(np.abs(_gain_product(lay["chain"], np.array([stop - 1e-7])))[0])
        assert at_stop <= 1e-9 * float(g.max()), f"layer {i}: gain {at_stop:.2e} at its stop {stop}"
        for proc in lay["chain"]:
            a = proc.get("gain") if proc["type"] == "gain" else proc.get("freq")
            if isinstance(a, list):
                assert a[0][2] <= start + 1e-12, f"layer {i}: first event at {a[0][2]} after the start {start}"


def _tiny(loop: bool) -> dm.Piece:
    spec = {"asset": "SW_MUS_Test", "use": "test", "description": "test", "loop": loop, "tempo": 120,
            "beatsPerBar": 4, "bars": 2, "loudnessLufs": -18.0,
            "buses": {"b": {"reverb": {"decay": 1.2, "wet": 0.4, "dry": 1.0, "tone": 3000, "preDelay": 0.02}}},
            "parts": [
                {"id": "lute", "instrument": "lute", "bus": "b", "level": 0.05,
                 "phrases": [{"bar": 1, "notes": "D3+A3:2 F4:1 r:1 | E4:2 D4:1.5 A4:0.5 |"}]},
                {"id": "drum", "instrument": "drum", "bus": "b", "level": 0.2,
                 "phrases": [{"bar": 1, "notes": "X:2 x:2 | X:2 R:2!0.2>1 |"}]},
                {"id": "choir", "instrument": "choir", "bus": "b", "level": 0.05,
                 "phrases": [{"bar": 2, "beat": 2, "notes": "D3+F3:2"}]}]}
    return dm.Piece("test", spec, False)


def test_loops_wrap_seamlessly_and_deterministically():
    p = _tiny(True)
    a = dm.render(p, SR, 9)
    b = dm.render(p, SR, 9)
    assert np.array_equal(a, b)
    assert a.shape == (2, int(round(p.seconds * SR)))
    assert np.all(np.isfinite(a))
    assert loops.seam_ratio(a) < 1.0
    # The choir's last chord and the drum roll ring past the loop end: their tails sound at the start of the loop.
    head = a[:, :int(0.02 * SR)]
    assert float(np.sqrt(np.mean(head ** 2))) > 1e-4


def test_oneshots_end_in_silence():
    y = dm.render(_tiny(False), SR, 9)
    pk = float(np.max(np.abs(y)))
    tail = y[:, -int(0.005 * SR):]
    assert float(np.sqrt(np.mean(tail ** 2))) < 1e-4 * pk
    assert abs(float(np.mean(y))) < 0.01 * pk


def test_drive_saturates_the_attack_only():
    """A driven drum (war drums) clips its attack but decays clean: late in the hit the fundamental is a sine."""
    spec = {"tuneHz": 50.0, "decaySec": 1.0, "drive": 0.5, "skin": 0.0, "beater": 0.0, "slap": 0.0, "modes": []}
    p = dict(dm.DEFAULTS["drum"], **spec)
    r = dm.AmbRecipe("t_drum", 1.5, 2, "", {"b": {"mode": "oneshot"}})
    with r.on("b"):
        dm.drum_hit(r, random.Random(1), 0.0, 0.2, 0.0, p)
    from abyss_audio import sfx
    y = sfx.render(r.to_json(), SR, 1)[0]
    late = y[int(0.6 * SR):int(0.8 * SR)]
    spec_late = np.abs(np.fft.rfft(late * np.hanning(late.shape[0])))
    f = np.fft.rfftfreq(late.shape[0], 1 / SR)
    fund = spec_late[(f > 40) & (f < 60)].max()
    third = spec_late[(f > 140) & (f < 160)].max()
    assert third < 0.03 * fund


def test_lute_is_slightly_detuned():
    """Each course is two strings a few cents apart (and the tuning map nudges some strings): it beats."""
    p = PIECES["knellwatch_lute"]
    part = next(pt for pt in p.spec["parts"] if pt["id"] == "lute")
    params = dm.part_params(part)
    assert 3.0 <= params["courseDetuneCents"] <= 12.0
    assert params["tuningCents"] and all(abs(c) <= 12.0 for c in params["tuningCents"].values())
    r = dm.AmbRecipe("t_lute", 4.0, 2, "", {"b": {"mode": "oneshot"}})
    with r.on("b"):
        dm.lute(r, random.Random(1), dm.Event(0.0, 2.0, "D4", 0.7, None), 0.05, 0.0, params)
    fund = sorted({lay["sources"][0]["freq"][-1][1] for lay in r.layers if lay["sources"][0]["kind"] == "osc"})[:2]
    cents = 1200.0 * math.log2(fund[1] / fund[0])
    assert cents == pytest.approx(params["courseDetuneCents"], abs=0.5)


def test_no_twelve_string_figure():
    """Story bible: never a 12-string town theme. The camp piece is sparse (rests outweigh the notes over the loop)
    and never runs a continuous arpeggio: no more than four plucks in a row without a rest or a held note."""
    p = PIECES["knellwatch_lute"]
    beats = p.beats
    sounding = 0.0
    run = 0
    longest = 0
    for ph in next(pt for pt in p.spec["parts"] if pt["id"] == "lute")["phrases"]:
        toks, _ = dm.parse_notes(ph["notes"], p.spec["beatsPerBar"], float(ph.get("beat", 0.0)))
        for t in toks:
            sounding += t.dur
        for raw in ph["notes"].split():
            if raw == "|":
                continue
            item, _, rest = raw.partition(":")
            dur = float(rest.split("!")[0]) if rest else 1.0
            run = run + 1 if item != "r" and dur <= 1.0 else 0
            longest = max(longest, run)
    assert sounding / beats < 0.7
    assert longest <= 4


def test_the_music_sits_on_the_bells_d():
    """The keepers' bell is in D (AU2): every track's lowest part centres on D (or its fifth, A)."""
    for key, p in PIECES.items():
        evs = dm.events(p)
        lows = [min(e.pitches) % 12 for part in p.spec["parts"] if part["instrument"] in ("drone", "choir", "brass",
                                                                                           "lute")
                for e in evs[part["id"]]]
        if lows:
            counts = {pc: lows.count(pc) for pc in set(lows)}
            assert max(counts, key=counts.get) in (2, 9), key  # D or A


def test_director_references_resolve():
    d = DATA["director"]
    tracks, stingers = set(DATA["tracks"]), set(DATA["stingers"])
    assert d["zone"] == "emerald_plains"
    assert set(d["zoneTracks"]) == {"explore", "combat", "victory"} and set(d["zoneTracks"].values()) <= tracks
    assert set(d.get("heldOverrides", {})) <= set(d["zoneTracks"]) and all(d.get("heldOverrides", {}).values())
    assert d["priority"][0] == "story"
    ids = [r["id"] for r in d["regions"]]
    assert len(ids) == len(set(ids)) and {"breach_rim", "knellwatch", "red_field"} <= set(ids)
    for r in d["regions"]:
        assert r["track"] == "" or r["track"] in tracks
        assert 0 <= r["col"] < 120 and 0 <= r["row"] < 120 and 0 < r["radius"] <= r["exitRadius"]
        assert r["source"] in ("design", "AU3 proposal") and r["use"]
        for cond, monster in r.get("while", {}).items():
            assert cond == "monsterAlive" and monster in dark_vox.MONSTER_FAMILY
    breach = next(r for r in d["regions"] if r["id"] == "breach_rim")
    assert (breach["col"], breach["row"], breach["radius"], breach["track"]) == (15, 95, 16, "")
    knell = next(r for r in d["regions"] if r["id"] == "knellwatch")
    assert (knell["col"], knell["row"], knell["radius"]) == (15, 15, 9)
    for boss, row in d["bosses"].items():
        assert boss in dark_vox.MONSTER_FAMILY and row["track"] in tracks and row["victory"] in tracks
        assert set(row["phaseStingers"].values()) <= stingers
    for row in d["encounters"].values():
        assert {row["engaged"], row["slain"]} <= stingers
        assert all(m in dark_vox.MONSTER_FAMILY for m in row["monsters"])
    for beat in d["story"]:
        if "track" in beat:
            assert beat["track"] == "" or beat["track"] in tracks
        if "stinger" in beat:
            assert beat["stinger"] in stingers
        then = beat.get("then")
        if isinstance(then, dict):
            assert then["track"] in tracks
    # U7 reads the beats it acts on from these fields, not from the prose (`until`, `at`).
    siege = next(b for b in d["story"] if b["beat"] == "cs_rf_siege")
    assert (siege["at"], siege["track"], siege["quest"], siege["then"]) == ("end", "siege", "q_rf_last_toll", "")
    finale = next(b for b in d["story"] if b["beat"] == "cs_rf_finale")
    assert (finale["at"], finale["stepIndex"], finale["track"]) == ("step 1", 0, "")
    assert finale["then"] == {"afterSec": 6.0, "track": "knellwatch_lute", "fadeInSec": 6.0}
    # Every track and stinger is used by the director.
    used = set(d["zoneTracks"].values()) | {r["track"] for r in d["regions"]} | \
        {b.get("track") for b in d["story"]} | {b["then"]["track"] for b in d["story"] if isinstance(b.get("then"),
                                                                                                  dict)} | \
        {row["track"] for row in d["bosses"].values()} | {row["victory"] for row in d["bosses"].values()}
    assert tracks <= used
    used_st = {s for row in d["bosses"].values() for s in row["phaseStingers"].values()} | \
        {s for row in d["encounters"].values() for s in (row["engaged"], row["slain"])} | \
        {b["stinger"] for b in d["story"] if "stinger" in b}
    assert stingers <= used_st


def test_manifest_tables():
    import render_all

    over = render_all.zone_overrides(DATA)
    assert over["ember_tower"] == render_all.ZONE_OVERRIDES["ember_tower"]
    # U7 released the combat -> drones row (UAbyssAudioSystem: a zone-overridden request that resolves to the track
    # already sounding is a no-op even with restart = true, so the combat edge keeps the drones going).
    assert "heldOverrides" not in DATA["director"]
    assert over["emerald_plains"] == {"emerald_plains_explore": "march_drones", "emerald_plains_combat": "march_drones",
                                      "emerald_plains_victory": "valdras_falls"}
    held = dict(DATA, director=dict(DATA["director"], heldOverrides={"combat": "waits for a reader"}))
    assert "emerald_plains_combat" not in render_all.zone_overrides(held)["emerald_plains"]
    entries = render_all.dark_entries(DATA)
    assert {e["key"] for e in entries} == set(DATA["tracks"])
    assert all(e["kind"] == "dark" and e["asset"].startswith("SW_MUS_") for e in entries)
    assert not any(e["key"] == "boss_ch1" for e in render_all.CH1_MUSIC)  # the dark score replaces it
    music = {e["key"]: {"asset": e["asset"]} for e in entries}
    assets = {p.asset: {"lengthSec": 1.0} for p in PIECES.values()}
    t = render_all.music_director_table(DATA, music, assets)
    assert t["decision"] == "AU3" and t["tracks"]["boss_ch1"] == "SW_MUS_BossCh1"
    assert t["zoneTracks"]["combat"] == "march_drones" and "heldOverrides" not in t
    assert t["bosses"]["valdras"]["phaseStingers"] == {"1": "stinger_valdras_roll_call",
                                                       "2": "stinger_valdras_no_retreat"}
    assert set(t["stingers"]) == set(DATA["stingers"])
    # A manifest rebuilt before AU3 was rendered lists no AU3 track or stinger (the regions keep the silences).
    t0 = render_all.music_director_table(DATA, {}, {})
    assert t0["tracks"] == {} and t0["stingers"] == {} and all(r["track"] == "" for r in t0["regions"])


def test_levels_leave_headroom():
    """Rendered at score level, nothing clips before mastering and every track has room to reach its target."""
    p = PIECES["stinger_valdras_roll_call"]
    y = dm.render(p, SR, 3)
    assert float(np.max(np.abs(y))) < 1.0
    gain = -16.0 - master.integrated_lufs(y, SR)
    assert -20.0 < gain < 30.0

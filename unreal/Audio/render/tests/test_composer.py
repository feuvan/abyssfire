"""Golden vectors of audio.md 6.3 / 6.6 / 6.8 (generated from the TypeScript; must match exactly)."""

import json
import pathlib

import pytest

from abyss_audio import SAMPLE_RATE, composer
from abyss_audio.score import ScorePlayer, ScoreSpec

DATA = pathlib.Path(__file__).resolve().parents[3] / "Data" / "music.json"


@pytest.fixture(scope="module")
def music():
    d = json.loads(DATA.read_text(encoding="utf-8"))
    composer.load_tables(d["composer"])
    return d


def test_make_rng_golden():
    r = composer.make_rng(1)
    got = [r() for _ in range(6)]
    want = [0.0000629502, 0.0157474282, 0.6164041024, 0.0716186350, 0.5584883580, 0.1735741980]
    assert got == pytest.approx(want, abs=5e-11)
    r = composer.make_rng(12345)
    assert [r() for _ in range(4)] == pytest.approx([0.7769387052, 0.3951726963, 0.6557702795, 0.4552956768],
                                                    abs=5e-11)
    a, b = composer.make_rng(0), composer.make_rng(1)
    assert [a() for _ in range(5)] == [b() for _ in range(5)]


def _events(ev):
    return [(e.beat, e.length, e.midi, round(e.velocity, 4)) for e in ev]


def test_write_phrase_golden(music):
    ev = composer.write_phrase(62, "major", [0, 4], 4, 69, 86, 0.6, composer.make_rng(7))
    assert _events(ev) == [(0, 2, 78, 0.8615), (2, 1, 78, 0.8309), (3, 1, 74, 0.6336), (4, 2, 73, 0.8228),
                           (6, 2, 73, 0.8161)]
    ev = composer.write_phrase(57, "dorian", [0, 3], 3, 64, 81, 0.45, composer.make_rng(3))
    assert _events(ev) == [(0, 2, 72, 0.825), (2, 1, 71, 0.6793), (3, 1, 74, 0.8786), (4, 2, 74, 0.6708)]


CHORDS = {
    # score: [(root, pad, arp, bell, (bass tri, bass sine))]
    "emerald_plains": [
        (0, [62, 66, 69], [62, 66, 69], [78, 81, 86], (50, 62)),
        (4, [61, 64, 69], [69, 73, 76], [76, 81, 85], (45, 57)),
        (5, [62, 66, 71], [71, 74, 78], [78, 83, 86], (47, 59)),
        (3, [62, 67, 71], [67, 71, 74], [79, 83, 86], (43, 55)),
    ],
    "menu": [
        (0, [57, 60, 64, 67], [64, 69, 72], [76, 81, 84], (45, 57)),
        (5, [64, 65, 69, 72], [65, 69, 72], [77, 81, 84], (41, 53)),
        (2, [60, 64, 67, 71], [64, 67, 72], [76, 79, 84], (48, 60)),
        (6, [62, 65, 67, 71], [67, 71, 74], [79, 83, 86], (43, 55)),
    ],
    "abyss_rift": [
        (0, [55, 59, 60, 63], [60, 63, 67], [79, 84, 87], (48, 60)),
        (5, [56, 60, 63, 67], [60, 63, 68], [80, 84, 87], (44, 56)),
        (3, [56, 60, 63, 65], [60, 65, 68], [77, 80, 84], (41, 53)),
        (4, [55, 59, 62, 65], [62, 67, 71], [79, 83, 86], (43, 55)),
    ],
}


@pytest.mark.parametrize("score_id", sorted(CHORDS))
def test_chord_tables(music, score_id):
    s = ScoreSpec.from_json(music["scores"][score_id])
    for root, pad, arp, bell, bass in CHORDS[score_id]:
        assert composer.voice_chord(s.tonic, s.mode, composer.chord_degrees(root, s.mode != "major"), 55, 72) == pad
        assert composer.voice_chord(s.tonic, s.mode, composer.chord_degrees(root), s.arp["low"], s.arp["high"]) == arp
        assert composer.voice_chord(s.tonic, s.mode, composer.chord_degrees(root), 76, 91) == bell
        n = composer.degree_to_midi(s.tonic, s.mode, root) - 12
        while n < 40:
            n += 12
        while n > 52:
            n -= 12
        assert (n, n + 12) == bass


def _player(music, score_id, state, bars, seed=42):
    p = ScorePlayer(ScoreSpec.from_json(music["scores"][score_id]), state, 0.0, seed, SAMPLE_RATE)
    p.schedule_bars(bars)
    return p


def test_score_player_plains_explore(music):
    p = _player(music, "emerald_plains", "explore", 32)
    t = p.trace
    assert t.oscillators == 368
    assert t.noise_starts == 0
    assert len(t.leads) == 26
    assert [(b, m, round(v, 4)) for b, m, v in t.leads[:5]] == [(25, 78, 0.6083), (26, 79, 0.8747), (28, 79, 0.8404),
                                                                (30, 79, 0.8275), (32, 78, 0.8125)]
    assert t.bells == [(0, 81), (8, 85), (48, 86), (56, 83), (64, 78), (73, 76), (112, 78)]


def test_score_player_plains_combat(music):
    p = _player(music, "emerald_plains", "combat", 16)
    t = p.trace
    assert t.oscillators == 351
    assert t.noise_starts == 64
    assert len(t.leads) == 30
    assert [(b, m, round(v, 4)) for b, m, v in t.leads[:5]] == [(0, 78, 0.8103), (2, 78, 0.8414), (3, 76, 0.6681),
                                                                (4, 74, 0.8265), (6, 74, 0.782)]


def test_score_player_menu(music):
    p = _player(music, "menu", "explore", 32)
    t = p.trace
    assert t.oscillators == 326
    assert len(t.leads) == 24
    assert [(b, m, round(v, 4)) for b, m, v in t.leads[:3]] == [(24, 71, 0.7985), (27, 72, 0.6698), (28, 71, 0.782)]
    assert t.bells == [(0, 81), (8, 84), (49, 76), (57, 79), (65, 81), (72, 84), (113, 84)]


def test_score_invariants(music):
    for sid, d in music["scores"].items():
        ScoreSpec.from_json(d).check_invariants()

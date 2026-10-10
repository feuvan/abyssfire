"""Story stingers (NEW, audio.md 10.2 / 10.3): chapter card (dawn), boss intro, whisper loop, cutscene in.

Written in the SFX recipe language with the score's voice shapes (pad: oscillators -> envelope gain -> lowpass;
bell: the ScorePlayer bell voice; drum: the ScorePlayer percussion drum). Rendered mono, then through the emerald
plains effects chain (stereo) and gain-matched to a short-term loudness 3 LU under the plains explore track.
"""

from __future__ import annotations

from ..composer import midi_to_hz
from ..sfx import Recipe, adsr, filt, gain, lfo, noise, osc, pluck_env

SRC = "unreal/Docs/spec/audio.md 10.3"


def _bell(r: Recipe, midi: float, t: float, level: float) -> None:
    """ScorePlayer.bellTone voice: sine n pluck(t, g, 2.6) stop t + 2.7 + sine 2.76 f pluck(t, 0.3 g, 1.2) stop t + 1.3."""
    f = midi_to_hz(midi)
    r.layer([osc("sine", [["set", f, t]], t, t + 2.7)], [gain(pluck_env(level, 2.6, t))])
    r.layer([osc("sine", [["set", f * 2.76, t]], t, t + 1.3)], [gain(pluck_env(level * 0.3, 1.2, t))])


def _drum(r: Recipe, t: float, peak: float) -> None:
    """ScorePlayer percussion drum: sine 220 -> 110 Hz (exp, 0.18 s), pluck(t, peak, 0.22), stop t + 0.25."""
    r.layer([osc("sine", [["set", 220, t], ["exp", 110, t + 0.18]], t, t + 0.25)], [gain(pluck_env(peak, 0.22, t))])


def chapter_card_dawn() -> Recipe:
    r = Recipe("stg_chapter_card_dawn", 9.0, description="chapter card, D major dawn (8.9 s card)", source=SRC)
    # (1) pad A3 D4 F#4 A4, triangle pairs +-6 cents -> gain -> lowpass.
    srcs = []
    for m in (57, 62, 66, 69):
        f = midi_to_hz(m)
        srcs.append(osc("triangle", [["set", f, 0]], 0, 9.0, detune=[["set", -6, 0]]))
        srcs.append(osc("triangle", [["set", f, 0]], 0, 9.0, detune=[["set", 6, 0]]))
    r.layer(srcs, [gain([["set", 0.0001, 0], ["lin", 0.025, 1.1], ["set", 0.025, 7.3], ["exp", 0.0001, 8.9]]),
                   filt("lowpass", [["set", 700, 0], ["lin", 1800, 1.1], ["lin", 1200, 7.3]])])
    # (2) bell arpeggio at the title reveal.
    for m, t in ((74, 1.10), (78, 1.28), (81, 1.46), (86, 1.64)):
        _bell(r, m, t, 0.05)
    # (3) breath: noise -> BP(400, 0.7).
    r.layer([noise(0, 3.0)], [filt("bandpass", 400, 0.7), gain([["set", 0, 0], ["lin", 0.03, 1.0], ["exp", 0.001, 3.0]])])
    # (4) subtitle chime: sine D6 pluck(2.0, 0.03, 1.5).
    r.layer([osc("sine", [["set", 1174.66, 2.0]], 2.0, 3.6)], [gain(pluck_env(0.03, 1.5, 2.0))])
    return r


def boss_intro() -> Recipe:
    r = Recipe("stg_boss_intro", 3.5, description="boss intro title (band, slash, epithet, hold, fade)", source=SRC)
    # (1) boom.
    r.adsr_osc("sine", [["set", 70, 0], ["exp", 35, 0.5]], 0, 0.75, adsr(0.002, 0.1, 0.3, 0.6, 0.35, 0))
    r.noise_burst(0.25, 300, "lowpass", 0, 0.3)
    # (2) slash rip synced with the slash bar (0.38 s).
    r.adsr_osc("sawtooth", [["set", 300, 0], ["exp", 2400, 0.38]], 0, 0.55, adsr(0.01, 0.2, 0.3, 0.3, 0.12, 0),
               [filt("bandpass", 1800, 2)])
    # (3) brass cluster D3 A3 Eb4, saw pairs +-8 cents -> gain -> lowpass.
    srcs = []
    for m in (50, 57, 63):
        f = midi_to_hz(m)
        srcs.append(osc("sawtooth", [["set", f, 0.25]], 0.25, 3.5, detune=[["set", -8, 0.25]]))
        srcs.append(osc("sawtooth", [["set", f, 0.25]], 0.25, 3.5, detune=[["set", 8, 0.25]]))
    r.layer(srcs, [gain([["set", 0.0001, 0.25], ["lin", 0.06, 0.8], ["set", 0.06, 2.98], ["exp", 0.0001, 3.5]]),
                   filt("lowpass", [["set", 400, 0.3], ["lin", 1500, 0.8], ["lin", 600, 3.4]])])
    # (4) two score drums.
    _drum(r, 0.78, 0.09)
    _drum(r, 1.05, 0.09)
    return r


WHISPER_RENDER_SEC = 8.0
WHISPER_LOOP_SEC = 6.0


def whisper_loop() -> Recipe:
    """Rendered 8 s; loops.crossfade_loop folds the last 2 s into the start -> a 6 s loop."""
    r = Recipe("stg_whisper_loop", WHISPER_RENDER_SEC, description="whisper bed (6 s loop)", source=SRC)
    end = WHISPER_RENDER_SEC
    r.layer([noise(0, end)], [filt("bandpass", [["set", 900, 0], ["lin", 1600, 3], ["lin", 900, 6]], 4),
                              gain([["set", 0.03, 0]])])
    for f, g in ((311.13, 0.012), (329.63, 0.012), (466.16, 0.008)):
        r.layer([osc("sine", [["set", f, 0]], 0, end, lfo=[lfo("detune", "sine", 5, 15, 0, end)])],
                [gain([["set", g, 0]])])
    return r


def cutscene_in() -> Recipe:
    r = Recipe("stg_cutscene_in", 1.2, description="letterbox bars slide in", source=SRC)
    r.layer([noise(0, 0.45)], [filt("bandpass", [["set", 600, 0], ["exp", 1800, 0.45]], 1),
                               gain(adsr(0.05, 0.2, 0.3, 0.2, 0.08, 0))])
    _bell(r, 62, 0.0, 0.04)
    return r


ALL = [chapter_card_dawn, boss_intro, whisper_loop, cutscene_in]

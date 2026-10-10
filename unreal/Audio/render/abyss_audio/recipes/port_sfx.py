"""DECISIONS A7: plains ambience bed, hero footsteps (grass / dirt / stone) and basic monster vocalisations (aggro, hurt,
death per family: humanoid = the goblin family, slime). Not in the web game; designed here in the same recipe
language as the web cues (oscillators, seeded noise, Web Audio biquads / shapers, ADSR) so they sit in the same
sonic world. Variants differ by noise seed and by small deterministic pitch / timing offsets (`variant` argument).
"""

from __future__ import annotations

import random

from ..sfx import Recipe, adsr, filt, gain, lfo, noise, osc, pluck_env, shape

SRC = "unreal/Docs/DECISIONS.md A7 (new content)"

# Per-variant pitch factors / timing jitter (deterministic): variants must sound like the same source, not identical.
PITCH = (1.0, 0.95, 1.06, 0.98)
TIMING = (0.0, 0.006, -0.004, 0.003)


def _p(v: int) -> float:
    return PITCH[v % len(PITCH)]


# ---------------------------------------------------------------------------------------------------------------------
# Footsteps (hero, A7): ~0.15-0.2 s, mono, quiet; triggered by the Run / Walk FootL / FootR notifies.
# ---------------------------------------------------------------------------------------------------------------------
def footstep_grass(v: int) -> Recipe:
    p = _p(v)
    r = Recipe(f"fs_grass_{v + 1:02d}", 0.2, description="footstep on grass: soft thump + leaf swish", source=SRC)
    r.adsr_osc("sine", [["set", 95 * p, 0], ["exp", 52 * p, 0.07]], 0, 0.1, adsr(0.002, 0.03, 0.2, 0.05, 0.13, 0))
    r.layer([noise(0, 0.14)], [filt("bandpass", [["set", 2200 * p, 0], ["exp", 3400 * p, 0.12]], 0.9),
                               gain(adsr(0.006, 0.035, 0.35, 0.08, 0.11, 0))])
    r.layer([noise(0.01, 0.07)], [filt("highpass", 6500), gain(adsr(0.002, 0.015, 0.3, 0.03, 0.035, 0.01))])
    return r


def footstep_dirt(v: int) -> Recipe:
    p = _p(v)
    r = Recipe(f"fs_dirt_{v + 1:02d}", 0.2, description="footstep on dirt: thump + gritty scuff", source=SRC)
    r.adsr_osc("sine", [["set", 115 * p, 0], ["exp", 58 * p, 0.06]], 0, 0.09, adsr(0.001, 0.025, 0.2, 0.05, 0.16, 0))
    r.layer([noise(0, 0.12)], [filt("lowpass", 1300 * p, 1), gain(adsr(0.002, 0.03, 0.3, 0.06, 0.14, 0))])
    r.layer([noise(0.005, 0.1)], [filt("bandpass", 3200 * p, 1.2), gain(adsr(0.003, 0.025, 0.3, 0.05, 0.05, 0.005))])
    return r


def footstep_stone(v: int) -> Recipe:
    p = _p(v)
    r = Recipe(f"fs_stone_{v + 1:02d}", 0.16, description="footstep on stone: hard heel tap", source=SRC)
    r.adsr_osc("sine", [["set", 190 * p, 0], ["exp", 120 * p, 0.05]], 0, 0.07, adsr(0.001, 0.02, 0.15, 0.04, 0.1, 0))
    r.layer([noise(0, 0.06)], [filt("bandpass", 3600 * p, 2.2), gain(adsr(0.001, 0.012, 0.25, 0.035, 0.14, 0))])
    r.layer([noise(0.004, 0.05)], [filt("highpass", 7000), gain(adsr(0.001, 0.01, 0.2, 0.025, 0.05, 0.004))])
    r.adsr_osc("triangle", [["set", 1450 * p, 0]], 0, 0.08, adsr(0.001, 0.015, 0.1, 0.05, 0.012, 0),
               [filt("bandpass", 1450 * p, 6)])
    return r


FOOTSTEPS = {"grass": footstep_grass, "dirt": footstep_dirt, "stone": footstep_stone}


# ---------------------------------------------------------------------------------------------------------------------
# Monster vocalisations (A7): aggro (idle -> chase), hurt (non-lethal non-tick hit), death (layered on monster_death).
# Humanoid = goblin family (nasal, rough sawtooth through formant band-passes); slime = wet gurgle and bubble pops.
# ---------------------------------------------------------------------------------------------------------------------
def _goblin_voice(r: Recipe, freq: list, start: float, stop: float, env: list, formants: tuple, growl_cents: float,
                  growl_rate: float) -> None:
    """One sawtooth 'voice' through two parallel formant band-passes (the same oscillator in two layers)."""
    for (f, q, level) in formants:
        r.layer([osc("sawtooth", freq, start, stop, lfo=[lfo("detune", "triangle", growl_rate, growl_cents, start, stop)])],
                [shape("tanh2"), filt("bandpass", f, q), gain([[k, val * level, t] for k, val, t in env])])


def aggro_humanoid(v: int) -> Recipe:
    p = _p(v)
    t0 = TIMING[v % len(TIMING)]
    r = Recipe(f"vox_aggro_humanoid_{v + 1:02d}", 0.5, description="goblin snarl (aggro)", source=SRC)
    if v % 2 == 0:
        freq = [["set", 250 * p, 0], ["lin", 430 * p, 0.09 + t0], ["exp", 290 * p, 0.42]]
    else:
        freq = [["set", 320 * p, 0], ["lin", 520 * p, 0.06 + t0], ["exp", 360 * p, 0.3], ["exp", 300 * p, 0.42]]
    _goblin_voice(r, freq, 0, 0.45, adsr(0.012, 0.09, 0.6, 0.28, 0.2, 0),
                  ((1050 * p, 3, 1.0), (2450 * p, 4, 0.55)), 45, 17)
    r.layer([noise(0, 0.4)], [filt("bandpass", 1600 * p, 1.4), gain(adsr(0.008, 0.06, 0.45, 0.25, 0.05, 0))])
    return r


def hurt_humanoid(v: int) -> Recipe:
    p = _p(v)
    r = Recipe(f"vox_hurt_humanoid_{v + 1:02d}", 0.3, description="goblin pain yelp", source=SRC)
    freq = [["set", 400 * p, 0], ["lin", 470 * p, 0.03], ["exp", 230 * p, 0.24]]
    _goblin_voice(r, freq, 0, 0.26, adsr(0.003, 0.05, 0.45, 0.15, 0.22, 0),
                  ((1300 * p, 3, 1.0), (2700 * p, 4, 0.5)), 30, 23)
    r.layer([noise(0, 0.12)], [filt("bandpass", 2100 * p, 1.5), gain(adsr(0.002, 0.03, 0.3, 0.06, 0.06, 0))])
    return r


def death_humanoid(v: int) -> Recipe:
    p = _p(v)
    r = Recipe(f"vox_death_humanoid_{v + 1:02d}", 0.75, description="goblin death wail", source=SRC)
    freq = [["set", 330 * p, 0], ["lin", 380 * p, 0.07], ["exp", 105 * p, 0.68]]
    _goblin_voice(r, freq, 0, 0.72, adsr(0.01, 0.1, 0.6, 0.5, 0.2, 0),
                  ((900 * p, 2.5, 1.0), (2150 * p, 4, 0.45)), 60, 9)
    r.layer([noise(0.05, 0.6)], [filt("bandpass", 1200 * p, 1.2), gain(adsr(0.02, 0.1, 0.5, 0.35, 0.045, 0.05))])
    return r


def _pop(r: Recipe, t: float, f0: float, f1: float, peak: float) -> None:
    r.layer([osc("sine", [["set", f0, t], ["exp", f1, t + 0.03]], t, t + 0.06)], [gain(pluck_env(peak, 0.05, t))])


def aggro_slime(v: int) -> Recipe:
    p = _p(v)
    r = Recipe(f"vox_aggro_slime_{v + 1:02d}", 0.55, description="slime gurgle (aggro)", source=SRC)
    r.layer([osc("sine", [["set", 140 * p, 0], ["lin", 230 * p, 0.12], ["exp", 160 * p, 0.5]], 0, 0.52,
                 lfo=[lfo("detune", "sine", 21 + 2 * v, 320, 0, 0.52)])],
            [filt("lowpass", 950 * p, 6), gain(adsr(0.02, 0.1, 0.6, 0.32, 0.26, 0))])
    for t, f0, f1, pk in ((0.05, 600, 1400, 0.06), (0.15 + TIMING[v % 4], 520, 1250, 0.05), (0.29, 700, 1600, 0.045)):
        _pop(r, t, f0 * p, f1 * p, pk)
    r.layer([noise(0, 0.45)], [filt("lowpass", 700 * p, 3), gain(adsr(0.03, 0.1, 0.5, 0.28, 0.05, 0))])
    return r


def hurt_slime(v: int) -> Recipe:
    p = _p(v)
    r = Recipe(f"vox_hurt_slime_{v + 1:02d}", 0.3, description="slime squelch (hurt)", source=SRC)
    r.layer([noise(0, 0.22)], [filt("bandpass", [["set", 380 * p, 0], ["exp", 1700 * p, 0.12]], 3),
                               gain(adsr(0.002, 0.04, 0.35, 0.15, 0.28, 0))])
    r.layer([osc("sine", [["set", 300 * p, 0], ["exp", 115 * p, 0.2]], 0, 0.25)], [gain(pluck_env(0.18, 0.22, 0))])
    _pop(r, 0.08, 800 * p, 1500 * p, 0.04)
    return r


def death_slime(v: int) -> Recipe:
    p = _p(v)
    r = Recipe(f"vox_death_slime_{v + 1:02d}", 0.7, description="slime burst and collapse (death)", source=SRC)
    r.layer([osc("sine", [["set", 230 * p, 0], ["exp", 58 * p, 0.6]], 0, 0.65,
                 lfo=[lfo("detune", "sine", 15 + v, 420, 0, 0.65)])],
            [filt("lowpass", 760 * p, 8), gain(adsr(0.01, 0.12, 0.55, 0.45, 0.26, 0))])
    for t, f0, f1, pk in ((0.1, 900, 1700, 0.06), (0.22, 700, 1300, 0.05), (0.37, 520, 950, 0.04)):
        _pop(r, t + TIMING[v % 4], f0 * p, f1 * p, pk)
    r.noise_burst(0.3, 800 * p, "lowpass", 0, 0.16)
    return r


VOCALS = {
    ("aggro", "humanoid"): aggro_humanoid,
    ("aggro", "slime"): aggro_slime,
    ("hurt", "humanoid"): hurt_humanoid,
    ("hurt", "slime"): hurt_slime,
    ("death", "humanoid"): death_humanoid,
    ("death", "slime"): death_slime,
}


# ---------------------------------------------------------------------------------------------------------------------
# Plains ambience bed (A7): stereo, wind + grass rustle + birds; rendered 40 s, the last 4 s fold into the start.
# ---------------------------------------------------------------------------------------------------------------------
AMBIENCE_RENDER_SEC = 40.0
AMBIENCE_OVERLAP_SEC = 4.0


def _wander(rng: random.Random, start: float, end: float, lo: float, hi: float, step: float) -> list:
    """A slow random-walk automation (linear ramps every `step` seconds), deterministic."""
    pts = [["set", rng.uniform(lo, hi), start]]
    t = start
    while t < end:
        t = min(end, t + step * rng.uniform(0.7, 1.3))
        pts.append(["lin", rng.uniform(lo, hi), t])
    return pts


def ambience_plains() -> Recipe:
    rng = random.Random(0x41B1E)
    end = AMBIENCE_RENDER_SEC
    r = Recipe("amb_plains", end, channels=2, description="plains day: wind, grass, birds (36 s loop)", source=SRC)
    # Wind body: two decorrelated low-passed noise beds, slow cutoff and level wander.
    for pan in (-0.55, 0.55):
        r.layer([noise(0, end)], [filt("lowpass", _wander(rng, 0, end, 380, 950, 3.0), 0.5),
                                  gain(_wander(rng, 0, end, 0.035, 0.075, 4.0))], pan=pan)
    # Grass rustle: high band noise with gentle swells.
    for pan in (-0.8, 0.8):
        r.layer([noise(0, end)], [filt("highpass", 3800), filt("lowpass", 9000),
                                  gain(_wander(rng, 0, end, 0.002, 0.010, 2.5))], pan=pan)
    # Gusts: band-passed swells.
    for t in (3.5, 14.0, 26.5):
        t += rng.uniform(-0.8, 0.8)
        d = rng.uniform(3.2, 4.6)
        r.layer([noise(t, t + d)],
                [filt("bandpass", [["set", 520, t], ["exp", 1300, t + d * 0.45], ["exp", 640, t + d]], 1.2),
                 gain([["set", 0.0001, t], ["lin", rng.uniform(0.03, 0.045), t + d * 0.45], ["exp", 0.0001, t + d]])],
                pan=rng.uniform(-0.6, 0.6))
    # Birds: little songs of up-sweep chirps and warbles at random places in the stereo field.
    t = 1.2
    while t < end - 1.5:
        pan = rng.uniform(-0.85, 0.85)
        base = rng.uniform(2600, 4200)
        level = rng.uniform(0.010, 0.022)
        if rng.random() < 0.6:
            for i in range(rng.randint(2, 5)):
                s = t + i * rng.uniform(0.09, 0.15)
                f0 = base * rng.uniform(0.92, 1.08)
                r.layer([osc("sine", [["set", f0, s], ["exp", f0 * 1.45, s + 0.05], ["exp", f0 * 1.2, s + 0.07]],
                             s, s + 0.09)], [gain(pluck_env(level, 0.08, s))], pan=pan)
        else:
            d = rng.uniform(0.3, 0.55)
            r.layer([osc("sine", [["set", base, t], ["lin", base * 1.12, t + d]], t, t + d + 0.05,
                         lfo=[lfo("detune", "sine", rng.uniform(22, 30), 160, t, t + d + 0.05)])],
                    [gain([["set", 0.0001, t], ["lin", level * 0.8, t + 0.04], ["set", level * 0.8, t + d - 0.08],
                           ["exp", 0.0001, t + d]])], pan=pan)
        t += rng.uniform(1.6, 4.2)
    return r

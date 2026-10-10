"""A minimal offline Web Audio graph: a voice = sources summed -> processor chain (shaper / biquad / gain / pan).

Every web recipe (SFXEngine, ScorePlayer, MusicEngine stinger) is a set of such voices mixed into one destination.
Sources start / stop on sample frames ceil(t * sr) (AudioScheduledSourceNode); an oscillator's first frame carries the
sub-sample phase offset. Audio connected into an AudioParam (LFO -> gain -> frequency / detune) is added to the
param's automation value per sample (a-rate, audio.md 2.1).
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field

import numpy as np

from . import biquad, osc, shaper
from .params import Param


@dataclass
class Osc:
    wave: str
    freq: Param
    start: float
    stop: float
    detune: Param = field(default_factory=lambda: Param(0.0))
    freq_mods: list["Mod"] = field(default_factory=list)
    detune_mods: list["Mod"] = field(default_factory=list)


@dataclass
class Mod:
    """An LFO oscillator through a gain node, connected into an AudioParam."""

    source: Osc
    gain: Param


@dataclass
class BufferSource:
    data: np.ndarray
    start: float
    stop: float
    loop: bool = False


@dataclass
class Filter:
    kind: str
    freq: Param
    q: float | None = None
    gain_db: float = 0.0


@dataclass
class Shaper:
    curve: np.ndarray


@dataclass
class Gain:
    gain: Param


@dataclass
class Voice:
    sources: list
    chain: list
    pan: float | None = None  # constant-power pan for stereo renders (None = centre / mono up-mix)
    tail: float = 0.05        # seconds rendered after the last stop (filter ringing)


def frame(t: float, sr: int) -> int:
    return int(math.ceil(t * sr - 1e-9))


def _render_osc(o: Osc, f0: int, f1: int, sr: int) -> np.ndarray:
    """Oscillator output on frames [f0, f1) (zero outside its own start/stop)."""
    out = np.zeros(f1 - f0)
    s0 = max(frame(o.start, sr), f0)
    s1 = min(frame(o.stop, sr), f1)
    if s1 <= s0:
        return out
    n = s1 - s0
    f = o.freq.values(s0, n, sr)
    for m in o.freq_mods:
        f = f + _render_mod(m, s0, s1, sr)
    if o.detune.events or o.detune.default != 0.0 or o.detune_mods:
        d = o.detune.values(s0, n, sr)
        for m in o.detune_mods:
            d = d + _render_mod(m, s0, s1, sr)
        f = f * np.power(2.0, d / 1200.0)
    start_frame_exact = o.start * sr
    phase0 = 0.0
    if s0 == frame(o.start, sr):
        phase0 = (s0 - start_frame_exact) * f[0] / sr
    out[s0 - f0:s1 - f0] = osc.render(o.wave, f, sr, phase0)
    return out


def _render_mod(m: Mod, f0: int, f1: int, sr: int) -> np.ndarray:
    sig = _render_osc(m.source, f0, f1, sr)
    return sig * m.gain.values(f0, f1 - f0, sr)


def _render_buffer(b: BufferSource, f0: int, f1: int, sr: int) -> np.ndarray:
    out = np.zeros(f1 - f0)
    s0 = frame(b.start, sr)
    s1 = frame(b.stop, sr)
    if b.loop:
        total = s1 - s0
        reps = int(math.ceil(total / max(1, b.data.shape[0])))
        data = np.tile(b.data, reps)[:total]
    else:
        data = b.data[: max(0, s1 - s0)]
    a0 = max(s0, f0)
    a1 = min(s0 + data.shape[0], f1)
    if a1 > a0:
        out[a0 - f0:a1 - f0] = data[a0 - s0:a1 - s0]
    return out


def voice_span(v: Voice, sr: int) -> tuple[int, int]:
    starts = [frame(s.start, sr) for s in v.sources]
    stops = [frame(s.stop, sr) for s in v.sources]
    return min(starts), max(stops) + int(round(v.tail * sr))


def render_voice(v: Voice, sr: int, f0: int | None = None, f1: int | None = None) -> tuple[int, np.ndarray]:
    """Returns (first frame, mono signal) over [f0, f1) (default: the voice's own span)."""
    a, b = voice_span(v, sr)
    if f0 is None:
        f0 = a
    if f1 is None:
        f1 = b
    x = np.zeros(f1 - f0)
    for s in v.sources:
        if isinstance(s, Osc):
            x += _render_osc(s, f0, f1, sr)
        elif isinstance(s, BufferSource):
            x += _render_buffer(s, f0, f1, sr)
        else:
            raise TypeError(s)
    for p in v.chain:
        if isinstance(p, Gain):
            x = x * p.gain.values(f0, f1 - f0, sr)
        elif isinstance(p, Filter):
            freq = p.freq.values(f0, f1 - f0, sr) if not p.freq.is_constant else _const_value(p.freq, f0, sr)
            x = biquad.process(x, p.kind, freq, p.q, p.gain_db, sr)
        elif isinstance(p, Shaper):
            x = shaper.apply(x, p.curve)
        else:
            raise TypeError(p)
    return f0, x


def _const_value(p: Param, f0: int, sr: int) -> float:
    if not p.events:
        return p.default
    return p.events[0][1]


def count_oscillators(voices: list[Voice]) -> int:
    n = 0
    for v in voices:
        for s in v.sources:
            if isinstance(s, Osc):
                n += 1 + len(s.freq_mods) + len(s.detune_mods)
    # Shared LFOs (one LFO feeding two oscillators' detune) are counted per Mod instance; callers that share a Mod
    # object across oscillators use count_unique_oscillators.
    return n


def count_unique_oscillators(voices: list[Voice]) -> int:
    seen: set[int] = set()
    for v in voices:
        for s in v.sources:
            if isinstance(s, Osc):
                seen.add(id(s))
                for m in s.freq_mods + s.detune_mods:
                    seen.add(id(m.source))
    return len(seen)


def mix(voices: list[Voice], n_total: int, sr: int, channels: int = 1) -> np.ndarray:
    """Mix voices into a (channels, n_total) buffer (mono voices are up-mixed L = R = x, or panned if pan is set)."""
    out = np.zeros((channels, n_total))
    for v in voices:
        a, b = voice_span(v, sr)
        a = max(a, 0)
        b = min(b, n_total)
        if b <= a:
            continue
        _, x = render_voice(v, sr, a, b)
        if channels == 1:
            out[0, a:b] += x
        elif v.pan is None:
            out[:, a:b] += x
        else:
            ang = (v.pan + 1.0) * math.pi / 4.0
            out[0, a:b] += x * math.cos(ang)
            out[1, a:b] += x * math.sin(ang)
    return out

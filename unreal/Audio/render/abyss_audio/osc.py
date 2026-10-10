"""Band-limited oscillators (audio.md 2.2), modelled on Chromium's PeriodicWave.

* Fourier sine coefficients b_n: square (2/(n pi))(1 - (-1)^n), sawtooth (-1)^(n+1) 2/(n pi), triangle
  8 sin(n pi / 2) / (n pi)^2; all a_n = 0, so every wave starts at phase 0 rising.
* 4096-point tables (sample rates 24k..88.2k), 36 pitch ranges (3 per octave). Range r keeps
  2048 * 2^(-r/3) partials; the lowest fundamental is nyquist / 2048. Playback interpolates linearly inside a table and
  between the two tables bracketing the current pitch range (Chromium WaveDataForFundamentalFrequency), so partials
  never cross Nyquist. The tables are normalised by the peak of the full-band table (range 0).
* The phase accumulates the instantaneous frequency f * 2^(detune / 1200) per sample, starting at 0 on the first
  frame; negative frequencies run the phase backwards (the lightning recipe dips below 0 Hz). Band-limiting uses |f|.
* Sine is computed directly (the exact limit of its one-partial table).
"""

from __future__ import annotations

import functools
import math

import numpy as np

TABLE_SIZE = 4096
MAX_PARTIALS = TABLE_SIZE // 2
RANGES_PER_OCTAVE = 3
CENTS_PER_RANGE = 1200 / RANGES_PER_OCTAVE
NUM_RANGES = int(round(RANGES_PER_OCTAVE * math.log2(TABLE_SIZE)))  # 36

WAVES = ("sine", "square", "sawtooth", "triangle")


def fourier_b(wave: str, n: np.ndarray) -> np.ndarray:
    n = n.astype(np.float64)
    if wave == "sine":
        return np.where(n == 1, 1.0, 0.0)
    if wave == "square":
        return (2.0 / (n * np.pi)) * (1.0 - np.power(-1.0, n))
    if wave == "sawtooth":
        return np.power(-1.0, n + 1) * 2.0 / (n * np.pi)
    if wave == "triangle":
        return 8.0 * np.sin(n * np.pi / 2.0) / np.square(n * np.pi)
    raise ValueError(f"unknown oscillator type {wave!r}")


def partials_for_range(r: int) -> int:
    culling = 2.0 ** (-(r * CENTS_PER_RANGE) / 1200.0)
    return int(culling * MAX_PARTIALS)


@functools.lru_cache(maxsize=None)
def tables(wave: str) -> np.ndarray:
    """(NUM_RANGES + 1, TABLE_SIZE + 1) tables; the extra column is the wrap sample for interpolation."""
    n = np.arange(1, MAX_PARTIALS, dtype=np.int64)
    b = fourier_b(wave, n)
    out = np.zeros((NUM_RANGES, TABLE_SIZE + 1), dtype=np.float64)
    scale = None
    for r in range(NUM_RANGES):
        keep = partials_for_range(r)
        spec = np.zeros(TABLE_SIZE // 2 + 1, dtype=np.complex128)
        lim = min(keep, MAX_PARTIALS - 1)
        spec[1:lim + 1] = -1j * b[:lim] * (TABLE_SIZE / 2.0)
        tab = np.fft.irfft(spec, TABLE_SIZE)
        if scale is None:
            peak = float(np.max(np.abs(tab)))
            scale = 1.0 / peak if peak > 0 else 1.0
        out[r, :TABLE_SIZE] = tab * scale
        out[r, TABLE_SIZE] = out[r, 0]
    return out


LOWEST_FUNDAMENTAL_FACTOR = 1.0 / MAX_PARTIALS  # times nyquist


def render(wave: str, freq: np.ndarray, sr: int, phase0: float = 0.0) -> np.ndarray:
    """One oscillator over len(freq) frames; freq = instantaneous frequency (Hz, detune applied, may be negative).

    phase0 is the phase (in cycles) of the first frame (sub-sample start offset).
    """
    n = freq.shape[0]
    if n == 0:
        return np.zeros(0)
    inc = freq / sr
    phase = np.empty(n, dtype=np.float64)
    phase[0] = phase0
    if n > 1:
        np.cumsum(inc[:-1], out=phase[1:])
        phase[1:] += phase0
    if wave == "sine":
        return np.sin(2.0 * np.pi * phase)
    tab = tables(wave)
    pos = np.mod(phase, 1.0) * TABLE_SIZE
    i0 = np.floor(pos).astype(np.int64)
    i0 = np.minimum(i0, TABLE_SIZE - 1)
    frac = pos - i0
    nyquist = sr / 2.0
    lowest = nyquist * LOWEST_FUNDAMENTAL_FACTOR
    af = np.abs(freq)
    with np.errstate(divide="ignore"):
        ratio = np.where(af > 0, af / lowest, 0.5)
        cents = np.log2(ratio) * 1200.0
    pitch_range = 1.0 + cents / CENTS_PER_RANGE
    pitch_range = np.clip(pitch_range, 0.0, NUM_RANGES - 1)
    r1 = pitch_range.astype(np.int64)  # higher (more partials)
    r2 = np.minimum(r1 + 1, NUM_RANGES - 1)  # lower (fewer partials)
    tf = pitch_range - r1
    hi = tab[r1, i0] * (1.0 - frac) + tab[r1, i0 + 1] * frac
    lo = tab[r2, i0] * (1.0 - frac) + tab[r2, i0 + 1] * frac
    return (1.0 - tf) * hi + tf * lo

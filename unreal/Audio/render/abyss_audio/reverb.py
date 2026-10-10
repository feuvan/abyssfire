"""Convolver reverb of the music chain (audio.md 2.6; MusicEngine.ts generateReverbIR).

IR: 2 channels, L = ceil(sr * (decay + 0.2)) frames, per channel h[i] = (rand*2 - 1) * exp(-(i/sr) * 3/decay) *
(1 - 0.3 i/L). ConvolverNode.normalize = true: scale = 10^(-58/20) / max(sqrt(sum h^2 / (channels*L)), 0.000125) *
44100 / sr (Chromium Reverb::CalculateNormalizationScale). Mono input -> stereo (x*hL, x*hR); stereo input -> (L*hL,
R*hR).
"""

from __future__ import annotations

import math

import numpy as np
from scipy.signal import oaconvolve

GAIN_CALIBRATION = 10.0 ** (-58.0 / 20.0)
GAIN_CALIBRATION_SR = 44100.0
MIN_POWER = 0.000125


def make_ir(decay: float, sr: int, rng: np.random.Generator) -> np.ndarray:
    duration = decay + 0.2
    length = int(math.ceil(sr * duration))
    i = np.arange(length, dtype=np.float64)
    env = np.exp(-(i / sr) * (3.0 / decay)) * (1.0 - (i / length) * 0.3)
    ir = np.empty((2, length))
    for ch in range(2):
        ir[ch] = (rng.random(length) * 2.0 - 1.0) * env
    return ir


def normalization_scale(ir: np.ndarray, sr: int) -> float:
    channels, length = ir.shape
    power = math.sqrt(float(np.sum(ir * ir)) / (channels * length))
    if not math.isfinite(power) or power < MIN_POWER:
        power = MIN_POWER
    scale = (1.0 / power) * GAIN_CALIBRATION
    scale *= GAIN_CALIBRATION_SR / sr
    if channels == 4:
        scale *= 0.5
    return scale


def convolve(x: np.ndarray, ir: np.ndarray, sr: int) -> np.ndarray:
    """x: (channels, n) with 1 or 2 channels -> (2, n + len(ir) - 1)."""
    h = ir * normalization_scale(ir, sr)
    if x.shape[0] == 1:
        return np.stack([oaconvolve(x[0], h[0]), oaconvolve(x[0], h[1])])
    return np.stack([oaconvolve(x[0], h[0]), oaconvolve(x[1], h[1])])

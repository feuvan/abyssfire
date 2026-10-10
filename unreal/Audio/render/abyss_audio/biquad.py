"""BiquadFilterNode as specified by Web Audio / Chromium (audio.md 2.3).

* lowpass / highpass: Q in dB, alpha = sin w0 / (2 * 10^(Q/20)); default Q = 1.
* bandpass: Q linear, alpha = sin w0 / (2 Q), 0 dB peak.
* lowshelf: S = 1, A = 10^(G/40), alpha = sin w0 / 2 * sqrt(2).
Frequencies are clamped to [0, nyquist] like Chromium (cutoff at nyquist = pass-through; at 0 = silence for LP).
Time-varying cutoffs are recomputed every sample (Chromium recomputes per frame for sample-accurate automation).
"""

from __future__ import annotations

import math

import numpy as np
from scipy.signal import lfilter

from .jit import njit

LOWPASS, HIGHPASS, BANDPASS, LOWSHELF = 0, 1, 2, 3
TYPES = {"lowpass": LOWPASS, "highpass": HIGHPASS, "bandpass": BANDPASS, "lowshelf": LOWSHELF}
DEFAULT_FREQ = 350.0
DEFAULT_Q = 1.0


@njit
def _coeffs(kind, f, q, gain_db, sr):
    """Normalised (b0, b1, b2, a1, a2)."""
    nyq = sr * 0.5
    cutoff = f / nyq
    if cutoff < 0.0:
        cutoff = 0.0
    if cutoff > 1.0:
        cutoff = 1.0
    if kind == 0 or kind == 1:  # lowpass / highpass
        if cutoff == 1.0:
            if kind == 0:
                return 1.0, 0.0, 0.0, 0.0, 0.0
            return 0.0, 0.0, 0.0, 0.0, 0.0
        if cutoff <= 0.0:
            if kind == 0:
                return 0.0, 0.0, 0.0, 0.0, 0.0
            return 1.0, 0.0, 0.0, 0.0, 0.0
        g = math.pow(10.0, -0.05 * q)
        w0 = math.pi * cutoff
        cw = math.cos(w0)
        alpha = 0.5 * math.sin(w0) * g
        a0 = 1.0 + alpha
        if kind == 0:
            b1 = 1.0 - cw
            b0 = 0.5 * b1
            b2 = b0
        else:
            b1 = -(1.0 + cw)
            b0 = 0.5 * (1.0 + cw)
            b2 = b0
        return b0 / a0, b1 / a0, b2 / a0, (-2.0 * cw) / a0, (1.0 - alpha) / a0
    if kind == 2:  # bandpass
        if cutoff <= 0.0 or cutoff >= 1.0:
            return 0.0, 0.0, 0.0, 0.0, 0.0
        if q <= 0.0:
            return 1.0, 0.0, 0.0, 0.0, 0.0
        w0 = math.pi * cutoff
        cw = math.cos(w0)
        alpha = math.sin(w0) / (2.0 * q)
        a0 = 1.0 + alpha
        return alpha / a0, 0.0, -alpha / a0, (-2.0 * cw) / a0, (1.0 - alpha) / a0
    # lowshelf
    A = math.pow(10.0, gain_db / 40.0)
    if cutoff >= 1.0:
        return A * A, 0.0, 0.0, 0.0, 0.0
    if cutoff <= 0.0:
        return 1.0, 0.0, 0.0, 0.0, 0.0
    w0 = math.pi * cutoff
    cw = math.cos(w0)
    alpha = 0.5 * math.sin(w0) * math.sqrt(2.0)
    k = math.cos(w0)
    k2 = 2.0 * math.sqrt(A) * alpha
    a_plus_one = A + 1.0
    a_minus_one = A - 1.0
    b0 = A * (a_plus_one - a_minus_one * k + k2)
    b1 = 2.0 * A * (a_minus_one - a_plus_one * k)
    b2 = A * (a_plus_one - a_minus_one * k - k2)
    a0 = a_plus_one + a_minus_one * k + k2
    a1 = -2.0 * (a_minus_one + a_plus_one * k)
    a2 = a_plus_one + a_minus_one * k - k2
    _ = cw
    return b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0


@njit
def _process_varying(x, kind, freqs, q, gain_db, sr):
    y = np.empty_like(x)
    x1 = 0.0
    x2 = 0.0
    y1 = 0.0
    y2 = 0.0
    last_f = -1.0
    b0 = b1 = b2 = a1 = a2 = 0.0
    for i in range(x.shape[0]):
        f = freqs[i]
        if f != last_f:
            b0, b1, b2, a1, a2 = _coeffs(kind, f, q, gain_db, sr)
            last_f = f
        xi = x[i]
        yi = b0 * xi + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2 = x1
        x1 = xi
        y2 = y1
        y1 = yi
        y[i] = yi
    return y


def coefficients(kind: str, f: float, q: float | None = None, gain_db: float = 0.0, sr: int = 48000):
    k = TYPES[kind]
    qq = DEFAULT_Q if q is None else q
    return _coeffs(k, float(f), float(qq), float(gain_db), float(sr))


def process(x: np.ndarray, kind: str, freq, q: float | None, gain_db: float, sr: int) -> np.ndarray:
    """Filter x (1-D or (channels, n)). freq: float or per-sample array (same length as x's last axis)."""
    k = TYPES[kind]
    qq = DEFAULT_Q if q is None else float(q)
    if x.ndim == 2:
        return np.stack([process(ch, kind, freq, q, gain_db, sr) for ch in x])
    if np.ndim(freq) == 0 or np.all(freq == freq[0]):
        f = float(freq if np.ndim(freq) == 0 else freq[0])
        b0, b1, b2, a1, a2 = _coeffs(k, f, qq, float(gain_db), float(sr))
        return lfilter([b0, b1, b2], [1.0, a1, a2], x)
    return _process_varying(np.ascontiguousarray(x, dtype=np.float64), k,
                            np.ascontiguousarray(freq, dtype=np.float64), qq, float(gain_db), float(sr))


def magnitude_db(kind: str, f: float, q: float | None, gain_db: float, sr: int, at_hz: float) -> float:
    b0, b1, b2, a1, a2 = coefficients(kind, f, q, gain_db, sr)
    w = 2 * math.pi * at_hz / sr
    z = complex(math.cos(w), math.sin(w))
    zi = 1 / z
    h = (b0 + b1 * zi + b2 * zi * zi) / (1 + a1 * zi + a2 * zi * zi)
    return 20 * math.log10(max(abs(h), 1e-30))

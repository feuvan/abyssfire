"""WaveShaperNode with a 256-point curve, oversample 'none' (audio.md 2.4)."""

from __future__ import annotations

import math

import numpy as np

CURVE_SIZE = 256

CURVES = {
    "tanh2": lambda x: math.tanh(x * 2),
    "tanh3": lambda x: math.tanh(x * 3),
    "tanh4": lambda x: math.tanh(x * 4),
    "sqrt": lambda x: math.copysign(math.sqrt(abs(x)), x) if x != 0 else 0.0,
}


def make_curve(name: str) -> np.ndarray:
    fn = CURVES[name]
    # The web fills a Float32Array: curve[i] = F(i/128 - 1).
    return np.array([fn(i / 128.0 - 1.0) for i in range(CURVE_SIZE)], dtype=np.float32).astype(np.float64)


def apply(x: np.ndarray, curve: np.ndarray) -> np.ndarray:
    n = curve.shape[0]
    v = (n - 1) * 0.5 * (x + 1.0)
    out = np.empty_like(x)
    lo = v <= 0.0
    hi = v >= n - 1
    mid = ~(lo | hi)
    out[lo] = curve[0]
    out[hi] = curve[n - 1]
    vm = v[mid]
    k = np.floor(vm).astype(np.int64)
    f = vm - k
    out[mid] = (1.0 - f) * curve[k] + f * curve[k + 1]
    return out

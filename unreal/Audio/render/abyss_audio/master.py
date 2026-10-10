"""Mastering: ITU-R BS.1770-4 loudness, true peak, a transparent look-ahead peak limiter, OGG Vorbis export (A1, A3)."""

from __future__ import annotations

import math

import numpy as np
import soundfile as sf
from scipy.ndimage import minimum_filter1d, uniform_filter1d
from scipy.signal import lfilter, resample_poly

from .jit import njit

# ---- BS.1770 ----


def _k_weighting(sr: int):
    # Pre-filter (high shelf) and RLB high-pass, parameterised for any rate (identical to the 48 kHz table of the
    # standard: shelf b = [1.53512485958697, -2.69169618940638, 1.19839281085285], a = [1, -1.69065929318241,
    # 0.73248077421585]; RLB a = [1, -1.99004745483398, 0.99007225036621]).
    f0 = 1681.974450955533
    g = 3.999843853973347
    q = 0.7071752369554196
    k = math.tan(math.pi * f0 / sr)
    vh = 10.0 ** (g / 20.0)
    vb = vh ** 0.4996667741545416
    a0 = 1.0 + k / q + k * k
    b_shelf = [(vh + vb * k / q + k * k) / a0, 2.0 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0]
    a_shelf = [1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0]
    f0 = 38.13547087602444
    q = 0.5003270373238773
    k = math.tan(math.pi * f0 / sr)
    d = 1.0 + k / q + k * k
    b_hp = [1.0, -2.0, 1.0]
    a_hp = [1.0, 2.0 * (k * k - 1.0) / d, (1.0 - k / q + k * k) / d]
    return (b_shelf, a_shelf), (b_hp, a_hp)


def _weighted_power(x: np.ndarray, sr: int, block_sec: float, step_sec: float) -> np.ndarray:
    if x.ndim == 1:
        x = x[None, :]
    (bs, as_), (bh, ah) = _k_weighting(sr)
    y = lfilter(bh, ah, lfilter(bs, as_, x, axis=1), axis=1)
    block = int(round(block_sec * sr))
    step = int(round(step_sec * sr))
    n = y.shape[1]
    if n < block:
        y = np.concatenate([y, np.zeros((y.shape[0], block - n))], axis=1)
        n = block
    sq = np.cumsum(np.concatenate([np.zeros((y.shape[0], 1)), y * y], axis=1), axis=1)
    starts = np.arange(0, n - block + 1, step)
    z = (sq[:, starts + block] - sq[:, starts]) / block  # (channels, blocks)
    return np.sum(z, axis=0)  # channel weights 1.0 (L, R / mono)


def integrated_lufs(x: np.ndarray, sr: int) -> float:
    p = _weighted_power(x, sr, 0.4, 0.1)
    with np.errstate(divide="ignore"):
        lk = -0.691 + 10.0 * np.log10(np.maximum(p, 1e-30))
    gated = p[lk > -70.0]
    if gated.size == 0:
        return -math.inf
    rel = -0.691 + 10.0 * math.log10(float(np.mean(gated))) - 10.0
    gated2 = p[(lk > -70.0) & (lk > rel)]
    if gated2.size == 0:
        return -math.inf
    return -0.691 + 10.0 * math.log10(float(np.mean(gated2)))


def short_term_max_lufs(x: np.ndarray, sr: int) -> float:
    p = _weighted_power(x, sr, 3.0, 0.1)
    return float(-0.691 + 10.0 * np.log10(max(float(np.max(p)), 1e-30)))


def momentary_max_lufs(x: np.ndarray, sr: int) -> float:
    p = _weighted_power(x, sr, 0.4, 0.1)
    return float(-0.691 + 10.0 * np.log10(max(float(np.max(p)), 1e-30)))


def true_peak(x: np.ndarray) -> float:
    """4x oversampled peak (linear)."""
    if x.ndim == 1:
        x = x[None, :]
    up = resample_poly(x, 4, 1, axis=1)
    return float(max(np.max(np.abs(up)), np.max(np.abs(x))))


def sample_peak(x: np.ndarray) -> float:
    return float(np.max(np.abs(x)))


def db(v: float) -> float:
    return 20.0 * math.log10(v) if v > 0 else -math.inf


def undb(d: float) -> float:
    return 10.0 ** (d / 20.0)


# ---- limiter ----


@njit
def _release(g, rel_coef):
    out = np.empty_like(g)
    cur = 1.0
    for i in range(g.shape[0]):
        t = g[i]
        if t < cur:
            cur = t
        else:
            cur = cur + (t - cur) * rel_coef
        out[i] = cur
    return out


def limit(x: np.ndarray, sr: int, ceiling_dbtp: float = -1.0, lookahead_ms: float = 1.5,
          release_ms: float = 60.0) -> tuple[np.ndarray, float]:
    """Peak limiter: gain = min(1, ceiling / |x|) min-filtered over +-lookahead, averaged over the lookahead (never
    exceeds the target), release-smoothed. Iterates the sample ceiling down until the 4x true peak is under
    `ceiling_dbtp`. Returns (y, max gain reduction dB)."""
    if x.ndim == 1:
        x = x[None, :]
    target_tp = undb(ceiling_dbtp)
    if true_peak(x) <= target_tp:
        return x, 0.0
    la = max(1, int(round(lookahead_ms * 1e-3 * sr)))
    rel = 1.0 - math.exp(-1.0 / (release_ms * 1e-3 * sr))
    # Per-frame true-peak detector: the 4x oversampled magnitude around each frame (and its neighbours).
    n = x.shape[1]
    up = np.max(np.abs(resample_poly(x, 4, 1, axis=1)), axis=0)[: n * 4]
    if up.shape[0] < n * 4:
        up = np.concatenate([up, np.zeros(n * 4 - up.shape[0])])
    peak = np.maximum(up.reshape(n, 4).max(axis=1), np.max(np.abs(x), axis=0))
    peak = np.maximum(peak, np.concatenate([peak[1:], peak[-1:]]))
    ceiling = target_tp
    y = x
    gr = 0.0
    for _ in range(12):
        with np.errstate(divide="ignore"):
            g = np.minimum(1.0, ceiling / np.maximum(peak, 1e-12))
        g = minimum_filter1d(g, size=2 * la + 1, mode="nearest")
        g = uniform_filter1d(g, size=la, mode="nearest")
        g = _release(np.ascontiguousarray(g), rel)
        y = x * g
        gr = -db(float(np.min(g)))
        if true_peak(y) <= target_tp:
            break
        ceiling *= undb(-0.2)
    return y, gr


def limit_loop(x: np.ndarray, sr: int, ceiling_dbtp: float = -1.0) -> tuple[np.ndarray, float]:
    """Circular limiting of a loop: process [x, x, x] and keep the middle copy."""
    n = x.shape[1]
    y, gr = limit(np.concatenate([x, x, x], axis=1), sr, ceiling_dbtp)
    return y[:, n:2 * n], gr


# ---- export ----

VORBIS_QUALITY = 0.6  # q6 (A1)


def write_ogg(path: str, x: np.ndarray, sr: int, quality: float = VORBIS_QUALITY) -> None:
    """libsndfile maps SFC_SET_COMPRESSION_LEVEL c to vorbis quality 1 - c (ogg_vorbis.c), so q6 = level 0.4."""
    if x.ndim == 1:
        x = x[None, :]
    data = np.ascontiguousarray(np.clip(x.T, -1.0, 1.0).astype(np.float32))
    # Written in blocks: libsndfile's Vorbis writer overflows the stack when handed minutes of audio in one call.
    with sf.SoundFile(path, "w", samplerate=sr, channels=data.shape[1], format="OGG", subtype="VORBIS",
                      compression_level=1.0 - quality) as f:
        block = 1 << 15
        for i in range(0, data.shape[0], block):
            f.write(data[i:i + block])


def read_audio(path: str) -> tuple[np.ndarray, int]:
    y, sr = sf.read(path, dtype="float64", always_2d=True)
    return y.T, sr

"""DynamicsCompressorNode: a transliteration of Chromium's DynamicsCompressorKernel (audio.md 2.7).

Chromium (third_party/blink/renderer/platform/audio/dynamics_compressor_kernel.cc) is derived from WebKit code under
the BSD licence:

    Copyright (C) 2011 Google Inc. All rights reserved.
    Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
    following conditions are met: 1. Redistributions of source code must retain the above copyright notice, this list
    of conditions and the following disclaimer. 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided
    with the distribution. 3. Neither the name of Apple Computer, Inc. ("Apple") nor the names of its contributors may
    be used to endorse or promote products derived from this software without specific prior written permission.
    THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
    BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
    STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
    EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Node constants (DynamicsCompressor): pre-delay 6 ms, release zones 0.09 / 0.16 / 0.42 / 0.98, post gain 0 dB, effect
blend 1. Detector = max |channel|, 32-frame divisions, adaptive release polynomial, automatic make-up gain
(1 / curve(1))^0.6. Arithmetic is float64 here (Chromium uses float32; the difference is far below audibility).
"""

from __future__ import annotations

import math

import numpy as np

from .jit import njit

PRE_DELAY_SEC = 0.006
RELEASE_ZONES = (0.09, 0.16, 0.42, 0.98)
MAX_PRE_DELAY_FRAMES = 1024
DIVISION_FRAMES = 32


@njit
def _db_to_lin(db):
    return math.pow(10.0, 0.05 * db)


@njit
def _lin_to_db(x):
    if x <= 0.0:
        return -1000.0
    return 20.0 * math.log10(x)


@njit
def _knee_curve(x, k, linear_threshold):
    if x < linear_threshold:
        return x
    return linear_threshold + (1.0 - math.exp(-k * (x - linear_threshold))) / k


@njit
def _saturate(x, k, linear_threshold, knee_threshold, db_knee_threshold, db_yknee_threshold, slope):
    if x < knee_threshold:
        return _knee_curve(x, k, linear_threshold)
    x_db = _lin_to_db(x)
    y_db = db_yknee_threshold + slope * (x_db - db_knee_threshold)
    return _db_to_lin(y_db)


@njit
def _slope_at(x, k, linear_threshold):
    if x < linear_threshold:
        return 1.0
    x2 = x * 1.001
    x_db = _lin_to_db(x)
    x2_db = _lin_to_db(x2)
    y_db = _lin_to_db(_knee_curve(x, k, linear_threshold))
    y2_db = _lin_to_db(_knee_curve(x2, k, linear_threshold))
    return (y2_db - y_db) / (x2_db - x_db)


@njit
def _k_at_slope(desired_slope, db_threshold, db_knee, linear_threshold):
    x_db = db_threshold + db_knee
    x = _db_to_lin(x_db)
    min_k = 0.1
    max_k = 10000.0
    k = 5.0
    for _ in range(15):
        slope = _slope_at(x, k, linear_threshold)
        if slope < desired_slope:
            max_k = k
        else:
            min_k = k
        k = math.sqrt(min_k * max_k)
    return k


def static_curve(db_threshold: float, db_knee: float, ratio: float):
    """(k, linear_threshold, knee_threshold, db_knee_threshold, db_yknee_threshold, slope, makeup_gain)."""
    linear_threshold = _db_to_lin(db_threshold)
    slope = 1.0 / ratio
    k = _k_at_slope(slope, db_threshold, db_knee, linear_threshold)
    db_knee_threshold = db_threshold + db_knee
    knee_threshold = _db_to_lin(db_knee_threshold)
    db_yknee_threshold = _lin_to_db(_knee_curve(knee_threshold, k, linear_threshold))
    full_range_gain = _saturate(1.0, k, linear_threshold, knee_threshold, db_knee_threshold, db_yknee_threshold, slope)
    makeup = math.pow(1.0 / full_range_gain, 0.6)
    return k, linear_threshold, knee_threshold, db_knee_threshold, db_yknee_threshold, slope, makeup


@njit
def _process(src, k, linear_threshold, knee_threshold, db_knee_threshold, db_yknee_threshold, slope,
             master_linear_gain, attack_time, release_time, sr, pre_delay_frames, z1, z2, z3, z4, state):
    channels = src.shape[0]
    n = src.shape[1]
    dst = np.zeros_like(src)
    mask = MAX_PRE_DELAY_FRAMES - 1
    delay = np.zeros((channels, MAX_PRE_DELAY_FRAMES))
    # state: [detector_average, compressor_gain, max_attack_compression_diff_db]
    detector_average_m = state[0]
    compressor_gain_m = state[1]
    max_attack_diff_db = state[2]
    read_index = 0
    write_index = pre_delay_frames

    attack_time = max(0.001, attack_time)
    attack_frames = attack_time * sr
    release_frames = sr * release_time
    sat_release_frames = 0.0025 * sr

    y1 = release_frames * z1
    y2 = release_frames * z2
    y3 = release_frames * z3
    y4 = release_frames * z4
    kA = 0.9999999999999998 * y1 + 1.8432219684323923e-16 * y2 - 1.9373394351676423e-16 * y3 + 8.824516011816245e-18 * y4
    kB = -1.5788320352845888 * y1 + 2.3305837032074286 * y2 - 0.9141194204840429 * y3 + 0.1623677525612032 * y4
    kC = 0.5334142869106424 * y1 - 1.272736789213631 * y2 + 0.9258856042207512 * y3 - 0.18656310191776226 * y4
    kD = 0.08783463138207234 * y1 - 0.1694162967925622 * y2 + 0.08588057951595272 * y3 - 0.00429891410546283 * y4
    kE = -0.042416883008123074 * y1 + 0.1115693827987602 * y2 - 0.09764676325265872 * y3 + 0.028494263462021576 * y4

    pi_over_two = math.pi / 2.0
    n_divisions = n // DIVISION_FRAMES
    frame_index = 0
    for _div in range(n_divisions):
        if math.isnan(detector_average_m) or math.isinf(detector_average_m):
            detector_average_m = 1.0
        desired_gain = detector_average_m
        scaled_desired_gain = math.asin(min(1.0, max(-1.0, desired_gain))) / pi_over_two

        is_releasing = scaled_desired_gain > compressor_gain_m
        if scaled_desired_gain > 0.0:
            compression_diff_db = _lin_to_db(compressor_gain_m / scaled_desired_gain)
        else:
            compression_diff_db = math.inf
        if is_releasing:
            max_attack_diff_db = -1.0
            if math.isnan(compression_diff_db) or math.isinf(compression_diff_db):
                compression_diff_db = -1.0
            x = compression_diff_db
            if x < -12.0:
                x = -12.0
            if x > 0.0:
                x = 0.0
            x = 0.25 * (x + 12.0)
            x2 = x * x
            x3 = x2 * x
            x4 = x2 * x2
            rel_frames = kA + kB * x + kC * x2 + kD * x3 + kE * x4
            db_per_frame = 5.0 / rel_frames
            envelope_rate = _db_to_lin(db_per_frame)
        else:
            if math.isnan(compression_diff_db) or math.isinf(compression_diff_db):
                compression_diff_db = 1.0
            if max_attack_diff_db == -1.0 or max_attack_diff_db < compression_diff_db:
                max_attack_diff_db = compression_diff_db
            eff_atten_diff_db = max(0.5, max_attack_diff_db)
            xx = 0.25 / eff_atten_diff_db
            envelope_rate = 1.0 - math.pow(xx, 1.0 / attack_frames)

        detector_average = detector_average_m
        compressor_gain = compressor_gain_m
        for _f in range(DIVISION_FRAMES):
            compressor_input = 0.0
            for c in range(channels):
                v = src[c, frame_index]
                delay[c, write_index] = v
                av = v if v > 0 else -v
                if compressor_input < av:
                    compressor_input = av
            abs_input = compressor_input
            shaped = _saturate(abs_input, k, linear_threshold, knee_threshold, db_knee_threshold,
                               db_yknee_threshold, slope)
            attenuation = 1.0 if abs_input <= 0.0001 else shaped / abs_input
            attenuation_db = -_lin_to_db(attenuation)
            if attenuation_db < 2.0:
                attenuation_db = 2.0
            dbpf = attenuation_db / sat_release_frames
            sat_release_rate = _db_to_lin(dbpf) - 1.0
            is_release = attenuation > detector_average
            rate = sat_release_rate if is_release else 1.0
            detector_average += (attenuation - detector_average) * rate
            if detector_average > 1.0:
                detector_average = 1.0
            if math.isnan(detector_average) or math.isinf(detector_average):
                detector_average = 1.0
            if envelope_rate < 1.0:
                compressor_gain += (scaled_desired_gain - compressor_gain) * envelope_rate
            else:
                compressor_gain *= envelope_rate
                if compressor_gain > 1.0:
                    compressor_gain = 1.0
            post_warp = math.sin(pi_over_two * compressor_gain)
            total_gain = master_linear_gain * post_warp  # effect blend 1: dry 0, wet 1
            for c in range(channels):
                dst[c, frame_index] = delay[c, read_index] * total_gain
            frame_index += 1
            read_index = (read_index + 1) & mask
            write_index = (write_index + 1) & mask
        detector_average_m = detector_average
        compressor_gain_m = compressor_gain
    state[0] = detector_average_m
    state[1] = compressor_gain_m
    state[2] = max_attack_diff_db
    return dst


def compress(x: np.ndarray, sr: int, threshold: float, knee: float, ratio: float, attack: float, release: float,
             post_gain_db: float = 0.0) -> np.ndarray:
    """Process x ((channels, n)) like a fresh DynamicsCompressorNode. Output is delayed by the 6 ms pre-delay; frames
    beyond the last full 32-frame division are zero (callers pad)."""
    k, lt, kt, dbkt, dbykt, slope, makeup = static_curve(threshold, knee, ratio)
    master = _db_to_lin(post_gain_db) * makeup
    pre_delay_frames = min(int(PRE_DELAY_SEC * sr), MAX_PRE_DELAY_FRAMES - 1)
    state = np.array([0.0, 1.0, -1.0])  # Reset(): detector_average 0, compressor_gain 1, max diff -1
    src = np.ascontiguousarray(x, dtype=np.float64)
    if src.ndim == 1:
        src = src[None, :]
    z1, z2, z3, z4 = RELEASE_ZONES
    return _process(src, k, lt, kt, dbkt, dbykt, slope, master, attack, release, float(sr), pre_delay_frames,
                    z1, z2, z3, z4, state)


def latency_frames(sr: int) -> int:
    return int(PRE_DELAY_SEC * sr)


def static_io_db(in_db: float, threshold: float, knee: float, ratio: float) -> float:
    """Static input -> output level (dB) including the make-up gain (for tests / the spec's table in 2.7)."""
    k, lt, kt, dbkt, dbykt, slope, makeup = static_curve(threshold, knee, ratio)
    x = _db_to_lin(in_db)
    y = _saturate(x, k, lt, kt, dbkt, dbykt, slope)
    return _lin_to_db(y * makeup)

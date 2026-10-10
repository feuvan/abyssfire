"""The per-theme music effects chain (audio.md 6.2; MusicEngine.ts buildEffectsChain / buildEffectsConfig).

input -> highpass 75 Hz (Q 0.7 dB) -> lowshelf 180 Hz (-4 dB) -> { dry x (1 - mix) ; delay(preDelay) -> convolver(IR)
x mix } -> compressor (-18 dB, knee 6, ratio 4, attack 3 ms, release 250 ms) -> limiter (-1 dB, knee 0, ratio 20,
attack 1 ms, release 100 ms). Both dynamics stages add Chromium's 6 ms look-ahead; the renders compensate it.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from . import biquad, dynamics, reverb

MOOD_REVERB = {
    "pastoral": (0.22, 1.2, 0.015),
    "mysterious": (0.35, 2.0, 0.025),
    "epic": (0.30, 1.8, 0.020),
    "exotic": (0.25, 1.4, 0.018),
    "dark": (0.38, 2.5, 0.030),
}


@dataclass(frozen=True)
class ChainConfig:
    mix: float
    decay: float
    pre_delay: float
    comp_threshold: float = -18.0
    comp_knee: float = 6.0
    comp_ratio: float = 4.0
    comp_attack: float = 0.003
    comp_release: float = 0.25
    lim_threshold: float = -1.0
    lim_release: float = 0.1

    def to_json(self) -> dict:
        return {
            "reverbMix": self.mix, "reverbDecay": self.decay, "reverbPreDelay": self.pre_delay,
            "compressor": [self.comp_threshold, self.comp_knee, self.comp_ratio, self.comp_attack, self.comp_release],
            "limiter": [self.lim_threshold, 0.0, 20.0, 0.001, self.lim_release],
        }


def config_for_theme(theme: dict) -> ChainConfig:
    mix, decay, pre = MOOD_REVERB[theme["mood"]]
    return ChainConfig(
        mix=float(theme.get("reverbMix", mix)),
        decay=float(theme.get("reverbDecay", decay)),
        pre_delay=float(theme.get("reverbPreDelay", pre)),
        comp_threshold=float(theme.get("compressorThreshold", -18.0)),
        comp_knee=float(theme.get("compressorKnee", 6.0)),
        comp_ratio=float(theme.get("compressorRatio", 4.0)),
        comp_attack=float(theme.get("compressorAttack", 0.003)),
        comp_release=float(theme.get("compressorRelease", 0.25)),
    )


def latency_frames(sr: int) -> int:
    return 2 * dynamics.latency_frames(sr)


def process(x: np.ndarray, cfg: ChainConfig, sr: int, rng: np.random.Generator) -> np.ndarray:
    """x: (1 or 2, n). Returns (2, n) — the chain output over the input's frames (reverb tail beyond n is cut; pad the
    input with silence to keep it). The output is delayed by latency_frames(sr)."""
    if x.ndim == 1:
        x = x[None, :]
    n = x.shape[1]
    y = biquad.process(x, "highpass", 75.0, 0.7, 0.0, sr)
    y = biquad.process(y, "lowshelf", 180.0, None, -4.0, sr)
    dry = y * (1.0 - cfg.mix)
    pre = int(round(cfg.pre_delay * sr))
    delayed = np.zeros_like(y)
    if pre < n:
        delayed[:, pre:] = y[:, : n - pre]
    ir = reverb.make_ir(cfg.decay, sr, rng)
    wet = reverb.convolve(delayed, ir, sr)[:, :n] * cfg.mix
    if dry.shape[0] == 1:
        dry = np.repeat(dry, 2, axis=0)  # mono dry + stereo wet sum: speakers up-mix L = R = x
    s = dry + wet
    # Pad so the last partial 32-frame division is processed too.
    pad = (-n) % dynamics.DIVISION_FRAMES
    if pad:
        s = np.concatenate([s, np.zeros((2, pad))], axis=1)
    s = dynamics.compress(s, sr, cfg.comp_threshold, cfg.comp_knee, cfg.comp_ratio, cfg.comp_attack, cfg.comp_release)
    s = dynamics.compress(s, sr, cfg.lim_threshold, 0.0, 20.0, 0.001, cfg.lim_release)
    return s[:, :n]


def process_loop(loop: np.ndarray, cfg: ChainConfig, sr: int, rng: np.random.Generator) -> np.ndarray:
    """Circular processing (audio.md 9.4): the chain runs on [loop, loop, loop]; the middle copy (latency
    compensated) has the reverb tail and the dynamics state of a loop that has been playing."""
    if loop.ndim == 1:
        loop = loop[None, :]
    n = loop.shape[1]
    lat = latency_frames(sr)
    y = process(np.concatenate([loop, loop, loop], axis=1), cfg, sr, rng)
    return y[:, n + lat: 2 * n + lat]


def process_oneshot(x: np.ndarray, cfg: ChainConfig, sr: int, rng: np.random.Generator, preroll_sec: float = 0.5,
                    tail_sec: float | None = None) -> np.ndarray:
    """One-shot (victory, stingers): silence pre-roll lets the dynamics settle (the web chain exists before the music
    starts), the reverb tail is kept, the look-ahead latency is removed."""
    if x.ndim == 1:
        x = x[None, :]
    if tail_sec is None:
        tail_sec = cfg.decay + 0.3
    pre = int(round(preroll_sec * sr))
    tail = int(round(tail_sec * sr))
    lat = latency_frames(sr)
    padded = np.concatenate([np.zeros((x.shape[0], pre)), x, np.zeros((x.shape[0], tail + lat))], axis=1)
    y = process(padded, cfg, sr, rng)
    return y[:, pre + lat: pre + lat + x.shape[1] + tail]

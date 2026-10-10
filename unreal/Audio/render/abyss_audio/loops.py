"""Loop construction (audio.md 9.4).

* Procedural scores: render N bars dry (N a multiple of the macro-loop: 32 for explore, 16 for combat) plus the release
  tails, overlap-add the tails onto the start (the tail of bar N-1 sounds over bar 0 of the next pass), then the effects
  chain runs circularly (chain.process_loop). Loop points = the whole file.
* Beds (whisper stinger, ambience): render length + overlap, equal-power crossfade the overlap into the start.
"""

from __future__ import annotations

import math

import numpy as np

from . import chain, graph
from .score import ScorePlayer, ScoreSpec

SCORE_TRIM = 0.4


def render_score_dry(spec: ScoreSpec, state: str, bars: int, seed: int, sr: int,
                     trim: float = SCORE_TRIM) -> tuple[np.ndarray, int, ScorePlayer]:
    """Dry mono score with tails: returns ((1, n_with_tail), loop_frames, player)."""
    p = ScorePlayer(spec, state, 0.0, seed, sr)
    p.schedule_bars(bars)
    loop_frames = int(round(bars * spec.beats_per_bar * p.beat_sec * sr))
    end = max(graph.voice_span(v, sr)[1] for v in p.voices)
    total = max(end, loop_frames)
    dry = graph.mix(p.voices, total, sr, channels=1) * trim
    return dry, loop_frames, p


def wrap_tail(x: np.ndarray, loop_frames: int) -> np.ndarray:
    """Overlap-add everything after loop_frames onto the start (repeatedly, for tails longer than the loop)."""
    out = x[:, :loop_frames].copy()
    rest = x[:, loop_frames:]
    while rest.shape[1] > 0:
        k = min(loop_frames, rest.shape[1])
        out[:, :k] += rest[:, :k]
        rest = rest[:, k:]
    return out


def render_score_loop(spec: ScoreSpec, state: str, bars: int, seed: int, sr: int, cfg: chain.ChainConfig,
                      rng: np.random.Generator) -> tuple[np.ndarray, ScorePlayer]:
    dry, loop_frames, player = render_score_dry(spec, state, bars, seed, sr)
    loop = wrap_tail(dry, loop_frames)
    return chain.process_loop(loop, cfg, sr, rng), player


def crossfade_loop(x: np.ndarray, overlap_frames: int) -> np.ndarray:
    """x rendered with `overlap_frames` extra at the end: the extra fades (cos) into the start (sin), equal power."""
    n = x.shape[1] - overlap_frames
    t = (np.arange(overlap_frames) + 0.5) / overlap_frames
    fade_in = np.sin(t * math.pi / 2)
    fade_out = np.cos(t * math.pi / 2)
    out = x[:, :n].copy()
    out[:, :overlap_frames] = x[:, :overlap_frames] * fade_in + x[:, n:] * fade_out
    return out


def seam_ratio(x: np.ndarray) -> float:
    """|x[0] - x[-1]| over the 99th percentile of |x[n+1] - x[n]| (<= 1 means the wrap is as smooth as the music)."""
    d = np.abs(np.diff(x, axis=1)).max(axis=0)
    p99 = float(np.percentile(d, 99)) or 1e-12
    return float(np.max(np.abs(x[:, 0] - x[:, -1]))) / p99

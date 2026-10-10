"""AudioParam automation timelines (audio.md 2.1).

Events: setValueAtTime ("set"), linearRampToValueAtTime ("lin"), exponentialRampToValueAtTime ("exp"). A ramp runs from
the previous event's (time, value) to its own (time, value); after the last event the value holds; before the first
event the param has its default. Values are computed at sample-frame times n / sr (a-rate).
"""

from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np

SET, LIN, EXP = "set", "lin", "exp"


@dataclass
class Param:
    default: float
    events: list[tuple[str, float, float]] = field(default_factory=list)  # (kind, value, time) in insertion order

    # ---- Web Audio API ----
    def set(self, value: float, time: float) -> "Param":
        self._insert((SET, float(value), float(time)))
        return self

    def lin(self, value: float, time: float) -> "Param":
        self._insert((LIN, float(value), float(time)))
        return self

    def exp(self, value: float, time: float) -> "Param":
        self._insert((EXP, float(value), float(time)))
        return self

    def _insert(self, ev: tuple[str, float, float]) -> None:
        # The spec keeps events sorted by time; equal times keep insertion order.
        idx = len(self.events)
        while idx > 0 and self.events[idx - 1][2] > ev[2]:
            idx -= 1
        self.events.insert(idx, ev)

    @staticmethod
    def const(value: float) -> "Param":
        return Param(float(value))

    @staticmethod
    def from_spec(spec, default: float) -> "Param":
        """A number (constant) or a list of [kind, value, time] events."""
        if spec is None:
            return Param(default)
        if isinstance(spec, (int, float)):
            return Param(float(spec))
        p = Param(default)
        for kind, value, time in spec:
            if kind not in (SET, LIN, EXP):
                raise ValueError(f"unknown automation kind {kind!r}")
            p._insert((kind, float(value), float(time)))
        return p

    def to_spec(self):
        if not self.events:
            return self.default
        return [[k, v, t] for (k, v, t) in self.events]

    @property
    def is_constant(self) -> bool:
        return not self.events or (len(self.events) == 1 and self.events[0][0] == SET)

    def last_time(self) -> float:
        return self.events[-1][2] if self.events else 0.0

    def shifted(self, dt: float) -> "Param":
        return Param(self.default, [(k, v, t + dt) for (k, v, t) in self.events])

    def scaled(self, k: float) -> "Param":
        return Param(self.default * k, [(kind, v * k, t) for (kind, v, t) in self.events])

    # ---- evaluation ----
    def values(self, n0: int, n: int, sr: int) -> np.ndarray:
        """Values at frames n0 .. n0 + n - 1 (time = frame / sr)."""
        t = (n0 + np.arange(n, dtype=np.float64)) / sr
        return self.values_at(t)

    def values_at(self, t: np.ndarray) -> np.ndarray:
        out = np.full(t.shape, self.default, dtype=np.float64)
        ev = self.events
        if not ev:
            return out
        if len(ev) == 1 and ev[0][0] == SET:
            # Common case: a constant set at the voice start (the default never sounds before it, 2.1).
            out[t >= ev[0][2]] = ev[0][1]
            return out
        prev_t, prev_v = None, self.default
        for i, (kind, v, time) in enumerate(ev):
            if kind == SET:
                seg_end = time
                if prev_t is not None:
                    _hold(out, t, prev_t, seg_end, prev_v)
                prev_t, prev_v = time, v
                continue
            # Ramp from (prev_t, prev_v) to (time, v). A ramp with no previous event starts at time 0 from the default.
            t0 = prev_t if prev_t is not None else 0.0
            v0 = prev_v
            mask = (t >= t0) & (t < time)
            if np.any(mask):
                if time <= t0:
                    pass
                elif kind == LIN:
                    out[mask] = v0 + (v - v0) * (t[mask] - t0) / (time - t0)
                else:  # EXP
                    if v0 == 0.0 or (v0 < 0) != (v < 0):
                        out[mask] = v0  # spec: opposite signs / V0 = 0 hold V0
                    else:
                        out[mask] = v0 * np.power(v / v0, (t[mask] - t0) / (time - t0))
            prev_t, prev_v = time, v
        # After the last event the value holds.
        out[t >= prev_t] = prev_v
        return out


def _hold(out: np.ndarray, t: np.ndarray, t0: float, t1: float, v: float) -> None:
    mask = (t >= t0) & (t < t1)
    out[mask] = v


def adsr(param: Param, a: float, d: float, s: float, r: float, peak: float, start: float) -> Param:
    """SFXEngine.createADSR (audio.md 2.8): set 0, lin peak, exp max(s*peak, 0.001), exp 0.001 (no sustain hold)."""
    sustain = max(s * peak, 0.001)
    param.set(0.0, start)
    param.lin(peak, start + a)
    param.exp(sustain, start + a + d)
    param.exp(0.001, start + a + d + r)
    return param


def score_env(param: Param, t: float, peak: float, attack: float, hold: float, release: float) -> Param:
    """ScorePlayer.envGain (audio.md 6.6)."""
    param.set(0.0001, t)
    param.lin(peak, t + attack)
    param.set(peak, t + attack + hold)
    param.exp(0.0001, t + attack + hold + release)
    return param


def pluck(param: Param, t: float, peak: float, decay: float) -> Param:
    """ScorePlayer.pluckGain (audio.md 6.6)."""
    param.set(0.0001, t)
    param.lin(peak, t + 0.006)
    param.exp(0.0001, t + decay)
    return param

"""Port of src/systems/audio/Composer.ts (audio.md 6.3). The xorshift RNG and the phrase writer are bit-exact: the RNG
draw order is the TypeScript order (golden vectors in tests/test_composer.py)."""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Callable, Sequence

MODES: dict[str, list[int]] = {
    "major": [0, 2, 4, 5, 7, 9, 11],
    "minor": [0, 2, 3, 5, 7, 8, 10],
    "dorian": [0, 2, 3, 5, 7, 9, 10],
    "phrygian": [0, 1, 3, 5, 7, 8, 10],
    "lydian": [0, 2, 4, 6, 7, 9, 11],
    "harmonicMinor": [0, 2, 3, 5, 7, 8, 11],
    "phrygianDominant": [0, 1, 4, 5, 7, 8, 10],
}

RHYTHMS_4: list[list[float]] = [[2, 1, 1], [1, 1, 2], [1.5, 0.5, 2], [1, 0.5, 0.5, 2], [3, 1], [-1, 1, 2], [2, -1, 1],
                                 [1, 1, 1, 1]]
RHYTHMS_3: list[list[float]] = [[2, 1], [1, 1, 1], [1.5, 0.5, 1], [3], [-1, 1, 1]]


def load_tables(composer_json: dict) -> None:
    """Use the exported tables (Data/music.json "composer") — identical to the defaults above."""
    MODES.clear()
    MODES.update({k: list(v) for k, v in composer_json["modes"].items()})
    RHYTHMS_4[:] = [list(c) for c in composer_json["rhythms4"]]
    RHYTHMS_3[:] = [list(c) for c in composer_json["rhythms3"]]


def midi_to_hz(midi: float) -> float:
    return 440.0 * math.pow(2.0, (midi - 69) / 12.0)


def degree_to_midi(tonic: int, mode: str, degree: int) -> int:
    steps = MODES[mode]
    octave = math.floor(degree / 7)
    idx = ((degree % 7) + 7) % 7
    return tonic + octave * 12 + steps[idx]


def chord_degrees(root: int, sevenths: bool = False) -> list[int]:
    return [root, root + 2, root + 4, root + 6] if sevenths else [root, root + 2, root + 4]


def voice_chord(tonic: int, mode: str, degrees: Sequence[int], low: int, high: int) -> list[int]:
    out: list[int] = []
    for d in degrees:
        n = degree_to_midi(tonic, mode, d)
        while n < low:
            n += 12
        while n > high:
            n -= 12
        if n >= low:
            out.append(n)
    return sorted(set(out))


def make_rng(seed: int) -> Callable[[], float]:
    """xorshift32 (Composer.ts makeRng): s = (seed >>> 0) || 1; s ^= s << 13; s ^= s >>> 17; s ^= s << 5; s / 2^32."""
    s = (int(seed) & 0xFFFFFFFF) or 1
    state = [s]

    def rng() -> float:
        x = state[0]
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= x >> 17
        x ^= (x << 5) & 0xFFFFFFFF
        state[0] = x
        return x / 4294967296.0

    return rng


@dataclass
class NoteEvent:
    beat: float
    length: float
    midi: int
    velocity: float


def write_phrase(tonic: int, mode: str, chord_roots: Sequence[int], beats_per_bar: int, low: int, high: int,
                 density: float, rng: Callable[[], float]) -> list[NoteEvent]:
    events: list[NoteEvent] = []
    cells = RHYTHMS_3 if beats_per_bar == 3 else RHYTHMS_4
    pool = [n for n in (degree_to_midi(tonic, mode, d) for d in range(-14, 28)) if low <= n <= high]
    if not pool:
        return events

    def chord_tones_in(root_deg: int) -> list[int]:
        pcs = {((degree_to_midi(tonic, mode, d) % 12) + 12) % 12 for d in chord_degrees(root_deg)}
        return [n for n in pool if ((n % 12) + 12) % 12 in pcs]

    def nearest(frm: float, choices: list[int]) -> int:
        best = choices[0]
        for n in choices:
            if abs(n - frm) < abs(best - frm):
                best = n
        return best

    first = chord_tones_in(chord_roots[0])
    current = nearest((low + high) / 2, first if first else pool)
    beat_at = 0
    for bar, root in enumerate(chord_roots):
        last_bar = bar == len(chord_roots) - 1
        cell = cells[math.floor(rng() * len(cells))]
        if rng() > density:
            found = next((c for c in cells if len(c) <= 2 and all(x > 0 for x in c)), None)
            if found is not None:
                cell = found
        if last_bar:
            cell = [1, 2] if beats_per_bar == 3 else [2, 2]
        tones = chord_tones_in(root)
        pos = 0.0
        for i, ln in enumerate(cell):
            dur = abs(ln)
            if ln > 0:
                strong = pos == 0 or (beats_per_bar == 4 and pos == 2)
                if strong and tones:
                    nxt = nearest(current + (-1 if rng() < 0.5 else 1), tones)
                else:
                    idx = pool.index(nearest(current, pool))
                    leap = 2 if rng() < 0.2 else 1
                    direction = -1 if rng() < 0.5 else 1
                    nxt = pool[max(0, min(len(pool) - 1, idx + direction * leap))]
                if last_bar and i == len(cell) - 1 and tones:
                    nxt = nearest(nxt, tones)
                events.append(NoteEvent(beat=beat_at + pos, length=dur, midi=nxt,
                                        velocity=(0.9 if strong else 0.7) * (0.85 + rng() * 0.15)))
                current = nxt
            pos += dur
        beat_at += beats_per_bar
    return events

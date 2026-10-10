"""Offline port of src/systems/audio/ScorePlayer.ts (audio.md 6.4 - 6.6).

The player schedules bar by bar exactly like the web (same section rules, same RNG draw order); instead of creating
Web Audio nodes it records graph Voices that graph.mix renders. Each voice keeps the web's node order (pad / bass /
arpeggio / lead: oscillators -> gain envelope -> lowpass; shaker: noise -> highpass -> gain). All voices are mono and
pass through the score trim (0.4) before the theme's effects chain.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field

import numpy as np

from . import composer
from .composer import chord_degrees, degree_to_midi, midi_to_hz, voice_chord, write_phrase
from .graph import BufferSource, Filter, Gain, Mod, Osc, Voice
from .params import Param, pluck, score_env

BASS_FLOOR = 40


@dataclass
class ScoreSpec:
    tonic: int
    mode: str
    progression: list[int]
    bars_per_chord: int
    beats_per_bar: int
    tempo: float
    pad: dict
    bass: dict | None
    lead: dict
    arp: dict | None
    bell: dict | None
    combat_tempo: float

    @staticmethod
    def from_json(d: dict) -> "ScoreSpec":
        return ScoreSpec(
            tonic=int(d["tonic"]), mode=d["mode"], progression=[int(x) for x in d["progression"]],
            bars_per_chord=int(d["barsPerChord"]), beats_per_bar=int(d["beatsPerBar"]), tempo=float(d["tempo"]),
            pad=dict(d["pad"]), bass=dict(d["bass"]) if d.get("bass") else None, lead=dict(d["lead"]),
            arp=dict(d["arp"]) if d.get("arp") else None, bell=dict(d["bell"]) if d.get("bell") else None,
            combat_tempo=float(d["combatTempo"]),
        )

    def check_invariants(self) -> None:
        # src/__tests__/Composer.test.ts: lead.low >= 55, arp.low >= 55, progression length >= 3.
        assert self.lead["low"] >= 55, "lead.low < 55"
        assert self.arp is None or self.arp["low"] >= 55, "arp.low < 55"
        assert len(self.progression) >= 3, "progression shorter than 3"


@dataclass
class ScoreTrace:
    """What the web player would have created (golden vectors, audio.md 6.8)."""

    oscillators: int = 0
    noise_starts: int = 0
    leads: list[tuple[float, int, float]] = field(default_factory=list)  # (beat, midi, velocity)
    bells: list[tuple[float, int]] = field(default_factory=list)  # (beat, midi)


class ScorePlayer:
    def __init__(self, spec: ScoreSpec, state: str, start_time: float, seed: int, sr: int):
        assert state in ("explore", "combat")
        self.spec = spec
        self.state = state
        self.rng = composer.make_rng(seed)
        self.beat_sec = 60.0 / (spec.tempo * (spec.combat_tempo if state == "combat" else 1.0))
        self.bar = 0
        self.start_time = start_time
        self.next_bar_time = start_time
        self.sr = sr
        self.noise: np.ndarray | None = None
        self.voices: list[Voice] = []
        self.trace = ScoreTrace()

    @property
    def bar_sec(self) -> float:
        return self.spec.beats_per_bar * self.beat_sec

    def schedule_bars(self, bars: int) -> None:
        for _ in range(bars):
            self._schedule_bar(self.bar, self.next_bar_time)
            self.bar += 1
            self.next_bar_time += self.spec.beats_per_bar * self.beat_sec

    # ---- structure ----
    def _section(self, bar: int) -> str:
        if self.state == "combat":
            return "thin" if bar % 16 >= 14 else "full"
        cycle = bar // 16
        b = bar % 16
        if b < 4:
            return "intro"
        if b < 12:
            return "full"
        if b < 14:
            return "thin"
        return "rest" if cycle % 2 == 1 else "thin"

    def _chord_root_at(self, bar: int) -> int:
        p = self.spec.progression
        return p[(bar // self.spec.bars_per_chord) % len(p)]

    def _schedule_bar(self, bar: int, t: float) -> None:
        sec = self._section(bar)
        root = self._chord_root_at(bar)
        bar_sec = self.spec.beats_per_bar * self.beat_sec
        chord_starts = bar % self.spec.bars_per_chord == 0
        if chord_starts and sec != "rest":
            self._pad(root, t, self.spec.bars_per_chord * bar_sec, 0.8 if sec == "intro" else 1.0)
        if self.spec.bass and (sec == "full" or (self.state == "combat" and sec == "thin")):
            beats = [b for b in (0, 2) if b < self.spec.beats_per_bar] if self.state == "combat" \
                else self.spec.bass["beats"]
            for b in beats:
                self._bass(root, t + b * self.beat_sec, 1.0 if b == 0 else 0.7)
        if self.spec.arp and sec in ("full", "thin"):
            self._arpeggio(root, t, 0.6 if sec == "thin" else 1.0)
        if sec == "full" and bar % 2 == 0 and (self.state == "combat" or self.rng() < 0.85):
            self._phrase(bar, t)
        if self.spec.bell and sec in ("intro", "thin") and bar % 2 == 0:
            self._bell_tone(root, t + self.beat_sec * (0 if self.rng() < 0.5 else 1))
        if self.state == "combat":
            self._percussion(t)

    def _beat_of(self, t: float) -> float:
        return round((t - self.start_time) / self.beat_sec, 6)

    # ---- voices ----
    def _osc(self, wave: str, freq: float, t: float, stop: float, detune: float = 0.0) -> Osc:
        self.trace.oscillators += 1
        o = Osc(wave=wave, freq=Param(440.0).set(freq, t), start=t, stop=stop)
        if detune:
            o.detune = Param(0.0).set(detune, t)
        return o

    def _pad(self, root_deg: int, t: float, dur: float, level: float) -> None:
        pad = self.spec.pad
        notes = voice_chord(self.spec.tonic, self.spec.mode, chord_degrees(root_deg, self.spec.mode != "major"), 55, 72)
        lp = Filter("lowpass", Param(350.0).set(pad["cutoff"] * 0.6, t).lin(pad["cutoff"], t + dur * 0.5)
                    .lin(pad["cutoff"] * 0.7, t + dur), q=0.5)
        attack = min(1.6, dur * 0.3)
        release = 1.8
        g = score_env(Param(1.0), t, pad["gain"] * level / math.sqrt(len(notes)), attack, max(0.1, dur - attack),
                      release)
        sources = []
        for n in notes:
            f = midi_to_hz(n)
            sources.append(self._osc(pad["wave"], f, t, t + dur + release + 0.1, -6))
            sources.append(self._osc(pad["wave"], f, t, t + dur + release + 0.1, 6))
        self.voices.append(Voice(sources=sources, chain=[Gain(g), lp]))

    def _bass(self, root_deg: int, t: float, accent: float) -> None:
        spec = self.spec.bass
        n = degree_to_midi(self.spec.tonic, self.spec.mode, root_deg) - 12
        while n < BASS_FLOOR:
            n += 12
        while n > BASS_FLOOR + 12:
            n -= 12
        lp = Filter("lowpass", Param(350.0).set(900, t).exp(260, t + 0.35))
        g = pluck(Param(1.0), t, spec["gain"] * accent, 0.55)
        sources = [self._osc("triangle", midi_to_hz(n), t, t + 0.6), self._osc("sine", midi_to_hz(n + 12), t, t + 0.6)]
        self.voices.append(Voice(sources=sources, chain=[Gain(g), lp]))

    def _arpeggio(self, root_deg: int, t: float, level: float) -> None:
        arp = self.spec.arp
        tones = voice_chord(self.spec.tonic, self.spec.mode, chord_degrees(root_deg), arp["low"], arp["high"])
        if not tones:
            return
        ext = [n for n in tones + [tones[0] + 12] if n <= arp["high"] + 12]
        per_beat = 2 if self.state == "combat" else int(arp["perBeat"])
        steps = self.spec.beats_per_bar * per_beat
        pattern = ext + list(reversed(ext[1:-1]))
        step_sec = self.beat_sec / per_beat
        for i in range(steps):
            if per_beat == 2 and i % 4 == 3 and self.rng() < 0.5:
                continue
            n = pattern[i % len(pattern)]
            at = t + i * step_sec
            lp = Filter("lowpass", Param(350.0).set(3200, at).exp(900, at + arp["decay"]))
            g = pluck(Param(1.0), at, arp["gain"] * level * (1.0 if i % per_beat == 0 else 0.7), arp["decay"])
            src = self._osc(arp["wave"], midi_to_hz(n), at, at + arp["decay"] + 0.05)
            self.voices.append(Voice(sources=[src], chain=[Gain(g), lp]))

    def _phrase(self, bar: int, t: float) -> None:
        lead = self.spec.lead
        events = write_phrase(
            self.spec.tonic, self.spec.mode, [self._chord_root_at(bar), self._chord_root_at(bar + 1)],
            self.spec.beats_per_bar, lead["low"], lead["high"],
            min(1.0, lead["density"] + 0.25) if self.state == "combat" else lead["density"], self.rng)
        for e in events:
            at = t + e.beat * self.beat_sec
            dur = e.length * self.beat_sec
            f = midi_to_hz(e.midi)
            lp = Filter("lowpass", Param(6000.0 if lead["wave"] == "sine" else 2400.0))
            release = min(0.9, dur * 0.8)
            g = score_env(Param(1.0), at, lead["gain"] * e.velocity, 0.04, max(0.05, dur * 0.7), release)
            stop = at + dur * 0.7 + release + 0.1
            a = self._osc(lead["wave"], f, at, stop, -4)
            b = self._osc(lead["wave"], f, at, stop, 4)
            self.trace.leads.append((self._beat_of(at), e.midi, e.velocity))
            if lead["vibrato"] > 0 and dur > 0.5:
                lfo = self._osc("sine", 5.2, at, stop)
                depth = Param(1.0).set(0, at).lin(lead["vibrato"], at + min(0.6, dur * 0.5))
                mod = Mod(source=lfo, gain=depth)
                a.detune_mods.append(mod)
                b.detune_mods.append(mod)
            self.voices.append(Voice(sources=[a, b], chain=[Gain(g), lp]))

    def _bell_tone(self, root_deg: int, t: float) -> None:
        bell = self.spec.bell
        tones = voice_chord(self.spec.tonic, self.spec.mode, chord_degrees(root_deg), 76, 91)
        if not tones:
            return
        n = tones[math.floor(self.rng() * len(tones))]
        self.trace.bells.append((self._beat_of(t), n))
        g = pluck(Param(1.0), t, bell["gain"], 2.6)
        self.voices.append(Voice(sources=[self._osc("sine", midi_to_hz(n), t, t + 2.7)], chain=[Gain(g)]))
        partial = pluck(Param(1.0), t, bell["gain"] * 0.3, 1.2)
        self.voices.append(Voice(sources=[self._osc("sine", midi_to_hz(n) * 2.76, t, t + 1.3)], chain=[Gain(partial)]))

    def _percussion(self, t: float) -> None:
        if self.noise is None:
            length = int(math.floor(self.sr * 0.3))
            r = composer.make_rng(7)
            self.noise = np.array([r() * 2 - 1 for _ in range(length)], dtype=np.float32).astype(np.float64)
        for b in range(self.spec.beats_per_bar):
            at = t + b * self.beat_sec
            if b % 2 == 0:
                g = pluck(Param(1.0), at, 0.09 if b == 0 else 0.06, 0.22)
                o = Osc(wave="sine", freq=Param(440.0).set(220, at).exp(110, at + 0.18), start=at, stop=at + 0.25)
                self.trace.oscillators += 1
                self.voices.append(Voice(sources=[o], chain=[Gain(g)]))
            for off in (0.5,):
                st = at + off * self.beat_sec
                self.trace.noise_starts += 1
                src = BufferSource(self.noise, st, st + 0.1)
                hp = Filter("highpass", Param(6000.0))
                g = pluck(Param(1.0), st, 0.025, 0.08)
                self.voices.append(Voice(sources=[src], chain=[hp, Gain(g)]))


def bar_count_seconds(spec: ScoreSpec, state: str, bars: int) -> float:
    beat = 60.0 / (spec.tempo * (spec.combat_tempo if state == "combat" else 1.0))
    return bars * spec.beats_per_bar * beat

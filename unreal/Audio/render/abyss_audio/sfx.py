"""SFX recipe language (audio.md 4, 11) and its interpreter.

A recipe is plain JSON-able data:

    {"id": "hit", "length": 0.2, "channels": 1, "layers": [layer, ...]}
    layer  = {"sources": [source, ...], "chain": [proc, ...], "pan": -1..1 (stereo recipes only, optional)}
    source = {"kind": "osc", "wave": "sine|square|sawtooth|triangle", "freq": auto, "detune": auto?,
              "start": s, "stop": s, "lfo": [{"target": "frequency|detune", "wave": w, "rate": auto,
                                              "depth": auto, "start": s, "stop": s}]?}
           | {"kind": "noise", "start": s, "stop": s, "frames": n?}      (fresh seeded uniform noise per source)
    proc   = {"type": "shaper", "curve": "tanh2|tanh3|tanh4|sqrt"}
           | {"type": "filter", "kind": "lowpass|highpass|bandpass|lowshelf", "freq": auto, "q": q?, "gain": dB?}
           | {"type": "gain", "gain": auto}
    auto   = number | [["set"|"lin"|"exp", value, time], ...]

The builders below mirror SFXEngine.ts helpers one to one (createADSR, createNoiseBurst, createTone, sfxChime), so the
recipe modules read like the TypeScript they port. Noise sources draw from one seeded generator per rendered variant
in layer order: variants differ only in their noise (audio.md 9.5).
"""

from __future__ import annotations

import math
import zlib
from typing import Any

import numpy as np

from . import graph, shaper
from .params import Param

Auto = Any  # number | list of [kind, value, time]


class Recipe:
    def __init__(self, rid: str, length: float, channels: int = 1, description: str = "", source: str = ""):
        self.id = rid
        self.length = float(length)
        self.channels = channels
        self.description = description
        self.source = source
        self.layers: list[dict] = []

    # ---- raw layer API ----
    def layer(self, sources: list[dict], chain: list[dict], pan: float | None = None) -> dict:
        lay: dict = {"sources": sources, "chain": chain}
        if pan is not None:
            lay["pan"] = float(pan)
        self.layers.append(lay)
        return lay

    @property
    def has_noise(self) -> bool:
        return any(s["kind"] == "noise" for lay in self.layers for s in lay["sources"])

    def to_json(self) -> dict:
        d = {"id": self.id, "length": round(self.length, 6), "channels": self.channels, "layers": self.layers}
        if self.description:
            d["description"] = self.description
        if self.source:
            d["source"] = self.source
        return d

    # ---- SFXEngine.ts helpers ----
    def adsr_osc(self, wave: str, freq: Auto, start: float, stop: float, env: list, chain_before: list | None = None,
                 detune: Auto | None = None, lfo: list | None = None, pan: float | None = None) -> dict:
        """osc -> [chain_before...] -> gain(env) -> destination."""
        return self.layer([osc(wave, freq, start, stop, detune=detune, lfo=lfo)],
                          list(chain_before or []) + [gain(env)], pan=pan)

    def noise_burst(self, duration: float, filter_freq: float, filter_type: str, start: float, peak: float = 0.25,
                    pan: float | None = None) -> dict:
        """createNoiseBurst: noise (ceil(sr*dur) frames) -> biquad(type, f set at start, default Q) ->
        gain ADSR(0.005, 0.3 dur, 0.3, 0.5 dur, peak) @ start; the source stops at start + duration."""
        return self.layer(
            [noise(start, start + duration, frames_for=duration)],
            [filt(filter_type, [["set", filter_freq, start]]),
             gain(adsr(0.005, duration * 0.3, 0.3, duration * 0.5, peak, start))], pan=pan)

    def tone(self, freq: float, wave: str, duration: float, level: float, start: float,
             pan: float | None = None) -> dict:
        """createTone: oscillator at a constant frequency, constant gain (hard on / off, QUIRK Q9 kept)."""
        return self.layer([osc(wave, [["set", freq, start]], start, start + duration)],
                          [gain([["set", level, start]])], pan=pan)

    def chime(self, freqs: list[float], step: float, level: float, t: float = 0.0) -> None:
        """sfxChime: per note set 0, lin level @+6 ms, exp 0.001 @+0.32; span +0.34."""
        for i, f in enumerate(freqs):
            s = t + i * step
            self.layer([osc("sine", [["set", f, s]], s, s + 0.34)],
                       [gain([["set", 0, s], ["lin", level, s + 0.006], ["exp", 0.001, s + 0.32]])])


# ---- data constructors ----
def osc(wave: str, freq: Auto, start: float, stop: float, detune: Auto | None = None,
        lfo: list | None = None) -> dict:
    d: dict = {"kind": "osc", "wave": wave, "freq": freq, "start": float(start), "stop": float(stop)}
    if detune is not None:
        d["detune"] = detune
    if lfo:
        d["lfo"] = lfo
    return d


def lfo(target: str, wave: str, rate: Auto, depth: Auto, start: float, stop: float) -> dict:
    return {"target": target, "wave": wave, "rate": rate, "depth": depth, "start": float(start), "stop": float(stop)}


def noise(start: float, stop: float, frames_for: float | None = None) -> dict:
    d: dict = {"kind": "noise", "start": float(start), "stop": float(stop)}
    if frames_for is not None:
        d["bufferSec"] = float(frames_for)
    return d


def filt(kind: str, freq: Auto, q: float | None = None, gain_db: float | None = None) -> dict:
    d: dict = {"type": "filter", "kind": kind, "freq": freq}
    if q is not None:
        d["q"] = float(q)
    if gain_db is not None:
        d["gain"] = float(gain_db)
    return d


def shape(curve: str) -> dict:
    return {"type": "shaper", "curve": curve}


def gain(g: Auto) -> dict:
    return {"type": "gain", "gain": g}


def adsr(a: float, d: float, s: float, r: float, peak: float, start: float) -> list:
    """createADSR as automation events."""
    sustain = max(s * peak, 0.001)
    return [["set", 0, start], ["lin", peak, start + a], ["exp", sustain, start + a + d],
            ["exp", 0.001, start + a + d + r]]


def env(peak: float, attack: float, hold: float, release: float, t: float) -> list:
    """ScorePlayer.envGain as automation events."""
    return [["set", 0.0001, t], ["lin", peak, t + attack], ["set", peak, t + attack + hold],
            ["exp", 0.0001, t + attack + hold + release]]


def pluck_env(peak: float, decay: float, t: float) -> list:
    """ScorePlayer.pluckGain as automation events."""
    return [["set", 0.0001, t], ["lin", peak, t + 0.006], ["exp", 0.0001, t + decay]]


# ---- interpreter ----
def seed_for(asset_id: str, variant: int) -> int:
    """crc32(assetId) ^ variant (audio.md 9.3)."""
    return (zlib.crc32(asset_id.encode("utf-8")) ^ variant) & 0xFFFFFFFF


def _param(spec: Auto, default: float) -> Param:
    return Param.from_spec(spec, default)


def build_voices(recipe: Recipe | dict, rng: np.random.Generator, sr: int) -> list[graph.Voice]:
    data = recipe.to_json() if isinstance(recipe, Recipe) else recipe
    voices = []
    for lay in data["layers"]:
        sources = []
        for s in lay["sources"]:
            if s["kind"] == "osc":
                o = graph.Osc(wave=s["wave"], freq=_param(s["freq"], 440.0), start=s["start"], stop=s["stop"])
                if "detune" in s:
                    o.detune = _param(s["detune"], 0.0)
                for m in s.get("lfo", []):
                    src = graph.Osc(wave=m["wave"], freq=_param(m["rate"], 440.0), start=m["start"], stop=m["stop"])
                    mod = graph.Mod(source=src, gain=_param(m["depth"], 1.0))
                    (o.freq_mods if m["target"] == "frequency" else o.detune_mods).append(mod)
                sources.append(o)
            elif s["kind"] == "noise":
                frames = int(math.ceil(sr * s.get("bufferSec", s["stop"] - s["start"])))
                data_buf = (rng.random(frames) * 2.0 - 1.0).astype(np.float32).astype(np.float64)
                sources.append(graph.BufferSource(data_buf, s["start"], s["stop"], loop=bool(s.get("loop", False))))
            else:
                raise ValueError(f"unknown source kind {s['kind']!r}")
        chain = []
        for p in lay["chain"]:
            if p["type"] == "gain":
                chain.append(graph.Gain(_param(p["gain"], 1.0)))
            elif p["type"] == "filter":
                chain.append(graph.Filter(p["kind"], _param(p["freq"], 350.0), p.get("q"), p.get("gain", 0.0)))
            elif p["type"] == "shaper":
                chain.append(graph.Shaper(shaper.make_curve(p["curve"])))
            else:
                raise ValueError(f"unknown processor {p['type']!r}")
        voices.append(graph.Voice(sources=sources, chain=chain, pan=lay.get("pan")))
    return voices


def render(recipe: Recipe | dict, sr: int, seed: int, extra_sec: float = 0.01) -> np.ndarray:
    """(channels, n) with n = ceil((length + extra) * sr)."""
    data = recipe.to_json() if isinstance(recipe, Recipe) else recipe
    rng = np.random.Generator(np.random.PCG64(seed))
    voices = build_voices(data, rng, sr)
    n = int(math.ceil((data["length"] + extra_sec) * sr))
    return graph.mix(voices, n, sr, channels=int(data.get("channels", 1)))

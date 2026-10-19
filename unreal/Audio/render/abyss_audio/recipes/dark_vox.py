"""AU1 (Docs/dark-remake/implementation-plan.md 7): the dark remake's monster voices, in the A7 recipe language.

Voice families (monsters.md 1.1; the monster `family` -> voice family map is `MONSTER_FAMILY`):

* ``undead``  the Unburied: a dry rasp, mail rattle, a groan that is almost a word (they keep their last order).
* ``human``   the Living (zealots, deserters): breath, effort, chant fragments (「死守——」).
* ``gnawer``  the Gravegnawers: the goblin set of port_sfx re-pitched lower, plus wet chittering.
* ``breach``  Breach-blood: the slime set of port_sfx darkened (lower, duller, no cute up-sweeps), plus a sizzle.

Each family has aggro / hurt / death (two variants each, as A7). Named voices (monsters.md 2.5-2.6, chapter1.md
10-11): the Corpseherd (cracked handbell for every cast, a hummed lullaby under the pre-fight lines, a hoarse
「回家吧」, an old woman's gasp when hurt) and Valdras (a drill-yard order, the phase-change roar (intro and P2; its own
take for P3), hurt, death, plate grinding, the standard cracking in a wind that isn't there, the P3 banner fire). Every
sound and the lullaby melody are original. No cue loops: EvSfx has no stop, so a looping one-shot would never end (the
lullaby is hummed once, 17.6 s).

Every AU1 one-shot is cut where its tail has fallen TRIM_DB under its peak and faded to exact zero (`trim_tail`): no
retained memory or concurrency voice is spent on inaudible padding.

Monsters without a throat (`SILENT_MONSTERS`) play no family cue; the design's non-vocal monster sounds (monsters.md 2)
are reserved as `PLANNED_CUES` (ids for the core, wave and owner) until an AU package renders them.

Synthesis: a glottal oscillator (sawtooth) through parallel formant band-passes whose centre frequencies glide between
vowel targets, so the ear hears the shape of a word without a word; rasp is an irregular glottal pulse train applied
as gain automation to formant-filtered noise, plus square-wave pitch jitter; consonants are filtered noise bursts;
metal is short inharmonic partials. Structural randomness (pulse timings, mail ticks) is seeded by the recipe id and
the noise by the asset name (sfx.seed_for), so every render is deterministic.
"""

from __future__ import annotations

import copy
import math
import random
import zlib

import numpy as np

from ..sfx import Recipe, filt, gain, lfo, noise, osc, pluck_env, shape

SRC = "unreal/Docs/dark-remake/implementation-plan.md 7 AU1; monsters.md 1.1, 2.1-2.6; chapter1.md 10-11, 16"

FAMILIES = ("undead", "human", "gnawer", "breach")

# Chapter 1 monster id -> voice family (monsters.md 2.0). Valdras and the Corpseherd also have named cues
# (NAMED_OVERRIDES); the family is their fallback.
MONSTER_FAMILY = {
    "risen_footman": "undead", "risen_arbalest": "undead", "risen_shieldbearer": "undead",
    "dead_standard_bearer": "undead", "sub_pitwarden": "undead", "valdras": "undead",
    "oath_zealot": "human", "deserter": "human", "deserter_drummer": "human", "hunt_dreck": "human",
    "hunt_harke": "human", "miniboss_corpseherd": "human",
    "gravegnawer": "gnawer", "bonegorger": "gnawer", "hunt_hungerbell": "gnawer",
    "breachclot": "breach", "cinderbloat": "breach",
    # Stretch (monsters.md 2.7, cut first): the Ash Outrider is a dead outrider on foot (the footman kit).
    "ash_outrider": "undead",
}
# Monsters with no voice (UE: play no family cue for them; their own sounds are PLANNED_CUES).
SILENT_MONSTERS = {
    "false_lantern": "stretch (monsters.md 2.7): a cold drifting light with a lamprey mouth, no throat; its sound is "
                     "the planned sfx_false_lantern_mouth",
}
# Monster-family names of the design (monsters.md 1.1) -> voice family.
FAMILY_ALIASES = {"unburied": "undead", "living": "human", "gravegnawers": "gnawer", "gravegnawer": "gnawer",
                  "breach_blood": "breach", "breachblood": "breach", "raised": "undead"}

# Per-variant pitch factors and timing offsets (variants are the same throat, not identical takes).
PITCH = (1.0, 0.94)
TIMING = (0.0, 0.012)

# Vowel / sonorant formant targets F1-F3 (Hz, adult male). Near-words glide between them.
VOWELS = {
    "i": (270, 2290, 3010), "I": (390, 1990, 2550), "e": (530, 1840, 2480), "ae": (660, 1720, 2410),
    "a": (730, 1090, 2440), "o": (570, 840, 2410), "u": (300, 870, 2240), "U": (440, 1020, 2240),
    "uh": (640, 1190, 2390), "schwa": (500, 1500, 2500), "y": (280, 1900, 2350), "ih": (340, 1400, 2600),
    "er": (490, 1350, 1690), "l": (360, 1000, 2700), "n": (280, 1300, 2500), "m": (250, 1100, 2300),
    "w": (290, 650, 2200),
}
FORMANT_Q = (5.0, 9.0, 11.0)
FORMANT_LEVEL = (1.0, 0.55, 0.28)


def _rng(rid: str) -> random.Random:
    return random.Random(zlib.crc32(rid.encode("utf-8")))


def _rid(kind: str, name: str, v: int) -> str:
    return f"vox_{kind}_{name}_{v + 1:02d}"


def _guarded(fn):
    """Recipe constructors return tidy recipes (_tidy) whose automations take effect on the source's first frame.

    A source starts on frame ceil(t * sr) while an automation's first `set` applies from its own time t on; when the
    float t and the frame time differ in the last bit, that first frame would read the param's default (gain 1.0:
    a one-sample spike at the start of a tick). Moving each automation's first `set` 1 us earlier fixes it without
    changing anything else."""

    def wrapped(*args, **kwargs):
        r = fn(*args, **kwargs)
        r.layers = _tidy(r.layers)
        seen: set[int] = set()
        for lay in r.layers:
            for s in lay["sources"]:
                for key in ("freq", "detune"):
                    _guard_auto(s.get(key), seen)
                for m in s.get("lfo", []):
                    _guard_auto(m.get("rate"), seen)
                    _guard_auto(m.get("depth"), seen)
            for proc in lay["chain"]:
                _guard_auto(proc.get("freq") if proc["type"] == "filter" else proc.get("gain"), seen)
        return r

    wrapped.__name__ = fn.__name__
    wrapped.__doc__ = fn.__doc__
    return wrapped


def _tidy(x):
    """Numbers to 7 significant digits (10 us at 18 s, far below a sample): compact, readable recipe data."""
    if isinstance(x, float):
        return float(f"{x:.7g}")
    if isinstance(x, list):
        return [_tidy(v) for v in x]
    if isinstance(x, dict):
        return {k: _tidy(v) for k, v in x.items()}
    return x


def _guard_auto(a, seen: set[int]) -> None:
    if isinstance(a, list) and a and a[0][0] == "set" and id(a[0]) not in seen:
        seen.add(id(a[0]))
        a[0][2] = round(a[0][2] - 1e-6, 9)


TRIM_DB = -72.0
TRIM_FADE_SEC = 0.02


def trim_tail(y: np.ndarray, sr: int, floor_db: float = TRIM_DB, fade_sec: float = TRIM_FADE_SEC) -> np.ndarray:
    """(channels, n) one-shot cut `fade_sec` after the last sample above `floor_db` re its peak, the kept tail faded
    (cos^2) to exact zero. What is cut lies under the floor: inaudible, but it costs retained memory and keeps a
    concurrency voice busy."""
    peak = float(np.max(np.abs(y))) if y.size else 0.0
    if peak <= 0.0:
        return y
    loud = np.nonzero(np.max(np.abs(y), axis=0) > peak * 10.0 ** (floor_db / 20.0))[0]
    n = min(y.shape[1], int(loud[-1]) + 1 + int(fade_sec * sr))
    out = y[:, :n].copy()
    k = min(n, int(fade_sec * sr))
    if k:
        w = np.cos(np.linspace(0.0, math.pi / 2, k)) ** 2
        w[-1] = 0.0  # cos(pi / 2)^2 is 4e-33, not 0
        out[:, n - k:] *= w
    return out


# ---------------------------------------------------------------------------------------------------------------------
# Building blocks
# ---------------------------------------------------------------------------------------------------------------------
def curve(*pts: tuple[float, float]) -> list:
    """Gain automation through (time, value) points: set, then linear ramps; a final value <= 1e-4 is reached with
    an exponential ramp (a natural decay)."""
    out = [["set", max(pts[0][1], 1e-4), pts[0][0]]]
    for t, v in pts[1:]:
        if v <= 1e-4:
            out.append(["exp", 1e-4, t])
        else:
            out.append(["lin", v, t])
    return out


def blip(peak: float, t: float, attack: float = 0.0005, decay: float = 0.006) -> list:
    """A short tick envelope (pluck_env's 6 ms attack is longer than a tick): rise in `attack`, fall in `decay`."""
    return [["set", 1e-4, t], ["lin", peak, t + attack], ["exp", 1e-4, t + attack + decay]]


def scaled(env: list, k: float) -> list:
    return [[kind, val * k if kind != "set" or val > 1e-4 else val, t] for kind, val, t in env]


def track(points: list[tuple[float, str]], i: int, fs: float) -> list:
    """Formant i's centre-frequency automation through (time, vowel) targets, scaled by fs (throat size)."""
    out = []
    for k, (t, vowel) in enumerate(points):
        out.append(["set" if k == 0 else "lin", VOWELS[vowel][i] * fs, t])
    return out


def jitter(start: float, stop: float, cents: float, rate: float = 29.0, slow_cents: float = 0.0,
           slow_rate: float = 6.3) -> list:
    """Pitch roughness: a square-wave detune flip (fry-like) and an optional slow wobble."""
    mods = [lfo("detune", "square", rate, cents, start, stop)]
    if slow_cents:
        mods.append(lfo("detune", "triangle", slow_rate, slow_cents, start, stop))
    return mods


def voiced(r: Recipe, f0, start: float, stop: float, env: list, points: list, fs: float = 1.0, *,
           wave: str = "sawtooth", drive: float = 0.0, curve_name: str = "tanh3", mods: list | None = None,
           levels: tuple = FORMANT_LEVEL, q: tuple = FORMANT_Q, post: list | None = None) -> None:
    """A glottal oscillator through parallel formant band-passes (one layer per formant, coherent sources).
    `drive` > 0 saturates the source first (shouts, strain): gain(drive) -> shaper."""
    for i, (qq, lv) in enumerate(zip(q, levels)):
        if lv <= 0:
            continue
        chain = ([gain(drive), shape(curve_name)] if drive else [])
        chain += [filt("bandpass", track(points, i, fs), qq)] + list(post or []) + [gain(scaled(env, lv))]
        r.layer([osc(wave, f0, start, stop, lfo=mods)], chain)


def pulses(rng: random.Random, start: float, stop: float, rate: tuple[float, float], floor: float = 0.1,
           spread: float = 0.3, peak: tuple[float, float] = (0.6, 1.0), attack: float = 0.0015) -> list:
    """An irregular pulse train as gain automation (glottal fry, stick-slip, chatter). `rate` = (Hz at start, Hz at
    stop), interpolated; each period: a fast rise to a random peak, an exponential fall to `floor`."""
    floor = max(floor, 1e-4)  # an exponential ramp never targets 0
    pts = [["set", floor, start]]
    t = start
    while t < stop:
        k = (t - start) / max(stop - start, 1e-6)
        hz = rate[0] + (rate[1] - rate[0]) * k
        dt = rng.uniform(1.0 - spread, 1.0 + spread) / hz
        pts.append(["lin", rng.uniform(*peak), t + attack])
        pts.append(["exp", floor, t + max(dt * 0.9, attack * 2)])
        t += dt
    return pts


def rasp(r: Recipe, rng: random.Random, start: float, stop: float, env: list, points: list, fs: float = 1.0,
         rate: tuple[float, float] = (38.0, 52.0), floor: float = 0.08, level2: float = 0.6,
         post: list | None = None) -> None:
    """Dry rasp: noise through the F1 / F2 formant tracks (broad), chopped by one irregular glottal pulse train."""
    pp = pulses(rng, start, stop, rate, floor)
    for i, (qq, lv) in enumerate(((2.2, 1.0), (3.5, level2))):
        r.layer([noise(start, stop)],
                [filt("bandpass", track(points, i, fs), qq)] + list(post or []) + [gain(pp), gain(scaled(env, lv))])


def breath(r: Recipe, start: float, stop: float, env: list, centre, q: float = 0.9, hp: float | None = None,
           post: list | None = None) -> None:
    """Breath / fricative: noise -> (high-pass) -> band-pass -> envelope."""
    chain = ([filt("highpass", hp)] if hp else []) + [filt("bandpass", centre, q)] + list(post or [])
    r.layer([noise(start, stop)], chain + [gain(env)])


def burst(r: Recipe, t: float, centre: float, level: float, dur: float = 0.014, q: float = 1.2) -> None:
    """A plosive release: a short band-passed noise click."""
    r.layer([noise(t, t + dur + 0.002)], [filt("bandpass", centre, q), gain(blip(level, t, 0.0008, dur))])


def ping(r: Recipe, t: float, partials: list[tuple[float, float, float]], level: float) -> None:
    """Struck metal: sine partials (freq, relative amplitude, decay seconds)."""
    for f, a, d in partials:
        r.layer([osc("sine", [["set", f, t]], t, t + d + 0.01)], [gain(pluck_env(level * a, d, t))])


def mail(r: Recipe, rng: random.Random, start: float, dur: float, density: float, level: float,
         bright: float = 1.0) -> None:
    """Mail shiver: scattered tiny rings (a noise tick and one or two high inharmonic partials), thinning out."""
    t = start + rng.uniform(0.0, 0.01)
    while t < start + dur:
        k = (t - start) / dur
        a = level * rng.uniform(0.35, 1.0) * (1.0 - 0.7 * k)
        f = rng.uniform(2900, 6100) * bright
        r.layer([noise(t, t + 0.008)], [filt("bandpass", f * 1.15, 7), gain(blip(a, t, 0.0004, 0.006))])
        d = rng.uniform(0.018, 0.04)
        r.layer([osc("sine", [["set", f, t]], t, t + d + 0.005)], [gain(pluck_env(a * 0.22, d, t))])
        if rng.random() < 0.5:
            f2 = f * rng.uniform(1.37, 1.62)
            if f2 < 15000:
                r.layer([osc("sine", [["set", f2, t]], t, t + d * 0.7 + 0.005)],
                        [gain(pluck_env(a * 0.12, d * 0.7, t))])
        t += rng.expovariate(density)


def crackle(r: Recipe, rng: random.Random, start: float, stop: float, density: float, level: float,
            hp: float = 3500.0, lp: float = 11000.0, decay: float = 0.004, fade: float = 1.5) -> None:
    """Sizzle / crackle: high-passed noise opened by sparse random spikes whose level fades out over the window
    (amplitude x (1 - k)^fade after a 30 ms fade-in), so the crackle dies with the body it rides on."""
    pts = [["set", 0.0, start]]
    t = start + rng.expovariate(density)
    span = max(stop - start, 1e-6)
    while t < stop - decay * 2.0:
        k = (t - start) / span
        a = level * rng.uniform(0.25, 1.0) * min(1.0, (t - start) / 0.03) * (1.0 - k) ** fade
        if a > 1e-4:
            pts += [["set", 0.0, t], ["lin", a, t + 0.0004], ["exp", 1e-4, t + decay * rng.uniform(0.6, 1.6)]]
        t += max(rng.expovariate(density), decay * 2.0)
    r.layer([noise(start, stop)], [filt("highpass", hp), filt("lowpass", lp), gain(pts)])


def chitter(r: Recipe, rng: random.Random, start: float, dur: float, rate: float, level: float,
            pitch: float = 1.0) -> None:
    """Teeth chatter: clusters of 3-6 wet clicks at `rate` Hz with short gaps between clusters, swelling and
    fading; each click a band-passed noise tick and a faint down-flicked tone."""
    t = start
    while t < start + dur:
        for _ in range(rng.randint(3, 6)):
            if t >= start + dur:
                break
            k = (t - start) / dur
            a = level * (0.45 + 0.55 * (1.0 - abs(2.0 * k - 1.0))) * rng.uniform(0.55, 1.0)
            fc = rng.uniform(2000, 3400) * pitch
            r.layer([noise(t, t + 0.006)], [filt("bandpass", fc, 2.5), gain(blip(a, t, 0.0004, 0.004))])
            ft = rng.uniform(1400, 2200) * pitch
            r.layer([osc("triangle", [["set", ft, t], ["exp", ft * 0.6, t + 0.01]], t, t + 0.012)],
                    [filt("lowpass", 3000), gain(blip(a * 0.18, t, 0.0008, 0.009))])
            t += rng.uniform(0.8, 1.2) / rate
        t += rng.uniform(0.025, 0.06)


def wet(r: Recipe, rng: random.Random, start: float, dur: float, level: float, n: int = 3) -> None:
    """Saliva: a low gurgle (detune-wobbled sine through a resonant low-pass) and a few wet smacks."""
    r.layer([osc("sine", [["set", 210 * rng.uniform(0.9, 1.1), start], ["exp", 150, start + dur]], start,
                 start + dur, lfo=[lfo("detune", "sine", rng.uniform(19, 26), 380, start, start + dur)])],
            [filt("lowpass", 820, 6), gain(curve((start, 0.0), (start + dur * 0.2, level), (start + dur, 1e-4)))])
    for _ in range(n):
        t = start + rng.uniform(0.05, 0.85) * dur
        f = rng.uniform(700, 1100)
        r.layer([osc("sine", [["set", f, t], ["exp", f * 0.38, t + 0.03]], t, t + 0.045)],
                [gain(pluck_env(level * 0.55, 0.04, t))])
        r.layer([noise(t, t + 0.02)], [filt("bandpass", rng.uniform(1100, 1700), 4), gain(pluck_env(level * 0.5, 0.02, t))])


def thud(r: Recipe, t: float, level: float, f0: float = 70.0, f1: float = 40.0, dur: float = 0.22) -> None:
    """A dull body impact: a falling sine and low-passed noise."""
    r.layer([osc("sine", [["set", f0, t], ["exp", f1, t + dur * 0.8]], t, t + dur)],
            [gain(curve((t, 0.0), (t + 0.004, level), (t + dur, 1e-4)))])
    r.layer([noise(t, t + dur * 0.6)], [filt("lowpass", 380, 1), gain(pluck_env(level * 0.6, dur * 0.6, t))])


def ash(r: Recipe, rng: random.Random, start: float, dur: float, level: float) -> None:
    """Ash crumbling: dry grit (band-passed noise in irregular grains) settling."""
    pp = pulses(rng, start, start + dur, (70.0, 25.0), floor=0.05, spread=0.6, peak=(0.2, 1.0), attack=0.0008)
    r.layer([noise(start, start + dur)],
            [filt("bandpass", [["set", 2600, start], ["exp", 1300, start + dur]], 1.4), gain(pp),
             gain(curve((start, 0.0), (start + 0.04, level), (start + dur * 0.45, level * 0.6), (start + dur, 1e-4)))])


def _variant(v: int) -> tuple[float, float]:
    return PITCH[v % len(PITCH)], TIMING[v % len(TIMING)]


# ---------------------------------------------------------------------------------------------------------------------
# Undead (the Unburied): dry rasp, mail, near-words. A large dead throat: formants x0.9, f0 70-105 Hz.
# ---------------------------------------------------------------------------------------------------------------------
UNDEAD_FS = 0.9


def _undead_voice(r: Recipe, rng: random.Random, f0: list, start: float, stop: float, env: list, points: list,
                  p: float, rasp_level: float = 0.9, rasp_rate: tuple[float, float] = (36.0, 48.0)) -> None:
    fs = UNDEAD_FS
    mods = jitter(start, stop, 38, rate=27.0 + 3 * rng.random(), slow_cents=22, slow_rate=5.1 + rng.random())
    voiced(r, f0, start, stop, scaled(env, 0.55), points, fs, drive=0.55, mods=mods)
    # Period doubling (the growl under the rasp): the same glottis an octave down, F1 only.
    sub = [[k, val * 0.5, t] for k, val, t in f0]
    voiced(r, sub, start, stop, scaled(env, 0.32), points, fs, drive=0.8, mods=mods, levels=(1.0, 0.0, 0.0))
    rasp(r, rng, start, stop, scaled(env, rasp_level), points, fs, rate=(rasp_rate[0] * p, rasp_rate[1] * p))


@_guarded
def aggro_undead(v: int) -> Recipe:
    """'Hold' (v1) / 'stand' (v2): the last order, rasped by a dead throat; mail shivers as it moves."""
    p, dt = _variant(v)
    rid = _rid("aggro", "undead", v)
    rng = _rng(rid)
    r = Recipe(rid, 0.85, description="Unburied aggro: a rasped near-word order + mail", source=SRC)
    if v % 2 == 0:  # h-o-l-(d)
        breath(r, 0.0, 0.11, curve((0.0, 0.0), (0.07, 0.05), (0.11, 1e-4)), 1100, 0.8)
        f0 = [["set", 84 * p, 0.06], ["lin", 98 * p, 0.16 + dt], ["exp", 76 * p, 0.5], ["exp", 70 * p, 0.6]]
        env = curve((0.06, 0.0), (0.1, 0.22), (0.3, 0.2), (0.5, 0.15), (0.57, 0.05), (0.6, 1e-4))
        pts = [(0.06, "o"), (0.33, "o"), (0.47, "l"), (0.6, "l")]
        _undead_voice(r, rng, f0, 0.06, 0.62, env, pts, p, rasp_level=0.75)
        burst(r, 0.635 + dt, 3300, 0.05, 0.012)
        breath(r, 0.64, 0.85, curve((0.64, 0.0), (0.68, 0.035), (0.85, 1e-4)), 900, 0.7)
    else:  # s-t-aa-n
        breath(r, 0.0, 0.12, curve((0.0, 0.0), (0.06, 0.07), (0.12, 1e-4)), 5200, 1.6, hp=3500)
        burst(r, 0.14, 3800, 0.06, 0.01)
        f0 = [["set", 92 * p, 0.15], ["lin", 104 * p, 0.22 + dt], ["exp", 74 * p, 0.62], ["exp", 66 * p, 0.72]]
        env = curve((0.15, 0.0), (0.18, 0.22), (0.45, 0.19), (0.6, 0.1), (0.72, 1e-4))
        pts = [(0.15, "ae"), (0.3, "a"), (0.5, "a"), (0.62, "n"), (0.72, "n")]
        _undead_voice(r, rng, f0, 0.15, 0.74, env, pts, p, rasp_level=0.75)
        breath(r, 0.7, 0.85, curve((0.7, 0.0), (0.73, 0.02), (0.85, 1e-4)), 800, 0.7)
    mail(r, rng, 0.04 + dt, 0.45, 55, 0.05)
    return r


@_guarded
def hurt_undead(v: int) -> Recipe:
    """A dry, rasped grunt jolted out of a dead chest; the mail jumps."""
    p, dt = _variant(v)
    rid = _rid("hurt", "undead", v)
    rng = _rng(rid)
    r = Recipe(rid, 0.38, description="Unburied hurt: rasped grunt + mail jolt", source=SRC)
    f0 = [["set", 96 * p, 0.0], ["lin", 104 * p, 0.03], ["exp", 68 * p, 0.26]]
    env = curve((0.0, 0.0), (0.006, 0.24), (0.08, 0.17), (0.22, 0.05), (0.27, 1e-4))
    pts = [(0.0, "uh" if v % 2 == 0 else "o"), (0.1, "uh" if v % 2 == 0 else "o"), (0.26, "schwa")]
    _undead_voice(r, rng, f0, 0.0, 0.28, env, pts, p, rasp_level=1.15, rasp_rate=(44.0, 30.0))
    breath(r, 0.2, 0.36, curve((0.2, 0.0), (0.23, 0.03), (0.36, 1e-4)), 1000, 0.8)
    mail(r, rng, 0.0 + dt, 0.2, 90, 0.04)
    return r


@_guarded
def death_undead(v: int) -> Recipe:
    """A long falling groan that is almost a word ('home' / 'why'), the voice breaking into fry; the body folds and
    crumbles to ash (mail cascade, a dull thud, grit)."""
    p, dt = _variant(v)
    rid = _rid("death", "undead", v)
    rng = _rng(rid)
    r = Recipe(rid, 1.05, description="Unburied death: near-word groan into fry, mail cascade, ash crumble", source=SRC)
    if v % 2 == 0:  # (h)-o-u-m
        breath(r, 0.0, 0.09, curve((0.0, 0.0), (0.06, 0.04), (0.09, 1e-4)), 1000, 0.8)
        pts = [(0.04, "o"), (0.3, "o"), (0.5, "u"), (0.62, "m"), (0.72, "m")]
    else:  # w-a-i
        pts = [(0.03, "w"), (0.12, "a"), (0.35, "a"), (0.6, "I"), (0.72, "i")]
    f0 = [["set", 92 * p, 0.04], ["lin", 98 * p, 0.12], ["exp", 64 * p, 0.5], ["exp", 46 * p, 0.72]]
    env = curve((0.04, 0.0), (0.09, 0.21), (0.35, 0.18), (0.6, 0.1), (0.72, 1e-4))
    _undead_voice(r, rng, f0, 0.04, 0.74, env, pts, p, rasp_level=1.0, rasp_rate=(46.0, 18.0))
    breath(r, 0.66, 0.95, curve((0.66, 0.0), (0.7, 0.03), (0.95, 1e-4)), 700, 0.7)
    mail(r, rng, 0.42 + dt, 0.4, 70, 0.05, bright=0.9)
    thud(r, 0.55 + dt, 0.06, 74 * p, 42 * p, 0.25)
    ash(r, rng, 0.6 + dt, 0.42, 0.05)
    return r


# ---------------------------------------------------------------------------------------------------------------------
# Human (the Living): breath, effort, chant fragments. f0 120-185 Hz, formants x1.0, mild strain.
# ---------------------------------------------------------------------------------------------------------------------
def _human_voice(r: Recipe, f0: list, start: float, stop: float, env: list, points: list, p: float,
                 drive: float = 0.6, breathy: float = 0.25, vib_from: float | None = None) -> None:
    mods = [lfo("detune", "triangle", 4.7, 8, start, stop)]
    if vib_from is not None:
        mods.append(lfo("detune", "sine", 5.6, [["set", 0, start], ["set", 0, vib_from], ["lin", 22, stop]],
                        start, stop))
    voiced(r, f0, start, stop, env, points, 1.0, drive=drive, curve_name="tanh2", mods=mods)
    for i, (qq, lv) in enumerate(((2.5, 1.0), (4.0, 0.7))):
        r.layer([noise(start, stop)], [filt("bandpass", track(points, i, 1.0), qq), gain(scaled(env, breathy * lv))])


def _inhale(r: Recipe, t: float, dur: float, level: float) -> None:
    breath(r, t, t + dur, curve((t, 0.0), (t + dur * 0.75, level), (t + dur, 1e-4)),
           [["set", 1200, t], ["lin", 1900, t + dur]], 1.1)


@_guarded
def aggro_human(v: int) -> Recipe:
    """v1: a sharp inhale and the chant fragment 「死守——」 (s-ih, sh-ou, held, strained, a late vibrato);
    v2: an effort war cry 'hh-HAH' with an exhale."""
    p, dt = _variant(v)
    rid = _rid("aggro", "human", v)
    r = Recipe(rid, 0.95 if v % 2 == 0 else 0.7, description="Living aggro: chant fragment / effort cry", source=SRC)
    _inhale(r, 0.0, 0.11, 0.05)
    if v % 2 == 0:
        breath(r, 0.12, 0.2, curve((0.12, 0.0), (0.15, 0.06), (0.2, 1e-4)), 6200, 1.6, hp=4200)
        f0 = [["set", 162 * p, 0.19], ["lin", 156 * p, 0.3]]
        _human_voice(r, f0, 0.19, 0.31, curve((0.19, 0.0), (0.21, 0.14), (0.28, 0.12), (0.31, 1e-4)),
                     [(0.19, "ih"), (0.31, "ih")], p)
        breath(r, 0.29, 0.39, curve((0.29, 0.0), (0.32, 0.07), (0.39, 1e-4)), 3000, 2.0, hp=1800)
        f0 = [["set", 166 * p, 0.37], ["lin", 176 * p, 0.46 + dt], ["lin", 168 * p, 0.7], ["exp", 146 * p, 0.92]]
        env = curve((0.37, 0.0), (0.41, 0.2), (0.7, 0.17), (0.86, 0.08), (0.93, 1e-4))
        _human_voice(r, f0, 0.37, 0.94, env, [(0.37, "o"), (0.62, "o"), (0.9, "u")], p, drive=0.75, vib_from=0.55)
    else:
        f0 = [["set", 138 * p, 0.12], ["lin", 186 * p, 0.2 + dt], ["lin", 172 * p, 0.36], ["exp", 128 * p, 0.5]]
        env = curve((0.12, 0.0), (0.15, 0.24), (0.36, 0.2), (0.47, 0.06), (0.5, 1e-4))
        breath(r, 0.1, 0.16, curve((0.1, 0.0), (0.13, 0.05), (0.16, 1e-4)), 1500, 1.0)
        _human_voice(r, f0, 0.12, 0.51, env, [(0.12, "uh"), (0.18, "a"), (0.4, "a"), (0.5, "uh")], p, drive=0.9,
                     breathy=0.3)
        breath(r, 0.45, 0.7, curve((0.45, 0.0), (0.49, 0.05), (0.7, 1e-4)), 1100, 0.8)
    return r


@_guarded
def hurt_human(v: int) -> Recipe:
    """A breath caught on the blow, then a short effort sound."""
    p, dt = _variant(v)
    rid = _rid("hurt", "human", v)
    r = Recipe(rid, 0.36, description="Living hurt: breath catch + effort grunt", source=SRC)
    breath(r, 0.0, 0.05, curve((0.0, 0.0), (0.01, 0.09), (0.05, 1e-4)), 1700, 1.0)
    if v % 2 == 0:
        f0 = [["set", 152 * p, 0.03], ["lin", 160 * p, 0.05], ["exp", 108 * p, 0.22]]
        pts = [(0.03, "uh"), (0.22, "schwa")]
    else:
        f0 = [["set", 130 * p, 0.03], ["lin", 138 * p, 0.05 + dt], ["exp", 96 * p, 0.22]]
        pts = [(0.03, "schwa"), (0.12, "U"), (0.22, "u")]
    env = curve((0.03, 0.0), (0.045, 0.24), (0.12, 0.15), (0.2, 0.04), (0.23, 1e-4))
    _human_voice(r, f0, 0.03, 0.24, env, pts, p, drive=0.8, breathy=0.3)
    breath(r, 0.18, 0.35, curve((0.18, 0.0), (0.21, 0.04), (0.35, 1e-4)), 1000, 0.8)
    return r


@_guarded
def death_human(v: int) -> Recipe:
    """v1: a long exhale under a failing moan; v2: the oath, unfinished — 「守……」 breaking into breath."""
    p, dt = _variant(v)
    rid = _rid("death", "human", v)
    r = Recipe(rid, 1.0, description="Living death: failing moan / unfinished oath, last breath", source=SRC)
    if v % 2 == 0:
        _inhale(r, 0.0, 0.08, 0.05)
        f0 = [["set", 134 * p, 0.07], ["lin", 140 * p, 0.12], ["exp", 92 * p, 0.55], ["exp", 74 * p, 0.7]]
        env = curve((0.07, 0.0), (0.11, 0.2), (0.35, 0.14), (0.6, 0.05), (0.7, 1e-4))
        _human_voice(r, f0, 0.07, 0.72, env, [(0.07, "a"), (0.3, "a"), (0.6, "schwa")], p, drive=0.5, breathy=0.45)
    else:
        breath(r, 0.0, 0.11, curve((0.0, 0.0), (0.04, 0.07), (0.11, 1e-4)), 3000, 2.0, hp=1800)
        f0 = [["set", 146 * p, 0.09], ["lin", 150 * p, 0.14 + dt], ["exp", 96 * p, 0.6]]
        env = curve((0.09, 0.0), (0.13, 0.18), (0.3, 0.12), (0.52, 0.03), (0.6, 1e-4))
        _human_voice(r, f0, 0.09, 0.62, env, [(0.09, "o"), (0.35, "o"), (0.6, "u")], p, drive=0.4, breathy=0.6)
    breath(r, 0.5, 1.0, curve((0.5, 0.0), (0.58, 0.05), (0.75, 0.025), (1.0, 1e-4)),
           [["set", 1000, 0.5], ["exp", 600, 1.0]], 0.7)
    return r


# ---------------------------------------------------------------------------------------------------------------------
# Gnawer (the Gravegnawers): port_sfx's goblin set re-pitched lower (glottis x0.66, formants x0.78), wet chitter.
# ---------------------------------------------------------------------------------------------------------------------
GNAWER_PITCH = 0.66
GNAWER_FORMANT = 0.78


def _scale_auto(a, k: float):
    if isinstance(a, (int, float)):
        return a * k
    return [[kind, val * k, t] for kind, val, t in a]


def repitched(base: Recipe, rid: str, pitch: float, formant: float, description: str, length: float | None = None,
              level: float = 1.0) -> Recipe:
    """A copy of `base` with every oscillator frequency scaled by `pitch` and every filter centre by `formant`
    (LFO rates and depths kept: the same growl, a bigger throat)."""
    r = Recipe(rid, length if length is not None else base.length, description=description, source=SRC)
    for lay in copy.deepcopy(base.layers):
        for s in lay["sources"]:
            if s["kind"] == "osc":
                s["freq"] = _scale_auto(s["freq"], pitch)
        for proc in lay["chain"]:
            if proc["type"] == "filter":
                proc["freq"] = _scale_auto(proc["freq"], formant)
            elif proc["type"] == "gain" and level != 1.0 and proc is lay["chain"][-1]:
                proc["gain"] = _scale_auto(proc["gain"], level)
        r.layers.append(lay)
    return r


@_guarded
def aggro_gnawer(v: int) -> Recipe:
    from . import port_sfx

    p, dt = _variant(v)
    rid = _rid("aggro", "gnawer", v)
    rng = _rng(rid)
    r = repitched(port_sfx.aggro_humanoid(v), rid, GNAWER_PITCH * p, GNAWER_FORMANT,
                  "Gravegnawer aggro: the goblin snarl a sixth lower, wet chitter", length=0.7)
    for lay in r.layers:  # the snarl starts after the chatter
        _shift_layer(lay, 0.16)
    chitter(r, rng, 0.0, 0.26 + dt, 48.0, 0.09, pitch=0.9 * p)
    wet(r, rng, 0.12, 0.5, 0.05, n=3)
    return r


@_guarded
def hurt_gnawer(v: int) -> Recipe:
    from . import port_sfx

    p, dt = _variant(v)
    rid = _rid("hurt", "gnawer", v)
    rng = _rng(rid)
    r = repitched(port_sfx.hurt_humanoid(v), rid, GNAWER_PITCH * p, GNAWER_FORMANT,
                  "Gravegnawer hurt: the goblin yelp lower, a wet chitter after", length=0.4)
    chitter(r, rng, 0.17 + dt, 0.17, 55.0, 0.06, pitch=1.0 * p)
    wet(r, rng, 0.02, 0.3, 0.04, n=2)
    return r


@_guarded
def death_gnawer(v: int) -> Recipe:
    from . import port_sfx

    p, dt = _variant(v)
    rid = _rid("death", "gnawer", v)
    rng = _rng(rid)
    r = repitched(port_sfx.death_humanoid(v), rid, GNAWER_PITCH * p, GNAWER_FORMANT,
                  "Gravegnawer death: the goblin wail lower, a choked gurgle, chatter running down", length=0.95)
    wet(r, rng, 0.25, 0.55, 0.06, n=4)
    chitter(r, rng, 0.62 + dt, 0.3, 36.0, 0.05, pitch=0.8 * p)
    thud(r, 0.5 + dt, 0.06, 90, 50, 0.18)
    return r


def _shift_layer(lay: dict, dt: float) -> None:
    """Delay a layer's sources and automation by dt seconds."""

    def sh(a):
        if isinstance(a, list):
            return [[k, v, t + dt] for k, v, t in a]
        return a

    for s in lay["sources"]:
        s["start"] += dt
        s["stop"] += dt
        if s["kind"] == "osc":
            s["freq"] = sh(s["freq"])
            for m in s.get("lfo", []):
                m["start"] += dt
                m["stop"] += dt
    for proc in lay["chain"]:
        if proc["type"] == "filter":
            proc["freq"] = sh(proc["freq"])
        elif proc["type"] == "gain":
            proc["gain"] = sh(proc["gain"])


# ---------------------------------------------------------------------------------------------------------------------
# Breach (Breach-blood): port_sfx's slime set darkened — glottis x0.72, filters x0.6, the cute up-swept pops turned
# into dull down-swept blorps — plus a thick squelch and a faint sizzle.
# ---------------------------------------------------------------------------------------------------------------------
BREACH_PITCH = 0.72
BREACH_FILTER = 0.6


def darkened(base: Recipe, rid: str, description: str, length: float | None = None) -> Recipe:
    r = repitched(base, rid, BREACH_PITCH, BREACH_FILTER, description, length)
    for lay in r.layers:
        for s in lay["sources"]:
            f = s.get("freq")
            # The slime pops: short sines with a rising exponential sweep -> falling, lower, quieter.
            if (s["kind"] == "osc" and s["wave"] == "sine" and s["stop"] - s["start"] <= 0.07 and isinstance(f, list)
                    and len(f) == 2 and f[1][1] > f[0][1]):
                lo, hi = f[0][1], f[1][1]
                s["freq"] = [["set", hi * 0.62, f[0][2]], ["exp", lo * 0.5, f[1][2]]]
                lay["chain"][-1]["gain"] = _scale_auto(lay["chain"][-1]["gain"], 0.6)
    return r


def _sizzle_bed(r: Recipe, rng: random.Random, start: float, stop: float, level: float) -> None:
    """A faint sizzle riding the body: a thin hiss and crackle, both fading out."""
    breath(r, start, stop, curve((start, 0.0), (start + 0.05, level * 0.12), (stop, 1e-4)), 6500, 0.9, hp=4500)
    crackle(r, rng, start, stop, 60.0, level, hp=3800, fade=1.2)


@_guarded
def aggro_breach(v: int) -> Recipe:
    from . import port_sfx

    p, dt = _variant(v)
    rid = _rid("aggro", "breach", v)
    rng = _rng(rid)
    r = darkened(port_sfx.aggro_slime(v), rid, "Breachclot aggro: the slime gurgle darkened, thick squelch, sizzle",
                 length=0.62)
    breath(r, 0.02, 0.4, curve((0.02, 0.0), (0.08, 0.12), (0.25, 0.08), (0.4, 1e-4)),
           [["set", 260 * p, 0.02], ["exp", 520 * p, 0.2], ["exp", 300 * p, 0.4]], 3.0)
    r.layer([osc("sine", [["set", 72 * p, 0.0], ["exp", 52 * p, 0.35]], 0.0, 0.4)],
            [gain(curve((0.0, 0.0), (0.02, 0.05), (0.4, 1e-4)))])
    _sizzle_bed(r, rng, 0.03 + dt, 0.6, 0.012)
    return r


@_guarded
def hurt_breach(v: int) -> Recipe:
    from . import port_sfx

    p, dt = _variant(v)
    rid = _rid("hurt", "breach", v)
    rng = _rng(rid)
    r = darkened(port_sfx.hurt_slime(v), rid, "Breachclot hurt: a thick squelch darkened, a hiss of sizzle",
                 length=0.36)
    breath(r, 0.0, 0.2, curve((0.0, 0.0), (0.005, 0.16), (0.08, 0.08), (0.2, 1e-4)),
           [["set", 340 * p, 0.0], ["exp", 800 * p, 0.1]], 2.6)
    _sizzle_bed(r, rng, 0.02 + dt, 0.34, 0.014)
    return r


@_guarded
def death_breach(v: int) -> Recipe:
    from . import port_sfx

    p, dt = _variant(v)
    rid = _rid("death", "breach", v)
    rng = _rng(rid)
    r = darkened(port_sfx.death_slime(v), rid, "Breachclot death: burst and collapse darkened into tar, sizzling out",
                 length=0.95)
    r.layer([osc("sine", [["set", 64 * p, 0.0], ["exp", 38 * p, 0.5]], 0.0, 0.55)],
            [gain(curve((0.0, 0.0), (0.008, 0.06), (0.55, 1e-4)))])
    breath(r, 0.05, 0.55, curve((0.05, 0.0), (0.1, 0.08), (0.55, 1e-4)), [["set", 500, 0.05], ["exp", 220, 0.55]],
           2.2)
    _sizzle_bed(r, rng, 0.1 + dt, 0.93, 0.016)
    return r


VOCALS = {
    ("aggro", "undead"): aggro_undead, ("hurt", "undead"): hurt_undead, ("death", "undead"): death_undead,
    ("aggro", "human"): aggro_human, ("hurt", "human"): hurt_human, ("death", "human"): death_human,
    ("aggro", "gnawer"): aggro_gnawer, ("hurt", "gnawer"): hurt_gnawer, ("death", "gnawer"): death_gnawer,
    ("aggro", "breach"): aggro_breach, ("hurt", "breach"): hurt_breach, ("death", "breach"): death_breach,
}


# ---------------------------------------------------------------------------------------------------------------------
# The Corpseherd (miniboss_corpseherd): a cracked handbell, a hummed lullaby, a hoarse 「回家吧」, an old woman's gasp.
# ---------------------------------------------------------------------------------------------------------------------
# Small bronze handbell partial ratios (hum, prime, tierce, quint, nominal, upper), amplitudes, decays (s).
HANDBELL = ((0.5, 0.22, 1.3), (1.0, 1.0, 1.1), (1.19, 0.5, 0.85), (1.5, 0.3, 0.7), (2.0, 0.42, 0.6),
            (2.52, 0.22, 0.38), (2.98, 0.16, 0.3), (4.08, 0.09, 0.18))


def _cracked_strike(r: Recipe, rng: random.Random, t: float, f: float, level: float) -> None:
    """One clapper strike on a cracked handbell: every partial paired with a detuned twin (fast, sour beating),
    decays cut short by the crack, a buzzing rattle where the crack chatters."""
    for ratio, a, d in HANDBELL:
        fr = f * ratio
        d *= 0.62
        twin = fr * (1.0 + rng.uniform(0.006, 0.016))
        for ff, aa in ((fr, a), (twin, a * 0.75)):
            if ff < 16000:
                r.layer([osc("sine", [["set", ff, t]], t, t + d + 0.01)], [gain(pluck_env(level * aa, d, t))])
    r.layer([noise(t, t + 0.006)], [filt("highpass", 3000), gain(blip(level * 0.5, t, 0.0003, 0.004))])
    buzz = pulses(rng, t, t + 0.16, (150.0, 110.0), floor=0.0, spread=0.35, peak=(0.4, 1.0), attack=0.0006)
    r.layer([noise(t, t + 0.17)], [filt("bandpass", f * 1.9, 3.0), gain(buzz),
                                   gain(curve((t, 0.0), (t + 0.004, level * 0.35), (t + 0.17, 1e-4)))])


@_guarded
def corpseherd_bell(v: int) -> Recipe:
    """Her cast: the cracked handbell on the crook, shaken (two strikes / three strikes)."""
    rid = _rid("bell", "corpseherd", v)
    rng = _rng(rid)
    r = Recipe(rid, 1.25, description="Corpseherd cast: a cracked bronze handbell, shaken", source=SRC)
    f = 1180.0 * (1.0 if v % 2 == 0 else 0.985)
    strikes = ((0.0, 0.2), (0.19, 0.12)) if v % 2 == 0 else ((0.0, 0.18), (0.15, 0.13), (0.29, 0.08))
    for t, lv in strikes:
        _cracked_strike(r, rng, t, f * rng.uniform(0.997, 1.003), lv)
    return r


# The lullaby (original): D aeolian with a flattened second for unease, 3/4 at 82 bpm, two four-bar phrases, hummed
# once (a one-shot: EvSfx cannot stop a loop). (MIDI, beats); None = rest.
LULLABY_BPM = 82.0
LULLABY_NOTES = (
    (57, 1), (62, 1), (65, 1), (64, 2), (62, 1), (60, 1), (62, 1), (63, 1), (62, 2), (None, 1),
    (65, 1), (67, 1), (65, 1), (63, 1), (62, 1), (60, 1), (57, 2), (60, 1), (62, 2), (None, 1),
)
LULLABY_SEC = 18.0


@_guarded
def corpseherd_lullaby() -> Recipe:
    """Hummed once under her pre-fight lines: an old voice, closed mouth, wavering, a little flat, breath between the
    phrases; both phrases, then silence (17.6 s once trimmed)."""
    rid = "vox_lullaby_corpseherd"
    r = Recipe(rid, LULLABY_SEC, description="Corpseherd: hummed lullaby (original melody), sung once", source=SRC)
    beat = 60.0 / LULLABY_BPM
    t = 0.35
    notes = []
    for midi, beats in LULLABY_NOTES:
        if midi is not None:
            notes.append((t, beats * beat, midi))
        t += beats * beat
    # One continuous glottis per phrase (gliding between notes), so the hum never restarts mid-phrase.
    phrases: list[list] = [[]]
    for i, n in enumerate(notes):
        if i and n[0] - (notes[i - 1][0] + notes[i - 1][1]) > 0.05:
            phrases.append([])
        phrases[-1].append(n)
    for k, ph in enumerate(phrases):
        s = ph[0][0]
        e = ph[-1][0] + ph[-1][1]
        f = []
        for j, (nt, dur, midi) in enumerate(ph):
            hz = 440.0 * 2 ** ((midi - 69) / 12.0) * (0.992 if midi == 63 else 0.997)  # sung a little flat
            f.append(["set" if j == 0 else "lin", hz, nt + (0.0 if j == 0 else 0.07)])
            f.append(["lin", hz * 0.997, nt + dur * 0.85])
        mods = [lfo("detune", "sine", 5.3, 20, s, e + 0.4), lfo("detune", "triangle", 0.31, 14, s, e + 0.4)]
        g = [["set", 1e-4, s]]
        for j, (nt, dur, _) in enumerate(ph):
            g += [["lin", 0.2 if j % 3 == 0 else 0.16, nt + 0.12], ["lin", 0.13, nt + dur * 0.9]]
        g += [["exp", 1e-4, e + 0.35]]
        # Closed-mouth hum: the nasal murmur (F1 ~260 Hz), a weak nasal ring, the rest damped.
        for chain in ([filt("lowpass", 900, 0), filt("bandpass", 265, 1.6), gain(1.0)],
                      [filt("bandpass", 2300, 7), gain(0.05)],
                      [filt("bandpass", 1050, 5), gain(0.05)]):
            r.layer([osc("sawtooth", f, s, e + 0.4, lfo=mods)], chain + [gain(g)])
        # Age: a breathy edge under the hum, and a breath in before each phrase.
        r.layer([noise(s, e + 0.4)], [filt("bandpass", 420, 1.2), gain(scaled(g, 0.12))])
        breath(r, s - 0.33, s - 0.02, curve((s - 0.33, 0.0), (s - 0.08, 0.03), (s - 0.02, 1e-4)),
               [["set", 900, s - 0.33], ["lin", 1400, s - 0.02]], 1.0)
    return r


@_guarded
def corpseherd_home(v: int) -> Recipe:
    """A hoarse 「回家吧」 (hui-jia-ba): whispered formants with a frail, broken voice under them."""
    p, dt = _variant(v)
    rid = _rid("home", "corpseherd", v)
    rng = _rng(rid)
    r = Recipe(rid, 1.55, description="Corpseherd: hoarse near-word 回家吧", source=SRC)
    fs = 1.14  # an old woman's tract
    # hu-i (rising), jia (high level), ba (falling, trailing into breath)
    sy = [
        (0.04, 0.42, [(0.04, "u"), (0.18, "u"), (0.38, "e"), (0.42, "I")], [["set", 175 * p, 0.04], ["lin", 205 * p, 0.42]]),
        (0.55, 0.92, [(0.55, "i"), (0.66, "i"), (0.78, "a"), (0.92, "a")], [["set", 214 * p, 0.55], ["lin", 210 * p, 0.92]]),
        (1.02 + dt, 1.4, [(1.02 + dt, "uh"), (1.1 + dt, "a"), (1.4, "schwa")],
         [["set", 196 * p, 1.02 + dt], ["exp", 150 * p, 1.4]]),
    ]
    breath(r, 0.0, 0.12, curve((0.0, 0.0), (0.05, 0.07), (0.12, 1e-4)), 1100, 0.9)  # h
    breath(r, 0.47, 0.6, curve((0.47, 0.0), (0.53, 0.07), (0.6, 1e-4)), 3600, 2.2, hp=2200)  # j (affricate)
    burst(r, 0.47, 3800, 0.04, 0.012)
    burst(r, 1.0 + dt, 700, 0.05, 0.016)  # b
    for s, e, pts, f0 in sy:
        env = curve((s, 0.0), (s + 0.04, 0.16), (e - 0.12, 0.13), (e, 1e-4))
        # Whisper: noise through the formants carries the word.
        for i, (qq, lv) in enumerate(((4.0, 1.0), (7.0, 0.8), (9.0, 0.35))):
            r.layer([noise(s, e)], [filt("bandpass", track(pts, i, fs), qq), gain(scaled(env, lv * 1.6))])
        # The frail voice under it: weak, jittery, breaking.
        mods = jitter(s, e, 45, rate=23.0 + 4 * rng.random(), slow_cents=30, slow_rate=6.8)
        voiced(r, f0, s, e, scaled(env, 0.3), pts, fs, drive=0.4, curve_name="tanh2", mods=mods)
    breath(r, 1.3, 1.55, curve((1.3, 0.0), (1.36, 0.04), (1.55, 1e-4)), 800, 0.7)
    return r


@_guarded
def corpseherd_hurt(v: int) -> Recipe:
    """An old woman's sharp gasp and a cracked whimper."""
    p, dt = _variant(v)
    rid = _rid("hurt", "corpseherd", v)
    rng = _rng(rid)
    r = Recipe(rid, 0.42, description="Corpseherd hurt: gasp and cracked whimper", source=SRC)
    breath(r, 0.0, 0.09, curve((0.0, 0.0), (0.02, 0.1), (0.09, 1e-4)), [["set", 1400, 0.0], ["lin", 2300, 0.09]], 1.2)
    f0 = [["set", 300 * p, 0.07], ["lin", 340 * p, 0.1 + dt], ["exp", 210 * p, 0.3]]
    env = curve((0.07, 0.0), (0.09, 0.14), (0.2, 0.09), (0.3, 1e-4))
    pts = [(0.07, "ae"), (0.3, "schwa")] if v % 2 == 0 else [(0.07, "e"), (0.18, "I"), (0.3, "i")]
    mods = jitter(0.07, 0.32, 60, rate=19.0 + 3 * rng.random(), slow_cents=25, slow_rate=7.0)
    voiced(r, f0, 0.07, 0.32, env, pts, 1.14, drive=0.5, curve_name="tanh2", mods=mods)
    breath(r, 0.07, 0.32, scaled(env, 0.4), 1800, 1.0)
    breath(r, 0.26, 0.42, curve((0.26, 0.0), (0.29, 0.03), (0.42, 1e-4)), 1000, 0.8)
    return r


# ---------------------------------------------------------------------------------------------------------------------
# Valdras (焚旗元帅): a great helm over a huge chest — f0 55-105 Hz, formants x0.85, two helm cavity resonances, the
# visor muffling everything above ~3 kHz. Plate, the standard, the banner fire.
# ---------------------------------------------------------------------------------------------------------------------
VALDRAS_FS = 0.85
HELM = [filt("lowpass", 3000, 1)]


def _valdras_voice(r: Recipe, rng: random.Random, f0: list, start: float, stop: float, env: list, points: list,
                   drive: float = 0.9, rasp_level: float = 0.55, rasp_rate: tuple[float, float] = (40.0, 34.0),
                   sub: float = 0.35) -> None:
    mods = jitter(start, stop, 26, rate=31.0 + 3 * rng.random(), slow_cents=12, slow_rate=4.4)
    voiced(r, f0, start, stop, env, points, VALDRAS_FS, drive=drive, mods=mods, post=HELM)
    # Helm cavity: the same glottis ringing in the metal.
    for fc, qq, lv in ((690.0, 11.0, 0.28), (1580.0, 13.0, 0.16)):
        r.layer([osc("sawtooth", f0, start, stop, lfo=mods)],
                [gain(drive), shape("tanh3"), filt("bandpass", fc, qq), gain(scaled(env, lv))])
    low = [[k, val * 0.5, t] for k, val, t in f0]
    voiced(r, low, start, stop, scaled(env, sub), points, VALDRAS_FS, drive=1.0, mods=mods, levels=(1.0, 0.0, 0.0),
           post=HELM)
    rasp(r, rng, start, stop, scaled(env, rasp_level), points, VALDRAS_FS, rate=rasp_rate, post=HELM)


def _plate_clank(r: Recipe, rng: random.Random, t: float, level: float, low: bool = False) -> None:
    base = (180.0, 410.0, 690.0, 1130.0, 1720.0) if low else (1110.0, 1890.0, 2630.0, 3710.0)
    parts = [(f * rng.uniform(0.98, 1.02), a, d) for f, a, d in
             zip(base, (1.0, 0.7, 0.55, 0.4, 0.3), (0.42, 0.33, 0.25, 0.18, 0.12) if low else (0.16, 0.12, 0.09, 0.06))]
    ping(r, t, parts, level)
    r.layer([noise(t, t + 0.03)], [filt("bandpass", 2200 if low else 4200, 1.0), gain(pluck_env(level * 0.6, 0.03, t))])


@_guarded
def valdras_order(v: int) -> Recipe:
    """A drill-yard order through the helm: v1 two barked syllables ('hold - fast'), v2 three ('not one step',
    shaped on 不许退). Near-words: the shape of a command, never a word."""
    p, dt = _variant(v)
    rid = _rid("order", "valdras", v)
    rng = _rng(rid)
    r = Recipe(rid, 1.15, description="Valdras: barked order through the great helm", source=SRC)
    if v % 2 == 0:
        breath(r, 0.0, 0.07, curve((0.0, 0.0), (0.04, 0.05), (0.07, 1e-4)), 900, 0.8, post=HELM)
        _valdras_voice(r, rng, [["set", 92, 0.04], ["lin", 102, 0.1], ["exp", 82, 0.36]], 0.04, 0.38,
                       curve((0.04, 0.0), (0.07, 0.25), (0.3, 0.2), (0.37, 1e-4)), [(0.04, "o"), (0.24, "o"), (0.36, "l")])
        burst(r, 0.43, 2400, 0.05, 0.012)
        breath(r, 0.45, 0.52, curve((0.45, 0.0), (0.48, 0.06), (0.52, 1e-4)), 3200, 1.6, post=HELM)
        _valdras_voice(r, rng, [["set", 98, 0.5], ["lin", 104, 0.56 + dt], ["exp", 74, 0.86]], 0.5, 0.88,
                       curve((0.5, 0.0), (0.53, 0.27), (0.75, 0.2), (0.87, 1e-4)), [(0.5, "a"), (0.7, "a"), (0.86, "ae")])
        breath(r, 0.86, 0.96, curve((0.86, 0.0), (0.9, 0.05), (0.96, 1e-4)), 4200, 1.8, post=HELM)
        burst(r, 0.98, 2800, 0.05, 0.012)
    else:
        burst(r, 0.0, 600, 0.06, 0.016)
        _valdras_voice(r, rng, [["set", 96, 0.01], ["exp", 80, 0.2]], 0.01, 0.22,
                       curve((0.01, 0.0), (0.03, 0.25), (0.15, 0.18), (0.21, 1e-4)), [(0.01, "u"), (0.2, "u")])
        breath(r, 0.25, 0.35, curve((0.25, 0.0), (0.29, 0.06), (0.35, 1e-4)), 3900, 1.8, post=HELM)
        _valdras_voice(r, rng, [["set", 86, 0.34], ["lin", 79, 0.42], ["lin", 90, 0.52]], 0.34, 0.54,
                       curve((0.34, 0.0), (0.36, 0.2), (0.47, 0.16), (0.53, 1e-4)), [(0.34, "y"), (0.53, "y")])
        burst(r, 0.6 + dt, 2600, 0.06, 0.012)
        breath(r, 0.6 + dt, 0.68 + dt, curve((0.6 + dt, 0.0), (0.62 + dt, 0.05), (0.68 + dt, 1e-4)), 1800, 1.0, post=HELM)
        _valdras_voice(r, rng, [["set", 104, 0.66 + dt], ["exp", 70, 1.04]], 0.66 + dt, 1.06,
                       curve((0.66 + dt, 0.0), (0.69 + dt, 0.28), (0.9, 0.2), (1.05, 1e-4)),
                       [(0.66 + dt, "u"), (0.78, "e"), (0.98, "i")])
    _plate_clank(r, rng, 0.02, 0.025)
    return r


@_guarded
def valdras_roar(v: int) -> Recipe:
    """The phase-change Roar (1.2 s clip): an open-throated roar through the helm, a breach undertone swelling under
    it, plate shuddering at the start. v1 (vox_valdras_roar: the intro, P2) opens on 'o'; v2 (vox_valdras_roar_p3:
    P3, under the banner fire) opens on 'uh' and closes on 'u'."""
    p, dt = _variant(v)
    rid = _rid("roar", "valdras", v)
    rng = _rng(rid)
    r = Recipe(rid, 1.75, description="Valdras: phase-change roar through the helm", source=SRC)
    f0 = [["set", 58 * p, 0.05], ["lin", 74 * p, 0.3], ["lin", 68 * p, 0.95], ["exp", 50 * p, 1.45]]
    env = curve((0.05, 0.0), (0.2, 0.26), (0.9, 0.24), (1.25, 0.12), (1.48, 1e-4))
    pts = ([(0.05, "o"), (0.3, "a"), (0.95, "a"), (1.4, "o")] if v % 2 == 0
           else [(0.05, "uh"), (0.3, "a"), (0.8, "o"), (1.4, "u")])
    _valdras_voice(r, rng, f0, 0.05, 1.5, env, pts, drive=1.4, rasp_level=1.1, rasp_rate=(34.0, 26.0), sub=0.18)
    # A second throat a fifth up, detuned: the roar is more than one voice.
    voiced(r, [[k, val * 1.498, t] for k, val, t in f0], 0.05, 1.5, scaled(env, 0.25), pts, VALDRAS_FS, drive=1.2,
           mods=jitter(0.05, 1.5, 30, rate=24.0), post=HELM)
    # Breach undertone: two low sines beating (wrong, not musical).
    for f, lv in ((41.0 * p, 0.05), (43.6 * p, 0.035)):
        r.layer([osc("sine", [["set", f, 0.0], ["exp", f * 0.88, 1.7]], 0.0, 1.72)],
                [gain(curve((0.0, 0.0), (0.45, lv), (1.2, lv * 0.8), (1.7, 1e-4)))])
    mail(r, rng, 0.0, 0.3, 50, 0.03, bright=0.75)
    _plate_clank(r, rng, 0.03 + dt, 0.04, low=True)
    return r


@_guarded
def valdras_hurt(v: int) -> Recipe:
    """A grunt inside the helm and a blow ringing on plate."""
    p, dt = _variant(v)
    rid = _rid("hurt", "valdras", v)
    rng = _rng(rid)
    r = Recipe(rid, 0.5, description="Valdras hurt: helm grunt + plate ring", source=SRC)
    f0 = [["set", 84 * p, 0.02], ["lin", 90 * p, 0.05], ["exp", 62 * p, 0.3]]
    env = curve((0.02, 0.0), (0.03, 0.25), (0.14, 0.17), (0.28, 0.04), (0.31, 1e-4))
    _valdras_voice(r, rng, f0, 0.02, 0.32, env, [(0.02, "uh" if v % 2 == 0 else "o"), (0.3, "schwa")], drive=1.1)
    _plate_clank(r, rng, 0.0 + dt, 0.05)
    return r


@_guarded
def valdras_death(_v: int = 0) -> Recipe:
    """The marshal kneels: a long groan falling into fry inside the helm, his knee and sword-point striking the dais,
    a grind of steel on stone, the scorched standard drooping (cloth sliding, a last soft flap), a last breath."""
    rid = "vox_death_valdras_01"
    rng = _rng(rid)
    r = Recipe(rid, 2.6, description="Valdras death: groan into fry, knee on the dais, the standard droops", source=SRC)
    f0 = [["set", 78, 0.03], ["lin", 84, 0.12], ["exp", 56, 0.8], ["exp", 40, 1.25]]
    env = curve((0.03, 0.0), (0.1, 0.24), (0.6, 0.18), (1.05, 0.08), (1.3, 1e-4))
    _valdras_voice(r, rng, f0, 0.03, 1.32, env, [(0.03, "a"), (0.5, "o"), (1.0, "u"), (1.3, "u")], drive=0.9,
                   rasp_level=0.9, rasp_rate=(36.0, 12.0))
    _plate_clank(r, rng, 0.9, 0.08, low=True)
    thud(r, 0.9, 0.2, 62, 36, 0.35)
    # Sword point grinding into stone.
    gp = pulses(rng, 1.12, 1.4, (34.0, 22.0), floor=0.15, spread=0.4)
    for fc, qq, lv in ((1850.0, 14.0, 1.0), (2960.0, 16.0, 0.7), (4410.0, 18.0, 0.45)):
        r.layer([noise(1.12, 1.4)], [filt("bandpass", fc, qq), gain(gp),
                                     gain(curve((1.12, 0.0), (1.15, 0.22 * lv), (1.4, 1e-4)))])
    # The standard drooping: cloth sliding down (slowing flutter), a last soft flap.
    fl = pulses(rng, 1.35, 2.25, (16.0, 5.0), floor=0.25, spread=0.35, peak=(0.5, 1.0), attack=0.01)
    r.layer([noise(1.35, 2.25)], [filt("bandpass", [["set", 900, 1.35], ["exp", 420, 2.25]], 1.3), gain(fl),
                                  gain(curve((1.35, 0.0), (1.5, 0.05), (2.0, 0.03), (2.25, 1e-4)))])
    r.layer([noise(2.12, 2.3)], [filt("lowpass", 520, 1), gain(curve((2.12, 0.0), (2.135, 0.06), (2.3, 1e-4)))])
    breath(r, 1.9, 2.6, curve((1.9, 0.0), (2.0, 0.025), (2.6, 1e-4)), 650, 0.8, post=HELM)
    return r


@_guarded
def valdras_plate(v: int) -> Recipe:
    """Plate grinding as he moves / winds up: stick-slip steel on steel (narrow inharmonic bands chopped by an
    irregular friction pulse train) over a low grind."""
    p, dt = _variant(v)
    rid = _rid("plate", "valdras", v)
    rng = _rng(rid)
    r = Recipe(rid, 0.9, description="Valdras: plate grinding", source=SRC)
    bands = ((1370.0, 1.0), (2210.0, 0.75), (3480.0, 0.5), (4990.0, 0.3)) if v % 2 == 0 else \
        ((1180.0, 1.0), (2050.0, 0.8), (3120.0, 0.55), (5230.0, 0.3))
    env = curve((0.0, 0.0), (0.18, 0.2), (0.55, 0.16), (0.85, 1e-4))
    gp = pulses(rng, 0.0, 0.86, (26.0, 18.0), floor=0.12, spread=0.45, peak=(0.4, 1.0), attack=0.002)
    for fc, lv in bands:
        r.layer([noise(0.0, 0.86)], [filt("bandpass", fc * rng.uniform(0.98, 1.02), 22.0), gain(gp), gain(scaled(env, lv))])
    r.layer([noise(0.0, 0.86)], [filt("lowpass", 420, 2), gain(gp), gain(scaled(env, 0.5))])
    _plate_clank(r, rng, 0.62 + dt, 0.03)
    return r


@_guarded
def valdras_standard(v: int) -> Recipe:
    """The standard cracking in a wind that isn't there: a gust that rises out of nothing and stops dead, cloth
    cracking like a whip, a thin unnatural whistle in it."""
    p, dt = _variant(v)
    rid = _rid("standard", "valdras", v)
    rng = _rng(rid)
    r = Recipe(rid, 1.3, description="Valdras: the standard cracking in a windless gust", source=SRC)
    end = 1.05 + dt
    r.layer([noise(0.0, end)], [filt("bandpass", [["set", 380, 0.0], ["exp", 1150, 0.6], ["exp", 520, end]], 1.3),
                                filt("lowpass", 2600, 0),
                                gain(curve((0.0, 0.0), (0.4, 0.16), (end - 0.12, 0.14), (end, 1e-4)))])
    r.layer([osc("sine", [["set", 1650 * p, 0.15], ["lin", 1730 * p, end]], 0.15, end,
                 lfo=[lfo("detune", "sine", 3.1, 35, 0.15, end)])],
            [gain(curve((0.15, 0.0), (0.5, 0.006), (end - 0.1, 0.005), (end, 1e-4)))])
    cracks = (0.42, 0.63) if v % 2 == 0 else (0.35, 0.52, 0.74)
    for i, t in enumerate(cracks):
        t += rng.uniform(-0.02, 0.02)
        lv = 0.32 * (1.0 if i == 0 else 0.8)
        r.layer([noise(t, t + 0.01)], [filt("bandpass", 2400, 0.7), gain(blip(lv * 0.7, t, 0.0003, 0.007))])
        r.layer([noise(t, t + 0.08)], [filt("lowpass", 420, 1), gain(blip(lv * 0.9, t, 0.002, 0.07))])
        r.layer([osc("sine", [["set", 95, t], ["exp", 55, t + 0.05]], t, t + 0.06)], [gain(pluck_env(lv * 0.4, 0.05, t))])
    return r


@_guarded
def valdras_fire() -> Recipe:
    """P3 No Retreat: the standard ignites with breach fire — a whoomp, a roaring body with crackle, and two low
    tones beating under it (the fire is wrong)."""
    rid = "vox_fire_valdras_01"
    rng = _rng(rid)
    r = Recipe(rid, 2.7, description="Valdras P3: the banner's breach fire roars", source=SRC)
    r.layer([noise(0.0, 0.5)], [filt("lowpass", [["set", 260, 0.0], ["exp", 4200, 0.32], ["exp", 900, 0.5]], 3),
                                gain(curve((0.0, 0.0), (0.12, 0.18), (0.32, 0.14), (0.5, 1e-4)))])
    thud(r, 0.04, 0.16, 68, 36, 0.4)
    am = pulses(rng, 0.25, 2.6, (9.0, 6.0), floor=0.55, spread=0.5, peak=(0.8, 1.0), attack=0.03)
    r.layer([noise(0.25, 2.6)], [filt("lowpass", 1500, 1), gain(am),
                                 gain(curve((0.25, 0.0), (0.5, 0.2), (1.7, 0.17), (2.6, 1e-4)))])
    r.layer([noise(0.25, 2.6)], [filt("bandpass", 240, 1.2), gain(am),
                                 gain(curve((0.25, 0.0), (0.55, 0.16), (1.8, 0.12), (2.6, 1e-4)))])
    crackle(r, rng, 0.3, 2.45, 38.0, 0.12, hp=2800, lp=9000, decay=0.006)
    for f, lv in ((61.7, 0.035), (65.4, 0.03), (123.4, 0.02), (130.9, 0.018)):
        r.layer([osc("sine", [["set", f, 0.2]], 0.2, 2.62)], [gain(curve((0.2, 0.0), (0.9, lv), (1.9, lv), (2.6, 1e-4)))])
    return r


# ---------------------------------------------------------------------------------------------------------------------
# Named cues (render_all.py: `SW_SFX_<PascalCue>_0n`; levels relative to the web `hit` cue's momentary maximum,
# like A7; bus / spatial / concurrency rows for audio_manifest.json `cues`).
# ---------------------------------------------------------------------------------------------------------------------
def _conc(mx: int, retrigger_ms: int) -> dict:
    return {"max": mx, "rule": "stopOldest", "minRetriggerMs": retrigger_ms}


NAMED_CUES = {
    # cue id: (recipe fn(v), variants, LU relative to hit, loop, concurrency, what triggers it)
    "vox_corpseherd_bell": (corpseherd_bell, 2, -4.0, False, _conc(2, 250),
                            "each Corpseherd cast (bell-toll bolt, Call the Flock) and the cs_rf_herd single ring"),
    "vox_corpseherd_lullaby": (lambda v: corpseherd_lullaby(), 1, -12.0, False, _conc(1, 1000),
                               "hummed once (17.6 s, one-shot: EvSfx has no stop) when her pre-fight dialogue opens; "
                               "it ends on its own, under stinger_miniboss if the fight starts first"),
    "vox_corpseherd_home": (corpseherd_home, 2, -5.0, False, _conc(1, 3000),
                            "her hoarse 回家吧: the cs_rf_herd death beat (and a mid-fight bark)"),
    "vox_corpseherd_hurt": (corpseherd_hurt, 2, -5.0, False, _conc(1, 120), "replaces monster_hurt for the Corpseherd"),
    "vox_valdras_order": (valdras_order, 2, -2.0, False, _conc(1, 2500),
                          "his aggro (replaces monster_aggro) and the boss-intro say steps"),
    "vox_valdras_roar": (lambda v: valdras_roar(0), 1, -1.0, False, _conc(1, 1000),
                         "the Roar clip under the intro title and entering P2 (EvMonsterPhase index 1)"),
    "vox_valdras_roar_p3": (lambda v: valdras_roar(1), 1, -1.0, False, _conc(1, 1000),
                            "the Roar clip entering P3 (EvMonsterPhase index 2), with vox_valdras_fire"),
    "vox_valdras_hurt": (valdras_hurt, 2, -5.0, False, _conc(2, 150), "replaces monster_hurt for Valdras"),
    "vox_valdras_death": (valdras_death, 1, -3.0, False, _conc(1, 0),
                          "replaces the monster_death vocal layer for Valdras (cs_rf_marshal_falls follows)"),
    "vox_valdras_plate": (valdras_plate, 2, -10.0, False, _conc(2, 400), "attack wind-ups and every few strides"),
    "vox_valdras_standard": (valdras_standard, 2, -8.0, False, _conc(1, 3000),
                             "the standard snapping: P2 Roll Call musters and idle beats in the Breach Rim"),
    "vox_valdras_fire": (lambda v: valdras_fire(), 1, -4.0, False, _conc(1, 0), "P3 No Retreat: the standard ignites"),
}

# Per-monster replacements of the family cues (UE U7: play the named cue instead of the family's).
NAMED_OVERRIDES = {
    "valdras": {"monster_aggro": "vox_valdras_order", "monster_hurt": "vox_valdras_hurt",
                "monster_death.vocal": "vox_valdras_death"},
    "miniboss_corpseherd": {"monster_aggro": "vox_corpseherd_bell", "monster_hurt": "vox_corpseherd_hurt",
                            "monster_death.vocal": "vox_corpseherd_home"},
}

# ---------------------------------------------------------------------------------------------------------------------
# Planned cues: the design's non-vocal monster sounds (monsters.md 2.1-2.7 "Sound" rows) that no AU package renders yet.
# The ids are reserved for the core (cue enum, the beat that emits them); `wave` is the wave whose behaviour needs the
# sound (the monster's W1 kit, or the W2 system that adds the beat: K3 on-death, K5 packs / block / morale); `owner` is
# the lane that renders them. audio_manifest.json lists them under `plannedCues` (no assets: UE skips a cue it has no
# row for). The family voices (aggro / hurt / death) above cover each monster's throat.
# ---------------------------------------------------------------------------------------------------------------------
PLANNED_OWNER = "audio (the next AU package, after AU1-AU3)"
PLANNED_CUES = {
    # cue id: (monsters, monsters.md sound, wave, spatial)
    "sfx_footman_blade_drag": (["risen_footman"], "a sword scraping the ground (while it walks)", "W1", "3d"),
    "sfx_arbalest_crank": (["risen_arbalest"], "a crossbow crank (reload)", "W1", "3d"),
    "sfx_arbalest_twang": (["risen_arbalest"], "a dry twang (the shot)", "W1", "3d"),
    "sfx_shieldbearer_block": (["risen_shieldbearer"], "a dull clang on block (K5 block)", "W2", "3d"),
    "sfx_shieldbearer_step": (["risen_shieldbearer"], "boots in step (the shield wall advancing, K5 packs)", "W2", "3d"),
    "sfx_standard_bearer_snap": (["dead_standard_bearer"],
                                 "the banner snapping in wind it should not have (vox_valdras_standard's recipe, "
                                 "smaller cloth)", "W2", "3d"),
    "sfx_standard_bearer_horn": (["dead_standard_bearer"], "a low horn note when it raises (the muster, K2)", "W2", "3d"),
    "sfx_zealot_flail": (["oath_zealot"], "a flail's chain whirr (wind-up)", "W1", "3d"),
    "sfx_deserter_flee": (["deserter"], "a hissed 「走！」 when they flee (K5 morale)", "W2", "3d"),
    "sfx_drummer_march": (["deserter_drummer"], "the drummer's march beat (its aura; no W1 spawn)", "W2", "3d"),
    "sfx_gnawer_bite": (["gravegnawer", "bonegorger", "hunt_hungerbell"], "a snapping bite (attack contact)", "W1", "3d"),
    "sfx_gnawer_claws": (["gravegnawer"], "scrabbling claws (moving, corpse-eating)", "W1", "3d"),
    "sfx_bonegorger_breath": (["bonegorger", "hunt_hungerbell"], "wet breathing (idle)", "W1", "3d"),
    "sfx_bonegorger_clapper": (["bonegorger", "hunt_hungerbell"],
                               "a low bell note on every swing (Hungerbell: the clapper of Ashwheat's bell, the "
                               "ASHWHEAT_NOMINAL bell of dark_ambience)", "W1", "3d"),
    "sfx_cinderbloat_wheeze": (["cinderbloat"], "a crackling wheeze while it walks", "W1", "3d"),
    "sfx_cinderbloat_fuse": (["cinderbloat"], "a rising hiss on the fuse (K3 on-death burst, 800 ms)", "W2", "3d"),
    "sfx_cinderbloat_burst": (["cinderbloat"], "a dull whump (the burst)", "W2", "3d"),
    "sfx_false_lantern_mouth": (["false_lantern"], "stretch: the lamprey mouth opening on aggro, the slow bolt",
                                "stretch", "3d"),
}

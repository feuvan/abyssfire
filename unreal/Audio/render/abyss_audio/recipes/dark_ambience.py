"""AU2 (Docs/dark-remake/implementation-plan.md 7): ambience and the bell, in the A7 recipe language.

Loops (Export/Ambience, whole-file loops; render_all.py group `ambience`), see AMBIENCE:

* ``SW_AMB_March``               the march bed (chapter1.md 16 "Wilderness"): a cold low wind with two long lulls (the
                                 silences), crows near and far, a far-off chapel bell. Zone bed of emerald_plains and
                                 of the presentation theme `march`. 60 s.
* ``SW_AMB_GnawedFieldRinging``  the Gnawed Field's night ringing (chapter1.md 5 M2, 8, 16): a clapper still carrying
                                 a jagged lip of Ashwheat's bell, shaken like a supper bell far off; the gnawers
                                 answer. A layer over the bed while `hunt_hungerbell` lives. 40 s.
* ``SW_AMB_BreachRim``           the Breach Rim's region bed (chapter1.md 9.1, 16): wind and a low breach hum only
                                 (D2, the bell's hum: the ley line), breathing slowly. Replaces the bed inside the
                                 region. 48 s.
* ``SW_AMB_AshwheatKnock_0n``    mono emitters at Ashwheat's barred doors (n = 1..3): something knocks behind them
                                 (19 / 23 / 29 s, so neighbouring doors never fall into step). Levelled as one group
                                 (one gain, relative to the web `hit` cue like the bells): the fist door stays the
                                 loudest, just under a combat hit.
* ``SW_AMB_KnellwatchHearth``    mono emitter at Knellwatch's hearth (chapter1.md 16: hearth crackle). 20 s.

Bells (Export/SFX, one-shot cues, stereo unless noted; render_all.py group `bells`), see BELL_CUES:

* ``bell_toll``        the keepers' bell in Knellwatch, one toll: dusk (once per name) and the end of every main
                       turn-in cutscene, heard from inside Knellwatch. A deep minor-third bell (nominal D4, strike
                       note D3), old and cracked: a faint sour buzz under the hum.
* ``bell_toll_far``    the same toll heard from out on the march (mono): darker, wetter, quieter; the dusk tolls while
                       the hero is outside Knellwatch.
* ``bell_cracked``     Ashwheat's split bell in the mud (mono): a dead, sour clank that chatters and stops.
* ``bell_self_ring``   the bell ringing with no hand on it (cs_rf_siege, the siege): the hum blooms out of nothing,
                       the clapper touches, then tolls out of step with any ringer.
* ``bell_dawn_toll``   the finale: the bell sings clean once, the toll sinks down the ley line, a pale shimmer
                       rises, then silence.
* ``bell_ambush_tell`` the ambush tell: a distant toll that sags out of tune while the ground stirs; the stir
                       peaks 1.45 s after the strike (play it 1.5 s before the hands rise). Centred (2d): the hands
                       rise around the hero, so the tell points nowhere.

Bell model: a church bell's partials (hum, prime, tierce, quint, nominal, ... to 6x the nominal), each as a doublet
(two sines 0.5-6 Hz apart: the warble of a real bell) decaying exponentially at its own T60, higher partials faster;
the clapper strike is a noise click and a dull thud; a crack adds a buzz (noise chopped at the prime frequency, the
crack's edges chattering). Distance is a bus low-pass and a darker, wetter reverb. One-shots are cut where the
tail has fallen BELL_TRIM_DB (-60 dB) under the peak (dark_vox.trim_tail): a bell's last 12 dB of reverb noise is
inaudible under any mix and only costs memory.

Loop construction (seams clean by construction): every layer carries a ``bus`` tag and every bus a mode.
``crossfade``: continuous noise beds spanning [0, loop + overlap] (their slow level / cutoff automation is periodic, so
the two sides of the fold match), rendered loop + overlap and the overlap folded into the start with an equal-power
crossfade (loops.crossfade_loop). ``linear``: continuous tones with a whole number of cycles per loop and periodic
automation (coherent: folded with a linear crossfade, an identity when both sides match). ``wrap``: discrete events
anywhere in [0, loop), rendered past their tails and overlap-added onto the start (loops.wrap_tail); a bus low-pass
and reverb then run circularly. One-shots render each bus with its tail kept. Structural randomness (event times,
pulse trains) is seeded by the recipe id, noise by the asset name (sfx.seed_for) and the bus index, reverb impulse
responses by the asset seed: renders are deterministic.

Every sound and every rhythm here is original.
"""

from __future__ import annotations

import contextlib
import math
import random

import numpy as np
from scipy.signal import oaconvolve

from .. import biquad, loops, sfx
from ..sfx import Recipe, filt, gain, lfo, noise, osc, pluck_env, shape
from .dark_vox import _guarded, _rng, blip, chitter, curve, pulses, scaled, trim_tail

SRC = ("unreal/Docs/dark-remake/implementation-plan.md 7 AU2; chapter1.md 2, 5 (M2), 8, 9.1, 10 (cs_rf_siege, "
       "cs_rf_finale), 14 (ambush), 16")

# The keepers' bell (Knellwatch): nominal D4, so the strike note is D3 and the hum D2 (the breach hum shares it).
KNELL_NOMINAL = 293.66
# The far chapel bell of the march bed: smaller and higher (nominal A4), so it is never mistaken for the ambush tell.
CHAPEL_NOMINAL = 440.0
# Ashwheat's split bell: a village bell sitting a quarter-tone under D5 (the split put it out of tune).
ASHWHEAT_NOMINAL = 563.8

# Church-bell partials (minor-third bell), relative to the nominal: (ratio, amplitude, T60 as a fraction of the hum's,
# doublet beat Hz).
BELL_PARTIALS = (
    (0.25, 0.50, 1.00, 0.45),   # hum
    (0.50, 0.70, 0.72, 0.80),   # prime (the strike note)
    (0.60, 0.58, 0.60, 1.30),   # tierce: the minor third, the dark colour
    (0.75, 0.20, 0.30, 1.90),   # quint
    (1.00, 1.00, 0.45, 1.10),   # nominal
    (1.25, 0.15, 0.22, 2.40),
    (1.50, 0.38, 0.24, 1.70),   # superquint
    (2.00, 0.27, 0.17, 2.90),   # octave nominal
    (2.51, 0.15, 0.11, 3.30),
    (2.66, 0.11, 0.09, 3.70),
    (3.01, 0.085, 0.075, 4.10),
    (3.36, 0.065, 0.06, 4.40),
    (4.02, 0.05, 0.045, 4.90),
    (4.55, 0.04, 0.036, 5.30),
    (5.18, 0.03, 0.028, 5.80),
    (6.12, 0.022, 0.02, 6.40),
)


# ---------------------------------------------------------------------------------------------------------------------
# Recipes with buses
# ---------------------------------------------------------------------------------------------------------------------
class AmbRecipe(Recipe):
    """A recipe whose layers carry a bus tag (`layer["bus"]`) and whose buses carry the loop mode and the space
    (`lowpass` Hz, `reverb` {decay, wet, dry, tone, preDelay}). `loop_sec` set = a whole-file loop of that length."""

    def __init__(self, rid: str, length: float, channels: int, description: str, buses: dict,
                 loop_sec: float | None = None, overlap_sec: float = 0.0):
        super().__init__(rid, length, channels=channels, description=description, source=SRC)
        self.buses = buses
        self.loop_sec = loop_sec
        self.overlap_sec = overlap_sec

    @contextlib.contextmanager
    def on(self, bus: str):
        """Layers added inside the block go to `bus`."""
        n0 = len(self.layers)
        yield self
        for lay in self.layers[n0:]:
            lay.setdefault("bus", bus)

    def to_json(self) -> dict:
        d = super().to_json()
        d["buses"] = self.buses
        if self.loop_sec is not None:
            d["loop"] = {"sec": self.loop_sec, "overlapSec": self.overlap_sec}
        return d


def _space(decay: float, wet: float, dry: float = 1.0, tone: float = 4000.0, pre: float = 0.02) -> dict:
    return {"decay": decay, "wet": wet, "dry": dry, "tone": tone, "preDelay": pre}


# ---------------------------------------------------------------------------------------------------------------------
# Periodic automation (loop beds)
# ---------------------------------------------------------------------------------------------------------------------
def periodic(points: list[tuple[float, float]], loop: float, overlap: float) -> list:
    """Linear automation through (time, value) points on [0, loop] (the value at `loop` must equal the value at 0),
    continued periodically to loop + overlap so both sides of a crossfaded seam carry the same automation."""
    pts = sorted(points)
    assert pts[0][0] == 0.0 and abs(pts[-1][0] - loop) < 1e-9 and abs(pts[-1][1] - pts[0][1]) < 1e-9, pts
    out = [["set", pts[0][1], 0.0]] + [["lin", v, t] for t, v in pts[1:]]
    if overlap > 0:
        ts = np.array([p[0] for p in pts])
        vs = np.array([p[1] for p in pts])
        for t, v in pts[1:]:
            if t < overlap:
                out.append(["lin", v, loop + t])
        out.append(["lin", float(np.interp(overlap, ts, vs)), loop + overlap])
    return out


def wander(rng: random.Random, loop: float, overlap: float, lo: float, hi: float, step: float) -> list:
    """A slow random walk (linear ramps every ~`step` s) that closes on itself at `loop`, continued periodically."""
    pts = [(0.0, rng.uniform(lo, hi))]
    t = 0.0
    while True:
        t += step * rng.uniform(0.7, 1.3)
        if t > loop - step * 0.5:
            break
        pts.append((t, rng.uniform(lo, hi)))
    pts.append((loop, pts[0][1]))
    return periodic(pts, loop, overlap)


def cycles(f: float, loop: float) -> float:
    """The nearest frequency with a whole number of cycles per loop (periodic tones on a `linear` bus)."""
    return round(f * loop) / loop


# ---------------------------------------------------------------------------------------------------------------------
# Building blocks
# ---------------------------------------------------------------------------------------------------------------------
def ring_env(peak: float, t60: float, t: float, attack: float = 0.004) -> tuple[list, float]:
    """A struck partial: a fast rise, then an exponential decay of 60 dB per `t60` seconds until 1e-4 (absolute), so
    the decay rate does not depend on the level. Returns (automation, end time)."""
    end = t + attack + t60 * max(0.25, math.log10(peak / 1e-4)) / 3.0
    return [["set", 1e-4, t], ["lin", peak, t + attack], ["exp", 1e-4, end]], end


def bell(r: Recipe, rng: random.Random, t: float, nominal: float, level: float, ring: float, *, force: float = 1.0,
         crack: float = 0.0, sour: float = 0.0, bend_cents: float = 0.0, partials=BELL_PARTIALS,
         max_hz: float = 12000.0, pan: float | None = None) -> None:
    """One clapper strike on a church bell. `ring` = the hum's T60 (s); `force` brightens the upper partials and the
    strike; `crack` adds the crack's buzz; `sour` widens the doublets (a cracked or split bell beats faster and louder);
    `bend_cents` lets every partial sag over its ring (an uncanny, dying toll)."""
    for ratio, amp, dk, beat in partials:
        f = nominal * ratio
        if f > max_hz:
            continue
        a = level * amp * force ** (0.7 * max(0.0, ratio - 1.0))
        if a < 2e-4:
            continue
        t60 = max(0.15, ring * dk)
        b = beat * (1.0 + 2.5 * sour)
        for ff, aa in ((f, a), (f + b, a * (0.5 + 0.35 * sour))):
            env, end = ring_env(aa, t60, t)
            freq = [["set", ff, t]]
            if bend_cents:
                freq.append(["exp", ff * 2.0 ** (bend_cents / 1200.0), end])
            r.layer([osc("sine", freq, t, end + 0.02)], [gain(env)], pan=pan)
    # The clapper: an iron click on bronze and the dull thud of its mass.
    r.layer([noise(t, t + 0.05)], [filt("bandpass", 2400, 0.8), gain(blip(level * 0.32 * force, t, 0.0006, 0.04))],
            pan=pan)
    r.layer([osc("sine", [["set", nominal * 0.42, t], ["exp", nominal * 0.22, t + 0.06]], t, t + 0.1)],
            [gain(curve((t, 0.0), (t + 0.002, level * 0.3 * force), (t + 0.09, 1e-4)))], pan=pan)
    if crack > 0:
        prime = nominal * 0.5
        buzz = pulses(rng, t + 0.003, t + 0.42, (prime, prime * 0.92), floor=0.05, spread=0.3, peak=(0.4, 1.0),
                      attack=0.0006)
        r.layer([noise(t + 0.003, t + 0.44)], [filt("bandpass", nominal * 2.6, 2.5), gain(buzz),
                                       gain(curve((t, 0.0), (t + 0.01, level * crack), (t + 0.42, 1e-4)))], pan=pan)


def caw(r: Recipe, rng: random.Random, t: float, f0: float, level: float, dur: float, pan: float | None = None) -> None:
    """A crow's caw ("kraa"): a hoarse, driven sawtooth through three formant bands, pitch jitter, a noisy rasp; the
    pitch rises into the call and falls off at the end."""
    freq = [["set", f0 * 0.9, t], ["lin", f0, t + 0.045], ["lin", f0 * 0.96, t + dur * 0.7],
            ["lin", f0 * 0.84, t + dur]]
    mods = [lfo("detune", "square", rng.uniform(85, 120), 160, t, t + dur + 0.01),
            lfo("detune", "triangle", rng.uniform(8, 11), 30, t, t + dur + 0.01)]
    env = curve((t, 0.0), (t + 0.025, level), (t + dur * 0.62, level * 0.85), (t + dur, 1e-4))
    grain = pulses(rng, t, t + dur, (f0 * 0.25, f0 * 0.2), floor=0.25, spread=0.4)
    for fc, q, lv in ((1250, 4.0, 1.0), (2050, 5.0, 0.75), (3100, 6.0, 0.35)):
        r.layer([osc("sawtooth", freq, t, t + dur + 0.01, lfo=mods)],
                [gain(3.0), shape("tanh3"), filt("highpass", 900, 0.0),
                 filt("bandpass", fc * rng.uniform(0.97, 1.03), q), filt("lowpass", 4500, 0.0), gain(grain),
                 gain(scaled(env, lv))], pan=pan)
    r.layer([noise(t, t + dur + 0.01)], [filt("bandpass", 1600, 1.6), filt("lowpass", 4000, 0.0), gain(grain),
                                         gain(scaled(env, 0.9))], pan=pan)


def crow_call(r: Recipe, rng: random.Random, t: float, n: int, f0: float, level: float, pan: float) -> float:
    """A series of `n` caws, each a little lower and softer; returns the end time."""
    for i in range(n):
        dur = rng.uniform(0.3, 0.42)
        caw(r, rng, t, f0 * (1.0 - 0.025 * i) * rng.uniform(0.98, 1.02), level * (1.0 - 0.1 * i), dur,
            pan=max(-1.0, min(1.0, pan + rng.uniform(-0.05, 0.05))))
        t += dur + rng.uniform(0.2, 0.36)
    return t


def flaps(r: Recipe, rng: random.Random, t: float, n: int, level: float, pan0: float, pan1: float) -> None:
    """A big bird taking off: wing beats (low whumps and a feather hiss), slowing and moving away across the field."""
    period = 0.13
    for i in range(n):
        k = i / max(1, n - 1)
        lv = level * (1.0 - 0.75 * k)
        pan = pan0 + (pan1 - pan0) * k
        r.layer([noise(t, t + 0.13)], [filt("bandpass", 520 - 180 * k, 0.9), filt("lowpass", 1800 - 900 * k, 0),
                                       gain(curve((t, 0.0), (t + 0.028, lv), (t + 0.12, 1e-4)))], pan=pan)
        r.layer([noise(t, t + 0.07)], [filt("bandpass", 2600, 1.2),
                                       gain(curve((t, 0.0), (t + 0.01, lv * 0.06), (t + 0.06, 1e-4)))], pan=pan)
        t += period * rng.uniform(0.9, 1.1)
        period *= 1.06


def gust(r: Recipe, t: float, dur: float, peak: float, pan: float, sweep=(420.0, 860.0, 520.0),
         q: float = 14.0) -> None:
    """Wind moaning through the ruins: a narrow band of wind that rises and falls in pitch, over a dark whoosh."""
    f0, f1, f2 = sweep
    s = [["set", f0, t], ["exp", f1, t + dur * 0.45], ["exp", f2, t + dur]]
    r.layer([noise(t, t + dur)], [filt("bandpass", s, q), filt("bandpass", s, q),
                                  gain(curve((t, 0.0), (t + dur * 0.45, peak * 3.0), (t + dur, 1e-4)))], pan=pan)
    r.layer([noise(t, t + dur)], [filt("bandpass", [[k, v * 0.55, tt] for k, v, tt in s], 0.9),
                                  filt("lowpass", [[k, v * 1.4, tt] for k, v, tt in s], 0.0),
                                  gain(curve((t, 0.0), (t + dur * 0.5, peak), (t + dur, 1e-4)))], pan=pan * 0.6)


def sparks(rng: random.Random, start: float, stop: float, density: float, level: float, decay: float = 0.004) -> list:
    """Sparse spikes as gain automation (crackle without dc), each a 0.4 ms rise and an exponential fall; silent
    from `stop` - decay."""
    pts = [["set", 0.0, start]]
    t = start + rng.expovariate(density)
    while t < stop - decay * 3.0:
        a = level * rng.uniform(0.2, 1.0) ** 2
        pts += [["set", 0.0, t], ["lin", a, t + 0.0004], ["exp", 1e-4, t + decay * rng.uniform(0.6, 1.6)]]
        t += max(rng.expovariate(density), decay * 2.5)
    pts.append(["set", 0.0, stop - decay])
    return pts


def spark_layers(r: Recipe, rng: random.Random, start: float, stop: float, density: float, level: float,
                 chain: list, post: list | None = None, decay: float = 0.004, pan: float | None = None,
                 chunk: float = 1.0) -> None:
    """Crackle (sparks) over [start, stop] as a row of `chunk`-second noise layers, each silent at its edges (a
    long layer with thousands of spikes is slow to evaluate)."""
    t = start
    while t < stop - 1e-9:
        e = min(stop, t + chunk)
        r.layer([noise(t, e)], list(chain) + [gain(sparks(rng, t, e, density, level, decay))] + list(post or []),
                pan=pan)
        t = e


def thump(r: Recipe, t: float, level: float, f0: float, f1: float, dur: float, lp: float = 400.0) -> None:
    r.layer([osc("sine", [["set", f0, t], ["exp", f1, t + dur * 0.7]], t, t + dur + 0.01)],
            [gain(curve((t, 0.0), (t + 0.003, level), (t + dur, 1e-4)))])
    r.layer([noise(t, t + dur * 0.6 + 0.01)], [filt("lowpass", lp, 1), gain(pluck_env(level * 0.5, dur * 0.6, t))])


# ---------------------------------------------------------------------------------------------------------------------
# Bells (one-shot cues)
# ---------------------------------------------------------------------------------------------------------------------
TOLL_SPACE = _space(2.4, 0.32, tone=3600, pre=0.025)   # the bell frame in the fort's yard


@_guarded
def bell_toll(v: int) -> Recipe:
    """One toll of the keepers' bell: deep, slow-warbling, a faint sour buzz from the crack. Variants differ in the
    clapper's force and how loud the crack chatters."""
    rid = f"bell_toll_{v + 1:02d}"
    rng = _rng(rid)
    force, crack = ((1.0, 0.05), (0.84, 0.04), (0.93, 0.08))[v % 3]
    r = AmbRecipe(rid, 9.0, 2, "Knellwatch bell: one toll (dusk, once per name; end of a main turn-in)",
                  {"bell": {"mode": "oneshot", "reverb": TOLL_SPACE}})
    with r.on("bell"):
        bell(r, rng, 0.0, KNELL_NOMINAL, 0.05, 14.0, force=force, crack=crack, sour=0.15)
    return r


@_guarded
def bell_toll_far(v: int) -> Recipe:
    """The keepers' bell heard from far out on the march (mono): the same deep toll, the crack's buzz lost in the
    distance, the highs gone, more room than bell. Clean pitch and no ground stir: never the ambush tell's sagging
    toll."""
    rid = f"bell_toll_far_{v + 1:02d}"
    rng = _rng(rid)
    force = (0.95, 0.85)[v % 2]
    r = AmbRecipe(rid, 7.0, 1, "Knellwatch bell from far off (dusk tolls while the hero is outside Knellwatch)",
                  {"far": {"mode": "oneshot", "lowpass": 1400.0,
                           "reverb": _space(3.2, 0.8, dry=0.45, tone=2000, pre=0.07)}})
    with r.on("far"):
        bell(r, rng, 0.02, KNELL_NOMINAL, 0.08, 9.0, force=force, crack=0.02, sour=0.12)
    return r


@_guarded
def bell_cracked(v: int) -> Recipe:
    """Ashwheat's split bell, struck where it lies in the mud (mono): every partial split into a sour pair, the ring
    choked within a second, the crack chattering, a dull thud of the mud under it."""
    rid = f"bell_cracked_{v + 1:02d}"
    rng = _rng(rid)
    force = (1.0, 0.7)[v % 2]
    r = AmbRecipe(rid, 1.9, 1, "Ashwheat's split bell: the cracked note", {"bell": {"mode": "oneshot",
                                                                                     "reverb": _space(0.8, 0.12)}})
    with r.on("bell"):
        for ratio, amp, dk, _beat in BELL_PARTIALS[1:12]:
            f = ASHWHEAT_NOMINAL * ratio
            a = 0.06 * amp * force ** (0.7 * max(0.0, ratio - 1.0))
            d = max(0.1, 1.6 * dk)
            split = 1.0 + rng.uniform(0.025, 0.045)
            for ff, aa in ((f, a), (f * split, a * 0.85)):
                r.layer([osc("sine", [["set", ff, 0.0]], 0.0, d + 0.02)], [gain(pluck_env(aa, d, 0.0))])
        r.layer([noise(0.0, 0.04)], [filt("bandpass", 2600, 0.9), gain(blip(0.03 * force, 0.0, 0.0005, 0.03))])
        thump(r, 0.0, 0.05 * force, 150.0, 85.0, 0.12)
        buzz = pulses(rng, 0.004, 0.5, (ASHWHEAT_NOMINAL * 0.25, ASHWHEAT_NOMINAL * 0.2), floor=0.03, spread=0.45,
                      peak=(0.3, 1.0), attack=0.0006)
        r.layer([noise(0.004, 0.52)], [filt("bandpass", 1450, 2.2), gain(buzz),
                                     gain(curve((0.0, 0.0), (0.008, 0.045 * force), (0.5, 1e-4)))])
    return r


@_guarded
def bell_self_ring(v: int) -> Recipe:
    """The bell rings with no hand on it: the hum swells out of nothing (no strike), the clapper barely touches, then
    tolls come out of step with any ringer's swing and fall back to a touch."""
    rid = f"bell_self_ring_{v + 1:02d}"
    rng = _rng(rid)
    r = AmbRecipe(rid, 14.0, 2, "Knellwatch bell rings by itself (cs_rf_siege, the siege)",
                  {"bell": {"mode": "oneshot", "reverb": _space(2.8, 0.4, tone=3200, pre=0.025)}})
    strikes = (((1.9, 0.12, 0.3), (2.75, 0.22, 0.45), (4.1, 0.85, 0.95), (6.35, 1.0, 1.0), (8.6, 0.3, 0.6)),
               ((1.6, 0.1, 0.3), (2.95, 0.5, 0.7), (4.7, 1.0, 1.0), (5.95, 0.4, 0.6), (8.1, 0.9, 0.95)))[v % 2]
    with r.on("bell"):
        # The bloom: hum, prime and tierce rising from silence as if something inside the bronze were singing.
        for ratio, amp, beat in ((0.25, 0.5, 0.45), (0.5, 0.32, 0.8), (0.6, 0.16, 1.3)):
            f = KNELL_NOMINAL * ratio
            for ff, aa in ((f, amp), (f + beat, amp * 0.6)):
                r.layer([osc("sine", [["set", ff, 0.0]], 0.0, 12.0)],
                        [gain(curve((0.0, 0.0), (2.4, 0.03 * aa), (3.4, 0.027 * aa), (11.9, 1e-4)))])
        # A cold breath of air with no wind in it.
        r.layer([noise(0.0, 7.5)], [filt("bandpass", [["set", 380, 0.0], ["exp", 620, 3.0], ["exp", 300, 7.4]], 0.8),
                                    gain(curve((0.0, 0.0), (2.8, 0.012), (7.4, 1e-4)))])
        for t, lv, force in strikes:
            bell(r, rng, t, KNELL_NOMINAL, 0.05 * lv, 8.0, force=force, crack=0.05, sour=0.15)
    return r


@_guarded
def bell_dawn_toll(_v: int = 0) -> Recipe:
    """The finale: the branded palm on the bell, one clean toll (the crack is silent for once); the toll sinks down the
    ley line (the hum glides down a fourth into the ground) while a pale shimmer rises and fades; then silence."""
    rid = "bell_dawn_toll_01"
    rng = _rng(rid)
    r = AmbRecipe(rid, 11.0, 2, "the dawn toll (cs_rf_finale): the toll sinks down the ley line",
                  {"bell": {"mode": "oneshot", "reverb": _space(3.2, 0.38, tone=4200, pre=0.03)},
                   "deep": {"mode": "oneshot"},
                   "air": {"mode": "oneshot", "reverb": _space(3.6, 0.9, dry=0.5, tone=6000, pre=0.04)}})
    with r.on("bell"):
        bell(r, rng, 0.05, KNELL_NOMINAL, 0.055, 15.0, force=1.0)
    with r.on("deep"):
        for f, a in ((73.42, 0.035), (36.71, 0.045), (146.83, 0.014)):
            r.layer([osc("sine", [["set", f, 0.6], ["exp", f * 0.749, 9.0]], 0.6, 10.6)],
                    [gain(curve((0.6, 0.0), (2.2, a), (6.0, a * 0.6), (10.5, 1e-4)))])
        r.layer([noise(0.8, 9.8)], [filt("lowpass", [["set", 170, 0.8], ["exp", 50, 9.5]], 3),
                                    gain(curve((0.8, 0.0), (2.6, 0.12), (9.7, 1e-4)))])
    with r.on("air"):
        for f, a in ((1174.66, 0.010), (1760.0, 0.007), (2349.32, 0.0035)):
            r.layer([osc("sine", [["set", f, 2.4]], 2.4, 10.6, lfo=[lfo("detune", "sine", 0.37, 6, 2.4, 10.6)])],
                    [gain(curve((2.4, 0.0), (5.6, a), (7.4, a * 0.8), (10.5, 1e-4)))])
            r.layer([osc("sine", [["set", f * 1.0035, 2.4]], 2.4, 10.6)],
                    [gain(curve((2.4, 0.0), (5.6, a * 0.6), (7.4, a * 0.5), (10.5, 1e-4)))])
    return r


@_guarded
def bell_ambush_tell(v: int) -> Recipe:
    """A distant toll of the deep bell, sagging out of tune as it dies, while the ground under the hero stirs: a sub
    rumble and crumbling soil that swell to 1.45 s after the strike (the hands rise at 1.5 s), then let go."""
    rid = f"bell_ambush_tell_{v + 1:02d}"
    rng = _rng(rid)
    # Centred: the tell is 2d and the hands rise around the hero (a fixed left / right pan would point at random).
    # The variants differ in the clapper's force and how far the toll sags.
    force, sag = ((0.75, -45.0), (0.7, -38.0))[v % 2]
    r = AmbRecipe(rid, 5.0, 2, "ambush tell: a distant toll 1.5 s before the hands rise",
                  {"far": {"mode": "oneshot", "lowpass": 900.0, "reverb": _space(3.0, 0.9, dry=0.35, tone=1800,
                                                                                   pre=0.06)},
                   "ground": {"mode": "oneshot"}})
    with r.on("far"):
        bell(r, rng, 0.02, KNELL_NOMINAL, 0.09, 6.0, force=force, crack=0.05, sour=0.3, bend_cents=sag)
    with r.on("ground"):
        r.layer([noise(0.25, 2.35)], [filt("lowpass", 95, 4), gain(curve((0.25, 0.0), (1.45, 0.17), (2.3, 1e-4)))])
        r.layer([osc("sine", [["set", 42.0, 0.3], ["exp", 33.0, 2.2]], 0.3, 2.25)],
                [gain(curve((0.3, 0.0), (1.45, 0.06), (2.2, 1e-4)))])
        soil = pulses(rng, 0.6, 2.1, (28.0, 60.0), floor=0.08, spread=0.6, peak=(0.3, 1.0), attack=0.001)
        r.layer([noise(0.6, 2.12)], [filt("bandpass", 650, 1.0), filt("lowpass", 2000, 0.0), gain(soil),
                                     gain(curve((0.6, 0.0), (1.45, 0.03), (2.1, 1e-4)))])
    return r


# ---------------------------------------------------------------------------------------------------------------------
# Loops
# ---------------------------------------------------------------------------------------------------------------------
MARCH_LOOP_SEC = 60.0
MARCH_OVERLAP_SEC = 6.0


@_guarded
def march_bed() -> Recipe:
    """The Burning March by day: a cold, low wind with two long lulls (12.5-29 s, 43-60 s: silence but for the wind's
    breath), crows calling and answering across the field, a far chapel bell, one bird lifting off nearby."""
    rid = "amb_march"
    rng = _rng(rid)
    L, ov = MARCH_LOOP_SEC, MARCH_OVERLAP_SEC
    E = L + ov
    r = AmbRecipe(rid, E, 2, "the march bed: wind, crows, distant bells, silences (60 s loop)",
                  {"bed": {"mode": "crossfade"},
                   "near": {"mode": "wrap", "reverb": _space(1.6, 0.18, tone=3000, pre=0.012)},
                   "far": {"mode": "wrap", "lowpass": 1900.0, "reverb": _space(3.4, 0.85, dry=0.4, tone=2200,
                                                                                 pre=0.035)}},
                  loop_sec=L, overlap_sec=ov)
    lull = periodic([(0.0, 1.0), (12.5, 1.0), (16.5, 0.18), (25.5, 0.2), (29.0, 1.0), (43.0, 0.92), (46.5, 0.14),
                     (56.0, 0.16), (60.0, 1.0)], L, ov)
    soft_lull = periodic([(0.0, 1.0), (12.5, 1.0), (16.5, 0.4), (25.5, 0.42), (29.0, 1.0), (43.0, 0.95), (46.5, 0.32),
                          (56.0, 0.36), (60.0, 1.0)], L, ov)
    with r.on("bed"):
        for pan in (-0.6, 0.6):
            r.layer([noise(0.0, E)], [filt("lowpass", wander(rng, L, ov, 210, 560, 3.2), 0.0),
                                      gain(wander(rng, L, ov, 0.05, 0.095, 4.5)), gain(lull)], pan=pan)
        r.layer([noise(0.0, E)], [filt("lowpass", 120, 2.0), filt("lowpass", 160, 0.0),
                                  gain(wander(rng, L, ov, 0.14, 0.22, 6.0)), gain(soft_lull)])
        for pan in (-0.8, 0.8):
            r.layer([noise(0.0, E)], [filt("highpass", 2200), filt("lowpass", 5000),
                                      gain(wander(rng, L, ov, 0.0008, 0.003, 2.4)), gain(lull)], pan=pan)
    with r.on("near"):
        gust(r, 29.4, 6.0, 0.035, -0.4)
        gust(r, 38.2, 5.2, 0.028, 0.5, sweep=(380.0, 760.0, 470.0))
        gust(r, 57.4, 5.5, 0.02, 0.2, sweep=(400.0, 700.0, 450.0))
        gust(r, 4.0, 4.6, 0.016, 0.55, sweep=(450.0, 820.0, 560.0))
        flaps(r, rng, 30.3, 7, 0.08, 0.3, 0.75)
        crow_call(r, rng, 31.25, 3, 560.0, 0.06, 0.4)
    with r.on("far"):
        crow_call(r, rng, 2.6, 3, 560.0, 0.06, -0.55)
        crow_call(r, rng, 9.8, 2, 600.0, 0.05, 0.5)
        crow_call(r, rng, 34.7, 2, 590.0, 0.055, -0.4)
        crow_call(r, rng, 40.8, 4, 575.0, 0.05, 0.62)
        for t, lv in ((5.4, 1.0), (9.0, 0.8), (37.0, 0.9)):
            bell(r, rng, t, CHAPEL_NOMINAL, 0.022 * lv, 5.5, force=0.7, pan=-0.2)
    return r


BREACH_LOOP_SEC = 48.0
BREACH_OVERLAP_SEC = 6.0


@_guarded
def breach_rim_bed() -> Recipe:
    """The Breach Rim (silence while Valdras lives): a hollow low wind and the breach's hum. The hum sits on D2, the
    keepers' bell's hum (the ley line), with a twin half a hertz above it (a slow throb), a sub-octave, a sawtooth
    body filtered to its low harmonics (so it reads on small speakers), a faint flat second (Eb2) for unease and a
    faint high D4 that drifts against itself; it breathes in irregular 10-13 s swells."""
    rid = "amb_breach_rim"
    rng = _rng(rid)
    L, ov = BREACH_LOOP_SEC, BREACH_OVERLAP_SEC
    E = L + ov
    r = AmbRecipe(rid, E, 2, "Breach Rim region bed: wind and a low breach hum only (48 s loop)",
                  {"bed": {"mode": "crossfade"}, "tone": {"mode": "linear"}}, loop_sec=L, overlap_sec=ov)
    breath = periodic([(0.0, 0.7), (5.5, 1.0), (12.0, 0.72), (18.5, 0.95), (24.0, 0.66), (30.0, 1.0), (36.0, 0.74),
                       (41.5, 0.9), (48.0, 0.7)], L, ov)
    with r.on("bed"):
        for pan in (-0.5, 0.5):
            r.layer([noise(0.0, E)], [filt("lowpass", wander(rng, L, ov, 170, 440, 4.0), 0.0),
                                      gain(wander(rng, L, ov, 0.15, 0.26, 5.0))], pan=pan)
        r.layer([noise(0.0, E)], [filt("bandpass", wander(rng, L, ov, 240, 340, 6.0), 3.0),
                                  gain(wander(rng, L, ov, 0.04, 0.09, 5.0))], pan=-0.15)
    with r.on("tone"):
        d2 = cycles(73.4162, L)
        for f, a, pan in ((d2, 0.028, -0.2), (cycles(d2 + 0.5, L), 0.022, 0.2), (cycles(36.7081, L), 0.035, None)):
            r.layer([osc("sine", [["set", f, 0.0]], 0.0, E)], [gain(a), gain(breath)], pan=pan)
        r.layer([osc("sawtooth", [["set", d2, 0.0]], 0.0, E)],
                [filt("lowpass", periodic([(0.0, 230.0), (12.0, 330.0), (24.0, 210.0), (36.0, 320.0), (48.0, 230.0)],
                                          L, ov), 4.0),
                 gain(0.012), gain(breath)])
        r.layer([osc("sine", [["set", cycles(77.7817, L), 0.0]], 0.0, E)], [gain(0.004), gain(breath)], pan=0.4)
        for f, pan in ((cycles(293.665, L), -0.5), (cycles(293.665 + 1.0 / 12.0, L), 0.5)):
            r.layer([osc("sine", [["set", f, 0.0]], 0.0, E)], [gain(0.0013), gain(breath)], pan=pan)
    return r


RINGING_LOOP_SEC = 40.0
# Fragment of a bell's lip (plate-like, inharmonic): (ratio, amplitude, decay s).
FRAGMENT = ((1.0, 1.0, 0.7), (1.38, 0.6, 0.55), (1.93, 0.75, 0.45), (2.61, 0.45, 0.35), (3.4, 0.35, 0.25),
            (4.37, 0.22, 0.18), (5.2, 0.14, 0.12))


def clang(r: Recipe, rng: random.Random, t: float, f0: float, level: float, pan: float) -> None:
    """The clapper and its jagged lip of bell striking: a bright, short, out-of-tune ring."""
    for ratio, a, d in FRAGMENT:
        f = f0 * ratio * rng.uniform(0.995, 1.005)
        for ff, aa in ((f, a), (f * 1.022, a * 0.6)):
            r.layer([osc("sine", [["set", ff, t]], t, t + d + 0.02)], [gain(pluck_env(level * aa, d, t))], pan=pan)
    r.layer([noise(t, t + 0.03)], [filt("bandpass", 3200, 1.0), gain(blip(level * 0.6, t, 0.0004, 0.022))], pan=pan)


@_guarded
def gnawed_field_ringing() -> Recipe:
    """Far off over the Gnawed Field, something shakes a clapper with a jagged lip of Ashwheat's bell still on it, the
    way a cook rings for supper: uneven bursts, long waits; once or twice a far chitter answers."""
    rid = "amb_gnawed_field_ringing"
    rng = _rng(rid)
    L = RINGING_LOOP_SEC
    r = AmbRecipe(rid, L, 2, "the Gnawed Field's night ringing (layer while hunt_hungerbell lives, 40 s loop)",
                  {"far": {"mode": "wrap", "lowpass": 3000.0, "reverb": _space(3.4, 0.9, dry=0.45, tone=2600,
                                                                                pre=0.04)},
                   "farther": {"mode": "wrap", "lowpass": 1700.0, "reverb": _space(3.0, 1.0, dry=0.25, tone=1800,
                                                                                    pre=0.05)}},
                  loop_sec=L)
    bursts = ((0.8, 6, 5.6), (7.6, 4, 5.0), (15.2, 2, 2.6), (22.4, 7, 6.2), (31.0, 3, 4.2))
    with r.on("far"):
        for t, n, rate in bursts:
            pan = rng.uniform(-0.35, 0.1)
            for i in range(n):
                lv = 0.045 * (1.0 if i % 2 == 0 else 0.72) * rng.uniform(0.75, 1.0) * (1.0 - 0.06 * i)
                clang(r, rng, t, 690.0 * rng.uniform(0.99, 1.01), lv, pan)
                t += rng.uniform(0.85, 1.2) / rate * (1.0 + 0.05 * i)
    with r.on("farther"):
        chitter(r, rng, 10.3, 0.9, 16.0, 0.05, pitch=0.85)
        chitter(r, rng, 26.6, 0.7, 17.0, 0.04, pitch=0.8)
    for lay in r.layers:
        if lay["bus"] == "farther":
            lay["pan"] = 0.6 if lay["sources"][0]["start"] < 20 else -0.65
    return r


HEARTH_LOOP_SEC = 20.0
HEARTH_OVERLAP_SEC = 4.0


@_guarded
def knellwatch_hearth() -> Recipe:
    """Knellwatch's hearth (mono emitter): a low fire's breath, flickering; sparse crackles and pops; one log settling
    into the ash."""
    rid = "amb_knellwatch_hearth"
    rng = _rng(rid)
    L, ov = HEARTH_LOOP_SEC, HEARTH_OVERLAP_SEC
    E = L + ov
    r = AmbRecipe(rid, E, 1, "Knellwatch hearth crackle (mono emitter, 20 s loop)",
                  {"bed": {"mode": "crossfade"}, "near": {"mode": "wrap"}}, loop_sec=L, overlap_sec=ov)
    with r.on("bed"):
        flicker = pulses(rng, 0.0, E, (6.0, 8.0), floor=0.55, spread=0.5, peak=(0.75, 1.0), attack=0.03)
        r.layer([noise(0.0, E)], [filt("lowpass", 420, 1.0), gain(flicker), gain(wander(rng, L, ov, 0.09, 0.14, 3.0))])
        r.layer([noise(0.0, E)], [filt("bandpass", 1100, 0.7), gain(flicker),
                                  gain(wander(rng, L, ov, 0.005, 0.01, 2.5))])
    with r.on("near"):
        spark_layers(r, rng, 0.0, L, 7.0, 0.12, [filt("highpass", 1800), filt("lowpass", 7500)], decay=0.005)
        for _ in range(9):
            t = rng.uniform(0.2, L - 0.3)
            f = rng.uniform(900, 1800)
            lv = rng.uniform(0.03, 0.07)
            r.layer([noise(t, t + 0.03)], [filt("bandpass", f, 5.0), gain(blip(lv, t, 0.0004, 0.025))])
            r.layer([noise(t, t + 0.01)], [filt("highpass", 3000), gain(blip(lv * 0.8, t, 0.0003, 0.006))])
        t = 12.3
        thump(r, t, 0.09, 95.0, 60.0, 0.22, lp=300.0)
        grains = pulses(rng, t + 0.05, t + 1.25, (60.0, 20.0), floor=0.05, spread=0.6, peak=(0.2, 1.0), attack=0.0008)
        r.layer([noise(t + 0.05, t + 1.27)],
                [filt("bandpass", [["set", 2400, t + 0.05], ["exp", 1200, t + 1.25]], 1.4), gain(grains),
                 gain(curve((t + 0.05, 0.0), (t + 0.1, 0.03), (t + 0.6, 0.018), (t + 1.25, 1e-4)))])
    return r


def knock(r: Recipe, rng: random.Random, t: float, level: float, heavy: float = 0.0) -> None:
    """A knock on a heavy barred door: the panel's low mode, the wood's body, a muffled knuckle; a fist (heavy) adds a
    deeper thud, the bar jumping in its iron brackets and the brackets ringing."""
    f = rng.uniform(0.92, 1.08)
    r.layer([osc("sine", [["set", 118 * f, t], ["exp", 86 * f, t + 0.07]], t, t + 0.17)],
            [gain(curve((t, 0.0), (t + 0.0025, level), (t + 0.16, 1e-4)))])
    r.layer([noise(t, t + 0.07)], [filt("bandpass", 330 * f, 3.0), gain(blip(level * 2.2, t, 0.0008, 0.06))])
    r.layer([noise(t, t + 0.045)], [filt("bandpass", 760 * f, 4.0), gain(blip(level * 1.3, t, 0.0006, 0.035))])
    r.layer([noise(t, t + 0.012)], [filt("bandpass", 1900 * f, 1.2), gain(blip(level * 0.25, t, 0.0004, 0.008))])
    if heavy > 0:
        r.layer([osc("sine", [["set", 82 * f, t], ["exp", 52 * f, t + 0.12]], t, t + 0.26)],
                [gain(curve((t, 0.0), (t + 0.003, level * heavy * 1.2), (t + 0.25, 1e-4)))])
        tb = t + 0.012
        for _ in range(rng.randint(2, 3)):
            fb = rng.uniform(1300, 2300)
            lv = level * heavy * 0.5 * rng.uniform(0.5, 1.0)
            r.layer([noise(tb, tb + 0.02)], [filt("bandpass", fb, 6.0), gain(blip(lv, tb, 0.0004, 0.014))])
            tb += rng.uniform(0.018, 0.035)
        for fr, a in ((2150.0, 0.06), (3080.0, 0.035)):
            r.layer([osc("sine", [["set", fr * f, t + 0.012]], t + 0.012, t + 0.14)],
                    [gain(pluck_env(level * heavy * a, 0.12, t + 0.012))])


def scrape(r: Recipe, rng: random.Random, t: float, dur: float, level: float) -> None:
    """A hand dragged slowly down the inside of the door: stick-slip friction, the body leaning on the wood."""
    pp = pulses(rng, t, t + dur, (34.0, 22.0), floor=0.25, spread=0.5, peak=(0.5, 1.0), attack=0.003)
    r.layer([noise(t, t + dur + 0.01)], [filt("bandpass", [["set", 1100, t], ["exp", 700, t + dur]], 1.6), gain(pp),
                                         gain(curve((t, 0.0), (t + 0.15, level), (t + dur * 0.8, level * 0.7),
                                                    (t + dur, 1e-4)))])
    r.layer([noise(t, t + dur + 0.01)], [filt("lowpass", 300, 1.0),
                                         gain(curve((t, 0.0), (t + 0.2, level * 0.6), (t + dur, 1e-4)))])


# (loop s, events): ("k", t, level) a knock, ("p", t, level) a fist that rattles the bar, ("s", t, dur) a scrape.
# The doors are levelled as one group (render_all: one gain), so these levels are their mix: the fist door (02) is
# the loudest, 01's knocks and 03's slower ones sit about 4 LU under it (momentary maximum).
KNOCK_PATTERNS = (
    (19.0, (("k", 0.4, 1.25), ("k", 0.93, 1.125), ("k", 1.47, 1.25), ("k", 8.2, 1.0), ("k", 9.45, 0.9),
            ("s", 13.0, 1.4))),
    (23.0, (("p", 0.6, 1.0), ("p", 1.45, 0.9), ("k", 10.8, 0.35), ("k", 11.13, 0.3), ("k", 11.47, 0.38))),
    (29.0, (("k", 1.0, 0.96), ("k", 2.6, 0.8), ("k", 4.9, 0.88), ("k", 6.15, 0.72), ("k", 17.5, 1.2),
            ("s", 24.0, 1.8))),
)


@_guarded
def ashwheat_knock(v: int) -> Recipe:
    """Something behind one of Ashwheat's barred doors (mono emitter): knocks, a fist that makes the bar jump, small
    taps, a hand dragged down the wood; long waits between. Nothing comes out."""
    rid = f"amb_ashwheat_knock_{v + 1:02d}"
    rng = _rng(rid)
    L, events = KNOCK_PATTERNS[v % len(KNOCK_PATTERNS)]
    r = AmbRecipe(rid, L, 1, f"Ashwheat barred door knocking (mono emitter, {L:g} s loop)",
                  {"near": {"mode": "wrap", "reverb": _space(0.6, 0.22, tone=1500, pre=0.008)}}, loop_sec=L)
    with r.on("near"):
        for kind, t, x in events:
            if kind == "k":
                knock(r, rng, t, 0.2 * x)
            elif kind == "p":
                knock(r, rng, t, 0.24 * x, heavy=x)
            else:
                scrape(r, rng, t, x, 0.05)
    return r


# ---------------------------------------------------------------------------------------------------------------------
# Rendering (bus modes, circular space for loops)
# ---------------------------------------------------------------------------------------------------------------------
def _sub(r: Recipe, bus: str, length: float) -> dict:
    return {"id": f"{r.id}:{bus}", "length": length, "channels": r.channels,
            "layers": [lay for lay in r.layers if lay.get("bus") == bus]}


def _bus_seed(seed: int, i: int) -> int:
    return (seed ^ (0x9E3779B1 * (i + 1))) & 0xFFFFFFFF


def _end(layers: list) -> float:
    return max(s["stop"] for lay in layers for s in lay["sources"])


def impulse_response(space: dict, sr: int, channels: int, seed: int) -> np.ndarray:
    """(channels, n): decorrelated noise with a 12 ms diffuse build-up and an exponential decay (-60 dB at `decay`),
    normalised to unit energy per channel, then darkened by a low-pass at `tone` (unit gain below it), after
    `preDelay` of silence."""
    rng = np.random.Generator(np.random.PCG64(seed))
    decay = float(space["decay"])
    n = int(math.ceil((decay + 0.05) * sr))
    t = np.arange(n) / sr
    env = np.exp(-t * (math.log(1000.0) / decay)) * (1.0 - np.exp(-t / 0.012))
    h = (rng.random((channels, n)) * 2.0 - 1.0) * env
    h /= np.sqrt(np.sum(h * h, axis=1, keepdims=True))
    if space.get("tone"):
        h = biquad.process(h, "lowpass", float(space["tone"]), 0.0, 0.0, sr)
    pre = int(round(float(space.get("preDelay", 0.0)) * sr))
    return np.concatenate([np.zeros((channels, pre)), h], axis=1)


def _convolve(x: np.ndarray, h: np.ndarray) -> np.ndarray:
    return np.stack([oaconvolve(x[c], h[c]) for c in range(x.shape[0])])


def apply_space(x: np.ndarray, spec: dict, sr: int, seed: int, loop: bool) -> np.ndarray:
    """Bus low-pass and reverb. loop=True: both run circularly (the result is periodic with x's length); otherwise
    the reverb tail is appended."""
    n = x.shape[1]
    if spec.get("lowpass"):
        if loop:
            x = biquad.process(np.concatenate([x, x], axis=1), "lowpass", float(spec["lowpass"]), 0.0, 0.0, sr)[:, n:]
        else:
            x = biquad.process(x, "lowpass", float(spec["lowpass"]), 0.0, 0.0, sr)
    rv = spec.get("reverb")
    if not rv:
        return x
    h = impulse_response(rv, sr, x.shape[0], seed)
    wet = _convolve(x, h)
    if loop:
        return float(rv.get("dry", 1.0)) * x + float(rv["wet"]) * loops.wrap_tail(wet, n)
    dry = np.zeros_like(wet)
    dry[:, :n] = x
    return float(rv.get("dry", 1.0)) * dry + float(rv["wet"]) * wet


def _linear_fold(x: np.ndarray, ov: int) -> np.ndarray:
    n = x.shape[1] - ov
    out = x[:, :n].copy()
    if ov:
        w = (np.arange(ov) + 0.5) / ov
        out[:, :ov] = x[:, :ov] * w + x[:, n:] * (1.0 - w)
    return out


def render_loop(r: AmbRecipe, sr: int, seed: int) -> np.ndarray:
    """(channels, loop frames), seamless (see the module docstring)."""
    n = int(round(r.loop_sec * sr))
    ov = int(round(r.overlap_sec * sr))
    out = np.zeros((r.channels, n))
    for i, (bus, spec) in enumerate(r.buses.items()):
        sub = _sub(r, bus, 0.0)
        if not sub["layers"]:
            continue
        mode = spec["mode"]
        if mode in ("crossfade", "linear"):
            sub["length"] = (n + ov) / sr
            x = sfx.render(sub, sr, _bus_seed(seed, i), extra_sec=0.0)[:, :n + ov]
            x = loops.crossfade_loop(x, ov) if mode == "crossfade" else _linear_fold(x, ov)
        elif mode == "wrap":
            sub["length"] = max(_end(sub["layers"]) + 0.1, r.loop_sec)
            x = loops.wrap_tail(sfx.render(sub, sr, _bus_seed(seed, i), extra_sec=0.0), n)
        else:
            raise ValueError(f"{r.id}: bus {bus!r} mode {mode!r} is not a loop mode")
        out += apply_space(x, spec, sr, _bus_seed(seed, 100 + i), loop=True)
    return out


BELL_TRIM_DB = -60.0


def render_oneshot(r: AmbRecipe, sr: int, seed: int, fade_sec: float = 0.08,
                   trim_db: float = BELL_TRIM_DB) -> np.ndarray:
    """(channels, n): every bus rendered to its last source's stop, its space applied with the tail kept; cut where
    the tail has fallen `trim_db` under the peak and faded (cos^2, `fade_sec`; dark_vox.trim_tail)."""
    parts = []
    for i, (bus, spec) in enumerate(r.buses.items()):
        sub = _sub(r, bus, 0.0)
        if not sub["layers"]:
            continue
        sub["length"] = max(_end(sub["layers"]), r.length)
        x = sfx.render(sub, sr, _bus_seed(seed, i), extra_sec=0.05)
        parts.append(apply_space(x, spec, sr, _bus_seed(seed, 100 + i), loop=False))
    total = max(p.shape[1] for p in parts)
    out = np.zeros((r.channels, total))
    for p in parts:
        out[:, :p.shape[1]] += p
    # Cut where the tail has fallen `trim_db` under the peak (reverb noise floor, not audible), then fade.
    return trim_tail(out, sr, trim_db, fade_sec)


# ---------------------------------------------------------------------------------------------------------------------
# Tables (render_all.py, audio_manifest.json)
# ---------------------------------------------------------------------------------------------------------------------
def _conc(mx: int, retrigger_ms: int) -> dict:
    return {"max": mx, "rule": "stopOldest", "minRetriggerMs": retrigger_ms}


# Bell cues -> (recipe fn(v), variants, LU relative to the web `hit` cue (momentary max, like A7), spatial,
# concurrency, when it plays). Assets `SW_SFX_<PascalCue>_0n` (`SW_SFX_<PascalCue>` for one variant).
BELL_CUES = {
    "bell_toll": (bell_toll, 3, -3.0, "2d", _conc(4, 400),
                  "Knellwatch, heard from inside it (the hero within the knellwatch music region, "
                  "musicDirector.regions): one toll per name at dusk (2.8 s apart, the bell's swing), one at the end "
                  "of every main turn-in cutscene (always in Knellwatch); the prologue's ten tolls"),
    "bell_toll_far": (bell_toll_far, 2, -9.0, "2d", _conc(4, 400),
                      "the dusk tolls while the hero is outside the knellwatch region: the same bell from far off "
                      "(mono; chapter1.md 16 Wilderness: distant bells); 2.8 s apart like bell_toll"),
    "bell_cracked": (bell_cracked, 2, -7.0, "3d", _conc(1, 1500),
                     "Ashwheat's split bell (story_rf_belfry, col 55, row 45; emit at that tile): inspecting it; now "
                     "and then from there"),
    "bell_self_ring": (bell_self_ring, 2, -3.0, "2d", _conc(1, 4000),
                       "cs_rf_siege step 2 (the bell rings with no hand on it); during the siege every 20-30 s"),
    "bell_dawn_toll": (bell_dawn_toll, 1, -1.0, "2d", _conc(1, 0),
                       "cs_rf_finale step 4 (the flash after the palm on the bell); then silence, then the lute"),
    "bell_ambush_tell": (bell_ambush_tell, 2, -5.0, "2d", _conc(1, 2000),
                         "the ambush event: play 1.5 s before the hands rise (the ground stir peaks at 1.45 s); "
                         "centred (2d): the hands rise around the hero, so the tell points nowhere"),
}

# Region membership (ambience regions and layers, and musicDirector.regions): the hero's tile (col, row) is inside
# when (col - c)^2 + (row - r)^2 <= radius^2 (Euclidean tile distance, as ExplorationGrid::Reveal) and leaves when it
# exceeds exitRadius (hysteresis); a `while` condition must hold too. Emitter attenuation: full volume within
# `innerTiles` of the emitter, then linear in amplitude to silence at innerTiles + falloffTiles (UE: an attenuation
# with the Linear shape over that distance).
REGION_RULE = ("inside when (col - c)^2 + (row - r)^2 <= radius^2 in tiles (Euclidean, the hero's tile; "
               "ExplorationGrid::Reveal's metric); left when it exceeds exitRadius; `while` must hold too "
               "(monsterAlive: a live instance of that monster id is in the zone)")
EMITTER_ATTENUATION_RULE = ("full volume within innerTiles of the emitter, then linear (amplitude) to silence at "
                            "innerTiles + falloffTiles; UE: attenuation shape Linear over that distance")

# Ambience loops -> (recipe fn(), level, role, manifest row). level: an integrated LUFS target, or a group levelled
# with one gain relative to the web `hit` cue ({"group", "relativeToHitLu"}: the mean momentary maximum of the
# group's assets, like the bells, so the doors keep their spread). Roles: `bed` (a zone / theme bed), `region`
# (replaces the bed inside a map region), `layer` (plays over the bed in an area while a condition holds), `emitter`
# (a point source placed by the set-piece layer: spatial, attenuated with distance).
KNOCK_LEVEL = {"group": "ashwheat_knock", "relativeToHitLu": -4.5}
AMBIENCE = {
    "SW_AMB_March": (march_bed, -30.0, "bed",
                     {"theme": "march", "zones": ["emerald_plains"]}),
    "SW_AMB_BreachRim": (breach_rim_bed, -30.0, "region",
                         {"region": "breach_rim", "zone": "emerald_plains", "mapRegion": "breach_rim",
                          "col": 15, "row": 95, "radius": 16, "exitRadius": 17, "replacesBed": True,
                          "use": "inside the Breach Rim (the map region breach_rim, 16 tiles around the dais at 15,95), "
                                 "always (no `while`: the breach stays open): wind and the breach hum only; the music "
                                 "is silent there while Valdras lives (musicDirector.regions breach_rim)"}),
    "SW_AMB_GnawedFieldRinging": (gnawed_field_ringing, -34.0, "layer",
                                  {"id": "gnawed_field_ringing", "zone": "emerald_plains",
                                   "col": 34, "row": 40, "radius": 15, "exitRadius": 17,
                                   "while": {"monsterAlive": "hunt_hungerbell"},
                                   "untilFlag": "rf_hungerbell_silenced",
                                   "use": "over the bed in the Gnawed Field (the M2 area, 34,40 r15) while "
                                          "hunt_hungerbell lives (W1); from W2 until the flag rf_hungerbell_silenced"}),
    "SW_AMB_AshwheatKnock_01": (lambda: ashwheat_knock(0), KNOCK_LEVEL, "emitter", {"id": "ashwheat_knock"}),
    "SW_AMB_AshwheatKnock_02": (lambda: ashwheat_knock(1), KNOCK_LEVEL, "emitter", {"id": "ashwheat_knock"}),
    "SW_AMB_AshwheatKnock_03": (lambda: ashwheat_knock(2), KNOCK_LEVEL, "emitter", {"id": "ashwheat_knock"}),
    "SW_AMB_KnellwatchHearth": (knellwatch_hearth, -33.0, "emitter", {"id": "knellwatch_hearth"}),
}

EMITTERS = {
    "ashwheat_knock": {"attenuation": {"innerTiles": 2.0, "falloffTiles": 8.0},
                       "placement": {"setPiece": "Ashwheat's barred doors (U4 set pieces)", "zone": "emerald_plains",
                                     "area": {"col": 40, "row": 52, "radius": 8}},
                       "use": "one asset per barred door in Ashwheat (40,52 r8), a different one for each neighbour "
                              "(19 / 23 / 29 s loops); spatial, attenuated (silent beyond 10 tiles)"},
    "knellwatch_hearth": {"attenuation": {"innerTiles": 1.5, "falloffTiles": 6.5},
                          "placement": {"campSlot": "campfire", "camp": 1, "zone": "emerald_plains",
                                        "anchor": {"col": 15, "row": 15}},
                          "use": "Knellwatch's hearth (camp 1's campfire slot, offset 0,0: tile 15,15); spatial, "
                                 "attenuated (silent beyond 8 tiles)"},
}

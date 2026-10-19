"""AU3 (Docs/dark-remake/implementation-plan.md 7): the dark remake's music, read from render/data/dark_scores.json.

The scores are data (dark_scores.json, loaded beside port_scores.json); this module is their interpreter: a small
score notation, the instruments that play it, and the director table (which track or silence plays where, which
stinger plays when) that render_all.py copies into audio_manifest.json for the core / UE lanes.

Tracks (Export/Music, stereo; loops are whole-file) and stingers (Export/Stingers, one-shots), all original:

* ``knellwatch_lute``  Knellwatch (chapter1.md 16): a sparse, slightly detuned plucked lute of our own, never a
                       12-string town theme: a single line in D minor with the bell's minor third (D-F) and the
                       breach's flat second (Eb), long rests, a low course left ringing.
* ``march_drones``     the wilderness: low drones in D (the bell's hum, D2) swelling out of silence and sinking back,
                       a bowed fifth, a faint flat second for unease, a glass shimmer; long silences between, so the
                       march bed (AU2: wind, crows, distant bells) is heard on its own.
* ``red_field``        the Red Field (south-west): a slow dead drum and a wordless men's choir (i - bII - VI - v).
* ``boss_ch1``         reworked for Valdras: war drums in a marching ostinato, a field snare, low horns grinding
                       D - Eb - D - C, a chanting men's choir, tremolo strings, the cracked bell once per 16 bars.
* ``siege``            the siege: drums from the field, an army out of step, a far horn; no melody.
* ``valdras_falls``    replaces the boss victory recording: one dead drum, the choir's last chord sinking as the
                       standard droops, a distant toll, silence (a one-shot within the 8 s boss-victory hold).
* stingers: hunt and mini-boss engaged, hunt / mini-boss slain, Valdras's phase changes (P2 Roll Call: a snare roll
  under his 1.2 s Roar into the muster; P3 No Retreat: the standard ignites), and the night chapter card.

Notation (``phrases[].notes``): tokens ``ITEM[:DUR][!VEL[>VEL2]]`` separated by spaces; ``|`` is a bar line (checked:
it must fall on a bar boundary). ITEM is ``r`` (rest), a pitch (``D4``, ``Eb2``, ``F#3``; middle C = C4), a chord
(``D2+A2+F3``, struck low to high), or a hit for unpitched instruments: ``X`` accent, ``x`` normal, ``o`` ghost, ``R``
a roll lasting DUR (``!0.2>1`` swells from 0.2 to 1). DUR is in beats (default 1; ``2/3`` is allowed). A phrase is
``{"bar": 1-based bar, "beat": offset in beats, "notes": "...", "repeat": n, "vel": factor, ...overrides}``; the
overrides (``vowel``, ``pan``) reach the instrument.

Instruments (layers in the A7 recipe language, so the recipe dump stays engine-agnostic): ``lute`` (additive plucked
courses: two strings per course a few cents apart, partials shaped by the pluck position and a body response, higher
partials dying faster, a pluck click and the body's knock), ``choir`` (sawtooth singers through vowel formants,
each singer detuned with its own vibrato, breath), ``drum`` (a slack membrane: a pitch-dropping fundamental, two
membrane modes, the beater, the head's slap, the skin; `drive` clips only the attack of war drums), ``snare``
(shell, head, wires; rolls), ``brass`` (softly driven sawtooth pairs, the low-pass opening with the attack),
``strings`` (three detuned saws, optional tremolo), ``drone`` (bowed saw pair through a wandering low-pass, a sub, a
breath of pitched noise), ``glass`` (two sines beating slowly) and ``bell`` (dark_ambience.bell: the keepers'
minor-third bell). Every envelope that decays ends with a 4 ms ramp to exact zero before its sources stop (no
clicks on quiet notes); the mix gets a 5 Hz DC blocker (circular for loops).

Every bus is ``wrap`` (loops: events anywhere in [0, loop), tails overlap-added onto the start, reverb circular) or
``oneshot`` (stingers, valdras_falls), rendered by dark_ambience.render_loop / render_oneshot. Structural randomness
(humanised timing and velocity, drum tuning jitter, choir entries) is seeded by the track id, noise by the asset name:
renders are deterministic. The music does not use the web's per-theme effects chain: each bus has its own space.
"""

from __future__ import annotations

import json
import math
import pathlib
import random
import re
import zlib
from dataclasses import dataclass, field
from fractions import Fraction

import numpy as np

from scipy.signal import lfilter

from ..composer import midi_to_hz
from ..sfx import filt, gain, lfo, noise, osc, shape
from .dark_ambience import AmbRecipe, bell, render_loop, render_oneshot
from .dark_vox import VOWELS, _guarded, blip, curve, pulses, scaled

SRC = ("unreal/Docs/dark-remake/implementation-plan.md 7 AU3; chapter1.md 2, 9.1, 10, 11, 16; story-bible.md "
       "(tone; no 12-string town theme, no imitation of any known track)")
DATA = pathlib.Path(__file__).resolve().parents[2] / "data" / "dark_scores.json"

PITCH_CLASSES = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
NAMES = ("C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B")
HITS = {"X": 1.0, "x": 0.72, "o": 0.42}
PITCH_RE = re.compile(r"^([A-G])(#|b)?(-?\d)$")
TOKEN_RE = re.compile(r"^([^:!]+)(?::([0-9./]+))?(?:!([0-9.]+)(?:>([0-9.]+))?)?$")


def load(path: pathlib.Path = DATA) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


# ---------------------------------------------------------------------------------------------------------------------
# Notation
# ---------------------------------------------------------------------------------------------------------------------
def pitch_midi(name: str) -> int:
    m = PITCH_RE.match(name)
    if not m:
        raise ValueError(f"bad pitch {name!r}")
    pc = PITCH_CLASSES[m.group(1)] + {"#": 1, "b": -1, None: 0}[m.group(2)]
    return 12 * (int(m.group(3)) + 1) + pc


def pitch_class_name(midi: int) -> str:
    return NAMES[midi % 12]


@dataclass
class Token:
    beat: float            # from the phrase start
    dur: float             # beats
    item: str              # "r", "X" / "x" / "o" / "R", or a pitch / chord
    vel: float
    vel_to: float | None = None

    @property
    def pitches(self) -> list[int]:
        return sorted(pitch_midi(p) for p in self.item.split("+"))


def parse_notes(text: str, beats_per_bar: int, offset: float = 0.0, default_vel: float = 0.8) -> tuple[list[Token],
                                                                                                         float]:
    """Tokens of one phrase and its length in beats. A bar line must fall on a bar boundary (offset = the phrase's
    position inside its first bar)."""
    out: list[Token] = []
    pos = Fraction(0)
    off = Fraction(offset).limit_denominator(96)
    for raw in text.split():
        if raw == "|":
            if (off + pos) % beats_per_bar != 0:
                raise ValueError(f"bar line at beat {float(off + pos)} is not on a bar boundary in {text!r}")
            continue
        m = TOKEN_RE.match(raw)
        if not m:
            raise ValueError(f"bad token {raw!r}")
        item, dur_s, vel_s, vel2_s = m.groups()
        dur = Fraction(dur_s).limit_denominator(96) if dur_s else Fraction(1)
        if dur <= 0:
            raise ValueError(f"non-positive duration in {raw!r}")
        if item not in ("r", "R") and item not in HITS:
            for p in item.split("+"):
                pitch_midi(p)
        vel = float(vel_s) if vel_s else default_vel
        if item != "r":
            out.append(Token(float(pos), float(dur), item, vel, float(vel2_s) if vel2_s else None))
        pos += dur
    return out, float(pos)


@dataclass
class Event:
    t: float               # seconds
    dur: float             # seconds (written length)
    item: str
    vel: float
    vel_to: float | None
    extra: dict = field(default_factory=dict)

    @property
    def pitches(self) -> list[int]:
        return sorted(pitch_midi(p) for p in self.item.split("+"))


PHRASE_KEYS = {"bar", "beat", "notes", "repeat", "vel", "label"}


def part_events(part: dict, tempo: float, beats_per_bar: int, rng: random.Random, total_beats: float | None,
                loop_sec: float | None) -> list[Event]:
    """Every event of a part in seconds. Humanise: timing (+-ms, uniform) and velocity (+-, uniform). Loops wrap an
    event pushed before 0 to the end (its tail then wraps onto the start)."""
    beat_sec = 60.0 / tempo
    hum = part.get("humanize", {})
    jt = float(hum.get("timingMs", 0.0)) / 1000.0
    jv = float(hum.get("velocity", 0.0))
    out = []
    for ph in part["phrases"]:
        start = (int(ph["bar"]) - 1) * beats_per_bar + float(ph.get("beat", 0.0))
        tokens, length = parse_notes(ph["notes"], beats_per_bar, float(ph.get("beat", 0.0)),
                                     float(part.get("defaultVel", 0.8)))
        extra = {k: v for k, v in ph.items() if k not in PHRASE_KEYS}
        for rep in range(int(ph.get("repeat", 1))):
            for tok in tokens:
                b = start + rep * length + tok.beat
                if total_beats is not None and not (0.0 <= b < total_beats - 1e-9):
                    raise ValueError(f"{part['id']}: event at beat {b} outside the track (0..{total_beats})")
                t = b * beat_sec + (rng.uniform(-jt, jt) if jt else 0.0)
                vel = tok.vel * float(ph.get("vel", 1.0)) * (1.0 + (rng.uniform(-jv, jv) if jv else 0.0))
                vel_to = tok.vel_to * float(ph.get("vel", 1.0)) if tok.vel_to is not None else None
                if loop_sec is not None:
                    t %= loop_sec
                else:
                    t = max(0.0, t)
                out.append(Event(t, tok.dur * beat_sec, tok.item, max(0.02, min(1.2, vel)), vel_to, extra))
    return out


# ---------------------------------------------------------------------------------------------------------------------
# Instruments: fn(r, rng, ev, level, pan, params). Params are the part's `params` over DEFAULTS[instrument].
# ---------------------------------------------------------------------------------------------------------------------
def _tuned(midi: int, params: dict) -> float:
    return midi_to_hz(midi) * 2.0 ** (float(params.get("tuningCents", {}).get(pitch_class_name(midi), 0.0)) / 1200.0)


def _ring(peak: float, t60: float, t: float, attack: float, damp_at: float | None = None,
          damp_sec: float = 0.12) -> tuple[list, float]:
    """A struck / plucked partial: rise in `attack`, decay 60 dB per `t60` s to 1e-4; stopped at `damp_at` (the
    finger / the damper): from there it falls to 1e-4 in `damp_sec`. Returns (automation, end)."""
    natural = t + attack + t60 * max(0.2, math.log10(max(peak, 2e-4) / 1e-4)) / 3.0
    env = [["set", 1e-4, t], ["lin", peak, t + attack]]
    if damp_at is None or damp_at >= natural - damp_sec:
        return env + [["exp", 1e-4, natural]], natural
    damp_at = max(damp_at, t + attack + 0.01)
    v = peak * 10.0 ** (-3.0 * (damp_at - t - attack) / t60)
    return env + [["exp", max(v, 1.5e-4), damp_at], ["exp", 1e-4, damp_at + damp_sec]], damp_at + damp_sec


def _bump_db(f: float, fc: float, db: float, width_oct: float) -> float:
    return db * math.exp(-0.5 * (math.log2(f / fc) / width_oct) ** 2)


def lute_body_db(f: float) -> float:
    """The lute's bowl: the air resonance (~125 Hz), the soundboard (~270, ~540 Hz), a presence bump, a roll-off
    above 2.8 kHz and below 90 Hz."""
    d = (_bump_db(f, 125.0, 6.0, 0.35) + _bump_db(f, 270.0, 4.5, 0.4) + _bump_db(f, 540.0, 2.5, 0.5)
         + _bump_db(f, 1150.0, 1.5, 0.6))
    if f > 2800.0:
        d -= 9.0 * math.log2(f / 2800.0)
    if f < 90.0:
        d -= 6.0 * math.log2(90.0 / f)
    return d


def lute(r, rng, ev: Event, level: float, pan: float, p: dict) -> None:
    """Plucked courses (two gut strings a few cents apart): additive partials f_n = n f0 sqrt(1 + B n^2) with the
    pluck-position comb |sin(n pi pos)| / n^1.15, the body response, brightness rising with velocity; each partial
    decays at its own rate and stops `ringSec` after the written length (the next note on that course, or the
    finger). A chord is struck low to high (`strumSec`)."""
    detune = float(p["courseDetuneCents"])
    pos = float(p["pluckPosition"])
    inharm = float(p["inharmonicity"])
    for k, m in enumerate(ev.pitches):
        t = ev.t + k * float(p["strumSec"])
        vel = ev.vel * (1.0 - 0.12 * k)
        f0 = _tuned(m, p)
        lv = level * vel
        damp = t + ev.dur + float(p["ringSec"])
        t60_1 = float(p["sustainSec"]) * (147.0 / f0) ** 0.4
        n_max = max(3, min(int(p["partials"]), int(float(p["maxHz"]) / f0)))
        bright = 0.55 + 0.45 * min(1.0, vel)
        glide = 2.0 ** ((3.0 + 4.0 * vel) / 1200.0)
        for n in range(1, n_max + 1):
            fn = n * f0 * math.sqrt(1.0 + inharm * n * n)
            # The partials of a real pluck are not phase-aligned: each starts up to 1.2 ms late (inaudible as
            # timing; it keeps the onset from summing into one sharp peak).
            tn = t + (rng.uniform(0.0, 0.0012) if n > 1 else 0.0)
            a = lv * abs(math.sin(math.pi * n * pos)) / n ** 1.15 * 10.0 ** (lute_body_db(fn) / 20.0)
            a *= bright ** (0.3 * (n - 1))
            if a < max(lv * 0.004, 3e-4):  # under -70 dB: not worth a layer (and its envelope would not decay)
                continue
            t60 = max(0.1, t60_1 / (1.0 + 0.35 * (n - 1)))
            for cents, amp, dk, dp in ((-detune / 2, 1.0, 1.0, -0.07), (detune / 2, 0.82, 0.88, 0.07)):
                fs = fn * 2.0 ** (cents / 1200.0)
                env, end = _ring(a * amp, t60 * dk, tn, 0.0022, damp)
                r.layer([osc("sine", [["set", fs * glide, tn], ["exp", fs, tn + 0.11]], tn, end + 0.01)],
                        [gain(env)], pan=max(-1.0, min(1.0, pan + dp)))
        # The flesh-and-nail click and the bowl's knock.
        r.layer([noise(t, t + 0.03)], [filt("highpass", 1200), filt("bandpass", 2900, 0.8),
                                       gain(blip(lv * float(p["click"]), t, 0.0006, 0.014))], pan=pan)
        r.layer([noise(t, t + 0.2)], [filt("bandpass", 128, 5.0),
                                      gain(blip(lv * float(p["knock"]), t, 0.002, 0.15))], pan=pan)


def _formant_track(points: list[tuple[float, str]], i: int, fs: float) -> list:
    return [["set" if k == 0 else "lin", VOWELS[v][i] * fs, t] for k, (t, v) in enumerate(points)]


FORMANT_Q = (5.0, 8.0, 10.0)
FORMANT_LEVEL = (1.0, 0.5, 0.22)
# The formant band-passes keep a small part of the sawtooth's energy: the make-up puts a choir `level` on the same
# footing as the other instruments' (a chord at level L reads about as loud as horns at level L).
CHOIR_MAKEUP = 3.0


def choir(r, rng, ev: Event, level: float, pan: float, p: dict) -> None:
    """Wordless singers: per note `voices` sawtooth singers (detuned across `spreadCents`, entries spread over
    `entrySec`, each with its own vibrato rate and a slow drift) through a source low-pass and three vowel formants;
    the vowel can morph over the note ("u>o>a"); breath through the second formant. The swell rises in `attackSec`,
    crests at 55 % of the note and releases in `releaseSec`."""
    voices = int(p["voices"])
    fs = float(p["formantScale"])
    vowels = str(ev.extra.get("vowel", p["vowel"])).split(">")
    width = float(p["width"])
    for m in ev.pitches:
        f0 = _tuned(m, p)
        boost = (220.0 / f0) ** float(p["lowBoost"]) if f0 < 220.0 else 1.0
        for v in range(voices):
            spread = (v - (voices - 1) / 2.0) / max(1.0, (voices - 1) / 2.0)
            cents = spread * float(p["spreadCents"]) + rng.uniform(-2.0, 2.0)
            f = f0 * 2.0 ** (cents / 1200.0)
            t = ev.t + rng.uniform(0.0, float(p["entrySec"]))
            d = max(0.05, ev.dur - (t - ev.t))
            att = min(float(p["attackSec"]), d * 0.8)
            rel = float(p["releaseSec"])
            end = t + d + rel
            lv = level * ev.vel * boost * CHOIR_MAKEUP / math.sqrt(voices)
            crest = t + max(att, d * 0.55)
            env = curve((t, 0.0), (t + att, lv * 0.82), (crest, lv), (t + d, lv * 0.78), (end, 1e-4))
            if ev.vel_to is not None:  # a swell to vel_to over the note (stingers)
                k = ev.vel_to / max(ev.vel, 1e-3)
                env = curve((t, 0.0), (t + att, lv * 0.6), (t + d, lv * k), (end, 1e-4))
            vib_rate = rng.uniform(*p["vibratoHz"])
            vib = float(p["vibratoCents"])
            mods = [lfo("detune", "sine", vib_rate, [["set", 0.0, t], ["lin", vib, t + min(0.9, d * 0.5)]], t, end),
                    lfo("detune", "triangle", rng.uniform(0.13, 0.31), float(p["driftCents"]), t, end)]
            pts = [(t + d * i / max(1, len(vowels) - 1), vw) for i, vw in enumerate(vowels)] if len(vowels) > 1 \
                else [(t, vowels[0])]
            vpan = max(-1.0, min(1.0, pan + spread * width))
            for i, (q, flv) in enumerate(zip(FORMANT_Q, FORMANT_LEVEL)):
                r.layer([osc("sawtooth", [["set", f, t]], t, end + 0.01, lfo=mods)],
                        [filt("lowpass", float(p["sourceLowpassHz"]) * fs, 0.0),
                         filt("bandpass", _formant_track(pts, i, fs), q), gain(scaled(env, flv))], pan=vpan)
            if float(p["breath"]) > 0:
                r.layer([noise(t, end + 0.01)], [filt("highpass", 900), filt("bandpass", _formant_track(pts, 1, fs),
                                                                               1.5),
                                                 gain(scaled(env, float(p["breath"])))], pan=vpan)


def drum(r, rng, ev: Event, level: float, pan: float, p: dict) -> None:
    """A slack membrane hit with a felt beater: the fundamental dropping from `dropCents` above `tuneHz` (a slack
    head), the membrane's inharmonic modes (`modes`: ratio, level, decay factor), the beater's thud (low-passed
    noise), the head's slap (what a small speaker hears) and the skin (band-passed noise); `drive` (0-1: the shaper's
    input peak) clips the attack of the fundamental (war drums) and lets the decay ring clean. A roll (R) repeats
    hits at `rollHz`, swelling from VEL to VEL2."""
    if ev.item == "R":
        _roll(r, rng, ev, level, pan, p, drum_hit)
        return
    drum_hit(r, rng, ev.t, level * ev.vel * HITS[ev.item], pan, p)


def drum_hit(r, rng, t: float, lv: float, pan: float, p: dict) -> None:
    f0 = float(p["tuneHz"]) * 2.0 ** (rng.uniform(-1.0, 1.0) * float(p["jitterCents"]) / 1200.0)
    decay = float(p["decaySec"])
    drop = 2.0 ** (float(p["dropCents"]) / 1200.0)
    env, end = _ring(lv, decay, t, 0.0025)
    k = float(p["drive"])
    # Drive: the shaper sees the decaying hit (peak `drive`), so only the attack saturates; the level is restored.
    chain = [gain(scaled(env, k / lv)), shape("tanh3"), gain(lv / math.tanh(3.0 * k))] if k > 0 else [gain(env)]
    r.layer([osc("sine", [["set", f0 * drop, t], ["exp", f0, t + float(p["dropSec"])],
                          ["exp", f0 * 0.97, end]], t, end + 0.01)], chain, pan=pan)
    for ratio, a, dk in p["modes"]:
        env2, end2 = _ring(lv * a, decay * dk, t, 0.002)
        r.layer([osc("sine", [["set", f0 * ratio * drop ** 0.5, t], ["exp", f0 * ratio, t + float(p["dropSec"])]],
                     t, end2 + 0.01)], [gain(env2)], pan=pan)
    r.layer([noise(t, t + 0.09)], [filt("lowpass", float(p["beaterHz"]), 2.0),
                                   gain(blip(lv * float(p["beater"]), t, 0.001, 0.06))], pan=pan)
    # The slap of the head under the beater: what carries the hit on small speakers.
    r.layer([noise(t, t + 0.04)], [filt("bandpass", 1700.0, 0.8),
                                   gain(blip(lv * float(p["slap"]), t, 0.0006, 0.025))], pan=pan)
    sk = float(p["skin"])
    if sk > 0:
        sd = decay * 0.35
        r.layer([noise(t, t + sd + 0.01)], [filt("bandpass", float(p["skinHz"]), 1.4),
                                           gain(blip(lv * sk, t, 0.0015, sd))], pan=pan)


def snare(r, rng, ev: Event, level: float, pan: float, p: dict) -> None:
    """A field side drum: the shell's tone, the head's mode, the snare wires (high band noise) and the stick click.
    A roll (R) is a stream of alternating-hand strokes at `rollHz` swelling from VEL to VEL2."""
    if ev.item == "R":
        _roll(r, rng, ev, level, pan, p, snare_hit)
        return
    snare_hit(r, rng, ev.t, level * ev.vel * HITS[ev.item], pan, p)


def snare_hit(r, rng, t: float, lv: float, pan: float, p: dict) -> None:
    decay = float(p["decaySec"])
    f = float(p["shellHz"]) * rng.uniform(0.985, 1.015)
    r.layer([osc("sine", [["set", f * 1.12, t], ["exp", f, t + 0.02]], t, t + decay * 0.8)],
            [gain(blip(lv * 0.6, t, 0.001, decay * 0.7))], pan=pan)
    r.layer([osc("triangle", [["set", f * 1.78, t]], t, t + decay * 0.5)],
            [gain(blip(lv * 0.25, t, 0.001, decay * 0.4))], pan=pan)
    r.layer([noise(t, t + decay + 0.01)], [filt("highpass", 1500), filt("bandpass", 4300, 0.6),
                                           gain(blip(lv * float(p["wires"]), t, 0.001, decay))], pan=pan)
    r.layer([noise(t, t + 0.016)], [filt("bandpass", 5200, 1.0), gain(blip(lv * 0.5, t, 0.0003, 0.008))], pan=pan)


def _roll(r, rng, ev: Event, level: float, pan: float, p: dict, hit) -> None:
    rate = float(p["rollHz"])
    n = max(1, int(ev.dur * rate))
    v0 = ev.vel
    v1 = ev.vel_to if ev.vel_to is not None else ev.vel
    for i in range(n):
        k = i / max(1, n - 1)
        t = ev.t + i / rate + rng.uniform(-0.004, 0.004)
        hand = 1.0 if i % 2 == 0 else 0.84
        hit(r, rng, max(0.0, t), level * (v0 + (v1 - v0) * k ** 1.6) * hand * 0.8, pan, p)


def brass(r, rng, ev: Event, level: float, pan: float, p: dict) -> None:
    """Low horns: a sawtooth pair (+-detune) driven into a soft clip, then a low-pass that opens with the attack
    (brighter when louder) and closes over the note; a late, slight vibrato."""
    for m in ev.pitches:
        f = _tuned(m, p)
        t = ev.t
        d = ev.dur
        att = min(float(p["attackSec"]), d * 0.7)
        end = t + d + float(p["releaseSec"])
        lv = level * ev.vel
        bright = float(p["brightHz"]) * (0.55 + 0.6 * min(1.0, ev.vel))
        lp = [["set", f * 1.6, t], ["exp", bright, t + att * 1.4], ["exp", max(f * 2.0, bright * 0.62), t + d],
              ["exp", f * 1.4, end]]
        env = curve((t, 0.0), (t + att, lv), (t + d, lv * float(p["sustain"])), (end, 1e-4))
        if ev.vel_to is not None:
            env = curve((t, 0.0), (t + att, lv * 0.5), (t + d, level * ev.vel_to), (end, 1e-4))
            lp = [["set", f * 1.6, t], ["exp", bright * 0.6, t + att], ["exp", bright * 1.2, t + d],
                  ["exp", f * 1.4, end]]
        mods = [lfo("detune", "sine", rng.uniform(4.6, 5.2),
                    [["set", 0.0, t], ["lin", 0.0, t + min(0.6, d * 0.5)], ["lin", float(p["vibratoCents"]),
                                                                            t + min(1.4, d)]], t, end)]
        det = float(p["detuneCents"])
        srcs = [osc("sawtooth", [["set", f, t]], t, end + 0.01, detune=[["set", -det, t]], lfo=mods),
                osc("sawtooth", [["set", f, t]], t, end + 0.01, detune=[["set", det, t]], lfo=mods)]
        chain = [gain(float(p["drive"])), shape("tanh3")] if float(p["drive"]) > 0 else []
        r.layer(srcs, chain + [filt("lowpass", lp, 2.0), gain(env)], pan=pan)


def strings(r, rng, ev: Event, level: float, pan: float, p: dict) -> None:
    """A low string section: three detuned sawtooth players (vibrato) through a low-pass; optional tremolo (a pulse
    train at `tremoloHz` as gain automation)."""
    for m in ev.pitches:
        f = _tuned(m, p)
        t = ev.t
        d = ev.dur
        end = t + d + float(p["releaseSec"])
        lv = level * ev.vel
        att = min(float(p["attackSec"]), d * 0.7)
        if ev.vel_to is not None:
            env = curve((t, 0.0), (t + att, lv * 0.4), (t + d, level * ev.vel_to), (end, 1e-4))
        else:
            env = curve((t, 0.0), (t + att, lv), (t + d, lv * 0.9), (end, 1e-4))
        srcs = []
        for k, c in enumerate(p["players"]):
            mods = [lfo("detune", "sine", 5.0 + 0.37 * k, [["set", 0.0, t], ["lin", float(p["vibratoCents"]),
                                                                              t + min(0.7, d)]], t, end)]
            srcs.append(osc("sawtooth", [["set", f, t]], t, end + 0.01, detune=[["set", float(c), t]], lfo=mods))
        chain = [filt("lowpass", float(p["brightHz"]), 0.5), filt("highpass", f * 0.7)]
        hz = float(p["tremoloHz"])
        if hz > 0:
            chain.append(gain(pulses(rng, t, end, (hz, hz), floor=0.3, spread=0.06, peak=(0.85, 1.0),
                                     attack=0.012)))
        r.layer(srcs, chain + [gain(env)], pan=pan)


def _wander_points(rng: random.Random, t0: float, t1: float, lo: float, hi: float, step: float) -> list:
    out = [["set", rng.uniform(lo, hi), t0]]
    t = t0
    while t + step < t1:
        t += step * rng.uniform(0.7, 1.3)
        out.append(["lin", rng.uniform(lo, hi), min(t, t1)])
    return out


def drone(r, rng, ev: Event, level: float, pan: float, p: dict) -> None:
    """A bowed low drone: a sawtooth pair through a slowly wandering low-pass, a sine an octave below, a narrow
    band of noise at the second harmonic (the bow's breath); swells in over `attackSec` and dies over `releaseSec`
    (both inside the written length)."""
    for m in ev.pitches:
        f = _tuned(m, p)
        t = ev.t
        d = ev.dur
        att = min(float(p["attackSec"]), d * 0.45)
        rel = min(float(p["releaseSec"]), d * 0.5)
        end = t + d
        lv = level * ev.vel
        env = curve((t, 0.0), (t + att, lv), (end - rel, lv * 0.85), (end, 1e-4))
        lo, hi = p["cutoffHz"]
        cut = _wander_points(rng, t, end, float(lo), float(hi), float(p["wanderSec"]))
        det = float(p["detuneCents"])
        r.layer([osc("sawtooth", [["set", f, t]], t, end + 0.01, detune=[["set", -det, t]]),
                 osc("sawtooth", [["set", f, t]], t, end + 0.01, detune=[["set", det, t]])],
                [filt("lowpass", cut, 3.0), gain(env)], pan=pan)
        if float(p["sub"]) > 0:
            r.layer([osc("sine", [["set", f / 2.0, t]], t, end + 0.01)], [gain(scaled(env, float(p["sub"])))],
                    pan=pan)
        if float(p["air"]) > 0:
            r.layer([noise(t, end + 0.01)], [filt("bandpass", 2.0 * f, 30.0), filt("bandpass", 2.0 * f, 30.0),
                                             gain(scaled(env, float(p["air"])))], pan=-pan)


def glass(r, rng, ev: Event, level: float, pan: float, p: dict) -> None:
    """A high, glassy shimmer: two sines `beatHz` apart (a slow beat) and a faint octave, swelling in and out."""
    for m in ev.pitches:
        f = _tuned(m, p)
        t, d = ev.t, ev.dur
        att = min(float(p["attackSec"]), d * 0.45)
        rel = min(float(p["releaseSec"]), d * 0.5)
        lv = level * ev.vel
        env = curve((t, 0.0), (t + att, lv), (t + d - rel, lv * 0.8), (t + d, 1e-4))
        for ff, a, pp in ((f, 1.0, -0.35), (f + float(p["beatHz"]), 0.8, 0.35), (2.0 * f * 1.0013, 0.18, 0.0)):
            r.layer([osc("sine", [["set", ff, t]], t, t + d + 0.01)], [gain(scaled(env, a))],
                    pan=max(-1.0, min(1.0, pan + pp)))


def bell_inst(r, rng, ev: Event, level: float, pan: float, p: dict) -> None:
    """The keepers' bell (dark_ambience.bell; the score pitch is the nominal: D4)."""
    for m in ev.pitches:
        bell(r, rng, ev.t, midi_to_hz(m), level * ev.vel, float(p["ringSec"]), force=float(p["force"]),
             crack=float(p["crack"]), sour=float(p["sour"]), bend_cents=float(p["bendCents"]), pan=pan)


INSTRUMENTS = {"lute": lute, "choir": choir, "drum": drum, "snare": snare, "brass": brass, "strings": strings,
               "drone": drone, "glass": glass, "bell": bell_inst}

DEFAULTS: dict[str, dict] = {
    "lute": {"courseDetuneCents": 6.0, "pluckPosition": 0.14, "inharmonicity": 4e-5, "sustainSec": 5.0,
             "ringSec": 1.0, "strumSec": 0.035, "partials": 14, "maxHz": 6000.0, "click": 0.3, "knock": 2.5,
             "tuningCents": {}},
    "choir": {"voices": 3, "spreadCents": 9.0, "entrySec": 0.12, "attackSec": 0.9, "releaseSec": 1.4,
              "vibratoHz": [4.7, 5.5], "vibratoCents": 14.0, "driftCents": 6.0, "vowel": "o", "formantScale": 1.0,
              "sourceLowpassHz": 2600.0, "breath": 0.12, "lowBoost": 0.6, "width": 0.35, "tuningCents": {}},
    "drum": {"tuneHz": 60.0, "dropCents": 700.0, "dropSec": 0.07, "decaySec": 0.6, "jitterCents": 20.0,
             "modes": [[1.59, 0.35, 0.45], [2.14, 0.22, 0.3], [2.65, 0.15, 0.25], [3.16, 0.1, 0.2]],
             "beaterHz": 700.0, "beater": 0.6, "slap": 0.25, "skinHz": 320.0, "skin": 1.0, "drive": 0.0,
             "rollHz": 14.0},
    "snare": {"shellHz": 190.0, "decaySec": 0.17, "wires": 0.55, "rollHz": 20.0},
    "brass": {"attackSec": 0.14, "releaseSec": 0.45, "brightHz": 850.0, "drive": 0.4, "detuneCents": 7.0,
              "sustain": 0.8, "vibratoCents": 6.0, "tuningCents": {}},
    "strings": {"players": [-9.0, 0.0, 8.0], "brightHz": 2400.0, "tremoloHz": 0.0, "attackSec": 0.3,
                "releaseSec": 0.6, "vibratoCents": 10.0, "tuningCents": {}},
    "drone": {"attackSec": 8.0, "releaseSec": 8.0, "cutoffHz": [140.0, 340.0], "wanderSec": 5.0, "detuneCents": 5.0,
              "sub": 0.6, "air": 0.0, "tuningCents": {}},
    "glass": {"attackSec": 4.0, "releaseSec": 5.0, "beatHz": 0.6, "tuningCents": {}},
    "bell": {"ringSec": 10.0, "force": 0.8, "crack": 0.04, "sour": 0.15, "bendCents": 0.0},
}


def part_params(part: dict) -> dict:
    p = dict(DEFAULTS[part["instrument"]])
    unknown = set(part.get("params", {})) - set(p)
    if unknown:
        raise ValueError(f"{part['id']}: unknown {part['instrument']} params {sorted(unknown)}")
    p.update(part.get("params", {}))
    return p


# ---------------------------------------------------------------------------------------------------------------------
# Tracks
# ---------------------------------------------------------------------------------------------------------------------
@dataclass
class Piece:
    """A score (track or stinger) resolved from dark_scores.json."""

    key: str
    spec: dict
    stinger: bool

    @property
    def loop(self) -> bool:
        return bool(self.spec.get("loop", False))

    @property
    def beats(self) -> float:
        return float(self.spec["bars"] * self.spec["beatsPerBar"])

    @property
    def seconds(self) -> float:
        """The written length (a loop's exact length)."""
        return self.beats * 60.0 / float(self.spec["tempo"])

    @property
    def asset(self) -> str:
        return self.spec["asset"]


def pieces(data: dict) -> list[Piece]:
    return [Piece(k, v, False) for k, v in data["tracks"].items()] + \
           [Piece(k, v, True) for k, v in data["stingers"].items()]


def _seed(s: str) -> int:
    return zlib.crc32(s.encode("utf-8")) & 0xFFFFFFFF


def events(piece: Piece) -> dict[str, list[Event]]:
    """part id -> events (the structural RNG is seeded by the track id, part by part)."""
    s = piece.spec
    out = {}
    for part in s["parts"]:
        rng = random.Random(_seed(f"{piece.key}:{part['id']}:events"))
        out[part["id"]] = part_events(part, float(s["tempo"]), int(s["beatsPerBar"]), rng,
                                      piece.beats if piece.loop else None, piece.seconds if piece.loop else None)
    return out


def _build(piece: Piece, solo: str | None = None) -> AmbRecipe:
    s = piece.spec
    mode = "wrap" if piece.loop else "oneshot"
    buses = {name: dict({"mode": mode}, **space) for name, space in s["buses"].items()}
    length = piece.seconds
    r = AmbRecipe(f"mus_{piece.key}", length, 2, s.get("description", piece.key), buses,
                  loop_sec=length if piece.loop else None)
    r.source = SRC
    evs = events(piece)
    for part in s["parts"]:
        if solo is not None and part["id"] != solo:
            continue
        if part["bus"] not in buses:
            raise ValueError(f"{piece.key}/{part['id']}: unknown bus {part['bus']!r}")
        fn = INSTRUMENTS[part["instrument"]]
        p = part_params(part)
        rng = random.Random(_seed(f"{piece.key}:{part['id']}:sound"))
        with r.on(part["bus"]):
            for ev in evs[part["id"]]:
                fn(r, rng, ev, float(part["level"]), float(ev.extra.get("pan", part.get("pan", 0.0))), p)
    return r


ZERO_TAIL_SEC = 0.004


def _zero_tails(r: AmbRecipe) -> None:
    """Every envelope that decays to its floor (an exponential ramp can only reach a small value, never 0) then falls
    linearly to exact zero in 4 ms, before its sources stop: a source stopping on a quiet note (the floor is only
    20-40 dB under a soft partial's peak) is not a click."""
    for lay in r.layers:
        stop = max(s["stop"] for s in lay["sources"])
        for proc in lay["chain"]:
            a = proc.get("gain") if proc["type"] == "gain" else None
            if not isinstance(a, list) or a[-1][0] != "exp":
                continue
            end = a[-1][2]
            if len(a) >= 2 and a[-1][1] < a[-2][1] and end + ZERO_TAIL_SEC <= stop + 1e-9:
                a.append(["lin", 0.0, end + ZERO_TAIL_SEC])


def _build_tailed(piece: Piece, solo: str | None = None) -> AmbRecipe:
    r = _build(piece, solo)
    _zero_tails(r)
    return r


build = _guarded(_build_tailed)


DC_BLOCK_HZ = 5.0


def dc_block(x: np.ndarray, sr: int, loop: bool) -> np.ndarray:
    """A one-pole DC blocker (5 Hz) on the mix: a decaying low drum or drone that starts at zero phase leaves a little
    DC; the corner is far below the music, so the phase of the drums is untouched (no new peaks). A loop is filtered
    circularly (two periods, the second kept), so the seam stays clean."""
    k = 1.0 - 2.0 * math.pi * DC_BLOCK_HZ / sr
    if not loop:
        return lfilter([1.0, -1.0], [1.0, -k], x, axis=1)
    n = x.shape[1]
    return lfilter([1.0, -1.0], [1.0, -k], np.concatenate([x, x], axis=1), axis=1)[:, n:]


# One-shots (stingers, valdras_falls) keep their reverb tails down to -72 dB under the peak: a stinger's length is its
# duck time and the falls end in a written silence (the bells' -60 dB trim is for retained SFX memory).
MUSIC_TRIM_DB = -72.0


def render(piece: Piece, sr: int, seed: int, solo: str | None = None) -> np.ndarray:
    """(2, n): a loop is exactly its written length (seamless); a one-shot keeps its tails (trimmed at -72 dB)."""
    r = build(piece, solo)
    if piece.loop:
        return dc_block(render_loop(r, sr, seed), sr, True)
    return dc_block(render_oneshot(r, sr, seed, trim_db=MUSIC_TRIM_DB), sr, False)

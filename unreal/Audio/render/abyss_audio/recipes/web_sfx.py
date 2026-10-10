"""The 33 web SFX cues: a line-by-line port of src/systems/audio/SFXEngine.ts (audio.md 4).

Every function builds the same nodes, frequencies, automation and envelopes as its TypeScript namesake, at trigger
time t = 0. The length of each recipe is the last `stop` (audio.md 4.1); the renderer appends 10 ms of silence.
"""

from __future__ import annotations

from ..sfx import Recipe, adsr, filt, lfo, shape

SRC = "src/systems/audio/SFXEngine.ts"


def _r(rid: str, length: float, line: str, desc: str) -> Recipe:
    return Recipe(rid, length, description=desc, source=f"{SRC}:{line}")


# ---------------------------------------------------------------------------------------------------------------------
# Combat
# ---------------------------------------------------------------------------------------------------------------------
def hit() -> Recipe:
    r = _r("hit", 0.2, "246-285", "metallic slash")
    dur = 0.2
    r.adsr_osc("sawtooth", [["set", 250, 0], ["exp", 80, dur]], 0, dur, adsr(0.002, 0.04, 0.15, 0.12, 0.25, 0),
               [filt("bandpass", 1200, 2)])
    r.adsr_osc("sine", [["set", 100, 0], ["exp", 40, 0.08]], 0, 0.1, adsr(0.001, 0.03, 0.1, 0.06, 0.2, 0))
    r.adsr_osc("square", [["set", 2200, 0], ["exp", 1800, dur]], 0, dur, adsr(0.001, 0.02, 0.08, 0.15, 0.06, 0),
               [filt("bandpass", 3500, 8)])
    r.noise_burst(0.08, 2500, "highpass", 0, 0.2)
    return r


def hit_heavy() -> Recipe:
    r = _r("hit_heavy", 0.35, "288-330", "deep metallic slash")
    dur = 0.35
    r.adsr_osc("sawtooth", [["set", 150, 0], ["exp", 40, dur]], 0, dur, adsr(0.002, 0.06, 0.25, 0.2, 0.3, 0),
               [filt("lowpass", [["set", 600, 0], ["exp", 200, dur]], 3)])
    r.adsr_osc("square", [["set", 120, 0], ["exp", 35, dur]], 0, dur, adsr(0.002, 0.08, 0.2, 0.2, 0.12, 0),
               [shape("tanh3")])
    r.adsr_osc("sine", [["set", 80, 0], ["exp", 25, 0.12]], 0, 0.15, adsr(0.001, 0.04, 0.1, 0.08, 0.25, 0))
    r.noise_burst(0.1, 800, "lowpass", 0, 0.25)
    r.noise_burst(0.06, 3000, "highpass", 0, 0.12)
    return r


def crit() -> Recipe:
    r = _r("crit", 0.35, "333-389", "sharp high impact")
    dur = 0.35
    r.adsr_osc("square", [["set", 700, 0], ["exp", 100, 0.15]], 0, dur, adsr(0.001, 0.04, 0.15, 0.2, 0.3, 0),
               [shape("tanh4"), filt("bandpass", 1800, 1.5)])
    r.adsr_osc("sawtooth", [["set", 1100, 0], ["exp", 250, 0.12]], 0, 0.2, adsr(0.001, 0.03, 0.1, 0.15, 0.18, 0),
               [filt("highpass", 800)])
    r.adsr_osc("sine", [["set", 120, 0], ["exp", 30, 0.1]], 0, 0.12, adsr(0.001, 0.03, 0.08, 0.07, 0.25, 0))
    r.adsr_osc("square", [["set", 3200, 0], ["exp", 2400, dur]], 0, dur, adsr(0.001, 0.02, 0.06, 0.25, 0.04, 0),
               [filt("bandpass", 4000, 12)])
    r.noise_burst(0.06, 4000, "highpass", 0, 0.3)
    r.noise_burst(0.15, 1500, "bandpass", 0.05, 0.08)
    return r


def miss() -> Recipe:
    r = _r("miss", 0.2, "392-404", "whoosh")
    r.adsr_osc("sine", [["set", 400, 0], ["exp", 150, 0.2]], 0, 0.2, adsr(0.01, 0.05, 0.3, 0.1, 0.1, 0))
    return r


def block() -> Recipe:
    r = _r("block", 0.2, "407-436", "hard clang")
    dur = 0.2
    r.adsr_osc("square", [["set", 350, 0], ["exp", 250, dur]], 0, dur, adsr(0.001, 0.03, 0.12, 0.12, 0.25, 0),
               [filt("bandpass", 2000, 8)])
    r.adsr_osc("triangle", [["set", 3200, 0], ["exp", 2800, dur]], 0, dur, adsr(0.001, 0.02, 0.08, 0.15, 0.06, 0),
               [filt("bandpass", 3500, 12)])
    r.noise_burst(0.04, 4000, "highpass", 0, 0.25)
    return r


def player_hurt() -> Recipe:
    r = _r("player_hurt", 0.25, "439-470", "dull thud")
    dur = 0.25
    r.adsr_osc("sine", [["set", 180, 0], ["exp", 50, dur]], 0, dur, adsr(0.002, 0.05, 0.2, 0.15, 0.3, 0),
               [filt("lowpass", 300, 3)])
    r.adsr_osc("sawtooth", [["set", 250, 0], ["exp", 100, dur]], 0, dur, adsr(0.002, 0.04, 0.15, 0.15, 0.1, 0),
               [shape("tanh2")])
    r.noise_burst(0.08, 400, "lowpass", 0, 0.22)
    return r


def monster_death() -> Recipe:
    r = _r("monster_death", 0.6, "473-488", "descending groan")
    dur = 0.6
    r.adsr_osc("sawtooth", [["set", 200, 0], ["exp", 40, dur]], 0, dur, adsr(0.01, 0.1, 0.4, 0.4, 0.25, 0))
    r.noise_burst(dur, 600, "lowpass", 0, 0.18)
    return r


def player_death() -> Recipe:
    r = _r("player_death", 1.2, "491-517", "dark descending")
    dur = 1.2
    r.adsr_osc("sawtooth", [["set", 300, 0], ["exp", 40, dur]], 0, dur, adsr(0.02, 0.15, 0.5, 0.8, 0.25, 0))
    r.adsr_osc("sawtooth", [["set", 285, 0], ["exp", 38, dur]], 0, dur, adsr(0.02, 0.2, 0.4, 0.9, 0.18, 0))
    return r


def dodge() -> Recipe:
    r = _r("dodge", 0.22, "212-226", "airy whoosh")
    dur = 0.22
    r.adsr_osc("triangle", [["set", 180, 0], ["exp", 920, dur * 0.7], ["exp", 420, dur]], 0, dur,
               adsr(0.004, 0.035, 0.18, 0.12, 0.16, 0))
    r.noise_burst(dur * 0.8, 2600, "highpass", 0, 0.12)
    return r


def resonance() -> Recipe:
    r = _r("resonance", 0.61, "229-243", "spirit flare")
    for i, f in enumerate([330, 495, 660]):
        r.tone(f, "sine", 0.5, 0.09 - i * 0.015, i * 0.055)
    r.noise_burst(0.28, 3200, "bandpass", 0, 0.08)
    return r


# ---------------------------------------------------------------------------------------------------------------------
# Skills
# ---------------------------------------------------------------------------------------------------------------------
def skill_melee() -> Recipe:
    r = _r("skill_melee", 0.35, "524-556", "sword swing")
    dur = 0.35
    r.adsr_osc("triangle", [["set", 250, 0], ["lin", 700, 0.06], ["exp", 150, dur]], 0, dur,
               adsr(0.003, 0.05, 0.2, 0.2, 0.28, 0), [filt("bandpass", [["set", 800, 0], ["exp", 400, dur]], 2)])
    r.adsr_osc("sawtooth", [["set", 1500, 0.05], ["exp", 400, 0.15]], 0.05, 0.2,
               adsr(0.001, 0.03, 0.1, 0.12, 0.12, 0.05), [filt("bandpass", 2500, 5)])
    r.noise_burst(0.15, 1200, "bandpass", 0, 0.2)
    r.noise_burst(0.04, 3000, "highpass", 0.05, 0.15)
    return r


def skill_fire() -> Recipe:
    r = _r("skill_fire", 0.5, "559-596", "crackling fire")
    dur = 0.5
    r.adsr_osc("sawtooth", [["set", 120, 0], ["lin", 300, 0.1], ["exp", 80, dur]], 0, dur,
               adsr(0.005, 0.08, 0.3, 0.3, 0.22, 0),
               [filt("lowpass", [["set", 300, 0], ["lin", 1200, 0.15], ["exp", 400, dur]], 4)])
    # Crackle: square LFO 30 Hz x 200 Hz into the frequency (400 Hz set at t).
    r.adsr_osc("sawtooth", [["set", 400, 0]], 0, dur, adsr(0.01, 0.1, 0.2, 0.3, 0.1, 0),
               [filt("bandpass", 2000, 3)], lfo=[lfo("frequency", "square", 30, 200, 0, dur)])
    r.noise_burst(dur * 0.8, 800, "bandpass", 0, 0.25)
    r.noise_burst(0.15, 4000, "highpass", 0.05, 0.1)
    return r


def skill_ice() -> Recipe:
    r = _r("skill_ice", 0.45, "599-634", "crystal shimmer")
    dur = 0.45
    for f in (900, 907):
        r.adsr_osc("sine", [["set", f, 0], ["lin", f * 1.4, 0.12], ["exp", f * 0.7, dur]], 0, dur,
                   adsr(0.005, 0.06, 0.3, 0.25, 0.15, 0), [filt("highpass", 600)])
    r.adsr_osc("triangle", [["set", 4500, 0], ["exp", 3000, dur]], 0, dur, adsr(0.002, 0.04, 0.15, 0.3, 0.04, 0),
               [filt("bandpass", 5000, 15)])
    r.noise_burst(0.08, 5000, "highpass", 0.02, 0.18)
    r.noise_burst(dur * 0.6, 3000, "highpass", 0.05, 0.08)
    return r


def skill_lightning() -> Recipe:
    r = _r("skill_lightning", 0.4, "637-685", "electric zap")
    dur = 0.4
    # Buzz: sawtooth LFO 60 Hz x 500 Hz into the frequency; the instantaneous frequency goes negative near the end.
    r.adsr_osc("square", [["set", 1200, 0], ["exp", 150, dur]], 0, dur, adsr(0.001, 0.03, 0.2, 0.3, 0.2, 0),
               [shape("sqrt")], lfo=[lfo("frequency", "sawtooth", 60, 500, 0, dur)])
    r.adsr_osc("sawtooth", [["set", 3000, 0], ["exp", 500, 0.1]], 0, 0.15, adsr(0.001, 0.02, 0.05, 0.08, 0.12, 0),
               [filt("highpass", 2000)])
    r.adsr_osc("sine", [["set", 80, 0.02], ["exp", 30, 0.15]], 0.02, 0.18, adsr(0.002, 0.04, 0.1, 0.1, 0.2, 0.02))
    r.noise_burst(0.08, 6000, "highpass", 0, 0.3)
    r.noise_burst(0.15, 2000, "bandpass", 0.05, 0.12)
    return r


def skill_heal() -> Recipe:
    r = _r("skill_heal", 0.55, "688-715", "warm ascending")
    for i, f in enumerate([400, 500, 600, 800]):
        s = i * 0.1
        r.adsr_osc("sine", [["set", f, s]], s, s + 0.25, adsr(0.01, 0.04, 0.5, 0.15, 0.15, s),
                   [filt("lowpass", f * 3, 2)])
        r.adsr_osc("sine", [["set", f * 1.005, s]], s, s + 0.25, adsr(0.015, 0.04, 0.4, 0.15, 0.1, s))
    r.noise_burst(0.3, 4000, "highpass", 0.15, 0.04)
    return r


def skill_buff() -> Recipe:
    r = _r("skill_buff", 0.5, "718-741", "chord chime")
    dur = 0.5
    for i, f in enumerate([523, 659, 784]):
        r.adsr_osc("triangle", [["set", f, 0]], 0, dur, adsr(0.01 + i * 0.01, 0.06, 0.4, 0.3, 0.12, 0),
                   [filt("lowpass", 3000, 1)])
        r.adsr_osc("sine", [["set", f * 2, 0]], 0, dur, adsr(0.02, 0.05, 0.3, 0.3, 0.04, 0))
    return r


# ---------------------------------------------------------------------------------------------------------------------
# Loot / items
# ---------------------------------------------------------------------------------------------------------------------
def loot_common() -> Recipe:
    r = _r("loot_common", 0.2, "748-761", "simple pickup")
    r.adsr_osc("sine", [["set", 800, 0], ["lin", 1200, 0.07], ["exp", 600, 0.2]], 0, 0.2,
               adsr(0.005, 0.04, 0.3, 0.12, 0.2, 0))
    return r


def loot_magic() -> Recipe:
    r = _r("loot_magic", 0.3, "764-788", "sparkle")
    r.adsr_osc("sine", [["set", 800, 0], ["exp", 1200, 0.3]], 0, 0.3, adsr(0.005, 0.05, 0.35, 0.2, 0.18, 0))
    r.adsr_osc("triangle", [["set", 1000, 0], ["exp", 1400, 0.3]], 0, 0.3, adsr(0.005, 0.06, 0.3, 0.2, 0.13, 0))
    return r


def loot_rare() -> Recipe:
    r = _r("loot_rare", 0.52, "791-810", "rich sparkle")
    dur = 0.4
    for i, f in enumerate([900, 1100, 1350]):
        s = i * 0.06
        r.adsr_osc("sine", [["set", f, s], ["exp", f * 1.3, s + dur * 0.6]], s, s + dur,
                   adsr(0.008, 0.06, 0.35, 0.25, 0.15, s))
    r.noise_burst(dur, 5000, "highpass", 0, 0.1)
    return r


def loot_legendary() -> Recipe:
    r = _r("loot_legendary", 0.79, "813-841", "grand arpeggio with echo")
    note = 0.25
    for i, f in enumerate([523, 659, 784, 1047]):
        s = i * 0.12
        r.adsr_osc("sine", [["set", f, s]], s, s + note, adsr(0.008, 0.05, 0.5, 0.18, 0.22, s))
        e = s + 0.18
        r.adsr_osc("sine", [["set", f, e]], e, e + note, adsr(0.008, 0.05, 0.3, 0.25, 0.1, e))
    return r


def equip() -> Recipe:
    r = _r("equip", 0.1, "844-858", "metallic click")
    r.adsr_osc("square", [["set", 400, 0]], 0, 0.1, adsr(0.002, 0.02, 0.1, 0.07, 0.3, 0))
    r.noise_burst(0.1, 3000, "highpass", 0, 0.15)
    return r


def potion() -> Recipe:
    r = _r("potion", 0.25, "861-885", "liquid bubble")
    dur = 0.25
    r.adsr_osc("sine", [["set", 300, 0]], 0, dur, adsr(0.01, 0.06, 0.5, 0.15, 0.2, 0),
               lfo=[lfo("frequency", "sine", [["set", 12, 0]], [["set", 25, 0]], 0, dur)])
    return r


# ---------------------------------------------------------------------------------------------------------------------
# UI
# ---------------------------------------------------------------------------------------------------------------------
def click() -> Recipe:
    r = _r("click", 0.05, "892-903", "click")
    r.adsr_osc("sine", [["set", 600, 0]], 0, 0.05, adsr(0.002, 0.01, 0.1, 0.035, 0.12, 0))
    return r


def panel_open() -> Recipe:
    r = _r("panel_open", 0.1, "906-918", "rising")
    r.adsr_osc("sine", [["set", 400, 0], ["exp", 600, 0.1]], 0, 0.1, adsr(0.005, 0.02, 0.4, 0.06, 0.15, 0))
    return r


def panel_close() -> Recipe:
    r = _r("panel_close", 0.1, "921-933", "falling")
    r.adsr_osc("sine", [["set", 600, 0], ["exp", 400, 0.1]], 0, 0.1, adsr(0.005, 0.02, 0.4, 0.06, 0.15, 0))
    return r


def error() -> Recipe:
    r = _r("error", 0.2, "936-950", "harsh buzz")
    for f in (200, 150):
        r.adsr_osc("square", [["set", f, 0]], 0, 0.2, adsr(0.003, 0.04, 0.5, 0.13, 0.15, 0))
    return r


# ---------------------------------------------------------------------------------------------------------------------
# World / progression
# ---------------------------------------------------------------------------------------------------------------------
def zone_transition() -> Recipe:
    r = _r("zone_transition", 1.02, "957-986", "mystical sweep with echo")
    dur = 0.8
    r.adsr_osc("sine", [["set", 200, 0], ["exp", 800, 0.35], ["exp", 400, dur]], 0, dur,
               adsr(0.02, 0.1, 0.5, 0.5, 0.25, 0))
    d = 0.22
    r.adsr_osc("sine", [["set", 200, d], ["exp", 800, d + 0.35], ["exp", 400, d + dur * 0.8]], d, d + dur,
               adsr(0.02, 0.12, 0.35, 0.45, 0.12, d))
    return r


def quest_complete() -> Recipe:
    r = _r("quest_complete", 0.75, "1007-1036", "fanfare with echo")
    note = 0.25
    for i, f in enumerate([392, 523, 659, 784]):
        s = i * 0.11
        r.adsr_osc("triangle", [["set", f, s]], s, s + note, adsr(0.008, 0.05, 0.5, 0.18, 0.2, s))
        e = s + 0.17
        r.adsr_osc("triangle", [["set", f, e]], e, e + note, adsr(0.008, 0.05, 0.3, 0.22, 0.1, e))
    return r


def quest_progress() -> Recipe:
    r = _r("quest_progress", 0.41, "78", "two-note chime")
    r.chime([880, 1175], 0.07, 0.35)
    return r


def quest_objective() -> Recipe:
    r = _r("quest_objective", 0.52, "79", "three-note chime")
    r.chime([659, 880, 1319], 0.09, 0.5)
    return r


def levelup() -> Recipe:
    r = _r("levelup", 1.02, "1039-1068", "grand arpeggio")
    note = 0.3
    for i, f in enumerate([261, 329, 392, 523, 659]):
        s = i * 0.13
        r.adsr_osc("sine", [["set", f, s]], s, s + note, adsr(0.01, 0.05, 0.55, 0.2, 0.2, s))
        e = s + 0.2
        r.adsr_osc("sine", [["set", f, e]], e, e + note, adsr(0.01, 0.06, 0.35, 0.28, 0.1, e))
    return r


def npc_interact() -> Recipe:
    r = _r("npc_interact", 0.19, "1071-1085", "gentle chime")
    for i, f in enumerate([523, 659]):
        s = i * 0.04
        r.adsr_osc("sine", [["set", f, s]], s, s + 0.15, adsr(0.008, 0.03, 0.4, 0.1, 0.15, s))
    return r


def anvil() -> Recipe:
    r = _r("anvil", 1.0, "90-114", "hammer on anvil")
    r.noise_burst(0.05, 3200, "bandpass", 0, 0.3)
    r.adsr_osc("sine", [["set", 180, 0], ["exp", 70, 0.12]], 0, 0.2, adsr(0.002, 0.05, 0.2, 0.1, 0.28, 0))
    for f, peak, dur in ((880, 0.12, 0.9), (2376, 0.07, 0.7), (4550, 0.04, 0.45), (1321, 0.05, 0.6)):
        r.adsr_osc("sine", [["set", f, 0], ["exp", f * 0.995, dur]], 0, dur + 0.1,
                   adsr(0.001, 0.04, 0.5, dur, peak, 0))
    return r


ALL = [hit, hit_heavy, crit, miss, block, player_hurt, monster_death, player_death, dodge, resonance, skill_melee,
       skill_fire, skill_ice, skill_lightning, skill_heal, skill_buff, loot_common, loot_magic, loot_rare,
       loot_legendary, equip, potion, click, panel_open, panel_close, error, zone_transition, quest_complete,
       quest_progress, quest_objective, levelup, npc_interact, anvil]


def recipes() -> dict[str, Recipe]:
    return {fn().id: fn() for fn in ALL}

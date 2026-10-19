"""AU2 ambience and bells (recipes/dark_ambience.py): recipes are data, layers end in silence, loop buses are seamless
by construction (periodic automation, whole tone cycles, circular wrap / reverb), one-shots end in silence, the
manifest tables and asset names."""

import json
import math

import numpy as np
import pytest

from abyss_audio import loops, sfx
from abyss_audio.params import Param
from abyss_audio.recipes import dark_ambience as da
from abyss_audio.recipes import dark_vox

SR = 48000


def _all_recipes():
    for name, (fn, _target, _role, _row) in da.AMBIENCE.items():
        yield name, fn
    for cue, (fn, n, _rel, _spatial, _conc, _use) in da.BELL_CUES.items():
        for v in range(n):
            yield f"{cue}:{v}", (lambda fn=fn, v=v: fn(v))


BUILDERS = dict(_all_recipes())
RECIPES = {k: fn() for k, fn in BUILDERS.items()}


@pytest.mark.parametrize("key", sorted(RECIPES))
def test_recipe_is_data_and_rebuilds_identically(key):
    r = RECIPES[key]
    d = r.to_json()
    assert json.loads(json.dumps(d)) == d
    assert d["layers"] and r.length > 0
    assert BUILDERS[key]().to_json() == d
    for lay in r.layers:
        assert lay.get("bus") in r.buses, f"layer without a known bus: {lay.get('bus')}"
    loop = r.loop_sec is not None
    for bus, spec in r.buses.items():
        assert spec["mode"] in (("crossfade", "linear", "wrap") if loop else ("oneshot",)), (bus, spec)


def _gain_product(chain: list, t: np.ndarray) -> np.ndarray:
    g = np.ones_like(t)
    for proc in chain:
        if proc["type"] == "gain":
            g = g * Param.from_spec(proc["gain"], 1.0).values_at(t)
    return g


@pytest.mark.parametrize("key", sorted(RECIPES))
def test_layers_are_silent_when_their_sources_stop(key):
    """A layer must have decayed when its last source stops (a cut is a click), and no automation may start after its
    layer's first frame. Continuous layers of a loop bed (crossfade / linear buses) instead run through the whole
    render, loop + overlap (crossfade beds may also hold chunks of crackle that start and end in silence)."""
    r = RECIPES[key]
    for i, lay in enumerate(r.layers):
        start = min(s["start"] for s in lay["sources"])
        stop = max(s["stop"] for s in lay["sources"])
        mode = r.buses[lay["bus"]]["mode"]
        if mode in ("crossfade", "linear") and start == 0.0 and stop >= r.loop_sec + r.overlap_sec - 1e-9:
            continue
        assert mode != "linear", f"layer {i}: a tone layer must run through the whole render"
        t = np.linspace(start, stop, 4000)
        g = np.abs(_gain_product(lay["chain"], t))
        at_stop = float(np.abs(_gain_product(lay["chain"], np.array([stop - 1e-7])))[0])
        assert at_stop <= max(5e-3 * float(g.max()), 1.001e-4), f"layer {i}: gain {at_stop:.2e} at its stop {stop}"
        for proc in lay["chain"]:
            a = proc.get("gain") if proc["type"] == "gain" else proc.get("freq")
            if isinstance(a, list):
                assert a[0][2] <= start + 1e-12, f"layer {i}: first event at {a[0][2]} after the start {start}"


@pytest.mark.parametrize("key", sorted(k for k, r in RECIPES.items() if r.loop_sec is not None))
def test_tone_layers_repeat_every_loop(key):
    """Linear (tone) layers are steady waves with a whole number of cycles per loop, no LFO and periodic automation,
    so both sides of the coherent fold are the same signal."""
    r = RECIPES[key]
    L, ov = r.loop_sec, r.overlap_sec
    t = np.linspace(0.0, ov, 257)
    for i, lay in enumerate(r.layers):
        mode = r.buses[lay["bus"]]["mode"]
        if mode == "linear":
            for proc in lay["chain"]:
                a = proc["gain"] if proc["type"] == "gain" else proc["freq"] if proc["type"] == "filter" else None
                if isinstance(a, list):
                    p = Param.from_spec(a, 0.0)
                    np.testing.assert_allclose(p.values_at(t), p.values_at(t + L), rtol=1e-6, atol=1e-9,
                                               err_msg=f"layer {i}")
            for s in lay["sources"]:
                assert s["kind"] == "osc" and "lfo" not in s and "detune" not in s
                assert len(s["freq"]) == 1, f"layer {i}: a tone must not glide"
                cyc = s["freq"][0][1] * L
                assert abs(cyc - round(cyc)) < 0.02, f"layer {i}: {cyc} cycles per loop"


def test_periodic_and_wander_close_on_themselves():
    import random

    a = da.periodic([(0.0, 1.0), (3.0, 0.2), (7.0, 0.5), (10.0, 1.0)], 10.0, 4.0)
    p = Param.from_spec(a, 0.0)
    t = np.linspace(0.0, 4.0, 101)
    np.testing.assert_allclose(p.values_at(t), p.values_at(t + 10.0), atol=1e-12)
    w = Param.from_spec(da.wander(random.Random(5), 30.0, 6.0, 0.2, 0.9, 3.0), 0.0)
    t = np.linspace(0.0, 6.0, 101)
    np.testing.assert_allclose(w.values_at(t), w.values_at(t + 30.0), atol=1e-12)
    assert da.cycles(73.4162, 48.0) * 48.0 == pytest.approx(round(73.4162 * 48.0))


def _ping_events(r, t0: float, n_copies: int, period: float) -> None:
    for k in range(n_copies):
        for t, f in ((0.3, 880.0), (1.7, 523.0), (3.6, 659.0)):  # the last one rings past the loop end
            tt = t0 + t + k * period
            r.layer([sfx.osc("sine", [["set", f, tt]], tt, tt + 0.9)], [sfx.gain(sfx.pluck_env(0.2, 0.85, tt))])


def test_wrap_bus_equals_a_steady_state_render():
    """The wrap bus with its low-pass and reverb, run circularly, equals the middle period of a three-period render
    of the same events (what a loop that has been playing sounds like)."""
    L = 4.0
    space = {"mode": "wrap", "lowpass": 2000.0, "reverb": da._space(0.9, 0.6, dry=0.7, tone=3000, pre=0.02)}
    loop = da.AmbRecipe("t_loop", L, 2, "", {"far": space}, loop_sec=L)
    with loop.on("far"):
        _ping_events(loop, 0.0, 1, L)
    y = da.render_loop(loop, SR, 123)
    long = da.AmbRecipe("t_long", 3 * L, 2, "", {"far": {"mode": "oneshot"}})
    with long.on("far"):
        _ping_events(long, 0.0, 3, L)
    x = sfx.render(da._sub(long, "far", 3 * L), SR, 0, extra_sec=0.0)[:, :int(3 * L * SR)]
    z = da.apply_space(x, space, SR, da._bus_seed(123, 100), loop=False)
    mid = z[:, int(L * SR):int(2 * L * SR)]
    assert y.shape == mid.shape
    assert float(np.max(np.abs(y - mid))) < 1e-6 * float(np.max(np.abs(mid))) + 1e-9


def test_loop_render_is_seamless_and_deterministic():
    L, ov = 4.0, 1.0
    r = da.AmbRecipe("t_mix", L + ov, 2, "", {"bed": {"mode": "crossfade"}, "tone": {"mode": "linear"},
                                              "ev": {"mode": "wrap", "reverb": da._space(0.8, 0.5)}},
                     loop_sec=L, overlap_sec=ov)
    with r.on("bed"):
        r.layer([sfx.noise(0.0, L + ov)], [sfx.filt("lowpass", 600, 0.0), sfx.gain(0.1)])
    with r.on("tone"):
        r.layer([sfx.osc("sawtooth", [["set", da.cycles(110.3, L), 0.0]], 0.0, L + ov)],
                [sfx.filt("lowpass", 400, 3.0), sfx.gain(0.05)])
    with r.on("ev"):
        _ping_events(r, 0.0, 1, L)
    a = da.render_loop(r, SR, 77)
    b = da.render_loop(r, SR, 77)
    assert a.shape == (2, int(L * SR))
    assert np.array_equal(a, b)

    def seam_step_ok(y):
        inner = float(np.max(np.abs(np.diff(y, axis=1))))
        return float(np.max(np.abs(y[:, 0] - y[:, -1]))) <= inner

    assert seam_step_ok(a)
    # The tone bus alone continues exactly across the seam (a coherent fold of a periodic signal is an identity).
    tone = da.AmbRecipe("t_tone", L + ov, 1, "", {"tone": {"mode": "linear"}}, loop_sec=L, overlap_sec=ov)
    with tone.on("tone"):
        tone.layer([sfx.osc("sawtooth", [["set", da.cycles(110.3, L), 0.0]], 0.0, L + ov)],
                   [sfx.filt("lowpass", 400, 3.0), sfx.gain(0.05)])
    y = da.render_loop(tone, SR, 1)
    x = sfx.render(da._sub(tone, "tone", L + ov), SR, da._bus_seed(1, 0), extra_sec=0.0)
    n, k0, k1 = int(L * SR), int(0.05 * SR), int(ov * SR)
    assert seam_step_ok(y)
    assert float(np.max(np.abs(y[:, k0:k1] - x[:, n + k0:n + k1]))) < 1e-6 * float(np.max(np.abs(x)))


@pytest.mark.parametrize("name", ["SW_AMB_AshwheatKnock_01", "SW_AMB_KnellwatchHearth"])
def test_shipped_loops_are_seamless(name):
    fn, _target, _role, _row = da.AMBIENCE[name]
    r = fn()
    y = da.render_loop(r, SR, sfx.seed_for(name, 1))
    assert y.shape == (r.channels, int(round(r.loop_sec * SR)))
    assert np.all(np.isfinite(y))
    assert loops.seam_ratio(y) < 1.0
    assert abs(float(np.mean(y))) < 0.01 * float(np.max(np.abs(y)))


@pytest.mark.parametrize("cue", ["bell_toll", "bell_toll_far", "bell_cracked", "bell_ambush_tell"])
def test_bells_strike_at_once_and_end_in_silence(cue):
    fn, _n, _rel, _spatial, _conc, _use = da.BELL_CUES[cue]
    y = da.render_oneshot(fn(0), SR, sfx.seed_for(f"SW_TEST_{cue}", 1))
    assert np.all(np.isfinite(y))
    env = np.max(np.abs(y), axis=0)
    pk = float(env.max())
    assert int(np.argmax(env > 0.1 * pk)) < int(0.05 * SR)  # the strike is at the start (the tell's timing)
    tail = y[:, -int(0.005 * SR):]
    assert float(np.sqrt(np.mean(tail ** 2))) < 1e-4 * pk
    assert abs(float(np.mean(y))) < 0.01 * pk


def test_bell_spatial_information():
    """The ambush tell is 2d and the hands rise around the hero: it must point nowhere (no panned layer; its stereo is
    the reverb's). The far toll is mono and the toll near; one cue each, so the core picks by where the hero is."""
    for v in range(da.BELL_CUES["bell_ambush_tell"][1]):
        r = da.bell_ambush_tell(v)
        assert r.channels == 2 and all("pan" not in lay for lay in r.layers)
    assert da.BELL_CUES["bell_ambush_tell"][3] == "2d"
    far = da.bell_toll_far(0)
    assert far.channels == 1 and da.BELL_CUES["bell_toll_far"][2] < da.BELL_CUES["bell_toll"][2] - 3.0
    near = da.bell_toll(0)
    assert near.channels == 2
    # The far toll is the same bell: its strongest partial is the keepers' bell's (D, not the march bed's A4 bell).
    freqs = {round(s["freq"][0][1], 1) for lay in far.layers for s in lay["sources"] if s["kind"] == "osc"}
    assert round(da.KNELL_NOMINAL, 1) in freqs and round(da.CHAPEL_NOMINAL, 1) not in freqs


def test_bell_tails_are_trimmed():
    """One-shots stop where the tail has fallen BELL_TRIM_DB under the peak (plus the fade): no seconds of inaudible
    reverb in memory."""
    for cue in ("bell_toll", "bell_dawn_toll"):
        fn = da.BELL_CUES[cue][0]
        y = da.render_oneshot(fn(0), SR, sfx.seed_for(f"SW_TEST_{cue}", 1))
        env = np.max(np.abs(y), axis=0)
        floor = float(env.max()) * 10.0 ** (da.BELL_TRIM_DB / 20.0)
        last = int(np.nonzero(env > floor)[0][-1])
        assert y.shape[1] - last <= int(0.08 * SR) + 1


def test_ambush_tell_ground_stir_peaks_before_the_hands_rise():
    r = da.bell_ambush_tell(0)
    ground = da._sub(r, "ground", r.length)
    x = sfx.render(ground, SR, 1)
    env = np.convolve(np.abs(x[0]), np.ones(960) / 960, mode="same")
    assert 1.3 <= float(np.argmax(env)) / SR <= 1.55


def test_tables_and_asset_names():
    import render_all

    assert render_all.named_assets("bell_toll") == ["SW_SFX_BellToll_01", "SW_SFX_BellToll_02", "SW_SFX_BellToll_03"]
    assert render_all.named_assets("bell_dawn_toll") == ["SW_SFX_BellDawnToll"]
    assert render_all.named_assets("bell_toll_far") == ["SW_SFX_BellTollFar_01", "SW_SFX_BellTollFar_02"]
    for cue, (_fn, n, rel, spatial, conc, use) in da.BELL_CUES.items():
        assert cue.startswith("bell_") and n >= 1 and -12.0 <= rel <= 0.0 and spatial in ("2d", "3d") and use
        assert set(conc) == {"max", "rule", "minRetriggerMs"}
    for name, (fn, target, role, row) in da.AMBIENCE.items():
        if isinstance(target, dict):  # a group levelled with one gain relative to `hit` (the knock doors)
            assert set(target) == {"group", "relativeToHitLu"} and -8.0 <= target["relativeToHitLu"] <= -2.0
            assert role == "emitter" and target["group"] == row["id"]
        else:
            assert name.startswith("SW_AMB_") and -40.0 <= target <= -28.0
        assert name.startswith("SW_AMB_") and role in ("bed", "region", "layer", "emitter")
        r = RECIPES[name]
        assert r.channels == (1 if role == "emitter" else 2)
        assert r.loop_sec and float(r.loop_sec).is_integer()  # whole-second loops: exact frame counts
        if role == "emitter":
            assert row["id"] in da.EMITTERS
    # Neighbouring doors never fall into step: the knock loops have different lengths.
    lens = [RECIPES[f"SW_AMB_AshwheatKnock_0{v}"].loop_sec for v in (1, 2, 3)]
    assert len(set(lens)) == 3
    table = render_all.ambience_table({n: {} for n in da.AMBIENCE})
    assert table["zones"] == {"emerald_plains": "SW_AMB_March", "ember_tower": "SW_AMB_Plains"}
    assert table["themes"] == {"plains": "SW_AMB_Plains", "march": "SW_AMB_March"}
    assert all(isinstance(v, str) for v in list(table["zones"].values()) + list(table["themes"].values()))
    assert set(table["regions"]) == {"breach_rim"} and set(table["layers"]) == {"gnawed_field_ringing"}
    assert set(table["emitters"]) == set(da.EMITTERS)
    # Machine-readable geometry, conditions and attenuation (U4 / U7 place and gate from data, not prose).
    assert "Euclidean" in table["regionRule"] and "Linear" in table["emitterAttenuationRule"]
    rim = table["regions"]["breach_rim"]
    assert (rim["col"], rim["row"], rim["radius"], rim["exitRadius"], rim["mapRegion"]) == (15, 95, 16, 17, "breach_rim")
    ring = table["layers"]["gnawed_field_ringing"]
    assert (ring["col"], ring["row"], ring["radius"]) == (34, 40, 15) and ring["exitRadius"] >= ring["radius"]
    assert ring["while"] == {"monsterAlive": "hunt_hungerbell"} and ring["untilFlag"] == "rf_hungerbell_silenced"
    for row in list(table["regions"].values()) + list(table["layers"].values()):
        assert 0 <= row["col"] < 120 and 0 <= row["row"] < 120 and 0 < row["radius"] <= row["exitRadius"]
        for cond, monster in row.get("while", {}).items():
            assert cond == "monsterAlive" and monster in dark_vox.MONSTER_FAMILY
    for eid, e in table["emitters"].items():
        att = e["attenuation"]
        assert e["spatial"] == "3d" and 0 < att["innerTiles"] < att["innerTiles"] + att["falloffTiles"] <= 12, eid
        place = e["placement"]
        where = place.get("area") or place.get("anchor")
        assert where and 0 <= where["col"] < 120 and 0 <= where["row"] < 120, eid
    assert table["emitters"]["knellwatch_hearth"]["placement"]["anchor"] == {"col": 15, "row": 15}
    # A manifest rebuilt from a render without AU2 keeps the A7 table.
    assert render_all.ambience_table({})["zones"]["emerald_plains"] == "SW_AMB_Plains"


def test_the_breach_hum_is_the_bells_hum():
    """The ley line: the Breach Rim's hum sits on the keepers' bell's hum (D2)."""
    hum = da.KNELL_NOMINAL * da.BELL_PARTIALS[0][0]
    r = RECIPES["SW_AMB_BreachRim"]
    tones = [s["freq"][0][1] for lay in r.layers if lay["bus"] == "tone" for s in lay["sources"]]
    assert min(abs(f - hum) for f in tones) < 0.05
    assert math.isclose(hum, 73.415, abs_tol=0.01)

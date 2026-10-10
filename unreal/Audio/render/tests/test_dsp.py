"""DSP emulation checks (audio.md 2, 9.9)."""

import math

import numpy as np
import pytest

from abyss_audio import SAMPLE_RATE as SR
from abyss_audio import biquad, dynamics, graph, master, osc, shaper
from abyss_audio.params import Param, adsr


# ---- AudioParam (2.1) ----
def test_param_ramps():
    p = Param(1.0).set(0.0, 0.0).lin(1.0, 1.0).exp(0.01, 2.0)
    t = np.array([-0.5, 0.0, 0.5, 1.0, 1.5, 2.0, 3.0])
    v = p.values_at(t)
    assert v[0] == 1.0  # default before the first event
    assert v[1] == 0.0
    assert v[2] == pytest.approx(0.5)
    assert v[3] == pytest.approx(1.0)
    assert v[4] == pytest.approx(0.1)  # 1 * (0.01)^(0.5)
    assert v[5] == pytest.approx(0.01)
    assert v[6] == pytest.approx(0.01)  # holds


def test_adsr_has_no_sustain_hold():
    p = adsr(Param(1.0), 0.01, 0.05, 0.5, 0.1, 0.2, 0.0)
    v = p.values_at(np.array([0.01, 0.06, 0.16, 0.5]))
    assert v[0] == pytest.approx(0.2)
    assert v[1] == pytest.approx(0.1)
    assert v[2] == pytest.approx(0.001)
    assert v[3] == pytest.approx(0.001)


# ---- oscillators (2.2) ----
@pytest.mark.parametrize("wave", ["square", "sawtooth", "triangle"])
def test_oscillator_band_limited(wave):
    f = 2500.0
    x = osc.render(wave, np.full(SR, f), SR)
    spec = np.abs(np.fft.rfft(x * np.hanning(x.size)))
    freqs = np.fft.rfftfreq(x.size, 1 / SR)
    peak = spec.max()
    # Nothing aliased: every component above -60 dB sits on a harmonic of f.
    strong = freqs[spec > peak * 1e-3]
    assert np.all(np.min(np.abs(strong[:, None] - f * np.arange(1, 11)[None, :]), axis=1) < 5.0)
    assert np.max(np.abs(x)) <= 1.0 + 1e-9


def test_oscillator_starts_at_phase_zero_rising():
    x = osc.render("sawtooth", np.full(64, 440.0), SR)
    assert abs(x[0]) < 1e-9 and x[1] > 0


def test_negative_frequency_runs_backwards():
    x = osc.render("sine", np.full(32, -440.0), SR)
    assert x[1] < 0


# ---- biquads (2.3) ----
def test_lowpass_q_in_db():
    # Q = 0 dB -> resonance 1 (|H(fc)| = 1 = 0 dB); default Q 1 dB -> +1 dB at fc.
    assert biquad.magnitude_db("lowpass", 1000, 0.0, 0, SR, 1000) == pytest.approx(0.0, abs=0.05)
    assert biquad.magnitude_db("lowpass", 1000, None, 0, SR, 1000) == pytest.approx(1.0, abs=0.05)
    assert biquad.magnitude_db("highpass", 75, 0.7, 0, SR, 75) == pytest.approx(0.7, abs=0.05)


def test_bandpass_unity_peak_and_lowshelf():
    assert biquad.magnitude_db("bandpass", 1200, 2, 0, SR, 1200) == pytest.approx(0.0, abs=0.01)
    assert biquad.magnitude_db("lowshelf", 180, None, -4, SR, 20) == pytest.approx(-4.0, abs=0.1)
    assert biquad.magnitude_db("lowshelf", 180, None, -4, SR, 5000) == pytest.approx(0.0, abs=0.1)


def test_time_varying_filter_matches_constant():
    rng = np.random.default_rng(1)
    x = rng.standard_normal(4800)
    a = biquad.process(x, "lowpass", 900.0, None, 0.0, SR)
    b = biquad._process_varying(x, biquad.LOWPASS, np.full(x.size, 900.0), 1.0, 0.0, float(SR))
    assert np.max(np.abs(a - b)) < 1e-9


# ---- wave shaper (2.4) ----
def test_shaper_curve_lookup():
    c = shaper.make_curve("tanh3")
    y = shaper.apply(np.array([-2.0, -1.0, 0.0, 0.5, 1.0, 2.0]), c)
    assert y[0] == pytest.approx(c[0]) and y[1] == pytest.approx(c[0])
    # x = 0 lands between curve[127] = F(-1/128) and curve[128] = F(0): the web's 256-point curve is off-centre.
    assert y[2] == pytest.approx(0.5 * (c[127] + c[128]))
    v = 127.5 * 1.5
    k = int(v)
    assert y[3] == pytest.approx((1 - (v - k)) * c[k] + (v - k) * c[k + 1])
    assert y[4] == pytest.approx(c[-1]) and y[5] == pytest.approx(c[-1])


# ---- Chromium compressor (2.7) ----
def test_compressor_static_curve_matches_spec_table():
    k, *_rest, makeup = dynamics.static_curve(-18, 6, 4)
    assert k == pytest.approx(13.43, abs=0.05)
    assert 20 * math.log10(makeup) == pytest.approx(6.95, abs=0.05)
    for i, o in ((-30, -23.05), (-18, -11.05), (-12, -7.63), (-6, -6.13), (0, -4.63)):
        assert dynamics.static_io_db(i, -18, 6, 4) == pytest.approx(o, abs=0.05)
    *_r, lim_makeup = dynamics.static_curve(-1, 0, 20)
    assert 20 * math.log10(lim_makeup) == pytest.approx(0.57, abs=0.05)


def test_compressor_steady_state_level_and_latency():
    t = np.arange(SR * 2) / SR
    x = 10 ** (-30 / 20) * np.sin(2 * math.pi * 1000 * t)
    y = dynamics.compress(np.stack([x, x]), SR, -18, 6, 4, 0.003, 0.25)
    lat = dynamics.latency_frames(SR)
    assert np.max(np.abs(y[0, :lat])) == 0.0  # 6 ms look-ahead
    steady = y[0, SR:]
    assert 20 * math.log10(np.max(np.abs(steady))) == pytest.approx(-23.05, abs=0.15)


# ---- graph ----
def test_sources_start_on_the_next_frame():
    v = graph.Voice([graph.Osc("sine", Param(1000.0), 0.001, 0.002)], [])
    f0, x = graph.render_voice(v, SR, 0, 200)
    nz = np.nonzero(x)[0]
    assert nz[0] >= math.ceil(0.001 * SR)
    assert nz[-1] < math.ceil(0.002 * SR)


# ---- BS.1770 (master) ----
def test_lufs_calibration():
    t = np.arange(SR * 5) / SR
    x = 10 ** (-20 / 20) * np.sin(2 * math.pi * 997 * t)
    assert master.integrated_lufs(np.stack([x, x]), SR) == pytest.approx(-20.0, abs=0.1)
    assert master.integrated_lufs(np.stack([x, np.zeros_like(x)]), SR) == pytest.approx(-23.01, abs=0.1)


def test_limiter_true_peak_ceiling():
    rng = np.random.default_rng(3)
    x = np.clip(rng.standard_normal((2, SR)) * 0.5, -1.5, 1.5)
    y, gr = master.limit(x, SR, -1.0)
    assert master.db(master.true_peak(y)) <= -1.0 + 1e-6
    assert gr > 0

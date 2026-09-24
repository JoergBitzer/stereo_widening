"""Tests of the M/S width reference (algorithm 2.1) and the biquad designs."""

import numpy as np
import pytest
from scipy import signal

from algorithms import bass_mono, filters
from algorithms.ms_width import ms_width
from stereo_eval import measures
from stereo_eval import signals as sig

FS = 44100
N = 5 * FS


@pytest.fixture
def x_uncorrelated():
    return sig.correlated_noise_pair(N, 0.0, np.random.default_rng(7))


def test_width_one_is_identity(x_uncorrelated):
    y = ms_width(x_uncorrelated, FS, width=1.0)
    assert np.max(np.abs(y - x_uncorrelated)) < 1e-12


def test_width_zero_is_mono(x_uncorrelated):
    y = ms_width(x_uncorrelated, FS, width=0.0)
    assert np.max(np.abs(y[:, 0] - y[:, 1])) < 1e-12


def test_mono_input_is_unchanged():
    mono = sig.pink_noise(N, np.random.default_rng(1))
    x = np.column_stack([mono, mono])
    y = ms_width(x, FS, width=2.0, side_shelf_db=6.0)
    assert np.max(np.abs(y - x)) < 1e-12


def test_mono_input_stays_mono_with_bass_mono():
    # the allpass on M changes the waveform, but L' = R' and the spectrum is unchanged
    mono = sig.pink_noise(N, np.random.default_rng(1))
    x = np.column_stack([mono, mono])
    y = ms_width(x, FS, width=2.0, bass_mono_hz=120.0)
    assert np.max(np.abs(y[:, 0] - y[:, 1])) < 1e-12
    assert measures.mono_sum_coloration(x, y, FS)["coloration_dB"] < 0.1


@pytest.mark.parametrize("params", [dict(width=0.0), dict(width=2.0), dict(width=1.5, side_shelf_db=6.0)])
def test_mono_sum_is_never_changed(x_uncorrelated, params):
    y = ms_width(x_uncorrelated, FS, **params)
    assert np.max(np.abs((y[:, 0] + y[:, 1]) - (x_uncorrelated[:, 0] + x_uncorrelated[:, 1]))) < 1e-12


@pytest.mark.parametrize("mode", ["hp2", "complementary", "lr4_allpass", "linear_phase"])
def test_mono_sum_is_not_coloured_by_bass_mono(x_uncorrelated, mode):
    y = ms_width(x_uncorrelated, FS, width=1.5, bass_mono_hz=150.0, bass_mono_mode=mode)
    delay = bass_mono.latency_samples(mode)  # compare time-aligned signals
    x_aligned, y_aligned = x_uncorrelated[: len(y) - delay], y[delay:]
    assert measures.mono_sum_coloration(x_aligned, y_aligned, FS)["coloration_dB"] < 0.1


def test_width_changes_side_level(x_uncorrelated):
    s_in = measures.levels(x_uncorrelated, FS)["S_dB"]
    s_out = measures.levels(ms_width(x_uncorrelated, FS, width=2.0), FS)["S_dB"]
    assert s_out - s_in == pytest.approx(6.02, abs=0.01)


def test_width_above_one_gives_negative_correlation(x_uncorrelated):
    assert measures.correlation(ms_width(x_uncorrelated, FS, width=2.0)) < -0.5


def test_constant_power_compensation(x_uncorrelated):
    # the compensation assumes equal power of M and S: make it exact for this test
    mid, side = measures.mid_side(x_uncorrelated)
    side *= np.sqrt(np.mean(mid**2) / np.mean(side**2))
    x = np.column_stack([mid + side, mid - side])
    rms_in = measures.rms_db(x)
    for width in (0.0, 0.5, 2.0):
        y = ms_width(x, FS, width=width, compensation="constant_power")
        assert measures.rms_db(y) == pytest.approx(rms_in, abs=1e-9)


def test_bass_mono_makes_low_frequencies_mono(x_uncorrelated):
    y = ms_width(x_uncorrelated, FS, width=1.0, bass_mono_hz=200.0)
    f, rho = measures.band_correlation(y, FS)
    assert np.all(rho[f <= 63] > 0.95)
    assert np.all(np.abs(rho[f >= 2000]) < 0.15)


def test_side_shelf_boosts_high_side_only(x_uncorrelated):
    y = ms_width(x_uncorrelated, FS, side_shelf_db=6.0, side_shelf_hz=2000.0)
    _, side_in = measures.mid_side(x_uncorrelated)
    _, side_out = measures.mid_side(y)
    f, lev_in = measures.band_levels_db(side_in, FS)
    _, lev_out = measures.band_levels_db(side_out, FS)
    diff = lev_out - lev_in
    assert np.all(np.abs(diff[f <= 200]) < 0.3)
    assert np.all(np.abs(diff[f >= 8000] - 6.0) < 0.3)


# --- biquad designs --------------------------------------------------------

def _gain_db(b, a, f_hz):
    _, h = signal.freqz(b, a, worN=[f_hz], fs=FS)
    return 20 * np.log10(np.abs(h[0]))


def test_highpass_is_butterworth():
    b, a = filters.highpass(100.0, FS)
    assert _gain_db(b, a, 100.0) == pytest.approx(-3.01, abs=0.01)
    assert _gain_db(b, a, 10000.0) == pytest.approx(0.0, abs=0.01)
    assert _gain_db(b, a, 50.0) == pytest.approx(-12.3, abs=0.1)  # 12 dB/octave (-12.3 = 10log10(1+2^4))


def test_lowpass_is_butterworth():
    b, a = filters.lowpass(1000.0, FS)
    assert _gain_db(b, a, 1000.0) == pytest.approx(-3.01, abs=0.01)
    assert _gain_db(b, a, 20.0) == pytest.approx(0.0, abs=0.01)


@pytest.mark.parametrize("gain_db", [-6.0, 6.0])
def test_high_shelf_gain(gain_db):
    b, a = filters.high_shelf(3000.0, gain_db, FS)
    assert _gain_db(b, a, 50.0) == pytest.approx(0.0, abs=0.05)
    assert _gain_db(b, a, 3000.0) == pytest.approx(gain_db / 2, abs=0.05)
    assert _gain_db(b, a, 20000.0) == pytest.approx(gain_db, abs=0.2)

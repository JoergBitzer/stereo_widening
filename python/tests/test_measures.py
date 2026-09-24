"""Validation of the measures with signals whose results are known in advance (Phase 1, step 3)."""

import numpy as np
import pytest
from scipy import signal

from stereo_eval import binaural, measures
from stereo_eval import signals as sig

FS = 44100
N = 10 * FS


@pytest.fixture
def rng():
    return np.random.default_rng(12345)


# --- correlation -----------------------------------------------------------

@pytest.mark.parametrize("rho", [1.0, 0.5, 0.0])
def test_broadband_correlation_of_noise_pair(rng, rho):
    x = sig.correlated_noise_pair(N, rho, rng)
    # pink noise has most energy at low frequencies -> few independent samples -> tolerance 0.1
    assert measures.correlation(x) == pytest.approx(rho, abs=0.1)


def test_antiphase_correlation_is_minus_one(rng):
    mono = sig.pink_noise(N, rng)
    assert measures.correlation(np.column_stack([mono, -mono])) == pytest.approx(-1.0, abs=1e-12)


def test_correlation_is_independent_of_gain(rng):
    mono = sig.pink_noise(N, rng)
    assert measures.correlation(np.column_stack([mono, 0.1 * mono])) == pytest.approx(1.0, abs=1e-12)


def test_hard_panned_correlation_is_undefined(rng):
    x = sig.pan_constant_power(sig.pink_noise(N, rng), -1.0)
    assert np.isnan(measures.correlation(x))


@pytest.mark.parametrize("rho", [1.0, 0.5, 0.0])
def test_band_correlation_of_noise_pair(rng, rho):
    x = sig.correlated_noise_pair(N, rho, rng)
    f, band_rho = measures.band_correlation(x, FS)
    assert np.all(np.isfinite(band_rho))
    assert np.all(np.abs(band_rho[f >= 100] - rho) < 0.15)


def test_correlation_over_time_marks_silence_as_nan(rng):
    x = np.zeros((2 * FS, 2))
    x[:FS] = sig.correlated_noise_pair(FS, 1.0, rng)
    _, rho = measures.correlation_over_time(x, FS)
    assert np.nanmin(rho) == pytest.approx(1.0)
    assert np.isnan(rho[-1])


# --- levels ----------------------------------------------------------------

def test_mid_side_levels(rng):
    mono = sig.pink_noise(N, rng)
    lev_mono = measures.levels(np.column_stack([mono, mono]), FS)
    lev_anti = measures.levels(np.column_stack([mono, -mono]), FS)
    assert lev_mono["S_dB"] < -150 and lev_mono["M_dB"] == pytest.approx(0.0, abs=1e-9)
    assert lev_anti["M_dB"] < -150 and lev_anti["S_dB"] == pytest.approx(0.0, abs=1e-9)


def test_side_to_mid_ratio_of_uncorrelated_noise_is_zero_db(rng):
    x = sig.correlated_noise_pair(N, 0.0, rng)
    assert measures.levels(x, FS)["S_minus_M_dB"] == pytest.approx(0.0, abs=1.0)


def test_loudness_of_sine():
    # BS.1770: a 997 Hz sine at 0 dBFS in one channel reads -3.01 LUFS,
    # so amplitude 0.1 in both channels reads -20 LUFS
    t = np.arange(5 * FS) / FS
    s = 0.1 * np.sin(2 * np.pi * 997 * t)
    assert measures.loudness_lufs(np.column_stack([s, s]), FS) == pytest.approx(-20.0, abs=0.1)


def test_third_octave_bands():
    bands = measures.third_octave_bands()
    assert bands[:, 1] == pytest.approx(1000.0 * 10 ** (np.arange(-13, 13) / 10))
    assert np.all(bands[1:, 0] == pytest.approx(bands[:-1, 2]))


# --- mono-sum colouration --------------------------------------------------

def _mono_pink(rng):
    mono = sig.pink_noise(N, rng)
    return mono, np.column_stack([mono, mono])


def test_identity_has_no_mono_coloration(rng):
    _, x = _mono_pink(rng)
    res = measures.mono_sum_coloration(x, x, FS)
    assert res["coloration_dB"] == pytest.approx(0.0, abs=1e-9)
    assert res["offset_dB"] == pytest.approx(0.0, abs=1e-9)


def test_haas_delay_colours_the_mono_sum(rng):
    mono, x = _mono_pink(rng)
    delay = int(0.001 * FS)
    haas = np.column_stack([mono, np.concatenate([np.zeros(delay), mono[:-delay]])])
    assert measures.mono_sum_coloration(x, haas, FS)["coloration_dB"] > 10.0


def test_complementary_comb_keeps_the_mono_sum(rng):
    mono, x = _mono_pink(rng)
    delay, g = int(0.01 * FS), 0.5
    delayed = np.concatenate([np.zeros(delay), mono[:-delay]])
    comb = np.column_stack([mono + g * delayed, mono - g * delayed])
    res = measures.mono_sum_coloration(x, comb, FS)
    assert res["coloration_dB"] == pytest.approx(0.0, abs=0.01)
    assert measures.correlation(comb) < 0.7


def test_broadband_gain_is_offset_not_coloration(rng):
    _, x = _mono_pink(rng)
    res = measures.mono_sum_coloration(x, 0.5 * x, FS)
    assert res["offset_dB"] == pytest.approx(-6.02, abs=0.01)
    assert res["coloration_dB"] == pytest.approx(0.0, abs=0.01)


# --- binaural / IACC -------------------------------------------------------

def test_itd_of_speaker_at_30_degrees():
    # Woodworth: ITD = a/c * (theta + sin theta) = 0.26 ms = 11.5 samples at 44.1 kHz
    ipsi = binaural.spherical_head_hrir(30.0, 90.0, FS)
    contra = binaural.spherical_head_hrir(30.0, -90.0, FS)
    assert 10 <= np.argmax(np.abs(contra)) - np.argmax(np.abs(ipsi)) <= 13


def test_head_shadow_attenuates_high_frequencies_at_far_ear():
    ipsi = binaural.spherical_head_hrir(30.0, 90.0, FS)
    contra = binaural.spherical_head_hrir(30.0, -90.0, FS)
    f, h_ipsi = signal.freqz(ipsi, worN=2048, fs=FS)
    _, h_contra = signal.freqz(contra, worN=2048, fs=FS)
    k_low, k_high = np.argmin(np.abs(f - 100)), np.argmin(np.abs(f - 8000))
    assert abs(h_ipsi[k_low]) == pytest.approx(abs(h_contra[k_low]), rel=0.05)
    assert 20 * np.log10(abs(h_ipsi[k_high]) / abs(h_contra[k_high])) > 3.0


def test_iacc_of_centred_mono_is_one(rng):
    _, x = _mono_pink(rng)
    assert binaural.iacc(binaural.loudspeaker_to_ears(x[: 2 * FS], FS), FS) == pytest.approx(1.0, abs=1e-6)


def test_iacc_decreases_with_decorrelation(rng):
    values = []
    for rho in (1.0, 0.5, 0.0):
        x = sig.correlated_noise_pair(3 * FS, rho, rng)
        _, band = binaural.band_iacc(binaural.loudspeaker_to_ears(x, FS), FS)
        values.append(np.mean(band[3:]))  # 1 kHz and above: little crosstalk correlation
    assert values[0] > values[1] > values[2]
    assert values[2] < 0.6

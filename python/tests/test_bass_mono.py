"""Tests of the bass mono variants (algorithms/bass_mono.py)."""

import numpy as np
import pytest
from scipy import signal

from algorithms import bass_mono, filters

FS = 44100
FC = 120.0
F = np.geomspace(10.0, 20000.0, 1000)


def test_lr4_sum_is_the_allpass():
    _, h_hp = signal.freqz(*filters.highpass(FC, FS), worN=F, fs=FS)
    _, h_lp = signal.freqz(*filters.lowpass(FC, FS), worN=F, fs=FS)
    _, h_ap = signal.freqz(*filters.allpass(FC, FS), worN=F, fs=FS)
    assert np.max(np.abs(h_hp**2 + h_lp**2 - h_ap)) < 1e-9
    assert np.max(np.abs(np.abs(h_ap) - 1.0)) < 1e-9


@pytest.mark.parametrize("variant", bass_mono.VARIANTS)
def test_mid_magnitude_is_unchanged(variant):
    h_mid, _ = bass_mono.frequency_responses(variant, FC, FS, F)
    assert np.max(np.abs(np.abs(h_mid) - 1.0)) < 1e-6


@pytest.mark.parametrize("variant", ["lr4_allpass", "linear_phase"])
def test_phase_aligned_variants_keep_mid_and_side_in_phase_above_fc(variant):
    h_mid, h_side = bass_mono.frequency_responses(variant, FC, FS, F)
    above = F > FC / 2
    assert np.max(np.abs(np.angle(h_side[above] / h_mid[above]))) < 1e-3


def test_hp2_has_a_phase_error_above_fc():
    h_mid, h_side = bass_mono.frequency_responses("hp2", FC, FS, F)
    k = np.argmin(np.abs(F - 2 * FC))
    assert np.degrees(np.angle(h_side[k] / h_mid[k])) > 30.0


@pytest.mark.parametrize("variant, min_attenuation_db", [
    ("hp2", 12.0), ("complementary", 2.5), ("lr4_allpass", 24.0), ("linear_phase", 24.0),
])
def test_side_attenuation_one_octave_below_fc(variant, min_attenuation_db):
    f = np.array([FC / 2])
    _, h_side = bass_mono.frequency_responses(variant, FC, FS, f)
    assert -20 * np.log10(np.abs(h_side[0])) > min_attenuation_db


def test_process_matches_frequency_response():
    rng = np.random.default_rng(3)
    mid, side = rng.standard_normal(FS), rng.standard_normal(FS)
    impulse = np.zeros(4096)
    impulse[0] = 1.0
    m_ir, s_ir = bass_mono.process(impulse, impulse, FS, FC, "lr4_allpass")
    f = np.array([60.0, 240.0, 1000.0])
    h_mid, h_side = bass_mono.frequency_responses("lr4_allpass", FC, FS, f)
    assert signal.freqz(m_ir, worN=f, fs=FS)[1] == pytest.approx(h_mid, abs=1e-4)
    assert signal.freqz(s_ir, worN=f, fs=FS)[1] == pytest.approx(h_side, abs=1e-4)
    m_out, _ = bass_mono.process(mid, side, FS, FC, "lr4_allpass")
    assert np.sum(m_out**2) == pytest.approx(np.sum(mid**2), rel=0.01)

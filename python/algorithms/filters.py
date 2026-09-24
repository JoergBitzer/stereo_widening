"""Biquad filter designs shared by the algorithm references.

Coefficients after R. Bristow-Johnson, "Cookbook formulae for audio EQ biquad
filter coefficients". The C++ implementation uses the same formulas, so Python
and C++ can be compared sample by sample.

Every design function returns (b, a) with a[0] = 1, ready for scipy.signal.lfilter.
"""

import numpy as np


def _normalize(b, a):
    b = np.asarray(b, dtype=np.float64)
    a = np.asarray(a, dtype=np.float64)
    return b / a[0], a / a[0]


def highpass(fc_hz, fs, q=1 / np.sqrt(2)):
    """2nd-order high-pass. q = 1/sqrt(2) gives a Butterworth response."""
    w0 = 2 * np.pi * fc_hz / fs
    alpha = np.sin(w0) / (2 * q)
    cos_w0 = np.cos(w0)
    b = [(1 + cos_w0) / 2, -(1 + cos_w0), (1 + cos_w0) / 2]
    a = [1 + alpha, -2 * cos_w0, 1 - alpha]
    return _normalize(b, a)


def lowpass(fc_hz, fs, q=1 / np.sqrt(2)):
    """2nd-order low-pass. q = 1/sqrt(2) gives a Butterworth response."""
    w0 = 2 * np.pi * fc_hz / fs
    alpha = np.sin(w0) / (2 * q)
    cos_w0 = np.cos(w0)
    b = [(1 - cos_w0) / 2, 1 - cos_w0, (1 - cos_w0) / 2]
    a = [1 + alpha, -2 * cos_w0, 1 - alpha]
    return _normalize(b, a)


def high_shelf(fc_hz, gain_db, fs, slope=1.0):
    """2nd-order high shelf. gain_db above fc_hz, 0 dB below. slope = 1: steepest without overshoot."""
    big_a = 10 ** (gain_db / 40)
    w0 = 2 * np.pi * fc_hz / fs
    cos_w0 = np.cos(w0)
    alpha = np.sin(w0) / 2 * np.sqrt((big_a + 1 / big_a) * (1 / slope - 1) + 2)
    two_sqrt_a_alpha = 2 * np.sqrt(big_a) * alpha
    b = [big_a * ((big_a + 1) + (big_a - 1) * cos_w0 + two_sqrt_a_alpha),
         -2 * big_a * ((big_a - 1) + (big_a + 1) * cos_w0),
         big_a * ((big_a + 1) + (big_a - 1) * cos_w0 - two_sqrt_a_alpha)]
    a = [(big_a + 1) - (big_a - 1) * cos_w0 + two_sqrt_a_alpha,
         2 * ((big_a - 1) - (big_a + 1) * cos_w0),
         (big_a + 1) - (big_a - 1) * cos_w0 - two_sqrt_a_alpha]
    return _normalize(b, a)

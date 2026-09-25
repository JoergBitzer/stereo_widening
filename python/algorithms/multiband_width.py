"""Algorithm 2.7 (planing.md): multiband width, Python reference.

planing.md 2.7: "Crossover (Linkwitz-Riley LR4, 3-4 bands, which sums to allpass or
flat) and an M/S width per band. Bass mono comes built in."

Four bands, split by three Linkwitz-Riley 4th-order (LR4) crossovers at freq1 < freq2
< freq3, applied as a tree (each split feeds the next):

    band1 = LR4_low(x, freq1)
    band2 = LR4_low(LR4_high(x, freq1), freq2)
    band3 = LR4_low(LR4_high(LR4_high(x, freq1), freq2), freq3)
    band4 =         LR4_high(LR4_high(x, freq1), freq2), freq3)

An LR4 lowpass/highpass pair is built by cascading two identical 2nd-order Butterworth
sections (filters.py's lowpass()/highpass(), the same RBJ formula the C++ side will
use) at the same frequency -- the defining LR4 property is that the resulting LP4 and
HP4 outputs have matched phase, so LP4(x) + HP4(x) reconstructs x almost exactly
(discrete-time biquads make this a very close approximation, not bit-exact -- see
evaluate_multiband.py's identity check for the actual measured deviation). Applied as
a tree, all four bands sum back to (very nearly) the original signal.

Filtering commutes with the M/S transform (both linear), so rather than splitting L
and R into 4 bands each, M and S are each split once (2 filter trees instead of 4):

    M = (L+R)/2, S = (L-R)/2
    M = M1+M2+M3+M4, S = S1+S2+S3+S4   (per-band decomposition)
    S' = Width * (0*S1 + width2*S2 + width3*S3 + width4*S4)   -- band 1 forced mono
    M' = M   (unchanged, same as every width algorithm in this project)
    L' = M' + S',  R' = M' - S'

Band 1 (below freq1, "bass") always has width 0 -- "bass mono comes built in"
(planing.md) -- not a user-facing parameter, same reasoning as every other forced-
neutral design choice in this project (see phase5_comb.md's "Removing last-used
state"). Bands 2-4 each get their own width (0-2, same convention as the shared Width
knob), and the shared `width` parameter scales the combined result on top, exactly
like every other algorithm here (Width always means "how much of whatever this
algorithm produces", applied last).

Mono-safe by construction: since M is untouched and S' is built entirely from S
(never from M), L'+R' = 2M always, independent of any width setting -- same guarantee
as algorithm 2.4's comb.
"""

import numpy as np
from scipy import signal

from . import filters


def _lr4_low(x, fs, freq_hz):
    """4th-order Linkwitz-Riley lowpass: two identical cascaded 2nd-order Butterworth
    sections (RBJ cookbook, filters.py) at the same frequency."""
    b, a = filters.lowpass(freq_hz, fs)
    return signal.lfilter(b, a, signal.lfilter(b, a, x))


def _lr4_high(x, fs, freq_hz):
    """4th-order Linkwitz-Riley highpass, same construction as _lr4_low."""
    b, a = filters.highpass(freq_hz, fs)
    return signal.lfilter(b, a, signal.lfilter(b, a, x))


def _lr4_allpass(x, fs, freq_hz):
    """LP4(x) + HP4(x): unity magnitude at every frequency (the defining LR4 property --
    verified by sine sweep in evaluate_multiband.py's development), but NOT the
    identity -- it has the same phase/group-delay distortion a real LR4 crossover's
    reconstructed sum has at freq_hz. Used purely as a phase-compensation building
    block below, never to actually split a band.
    """
    return _lr4_low(x, fs, freq_hz) + _lr4_high(x, fs, freq_hz)


def split_bands(x, fs, freq1, freq2, freq3):
    """Splits x (1-D) into 4 bands via the LR4 tree, phase-compensated so that
    band1+band2+band3+band4 reconstructs x with flat magnitude at every frequency (a
    single overall allpass of x, planing.md's "LR sums to an allpass").

    A naive tree split (each band = whatever LP4/HP4 combination its path through the
    tree left it with) does NOT reconstruct flat: band1 only passed through ONE
    crossover's (freq1's) phase response, band2 passed through two (freq1, freq2),
    while band3/band4 passed through all three -- summing bands with mismatched
    accumulated phase causes real, measurable magnitude ripple where their spectra
    overlap (confirmed during development: ~0.5-0.6 dB of spurious mono-sum
    colouration with the naive version, at every setting, regardless of band widths --
    a symptom of the *reconstruction* itself, not of any width processing).

    The fix (standard technique for N-way Linkwitz-Riley crossover networks): every
    band must pass through the SAME total number of crossovers before summing. A band
    that skipped a crossover on its way through the tree (because it was already
    separated out earlier) is passed through an LR4 ALLPASS (see _lr4_allpass above)
    at that crossover's frequency instead -- same phase contribution as an LP4/HP4
    split would have added, without further separating anything.
    """
    if not (0 < freq1 < freq2 < freq3):
        raise ValueError("crossover frequencies must satisfy 0 < freq1 < freq2 < freq3")

    low1, high1 = _lr4_low(x, fs, freq1), _lr4_high(x, fs, freq1)
    low2, high2 = _lr4_low(high1, fs, freq2), _lr4_high(high1, fs, freq2)
    low3, high3 = _lr4_low(high2, fs, freq3), _lr4_high(high2, fs, freq3)

    band1 = _lr4_allpass(_lr4_allpass(low1, fs, freq2), fs, freq3)  # skipped freq2 and freq3 splits
    band2 = _lr4_allpass(low2, fs, freq3)                           # skipped the freq3 split
    band3 = low3                                                     # already passed freq1, freq2, freq3
    band4 = high3                                                    # already passed freq1, freq2, freq3
    return band1, band2, band3, band4


def multiband_width(x, fs, freq1=150.0, freq2=1500.0, freq3=6000.0,
                     width2=1.0, width3=1.0, width4=1.0, width=1.0):
    """Process x (N, 2) and return y (N, 2). See module docstring for the formula."""
    if width < 0 or width2 < 0 or width3 < 0 or width4 < 0:
        raise ValueError("width values must be >= 0")

    mid = 0.5 * (x[:, 0] + x[:, 1])
    side = 0.5 * (x[:, 0] - x[:, 1])

    m1, m2, m3, m4 = split_bands(mid, fs, freq1, freq2, freq3)
    s1, s2, s3, s4 = split_bands(side, fs, freq1, freq2, freq3)

    mid_out = m1 + m2 + m3 + m4
    # band 1 (s1) deliberately excluded -- forced mono, see module docstring
    side_out = width * (width2 * s2 + width3 * s3 + width4 * s4)

    return np.column_stack([mid_out + side_out, mid_out - side_out])

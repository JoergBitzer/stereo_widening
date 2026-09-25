"""Algorithm 2.5 (planing.md): allpass-cascade decorrelation, Python reference.

planing.md 2.5 describes two closely related techniques under one entry:
- filter L and R (or a mono source feeding both channels) through *different* allpass
  cascades -- magnitude is unchanged per channel, only phase differs, so the channels
  decorrelate; and
- the "common approach" S' = S + k*AP(M), injecting an allpass-filtered copy of the mid
  signal into the side signal.

The second formula alone (M left untouched, only S modified) would be perfectly
mono-safe by construction, same as algorithm 2.4's comb -- but planing.md explicitly
lists "the mono sum is *not* flat" as this algorithm's own con, which only follows from
the first technique (M *itself* differently phase-shifted per channel). This reference
therefore implements the first technique, generalised to also work as a genuine "add
on top of existing stereo content" tool (planing.md's group S), not just mono->stereo
(group P):

    Y1 = AP1(M), Y2 = AP2(M)   -- two different allpass cascades applied to the mid
                                   signal M = (L+R)/2. Each cascade preserves M's
                                   magnitude spectrum exactly (allpass property,
                                   |H(f)| = 1 at every frequency); AP1 and AP2 differ
                                   only in their pole/section frequencies, so Y1 and Y2
                                   have the same magnitude spectrum as M but different
                                   phase -- genuine decorrelation with no comb-filter
                                   magnitude colouration in the M-derived component
                                   itself.
    L' = L + amount * (Y1 - M)
    R' = R + amount * (Y2 - M)

At amount = 0 this is an exact bypass (L' = L, R' = R). At amount = 1, for a dual-mono
input (L = R = M, so S = 0), L' = Y1 and R' = Y2 exactly -- each channel is *purely* an
allpass-filtered copy of M, so each channel's own magnitude spectrum exactly equals
M's (planing.md's stated "pro"). At intermediate amounts, or with existing stereo
content (S != 0) mixed in, the *sum* L' + R' = (L+R) + amount*(Y1+Y2-2M) generally
differs from L+R -- Y1 and Y2 have the same magnitude as M but different phase, so
summing two differently-phase-shifted copies of the same signal causes frequency-
dependent constructive/destructive interference, exactly planing.md's stated con ("the
mono sum is not flat"). Verified numerically in evaluate_allpass.py.

`width` (shared across every algorithm, applied last, same convention as every other
algorithm in this project) scales the resulting side signal S' = (L'-R')/2 on top of
the above; the mid M' = (L'+R')/2 is passed through unscaled, same as every other
algorithm.

Parameters, per the project's "2 + Width" control-minimisation convention (see
algorithms/comb.py):
- amount (0-1, GUI 0-100 %): wet amount of the decorrelated component. 0 is neutral/
  bypass -- the correct default (see StereoWidener's neutral-defaults convention,
  phase5_comb.md's "Removing last-used state" section).
- spread_octaves: how far apart AP2's cascade frequencies sit from AP1's own (in
  octaves). 0 makes AP1 == AP2 (no decorrelation at any amount); larger values spread
  the two cascades' phase responses further apart, both stronger decorrelation and
  stronger mono-sum colouration -- a genuine trade-off knob, not a "more is free" one.

Reference: general "decorrelation filter" technique described in planing.md 2.5;
J. S. Kendall, "The Decorrelation of Audio Signals and Its Impact on Spatial Imagery",
Computer Music Journal, 1995, is the classic citation for this family of techniques.
"""

import numpy as np
from scipy import signal

from . import filters

# Base (AP1) cascade centre frequencies: 4 stages, geometrically spaced across the
# musically relevant range. AP2's frequencies are these, shifted up by spread_octaves.
_BASE_FREQS_HZ = [200.0, 700.0, 2400.0, 8000.0]
_Q = 1 / np.sqrt(2)  # Butterworth-flat-group-delay-ish allpass section, same as filters.allpass's own default


def _cascade_sos_like(freqs_hz, fs):
    """Returns a list of (b, a) biquad sections for a cascade of allpasses at freqs_hz."""
    return [filters.allpass(f, fs, _Q) for f in freqs_hz]


def _apply_cascade(x, sections):
    y = x
    for b, a in sections:
        y = signal.lfilter(b, a, y)
    return y


def allpass_decorrelate(x, fs, amount=0.5, spread_octaves=1.0, width=1.0):
    """Process x (N, 2) and return y (N, 2). See module docstring for the formula."""
    if not 0.0 <= amount <= 1.0:
        raise ValueError("amount must be in 0..1")
    if spread_octaves < 0:
        raise ValueError("spread_octaves must be >= 0")
    if width < 0:
        raise ValueError("width must be >= 0")

    mid = 0.5 * (x[:, 0] + x[:, 1])

    # Clamped below Nyquist with margin: the RBJ cookbook biquad formulas (filters.py)
    # are only valid for w0 < pi (fc < fs/2), and get numerically unstable approaching
    # it. spread_octaves shifting a high base frequency (8000 Hz) up by 2 octaves would
    # reach 32 kHz -- above Nyquist at 44.1/48 kHz -- without this clamp.
    nyquist_margin_hz = 0.45 * fs
    freqs1 = [min(f, nyquist_margin_hz) for f in _BASE_FREQS_HZ]
    freqs2 = [min(f * (2.0 ** spread_octaves), nyquist_margin_hz) for f in _BASE_FREQS_HZ]
    y1 = _apply_cascade(mid, _cascade_sos_like(freqs1, fs))
    y2 = _apply_cascade(mid, _cascade_sos_like(freqs2, fs))

    l_out = x[:, 0] + amount * (y1 - mid)
    r_out = x[:, 1] + amount * (y2 - mid)

    mid_out = 0.5 * (l_out + r_out)
    side_out = width * 0.5 * (l_out - r_out)
    return np.column_stack([mid_out + side_out, mid_out - side_out])

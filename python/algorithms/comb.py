"""Algorithm 2.4: Complementary comb filter pseudo-stereo (Lauridsen/Schroeder), Python
reference.

Signal flow (the C++ class StereoWidener/algorithms/ComplementaryComb uses the same
order):

    M = (L + R)/2             S = (L - R)/2
    Sc = HighPass(M, crossover_hz)[n - delay_samples]   (a delayed, filtered copy of M)
    S' = width * (S + gain * Sc)
    L' = M + S'                R' = M - S'

Properties:
- M is never touched, so L' + R' = 2M always: perfectly mono-compatible by
  construction, independent of delay_ms, gain, or width (planing.md 2.4: "The combs
  are complementary ... The mono sum is perfectly clean").
- The high-pass on the delayed contribution (not on S or M themselves) is planing.md's
  own suggested improvement ("apply only above ~300 Hz"): low frequencies carry most of
  a mix's energy and are the most audible as "phasiness"/comb-filtering colouration, so
  they are excluded from the added pseudo-stereo content -- only the highs get widened.
- Each channel on its own does get real comb-filtering colouration (a notch pattern in
  its spectrum), especially audible on headphones -- the known trade-off of this
  technique, not a bug.
- width = 0 collapses to mono (like every other algorithm here); width = 1 leaves the
  input's own S untouched and adds the comb term at gain; width = 2 doubles both.

Reference: M. R. Schroeder, "An Artificial Stereophonic Effect Obtained from a Single
Audio Signal", J. Audio Eng. Soc., 1958.
"""

import numpy as np
from scipy import signal

from . import filters


def comb(x, fs, delay_ms=10.0, gain=0.5, width=1.0, crossover_hz=300.0):
    """Process x (N, 2) and return y (N, 2).

    delay_ms      comb delay in milliseconds (planing.md: typically 5-20 ms)
    gain          0..1, depth of the delayed contribution added to the side signal
                  (planing.md: typically 0.3-0.7)
    width         0..2 (0 = mono, 1 = unchanged input S plus the comb term, 2 = doubled)
    crossover_hz  high-pass corner applied to the delayed contribution before it is
                  added to S; 0 (or None) disables the crossover entirely
    """
    if delay_ms < 0:
        raise ValueError("delay_ms must be >= 0")
    if width < 0:
        raise ValueError("width must be >= 0")

    mid = 0.5 * (x[:, 0] + x[:, 1])
    side = 0.5 * (x[:, 0] - x[:, 1])

    delay_samples = int(round(delay_ms * 1e-3 * fs))
    delayed_mid = np.concatenate([np.zeros(delay_samples), mid])[: len(mid)]

    if crossover_hz:
        b, a = filters.highpass(crossover_hz, fs)
        delayed_mid = signal.lfilter(b, a, delayed_mid)

    side_out = width * (side + gain * delayed_mid)
    return np.column_stack([mid + side_out, mid - side_out])

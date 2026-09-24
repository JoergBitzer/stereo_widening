"""Algorithm 2.1: Mid/Side width with bass mono and side shelf (Python reference).

Signal flow (the C++ class StereoWidener/algorithms/MSWidth uses the same order):

    M = (L + R)/2            S = (L - R)/2
                             S = HighShelf(S, side_shelf_hz, side_shelf_db)
                             S = HighPass(S, bass_mono_hz)      (bass mono, optional)
                             S = width * S
    optional level compensation g applied to M and S
    L' = M + S               R' = M - S

Properties:
- The mono sum L' + R' = 2M does not depend on any parameter (except the
  compensation gain): M/S width is mono-compatible by construction.
- width = 0 gives mono, width = 1 with all other parameters neutral gives L' = L, R' = R.
- Mono input (S = 0) is not changed: M/S width cannot create stereo.

Level compensation "constant_power": g = sqrt(2 / (1 + width^2)).
For M and S of equal power (e.g. uncorrelated L/R) the output power stays constant.
"""

import numpy as np
from scipy import signal

from . import filters


def ms_width(x, fs, width=1.0, bass_mono_hz=None, side_shelf_db=0.0, side_shelf_hz=3000.0,
             compensation="none"):
    """Process x (N, 2) and return y (N, 2).

    width          0 ... 2 (0 = mono, 1 = unchanged, 2 = side signal doubled)
    bass_mono_hz   None (off) or cut-off frequency of the side high-pass (e.g. 40 ... 300 Hz)
    side_shelf_db  gain of the high shelf in the side channel (0 dB = off)
    side_shelf_hz  corner frequency of the side shelf
    compensation   "none" or "constant_power"
    """
    if width < 0:
        raise ValueError("width must be >= 0")

    mid = 0.5 * (x[:, 0] + x[:, 1])
    side = 0.5 * (x[:, 0] - x[:, 1])

    if side_shelf_db != 0.0:
        b, a = filters.high_shelf(side_shelf_hz, side_shelf_db, fs)
        side = signal.lfilter(b, a, side)

    if bass_mono_hz is not None:
        b, a = filters.highpass(bass_mono_hz, fs)
        side = signal.lfilter(b, a, side)

    side = width * side

    if compensation == "constant_power":
        g = np.sqrt(2.0 / (1.0 + width**2))
        mid, side = g * mid, g * side
    elif compensation != "none":
        raise ValueError(f"unknown compensation '{compensation}'")

    return np.column_stack([mid + side, mid - side])

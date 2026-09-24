"""Algorithm 2.1: Mid/Side width with bass mono and side shelf (Python reference).

Signal flow (the C++ class StereoWidener/algorithms/MSWidth uses the same order):

    M = (L + R)/2            S = (L - R)/2
                             S = HighShelf(S, side_shelf_hz, side_shelf_db)
    bass mono (optional, see bass_mono.py; default "lr4_allpass"):
    M = AP2(M)               S = HP4(S)          (LR4 high-pass, allpass on M keeps the phase aligned)
                             S = width * S
    optional level compensation g applied to M and S
    L' = M + S               R' = M - S

Properties:
- The mono sum L' + R' = 2M' only depends on M. Without bass mono it is unchanged;
  with bass mono M passes an allpass (or a delay), so its magnitude spectrum is
  unchanged. M/S width is mono-compatible by construction.
- width = 0 gives mono, width = 1 with all other parameters neutral gives L' = L, R' = R.
  With bass mono "lr4_allpass" and width = 1, the output above fc is the input through
  a common allpass (same phase in L and R, stereo image unchanged).
- Mono input (S = 0) is not changed: M/S width cannot create stereo.

Level compensation "constant_power": g = sqrt(2 / (1 + width^2)).
For M and S of equal power (e.g. uncorrelated L/R) the output power stays constant.
"""

import numpy as np
from scipy import signal

from . import bass_mono, filters


def ms_width(x, fs, width=1.0, bass_mono_hz=None, side_shelf_db=0.0, side_shelf_hz=3000.0,
             compensation="none", bass_mono_mode="lr4_allpass"):
    """Process x (N, 2) and return y (N, 2).

    width          0 ... 2 (0 = mono, 1 = unchanged, 2 = side signal doubled)
    bass_mono_hz   None (off) or crossover frequency of the bass mono (e.g. 40 ... 300 Hz)
    side_shelf_db  gain of the high shelf in the side channel (0 dB = off)
    side_shelf_hz  corner frequency of the side shelf
    compensation   "none" or "constant_power"
    bass_mono_mode one of bass_mono.VARIANTS
    """
    if width < 0:
        raise ValueError("width must be >= 0")

    mid = 0.5 * (x[:, 0] + x[:, 1])
    side = 0.5 * (x[:, 0] - x[:, 1])

    if side_shelf_db != 0.0:
        b, a = filters.high_shelf(side_shelf_hz, side_shelf_db, fs)
        side = signal.lfilter(b, a, side)

    if bass_mono_hz is not None:
        mid, side = bass_mono.process(mid, side, fs, bass_mono_hz, bass_mono_mode)

    side = width * side

    if compensation == "constant_power":
        g = np.sqrt(2.0 / (1.0 + width**2))
        mid, side = g * mid, g * side
    elif compensation != "none":
        raise ValueError(f"unknown compensation '{compensation}'")

    return np.column_stack([mid + side, mid - side])

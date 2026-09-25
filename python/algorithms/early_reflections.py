"""Algorithm 2.12 (planing.md): early-reflection / room widening, Python reference.

planing.md 2.12: "Add a few short, *decorrelated* early reflections (different for L
and R, within 5-40 ms, low level). The effect is apparent source width (ASW), known
from room acoustics." Group S+P (works on existing stereo content and creates pseudo-
stereo from mono), mono-compatibility "o" (partial, like algorithm 2.5's allpass
decorrelation -- not mono-safe by construction, see below).

A small, fixed number of delayed, decreasing-gain copies of the mid signal M are added
to each channel, using a DIFFERENT set of delay times per channel (the actual source of
decorrelation/width -- "different for L and R"):

    Y_L = sum_k gain[k] * M[n - delay_L[k]]
    Y_R = sum_k gain[k] * M[n - delay_R[k]]
    L' = L + amount * Y_L
    R' = R + amount * Y_R

Same structural pattern as algorithm 2.5's allpass decorrelation (L' = L + amount*(...),
see algorithms/allpass_decorrelation.py) rather than algorithm 2.4's comb (S' = S +
gain*...): adding a DIFFERENT reflection pattern to L than to R means L'+R' generally
does NOT equal L+R once amount > 0 (two different sets of delayed copies of M, summed,
don't cancel back to 2M) -- this is exactly planing.md's own "o" (partial) mono-
compatibility rating for this algorithm, the same trade-off allpass decorrelation makes
for the same underlying reason (a technique that decorrelates channels by construction
cannot also leave the mono sum untouched).

Tap layout, per channel: kNumReflections (5) delays, at irregular (non-harmonic, to
avoid audible periodicity/metallic ringing) fractional positions within a "spread"
window whose width is controlled by room_size (small room = tight cluster of early
reflections, large room = wider spread, further into planing.md's suggested 5-40 ms
range), offset by a shared pre_delay (the time before the first reflection arrives --
mimics the source-to-wall distance, a GlobalSettings default in the plugin, not a
user-facing knob, see docs/algorithms for the "2 + Width" convention this follows).
Gains decrease with each successive (later) tap, the same "early reflections trail off"
shape real room acoustics have.

Parameters, per the project's "2 + Width" control-minimisation convention:
- amount (0-1, GUI 0-100 %): 0 is an exact, algebraically neutral bypass -- the correct
  default (see phase5_comb.md's "Removing last-used state" for why every algorithm's
  default must be neutral).
- room_size (0-1, GUI 0-100 %): no neutral value of its own -- inert whenever
  amount = 0, same reasoning as comb's Delay/allpass's Spread defaults.
- pre_delay_ms: not a user-facing parameter in the plugin (a GlobalSettings default,
  like comb's crossover frequency); kept as a function parameter here for evaluation
  flexibility.

Reference: apparent source width (ASW) via early lateral reflections is standard room-
acoustics/concert-hall literature; see e.g. L. Beranek, "Concert Halls and Opera
Houses: Music, Acoustics, and Architecture", 2nd ed., Springer, 2004, ch. 2.
"""

import numpy as np

kNumReflections = 5

# Fractional tap positions within the room-size spread window (0..1), irregular and
# DIFFERENT per channel -- the actual source of L/R decorrelation. Deliberately
# non-harmonic spacing (not evenly spaced) to avoid the taps themselves creating an
# audible comb-filter periodicity.
_L_FRACTIONS = [0.06, 0.24, 0.43, 0.66, 0.90]
_R_FRACTIONS = [0.11, 0.30, 0.52, 0.74, 0.97]

# Per-tap gain, same profile for both channels (only the delay times differ) -- later
# (larger-index) taps are quieter, matching a real room's decaying reflection pattern.
_BASE_GAIN = 0.45
_GAIN_DECAY = 0.7

# Room-size (0-100 %) maps to a spread window this wide, in ms -- small room clusters
# reflections tightly and early, large room spreads them further into planing.md's
# suggested 5-40 ms window.
_ROOM_MIN_SPREAD_MS = 8.0
_ROOM_MAX_SPREAD_MS = 32.0


def _tap_gains():
    return [_BASE_GAIN * (_GAIN_DECAY ** k) for k in range(kNumReflections)]


def _tap_delays_ms(fractions, room_size, pre_delay_ms):
    spread_ms = _ROOM_MIN_SPREAD_MS + room_size * (_ROOM_MAX_SPREAD_MS - _ROOM_MIN_SPREAD_MS)
    return [pre_delay_ms + f * spread_ms for f in fractions]


def _sum_reflections(mid, fs, delays_ms, gains):
    # Fractional (not rounded-to-integer-sample) delay, linearly interpolated between
    # the two neighbouring integer-sample taps -- matches the C++ plugin's
    # juce::dsp::DelayLine<float, DelayLineInterpolationTypes::Linear> exactly (see
    # StereoWidener/algorithms/EarlyReflections.cpp), rather than rounding each tap to
    # the nearest whole sample as algorithm 2.4's comb reference does. With 5 taps per
    # channel summed into a dense comb filter, a sub-sample rounding difference shifts
    # notch/peak positions enough to blow up the mono_coloration_dB cross-check metric
    # (a max-abs-per-band measure, very sensitive near notches) even though it is
    # negligible for every other metric -- see python/crosscheck_early_reflections.py.
    out = np.zeros_like(mid)
    for delay_ms, gain in zip(delays_ms, gains):
        delay_samples = delay_ms * 1e-3 * fs
        floor_delay = int(np.floor(delay_samples))
        frac = delay_samples - floor_delay
        near = np.concatenate([np.zeros(floor_delay), mid])[: len(mid)]
        far = np.concatenate([np.zeros(floor_delay + 1), mid])[: len(mid)]
        delayed = (1.0 - frac) * near + frac * far
        out += gain * delayed
    return out


def early_reflections(x, fs, amount=0.5, room_size=0.5, pre_delay_ms=5.0):
    """Process x (N, 2) and return y (N, 2). See module docstring for the formula."""
    if not 0.0 <= amount <= 1.0:
        raise ValueError("amount must be in 0..1")
    if not 0.0 <= room_size <= 1.0:
        raise ValueError("room_size must be in 0..1")
    if pre_delay_ms < 0:
        raise ValueError("pre_delay_ms must be >= 0")

    mid = 0.5 * (x[:, 0] + x[:, 1])
    gains = _tap_gains()

    refl_l = _sum_reflections(mid, fs, _tap_delays_ms(_L_FRACTIONS, room_size, pre_delay_ms), gains)
    refl_r = _sum_reflections(mid, fs, _tap_delays_ms(_R_FRACTIONS, room_size, pre_delay_ms), gains)

    l_out = x[:, 0] + amount * refl_l
    r_out = x[:, 1] + amount * refl_r
    return np.column_stack([l_out, r_out])

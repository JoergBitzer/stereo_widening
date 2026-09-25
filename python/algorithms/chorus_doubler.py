"""Algorithm 2.11 (planing.md): micro-pitch / chorus doubler ("Dimension D" style),
Python reference.

planing.md 2.11: "L and R get slightly different short, modulated delays (5-30 ms,
LFO) ... the classic 'Dimension D' or 'micro shift' effect (Eventide H3000 style)."
Group S+P (works on existing stereo content and creates pseudo-stereo from mono),
mono-compatibility "-" -- worse than algorithm 2.5's allpass decorrelation or 2.12's
early reflections (both "o"), matching planing.md's own con: "mono sum shows comb and
flanging artefacts."

Per explicit user decision, this implements only the modulated-delay chorus/
Dimension-D member of planing.md 2.11's family, not the fixed-cent micro-pitch-shift
variant (which needs a structurally different ramp/sawtooth-LFO, crossfading delay
line -- a real redesign, not a parameter change, and out of scope here).

The mid signal M is fed through two independently LFO-modulated delay lines, one per
output channel, with the two LFOs held a constant quarter-cycle (90 deg, "quadrature")
apart -- the actual source of L/R decorrelation, same reason allpass decorrelation
uses two different cascades and early reflections uses two different tap patterns:

    delay_L(t) = kBaseDelayMs + depth * kMaxDepthMs * sin(2*pi*rate*t)
    delay_R(t) = kBaseDelayMs + depth * kMaxDepthMs * sin(2*pi*rate*t + kStereoPhaseOffset)
    Y_L[n] = M[n - delay_L(n)]  (fractional/interpolated read) - M[n]
    Y_R[n] = M[n - delay_R(n)]  (fractional/interpolated read) - M[n]
    L' = L + amount * Y_L
    R' = R + amount * Y_R

Same structural pattern as algorithms 2.5/2.12 (`L' = L + amount*(...)`, add directly
to L/R) rather than 2.4's comb (`S' = S + gain*...`): two different, continuously
time-varying delay patterns added to L and R means L'+R' generally does NOT equal
L+R once amount > 0 -- planing.md's own "-" mono-compatibility rating (WORSE than
2.5/2.12's "o": here the interference pattern between L and R is itself constantly
sweeping with the LFO, "flanging" through a range of comb-filter notch positions
over time, rather than sitting at one fixed set of notches).

At amount = 0 this is an exact bypass (L' = L, R' = R) -- the correct neutral default
(see phase5_comb.md's "Removing last-used state").

Parameters, per the project's "2 + Width" control-minimisation convention:
- amount (0-1, GUI 0-100 %): 0 is an exact, algebraically neutral bypass.
- depth (0-1, GUI 0-100 %): scales the LFO's modulation excursion. depth = 0 is a
  STATIC (non-swept) pair of delay taps kBaseDelayMs and kBaseDelayMs+kStereoOffsetMs
  apart -- a fixed, constant L/R separation that is deliberately NOT scaled by depth
  (see kStereoOffsetMs below), so L and R always stay genuinely different even with
  no modulation at all. No neutral value of its own -- inert whenever amount = 0,
  same reasoning as comb's Delay/allpass's Spread/early-reflections' Room Size
  defaults.
- rate (Hz): NOT a user-facing parameter in the plugin -- a GlobalSettings default
  (like comb's crossover frequency, early reflections' pre-delay), kept as a function
  parameter here for evaluation flexibility. Kept deliberately slow/"Dimension D"-like
  by default (a fast rate turns this into an obvious vibrato/warble, a different,
  arguably worse-sounding effect for a width tool); exposing it live risks users
  dialling in that worse-sounding regime, so it is fixed by design rather than a
  third knob.
- base delay (kBaseDelayMs), the fixed static L/R separation (kStereoOffsetMs, always
  present regardless of depth), max depth range (kMaxDepthMs), and the L/R LFO phase
  offset (kStereoPhaseOffsetRadians) are compiled-in constants, mirroring allpass's
  fixed cascade frequencies / early reflections' fixed tap-fraction pattern.

Reference: the "Dimension D" / stereo chorus family described in planing.md 2.11;
the underlying "modulated delay line" chorus technique is standard (e.g. Dattorro,
"Effect Design Part 2: Delay Line Modulation and Chorus", JAES 1997).
"""

import numpy as np

kBaseDelayMs = 15.0
kStereoOffsetMs = 3.0  # fixed L/R base separation, present even at depth = 0 -- see below
kMaxDepthMs = 10.0
kStereoPhaseOffsetRadians = np.pi / 2.0


def _modulated_delay_read(mid, fs, delay_samples):
    """Fractional, linearly-interpolated, time-varying delay read of `mid`.

    delay_samples: array, same length as mid, the (possibly per-sample-varying)
    delay in samples to apply at each output sample n. Matches the same linear-
    interpolation convention as early_reflections.py's _sum_reflections (and, in
    the C++ plugin, juce::dsp::DelayLine<float, DelayLineInterpolationTypes::Linear>):
    out[n] = (1-frac)*mid[n-floor_delay] + frac*mid[n-floor_delay-1], zero-padded
    before the start of the signal.
    """
    n = len(mid)
    floor_delay = np.floor(delay_samples).astype(int)
    frac = delay_samples - floor_delay

    idx_near = np.arange(n) - floor_delay
    idx_far = idx_near - 1

    near = np.where(idx_near >= 0, mid[np.clip(idx_near, 0, n - 1)], 0.0)
    far = np.where(idx_far >= 0, mid[np.clip(idx_far, 0, n - 1)], 0.0)
    return (1.0 - frac) * near + frac * far


def chorus_doubler(x, fs, amount=0.5, depth=0.5, rate=0.3):
    """Process x (N, 2) and return y (N, 2). See module docstring for the formula."""
    if not 0.0 <= amount <= 1.0:
        raise ValueError("amount must be in 0..1")
    if not 0.0 <= depth <= 1.0:
        raise ValueError("depth must be in 0..1")
    if rate <= 0.0:
        raise ValueError("rate must be > 0")

    n = len(x)
    mid = 0.5 * (x[:, 0] + x[:, 1])
    t = np.arange(n) / fs
    phase = 2.0 * np.pi * rate * t

    excursion_samples = depth * kMaxDepthMs * 1e-3 * fs
    base_samples = kBaseDelayMs * 1e-3 * fs
    stereo_offset_samples = kStereoOffsetMs * 1e-3 * fs

    # kStereoOffsetMs is NOT scaled by depth: L and R must stay genuinely different at
    # every depth setting, including depth=0 -- without it, depth=0 collapses the LFO
    # excursion for BOTH channels to exactly the same constant (base_samples), so L and
    # R would read the identical delayed copy of M. That isn't "no chorus", it's a
    # single comb filter baked identically into both channels (no complementary +/-
    # structure like ComplementaryComb, so each channel's own spectrum AND the mono sum
    # both take the full notch pattern) -- found via evaluate_chorus_doubler.py showing
    # depth=0 as the *worst* setting for dLUFS/mono coloration, backwards from what a
    # "turn depth down" control should do.
    delay_l = base_samples + excursion_samples * np.sin(phase)
    delay_r = base_samples + stereo_offset_samples + excursion_samples * np.sin(phase + kStereoPhaseOffsetRadians)

    y_l = _modulated_delay_read(mid, fs, delay_l) - mid
    y_r = _modulated_delay_read(mid, fs, delay_r) - mid

    l_out = x[:, 0] + amount * y_l
    r_out = x[:, 1] + amount * y_r
    return np.column_stack([l_out, r_out])

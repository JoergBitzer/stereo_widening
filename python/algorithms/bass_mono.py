"""Bass mono variants: remove the side signal below a crossover frequency fc.

Problem: a plain 2nd-order high-pass on S ("hp2") also shifts the phase of S
above fc, while M is unfiltered. For a panned source, L and R are then no longer
scaled copies of each other: the image smears (correlation < 1 far above fc).

Variants (all take M and S and return the processed M and S):

  "hp2"            S' = HP2(S),                 M' = M
                   2nd-order Butterworth high-pass. Phase error above fc.

  "complementary"  S' = S - LP2(S),             M' = M
                   no phase change far above fc, but only 6 dB/octave, and a
                   +2 dB bump near fc. Cheap, but a weak bass mono.

  "lr4_allpass"    S' = HP4(S),                 M' = AP2(M)
                   HP4 = Linkwitz-Riley high-pass (two cascaded Butterworth HP2).
                   AP2 = LP4 + HP4, a 2nd-order allpass (same fc, Q = 1/sqrt(2)).
                   Above fc, HP4 ~ AP2, so M and S get the same phase: the high
                   band is (M +- S) through a common allpass, which does not change
                   the stereo image. Equivalent to splitting L/R with an LR4 crossover
                   and making the low band mono. Zero latency, 3 biquads.

  "linear_phase"   S' = HP_lin(S),              M' = delay(M)
                   linear-phase FIR with the magnitude of LR4, HP_lin = delay - LP_lin.
                   No phase change at all, but latency (numtaps - 1)/2 samples.

In all variants the mono sum only depends on M', whose magnitude is unchanged
(allpass or delay), so bass mono never colours the mono sum.
"""

import numpy as np
from scipy import signal

from . import filters

VARIANTS = ("hp2", "complementary", "lr4_allpass", "linear_phase")
LINEAR_PHASE_TAPS = 8191  # (8191-1)/2 = 4095 samples = 93 ms latency at 44.1 kHz


def linear_phase_lowpass(fc_hz, fs, numtaps=LINEAR_PHASE_TAPS):
    """Linear-phase FIR with the magnitude of an LR4 low-pass: 1 / (1 + (f/fc)^4)."""
    f = np.linspace(0.0, fs / 2, 16385)  # 1.3 Hz grid, fine enough for fc >= 40 Hz
    gain = 1.0 / (1.0 + (f / fc_hz) ** 4)
    return signal.firwin2(numtaps, f, gain, fs=fs, window="blackman")


def latency_samples(variant, numtaps=LINEAR_PHASE_TAPS):
    return (numtaps - 1) // 2 if variant == "linear_phase" else 0


def filters_for(variant, fc_hz, fs):
    """Return (mid_filters, side_filters): lists of (b, a) applied in series.

    For "complementary" the side list describes the low-pass that is subtracted.
    """
    if variant == "hp2":
        return [], [filters.highpass(fc_hz, fs)]
    if variant == "complementary":
        return [], [filters.lowpass(fc_hz, fs)]
    if variant == "lr4_allpass":
        return [filters.allpass(fc_hz, fs)], [filters.highpass(fc_hz, fs), filters.highpass(fc_hz, fs)]
    if variant == "linear_phase":
        lp = linear_phase_lowpass(fc_hz, fs)
        delay = np.zeros(len(lp))
        delay[(len(lp) - 1) // 2] = 1.0
        return [(delay, np.array([1.0]))], [(delay - lp, np.array([1.0]))]
    raise ValueError(f"unknown bass mono variant '{variant}', use one of {VARIANTS}")


def _apply(chain, x):
    for b, a in chain:
        x = signal.lfilter(b, a, x) if len(a) > 1 else signal.fftconvolve(x, b)[: len(x)]
    return x


def process(mid, side, fs, fc_hz, variant="lr4_allpass"):
    """Apply bass mono to M and S. Returns (mid', side')."""
    mid_chain, side_chain = filters_for(variant, fc_hz, fs)
    if variant == "complementary":
        return mid, side - _apply(side_chain, side)
    return _apply(mid_chain, mid), _apply(side_chain, side)


def frequency_responses(variant, fc_hz, fs, f):
    """Complex responses (H_mid, H_side) at frequencies f, delay of linear phase removed."""
    mid_chain, side_chain = filters_for(variant, fc_hz, fs)

    def chain_response(chain):
        h = np.ones(len(f), dtype=complex)
        for b, a in chain:
            h *= signal.freqz(b, a, worN=f, fs=fs)[1]
        return h

    h_mid, h_side = chain_response(mid_chain), chain_response(side_chain)
    if variant == "complementary":
        h_side = 1.0 - h_side
    delay = latency_samples(variant)
    if delay:
        undo = np.exp(2j * np.pi * f / fs * delay)
        h_mid, h_side = h_mid * undo, h_side * undo
    return h_mid, h_side

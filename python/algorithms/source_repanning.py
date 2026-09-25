"""Algorithm 2.8 (planing.md): STFT-based panning expansion ("source re-panning").

Python-only PROTOTYPE for evaluation -- explicitly NOT part of the Phase 5 v1
algorithm set, and not held to this project's usual bar (no C++ class, no "2 + Width"
parameter minimisation, no exact-bypass/mono-safety verification). The point of this
module is only to produce real output audio and stereo_eval measurements so the
technique's actual quality/artifacts can be judged before committing to a full
redesign + C++ implementation -- planing.md itself flags this as the most
"intelligent" but also most complex/expensive technique in the catalogue.

planing.md 2.8: per time-frequency STFT bin, estimate a panning index (Avendano & Jot,
2004):

    psi(k) = sign(|X_L|-|X_R|) * (1 - 2*|X_L * conj(X_R)| / (|X_L|^2 + |X_R|^2))

(+1 = hard left, -1 = hard right, 0 = centre/dual-mono). Map it through an expansion
curve psi' = f(psi) (planing.md's example: "sources at 30% move to 60%", i.e. a
simple *2 gain, clamped to +-1 here), then re-synthesise each bin with new gains from
a constant-power pan law applied to psi', redistributing the bin's total energy
between channels while keeping each channel's OWN original phase:

    gL = sqrt((1+psi')/2),  gR = sqrt((1-psi')/2)
    mag = sqrt(|X_L|^2 + |X_R|^2)
    X_L' = gL * mag * exp(i*angle(X_L)),  X_R' = gR * mag * exp(i*angle(X_R))

NOT implemented here: planing.md's "extension" of primary-ambient decomposition
(coherence-based separation of direct vs. ambient content, to protect a centred
vocal from being pushed around by the same curve as everything else). That is a
materially different, more complex technique layered on top of this one -- worth
evaluating separately if this base version looks promising enough to justify it.

A key expected limitation, worth checking for explicitly in the evaluation: the
psi(k) formula models a single coherent point source per bin. Real program material
usually has multiple simultaneous sources and/or incoherent ambience sharing a bin, so
even `expansion = 1.0` (i.e. psi' = psi, "no expansion requested") is NOT expected to
be a clean bypass -- re-synthesising from a magnitude-only per-bin pan model discards
whatever inter-channel phase relationship isn't explained by that one-source-per-bin
assumption. If the evaluation shows large mono_coloration_dB / audible artefacts even
at expansion = 1.0, that is this limitation showing up, not an implementation bug.

STFT/ISTFT via scipy.signal (Hann analysis+synthesis window, 75% overlap, which
satisfies the constant-overlap-add condition scipy relies on for clean
reconstruction of anything that doesn't go through the pan remap).

Reference: C. Avendano, J.-M. Jot, "Frequency Domain Techniques for Stereo to
Multichannel Upmix," AES 22nd International Conference, 2002 (planing.md cites 2004;
the widely-cited version of this specific panning-index formula is the 2002 AES
paper).
"""

import numpy as np
from scipy.signal import stft, istft

DEFAULT_N_FFT = 2048
DEFAULT_HOP = DEFAULT_N_FFT // 4  # 75% overlap, COLA-valid for scipy's default Hann window


def _pan_index(XL, XR, eps=1e-12):
    cross = np.abs(XL * np.conj(XR))
    energy = np.abs(XL) ** 2 + np.abs(XR) ** 2
    psi = 1.0 - 2.0 * cross / (energy + eps)
    sign = np.sign(np.abs(XL) - np.abs(XR))
    sign[sign == 0] = 1.0
    return sign * psi


def source_repanning(x, fs, expansion=2.0, n_fft=DEFAULT_N_FFT, hop=None):
    """Process x (N, 2) and return y (N, 2). See module docstring for the formula.

    expansion: multiplies the estimated pan index before re-synthesis, clamped to
    +-1 (planing.md's own example, "sources at 30% move to 60%", is expansion=2.0).
    expansion=1.0 is "no remap requested" but is NOT an exact bypass -- see the
    module docstring's note on the one-coherent-source-per-bin limitation.
    """
    if hop is None:
        hop = n_fft // 4
    noverlap = n_fft - hop

    _, _, XL = stft(x[:, 0], fs=fs, nperseg=n_fft, noverlap=noverlap, boundary="zeros")
    _, _, XR = stft(x[:, 1], fs=fs, nperseg=n_fft, noverlap=noverlap, boundary="zeros")

    psi = _pan_index(XL, XR)
    psi_new = np.clip(psi * expansion, -1.0, 1.0)

    gL = np.sqrt(np.clip((1.0 + psi_new) / 2.0, 0.0, 1.0))
    gR = np.sqrt(np.clip((1.0 - psi_new) / 2.0, 0.0, 1.0))
    mag = np.sqrt(np.abs(XL) ** 2 + np.abs(XR) ** 2)

    XL_new = gL * mag * np.exp(1j * np.angle(XL))
    XR_new = gR * mag * np.exp(1j * np.angle(XR))

    _, l_out = istft(XL_new, fs=fs, nperseg=n_fft, noverlap=noverlap, boundary=True)
    _, r_out = istft(XR_new, fs=fs, nperseg=n_fft, noverlap=noverlap, boundary=True)

    n = len(x)
    l_out = l_out[:n] if len(l_out) >= n else np.pad(l_out, (0, n - len(l_out)))
    r_out = r_out[:n] if len(r_out) >= n else np.pad(r_out, (0, n - len(r_out)))
    return np.column_stack([l_out, r_out])

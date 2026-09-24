"""Objective stereo measures.

Notation: x has shape (N, 2), L = x[:, 0], R = x[:, 1],
M = (L + R)/2, S = (L - R)/2.

Per-band values are computed from Welch spectra (auto- and cross-power spectral
densities) that are summed inside 1/3-octave bands. The correlation at lag 0 in a
band is Re{sum S_LR} / sqrt(sum S_LL * sum S_RR).
"""

import numpy as np
import pyloudnorm
from scipy import signal

EPS = 1e-20
WELCH_NPERSEG = 16384  # 2.7 Hz resolution at 44.1 kHz, >= 2 bins in the 50 Hz band


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def mid_side(x):
    """Return (M, S) with M = (L+R)/2 and S = (L-R)/2."""
    return 0.5 * (x[:, 0] + x[:, 1]), 0.5 * (x[:, 0] - x[:, 1])


def to_db(power_ratio):
    return 10.0 * np.log10(np.maximum(power_ratio, EPS))


def rms_db(x):
    return to_db(np.mean(np.asarray(x) ** 2))


def third_octave_bands(f_low=50.0, f_high=16000.0):
    """Centre frequencies and edges of 1/3-octave bands (base-10, IEC 61260).

    Returns an array of shape (num_bands, 3): [f_lower, f_centre, f_upper].
    """
    k = np.arange(np.round(10 * np.log10(f_low / 1000.0)),
                  np.round(10 * np.log10(f_high / 1000.0)) + 1)
    fc = 1000.0 * 10.0 ** (k / 10.0)
    return np.column_stack([fc * 10 ** (-1 / 20), fc, fc * 10 ** (1 / 20)])


def _welch_spectra(x, fs):
    """Return f, S_LL, S_RR, S_LR (cross spectrum, complex)."""
    nperseg = min(WELCH_NPERSEG, len(x))
    f, s_ll = signal.welch(x[:, 0], fs, nperseg=nperseg)
    _, s_rr = signal.welch(x[:, 1], fs, nperseg=nperseg)
    _, s_lr = signal.csd(x[:, 0], x[:, 1], fs, nperseg=nperseg)
    return f, s_ll, s_rr, s_lr


def _band_sum(f, spectrum, bands):
    """Sum spectrum values inside each band. Bands without bins give NaN."""
    out = np.full(len(bands), np.nan, dtype=spectrum.dtype)
    for i, (f_lo, _, f_hi) in enumerate(bands):
        idx = (f >= f_lo) & (f < f_hi)
        if np.any(idx):
            out[i] = np.sum(spectrum[idx])
    return out


# ---------------------------------------------------------------------------
# correlation
# ---------------------------------------------------------------------------

def correlation(x):
    """Broadband correlation coefficient at lag 0 (what a correlation meter shows)."""
    left, right = x[:, 0], x[:, 1]
    denom = np.sqrt(np.sum(left**2) * np.sum(right**2))
    return float(np.sum(left * right) / denom) if denom > EPS else float("nan")


def correlation_over_time(x, fs, block_s=0.1, hop_s=0.05, silence_db=-70.0):
    """Block-wise correlation. Returns (t_centre_s, rho). Silent blocks are NaN."""
    block, hop = int(block_s * fs), int(hop_s * fs)
    starts = np.arange(0, max(len(x) - block, 0) + 1, hop)
    rho = np.full(len(starts), np.nan)
    for i, start in enumerate(starts):
        segment = x[start:start + block]
        if rms_db(segment) > silence_db:
            rho[i] = correlation(segment)
    return (starts + block / 2) / fs, rho


def band_correlation(x, fs, bands=None):
    """Correlation per 1/3-octave band. Returns (f_centre, rho)."""
    bands = third_octave_bands() if bands is None else bands
    f, s_ll, s_rr, s_lr = _welch_spectra(x, fs)
    p_ll = _band_sum(f, s_ll, bands)
    p_rr = _band_sum(f, s_rr, bands)
    p_lr = _band_sum(f, s_lr, bands)
    rho = np.real(p_lr) / np.sqrt(np.maximum(p_ll * p_rr, EPS))
    return bands[:, 1], rho


# ---------------------------------------------------------------------------
# levels
# ---------------------------------------------------------------------------

def levels(x, fs):
    """Levels in dB: L, R, M, S, overall RMS (all channels), side-to-mid ratio, LUFS."""
    mid, side = mid_side(x)
    result = {
        "L_dB": rms_db(x[:, 0]),
        "R_dB": rms_db(x[:, 1]),
        "M_dB": rms_db(mid),
        "S_dB": rms_db(side),
        "rms_dB": rms_db(x),
    }
    result["S_minus_M_dB"] = result["S_dB"] - result["M_dB"]
    result["LUFS"] = loudness_lufs(x, fs)
    return result


def loudness_lufs(x, fs):
    """Integrated loudness after ITU-R BS.1770 (needs at least 0.4 s of signal)."""
    if len(x) < int(0.4 * fs):
        return float("nan")
    return float(pyloudnorm.Meter(fs).integrated_loudness(x))


# ---------------------------------------------------------------------------
# mono compatibility
# ---------------------------------------------------------------------------

def band_levels_db(mono, fs, bands=None):
    """Power per 1/3-octave band in dB for a single-channel signal. Returns (f_centre, level)."""
    bands = third_octave_bands() if bands is None else bands
    nperseg = min(WELCH_NPERSEG, len(mono))
    f, s = signal.welch(mono, fs, nperseg=nperseg)
    return bands[:, 1], to_db(_band_sum(f, s, bands))


def mono_sum_coloration(x_in, x_out, fs, bands=None, silence_db=-100.0):
    """Compare the mono sum L+R of output and input per 1/3-octave band.

    Returns a dict with
      f_centre      band centre frequencies
      diff_dB       level(L'+R') - level(L+R) per band
      offset_dB     broadband level change of the mono sum
      coloration_dB max |diff - offset|: spectral change of the mono sum,
                    independent of a broadband gain change (0 dB = no colouration)
    Bands in which the input mono sum is below silence_db are ignored (NaN).
    """
    f_c, level_in = band_levels_db(x_in[:, 0] + x_in[:, 1], fs, bands)
    _, level_out = band_levels_db(x_out[:, 0] + x_out[:, 1], fs, bands)
    diff = level_out - level_in
    diff[level_in < silence_db] = np.nan
    offset = rms_db(x_out[:, 0] + x_out[:, 1]) - rms_db(x_in[:, 0] + x_in[:, 1])
    coloration = np.nanmax(np.abs(diff - offset)) if np.any(np.isfinite(diff)) else float("nan")
    return {"f_centre": f_c, "diff_dB": diff, "offset_dB": offset, "coloration_dB": float(coloration)}

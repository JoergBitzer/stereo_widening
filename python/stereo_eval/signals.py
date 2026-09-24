"""Building blocks for test signals.

Every function that uses random numbers takes a numpy Generator, so all
test signals are reproducible with a fixed seed.
"""

import numpy as np
from scipy import signal


def db_to_lin(db):
    return 10.0 ** (db / 20.0)


def pink_noise(num_samples, rng):
    """Pink noise (-3 dB/octave) by spectral shaping of white noise, normalised to RMS = 1."""
    white = rng.standard_normal(num_samples)
    spectrum = np.fft.rfft(white)
    k = np.arange(len(spectrum))
    k[0] = 1  # avoid division by zero; DC is removed below
    spectrum = spectrum / np.sqrt(k)
    spectrum[0] = 0.0
    pink = np.fft.irfft(spectrum, n=num_samples)
    return pink / np.sqrt(np.mean(pink**2))


def correlated_noise_pair(num_samples, rho, rng):
    """Two pink noise channels with correlation coefficient rho (0 <= rho <= 1).

    L = sqrt(rho)*c + sqrt(1-rho)*n1,  R = sqrt(rho)*c + sqrt(1-rho)*n2
    with c, n1, n2 independent and of unit power, so E{L*R} = rho.
    """
    if not 0.0 <= rho <= 1.0:
        raise ValueError("rho must be in [0, 1]; use polarity inversion for negative values")
    common = pink_noise(num_samples, rng)
    n1 = pink_noise(num_samples, rng)
    n2 = pink_noise(num_samples, rng)
    left = np.sqrt(rho) * common + np.sqrt(1.0 - rho) * n1
    right = np.sqrt(rho) * common + np.sqrt(1.0 - rho) * n2
    return np.column_stack([left, right])


def log_sweep(duration_s, fs, f_start=20.0, f_stop=20000.0, fade_s=0.01):
    """Exponential (log) sine sweep with short fades, peak amplitude 1."""
    t = np.arange(int(duration_s * fs)) / fs
    sweep = signal.chirp(t, f0=f_start, t1=duration_s, f1=f_stop, method="logarithmic", phi=-90)
    num_fade = int(fade_s * fs)
    fade = 0.5 * (1 - np.cos(np.pi * np.arange(num_fade) / num_fade))
    sweep[:num_fade] *= fade
    sweep[-num_fade:] *= fade[::-1]
    return sweep


def pan_constant_power(mono, pan):
    """Pan a mono signal. pan = -1 (hard left) ... 0 (centre) ... +1 (hard right).

    Sine/cosine law: equal power for all positions, -3 dB per channel in the centre.
    """
    angle = (pan + 1.0) * np.pi / 4.0
    return np.column_stack([np.cos(angle) * mono, np.sin(angle) * mono])


def synthetic_stereo_reverb_ir(fs, rt60_s, rng, length_s=None, predelay_s=0.01):
    """Stereo impulse response: exponentially decaying, independent noise per channel.

    The two channels are uncorrelated, so the reverb adds diffuse width.
    Each channel has unit energy.
    """
    if length_s is None:
        length_s = 1.5 * rt60_s
    num_samples = int(length_s * fs)
    t = np.arange(num_samples) / fs
    decay = np.exp(-6.908 * t / rt60_s)  # -60 dB at t = rt60 (ln(1000) = 6.908)
    predelay = np.zeros(int(predelay_s * fs))
    ir = np.empty((len(predelay) + num_samples, 2))
    for ch in range(2):
        tail = rng.standard_normal(num_samples) * decay
        tail /= np.sqrt(np.sum(tail**2))
        ir[:, ch] = np.concatenate([predelay, tail])
    return ir


def add_reverb(mono, ir, wet_db):
    """Dry mono signal in the centre plus a stereo reverb tail at wet_db relative to the dry level."""
    dry = np.column_stack([mono, mono]) / np.sqrt(2.0)
    wet = np.column_stack([signal.fftconvolve(mono, ir[:, ch]) for ch in range(2)])
    out = np.zeros((len(wet), 2))
    out[: len(dry)] += dry
    out += db_to_lin(wet_db) * wet / np.sqrt(2.0)
    return out


def loop_to_length(x, num_samples):
    """Repeat a signal (N,) or (N, 2) until it has num_samples samples."""
    reps = int(np.ceil(num_samples / len(x)))
    tiled = np.concatenate([x] * reps, axis=0)
    return tiled[:num_samples]


def normalize_rms(x, target_db):
    """Scale so the RMS over all channels is target_db dBFS."""
    rms = np.sqrt(np.mean(x**2))
    return x * db_to_lin(target_db) / rms


def normalize_peak(x, target_db=-1.0):
    return x * db_to_lin(target_db) / np.max(np.abs(x))

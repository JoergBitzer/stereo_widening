"""Loudspeaker playback model and IACC.

Head model: spherical head after
    C. P. Brown, R. O. Duda, "A Structural Model for Binaural Sound Synthesis",
    IEEE Trans. Speech and Audio Processing 6(5), 1998.
Only the head shadow (first-order shelving filter) and the interaural time delay
are modelled; pinna and torso are ignored. This is enough to show how much
interaural decorrelation a stereo signal produces on a standard loudspeaker setup
(speakers at +-30 degrees), and it needs no measured HRTF data.

Angle convention: azimuth 0 = front, positive = left. Left ear at +90, right ear at -90.
"""

import numpy as np
from scipy import signal

HEAD_RADIUS_M = 0.0875
SPEED_OF_SOUND = 343.0
ALPHA_MIN = 0.1
THETA_MIN_DEG = 150.0
HRIR_LENGTH = 512
HRIR_BULK_DELAY = 32  # samples, keeps the filters causal


def spherical_head_hrir(source_az_deg, ear_az_deg, fs, length=HRIR_LENGTH):
    """Impulse response from a far-field source to one ear of a spherical head."""
    theta = np.deg2rad(min(abs(source_az_deg - ear_az_deg), 360.0 - abs(source_az_deg - ear_az_deg)))
    a_c = HEAD_RADIUS_M / SPEED_OF_SOUND

    # head shadow: H = (1 + j*alpha*w/(2*w0)) / (1 + j*w/(2*w0)), w0 = c/a
    alpha = (1 + ALPHA_MIN / 2) + (1 - ALPHA_MIN / 2) * np.cos(np.rad2deg(theta) / THETA_MIN_DEG * np.pi)
    # delay relative to the head centre (Woodworth-type formula), shifted to be >= 0
    if theta < np.pi / 2:
        delay_s = -a_c * np.cos(theta)
    else:
        delay_s = a_c * (theta - np.pi / 2)
    delay_s += a_c + HRIR_BULK_DELAY / fs

    w = 2 * np.pi * np.fft.rfftfreq(length, 1 / fs)
    w0 = 1.0 / a_c
    shadow = (1 + 1j * alpha * w / (2 * w0)) / (1 + 1j * w / (2 * w0))
    return np.fft.irfft(shadow * np.exp(-1j * w * delay_s), n=length)


def loudspeaker_to_ears(x, fs, speaker_az_deg=30.0):
    """Simulate stereo playback over two loudspeakers at +-speaker_az_deg.

    Returns the ear signals, shape (N + HRIR_LENGTH - 1, 2): [left ear, right ear].
    """
    h = {
        (spk, ear): spherical_head_hrir(spk, ear, fs)
        for spk in (speaker_az_deg, -speaker_az_deg)
        for ear in (90.0, -90.0)
    }
    left_spk, right_spk = x[:, 0], x[:, 1]
    ear_left = (signal.fftconvolve(left_spk, h[(speaker_az_deg, 90.0)])
                + signal.fftconvolve(right_spk, h[(-speaker_az_deg, 90.0)]))
    ear_right = (signal.fftconvolve(left_spk, h[(speaker_az_deg, -90.0)])
                 + signal.fftconvolve(right_spk, h[(-speaker_az_deg, -90.0)]))
    return np.column_stack([ear_left, ear_right])


def iacc(ears, fs, max_lag_s=0.001):
    """Interaural cross-correlation coefficient: max |IACF(tau)| for |tau| <= max_lag_s.

    IACF(tau) = sum eL(n) eR(n+tau) / sqrt(sum eL^2 * sum eR^2)
    IACC = 1: both ears get the same signal (narrow, point-like image).
    Low IACC: decorrelated ear signals (wide, diffuse image).
    """
    e_left, e_right = ears[:, 0], ears[:, 1]
    denom = np.sqrt(np.sum(e_left**2) * np.sum(e_right**2))
    if denom <= 0:
        return float("nan")
    iacf = signal.correlate(e_right, e_left, mode="full", method="fft") / denom
    lags = signal.correlation_lags(len(e_right), len(e_left), mode="full")
    max_lag = int(round(max_lag_s * fs))
    window = np.abs(lags) <= max_lag
    return float(np.max(np.abs(iacf[window])))


OCTAVE_CENTRES = np.array([125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0])


def band_iacc(ears, fs, centres=OCTAVE_CENTRES):
    """IACC per octave band (4th-order Butterworth band-pass, zero phase)."""
    values = []
    for fc in centres:
        sos = signal.butter(4, [fc / np.sqrt(2), fc * np.sqrt(2)], btype="bandpass", fs=fs, output="sos")
        values.append(iacc(signal.sosfiltfilt(sos, ears, axis=0), fs))
    return centres, np.array(values)

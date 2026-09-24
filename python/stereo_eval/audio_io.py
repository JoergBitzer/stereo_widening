"""Reading and writing audio files.

All signals in this project are numpy arrays of shape (num_samples, 2):
column 0 is the left channel, column 1 the right channel.
"""

import numpy as np
import soundfile as sf

PROJECT_FS = 44100  # sample rate of all test material


def read_stereo(path, expected_fs=PROJECT_FS):
    """Read a wav file and return (x, fs) with x of shape (N, 2).

    Mono files are duplicated to both channels (L = R).
    """
    x, fs = sf.read(path, dtype="float64", always_2d=True)
    if expected_fs is not None and fs != expected_fs:
        raise ValueError(f"{path}: sample rate {fs} Hz, expected {expected_fs} Hz")
    if x.shape[1] == 1:
        x = np.repeat(x, 2, axis=1)
    elif x.shape[1] > 2:
        raise ValueError(f"{path}: {x.shape[1]} channels, expected 1 or 2")
    return x, fs


def read_mono(path, expected_fs=PROJECT_FS):
    """Read a wav file and return (x, fs) with x of shape (N,), the mid signal (L+R)/2."""
    x, fs = read_stereo(path, expected_fs)
    return 0.5 * (x[:, 0] + x[:, 1]), fs


def write_stereo(path, x, fs=PROJECT_FS):
    """Write x (N, 2) as 32-bit float wav, so no quantisation is added to test signals."""
    x = np.asarray(x)
    if x.ndim != 2 or x.shape[1] != 2:
        raise ValueError("x must have shape (N, 2)")
    sf.write(path, x, fs, subtype="FLOAT")

"""Generate the synthetic test signals (Phase 1) into test_signals/generated/.

All signals: 44.1 kHz, 32-bit float wav, fixed random seed (reproducible).
Signals that use samples need ./copy_test_samples.sh to have run first.

Usage:  python python/generate_test_signals.py
"""

import os
import sys

import numpy as np

from stereo_eval import audio_io
from stereo_eval import signals as sig

FS = audio_io.PROJECT_FS
SEED = 20260924
NOISE_DURATION_S = 10.0
NOISE_RMS_DB = -20.0

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SAMPLE_DIR = os.path.join(PROJECT_DIR, "test_signals", "samples")
OUT_DIR = os.path.join(PROJECT_DIR, "test_signals", "generated")


def save(name, x, description):
    audio_io.write_stereo(os.path.join(OUT_DIR, name), x, FS)
    print(f"  {name:<32s} {len(x) / FS:5.1f} s  {description}")


def noise_signals(rng):
    n = int(NOISE_DURATION_S * FS)
    for rho, name in [(1.0, "noise_pink_mono.wav"),
                      (0.5, "noise_pink_rho050.wav"),
                      (0.0, "noise_pink_uncorrelated.wav")]:
        x = sig.normalize_rms(sig.correlated_noise_pair(n, rho, rng), NOISE_RMS_DB)
        save(name, x, f"pink noise, correlation {rho:+.1f}")

    mono = sig.normalize_rms(sig.pink_noise(n, rng), NOISE_RMS_DB)
    save("noise_pink_antiphase.wav", np.column_stack([mono, -mono]), "pink noise, R = -L (correlation -1)")


def sweep_signals():
    sweep = sig.log_sweep(10.0, FS) * sig.db_to_lin(-6.0)
    save("sweep_mono.wav", np.column_stack([sweep, sweep]), "log sweep 20 Hz - 20 kHz, L = R, -6 dBFS")


def panned_speech():
    speech, _ = audio_io.read_mono(os.path.join(SAMPLE_DIR, "speech_dry_answers.wav"))
    speech = sig.normalize_peak(speech, -3.0)
    for pan, label in [(-1.0, "L100"), (-0.5, "L50"), (0.0, "C"), (0.5, "R50"), (1.0, "R100")]:
        save(f"speech_pan_{label}.wav", sig.pan_constant_power(speech, pan),
             f"dry speech, constant-power pan {pan:+.1f}")


def speech_with_reverb(rng):
    speech, _ = audio_io.read_mono(os.path.join(SAMPLE_DIR, "speech_dry_answers.wav"))
    ir = sig.synthetic_stereo_reverb_ir(FS, rt60_s=1.2, rng=rng)
    x = sig.normalize_peak(sig.add_reverb(speech, ir, wet_db=-6.0), -3.0)
    save("speech_mono_synthreverb.wav", x, "dry speech centre + uncorrelated reverb, RT60 1.2 s, wet -6 dB")


def small_mix():
    """Bass (mono, centre) + synth (stereo) + vocal phrase panned 30 % left, about 9.6 s."""
    bass, _ = audio_io.read_stereo(os.path.join(SAMPLE_DIR, "bass_loop.wav"))
    synth, _ = audio_io.read_stereo(os.path.join(SAMPLE_DIR, "synth_loop.wav"))
    vocal, _ = audio_io.read_mono(os.path.join(SAMPLE_DIR, "vocal_phrase_always.wav"))
    n = 2 * len(synth)
    mix = (sig.normalize_rms(sig.loop_to_length(bass, n), -20.0)
           + sig.normalize_rms(sig.loop_to_length(synth, n), -22.0))
    vocal_st = sig.normalize_rms(sig.pan_constant_power(vocal, -0.3), -20.0)
    start = int(1.0 * FS)
    length = min(len(vocal_st), n - start)
    mix[start:start + length] += vocal_st[:length]
    save("mix_small.wav", sig.normalize_peak(mix, -1.0), "bass centre + stereo synth + vocal 30 % left")


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    rng = np.random.default_rng(SEED)
    print(f"writing to {OUT_DIR}")
    noise_signals(rng)
    sweep_signals()
    if not os.path.isdir(SAMPLE_DIR):
        print("test_signals/samples missing: run ./copy_test_samples.sh first", file=sys.stderr)
        return 1
    panned_speech()
    speech_with_reverb(rng)
    small_mix()
    return 0


if __name__ == "__main__":
    sys.exit(main())

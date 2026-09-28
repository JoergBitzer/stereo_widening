"""Compare WidenerRender builds (e.g. different optimisation levels) against a reference.

Renders every factory preset (python/make_factory_presets.py) with each build, on a
mono and a stereo test signal, and reports per build:
  - max |difference| to the reference build's output, in dB relative to the
    reference's peak (-inf = bit-identical), worst over all presets and signals;
  - whether any output contains NaN/Inf;
  - render time: a 60 s pink-noise render per algorithm, best of 3, as a real-time
    factor (render time / audio duration) -- includes file I/O, so only for comparing
    builds with each other.

Usage:  python python/compare_builds.py <reference WidenerRender> <name>=<WidenerRender> ...

(c) J. Bitzer, Jade HS, MIT license
"""

import os
import subprocess
import sys
import tempfile
import time

import numpy as np

from stereo_eval import audio_io
from make_factory_presets import PRESETS, DEFAULTS, ALGORITHMS
from evaluate_factory_presets import render_args, SIGNAL_DIR

SIGNALS = {"mono synth": ("samples/synth_loop.wav", True),
           "stereo mix": ("samples/mix_loop_carry_on.wav", False)}
TIMING_SECONDS = 60


def render(binary, in_path, out_path, algorithm, params):
    subprocess.run([binary, in_path, out_path] + render_args(algorithm, params),
                   capture_output=True, text=True, check=True)
    y, _ = audio_io.read_stereo(out_path)
    return y


def preset_params(values):
    p = {k: d[0] for k, d in DEFAULTS.items()}
    p.update(values)
    return p


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    reference = sys.argv[1]
    builds = dict(arg.split("=", 1) for arg in sys.argv[2:])

    with tempfile.TemporaryDirectory() as tmp:
        inputs = {}
        for name, (path, force_mono) in SIGNALS.items():
            x, fs = audio_io.read_stereo(os.path.join(SIGNAL_DIR, path))
            if force_mono:
                x = np.repeat(x.mean(axis=1, keepdims=True), 2, axis=1)
            inputs[name] = os.path.join(tmp, name.replace(" ", "_") + ".wav")
            audio_io.write_stereo(inputs[name], x, fs)
        out = os.path.join(tmp, "out.wav")

        # reference outputs
        ref = {}
        for _, preset, algorithm, values, _ in PRESETS:
            for sig, in_path in inputs.items():
                ref[(preset, sig)] = render(reference, in_path, out, algorithm, preset_params(values))

        print("Accuracy against the reference build (all 21 presets x 2 signals):")
        print(f"{'build':10s} {'max diff':>10s}  {'worst preset / signal':44s} {'NaN/Inf':>7s}")
        for build, binary in builds.items():
            worst, worst_at, bad = -np.inf, "-", False
            for _, preset, algorithm, values, _ in PRESETS:
                for sig, in_path in inputs.items():
                    y = render(binary, in_path, out, algorithm, preset_params(values))
                    r = ref[(preset, sig)]
                    bad |= not np.all(np.isfinite(y))
                    n = min(len(y), len(r))
                    diff = np.max(np.abs(y[:n] - r[:n]))
                    peak = max(np.max(np.abs(r)), 1e-12)
                    diff_db = 20 * np.log10(diff / peak) if diff > 0 else -np.inf
                    if diff_db > worst:
                        worst, worst_at = diff_db, f"{preset} / {sig}"
            print(f"{build:10s} {worst:8.1f} dB  {worst_at:44s} {'yes' if bad else 'no':>7s}")

        # timing: every algorithm with a representative (non-neutral) preset
        rng = np.random.default_rng(1)
        fs = 48000
        white = rng.standard_normal((TIMING_SECONDS * fs, 2))
        noise_path = os.path.join(tmp, "noise.wav")
        audio_io.write_stereo(noise_path, 0.1 * white, fs)
        timing_presets = {"broadband": {"broadbandWidth": 150}}  # Init would be neutral
        for _, preset, algorithm, values, _ in PRESETS:
            timing_presets.setdefault(algorithm, values)
        print(f"\nRender time for {TIMING_SECONDS} s of stereo noise at 48 kHz, as % of real time "
              f"(best of 3, incl. file I/O):")
        print(f"{'build':10s} " + " ".join(f"{a:>9s}" for a in ALGORITHMS))
        for build, binary in [("reference", reference)] + list(builds.items()):
            row = []
            for algorithm in ALGORITHMS:
                params = preset_params(timing_presets.get(algorithm, {}))
                best = np.inf
                for _ in range(3):
                    t0 = time.perf_counter()
                    subprocess.run([binary, noise_path, out] + render_args(algorithm, params),
                                   capture_output=True, check=True)
                    best = min(best, time.perf_counter() - t0)
                row.append(100.0 * best / TIMING_SECONDS)
            print(f"{build:10s} " + " ".join(f"{v:8.2f}%" for v in row))


if __name__ == "__main__":
    main()

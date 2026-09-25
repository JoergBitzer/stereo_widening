"""Evaluate algorithm 2.12 (early-reflection / room widening) on the test material
(Phase 5, step 1).

Writes a text summary and one figure per signal to python/results/early_reflections/.
Optionally writes the processed audio to test_signals/processed/early_reflections/.

Also checks the one invariant the module docstring guarantees algebraically: amount = 0
must be an exact bypass (y == x). Unlike algorithm 2.4 (comb), this algorithm's mono
sum is *not* expected to stay flat once amount > 0 -- that is the documented trade-off
(planing.md 2.12's own "o" partial mono-compatibility rating), same reasoning as
algorithm 2.5's allpass decorrelation; the summary table's "mono col" column is
expected to be non-zero here.

Usage:  python python/evaluate_early_reflections.py [--write-audio]
"""

import os
import sys

import numpy as np

from algorithms.early_reflections import early_reflections
from stereo_eval import audio_io, report

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESULT_DIR = os.path.join(PROJECT_DIR, "python", "results", "early_reflections")
AUDIO_OUT_DIR = os.path.join(PROJECT_DIR, "test_signals", "processed", "early_reflections")

SIGNALS = [
    "generated/noise_pink_rho050.wav",
    "generated/speech_pan_L50.wav",
    "generated/speech_mono_synthreverb.wav",
    "generated/mix_small.wav",
    "samples/speech_dry_answers.wav",   # dual mono: reflections are the only source of width
    "samples/speech_wet_answers.wav",
    "samples/mix_loop_let_it_be.wav",
]

SETTINGS = [
    ("a050_r050", dict(amount=0.5, room_size=0.5)),
    ("a025_r025", dict(amount=0.25, room_size=0.25)),  # small room, subtle
    ("a100_r100", dict(amount=1.0, room_size=1.0)),    # large room, full amount -- worst case
    ("a050_r000", dict(amount=0.5, room_size=0.0)),    # smallest room, tap cluster tightest
    ("a000", dict(amount=0.0, room_size=0.5)),         # bypass check
    ("a050_r050_w200", dict(amount=0.5, room_size=0.5, width=2.0)),
]

PLOT_SETTING = "a050_r050"
BYPASS_SETTING = "a000"


def main():
    write_audio = "--write-audio" in sys.argv
    os.makedirs(RESULT_DIR, exist_ok=True)
    rows = []
    header = (f"{'signal':<28s} {'setting':<16s} {'rho in':>7s} {'rho out':>7s} {'IACC in':>7s} "
              f"{'IACC out':>8s} {'dS-M':>6s} {'dLUFS':>6s} {'mono col':>8s}")
    print(header)

    bypass_ok = True

    for rel_path in SIGNALS:
        path = os.path.join(PROJECT_DIR, "test_signals", rel_path)
        if not os.path.exists(path):
            print(f"missing {path} (run copy_test_samples.sh / generate_test_signals.py)", file=sys.stderr)
            continue
        x, fs = audio_io.read_stereo(path)
        name = os.path.splitext(os.path.basename(path))[0]
        for label, raw_params in SETTINGS:
            params = dict(raw_params)  # don't mutate SETTINGS -- it's reused for every signal
            width = params.pop("width", 1.0)
            y_raw = early_reflections(x, fs, **params)
            # Width applied as the final shared trim, same convention as every other
            # algorithm: scale the ADDED part (y_raw - x), not the whole signal.
            y = x + width * (y_raw - x)
            ev = report.evaluate(x, y, fs)
            row = (f"{name:<28s} {label:<16s} {ev['in']['correlation']:7.2f} {ev['out']['correlation']:7.2f} "
                   f"{ev['in']['iacc']:7.2f} {ev['out']['iacc']:8.2f} {ev['change']['S_minus_M_dB']:6.1f} "
                   f"{ev['change']['LUFS']:6.1f} {ev['change']['mono_coloration_dB']:8.2f}")
            print(row)
            rows.append(row)

            if label == BYPASS_SETTING:
                diff = float(np.max(np.abs(y - x)))
                if diff > 1e-9:
                    print(f"  FAIL {name}: amount=0 should be an exact bypass, max abs diff = {diff}", file=sys.stderr)
                    bypass_ok = False

            if label == PLOT_SETTING:
                report.plot_evaluation(ev, x, y, os.path.join(RESULT_DIR, f"{name}_{label}.png"),
                                       title=f"Early reflections: {name}, {params}")
            if write_audio:
                os.makedirs(AUDIO_OUT_DIR, exist_ok=True)
                audio_io.write_stereo(os.path.join(AUDIO_OUT_DIR, f"{name}_{label}.wav"), y, fs)

    with open(os.path.join(RESULT_DIR, "summary.txt"), "w") as f:
        f.write("Early-reflection / room widening (algorithm 2.12)\n")
        f.write("dS-M: change of side-to-mid ratio in dB, mono col: colouration of L+R in dB "
                "(expected NON-zero once amount > 0 -- planing.md 2.12's own \"o\" partial "
                "mono-compatibility rating, same reasoning as algorithm 2.5's allpass)\n\n")
        f.write(header + "\n" + "\n".join(rows) + "\n")
    print(f"\nresults in {RESULT_DIR}")
    print("amount=0 bypass sanity check:", "OK" if bypass_ok else "FAILED")
    return 0 if bypass_ok else 1


if __name__ == "__main__":
    sys.exit(main())

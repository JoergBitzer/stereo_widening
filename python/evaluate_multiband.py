"""Evaluate algorithm 2.7 (multiband width) on the test material (Phase 5, step 1).

Writes a text summary and one figure per signal to python/results/multiband/.
Optionally writes the processed audio to test_signals/processed/multiband/.

Also checks that the "default" setting (all band widths + the shared Width at their
neutral 1.0/100%) reconstructs the input with (close to) flat magnitude -- using
stereo_eval's mono_coloration_dB (a per-1/3-octave-band *level* comparison), not a raw
sample-domain difference: the phase-compensated LR4 tree reconstructs x as a single
overall allpass (flat magnitude, but real phase/group-delay distortion, matching
planing.md's "LR sums to an allpass"), and a raw sample diff would report large
"error" purely from that expected, inaudible-as-coloration phase shift. See
algorithms/multiband_width.py's split_bands() docstring for why the naive (pre-
compensation) version failed this same check by ~0.5-0.6 dB.

Usage:  python python/evaluate_multiband.py [--write-audio]
"""

import os
import sys

import numpy as np

from algorithms.multiband_width import multiband_width
from stereo_eval import audio_io, report

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESULT_DIR = os.path.join(PROJECT_DIR, "python", "results", "multiband")
AUDIO_OUT_DIR = os.path.join(PROJECT_DIR, "test_signals", "processed", "multiband")

SIGNALS = [
    "generated/noise_pink_rho050.wav",
    "generated/speech_pan_L50.wav",
    "generated/speech_mono_synthreverb.wav",
    "generated/mix_small.wav",
    "samples/speech_dry_answers.wav",
    "samples/speech_wet_answers.wav",
    "samples/mix_loop_let_it_be.wav",
]

# (label, kwargs) -- crossovers 150/1500/6000 Hz throughout except where the setting
# itself is about testing a different split. "default": every band width and the
# shared Width at their neutral value (1.0/100%) -- verifies LR4 reconstruction AND
# demonstrates "bass mono comes built in" (band 1 is always forced to width 0
# regardless of width2/3/4, see algorithms/multiband_width.py's docstring) in the same
# run, since the per-octave correlation plot shows the low end forced to ~1.0 while
# everything above freq1 passes through at the input's own existing correlation.
SETTINGS = [
    ("default", dict()),
    ("narrow_high", dict(width4=0.0)),                       # only the top band collapses to mono
    ("wide_mid", dict(width3=2.0)),                           # only the mid band widens
    ("all_narrow", dict(width2=0.0, width3=0.0, width4=0.0)), # fully mono (band 1 already forced)
    ("width_trim", dict(width=0.5)),                          # shared Width knob scales everything
]

PLOT_SETTING = "default"
IDENTITY_SETTING = "default"
IDENTITY_TOLERANCE_DB = 0.15  # max allowed mono_coloration_dB at the neutral setting


def main():
    write_audio = "--write-audio" in sys.argv
    os.makedirs(RESULT_DIR, exist_ok=True)
    rows = []
    header = (f"{'signal':<28s} {'setting':<14s} {'rho in':>7s} {'rho out':>7s} {'IACC in':>7s} "
              f"{'IACC out':>8s} {'dS-M':>6s} {'dLUFS':>6s} {'mono col':>8s}")
    print(header)

    identity_ok = True

    for rel_path in SIGNALS:
        path = os.path.join(PROJECT_DIR, "test_signals", rel_path)
        if not os.path.exists(path):
            print(f"missing {path} (run copy_test_samples.sh / generate_test_signals.py)", file=sys.stderr)
            continue
        x, fs = audio_io.read_stereo(path)
        name = os.path.splitext(os.path.basename(path))[0]
        for label, kwargs in SETTINGS:
            y = multiband_width(x, fs, **kwargs)
            ev = report.evaluate(x, y, fs)
            row = (f"{name:<28s} {label:<14s} {ev['in']['correlation']:7.2f} {ev['out']['correlation']:7.2f} "
                   f"{ev['in']['iacc']:7.2f} {ev['out']['iacc']:8.2f} {ev['change']['S_minus_M_dB']:6.1f} "
                   f"{ev['change']['LUFS']:6.1f} {ev['change']['mono_coloration_dB']:8.2f}")
            print(row)
            rows.append(row)

            if label == IDENTITY_SETTING:
                coloration_db = ev["change"]["mono_coloration_dB"]
                if coloration_db > IDENTITY_TOLERANCE_DB:
                    print(f"  FAIL {name}: default-setting mono coloration {coloration_db:.3f} dB "
                          f"exceeds tolerance {IDENTITY_TOLERANCE_DB} dB", file=sys.stderr)
                    identity_ok = False

            if label == PLOT_SETTING:
                report.plot_evaluation(ev, x, y, os.path.join(RESULT_DIR, f"{name}_{label}.png"),
                                       title=f"Multiband width: {name}, {label}")
            if write_audio:
                os.makedirs(AUDIO_OUT_DIR, exist_ok=True)
                audio_io.write_stereo(os.path.join(AUDIO_OUT_DIR, f"{name}_{label}.wav"), y, fs)

    with open(os.path.join(RESULT_DIR, "summary.txt"), "w") as f:
        f.write("Multiband width (algorithm 2.7)\n")
        f.write("dS-M: change of side-to-mid ratio in dB, mono col: colouration of L+R in dB "
                "(should be ~0 dB at every setting -- mono-compatible by construction, M untouched)\n\n")
        f.write(header + "\n" + "\n".join(rows) + "\n")
    print(f"\nresults in {RESULT_DIR}")
    print(f"'{IDENTITY_SETTING}' reconstruction sanity check:", "OK" if identity_ok else "FAILED")
    return 0 if identity_ok else 1


if __name__ == "__main__":
    sys.exit(main())

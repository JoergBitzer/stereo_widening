"""Evaluate algorithm 2.1 (M/S width) on the test material (Phase 1, step 4).

Writes a text summary and one figure per signal to python/results/ms_width/.
Optionally writes the processed audio to test_signals/processed/ms_width/ for listening.

Usage:  python python/evaluate_ms_width.py [--write-audio]
"""

import os
import sys

from algorithms.ms_width import ms_width
from stereo_eval import audio_io, report

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESULT_DIR = os.path.join(PROJECT_DIR, "python", "results", "ms_width")
AUDIO_OUT_DIR = os.path.join(PROJECT_DIR, "test_signals", "processed", "ms_width")

SIGNALS = [
    "generated/noise_pink_rho050.wav",
    "generated/speech_pan_L50.wav",
    "generated/speech_mono_synthreverb.wav",
    "generated/mix_small.wav",
    "samples/speech_dry_answers.wav",   # dual mono: M/S width has no effect
    "samples/speech_wet_answers.wav",
    "samples/mix_loop_let_it_be.wav",
]

SETTINGS = [
    ("w000", dict(width=0.0)),
    ("w050", dict(width=0.5)),
    ("w150", dict(width=1.5)),
    ("w200", dict(width=2.0)),
    ("w150_bass120", dict(width=1.5, bass_mono_hz=120.0)),
    ("w150_bass120_shelf+3", dict(width=1.5, bass_mono_hz=120.0, side_shelf_db=3.0)),
    ("w150_cp", dict(width=1.5, compensation="constant_power")),
]

PLOT_SETTING = "w150_bass120"


def main():
    write_audio = "--write-audio" in sys.argv
    os.makedirs(RESULT_DIR, exist_ok=True)
    rows = []
    header = (f"{'signal':<28s} {'setting':<22s} {'rho in':>7s} {'rho out':>7s} {'IACC in':>7s} "
              f"{'IACC out':>8s} {'dS-M':>6s} {'dLUFS':>6s} {'mono col':>8s}")
    print(header)
    for rel_path in SIGNALS:
        path = os.path.join(PROJECT_DIR, "test_signals", rel_path)
        if not os.path.exists(path):
            print(f"missing {path} (run copy_test_samples.sh / generate_test_signals.py)", file=sys.stderr)
            continue
        x, fs = audio_io.read_stereo(path)
        name = os.path.splitext(os.path.basename(path))[0]
        for label, params in SETTINGS:
            y = ms_width(x, fs, **params)
            ev = report.evaluate(x, y, fs)
            row = (f"{name:<28s} {label:<22s} {ev['in']['correlation']:7.2f} {ev['out']['correlation']:7.2f} "
                   f"{ev['in']['iacc']:7.2f} {ev['out']['iacc']:8.2f} {ev['change']['S_minus_M_dB']:6.1f} "
                   f"{ev['change']['LUFS']:6.1f} {ev['change']['mono_coloration_dB']:8.2f}")
            print(row)
            rows.append(row)
            if label == PLOT_SETTING:
                report.plot_evaluation(ev, x, y, os.path.join(RESULT_DIR, f"{name}_{label}.png"),
                                       title=f"M/S width: {name}, {params}")
            if write_audio:
                os.makedirs(AUDIO_OUT_DIR, exist_ok=True)
                audio_io.write_stereo(os.path.join(AUDIO_OUT_DIR, f"{name}_{label}.wav"), y, fs)

    with open(os.path.join(RESULT_DIR, "summary.txt"), "w") as f:
        f.write("M/S width (algorithm 2.1)\n")
        f.write("dS-M: change of side-to-mid ratio in dB, mono col: colouration of L+R in dB\n\n")
        f.write(header + "\n" + "\n".join(rows) + "\n")
    print(f"\nresults in {RESULT_DIR}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

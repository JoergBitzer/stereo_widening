"""Evaluate algorithm 2.8 (STFT panning expansion / source re-panning) prototype on
the test material -- a feasibility check, NOT a Phase 5 v1 evaluation. Unlike every
other evaluate_*.py in this project, this one always writes the processed audio (not
behind a --write-audio flag): the actual point of this run is to hand over real files
to listen to, since planing.md flags this as the most complex/expensive technique in
the catalogue and it needs judging by ear, not just by the numbers below.

Writes a text summary and one figure per (signal, setting) to
python/results/source_repanning/, and every processed file to
test_signals/processed/source_repanning/.

Includes an "identity" setting (expansion = 1.0, i.e. no remap requested) specifically
to surface algorithms/source_repanning.py's documented limitation: re-synthesising
from a one-coherent-source-per-bin panning-index model is not expected to be a clean
bypass for real (multi-source/ambient) material, unlike every algorithm in the actual
plugin. A large mono_coloration_dB or audibly changed timbre at "identity" is that
limitation, not a bug -- see the module docstring.

Usage:  python python/evaluate_source_repanning.py
"""

import os
import sys

from algorithms.source_repanning import source_repanning
from stereo_eval import audio_io, report

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESULT_DIR = os.path.join(PROJECT_DIR, "python", "results", "source_repanning")
AUDIO_OUT_DIR = os.path.join(PROJECT_DIR, "test_signals", "processed", "source_repanning")

SIGNALS = [
    "generated/noise_pink_rho050.wav",
    "generated/speech_pan_L50.wav",
    "generated/speech_mono_synthreverb.wav",
    "generated/mix_small.wav",
    "samples/speech_dry_answers.wav",
    "samples/speech_wet_answers.wav",
    "samples/mix_loop_let_it_be.wav",
]

# (label, expansion) -- expansion=1.0 ("identity") is the "no remap requested"
# baseline (see module docstring for why it is NOT expected to be a clean bypass);
# 2.0 matches planing.md's own worked example ("sources at 30% move to 60%").
SETTINGS = [
    ("identity", 1.0),
    ("mild", 1.5),
    ("moderate", 2.0),
    ("strong", 3.0),
]

PLOT_SETTINGS = ("identity", "moderate")


def main():
    os.makedirs(RESULT_DIR, exist_ok=True)
    os.makedirs(AUDIO_OUT_DIR, exist_ok=True)

    rows = []
    header = (f"{'signal':<28s} {'setting':<10s} {'rho in':>7s} {'rho out':>7s} {'IACC in':>7s} "
              f"{'IACC out':>8s} {'dS-M':>6s} {'dLUFS':>6s} {'mono col':>8s}")
    print(header)

    for rel_path in SIGNALS:
        path = os.path.join(PROJECT_DIR, "test_signals", rel_path)
        if not os.path.exists(path):
            print(f"missing {path} (run copy_test_samples.sh / generate_test_signals.py)", file=sys.stderr)
            continue
        x, fs = audio_io.read_stereo(path)
        name = os.path.splitext(os.path.basename(path))[0]

        for label, expansion in SETTINGS:
            y = source_repanning(x, fs, expansion=expansion)
            audio_io.write_stereo(os.path.join(AUDIO_OUT_DIR, f"{name}_{label}.wav"), y, fs)

            ev = report.evaluate(x, y, fs)
            row = (f"{name:<28s} {label:<10s} {ev['in']['correlation']:7.2f} {ev['out']['correlation']:7.2f} "
                   f"{ev['in']['iacc']:7.2f} {ev['out']['iacc']:8.2f} {ev['change']['S_minus_M_dB']:6.1f} "
                   f"{ev['change']['LUFS']:6.1f} {ev['change']['mono_coloration_dB']:8.2f}")
            print(row)
            rows.append(row)

            if label in PLOT_SETTINGS:
                report.plot_evaluation(ev, x, y, os.path.join(RESULT_DIR, f"{name}_{label}.png"),
                                        title=f"Source re-panning (2.8 prototype): {name}, {label}")

    with open(os.path.join(RESULT_DIR, "summary.txt"), "w") as f:
        f.write("STFT panning expansion / source re-panning (algorithm 2.8, Python-only prototype)\n")
        f.write("dS-M: change of side-to-mid ratio in dB, mono col: colouration of L+R in dB\n")
        f.write("'identity' (expansion=1.0) is NOT expected to be a clean bypass -- see the\n")
        f.write("module docstring's one-coherent-source-per-bin limitation.\n\n")
        f.write(header + "\n" + "\n".join(rows) + "\n")
    print(f"\nresults in {RESULT_DIR}")
    print(f"processed audio in {AUDIO_OUT_DIR}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

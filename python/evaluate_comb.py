"""Evaluate algorithm 2.4 (complementary comb pseudo-stereo) on the test material
(Phase 5, step 1).

Writes a text summary and one figure per signal to python/results/comb/.
Optionally writes the processed audio to test_signals/processed/comb/ for listening.

Usage:  python python/evaluate_comb.py [--write-audio]
"""

import os
import sys

from algorithms.comb import comb
from stereo_eval import audio_io, report

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESULT_DIR = os.path.join(PROJECT_DIR, "python", "results", "comb")
AUDIO_OUT_DIR = os.path.join(PROJECT_DIR, "test_signals", "processed", "comb")

SIGNALS = [
    "generated/noise_pink_rho050.wav",
    "generated/speech_pan_L50.wav",
    "generated/speech_mono_synthreverb.wav",
    "generated/mix_small.wav",
    "samples/speech_dry_answers.wav",   # dual mono: the comb term is the only source of width
    "samples/speech_wet_answers.wav",
    "samples/mix_loop_let_it_be.wav",
]

# delay_ms/gain ranges from planing.md 2.4 ("D ~= 5-20 ms and g ~= 0.3-0.7");
# crossover_hz=0 disables the high-pass, to show its effect against the default 300 Hz
SETTINGS = [
    ("d10_g050", dict(delay_ms=10.0, gain=0.5)),
    ("d05_g030", dict(delay_ms=5.0, gain=0.3)),
    ("d20_g070", dict(delay_ms=20.0, gain=0.7)),
    ("d10_g050_noxover", dict(delay_ms=10.0, gain=0.5, crossover_hz=0)),
    ("d10_g050_w200", dict(delay_ms=10.0, gain=0.5, width=2.0)),
    ("d10_g050_w000", dict(delay_ms=10.0, gain=0.5, width=0.0)),
]

PLOT_SETTING = "d10_g050"


def main():
    write_audio = "--write-audio" in sys.argv
    os.makedirs(RESULT_DIR, exist_ok=True)
    rows = []
    header = (f"{'signal':<28s} {'setting':<20s} {'rho in':>7s} {'rho out':>7s} {'IACC in':>7s} "
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
            y = comb(x, fs, **params)
            ev = report.evaluate(x, y, fs)
            row = (f"{name:<28s} {label:<20s} {ev['in']['correlation']:7.2f} {ev['out']['correlation']:7.2f} "
                   f"{ev['in']['iacc']:7.2f} {ev['out']['iacc']:8.2f} {ev['change']['S_minus_M_dB']:6.1f} "
                   f"{ev['change']['LUFS']:6.1f} {ev['change']['mono_coloration_dB']:8.2f}")
            print(row)
            rows.append(row)
            if label == PLOT_SETTING:
                report.plot_evaluation(ev, x, y, os.path.join(RESULT_DIR, f"{name}_{label}.png"),
                                       title=f"Complementary comb: {name}, {params}")
            if write_audio:
                os.makedirs(AUDIO_OUT_DIR, exist_ok=True)
                audio_io.write_stereo(os.path.join(AUDIO_OUT_DIR, f"{name}_{label}.wav"), y, fs)

    with open(os.path.join(RESULT_DIR, "summary.txt"), "w") as f:
        f.write("Complementary comb pseudo-stereo (algorithm 2.4)\n")
        f.write("dS-M: change of side-to-mid ratio in dB, mono col: colouration of L+R in dB "
                "(should be ~0 dB at every setting -- mono-compatible by construction)\n\n")
        f.write(header + "\n" + "\n".join(rows) + "\n")
    print(f"\nresults in {RESULT_DIR}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

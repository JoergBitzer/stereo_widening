"""Render the test signals through the real StereoWidener C++ algorithms (Phase 3,
step 5: "Render the test signals through the plugin and run the evaluation report").

Runs the compiled WidenerRender tool -- the exact algorithm classes the plugin uses,
not the Python reference in algorithms/ms_width.py (which models a more elaborate,
not-yet-implemented version with an allpass-aligned bass mono and a configurable-gain
side shelf; see that module's docstring) -- on a set of test signals and several
algorithm/setting combinations, then measures the result with stereo_eval.report, the
same way evaluate_ms_width.py measures the Python reference.

Writes a text summary and one figure per signal to python/results/widener_plugin/, and
keeps every processed file in test_signals/processed/widener_plugin/ (the audio *is*
this script's primary output, needed for the measurement step, so there is no
--write-audio switch like evaluate_ms_width.py's).

Also checks one invariant that this project's C++/console tests already cover in
isolation (docs/algorithms/phase3_stereo_widener.md): MSWidthFiltered with both aux
knobs turned to "Off" must be bit-exact with MSWidthBroadband at the same width. Running
it again here, through the actual test-signal corpus, adds no new coverage on its own,
but exercising it on real program material (not just synthetic pink noise) is a cheap
extra confidence check now that the infrastructure to do so exists.

Usage:  python python/evaluate_widener_plugin.py [path/to/WidenerRender]
"""

import os
import subprocess
import sys

import numpy as np

from stereo_eval import audio_io, report

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESULT_DIR = os.path.join(PROJECT_DIR, "python", "results", "widener_plugin")
AUDIO_OUT_DIR = os.path.join(PROJECT_DIR, "test_signals", "processed", "widener_plugin")
DEFAULT_BINARY = os.path.join(
    PROJECT_DIR, "..", "build", "stereo_widening", "tools", "widener_render",
    "WidenerRender_artefacts", "Debug", "WidenerRender")

SIGNALS = [
    "generated/noise_pink_rho050.wav",
    "generated/speech_pan_L50.wav",
    "generated/speech_mono_synthreverb.wav",
    "generated/mix_small.wav",
    "samples/speech_dry_answers.wav",   # dual mono: M/S width has no effect
    "samples/speech_wet_answers.wav",
    "samples/mix_loop_let_it_be.wav",
]

# (label, algorithm, width_percent, bassCutoffHz, highShelfHz) -- the last two only
# matter for "filtered" (see algorithms/MSWidthFiltered.h); "..._off" uses values in
# both knobs' Off zones (below 40 Hz / above 16000 Hz), which must reduce to plain
# broadband width -- the sanity check described in the module docstring.
SETTINGS = [
    ("broadband_w000", "broadband", 0, 150, 8000),
    ("broadband_w150", "broadband", 150, 150, 8000),
    ("broadband_w200", "broadband", 200, 150, 8000),
    ("filtered_w150_bass120_shelf8k", "filtered", 150, 120, 8000),
    ("filtered_w150_off", "filtered", 150, 20, 20000),
]

PLOT_SETTING = "filtered_w150_bass120_shelf8k"
OFF_CHECK_SETTINGS = ("broadband_w150", "filtered_w150_off")


def render(binary, input_path, output_path, algorithm, width, bass_cutoff, high_shelf):
    subprocess.run([binary, input_path, output_path, algorithm,
                     str(width), str(bass_cutoff), str(high_shelf)],
                    capture_output=True, text=True, check=True)


def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_BINARY
    if not os.path.exists(binary):
        print(f"WidenerRender binary not found at {binary}\n"
              f"build it first: cmake --build <builddir> --target WidenerRender", file=sys.stderr)
        return 1

    os.makedirs(RESULT_DIR, exist_ok=True)
    os.makedirs(AUDIO_OUT_DIR, exist_ok=True)

    rows = []
    header = (f"{'signal':<28s} {'setting':<32s} {'rho in':>7s} {'rho out':>7s} {'IACC in':>7s} "
              f"{'IACC out':>8s} {'dS-M':>6s} {'dLUFS':>6s} {'mono col':>8s}")
    print(header)

    off_check_ok = True

    for rel_path in SIGNALS:
        in_path = os.path.join(PROJECT_DIR, "test_signals", rel_path)
        if not os.path.exists(in_path):
            print(f"missing {in_path} (run copy_test_samples.sh / generate_test_signals.py)", file=sys.stderr)
            continue
        x, fs = audio_io.read_stereo(in_path)
        name = os.path.splitext(os.path.basename(in_path))[0]

        outputs = {}
        for label, algorithm, width, bass_cutoff, high_shelf in SETTINGS:
            out_path = os.path.join(AUDIO_OUT_DIR, f"{name}_{label}.wav")
            render(binary, in_path, out_path, algorithm, width, bass_cutoff, high_shelf)
            y, _ = audio_io.read_stereo(out_path, expected_fs=fs)
            outputs[label] = y

            ev = report.evaluate(x, y, fs)
            row = (f"{name:<28s} {label:<32s} {ev['in']['correlation']:7.2f} {ev['out']['correlation']:7.2f} "
                   f"{ev['in']['iacc']:7.2f} {ev['out']['iacc']:8.2f} {ev['change']['S_minus_M_dB']:6.1f} "
                   f"{ev['change']['LUFS']:6.1f} {ev['change']['mono_coloration_dB']:8.2f}")
            print(row)
            rows.append(row)

            if label == PLOT_SETTING:
                report.plot_evaluation(ev, x, y, os.path.join(RESULT_DIR, f"{name}_{label}.png"),
                                        title=f"StereoWidener plugin: {name}, {label}")

        a, b = OFF_CHECK_SETTINGS
        diff = float(np.max(np.abs(outputs[a] - outputs[b])))
        if diff > 1e-6:
            print(f"  FAIL {name}: {b} should be bit-exact with {a}, max abs diff = {diff}", file=sys.stderr)
            off_check_ok = False

    with open(os.path.join(RESULT_DIR, "summary.txt"), "w") as f:
        f.write("StereoWidener plugin (real C++ algorithm classes, not the Python reference)\n")
        f.write("dS-M: change of side-to-mid ratio in dB, mono col: colouration of L+R in dB\n\n")
        f.write(header + "\n" + "\n".join(rows) + "\n")
    print(f"\nresults in {RESULT_DIR}")
    print(f"{OFF_CHECK_SETTINGS[1]} == {OFF_CHECK_SETTINGS[0]} sanity check:", "OK" if off_check_ok else "FAILED")
    return 0 if off_check_ok else 1


if __name__ == "__main__":
    sys.exit(main())

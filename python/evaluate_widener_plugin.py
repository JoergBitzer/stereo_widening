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

# (label, algorithm, width_percent, bassCutoffHz, highShelfHz, combDelayMs,
# combGainPercent, combCrossoverHz, allpassAmountPercent, allpassSpreadPercent,
# mbFreq1, mbFreq2, mbFreq3, mbWidth2Percent, mbWidth3Percent, mbWidth4Percent,
# erAmountPercent, erRoomSizePercent, erPreDelayMs) --
# bassCutoffHz/highShelfHz only matter for "filtered" (see algorithms/
# MSWidthFiltered.h); "..._off" uses values in both knobs' Off zones (below 40 Hz /
# above 16000 Hz), which must reduce to plain broadband width -- the sanity check
# described in the module docstring. combDelayMs/combGainPercent/combCrossoverHz only
# matter for "comb" (see algorithms/ComplementaryComb.h); 300 Hz matches GlobalSettings'
# own default (not a user-facing knob in the plugin itself). allpassAmountPercent/
# allpassSpreadPercent only matter for "allpass" (see algorithms/
# AllpassDecorrelation.h); spread percentages here mirror python/evaluate_allpass.py's
# spread_octaves settings (spread_octaves / kMaxSpreadOctaves=2.0 -> percent). mbFreq1/2/3
# and mbWidth2/3/4Percent only matter for "multiband" (see algorithms/MultibandWidth.h),
# mirroring python/evaluate_multiband.py's own settings; band 1's width is always 0.
# erAmountPercent/erRoomSizePercent/erPreDelayMs only matter for "earlyrefl" (see
# algorithms/EarlyReflections.h), mirroring python/evaluate_early_reflections.py's own
# settings; erPreDelayMs mirrors GlobalSettings' own default (not a user-facing knob).
# chorusAmountPercent/chorusDepthPercent/chorusRateHz only matter for "chorus" (see
# algorithms/ChorusDoubler.h), mirroring python/evaluate_chorus_doubler.py's own
# settings; chorusRateHz mirrors GlobalSettings' own default (not a user-facing knob).
SETTINGS = [
    ("broadband_w000", "broadband", 0, 150, 8000, 10, 50, 300, 50, 50, 150, 1500, 6000, 100, 100, 100, 50, 50, 5, 50, 50, 0.3),
    ("broadband_w150", "broadband", 150, 150, 8000, 10, 50, 300, 50, 50, 150, 1500, 6000, 100, 100, 100, 50, 50, 5, 50, 50, 0.3),
    ("broadband_w200", "broadband", 200, 150, 8000, 10, 50, 300, 50, 50, 150, 1500, 6000, 100, 100, 100, 50, 50, 5, 50, 50, 0.3),
    ("filtered_w150_bass120_shelf8k", "filtered", 150, 120, 8000, 10, 50, 300, 50, 50, 150, 1500, 6000, 100, 100, 100, 50, 50, 5, 50, 50, 0.3),
    ("filtered_w150_off", "filtered", 150, 20, 20000, 10, 50, 300, 50, 50, 150, 1500, 6000, 100, 100, 100, 50, 50, 5, 50, 50, 0.3),
    ("comb_d10_g050", "comb", 100, 150, 8000, 10, 50, 300, 50, 50, 150, 1500, 6000, 100, 100, 100, 50, 50, 5, 50, 50, 0.3),
    ("comb_d05_g030", "comb", 100, 150, 8000, 5, 30, 300, 50, 50, 150, 1500, 6000, 100, 100, 100, 50, 50, 5, 50, 50, 0.3),
    ("comb_d20_g070", "comb", 100, 150, 8000, 20, 70, 300, 50, 50, 150, 1500, 6000, 100, 100, 100, 50, 50, 5, 50, 50, 0.3),
    ("allpass_a050_s10", "allpass", 100, 150, 8000, 10, 50, 300, 50, 50, 150, 1500, 6000, 100, 100, 100, 50, 50, 5, 50, 50, 0.3),   # amount=0.5, spread=1.0 oct
    ("allpass_a025_s05", "allpass", 100, 150, 8000, 10, 50, 300, 25, 25, 150, 1500, 6000, 100, 100, 100, 50, 50, 5, 50, 50, 0.3),   # amount=0.25, spread=0.5 oct
    ("allpass_a100_s20", "allpass", 100, 150, 8000, 10, 50, 300, 100, 100, 150, 1500, 6000, 100, 100, 100, 50, 50, 5, 50, 50, 0.3), # amount=1.0, spread=2.0 oct
    ("multiband_default", "multiband", 100, 150, 8000, 10, 50, 300, 50, 50, 150, 1500, 6000, 100, 100, 100, 50, 50, 5, 50, 50, 0.3),
    ("multiband_narrow_high", "multiband", 100, 150, 8000, 10, 50, 300, 50, 50, 150, 1500, 6000, 100, 100, 0, 50, 50, 5, 50, 50, 0.3),
    ("multiband_wide_mid", "multiband", 100, 150, 8000, 10, 50, 300, 50, 50, 150, 1500, 6000, 100, 200, 100, 50, 50, 5, 50, 50, 0.3),
    ("multiband_all_narrow", "multiband", 100, 150, 8000, 10, 50, 300, 50, 50, 150, 1500, 6000, 0, 0, 0, 50, 50, 5, 50, 50, 0.3),
    ("earlyrefl_a050_r050", "earlyrefl", 100, 150, 8000, 10, 50, 300, 50, 50, 150, 1500, 6000, 100, 100, 100, 50, 50, 5, 50, 50, 0.3),
    ("earlyrefl_a025_r025", "earlyrefl", 100, 150, 8000, 10, 50, 300, 50, 50, 150, 1500, 6000, 100, 100, 100, 25, 25, 5, 50, 50, 0.3),
    ("earlyrefl_a100_r100", "earlyrefl", 100, 150, 8000, 10, 50, 300, 50, 50, 150, 1500, 6000, 100, 100, 100, 100, 100, 5, 50, 50, 0.3),
    ("chorus_a050_d050", "chorus", 100, 150, 8000, 10, 50, 300, 50, 50, 150, 1500, 6000, 100, 100, 100, 50, 50, 5, 50, 50, 0.3),
    ("chorus_a025_d025", "chorus", 100, 150, 8000, 10, 50, 300, 50, 50, 150, 1500, 6000, 100, 100, 100, 50, 50, 5, 25, 25, 0.3),
    ("chorus_a100_d100", "chorus", 100, 150, 8000, 10, 50, 300, 50, 50, 150, 1500, 6000, 100, 100, 100, 50, 50, 5, 100, 100, 0.3),
]

PLOT_SETTING = "filtered_w150_bass120_shelf8k"
COMB_PLOT_SETTING = "comb_d10_g050"
ALLPASS_PLOT_SETTING = "allpass_a050_s10"
MULTIBAND_PLOT_SETTING = "multiband_default"
EARLY_REFLECTIONS_PLOT_SETTING = "earlyrefl_a050_r050"
CHORUS_PLOT_SETTING = "chorus_a050_d050"
OFF_CHECK_SETTINGS = ("broadband_w150", "filtered_w150_off")


def render(binary, input_path, output_path, algorithm, width, bass_cutoff, high_shelf,
           comb_delay_ms, comb_gain_percent, comb_crossover_hz,
           allpass_amount_percent, allpass_spread_percent,
           mb_freq1, mb_freq2, mb_freq3, mb_width2_percent, mb_width3_percent, mb_width4_percent,
           er_amount_percent, er_room_size_percent, er_pre_delay_ms,
           chorus_amount_percent, chorus_depth_percent, chorus_rate_hz):
    subprocess.run([binary, input_path, output_path, algorithm,
                     str(width), str(bass_cutoff), str(high_shelf),
                     str(comb_delay_ms), str(comb_gain_percent), str(comb_crossover_hz),
                     str(allpass_amount_percent), str(allpass_spread_percent),
                     str(mb_freq1), str(mb_freq2), str(mb_freq3),
                     str(mb_width2_percent), str(mb_width3_percent), str(mb_width4_percent),
                     str(er_amount_percent), str(er_room_size_percent), str(er_pre_delay_ms),
                     str(chorus_amount_percent), str(chorus_depth_percent), str(chorus_rate_hz)],
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
        for (label, algorithm, width, bass_cutoff, high_shelf, comb_delay, comb_gain, comb_xover,
             allpass_amount, allpass_spread, mb_freq1, mb_freq2, mb_freq3,
             mb_width2, mb_width3, mb_width4, er_amount, er_room_size, er_pre_delay,
             chorus_amount, chorus_depth, chorus_rate) in SETTINGS:
            out_path = os.path.join(AUDIO_OUT_DIR, f"{name}_{label}.wav")
            render(binary, in_path, out_path, algorithm, width, bass_cutoff, high_shelf,
                   comb_delay, comb_gain, comb_xover, allpass_amount, allpass_spread,
                   mb_freq1, mb_freq2, mb_freq3, mb_width2, mb_width3, mb_width4,
                   er_amount, er_room_size, er_pre_delay,
                   chorus_amount, chorus_depth, chorus_rate)
            y, _ = audio_io.read_stereo(out_path, expected_fs=fs)
            outputs[label] = y

            ev = report.evaluate(x, y, fs)
            row = (f"{name:<28s} {label:<32s} {ev['in']['correlation']:7.2f} {ev['out']['correlation']:7.2f} "
                   f"{ev['in']['iacc']:7.2f} {ev['out']['iacc']:8.2f} {ev['change']['S_minus_M_dB']:6.1f} "
                   f"{ev['change']['LUFS']:6.1f} {ev['change']['mono_coloration_dB']:8.2f}")
            print(row)
            rows.append(row)

            if label in (PLOT_SETTING, COMB_PLOT_SETTING, ALLPASS_PLOT_SETTING, MULTIBAND_PLOT_SETTING,
                         EARLY_REFLECTIONS_PLOT_SETTING, CHORUS_PLOT_SETTING):
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

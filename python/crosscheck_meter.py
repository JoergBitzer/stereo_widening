"""Cross-check the C++ StereoMeterState against stereo_eval.measures (Phase 2, step 3).

Runs the compiled MeterCrossCheck tool on a set of test signals and compares its
correlation and RMS-level output to the Python measures for the same file.

StereoMeterState is a continuous leaky integrator (tau_s, like a hardware meter), while
stereo_eval.measures.correlation() is the true broadband correlation over the whole
file. They only agree closely for signals that are stationary over several times tau_s
(the noise test signals); for non-stationary material (speech, mixes) a mismatch is
expected, not a bug, so those signals are reported but not asserted on.

Usage:  python python/crosscheck_meter.py [path/to/MeterCrossCheck]
"""

import os
import re
import subprocess
import sys

from stereo_eval import audio_io, measures

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_BINARY = os.path.join(
    PROJECT_DIR, "..", "build", "stereo_widening", "tools", "meter_crosscheck",
    "MeterCrossCheck_artefacts", "Debug", "MeterCrossCheck")

# (file, stationary) -- "stationary" signals get an assertion-strength tolerance check,
# non-stationary ones are printed for information only (see module docstring)
SIGNALS = [
    ("generated/noise_pink_mono.wav", True),
    ("generated/noise_pink_rho050.wav", True),
    ("generated/noise_pink_uncorrelated.wav", True),
    ("generated/noise_pink_antiphase.wav", True),
    ("generated/speech_pan_L50.wav", False),
    ("generated/mix_small.wav", False),
    ("samples/mix_loop_let_it_be.wav", False),
]

TAU_S = 0.3
STATIONARY_TOLERANCE = 0.08  # correlation, absolute
LEVEL_TOLERANCE_DB = 0.5


def run_cpp(binary, path, tau_s=TAU_S):
    result = subprocess.run([binary, path, "512", str(tau_s)], capture_output=True, text=True, check=True)
    values = {}
    for match in re.finditer(r"(\w+)=(-?[\d.]+|nan|-nan)", result.stdout):
        values[match.group(1)] = float(match.group(2))
    return values


def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_BINARY
    if not os.path.exists(binary):
        print(f"MeterCrossCheck binary not found at {binary}\n"
              f"build it first: cmake --build <builddir> --target MeterCrossCheck", file=sys.stderr)
        return 1

    ok = True
    print(f"{'signal':<24s} {'rho py':>7s} {'rho cpp':>8s} {'diff':>6s}  {'M py':>7s} {'M cpp':>7s}  "
          f"{'S py':>7s} {'S cpp':>7s}  stationary")
    for rel_path, stationary in SIGNALS:
        path = os.path.join(PROJECT_DIR, "test_signals", rel_path)
        if not os.path.exists(path):
            print(f"missing {path}", file=sys.stderr)
            continue
        x, fs = audio_io.read_stereo(path)
        py = measures.levels(x, fs)
        py_rho = measures.correlation(x)
        cpp = run_cpp(binary, path)

        rho_diff = cpp["correlation"] - py_rho
        print(f"{os.path.basename(rel_path):<24s} {py_rho:7.3f} {cpp['correlation']:8.3f} {rho_diff:6.3f}  "
              f"{py['M_dB']:7.2f} {cpp['rms_M_dB']:7.2f}  {py['S_dB']:7.2f} {cpp['rms_S_dB']:7.2f}  "
              f"{'yes' if stationary else 'no'}")

        if stationary:
            if abs(rho_diff) > STATIONARY_TOLERANCE:
                print(f"  FAIL correlation mismatch: {abs(rho_diff):.3f} > {STATIONARY_TOLERANCE}", file=sys.stderr)
                ok = False
            for label, py_db, cpp_db in [("M", py["M_dB"], cpp["rms_M_dB"]), ("S", py["S_dB"], cpp["rms_S_dB"])]:
                if py_db < -60 and cpp_db < -60:
                    continue  # both effectively silent (e.g. S of mono noise): dB comparison is meaningless
                if abs(py_db - cpp_db) > LEVEL_TOLERANCE_DB:
                    print(f"  FAIL {label} level mismatch: {abs(py_db - cpp_db):.2f} dB > {LEVEL_TOLERANCE_DB}",
                          file=sys.stderr)
                    ok = False

    print("\nnon-stationary rows are informational (see module docstring), not checked")
    print("OK" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

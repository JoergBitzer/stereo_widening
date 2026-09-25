"""Direct numeric cross-check: Python reference (python/algorithms/early_reflections.py,
run via evaluate_early_reflections.py) vs. the real C++ plugin DSP (StereoWidener/
algorithms/EarlyReflections.cpp, run via tools/widener_render + evaluate_widener_plugin.py).

Same structure and reasoning as crosscheck_comb.py/crosscheck_allpass.py/
crosscheck_multiband.py. This cross-check caught two real C++-side bugs during
development, both in how EarlyReflections::process() reads its single shared
juce::dsp::DelayLine for kNumReflections*2 taps per pushed sample (see
EarlyReflections.cpp for the full account): first, leaving popSample()'s
updateReadPointer at its default true on every call let the read cursor free-run ahead
of the write cursor, corrupting every tap's actual delay within a few dozen samples
(showed up here as mono_coloration_dB up to 16 dB too high on full-spectrum music).
The first fix (passing false on every call) went too far the other way -- it froze the
read cursor entirely, so every tap read one fixed buffer slot refreshed only once per
buffer revolution (~52 ms), audible as periodic crackle but *invisible* to this
cross-check's aggregate statistics (confirmed separately with a direct time-domain
spike detector on the isolated added component, since none of report.evaluate()'s
measures are click-sensitive). The correct fix -- advance the read cursor by exactly
one step per pushed sample, via updateReadPointer=true on only the temporally last of
the kNumReflections*2 popSample() calls -- brought every diff down to 0.000 to the
printed precision, the tightest of all four cross-checks so far.

Run after both python/evaluate_early_reflections.py and python/evaluate_widener_plugin.py
(needs their summary.txt outputs). Usage: python python/crosscheck_early_reflections.py
"""

import os
import re
import sys

PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
PYTHON_SUMMARY = os.path.join(PROJECT_DIR, "results", "early_reflections", "summary.txt")
CPP_SUMMARY = os.path.join(PROJECT_DIR, "results", "widener_plugin", "summary.txt")
OUT_PATH = os.path.join(PROJECT_DIR, "results", "early_reflections", "crosscheck_vs_cpp.txt")

# settings shared by both evaluation scripts: (python label, C++ label)
SETTINGS = [
    ("a050_r050", "earlyrefl_a050_r050"),
    ("a025_r025", "earlyrefl_a025_r025"),
    ("a100_r100", "earlyrefl_a100_r100"),
]

# (metric name, display column index in the original summary table, tolerance) --
# tight, matching allpass's/multiband's own near-exact cross-checks now that both
# read-cursor bugs are fixed (see module docstring); a little headroom above the
# observed 0.000 diffs for float32-vs-float64 rounding on other test material/settings.
METRICS = [
    ("rho out", 3, 0.02),
    ("IACC out", 5, 0.02),
    ("dS-M", 6, 0.3),
    ("dLUFS", 7, 0.2),
    ("mono col", 8, 0.1),
]

ROW_RE = re.compile(r"^(\S+)\s+(\S+)\s+(.*)$")


def parse_summary(path):
    rows = {}
    with open(path) as f:
        for line in f:
            m = ROW_RE.match(line)
            if not m:
                continue
            signal, setting, rest = m.groups()
            fields = rest.split()
            if len(fields) < 6 or setting in ("signal", "in", "out"):
                continue
            try:
                rows[(signal, setting)] = [float(x) for x in fields]
            except ValueError:
                continue
    return rows


def main():
    if not os.path.exists(PYTHON_SUMMARY):
        print(f"missing {PYTHON_SUMMARY} -- run python/evaluate_early_reflections.py first", file=sys.stderr)
        return 1
    if not os.path.exists(CPP_SUMMARY):
        print(f"missing {CPP_SUMMARY} -- run python/evaluate_widener_plugin.py first", file=sys.stderr)
        return 1

    py_rows = parse_summary(PYTHON_SUMMARY)
    cpp_rows = parse_summary(CPP_SUMMARY)

    signals = sorted({signal for (signal, setting) in py_rows if setting in dict(SETTINGS)})

    lines = []
    header = f"{'signal':<28s} {'setting':<14s}" + "".join(f"{name:>10s}" for name, _, _ in METRICS)
    lines.append(header)

    all_ok = True
    max_abs_diff = {name: 0.0 for name, _, _ in METRICS}

    for signal in signals:
        for py_setting, cpp_setting in SETTINGS:
            py = py_rows.get((signal, py_setting))
            cpp = cpp_rows.get((signal, cpp_setting))
            if py is None or cpp is None:
                print(f"  missing row for {signal}/{py_setting}", file=sys.stderr)
                all_ok = False
                continue

            row_bits = []
            for name, idx, tol in METRICS:
                field_idx = idx - 2  # display column -> 0-indexed [rho_in, rho_out, iacc_in, iacc_out, dS-M, dLUFS, mono_col]
                diff = abs(py[field_idx] - cpp[field_idx])
                max_abs_diff[name] = max(max_abs_diff[name], diff)
                ok = diff <= tol
                all_ok = all_ok and ok
                row_bits.append(f"{diff:9.3f}{'' if ok else '*'}")
            lines.append(f"{signal:<28s} {py_setting:<14s}" + "".join(row_bits))

    lines.append("")
    lines.append("max |python - c++| per metric:")
    for name, _, tol in METRICS:
        lines.append(f"  {name:<10s} {max_abs_diff[name]:.3f} (tolerance {tol})")
    lines.append("")
    lines.append("* marks a diff exceeding tolerance")
    lines.append("PASS" if all_ok else "FAIL")

    report = "\n".join(lines)
    print(report)
    os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
    with open(OUT_PATH, "w") as f:
        f.write(report + "\n")
    print(f"\nresults in {OUT_PATH}")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())

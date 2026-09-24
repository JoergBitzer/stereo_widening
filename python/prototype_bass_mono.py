"""Compare the bass mono variants (see algorithms/bass_mono.py).

Part 1 (analytic): filter responses, and interchannel phase (IPD) and level (ILD)
difference for a source panned 50 % left after M/S width 1.5 with bass mono.
An ideal bass mono keeps IPD = 0 for a panned (mono) source at all frequencies.

Part 2 (signals): M/S width 1.5 with each variant on the test material.

Output: python/results/bass_mono/summary.txt and bass_mono_comparison.png

Usage:  python python/prototype_bass_mono.py [fc_hz]
"""

import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

from algorithms import bass_mono  # noqa: E402
from algorithms.ms_width import ms_width  # noqa: E402
from stereo_eval import audio_io, measures, report  # noqa: E402

FS = audio_io.PROJECT_FS
WIDTH = 1.5
PAN = -0.5  # source 50 % left

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESULT_DIR = os.path.join(PROJECT_DIR, "python", "results", "bass_mono")
SIGNALS = [
    "generated/speech_pan_L50.wav",
    "generated/noise_pink_rho050.wav",
    "generated/mix_small.wav",
    "samples/mix_loop_let_it_be.wav",
]
COST = {"hp2": "1 biquad", "complementary": "1 biquad", "lr4_allpass": "3 biquads",
        "linear_phase": f"2 FIR x {bass_mono.LINEAR_PHASE_TAPS} taps"}


class Tee:
    """Print to the console and collect the lines for summary.txt."""

    def __init__(self):
        self.lines = []

    def __call__(self, text=""):
        print(text)
        self.lines.append(text)


def panned_source_response(variant, fc, f):
    """Transfer functions from a mono source (panned by PAN) to L' and R'."""
    angle = (PAN + 1.0) * np.pi / 4.0
    g_left, g_right = np.cos(angle), np.sin(angle)
    h_mid, h_side = bass_mono.frequency_responses(variant, fc, FS, f)
    mid_part = 0.5 * (g_left + g_right) * h_mid
    side_part = WIDTH * 0.5 * (g_left - g_right) * h_side
    return mid_part + side_part, mid_part - side_part


def analytic_part(fc, out):
    f = np.geomspace(10.0, 20000.0, 2000)
    fig, axes = plt.subplots(1, 3, figsize=(16, 4.8))
    out(f"Part 1: analytic, fc = {fc:g} Hz, width = {WIDTH}, source panned {PAN * 100:+.0f} %")
    out(f"  {'variant':<14s} {'S@fc/2':>7s} {'S@fc/4':>7s} {'S bump':>7s} {'max IPD':>8s} "
        f"{'IPD>2fc':>8s} {'latency':>9s}  cost")
    for variant in bass_mono.VARIANTS:
        _, h_side = bass_mono.frequency_responses(variant, fc, FS, f)
        h_left, h_right = panned_source_response(variant, fc, f)
        ipd = np.degrees(np.angle(h_left / h_right))
        ild = 20 * np.log10(np.abs(h_left) / np.abs(h_right))
        s_db = 20 * np.log10(np.maximum(np.abs(h_side), 1e-12))

        def s_at(freq):
            return s_db[np.argmin(np.abs(f - freq))]

        latency_ms = 1000 * bass_mono.latency_samples(variant) / FS
        out(f"  {variant:<14s} {s_at(fc / 2):7.1f} {s_at(fc / 4):7.1f} {np.max(s_db):7.2f} "
            f"{np.max(np.abs(ipd)):8.1f} {np.max(np.abs(ipd[f > 2 * fc])):8.2f} {latency_ms:7.1f}ms  "
            f"{COST[variant]}")
        axes[0].semilogx(f, s_db, label=variant)
        axes[1].semilogx(f, ipd, label=variant)
        axes[2].semilogx(f, ild, label=variant)
    out("  S@: side gain in dB, S bump: max side gain (> 0 dB = boost), IPD in degrees")

    axes[0].set_ylim(-60, 5)
    axes[0].set_ylabel("side gain / dB")
    axes[0].set_title("Side-channel response")
    axes[1].set_ylabel("IPD / degree")
    axes[1].set_title(f"Interchannel phase, source {PAN * 100:+.0f} %, width {WIDTH}")
    axes[2].set_ylabel("ILD / dB")
    axes[2].set_title("Interchannel level difference")
    for ax in axes:
        ax.axvline(fc, color="gray", linestyle=":")
        ax.set_xlabel("f / Hz")
        ax.grid(True, which="both", alpha=0.3)
        ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(RESULT_DIR, f"bass_mono_comparison_fc{fc:g}.png"), dpi=100)
    plt.close(fig)


def signal_part(fc, out):
    out()
    out(f"Part 2: signals, M/S width {WIDTH}, bass mono fc = {fc:g} Hz")
    out(f"  {'signal':<22s} {'variant':<14s} {'rho out':>7s} {'min band rho >2fc':>17s} "
        f"{'IACC out':>8s} {'mono col':>8s} {'dLUFS':>6s}")
    for rel_path in SIGNALS:
        path = os.path.join(PROJECT_DIR, "test_signals", rel_path)
        if not os.path.exists(path):
            out(f"  missing {rel_path}")
            continue
        x, fs = audio_io.read_stereo(path)
        name = os.path.splitext(os.path.basename(path))[0]
        for variant in ("none",) + bass_mono.VARIANTS:
            if variant == "none":
                y = ms_width(x, fs, width=WIDTH)
            else:
                y = ms_width(x, fs, width=WIDTH, bass_mono_hz=fc, bass_mono_mode=variant)
                delay = bass_mono.latency_samples(variant)
                y = y[delay:]  # compensate latency for the comparison
                y = np.vstack([y, np.zeros((delay, 2))])
            ev = report.evaluate(x, y, fs)
            f_c, rho = measures.band_correlation(y, fs)
            out(f"  {name:<22s} {variant:<14s} {ev['out']['correlation']:7.2f} "
                f"{np.nanmin(rho[f_c > 2 * fc]):17.2f} {ev['out']['iacc']:8.2f} "
                f"{ev['change']['mono_coloration_dB']:8.2f} {ev['change']['LUFS']:6.2f}")


def main():
    fc = float(sys.argv[1]) if len(sys.argv) > 1 else 120.0
    os.makedirs(RESULT_DIR, exist_ok=True)
    out = Tee()
    analytic_part(fc, out)
    signal_part(fc, out)
    summary = os.path.join(RESULT_DIR, f"summary_fc{fc:g}.txt")
    with open(summary, "w") as f:
        f.write("\n".join(out.lines) + "\n")
    print(f"\nresults in {RESULT_DIR}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

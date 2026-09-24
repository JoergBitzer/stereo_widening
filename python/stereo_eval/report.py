"""Evaluation report: all measures for one signal, or input vs. output of an algorithm.

The same functions are used for the Python prototypes and for audio rendered
by the plugin, so the results can be compared directly.
"""

import os

import matplotlib
import numpy as np

from . import binaural, measures


def analyze(x, fs, with_iacc=True):
    """All single-signal measures for x (N, 2). Returns a dict."""
    result = {"correlation": measures.correlation(x)}
    result.update(measures.levels(x, fs))
    result["band_f"], result["band_correlation"] = measures.band_correlation(x, fs)
    result["time_t"], result["time_correlation"] = measures.correlation_over_time(x, fs)
    if with_iacc:
        ears = binaural.loudspeaker_to_ears(x, fs)
        result["iacc"] = binaural.iacc(ears, fs)
        result["iacc_band_f"], result["iacc_band"] = binaural.band_iacc(ears, fs)
    return result


def evaluate(x_in, x_out, fs, with_iacc=True):
    """Compare input and output of a stereo algorithm. Returns {'in', 'out', 'mono_sum', 'change'}."""
    res_in = analyze(x_in, fs, with_iacc)
    res_out = analyze(x_out, fs, with_iacc)
    mono = measures.mono_sum_coloration(x_in, x_out, fs)
    change = {
        "correlation": res_out["correlation"] - res_in["correlation"],
        "rms_dB": res_out["rms_dB"] - res_in["rms_dB"],
        "LUFS": res_out["LUFS"] - res_in["LUFS"],
        "S_minus_M_dB": res_out["S_minus_M_dB"] - res_in["S_minus_M_dB"],
        "mono_offset_dB": mono["offset_dB"],
        "mono_coloration_dB": mono["coloration_dB"],
    }
    if with_iacc:
        change["iacc"] = res_out["iacc"] - res_in["iacc"]
    return {"in": res_in, "out": res_out, "mono_sum": mono, "change": change}


SCALAR_KEYS = ["correlation", "iacc", "L_dB", "R_dB", "M_dB", "S_dB", "S_minus_M_dB", "rms_dB", "LUFS"]


def format_analysis(result, title=""):
    lines = [title] if title else []
    for key in SCALAR_KEYS:
        if key in result:
            lines.append(f"  {key:<14s} {result[key]:8.3f}")
    return "\n".join(lines)


def format_evaluation(ev, title=""):
    lines = [title] if title else []
    lines.append(f"  {'measure':<14s} {'in':>8s} {'out':>8s} {'change':>8s}")
    for key in SCALAR_KEYS:
        if key in ev["in"]:
            v_in, v_out = ev["in"][key], ev["out"][key]
            lines.append(f"  {key:<14s} {v_in:8.3f} {v_out:8.3f} {v_out - v_in:8.3f}")
    lines.append(f"  {'mono offset':<14s} {'':8s} {'':8s} {ev['change']['mono_offset_dB']:8.3f}")
    lines.append(f"  {'mono colour.':<14s} {'':8s} {'':8s} {ev['change']['mono_coloration_dB']:8.3f}")
    return "\n".join(lines)


def _goniometer(ax, x, title, max_points=20000):
    """Lissajous plot: S on the x-axis, M on the y-axis (mono = vertical line)."""
    mid, side = measures.mid_side(x)
    step = max(1, len(x) // max_points)
    peak = max(np.max(np.abs(mid)), np.max(np.abs(side)), 1e-9)
    ax.plot(side[::step] / peak, mid[::step] / peak, ".", markersize=1, alpha=0.3)
    ax.set_xlim(-1, 1)
    ax.set_ylim(-1, 1)
    ax.set_aspect("equal")
    ax.set_xlabel("S")
    ax.set_ylabel("M")
    ax.set_title(title)


def _octave_ticks(ax):
    centres = binaural.OCTAVE_CENTRES
    ax.set_xticks(centres)
    ax.set_xticklabels([f"{c / 1000:g}k" if c >= 1000 else f"{c:g}" for c in centres])
    ax.minorticks_off()


def plot_evaluation(ev, x_in, x_out, path, title=""):
    """Save a figure with goniometers, band correlation, mono sum, IACC and correlation over time."""
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(2, 3, figsize=(15, 9))
    _goniometer(axes[0, 0], x_in, "Goniometer in")
    _goniometer(axes[0, 1], x_out, "Goniometer out")

    ax = axes[0, 2]
    ax.semilogx(ev["in"]["band_f"], ev["in"]["band_correlation"], "o-", label="in")
    ax.semilogx(ev["out"]["band_f"], ev["out"]["band_correlation"], "o-", label="out")
    ax.set_ylim(-1.05, 1.05)
    ax.set_xlabel("f / Hz")
    ax.set_ylabel("correlation")
    ax.set_title("Correlation per 1/3 octave")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()

    ax = axes[1, 0]
    ax.semilogx(ev["mono_sum"]["f_centre"], ev["mono_sum"]["diff_dB"], "o-")
    ax.axhline(ev["mono_sum"]["offset_dB"], color="gray", linestyle="--", label="broadband offset")
    ax.set_xlabel("f / Hz")
    ax.set_ylabel("dB")
    ax.set_title(f"Mono sum out - in (colouration {ev['mono_sum']['coloration_dB']:.1f} dB)")
    lo, hi = np.nanmin(ev["mono_sum"]["diff_dB"]), np.nanmax(ev["mono_sum"]["diff_dB"])
    ax.set_ylim(min(lo, -1.0) - 0.5, max(hi, 1.0) + 0.5)  # at least +-1 dB, hides numerical noise
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()

    ax = axes[1, 1]
    if "iacc_band" in ev["in"]:
        ax.semilogx(ev["in"]["iacc_band_f"], ev["in"]["iacc_band"], "o-", label=f"in ({ev['in']['iacc']:.2f})")
        ax.semilogx(ev["out"]["iacc_band_f"], ev["out"]["iacc_band"], "o-", label=f"out ({ev['out']['iacc']:.2f})")
        ax.set_ylim(0, 1.05)
        ax.legend()
    ax.set_xlabel("f / Hz")
    ax.set_ylabel("IACC")
    ax.set_title("IACC per octave (loudspeakers +-30 deg)")
    _octave_ticks(ax)
    ax.grid(True, which="both", alpha=0.3)

    ax = axes[1, 2]
    ax.plot(ev["in"]["time_t"], ev["in"]["time_correlation"], label="in")
    ax.plot(ev["out"]["time_t"], ev["out"]["time_correlation"], label="out")
    ax.set_ylim(-1.05, 1.05)
    ax.set_xlabel("t / s")
    ax.set_ylabel("correlation")
    ax.set_title("Correlation over time (100 ms blocks)")
    ax.grid(True, alpha=0.3)
    ax.legend()

    fig.suptitle(title)
    fig.tight_layout()
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    fig.savefig(path, dpi=100)
    plt.close(fig)

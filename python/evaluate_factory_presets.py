"""Evaluate StereoWidener's factory presets (python/make_factory_presets.py).

Each preset is rendered through the compiled WidenerRender tool (the plugin's own
algorithm classes) on a source close to its intended use, then measured with
stereo_eval: loudness change (LUFS), correlation before/after, the level change of the
mono sum L+R and its colouration (max deviation of the 1/3-octave mono-sum spectrum
from that broadband offset). Presets for mono sources get a mono version of their
source (L = R = mid), because that is the case they are made for.

The utility section is neutral in every preset except for the output Gain (loudness
compensation, since there is no auto gain), which is applied here after WidenerRender
(algorithm only) -- the same order as in the plugin.

Usage:  python python/evaluate_factory_presets.py [path/to/WidenerRender]

(c) J. Bitzer, Jade HS, MIT license
"""

import os
import subprocess
import sys
import tempfile

import numpy as np

from stereo_eval import audio_io, measures
from make_factory_presets import PRESETS, DEFAULTS

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIGNAL_DIR = os.path.join(PROJECT_DIR, "test_signals")
DEFAULT_BINARY = os.path.join(
    PROJECT_DIR, "..", "build", "stereo_widening", "tools", "widener_render",
    "WidenerRender_artefacts", "Debug", "WidenerRender")

# source -> (test signal, force mono)
SOURCES = {
    "mono synth": ("samples/synth_loop.wav", True),
    "mono arpeggio": ("samples/synth_loop.wav", True),
    "mono electric piano": ("samples/synth_loop.wav", True),
    "mono organ": ("samples/synth_loop.wav", True),
    "mono string patch": ("samples/synth_loop.wav", True),
    "mono clean guitar": ("samples/synth_loop.wav", True),
    "mono distorted guitar": ("samples/synth_loop.wav", True),
    "mono acoustic guitar": ("samples/synth_loop.wav", True),
    "mono synth bass": ("samples/bass_loop.wav", True),
    "mono bass guitar": ("samples/bass_loop.wav", True),
    "mono vocal": ("samples/vocal_phrase_always.wav", True),
    "backing vocals": ("samples/vocal_phrase_holdme.wav", False),
    "mono percussion": ("samples/drums_loop_02.wav", True),
    "stereo pad": ("generated/noise_pink_rho050.wav", False),
    "stereo piano": ("generated/noise_pink_rho050.wav", False),
    "stereo acoustic guitar": ("generated/noise_pink_rho050.wav", False),
    "stereo overheads": ("generated/noise_pink_rho050.wav", False),
    "stereo drum bus": ("generated/noise_pink_rho050.wav", False),
    "full mix": ("samples/mix_loop_carry_on.wav", False),
}

WIDTH_PARAM = {"broadband": "broadbandWidth", "filtered": "filteredWidth", "comb": "combWidth",
               "allpass": "allpassWidth", "multiband": "broadbandWidth", "earlyrefl": "earlyReflWidth",
               "chorus": "chorusWidth"}


def render_args(algorithm, p):
    names = ["bassCutoff", "highShelfFreq", "combDelay", "combGain", "combCrossover",
             "allpassAmount", "allpassSpread", "multibandFreq1", "multibandFreq2", "multibandFreq3",
             "multibandWidth2", "multibandWidth3", "multibandWidth4",
             "earlyReflAmount", "earlyReflRoomSize", "earlyReflPreDelay",
             "chorusAmount", "chorusDepth", "chorusRate", "highShelfGain"]
    return [algorithm, str(p[WIDTH_PARAM[algorithm]])] + [str(p[n]) for n in names]


def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_BINARY
    if not os.path.isfile(binary):
        sys.exit(f"WidenerRender not found at {binary}; build the WidenerRender target first")

    print(f"{'preset':32s} {'source':24s} {'dLUFS':>6s} {'corr in':>7s} {'corr out':>8s} "
          f"{'S-M out':>7s} {'mono dB':>7s} {'mono col':>8s}")
    with tempfile.TemporaryDirectory() as tmp:
        for _, name, algorithm, values, source in PRESETS:
            if source not in SOURCES:
                continue
            path, force_mono = SOURCES[source]
            x, fs = audio_io.read_stereo(os.path.join(SIGNAL_DIR, path))
            if force_mono:
                mid = 0.5 * (x[:, 0] + x[:, 1])
                x = np.stack([mid, mid], axis=1)
            in_path, out_path = os.path.join(tmp, "in.wav"), os.path.join(tmp, "out.wav")
            audio_io.write_stereo(in_path, x, fs)
            p = {k: d[0] for k, d in DEFAULTS.items()}
            p.update(values)
            subprocess.run([binary, in_path, out_path] + render_args(algorithm, p),
                           capture_output=True, text=True, check=True)
            y, _ = audio_io.read_stereo(out_path)
            y = y * 10.0 ** (p["outputGain"] / 20.0)  # the utility section's output Gain
            n = min(len(x), len(y))
            x, y = x[:n], y[:n]
            lx, ly = measures.levels(x, fs), measures.levels(y, fs)
            mono = measures.mono_sum_coloration(x, y, fs)
            print(f"{name:32s} {source:24s} {ly['LUFS'] - lx['LUFS']:6.1f} {measures.correlation(x):7.2f} "
                  f"{measures.correlation(y):8.2f} {ly['S_minus_M_dB']:7.1f} {mono['offset_dB']:7.1f} "
                  f"{mono['coloration_dB']:8.1f}")


if __name__ == "__main__":
    main()

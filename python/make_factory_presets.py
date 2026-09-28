"""Generate StereoWidener's factory presets (StereoWidener/presets/*.xml).

One XML file per preset, in the format PresetHandler writes itself (the APVTS state:
<StereoWidenerVTS presetname=.. bank=.. version=.. category=..> with one
<PARAM id value/> per parameter). Every file holds ALL parameters: anything a preset
doesn't set gets its default, so loading a preset always gives the same sound (the
utility section -- rotation, balance, flip, output gain, monitor -- stays neutral).

The presets are the ideas table of plan2.md, Phase 6, 3.1. Each file carries
presetversion="N": raise a preset's version when its values change, so an update
reaches users who still have the unmodified factory copy (see PresetHandler).

DEFAULTS is the plugin's own default state (a fresh Init.xml, v0.1.30). Pass the path
of a freshly written Init.xml to check that DEFAULTS still matches the plugin's
parameter IDs:

Usage:  python python/make_factory_presets.py [path/to/fresh/Init.xml]

(c) J. Bitzer, Jade HS, MIT license
"""

import os
import re
import sys
import xml.etree.ElementTree as ET

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRESET_DIR = os.path.join(PROJECT_DIR, "StereoWidener", "presets")
CMAKE_FILE = os.path.join(PROJECT_DIR, "StereoWidener", "CMakeLists.txt")

# order of g_algorithmNames (StereoWidener.h); the preset stores the index
ALGORITHMS = ["broadband", "filtered", "comb", "allpass", "multiband", "earlyrefl", "chorus"]

# parameter id -> (default, min, max)
DEFAULTS = {
    "algorithm": (0, 0, 6),
    "allpassAmount": (0, 0, 100), "allpassSpread": (50, 0, 100), "allpassWidth": (100, 0, 200),
    "balance": (0, -100, 100),
    "bassCutoff": (30, 30, 500),                # < 40 Hz = Off
    "broadbandWidth": (100, 0, 200),
    "chorusAmount": (0, 0, 100), "chorusDepth": (50, 0, 100), "chorusRate": (0.3, 0.05, 2),
    "chorusWidth": (100, 0, 200),
    "combCrossover": (300, 50, 2000), "combDelay": (10, 5, 20), "combGain": (0, 0, 100),
    "combWidth": (100, 0, 200),
    "earlyReflAmount": (0, 0, 100), "earlyReflPreDelay": (5, 0, 20), "earlyReflRoomSize": (50, 0, 100),
    "earlyReflWidth": (100, 0, 200),
    "filteredWidth": (100, 0, 200),
    "highShelfFreq": (16500, 1000, 16500),      # > 16 kHz = Off
    "highShelfGain": (3, -6, 6),
    "invertL": (0, 0, 1), "invertR": (0, 0, 1),
    "monitorMode": (0, 0, 10),
    "multibandFreq1": (150, 40, 18000), "multibandFreq2": (1500, 40, 18000), "multibandFreq3": (6000, 40, 18000),
    "multibandWidth2": (100, 0, 200), "multibandWidth3": (100, 0, 200), "multibandWidth4": (100, 0, 200),
    "outputGain": (0, -24, 24),
    "rotation": (0, -180, 180),
    "swapLR": (0, 0, 1),
}

# (file name, preset name, algorithm, {parameter: value}, source used by evaluate_factory_presets.py)
# outputGain: loudness compensation where a preset changes the loudness by 1 dB or more
# on its source (measured by evaluate_factory_presets.py, rounded to 0.5 dB).
PRESETS = [
    ("SynthLead_PseudoStereo", "Synth Lead - Pseudo Stereo", "comb",
     {"combDelay": 12, "combGain": 60, "combCrossover": 300, "combWidth": 120, "outputGain": -1.5}, "mono synth"),
    ("SynthBass_WideTop", "Synth Bass - Wide Top", "comb",
     {"combDelay": 8, "combGain": 60, "combCrossover": 250}, "mono synth bass"),
    ("SynthPad_Big", "Synth Pad - Big", "multiband",
     {"multibandFreq1": 200, "multibandFreq2": 2000, "multibandFreq3": 8000,
      "multibandWidth2": 110, "multibandWidth3": 140, "multibandWidth4": 160}, "stereo pad"),
    ("SynthArp_Shimmer", "Synth Arp - Shimmer", "allpass",
     {"allpassAmount": 35, "allpassSpread": 40, "allpassWidth": 110, "outputGain": 2.5}, "mono arpeggio"),
    ("GuitarClean_ChorusWide", "Guitar Clean - Chorus Wide", "chorus",
     {"chorusAmount": 50, "chorusDepth": 40, "chorusRate": 0.4, "chorusWidth": 120, "outputGain": 2.5}, "mono clean guitar"),
    ("GuitarRhythm_Double", "Guitar Rhythm - Double", "chorus",
     {"chorusAmount": 35, "chorusDepth": 30, "chorusRate": 0.2, "chorusWidth": 130, "outputGain": 2.5}, "mono distorted guitar"),
    ("AcousticGuitar_MonoMic", "Acoustic Guitar - Mono Mic", "earlyrefl",
     {"earlyReflAmount": 35, "earlyReflRoomSize": 30, "earlyReflPreDelay": 3, "earlyReflWidth": 110},
     "mono acoustic guitar"),
    ("AcousticGuitar_StereoPair", "Acoustic Guitar - Stereo Pair", "filtered",
     {"filteredWidth": 130, "bassCutoff": 120, "highShelfFreq": 6000, "highShelfGain": 2, "outputGain": -1.0}, "stereo acoustic guitar"),
    ("Piano_Stereo", "Piano - Stereo", "multiband",
     {"multibandFreq1": 150, "multibandFreq2": 800, "multibandFreq3": 5000,
      "multibandWidth2": 100, "multibandWidth3": 120, "multibandWidth4": 140}, "stereo piano"),
    ("ElectricPiano_PseudoStereo", "Electric Piano - Pseudo Stereo", "comb",
     {"combDelay": 7, "combGain": 50, "combCrossover": 250}, "mono electric piano"),
    ("Organ_RotaryFeel", "Organ - Rotary Feel", "chorus",
     {"chorusAmount": 60, "chorusDepth": 60, "chorusRate": 1.5, "chorusWidth": 130, "outputGain": 2.0}, "mono organ"),
    ("Strings_MonoPatch", "Strings - Mono Patch", "allpass",
     {"allpassAmount": 30, "allpassSpread": 40, "allpassWidth": 110, "outputGain": 2.5}, "mono string patch"),
    ("LeadVocal_Space", "Lead Vocal - Space", "earlyrefl",
     {"earlyReflAmount": 20, "earlyReflRoomSize": 25, "earlyReflPreDelay": 8, "earlyReflWidth": 100}, "mono vocal"),
    ("BackingVocals_Wide", "Backing Vocals - Wide", "chorus",
     {"chorusAmount": 45, "chorusDepth": 50, "chorusRate": 0.3, "chorusWidth": 140, "outputGain": 2.0}, "backing vocals"),
    ("Drums_Overheads", "Drums - Overheads", "filtered",
     {"filteredWidth": 130, "bassCutoff": 150, "highShelfFreq": 8000, "highShelfGain": 2}, "stereo overheads"),
    ("Drums_Bus", "Drums - Bus", "multiband",
     {"multibandFreq1": 120, "multibandFreq2": 1000, "multibandFreq3": 6000,
      "multibandWidth2": 100, "multibandWidth3": 115, "multibandWidth4": 135}, "stereo drum bus"),
    ("Percussion_MonoShaker", "Percussion - Mono Shaker", "allpass",
     {"allpassAmount": 25, "allpassSpread": 40, "allpassWidth": 110, "outputGain": 1.5}, "mono percussion"),
    ("BassGuitar_GritOnly", "Bass Guitar - Grit Only", "comb",
     {"combDelay": 10, "combGain": 60, "combCrossover": 400}, "mono bass guitar"),
    ("Master_GentleWiden", "Master - Gentle Widen", "filtered",
     {"filteredWidth": 115, "bassCutoff": 100, "highShelfFreq": 10000, "highShelfGain": 1.5}, "full mix"),
    ("Master_TightLowEnd", "Master - Tight Low End", "multiband",
     {"multibandFreq1": 120, "multibandFreq2": 1000, "multibandFreq3": 8000,
      "multibandWidth2": 100, "multibandWidth3": 105, "multibandWidth4": 115}, "full mix"),
    # neutral starting point: all defaults (what "Init" used to be on a fresh install)
    ("Init", "Init", "broadband", {}, "-"),
]
PRESET_VERSION = 1


def plugin_version():
    with open(CMAKE_FILE) as f:
        return re.search(r"project\(\$\{TARGET_NAME\} VERSION ([0-9.]+)\)", f.read()).group(1)


def check_against_plugin(init_xml):
    """The plugin's parameter IDs must be exactly DEFAULTS' keys, with the same defaults."""
    plugin = {p.get("id"): float(p.get("value")) for p in ET.parse(init_xml).getroot().iter("PARAM")}
    missing, unknown = sorted(set(plugin) - set(DEFAULTS)), sorted(set(DEFAULTS) - set(plugin))
    differ = [k for k in plugin if k in DEFAULTS and abs(plugin[k] - DEFAULTS[k][0]) > 1e-3 * max(1, abs(plugin[k]))]
    if missing or unknown or differ:
        sys.exit(f"DEFAULTS out of date: missing {missing}, unknown {unknown}, different defaults {differ}")
    print(f"DEFAULTS match {init_xml} ({len(plugin)} parameters)")


def format_value(v):
    return repr(float(v))


def preset_xml(name, algorithm, values, version):
    params = {k: d[0] for k, d in DEFAULTS.items()}
    params["algorithm"] = ALGORITHMS.index(algorithm)
    for k, v in values.items():
        if k not in DEFAULTS:
            sys.exit(f"{name}: unknown parameter {k}")
        lo, hi = DEFAULTS[k][1], DEFAULTS[k][2]
        if not lo <= v <= hi:
            sys.exit(f"{name}: {k} = {v} outside {lo}..{hi}")
        params[k] = v
    f = [params[f"multibandFreq{i}"] for i in (1, 2, 3)]
    if not (f[0] * 1.05 <= f[1] and f[1] * 1.05 <= f[2]):
        sys.exit(f"{name}: crossovers not ascending (at least 5 % apart): {f}")
    lines = ['<?xml version="1.0" encoding="UTF-8"?>', "",
             f'<StereoWidenerVTS presetname="{name}" bank="Factory" version="{version}" '
             f'category="Unknown" presetversion="{PRESET_VERSION}">']
    lines += [f'  <PARAM id="{k}" value="{format_value(params[k])}"/>' for k in sorted(params)]
    lines.append("</StereoWidenerVTS>")
    return "\n".join(lines) + "\n"


def main():
    if len(sys.argv) > 1:
        check_against_plugin(sys.argv[1])
    os.makedirs(PRESET_DIR, exist_ok=True)
    version = plugin_version()
    for old in os.listdir(PRESET_DIR):
        if old.endswith(".xml"):
            os.remove(os.path.join(PRESET_DIR, old))
    for file_name, name, algorithm, values, _ in PRESETS:
        with open(os.path.join(PRESET_DIR, file_name + ".xml"), "w") as f:
            f.write(preset_xml(name, algorithm, values, version))
        print(f"{file_name + '.xml':36s} {name:32s} {algorithm}")
    print(f"{len(PRESETS)} presets written to {PRESET_DIR}")


if __name__ == "__main__":
    main()

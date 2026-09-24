# StereoWidener

A stereo-width processing JUCE plugin. Part of the [stereo_widening](../README.md)
project; built from a copy of [StereoAnalyzer](../StereoAnalyzer/), sharing its metering
components (`shared/metering/`) so both plugins show the same goniometer/level-meter
look and any fix helps both.

See [../docs/algorithms/phase3_stereo_widener.md](../docs/algorithms/phase3_stereo_widener.md)
for the architecture, the algorithm-switch crossfade, and the two algorithms, and
[../docs/algorithms/phase4_settings.md](../docs/algorithms/phase4_settings.md) for the
global settings file.

## Build

From the shared `AudioDev/build` directory (see the top-level `../README.md`):

```console
cmake --build . --target StereoWidener_VST3 -j8
cmake --build . --target StereoWidener_Standalone -j8
```

## Evaluating the algorithms

`tools/widener_render` (built the same way, target `WidenerRender`) renders a wav file
through the real algorithm classes headlessly. `python/evaluate_widener_plugin.py` runs
it across the project's test-signal corpus and measures the results with
`python/stereo_eval`, writing a summary and plots to `python/results/widener_plugin/`:

```console
cmake --build . --target WidenerRender -j8
cd ../stereo_widening/python && python evaluate_widener_plugin.py
```

## Parameters

- **Width**: 0-200 %. 0 collapses the side signal to mono, 100 is unity (unchanged from
  the input), 200 doubles the side signal.
- **Algorithm**: which stereo-widening algorithm processes the signal, selected from the
  list below the Width knob. Switching is crossfaded (equal-power, 30 ms) so it never
  clicks. Click the "?" button next to the selector for an explanation of the active
  algorithm, with a citation to a written source.
  - *M/S Width (Broadband)*: plain M/S width control across the whole spectrum. The two
    knobs flanking Width are unused (greyed out) for this algorithm.
  - *M/S Width (Filtered / Bass Mono)*: the same control, but the side signal is
    high-pass filtered first, so bass content is forced mono and only the highs get
    widened, then high-shelved to restore some "air". The left knob sets the **Bass
    Cutoff** (40-500 Hz, turn below 40 Hz for "Off" -- bypasses the high-pass entirely),
    the right knob sets the **High Shelf** frequency (1000-16000 Hz, turn above 16 kHz
    for "Off" -- bypasses the shelf entirely). The shelf's gain (default 3 dB) is not a
    parameter; it's read from the global settings file, see below.
- **Utilities** (below the algorithm selector; applied regardless of which algorithm is
  selected, see [phase4_settings.md](../docs/algorithms/phase4_settings.md)):
  - **Rotation**: -45..+45 degrees, the stereo image's rotation in the L/R plane.
  - **Balance**: -100..+100 %. Positive attenuates L (image moves right), negative
    attenuates R (image moves left).
  - **Swap / Inv L / Inv R**: swap the two channels, or invert either channel's polarity.
  - **Monitor**: Normal / Mono Check (listen to L+R) / Solo Side (listen to S) --
    auditioning modes that override the final output; always switch back to Normal
    before bouncing/exporting.

## Global settings file

See [../docs/algorithms/phase4_settings.md](../docs/algorithms/phase4_settings.md).
`~/.config/StereoWidener/settings.json` (created automatically on first run) stores
user-wide defaults that aren't part of a DAW project's own saved state -- a project's
saved parameter values and GUI size always take priority over these once they exist:

```json
{
  "highShelfGainDb": 3.0,
  "guiScaleFactor": 1.0,
  "meterIntegrationTimeS": 0.3,
  "meterPeakHoldTimeS": 1.5,
  "meterPeakDecayDbPerS": 20.0,
  "lastUsedState": { "width": 100.0, "algorithm": 0, "...": "..." }
}
```

Edit and save while the plugin/DAW is closed; it's read once when a plugin instance is
created, not watched live. `lastUsedState` is written automatically (whenever a plugin
instance closes) and used to seed a brand new instance's starting values -- editing it
by hand works too, but it will be overwritten the next time an instance closes.

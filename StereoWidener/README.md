# StereoWidener

A stereo-width processing JUCE plugin. Part of the [stereo_widening](../README.md)
project; built from a copy of [StereoAnalyzer](../StereoAnalyzer/), sharing its metering
components (`shared/metering/`) so both plugins show the same goniometer/level-meter
look and any fix helps both.

See [../docs/algorithms/phase3_stereo_widener.md](../docs/algorithms/phase3_stereo_widener.md)
for the architecture, the algorithm-switch crossfade, and the first two algorithms,
[../docs/algorithms/phase4_settings.md](../docs/algorithms/phase4_settings.md) for the
global settings file, and
[../docs/algorithms/phase5_comb.md](../docs/algorithms/phase5_comb.md) for the third
algorithm (comb pseudo-stereo) and the aux-knob rebinding mechanism it introduced, and
[../docs/algorithms/phase5_allpass.md](../docs/algorithms/phase5_allpass.md) for the
fourth algorithm (allpass decorrelation) and the "not mono-safe" badge it introduced.

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
    Cutoff** (40-500 Hz, turn below 40 Hz for "Off" -- bypasses the high-pass entirely,
    and is the default: double-clicking the knob resets to Off, not to some fixed
    cutoff), the right knob sets the **High Shelf** frequency (1000-16000 Hz, turn above
    16 kHz for "Off" -- bypasses the shelf entirely, and is likewise the default). The
    shelf's gain (default 3 dB) is not a parameter; it's read from the global settings
    file, see below.
  - *Complementary Comb (Pseudo-Stereo)*: a delayed, gained copy of the mid signal is
    added to the side signal (Lauridsen/Schroeder pseudo-stereo) -- unlike the two M/S
    algorithms above, this one creates real width even from dual-mono input. The left
    knob sets **Delay** (5-20 ms), the right knob sets **Gain** (0-100 %, defaults to
    0 % -- no effect until dialled in, same "neutral by default" reasoning as Bass
    Cutoff/High Shelf above). The crossover frequency above which the delayed signal is
    added (default 300 Hz, keeping bass content out of the effect) is not a parameter;
    it's read from the global settings file, see below. See
    [phase5_comb.md](../docs/algorithms/phase5_comb.md) for the algorithm and its
    verification.
  - *Allpass Decorrelation*: the mid signal is filtered through two different allpass
    cascades and blended into each channel, decorrelating L/R without altering either
    channel's own magnitude spectrum -- also creates real width from dual-mono input,
    like Complementary Comb. The left knob sets **Amount** (0-100 %, defaults to 0 % --
    an exact bypass), the right knob sets **Spread** (0-100 %, how far apart the two
    cascades' frequencies sit). **Not mono-safe**: unlike every other algorithm here,
    the mono sum (L+R) is coloured once Amount is above 0 -- StereoWidenerGUI shows a
    warning below the algorithm selector when this algorithm is active; check your mix
    in mono (Utilities -> Monitor -> Mono Check) before committing to a setting. See
    [phase5_allpass.md](../docs/algorithms/phase5_allpass.md) for the algorithm and its
    verification.

  Every parameter's default -- and so what double-clicking its knob resets it to -- is
  chosen to be as close to neutral/pass-through processing as possible for its
  algorithm (Width 100 %, Rotation/Balance 0, Bass Cutoff/High Shelf/Comb Gain/Allpass
  Amount Off/0 %, etc.). To start a session from your own preferred settings instead, save an `init`
  preset (see `tools/PresetHandler.h`) rather than relying on the plugin to remember
  its last state.

  Note: the two knobs flanking Width are shared widgets -- which parameter they
  actually control, their range, and their unit all change with the selected
  algorithm (rebound automatically on switch); they are not per-algorithm knobs.
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

See [../docs/algorithms/phase4_settings.md](../docs/algorithms/phase4_settings.md) (and
its correction note about `lastUsedState`, removed later -- see below).
`~/.config/StereoWidener/settings.json` (created automatically on first run) stores
user-wide preferences that aren't part of a DAW project's own saved state -- a project's
saved parameter values and GUI size always take priority over these once they exist:

```json
{
  "highShelfGainDb": 3.0,
  "guiScaleFactor": 1.0,
  "meterIntegrationTimeS": 0.3,
  "meterPeakHoldTimeS": 1.5,
  "meterPeakDecayDbPerS": 20.0,
  "combCrossoverHz": 300.0
}
```

Edit and save while the plugin/DAW is closed; it's read once when a plugin instance is
created, not watched live. Note this file does **not** store parameter values or their
defaults (it did briefly, as `lastUsedState`; removed, see
[phase4_settings.md](../docs/algorithms/phase4_settings.md)) -- every parameter's
default is a fixed, compiled-in, neutral value, and restoring your own preferred
settings across sessions is what an `init` preset is for instead.

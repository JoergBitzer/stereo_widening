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
fourth algorithm (allpass decorrelation) and the "not mono-safe" badge it introduced,
[../docs/algorithms/phase5_gui_compaction.md](../docs/algorithms/phase5_gui_compaction.md)
for the current three-column GUI layout, and
[../docs/algorithms/phase5_multiband.md](../docs/algorithms/phase5_multiband.md) for
the fifth algorithm (multiband width), its allpass phase-compensation, and the
dynamic-window-resize/multi-param-grid mechanism it introduced.

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
  - *Multiband Width*: splits the signal into 4 bands (3 crossovers) and applies an
    independent M/S width to each of the upper 3 bands; the lowest band's width is
    always 0 ("bass mono comes built in", not a parameter). The only algorithm here
    that doesn't fit "2 + Width" (6 parameters), so it gets its own grid of 6 knobs
    below the usual layout instead of the two flanking knobs, and the plugin window
    grows while it's selected: **Low-Mid / Mid-High / High-Air** (the 3 crossover
    frequencies, 40-400/200-4000/1000-18000 Hz, each knob clamped against its
    neighbours so they can't be dragged past each other) and **Low-Mid / Mid-High /
    High** (the 3 band widths, 0-200 %, default 100 % -- neutral, same as the shared
    Width knob). See [phase5_multiband.md](../docs/algorithms/phase5_multiband.md) for
    the algorithm (including the allpass phase-compensation needed for a flat
    reconstruction) and its verification.
  - *Early Reflections (Room Widening)*: a handful of short, quiet, delayed copies of
    the mid signal are added to each channel, using a DIFFERENT set of delay times for
    L than for R -- mimics the early reflections a real room adds before its late
    reverb tail arrives (apparent source width), and also creates real width from
    dual-mono input, like Complementary Comb and Allpass Decorrelation. The left knob
    sets **Amount** (0-100 %, defaults to 0 % -- an exact bypass), the right knob sets
    **Room Size** (0-100 %, how spread out the reflections are: small room = tight
    cluster, large room = spread further out). The pre-delay before the first
    reflection (default 5 ms) and the number of reflections (5) are not parameters;
    the former is read from the global settings file, the latter is fixed. **Not
    mono-safe**, same reasoning and warning badge as Allpass Decorrelation. See
    [phase5_early_reflections.md](../docs/algorithms/phase5_early_reflections.md) for
    the algorithm (including a `juce::dsp::DelayLine` read-cursor bug found and fixed
    during cross-checking) and its verification.
  - *Chorus Doubler (Dimension D)*: the mid signal is fed through two independently
    LFO-modulated delay lines, one per channel, held a quarter-cycle apart -- the
    classic modulated-delay chorus/"Dimension D" effect, also creating real width
    from mono input like Complementary Comb, Allpass Decorrelation and Early
    Reflections. The left knob sets **Amount** (0-100 %, defaults to 0 % -- an exact
    bypass), the right knob sets **Depth** (0-100 %, how much the delay time swings
    around its centre). The LFO rate is not a parameter; it is read from the global
    settings file, kept deliberately slow so the effect stays lush rather than turning
    into an obvious vibrato/warble. **Not mono-safe** -- more so than Allpass
    Decorrelation or Early Reflections, since the delay difference between L and R is
    itself constantly sweeping ("flanging"), not fixed. See
    [phase5_chorus.md](../docs/algorithms/phase5_chorus.md) for the algorithm and its
    verification.

  Every parameter's default -- and so what double-clicking its knob resets it to -- is
  chosen to be as close to neutral/pass-through processing as possible for its
  algorithm (Width 100 %, Rotation/Balance 0, Bass Cutoff/High Shelf/Comb Gain/Allpass
  Amount Off/0 %, etc.). To start a session from your own preferred settings instead, save an `init`
  preset (see `tools/PresetHandler.h`) rather than relying on the plugin to remember
  its last state.

  Note: the two knobs flank Width in one row, below the algorithm selector, inside a
  boxed "parameter" card (left two-thirds of the window -- see
  [phase6_gui_thirds.md](../docs/algorithms/phase6_gui_thirds.md) for the current
  layout) -- and are shared widgets: which parameter they actually control, their
  range, and their unit all change with the selected algorithm (rebound automatically
  on switch); they are not per-algorithm knobs.
- **Utilities** (its own boxed card, the right third of the window, since every one of
  them acts on the final output signal regardless of which algorithm is selected --
  see [phase4_settings.md](../docs/algorithms/phase4_settings.md) for what each one
  does and [phase6_gui_thirds.md](../docs/algorithms/phase6_gui_thirds.md) for the
  layout):
  - **Rotation**: -45..+45 degrees, the stereo image's rotation in the L/R plane.
  - **Balance**: -100..+100 %. Positive attenuates L (image moves right), negative
    attenuates R (image moves left).
  - **Gain**: -24..+6 dB, 0.5 dB steps, defaults to 0 dB -- a final output trim,
    applied last (after Monitor mode, so it also scales whatever is currently being
    auditioned).
  - **Flip** (caption above the three toggle buttons): **Swap / Inv L / Inv R** --
    swap the two channels, or invert either channel's polarity.
  - **Monitor**: Normal / Mono Check (listen to L+R) / Solo Side (listen to S) --
    auditioning modes that override the final output; always switch back to Normal
    before bouncing/exporting.

## Day/night theme

The small icon button in the top-right corner (sun/moon) switches between two GUI
colour themes -- Day (the "Jade" house style: white background, red knob handles) and
Night (this plugin's original dark look, recoloured to match: grey knobs, same red
handles). Purely cosmetic: it does not affect the metering/goniometer displays, or any
parameter value. The choice is remembered (global settings file, below), default
Night. See [phase6_daynight_theme.md](../docs/algorithms/phase6_daynight_theme.md).

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

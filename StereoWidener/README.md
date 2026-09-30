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

- **Algorithm**: which stereo-widening algorithm processes the signal, selected from the
  list below the meters. Its controls appear in the card below the selector (the
  algorithm's "playground", the same size for every algorithm, so the window never
  changes size on a switch). Every algorithm has its own **Width** (0-200 %: 0
  collapses the side signal to mono, 100 is unity, 200 doubles the side signal) and
  its own settings, so switching algorithms and back keeps each one's values. Switching is crossfaded (equal-power, 30 ms) so it never
  clicks. Click the "?" button next to the selector for an explanation of the active
  algorithm, with a citation to a written source.
  - *M/S Width (Broadband)*: plain M/S width control across the whole spectrum. Width
    is its only control -- a single knob, to show how simple the basic technique is.
  - *M/S Width (Filtered / Bass Mono)*: the same control, but the side signal is
    high-pass filtered first, so bass content is forced mono and only the highs get
    widened, then high-shelved to restore some "air". Controls: **Width**, **Bass
    Cutoff** (40-500 Hz, turn below 40 Hz for "Off" -- bypasses the high-pass entirely,
    and is the default: double-clicking the knob resets to Off, not to some fixed
    cutoff), **High Shelf** frequency (1000-16000 Hz, turn above 16 kHz for "Off" --
    bypasses the shelf entirely, and is likewise the default) and **Shelf Gain** (-6 to
    +6 dB, default +3 dB; until v0.1.18 a fixed value in the settings file). The graph
    above the knobs shows the gain the side signal gets at each frequency; drag its two
    points to set the cutoff and shelf frequencies, drag the shelf point up/down for
    its gain, double-click a point to reset it.
  - *Complementary Comb (Pseudo-Stereo)*: a delayed, gained copy of the mid signal is
    added to the side signal (Lauridsen/Schroeder pseudo-stereo) -- unlike the two M/S
    algorithms above, this one creates real width even from dual-mono input. Controls:
    **Width**, **Delay** (5-20 ms), **Gain** (0-100 %, defaults to 0 % -- no effect
    until dialled in, same "neutral by default" reasoning as Bass Cutoff/High Shelf
    above) and **Crossover** (50-2000 Hz, default 300 Hz: the delayed signal is only
    added above it, keeping bass content out of the effect; until v0.1.22 a fixed value
    in the settings file). The graph shows what happens to a centred (mono) input on a
    linear 0-2 kHz axis: L gets peaks where R gets notches, 1/Delay apart; drag the
    vertical line to move the crossover. See
    [phase5_comb.md](../docs/algorithms/phase5_comb.md) for the algorithm and its
    verification, [phase6_playground_comb.md](../docs/algorithms/phase6_playground_comb.md)
    for the display.
  - *Allpass Decorrelation*: the mid signal is filtered through two different allpass
    cascades and blended into each channel. Each allpass copy has the dry signal's
    exact magnitude spectrum but a different phase; blending it with the dry signal
    decorrelates L/R -- and also colours each channel and the mono sum. It creates
    real width from dual-mono input, like Complementary Comb. Controls: **Width**,
    **Amount** (0-100 %, defaults to 0 % -- an exact bypass) and **Spread** (0-100 %,
    how far apart the two cascades' frequencies sit). The graph shows what happens to
    a centred (mono) input -- the gain of L, R and the mono sum, with the allpass
    stages' frequencies marked; drag left/right for Spread, up/down for Amount.
    **Not mono-safe** (like Early Reflections and Chorus Doubler): the mono sum (L+R)
    is coloured once Amount is above 0 -- StereoWidenerGUI shows a warning below the
    playground when such an algorithm is active; check your mix in mono (Utilities ->
    Monitor -> Mono Check) before committing to a setting. See
    [phase5_allpass.md](../docs/algorithms/phase5_allpass.md) for the algorithm and its
    verification, [phase6_playground_allpass.md](../docs/algorithms/phase6_playground_allpass.md)
    for the display.
  - *Multiband Width*: splits the signal into 4 bands (3 crossovers) and applies an
    independent M/S width to each of the upper 3 bands; the lowest band is always mono
    ("bass mono comes built in", not a parameter). The display shows the 4 bands on a
    frequency axis, each band's bar height being its width (dashed line = 100 %): drag
    a band up/down for its width, drag the lines between bands to move the crossovers
    (a crossover stops at its neighbours), double-click to reset. Below it, knobs for
    exact values in two staggered rows, ordered like the frequency axis: Split 1-3 on
    top (the crossovers, each anywhere from 40 Hz to 18 kHz as long as it stays between
    its neighbours), and Width 2-4 below, each between the two splits that bound its
    band (the band widths, 0-200 %, default 100 % -- neutral). There is no
    overall Width (removed in v0.1.21: it scaled all bands on top of their own widths).
    See [phase5_multiband.md](../docs/algorithms/phase5_multiband.md) for the algorithm
    (including the allpass phase-compensation needed for a flat reconstruction) and
    its verification, [phase6_playground_multiband.md](../docs/algorithms/phase6_playground_multiband.md)
    for the display.
  - *Early Reflections (Room Widening)*: a handful of short, quiet, delayed copies of
    the mid signal are added to each channel, using a DIFFERENT set of delay times for
    L than for R -- mimics the early reflections a real room adds before its late
    reverb tail arrives (apparent source width), and also creates real width from
    dual-mono input, like Complementary Comb and Allpass Decorrelation. Controls:
    **Width**, **Amount** (0-100 %, defaults to 0 % -- an exact bypass), **Room Size**
    (0-100 %, how spread out the reflections are: small room = tight cluster, large
    room = spread further out) and **Pre-delay** (0-20 ms, default 5 ms, the time
    before the first reflection; until v0.1.23 a fixed value in the settings file).
    The number of reflections (5 per channel) is fixed. The display is an echogram:
    the direct sound at 0 ms, L's reflections above the time axis, R's below, bar
    height = level in dB; drag the Pre-delay and room-end lines sideways, drag up/down
    elsewhere for Amount. **Not mono-safe**, same reasoning and warning badge as
    Allpass Decorrelation. See
    [phase5_early_reflections.md](../docs/algorithms/phase5_early_reflections.md) for
    the algorithm (including a `juce::dsp::DelayLine` read-cursor bug found and fixed
    during cross-checking) and its verification,
    [phase6_playground_early_reflections.md](../docs/algorithms/phase6_playground_early_reflections.md)
    for the display.
  - *Chorus Doubler*: the mid signal is fed through two independently
    LFO-modulated delay lines, one per channel, held a quarter-cycle apart -- the
    classic modulated-delay stereo chorus effect, also creating real width
    from mono input like Complementary Comb, Allpass Decorrelation and Early
    Reflections. Controls: **Width**, **Amount** (0-100 %, defaults to 0 % -- an
    exact bypass), **Depth** (0-100 %, how much the delay time swings around its
    centre) and **Rate** (0.05-2 Hz, default 0.3 Hz; capped at 2 Hz so the effect stays
    lush rather than turning into an obvious vibrato/warble; until v0.1.24 a fixed
    value in the settings file). The display shows both channels' delay times over
    4 s; drag up/down for Depth, left/right for Rate. **Not mono-safe** -- more so than Allpass
    Decorrelation or Early Reflections, since the delay difference between L and R is
    itself constantly sweeping ("flanging"), not fixed. See
    [phase5_chorus.md](../docs/algorithms/phase5_chorus.md) for the algorithm and its
    verification, [phase6_playground_chorus.md](../docs/algorithms/phase6_playground_chorus.md)
    for the display.

  Factory presets (v0.1.31): 20 starting points, mostly per instrument, each using the
  algorithm that suits it (e.g. "Synth Lead - Pseudo Stereo" = Complementary Comb,
  "Guitar Clean - Chorus Wide" = Chorus Doubler, "Master - Gentle Widen" = M/S
  Filtered), plus a neutral "Init". Some set the output Gain to keep the loudness
  unchanged. Generated by `python/make_factory_presets.py`, embedded in the plugin and
  copied into the user preset folder when missing (never over a preset you saved) -- see
  [phase6_factory_presets.md](../docs/algorithms/phase6_factory_presets.md).

  Presets saved before v0.1.16 load without their Width setting (each algorithm now has
  its own Width parameter; the old shared one is gone).

  Every parameter's default -- and so what double-clicking its knob resets it to -- is
  chosen to be as close to neutral/pass-through processing as possible for its
  algorithm (Width 100 %, Rotation/Balance 0, Bass Cutoff/High Shelf/Comb Gain/Allpass
  Amount Off/0 %, etc.). To start a session from your own preferred settings instead, save an `init`
  preset (see `tools/PresetHandler.h`) rather than relying on the plugin to remember
  its last state.

  Each algorithm's controls are its own knobs, bound to its own parameters -- see
  [phase6_playground_step0.md](../docs/algorithms/phase6_playground_step0.md) and
  [plan_changeGUI.md](../plan_changeGUI.md), which gives every algorithm a dedicated
  playground with graphics suited to it, one algorithm at a time.
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
`settings.json` in the preset folder (e.g. `~/.config/Jade_Hochschule/StereoWidener/` on
Linux; until the first 1.0.4 build `~/.config/StereoWidener/`, moved automatically;
created on first run) stores
user-wide preferences that aren't part of a DAW project's own saved state -- a project's
saved parameter values and GUI size always take priority over these once they exist.
Since v0.1.25 it holds no processing settings at all any more (the shelf gain, comb
crossover, early-reflection pre-delay and chorus rate it used to hold are all
parameters now), only the GUI size, the theme (`useDayTheme`) and the meter ballistics:

```json
{
  "guiScaleFactor": 1.0,
  "useDayTheme": false,
  "meterIntegrationTimeS": 0.3,
  "meterPeakHoldTimeS": 1.5,
  "meterPeakDecayDbPerS": 20.0
}
```

Edit and save while the plugin/DAW is closed; it's read once when a plugin instance is
created, not watched live. Note this file does **not** store parameter values or their
defaults (it did briefly, as `lastUsedState`; removed, see
[phase4_settings.md](../docs/algorithms/phase4_settings.md)) -- every parameter's
default is a fixed, compiled-in, neutral value, and restoring your own preferred
settings across sessions is what an `init` preset is for instead.

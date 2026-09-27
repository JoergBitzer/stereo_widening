# Playground 3: Multiband Width, the band-split element (v0.1.21)

Step 3 of [plan_changeGUI.md](../../plan_changeGUI.md), section 4.1: a display of the
four bands that is also their control, plus compact knobs for exact values. Multiband
Width loses its overall Width.

![Defaults; varied splits and widths; Day theme with band 2 at 0 % and band 3 at 200 %; Filtered, for comparison after the shared-axis refactor](img/playground_multiband.png)

## The playground

**Band-split display** (`playgrounds/BandSplitView.h/.cpp`):

- The four bands on a log frequency axis (20 Hz-20 kHz), numbered 1-4 along the top,
  separated by the three crossover lines.
- Band 1 is always mono (the algorithm's built-in "bass mono", decided in
  plan_changeGUI.md 7.2): shown as "Mono" with a flat line at the bottom; it can't be
  changed.
- Bands 2-4: each is a bar whose height is its width, 0-200 %, with a dashed line at
  100 % (unchanged) and the value above the bar. Drag a band up or down to change its
  width.
- Crossover lines with a grip at the top: drag sideways. A crossover stops at its
  neighbours (at least 5 % apart, `MultibandWidth::kMinCrossoverRatio`, the same rule
  the DSP applies to automated values); hovering or dragging shows its frequency.
- Double-click resets what's under the mouse. Drags are host gestures
  (`juce::ParameterAttachment`), so automation recording and undo work.

**Knobs** below, in two groups: Split 1-3 (the crossovers; split k separates band k
and k+1) and Band 2-4 (the widths), for exact values. The crossover knobs also stop at
their neighbours.

## No overall Width any more

Until now a master Width scaled bands 2-4 on top of their own widths, so how wide a
band ended up depended on two knobs -- "counterintuitive" per the plan review. It is
removed (parameter `multibandMasterWidth`); each band's own width is the only control.
At its default (100 %) it multiplied by exactly 1, so default processing is unchanged:
a render with the old master at 100 % is byte-identical before and after. Presets
that set it to anything else lose that setting.

The host-visible parameter names are now unambiguous ("Crossover 1-3", "Band 2-4
Width" instead of "Low-Mid"/"Mid-High" appearing twice); the parameter IDs are
unchanged. The Python reference (`python/algorithms/multiband_width.py`) keeps its
`width` argument; its default of 1.0 matches the plugin.

## Other changes

- **Value boxes size their text to the box** (`StereoWidenerLookAndFeel::
  createSliderTextBox()`): JUCE always used a 15 px font, so "1500 Hz" was cut off in
  the compact knobs' 44 px boxes. Now 0.85 x the box height, at most 15 px. This
  affects every knob: the large Width knob's value is as before; smaller boxes get a
  smaller, no longer squashed font.
- **Shared pieces**: `LogFrequencyAxis.h` (pixel-frequency mapping and grid) is now
  used by both `FrequencyGraph` and `BandSplitView`; `ParameterValues.h` holds the
  clamp-before-set helpers both need.
- **Removed**: `KnobsPlayground`'s grid layout and the interim Multiband knob grid --
  no algorithm with more than three parameters uses `KnobsPlayground` any more. The
  grid constants became `g_smallKnob*` (used by Filtered); `g_compactKnob*` is new.

## Verification

- **Dragging** (synthetic mouse events on the display, throwaway tool): splits move,
  a split stops at its neighbour (315 Hz = 300 Hz x 1.05), band widths follow vertical
  drags and clamp at 0 %, band 1 can't be changed, double-click resets:
  [drag_test.txt](../../python/results/playground_multiband/drag_test.txt).
- **Sound unchanged at defaults**, see above.
- **GUI**: offline renders in several states, both themes; Filtered re-checked after
  the axis refactor.
- **pluginval --strictness-level 10**: 4/4 SUCCESS, zero JUCE assertions (this build
  also covers the simplified Broadband playground, v0.1.20):
  [console.txt](../../python/results/playground_multiband/console.txt).

## Files

- `StereoWidener/playgrounds/BandSplitView.h/.cpp`, `MultibandPlayground.h/.cpp`,
  `LogFrequencyAxis.h`, `ParameterValues.h` (new).
- `StereoWidener/playgrounds/FrequencyGraph.h/.cpp`, `FilteredPlayground.cpp`: use the
  shared axis/helpers.
- `StereoWidener/algorithms/MultibandWidth.h/.cpp`: no master Width, clearer names,
  `kMinCrossoverRatio`, help text explains the display.
- `StereoWidener/AlgorithmPlayground.h/.cpp`: knob label override; grid layout removed;
  factory creates `MultibandPlayground`.
- `StereoWidener/PluginLookAndFeel.h/.cpp`: value-box font.
- `StereoWidener/PluginSettings.h`: knob size constants.
- `tools/widener_render/main.cpp`: width argument ignored for multiband.
- `StereoWidener/README.md`, `StereoWidener/CMakeLists.txt` (new sources; version
  0.1.20 -> 0.1.21).

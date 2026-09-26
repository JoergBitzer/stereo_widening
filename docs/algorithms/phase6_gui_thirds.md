# Phase 6 GUI polish -- three-column redesign ("divide the parameter part into thirds")

A GUI-only redesign (Phase 6, "v1 release"), following on from
[phase5_gui_compaction.md](phase5_gui_compaction.md) and
[phase6_daynight_theme.md](phase6_daynight_theme.md), per four explicit requests:

1. The three meter-row displays (Input, Goniometer, Output) should all be the same
   height -- the goniometer was a little shorter.
2. The area below the meter row should be divided into thirds: the right third is
   Utilities only, emphasised with a small surrounding "card" (slightly brighter
   background).
3. The algorithm selector should move to the middle of the left two-thirds, directly
   below the input meter + goniometer.
4. The "?" help button should move to the right of the algorithm selector, exactly as
   tall as it. All of the active algorithm's own parameters should sit below the
   selector, in their own card matching the Utilities panel's treatment; the two cards
   are separated the same way the three meter-row panels are.

## The goniometer height bug (request 1)

`StereoWidenerGUI::resized()` set the level meters' bounds with
`.reduced(g_levelMeterPadding)` (2 px on all four sides) but the goniometer's with
`.reduced(g_goniometerPadding)` (4 px on all four sides) -- different padding values,
so the goniometer came out 4 px shorter (252 px vs. the level meters' 256 px) purely
by accident, not by design. Fixed with `Rectangle::reduced(deltaX, deltaY)`'s two-axis
overload: the goniometer keeps its own (larger) *horizontal* padding for visual
breathing room from the level meters either side, but now uses the level meters' own
`g_levelMeterPadding` vertically, so all three come out exactly the same height.
Verified by direct pixel measurement (not just eyeballing a screenshot): all three
panels measure 254 px tall, same top and bottom row, at the default window size.

## The shared left-two-thirds/right-third grid (requests 2-4)

The key design decision: rather than dividing the parameter area into thirds
*independently* of the meter row above it, `g_levelMeterWidth` (the input/output level
meters' width) now does double duty as the single shared right-column width for
*every* row in the GUI -- the meter row, the algorithm-selector row, and the two boxed
panels. This is what makes request 3 ("directly below the input and goniometer
display") exact rather than approximate: the algorithm-selector row's left block is
`getWidth() - levelMeterWidth`, which is *by construction* the combined width of the
input meter and goniometer above it, not a separately-computed "roughly two-thirds".

`g_levelMeterWidth` changed from 115 to 165 px (at the default 480 px window width,
`480/3 = 160`; 165 gives the Utilities panel's content a few pixels of headroom
inside its own padding -- see below) -- a deliberate widening of the input/output
level meters themselves, not just a description of the space below them. This makes
the meter row itself closer to three true equal thirds too, not only the area below it.

Below the meter row:
- **Algorithm selector row**, unboxed, centred within the left two-thirds. The "?"
  help button moved to the right of the combo box (previously to its left) and is now
  exactly `g_algorithmRowHeight` square -- both the button and the combo box derive
  their height from the same constant, so there is nothing to keep in sync by hand
  (previously `g_helpButtonSize` was a separate constant that happened to differ from
  `g_algorithmRowHeight`).
- **Two boxed "card" panels**, side by side, drawn with a slightly brighter background
  in `StereoWidenerGUI::paint()` (`background.brighter(0.08f)`, the same relative
  brightening in both day/night themes) at bounds computed in `resized()` and stored
  in `m_paramPanelBounds`/`m_utilPanelBounds`:
  - **Left (parameters)**: aux-left / Width / aux-right in one row (previously the aux
    knobs were their own stacked column to the left of a separate "Width + algorithm
    selector + badge" middle column -- the algorithm selector's move out freed this
    space up for a single combined row), then the "not mono-safe" badge, then (only
    for Multiband Width) its dedicated 6-knob grid, growing this panel specifically.
  - **Right (Utilities)**: unchanged internally (Rotation/Balance, Flip buttons,
    Monitor selector) -- now visibly boxed to match the parameter panel's treatment,
    where it previously had no background of its own.

  Both panels share the *same* height in the common case (like the three meter-row
  panels, a matched pair of cards) -- the parameter panel is the only one that grows
  taller on its own, and only when the multiband grid is active; the Utilities panel
  stays at the shared base height regardless. Separated the same way the meter-row
  panels are: contiguous cells (no explicit gap removed between them), each with its
  own internal `g_panelPadding` inset, so the *visible* gap between the two card
  backgrounds is padding-based, not a differently-styled gap.

![Default (M/S Width Broadband): equal-height meter panels, algorithm selector centred below input+goniometer with the "?" button to its right at matching height, aux-left/Width/aux-right in one row inside the parameter card, Utilities in its own card to the right](img/phase6_thirds_broadband.png)

![Multiband Width selected: the parameter panel grows to fit its dedicated 6-knob grid below the aux/Width row; the Utilities panel stays at its normal (shorter) height, both still top-anchored to the same row](img/phase6_thirds_multiband.png)

![Chorus Doubler selected (not mono-safe): Amount/Depth flank Width correctly, the warning badge sits inside the parameter card below the knob row](img/phase6_thirds_chorus.png)

## Verification

**GUI offline render** (throwaway `WidenerGuiSnapshot` console tool, rendering the
default algorithm, Multiband Width, and a "not mono-safe" algorithm (Chorus Doubler);
built and fully removed afterwards per the project's convention): confirmed all four
requests above, plus no regression in the day/night theme (both card panels take the
theme's own background-brightening treatment automatically, since they read the
ambient `ResizableWindow::backgroundColourId` at paint time) and in the mono-safe
badge/multiband-grid mechanisms already covered by prior verification passes.

**Direct pixel measurement** (not just a screenshot glance): the three meter-row
panels measure exactly 254 px tall, same top and bottom, at the default window size --
confirms request 1 precisely, not approximately.

**pluginval --strictness-level 10**: SUCCESS, three consecutive clean runs.

## Files

- `StereoWidener/PluginSettings.h`: `g_levelMeterWidth` widened (115 -> 165, now also
  the shared right-column width for every row); `g_panelPadding`/`g_panelCornerSize`
  (new); `g_paramKnobGap` replaces `g_auxKnobVGap` (aux knobs no longer stacked);
  `g_helpButtonSize` removed (the help button's size now derives from
  `g_algorithmRowHeight` directly); `g_utilColumnWidth` removed (the Utilities panel's
  content width now derives from its own card bounds).
- `StereoWidener/StereoWidener.h`: `m_paramPanelBounds`/`m_utilPanelBounds` (new).
- `StereoWidener/StereoWidener.cpp`: `paint()`, `resized()`,
  `getRequiredContentHeight()` all rewritten for the new layout.
- `StereoWidener/CMakeLists.txt`: version bumped 0.1.11 -> 0.1.12.

## Follow-up (v0.1.13): quarters and a more prominent divider

Two more explicit requests:

1. **Level meters "a little too big"**: the meter row now uses *quarters* (input 1/4,
   goniometer 2/4, output 1/4) instead of the thirds it shared with the panel row
   below. Deliberately decoupled into its own constant, `g_levelMeterWidth` (now 120,
   `= g_minGuiSize_x/4`), separate from `g_rightBlockWidth` (165, the panel row's own
   thirds-based width) -- the two rows now use different fractions on purpose.
2. **More prominent divider** between the parameter and Utilities cards: an explicit
   `g_panelDividerWidth` (6px) gap, drawn in the plain (unbrightened) background
   colour, sits between the two panels' rounded-rect backgrounds -- previously they
   were flush against each other with no visible line at all.
   - While verifying this in Day mode, found the card background itself was nearly
     invisible: `background.brighter(0.08f)` on Day's ~0.95-brightness background has
     almost no 8-bit headroom left (242 vs 243). Fixed with a brightness-adaptive
     direction in `paint()`: darken when the background is already light, brighten
     when it's dark (`getPerceivedBrightness() > 0.5f ? darker(0.06f) : brighter(0.08f)`).

### Files (follow-up)

- `StereoWidener/PluginSettings.h`: `g_levelMeterWidth` decoupled from
  `g_rightBlockWidth` (120 vs 165); `g_panelDividerWidth` (new).
- `StereoWidener/StereoWidener.cpp`: `resized()` reserves the divider gap between the
  two panels; `paint()`'s panel colour now picks a brightness-adaptive direction.
- `StereoWidener/CMakeLists.txt`: version bumped 0.1.12 -> 0.1.13.

## Crash fix (v0.1.14): out-of-range log-frequency values crashing pluginval

Found while verifying the v0.1.13 work above, not requested, but a real,
pre-existing bug: `pluginval --strictness-level 10` intermittently crashed (exit 139,
"Segmentation fault") during "Editor Automation", roughly 1 run in 6-8, and every run
(even successful ones) printed dozens of "JUCE Assertion failure in
juce_NormalisableRange.h:265" warnings -- present since the log-frequency knobs were
introduced (Phase 5), just never chased down until it happened to be fatal under a
debugger during this round's testing.

- Root cause: `makeLogFrequencyParameter()`/`makeLogFrequencyParameterWithOff()`
  (StereoWidener.cpp) gave their `NormalisableRange<float>` a custom
  `snapToLegalValue` lambda that only rounded to the nearest whole Hz
  (`std::round(value)`) and never clamped into `[rangeStart, rangeEnd]`, unlike
  JUCE's own default implementation. Any out-of-range raw value reaching that lambda
  (e.g. a `Slider`'s stale pre-attachment value, such as 10, being formatted against a
  `[40, 400]` Hz parameter) sailed through unclamped into the log `convertTo0To1`
  formula (`log(value/rangeStart)/log(rangeEnd/rangeStart)`), producing a result
  outside `[0, 1]` and tripping `NormalisableRange::clampTo0To1`'s assertion --
  benign as a logged warning without a debugger attached, but a fatal, uncaught
  `SIGTRAP` under one (which is how pluginval's automation testing found it).
- Confirmed via `gdb`-loop reproduction (12/12 attempts crashed identically before
  the fix, at `clampTo0To1` called from `Slider::Pimpl::updateRange()`'s unconditional
  final `updateText()`, formatting the slider's still-stale raw value through the
  parameter's own `convertTo0to1` -- installed as the slider's `textFromValueFunction`
  by `SliderParameterAttachment`'s constructor, bypassing the slider's own
  (also-buggy) clamp entirely).
- Fix: both lambdas now clamp before rounding --
  `std::round(juce::jlimit(rangeStart, rangeEnd, value))`. Also pre-sync
  `bindAuxKnob()`/`bindMultiKnob()`'s knob to the parameter's real value before
  constructing the `SliderAttachment` (defensive; the clamp fix above is the actual
  root-cause fix). Verified with 15/15 clean `gdb`-loop reproduction attempts
  (previously 12/12 crashed) and multiple plain `pluginval` runs, all showing zero
  assertion warnings (previously 20-40 per run) and `SUCCESS`.
- A separate, unrelated, pre-existing crash was also observed during this
  investigation: an intermittent `SIGSEGV` inside JUCE's own
  `XEmbedComponent::Pimpl::handleX11Event` during editor teardown (X11/Linux
  windowing only, no application code in the backtrace at all). Left as-is -- this
  is a JUCE-internal Linux windowing race, out of scope for a plugin-side fix.

### Files (crash fix)

- `StereoWidener/StereoWidener.cpp`:
  `makeLogFrequencyParameter()`/`makeLogFrequencyParameterWithOff()`'s
  `snapToLegalValue` lambdas now clamp into range before rounding; `bindAuxKnob()`/
  `bindMultiKnob()` also pre-sync the knob's raw value from the parameter before
  constructing the `SliderAttachment`.
- `StereoWidener/CMakeLists.txt`: version bumped 0.1.13 -> 0.1.14.

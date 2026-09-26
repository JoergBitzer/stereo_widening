# GUI redesign: per-algorithm "playground" instead of one generic knob layout

Planning document only -- no code changes in this step. Written in response to:

> "I am not happy with changing size for one algorithm. I would like to have the
> lower left field as kind of working field / playground for each algorithm with its
> optimized gui elements (e.g. a special band split element). Think about how to do
> that and explain your next steps."

## 1. The problem with the current design

The lower-left card (`m_paramPanelBounds`, set up in `StereoWidenerGUI::resized()`,
`StereoWidener.cpp`) currently shows exactly one layout for every algorithm: Width
in the middle, flanked by up to two generic rotary "aux" knobs
(`m_auxLeftKnob`/`m_auxRightKnob`), each relabelled per algorithm via
`StereoAlgorithm::getAuxLeftInfo()`/`getAuxRightInfo()` (`algorithms/StereoAlgorithm.h`).
One algorithm, Multiband Width, doesn't fit that pattern at all -- it needs 6 values
(3 crossover frequencies + 3 band widths), so it instead gets a bolted-on 6-knob grid
below the row, driven by `getNumMultiParams()`/`getMultiParamInfo()`.

That grid is the concrete thing you're unhappy with: `getRequiredContentHeight()`
computes a *taller* panel only when the active algorithm's `getNumMultiParams() > 0`,
so **the whole plugin window resizes** when you switch to or from Multiband Width.
Every other algorithm is fine with the generic two-knob row, but it's also not
particularly *good* for them -- two identical grey rotary knobs is the same
presentation whether the parameter is a delay time (Complementary Comb), a filter
cutoff (M/S Width Filtered), a spread percentage (Allpass Decorrelation), or a room
size (Early Reflections). None of these get anything visually suited to what they
actually represent.

## 2. Design goals for the replacement

1. **Fixed size.** The lower-left area has one size, decided once, independent of
   which algorithm is active. Switching algorithms never resizes the window.
2. **Per-algorithm content.** Each algorithm can present whatever controls suit it
   best -- generic knobs for the simple cases, a dedicated custom widget (e.g. an
   interactive band-split diagram) for the ones that benefit from one.
3. **Low cost for the common case.** Five of the seven algorithms today are well
   served by "Width + 0/1/2 labelled knobs." That should stay a one-line
   declaration, not a hand-written widget per algorithm.
4. **No change to the DSP layer.** `algorithms/*.cpp` and `StereoAlgorithm.h` stay
   free of any GUI dependency -- important because `tools/widener_render` compiles
   those same `.cpp` files into a plain console app linked only against
   `juce_audio_basics`/`juce_dsp` (see its CMakeLists.txt), with no
   `juce_gui_basics` at all. Pulling `juce::Component` into `StereoAlgorithm` would
   break that tool. The playground concept below is entirely a GUI-layer
   (`StereoWidener/`, not `StereoWidener/algorithms/`) concept, reading the same
   `StereoAlgorithmParams`/parameter IDs the DSP side already exposes.
5. **No parameter/preset changes.** Same parameter IDs, ranges and defaults
   throughout -- this is a presentation change, existing presets keep loading.

## 3. Proposed architecture

### 3.1 `AlgorithmPlayground`: one small `juce::Component` per algorithm

A new abstract base class (new file, e.g. `StereoWidener/AlgorithmPlayground.h`):

```cpp
class AlgorithmPlayground : public juce::Component
{
public:
    // Bind to the given algorithm's parameters. Called once whenever the algorithm
    // selector changes (and once at startup for the initial algorithm). Replaces
    // today's updateAuxKnobsForActiveAlgorithm()'s per-algorithm special-casing.
    virtual void bindToAlgorithm(juce::AudioProcessorValueTreeState& apvts, int algorithmIndex) = 0;

    // Called from StereoWidenerGUI::resized() with the fixed playground rectangle
    // (m_paramPanelBounds' inner area, minus the Width knob's own space -- see 3.3).
    // Component::resized() already exists for this; no new method needed.
};
```

`StereoWidenerGUI` holds one `std::unique_ptr<AlgorithmPlayground>` per algorithm
(built once, up front, in the constructor -- not recreated on every switch), plus one
"current" pointer. On an algorithm change it hides the old one and shows/resizes the
new one (`setVisible`, `addAndMakeVisible`, `resized()`), exactly the same
show/hide idea `updateAuxKnobsForActiveAlgorithm()` already uses for the multiband
grid's 6 knobs today, just at the level of a whole component instead of individual
knobs.

### 3.2 `GenericKnobsPlayground`: the default, for the simple cases

One reusable playground class that reproduces today's aux-left/aux-right row, but
built generically from `getAuxLeftInfo()`/`getAuxRightInfo()` (0, 1, or 2 active
knobs, centred within the fixed area) -- this is what M/S Width (Broadband/Filtered),
Complementary Comb, Allpass Decorrelation, Early Reflections and Chorus Doubler all
use, unchanged from today's visuals and parameter bindings. Today's
`bindAuxKnob()`/aux-knob layout code in `StereoWidener.cpp` moves into this class
essentially as-is; no behaviour change for these six algorithms.

### 3.3 Where does Width live?

Width (`m_widthKnob`) is the one control every algorithm shares, and it isn't part of
any algorithm's own "extra" parameters -- I'd keep it as fixed chrome *above* the
playground area (same position as today), not inside the swappable component. Each
`AlgorithmPlayground` only owns the space below it. This keeps Width visually
consistent across every algorithm (per plan2.md's original framing of Width as
the one control that's always present) and means a custom playground like the band
split (3.4) only has to design around its own controls, not also reimplement Width.

Open question I'd like your call on: should the fixed playground height be sized to
today's *short* case (0/1/2 knobs -- what every algorithm except Multiband needs),
or to whatever the new band-split widget (3.4) turns out to need? If the band-split
widget needs more vertical room than today's short case, the window's *default*
height would grow slightly (every algorithm's playground area gets the same fixed
size, including the simple ones, which would then have some empty/centred space).
My default recommendation: size it to the band-split widget's actual needs once it's
designed (3.4 first, then fix the constant), since a bit of unused space around a
few centred knobs looks fine, but a cramped band-split diagram would not.

### 3.4 `BandSplitPlayground`: the "special band split element" for Multiband Width

This is the genuinely new piece of GUI work, replacing Multiband Width's 6-knob grid.
Concept (subject to iteration once I start prototyping):

- A horizontal frequency axis, log-scaled (matching `makeLogFrequencyParameter()`'s
  own mapping, so the visual position of a frequency matches how the plugin already
  thinks about it), spanning the three crossover parameters' combined range.
- Confirmed against `algorithms/MultibandWidth.cpp`: 3 crossovers
  (`g_paramMultibandFreq1`/`Freq2`/`Freq3`, labelled "Low-Mid"/"Mid-High"/"High-Air")
  split the signal into **4 bands**. Only 3 dedicated per-band width parameters exist
  (`g_paramMultibandWidth2`/`Width3`/`Width4`, labelled "Low-Mid"/"Mid-High"/"High") --
  the lowest band reuses the shared Width knob rather than having a 4th of its own.
  So the widget needs 3 draggable crossover handles and 3 band-width controls (plus
  the always-present Width knob covering the 4th/lowest band, per 3.3).
- Each of the 3 upper bands gets its own small width control -- most likely a short
  vertical slider or a small rotary knob positioned under/over its band's region on
  the axis, bound to that band's existing width parameter.
- Dragging a crossover handle updates that frequency parameter directly and
  continuously (`setValueNotifyingHost`), with a small numeric readout for precision
  (a text box, or a tooltip while dragging) -- the generic rotary knobs' text boxes
  already give exact values today; the new widget needs to keep that precision, not
  just the visual.
- Colour-code the bands so the split is legible at a glance (reusing the theme's
  existing palette, not inventing new colours -- see `PluginLookAndFeel.h`).

This is new interactive-mouse-input code (drag handling, hit-testing, snapping),
which is more design and testing effort than anything else in this change, and is
why it's its own phase below rather than bundled with the mechanical refactor.

## 4. Implementation phases (each its own branch + commit, per the current workflow)

**Phase A -- introduce the abstraction, no visual change.**
Add `AlgorithmPlayground` + `GenericKnobsPlayground`, move today's aux-knob binding
logic into it, and use it for all seven algorithms (Multiband Width still gets its
6-knob grid, just now hosted inside a playground instance instead of inline in
`resized()`). Verify via the offline GUI snapshot tool that every algorithm renders
pixel-identical to before. This alone does **not** yet fix the resizing complaint for
Multiband Width -- it's a pure refactor, kept as its own reviewable step rather than
mixed with the behaviour change in Phase B.

**Phase B -- fix the window-resize problem.**
Give the playground area a single fixed size (`getRequiredContentHeight()` stops
depending on `getNumMultiParams()`), and adapt Multiband Width's grid to fit that
fixed area (likely smaller knobs, still the plain 6-knob grid at this point -- not
the band-split widget yet). This is the smallest change that satisfies "don't change
size for one algorithm," decoupled from the larger band-split design/build effort.

**Phase C -- the band-split widget.**
Design (mock up a few visual options, likely as static GUI-snapshot renders first,
since the interaction can't be screenshot-tested the same way pluginval's automated
fuzzing can't meaningfully drive custom mouse-drag widgets) and implement
`BandSplitPlayground`, replacing Multiband Width's 6-knob grid from Phase B.

**Phase D (optional, not required for this request) -- revisit other algorithms.**
Once the mechanism exists, consider whether Complementary Comb (a small delay-line
diagram) or Early Reflections (a decaying-taps diagram) would similarly benefit from
a bespoke playground instead of generic knobs. Explicitly deferred; listed here so
it isn't forgotten, not scheduled.

## 5. Verification plan (per algorithm, per phase)

- Offline `WidenerGuiSnapshot` throwaway tool (built, used, fully removed each time,
  per project convention) across all seven algorithms, to catch layout regressions
  without needing a real host.
- `pluginval --strictness-level 10`, several consecutive clean runs, per the
  project's established bar.
- Phase C additionally needs manual interaction testing in the real Standalone
  build (drag the crossover handles, confirm the bound parameters and their text
  readouts update correctly, confirm undo/automation still works through the normal
  `AudioProcessorValueTreeState` attachment mechanism) -- automated tooling doesn't
  exercise custom mouse-drag code the way it does a `juce::Slider`.

## 6. Things I want your input on before starting Phase A

1. Confirm Width should stay fixed above the playground (3.3), not become part of
   each algorithm's own space.
2. Confirm the phase order above (mechanical refactor, then fix the resize, then
   build the band-split widget) rather than trying to do it all in one pass.
3. Whether the fixed playground size should be decided now (sized to today's short
   case, accepting that the band-split widget will have to fit within it) or after
   Phase C's design is clearer (my recommendation, see 3.3).

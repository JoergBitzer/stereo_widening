# GUI redesign: a per-algorithm "playground" (second planning phase)

Planning document only -- no code changes yet. Original request:

> "I am not happy with changing size for one algorithm. I would like to have the
> lower left field as kind of working field / playground for each algorithm with its
> optimized gui elements (e.g. a special band split element). Think about how to do
> that and explain your next steps."

This is the second revision, rewritten after your review of the first draft (your
answers and comments are kept verbatim in section 8).

## 1. Decisions from the review

| Topic | First draft | Now |
|---|---|---|
| Width knob | Fixed chrome above the playground, shared by all algorithms | **Part of each playground.** Each algorithm decides whether and how it shows width (Multiband gets per-band widths instead of one global knob). |
| Existing algorithms' controls | Unchanged; generic knob row reused | **May change.** Each algorithm gets the controls it really needs, including parameters that are currently hidden in `settings.json`. |
| Presets | Must stay compatible | **Compatibility not required.** New parameters may be added. |
| Rollout | Refactor, then fix size, then band split | **Infrastructure first, then one algorithm at a time**, each on its own branch. |
| Playground size | Decide after the band-split design | **Fixed now, at today's size** (your gut feeling: the band split will fit). Revisited only if a playground genuinely doesn't fit. |

## 2. Design goals (revised)

1. **Fixed size.** The lower-left card has one size for every algorithm. Switching
   algorithms never resizes the window.
2. **Each algorithm draws its own playground.** Its own controls, its own layout, and
   where it helps, a small live graphic (a filter curve, a band split, a reflection
   pattern) that shows what the controls actually do.
3. **Self-explanatory controls.** Every control is labelled with its unit, and the
   graphic reacts immediately when a control moves, so it's clear what each one
   does -- this is a teaching plugin as much as a tool (planing.md section 5). All
   playgrounds share one visual language: the same knob style, graph style and
   theme colours (Day/Night, see `PluginLookAndFeel.h`), so they look like parts of
   one plugin, not seven.
4. **The DSP layer stays GUI-free.** `algorithms/*.cpp` and `StereoAlgorithm.h` must
   not depend on `juce_gui_basics`: `tools/widener_render` compiles those same files
   into a console app linked only against `juce_audio_basics`/`juce_dsp`. Playgrounds
   live in `StereoWidener/` (GUI layer). The algorithms may *describe* their
   parameters and expose pure-math helpers (e.g. "magnitude response at f"), both
   plain data/functions with no GUI dependency, which the playgrounds use for drawing.

(Dropped from the first draft: "low cost for the common case" and "no
parameter/preset changes".)

## 3. Architecture

### 3.1 Per-algorithm parameter lists instead of aux-left/aux-right/multi

Today every algorithm receives `StereoAlgorithmParams { width, auxLeft, auxRight,
multi[6] }`, and `StereoWidener.cpp` has hand-maintained per-index tables
(`auxLeftParamIdFor()`, `auxRightParamIdFor()`, `auxMultiParamIdFor()`,
`paramsFor()`) mapping each algorithm's slots to parameter IDs. That model assumes
"Width + 2 knobs" and only works for Multiband via the bolted-on `multi` array. Once
each algorithm has its own set of 1-6 controls it no longer fits.

Proposal: each algorithm declares its own parameter list, as plain data:

```cpp
struct AlgorithmParamSpec          // GUI-free, lives in StereoAlgorithm.h
{
    const char* id;                // e.g. "msFilteredShelfGain"
    const char* name;              // e.g. "Shelf Gain"
    const char* unit;              // e.g. "dB"
    float min, max, defaultValue, step;
    bool logFrequency = false;     // true -> makeLogFrequencyParameter()-style range
};

virtual std::vector<AlgorithmParamSpec> getParamSpecs() const = 0;  // replaces getAuxLeftInfo()/getAuxRightInfo()/getNumMultiParams()/getMultiParamInfo()
```

and `process()` receives `std::array<float, kMaxAlgorithmParams> values`, indexed by
the algorithm's own enum (the pattern `MultibandWidth` already uses for `multi`).
`StereoWidenerAudio::addParameter()` then registers every algorithm's specs in a
loop, and `paramsFor()` fills `values` from the matching parameter pointers. That
removes the per-index ID tables entirely, and adding a parameter to an algorithm
becomes a one-place change inside that algorithm's own class.

Width becomes one of an algorithm's own parameters. One global `width` parameter
shared by all algorithms (today's `g_paramWidth`) vs. one width parameter per
algorithm is an open question (section 7.1).

### 3.2 `AlgorithmPlayground`: one `juce::Component` per algorithm

```cpp
class AlgorithmPlayground : public juce::Component   // StereoWidener/AlgorithmPlayground.h
{
public:
    explicit AlgorithmPlayground(juce::AudioProcessorValueTreeState& apvts);
    // resized()/paint() implemented per algorithm, always within the same fixed bounds
};
```

- One subclass per algorithm (`BroadbandPlayground`, `FilteredPlayground`,
  `BandSplitPlayground`, ...), each owning its own sliders/buttons and their
  `SliderAttachment`s, created **once** in `StereoWidenerGUI`'s constructor and
  bound permanently to their own parameters. Because each algorithm has its own
  parameters and its own controls, no rebinding happens on an algorithm switch --
  which also retires `bindAuxKnob()`/`bindMultiKnob()` and the whole class of stale
  slider value bugs behind the v0.1.14 crash fix.
- On an algorithm switch, `StereoWidenerGUI` only hides the old playground and shows
  the new one. All playgrounds get the same bounds in `resized()`.
- Live graphics are painted from the current parameter values (read via the
  APVTS on the message thread, repainted on parameter change through a listener or
  a low-rate timer) using the algorithm's own pure-math helpers -- never by reading
  DSP state across threads.

### 3.3 Shared building blocks

Built once, in the first algorithm step that needs them, then reused:

- `PlaygroundKnob`: label + rotary + value box, sized for the playground grid (today's
  aux-knob styling, packaged so every playground uses the same one).
- `FrequencyGraph`: log frequency axis (20 Hz-20 kHz), grid, a curve from a
  `std::function<float(float hz)>`, and optional draggable vertical handles bound to
  frequency parameters. Needed by MS Filtered, Complementary Comb, Multiband and
  Allpass.
- The existing "not mono-safe" badge stays shared: a thin strip at the bottom of the
  card, owned by `StereoWidenerGUI`, not by the playgrounds.

### 3.4 Fixed size

Today the card's inner area is about 290 x 175 px at scale 1.0 (card width
`480 - g_rightBlockWidth - g_panelDividerWidth - 2*g_panelPadding`; height set by
the taller of the knob row + badge and the Utilities column). Minus the shared badge
strip, each playground gets about **290 x 155 px**. `getRequiredContentHeight()`
stops depending on the active algorithm.

That is tight for "graph + 3-4 knobs": a row of four 48 px knobs with label and value
box is about 240 x 80 px, leaving about 70 px for a graph. Feasible, but if a
playground clearly doesn't fit, the fallback is to raise the fixed height once for
every algorithm (still never resizing on a switch), not to go back to per-algorithm
sizes.

## 4. Per-algorithm plan

"From settings" means the value currently lives in `settings.json` via
`GlobalSettings` (loaded once at construction, applied through a setter in
`StereoWidenerAudio`'s constructor, not automatable, not saved per project). Moving
these into real parameters means each algorithm must pick them up per block with
change detection and (where needed) smoothing, the way
`MultibandWidth::updateFrequenciesIfNeeded()` already does for its crossovers, and
the field is removed from `GlobalSettings`/`settings.json` in the same step.
Each new parameter defaults to the current hardcoded/settings value, so default
processing sounds identical before and after.

| # | Algorithm | Controls in its playground | New parameters | Graphic |
|---|---|---|---|---|
| 0 | M/S Width (Broadband) | Width | -- | Large Width knob; a readout/bar of what Width means in level terms (side gain in dB, mid/side balance). |
| 1 | M/S Width (Filtered) | Width, Bass Cutoff (Off), High Shelf Freq (Off), **High Shelf Gain** | High Shelf Gain (from settings, default 3 dB) | `FrequencyGraph` of the side-channel treatment (high-pass + high shelf); cutoff and shelf frequency draggable on the curve. |
| 2 | Complementary Comb | Width, Delay, Gain, **Crossover** | Crossover (from settings, default 300 Hz) | Complementary L/R comb responses on a `FrequencyGraph`, the crossover region shaded. |
| 3 | Allpass Decorrelation | Width, Amount, Spread | none planned (internal constants like the 4 stage base frequencies could become parameters later if useful) | The 4 allpass centre frequencies for L and R on a frequency axis, showing how Spread pushes them apart. |
| 4 | Multiband Width | 3 crossovers, per-band widths | Possibly a band-1 width (section 7.2); **no global Width knob** | `BandSplitPlayground`: see 4.1. |
| 5 | Early Reflections | Width, Amount, Room Size, **Pre-delay** | Pre-delay (from settings, default 5 ms) | Tap diagram: L taps above, R taps below a time axis, heights = tap gains, moving with Room Size and Pre-delay. |
| 6 | Chorus Doubler | Width, Amount, Depth, **Rate** | Rate (from settings, default 0.3 Hz) | The two modulated delay times over one LFO cycle (L and R, 90 degrees apart), scaled by Depth. |

### 4.1 Multiband Width: the band-split element

Corrected against `algorithms/MultibandWidth.cpp` (the first draft got this wrong):
3 crossovers split the signal into 4 bands. **Band 1 (lowest) is always mono** (its
side part is discarded, not scaled by Width). Bands 2-4 each have their own width
(`multibandWidth2/3/4`), and the global Width multiplies all three on top -- the
"single global width knob" that makes no sense here.

Playground:

- A `FrequencyGraph` spanning the crossovers' range, divided into 4 colour-coded
  bands by 3 draggable crossover handles (labelled with their frequency while
  dragging, exact value also editable).
- Inside each of bands 2-4, a width control (a small vertical slider or knob) placed
  in that band's own region, so "this width belongs to this band" is visible at a
  glance. The band's fill height or opacity can reflect its width.
- Band 1 shown as "Mono" (unless 7.2 adds a width for it).
- The global Width parameter is dropped from this algorithm; bands 2-4's own width
  ranges take over its job.
- Crossover ordering (Freq1 < Freq2 < Freq3) is already enforced by
  `clampCrossoverKnob()`; the handles keep that behaviour (a handle stops at its
  neighbour).

## 5. Rollout (each step its own branch, merged when verified)

**Step 0 -- infrastructure, no visual change yet.** ✅ Done in v0.1.16, see
[phase6_playground_step0.md](docs/algorithms/phase6_playground_step0.md). Deviation:
the per-algorithm Width (7.1, decided after this was written) was already introduced
here, since each algorithm's spec list had to be designed now anyway.
`AlgorithmParamSpec`/`getParamSpecs()` and `values[]` in the DSP interface (all 7
algorithms migrated mechanically, same parameters and IDs as today), the
`AlgorithmPlayground` base class, the switching logic in `StereoWidenerGUI`, the
fixed-size card, the shared badge strip, and `PlaygroundKnob`. Each algorithm
initially gets a plain "knobs only" playground reproducing today's controls, so the
plugin looks and sounds the same except that Multiband's 6 knobs must now fit the
fixed size (interim, replaced in its own step) -- this is where the window stops
resizing. `tools/widener_render` is updated for the new interface in the same step.

**Steps 1-7 -- one algorithm per step**, in this proposed order (simplest first, to
settle the visual language; then the ones that need `FrequencyGraph`; then the rest):

1. M/S Width (Broadband) -- smallest possible playground, sets the visual style.
   ✅ Done in v0.1.18; simplified in v0.1.20 to only the Width knob ("this should show
   how easy it is"), see [phase6_playground_broadband.md](docs/algorithms/phase6_playground_broadband.md).
2. M/S Width (Filtered) -- builds `FrequencyGraph`; moves High Shelf Gain out of
   settings.
   ✅ Done in v0.1.19, see [phase6_playground_filtered.md](docs/algorithms/phase6_playground_filtered.md).
3. Multiband Width -- the band-split element (reuses `FrequencyGraph`).
   ✅ Done in v0.1.21, see [phase6_playground_multiband.md](docs/algorithms/phase6_playground_multiband.md)
   (a dedicated `BandSplitView` sharing `LogFrequencyAxis` with `FrequencyGraph`).
4. Complementary Comb -- moves Crossover out of settings.
   ✅ Done in v0.1.23, see [phase6_playground_comb.md](docs/algorithms/phase6_playground_comb.md)
   (linear frequency axis instead of log: the teeth are evenly spaced in Hz).
5. Early Reflections -- moves Pre-delay out of settings.
   ✅ Done in v0.1.24, see [phase6_playground_early_reflections.md](docs/algorithms/phase6_playground_early_reflections.md).
6. Chorus Doubler -- moves Rate out of settings.
   ✅ Done in v0.1.25, see [phase6_playground_chorus.md](docs/algorithms/phase6_playground_chorus.md).
7. Allpass Decorrelation.

Each step: new playground, any new parameters, the `GlobalSettings` cleanup for that
algorithm, a doc in `docs/algorithms/`, `plan2.md` entry, version bump, README update.

## 6. Verification (every step)

- Offline GUI snapshot tool (throwaway, removed afterwards) across **all seven**
  algorithms, both themes, to confirm the new playground and no regression elsewhere.
- **Sound unchanged at defaults:** render the test signals through
  `tools/widener_render` before and after the step and compare with
  `python/stereo_eval`; new parameters default to the old hardcoded/settings values,
  so default output must match. Results saved to `python/results`.
- `pluginval --strictness-level 10`, several runs (the known, unrelated JUCE X11
  teardown crash excepted and identified by backtrace, as before).
- For interactive graphics (draggable handles): manual test in the Standalone build
  -- dragging updates the parameter and its readout, automation and undo still work,
  handles respect their limits. Automated tools don't exercise custom mouse code.

## 7. Open questions (none block Step 0)

1. **Width: one shared parameter or one per algorithm?** Shared (today's model) keeps
   the width setting when switching between similar algorithms and keeps the
   switch crossfade smooth; per-algorithm allows ranges and meanings that fit each
   algorithm (and matches "each algorithm has its own playground"). My
   recommendation: one width parameter per algorithm, since presets don't need to
   stay compatible anyway and it removes the last shared "special" control.
   Answer: "I agree, one width parameter per algorithm."

2. **Multiband band 1:** keep it fixed mono (today's DSP, "bass mono" is the point of
   the algorithm) or give it its own width like bands 2-4? Recommendation: keep it
   mono, clearly labelled, to avoid adding a control that mostly breaks mono
   compatibility.
   Answer: "I agree, keep band 1 mono."
3. **Step order** in section 5 -- fine as proposed, or do you want Multiband earlier
   since it motivated the change?
   Answer: "No, I think the order is fine as proposed. We can change it later if we want to."

## 8. Your review of the first draft (verbatim)

Answers to the first draft's questions:

1. *Width fixed above the playground?* -- "NO, we should change that."
2. *Phase order?* -- "yes we should change each aalgorith one by one."
3. *Fix the size now or later?* -- "difficult to say, but my gut-feeling is, that
   the band-split widget will fit in the space available."

Comments:

1. "Several algorithms have important parameters hidden in the settings. Therefore,
   I would loosen the strict requirement of no changes for the remaining
   algorithms. This changes the width idea as well. Again for the multiband approach
   a single global width knob is counterintuitive. So, the GUI playground should
   really adapt in the best possible way to the algorithm. For example, MS broadband
   has only one knob for width. MS filtered has at the moment width, lowcut and
   highcut. But we could add the gain of the high-shelv filter as well. Therefore,
   adapt this document in a second planning phase."
2. "Change the design goals: 2.3 and 2.5 are not valid anymore. Each algorithm can
   draw its own playground with its own controls. The playground should be designed
   in a way that it is clear to the user what the controls are doing. The width knob
   should be part of the playground and not fixed above it. The presets do not have
   to be compatible anymore, since we add new parameters if necessary."

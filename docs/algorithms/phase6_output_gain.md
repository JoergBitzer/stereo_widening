# Output Gain knob (v0.1.15)

User request, verbatim: *"lets add a third utility knob right next to the existing
knobs that adjust the gain of the output, range should be -24dB to +6dB, default is
0dB. The knob should be labeled 'Gain' and should allow for fine adjustments in 0.5dB
increments. If necessary reduce the size of the other knobs to accommodate the new
'Gain' knob."*

## What changed

A third knob, **Gain**, added to the Utilities card's top row, next to Rotation and
Balance -- -24 dB to +6 dB, 0.5 dB steps, defaulting to 0 dB.

- **Parameter** (`g_paramGain`, StereoWidener.h): a plain `AudioParameterFloat`, but
  built with `makeFloatParameterWithStep()` (StereoWidener.cpp) instead of the usual
  `makeFloatParameter()` -- the existing helper derives its slider/automation step
  from `10^-numDecimalPlaces`, which can only ever produce a power-of-ten increment
  (1, 0.1, 0.01, ...) and can't express 0.5. The new helper takes an explicit
  `stepSize` field instead. The displayed decimal count still comes out right on its
  own: `AudioParameterFloat`'s default text formatting derives it from the
  `NormalisableRange`'s own interval (0.5), not from a separately-declared
  `numDecimalPlaces` -- confirmed by reading JUCE's own
  `AudioParameterFloat::AudioParameterFloat()` (juce_AudioParameterFloat.cpp), which
  computes the decimal count from `range.interval` when no custom
  `stringFromValueFunction` is supplied.
- **DSP** (`UtilityProcessor.h`/`.cpp`): a `gainDb` field on `UtilityParams`, applied
  as the very last step of `UtilityProcessor::process()` -- deliberately *after*
  Monitor mode, not before, so it also trims whatever is currently being auditioned
  (Mono Check / Solo Side), the same way a real output fader would sit after a
  solo/mute section on a mixer. `juce::Decibels::decibelsToGain()` converts once per
  block, outside the per-sample loop.
- **GUI**: the knob row (`StereoWidenerGUI::resized()`) now lays out three knobs
  (Rotation / Balance / Gain) instead of two. `g_utilKnobSize` shrunk 48 -> 40px (per
  the request's own fallback instruction) so `3*40 + 2*12` (`g_utilKnobGap`) = 144px
  matches the existing toggle-button row's own width (`3*44 + 2*6` = 144px) -- the
  Utilities card doesn't need to widen at all.

## Verification

**Offline GUI render** (throwaway `WidenerGuiSnapshot` console tool, built, used, and
fully removed afterwards, including the `add_subdirectory()` line in
`/home/bitzer/AudioDev/CMakeLists.txt`, per the project's convention): confirmed the
three knobs render evenly spaced, at both extremes of the display text ("-24.0 dB",
"6.0 dB" -- neither clipped) and at the default ("0.0 dB"), with no regression to the
Width/aux-knob row, the mono-safe badge, or the toggle-button row's own width match,
across both a broadband and a not-mono-safe algorithm (Chorus Doubler).

**pluginval --strictness-level 10**: 4/6 plain runs `SUCCESS` with zero assertion
warnings; the other 2 hit the same pre-existing, unrelated `SIGSEGV` inside JUCE's own
`XEmbedComponent::Pimpl::handleX11Event` during editor teardown documented in
[phase6_gui_thirds.md](phase6_gui_thirds.md#crash-fix-v0114-out-of-range-log-frequency-values-crashing-pluginval)
-- confirmed via a fresh `gdb`-loop reproduction showing the identical backtrace (no
application frames at all), i.e. not a regression from this change.

## Files

- `StereoWidener/StereoWidener.h`: `g_paramGain` (new); `m_gainParam`; GUI members
  `m_gainLabel`/`m_gainKnob`/`m_gainAttachment`.
- `StereoWidener/StereoWidener.cpp`: `makeFloatParameterWithStep()` (new helper);
  `addParameter()`/`prepareParameter()`/`processSynchronBlock()` wire up the new
  parameter; constructor builds the Gain knob/label/attachment; `resized()` lays out
  three utility knobs instead of two.
- `StereoWidener/UtilityProcessor.h`/`.cpp`: `gainDb` field, applied last.
- `StereoWidener/PluginSettings.h`: `g_utilKnobSize` 48 -> 40.
- `StereoWidener/README.md`: documents the new knob.
- `StereoWidener/CMakeLists.txt`: version bumped 0.1.14 -> 0.1.15.

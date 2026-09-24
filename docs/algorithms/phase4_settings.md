# Phase 4 -- Global settings file (started)

Phase 4's plan (plan2.md) is "Settings, utilities, latency": a global ini file with
user defaults/preferences, stereo utilities (mono, swap, invert, rotation, balance),
and per-algorithm latency reporting. This first step covers the settings-file
infrastructure and its first actual setting.

## Format: JSON, not juce::PropertiesFile

plan2.md originally specified `juce::PropertiesFile`. User request instead: "I would
prefer a simple YAML or TOML format. JSON is also OK." `juce::PropertiesFile` only
supports XML or binary storage (`PropertiesFile::StorageFormat`) -- no YAML/TOML/JSON
option. JUCE has no built-in YAML or TOML parser either; adding one would mean vendoring
a third-party library. `juce::JSON` (parse/stringify a `juce::var` tree), however, is
already part of `juce_core` with no extra dependency, so **JSON** is what
`GlobalSettings` uses.

## `GlobalSettings` (`StereoWidener/GlobalSettings.h`/`.cpp`)

A small class, constructed once by `StereoWidenerAudio`'s constructor:
- Reads `~/.config/StereoWidener/settings.json` (Linux;
  `juce::File::userApplicationDataDirectory` resolves to the right platform-specific
  location elsewhere) via `juce::JSON::parse()`.
- If the file doesn't exist yet, writes it with default values first -- so a first-time
  user has something to find and edit, rather than a setting that silently does nothing
  until they know to create the file themselves.
- If the file exists but is missing a key, or isn't valid JSON at all, falls back to
  the compiled-in default for whatever couldn't be read, rather than failing to load or
  crashing the plugin. Verified with a throwaway tool: deleting the file, editing a
  value, and corrupting the file with invalid JSON all behave as expected (create with
  defaults, pick up the edited value, and fall back to the default without crashing).
- Loaded once at construction, not watched for live changes while a plugin instance is
  open -- matches plan2.md's own description ("the ini file only provides the defaults
  and user preferences"), and keeps the implementation simple for this first setting.

These are defaults, not part of a DAW project's saved state -- each plugin instance
still saves/restores its own actual parameter values via
`AudioProcessor::getStateInformation()` exactly as before.

## First setting: the side high-shelf's gain

User request: "we should add the 3dB for the highpass shelf in the configuration
file." `MSWidthFiltered`'s side-channel high-shelf boost (see
[phase3_stereo_widener.md](phase3_stereo_widener.md)) was a compiled-in constant,
`kHighShelfGainDb = 3.0f`, explicitly flagged there as "fixed for this first version".
It is now `GlobalSettings::getHighShelfGainDb()`, default 3.0 dB, editable in
`~/.config/StereoWidener/settings.json`:

```json
{
  "highShelfGainDb": 3.0
}
```

**Design choice: not a `StereoAlgorithmParams` field.** `StereoAlgorithmParams`
(`algorithms/StereoAlgorithm.h`) carries the *per-block, host-automatable* values
(Width, and the two aux-knob values) that `StereoWidenerAudio` rebuilds every
`processSynchronBlock()` call from live `AudioParameterFloat`s. The shelf gain is
neither automatable nor per-block-varying -- it is a fixed default loaded once at
startup -- so it goes through a new `MSWidthFiltered::setHighShelfGainDb(float)`
setter instead, called once by `StereoWidenerAudio`'s constructor right after creating
the algorithm instances:

```cpp
if (auto* filtered = dynamic_cast<MSWidthFiltered*>(m_algorithms[1].get()))
    filtered->setHighShelfGainDb(m_globalSettings.getHighShelfGainDb());
```

This also keeps `algorithms/` decoupled from `GlobalSettings`: `MSWidthFiltered` only
ever receives an already-resolved gain value, with no knowledge of where it came from
(an early draft had `MSWidthFiltered::getDescription()` reference
`GlobalSettings::getSettingsFile()` directly for a nicer help-popup message; reverted,
since `algorithms/` classes are meant to stay self-contained -- see planing.md section 5
-- and the "?" help popup's text saying "configurable in the global settings file" is
just as clear without the cross-module include).

`setHighShelfGainDb()` also resets the cached "last high shelf frequency" so the
filter's `juce::dsp::IIR` coefficients recompute with the new gain on the very next
`process()` call, even if called after processing has already started (not currently
exercised -- the gain is only ever set once, at construction -- but a defensive detail
that costs nothing to get right up front).

## Verification

A throwaway console tool (cleaned up after use, per the project's established pattern)
checked, directly against `GlobalSettings` and `MSWidthFiltered`, without needing the
full plugin:
- Deleting the settings file and constructing `GlobalSettings` creates it with
  `{"highShelfGainDb": 3.0}` and reports 3.0 dB.
- Editing the file to `6.0` and constructing a fresh `GlobalSettings` reports 6.0 dB.
- Corrupting the file with invalid JSON falls back to the compiled-in 3.0 dB default,
  without crashing.
- Rendering a 12 kHz test tone through `MSWidthFiltered` with `setHighShelfGainDb(3)`,
  `(6)`, and `(0)` measured a side-channel boost of approximately 2.7, 5.4, and 0.0 dB
  respectively (a shelf filter approaches but does not exactly reach its nominal gain
  at a frequency only ~0.6 octaves above the corner, so these being close to, not
  exactly, 3/6/0 dB is expected) -- confirming the loaded value genuinely changes the
  filter's output, in the right proportion (2.7:5.4 matches 3:6).

`pluginval --strictness-level 10` passes on the rebuilt VST3, and running it (which
constructs a `StereoWidenerAudio`, and therefore a `GlobalSettings`) confirmed the
settings file is created for real at `~/.config/StereoWidener/settings.json` during
normal plugin use, not just in the isolated test tool.

## Not yet implemented (remaining Phase 4 scope, per plan2.md)

- Profile (Mastering / Creative), last-used-state-as-default, and GUI/meter option
  defaults are not yet part of `GlobalSettings` -- only the one setting requested so
  far. The class is intentionally small and easy to extend with more JSON keys later.
- Utilities (2.13/2.2: mono, L/R swap, polarity invert, rotation, balance, mono check,
  solo side) -- not started.
- Latency reporting per algorithm (`StereoAlgorithm::getLatencySamples()` exists but
  isn't yet connected to `AudioProcessor::setLatencySamples()` on a switch) -- not
  needed yet since both current algorithms report 0 latency.

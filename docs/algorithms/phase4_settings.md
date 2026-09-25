# Phase 4 -- Settings and utilities (latency not started)

Phase 4's plan (plan2.md) is "Settings, utilities, latency": a global settings file
with user defaults/preferences, stereo utilities (mono, swap, invert, rotation,
balance), and per-algorithm latency reporting. Settings and utilities are both done (a
profile setting was deliberately skipped, see below); latency reporting has not been
started, since every current algorithm reports 0 latency and there is nothing to report
yet.

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

## Step 2: utilities (planing.md 2.13 + 2.2)

User request: "lets start with phase 4 step 2 utilities." planing.md 2.13 ("Utilities,
not algorithms, but should be in the plugin"): "Mono (sum), L/R swap, polarity invert
L/R, balance, and 'mono check' (listen to `L+R`) as well as 'solo side' (listen to
`S`)"; plan2.md groups this with 2.2 (Stereo Rotation): "Together with M/S width this
gives a complete 'image editor': width, rotation, balance, and polarity inversion of one
channel."

**New `UtilityProcessor`** (`StereoWidener/UtilityProcessor.h`/`.cpp`), applied once in
`StereoWidenerAudio::processSynchronBlock()` after the selected width algorithm's
(possibly crossfaded) output, regardless of which algorithm is active -- unlike
`MSWidthBroadband`/`MSWidthFiltered`, it is not itself a `StereoAlgorithm`: it always
runs. Stateless (pure per-sample math), so it needs no `prepare()`/`reset()`. Order:
Rotation -> Balance -> Invert L/R -> Swap L/R -> Monitor mode.

- **Rotation** (2.2): the textbook 2x2 image-plane matrix,
  `L' = L*cos(phi) - R*sin(phi)`, `R' = L*sin(phi) + R*cos(phi)`. Range -45..+45
  degrees (a full ±180 degrees would just relabel L/R content already reachable other
  ways). New knob, left of Balance in the Utilities row.
- **Balance**: a simple attenuate-one-side law (not an equal-power pan law -- that is
  for panning a mono source, this adjusts an already-stereo signal's relative level):
  positive attenuates L (image moves right), negative attenuates R (image moves left).
  Range -100..+100 %.
- **Invert L / Invert R**: independent polarity-invert toggles, one button each
  (`AudioParameterBool` + `ButtonAttachment`, JUCE's standard toggle-button pattern).
- **Swap L/R**: one toggle button, swaps the two channels.
- **Monitor mode** (a 3-item `AudioParameterChoice`: Normal / Mono Check (L+R) / Solo
  Side (S)): applied last, as an override for auditioning -- it replaces L/R with the
  mono sum or the side signal so it always reflects everything upstream. This one
  control deliberately covers two planing.md 2.13 entries that are the same DSP
  operation viewed two ways -- "Mono (sum)" and "mono check (listen to L+R)" -- plus
  "solo side"; consolidated into one 3-way selector rather than a redundant separate
  "Mono" toggle that would compute the identical `(L+R)/2`.

**GUI**: a new "Utilities" section below the algorithm selector -- Rotation/Balance
knobs (smaller than the aux knobs, `g_utilitiesKnobSize` = 56 vs. 64, since this row
also needs space for the toggle buttons and Monitor selector), then a row with the
three toggle buttons and the Monitor combo box. `PluginSettings.h`'s `g_minGuiSize_y`
grew from 491 to 655 to fit it (title 18 + knob row ~84 + toggle row 28 + gaps, with a
small margin) -- verified with an offline render at the new default size: every control
fits with ~45px of vertical margin to spare, and the toggle buttons visibly distinguish
their on/off state (JUCE's default LookAndFeel dims/brightens a `TextButton` based on
`getToggleState()` once `setClickingTogglesState(true)` is set).

**Verification**: a throwaway console tool checked `UtilityProcessor::process()`
against hand-computed expected outputs for all nine cases (rotation at 0 and 90
degrees, both balance directions, invert, swap, both monitor modes, and plain
passthrough with every parameter at its default) -- all matched to within floating-point
tolerance. `pluginval --strictness-level 10` passes, including its parameter-fuzzing
test, now also covering all six new parameters (rotation, balance, invert L, invert R,
swap, monitor mode) with no crashes.

## Step 1 continued: last-used state, GUI size, meter options

> **Correction (Phase 5 follow-up, see
> [phase5_comb.md](phase5_comb.md#removing-last-used-state-neutral-defaults-instead)):
> the "last used state" mechanism described in this section was removed.** It seeded
> each parameter's construction-time default (and therefore also its GUI knob's
> double-click-reset value, which JUCE wires to the parameter's default automatically)
> from whatever was last saved, so double-clicking a knob reset it to whatever was last
> dialled in rather than to a neutral/pass-through value -- an undesired side effect not
> caught at the time. `GlobalSettings::getLastUsedParam()`/`saveLastUsedState()` and the
> `lastUsedState` JSON field no longer exist; every parameter's default is now a fixed,
> compiled-in, neutral value (Width 100 %, Bass Cutoff/High Shelf/Comb Gain Off/0 %,
> etc.), and restoring a previous session's settings is now the `init` preset's job
> (`PresetHandler`), not `GlobalSettings`'. The rest of this section is kept as a
> historical record of the original (superseded) design; the GUI-size/meter-option
> parts described below are unaffected and still work as described.

User feedback while starting step 2: "I am not sure if the settings file should include
profile. The last used state for new instances is a good idea. Here it is important
that the settings inside the project will be used if exist. The same goes for GUI size
and meter options. However, implement the additional settings." Decisions:
- **Profile (Mastering/Creative)**: skipped, per the user's own stated uncertainty --
  not added to `GlobalSettings` this round.
- **Last used state**: implemented, for every parameter (Width, Algorithm, Bass Cutoff,
  High Shelf, Rotation, Balance, Invert L, Invert R, Swap L/R, Monitor mode).
- **GUI size**: implemented -- the default scale factor for a brand new instance now
  comes from `GlobalSettings` instead of a hardcoded `1.0`.
- **Meter options** (RMS integration time, peak hold time, peak decay rate): the
  *defaults* are implemented; StereoWidener does not yet have a per-project override
  mechanism for these at all (unlike `StereoAnalyzer`'s Settings popup), so there is
  nothing for the global default to be overridden by yet -- see "Not yet implemented"
  below.

**"Project state always wins" -- how it works, without any extra code.** JUCE's own
plugin lifecycle already gives this for free, as long as the global default is only
ever used to seed the parameter's *construction-time* value: `AudioProcessor::
setStateInformation()` (called by the host after construction, when a DAW project has
its own saved state) unconditionally replaces the whole parameter tree
(`m_parameterVTS->replaceState(vt)`, `PluginProcessor.cpp`, unchanged from before this
work) and separately overwrites `m_pluginScaleFactor` from the project's own
`PluginSize`/`ScaleFactor` XML property (also already existed). So `GlobalSettings`'
"last used" values only need to influence what a *brand new* instance's parameters
and scale factor start as, at construction time -- anything a project restores
afterwards naturally takes priority, with no extra "which one wins" logic needed.

**Where `GlobalSettings` lives, and why.** It stays owned by `StereoWidenerAudio`
(`m_algo`), not by `StereoWidenerAudioProcessor` itself, for an initialization-order
reason: `StereoWidenerAudioProcessor`'s constructor builds `m_algo` via its own
member-initializer-list (`m_algo(this)`) *before* the constructor's own body runs, and
C++ constructs members in declaration order -- `m_algo` is declared first in
`PluginProcessor.h`, so only members inside it (not sibling members declared after it)
are guaranteed constructed by the time anything inside `m_algo`'s own construction runs.
A new `StereoWidenerAudio::getGlobalSettings()` accessor lets
`StereoWidenerAudioProcessor` reach it safely from its own constructor *body* and
destructor *body* (both run after/before `m_algo` is fully alive), for the GUI-scale
default and for saving "last used" state, respectively.

**Saving "last used" state**: `StereoWidenerAudioProcessor`'s destructor (previously
empty) now reads every current parameter's raw value via
`AudioProcessorValueTreeState::getRawParameterValue()` and calls
`GlobalSettings::saveLastUsedState()`, which rewrites the whole settings file (all
fields, not just the changed ones -- `GlobalSettings::write()` always serialises every
member). **Seeding new defaults**: `StereoWidenerAudio::addParameter()`'s
`makeFloatParameter()`/`makeFrequencyParameterWithOff()`/
`makeLogFrequencyParameterWithOff()` helpers now take an explicit `defaultValue`
argument (rather than always using the `g_param*` struct's own compiled-in default),
passed as `GlobalSettings::getLastUsedParam(id, compiledInDefault)` -- clamped to the
parameter's own range defensively, in case a hand-edited settings file has a
stale/out-of-range value from before a range changed.

**JSON schema** (all fields optional; a missing one keeps its compiled-in default):

```json
{
  "highShelfGainDb": 3.0,
  "guiScaleFactor": 1.0,
  "meterIntegrationTimeS": 0.3,
  "meterPeakHoldTimeS": 1.5,
  "meterPeakDecayDbPerS": 20.0,
  "lastUsedState": {
    "width": 150.0,
    "algorithm": 1,
    "bassCutoff": 120.0,
    "highShelfFreq": 8000.0,
    "rotation": -12.5,
    "balance": 0.0,
    "invertL": 0.0,
    "invertR": 0.0,
    "swapLR": 0.0,
    "monitorMode": 0.0
  }
}
```

**Verification**: a throwaway console tool checked `GlobalSettings` directly (no need
for the full plugin, matching the pattern from step 1's own verification): a fresh
instance reports every compiled-in default correctly; `saveLastUsedState()` followed by
constructing a new `GlobalSettings` correctly reloads the GUI scale factor and every
saved parameter value, while an *unsaved* parameter ID still falls back to its given
default; hand-editing the meter-option fields directly in the file and reconstructing
also loads correctly. All 15 checks passed.

What was **not** verified end-to-end in this sandbox (no interactive DAW/GUI available,
see `../README.md`): actually opening the real plugin, changing a value, closing it,
reopening a fresh instance, and confirming the change is really there. The reasoning
above (constructor/destructor timing, `setStateInformation()`'s override behaviour) was
traced carefully by hand and pluginval's repeated construct/destroy cycles (many
sample-rate/block-size combinations) completed without crashing, but that is not the
same as confirming the *values* survive a real close/reopen -- worth a manual check in
a DAW when convenient.

## Not yet implemented (remaining Phase 4 scope, per plan2.md)

- Profile (Mastering / Creative) -- deliberately skipped, see above.
- A per-project meter-options override (a Settings popup, like `StereoAnalyzer`'s) --
  `GlobalSettings`' meter-option defaults have nothing to be overridden by yet.
- Latency reporting per algorithm (`StereoAlgorithm::getLatencySamples()` exists but
  isn't yet connected to `AudioProcessor::setLatencySamples()` on a switch) -- not
  needed yet since both current algorithms report 0 latency.
- Manual end-to-end verification of "last used state" surviving a real close/reopen in
  a DAW (see the Verification note above).

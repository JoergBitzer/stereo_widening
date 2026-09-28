# Factory presets, step 1: the presets (v0.1.31)

Plan: [plan2.md](../../plan2.md), Phase 6, 3. Twenty presets, mostly per instrument,
each with the algorithm that fits it best (mono sources get algorithms that *create*
stereo, stereo sources the mono-safe M/S ones), plus a neutral **Init**. No categories
(user decision).

## How they are made

- `python/make_factory_presets.py` holds the table (name, algorithm, values) and
  writes one XML file per preset into `StereoWidener/presets/`, in PresetHandler's own
  format. Every file holds **all** 34 parameters; anything a preset doesn't set is at
  its default, so loading a preset always gives the same sound (utility section
  neutral except the output Gain, see below).
- Each file has `bank="Factory"`, `category="Unknown"`, `version` (plugin version) and
  a new `presetversion="1"` -- to be raised when a preset's values change (used by
  step 2, the improved deployment).
- Checks: every value inside its parameter's range, crossovers ascending (at least
  5 % apart); passing the path of a freshly written `Init.xml` checks that the
  script's parameter list and defaults still match the plugin (done: 34 parameters
  match v0.1.30/31).
- `StereoWidener/CMakeLists.txt`: `FACTORY_PRESETS` defined, `presets/*.xml` embedded
  with `juce_add_binary_data` (target `StereoWidener-presets`, linked to the plugin).

Deployment is PresetHandler's existing mechanism, unchanged in this step: on the very
first start (preset folder didn't exist yet) all embedded XML files are copied there.
Verified with a fresh `HOME`: 21 files deployed, contents as generated. On machines
that already have the folder nothing is deployed yet -- that is step 2.

## Tuning

`python/evaluate_factory_presets.py` renders each preset through `WidenerRender` (the
plugin's own algorithm classes) on a source close to its use -- the mono version
(L = R) of the synth, bass, vocal or drum loop for mono-source presets, pink noise with
correlation 0.5 for stereo instruments, a mix loop for the master presets -- and
measures loudness change, correlation, and the mono sum's level change and colouration.

Changes against the first table in plan2.md:

- **Allpass presets** (Synth Arp, Strings, Percussion): at Amount 50-70 % the mono sum
  lost 5 dB with up to 10 dB colouration. A sweep (Amount 20-100 %, Spread 40/80 %)
  showed colouration growing quickly above ~35 % and with larger Spread; now Amount
  25-35 %, Spread 40 %, Width 110 % (colouration 2.4-3 dB).
- **Bass presets** (Synth Bass, Bass Guitar): almost no effect on the bass loop
  (S-M -22/-28 dB) with the crossover at 500/800 Hz; now 250/400 Hz, Gain 60 %.
- **Loudness**: chorus and allpass presets were 1.6-2.6 LUFS quieter, the comb lead
  and the stereo acoustic guitar 1-1.4 LUFS louder. Since there is no auto gain, these
  presets set the output Gain (rounded to 0.5 dB); all presets are now within
  -0.1..+0.9 LUFS of their source.

Result ([evaluation.txt](../../python/results/factory_presets/evaluation.txt)):

| Preset | Algorithm | dLUFS | corr in -> out | mono-sum change / colouration |
|---|---|---|---|---|
| Synth Lead - Pseudo Stereo | Comb | -0.1 | 1.00 -> 0.50 | -1.5 dB (the Gain) / 0.0 dB |
| Synth Bass - Wide Top | Comb | +0.2 | 1.00 -> 0.91 | 0 / 0 |
| Synth Pad - Big | Multiband | +0.7 | 0.49 -> 0.64 | 0 / 0 |
| Synth Arp - Shimmer | Allpass | -0.1 | 1.00 -> 0.58 | -1.2 / 3.0 |
| Guitar Clean - Chorus Wide | Chorus | 0.0 | 1.00 -> 0.38 | -1.7 / 3.3 |
| Guitar Rhythm - Double | Chorus | +0.1 | 1.00 -> 0.64 | -0.7 / 3.0 |
| Acoustic Guitar - Mono Mic | Early Refl. | +0.1 | 1.00 -> 0.95 | 0 / 1.2 |
| Acoustic Guitar - Stereo Pair | Filtered | 0.0 | 0.49 -> 0.55 | -1.0 (the Gain) / 0 |
| Piano - Stereo | Multiband | +0.5 | 0.49 -> 0.67 | 0 / 0 |
| Electric Piano - Pseudo Stereo | Comb | +0.8 | 1.00 -> 0.70 | 0 / 0 |
| Organ - Rotary Feel | Chorus | +0.1 | 1.00 -> 0.06 | -2.6 / 2.3 |
| Strings - Mono Patch | Allpass | +0.2 | 1.00 -> 0.71 | -0.6 / 2.4 |
| Lead Vocal - Space | Early Refl. | +0.1 | 1.00 -> 0.99 | 0 / 0.8 |
| Backing Vocals - Wide | Chorus | +0.2 | 0.99 -> 0.37 | -1.4 / 2.4 |
| Drums - Overheads | Filtered | +0.9 | 0.49 -> 0.58 | 0 / 0 |
| Drums - Bus | Multiband | +0.3 | 0.49 -> 0.68 | 0 / 0 |
| Percussion - Mono Shaker | Allpass | -0.1 | 1.00 -> 0.90 | 0 / 2.9 |
| Bass Guitar - Grit Only | Comb | +0.1 | 1.00 -> 0.96 | 0 / 0 |
| Master - Gentle Widen | Filtered | 0.0 | 0.99 -> 0.99 | 0 / 0 |
| Master - Tight Low End | Multiband | 0.0 | 0.99 -> 0.99 | 0 / 0 |

The M/S and comb presets keep the mono sum exact (0 dB colouration); the chorus,
allpass and early-reflection presets trade some mono compatibility for real width
from mono, as expected. The master presets barely change the (almost mono) mix loop --
intentionally gentle. The values are measured on stand-in material (no guitar, organ
or string recordings in `test_signals/`); they are starting points, worth a listening
pass on real instruments.

## Verification

- Fresh install (temporary `HOME`): 21 presets deployed, files as generated.
- pluginval --strictness-level 10: 3/3 SUCCESS, zero JUCE assertions.
- The existing user preset folder and settings were not touched.

## Files

- `python/make_factory_presets.py`, `python/evaluate_factory_presets.py` (new).
- `StereoWidener/presets/*.xml` (new, generated).
- `StereoWidener/CMakeLists.txt`: `FACTORY_PRESETS`, binary data, 0.1.30 -> 0.1.31.

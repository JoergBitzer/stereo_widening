# Stereo Widening Plugin – Plan 2

This plan replaces the roadmap in `planing.md`. It includes the suggested roadmap changes
and the answers to the open questions. The algorithm theory (formulas, pros and cons,
references) stays in `planing.md`, section 2 and section 8. It is not repeated here.

---

## 1. Decisions (from the answers)

| Topic | Decision | Consequence for the plan |
|-------|----------|--------------------------|
| Target group | **Both**: neutral mastering/mixing tool *and* creative effect. The user chooses. The choice is saved in an **ini file**. | A global *profile* setting (Mastering / Creative), saved in a user ini file (see 4.4). Each algorithm is tagged "mono-safe" or "creative". |
| Mono input | Supported, but **stereo is the default**. Mono is not forced. | Bus layouts: stereo→stereo (default) and mono→stereo. Pseudo-stereo modes are the useful ones for mono input. |
| Latency | **Acceptable** | STFT modes and linear-phase crossovers are allowed. Latency is shown in the GUI. |
| Playback | **Loudspeakers** first. Headphones come in a later version, with a clear note in the GUI. | Crosstalk cancellation (2.10) moves to the top of v2. Headphone crossfeed comes later. Evaluation uses a loudspeaker model (±30°). |
| Teaching | **Yes.** The code is a teaching example for students and young engineers, and at the same time a professional tool. | Code rules (section 5). One small class per algorithm. Python reference and short documentation for each algorithm. |

---

## 2. Algorithms and Release Scope

Numbers refer to `planing.md`, section 2.

| # | Algorithm | Tag | Release |
|---|-----------|-----|---------|
| 2.1 | M/S width + bass mono + side shelf | mono-safe | **v1 (first)** |
| 2.2 | Rotation / 2×2 matrix (utility) | mono-safe | v1 (utilities) |
| 2.3 | Haas delay | creative | v1 |
| 2.4 | Complementary comb (above crossover) | mono-safe | v1 |
| 2.5 | Allpass / velvet-noise decorrelation | creative | v1 |
| 2.7 | Multiband width (LR4 or linear phase) | mono-safe | v1 |
| 2.10 | Crosstalk cancellation (loudspeakers) | loudspeaker | **v2 (first)** |
| 2.8 | STFT panning expansion / primary–ambient | mono-safe | v2 |
| 2.9 | PCA / adaptive rotation | mono-safe | v2 |
| 2.11 | Micro-pitch / chorus doubler | creative | v2 |
| 2.6 | Spectral interleaving | mono-safe | v2 |
| 2.12 | Early reflections | creative | later |
| – | Headphone crossfeed (Bauer) | headphone | later, with GUI note |

- **Mastering profile:** shows mono-safe and loudspeaker modes only. The default is M/S.
- **Creative profile:** shows all modes. Creative modes carry a visible
  "not mono-safe" badge.

---

## 3. Project Layout

The project contains **two plugins** that share code. The analyzer comes first and is the
base of the widener.

```
stereo_widening/
├── planing.md, plan2.md, README.md
├── .gitignore                # excludes test_signals/, build/, python caches
├── shared/                   # code used by both plugins
│   ├── metering/             # goniometer, correlation meter, level meters, FIFO
│   └── dsp/                  # M/S helpers, crossovers, smoothing wrappers
├── StereoAnalyzer/           # plugin 1: analysis only (from AdvancedAudioTemplate)
├── StereoWidener/            # plugin 2: starts as a copy of StereoAnalyzer
│   └── algorithms/           # one .h/.cpp per algorithm
├── python/
│   ├── stereo_eval/          # measures (correlation, mono sum, level, IACC)
│   ├── algorithms/           # Python reference of each algorithm
│   ├── generate_test_signals.py
│   └── notebooks/
├── test_signals/             # NOT in the repository
│   ├── samples/              # copied from /home/bitzer/Music/samples/
│   └── generated/            # output of generate_test_signals.py
├── test_signals.md           # list of which samples were copied from where (in the repo)
└── docs/algorithms/          # one short page per algorithm (theory, parameters, plots)
```

- Both plugins are added to the top-level `AudioDev/CMakeLists.txt` with
  `add_subdirectory(stereo_widening/StereoAnalyzer)` and
  `add_subdirectory(stereo_widening/StereoWidener)`.
- `shared/` means the widener reuses the analyzer's meters. They are not copied, so a
  bug fix helps both plugins.
- Start a git repository (the folder is not under version control yet).

---

## 4. Roadmap

### Phase 0 – Setup
1. `git init`, `.gitignore` (with `test_signals/`), README skeleton.
2. Create the folder structure from section 3.
3. Copy a few samples to `test_signals/samples/` and record the source paths in
   `test_signals.md`. The samples are excluded from the repository (licences).
   Candidates:
   - Speech, dry and wet: `Function Loops - AI - Speech Vocal Hooks/FL_AIV_Spoken_Vocals_Dry`
     and `…_Wet`
   - Singing or ad-libs: `UNDRGRND Sounds …/Lo-Fi Soul/US_LFS_Vocal_Phrases`
   - Drums (transients): `wa_synth_drums/unprocessed drums`, `musicradar_sub/musicradar-realworld-drum-samples`
   - Stereo loops or mix-like material: `musicradar_sub/musicradar-pop-samples`,
     `musicradar-electronic-pop-samples`

### Phase 1 – Test signals and Python evaluation (roadmap suggestion 2)
1. `generate_test_signals.py` writes reproducible WAV files (fixed seed, 48 kHz):
   - mono pink noise (L = R)
   - independent pink noise L/R (ρ ≈ 0)
   - polarity-inverted noise (ρ = −1)
   - log sine sweep
   - hard-panned and partly panned sources (e.g. speech at −100 %, −50 %, 0, +50 %, +100 %)
   - mono speech + stereo reverb (a synthetic, decorrelated exponential noise tail)
   - a small "mix" built from the copied samples (drums centred, vocal slightly panned)
2. `stereo_eval` Python package:
   - **Correlation** ρ, broadband and per 1/3 octave, and over time (block-wise).
   - **Mono-sum colouration:** `|L'+R'|` vs. `|L+R|` in dB per 1/3 octave, and the maximum
     deviation as one number.
   - **Level change:** RMS and LUFS (`pyloudnorm`), in vs. out. This is used to calibrate
     auto gain.
   - **IACC** for loudspeaker playback: convolve L/R with HRIRs for ±30° (e.g. KEMAR or
     a SOFA file), then compute the interaural cross-correlation (max in ±1 ms, broadband
     and per band).
   - **Report function:** `evaluate(in_wav, out_wav) → dict + plots`. The same report is
     used for Python prototypes and for audio rendered by the plugin.
3. Validate the measures with signals whose values are known (mono noise gives ρ = 1,
   independent noise gives ρ = 0, polarity-inverted noise gives ρ = −1).
4. Python reference of **2.1 (M/S + bass mono + side shelf)**, evaluated with the report.

### Phase 2 – StereoAnalyzer plugin (roadmap suggestion 1) — done except the spectrum
A separate plugin, created from AdvancedAudioTemplate. It passes the audio through unchanged.
Details, findings and the cross-check numbers: `docs/algorithms/phase2_stereo_analyzer.md`.
1. ✅ Template copy, rename to `StereoAnalyzer`, VST3/Standalone build verified, GUI
   confirmed running (screenshot in the docs page). Along the way, found and fixed a
   real heap-corruption bug in the (per-plugin copied) `SynchronBlockProcessor`'s
   direct-through mode — worth backporting to AdvancedAudioTemplate, see the docs page.
   `pluginval --strictness-level 10` now passes cleanly. **Verified live in Reaper**
   with a real audio file playing through it — goniometer, level meters and correlation
   meter all update correctly (screenshot and notes in the docs page).
2. ✅ Metering components in `shared/metering/` (`StereoMeterState`, `MeterFifo`,
   `GoniometerComponent`, `CorrelationMeterComponent`, `LevelMeterComponent`):
   - lock-free FIFO from the audio thread to the GUI, and a timer-based repaint
   - **Goniometer / vectorscope** (M vs. S, with persistence/fade; the fade duration is
     a settings-page parameter, "Afterglow", in seconds, sample-rate-independent)
   - **Correlation meter** (−1 … +1, selectable integration time: 100/300/1000 ms)
   - **L/R/M/S level meters** (peak and RMS), S/M ratio ("width estimate"); balance
     display deferred to Phase 4 (utilities)
   - **Spectrum** of `L+R` and `L−R`: not yet implemented (optional in the original
     plan; candidate for a later pass using `TGMStaticLib/FFT.h`)
3. ✅ **Cross-check against Python:** `tools/meter_crosscheck` (headless console tool,
   no GUI/display needed) + `python/crosscheck_meter.py`. All four stationary test
   signals (mono, ρ=0.5, uncorrelated, anti-phase noise) match `stereo_eval` within
   tolerance; correlation is exactly +1 / −1 / ≈0 as expected.
4. The analyzer is usable as a standalone tool once GUI verification is done in a DAW.

### Phase 3 – StereoWidener, first algorithm — done
Details, findings and verification: `docs/algorithms/phase3_stereo_widener.md`.
1. ✅ Copied StereoAnalyzer to `StereoWidener` (same tools/, template plumbing).
   Meters now show **input and output**: two `StereoMeterState`s, two `LevelMeterComponent`s
   (left/right), and one `GoniometerComponent` with the input/output overlaid as two
   colours in the same circle (green/blue) via a new `setSecondarySeries()` -- kept
   generic in `shared/metering/`, so `StereoAnalyzer`'s single-series goniometer is
   unaffected.
2. ✅ Core infrastructure:
   - `StereoAlgorithm` interface (`algorithms/StereoAlgorithm.h`), with `getName()`,
     `isMonoSafe()` and `getLatencySamples()` as specified.
   - Algorithm switching with an equal-power (cos/sin) crossfade, 30 ms.
   - Width (0–200 %) implemented. Mix, Output gain, Bass mono (as a separate control),
     Auto gain and Bypass deferred to Phase 4 -- not needed yet to exercise the switch.
   - Bus layout: stereo→stereo only so far (mono→stereo deferred to Phase 4; a mono
     buffer is currently passed through unprocessed as a safety guard).
3. ✅ **Algorithm 2.1**, split into two variants specifically so switching between them
   is audible: `MSWidthBroadband` (plain broadband width) and `MSWidthFiltered` (the
   same control with the side signal high-pass filtered first, forcing bass mono, then
   high-shelved for "air" -- the side-shelf part). Bass Cutoff and High Shelf frequency
   are adjustable via two aux knobs that flank Width and are relabelled/enabled per
   algorithm (`StereoAlgorithm::getAuxLeftInfo()`/`getAuxRightInfo()`); the shelf's gain
   is a fixed +3 dB constant for this first version, not yet its own parameter. A "?"
   button next to the algorithm selector shows each algorithm's description and a
   citation to a written source.
4. ✅ Null test: width = 100 % gives output = input for `MSWidthBroadband`
   (-144 dBFS, C++/JUCE only so far -- no Python reference for this algorithm yet).
   `MSWidthFiltered` deliberately does *not* pass this test (see the docs page for why).
5. ✅ Rendered the test signal corpus (7 signals: pink noise, panned speech, speech with
   synthetic reverb, a small mix, and three sample-based files including dual-mono
   speech) through the real C++ algorithm classes via a new headless console tool,
   `tools/widener_render` (same pattern as `tools/meter_crosscheck`), and measured the
   results with `python/stereo_eval.report` via a new `python/evaluate_widener_plugin.py`
   -- results and one plot per signal in `python/results/widener_plugin/` (gitignored,
   regenerate with that script). Width scaling matched theory exactly (dS-M = +3.5 /
   +6.0 dB at 150 % / 200 %, i.e. 20·log10(1.5) / 20·log10(2.0)), mono-sum colouration
   was exactly 0.0 dB everywhere (M/S width's mono-compatibility-by-construction,
   confirmed on real program material, not just synthetic signals), and the per-octave
   correlation plot for `MSWidthFiltered` visibly shows the bass staying correlated
   below the Bass Cutoff and decorrelating above it, exactly as designed.

### Phase 4 – Settings, utilities, latency
1. **Global ini file** using `juce::PropertiesFile` (in the user's application-data
   folder, e.g. `~/.config/StereoWidener/StereoWidener.settings` on Linux). It stores:
   - profile (Mastering / Creative)
   - last used state, used as the default for new instances
   - GUI size and meter options (integration time, goniometer persistence)
   The state of each instance is still saved in the DAW project as usual. The ini file
   only provides the defaults and user preferences.
2. Utilities (2.13 and 2.2): mono, L/R swap, polarity invert, rotation, balance, mono
   check (listen to L+R), and solo side.
3. Latency: each mode reports its latency with `setLatencySamples()` when it is
   selected, and the GUI shows the current latency in ms. If a mode switch during
   playback turns out to be a problem in hosts, add an option "constant latency"
   (all modes padded to the maximum).

### Phase 5 – Algorithms 2–5 (v1)
Order: **2.4 comb → 2.7 multiband → 2.5 allpass/velvet → 2.3 Haas**. The mono-safe modes
come first. For each algorithm:
1. Python reference in `python/algorithms/`, evaluation report, and choice of default
   parameters and ranges.
2. C++ class in `StereoWidener/algorithms/`, plus a null test or reference test against
   Python.
3. A documentation page in `docs/algorithms/` (theory, block diagram, parameters,
   evaluation plots).
4. For creative modes: the "not mono-safe" badge and a mono-check hint in the GUI.

### Phase 6 – v1 release
1. GUI: profile switch, mode selector (filtered by profile), macro width, parameter panel
   for the selected mode, in/out meters, latency display.
2. Auto gain (calibrated with the level measurements from Phase 1).
3. Factory presets for both profiles.
4. pluginval (strictness 10), tests in Reaper and AudioPluginHost, CPU check.
5. README, user documentation, versioning.

### Phase 7 – v2 and later
1. **Crosstalk cancellation (2.10)**, the loudspeaker focus. Parameters: speaker angle,
   listener distance, regularisation (limit the bass boost of S). Evaluate with the IACC
   of the loudspeaker model.
2. STFT panning expansion / primary–ambient decomposition (2.8), reusing the OutOfPhase
   WOLA structure and `TGMStaticLib/FFT`.
3. PCA rotation (2.9), micro-pitch (2.11), spectral interleaving (2.6).
4. Later: **headphone version** (crossfeed, and optionally binaural), with a clear GUI note
   saying which modes are meant for loudspeakers and which for headphones.

---

## 5. Code Rules (teaching and professional)

- **One algorithm = one small class** (`.h/.cpp`). It has no GUI and no JUCE dependency
  beyond basic types, so it can be read and tested in isolation.
- **File header:** a short description of the principle, the key formula, and references
  (as in `Stereoids/allpass.h`).
- **Readable before clever:** plain loops, clear names (`sideGain`, `bassMonoFreq_Hz`),
  units in the names, and no template tricks.
- **The Python reference sits next to each C++ class** (same name, same parameters). The
  test compares both.
- **Real-time safety:** no allocation, locks, or file I/O in `process()`. Everything is
  prepared in `prepare()`, and all parameters are smoothed.
- **Consistency with the existing environment:** AdvancedAudioTemplate conventions
  (`SynchronBlockProcessor`, `PluginSettings.h`, preset handler), and `TGMStaticLib`
  for filters, FFT and smoothing.
- Each algorithm documentation page can serve directly as the script for a video or
  lecture unit.

---

## 6. Next Concrete Steps

1. Phase 0: git repository, folder structure, copy the samples, `test_signals.md`.
2. Phase 1: `generate_test_signals.py` and the `stereo_eval` package, validated with
   known signals.
3. Phase 2: create StereoAnalyzer from the template and start with the correlation meter
   and the goniometer.

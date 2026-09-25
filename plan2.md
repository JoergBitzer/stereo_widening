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

### Phase 4 – Settings, utilities, latency — settings and utilities done
Details and verification: `docs/algorithms/phase4_settings.md`.
1. ✅ **Global settings file** -- format changed from the original plan's
   `juce::PropertiesFile` (XML/binary only) to **JSON** (user preference: "a simple
   YAML or TOML format. JSON is also OK"; JUCE has no built-in YAML/TOML parser, and
   `juce::JSON` needs no extra dependency). `StereoWidener/GlobalSettings.h`/`.cpp`
   reads/creates `~/.config/StereoWidener/settings.json`, storing:
   - the side high-shelf's gain (`MSWidthFiltered`, previously a fixed 3 dB constant)
   - ~~last used state (every parameter value), used as the default for a brand new
     instance~~ -- **removed in the Phase 5 comb follow-up**: it made a GUI knob's
     double-click reset go to whatever was last dialled in rather than to a neutral
     value; every parameter default is now a fixed, compiled-in, neutral value instead,
     and the `init` preset covers "restore my last settings" (see
     `docs/algorithms/phase5_comb.md`)
   - GUI size ("project always wins once it exists" behaviour, unaffected by the above)
   - meter options (integration time, peak hold, peak decay) -- defaults only, since
     StereoWidener has no per-project override mechanism for these yet (unlike
     StereoAnalyzer's Settings popup)
   - profile (Mastering / Creative): deliberately **skipped**, per the user's own
     stated uncertainty about whether it belongs in this file at all
   The state of each instance is still saved in the DAW project as usual. The settings
   file only provides the defaults and user preferences.
2. ✅ Utilities (2.13 and 2.2): mono, L/R swap, polarity invert, rotation, balance, mono
   check (listen to L+R), and solo side. New `UtilityProcessor`, applied after the
   selected width algorithm regardless of which one is active. "Mono (sum)" and "mono
   check" consolidated into one 3-way Monitor selector (Normal / Mono Check / Solo
   Side) with "solo side", since they are the same DSP operation.
3. Latency: each mode reports its latency with `setLatencySamples()` when it is
   selected, and the GUI shows the current latency in ms. If a mode switch during
   playback turns out to be a problem in hosts, add an option "constant latency"
   (all modes padded to the maximum). Not started (`StereoAlgorithm::getLatencySamples()`
   exists but isn't wired to `setLatencySamples()` yet -- not needed while every
   algorithm reports 0).

### Phase 5 – Algorithms 2–5 (v1)
Order: **2.4 comb → 2.5 allpass/velvet → 2.7 multiband → 2.12 early reflections → 2.3
Haas** (changed twice from the original comb → multiband → allpass/velvet → Haas
order: allpass/velvet moved ahead of multiband, and 2.12 early-reflection/room
widening inserted ahead of 2.3 Haas per explicit request -- 2.3 is deferred, not
dropped). The mono-safe modes come first.

Every algorithm goes through the same four steps; each subsection below tracks them:
1. Python reference in `python/algorithms/`, evaluation report, and choice of default
   parameters and ranges.
2. C++ class in `StereoWidener/algorithms/`, plus a null test or reference test against
   Python.
3. A documentation page in `docs/algorithms/` (theory, block diagram, parameters,
   evaluation plots).
4. For creative modes: the "not mono-safe" badge and a mono-check hint in the GUI.

#### 2.4 Complementary comb -- done (v0.1.3)
1. ✅ Python reference: `python/algorithms/comb.py`, `python/evaluate_comb.py`.
2. ✅ C++ class: `StereoWidener/algorithms/ComplementaryComb.{h,cpp}`; cross-checked
   against the real plugin DSP via `tools/widener_render` +
   `python/evaluate_widener_plugin.py`, and directly against the Python reference via
   the new `python/crosscheck_comb.py` (PASS, see the doc page).
3. ✅ Doc page: [phase5_comb.md](docs/algorithms/phase5_comb.md).
4. N/A -- mono-safe by construction (`L'+R' = 2M` always), no badge/hint needed.

Two user-facing parameters (Delay, Gain) plus the shared Width knob, per explicit
request to minimise controls to "2 + Width"; the crossover frequency is a
`GlobalSettings` default instead of a third knob. Required a new dynamic
aux-knob-rebinding mechanism in `StereoWidenerGUI` (the two aux knob widgets now
represent different parameters depending on the active algorithm, rather than each
algorithm getting its own dedicated knob pair). A follow-up in the same doc page also
removed the "last used state" mechanism from `GlobalSettings` (it made a knob's
double-click reset go to whatever was last dialled in instead of a neutral value) --
every parameter default is now the neutral/pass-through value for its algorithm
(v0.1.4). A second follow-up fixed audible "zipper" noise on Delay changes
(`juce::dsp::DelayLine::setDelay()` was stepping the read position instantly); fixed by
ramping it with `juce::SmoothedValue<float>` instead -- considered and rejected reusing
a more complex hand-written time-variant delay-line class (designed for a different,
N-channel feedback-delay use case, and unsafe with a single mono channel) in favour of
this much smaller fix reusing only already-verified JUCE building blocks (v0.1.8).

#### 2.5 Allpass decorrelation -- done (v0.1.5)
1. ✅ Python reference: `python/algorithms/allpass_decorrelation.py`,
   `python/evaluate_allpass.py`. Asserts `amount = 0` is an exact bypass; confirmed the
   mono sum is genuinely non-flat once `amount > 0` (unlike comb), and that
   `spread = 0` still colours the mono sum despite leaving the side signal untouched
   bit-for-bit (see the doc page's derivation).
2. ✅ C++ class: `StereoWidener/algorithms/AllpassDecorrelation.{h,cpp}`; cross-checked
   against the real plugin DSP via `tools/widener_render` +
   `python/evaluate_widener_plugin.py`, and directly against the Python reference via
   the new `python/crosscheck_allpass.py` (PASS, diffs ~0.00 -- no delay-line
   interpolation difference this time, unlike comb's cross-check).
3. ✅ Doc page: [phase5_allpass.md](docs/algorithms/phase5_allpass.md).
4. ✅ "Not mono-safe" badge + mono-check hint: `StereoWidenerGUI` now shows an orange
   warning below the algorithm selector whenever the active algorithm's
   `isMonoSafe()` is false (first algorithm to trigger it), pointing at the existing
   Monitor "Mono Check (L+R)" utility. `g_minGuiSize_y` grown 655 -> 685 to fit the new
   row.

Two parameters (Amount, Spread) plus the shared Width knob, both defaulting to their
neutral values (Amount 0 % = exact bypass; Spread has no neutral value of its own,
inert whenever Amount = 0) per the same convention established in 2.4's follow-up.
Velvet-noise decorrelators (planing.md's other suggested technique under this same
entry) were not implemented -- the allpass-cascade approach alone already satisfies
the algorithm's spec and its own "con"; velvet noise would need a different
(convolution-based) engine and is left for a future revisit if the cascade approach
turns out insufficient in practice. Next: 2.7 multiband.

#### 2.7 Multiband width -- done (v0.1.7)
1. ✅ Python reference: `python/algorithms/multiband_width.py`,
   `python/evaluate_multiband.py`. Found and fixed a real reconstruction bug before
   any C++ was written: a naive LR4 crossover tree does not sum flat (~0.5-0.6 dB
   spurious mono-sum colouration at every setting); fixed with allpass
   phase-compensation (down to 0.01-0.04 dB) -- see the doc page for the derivation.
2. ✅ C++ class: `StereoWidener/algorithms/MultibandWidth.{h,cpp}`; cross-checked
   against the real plugin DSP via `tools/widener_render` +
   `python/evaluate_widener_plugin.py`, and directly via the new
   `python/crosscheck_multiband.py` (PASS, diffs 0.000 to printed precision -- no
   delay-line interpolation involved, same as allpass's own cross-check).
3. ✅ Doc page: [phase5_multiband.md](docs/algorithms/phase5_multiband.md).
4. N/A -- mono-safe by construction (M untouched), no badge/hint needed.

The one algorithm so far that doesn't fit "2 + Width" (needs 6 parameters: 3 crossover
frequencies + 3 band widths, band 1 always forced mono). Discussed the design with the
user before implementing (per their explicit request) and agreed: a generic knob grid
rather than a dedicated band-split-editor widget for v1, and a plugin window that
resizes per algorithm rather than always reserving space for the largest case. Required
a new, additive parameter mechanism (`StereoAlgorithmParams::multi`,
`StereoAlgorithm::getNumMultiParams()`/`getMultiParamInfo()`, all default-implemented
so the four existing algorithms needed zero changes) alongside the existing
`auxLeft`/`auxRight` pair, and new GUI plumbing for the window resize
(`StereoWidenerGUI::getRequiredContentHeight()`/`onActiveAlgorithmChanged`,
`PluginEditor.cpp`'s `updateWindowSizeForActiveAlgorithm()` -- which also fixed a
latent preset-bar-height bug that only mattered once the aspect ratio could change).
Two GUI bugs found and fixed during offline verification: crossover knobs
collapsing to their range minimum on startup (a knob-clamping guard firing during
initial binding, before all three knobs had their real values) and a decimal-place
display bug (see the doc page). Next: 2.12 early reflections (2.3 Haas deferred, see
below).

#### 2.12 Early reflections / room widening -- done (v0.1.9)
1. ✅ Python reference: `python/algorithms/early_reflections.py`,
   `python/evaluate_early_reflections.py`. Asserts `amount = 0` is an exact bypass;
   confirmed the mono sum is genuinely non-flat once `amount > 0` and genuine width
   from dual-mono input, same as allpass decorrelation.
2. ✅ C++ class: `StereoWidener/algorithms/EarlyReflections.{h,cpp}`; cross-checked
   against the real plugin DSP via `tools/widener_render` +
   `python/evaluate_widener_plugin.py`, and directly via the new
   `python/crosscheck_early_reflections.py` (PASS, diffs 0.000 to printed precision --
   the tightest cross-check so far). Found and fixed **two** real bugs, both in
   `juce::dsp::DelayLine::popSample()`'s `updateReadPointer` flag (this algorithm reads
   one shared delay line ten times per pushed sample, unlike comb's one-pop-per-push):
   first, the cross-check caught leaving it at its default `true` on every call, which
   let the read cursor free-run ahead of the write cursor and corrupt every tap's
   delay. The first fix (`false` on every call) overcorrected -- it froze the read
   cursor entirely, so every tap read one fixed buffer slot refreshed only once per
   buffer revolution, audible as periodic crackle at Amount > 0 and Width > 0 (reported
   live by the user; invisible to the cross-check's aggregate statistics, since the
   stale samples read back were still real, correlated audio). Correct fix:
   `updateReadPointer=true` on only the temporally last of the ten `popSample()` calls
   per sample, `false` on the rest -- see the doc page for the full account.
3. ✅ Doc page: [phase5_early_reflections.md](docs/algorithms/phase5_early_reflections.md).
4. N/A -- badge/hint mechanism already generic since 2.5, no new GUI code needed;
   confirmed via a new throwaway `WidenerGuiSnapshot` offline-render tool that it shows
   correctly for this algorithm.

Two user-facing parameters (Amount, Room Size) plus the shared Width knob, decided by
weighing the user's own three-knob suggestion (`nr_of_reflections`, `RoomSize`,
`pre-delay`) against the "2 + Width"/neutral-default conventions: Amount (0 % neutral
bypass, not originally suggested but required to preserve the project's
every-algorithm-has-an-exact-bypass invariant), Room Size (50 % default, no neutral
value of its own), with pre-delay pushed to a `GlobalSettings` default (mirroring
comb's `crossoverHz`) and reflection count fixed at a compiled-in constant (mirroring
allpass's fixed cascade-stage count). Next: 2.3 Haas.

#### 2.3 Haas / precedence-effect delay -- deferred
planing.md 2.3: a short inter-channel delay (creative, not mono-safe like 2.5). Skipped
ahead of 2.12 (early reflections) per explicit request; all four steps still not
started.

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

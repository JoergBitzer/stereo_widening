# Stereo Widening Plugin – Planning

Goal: a JUCE plugin that widens, narrows or otherwise changes the stereo image of a
signal. It offers several algorithms that the user can switch between.

Starting point: the existing `AudioDev` environment (top-level `CMakeLists.txt` from
AudioDevOrga, `JUCE/`, `Libs/TGMStaticLib`) and the **AdvancedAudioTemplate**
(`SynchronBlockProcessor`, preset handler, resizable GUI, versioning).

---

## 1. Notation

- Input: `L[n]`, `R[n]`. Output: `L'[n]`, `R'[n]`.
- Mid/Side: `M = (L + R)/2`, `S = (L − R)/2`, and back: `L = M + S`, `R = M − S`.
- **Mono compatibility**: the mono sum `L' + R'` should sound like `L + R` (no comb
  filtering, no cancellation). Check this for every algorithm.
- **Correlation**: `ρ = E{L·R} / sqrt(E{L²}·E{R²})`. `ρ = 1` is mono, `ρ ≈ 0` is wide or
  decorrelated, and `ρ < 0` means phase problems. Every widener lowers `ρ`.

---

## 2. Algorithm Catalogue

Each entry lists the principle, its parameters, and its pros and cons. The **Group**
column in the table in section 3 says whether an algorithm works on existing stereo
(**S**), creates stereo from mono (**P** = pseudo-stereo), or both.

### 2.1 Mid/Side Width (classic)
- `S' = w · S`, `M' = m · M` (often with `m` chosen to keep the level constant, e.g.
  `m = 2/(1+w)`, or constant-power scaling).
- `w = 0` is mono, `w = 1` is unchanged, `w > 1` is wider, and `w → ∞` leaves only the side signal.
- Equivalent 2×2 matrix form:
  `L' = a·L + b·R`, `R' = b·L + a·R` with `a = (m+w)/2`, `b = (m−w)/2`.
- **Pros:** trivial, zero latency, fully mono-compatible (the mono sum depends on `M` only).
- **Cons:** does nothing to mono input. At high `w` it raises the level of reverb and
  noise and makes a "hollow" centre.
- **Variants:**
  - **Bass mono:** high-pass the side channel (`S` → HPF at 80–200 Hz). This is
    standard in mastering.
  - **Frequency-dependent M/S (shelving):** a shelf or peak EQ in `S`, for example
    boosting the side signal above 2 kHz only.
  - **Blumlein shuffler:** boost `S` at low frequencies (below ~700 Hz) to correct
    the image width of coincident-mic and loudspeaker playback (Blumlein, Gerzon).

### 2.2 Stereo Rotation / General 2×2 Matrix
- Rotate the signal vector in the L/R plane:
  `[L';R'] = [cos φ, −sin φ; sin φ, cos φ]·[L;R]`. On the goniometer this is a rotation.
  It moves the whole image, not only its width.
- Together with M/S width this gives a complete "image editor": width, rotation,
  balance, and polarity inversion of one channel.
- **Pros:** cheap, useful as a utility. **Cons:** not a widener on its own.

### 2.3 Haas / Precedence Effect Delay
- Delay one channel by 1–30 ms (sometimes with a small gain difference). The source
  is perceived toward the earlier channel, and the image seems wider.
- Better variant: delay only the **side** part, or only a band-limited part
  (for example above 1 kHz).
- **Pros:** strong effect, and works on mono input (P).
- **Cons:** **not mono-compatible**, because `L+R` gives comb filtering
  (`1 + z^{-D}`). It can collapse badly on phones and club PAs.

### 2.4 Complementary Comb Filters (Lauridsen / Schroeder pseudo-stereo)
- `L' = x + g·x[n−D]`, `R' = x − g·x[n−D]` with `D` ≈ 5–20 ms and `g` ≈ 0.3–0.7.
- The combs are complementary: `L' + R' = 2x`. The mono sum is **perfectly clean**.
- Can be applied to `M` and added to the existing `S`: `S' = S + g·M[n−D]`.
- **Pros:** mono-compatible pseudo-stereo, and cheap.
- **Cons:** audible "phasiness" or combing in each channel on its own, especially with
  headphones. Better with several delays or a crossover (apply only above ~300 Hz).

### 2.5 Allpass Decorrelation
- Filter L and R (or a mono source feeding both) through **different allpass
  cascades**. Magnitude is unchanged and the phase differs, so the signals decorrelate.
- Types: cascades of 2nd-order allpasses (`Stereoids/allpass.h`, Regalia/Mitra
  lattice structure), Schroeder allpasses with longer delays, and
  **velvet-noise decorrelators** (Alary, Politis, Välimäki 2017). Velvet noise is
  efficient and gives good results for transient signals.
- Common approach: `S' = S + k·AP(M)`. This creates side content from the mid signal.
- **Pros:** the magnitude spectrum of each channel is preserved, and works on mono
  input (P).
- **Cons:** the mono sum is *not* flat (phase differences between L and R cause
  cancellation). Transient smearing with long allpasses. Parameter design
  (pole placement) is a small research task of its own.

### 2.6 Spectral Panning / Frequency Interleaving
- Split the signal into many bands (filter bank or FIR) and pan bands alternately
  left and right, for example with complementary filters
  `H_L(f) + H_R(f) = 1` where `H_L` has "odd" bands and `H_R` has "even" bands.
- This is a generalisation of 2.4. With complementary filters it stays mono-compatible.
- **Pros:** mono-compatible pseudo-stereo, controllable density.
- **Cons:** timbral colouration in each channel; the band layout needs tuning.

### 2.7 Multiband Width
- Crossover (Linkwitz-Riley LR4, 3–4 bands, which sums to allpass or flat) and an
  M/S width per band. Bass mono comes built in.
- Optional: per-band decorrelation (2.5) instead of M/S only.
- **Pros:** the tool most used in practice (mastering). Very controllable.
- **Cons:** more GUI and parameters. Crossover phase must be handled (LR sums to an
  allpass, or use linear phase with latency).

### 2.8 STFT-Based Panning Expansion (source re-panning)
- Per time-frequency bin, estimate the panning index, for example as in
  Avendano & Jot (2004):
  `ψ(k) = 1 − 2|X_L X_R*| / (|X_L|² + |X_R|²)` with the sign from `|X_L| > |X_R|`.
  Alternatively, use `atan(|X_R|/|X_L|)`.
- Map the pan position through a curve `ψ' = f(ψ)` (for example `f` expands:
  sources at 30 % move to 60 %; the centre stays in the centre). Then re-synthesise with
  new gains per bin (magnitude re-panning with the original phases).
- Extension: **primary–ambient decomposition** (coherence based, or PCA per band).
  Keep the direct sound, decorrelate or widen the ambient part only.
- **Pros:** the most "intelligent" approach. Sources stay sharp while the image
  widens. The centre (vocals) can be protected.
- **Cons:** latency (block size, e.g. 1024–2048 at 48 kHz gives 20–40 ms), musical
  noise or artefacts, and more CPU. `TGMStaticLib/FFT.h` and the WOLA experience
  from OutOfPhase can be reused.

### 2.9 PCA / Adaptive Rotation
- Estimate the 2×2 covariance matrix (recursively, with a time constant). Its
  principal axis is the dominant direction. Rotate so the principal axis becomes
  the centre, widen the orthogonal part, and rotate back.
- Also usable per band (combined with 2.7) or as a simple "auto-centre".
- **Pros:** adaptive, cheap in the time domain. **Cons:** can pump if the time
  constants are wrong.

### 2.10 Crosstalk Cancellation / "Beyond the speakers" (loudspeaker 3D)
- Loudspeaker playback: part of the left speaker reaches the right ear (delayed by
  about 0.1–0.3 ms and low-pass filtered by the head shadow). Subtract an estimate of this
  crosstalk: `L' = L − g·LP(R[n−d])`, `R' = R − g·LP(L[n−d])`. A recursive
  (Atal-Schroeder / Cooper-Bauer) structure also cancels the crosstalk of the
  cancellation signal.
- Simplified "shuffler" form: done in the M/S domain with filters `1/(1+C)` for M
  and `1/(1−C)` for S.
- **Pros:** a width outside the speaker base can be perceived.
- **Cons:** only works for loudspeakers with a small sweet spot. Wrong for headphones.
  Bass boost of S must be limited (regularisation).
- **Opposite direction for headphones: crossfeed (Bauer, 1961).** This *narrows* the
  image in a natural way. It is a good extra mode ("Headphone mode").

### 2.11 Micro-Pitch / Chorus Doubler
- L and R get slightly different short, modulated delays (5–30 ms, LFO) or fixed
  detune (±5–15 cent, using a delay-line pitch shifter). This is the classic
  "Dimension D" or "micro shift" effect (Eventide H3000 style).
- **Pros:** very popular sound, works on mono (P), and sounds "lush".
- **Cons:** changes the sound (modulation, chorus). Mono sum shows comb and flanging
  artefacts. This is an effect more than a neutral tool.

### 2.12 Early-Reflection / Room Widening
- Add a few short, *decorrelated* early reflections (different for L and R,
  within 5–40 ms, low level). The effect is apparent source width (ASW), known from
  room acoustics.
- **Pros:** natural. **Cons:** adds room, and overlaps with a reverb plugin.

### 2.13 Utilities (not algorithms, but should be in the plugin)
- Mono (sum), L/R swap, polarity invert L/R, balance, and "mono check" (listen to
  `L+R`) as well as "solo side" (listen to `S`).

---

## 3. Comparison and Selection

| # | Algorithm | Group | Mono-compat. | Latency | CPU | Effort |
|---|-----------|-------|--------------|---------|-----|--------|
| 2.1 | M/S width (+ bass mono, shelf) | S | ++ | 0 | – | low |
| 2.2 | Rotation / 2×2 matrix | S | ++ | 0 | – | low |
| 2.3 | Haas delay | S+P | – – | 0 | – | low |
| 2.4 | Complementary comb | S+P | ++ | 0 | – | low |
| 2.5 | Allpass / velvet decorrelation | S+P | o | 0 | low | medium |
| 2.6 | Spectral interleaving | P | + | 0 (IIR) / FIR | low–med | medium |
| 2.7 | Multiband width | S | + | 0 (LR) | low | medium |
| 2.8 | STFT panning expansion | S | + | 20–40 ms | high | high |
| 2.9 | PCA rotation | S | + | 0 | low | medium |
| 2.10 | Crosstalk cancellation / crossfeed | S | o | 0 | low | medium |
| 2.11 | Micro-pitch / chorus | S+P | – | ~0 | low | medium |
| 2.12 | Early reflections | S+P | o | 0 | low | medium |

**Suggested scope**

- **Version 1 (5 modes):**
  1. M/S width with bass mono and side shelf (2.1)
  2. Haas (2.3), with a mono-compatibility warning in the GUI
  3. Complementary comb (2.4), with a high-pass so it is applied only above a crossover
     frequency
  4. Allpass decorrelator (2.5), reusing Stereoids
  5. Multiband width (2.7)
- **Version 2:** STFT panning expansion (2.8), crosstalk cancellation and crossfeed
  (2.10), micro-pitch (2.11), PCA (2.9).
- Utilities (2.13) and metering are in v1.

---

## 4. Plugin Architecture

### 4.1 Algorithm interface (strategy pattern)
```cpp
class StereoAlgorithm
{
public:
    virtual ~StereoAlgorithm() = default;
    virtual void prepare (double fs, int maxBlockSize) = 0;
    virtual void reset() = 0;
    virtual void process (float* left, float* right, int numSamples) = 0; // in-place
    virtual int  getLatencySamples() const { return 0; }
    virtual void setWidth (float width) = 0;   // common macro control 0..2 (0..200 %)
    // algorithm-specific parameters are read from APVTS inside each class
};
```
- One class per algorithm, in its own `.h/.cpp` file. They can be tested in a
  command-line host independent of the GUI.
- `StereoWidener` (the "YourPluginName" class of the template) owns all algorithm
  instances. It is created in `prepare`, so there is **no allocation in the audio thread**.

### 4.2 Switching algorithms
- `AudioParameterChoice "Algorithm"`. It must be automatable and saved in presets.
- **Click-free switch:** run the old and new algorithms in parallel for 20–50 ms and
  crossfade (equal power), then `reset()` the old one.
- **Latency:** the host PDC should not jump. Two options:
  (a) report the maximum latency of all algorithms and pad the others with a
  delay (recommended once the STFT mode exists), or
  (b) call `setLatencySamples()` on switch (hosts handle this differently).

### 4.3 Common parameters (all modes)
- Width (0–200 %), Dry/Wet mix, Output gain, Bass-mono frequency (off, 40–300 Hz),
  **Auto gain** (keep loudness or RMS constant while widening), Bypass.
- Algorithm-specific parameters (delay, comb gain, number of allpasses, crossovers,
  …) are shown or hidden depending on the selected mode.
- All continuous parameters are smoothed (`TGMStaticLib/SmoothParameter.h`).
- Width stays one "macro" parameter, so the user can switch modes and compare at the
  same setting.

### 4.4 Metering / visualisation (important for this kind of plugin)
- **Goniometer / vectorscope** (Lissajous plot of M vs. S), for input and output.
- **Correlation meter** (−1 … +1), optionally per band.
- M and S level meters.
- Optional: a spectrum of `L+R` (in vs. out) to show mono-compatibility problems
  directly.
- Implementation: a lock-free FIFO from the audio thread to the GUI and a timer-based
  repaint.

### 4.5 Reusable code already on disk
- `AdvancedAudioTemplate`: project skeleton, presets, GUI scaling.
- `Libs/TGMStaticLib`: `FFT`, `FirstOrderFilter`, `SOSFilter`,
  `FreeOrderLowHighpassFilter` (crossovers), `SmoothParameter`, `PeakFilter` (side
  shelf and peak).
- `Stereoids/allpass.h`: 2nd-order allpass (lattice) for the decorrelator.
- `OutOfPhase`: STFT/WOLA structure for the v2 STFT mode.

---

## 5. Evaluation / Testing

- **Test signals:** mono pink noise, a sine sweep, hard-panned or partly panned
  sources, drums (transients), speech, full mixes, and a mono source with reverb.
- **Objective measures (Python):**
  - correlation `ρ` in and out (broadband and per 1/3 octave)
  - mono-sum spectrum `|L'+R'|` vs. `|L+R|` (colouration in dB)
  - level change (RMS or LUFS) for the auto-gain calibration
  - IACC with a binaural model or HRTFs, for loudspeaker playback
- **Unit tests** per algorithm: width = 1 must give a null test against the input
  (except for algorithms that are intentionally different). Test mono in and out,
  and check that there are no NaN/Inf and no denormals.
- **Listening:** loudspeakers *and* headphones, plus a mono check. Optional small
  MUSHRA-style comparison of the modes (a good student project).
- **Hosts:** Reaper, pluginval (`pluginval --strictness-level 10`), and AudioPluginHost
  from JUCE.

---

## 6. Roadmap (how to start)

Suggested Changes: 
1) Switch Metering with Algorithm 1. It makes sense to start with an analysis plugin (independent Plugin)
We use this plugin as the starting point for the stereo widening plugin. The first algorithm is the M/S width with bass mono and side shelf (2.1). The metering is important to visualize the effect of the algorithm.
2) Lets generate some test signals (mono pink noise, sine sweep, hard-panned sources, drums, speech, full mixes, and a mono source with reverb) and implement the objective measures in Python (correlation, mono-sum spectrum, level change, IACC). This will help us to evaluate the performance of the algorithm and ensure that it meets our requirements. For samples, copy a few examples (Speech, Adlibs or singing) from /home/bitzer/Music/samples/ to a subdirectory /test_signals/ in the project. We can use these samples for testing and evaluation. exclude the samples from the repository.

1. **Python prototyping** (same workflow as OutOfPhase). Put the notebooks in `python/`.
   - Implement 2.1, 2.3, 2.4, 2.5 and 2.7 as functions `(L, R, params) → (L', R')`.
   - Build the evaluation script from section 5 (correlation, mono-sum spectrum).
   - Listen and choose default parameters and parameter ranges.
2. **Project setup:** copy AdvancedAudioTemplate to `stereo_widening`, rename to
   `StereoWidener`, add it to the top-level `CMakeLists.txt`, build an empty
   pass-through plugin, and check it in a DAW.
3. **Core infrastructure:** `StereoAlgorithm` interface, algorithm switching with
   crossfade, common parameters, utilities (mono, swap, polarity, solo S), and bypass.
4. **Algorithm 1: M/S width + bass mono.** This is the reference for everything else.
   Write a C++ vs. Python null test.
5. **Metering:** goniometer and correlation meter. Build them early, because they help
   during all later development.
6. **Algorithms 2–5** one after another, each with a Python reference and a test.
7. **GUI:** mode selector, macro width, mode-dependent parameter panel, and meters.
8. **Presets, auto gain, pluginval, documentation (README), and versioning.**
9. **v2:** STFT panning expansion, crosstalk cancellation and crossfeed, micro-pitch,
   PCA.

---

## 7. Open Questions

- Target group: mastering or mixing tool (neutral, mono-safe), or creative effect (Haas,
  chorus)? This decides the default modes and GUI style.
  Answer: both. Users can choose and the final state is saved in a ini file
- Should mono input be supported explicitly (mono → stereo bus layout)?
  Answer: yes, but the plugin should not force mono input. It should work on stereo
  input as default.
- Is latency acceptable (for the STFT mode, and for linear-phase crossovers)?
  Answer: yes
- Loudspeaker or headphone focus (relevant for 2.10)?
  Answer: definietly Loudspeakers, headphones in a later version with clearly stated information at the GUI.
- Should it be used as a teaching example (video series or course)? If so, the code
  should be simple and each algorithm easy to read in isolation.
  Answer: yes, the code should be easy to read and understand. The plugin should be a teaching example for students and young engineers, but still be a professional tool for mastering and mixing engineers.

---

## 8. References

- M. Gerzon, "Stereo Shuffling: New Approach – Old Technique", Studio Sound, 1986.
- A. D. Blumlein, British Patent 394,325 (1931), shuffler and M/S.
- M. R. Schroeder, "An artificial stereophonic effect obtained from a single audio
  signal", JAES 6(2), 1958.
- H. Lauridsen, pseudo-stereo using complementary comb filters, 1954.
- B. S. Atal, M. R. Schroeder, "Apparent sound source translator", US Patent 3,236,949
  (1966), crosstalk cancellation.
- B. B. Bauer, "Stereophonic Earphones and Binaural Loudspeakers", JAES 9(2), 1961.
- P. A. Regalia, S. K. Mitra, P. P. Vaidyanathan, "The digital all-pass filter:
  A versatile signal processing building block", Proc. IEEE 76, 1988.
- G. Kendall, "The Decorrelation of Audio Signals and Its Impact on Spatial Imagery",
  CMJ 19(4), 1995.
- B. Alary, A. Politis, V. Välimäki, "Velvet-Noise Decorrelator", DAFx 2017.
- C. Avendano, J.-M. Jot, "A Frequency-Domain Approach to Multichannel Upmix",
  JAES 52(7/8), 2004.
- C. Faller, "Multiple-Loudspeaker Playback of Stereo Signals", JAES 54(11), 2006.
- U. Zölzer (ed.), *DAFX – Digital Audio Effects*, 2nd ed., Wiley 2011 (spatial
  effects chapter).

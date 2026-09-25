# Algorithm 2.8 prototype -- STFT panning expansion / source re-panning

A Python-only feasibility check, requested explicitly instead of a full Phase 5-style
implementation: planing.md flags this as the most "intelligent" but also the most
complex/expensive technique in the catalogue ("more redesign... worth the effort?"),
so this is a rough, honest prototype meant to be *listened to* and judged before
committing to a C++ implementation -- not a polished algorithm, and not held to this
project's usual "2 + Width" / exact-bypass / mono-safety bar.

## What was built

`python/algorithms/source_repanning.py` implements planing.md 2.8's base technique
(the Avendano & Jot panning-index formula), via `scipy.signal.stft`/`istft`
(`n_fft=2048`, 75% overlap):

1. Per STFT bin, estimate a panning index `psi(k) in [-1, 1]` from the two channels'
   complex spectra (+1 = hard left, -1 = hard right, 0 = centre/coherent-and-equal).
2. Remap it through an expansion curve, `psi' = clip(psi * expansion, -1, 1)`
   (planing.md's own example, "30% moves to 60%", is `expansion = 2.0`).
3. Re-synthesise with a constant-power pan law applied to `psi'`, redistributing each
   bin's *total* energy between channels while keeping each channel's own original
   phase.

**Explicitly NOT implemented**: planing.md's "extension" of primary-ambient
decomposition (coherence-based separation of direct vs. ambient content, meant to
protect a centred vocal from the same remap as everything else). That is a
materially different, more complex technique layered on top -- only worth building if
the base version below looks promising enough to justify it.

`python/evaluate_source_repanning.py` runs it on this project's usual 7-signal
corpus at four expansion settings (`identity` = 1.0, `mild` = 1.5, `moderate` = 2.0,
`strong` = 3.0) and, unlike every other `evaluate_*.py` in this project, **always**
writes the processed audio (not behind `--write-audio`) to
`test_signals/processed/source_repanning/` -- the point of this exercise is real
files to listen to. Numbers and plots are in `python/results/source_repanning/`.

## Findings

**"Identity" (`expansion = 1.0`, i.e. "no remap requested") is measurably NOT a
bypass**, confirming a limitation flagged in the module docstring before running
anything: the panning-index model assumes one coherent point source per bin, which
real material rarely is. For `speech_pan_L50` (a single hard-panned mono source),
`identity` already raises IACC from 0.89 (input) to 0.99 and visibly narrows the
goniometer image (`speech_pan_L50_identity.png`) -- re-synthesising from a magnitude
model of the pan discards information a real bypass would keep.

**The core mechanism works as intended for a genuine single panned source, but only
at a fairly aggressive setting.** Still `speech_pan_L50`, at `moderate`
(`expansion = 2.0`): the goniometer image visibly rotates wider
(`speech_pan_L50_moderate.png`), matching planing.md's own "30% moves to 60%"
description. But IACC per octave still sits *above* the input below ~1 kHz even at
this setting -- the net widening only shows up once the low-mid-frequency narrowing
bias from the identity-isn't-a-bypass problem is outweighed by the requested
expansion.

![speech_pan_L50, moderate (expansion=2.0): the output goniometer visibly rotates wider than the input, matching planing.md's own "30% moves to 60%" example -- but IACC per octave (bottom middle) still sits above the input below ~1 kHz](../../python/results/source_repanning/speech_pan_L50_moderate.png)

**On a realistic multi-source mix, the effect is the opposite of the goal.** For
`mix_small` at `moderate`, the output goniometer is if anything *narrower* than the
input, and IACC per octave sits above the input across almost the whole spectrum --
i.e. the algorithm measurably narrows the image on real program material instead of
widening it. Likely cause: with several simultaneous, differently-panned sources
sharing each bin, the single dominant per-bin panning index doesn't correspond to
any one real source's position, and remapping/expanding that blended estimate
doesn't reproduce "spread the mix wider" -- it just launders away some of the real
inter-channel structure that a magnitude-only, one-source-per-bin resynthesis can't
represent.

![mix_small, moderate: the output goniometer is if anything narrower than the input, and IACC per octave (bottom middle) sits above the input across almost the whole spectrum -- the opposite of the intended widening](../../python/results/source_repanning/mix_small_moderate.png)

**On already-decorrelated/ambient material, it does effectively nothing.** For
`noise_pink_rho050`, input and output curves are nearly indistinguishable at every
setting (`noise_pink_rho050_moderate.png`) -- neither harmful nor useful there.

**`mono_coloration_dB` stays small everywhere (0.00-0.31 dB across all
signals/settings)** -- this project's usual level-based metric does not surface this
algorithm's real problems, which are about *imaging* (does the image actually get
wider where intended?) and likely about audible artefacts from independent
bin-by-bin gain decisions varying frame to frame ("musical noise"/swishing), which
none of `stereo_eval`'s current measures are designed to catch. **Listening to the
produced files matters more than these numbers for this algorithm specifically.**

**Latency**: `n_fft = 2048` gives roughly 46 ms at 44.1 kHz, in planing.md's own
stated 20-40 ms ballpark (at the high end) -- a real cost for a real-time plugin,
independent of every finding above.

## Recommendation

The base technique, as implemented, does not deliver "sources stay sharp while the
image widens" on real (multi-source) program material -- planing.md's stated pro
for this algorithm. It does work in the narrow "single dominant panned source"
case, at the cost of measurable narrowing everywhere else in the spectrum/on other
material. Before investing in a C++ implementation, worth checking by ear whether:
- the low-mid-frequency narrowing bias (present even for a single clean source) is
  audible and objectionable on its own, and
- the "musical noise"/time-varying-gain artefact this class of technique is known
  for is present and how bad it is on `mix_small`/`mix_loop_let_it_be`.

If both are a problem, the primary-ambient decomposition extension (explicitly not
attempted here) is probably not a small tweak but a required redesign, not an
optional add-on -- which matches the suspicion that prompted this evaluation.

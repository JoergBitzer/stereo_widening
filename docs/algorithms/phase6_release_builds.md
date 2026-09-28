# Release builds at different optimisation levels (v0.1.32)

Plan: [plan2.md](../../plan2.md), Phase 6, 4 (pluginval, CPU check). Until now every
test ran on the Debug build (`-O0`, assertions on). This checks that optimised builds
work and sound the same. No code changes were needed.

## Builds

Six configurations of v0.1.32 (GCC 13.3, x86-64 Linux), each in its own build
folder: VST3, Standalone and `WidenerRender`.

| Name | CMake build type | Flags | VST3 .so |
|---|---|---|---|
| O1 | RelWithDebInfo | `-O1 -DNDEBUG` | 10.9 MB |
| O2 | RelWithDebInfo | `-O2 -g -DNDEBUG` | 113.5 MB (debug info) |
| O3lto | Release | JUCE's `-O3 -flto` | 5.3 MB |
| Os | MinSizeRel | `-Os -DNDEBUG` | 9.6 MB |
| Ofast | RelWithDebInfo | `-Ofast -DNDEBUG` (fast-math) | 12.6 MB |
| O3native | RelWithDebInfo | `-O3 -march=native -DNDEBUG` | 12.9 MB |

All compile with no errors and no compiler warnings. The only messages are two
linker notes in O3lto ("using serial compilation of N LTRANS jobs"): LTO links on a
single core, which only costs build time.

## Tests per build

- **pluginval --strictness-level 10** (temporary `HOME`): SUCCESS for all six. (A
  release build has assertions off; the Debug runs, all assertion-free, cover those.)
- **Factory presets:** the Standalone with a fresh `HOME` deploys all 21 presets, with
  values identical to `StereoWidener/presets/` (all six builds).
- **Sound:** `python/compare_builds.py` renders all 21 factory presets on a mono synth
  loop and a stereo mix with each build's `WidenerRender` and compares against the
  Debug build ([compare_builds.txt](../../python/results/release_builds/compare_builds.txt)):

| Build | Max. difference to Debug | NaN/Inf |
|---|---|---|
| O1, O2, O3, Os | bit-identical | no |
| Ofast | -84 dB re peak (Master - Tight Low End) | no |
| O3native | -73 dB re peak (Synth Pad - Big) | no |

  Without fast-math, GCC keeps IEEE semantics, so O1-O3/Os are bit-identical to Debug.
  `-Ofast` reorders float maths and `-march=native` fuses multiply-adds (FMA); both
  change the rounding, and the largest change (in the multiband crossovers' IIR
  filters) is -73 dB, far below audibility. The code has no NaN/Inf tests that
  fast-math could remove; the meters clamp before `log10`.

  (`WidenerRender` gets JUCE's `-O3` in Release but not its LTO flags, so its O3lto
  row is plain `-O3`.)

## CPU

Render time of 60 s of stereo noise at 48 kHz, as % of real time on one core (best of
3, including file I/O, so an upper bound):

| Build | Broadband | Filtered | Comb | Allpass | Multiband | Early refl. | Chorus |
|---|---|---|---|---|---|---|---|
| Debug | 0.51 % | 0.84 % | 1.07 % | 1.77 % | 7.51 % | 3.63 % | 1.37 % |
| O1 | 0.12 % | 0.16 % | 0.19 % | 0.33 % | 1.12 % | 0.52 % | 0.27 % |
| O2 | 0.12 % | 0.16 % | 0.19 % | 0.28 % | 1.03 % | 0.49 % | 0.27 % |
| O3 | 0.11 % | 0.16 % | 0.19 % | 0.30 % | 1.12 % | 0.49 % | 0.26 % |
| Os | 0.12 % | 0.16 % | 0.19 % | 0.28 % | 1.08 % | 0.64 % | 0.27 % |
| Ofast | 0.11 % | 0.16 % | 0.19 % | 0.28 % | 1.06 % | 0.54 % | 0.26 % |
| O3native | 0.11 % | 0.16 % | 0.17 % | 0.27 % | 1.05 % | 0.40 % | 0.23 % |

Optimised builds are 4-7x faster than Debug; between optimisation levels the
differences are small (mostly within measurement noise). Multiband Width is the most
expensive algorithm at about 1 % of a core, everything else below 0.7 %.

## Recommendation

- Ship the **Release** build (JUCE's default `-O3 -flto`): bit-identical to what was
  tested and verified in Debug, smallest binary, same speed.
- `-Ofast` gains nothing measurable here, so it isn't worth the changed rounding.
- `-march=native` must not be used for distributed binaries (they would only run on
  CPUs with the build machine's instruction set); the gain is small anyway.
- Optional: `-flto=auto` would let the LTO link use all cores (build time only).

## Reproduce

`python/results/release_builds/build_all.sh` (configure + build all six) and
`plugin_tests.sh <config...>` (pluginval + preset deployment), then
`python python/compare_builds.py <Debug WidenerRender> <name>=<WidenerRender> ...`.
Build folders were in the session scratchpad (not kept).

## Files

- `python/compare_builds.py` (new).

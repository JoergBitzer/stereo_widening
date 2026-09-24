# StereoWidener

A stereo-width processing JUCE plugin. Part of the [stereo_widening](../README.md)
project; built from a copy of [StereoAnalyzer](../StereoAnalyzer/), sharing its metering
components (`shared/metering/`) so both plugins show the same goniometer/level-meter
look and any fix helps both.

See [../docs/algorithms/phase3_stereo_widener.md](../docs/algorithms/phase3_stereo_widener.md)
for the architecture, the algorithm-switch crossfade, and the two algorithms.

## Build

From the shared `AudioDev/build` directory (see the top-level `../README.md`):

```console
cmake --build . --target StereoWidener_VST3 -j8
cmake --build . --target StereoWidener_Standalone -j8
```

## Parameters

- **Width**: 0-200 %. 0 collapses the side signal to mono, 100 is unity (unchanged from
  the input), 200 doubles the side signal.
- **Algorithm**: which stereo-widening algorithm processes the signal. Switching is
  crossfaded (equal-power, 30 ms) so it never clicks.
  - *M/S Width (Broadband)*: plain M/S width control across the whole spectrum.
  - *M/S Width (Filtered / Bass Mono)*: the same control, but the side signal is
    high-pass filtered first, so bass content is forced mono and only the highs get
    widened.

# StereoAnalyzer

A pass-through JUCE plugin for visualising the stereo image: goniometer, correlation
meter, and L/R/M/S level meters. Part of the [stereo_widening](../README.md) project;
built from the [AdvancedAudioTemplate](https://github.com/JoergBitzer/AdvancedAudioTemplate).

See [../docs/algorithms/phase2_stereo_analyzer.md](../docs/algorithms/phase2_stereo_analyzer.md)
for the architecture, the design decisions, and the cross-check against
`python/stereo_eval`.

## Build

From the shared `AudioDev/build` directory (see the top-level `../README.md`):

```console
cmake --build . --target StereoAnalyzer_VST3 -j8
cmake --build . --target StereoAnalyzer_Standalone -j8
```

## Parameters

- **Integration**: the time constant shared by the RMS and correlation meters
  (Fast 100 ms / Medium 300 ms / Slow 1000 ms).

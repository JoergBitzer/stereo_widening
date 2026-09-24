# Stereo Widening

JUCE audio plugins that analyse and change the stereo image of audio signals.
The project is a teaching example for students and young engineers, and at the same
time a tool for mixing and mastering.

- **StereoAnalyzer**: goniometer, correlation meter, L/R/M/S levels, spectra (in progress)
- **StereoWidener**: several switchable stereo widening algorithms, in two profiles:
  Mastering (mono-safe) and Creative (planned)

Planning documents: [planing.md](planing.md) (algorithm catalogue and theory) and
[plan2.md](plan2.md) (current plan and roadmap).

## Structure

| Folder | Content |
|--------|---------|
| `shared/` | code used by both plugins (metering, DSP helpers) |
| `StereoAnalyzer/` | analysis plugin |
| `StereoWidener/` | widening plugin, one class per algorithm in `algorithms/` |
| `python/` | test signal generation, evaluation (`stereo_eval`), algorithm references |
| `docs/algorithms/` | one short page per algorithm |
| `test_signals/` | test audio, **not in the repository** (see [test_signals.md](test_signals.md)) |

## Setup

The plugins use the AudioDev environment: the top-level `CMakeLists.txt` from
AudioDevOrga, `JUCE/`, and `Libs/TGMStaticLib`, based on the
[AdvancedAudioTemplate](https://github.com/JoergBitzer/AdvancedAudioTemplate).

```
AudioDev/
├── CMakeLists.txt     # add_subdirectory(stereo_widening/StereoAnalyzer) ...
├── JUCE/
├── Libs/
└── stereo_widening/   # this repository
```

Test samples:

```console
./copy_test_samples.sh            # copies from ~/Music/samples to test_signals/samples
```

## License

TBD

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

Python environment, test signals and evaluation (run from the project folder):

```console
python3 -m venv .venv && .venv/bin/pip install -r python/requirements.txt
./copy_test_samples.sh                              # samples from ~/Music/samples
cd python
../.venv/bin/python generate_test_signals.py        # -> test_signals/generated/
../.venv/bin/python -m pytest                       # validates measures and algorithms
../.venv/bin/python evaluate_ms_width.py            # -> python/results/ms_width/
```

`stereo_eval` provides correlation (broadband, per 1/3 octave, over time), L/R/M/S
levels, loudness (BS.1770), mono-sum colouration, and IACC for loudspeaker playback
(spherical head model, speakers at ±30°). `report.evaluate(x_in, x_out, fs)` runs all
of them.

## License

TBD

# Consistent Width in every algorithm (v0.1.26)

Question from review: are there differences between Amount and Width, and can Width
behave as in most algorithms -- 0 % = mono output, 100 % = transparent, 200 % =
extra wide?

## Before

| Algorithm | Width | 0 % mono / 100 % transparent / 200 % extra wide? |
|---|---|---|
| M/S Broadband | scales S | yes |
| M/S Filtered | scales the filtered S | yes (transparent with both stages Off) |
| Complementary Comb | scales S including the added comb part | yes (transparent at Gain 0 %) |
| Allpass Decorrelation | scales S after the decorrelation | yes (transparent at Amount 0 %) |
| Multiband | one width per band | yes, per band |
| **Early Reflections** | `L' = L + Width * Amount * Y_L` | **no** -- Width was a second Amount; 0 % meant "no reflections" |
| **Chorus Doubler** | `L' = L + Width * Amount * Y_L` | **no**, same |

## Change

In Early Reflections and Chorus Doubler, Amount alone now blends in the effect, and
Width is applied last as a plain M/S width on the result -- the same pattern
AllpassDecorrelation already used:

    L1 = L + Amount * Y_L,   R1 = R + Amount * Y_R
    M' = (L1 + R1) / 2,      S' = Width * (L1 - R1) / 2
    L' = M' + S',            R' = M' - S'

At Width 0 % the output is mono (it contains the effect's contribution to the mono
sum, as Utilities -> Monitor -> Mono Check would); at 100 % Width changes nothing;
at 200 % the side signal, including the width the effect creates, is doubled.

The displays follow: the echogram's reflection levels and the chorus curves' fading
now depend on Amount only. The help texts say what Width does.

Compatibility: at Width 100 % the output is the same as before (within float
rounding). Early Reflections/Chorus settings with Width != 100 % sound different.
The Python reference implementations (`python/algorithms/early_reflections.py`,
`chorus_doubler.py`) have no Width and correspond to the plugin at 100 % -- unchanged.

## Verification

[python/results/consistent_width/check.txt](../../python/results/consistent_width/check.txt),
on `mix_loop_let_it_be.wav`, for both algorithms:

- Width 100 %: max |new - old| = 1.2e-7 (Early Reflections), 6.0e-8 (Chorus), i.e.
  about -138 dB, float rounding.
- Width 0 %: L = R exactly (mono); mid unchanged.
- Width 200 %: S exactly doubled (max error 1.2e-7), mid unchanged.
- Amount 0 %, Width 100 %: output = input exactly (transparent).
- Python cross-checks: `crosscheck_early_reflections.py` and
  `crosscheck_chorus_doubler.py` PASS.
- GUI: both displays render unchanged at Width 0 %.
- `pluginval --strictness-level 10`: 4/4 SUCCESS, zero JUCE assertions.

## Files

- `StereoWidener/algorithms/EarlyReflections.h/.cpp`, `ChorusDoubler.h/.cpp`: M/S
  width on the output, docs, help text.
- `StereoWidener/playgrounds/EchogramView.h/.cpp`, `DelayModulationView.h/.cpp`,
  `EarlyReflectionsPlayground.cpp`, `ChorusPlayground.cpp`: no longer take Width.
- `StereoWidener/CMakeLists.txt`: version 0.1.25 -> 0.1.26.

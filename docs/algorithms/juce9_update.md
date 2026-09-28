# Update to JUCE 9.0.3 (v1.0.1)

A Windows build on a machine with JUCE's `develop` branch failed with many errors. The
Linux machine was on `develop` too (a snapshot of November 2024, JUCE 8.0.x). JUCE's
release branch is `master` (there is no `main`); it is now at **JUCE 9.0.3**
(released 2026-09-28), and `AudioDev/JUCE` was switched to it
(`git checkout master && git pull`).

## Changes needed

Only two, both warnings, no errors:

- `StereoWidener/PluginProcessor.cpp`: `jassert(("message", condition))` (a comma
  expression, "left operand has no effect") -> `jassert(condition); // message`.
- `tools/widener_render/main.cpp`: JUCE 9 deprecates
  `AudioFormat::createWriterFor(OutputStream*, rate, channels, bits, metadata, quality)`;
  now uses `AudioFormatWriterOptions` (32-bit float wav, as before).

## Verification (Linux, GCC 13.3)

- Fresh Debug and Release builds (VST3, Standalone, WidenerRender): no errors, no
  warnings.
- A strict build with `-std=c++17 -Wpedantic` (standard C++17 without GNU extensions,
  closer to what MSVC accepts): no warnings in our code. No MSVC-specific problem spots
  found in a scan (no designated initializers, VLAs, GCC attributes; `M_PI` is used
  with `_USE_MATH_DEFINES`).
- Sound: all 21 factory presets on a mono and a stereo signal, JUCE 9 Debug and JUCE 9
  Release against the previous JUCE 8 Debug build: **bit-identical**
  ([compare_juce8_juce9.txt](../../python/results/juce9/compare_juce8_juce9.txt)).
  CPU unchanged.
- pluginval --strictness-level 10: Debug 3/3, Release 1/1 SUCCESS, zero JUCE
  assertions ([console.txt](../../python/results/juce9/console.txt)).
- Factory presets: fresh install deploys all 21, values identical.
- The VST3 now reports `SDKVersion: VST 3.8.0` -- the VST3 SDK bundled with JUCE 9 is
  under the MIT license (README updated).

Not tested here: MSVC (Windows) and Xcode (macOS) -- no such compilers on this
machine. On Windows, use JUCE `master` (9.0.3) as well.

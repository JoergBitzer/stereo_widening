/**
 * @file ComplementaryComb.h
 * @brief Algorithm 2.4 (planing.md): complementary comb filter pseudo-stereo
 *        (Lauridsen / Schroeder).
 *
 * planing.md's base formula for a mono input x: L' = x + g*x[n-D], R' = x - g*x[n-D].
 * Generalised to a stereo input as planing.md itself suggests ("apply to M and add to
 * the existing S"): the mid signal M is left untouched (so L'+R' = 2M = L+R always --
 * perfectly mono-compatible by construction, independent of Delay/Gain/Width), and a
 * delayed, gained copy of M is added to the existing side signal:
 *
 *     M' = M
 *     S' = Width * (S + Gain * HighPass(M, crossoverHz)[n - Delay])
 *     L' = M' + S',  R' = M' - S'
 *
 * The high-pass on the delayed contribution (not on S or M themselves) is planing.md's
 * own suggested improvement ("Better with ... a crossover (apply only above ~300 Hz)"):
 * low frequencies carry most of a mix's energy and are the most audible as "phasiness"/
 * comb-filtering colouration, so they are excluded -- only the highs get the
 * comb-widened treatment. Verified in python/evaluate_comb.py: disabling the crossover
 * measurably increases the low-frequency-heavy decorrelation (dS-M), and the
 * per-octave correlation plot shows "out" tracking "in" closely below the crossover and
 * dropping above it.
 *
 * Parameters (getParamSpecs()): Width, Delay and Gain. Width scales the whole
 * resulting S', exactly like MSWidthBroadband/MSWidthFiltered, so every algorithm
 * shares the same "how much effect" feel. The crossover frequency is still a
 * GlobalSettings default (Phase 4), not yet a parameter -- see setCrossoverHz().
 *
 * Unlike MSWidthBroadband/MSWidthFiltered, this algorithm creates real width from
 * dual-mono input (verified in python/evaluate_comb.py: speech_dry_answers, which M/S
 * width cannot touch at all, gets genuinely decorrelated here) -- the actual point of a
 * *pseudo*-stereo technique, as opposed to a *width* technique that can only reshape
 * width that already exists.
 *
 * The Delay knob's value is smoothed (smoothedDelaySamples, see the file's own
 * comment), not applied to delayLine directly: an unsmoothed juce::dsp::DelayLine::
 * setDelay() steps the read position discontinuously, causing an audible "zipper"
 * click on every change -- most noticeable while dragging the knob. Considered and
 * rejected: a dedicated time-variant delay-line class with built-in ramping (found to
 * be designed for a different use case here -- N-channel feedback/crosstalk delays --
 * and not safely usable with a single mono channel, see git history/PR discussion);
 * simply ramping juce::SmoothedValue<float> and feeding it to the existing,
 * already-verified DelayLine every sample achieves the same fix with no new class and
 * no external dependency.
 *
 * Reference: M. R. Schroeder, "An Artificial Stereophonic Effect Obtained from a
 * Single Audio Signal", J. Audio Eng. Soc., 1958.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_dsp/juce_dsp.h>
#include "StereoAlgorithm.h"

class ComplementaryComb : public StereoAlgorithm
{
public:
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, const AlgorithmParamValues& values) noexcept override;

    // Ranges from planing.md 2.4: "D ~= 5-20 ms and g ~= 0.3-0.7". Gain is widened to
    // the full 0-100 %; 0 % is neutral (same output as MSWidthBroadband), so Delay just
    // starts at a representative mid-range value.
    enum ParamIndex { kWidth = 0, kDelay, kGain };
    std::vector<AlgorithmParamSpec> getParamSpecs() const override
    {
        return {
            AlgorithmParamSpec::width("combWidth"),
            AlgorithmParamSpec::linear("combDelay", "Delay", "ms", 5.0f, 20.0f, 10.0f, 1),
            AlgorithmParamSpec::linear("combGain", "Gain", "%", 0.0f, 100.0f, 0.0f)
        };
    }

    const char* getName() const noexcept override { return "Complementary Comb (Pseudo-Stereo)"; }
    juce::String getDescription() const override;
    bool isMonoSafe() const noexcept override { return true; }
    int getLatencySamples() const noexcept override { return 0; } // the delay only feeds S, it is not an output-wide latency

    /** User-configurable default (GlobalSettings, Phase 4): the delayed contribution
     *  added to S is high-pass filtered above this frequency first, so low frequencies
     *  (the most audible as "phasiness") are excluded -- planing.md's own suggested
     *  improvement, see the file header. Not itself a user-facing parameter. */
    void setCrossoverHz(float hz) noexcept;

    static constexpr float kFilterQ = 0.70710678f; // Butterworth (maximally flat)
    // a bit past the Delay parameter's own max (20 ms, getParamSpecs()), so the delay line never needs to grow after prepare()
    static constexpr float kMaxDelayMs = 25.0f;
    // How long a Delay-knob change takes to glide in, rather than stepping instantly
    // (see smoothedDelaySamples below) -- matches the project's other short UI-driven
    // ramps, e.g. StereoWidenerAudio::kCrossfadeSeconds.
    static constexpr float kDelaySmoothingSeconds = 0.02f;

private:
    void updateCrossoverFilter() noexcept;

    double sampleRate = 48000.0;
    float crossoverHz = 300.0f;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 4096 };
    juce::dsp::IIR::Filter<float> crossoverFilter;

    // Ramps delayLine's read position smoothly to a new Delay-knob value instead of
    // stepping it instantly, which caused an audible "zipper" click (the read pointer
    // jumping discontinuously to a different position in the circular buffer every
    // time the parameter changed -- most noticeable while dragging the knob). false
    // until the first process() call, which snaps (no ramp) to that block's delay
    // instead of gliding in from 0 -- see process().
    juce::SmoothedValue<float> smoothedDelaySamples;
    bool delayInitialized = false;
};

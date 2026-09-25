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
 * Two user-facing parameters, per the user's explicit request to minimise per-algorithm
 * controls to "2 + Width": Delay (StereoWidenerGUI's left aux knob for this algorithm)
 * and Gain (the right aux knob). Width (shared across every algorithm) scales the whole
 * resulting S', exactly like MSWidthBroadband/MSWidthFiltered, so every algorithm
 * shares the same "how much effect" feel. The crossover frequency is a GlobalSettings
 * default (Phase 4), not a user-facing knob -- see setCrossoverHz().
 *
 * Unlike MSWidthBroadband/MSWidthFiltered, this algorithm creates real width from
 * dual-mono input (verified in python/evaluate_comb.py: speech_dry_answers, which M/S
 * width cannot touch at all, gets genuinely decorrelated here) -- the actual point of a
 * *pseudo*-stereo technique, as opposed to a *width* technique that can only reshape
 * width that already exists.
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
    void process(juce::AudioBuffer<float>& buffer, const StereoAlgorithmParams& params) noexcept override;

    const char* getName() const noexcept override { return "Complementary Comb (Pseudo-Stereo)"; }
    juce::String getDescription() const override;
    AuxKnobInfo getAuxLeftInfo() const noexcept override { return { true, "Delay" }; }
    AuxKnobInfo getAuxRightInfo() const noexcept override { return { true, "Gain" }; }
    bool isMonoSafe() const noexcept override { return true; }
    int getLatencySamples() const noexcept override { return 0; } // the delay only feeds S, it is not an output-wide latency

    /** User-configurable default (GlobalSettings, Phase 4): the delayed contribution
     *  added to S is high-pass filtered above this frequency first, so low frequencies
     *  (the most audible as "phasiness") are excluded -- planing.md's own suggested
     *  improvement, see the file header. Not itself a user-facing parameter. */
    void setCrossoverHz(float hz) noexcept;

    static constexpr float kFilterQ = 0.70710678f; // Butterworth (maximally flat)
    // a bit past the Delay knob's own max (g_paramCombDelay.maxValue = 20 ms,
    // StereoWidener.h), so the delay line never needs to grow after prepare()
    static constexpr float kMaxDelayMs = 25.0f;

private:
    void updateCrossoverFilter() noexcept;

    double sampleRate = 48000.0;
    float crossoverHz = 300.0f;
    float lastDelayMs = -1.0f;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 4096 };
    juce::dsp::IIR::Filter<float> crossoverFilter;
};

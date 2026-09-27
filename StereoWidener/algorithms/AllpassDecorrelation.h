/**
 * @file AllpassDecorrelation.h
 * @brief Algorithm 2.5 (planing.md): allpass-cascade decorrelation.
 *
 * Two different allpass cascades (kNumStages 2nd-order allpass sections each, built
 * with juce::dsp::IIR::Coefficients::makeAllPass -- same RBJ cookbook formula as the
 * Python reference, python/algorithms/allpass_decorrelation.py, which has the full
 * derivation) are applied to the mid signal M, giving two copies with M's exact
 * magnitude spectrum but different phase:
 *
 *     Y1 = AP1(M), Y2 = AP2(M)
 *     L' = L + Amount * (Y1 - M)
 *     R' = R + Amount * (Y2 - M)
 *     M' = (L' + R') / 2,  S' = Width * (L' - R') / 2
 *     L'' = M' + S',  R'' = M' - S'
 *
 * Amount = 0 is an exact bypass. AP1's cascade frequencies are fixed (kBaseFreqsHz);
 * AP2's are AP1's shifted up by Spread octaves -- Spread = 0 makes AP1 == AP2 (Y1 = Y2),
 * which leaves S completely untouched (L'-R' algebraically reduces to L-R) but *does*
 * still colour M (see the Python reference's module docstring for why -- summing a
 * signal with a phase-shifted copy of itself is not magnitude-neutral even though each
 * copy alone has an unchanged magnitude spectrum). Spread > 0 makes AP1 != AP2, adding
 * genuine inter-channel decorrelation on top of that.
 *
 * Unlike algorithm 2.4's comb, this one is NOT mono-safe: because M itself changes
 * (not just S, see above), L''+R'' generally does *not* equal L+R once Amount > 0 --
 * planing.md's own stated con for this technique. isMonoSafe() reports false, and
 * StereoWidenerGUI shows a "not mono-safe" badge + mono-check hint when this algorithm
 * is selected (see StereoWidener.cpp).
 *
 * Parameters (getParamSpecs()): Width, Amount and Spread.
 * Amount defaults to 0 % (neutral/bypass, matching every other algorithm's neutral-
 * default convention, see GlobalSettings.h); Spread has no "neutral" value of its own
 * (inert when Amount = 0, same reasoning as ComplementaryComb's Delay default).
 *
 * Reference: general "decorrelation filter" technique, planing.md 2.5; J. S. Kendall,
 * "The Decorrelation of Audio Signals and Its Impact on Spatial Imagery", Computer
 * Music Journal, 1995.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <array>
#include <juce_dsp/juce_dsp.h>
#include "StereoAlgorithm.h"

class AllpassDecorrelation : public StereoAlgorithm
{
public:
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, const AlgorithmParamValues& values) noexcept override;

    // Spread 0-100 % maps to 0..kMaxSpreadOctaves inside process().
    enum ParamIndex { kWidth = 0, kAmount, kSpread };
    std::vector<AlgorithmParamSpec> getParamSpecs() const override
    {
        return {
            AlgorithmParamSpec::width("allpassWidth"),
            AlgorithmParamSpec::linear("allpassAmount", "Amount", "%", 0.0f, 100.0f, 0.0f),
            AlgorithmParamSpec::linear("allpassSpread", "Spread", "%", 0.0f, 100.0f, 50.0f)
        };
    }

    // Pure math for the GUI's display (no GUI dependency), the same designs process()
    // uses.

    /** Centre frequency of allpass stage (0..kNumStages-1) of the left (cascade 1,
     *  fixed) or right (cascade 2, shifted up by spreadOctaves) channel. */
    static float stageFrequencyHz(int stage, bool left, float spreadOctaves, double sampleRate) noexcept
    {
        const float hz = left ? kBaseFreqsHz[stage] : kBaseFreqsHz[stage] * std::pow(2.0f, spreadOctaves);
        return juce::jmin(hz, 0.45f * (float) sampleRate); // stay clear of Nyquist
    }

    /** How a mono (centred) input comes out of L, R and the mono sum (L+R)/2, as gain
     *  in dB at frequencyHz -- with S = 0: L1 = M (1 + Amount (H1 - 1)), R1 likewise
     *  with H2, then Width on the result. Each cascade alone is magnitude-flat; the
     *  colouration comes from blending it with the dry signal. -100 dB = -inf. */
    static void monoInputGainDb(float frequencyHz, const AlgorithmParamValues& values, double sampleRate,
                                float& leftDb, float& rightDb, float& monoDb) noexcept;

    const char* getName() const noexcept override { return "Allpass Decorrelation"; }
    juce::String getDescription() const override;
    bool isMonoSafe() const noexcept override { return false; }
    int getLatencySamples() const noexcept override { return 0; }

    static constexpr int kNumStages = 4;
    static constexpr float kFilterQ = 0.70710678f; // Butterworth, matches ComplementaryComb's crossover
    static constexpr float kMaxSpreadOctaves = 2.0f;
    static constexpr float kBaseFreqsHz[kNumStages] = { 200.0f, 700.0f, 2400.0f, 8000.0f };

private:
    void updateCascades(float spreadOctaves) noexcept;

    double sampleRate = 48000.0;
    float lastSpreadOctaves = -1.0f;

    std::array<juce::dsp::IIR::Filter<float>, kNumStages> cascade1;
    std::array<juce::dsp::IIR::Filter<float>, kNumStages> cascade2;
};

#include "AllpassDecorrelation.h"
#include <cmath>

namespace
{
    bool hasChanged(float value, float lastValue) noexcept
    {
        return std::abs(value - lastValue) > 1.0e-6f;
    }
}

void AllpassDecorrelation::prepare(double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 1 };
    for (auto& stage : cascade1)
        stage.prepare(spec);
    for (auto& stage : cascade2)
        stage.prepare(spec);

    lastSpreadOctaves = -1.0f;
    updateCascades(0.0f);
}

void AllpassDecorrelation::reset()
{
    for (auto& stage : cascade1)
        stage.reset();
    for (auto& stage : cascade2)
        stage.reset();
}

void AllpassDecorrelation::updateCascades(float spreadOctaves) noexcept
{
    // Clamped below Nyquist with margin, same reasoning as the Python reference
    // (algorithms/allpass_decorrelation.py): the RBJ cookbook allpass formula
    // (juce::dsp::IIR::Coefficients::makeAllPass uses the same one) is only valid
    // below Nyquist, and a large Spread could otherwise push a high base frequency
    // (8000 Hz) above it.
    const float nyquistMarginHz = 0.45f * (float) sampleRate;
    const float shiftFactor = std::pow(2.0f, spreadOctaves);

    for (int i = 0; i < kNumStages; ++i)
    {
        const float freq1 = juce::jmin(kBaseFreqsHz[i], nyquistMarginHz);
        const float freq2 = juce::jmin(kBaseFreqsHz[i] * shiftFactor, nyquistMarginHz);
        *cascade1[(size_t) i].coefficients = *juce::dsp::IIR::Coefficients<float>::makeAllPass(sampleRate, freq1, kFilterQ);
        *cascade2[(size_t) i].coefficients = *juce::dsp::IIR::Coefficients<float>::makeAllPass(sampleRate, freq2, kFilterQ);
    }
}

void AllpassDecorrelation::process(juce::AudioBuffer<float>& buffer, const AlgorithmParamValues& values) noexcept
{
    const float width = values[kWidth] * 0.01f; // % -> 0..2
    const float spreadOctaves = (values[kSpread] * 0.01f) * kMaxSpreadOctaves; // % -> 0..1 -> octaves
    if (hasChanged(spreadOctaves, lastSpreadOctaves))
    {
        updateCascades(spreadOctaves);
        lastSpreadOctaves = spreadOctaves;
    }

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    const int numSamples = buffer.getNumSamples();
    const float amount = values[kAmount] * 0.01f; // % -> 0..1

    for (int i = 0; i < numSamples; ++i)
    {
        const float mid = 0.5f * (left[i] + right[i]);

        float y1 = mid;
        for (auto& stage : cascade1)
            y1 = stage.processSample(y1);
        float y2 = mid;
        for (auto& stage : cascade2)
            y2 = stage.processSample(y2);

        const float lOut = left[i] + amount * (y1 - mid);
        const float rOut = right[i] + amount * (y2 - mid);

        const float midOut = 0.5f * (lOut + rOut);
        const float sideOut = width * 0.5f * (lOut - rOut);
        left[i] = midOut + sideOut;
        right[i] = midOut - sideOut;
    }
}

juce::String AllpassDecorrelation::getDescription() const
{
    return "Allpass-cascade decorrelation.\n\n"
           "The mid signal M is filtered through two DIFFERENT cascades of allpass "
           "sections, giving two copies Y1/Y2 with M's exact magnitude spectrum but "
           "different phase. Amount blends each channel from dry towards its own "
           "decorrelated copy: L' = L + Amount*(Y1-M), R' = R + Amount*(Y2-M). Spread "
           "controls how far apart the two cascades' frequencies sit -- 0 makes them "
           "identical (no decorrelation).\n\n"
           "Unlike Complementary Comb, this technique is NOT mono-compatible: because "
           "the mid signal itself changes, the mono sum L'+R' is coloured once Amount "
           "is above 0 -- a real trade-off of this technique, not a bug. Check your "
           "mix in mono (Utilities -> Monitor -> Mono Check) before committing to a "
           "setting.\n\n"
           "Source: J. S. Kendall, \"The Decorrelation of Audio Signals and Its Impact "
           "on Spatial Imagery\", Computer Music Journal, 1995.";
}

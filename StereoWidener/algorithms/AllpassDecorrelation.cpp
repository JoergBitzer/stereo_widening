#include "AllpassDecorrelation.h"
#include <cmath>
#include "BiquadResponse.h"

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
    for (int i = 0; i < kNumStages; ++i)
    {
        *cascade1[(size_t) i].coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeAllPass(
            sampleRate, stageFrequencyHz(i, true, spreadOctaves, sampleRate), kFilterQ);
        *cascade2[(size_t) i].coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeAllPass(
            sampleRate, stageFrequencyHz(i, false, spreadOctaves, sampleRate), kFilterQ);
    }
}

void AllpassDecorrelation::monoInputGainDb(float frequencyHz, const AlgorithmParamValues& values, double sampleRate,
                                           float& leftDb, float& rightDb, float& monoDb) noexcept
{
    const float spreadOctaves = (values[kSpread] * 0.01f) * kMaxSpreadOctaves;
    std::complex<double> h1 = 1.0, h2 = 1.0;
    for (int i = 0; i < kNumStages; ++i)
    {
        h1 *= biquadResponse(juce::dsp::IIR::ArrayCoefficients<float>::makeAllPass(
                  sampleRate, stageFrequencyHz(i, true, spreadOctaves, sampleRate), kFilterQ), frequencyHz, sampleRate);
        h2 *= biquadResponse(juce::dsp::IIR::ArrayCoefficients<float>::makeAllPass(
                  sampleRate, stageFrequencyHz(i, false, spreadOctaves, sampleRate), kFilterQ), frequencyHz, sampleRate);
    }
    const double amount = values[kAmount] * 0.01, width = values[kWidth] * 0.01;
    const auto l1 = 1.0 + amount * (h1 - 1.0);
    const auto r1 = 1.0 + amount * (h2 - 1.0);
    const auto mid = 0.5 * (l1 + r1);
    const auto side = width * 0.5 * (l1 - r1);
    const auto toDb = [](std::complex<double> h) { return juce::Decibels::gainToDecibels((float) std::abs(h), -100.0f); };
    leftDb = toDb(mid + side);
    rightDb = toDb(mid - side);
    monoDb = toDb(mid);
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
           "The graph shows what happens to a centred (mono) input: the gain of L "
           "(red), R (blue) and the mono sum L+R (grey). The markers along the bottom "
           "are the four allpass stages' frequencies, L's fixed and R's shifted up by "
           "Spread. Drag left/right for Spread, up/down for Amount.\n\n"
           "Unlike Complementary Comb, this technique is NOT mono-compatible: because "
           "the mid signal itself changes, the mono sum L'+R' is coloured once Amount "
           "is above 0 -- a real trade-off of this technique, not a bug. Check your "
           "mix in mono (Utilities -> Monitor -> Mono Check) before committing to a "
           "setting.\n\n"
           "Source: J. S. Kendall, \"The Decorrelation of Audio Signals and Its Impact "
           "on Spatial Imagery\", Computer Music Journal, 1995.";
}

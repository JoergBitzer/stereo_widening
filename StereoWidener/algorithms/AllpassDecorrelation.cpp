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
           "The mid signal M runs through two different cascades of four allpass "
           "filters -- same magnitude spectrum, different phase. Each channel is "
           "blended towards its own copy: L1 = L + Amount*(Y1-M), R1 = R + "
           "Amount*(Y2-M); Width then scales the side signal of the result. This "
           "creates real width even from a mono source.\n\n"
           + getControlsText() + "\n\n"
           "Display: what happens to a centred (mono) input -- the gain of L (red), R "
           "(blue) and the mono sum L+R (grey). The marks are the allpass stages' "
           "frequencies, L's fixed (top) and R's shifted up by Spread (bottom). Drag "
           "left/right for Spread, up/down for Amount.\n\n"
           "Not mono-compatible: the blend changes the mid signal, so the mono sum is "
           "coloured once Amount is above 0. Check your mix in mono (Utilities -> Monitor -> Mono Check).\n\n"
           "Source: J. S. Kendall, \"The Decorrelation of Audio Signals and Its Impact "
           "on Spatial Imagery\", Computer Music Journal, 1995.";
}

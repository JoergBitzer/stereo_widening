#include "MSWidthFiltered.h"

void MSWidthFiltered::prepare(double sampleRate, int maxBlockSize)
{
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 1 };
    sideHighpass.prepare(spec);
    // second-order Butterworth highpass: -12 dB/oct below kCrossoverHz, so the side
    // signal's bass content is removed (not just attenuated) well before it reaches M
    *sideHighpass.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, kCrossoverHz);
}

void MSWidthFiltered::reset()
{
    sideHighpass.reset();
}

void MSWidthFiltered::process(juce::AudioBuffer<float>& buffer, float width) noexcept
{
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float m = 0.5f * (left[i] + right[i]);
        const float s = 0.5f * (left[i] - right[i]);
        const float sHigh = sideHighpass.processSample(s); // low content removed -> forced mono via M
        const float sOut = sHigh * width;
        left[i] = m + sOut;
        right[i] = m - sOut;
    }
}

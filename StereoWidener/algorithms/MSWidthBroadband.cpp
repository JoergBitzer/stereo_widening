#include "MSWidthBroadband.h"

void MSWidthBroadband::process(juce::AudioBuffer<float>& buffer, const AlgorithmParamValues& values) noexcept
{
    const float width = values[kWidth] * 0.01f; // % -> 0..2
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float m = 0.5f * (left[i] + right[i]);
        const float s = 0.5f * (left[i] - right[i]) * width;
        left[i] = m + s;
        right[i] = m - s;
    }
}

juce::String MSWidthBroadband::getDescription() const
{
    return "Mid/Side width control.\n\n"
           "M = (L+R)/2, S = (L-R)/2. The side signal S is scaled by Width and the "
           "mid/side pair is recombined (L' = M + width*S, R' = M - width*S). Width=0 "
           "collapses to mono, Width=100% is unchanged, Width=200% doubles the side "
           "signal. Applied equally across the whole spectrum, including the bass.\n\n"
           "Source: R. Streicher and F. A. Everest, \"The New Stereo Soundbook\", "
           "3rd ed., Audio Engineering Associates, 2006 (stereo enhancement / M-S "
           "technique).";
}

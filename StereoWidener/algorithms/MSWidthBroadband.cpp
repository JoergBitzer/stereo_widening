#include "MSWidthBroadband.h"

void MSWidthBroadband::process(juce::AudioBuffer<float>& buffer, float width) noexcept
{
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

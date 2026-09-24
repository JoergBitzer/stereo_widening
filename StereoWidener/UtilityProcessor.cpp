#include "UtilityProcessor.h"
#include <cmath>

void UtilityProcessor::process(juce::AudioBuffer<float>& buffer, const UtilityParams& params) const noexcept
{
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    const int numSamples = buffer.getNumSamples();

    const bool rotationActive = std::abs(params.rotationDeg) > 1.0e-6f;
    const float rotationRad = params.rotationDeg * juce::MathConstants<float>::pi / 180.0f;
    const float cosPhi = std::cos(rotationRad);
    const float sinPhi = std::sin(rotationRad);

    // simple attenuate-one-side balance law (not an equal-power pan law -- this
    // adjusts the relative level of an already-stereo signal's two channels, it does
    // not pan a mono source)
    const float gainL = params.balance > 0.0f ? 1.0f - params.balance : 1.0f;
    const float gainR = params.balance < 0.0f ? 1.0f + params.balance : 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float l = left[i];
        float r = right[i];

        if (rotationActive)
        {
            const float rotatedL = l * cosPhi - r * sinPhi;
            const float rotatedR = l * sinPhi + r * cosPhi;
            l = rotatedL;
            r = rotatedR;
        }

        l *= gainL;
        r *= gainR;

        if (params.invertL) l = -l;
        if (params.invertR) r = -r;

        if (params.swapLR)
            std::swap(l, r);

        if (params.monitorMode == UtilityParams::MonoCheck)
        {
            const float mono = 0.5f * (l + r);
            l = mono;
            r = mono;
        }
        else if (params.monitorMode == UtilityParams::SoloSide)
        {
            const float side = 0.5f * (l - r);
            l = side;
            r = side;
        }

        left[i] = l;
        right[i] = r;
    }
}

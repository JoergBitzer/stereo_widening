#include "ComplementaryComb.h"
#include <cmath>

namespace
{
    // same epsilon-comparison pattern as MSWidthFiltered.cpp's hasChanged()
    bool hasChanged(float value, float lastValue) noexcept
    {
        return std::abs(value - lastValue) > 1.0e-6f;
    }
}

void ComplementaryComb::prepare(double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;

    const int maxDelaySamples = juce::roundToInt(kMaxDelayMs * 0.001f * (float) sampleRate) + 1;
    delayLine.setMaximumDelayInSamples(maxDelaySamples);
    delayLine.prepare(juce::dsp::ProcessSpec { sampleRate, (juce::uint32) maxBlockSize, 1 });

    crossoverFilter.prepare(juce::dsp::ProcessSpec { sampleRate, (juce::uint32) maxBlockSize, 1 });
    updateCrossoverFilter();

    lastDelayMs = -1.0f; // force the delay to be (re)applied on the next process()
}

void ComplementaryComb::reset()
{
    delayLine.reset();
    crossoverFilter.reset();
}

void ComplementaryComb::setCrossoverHz(float hz) noexcept
{
    crossoverHz = hz;
    updateCrossoverFilter();
}

void ComplementaryComb::updateCrossoverFilter() noexcept
{
    *crossoverFilter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, crossoverHz, kFilterQ);
}

void ComplementaryComb::process(juce::AudioBuffer<float>& buffer, const StereoAlgorithmParams& params) noexcept
{
    if (hasChanged(params.auxLeft, lastDelayMs))
    {
        delayLine.setDelay(params.auxLeft * 0.001f * (float) sampleRate);
        lastDelayMs = params.auxLeft;
    }

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    const int numSamples = buffer.getNumSamples();
    const float gain = params.auxRight; // 0..1, see StereoWidenerAudio::paramsFor()

    for (int i = 0; i < numSamples; ++i)
    {
        const float m = 0.5f * (left[i] + right[i]);
        const float s = 0.5f * (left[i] - right[i]);

        delayLine.pushSample(0, m);
        const float delayedMid = delayLine.popSample(0);
        const float delayedFiltered = crossoverFilter.processSample(delayedMid);

        const float sOut = params.width * (s + gain * delayedFiltered);
        left[i] = m + sOut;
        right[i] = m - sOut;
    }
}

juce::String ComplementaryComb::getDescription() const
{
    return "Complementary comb filter pseudo-stereo (Lauridsen/Schroeder).\n\n"
           "A delayed, gained copy of the mid signal M is added to the side signal S: "
           "S' = S + Gain * M[n-Delay], high-pass filtered above "
           + juce::String(juce::roundToInt(crossoverHz)) + " Hz first so low frequencies "
           "-- the most audible as \"phasiness\" -- are excluded and only the highs get "
           "the comb-widened treatment. The mid signal itself is never touched, so "
           "L'+R' = 2M always: perfectly mono-compatible by construction, and unlike "
           "the M/S width algorithms, this one creates real width from dual-mono "
           "input.\n\n"
           "Each channel on its own does get audible comb-filtering colouration, "
           "especially on headphones -- this is the known trade-off of this "
           "technique, not a bug.\n\n"
           "Source: M. R. Schroeder, \"An Artificial Stereophonic Effect Obtained from "
           "a Single Audio Signal\", J. Audio Eng. Soc., 1958.";
}

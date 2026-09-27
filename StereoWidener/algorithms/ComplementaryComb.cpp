#include "ComplementaryComb.h"

void ComplementaryComb::prepare(double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;

    const int maxDelaySamples = juce::roundToInt(kMaxDelayMs * 0.001f * (float) sampleRate) + 1;
    delayLine.setMaximumDelayInSamples(maxDelaySamples);
    delayLine.prepare(juce::dsp::ProcessSpec { sampleRate, (juce::uint32) maxBlockSize, 1 });

    crossoverFilter.prepare(juce::dsp::ProcessSpec { sampleRate, (juce::uint32) maxBlockSize, 1 });
    updateCrossoverFilter();

    smoothedDelaySamples.reset(sampleRate, (double) kDelaySmoothingSeconds);
    delayInitialized = false; // force a snap (not a glide-in from 0) on the next process()
}

void ComplementaryComb::reset()
{
    delayLine.reset();
    crossoverFilter.reset();
    delayInitialized = false; // same reasoning as in prepare(): snap cleanly, don't glide in
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

void ComplementaryComb::process(juce::AudioBuffer<float>& buffer, const AlgorithmParamValues& values) noexcept
{
    const float width = values[kWidth] * 0.01f; // % -> 0..2
    const float targetDelaySamples = values[kDelay] * 0.001f * (float) sampleRate;
    if (!delayInitialized)
    {
        smoothedDelaySamples.setCurrentAndTargetValue(targetDelaySamples); // first block: snap, no glide-in from 0
        delayInitialized = true;
    }
    else
    {
        smoothedDelaySamples.setTargetValue(targetDelaySamples);
    }

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    const int numSamples = buffer.getNumSamples();
    const float gain = values[kGain] * 0.01f; // % -> 0..1

    for (int i = 0; i < numSamples; ++i)
    {
        // Re-applied every sample (not just when the target changes): this is what
        // actually glides the read position smoothly instead of stepping it -- see the
        // file header/smoothedDelaySamples' own comment. Once settled,
        // getNextValue() just keeps returning the target, so this costs nothing extra
        // in the steady state beyond the (cheap) smoothed-value update itself.
        delayLine.setDelay(smoothedDelaySamples.getNextValue());

        const float m = 0.5f * (left[i] + right[i]);
        const float s = 0.5f * (left[i] - right[i]);

        delayLine.pushSample(0, m);
        const float delayedMid = delayLine.popSample(0);
        const float delayedFiltered = crossoverFilter.processSample(delayedMid);

        const float sOut = width * (s + gain * delayedFiltered);
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

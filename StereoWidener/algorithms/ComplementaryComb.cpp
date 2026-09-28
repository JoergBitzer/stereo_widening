#include "ComplementaryComb.h"
#include "BiquadResponse.h"
#include <cmath>

void ComplementaryComb::prepare(double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;

    const int maxDelaySamples = juce::roundToInt(kMaxDelayMs * 0.001f * (float) sampleRate) + 1;
    delayLine.setMaximumDelayInSamples(maxDelaySamples);
    delayLine.prepare(juce::dsp::ProcessSpec { sampleRate, (juce::uint32) maxBlockSize, 1 });

    crossoverFilter.prepare(juce::dsp::ProcessSpec { sampleRate, (juce::uint32) maxBlockSize, 1 });
    lastCrossoverHz = -1.0f; // force updateCrossoverIfNeeded() to (re)compute on the next process()

    smoothedDelaySamples.reset(sampleRate, (double) kDelaySmoothingSeconds);
    delayInitialized = false; // force a snap (not a glide-in from 0) on the next process()
}

void ComplementaryComb::reset()
{
    delayLine.reset();
    crossoverFilter.reset();
    delayInitialized = false; // same reasoning as in prepare(): snap cleanly, don't glide in
}

void ComplementaryComb::updateCrossoverIfNeeded(float crossoverHz) noexcept
{
    if (std::abs(crossoverHz - lastCrossoverHz) <= 1.0e-6f)
        return;
    *crossoverFilter.coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(sampleRate, crossoverHz, kFilterQ);
    lastCrossoverHz = crossoverHz;
}

void ComplementaryComb::monoInputGainDb(float frequencyHz, const AlgorithmParamValues& values, double sampleRate,
                                        float& leftDb, float& rightDb) noexcept
{
    const double delaySeconds = values[kDelay] * 0.001;
    const std::complex<double> a = (values[kWidth] * 0.01) * (values[kGain] * 0.01)
        * biquadResponse(juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(sampleRate, values[kCrossover], kFilterQ),
                         frequencyHz, sampleRate)
        * std::polar(1.0, -juce::MathConstants<double>::twoPi * frequencyHz * delaySeconds);
    leftDb = juce::Decibels::gainToDecibels((float) std::abs(1.0 + a), -100.0f);
    rightDb = juce::Decibels::gainToDecibels((float) std::abs(1.0 - a), -100.0f);
}

void ComplementaryComb::process(juce::AudioBuffer<float>& buffer, const AlgorithmParamValues& values) noexcept
{
    updateCrossoverIfNeeded(values[kCrossover]);
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
           "A delayed copy of the mid signal M, high-pass filtered above the Crossover, "
           "is added to the side signal: S' = Width * (S + Gain * M[n-Delay]). This "
           "creates real width even from a mono source. The mid signal is never "
           "touched, so L'+R' = 2M: perfectly mono-compatible. Each channel on its own "
           "is comb-filtered (audible on headphones) -- the known trade-off of this "
           "technique.\n\n"
           + getControlsText() + "\n\n"
           "Display: what happens to a centred (mono) input, on a linear 0-2 kHz axis "
           "(the pattern continues up to 20 kHz): L (red) has peaks where R (blue) has "
           "notches, 1/Delay apart; below the Crossover both stay flat. Drag the "
           "vertical line to move the Crossover.\n\n"
           "Source: M. R. Schroeder, \"An Artificial Stereophonic Effect Obtained from "
           "a Single Audio Signal\", J. Audio Eng. Soc., 1958.";
}

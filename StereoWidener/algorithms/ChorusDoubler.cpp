#include "ChorusDoubler.h"

void ChorusDoubler::prepare(double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;

    const int maxDelaySamples = juce::roundToInt(kMaxDelayMs * 0.001f * (float) sampleRate) + 1;
    delayLineL.setMaximumDelayInSamples(maxDelaySamples);
    delayLineR.setMaximumDelayInSamples(maxDelaySamples);
    delayLineL.prepare(juce::dsp::ProcessSpec { sampleRate, (juce::uint32) maxBlockSize, 1 });
    delayLineR.prepare(juce::dsp::ProcessSpec { sampleRate, (juce::uint32) maxBlockSize, 1 });

    smoothedDepth.reset(sampleRate, (double) kDepthSmoothingSeconds);
    depthInitialized = false; // force a snap (not a glide-in from 0) on the next process()
    phase = 0.0f;
}

void ChorusDoubler::reset()
{
    delayLineL.reset();
    delayLineR.reset();
    depthInitialized = false; // same reasoning as in prepare(): snap cleanly, don't glide in
    phase = 0.0f;
}

void ChorusDoubler::process(juce::AudioBuffer<float>& buffer, const AlgorithmParamValues& values) noexcept
{
    const float width = values[kWidth] * 0.01f; // % -> 0..2
    const float targetDepth = values[kDepth] * 0.01f; // % -> 0..1
    if (!depthInitialized)
    {
        smoothedDepth.setCurrentAndTargetValue(targetDepth);
        depthInitialized = true;
    }
    else
    {
        smoothedDepth.setTargetValue(targetDepth);
    }

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    const int numSamples = buffer.getNumSamples();
    const float amount = values[kAmount] * 0.01f; // % -> 0..1

    const float samplesPerMs = 0.001f * (float) sampleRate;
    const float baseSamples = kBaseDelayMs * samplesPerMs;
    const float stereoOffsetSamples = kStereoOffsetMs * samplesPerMs;
    const float phaseIncrement = juce::MathConstants<float>::twoPi * values[kRate] / (float) sampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        // Re-applied every sample: this is what actually glides the LFO excursion
        // smoothly instead of stepping it -- see the file header. Once settled,
        // getNextValue() just keeps returning the target.
        const float depth = smoothedDepth.getNextValue();
        const float excursionSamples = depth * kMaxDepthMs * samplesPerMs;

        // kStereoOffsetMs is NOT scaled by depth -- see the file header for why L and
        // R must stay genuinely different even at depth = 0.
        const float delayLSamples = baseSamples + excursionSamples * std::sin(phase);
        const float delayRSamples = baseSamples + stereoOffsetSamples
                                     + excursionSamples * std::sin(phase + kStereoPhaseOffsetRadians);

        const float m = 0.5f * (left[i] + right[i]);

        // One push + one pop per sample per delay line (default updateReadPointer =
        // true) -- the safe, standard usage; see the file header for why this
        // algorithm deliberately does NOT reuse EarlyReflections' shared-delay-line,
        // multiple-taps-per-push trick.
        delayLineL.setDelay(delayLSamples);
        delayLineL.pushSample(0, m);
        const float yL = delayLineL.popSample(0) - m;

        delayLineR.setDelay(delayRSamples);
        delayLineR.pushSample(0, m);
        const float yR = delayLineR.popSample(0) - m;

        // Amount blends the effect in; Width is then a plain M/S width on the result,
        // like every other algorithm's (see the file header).
        const float lOut = left[i] + amount * yL;
        const float rOut = right[i] + amount * yR;
        const float midOut = 0.5f * (lOut + rOut);
        const float sideOut = width * 0.5f * (lOut - rOut);
        left[i] = midOut + sideOut;
        right[i] = midOut - sideOut;

        phase += phaseIncrement;
        if (phase >= juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi; // wrap -- sin() is exactly periodic at 2*pi, so this is seamless
    }
}

juce::String ChorusDoubler::getDescription() const
{
    return "Chorus doubler (slow stereo chorus ensemble).\n\n"
           "The mid signal runs through two slowly modulated delay lines, one per "
           "channel, swinging a quarter cycle apart (R 3 ms longer on average) -- the "
           "classic stereo chorus width. This creates real width even from a "
           "mono source. Width then scales the side signal of the result.\n\n"
           + getControlsText() + "\n\n"
           "Display: both channels' delay times over 4 seconds. Drag up/down for "
           "Depth, left/right for Rate; the curves fade when Amount is 0.\n\n"
           "Not mono-compatible -- more so than Allpass Decorrelation or Early "
           "Reflections: the L/R delay difference keeps sweeping, so the mono sum shows "
           "a moving, \"flanging\" comb pattern once Amount is above 0. Check your mix in mono (Utilities -> Monitor -> Mono Check).\n\n"
           "Source: R. Dattorro, \"Effect Design Part 2: Delay Line Modulation and "
           "Chorus\", J. Audio Eng. Soc., 1997.";
}

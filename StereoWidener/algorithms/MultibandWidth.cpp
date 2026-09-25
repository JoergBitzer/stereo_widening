#include "MultibandWidth.h"
#include <algorithm>
#include <cmath>

namespace
{
    bool hasChanged(float value, float lastValue) noexcept
    {
        return std::abs(value - lastValue) > 1.0e-6f;
    }

    // Defensive safety net, independent of whatever ordering the GUI enforces on
    // manual knob drags (StereoWidenerGUI clamps each crossover knob against its
    // neighbours' current values on drag, but a DAW can still automate the three
    // crossover parameters directly, bypassing that): sorts the three raw values and
    // keeps them at least minRatio apart, so the filter coefficients built from them
    // are always valid (0 < f1 < f2 < f3) no matter what values actually arrive.
    void sortAndSeparate(float& f1, float& f2, float& f3) noexcept
    {
        std::array<float, 3> f { f1, f2, f3 };
        std::sort(f.begin(), f.end());
        constexpr float minRatio = 1.05f; // >= 5 % apart
        f[1] = juce::jmax(f[1], f[0] * minRatio);
        f[2] = juce::jmax(f[2], f[1] * minRatio);
        f1 = f[0]; f2 = f[1]; f3 = f[2];
    }
}

void MultibandWidth::prepare(double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 1 };
    midSplitter.prepare(spec);
    sideSplitter.prepare(spec);
    lastFreq1 = lastFreq2 = lastFreq3 = -1.0f;
}

void MultibandWidth::reset()
{
    midSplitter.reset();
    sideSplitter.reset();
}

void MultibandWidth::updateFrequenciesIfNeeded(float freq1, float freq2, float freq3) noexcept
{
    if (!hasChanged(freq1, lastFreq1) && !hasChanged(freq2, lastFreq2) && !hasChanged(freq3, lastFreq3))
        return;

    sortAndSeparate(freq1, freq2, freq3);
    midSplitter.setFrequencies(sampleRate, freq1, freq2, freq3);
    sideSplitter.setFrequencies(sampleRate, freq1, freq2, freq3);
    lastFreq1 = freq1; lastFreq2 = freq2; lastFreq3 = freq3;
}

void MultibandWidth::process(juce::AudioBuffer<float>& buffer, const StereoAlgorithmParams& params) noexcept
{
    updateFrequenciesIfNeeded(params.multi[kFreq1], params.multi[kFreq2], params.multi[kFreq3]);

    const float width2 = params.multi[kWidth2];
    const float width3 = params.multi[kWidth3];
    const float width4 = params.multi[kWidth4];

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float mid = 0.5f * (left[i] + right[i]);
        const float side = 0.5f * (left[i] - right[i]);

        float m1, m2, m3, m4;
        midSplitter.process(mid, m1, m2, m3, m4);
        float s1, s2, s3, s4;
        sideSplitter.process(side, s1, s2, s3, s4);
        juce::ignoreUnused(s1); // band 1 forced mono -- deliberately excluded below

        const float midOut = m1 + m2 + m3 + m4;
        const float sideOut = params.width * (width2 * s2 + width3 * s3 + width4 * s4);

        left[i] = midOut + sideOut;
        right[i] = midOut - sideOut;
    }
}

AuxKnobInfo MultibandWidth::getMultiParamInfo(int index) const noexcept
{
    switch (index)
    {
        case kFreq1:  return { true, "Low-Mid" };
        case kFreq2:  return { true, "Mid-High" };
        case kFreq3:  return { true, "High-Air" };
        case kWidth2: return { true, "Low-Mid" };
        case kWidth3: return { true, "Mid-High" };
        case kWidth4: return { true, "High" };
        default:      return {};
    }
}

juce::String MultibandWidth::getDescription() const
{
    return "Multiband width: 4 bands, 3 crossovers.\n\n"
           "The signal is split into 4 frequency bands by 3 Linkwitz-Riley 4th-order "
           "crossovers (Low-Mid, Mid-High, High-Air), and each band above the first "
           "gets its own M/S width control -- the lowest band's width is always 0 "
           "(\"bass mono\"), not a parameter: low frequencies translate poorly to mono "
           "playback if left wide and carry most of a mix's energy, so this is built "
           "in rather than left to the user to remember.\n\n"
           "Reconstructing a multi-way crossover exactly (flat magnitude) needs care: "
           "naively summing the bands back together leaves audible colouration, "
           "because each band picked up a different amount of the crossovers' own "
           "phase distortion on its way through the split. Every band here is "
           "phase-compensated (extra allpass filtering for whichever crossovers it "
           "skipped) so the reconstruction is flat to within a small fraction of a dB "
           "at every band width -- verified in python/evaluate_multiband.py.\n\n"
           "Source: S. Linkwitz, \"Active Crossover Networks for Noncoincident "
           "Drivers\", J. Audio Eng. Soc., 1976; B. Katz, \"Mastering Audio: The Art "
           "and the Science\", 3rd ed., Focal Press, 2015, ch. 3.";
}

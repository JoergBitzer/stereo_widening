#include "MSWidthFiltered.h"
#include <cmath>

namespace
{
    // same epsilon-comparison pattern as StereoAnalyzer.cpp's hasChanged(): avoids
    // -Wfloat-equal, and a tiny float rounding difference should not trigger an
    // unnecessary (audibly clicky) IIR coefficient recompute anyway
    bool hasChanged(float value, float lastValue) noexcept
    {
        return std::abs(value - lastValue) > 1.0e-6f;
    }
}

void MSWidthFiltered::prepare(double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 1 };
    sideHighpass.prepare(spec);
    sideHighShelf.prepare(spec);
    lastBassCutoffHz = -1.0f;  // force updateFiltersIfNeeded() to (re)compute on the next process()
    lastHighShelfHz = -1.0f;
}

void MSWidthFiltered::reset()
{
    sideHighpass.reset();
    sideHighShelf.reset();
}

void MSWidthFiltered::updateFiltersIfNeeded(float bassCutoffHz, float highShelfHz) noexcept
{
    if (hasChanged(bassCutoffHz, lastBassCutoffHz))
    {
        bassCutoffBypassed = bassCutoffHz < kBassCutoffOffThreshold;
        if (!bassCutoffBypassed)
            *sideHighpass.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, bassCutoffHz, kFilterQ);
        lastBassCutoffHz = bassCutoffHz;
    }
    if (hasChanged(highShelfHz, lastHighShelfHz))
    {
        highShelfBypassed = highShelfHz > kHighShelfOffThreshold;
        if (!highShelfBypassed)
        {
            const float gainFactor = juce::Decibels::decibelsToGain(highShelfGainDb);
            *sideHighShelf.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, highShelfHz, kFilterQ, gainFactor);
        }
        lastHighShelfHz = highShelfHz;
    }
}

void MSWidthFiltered::process(juce::AudioBuffer<float>& buffer, const AlgorithmParamValues& values) noexcept
{
    updateFiltersIfNeeded(values[kBassCutoff], values[kHighShelf]);
    const float width = values[kWidth] * 0.01f; // % -> 0..2

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float m = 0.5f * (left[i] + right[i]);
        const float s = 0.5f * (left[i] - right[i]);
        // "Off" bypasses the stage entirely (transparent), rather than just pushing its
        // frequency to an extreme -- see the file header and kBassCutoffOffThreshold/
        // kHighShelfOffThreshold.
        const float sBassMono = bassCutoffBypassed ? s : sideHighpass.processSample(s);
        const float sShelved = highShelfBypassed ? sBassMono : sideHighShelf.processSample(sBassMono);
        const float sOut = sShelved * width;
        left[i] = m + sOut;
        right[i] = m - sOut;
    }
}

juce::String MSWidthFiltered::getDescription() const
{
    return "Mid/Side width control with bass mono and a side high shelf.\n\n"
           "Same M/S width control as the broadband algorithm, but the side signal S "
           "is high-pass filtered at the Bass Cutoff frequency first, so content below "
           "it is forced into the mid signal (i.e. mono) regardless of the Width "
           "setting -- low frequencies translate poorly to mono playback if left wide "
           "and carry most of a mix's energy. A high shelf at the High Shelf frequency "
           "(currently " + juce::String(highShelfGainDb, 1) + " dB, configurable in "
           "the global settings file -- see GlobalSettings.h) then restores some "
           "high-frequency \"air\" to the widened side signal. Either stage can be "
           "switched off entirely: turn Bass Cutoff below 40 Hz, or High Shelf above "
           "16 kHz.\n\n"
           "Source: B. Katz, \"Mastering Audio: The Art and the Science\", 3rd ed., "
           "Focal Press, 2015, ch. 3 (\"Mono Compatibility and M-S Processing\").";
}

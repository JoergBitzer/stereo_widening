#include "MSWidthFiltered.h"
#include <cmath>
#include "BiquadResponse.h"

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

void MSWidthFiltered::updateFiltersIfNeeded(float bassCutoffHz, float highShelfHz, float shelfGainDb) noexcept
{
    if (hasChanged(bassCutoffHz, lastBassCutoffHz))
    {
        bassCutoffBypassed = bassCutoffHz < kBassCutoffOffThreshold;
        if (!bassCutoffBypassed)
            *sideHighpass.coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(sampleRate, bassCutoffHz, kFilterQ);
        lastBassCutoffHz = bassCutoffHz;
    }
    if (hasChanged(highShelfHz, lastHighShelfHz) || hasChanged(shelfGainDb, lastShelfGainDb))
    {
        highShelfBypassed = highShelfHz > kHighShelfOffThreshold;
        if (!highShelfBypassed)
            *sideHighShelf.coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf(
                sampleRate, highShelfHz, kFilterQ, juce::Decibels::decibelsToGain(shelfGainDb));
        lastHighShelfHz = highShelfHz;
        lastShelfGainDb = shelfGainDb;
    }
}

float MSWidthFiltered::sideGainDb(float frequencyHz, const AlgorithmParamValues& values, double sampleRate) noexcept
{
    double gain = values[kWidth] * 0.01;
    if (values[kBassCutoff] >= kBassCutoffOffThreshold)
        gain *= std::abs(biquadResponse(juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(sampleRate, values[kBassCutoff], kFilterQ),
                                frequencyHz, sampleRate));
    if (values[kHighShelf] <= kHighShelfOffThreshold)
        gain *= std::abs(biquadResponse(juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf(sampleRate, values[kHighShelf], kFilterQ,
                                    juce::Decibels::decibelsToGain(values[kShelfGain])),
                                frequencyHz, sampleRate));
    return juce::Decibels::gainToDecibels((float) gain, -100.0f);
}

void MSWidthFiltered::process(juce::AudioBuffer<float>& buffer, const AlgorithmParamValues& values) noexcept
{
    updateFiltersIfNeeded(values[kBassCutoff], values[kHighShelf], values[kShelfGain]);
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
    return "Mid/Side width with bass mono and a side high shelf.\n\n"
           "Like M/S Width (Broadband), but the side signal is high-pass filtered first "
           "(the bass stays mono -- low frequencies carry most of a mix's energy and "
           "translate poorly to mono playback when wide) and then high-shelved, before "
           "Width scales it.\n\n"
           + getControlsText() + "\n\n"
           "Display: the gain the side signal gets at each frequency (Width and both "
           "filters combined); the mid signal is unchanged (0 dB line). Drag the two "
           "points for the frequencies, the shelf point up/down for its gain; "
           "double-click resets.\n\n"
           "Mono-compatible: the mid signal is never changed.\n\n"
           "Source: B. Katz, \"Mastering Audio: The Art and the Science\", 3rd ed., "
           "Focal Press, 2015, ch. 3 (\"Mono Compatibility and M-S Processing\").";
}

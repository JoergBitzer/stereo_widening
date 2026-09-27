/**
 * @file MSWidthBroadband.h
 * @brief Algorithm 2.1 (planing.md), broadband case: plain M/S width control.
 *
 * M = (L+R)/2, S = (L-R)/2 (same notation as StereoMeterState and python/stereo_eval).
 * Scaling S by width and recombining (L' = M + width*S, R' = M - width*S) is the
 * textbook stereo-width control: width = 0 collapses to mono, width = 1 is the
 * identity (bit-exact passthrough, used for the null test in
 * docs/algorithms/phase3_stereo_widener.md), width = 2 doubles the side signal.
 *
 * No filtering: the whole spectrum (including the bass) is widened equally. See
 * MSWidthFiltered.h for the "bass mono" alternative that keeps low frequencies
 * centred, which exists specifically to exercise the algorithm-switch crossfade
 * (StereoWidenerAudio::processSynchronBlock) against a second, audibly different mode.
 * Stateless (no filters, no memory), so prepare()/reset() have nothing to do. Its
 * only parameter is Width.
 *
 * Reference: R. Streicher and F. A. Everest, "The New Stereo Soundbook", 3rd ed.,
 * Audio Engineering Associates, 2006 -- the M/S width technique implemented here is
 * standard mastering/mixing practice, covered in ch. 2 ("Microphone Technique") and
 * ch. 9 ("Stereo Enhancement and Manipulation"). Tangent law (see
 * hardPannedSourceAngleDeg()): V. Pulkki, "Virtual Sound Source Positioning Using
 * Vector Base Amplitude Panning", J. Audio Eng. Soc. 45(6), 1997.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include "StereoAlgorithm.h"

class MSWidthBroadband : public StereoAlgorithm
{
public:
    void prepare(double sampleRate, int maxBlockSize) override { juce::ignoreUnused(sampleRate, maxBlockSize); }
    void reset() override {}
    void process(juce::AudioBuffer<float>& buffer, const AlgorithmParamValues& values) noexcept override;

    enum ParamIndex { kWidth = 0 };
    std::vector<AlgorithmParamSpec> getParamSpecs() const override
    {
        return { AlgorithmParamSpec::width("broadbandWidth") };
    }

    // Pure math for the GUI's playground (no GUI dependency).

    /** Side-signal gain in dB for widthPercent; -100 dB stands for -inf (0 % = mono). */
    static float sideGainDb(float widthPercent) noexcept
    {
        return juce::Decibels::gainToDecibels(widthPercent * 0.01f, -100.0f);
    }

    /** Where a hard-panned source (e.g. L only) is heard after widening, in degrees from
     *  the centre, for loudspeakers at +-kSpeakerAngleDeg. For an L-only input, M = S
     *  = L/2, so L' = L(1+w)/2 and R' = L(1-w)/2; the stereophonic tangent law,
     *  tan(phi) / tan(phi0) = (L'-R') / (L'+R'), then gives tan(phi) = w * tan(phi0).
     *  w = 1 keeps the source at the loudspeaker; w > 1 moves it beyond (R' is then in
     *  antiphase). */
    static float hardPannedSourceAngleDeg(float widthPercent) noexcept
    {
        const float speakerRad = juce::degreesToRadians(kSpeakerAngleDeg);
        return juce::radiansToDegrees(std::atan(widthPercent * 0.01f * std::tan(speakerRad)));
    }

    static constexpr float kSpeakerAngleDeg = 30.0f; // standard stereo triangle

    const char* getName() const noexcept override { return "M/S Width (Broadband)"; }
    juce::String getDescription() const override;
    bool isMonoSafe() const noexcept override { return true; }
    int getLatencySamples() const noexcept override { return 0; }
};

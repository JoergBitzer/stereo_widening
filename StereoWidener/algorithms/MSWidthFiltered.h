/**
 * @file MSWidthFiltered.h
 * @brief Algorithm 2.1 (planing.md), "bass mono + side shelf" case: M/S width control
 *        with the side signal high-pass filtered (bass mono) and high-shelved (air)
 *        before the width scaling.
 *
 * Same M/S recombination as MSWidthBroadband, but the side signal S first runs through
 * two filters:
 *  1. A high-pass at the Bass Cutoff frequency (StereoWidenerGUI's left aux knob,
 *     params.auxLeft). Content below the cutoff is removed from S entirely -- forced
 *     into M, i.e. mono -- while content above it passes through to the width control
 *     unchanged. This is the standard "keep the bass mono" mastering trick: low
 *     frequencies translate better to mono playback and carry most of a mix's energy,
 *     so collapsing them to the centre is usually inaudible as a width change but
 *     avoids phase-cancellation problems on mono sum.
 *  2. A high shelf at the High Shelf frequency (the right aux knob, params.auxRight),
 *     boosting the side signal above that frequency by highShelfGainDb (default 3 dB,
 *     see setHighShelfGainDb()). This restores some of the high-frequency "air"/
 *     openness that step 1 and the width control together tend to reduce perceptually.
 *     Only the frequency is exposed as an automatable parameter for this first version
 *     (see docs/algorithms/phase3_stereo_widener.md); the gain is a user-configurable
 *     default from GlobalSettings (StereoWidener/GlobalSettings.h, Phase 4), set once
 *     by StereoWidenerAudio's constructor -- not itself an automatable parameter.
 *
 * Both knobs have an "off" zone past their normal range, a common pattern for a cutoff
 * control: dragging Bass Cutoff below kBassCutoffOffThreshold (its range extends a bit
 * further down than that) bypasses the high-pass entirely (the side signal passes
 * through untouched, no bass-mono effect at all); dragging High Shelf above
 * kHighShelfOffThreshold bypasses the shelf. StereoWidener.h's g_paramBassCutoff /
 * g_paramHighShelfFreq and StereoWidenerGUI both read these two constants, so the
 * parameter range, the DAW-visible "Off" text, and the actual DSP bypass all agree on
 * the same threshold.
 *
 * Both filters are recomputed only when their controlling parameter actually changes
 * (see updateFiltersIfNeeded() in the .cpp), not every sample, since juce::dsp::IIR
 * coefficient calculation is too expensive to redo unconditionally at audio rate.
 *
 * Deliberately the second, audibly different algorithm alongside MSWidthBroadband, so
 * switching between the two exercises StereoWidenerAudio's crossfade
 * (see docs/algorithms/phase3_stereo_widener.md).
 *
 * Reference: B. Katz, "Mastering Audio: The Art and the Science", 3rd ed., Focal Press,
 * 2015, ch. 3 ("Mono Compatibility and M-S Processing") -- bass-mono and M/S width
 * shaping as standard mastering practice.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_dsp/juce_dsp.h>
#include "StereoAlgorithm.h"

class MSWidthFiltered : public StereoAlgorithm
{
public:
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, const StereoAlgorithmParams& params) noexcept override;

    const char* getName() const noexcept override { return "M/S Width (Filtered / Bass Mono)"; }
    juce::String getDescription() const override;
    AuxKnobInfo getAuxLeftInfo() const noexcept override { return { true, "Bass Cutoff" }; }
    AuxKnobInfo getAuxRightInfo() const noexcept override { return { true, "High Shelf" }; }
    bool isMonoSafe() const noexcept override { return true; }
    int getLatencySamples() const noexcept override { return 0; }

    /** User-configurable default (GlobalSettings, Phase 4), not an automatable
     *  parameter -- see the file header. Safe to call at any time, including after
     *  processing has started; takes effect on the next process() call. */
    void setHighShelfGainDb(float gainDb) noexcept
    {
        highShelfGainDb = gainDb;
        lastHighShelfHz = -1.0f; // force updateFiltersIfNeeded() to recompute with the new gain
    }

    static constexpr float kFilterQ = 0.70710678f;  // Butterworth (maximally flat)

    // "Off" zone thresholds, see the file header. The parameter ranges (StereoWidener.h)
    // extend a bit past these on the "off" side, e.g. Bass Cutoff's range starts below
    // kBassCutoffOffThreshold, so there is room on the knob to reach the off position.
    static constexpr float kBassCutoffOffThreshold = 40.0f;
    static constexpr float kHighShelfOffThreshold = 16000.0f;

private:
    void updateFiltersIfNeeded(float bassCutoffHz, float highShelfHz) noexcept;

    double sampleRate = 48000.0;
    float highShelfGainDb = 3.0f; // compiled-in fallback; see setHighShelfGainDb() and GlobalSettings
    float lastBassCutoffHz = -1.0f;
    float lastHighShelfHz = -1.0f;
    bool bassCutoffBypassed = false;
    bool highShelfBypassed = false;

    juce::dsp::IIR::Filter<float> sideHighpass;
    juce::dsp::IIR::Filter<float> sideHighShelf;
};

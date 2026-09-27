/**
 * @file MSWidthFiltered.h
 * @brief Algorithm 2.1 (planing.md), "bass mono + side shelf" case: M/S width control
 *        with the side signal high-pass filtered (bass mono) and high-shelved (air)
 *        before the width scaling.
 *
 * Same M/S recombination as MSWidthBroadband, but the side signal S first runs through
 * two filters:
 *  1. A high-pass at the Bass Cutoff frequency (kBassCutoff). Content below the
 *     cutoff is removed from S entirely -- forced into M, i.e. mono -- while content
 *     above it passes through to the width control unchanged. This is the standard
 *     "keep the bass mono" mastering trick: low frequencies translate better to mono
 *     playback and carry most of a mix's energy, so collapsing them to the centre is
 *     usually inaudible as a width change but avoids phase-cancellation problems on
 *     mono sum.
 *  2. A high shelf at the High Shelf frequency (kHighShelf), changing the side
 *     signal above that frequency by Shelf Gain (kShelfGain, -6..+6 dB, default
 *     +3 dB). A boost restores some of the high-frequency "air"/openness that step 1
 *     and the width control together tend to reduce perceptually. (Until v0.1.18 the
 *     gain was a fixed default from the global settings file, not a parameter.)
 *
 * Both frequency parameters have an "off" zone past their normal range, a common
 * pattern for a cutoff control: dragging Bass Cutoff below kBassCutoffOffThreshold
 * (its range extends a bit further down than that) bypasses the high-pass entirely
 * (the side signal passes through untouched, no bass-mono effect at all); dragging
 * High Shelf above kHighShelfOffThreshold bypasses the shelf. getParamSpecs() uses the
 * same two constants for the displayed "Off" text, so the parameter range, the
 * DAW-visible text, and the actual DSP bypass all agree on the same threshold.
 *
 * Bass Cutoff is linear on purpose: a skewed mapping would give the low end -- exactly
 * where the narrow 30-40 Hz Off zone sits -- disproportionately much of the knob's
 * rotation. High Shelf spans more than four octaves (1-16.5 kHz), so it uses a true
 * log mapping instead; linear would cram the useful 1-4 kHz into a sliver of the knob.
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
    void process(juce::AudioBuffer<float>& buffer, const AlgorithmParamValues& values) noexcept override;

    enum ParamIndex { kWidth = 0, kBassCutoff, kHighShelf, kShelfGain };
    std::vector<AlgorithmParamSpec> getParamSpecs() const override
    {
        // Both frequency controls default to Off, so the algorithm starts out identical
        // to MSWidthBroadband until the user dials them in.
        return {
            AlgorithmParamSpec::width("filteredWidth"),
            AlgorithmParamSpec::linear("bassCutoff", "Bass Cutoff", "Hz", 30.0f, 500.0f, 30.0f)
                .withOffBelow(kBassCutoffOffThreshold),
            AlgorithmParamSpec::logFrequency("highShelfFreq", "High Shelf", 1000.0f, 16500.0f, 16500.0f)
                .withOffAbove(kHighShelfOffThreshold),
            AlgorithmParamSpec::linear("highShelfGain", "Shelf Gain", "dB", -6.0f, 6.0f, 3.0f, 1)
        };
    }

    /** Gain in dB that the side signal gets at frequencyHz for the given parameter
     *  values: Width plus both filter stages, from the same filter designs process()
     *  uses. Pure math for the GUI's response display; -100 dB stands for -inf. */
    static float sideGainDb(float frequencyHz, const AlgorithmParamValues& values, double sampleRate) noexcept;

    const char* getName() const noexcept override { return "M/S Width (Filtered / Bass Mono)"; }
    juce::String getDescription() const override;
    bool isMonoSafe() const noexcept override { return true; }
    int getLatencySamples() const noexcept override { return 0; }

    static constexpr float kFilterQ = 0.70710678f;  // Butterworth (maximally flat)

    // "Off" zone thresholds, see the file header. The parameter ranges (getParamSpecs())
    // extend a bit past these on the "off" side, e.g. Bass Cutoff's range starts below
    // kBassCutoffOffThreshold, so there is room on the knob to reach the off position.
    static constexpr float kBassCutoffOffThreshold = 40.0f;
    static constexpr float kHighShelfOffThreshold = 16000.0f;

private:
    void updateFiltersIfNeeded(float bassCutoffHz, float highShelfHz, float shelfGainDb) noexcept;

    double sampleRate = 48000.0;
    float lastBassCutoffHz = -1.0f;
    float lastHighShelfHz = -1.0f;
    float lastShelfGainDb = 0.0f;
    bool bassCutoffBypassed = false;
    bool highShelfBypassed = false;

    juce::dsp::IIR::Filter<float> sideHighpass;
    juce::dsp::IIR::Filter<float> sideHighShelf;
};

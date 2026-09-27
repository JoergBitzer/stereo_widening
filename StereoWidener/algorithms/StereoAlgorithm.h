/**
 * @file StereoAlgorithm.h
 * @brief Common interface for a stereo-widening algorithm, so StereoWidenerAudio can
 *        switch between algorithms without knowing what each one does internally.
 *
 * One small class per algorithm (see planing.md section 5, "code rules": the codebase
 * is also a teaching example). Each algorithm declares its own parameters
 * (getParamSpecs()) and processes a stereo buffer in place from their current values.
 *
 * Deliberately free of any GUI dependency: tools/widener_render compiles these files
 * into a console app linked only against juce_audio_basics/juce_dsp. The plugin's GUI
 * (StereoWidener/AlgorithmPlayground.h) reads the same parameter specs to build its
 * controls.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <array>
#include <limits>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>

// Upper bound on getParamSpecs().size() for any algorithm -- sizes the fixed array
// passed to process(), so the audio thread never allocates. Raise it if an algorithm
// ever needs more.
static constexpr int kMaxAlgorithmParams = 8;

/** One parameter of one algorithm, as plain data. StereoWidenerAudio turns every
 *  algorithm's specs into APVTS parameters; the values reach process() in the units
 *  declared here (e.g. "%" arrives as 0..100, "Hz" as Hz) -- converting to whatever
 *  the DSP needs internally is the algorithm's own job. */
struct AlgorithmParamSpec
{
    enum class Scale
    {
        Linear,       // equal value steps get equal knob rotation
        LogFrequency  // equal frequency *ratios* get equal rotation (several octaves)
    };

    const char* id = "";      // APVTS parameter ID, unique across all algorithms
    const char* name = "";    // shown as the control's label
    const char* unit = "";    // appended to the displayed value, e.g. "%", "Hz", "ms"
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    int numDecimalPlaces = 0; // display precision; also the step (10^-n) for Linear
    Scale scale = Scale::Linear;

    // "Off" zones: values below offBelow / above offAbove are displayed as "Off" (the
    // algorithm bypasses that stage there, e.g. MSWidthFiltered's Bass Cutoff).
    float offBelow = std::numeric_limits<float>::lowest();
    float offAbove = std::numeric_limits<float>::max();

    static AlgorithmParamSpec linear(const char* id, const char* name, const char* unit,
                                     float minValue, float maxValue, float defaultValue,
                                     int numDecimalPlaces = 0)
    {
        AlgorithmParamSpec s;
        s.id = id; s.name = name; s.unit = unit;
        s.minValue = minValue; s.maxValue = maxValue; s.defaultValue = defaultValue;
        s.numDecimalPlaces = numDecimalPlaces;
        return s;
    }

    static AlgorithmParamSpec logFrequency(const char* id, const char* name,
                                           float minValue, float maxValue, float defaultValue)
    {
        auto s = linear(id, name, "Hz", minValue, maxValue, defaultValue, 0);
        s.scale = Scale::LogFrequency;
        return s;
    }

    AlgorithmParamSpec withOffBelow(float threshold) const { auto s = *this; s.offBelow = threshold; return s; }
    AlgorithmParamSpec withOffAbove(float threshold) const { auto s = *this; s.offAbove = threshold; return s; }

    // The standard 0-200 % width control: 0 = mono, 100 = unchanged, 200 = double the
    // side signal. Every algorithm that has one declares its own (own ID), so each
    // keeps its own setting.
    static AlgorithmParamSpec width(const char* id, const char* name = "Width")
    {
        return linear(id, name, "%", 0.0f, 200.0f, 100.0f);
    }
};

/** Current values for process(), indexed like getParamSpecs() (each algorithm defines
 *  an enum for its own indices). Entries past getParamSpecs().size() are unused. */
using AlgorithmParamValues = std::array<float, kMaxAlgorithmParams>;

class StereoAlgorithm
{
public:
    virtual ~StereoAlgorithm() = default;

    /** Call from prepareToPlay before the first process() call, and again whenever the
     *  sample rate or block size changes. */
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;

    /** Clears any internal filter/delay state. Called when this algorithm becomes the
     *  active one again after being switched away from, so stale state from before the
     *  switch never leaks into new audio. */
    virtual void reset() = 0;

    /** Processes buffer in place. buffer always has exactly 2 channels (L, R); the
     *  caller (StereoWidenerAudio) is responsible for that, so algorithms don't each
     *  have to guard against mono/multichannel input. */
    virtual void process(juce::AudioBuffer<float>& buffer, const AlgorithmParamValues& values) noexcept = 0;

    /** This algorithm's parameters, in the index order process() reads them. Called
     *  at setup time only (parameter creation, GUI construction), never per block. */
    virtual std::vector<AlgorithmParamSpec> getParamSpecs() const = 0;

    /** Display name, shown in the algorithm selector. */
    virtual const char* getName() const noexcept = 0;

    /** Explanation shown in the "?" help popup next to the algorithm selector: what the
     *  algorithm does and why, with a citation to a written source -- this is a teaching
     *  tool as much as a plugin (planing.md section 5). Plain text, may contain '\n'. */
    virtual juce::String getDescription() const = 0;

    /** Mastering profile (plan2.md section 2) only offers mono-safe algorithms. */
    virtual bool isMonoSafe() const noexcept = 0;

    /** Extra latency this algorithm adds, in samples, beyond StereoWidenerAudio's own
     *  (currently zero -- see StereoWidener.h). Every algorithm so far reports 0; a
     *  future linear-phase crossover would not. */
    virtual int getLatencySamples() const noexcept = 0;
};

/**
 * @file MultibandWidth.h
 * @brief Algorithm 2.7 (planing.md): multiband width, 4 bands / 3 Linkwitz-Riley 4th
 *        order (LR4) crossovers, band 1 ("bass") always forced mono.
 *
 * planing.md 2.7: "Crossover (Linkwitz-Riley LR4, 3-4 bands, which sums to allpass or
 * flat) and an M/S width per band. Bass mono comes built in."
 *
 * Mid/side split once (filtering commutes with the linear M/S transform, so this is
 * cheaper than splitting L and R separately and computing M/S per band), each run
 * through the same 4-band LR4 tree:
 *
 *     M = (L+R)/2, S = (L-R)/2
 *     M = M1+M2+M3+M4, S = S1+S2+S3+S4        (per-band decomposition, see .cpp)
 *     S' = Width * (0*S1 + width2*S2 + width3*S3 + width4*S4)   -- band 1 forced mono
 *     M' = M   (unchanged, same convention as every width algorithm in this project)
 *     L' = M' + S',  R' = M' - S'
 *
 * A NAIVE tree split (each band = whatever LP4/HP4 combination its path through the
 * tree left it with) does NOT reconstruct flat -- band 1 only passed through ONE
 * crossover's phase response, band 2 passed through two, band 3/4 through all three;
 * summing bands with mismatched accumulated phase causes real, measurable magnitude
 * ripple. Every band is therefore phase-compensated (extra LR4 allpass stages for
 * bands that "skipped" a crossover on their way through the tree) so all four bands
 * carry the same total phase before summing -- see splitBands() in the .cpp, and
 * python/algorithms/multiband_width.py's module docstring for the full derivation and
 * the measured before/after numbers (~0.5-0.6 dB mono-sum colouration without
 * compensation, ~0.01-0.04 dB with it).
 *
 * Mono-safe by construction: M' is never touched, so L'+R' = 2M always, regardless of
 * any width setting -- same guarantee as algorithm 2.4's comb.
 *
 * Parameters (getParamSpecs()): the three crossover frequencies and the widths of
 * bands 2-4 (0-200 %, 100 % = unchanged). Band 1 is always mono, see above. There is
 * no overall Width: until v0.1.20 a master Width scaled all three band widths on top,
 * which made "how wide is this band" depend on two knobs -- removed (at its default of
 * 100 % it had no effect, so default processing is unchanged).

 * Reference: S. Linkwitz, "Active Crossover Networks for Noncoincident Drivers",
 * J. Audio Eng. Soc., 1976 (the LR4 crossover itself); B. Katz, "Mastering Audio: The
 * Art and the Science", 3rd ed., Focal Press, 2015, ch. 3 (multiband width in
 * mastering practice).
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_dsp/juce_dsp.h>
#include "StereoAlgorithm.h"

class MultibandWidth : public StereoAlgorithm
{
public:
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer, const AlgorithmParamValues& values) noexcept override;

    // The crossover ranges overlap a little between neighbours on purpose: process()
    // sorts and separates the three raw values defensively
    // (updateFrequenciesIfNeeded()), and the GUI additionally stops each crossover at
    // its neighbour while dragging, so independent per-parameter ranges are simpler
    // than keeping three parameter ranges mutually exclusive.
    enum ParamIndex { kFreq1 = 0, kFreq2, kFreq3, kWidth2, kWidth3, kWidth4 };
    std::vector<AlgorithmParamSpec> getParamSpecs() const override
    {
        return {
            AlgorithmParamSpec::logFrequency("multibandFreq1", "Crossover 1", 40.0f, 400.0f, 150.0f),
            AlgorithmParamSpec::logFrequency("multibandFreq2", "Crossover 2", 200.0f, 4000.0f, 1500.0f),
            AlgorithmParamSpec::logFrequency("multibandFreq3", "Crossover 3", 1000.0f, 18000.0f, 6000.0f),
            AlgorithmParamSpec::width("multibandWidth2", "Band 2 Width"),
            AlgorithmParamSpec::width("multibandWidth3", "Band 3 Width"),
            AlgorithmParamSpec::width("multibandWidth4", "Band 4 Width")
        };
    }

    // Adjacent crossovers always stay at least this ratio apart (5 %): enforced by
    // process() on whatever values arrive, and by the GUI while dragging.
    static constexpr float kMinCrossoverRatio = 1.05f;

    const char* getName() const noexcept override { return "Multiband Width"; }
    juce::String getDescription() const override;
    bool isMonoSafe() const noexcept override { return true; }
    int getLatencySamples() const noexcept override { return 0; }

    static constexpr float kFilterQ = 0.70710678f; // Butterworth, matches every other algorithm's filters

private:
    // One LR4 (4th-order) lowpass or highpass: two identical cascaded 2nd-order
    // Butterworth biquads at the same frequency (same construction as the Python
    // reference's _lr4_low()/_lr4_high()).
    struct Lr4Filter
    {
        juce::dsp::IIR::Filter<float> stage1, stage2;
        void prepare(const juce::dsp::ProcessSpec& spec) { stage1.prepare(spec); stage2.prepare(spec); }
        void reset() { stage1.reset(); stage2.reset(); }
        void setCoefficients(juce::dsp::IIR::Coefficients<float>::Ptr c) noexcept
        {
            *stage1.coefficients = *c;
            *stage2.coefficients = *c;
        }
        float process(float x) noexcept { return stage2.processSample(stage1.processSample(x)); }
    };

    // One crossover: LP4 and HP4 at the same frequency, low+high ~= input with flat
    // magnitude (the defining LR4 property) but real phase/group-delay distortion.
    struct Lr4Split
    {
        Lr4Filter low, high;
        void prepare(const juce::dsp::ProcessSpec& spec) { low.prepare(spec); high.prepare(spec); }
        void reset() { low.reset(); high.reset(); }
        void setFrequency(double sampleRate, float freqHz) noexcept
        {
            low.setCoefficients(juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, freqHz, kFilterQ));
            high.setCoefficients(juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, freqHz, kFilterQ));
        }
    };

    // Phase-compensation building block: LP4(x) + HP4(x), see the file header.
    struct Lr4Allpass
    {
        Lr4Split split;
        void prepare(const juce::dsp::ProcessSpec& spec) { split.prepare(spec); }
        void reset() { split.reset(); }
        void setFrequency(double sampleRate, float freqHz) noexcept { split.setFrequency(sampleRate, freqHz); }
        float process(float x) noexcept { return split.low.process(x) + split.high.process(x); }
    };

    // One signal's (M or S) full phase-compensated 4-band split -- two independent
    // instances below, one per signal, since filtering commutes with the M/S
    // transform (see the file header).
    struct BandSplitter
    {
        Lr4Split split1, split2, split3;     // the actual 3 crossovers
        Lr4Allpass comp1a, comp1b, comp2;    // phase compensation, see the file header

        void prepare(const juce::dsp::ProcessSpec& spec)
        {
            split1.prepare(spec); split2.prepare(spec); split3.prepare(spec);
            comp1a.prepare(spec); comp1b.prepare(spec); comp2.prepare(spec);
        }
        void reset()
        {
            split1.reset(); split2.reset(); split3.reset();
            comp1a.reset(); comp1b.reset(); comp2.reset();
        }
        void setFrequencies(double sampleRate, float f1, float f2, float f3) noexcept
        {
            split1.setFrequency(sampleRate, f1);
            split2.setFrequency(sampleRate, f2);
            split3.setFrequency(sampleRate, f3);
            comp1a.setFrequency(sampleRate, f2);
            comp1b.setFrequency(sampleRate, f3);
            comp2.setFrequency(sampleRate, f3);
        }
        void process(float x, float& band1, float& band2, float& band3, float& band4) noexcept
        {
            const float low1 = split1.low.process(x);
            const float high1 = split1.high.process(x);
            const float low2 = split2.low.process(high1);
            const float high2 = split2.high.process(high1);
            const float low3 = split3.low.process(high2);
            const float high3 = split3.high.process(high2);

            band1 = comp1b.process(comp1a.process(low1)); // skipped the freq2 and freq3 splits
            band2 = comp2.process(low2);                  // skipped the freq3 split
            band3 = low3;                                  // already passed freq1, freq2, freq3
            band4 = high3;                                 // already passed freq1, freq2, freq3
        }
    };

    void updateFrequenciesIfNeeded(float freq1, float freq2, float freq3) noexcept;

    double sampleRate = 48000.0;
    float lastFreq1 = -1.0f, lastFreq2 = -1.0f, lastFreq3 = -1.0f;

    BandSplitter midSplitter, sideSplitter;
};

#pragma once

#include <memory>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "tools/SynchronBlockProcessor.h"
#include "PluginSettings.h"
#include "AlgorithmHelpPanel.h"
#include "GlobalSettings.h"
#include "UtilityProcessor.h"
#include "algorithms/StereoAlgorithm.h"
#include "algorithms/MSWidthBroadband.h"
#include "algorithms/MSWidthFiltered.h"
#include "algorithms/ComplementaryComb.h"
#include "algorithms/AllpassDecorrelation.h"
#include "../shared/metering/StereoMeterState.h"
#include "../shared/metering/GoniometerComponent.h"
#include "../shared/metering/LevelMeterComponent.h"

class StereoWidenerAudioProcessor;

// Width: 0 % collapses the side signal to mono, 100 % is unity (unchanged from input),
// 200 % doubles the side signal. See algorithms/MSWidthBroadband.h.
const struct
{
	const std::string ID = "width";
	const std::string name = "Width";
	const std::string unitName = "%";
	const float minValue = 0.0f;
	const float maxValue = 200.0f;
	const float defaultValue = 100.0f;
	const float skew = 1.0f;
	const int numDecimalPlaces = 0;
}g_paramWidth;

// StereoWidenerGUI's left aux knob when the active algorithm enables it (currently only
// MSWidthFiltered -- see AuxKnobInfo). The range extends below
// MSWidthFiltered::kBassCutoffOffThreshold (40 Hz) on purpose: that bottom stretch is
// the knob's "Off" position (filter fully bypassed, see MSWidthFiltered.h), a common
// pattern for a cutoff control -- turn it low enough and the effect switches off
// instead of just approaching an extreme frequency. skew is 1.0 (linear) deliberately:
// with JUCE's skew formula (value = start + (end-start)*proportion^(1/skew), see
// NormalisableRange), any skew < 1 gives the LOW end of the range disproportionately
// more of the knob's rotation -- exactly the low end this Off zone sits in, which
// first made it occupy a full third of the knob's travel instead of a small sliver
// right at the minimum. Linear skew keeps rotation proportional to the (deliberately
// narrow, 10 Hz) Off zone's actual share of the total range.
const struct
{
	const std::string ID = "bassCutoff";
	const std::string name = "Bass Cutoff";
	const std::string unitName = "Hz";
	const float minValue = 30.0f;  // 30-40 Hz: "Off" zone, see the comment above
	const float maxValue = 500.0f;
	// Off by default (neutral/pass-through: MSWidthFiltered reduces to plain width,
	// same as MSWidthBroadband, until the user dials this in) -- also the value a
	// double-click on the knob resets to, see GlobalSettings.h.
	const float defaultValue = minValue;
	const float skew = 1.0f;
	const int numDecimalPlaces = 0;
}g_paramBassCutoff;

// StereoWidenerGUI's right aux knob when the active algorithm enables it. Mirrors
// g_paramBassCutoff in having an "Off" zone above MSWidthFiltered::kHighShelfOffThreshold
// (16 kHz), but differs in one respect: linear is fine for Bass Cutoff's much narrower
// practical range, but not here -- 1000-16500 Hz spans more than four octaves, and a
// linear (or simple power-law skewed) mapping would cram the musically useful low end
// (1-4 kHz) into a sliver of the knob while wasting most of the rotation on the top
// octave. This parameter is therefore built with makeLogFrequencyParameterWithOff()
// (StereoWidener.cpp) instead of makeFrequencyParameterWithOff(): a *true* logarithmic
// mapping (equal Hz ratios get equal rotation, not just "some skew"), so the field below
// named `skew` is unused for this parameter -- kept only so the struct still matches the
// same shape as the other g_param* definitions.
const struct
{
	const std::string ID = "highShelfFreq";
	const std::string name = "High Shelf";
	const std::string unitName = "Hz";
	const float minValue = 1000.0f;
	const float maxValue = 16500.0f; // 16000-16500 Hz: "Off" zone, see the comment above
	// Off by default, same reasoning as g_paramBassCutoff's defaultValue above.
	const float defaultValue = maxValue;
	const float skew = 1.0f; // unused, see the comment above
	const int numDecimalPlaces = 0;
}g_paramHighShelfFreq;

// ComplementaryComb's aux knobs (Phase 5, algorithm 2.4). Range from planing.md 2.4:
// "D ~= 5-20 ms and g ~= 0.3-0.7"; Gain's range is widened to the full 0-100 % (g's
// "usable" spec range sits comfortably in the middle) so the user isn't limited to a
// narrower band than the Width knob's own 0-200 %.
const struct
{
	const std::string ID = "combDelay";
	const std::string name = "Delay";
	const std::string unitName = "ms";
	const float minValue = 5.0f;
	const float maxValue = 20.0f;
	// No "neutral" value of its own (see g_paramCombGain below -- Gain = 0 % already
	// makes the whole algorithm neutral regardless of Delay), so this just starts at a
	// representative mid-range value for when the user raises Gain.
	const float defaultValue = 10.0f;
	const float skew = 1.0f;
	const int numDecimalPlaces = 1;
}g_paramCombDelay;

const struct
{
	const std::string ID = "combGain";
	const std::string name = "Gain";
	const std::string unitName = "%";
	const float minValue = 0.0f;
	const float maxValue = 100.0f;
	// 0 % is neutral (no delayed contribution added to S -- same output as
	// MSWidthBroadband at the same Width) until the user dials this in, same reasoning
	// as g_paramBassCutoff/g_paramHighShelfFreq's Off defaults above.
	const float defaultValue = minValue;
	const float skew = 1.0f;
	const int numDecimalPlaces = 0;
}g_paramCombGain;

// AllpassDecorrelation's aux knobs (Phase 5, algorithm 2.5). See AllpassDecorrelation.h
// for the formula; Spread's range (0-2 octaves internally, see
// AllpassDecorrelation::kMaxSpreadOctaves) is exposed here as a plain 0-100 % knob, same
// convention as g_paramCombGain, with the actual octave conversion done in
// StereoWidenerAudio::paramsFor().
const struct
{
	const std::string ID = "allpassAmount";
	const std::string name = "Amount";
	const std::string unitName = "%";
	const float minValue = 0.0f;
	const float maxValue = 100.0f;
	// 0 % is neutral (exact bypass, algebraically -- see AllpassDecorrelation.h) until
	// the user dials this in, same reasoning as g_paramCombGain's default above.
	const float defaultValue = minValue;
	const float skew = 1.0f;
	const int numDecimalPlaces = 0;
}g_paramAllpassAmount;

const struct
{
	const std::string ID = "allpassSpread";
	const std::string name = "Spread";
	const std::string unitName = "%";
	const float minValue = 0.0f;
	const float maxValue = 100.0f;
	// No "neutral" value of its own -- inert whenever Amount = 0, same reasoning as
	// g_paramCombDelay's default above.
	const float defaultValue = 50.0f;
	const float skew = 1.0f;
	const int numDecimalPlaces = 0;
}g_paramAllpassSpread;

constexpr const char* g_paramAlgorithmID = "algorithm";
constexpr const char* g_paramAlgorithmName = "Algorithm";

// Must stay in the same order as the algorithm instances StereoWidenerAudio's
// constructor creates in algorithms/ -- see the comment there, and
// StereoWidenerGUI::auxLeftParamIdFor()/auxRightParamIdFor() (StereoWidener.cpp), which
// must also stay in sync with this order.
const juce::StringArray g_algorithmNames {
    "M/S Width (Broadband)",
    "M/S Width (Filtered / Bass Mono)",
    "Complementary Comb (Pseudo-Stereo)",
    "Allpass Decorrelation"
};

// ---- Utilities (Phase 4 step 2: planing.md 2.13 + 2.2) -------------------------
// Applied by UtilityProcessor after the selected width algorithm, regardless of which
// one is active -- see UtilityProcessor.h for the processing order and rationale.

const struct
{
	const std::string ID = "rotation";
	const std::string name = "Rotation";
	const std::string unitName = "deg";
	const float minValue = -45.0f;
	const float maxValue = 45.0f;
	const float defaultValue = 0.0f;
	const float skew = 1.0f;
	const int numDecimalPlaces = 1;
}g_paramRotation;

const struct
{
	const std::string ID = "balance";
	const std::string name = "Balance";
	const std::string unitName = "%";
	const float minValue = -100.0f;
	const float maxValue = 100.0f;
	const float defaultValue = 0.0f;
	const float skew = 1.0f;
	const int numDecimalPlaces = 0;
}g_paramBalance;

const struct
{
	const std::string ID = "invertL";
	const std::string name = "Invert L";
}g_paramInvertL;

const struct
{
	const std::string ID = "invertR";
	const std::string name = "Invert R";
}g_paramInvertR;

const struct
{
	const std::string ID = "swapLR";
	const std::string name = "Swap L/R";
}g_paramSwapLR;

constexpr const char* g_paramMonitorModeID = "monitorMode";
constexpr const char* g_paramMonitorModeName = "Monitor";
// indices must match UtilityParams::MonitorMode (UtilityProcessor.h)
const juce::StringArray g_monitorModeNames { "Normal", "Mono Check (L+R)", "Solo Side (S)" };

class StereoWidenerAudio : public SynchronBlockProcessor
{
public:
    StereoWidenerAudio(StereoWidenerAudioProcessor* processor);
    void prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels);
    virtual int processSynchronBlock(juce::AudioBuffer<float>&, juce::MidiBuffer& midiMessages, int NrOfBlocksSinceLastProcessBlock);

    // parameter handling
  	void addParameter(std::vector < std::unique_ptr<juce::RangedAudioParameter>>& paramVector);
    void prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState>&  vts);

    // Neither algorithm currently adds latency (see StereoAlgorithm::getLatencySamples());
    // a future linear-phase mode would need to report the max over all algorithms here
    // instead, and StereoWidenerAudioProcessor would need to call setLatencySamples()
    // again on every algorithm switch (plan2.md Phase 4, "Latency").
    int getLatency(){return 0;};

    // Read-only access for the GUI (help popup text, aux-knob labels/enabled state):
    // deliberately keyed by the algorithm's own index in g_algorithmNames/the Algorithm
    // choice parameter, not by StereoWidenerAudio's internal (possibly mid-crossfade)
    // active/target index -- the GUI only ever needs "what does the *selected* one say".
    int getNumAlgorithms() const noexcept { return (int) m_algorithms.size(); }
    const StereoAlgorithm& getAlgorithm(int index) const noexcept { return *m_algorithms[(size_t) index]; }

    // StereoWidenerAudioProcessor reads/writes this from its own constructor/destructor
    // (GUI scale factor default and persistence) -- GlobalSettings must
    // stay owned here rather than by the processor, since the processor's constructor
    // builds this whole object (m_algo) via its member-initializer-list before its own
    // constructor *body* runs, so only members already constructed by then (i.e. ones
    // that live inside m_algo, not siblings of it) are safe to use at that point.
    GlobalSettings& getGlobalSettings() noexcept { return m_globalSettings; }

    StereoMeterState m_meterStateIn;
    StereoMeterState m_meterStateOut;

private:
	StereoWidenerAudioProcessor* m_processor;
    double m_sampleRate = 44100.0;

    juce::AudioParameterFloat* m_widthParam = nullptr;
    juce::AudioParameterFloat* m_bassCutoffParam = nullptr;
    juce::AudioParameterFloat* m_highShelfFreqParam = nullptr;
    juce::AudioParameterFloat* m_combDelayParam = nullptr;
    juce::AudioParameterFloat* m_combGainParam = nullptr;
    juce::AudioParameterFloat* m_allpassAmountParam = nullptr;
    juce::AudioParameterFloat* m_allpassSpreadParam = nullptr;
    juce::AudioParameterChoice* m_algorithmParam = nullptr;

    // Builds this block's params for algorithmIndex, sourced from whichever aux
    // parameters *that* algorithm actually uses (each algorithm may have its own --
    // see the g_paramBassCutoff/g_paramCombDelay comments). Needed because a crossfade
    // runs two DIFFERENT algorithms in the same block, each needing its own aux values,
    // not one shared pair -- see processSynchronBlock().
    StereoAlgorithmParams paramsFor(int algorithmIndex, float width) const noexcept;

    juce::AudioParameterFloat* m_rotationParam = nullptr;
    juce::AudioParameterFloat* m_balanceParam = nullptr;
    juce::AudioParameterBool* m_invertLParam = nullptr;
    juce::AudioParameterBool* m_invertRParam = nullptr;
    juce::AudioParameterBool* m_swapLRParam = nullptr;
    juce::AudioParameterChoice* m_monitorModeParam = nullptr;
    UtilityProcessor m_utilityProcessor;

    std::vector<std::unique_ptr<StereoAlgorithm>> m_algorithms;
    int m_activeIndex = 0;

    // loaded once in the constructor (plan2.md Phase 4, "Global settings file"), not
    // re-read afterwards -- see GlobalSettings.h
    GlobalSettings m_globalSettings;

    // Equal-power crossfade between the outgoing and incoming algorithm on a switch
    // (plan2.md Phase 3: "Algorithm switching with an equal-power crossfade
    // (20-50 ms)"), so changing modes never clicks. Both algorithms process the whole
    // block every tick while a crossfade is in progress (only while m_crossfading is
    // true -- negligible extra CPU the rest of the time, since only the active
    // algorithm runs then).
    juce::SmoothedValue<float> m_crossfadeProgress;
    bool m_crossfading = false;
    int m_targetIndex = 0;
    juce::AudioBuffer<float> m_crossfadeScratch;
    static constexpr float kCrossfadeSeconds = 0.03f;
};

class StereoWidenerGUI : public juce::Component
{
public:
	StereoWidenerGUI(StereoWidenerAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts);

	void paint(juce::Graphics& g) override;
	void resized() override;
private:
    void showAlgorithmHelp();
    // Relabels and rebinds the two aux knobs for whichever algorithm is now selected
    // (see StereoAlgorithm::getAuxLeftInfo()/getAuxRightInfo() for the label/enabled
    // state, and bindAuxKnob() for the rebinding -- each algorithm may have its own aux
    // parameters, e.g. MSWidthFiltered's Bass Cutoff/High Shelf vs. ComplementaryComb's
    // Delay/Gain, so the knob *positions* are shared but which parameter each one
    // actually controls changes with the algorithm), and updates the "not mono-safe"
    // badge (StereoAlgorithm::isMonoSafe(), plan2.md Phase 5 step 4 -- first needed by
    // AllpassDecorrelation, algorithm 2.5). Called once at construction for the initial
    // selection, and from m_algorithmBox.onChange after that (which fires for both user
    // clicks and host-automation-driven changes -- see ComboBoxParameterAttachment::
    // setValue() in JUCE, it notifies external listeners even though it suppresses the
    // attachment's own re-entrant one).
    void updateAuxKnobsForActiveAlgorithm();

    // Rebinds knob to the parameter named paramId (destroying/recreating attachment),
    // configuring whatever display formatting that specific parameter needs; paramId
    // empty means "no parameter for this algorithm" -- detach and disable the knob.
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    void bindAuxKnob(juce::Slider& knob, std::unique_ptr<SliderAttachment>& attachment, const juce::String& paramId);

	StereoWidenerAudioProcessor& m_processor;
    juce::AudioProcessorValueTreeState& m_apvts;

    LevelMeterComponent m_levelMeterIn;
    GoniometerComponent m_goniometer;
    LevelMeterComponent m_levelMeterOut;

    juce::Label m_auxLeftLabel;
    juce::Slider m_auxLeftKnob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_auxLeftAttachment;

    juce::Label m_widthLabel;
    juce::Slider m_widthKnob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_widthAttachment;

    juce::Label m_auxRightLabel;
    juce::Slider m_auxRightKnob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_auxRightAttachment;

    juce::TextButton m_helpButton { "?" };
    juce::ComboBox m_algorithmBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> m_algorithmAttachment;

    // "Not mono-safe" badge (plan2.md Phase 5 step 4): visible only when the active
    // algorithm's isMonoSafe() is false, updated by updateAuxKnobsForActiveAlgorithm().
    juce::Label m_monoSafeBadge;

    // Utilities (Phase 4 step 2), applied regardless of the selected algorithm -- see
    // UtilityProcessor.h. Stacked in their own column below the output meter (Phase 5
    // GUI compaction: every utility acts on the final output signal), not a full-width
    // row below the algorithm selector any more -- see StereoWidenerGUI::resized().
    juce::Label m_rotationLabel;
    juce::Slider m_rotationKnob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_rotationAttachment;

    juce::Label m_balanceLabel;
    juce::Slider m_balanceKnob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_balanceAttachment;

    // Caption above the toggle buttons -- previously the buttons' own text ("Swap",
    // "Inv L", "Inv R") was their only description; this names the group, matching the
    // pattern every other control here has (a label above it).
    juce::Label m_toggleCaption;

    juce::TextButton m_swapLRButton { "Swap" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> m_swapLRAttachment;

    juce::TextButton m_invertLButton { "Inv L" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> m_invertLAttachment;

    juce::TextButton m_invertRButton { "Inv R" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> m_invertRAttachment;

    // Caption above the Monitor selector, same reasoning as m_toggleCaption -- the
    // combo box only showed its current choice ("Normal"/"Mono Check (L+R)"/"Solo Side
    // (S)"), with nothing naming what the control as a whole is.
    juce::Label m_monitorLabel;

    juce::ComboBox m_monitorModeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> m_monitorModeAttachment;
};

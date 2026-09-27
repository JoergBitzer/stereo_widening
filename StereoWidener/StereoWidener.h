#pragma once

#include <array>
#include <memory>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "tools/SynchronBlockProcessor.h"
#include "PluginSettings.h"
#include "AlgorithmHelpPanel.h"
#include "AlgorithmPlayground.h"
#include "GlobalSettings.h"
#include "UtilityProcessor.h"
#include "algorithms/StereoAlgorithm.h"
#include "algorithms/MSWidthBroadband.h"
#include "algorithms/MSWidthFiltered.h"
#include "algorithms/ComplementaryComb.h"
#include "algorithms/AllpassDecorrelation.h"
#include "algorithms/MultibandWidth.h"
#include "algorithms/EarlyReflections.h"
#include "algorithms/ChorusDoubler.h"
#include "../shared/metering/StereoMeterState.h"
#include "../shared/metering/GoniometerComponent.h"
#include "../shared/metering/LevelMeterComponent.h"

class StereoWidenerAudioProcessor;

// Each algorithm's own parameters (IDs, ranges, defaults, units) are declared by the
// algorithm itself -- see StereoAlgorithm::getParamSpecs() and algorithms/*.h.
// StereoWidenerAudio::addParameter() turns them into APVTS parameters.

constexpr const char* g_paramAlgorithmID = "algorithm";
constexpr const char* g_paramAlgorithmName = "Algorithm";

// Must stay in the same order as the algorithm instances StereoWidenerAudio's
// constructor creates in algorithms/ -- see the comment there.
const juce::StringArray g_algorithmNames {
    "M/S Width (Broadband)",
    "M/S Width (Filtered / Bass Mono)",
    "Complementary Comb (Pseudo-Stereo)",
    "Allpass Decorrelation",
    "Multiband Width",
    "Early Reflections (Room Widening)",
    "Chorus Doubler (Dimension D)"
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

// Output trim, applied last in UtilityProcessor (after Monitor mode, so it scales
// whatever is currently being auditioned too -- see UtilityProcessor.h). stepSize
// (not numDecimalPlaces) drives the actual knob/automation increment here -- 0.5 dB,
// not a power of ten -- see makeFloatParameterWithStep() (StereoWidener.cpp).
const struct
{
	const std::string ID = "outputGain";
	const std::string name = "Gain";
	const std::string unitName = "dB";
	const float minValue = -24.0f;
	const float maxValue = 6.0f;
	const float defaultValue = 0.0f;
	const float skew = 1.0f;
	const int numDecimalPlaces = 1;
	const float stepSize = 0.5f;
}g_paramGain;

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

    // Read-only access for the GUI (help popup text, parameter specs for the playgrounds):
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

    // One entry per algorithm (same order as m_algorithms), each holding that
    // algorithm's parameters in its own getParamSpecs() order. Filled once by
    // prepareParameter(); read-only on the audio thread.
    std::vector<std::vector<juce::AudioParameterFloat*>> m_algorithmParams;
    juce::AudioParameterChoice* m_algorithmParam = nullptr;

    // This block's parameter values for algorithmIndex. A crossfade runs two
    // different algorithms in the same block, each with its own parameters -- see
    // processSynchronBlock().
    AlgorithmParamValues valuesFor(int algorithmIndex) const noexcept;

    juce::AudioParameterFloat* m_rotationParam = nullptr;
    juce::AudioParameterFloat* m_balanceParam = nullptr;
    juce::AudioParameterFloat* m_gainParam = nullptr;
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

    // Total content height (px, at scale = 1.0). Fixed: the same for every algorithm,
    // since each algorithm's controls live in a playground of one shared size (see
    // AlgorithmPlayground.h) -- switching algorithms never resizes the window.
    // PluginEditor.cpp sizes the window from this once, at construction.
    static int getRequiredContentHeight() noexcept;

private:
    void showAlgorithmHelp();

    // Shows the selected algorithm's playground (hiding the others) and updates the
    // "not mono-safe" badge. Called once at construction for the initial selection,
    // and from m_algorithmBox.onChange after that (which fires for both user clicks and
    // host-automation-driven changes -- see ComboBoxParameterAttachment::setValue() in
    // JUCE, it notifies external listeners even though it suppresses the attachment's
    // own re-entrant one).
    void showPlaygroundForSelectedAlgorithm();

	StereoWidenerAudioProcessor& m_processor;
    juce::AudioProcessorValueTreeState& m_apvts;

    // The parameter panel (left two-thirds: the selected algorithm's playground and the
    // mono-safe badge) and the Utilities panel (right third) are each drawn
    // with a slightly brighter "card" background in paint() -- these are set in
    // resized() and just read back in paint(), not used for child layout (that still
    // happens directly against the Rectangle<int> locals in resized() itself).
    juce::Rectangle<int> m_paramPanelBounds;
    juce::Rectangle<int> m_utilPanelBounds;

    LevelMeterComponent m_levelMeterIn;
    GoniometerComponent m_goniometer;
    LevelMeterComponent m_levelMeterOut;

    // One playground per algorithm (same order as g_algorithmNames), created once and
    // bound permanently to that algorithm's own parameters; only the selected one is
    // visible. See AlgorithmPlayground.h.
    std::vector<std::unique_ptr<AlgorithmPlayground>> m_playgrounds;

    juce::TextButton m_helpButton { "?" };
    juce::ComboBox m_algorithmBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> m_algorithmAttachment;

    // "Not mono-safe" badge (plan2.md Phase 5 step 4): visible only when the active
    // algorithm's isMonoSafe() is false, updated by showPlaygroundForSelectedAlgorithm().
    // Shared by all algorithms, a strip at the bottom of the parameter card.
    juce::Label m_monoSafeBadge;

    // Build/version footer, anchored to the very bottom of the whole plugin window in
    // both themes -- text set once in the constructor. Previously drawn inside the
    // goniometer's own corner (GoniometerComponent::setCornerText()); moved out to its
    // own label per explicit request, so it isn't tied to the meter panel's own
    // (unthemed, always-black) background.
    juce::Label m_footerLabel;

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

    juce::Label m_gainLabel;
    juce::Slider m_gainKnob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_gainAttachment;

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

/**
 * @file AlgorithmPlayground.h
 * @brief The lower-left "playground" of the StereoWidener GUI: one component per
 *        algorithm, holding that algorithm's own controls (plan_changeGUI.md).
 *
 * Every playground gets the same fixed bounds (StereoWidenerGUI::resized()), so
 * switching algorithms never resizes the window. Each is created once, bound
 * permanently to its own algorithm's parameters (StereoAlgorithm::getParamSpecs()),
 * and only shown or hidden on an algorithm switch -- no slider is ever rebound to a
 * different parameter.
 *
 * KnobsPlayground is the plain "one knob per parameter" version every algorithm starts
 * with; algorithms get their own dedicated playground classes (with graphics suited to
 * what they do, in playgrounds/) one at a time, see plan_changeGUI.md section 5.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "algorithms/StereoAlgorithm.h"

/** Label above a rotary knob with its value box below, permanently attached to one
 *  parameter. The displayed text (value + unit, or "Off") comes from the parameter
 *  itself, see StereoWidenerAudio::addParameter(). */
class PlaygroundKnob : public juce::Component
{
public:
    PlaygroundKnob(juce::AudioProcessorValueTreeState& apvts, const AlgorithmParamSpec& spec);

    /** Knob diameter, label/value-box height, and value-box width (may be wider than
     *  the knob, e.g. to fit "6000 Hz" under a small knob). All in pixels, already
     *  scaled. */
    void setKnobLayout(int knobSize, int textHeight, int textBoxWidth);
    int getPreferredWidth() const noexcept { return juce::jmax(m_knobSize, m_textBoxWidth); }
    int getPreferredHeight() const noexcept { return m_textHeight + m_knobSize + m_textHeight; }

    juce::Slider& getSlider() noexcept { return m_slider; }

    void resized() override;

private:
    juce::Label m_label;
    juce::Slider m_slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_attachment;
    int m_knobSize = 64;
    int m_textHeight = 16;
    int m_textBoxWidth = 64;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaygroundKnob)
};

class AlgorithmPlayground : public juce::Component
{
public:
    explicit AlgorithmPlayground(juce::AudioProcessorValueTreeState& apvts) : m_apvts(apvts) {}

    /** The GUI's current zoom (window width / g_minGuiSize_x); playgrounds scale their
     *  own fixed pixel sizes (PluginSettings.h) by it. */
    void setScaleFactor(float scale)
    {
        m_scale = scale;
        resized();
    }

protected:
    int scaled(int px) const noexcept { return juce::roundToInt((float) px * m_scale); }

    juce::RangedAudioParameter& getParameter(const juce::String& id) const
    {
        auto* param = m_apvts.getParameter(id);
        jassert(param != nullptr);
        return *param;
    }

    /** Calls onChange with the parameter's value (in its own units) right away and
     *  again after every change, including host automation -- always on the message
     *  thread, so it may repaint directly. For live graphics that follow a parameter. */
    void watchParameter(const juce::String& id, std::function<void(float)> onChange)
    {
        m_watchers.push_back(std::make_unique<juce::ParameterAttachment>(getParameter(id), std::move(onChange)));
        m_watchers.back()->sendInitialUpdate();
    }

    juce::AudioProcessorValueTreeState& m_apvts;
    float m_scale = 1.0f;

private:
    std::vector<std::unique_ptr<juce::ParameterAttachment>> m_watchers;
};

/** One PlaygroundKnob per parameter. Up to three parameters: the first (the
 *  algorithm's Width) as a large knob in the middle, the others flanking it. More than
 *  three: a centred grid of small knobs. */
class KnobsPlayground : public AlgorithmPlayground
{
public:
    KnobsPlayground(juce::AudioProcessorValueTreeState& apvts, const StereoAlgorithm& algorithm);

    void resized() override;

protected:
    PlaygroundKnob& getKnob(int index) noexcept { return *m_knobs[(size_t) index]; }

private:
    void layoutAroundMainKnob();
    void layoutGrid();

    std::vector<std::unique_ptr<PlaygroundKnob>> m_knobs;
};

/** The playground for algorithm, bound to its parameters in apvts. */
std::unique_ptr<AlgorithmPlayground> createPlayground(juce::AudioProcessorValueTreeState& apvts,
                                                      const StereoAlgorithm& algorithm);

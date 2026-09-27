/**
 * @file BandSplitView.h
 * @brief MultibandWidth's band-split element (plan_changeGUI.md, section 4.1): the four
 *        bands on a log frequency axis, separated by the three crossovers, each band a
 *        bar whose height is its width.
 *
 * - Band 1 (lowest) is always mono and shown as such.
 * - Bands 2-4: drag up/down to change the band's width (dashed line = 100 %).
 * - The lines between bands are the crossovers: drag sideways. A crossover stops at
 *   its neighbours (kept at least minCrossoverRatio apart).
 * - Double-click resets what's under the mouse to its default.
 *
 * Drags are sent as host gestures (juce::ParameterAttachment), so automation recording
 * and undo work as with a knob.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LogFrequencyAxis.h"

class BandSplitView : public juce::Component
{
public:
    static constexpr int kNumCrossovers = 3;
    static constexpr int kNumBands = kNumCrossovers + 1;

    /** bandWidths are bands 2-4 (band 1 has none, it's always mono). */
    BandSplitView(const std::array<juce::RangedAudioParameter*, kNumCrossovers>& crossovers,
                  const std::array<juce::RangedAudioParameter*, kNumCrossovers>& bandWidths,
                  float minCrossoverRatio);

    void setScaleFactor(float scale) { m_scale = scale; repaint(); }

    /** hz, limited to at least minCrossoverRatio above crossover index-1 and below
     *  crossover index+1 (current values). If the neighbours are themselves closer
     *  than that, the lower limit wins -- one clamp, no back-and-forth. */
    float limitCrossover(int index, float hz) const;

    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    // What the mouse is on: a crossover line (index 0-2) or a band's bar (band index
    // 1-3, i.e. bands 2-4; band 0 can't be changed).
    struct Target
    {
        enum class Kind { None, Crossover, Band } kind = Kind::None;
        int index = -1;
        bool operator==(const Target& other) const { return kind == other.kind && index == other.index; }
    };

    juce::Rectangle<float> getPlotArea() const;
    LogFrequencyAxis getAxis() const { return { getPlotArea() }; }
    float getFontSize() const;
    float getBarTop() const;               // y of a 200 % bar
    float yForWidth(float percent) const;
    float pixelsPerPercent() const;
    float getCrossoverX(int index) const;
    std::array<float, kNumBands + 1> getBandEdges() const;
    Target findTarget(juce::Point<float> position) const;
    void setHovered(Target target);
    juce::ParameterAttachment& attachmentFor(Target target);
    juce::RangedAudioParameter& parameterFor(Target target);

    std::array<juce::RangedAudioParameter*, kNumCrossovers> m_crossovers;
    std::array<juce::RangedAudioParameter*, kNumCrossovers> m_bandWidths;
    std::array<std::unique_ptr<juce::ParameterAttachment>, kNumCrossovers> m_crossoverAttachments;
    std::array<std::unique_ptr<juce::ParameterAttachment>, kNumCrossovers> m_bandWidthAttachments;
    float m_minCrossoverRatio;
    float m_scale = 1.0f;

    Target m_hovered;
    Target m_dragged;
    float m_dragStartWidth = 0.0f;
    float m_dragStartY = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandSplitView)
};

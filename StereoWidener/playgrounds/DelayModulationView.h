/**
 * @file DelayModulationView.h
 * @brief ChorusDoubler's display: the delay times of the two modulated delay lines
 *        over a fixed time window -- L and R swinging a quarter cycle apart, R a little
 *        longer on average. Depth sets how far they swing, Rate how many cycles fit.
 *
 * - Drag up/down to change Depth, left/right to change Rate (right = faster).
 * - Double-click resets both.
 * - The curves fade when Amount is 0 (no effect is added then).
 *
 * Drags are sent as host gestures (juce::ParameterAttachment), so automation recording
 * and undo work as with a knob.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class DelayModulationView : public juce::Component
{
public:
    DelayModulationView(juce::RangedAudioParameter& amount, juce::RangedAudioParameter& depth,
                        juce::RangedAudioParameter& rate);

    void setScaleFactor(float scale) { m_scale = scale; repaint(); }

    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    static constexpr float kWindowSeconds = 4.0f;
    static constexpr float kMinMs = 4.0f;  // delay axis: covers the full range
    static constexpr float kMaxMs = 30.0f; // (5-28 ms at maximum Depth)
    static constexpr float kRateFactorPerWidth = 8.0f; // dragging across the whole width multiplies Rate by this

private:
    juce::Rectangle<float> getPlotArea() const;
    float getFontSize() const;
    float yForMs(float ms) const;
    float pixelsPerMs() const;

    juce::RangedAudioParameter& m_amount;
    juce::RangedAudioParameter& m_depth;
    juce::RangedAudioParameter& m_rate;
    std::unique_ptr<juce::ParameterAttachment> m_amountAttachment, m_depthAttachment, m_rateAttachment;
    float m_scale = 1.0f;

    bool m_hovered = false;
    bool m_dragging = false;
    float m_dragStartDepth = 0.0f;
    float m_dragStartRate = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayModulationView)
};

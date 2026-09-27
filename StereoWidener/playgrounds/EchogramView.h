/**
 * @file EchogramView.h
 * @brief EarlyReflections' display: an echogram of what one impulse at the input
 *        produces -- the direct sound at 0 ms, the left channel's reflections above
 *        the time axis, the right channel's below it, each bar's height its level
 *        relative to the direct sound, in dB (0 to kMinDb). The two different patterns
 *        are the width.
 *
 * - The Pre-delay line (start of the reflections) and the room-end line (end of the
 *   Room Size window) are dragged sideways.
 * - Dragging up/down anywhere else changes Amount (taller reflections).
 * - Double-click resets what's under the mouse.
 *
 * Drags are sent as host gestures (juce::ParameterAttachment), so automation recording
 * and undo work as with a knob.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class EchogramView : public juce::Component
{
public:
    EchogramView(juce::RangedAudioParameter& width, juce::RangedAudioParameter& amount,
                 juce::RangedAudioParameter& roomSize, juce::RangedAudioParameter& preDelay);

    void setScaleFactor(float scale) { m_scale = scale; repaint(); }

    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    static constexpr float kMaxTimeMs = 55.0f; // longest possible pattern: 20 ms pre-delay + 32 ms spread
    static constexpr float kMinDb = -30.0f;    // bottom of the level axis (a bar of zero height)

private:
    enum class Target { None, PreDelay, RoomEnd, Amount };

    juce::Rectangle<float> getPlotArea() const;
    float getFontSize() const;
    float xForTime(float ms) const;
    float timeForX(float x) const;
    float getRoomEndMs() const;
    float barHeight(float level, float halfHeight) const; // level relative to the direct sound (1 = 0 dB)
    Target findTarget(juce::Point<float> position) const;
    void setHovered(Target target);
    juce::RangedAudioParameter* parameterFor(Target target) const;
    juce::ParameterAttachment* attachmentFor(Target target) const;

    juce::RangedAudioParameter& m_width;
    juce::RangedAudioParameter& m_amount;
    juce::RangedAudioParameter& m_roomSize;
    juce::RangedAudioParameter& m_preDelay;
    std::unique_ptr<juce::ParameterAttachment> m_widthAttachment, m_amountAttachment, m_roomSizeAttachment, m_preDelayAttachment;
    float m_scale = 1.0f;

    Target m_hovered = Target::None;
    Target m_dragged = Target::None;
    float m_dragStartAmount = 0.0f;
    float m_dragStartY = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EchogramView)
};

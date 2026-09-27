/**
 * @file FrequencyGraph.h
 * @brief Reusable frequency-response display for the playgrounds (plan_changeGUI.md,
 *        section 3.3): log frequency axis 20 Hz-20 kHz, dB axis, one curve, and
 *        draggable points bound to frequency (and optionally gain) parameters.
 *
 * Dragging a point sideways sets its frequency parameter; if it also has a gain
 * parameter, dragging up/down changes that gain by the dB moved. Double-click resets
 * the point's parameters to their defaults. Drags are sent as host gestures
 * (juce::ParameterAttachment), so automation recording and undo work as with a knob.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class FrequencyGraph : public juce::Component
{
public:
    FrequencyGraph(float minDb, float maxDb);

    /** Gain in dB to draw at a frequency; called for every pixel column on repaint. */
    void setCurve(std::function<float(float frequencyHz)> gainDbAt) { m_curve = std::move(gainDbAt); repaint(); }

    /** Name of the curve (top left, curve colour) and a labelled horizontal reference
     *  line (e.g. the unchanged mid signal at 0 dB). */
    void setCurveName(const juce::String& name) { m_curveName = name; repaint(); }
    void setReferenceLine(float db, const juce::String& name) { m_referenceDb = db; m_referenceName = name; repaint(); }

    /** A draggable point on the curve at frequencyParam's value. */
    void addHandle(juce::RangedAudioParameter& frequencyParam, juce::RangedAudioParameter* gainParam = nullptr);

    void setScaleFactor(float scale) { m_scale = scale; repaint(); }

    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    static constexpr float kMinHz = 20.0f;
    static constexpr float kMaxHz = 20000.0f;

private:
    struct Handle
    {
        juce::RangedAudioParameter* frequency = nullptr;
        juce::RangedAudioParameter* gain = nullptr;
        std::unique_ptr<juce::ParameterAttachment> frequencyAttachment;
        std::unique_ptr<juce::ParameterAttachment> gainAttachment;
    };

    juce::Rectangle<float> getPlotArea() const;
    float xForFrequency(float hz) const;
    float frequencyForX(float x) const;
    float yForDb(float db) const;
    float pixelsPerDb() const;
    juce::Point<float> getHandlePosition(const Handle& handle) const;
    int findHandleAt(juce::Point<float> position) const;
    void setHovered(int index);

    float m_minDb, m_maxDb;
    std::function<float(float)> m_curve;
    juce::String m_curveName;
    float m_referenceDb = 0.0f;
    juce::String m_referenceName;
    std::vector<std::unique_ptr<Handle>> m_handles;
    float m_scale = 1.0f;

    int m_hovered = -1;
    int m_dragged = -1;
    float m_dragStartGain = 0.0f;
    float m_dragStartY = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FrequencyGraph)
};

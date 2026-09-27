/**
 * @file FrequencyGraph.h
 * @brief Reusable frequency-response display for the playgrounds (plan_changeGUI.md,
 *        section 3.3): log frequency axis 20 Hz-20 kHz, dB axis, one or two curves,
 *        draggable points on the first curve bound to frequency (and optionally gain)
 *        parameters, and draggable vertical frequency markers.
 *
 * Dragging a point sideways sets its frequency parameter; if it also has a gain
 * parameter, dragging up/down changes that gain by the dB moved. A marker is dragged
 * sideways; the region below its frequency is shaded. Double-click resets to defaults. Drags are sent as host gestures
 * (juce::ParameterAttachment), so automation recording and undo work as with a knob.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "FrequencyAxis.h"

class FrequencyGraph : public juce::Component
{
public:
    FrequencyGraph(float minDb, float maxDb);

    /** Adds a curve: gain in dB at a frequency. The first curve is drawn in the accent
     *  colour (optionally filled down to the bottom), the second in the secondary
     *  colour, a third in the text colour; the name is shown top left in the curve's
     *  colour. */
    void addCurve(const juce::String& name, std::function<float(float frequencyHz)> gainDbAt, bool fillBelow = false);

    /** Like addCurve(), for curves too detailed to sample once per pixel (e.g. comb
     *  teeth crowding together on the log axis): returns the curve's lowest and highest
     *  gain in dB over [loHz, hiHz], one pixel column; dense regions draw as a band. */
    void addCurveRange(const juce::String& name, std::function<juce::Range<float>(float loHz, float hiHz)> gainDbRange);

    /** A labelled horizontal reference line (e.g. the unchanged mid signal at 0 dB). */
    void setReferenceLine(float db, const juce::String& name) { m_referenceDb = db; m_referenceName = name; repaint(); }

    /** A draggable point on the first curve at frequencyParam's value. */
    void addHandle(juce::RangedAudioParameter& frequencyParam, juce::RangedAudioParameter* gainParam = nullptr);

    /** A draggable vertical line at frequencyParam's value, labelled name, with the
     *  frequencies below it shaded (e.g. a crossover below which an effect is off). */
    void addMarker(juce::RangedAudioParameter& frequencyParam, const juce::String& name);

    /** Frequency marks (e.g. filter stages): a faint vertical line each, with a small
     *  triangle at the top (topEdge) or bottom edge, in the first or second curve's
     *  colour. frequenciesHz is called on every repaint. */
    void addFrequencyMarks(std::function<std::vector<float>()> frequenciesHz, bool secondaryColour, bool topEdge);

    /** Dragging anywhere that isn't a point or marker changes up to two parameters,
     *  relative to where the drag started: a drag across the whole width changes
     *  horizontal by horizontalRange, across the whole height (upwards) vertical by
     *  verticalRange. Double-click there resets both. Either may be nullptr. */
    void setFreeDrag(juce::RangedAudioParameter* horizontal, float horizontalRange,
                     juce::RangedAudioParameter* vertical, float verticalRange);

    /** Linear frequency axis from 0 to maxHz instead of the default log 20 Hz-20 kHz. */
    void setLinearAxis(float maxHz) { m_linearMaxHz = maxHz; repaint(); }

    void setScaleFactor(float scale) { m_scale = scale; repaint(); }

    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    struct Curve
    {
        juce::String name;
        std::function<juce::Range<float>(float, float)> gainDbRange;
        bool fillBelow = false;
    };

    struct Handle
    {
        juce::RangedAudioParameter* frequency = nullptr;
        juce::RangedAudioParameter* gain = nullptr;
        std::unique_ptr<juce::ParameterAttachment> frequencyAttachment;
        std::unique_ptr<juce::ParameterAttachment> gainAttachment;
        bool isMarker = false;
        juce::String name; // markers only
    };

    struct FrequencyMarks
    {
        std::function<std::vector<float>()> frequenciesHz;
        bool secondaryColour = false;
        bool topEdge = false;
    };

    struct FreeDragAxis
    {
        juce::RangedAudioParameter* parameter = nullptr;
        float range = 0.0f;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        float startValue = 0.0f;
    };

    static constexpr int kFreeDrag = -2; // m_hovered/m_dragged value for a free drag

    void addHandleImpl(std::unique_ptr<Handle> handle);
    void drawCurve(juce::Graphics& g, const Curve& curve, juce::Colour colour) const;

    juce::Rectangle<float> getPlotArea() const;
    FrequencyAxis getAxis() const
    {
        return m_linearMaxHz > 0.0f ? FrequencyAxis::linear(getPlotArea(), m_linearMaxHz) : FrequencyAxis::log(getPlotArea());
    }
    float yForDb(float db) const;
    float pixelsPerDb() const;
    juce::Point<float> getHandlePosition(const Handle& handle) const;
    int findHandleAt(juce::Point<float> position) const;
    void setHovered(int index);

    float m_minDb, m_maxDb;
    float m_linearMaxHz = 0.0f; // 0: log axis
    std::vector<FrequencyMarks> m_frequencyMarks;
    FreeDragAxis m_freeHorizontal, m_freeVertical;
    std::vector<Curve> m_curves;
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

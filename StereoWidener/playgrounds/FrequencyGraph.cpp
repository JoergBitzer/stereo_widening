#include "FrequencyGraph.h"
#include "PlaygroundStyle.h"

namespace
{
    float currentValue(const juce::RangedAudioParameter& param)
    {
        return param.convertFrom0to1(param.getValue());
    }

    // Always clamp before handing a value to a parameter: a log-frequency range's
    // convertTo0to1() asserts on values outside its range (see makeLogFrequencyRange()).
    float clampToRange(const juce::RangedAudioParameter& param, float value)
    {
        const auto& range = param.getNormalisableRange();
        return juce::jlimit(range.start, range.end, value);
    }

    float defaultValue(const juce::RangedAudioParameter& param)
    {
        return param.convertFrom0to1(param.getDefaultValue());
    }
}

FrequencyGraph::FrequencyGraph(float minDb, float maxDb) : m_minDb(minDb), m_maxDb(maxDb)
{
}

void FrequencyGraph::addHandle(juce::RangedAudioParameter& frequencyParam, juce::RangedAudioParameter* gainParam)
{
    auto handle = std::make_unique<Handle>();
    handle->frequency = &frequencyParam;
    handle->gain = gainParam;
    handle->frequencyAttachment = std::make_unique<juce::ParameterAttachment>(frequencyParam, [this](float) { repaint(); });
    if (gainParam != nullptr)
        handle->gainAttachment = std::make_unique<juce::ParameterAttachment>(*gainParam, [this](float) { repaint(); });
    m_handles.push_back(std::move(handle));
    repaint();
}

juce::Rectangle<float> FrequencyGraph::getPlotArea() const
{
    return getLocalBounds().toFloat().reduced(4.0f * m_scale);
}

float FrequencyGraph::xForFrequency(float hz) const
{
    const auto plot = getPlotArea();
    return plot.getX() + plot.getWidth() * std::log(hz / kMinHz) / std::log(kMaxHz / kMinHz);
}

float FrequencyGraph::frequencyForX(float x) const
{
    const auto plot = getPlotArea();
    const float proportion = juce::jlimit(0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
    return kMinHz * std::pow(kMaxHz / kMinHz, proportion);
}

float FrequencyGraph::pixelsPerDb() const
{
    return getPlotArea().getHeight() / (m_maxDb - m_minDb);
}

float FrequencyGraph::yForDb(float db) const
{
    const auto plot = getPlotArea();
    return plot.getBottom() - (juce::jlimit(m_minDb, m_maxDb, db) - m_minDb) * pixelsPerDb();
}

juce::Point<float> FrequencyGraph::getHandlePosition(const Handle& handle) const
{
    const float hz = currentValue(*handle.frequency);
    const float db = m_curve != nullptr ? m_curve(hz) : 0.0f;
    return { xForFrequency(juce::jlimit(kMinHz, kMaxHz, hz)), yForDb(db) };
}

int FrequencyGraph::findHandleAt(juce::Point<float> position) const
{
    int best = -1;
    float bestDistance = 9.0f * m_scale; // hit radius, a bit larger than the drawn point
    for (size_t i = 0; i < m_handles.size(); ++i)
    {
        const float distance = position.getDistanceFrom(getHandlePosition(*m_handles[i]));
        if (distance < bestDistance)
        {
            best = (int) i;
            bestDistance = distance;
        }
    }
    return best;
}

void FrequencyGraph::paint(juce::Graphics& g)
{
    const auto style = PlaygroundStyle::of(*this);
    g.setColour(style.background);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), PlaygroundStyle::kCornerSize * m_scale);

    const auto plot = getPlotArea();
    const float fontSize = PlaygroundStyle::kFontSize * 0.9f * m_scale;
    g.setFont(juce::Font(juce::FontOptions(fontSize)));

    // Frequency grid: decades labelled, 2-5-10 steps in between faint.
    struct GridLine { float hz; bool decade; };
    for (auto line : { GridLine { 50.0f, false }, GridLine { 100.0f, true }, GridLine { 200.0f, false },
                       GridLine { 500.0f, false }, GridLine { 1000.0f, true }, GridLine { 2000.0f, false },
                       GridLine { 5000.0f, false }, GridLine { 10000.0f, true } })
    {
        const float hz = line.hz;
        const bool decade = line.decade;
        const float x = xForFrequency(hz);
        g.setColour(style.grid.withMultipliedAlpha(decade ? 1.0f : 0.5f));
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
        if (decade)
        {
            g.setColour(style.text.withAlpha(0.6f));
            g.drawText(hz >= 1000.0f ? juce::String(juce::roundToInt(hz / 1000.0f)) + "k" : juce::String(juce::roundToInt(hz)),
                       juce::Rectangle<float>(x + 2.0f * m_scale, plot.getBottom() - fontSize, 3.0f * fontSize, fontSize),
                       juce::Justification::centredLeft);
        }
    }

    // Level grid every 6 dB, the reference line stronger and labelled.
    g.setColour(style.grid.withMultipliedAlpha(0.5f));
    for (float db = std::ceil(m_minDb / 6.0f) * 6.0f; db <= m_maxDb; db += 6.0f)
        g.drawHorizontalLine(juce::roundToInt(yForDb(db)), plot.getX(), plot.getRight());
    const float referenceY = yForDb(m_referenceDb);
    g.setColour(style.text.withAlpha(0.5f));
    g.drawHorizontalLine(juce::roundToInt(referenceY), plot.getX(), plot.getRight());

    // The curve, filled down to the bottom of the plot.
    if (m_curve != nullptr)
    {
        juce::Path curve;
        curve.startNewSubPath(plot.getX(), yForDb(m_curve(frequencyForX(plot.getX()))));
        for (float x = plot.getX() + 1.0f; x <= plot.getRight(); x += 1.0f)
            curve.lineTo(x, yForDb(m_curve(frequencyForX(x))));
        juce::Path filled(curve);
        filled.lineTo(plot.getRight(), plot.getBottom());
        filled.lineTo(plot.getX(), plot.getBottom());
        filled.closeSubPath();
        g.setColour(style.accent.withAlpha(0.25f));
        g.fillPath(filled);
        g.setColour(style.accent);
        g.strokePath(curve, juce::PathStrokeType(2.0f * m_scale));
    }

    // Names drawn on top of the curve, each on a small backing box so they stay
    // readable wherever the curve runs.
    const auto drawTag = [&](const juce::String& text, juce::Colour colour, float x, float y, bool alignRight)
    {
        const float width = (float) juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), text) + 6.0f * m_scale;
        auto box = juce::Rectangle<float>(alignRight ? x - width : x, y, width, fontSize + 2.0f * m_scale).constrainedWithin(plot);
        g.setColour(style.background.withAlpha(0.8f));
        g.fillRoundedRectangle(box, 2.0f * m_scale);
        g.setColour(colour);
        g.drawText(text, box, juce::Justification::centred);
    };
    if (m_curveName.isNotEmpty())
        drawTag(m_curveName, style.accent, plot.getX() + 2.0f * m_scale, plot.getY() + 1.0f * m_scale, false);
    if (m_referenceName.isNotEmpty())
        drawTag(m_referenceName, style.text, plot.getX() + 2.0f * m_scale, referenceY + 2.0f * m_scale, false);

    // Handles, with the value of the one under the mouse (or being dragged).
    for (size_t i = 0; i < m_handles.size(); ++i)
    {
        const bool active = (int) i == m_hovered || (int) i == m_dragged;
        const float radius = (active ? 6.5f : 5.0f) * m_scale;
        const auto position = getHandlePosition(*m_handles[i]);
        const auto circle = juce::Rectangle<float>(2.0f * radius, 2.0f * radius).withCentre(position);
        g.setColour(style.accent);
        g.fillEllipse(circle);
        g.setColour(style.text);
        g.drawEllipse(circle, 1.5f * m_scale);
    }
    const int labelled = m_dragged >= 0 ? m_dragged : m_hovered;
    if (labelled >= 0)
    {
        const auto& handle = *m_handles[(size_t) labelled];
        juce::String text = handle.frequency->getCurrentValueAsText();
        if (handle.gain != nullptr)
            text << ", " << handle.gain->getCurrentValueAsText();
        const auto position = getHandlePosition(handle);
        const float width = (float) juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), text) + 8.0f * m_scale;
        auto box = juce::Rectangle<float>(width, fontSize + 4.0f * m_scale)
                       .withCentre(position.translated(0.0f, position.y > plot.getCentreY() ? -2.0f * fontSize : 2.0f * fontSize));
        box = box.constrainedWithin(plot);
        g.setColour(style.background.withAlpha(0.9f));
        g.fillRoundedRectangle(box, 3.0f * m_scale);
        g.setColour(style.text);
        g.drawText(text, box, juce::Justification::centred);
    }
}

void FrequencyGraph::setHovered(int index)
{
    if (index != m_hovered)
    {
        m_hovered = index;
        setMouseCursor(index >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void FrequencyGraph::mouseMove(const juce::MouseEvent& e)
{
    setHovered(findHandleAt(e.position));
}

void FrequencyGraph::mouseExit(const juce::MouseEvent&)
{
    setHovered(-1);
}

void FrequencyGraph::mouseDown(const juce::MouseEvent& e)
{
    m_dragged = findHandleAt(e.position);
    if (m_dragged < 0)
        return;
    auto& handle = *m_handles[(size_t) m_dragged];
    handle.frequencyAttachment->beginGesture();
    if (handle.gain != nullptr)
    {
        handle.gainAttachment->beginGesture();
        m_dragStartGain = currentValue(*handle.gain);
        m_dragStartY = e.position.y;
    }
    repaint();
}

void FrequencyGraph::mouseDrag(const juce::MouseEvent& e)
{
    if (m_dragged < 0)
        return;
    auto& handle = *m_handles[(size_t) m_dragged];
    handle.frequencyAttachment->setValueAsPartOfGesture(clampToRange(*handle.frequency, frequencyForX(e.position.x)));
    if (handle.gain != nullptr)
    {
        const float gain = m_dragStartGain + (m_dragStartY - e.position.y) / pixelsPerDb();
        handle.gainAttachment->setValueAsPartOfGesture(clampToRange(*handle.gain, gain));
    }
}

void FrequencyGraph::mouseUp(const juce::MouseEvent&)
{
    if (m_dragged < 0)
        return;
    auto& handle = *m_handles[(size_t) m_dragged];
    handle.frequencyAttachment->endGesture();
    if (handle.gain != nullptr)
        handle.gainAttachment->endGesture();
    m_dragged = -1;
    repaint();
}

void FrequencyGraph::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int index = findHandleAt(e.position);
    if (index < 0)
        return;
    auto& handle = *m_handles[(size_t) index];
    handle.frequencyAttachment->setValueAsCompleteGesture(defaultValue(*handle.frequency));
    if (handle.gain != nullptr)
        handle.gainAttachment->setValueAsCompleteGesture(defaultValue(*handle.gain));
}

#include "FrequencyGraph.h"
#include "ParameterValues.h"
#include <tuple>

using namespace ParameterValues;

FrequencyGraph::FrequencyGraph(float minDb, float maxDb) : m_minDb(minDb), m_maxDb(maxDb)
{
}

void FrequencyGraph::addCurve(const juce::String& name, std::function<float(float)> gainDbAt, bool fillBelow)
{
    // A smooth curve: its value at the column's (geometric) centre stands for the column.
    m_curves.push_back({ name,
                         [gainDbAt = std::move(gainDbAt)](float loHz, float hiHz)
                         {
                             const float db = gainDbAt(std::sqrt(loHz * hiHz));
                             return juce::Range<float>(db, db);
                         },
                         fillBelow });
    repaint();
}

void FrequencyGraph::addCurveRange(const juce::String& name, std::function<juce::Range<float>(float, float)> gainDbRange)
{
    m_curves.push_back({ name, std::move(gainDbRange), false });
    repaint();
}

void FrequencyGraph::addFrequencyMarks(std::function<std::vector<float>()> frequenciesHz, bool secondaryColour, bool topEdge)
{
    m_frequencyMarks.push_back({ std::move(frequenciesHz), secondaryColour, topEdge });
    repaint();
}

void FrequencyGraph::setFreeDrag(juce::RangedAudioParameter* horizontal, float horizontalRange,
                                 juce::RangedAudioParameter* vertical, float verticalRange)
{
    for (auto [axis, parameter, range] : { std::tuple { &m_freeHorizontal, horizontal, horizontalRange },
                                           std::tuple { &m_freeVertical, vertical, verticalRange } })
    {
        axis->parameter = parameter;
        axis->range = range;
        axis->attachment = parameter != nullptr
            ? std::make_unique<juce::ParameterAttachment>(*parameter, [this](float) { repaint(); }) : nullptr;
    }
}

void FrequencyGraph::addHandleImpl(std::unique_ptr<Handle> handle)
{
    handle->frequencyAttachment = std::make_unique<juce::ParameterAttachment>(*handle->frequency, [this](float) { repaint(); });
    if (handle->gain != nullptr)
        handle->gainAttachment = std::make_unique<juce::ParameterAttachment>(*handle->gain, [this](float) { repaint(); });
    m_handles.push_back(std::move(handle));
    repaint();
}

void FrequencyGraph::addHandle(juce::RangedAudioParameter& frequencyParam, juce::RangedAudioParameter* gainParam)
{
    auto handle = std::make_unique<Handle>();
    handle->frequency = &frequencyParam;
    handle->gain = gainParam;
    addHandleImpl(std::move(handle));
}

void FrequencyGraph::addMarker(juce::RangedAudioParameter& frequencyParam, const juce::String& name)
{
    auto handle = std::make_unique<Handle>();
    handle->frequency = &frequencyParam;
    handle->isMarker = true;
    handle->name = name;
    addHandleImpl(std::move(handle));
}

juce::Rectangle<float> FrequencyGraph::getPlotArea() const
{
    return getLocalBounds().toFloat().reduced(4.0f * m_scale);
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
    const auto axis = getAxis();
    const float hz = juce::jlimit(axis.minHz, axis.maxHz, current(*handle.frequency));
    const float x = axis.xForFrequency(hz);
    if (handle.isMarker) // grip at the top of the line
        return { x, getPlotArea().getY() + 0.45f * PlaygroundStyle::kFontSize * m_scale };
    const float db = m_curves.empty() ? 0.0f : m_curves.front().gainDbRange(hz, hz).getStart();
    return { x, yForDb(db) };
}

int FrequencyGraph::findHandleAt(juce::Point<float> position) const
{
    int best = -1;
    float bestDistance = 9.0f * m_scale; // hit radius, a bit larger than the drawn point
    for (size_t i = 0; i < m_handles.size(); ++i)
    {
        const auto handlePosition = getHandlePosition(*m_handles[i]);
        // A marker can be grabbed anywhere along its line.
        const float distance = m_handles[i]->isMarker ? std::abs(position.x - handlePosition.x) * 1.8f
                                                      : position.getDistanceFrom(handlePosition);
        if (distance < bestDistance)
        {
            best = (int) i;
            bestDistance = distance;
        }
    }
    return best;
}

void FrequencyGraph::drawCurve(juce::Graphics& g, const Curve& curve, juce::Colour colour) const
{
    // One pixel column at a time: where the curve varies by more than a pixel within a
    // column (dense comb teeth), draw the column's full range as a vertical stroke, so
    // the curve shows as a band instead of aliasing.
    const auto plot = getPlotArea();
    const auto axis = getAxis();
    juce::Path line, top;
    float lastY = 0.0f;
    bool firstColumn = true;
    for (float x = plot.getX(); x <= plot.getRight(); x += 1.0f)
    {
        const auto range = curve.gainDbRange(axis.frequencyForX(x - 0.5f), axis.frequencyForX(x + 0.5f));
        const float yTop = yForDb(range.getEnd());
        const float yBottom = yForDb(range.getStart());
        if (firstColumn)
        {
            line.startNewSubPath(x, yTop);
            line.lineTo(x, yBottom);
            top.startNewSubPath(x, yTop);
            lastY = yBottom;
            firstColumn = false;
            continue;
        }
        top.lineTo(x, yTop);
        const bool topFirst = std::abs(lastY - yTop) < std::abs(lastY - yBottom);
        line.lineTo(x, topFirst ? yTop : yBottom);
        line.lineTo(x, topFirst ? yBottom : yTop);
        lastY = topFirst ? yBottom : yTop;
    }

    if (curve.fillBelow)
    {
        top.lineTo(plot.getRight(), plot.getBottom());
        top.lineTo(plot.getX(), plot.getBottom());
        top.closeSubPath();
        g.setColour(colour.withAlpha(0.25f));
        g.fillPath(top);
    }
    g.setColour(colour);
    g.strokePath(line, juce::PathStrokeType(2.0f * m_scale));
}

void FrequencyGraph::paint(juce::Graphics& g)
{
    const auto style = PlaygroundStyle::of(*this);
    g.setColour(style.background);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), PlaygroundStyle::kCornerSize * m_scale);

    const auto plot = getPlotArea();
    const float fontSize = PlaygroundStyle::kFontSize * 0.9f * m_scale;
    g.setFont(juce::Font(juce::FontOptions(fontSize)));

    const auto axis = getAxis();
    axis.drawGrid(g, style, fontSize);

    // Markers' shaded regions go under everything else.
    for (const auto& handle : m_handles)
        if (handle->isMarker)
        {
            const float x = getHandlePosition(*handle).x;
            g.setColour(style.grid.withMultipliedAlpha(0.6f));
            g.fillRect(juce::Rectangle<float>(plot.getX(), plot.getY(), x - plot.getX(), plot.getHeight()));
        }

    // Level grid every 6 dB, the reference line stronger.
    g.setColour(style.grid.withMultipliedAlpha(0.5f));
    for (float db = std::ceil(m_minDb / 6.0f) * 6.0f; db <= m_maxDb; db += 6.0f)
        g.drawHorizontalLine(juce::roundToInt(yForDb(db)), plot.getX(), plot.getRight());
    const float referenceY = yForDb(m_referenceDb);
    g.setColour(style.text.withAlpha(0.5f));
    g.drawHorizontalLine(juce::roundToInt(referenceY), plot.getX(), plot.getRight());

    // Frequency marks: faint vertical lines, a triangle at their edge.
    for (const auto& marks : m_frequencyMarks)
    {
        const auto colour = marks.secondaryColour ? style.secondary : style.accent;
        const float size = 5.0f * m_scale;
        for (float hz : marks.frequenciesHz())
        {
            const float x = axis.xForFrequency(hz);
            const float dashes[] = { 2.0f * m_scale, 3.0f * m_scale };
            g.setColour(colour.withAlpha(0.45f));
            g.drawDashedLine(juce::Line<float>(x, plot.getY(), x, plot.getBottom()), dashes, 2, 1.0f * m_scale);
            juce::Path triangle;
            const float edge = marks.topEdge ? plot.getY() : plot.getBottom();
            const float tip = marks.topEdge ? edge + size : edge - size;
            triangle.addTriangle(x - size * 0.7f, edge, x + size * 0.7f, edge, x, tip);
            g.setColour(colour);
            g.fillPath(triangle);
        }
    }

    // The second curve first, so the first (accent) one is on top.
    const auto colourOf = [&](size_t index)
    {
        return index == 0 ? style.accent : index == 1 ? style.secondary : style.text.withAlpha(0.8f);
    };
    for (size_t i = m_curves.size(); i-- > 0;)
        drawCurve(g, m_curves[i], colourOf(i));

    // Markers (vertical lines) and points, with the value of the one under the mouse
    // (or being dragged).
    for (size_t i = 0; i < m_handles.size(); ++i)
    {
        const auto& handle = *m_handles[i];
        const bool active = (int) i == m_hovered || (int) i == m_dragged;
        const auto position = getHandlePosition(handle);
        if (handle.isMarker)
        {
            g.setColour(active ? style.accent : style.text.withAlpha(0.8f));
            g.drawLine(juce::Line<float>(position.x, plot.getY(), position.x, plot.getBottom()), (active ? 2.5f : 1.5f) * m_scale);
        }
        const float radius = (handle.isMarker ? (active ? 5.5f : 4.0f) : (active ? 6.5f : 5.0f)) * m_scale;
        const auto circle = juce::Rectangle<float>(2.0f * radius, 2.0f * radius).withCentre(position);
        g.setColour(style.accent);
        g.fillEllipse(circle);
        g.setColour(style.text);
        g.drawEllipse(circle, 1.5f * m_scale);
    }
    // Names drawn on top of curves, markers and points, each on a small backing box
    // so they stay readable wherever those run.
    const auto drawTag = [&](const juce::String& text, juce::Colour colour, float x, float y) -> float
    {
        const float width = (float) juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), text) + 6.0f * m_scale;
        auto box = juce::Rectangle<float>(x, y, width, fontSize + 2.0f * m_scale).constrainedWithin(plot);
        g.setColour(style.background.withAlpha(0.8f));
        g.fillRoundedRectangle(box, 2.0f * m_scale);
        g.setColour(colour);
        g.drawText(text, box, juce::Justification::centred);
        return box.getRight();
    };
    float tagX = plot.getX() + 2.0f * m_scale;
    for (size_t i = 0; i < m_curves.size(); ++i)
        if (m_curves[i].name.isNotEmpty())
            tagX = drawTag(m_curves[i].name, colourOf(i), tagX, plot.getY() + 1.0f * m_scale) + 3.0f * m_scale;
    if (m_referenceName.isNotEmpty())
        drawTag(m_referenceName, style.text, plot.getX() + 2.0f * m_scale, referenceY + 2.0f * m_scale);
    for (size_t i = 0; i < m_handles.size(); ++i)
        if (m_handles[i]->isMarker && m_handles[i]->name.isNotEmpty() && (int) i != m_hovered && (int) i != m_dragged)
            drawTag(m_handles[i]->name, style.text, getHandlePosition(*m_handles[i]).x + 4.0f * m_scale, plot.getBottom() - 2.4f * fontSize);

    if (m_dragged == kFreeDrag || (m_dragged < 0 && m_hovered == kFreeDrag))
    {
        juce::String text;
        for (const auto* axisParam : { &m_freeHorizontal, &m_freeVertical })
            if (axisParam->parameter != nullptr)
                text << (text.isEmpty() ? "" : ", ") << axisParam->parameter->getName(32) << " " << axisParam->parameter->getCurrentValueAsText();
        const float width = (float) juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), text) + 8.0f * m_scale;
        auto box = juce::Rectangle<float>(width, fontSize + 4.0f * m_scale)
                       .withCentre({ plot.getCentreX(), plot.getBottom() - 1.6f * fontSize });
        g.setColour(style.background.withAlpha(0.9f));
        g.fillRoundedRectangle(box, 3.0f * m_scale);
        g.setColour(style.text);
        g.drawText(text, box, juce::Justification::centred);
    }

    const int labelled = m_dragged >= 0 ? m_dragged : m_hovered;
    if (labelled >= 0)
    {
        const auto& handle = *m_handles[(size_t) labelled];
        juce::String text = handle.frequency->getCurrentValueAsText();
        if (handle.gain != nullptr)
            text << ", " << handle.gain->getCurrentValueAsText();
        if (handle.isMarker && handle.name.isNotEmpty())
            text = handle.name + " " + text;
        const auto position = getHandlePosition(handle);
        const float width = (float) juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), text) + 8.0f * m_scale;
        auto box = handle.isMarker
            ? juce::Rectangle<float>(position.x + 6.0f * m_scale, plot.getCentreY(), width, fontSize + 4.0f * m_scale)
            : juce::Rectangle<float>(width, fontSize + 4.0f * m_scale)
                  .withCentre(position.translated(0.0f, position.y > plot.getCentreY() ? -2.0f * fontSize : 2.0f * fontSize));
        if (handle.isMarker && box.getRight() > plot.getRight())
            box.setX(position.x - 6.0f * m_scale - width);
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
        const bool marker = index >= 0 && m_handles[(size_t) index]->isMarker;
        setMouseCursor(index == kFreeDrag ? juce::MouseCursor::UpDownLeftRightResizeCursor
                     : index < 0          ? juce::MouseCursor::NormalCursor
                     : marker             ? juce::MouseCursor::LeftRightResizeCursor
                                          : juce::MouseCursor::PointingHandCursor);
        repaint();
    }
}

void FrequencyGraph::mouseMove(const juce::MouseEvent& e)
{
    const int handle = findHandleAt(e.position);
    const bool freeDrag = m_freeHorizontal.parameter != nullptr || m_freeVertical.parameter != nullptr;
    setHovered(handle < 0 && freeDrag && getPlotArea().contains(e.position) ? kFreeDrag : handle);
}

void FrequencyGraph::mouseExit(const juce::MouseEvent&)
{
    setHovered(-1);
}

void FrequencyGraph::mouseDown(const juce::MouseEvent& e)
{
    m_dragged = findHandleAt(e.position);
    if (m_dragged < 0 && (m_freeHorizontal.parameter != nullptr || m_freeVertical.parameter != nullptr))
    {
        m_dragged = kFreeDrag;
        for (auto* axis : { &m_freeHorizontal, &m_freeVertical })
            if (axis->parameter != nullptr)
            {
                axis->startValue = current(*axis->parameter);
                axis->attachment->beginGesture();
            }
        repaint();
        return;
    }
    if (m_dragged < 0)
        return;
    auto& handle = *m_handles[(size_t) m_dragged];
    handle.frequencyAttachment->beginGesture();
    if (handle.gain != nullptr)
    {
        handle.gainAttachment->beginGesture();
        m_dragStartGain = current(*handle.gain);
        m_dragStartY = e.position.y;
    }
    repaint();
}

void FrequencyGraph::mouseDrag(const juce::MouseEvent& e)
{
    if (m_dragged == kFreeDrag)
    {
        const auto plot = getPlotArea();
        if (m_freeHorizontal.parameter != nullptr)
            m_freeHorizontal.attachment->setValueAsPartOfGesture(clampToRange(*m_freeHorizontal.parameter,
                m_freeHorizontal.startValue + (float) e.getDistanceFromDragStartX() / plot.getWidth() * m_freeHorizontal.range));
        if (m_freeVertical.parameter != nullptr)
            m_freeVertical.attachment->setValueAsPartOfGesture(clampToRange(*m_freeVertical.parameter,
                m_freeVertical.startValue - (float) e.getDistanceFromDragStartY() / plot.getHeight() * m_freeVertical.range));
        return;
    }
    if (m_dragged < 0)
        return;
    auto& handle = *m_handles[(size_t) m_dragged];
    handle.frequencyAttachment->setValueAsPartOfGesture(clampToRange(*handle.frequency, getAxis().frequencyForX(e.position.x)));
    if (handle.gain != nullptr)
    {
        const float gain = m_dragStartGain + (m_dragStartY - e.position.y) / pixelsPerDb();
        handle.gainAttachment->setValueAsPartOfGesture(clampToRange(*handle.gain, gain));
    }
}

void FrequencyGraph::mouseUp(const juce::MouseEvent&)
{
    if (m_dragged == kFreeDrag)
    {
        for (auto* axis : { &m_freeHorizontal, &m_freeVertical })
            if (axis->parameter != nullptr)
                axis->attachment->endGesture();
        m_dragged = -1;
        repaint();
        return;
    }
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
    {
        for (auto* axis : { &m_freeHorizontal, &m_freeVertical })
            if (axis->parameter != nullptr && getPlotArea().contains(e.position))
                axis->attachment->setValueAsCompleteGesture(defaultValue(*axis->parameter));
        return;
    }
    auto& handle = *m_handles[(size_t) index];
    handle.frequencyAttachment->setValueAsCompleteGesture(defaultValue(*handle.frequency));
    if (handle.gain != nullptr)
        handle.gainAttachment->setValueAsCompleteGesture(defaultValue(*handle.gain));
}

#include "EchogramView.h"
#include "ParameterValues.h"
#include "PlaygroundStyle.h"
#include "../algorithms/EarlyReflections.h"

using namespace ParameterValues;

EchogramView::EchogramView(juce::RangedAudioParameter& width, juce::RangedAudioParameter& amount,
                           juce::RangedAudioParameter& roomSize, juce::RangedAudioParameter& preDelay)
    : m_width(width), m_amount(amount), m_roomSize(roomSize), m_preDelay(preDelay)
{
    const auto repaintOnChange = [this](float) { repaint(); };
    m_widthAttachment = std::make_unique<juce::ParameterAttachment>(m_width, repaintOnChange);
    m_amountAttachment = std::make_unique<juce::ParameterAttachment>(m_amount, repaintOnChange);
    m_roomSizeAttachment = std::make_unique<juce::ParameterAttachment>(m_roomSize, repaintOnChange);
    m_preDelayAttachment = std::make_unique<juce::ParameterAttachment>(m_preDelay, repaintOnChange);
}

juce::Rectangle<float> EchogramView::getPlotArea() const
{
    auto plot = getLocalBounds().toFloat().reduced(4.0f * m_scale);
    plot.removeFromBottom(getFontSize()); // time labels
    return plot;
}

float EchogramView::getFontSize() const
{
    return PlaygroundStyle::kFontSize * 0.9f * m_scale;
}

float EchogramView::xForTime(float ms) const
{
    const auto plot = getPlotArea().reduced(3.0f * m_scale, 0.0f); // keep the direct sound's bar off the edge
    return plot.getX() + plot.getWidth() * juce::jlimit(0.0f, kMaxTimeMs, ms) / kMaxTimeMs;
}

float EchogramView::timeForX(float x) const
{
    const auto plot = getPlotArea().reduced(3.0f * m_scale, 0.0f);
    return kMaxTimeMs * juce::jlimit(0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
}

float EchogramView::getRoomEndMs() const
{
    // the end of the Room Size window = where a reflection at fraction 1.0 would be
    const float spreadMs = EarlyReflections::kRoomMinSpreadMs
                         + current(m_roomSize) * 0.01f * (EarlyReflections::kRoomMaxSpreadMs - EarlyReflections::kRoomMinSpreadMs);
    return current(m_preDelay) + spreadMs;
}

float EchogramView::barHeight(float level, float halfHeight) const
{
    // dB, not linear: realistic reflections (e.g. -11 dB) would otherwise be tiny bars
    // next to the direct sound, and dB shows their decay in even steps.
    const float db = juce::Decibels::gainToDecibels(level, kMinDb);
    return halfHeight * (db - kMinDb) / -kMinDb;
}

EchogramView::Target EchogramView::findTarget(juce::Point<float> position) const
{
    if (!getLocalBounds().toFloat().contains(position))
        return Target::None;
    const float grab = 5.0f * m_scale;
    const float toPreDelay = std::abs(position.x - xForTime(current(m_preDelay)));
    const float toRoomEnd = std::abs(position.x - xForTime(getRoomEndMs()));
    if (juce::jmin(toPreDelay, toRoomEnd) <= grab)
        return toPreDelay <= toRoomEnd ? Target::PreDelay : Target::RoomEnd;
    return Target::Amount;
}

juce::RangedAudioParameter* EchogramView::parameterFor(Target target) const
{
    switch (target)
    {
        case Target::PreDelay: return &m_preDelay;
        case Target::RoomEnd:  return &m_roomSize;
        case Target::Amount:   return &m_amount;
        case Target::None:     break;
    }
    return nullptr;
}

juce::ParameterAttachment* EchogramView::attachmentFor(Target target) const
{
    switch (target)
    {
        case Target::PreDelay: return m_preDelayAttachment.get();
        case Target::RoomEnd:  return m_roomSizeAttachment.get();
        case Target::Amount:   return m_amountAttachment.get();
        case Target::None:     break;
    }
    return nullptr;
}

void EchogramView::paint(juce::Graphics& g)
{
    const auto style = PlaygroundStyle::of(*this);
    g.setColour(style.background);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), PlaygroundStyle::kCornerSize * m_scale);

    const auto plot = getPlotArea();
    const float fontSize = getFontSize();
    g.setFont(juce::Font(juce::FontOptions(fontSize)));
    const float centreY = plot.getCentreY();
    const float halfHeight = 0.5f * plot.getHeight() - 2.0f * m_scale; // 0 dB (the direct sound)

    const float preDelayMs = current(m_preDelay);
    const float roomEndMs = getRoomEndMs();
    const float roomSize = current(m_roomSize) * 0.01f;
    const float level = current(m_width) * 0.01f * current(m_amount) * 0.01f;

    // Time grid every 10 ms, labelled below the plot.
    for (float ms = 10.0f; ms < kMaxTimeMs; ms += 10.0f)
    {
        const float x = xForTime(ms);
        g.setColour(style.grid);
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
        g.setColour(style.text.withAlpha(0.6f));
        g.drawText(juce::String(juce::roundToInt(ms)) + (ms >= 50.0f ? " ms" : ""),
                   juce::Rectangle<float>(x - 2.0f * fontSize, plot.getBottom(), 4.0f * fontSize, fontSize), juce::Justification::centred);
    }

    // Pre-delay shaded, the room window lightly tinted, the time axis in the middle.
    g.setColour(style.grid.withMultipliedAlpha(0.6f));
    g.fillRect(juce::Rectangle<float>(xForTime(0.0f), plot.getY(), xForTime(preDelayMs) - xForTime(0.0f), plot.getHeight()));
    g.setColour(style.accent.withAlpha(0.08f));
    g.fillRect(juce::Rectangle<float>(xForTime(preDelayMs), plot.getY(), xForTime(roomEndMs) - xForTime(preDelayMs), plot.getHeight()));
    g.setColour(style.grid.withMultipliedAlpha(0.6f));
    for (float db : { -10.0f, -20.0f })
        for (float sign : { -1.0f, 1.0f })
            g.drawHorizontalLine(juce::roundToInt(centreY + sign * barHeight(juce::Decibels::decibelsToGain(db), halfHeight)),
                                 plot.getX(), plot.getRight());
    g.setColour(style.text.withAlpha(0.5f));
    g.drawHorizontalLine(juce::roundToInt(centreY), plot.getX(), plot.getRight());

    // Direct sound: level 1 in both channels.
    const float directX = xForTime(0.0f);
    g.setColour(style.text.withAlpha(0.8f));
    g.drawLine(juce::Line<float>(directX, centreY - halfHeight, directX, centreY + halfHeight), 2.0f * m_scale);

    // Reflections: L up, R down. A tick on the axis marks each position even at Amount 0.
    const float barWidth = 3.0f * m_scale;
    for (int k = 0; k < EarlyReflections::kNumReflections; ++k)
    {
        const float height = barHeight(EarlyReflections::tapGain(k) * level, halfHeight);
        for (bool left : { true, false })
        {
            const float x = xForTime(EarlyReflections::tapTimeMs(k, left, roomSize, preDelayMs));
            const float sign = left ? -1.0f : 1.0f;
            const auto colour = left ? style.accent : style.secondary;
            g.setColour(colour.withAlpha(0.6f));
            g.drawLine(juce::Line<float>(x, centreY, x, centreY + sign * 3.0f * m_scale), 1.5f * m_scale);
            g.setColour(colour);
            g.drawLine(juce::Line<float>(x, centreY, x, centreY + sign * height), barWidth);
        }
    }

    // Pre-delay and room-end lines with grips at the top.
    for (auto target : { Target::PreDelay, Target::RoomEnd })
    {
        const bool active = target == m_hovered || target == m_dragged;
        const float x = xForTime(target == Target::PreDelay ? preDelayMs : roomEndMs);
        g.setColour(active ? style.accent : style.text.withAlpha(0.7f));
        g.drawLine(juce::Line<float>(x, plot.getY(), x, plot.getBottom()), (active ? 2.5f : 1.2f) * m_scale);
        const float radius = (active ? 5.5f : 4.0f) * m_scale;
        const auto grip = juce::Rectangle<float>(2.0f * radius, 2.0f * radius).withCentre({ x, plot.getY() + radius });
        g.setColour(style.accent);
        g.fillEllipse(grip);
        g.setColour(style.text);
        g.drawEllipse(grip, 1.0f * m_scale);
    }

    // Labels on backing boxes: channel names, and the value of what's under the mouse.
    const auto drawTag = [&](const juce::String& text, juce::Colour colour, juce::Rectangle<float> box)
    {
        box = box.constrainedWithin(plot);
        g.setColour(style.background.withAlpha(0.85f));
        g.fillRoundedRectangle(box, 2.0f * m_scale);
        g.setColour(colour);
        g.drawText(text, box, juce::Justification::centred);
    };
    const auto textWidth = [&](const juce::String& text)
    {
        return (float) juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), text) + 6.0f * m_scale;
    };
    const float tagHeight = fontSize + 2.0f * m_scale;
    drawTag("L", style.accent, { plot.getRight() - textWidth("L"), plot.getY() + 1.0f * m_scale, textWidth("L"), tagHeight });
    drawTag("R", style.secondary, { plot.getRight() - textWidth("R"), plot.getBottom() - tagHeight - 1.0f * m_scale, textWidth("R"), tagHeight });

    const auto shown = m_dragged != Target::None ? m_dragged : m_hovered;
    if (shown != Target::None)
    {
        const juce::String text = shown == Target::PreDelay ? "Pre-delay " + m_preDelay.getCurrentValueAsText()
                                : shown == Target::RoomEnd  ? "Room Size " + m_roomSize.getCurrentValueAsText()
                                                            : "Amount " + m_amount.getCurrentValueAsText();
        const float x = shown == Target::PreDelay ? xForTime(preDelayMs)
                      : shown == Target::RoomEnd  ? xForTime(roomEndMs)
                                                  : plot.getCentreX();
        const float width = textWidth(text) + 2.0f * m_scale;
        auto box = juce::Rectangle<float>(x + 6.0f * m_scale, centreY - tagHeight - 4.0f * m_scale, width, tagHeight);
        if (shown == Target::Amount || box.getRight() > plot.getRight())
            box.setX(shown == Target::Amount ? x - 0.5f * width : x - 6.0f * m_scale - width);
        drawTag(text, style.text, box);
    }
}

void EchogramView::setHovered(Target target)
{
    if (target == m_hovered)
        return;
    m_hovered = target;
    setMouseCursor(target == Target::Amount ? juce::MouseCursor::UpDownResizeCursor
                 : target == Target::None   ? juce::MouseCursor::NormalCursor
                                            : juce::MouseCursor::LeftRightResizeCursor);
    repaint();
}

void EchogramView::mouseMove(const juce::MouseEvent& e)
{
    setHovered(findTarget(e.position));
}

void EchogramView::mouseExit(const juce::MouseEvent&)
{
    setHovered(Target::None);
}

void EchogramView::mouseDown(const juce::MouseEvent& e)
{
    m_dragged = findTarget(e.position);
    if (m_dragged == Target::None)
        return;
    attachmentFor(m_dragged)->beginGesture();
    m_dragStartAmount = current(m_amount);
    m_dragStartY = e.position.y;
    repaint();
}

void EchogramView::mouseDrag(const juce::MouseEvent& e)
{
    if (m_dragged == Target::None)
        return;
    float value = 0.0f;
    if (m_dragged == Target::PreDelay)
    {
        value = timeForX(e.position.x);
    }
    else if (m_dragged == Target::RoomEnd)
    {
        // the room window spans kRoomMin..kRoomMaxSpreadMs after the pre-delay
        const float spreadMs = timeForX(e.position.x) - current(m_preDelay);
        value = 100.0f * (spreadMs - EarlyReflections::kRoomMinSpreadMs)
              / (EarlyReflections::kRoomMaxSpreadMs - EarlyReflections::kRoomMinSpreadMs);
    }
    else
    {
        // dragging by half the plot's height changes Amount by 100 %
        value = m_dragStartAmount + (m_dragStartY - e.position.y) / (0.5f * getPlotArea().getHeight()) * 100.0f;
    }
    attachmentFor(m_dragged)->setValueAsPartOfGesture(clampToRange(*parameterFor(m_dragged), value));
}

void EchogramView::mouseUp(const juce::MouseEvent&)
{
    if (m_dragged == Target::None)
        return;
    attachmentFor(m_dragged)->endGesture();
    m_dragged = Target::None;
    repaint();
}

void EchogramView::mouseDoubleClick(const juce::MouseEvent& e)
{
    const auto target = findTarget(e.position);
    if (target != Target::None)
        attachmentFor(target)->setValueAsCompleteGesture(defaultValue(*parameterFor(target)));
}

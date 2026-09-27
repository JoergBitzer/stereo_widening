#include "BandSplitView.h"
#include "ParameterValues.h"

using namespace ParameterValues;

BandSplitView::BandSplitView(const std::array<juce::RangedAudioParameter*, kNumCrossovers>& crossovers,
                             const std::array<juce::RangedAudioParameter*, kNumCrossovers>& bandWidths,
                             float minCrossoverRatio)
    : m_crossovers(crossovers), m_bandWidths(bandWidths), m_minCrossoverRatio(minCrossoverRatio)
{
    for (int i = 0; i < kNumCrossovers; ++i)
    {
        m_crossoverAttachments[(size_t) i] = std::make_unique<juce::ParameterAttachment>(*m_crossovers[(size_t) i], [this](float) { repaint(); });
        m_bandWidthAttachments[(size_t) i] = std::make_unique<juce::ParameterAttachment>(*m_bandWidths[(size_t) i], [this](float) { repaint(); });
    }
}

juce::Rectangle<float> BandSplitView::getPlotArea() const
{
    return getLocalBounds().toFloat().reduced(4.0f * m_scale);
}

float BandSplitView::getFontSize() const
{
    return PlaygroundStyle::kFontSize * 0.9f * m_scale;
}

float BandSplitView::getBarTop() const
{
    return getPlotArea().getY() + getFontSize() + 2.0f * m_scale; // band numbers above
}

float BandSplitView::pixelsPerPercent() const
{
    return (getPlotArea().getBottom() - getBarTop()) / m_bandWidths[0]->getNormalisableRange().end;
}

float BandSplitView::yForWidth(float percent) const
{
    return getPlotArea().getBottom() - percent * pixelsPerPercent();
}

float BandSplitView::getCrossoverX(int index) const
{
    const float hz = current(*m_crossovers[(size_t) index]);
    return getAxis().xForFrequency(juce::jlimit(LogFrequencyAxis::kMinHz, LogFrequencyAxis::kMaxHz, hz));
}

std::array<float, BandSplitView::kNumBands + 1> BandSplitView::getBandEdges() const
{
    const auto plot = getPlotArea();
    return { plot.getX(), getCrossoverX(0), getCrossoverX(1), getCrossoverX(2), plot.getRight() };
}

BandSplitView::Target BandSplitView::findTarget(juce::Point<float> position) const
{
    if (!getPlotArea().contains(position))
        return {};

    // Crossover lines first (grab within a few pixels), nearest wins.
    Target target;
    float bestDistance = 5.0f * m_scale;
    for (int i = 0; i < kNumCrossovers; ++i)
    {
        const float distance = std::abs(position.x - getCrossoverX(i));
        if (distance <= bestDistance)
        {
            target = { Target::Kind::Crossover, i };
            bestDistance = distance;
        }
    }
    if (target.kind != Target::Kind::None)
        return target;

    const auto edges = getBandEdges();
    for (int band = 1; band < kNumBands; ++band) // band 0 is always mono, nothing to drag
        if (position.x >= edges[(size_t) band] && position.x < edges[(size_t) band + 1])
            return { Target::Kind::Band, band };
    return {};
}

float BandSplitView::limitCrossover(int index, float hz) const
{
    if (index < kNumCrossovers - 1)
        hz = juce::jmin(hz, current(*m_crossovers[(size_t) index + 1]) / m_minCrossoverRatio);
    if (index > 0)
        hz = juce::jmax(hz, current(*m_crossovers[(size_t) index - 1]) * m_minCrossoverRatio);
    return hz;
}

juce::ParameterAttachment& BandSplitView::attachmentFor(Target target)
{
    return target.kind == Target::Kind::Crossover ? *m_crossoverAttachments[(size_t) target.index]
                                                  : *m_bandWidthAttachments[(size_t) target.index - 1];
}

juce::RangedAudioParameter& BandSplitView::parameterFor(Target target)
{
    return target.kind == Target::Kind::Crossover ? *m_crossovers[(size_t) target.index]
                                                  : *m_bandWidths[(size_t) target.index - 1];
}

void BandSplitView::paint(juce::Graphics& g)
{
    const auto style = PlaygroundStyle::of(*this);
    g.setColour(style.background);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), PlaygroundStyle::kCornerSize * m_scale);

    const auto plot = getPlotArea();
    const auto axis = getAxis();
    const float fontSize = getFontSize();
    g.setFont(juce::Font(juce::FontOptions(fontSize)));
    axis.drawGrid(g, style, fontSize);

    // 100 % = unchanged
    const float y100 = yForWidth(100.0f);
    const float dashes[] = { 3.0f * m_scale, 3.0f * m_scale };
    g.setColour(style.text.withAlpha(0.5f));
    g.drawDashedLine(juce::Line<float>(plot.getX(), y100, plot.getRight(), y100), dashes, 2, 1.0f * m_scale);
    // labelled in band 1, which is always mono, so no bar or value ever sits there
    g.drawText("100 %", juce::Rectangle<float>(plot.getX() + 2.0f * m_scale, y100 - fontSize - 1.0f * m_scale, 4.0f * fontSize, fontSize),
               juce::Justification::centredLeft);

    // Bands: number on top, bar height = width. Band 1 is always mono.
    const auto edges = getBandEdges();
    for (int band = 0; band < kNumBands; ++band)
    {
        const float left = edges[(size_t) band];
        const float right = edges[(size_t) band + 1];
        const float centreX = 0.5f * (left + right);
        const float bandWidthPx = right - left;

        g.setColour(style.text.withAlpha(0.6f));
        if (bandWidthPx > fontSize)
            g.drawText(juce::String(band + 1), juce::Rectangle<float>(left, plot.getY(), bandWidthPx, fontSize), juce::Justification::centred);

        if (band == 0)
        {
            g.setColour(style.accent);
            g.drawHorizontalLine(juce::roundToInt(plot.getBottom() - 1.0f), left, right);
            g.setColour(style.text);
            if (bandWidthPx > 3.0f * fontSize)
                g.drawText("Mono", juce::Rectangle<float>(left, yForWidth(100.0f) + 2.0f * m_scale, bandWidthPx, fontSize),
                           juce::Justification::centred);
            continue;
        }

        const float widthPercent = current(*m_bandWidths[(size_t) band - 1]);
        const float top = yForWidth(widthPercent);
        const bool active = m_hovered == Target { Target::Kind::Band, band } || m_dragged == Target { Target::Kind::Band, band };
        const auto bar = juce::Rectangle<float>(left + 1.0f * m_scale, top, bandWidthPx - 2.0f * m_scale, plot.getBottom() - top);
        g.setColour(style.accent.withAlpha(active ? 0.55f : 0.35f));
        g.fillRect(bar);
        g.setColour(style.accent);
        g.fillRect(bar.withHeight(2.0f * m_scale));

        const juce::String text = juce::String(juce::roundToInt(widthPercent)) + " %";
        const float textWidth = (float) juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), text);
        if (bandWidthPx > textWidth + 4.0f * m_scale)
        {
            const float textY = juce::jmax(getBarTop(), top - fontSize - 1.0f * m_scale);
            g.setColour(style.text);
            g.drawText(text, juce::Rectangle<float>(centreX - 0.5f * bandWidthPx, textY, bandWidthPx, fontSize), juce::Justification::centred);
        }
    }

    // Crossover lines with a grip at the top; the one under the mouse shows its value.
    for (int i = 0; i < kNumCrossovers; ++i)
    {
        const bool active = m_hovered == Target { Target::Kind::Crossover, i } || m_dragged == Target { Target::Kind::Crossover, i };
        const float x = getCrossoverX(i);
        g.setColour(active ? style.accent : style.text.withAlpha(0.8f));
        g.drawLine(juce::Line<float>(x, plot.getY(), x, plot.getBottom()), (active ? 2.5f : 1.5f) * m_scale);
        const float radius = (active ? 5.5f : 4.0f) * m_scale;
        const auto grip = juce::Rectangle<float>(2.0f * radius, 2.0f * radius).withCentre({ x, plot.getY() + 0.5f * fontSize });
        g.setColour(style.accent);
        g.fillEllipse(grip);
        g.setColour(style.text);
        g.drawEllipse(grip, 1.0f * m_scale);

        if (active)
        {
            const auto text = m_crossovers[(size_t) i]->getCurrentValueAsText();
            const float width = (float) juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), text) + 6.0f * m_scale;
            auto box = juce::Rectangle<float>(x + 4.0f * m_scale, getBarTop(), width, fontSize + 2.0f * m_scale);
            if (box.getRight() > plot.getRight())
                box.setX(x - 4.0f * m_scale - width);
            g.setColour(style.background.withAlpha(0.9f));
            g.fillRoundedRectangle(box, 2.0f * m_scale);
            g.setColour(style.text);
            g.drawText(text, box, juce::Justification::centred);
        }
    }
}

void BandSplitView::setHovered(Target target)
{
    if (target == m_hovered)
        return;
    m_hovered = target;
    setMouseCursor(target.kind == Target::Kind::Crossover ? juce::MouseCursor::LeftRightResizeCursor
                 : target.kind == Target::Kind::Band      ? juce::MouseCursor::UpDownResizeCursor
                                                          : juce::MouseCursor::NormalCursor);
    repaint();
}

void BandSplitView::mouseMove(const juce::MouseEvent& e)
{
    setHovered(findTarget(e.position));
}

void BandSplitView::mouseExit(const juce::MouseEvent&)
{
    setHovered({});
}

void BandSplitView::mouseDown(const juce::MouseEvent& e)
{
    m_dragged = findTarget(e.position);
    if (m_dragged.kind == Target::Kind::None)
        return;
    attachmentFor(m_dragged).beginGesture();
    if (m_dragged.kind == Target::Kind::Band)
    {
        m_dragStartWidth = current(parameterFor(m_dragged));
        m_dragStartY = e.position.y;
    }
    repaint();
}

void BandSplitView::mouseDrag(const juce::MouseEvent& e)
{
    if (m_dragged.kind == Target::Kind::None)
        return;
    auto& param = parameterFor(m_dragged);
    float value = 0.0f;
    if (m_dragged.kind == Target::Kind::Crossover)
        value = limitCrossover(m_dragged.index, getAxis().frequencyForX(e.position.x)); // stops at the neighbours
    else
    {
        value = m_dragStartWidth + (m_dragStartY - e.position.y) / pixelsPerPercent();
    }
    attachmentFor(m_dragged).setValueAsPartOfGesture(clampToRange(param, value));
}

void BandSplitView::mouseUp(const juce::MouseEvent&)
{
    if (m_dragged.kind == Target::Kind::None)
        return;
    attachmentFor(m_dragged).endGesture();
    m_dragged = {};
    repaint();
}

void BandSplitView::mouseDoubleClick(const juce::MouseEvent& e)
{
    const auto target = findTarget(e.position);
    if (target.kind == Target::Kind::None)
        return;
    auto& param = parameterFor(target);
    float value = defaultValue(param);
    if (target.kind == Target::Kind::Crossover)
        value = limitCrossover(target.index, value);
    attachmentFor(target).setValueAsCompleteGesture(clampToRange(param, value));
}

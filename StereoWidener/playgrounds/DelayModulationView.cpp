#include "DelayModulationView.h"
#include "ParameterValues.h"
#include "PlaygroundStyle.h"
#include "../algorithms/ChorusDoubler.h"

using namespace ParameterValues;

DelayModulationView::DelayModulationView(juce::RangedAudioParameter& width, juce::RangedAudioParameter& amount,
                                         juce::RangedAudioParameter& depth, juce::RangedAudioParameter& rate)
    : m_width(width), m_amount(amount), m_depth(depth), m_rate(rate)
{
    const auto repaintOnChange = [this](float) { repaint(); };
    m_widthAttachment = std::make_unique<juce::ParameterAttachment>(m_width, repaintOnChange);
    m_amountAttachment = std::make_unique<juce::ParameterAttachment>(m_amount, repaintOnChange);
    m_depthAttachment = std::make_unique<juce::ParameterAttachment>(m_depth, repaintOnChange);
    m_rateAttachment = std::make_unique<juce::ParameterAttachment>(m_rate, repaintOnChange);
    setMouseCursor(juce::MouseCursor::UpDownLeftRightResizeCursor);
}

juce::Rectangle<float> DelayModulationView::getPlotArea() const
{
    auto plot = getLocalBounds().toFloat().reduced(4.0f * m_scale);
    plot.removeFromBottom(getFontSize()); // time labels
    return plot;
}

float DelayModulationView::getFontSize() const
{
    return PlaygroundStyle::kFontSize * 0.9f * m_scale;
}

float DelayModulationView::pixelsPerMs() const
{
    return getPlotArea().getHeight() / (kMaxMs - kMinMs);
}

float DelayModulationView::yForMs(float ms) const
{
    return getPlotArea().getBottom() - (ms - kMinMs) * pixelsPerMs();
}

void DelayModulationView::paint(juce::Graphics& g)
{
    const auto style = PlaygroundStyle::of(*this);
    g.setColour(style.background);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), PlaygroundStyle::kCornerSize * m_scale);

    const auto plot = getPlotArea();
    const float fontSize = getFontSize();
    g.setFont(juce::Font(juce::FontOptions(fontSize)));

    // Grid: seconds along the bottom, 10/20 ms delay lines labelled on the left.
    for (int second = 1; second < (int) kWindowSeconds + 1; ++second)
    {
        const float x = plot.getX() + plot.getWidth() * (float) second / kWindowSeconds;
        g.setColour(style.grid);
        if (second < (int) kWindowSeconds)
            g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
        g.setColour(style.text.withAlpha(0.6f));
        g.drawText(juce::String(second) + " s", juce::Rectangle<float>(x - 3.0f * fontSize, plot.getBottom(), 3.0f * fontSize, fontSize),
                   juce::Justification::centredRight);
    }
    for (float ms : { 10.0f, 20.0f })
    {
        g.setColour(style.grid);
        g.drawHorizontalLine(juce::roundToInt(yForMs(ms)), plot.getX(), plot.getRight());
    }

    // The two delay times over the window; faded when no effect is added.
    const float depth = current(m_depth) * 0.01f;
    const float rateHz = current(m_rate);
    const bool active = current(m_width) * current(m_amount) > 0.0f;
    for (bool left : { false, true }) // R first, so L is on top
    {
        const auto yAt = [&](float x)
        {
            const float seconds = kWindowSeconds * (x - plot.getX()) / plot.getWidth();
            return yForMs(ChorusDoubler::delayMs(left, depth, juce::MathConstants<float>::twoPi * rateHz * seconds));
        };
        juce::Path curve;
        curve.startNewSubPath(plot.getX(), yAt(plot.getX()));
        for (float x = plot.getX() + 1.0f; x <= plot.getRight(); x += 1.0f)
            curve.lineTo(x, yAt(x));
        const auto colour = left ? style.accent : style.secondary;
        g.setColour(colour.withMultipliedAlpha(active ? 1.0f : 0.35f));
        g.strokePath(curve, juce::PathStrokeType(2.0f * m_scale));
    }

    // Labels on backing boxes.
    const auto drawTag = [&](const juce::String& text, juce::Colour colour, float x, float y, bool alignRight)
    {
        const float width = (float) juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), text) + 6.0f * m_scale;
        auto box = juce::Rectangle<float>(alignRight ? x - width : x, y, width, fontSize + 2.0f * m_scale).constrainedWithin(plot);
        g.setColour(style.background.withAlpha(0.85f));
        g.fillRoundedRectangle(box, 2.0f * m_scale);
        g.setColour(colour);
        g.drawText(text, box, juce::Justification::centred);
        return box.getX();
    };
    for (float ms : { 10.0f, 20.0f })
        drawTag(juce::String(juce::roundToInt(ms)) + " ms", style.text.withAlpha(0.7f), plot.getX() + 2.0f * m_scale,
                yForMs(ms) - 0.5f * fontSize - 1.0f * m_scale, false);
    const float rTagX = drawTag("R", style.secondary, plot.getRight(), plot.getY() + 1.0f * m_scale, true);
    drawTag("L", style.accent, rTagX - 3.0f * m_scale, plot.getY() + 1.0f * m_scale, true);
    const juce::String status = m_hovered || m_dragging ? "Depth " + m_depth.getCurrentValueAsText() + ", Rate " + m_rate.getCurrentValueAsText()
                              : !active                 ? juce::String("Amount 0 %: no effect")
                                                        : juce::String();
    if (status.isNotEmpty()) // centred near the bottom
        drawTag(status, style.text, plot.getCentreX() + 0.5f * (float) juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), status),
                plot.getBottom() - fontSize - 4.0f * m_scale, true);
}

void DelayModulationView::mouseEnter(const juce::MouseEvent&)
{
    m_hovered = true;
    repaint();
}

void DelayModulationView::mouseExit(const juce::MouseEvent&)
{
    m_hovered = false;
    repaint();
}

void DelayModulationView::mouseDown(const juce::MouseEvent&)
{
    m_dragging = true;
    m_dragStartDepth = current(m_depth);
    m_dragStartRate = current(m_rate);
    m_depthAttachment->beginGesture();
    m_rateAttachment->beginGesture();
    repaint();
}

void DelayModulationView::mouseDrag(const juce::MouseEvent& e)
{
    // Up/down: the curves' peaks follow the mouse (Depth 100 % = 10 ms swing).
    const float depthPercent = m_dragStartDepth - (float) e.getDistanceFromDragStartY() / pixelsPerMs()
                                                  / ChorusDoubler::kMaxDepthMs * 100.0f;
    // Left/right: multiplicative, so slow and fast rates are equally easy to set.
    const float rateHz = m_dragStartRate * std::pow(kRateFactorPerWidth, (float) e.getDistanceFromDragStartX() / getPlotArea().getWidth());
    m_depthAttachment->setValueAsPartOfGesture(clampToRange(m_depth, depthPercent));
    m_rateAttachment->setValueAsPartOfGesture(clampToRange(m_rate, rateHz));
}

void DelayModulationView::mouseUp(const juce::MouseEvent&)
{
    m_depthAttachment->endGesture();
    m_rateAttachment->endGesture();
    m_dragging = false;
    repaint();
}

void DelayModulationView::mouseDoubleClick(const juce::MouseEvent&)
{
    m_depthAttachment->setValueAsCompleteGesture(defaultValue(m_depth));
    m_rateAttachment->setValueAsCompleteGesture(defaultValue(m_rate));
}

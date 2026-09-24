#include "GoniometerComponent.h"

namespace
{
    // circle radius = this fraction of half the smaller bounds dimension, so the
    // outer grid circle never quite touches the component edge (a fraction, not a
    // pixel size, so it does not need to scale with scaleFactor)
    constexpr float kCircleMarginFraction = 0.95f;

    // cos(45 deg) = sin(45 deg): projects the L and R grid diagonals onto the S/M axes
    constexpr float kCos45Deg = 0.70710678f;

    // oldest/newest history point opacity ("phosphor" persistence: newer = brighter)
    constexpr float kOldestPointAlpha = 0.10f;
    constexpr float kNewestPointAlpha = 0.65f;

    // pixel sizes at scaleFactor 1.0
    constexpr float kPointDiameter = 2.0f;
    constexpr float kAxisLineThickness = 1.0f;
    constexpr float kDiagonalLineThickness = 0.5f;

    // "L"/"R"/"M" axis labels: hand-tuned box size and gap from the axis they label
    constexpr float kLabelBoxWidth = 20.0f;
    constexpr float kLabelBoxHeight = 12.0f;
    constexpr float kMLabelGapAboveCircle = 14.0f;
    constexpr float kLRLabelGapBeyondDiagonal = 12.0f;
}

GoniometerComponent::GoniometerComponent(StereoMeterState& stateToDisplay, juce::String labelText)
    : state(stateToDisplay), label(std::move(labelText))
{
}

void GoniometerComponent::refresh()
{
    state.getGoniometerFifo().drainInto(drainX, drainY);
    for (size_t i = 0; i < drainX.size(); ++i)
        history.emplace_back(drainX[i], drainY[i]);

    // points arrive one per audio sample, so afterglowTime_s converts directly to a
    // point count; recomputed every tick since sample rate or afterglowTime_s can change
    const size_t maxHistoryPoints = (size_t) juce::jmax(1.0, afterglowTime_s * state.getSampleRate());
    while (history.size() > maxHistoryPoints)
        history.pop_front();
}

juce::Point<float> GoniometerComponent::toScreen(float s, float m) const
{
    // the goniometer is rotated 45 degrees from the L/R axes: draw M (mono) upward and
    // S (side) to the right, so the plot fits a square bounds without wasted corners.
    // Uses contentBounds (set by paint(), after the panel title/border are reserved),
    // not getLocalBounds(), so the plotted points always line up with the grid.
    const float radius = 0.5f * juce::jmin(contentBounds.getWidth(), contentBounds.getHeight()) * kCircleMarginFraction;
    const float cx = contentBounds.getCentreX();
    const float cy = contentBounds.getCentreY();
    return { cx + s * radius, cy - m * radius };
}

void GoniometerComponent::paint(juce::Graphics& g)
{
    contentBounds = MeterLookAndFeel::drawPanel(g, getLocalBounds().toFloat(), label, scaleFactor);

    const float labelBoxWidth = kLabelBoxWidth * scaleFactor;
    const float labelBoxHeight = kLabelBoxHeight * scaleFactor;
    const float mLabelGap = kMLabelGapAboveCircle * scaleFactor;
    const float lrLabelGap = kLRLabelGapBeyondDiagonal * scaleFactor;
    const float pointDiameter = kPointDiameter * scaleFactor;

    // grid: outer circle, L/R diagonals (+-45 deg), M/S cross
    g.setColour(MeterLookAndFeel::grid);
    const float radius = 0.5f * juce::jmin(contentBounds.getWidth(), contentBounds.getHeight()) * kCircleMarginFraction;
    juce::Point<float> centre = contentBounds.getCentre();
    g.drawEllipse(centre.x - radius, centre.y - radius, 2.0f * radius, 2.0f * radius, kAxisLineThickness * scaleFactor);
    g.drawLine(centre.x, centre.y - radius, centre.x, centre.y + radius, kAxisLineThickness * scaleFactor); // M axis (mono)
    g.drawLine(centre.x - radius, centre.y, centre.x + radius, centre.y, kAxisLineThickness * scaleFactor); // S axis (side)
    const float d = radius * kCos45Deg;
    g.drawLine(centre.x - d, centre.y - d, centre.x + d, centre.y + d, kDiagonalLineThickness * scaleFactor); // L axis
    g.drawLine(centre.x - d, centre.y + d, centre.x + d, centre.y - d, kDiagonalLineThickness * scaleFactor); // R axis

    g.setColour(MeterLookAndFeel::text);
    g.setFont(MeterLookAndFeel::labelFontSize * scaleFactor);
    g.drawText("M", centre.x - 0.5f * labelBoxWidth, centre.y - radius - mLabelGap,
               labelBoxWidth, labelBoxHeight, juce::Justification::centred);
    g.drawText("L", centre.x - d - lrLabelGap, centre.y - d - labelBoxHeight,
               labelBoxWidth, labelBoxHeight, juce::Justification::centred);
    g.drawText("R", centre.x + d - labelBoxWidth + lrLabelGap * 0.5f, centre.y - d - labelBoxHeight,
               labelBoxWidth, labelBoxHeight, juce::Justification::centred);

    // points, oldest = dimmest ("phosphor" persistence)
    const int numPoints = (int) history.size();
    if (numPoints > 0)
    {
        int i = 0;
        for (const auto& p : history)
        {
            const float age = (float) i / (float) numPoints; // 0 = oldest, 1 = newest
            g.setColour(MeterLookAndFeel::meterGood.withAlpha(kOldestPointAlpha + (kNewestPointAlpha - kOldestPointAlpha) * age));
            auto screenPoint = toScreen(p.x, p.y);
            g.fillEllipse(screenPoint.x - 0.5f * pointDiameter, screenPoint.y - 0.5f * pointDiameter, pointDiameter, pointDiameter);
            ++i;
        }
    }
}

void GoniometerComponent::resized()
{
}

#include "GoniometerComponent.h"

GoniometerComponent::GoniometerComponent(StereoMeterState& stateToDisplay, juce::String labelText)
    : state(stateToDisplay), label(std::move(labelText))
{
    startTimerHz(30);
}

GoniometerComponent::~GoniometerComponent()
{
    stopTimer();
}

void GoniometerComponent::timerCallback()
{
    state.getGoniometerFifo().drainInto(drainX, drainY);
    for (size_t i = 0; i < drainX.size(); ++i)
        history.emplace_back(drainX[i], drainY[i]);

    while ((int) history.size() > maxHistoryPoints)
        history.pop_front();

    repaint();
}

juce::Point<float> GoniometerComponent::toScreen(float s, float m) const
{
    // the goniometer is rotated 45 degrees from the L/R axes: draw M (mono) upward and
    // S (side) to the right, so the plot fits a square bounds without wasted corners
    auto bounds = getLocalBounds().toFloat();
    const float radius = 0.5f * juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.95f;
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    return { cx + s * radius, cy - m * radius };
}

void GoniometerComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colours::black);

    // grid: outer circle, L/R diagonals (+-45 deg), M/S cross
    g.setColour(juce::Colours::darkgrey);
    const float radius = 0.5f * juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.95f;
    juce::Point<float> centre = bounds.getCentre();
    g.drawEllipse(centre.x - radius, centre.y - radius, 2.0f * radius, 2.0f * radius, 1.0f);
    g.drawLine(centre.x, centre.y - radius, centre.x, centre.y + radius, 1.0f); // M axis (mono)
    g.drawLine(centre.x - radius, centre.y, centre.x + radius, centre.y, 1.0f); // S axis (side)
    const float d = radius * 0.70710678f; // cos(45 deg) = sin(45 deg)
    g.drawLine(centre.x - d, centre.y - d, centre.x + d, centre.y + d, 0.5f); // L axis
    g.drawLine(centre.x - d, centre.y + d, centre.x + d, centre.y - d, 0.5f); // R axis

    g.setColour(juce::Colours::grey);
    g.setFont(11.0f);
    g.drawText("M", centre.x - 8, centre.y - radius - 14, 16, 12, juce::Justification::centred);
    g.drawText("L", centre.x - d - 14, centre.y - d - 12, 20, 12, juce::Justification::centred);
    g.drawText("R", centre.x + d - 6, centre.y - d - 12, 20, 12, juce::Justification::centred);

    // points, oldest = dimmest ("phosphor" persistence)
    const int numPoints = (int) history.size();
    if (numPoints > 0)
    {
        int i = 0;
        for (const auto& p : history)
        {
            const float age = (float) i / (float) numPoints; // 0 = oldest, 1 = newest
            g.setColour(juce::Colours::limegreen.withAlpha(0.10f + 0.65f * age));
            auto screenPoint = toScreen(p.x, p.y);
            g.fillEllipse(screenPoint.x - 1.0f, screenPoint.y - 1.0f, 2.0f, 2.0f);
            ++i;
        }
    }

    if (label.isNotEmpty())
    {
        g.setColour(juce::Colours::white);
        g.setFont(13.0f);
        g.drawText(label, getLocalBounds().removeFromTop(16), juce::Justification::centred);
    }
}

void GoniometerComponent::resized()
{
}

#include "BroadbandPlayground.h"
#include "PlaygroundStyle.h"
#include "../PluginSettings.h"
#include "../algorithms/MSWidthBroadband.h"

namespace
{
    const juce::String kDegree = juce::String::fromUTF8("\xc2\xb0");
    const juce::String kPlusMinus = juce::String::fromUTF8("\xc2\xb1");
}

void StereoImageView::setWidthPercent(float widthPercent)
{
    m_widthPercent = widthPercent;
    repaint();
}

void StereoImageView::paint(juce::Graphics& g)
{
    const auto style = PlaygroundStyle::of(*this);
    auto bounds = getLocalBounds().toFloat();
    g.setColour(style.background);
    g.fillRoundedRectangle(bounds, PlaygroundStyle::kCornerSize * m_scale);

    const float fontSize = PlaygroundStyle::kFontSize * m_scale;
    const float lineHeight = fontSize * 1.3f;
    auto area = bounds.reduced(6.0f * m_scale);
    auto textArea = area.removeFromBottom(2.0f * lineHeight);
    area.removeFromTop(lineHeight); // room for the loudspeaker labels

    // Listener at the bottom centre, front half-circle above. JUCE arc angles run
    // clockwise from 12 o'clock, so negative angles are to the listener's left.
    const float radius = juce::jmin(area.getWidth() * 0.5f, area.getHeight());
    const juce::Point<float> centre(area.getCentreX(), area.getBottom());
    const auto pointAt = [&](float angleRad, float r)
    {
        return centre + juce::Point<float>(r * std::sin(angleRad), -r * std::cos(angleRad));
    };

    juce::Path frontArc;
    frontArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f,
                           -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
    g.setColour(style.grid);
    g.strokePath(frontArc, juce::PathStrokeType(1.0f * m_scale));

    // Where hard-panned sources are heard at the current width.
    const float phiRad = juce::degreesToRadians(MSWidthBroadband::hardPannedSourceAngleDeg(m_widthPercent));
    g.setColour(style.accent);
    if (phiRad < 0.01f)
    {
        g.drawLine(juce::Line<float>(centre, pointAt(0.0f, radius)), 2.5f * m_scale);
    }
    else
    {
        juce::Path wedge;
        wedge.startNewSubPath(centre);
        wedge.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, -phiRad, phiRad);
        wedge.closeSubPath();
        g.setColour(style.accent.withAlpha(0.35f));
        g.fillPath(wedge);
        g.setColour(style.accent);
        g.drawLine(juce::Line<float>(centre, pointAt(-phiRad, radius)), 2.0f * m_scale);
        g.drawLine(juce::Line<float>(centre, pointAt(phiRad, radius)), 2.0f * m_scale);
    }

    // Loudspeakers at +-30 degrees, dashed lines from the listener.
    g.setFont(juce::Font(juce::FontOptions(fontSize)));
    const float speakerRad = juce::degreesToRadians(MSWidthBroadband::kSpeakerAngleDeg);
    const float speakerSize = 7.0f * m_scale;
    for (float side : { -1.0f, 1.0f })
    {
        const auto speaker = pointAt(side * speakerRad, radius);
        const float dashes[] = { 3.0f * m_scale, 3.0f * m_scale };
        g.setColour(style.grid);
        g.drawDashedLine(juce::Line<float>(centre, speaker), dashes, 2, 1.0f * m_scale);
        g.setColour(style.text);
        g.fillRect(juce::Rectangle<float>(speakerSize, speakerSize).withCentre(speaker));
        const auto label = juce::Rectangle<float>(2.0f * fontSize, lineHeight)
                               .withCentre(speaker.translated(0.0f, -lineHeight * 0.8f));
        g.drawText(side < 0.0f ? "L" : "R", label, juce::Justification::centred);
    }
    g.setColour(style.text);
    g.fillEllipse(juce::Rectangle<float>(8.0f * m_scale, 8.0f * m_scale).withCentre(centre));

    // Readout: the side signal's gain and the angle of hard-panned sources.
    const float sideDb = MSWidthBroadband::sideGainDb(m_widthPercent);
    const juce::String sideText = sideDb <= -99.0f ? juce::String("-inf")
                                : (sideDb > 0.05f ? "+" : "") + juce::String(sideDb, 1);
    const juce::String angleText = phiRad < 0.01f
        ? juce::String("Mono")
        : "Hard L/R at " + kPlusMinus + juce::String(juce::roundToInt(juce::radiansToDegrees(phiRad))) + kDegree;
    g.drawFittedText("Side gain " + sideText + " dB", textArea.removeFromTop(lineHeight).toNearestInt(),
                     juce::Justification::centred, 1);
    g.drawFittedText(angleText, textArea.toNearestInt(), juce::Justification::centred, 1);
}

BroadbandPlayground::BroadbandPlayground(juce::AudioProcessorValueTreeState& apvts, const StereoAlgorithm& algorithm)
    : AlgorithmPlayground(apvts),
      m_widthKnob(apvts, algorithm.getParamSpecs()[MSWidthBroadband::kWidth])
{
    addAndMakeVisible(m_widthKnob);
    addAndMakeVisible(m_imageView);
    watchParameter(algorithm.getParamSpecs()[MSWidthBroadband::kWidth].id,
                   [this](float widthPercent) { m_imageView.setWidthPercent(widthPercent); });
}

void BroadbandPlayground::resized()
{
    m_widthKnob.setKnobLayout(scaled(g_widthKnobSize), scaled(g_widthKnobLabelHeight), scaled(g_widthKnobSize));
    m_imageView.setScaleFactor(m_scale);

    auto area = getLocalBounds();
    auto knobColumn = area.removeFromLeft(m_widthKnob.getPreferredWidth());
    m_widthKnob.setBounds(knobColumn.withSizeKeepingCentre(knobColumn.getWidth(), m_widthKnob.getPreferredHeight()));
    area.removeFromLeft(scaled(g_paramKnobGap));
    m_imageView.setBounds(area);
}

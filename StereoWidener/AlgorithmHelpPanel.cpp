#include "AlgorithmHelpPanel.h"
#include <cmath>

AlgorithmHelpPanel::AlgorithmHelpPanel(const juce::String& title, const juce::String& description)
    : m_title(title)
{
    juce::AttributedString attributed;
    attributed.append(description, juce::Font(juce::FontOptions(kBodyFontSize)), juce::Colours::whitesmoke);
    attributed.setWordWrap(juce::AttributedString::byWord);
    attributed.setLineSpacing(2.0f);
    m_bodyLayout.createLayout(attributed, (float) (width - 2 * kPadding));

    const int totalHeight = kPadding + kTitleHeight + kTitleBodyGap
                           + juce::roundToInt(std::ceil(m_bodyLayout.getHeight())) + kPadding;
    setSize(width, totalHeight);
}

void AlgorithmHelpPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(kTitleFontSize)).boldened());
    g.drawText(m_title, kPadding, kPadding, getWidth() - 2 * kPadding, kTitleHeight, juce::Justification::centredLeft);

    m_bodyLayout.draw(g, juce::Rectangle<float>((float) kPadding, (float) (kPadding + kTitleHeight + kTitleBodyGap),
                                                 (float) (getWidth() - 2 * kPadding), m_bodyLayout.getHeight()));
}

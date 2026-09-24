#include "LevelMeterComponent.h"

namespace
{
    constexpr float kBarSideMarginFraction = 0.15f; // horizontal gap between adjacent bars, as a fraction of bar width
    constexpr float kNameLabelHeight = 14.0f;  // "L"/"R"/"M"/"S" label above each bar
    constexpr float kValueLabelHeight = 11.0f; // numeric peak-dB readout, between the name and the bar
    constexpr float kWidthReadoutHeight = 16.0f; // "S-M x.x dB" row below the bars
    constexpr int kNumBars = 4; // L, R, M, S

    constexpr float kScaleColumnWidth = 26.0f; // dB scale, to the right of the bars
    constexpr float kTickLabelHeight = 10.0f;
    const float kTickValuesDb[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f, -60.0f };

    // Zone boundaries for the RMS bar colouring. A tasteful convention (matching common
    // DAW meters), not an EBU/ITU standard: green up to -18 dBFS (comfortable headroom),
    // amber -18..-6 dBFS (getting hot), red above -6 dBFS (close to clipping).
    constexpr float kCautionThresholdDb = -18.0f;
    constexpr float kDangerThresholdDb = -6.0f;

    juce::Colour zoneColourForDb(float db)
    {
        if (db < kCautionThresholdDb)
            return MeterLookAndFeel::meterGood;
        if (db < kDangerThresholdDb)
            return MeterLookAndFeel::meterCaution;
        return MeterLookAndFeel::meterDanger;
    }

    juce::String formatDb(float db)
    {
        return db <= -99.0f ? "-inf" : juce::String(db, 1);
    }
}

LevelMeterComponent::LevelMeterComponent(StereoMeterState& stateToDisplay, juce::String labelText)
    : state(stateToDisplay), label(std::move(labelText))
{
}

float LevelMeterComponent::dbToFraction(float db) const noexcept
{
    return juce::jlimit(0.0f, 1.0f, (db - rangeMinDb) / (rangeMaxDb - rangeMinDb));
}

void LevelMeterComponent::drawScale(juce::Graphics& g, juce::Rectangle<float> barsBounds,
                                     juce::Rectangle<float> scaleBounds) const
{
    g.setFont(MeterLookAndFeel::smallLabelFontSize);
    for (float db : kTickValuesDb)
    {
        if (db < rangeMinDb || db > rangeMaxDb)
            continue;
        const float y = barsBounds.getBottom() - dbToFraction(db) * barsBounds.getHeight();

        g.setColour(MeterLookAndFeel::grid);
        g.drawHorizontalLine((int) y, barsBounds.getX(), barsBounds.getRight());

        g.setColour(MeterLookAndFeel::text);
        g.drawText(juce::String((int) db), scaleBounds.getX(), y - 0.5f * kTickLabelHeight,
                   scaleBounds.getWidth(), kTickLabelHeight, juce::Justification::centredLeft);
    }
}

void LevelMeterComponent::drawBar(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& name,
                                   float rmsDb, float peakDb) const
{
    g.setColour(MeterLookAndFeel::text);
    g.setFont(MeterLookAndFeel::labelFontSize);
    g.drawText(name, bounds.removeFromTop(kNameLabelHeight), juce::Justification::centred);

    // peak text follows the same colour zone as the peak line drawn below (green/amber/red)
    const auto peakColour = zoneColourForDb(peakDb);
    g.setColour(peakColour);
    g.setFont(MeterLookAndFeel::smallLabelFontSize);
    g.drawText(formatDb(peakDb), bounds.removeFromTop(kValueLabelHeight), juce::Justification::centred);

    // "bounds" is now the meter area proper, matching drawScale()'s barsBounds (both are
    // barsArea with the name/value label rows trimmed off the top)
    auto barArea = bounds.reduced(bounds.getWidth() * kBarSideMarginFraction, 0.0f);

    // RMS bar, filled in up to three colour zones (see kCautionThresholdDb/kDangerThresholdDb)
    const float rmsFraction = dbToFraction(rmsDb);
    const float cautionFraction = dbToFraction(kCautionThresholdDb);
    const float dangerFraction = dbToFraction(kDangerThresholdDb);
    auto fillZone = [&](float fromFraction, float toFraction, juce::Colour colour)
    {
        const float top = juce::jmin(toFraction, rmsFraction);
        if (top <= fromFraction)
            return;
        g.setColour(colour);
        g.fillRect(barArea.getX(), barArea.getBottom() - top * barArea.getHeight(),
                   barArea.getWidth(), (top - fromFraction) * barArea.getHeight());
    };
    fillZone(0.0f, cautionFraction, MeterLookAndFeel::meterGood);
    fillZone(cautionFraction, dangerFraction, MeterLookAndFeel::meterCaution);
    fillZone(dangerFraction, 1.0f, MeterLookAndFeel::meterDanger);

    // peak: a thin horizontal line at the peak level, held in place for a while after the
    // last new peak before it starts to fall again (see StereoMeterState::setPeakHoldTime)
    const float peakY = barArea.getBottom() - dbToFraction(peakDb) * barArea.getHeight();
    g.setColour(peakColour);
    g.drawHorizontalLine((int) peakY, barArea.getX(), barArea.getRight());
}

void LevelMeterComponent::paint(juce::Graphics& g)
{
    auto content = MeterLookAndFeel::drawPanel(g, getLocalBounds().toFloat(), label);

    auto widthReadoutRow = content.removeFromBottom(kWidthReadoutHeight);
    auto scaleColumn = content.removeFromRight(kScaleColumnWidth);
    auto barsArea = content; // 4 bars: name label + value label + coloured meter, left to right

    auto meterArea = barsArea.withTrimmedTop(kNameLabelHeight + kValueLabelHeight);
    drawScale(g, meterArea, scaleColumn);

    const float barWidth = barsArea.getWidth() / (float) kNumBars;
    drawBar(g, barsArea.removeFromLeft(barWidth), "L",
            state.getRmsDb(StereoMeterState::Left), state.getPeakDb(StereoMeterState::Left));
    drawBar(g, barsArea.removeFromLeft(barWidth), "R",
            state.getRmsDb(StereoMeterState::Right), state.getPeakDb(StereoMeterState::Right));
    drawBar(g, barsArea.removeFromLeft(barWidth), "M",
            state.getRmsDb(StereoMeterState::Mid), state.getPeakDb(StereoMeterState::Mid));
    drawBar(g, barsArea.removeFromLeft(barWidth), "S",
            state.getRmsDb(StereoMeterState::Side), state.getPeakDb(StereoMeterState::Side));

    // width estimate readout (S - M in dB): 0 dB = equal power, very negative = narrow/mono
    g.setColour(MeterLookAndFeel::text);
    g.setFont(MeterLookAndFeel::labelFontSize);
    juce::String widthText = "S-M " + juce::String(state.getWidthEstimateDb(), 1) + " dB";
    g.drawText(widthText, widthReadoutRow, juce::Justification::centred);
}

void LevelMeterComponent::resized()
{
}

#include "GoniometerComponent.h"

#include <cmath>

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

    // Upper bound on how many points paint() draws per frame, regardless of how many
    // history actually holds (which can reach afterglowTime_s * sampleRate -- up to
    // ~88000 at the parameter's 2 s maximum and 44.1 kHz). Drawing that many individual
    // fillEllipse() calls every repaint measured ~280 ms per frame at the maximum in a
    // Debug build -- catastrophically slower than the 30 Hz repaint rate needs, and not
    // something a Release build alone would fix (a Debug build measured ~9 us per
    // fillEllipse() call once past JUCE's one-time startup cost; even 3000 points, this
    // constant's first value, still cost ~28 ms, most of the 33 ms/frame budget at
    // 30 Hz, with two more meter components sharing the same paint pass). history is
    // far denser than the display has pixels for anyway, so a fixed stride (picked in
    // paint() from history.size() / kMaxDrawnPoints) keeps cost bounded with no visible
    // loss of detail: nearby points already overlap heavily on screen.
    constexpr int kMaxDrawnPoints = 1500;

    // pixel sizes at scaleFactor 1.0
    constexpr float kPointDiameter = 2.0f;
    constexpr float kAxisLineThickness = 1.0f;
    constexpr float kDiagonalLineThickness = 0.5f;

    // "L"/"R"/"M" axis labels: hand-tuned box size and gap from the axis they label
    constexpr float kLabelBoxWidth = 20.0f;
    constexpr float kLabelBoxHeight = 12.0f;
    constexpr float kMLabelGapAboveCircle = 14.0f;
    constexpr float kLRLabelGapBeyondDiagonal = 12.0f;

    // optional corner text (see setCornerText()): stacked lines anchored at the panel's
    // bottom-left corner, in a grey a bit darker than the usual label text so it reads
    // as a quiet footnote rather than competing with the actual meter readouts
    constexpr float kCornerTextMargin = 4.0f;
    constexpr float kCornerTextFontSize = 10.0f;
    constexpr float kCornerTextLineHeight = 12.0f;
    const juce::Colour kCornerTextColour = MeterLookAndFeel::text.darker(0.4f);
}

GoniometerComponent::GoniometerComponent(StereoMeterState& stateToDisplay, juce::String labelText)
    : state(stateToDisplay), label(std::move(labelText))
{
}

void GoniometerComponent::drainSeries(StereoMeterState& s, std::deque<juce::Point<float>>& hist,
                                       std::vector<float>& scratchX, std::vector<float>& scratchY)
{
    s.getGoniometerFifo().drainInto(scratchX, scratchY);
    for (size_t i = 0; i < scratchX.size(); ++i)
        hist.emplace_back(scratchX[i], scratchY[i]);
}

void GoniometerComponent::refresh()
{
    drainSeries(state, history, drainX, drainY);
    if (secondaryState != nullptr)
        drainSeries(*secondaryState, secondaryHistory, drainX2, drainY2);

    // points arrive one per audio sample, so afterglowTime_s converts directly to a
    // point count; recomputed every tick since sample rate or afterglowTime_s can
    // change. Both series share the primary's sample rate (in practice input and
    // output always run at the same rate).
    const size_t maxHistoryPoints = (size_t) juce::jmax(1.0, afterglowTime_s * state.getSampleRate());
    while (history.size() > maxHistoryPoints)
        history.pop_front();
    while (secondaryHistory.size() > maxHistoryPoints)
        secondaryHistory.pop_front();
}

void GoniometerComponent::setSecondarySeries(StereoMeterState* stateToDisplay, juce::Colour colour)
{
    secondaryState = stateToDisplay;
    secondaryColour = colour;
    secondaryHistory.clear();
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
    // A hard-left signal (R=0) has S = M = L/2 (same sign): it traces the "/" diagonal
    // (bottom-left to top-right), ending near the top-right corner. A hard-right signal
    // (L=0) has S = -M (opposite sign): it traces the "\" diagonal (top-left to
    // bottom-right), ending near the top-left corner. The L/R text labels below are
    // placed accordingly, at the corner each one's own diagonal actually points to --
    // not at the corner that would match the (misleading) name of the *other* diagonal.
    const float d = radius * kCos45Deg;
    g.drawLine(centre.x - d, centre.y - d, centre.x + d, centre.y + d, kDiagonalLineThickness * scaleFactor); // "\", hard-right diagonal
    g.drawLine(centre.x - d, centre.y + d, centre.x + d, centre.y - d, kDiagonalLineThickness * scaleFactor); // "/", hard-left diagonal

    g.setColour(MeterLookAndFeel::text);
    g.setFont(MeterLookAndFeel::labelFontSize * scaleFactor);
    g.drawText("M", centre.x - 0.5f * labelBoxWidth, centre.y - radius - mLabelGap,
               labelBoxWidth, labelBoxHeight, juce::Justification::centred);
    g.drawText("R", centre.x - d - lrLabelGap, centre.y - d - labelBoxHeight,
               labelBoxWidth, labelBoxHeight, juce::Justification::centred);
    g.drawText("L", centre.x + d - labelBoxWidth + lrLabelGap * 0.5f, centre.y - d - labelBoxHeight,
               labelBoxWidth, labelBoxHeight, juce::Justification::centred);

    if (!cornerTextLines.isEmpty())
    {
        const float margin = kCornerTextMargin * scaleFactor;
        const float lineHeight = kCornerTextLineHeight * scaleFactor;
        // Nearly the full panel width, not just a fixed fraction of it: the text sits
        // right at the bottom margin, well clear of the circle (which occupies the
        // panel's centre), so there is no risk of overlapping it -- and a smaller
        // goniometer (StereoWidener's, ~60% of StereoAnalyzer's size) needs every pixel
        // it can get to fit a one-line build/version footer without truncating.
        const float textWidth = contentBounds.getWidth() - 2.0f * margin;

        g.setColour(kCornerTextColour);
        g.setFont(kCornerTextFontSize * scaleFactor);
        for (int i = 0; i < cornerTextLines.size(); ++i)
        {
            // stack upward from the bottom margin, so the last line sits lowest and the
            // array's own top-to-bottom order matches the drawn reading order
            const float y = contentBounds.getBottom() - margin - (float) (cornerTextLines.size() - i) * lineHeight;
            g.drawText(cornerTextLines[i], contentBounds.getX() + margin, y, textWidth, lineHeight,
                       juce::Justification::centredLeft);
        }
    }

    // points, oldest = dimmest ("phosphor" persistence). Primary series first, then the
    // optional secondary series on top (e.g. StereoWidener's output over its input), so
    // the more relevant/recent signal isn't hidden underneath the other.
    drawSeries(g, history, primaryColour, pointDiameter);
    if (secondaryState != nullptr)
        drawSeries(g, secondaryHistory, secondaryColour, pointDiameter);
}

void GoniometerComponent::drawSeries(juce::Graphics& g, const std::deque<juce::Point<float>>& hist,
                                      juce::Colour colour, float pointDiameter) const
{
    // Stride through history rather than drawing every point, so paint() cost stays
    // bounded (see kMaxDrawnPoints) however many points afterglowTime_s currently keeps
    // in history; age is computed from the real position in the full history, not the
    // decimated draw order, so the fade timing itself is unaffected by the stride.
    const int numPoints = (int) hist.size();
    if (numPoints == 0)
        return;

    const int stride = juce::jmax(1, numPoints / kMaxDrawnPoints);
    for (int i = 0; i < numPoints; i += stride)
    {
        const float age = (float) i / (float) numPoints; // 0 = oldest, 1 = newest
        const auto& p = hist[(size_t) i]; // std::deque: O(1) random access

        // sqrt(S^2 + M^2) is the point's distance from the centre in normalised (-1..1)
        // coordinates, exactly matching the grid circle's radius of 1 (both axes use
        // the same screen-pixel scale, see toScreen()). Above 0 dBFS input this can
        // exceed 1: clamp the point onto the circle rather than letting it land
        // anywhere in the component's rectangle, and colour it red -- a visible
        // overload marker instead of a silently misleading position, regardless of
        // which series it belongs to.
        float s = p.x, m = p.y;
        const float magnitude = std::sqrt(s * s + m * m);
        const bool isOverload = magnitude > 1.0f;
        if (isOverload)
        {
            s /= magnitude;
            m /= magnitude;
        }

        const auto baseColour = isOverload ? MeterLookAndFeel::meterDanger : colour;
        g.setColour(baseColour.withAlpha(kOldestPointAlpha + (kNewestPointAlpha - kOldestPointAlpha) * age));
        auto screenPoint = toScreen(s, m);
        g.fillEllipse(screenPoint.x - 0.5f * pointDiameter, screenPoint.y - 0.5f * pointDiameter, pointDiameter, pointDiameter);
    }
}

void GoniometerComponent::resized()
{
}

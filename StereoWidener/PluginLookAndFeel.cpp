#include "PluginLookAndFeel.h"

namespace
{
    // Day ("Jade" house style) -- see Libs/TGMTools/JadeLookAndFeel.h for the values
    // this matches (not literally shared/included: that class is defined in a
    // different plugin's own build target and hardcodes its palette directly into
    // drawRotarySlider() rather than exposing it, so it cannot itself serve two
    // different themes -- see PluginLookAndFeel.h).
    const juce::Colour kDayBackground { juce::Colour::fromFloatRGBA(0.95f, 0.95f, 0.95f, 1.0f) };  // JadeWhite
    const juce::Colour kDayText       { juce::Colour::fromFloatRGBA(0.357f, 0.373f, 0.341f, 1.0f) }; // JadeGray

    // Knob/button fill: a much LIGHTER grey than kDayText -- an earlier version reused
    // kDayText directly here, which read as "too dark, too high contrast" against
    // kDayBackground (reported after seeing it rendered). This is JadeGray blended
    // most of the way towards JadeWhite, keeping the same hue family while giving
    // knobs/buttons a soft light-grey fill instead of a near-black one.
    const juce::Colour kDayKnobDisc = kDayText.interpolatedWith(kDayBackground, 0.6f);

    // the "kept" red handle/pointer colour -- identical in both themes, see file header
    const juce::Colour kAccentRed { juce::Colour::fromFloatRGBA(0.890f, 0.024f, 0.075f, 1.0f) }; // JadeRed

    // Text sitting directly on the red accent colour (a button's toggled-on state, a
    // highlighted popup-menu row): white in both themes -- kAccentRed is deliberately
    // kept dark/saturated enough that this always reads clearly, so this one colour
    // does not need to vary by theme the way everything else here does.
    const juce::Colour kTextOnAccent = juce::Colours::white;

    // Night: much darker than the plugin's previous plain juce::Colours::darkgrey
    // background, per explicit request ("closer to black").
    const juce::Colour kNightBackground { juce::Colour::fromFloatRGBA(0.07f, 0.07f, 0.07f, 1.0f) };
    const juce::Colour kNightText       = juce::Colours::whitesmoke;

    // Knob/button fill: a LITTLE LIGHTER than kNightBackground, the same relative
    // relationship Day mode already has (there, kDayKnobDisc is visibly different
    // from kDayBackground). An earlier version matched this to
    // shared/metering/MeterLookAndFeel.h's own pure-black `background` constant
    // exactly, but with kNightBackground *also* darkened to near-black in the same
    // pass, knobs/buttons ended up barely distinguishable from the window itself --
    // corrected after seeing it rendered. Kept clearly darker than Day's own
    // kDayKnobDisc, so the overall "much darker" request still holds.
    const juce::Colour kNightKnobDisc { juce::Colour::fromFloatRGBA(0.18f, 0.18f, 0.18f, 1.0f) };
}

StereoWidenerLookAndFeel::StereoWidenerLookAndFeel(Theme theme)
    : m_theme(theme)
{
    applyTheme();
}

void StereoWidenerLookAndFeel::setTheme(Theme newTheme)
{
    if (m_theme == newTheme)
        return;
    m_theme = newTheme;
    applyTheme();
}

void StereoWidenerLookAndFeel::applyTheme()
{
    const bool isDay = (m_theme == Theme::Day);
    const juce::Colour background = isDay ? kDayBackground : kNightBackground;
    const juce::Colour text       = isDay ? kDayText       : kNightText;
    m_knobDiscColour   = isDay ? kDayKnobDisc : kNightKnobDisc;
    m_knobHandleColour = kAccentRed;
    // Deliberately its own colour: this must contrast against m_knobDiscColour (a
    // TextButton's fill, buttonColourId below), NOT against the window background --
    // aliasing it to `text` here once made Day mode's button labels invisible (Day's
    // `text` and `m_knobDiscColour` were the same dark shade at the time), since a
    // button's own fill was dark in Day mode but light windows/labels expect dark
    // text -- two different roles that happened to collide. See the header member
    // comment. Day's button fill is now a light grey (kDayKnobDisc, see above), so
    // its own text needs to be dark again -- kDayText, not kDayBackground.
    m_buttonTextColour = isDay ? kDayText : kNightText;

    setColour(juce::ResizableWindow::backgroundColourId, background);

    setColour(juce::Slider::ColourIds::backgroundColourId, m_knobDiscColour);
    setColour(juce::Slider::ColourIds::thumbColourId, m_knobHandleColour);
    setColour(juce::Slider::ColourIds::trackColourId, m_knobDiscColour);
    setColour(juce::Slider::ColourIds::textBoxTextColourId, text);
    setColour(juce::Slider::ColourIds::textBoxBackgroundColourId, background);
    setColour(juce::Slider::ColourIds::textBoxOutlineColourId, m_knobDiscColour);

    setColour(juce::Label::ColourIds::textColourId, text);

    setColour(juce::TextButton::ColourIds::buttonColourId, m_knobDiscColour);
    setColour(juce::TextButton::ColourIds::buttonOnColourId, m_knobHandleColour);
    setColour(juce::TextButton::ColourIds::textColourOnId, kTextOnAccent);
    setColour(juce::TextButton::ColourIds::textColourOffId, m_buttonTextColour);

    setColour(juce::ComboBox::ColourIds::backgroundColourId, isDay ? background.darker(0.05f) : background.brighter(0.15f));
    setColour(juce::ComboBox::ColourIds::textColourId, text);
    setColour(juce::ComboBox::ColourIds::outlineColourId, m_knobDiscColour);
    setColour(juce::ComboBox::ColourIds::arrowColourId, text);

    setColour(juce::PopupMenu::ColourIds::backgroundColourId, background);
    setColour(juce::PopupMenu::ColourIds::textColourId, text);
    setColour(juce::PopupMenu::ColourIds::highlightedBackgroundColourId, m_knobHandleColour);
    setColour(juce::PopupMenu::ColourIds::highlightedTextColourId, kTextOnAccent);

    setColour(juce::TextEditor::ColourIds::backgroundColourId, background);
    setColour(juce::TextEditor::ColourIds::textColourId, text);
    setColour(juce::TextEditor::ColourIds::outlineColourId, m_knobDiscColour);
    setColour(juce::TextEditor::ColourIds::focusedOutlineColourId, m_knobHandleColour);
}

void StereoWidenerLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                                float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    juce::ignoreUnused(slider);
    const auto radius = (float) juce::jmin(width / 2, height / 2) - 4.0f;
    const auto centreX = (float) x + (float) width * 0.5f;
    const auto centreY = (float) y + (float) height * 0.5f;
    const auto rx = centreX - radius;
    const auto ry = centreY - radius;
    const auto rw = 2.0f * radius;
    const auto angle = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * sliderPos;

    // disc -- same geometry as Libs/TGMTools/JadeLookAndFeel's own rotary knob, but
    // reading m_knobDiscColour/m_knobHandleColour (set per-theme in applyTheme())
    // instead of hardcoding Jade's own palette, so one routine serves both themes.
    g.setColour(m_knobDiscColour);
    g.fillEllipse(rx, ry, rw, rw);

    // outline, brightening with sliderPos -- shows how far the knob is turned even
    // before looking at the pointer
    g.setColour(m_knobHandleColour.withMultipliedBrightness(0.3f + 0.7f * sliderPos));
    g.drawEllipse(rx, ry, rw, rw, (float) juce::jmax((int) ((float) width * 0.07f), 5));

    // pointer -- the "handle", kept red in both themes (see file header)
    const int pointSize = juce::jmax(width / 6, 10);
    juce::Path p;
    p.addEllipse(-pointSize / 2.0f, -0.95f * radius, (float) pointSize, (float) pointSize);
    p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));

    g.setColour(m_knobHandleColour);
    g.fillPath(p);
}

void StereoWidenerLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                                    bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    LookAndFeel_V4::drawButtonBackground(g, button, backgroundColour, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    if (button.getComponentID() != themeToggleComponentID)
        return;

    // A little lighter than the button's own fill (visible in Night mode, where the
    // fill is close in brightness to its surroundings; a no-op in practice in Day
    // mode, where the fill is already near-white and so can't get much brighter --
    // matching the report that the Day button needed no such fix).
    g.setColour(backgroundColour.brighter(0.3f));
    g.drawRoundedRectangle(button.getLocalBounds().toFloat().reduced(0.5f, 0.5f), 6.0f, 1.0f);
}

/**
 * @file PluginLookAndFeel.h
 * @brief Day/night GUI theme (Phase 6, GUI polish): two colour palettes sharing one
 *        custom rotary-knob drawing style, switchable at runtime.
 *
 * Day mode is the "Jade" house style already used by other Jade-branded plugins in
 * this codebase (see Libs/TGMTools/JadeLookAndFeel.h): a white background, a grey
 * knob disc, and a red handle/pointer indicating the current value -- a deliberately
 * different knob style from JUCE's stock rotary arc-slider look. Not reused verbatim
 * (that class hardcodes its palette directly into drawRotarySlider() rather than
 * reading it via colour IDs/members, so it cannot itself serve two different
 * palettes); this class factors the SAME disc+pointer drawing style out into one
 * routine driven by two member colours, so day and night share the drawing code and
 * differ only in which colours are set.
 *
 * Night mode keeps this plugin's existing dark background (unchanged look for anyone
 * who never touches the toggle -- Night is the default, see GlobalSettings::
 * getUseDayTheme()), but recolours the knob disc from white/light (the stock JUCE
 * dark-scheme thumb colour, `LookAndFeel_V4::getDarkColourScheme()`) to a mid grey,
 * and gives it the same red handle/pointer as Day mode -- the accent colour is a
 * constant across both themes; only the surrounding palette changes.
 *
 * Deliberately does NOT touch the metering/goniometer components (shared/metering):
 * those draw entirely from their own independent
 * MeterLookAndFeel colour constants (shared/metering/MeterLookAndFeel.h), never via
 * juce::LookAndFeel/findColour(), so a theme switch cannot affect them -- exactly the
 * "this will not change the displays" requirement this feature was built to.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class StereoWidenerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    enum class Theme { Day, Night };

    explicit StereoWidenerLookAndFeel(Theme theme);

    void setTheme(Theme newTheme);
    Theme getTheme() const noexcept { return m_theme; }

    // Set as PluginEditor's theme-toggle button's own Component ID, so
    // drawButtonBackground() below can recognise it without a dedicated public flag.
    // A plain C string literal, not a static juce::String: a juce::String with static
    // storage duration at namespace/class scope is a known cross-translation-unit
    // static-initialisation-order hazard (its constructor can run before or after
    // JUCE's own internal string machinery is ready, depending on link order) --
    // avoided entirely by never constructing one outside function scope.
    static constexpr const char* themeToggleComponentID = "themeToggle";

    // Which icon the theme toggle shows: set as a component property ("sun" or
    // "moon") on that button; drawButtonBackground() draws it as a vector icon in the
    // button's textColourOffId colour. Drawn, not a text glyph: the Unicode sun/moon
    // (U+2600/U+263E) are missing or too wide in some system fonts -- on Windows the
    // sun was drawn as "..." (elided text).
    static constexpr const char* themeIconProperty = "themeIcon";
    static void drawThemeIcon(juce::Graphics& g, juce::Rectangle<float> area, bool sun, juce::Colour colour);

private:
    void applyTheme();
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override;

    // Stock LookAndFeel_V4 draws no border at all for a standalone (non-segmented)
    // TextButton -- confirmed by reading its source before adding this override, not
    // assumed. Harmless for every ordinary (rectangular, labelled) button here, whose
    // fill already contrasts enough against its surroundings, but the small icon-only
    // theme-toggle button needs one: its fill can end up close in brightness to its
    // surroundings (e.g. Night mode's fill against the also-dark preset bar), and
    // without any stroke it visually disappears except for its glyph. Reuses the
    // stock fill first, then adds a border JUST for that one button.
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    // JUCE's slider value box always uses a 15 px font, however small the box, so
    // e.g. "1500 Hz" was cut off ("1500 ...") in the compact knobs' boxes. Sized to the
    // box height instead (at most 15 px, so larger boxes look as before).
    juce::Label* createSliderTextBox(juce::Slider& slider) override;

    Theme m_theme;
    juce::Colour m_knobDiscColour;
    juce::Colour m_knobHandleColour;  // the "red handle" -- same hue in both themes, see file header
    juce::Colour m_buttonTextColour;  // deliberately its OWN colour, not aliased to the
                                       // ambient label/knob-disc colour -- see PluginLookAndFeel.cpp
};

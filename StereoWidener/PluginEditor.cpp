
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginSettings.h"

namespace
{
    // The moon/sun icon colours are fixed, independent of theme -- a dark moon, a
    // bright sun -- rather than following the general button-text convention
    // (m_buttonTextColour), per explicit request. The button's own fill follows the
    // opposite rule: white specifically in Day mode (an explicit per-component
    // override, brighter than the general -- now light-grey -- Day button fill), and
    // the theme's own ambient button fill in Night mode (no override, so it always
    // exactly tracks whatever StereoWidenerLookAndFeel currently uses there).
    const juce::Colour kMoonColour { juce::Colour::fromFloatRGBA(0.12f, 0.14f, 0.22f, 1.0f) }; // dark navy
    const juce::Colour kSunColour  { juce::Colour::fromFloatRGBA(1.00f, 0.78f, 0.20f, 1.0f) };  // bright gold

    void applyThemeButtonStyle(juce::TextButton& button, StereoWidenerLookAndFeel::Theme theme)
    {
        // Shows the icon of the mode a click would switch TO (common toggle-icon
        // convention): a moon in Day mode, a sun in Night mode. Drawn by
        // StereoWidenerLookAndFeel::drawThemeIcon(), not a text glyph (see there).
        const bool showSun = theme != StereoWidenerLookAndFeel::Theme::Day;
        button.setButtonText({});
        button.getProperties().set(StereoWidenerLookAndFeel::themeIconProperty, showSun ? "sun" : "moon");
        button.setTitle(showSun ? "Switch to day theme" : "Switch to night theme"); // accessibility
        button.repaint();
        if (theme == StereoWidenerLookAndFeel::Theme::Day)
        {
            button.setColour(juce::TextButton::ColourIds::buttonColourId, juce::Colours::white);
            button.setColour(juce::TextButton::ColourIds::textColourOffId, kMoonColour);
        }
        else
        {
            button.removeColour(juce::TextButton::ColourIds::buttonColourId); // falls back to the ambient (black) button fill
            button.setColour(juce::TextButton::ColourIds::textColourOffId, kSunColour);
        }
    }
}

//==============================================================================
#if WITH_MIDIKEYBOARD
StereoWidenerAudioProcessorEditor::StereoWidenerAudioProcessorEditor (StereoWidenerAudioProcessor& p)
    : AudioProcessorEditor (&p),
        m_lookAndFeel(p.m_algo.getGlobalSettings().getUseDayTheme() ? StereoWidenerLookAndFeel::Theme::Day
                                                                     : StereoWidenerLookAndFeel::Theme::Night),
        m_processorRef (p), m_presetGUI(p.m_presets),
    	m_keyboard(m_processorRef.m_keyboardState, MidiKeyboardComponent::Orientation::horizontalKeyboard),
        m_wheels(p.m_wheelState), m_editor(p,*p.m_parameterVTS)
#else
StereoWidenerAudioProcessorEditor::StereoWidenerAudioProcessorEditor (StereoWidenerAudioProcessor& p)
    : AudioProcessorEditor (&p),
        m_lookAndFeel(p.m_algo.getGlobalSettings().getUseDayTheme() ? StereoWidenerLookAndFeel::Theme::Day
                                                                     : StereoWidenerLookAndFeel::Theme::Night),
        m_processorRef (p), m_presetGUI(p.m_presets), m_editor(p,*p.m_parameterVTS)
#endif
{
    setResizable(true,true);
    applyWindowSize();

	addAndMakeVisible(m_presetGUI);

    // Day/night theme toggle (shows the icon of the mode a click switches TO, common
    // toggle-icon convention). Top-right corner, independent of the preset bar's own
    // layout -- see resized(). Component ID lets StereoWidenerLookAndFeel::
    // drawButtonBackground() give just this one (small, icon-only) button a visible
    // border, without a dedicated public flag.
    m_themeButton.setComponentID(StereoWidenerLookAndFeel::themeToggleComponentID);
    applyThemeButtonStyle(m_themeButton, m_lookAndFeel.getTheme());
    m_themeButton.onClick = [this] { toggleTheme(); };
    addAndMakeVisible(m_themeButton);
#if WITH_MIDIKEYBOARD
	addAndMakeVisible(m_keyboard);
    addAndMakeVisible(m_wheels);
#endif

    // from here your algo editor ---------
    addAndMakeVisible(m_editor);

    // Last, after every child has been added: setLookAndFeel() notifies only the
    // components that are children at that moment, and a Slider builds its value box
    // from the look-and-feel's colours only when notified. Set earlier, the value boxes
    // kept JUCE's default dark-scheme colours (white text) until the first theme
    // toggle -- unreadable on the Day theme's light background.
    setLookAndFeel(&m_lookAndFeel);
}

StereoWidenerAudioProcessorEditor::~StereoWidenerAudioProcessorEditor()
{
    // Cleared before any member is torn down (see m_lookAndFeel's own declaration-order
    // comment in the header) -- belt and braces against Component's own destructor
    // still consulting getLookAndFeel() while child components are being destroyed.
    setLookAndFeel(nullptr);
}

void StereoWidenerAudioProcessorEditor::toggleTheme()
{
    const bool nowDay = m_lookAndFeel.getTheme() != StereoWidenerLookAndFeel::Theme::Day;
    const auto newTheme = nowDay ? StereoWidenerLookAndFeel::Theme::Day : StereoWidenerLookAndFeel::Theme::Night;
    m_lookAndFeel.setTheme(newTheme);
    m_processorRef.m_algo.getGlobalSettings().saveUseDayTheme(nowDay);
    applyThemeButtonStyle(m_themeButton, newTheme);
    sendLookAndFeelChange(); // repaints this + every child that doesn't have its own explicit LookAndFeel
}

void StereoWidenerAudioProcessorEditor::applyWindowSize()
{
    // Width-based scale factor (same one resized() computes and persists via
    // m_processorRef.setScaleFactor()), restored from the previous session.
    const float scaleFactor = m_processorRef.getScaleFactor();
    const int contentHeight = m_editor.getRequiredContentHeight(); // unscaled, scale = 1.0
    const int presetBarHeight = g_minPresetHandlerHeight + 1; // matches resized()'s own "+1" gap
    const float ratio = (float) (contentHeight + presetBarHeight) / (float) g_minGuiSize_x; // height/width at scale 1.0

    setResizeLimits(g_minGuiSize_x, static_cast<int>(g_minGuiSize_x * ratio), g_maxGuiSize_x, static_cast<int>(g_maxGuiSize_x * ratio));
    getConstrainer()->setFixedAspectRatio(1.0 / (double) ratio);
    setSize(static_cast<int>(scaleFactor * g_minGuiSize_x), static_cast<int>(scaleFactor * g_minGuiSize_x * ratio));
}

//==============================================================================
void StereoWidenerAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}
void StereoWidenerAudioProcessorEditor::resized()
{
    int height = getHeight();
    int width = getWidth();
    float scaleFactor = float(width)/g_minGuiSize_x;
    m_processorRef.setScaleFactor(scaleFactor);

    // Everything below scales with the width-based scaleFactor (the aspect ratio is
    // fixed, see applyWindowSize()).
    // Theme toggle: fixed top-right corner, independent of whether the preset bar
    // exists (WITH_PRESETHANDLERGUI) -- so it also has a sensible position in a build
    // without it.
    const int themeButtonSize = juce::roundToInt(scaleFactor * g_themeButtonSize);
    const int themeButtonGap = juce::roundToInt(scaleFactor * g_themeButtonGap);
    m_themeButton.setBounds(width - themeButtonGap - themeButtonSize, themeButtonGap, themeButtonSize, themeButtonSize);

#if WITH_PRESETHANDLERGUI
    const int presetBarHeight = juce::roundToInt(scaleFactor * g_minPresetHandlerHeight);
    // Shrunk on the right to leave room for the theme button above, so the preset
    // bar's own centred Prev/Combo/Next/Save cluster never overlaps it.
    m_presetGUI.setBounds(0, 0, width - themeButtonSize - 2 * themeButtonGap, presetBarHeight);
#endif
#if WITH_MIDIKEYBOARD
    m_wheels.setBounds(0, static_cast<int> (height*(1-g_midikeyboardratio)),  static_cast<int> (g_wheelstokeyboardratio*width),  static_cast<int> (height*g_midikeyboardratio));
    m_keyboard.setBounds(static_cast<int> (g_wheelstokeyboardratio*width), static_cast<int> (height*(1-g_midikeyboardratio)), static_cast<int> ((1.0-g_wheelstokeyboardratio)*width),static_cast<int> ( height*g_midikeyboardratio));
#endif
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
#if WITH_MIDIKEYBOARD
    #if WITH_PRESETHANDLERGUI
    m_editor.setBounds(0, presetBarHeight + 1,
                        width, static_cast<int> (height*(1-g_midikeyboardratio)) - (presetBarHeight + 1));
    #else
    m_editor.setBounds(0, 0, width, static_cast<int> (height*(1-g_midikeyboardratio) ));

    #endif
#else
    #if WITH_PRESETHANDLERGUI
    m_editor.setBounds(0, presetBarHeight + 1, width, height - (presetBarHeight + 1));
    #else
    m_editor.setBounds(0, 0, width, height);

    #endif
#endif

}

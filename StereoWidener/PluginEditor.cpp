
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginSettings.h"

//==============================================================================
#if WITH_MIDIKEYBOARD
StereoWidenerAudioProcessorEditor::StereoWidenerAudioProcessorEditor (StereoWidenerAudioProcessor& p)
    : AudioProcessorEditor (&p), m_processorRef (p), m_presetGUI(p.m_presets),
    	m_keyboard(m_processorRef.m_keyboardState, MidiKeyboardComponent::Orientation::horizontalKeyboard),
        m_wheels(p.m_wheelState), m_editor(p,*p.m_parameterVTS)
#else
StereoWidenerAudioProcessorEditor::StereoWidenerAudioProcessorEditor (StereoWidenerAudioProcessor& p)
    : AudioProcessorEditor (&p), m_processorRef (p), m_presetGUI(p.m_presets), m_editor(p,*p.m_parameterVTS)
#endif
{
    float scaleFactor = m_processorRef.getScaleFactor();
    setResizeLimits (g_minGuiSize_x,static_cast<int>(g_minGuiSize_x*g_guiratio) , g_maxGuiSize_x, static_cast<int>(g_maxGuiSize_x*g_guiratio));
    setResizable(true,true);
    getConstrainer()->setFixedAspectRatio(1./g_guiratio);
    setSize (static_cast<int>(scaleFactor*g_minGuiSize_x), static_cast<int>(scaleFactor*g_minGuiSize_x*g_guiratio));

	addAndMakeVisible(m_presetGUI);
#if WITH_MIDIKEYBOARD
	addAndMakeVisible(m_keyboard);
    addAndMakeVisible(m_wheels);
#endif

    // from here your algo editor ---------
    addAndMakeVisible(m_editor);

    // Resizes the window to fit whichever algorithm ends up selected (e.g. a restored
    // DAW project's own saved choice) -- see updateWindowSizeForActiveAlgorithm(). The
    // manual call right after wiring the callback is needed because m_editor's own
    // constructor already ran its initial updateAuxKnobsForActiveAlgorithm() (and so
    // already tried to fire onActiveAlgorithmChanged) before this callback existed to
    // catch it.
    m_editor.onActiveAlgorithmChanged = [this] { updateWindowSizeForActiveAlgorithm(); };
    updateWindowSizeForActiveAlgorithm();
}

StereoWidenerAudioProcessorEditor::~StereoWidenerAudioProcessorEditor()
{
}

void StereoWidenerAudioProcessorEditor::updateWindowSizeForActiveAlgorithm()
{
    // Width-based scale factor (same one resized() computes and persists via
    // m_processorRef.setScaleFactor()) is preserved across this resize -- only the
    // height/aspect ratio changes, not how "zoomed in" the plugin currently is.
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

    // scaleFactor-based (not height*g_minPresetHandlerHeight/g_minGuiSize_y, a ratio
    // that assumed height always equals scaleFactor*g_minGuiSize_y -- no longer true
    // once the window's own aspect ratio changes per algorithm, see
    // updateWindowSizeForActiveAlgorithm()).
#if WITH_PRESETHANDLERGUI
    const int presetBarHeight = juce::roundToInt(scaleFactor * g_minPresetHandlerHeight);
    m_presetGUI.setBounds(0, 0, width, presetBarHeight);
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

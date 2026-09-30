/**
 * @file GlobalSettings.h
 * @brief User-wide default settings for StereoWidener (plan2.md Phase 4, "Global
 *        settings file"), loaded once at plugin startup from a small JSON file
 *        (settings.json) in the user preset folder (PresetHandler::getUserPresetsFolder()).
 *
 * JUCE's usual class for this job, juce::PropertiesFile, only writes XML or binary.
 * This uses juce::JSON instead (also built into juce_core, so no extra dependency) for
 * a format that is simple to read and hand-edit, per the user's own preference.
 *
 * These are defaults/preferences, not part of a DAW project's saved state: each plugin
 * instance still saves/restores its own actual parameter values via
 * AudioProcessor::getStateInformation() as usual (see PluginProcessor.cpp), and that
 * always wins once it exists -- setStateInformation() runs after construction and
 * unconditionally replaces the whole parameter tree.
 *
 * Parameter defaults themselves are NOT stored here (and never were meant to drift):
 * every g_param*.defaultValue in StereoWidener.h is a fixed, compiled-in value chosen
 * to be as close to neutral/pass-through processing as possible, and that is also the
 * value a GUI knob resets to on double-click (JUCE wires a parameter's own default to
 * its slider's double-click-return-value automatically). An earlier design instead
 * seeded each parameter's default from a "last used state" recorded here, which meant
 * double-click reset to whatever was last dialled in rather than to neutral -- removed
 * for that reason; restoring a previous session's settings is what the init.xml preset
 * (PresetHandler) is for, and it is a better fit for that job since it is an explicit,
 * inspectable, user-chosen action rather than an invisible side effect of closing the
 * plugin.
 *
 * If the file doesn't exist yet (first run), it is created with default values, so
 * there is something for the user to find and edit -- rather than a setting that only
 * works if you already know to create the file yourself.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_core/juce_core.h>

class GlobalSettings
{
public:
    /** Loads from the user's application-data folder, creating the file with default
     *  values if it doesn't exist yet. Safe to construct more than once (e.g. one per
     *  plugin instance): each instance re-reads the file at construction, so an
     *  external edit is picked up by any instance created after that edit (existing
     *  instances are not watched/reloaded live -- see the file header). */
    GlobalSettings();

    /** Default GUI scale factor for a brand new instance (StereoWidenerAudioProcessor's
     *  m_pluginScaleFactor before any project state is restored). Default 1.0. */
    float getGuiScaleFactor() const noexcept { return guiScaleFactor; }

    /** Day/night GUI theme (see PluginLookAndFeel.h) for a brand new instance. Default
     *  false (Night) -- the plugin's existing look, unchanged for anyone who never
     *  touches the toggle. A pure UI convenience like guiScaleFactor, not a processing
     *  default, so it is not affected by the "parameter defaults never drift" reasoning
     *  in the file header. */
    bool getUseDayTheme() const noexcept { return useDayTheme; }

    /** Default meter ballistics (StereoMeterState::prepare()'s own parameters) for a
     *  brand new instance. StereoWidener has no per-project override for these yet
     *  (unlike StereoAnalyzer's Settings popup), so these are the only source for now. */
    float getMeterIntegrationTimeS() const noexcept { return meterIntegrationTimeS; }
    float getMeterPeakHoldTimeS() const noexcept { return meterPeakHoldTimeS; }
    float getMeterPeakDecayDbPerS() const noexcept { return meterPeakDecayDbPerS; }

    /** Called once, from StereoWidenerAudioProcessor's destructor, with the current GUI
     *  scale factor. Persists it (and the rest of the settings, unchanged) to disk,
     *  becoming the next brand new instance's starting window size. Unlike parameter
     *  values (see the file header), the GUI size is a pure UI convenience, not a
     *  processing default, so it is not affected by the reasoning above. */
    void saveGuiScaleFactor(float newGuiScaleFactor);

    /** Called from the theme-toggle button's click handler (StereoWidenerAudioProcessorEditor),
     *  same immediate-persist pattern as saveGuiScaleFactor() above. */
    void saveUseDayTheme(bool newUseDayTheme);

    /** Where the settings file lives. Exposed mainly for logging/diagnostics. */
    static juce::File getSettingsFile();

private:
    void load();
    void write(const juce::File& file) const;

    float guiScaleFactor = 1.0f;
    float meterIntegrationTimeS = 0.3f;  // matches StereoMeterState::prepare()'s own defaults
    float meterPeakHoldTimeS = 1.5f;
    float meterPeakDecayDbPerS = 20.0f;
    bool useDayTheme = false;
};

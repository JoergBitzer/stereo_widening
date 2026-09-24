/**
 * @file GlobalSettings.h
 * @brief User-wide default settings for StereoWidener (plan2.md Phase 4, "Global
 *        settings file"), loaded once at plugin startup from a small JSON file in the
 *        user's application-data folder.
 *
 * JUCE's usual class for this job, juce::PropertiesFile, only writes XML or binary.
 * This uses juce::JSON instead (also built into juce_core, so no extra dependency) for
 * a format that is simple to read and hand-edit, per the user's own preference.
 *
 * These are defaults/preferences, not part of a DAW project's saved state: each plugin
 * instance still saves/restores its own actual parameter values via
 * AudioProcessor::getStateInformation() as usual (see PluginProcessor.cpp), and that
 * always wins once it exists -- setStateInformation() runs after construction and
 * unconditionally replaces the whole parameter tree (see StereoWidener.cpp's
 * addParameter(), which is the only place lastUsedState's values are actually used).
 * This file only supplies the values a *brand new* instance -- one with no saved
 * project state yet -- starts with.
 *
 * If the file doesn't exist yet (first run), it is created with default values, so
 * there is something for the user to find and edit -- rather than a setting that only
 * works if you already know to create the file yourself.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_core/juce_core.h>
#include <map>

class GlobalSettings
{
public:
    /** Loads from the user's application-data folder, creating the file with default
     *  values if it doesn't exist yet. Safe to construct more than once (e.g. one per
     *  plugin instance): each instance re-reads the file at construction, so an
     *  external edit is picked up by any instance created after that edit (existing
     *  instances are not watched/reloaded live -- see the file header). */
    GlobalSettings();

    /** Gain (dB) of MSWidthFiltered's side-channel high shelf. Default 3.0 dB. */
    float getHighShelfGainDb() const noexcept { return highShelfGainDb; }

    /** Default GUI scale factor for a brand new instance (StereoWidenerAudioProcessor's
     *  m_pluginScaleFactor before any project state is restored). Default 1.0. */
    float getGuiScaleFactor() const noexcept { return guiScaleFactor; }

    /** Default meter ballistics (StereoMeterState::prepare()'s own parameters) for a
     *  brand new instance. StereoWidener has no per-project override for these yet
     *  (unlike StereoAnalyzer's Settings popup), so these are the only source for now. */
    float getMeterIntegrationTimeS() const noexcept { return meterIntegrationTimeS; }
    float getMeterPeakHoldTimeS() const noexcept { return meterPeakHoldTimeS; }
    float getMeterPeakDecayDbPerS() const noexcept { return meterPeakDecayDbPerS; }

    /** The value a parameter with this ID (an AudioProcessorValueTreeState parameter
     *  ID, e.g. g_paramWidth.ID) last had when some previous instance closed, or
     *  fallbackValue if there is no recorded value yet (e.g. the very first run).
     *  StereoWidener.cpp's addParameter() uses this to seed each parameter's
     *  construction-time default, which a DAW project's own saved state (restored
     *  afterwards) always overrides. */
    double getLastUsedParam(const juce::String& paramId, double fallbackValue) const;

    /** Called once, from StereoWidenerAudioProcessor's destructor, with every current
     *  parameter's ID/value and the current GUI scale factor. Persists them (and the
     *  rest of the settings, unchanged) to disk, becoming the next brand new instance's
     *  starting point. */
    void saveLastUsedState(const std::map<juce::String, double>& paramValues, float newGuiScaleFactor);

    /** Where the settings file lives. Exposed mainly for logging/diagnostics. */
    static juce::File getSettingsFile();

private:
    void load();
    void write(const juce::File& file) const;

    float highShelfGainDb = 3.0f; // matches the previous hardcoded MSWidthFiltered::kHighShelfGainDb
    float guiScaleFactor = 1.0f;
    float meterIntegrationTimeS = 0.3f;  // matches StereoMeterState::prepare()'s own defaults
    float meterPeakHoldTimeS = 1.5f;
    float meterPeakDecayDbPerS = 20.0f;

    // parameter ID -> last used value, or an empty/invalid var if none has been saved
    // yet (e.g. the very first run); see getLastUsedParam()/saveLastUsedState()
    juce::var lastUsedState;
};

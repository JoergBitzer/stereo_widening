/**
 * @file GlobalSettings.h
 * @brief User-wide default settings for StereoWidener (plan2.md Phase 4, "Global ini
 *        file"), loaded once at plugin startup from a small JSON file in the user's
 *        application-data folder.
 *
 * JUCE's usual class for this job, juce::PropertiesFile, only writes XML or binary.
 * This uses juce::JSON instead (also built into juce_core, so no extra dependency) for
 * a format that is simple to read and hand-edit, per the user's own preference.
 *
 * These are defaults/preferences, not part of a DAW project's saved state: each plugin
 * instance still saves/restores its own actual parameter values via
 * AudioProcessor::getStateInformation() as usual (see PluginProcessor.cpp). This file
 * only supplies the values a brand-new instance starts with, for things that aren't
 * (yet) exposed as automatable parameters -- currently just the "Filtered" algorithm's
 * side high-shelf gain (previously a hardcoded constant,
 * MSWidthFiltered::kHighShelfGainDb; see algorithms/MSWidthFiltered.h).
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

    /** Gain (dB) of MSWidthFiltered's side-channel high shelf. Default 3.0 dB. */
    float getHighShelfGainDb() const noexcept { return highShelfGainDb; }

    /** Where the settings file lives. Exposed mainly for logging/diagnostics. */
    static juce::File getSettingsFile();

private:
    void load();
    void writeDefaults(const juce::File& file) const;

    float highShelfGainDb = 3.0f; // matches the previous hardcoded MSWidthFiltered::kHighShelfGainDb
};

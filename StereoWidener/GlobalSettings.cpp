#include "GlobalSettings.h"

juce::File GlobalSettings::getSettingsFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("StereoWidener")
        .getChildFile("settings.json");
}

GlobalSettings::GlobalSettings()
{
    load();
}

void GlobalSettings::load()
{
    const juce::File file = getSettingsFile();
    if (!file.existsAsFile())
    {
        writeDefaults(file);
        return;
    }

    const juce::var parsed = juce::JSON::parse(file);
    if (auto* obj = parsed.getDynamicObject())
    {
        if (obj->hasProperty("highShelfGainDb"))
            highShelfGainDb = (float) (double) obj->getProperty("highShelfGainDb");
    }
    // a missing/unparseable/incomplete file just keeps whichever defaults above were
    // not overwritten -- a broken settings file must never stop the plugin from loading
}

void GlobalSettings::writeDefaults(const juce::File& file) const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("highShelfGainDb", (double) highShelfGainDb);
    const juce::var root(obj);

    file.getParentDirectory().createDirectory();
    file.replaceWithText(juce::JSON::toString(root));
}

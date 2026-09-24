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
        write(file); // first run: create with the compiled-in defaults above
        return;
    }

    const juce::var parsed = juce::JSON::parse(file);
    if (auto* obj = parsed.getDynamicObject())
    {
        if (obj->hasProperty("highShelfGainDb"))
            highShelfGainDb = (float) (double) obj->getProperty("highShelfGainDb");
        if (obj->hasProperty("guiScaleFactor"))
            guiScaleFactor = (float) (double) obj->getProperty("guiScaleFactor");
        if (obj->hasProperty("meterIntegrationTimeS"))
            meterIntegrationTimeS = (float) (double) obj->getProperty("meterIntegrationTimeS");
        if (obj->hasProperty("meterPeakHoldTimeS"))
            meterPeakHoldTimeS = (float) (double) obj->getProperty("meterPeakHoldTimeS");
        if (obj->hasProperty("meterPeakDecayDbPerS"))
            meterPeakDecayDbPerS = (float) (double) obj->getProperty("meterPeakDecayDbPerS");
        if (obj->hasProperty("lastUsedState"))
            lastUsedState = obj->getProperty("lastUsedState");
    }
    // a missing/unparseable/incomplete file just keeps whichever defaults above were
    // not overwritten -- a broken settings file must never stop the plugin from loading
}

double GlobalSettings::getLastUsedParam(const juce::String& paramId, double fallbackValue) const
{
    if (auto* obj = lastUsedState.getDynamicObject())
        if (obj->hasProperty(paramId))
            return (double) obj->getProperty(paramId);
    return fallbackValue;
}

void GlobalSettings::saveLastUsedState(const std::map<juce::String, double>& paramValues, float newGuiScaleFactor)
{
    guiScaleFactor = newGuiScaleFactor;

    auto* obj = new juce::DynamicObject();
    for (const auto& [id, value] : paramValues)
        obj->setProperty(id, value);
    lastUsedState = juce::var(obj);

    write(getSettingsFile());
}

void GlobalSettings::write(const juce::File& file) const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("highShelfGainDb", (double) highShelfGainDb);
    obj->setProperty("guiScaleFactor", (double) guiScaleFactor);
    obj->setProperty("meterIntegrationTimeS", (double) meterIntegrationTimeS);
    obj->setProperty("meterPeakHoldTimeS", (double) meterPeakHoldTimeS);
    obj->setProperty("meterPeakDecayDbPerS", (double) meterPeakDecayDbPerS);
    obj->setProperty("lastUsedState", lastUsedState);
    const juce::var root(obj);

    file.getParentDirectory().createDirectory();
    file.replaceWithText(juce::JSON::toString(root));
}

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
        if (obj->hasProperty("combCrossoverHz"))
            combCrossoverHz = (float) (double) obj->getProperty("combCrossoverHz");
        if (obj->hasProperty("earlyReflectionsPreDelayMs"))
            earlyReflectionsPreDelayMs = (float) (double) obj->getProperty("earlyReflectionsPreDelayMs");
    }
    // a missing/unparseable/incomplete file just keeps whichever defaults above were
    // not overwritten -- a broken settings file must never stop the plugin from loading
}

void GlobalSettings::saveGuiScaleFactor(float newGuiScaleFactor)
{
    guiScaleFactor = newGuiScaleFactor;
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
    obj->setProperty("combCrossoverHz", (double) combCrossoverHz);
    obj->setProperty("earlyReflectionsPreDelayMs", (double) earlyReflectionsPreDelayMs);
    const juce::var root(obj);

    file.getParentDirectory().createDirectory();
    file.replaceWithText(juce::JSON::toString(root));
}

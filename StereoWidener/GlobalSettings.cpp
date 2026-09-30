#include "GlobalSettings.h"
#include "tools/PresetHandler.h"

// In the preset folder (determined by the preset handler), so presets and settings are in one
// place, e.g. ~/.config/Jade_Hochschule/StereoWidener/settings.json on Linux. Not ".xml": the
// preset handler loads every .xml file in that folder as a preset.
juce::File GlobalSettings::getSettingsFile()
{
    bool folderWasCreated;
    return PresetHandler::getUserPresetsFolder(folderWasCreated).getChildFile("settings.json");
}

// Until v1.0.4 (first build): <userApplicationDataDirectory>/StereoWidener/settings.json
static juce::File getOldSettingsFile()
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
    const juce::File oldFile = getOldSettingsFile();
    if (!file.existsAsFile() && oldFile.existsAsFile())
    {
        // move the settings of an earlier version to the new place (and remove its empty folder)
        oldFile.moveFileTo(file);
        if (oldFile.getParentDirectory().getNumberOfChildFiles(juce::File::findFilesAndDirectories) == 0)
            oldFile.getParentDirectory().deleteFile();
    }
    if (!file.existsAsFile())
    {
        write(file); // first run: create with the compiled-in defaults above
        return;
    }

    const juce::var parsed = juce::JSON::parse(file);
    if (auto* obj = parsed.getDynamicObject())
    {
        if (obj->hasProperty("guiScaleFactor"))
            guiScaleFactor = (float) (double) obj->getProperty("guiScaleFactor");
        if (obj->hasProperty("meterIntegrationTimeS"))
            meterIntegrationTimeS = (float) (double) obj->getProperty("meterIntegrationTimeS");
        if (obj->hasProperty("meterPeakHoldTimeS"))
            meterPeakHoldTimeS = (float) (double) obj->getProperty("meterPeakHoldTimeS");
        if (obj->hasProperty("meterPeakDecayDbPerS"))
            meterPeakDecayDbPerS = (float) (double) obj->getProperty("meterPeakDecayDbPerS");
        if (obj->hasProperty("useDayTheme"))
            useDayTheme = (bool) obj->getProperty("useDayTheme");
    }
    // a missing/unparseable/incomplete file just keeps whichever defaults above were
    // not overwritten -- a broken settings file must never stop the plugin from loading
}

void GlobalSettings::saveGuiScaleFactor(float newGuiScaleFactor)
{
    guiScaleFactor = newGuiScaleFactor;
    write(getSettingsFile());
}

void GlobalSettings::saveUseDayTheme(bool newUseDayTheme)
{
    useDayTheme = newUseDayTheme;
    write(getSettingsFile());
}

void GlobalSettings::write(const juce::File& file) const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("guiScaleFactor", (double) guiScaleFactor);
    obj->setProperty("meterIntegrationTimeS", (double) meterIntegrationTimeS);
    obj->setProperty("meterPeakHoldTimeS", (double) meterPeakHoldTimeS);
    obj->setProperty("meterPeakDecayDbPerS", (double) meterPeakDecayDbPerS);
    obj->setProperty("useDayTheme", useDayTheme);
    const juce::var root(obj);

    file.getParentDirectory().createDirectory();
    file.replaceWithText(juce::JSON::toString(root));
}

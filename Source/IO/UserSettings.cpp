#include "UserSettings.h"
#include "AppPaths.h"

namespace jgl
{

juce::PropertiesFile& UserSettings::get()
{
    static std::unique_ptr<juce::PropertiesFile> file;
    if (file == nullptr)
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "settings";
        o.filenameSuffix = ".xml";
        o.storageFormat = juce::PropertiesFile::storeAsXML;
        o.millisecondsBeforeSaving = 500;
        file = std::make_unique<juce::PropertiesFile> (AppPaths::settingsFolder().getChildFile ("settings.xml"), o);
    }
    return *file;
}

bool UserSettings::isFavourite (const juce::String& key)
{
    return get().getBoolValue ("fav_" + key, false);
}

void UserSettings::setFavourite (const juce::String& key, bool favourite)
{
    get().setValue ("fav_" + key, favourite);
    get().saveIfNeeded();
}

} // namespace jgl

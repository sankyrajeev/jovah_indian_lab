#include "AppPaths.h"

namespace jgl
{

namespace
{
    juce::File& overriddenRoot()
    {
        static juce::File f;
        return f;
    }

    juce::File ensure (const juce::File& f)
    {
        if (! f.isDirectory())
            f.createDirectory();
        return f;
    }
}

void AppPaths::overrideRoot (const juce::File& newRoot) { overriddenRoot() = newRoot; }

juce::File AppPaths::root()
{
    if (overriddenRoot() != juce::File())
        return ensure (overriddenRoot());

    auto music = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    if (! music.isDirectory())
        music = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Music");
    return ensure (music.getChildFile ("Jovah Groove Lab"));
}

juce::File AppPaths::exportsFolder() { return ensure (root().getChildFile ("Exports")); }
juce::File AppPaths::samplesFolder() { return ensure (root().getChildFile ("Samples")); }
juce::File AppPaths::presetsFolder() { return ensure (root().getChildFile ("Presets")); }

juce::File AppPaths::settingsFolder()
{
    if (overriddenRoot() != juce::File())
        return ensure (overriddenRoot().getChildFile ("Settings"));

   #if JUCE_MAC
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support");
   #else
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #endif
    return ensure (base.getChildFile ("Jovah Groove Lab"));
}

juce::File AppPaths::logFile() { return settingsFolder().getChildFile ("JovahGrooveLab.log"); }

} // namespace jgl

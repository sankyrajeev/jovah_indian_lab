#include "PresetManager.h"

#include "../Core/ParamIDs.h"
#include "../Core/Styles.h"
#include "AppPaths.h"

namespace jgl
{

const juce::Identifier PresetManager::presetType ("JGLPreset");

namespace
{
    const juce::Identifier paramNode ("P");
    const juce::Identifier idAttr ("id");
    const juce::Identifier valueAttr ("v");

    juce::ValueTree makeFactory (const char* name, StyleId style, FrameworkId fw, double bpm, int key, int scale,
                                 int bars, std::initializer_list<InstrumentId> ensemble, uint64_t seed)
    {
        juce::ValueTree p (PresetManager::presetType);
        p.setProperty ("name", name, nullptr);
        p.setProperty ("factory", true, nullptr);
        p.setProperty ("seeds", SeedSet::fromMaster (seed).toString(), nullptr);

        const auto& s = styleProfile (style);
        using PM = PresetManager;
        PM::setValue (p, ParamIDs::style, (float) style);
        PM::setValue (p, ParamIDs::framework, (float) fw);
        PM::setValue (p, ParamIDs::bpm, (float) bpm);
        PM::setValue (p, ParamIDs::key, (float) key);
        PM::setValue (p, ParamIDs::scale, (float) scale);
        PM::setValue (p, ParamIDs::bars, (float) ParamIDs::indexForBars (bars));
        PM::setValue (p, ParamIDs::complexity, s.complexity);
        PM::setValue (p, ParamIDs::swing, s.swing);
        PM::setValue (p, ParamIDs::groove, s.groove);
        PM::setValue (p, ParamIDs::humanize, s.humanize);
        PM::setValue (p, ParamIDs::density, s.density);
        PM::setValue (p, ParamIDs::energy, s.energy);
        PM::setValue (p, ParamIDs::variation, s.variation);
        PM::setValue (p, ParamIDs::ghost, s.ghost);
        PM::setValue (p, ParamIDs::accent, s.accent);
        PM::setValue (p, ParamIDs::reverb, s.reverb);
        PM::setValue (p, ParamIDs::delay, s.delay);
        PM::setValue (p, ParamIDs::saturation, s.saturation);
        PM::setValue (p, ParamIDs::delayTime, 1.0f);
        PM::setValue (p, ParamIDs::filter, 0.0f);
        PM::setValue (p, ParamIDs::level, 0.0f);

        for (auto id : allInstruments())
        {
            bool on = false;
            for (auto e : ensemble) on = on || e == id;
            PM::setValue (p, ParamIDs::instrument (id), on ? 1.0f : 0.0f);
        }
        return p;
    }

    std::vector<juce::ValueTree> buildFactory()
    {
        using I = InstrumentId;
        using S = StyleId;
        using F = FrameworkId;
        // Scale indices: 0 Major, 1 Natural Minor, 2 Harmonic Minor, 3 Dorian, 4 Phrygian, 5 Phrygian Dominant,
        // 6 Mixolydian, 7 Major Pent, 8 Minor Pent, 9 Bhairav, 10 Bhairavi, 11 Yaman, 12 Kafi, 13 Bhupali,
        // 14 Malkauns, 15 Desh, 16 Todi, 17 Charukeshi
        return {
            makeFactory ("Afro Indian Starter",  S::AfroIndian,         F::StyleDefault, 124, 2, 1,  4, { I::Dhol, I::Tabla, I::Tumbi, I::Ghungroo }, 0x1A2B3C01),
            makeFactory ("Tribal Fire",          S::TribalRitual,       F::StyleDefault, 126, 4, 4,  4, { I::Dhol, I::Tabla, I::Manjira, I::Taali, I::Tanpura }, 0x1A2B3C02),
            makeFactory ("Rupak Underground",    S::IndianUnderground,  F::StyleDefault, 128, 5, 5,  4, { I::Tabla, I::Dholak, I::Tumbi, I::Manjira }, 0x1A2B3C03),
            makeFactory ("Bhairav Dreams",       S::PsychedelicIndia,   F::StyleDefault, 120, 1, 9,  8, { I::Sitar, I::Tabla, I::Tanpura, I::Ghungroo }, 0x1A2B3C04),
            makeFactory ("Yaman Evening",        S::MelodicIndian,      F::Keherwa,      118, 2, 11, 4, { I::Bansuri, I::Sitar, I::Tabla, I::Tanpura }, 0x1A2B3C05),
            makeFactory ("Peak Dhol Machine",    S::PeakTimePercussion, F::StyleDefault, 128, 9, 2,  4, { I::Dhol, I::Tabla, I::Dholak, I::Manjira, I::Taali }, 0x1A2B3C06),
            makeFactory ("Minimal Malkauns",     S::MinimalHypnosis,    F::StyleDefault, 122, 7, 14, 4, { I::Tabla, I::Ghungroo, I::Tumbi }, 0x1A2B3C07),
            makeFactory ("Desh Folk Party",      S::FolkElectronica,    F::StyleDefault, 120, 7, 15, 4, { I::Dhol, I::Dholak, I::Tumbi, I::Taali }, 0x1A2B3C08),
            makeFactory ("Dadra Sunrise",        S::MelodicIndian,      F::Dadra,        112, 4, 12, 4, { I::Bansuri, I::Tabla, I::Ghungroo, I::Tanpura }, 0x1A2B3C09),
            makeFactory ("Teentaal Rush",        S::PeakTimePercussion, F::Teentaal,     130, 0, 1,  2, { I::Tabla, I::Manjira, I::Dhol }, 0x1A2B3C0A),
            makeFactory ("Charukeshi Afro",      S::AfroIndian,         F::StyleDefault, 123, 11, 17, 4, { I::Dhol, I::Tabla, I::Sitar, I::Ghungroo }, 0x1A2B3C0B),
            makeFactory ("Tabla Solo Keherwa",   S::Custom,             F::Keherwa,      100, 1, 1,  4, { I::Tabla }, 0x1A2B3C0C),
            makeFactory ("Todi Ritual Drone",    S::TribalRitual,       F::Rupak,        118, 3, 16, 8, { I::Dhol, I::Tabla, I::Tanpura, I::Bansuri, I::Manjira }, 0x1A2B3C0D),
            makeFactory ("Bhangra Club Tool",    S::FolkElectronica,    F::Club,         126, 9, 6,  2, { I::Dhol, I::Taali, I::Manjira }, 0x1A2B3C0E),
        };
    }
}

const std::vector<juce::ValueTree>& PresetManager::factoryPresets()
{
    static const std::vector<juce::ValueTree> f = buildFactory();
    return f;
}

void PresetManager::setValue (juce::ValueTree& preset, const juce::String& paramId, float value)
{
    auto node = preset.getChildWithProperty (idAttr, paramId);
    if (! node.isValid())
    {
        node = juce::ValueTree (paramNode);
        node.setProperty (idAttr, paramId, nullptr);
        preset.appendChild (node, nullptr);
    }
    node.setProperty (valueAttr, value, nullptr);
}

bool PresetManager::getValue (const juce::ValueTree& preset, const juce::String& paramId, float& value)
{
    auto node = preset.getChildWithProperty (idAttr, paramId);
    if (! node.isValid() || ! node.hasProperty (valueAttr))
        return false;
    value = (float) node.getProperty (valueAttr);
    return true;
}

juce::File PresetManager::fileFor (const juce::String& name)
{
    return AppPaths::presetsFolder().getChildFile (juce::File::createLegalFileName (name) + ".jglpreset");
}

juce::StringArray PresetManager::userPresetNames()
{
    juce::StringArray names;
    for (auto& f : AppPaths::presetsFolder().findChildFiles (juce::File::findFiles, false, "*.jglpreset"))
    {
        if (auto xml = juce::XmlDocument::parse (f))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            names.add (tree.getProperty ("name", f.getFileNameWithoutExtension()).toString());
        }
    }
    names.sortNatural();
    return names;
}

juce::Result PresetManager::saveUserPreset (const juce::String& name, const juce::ValueTree& preset)
{
    if (name.trim().isEmpty())
        return juce::Result::fail ("Preset name is empty");

    auto copy = preset.createCopy();
    copy.setProperty ("name", name.trim(), nullptr);
    copy.setProperty ("factory", false, nullptr);

    auto xml = copy.createXml();
    if (xml == nullptr || ! xml->writeTo (fileFor (name.trim())))
        return juce::Result::fail ("Could not write preset file");
    return juce::Result::ok();
}

juce::ValueTree PresetManager::loadUserPreset (const juce::String& name)
{
    if (auto xml = juce::XmlDocument::parse (fileFor (name)))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.hasType (presetType))
            return tree;
    }
    return {};
}

bool PresetManager::deleteUserPreset (const juce::String& name)
{
    return fileFor (name).deleteFile();
}

} // namespace jgl

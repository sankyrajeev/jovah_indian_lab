#include "Parameters.h"

#include "../Core/Scales.h"
#include "../Core/Settings.h"
#include "../Core/Styles.h"

namespace jgl
{

namespace
{
    juce::String percentText (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }

    std::unique_ptr<juce::AudioParameterFloat> unitParam (const char* id, const char* name, float def)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, 1.0f), def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace ParamIDs;
    const auto& afro = styleProfile (StyleId::AfroIndian);
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::bpm, 1 }, "BPM", juce::NormalisableRange<float> (70.0f, 160.0f, 0.1f), 124.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); })
            .withLabel ("BPM")));

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::key, 1 }, "Key", keyNames(), 2));

    juce::StringArray scaleNames;
    for (auto& s : scaleProfiles())
        scaleNames.add (s.name);
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::scale, 1 }, "Scale", scaleNames, 1));

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::bars, 1 }, "Loop Length",
                                                              juce::StringArray { "1 bar", "2 bars", "4 bars", "8 bars" }, 2));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::level, 1 }, "Output Level", juce::NormalisableRange<float> (-24.0f, 0.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; })
            .withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::style, 1 }, "Style", styleNames(), 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::framework, 1 }, "Rhythm Framework", frameworkNames(), 0));

    layout.add (unitParam (complexity, "Complexity", afro.complexity));
    layout.add (unitParam (swing, "Swing", afro.swing));
    layout.add (unitParam (groove, "Groove", afro.groove));
    layout.add (unitParam (humanize, "Humanize", afro.humanize));
    layout.add (unitParam (density, "Density", afro.density));
    layout.add (unitParam (energy, "Energy", afro.energy));
    layout.add (unitParam (variation, "Variation", afro.variation));
    layout.add (unitParam (ghost, "Ghost Notes", afro.ghost));
    layout.add (unitParam (accent, "Accent", afro.accent));

    layout.add (unitParam (saturation, "Saturation", afro.saturation));
    layout.add (unitParam (reverb, "Reverb", afro.reverb));
    layout.add (unitParam (delay, "Delay", afro.delay));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { delayTime, 1 }, "Delay Time", delayTimeNames(), 1));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { filter, 1 }, "Filter", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            if (std::abs (v) <= 0.02f) return juce::String ("Off");
            return (v < 0 ? juce::String ("LP ") : juce::String ("HP ")) + juce::String (juce::roundToInt (std::abs (v) * 100.0f)) + "%";
        })));

    layout.add (unitParam (voiceBend, "Voice Bend", 0.5f));
    layout.add (unitParam (voiceShake, "Voice Shake", 0.35f));
    layout.add (unitParam (voiceFalls, "Voice Falls", 0.3f));
    layout.add (unitParam (voiceStutter, "Voice Stutter", 0.15f));
    layout.add (unitParam (voiceOctave, "Voice Octave", 0.2f));
    layout.add (unitParam (voiceVibrato, "Voice Vibrato", 0.3f));

    for (auto id : allInstruments())
    {
        bool on = false;
        for (auto e : afro.suggestedEnsemble) on = on || e == id;
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ParamIDs::instrument (id), 1 },
                                                                juce::String (instrumentInfo (id).name), on));
    }

    return layout;
}

const juce::StringArray& tempoPresets()
{
    static const juce::StringArray t { "90", "100", "110", "115", "118", "120", "122", "124", "126", "128", "130", "135", "140", "150" };
    return t;
}

const std::vector<double>& exportSampleRates()
{
    static const std::vector<double> r { 0.0, 44100.0, 48000.0, 88200.0, 96000.0 };
    return r;
}

juce::String exportSampleRateName (double rate)
{
    if (rate <= 0.0) return "Session rate";
    return juce::String (rate / 1000.0, rate == 44100.0 || rate == 88200.0 ? 1 : 0) + " kHz";
}

} // namespace jgl

#include "VoicePanel.h"

#include "../Core/ParamIDs.h"
#include "../Plugin/PluginProcessor.h"

namespace jgl
{

VoicePanel::VoicePanel (GrooveLabProcessor& p, InstrumentId id, std::function<void()> onLoad)
    : processor (p), instrument (id), isVoice (id == InstrumentId::CustomVoice)
{
    if (isVoice)
    {
        addKnob (expressionKnobs, ParamIDs::voiceBend,    "Bend",    "Meend glides between notes and scoops into notes");
        addKnob (expressionKnobs, ParamIDs::voiceShake,   "Shake",   "Gamaka: the note swings to its neighbour and back");
        addKnob (expressionKnobs, ParamIDs::voiceFalls,   "Falls",   "Pitch dives at the ends of phrases");
        addKnob (expressionKnobs, ParamIDs::voiceStutter, "Stutter", "Rhythmic re-triggers before a note settles");
        addKnob (expressionKnobs, ParamIDs::voiceOctave,  "Octave",  "Octave leaps on phrase peaks");
        addKnob (expressionKnobs, ParamIDs::voiceVibrato, "Vibrato", "Vibrato depth on held notes");

        loadButton.onClick = [onLoad] { if (onLoad) onLoad(); };
        addAndMakeVisible (loadButton);
    }

    addKnob (envelopeKnobs, ParamIDs::sampleAttack,  "Attack",  "Fade-in of every note. Raise it to soften a sharp, tongued or plucked start");
    addKnob (envelopeKnobs, ParamIDs::sampleDecay,   "Decay",   "Time to settle from full level down to the sustain level");
    addKnob (envelopeKnobs, ParamIDs::sampleSustain, "Sustain", "Level of a held note after the decay");
    addKnob (envelopeKnobs, ParamIDs::sampleRelease, "Release", "Fade-out after each note ends; longer overlaps notes smoothly");

    setSize (460, isVoice ? 480 : 210);
}

Knob& VoicePanel::addKnob (juce::OwnedArray<Knob>& into, const char* paramId, const char* caption, const char* tip)
{
    auto* k = into.add (new Knob (caption));
    k->slider.setTooltip (tip);
    addAndMakeVisible (k);
    attachments.add (new juce::AudioProcessorValueTreeState::SliderAttachment (processor.getState(), paramId, k->slider));
    return *k;
}

void VoicePanel::paint (juce::Graphics& g)
{
    g.fillAll (Colours::panel);
    auto r = getLocalBounds().reduced (18, 14);
    g.setColour (Colours::inkSoft);
    g.setFont (Fonts::caps (12.0f, 0.22f));
    g.drawText (isVoice ? juce::String ("YOUR SAMPLE") : juce::String (instrumentInfo (instrument).name).toUpperCase() + " SAMPLES",
                r.removeFromTop (18), juce::Justification::centredLeft);

    if (isVoice)
    {
        auto voice = processor.getUserVoice();
        g.setColour (Colours::ink);
        g.setFont (Fonts::serif (21.0f, true));
        g.drawText (voice != nullptr ? voice->name : juce::String ("Nothing loaded yet"), r.removeFromTop (28), juce::Justification::centredLeft, true);
        g.setColour (Colours::inkSoft);
        g.setFont (Fonts::serif (16.0f));
        g.drawText (voice != nullptr ? "Detected " + processor.describeUserVoice() + ". Played in your key and scale."
                                     : juce::String ("Load or drag in one sustained note (WAV, AIFF, FLAC)."),
                    r.removeFromTop (22), juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour (Colours::inkSoft);
        g.setFont (Fonts::serif (16.0f));
        g.drawText ("Shapes every recorded note. Shared with Your Sample.", r.removeFromTop (24), juce::Justification::centredLeft, true);
    }

    if (! envelopeCaption.isEmpty())
    {
        g.setColour (Colours::line);
        g.drawHorizontalLine (envelopeCaption.getY(), (float) envelopeCaption.getX(), (float) envelopeCaption.getRight());
        g.setColour (Colours::inkSoft);
        g.setFont (Fonts::caps (11.0f, 0.22f));
        g.drawText ("ENVELOPE", envelopeCaption.withTrimmedTop (4), juce::Justification::centredLeft);
    }
}

void VoicePanel::resized()
{
    auto r = getLocalBounds().reduced (18, 14);
    r.removeFromTop (isVoice ? 18 + 28 + 22 + 10 : 18 + 24 + 6);

    if (isVoice)
    {
        loadButton.setBounds (r.removeFromBottom (36).withSizeKeepingCentre (190, 36));
        r.removeFromBottom (8);
    }

    auto envelopeRow = r.removeFromBottom (isVoice ? 100 : r.getHeight());
    envelopeCaption = isVoice ? r.removeFromBottom (22) : juce::Rectangle<int>();   // else the title says it

    {
        const int w = envelopeRow.getWidth() / juce::jmax (1, envelopeKnobs.size());
        for (auto* k : envelopeKnobs)
            k->setBounds (envelopeRow.removeFromLeft (w).reduced (isVoice ? 14 : 6, 0));
    }

    if (isVoice)
    {
        auto row1 = r.removeFromTop (r.getHeight() / 2);
        const int w = r.getWidth() / 3;
        for (int i = 0; i < expressionKnobs.size(); ++i)
        {
            auto& row = i < 3 ? row1 : r;
            expressionKnobs[i]->setBounds (row.removeFromLeft (w).reduced (8, 0));
        }
    }
}

} // namespace jgl

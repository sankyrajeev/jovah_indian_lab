#include "VoicePanel.h"

#include "../Core/ParamIDs.h"
#include "../Plugin/PluginProcessor.h"

namespace jgl
{

VoicePanel::VoicePanel (GrooveLabProcessor& p, std::function<void()> onLoad) : processor (p)
{
    struct Spec { const char* id; const char* caption; const char* tip; };
    const Spec specs[] = {
        { ParamIDs::voiceBend,    "Bend",    "Meend glides between notes and scoops into notes" },
        { ParamIDs::voiceShake,   "Shake",   "Gamaka: the note swings to its neighbour and back" },
        { ParamIDs::voiceFalls,   "Falls",   "Pitch dives at the ends of phrases" },
        { ParamIDs::voiceStutter, "Stutter", "Rhythmic re-triggers before a note settles" },
        { ParamIDs::voiceOctave,  "Octave",  "Octave leaps on phrase peaks" },
        { ParamIDs::voiceVibrato, "Vibrato", "Vibrato depth on held notes" },
    };

    for (auto& s : specs)
    {
        auto* k = knobs.add (new Knob (s.caption));
        k->slider.setTooltip (s.tip);
        addAndMakeVisible (k);
        attachments.add (new juce::AudioProcessorValueTreeState::SliderAttachment (processor.getState(), s.id, k->slider));
    }

    loadButton.onClick = [onLoad] { if (onLoad) onLoad(); };
    addAndMakeVisible (loadButton);
    setSize (420, 330);
}

void VoicePanel::paint (juce::Graphics& g)
{
    g.fillAll (Colours::panel);
    auto r = getLocalBounds().reduced (18, 14);
    g.setColour (Colours::inkSoft);
    g.setFont (Fonts::caps (12.0f, 0.22f));
    g.drawText ("YOUR SAMPLE", r.removeFromTop (18), juce::Justification::centredLeft);

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

void VoicePanel::resized()
{
    auto r = getLocalBounds().reduced (18, 14);
    r.removeFromTop (18 + 28 + 22 + 10);
    loadButton.setBounds (r.removeFromBottom (36).withSizeKeepingCentre (190, 36));
    r.removeFromBottom (8);
    auto row1 = r.removeFromTop (r.getHeight() / 2);
    const int w = r.getWidth() / 3;
    for (int i = 0; i < knobs.size(); ++i)
    {
        auto& row = i < 3 ? row1 : r;
        knobs[i]->setBounds (row.removeFromLeft (w).reduced (8, 0));
    }
}

} // namespace jgl

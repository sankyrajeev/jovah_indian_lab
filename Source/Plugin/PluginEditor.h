#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../UI/InstrumentTile.h"
#include "../UI/Theme.h"
#include "../UI/WaveformView.h"
#include "PluginProcessor.h"

namespace jgl
{

class GrooveLabEditor : public juce::AudioProcessorEditor,
                        public juce::DragAndDropContainer,
                        private juce::ChangeListener,
                        private juce::Timer
{
public:
    explicit GrooveLabEditor (GrooveLabProcessor&);
    ~GrooveLabEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int defaultWidth = 1200;
    static constexpr int defaultHeight = 790;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void setupTopBar();
    void setupGroove();
    void setupActions();
    void setupResults();
    void refreshResult();
    void refreshPresetList();
    void setStatus (const juce::String& s);
    void savePresetDialog();
    void showUtilityMenu();
    Knob& addKnob (juce::OwnedArray<Knob>& into, const char* paramId, const juce::String& caption, const juce::String& tooltip);

    GrooveLabProcessor& processor;
    GrooveLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // Top bar
    juce::Slider bpmSlider;
    juce::ComboBox tempoPresetBox, keyBox, scaleBox, presetBox;
    juce::ToggleButton hostTempoToggle { "Host tempo" };
    std::array<juce::TextButton, 4> barButtons;
    juce::Slider levelSlider;
    juce::TextButton savePresetButton { "Save" };
    std::unique_ptr<SliderAttachment> bpmAttachment, levelAttachment;
    std::unique_ptr<ComboAttachment> keyAttachment, scaleAttachment;
    std::unique_ptr<juce::ParameterAttachment> barsAttachment, styleAttachment;

    // Ensemble
    juce::TextButton allButton { "ALL" }, noneButton { "NONE" };
    juce::OwnedArray<InstrumentTile> tiles;

    // Groove / sound
    juce::ComboBox styleBox, frameworkBox, delayTimeBox;
    std::unique_ptr<ComboAttachment> frameworkAttachment, delayTimeAttachment;
    juce::OwnedArray<Knob> grooveKnobs, soundKnobs;
    juce::OwnedArray<SliderAttachment> knobAttachments;

    // Actions
    juce::TextButton generateButton { "GENERATE" };
    juce::TextButton newRhythmButton { "New rhythm" }, newMelodyButton { "New melody" };
    juce::TextButton backButton { "<" }, forwardButton { ">" };
    juce::TextButton copyCodeButton { "Copy loop code" }, pasteCodeButton { "Paste code" };

    // Results
    WaveformView waveform { processor };
    DragHandle dragHandle { processor };
    juce::TextButton playButton { "PLAY" };
    juce::ToggleButton syncToggle { "Sync to host transport" };
    juce::TextButton exportButton { "EXPORT WAV" };
    juce::ComboBox depthBox, rateBox;
    juce::TextButton folderButton { "Open folder" }, utilityButton { "..." };
    juce::Label statusLabel;

    juce::String loopTitle, loopDetails, partSummary;
    bool updatingStyle = false;
    int tick = 0;
    juce::Rectangle<int> topBar, ensembleArea, grooveArea, actionArea, resultArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrooveLabEditor)
};

} // namespace jgl

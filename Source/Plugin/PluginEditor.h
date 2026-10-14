#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../UI/InstrumentTile.h"
#include "../UI/Theme.h"
#include "../UI/WaveformView.h"
#include "../UI/Widgets.h"
#include "PluginProcessor.h"

namespace jgl
{

class GrooveLabEditor : public juce::AudioProcessorEditor,
                        public juce::DragAndDropContainer,
                        public juce::FileDragAndDropTarget,
                        private juce::ChangeListener,
                        private juce::Timer
{
public:
    explicit GrooveLabEditor (GrooveLabProcessor&);
    ~GrooveLabEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { fileDragActive = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { fileDragActive = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

    static constexpr int defaultWidth = 1280;
    static constexpr int defaultHeight = 880;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void setupTopBar();
    void setupEnsemble();
    void setupGroove();
    void setupCreate();
    void setupResults();

    void refreshResult();
    void refreshPresetList();
    void layoutRows();
    void setStatus (const juce::String& s);
    void savePresetDialog();
    void showMainMenu();
    void showStyleMenu();
    void addUtilityItems (juce::PopupMenu& m);
    void chooseVoiceSample();
    void showVoicePanel (InstrumentId id = InstrumentId::CustomVoice);
    void loadVoice (const juce::File& f);
    void handleUtilityResult (int r);
    Knob& addKnob (juce::OwnedArray<Knob>& into, const char* paramId, const juce::String& caption, const juce::String& tooltip);

    GrooveLabProcessor& processor;
    GrooveLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // Top bar
    juce::Slider bpmSlider;
    IconButton bpmUp { decor::Icon::ChevronUp, decor::Icon::ChevronUp, "Tempo +1" };
    IconButton bpmDown { decor::Icon::ChevronDown, decor::Icon::ChevronDown, "Tempo -1" };
    juce::ComboBox keyBox, scaleBox, presetBox;
    std::array<juce::TextButton, 4> barButtons;
    juce::Slider levelSlider;
    IconButton menuButton { decor::Icon::Dots, decor::Icon::Dots, "Save favourite, loop codes, host sync, samples and folders" };
    std::unique_ptr<SliderAttachment> bpmAttachment, levelAttachment;
    std::unique_ptr<ComboAttachment> keyAttachment, scaleAttachment;
    std::unique_ptr<juce::ParameterAttachment> barsAttachment, styleAttachment, frameworkAttachment;

    // Ensemble
    IconButton searchButton { decor::Icon::Search, decor::Icon::Search, "Search instruments" };
    juce::TextEditor searchBox;
    juce::TextButton allTab { "ALL" }, favTab { "FAV" };
    bool favouritesOnly = false;
    juce::Viewport rowViewport;
    juce::Component rowHolder;
    juce::OwnedArray<InstrumentRow> rows;

    // Groove
    StyleDial styleDial;
    juce::OwnedArray<Knob> leftKnobs, rightKnobs, soundKnobs;
    juce::OwnedArray<SliderAttachment> knobAttachments;
    juce::ComboBox delayTimeBox;
    std::unique_ptr<ComboAttachment> delayTimeAttachment;
    juce::String styleName, frameworkName;
    juce::Rectangle<int> styleTitleArea, styleMenuArea;

    // Create
    GenerateButton generateButton;
    IconTextButton newRhythmButton { "NEW RHYTHM", decor::Icon::Wave }, newMelodyButton { "NEW MELODY", decor::Icon::Bars };
    IconButton backButton { decor::Icon::ChevronLeft, decor::Icon::ChevronLeft, "Previous loop" };
    IconButton forwardButton { decor::Icon::ChevronRight, decor::Icon::ChevronRight, "Next loop" };
    IconTextButton copyCodeButton { "COPY LOOP CODE", decor::Icon::Copy }, pasteCodeButton { "PASTE CODE", decor::Icon::Upload };

    // Results
    WaveformView waveform { processor };
    IconButton playButton { decor::Icon::Play, decor::Icon::Pause, "Play / stop the preview" };
    juce::TextButton exportButton { "EXPORT WAV" };
    juce::ComboBox depthBox, rateBox;
    IconTextButton folderButton { "OPEN FOLDER", decor::Icon::Folder };
    IconButton utilityButton { decor::Icon::Dots, decor::Icon::Dots, "Samples, folders and log" };

    std::unique_ptr<juce::FileChooser> chooser;
    bool fileDragActive = false;
    juce::String loopTitle, loopDetails, statusText;
    double statusUntil = 0.0;
    int tick = 0;
    juce::Rectangle<int> topBar, ensembleArea, grooveArea, createArea, bottomBar, captionsY;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrooveLabEditor)
};

} // namespace jgl

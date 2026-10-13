#include "PluginEditor.h"

#include "../Core/Scales.h"
#include "../Core/Styles.h"
#include "../IO/AppPaths.h"
#include "../IO/PresetManager.h"

namespace jgl
{

namespace
{
    constexpr int userPresetIdBase = 1000;

    void styleSmallButton (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, Colours::panelRaised);
    }

    void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title)
    {
        g.setColour (Colours::textDim);
        g.setFont (uiFont (12.0f, true));
        g.drawText (title, area.removeFromTop (18), juce::Justification::centredLeft);
    }
}

//==============================================================================
GrooveLabEditor::GrooveLabEditor (GrooveLabProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    setupTopBar();
    setupGroove();
    setupActions();
    setupResults();

    processor.addChangeListener (this);
    refreshResult();

    setSize (defaultWidth, defaultHeight);
    startTimerHz (30);
}

GrooveLabEditor::~GrooveLabEditor()
{
    stopTimer();
    processor.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

Knob& GrooveLabEditor::addKnob (juce::OwnedArray<Knob>& into, const char* paramId, const juce::String& caption, const juce::String& tooltip)
{
    auto* k = into.add (new Knob (caption));
    k->slider.setTooltip (tooltip);
    addAndMakeVisible (k);
    knobAttachments.add (new SliderAttachment (processor.getState(), paramId, k->slider));
    return *k;
}

//==============================================================================
void GrooveLabEditor::setupTopBar()
{
    auto& state = processor.getState();

    bpmSlider.setSliderStyle (juce::Slider::LinearBarVertical);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 100, 34);
    bpmSlider.setTextValueSuffix (" BPM");
    bpmSlider.setMouseDragSensitivity (300);
    bpmSlider.setTooltip ("Tempo. Drag up/down or double-click to type (70-160 BPM).");
    addAndMakeVisible (bpmSlider);
    bpmAttachment = std::make_unique<SliderAttachment> (state, ParamIDs::bpm, bpmSlider);
    bpmSlider.setNumDecimalPlacesToDisplay (1);

    tempoPresetBox.setTextWhenNothingSelected ("Tempo");
    tempoPresetBox.addItemList (tempoPresets(), 1);
    tempoPresetBox.setTooltip ("Common tempos");
    tempoPresetBox.onChange = [this]
    {
        const auto v = tempoPresetBox.getText().getFloatValue();
        if (v > 0.0f)
            if (auto* prm = processor.getState().getParameter (ParamIDs::bpm))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->convertTo0to1 (v));
                prm->endChangeGesture();
            }
        tempoPresetBox.setSelectedId (0, juce::dontSendNotification);
    };
    addAndMakeVisible (tempoPresetBox);

    hostTempoToggle.setTooltip ("Follow the host's tempo automatically");
    hostTempoToggle.setToggleState (processor.followHostTempo, juce::dontSendNotification);
    hostTempoToggle.onClick = [this] { processor.followHostTempo = hostTempoToggle.getToggleState(); };
    hostTempoToggle.setVisible (! processor.isStandalone());
    addChildComponent (hostTempoToggle);

    keyBox.addItemList (keyNames(), 1);
    keyBox.setTooltip ("Key (tonic / Sa). Melodic parts and the tabla's treble drum follow it; other drums are unpitched.");
    addAndMakeVisible (keyBox);
    keyAttachment = std::make_unique<ComboAttachment> (state, ParamIDs::key, keyBox);

    {
        int id = 1;
        scaleBox.addSectionHeading ("Scales & modes");
        for (int i = 0; i < numScales(); ++i)
        {
            const auto& s = scaleProfile (i);
            if (s.ragaInspired && i > 0 && ! scaleProfile (i - 1).ragaInspired)
                scaleBox.addSectionHeading ("Raga-inspired profiles (simplified)");
            scaleBox.addItem (s.name, id++);
        }
    }
    scaleBox.setTooltip ("Scale or raga-inspired profile. Raga profiles model aroha/avaroha and vadi/samvadi, not a full classical grammar.");
    addAndMakeVisible (scaleBox);
    scaleAttachment = std::make_unique<ComboAttachment> (state, ParamIDs::scale, scaleBox);

    const char* barNames[] = { "1", "2", "4", "8" };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = barButtons[(size_t) i];
        b.setButtonText (barNames[i]);
        b.setClickingTogglesState (false);
        b.setTooltip (juce::String (barNames[i]) + (i == 0 ? " bar loop" : " bar loop"));
        b.setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < 3 ? juce::Button::ConnectedOnRight : 0));
        b.onClick = [this, i] { if (barsAttachment) barsAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    if (auto* prm = state.getParameter (ParamIDs::bars))
    {
        barsAttachment = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v)
        {
            const int idx = juce::roundToInt (v);
            for (int i = 0; i < 4; ++i)
                barButtons[(size_t) i].setToggleState (i == idx, juce::dontSendNotification);
        });
        barsAttachment->sendInitialUpdate();
    }

    levelSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    levelSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    levelSlider.setTooltip ("Master output level (applied to preview and export; loops are normalised to -1 dBFS first)");
    levelSlider.setPopupDisplayEnabled (true, true, this);
    addAndMakeVisible (levelSlider);
    levelAttachment = std::make_unique<SliderAttachment> (state, ParamIDs::level, levelSlider);

    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.setTooltip ("Factory presets (fixed seeds) and your saved favourites");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        const auto& factory = PresetManager::factoryPresets();
        if (id <= (int) factory.size())
            processor.applyPreset (factory[(size_t) id - 1]);
        else if (id >= userPresetIdBase)
        {
            auto tree = PresetManager::loadUserPreset (presetBox.getItemText (presetBox.indexOfItemId (id)));
            if (tree.isValid()) processor.applyPreset (tree);
        }
        setStatus (processor.statusMessage);
    };
    addAndMakeVisible (presetBox);
    refreshPresetList();

    savePresetButton.setTooltip ("Save the current settings and exact loop (seeds) as a favourite preset");
    savePresetButton.onClick = [this] { savePresetDialog(); };
    addAndMakeVisible (savePresetButton);
}

void GrooveLabEditor::setupGroove()
{
    auto& state = processor.getState();

    allButton.onClick = [this] { processor.setAllInstruments (true); };
    noneButton.onClick = [this] { processor.setAllInstruments (false); };
    allButton.setTooltip ("Enable every instrument");
    noneButton.setTooltip ("Disable every instrument");
    styleSmallButton (allButton);
    styleSmallButton (noneButton);
    addAndMakeVisible (allButton);
    addAndMakeVisible (noneButton);

    for (auto id : allInstruments())
        addAndMakeVisible (tiles.add (new InstrumentTile (processor, id)));

    // Style: user selection also loads the personality's knob defaults.
    styleBox.addItemList (styleNames(), 1);
    styleBox.setTooltip ("Generation personality. Changes the pattern vocabulary, phrasing, microtiming and melodic behaviour, and loads its default knob positions.");
    if (auto* prm = state.getParameter (ParamIDs::style))
    {
        styleAttachment = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v)
        {
            const juce::ScopedValueSetter<bool> svs (updatingStyle, true);
            styleBox.setSelectedItemIndex (juce::roundToInt (v), juce::dontSendNotification);
        });
        styleAttachment->sendInitialUpdate();
    }
    styleBox.onChange = [this]
    {
        if (updatingStyle) return;
        const int idx = styleBox.getSelectedItemIndex();
        if (idx < 0 || styleAttachment == nullptr) return;
        styleAttachment->setValueAsCompleteGesture ((float) idx);
        processor.applyStyleDefaults (idx);
        setStatus (juce::String (styleProfile (idx).name) + ": " + styleProfile (idx).description);
    };
    addAndMakeVisible (styleBox);

    frameworkBox.addItemList (frameworkNames(), 1);
    frameworkBox.setTooltip ("Rhythmic framework (taal-inspired, adapted to a 4/4 club grid). 'Style Default' lets the personality choose.");
    addAndMakeVisible (frameworkBox);
    frameworkAttachment = std::make_unique<ComboAttachment> (state, ParamIDs::framework, frameworkBox);

    using namespace ParamIDs;
    addKnob (grooveKnobs, complexity, "Complexity", "Syncopation, ghost-note placement, rolls and polymeter");
    addKnob (grooveKnobs, swing, "Swing", "16th swing, 50% (straight) to 75%");
    addKnob (grooveKnobs, groove, "Groove", "Depth of the personality's microtiming and accent pocket");
    addKnob (grooveKnobs, humanize, "Humanize", "Per-hit timing/velocity variation plus slow player drift");
    addKnob (grooveKnobs, density, "Density", "How many notes: thins or fills the patterns");
    addKnob (grooveKnobs, energy, "Energy", "Picks busier/driving or calmer patterns, fill size, intensity");
    addKnob (grooveKnobs, variation, "Variation", "How much bars differ from each other (A / A' / B / fill)");
    addKnob (grooveKnobs, ghost, "Ghost", "Ghost-note level");
    addKnob (grooveKnobs, accent, "Accent", "Accent strength / dynamic contrast");

    addKnob (soundKnobs, saturation, "Saturation", "Warm tape-style saturation (2x oversampled)");
    addKnob (soundKnobs, reverb, "Reverb", "Room / hall send. Tails wrap seamlessly around the loop.");
    addKnob (soundKnobs, delay, "Delay", "Tempo-synced ping-pong delay send");
    addKnob (soundKnobs, filter, "Filter", "DJ filter: left = low-pass, right = high-pass, centre = off");

    delayTimeBox.addItemList (delayTimeNames(), 1);
    delayTimeBox.setTooltip ("Delay time (tempo-synced)");
    addAndMakeVisible (delayTimeBox);
    delayTimeAttachment = std::make_unique<ComboAttachment> (state, delayTime, delayTimeBox);
}

void GrooveLabEditor::setupActions()
{
    generateButton.setColour (juce::TextButton::buttonColourId, Colours::accent);
    generateButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    generateButton.setTooltip ("Generate a new loop. Locked parts are kept.");
    generateButton.onClick = [this] { processor.generateNew(); setStatus (processor.statusMessage); };
    addAndMakeVisible (generateButton);

    newRhythmButton.setTooltip ("New percussion, keep the melody");
    newMelodyButton.setTooltip ("New melody, keep the percussion");
    newRhythmButton.onClick = [this] { processor.regenerateFamily (true); setStatus (processor.statusMessage); };
    newMelodyButton.onClick = [this] { processor.regenerateFamily (false); setStatus (processor.statusMessage); };
    backButton.setTooltip ("Previous loop");
    forwardButton.setTooltip ("Next loop");
    backButton.onClick = [this] { processor.historyBack(); };
    forwardButton.onClick = [this] { processor.historyForward(); };

    copyCodeButton.setTooltip ("Copy a code that reproduces this exact loop (settings + seeds)");
    copyCodeButton.onClick = [this]
    {
        juce::SystemClipboard::copyTextToClipboard (processor.getLoopCode());
        setStatus ("Loop code copied to the clipboard");
    };
    pasteCodeButton.setTooltip ("Recreate a loop from a loop code in the clipboard");
    pasteCodeButton.onClick = [this]
    {
        if (processor.applyLoopCode (juce::SystemClipboard::getTextFromClipboard()))
            setStatus ("Loop recreated from code");
        else
            setStatus ("The clipboard does not contain a Jovah Groove Lab loop code");
    };

    for (auto* b : { &newRhythmButton, &newMelodyButton, &backButton, &forwardButton, &copyCodeButton, &pasteCodeButton })
    {
        styleSmallButton (*b);
        addAndMakeVisible (*b);
    }
}

void GrooveLabEditor::setupResults()
{
    waveform.onStatus = [this] (const juce::String& s) { setStatus (s); };
    waveform.onClick = [this] { playButton.triggerClick(); };
    dragHandle.onStatus = [this] (const juce::String& s) { setStatus (s); };
    addAndMakeVisible (waveform);
    addAndMakeVisible (dragHandle);

    playButton.setColour (juce::TextButton::buttonOnColourId, Colours::play);
    playButton.setTooltip ("Preview the loop");
    playButton.onClick = [this] { processor.getPlayer().setPlaying (! processor.getPlayer().isPlayRequested()); };
    addAndMakeVisible (playButton);

    syncToggle.setTooltip ("When the host is playing, lock the preview to the host's bar position");
    syncToggle.setToggleState (processor.getPlayer().isSyncedToHost(), juce::dontSendNotification);
    syncToggle.onClick = [this] { processor.getPlayer().setSyncToHost (syncToggle.getToggleState()); };
    syncToggle.setVisible (! processor.isStandalone());
    addChildComponent (syncToggle);

    exportButton.setColour (juce::TextButton::buttonColourId, Colours::panelRaised.brighter (0.08f));
    exportButton.setTooltip ("Write the loop to the exports folder");
    exportButton.onClick = [this]
    {
        juce::File f;
        const auto res = processor.exportCurrentLoop (f);
        setStatus (res.wasOk() ? "Saved " + f.getFullPathName() : res.getErrorMessage());
    };
    addAndMakeVisible (exportButton);

    depthBox.addItemList (bitDepthNames(), 1);
    depthBox.setSelectedItemIndex ((int) processor.exportDepth, juce::dontSendNotification);
    depthBox.setTooltip ("WAV format");
    depthBox.onChange = [this] { processor.exportDepth = (WavBitDepth) depthBox.getSelectedItemIndex(); };
    addAndMakeVisible (depthBox);

    {
        int id = 1, selected = 1;
        for (double r : exportSampleRates())
        {
            if (std::abs (r - processor.exportSampleRate) < 0.5) selected = id;
            rateBox.addItem (exportSampleRateName (r), id++);
        }
        rateBox.setSelectedId (selected, juce::dontSendNotification);
    }
    rateBox.setTooltip ("Export sample rate (the loop is re-rendered at that rate, not resampled)");
    rateBox.onChange = [this] { processor.exportSampleRate = exportSampleRates()[(size_t) juce::jmax (0, rateBox.getSelectedItemIndex())]; };
    addAndMakeVisible (rateBox);

    folderButton.setTooltip ("Show the exports folder");
    folderButton.onClick = [] { AppPaths::exportsFolder().startAsProcess(); };
    utilityButton.setTooltip ("Samples, folders and log");
    utilityButton.onClick = [this] { showUtilityMenu(); };
    styleSmallButton (folderButton);
    styleSmallButton (utilityButton);
    addAndMakeVisible (folderButton);
    addAndMakeVisible (utilityButton);

    statusLabel.setFont (uiFont (12.5f));
    statusLabel.setColour (juce::Label::textColourId, Colours::textDim);
    statusLabel.setText ("Export folder: " + AppPaths::exportsFolder().getFullPathName(), juce::dontSendNotification);
    addAndMakeVisible (statusLabel);
}

//==============================================================================
void GrooveLabEditor::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("Factory");
    int id = 1;
    for (auto& p : PresetManager::factoryPresets())
        presetBox.addItem (p.getProperty ("name").toString(), id++);

    const auto user = PresetManager::userPresetNames();
    if (! user.isEmpty())
    {
        presetBox.addSectionHeading ("Favourites");
        int uid = userPresetIdBase;
        for (auto& n : user)
            presetBox.addItem (n, uid++);
    }
}

void GrooveLabEditor::savePresetDialog()
{
    auto* w = new juce::AlertWindow ("Save favourite",
                                     "Saves all settings and the exact loop (its seeds).",
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", styleProfile (juce::roundToInt (processor.getState().getRawParameterValue (ParamIDs::style)->load())).name
                                 + juce::String (" ") + juce::Time::getCurrentTime().formatted ("%H%M%S"));
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
    {
        if (result != 1) return;
        const auto name = w->getTextEditorContents ("name").trim();
        const auto res = PresetManager::saveUserPreset (name, processor.capturePreset (name));
        setStatus (res.wasOk() ? "Saved favourite \"" + name + "\"" : res.getErrorMessage());
        refreshPresetList();
    }), true);
}

void GrooveLabEditor::showUtilityMenu()
{
    juce::PopupMenu m;
    auto lib = processor.getService().getSamples();
    const int n = lib != nullptr ? lib->numSamples() : 0;
    m.addSectionHeader (n > 0 ? juce::String (n) + " recorded samples loaded" : "Using modelled instruments (no samples installed)");
    m.addItem (1, "Open samples folder");
    m.addItem (2, "Rescan samples");
    m.addSeparator();
    m.addItem (3, "Open exports folder");
    m.addItem (4, "Open presets folder");
    m.addItem (5, "Open log file");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&utilityButton), [this] (int r)
    {
        switch (r)
        {
            case 1: AppPaths::samplesFolder().startAsProcess(); break;
            case 2: processor.getService().rescanSamples (AppPaths::samplesFolder()); setStatus ("Rescanning " + AppPaths::samplesFolder().getFullPathName()); break;
            case 3: AppPaths::exportsFolder().startAsProcess(); break;
            case 4: AppPaths::presetsFolder().startAsProcess(); break;
            case 5: AppPaths::logFile().revealToUser(); break;
            default: break;
        }
    });
}

void GrooveLabEditor::setStatus (const juce::String& s)
{
    statusLabel.setText (s, juce::dontSendNotification);
}

void GrooveLabEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshResult();
}

void GrooveLabEditor::refreshResult()
{
    auto r = processor.getCurrentResult();
    waveform.setResult (r);
    loopTitle = processor.getLoopTitle();
    loopDetails = processor.getLoopDetails();

    partSummary.clear();
    if (r != nullptr)
    {
        juce::StringArray parts;
        for (auto& p : r->arrangement.parts)
            parts.add (juce::String (instrumentInfo (p.instrument).name) + ": " + p.summary);
        partSummary = parts.joinIntoString ("     ");
    }
    repaint (resultArea);
}

void GrooveLabEditor::timerCallback()
{
    auto& player = processor.getPlayer();
    waveform.setPlayhead (player.getPlayheadFraction());
    waveform.setBusy (processor.isRenderPending());

    const bool playing = player.isPlayRequested();
    playButton.setToggleState (playing, juce::dontSendNotification);
    playButton.setButtonText (playing ? "STOP" : "PLAY");

    backButton.setEnabled (processor.canGoBack());
    forwardButton.setEnabled (processor.canGoForward());

    if (++tick % 15 == 0)
        for (auto* t : tiles)
            t->refresh();
}

//==============================================================================
void GrooveLabEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    // Top bar
    g.setColour (Colours::panel);
    g.fillRect (topBar);
    g.setColour (Colours::outline);
    g.drawHorizontalLine (topBar.getBottom(), 0.0f, (float) getWidth());

    auto brand = topBar.reduced (20, 0).removeFromLeft (230);
    g.setColour (Colours::text);
    g.setFont (uiFont (21.0f, true).withExtraKerningFactor (0.08f));
    g.drawText ("JOVAH", brand.removeFromTop (topBar.getHeight() / 2 + 6).withTrimmedTop (12), juce::Justification::bottomLeft);
    g.setColour (Colours::accent);
    g.setFont (uiFont (12.0f, true).withExtraKerningFactor (0.25f));
    g.drawText ("GROOVE LAB", brand.withTrimmedTop (2), juce::Justification::topLeft);

    // Small captions in the top bar
    g.setColour (Colours::textFaint);
    g.setFont (uiFont (10.0f, true));
    auto caption = [&] (juce::Component& c, const char* text)
    {
        g.drawText (text, c.getX(), c.getY() - 14, c.getWidth() + 60, 12, juce::Justification::centredLeft);
    };
    caption (bpmSlider, "TEMPO");
    caption (keyBox, "KEY");
    caption (scaleBox, "SCALE");
    caption (barButtons[0], "BARS");
    caption (levelSlider, "OUTPUT");
    caption (presetBox, "PRESET");

    // Panels
    for (auto area : { ensembleArea, grooveArea, actionArea, resultArea })
    {
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (area.toFloat(), 10.0f);
        g.setColour (Colours::outline);
        g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 10.0f, 1.0f);
    }

    drawSectionTitle (g, ensembleArea.reduced (16, 12), "ENSEMBLE");
    drawSectionTitle (g, grooveArea.reduced (16, 12), "GROOVE");
    {
        auto soundTitle = grooveArea.reduced (16, 12);
        soundTitle.setY (soundKnobs.isEmpty() ? soundTitle.getY() : soundKnobs[0]->getY() - 22);
        drawSectionTitle (g, soundTitle, "SOUND");
    }
    drawSectionTitle (g, actionArea.reduced (16, 12), "CREATE");

    // Result texts
    auto res = resultArea.reduced (18, 12);
    g.setColour (Colours::text);
    g.setFont (uiFont (19.0f, true));
    g.drawText (loopTitle, res.removeFromTop (26), juce::Justification::centredLeft);
    g.setColour (Colours::textDim);
    g.setFont (uiFont (13.0f));
    g.drawText (loopDetails, res.removeFromTop (20), juce::Justification::centredLeft);
    g.setColour (Colours::textFaint);
    g.setFont (uiFont (11.5f));
    g.drawText (partSummary, res.removeFromTop (18), juce::Justification::centredLeft, true);

    // Level read-out
    g.setColour (Colours::textDim);
    g.setFont (uiFont (11.0f));
    const auto lvl = processor.getState().getRawParameterValue (ParamIDs::level)->load();
    g.drawText (juce::String (lvl, 1) + " dB", levelSlider.getX(), levelSlider.getY() - 14, levelSlider.getWidth(), 12,
                juce::Justification::centredRight);
}

void GrooveLabEditor::resized()
{
    auto r = getLocalBounds();
    topBar = r.removeFromTop (76);

    // ---- top bar
    {
        auto t = topBar.reduced (20, 0);
        t.removeFromLeft (230);
        auto row = t.withTrimmedTop (30).withHeight (34);

        bpmSlider.setBounds (row.removeFromLeft (118));
        row.removeFromLeft (4);
        tempoPresetBox.setBounds (row.removeFromLeft (34));
        row.removeFromLeft (6);
        if (hostTempoToggle.isVisible())
        {
            hostTempoToggle.setBounds (row.removeFromLeft (108));
            row.removeFromLeft (6);
        }
        row.removeFromLeft (10);
        keyBox.setBounds (row.removeFromLeft (70));
        row.removeFromLeft (10);
        scaleBox.setBounds (row.removeFromLeft (184));
        row.removeFromLeft (16);
        for (auto& b : barButtons)
            b.setBounds (row.removeFromLeft (38));
        row.removeFromLeft (18);

        savePresetButton.setBounds (row.removeFromRight (56));
        row.removeFromRight (6);
        presetBox.setBounds (row.removeFromRight (160));
        row.removeFromRight (14);
        levelSlider.setBounds (row);
    }

    r.reduce (16, 14);

    // ---- results (bottom)
    resultArea = r.removeFromBottom (268);
    r.removeFromBottom (14);
    {
        auto res = resultArea.reduced (18, 12);
        res.removeFromTop (26 + 20 + 18 + 6);
        auto bottom = res.removeFromBottom (24);
        statusLabel.setBounds (bottom);
        res.removeFromBottom (8);

        auto transport = res.removeFromBottom (36);
        playButton.setBounds (transport.removeFromLeft (96));
        transport.removeFromLeft (10);
        if (syncToggle.isVisible())
            syncToggle.setBounds (transport.removeFromLeft (190));
        utilityButton.setBounds (transport.removeFromRight (40));
        transport.removeFromRight (6);
        folderButton.setBounds (transport.removeFromRight (104));
        transport.removeFromRight (12);
        rateBox.setBounds (transport.removeFromRight (124));
        transport.removeFromRight (6);
        depthBox.setBounds (transport.removeFromRight (124));
        transport.removeFromRight (6);
        exportButton.setBounds (transport.removeFromRight (128));
        res.removeFromBottom (10);

        dragHandle.setBounds (res.removeFromRight (86));
        res.removeFromRight (10);
        waveform.setBounds (res);
    }

    // ---- three columns
    ensembleArea = r.removeFromLeft (372);
    r.removeFromLeft (14);
    actionArea = r.removeFromRight (232);
    r.removeFromRight (14);
    grooveArea = r;

    {
        auto e = ensembleArea.reduced (16, 12);
        auto header = e.removeFromTop (22);
        noneButton.setBounds (header.removeFromRight (58).withSizeKeepingCentre (58, 22));
        header.removeFromRight (6);
        allButton.setBounds (header.removeFromRight (48).withSizeKeepingCentre (48, 22));
        e.removeFromTop (10);

        const int cols = 2, rows = (tiles.size() + 1) / 2, gap = 8;
        const int tw = (e.getWidth() - gap) / cols;
        const int th = juce::jmin (66, (e.getHeight() - gap * (rows - 1)) / rows);
        for (int i = 0; i < tiles.size(); ++i)
            tiles[i]->setBounds (e.getX() + (i % cols) * (tw + gap), e.getY() + (i / cols) * (th + gap), tw, th);
    }

    {
        auto gr = grooveArea.reduced (16, 12);
        gr.removeFromTop (24);
        auto combos = gr.removeFromTop (30);
        styleBox.setBounds (combos.removeFromLeft (combos.getWidth() / 2 - 5));
        combos.removeFromLeft (10);
        frameworkBox.setBounds (combos);
        gr.removeFromTop (8);

        const int knobW = gr.getWidth() / 5;
        const int knobH = 86;
        auto row1 = gr.removeFromTop (knobH);
        gr.removeFromTop (4);
        auto row2 = gr.removeFromTop (knobH);
        for (int i = 0; i < grooveKnobs.size(); ++i)
        {
            auto& row = i < 5 ? row1 : row2;
            grooveKnobs[i]->setBounds (row.removeFromLeft (knobW).reduced (4, 0));
        }

        gr.removeFromTop (24);
        auto row3 = gr.removeFromTop (knobH);
        for (int i = 0; i < soundKnobs.size(); ++i)
        {
            soundKnobs[i]->setBounds (row3.removeFromLeft (knobW).reduced (4, 0));
            if (i == 2)
                delayTimeBox.setBounds (row3.removeFromLeft (knobW).withSizeKeepingCentre (knobW - 10, 28));
        }
    }

    {
        auto a = actionArea.reduced (16, 12);
        a.removeFromTop (26);
        generateButton.setBounds (a.removeFromTop (96));
        a.removeFromTop (10);
        auto pair = a.removeFromTop (32);
        newRhythmButton.setBounds (pair.removeFromLeft (pair.getWidth() / 2 - 4));
        pair.removeFromLeft (8);
        newMelodyButton.setBounds (pair);
        a.removeFromTop (8);
        auto hist = a.removeFromTop (32);
        backButton.setBounds (hist.removeFromLeft (hist.getWidth() / 2 - 4));
        hist.removeFromLeft (8);
        forwardButton.setBounds (hist);
        a.removeFromTop (8);
        copyCodeButton.setBounds (a.removeFromTop (30));
        a.removeFromTop (6);
        pasteCodeButton.setBounds (a.removeFromTop (30));
    }
}

} // namespace jgl

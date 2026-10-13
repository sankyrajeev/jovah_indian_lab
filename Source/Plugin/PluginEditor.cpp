#include "PluginEditor.h"

#include "../Core/Scales.h"
#include "../Core/Styles.h"
#include "../IO/AppPaths.h"
#include "../IO/PresetManager.h"
#include "../UI/Decor.h"
#include "../UI/VoicePanel.h"

namespace jgl
{

namespace
{
    constexpr int userPresetIdBase = 1000;

    enum MenuIds
    {
        idSaveFavourite = 1, idCopyCode, idPasteCode, idSelectAll, idClearAll, idHostTempo, idHostSync,
        idOpenSamples = 50, idRescan, idOpenExports, idOpenPresets, idOpenLog,
        idStyleBase = 100, idFrameworkBase = 200
    };

    void setStyle (juce::Component& c, const char* s) { c.getProperties().set ("jglStyle", s); }

    void caption (juce::Graphics& g, const juce::Component& c, const juce::String& text)
    {
        g.setColour (Colours::inkSoft);
        g.setFont (Fonts::caps (10.5f));
        g.drawText (text, c.getX(), c.getY() - 18, c.getWidth() + 40, 14, juce::Justification::centredLeft);
    }
}

//==============================================================================
GrooveLabEditor::GrooveLabEditor (GrooveLabProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    setupTopBar();
    setupEnsemble();
    setupGroove();
    setupCreate();
    setupResults();

    // Children created before being parented built sub-components with the default look.
    sendLookAndFeelChange();

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

Knob& GrooveLabEditor::addKnob (juce::OwnedArray<Knob>& into, const char* paramId, const juce::String& cap, const juce::String& tooltip)
{
    auto* k = into.add (new Knob (cap));
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
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 100, 36);
    bpmSlider.setTextValueSuffix (" BPM");
    bpmSlider.setMouseDragSensitivity (300);
    bpmSlider.setTooltip ("Tempo: drag up/down or double-click to type (70-160 BPM)");
    addAndMakeVisible (bpmSlider);
    bpmAttachment = std::make_unique<SliderAttachment> (state, ParamIDs::bpm, bpmSlider);
    bpmSlider.setNumDecimalPlacesToDisplay (1);

    auto nudge = [this] (double delta)
    {
        if (auto* prm = processor.getState().getParameter (ParamIDs::bpm))
        {
            const float v = juce::jlimit (70.0f, 160.0f, (float) std::round (bpmSlider.getValue() + delta));
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
            prm->endChangeGesture();
        }
    };
    bpmUp.onClick = [nudge] { nudge (1.0); };
    bpmDown.onClick = [nudge] { nudge (-1.0); };
    addAndMakeVisible (bpmUp);
    addAndMakeVisible (bpmDown);

    keyBox.addItemList (keyNames(), 1);
    keyBox.setTooltip ("Key (Sa). Melodic parts and the tabla's treble drum follow it; other drums are unpitched.");
    addAndMakeVisible (keyBox);
    keyAttachment = std::make_unique<ComboAttachment> (state, ParamIDs::key, keyBox);

    scaleBox.addSectionHeading ("Scales & modes");
    for (int i = 0; i < numScales(); ++i)
    {
        const auto& s = scaleProfile (i);
        if (s.ragaInspired && i > 0 && ! scaleProfile (i - 1).ragaInspired)
            scaleBox.addSectionHeading ("Raga-inspired profiles (simplified)");
        scaleBox.addItem (s.name, i + 1);
    }
    scaleBox.setTooltip ("Scale or raga-inspired profile (aroha/avaroha and vadi/samvadi; not a full classical grammar)");
    addAndMakeVisible (scaleBox);
    scaleAttachment = std::make_unique<ComboAttachment> (state, ParamIDs::scale, scaleBox);

    const char* barNames[] = { "1", "2", "4", "8" };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = barButtons[(size_t) i];
        b.setButtonText (barNames[i]);
        setStyle (b, "segment");
        b.setTooltip (juce::String (barNames[i]) + (i == 0 ? " bar" : " bars"));
        b.onClick = [this, i] { if (barsAttachment) barsAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    if (auto* prm = state.getParameter (ParamIDs::bars))
    {
        barsAttachment = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v)
        {
            for (int i = 0; i < 4; ++i)
                barButtons[(size_t) i].setToggleState (i == juce::roundToInt (v), juce::dontSendNotification);
        });
        barsAttachment->sendInitialUpdate();
    }

    levelSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    levelSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    levelSlider.setPopupDisplayEnabled (true, true, this);
    levelSlider.setTooltip ("Output level (loops are normalised to -1 dBFS first)");
    addAndMakeVisible (levelSlider);
    levelAttachment = std::make_unique<SliderAttachment> (state, ParamIDs::level, levelSlider);

    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.setTooltip ("Factory presets and your favourites");
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

    menuButton.framed = true;
    menuButton.onClick = [this] { showMainMenu(); };
    addAndMakeVisible (menuButton);
}

void GrooveLabEditor::setupEnsemble()
{
    searchButton.onClick = [this]
    {
        searchBox.setVisible (! searchBox.isVisible());
        if (searchBox.isVisible()) searchBox.grabKeyboardFocus();
        else searchBox.clear();
        layoutRows();
        repaint (ensembleArea);
    };
    addAndMakeVisible (searchButton);

    searchBox.setTextToShowWhenEmpty ("Search", Colours::inkFaint);
    searchBox.setFont (Fonts::serif (17.0f));
    searchBox.onTextChange = [this] { layoutRows(); };
    searchBox.onEscapeKey = [this] { searchBox.clear(); searchBox.setVisible (false); layoutRows(); repaint (ensembleArea); };
    addChildComponent (searchBox);

    for (auto* tab : { &allTab, &favTab })
    {
        setStyle (*tab, "flat");
        addAndMakeVisible (*tab);
    }
    allTab.onClick = [this] { favouritesOnly = false; layoutRows(); repaint (ensembleArea); };
    favTab.onClick = [this] { favouritesOnly = true; layoutRows(); repaint (ensembleArea); };
    allTab.setTooltip ("All instruments");
    favTab.setTooltip ("Favourite instruments only");

    rowViewport.setViewedComponent (&rowHolder, false);
    rowViewport.setScrollBarsShown (true, false);
    rowViewport.setScrollBarThickness (6);
    addAndMakeVisible (rowViewport);

    std::vector<InstrumentId> order { InstrumentId::CustomVoice };   // your own sound first
    for (auto id : allInstruments())
        if (id != InstrumentId::CustomVoice)
            order.push_back (id);

    for (auto id : order)
    {
        auto* row = rows.add (new InstrumentRow (processor, id));
        row->onFavouriteChanged = [this] { if (favouritesOnly) layoutRows(); };
        row->onLoadSample = [this] { chooseVoiceSample(); };
        row->onVoiceSettings = [this] { showVoicePanel(); };
        rowHolder.addAndMakeVisible (row);
    }
}

void GrooveLabEditor::setupGroove()
{
    auto& state = processor.getState();

    styleDial.setNumItems (numStyles);
    styleDial.setTooltip ("Generation personality: drag, scroll or click to choose. Loads the style's default knob settings.");
    if (auto* prm = state.getParameter (ParamIDs::style))
    {
        styleAttachment = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v)
        {
            const int i = juce::roundToInt (v);
            styleDial.setIndex (i);
            styleName = styleProfile (i).name;
            repaint (styleTitleArea.getUnion (styleMenuArea));
        });
        styleAttachment->sendInitialUpdate();
    }
    if (auto* prm = state.getParameter (ParamIDs::framework))
    {
        frameworkAttachment = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v)
        {
            const int i = juce::roundToInt (v);
            frameworkName = i == 0 ? juce::String() : juce::String (framework ((FrameworkId) i).name);
            repaint (styleMenuArea);
        });
        frameworkAttachment->sendInitialUpdate();
    }
    styleDial.onUserChange = [this] (int i)
    {
        if (styleAttachment) styleAttachment->setValueAsCompleteGesture ((float) i);
        processor.applyStyleDefaults (i);
        setStatus (juce::String (styleProfile (i).name) + ": " + styleProfile (i).description);
    };
    styleDial.onClick = [this] { showStyleMenu(); };
    addAndMakeVisible (styleDial);

    using namespace ParamIDs;
    addKnob (leftKnobs, complexity, "Complexity", "Syncopation, ghost-note placement, rolls and polymeter");
    addKnob (leftKnobs, energy, "Energy", "Busier, driving patterns and bigger fills, or calmer ones");
    addKnob (leftKnobs, variation, "Variation", "How much bars differ (A / A' / B / fill)");
    addKnob (leftKnobs, humanize, "Humanize", "Per-hit timing/velocity variation plus slow player drift");
    addKnob (rightKnobs, groove, "Groove", "Depth of the style's microtiming and accent pocket");
    addKnob (rightKnobs, swing, "Swing", "16th swing, 50% (straight) to 75%");
    addKnob (rightKnobs, density, "Density", "Thin out or fill in the patterns");
    addKnob (rightKnobs, accent, "Accent", "Accent strength / dynamic contrast");
    addKnob (soundKnobs, ghost, "Ghost", "Ghost-note level");
    addKnob (soundKnobs, saturation, "Saturation", "Warm saturation (2x oversampled)");
    addKnob (soundKnobs, reverb, "Reverb", "Room / hall; tails wrap seamlessly around the loop");
    addKnob (soundKnobs, delay, "Delay", "Tempo-synced ping-pong delay");
    addKnob (soundKnobs, filter, "Filter", "DJ filter: left = low-pass, right = high-pass, centre = off");

    delayTimeBox.addItemList (delayTimeNames(), 1);
    delayTimeBox.setTooltip ("Delay time (tempo-synced)");
    addAndMakeVisible (delayTimeBox);
    delayTimeAttachment = std::make_unique<ComboAttachment> (state, delayTime, delayTimeBox);
}

void GrooveLabEditor::setupCreate()
{
    generateButton.onClick = [this] { processor.generateNew(); setStatus (processor.statusMessage); };
    addAndMakeVisible (generateButton);

    newRhythmButton.setTooltip ("New percussion, keep the melody");
    newMelodyButton.setTooltip ("New melody, keep the percussion");
    newRhythmButton.onClick = [this] { processor.regenerateFamily (true); setStatus (processor.statusMessage); };
    newMelodyButton.onClick = [this] { processor.regenerateFamily (false); setStatus (processor.statusMessage); };
    backButton.onClick = [this] { processor.historyBack(); };
    forwardButton.onClick = [this] { processor.historyForward(); };
    backButton.framed = forwardButton.framed = true;

    copyCodeButton.setTooltip ("Copy a code that reproduces this exact loop (settings + seeds)");
    copyCodeButton.onClick = [this]
    {
        juce::SystemClipboard::copyTextToClipboard (processor.getLoopCode());
        setStatus ("Loop code copied to the clipboard");
    };
    pasteCodeButton.setTooltip ("Recreate a loop from a loop code in the clipboard");
    pasteCodeButton.onClick = [this]
    {
        setStatus (processor.applyLoopCode (juce::SystemClipboard::getTextFromClipboard())
                       ? "Loop recreated from code" : "The clipboard does not contain a loop code");
    };

    for (juce::Component* c : { (juce::Component*) &newRhythmButton, (juce::Component*) &newMelodyButton, (juce::Component*) &backButton,
                                 (juce::Component*) &forwardButton, (juce::Component*) &copyCodeButton, (juce::Component*) &pasteCodeButton })
        addAndMakeVisible (c);
}

void GrooveLabEditor::setupResults()
{
    waveform.onStatus = [this] (const juce::String& s) { setStatus (s); };
    waveform.onClick = [this] { playButton.triggerClick(); };
    addAndMakeVisible (waveform);

    playButton.roundDark = true;
    playButton.onClick = [this] { processor.getPlayer().setPlaying (! processor.getPlayer().isPlayRequested()); };
    addAndMakeVisible (playButton);

    exportButton.setTooltip ("Write the loop to " + AppPaths::exportsFolder().getFullPathName());
    exportButton.onClick = [this]
    {
        juce::File f;
        const auto res = processor.exportCurrentLoop (f);
        setStatus (res.wasOk() ? "Saved " + f.getFileName() : res.getErrorMessage());
    };
    addAndMakeVisible (exportButton);

    depthBox.addItemList (bitDepthNames(), 1);
    depthBox.setSelectedItemIndex ((int) processor.exportDepth, juce::dontSendNotification);
    depthBox.setTooltip ("WAV format");
    depthBox.onChange = [this] { processor.exportDepth = (WavBitDepth) depthBox.getSelectedItemIndex(); };
    addAndMakeVisible (depthBox);

    int id = 1, selected = 1;
    for (double r : exportSampleRates())
    {
        if (std::abs (r - processor.exportSampleRate) < 0.5) selected = id;
        rateBox.addItem (exportSampleRateName (r), id++);
    }
    rateBox.setSelectedId (selected, juce::dontSendNotification);
    rateBox.setTooltip ("Export sample rate (re-rendered, not resampled)");
    rateBox.onChange = [this] { processor.exportSampleRate = exportSampleRates()[(size_t) juce::jmax (0, rateBox.getSelectedItemIndex())]; };
    addAndMakeVisible (rateBox);

    folderButton.setTooltip ("Show the exports folder");
    folderButton.onClick = [] { AppPaths::exportsFolder().startAsProcess(); };
    addAndMakeVisible (folderButton);

    utilityButton.framed = true;
    utilityButton.onClick = [this]
    {
        juce::PopupMenu m;
        m.setLookAndFeel (&lnf);
        addUtilityItems (m);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&utilityButton), [this] (int r) { handleUtilityResult (r); });
    };
    addAndMakeVisible (utilityButton);
}

//==============================================================================
void GrooveLabEditor::addUtilityItems (juce::PopupMenu& m)
{
    auto lib = processor.getService().getSamples();
    const int n = lib != nullptr ? lib->numSamples() : 0;
    m.addSectionHeader (n > 0 ? juce::String (n) + " recorded samples loaded" : "Modelled instruments (no samples installed)");
    m.addItem (idOpenSamples, "Open samples folder");
    m.addItem (idRescan, "Rescan samples");
    m.addSeparator();
    m.addItem (idOpenExports, "Open exports folder");
    m.addItem (idOpenPresets, "Open presets folder");
    m.addItem (idOpenLog, "Open log file");
}

void GrooveLabEditor::handleUtilityResult (int r)
{
    switch (r)
    {
        case idOpenSamples: AppPaths::samplesFolder().startAsProcess(); break;
        case idRescan:      processor.getService().rescanSamples (AppPaths::samplesFolder()); setStatus ("Rescanning samples..."); break;
        case idOpenExports: AppPaths::exportsFolder().startAsProcess(); break;
        case idOpenPresets: AppPaths::presetsFolder().startAsProcess(); break;
        case idOpenLog:     AppPaths::logFile().revealToUser(); break;
        default: break;
    }
}

void GrooveLabEditor::showMainMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    m.addItem (idSaveFavourite, "Save as favourite preset...");
    m.addItem (idCopyCode, "Copy loop code");
    m.addItem (idPasteCode, "Paste loop code");
    m.addSeparator();
    m.addItem (idSelectAll, "Select all instruments");
    m.addItem (idClearAll, "Clear instruments");
    if (! processor.isStandalone())
    {
        m.addSeparator();
        m.addItem (idHostTempo, "Follow host tempo", true, processor.followHostTempo);
        m.addItem (idHostSync, "Sync preview to host transport", true, processor.getPlayer().isSyncedToHost());
    }
    m.addSeparator();
    addUtilityItems (m);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton), [this] (int r)
    {
        switch (r)
        {
            case idSaveFavourite: savePresetDialog(); break;
            case idCopyCode:      copyCodeButton.triggerClick(); break;
            case idPasteCode:     pasteCodeButton.triggerClick(); break;
            case idSelectAll:     processor.setAllInstruments (true); break;
            case idClearAll:      processor.setAllInstruments (false); break;
            case idHostTempo:     processor.followHostTempo = ! processor.followHostTempo; break;
            case idHostSync:      processor.getPlayer().setSyncToHost (! processor.getPlayer().isSyncedToHost()); break;
            default:              handleUtilityResult (r); break;
        }
    });
}

void GrooveLabEditor::showStyleMenu()
{
    juce::PopupMenu m, fw;
    m.setLookAndFeel (&lnf);
    fw.setLookAndFeel (&lnf);
    m.addSectionHeader ("Style");
    for (int i = 0; i < numStyles; ++i)
        m.addItem (idStyleBase + i, styleProfile (i).name, true, i == styleDial.getIndex());

    const int currentFw = juce::roundToInt (processor.getState().getRawParameterValue (ParamIDs::framework)->load());
    for (int i = 0; i < numFrameworks; ++i)
        fw.addItem (idFrameworkBase + i, framework ((FrameworkId) i).name, true, i == currentFw);
    m.addSeparator();
    m.addSubMenu ("Rhythmic framework", fw);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (styleMenuArea)), [this] (int r)
    {
        if (r >= idStyleBase && r < idStyleBase + numStyles)
        {
            const int i = r - idStyleBase;
            styleDial.setIndex (i);
            if (styleAttachment) styleAttachment->setValueAsCompleteGesture ((float) i);
            processor.applyStyleDefaults (i);
            setStatus (juce::String (styleProfile (i).name) + ": " + styleProfile (i).description);
        }
        else if (r >= idFrameworkBase && r < idFrameworkBase + numFrameworks && frameworkAttachment)
        {
            frameworkAttachment->setValueAsCompleteGesture ((float) (r - idFrameworkBase));
        }
    });
}

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
    auto* w = new juce::AlertWindow ("Save favourite", "Saves all settings and this exact loop.", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", styleName + " " + juce::Time::getCurrentTime().formatted ("%H%M%S"));
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

void GrooveLabEditor::setStatus (const juce::String& s)
{
    statusText = s;
    statusUntil = juce::Time::getMillisecondCounterHiRes() + 4500.0;
    repaint (bottomBar);
}

void GrooveLabEditor::changeListenerCallback (juce::ChangeBroadcaster*) { refreshResult(); }

void GrooveLabEditor::refreshResult()
{
    waveform.setResult (processor.getCurrentResult());
    loopTitle = processor.getLoopTitle();
    loopDetails = processor.getLoopDetails();
    repaint (bottomBar);
}

void GrooveLabEditor::timerCallback()
{
    auto& player = processor.getPlayer();
    waveform.setPlayhead (player.getPlayheadFraction());
    waveform.setBusy (processor.isRenderPending());
    playButton.setToggleState (player.isPlayRequested(), juce::dontSendNotification);
    backButton.setEnabled (processor.canGoBack());
    forwardButton.setEnabled (processor.canGoForward());

    if (statusUntil > 0.0 && juce::Time::getMillisecondCounterHiRes() > statusUntil)
    {
        statusUntil = 0.0;
        repaint (bottomBar);
    }

    if (++tick % 15 == 0)
        for (auto* r : rows)
            r->refresh();
}

//==============================================================================
void GrooveLabEditor::layoutRows()
{
    const auto filter = searchBox.getText().trim();
    const int w = rowViewport.getWidth() - (rowViewport.isVerticalScrollBarShown() ? 6 : 0);
    const int rowH = 50;
    int y = 0;
    for (auto* r : rows)
    {
        const auto& info = instrumentInfo (r->getInstrument());
        const bool show = (! favouritesOnly || r->isFavourite())
                       && (filter.isEmpty() || juce::String (info.name).containsIgnoreCase (filter) || juce::String (info.role).containsIgnoreCase (filter));
        r->setVisible (show);
        if (show)
        {
            r->setBounds (0, y, juce::jmax (100, w), rowH);
            y += rowH;
        }
    }
    rowHolder.setSize (juce::jmax (100, w), juce::jmax (y, 1));
}

void GrooveLabEditor::paint (juce::Graphics& g)
{
    decor::drawPaper (g, getLocalBounds());
    const auto flourish = Colours::copperDeep.withAlpha (0.28f);
    decor::drawFloralCorner (g, { 0.0f, 300.0f, 70.0f, 220.0f }, false, false, flourish);
    decor::drawFloralCorner (g, { (float) getWidth() - 90.0f, 2.0f, 90.0f, 120.0f }, true, false, flourish);

    // Logo
    {
        auto logo = topBar.withWidth (250).reduced (28, 10);
        g.setColour (Colours::ink);
        g.setFont (Fonts::caps (40.0f, 0.12f));
        g.drawText ("JOVAH", logo.removeFromTop (46), juce::Justification::bottomLeft);
        g.setColour (Colours::copperDeep);
        g.setFont (Fonts::caps (15.0f, 0.42f));
        g.drawText ("GROOVE LAB", logo.translated (22, 0), juce::Justification::topLeft);
    }

    // Top-bar captions
    caption (g, bpmSlider, "TEMPO");
    caption (g, keyBox, "KEY");
    caption (g, scaleBox, "SCALE");
    caption (g, barButtons[0], "BARS");
    caption (g, levelSlider, "OUTPUT");
    caption (g, presetBox, "PRESET");

    // Bars segment frame
    {
        auto seg = barButtons[0].getBounds().getUnion (barButtons[3].getBounds()).toFloat().expanded (4.0f, 2.0f);
        g.setColour (Colours::paperLight);
        g.fillRoundedRectangle (seg, 8.0f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (seg, 8.0f, 1.0f);
    }

    // Panels
    auto panel = [&g] (juce::Rectangle<float> r)
    {
        g.setColour (juce::Colours::black.withAlpha (0.05f));
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 14.0f);
        g.setColour (Colours::panel.withAlpha (0.82f));
        g.fillRoundedRectangle (r, 14.0f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);
    };
    panel (ensembleArea.toFloat());

    {
        auto arch = decor::archPanel (grooveArea.toFloat(), 46.0f);
        g.setColour (juce::Colours::black.withAlpha (0.05f));
        g.fillPath (arch, juce::AffineTransform::translation (0.0f, 2.0f));
        g.setColour (Colours::panel.withAlpha (0.82f));
        g.fillPath (arch);
        g.setColour (Colours::line);
        g.strokePath (arch, juce::PathStrokeType (1.0f));
        g.setColour (Colours::line.withAlpha (0.7f));
        g.strokePath (decor::archPanel (grooveArea.toFloat().reduced (8.0f), 40.0f), juce::PathStrokeType (0.8f));
    }

    // Create panel with its painting
    {
        auto c = createArea.toFloat();
        panel (c);
        juce::Graphics::ScopedSaveState s (g);
        juce::Path clip;
        clip.addRoundedRectangle (c.reduced (1.0f), 14.0f);
        g.reduceClipRegion (clip);
        decor::drawTempleSunset (g, c.withTrimmedTop (c.getHeight() * 0.22f));
        g.setGradientFill (juce::ColourGradient (Colours::panel, c.getX(), c.getY() + c.getHeight() * 0.18f,
                                                 Colours::panel.withAlpha (0.0f), c.getX(), c.getY() + c.getHeight() * 0.42f, false));
        g.fillRect (c.withHeight (c.getHeight() * 0.45f));
    }

    // Section titles
    g.setColour (Colours::inkSoft);
    g.setFont (Fonts::caps (12.0f, 0.22f));
    if (! searchBox.isVisible())
        g.drawText ("ENSEMBLE", ensembleArea.getX() + 24, ensembleArea.getY() + 18, 150, 20, juce::Justification::centredLeft);
    g.drawText ("CREATE", createArea.getX() + 24, createArea.getY() + 18, 150, 20, juce::Justification::centredLeft);

    // Tab underline dots
    {
        auto& active = favouritesOnly ? favTab : allTab;
        const auto c = active.getBounds().toFloat();
        g.setColour (Colours::copper);
        g.fillEllipse (c.getCentreX() - 6.0f, c.getBottom() - 1.0f, 4.0f, 4.0f);
        g.fillEllipse (c.getCentreX() + 2.0f, c.getBottom() - 1.0f, 4.0f, 4.0f);
        if (! favouritesOnly)
        {
            g.setColour (Colours::paperLight);
            g.fillRoundedRectangle (allTab.getBounds().toFloat().reduced (2.0f, 4.0f), 6.0f);
        }
    }

    // Style title with ornaments
    {
        const auto t = styleTitleArea.toFloat();
        decor::drawFlowerOrnament (g, { t.getCentreX(), t.getY() - 18.0f }, 22.0f, Colours::copper);
        g.setColour (Colours::ink);
        g.setFont (Fonts::serif (44.0f));
        g.drawText (styleName, t, juce::Justification::centred);
        const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), styleName);
        const float y = t.getCentreY() + 4.0f;
        const float room = (float) grooveArea.getWidth() * 0.5f - 140.0f - tw * 0.5f;   // space before the knob columns
        if (room > 24.0f)
            for (float s : { -1.0f, 1.0f })
            {
                const float x = t.getCentreX() + s * (tw * 0.5f + 20.0f);
                decor::drawDiamond (g, { x, y }, 5.0f, Colours::copper, false);
                g.setColour (Colours::copper.withAlpha (0.6f));
                g.drawLine (x + s * 8.0f, y, x + s * juce::jlimit (8.0f, 60.0f, room - 10.0f), y, 0.8f);
            }
        g.setColour (Colours::ink);
        g.setFont (Fonts::caps (12.0f, 0.3f));
        const auto label = frameworkName.isEmpty() ? juce::String ("STYLE") : "STYLE  |  " + frameworkName.toUpperCase();
        g.drawText (label, styleMenuArea.withTrimmedRight (16), juce::Justification::centred);
        decor::drawIcon (g, decor::Icon::ChevronDown,
                         { (float) styleMenuArea.getCentreX() + juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), label) * 0.5f - 2.0f,
                           (float) styleMenuArea.getCentreY() - 6.0f, 12.0f, 12.0f }, Colours::ink);

        decor::drawFlowerOrnament (g, { (float) styleDial.getBounds().getCentreX(), (float) styleDial.getBottom() + 4.0f }, 18.0f, Colours::copper);
    }

    // Separators in the sound row
    if (soundKnobs.size() == 5)
    {
        g.setColour (Colours::line);
        const int x1 = (soundKnobs[0]->getRight() + soundKnobs[1]->getX()) / 2;
        const int x2 = (soundKnobs[3]->getRight() + soundKnobs[4]->getX()) / 2;
        for (int x : { x1, x2 })
            g.drawVerticalLine (x, (float) soundKnobs[0]->getY() + 8.0f, (float) soundKnobs[0]->getBottom() - 8.0f);
    }

    // Bottom bar texts
    {
        auto b = bottomBar.withTrimmedLeft (playButton.getRight() - bottomBar.getX() + 18).withRight (exportButton.getX() - 16);
        g.setColour (Colours::ink);
        g.setFont (Fonts::serif (25.0f));
        g.drawText (loopTitle, b.removeFromTop (b.getHeight() / 2 + 4), juce::Justification::bottomLeft, true);
        const bool showStatus = statusUntil > 0.0;
        g.setColour (showStatus ? Colours::copperDeep : Colours::inkSoft);
        g.setFont (Fonts::serif (16.0f, showStatus));
        g.drawText (showStatus ? statusText : loopDetails, b, juce::Justification::topLeft, true);
    }
}

void GrooveLabEditor::paintOverChildren (juce::Graphics& g)
{
    if (! fileDragActive)
        return;
    auto r = getLocalBounds().toFloat().reduced (12.0f);
    g.setColour (Colours::paperLight.withAlpha (0.88f));
    g.fillRoundedRectangle (r, 18.0f);
    const float dashes[] = { 8.0f, 6.0f };
    juce::Path border, dashed;
    border.addRoundedRectangle (r.reduced (6.0f), 16.0f);
    juce::PathStrokeType (2.0f).createDashedStroke (dashed, border, dashes, 2);
    g.setColour (Colours::copper);
    g.fillPath (dashed);
    g.strokePath (decor::lotus (juce::Rectangle<float> (120.0f, 90.0f).withCentre (r.getCentre().translated (0.0f, -60.0f))), juce::PathStrokeType (1.6f));
    g.setColour (Colours::ink);
    g.setFont (Fonts::serif (34.0f));
    g.drawText ("Drop a single note", r.withTrimmedTop (r.getHeight() * 0.5f).removeFromTop (44.0f), juce::Justification::centred);
    g.setColour (Colours::inkSoft);
    g.setFont (Fonts::serif (19.0f));
    g.drawText ("Your Sample will play the melodies with it: bends, shakes, falls and all.",
                r.withTrimmedTop (r.getHeight() * 0.5f + 46.0f).removeFromTop (30.0f), juce::Justification::centred);
}

bool GrooveLabEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac"))
            return true;
    return false;
}

void GrooveLabEditor::filesDropped (const juce::StringArray& files, int, int)
{
    fileDragActive = false;
    repaint();
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac"))
        {
            loadVoice (juce::File (f));
            return;
        }
}

void GrooveLabEditor::loadVoice (const juce::File& f)
{
    const auto res = processor.loadUserVoice (f);
    setStatus (res.wasOk() ? processor.statusMessage : res.getErrorMessage());
    for (auto* r : rows) r->repaint();
}

void GrooveLabEditor::chooseVoiceSample()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a single-note sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f.existsAsFile())
            loadVoice (f);
    });
}

void GrooveLabEditor::showVoicePanel()
{
    auto panel = std::make_unique<VoicePanel> (processor, [this] { chooseVoiceSample(); });
    panel->setLookAndFeel (&lnf);
    juce::Rectangle<int> anchor;
    for (auto* r : rows)
        if (r->getInstrument() == InstrumentId::CustomVoice)
            anchor = getLocalArea (r, r->getLocalBounds());
    juce::CallOutBox::launchAsynchronously (std::move (panel), anchor, this);
}

void GrooveLabEditor::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && (styleMenuArea.contains (e.getPosition()) || styleTitleArea.contains (e.getPosition())))
        showStyleMenu();
}

void GrooveLabEditor::mouseMove (const juce::MouseEvent& e)
{
    const bool overStyle = styleMenuArea.contains (e.getPosition()) || styleTitleArea.contains (e.getPosition());
    setMouseCursor (overStyle ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void GrooveLabEditor::resized()
{
    auto r = getLocalBounds();

    // ---- top bar
    topBar = r.removeFromTop (88);
    {
        auto row = topBar.withTrimmedLeft (270).withTrimmedRight (22).withTrimmedTop (36).withHeight (40);
        auto tempo = row.removeFromLeft (150);
        auto arrows = tempo.removeFromRight (24);
        bpmSlider.setBounds (tempo);
        bpmUp.setBounds (arrows.removeFromTop (20));
        bpmDown.setBounds (arrows);
        row.removeFromLeft (18);
        keyBox.setBounds (row.removeFromLeft (76));
        row.removeFromLeft (20);
        scaleBox.setBounds (row.removeFromLeft (186));
        row.removeFromLeft (26);
        auto bars = row.removeFromLeft (156).reduced (4, 2);
        for (auto& b : barButtons)
            b.setBounds (bars.removeFromLeft (37));
        row.removeFromLeft (24);
        menuButton.setBounds (row.removeFromRight (42));
        row.removeFromRight (10);
        presetBox.setBounds (row.removeFromRight (164));
        row.removeFromRight (20);
        levelSlider.setBounds (row);
    }

    r.reduce (22, 0);

    // ---- bottom bar and waveform strip
    bottomBar = r.removeFromBottom (72).withTrimmedBottom (8);
    {
        auto b = bottomBar;
        playButton.setBounds (b.removeFromLeft (58).withSizeKeepingCentre (54, 54));
        utilityButton.setBounds (b.removeFromRight (44).withSizeKeepingCentre (44, 40));
        b.removeFromRight (10);
        folderButton.setBounds (b.removeFromRight (150).withSizeKeepingCentre (150, 40));
        b.removeFromRight (10);
        rateBox.setBounds (b.removeFromRight (140).withSizeKeepingCentre (140, 40));
        b.removeFromRight (10);
        depthBox.setBounds (b.removeFromRight (124).withSizeKeepingCentre (124, 40));
        b.removeFromRight (10);
        exportButton.setBounds (b.removeFromRight (150).withSizeKeepingCentre (150, 40));
    }
    waveform.setBounds (r.removeFromBottom (100));
    r.removeFromBottom (12);
    r.removeFromTop (6);

    // ---- three columns
    ensembleArea = r.removeFromLeft (370);
    r.removeFromLeft (16);
    createArea = r.removeFromRight (300);
    r.removeFromRight (16);
    grooveArea = r;

    {
        auto e = ensembleArea.reduced (14, 12);
        auto header = e.removeFromTop (34);
        favTab.setBounds (header.removeFromRight (52));
        allTab.setBounds (header.removeFromRight (56));
        header.removeFromRight (6);
        searchButton.setBounds (header.removeFromRight (30).withSizeKeepingCentre (26, 26));
        searchBox.setBounds (header.withTrimmedLeft (8).withSizeKeepingCentre (header.getWidth() - 12, 28));
        e.removeFromTop (8);
        rowViewport.setBounds (e);
        layoutRows();
    }

    {
        auto gr = grooveArea.reduced (18, 0);
        const int colW = 104;
        auto left = gr.removeFromLeft (colW);
        auto right = gr.removeFromRight (colW);

        auto knobColumn = [] (juce::Rectangle<int> col, juce::OwnedArray<Knob>& knobs)
        {
            col.removeFromTop (80);
            const int h = juce::jmin (118, (col.getHeight() - 60) / knobs.size());
            for (auto* k : knobs)
                k->setBounds (col.removeFromTop (h).withSizeKeepingCentre (96, h - 4));
            return col;
        };
        knobColumn (left, leftKnobs);
        auto rightRest = knobColumn (right, rightKnobs);
        delayTimeBox.setBounds (rightRest.removeFromTop (36).withSizeKeepingCentre (110, 32));

        auto centre = gr.reduced (8, 0);
        centre.removeFromTop (58);
        styleTitleArea = centre.removeFromTop (54).withSizeKeepingCentre (centre.getWidth() + 120, 54);
        styleMenuArea = centre.removeFromTop (24).withSizeKeepingCentre (300, 24);
        centre.removeFromTop (10);
        auto sound = centre.removeFromBottom (122);
        centre.removeFromBottom (18);
        const int dial = juce::jmin (centre.getWidth(), centre.getHeight());
        styleDial.setBounds (centre.withSizeKeepingCentre (dial, dial));

        sound = sound.withSizeKeepingCentre (juce::jmin (sound.getWidth() + 40, 5 * 76), sound.getHeight());
        const int kw = sound.getWidth() / juce::jmax (1, soundKnobs.size());
        for (auto* k : soundKnobs)
            k->setBounds (sound.removeFromLeft (kw).reduced (2, 6));
    }

    {
        auto c = createArea.reduced (20, 16);
        c.removeFromTop (26);
        generateButton.setBounds (c.removeFromTop (250));
        c.removeFromTop (6);
        auto pair = c.removeFromTop (40);
        newRhythmButton.setBounds (pair.removeFromLeft (pair.getWidth() / 2 - 5));
        pair.removeFromLeft (10);
        newMelodyButton.setBounds (pair);
        c.removeFromTop (12);
        auto hist = c.removeFromTop (36).withSizeKeepingCentre (200, 36);
        backButton.setBounds (hist.removeFromLeft (94));
        forwardButton.setBounds (hist.removeFromRight (94));
        c.removeFromTop (12);
        copyCodeButton.setBounds (c.removeFromTop (38).withSizeKeepingCentre (200, 38));
        c.removeFromTop (8);
        pasteCodeButton.setBounds (c.removeFromTop (38).withSizeKeepingCentre (200, 38));
    }
}

} // namespace jgl

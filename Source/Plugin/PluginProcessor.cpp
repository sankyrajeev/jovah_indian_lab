#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "../Core/Rng.h"
#include "../Core/Scales.h"
#include "../Core/Styles.h"
#include "../DSP/LoopRenderer.h"
#include "../IO/AppPaths.h"
#include "../IO/PresetManager.h"

namespace jgl
{

namespace
{
    std::vector<juce::String> allParameterIds()
    {
        using namespace ParamIDs;
        std::vector<juce::String> ids { ParamIDs::bpm, ParamIDs::key, ParamIDs::scale, ParamIDs::bars, ParamIDs::level,
                                        ParamIDs::style, ParamIDs::framework, complexity, swing, groove, humanize,
                                        density, energy, variation, ghost, accent, saturation, reverb, delay,
                                        delayTime, filter, voiceBend, voiceShake, voiceFalls, voiceStutter,
                                        voiceOctave, voiceVibrato };
        for (auto id : allInstruments())
            ids.push_back (ParamIDs::instrument (id));
        return ids;
    }

    void installLogger()
    {
       #if JGL_UNIT_TESTS
        return;   // keep test output on stdout
       #endif
        static std::unique_ptr<juce::FileLogger> logger;
        if (logger == nullptr && juce::Logger::getCurrentLogger() == nullptr)
        {
            logger.reset (new juce::FileLogger (AppPaths::logFile(), "Jovah Groove Lab log", 512 * 1024));
            juce::Logger::setCurrentLogger (logger.get());
        }
    }

    const juce::Identifier extraStateType ("JGL");
}

//==============================================================================
GrooveLabProcessor::GrooveLabProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    installLogger();
    juce::Logger::writeToLog ("Jovah Groove Lab " JGL_VERSION_STRING " starting ("
                              + juce::String (getWrapperTypeDescription (wrapperType)) + ")");

    for (auto& id : allParameterIds())
        apvts.addParameterListener (id, this);

    seeds = SeedSet::fromMaster (freshSeed());
    pushHistory();

    service.rescanSamples (AppPaths::samplesFolder());
    startTimerHz (30);
}

GrooveLabProcessor::~GrooveLabProcessor()
{
    stopTimer();
    for (auto& id : allParameterIds())
        apvts.removeParameterListener (id, this);
}

//==============================================================================
void GrooveLabProcessor::prepareToPlay (double sampleRate, int)
{
    preparedSampleRate.store (sampleRate > 0.0 ? sampleRate : 48000.0);
    player.prepare (sampleRate);
}

void GrooveLabProcessor::releaseResources() {}

bool GrooveLabProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void GrooveLabProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // MIDI: any note starts the loop from bar 1, releasing all notes stops it.
    bool midiStart = false, midiStop = false;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            ++heldNotes;
            midiStart = true;
        }
        else if (m.isNoteOff())
        {
            heldNotes = std::max (0, heldNotes - 1);
            if (heldNotes == 0) midiStop = true;
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            heldNotes = 0;
            midiStop = true;
        }
    }
    midi.clear();

    LoopPlayer::HostPosition host;
    if (auto* ph = getPlayHead())
    {
        if (const auto pos = ph->getPosition())
        {
            if (const auto ppq = pos->getPpqPosition())
            {
                host.valid = true;
                host.ppq = *ppq;
                host.playing = pos->getIsPlaying();
            }
            if (const auto bpm = pos->getBpm())
                hostBpm.store (*bpm);
        }
    }

    player.process (buffer, host, midiStart, midiStop);
}

//==============================================================================
void GrooveLabProcessor::parameterChanged (const juce::String&, float)
{
    // May be called from any thread (automation): just flag it.
    paramsDirty.store (true);
}

float GrooveLabProcessor::param (const char* id) const
{
    if (auto* p = apvts.getRawParameterValue (id))
        return p->load();
    jassertfalse;
    return 0.0f;
}

int GrooveLabProcessor::choice (const char* id) const { return juce::roundToInt (param (id)); }

ComposeSettings GrooveLabProcessor::composeSettingsFromParams() const
{
    using namespace ParamIDs;
    ComposeSettings cs;
    cs.bars = barsForIndex (choice (ParamIDs::bars));
    cs.scaleIndex = choice (ParamIDs::scale);
    cs.styleIndex = choice (ParamIDs::style);
    cs.frameworkIndex = choice (ParamIDs::framework);
    cs.complexity = param (complexity);
    cs.density = param (density);
    cs.energy = param (energy);
    cs.variation = param (variation);
    cs.voiceBend = param (voiceBend);
    cs.voiceShake = param (voiceShake);
    cs.voiceFalls = param (voiceFalls);
    cs.voiceStutter = param (voiceStutter);
    cs.voiceOctave = param (voiceOctave);
    for (auto id : allInstruments())
        cs.enabled[(size_t) toIndex (id)] = param (ParamIDs::instrument (id).toRawUTF8()) > 0.5f;
    return cs;
}

RenderSettings GrooveLabProcessor::renderSettingsFromParams() const
{
    using namespace ParamIDs;
    RenderSettings rs;
    rs.bpm = param (ParamIDs::bpm);
    rs.keyRoot = choice (ParamIDs::key);
    rs.swing = param (swing);
    rs.groove = param (groove);
    rs.humanize = param (humanize);
    rs.ghost = param (ghost);
    rs.accent = param (accent);
    rs.saturation = param (saturation);
    rs.reverb = param (reverb);
    rs.delay = param (delay);
    rs.delayTime = choice (delayTime);
    rs.filter = param (filter);
    rs.levelDb = param (ParamIDs::level);
    rs.voiceVibrato = param (voiceVibrato);
    rs.sampleRate = preparedSampleRate.load();
    return rs;
}

void GrooveLabProcessor::requestRender()
{
    GenerationService::Request req;
    req.compose = composeSettingsFromParams();
    req.render = renderSettingsFromParams();
    {
        std::lock_guard<std::mutex> g (seedLock);
        req.seeds = seeds;
    }
    req.userVoice = userVoice;
    req.id = nextRequestId++;
    lastRequestedId = req.id;
    renderPending = true;
    service.submit (req);
}

void GrooveLabProcessor::timerCallback()
{
    player.collectGarbage();

    // Follow the host tempo (plugin builds only).
    if (followHostTempo && ! isStandalone())
    {
        const double hb = hostBpm.load();
        if (hb >= 70.0 && hb <= 160.0 && std::abs (hb - param (ParamIDs::bpm)) > 0.05)
            if (auto* p = apvts.getParameter (ParamIDs::bpm))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) hb));
    }

    // Device sample rate changed: re-render at the new rate (the player resamples meanwhile).
    if (currentResult != nullptr && std::abs (currentResult->render.sampleRate - preparedSampleRate.load()) > 0.5
        && ! renderPending)
        paramsDirty.store (true);

    if (stateRestored.exchange (false))
        paramsDirty.store (true);

    const double now = juce::Time::getMillisecondCounterHiRes();
    if (paramsDirty.load())
    {
        if (lastChangeTime <= 0.0)
            lastChangeTime = now;
        if (now - lastChangeTime > 90.0)     // debounce knob drags
        {
            paramsDirty.store (false);
            lastChangeTime = 0.0;
            requestRender();
        }
    }

    if (auto r = service.takeResult())
    {
        currentResult = r;
        renderPending = r->id != lastRequestedId;

        auto pl = std::make_unique<PlaybackLoop>();
        pl->audio.makeCopyOf (r->audio);
        pl->sampleRate = r->render.sampleRate;
        pl->lengthBeats = r->lengthBeats();
        pl->id = r->id;
        player.publish (std::move (pl));

        sendChangeMessage();
    }
}

//==============================================================================
uint64_t GrooveLabProcessor::freshSeed()
{
    static uint64_t counter = 0;
    return hashCombine ((uint64_t) juce::Time::getHighResolutionTicks(),
                        (uint64_t) juce::Random::getSystemRandom().nextInt64() ^ (++counter * 0x9E3779B97F4A7C15ull));
}

void GrooveLabProcessor::pushHistory()
{
    // seedLock must be held by the caller, or construction time.
    if (historyIndex >= 0 && historyIndex < (int) history.size() && history[(size_t) historyIndex] == seeds)
        return;
    history.resize ((size_t) (historyIndex + 1));
    history.push_back (seeds);
    if (history.size() > 64)
        history.erase (history.begin());
    historyIndex = (int) history.size() - 1;
}

void GrooveLabProcessor::generateNew()
{
    {
        std::lock_guard<std::mutex> g (seedLock);
        auto next = SeedSet::fromMaster (freshSeed());
        for (int i = 0; i < numInstruments; ++i)
            if (locks[(size_t) i])
                next.partSeeds[(size_t) i] = seeds.partSeeds[(size_t) i];
        seeds = next;
        pushHistory();
    }
    statusMessage = "New loop";
    requestRender();
}

void GrooveLabProcessor::regeneratePart (InstrumentId id)
{
    {
        std::lock_guard<std::mutex> g (seedLock);
        seeds.partSeeds[(size_t) toIndex (id)] = freshSeed();
        pushHistory();
    }
    statusMessage = "New " + juce::String (instrumentInfo (id).name) + " part";
    requestRender();
}

void GrooveLabProcessor::regenerateFamily (bool percussion)
{
    {
        std::lock_guard<std::mutex> g (seedLock);
        for (auto id : allInstruments())
            if (isPercussion (id) == percussion && ! locks[(size_t) toIndex (id)])
                seeds.partSeeds[(size_t) toIndex (id)] = freshSeed();
        pushHistory();
    }
    statusMessage = percussion ? "New rhythm, melody kept" : "New melody, rhythm kept";
    requestRender();
}

void GrooveLabProcessor::setLocked (InstrumentId id, bool locked)
{
    std::lock_guard<std::mutex> g (seedLock);
    locks[(size_t) toIndex (id)] = locked;
}

bool GrooveLabProcessor::isLocked (InstrumentId id) const
{
    std::lock_guard<std::mutex> g (seedLock);
    return locks[(size_t) toIndex (id)];
}

void GrooveLabProcessor::setAllInstruments (bool enabled)
{
    for (auto id : allInstruments())
        if (auto* p = apvts.getParameter (ParamIDs::instrument (id)))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (enabled ? 1.0f : 0.0f);
            p->endChangeGesture();
        }
}

bool GrooveLabProcessor::canGoBack() const    { std::lock_guard<std::mutex> g (seedLock); return historyIndex > 0; }
bool GrooveLabProcessor::canGoForward() const { std::lock_guard<std::mutex> g (seedLock); return historyIndex + 1 < (int) history.size(); }

void GrooveLabProcessor::historyBack()
{
    {
        std::lock_guard<std::mutex> g (seedLock);
        if (historyIndex <= 0) return;
        seeds = history[(size_t) --historyIndex];
    }
    statusMessage = "Previous loop";
    requestRender();
}

void GrooveLabProcessor::historyForward()
{
    {
        std::lock_guard<std::mutex> g (seedLock);
        if (historyIndex + 1 >= (int) history.size()) return;
        seeds = history[(size_t) ++historyIndex];
    }
    statusMessage = "Next loop";
    requestRender();
}

//==============================================================================
void GrooveLabProcessor::applyStyleDefaults (int styleIndex)
{
    using namespace ParamIDs;
    const auto& s = styleProfile (styleIndex);
    const std::pair<const char*, float> values[] = {
        { complexity, s.complexity }, { swing, s.swing }, { groove, s.groove }, { humanize, s.humanize },
        { density, s.density }, { energy, s.energy }, { variation, s.variation }, { ghost, s.ghost },
        { accent, s.accent }, { reverb, s.reverb }, { delay, s.delay }, { saturation, s.saturation } };

    for (auto& [id, v] : values)
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            p->endChangeGesture();
        }
}

void GrooveLabProcessor::applyPreset (const juce::ValueTree& preset)
{
    for (auto& id : allParameterIds())
    {
        float v = 0.0f;
        if (PresetManager::getValue (preset, id, v))
            if (auto* p = apvts.getParameter (id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 (v));
                p->endChangeGesture();
            }
    }

    SeedSet s;
    if (SeedSet::fromString (preset.getProperty ("seeds").toString(), s))
    {
        std::lock_guard<std::mutex> g (seedLock);
        seeds = s;
        pushHistory();
    }

    statusMessage = "Loaded " + preset.getProperty ("name").toString();
    paramsDirty.store (true);
}

juce::ValueTree GrooveLabProcessor::capturePreset (const juce::String& name) const
{
    juce::ValueTree p (PresetManager::presetType);
    p.setProperty ("name", name, nullptr);
    for (auto& id : allParameterIds())
        if (auto* prm = apvts.getParameter (id))
            PresetManager::setValue (p, id, prm->convertFrom0to1 (prm->getValue()));

    std::lock_guard<std::mutex> g (seedLock);
    p.setProperty ("seeds", seeds.toString(), nullptr);
    return p;
}

juce::String GrooveLabProcessor::getLoopCode() const
{
    auto xml = capturePreset ("Loop code").createXml();
    if (xml == nullptr) return {};

    juce::MemoryOutputStream raw;
    {
        juce::GZIPCompressorOutputStream gz (raw, 9);
        gz << xml->toString (juce::XmlElement::TextFormat().singleLine().withoutHeader());
    }
    return "JGL1:" + juce::Base64::toBase64 (raw.getData(), raw.getDataSize());
}

bool GrooveLabProcessor::applyLoopCode (const juce::String& code)
{
    auto text = code.trim();
    if (! text.startsWith ("JGL1:"))
        return false;

    juce::MemoryOutputStream decoded;
    if (! juce::Base64::convertFromBase64 (decoded, text.substring (5)))
        return false;

    juce::MemoryInputStream in (decoded.getData(), decoded.getDataSize(), false);
    juce::GZIPDecompressorInputStream gz (in);
    auto xml = juce::parseXML (gz.readEntireStreamAsString());
    if (xml == nullptr)
        return false;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.hasType (PresetManager::presetType))
        return false;

    applyPreset (tree);
    statusMessage = "Loop code applied";
    return true;
}

//==============================================================================
juce::String GrooveLabProcessor::getLoopTitle() const
{
    auto r = currentResult;
    if (r == nullptr) return "Generating...";
    if (r->arrangement.parts.empty()) return "No instruments selected";

    juce::String t = styleProfile (r->compose.styleIndex).name;
    if (r->hasMelodic())
        t << "  in  " << keyNames()[r->render.keyRoot] << " " << scaleProfile (r->compose.scaleIndex).name;
    else
        t << "  |  percussion only";
    return t;
}

juce::String GrooveLabProcessor::getLoopDetails() const
{
    auto r = currentResult;
    if (r == nullptr) return {};

    juce::StringArray names;
    for (auto& p : r->arrangement.parts)
        names.add (instrumentInfo (p.instrument).name);

    const int bars = r->arrangement.bars;
    juce::String d = names.joinIntoString (" + ");
    d << "   |   " << juce::String (r->render.bpm, std::abs (r->render.bpm - std::round (r->render.bpm)) < 0.05 ? 0 : 1) << " BPM"
      << "   |   " << bars << (bars == 1 ? " bar" : " bars")
      << "   |   " << framework ((FrameworkId) r->arrangement.frameworkIndex).name
      << "   |   seed " << juce::String::toHexString ((juce::int64) r->seeds.masterSeed).substring (0, 8).toUpperCase();
    return d;
}

LoopFileInfo GrooveLabProcessor::getFileInfo() const
{
    LoopFileInfo info;
    if (auto r = currentResult)
    {
        info.styleTag = styleProfile (r->compose.styleIndex).fileTag;
        info.bpm = r->render.bpm;
        info.bars = r->arrangement.bars;
        info.keyLabel = r->hasMelodic() ? keyLabel (r->render.keyRoot, r->compose.scaleIndex) : juce::String();
    }
    return info;
}

juce::Result GrooveLabProcessor::exportCurrentLoop (juce::File& writtenFile)
{
    auto r = currentResult;
    if (r == nullptr || r->audio.getNumSamples() == 0)
        return juce::Result::fail ("Nothing to export yet");
    if (r->arrangement.parts.empty())
        return juce::Result::fail ("Select at least one instrument");

    const double sr = exportSampleRate > 0.0 ? exportSampleRate : r->render.sampleRate;
    const juce::AudioBuffer<float>* audio = &r->audio;
    std::shared_ptr<LoopResult> rerendered;

    if (std::abs (sr - r->render.sampleRate) > 0.5)
    {
        GenerationService::Request q;
        q.compose = r->compose;
        q.render = r->render;
        q.render.sampleRate = sr;
        q.seeds = r->seeds;
        q.userVoice = r->userVoice;
        auto lib = service.getSamples();
        rerendered = GenerationService::renderNow (q, lib.get(), nullptr, &r->arrangement);
        if (rerendered == nullptr)
            return juce::Result::fail ("Rendering at " + juce::String (sr) + " Hz failed");
        audio = &rerendered->audio;
    }

    writtenFile = WavExporter::uniqueFile (AppPaths::exportsFolder(), WavExporter::makeFileName (getFileInfo()));
    const int rootMidi = r->hasMelodic() ? (int) LoopRenderer::tonicMidi (InstrumentId::Sitar, r->render.keyRoot) : -1;
    auto res = WavExporter::write (*audio, sr, exportDepth, writtenFile, r->render.bpm, r->arrangement.bars * 4, rootMidi);

    juce::Logger::writeToLog ("JGL: export " + writtenFile.getFullPathName() + " -> "
                              + (res.wasOk() ? juce::String ("ok") : res.getErrorMessage()));
    statusMessage = res.wasOk() ? "Exported " + writtenFile.getFileName() : res.getErrorMessage();
    return res;
}

juce::Result GrooveLabProcessor::prepareDragFile (juce::File& file)
{
    auto r = currentResult;
    if (r == nullptr)
        return juce::Result::fail ("Nothing to drag yet");

    const auto key = juce::String ((juce::int64) r->id) + "_" + juce::String ((int) exportDepth) + "_" + juce::String (exportSampleRate);
    auto it = dragFileCache.find (key);
    if (it != dragFileCache.end() && it->second.existsAsFile())
    {
        file = it->second;
        return juce::Result::ok();
    }

    auto res = exportCurrentLoop (file);
    if (res.wasOk())
        dragFileCache[key] = file;
    return res;
}

//==============================================================================
juce::Result GrooveLabProcessor::loadUserVoice (const juce::File& file)
{
    juce::String err;
    auto voice = SampleLibrary::loadVoice (file, err);
    if (voice == nullptr)
        return juce::Result::fail (err);

    // Keep a private copy so sessions survive the original being moved or deleted.
    auto stored = file;
    if (! file.isAChildOf (AppPaths::voicesFolder()))
    {
        stored = WavExporter::uniqueFile (AppPaths::voicesFolder(), file.getFileName());
        if (! file.copyFileTo (stored))
            stored = file;
    }

    voice->name = file.getFileNameWithoutExtension();
    userVoice = voice;
    userVoiceFile = stored;

    if (auto* p = apvts.getParameter (ParamIDs::instrument (InstrumentId::CustomVoice)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (1.0f);
        p->endChangeGesture();
    }

    juce::Logger::writeToLog ("JGL: loaded voice " + file.getFullPathName() + " root " + juce::String (voice->rootMidi, 2)
                              + (voice->loopStart >= 0 ? " with sustain loop" : " without loop"));
    statusMessage = "Loaded " + file.getFileName() + ": " + describeUserVoice();
    paramsDirty.store (true);
    return juce::Result::ok();
}

juce::String GrooveLabProcessor::describeUserVoice() const
{
    auto v = userVoice;
    if (v == nullptr)
        return "No sample loaded";
    const int nearest = (int) std::lround (v->rootMidi);
    const int cents = (int) std::lround ((v->rootMidi - nearest) * 100.0);
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    juce::String note = juce::String (names[((nearest % 12) + 12) % 12]) + juce::String (nearest / 12 - 1);
    if (cents != 0) note << (cents > 0 ? " +" : " ") << cents << " cents";
    return note + (v->loopStart >= 0 ? ", looped sustain" : "");
}

//==============================================================================
void GrooveLabProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto old = state.getChildWithName (extraStateType);
    if (old.isValid())
        state.removeChild (old, nullptr);

    juce::ValueTree extra (extraStateType);
    {
        std::lock_guard<std::mutex> g (seedLock);
        extra.setProperty ("seeds", seeds.toString(), nullptr);
        juce::String lockString;
        for (auto l : locks) lockString << (l ? "1" : "0");
        extra.setProperty ("locks", lockString, nullptr);
    }
    extra.setProperty ("exportDepth", (int) exportDepth, nullptr);
    extra.setProperty ("exportRate", exportSampleRate, nullptr);
    extra.setProperty ("followHostTempo", followHostTempo, nullptr);
    extra.setProperty ("syncHost", player.isSyncedToHost(), nullptr);
    extra.setProperty ("voiceFile", userVoiceFile.getFullPathName(), nullptr);
    state.appendChild (extra, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void GrooveLabProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.hasType (apvts.state.getType()))
        return;

    auto extra = tree.getChildWithName (extraStateType);
    if (extra.isValid())
    {
        tree.removeChild (extra, nullptr);

        SeedSet s;
        std::lock_guard<std::mutex> g (seedLock);
        if (SeedSet::fromString (extra.getProperty ("seeds").toString(), s))
        {
            seeds = s;
            pushHistory();
        }
        const auto lockString = extra.getProperty ("locks").toString();
        for (int i = 0; i < numInstruments && i < lockString.length(); ++i)
            locks[(size_t) i] = lockString[i] == '1';

        exportDepth = (WavBitDepth) juce::jlimit (0, 2, (int) extra.getProperty ("exportDepth", 0));
        exportSampleRate = (double) extra.getProperty ("exportRate", 0.0);
        followHostTempo = (bool) extra.getProperty ("followHostTempo", false);
        player.setSyncToHost ((bool) extra.getProperty ("syncHost", true));

        const auto voicePath = extra.getProperty ("voiceFile").toString();
        if (voicePath.isNotEmpty())
        {
            juce::String err;
            const juce::File vf (voicePath);
            if (auto v = SampleLibrary::loadVoice (vf, err))
            {
                userVoice = v;
                userVoiceFile = vf;
            }
            else
                juce::Logger::writeToLog ("JGL: could not restore voice sample: " + err);
        }
    }

    apvts.replaceState (tree);
    stateRestored.store (true);
}

juce::AudioProcessorEditor* GrooveLabProcessor::createEditor()
{
    return new GrooveLabEditor (*this);
}

} // namespace jgl

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new jgl::GrooveLabProcessor();
}

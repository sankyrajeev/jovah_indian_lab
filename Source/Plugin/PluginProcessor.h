#pragma once

#include <map>
#include <mutex>

#include <juce_audio_processors/juce_audio_processors.h>

#include "../Engine/GenerationService.h"
#include "../Engine/LoopPlayer.h"
#include "../IO/WavExporter.h"
#include "Parameters.h"

namespace jgl
{

/**
    Plugin processor.

    Threads
    - Audio thread: processBlock() -> LoopPlayer::process() only (lock-free, allocation-free).
    - Message thread: all user actions, state bookkeeping, a 30 Hz timer that debounces parameter
      changes into render requests and picks up finished loops.
    - Generation worker (GenerationService): composition, synthesis, effects.

    Parameter changes are signalled from any thread through atomics only; the timer turns them
    into render requests. Seeds, locks and history are message-thread state guarded by a mutex
    because hosts may call setStateInformation() from other threads.
*/
class GrooveLabProcessor : public juce::AudioProcessor,
                           public juce::ChangeBroadcaster,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::Timer
{
public:
    GrooveLabProcessor();
    ~GrooveLabProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Jovah Groove Lab"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Message-thread API used by the editor
    void generateNew();                              // new loop; locked parts keep their seeds
    void regeneratePart (InstrumentId id);           // new seed for one part
    void regenerateFamily (bool percussion);         // "New rhythm" / "New melody"
    void setLocked (InstrumentId id, bool locked);
    bool isLocked (InstrumentId id) const;
    void setAllInstruments (bool enabled);

    bool canGoBack() const;
    bool canGoForward() const;
    void historyBack();
    void historyForward();

    void applyStyleDefaults (int styleIndex);
    void applyPreset (const juce::ValueTree& preset);
    juce::ValueTree capturePreset (const juce::String& name) const;

    juce::String getLoopCode() const;
    bool applyLoopCode (const juce::String& code);

    std::shared_ptr<const LoopResult> getCurrentResult() const { return currentResult; }
    juce::String getLoopTitle() const;
    juce::String getLoopDetails() const;
    LoopFileInfo getFileInfo() const;

    /** Writes the current loop to the exports folder in the selected format. */
    juce::Result exportCurrentLoop (juce::File& writtenFile);
    /** Same as export, but re-uses the file if this exact loop was already written. */
    juce::Result prepareDragFile (juce::File& file);

    /** Loads a single-note recording into "Your Sample" (and enables it). */
    juce::Result loadUserVoice (const juce::File& file);
    std::shared_ptr<const SampleLibrary::Sample> getUserVoice() const { return userVoice; }
    juce::String describeUserVoice() const;

    LoopPlayer& getPlayer() noexcept { return player; }
    GenerationService& getService() noexcept { return service; }
    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }
    bool isRenderPending() const noexcept { return renderPending || service.isBusy(); }

    WavBitDepth exportDepth = WavBitDepth::Pcm24;
    double exportSampleRate = 0.0;        // 0 = session rate
    bool followHostTempo = false;
    bool isStandalone() const noexcept { return wrapperType == wrapperType_Standalone; }
    double getHostBpm() const noexcept { return hostBpm.load(); }

    juce::String statusMessage;           // last user-facing status (message thread)

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void timerCallback() override;

    void requestRender();
    ComposeSettings composeSettingsFromParams() const;
    RenderSettings renderSettingsFromParams() const;
    float param (const char* id) const;
    int choice (const char* id) const;
    void pushHistory();
    static uint64_t freshSeed();

    juce::AudioProcessorValueTreeState apvts;
    GenerationService service;
    LoopPlayer player;

    mutable std::mutex seedLock;
    SeedSet seeds;
    std::array<bool, (size_t) numInstruments> locks {};
    std::vector<SeedSet> history;
    int historyIndex = -1;

    std::shared_ptr<const LoopResult> currentResult;
    std::shared_ptr<const SampleLibrary::Sample> userVoice;
    juce::File userVoiceFile;
    uint64_t nextRequestId = 1;
    uint64_t lastRequestedId = 0;
    bool renderPending = false;

    std::map<juce::String, juce::File> dragFileCache;

    std::atomic<bool> paramsDirty { true }, stateRestored { false };
    std::atomic<double> preparedSampleRate { 48000.0 };
    std::atomic<double> hostBpm { 0.0 };
    double lastChangeTime = 0.0;
    int heldNotes = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrooveLabProcessor)
};

} // namespace jgl

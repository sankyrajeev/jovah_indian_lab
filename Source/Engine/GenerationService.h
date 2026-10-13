#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>

#include <juce_core/juce_core.h>

#include "../DSP/SampleLibrary.h"
#include "LoopResult.h"

namespace jgl
{

/**
    Background worker that composes and renders loops.

    - submit() is non-blocking; the newest request wins and cancels any render in flight.
    - When only render settings changed (swing, FX, key, BPM ...), the cached arrangement is
      re-used so the groove stays identical while the sound updates.
    - takeResult() is polled by the message thread.
    - The user sample library is (re)loaded on this thread too.
*/
class GenerationService : private juce::Thread
{
public:
    struct Request
    {
        ComposeSettings compose;
        RenderSettings render;
        SeedSet seeds;
        uint64_t id = 0;
    };

    GenerationService();
    ~GenerationService() override;

    void submit (const Request& request);
    std::shared_ptr<const LoopResult> takeResult();

    void rescanSamples (const juce::File& root);
    std::shared_ptr<const SampleLibrary> getSamples() const;

    bool isBusy() const noexcept { return busy.load(); }

    /** Synchronous compose + render (export at another sample rate, tests, CLI). */
    static std::shared_ptr<LoopResult> renderNow (const Request& request, const SampleLibrary* samples,
                                                  const std::atomic<bool>* cancel = nullptr,
                                                  const Arrangement* reuseArrangement = nullptr);

private:
    void run() override;

    mutable std::mutex lock;
    std::optional<Request> pending;
    std::optional<juce::File> pendingSampleRoot;
    std::shared_ptr<const LoopResult> ready;
    std::shared_ptr<const SampleLibrary> samples;

    // Worker-thread cache
    std::optional<ComposeSettings> cachedCompose;
    SeedSet cachedSeeds;
    std::shared_ptr<Arrangement> cachedArrangement;

    std::atomic<bool> cancelFlag { false }, busy { false };
    juce::WaitableEvent wake;
};

} // namespace jgl

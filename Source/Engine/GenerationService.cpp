#include "GenerationService.h"

#include "../DSP/LoopRenderer.h"
#include "../Generation/Arranger.h"

namespace jgl
{

GenerationService::GenerationService() : juce::Thread ("JGL Generator")
{
    startThread (juce::Thread::Priority::normal);
}

GenerationService::~GenerationService()
{
    cancelFlag.store (true);
    signalThreadShouldExit();
    wake.signal();
    stopThread (10000);
}

void GenerationService::submit (const Request& request)
{
    {
        std::lock_guard<std::mutex> g (lock);
        pending = request;
        cancelFlag.store (true);   // abandon an in-flight render; the newest request wins
    }
    wake.signal();
}

std::shared_ptr<const LoopResult> GenerationService::takeResult()
{
    std::lock_guard<std::mutex> g (lock);
    auto r = ready;
    ready.reset();
    return r;
}

void GenerationService::rescanSamples (const juce::File& root)
{
    {
        std::lock_guard<std::mutex> g (lock);
        pendingSampleRoot = root;
    }
    wake.signal();
}

std::shared_ptr<const SampleLibrary> GenerationService::getSamples() const
{
    std::lock_guard<std::mutex> g (lock);
    return samples;
}

std::shared_ptr<LoopResult> GenerationService::renderNow (const Request& request, const SampleLibrary* lib,
                                                          const std::atomic<bool>* cancel,
                                                          const Arrangement* reuse)
{
    const auto start = juce::Time::getMillisecondCounterHiRes();

    auto result = std::make_shared<LoopResult>();
    result->id = request.id;
    result->compose = request.compose;
    result->render = request.render;
    result->seeds = request.seeds;
    result->userVoice = request.userVoice;
    result->arrangement = reuse != nullptr ? *reuse : Arranger::compose (request.compose, request.seeds);

    LoopRenderer::Options opt;
    opt.samples = lib;
    opt.userVoice = request.userVoice.get();
    opt.cancel = cancel;

    if (! LoopRenderer::render (result->arrangement, request.render, result->audio, opt))
        return nullptr;

    result->computePeaks (1200);
    result->renderMilliseconds = juce::Time::getMillisecondCounterHiRes() - start;
    return result;
}

void GenerationService::run()
{
    while (! threadShouldExit())
    {
        wake.wait (500);
        if (threadShouldExit())
            break;

        std::optional<juce::File> sampleRoot;
        std::optional<Request> req;
        {
            std::lock_guard<std::mutex> g (lock);
            std::swap (sampleRoot, pendingSampleRoot);
            std::swap (req, pending);
            cancelFlag.store (false);
        }

        if (sampleRoot.has_value())
        {
            auto lib = std::make_shared<SampleLibrary>();
            const int n = lib->load (*sampleRoot);
            juce::Logger::writeToLog ("JGL: loaded " + juce::String (n) + " user samples from " + sampleRoot->getFullPathName());
            std::lock_guard<std::mutex> g (lock);
            samples = lib;
        }

        if (! req.has_value())
            continue;

        busy.store (true);

        // Re-use the composition when only the sound changed.
        if (! cachedCompose.has_value() || *cachedCompose != req->compose || cachedSeeds != req->seeds)
        {
            cachedArrangement = std::make_shared<Arrangement> (Arranger::compose (req->compose, req->seeds));
            cachedCompose = req->compose;
            cachedSeeds = req->seeds;
        }

        std::shared_ptr<const SampleLibrary> lib;
        {
            std::lock_guard<std::mutex> g (lock);
            lib = samples;
        }

        auto result = renderNow (*req, lib.get(), &cancelFlag, cachedArrangement.get());

        busy.store (false);

        if (result != nullptr)
        {
            juce::Logger::writeToLog ("JGL: rendered loop " + juce::String ((juce::int64) result->id) + " in "
                                      + juce::String (result->renderMilliseconds, 1) + " ms ("
                                      + juce::String (result->arrangement.totalEvents()) + " events)");
            std::lock_guard<std::mutex> g (lock);
            ready = result;
        }
        else
        {
            // Cancelled by a newer request: make sure we loop round to pick it up.
            wake.signal();
        }
    }
}

} // namespace jgl

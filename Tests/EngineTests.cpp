#include "TestHelpers.h"
#include "Engine/LoopPlayer.h"
#include "Plugin/PluginProcessor.h"

namespace jgl
{

namespace
{
    std::unique_ptr<PlaybackLoop> rampLoop (int length, double sr, double beats, float scale, uint64_t id)
    {
        auto l = std::make_unique<PlaybackLoop>();
        l->audio.setSize (2, length);
        for (int i = 0; i < length; ++i)
        {
            l->audio.setSample (0, i, scale * (float) i / (float) length);
            l->audio.setSample (1, i, -scale * (float) i / (float) length);
        }
        l->sampleRate = sr;
        l->lengthBeats = beats;
        l->id = id;
        return l;
    }
}

class EngineTests : public juce::UnitTest
{
public:
    EngineTests() : juce::UnitTest ("Real-time engine & plugin", "JGL") {}

    void runTest() override
    {
        beginTest ("Player plays the loop sample-exactly and loops");
        {
            LoopPlayer player;
            player.prepare (48000.0);
            player.publish (rampLoop (1000, 48000.0, 4.0, 1.0f, 1));
            player.setPlaying (true);

            juce::AudioBuffer<float> block (2, 256);
            std::vector<float> out;
            for (int b = 0; b < 12; ++b)
            {
                player.process (block, {}, false, false);
                for (int i = 0; i < 256; ++i)
                    out.push_back (block.getSample (0, i));
            }
            // After the short fade-in, output equals the ramp, wrapping every 1000 samples.
            for (int i = 400; i < (int) out.size(); ++i)
                expectWithinAbsoluteError (out[(size_t) i], (float) (i % 1000) / 1000.0f, 1.0e-5f);
        }

        beginTest ("Player follows the host bar position");
        {
            LoopPlayer player;
            player.prepare (48000.0);
            player.publish (rampLoop (16000, 48000.0, 16.0, 1.0f, 1));
            juce::AudioBuffer<float> block (2, 64);
            LoopPlayer::HostPosition host;
            host.valid = true;
            host.playing = true;
            host.ppq = 21.0;   // 5 beats into the second cycle of a 16-beat loop
            player.process (block, host, false, false);
            expectWithinAbsoluteError (player.getPlayheadFraction(), (float) ((5.0 * 1000.0 + 64.0) / 16000.0), 1.0e-4f);
            host.playing = false;
            for (int i = 0; i < 20; ++i) player.process (block, host, false, false);
            expectEquals (player.getPlayheadFraction(), -1.0f);   // stops with the host when not latched
        }

        beginTest ("Swapping loops crossfades without clicks");
        {
            LoopPlayer player;
            player.prepare (48000.0);
            auto constant = [] (float v, uint64_t id)
            {
                auto l = std::make_unique<PlaybackLoop>();
                l->audio.setSize (2, 48000);
                for (int c = 0; c < 2; ++c) juce::FloatVectorOperations::fill (l->audio.getWritePointer (c), v, 48000);
                l->lengthBeats = 4.0; l->id = id;
                return l;
            };
            player.publish (constant (0.5f, 1));
            player.setPlaying (true);
            juce::AudioBuffer<float> block (2, 512);
            for (int i = 0; i < 4; ++i) player.process (block, {}, false, false);
            player.publish (constant (-0.5f, 2));
            float maxStep = 0.0f, prev = block.getSample (0, 511);
            for (int b = 0; b < 4; ++b)
            {
                player.process (block, {}, false, false);
                for (int i = 0; i < 512; ++i)
                {
                    maxStep = std::max (maxStep, std::abs (block.getSample (0, i) - prev));
                    prev = block.getSample (0, i);
                }
            }
            expectLessThan (maxStep, 0.01f);
            expectWithinAbsoluteError (prev, -0.5f, 1.0e-6f);
        }

        beginTest ("Lock-free hand-off under stress: nothing leaks, nothing freed on the audio thread");
        {
            const int before = PlaybackLoop::liveInstances.load();
            {
                LoopPlayer player;
                player.prepare (44100.0);
                player.setPlaying (true);
                std::atomic<bool> run { true };

                std::thread audio ([&]
                {
                    juce::AudioBuffer<float> block (2, 128);
                    while (run.load())
                        player.process (block, {}, false, false);
                });

                for (int i = 0; i < 400; ++i)
                {
                    player.publish (rampLoop (2000 + i, 48000.0, 4.0, 0.5f, (uint64_t) i + 1));
                    if (i % 3 == 0) std::this_thread::yield();
                }
                juce::Thread::sleep (50);
                run.store (false);
                audio.join();
                player.collectGarbage();
                expectLessOrEqual (PlaybackLoop::liveInstances.load() - before, 3);   // current + previous + waiting
            }
            expectEquals (PlaybackLoop::liveInstances.load(), before);
        }

        beginTest ("Generation service: newest request wins");
        {
            GenerationService service;
            GenerationService::Request req;
            req.compose = test::compose ((int) StyleId::AfroIndian, { InstrumentId::Tabla, InstrumentId::Dhol });
            req.render = test::render();
            for (uint64_t i = 1; i <= 8; ++i)
            {
                req.id = i;
                req.seeds = SeedSet::fromMaster (i);
                service.submit (req);
            }
            std::shared_ptr<const LoopResult> last;
            const auto deadline = juce::Time::getMillisecondCounter() + 20000;
            while (juce::Time::getMillisecondCounter() < deadline)
            {
                if (auto r = service.takeResult()) last = r;
                if (last != nullptr && last->id == 8) break;
                juce::Thread::sleep (10);
            }
            expect (last != nullptr && last->id == 8);
        }

        beginTest ("Plugin state round-trip restores settings, seeds and locks");
        {
            juce::MemoryBlock state;
            juce::String seedsBefore, codeBefore;
            {
                GrooveLabProcessor p;
                auto& s = p.getState();
                s.getParameter (ParamIDs::bpm)->setValueNotifyingHost (s.getParameter (ParamIDs::bpm)->convertTo0to1 (131.5f));
                s.getParameter (ParamIDs::scale)->setValueNotifyingHost (s.getParameter (ParamIDs::scale)->convertTo0to1 (9.0f));
                s.getParameter (ParamIDs::instrument (InstrumentId::Sitar))->setValueNotifyingHost (1.0f);
                p.generateNew();
                p.setLocked (InstrumentId::Tabla, true);
                p.exportDepth = WavBitDepth::Float32;
                p.getStateInformation (state);
                codeBefore = p.getLoopCode();
                seedsBefore = p.capturePreset ("x").getProperty ("seeds").toString();
            }
            {
                GrooveLabProcessor p;
                p.setStateInformation (state.getData(), (int) state.getSize());
                auto& s = p.getState();
                expectWithinAbsoluteError (s.getRawParameterValue (ParamIDs::bpm)->load(), 131.5f, 0.05f);
                expectEquals (juce::roundToInt (s.getRawParameterValue (ParamIDs::scale)->load()), 9);
                expect (s.getRawParameterValue (ParamIDs::instrument (InstrumentId::Sitar).toRawUTF8())->load() > 0.5f);
                expect (p.isLocked (InstrumentId::Tabla));
                expect (! p.isLocked (InstrumentId::Dhol));
                expect (p.exportDepth == WavBitDepth::Float32);
                expectEquals (p.capturePreset ("x").getProperty ("seeds").toString(), seedsBefore);
            }
            {
                GrooveLabProcessor p;
                expect (p.applyLoopCode (codeBefore));
                expectEquals (p.capturePreset ("x").getProperty ("seeds").toString(), seedsBefore);
                expectWithinAbsoluteError (p.getState().getRawParameterValue (ParamIDs::bpm)->load(), 131.5f, 0.05f);
                expect (! p.applyLoopCode ("not a code"));
            }
        }

        beginTest ("Plugin produces audio end-to-end and exports it");
        {
            GrooveLabProcessor p;
            p.prepareToPlay (44100.0, 512);
            const auto deadline = juce::Time::getMillisecondCounter() + 20000;
            while (p.getCurrentResult() == nullptr && juce::Time::getMillisecondCounter() < deadline)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (20);

            auto r = p.getCurrentResult();
            expect (r != nullptr);
            if (r == nullptr) return;
            expectEquals (r->render.sampleRate, 44100.0);

            p.getPlayer().setPlaying (true);
            juce::AudioBuffer<float> block (2, 512);
            juce::MidiBuffer midi;
            float peak = 0.0f;
            for (int i = 0; i < 200; ++i)
            {
                p.processBlock (block, midi);
                peak = std::max (peak, block.getMagnitude (0, 512));
            }
            expectGreaterThan (peak, 0.1f);

            juce::File f;
            expect (p.exportCurrentLoop (f).wasOk());
            expect (f.existsAsFile() && f.getFileName().startsWith ("IndianGroove_AfroIndian_124BPM_"));

            juce::File drag1, drag2;
            expect (p.prepareDragFile (drag1).wasOk());
            expect (p.prepareDragFile (drag2).wasOk());
            expect (drag1 == drag2, "repeated drags of the same loop re-use one file");

            p.exportSampleRate = 48000.0;
            juce::File f48;
            expect (p.exportCurrentLoop (f48).wasOk());
            juce::AudioFormatManager fm;
            fm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f48));
            expectEquals (reader->sampleRate, 48000.0);
            expectEquals (reader->lengthInSamples, loopLengthSamples (r->arrangement.bars, r->render.bpm, 48000.0));
            p.releaseResources();
        }
    }
};

static EngineTests engineTests;

} // namespace jgl

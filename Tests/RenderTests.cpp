#include "TestHelpers.h"
#include "DSP/SampleLibrary.h"
#include "IO/WavExporter.h"

namespace jgl
{

class RenderTests : public juce::UnitTest
{
public:
    RenderTests() : juce::UnitTest ("Audio rendering", "JGL") {}

    void runTest() override
    {
        beginTest ("Every style renders finite, normalised, unclipped audio");
        {
            for (int style = 0; style < numStyles; ++style)
            {
                auto r = test::renderLoop (test::composeAll (style, 2), test::render (styleProfile (style).defaultBpm), (uint64_t) style + 3);
                expect (r != nullptr);
                if (r == nullptr) continue;
                expect (! test::hasNonFinite (r->audio), "finite samples");
                const float peak = r->audio.getMagnitude (0, r->audio.getNumSamples());
                expectWithinAbsoluteError (juce::Decibels::gainToDecibels (peak), LoopRenderer::normaliseCeilingDb, 0.01f);
                expectGreaterThan (r->audio.getRMSLevel (0, 0, r->audio.getNumSamples()), 0.01f);
            }
        }

        beginTest ("Output level lowers the loop, never above the ceiling");
        {
            auto cs = test::compose ((int) StyleId::AfroIndian, { InstrumentId::Dhol, InstrumentId::Tabla });
            auto rs = test::render();
            rs.levelDb = -6.0f;
            auto r = test::renderLoop (cs, rs, 8);
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (r->audio.getMagnitude (0, r->audio.getNumSamples())), -7.0f, 0.02f);
            rs.levelDb = 12.0f;   // out of range values are clamped to 0 dB
            r = test::renderLoop (cs, rs, 8);
            expectLessOrEqual (r->audio.getMagnitude (0, r->audio.getNumSamples()), juce::Decibels::decibelsToGain (-0.99f));
        }

        beginTest ("No instruments -> silent loop of the right length, no crash");
        {
            auto r = test::renderLoop (test::compose ((int) StyleId::Custom, {}), test::render (120.0), 1);
            expect (r != nullptr);
            expectEquals (r->audio.getNumSamples(), 384000);
            expectEquals (r->audio.getMagnitude (0, r->audio.getNumSamples()), 0.0f);
        }

        beginTest ("Rendering is deterministic");
        {
            const auto cs = test::compose ((int) StyleId::PsychedelicIndia, { InstrumentId::Sitar, InstrumentId::Tabla, InstrumentId::Tanpura }, 2);
            auto a = test::renderLoop (cs, test::render(), 42);
            auto b = test::renderLoop (cs, test::render(), 42);
            float maxDiff = 0.0f;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < a->audio.getNumSamples(); ++i)
                    maxDiff = std::max (maxDiff, std::abs (a->audio.getSample (c, i) - b->audio.getSample (c, i)));
            expectEquals (maxDiff, 0.0f);
        }

        beginTest ("Loop seam is continuous (tails wrap around)");
        {
            for (int style : { (int) StyleId::AfroIndian, (int) StyleId::PsychedelicIndia, (int) StyleId::MelodicIndian })
            {
                auto rs = test::render();
                rs.reverb = 0.8f;
                rs.delay = 0.6f;
                auto r = test::renderLoop (test::composeAll (style, 1), rs, 17);
                const auto& b = r->audio;
                const int n = b.getNumSamples();
                for (int c = 0; c < 2; ++c)
                {
                    float maxStep = 0.0f;
                    for (int i = 1; i < n; ++i)
                        maxStep = std::max (maxStep, std::abs (b.getSample (c, i) - b.getSample (c, i - 1)));
                    const float seam = std::abs (b.getSample (c, 0) - b.getSample (c, n - 1));
                    expectLessOrEqual (seam, maxStep, "seam step must not exceed the largest step inside the loop");
                }

                // The tail region at the end of the loop is not silent (reverb / tails carried over).
                expectGreaterThan (b.getRMSLevel (0, n - 2400, 2400), 1.0e-4f);
            }
        }

        beginTest ("Recorded samples override the modelled voices");
        {
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("JGL_samples_test");
            dir.deleteRecursively();
            auto naDir = dir.getChildFile ("tabla").getChildFile ("Na");
            naDir.createDirectory();

            juce::AudioBuffer<float> click (1, 4800);
            for (int i = 0; i < 4800; ++i)
                click.setSample (0, i, 0.8f * std::sin (i * 0.2f) * std::exp (-i / 600.0f));
            juce::AudioBuffer<float> stereo (2, 4800);
            stereo.copyFrom (0, 0, click, 0, 0, 4800);
            stereo.copyFrom (1, 0, click, 0, 0, 4800);
            expect (WavExporter::write (stereo, 44100.0, WavBitDepth::Pcm24, naDir.getChildFile ("na_v1.wav")).wasOk());
            expect (WavExporter::write (stereo, 44100.0, WavBitDepth::Pcm24, naDir.getChildFile ("na_v2.wav")).wasOk());
            auto notes = dir.getChildFile ("sitar").getChildFile ("notes");
            notes.createDirectory();
            expect (WavExporter::write (stereo, 48000.0, WavBitDepth::Pcm16, notes.getChildFile ("sitar_C3.wav")).wasOk());

            SampleLibrary lib;
            expectEquals (lib.load (dir), 3);
            expect (lib.hasArticulation (InstrumentId::Tabla, TablaArt::Na));
            expect (! lib.hasArticulation (InstrumentId::Tabla, TablaArt::Ge));
            expect (lib.hasMelodicNotes (InstrumentId::Sitar));
            expectEquals (lib.numLayers (InstrumentId::Tabla, TablaArt::Na), 2);
            expect (lib.pick (InstrumentId::Tabla, TablaArt::Na, 0.9f, 1)->layer == 2);
            expect (lib.pick (InstrumentId::Tabla, TablaArt::Na, 0.1f, 1)->layer == 1);

            // Render a tabla part with and without samples: they must differ.
            auto cs = test::compose ((int) StyleId::Custom, { InstrumentId::Tabla });
            const auto a = Arranger::compose (cs, SeedSet::fromMaster (3));
            juce::AudioBuffer<float> synth, sampled;
            LoopRenderer::Options opt;
            LoopRenderer::render (a, test::render(), synth, opt);
            opt.samples = &lib;
            LoopRenderer::render (a, test::render(), sampled, opt);
            float diff = 0.0f;
            for (int i = 0; i < synth.getNumSamples(); ++i)
                diff = std::max (diff, std::abs (synth.getSample (0, i) - sampled.getSample (0, i)));
            expectGreaterThan (diff, 0.01f);
            expect (! test::hasNonFinite (sampled));

            dir.deleteRecursively();
        }

        beginTest ("Root note parsing for multi-samples");
        {
            expectEquals (SampleLibrary::parseRootNote ("sitar_C3"), 48);   // scientific pitch: C4 = 60
            expectEquals (SampleLibrary::parseRootNote ("bansuri-F#4"), 66);
            expectEquals (SampleLibrary::parseRootNote ("tumbi_Bb4_soft"), 70);
            expectEquals (SampleLibrary::parseRootNote ("note_62"), 62);
            expectEquals (SampleLibrary::parseRootNote ("nothing"), -1);
        }
    }
};

static RenderTests renderTests;

} // namespace jgl

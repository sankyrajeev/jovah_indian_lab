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

#include "IO/SampleImporter.h"

namespace jgl
{

class ImporterTests : public juce::UnitTest
{
public:
    ImporterTests() : juce::UnitTest ("Sample importer", "JGL") {}

    void runTest() override
    {
        const double sr = 44100.0;

        beginTest ("Onset detection finds each hit in a take");
        {
            juce::AudioBuffer<float> take (1, (int) (sr * 2.0));
            take.clear();
            const double times[] = { 0.1, 0.45, 0.9, 1.5 };
            const float levels[] = { 0.9f, 0.3f, 0.6f, 0.15f };
            for (int h = 0; h < 4; ++h)
                for (int i = 0; i < (int) (0.25 * sr); ++i)
                    take.addSample (0, (int) (times[h] * sr) + i, levels[h] * std::sin ((float) i * 0.07f) * std::exp (-(float) i / 2000.0f));

            const auto onsets = SampleImporter::detectOnsets (take, sr);
            expectEquals ((int) onsets.size(), 4);
            for (size_t k = 0; k < onsets.size() && k < 4; ++k)
                expectWithinAbsoluteError (onsets[k] / sr, times[k], 0.006);
        }

        beginTest ("Pitch estimation (YIN)");
        {
            for (double hz : { 110.0, 220.0, 293.66, 587.33 })
            {
                juce::AudioBuffer<float> note (1, (int) sr);
                for (int i = 0; i < note.getNumSamples(); ++i)
                {
                    double v = 0.0;
                    for (int k = 1; k <= 8; ++k) v += std::sin (juce::MathConstants<double>::twoPi * hz * k * i / sr) / k;
                    note.setSample (0, i, (float) (0.3 * v));
                }
                expectWithinAbsoluteError (SampleImporter::estimatePitchHz (note, sr), hz, hz * 0.01);
            }
            expectEquals (SampleImporter::noteName (62), juce::String ("D4"));
        }

        beginTest ("Import writes a playable sample library with velocity layers");
        {
            auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("JGL_import_test");
            tmp.deleteRecursively();
            auto src = tmp.getChildFile ("src");
            src.createDirectory();

            juce::AudioBuffer<float> take (2, (int) (sr * 3.0));
            take.clear();
            for (int h = 0; h < 6; ++h)
                for (int i = 0; i < (int) (0.3 * sr); ++i)
                    for (int c = 0; c < 2; ++c)
                        take.addSample (c, (int) ((0.2 + 0.45 * h) * sr) + i,
                                        (0.15f + 0.15f * (float) h) * std::sin ((float) i * 0.05f) * std::exp (-(float) i / 3000.0f));
            expect (WavExporter::write (take, sr, WavBitDepth::Pcm24, src.getChildFile ("na_take.wav")).wasOk());

            SampleImporter::Options o;
            o.instrument = InstrumentId::Tabla;
            o.articulation = "Na";
            o.velocityLayers = 3;
            o.destinationRoot = tmp.getChildFile ("Samples");
            const auto rep = SampleImporter::run ({ src }, o);
            expectEquals (rep.samplesWritten, 6);

            SampleLibrary lib;
            expectEquals (lib.load (o.destinationRoot), 6);
            expectEquals (lib.numLayers (InstrumentId::Tabla, TablaArt::Na), 3);
            expect (lib.pick (InstrumentId::Tabla, TablaArt::Na, 1.0f, 7)->layer == 3);

            // Melodic note with the root detected from the audio
            juce::AudioBuffer<float> note (1, (int) sr);
            for (int i = 0; i < note.getNumSamples(); ++i)
                note.setSample (0, i, 0.4f * (float) std::sin (juce::MathConstants<double>::twoPi * 293.66 * i / sr));
            auto melodicSrc = tmp.getChildFile ("mel");
            melodicSrc.createDirectory();
            expect (WavExporter::write (note, sr, WavBitDepth::Pcm24, melodicSrc.getChildFile ("sustain_take1.wav")).wasOk());
            SampleImporter::Options m;
            m.instrument = InstrumentId::Nadaswaram;
            m.destinationRoot = o.destinationRoot;
            expectEquals (SampleImporter::run ({ melodicSrc }, m).samplesWritten, 1);
            expect (o.destinationRoot.getChildFile ("nadaswaram/notes/nadaswaram_D4.wav").existsAsFile());
            SampleLibrary lib2;
            lib2.load (o.destinationRoot);
            expect (lib2.hasMelodicNotes (InstrumentId::Nadaswaram));

            tmp.deleteRecursively();
        }
    }
};

static ImporterTests importerTests;

} // namespace jgl

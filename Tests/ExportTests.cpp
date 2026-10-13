#include "TestHelpers.h"
#include "IO/AppPaths.h"
#include "IO/PresetManager.h"
#include "IO/WavExporter.h"

namespace jgl
{

class ExportTests : public juce::UnitTest
{
public:
    ExportTests() : juce::UnitTest ("WAV export & presets", "JGL") {}

    void runTest() override
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();

        auto loop = test::renderLoop (test::compose ((int) StyleId::AfroIndian, { InstrumentId::Dhol, InstrumentId::Tabla, InstrumentId::Tumbi }),
                                      test::render (124.0, 44100.0), 11);
        const auto dir = AppPaths::exportsFolder();

        beginTest ("File names follow IndianGroove_<Style>_<BPM>BPM_<Key>_<Bars>Bars.wav");
        {
            LoopFileInfo info;
            info.styleTag = "AfroIndian";
            info.bpm = 124.0;
            info.keyLabel = keyLabel (2, 1);
            info.bars = 4;
            expectEquals (WavExporter::makeFileName (info), juce::String ("IndianGroove_AfroIndian_124BPM_Dmin_4Bars.wav"));
            info.keyLabel = {};
            info.bars = 1;
            info.bpm = 127.5;
            expectEquals (WavExporter::makeFileName (info), juce::String ("IndianGroove_AfroIndian_127.5BPM_1Bar.wav"));

            auto f1 = WavExporter::uniqueFile (dir, "dup.wav");
            f1.replaceWithText ("x");
            auto f2 = WavExporter::uniqueFile (dir, "dup.wav");
            expectEquals (f2.getFileName(), juce::String ("dup_2.wav"));
        }

        for (auto depth : { WavBitDepth::Pcm24, WavBitDepth::Pcm16, WavBitDepth::Float32 })
        {
            beginTest ("Round trip " + bitDepthNames()[(int) depth]);
            const auto file = dir.getChildFile ("roundtrip_" + juce::String ((int) depth) + ".wav");
            const auto res = WavExporter::write (loop->audio, 44100.0, depth, file, 124.0, 16, 50);
            expect (res.wasOk(), res.getErrorMessage());

            std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
            expect (reader != nullptr);
            if (reader == nullptr) continue;

            expectEquals (reader->sampleRate, 44100.0);
            expectEquals ((int) reader->numChannels, 2);
            expectEquals ((int) reader->bitsPerSample, depth == WavBitDepth::Pcm16 ? 16 : (depth == WavBitDepth::Pcm24 ? 24 : 32));
            expect (reader->usesFloatingPointData == (depth == WavBitDepth::Float32), "float flag");
            expectEquals (reader->lengthInSamples, (juce::int64) loop->audio.getNumSamples());
            expectEquals (reader->lengthInSamples, loopLengthSamples (4, 124.0, 44100.0));

            juce::AudioBuffer<float> back (2, (int) reader->lengthInSamples);
            reader->read (&back, 0, back.getNumSamples(), 0, true, true);
            float maxErr = 0.0f;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < back.getNumSamples(); ++i)
                    maxErr = std::max (maxErr, std::abs (back.getSample (c, i) - loop->audio.getSample (c, i)));
            const float tolerance = depth == WavBitDepth::Pcm16 ? 1.0e-4f : (depth == WavBitDepth::Pcm24 ? 1.0e-6f : 0.0f);
            expectLessOrEqual (maxErr, tolerance);

            // First sample of the file is the first sample of bar 1 (no leading silence / offset).
            expectWithinAbsoluteError (back.getSample (0, 0), loop->audio.getSample (0, 0), tolerance + 1.0e-7f);

            const auto tempo = reader->metadataValues.getValue (juce::WavAudioFormat::acidTempo, "0").getFloatValue();
            expectWithinAbsoluteError (tempo, 124.0f, 0.01f);
            expectEquals (reader->metadataValues.getValue (juce::WavAudioFormat::acidBeats, "0").getIntValue(), 16);
        }

        beginTest ("Export at other sample rates re-renders with exact length");
        {
            GenerationService::Request q;
            q.compose = loop->compose;
            q.render = loop->render;
            q.render.sampleRate = 96000.0;
            q.seeds = loop->seeds;
            auto hi = GenerationService::renderNow (q, nullptr, nullptr, &loop->arrangement);
            expectEquals ((juce::int64) hi->audio.getNumSamples(), loopLengthSamples (4, 124.0, 96000.0));
            const auto file = dir.getChildFile ("hi.wav");
            expect (WavExporter::write (hi->audio, 96000.0, WavBitDepth::Pcm24, file).wasOk());
            std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
            expectEquals (reader->sampleRate, 96000.0);
        }

        beginTest ("Presets save and load");
        {
            const auto& factory = PresetManager::factoryPresets();
            expectGreaterOrEqual ((int) factory.size(), 12);
            for (auto& p : factory)
            {
                float v = 0.0f;
                expect (PresetManager::getValue (p, "bpm", v) && v >= 70.0f && v <= 160.0f);
                SeedSet s;
                expect (SeedSet::fromString (p.getProperty ("seeds").toString(), s));
            }

            auto copy = factory.front().createCopy();
            expect (PresetManager::saveUserPreset ("My Favourite", copy).wasOk());
            expect (PresetManager::userPresetNames().contains ("My Favourite"));
            auto loaded = PresetManager::loadUserPreset ("My Favourite");
            expect (loaded.isValid());
            float bpm = 0.0f;
            expect (PresetManager::getValue (loaded, "bpm", bpm));
            expectEquals (bpm, 124.0f);
            expect (PresetManager::deleteUserPreset ("My Favourite"));
            expect (! PresetManager::userPresetNames().contains ("My Favourite"));
        }
    }
};

static ExportTests exportTests;

} // namespace jgl

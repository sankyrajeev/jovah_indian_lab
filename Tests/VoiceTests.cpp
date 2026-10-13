#include "TestHelpers.h"
#include "DSP/PitchCurve.h"
#include "DSP/SampleAnalysis.h"
#include "DSP/SampleLibrary.h"
#include "IO/WavExporter.h"
#include "Plugin/PluginProcessor.h"

namespace jgl
{

namespace
{
    /** A horn-like test note: steady for `steadySec`, then the pitch sags and fades (like a real "descent"). */
    juce::AudioBuffer<float> hornNote (double hz, double sr, double steadySec, double tailSec)
    {
        const int n = (int) ((steadySec + tailSec) * sr);
        juce::AudioBuffer<float> b (1, n);
        double phase = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            const double sag = t > steadySec ? -1.2 * (t - steadySec) / tailSec : 0.0;
            const double f = hz * std::pow (2.0, sag / 12.0);
            phase += juce::MathConstants<double>::twoPi * f / sr;
            const double env = std::min (1.0, t / 0.03) * (t > steadySec ? std::exp (-(t - steadySec) * 3.0) : 1.0);
            double v = 0.0;
            for (int k = 1; k <= 6; ++k) v += std::sin (phase * k) / k;
            b.setSample (0, i, (float) (0.4 * env * v));
        }
        return b;
    }
}

class VoiceTests : public juce::UnitTest
{
public:
    VoiceTests() : juce::UnitTest ("Your Sample voice", "JGL") {}

    void runTest() override
    {
        const double sr = 44100.0;
        const auto note = hornNote (466.16, sr, 2.0, 1.2);

        beginTest ("Analysis finds the true pitch and a loop in the steady part");
        {
            const auto loop = SampleAnalysis::findSustainLoop (note, sr);
            expectWithinAbsoluteError (loop.medianMidi, 70.0, 0.05);
            expect (loop.valid());
            expect (loop.end < (int) (2.0 * sr), "loop must stay out of the falling tail");
            const double period = sr / 466.16;
            const double periods = (loop.end - loop.start) / period;
            expectWithinAbsoluteError (periods, std::round (periods), 0.05);
        }

        beginTest ("Long notes sustain through the loop without clicks, in tune");
        {
            SampleLibrary::Sample s;
            s.data = note;
            s.sampleRate = sr;
            SampleLibrary::analyseNote (s);
            expect (s.loopStart >= 0);

            std::vector<float> out;
            PitchCurve flat;
            SampleLibrary::renderNote (s, 62.0, 0.9f, 5.0, flat, 48000.0, out);   // D4, much longer than the recording
            expectGreaterThan ((int) out.size(), (int) (5.0 * 48000.0));

            juce::AudioBuffer<float> b (1, (int) out.size());
            for (size_t i = 0; i < out.size(); ++i) b.setSample (0, (int) i, out[i]);
            expectGreaterThan (b.getRMSLevel (0, (int) (4.0 * 48000.0), 48000), 0.05f);   // still sounding at 4-5 s

            float maxStep = 0.0f;
            for (int i = 1; i < (int) (5.0 * 48000.0); ++i)
                maxStep = std::max (maxStep, std::abs (out[(size_t) i] - out[(size_t) i - 1]));
            expectLessThan (maxStep, 0.12f);

            const double hz = SampleAnalysis::estimatePitchHz (b, 48000.0, 24000, 200000);
            expectWithinAbsoluteError (SampleAnalysis::hzToMidi (hz), 62.0, 0.05);
        }

        beginTest ("Pitch curves bend the sample");
        {
            SampleLibrary::Sample s;
            s.data = note;
            s.sampleRate = sr;
            SampleLibrary::analyseNote (s);
            PitchCurve fall;
            fall.fallSemis = -5.0;
            fall.fallStart = 0.6;
            fall.fallEnd = 1.0;
            std::vector<float> out;
            SampleLibrary::renderNote (s, 70.0, 0.9f, 1.0, fall, 48000.0, out);
            juce::AudioBuffer<float> b (1, (int) out.size());
            for (size_t i = 0; i < out.size(); ++i) b.setSample (0, (int) i, out[i]);
            const double start = SampleAnalysis::hzToMidi (SampleAnalysis::estimatePitchHz (b, 48000.0, 4800, 24000));
            const double end = SampleAnalysis::hzToMidi (SampleAnalysis::estimatePitchHz (b, 48000.0, 44000, 48500));
            expectWithinAbsoluteError (start, 70.0, 0.1);
            expectLessThan (end, 67.0);
        }

        beginTest ("Expression knobs shape the melody, notes stay in the scale");
        {
            auto base = test::compose ((int) StyleId::MelodicIndian, { InstrumentId::CustomVoice, InstrumentId::Tabla }, 4, 10);
            auto count = [&] (float bend, float stutter, float falls)
            {
                auto cs = base;
                cs.voiceBend = bend; cs.voiceStutter = stutter; cs.voiceFalls = falls; cs.voiceShake = 0.5f; cs.voiceOctave = 0.5f;
                int events = 0, glides = 0, fallsN = 0;
                for (uint64_t seed = 1; seed <= 10; ++seed)
                {
                    const auto a = Arranger::compose (cs, SeedSet::fromMaster (seed));
                    for (auto& p : a.parts)
                    {
                        if (p.instrument != InstrumentId::CustomVoice) continue;
                        for (auto& e : p.events)
                        {
                            ++events;
                            glides += e.glideFrom != 0.0f ? 1 : 0;
                            fallsN += e.fallSemis < 0.0f ? 1 : 0;
                            expect (scaleContains (scaleProfile (10), e.pitch), "voice pitch in scale");
                        }
                    }
                }
                return std::array<int, 3> { events, glides, fallsN };
            };
            const auto calm = count (0.0f, 0.0f, 0.0f);
            const auto wild = count (1.0f, 1.0f, 1.0f);
            expectGreaterThan (wild[0], calm[0]);            // stutters add notes
            expectGreaterThan (wild[1], calm[1] * 2 + 5);    // many more bends
            expectGreaterThan (wild[2], calm[2] + 5);        // more falls
        }

        beginTest ("Register follows the sample; processor loads, renders and restores it");
        {
            const double t = LoopRenderer::tonicMidi (InstrumentId::CustomVoice, 2, 70.0);
            expect (std::abs (t - 67.0) <= 6.0 && pitchClass ((int) t) == 2);

            auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("jgl_voice_test.wav");
            juce::AudioBuffer<float> st (2, note.getNumSamples());
            st.copyFrom (0, 0, note, 0, 0, note.getNumSamples());
            st.copyFrom (1, 0, note, 0, 0, note.getNumSamples());
            expect (WavExporter::write (st, sr, WavBitDepth::Pcm24, file).wasOk());

            juce::MemoryBlock state;
            {
                GrooveLabProcessor p;
                expect (p.loadUserVoice (file).wasOk());
                expect (p.getUserVoice() != nullptr);
                expect (p.describeUserVoice().startsWith ("A#4"));
                expect (p.getState().getRawParameterValue (ParamIDs::instrument (InstrumentId::CustomVoice).toRawUTF8())->load() > 0.5f);
                p.getStateInformation (state);
            }
            {
                GrooveLabProcessor p;
                p.setStateInformation (state.getData(), (int) state.getSize());
                expect (p.getUserVoice() != nullptr, "voice restored with the session");
            }
            expect (! GrooveLabProcessor().loadUserVoice (juce::File ("/nonexistent.wav")).wasOk());
            file.deleteFile();

            // Rendering with the voice produces sound for its part
            auto cs = test::compose ((int) StyleId::PsychedelicIndia, { InstrumentId::CustomVoice }, 2, 1);
            GenerationService::Request req;
            req.compose = cs;
            req.render = test::render (120.0);
            req.seeds = SeedSet::fromMaster (5);
            juce::String err;
            auto v = SampleLibrary::loadVoice (juce::File(), err);
            expect (v == nullptr && err.isNotEmpty());
            SampleLibrary::Sample s;
            s.data = note; s.sampleRate = sr; SampleLibrary::analyseNote (s);
            req.userVoice = std::make_shared<SampleLibrary::Sample> (std::move (s));
            auto r = GenerationService::renderNow (req, nullptr);
            expectGreaterThan (r->audio.getRMSLevel (0, 0, r->audio.getNumSamples()), 0.01f);
        }
    }
};

static VoiceTests voiceTests;

} // namespace jgl

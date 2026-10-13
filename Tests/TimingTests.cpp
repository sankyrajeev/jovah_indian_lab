#include "TestHelpers.h"
#include "Generation/GrooveEngine.h"

namespace jgl
{

class TimingTests : public juce::UnitTest
{
public:
    TimingTests() : juce::UnitTest ("Loop timing", "JGL") {}

    void runTest() override
    {
        beginTest ("Loop length formula is exact");
        {
            expectEquals ((int) loopLengthSamples (4, 120.0, 48000.0), 384000);   // 8.000 s
            expectEquals ((int) loopLengthSamples (4, 120.0, 44100.0), 352800);
            expectEquals ((int) loopLengthSamples (1, 120.0, 48000.0), 96000);
            expectEquals ((int) loopLengthSamples (8, 70.0, 96000.0), 2633143);   // 27.428571 s
            expectEquals ((int) loopLengthSamples (4, 124.0, 44100.0), 341419);   // 7.741935 s rounded
            expectEquals ((int) loopLengthSamples (2, 160.0, 48000.0), 144000);
        }

        beginTest ("Rendered WAV duration matches BPM x bars at several rates");
        {
            const auto cs4 = test::compose ((int) StyleId::MinimalHypnosis, { InstrumentId::Tabla, InstrumentId::Tumbi });
            for (double sr : { 44100.0, 48000.0, 96000.0 })
                for (double bpm : { 70.0, 124.0, 160.0 })
                    for (int bars : { 1, 2, 4, 8 })
                    {
                        if (sr > 50000.0 && bars == 8) continue;   // keep the suite fast
                        auto cs = cs4;
                        cs.bars = bars;
                        auto r = test::renderLoop (cs, test::render (bpm, sr), 99);
                        expect (r != nullptr);
                        if (r == nullptr) continue;
                        expectEquals ((juce::int64) r->audio.getNumSamples(), loopLengthSamples (bars, bpm, sr),
                                      "bars " + juce::String (bars) + " bpm " + juce::String (bpm) + " sr " + juce::String (sr));
                        expectEquals (r->audio.getNumChannels(), 2);
                    }
        }

        beginTest ("No leading silence: the loop starts with sound");
        {
            auto r = test::renderLoop (test::compose ((int) StyleId::PeakTimePercussion, { InstrumentId::Dhol, InstrumentId::Tabla }),
                                       test::render (128.0), 5);
            // Within the first 10 ms there must be signal well above the noise floor.
            const int n = (int) (0.010 * 48000.0);
            expectGreaterThan (r->audio.getMagnitude (0, n), 0.05f);
        }

        beginTest ("Swing warp");
        {
            RenderSettings rs = test::render();
            const auto& style = styleProfile (StyleId::Custom);

            rs.swing = 0.0f; rs.groove = 0.0f;
            GrooveEngine straight (rs, style);
            for (double b : { 0.0, 0.25, 0.5, 0.75, 1.0, 3.75 })
                expectWithinAbsoluteError (straight.warpBeat (b, true), b, 1.0e-9);

            rs.swing = 1.0f;
            GrooveEngine full (rs, style);
            expectWithinAbsoluteError (full.warpBeat (0.25, true), 0.375, 1.0e-9);  // 75% swing
            expectWithinAbsoluteError (full.warpBeat (0.5, true), 0.5, 1.0e-9);     // 8ths untouched
            expectWithinAbsoluteError (full.warpBeat (0.25, false), 0.25, 1.0e-9);  // triplets never swing

            double prev = -1.0;
            for (int i = 0; i <= 64; ++i)
            {
                const double w = full.warpBeat (i / 32.0, true);
                expect (w > prev, "warp must be monotonic");
                prev = w;
            }
        }

        beginTest ("Humanised onsets stay close to the grid");
        {
            auto cs = test::composeAll ((int) StyleId::AfroIndian);
            auto a = Arranger::compose (cs, SeedSet::fromMaster (77));
            RenderSettings rs = test::render (124.0);
            rs.humanize = 1.0f;
            rs.swing = 0.0f;
            rs.groove = 1.0f;
            GrooveEngine ge (rs, styleProfile (cs.styleIndex));
            double worst = 0.0;
            for (auto& p : a.parts)
                for (auto& e : p.events)
                    worst = std::max (worst, std::abs (ge.onsetSeconds (e, p) - e.beat * 60.0 / rs.bpm));
            expectLessThan (worst, 0.06);
        }
    }
};

static TimingTests timingTests;

} // namespace jgl

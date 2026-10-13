#include "TestHelpers.h"
#include "Generation/PatternLibrary.h"

namespace jgl
{

class GenerationTests : public juce::UnitTest
{
public:
    GenerationTests() : juce::UnitTest ("Rhythm & phrase generation", "JGL") {}

    void runTest() override
    {
        beginTest ("Pattern library cells are valid");
        {
            for (auto& c : percussionCells())
            {
                const auto steps = stripNotation (c.pattern).length();
                expect (steps > 0 && steps % (4 * c.stepsPerBeat) == 0, juce::String ("cell length: ") + c.label);
                const auto parsed = parseCell (c);
                int hits = 0;
                for (auto& s : parsed)
                {
                    if (! s.hit) continue;
                    ++hits;
                    expect (s.articulation >= 0 && s.articulation < (int) instrumentInfo (c.instrument).articulations.size(),
                            juce::String ("articulation in range: ") + c.label);
                }
                expectGreaterThan (hits, 0);
            }
            for (auto& c : melodyRhythmCells())
                expect (stripNotation (c.pattern).length() % (4 * c.stepsPerBeat) == 0);
        }

        beginTest ("Same seed + settings reproduce the identical loop");
        {
            for (int style = 0; style < numStyles; ++style)
            {
                const auto cs = test::composeAll (style);
                const auto a = Arranger::compose (cs, SeedSet::fromMaster (1234));
                const auto b = Arranger::compose (cs, SeedSet::fromMaster (1234));
                expect (test::sameArrangement (a, b), styleProfile (style).name);
                expectEquals ((int) a.parts.size(), numInstruments);
            }
        }

        beginTest ("Different seeds give meaningfully different loops");
        {
            for (int style = 0; style < numStyles; ++style)
            {
                int different = 0;
                for (uint64_t s = 0; s < 10; ++s)
                {
                    const auto cs = test::compose (style, { InstrumentId::Tabla, InstrumentId::Dhol, InstrumentId::Sitar });
                    const auto a = Arranger::compose (cs, SeedSet::fromMaster (100 + s));
                    const auto b = Arranger::compose (cs, SeedSet::fromMaster (200 + s));
                    different += test::sameArrangement (a, b) ? 0 : 1;
                }
                expectEquals (different, 10, styleProfile (style).name);
            }
        }

        beginTest ("Regenerating one part leaves every other part untouched");
        {
            const auto cs = test::composeAll ((int) StyleId::AfroIndian);
            auto seeds = SeedSet::fromMaster (555);
            const auto before = Arranger::compose (cs, seeds);
            seeds.partSeeds[(size_t) toIndex (InstrumentId::Tabla)] = 0xABCDEF;
            const auto after = Arranger::compose (cs, seeds);

            for (size_t i = 0; i < before.parts.size(); ++i)
            {
                const bool same = test::sameEvents (before.parts[i], after.parts[i]);
                if (before.parts[i].instrument == InstrumentId::Tabla)
                    expect (! same, "tabla must change");
                else
                    expect (same, juce::String (instrumentInfo (before.parts[i].instrument).name) + " must be preserved");
            }
        }

        beginTest ("New melody keeps the groove, new rhythm keeps the melody");
        {
            const auto cs = test::composeAll ((int) StyleId::PsychedelicIndia);
            auto seeds = SeedSet::fromMaster (9001);
            const auto before = Arranger::compose (cs, seeds);
            for (auto id : allInstruments())
                if (isMelodic (id)) seeds.partSeeds[(size_t) toIndex (id)] ^= 0x5555;
            const auto after = Arranger::compose (cs, seeds);
            for (size_t i = 0; i < before.parts.size(); ++i)
                if (isPercussion (before.parts[i].instrument))
                    expect (test::sameEvents (before.parts[i], after.parts[i]), "percussion preserved");
        }

        beginTest ("Events are inside the loop and well formed");
        {
            for (int style = 0; style < numStyles; ++style)
                for (int bars : { 1, 2, 4, 8 })
                {
                    const auto cs = test::composeAll (style, bars);
                    const auto a = Arranger::compose (cs, SeedSet::fromMaster ((uint64_t) (style * 31 + bars)));
                    expectEquals (a.bars, bars);
                    for (auto& p : a.parts)
                    {
                        expect (! p.events.empty(), juce::String (instrumentInfo (p.instrument).name) + " produced no events");
                        for (auto& e : p.events)
                        {
                            expect (e.beat >= 0.0 && e.beat < a.lengthBeats(), "event inside loop");
                            expect (e.velocity > 0.0f && e.velocity <= 1.0f, "velocity range");
                            expect (e.length > 0.0, "positive length");
                            if (isPercussion (p.instrument))
                                expect (e.articulation >= 0 && e.articulation < (int) instrumentInfo (p.instrument).articulations.size());
                        }
                    }
                }
        }

        beginTest ("Lead drums anchor the downbeat");
        {
            for (int style = 0; style < numStyles; ++style)
                for (uint64_t seed = 1; seed <= 6; ++seed)
                {
                    const auto cs = test::compose (style, { InstrumentId::Tabla, InstrumentId::Dhol });
                    const auto a = Arranger::compose (cs, SeedSet::fromMaster (seed));
                    for (auto& p : a.parts)
                    {
                        bool downbeat = false;
                        for (auto& e : p.events)
                            downbeat = downbeat || e.beat == 0.0;
                        expect (downbeat, juce::String (instrumentInfo (p.instrument).name) + " has no hit on bar 1 beat 1 in " + styleProfile (style).name);
                    }
                }
        }

        beginTest ("Density and energy shape the amount of material");
        {
            auto count = [] (float density, int style)
            {
                int n = 0;
                for (uint64_t seed = 1; seed <= 12; ++seed)
                {
                    auto cs = test::compose (style, { InstrumentId::Tabla, InstrumentId::Dhol, InstrumentId::Dholak, InstrumentId::Manjira });
                    cs.density = density;
                    n += Arranger::compose (cs, SeedSet::fromMaster (seed)).totalEvents();
                }
                return n;
            };
            for (int style : { (int) StyleId::AfroIndian, (int) StyleId::TribalRitual, (int) StyleId::Custom })
                expectGreaterThan (count (0.9f, style), (int) (count (0.15f, style) * 1.15));

            // Personalities behave differently: peak-time is much busier than minimal.
            auto events = [] (int style)
            {
                int n = 0;
                for (uint64_t seed = 1; seed <= 12; ++seed)
                    n += Arranger::compose (test::compose (style, { InstrumentId::Tabla, InstrumentId::Dhol, InstrumentId::Manjira }),
                                            SeedSet::fromMaster (seed)).totalEvents();
                return n;
            };
            expectGreaterThan (events ((int) StyleId::PeakTimePercussion), (int) (events ((int) StyleId::MinimalHypnosis) * 1.5));
        }

        beginTest ("Phrases have fills at the end and repetition inside");
        {
            int lastBeatHits = 0, firstBarLastBeatHits = 0, identicalBar1And3 = 0, trials = 0;
            for (uint64_t seed = 1; seed <= 20; ++seed)
            {
                const auto cs = test::compose ((int) StyleId::PeakTimePercussion, { InstrumentId::Tabla });
                const auto a = Arranger::compose (cs, SeedSet::fromMaster (seed));
                const auto& p = a.parts.front();
                std::vector<double> bar1, bar3;
                for (auto& e : p.events)
                {
                    if (e.beat >= 15.0) ++lastBeatHits;
                    if (e.beat >= 3.0 && e.beat < 4.0) ++firstBarLastBeatHits;
                    if (e.beat < 4.0) bar1.push_back (e.beat);
                    if (e.beat >= 8.0 && e.beat < 12.0) bar3.push_back (e.beat - 8.0);
                }
                identicalBar1And3 += bar1 == bar3 ? 1 : 0;
                ++trials;
            }
            expectGreaterThan (lastBeatHits, firstBarLastBeatHits);   // phrase-end fills
            expectGreaterThan (identicalBar1And3, trials / 3);          // motif repetition (A ... A)
        }

        beginTest ("Seed sets round-trip through text");
        {
            auto s = SeedSet::fromMaster (0xDEADBEEFCAFEull);
            s.partSeeds[3] = 42;
            SeedSet back;
            expect (SeedSet::fromString (s.toString(), back));
            expect (back == s);
            expect (! SeedSet::fromString ("garbage", back));
        }
    }
};

static GenerationTests generationTests;

} // namespace jgl

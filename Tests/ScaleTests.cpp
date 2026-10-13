#include "TestHelpers.h"
#include "Generation/MelodyGenerator.h"

namespace jgl
{

class ScaleTests : public juce::UnitTest
{
public:
    ScaleTests() : juce::UnitTest ("Keys, scales and raga profiles", "JGL") {}

    void runTest() override
    {
        beginTest ("Scale profiles are well formed");
        {
            expectEquals (keyNames().size(), 12);
            expectGreaterOrEqual (numScales(), 17);
            for (int i = 0; i < numScales(); ++i)
            {
                const auto& s = scaleProfile (i);
                expect (s.notes.front() == 0, s.name);
                for (size_t k = 1; k < s.notes.size(); ++k)
                    expect (s.notes[k] > s.notes[k - 1] && s.notes[k] < 12, s.name + " ascending pitch classes");
                for (int p : s.aroha)   expect (scaleContains (s, p), s.name + " aroha subset");
                for (int p : s.avaroha) expect (scaleContains (s, p), s.name + " avaroha subset");
                expect (scaleContains (s, s.vadi) && scaleContains (s, s.samvadi), s.name + " vadi/samvadi in scale");
                expect (s.fileTag.isNotEmpty());
                if (s.ragaInspired)
                    expect (s.name.contains ("inspired"), "raga profiles must be labelled as inspired");
            }
        }

        beginTest ("Scale stepping and labels");
        {
            const auto& major = scaleProfile (0);
            expectEquals (stepInScale (major, 0, 1), 2);
            expectEquals (stepInScale (major, 0, 2), 4);
            expectEquals (stepInScale (major, 0, -1), -1);
            expectEquals (stepInScale (major, 11, 1), 12);
            expectEquals (keyLabel (2, 1), juce::String ("Dmin"));
            expectEquals (keyLabel (6, 0), juce::String ("F#maj"));
            expectEquals (keyLabel (1, 9), juce::String ("C#Bhairav"));
        }

        beginTest ("Melodic parts stay in the selected scale and respect aroha / avaroha");
        {
            int notes = 0, directionViolations = 0;
            for (int scale = 0; scale < numScales(); ++scale)
                for (int style : { (int) StyleId::PsychedelicIndia, (int) StyleId::MelodicIndian, (int) StyleId::FolkElectronica, (int) StyleId::AfroIndian })
                    for (uint64_t seed = 1; seed <= 4; ++seed)
                    {
                        auto cs = test::compose (style, { InstrumentId::Sitar, InstrumentId::Bansuri, InstrumentId::Tumbi, InstrumentId::Tanpura }, 4, scale);
                        const auto a = Arranger::compose (cs, SeedSet::fromMaster (seed * 7919 + (uint64_t) scale));
                        const auto ctx = GenerationContext::make (cs);
                        const auto& s = scaleProfile (scale);

                        for (auto& p : a.parts)
                        {
                            const auto range = MelodyGenerator::pitchRange (p.instrument, ctx);
                            for (size_t i = 0; i < p.events.size(); ++i)
                            {
                                const auto& e = p.events[i];
                                ++notes;
                                if (p.instrument == InstrumentId::Tanpura)
                                {
                                    expect (e.pitch == 0 || e.pitch == -12 || e.pitch == s.tanpuraString, "tanpura strings");
                                    continue;
                                }
                                expect (scaleContains (s, e.pitch), s.name + ": pitch " + juce::String (e.pitch) + " not in scale");
                                expect (range.contains (e.pitch) || e.pitch == range.getEnd(), "pitch inside instrument range");
                                if (e.ornament == OrnKan || e.ornament == OrnMurki)
                                    expect (scaleContains (s, e.ornamentPitch), "grace notes from the scale");
                                if (e.glideFrom != 0.0f && p.instrument != InstrumentId::Tumbi)
                                    expect (scaleContains (s, e.pitch + juce::roundToInt (e.glideFrom)), "meend starts on a scale note");

                                if (i > 0 && s.ragaInspired)
                                {
                                    const int prev = p.events[i - 1].pitch;
                                    if (e.pitch > prev && ! arohaContains (s, e.pitch)) ++directionViolations;
                                    if (e.pitch < prev && ! avarohaContains (s, e.pitch)) ++directionViolations;
                                }
                            }
                        }
                    }

            expectGreaterThan (notes, 2000);
            expectEquals (directionViolations, 0);
        }

        beginTest ("Melodic notes transpose with the key; unpitched drums do not");
        {
            expectEquals (LoopRenderer::tonicMidi (InstrumentId::Sitar, 0) + 2.0, LoopRenderer::tonicMidi (InstrumentId::Sitar, 2));
            for (int k = 0; k < 12; ++k)
            {
                const double hz = LoopRenderer::tablaSaHz (k);
                expect (hz >= 219.0 && hz <= 416.0, "dayan stays in its natural range");
                const double t = LoopRenderer::tonicMidi (InstrumentId::Tumbi, k);
                expect (t >= 67 && t <= 78, "tumbi register");
            }

            // Unpitched percussion renders identically in every key.
            auto cs = test::compose ((int) StyleId::PeakTimePercussion,
                                     { InstrumentId::Dhol, InstrumentId::Dholak, InstrumentId::Manjira, InstrumentId::Ghungroo, InstrumentId::Taali }, 2);
            auto inC  = test::renderLoop (cs, test::render (128.0, 48000.0, 0), 31);
            auto inFs = test::renderLoop (cs, test::render (128.0, 48000.0, 6), 31);
            float maxDiff = 0.0f;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < inC->audio.getNumSamples(); ++i)
                    maxDiff = std::max (maxDiff, std::abs (inC->audio.getSample (c, i) - inFs->audio.getSample (c, i)));
            expectEquals (maxDiff, 0.0f);
        }
    }
};

static ScaleTests scaleTests;

} // namespace jgl

#include "PatternLibrary.h"

namespace jgl
{

namespace
{
    constexpr uint32_t AF = 1u << (uint32_t) StyleId::AfroIndian;
    constexpr uint32_t TR = 1u << (uint32_t) StyleId::TribalRitual;
    constexpr uint32_t UN = 1u << (uint32_t) StyleId::IndianUnderground;
    constexpr uint32_t PS = 1u << (uint32_t) StyleId::PsychedelicIndia;
    constexpr uint32_t ME = 1u << (uint32_t) StyleId::MelodicIndian;
    constexpr uint32_t PK = 1u << (uint32_t) StyleId::PeakTimePercussion;
    constexpr uint32_t MI = 1u << (uint32_t) StyleId::MinimalHypnosis;
    constexpr uint32_t FO = 1u << (uint32_t) StyleId::FolkElectronica;
    constexpr uint32_t CU = 1u << (uint32_t) StyleId::Custom;
    constexpr uint32_t ALL = allStylesMask;

    constexpr uint32_t KEH = 1u << (uint32_t) FrameworkId::Keherwa;
    constexpr uint32_t DAD = 1u << (uint32_t) FrameworkId::Dadra;
    constexpr uint32_t TEE = 1u << (uint32_t) FrameworkId::Teentaal;
    constexpr uint32_t RUP = 1u << (uint32_t) FrameworkId::Rupak;
    constexpr uint32_t CLB = 1u << (uint32_t) FrameworkId::Club;
    constexpr uint32_t TRE = 1u << (uint32_t) FrameworkId::Tresillo;

    using I = InstrumentId;
}

const std::vector<PatternCell>& percussionCells()
{
    static const std::vector<PatternCell> cells =
    {
        //------------------------------------------------------------------
        // TABLA   d Dha  h Dhin  g Ge  k Ka  n Na  t Tin  u Tun  e Te  r Re  o Ghe (bayan glide)
        //------------------------------------------------------------------
        { I::Tabla, "D.g. n.e. n.k. H.n.", 4, ME | PS | FO | MI | CU, KEH, .40f, .30f, 1.2f, "Keherwa theka" },
        { I::Tabla, "D.gn e.n. d.gn e.kG", 4, AF | PK | UN | TR | FO | CU, KEH, .60f, .50f, 1.0f, "Keherwa, club cut" },
        { I::Tabla, "D..o .n.e .n.k H..o", 4, PS | MI | ME, KEH, .45f, .50f, 1.0f, "Keherwa with gumki" },
        { I::Tabla, "Dhn den dhn den", 3, ME | AF | FO | PS | CU, DAD, .40f, .30f, 1.2f, "Dadra theka" },
        { I::Tabla, "D.n den G.n deN", 3, AF | ME | TR | CU, DAD, .55f, .50f, 1.0f, "Dadra, open bayan" },
        { I::Tabla, "Dhhd Dhhd dttn Nhhd", 4, TR | PK | PS | CU, TEE, .75f, .60f, 1.2f, "Teentaal, drut" },
        { I::Tabla, "D.h. h.d. D.h. h.d. d.t. t.n. N.h. h.d.", 4, ME | PS | MI | TR | CU, TEE, .50f, .40f, 1.0f, "Teentaal, madhya" },
        { I::Tabla, "T.t H.h. T.t H.h. er", 4, UN | PS | AF | CU, RUP, .55f, .55f, 1.2f, "Rupak 3+2+2" },
        { I::Tabla, "k.n G.n. k.n G.n. gn", 4, UN | TR | PK | CU, RUP, .60f, .50f, 1.0f, "Rupak, bayan drive" },
        { I::Tabla, "D.nG .nH. n.gN e.Gn", 4, AF | FO | CU, TRE, .60f, .60f, 1.2f, "Tresillo bols" },
        { I::Tabla, "D..g ..N. k.g. .N.n", 4, AF | MI | ME | CU, TRE, .40f, .40f, 1.0f, "Tresillo, sparse" },
        { I::Tabla, "D.nG .nH. n.gN .Gn. D.nG .nH. n.gN erGn", 4, AF | FO, TRE, .65f, .65f, 0.9f, "Tresillo call & answer" },
        { I::Tabla, "D.gG .kD. gGk. D.nE", 4, TR | PK, 0, .80f, .60f, 1.0f, "Ritual bayan" },
        { I::Tabla, "k.n. G.n. k.n. Gnen", 4, UN | MI | PK, 0, .55f, .45f, 1.0f, "Rolling ge" },
        { I::Tabla, "U..o ..n. T.e. o.n.", 4, PS | MI | ME, 0, .40f, .50f, 1.0f, "Tun & gumki" },
        { I::Tabla, "Dnen Gnen Dnen Gnek", 4, PK | TR, 0, .95f, .70f, 1.0f, "Driving 16ths" },
        { I::Tabla, "D... n..g ..n. o...", 4, MI | UN | PS, 0, .25f, .30f, 1.0f, "Minimal bols" },
        { I::Tabla, "D.nd .nD. n.dD nd.n", 4, FO | AF, 0, .70f, .50f, 1.0f, "Bouncy folk" },
        { I::Tabla, "Derk terk Derk Terk", 4, PK | TR | PS, 0, .90f, .95f, 0.7f, "Rela" },
        { I::Tabla, "D.n. G.n. D.nk G.nk", 4, PK | UN | CU, CLB, .70f, .40f, 1.1f, "Club bayan on 2 & 4" },

        //------------------------------------------------------------------
        // DHOL    b Dagga (bass)  m Dagga muted  t Tilli  e Tilli edge  x Both
        //------------------------------------------------------------------
        { I::Dhol, "B..t .tb. B..t .tb.", 4, FO | PK | AF | CU, 0, .60f, .40f, 1.2f, "Chaal" },
        { I::Dhol, "B.tt .tb. Bt.t .tbt", 4, FO | PK, 0, .80f, .60f, 1.0f, "Chaal, busy" },
        { I::Dhol, "B.t. b.t. B.t. b.te", 4, PK | TR | UN | CU, CLB, .85f, .30f, 1.2f, "Four on the floor" },
        { I::Dhol, "X.tb B.tb X.tb B.tt", 4, PK | TR, CLB, .95f, .50f, 1.0f, "Peak drive" },
        { I::Dhol, "B..b ..B. m.b. B.mm", 4, TR | AF, TRE, .75f, .50f, 1.1f, "Tom ritual 3-3-2" },
        { I::Dhol, "B..t ..b. ..t. b..t", 4, AF | MI | PS, TRE, .55f, .45f, 1.1f, "Afro skip" },
        { I::Dhol, "Bm.. t.m. Bm.. t.mt", 4, UN | PK, 0, .70f, .50f, 1.0f, "Underground thud" },
        { I::Dhol, "B... .... b... t...", 4, MI | PS | ME, 0, .30f, .20f, 1.0f, "Minimal" },
        { I::Dhol, "B..t ..t. b..t ..tt", 4, PS | AF | ME, 0, .55f, .50f, 1.0f, "Psych swing" },
        { I::Dhol, "X..b X.b. X..b Xtbt", 4, TR | PK, 0, .95f, .60f, 1.0f, "Ritual stomp" },
        { I::Dhol, "B.tb.tB.tb.t", 3, AF | TR | ME, DAD, .60f, .45f, 1.1f, "12/8 roll" },
        { I::Dhol, "B..b..B..m.t", 3, ME | AF | PS, DAD, .45f, .35f, 1.0f, "12/8 sparse" },
        { I::Dhol, "B..t .bB. .t.b .tt.", 4, UN | TR | PS, RUP, .65f, .50f, 1.1f, "Rupak drive" },
        { I::Dhol, "B.tb B.tb m.t. B.tt", 4, TR | PK, TEE, .75f, .50f, 1.0f, "Teentaal vibhags" },

        //------------------------------------------------------------------
        // DHOLAK  g Ge  s Ge slide  n Na  t Ta  c Chat
        //------------------------------------------------------------------
        { I::Dholak, "g.tn .tg. g.tn .tc.", 4, FO | AF | CU, 0, .65f, .45f, 1.2f, "Folk theka" },
        { I::Dholak, "g..n .tg. s..n .tn.", 4, AF | PS | FO, TRE, .55f, .50f, 1.1f, "Afro slide" },
        { I::Dholak, "g.tn g.tn g.tn g.tn", 4, PK | TR | FO, CLB, .85f, .50f, 1.0f, "Peak gallop" },
        { I::Dholak, "s..n ..t. s..n .tt.", 4, PS | ME | MI, 0, .45f, .50f, 1.0f, "Gumki swing" },
        { I::Dholak, "g.ntnsg.ntnt", 3, ME | AF | FO, DAD, .55f, .50f, 1.1f, "Dadra lilt" },
        { I::Dholak, "..tg ..tg ..tg .cts", 4, UN | PK, 0, .60f, .55f, 1.0f, "Offbeat bass" },
        { I::Dholak, "g... ..t. ..g. ..t.", 4, MI | ME | PS, 0, .30f, .20f, 1.0f, "Minimal" },
        { I::Dholak, "G.tn g.tn G.tn c.tn", 4, FO | ME | CU, KEH, .60f, .45f, 1.1f, "Keherwa dholak" },
        { I::Dholak, "g.tn .n.g .tn. n.ct", 4, UN | PS, RUP, .55f, .50f, 1.0f, "Rupak dholak" },

        //------------------------------------------------------------------
        // MANJIRA  o Open  c Closed
        //------------------------------------------------------------------
        { I::Manjira, "..o. ..o. ..o. ..o.", 4, ALL, 0, .50f, .20f, 1.2f, "Offbeat ring" },
        { I::Manjira, "cco. cco. cco. cco.", 4, PK | TR | FO | UN, 0, .75f, .45f, 1.0f, "Gallop" },
        { I::Manjira, "o.c. o.c. o.c. o.cc", 4, FO | TR | ME, 0, .65f, .35f, 1.0f, "Folk pulse" },
        { I::Manjira, "c.oc.oc.oc.o", 3, AF | ME | TR, DAD, .55f, .40f, 1.0f, "Triplet ring" },
        { I::Manjira, ".... ..o. .... ..o.", 4, MI | PS | ME, 0, .25f, .15f, 1.0f, "Sparse ring" },
        { I::Manjira, "o..c ..o. .c.. o.c.", 4, TR | AF, TRE, .70f, .45f, 1.0f, "3-3-2 ring" },
        { I::Manjira, "..c. o.c. ..c. o.cc", 4, UN | PS, 0, .60f, .55f, 1.0f, "Broken" },

        //------------------------------------------------------------------
        // GHUNGROO  s Shake  l Long shake  a Stamp
        //------------------------------------------------------------------
        { I::Ghungroo, "s,S, s,S, s,S, s,S,", 4, AF | PK | FO | TR | CU, 0, .70f, .40f, 1.3f, "Shaker 16ths" },
        { I::Ghungroo, "s.S. s.S. s.S. s.S.", 4, AF | MI | PS | ME | UN, 0, .50f, .20f, 1.1f, "Shaker 8ths" },
        { I::Ghungroo, "l... .... l... ....", 4, PS | ME | MI, 0, .30f, .20f, 1.0f, "Swells" },
        { I::Ghungroo, "a... .... a... s.s.", 4, TR | FO | UN, 0, .55f, .35f, 1.0f, "Dancer's stamp" },
        { I::Ghungroo, "s,Ss,Ss,Ss,S", 3, AF | ME | TR, DAD, .60f, .45f, 1.1f, "Triplet shaker" },
        { I::Ghungroo, "..l. ..l. ..l. ..l.", 4, PS | MI | AF, 0, .45f, .30f, 1.0f, "Offbeat swell" },

        //------------------------------------------------------------------
        // TAALI  c Clap  g Group clap
        //------------------------------------------------------------------
        { I::Taali, ".... c... .... c...", 4, PK | UN | CU | MI, CLB, .50f, .15f, 1.1f, "Backbeat" },
        { I::Taali, ".... g... .... g...", 4, FO | TR | PK, 0, .55f, .15f, 1.0f, "Group backbeat" },
        { I::Taali, ".... g..c .... g.c.", 4, FO | AF, 0, .65f, .35f, 1.0f, "Bhangra claps" },
        { I::Taali, "...c ..c. ..c. ..c.", 4, AF | TR, TRE, .55f, .45f, 1.0f, "Afro claps" },
        { I::Taali, "c..c ..c. c..c ..c.", 4, TR | AF, 0, .75f, .50f, 1.0f, "Ritual claps" },
        { I::Taali, "g..c..g..c..", 3, ME | AF | TR, DAD, .50f, .35f, 1.0f, "Dadra claps" },
        { I::Taali, "c..c .c.c ..c. c.c.", 4, UN | PS, RUP, .55f, .50f, 1.0f, "Rupak claps" },
        { I::Taali, ".... .... .... c...", 4, MI | PS | ME, 0, .20f, .10f, 1.0f, "Single clap" },
    };
    return cells;
}

const std::vector<MelodyRhythmCell>& melodyRhythmCells()
{
    static const std::vector<MelodyRhythmCell> cells =
    {
        // Riffs (tumbi, hypnotic leads)
        { "X.xx X.x. X.xx X.x.", 4, FO | PK, true, .80f },
        { "X..x ..x. ..x. .x..", 4, AF | PS | ME | CU, true, .55f },
        { "X... x.x. ..x. ..x.", 4, UN | MI | PK, true, .50f },
        { "x... .... x... ..x.", 4, MI | PS, true, .30f },
        { "Xxxx X.x. Xxxx X.xx", 4, FO | PK, true, .90f },
        { "X.xX.xX.xX.x", 3, AF | ME | TR, true, .60f },
        { "X..X ..X. X..X ..X.", 4, AF | PK | FO | TR | CU, true, .65f },
        { "..x. ..x. ..x. .xX.", 4, UN | MI | PS, true, .45f },

        // Phrases (sitar, bansuri)
        { "x--- x-x- x--- x---", 4, ME | PS | CU | TR, false, .40f },
        { "x.x. x--- x.x. x---", 4, PS | FO | CU, false, .55f },
        { "x--- ---- x--- x-x-", 4, ME | TR | MI, false, .30f },
        { "..x- x.x- x--- ..x-", 4, AF | PS | UN, false, .55f },
        { "x-x- xxx- x--- x-x-", 4, FO | PK | PS, false, .70f },
        { "x--- ---- --x- x-x-", 4, ME | MI | TR, false, .30f },
        { "x--x-xx--x--", 3, ME | AF, false, .45f },
        { "x.xx x-.. x.xx x-..", 4, PS | FO | AF, false, .65f },
    };
    return cells;
}

juce::String stripNotation (const char* pattern)
{
    juce::String s (pattern);
    return s.removeCharacters (" |");
}

int cellBars (const PatternCell& cell)
{
    const int steps = stripNotation (cell.pattern).length();
    return juce::jmax (1, steps / (cell.stepsPerBeat * 4));
}

std::vector<ParsedStep> parseCell (const PatternCell& cell)
{
    const auto& info = instrumentInfo (cell.instrument);
    const auto p = stripNotation (cell.pattern);

    std::vector<ParsedStep> steps ((size_t) p.length());
    int ghostAlt = 0;

    for (int i = 0; i < p.length(); ++i)
    {
        const auto c = p[i];
        auto& st = steps[(size_t) i];

        if (c == '.')
            continue;

        if (c == ',')
        {
            st.hit = true;
            st.dyn = Dynamic::Ghost;
            st.articulation = ghostArticulation (cell.instrument, ghostAlt++);
            continue;
        }

        if (c == '~')
        {
            st.hit = true;
            st.roll = true;
            st.articulation = ghostArticulation (cell.instrument, 0);
            continue;
        }

        const auto lower = (char) juce::CharacterFunctions::toLowerCase (c);
        for (size_t a = 0; a < info.patternChars.size(); ++a)
        {
            if (info.patternChars[a] == lower)
            {
                st.hit = true;
                st.articulation = (int) a;
                st.dyn = juce::CharacterFunctions::isUpperCase (c) ? Dynamic::Accent : Dynamic::Normal;
                break;
            }
        }

        jassert (st.hit); // unknown notation character
    }

    return steps;
}

} // namespace jgl

#include "Scales.h"

#include <algorithm>

namespace jgl
{

static ScaleProfile western (const char* name, const char* tag, std::vector<int> notes, int samvadi = 7, const char* desc = "")
{
    ScaleProfile s;
    s.name = name;
    s.fileTag = tag;
    s.notes = notes;
    s.aroha = notes;
    s.avaroha = notes;
    s.vadi = 0;
    s.samvadi = std::find (notes.begin(), notes.end(), samvadi) != notes.end() ? samvadi : notes[notes.size() / 2];
    s.tanpuraString = std::find (notes.begin(), notes.end(), 7) != notes.end() ? -5 : -7;
    s.description = desc;
    return s;
}

static ScaleProfile raga (const char* name, const char* tag,
                          std::vector<int> notes, std::vector<int> aroha, std::vector<int> avaroha,
                          int vadi, int samvadi, std::vector<std::vector<int>> phrases,
                          int tanpura, const char* desc)
{
    ScaleProfile s;
    s.name = name;
    s.fileTag = tag;
    s.ragaInspired = true;
    s.notes = notes;
    s.aroha = aroha;
    s.avaroha = avaroha;
    s.vadi = vadi;
    s.samvadi = samvadi;
    s.phrases = phrases;
    s.tanpuraString = tanpura;
    s.description = desc;
    return s;
}

static std::vector<ScaleProfile> buildScales()
{
    std::vector<ScaleProfile> v;

    // Western / modal ------------------------------------------------------
    v.push_back (western ("Major",             "maj",     { 0, 2, 4, 5, 7, 9, 11 }));
    v.push_back (western ("Natural Minor",     "min",     { 0, 2, 3, 5, 7, 8, 10 }));
    v.push_back (western ("Harmonic Minor",    "hmin",    { 0, 2, 3, 5, 7, 8, 11 }));
    v.push_back (western ("Dorian",            "dor",     { 0, 2, 3, 5, 7, 9, 10 }));
    v.push_back (western ("Phrygian",          "phr",     { 0, 1, 3, 5, 7, 8, 10 }));
    v.push_back (western ("Phrygian Dominant", "phrdom",  { 0, 1, 4, 5, 7, 8, 10 }));
    v.push_back (western ("Mixolydian",        "mix",     { 0, 2, 4, 5, 7, 9, 10 }));
    v.push_back (western ("Major Pentatonic",  "majpent", { 0, 2, 4, 7, 9 }));
    v.push_back (western ("Minor Pentatonic",  "minpent", { 0, 3, 5, 7, 10 }));

    // Raga-inspired profiles ------------------------------------------------
    // Phrases are short characteristic movements (semitones from Sa).
    v.push_back (raga ("Bhairav-inspired", "Bhairav",
                       { 0, 1, 4, 5, 7, 8, 11 },
                       { 0, 1, 4, 5, 7, 8, 11 },
                       { 0, 1, 4, 5, 7, 8, 11 },
                       8, 1,
                       { { 4, 5, 8, 8, 7 }, { 4, 5, 1, 1, 0 }, { 11, 12, 8, 7 } },
                       -5, "Komal Re & Dha, shuddha Ga & Ni. Heavy emphasis on komal Dha and Re."));

    v.push_back (raga ("Bhairavi-inspired", "Bhairavi",
                       { 0, 1, 3, 5, 7, 8, 10 },
                       { 0, 1, 3, 5, 7, 8, 10 },
                       { 0, 1, 3, 5, 7, 8, 10 },
                       5, 0,
                       { { 5, 3, 1, 0 }, { 7, 8, 7, 5, 3 }, { 10, 8, 7 } },
                       -7, "All komal swaras (Re, Ga, Dha, Ni). Expressive, devotional colour."));

    v.push_back (raga ("Yaman-inspired", "Yaman",
                       { 0, 2, 4, 6, 7, 9, 11 },
                       { 0, 2, 4, 6, 9, 11 },          // Pa usually skipped going up
                       { 0, 2, 4, 6, 7, 9, 11 },
                       4, 11,
                       { { -1, 2, 4 }, { 6, 4, 2, 0 }, { 4, 6, 9, 11, 12 } },
                       -5, "Tivra Ma. Ascent avoids Pa; phrases often start from low Ni."));

    v.push_back (raga ("Kafi-inspired", "Kafi",
                       { 0, 2, 3, 5, 7, 9, 10 },
                       { 0, 2, 3, 5, 7, 9, 10 },
                       { 0, 2, 3, 5, 7, 9, 10 },
                       7, 0,
                       { { 0, 2, 3, 5, 7 }, { 10, 9, 7, 5, 3, 2 }, { 7, 5, 3, 2, 0 } },
                       -5, "Komal Ga and Ni (Dorian colour). Folk and semi-classical feel."));

    v.push_back (raga ("Bhupali-inspired", "Bhupali",
                       { 0, 2, 4, 7, 9 },
                       { 0, 2, 4, 7, 9 },
                       { 0, 2, 4, 7, 9 },
                       4, 9,
                       { { 4, 2, 0, -3, 0 }, { 7, 9, 12, 9, 7 }, { 4, 7, 4, 2, 0 } },
                       -5, "Pentatonic (Sa Re Ga Pa Dha). Bright and open."));

    v.push_back (raga ("Malkauns-inspired", "Malkauns",
                       { 0, 3, 5, 8, 10 },
                       { 0, 3, 5, 8, 10 },
                       { 0, 3, 5, 8, 10 },
                       5, 0,
                       { { 5, 3, 5, 8, 5 }, { 10, 8, 5, 3, 0 }, { -2, 0, 3, 5 } },
                       -7, "Pentatonic without Re and Pa. Deep, hypnotic, late-night colour."));

    v.push_back (raga ("Desh-inspired", "Desh",
                       { 0, 2, 4, 5, 7, 9, 10, 11 },
                       { 0, 2, 5, 7, 11 },              // S R M P N S'
                       { 0, 2, 4, 5, 7, 9, 10 },         // S' n D P M G R S
                       2, 7,
                       { { 2, 5, 7, 11, 12 }, { 10, 9, 7, 5, 4, 2 }, { 2, 4, 2, 0 } },
                       -5, "Shuddha Ni ascending, komal Ni descending. Folk, monsoon character."));

    v.push_back (raga ("Todi-inspired", "Todi",
                       { 0, 1, 3, 6, 7, 8, 11 },
                       { 0, 1, 3, 6, 8, 11 },            // Pa weak in ascent
                       { 0, 1, 3, 6, 7, 8, 11 },
                       8, 3,
                       { { 1, 3, 1, 0 }, { 6, 8, 6, 3 }, { 8, 11, 12 } },
                       -1, "Komal Re/Ga/Dha with tivra Ma. Intense and dramatic."));

    v.push_back (raga ("Charukeshi-inspired", "Charukeshi",
                       { 0, 2, 4, 5, 7, 8, 10 },
                       { 0, 2, 4, 5, 7, 8, 10 },
                       { 0, 2, 4, 5, 7, 8, 10 },
                       7, 2,
                       { { 7, 8, 7, 5, 4 }, { 10, 8, 7 }, { 2, 4, 5, 7 } },
                       -5, "Major lower half, minor upper half (komal Dha & Ni). Bittersweet."));

    return v;
}

const std::vector<ScaleProfile>& scaleProfiles()
{
    static const std::vector<ScaleProfile> s = buildScales();
    return s;
}

int numScales() { return (int) scaleProfiles().size(); }

const ScaleProfile& scaleProfile (int index)
{
    auto& s = scaleProfiles();
    return s[(size_t) juce::jlimit (0, (int) s.size() - 1, index)];
}

const juce::StringArray& keyNames()
{
    static const juce::StringArray k { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return k;
}

static bool containsPc (const std::vector<int>& set, int semis)
{
    const int pc = pitchClass (semis);
    return std::find (set.begin(), set.end(), pc) != set.end();
}

bool scaleContains (const ScaleProfile& s, int semis)   { return containsPc (s.notes, semis); }
bool arohaContains (const ScaleProfile& s, int semis)   { return containsPc (s.aroha, semis); }
bool avarohaContains (const ScaleProfile& s, int semis) { return containsPc (s.avaroha, semis); }

std::vector<int> scalePitchesInRange (const ScaleProfile& s, int lo, int hi)
{
    std::vector<int> out;
    for (int p = lo; p <= hi; ++p)
        if (scaleContains (s, p))
            out.push_back (p);
    return out;
}

int stepInScale (const ScaleProfile& s, int fromPitch, int steps)
{
    int p = fromPitch;
    const int dir = steps >= 0 ? 1 : -1;
    for (int n = 0; n < std::abs (steps); ++n)
    {
        do { p += dir; } while (! scaleContains (s, p));
    }
    return p;
}

juce::String keyLabel (int keyRoot, int scaleIndex)
{
    auto name = keyNames()[pitchClass (keyRoot)].replace ("#", "#");
    return name + scaleProfile (scaleIndex).fileTag;
}

} // namespace jgl

#pragma once

#include <vector>
#include <juce_core/juce_core.h>

namespace jgl
{

/**
    A scale or a *raga-inspired* scale profile.

    Raga-inspired profiles are deliberately simplified: they model the note set,
    separate ascending (aroha) / descending (avaroha) note sets, emphasised notes
    (vadi / samvadi) and a few characteristic movements. They are NOT complete
    classical raga grammars (no time-of-day rules, chalan, specific shrutis, etc.),
    and the UI labels them as "-inspired" accordingly.

    All pitches are semitones relative to the tonic (Sa).
*/
struct ScaleProfile
{
    juce::String name;          // display name
    juce::String fileTag;       // used in export file names, e.g. "min" -> "Dmin"
    bool ragaInspired = false;
    std::vector<int> notes;     // pitch classes, ascending, includes 0
    std::vector<int> aroha;     // pitch classes allowed when moving up
    std::vector<int> avaroha;   // pitch classes allowed when moving down
    int vadi = 0;               // most emphasised note
    int samvadi = 7;            // second most emphasised note
    std::vector<std::vector<int>> phrases;  // characteristic movements (semitones, may span octaves)
    int tanpuraString = -5;     // first tanpura string relative to Sa (Pa = -5, Ma = -7, Ni = -1)
    juce::String description;
};

const std::vector<ScaleProfile>& scaleProfiles();
const ScaleProfile& scaleProfile (int index);
int numScales();

const juce::StringArray& keyNames();   // C, C#, ... B

inline int pitchClass (int semis) noexcept { return ((semis % 12) + 12) % 12; }

bool scaleContains (const ScaleProfile& s, int semis);
bool arohaContains (const ScaleProfile& s, int semis);
bool avarohaContains (const ScaleProfile& s, int semis);

/** All scale pitches (semitones from Sa) inside [lo, hi]. */
std::vector<int> scalePitchesInRange (const ScaleProfile& s, int lo, int hi);

/** Pitch reached by moving `steps` scale degrees from `fromPitch` (which must be in the scale). */
int stepInScale (const ScaleProfile& s, int fromPitch, int steps);

/** Short key label used for file names, e.g. "Dmin", "F#Bhairav". */
juce::String keyLabel (int keyRoot, int scaleIndex);

} // namespace jgl

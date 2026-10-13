#pragma once

#include <array>
#include "MusicTypes.h"

namespace jgl
{

//==============================================================================
/** Generation personalities. These change the actual algorithms' inputs
    (pattern vocabulary, microtiming, phrase rules, melodic behaviour), not just a name. */
enum class StyleId : int
{
    AfroIndian = 0,
    TribalRitual,
    IndianUnderground,
    PsychedelicIndia,
    MelodicIndian,
    PeakTimePercussion,
    MinimalHypnosis,
    FolkElectronica,
    Custom,
    Count
};

constexpr int numStyles = (int) StyleId::Count;

/** Bit mask used to tag pattern-library cells with the styles they belong to. */
inline uint32_t styleBit (StyleId s) noexcept { return 1u << (uint32_t) s; }
constexpr uint32_t allStylesMask = 0x1FFu;

//==============================================================================
/** Rhythmic frameworks inspired by Indian taals, adapted to a 4/4 club grid. */
enum class FrameworkId : int
{
    StyleDefault = 0,
    Keherwa,     // 8 matras
    Dadra,       // 6 matras (triplet feel)
    Teentaal,    // 16 matras
    Rupak,       // 7 matras (3+2+2)
    Club,        // modern four-on-the-floor
    Tresillo,    // 3+3+2 syncopation
    Count
};

constexpr int numFrameworks = (int) FrameworkId::Count;

struct Framework
{
    const char* name;
    const char* description;
    int matras;
    bool triplet;                      // natural grid is triplet 8ths (12 per bar)
    std::array<float, 16> accent16;    // where accents gravitate on a straight 16th grid
    std::array<float, 12> accent12;    // same for the triplet grid
    std::vector<int> groupStarts16;    // grouping boundaries (asymmetric feels)
};

const Framework& framework (FrameworkId id);
juce::StringArray frameworkNames();

//==============================================================================
struct MelodyBehaviour
{
    float noteDensity;     // 0..1 notes per available slot
    float legato;          // 0..1 how much notes sustain into the next
    float ornamentChance;  // meend / kan / murki probability
    float leapiness;       // 0..1 willingness to leap
    float restChance;      // chance of rests inside phrases
    int rangeSemis;        // melodic span above Sa
    bool hypnoticRiff;     // repeat a short cell rather than develop a phrase
};

struct StyleProfile
{
    StyleId id;
    const char* name;
    const char* fileTag;
    const char* description;
    FrameworkId defaultFramework;
    double defaultBpm;

    // Default knob positions (applied to the UI when the style is selected)
    float complexity, swing, groove, humanize, density, energy, variation, ghost, accent;
    float reverb, delay, saturation;
    int suggestedScale;
    std::vector<InstrumentId> suggestedEnsemble;

    // Groove: microtiming per 16th position within a beat (fraction of a 16th, + = late)
    std::array<float, 4> microTiming;
    std::array<float, 4> velocityShape;

    // Pattern behaviour
    float polymeterChance;  // supporting percussion uses a polymetric cycle (3/5/7 over 16)
    float tripletChance;    // supporting percussion picks a triplet-grid pattern
    float fillIntensity;    // size of phrase-end fills
    float repetition;       // hypnotic repetition: 1 = bars almost identical
    float rollChance;       // rolls / tirakita inside the groove

    MelodyBehaviour melody;
};

const StyleProfile& styleProfile (StyleId id);
const StyleProfile& styleProfile (int index);
juce::StringArray styleNames();

/** Resolves "Style default" to the style's own framework. */
FrameworkId resolveFramework (int styleIndex, int frameworkIndex);

} // namespace jgl

#pragma once

#include "../Core/Instruments.h"
#include "../Core/Styles.h"

namespace jgl
{

/**
    Hand-designed rhythmic vocabulary.

    Each percussion cell is a one- or two-bar pattern written in a compact notation:
      - one character per grid step (16ths when stepsPerBeat = 4, triplet 8ths when 3)
      - an instrument letter = a stroke / articulation (see InstrumentInfo::patternChars)
      - UPPERCASE letter = accented stroke, lowercase = normal
      - ',' = ghost note, '~' = roll on that step, '.' = rest
      - spaces and '|' are ignored (readability only)

    The generator never plays these verbatim: it selects cells by style, framework,
    energy and complexity, then mutates, thins, embellishes and arranges them into
    phrases (see RhythmGenerator). They exist so that the *skeleton* of every groove
    carries real rhythmic intent.
*/
struct PatternCell
{
    InstrumentId instrument;
    const char* pattern;
    int stepsPerBeat;
    uint32_t styles;       // bit mask of StyleId
    uint32_t frameworks;   // bit mask of FrameworkId (0 = framework-neutral)
    float energy;
    float complexity;
    float weight;
    const char* label;
};

/** Melodic rhythm cell: 'x' onset ('X' accented), '-' sustain, '.' rest. */
struct MelodyRhythmCell
{
    const char* pattern;
    int stepsPerBeat;
    uint32_t styles;
    bool riff;           // suited to short repeating riffs (tumbi)
    float energy;
};

struct ParsedStep
{
    bool hit = false;
    int articulation = 0;
    Dynamic dyn = Dynamic::Normal;
    bool roll = false;
};

const std::vector<PatternCell>& percussionCells();
const std::vector<MelodyRhythmCell>& melodyRhythmCells();

juce::String stripNotation (const char* pattern);
std::vector<ParsedStep> parseCell (const PatternCell& cell);
int cellBars (const PatternCell& cell);

inline uint32_t frameworkBit (FrameworkId f) noexcept { return 1u << (uint32_t) f; }

} // namespace jgl

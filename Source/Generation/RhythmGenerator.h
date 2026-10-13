#pragma once

#include "GenerationContext.h"
#include "PatternLibrary.h"

namespace jgl
{

/**
    Percussion part generator.

    Pipeline per part:
      1. choose a skeleton cell from the pattern library (style, framework, energy, complexity)
         - or build a polymetric cycle for texture instruments (3/5/7 against 16)
      2. embellish it: density thinning, ghost notes that anticipate strong hits, rolls
         (using fixed per-step random draws so knob changes morph rather than reshuffle)
      3. arrange it into a phrase: A A' A F, with second-half variants, mid-phrase
         pickups and phrase-end fills whose size follows energy and the style
      4. convert to grid events with composed dynamics
*/
class RhythmGenerator
{
public:
    static Part generate (InstrumentId instrument, uint64_t seed, const GenerationContext& ctx);

    struct Step
    {
        bool hit = false;
        int articulation = 0;
        Dynamic dyn = Dynamic::Normal;
        bool roll = false;
        float velocity = -1.0f;   // < 0: derive from dynamic and metric position
    };
    using Bar = std::vector<Step>;
};

} // namespace jgl

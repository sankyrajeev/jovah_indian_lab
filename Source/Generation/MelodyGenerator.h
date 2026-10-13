#pragma once

#include "GenerationContext.h"
#include "PatternLibrary.h"

namespace jgl
{

/**
    Melodic part generator (sitar, bansuri, tumbi, tanpura).

    - Every pitch is drawn from the selected scale profile. For raga-inspired profiles,
      upward motion only uses aroha notes and downward motion only avaroha notes, and the
      vadi / samvadi are emphasised on strong beats.
    - Phrases are built as call (ends open, on Pa / samvadi / Re) and response (resolves to Sa),
      with the response re-using the call's rhythm and opening so the phrase is motivic.
    - With two lead instruments the lead plays the calls and the other answers.
    - Riffs (tumbi, hypnotic styles) repeat a short hook with phrase-end variations.
    - Ornaments: meend (glides) and murki for sitar, kan (grace notes) for bansuri,
      slide-ins for tumbi.
*/
class MelodyGenerator
{
public:
    static Part generate (InstrumentId instrument, uint64_t seed, const GenerationContext& ctx);

    /** Instrument range relative to Sa, used by the generator and validated by tests. */
    static juce::Range<int> pitchRange (InstrumentId instrument, const GenerationContext& ctx);
};

} // namespace jgl

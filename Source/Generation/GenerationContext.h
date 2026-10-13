#pragma once

#include "../Core/Instruments.h"
#include "../Core/Scales.h"
#include "../Core/Settings.h"
#include "../Core/Styles.h"

namespace jgl
{

/** Ensemble roles assigned per generation, so parts interact sensibly. */
enum class MelodicRole
{
    None,
    Lead,      // develops call phrases (and answers itself when alone)
    Answer,    // answers the lead in the response segments
    Riff,      // short repeating hook
    Drone
};

/** Read-only context shared by all part generators of one arrangement. */
struct GenerationContext
{
    ComposeSettings settings;
    const StyleProfile* style = nullptr;
    const ScaleProfile* scale = nullptr;
    FrameworkId framework = FrameworkId::Keherwa;
    int numPercussion = 0;
    int numMelodic = 0;

    static GenerationContext make (const ComposeSettings& cs)
    {
        GenerationContext c;
        c.settings = cs;
        c.style = &styleProfile (cs.styleIndex);
        c.scale = &scaleProfile (cs.scaleIndex);
        c.framework = resolveFramework (cs.styleIndex, cs.frameworkIndex);
        for (auto id : allInstruments())
        {
            if (! cs.isEnabled (id)) continue;
            if (isPercussion (id)) ++c.numPercussion;
            else                   ++c.numMelodic;
        }
        return c;
    }

    bool has (InstrumentId id) const noexcept { return settings.isEnabled (id); }

    MelodicRole roleFor (InstrumentId id) const noexcept
    {
        switch (id)
        {
            case InstrumentId::Tanpura: return MelodicRole::Drone;
            case InstrumentId::Tumbi:   return MelodicRole::Riff;
            case InstrumentId::CustomVoice: return MelodicRole::Lead;
            case InstrumentId::Nadaswaram: return has (InstrumentId::CustomVoice) ? MelodicRole::Answer : MelodicRole::Lead;
            case InstrumentId::Sitar:      return (has (InstrumentId::Nadaswaram) || has (InstrumentId::CustomVoice)) ? MelodicRole::Answer : MelodicRole::Lead;
            case InstrumentId::Bansuri:    return (has (InstrumentId::Sitar) || has (InstrumentId::Nadaswaram) || has (InstrumentId::CustomVoice))
                                                      ? MelodicRole::Answer : MelodicRole::Lead;
            default:                       return MelodicRole::None;
        }
    }

    /** Answering instruments take turns: returns this instrument's turn index and the number of answerers. */
    std::pair<int, int> answerSlot (InstrumentId id) const noexcept
    {
        int slot = 0, count = 0;
        for (auto a : { InstrumentId::Nadaswaram, InstrumentId::Sitar, InstrumentId::Bansuri })
            if (has (a) && roleFor (a) == MelodicRole::Answer)
            {
                if (a == id) slot = count;
                ++count;
            }
        return { slot, count };
    }
};

} // namespace jgl

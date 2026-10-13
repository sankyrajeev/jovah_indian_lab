#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "Core/Instruments.h"
#include "Core/Scales.h"
#include "Core/Settings.h"
#include "Core/Styles.h"
#include "DSP/LoopRenderer.h"
#include "Engine/GenerationService.h"
#include "Generation/Arranger.h"

namespace jgl::test
{

inline ComposeSettings compose (int style, std::initializer_list<InstrumentId> instruments, int bars = 4, int scale = 1)
{
    ComposeSettings cs;
    const auto& s = styleProfile (style);
    cs.styleIndex = style;
    cs.bars = bars;
    cs.scaleIndex = scale;
    cs.complexity = s.complexity;
    cs.density = s.density;
    cs.energy = s.energy;
    cs.variation = s.variation;
    for (auto id : instruments)
        cs.enabled[(size_t) toIndex (id)] = true;
    return cs;
}

inline ComposeSettings composeAll (int style, int bars = 4, int scale = 1)
{
    auto cs = compose (style, {}, bars, scale);
    cs.enabled.fill (true);
    return cs;
}

inline RenderSettings render (double bpm = 124.0, double sr = 48000.0, int key = 2)
{
    RenderSettings rs;
    rs.bpm = bpm;
    rs.sampleRate = sr;
    rs.keyRoot = key;
    return rs;
}

inline bool sameEvents (const Part& a, const Part& b)
{
    if (a.events.size() != b.events.size()) return false;
    for (size_t i = 0; i < a.events.size(); ++i)
    {
        const auto& x = a.events[i];
        const auto& y = b.events[i];
        if (x.beat != y.beat || x.articulation != y.articulation || x.pitch != y.pitch
            || x.velocity != y.velocity || x.dyn != y.dyn || x.seed != y.seed || x.length != y.length)
            return false;
    }
    return true;
}

inline bool sameArrangement (const Arrangement& a, const Arrangement& b)
{
    if (a.parts.size() != b.parts.size()) return false;
    for (size_t i = 0; i < a.parts.size(); ++i)
        if (a.parts[i].instrument != b.parts[i].instrument || ! sameEvents (a.parts[i], b.parts[i]))
            return false;
    return true;
}

inline bool hasNonFinite (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (c, i)))
                return true;
    return false;
}

inline std::shared_ptr<LoopResult> renderLoop (const ComposeSettings& cs, const RenderSettings& rs, uint64_t seed)
{
    GenerationService::Request req;
    req.compose = cs;
    req.render = rs;
    req.seeds = SeedSet::fromMaster (seed);
    return GenerationService::renderNow (req, nullptr);
}

} // namespace jgl::test

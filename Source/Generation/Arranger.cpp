#include "Arranger.h"
#include "MelodyGenerator.h"
#include "RhythmGenerator.h"

namespace jgl
{

Part Arranger::composePart (InstrumentId id, uint64_t seed, const GenerationContext& ctx)
{
    if (isPercussion (id))
        return RhythmGenerator::generate (id, seed, ctx);
    return MelodyGenerator::generate (id, seed, ctx);
}

Arrangement Arranger::compose (const ComposeSettings& settings, const SeedSet& seeds)
{
    auto cs = settings;
    cs.bars = juce::jlimit (1, 8, cs.bars);
    if (cs.bars == 3) cs.bars = 4;
    if (cs.bars > 4) cs.bars = 8;

    const auto ctx = GenerationContext::make (cs);

    Arrangement a;
    a.bars = cs.bars;
    a.scaleIndex = cs.scaleIndex;
    a.styleIndex = cs.styleIndex;
    a.frameworkIndex = (int) ctx.framework;

    for (auto id : allInstruments())
    {
        if (! cs.isEnabled (id))
            continue;

        auto part = composePart (id, seeds.partSeeds[(size_t) toIndex (id)], ctx);
        a.parts.push_back (std::move (part));
    }

    return a;
}

} // namespace jgl

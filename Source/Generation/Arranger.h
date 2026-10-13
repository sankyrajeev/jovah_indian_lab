#pragma once

#include "GenerationContext.h"

namespace jgl
{

/**
    Builds an Arrangement from compose settings and per-part seeds.

    Each part is a pure function of (instrument, its seed, the settings and the set of
    enabled instruments). Therefore:
      - regenerating one part (new seed for that part) leaves every other part identical
      - locking a part (keeping its seed) preserves it exactly across new generations
      - "new melody" / "new rhythm" just reseed the melodic / percussion families
      - any loop can be reproduced from its SeedSet + settings
*/
class Arranger
{
public:
    static Arrangement compose (const ComposeSettings& settings, const SeedSet& seeds);
    static Part composePart (InstrumentId id, uint64_t seed, const GenerationContext& ctx);
};

} // namespace jgl

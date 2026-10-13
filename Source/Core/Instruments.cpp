#include "Instruments.h"
#include "Rng.h"

namespace jgl
{

static std::vector<InstrumentInfo> buildRegistry()
{
    using F = InstrumentFamily;
    std::vector<InstrumentInfo> r;

    r.push_back ({ InstrumentId::Tabla, "Tabla", "tabla", "Lead hand drums", F::Percussion, false,
                   -2.8f, 0.0f, 0.22f, 0.05f, 0.7f,
                   { "Dha", "Dhin", "Ge", "Ka", "Na", "Tin", "Tun", "Te", "Re", "Ghe" },
                   { 'd', 'h', 'g', 'k', 'n', 't', 'u', 'e', 'r', 'o' } });

    r.push_back ({ InstrumentId::Dhol, "Dhol", "dhol", "Driving barrel drum", F::Percussion, false,
                   -0.6f, 0.0f, 0.12f, 0.0f, 1.0f,
                   { "Dagga", "DaggaMuted", "Tilli", "TilliEdge", "Both" },
                   { 'b', 'm', 't', 'e', 'x' } });

    r.push_back ({ InstrumentId::Dholak, "Dholak", "dholak", "Folk barrel drum", F::Percussion, false,
                   1.0f, -0.18f, 0.18f, 0.04f, 0.9f,
                   { "Ge", "GeSlide", "Na", "Ta", "Chat" },
                   { 'g', 's', 'n', 't', 'c' } });

    r.push_back ({ InstrumentId::Manjira, "Manjira", "manjira", "Hand cymbals", F::Percussion, false,
                   -8.0f, 0.32f, 0.25f, 0.12f, 0.8f,
                   { "Open", "Closed" },
                   { 'o', 'c' } });

    r.push_back ({ InstrumentId::Ghungroo, "Ghungroo", "ghungroo", "Ankle bells", F::Percussion, false,
                   -2.0f, -0.35f, 0.2f, 0.08f, 0.9f,
                   { "Shake", "LongShake", "Stamp" },
                   { 's', 'l', 'a' } });

    r.push_back ({ InstrumentId::Taali, "Taali", "taali", "Hand claps", F::Percussion, false,
                   3.0f, 0.12f, 0.3f, 0.1f, 1.0f,
                   { "Clap", "Group" },
                   { 'c', 'g' } });

    r.push_back ({ InstrumentId::Tumbi, "Tumbi", "tumbi", "Single-string riff", F::String, true,
                   5.0f, 0.22f, 0.18f, 0.28f, 0.6f,
                   { "Pluck" }, {} });

    r.push_back ({ InstrumentId::Sitar, "Sitar", "sitar", "Lead phrases, meend", F::String, true,
                   5.0f, -0.12f, 0.32f, 0.3f, 0.8f,
                   { "Pluck" }, {} });

    r.push_back ({ InstrumentId::Bansuri, "Bansuri", "bansuri", "Breathy flute lines", F::Wind, true,
                   3.0f, 0.1f, 0.4f, 0.35f, 0.9f,
                   { "Note" }, {} });

    r.push_back ({ InstrumentId::Tanpura, "Tanpura", "tanpura", "Drone bed", F::Drone, true,
                   0.0f, 0.0f, 0.35f, 0.0f, 1.0f,
                   { "Pluck" }, {} });

    r.push_back ({ InstrumentId::Nadaswaram, "Nadaswaram", "nadaswaram", "Temple double-reed lead", F::Wind, true,
                   0.0f, 0.05f, 0.38f, 0.28f, 0.9f,
                   { "Note" }, {} });

    return r;
}

static const std::vector<InstrumentInfo>& registry()
{
    static const std::vector<InstrumentInfo> r = buildRegistry();
    return r;
}

const InstrumentInfo& instrumentInfo (InstrumentId id)
{
    auto& r = registry();
    const int i = juce::jlimit (0, (int) r.size() - 1, toIndex (id));
    return r[(size_t) i];
}

const std::vector<InstrumentId>& allInstruments()
{
    static const std::vector<InstrumentId> ids = []
    {
        std::vector<InstrumentId> v;
        for (int i = 0; i < numInstruments; ++i)
            v.push_back (instrumentFromIndex (i));
        return v;
    }();
    return ids;
}

bool isPercussion (InstrumentId id) { return instrumentInfo (id).family == InstrumentFamily::Percussion; }
bool isMelodic (InstrumentId id)    { return instrumentInfo (id).melodic; }

int findInstrumentByKey (const juce::String& key)
{
    for (auto& info : registry())
        if (key.equalsIgnoreCase (info.key))
            return toIndex (info.id);
    return -1;
}

bool isBassArticulation (InstrumentId id, int a)
{
    switch (id)
    {
        case InstrumentId::Tabla:  return a == TablaArt::Dha || a == TablaArt::Dhin || a == TablaArt::Ge || a == TablaArt::Ghe;
        case InstrumentId::Dhol:   return a == DholArt::Dagga || a == DholArt::DaggaMuted || a == DholArt::Both;
        case InstrumentId::Dholak: return a == DholakArt::Ge || a == DholakArt::GeSlide;
        default:                   return false;
    }
}

int ghostArticulation (InstrumentId id, int alternate)
{
    switch (id)
    {
        case InstrumentId::Tabla:    return (alternate & 1) ? TablaArt::Re : TablaArt::Te;
        case InstrumentId::Dhol:     return DholArt::TilliEdge;
        case InstrumentId::Dholak:   return (alternate & 1) ? DholakArt::Chat : DholakArt::Ta;
        case InstrumentId::Manjira:  return ManjiraArt::Closed;
        case InstrumentId::Ghungroo: return GhungrooArt::Shake;
        case InstrumentId::Taali:    return TaaliArt::Clap;
        default:                     return 0;
    }
}

//==============================================================================
SeedSet SeedSet::fromMaster (uint64_t master)
{
    SeedSet s;
    s.masterSeed = master;
    for (int i = 0; i < numInstruments; ++i)
        s.partSeeds[(size_t) i] = hashCombine (master, 0x1000u + (uint64_t) i);
    return s;
}

juce::String SeedSet::toString() const
{
    juce::StringArray a;
    a.add (juce::String::toHexString ((juce::int64) masterSeed));
    for (auto s : partSeeds)
        a.add (juce::String::toHexString ((juce::int64) s));
    return a.joinIntoString (":");
}

bool SeedSet::fromString (const juce::String& text, SeedSet& out)
{
    juce::StringArray a;
    a.addTokens (text, ":", "");
    if (a.size() != numInstruments + 1)
        return false;

    SeedSet s;
    s.masterSeed = (uint64_t) a[0].getHexValue64();
    for (int i = 0; i < numInstruments; ++i)
        s.partSeeds[(size_t) i] = (uint64_t) a[i + 1].getHexValue64();
    out = s;
    return true;
}

} // namespace jgl

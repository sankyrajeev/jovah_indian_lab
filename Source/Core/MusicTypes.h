#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <juce_core/juce_core.h>

namespace jgl
{

//==============================================================================
enum class InstrumentId : int
{
    Tabla = 0,
    Dhol,
    Dholak,
    Manjira,
    Ghungroo,
    Taali,
    Tumbi,
    Sitar,
    Bansuri,
    Tanpura,
    Nadaswaram,
    Count
};

constexpr int numInstruments = (int) InstrumentId::Count;

inline int toIndex (InstrumentId id) noexcept { return (int) id; }
inline InstrumentId instrumentFromIndex (int i) noexcept { return (InstrumentId) i; }

//==============================================================================
/** Dynamic level of a hit as composed (render-time Accent/Ghost knobs shape these). */
enum class Dynamic : uint8_t
{
    Ghost = 0,
    Normal = 1,
    Accent = 2
};

/** Melodic ornament flags. */
enum Ornament : uint8_t
{
    OrnNone = 0,
    OrnKan = 1,      // grace note from the upper neighbour (bansuri)
    OrnMurki = 2,    // quick turn before the note (sitar)
    OrnSlideIn = 3,  // short slide into the note from below (tumbi)
    OrnGamaka = 4    // oscillation to the upper neighbour and back (nadaswaram)
};

//==============================================================================
/**
    One musical event on the loop's grid.

    Positions are in beats (quarter notes) from the loop start and are *pre-groove*:
    swing, microtiming templates and humanisation are applied by the GrooveEngine at
    render time, so changing swing does not change the composition.
*/
struct NoteEvent
{
    double beat = 0.0;          // grid position, beats from loop start
    double length = 0.25;       // beats (melodic duration; informational for percussion)
    float velocity = 0.8f;      // 0..1 composed dynamic
    Dynamic dyn = Dynamic::Normal;
    int articulation = 0;       // instrument-specific articulation / stroke
    int pitch = 0;              // melodic: semitones relative to the tonic (Sa)
    float glideFrom = 0.0f;     // melodic: start offset in semitones (meend / slide)
    float glideBeats = 0.0f;    // melodic: glide duration in beats
    uint8_t ornament = OrnNone;
    int ornamentPitch = 0;      // pitch of the grace note for kan/murki
    bool swingable = true;      // false for triplet-grid events
    uint32_t seed = 0;          // per-hit micro-variation & humanisation draws
};

struct Part
{
    InstrumentId instrument = InstrumentId::Tabla;
    uint64_t seed = 0;
    std::vector<NoteEvent> events;
    juce::String summary;   // short human-readable description of the pattern
};

/** The composed loop: a set of parts on a 4/4 grid. */
struct Arrangement
{
    int bars = 4;
    static constexpr double beatsPerBar = 4.0;
    int scaleIndex = 0;
    int styleIndex = 0;
    int frameworkIndex = 0;
    std::vector<Part> parts;

    double lengthBeats() const noexcept { return bars * beatsPerBar; }

    const Part* findPart (InstrumentId id) const noexcept
    {
        for (auto& p : parts)
            if (p.instrument == id)
                return &p;
        return nullptr;
    }

    int totalEvents() const noexcept
    {
        int n = 0;
        for (auto& p : parts)
            n += (int) p.events.size();
        return n;
    }
};

//==============================================================================
/** Per-instrument seeds + lock flags. Together with the settings this fully
    determines a loop, which is what makes loops reproducible. */
struct SeedSet
{
    std::array<uint64_t, (size_t) numInstruments> partSeeds {};
    uint64_t masterSeed = 0;

    bool operator== (const SeedSet& o) const noexcept { return partSeeds == o.partSeeds && masterSeed == o.masterSeed; }
    bool operator!= (const SeedSet& o) const noexcept { return ! (*this == o); }

    static SeedSet fromMaster (uint64_t master);

    juce::String toString() const;
    static bool fromString (const juce::String& text, SeedSet& out);
};

} // namespace jgl

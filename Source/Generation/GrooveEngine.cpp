#include "GrooveEngine.h"
#include "../Core/Rng.h"

namespace jgl
{

GrooveEngine::GrooveEngine (const RenderSettings& r, const StyleProfile& s)
    : rs (r), style (s), swingRatio (0.5 + 0.25 * juce::jlimit (0.0f, 1.0f, r.swing))
{
}

double GrooveEngine::warpBeat (double beat, bool swingable) const noexcept
{
    if (! swingable)
        return beat;

    // Swing: piecewise-linear warp inside each 8th note.
    const double eighth = std::floor (beat * 2.0) * 0.5;
    const double x = beat - eighth;           // 0 .. 0.5
    const double sw = swingRatio;
    double warped;
    if (x < 0.25)
        warped = x * (2.0 * sw);
    else
        warped = 0.5 * sw + (x - 0.25) * (2.0 * (1.0 - sw));

    double out = eighth + warped;

    // Personality microtiming on the 16th grid.
    const double sixteenth = beat * 4.0;
    const double nearest = std::round (sixteenth);
    if (std::abs (sixteenth - nearest) < 1.0e-6)
    {
        const int pos = ((int) nearest % 4 + 4) % 4;
        out += style.microTiming[(size_t) pos] * rs.groove * 0.25 * 1.2;
    }

    return out;
}

double GrooveEngine::humanOffsetSeconds (const NoteEvent& e, const Part& part) const noexcept
{
    if (rs.humanize <= 0.0f)
        return 0.0;

    const auto& info = instrumentInfo (part.instrument);

    Rng r (hashCombine (e.seed, 0xA11CEull));
    const double z = juce::jlimit (-2.5, 2.5, r.gaussian());

    // Slow drift: one draw per bar per part, smoothly interpolated.
    const double barPos = e.beat / Arrangement::beatsPerBar;
    const int bar = (int) std::floor (barPos);
    const double frac = barPos - bar;
    Rng d0 (hashCombine (part.seed, (uint64_t) 0xD71F7 + (uint64_t) bar));
    Rng d1 (hashCombine (part.seed, (uint64_t) 0xD71F7 + (uint64_t) bar + 1));
    const double drift = d0.gaussian() * (1.0 - frac) + d1.gaussian() * frac;

    const double h = rs.humanize * info.humanizeScale;
    return h * (z * 0.0075 + drift * 0.0035);
}

double GrooveEngine::onsetSeconds (const NoteEvent& e, const Part& part) const noexcept
{
    return warpBeat (e.beat, e.swingable) * secondsPerBeat() + humanOffsetSeconds (e, part);
}

double GrooveEngine::endSeconds (const NoteEvent& e, const Part& part) const noexcept
{
    const double endBeat = e.beat + e.length;
    const double gridEnd = std::abs (endBeat * 4.0 - std::round (endBeat * 4.0)) < 1.0e-6;
    return warpBeat (endBeat, e.swingable && gridEnd) * secondsPerBeat() + humanOffsetSeconds (e, part);
}

float GrooveEngine::velocity (const NoteEvent& e, InstrumentId id) const noexcept
{
    float v = e.velocity;

    switch (e.dyn)
    {
        case Dynamic::Accent: v *= 0.8f + 0.4f * rs.accent; break;
        case Dynamic::Ghost:  v *= 0.2f + 1.3f * rs.ghost; break;
        default:              v *= 1.0f - 0.15f * rs.accent; break;
    }

    // Personality velocity shape on the 16th grid.
    if (e.swingable)
    {
        const double sixteenth = e.beat * 4.0;
        const double nearest = std::round (sixteenth);
        if (std::abs (sixteenth - nearest) < 1.0e-6)
        {
            const int pos = ((int) nearest % 4 + 4) % 4;
            v *= 1.0f + (style.velocityShape[(size_t) pos] - 1.0f) * rs.groove;
        }
    }

    if (rs.humanize > 0.0f)
    {
        Rng r (hashCombine (e.seed, 0xBEEFull));
        const float scale = instrumentInfo (id).humanizeScale;
        v *= 1.0f + (float) (juce::jlimit (-2.0, 2.0, r.gaussian()) * 0.08 * rs.humanize * scale);
    }

    return juce::jlimit (0.02f, 1.0f, v);
}

} // namespace jgl

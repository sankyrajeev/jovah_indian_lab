#pragma once

#include "../Core/Instruments.h"
#include "../Core/Settings.h"
#include "../Core/Styles.h"

namespace jgl
{

/**
    Turns grid positions into performed time and dynamics.

    - Swing: each 8th is warped piecewise-linearly so the off-beat 16th moves from 50% to
      up to 75% (MPC-style); 32nds in between are warped consistently.
    - Groove: the personality's microtiming and velocity template per 16th position,
      scaled by the Groove knob.
    - Humanise: per-hit Gaussian timing / velocity deviations plus a slow per-bar drift
      ("the player pushes and pulls"), scaled per instrument. Deterministic per event seed.
    - Accent / Ghost knobs reshape the composed dynamics.
*/
class GrooveEngine
{
public:
    GrooveEngine (const RenderSettings& rs, const StyleProfile& style);

    /** Grid beat -> performed beat (swing + groove template only). */
    double warpBeat (double beat, bool swingable) const noexcept;

    /** Event onset in seconds from loop start (may be slightly negative or past the end; callers wrap). */
    double onsetSeconds (const NoteEvent& e, const Part& part) const noexcept;

    /** End of a melodic note in seconds, consistent with the onset warp. */
    double endSeconds (const NoteEvent& e, const Part& part) const noexcept;

    float velocity (const NoteEvent& e, InstrumentId id) const noexcept;

    double secondsPerBeat() const noexcept { return 60.0 / rs.bpm; }

private:
    double humanOffsetSeconds (const NoteEvent& e, const Part& part) const noexcept;

    RenderSettings rs;
    const StyleProfile& style;
    double swingRatio;   // 0.5 .. 0.75
};

} // namespace jgl

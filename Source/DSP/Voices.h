#pragma once

#include <vector>
#include "../Core/Instruments.h"
#include "PitchCurve.h"

namespace jgl
{

/** Everything a voice model needs to render one hit / note (mono). */
struct VoiceRequest
{
    InstrumentId instrument = InstrumentId::Tabla;
    int articulation = 0;
    float velocity = 0.8f;
    double sampleRate = 48000.0;

    // Melodic
    double frequency = 220.0;     // target pitch (Hz)
    double durationSec = 0.5;     // sounding time before release
    float glideFromSemis = 0.0f;  // meend / slide start offset
    double glideSec = 0.0;
    uint8_t ornament = OrnNone;
    double ornamentSemis = 0.0;   // grace note offset relative to the note
    float fallSemis = 0.0f;       // dive at the end of the note (negative = down)
    double fallStartSec = 0.0;
    float vibratoSemis = 0.0f;    // extra vibrato depth (sample-driven voices)

    // Percussion
    double tablaSaHz = 277.0;     // dayan tuning (tabla is traditionally tuned to Sa)
    float bassGain = 1.0f;        // bayan / bass-head level (low-end management)

    uint32_t seed = 0;
};

/**
    Physically-informed synthesis models for every instrument in the library.

    Percussion uses modal synthesis (decaying, gliding membrane / plate modes) with
    filtered-noise contact transients; each hit is rendered fresh with seed-driven
    micro-variation, and velocity changes timbre (brightness, transient) not just level.
    Melodic instruments use a band-limited additive engine with per-harmonic decay, a
    moving "jawari" formant (sitar / tanpura buzz), body formants, vibrato and pitch
    curves for meend, kan, murki and slides.

    These are synthesized approximations, not recordings. Recorded multi-samples placed
    in the user sample folder take priority (see SampleLibrary).
*/
class Voices
{
public:
    /** Renders into `out` (resized as needed). */
    static void render (const VoiceRequest& request, std::vector<float>& out);

    /** How early the sound must start so the main note lands on the beat (grace notes). */
    static double leadInSeconds (const VoiceRequest& request);

    /** Pitch contour for a melodic note: ornaments, glides, falls, vibrato. */
    static PitchCurve makePitchCurve (const VoiceRequest& request);
};

} // namespace jgl

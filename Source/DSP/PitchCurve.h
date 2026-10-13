#pragma once

#include <cmath>
#include <utility>
#include <vector>

#include "DspUtils.h"

namespace jgl
{

/**
    Pitch offset (in semitones) over the life of a note. Shared by the synthesized voices
    and by sample playback, so recorded instruments get the same ornaments:
      - grace-note points (kan, murki)
      - meend / scoop: glide from an offset into the note
      - initial tension offset (string kick, breath scoop)
      - gamaka: oscillation up to the neighbour and back
      - fall: dive away from the note towards its end
      - vibrato
*/
struct PitchCurve
{
    double glideFrom = 0.0, glideSec = 0.0;
    double initialSemis = 0.0, initialTau = 0.03;
    std::vector<std::pair<double, double>> points;   // (time, semis) held values
    double ramp = 0.014;
    double gamakaDepth = 0.0, gamakaRate = 6.5, gamakaStart = 0.12, gamakaEnd = 0.0;
    double fallSemis = 0.0, fallStart = 0.0, fallEnd = 0.0;
    double vibratoSemis = 0.0, vibratoRate = 5.6, vibratoDelay = 0.25, vibratoPhase = 0.0;

    double semis (double t) const noexcept
    {
        using dsp::smoothstep;
        double s = 0.0;

        if (! points.empty())
        {
            double v = points.front().second;
            for (size_t i = 1; i < points.size(); ++i)
            {
                const double c = points[i].first;
                if (t >= c) { v = points[i].second; continue; }
                if (t > c - ramp) v += (points[i].second - v) * smoothstep ((t - (c - ramp)) / ramp);
                break;
            }
            s += v;
        }

        if (glideSec > 0.0 && glideFrom != 0.0)
            s += glideFrom * (1.0 - smoothstep (t / glideSec));

        if (initialSemis != 0.0)
            s += initialSemis * std::exp (-t / initialTau);

        if (gamakaDepth != 0.0 && t > gamakaStart && t < gamakaEnd)
        {
            const double u = t - gamakaStart;
            const double fade = std::min (1.0, u / 0.08) * std::min (1.0, (gamakaEnd - t) / 0.08);
            s += gamakaDepth * fade * 0.5 * (1.0 - std::cos (dsp::twoPi * gamakaRate * u));
        }

        if (fallSemis != 0.0 && t > fallStart && fallEnd > fallStart)
        {
            const double u = std::min (1.0, (t - fallStart) / (fallEnd - fallStart));
            s += fallSemis * u * u;   // accelerating dive
        }

        if (vibratoSemis > 0.0 && t > vibratoDelay)
        {
            const double fadeIn = std::min (1.0, (t - vibratoDelay) / 0.3);
            s += vibratoSemis * fadeIn * std::sin (vibratoPhase + dsp::twoPi * vibratoRate * (t - vibratoDelay));
        }

        return s;
    }
};

} // namespace jgl

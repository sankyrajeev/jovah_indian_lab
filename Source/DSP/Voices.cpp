#include "Voices.h"
#include "DspUtils.h"

#include <algorithm>

namespace jgl
{

using namespace jgl::dsp;

namespace
{
    //==========================================================================
    // Pitch curve for melodic notes: glide (meend), initial pitch offset (string
    // tension "kick" / breath scoop) and grace-note ornaments.
    struct PitchCurve
    {
        double glideFrom = 0.0, glideSec = 0.0;
        double initialSemis = 0.0, initialTau = 0.03;
        std::vector<std::pair<double, double>> points;   // (time, semis) held values, last one ends at 0
        double ramp = 0.014;
        double gamakaDepth = 0.0, gamakaRate = 6.5, gamakaStart = 0.12, gamakaEnd = 0.0;

        double semis (double t) const noexcept
        {
            double s = 0.0;

            if (! points.empty())
            {
                // Held values with a smooth ramp into each change point.
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

            // Gamaka: the note swings up to its neighbour and back, easing in and out.
            if (gamakaDepth != 0.0 && t > gamakaStart && t < gamakaEnd)
            {
                const double u = t - gamakaStart;
                const double fade = std::min (1.0, u / 0.08) * std::min (1.0, (gamakaEnd - t) / 0.08);
                s += gamakaDepth * fade * 0.5 * (1.0 - std::cos (twoPi * gamakaRate * u));
            }

            return s;
        }
    };

    struct Formant { double hz, bw, gain; };

    struct HarmonicSpec
    {
        int numHarmonics = 24;
        double ampExponent = 1.0;
        double t60Base = 1.0, t60Slope = 0.05;
        double jawariStart = 0.0, jawariEnd = 0.0, jawariTau = 0.5, jawariWidth = 4.0, jawariGain = 0.0;
        std::vector<Formant> formants;
        std::vector<double> customAmps;   // overrides 1/n^exp when set
        double brightnessVel = 0.5;
        double attackSec = 0.002, releaseSec = 0.05;
        bool sustained = false;
        double swellDb = 0.0, sustainDecayDbPerSec = 0.0;
        double vibratoRate = 0.0, vibratoDepth = 0.0, vibratoDelay = 0.0;
    };

    /** Band-limited additive voice using the Chebyshev recursion sin(n x) = 2cos(x)sin((n-1)x) - sin((n-2)x). */
    void renderHarmonic (std::vector<float>& out, const HarmonicSpec& spec, double f0, const PitchCurve& pitch,
                         double velocity, double durationSec, double sr, Rng& rng, double startOffsetSec = 0.0)
    {
        const int H = spec.numHarmonics;
        const double maxT60 = spec.sustained ? durationSec + spec.releaseSec
                                             : std::min (durationSec + spec.releaseSec, spec.t60Base * 1.1);
        const int total = (int) (std::max (0.02, maxT60) * sr) + 1;
        const int offset = (int) (startOffsetSec * sr);
        if ((int) out.size() < total + offset)
            out.resize ((size_t) (total + offset), 0.0f);

        std::vector<double> base ((size_t) H + 1, 0.0), t60 ((size_t) H + 1, 1.0), aPrev ((size_t) H + 1, 0.0), aNext ((size_t) H + 1, 0.0);
        const double bright = 1.0 - spec.brightnessVel * (1.0 - velocity);
        for (int n = 1; n <= H; ++n)
        {
            double a = n <= (int) spec.customAmps.size() ? spec.customAmps[(size_t) n - 1] : 1.0 / std::pow ((double) n, spec.ampExponent);
            if (n > 4) a *= std::pow (bright, 0.5 + (n - 4) * 0.08);
            a *= 1.0 + 0.06 * rng.bipolar();   // per-note micro variation
            base[(size_t) n] = a;
            t60[(size_t) n] = spec.t60Base / (1.0 + spec.t60Slope * (n - 1));
        }

        const double vibPhase0 = rng.uniform() * twoPi;
        const double vibRate = spec.vibratoRate * (1.0 + 0.05 * rng.bipolar());

        auto freqAt = [&] (double t)
        {
            double semis = pitch.semis (t);
            double vib = 0.0;
            if (spec.vibratoDepth > 0.0 && t > spec.vibratoDelay)
            {
                const double fadeIn = std::min (1.0, (t - spec.vibratoDelay) / 0.35);
                vib = spec.vibratoDepth * fadeIn * std::sin (vibPhase0 + twoPi * vibRate * t);
            }
            return f0 * std::pow (2.0, semis / 12.0) * (1.0 + vib);
        };

        auto ampEnvelope = [&] (double t)
        {
            double e = t < spec.attackSec ? smoothstep (t / spec.attackSec) : 1.0;
            if (spec.sustained)
            {
                const double swell = std::pow (10.0, spec.swellDb * std::min (1.0, t / 0.5) / 20.0);
                const double sus = std::pow (10.0, -spec.sustainDecayDbPerSec * t / 20.0);
                e *= swell * sus;
            }
            if (t > durationSec)
                e *= std::max (0.0, 1.0 - (t - durationSec) / std::max (0.005, spec.releaseSec));
            return e;
        };

        auto computeAmps = [&] (double t, double f, std::vector<double>& a)
        {
            const double jc = spec.jawariEnd + (spec.jawariStart - spec.jawariEnd) * std::exp (-t / spec.jawariTau);
            const double env = ampEnvelope (t);
            for (int n = 1; n <= H; ++n)
            {
                const double hf = n * f;
                double g = 0.0;
                if (hf < sr * 0.45)
                {
                    g = base[(size_t) n] * env;
                    if (hf > sr * 0.38) g *= (sr * 0.45 - hf) / (sr * 0.07);
                    if (! spec.sustained)
                        g *= std::exp (-t * 6.907755 / t60[(size_t) n]);
                    if (spec.jawariGain > 0.0)
                    {
                        const double d = (n - jc) / spec.jawariWidth;
                        g *= 1.0 + spec.jawariGain * std::exp (-d * d);
                    }
                    for (const auto& fm : spec.formants)
                    {
                        const double d = (hf - fm.hz) / fm.bw;
                        g *= 1.0 + fm.gain * std::exp (-d * d);
                    }
                }
                a[(size_t) n] = g;
            }
        };

        constexpr int block = 16;
        double phase = rng.uniform() * 0.5;
        computeAmps (0.0, freqAt (0.0), aPrev);

        for (int start = 0; start < total; start += block)
        {
            const int len = std::min (block, total - start);
            const double tEnd = (start + len) / sr;
            const double fStart = freqAt (start / sr);
            const double fEnd = freqAt (tEnd);
            computeAmps (tEnd, fEnd, aNext);

            for (int i = 0; i < len; ++i)
            {
                const double frac = (double) i / len;
                const double f = fStart + (fEnd - fStart) * frac;
                phase = wrapPhase (phase + twoPi * f / sr);

                const double s1 = fastSin (phase);
                const double c2 = 2.0 * fastSin (phase + pi * 0.5 > pi ? phase - pi * 1.5 : phase + pi * 0.5);
                double sPrev = 0.0, sCur = s1;
                double sum = (aPrev[1] + (aNext[1] - aPrev[1]) * frac) * s1;
                for (int n = 2; n <= H; ++n)
                {
                    const double sNext = c2 * sCur - sPrev;
                    sPrev = sCur;
                    sCur = sNext;
                    const double a = aPrev[(size_t) n] + (aNext[(size_t) n] - aPrev[(size_t) n]) * frac;
                    if (a != 0.0)
                        sum += a * sNext;
                }
                out[(size_t) (offset + start + i)] += (float) sum;
            }

            std::swap (aPrev, aNext);
        }
    }

    float velCurve (float v, float exponent = 1.5f) { return std::pow (juce::jlimit (0.0f, 1.0f, v), exponent); }

    //==========================================================================
    // TABLA
    void renderDayan (std::vector<float>& out, int stroke, double saHz, float vel, double sr, Rng& rng)
    {
        const double f0 = saHz * (1.0 + 0.004 * rng.bipolar());
        const double b = 0.45 + 0.55 * vel;
        const double amp = velCurve (vel);
        std::vector<Mode> m;
        auto harm = [&] (double ratio, double a, double t60) { m.push_back ({ f0 * ratio * (1.0 + 0.002 * rng.bipolar()), a * amp, t60 * (0.9 + 0.2 * rng.uniform()), 1.012, 0.012 }); };

        switch (stroke)
        {
            case TablaArt::Na:
                harm (1, .45, .55); harm (2, 1.0, .5); harm (3, .55 * b, .35); harm (4, .3 * b, .25); harm (5, .2 * b, .18); harm (6, .1 * b, .12);
                harm (7, .07 * b, .09); harm (8, .05 * b, .07); harm (10, .03 * b, .05);
                harm (1.52, .12, .06); harm (2.47, .1, .05); harm (3.6, .08 * b, .04);
                renderModes (out, m, sr, rng);
                renderNoiseBurst (out, 1, 2500, .7, .003, .5 * amp * b, sr, rng.next());
                renderNoiseBurst (out, 2, 3800, 4.0, .01, .65 * amp, sr, rng.next());
                break;

            case TablaArt::Tin:
                harm (1, .8, .9); harm (2, 1.0, .7); harm (3, .4 * b, .4); harm (4, .2 * b, .3); harm (5, .1 * b, .2);
                harm (1.52, .08, .05); harm (2.47, .06, .04);
                renderModes (out, m, sr, rng);
                renderNoiseBurst (out, 1, 2000, .7, .003, .3 * amp * b, sr, rng.next());
                break;

            case TablaArt::Tun:
                harm (1, 1.0, 1.2); harm (2, .3, .6); harm (3, .12 * b, .35); harm (4, .05 * b, .2);
                renderModes (out, m, sr, rng);
                renderNoiseBurst (out, 0, 1200, .7, .006, .3 * amp, sr, rng.next());
                break;

            case TablaArt::Re:
                harm (1, .4, .05); harm (2, .4, .04); harm (3, .25, .035);
                renderModes (out, m, sr, rng);
                renderNoiseBurst (out, 2, 1600, 1.5, .009, 1.1 * amp, sr, rng.next());
                renderNoiseBurst (out, 1, 3000, .7, .0015, .3 * amp, sr, rng.next());
                break;

            case TablaArt::Te:
            default:
                harm (1, .5, .06); harm (2, .35, .05); harm (3, .2, .04);
                renderModes (out, m, sr, rng);
                renderNoiseBurst (out, 2, 1100, 1.2, .012, 1.2 * amp, sr, rng.next());
                renderNoiseBurst (out, 1, 3000, .7, .0015, .3 * amp, sr, rng.next());
                break;
        }
    }

    void renderBayan (std::vector<float>& out, int stroke, float vel, float gain, double sr, Rng& rng)
    {
        const double fb = 98.0 * (1.0 + 0.012 * rng.bipolar());
        const double amp = velCurve (vel) * gain;
        std::vector<Mode> m;

        switch (stroke)
        {
            case TablaArt::Ka:
                m.push_back ({ fb * 1.1, .5 * amp, .05, 1.05, .01 });
                m.push_back ({ fb * 1.75, .3 * amp, .04, 1.05, .01 });
                renderModes (out, m, sr, rng);
                renderNoiseBurst (out, 0, 900, .7, .015, 1.0 * amp, sr, rng.next());
                renderNoiseBurst (out, 2, 2000, 1.0, .002, .35 * amp, sr, rng.next());
                break;

            case TablaArt::Ghe:
            {
                // Gumki: the heel slides and the pitch rises while the note rings.
                const double rise = 1.18 + 0.1 * rng.uniform();
                m.push_back ({ fb * rise, 1.0 * amp, 1.1, 1.0 / rise, .11 });
                m.push_back ({ fb * rise * 2.0, .22 * amp, .5, 1.0 / rise, .11 });
                m.push_back ({ fb * rise * 1.58, .12 * amp, .3, 1.0 / rise, .11 });
                renderModes (out, m, sr, rng, 0.001);
                renderNoiseBurst (out, 0, 500, .7, .006, .3 * amp, sr, rng.next());
                break;
            }

            case TablaArt::Ge:
            default:
                m.push_back ({ fb, 1.0 * amp, .75, 1.07, .05 });
                m.push_back ({ fb * 2.0, .25 * amp, .35, 1.07, .05 });
                m.push_back ({ fb * 1.58, .15 * amp, .25, 1.05, .04 });
                m.push_back ({ fb * 2.7, .08 * amp, .15, 1.05, .03 });
                renderModes (out, m, sr, rng, 0.001);
                renderNoiseBurst (out, 0, 500, .7, .006, .35 * amp, sr, rng.next());
                break;
        }
    }

    void renderTabla (std::vector<float>& out, const VoiceRequest& r, Rng& rng)
    {
        out.assign ((size_t) (1.4 * r.sampleRate), 0.0f);
        const float v = r.velocity;
        switch (r.articulation)
        {
            case TablaArt::Dha:  renderDayan (out, TablaArt::Na, r.tablaSaHz, v, r.sampleRate, rng);  renderBayan (out, TablaArt::Ge, v * 0.9f, r.bassGain, r.sampleRate, rng); break;
            case TablaArt::Dhin: renderDayan (out, TablaArt::Tin, r.tablaSaHz, v, r.sampleRate, rng); renderBayan (out, TablaArt::Ge, v * 0.9f, r.bassGain, r.sampleRate, rng); break;
            case TablaArt::Ge:
            case TablaArt::Ka:
            case TablaArt::Ghe:  renderBayan (out, r.articulation, v, r.bassGain, r.sampleRate, rng); break;
            default:             renderDayan (out, r.articulation, r.tablaSaHz, v, r.sampleRate, rng); break;
        }
    }

    //==========================================================================
    // DHOL
    void renderDagga (std::vector<float>& out, bool muted, float vel, double sr, Rng& rng)
    {
        const double f = 64.0 * (1.0 + 0.015 * rng.bipolar());
        const double amp = velCurve (vel, 1.3f);
        const double b = 0.4 + 0.6 * vel;
        const double decay = muted ? 0.28 : 1.0;
        static const double ratios[] = { 1.0, 1.59, 2.14, 2.30, 2.65, 2.92, 3.16 };
        static const double amps[]   = { 1.0, .55, .35, .3, .18, .12, .08 };
        static const double t60s[]   = { .65, .3, .22, .18, .14, .1, .08 };

        std::vector<Mode> m;
        for (int i = 0; i < 7; ++i)
            m.push_back ({ f * ratios[i], amps[i] * amp * (i == 0 ? 1.0 : b), t60s[i] * decay * (0.9 + 0.2 * rng.uniform()),
                           1.45, muted ? .018 : .028 });

        std::vector<float> tmp (out.size(), 0.0f);
        renderModes (tmp, m, sr, rng, 0.0006);
        renderNoiseBurst (tmp, 2, 180, .7, .012, .7 * amp, sr, rng.next());
        renderNoiseBurst (tmp, 2, 1500, 1.0, .004, .35 * amp * b, sr, rng.next());

        // Dhol bass is driven hard: gentle saturation adds the characteristic grit.
        const double drive = 1.6;
        const double norm = 1.0 / std::tanh (drive);
        for (size_t i = 0; i < out.size(); ++i)
            out[i] += (float) (std::tanh (tmp[i] * drive) * norm);
    }

    void renderTilli (std::vector<float>& out, bool edge, float vel, double sr, Rng& rng)
    {
        const double f = (edge ? 760.0 : 520.0) * (1.0 + 0.02 * rng.bipolar());
        const double amp = velCurve (vel, 1.4f);
        const double decay = edge ? 0.6 : 1.0;
        static const double ratios[] = { 1.0, 1.47, 1.93, 2.6, 3.4 };
        static const double amps[]   = { 1.0, .7, .5, .35, .2 };
        static const double t60s[]   = { .12, .1, .08, .06, .05 };

        std::vector<Mode> m;
        for (int i = 0; i < 5; ++i)
            m.push_back ({ f * ratios[i], amps[i] * amp * 0.45, t60s[i] * decay, 1.03, .006 });

        renderModes (out, m, sr, rng);
        renderNoiseBurst (out, 2, edge ? 5000 : 3500, 1.2, .009 * decay, .9 * amp, sr, rng.next());
        renderNoiseBurst (out, 1, 5000, .7, .0015, .4 * amp, sr, rng.next());
    }

    void renderDhol (std::vector<float>& out, const VoiceRequest& r, Rng& rng)
    {
        out.assign ((size_t) (1.2 * r.sampleRate), 0.0f);
        const double sr = r.sampleRate;
        switch (r.articulation)
        {
            case DholArt::Dagga:      renderDagga (out, false, r.velocity * r.bassGain, sr, rng); break;
            case DholArt::DaggaMuted: renderDagga (out, true, r.velocity * r.bassGain, sr, rng); break;
            case DholArt::Tilli:      renderTilli (out, false, r.velocity, sr, rng); break;
            case DholArt::TilliEdge:  renderTilli (out, true, r.velocity, sr, rng); break;
            case DholArt::Both:
            default:                  renderDagga (out, false, r.velocity * r.bassGain, sr, rng); renderTilli (out, false, r.velocity, sr, rng); break;
        }
    }

    //==========================================================================
    // DHOLAK
    void renderDholak (std::vector<float>& out, const VoiceRequest& r, Rng& rng)
    {
        out.assign ((size_t) (1.2 * r.sampleRate), 0.0f);
        const double sr = r.sampleRate;
        const double amp = velCurve (r.velocity);
        const double b = 0.45 + 0.55 * r.velocity;
        std::vector<Mode> m;

        switch (r.articulation)
        {
            case DholakArt::Ge:
            {
                const double f = 118.0 * (1.0 + 0.012 * rng.bipolar());
                const double a = amp * r.bassGain;
                m.push_back ({ f, 1.0 * a, .45, 1.08, .03 });
                m.push_back ({ f * 1.6, .35 * a, .2, 1.08, .03 });
                m.push_back ({ f * 2.3, .15 * a * b, .12, 1.05, .02 });
                renderModes (out, m, sr, rng, 0.0008);
                renderNoiseBurst (out, 0, 600, .7, .008, .3 * a, sr, rng.next());
                break;
            }
            case DholakArt::GeSlide:
            {
                const double rise = 1.25 + 0.1 * rng.uniform();
                const double f = 118.0 * rise;
                const double a = amp * r.bassGain;
                m.push_back ({ f, 1.0 * a, .7, 1.0 / rise, .14 });
                m.push_back ({ f * 1.6, .3 * a, .3, 1.0 / rise, .14 });
                renderModes (out, m, sr, rng, 0.0008);
                renderNoiseBurst (out, 0, 600, .7, .008, .3 * a, sr, rng.next());
                break;
            }
            case DholakArt::Na:
            {
                const double f = 340.0 * (1.0 + 0.01 * rng.bipolar());
                m.push_back ({ f, .8 * amp, .25, 1.02, .01 });
                m.push_back ({ f * 1.52, 1.0 * amp, .22, 1.02, .01 });
                m.push_back ({ f * 2.0, .5 * amp * b, .14, 1.02, .01 });
                m.push_back ({ f * 2.48, .35 * amp * b, .1, 1.02, .01 });
                m.push_back ({ f * 3.1, .2 * amp * b, .08, 1.02, .01 });
                renderModes (out, m, sr, rng);
                renderNoiseBurst (out, 2, 2500, 2.0, .006, .5 * amp, sr, rng.next());
                break;
            }
            case DholakArt::Ta:
            {
                const double f = 340.0;
                m.push_back ({ f, .5 * amp, .05 });
                m.push_back ({ f * 1.52, .4 * amp, .04 });
                m.push_back ({ f * 2.0, .3 * amp, .03 });
                renderModes (out, m, sr, rng);
                renderNoiseBurst (out, 2, 1300, 1.0, .014, 1.1 * amp, sr, rng.next());
                break;
            }
            case DholakArt::Chat:
            default:
                m.push_back ({ 1900.0, .3 * amp, .03 });
                renderModes (out, m, sr, rng);
                renderNoiseBurst (out, 2, 3200, 2.5, .005, 1.1 * amp, sr, rng.next());
                renderNoiseBurst (out, 1, 5000, .7, .0012, .3 * amp, sr, rng.next());
                break;
        }
    }

    //==========================================================================
    // MANJIRA: two small thick bronze cymbals struck together.
    void renderManjira (std::vector<float>& out, const VoiceRequest& r, Rng& rng)
    {
        const bool open = r.articulation == ManjiraArt::Open;
        const double sr = r.sampleRate;
        out.assign ((size_t) ((open ? 2.6 : 0.3) * sr), 0.0f);

        const double amp = velCurve (r.velocity, 1.3f) * 0.32;
        const double base = 1850.0 * (1.0 + 0.03 * rng.bipolar());
        const double decay = open ? 1.0 : 0.035;
        static const double ratios[] = { 1.0, 1.37, 2.04, 2.53, 3.18, 3.91, 4.67, 5.5 };
        static const double amps[]   = { .6, 1.0, .8, .6, .5, .35, .25, .18 };
        static const double t60s[]   = { 2.2, 1.9, 1.6, 1.3, 1.0, .8, .6, .45 };

        std::vector<Mode> m;
        for (int plate = 0; plate < 2; ++plate)
        {
            const double pf = base * (plate == 0 ? 1.0 : 1.031);
            const double pa = plate == 0 ? 1.0 : 0.6;
            for (int i = 0; i < 8; ++i)
            {
                const double f = pf * ratios[i];
                const double a = amps[i] * amp * pa * (0.85 + 0.3 * rng.uniform());
                const double t = t60s[i] * decay * (0.85 + 0.3 * rng.uniform());
                m.push_back ({ f, a, t });
                m.push_back ({ f + 2.3 + rng.uniform(), a * 0.7, t });   // beating pair -> shimmer
            }
        }
        renderModes (out, m, sr, rng, 0.0003);
        renderNoiseBurst (out, 1, 4500, .7, open ? .025 : .02, .6 * amp / 0.32 * 0.4, sr, rng.next());
    }

    //==========================================================================
    // GHUNGROO: clusters of tiny bells.
    void renderBellGrain (std::vector<float>& out, int offset, double amp, double sr, Rng& rng)
    {
        const double f = 4200.0 + rng.uniform() * 3000.0;
        const double t = 0.06 + rng.uniform() * 0.06;
        std::vector<float> g ((size_t) (t * 1.3 * sr) + 1, 0.0f);
        std::vector<Mode> m { { f, amp, t }, { f * 1.81, amp * .5, t * .7 }, { f * 2.93, amp * .25, t * .5 } };
        renderModes (g, m, sr, rng, 0.0002);
        renderNoiseBurst (g, 1, 6000, .7, .0015, amp * .5, sr, rng.next());
        for (size_t i = 0; i < g.size() && offset + (int) i < (int) out.size(); ++i)
            out[(size_t) offset + i] += g[i];
    }

    void renderGhungroo (std::vector<float>& out, const VoiceRequest& r, Rng& rng)
    {
        const double sr = r.sampleRate;
        out.assign ((size_t) (0.7 * sr), 0.0f);
        const double amp = velCurve (r.velocity, 1.3f) * 0.16;

        switch (r.articulation)
        {
            case GhungrooArt::LongShake:
            {
                const int n = 40;
                for (int k = 0; k < n; ++k)
                {
                    const double u = rng.uniform();
                    const double t = 0.32 * std::sqrt (u);
                    const double a = amp * (0.35 + 0.65 * (t / 0.32)) * (0.4 + 0.6 * rng.uniform());
                    renderBellGrain (out, (int) (t * sr), a, sr, rng);
                }
                break;
            }
            case GhungrooArt::Stamp:
            {
                std::vector<Mode> thud { { 85.0, velCurve (r.velocity) * 0.25, .1, 1.3, .015 } };
                renderModes (out, thud, sr, rng, 0.001);
                for (int k = 0; k < 30; ++k)
                    renderBellGrain (out, (int) (0.025 * std::pow (rng.uniform(), 1.5) * sr), amp * (0.5 + 0.5 * rng.uniform()), sr, rng);
                break;
            }
            case GhungrooArt::Shake:
            default:
            {
                const int n = 12 + (int) (10 * r.velocity);
                for (int k = 0; k < n; ++k)
                {
                    const double t = 0.045 * std::pow (rng.uniform(), 1.8);
                    renderBellGrain (out, (int) (t * sr), amp * (0.4 + 0.6 * rng.uniform()), sr, rng);
                }
                break;
            }
        }
    }

    //==========================================================================
    // TAALI: hand claps (single and group).
    void renderTaali (std::vector<float>& out, const VoiceRequest& r, Rng& rng)
    {
        const double sr = r.sampleRate;
        out.assign ((size_t) (0.4 * sr), 0.0f);
        const double amp = velCurve (r.velocity, 1.3f);

        auto clap = [&] (double startSec, double centre, double scale)
        {
            const int off = (int) (startSec * sr);
            renderNoiseBurst (out, 2, centre, 1.4, .004, .8 * amp * scale, sr, rng.next(), off);
            renderNoiseBurst (out, 2, centre * 1.1, 1.4, .004, .7 * amp * scale, sr, rng.next(), off + (int) ((0.008 + 0.002 * rng.uniform()) * sr));
            renderNoiseBurst (out, 2, centre * 0.95, 1.4, .004, .7 * amp * scale, sr, rng.next(), off + (int) ((0.016 + 0.002 * rng.uniform()) * sr));
            renderNoiseBurst (out, 2, centre * 0.85, 1.0, .05, 1.0 * amp * scale, sr, rng.next(), off + (int) (0.024 * sr));
        };

        if (r.articulation == TaaliArt::Group)
        {
            for (int k = 0; k < 6; ++k)
                clap (0.022 * rng.uniform(), 800.0 + rng.uniform() * 1000.0, 0.35 + 0.25 * rng.uniform());
        }
        else
        {
            clap (0.0, 1150.0 * (1.0 + 0.08 * rng.bipolar()), 1.0);
        }
    }

    //==========================================================================
    // Melodic instruments
    PitchCurve makePitchCurve (const VoiceRequest& r)
    {
        PitchCurve pc;
        pc.glideFrom = r.glideFromSemis;
        pc.glideSec = r.glideSec;

        if (r.ornament == OrnKan)
        {
            pc.points = { { 0.0, r.ornamentSemis }, { 0.045, 0.0 } };
            pc.ramp = 0.018;
        }
        else if (r.ornament == OrnGamaka)
        {
            pc.gamakaDepth = r.ornamentSemis;
            pc.gamakaRate = 6.0;
            pc.gamakaStart = std::min (0.15, r.durationSec * 0.25);
            pc.gamakaEnd = std::max (pc.gamakaStart, r.durationSec * 0.85);
        }
        else if (r.ornament == OrnMurki)
        {
            const double up = r.ornamentSemis;
            pc.points = { { 0.0, up }, { 0.04, 0.0 }, { 0.08, -std::max (1.0, up * 0.5) }, { 0.12, 0.0 } };
            pc.ramp = 0.012;
        }
        return pc;
    }

    void renderSitar (std::vector<float>& out, const VoiceRequest& r, Rng& rng)
    {
        HarmonicSpec s;
        s.numHarmonics = 48;
        s.ampExponent = 0.55;
        s.t60Base = 2.6;
        s.t60Slope = 0.045;
        s.jawariStart = 24.0; s.jawariEnd = 8.0; s.jawariTau = 0.55; s.jawariWidth = 5.0; s.jawariGain = 2.4;
        s.formants = { { 900.0, 600.0, 0.5 } };
        s.brightnessVel = 0.6;
        s.attackSec = 0.0015;
        s.releaseSec = 0.06;

        auto pc = makePitchCurve (r);
        const double lead = Voices::leadInSeconds (r);
        const double amp = velCurve (r.velocity, 1.2f) * 0.09;
        out.clear();
        std::vector<float> tmp;
        renderHarmonic (tmp, s, r.frequency, pc, r.velocity, r.durationSec + lead, r.sampleRate, rng);
        renderNoiseBurst (tmp, 2, 2800, 1.5, .004, .25 * velCurve (r.velocity), r.sampleRate, rng.next());
        out.resize (tmp.size());
        for (size_t i = 0; i < tmp.size(); ++i)
            out[i] = (float) (tmp[i] * amp);
    }

    void renderTanpura (std::vector<float>& out, const VoiceRequest& r, Rng& rng)
    {
        HarmonicSpec s;
        s.numHarmonics = 40;
        s.ampExponent = 0.85;
        s.t60Base = 7.0;
        s.t60Slope = 0.025;
        s.jawariStart = 22.0; s.jawariEnd = 6.0; s.jawariTau = 1.6; s.jawariWidth = 4.0; s.jawariGain = 4.5;
        s.brightnessVel = 0.3;
        s.attackSec = 0.012;
        s.releaseSec = 0.4;

        PitchCurve pc;
        const double dur = std::max (r.durationSec * 1.6, 3.5);
        std::vector<float> tmp;
        renderHarmonic (tmp, s, r.frequency, pc, r.velocity, dur, r.sampleRate, rng);
        // A second, very slightly sharp partial set: the two halves of the string / bridge give a slow chorus.
        Rng rng2 (rng.next());
        s.jawariStart = 20.0; s.jawariTau = 1.9;
        renderHarmonic (tmp, s, r.frequency * 1.0009, pc, r.velocity * 0.7, dur, r.sampleRate, rng2);

        const double amp = velCurve (r.velocity, 1.0f) * 0.05;
        out.resize (tmp.size());
        for (size_t i = 0; i < tmp.size(); ++i)
            out[i] = (float) (tmp[i] * amp);
    }

    void renderTumbi (std::vector<float>& out, const VoiceRequest& r, Rng& rng)
    {
        HarmonicSpec s;
        s.numHarmonics = 36;
        s.ampExponent = 0.45;
        s.t60Base = 1.1;
        s.t60Slope = 0.09;
        s.formants = { { 1100.0, 450.0, 1.3 }, { 2700.0, 900.0, 0.6 } };
        s.brightnessVel = 0.5;
        s.attackSec = 0.0008;
        s.releaseSec = 0.03;

        auto pc = makePitchCurve (r);
        pc.initialSemis = 0.28;   // the string's tension kick at the pluck
        pc.initialTau = 0.03;

        std::vector<float> tmp;
        renderHarmonic (tmp, s, r.frequency, pc, r.velocity, r.durationSec, r.sampleRate, rng);
        renderNoiseBurst (tmp, 2, 3500, 1.2, .002, .35 * velCurve (r.velocity), r.sampleRate, rng.next());

        const double amp = velCurve (r.velocity, 1.2f) * 0.05;
        out.resize (tmp.size());
        for (size_t i = 0; i < tmp.size(); ++i)
            out[i] = (float) (tmp[i] * amp);
    }

    void renderBansuri (std::vector<float>& out, const VoiceRequest& r, Rng& rng)
    {
        HarmonicSpec s;
        s.numHarmonics = 8;
        s.customAmps = { 1.0, .32, .13, .06, .035, .02, .012, .008 };
        s.sustained = true;
        s.attackSec = r.ornament == OrnKan ? 0.025 : 0.055;
        s.releaseSec = 0.09;
        s.swellDb = 1.5;
        s.sustainDecayDbPerSec = 2.0;
        s.vibratoRate = 5.2;
        s.vibratoDepth = 0.0035;
        s.vibratoDelay = 0.22;
        s.brightnessVel = 0.4;

        auto pc = makePitchCurve (r);
        if (r.ornament == OrnNone && r.glideSec <= 0.0)
        {
            pc.initialSemis = -0.22;   // breath scoop into the note
            pc.initialTau = 0.05;
        }

        const double sr = r.sampleRate;
        const double lead = Voices::leadInSeconds (r);
        const double dur = r.durationSec + lead;
        std::vector<float> tone;
        renderHarmonic (tone, s, r.frequency, pc, r.velocity, dur, sr, rng);

        // Breath: noise focused around the fundamental and 2nd harmonic, plus air and a chiff.
        Noise noise (rng.next());
        Biquad b1, b2, air, chiff;
        b1.setBandpass (r.frequency, 12.0, sr);
        b2.setBandpass (r.frequency * 2.0, 10.0, sr);
        air.setHighpass (2000.0, 0.7, sr);
        chiff.setBandpass (r.frequency * 3.0, 2.0, sr);

        const double attack = s.attackSec;
        for (size_t i = 0; i < tone.size(); ++i)
        {
            const double t = i / sr;
            double env = t < attack ? smoothstep (t / attack) : 1.0;
            if (t > dur) env *= std::max (0.0, 1.0 - (t - dur) / s.releaseSec);
            const float n = noise.next();
            const double breath = b1.process (n) * 0.9 + b2.process (n) * 0.45 + air.process (n) * 0.03;
            const double ch = chiff.process (n) * 0.6 * std::exp (-t / 0.04);
            tone[i] += (float) ((breath * env + ch) * 0.35);
        }

        const double amp = velCurve (r.velocity, 1.0f) * 0.22;
        out.resize (tone.size());
        for (size_t i = 0; i < tone.size(); ++i)
            out[i] = (float) (tone[i] * amp);
    }
}

namespace
{
    /** Nadaswaram: loud conical double reed. Bright, buzzy spectrum with nasal formants,
        reed "bark" at the attack, wide vibrato, gamakas and glides between notes. */
    void renderNadaswaram (std::vector<float>& out, const VoiceRequest& r, Rng& rng)
    {
        HarmonicSpec s;
        s.numHarmonics = 32;
        s.ampExponent = 0.55;
        s.formants = { { 650.0, 260.0, 0.8 }, { 1250.0, 450.0, 2.6 }, { 2800.0, 900.0, 1.4 } };
        s.sustained = true;
        s.attackSec = r.ornament == OrnKan ? 0.02 : 0.035;
        s.releaseSec = 0.08;
        s.swellDb = 2.0;
        s.sustainDecayDbPerSec = 1.0;
        s.vibratoRate = 5.6;
        s.vibratoDepth = r.ornament == OrnGamaka ? 0.0 : 0.006;
        s.vibratoDelay = 0.3;
        s.brightnessVel = 0.5;

        auto pc = makePitchCurve (r);
        if (r.ornament == OrnNone && r.glideSec <= 0.0)
        {
            pc.initialSemis = -0.45;   // players lean into the note from below
            pc.initialTau = 0.06;
        }

        const double sr = r.sampleRate;
        const double lead = Voices::leadInSeconds (r);
        const double dur = r.durationSec + lead;
        std::vector<float> tone;
        renderHarmonic (tone, s, r.frequency, pc, r.velocity, dur, sr, rng);

        // Reed nonlinearity + buzz: slow random amplitude flutter, soft clipping, reed noise.
        Noise noise (rng.next());
        dsp::Biquad buzz, bark;
        buzz.setBandpass (3200.0, 1.2, sr);
        bark.setBandpass (r.frequency * 4.0, 1.5, sr);
        dsp::OnePole flutterLp;
        flutterLp.setLowpass (18.0, sr);
        const double peak = 1.0e-9 + [&] { float m = 0; for (auto v : tone) m = std::max (m, std::abs (v)); return (double) m; }();

        for (size_t i = 0; i < tone.size(); ++i)
        {
            const double t = i / sr;
            double env = t < s.attackSec ? smoothstep (t / s.attackSec) : 1.0;
            if (t > dur) env *= std::max (0.0, 1.0 - (t - dur) / s.releaseSec);
            const float n = noise.next();
            const double flutter = 1.0 + 0.6 * flutterLp.lowpass (n);
            double x = tone[i] / peak * flutter;
            x = std::tanh (1.6 * x) / std::tanh (1.6);                       // reed saturation
            x += (buzz.process (n) * 0.06 + bark.process (n) * 0.5 * std::exp (-t / 0.03)) * env;
            tone[i] = (float) x;
        }

        const double amp = velCurve (r.velocity, 1.0f) * 0.3;
        out.resize (tone.size());
        for (size_t i = 0; i < tone.size(); ++i)
            out[i] = (float) (tone[i] * amp);
    }
}

//==============================================================================
double Voices::leadInSeconds (const VoiceRequest& r)
{
    switch (r.ornament)
    {
        case OrnKan:   return 0.045;
        case OrnMurki: return 0.12;
        default:       return 0.0;
    }
}

void Voices::render (const VoiceRequest& r, std::vector<float>& out)
{
    Rng rng (hashCombine (r.seed, (uint64_t) 0x5EED + (uint64_t) r.articulation));

    switch (r.instrument)
    {
        case InstrumentId::Tabla:    renderTabla (out, r, rng); break;
        case InstrumentId::Dhol:     renderDhol (out, r, rng); break;
        case InstrumentId::Dholak:   renderDholak (out, r, rng); break;
        case InstrumentId::Manjira:  renderManjira (out, r, rng); break;
        case InstrumentId::Ghungroo: renderGhungroo (out, r, rng); break;
        case InstrumentId::Taali:    renderTaali (out, r, rng); break;
        case InstrumentId::Sitar:    renderSitar (out, r, rng); break;
        case InstrumentId::Tanpura:  renderTanpura (out, r, rng); break;
        case InstrumentId::Tumbi:    renderTumbi (out, r, rng); break;
        case InstrumentId::Bansuri:  renderBansuri (out, r, rng); break;
        case InstrumentId::Nadaswaram: renderNadaswaram (out, r, rng); break;
        default:                     out.clear(); break;
    }
}

} // namespace jgl

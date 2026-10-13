#pragma once

#include <cmath>
#include <vector>

#include "../Core/Rng.h"

namespace jgl::dsp
{

constexpr double twoPi = 6.283185307179586476925286766559;
constexpr double pi    = 3.1415926535897932384626433832795;

inline double midiToHz (double midi) noexcept { return 440.0 * std::pow (2.0, (midi - 69.0) / 12.0); }
inline float dbToGain (float db) noexcept { return std::pow (10.0f, db / 20.0f); }

/** Per-sample multiplier giving -60 dB after t60 seconds. */
inline double t60Multiplier (double t60, double sr) noexcept
{
    return t60 <= 0.0 ? 0.0 : std::pow (10.0, -3.0 / (t60 * sr));
}

/** sin(x) for x in [-pi, pi], max error ~4e-6. */
inline double fastSin (double x) noexcept
{
    if (x > pi * 0.5)       x = pi - x;
    else if (x < -pi * 0.5) x = -pi - x;
    const double x2 = x * x;
    return x * (1.0 + x2 * (-1.0 / 6.0 + x2 * (1.0 / 120.0 + x2 * (-1.0 / 5040.0 + x2 * (1.0 / 362880.0)))));
}

inline double wrapPhase (double p) noexcept
{
    while (p > pi)  p -= twoPi;
    while (p < -pi) p += twoPi;
    return p;
}

inline double smoothstep (double x) noexcept
{
    x = x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x);
    return x * x * (3.0 - 2.0 * x);
}

//==============================================================================
/** Deterministic white noise. */
class Noise
{
public:
    explicit Noise (uint64_t seed) : rng (seed) {}
    float next() noexcept { return (float) (rng.uniform() * 2.0 - 1.0); }
private:
    Rng rng;
};

//==============================================================================
/** RBJ biquad, transposed direct form II. */
class Biquad
{
public:
    void setLowpass (double f, double q, double sr)  { design (0, f, q, sr, 0.0); }
    void setHighpass (double f, double q, double sr) { design (1, f, q, sr, 0.0); }
    void setBandpass (double f, double q, double sr) { design (2, f, q, sr, 0.0); }
    void setPeak (double f, double q, double gainDb, double sr) { design (3, f, q, sr, gainDb); }

    float process (float x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }

    void reset() noexcept { z1 = z2 = 0.0; }

private:
    void design (int type, double f, double q, double sr, double gainDb)
    {
        f = std::min (f, sr * 0.45);
        f = std::max (f, 5.0);
        const double w0 = twoPi * f / sr;
        const double cs = std::cos (w0), sn = std::sin (w0);
        const double alpha = sn / (2.0 * std::max (0.05, q));
        double nb0 = 0, nb1 = 0, nb2 = 0, na0 = 1, na1 = 0, na2 = 0;

        switch (type)
        {
            case 0: nb0 = (1 - cs) / 2; nb1 = 1 - cs; nb2 = (1 - cs) / 2; na0 = 1 + alpha; na1 = -2 * cs; na2 = 1 - alpha; break;
            case 1: nb0 = (1 + cs) / 2; nb1 = -(1 + cs); nb2 = (1 + cs) / 2; na0 = 1 + alpha; na1 = -2 * cs; na2 = 1 - alpha; break;
            case 2: nb0 = alpha; nb1 = 0; nb2 = -alpha; na0 = 1 + alpha; na1 = -2 * cs; na2 = 1 - alpha; break;
            default:
            {
                const double A = std::pow (10.0, gainDb / 40.0);
                nb0 = 1 + alpha * A; nb1 = -2 * cs; nb2 = 1 - alpha * A;
                na0 = 1 + alpha / A; na1 = -2 * cs; na2 = 1 - alpha / A;
                break;
            }
        }

        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
    }

    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};

//==============================================================================
class OnePole
{
public:
    void setLowpass (double f, double sr) noexcept { a = std::exp (-twoPi * std::min (f, sr * 0.49) / sr); }
    float lowpass (float x) noexcept { z = x * (1.0 - a) + z * a; return (float) z; }
    float highpass (float x) noexcept { return x - lowpass (x); }
    void reset() noexcept { z = 0.0; }
private:
    double a = 0.0, z = 0.0;
};

//==============================================================================
/** Topology-preserving state variable filter (Zavalishin). */
class Svf
{
public:
    void set (double freq, double q, double sr) noexcept
    {
        freq = std::min (freq, sr * 0.48);
        g = std::tan (pi * freq / sr);
        k = 1.0 / std::max (0.1, q);
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void processSample (float x, float& lp, float& bp, float& hp) noexcept
    {
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        lp = (float) v2;
        bp = (float) v1;
        hp = (float) (x - k * v1 - v2);
    }

    void reset() noexcept { ic1 = ic2 = 0.0; }

private:
    double g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
};

//==============================================================================
/** Decaying sinusoidal mode, optionally with an exponential pitch glide (membrane tension). */
struct Mode
{
    double freq;        // final frequency (Hz)
    double amp;
    double t60;         // seconds to -60 dB
    double glideRatio = 1.0;  // start frequency = freq * glideRatio
    double glideTime = 0.02;  // time constant (s)
};

/** Adds a set of modes into `out` starting at sample 0. */
inline void renderModes (std::vector<float>& out, const std::vector<Mode>& modes, double sr, Rng& rng,
                         double attackSec = 0.0004)
{
    const double nyquistLimit = sr * 0.45;
    const int attackSamples = std::max (1, (int) (attackSec * sr));

    for (const auto& m : modes)
    {
        if (m.amp <= 0.0 || m.freq <= 0.0 || m.freq * std::max (1.0, m.glideRatio) >= nyquistLimit)
            continue;

        const int n = std::min ((int) out.size(), (int) (m.t60 * 1.25 * sr) + 1);
        const double envMul = t60Multiplier (m.t60, sr);
        const double glideMul = std::exp (-1.0 / (std::max (1.0e-4, m.glideTime) * sr));
        double g = m.glideRatio - 1.0;
        double env = m.amp;
        double phase = rng.uniform() * 0.3;   // slight random start phase
        const double w = twoPi / sr;

        for (int i = 0; i < n; ++i)
        {
            const double f = m.freq * (1.0 + g);
            g *= glideMul;
            phase = wrapPhase (phase + w * f);
            const double a = i < attackSamples ? (double) i / attackSamples : 1.0;
            out[(size_t) i] += (float) (env * a * fastSin (phase));
            env *= envMul;
        }
    }
}

/** Adds a filtered noise burst with exponential decay. type: 0 LP, 1 HP, 2 BP. */
inline void renderNoiseBurst (std::vector<float>& out, int type, double freq, double q, double decaySec,
                              double amp, double sr, uint64_t seed, int offsetSamples = 0, double attackSec = 0.0003)
{
    Biquad f;
    if (type == 0)      f.setLowpass (freq, q, sr);
    else if (type == 1) f.setHighpass (freq, q, sr);
    else                f.setBandpass (freq, q, sr);

    Noise noise (seed);
    const int n = std::min ((int) out.size() - offsetSamples, (int) (decaySec * 7.0 * sr) + 1);
    const double mul = std::exp (-1.0 / (std::max (1.0e-4, decaySec) * sr));
    const int attackSamples = std::max (1, (int) (attackSec * sr));
    double env = amp;

    for (int i = 0; i < n; ++i)
    {
        const double a = i < attackSamples ? (double) i / attackSamples : 1.0;
        out[(size_t) (i + offsetSamples)] += (float) (f.process (noise.next()) * env * a);
        env *= mul;
    }
}

} // namespace jgl::dsp

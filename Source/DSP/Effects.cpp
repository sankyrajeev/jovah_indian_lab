#include "Effects.h"
#include "DspUtils.h"

#include <juce_dsp/juce_dsp.h>

namespace jgl
{

//==============================================================================
void FdnReverb::prepare (double sampleRate, double t60, double dampingHz, double size, double preDelayMs)
{
    sr = sampleRate;
    static const double baseMs[N] = { 29.7, 37.1, 41.1, 43.7, 53.3, 59.9, 67.7, 79.3 };
    for (int i = 0; i < N; ++i)
    {
        lengths[(size_t) i] = std::max (8, (int) (baseMs[i] * size * 0.001 * sr));
        lines[(size_t) i].assign ((size_t) lengths[(size_t) i], 0.0f);
        // Loss per pass so that the loop decays 60 dB in t60 seconds.
        gains[(size_t) i] = std::pow (10.0, -3.0 * lengths[(size_t) i] / (t60 * sr));
    }

    lpCoeff = std::exp (-dsp::twoPi * dampingHz / sr);

    static const double diffMs[4] = { 4.7, 3.6, 12.7, 9.3 };
    for (int i = 0; i < 4; ++i)
    {
        diffLen[(size_t) i] = std::max (4, (int) (diffMs[i] * 0.001 * sr));
        diffusers[(size_t) i].assign ((size_t) diffLen[(size_t) i], 0.0f);
    }

    preLen = std::max (1, (int) (preDelayMs * 0.001 * sr));
    preDelay.assign ((size_t) preLen, 0.0f);

    hpCoeff = std::exp (-dsp::twoPi * 180.0 / sr);   // keep the low end of the tail clean
    reset();
}

void FdnReverb::reset()
{
    for (auto& l : lines) std::fill (l.begin(), l.end(), 0.0f);
    for (auto& d : diffusers) std::fill (d.begin(), d.end(), 0.0f);
    std::fill (preDelay.begin(), preDelay.end(), 0.0f);
    writePos.fill (0);
    diffPos.fill (0);
    lpState.fill (0.0);
    prePos = 0;
    hpL = hpR = 0.0;
}

void FdnReverb::process (float* left, float* right, int numSamples)
{
    constexpr double diffG = 0.6;

    for (int s = 0; s < numSamples; ++s)
    {
        // Mono pre-delayed input
        float in = 0.5f * (left[s] + right[s]);
        const float delayed = preDelay[(size_t) prePos];
        preDelay[(size_t) prePos] = in;
        prePos = (prePos + 1) % preLen;
        in = delayed;

        // Series all-pass diffusion
        double x = in;
        for (int d = 0; d < 4; ++d)
        {
            auto& buf = diffusers[(size_t) d];
            const double z = buf[(size_t) diffPos[(size_t) d]];
            const double v = x + diffG * z;
            buf[(size_t) diffPos[(size_t) d]] = (float) v;
            x = z - diffG * v;
            diffPos[(size_t) d] = (diffPos[(size_t) d] + 1) % diffLen[(size_t) d];
        }

        // Read lines, damp
        double out[N];
        double sum = 0.0;
        for (int i = 0; i < N; ++i)
        {
            const double v = lines[(size_t) i][(size_t) writePos[(size_t) i]];
            lpState[(size_t) i] = v * (1.0 - lpCoeff) + lpState[(size_t) i] * lpCoeff;
            out[i] = lpState[(size_t) i] * gains[(size_t) i];
            sum += out[i];
        }

        // Householder feedback: y = out - 2/N * sum(out)
        const double h = sum * (2.0 / N);
        for (int i = 0; i < N; ++i)
        {
            const double fbv = out[i] - h + x * 0.35;
            lines[(size_t) i][(size_t) writePos[(size_t) i]] = (float) fbv;
            writePos[(size_t) i] = (writePos[(size_t) i] + 1) % lengths[(size_t) i];
        }

        double l = out[0] - out[2] + out[4] - out[6] + 0.5 * (out[1] - out[7]);
        double r = out[1] - out[3] + out[5] - out[7] + 0.5 * (out[2] - out[4]);

        // High-pass the tail
        hpL = l * (1.0 - hpCoeff) + hpL * hpCoeff;
        hpR = r * (1.0 - hpCoeff) + hpR * hpCoeff;
        left[s]  = (float) ((l - hpL) * 0.55);
        right[s] = (float) ((r - hpR) * 0.55);
    }
}

//==============================================================================
void PingPongDelay::prepare (double sampleRate, double delaySeconds, double feedback)
{
    len = std::max (1, (int) std::lround (delaySeconds * sampleRate));
    bufL.assign ((size_t) len, 0.0f);
    bufR.assign ((size_t) len, 0.0f);
    fb = juce::jlimit (0.0, 0.85, feedback);
    lpA = std::exp (-dsp::twoPi * 5500.0 / sampleRate);
    hpA = std::exp (-dsp::twoPi * 280.0 / sampleRate);
    reset();
}

void PingPongDelay::reset()
{
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    pos = 0;
    lpL = lpR = hpL = hpR = 0.0;
}

void PingPongDelay::process (float* left, float* right, int numSamples)
{
    for (int s = 0; s < numSamples; ++s)
    {
        const float dl = bufL[(size_t) pos];
        const float dr = bufR[(size_t) pos];

        // Filter the feedback path (darker, thinner repeats)
        lpL = dl * (1.0 - lpA) + lpL * lpA;
        lpR = dr * (1.0 - lpA) + lpR * lpA;
        hpL = lpL * (1.0 - hpA) + hpL * hpA;
        hpR = lpR * (1.0 - hpA) + hpR * hpA;
        const double fl = lpL - hpL;
        const double fr = lpR - hpR;

        // Ping-pong: mono input enters left, repeats cross over.
        const double in = 0.5 * (left[s] + right[s]);
        bufL[(size_t) pos] = (float) (in + fr * fb);
        bufR[(size_t) pos] = (float) (fl * fb);
        pos = (pos + 1) % len;

        left[s] = dl;
        right[s] = dr;
    }
}

//==============================================================================
void DjFilter::prepare (double sampleRate, float amount)
{
    active = std::abs (amount) > 0.02f;
    highpass = amount > 0.0f;
    const double a = std::min (1.0, (std::abs (amount) - 0.02) / 0.98);
    const double freq = highpass ? 20.0 * std::pow (2500.0 / 20.0, a)        // 20 Hz .. 2.5 kHz
                                 : 20000.0 * std::pow (250.0 / 20000.0, a);  // 20 kHz .. 250 Hz
    const double f = std::min (freq, sampleRate * 0.45);
    g = std::tan (dsp::pi * f / sampleRate);
    k = 1.0 / 0.9;
    a1 = 1.0 / (1.0 + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
    reset();
}

void DjFilter::reset() { st = {}; }

void DjFilter::process (juce::AudioBuffer<float>& buffer)
{
    if (! active)
        return;

    for (int c = 0; c < std::min (2, buffer.getNumChannels()); ++c)
    {
        auto* d = buffer.getWritePointer (c);
        auto& s = st[(size_t) c];
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const double x = d[i];
            const double v3 = x - s.ic2;
            const double v1 = a1 * s.ic1 + a2 * v3;
            const double v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
            s.ic1 = 2.0 * v1 - s.ic1;
            s.ic2 = 2.0 * v2 - s.ic2;
            d[i] = (float) (highpass ? x - k * v1 - v2 : v2);
        }
    }
}

//==============================================================================
struct Saturator::Impl
{
    static constexpr int chunk = 2048;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
};

Saturator::Saturator() : impl (std::make_unique<Impl>()) {}
Saturator::~Saturator() = default;

void Saturator::prepare (int numChannels, float newAmount)
{
    amount = newAmount;
    impl->os = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) numChannels, 1,
                   juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    impl->os->initProcessing ((size_t) Impl::chunk);
}

void Saturator::process (juce::AudioBuffer<float>& buffer)
{
    if (! isActive() || impl->os == nullptr)
        return;

    const float drive = 1.0f + amount * 5.0f;
    const float norm = 1.0f / std::tanh (drive);
    const float bias = 0.08f * amount;          // gentle asymmetry -> warm even harmonics
    const float biasOut = std::tanh (bias * drive);
    const float mix = std::min (1.0f, amount * 1.5f);

    juce::dsp::AudioBlock<float> whole (buffer);
    for (int start = 0; start < buffer.getNumSamples(); start += Impl::chunk)
    {
        const int len = std::min (Impl::chunk, buffer.getNumSamples() - start);
        auto block = whole.getSubBlock ((size_t) start, (size_t) len);
        auto up = impl->os->processSamplesUp (block);

        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            auto* d = up.getChannelPointer (c);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                const float x = d[i];
                const float wet = (std::tanh ((x + bias) * drive) - biasOut) * norm;
                d[i] = x + (wet - x) * mix;
            }
        }

        impl->os->processSamplesDown (block);
    }
}

} // namespace jgl

#include "SampleAnalysis.h"

#include <algorithm>
#include <vector>

namespace jgl
{

namespace
{
    /** One YIN estimate at `start` (window W). Returns 0 if unvoiced. */
    double yinAt (const float* d, int n, int start, double sr, int W)
    {
        const int minLag = std::max (2, (int) (sr / 1500.0));
        const int maxLag = std::min (W - 1, (int) (sr / 50.0));
        if (start < 0 || start + W + maxLag >= n)
            return 0.0;

        std::vector<double> diff ((size_t) maxLag + 1, 0.0);
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            double s = 0.0;
            for (int i = 0; i < W; ++i)
            {
                const double x = (double) d[start + i] - (double) d[start + i + tau];
                s += x * x;
            }
            diff[(size_t) tau] = s;
        }

        double running = 0.0;
        std::vector<double> cmnd ((size_t) maxLag + 1, 1.0);
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            running += diff[(size_t) tau];
            cmnd[(size_t) tau] = running > 0.0 ? diff[(size_t) tau] * tau / running : 1.0;
        }

        for (int tau = minLag; tau < maxLag - 1; ++tau)
        {
            if (cmnd[(size_t) tau] < 0.12)
            {
                while (tau + 1 < maxLag - 1 && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau]) ++tau;
                const double a = cmnd[(size_t) tau - 1], b = cmnd[(size_t) tau], c = cmnd[(size_t) tau + 1];
                const double den = a - 2.0 * b + c;
                const double refined = tau + (std::abs (den) > 1.0e-12 ? 0.5 * (a - c) / den : 0.0);
                return sr / refined;
            }
        }
        return 0.0;
    }

    double median (std::vector<double> v)
    {
        if (v.empty()) return 0.0;
        std::sort (v.begin(), v.end());
        return v[v.size() / 2];
    }
}

double SampleAnalysis::estimatePitchHz (const juce::AudioBuffer<float>& mono, double sr, int startSample, int endSample)
{
    const int n = mono.getNumSamples();
    const auto* d = mono.getReadPointer (0);
    const int from = startSample >= 0 ? startSample : (int) (0.08 * sr);
    const int to = endSample > 0 ? std::min (endSample, n) : n;
    const int W = 2048;
    std::vector<double> est;
    for (int s = from; s + W < to && est.size() < 24; s += W / 2)
    {
        const double hz = yinAt (d, n, s, sr, W);
        if (hz > 0.0) est.push_back (hz);
    }
    return median (est);
}

SampleAnalysis::Loop SampleAnalysis::findSustainLoop (const juce::AudioBuffer<float>& mono, double sr)
{
    Loop loop;
    const int n = mono.getNumSamples();
    const auto* d = mono.getReadPointer (0);
    const int hop = (int) (0.02 * sr);
    const int W = 1024;
    const int frames = n / hop;
    if (frames < 8)
        return loop;

    std::vector<double> rmsDb ((size_t) frames, -120.0), midi ((size_t) frames, 0.0);
    double maxDb = -120.0;
    for (int f = 0; f < frames; ++f)
    {
        double s = 0.0;
        for (int i = f * hop; i < std::min (n, f * hop + hop); ++i) s += (double) d[i] * d[i];
        rmsDb[(size_t) f] = 10.0 * std::log10 (s / hop + 1.0e-12);
        maxDb = std::max (maxDb, rmsDb[(size_t) f]);
        const double hz = yinAt (d, n, f * hop, sr, W);
        midi[(size_t) f] = hz > 0.0 ? hzToMidi (hz) : 0.0;
    }

    std::vector<double> loudPitches;
    for (int f = 0; f < frames; ++f)
        if (rmsDb[(size_t) f] > maxDb - 10.0 && midi[(size_t) f] > 0.0)
            loudPitches.push_back (midi[(size_t) f]);
    loop.medianMidi = median (loudPitches);
    if (loop.medianMidi <= 0.0)
        return loop;

    // Longest run of steady frames after the attack.
    const int skip = std::max (1, (int) (0.06 * sr / hop));
    int bestStart = -1, bestLen = 0, runStart = -1;
    for (int f = skip; f <= frames; ++f)
    {
        const bool steady = f < frames && rmsDb[(size_t) f] > maxDb - 7.0
                         && midi[(size_t) f] > 0.0 && std::abs (midi[(size_t) f] - loop.medianMidi) < 0.2;
        if (steady && runStart < 0) runStart = f;
        if (! steady && runStart >= 0)
        {
            if (f - runStart > bestLen) { bestLen = f - runStart; bestStart = runStart; }
            runStart = -1;
        }
    }
    if (bestLen * hop < (int) (0.15 * sr))
        return loop;

    const int regionStart = bestStart * hop, regionEnd = (bestStart + bestLen) * hop;
    const int len = regionEnd - regionStart;
    int ls = regionStart + len / 6;
    int le = regionEnd - len / 8;

    // Whole number of periods, then snap to rising zero crossings.
    const double period = sr / (440.0 * std::pow (2.0, (loop.medianMidi - 69.0) / 12.0));
    const int periods = std::max (4, (int) std::floor ((le - ls) / period));
    le = ls + (int) std::lround (periods * period);

    auto risingZero = [&] (int around)
    {
        const int span = (int) period;
        for (int k = 0; k < span; ++k)
            for (int pos : { around + k, around - k })
                if (pos > 0 && pos < n - 1 && d[pos - 1] < 0.0f && d[pos] >= 0.0f)
                    return pos;
        return around;
    };
    ls = risingZero (ls);
    le = risingZero (le);

    const int xf = std::min ((int) (0.03 * sr), (le - ls) / 3);
    if (le - ls > (int) (0.05 * sr) && ls > xf && le < n)
    {
        loop.start = ls;
        loop.end = le;
    }
    return loop;
}

} // namespace jgl

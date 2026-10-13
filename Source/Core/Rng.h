#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace jgl
{

/** SplitMix64 step: used for seeding and for hashing seeds together. */
inline uint64_t splitMix64 (uint64_t& state) noexcept
{
    uint64_t z = (state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

inline uint64_t hashCombine (uint64_t a, uint64_t b) noexcept
{
    uint64_t s = a ^ (b + 0x9E3779B97F4A7C15ull + (a << 6) + (a >> 2));
    return splitMix64 (s);
}

/**
    Deterministic, platform-independent PRNG (xoshiro256**).

    Everything musical in the plugin is derived from instances of this class,
    so a seed plus the generation settings always reproduces the same loop
    on every machine. std::random distributions are deliberately avoided
    because their output is implementation-defined.
*/
class Rng
{
public:
    explicit Rng (uint64_t seed = 1) noexcept { reseed (seed); }

    void reseed (uint64_t seed) noexcept
    {
        uint64_t s = seed;
        for (auto& v : st)
            v = splitMix64 (s);
    }

    uint64_t next() noexcept
    {
        const uint64_t result = rotl (st[1] * 5, 7) * 9;
        const uint64_t t = st[1] << 17;
        st[2] ^= st[0];
        st[3] ^= st[1];
        st[1] ^= st[2];
        st[0] ^= st[3];
        st[2] ^= t;
        st[3] = rotl (st[3], 45);
        return result;
    }

    uint32_t next32() noexcept { return (uint32_t) (next() >> 32); }

    /** Uniform in [0, 1). */
    double uniform() noexcept { return (double) (next() >> 11) * (1.0 / 9007199254740992.0); }
    float uniformf() noexcept { return (float) uniform(); }

    double range (double lo, double hi) noexcept { return lo + (hi - lo) * uniform(); }

    /** Uniform integer in [0, n). */
    int integer (int n) noexcept { return n <= 1 ? 0 : (int) (uniform() * n); }

    bool chance (double p) noexcept { return uniform() < p; }

    /** Standard normal (Box-Muller, cos branch only for determinism). */
    double gaussian() noexcept
    {
        double u1 = uniform();
        if (u1 < 1.0e-12) u1 = 1.0e-12;
        const double u2 = uniform();
        return std::sqrt (-2.0 * std::log (u1)) * std::cos (6.283185307179586 * u2);
    }

    /** Bipolar uniform in [-1, 1). */
    double bipolar() noexcept { return uniform() * 2.0 - 1.0; }

    template <typename Container>
    int weightedIndex (const Container& weights) noexcept
    {
        double total = 0.0;
        for (auto w : weights)
            total += (w > 0 ? (double) w : 0.0);

        if (total <= 0.0)
            return -1;

        double r = uniform() * total;
        int i = 0;
        for (auto w : weights)
        {
            const double ww = (w > 0 ? (double) w : 0.0);
            if (r < ww)
                return i;
            r -= ww;
            ++i;
        }
        return i - 1;
    }

    /** Derive an independent generator, e.g. one per bar or per sub-task. */
    Rng fork (uint64_t salt) const noexcept { return Rng (hashCombine (st[0] ^ rotl (st[2], 13), salt)); }

private:
    static uint64_t rotl (uint64_t x, int k) noexcept { return (x << k) | (x >> (64 - k)); }
    uint64_t st[4] {};
};

/** Fixed per-step random draws. Parameters compare against these, so turning
    e.g. density up only *adds* notes to a pattern instead of reshuffling it. */
inline std::vector<double> stableDraws (uint64_t seed, int count)
{
    Rng r (seed);
    std::vector<double> d ((size_t) count);
    for (auto& v : d)
        v = r.uniform();
    return d;
}

} // namespace jgl

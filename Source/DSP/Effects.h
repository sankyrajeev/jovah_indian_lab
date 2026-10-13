#pragma once

#include <array>
#include <memory>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>

namespace jgl
{

/**
    Offline effects used by the loop renderer. They run on the generation worker,
    never on the audio thread, and are applied *circularly* by the renderer (warm-up
    passes over the loop itself) so reverb and delay tails wrap seamlessly.
*/

/** 8-line feedback delay network reverb with input diffusion and per-line damping. */
class FdnReverb
{
public:
    void prepare (double sampleRate, double t60Seconds, double dampingHz, double size, double preDelayMs);
    void reset();
    /** Processes a stereo send in place (100% wet output). */
    void process (float* left, float* right, int numSamples);

private:
    static constexpr int N = 8;
    double sr = 48000.0;
    std::array<std::vector<float>, N> lines;
    std::array<int, N> lengths {}, writePos {};
    std::array<double, N> gains {}, lpState {};
    double lpCoeff = 0.0;

    std::array<std::vector<float>, 4> diffusers;
    std::array<int, 4> diffLen {}, diffPos {};

    std::vector<float> preDelay;
    int preLen = 1, prePos = 0;

    double hpL = 0.0, hpR = 0.0, hpCoeff = 0.0;
};

/** Tempo-synced stereo ping-pong delay with filtered feedback. Wet only. */
class PingPongDelay
{
public:
    void prepare (double sampleRate, double delaySeconds, double feedback);
    void reset();
    void process (float* left, float* right, int numSamples);

private:
    std::vector<float> bufL, bufR;
    int len = 1, pos = 0;
    double fb = 0.35;
    double lpL = 0, lpR = 0, hpL = 0, hpR = 0, lpA = 0, hpA = 0;
};

/** Master DJ filter: amount < 0 low-pass sweep, > 0 high-pass sweep, ~0 bypass. */
class DjFilter
{
public:
    void prepare (double sampleRate, float amount);
    void reset();
    void process (juce::AudioBuffer<float>& buffer);
    bool isActive() const noexcept { return active; }

private:
    struct State { double ic1 = 0, ic2 = 0; };
    std::array<State, 2> st;
    double g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
    bool highpass = false, active = false;
};

/** Smooth, slightly asymmetric tanh saturation, 2x oversampled. amount 0..1 */
class Saturator
{
public:
    Saturator();
    ~Saturator();
    void prepare (int numChannels, float amount);
    void process (juce::AudioBuffer<float>& buffer);
    bool isActive() const noexcept { return amount > 0.001f; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    float amount = 0.0f;
};

} // namespace jgl

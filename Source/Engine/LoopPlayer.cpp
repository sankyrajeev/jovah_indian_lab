#include "LoopPlayer.h"

namespace jgl
{

LoopPlayer::LoopPlayer() = default;

LoopPlayer::~LoopPlayer()
{
    // Destroyed on the message thread after audio processing has stopped.
    delete current;
    delete previous;
    while (auto* p = toAudio.pop())   delete p;
    while (auto* p = toRelease.pop()) delete p;
}

void LoopPlayer::prepare (double sampleRate)
{
    deviceRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    fadeLength = std::max (64, (int) (0.006 * deviceRate));
}

void LoopPlayer::publish (std::unique_ptr<PlaybackLoop> loop)
{
    collectGarbage();
    if (loop == nullptr)
        return;

    if (waitingToPublish == nullptr && toAudio.push (loop.get()))
    {
        loop.release();
        return;
    }

    // FIFO full (audio not running): keep only the newest, retry from collectGarbage().
    waitingToPublish = std::move (loop);
}

void LoopPlayer::collectGarbage()
{
    while (auto* p = toRelease.pop())
        delete p;

    if (waitingToPublish != nullptr && toAudio.push (waitingToPublish.get()))
        waitingToPublish.release();
}

void LoopPlayer::retire (PlaybackLoop* p) noexcept
{
    if (p != nullptr)
    {
        const bool ok = toRelease.push (p);
        jassert (ok); // guaranteed by the free-space check before accepting loops
        juce::ignoreUnused (ok);
    }
}

float LoopPlayer::readSample (const PlaybackLoop& l, int channel, double p) const noexcept
{
    const int len = l.audio.getNumSamples();
    if (len <= 0) return 0.0f;
    const int ch = std::min (channel, l.audio.getNumChannels() - 1);
    const auto* d = l.audio.getReadPointer (ch);
    const int i0 = (int) p;
    const int i1 = i0 + 1 >= len ? 0 : i0 + 1;
    const float frac = (float) (p - i0);
    return d[i0] + (d[i1] - d[i0]) * frac;
}

void LoopPlayer::process (juce::AudioBuffer<float>& buffer, const HostPosition& host, bool midiStart, bool midiStop) noexcept
{
    const int n = buffer.getNumSamples();
    buffer.clear();

    // 1. Accept new loops (only when the release FIFO can take what we may retire).
    while (toAudio.fifo.getNumReady() > 0 && toRelease.fifo.getFreeSpace() >= 2)
    {
        auto* incoming = toAudio.pop();
        if (incoming == nullptr || incoming->audio.getNumSamples() <= 0)
        {
            retire (incoming);
            continue;
        }

        if (current == nullptr)
        {
            current = incoming;
            pos = 0.0;
            continue;
        }

        const double beats = pos / current->audio.getNumSamples() * current->lengthBeats;
        retire (previous);
        previous = current;
        prevPos = pos;
        current = incoming;
        pos = std::fmod (beats, current->lengthBeats) / current->lengthBeats * current->audio.getNumSamples();
        pos = juce::jlimit (0.0, (double) current->audio.getNumSamples() - 1.0, pos);
        fadeRemaining = fadeLength;
    }

    if (current == nullptr)
    {
        playhead.store (-1.0f);
        audible.store (false);
        return;
    }

    // 2. Transport
    if (midiStart) { midiPlaying = true; if (! wasPlaying) pos = 0.0; }
    if (midiStop)  midiPlaying = false;

    const bool hostDriven = syncToHost.load() && host.valid && host.playing;
    const bool wantPlay = playRequested.load() || midiPlaying || hostDriven;

    if (restartRequested.exchange (false) && ! wasPlaying && ! hostDriven)
        pos = 0.0;

    const double len = (double) current->audio.getNumSamples();

    if (hostDriven)
    {
        double beats = std::fmod (host.ppq, current->lengthBeats);
        if (beats < 0.0) beats += current->lengthBeats;
        pos = juce::jlimit (0.0, len - 1.0, beats / current->lengthBeats * len);

        if (previous != nullptr)
        {
            const double plen = (double) previous->audio.getNumSamples();
            double pb = std::fmod (host.ppq, previous->lengthBeats);
            if (pb < 0.0) pb += previous->lengthBeats;
            prevPos = juce::jlimit (0.0, plen - 1.0, pb / previous->lengthBeats * plen);
        }
    }

    if (! wantPlay && outGain <= 0.0f)
    {
        playhead.store (-1.0f);
        audible.store (false);
        wasPlaying = false;
        if (previous != nullptr && fadeRemaining <= 0) { retire (previous); previous = nullptr; }
        return;
    }

    const double rate = current->sampleRate / deviceRate;
    const double prevRate = previous != nullptr ? previous->sampleRate / deviceRate : 1.0;
    const float target = wantPlay ? 1.0f : 0.0f;
    const float gainStep = 1.0f / (float) fadeLength;

    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        if (outGain < target)      outGain = std::min (target, outGain + gainStep);
        else if (outGain > target) outGain = std::max (target, outGain - gainStep);

        float l = readSample (*current, 0, pos);
        float r = readSample (*current, 1, pos);

        if (previous != nullptr && fadeRemaining > 0)
        {
            const float t = (float) fadeRemaining / (float) fadeLength;   // 1 -> 0
            l = l * (1.0f - t) + readSample (*previous, 0, prevPos) * t;
            r = r * (1.0f - t) + readSample (*previous, 1, prevPos) * t;
            prevPos += prevRate;
            const double plen = (double) previous->audio.getNumSamples();
            if (prevPos >= plen) prevPos -= plen;
            if (--fadeRemaining == 0)
            {
                retire (previous);
                previous = nullptr;
            }
        }

        outL[i] = l * outGain;
        if (outR != nullptr) outR[i] = r * outGain;

        pos += rate;
        if (pos >= len) pos -= len;
    }

    playhead.store ((float) (pos / len));
    audible.store (outGain > 0.0f);
    playingId.store (current->id);
    wasPlaying = wantPlay;
}

} // namespace jgl

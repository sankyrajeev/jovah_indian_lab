#pragma once

#include <array>
#include <atomic>
#include <memory>

#include <juce_audio_basics/juce_audio_basics.h>

namespace jgl
{

/** Audio the real-time thread plays: a copy of a rendered loop. */
struct PlaybackLoop
{
    PlaybackLoop() { ++liveInstances; }
    ~PlaybackLoop() { --liveInstances; }
    PlaybackLoop (const PlaybackLoop&) = delete;
    PlaybackLoop& operator= (const PlaybackLoop&) = delete;

    /** Instance counter, used by tests to prove nothing leaks across the audio-thread hand-off. */
    static inline std::atomic<int> liveInstances { 0 };

    juce::AudioBuffer<float> audio;
    double sampleRate = 48000.0;
    double lengthBeats = 16.0;
    uint64_t id = 0;
};

/**
    Real-time loop playback.

    Threading contract
    - publish() / collectGarbage() / setPlaying(): message thread only.
    - process(): audio thread only. It never allocates, frees, locks or does I/O.

    New loops travel to the audio thread through a lock-free single-producer /
    single-consumer FIFO of raw pointers; loops the audio thread is done with travel back
    through a second FIFO and are deleted on the message thread. The audio thread only
    accepts a new loop when the release FIFO has room, so nothing can ever leak or be
    freed on the audio thread.

    Swaps crossfade over ~6 ms and keep the musical position, so regenerating while
    playing never clicks. When the loop's sample rate differs from the device rate
    (e.g. right after a sample-rate change) playback resamples until the re-render lands.
*/
class LoopPlayer
{
public:
    LoopPlayer();
    ~LoopPlayer();

    struct HostPosition
    {
        bool valid = false;
        bool playing = false;
        double ppq = 0.0;
    };

    void prepare (double sampleRate);

    // Message thread ----------------------------------------------------------
    void publish (std::unique_ptr<PlaybackLoop> loop);
    void collectGarbage();
    void setPlaying (bool shouldPlay) noexcept { playRequested.store (shouldPlay); if (shouldPlay) restartRequested.store (true); }
    void setSyncToHost (bool sync) noexcept { syncToHost.store (sync); }
    bool isSyncedToHost() const noexcept { return syncToHost.load(); }

    // Any thread --------------------------------------------------------------
    bool isPlayRequested() const noexcept { return playRequested.load(); }
    bool isAudible() const noexcept { return audible.load(); }
    /** 0..1 position in the current loop, -1 when stopped. */
    float getPlayheadFraction() const noexcept { return playhead.load(); }
    uint64_t getPlayingLoopId() const noexcept { return playingId.load(); }

    // Audio thread -------------------------------------------------------------
    void process (juce::AudioBuffer<float>& buffer, const HostPosition& host, bool midiStart, bool midiStop) noexcept;

private:
    static constexpr int fifoSize = 16;

    struct PtrFifo
    {
        juce::AbstractFifo fifo { fifoSize };
        std::array<PlaybackLoop*, fifoSize> items {};

        bool push (PlaybackLoop* p) noexcept
        {
            if (fifo.getFreeSpace() < 1) return false;
            const auto scope = fifo.write (1);
            if (scope.blockSize1 > 0) items[(size_t) scope.startIndex1] = p;
            else                      items[(size_t) scope.startIndex2] = p;
            return true;
        }

        PlaybackLoop* pop() noexcept
        {
            if (fifo.getNumReady() < 1) return nullptr;
            PlaybackLoop* p = nullptr;
            const auto scope = fifo.read (1);
            if (scope.blockSize1 > 0) p = items[(size_t) scope.startIndex1];
            else                      p = items[(size_t) scope.startIndex2];
            return p;
        }
    };

    void retire (PlaybackLoop* p) noexcept;
    float readSample (const PlaybackLoop& l, int channel, double pos) const noexcept;

    PtrFifo toAudio, toRelease;
    std::unique_ptr<PlaybackLoop> waitingToPublish;   // message thread only

    // Audio-thread state
    PlaybackLoop* current = nullptr;
    PlaybackLoop* previous = nullptr;
    double pos = 0.0, prevPos = 0.0;
    int fadeRemaining = 0, fadeLength = 256;
    float outGain = 0.0f;
    bool wasPlaying = false;
    bool midiPlaying = false;
    double deviceRate = 48000.0;

    std::atomic<bool> playRequested { false }, restartRequested { false }, syncToHost { true }, audible { false };
    std::atomic<float> playhead { -1.0f };
    std::atomic<uint64_t> playingId { 0 };
};

} // namespace jgl

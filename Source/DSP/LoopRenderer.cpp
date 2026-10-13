#include "LoopRenderer.h"

#include "../Core/Instruments.h"
#include "../Core/Scales.h"
#include "../Core/Styles.h"
#include "../Generation/GrooveEngine.h"
#include "DspUtils.h"
#include "Effects.h"
#include "SampleLibrary.h"
#include "Voices.h"

#include <map>

namespace jgl
{

namespace
{
    bool cancelled (const LoopRenderer::Options& o)
    {
        return o.cancel != nullptr && o.cancel->load (std::memory_order_relaxed);
    }

    /** Runs a stateful processor over the loop until it reaches steady state, keeping the last pass. */
    template <typename Fn>
    void processCircular (juce::AudioBuffer<float>& buf, double warmupSeconds, double sr, Fn&& fn)
    {
        const int len = buf.getNumSamples();
        if (len <= 0) return;
        const int passes = juce::jlimit (1, 64, (int) std::ceil (warmupSeconds * sr / len));
        juce::AudioBuffer<float> tmp;
        for (int p = 0; p < passes; ++p)
        {
            tmp.makeCopyOf (buf);
            fn (tmp);
        }
        fn (buf);
    }

    void addCircular (juce::AudioBuffer<float>& dest, const std::vector<float>& mono, juce::int64 start, float pan)
    {
        const auto len = (juce::int64) dest.getNumSamples();
        if (len <= 0 || mono.empty()) return;

        const float angle = (juce::jlimit (-1.0f, 1.0f, pan) + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        const float gl = std::cos (angle) * 1.41421356f * 0.7071f;
        const float gr = std::sin (angle) * 1.41421356f * 0.7071f;
        auto* l = dest.getWritePointer (0);
        auto* r = dest.getWritePointer (1);

        juce::int64 pos = ((start % len) + len) % len;
        for (size_t i = 0; i < mono.size(); ++i)
        {
            const float v = mono[i];
            l[pos] += v * gl;
            r[pos] += v * gr;
            if (++pos == len) pos = 0;
        }
    }

    float panOffset (InstrumentId id, const NoteEvent& e)
    {
        switch (id)
        {
            case InstrumentId::Tabla:
                return isBassArticulation (id, e.articulation) && e.articulation != TablaArt::Dha && e.articulation != TablaArt::Dhin ? -0.1f : 0.06f;
            case InstrumentId::Dhol:
                return (e.articulation == DholArt::Tilli || e.articulation == DholArt::TilliEdge) ? 0.12f : 0.0f;
            case InstrumentId::Tanpura:
            {
                if (e.pitch <= -12) return 0.35f;
                if (e.pitch < 0)    return -0.4f;
                const int beatInBar = (int) std::lround (std::fmod (e.beat, 4.0));
                return beatInBar == 1 ? 0.2f : -0.12f;
            }
            default:
                return 0.0f;
        }
    }

    /** Sympathetic (tarab) strings: a bank of resonators tuned to the scale, excited by the sitar. */
    void applyTarab (juce::AudioBuffer<float>& part, int keyRoot, int scaleIndex, double sr)
    {
        const auto& scale = scaleProfile (scaleIndex);
        const double saMidi = LoopRenderer::tonicMidi (InstrumentId::Sitar, keyRoot) + 12.0;
        std::vector<double> freqs;
        for (int p : scale.notes)
        {
            if (freqs.size() >= 7) break;
            freqs.push_back (dsp::midiToHz (saMidi + p));
        }

        struct Res { std::vector<float> buf; int len = 1, pos = 0; double g = 0, lp = 0; };
        std::vector<Res> res (freqs.size());
        for (size_t i = 0; i < freqs.size(); ++i)
        {
            res[i].len = std::max (2, (int) std::lround (sr / freqs[i]));
            res[i].buf.assign ((size_t) res[i].len, 0.0f);
            res[i].g = std::pow (10.0, -3.0 * res[i].len / (1.8 * sr));
        }

        processCircular (part, 4.0, sr, [&] (juce::AudioBuffer<float>& b)
        {
            auto* l = b.getWritePointer (0);
            auto* r = b.getWritePointer (1);
            for (int s = 0; s < b.getNumSamples(); ++s)
            {
                const float in = 0.5f * (l[s] + r[s]) * 0.05f;
                double wl = 0.0, wr = 0.0;
                for (size_t i = 0; i < res.size(); ++i)
                {
                    auto& rz = res[i];
                    const double y = rz.buf[(size_t) rz.pos];
                    rz.lp = 0.5 * (y + rz.lp);                    // string damping
                    rz.buf[(size_t) rz.pos] = (float) (in + rz.g * rz.lp);
                    rz.pos = (rz.pos + 1) % rz.len;
                    (i & 1 ? wr : wl) += y;
                }
                l[s] += (float) (wl * 0.6);
                r[s] += (float) (wr * 0.6);
            }
        });
    }

    /** 2nd-order high-pass applied circularly (keeps the low end for the drums). */
    void highPassCircular (juce::AudioBuffer<float>& buf, double freq, double sr)
    {
        dsp::Biquad f[2];
        for (auto& b : f) b.setHighpass (freq, 0.707, sr);
        processCircular (buf, 0.2, sr, [&] (juce::AudioBuffer<float>& b)
        {
            for (int c = 0; c < 2; ++c)
            {
                auto* d = b.getWritePointer (c);
                for (int i = 0; i < b.getNumSamples(); ++i)
                    d[i] = f[c].process (d[i]);
            }
        });
    }

    bool renderWithSamples (const SampleLibrary* lib, const VoiceRequest& req, double midi, std::vector<float>& out)
    {
        if (lib == nullptr)
            return false;

        const auto id = req.instrument;
        if (isMelodic (id))
        {
            if (const auto* s = lib->pickNote (id, midi, req.seed))
            {
                SampleLibrary::renderNote (*s, midi, req.velocity, req.durationSec, req.glideFromSemis, req.glideSec, req.sampleRate, out);
                return true;
            }
            return false;
        }

        auto renderOne = [&] (int art, float vel, std::vector<float>& dst)
        {
            if (const auto* s = lib->pick (id, art, vel, req.seed + (uint32_t) art))
            {
                SampleLibrary::renderHit (*s, vel, lib->numLayers (id, art), req.sampleRate, dst);
                return true;
            }
            return false;
        };

        if (lib->hasArticulation (id, req.articulation))
            return renderOne (req.articulation, req.velocity, out);

        // Compound strokes can be built from their components.
        int a = -1, b = -1;
        if (id == InstrumentId::Tabla && req.articulation == TablaArt::Dha)  { a = TablaArt::Na;  b = TablaArt::Ge; }
        if (id == InstrumentId::Tabla && req.articulation == TablaArt::Dhin) { a = TablaArt::Tin; b = TablaArt::Ge; }
        if (id == InstrumentId::Dhol && req.articulation == DholArt::Both)   { a = DholArt::Tilli; b = DholArt::Dagga; }

        if (a >= 0 && lib->hasArticulation (id, a) && lib->hasArticulation (id, b))
        {
            std::vector<float> second;
            renderOne (a, req.velocity, out);
            renderOne (b, req.velocity * 0.9f * req.bassGain, second);
            if (second.size() > out.size()) out.resize (second.size(), 0.0f);
            for (size_t i = 0; i < second.size(); ++i) out[i] += second[i];
            return true;
        }
        return false;
    }
}

//==============================================================================
double LoopRenderer::tonicMidi (InstrumentId id, int keyRoot)
{
    const int pc = pitchClass (keyRoot);
    switch (id)
    {
        case InstrumentId::Sitar:   return 48 + pc;                    // C3 .. B3
        case InstrumentId::Tanpura: return 48 + pc;
        case InstrumentId::Bansuri: return 60 + pc;                    // C4 .. B4
        case InstrumentId::Nadaswaram: return 57 + pitchClass (pc - 9); // A3 .. G#4
        case InstrumentId::Tumbi:   return 67 + pitchClass (pc - 7);   // G4 .. F#5
        default:                    return 60 + pc;
    }
}

double LoopRenderer::tablaSaHz (int keyRoot)
{
    return dsp::midiToHz (57 + pitchClass (keyRoot - 9));             // A3 .. G#4
}

bool LoopRenderer::render (const Arrangement& a, const RenderSettings& rs, juce::AudioBuffer<float>& out, const Options& opt)
{
    const double sr = rs.sampleRate;
    if (sr < 8000.0 || rs.bpm < 20.0 || a.bars < 1)
        return false;

    const auto L = loopLengthSamples (a.bars, rs.bpm, sr);
    if (L <= 0 || L > (juce::int64) std::numeric_limits<int>::max())
        return false;

    const int len = (int) L;
    out.setSize (2, len, false, true, false);
    out.clear();

    juce::AudioBuffer<float> reverbSend (2, len), delaySend (2, len), partBuf (2, len);
    reverbSend.clear();
    delaySend.clear();

    const auto& style = styleProfile (a.styleIndex);
    GrooveEngine groove (rs, style);
    const double secPerBeat = 60.0 / rs.bpm;

    // Dhol bass onsets: other drums' bass strokes duck under them (low-end management).
    std::vector<double> daggaTimes;
    if (const auto* dhol = a.findPart (InstrumentId::Dhol))
        for (auto& e : dhol->events)
            if (isBassArticulation (InstrumentId::Dhol, e.articulation))
                daggaTimes.push_back (groove.onsetSeconds (e, *dhol));

    const double loopSec = L / sr;
    auto nearDagga = [&] (double t)
    {
        for (double d : daggaTimes)
        {
            double diff = std::abs (t - d);
            diff = std::min (diff, loopSec - diff);
            if (diff < 0.035) return true;
        }
        return false;
    };

    std::vector<float> mono;

    // Drone plucks repeat every bar: render each string twice (two takes) and re-use them.
    std::map<std::pair<int, int>, std::pair<float, std::vector<float>>> droneCache;

    for (const auto& part : a.parts)
    {
        if (cancelled (opt))
            return false;

        partBuf.clear();
        const auto id = part.instrument;
        const auto& info = instrumentInfo (id);

        for (const auto& e : part.events)
        {
            VoiceRequest req;
            req.instrument = id;
            req.articulation = e.articulation;
            req.velocity = groove.velocity (e, id);
            req.sampleRate = sr;
            req.seed = e.seed;
            req.tablaSaHz = tablaSaHz (rs.keyRoot);

            double onset = groove.onsetSeconds (e, part);
            double midi = 0.0;

            if (info.melodic)
            {
                midi = tonicMidi (id, rs.keyRoot) + e.pitch;
                req.frequency = dsp::midiToHz (midi);
                req.durationSec = std::max (0.03, groove.endSeconds (e, part) - onset);
                req.glideFromSemis = e.glideFrom;
                req.glideSec = e.glideBeats * secPerBeat;
                req.ornament = e.ornament;
                req.ornamentSemis = e.ornamentPitch - e.pitch;
                onset -= Voices::leadInSeconds (req);
            }
            else if (id != InstrumentId::Dhol && isBassArticulation (id, e.articulation) && nearDagga (onset))
            {
                req.bassGain = 0.5f;
            }

            if (! renderWithSamples (opt.samples, req, midi, mono))
            {
                if (id == InstrumentId::Tanpura)
                {
                    const auto key = std::make_pair (e.pitch, (int) (e.seed & 1u));
                    auto it = droneCache.find (key);
                    if (it == droneCache.end())
                    {
                        Voices::render (req, mono);
                        it = droneCache.emplace (key, std::make_pair (req.velocity, mono)).first;
                    }
                    const float scale = req.velocity / std::max (0.05f, it->second.first);
                    mono = it->second.second;
                    for (auto& v : mono) v *= scale;
                }
                else
                {
                    Voices::render (req, mono);
                }
            }

            addCircular (partBuf, mono, (juce::int64) std::llround (onset * sr), info.pan + panOffset (id, e));
        }

        if (id == InstrumentId::Sitar)
            applyTarab (partBuf, rs.keyRoot, a.scaleIndex, sr);

        // Keep the sub range for the drums.
        if (id == InstrumentId::Tanpura || id == InstrumentId::Sitar || id == InstrumentId::Bansuri || id == InstrumentId::Nadaswaram)
            highPassCircular (partBuf, id == InstrumentId::Tanpura ? 85.0 : (id == InstrumentId::Nadaswaram ? 120.0 : 70.0), sr);

        const float gain = dsp::dbToGain (info.gainDb);
        for (int c = 0; c < 2; ++c)
        {
            out.addFrom (c, 0, partBuf, c, 0, len, gain);
            reverbSend.addFrom (c, 0, partBuf, c, 0, len, gain * info.reverbSend);
            delaySend.addFrom (c, 0, partBuf, c, 0, len, gain * info.delaySend);
        }
    }

    if (cancelled (opt))
        return false;

    // Delay (feeds into the reverb a little for depth)
    if (rs.delay > 0.001f)
    {
        PingPongDelay delay;
        const double delaySec = delayTimeBeats (rs.delayTime) * secPerBeat;
        const double fb = 0.25 + 0.35 * rs.delay;
        delay.prepare (sr, delaySec, fb);
        const double warm = delaySec * std::log (0.001) / std::log (std::max (0.01, fb));
        processCircular (delaySend, warm, sr, [&] (juce::AudioBuffer<float>& b)
        {
            delay.process (b.getWritePointer (0), b.getWritePointer (1), b.getNumSamples());
        });
        const float wet = rs.delay * 0.9f;
        for (int c = 0; c < 2; ++c)
        {
            out.addFrom (c, 0, delaySend, c, 0, len, wet);
            reverbSend.addFrom (c, 0, delaySend, c, 0, len, wet * 0.3f);
        }
    }

    if (rs.reverb > 0.001f)
    {
        FdnReverb reverb;
        const double t60 = 1.1 + 1.4 * rs.reverb;
        reverb.prepare (sr, t60, 6500.0, 1.0, 18.0);
        processCircular (reverbSend, t60 * 1.2, sr, [&] (juce::AudioBuffer<float>& b)
        {
            reverb.process (b.getWritePointer (0), b.getWritePointer (1), b.getNumSamples());
        });
        for (int c = 0; c < 2; ++c)
            out.addFrom (c, 0, reverbSend, c, 0, len, rs.reverb);
    }

    if (cancelled (opt))
        return false;

    // Master: DC / sub-rumble high-pass, DJ filter, then saturation
    highPassCircular (out, 22.0, sr);
    {
        DjFilter filter;
        filter.prepare (sr, rs.filter);
        if (filter.isActive())
            processCircular (out, 0.05, sr, [&] (juce::AudioBuffer<float>& b) { filter.process (b); });

        Saturator sat;
        sat.prepare (2, rs.saturation);
        if (sat.isActive())
            processCircular (out, 0.01, sr, [&] (juce::AudioBuffer<float>& b) { sat.process (b); });
    }

    // Normalise to the ceiling, then apply the master level (<= 0 dB, so never clips).
    if (opt.normalise)
    {
        const float peak = juce::jmax (out.getMagnitude (0, 0, len), out.getMagnitude (1, 0, len));
        if (peak > 1.0e-6f)
            out.applyGain (dsp::dbToGain (normaliseCeilingDb) / peak);
    }
    out.applyGain (dsp::dbToGain (juce::jmin (0.0f, rs.levelDb)));

    return true;
}

} // namespace jgl

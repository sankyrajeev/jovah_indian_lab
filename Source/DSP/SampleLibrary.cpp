#include "SampleLibrary.h"
#include "../Core/Rng.h"
#include "DspUtils.h"

namespace jgl
{

namespace
{
    int parseLayer (const juce::String& name)
    {
        const auto lower = name.toLowerCase();
        for (int v = 1; v <= 8; ++v)
        {
            const auto tag = "_v" + juce::String (v);
            const int pos = lower.indexOf (tag);
            if (pos >= 0)
            {
                const int after = pos + tag.length();
                if (after >= lower.length() || ! juce::CharacterFunctions::isDigit (lower[after]))
                    return v;
            }
        }
        return 0;
    }

    std::unique_ptr<SampleLibrary::Sample> loadFile (juce::AudioFormatManager& fm, const juce::File& f)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
        if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
            return nullptr;

        const auto maxLen = (juce::int64) (reader->sampleRate * 12.0);
        const int len = (int) std::min (reader->lengthInSamples, maxLen);
        const int ch = (int) std::max (1u, reader->numChannels);

        juce::AudioBuffer<float> tmp (ch, len);
        reader->read (&tmp, 0, len, 0, true, ch > 1);

        auto s = std::make_unique<SampleLibrary::Sample>();
        s->data.setSize (1, len);
        s->data.clear();
        for (int c = 0; c < ch; ++c)
            s->data.addFrom (0, 0, tmp, c, 0, len, 1.0f / (float) ch);
        s->sampleRate = reader->sampleRate;
        s->name = f.getFileName();

        // Short fade-out if the file was trimmed, to avoid clicks.
        const int fade = std::min (len, (int) (0.01 * s->sampleRate));
        if (reader->lengthInSamples > maxLen)
            s->data.applyGainRamp (0, len - fade, fade, 1.0f, 0.0f);

        return s;
    }

    float readInterpolated (const juce::AudioBuffer<float>& b, double pos)
    {
        const int n = b.getNumSamples();
        const int i = (int) pos;
        if (i < 0 || i >= n - 1)
            return 0.0f;
        const float frac = (float) (pos - i);
        const auto* d = b.getReadPointer (0);
        return d[i] + (d[i + 1] - d[i]) * frac;
    }
}

int SampleLibrary::parseRootNote (const juce::String& name)
{
    // Prefer note names (C3, F#4, Bb2) in the last token, then plain MIDI numbers.
    juce::StringArray tokens;
    tokens.addTokens (name, "_- .", "");
    for (int t = tokens.size(); --t >= 0;)
    {
        const auto tok = tokens[t].trim();
        if (tok.isEmpty()) continue;

        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        static const char* flats[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        for (int pc = 11; pc >= 0; --pc)
        {
            for (auto* table : { names, flats })
            {
                const juce::String n (table[pc]);
                if (tok.startsWithIgnoreCase (n) && tok.length() > n.length())
                {
                    const auto rest = tok.substring (n.length());
                    if (rest.containsOnly ("-0123456789") && rest != "-")
                        return juce::jlimit (0, 127, (rest.getIntValue() + 1) * 12 + pc);
                }
            }
        }

        if (tok.containsOnly ("0123456789") && tok.length() <= 3)
        {
            const int v = tok.getIntValue();
            if (v >= 12 && v <= 120)
                return v;
        }
    }
    return -1;
}

int SampleLibrary::load (const juce::File& root)
{
    samples.clear();
    total = 0;

    if (! root.isDirectory())
        return 0;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    const juce::String wildcard = "*.wav;*.aif;*.aiff;*.flac";

    for (auto id : allInstruments())
    {
        const auto& info = instrumentInfo (id);
        const auto instDir = root.getChildFile (info.key);
        if (! instDir.isDirectory())
            continue;

        if (info.melodic)
        {
            for (auto& f : instDir.getChildFile ("notes").findChildFiles (juce::File::findFiles, false, wildcard))
            {
                const int rootNote = parseRootNote (f.getFileNameWithoutExtension());
                if (rootNote < 0)
                    continue;
                if (auto s = loadFile (fm, f))
                {
                    s->rootMidi = rootNote;
                    samples[{ toIndex (id), -1 }].push_back (std::move (s));
                    ++total;
                }
            }
            continue;
        }

        for (int a = 0; a < (int) info.articulations.size(); ++a)
        {
            const auto dir = instDir.getChildFile (info.articulations[(size_t) a]);
            if (! dir.isDirectory())
                continue;

            auto files = dir.findChildFiles (juce::File::findFiles, false, wildcard);
            files.sort();
            for (auto& f : files)
            {
                if (auto s = loadFile (fm, f))
                {
                    s->layer = parseLayer (f.getFileNameWithoutExtension());
                    samples[{ toIndex (id), a }].push_back (std::move (s));
                    ++total;
                }
            }
        }
    }

    return total;
}

bool SampleLibrary::hasArticulation (InstrumentId id, int a) const
{
    auto it = samples.find ({ toIndex (id), a });
    return it != samples.end() && ! it->second.empty();
}

bool SampleLibrary::hasMelodicNotes (InstrumentId id) const { return hasArticulation (id, -1); }

int SampleLibrary::numLayers (InstrumentId id, int a) const
{
    auto it = samples.find ({ toIndex (id), a });
    if (it == samples.end()) return 0;
    int maxLayer = 0;
    for (auto& s : it->second) maxLayer = std::max (maxLayer, s->layer);
    return std::max (1, maxLayer);
}

const SampleLibrary::Sample* SampleLibrary::pick (InstrumentId id, int a, float velocity, uint32_t seed) const
{
    auto it = samples.find ({ toIndex (id), a });
    if (it == samples.end() || it->second.empty())
        return nullptr;

    const auto& list = it->second;
    int maxLayer = 0;
    for (auto& s : list) maxLayer = std::max (maxLayer, s->layer);

    int wantLayer = 0;
    if (maxLayer > 0)
        wantLayer = juce::jlimit (1, maxLayer, 1 + (int) (velocity * (float) maxLayer * 0.999f));

    std::vector<const Sample*> takes;
    for (auto& s : list)
        if (maxLayer == 0 || s->layer == wantLayer || s->layer == 0)
            takes.push_back (s.get());
    if (takes.empty())
        for (auto& s : list) takes.push_back (s.get());

    // Seeds are unique per event, so consecutive hits get statistically independent takes;
    // mixing in the take count avoids the same index for identical seeds across layers.
    Rng r (hashCombine (seed, (uint64_t) takes.size()));
    return takes[(size_t) r.integer ((int) takes.size())];
}

const SampleLibrary::Sample* SampleLibrary::pickNote (InstrumentId id, double midi, uint32_t seed) const
{
    auto it = samples.find ({ toIndex (id), -1 });
    if (it == samples.end() || it->second.empty())
        return nullptr;

    double best = 1.0e9;
    std::vector<const Sample*> nearest;
    for (auto& s : it->second)
    {
        const double d = std::abs (s->rootMidi - midi);
        if (d < best - 0.01) { best = d; nearest.clear(); }
        if (d <= best + 0.01) nearest.push_back (s.get());
    }
    Rng r (seed);
    return nearest[(size_t) r.integer ((int) nearest.size())];
}

void SampleLibrary::renderHit (const Sample& s, float velocity, int numLayers, double sampleRate, std::vector<float>& out)
{
    const double ratio = s.sampleRate / sampleRate;
    const int len = (int) (s.data.getNumSamples() / ratio);
    out.assign ((size_t) std::max (0, len), 0.0f);

    // Within a layer, scale gain by velocity relative to the layer's nominal velocity.
    float gain = std::pow (juce::jlimit (0.0f, 1.0f, velocity), 1.4f);
    if (numLayers > 1 && s.layer > 0)
    {
        const float nominal = (float) s.layer / (float) numLayers;
        gain = juce::jlimit (0.1f, 1.5f, std::pow (velocity / nominal, 1.2f));
    }

    for (int i = 0; i < len; ++i)
        out[(size_t) i] = readInterpolated (s.data, i * ratio) * gain;
}

void SampleLibrary::renderNote (const Sample& s, double targetMidi, float velocity, double durationSec,
                                float glideFromSemis, double glideSec, double sampleRate, std::vector<float>& out)
{
    const double release = 0.04;
    const int len = (int) ((durationSec + release) * sampleRate);
    out.assign ((size_t) std::max (0, len), 0.0f);

    const double baseRatio = s.sampleRate / sampleRate;
    const float gain = std::pow (juce::jlimit (0.0f, 1.0f, velocity), 1.2f);
    double pos = 0.0;

    for (int i = 0; i < len; ++i)
    {
        const double t = i / sampleRate;
        double semis = targetMidi - s.rootMidi;
        if (glideSec > 0.0)
            semis += glideFromSemis * (1.0 - dsp::smoothstep (t / glideSec));

        float env = 1.0f;
        if (t > durationSec)
            env = (float) std::max (0.0, 1.0 - (t - durationSec) / release);

        out[(size_t) i] = readInterpolated (s.data, pos) * gain * env;
        pos += baseRatio * std::pow (2.0, semis / 12.0);
        if (pos >= s.data.getNumSamples() - 1)
            break;
    }
}

juce::StringArray SampleLibrary::describe() const
{
    juce::StringArray lines;
    for (auto id : allInstruments())
    {
        int n = 0;
        for (auto& [key, list] : samples)
            if (key.first == toIndex (id))
                n += (int) list.size();
        if (n > 0)
            lines.add (juce::String (instrumentInfo (id).name) + ": " + juce::String (n) + " samples");
    }
    return lines;
}

} // namespace jgl

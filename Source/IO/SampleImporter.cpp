#include "SampleImporter.h"

#include "../DSP/SampleLibrary.h"
#include "WavExporter.h"
#include "../DSP/SampleAnalysis.h"

#include <algorithm>

namespace jgl
{

namespace
{
    struct Hit
    {
        juce::AudioBuffer<float> audio;   // mono
        double sampleRate = 48000.0;
        float peak = 0.0f;
        juce::String source;
    };

    bool readMono (juce::AudioFormatManager& fm, const juce::File& f, juce::AudioBuffer<float>& out, double& sr)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
        if (reader == nullptr || reader->lengthInSamples <= 0)
            return false;
        const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 600.0));
        const int ch = (int) std::max (1u, reader->numChannels);
        juce::AudioBuffer<float> tmp (ch, len);
        reader->read (&tmp, 0, len, 0, true, ch > 1);
        out.setSize (1, len);
        out.clear();
        for (int c = 0; c < ch; ++c)
            out.addFrom (0, 0, tmp, c, 0, len, 1.0f / (float) ch);
        sr = reader->sampleRate;
        return true;
    }

    int firstAbove (const juce::AudioBuffer<float>& b, int from, int to, float level)
    {
        const auto* d = b.getReadPointer (0);
        for (int i = from; i < to; ++i)
            if (std::abs (d[i]) >= level) return i;
        return to;
    }

    juce::AudioBuffer<float> cut (const juce::AudioBuffer<float>& src, int start, int end, double sr)
    {
        const int len = std::max (1, end - start);
        juce::AudioBuffer<float> b (1, len);
        b.copyFrom (0, 0, src, 0, start, len);
        const int fadeIn = std::min (len, (int) (0.001 * sr));
        const int fadeOut = std::min (len, (int) (0.012 * sr));
        b.applyGainRamp (0, fadeIn, 0.0f, 1.0f);
        b.applyGainRamp (len - fadeOut, fadeOut, 1.0f, 0.0f);
        return b;
    }

    juce::Array<juce::File> expand (const juce::Array<juce::File>& inputs)
    {
        juce::Array<juce::File> files;
        for (auto& f : inputs)
        {
            if (f.isDirectory())
            {
                auto found = f.findChildFiles (juce::File::findFiles, true, "*.wav;*.aif;*.aiff;*.flac");
                found.sort();
                files.addArray (found);
            }
            else if (f.existsAsFile())
                files.add (f);
        }
        return files;
    }
}

//==============================================================================
std::vector<int> SampleImporter::detectOnsets (const juce::AudioBuffer<float>& mono, double sr)
{
    const int hop = 128;
    const int n = mono.getNumSamples();
    const int frames = n / hop;
    std::vector<float> env ((size_t) std::max (0, frames));
    const auto* d = mono.getReadPointer (0);
    float globalPeak = 0.0f;
    for (int f = 0; f < frames; ++f)
    {
        float m = 0.0f;
        for (int i = 0; i < hop; ++i) m = std::max (m, std::abs (d[f * hop + i]));
        env[(size_t) f] = juce::Decibels::gainToDecibels (m, -120.0f);
        globalPeak = std::max (globalPeak, m);
    }

    const float floorDb = juce::Decibels::gainToDecibels (globalPeak, -120.0f) - 42.0f;
    const int minGap = (int) (0.07 * sr / hop);
    std::vector<int> onsets;
    int last = -minGap;

    for (int f = 1; f < frames; ++f)
    {
        float recentMin = env[(size_t) f - 1];
        for (int k = 2; k <= 8 && f - k >= 0; ++k)
            recentMin = std::min (recentMin, env[(size_t) (f - k)]);

        const bool rise = env[(size_t) f] - recentMin > 12.0f || (f <= 8 && env[(size_t) f] > floorDb && onsets.empty() && f - last >= minGap);
        if (rise && env[(size_t) f] > floorDb && f - last >= minGap)
        {
            // Refine to the first sample of the attack, with 1 ms pre-roll.
            const int from = std::max (0, (f - 1) * hop);
            const int to = std::min (n, (f + 1) * hop);
            float localPeak = 0.0f;
            for (int i = from; i < to; ++i) localPeak = std::max (localPeak, std::abs (d[i]));
            const int start = std::max (0, firstAbove (mono, from, to, localPeak * 0.05f) - (int) (0.001 * sr));
            onsets.push_back (start);
            last = f;
        }
    }
    return onsets;
}

int SampleImporter::trimEnd (const juce::AudioBuffer<float>& mono, int start, int end, float thresholdDb)
{
    const auto* d = mono.getReadPointer (0);
    float peak = 0.0f;
    for (int i = start; i < end; ++i) peak = std::max (peak, std::abs (d[i]));
    const float thr = peak * juce::Decibels::decibelsToGain (thresholdDb);
    int lastLoud = start;
    for (int i = end - 1; i >= start; --i)
        if (std::abs (d[i]) > thr) { lastLoud = i; break; }
    return std::min (end, lastLoud + 480);
}

double SampleImporter::estimatePitchHz (const juce::AudioBuffer<float>& mono, double sr)
{
    return SampleAnalysis::estimatePitchHz (mono, sr);
}

juce::String SampleImporter::noteName (int midi)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
}

//==============================================================================
SampleImporter::Report SampleImporter::run (const juce::Array<juce::File>& inputs, const Options& o)
{
    Report report;
    const auto& info = instrumentInfo (o.instrument);
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    const auto files = expand (inputs);

    if (files.isEmpty())
    {
        report.messages.add ("No audio files found.");
        return report;
    }

    const auto instDir = o.destinationRoot.getChildFile (info.key);

    if (info.melodic)
    {
        const auto notesDir = instDir.getChildFile ("notes");
        notesDir.createDirectory();
        for (auto& f : files)
        {
            juce::AudioBuffer<float> mono;
            double sr = 0.0;
            if (! readMono (fm, f, mono, sr)) { report.messages.add ("Cannot read " + f.getFileName()); continue; }
            ++report.filesRead;

            int midi = SampleLibrary::parseRootNote (f.getFileNameWithoutExtension());
            if (midi < 0)
            {
                const double hz = estimatePitchHz (mono, sr);
                if (hz <= 0.0) { report.messages.add ("No stable pitch in " + f.getFileName() + ", skipped"); continue; }
                midi = (int) std::lround (69.0 + 12.0 * std::log2 (hz / 440.0));
                report.messages.add (f.getFileName() + ": detected " + juce::String (hz, 1) + " Hz = " + noteName (midi));
            }

            float peak = mono.getMagnitude (0, mono.getNumSamples());
            const int s0 = std::max (0, firstAbove (mono, 0, mono.getNumSamples(), peak * 0.003f) - (int) (0.001 * sr));
            const int s1 = trimEnd (mono, s0, mono.getNumSamples());
            auto note = cut (mono, s0, s1, sr);
            peak = note.getMagnitude (0, note.getNumSamples());
            if (peak > 0.0f) note.applyGain (juce::Decibels::decibelsToGain (-3.0f) / peak);

            const auto out = WavExporter::uniqueFile (notesDir, juce::String (info.key) + "_" + noteName (midi) + ".wav");
            if (WavExporter::write (note, sr, WavBitDepth::Pcm24, out).wasOk()) ++report.samplesWritten;
        }
        return report;
    }

    // Percussion ------------------------------------------------------------------
    int artIndex = -1;
    for (int a = 0; a < (int) info.articulations.size(); ++a)
        if (o.articulation.equalsIgnoreCase (info.articulations[(size_t) a]))
            artIndex = a;
    if (artIndex < 0)
    {
        juce::StringArray valid;
        for (auto* a : info.articulations) valid.add (a);
        report.messages.add ("Unknown articulation \"" + o.articulation + "\" for " + info.name + ". Use one of: " + valid.joinIntoString (", "));
        return report;
    }

    std::vector<Hit> hits;
    for (auto& f : files)
    {
        juce::AudioBuffer<float> mono;
        double sr = 0.0;
        if (! readMono (fm, f, mono, sr)) { report.messages.add ("Cannot read " + f.getFileName()); continue; }
        ++report.filesRead;

        std::vector<int> starts;
        if (o.slice) starts = detectOnsets (mono, sr);
        if (starts.empty()) starts = { std::max (0, firstAbove (mono, 0, mono.getNumSamples(), mono.getMagnitude (0, mono.getNumSamples()) * 0.01f) - (int) (0.001 * sr)) };

        for (size_t k = 0; k < starts.size(); ++k)
        {
            const int s0 = starts[k];
            int s1 = k + 1 < starts.size() ? starts[k + 1] : mono.getNumSamples();
            s1 = std::min (s1, s0 + (int) (6.0 * sr));
            s1 = trimEnd (mono, s0, s1);
            if (s1 - s0 < (int) (0.02 * sr)) continue;

            Hit h;
            h.audio = cut (mono, s0, s1, sr);
            h.sampleRate = sr;
            h.peak = h.audio.getMagnitude (0, h.audio.getNumSamples());
            h.source = f.getFileName();
            hits.push_back (std::move (h));
        }
    }

    if (hits.empty())
    {
        report.messages.add ("No hits found.");
        return report;
    }

    // One gain for all hits keeps their dynamics; loudest hit at -1 dBFS.
    float maxPeak = 0.0f;
    for (auto& h : hits) maxPeak = std::max (maxPeak, h.peak);
    const float gain = maxPeak > 0.0f ? juce::Decibels::decibelsToGain (-1.0f) / maxPeak : 1.0f;

    std::sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b) { return a.peak < b.peak; });
    const int layers = juce::jlimit (1, 4, std::min (o.velocityLayers, (int) hits.size()));

    const auto artDir = instDir.getChildFile (info.articulations[(size_t) artIndex]);
    artDir.createDirectory();
    const auto stem = juce::String (info.articulations[(size_t) artIndex]).toLowerCase();

    for (size_t i = 0; i < hits.size(); ++i)
    {
        auto& h = hits[i];
        h.audio.applyGain (gain);
        const int layer = layers > 1 ? 1 + (int) (i * (size_t) layers / hits.size()) : 0;
        const auto name = stem + (layer > 0 ? "_v" + juce::String (layer) : juce::String()) + "_rr" + juce::String ((int) i + 1) + ".wav";
        const auto out = WavExporter::uniqueFile (artDir, name);
        if (WavExporter::write (h.audio, h.sampleRate, WavBitDepth::Pcm24, out).wasOk())
            ++report.samplesWritten;
    }

    report.messages.add (juce::String ((int) hits.size()) + " hits -> " + artDir.getFullPathName()
                         + (layers > 1 ? " (" + juce::String (layers) + " velocity layers)" : juce::String()));
    return report;
}

} // namespace jgl

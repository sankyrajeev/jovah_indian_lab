#include "WavExporter.h"
#include "../Core/Rng.h"

namespace jgl
{

juce::Result WavExporter::write (const juce::AudioBuffer<float>& audio, double sampleRate, WavBitDepth depth,
                                 const juce::File& file, double bpm, int beats, int rootMidi)
{
    if (audio.getNumSamples() <= 0 || audio.getNumChannels() <= 0)
        return juce::Result::fail ("Nothing to export");

    if (! file.getParentDirectory().createDirectory())
        return juce::Result::fail ("Cannot create folder " + file.getParentDirectory().getFullPathName());

    const auto tmp = file.getSiblingFile (file.getFileNameWithoutExtension() + ".partial.wav");
    tmp.deleteFile();

    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (tmp);
    if (! static_cast<juce::FileOutputStream*> (stream.get())->openedOk())
        return juce::Result::fail ("Cannot write to " + tmp.getFullPathName());

    const int bits = depth == WavBitDepth::Pcm16 ? 16 : (depth == WavBitDepth::Pcm24 ? 24 : 32);

    auto options = juce::AudioFormatWriterOptions()
                       .withSampleRate (sampleRate)
                       .withNumChannels (2)
                       .withBitsPerSample (bits)
                       .withSampleFormat (depth == WavBitDepth::Float32
                                              ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint
                                              : juce::AudioFormatWriterOptions::SampleFormat::integral);

    if (bpm > 0.0 && beats > 0)
    {
        options = options.withMetadata (juce::WavAudioFormat::acidStretch, "1")
                         .withMetadata (juce::WavAudioFormat::acidOneShot, "0")
                         .withMetadata (juce::WavAudioFormat::acidDiskBased, "0")
                         .withMetadata (juce::WavAudioFormat::acidizerFlag, "1")
                         .withMetadata (juce::WavAudioFormat::acidBeats, juce::String (beats))
                         .withMetadata (juce::WavAudioFormat::acidNumerator, "4")
                         .withMetadata (juce::WavAudioFormat::acidDenominator, "4")
                         .withMetadata (juce::WavAudioFormat::acidTempo, juce::String (bpm, 3))
                         .withMetadata (juce::WavAudioFormat::acidRootSet, rootMidi >= 0 ? "1" : "0")
                         .withMetadata (juce::WavAudioFormat::acidRootNote, juce::String (juce::jmax (0, rootMidi)));
    }

    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (stream, options);
    if (writer == nullptr)
        return juce::Result::fail ("The WAV writer rejected these settings (" + juce::String (bits) + "-bit, "
                                   + juce::String (sampleRate) + " Hz)");

    // Always write stereo.
    juce::AudioBuffer<float> out (2, audio.getNumSamples());
    out.copyFrom (0, 0, audio, 0, 0, audio.getNumSamples());
    out.copyFrom (1, 0, audio, juce::jmin (1, audio.getNumChannels() - 1), 0, audio.getNumSamples());

    if (depth == WavBitDepth::Pcm16)
    {
        // TPDF dither at 1 LSB, deterministic.
        Rng r (0xD17E4ull);
        const float lsb = 1.0f / 32768.0f;
        for (int c = 0; c < 2; ++c)
        {
            auto* d = out.getWritePointer (c);
            for (int i = 0; i < out.getNumSamples(); ++i)
                d[i] = juce::jlimit (-1.0f, 1.0f - lsb, d[i] + (float) (r.uniform() - r.uniform()) * lsb);
        }
    }
    else if (depth == WavBitDepth::Pcm24)
    {
        const float maxVal = 1.0f - 1.0f / 8388608.0f;
        for (int c = 0; c < 2; ++c)
            juce::FloatVectorOperations::clip (out.getWritePointer (c), out.getReadPointer (c), -1.0f, maxVal, out.getNumSamples());
    }

    if (! writer->writeFromAudioSampleBuffer (out, 0, out.getNumSamples()))
        return juce::Result::fail ("Writing audio data failed");

    if (! writer->flush())
        return juce::Result::fail ("Flushing the WAV file failed");

    writer.reset();   // closes the stream

    file.deleteFile();
    if (! tmp.moveFileTo (file))
        return juce::Result::fail ("Cannot move the exported file into place: " + file.getFullPathName());

    return juce::Result::ok();
}

juce::String WavExporter::makeFileName (const LoopFileInfo& info)
{
    juce::String bpm = std::abs (info.bpm - std::round (info.bpm)) < 0.005
                           ? juce::String ((int) std::lround (info.bpm))
                           : juce::String (info.bpm, 1);

    juce::String name = "IndianGroove_" + info.styleTag + "_" + bpm + "BPM_";
    if (info.keyLabel.isNotEmpty())
        name << info.keyLabel << "_";
    name << info.bars << (info.bars == 1 ? "Bar" : "Bars") << ".wav";
    return juce::File::createLegalFileName (name);
}

juce::File WavExporter::uniqueFile (const juce::File& folder, const juce::String& fileName)
{
    auto f = folder.getChildFile (fileName);
    if (! f.exists())
        return f;

    const auto stem = f.getFileNameWithoutExtension();
    const auto ext = f.getFileExtension();
    for (int i = 2; i < 10000; ++i)
    {
        auto candidate = folder.getChildFile (stem + "_" + juce::String (i) + ext);
        if (! candidate.exists())
            return candidate;
    }
    return folder.getNonexistentChildFile (stem, ext, false);
}

} // namespace jgl

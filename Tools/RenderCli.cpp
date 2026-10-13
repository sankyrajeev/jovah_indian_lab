// jgl_render: generate and render loops offline, without a host.
//
//   jgl_render --style 0 --bpm 124 --key D --scale 1 --bars 4 --inst dhol,tabla,tumbi --seed 42 --out loop.wav
//   jgl_render --list                       (styles, scales, instruments, frameworks)
//   jgl_render ... --describe               (print the composed parts)
//   jgl_render ... --stems out_dir          (also write one WAV per part)

#include <juce_audio_basics/juce_audio_basics.h>

#include "../Source/Core/Instruments.h"
#include "../Source/Core/Scales.h"
#include "../Source/Core/Styles.h"
#include "../Source/DSP/LoopRenderer.h"
#include "../Source/DSP/SampleLibrary.h"
#include "../Source/Engine/GenerationService.h"
#include "../Source/Generation/Arranger.h"
#include "../Source/IO/WavExporter.h"

using namespace jgl;

namespace
{
    juce::String arg (const juce::StringArray& a, const juce::String& name, const juce::String& def = {})
    {
        const int i = a.indexOf (name);
        return (i >= 0 && i + 1 < a.size()) ? a[i + 1] : def;
    }

    int parseKey (const juce::String& k)
    {
        const int idx = keyNames().indexOf (k.toUpperCase().replace ("B", "b").replace ("b", "B"));
        if (idx >= 0) return idx;
        return juce::jlimit (0, 11, k.getIntValue());
    }

    void printList()
    {
        std::cout << "Styles:\n";
        for (int i = 0; i < numStyles; ++i)
            std::cout << "  " << i << "  " << styleProfile (i).name << "\n";
        std::cout << "Frameworks:\n";
        for (int i = 0; i < numFrameworks; ++i)
            std::cout << "  " << i << "  " << framework ((FrameworkId) i).name << "\n";
        std::cout << "Scales:\n";
        for (int i = 0; i < numScales(); ++i)
            std::cout << "  " << i << "  " << scaleProfile (i).name << "\n";
        std::cout << "Instruments:\n";
        for (auto id : allInstruments())
            std::cout << "  " << instrumentInfo (id).key << "\n";
    }
}

int main (int argc, char* argv[])
{
    juce::StringArray a;
    for (int i = 1; i < argc; ++i)
        a.add (argv[i]);

    if (a.contains ("--list") || a.contains ("-h") || a.contains ("--help"))
    {
        printList();
        return 0;
    }

    GenerationService::Request req;
    auto& cs = req.compose;
    auto& rs = req.render;

    cs.styleIndex = arg (a, "--style", "0").getIntValue();
    const auto& style = styleProfile (cs.styleIndex);
    cs.frameworkIndex = arg (a, "--framework", "0").getIntValue();
    cs.scaleIndex = arg (a, "--scale", juce::String (style.suggestedScale)).getIntValue();
    cs.bars = arg (a, "--bars", "4").getIntValue();
    cs.complexity = arg (a, "--complexity", juce::String (style.complexity)).getFloatValue();
    cs.density = arg (a, "--density", juce::String (style.density)).getFloatValue();
    cs.energy = arg (a, "--energy", juce::String (style.energy)).getFloatValue();
    cs.variation = arg (a, "--variation", juce::String (style.variation)).getFloatValue();

    const auto inst = arg (a, "--inst");
    if (inst.isEmpty())
    {
        for (auto id : style.suggestedEnsemble)
            cs.enabled[(size_t) toIndex (id)] = true;
    }
    else
    {
        juce::StringArray names;
        names.addTokens (inst, ",", "");
        for (auto& n : names)
        {
            const int idx = findInstrumentByKey (n.trim());
            if (idx < 0) { std::cerr << "Unknown instrument: " << n << "\n"; return 1; }
            cs.enabled[(size_t) idx] = true;
        }
    }

    rs.bpm = arg (a, "--bpm", juce::String (style.defaultBpm)).getDoubleValue();
    rs.keyRoot = parseKey (arg (a, "--key", "D"));
    rs.sampleRate = arg (a, "--sr", "48000").getDoubleValue();
    rs.swing = arg (a, "--swing", juce::String (style.swing)).getFloatValue();
    rs.groove = arg (a, "--groove", juce::String (style.groove)).getFloatValue();
    rs.humanize = arg (a, "--humanize", juce::String (style.humanize)).getFloatValue();
    rs.ghost = arg (a, "--ghost", juce::String (style.ghost)).getFloatValue();
    rs.accent = arg (a, "--accent", juce::String (style.accent)).getFloatValue();
    rs.reverb = arg (a, "--reverb", juce::String (style.reverb)).getFloatValue();
    rs.delay = arg (a, "--delay", juce::String (style.delay)).getFloatValue();
    rs.saturation = arg (a, "--saturation", juce::String (style.saturation)).getFloatValue();
    rs.filter = arg (a, "--filter", "0").getFloatValue();

    cs.voiceBend = arg (a, "--bend", "0.3").getFloatValue();
    cs.voiceShake = arg (a, "--shake", "0.1").getFloatValue();
    cs.voiceFalls = arg (a, "--falls", "0.2").getFloatValue();
    cs.voiceStutter = arg (a, "--stutter", "0.1").getFloatValue();
    cs.voiceOctave = arg (a, "--octave", "0.15").getFloatValue();
    rs.voiceVibrato = arg (a, "--vibrato", "0.0").getFloatValue();

    const auto voicePath = arg (a, "--voice");
    if (voicePath.isNotEmpty())
    {
        juce::String err;
        auto v = SampleLibrary::loadVoice (juce::File::getCurrentWorkingDirectory().getChildFile (voicePath), err);
        if (v == nullptr) { std::cerr << err << "\n"; return 1; }
        std::cout << "Voice root " << juce::String (v->rootMidi, 2) << " (" << juce::String (440.0 * std::pow (2.0, (v->rootMidi - 69.0) / 12.0), 1)
                  << " Hz), loop " << v->loopStart << ".." << v->loopEnd << " @ " << v->sampleRate << "\n";
        req.userVoice = v;
        cs.enabled[(size_t) toIndex (InstrumentId::CustomVoice)] = true;
    }

    const auto seed = (uint64_t) arg (a, "--seed", "1").getLargeIntValue();
    req.seeds = SeedSet::fromMaster (seed);

    SampleLibrary lib;
    const auto samplesDir = arg (a, "--samples");
    if (samplesDir.isNotEmpty())
        std::cout << "Loaded " << lib.load (juce::File (samplesDir)) << " samples\n";

    auto result = GenerationService::renderNow (req, &lib);
    if (result == nullptr)
    {
        std::cerr << "Render failed\n";
        return 1;
    }

    if (a.contains ("--describe"))
    {
        for (auto& p : result->arrangement.parts)
        {
            std::cout << instrumentInfo (p.instrument).name << " [" << p.summary << "] " << p.events.size() << " events\n";
            if (a.contains ("--events"))
                for (auto& e : p.events)
                    std::cout << "   beat " << juce::String (e.beat, 3) << "  art " << e.articulation << "  pitch " << e.pitch
                              << "  vel " << juce::String (e.velocity, 2) << "  dyn " << (int) e.dyn
                              << "  len " << juce::String (e.length, 3) << "\n";
        }
    }

    if (a.contains ("--grid"))
    {
        // Drum-machine style view on a 16th grid: letter = stroke, UPPER = accent, ',' = ghost.
        for (auto& p : result->arrangement.parts)
        {
            const auto& info = instrumentInfo (p.instrument);
            std::cout << juce::String (info.name).paddedRight (' ', 9) << " ";
            for (int bar = 0; bar < result->arrangement.bars; ++bar)
            {
                juce::String line = juce::String::repeatedString (".", 16);
                for (auto& e : p.events)
                {
                    if (e.beat < bar * 4.0 || e.beat >= bar * 4.0 + 4.0) continue;
                    const int step = (int) std::floor ((e.beat - bar * 4.0) * 4.0 + 1.0e-6);
                    juce::juce_wchar c = 'x';
                    if (! info.patternChars.empty())
                        c = (juce::juce_wchar) info.patternChars[(size_t) juce::jlimit (0, (int) info.patternChars.size() - 1, e.articulation)];
                    else
                        c = (juce::juce_wchar) ("0123456789ab" [((e.pitch % 12) + 12) % 12]);
                    if (e.dyn == Dynamic::Accent) c = juce::CharacterFunctions::toUpperCase (c);
                    if (e.dyn == Dynamic::Ghost) c = ',';
                    line = line.substring (0, step) + juce::String::charToString (c) + line.substring (step + 1);
                }
                std::cout << line.substring (0, 4) << " " << line.substring (4, 8) << " " << line.substring (8, 12) << " " << line.substring (12) << " | ";
            }
            std::cout << "\n";
        }
    }

    const auto out = juce::File::getCurrentWorkingDirectory().getChildFile (arg (a, "--out", "loop.wav"));
    const auto depth = (WavBitDepth) arg (a, "--depth", "0").getIntValue();
    const bool melodic = result->hasMelodic();
    const auto r = WavExporter::write (result->audio, rs.sampleRate, depth, out, rs.bpm, cs.bars * 4,
                                       melodic ? (int) LoopRenderer::tonicMidi (InstrumentId::Sitar, rs.keyRoot) : -1);
    if (r.failed())
    {
        std::cerr << r.getErrorMessage() << "\n";
        return 1;
    }

    std::cout << "Wrote " << out.getFullPathName() << "  (" << result->audio.getNumSamples() << " samples, "
              << juce::String (result->renderMilliseconds, 1) << " ms render, "
              << result->arrangement.totalEvents() << " events)\n";

    if (a.contains ("--levels"))
    {
        // Un-normalised, dry, per-part levels including the instrument mix gain.
        for (auto& p : result->arrangement.parts)
        {
            Arrangement solo = result->arrangement;
            solo.parts = { p };
            juce::AudioBuffer<float> buf;
            LoopRenderer::Options opt;
            opt.normalise = false;
            opt.userVoice = req.userVoice.get();
            RenderSettings dry = rs;
            dry.reverb = 0; dry.delay = 0; dry.saturation = 0; dry.filter = 0;
            LoopRenderer::render (solo, dry, buf, opt);
            const float rms = std::sqrt ((buf.getRMSLevel (0, 0, buf.getNumSamples()) * buf.getRMSLevel (0, 0, buf.getNumSamples())
                                        + buf.getRMSLevel (1, 0, buf.getNumSamples()) * buf.getRMSLevel (1, 0, buf.getNumSamples())) * 0.5f);
            std::cout << "LEVEL " << instrumentInfo (p.instrument).key << " rms " << juce::Decibels::gainToDecibels (rms)
                      << " dB  peak " << juce::Decibels::gainToDecibels (buf.getMagnitude (0, buf.getNumSamples())) << " dB\n";
        }
    }

    const auto stems = arg (a, "--stems");
    if (stems.isNotEmpty())
    {
        juce::File dir = juce::File::getCurrentWorkingDirectory().getChildFile (stems);
        dir.createDirectory();
        for (auto& p : result->arrangement.parts)
        {
            Arrangement solo = result->arrangement;
            solo.parts = { p };
            juce::AudioBuffer<float> buf;
            LoopRenderer::Options opt;
            opt.samples = &lib;
            opt.normalise = false;
            opt.userVoice = req.userVoice.get();
            RenderSettings dry = rs;
            dry.reverb = 0; dry.delay = 0; dry.saturation = 0; dry.filter = 0;
            LoopRenderer::render (solo, dry, buf, opt);
            WavExporter::write (buf, rs.sampleRate, WavBitDepth::Float32,
                                dir.getChildFile (juce::String (instrumentInfo (p.instrument).key) + ".wav"));
        }
    }

    return 0;
}

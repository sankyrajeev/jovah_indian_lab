// jgl_import: bring licensed recordings into Jovah Groove Lab's sample folder.
//
//   Percussion (a folder of single hits, or long takes that get sliced into hits):
//     jgl_import --instrument tabla --articulation Na --layers 3  ~/Packs/Tabla/Na/
//   Melodic (one sustained note per file; root from the name or detected):
//     jgl_import --instrument nadaswaram  ~/Packs/Nadaswaram/Sustains/
//
//   Options: --out <folder> (default ~/Music/Jovah Groove Lab/Samples), --no-slice, --list

#include <juce_audio_formats/juce_audio_formats.h>

#include "../Source/IO/AppPaths.h"
#include "../Source/IO/SampleImporter.h"

using namespace jgl;

int main (int argc, char* argv[])
{
    juce::StringArray a;
    for (int i = 1; i < argc; ++i)
        a.add (argv[i]);

    auto value = [&a] (const juce::String& name) -> juce::String
    {
        const int i = a.indexOf (name);
        return (i >= 0 && i + 1 < a.size()) ? a[i + 1] : juce::String();
    };

    if (a.isEmpty() || a.contains ("--help") || a.contains ("--list"))
    {
        std::cout << "Usage: jgl_import --instrument <key> [--articulation <name>] [--layers 1-4] [--no-slice] [--out dir] files/folders...\n\n";
        for (auto id : allInstruments())
        {
            const auto& info = instrumentInfo (id);
            std::cout << "  " << juce::String (info.key).paddedRight (' ', 12);
            if (info.melodic) std::cout << "(melodic: one note per file)\n";
            else
            {
                juce::StringArray arts;
                for (auto* s : info.articulations) arts.add (s);
                std::cout << "articulations: " << arts.joinIntoString (", ") << "\n";
            }
        }
        return 0;
    }

    SampleImporter::Options o;
    const int inst = findInstrumentByKey (value ("--instrument"));
    if (inst < 0) { std::cerr << "Unknown or missing --instrument (see --list)\n"; return 1; }
    o.instrument = instrumentFromIndex (inst);
    o.articulation = value ("--articulation");
    o.velocityLayers = juce::jmax (1, value ("--layers").getIntValue());
    o.slice = ! a.contains ("--no-slice");
    o.destinationRoot = value ("--out").isNotEmpty() ? juce::File::getCurrentWorkingDirectory().getChildFile (value ("--out"))
                                                    : AppPaths::samplesFolder();

    if (! instrumentInfo (o.instrument).melodic && o.articulation.isEmpty())
    {
        std::cerr << "--articulation is required for percussion (see --list)\n";
        return 1;
    }

    juce::Array<juce::File> inputs;
    for (int i = 0; i < a.size(); ++i)
    {
        if (a[i].startsWith ("--"))
        {
            if (a[i] != "--no-slice") ++i;   // skip the option's value
            continue;
        }
        inputs.add (juce::File::getCurrentWorkingDirectory().getChildFile (a[i]));
    }

    const auto report = SampleImporter::run (inputs, o);
    for (auto& m : report.messages)
        std::cout << m << "\n";
    std::cout << report.filesRead << " files read, " << report.samplesWritten << " samples written to "
              << o.destinationRoot.getFullPathName() << "\n";
    return report.samplesWritten > 0 ? 0 : 1;
}

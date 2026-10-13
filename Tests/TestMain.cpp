#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "IO/AppPaths.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // Keep exports, presets and logs out of the user's real folders.
    auto tempRoot = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("JGLTests_" + juce::String (juce::Time::currentTimeMillis()));
    tempRoot.createDirectory();
    jgl::AppPaths::overrideRoot (tempRoot);

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);

    if (argc > 1)
        runner.runTestsInCategory (argv[1]);
    else
        runner.runTestsInCategory ("JGL");

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        const auto* r = runner.getResult (i);
        failures += r->failures;
        passes += r->passes;
        if (r->failures > 0)
            for (auto& m : r->messages)
                std::cout << "  FAIL [" << r->unitTestName << " / " << r->subcategoryName << "] " << m << "\n";
    }

    std::cout << "\n" << runner.getNumResults() << " test groups, " << passes << " checks passed, "
              << failures << " failed\n";

    tempRoot.deleteRecursively();
    return failures > 0 ? 1 : 0;
}

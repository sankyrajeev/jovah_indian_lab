#include "Styles.h"

namespace jgl
{

//==============================================================================
static std::vector<Framework> buildFrameworks()
{
    std::vector<Framework> f;

    f.push_back ({ "Style Default", "Use the personality's own rhythmic framework", 0, false,
                   { 1, .1f, .4f, .15f, .9f, .1f, .45f, .2f, .95f, .1f, .4f, .15f, .9f, .15f, .5f, .25f },
                   { 1, .3f, .5f, .8f, .3f, .5f, .9f, .3f, .5f, .8f, .3f, .5f },
                   { 0, 4, 8, 12 } });

    f.push_back ({ "Keherwa (8)", "8-matra cycle: Dha Ge Na Ti | Na Ka Dhi Na", 8, false,
                   { 1, .1f, .5f, .2f, .6f, .1f, .5f, .25f, .7f, .1f, .45f, .2f, .8f, .15f, .5f, .3f },
                   { 1, .3f, .5f, .8f, .3f, .5f, .9f, .3f, .5f, .8f, .3f, .5f },
                   { 0, 8 } });

    f.push_back ({ "Dadra (6)", "6-matra lilt: Dha Dhi Na | Dha Ti Na, on a triplet grid", 6, true,
                   { 1, .1f, .3f, .7f, .2f, .3f, .7f, .2f, .9f, .1f, .3f, .7f, .2f, .3f, .7f, .2f },
                   { 1, .4f, .5f, .8f, .3f, .5f, .9f, .4f, .5f, .8f, .3f, .5f },
                   { 0, 6 } });

    f.push_back ({ "Teentaal (16)", "16 matras in four vibhags; khali on 9", 16, false,
                   { 1, .5f, .5f, .6f, .9f, .5f, .5f, .6f, .4f, .4f, .4f, .5f, .9f, .5f, .5f, .7f },
                   { 1, .4f, .5f, .8f, .4f, .5f, .6f, .4f, .5f, .8f, .4f, .5f },
                   { 0, 4, 8, 12 } });

    f.push_back ({ "Rupak (7)", "7 matras grouped 3+2+2, folded onto 16 steps", 7, false,
                   { .8f, .2f, .3f, 1, .2f, .9f, .2f, .7f, .2f, .3f, .9f, .2f, .8f, .2f, .7f, .3f },
                   { 1, .3f, .5f, .8f, .3f, .7f, .9f, .3f, .5f, .8f, .3f, .7f },
                   { 0, 3, 5, 7, 10, 12, 14 } });

    f.push_back ({ "Club 4/4", "Modern four-on-the-floor with offbeat lift", 8, false,
                   { 1, .1f, .45f, .15f, .9f, .1f, .5f, .2f, .95f, .1f, .45f, .15f, .9f, .15f, .55f, .25f },
                   { 1, .3f, .5f, .8f, .3f, .5f, .9f, .3f, .5f, .8f, .3f, .5f },
                   { 0, 4, 8, 12 } });

    f.push_back ({ "Tresillo 3+3+2", "Syncopated 3+3+2 grouping shared by Indian folk & Afro grooves", 8, false,
                   { 1, .1f, .2f, .85f, .1f, .2f, .8f, .2f, .95f, .1f, .2f, .85f, .1f, .2f, .8f, .25f },
                   { 1, .3f, .5f, .8f, .3f, .5f, .9f, .3f, .5f, .8f, .3f, .5f },
                   { 0, 3, 6, 8, 11, 14 } });

    return f;
}

const Framework& framework (FrameworkId id)
{
    static const std::vector<Framework> f = buildFrameworks();
    return f[(size_t) juce::jlimit (0, (int) f.size() - 1, (int) id)];
}

juce::StringArray frameworkNames()
{
    juce::StringArray a;
    for (int i = 0; i < numFrameworks; ++i)
        a.add (framework ((FrameworkId) i).name);
    return a;
}

//==============================================================================
static std::vector<StyleProfile> buildStyles()
{
    using I = InstrumentId;
    std::vector<StyleProfile> s;

    // name, tag, desc, framework, bpm,
    // complexity, swing, groove, humanize, density, energy, variation, ghost, accent,
    // reverb, delay, saturation, scale, ensemble,
    // microTiming, velocityShape,
    // polymeter, triplet, fill, repetition, roll,
    // melody { density, legato, ornament, leap, rest, range, riff }

    s.push_back ({ StyleId::AfroIndian, "Afro Indian", "AfroIndian",
                   "Polyrhythmic hand drums, 3+3+2 syncopation and hypnotic riffs with a laid-back pocket.",
                   FrameworkId::Tresillo, 122.0,
                   .55f, .35f, .7f, .4f, .6f, .65f, .45f, .55f, .6f,
                   .25f, .2f, .15f, 1, { I::Dhol, I::Tabla, I::Tumbi, I::Ghungroo },
                   { 0.0f, .10f, .02f, .14f }, { 1.0f, .74f, .9f, .8f },
                   .45f, .35f, .5f, .65f, .2f,
                   { .45f, .2f, .3f, .3f, .3f, 12, true } });

    s.push_back ({ StyleId::TribalRitual, "Tribal Ritual", "TribalRitual",
                   "Layered ceremonial drums, cymbal pulse, rolls that build tension, drone underneath.",
                   FrameworkId::Teentaal, 124.0,
                   .6f, .2f, .5f, .35f, .7f, .8f, .5f, .4f, .8f,
                   .3f, .1f, .25f, 4, { I::Dhol, I::Tabla, I::Manjira, I::Taali, I::Tanpura },
                   { 0.0f, -.03f, .03f, .05f }, { 1.0f, .7f, .85f, .75f },
                   .3f, .3f, .8f, .55f, .45f,
                   { .3f, .7f, .4f, .2f, .5f, 7, false } });

    s.push_back ({ StyleId::IndianUnderground, "Indian Underground", "IndianUnderground",
                   "Raw, repetitive and off-kilter: Rupak-style 3+2+2 cycles running against the 4/4 grid.",
                   FrameworkId::Rupak, 128.0,
                   .5f, .1f, .4f, .25f, .55f, .7f, .3f, .45f, .7f,
                   .15f, .25f, .35f, 5, { I::Tabla, I::Dholak, I::Tumbi, I::Manjira },
                   { 0.0f, 0.0f, -.03f, .02f }, { 1.0f, .6f, .85f, .7f },
                   .6f, .1f, .35f, .8f, .25f,
                   { .4f, .1f, .15f, .5f, .4f, 7, true } });

    s.push_back ({ StyleId::PsychedelicIndia, "Psychedelic India", "PsychedelicIndia",
                   "Sitar phrases with meend, tanpura drone and evolving percussion soaked in echoes.",
                   FrameworkId::Keherwa, 120.0,
                   .6f, .25f, .55f, .45f, .55f, .55f, .65f, .5f, .55f,
                   .45f, .45f, .2f, 9, { I::Sitar, I::Tabla, I::Tanpura, I::Ghungroo },
                   { 0.0f, .06f, .03f, .08f }, { 1.0f, .75f, .88f, .8f },
                   .4f, .3f, .55f, .5f, .35f,
                   { .55f, .6f, .6f, .35f, .25f, 14, false } });

    s.push_back ({ StyleId::MelodicIndian, "Melodic Indian", "MelodicIndian",
                   "Expressive strings and winds in call-and-response over a lilting Dadra groove.",
                   FrameworkId::Dadra, 118.0,
                   .4f, .3f, .5f, .45f, .5f, .5f, .5f, .45f, .5f,
                   .4f, .35f, .1f, 11, { I::Bansuri, I::Sitar, I::Tabla, I::Tanpura },
                   { 0.0f, .05f, .02f, .07f }, { 1.0f, .75f, .88f, .8f },
                   .15f, .4f, .4f, .45f, .2f,
                   { .5f, .75f, .55f, .3f, .3f, 14, false } });

    s.push_back ({ StyleId::PeakTimePercussion, "Peak-Time Percussion", "PeakTime",
                   "Driving, tightly locked dhol and tabla with strong accents and big phrase-end fills.",
                   FrameworkId::Club, 128.0,
                   .55f, .05f, .35f, .2f, .8f, .95f, .4f, .35f, .85f,
                   .15f, .15f, .3f, 2, { I::Dhol, I::Tabla, I::Dholak, I::Manjira, I::Taali },
                   { 0.0f, -.02f, -.03f, 0.0f }, { 1.0f, .7f, .9f, .75f },
                   .25f, .05f, .7f, .6f, .4f,
                   { .35f, .1f, .1f, .3f, .4f, 7, true } });

    s.push_back ({ StyleId::MinimalHypnosis, "Minimal Hypnosis", "MinimalHypnosis",
                   "Sparse, precise and deeply repetitive. Every hit placed with intent.",
                   FrameworkId::Keherwa, 122.0,
                   .3f, .15f, .4f, .25f, .3f, .4f, .15f, .4f, .5f,
                   .3f, .35f, .1f, 14, { I::Tabla, I::Ghungroo, I::Tumbi },
                   { 0.0f, .03f, 0.0f, .05f }, { 1.0f, .65f, .85f, .7f },
                   .5f, .1f, .15f, .95f, .05f,
                   { .3f, .3f, .2f, .2f, .5f, 7, true } });

    s.push_back ({ StyleId::FolkElectronica, "Folk Electronica", "FolkElectronica",
                   "Bhangra-flavoured dhol chaal, bouncy dholak and lively folk riffs, modern pocket.",
                   FrameworkId::Keherwa, 120.0,
                   .45f, .45f, .65f, .5f, .65f, .75f, .5f, .45f, .65f,
                   .2f, .2f, .15f, 15, { I::Dhol, I::Dholak, I::Tumbi, I::Taali },
                   { 0.0f, .08f, .05f, .12f }, { 1.0f, .7f, .9f, .8f },
                   .15f, .2f, .6f, .6f, .3f,
                   { .6f, .25f, .35f, .35f, .2f, 12, false } });

    s.push_back ({ StyleId::Custom, "Custom", "Custom",
                   "Neutral engine: the framework and every knob are fully in your hands.",
                   FrameworkId::Keherwa, 124.0,
                   .5f, .3f, .5f, .4f, .5f, .6f, .5f, .5f, .6f,
                   .25f, .2f, .1f, 1, { I::Tabla, I::Dhol, I::Sitar },
                   { 0.0f, .04f, .02f, .06f }, { 1.0f, .75f, .88f, .8f },
                   .3f, .2f, .5f, .6f, .25f,
                   { .5f, .4f, .4f, .35f, .3f, 12, false } });

    return s;
}

static const std::vector<StyleProfile>& styles()
{
    static const std::vector<StyleProfile> s = buildStyles();
    return s;
}

const StyleProfile& styleProfile (int index)
{
    auto& s = styles();
    return s[(size_t) juce::jlimit (0, (int) s.size() - 1, index)];
}

const StyleProfile& styleProfile (StyleId id) { return styleProfile ((int) id); }

juce::StringArray styleNames()
{
    juce::StringArray a;
    for (auto& s : styles())
        a.add (s.name);
    return a;
}

FrameworkId resolveFramework (int styleIndex, int frameworkIndex)
{
    if (frameworkIndex <= 0 || frameworkIndex >= numFrameworks)
        return styleProfile (styleIndex).defaultFramework;
    return (FrameworkId) frameworkIndex;
}

} // namespace jgl

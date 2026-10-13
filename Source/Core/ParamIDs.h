#pragma once

#include "Instruments.h"

namespace jgl::ParamIDs
{
    // Global
    inline constexpr const char* bpm        = "bpm";
    inline constexpr const char* key        = "key";
    inline constexpr const char* scale      = "scale";
    inline constexpr const char* bars       = "bars";
    inline constexpr const char* level      = "level";

    // Groove / generation
    inline constexpr const char* style      = "style";
    inline constexpr const char* framework  = "framework";
    inline constexpr const char* complexity = "complexity";
    inline constexpr const char* swing      = "swing";
    inline constexpr const char* groove     = "groove";
    inline constexpr const char* humanize   = "humanize";
    inline constexpr const char* density    = "density";
    inline constexpr const char* energy     = "energy";
    inline constexpr const char* variation  = "variation";
    inline constexpr const char* ghost      = "ghost";
    inline constexpr const char* accent     = "accent";

    // Effects
    inline constexpr const char* saturation = "saturation";
    inline constexpr const char* reverb     = "reverb";
    inline constexpr const char* delay      = "delay";
    inline constexpr const char* delayTime  = "delayTime";
    inline constexpr const char* filter     = "filter";

    // Sample-driven voice expression
    inline constexpr const char* voiceBend    = "voiceBend";
    inline constexpr const char* voiceShake   = "voiceShake";
    inline constexpr const char* voiceFalls   = "voiceFalls";
    inline constexpr const char* voiceStutter = "voiceStutter";
    inline constexpr const char* voiceOctave  = "voiceOctave";
    inline constexpr const char* voiceVibrato = "voiceVibrato";

    /** "inst_tabla", "inst_sitar" ... */
    inline juce::String instrument (InstrumentId id) { return juce::String ("inst_") + instrumentInfo (id).key; }

    /** Loop lengths offered by the "bars" choice parameter. */
    inline int barsForIndex (int index) { static const int b[] = { 1, 2, 4, 8 }; return b[juce::jlimit (0, 3, index)]; }
    inline int indexForBars (int numBars) { return numBars <= 1 ? 0 : numBars <= 2 ? 1 : numBars <= 4 ? 2 : 3; }
}

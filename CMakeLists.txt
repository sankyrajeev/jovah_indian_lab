cmake_minimum_required(VERSION 3.22)

project(JovahGrooveLab VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

option(JGL_BUILD_TESTS "Build the unit test runner" ON)
option(JGL_BUILD_TOOLS "Build the command-line loop renderer" ON)
option(JGL_COPY_PLUGIN_AFTER_BUILD "Install plugins to the user plugin folders after building" OFF)
set(JGL_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout. If empty, JUCE is fetched from GitHub.")

# ---------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------
if(JGL_JUCE_DIR)
    add_subdirectory(${JGL_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
# Engine code: music theory, generation, synthesis, rendering, export.
# No dependency on the plugin wrapper or GUI, so it is shared by the plugin,
# the unit tests and the command-line renderer.
set(JGL_ENGINE_SOURCES
    Source/Core/Instruments.cpp
    Source/Core/Scales.cpp
    Source/Core/Styles.cpp
    Source/Generation/PatternLibrary.cpp
    Source/Generation/RhythmGenerator.cpp
    Source/Generation/MelodyGenerator.cpp
    Source/Generation/Arranger.cpp
    Source/Generation/GrooveEngine.cpp
    Source/DSP/Voices.cpp
    Source/DSP/SampleLibrary.cpp
    Source/DSP/SampleAnalysis.cpp
    Source/DSP/Effects.cpp
    Source/DSP/LoopRenderer.cpp
    Source/Engine/LoopPlayer.cpp
    Source/Engine/GenerationService.cpp
    Source/IO/AppPaths.cpp
    Source/IO/WavExporter.cpp
    Source/IO/PresetManager.cpp
    Source/IO/SampleImporter.cpp)

set(JGL_PLUGIN_SOURCES
    Source/Plugin/Parameters.cpp
    Source/Plugin/PluginProcessor.cpp
    Source/Plugin/PluginEditor.cpp
    Source/UI/Theme.cpp
    Source/UI/InstrumentTile.cpp
    Source/UI/WaveformView.cpp
    Source/UI/Decor.cpp
    Source/UI/Widgets.cpp
    Source/UI/VoicePanel.cpp
    Source/IO/UserSettings.cpp)

set(JGL_COMMON_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JGL_VERSION_STRING="${PROJECT_VERSION}")

# JUCE's recommended warnings plus our own policy: switch statements over instrument/
# articulation enums intentionally use `default`, and exact float comparison is only
# used for change detection of settings snapshots.
set(JGL_WARNING_OVERRIDES
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wno-switch-enum -Wno-float-equal>)

set(JGL_ALL_OWN_SOURCES ${JGL_ENGINE_SOURCES} ${JGL_PLUGIN_SOURCES})
file(GLOB JGL_TEST_AND_TOOL_SOURCES CONFIGURE_DEPENDS Tests/*.cpp Tools/*.cpp)
# Source-level options come after target/interface options, so these win over JUCE's flags.
set_source_files_properties(${JGL_ALL_OWN_SOURCES} ${JGL_TEST_AND_TOOL_SOURCES}
    PROPERTIES COMPILE_OPTIONS "${JGL_WARNING_OVERRIDES}")

set(JGL_JUCE_MODULES
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp)

# Bundled typefaces (SIL Open Font License, see Resources/Fonts/OFL-*.txt)
juce_add_binary_data(JGLBinaryData SOURCES
    Resources/Fonts/CormorantGaramond-Medium.ttf
    Resources/Fonts/CormorantGaramond-Bold.ttf
    Resources/Fonts/Cinzel-Medium.ttf)

# ---------------------------------------------------------------------------
# Plugin: VST3 + Standalone everywhere, AU on macOS
# ---------------------------------------------------------------------------
set(JGL_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND JGL_FORMATS AU)
endif()

juce_add_plugin(JovahGrooveLab
    COMPANY_NAME "Jovah"
    COMPANY_WEBSITE "https://github.com/sankyrajeev/jovah_indian_lab"
    BUNDLE_ID com.jovah.groovelab
    PLUGIN_MANUFACTURER_CODE Jova
    PLUGIN_CODE JgLb
    FORMATS ${JGL_FORMATS}
    PRODUCT_NAME "Jovah Groove Lab"
    DESCRIPTION "Indian instrumental loop generator"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Generator
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD ${JGL_COPY_PLUGIN_AFTER_BUILD})

target_sources(JovahGrooveLab PRIVATE ${JGL_ENGINE_SOURCES} ${JGL_PLUGIN_SOURCES})
target_include_directories(JovahGrooveLab PRIVATE Source)
target_compile_definitions(JovahGrooveLab PUBLIC ${JGL_COMMON_DEFINITIONS} JUCE_MODAL_LOOPS_PERMITTED=0)
target_link_libraries(JovahGrooveLab
    PRIVATE ${JGL_JUCE_MODULES} JGLBinaryData
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Unit tests
# ---------------------------------------------------------------------------
if(JGL_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(JGLTests PRODUCT_NAME "JGLTests")
    target_sources(JGLTests PRIVATE
        ${JGL_ENGINE_SOURCES}
        ${JGL_PLUGIN_SOURCES}
        Tests/TestMain.cpp
        Tests/TimingTests.cpp
        Tests/ScaleTests.cpp
        Tests/GenerationTests.cpp
        Tests/RenderTests.cpp
        Tests/ExportTests.cpp
        Tests/EngineTests.cpp
        Tests/VoiceTests.cpp
        Tests/TestHelpers.h)
    target_include_directories(JGLTests PRIVATE Source)
    target_compile_definitions(JGLTests PRIVATE ${JGL_COMMON_DEFINITIONS} JGL_UNIT_TESTS=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries(JGLTests
        PRIVATE ${JGL_JUCE_MODULES} JGLBinaryData
                juce::juce_recommended_config_flags
                juce::juce_recommended_warning_flags)
    add_test(NAME JGLTests COMMAND JGLTests)
endif()

# ---------------------------------------------------------------------------
# Command-line renderer (offline loop generation, useful for CI and auditioning)
# ---------------------------------------------------------------------------
if(JGL_BUILD_TOOLS)
    juce_add_console_app(jgl_render PRODUCT_NAME "jgl_render")
    target_sources(jgl_render PRIVATE ${JGL_ENGINE_SOURCES} Tools/RenderCli.cpp)
    target_include_directories(jgl_render PRIVATE Source)
    target_compile_definitions(jgl_render PRIVATE ${JGL_COMMON_DEFINITIONS})
        target_link_libraries(jgl_render
        PRIVATE ${JGL_JUCE_MODULES}
                juce::juce_recommended_config_flags
                juce::juce_recommended_warning_flags)

    juce_add_console_app(jgl_import PRODUCT_NAME "jgl_import")
    target_sources(jgl_import PRIVATE ${JGL_ENGINE_SOURCES} Tools/ImportCli.cpp)
    target_include_directories(jgl_import PRIVATE Source)
    target_compile_definitions(jgl_import PRIVATE ${JGL_COMMON_DEFINITIONS})
    target_link_libraries(jgl_import
        PRIVATE ${JGL_JUCE_MODULES}
                juce::juce_recommended_config_flags
                juce::juce_recommended_warning_flags)
endif()

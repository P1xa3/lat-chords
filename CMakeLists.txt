cmake_minimum_required(VERSION 3.24)
project(LTA_CHORDS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.16-security
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# VST3 runs in compatible DAWs on both Windows and macOS.
# Apple Audio Unit is generated only on macOS (e.g. Logic Pro).
set(LTA_PLUGIN_FORMATS VST3)
if(APPLE)
    list(APPEND LTA_PLUGIN_FORMATS AU)
endif()

juce_add_plugin(LTA_CHORDS
    COMPANY_NAME "LTA"
    BUNDLE_ID "uk.listentoaarxn.ltachords"
    PLUGIN_MANUFACTURER_CODE LtAx
    PLUGIN_CODE Lchd
    FORMATS ${LTA_PLUGIN_FORMATS}
    PRODUCT_NAME "LTA CHORDS"
    PLUGIN_NAME "LTA CHORDS"
    DESCRIPTION "Original chord progression generator, preview synth and MIDI exporter"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(LTA_CHORDS)
target_sources(LTA_CHORDS PRIVATE
    Source/ChordEngine.h
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
)
target_compile_definitions(LTA_CHORDS PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
target_link_libraries(LTA_CHORDS PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

option(LTA_BUILD_TESTS "Build standalone theory engine tests" ON)
if(LTA_BUILD_TESTS)
    enable_testing()
    add_executable(chord_engine_tests tests/engine_tests.cpp)
    target_compile_features(chord_engine_tests PRIVATE cxx_std_17)
    add_test(NAME theory_engine COMMAND chord_engine_tests)
endif()

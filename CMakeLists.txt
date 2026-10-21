# =============================================================================
#  AmpForge - an original guitar amplifier simulator (VST3)
#
#  Build:
#    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
#    cmake --build build --config Release --target AmpForge_VST3
#
#  JUCE is NOT vendored in this repository.  It is fetched by the GitHub Actions
#  workflow (pinned to an exact commit) into ext/JUCE, or you can point this
#  build at your own checkout with -DAMPFORGE_JUCE_DIR=/path/to/JUCE
# =============================================================================

cmake_minimum_required(VERSION 3.22)

# JUCE 8 requires the C language to be enabled as well (it builds zlib, libpng,
# libjpeg and libflac as C).
project(AmpForge
        VERSION 1.0.0
        DESCRIPTION "AmpForge - guitar amplifier simulator"
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# Optional automated validation target (Tests/AmpForgeTests.cpp)
# -----------------------------------------------------------------------------
option(AMPFORGE_BUILD_TESTS "Build the AmpForge automated validation tests" OFF)

# -----------------------------------------------------------------------------
# JUCE
# -----------------------------------------------------------------------------
set(AMPFORGE_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ext/JUCE"
    CACHE PATH "Path to a JUCE source tree (tag 8.0.15)")

if(NOT EXISTS "${AMPFORGE_JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE was not found at '${AMPFORGE_JUCE_DIR}'.\n"
        "Either run the GitHub Actions workflow (which checks JUCE out for you),\n"
        "clone it once locally:\n"
        "    git clone --branch 8.0.15 --depth 1 https://github.com/juce-framework/JUCE.git ext/JUCE\n"
        "or configure with -DAMPFORGE_JUCE_DIR=/path/to/JUCE")
endif()

message(STATUS "AmpForge: using JUCE from ${AMPFORGE_JUCE_DIR}")

# -----------------------------------------------------------------------------
# JUCE module configuration flags.
#
# These must be set as *directory* properties before JUCE is added, because the
# JUCE module targets are compiled in a sub-scope and would otherwise fall back
# to their defaults (which pull in WebKit / libcurl that AmpForge never uses).
# -----------------------------------------------------------------------------
add_compile_definitions(
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_LOAD_CURL_SYMBOLS_LAZILY=0)

add_subdirectory("${AMPFORGE_JUCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE")

# -----------------------------------------------------------------------------
# The plugin
# -----------------------------------------------------------------------------
juce_add_plugin(AmpForge
    # --- identity ------------------------------------------------------------
    PRODUCT_NAME                "AmpForge"
    COMPANY_NAME                "AmpForge Audio"
    COMPANY_WEBSITE             "https://github.com/YOUR_ORG/AmpForge"
    COMPANY_EMAIL               "contact@example.invalid"
    COMPANY_COPYRIGHT           "Copyright (c) 2026 AmpForge Audio"
    BUNDLE_ID                   "com.ampforge.ampforge"
    PLUGIN_MANUFACTURER_CODE    Afge
    PLUGIN_CODE                 Amp1
    DESCRIPTION                 "AmpForge - guitar amplifier simulator with three original amp models, cabinet simulation, delay and reverb."
    VERSION                     "1.0.0"

    # --- formats: VST3 only --------------------------------------------------
    FORMATS                     VST3
    VST3_CATEGORIES             Fx Distortion
    VST3_AUTO_MANIFEST          TRUE

    # --- capabilities --------------------------------------------------------
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE)

juce_generate_juce_header(AmpForge)

target_sources(AmpForge PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/ParamIDs.h
    Source/DSP/Biquad.h
    Source/DSP/AmpModel.h
    Source/DSP/AmpModel.cpp
    Source/DSP/NoiseGate.h
    Source/DSP/NoiseGate.cpp
    Source/DSP/ReverbEngine.h
    Source/DSP/ReverbEngine.cpp
    Source/DSP/DelayEngine.h
    Source/DSP/DelayEngine.cpp
    Source/DSP/CabinetSim.h
    Source/DSP/CabinetSim.cpp
    Source/DSP/LevelMeter.h
    Source/DSP/LevelMeter.cpp
    Source/UI/AmpForgeLookAndFeel.h
    Source/UI/AmpForgeLookAndFeel.cpp
    Source/UI/MeterComponent.h
    Source/UI/MeterComponent.cpp)

target_include_directories(AmpForge PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Source")

target_compile_definitions(AmpForge PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_ALLOW_STATIC_NULL_VARIABLES=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_USE_MP3AUDIOFORMAT=0
    JUCE_WASAPI=1)

target_link_libraries(AmpForge
    PUBLIC
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
    PRIVATE
        juce::juce_recommended_warning_flags)

# -----------------------------------------------------------------------------
# Automated validation
# -----------------------------------------------------------------------------
if(AMPFORGE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

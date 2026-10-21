#!/usr/bin/env bash
# =============================================================================
#  AmpForge - check that every file the build expects is present.
#  Used by the "verify-sources" job in .github/workflows/build.yml and handy
#  locally:  bash Scripts/verify_sources.sh
# =============================================================================
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${root}"

files=(
    "CMakeLists.txt"
    "README.md"
    "LICENSE"
    "THIRD_PARTY_NOTICES.md"
    "Source/PluginProcessor.h"
    "Source/PluginProcessor.cpp"
    "Source/PluginEditor.h"
    "Source/PluginEditor.cpp"
    "Source/ParamIDs.h"
    "Source/DSP/AmpModel.h"
    "Source/DSP/AmpModel.cpp"
    "Source/DSP/NoiseGate.h"
    "Source/DSP/NoiseGate.cpp"
    "Source/DSP/ReverbEngine.h"
    "Source/DSP/ReverbEngine.cpp"
    "Source/DSP/DelayEngine.h"
    "Source/DSP/DelayEngine.cpp"
    "Source/DSP/CabinetSim.h"
    "Source/DSP/CabinetSim.cpp"
    "Source/DSP/LevelMeter.h"
    "Source/DSP/LevelMeter.cpp"
    "Source/DSP/Biquad.h"
    "Source/DSP/DelayLine.h"
    "Source/UI/AmpForgeLookAndFeel.h"
    "Source/UI/AmpForgeLookAndFeel.cpp"
    "Source/UI/MeterComponent.h"
    "Source/UI/MeterComponent.cpp"
    "Source/UI/OptionSelector.h"
    "Tests/AmpForgeTests.cpp"
    "Tests/CMakeLists.txt"
    ".github/workflows/build.yml"
    "Scripts/verify_sources.sh"
)

missing=0

for f in "${files[@]}"; do
    if [ ! -f "${f}" ]; then
        echo "::error::missing file: ${f}"
        missing=$((missing + 1))
    fi
done

if [ "${missing}" -ne 0 ]; then
    echo "${missing} expected file(s) are missing."
    exit 1
fi

echo "All ${#files[@]} expected files are present."

# --- a few cheap consistency checks -------------------------------------------
if ! grep -q "juce_add_plugin(AmpForge" CMakeLists.txt; then
    echo "::error::CMakeLists.txt does not define the AmpForge plugin target"
    exit 1
fi

if ! grep -q "AMPFORGE_BUILD_TESTS" CMakeLists.txt; then
    echo "::error::CMakeLists.txt does not define the AMPFORGE_BUILD_TESTS option"
    exit 1
fi

if ! grep -q "juce_add_console_app(AmpForgeTests" Tests/CMakeLists.txt; then
    echo "::error::Tests/CMakeLists.txt does not define the AmpForgeTests target"
    exit 1
fi

echo "Consistency checks passed."

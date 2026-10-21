# =============================================================================
#  AmpForge automated validation (console application + CTest)
#
#  Configured with:  cmake -S . -B build -DAMPFORGE_BUILD_TESTS=ON
#  Run with:         ctest --test-dir build -C Release --output-on-failure
# =============================================================================

juce_add_console_app(AmpForgeTests PRODUCT_NAME "AmpForge Tests")

juce_generate_juce_header(AmpForgeTests)

target_sources(AmpForgeTests PRIVATE
    AmpForgeTests.cpp)

# The tests check that the repository is complete, so they need to know where
# the repository lives.  Backslashes are normalised so the path is a valid
# C string literal on Windows too.
set(AMPFORGE_TESTS_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
string(REPLACE "\\" "/" AMPFORGE_TESTS_SOURCE_DIR "${AMPFORGE_TESTS_SOURCE_DIR}")

target_compile_definitions(AmpForgeTests PRIVATE
    AMPFORGE_SOURCE_DIR="${AMPFORGE_TESTS_SOURCE_DIR}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(AmpForgeTests PRIVATE
    AmpForge
    juce::juce_recommended_config_flags)

add_test(NAME AmpForgeTests COMMAND AmpForgeTests)

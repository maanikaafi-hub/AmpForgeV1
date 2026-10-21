/*  AmpForge - parameter identifiers, ranges and value formatting.
 *
 *  Every parameter of the plugin is declared in one place so that the
 *  processor, the editor and the tests can never drift apart.
 */
#pragma once

#include <JuceHeader.h>

namespace ampforge
{

//==============================================================================
/** Parameter IDs (also used as keys in the saved state). */
namespace id
{
    inline constexpr const char* ampModel       = "ampModel";
    inline constexpr const char* gain           = "gain";
    inline constexpr const char* bass           = "bass";
    inline constexpr const char* mid            = "mid";
    inline constexpr const char* treble         = "treble";
    inline constexpr const char* presence       = "presence";
    inline constexpr const char* master         = "master";

    inline constexpr const char* gateEnable     = "gateEnable";
    inline constexpr const char* gateThreshold  = "gateThreshold";
    inline constexpr const char* gateRelease    = "gateRelease";

    inline constexpr const char* cabEnable      = "cabEnable";
    inline constexpr const char* cabProfile     = "cabProfile";

    inline constexpr const char* reverbMode     = "reverbMode";
    inline constexpr const char* reverbMix      = "reverbMix";
    inline constexpr const char* reverbDecay    = "reverbDecay";
    inline constexpr const char* reverbPreDelay = "reverbPreDelay";
    inline constexpr const char* reverbTone     = "reverbTone";

    inline constexpr const char* delayMode      = "delayMode";
    inline constexpr const char* delayTime      = "delayTime";
    inline constexpr const char* delayFeedback  = "delayFeedback";
    inline constexpr const char* delayMix       = "delayMix";
    inline constexpr const char* delayTone      = "delayTone";
}

//==============================================================================
/** Human readable choice lists (order matters: it is the parameter index). */
namespace choice
{
    inline juce::StringArray ampModels()   { return { "American Clean", "British Crunch", "Modern High Gain" }; }
    inline juce::StringArray reverbModes() { return { "Room", "Spring", "Hall" }; }
    inline juce::StringArray delayModes()  { return { "Digital", "Analog" }; }
    inline juce::StringArray cabProfiles() { return { "4x12 Closed", "2x12 Combo", "1x12 Open", "User IR" }; }
}

//==============================================================================
/** Default values, kept next to the ranges so both stay in sync. */
namespace def
{
    inline constexpr int    ampModel       = 1;      // British Crunch
    inline constexpr float  gain           = 4.0f;
    inline constexpr float  bass           = 0.0f;
    inline constexpr float  mid            = 0.0f;
    inline constexpr float  treble         = 0.0f;
    inline constexpr float  presence       = 0.0f;
    inline constexpr float  master         = -10.0f;

    inline constexpr bool   gateEnable     = true;
    inline constexpr float  gateThreshold  = -52.0f;
    inline constexpr float  gateRelease    = 150.0f;

    inline constexpr bool   cabEnable      = true;
    inline constexpr int    cabProfile     = 0;      // 4x12 Closed

    inline constexpr int    reverbMode     = 0;      // Room
    inline constexpr float  reverbMix      = 20.0f;
    inline constexpr float  reverbDecay    = 35.0f;
    inline constexpr float  reverbPreDelay = 18.0f;
    inline constexpr float  reverbTone     = 55.0f;

    inline constexpr int    delayMode      = 0;      // Digital
    inline constexpr float  delayTime      = 320.0f;
    inline constexpr float  delayFeedback  = 25.0f;
    inline constexpr float  delayMix       = 15.0f;
    inline constexpr float  delayTone      = 60.0f;
}

//==============================================================================
/** Value conversions shared by the DSP, the GUI and the tests. */
namespace convert
{
    /** gain knob (0..10) -> 0..1 */
    inline float driveNorm      (float v) noexcept { return juce::jlimit (0.0f, 1.0f, v / 10.0f); }

    /** reverb decay knob (0..100) -> seconds (0.15 .. 8.0) */
    inline float decayToSeconds (float v) noexcept
    {
        const auto n = juce::jlimit (0.0f, 1.0f, v / 100.0f);
        return 0.15f * std::pow (8.0f / 0.15f, n);
    }

    /** delay/reverb mix knob (0..100) -> 0..1 */
    inline float mixNorm        (float v) noexcept { return juce::jlimit (0.0f, 1.0f, v / 100.0f); }

    /** delay feedback knob (0..95) -> 0..0.9 (the stable region) */
    inline float feedbackNorm   (float v) noexcept { return juce::jlimit (0.0f, 0.9f, (v / 95.0f) * 0.9f); }

    /** tone knob (0..100) -> 0..1 (0 = dark, 1 = bright) */
    inline float toneNorm       (float v) noexcept { return juce::jlimit (0.0f, 1.0f, v / 100.0f); }

    /** dB -> linear gain, with a hard floor for "-inf" style settings */
    inline float dbToGain       (float db) noexcept
    {
        return db <= -100.0f ? 0.0f : juce::Decibels::decibelsToGain (db);
    }
}

//==============================================================================
/** Display strings for the GUI value readouts. */
namespace display
{
    inline juce::String dB        (float v) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; }
    inline juce::String percent   (float v) { return juce::String (v, 0) + " %"; }
    inline juce::String gainValue (float v) { return juce::String (v, 1); }
    inline juce::String ms        (float v) { return juce::String (v, v < 100.0f ? 1 : 0) + " ms"; }
    inline juce::String seconds   (float v) { return juce::String (v, v < 1.0f ? 2 : 1) + " s"; }
}

} // namespace ampforge

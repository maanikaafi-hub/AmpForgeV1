/*  AmpForge - the three amp models.
 *
 *  Each model is a genuinely different signal path (different filter
 *  frequencies, a different number of gain stages, a different clipping law and
 *  a different amount of power-supply sag), not a shared path with a different
 *  EQ preset on the end.
 *
 *    1. American Clean   - 2 gain stages, cubic soft clip, wide dynamics,
 *                          gentle mid scoop, smooth high end.
 *    2. British Crunch   - 3 gain stages, asymmetric (even harmonic) clip,
 *                          midrange hump, bright cap, responsive breakup.
 *    3. Modern High Gain - 3 gain stages, hard clip, tight high-passed low end,
 *                          power amp resonance, heavy sustain.
 *
 *  Real-time contract:
 *    - prepare()  : allocates nothing except a fixed, pre-sized channel array.
 *    - setXXX()   : only stores a smoothing target (call from any thread).
 *    - updateSmoothers() + processSample() : no allocation, no locks, no I/O.
 */
#pragma once

#include <JuceHeader.h>
#include "Biquad.h"

namespace ampforge
{

enum class AmpType
{
    AmericanClean  = 0,
    BritishCrunch  = 1,
    ModernHighGain = 2
};

inline constexpr int numAmpTypes = 3;

inline juce::String ampTypeName (AmpType t)
{
    switch (t)
    {
        case AmpType::AmericanClean:  return "American Clean";
        case AmpType::BritishCrunch:  return "British Crunch";
        case AmpType::ModernHighGain: return "Modern High Gain";
    }
    return {};
}

//==============================================================================
class AmpModel
{
public:
    AmpModel() = default;

    /** Call from prepareToPlay().  No allocation beyond prepare() itself. */
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    /** Switching model re-tunes every stage and clears the filter state. */
    void setType (AmpType newType) noexcept;
    AmpType getType() const noexcept { return type; }

    /** 0..1 (from the Gain knob) */
    void setDrive    (float driveNormalised) noexcept;
    /** master volume in dB (-60 .. +6) */
    void setMasterDb (float masterDb) noexcept;
    /** tone stack, each -12 .. +12 dB */
    void setTone     (float bassDb, float midDb, float trebleDb, float presenceDb) noexcept;

    /** Advance the parameter smoothing by exactly one sample.
     *  MUST be called once per sample, before any channel is processed. */
    void updateSmoothers() noexcept;

    /** Process one sample of one channel. */
    float processSample (int channel, float input) noexcept;

private:
    //==========================================================================
    enum class ClipStyle { Cubic, Asymmetric, Hard };

    struct Character
    {
        float preGainMinDb  = -2.0f;
        float preGainMaxDb  = 18.0f;

        float inputHP       = 32.0f;    // Hz   - tightens the low end
        float brightFreq    = 1500.0f;  // Hz   - pre-gain bright cap (high shelf)
        float brightGainDb  = 3.0f;

        float midFreq       = 850.0f;   // Hz   - inter stage voicing
        float midGainDb     = -2.5f;
        float midQ          = 0.7f;

        float stageLP       = 9000.0f;  // Hz   - tames fizz after each stage

        float lowResFreq    = 95.0f;    // Hz   - power amp / cabinet resonance
        float lowResGainDb  = 1.5f;
        float lowResQ       = 0.8f;

        float bassFreq      = 100.0f;   // tone stack
        float midToneFreq   = 780.0f;
        float midToneQ      = 0.9f;
        float trebleFreq    = 2400.0f;
        float presenceFreq  = 3800.0f;
        float presenceQ     = 0.7f;

        float sagAmount     = 0.12f;    // dynamic power supply compression
        float sagAttackSec  = 0.004f;
        float sagReleaseSec = 0.120f;

        float stageGains[3] = { 1.0f, 0.85f, 0.0f };
        int   numStages     = 2;
        ClipStyle clipStyle = ClipStyle::Cubic;

        float outputTrim    = 1.0f;     // static level match between models
    };

    struct Channel
    {
        Biquad inputHP, brightCap, midHump, stageLP, lowRes;
        Biquad toneLow, toneMid, toneHigh, presence, dcBlock;
        float sag = 0.0f;
    };

    //==========================================================================
    static Character getCharacter (AmpType) noexcept;
    static float clipSample (float x, ClipStyle) noexcept;

    void updateCoefficients() noexcept;
    void updateSagCoefficients() noexcept
    {
        sagAttackCoeff  = (float) (1.0 - std::exp (-1.0 / std::max (1.0e-6, character.sagAttackSec  * sampleRate)));
        sagReleaseCoeff = (float) (1.0 - std::exp (-1.0 / std::max (1.0e-6, character.sagReleaseSec * sampleRate)));
    }

    //==========================================================================
    std::array<Channel, 2> channels;
    int numChannels = 2;

    Character character = getCharacter (AmpType::AmericanClean);
    AmpType type        = AmpType::AmericanClean;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driveSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> masterSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bassSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> midSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> trebleSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> presenceSmoothed;

    float currentDrive    = 0.0f;
    float currentMasterDb = -6.0f;
    float currentBass     = 0.0f;
    float currentMid      = 0.0f;
    float currentTreble   = 0.0f;
    float currentPresence = 0.0f;

    float preGain    = 1.0f;
    float masterGain = 0.5f;

    float lastBass = 1.0e9f, lastMid = 1.0e9f, lastTreble = 1.0e9f, lastPresence = 1.0e9f;

    float sagAttackCoeff  = 0.01f;
    float sagReleaseCoeff = 0.001f;

    double sampleRate = 44100.0;
    bool coefficientsDirty = true;
    bool prepared = false;
};

} // namespace ampforge

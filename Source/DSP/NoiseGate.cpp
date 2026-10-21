/*  AmpForge - noise gate (implementation). */
#include "NoiseGate.h"

namespace ampforge
{

namespace
{
    constexpr float holdTimeSeconds  = 0.04f;   // keeps the gate from chattering
    constexpr float attackSeconds    = 0.0015f;
    constexpr float hysteresisDb     = 4.0f;    // re-close 4 dB below the open point
    constexpr float meterFloorDb     = -60.0f;
}

//==============================================================================
void NoiseGate::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate  = spec.sampleRate;
    numChannels = juce::jlimit (1, 2, (int) spec.numChannels);

    attackCoeff  = (float) (1.0 - std::exp (-1.0 / (attackSeconds * sampleRate)));
    holdSamples  = (int) std::ceil (holdTimeSeconds * sampleRate);

    setReleaseMs (150.0f);
    reset();
}

void NoiseGate::reset() noexcept
{
    gateGain    = 0.0f;
    holdCounter = 0;
    isOpen      = false;
    lastGainReductionDb.store (meterFloorDb);
}

void NoiseGate::setEnabled (bool shouldBeEnabled) noexcept
{
    enabled.store (shouldBeEnabled);

    if (! shouldBeEnabled)
        lastGainReductionDb.store (0.0f);
}

void NoiseGate::setThresholdDb (float thresholdDb) noexcept
{
    const auto db = juce::jlimit (-80.0f, 0.0f, thresholdDb);
    thresholdLinear.store (juce::Decibels::decibelsToGain ((double) db));

    openThreshold  = thresholdLinear.load();
    closeThreshold = juce::Decibels::decibelsToGain ((double) (db - hysteresisDb));
}

void NoiseGate::setReleaseMs (float releaseMs) noexcept
{
    const auto ms = juce::jlimit (20.0f, 800.0f, releaseMs);
    releaseCoeff = (float) (1.0 - std::exp (-1.0 / (ms * 0.001 * sampleRate)));
    releaseCoeffVal.store (releaseCoeff);
}

//==============================================================================
void NoiseGate::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples  = buffer.getNumSamples();
    const int numChans    = juce::jmin (buffer.getNumChannels(), numChannels);

    if (numSamples <= 0 || numChans <= 0)
        return;

    if (! enabled.load())
    {
        gateGain = 1.0f;
        isOpen = true;
        holdCounter = 0;
        return;
    }

    const auto channels = buffer.getArrayOfWritePointers();

    const float open  = openThreshold;
    const float close = closeThreshold;
    const float atk   = attackCoeff;
    const float rel   = releaseCoeff;

    float minGain = 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // --- one detector for all channels: no stereo image shifts ---------
        float env = 0.0f;

        for (int ch = 0; ch < numChans; ++ch)
            env = std::max (env, std::abs (channels[ch][i]));

        if (env >= open)
        {
            isOpen = true;
            holdCounter = holdSamples;
        }
        else if (env < close)
        {
            if (holdCounter > 0)
                --holdCounter;
            else
                isOpen = false;
        }

        const float target = isOpen ? 1.0f : 0.0f;
        const float coeff  = target > gateGain ? atk : rel;

        gateGain += coeff * (target - gateGain);

        if (gateGain < minGain)
            minGain = gateGain;

        for (int ch = 0; ch < numChans; ++ch)
            channels[ch][i] *= gateGain;
    }

    const float gainDb = (float) juce::Decibels::gainToDecibels ((double) juce::jmax (minGain, 0.001f));

    lastGainReductionDb.store (juce::jlimit (meterFloorDb, 0.0f, gainDb));
}

} // namespace ampforge

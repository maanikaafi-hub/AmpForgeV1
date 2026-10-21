/*  AmpForge - reverb (implementation). */
#include "ReverbEngine.h"

namespace ampforge
{

//==============================================================================
ReverbEngine::Tunings ReverbEngine::getTunings (Mode m) noexcept
{
    Tunings t;

    switch (m)
    {
        case Mode::Room:
            t.combScale       = 0.62f;
            t.feedbackBase    = 0.76f;
            t.feedbackRange   = 0.18f;
            t.dampMin         = 0.15f;
            t.dampMax         = 0.65f;
            t.allpassFeedback = 0.50f;
            t.wetGain         = 0.90f;
            t.highPassHz      = 140.0f;
            t.lowPassMinHz    = 900.0f;
            t.lowPassMaxHz    = 11000.0f;
            t.preDelayMaxMs   = 40.0f;
            break;

        case Mode::Spring:
            t.combScale       = 0.34f;
            t.feedbackBase    = 0.72f;
            t.feedbackRange   = 0.20f;
            t.dampMin         = 0.30f;
            t.dampMax         = 0.80f;
            t.allpassFeedback = 0.62f;
            t.wetGain         = 0.80f;
            t.highPassHz      = 320.0f;
            t.lowPassMinHz    = 1200.0f;
            t.lowPassMaxHz    = 5200.0f;
            t.modRateHz       = 0.70f;
            t.modDepthMs      = 0.35f;
            t.bandFreqHz      = 1250.0f;
            t.bandGainDb      = 4.0f;
            t.bandQ           = 1.2f;
            t.preDelayMaxMs   = 30.0f;
            break;

        case Mode::Hall:
            t.combScale       = 1.25f;
            t.feedbackBase    = 0.82f;
            t.feedbackRange   = 0.15f;
            t.dampMin         = 0.10f;
            t.dampMax         = 0.55f;
            t.allpassFeedback = 0.50f;
            t.wetGain         = 1.00f;
            t.highPassHz      = 110.0f;
            t.lowPassMinHz    = 800.0f;
            t.lowPassMaxHz    = 13000.0f;
            t.preDelayMaxMs   = 120.0f;
            break;
    }

    return t;
}

//==============================================================================
void ReverbEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate  = spec.sampleRate;
    numChannels = juce::jlimit (1, 2, (int) spec.numChannels);

    // Allocate every line for the longest possible tuning at this rate so that
    // switching mode never allocates.
    const double maxCombMs = combMilliseconds[3] * 1.25 + stereoSpreadMs + 1.0;
    const int combSize = (int) std::ceil (maxCombMs * 0.001 * sampleRate) + 16;
    const int allpassSize = (int) std::ceil ((allpassMilliseconds[0] + 1.0) * 0.001 * sampleRate) + 16;

    for (auto& c : combs)    c.allocate (combSize);
    for (auto& a : allpasses) a.allocate (allpassSize);

    preDelay.setSize ((int) std::ceil (0.25 * sampleRate) + 16);

    mixSmoothed.reset      (sampleRate, 0.05);
    decaySmoothed.reset    (sampleRate, 0.10);
    preDelaySmoothed.reset (sampleRate, 0.12);
    toneSmoothed.reset     (sampleRate, 0.05);

    mixSmoothed.setCurrentAndTargetValue      (mixSmoothed.getCurrentValue());
    decaySmoothed.setCurrentAndTargetValue    (decaySmoothed.getCurrentValue());
    preDelaySmoothed.setCurrentAndTargetValue (preDelaySmoothed.getCurrentValue());
    toneSmoothed.setCurrentAndTargetValue     (toneSmoothed.getCurrentValue());

    tunings = getTunings (mode);
    applyModeSettings();
    reset();

    lastDecay = -1.0f;
    lastTone  = -1.0f;
    prepared  = true;
}

void ReverbEngine::reset() noexcept
{
    for (auto& c : combs)    c.reset();
    for (auto& a : allpasses) a.reset();

    preDelay.reset();
    modPhase = 0.0f;

    for (auto& f : wetHighPass) f.reset();
    for (auto& f : wetLowPass)  f.reset();
    for (auto& f : springBand)  f.reset();
}

void ReverbEngine::setMode (Mode newMode) noexcept
{
    if (newMode == mode && prepared)
        return;

    mode = newMode;
    tunings = getTunings (newMode);

    if (prepared)
    {
        // No allocation: only the effective lengths of the pre-allocated lines
        // change, and the tails are cleared so no stale samples leak out.
        applyModeSettings();
        reset();
    }

    lastDecay = -1.0f;
    lastTone  = -1.0f;
}

void ReverbEngine::applyModeSettings() noexcept
{
    const double fs = sampleRate;

    for (int ch = 0; ch < 2; ++ch)
    {
        for (int k = 0; k < 4; ++k)
        {
            const auto ms = combMilliseconds[k] * tunings.combScale
                          + (ch == 1 ? stereoSpreadMs : 0.0f);
            combs[(size_t) (ch * 4 + k)].setLength ((int) std::ceil (ms * 0.001 * fs));
        }

        for (int k = 0; k < 3; ++k)
        {
            const auto ms = allpassMilliseconds[k] * juce::jmax (0.35f, tunings.combScale);
            allpasses[(size_t) (ch * 3 + k)].setLength ((int) std::ceil (ms * 0.001 * fs));
        }
    }
}

//==============================================================================
void ReverbEngine::setMix        (float mix01) noexcept        { mixSmoothed.setTargetValue      (juce::jlimit (0.0f, 1.0f, mix01)); }
void ReverbEngine::setDecay      (float decay01) noexcept      { decaySmoothed.setTargetValue    (juce::jlimit (0.0f, 1.0f, decay01)); }
void ReverbEngine::setPreDelayMs (float milliseconds) noexcept { preDelaySmoothed.setTargetValue (juce::jlimit (0.0f, tunings.preDelayMaxMs, milliseconds)); }
void ReverbEngine::setTone       (float tone01) noexcept       { toneSmoothed.setTargetValue     (juce::jlimit (0.0f, 1.0f, tone01)); }

//==============================================================================
void ReverbEngine::updateTunings (float decay, float tone) noexcept
{
    const double fs = sampleRate;

    const float feedback = juce::jlimit (0.20f, 0.965f,
                                         tunings.feedbackBase + tunings.feedbackRange * decay);
    const float damp     = juce::jlimit (0.02f, 0.92f,
                                         tunings.dampMin + (tunings.dampMax - tunings.dampMin) * (1.0f - tone));

    for (auto& c : combs)
    {
        c.feedback = feedback;
        c.damp     = damp;
    }

    for (auto& a : allpasses)
        a.feedback = tunings.allpassFeedback;

    // Keep the wet level roughly constant as the decay is turned up: the
    // steady state gain of a damped comb is proportional to 1 / (1 - feedback).
    combInputGain = 0.20f * (1.0f - feedback);

    const double lpHz = tunings.lowPassMinHz
                      * std::pow (tunings.lowPassMaxHz / tunings.lowPassMinHz, (double) tone);

    for (int ch = 0; ch < 2; ++ch)
    {
        wetHighPass[(size_t) ch].setHighPass (fs, tunings.highPassHz, 0.7071);
        wetLowPass [(size_t) ch].setLowPass  (fs, lpHz, 0.7071);

        if (tunings.bandGainDb != 0.0f)
            springBand[(size_t) ch].setPeaking (fs, tunings.bandFreqHz, tunings.bandQ, tunings.bandGainDb);
        else
            springBand[(size_t) ch].makeBypass();
    }

    lastDecay = decay;
    lastTone  = tone;
}

//==============================================================================
void ReverbEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! prepared)
        return;

    const int numSamples = buffer.getNumSamples();
    const int numChans   = juce::jmin (buffer.getNumChannels(), numChannels);

    if (numSamples <= 0 || numChans <= 0)
        return;

    const auto channels = buffer.getArrayOfWritePointers();

    const double fs = sampleRate;
    const float modDepthSamples = tunings.modDepthMs * 0.001f * (float) fs;
    const float modIncrement = (float) (2.0 * juce::MathConstants<double>::pi * tunings.modRateHz / fs);

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix        = mixSmoothed.getNextValue();
        const float decay      = decaySmoothed.getNextValue();
        const float preDelayMs = preDelaySmoothed.getNextValue();
        const float tone       = toneSmoothed.getNextValue();

        if (std::abs (decay - lastDecay) > 0.002f || std::abs (tone - lastTone) > 0.002f)
            updateTunings (decay, tone);

        // The reverb keeps running even at mix = 0 so that raising the mix
        // never produces a burst of stale tail samples.

        // --- mono send into the pre-delay (a reverb is fed a summed signal) --
        float mono = 0.0f;

        for (int ch = 0; ch < numChans; ++ch)
            mono += channels[ch][i];

        mono *= 1.0f / (float) numChans;

        preDelay.write (mono);

        float modulation = 0.0f;

        if (modDepthSamples > 0.0f)
        {
            modPhase += modIncrement;

            if (modPhase > juce::MathConstants<float>::twoPi)
                modPhase -= juce::MathConstants<float>::twoPi;

            modulation = std::sin (modPhase) * modDepthSamples;
        }

        const float readPos = juce::jmax (1.0f, preDelayMs * 0.001f * (float) fs + modulation);
        const float delayed = preDelay.read (readPos);

        // --- per channel comb bank -> all-pass chain -> tone ----------------
        for (int ch = 0; ch < numChans; ++ch)
        {
            float wet = 0.0f;

            const int combBase = ch * 4;

            for (int k = 0; k < 4; ++k)
                wet += combs[(size_t) (combBase + k)].process (delayed * combInputGain);

            wet *= 0.25f;

            const int apBase = ch * 3;

            for (int k = 0; k < 3; ++k)
                wet = allpasses[(size_t) (apBase + k)].process (wet);

            wet = wetHighPass[(size_t) ch].process (wet);
            wet = wetLowPass [(size_t) ch].process (wet);
            wet = springBand [(size_t) ch].process (wet);

            channels[ch][i] = channels[ch][i] * (1.0f - mix) + wet * tunings.wetGain * mix;
        }
    }
}

} // namespace ampforge

/*  AmpForge - delay (implementation). */
#include "DelayEngine.h"

namespace ampforge
{

namespace
{
    constexpr float maxDelaySeconds   = 1.5f;
    constexpr float analogSpreadRatio = 1.007f;   // right channel offset
    constexpr double wowRateHz        = 0.55;
    constexpr double flutterRateHz    = 6.30;
    constexpr float  wowDepthMs       = 0.30f;
    constexpr float  flutterDepthMs   = 0.045f;
}

//==============================================================================
void DelayEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate  = spec.sampleRate;
    numChannels = juce::jlimit (1, 2, (int) spec.numChannels);

    maxDelaySamples = (int) std::ceil (maxDelaySeconds * sampleRate) + 8;

    for (auto& c : channels)
        c.line.setSize (maxDelaySamples);

    timeSmoothed.reset     (sampleRate, 0.08);
    feedbackSmoothed.reset (sampleRate, 0.05);
    mixSmoothed.reset      (sampleRate, 0.05);
    toneSmoothed.reset     (sampleRate, 0.05);

    lastTone = -1.0f;
    reset();
    prepared = true;
}

void DelayEngine::reset() noexcept
{
    for (size_t i = 0; i < channels.size(); ++i)
    {
        auto& c = channels[i];
        c.line.reset();
        c.feedbackLowPass.reset();
        c.feedbackHighPass.reset();
        c.toneLowPass.reset();
        c.lfoPhase = (i == 0) ? 0.0f : juce::MathConstants<float>::halfPi;
    }

    lastTone = -1.0f;
}

void DelayEngine::setMode (Mode newMode) noexcept
{
    if (newMode == mode && prepared)
        return;

    mode = newMode;
    lastTone = -1.0f;
}

//==============================================================================
void DelayEngine::setTimeMs    (float milliseconds) noexcept { timeSmoothed.setTargetValue     (juce::jlimit (10.0f, 1500.0f, milliseconds)); }
void DelayEngine::setFeedback  (float feedback01) noexcept   { feedbackSmoothed.setTargetValue (juce::jlimit (0.0f, 0.9f, feedback01)); }
void DelayEngine::setMix       (float mix01) noexcept        { mixSmoothed.setTargetValue      (juce::jlimit (0.0f, 1.0f, mix01)); }
void DelayEngine::setTone      (float tone01) noexcept       { toneSmoothed.setTargetValue     (juce::jlimit (0.0f, 1.0f, tone01)); }

//==============================================================================
void DelayEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! prepared)
        return;

    const int numSamples = buffer.getNumSamples();
    const int numChans   = juce::jmin (buffer.getNumChannels(), numChannels);

    if (numSamples <= 0 || numChans <= 0)
        return;

    const auto chans = buffer.getArrayOfWritePointers();

    const double fs = sampleRate;
    const bool analog = (mode == Mode::Analog);

    const float wowIncrement     = (float) (2.0 * juce::MathConstants<double>::pi * wowRateHz / fs);
    const float flutterIncrement = (float) (2.0 * juce::MathConstants<double>::pi * flutterRateHz / fs);
    const float maxRead          = (float) (maxDelaySamples - 2);

    for (int i = 0; i < numSamples; ++i)
    {
        const float timeMs   = timeSmoothed.getNextValue();
        const float feedback = feedbackSmoothed.getNextValue();
        const float mix      = mixSmoothed.getNextValue();
        const float tone     = toneSmoothed.getNextValue();

        if (std::abs (tone - lastTone) > 0.002f)
        {
            const double cutoff = 500.0 * std::pow (16000.0 / 500.0, (double) juce::jlimit (0.0f, 1.0f, tone));

            for (auto& c : channels)
            {
                c.toneLowPass.setLowPass     (fs, cutoff, 0.7071);
                c.feedbackLowPass.setLowPass (fs, analog ? 3400.0 : 12000.0, 0.7071);
                c.feedbackHighPass.setHighPass (fs, analog ? 110.0 : 20.0, 0.7071);
            }

            lastTone = tone;
        }

        const float baseDelay = timeMs * 0.001f * (float) fs;

        for (int ch = 0; ch < numChans; ++ch)
        {
            auto& c = channels[(size_t) ch];

            float modulation = 0.0f;

            if (analog)
            {
                c.lfoPhase += wowIncrement;

                if (c.lfoPhase > juce::MathConstants<float>::twoPi)
                    c.lfoPhase -= juce::MathConstants<float>::twoPi;

                modulation = std::sin (c.lfoPhase) * wowDepthMs * 0.001f * (float) fs
                           + std::sin (c.lfoPhase * (flutterRateHz / wowRateHz)) * flutterDepthMs * 0.001f * (float) fs;
            }

            const float spread = (analog && ch == 1) ? analogSpreadRatio : 1.0f;
            const float readPos = juce::jlimit (1.0f, maxRead, baseDelay * spread + modulation);

            const float dry = chans[ch][i];
            const float delayed = c.line.read (readPos);

            // --- feedback path ---------------------------------------------
            float toWrite = dry + delayed * feedback;

            if (analog)
            {
                toWrite = c.feedbackHighPass.process (toWrite);
                toWrite = c.feedbackLowPass.process (toWrite);
                // the fast tanh approximation is only valid for |x| <= 5
                toWrite = 0.85f * juce::dsp::FastMathApproximations::tanh (juce::jlimit (-4.0f, 4.0f, 1.15f * toWrite));
            }

            c.line.write (toWrite);

            // --- output ------------------------------------------------------
            const float wet = c.toneLowPass.process (delayed);

            chans[ch][i] = dry * (1.0f - mix) + wet * mix;
        }
    }
}

} // namespace ampforge

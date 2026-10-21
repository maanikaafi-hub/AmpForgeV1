/*  AmpForge - level meters (implementation). */
#include "LevelMeter.h"

namespace ampforge
{

void LevelMeter::prepare (int numChannelsToUse) noexcept
{
    juce::ignoreUnused (numChannelsToUse);
    reset();
}

void LevelMeter::reset() noexcept
{
    channels[0].peak.store (0.0f);
    channels[0].rms.store  (0.0f);
    channels[1].peak.store (0.0f);
    channels[1].rms.store  (0.0f);
}

void LevelMeter::push (const juce::AudioBuffer<float>& buffer, int numChannelsToMeasure) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numChans   = juce::jlimit (1, 2, numChannelsToMeasure);

    if (numSamples <= 0)
        return;

    for (int ch = 0; ch < numChans; ++ch)
    {
        const auto* data = buffer.getReadPointer (juce::jmin (ch, buffer.getNumChannels() - 1));

        float peak = 0.0f;
        double sum = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            const float s = data[i];
            const float a = std::abs (s);

            if (a > peak)
                peak = a;

            sum += (double) s * (double) s;
        }

        channels[(size_t) ch].peak.store (peak);
        channels[(size_t) ch].rms.store  ((float) std::sqrt (sum / (double) numSamples));
    }
}

} // namespace ampforge

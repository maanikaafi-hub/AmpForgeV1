/*  AmpForge - peak / RMS level meters.
 *
 *  Updated from processBlock(), read from the editor's timer.  The atomics are
 *  lock free on every supported platform, so the GUI can poll them freely.
 */
#pragma once

#include <JuceHeader.h>

namespace ampforge
{

class LevelMeter
{
public:
    void prepare (int numChannelsToUse) noexcept;
    void reset() noexcept;

    /** Call once per block from the audio thread. */
    void push (const juce::AudioBuffer<float>& buffer, int numChannelsToMeasure) noexcept;

    float getPeak (int channel) const noexcept
    {
        return channels[(size_t) juce::jlimit (0, 1, channel)].peak.load();
    }

    float getRms (int channel) const noexcept
    {
        return channels[(size_t) juce::jlimit (0, 1, channel)].rms.load();
    }

private:
    struct Channel
    {
        std::atomic<float> peak { 0.0f };
        std::atomic<float> rms  { 0.0f };
    };

    std::array<Channel, 2> channels;
};

} // namespace ampforge

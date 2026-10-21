/*  AmpForge - delay (Digital / Analog inspired).
 *
 *  Digital: a clean interpolated delay line, no colouration in the loop.
 *  Analog : the feedback path runs through a low pass, a DC blocking high pass
 *           and a soft saturator, and the read position is modulated by a slow
 *           LFO (wow and flutter).  The right channel runs marginally longer
 *           than the left, which is how a bucket brigade stereo delay spreads.
 *
 *  Stability: feedback is clamped to 0.9 and, in analog mode, the in-loop
 *  filters and saturator all have a gain of 1 or less, so the loop gain is
 *  strictly below unity and the delay can never run away.
 */
#pragma once

#include <JuceHeader.h>
#include "Biquad.h"
#include "DelayLine.h"

namespace ampforge
{

class DelayEngine
{
public:
    enum class Mode { Digital = 0, Analog };

    static juce::String modeName (Mode m)
    {
        return m == Mode::Digital ? "Digital" : "Analog";
    }

    //--------------------------------------------------------------------------
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    void setMode       (Mode newMode) noexcept;
    Mode getMode() const noexcept { return mode; }

    void setTimeMs    (float milliseconds) noexcept;  // 10..1500
    void setFeedback  (float feedback01) noexcept;    // 0..1 (clamped to 0.9)
    void setMix       (float mix01) noexcept;         // 0..1
    void setTone      (float tone01) noexcept;        // 0..1 (dark..bright)

    /** In place.  Real-time safe. */
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    struct Channel
    {
        DelayLine line;
        Biquad feedbackLowPass;
        Biquad feedbackHighPass;
        Biquad toneLowPass;
        float lfoPhase = 0.0f;
    };

    std::array<Channel, 2> channels;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> timeSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> feedbackSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> toneSmoothed;

    Mode mode = Mode::Digital;

    float lastTone = -1.0f;
    double sampleRate = 44100.0;
    int numChannels = 2;
    int maxDelaySamples = 1024;
    bool prepared = false;
};

} // namespace ampforge

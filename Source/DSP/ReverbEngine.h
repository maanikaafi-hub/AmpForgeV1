/*  AmpForge - reverb (Room / Spring inspired / Hall).
 *
 *  Schroeder style topology: a mono pre-delay feeds parallel damped comb
 *  filters, whose sum runs through a series of all-pass filters, followed by a
 *  tone stage.  The three modes differ in comb length, damping, pre-delay
 *  range, wet filtering and (for Spring) in a slow modulation of the pre-delay,
 *  which is what gives a spring tank its loose, wobbling character.
 *
 *  Real-time contract: prepare() allocates; every other call is lock free and
 *  allocation free, including setMode() (it only re-lengthens the existing
 *  comb buffers).
 */
#pragma once

#include <JuceHeader.h>
#include "Biquad.h"
#include "DelayLine.h"

namespace ampforge
{

class ReverbEngine
{
public:
    enum class Mode { Room = 0, Spring, Hall };

    static juce::String modeName (Mode m)
    {
        switch (m)
        {
            case Mode::Room:   return "Room";
            case Mode::Spring: return "Spring";
            case Mode::Hall:   return "Hall";
        }
        return {};
    }

    //--------------------------------------------------------------------------
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    void setMode (Mode newMode) noexcept;
    Mode getMode() const noexcept { return mode; }

    void setMix        (float mix01) noexcept;        // 0..1
    void setDecay      (float decay01) noexcept;      // 0..1
    void setPreDelayMs (float milliseconds) noexcept; // 0..120
    void setTone       (float tone01) noexcept;       // 0..1 (dark..bright)

    /** In place.  Real-time safe. */
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    //==========================================================================
    struct Comb
    {
        std::vector<float> buffer;
        int length = 64;
        int index  = 0;
        float store    = 0.0f;
        float feedback = 0.82f;
        float damp     = 0.30f;

        void allocate (int maxLength) { buffer.assign ((size_t) std::max (16, maxLength), 0.0f); length = (int) buffer.size(); index = 0; }
        void reset() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); store = 0.0f; index = 0; }

        void setLength (int newLength) noexcept
        {
            length = juce::jlimit (4, (int) buffer.size(), newLength);
            if (index >= length) index = 0;
        }

        inline float process (float input) noexcept
        {
            const float output = buffer[(size_t) index];

            // one pole low pass inside the feedback path: high frequencies die
            // away faster than low ones, exactly as in a real room
            store = output * (1.0f - damp) + store * damp;

            buffer[(size_t) index] = input + store * feedback;

            if (++index >= length)
                index = 0;

            return output;
        }
    };

    struct Allpass
    {
        std::vector<float> buffer;
        int length = 64;
        int index  = 0;
        float feedback = 0.5f;

        void allocate (int maxLength) { buffer.assign ((size_t) std::max (16, maxLength), 0.0f); length = (int) buffer.size(); index = 0; }
        void reset() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); index = 0; }

        void setLength (int newLength) noexcept
        {
            length = juce::jlimit (4, (int) buffer.size(), newLength);
            if (index >= length) index = 0;
        }

        inline float process (float input) noexcept
        {
            const float bufout = buffer[(size_t) index];
            const float output = -input + bufout;

            buffer[(size_t) index] = input + bufout * feedback;

            if (++index >= length)
                index = 0;

            return output;
        }
    };

    struct Tunings
    {
        float combScale        = 1.0f;
        float feedbackBase     = 0.80f;
        float feedbackRange    = 0.16f;
        float dampMin          = 0.15f;
        float dampMax          = 0.70f;
        float allpassFeedback  = 0.50f;
        float wetGain          = 1.00f;
        float highPassHz       = 130.0f;
        float lowPassMinHz     = 900.0f;
        float lowPassMaxHz     = 12000.0f;
        float modRateHz        = 0.0f;     // spring wobble
        float modDepthMs       = 0.0f;
        float bandFreqHz       = 1200.0f;  // spring tank resonance
        float bandGainDb       = 0.0f;
        float bandQ            = 1.0f;
        float preDelayMaxMs    = 120.0f;
    };

    //==========================================================================
    static Tunings getTunings (Mode) noexcept;
    void applyModeSettings() noexcept;
    void updateTunings (float decay, float tone) noexcept;

    //==========================================================================
    static constexpr float combMilliseconds[4]    = { 25.31f, 29.03f, 32.19f, 35.31f };
    static constexpr float stereoSpreadMs         = 0.52f;
    static constexpr float allpassMilliseconds[3] = { 12.61f, 10.00f, 7.73f };

    std::array<Comb, 8>    combs;      // 4 per channel
    std::array<Allpass, 6> allpasses;  // 3 per channel
    DelayLine preDelay;

    std::array<Biquad, 2> wetHighPass;
    std::array<Biquad, 2> wetLowPass;
    std::array<Biquad, 2> springBand;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> decaySmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> preDelaySmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> toneSmoothed;

    Tunings tunings = getTunings (Mode::Room);
    Mode mode = Mode::Room;

    float lastDecay = -1.0f;
    float lastTone  = -1.0f;
    float combInputGain = 0.03f;

    double sampleRate = 44100.0;
    int numChannels = 2;
    float modPhase = 0.0f;
    bool prepared = false;
};

} // namespace ampforge

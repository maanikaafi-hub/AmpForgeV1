/*  AmpForge - noise gate.
 *
 *  A single envelope (the loudest channel) drives the gate so that the stereo
 *  image is never pulled sideways while the gate is moving.
 *
 *  Threshold : -80 .. 0 dB
 *  Release   : 20 .. 800 ms
 *  Attack, hold time and hysteresis are internal: they are chosen so that the
 *  gate closes cleanly on single notes without chattering on decay tails.
 */
#pragma once

#include <JuceHeader.h>

namespace ampforge
{

class NoiseGate
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    void setEnabled      (bool shouldBeEnabled) noexcept;
    void setThresholdDb  (float thresholdDb) noexcept;
    void setReleaseMs    (float releaseMs) noexcept;

    /** In place, per sample.  Real-time safe. */
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    /** Gain reduction in dB (negative), for the meter. */
    float getGainReductionDb() const noexcept { return lastGainReductionDb.load(); }

    bool isEnabled() const noexcept { return enabled; }

private:
    double sampleRate = 44100.0;
    int numChannels = 2;

    std::atomic<bool>  enabled          { true };
    std::atomic<float> thresholdLinear  { 0.0025f };
    std::atomic<float> releaseCoeffVal  { 0.001f };
    std::atomic<float> lastGainReductionDb { 0.0f };

    float attackCoeff  = 0.02f;
    float releaseCoeff = 0.001f;
    float openThreshold  = 0.0025f;
    float closeThreshold = 0.0015f;

    float gateGain = 0.0f;
    int   holdCounter = 0;
    int   holdSamples = 0;
    bool  isOpen = false;
};

} // namespace ampforge

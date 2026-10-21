/*  AmpForge - level meters for the editor. */
#pragma once

#include <JuceHeader.h>

//==============================================================================
/** Vertical peak/RMS meter with peak hold.  Polls the processor's lock-free
 *  meter values from a 30 Hz timer; it never touches the audio thread. */
class MeterComponent : public juce::Component, private juce::Timer
{
public:
    using LevelSource = std::function<float (int channel)>;

    MeterComponent (const juce::String& titleToUse, LevelSource source, int numChannelsToUse);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    juce::String title;
    LevelSource getLevel;
    int numChannels;

    float displayed[2] = { 0.0f, 0.0f };
    float peakHold[2]  = { 0.0f, 0.0f };
    float clipGlow[2]  = { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterComponent)
};

//==============================================================================
/** Horizontal gain reduction bar (noise gate). */
class GainReductionMeter : public juce::Component, private juce::Timer
{
public:
    using Source = std::function<float()>;   // dB, 0 or negative

    GainReductionMeter (Source source);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { currentDb = getReductionDb(); repaint(); }

    Source getReductionDb;
    float currentDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainReductionMeter)
};

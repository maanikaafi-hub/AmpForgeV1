/*  AmpForge - processor.
 *
 *  Signal flow (per channel):
 *
 *      in -> input meter -> noise gate -> amp model (gain stages, tone stack,
 *      presence, master) -> cabinet simulation -> delay -> reverb -> DC block
 *      -> soft output limiter -> output meter -> out
 *
 *  Supported layouts: mono, stereo, mono->stereo and stereo->mono.
 *  Supported rates: 44.1, 48, 88.2, 96 kHz (and anything else the host uses).
 */
#pragma once

#include <JuceHeader.h>

#include "ParamIDs.h"
#include "DSP/Biquad.h"
#include "DSP/AmpModel.h"
#include "DSP/NoiseGate.h"
#include "DSP/ReverbEngine.h"
#include "DSP/DelayEngine.h"
#include "DSP/CabinetSim.h"
#include "DSP/LevelMeter.h"

//==============================================================================
class AmpForgeAudioProcessor : public juce::AudioProcessor
{
public:
    //==========================================================================
    AmpForgeAudioProcessor();
    ~AmpForgeAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    using juce::AudioProcessor::processBlock;   // keep the double precision overload visible
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==========================================================================
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override   { return false; }
    bool producesMidi() const override  { return false; }
    bool isMidiEffect() const override  { return false; }
    double getTailLengthSeconds() const override { return cachedTailSeconds.load(); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState& getPluginState() noexcept   { return apvts; }
    ampforge::CabinetSim&               getCabinet() noexcept       { return cabinet; }
    const ampforge::LevelMeter&         getInputMeter() const noexcept  { return inputMeter; }
    const ampforge::LevelMeter&         getOutputMeter() const noexcept { return outputMeter; }
    float                               getGateReductionDb() const noexcept { return gate.getGainReductionDb(); }

    /** Relative path of the VST3 bundle produced by the Release build.
     *  Kept here so the CI verification step and the tests agree on it. */
    static const char* getExpectedVST3Path() noexcept
    {
        return "build/AmpForge_artefacts/Release/VST3/AmpForge.vst3";
    }

private:
    //==========================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    double computeTailSeconds() const noexcept;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    ampforge::AmpModel     amp;
    ampforge::NoiseGate    gate;
    ampforge::DelayEngine  delay;
    ampforge::ReverbEngine reverb;
    ampforge::CabinetSim   cabinet;

    std::array<ampforge::Biquad, 2> outputDCBlocker;

    ampforge::LevelMeter inputMeter, outputMeter;

    std::atomic<double> cachedTailSeconds { 0.5 };

    int lastAmpType     = -1;
    int lastReverbMode  = -1;
    int lastDelayMode   = -1;
    int lastCabProfile  = -1;

    int outputChannels  = 2;

    bool prepared = false;

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpForgeAudioProcessor)
};

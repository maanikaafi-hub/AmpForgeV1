/*  AmpForge - processor (implementation). */
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    /** Transparent below the knee, smoothly limits above it, with a hard
     *  ceiling of just under 1.0 so the plug-in can never hand a clipped
     *  sample to the host. */
    constexpr float limiterKnee    = 0.85f;
    constexpr float limiterCeiling = 0.99f;
    constexpr float limiterRange   = limiterCeiling - limiterKnee;

    /** JUCE's FastMathApproximations::tanh() is a Pade approximant that is only
     *  valid for arguments between -5 and +5 (it grows without bound outside
     *  that range), so every call clamps its argument first. */
    inline float tanhClamped (float x) noexcept
    {
        return juce::dsp::FastMathApproximations::tanh (juce::jlimit (-4.0f, 4.0f, x));
    }

    inline float softLimit (float x) noexcept
    {
        const float a = std::abs (x);

        if (a <= limiterKnee)
            return x;

        const float excess  = juce::jmin (a - limiterKnee, 4.0f * limiterRange);
        const float limited = limiterKnee + limiterRange * tanhClamped (excess / limiterRange);

        return x < 0.0f ? -limited : limited;
    }
}

//==============================================================================
AmpForgeAudioProcessor::AmpForgeAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("AmpForgeParameters"), createParameterLayout())
{
    lastAmpType    = -1;
    lastReverbMode = -1;
    lastDelayMode  = -1;
    lastCabProfile = -1;
}

AmpForgeAudioProcessor::~AmpForgeAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AmpForgeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    using juce::NormalisableRange;
    using juce::ParameterID;

    auto add = [&params] (juce::RangedAudioParameter* p) { params.emplace_back (p); };

    // --- amp -----------------------------------------------------------------
    add (new juce::AudioParameterChoice (ParameterID { ampforge::id::ampModel, 1 }, "Amp Model",
                                         ampforge::choice::ampModels(), ampforge::def::ampModel));
    add (new juce::AudioParameterFloat  (ParameterID { ampforge::id::gain, 1 }, "Gain",
                                         NormalisableRange<float> (0.0f, 10.0f, 0.01f), ampforge::def::gain));
    add (new juce::AudioParameterFloat  (ParameterID { ampforge::id::bass, 1 }, "Bass",
                                         NormalisableRange<float> (-12.0f, 12.0f, 0.1f), ampforge::def::bass));
    add (new juce::AudioParameterFloat  (ParameterID { ampforge::id::mid, 1 }, "Mid",
                                         NormalisableRange<float> (-12.0f, 12.0f, 0.1f), ampforge::def::mid));
    add (new juce::AudioParameterFloat  (ParameterID { ampforge::id::treble, 1 }, "Treble",
                                         NormalisableRange<float> (-12.0f, 12.0f, 0.1f), ampforge::def::treble));
    add (new juce::AudioParameterFloat  (ParameterID { ampforge::id::presence, 1 }, "Presence",
                                         NormalisableRange<float> (-12.0f, 12.0f, 0.1f), ampforge::def::presence));
    add (new juce::AudioParameterFloat  (ParameterID { ampforge::id::master, 1 }, "Master",
                                         NormalisableRange<float> (-60.0f, 6.0f, 0.1f, 0.7f), ampforge::def::master));

    // --- gate ----------------------------------------------------------------
    add (new juce::AudioParameterBool   (ParameterID { ampforge::id::gateEnable, 1 }, "Gate",
                                         ampforge::def::gateEnable));
    add (new juce::AudioParameterFloat  (ParameterID { ampforge::id::gateThreshold, 1 }, "Gate Threshold",
                                         NormalisableRange<float> (-80.0f, 0.0f, 0.5f), ampforge::def::gateThreshold));
    add (new juce::AudioParameterFloat  (ParameterID { ampforge::id::gateRelease, 1 }, "Gate Release",
                                         NormalisableRange<float> (20.0f, 800.0f, 1.0f, 0.5f), ampforge::def::gateRelease));

    // --- cabinet -------------------------------------------------------------
    add (new juce::AudioParameterBool   (ParameterID { ampforge::id::cabEnable, 1 }, "Cabinet",
                                         ampforge::def::cabEnable));
    add (new juce::AudioParameterChoice (ParameterID { ampforge::id::cabProfile, 1 }, "Cab Profile",
                                         ampforge::choice::cabProfiles(), ampforge::def::cabProfile));

    // --- reverb --------------------------------------------------------------
    add (new juce::AudioParameterChoice (ParameterID { ampforge::id::reverbMode, 1 }, "Reverb Mode",
                                         ampforge::choice::reverbModes(), ampforge::def::reverbMode));
    add (new juce::AudioParameterFloat  (ParameterID { ampforge::id::reverbMix, 1 }, "Reverb Mix",
                                         NormalisableRange<float> (0.0f, 100.0f, 0.5f), ampforge::def::reverbMix));
    add (new juce::AudioParameterFloat  (ParameterID { ampforge::id::reverbDecay, 1 }, "Reverb Decay",
                                         NormalisableRange<float> (0.0f, 100.0f, 0.5f), ampforge::def::reverbDecay));
    add (new juce::AudioParameterFloat  (ParameterID { ampforge::id::reverbPreDelay, 1 }, "Reverb Pre-Delay",
                                         NormalisableRange<float> (0.0f, 120.0f, 0.5f), ampforge::def::reverbPreDelay));
    add (new juce::AudioParameterFloat  (ParameterID { ampforge::id::reverbTone, 1 }, "Reverb Tone",
                                         NormalisableRange<float> (0.0f, 100.0f, 0.5f), ampforge::def::reverbTone));

    // --- delay ---------------------------------------------------------------
    add (new juce::AudioParameterChoice (ParameterID { ampforge::id::delayMode, 1 }, "Delay Mode",
                                         ampforge::choice::delayModes(), ampforge::def::delayMode));
    add (new juce::AudioParameterFloat  (ParameterID { ampforge::id::delayTime, 1 }, "Delay Time",
                                         NormalisableRange<float> (10.0f, 1500.0f, 0.5f, 0.35f), ampforge::def::delayTime));
    add (new juce::AudioParameterFloat  (ParameterID { ampforge::id::delayFeedback, 1 }, "Delay Feedback",
                                         NormalisableRange<float> (0.0f, 95.0f, 0.5f), ampforge::def::delayFeedback));
    add (new juce::AudioParameterFloat  (ParameterID { ampforge::id::delayMix, 1 }, "Delay Mix",
                                         NormalisableRange<float> (0.0f, 100.0f, 0.5f), ampforge::def::delayMix));
    add (new juce::AudioParameterFloat  (ParameterID { ampforge::id::delayTone, 1 }, "Delay Tone",
                                         NormalisableRange<float> (0.0f, 100.0f, 0.5f), ampforge::def::delayTone));

    return { params.begin(), params.end() };
}

//==============================================================================
bool AmpForgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Only a single main input bus and a single main output bus, each mono or
    // stereo.  {1,1} {1,2} {2,1} {2,2} are all supported.
    const auto mainIn  = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    const bool mainInOk  = mainIn  == juce::AudioChannelSet::mono()  || mainIn  == juce::AudioChannelSet::stereo();
    const bool mainOutOk = mainOut == juce::AudioChannelSet::mono()  || mainOut == juce::AudioChannelSet::stereo();

    if (! mainInOk || ! mainOutOk)
        return false;

    for (int i = 1; i < layouts.inputBuses.size(); ++i)
        if (layouts.getNumChannels (true, i) > 0)
            return false;

    for (int i = 1; i < layouts.outputBuses.size(); ++i)
        if (layouts.getNumChannels (false, i) > 0)
            return false;

    return true;
}

//==============================================================================
void AmpForgeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    outputChannels = juce::jlimit (1, 2, getTotalNumOutputChannels());

    // The DSP is prepared for the widest bus we will ever see, so that a
    // mono->stereo or stereo->mono layout never feeds a module more channels
    // than it was prepared for.
    const int dspChannels = juce::jlimit (1, 2, juce::jmax (getTotalNumInputChannels(),
                                                            getTotalNumOutputChannels()));

    const juce::dsp::ProcessSpec spec { sampleRate,
                                        (juce::uint32) juce::jmax (1, samplesPerBlock),
                                        (juce::uint32) dspChannels };

    amp.prepare     (spec);
    gate.prepare    (spec);
    delay.prepare   (spec);
    reverb.prepare  (spec);
    cabinet.prepare (spec);

    for (auto& f : outputDCBlocker)
    {
        f.reset();
        f.setHighPass (sampleRate, 20.0, 0.7071);
    }

    inputMeter.prepare (outputChannels);
    outputMeter.prepare (outputChannels);

    // Force the switch based settings to be re-applied on the first block.
    lastAmpType    = -1;
    lastReverbMode = -1;
    lastDelayMode  = -1;
    lastCabProfile = -1;

    cachedTailSeconds.store (computeTailSeconds());
    prepared = true;
}

void AmpForgeAudioProcessor::releaseResources()
{
    amp.reset();
    gate.reset();
    delay.reset();
    reverb.reset();
    cabinet.reset();

    inputMeter.reset();
    outputMeter.reset();

    prepared = false;
}

//==============================================================================
double AmpForgeAudioProcessor::computeTailSeconds() const noexcept
{
    const float delayMixVal  = *apvts.getRawParameterValue (ampforge::id::delayMix);
    const float delayTimeVal = *apvts.getRawParameterValue (ampforge::id::delayTime);
    const float delayFbVal   = *apvts.getRawParameterValue (ampforge::id::delayFeedback);
    const float reverbMixVal = *apvts.getRawParameterValue (ampforge::id::reverbMix);
    const float reverbDecVal = *apvts.getRawParameterValue (ampforge::id::reverbDecay);

    double tail = 0.25;

    if (delayMixVal > 0.5f)
        tail += (double) delayTimeVal * 0.001 * (1.0 + 12.0 * (double) ampforge::convert::feedbackNorm (delayFbVal));

    if (reverbMixVal > 0.5f)
        tail += (double) ampforge::convert::decayToSeconds (reverbDecVal) * 2.0;

    return juce::jlimit (0.0, 30.0, tail);
}

//==============================================================================
void AmpForgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    // Flush denormals to zero for the whole block: no CPU spikes caused by
    // long tails decaying through the subnormal range.
    juce::ScopedNoDenormals noDenormals;

    if (! prepared)
        return;

    const int numSamples = buffer.getNumSamples();
    const int numIn      = getTotalNumInputChannels();
    const int numOut     = getTotalNumOutputChannels();
    const int numChans   = juce::jlimit (1, 2, numOut);

    if (numSamples <= 0)
        return;

    // Channels that exist in the buffer but carry no input must be silent.
    for (int ch = numIn; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // ---------------------------------------------------------------------
    // Parameter snapshot (one atomic load each, no locks)
    // ---------------------------------------------------------------------
    const int   ampTypeVal   = (int) *apvts.getRawParameterValue (ampforge::id::ampModel);
    const float gainVal      = *apvts.getRawParameterValue (ampforge::id::gain);
    const float bassVal      = *apvts.getRawParameterValue (ampforge::id::bass);
    const float midVal       = *apvts.getRawParameterValue (ampforge::id::mid);
    const float trebleVal    = *apvts.getRawParameterValue (ampforge::id::treble);
    const float presenceVal  = *apvts.getRawParameterValue (ampforge::id::presence);
    const float masterVal    = *apvts.getRawParameterValue (ampforge::id::master);

    const bool  gateOn       = *apvts.getRawParameterValue (ampforge::id::gateEnable) > 0.5f;
    const float gateThresh   = *apvts.getRawParameterValue (ampforge::id::gateThreshold);
    const float gateRelease  = *apvts.getRawParameterValue (ampforge::id::gateRelease);

    const bool  cabOn        = *apvts.getRawParameterValue (ampforge::id::cabEnable) > 0.5f;
    const int   cabProfile   = (int) *apvts.getRawParameterValue (ampforge::id::cabProfile);

    const int   revMode      = (int) *apvts.getRawParameterValue (ampforge::id::reverbMode);
    const float revMix       = *apvts.getRawParameterValue (ampforge::id::reverbMix);
    const float revDecay     = *apvts.getRawParameterValue (ampforge::id::reverbDecay);
    const float revPreDelay  = *apvts.getRawParameterValue (ampforge::id::reverbPreDelay);
    const float revTone      = *apvts.getRawParameterValue (ampforge::id::reverbTone);

    const int   delMode      = (int) *apvts.getRawParameterValue (ampforge::id::delayMode);
    const float delTime      = *apvts.getRawParameterValue (ampforge::id::delayTime);
    const float delFeedback  = *apvts.getRawParameterValue (ampforge::id::delayFeedback);
    const float delMix       = *apvts.getRawParameterValue (ampforge::id::delayMix);
    const float delTone      = *apvts.getRawParameterValue (ampforge::id::delayTone);

    // ---------------------------------------------------------------------
    // Discrete switches
    // ---------------------------------------------------------------------
    if (ampTypeVal != lastAmpType)
    {
        amp.setType ((ampforge::AmpType) ampTypeVal);
        lastAmpType = ampTypeVal;
    }

    if (revMode != lastReverbMode)
    {
        reverb.setMode ((ampforge::ReverbEngine::Mode) revMode);
        lastReverbMode = revMode;
    }

    if (delMode != lastDelayMode)
    {
        delay.setMode ((ampforge::DelayEngine::Mode) delMode);
        lastDelayMode = delMode;
    }

    if (cabProfile != lastCabProfile)
    {
        cabinet.requestProfile ((ampforge::CabinetSim::Profile) cabProfile);
        lastCabProfile = cabProfile;
    }

    // ---------------------------------------------------------------------
    // Continuous parameters (each one is smoothed internally)
    // ---------------------------------------------------------------------
    amp.setDrive    (ampforge::convert::driveNorm (gainVal));
    amp.setTone     (bassVal, midVal, trebleVal, presenceVal);
    amp.setMasterDb (masterVal);

    gate.setEnabled     (gateOn);
    gate.setThresholdDb (gateThresh);
    gate.setReleaseMs   (gateRelease);

    delay.setTimeMs   (delTime);
    delay.setFeedback (ampforge::convert::feedbackNorm (delFeedback));
    delay.setMix      (ampforge::convert::mixNorm (delMix));
    delay.setTone     (ampforge::convert::toneNorm (delTone));

    reverb.setMix        (ampforge::convert::mixNorm (revMix));
    reverb.setDecay      (juce::jlimit (0.0f, 1.0f, revDecay / 100.0f));
    reverb.setPreDelayMs (revPreDelay);
    reverb.setTone       (ampforge::convert::toneNorm (revTone));

    cabinet.setEnabled (cabOn);

    // ---------------------------------------------------------------------
    // Channel routing
    // ---------------------------------------------------------------------
    if (numIn == 1 && numOut == 2)
    {
        // mono in, stereo out: feed both amp channels with the same signal so
        // that the cabinet IR, the delay and the reverb still produce width.
        buffer.copyFrom (1, 0, buffer.getReadPointer (0), numSamples);
    }
    else if (numIn == 2 && numOut == 1 && buffer.getNumChannels() >= 2)
    {
        // stereo in, mono out: fold down before the amp
        buffer.addFrom (0, 0, buffer.getReadPointer (1), numSamples);
        juce::FloatVectorOperations::multiply (buffer.getWritePointer (0), 0.5f, numSamples);
    }

    inputMeter.push (buffer, juce::jmax (1, juce::jmin (numIn, numChans)));

    // ---------------------------------------------------------------------
    // Gate -> amp
    // ---------------------------------------------------------------------
    gate.process (buffer);

    {
        const auto channels = buffer.getArrayOfWritePointers();
        const int chans = juce::jmin (numChans, buffer.getNumChannels());

        for (int i = 0; i < numSamples; ++i)
        {
            amp.updateSmoothers();

            for (int ch = 0; ch < chans; ++ch)
                channels[ch][i] = amp.processSample (ch, channels[ch][i]);
        }
    }

    // ---------------------------------------------------------------------
    // Cabinet -> delay -> reverb
    // ---------------------------------------------------------------------
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    cabinet.process (context);
    delay.process  (buffer);
    reverb.process (buffer);

    // ---------------------------------------------------------------------
    // Output: DC block, safety limiter, metering
    // ---------------------------------------------------------------------
    {
        const int chans = juce::jmin (numChans, buffer.getNumChannels());

        for (int ch = 0; ch < chans; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            auto& dc   = outputDCBlocker[(size_t) ch];

            for (int i = 0; i < numSamples; ++i)
                data[i] = softLimit (dc.process (data[i]));
        }

        outputMeter.push (buffer, chans);
    }

    cachedTailSeconds.store (computeTailSeconds());
}

//==============================================================================
void AmpForgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();

    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, destData);
}

void AmpForgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = juce::AudioProcessor::getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

//==============================================================================
juce::AudioProcessorEditor* AmpForgeAudioProcessor::createEditor()
{
    return new AmpForgeAudioProcessorEditor (*this);
}

//==============================================================================
// This creates new instances of the plug-in. Every JUCE plug-in wrapper (the
// VST3 client included) calls this, so it has to exist exactly once in the
// plug-in target and must be outside any namespace.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AmpForgeAudioProcessor();
}

/*  AmpForge - automated validation.
 *
 *  What these tests DO prove:
 *    * every parameter exists, has the documented default and survives a
 *      save/restore round trip;
 *    * processing produces finite, bounded values at 44.1 / 48 / 88.2 / 96 kHz
 *      for sine, silence, impulse and extreme setting input;
 *    * silence produces silence and an impulse produces a decaying tail;
 *    * every mono / stereo bus layout the plug-in advertises can be processed;
 *    * every source file the build needs is present.
 *
 *  What they do NOT prove: anything about how it sounds, and nothing about
 *  behaviour inside a DAW.  A green test run is not a substitute for listening
 *  to the plug-in in Cubase.
 */
#include <JuceHeader.h>

#include "../Source/PluginProcessor.h"

using namespace ampforge;

namespace
{
    const juce::File sourceDirectory { AMPFORGE_SOURCE_DIR };

    //==========================================================================
    void setNormalised (AmpForgeAudioProcessor& processor, const juce::String& parameterID, float value)
    {
        if (auto* p = processor.getPluginState().getParameter (parameterID))
            p->setValueNotifyingHost (value);
    }

    float normalised (AmpForgeAudioProcessor& processor, const juce::String& parameterID)
    {
        if (auto* p = processor.getPluginState().getParameter (parameterID))
            return p->getValue();

        return -1.0f;
    }

    bool bufferIsFinite (const juce::AudioBuffer<float>& buffer, int numChannels, int numSamples)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* data = buffer.getReadPointer (ch);

            for (int i = 0; i < numSamples; ++i)
                if (! std::isfinite (data[i]))
                    return false;
        }

        return true;
    }

    float bufferMaxAbs (const juce::AudioBuffer<float>& buffer, int numChannels, int numSamples)
    {
        float max = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* data = buffer.getReadPointer (ch);

            for (int i = 0; i < numSamples; ++i)
                max = juce::jmax (max, std::abs (data[i]));
        }

        return max;
    }

    void fillSine (juce::AudioBuffer<float>& buffer, double sampleRate, double frequency, float amplitude)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                data[i] = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * frequency
                                                        * (double) i / sampleRate);
        }
    }
}

//==============================================================================
class ParameterTests : public juce::UnitTest
{
public:
    ParameterTests() : juce::UnitTest ("AmpForge parameters and state", "ampforge") {}

    void runTest() override
    {
        //----------------------------------------------------------------------
        beginTest ("every parameter exists");

        AmpForgeAudioProcessor processor;

        expectEquals ((int) processor.getParameters().size(), 22, "unexpected number of parameters");

        const juce::StringArray required =
        {
            id::ampModel, id::gain, id::bass, id::mid, id::treble, id::presence, id::master,
            id::gateEnable, id::gateThreshold, id::gateRelease,
            id::cabEnable, id::cabProfile,
            id::reverbMode, id::reverbMix, id::reverbDecay, id::reverbPreDelay, id::reverbTone,
            id::delayMode, id::delayTime, id::delayFeedback, id::delayMix, id::delayTone
        };

        for (auto& parameterID : required)
            expect (processor.getPluginState().getParameter (parameterID) != nullptr,
                    "missing parameter: " + parameterID);

        //----------------------------------------------------------------------
        beginTest ("default values");

        expectEquals ((int) (float) *processor.getPluginState().getRawParameterValue (id::ampModel), def::ampModel, "ampModel");
        expectWithinAbsoluteError ((float) *processor.getPluginState().getRawParameterValue (id::gain), def::gain, 0.001f, "gain");
        expectWithinAbsoluteError ((float) *processor.getPluginState().getRawParameterValue (id::bass), def::bass, 0.001f, "bass");
        expectWithinAbsoluteError ((float) *processor.getPluginState().getRawParameterValue (id::mid), def::mid, 0.001f, "mid");
        expectWithinAbsoluteError ((float) *processor.getPluginState().getRawParameterValue (id::treble), def::treble, 0.001f, "treble");
        expectWithinAbsoluteError ((float) *processor.getPluginState().getRawParameterValue (id::presence), def::presence, 0.001f, "presence");
        expectWithinAbsoluteError ((float) *processor.getPluginState().getRawParameterValue (id::master), def::master, 0.001f, "master");
        expectWithinAbsoluteError ((float) *processor.getPluginState().getRawParameterValue (id::gateThreshold), def::gateThreshold, 0.001f, "gateThreshold");
        expectWithinAbsoluteError ((float) *processor.getPluginState().getRawParameterValue (id::delayTime), def::delayTime, 0.001f, "delayTime");
        expectWithinAbsoluteError ((float) *processor.getPluginState().getRawParameterValue (id::reverbMix), def::reverbMix, 0.001f, "reverbMix");

        //----------------------------------------------------------------------
        beginTest ("state save / restore round trip");

        for (auto* p : processor.getParameters())
        {
            // booleans can only hold 0 or 1, so flip them instead
            if (p->isBoolean())
                p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
            else
                p->setValueNotifyingHost (0.63f);
        }

        juce::MemoryBlock state;
        processor.getStateInformation (state);

        expect (state.getSize() > 0, "state should not be empty");

        AmpForgeAudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());

        for (auto* p : processor.getParameters())
        {
            const auto* withID = dynamic_cast<const juce::AudioProcessorParameterWithID*> (p);

            if (withID == nullptr)
            {
                expect (false, "a parameter has no ID");
                continue;
            }

            const auto parameterID = withID->getParameterID();
            auto* q = restored.getPluginState().getParameter (parameterID);

            expect (q != nullptr, "parameter missing after restore: " + parameterID);

            if (q != nullptr)
                expectWithinAbsoluteError (q->getValue(), p->getValue(), 0.002f,
                                           "value mismatch after restore: " + parameterID);
        }

        //----------------------------------------------------------------------
        beginTest ("a corrupt state block must not disturb the defaults");

        AmpForgeAudioProcessor untouched;
        const char garbage[64] = { 'n', 'o', 't', ' ', 'a', ' ', 's', 't', 'a', 't', 'e' };
        untouched.setStateInformation (garbage, (int) sizeof (garbage));

        expectWithinAbsoluteError ((float) *untouched.getPluginState().getRawParameterValue (id::gain),
                                   def::gain, 0.001f, "gain should still be at its default");
    }
};

//==============================================================================
class ProcessingTests : public juce::UnitTest
{
public:
    ProcessingTests() : juce::UnitTest ("AmpForge audio processing", "ampforge") {}

    void runTest() override
    {
        const double sampleRates[] = { 44100.0, 48000.0, 88200.0, 96000.0 };

        //----------------------------------------------------------------------
        for (auto sampleRate : sampleRates)
        {
            beginTest ("finite output at " + juce::String (sampleRate) + " Hz");

            AmpForgeAudioProcessor processor;
            processor.prepareToPlay (sampleRate, 512);

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;

            for (int block = 0; block < 24; ++block)
            {
                fillSine (buffer, sampleRate, 220.0, 0.25f);
                processor.processBlock (buffer, midi);
            }

            expect (bufferIsFinite (buffer, 2, buffer.getNumSamples()), "output must be finite");

            const auto peak = bufferMaxAbs (buffer, 2, buffer.getNumSamples());
            expectGreaterThan (peak, 0.0005f, "output must not be silent");
            expectLessOrEqual (peak, 1.0f, "output must not exceed the limiter ceiling");
        }

        //----------------------------------------------------------------------
        beginTest ("silence in produces silence out");

        {
            AmpForgeAudioProcessor processor;
            processor.prepareToPlay (48000.0, 512);

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;

            buffer.clear();

            for (int block = 0; block < 16; ++block)
                processor.processBlock (buffer, midi);

            expect (bufferIsFinite (buffer, 2, buffer.getNumSamples()), "silence must stay finite");
            expectLessThan (bufferMaxAbs (buffer, 2, buffer.getNumSamples()), 1.0e-6f, "silence must stay silent");
        }

        //----------------------------------------------------------------------
        beginTest ("impulse produces a decaying tail");

        {
            AmpForgeAudioProcessor processor;
            processor.prepareToPlay (48000.0, 256);

            juce::AudioBuffer<float> buffer (2, 256);
            juce::MidiBuffer midi;

            buffer.clear();
            buffer.setSample (0, 0, 1.0f);

            processor.processBlock (buffer, midi);

            expect (bufferIsFinite (buffer, 2, 256), "impulse response must be finite");
            expect (std::abs (buffer.getSample (0, 0)) > 0.0f, "the impulse must produce output");

            // five seconds of silence: every tail (delay and reverb included)
            // must have decayed away
            buffer.clear();

            float loudestTail = 0.0f;
            const int blocks = (int) (5.0 * 48000.0 / 256.0);

            for (int block = 0; block < blocks; ++block)
            {
                // processBlock() is in place, so the buffer must be re-cleared:
                // otherwise the output of one block would be fed back in as the
                // input of the next one.
                buffer.clear();
                processor.processBlock (buffer, midi);
                loudestTail = juce::jmax (loudestTail, bufferMaxAbs (buffer, 2, 256));
            }

            expectLessThan (loudestTail, 5.0e-3f, "the tail must decay");
        }

        //----------------------------------------------------------------------
        beginTest ("extreme settings stay finite and bounded");

        {
            AmpForgeAudioProcessor processor;
            processor.prepareToPlay (48000.0, 256);

            setNormalised (processor, id::gain,         1.0f);
            setNormalised (processor, id::master,       1.0f);
            setNormalised (processor, id::bass,         1.0f);
            setNormalised (processor, id::mid,          1.0f);
            setNormalised (processor, id::treble,       1.0f);
            setNormalised (processor, id::presence,     1.0f);
            setNormalised (processor, id::delayMix,     1.0f);
            setNormalised (processor, id::delayFeedback, 1.0f);
            setNormalised (processor, id::reverbMix,    1.0f);
            setNormalised (processor, id::reverbDecay,  1.0f);
            setNormalised (processor, id::gateEnable,   0.0f);

            juce::AudioBuffer<float> buffer (2, 256);
            juce::MidiBuffer midi;

            juce::Random random (1234);

            for (int block = 0; block < 600; ++block)   // ~3.2 seconds
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    auto* data = buffer.getWritePointer (ch);

                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                        data[i] = random.nextFloat() * 2.0f - 1.0f;
                }

                processor.processBlock (buffer, midi);

                if (! bufferIsFinite (buffer, 2, buffer.getNumSamples()))
                {
                    expect (false, "non finite sample at block " + juce::String (block));
                    break;
                }

                if (bufferMaxAbs (buffer, 2, buffer.getNumSamples()) > 1.001f)
                {
                    expect (false, "output exceeded the limiter ceiling at block " + juce::String (block));
                    break;
                }
            }

            expect (bufferIsFinite (buffer, 2, buffer.getNumSamples()), "extreme settings must stay finite");
        }

        //----------------------------------------------------------------------
        beginTest ("gain staging is usable and there is no DC offset");

        {
            // A 220 Hz sine at -14 dBFS stands in for a fairly hot guitar pickup.
            // Playing it through the default settings must produce something
            // clearly audible but not pinned to the limiter, and the chain must
            // not develop a DC offset.
            constexpr double fs      = 48000.0;
            constexpr int    blockSz = 256;
            constexpr float  freq    = 220.0f;
            constexpr float  level   = 0.2f;

            const int models[] = { 0, 1, 2 };   // American Clean, British Crunch, Modern High Gain
            const char* names[] = { "american clean", "british crunch", "modern high gain" };

            for (int m = 0; m < 3; ++m)
            {
                AmpForgeAudioProcessor processor;
                processor.prepareToPlay (fs, blockSz);
                setNormalised (processor, id::ampModel, (float) models[m] / 2.0f);

                juce::AudioBuffer<float> buffer (2, blockSz);
                juce::MidiBuffer midi;

                double phase = 0.0;
                const double phaseStep = 2.0 * juce::MathConstants<double>::pi * (double) freq / fs;

                float peak = 0.0f;
                double sum = 0.0;
                int counted = 0;

                const int blocks = (int) (2.0 * fs / (double) blockSz);   // two seconds

                for (int block = 0; block < blocks; ++block)
                {
                    for (int i = 0; i < blockSz; ++i)
                    {
                        const float v = level * (float) std::sin (phase);
                        phase += phaseStep;
                        buffer.setSample (0, i, v);
                        buffer.setSample (1, i, v);
                    }

                    processor.processBlock (buffer, midi);

                    // ignore the first half second: gain smoothing and the
                    // cabinet pre-delay are still settling there
                    if (block < (int) (0.5 * fs / (double) blockSz))
                        continue;

                    for (int i = 0; i < blockSz; ++i)
                    {
                        peak = juce::jmax (peak, std::abs (buffer.getSample (0, i)));
                        sum  += (double) buffer.getSample (0, i);
                        ++counted;
                    }
                }

                const float dc = (float) (sum / (double) counted);

                Logger::writeToLog (juce::String (names[m]) + ": peak = " + juce::String (peak, 4)
                                    + ", dc = " + juce::String (dc, 8));

                expectGreaterThan (peak, 0.01f,
                                   juce::String (names[m]) + " must produce a usable output level");

                expectLessThan (peak, 1.001f,
                                juce::String (names[m]) + " must not exceed the limiter ceiling");

                expectLessThan (std::abs (dc), 1.0e-3f,
                                juce::String (names[m]) + " must not develop a DC offset");
            }
        }
    }
};

//==============================================================================
class BusLayoutTests : public juce::UnitTest
{
public:
    BusLayoutTests() : juce::UnitTest ("AmpForge bus layouts", "ampforge") {}

    void runTest() override
    {
        struct LayoutCase
        {
            const char* name;
            juce::AudioChannelSet input;
            juce::AudioChannelSet output;
        };

        const LayoutCase cases[] =
        {
            { "mono -> mono",   juce::AudioChannelSet::mono(),   juce::AudioChannelSet::mono() },
            { "mono -> stereo", juce::AudioChannelSet::mono(),   juce::AudioChannelSet::stereo() },
            { "stereo -> mono", juce::AudioChannelSet::stereo(), juce::AudioChannelSet::mono() },
            { "stereo -> stereo", juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo() }
        };

        for (auto& c : cases)
        {
            beginTest (juce::String (c.name) + " layout");

            AmpForgeAudioProcessor processor;

            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add  (c.input);
            layout.outputBuses.add (c.output);

            expect (processor.isBusesLayoutSupported (layout), juce::String (c.name) + " must be supported");
            expect (processor.setBusesLayout (layout), juce::String (c.name) + " must be applicable");

            processor.prepareToPlay (48000.0, 256);

            juce::AudioBuffer<float> buffer (juce::jmax (c.input.size(), c.output.size()), 256);
            juce::MidiBuffer midi;

            fillSine (buffer, 48000.0, 220.0, 0.25f);
            processor.processBlock (buffer, midi);

            const int numOut = juce::jmax (1, c.output.size());

            expect (bufferIsFinite (buffer, numOut, 256), juce::String (c.name) + " output must be finite");
            expectGreaterThan (bufferMaxAbs (buffer, numOut, 256), 0.0f, juce::String (c.name) + " must produce output");
        }
    }
};

//==============================================================================
class RepositoryTests : public juce::UnitTest
{
public:
    RepositoryTests() : juce::UnitTest ("AmpForge repository files", "ampforge") {}

    void runTest() override
    {
        beginTest ("all expected source files exist");

        static const char* requiredFiles[] =
        {
            "CMakeLists.txt",
            "README.md",
            "LICENSE",
            "Source/PluginProcessor.h",
            "Source/PluginProcessor.cpp",
            "Source/PluginEditor.h",
            "Source/PluginEditor.cpp",
            "Source/ParamIDs.h",
            "Source/DSP/AmpModel.h",
            "Source/DSP/AmpModel.cpp",
            "Source/DSP/NoiseGate.h",
            "Source/DSP/NoiseGate.cpp",
            "Source/DSP/ReverbEngine.h",
            "Source/DSP/ReverbEngine.cpp",
            "Source/DSP/DelayEngine.h",
            "Source/DSP/DelayEngine.cpp",
            "Source/DSP/CabinetSim.h",
            "Source/DSP/CabinetSim.cpp",
            "Source/DSP/LevelMeter.h",
            "Source/DSP/LevelMeter.cpp",
            "Source/DSP/Biquad.h",
            "Source/DSP/DelayLine.h",
            "Source/UI/AmpForgeLookAndFeel.h",
            "Source/UI/AmpForgeLookAndFeel.cpp",
            "Source/UI/MeterComponent.h",
            "Source/UI/MeterComponent.cpp",
            "Source/UI/OptionSelector.h",
            ".github/workflows/build.yml",
            "Tests/AmpForgeTests.cpp",
            "Tests/CMakeLists.txt"
        };

        for (auto* relativePath : requiredFiles)
        {
            const auto file = sourceDirectory.getChildFile (juce::String (relativePath));
            expect (file.existsAsFile(), "missing file: " + juce::String (relativePath));
        }

        beginTest ("the documented artefact path is stable");

        expect (juce::String (AmpForgeAudioProcessor::getExpectedVST3Path())
                  == "build/AmpForge_artefacts/Release/VST3/AmpForge.vst3",
                "the workflow, the README and the tests must agree on the artefact path");
    }
};


//==============================================================================
static ParameterTests   parameterTests;
static ProcessingTests  processingTests;
static BusLayoutTests   busLayoutTests;
static RepositoryTests  repositoryTests;

//==============================================================================
int main (int argc, char** argv)
{
    juce::ignoreUnused (argc, argv);

    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    juce::UnitTestRunner runner;
    runner.runAllTests();

    int failures = 0;

    for (int i = 0; i < runner.getNumResults(); ++i)
        if (auto* result = runner.getResult (i))
            failures += result->failures;

    juce::Logger::writeToLog ("AmpForge tests finished with " + juce::String (failures) + " failure(s).");

    return failures == 0 ? 0 : 1;
}

/*  AmpForge - editor.
 *
 *  Layout:
 *    +--------------------------------------------------------------+
 *    |  AMPFORGE badge        |  amp model selector   |  power lamp  |
 *    +--------------------------------------------------------------+
 *    |  AMPLIFIER (6 knobs)   | LEVELS | CABINET                     |
 *    +--------------------------------------------------------------+
 *    | NOISE GATE   |  REVERB (mode + 4 knobs) | DELAY (mode + 4)    |
 *    +--------------------------------------------------------------+
 */
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/AmpForgeLookAndFeel.h"
#include "UI/MeterComponent.h"
#include "UI/OptionSelector.h"

//==============================================================================
class AmpForgeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit AmpForgeAudioProcessorEditor (AmpForgeAudioProcessor&);
    ~AmpForgeAudioProcessorEditor() override;

    //==========================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    //==========================================================================
    struct KnobControl
    {
        juce::Slider slider;
        juce::Label  nameLabel;
        juce::Label  valueLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        std::function<juce::String (float)> formatValue;
    };

    KnobControl& addKnob (const juce::String& parameterID, const juce::String& name,
                          std::function<juce::String (float)> formatter);

    static void layoutGrid (juce::Rectangle<int> area, const std::vector<KnobControl*>& items, int columns);
    void layoutKnobs();
    void drawHeader (juce::Graphics&, juce::Rectangle<int>);
    void showIRChooser();

    void timerCallback() override;

    //==========================================================================
    AmpForgeAudioProcessor& processor;
    AmpForgeLookAndFeel lookAndFeel;

    std::vector<std::unique_ptr<KnobControl>> knobs;
    std::vector<KnobControl*> ampKnobs, gateKnobs, reverbKnobs, delayKnobs;

    std::unique_ptr<OptionSelector>  ampSelector, cabSelector, reverbSelector, delaySelector;
    std::unique_ptr<MeterComponent>  inputMeter, outputMeter;
    std::unique_ptr<GainReductionMeter> gateMeter;

    std::unique_ptr<juce::TextButton> gateButton, cabButton, loadIRButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> gateAttachment, cabAttachment;

    juce::Label irNameLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::Rectangle<int> headerBounds;
    juce::Rectangle<int> ampPanel, meterPanel, cabPanel;
    juce::Rectangle<int> gatePanel, reverbPanel, delayPanel;

    int lastLoadCounter = -1;

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpForgeAudioProcessorEditor)
};

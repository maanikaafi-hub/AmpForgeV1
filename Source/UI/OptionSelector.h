/*  AmpForge - hardware style selector buttons for a choice parameter.
 *
 *  A row of mutually exclusive buttons that stay in sync with host automation:
 *  clicks write to the parameter, and the parameter value (polled at 15 Hz)
 *  drives the lit state.  Header only so it can be dropped into any panel.
 */
#pragma once

#include <JuceHeader.h>

class OptionSelector : public juce::Component, private juce::Timer
{
public:
    OptionSelector (juce::AudioProcessorValueTreeState& stateToUse,
                    const juce::String& parameterID,
                    const juce::StringArray& options)
        : state (stateToUse), paramID (parameterID)
    {
        for (int i = 0; i < options.size(); ++i)
        {
            auto* b = buttons.add (new juce::TextButton (options[i]));
            b->setClickingTogglesState (false);
            b->onClick = [this, i] { select (i); };
            addAndMakeVisible (b);
        }

        startTimerHz (15);
        updateButtonStates();
    }

    ~OptionSelector() override { stopTimer(); }

    /** Optional veto: return false to swallow the click (used to open the
     *  impulse response browser instead of switching cabinet profile). */
    std::function<bool (int index)> beforeSelect;

    void resized() override
    {
        juce::FlexBox fb;
        fb.justifyContent = juce::FlexBox::JustifyContent::center;
        fb.alignContent   = juce::FlexBox::AlignContent::stretch;

        for (auto* b : buttons)
            fb.items.add (juce::FlexItem (*b).withFlex (1.0f, 1.0f).withMargin ({ 0.0f, 2.0f, 0.0f, 2.0f }));

        fb.performLayout (getLocalBounds());
    }

private:
    void select (int index)
    {
        if (beforeSelect != nullptr && ! beforeSelect (index))
            return;

        if (auto* p = state.getParameter (paramID))
        {
            const float normalised = p->getNormalisableRange().convertTo0to1 ((float) index);
            p->setValueNotifyingHost (normalised);
        }

        updateButtonStates();
    }

    void timerCallback() override { updateButtonStates(); }

    void updateButtonStates()
    {
        int current = -1;

        if (auto* p = state.getParameter (paramID))
            current = juce::roundToInt (p->getNormalisableRange().convertFrom0to1 (p->getValue()));

        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == current, juce::dontSendNotification);
    }

    juce::AudioProcessorValueTreeState& state;
    juce::String paramID;
    juce::OwnedArray<juce::TextButton> buttons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptionSelector)
};

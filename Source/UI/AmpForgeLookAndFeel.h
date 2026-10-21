/*  AmpForge - custom look and feel.
 *
 *  Original hardware inspired styling: dark anodised panels, knurled metal
 *  knobs with a pointer, amber indicator arcs and pilot lamps.
 */
#pragma once

#include <JuceHeader.h>

class AmpForgeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AmpForgeLookAndFeel();

    //--------------------------------------------------------------------------
    static juce::Colour chassisColour()  noexcept { return juce::Colour (0xff191b1f); }
    static juce::Colour panelColour()    noexcept { return juce::Colour (0xff22252b); }
    static juce::Colour panelTop()       noexcept { return juce::Colour (0xff2b2f36); }
    static juce::Colour panelEdge()      noexcept { return juce::Colour (0xff0d0e10); }
    static juce::Colour insetColour()    noexcept { return juce::Colour (0xff0f1114); }
    static juce::Colour textColour()     noexcept { return juce::Colour (0xffe7ecf3); }
    static juce::Colour labelColour()    noexcept { return juce::Colour (0xff98a2b2); }
    static juce::Colour accentColour()   noexcept { return juce::Colour (0xffffa03c); }
    static juce::Colour accentDim()      noexcept { return juce::Colour (0xff5a3a17); }
    static juce::Colour ledColour()      noexcept { return juce::Colour (0xffff5a2b); }
    static juce::Colour ledOffColour()   noexcept { return juce::Colour (0xff3d2a24); }

    //--------------------------------------------------------------------------
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&, bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    void drawLabel (juce::Graphics&, juce::Label&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    //--------------------------------------------------------------------------
    /** Panel drawing helper used by the editor. */
    static void drawPanel (juce::Graphics&, juce::Rectangle<int> bounds,
                           const juce::String& title, bool drawScrews = true);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpForgeLookAndFeel)
};

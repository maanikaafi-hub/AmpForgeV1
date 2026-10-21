/*  AmpForge - custom look and feel (implementation). */
#include "AmpForgeLookAndFeel.h"

//==============================================================================
AmpForgeLookAndFeel::AmpForgeLookAndFeel()
{
    setColour (juce::Slider::rotarySliderFillColourId,   accentColour());
    setColour (juce::Slider::textBoxTextColourId,        textColour());
    setColour (juce::Label::textColourId,                textColour());
    setColour (juce::TextButton::buttonColourId,         juce::Colour (0xff2c3037));
    setColour (juce::TextButton::buttonOnColourId,       accentColour());
    setColour (juce::TextButton::textColourOffId,        labelColour());
    setColour (juce::TextButton::textColourOnId,         juce::Colour (0xff20160a));
    setColour (juce::ComboBox::backgroundColourId,       insetColour());
    setColour (juce::ComboBox::textColourId,             textColour());
    setColour (juce::PopupMenu::backgroundColourId,      juce::Colour (0xff1b1d21));
    setColour (juce::PopupMenu::textColourId,            textColour());
    setColour (juce::TooltipWindow::backgroundColourId,  juce::Colour (0xff1b1d21));
    setColour (juce::TooltipWindow::textColourId,        textColour());
}

//==============================================================================
void AmpForgeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float startAngle, float endAngle,
                                            juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);

    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre  = bounds.getCentre();
    const float angle  = startAngle + sliderPos * (endAngle - startAngle);

    // --- outer bezel ---------------------------------------------------------
    juce::ColourGradient bezel (juce::Colours::white.withAlpha (0.18f), centre.x, bounds.getY(),
                                juce::Colour (0xff090a0c), centre.x, bounds.getBottom(), false);
    g.setGradientFill (bezel);
    g.fillEllipse (bounds);

    g.setColour (panelEdge());
    g.drawEllipse (bounds, 1.0f);

    // --- track and value arc -------------------------------------------------
    const float arcRadius = radius - bounds.getWidth() * 0.055f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff33373f));
    g.strokePath (track, juce::PathStrokeType (bounds.getWidth() * 0.055f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    if (sliderPos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
        g.setColour (slider.isEnabled() ? accentColour() : labelColour());
        g.strokePath (value, juce::PathStrokeType (bounds.getWidth() * 0.055f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // --- knurled body --------------------------------------------------------
    const auto body = bounds.reduced (bounds.getWidth() * 0.16f);

    juce::ColourGradient bodyFill (juce::Colour (0xff4a4f58), centre.x, body.getY(),
                                   juce::Colour (0xff14161a), centre.x, body.getBottom(), false);
    g.setGradientFill (bodyFill);
    g.fillEllipse (body);

    g.setColour (juce::Colour (0xff0c0d0f));
    g.drawEllipse (body, 1.2f);

    const float bodyRadius = body.getWidth() * 0.5f;

    g.setColour (juce::Colours::black.withAlpha (0.35f));

    for (int i = 0; i < 28; ++i)
    {
        const float a = (float) i / 28.0f * juce::MathConstants<float>::twoPi;
        const float c = std::cos (a), s = std::sin (a);

        juce::Line<float> line (centre.x + c * bodyRadius * 0.74f, centre.y + s * bodyRadius * 0.74f,
                                centre.x + c * bodyRadius * 0.98f, centre.y + s * bodyRadius * 0.98f);

        g.drawLine (line, 1.1f);
    }

    // --- pointer -------------------------------------------------------------
    juce::Path pointer;
    const float pointerWidth  = bodyRadius * 0.20f;
    const float pointerLength = bodyRadius * 0.92f;

    pointer.addRoundedRectangle (-pointerWidth * 0.5f, -pointerLength,
                                 pointerWidth, pointerLength * 0.80f, pointerWidth * 0.45f);

    g.setColour (juce::Colour (0xfff2f5fa));
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));

    // --- centre cap ----------------------------------------------------------
    const float capRadius = bodyRadius * 0.24f;
    g.setColour (juce::Colour (0xff1c1f24));
    g.fillEllipse (centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawEllipse (centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.0f);
}

//==============================================================================
void AmpForgeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const float corner = juce::jmin (5.0f, bounds.getHeight() * 0.25f);

    const bool on = button.getToggleState();

    juce::Colour base = backgroundColour;

    if (on)
        base = base.interpolatedWith (accentColour(), 0.85f);
    else if (shouldDrawButtonAsDown)
        base = base.brighter (0.15f);
    else if (shouldDrawButtonAsHighlighted)
        base = base.brighter (0.08f);

    juce::ColourGradient fill (base.brighter (0.22f), bounds.getCentreX(), bounds.getY(),
                               base.darker (0.45f), bounds.getCentreX(), bounds.getBottom(), false);

    g.setGradientFill (fill);
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.drawRoundedRectangle (bounds, corner, 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), corner, 1.0f);

    if (on)
    {
        // pilot lamp
        const float lampSize = juce::jmin (bounds.getHeight() * 0.30f, 7.0f);
        const auto lamp = juce::Rectangle<float> (lampSize, lampSize)
                              .withCentre (juce::Point<float> (bounds.getX() + lampSize * 1.6f, bounds.getCentreY()));

        g.setColour (ledColour().withAlpha (0.35f));
        g.fillEllipse (lamp.expanded (2.0f));

        g.setColour (ledColour());
        g.fillEllipse (lamp);
    }
}

void AmpForgeLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                          bool, bool shouldDrawButtonAsDown)
{
    const bool on = button.getToggleState();

    auto font = getTextButtonFont (button, button.getHeight());
    g.setFont (font);

    g.setColour (on ? juce::Colour (0xff20160a)
                    : (button.isEnabled() ? labelColour() : labelColour().withAlpha (0.5f)));

    const int indent = juce::jmin (10, button.proportionOfWidth (0.25f));

    g.drawFittedText (button.getButtonText(),
                      indent + (on ? 8 : 0),
                      shouldDrawButtonAsDown ? 1 : 0,
                      button.getWidth() - indent * 2,
                      button.getHeight(),
                      juce::Justification::centred, 1);
}

//==============================================================================
void AmpForgeLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    if (! label.isBeingEdited())
    {
        auto alpha = label.isEnabled() ? 1.0f : 0.4f;
        const auto textArea = label.getLocalBounds();

        g.setFont (getLabelFont (label));
        g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));

        g.drawFittedText (label.getText(), textArea, label.getJustificationType(), 1, 1.0f);
    }
}

juce::Font AmpForgeLookAndFeel::getLabelFont (juce::Label& label)
{
    return juce::Font (juce::FontOptions (juce::jmin (15.0f, label.getHeight() * 0.72f)));
}

juce::Font AmpForgeLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (juce::jmin (15.0f, (float) buttonHeight * 0.52f), juce::Font::bold));
}

//==============================================================================
void AmpForgeLookAndFeel::drawPanel (juce::Graphics& g, juce::Rectangle<int> bounds,
                                     const juce::String& title, bool drawScrews)
{
    if (bounds.isEmpty())
        return;

    const auto area = bounds.toFloat();
    const float corner = 8.0f;

    // --- body ----------------------------------------------------------------
    juce::ColourGradient fill (panelTop(), area.getCentreX(), area.getY(),
                               panelColour().darker (0.35f), area.getCentreX(), area.getBottom(), false);

    g.setGradientFill (fill);
    g.fillRoundedRectangle (area, corner);

    g.setColour (panelEdge());
    g.drawRoundedRectangle (area, corner, 1.2f);

    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (area.reduced (1.5f), corner, 1.0f);

    // brushed metal streaks
    g.setColour (juce::Colours::white.withAlpha (0.018f));

    for (int x = bounds.getX() + 6; x < bounds.getRight() - 6; x += 4)
        g.drawVerticalLine (x, (float) bounds.getY() + 2.0f, (float) bounds.getBottom() - 2.0f);

    // --- title --------------------------------------------------------------
    if (title.isNotEmpty())
    {
        g.setColour (labelColour());
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawText (title.toUpperCase(), bounds.reduced (12, 4).removeFromTop (16),
                    juce::Justification::centredLeft);

        g.setColour (accentDim());
        g.fillRect (bounds.getX() + 12, bounds.getY() + 21, bounds.getWidth() - 24, 1);
    }

    // --- screws --------------------------------------------------------------
    if (drawScrews)
    {
        const float r = 3.0f;

        for (auto p : { juce::Point<float> ((float) bounds.getX() + 8.0f, (float) bounds.getY() + 8.0f),
                        juce::Point<float> ((float) bounds.getRight() - 8.0f, (float) bounds.getY() + 8.0f),
                        juce::Point<float> ((float) bounds.getX() + 8.0f, (float) bounds.getBottom() - 8.0f),
                        juce::Point<float> ((float) bounds.getRight() - 8.0f, (float) bounds.getBottom() - 8.0f) })
        {
            g.setColour (juce::Colour (0xff54595f));
            g.fillEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f);

            g.setColour (juce::Colour (0xff0c0d0f));
            g.drawLine (p.x - r * 0.6f, p.y - r * 0.6f, p.x + r * 0.6f, p.y + r * 0.6f, 1.2f);
        }
    }
}

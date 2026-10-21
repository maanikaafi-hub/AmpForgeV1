/*  AmpForge - level meters (implementation). */
#include "MeterComponent.h"
#include "AmpForgeLookAndFeel.h"

//==============================================================================
MeterComponent::MeterComponent (const juce::String& titleToUse, LevelSource source, int numChannelsToUse)
    : title (titleToUse),
      getLevel (std::move (source)),
      numChannels (juce::jlimit (1, 2, numChannelsToUse))
{
    startTimerHz (30);
}

void MeterComponent::timerCallback()
{
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float level = juce::jlimit (0.0f, 1.2f, getLevel (ch));

        // instant attack, ~12 dB per second release
        displayed[(size_t) ch] = level > displayed[(size_t) ch]
                                    ? level
                                    : displayed[(size_t) ch] + (level - displayed[(size_t) ch]) * 0.25f;

        peakHold[(size_t) ch] = juce::jmax (level, peakHold[(size_t) ch] - 0.006f);

        if (peakHold[(size_t) ch] < 0.0f)
            peakHold[(size_t) ch] = 0.0f;

        clipGlow[(size_t) ch] = level >= 0.985f ? 1.0f
                                                : juce::jmax (0.0f, clipGlow[(size_t) ch] - 0.05f);
    }

    repaint();
}

void MeterComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (3.0f);

    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.setColour (AmpForgeLookAndFeel::labelColour());
    g.drawText (title.toUpperCase(), bounds.removeFromTop (14.0f), juce::Justification::centred);

    bounds.reduce (2.0f, 0.0f);

    const float gap = 4.0f;
    const float barWidth = (bounds.getWidth() - gap * (float) (numChannels - 1)) / (float) numChannels;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto bar = bounds.removeFromLeft (barWidth);

        bounds.removeFromLeft (gap);

        // --- well ------------------------------------------------------------
        g.setColour (AmpForgeLookAndFeel::insetColour());
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (bar, 3.0f, 1.0f);

        // --- scale -----------------------------------------------------------
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.setFont (juce::Font (juce::FontOptions (7.5f)));

        const float usable = bar.getHeight() - 4.0f;

        for (float db : { -6.0f, -12.0f, -24.0f })
        {
            const float y = bar.getBottom() - 2.0f - usable * (float) std::pow (10.0, db / 20.0);
            g.drawHorizontalLine (juce::roundToInt (y), bar.getX() + 1.0f, bar.getRight() - 1.0f);
        }

        // --- level -----------------------------------------------------------
        const float level = juce::jlimit (0.0f, 1.0f, displayed[(size_t) ch]);
        const float height = level * usable;

        if (height > 0.5f)
        {
            const auto fill = bar.reduced (1.5f).withHeight (height).withBottomY (bar.getBottom() - 2.0f);

            juce::ColourGradient grad (juce::Colour (0xff3ad07a), fill.getX(), fill.getBottom(),
                                       juce::Colour (0xffe2452f), fill.getX(), fill.getBottom() - usable * 0.75f,
                                       false);

            g.setGradientFill (grad);
            g.fillRoundedRectangle (fill, 2.0f);
        }

        // --- peak hold --------------------------------------------------------
        const float peak = juce::jlimit (0.0f, 1.0f, peakHold[(size_t) ch]);

        if (peak > 0.001f)
        {
            const float y = bar.getBottom() - 2.0f - peak * usable;
            g.setColour (AmpForgeLookAndFeel::accentColour());
            g.drawHorizontalLine (juce::roundToInt (y), bar.getX() + 1.0f, bar.getRight() - 1.0f);
        }

        // --- clip lamp --------------------------------------------------------
        const float glow = clipGlow[(size_t) ch];

        g.setColour (AmpForgeLookAndFeel::ledColour().withAlpha (0.25f + 0.75f * glow));
        g.fillEllipse (bar.getCentreX() - 3.0f, bar.getY() + 1.0f, 6.0f, 6.0f);
    }
}

//==============================================================================
GainReductionMeter::GainReductionMeter (Source source)
    : getReductionDb (std::move (source))
{
    startTimerHz (30);
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (AmpForgeLookAndFeel::insetColour());
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    const float reduction = juce::jlimit (-40.0f, 0.0f, currentDb);
    const float norm = -reduction / 40.0f;

    if (norm > 0.001f)
    {
        const auto fill = bounds.reduced (1.5f).withWidth (juce::jmax (3.0f, bounds.getWidth() * norm));

        juce::ColourGradient grad (AmpForgeLookAndFeel::accentColour(), fill.getX(), fill.getCentreY(),
                                   AmpForgeLookAndFeel::accentDim(),   fill.getRight(), fill.getCentreY(), false);

        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 2.0f);
    }

    g.setColour (AmpForgeLookAndFeel::labelColour());
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText (juce::String (reduction, 1) + " dB", bounds, juce::Justification::centred);
}

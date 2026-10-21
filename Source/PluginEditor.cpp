/*  AmpForge - editor (implementation). */
#include "PluginEditor.h"

using namespace ampforge;

namespace
{
    constexpr int defaultWidth  = 1060;
    constexpr int defaultHeight = 640;
}

//==============================================================================
AmpForgeAudioProcessorEditor::AmpForgeAudioProcessorEditor (AmpForgeAudioProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    // --- amplifier -----------------------------------------------------------
    ampKnobs.push_back (&addKnob (id::gain,     "GAIN",     display::gainValue));
    ampKnobs.push_back (&addKnob (id::bass,     "BASS",     display::dB));
    ampKnobs.push_back (&addKnob (id::mid,      "MID",      display::dB));
    ampKnobs.push_back (&addKnob (id::treble,   "TREBLE",   display::dB));
    ampKnobs.push_back (&addKnob (id::presence, "PRESENCE", display::dB));
    ampKnobs.push_back (&addKnob (id::master,   "MASTER",   display::dB));

    // --- gate -----------------------------------------------------------------
    gateKnobs.push_back (&addKnob (id::gateThreshold, "THRESHOLD", display::dB));
    gateKnobs.push_back (&addKnob (id::gateRelease,   "RELEASE",   display::ms));

    // --- reverb ---------------------------------------------------------------
    reverbKnobs.push_back (&addKnob (id::reverbMix,       "MIX",       display::percent));
    reverbKnobs.push_back (&addKnob (id::reverbDecay,     "DECAY",
                                     [] (float v) { return display::seconds (ampforge::convert::decayToSeconds (v)); }));
    reverbKnobs.push_back (&addKnob (id::reverbPreDelay,  "PRE-DELAY", display::ms));
    reverbKnobs.push_back (&addKnob (id::reverbTone,      "TONE",      display::percent));

    // --- delay ----------------------------------------------------------------
    delayKnobs.push_back (&addKnob (id::delayTime,     "TIME",     display::ms));
    delayKnobs.push_back (&addKnob (id::delayFeedback, "FEEDBACK", display::percent));
    delayKnobs.push_back (&addKnob (id::delayMix,      "MIX",      display::percent));
    delayKnobs.push_back (&addKnob (id::delayTone,     "TONE",     display::percent));

    // --- selectors -------------------------------------------------------------
    ampSelector = std::make_unique<OptionSelector> (processor.getPluginState(), id::ampModel,
                                                    ampforge::choice::ampModels());
    addAndMakeVisible (*ampSelector);

    cabSelector = std::make_unique<OptionSelector> (processor.getPluginState(), id::cabProfile,
                                                    ampforge::choice::cabProfiles());
    cabSelector->beforeSelect = [this] (int index)
    {
        // "User IR" with nothing loaded yet: open the browser instead.
        if (index == (int) ampforge::CabinetSim::Profile::UserIR && ! processor.getCabinet().hasUserIR())
        {
            showIRChooser();
            return false;
        }

        return true;
    };
    addAndMakeVisible (*cabSelector);

    reverbSelector = std::make_unique<OptionSelector> (processor.getPluginState(), id::reverbMode,
                                                       ampforge::choice::reverbModes());
    addAndMakeVisible (*reverbSelector);

    delaySelector = std::make_unique<OptionSelector> (processor.getPluginState(), id::delayMode,
                                                      ampforge::choice::delayModes());
    addAndMakeVisible (*delaySelector);

    // --- meters ----------------------------------------------------------------
    inputMeter = std::make_unique<MeterComponent> ("IN", [this] (int ch) { return processor.getInputMeter().getPeak (ch); }, 2);
    outputMeter = std::make_unique<MeterComponent> ("OUT", [this] (int ch) { return processor.getOutputMeter().getPeak (ch); }, 2);
    gateMeter = std::make_unique<GainReductionMeter> ([this] { return processor.getGateReductionDb(); });

    addAndMakeVisible (*inputMeter);
    addAndMakeVisible (*outputMeter);
    addAndMakeVisible (*gateMeter);

    // --- switches ---------------------------------------------------------------
    gateButton = std::make_unique<juce::TextButton> ("GATE ON");
    gateButton->setClickingTogglesState (true);
    gateButton->setTooltip ("Enables the noise gate");
    addAndMakeVisible (*gateButton);
    gateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                         (processor.getPluginState(), id::gateEnable, *gateButton);

    cabButton = std::make_unique<juce::TextButton> ("CAB ON");
    cabButton->setClickingTogglesState (true);
    cabButton->setTooltip ("Enables the cabinet simulation");
    addAndMakeVisible (*cabButton);
    cabAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                        (processor.getPluginState(), id::cabEnable, *cabButton);

    loadIRButton = std::make_unique<juce::TextButton> ("LOAD IR...");
    loadIRButton->setTooltip ("Load a user impulse response (wav / aiff / flac / ogg)");
    loadIRButton->onClick = [this] { showIRChooser(); };
    addAndMakeVisible (*loadIRButton);

    irNameLabel.setText ("4x12 Closed", juce::dontSendNotification);
    irNameLabel.setJustificationType (juce::Justification::centred);
    irNameLabel.setColour (juce::Label::textColourId, AmpForgeLookAndFeel::labelColour());
    addAndMakeVisible (irNameLabel);

    // --- window ------------------------------------------------------------------
    setSize (defaultWidth, defaultHeight);
    setResizable (true, true);
    setResizeLimits (900, 560, 1920, 1280);

    if (auto* boundsConstrainer = getConstrainer())
        boundsConstrainer->setFixedAspectRatio ((double) defaultWidth / (double) defaultHeight);

    startTimerHz (10);
}

AmpForgeAudioProcessorEditor::~AmpForgeAudioProcessorEditor()
{
    stopTimer();

    for (auto& k : knobs)
    {
        k->slider.setLookAndFeel (nullptr);
        k->nameLabel.setLookAndFeel (nullptr);
        k->valueLabel.setLookAndFeel (nullptr);
    }

    setLookAndFeel (nullptr);
}

//==============================================================================
AmpForgeAudioProcessorEditor::KnobControl&
AmpForgeAudioProcessorEditor::addKnob (const juce::String& parameterID, const juce::String& name,
                                       std::function<juce::String (float)> formatter)
{
    auto knob = std::make_unique<KnobControl>();
    auto& k = *knob;

    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    k.slider.setScrollWheelEnabled (true);
    k.slider.setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (k.slider);

    k.nameLabel.setText (name, juce::dontSendNotification);
    k.nameLabel.setJustificationType (juce::Justification::centred);
    k.nameLabel.setColour (juce::Label::textColourId, AmpForgeLookAndFeel::labelColour());
    k.nameLabel.setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (k.nameLabel);

    k.valueLabel.setJustificationType (juce::Justification::centred);
    k.valueLabel.setColour (juce::Label::textColourId, AmpForgeLookAndFeel::accentColour());
    k.valueLabel.setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (k.valueLabel);

    k.formatValue = std::move (formatter);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                       (processor.getPluginState(), parameterID, k.slider);

    auto updateText = [&k]()
    {
        if (k.formatValue != nullptr)
            k.valueLabel.setText (k.formatValue ((float) k.slider.getValue()), juce::dontSendNotification);
    };

    k.slider.onValueChange = updateText;
    updateText();

    knobs.push_back (std::move (knob));
    return *knobs.back();
}

//==============================================================================
void AmpForgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (juce::Colour (0xff2c2f35), 0.0f, 0.0f,
                                     juce::Colour (0xff14161a), 0.0f, (float) getHeight(), false);
    g.setGradientFill (background);
    g.fillAll();

    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillRect (0, 0, getWidth(), 2);

    drawHeader (g, headerBounds);

    AmpForgeLookAndFeel::drawPanel (g, ampPanel,    "Amplifier");
    AmpForgeLookAndFeel::drawPanel (g, meterPanel,  "Levels");
    AmpForgeLookAndFeel::drawPanel (g, cabPanel,    "Cabinet");
    AmpForgeLookAndFeel::drawPanel (g, gatePanel,   "Noise Gate");
    AmpForgeLookAndFeel::drawPanel (g, reverbPanel, "Reverb");
    AmpForgeLookAndFeel::drawPanel (g, delayPanel,  "Delay");
}

void AmpForgeAudioProcessorEditor::drawHeader (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (bounds.isEmpty())
        return;

    // --- chassis strip ---------------------------------------------------------
    juce::ColourGradient strip (juce::Colour (0xff35393f), 0.0f, (float) bounds.getY(),
                                juce::Colour (0xff181b1f), 0.0f, (float) bounds.getBottom(), false);
    g.setGradientFill (strip);
    g.fillRect (bounds);

    g.setColour (juce::Colour (0xff0a0b0d));
    g.drawHorizontalLine (bounds.getBottom() - 1, (float) bounds.getX(), (float) bounds.getRight());

    // --- brand ------------------------------------------------------------------
    auto brandArea = bounds.removeFromLeft (280).reduced (16, 8);

    g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)));
    g.setColour (juce::Colour (0xff0e0f11));
    g.drawText ("AMPFORGE", brandArea.translated (0, 2), juce::Justification::centredLeft);

    juce::ColourGradient brand (juce::Colour (0xfffdf3e2), 0.0f, (float) brandArea.getY(),
                                AmpForgeLookAndFeel::accentColour(), 0.0f, (float) brandArea.getBottom(), false);
    g.setGradientFill (brand);
    g.drawText ("AMPFORGE", brandArea, juce::Justification::centredLeft);

    g.setFont (juce::Font (juce::FontOptions (10.5f)));
    g.setColour (AmpForgeLookAndFeel::labelColour());
    g.drawText ("GUITAR AMP SIMULATOR", brandArea.removeFromBottom (14).withTrimmedLeft (4),
                juce::Justification::centredLeft);

    // --- spacer + selector area handled by child components ----------------------
    const auto lampArea = bounds.removeFromRight (60).reduced (0, 10);
    const float lampSize = juce::jmin (14.0f, (float) lampArea.getHeight());

    g.setColour (AmpForgeLookAndFeel::ledColour().withAlpha (0.25f));
    g.fillEllipse ((float) lampArea.getCentreX() - lampSize, (float) lampArea.getCentreY() - lampSize,
                   lampSize * 2.0f, lampSize * 2.0f);

    g.setColour (AmpForgeLookAndFeel::ledColour());
    g.fillEllipse ((float) lampArea.getCentreX() - lampSize * 0.5f, (float) lampArea.getCentreY() - lampSize * 0.5f,
                   lampSize, lampSize);
}

//==============================================================================
void AmpForgeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    headerBounds = area.removeFromTop (juce::jmin (64, (int) ((double) getHeight() * 0.10)));
    area.removeFromTop (10);

    const int bottomHeight = juce::roundToInt ((double) area.getHeight() * 0.40);
    auto bottom = area.removeFromBottom (bottomHeight);
    auto top = area;

    // --- top row: amplifier | levels | cabinet ----------------------------------
    const int meterWidth = juce::roundToInt ((double) top.getWidth() * 0.16);
    const int sideWidth  = (top.getWidth() - meterWidth) / 2;

    ampPanel   = top.removeFromLeft (sideWidth);
    meterPanel = top.removeFromLeft (meterWidth);
    cabPanel   = top;

    // --- bottom row: gate | reverb | delay ---------------------------------------
    const int gateWidth = juce::roundToInt ((double) bottom.getWidth() * 0.26);
    const int fxWidth   = (bottom.getWidth() - gateWidth) / 2;

    gatePanel   = bottom.removeFromLeft (gateWidth);
    reverbPanel = bottom.removeFromLeft (fxWidth);
    delayPanel  = bottom;

    // --- header contents -----------------------------------------------------------
    if (ampSelector != nullptr)
        ampSelector->setBounds (headerBounds.withTrimmedLeft (290)
                                            .withTrimmedRight (70)
                                            .reduced (0, 14));

    layoutKnobs();
}

void AmpForgeAudioProcessorEditor::layoutKnobs()
{
    auto inner = [] (juce::Rectangle<int> panel)
    {
        return panel.reduced (16, 30);
    };

    layoutGrid (inner (ampPanel),   ampKnobs,    3);

    // --- levels ---------------------------------------------------------------------
    {
        auto m = inner (meterPanel);
        const int w = m.getWidth() / 2;

        if (inputMeter != nullptr)  inputMeter->setBounds  (m.removeFromLeft (w).reduced (3, 0));
        if (outputMeter != nullptr) outputMeter->setBounds (m.reduced (3, 0));
    }

    // --- cabinet ----------------------------------------------------------------------
    {
        auto c = inner (cabPanel);

        if (cabSelector != nullptr)  cabSelector->setBounds  (c.removeFromTop (28));
        c.removeFromTop (8);

        if (loadIRButton != nullptr) loadIRButton->setBounds (c.removeFromTop (28));
        c.removeFromTop (8);

        irNameLabel.setBounds (c.removeFromTop (20));
        c.removeFromTop (8);

        if (cabButton != nullptr)    cabButton->setBounds    (c.removeFromTop (28));
    }

    // --- gate --------------------------------------------------------------------------
    {
        auto gt = inner (gatePanel);

        if (gateButton != nullptr) gateButton->setBounds (gt.removeFromTop (26));
        gt.removeFromTop (8);

        if (gateMeter != nullptr)   gateMeter->setBounds   (gt.removeFromBottom (20));
        gt.removeFromBottom (8);

        layoutGrid (gt, gateKnobs, 2);
    }

    // --- reverb --------------------------------------------------------------------------
    {
        auto r = inner (reverbPanel);

        if (reverbSelector != nullptr) reverbSelector->setBounds (r.removeFromTop (28));
        r.removeFromTop (8);

        layoutGrid (r, reverbKnobs, 2);
    }

    // --- delay ----------------------------------------------------------------------------
    {
        auto d = inner (delayPanel);

        if (delaySelector != nullptr) delaySelector->setBounds (d.removeFromTop (28));
        d.removeFromTop (8);

        layoutGrid (d, delayKnobs, 2);
    }
}

void AmpForgeAudioProcessorEditor::layoutGrid (juce::Rectangle<int> area,
                                               const std::vector<KnobControl*>& items, int columns)
{
    if (items.empty() || area.isEmpty() || columns <= 0)
        return;

    const int rows  = ((int) items.size() + columns - 1) / columns;
    const int cellW = area.getWidth()  / columns;
    const int cellH = area.getHeight() / juce::jmax (1, rows);

    for (int i = 0; i < (int) items.size(); ++i)
    {
        const int col = i % columns;
        const int row = i / columns;

        auto cell = juce::Rectangle<int> (area.getX() + col * cellW,
                                          area.getY() + row * cellH,
                                          cellW, cellH).reduced (8, 4);

        if (cell.isEmpty())
            continue;

        auto* k = items[(size_t) i];

        auto knobArea = cell.removeFromTop (juce::roundToInt ((double) cell.getHeight() * 0.68));
        const int size = juce::jmax (24, juce::jmin (knobArea.getWidth(), knobArea.getHeight()));

        k->slider.setBounds (knobArea.withSizeKeepingCentre (size, size));
        k->nameLabel.setBounds  (cell.removeFromTop (juce::jmin (16, cell.getHeight() / 2)));
        k->valueLabel.setBounds (cell);
    }
}

//==============================================================================
void AmpForgeAudioProcessorEditor::showIRChooser()
{
    auto startDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    fileChooser = std::make_unique<juce::FileChooser> ("Choose an impulse response (wav, aiff, flac, ogg)...",
                                                       startDir, "*.wav;*.aiff;*.aif;*.flac;*.ogg");

    const auto browserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    auto safeThis = juce::Component::SafePointer<AmpForgeAudioProcessorEditor> (this);

    fileChooser->launchAsync (browserFlags, [this, safeThis] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        const auto file = chooser.getResult();

        if (file == juce::File{})
            return;

        processor.getCabinet().requestUserIR (file);

        if (auto* param = processor.getPluginState().getParameter (id::cabProfile))
        {
            const float normalised = param->getNormalisableRange()
                                          .convertTo0to1 ((float) ampforge::CabinetSim::Profile::UserIR);
            param->setValueNotifyingHost (normalised);
        }
    });
}

void AmpForgeAudioProcessorEditor::timerCallback()
{
    auto& cabinet = processor.getCabinet();
    const int counter = cabinet.getLoadCounter();

    if (counter != lastLoadCounter || irNameLabel.getText().isEmpty())
    {
        lastLoadCounter = counter;

        auto name = cabinet.getCurrentIRName();

        if (name.isEmpty())
            name = ampforge::CabinetSim::profileName (ampforge::CabinetSim::Profile::FourTwelveClosed);

        if (cabinet.isLoading())
            name = "loading " + name + "...";

        irNameLabel.setText (name, juce::dontSendNotification);
    }
}

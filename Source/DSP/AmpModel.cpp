/*  AmpForge - the three amp models (implementation). */
#include "AmpModel.h"

namespace ampforge
{

//==============================================================================
AmpModel::Character AmpModel::getCharacter (AmpType t) noexcept
{
    Character c;

    switch (t)
    {
        //----------------------------------------------------------------------
        case AmpType::AmericanClean:
            c.preGainMinDb  = -2.0f;
            c.preGainMaxDb  = 18.0f;
            c.inputHP       = 32.0f;
            c.brightFreq    = 1800.0f;
            c.brightGainDb  = 2.5f;
            c.midFreq       = 900.0f;
            c.midGainDb     = -2.5f;
            c.midQ          = 0.7f;
            c.stageLP       = 9500.0f;
            c.lowResFreq    = 95.0f;
            c.lowResGainDb  = 1.5f;
            c.lowResQ       = 0.8f;
            c.bassFreq      = 100.0f;
            c.midToneFreq   = 780.0f;
            c.midToneQ      = 0.9f;
            c.trebleFreq    = 2400.0f;
            c.presenceFreq  = 3800.0f;
            c.presenceQ     = 0.7f;
            c.sagAmount     = 0.12f;
            c.sagAttackSec  = 0.004f;
            c.sagReleaseSec = 0.120f;
            c.stageGains[0] = 1.0f;
            c.stageGains[1] = 0.85f;
            c.stageGains[2] = 0.0f;
            c.numStages     = 2;
            c.clipStyle     = ClipStyle::Cubic;
            // the clean model has far less compression than the other two, so
            // it gets a little more output trim: switching amp models should
            // change the character, not the loudness
            c.outputTrim    = 1.5f;
            break;

        //----------------------------------------------------------------------
        case AmpType::BritishCrunch:
            c.preGainMinDb  = 0.0f;
            c.preGainMaxDb  = 34.0f;
            c.inputHP       = 45.0f;
            c.brightFreq    = 2200.0f;
            c.brightGainDb  = 6.0f;
            c.midFreq       = 720.0f;
            c.midGainDb     = 5.5f;
            c.midQ          = 1.0f;
            c.stageLP       = 7000.0f;
            c.lowResFreq    = 120.0f;
            c.lowResGainDb  = 2.0f;
            c.lowResQ       = 0.9f;
            c.bassFreq      = 120.0f;
            c.midToneFreq   = 650.0f;
            c.midToneQ      = 0.8f;
            c.trebleFreq    = 2800.0f;
            c.presenceFreq  = 3600.0f;
            c.presenceQ     = 0.6f;
            c.sagAmount     = 0.28f;
            c.sagAttackSec  = 0.002f;
            c.sagReleaseSec = 0.090f;
            c.stageGains[0] = 1.60f;
            c.stageGains[1] = 1.25f;
            c.stageGains[2] = 1.00f;
            c.numStages     = 3;
            c.clipStyle     = ClipStyle::Asymmetric;
            c.outputTrim    = 0.85f;
            break;

        //----------------------------------------------------------------------
        case AmpType::ModernHighGain:
            c.preGainMinDb  = 6.0f;
            c.preGainMaxDb  = 50.0f;
            c.inputHP       = 105.0f;      // tight low end
            c.brightFreq    = 3000.0f;
            c.brightGainDb  = 8.0f;
            c.midFreq       = 1100.0f;
            c.midGainDb     = 1.5f;
            c.midQ          = 0.8f;
            c.stageLP       = 6200.0f;
            c.lowResFreq    = 130.0f;
            c.lowResGainDb  = 3.0f;
            c.lowResQ       = 1.1f;
            c.bassFreq      = 140.0f;
            c.midToneFreq   = 900.0f;
            c.midToneQ      = 1.0f;
            c.trebleFreq    = 3200.0f;
            c.presenceFreq  = 4200.0f;
            c.presenceQ     = 0.8f;
            c.sagAmount     = 0.10f;
            c.sagAttackSec  = 0.001f;
            c.sagReleaseSec = 0.060f;
            c.stageGains[0] = 2.20f;
            c.stageGains[1] = 1.70f;
            c.stageGains[2] = 1.35f;
            c.numStages     = 3;
            c.clipStyle     = ClipStyle::Hard;
            c.outputTrim    = 0.70f;
            break;
    }

    return c;
}

//==============================================================================
/** JUCE's FastMathApproximations::tanh() is a Pade approximant that is only
 *  valid for arguments between -5 and +5; outside that range it grows without
 *  bound instead of saturating.  A heavily driven pre-amp stage reaches
 *  arguments in the hundreds, so the argument is clamped first: tanh(4) is
 *  0.9993, so clamping at 4 is inaudible. */
static inline float tanhLimited (float x) noexcept
{
    return juce::dsp::FastMathApproximations::tanh (juce::jlimit (-4.0f, 4.0f, x));
}

//==============================================================================
float AmpModel::clipSample (float x, ClipStyle style) noexcept
{
    switch (style)
    {
        case ClipStyle::Cubic:
        {
            // Unity small signal gain, smooth transfer, hard limit at |x| = 1.5
            const float c = juce::jlimit (-1.0f, 1.0f, 0.6666667f * x);
            return 1.5f * (c - (c * c * c) * (1.0f / 3.0f));
        }

        case ClipStyle::Asymmetric:
        {
            // tanh plus an even order term: adds 2nd harmonics and makes the
            // two half waves clip differently, which is what gives a class A
            // style breakup its "chewy" character.  The small DC offset the
            // squared term produces is removed by the DC blocker at the end of
            // the chain.
            const float t = tanhLimited (1.15f * x);
            return t + 0.18f * t * t;
        }

        case ClipStyle::Hard:
        default:
        {
            // Steep diode pair style transfer - lots of sustain.
            return 0.95f * tanhLimited (1.9f * x);
        }
    }
}

//==============================================================================
void AmpModel::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate  = spec.sampleRate;
    numChannels = juce::jlimit (1, 2, (int) spec.numChannels);

    constexpr double ramp = 0.04;
    driveSmoothed.reset    (spec.sampleRate, ramp);
    masterSmoothed.reset   (spec.sampleRate, ramp);
    bassSmoothed.reset     (spec.sampleRate, ramp);
    midSmoothed.reset      (spec.sampleRate, ramp);
    trebleSmoothed.reset   (spec.sampleRate, ramp);
    presenceSmoothed.reset (spec.sampleRate, ramp);

    driveSmoothed.setCurrentAndTargetValue    (currentDrive);
    masterSmoothed.setCurrentAndTargetValue   (currentMasterDb);
    bassSmoothed.setCurrentAndTargetValue     (currentBass);
    midSmoothed.setCurrentAndTargetValue      (currentMid);
    trebleSmoothed.setCurrentAndTargetValue   (currentTreble);
    presenceSmoothed.setCurrentAndTargetValue (currentPresence);

    updateSagCoefficients();
    reset();

    coefficientsDirty = true;
    updateCoefficients();
    prepared = true;
}

void AmpModel::reset() noexcept
{
    for (auto& c : channels)
    {
        c.inputHP.reset();   c.brightCap.reset(); c.midHump.reset();
        c.stageLP.reset();   c.lowRes.reset();    c.toneLow.reset();
        c.toneMid.reset();   c.toneHigh.reset();  c.presence.reset();
        c.dcBlock.reset();
        c.sag = 0.0f;
    }
}

void AmpModel::setType (AmpType newType) noexcept
{
    if (newType == type && prepared)
        return;

    type      = newType;
    character = getCharacter (newType);

    updateSagCoefficients();
    reset();
    coefficientsDirty = true;
}

void AmpModel::setDrive (float driveNormalised) noexcept
{
    driveSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, driveNormalised));
}

void AmpModel::setMasterDb (float masterDb) noexcept
{
    masterSmoothed.setTargetValue (juce::jlimit (-60.0f, 6.0f, masterDb));
}

void AmpModel::setTone (float bassDb, float midDb, float trebleDb, float presenceDb) noexcept
{
    bassSmoothed.setTargetValue     (juce::jlimit (-12.0f, 12.0f, bassDb));
    midSmoothed.setTargetValue      (juce::jlimit (-12.0f, 12.0f, midDb));
    trebleSmoothed.setTargetValue   (juce::jlimit (-12.0f, 12.0f, trebleDb));
    presenceSmoothed.setTargetValue (juce::jlimit (-12.0f, 12.0f, presenceDb));
}

//==============================================================================
void AmpModel::updateSmoothers() noexcept
{
    currentDrive    = driveSmoothed.getNextValue();
    currentMasterDb = masterSmoothed.getNextValue();
    currentBass     = bassSmoothed.getNextValue();
    currentMid      = midSmoothed.getNextValue();
    currentTreble   = trebleSmoothed.getNextValue();
    currentPresence = presenceSmoothed.getNextValue();

    const auto db = character.preGainMinDb
                  + (character.preGainMaxDb - character.preGainMinDb) * currentDrive;

    preGain    = juce::Decibels::decibelsToGain ((double) db);
    masterGain = juce::Decibels::decibelsToGain ((double) currentMasterDb) * character.outputTrim;

    updateCoefficients();
}

void AmpModel::updateCoefficients() noexcept
{
    const double fs = sampleRate;

    if (coefficientsDirty)
    {
        for (auto& c : channels)
        {
            c.inputHP.setHighPass  (fs, character.inputHP,     0.7071);
            c.brightCap.setHighShelf (fs, character.brightFreq,  character.brightGainDb);
            c.midHump.setPeaking   (fs, character.midFreq,     character.midQ,   character.midGainDb);
            c.stageLP.setLowPass   (fs, character.stageLP,     0.7071);
            c.lowRes.setPeaking    (fs, character.lowResFreq,  character.lowResQ, character.lowResGainDb);
            c.dcBlock.setHighPass  (fs, 18.0,                  0.7071);

            c.toneLow.setLowShelf  (fs, character.bassFreq,     currentBass);
            c.toneMid.setPeaking   (fs, character.midToneFreq,  character.midToneQ, currentMid);
            c.toneHigh.setHighShelf (fs, character.trebleFreq,  currentTreble);
            c.presence.setPeaking  (fs, character.presenceFreq, character.presenceQ, currentPresence);
        }

        lastBass = currentBass;
        lastMid = currentMid;
        lastTreble = currentTreble;
        lastPresence = currentPresence;
        coefficientsDirty = false;
        return;
    }

    if (std::abs (currentBass - lastBass) > 0.02f)
    {
        for (auto& c : channels) c.toneLow.setLowShelf (fs, character.bassFreq, currentBass);
        lastBass = currentBass;
    }

    if (std::abs (currentMid - lastMid) > 0.02f)
    {
        for (auto& c : channels) c.toneMid.setPeaking (fs, character.midToneFreq, character.midToneQ, currentMid);
        lastMid = currentMid;
    }

    if (std::abs (currentTreble - lastTreble) > 0.02f)
    {
        for (auto& c : channels) c.toneHigh.setHighShelf (fs, character.trebleFreq, currentTreble);
        lastTreble = currentTreble;
    }

    if (std::abs (currentPresence - lastPresence) > 0.02f)
    {
        for (auto& c : channels) c.presence.setPeaking (fs, character.presenceFreq, character.presenceQ, currentPresence);
        lastPresence = currentPresence;
    }
}

//==============================================================================
float AmpModel::processSample (int channel, float input) noexcept
{
    auto& c = channels[(size_t) juce::jlimit (0, numChannels - 1, channel)];

    // --- power supply sag: louder playing pulls the rail down ---------------
    const float rect = std::abs (input);
    const float sagCoeff = rect > c.sag ? sagAttackCoeff : sagReleaseCoeff;
    c.sag += sagCoeff * (rect - c.sag);

    float x = input * preGain / (1.0f + character.sagAmount * c.sag);

    // --- pre amp voicing ---------------------------------------------------
    x = c.inputHP.process (x);      // rumble / flub control
    x = c.brightCap.process (x);    // bright cap across the gain control

    // --- gain stages --------------------------------------------------------
    const int stages = juce::jlimit (1, 3, character.numStages);

    for (int s = 0; s < stages; ++s)
    {
        x = clipSample (x * character.stageGains[(size_t) s], character.clipStyle);

        if (s == 0)
            x = c.midHump.process (x);   // inter stage voicing

        x = c.stageLP.process (x);       // keep the fizz down
    }

    // --- power amp resonance, tone stack, presence, master ------------------
    x = c.lowRes.process (x);
    x = c.toneLow.process (x);
    x = c.toneMid.process (x);
    x = c.toneHigh.process (x);
    x = c.presence.process (x);

    x *= masterGain;

    // --- remove the DC that asymmetric clipping introduces ------------------
    return c.dcBlock.process (x);
}

} // namespace ampforge

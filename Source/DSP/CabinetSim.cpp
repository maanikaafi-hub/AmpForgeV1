/*  AmpForge - cabinet simulation (implementation). */
#include "CabinetSim.h"

#include <vector>

namespace ampforge
{

//==============================================================================
class CabinetSim::LoaderThread : public juce::Thread
{
public:
    explicit LoaderThread (CabinetSim& ownerToUse)
        : juce::Thread ("AmpForge IR Loader"), owner (ownerToUse) {}

    void run() override
    {
        while (! threadShouldExit())
        {
            owner.serviceRequests();

            if (wait (40))   // returns true when the thread should exit
                break;
        }
    }

private:
    CabinetSim& owner;
};

//==============================================================================
CabinetSim::CabinetSim()
{
    loader = std::make_unique<LoaderThread> (*this);
    loader->startThread (juce::Thread::Priority::normal);
}

CabinetSim::~CabinetSim()
{
    if (loader != nullptr)
        loader->stopThread (3000);
}

//==============================================================================
void CabinetSim::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    // Pre-allocate the wet path so that process() never allocates.  8192
    // samples covers every buffer size the plugin accepts at 96 kHz.
    const int wetSize = juce::jmax ((int) spec.maximumBlockSize, 8192);
    wetBuffer.setSize (2, wetSize);
    wetBuffer.clear();

    convolution.prepare (spec);
    cabMix.reset (spec.sampleRate, 0.04);

    reset();

    // Apply the current IR immediately so the very first processed block is
    // already cabinet-simulated (bounded arithmetic, no disk, no locks).
    const int requested = requestedProfile.load();
    const bool wantUser = userIRValid.load() && (requested < 0 || requested == (int) Profile::UserIR);

    if (wantUser)
        applyStoredUserIR();
    else
        applyBuiltIn ((Profile) (requested >= 0 && requested != (int) Profile::UserIR
                                     ? requested
                                     : (int) Profile::FourTwelveClosed));

    prepared = true;
}

void CabinetSim::reset() noexcept
{
    convolution.reset();
    cabMix.setCurrentAndTargetValue (enabled.load() ? 1.0f : 0.0f);
}

void CabinetSim::setEnabled (bool shouldBeEnabled) noexcept
{
    enabled.store (shouldBeEnabled);
}

void CabinetSim::requestUserIR (const juce::File& file)
{
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        pendingFile = file;
    }

    fileRequestToken.fetch_add (1);
}

//==============================================================================
juce::String CabinetSim::getCurrentIRName() const noexcept
{
    const juce::SpinLock::ScopedLockType lock (stateLock);
    return currentIRName;
}

//==============================================================================
void CabinetSim::process (const juce::dsp::ProcessContextReplacing<float>& context) noexcept
{
    if (! prepared)
        return;

    auto& outputBlock = context.getOutputBlock();

    const int numSamples  = (int) outputBlock.getNumSamples();
    const int numChannels = juce::jmin ((int) outputBlock.getNumChannels(), wetBuffer.getNumChannels());

    if (numSamples <= 0 || numChannels <= 0 || numSamples > wetBuffer.getNumSamples())
        return;

    cabMix.setTargetValue (enabled.load() ? 1.0f : 0.0f);

    const bool active = cabMix.getCurrentValue() > 0.0001f || cabMix.getTargetValue() > 0.0001f;

    if (! active)
        return;   // the replacing context already holds the dry signal

    for (int ch = 0; ch < numChannels; ++ch)
        juce::FloatVectorOperations::copy (wetBuffer.getWritePointer (ch),
                                           outputBlock.getChannelPointer ((size_t) ch),
                                           numSamples);

    auto wetBlock = juce::dsp::AudioBlock<float> (wetBuffer).getSubBlock (0, (size_t) numSamples);
    juce::dsp::ProcessContextReplacing<float> wetContext (wetBlock);

    convolution.process (wetContext);

    for (int i = 0; i < numSamples; ++i)
    {
        const float m = cabMix.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* out = outputBlock.getChannelPointer ((size_t) ch);
            const auto* wet = wetBlock.getChannelPointer ((size_t) ch);

            out[i] = out[i] * (1.0f - m) + wet[i] * m;
        }
    }
}

//==============================================================================
void CabinetSim::serviceRequests()
{
    const int fileToken = fileRequestToken.load();

    if (fileToken != lastFileToken)
    {
        lastFileToken = fileToken;
        loading.store (true);

        juce::File file;
        {
            const juce::SpinLock::ScopedLockType lock (stateLock);
            file = pendingFile;
        }

        loadUserFile (file);
        loading.store (false);
        return;
    }

    const int requested = requestedProfile.load();

    if (requested >= 0 && requested != appliedProfile.load())
    {
        loading.store (true);
        applyBuiltIn ((Profile) requested);
        loading.store (false);
    }
}

void CabinetSim::applyBuiltIn (Profile p)
{
    if (p == Profile::UserIR)
    {
        if (userIRValid.load())
        {
            applyStoredUserIR();
            return;
        }

        p = Profile::FourTwelveClosed;   // nothing loaded yet: fall back
    }

    auto ir = generateProfile (p, sampleRate);

    convolution.loadImpulseResponse (std::move (ir), sampleRate,
                                     juce::dsp::Convolution::Stereo::yes,
                                     juce::dsp::Convolution::Trim::no,
                                     juce::dsp::Convolution::Normalise::no);

    appliedProfile.store ((int) p);
    userIRValid.store (false);

    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        currentIRName = profileName (p);
    }
}

void CabinetSim::applyStoredUserIR()
{
    juce::AudioBuffer<float> copy;
    double rate = sampleRate;

    {
        const juce::SpinLock::ScopedLockType lock (stateLock);

        if (userIR.getNumSamples() <= 0)
            return;

        copy = userIR;          // allocation happens here, off the audio thread
        rate = userIRRate;
    }

    convolution.loadImpulseResponse (std::move (copy), rate,
                                     juce::dsp::Convolution::Stereo::yes,
                                     juce::dsp::Convolution::Trim::yes,
                                     juce::dsp::Convolution::Normalise::yes);

    appliedProfile.store ((int) Profile::UserIR);

    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        currentIRName = userIRName;
    }
}

void CabinetSim::loadUserFile (const juce::File& file)
{
    juce::AudioFormatManager manager;
    manager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (manager.createReaderFor (file));

    if (reader == nullptr)
        return;

    const int maxSamples = (int) (2.0 * sampleRate);   // never convolve more than 2 s
    const int numSamples = juce::jlimit (1, maxSamples, (int) reader->lengthInSamples);
    const int numChans   = juce::jlimit (1, 2, (int) reader->numChannels);

    if (numSamples <= 0)
        return;

    juce::AudioBuffer<float> decoded (numChans, numSamples);

    if (! reader->read (&decoded, 0, numSamples, 0, true, numChans > 1))
        return;

    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        userIR     = decoded;
        userIRRate = reader->sampleRate > 0.0 ? reader->sampleRate : sampleRate;
        userIRName = file.getFileName();
    }

    userIRValid.store (true);
    applyStoredUserIR();
    loadCounter.fetch_add (1);
}

//==============================================================================
juce::AudioBuffer<float> CabinetSim::generateProfile (Profile p, double sampleRate)
{
    struct ProfileSpec
    {
        double lengthMs;
        double modeQ;              // Q of the low modes (how "boomy" the box is)
        double modeQFold;          // frequency where that Q has dropped to half
        double noiseDecaySeconds;  // length of the diffuse part
        double noiseLevel;         // how much diffuse energy fills the notches
        double lowPassHz;
        double highPassHz;
        float  trimDb;
        uint32_t seed;
    };

    ProfileSpec spec = { 95.0, 12.0, 1500.0, 0.032, 0.50, 7600.0, 70.0, 1.0f, 0x85ebca6bu };

    switch (p)
    {
        case Profile::FourTwelveClosed:
            spec = { 115.0, 15.0, 1600.0, 0.040, 0.55, 6200.0, 55.0, 2.0f, 0x9e3779b1u };
            break;
        case Profile::TwoTwelveCombo:
            spec = {  95.0, 12.0, 1500.0, 0.032, 0.50, 7600.0, 70.0, 1.0f, 0x85ebca6bu };
            break;
        case Profile::OneTwelveOpen:
            spec = {  78.0,  9.0, 1300.0, 0.024, 0.45, 9000.0, 92.0, 0.0f, 0xc2b2ae35u };
            break;
        case Profile::UserIR:
        default:
            break;
    }

    const int length = juce::jlimit (512, 16384, (int) std::ceil (spec.lengthMs * 0.001 * sampleRate));
    juce::AudioBuffer<float> ir (2, length);
    ir.clear();

    // A cabinet is a set of damped resonances (the box and the cones), a few
    // early reflections from the back and sides, and a diffuse decaying tail.
    static const float modeFreqs[] = {   62.0f,   84.0f,  112.0f,  148.0f,  190.0f,  245.0f,
                                        310.0f,  400.0f,  520.0f,  660.0f,  850.0f, 1080.0f,
                                       1400.0f, 1800.0f, 2300.0f, 2950.0f, 3750.0f, 4800.0f };
    const int numModes = (int) (sizeof (modeFreqs) / sizeof (modeFreqs[0]));

    // Early reflections from the back and the sides of the box.  Their level
    // is deliberately modest: stronger reflections comb with the direct sound
    // and punch deep, unnatural holes into the response.
    static const float reflectionMs[6]   = { 0.35f, 0.85f, 1.60f, 2.50f, 3.70f, 5.20f };
    static const float reflectionGain[6] = { 0.28f, 0.22f, 0.17f, 0.13f, 0.10f, 0.07f };

    const int preDelaySamples = (int) (0.0007 * sampleRate);   // acoustic centre

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* out = ir.getWritePointer (ch);

        uint32_t rngState = spec.seed + (uint32_t) ch * 7919u;

        auto nextRandom = [&rngState]()
        {
            rngState = rngState * 1664525u + 1013904223u;
            return (float) ((double) (int32_t) rngState / 2147483648.0);
        };

        // ---- resonant modes (a two pole resonator hit by an impulse) --------
        for (int k = 0; k < numModes; ++k)
        {
            const double detune = (ch == 0) ? 0.988 : 1.012;
            const double freq = juce::jlimit (20.0, sampleRate * 0.45, modeFreqs[k] * detune);
            // A real cabinet is resonant down low and heavily damped up high:
            // giving every mode the same decay time would make the upper modes
            // absurdly high Q and turn the response into a comb filter with
            // 30 dB deep notches.  Specifying the Q instead keeps the modes
            // evenly damped across the spectrum.
            const double q    = juce::jmax (2.0, spec.modeQ / (1.0 + freq / spec.modeQFold));
            const double tau  = juce::jmax (0.0004, q / (juce::MathConstants<double>::pi * freq));

            const double w    = 2.0 * juce::MathConstants<double>::pi * freq / sampleRate;
            const double r    = std::exp (-1.0 / (tau * sampleRate));
            const double a1   = 2.0 * r * std::cos (w);
            const double a2   = -r * r;

            // A resonator hit with an impulse peaks at excite / (1 - r) in the
            // frequency domain, which for the low, long ringing modes is three
            // orders of magnitude more than for the high, damped ones.  Scaling
            // the excitation by (1 - r) makes every mode contribute the level
            // that pow (0.87, k) asks for, so the spectrum follows that gentle
            // downward tilt instead of one enormous bass peak.  The sign is
            // deliberately not randomised: random signs make the modes cancel
            // at some frequencies and leave 25 dB deep holes, which no real
            // cabinet has.  The left / right detuning is what decorrelates the
            // two channels instead.
            const double excite = std::pow (0.87, (double) k) * (1.0 - r) * 2.0 * std::sin (w);

            double y1 = 0.0, y2 = 0.0;

            for (int i = preDelaySamples; i < length; ++i)
            {
                const double x = (i == preDelaySamples) ? excite : 0.0;
                const double y = x + a1 * y1 + a2 * y2;

                y2 = y1;
                y1 = y;

                out[i] += (float) y;
            }
        }

        // ---- early reflections ---------------------------------------------
        for (int k = 0; k < 6; ++k)
        {
            const int idx = preDelaySamples
                          + (int) (reflectionMs[k] * 0.001 * sampleRate * ((ch == 0) ? 1.0 : 1.08));

            if (idx >= 0 && idx < length)
                out[idx] += reflectionGain[k] * ((ch == 0) ? 1.0f : 0.92f);
        }

        // ---- diffuse, filtered, exponentially decaying noise tail -----------
        const float lpCoeff = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * spec.lowPassHz / sampleRate));
        const float hpCoeff = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 220.0 / sampleRate));
        const float envStep = (float) (1.0 / (spec.noiseDecaySeconds * sampleRate));

        float lpState = 0.0f, hpState = 0.0f;

        for (int i = preDelaySamples; i < length; ++i)
        {
            const float t   = (float) (i - preDelaySamples);
            const float env = std::exp (-t * envStep);
            const float n   = nextRandom() * env;

            lpState += lpCoeff * (n - lpState);
            hpState += hpCoeff * (lpState - hpState);

            out[i] += (float) spec.noiseLevel * (lpState - hpState);
        }

        // ---- overall shaping -------------------------------------------------
        Biquad highPass, lowPass;
        highPass.setHighPass (sampleRate, spec.highPassHz, 0.7071);
        lowPass.setLowPass   (sampleRate, spec.lowPassHz,  0.7071);

        for (int i = 0; i < length; ++i)
            out[i] = lowPass.process (highPass.process (out[i]));

        // ---- fade out the tail so it can never click --------------------------
        const int fade = juce::jmin (512, length / 4);

        for (int i = 0; i < fade; ++i)
            out[length - fade + i] *= (float) i / (float) fade;

        // ---- level ---------------------------------------------------------
        //      The cabinet is scaled so that the average magnitude of its
        //      response over 200 Hz .. 4 kHz (the body of an electric guitar)
        //      sits at 0 dB.  Unit energy normalisation - the obvious first
        //      choice - puts the modal peaks of a cabinet 15-20 dB above the
        //      mid band, which silently adds a huge amount of gain on
        //      sustained notes and drives the output limiter permanently;
        //      peak normalising instead buries the mid band 15 dB down and
        //      makes the plug-in far too quiet.  Averaging over the body of
        //      the instrument keeps the gain structure predictable, and the
        //      loudest point is then capped at +12 dB so that the low
        //      resonances cannot run away either.
        //
        //      The FFT runs on the loader thread, never on the audio thread.
        constexpr int fftOrder = 15;
        constexpr int fftSize  = 1 << fftOrder;         // 32768, twice the longest IR

        double normalise = 1.0;

        if (length <= fftSize)
        {
            // juce::dsp::FFT wants the real samples packed into the first half
            // of a 2 * size buffer; the second half receives the spectrum.
            std::vector<float> fftData ((size_t) fftSize * 2, 0.0f);

            for (int i = 0; i < length; ++i)
                fftData[(size_t) i] = out[i];

            juce::dsp::FFT fft (fftOrder);
            fft.performRealOnlyForwardTransform (fftData.data());

            const double binHz = sampleRate / (double) fftSize;

            double peakMagnitude = 0.0;
            double logSum = 0.0;
            int    bandBins = 0;

            for (int i = 0; i <= fftSize / 2; ++i)
            {
                const double re  = (double) fftData[(size_t) i * 2];
                const double im  = (double) fftData[(size_t) i * 2 + 1];
                const double mag = std::sqrt (re * re + im * im);

                peakMagnitude = juce::jmax (peakMagnitude, mag);

                const double frequency = (double) i * binHz;

                if (frequency >= 200.0 && frequency <= 4000.0)
                {
                    logSum += std::log (mag + 1.0e-12);
                    ++bandBins;
                }
            }

            const double bandMean = bandBins > 0 ? std::exp (logSum / (double) bandBins) : peakMagnitude;

            if (bandMean > 1.0e-9)
                normalise = 1.0 / bandMean;

            if (peakMagnitude * normalise > 4.0 && peakMagnitude > 1.0e-9)
                normalise = 4.0 / peakMagnitude;

        }

        const float gain = (float) (normalise * juce::Decibels::decibelsToGain ((double) spec.trimDb));

        for (int i = 0; i < length; ++i)
            out[i] *= gain;
    }

    return ir;
}

} // namespace ampforge

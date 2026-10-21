/*  AmpForge - cabinet simulation.
 *
 *  Three cabinet profiles are generated procedurally (deterministic, so every
 *  build sounds identical and no binary assets are needed), plus an optional
 *  user impulse response loaded from a wav/aiff/flac file.
 *
 *  Threading model
 *  ---------------
 *  * The audio thread only ever calls process(), which runs the convolution
 *    engine and a dry/wet cross fade.  No allocation, no locks, no file I/O.
 *  * Generating an IR, or decoding an IR file, is done on a dedicated loader
 *    thread.  The finished IR is handed to the convolution engine with
 *    loadImpulseResponse(), which JUCE documents as wait-free, so the audio
 *    thread is never blocked.
 *  * prepareToPlay() applies the current IR synchronously; that is bounded
 *    arithmetic (a few thousand multiplies) with no disk access.
 */
#pragma once

#include <JuceHeader.h>
#include "Biquad.h"

namespace ampforge
{

class CabinetSim
{
public:
    enum class Profile
    {
        FourTwelveClosed = 0,
        TwoTwelveCombo,
        OneTwelveOpen,
        UserIR
    };

    static juce::String profileName (Profile p)
    {
        switch (p)
        {
            case Profile::FourTwelveClosed: return "4x12 Closed";
            case Profile::TwoTwelveCombo:   return "2x12 Combo";
            case Profile::OneTwelveOpen:    return "1x12 Open";
            case Profile::UserIR:           return "User IR";
        }
        return {};
    }

    //==========================================================================
    CabinetSim();
    ~CabinetSim();

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    void setEnabled (bool shouldBeEnabled) noexcept;
    bool isEnabled() const noexcept { return enabled.load(); }

    /** Ask for a built-in profile.  Cheap: the work happens on the loader thread. */
    void requestProfile (Profile p) noexcept { requestedProfile.store ((int) p); }

    /** Ask for a user IR file.  The file is decoded on the loader thread. */
    void requestUserIR (const juce::File& file);

    /** Real-time safe. */
    void process (const juce::dsp::ProcessContextReplacing<float>& context) noexcept;

    juce::String getCurrentIRName() const noexcept;
    bool isLoading() const noexcept      { return loading.load(); }
    bool hasUserIR() const noexcept      { return userIRValid.load(); }
    int  getLoadCounter() const noexcept { return loadCounter.load(); }
    int  getAppliedProfile() const noexcept { return appliedProfile.load(); }

    /** Builds one of the built-in impulse responses (deterministic). */
    static juce::AudioBuffer<float> generateProfile (Profile p, double sampleRate);

private:
    //==========================================================================
    class LoaderThread;

    friend class LoaderThread;

    void serviceRequests();
    void loadUserFile (const juce::File& file);
    void applyBuiltIn (Profile p);
    void applyStoredUserIR();

    //==========================================================================
    juce::dsp::Convolution convolution;

    std::atomic<int>  requestedProfile { -1 };
    std::atomic<int>  appliedProfile   { -1 };
    std::atomic<bool> enabled          { true };
    std::atomic<bool> userIRValid      { false };
    std::atomic<bool> loading          { false };
    std::atomic<int>  loadCounter      { 0 };
    std::atomic<int>  fileRequestToken { 0 };

    int lastFileToken = 0;

    /** Guards userIR / userIRRate / userIRName / pendingFile / currentIRName.
     *  IMPORTANT: this lock is never touched by the audio thread. */
    juce::SpinLock stateLock;

    juce::File pendingFile;
    juce::AudioBuffer<float> userIR;
    double userIRRate = 44100.0;
    juce::String userIRName;
    juce::String currentIRName;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> cabMix { 1.0f };
    juce::AudioBuffer<float> wetBuffer;

    std::unique_ptr<LoaderThread> loader;

    double sampleRate = 44100.0;
    bool prepared = false;
};

} // namespace ampforge

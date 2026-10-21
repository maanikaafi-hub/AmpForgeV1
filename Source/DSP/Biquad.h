/*  AmpForge - minimal, allocation free filter building blocks.
 *
 *  These are used instead of juce::dsp::IIR so that every filter coefficient
 *  change is a plain store: no reference counted allocation can ever happen
 *  inside the audio callback.
 *
 *  The designers use the well known Robert Bristow-Johnson
 *  "Audio EQ Cookbook" biquad formulas.
 */
#pragma once

#include <JuceHeader.h>

#include <cmath>
#include <algorithm>

namespace ampforge
{

//==============================================================================
/** Transposed direct form II biquad.  Real-time safe: setState() only writes
 *  five floats, process() never allocates and never blocks.
 */
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void reset() noexcept { z1 = 0.0f; z2 = 0.0f; }

    /** b/a are normalised by a0 inside this call. */
    void set (double b0n, double b1n, double b2n,
              double a0n, double a1n, double a2n) noexcept
    {
        const double inv = std::abs (a0n) < 1.0e-12 ? 1.0e12 : 1.0 / a0n;
        b0 = (float) (b0n * inv);
        b1 = (float) (b1n * inv);
        b2 = (float) (b2n * inv);
        a1 = (float) (a1n * inv);
        a2 = (float) (a2n * inv);
    }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    inline void makeBypass() noexcept { b0 = 1.0f; b1 = b2 = a1 = a2 = 0.0f; }

    //--------------------------------------------------------------------------
    void setLowPass (double fs, double freq, double q) noexcept
    {
        const double f = std::clamp (freq, 10.0, fs * 0.45);
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f / fs;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * std::max (0.05, q));
        set ((1.0 - cw) * 0.5, 1.0 - cw, (1.0 - cw) * 0.5,
             1.0 + alpha, -2.0 * cw, 1.0 - alpha);
    }

    void setHighPass (double fs, double freq, double q) noexcept
    {
        const double f = std::clamp (freq, 5.0, fs * 0.45);
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f / fs;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * std::max (0.05, q));
        set ((1.0 + cw) * 0.5, -(1.0 + cw), (1.0 + cw) * 0.5,
             1.0 + alpha, -2.0 * cw, 1.0 - alpha);
    }

    void setPeaking (double fs, double freq, double q, double gainDb) noexcept
    {
        const double f = std::clamp (freq, 10.0, fs * 0.45);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f / fs;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * std::max (0.05, q));
        set (1.0 + alpha * A, -2.0 * cw, 1.0 - alpha * A,
             1.0 + alpha / A, -2.0 * cw, 1.0 - alpha / A);
    }

    void setLowShelf (double fs, double freq, double gainDb) noexcept
    {
        const double f = std::clamp (freq, 10.0, fs * 0.45);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f / fs;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw * 0.5 * std::sqrt (2.0);
        const double sa = 2.0 * std::sqrt (A) * alpha;
        set (A * ((A + 1.0) - (A - 1.0) * cw + sa),
             2.0 * A * ((A - 1.0) - (A + 1.0) * cw),
             A * ((A + 1.0) - (A - 1.0) * cw - sa),
             (A + 1.0) + (A - 1.0) * cw + sa,
             -2.0 * ((A - 1.0) + (A + 1.0) * cw),
             (A + 1.0) + (A - 1.0) * cw - sa);
    }

    void setHighShelf (double fs, double freq, double gainDb) noexcept
    {
        const double f = std::clamp (freq, 10.0, fs * 0.45);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f / fs;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw * 0.5 * std::sqrt (2.0);
        const double sa = 2.0 * std::sqrt (A) * alpha;
        set (A * ((A + 1.0) + (A - 1.0) * cw + sa),
             -2.0 * A * ((A - 1.0) + (A + 1.0) * cw),
             A * ((A + 1.0) + (A - 1.0) * cw - sa),
             (A + 1.0) - (A - 1.0) * cw + sa,
             2.0 * ((A - 1.0) - (A + 1.0) * cw),
             (A + 1.0) - (A - 1.0) * cw - sa);
    }
};

//==============================================================================
/** One pole smoother / filter (6 dB per octave). */
struct OnePole
{
    float coeff = 1.0f;
    float state = 0.0f;

    void reset() noexcept { state = 0.0f; }

    void setLowPass (double fs, double freq) noexcept
    {
        const double f = std::clamp (freq, 5.0, fs * 0.45);
        coeff = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * f / fs));
    }

    /** Coeff for a pure smoothing/follower with the given time constant. */
    void setTimeConstant (double fs, double seconds) noexcept
    {
        const auto t = std::max (1.0e-6, seconds);
        coeff = (float) (1.0 - std::exp (-1.0 / (t * fs)));
    }

    inline float process (float x) noexcept
    {
        state += coeff * (x - state);
        return state;
    }
};

} // namespace ampforge

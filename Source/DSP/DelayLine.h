/*  AmpForge - fixed size circular delay line with interpolated reads. */
#pragma once

#include <JuceHeader.h>

#include <algorithm>
#include <vector>

namespace ampforge
{

/** A circular delay line whose storage is allocated once in prepare().
 *
 *  write()/read() are real-time safe: no allocation, no locks, no I/O.
 */
class DelayLine
{
public:
    void setSize (int newSize)
    {
        buffer.assign ((size_t) std::max (8, newSize), 0.0f);
        writeIndex = 0;
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

    int getSize() const noexcept { return (int) buffer.size(); }

    inline void write (float value) noexcept
    {
        buffer[(size_t) writeIndex] = value;

        if (++writeIndex >= (int) buffer.size())
            writeIndex = 0;
    }

    /** Reads the sample written `delayInSamples` ago.  Fractional delays are
     *  linearly interpolated. */
    inline float read (float delayInSamples) const noexcept
    {
        const int size = (int) buffer.size();

        if (size < 2)
            return 0.0f;

        float pos = (float) writeIndex - delayInSamples;

        while (pos < 0.0f)          pos += (float) size;
        while (pos >= (float) size) pos -= (float) size;

        const int i0 = (int) pos;
        const int i1 = (i0 + 1 >= size) ? 0 : i0 + 1;
        const float frac = pos - (float) i0;
        const float a = buffer[(size_t) i0];
        const float b = buffer[(size_t) i1];

        return a + frac * (b - a);
    }

private:
    std::vector<float> buffer { 8, 0.0f };
    int writeIndex = 0;
};

} // namespace ampforge

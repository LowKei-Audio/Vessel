#pragma once
// Circular delay buffer with fractional (Hermite) read.
// Hermite interpolation keeps modulated reads (chorus, tape glide) smooth
// without the high-frequency loss of linear interpolation.

#include <algorithm>
#include <cstddef>
#include <vector>

namespace vessel::dsp
{

class DelayLine
{
public:
    // Allocates enough room for maxDelaySamples plus interpolation headroom.
    // Call from prepare only, never on the audio thread.
    void prepare (std::size_t maxDelaySamples)
    {
        std::size_t size = 1;
        while (size < maxDelaySamples + 4)
            size <<= 1;                       // power of two -> cheap wraparound mask

        buffer.assign (size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    void push (float x) noexcept
    {
        buffer[writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    // Reads the signal `delay` samples back. Call after push(): delay 1 is the
    // most recent sample. Minimum is 2 so the interpolator's newer neighbour
    // (delay - 1) is always a sample that has actually been written.
    float read (float delay) const noexcept
    {
        delay = std::clamp (delay, 2.0f, maxDelay());

        const auto whole = static_cast<std::size_t> (delay);
        const float frac = delay - static_cast<float> (whole);

        // Four neighbours around the read point, newest to oldest.
        const std::size_t base = writePos + mask + 1 - whole; // +size avoids underflow
        const float y0 = buffer[(base + 1) & mask];
        const float y1 = buffer[(base)     & mask];
        const float y2 = buffer[(base - 1) & mask];
        const float y3 = buffer[(base - 2) & mask];

        // 4-point, 3rd-order Hermite between y1 (frac=0) and y2 (frac=1).
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * frac + c2) * frac + c1) * frac + y1;
    }

    float maxDelay() const noexcept { return static_cast<float> (mask - 3); }

private:
    std::vector<float> buffer { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    std::size_t mask = 7;
    std::size_t writePos = 0;
};

} // namespace vessel::dsp

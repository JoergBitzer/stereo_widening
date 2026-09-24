/**
 * @file MeterFifo.h
 * @brief Lock-free single-producer / single-consumer FIFO for goniometer sample points.
 *
 * The audio thread pushes one (x, y) point per sample (push() never allocates, never
 * blocks, and never waits). The GUI thread drains all currently available points on a
 * timer tick. If the GUI falls behind, new points are silently dropped (the FIFO does
 * not grow and does not overwrite unread data) -- a few missed scope points are
 * harmless, blocking the audio thread is not.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

class MeterFifo
{
public:
    /** capacity in points; default 16384 is about 340 ms at 48 kHz, far more than one
     *  GUI timer tick (typically 30-60 Hz) needs to drain. */
    explicit MeterFifo(int capacity = 1 << 14);

    /** Audio thread: push one point. Drops the point if the FIFO is full. */
    void push(float x, float y) noexcept;

    /** GUI thread: appends all currently available points to outX/outY (both cleared first). */
    void drainInto(std::vector<float>& outX, std::vector<float>& outY);

private:
    juce::AbstractFifo fifo;
    std::vector<float> bufferX, bufferY;
};

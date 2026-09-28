#include "EarlyReflections.h"
#include <cmath>

namespace
{
    bool hasChanged(float value, float lastValue) noexcept
    {
        return std::abs(value - lastValue) > 1.0e-6f;
    }
}

void EarlyReflections::prepare(double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;

    const int maxDelaySamples = juce::roundToInt(kMaxDelayMs * 0.001f * (float) sampleRate) + 1;
    midDelayLine.setMaximumDelayInSamples(maxDelaySamples);
    midDelayLine.prepare(juce::dsp::ProcessSpec { sampleRate, (juce::uint32) maxBlockSize, 1 });

    for (int k = 0; k < kNumReflections; ++k)
    {
        leftSmoothedDelaySamples[(size_t) k].reset(sampleRate, (double) kDelaySmoothingSeconds);
        rightSmoothedDelaySamples[(size_t) k].reset(sampleRate, (double) kDelaySmoothingSeconds);
        gains[(size_t) k] = tapGain(k);
    }

    lastRoomSize = -1.0f;
    delayInitialized = false; // force a snap (not a glide-in from 0) on the next process()
}

void EarlyReflections::reset()
{
    midDelayLine.reset();
    delayInitialized = false; // same reasoning as in prepare(): snap cleanly, don't glide in
}

void EarlyReflections::updateTapTargets(float roomSize, float preDelayMs) noexcept
{
    const float samplesPerMs = 0.001f * (float) sampleRate;

    for (int k = 0; k < kNumReflections; ++k)
    {
        const float leftDelaySamples = tapTimeMs(k, true, roomSize, preDelayMs) * samplesPerMs;
        const float rightDelaySamples = tapTimeMs(k, false, roomSize, preDelayMs) * samplesPerMs;

        if (!delayInitialized)
        {
            leftSmoothedDelaySamples[(size_t) k].setCurrentAndTargetValue(leftDelaySamples);
            rightSmoothedDelaySamples[(size_t) k].setCurrentAndTargetValue(rightDelaySamples);
        }
        else
        {
            leftSmoothedDelaySamples[(size_t) k].setTargetValue(leftDelaySamples);
            rightSmoothedDelaySamples[(size_t) k].setTargetValue(rightDelaySamples);
        }
    }
    delayInitialized = true;
}

void EarlyReflections::process(juce::AudioBuffer<float>& buffer, const AlgorithmParamValues& values) noexcept
{
    const float width = values[kWidth] * 0.01f; // % -> 0..2
    const float roomSize = values[kRoomSize] * 0.01f; // % -> 0..1
    const float preDelayMs = values[kPreDelay];
    if (!delayInitialized || hasChanged(roomSize, lastRoomSize) || hasChanged(preDelayMs, lastPreDelayMs))
    {
        updateTapTargets(roomSize, preDelayMs);
        lastRoomSize = roomSize;
        lastPreDelayMs = preDelayMs;
    }

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    const int numSamples = buffer.getNumSamples();
    const float amount = values[kAmount] * 0.01f; // % -> 0..1

    for (int i = 0; i < numSamples; ++i)
    {
        const float m = 0.5f * (left[i] + right[i]);
        midDelayLine.pushSample(0, m);

        // This shared delay line is read kNumReflections*2 times per pushed sample
        // (one popSample() per tap, both channels). popSample()'s updateReadPointer
        // controls its OWN internal read cursor, which must advance by EXACTLY ONE
        // step per pushed sample to stay locked to pushSample()'s write cursor --
        // exactly like the usual one-push/one-pop-per-sample pattern, just with the
        // single pop replaced by kNumReflections*2 of them. Passing updateReadPointer
        // = true on every call over-advances the read cursor (it free-runs ahead of
        // the write cursor, corrupting every tap's actual delay within a few dozen
        // samples); passing false on EVERY call never advances it at all, which is
        // just as wrong the other way (the read cursor freezes, so every tap reads a
        // fixed physical buffer slot instead of "N samples ago", refreshed only once
        // per buffer revolution -- audible as periodic crackle, not caught by this
        // project's statistical evaluation metrics since the stale, rarely-refreshed
        // samples are still real, correlated audio). The fix: false on every call
        // except the temporally LAST one this sample (here, R's final tap), which
        // advances the shared read cursor by the one step this sample needs (the
        // decrement in popSample() happens after its own read, so every read up to
        // and including that last call still sees the same, correctly-synced cursor
        // position -- see juce_DelayLine.cpp).
        float reflL = 0.0f;
        for (int k = 0; k < kNumReflections; ++k)
        {
            midDelayLine.setDelay(leftSmoothedDelaySamples[(size_t) k].getNextValue());
            reflL += gains[(size_t) k] * midDelayLine.popSample(0, -1.0f, false);
        }
        float reflR = 0.0f;
        for (int k = 0; k < kNumReflections; ++k)
        {
            const bool isLastTapThisSample = (k == kNumReflections - 1);
            midDelayLine.setDelay(rightSmoothedDelaySamples[(size_t) k].getNextValue());
            reflR += gains[(size_t) k] * midDelayLine.popSample(0, -1.0f, isLastTapThisSample);
        }

        // Amount blends the reflections in; Width is then a plain M/S width on the
        // result, like every other algorithm's (see the file header).
        const float lOut = left[i] + amount * reflL;
        const float rOut = right[i] + amount * reflR;
        const float midOut = 0.5f * (lOut + rOut);
        const float sideOut = width * 0.5f * (lOut - rOut);
        left[i] = midOut + sideOut;
        right[i] = midOut - sideOut;
    }
}

juce::String EarlyReflections::getDescription() const
{
    return "Early-reflection / room widening.\n\n"
           "Five short, quiet delayed copies of the mid signal are added to each "
           "channel -- a different set of delay times for L than for R, which is what "
           "creates the width, like the early reflections of a real room (\"apparent "
           "source width\"). This creates real width even from a mono source. Width "
           "then scales the side signal of the result.\n\n"
           + getControlsText() + "\n\n"
           "Display: an echogram -- the direct sound at 0 ms, L's reflections above the "
           "time axis (red), R's below it (blue), bar height = level in dB relative to "
           "the direct sound. Drag the Pre-delay and room-end lines sideways, up/down "
           "elsewhere for Amount.\n\n"
           "Not mono-compatible: L and R get different reflections, so the mono sum is "
           "coloured once Amount is above 0. Check your mix in mono (Utilities -> Monitor -> Mono Check).\n\n"
           "Source: L. Beranek, \"Concert Halls and Opera Houses: Music, Acoustics, "
           "and Architecture\", 2nd ed., Springer, 2004, ch. 2 (apparent source width "
           "via early lateral reflections).";
}

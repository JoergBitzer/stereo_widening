/**
 * @file StereoMeterState.h
 * @brief Real-time-safe stereo metering engine, shared by StereoAnalyzer and StereoWidener.
 *
 * processBlock() runs on the audio thread: no allocation, no locks (other than the
 * lock-free MeterFifo push). The GUI thread reads the meter values with the getters
 * below (plain atomic loads) and drains the goniometer FIFO on a timer.
 *
 * Notation matches python/stereo_eval: M = (L+R)/2, S = (L-R)/2 ("mid" and "side").
 * A mono input (1 channel) is treated as L = R, same as stereo_eval.audio_io.read_stereo.
 *
 * RMS and correlation use the same one-pole ("leaky integrator") time constant, so a
 * correlation meter and a level meter built on this class always agree on how fast they
 * react. Peak meters hold their value for peakHoldTime_s after the last new peak, then
 * decay linearly in dB per second, like a hardware peak meter.
 *
 * Cross-check against python/stereo_eval: the broadband correlation and the RMS levels
 * should match stereo_eval.measures within the difference given by the integration time
 * (this class is continuous/causal, stereo_eval.correlation() is a block average).
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

#include "MeterFifo.h"

class StereoMeterState
{
public:
    enum ChannelIndex { Left = 0, Right = 1, Mid = 2, Side = 3, NumChannels = 4 };

    StereoMeterState() = default;

    /** Call from prepareToPlay. rmsTimeConstant_s is used for both the RMS meters and
     *  the correlation meter (typical hardware correlation meters use 100 ms - 1 s). */
    void prepare(double sampleRate, float rmsTimeConstant_s = 0.3f, float peakDecay_dBPerSecond = 20.0f,
                 float peakHoldTime_s = 1.5f);

    /** Resets all running state to silence (e.g. on playback stop). Not audio-thread safe
     *  against a concurrent processBlock() call; call only when processing is stopped. */
    void reset();

    /** Change the peak ballistics without resetting the running RMS/correlation state
     *  (unlike calling prepare() again). Safe to call from the audio thread; not safe to
     *  call concurrently with processBlock() from a different thread (matches the
     *  Integration parameter's own "read once per block on the audio thread" pattern,
     *  see StereoAnalyzer.cpp). */
    void setPeakDecayRate(float dBPerSecond) noexcept;
    void setPeakHoldTime(float seconds) noexcept;

    /** Audio thread. Reads buffer (1 or 2 channels), updates all meters, and pushes one
     *  (S, M) goniometer point per sample. Does not modify buffer: purely an observer. */
    void processBlock(const juce::AudioBuffer<float>& buffer) noexcept;

    // ---- GUI thread ---------------------------------------------------------------
    float getRmsDb(ChannelIndex ch) const noexcept { return rmsDb[(int) ch].load(std::memory_order_relaxed); }
    float getPeakDb(ChannelIndex ch) const noexcept { return peakDb[(int) ch].load(std::memory_order_relaxed); }
    float getCorrelation() const noexcept { return correlation.load(std::memory_order_relaxed); }
    /** S - M in dB: 0 = equal power (typical for wide, uncorrelated material), very
     *  negative = narrow/mono, matches stereo_eval.measures.levels()["S_minus_M_dB"]. */
    float getWidthEstimateDb() const noexcept { return getRmsDb(Side) - getRmsDb(Mid); }

    MeterFifo& getGoniometerFifo() noexcept { return goniometerFifo; }

    static constexpr float minusInfDb = -144.0f;

private:
    double fs = 44100.0;
    float rmsAlpha = 0.0f;      // one-pole coefficient for RMS and correlation power sums
    float peakDecayFactor = 1.0f; // per-sample multiplicative peak decay, once the hold expires
    int peakHoldSamples = 0;      // samples a new peak is held before peakDecayFactor kicks in

    float sumSquare[NumChannels] { 0.0f, 0.0f, 0.0f, 0.0f }; // leaky mean of L^2, R^2, M^2, S^2
    float sumLR = 0.0f; // leaky mean of L * R, for the correlation numerator

    float peakLinear[NumChannels] { 0.0f, 0.0f, 0.0f, 0.0f };
    int peakHoldCounter[NumChannels] { 0, 0, 0, 0 }; // samples remaining before this channel's peak may decay

    std::atomic<float> rmsDb[NumChannels] { { minusInfDb }, { minusInfDb }, { minusInfDb }, { minusInfDb } };
    std::atomic<float> peakDb[NumChannels] { { minusInfDb }, { minusInfDb }, { minusInfDb }, { minusInfDb } };
    std::atomic<float> correlation { 0.0f };

    MeterFifo goniometerFifo;

    static float linearPowerToDb(float power) noexcept
    {
        return 10.0f * std::log10(std::max(power, 1.0e-12f));
    }
    static float linearAmplitudeToDb(float amplitude) noexcept
    {
        return 20.0f * std::log10(std::max(amplitude, 1.0e-12f));
    }
};

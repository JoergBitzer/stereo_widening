#include "StereoMeterState.h"

void StereoMeterState::prepare(double sampleRate, float rmsTimeConstant_s, float peakDecay_dBPerSecond,
                                float peakHoldTime_s)
{
    fs = sampleRate;
    // one-pole coefficient so that after rmsTimeConstant_s the response of a step input
    // has reached (1 - 1/e): alpha = exp(-1 / (tau * fs))
    rmsAlpha = std::exp(-1.0f / (rmsTimeConstant_s * (float) fs));
    setPeakDecayRate(peakDecay_dBPerSecond);
    setPeakHoldTime(peakHoldTime_s);
    reset();
}

void StereoMeterState::setPeakDecayRate(float dBPerSecond) noexcept
{
    // linear-amplitude decay factor so that the peak falls by dBPerSecond dB every
    // second, once the hold time has expired: 20*log10(factor^fs) = -dBPerSecond
    peakDecayFactor = std::pow(10.0f, -dBPerSecond / (20.0f * (float) fs));
}

void StereoMeterState::setPeakHoldTime(float seconds) noexcept
{
    peakHoldSamples = (int) std::lround(std::max(0.0f, seconds) * (float) fs);
}

void StereoMeterState::reset()
{
    for (int c = 0; c < NumChannels; ++c)
    {
        sumSquare[c] = 0.0f;
        peakLinear[c] = 0.0f;
        peakHoldCounter[c] = 0;
        rmsDb[c].store(minusInfDb, std::memory_order_relaxed);
        peakDb[c].store(minusInfDb, std::memory_order_relaxed);
    }
    sumLR = 0.0f;
    correlation.store(0.0f, std::memory_order_relaxed);
}

void StereoMeterState::processBlock(const juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    const float* left  = buffer.getReadPointer(0);
    const float* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : left; // mono: R = L

    const float oneMinusAlpha = 1.0f - rmsAlpha;

    for (int n = 0; n < numSamples; ++n)
    {
        const float l = left[n];
        const float r = right[n];
        const float m = 0.5f * (l + r);
        const float s = 0.5f * (l - r);
        const float values[NumChannels] = { l, r, m, s };

        for (int c = 0; c < NumChannels; ++c)
        {
            sumSquare[c] = rmsAlpha * sumSquare[c] + oneMinusAlpha * values[c] * values[c];

            const float absValue = std::abs(values[c]);
            if (absValue >= peakLinear[c])
            {
                // new peak: jump to it and (re)start the hold
                peakLinear[c] = absValue;
                peakHoldCounter[c] = peakHoldSamples;
            }
            else if (peakHoldCounter[c] > 0)
            {
                // still within the hold time: peak stays exactly where it is
                --peakHoldCounter[c];
            }
            else
            {
                // hold expired: decay towards the current signal
                peakLinear[c] *= peakDecayFactor;
            }
        }
        sumLR = rmsAlpha * sumLR + oneMinusAlpha * l * r;

        goniometerFifo.push(s, m);
    }

    for (int c = 0; c < NumChannels; ++c)
    {
        rmsDb[c].store(linearPowerToDb(sumSquare[c]), std::memory_order_relaxed);
        peakDb[c].store(linearAmplitudeToDb(peakLinear[c]), std::memory_order_relaxed);
    }
    // sumSquare[Left] and [Right] double as the correlation denominator (leaky L^2, R^2)
    const float denominator = std::sqrt(std::max(sumSquare[Left] * sumSquare[Right], 1.0e-12f));
    correlation.store(sumLR / denominator, std::memory_order_relaxed);
}

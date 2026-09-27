/**
 * @file BiquadResponse.h
 * @brief Frequency response of one biquad, for the algorithms' pure-math helpers that
 *        the GUI's displays draw (no GUI dependency).
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <array>
#include <complex>
#include <juce_audio_basics/juce_audio_basics.h>

/** H(e^jw) of one biquad at frequencyHz, coefficients in JUCE's {b0, b1, b2, a0, a1, a2}
 *  order (juce::dsp::IIR::ArrayCoefficients). */
inline std::complex<double> biquadResponse(const std::array<float, 6>& c, double frequencyHz, double sampleRate) noexcept
{
    const std::complex<double> z1 = std::polar(1.0, -juce::MathConstants<double>::twoPi * frequencyHz / sampleRate);
    const std::complex<double> z2 = z1 * z1;
    return ((double) c[0] + (double) c[1] * z1 + (double) c[2] * z2)
         / ((double) c[3] + (double) c[4] * z1 + (double) c[5] * z2);
}

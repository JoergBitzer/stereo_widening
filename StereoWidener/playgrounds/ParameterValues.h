/**
 * @file ParameterValues.h
 * @brief Small helpers for playground displays that set parameters directly (drag
 *        handles), in the parameter's own units.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace ParameterValues
{
    inline float current(const juce::RangedAudioParameter& param)
    {
        return param.convertFrom0to1(param.getValue());
    }

    inline float defaultValue(const juce::RangedAudioParameter& param)
    {
        return param.convertFrom0to1(param.getDefaultValue());
    }

    /** Always clamp before handing a value to a parameter: a log-frequency range's
     *  convertTo0to1() asserts on values outside its range (see makeLogFrequencyRange()
     *  in StereoWidener.cpp). */
    inline float clampToRange(const juce::RangedAudioParameter& param, float value)
    {
        const auto& range = param.getNormalisableRange();
        return juce::jlimit(range.start, range.end, value);
    }
}

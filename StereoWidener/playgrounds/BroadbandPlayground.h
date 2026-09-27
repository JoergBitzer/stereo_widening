/**
 * @file BroadbandPlayground.h
 * @brief Playground for MSWidthBroadband: the Width knob next to a picture of what it
 *        does to the stereo image -- where a hard-panned source is heard (tangent law,
 *        see MSWidthBroadband::hardPannedSourceAngleDeg()) and the side signal's gain.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#pragma once
#include "../AlgorithmPlayground.h"

/** Top-down view of a listener and two loudspeakers at +-30 degrees; the shaded wedge
 *  spans the directions hard-panned sources are heard from at the current width. */
class StereoImageView : public juce::Component
{
public:
    void setWidthPercent(float widthPercent);
    void setScaleFactor(float scale) { m_scale = scale; repaint(); }
    void paint(juce::Graphics& g) override;

private:
    float m_widthPercent = 100.0f;
    float m_scale = 1.0f;
};

class BroadbandPlayground : public AlgorithmPlayground
{
public:
    BroadbandPlayground(juce::AudioProcessorValueTreeState& apvts, const StereoAlgorithm& algorithm);
    void resized() override;

private:
    PlaygroundKnob m_widthKnob;
    StereoImageView m_imageView;
};

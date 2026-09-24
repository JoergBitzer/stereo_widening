/**
 * @file main.cpp
 * @brief Headless cross-check: feeds a wav file through StereoMeterState in fixed-size
 *        blocks (as a host would) and prints the final meter values.
 *
 * Usage: MeterCrossCheck <file.wav> [blockSize] [tau_s]
 *
 * Compare the printed values against python/stereo_eval/measures.py for the same file
 * (see python/crosscheck_meter.py). StereoMeterState is a continuous leaky integrator
 * (like a hardware meter), while stereo_eval.correlation() is the true broadband
 * correlation over the whole file, so they only agree closely for signals that are
 * stationary over several times the integration time tau_s (e.g. the noise test
 * signals). For non-stationary material (speech, mixes) the leaky value mostly
 * reflects the last ~tau_s seconds, which is the expected, intended behaviour of a
 * real-time meter, not a bug.
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>

#include "shared/metering/StereoMeterState.h"

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "usage: MeterCrossCheck <file.wav> [blockSize=512] [tau_s=0.3]\n";
        return 1;
    }

    const juce::File file(argv[1]);
    const int blockSize = argc > 2 ? std::atoi(argv[2]) : 512;
    const float tau_s = argc > 3 ? (float) std::atof(argv[3]) : 0.3f;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
    {
        std::cerr << "could not open " << file.getFullPathName() << "\n";
        return 1;
    }

    const int numChannels = (int) reader->numChannels;
    const int numSamples = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> fullBuffer(numChannels, numSamples);
    reader->read(&fullBuffer, 0, numSamples, 0, true, true);

    StereoMeterState meterState;
    meterState.prepare(reader->sampleRate, tau_s);

    for (int start = 0; start < numSamples; start += blockSize)
    {
        const int thisBlockSize = std::min(blockSize, numSamples - start);
        juce::AudioBuffer<float> block(fullBuffer.getArrayOfWritePointers(), numChannels, start, thisBlockSize);
        meterState.processBlock(block);
    }

    std::cout << "file=" << file.getFileName() << " fs=" << reader->sampleRate
              << " samples=" << numSamples << " channels=" << numChannels
              << " blockSize=" << blockSize << " tau_s=" << tau_s << "\n";
    std::cout << "correlation=" << meterState.getCorrelation() << "\n";
    std::cout << "rms_L_dB=" << meterState.getRmsDb(StereoMeterState::Left)
              << " rms_R_dB=" << meterState.getRmsDb(StereoMeterState::Right)
              << " rms_M_dB=" << meterState.getRmsDb(StereoMeterState::Mid)
              << " rms_S_dB=" << meterState.getRmsDb(StereoMeterState::Side) << "\n";
    std::cout << "peak_L_dB=" << meterState.getPeakDb(StereoMeterState::Left)
              << " peak_R_dB=" << meterState.getPeakDb(StereoMeterState::Right)
              << " peak_M_dB=" << meterState.getPeakDb(StereoMeterState::Mid)
              << " peak_S_dB=" << meterState.getPeakDb(StereoMeterState::Side) << "\n";
    std::cout << "width_S_minus_M_dB=" << meterState.getWidthEstimateDb() << "\n";

    return 0;
}

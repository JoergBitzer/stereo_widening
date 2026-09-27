/**
 * @file main.cpp
 * @brief Renders a wav file through one of StereoWidener's real algorithm classes
 *        (MSWidthBroadband, MSWidthFiltered, ComplementaryComb, AllpassDecorrelation,
 *        MultibandWidth, EarlyReflections or ChorusDoubler -- the exact C++ code the
 *        plugin runs, not a re-implementation) and writes the result to another wav
 *        file.
 *
 * Usage: WidenerRender <input.wav> <output.wav> <broadband|filtered|comb|allpass|multiband|earlyrefl|chorus>
 *                       [width_percent=100] [bassCutoffHz=150] [highShelfHz=8000]
 *                       [combDelayMs=10] [combGainPercent=50] [combCrossoverHz=300]
 *                       [allpassAmountPercent=50] [allpassSpreadPercent=50]
 *                       [mbFreq1=150] [mbFreq2=1500] [mbFreq3=6000]
 *                       [mbWidth2Percent=100] [mbWidth3Percent=100] [mbWidth4Percent=100]
 *                       [erAmountPercent=50] [erRoomSizePercent=50] [erPreDelayMs=5]
 *                       [chorusAmountPercent=50] [chorusDepthPercent=50] [chorusRateHz=0.3]
 *                       [highShelfGainDb=3]
 *
 * width_percent: 0-200, matching the plugin's Width parameter (0 = mono, 100 = unity,
 * 200 = double the side signal). bassCutoffHz/highShelfHz only matter for "filtered";
 * pass a value in their "Off" zone (e.g. bassCutoffHz below 40, or highShelfHz above
 * 16000) to bypass that stage, same as turning the corresponding knob to Off in the
 * plugin (see MSWidthFiltered.h); highShelfGainDb (last, added later) sets the
 * shelf's gain. combDelayMs/combGainPercent/combCrossoverHz only
 * matter for "comb" (see ComplementaryComb.h).
 * allpassAmountPercent/allpassSpreadPercent only matter for "allpass" (see
 * AllpassDecorrelation.h). mbFreq1/2/3 and mbWidth2/3/4Percent only matter for
 * "multiband" (see MultibandWidth.h); band 1's width is always 0, not a parameter,
 * and width_percent is ignored for "multiband" (it has no overall Width since v0.1.20).
 * erAmountPercent/erRoomSizePercent/erPreDelayMs only matter for "earlyrefl" (see
 * EarlyReflections.h). chorusAmountPercent/chorusDepthPercent/
 * chorusRateHz only matter for "chorus" (see ChorusDoubler.h); chorusRateHz mirrors
 * GlobalSettings' own default (not a user-facing knob in the plugin itself).
 *
 * python/evaluate_widener_plugin.py calls this once per (signal, setting) pair, then
 * runs python/stereo_eval's report.evaluate() on the resulting (input, output) file
 * pair -- the same measures used throughout this project, applied to the actual plugin
 * DSP instead of the Python reference implementation in python/algorithms/ms_width.py
 * (see Phase 3, "Render the test signals through the plugin and run the evaluation
 * report", plan2.md).
 *
 * (c) J. Bitzer, Jade HS, MIT license
 */

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <memory>

#include "algorithms/MSWidthBroadband.h"
#include "algorithms/MSWidthFiltered.h"
#include "algorithms/ComplementaryComb.h"
#include "algorithms/AllpassDecorrelation.h"
#include "algorithms/MultibandWidth.h"
#include "algorithms/EarlyReflections.h"
#include "algorithms/ChorusDoubler.h"

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        std::cerr << "usage: WidenerRender <input.wav> <output.wav> <broadband|filtered|comb|allpass|multiband|earlyrefl|chorus> "
                     "[width_percent=100] [bassCutoffHz=150] [highShelfHz=8000] "
                     "[combDelayMs=10] [combGainPercent=50] [combCrossoverHz=300] "
                     "[allpassAmountPercent=50] [allpassSpreadPercent=50] "
                     "[mbFreq1=150] [mbFreq2=1500] [mbFreq3=6000] "
                     "[mbWidth2Percent=100] [mbWidth3Percent=100] [mbWidth4Percent=100] "
                     "[erAmountPercent=50] [erRoomSizePercent=50] [erPreDelayMs=5] "
                     "[chorusAmountPercent=50] [chorusDepthPercent=50] [chorusRateHz=0.3]\n";
        return 1;
    }

    // getChildFile (rather than the File(String) constructor) accepts a relative path,
    // resolving it against the current working directory; an absolute path still works
    // unchanged either way
    const juce::File inputFile = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    const juce::File outputFile = juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]);
    const juce::String algorithmName(argv[3]);
    const float widthPercent = argc > 4 ? (float) std::atof(argv[4]) : 100.0f;
    const float bassCutoffHz = argc > 5 ? (float) std::atof(argv[5]) : 150.0f;
    const float highShelfHz = argc > 6 ? (float) std::atof(argv[6]) : 8000.0f;
    const float combDelayMs = argc > 7 ? (float) std::atof(argv[7]) : 10.0f;
    const float combGainPercent = argc > 8 ? (float) std::atof(argv[8]) : 50.0f;
    const float combCrossoverHz = argc > 9 ? (float) std::atof(argv[9]) : 300.0f;
    const float allpassAmountPercent = argc > 10 ? (float) std::atof(argv[10]) : 50.0f;
    const float allpassSpreadPercent = argc > 11 ? (float) std::atof(argv[11]) : 50.0f;
    const float mbFreq1 = argc > 12 ? (float) std::atof(argv[12]) : 150.0f;
    const float mbFreq2 = argc > 13 ? (float) std::atof(argv[13]) : 1500.0f;
    const float mbFreq3 = argc > 14 ? (float) std::atof(argv[14]) : 6000.0f;
    const float mbWidth2Percent = argc > 15 ? (float) std::atof(argv[15]) : 100.0f;
    const float mbWidth3Percent = argc > 16 ? (float) std::atof(argv[16]) : 100.0f;
    const float mbWidth4Percent = argc > 17 ? (float) std::atof(argv[17]) : 100.0f;
    const float erAmountPercent = argc > 18 ? (float) std::atof(argv[18]) : 50.0f;
    const float erRoomSizePercent = argc > 19 ? (float) std::atof(argv[19]) : 50.0f;
    const float erPreDelayMs = argc > 20 ? (float) std::atof(argv[20]) : 5.0f;
    const float chorusAmountPercent = argc > 21 ? (float) std::atof(argv[21]) : 50.0f;
    const float chorusDepthPercent = argc > 22 ? (float) std::atof(argv[22]) : 50.0f;
    const float chorusRateHz = argc > 23 ? (float) std::atof(argv[23]) : 0.3f;
    const float highShelfGainDb = argc > 24 ? (float) std::atof(argv[24]) : 3.0f;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(inputFile));
    if (reader == nullptr)
    {
        std::cerr << "could not open " << inputFile.getFullPathName() << "\n";
        return 1;
    }

    const int numSamples = (int) reader->lengthInSamples;
    const int sourceChannels = (int) reader->numChannels;

    // matches python/stereo_eval/audio_io.read_stereo: mono input duplicated to L=R,
    // since the algorithm classes require exactly 2 channels (algorithms/StereoAlgorithm.h)
    juce::AudioBuffer<float> buffer(2, numSamples);
    if (sourceChannels == 1)
    {
        juce::AudioBuffer<float> monoBuffer(1, numSamples);
        reader->read(&monoBuffer, 0, numSamples, 0, true, false);
        buffer.copyFrom(0, 0, monoBuffer, 0, 0, numSamples);
        buffer.copyFrom(1, 0, monoBuffer, 0, 0, numSamples);
    }
    else if (sourceChannels == 2)
    {
        reader->read(&buffer, 0, numSamples, 0, true, true);
    }
    else
    {
        std::cerr << inputFile.getFullPathName() << ": " << sourceChannels << " channels, expected 1 or 2\n";
        return 1;
    }

    std::unique_ptr<StereoAlgorithm> algorithm;
    if (algorithmName == "broadband")
        algorithm = std::make_unique<MSWidthBroadband>();
    else if (algorithmName == "filtered")
        algorithm = std::make_unique<MSWidthFiltered>();
    else if (algorithmName == "comb")
        algorithm = std::make_unique<ComplementaryComb>();
    else if (algorithmName == "allpass")
        algorithm = std::make_unique<AllpassDecorrelation>();
    else if (algorithmName == "multiband")
        algorithm = std::make_unique<MultibandWidth>();
    else if (algorithmName == "earlyrefl")
        algorithm = std::make_unique<EarlyReflections>();
    else if (algorithmName == "chorus")
        algorithm = std::make_unique<ChorusDoubler>();
    else
    {
        std::cerr << "unknown algorithm '" << algorithmName << "', expected broadband, filtered, comb, allpass, multiband, earlyrefl or chorus\n";
        return 1;
    }

    if (auto* chorus = dynamic_cast<ChorusDoubler*>(algorithm.get()))
        chorus->setRateHz(chorusRateHz);

    constexpr int blockSize = 512;
    algorithm->prepare(reader->sampleRate, blockSize);
    algorithm->reset();

    // Same units as the plugin's parameters (see each algorithm's getParamSpecs()):
    // percentages as 0-100, frequencies in Hz, times in ms.
    AlgorithmParamValues values {};
    if (algorithmName == "broadband")
    {
        values[MSWidthBroadband::kWidth] = widthPercent;
    }
    else if (algorithmName == "comb")
    {
        values[ComplementaryComb::kWidth] = widthPercent;
        values[ComplementaryComb::kDelay] = combDelayMs;
        values[ComplementaryComb::kGain] = combGainPercent;
        values[ComplementaryComb::kCrossover] = combCrossoverHz;
    }
    else if (algorithmName == "allpass")
    {
        values[AllpassDecorrelation::kWidth] = widthPercent;
        values[AllpassDecorrelation::kAmount] = allpassAmountPercent;
        values[AllpassDecorrelation::kSpread] = allpassSpreadPercent;
    }
    else if (algorithmName == "multiband")
    {
        values[MultibandWidth::kFreq1] = mbFreq1;
        values[MultibandWidth::kFreq2] = mbFreq2;
        values[MultibandWidth::kFreq3] = mbFreq3;
        values[MultibandWidth::kWidth2] = mbWidth2Percent;
        values[MultibandWidth::kWidth3] = mbWidth3Percent;
        values[MultibandWidth::kWidth4] = mbWidth4Percent;
    }
    else if (algorithmName == "earlyrefl")
    {
        values[EarlyReflections::kWidth] = widthPercent;
        values[EarlyReflections::kAmount] = erAmountPercent;
        values[EarlyReflections::kRoomSize] = erRoomSizePercent;
        values[EarlyReflections::kPreDelay] = erPreDelayMs;
    }
    else if (algorithmName == "chorus")
    {
        values[ChorusDoubler::kWidth] = widthPercent;
        values[ChorusDoubler::kAmount] = chorusAmountPercent;
        values[ChorusDoubler::kDepth] = chorusDepthPercent;
    }
    else // filtered
    {
        values[MSWidthFiltered::kWidth] = widthPercent;
        values[MSWidthFiltered::kBassCutoff] = bassCutoffHz;
        values[MSWidthFiltered::kHighShelf] = highShelfHz;
        values[MSWidthFiltered::kShelfGain] = highShelfGainDb;
    }

    // block-sized processing, not one giant call, so MSWidthFiltered's per-sample IIR
    // filter state behaves exactly as it would inside the real plugin's
    // processSynchronBlock() (host-sized blocks), not artificially reset every call
    for (int start = 0; start < numSamples; start += blockSize)
    {
        const int thisBlockSize = std::min(blockSize, numSamples - start);
        juce::AudioBuffer<float> block(buffer.getArrayOfWritePointers(), 2, start, thisBlockSize);
        algorithm->process(block, values);
    }

    outputFile.getParentDirectory().createDirectory();
    outputFile.deleteFile(); // FileOutputStream appends to an existing file otherwise

    // 32-bit float, matching python/stereo_eval/audio_io.write_stereo (subtype="FLOAT"),
    // so no quantisation is added beyond what the algorithm itself does
    std::unique_ptr<juce::AudioFormatWriter> writer(
        juce::WavAudioFormat().createWriterFor(new juce::FileOutputStream(outputFile),
                                                reader->sampleRate, 2, 32, {}, 0));
    if (writer == nullptr)
    {
        std::cerr << "could not create " << outputFile.getFullPathName() << "\n";
        return 1;
    }
    writer->writeFromAudioSampleBuffer(buffer, 0, numSamples);

    if (algorithmName == "comb")
        std::cout << "wrote " << outputFile.getFullPathName() << " (comb, width=" << widthPercent
                   << "%, delay=" << combDelayMs << " ms, gain=" << combGainPercent
                   << "%, crossover=" << combCrossoverHz << " Hz)\n";
    else if (algorithmName == "allpass")
        std::cout << "wrote " << outputFile.getFullPathName() << " (allpass, width=" << widthPercent
                   << "%, amount=" << allpassAmountPercent << "%, spread=" << allpassSpreadPercent << "%)\n";
    else if (algorithmName == "multiband")
        std::cout << "wrote " << outputFile.getFullPathName() << " (multiband, width=" << widthPercent
                   << "%, freqs=" << mbFreq1 << "/" << mbFreq2 << "/" << mbFreq3
                   << " Hz, widths=" << mbWidth2Percent << "/" << mbWidth3Percent << "/" << mbWidth4Percent << "%)\n";
    else if (algorithmName == "earlyrefl")
        std::cout << "wrote " << outputFile.getFullPathName() << " (earlyrefl, width=" << widthPercent
                   << "%, amount=" << erAmountPercent << "%, roomSize=" << erRoomSizePercent
                   << "%, preDelay=" << erPreDelayMs << " ms)\n";
    else if (algorithmName == "chorus")
        std::cout << "wrote " << outputFile.getFullPathName() << " (chorus, width=" << widthPercent
                   << "%, amount=" << chorusAmountPercent << "%, depth=" << chorusDepthPercent
                   << "%, rate=" << chorusRateHz << " Hz)\n";
    else
        std::cout << "wrote " << outputFile.getFullPathName() << " (" << algorithmName
                   << ", width=" << widthPercent << "%, bassCutoff=" << bassCutoffHz
                   << " Hz, highShelf=" << highShelfHz << " Hz)\n";
    return 0;
}

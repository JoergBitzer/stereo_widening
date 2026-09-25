/**
 * @file main.cpp
 * @brief Renders a wav file through one of StereoWidener's real algorithm classes
 *        (MSWidthBroadband, MSWidthFiltered or ComplementaryComb -- the exact C++ code
 *        the plugin runs, not a re-implementation) and writes the result to another wav
 *        file.
 *
 * Usage: WidenerRender <input.wav> <output.wav> <broadband|filtered|comb>
 *                       [width_percent=100] [bassCutoffHz=150] [highShelfHz=8000]
 *                       [combDelayMs=10] [combGainPercent=50] [combCrossoverHz=300]
 *
 * width_percent: 0-200, matching the plugin's Width parameter (0 = mono, 100 = unity,
 * 200 = double the side signal). bassCutoffHz/highShelfHz only matter for "filtered";
 * pass a value in their "Off" zone (e.g. bassCutoffHz below 40, or highShelfHz above
 * 16000) to bypass that stage, same as turning the corresponding knob to Off in the
 * plugin (see MSWidthFiltered.h). combDelayMs/combGainPercent/combCrossoverHz only
 * matter for "comb" (see ComplementaryComb.h); combCrossoverHz mirrors GlobalSettings'
 * combCrossoverHz default (not a user-facing knob in the plugin itself).
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

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        std::cerr << "usage: WidenerRender <input.wav> <output.wav> <broadband|filtered|comb> "
                     "[width_percent=100] [bassCutoffHz=150] [highShelfHz=8000] "
                     "[combDelayMs=10] [combGainPercent=50] [combCrossoverHz=300]\n";
        return 1;
    }

    // getChildFile (rather than the File(String) constructor) accepts a relative path,
    // resolving it against the current working directory; an absolute path still works
    // unchanged either way
    const juce::File inputFile = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    const juce::File outputFile = juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]);
    const juce::String algorithmName(argv[3]);
    const float width = (argc > 4 ? (float) std::atof(argv[4]) : 100.0f) * 0.01f;
    const float bassCutoffHz = argc > 5 ? (float) std::atof(argv[5]) : 150.0f;
    const float highShelfHz = argc > 6 ? (float) std::atof(argv[6]) : 8000.0f;
    const float combDelayMs = argc > 7 ? (float) std::atof(argv[7]) : 10.0f;
    const float combGainPercent = argc > 8 ? (float) std::atof(argv[8]) : 50.0f;
    const float combCrossoverHz = argc > 9 ? (float) std::atof(argv[9]) : 300.0f;

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
    else
    {
        std::cerr << "unknown algorithm '" << algorithmName << "', expected broadband, filtered or comb\n";
        return 1;
    }

    if (auto* comb = dynamic_cast<ComplementaryComb*>(algorithm.get()))
        comb->setCrossoverHz(combCrossoverHz);

    constexpr int blockSize = 512;
    algorithm->prepare(reader->sampleRate, blockSize);
    algorithm->reset();

    StereoAlgorithmParams params;
    params.width = width;
    if (algorithmName == "comb")
    {
        params.auxLeft = combDelayMs;
        params.auxRight = combGainPercent * 0.01f;
    }
    else
    {
        params.auxLeft = bassCutoffHz;
        params.auxRight = highShelfHz;
    }

    // block-sized processing, not one giant call, so MSWidthFiltered's per-sample IIR
    // filter state behaves exactly as it would inside the real plugin's
    // processSynchronBlock() (host-sized blocks), not artificially reset every call
    for (int start = 0; start < numSamples; start += blockSize)
    {
        const int thisBlockSize = std::min(blockSize, numSamples - start);
        juce::AudioBuffer<float> block(buffer.getArrayOfWritePointers(), 2, start, thisBlockSize);
        algorithm->process(block, params);
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
        std::cout << "wrote " << outputFile.getFullPathName() << " (comb, width=" << (width * 100.0f)
                   << "%, delay=" << combDelayMs << " ms, gain=" << combGainPercent
                   << "%, crossover=" << combCrossoverHz << " Hz)\n";
    else
        std::cout << "wrote " << outputFile.getFullPathName() << " (" << algorithmName
                   << ", width=" << (width * 100.0f) << "%, bassCutoff=" << bassCutoffHz
                   << " Hz, highShelf=" << highShelfHz << " Hz)\n";
    return 0;
}

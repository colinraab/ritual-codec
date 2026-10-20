// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <stdexcept>

namespace ritual_app
{
inline void validateInput (const juce::AudioFormatReader& reader)
{
    if (reader.numChannels < 1 || reader.numChannels > 2 || reader.lengthInSamples <= 0
        || reader.sampleRate < 8000 || reader.sampleRate > 192000)
        throw std::runtime_error ("Choose a mono or stereo file between 8 and 192 kHz.");
}

// Streams bounded blocks and removes the processor's fixed latency. The result has
// the source's channel count, sample rate and exact number of frames.
inline void renderWav (const juce::File& source, const juce::File& destination,
                       const juce::MemoryBlock& state, std::atomic<bool>& cancel,
                       std::atomic<float>& progress)
{
    if (source == destination || destination.exists())
        throw std::runtime_error ("Choose a new output filename.");
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (source));
    if (! reader) throw std::runtime_error ("Cannot read the source file.");
    validateInput (*reader);
    if (reader->lengthInSamples > (0xffffffffLL - 4096) / (reader->numChannels * 4))
        throw std::runtime_error ("The output exceeds WAV's 4 GB limit.");

    RitualAudioProcessor processor;
    const auto channels = static_cast<int> (reader->numChannels);
    const auto layout = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    if (! processor.setBusesLayout ({ { layout }, { layout } }))
        throw std::runtime_error ("Unsupported channel layout.");
    processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    processor.setRateAndBufferSizeDetails (reader->sampleRate, 512);
    processor.prepareToPlay (reader->sampleRate, 512);
    std::unique_ptr<juce::OutputStream> output (destination.createOutputStream());
    if (! output) throw std::runtime_error ("Cannot create the output file.");
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (output, juce::AudioFormatWriterOptions()
        .withSampleRate (reader->sampleRate).withNumChannels (channels)
        .withBitsPerSample (32)
        .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
    if (! writer) throw std::runtime_error ("Cannot create a WAV writer.");
    juce::AudioBuffer<float> block (channels, 512);
    juce::MidiBuffer midi;
    const auto latency = processor.getLatencySamples();
    const auto total = reader->lengthInSamples + latency;
    for (juce::int64 position = 0; position < total;)
    {
        if (cancel.load()) throw std::runtime_error ("Export cancelled.");
        const auto count = static_cast<int> (juce::jmin<juce::int64> (512, total - position));
        block.setSize (channels, count, false, false, true);
        block.clear();
        const auto inputCount = static_cast<int> (juce::jmax<juce::int64> (0,
            juce::jmin<juce::int64> (count, reader->lengthInSamples - position)));
        if (inputCount > 0 && ! reader->read (&block, 0, inputCount, position, true, true))
            throw std::runtime_error ("Reading the source failed.");
        processor.processBlock (block, midi);
        const auto skip = static_cast<int> (juce::jlimit<juce::int64> (0, count, latency - position));
        if (count > skip && ! writer->writeFromAudioSampleBuffer (block, skip, count - skip))
            throw std::runtime_error ("Writing failed. Check available disk space.");
        position += count;
        progress.store (static_cast<float> (position) / static_cast<float> (total));
    }
    if (! writer->flush()) throw std::runtime_error ("Could not finish the WAV file.");
}
}

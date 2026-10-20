// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Render.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include <thread>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        const juce::File root (argc > 1 ? argv[1] : juce::File::getCurrentWorkingDirectory().getFullPathName());
        const auto directory = root.getChildFile ("standalone-render-" + juce::Uuid().toString());
        if (! directory.createDirectory()) throw std::runtime_error ("Cannot create test folder");
        std::atomic<bool> cancel { false };
        std::atomic<float> progress { 0 };
        for (const auto rate : { 44100, 48000, 96000 })
            for (const auto channels : { 1, 2 })
            {
                const auto name = juce::String (rate) + "-" + juce::String (channels);
                const auto source = directory.getChildFile (name + "-source.wav");
                const auto output = directory.getChildFile (name + "-output.wav");
                juce::AudioBuffer<float> input (channels, 12345);
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < input.getNumSamples(); ++i)
                        input.setSample (c, i, 0.3f * std::sin (static_cast<float> (i) * 0.03f + c));
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::OutputStream> stream (source.createOutputStream());
                auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions()
                    .withSampleRate (rate).withNumChannels (channels).withBitsPerSample (32)
                    .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
                if (! writer || ! writer->writeFromAudioSampleBuffer (input, 0, input.getNumSamples()))
                    throw std::runtime_error ("Fixture write failed");
                writer.reset();
                RitualAudioProcessor processor;
                processor.parameters.getParameter ("bypass")->setValueNotifyingHost (1.0f);
                juce::MemoryBlock state;
                processor.getStateInformation (state);
                ritual_app::renderWav (source, output, state, cancel, progress);
                juce::AudioFormatManager formats;
                formats.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (output));
                if (! reader || reader->lengthInSamples != input.getNumSamples()
                    || reader->numChannels != static_cast<unsigned> (channels) || reader->sampleRate != rate)
                    throw std::runtime_error ("Export duration or format mismatch");
                juce::AudioBuffer<float> result (channels, input.getNumSamples());
                reader->read (&result, 0, result.getNumSamples(), 0, true, true);
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < input.getNumSamples(); ++i)
                        if (std::abs (result.getSample (c, i) - input.getSample (c, i)) > 0.000001f)
                            throw std::runtime_error ("Bypass latency compensation mismatch");
                bool protectedOutput = false;
                try { ritual_app::renderWav (source, output, state, cancel, progress); }
                catch (const std::exception&) { protectedOutput = true; }
                if (! protectedOutput) throw std::runtime_error ("Existing output was overwritten");
                processor.parameters.getParameter ("bypass")->setValueNotifyingHost (0.0f);
                processor.parameters.getParameter ("crunch")->setValueNotifyingHost (1.0f);
                processor.parameters.getParameter ("punch")->setValueNotifyingHost (0.0f);
                for (int mode : { 0, 1 })
                {
                    processor.parameters.getParameter ("bitstream")->setValueNotifyingHost (static_cast<float> (mode));
                    processor.getStateInformation (state);
                    auto processed = directory.getChildFile (name + "-mode-" + juce::String (mode) + ".wav");
                    ritual_app::renderWav (source, processed, state, cancel, progress);
                    std::unique_ptr<juce::AudioFormatReader> wet (formats.createReaderFor (processed));
                    if (! wet || wet->lengthInSamples != input.getNumSamples())
                        throw std::runtime_error ("Processed length mismatch");
                    wet->read (&result, 0, result.getNumSamples(), 0, true, true);
                    float difference = 0;
                    for (int c = 0; c < channels; ++c)
                        for (int i = 0; i < result.getNumSamples(); ++i)
                        {
                            const auto value = result.getSample (c, i);
                            if (! std::isfinite (value)) throw std::runtime_error ("Nonfinite export");
                            difference += std::abs (value - input.getSample (c, i));
                        }
                    std::cout << name << " mode " << mode << " difference " << difference << "\n";
                    if (difference < 1) throw std::runtime_error ("Effect was not applied");
                }
            }
        // A full-length input exercises streaming and cancellation beyond the short format matrix.
        const auto longSource = directory.getChildFile ("six-minute-source.wav");
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (longSource.createOutputStream());
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions()
            .withSampleRate (48000).withNumChannels (2).withBitsPerSample (16));
        juce::AudioBuffer<float> chunk (2, 48000);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < chunk.getNumSamples(); ++i)
                chunk.setSample (c, i, 0.25f * std::sin (i * 0.02f + c));
        if (! writer) throw std::runtime_error ("Long fixture writer failed");
        for (int second = 0; second < 360; ++second)
            if (! writer->writeFromAudioSampleBuffer (chunk, 0, chunk.getNumSamples()))
                throw std::runtime_error ("Long fixture write failed");
        writer.reset();
        RitualAudioProcessor processor;
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        const auto cancelled = directory.getChildFile ("cancelled.wav");
        progress.store (0);
        std::thread observer ([&]
        {
            while (progress.load() < 0.02f && ! cancel.load())
                std::this_thread::sleep_for (std::chrono::milliseconds (1));
            cancel.store (true);
        });
        bool cancelledCorrectly = false;
        try { ritual_app::renderWav (longSource, cancelled, state, cancel, progress); }
        catch (const std::exception& error) { cancelledCorrectly = juce::String (error.what()) == "Export cancelled."; }
        cancel.store (true);
        observer.join();
        if (! cancelledCorrectly || progress.load() >= 1.0f)
            throw std::runtime_error ("In-progress export did not cancel");
        cancel.store (false);
        const auto malformed = directory.getChildFile ("malformed.wav");
        malformed.replaceWithText ("not an audio file");
        bool rejected = false;
        try { ritual_app::renderWav (malformed, directory.getChildFile ("invalid-output.wav"), state, cancel, progress); }
        catch (const std::exception&) { rejected = true; }
        if (! rejected || directory.getChildFile ("invalid-output.wav").exists())
            throw std::runtime_error ("Malformed source was not safely rejected");
        std::cout << "Passed: six source formats, bypass sample alignment, both modes, output protection, six-minute source cancellation, malformed input.\nArtifacts: "
                  << directory.getFullPathName() << '\n';
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

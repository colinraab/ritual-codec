// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Colin Raab and contributors.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../src/classic_engine.hpp"
#include "../src/rit_stream.hpp"
#include "SpectrumTap.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

class RitualAudioProcessorEditor;

/** The audio processor for the Ritual Codec effect.

    The DSP engine works on interleaved mono or stereo audio.  This class owns
    the host-facing JUCE state, converts the AudioBuffer representation, and
    keeps the dry signal at the same fixed 256-sample latency as the engine.
*/
class RitualAudioProcessor final : public juce::AudioProcessor
{
public:
    // These strings are part of the saved-state and host-automation contract.
    // Existing parameter IDs retain their version hint for host compatibility.
    // The experimental bitstream parameter is appended with hint 2 below.
    inline static constexpr int parameterVersion = 1;
    inline static constexpr int bitstreamParameterVersion = 2;
    inline static constexpr int stateVersion = 2;

    inline static constexpr char budgetParameterID[] = "budget";
    inline static constexpr char competitionParameterID[] = "competition";
    inline static constexpr char memoryParameterID[] = "memory";
    inline static constexpr char punchParameterID[] = "punch";
    inline static constexpr char crunchParameterID[] = "crunch";
    inline static constexpr char clockParameterID[] = "clock";
    inline static constexpr char downsampleParameterID[] = "downsample";
    inline static constexpr char mixParameterID[] = "mix";
    inline static constexpr char outputParameterID[] = "output";
    inline static constexpr char bypassParameterID[] = "bypass";
    inline static constexpr char bitstreamParameterID[] = "bitstream";

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    RitualAudioProcessor();
    ~RitualAudioProcessor() override;

    const juce::String getName() const override;

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported (const BusesLayout&) const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    juce::AudioProcessorParameter* getBypassParameter() const override;
    double getTailLengthSeconds() const override;

    bool hasEditor() const override;
    juce::AudioProcessorEditor* createEditor() override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Read by the editor without taking a lock.  Values are finite peak levels
    // from the most recently processed block.
    SpectrumTap spectrumTap;

    std::atomic<float> inputPeak { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
    // The packet rate currently selected by the experimental stream path.
    // This is published for the editor without taking an audio-thread lock.
    std::atomic<float> streamBitrateKbps { 0.0f };

    // Public by design: editor controls and host integrations attach to this.
    juce::AudioProcessorValueTreeState parameters;

private:
    static constexpr int maxChannels = 2;
    static constexpr int dryDelaySamples = classic::Engine::latencySamples();
    static constexpr int streamDelaySamples = static_cast<int> (rit::block);
    static constexpr double parameterSmoothingSeconds = 0.005;

    static juce::AudioProcessor::BusesProperties makeBusesProperties();

    static bool isKnownParameterID (juce::StringRef id) noexcept;
    bool isValidStateXml (const juce::XmlElement& xml) const noexcept;
    bool isValidLegacyStateXml (const juce::XmlElement& xml) const noexcept;

    static float finiteClamped (float value, float fallback, float minimum, float maximum) noexcept;
    static int finiteIntClamped (float value, int fallback, int minimum, int maximum) noexcept;
    static unsigned streamBitrateFor (float budget, int sampleRate, int channels) noexcept;

    float readParameter (const std::atomic<float>* parameter, float fallback,
                         float minimum, float maximum) const noexcept;
    int readIntegerParameter (const std::atomic<float>* parameter, int fallback,
                              int minimum, int maximum) const noexcept;
    bool readBoolParameter (const std::atomic<float>* parameter, bool fallback) const noexcept;
    void resetSmoothers (double sampleRate) noexcept;
    void updateSmootherTargets() noexcept;
    void processAudio (juce::AudioBuffer<float>&, juce::MidiBuffer&, bool forceBypass) noexcept;

    std::unique_ptr<classic::Engine> engine;
    std::unique_ptr<rit::Streaming> stream;
    int preparedChannels = 2;
    int preparedSampleRate = 0;
    bool prepared = false;
    std::uint64_t samplePosition = 0;

    std::array<std::array<float, dryDelaySamples>, maxChannels> dryDelay {};
    std::array<std::array<float, streamDelaySamples>, maxChannels> streamDelay {};
    unsigned configuredStreamBitrate = 0;
    std::uint64_t streamPosition = 0;
    int streamWarmSamples = 0;
    int streamFadeSamples = 1;
    float streamMix = 0.0f;
    bool streamRunning = false;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> budgetSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> competitionSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> memorySmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> punchSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> crunchSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputSmoother;

    std::atomic<float>* budgetParameter = nullptr;
    std::atomic<float>* competitionParameter = nullptr;
    std::atomic<float>* memoryParameter = nullptr;
    std::atomic<float>* punchParameter = nullptr;
    std::atomic<float>* crunchParameter = nullptr;
    std::atomic<float>* clockParameter = nullptr;
    std::atomic<float>* downsampleParameter = nullptr;
    std::atomic<float>* mixParameter = nullptr;
    std::atomic<float>* outputParameter = nullptr;
    std::atomic<float>* bypassParameter = nullptr;
    std::atomic<float>* bitstreamParameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RitualAudioProcessor)
};

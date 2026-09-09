// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Colin Raab and contributors.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../src/bitfamine.hpp"

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
    // The version hint is deliberately kept at 1 until a migration is added.
    inline static constexpr int parameterVersion = 1;
    inline static constexpr int stateVersion = 1;

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

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    RitualAudioProcessor();
    ~RitualAudioProcessor() override;

    const juce::String getName() const override;

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
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
    std::atomic<float> inputPeak { 0.0f };
    std::atomic<float> outputPeak { 0.0f };

    // Public by design: editor controls and host integrations attach to this.
    juce::AudioProcessorValueTreeState parameters;

private:
    static constexpr int maxChannels = 2;
    static constexpr int dryDelaySamples = bf::Engine::latencySamples();
    static constexpr double parameterSmoothingSeconds = 0.005;

    static juce::AudioProcessor::BusesProperties makeBusesProperties();

    static bool isKnownParameterID (juce::StringRef id) noexcept;
    bool isValidStateXml (const juce::XmlElement& xml) const noexcept;

    static float finiteClamped (float value, float fallback, float minimum, float maximum) noexcept;
    static int finiteIntClamped (float value, int fallback, int minimum, int maximum) noexcept;

    float readParameter (const std::atomic<float>* parameter, float fallback,
                         float minimum, float maximum) const noexcept;
    int readIntegerParameter (const std::atomic<float>* parameter, int fallback,
                              int minimum, int maximum) const noexcept;
    bool readBoolParameter (const std::atomic<float>* parameter, bool fallback) const noexcept;
    void resetSmoothers (double sampleRate) noexcept;
    void updateSmootherTargets() noexcept;
    void processAudio (juce::AudioBuffer<float>&, juce::MidiBuffer&, bool forceBypass) noexcept;

    std::unique_ptr<bf::Engine> engine;
    int preparedChannels = 2;
    int preparedSampleRate = 0;
    bool prepared = false;
    std::uint64_t samplePosition = 0;

    std::array<std::array<float, dryDelaySamples>, maxChannels> dryDelay {};

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RitualAudioProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

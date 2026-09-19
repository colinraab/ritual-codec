// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Colin Raab and contributors.
#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <cmath>
#include <limits>

namespace
{
constexpr float defaultBudget = 0.4f;
constexpr float defaultCompetition = 0.65f;
constexpr float defaultMemory = 0.65f;
constexpr float defaultPunch = 0.9f;
constexpr float defaultCrunch = 0.25f;
constexpr int defaultClock = 4;
constexpr int defaultDownsample = 1;
constexpr float defaultMix = 1.0f;
constexpr float defaultOutput = 0.0f;
constexpr bool defaultBypass = false;
constexpr bool defaultBitstream = false;
constexpr unsigned streamVersion = 2;

bool isFiniteRate (double rate) noexcept
{
    return std::isfinite (rate) && rate >= 8000.0 && rate <= 192000.0;
}

unsigned ceilUnsigned (double value) noexcept
{
    if (! std::isfinite (value) || value <= 1.0)
        return 1u;

    const auto rounded = std::ceil (value);
    if (rounded >= static_cast<double> (std::numeric_limits<unsigned>::max()))
        return std::numeric_limits<unsigned>::max();

    return static_cast<unsigned> (rounded);
}

float actualBitrateKbps (unsigned requested, int sampleRate) noexcept
{
    if (requested == 0 || sampleRate <= 0)
        return 0.0f;

    const auto packetBytes = static_cast<unsigned> (static_cast<std::uint64_t> (requested)
                                                    * 1000u * rit::block
                                                    / (8u * static_cast<unsigned> (sampleRate)));
    return static_cast<float> (static_cast<double> (packetBytes) * 8.0
                               * static_cast<double> (sampleRate)
                               / (1000.0 * static_cast<double> (rit::block)));
}
}

juce::AudioProcessor::BusesProperties RitualAudioProcessor::makeBusesProperties()
{
    return BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
}

juce::AudioProcessorValueTreeState::ParameterLayout RitualAudioProcessor::createParameterLayout()
{
    using juce::AudioParameterBool;
    using juce::AudioParameterFloat;
    using juce::AudioParameterInt;
    using juce::NormalisableRange;
    using juce::ParameterID;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { budgetParameterID, parameterVersion }, "Budget",
        NormalisableRange<float> { 0.0f, 1.0f }, defaultBudget));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { competitionParameterID, parameterVersion }, "Competition",
        NormalisableRange<float> { 0.0f, 1.0f }, defaultCompetition));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { memoryParameterID, parameterVersion }, "Memory",
        NormalisableRange<float> { 0.0f, 1.0f }, defaultMemory));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { punchParameterID, parameterVersion }, "Punch",
        NormalisableRange<float> { 0.0f, 1.0f }, defaultPunch));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { crunchParameterID, parameterVersion }, "Crunch",
        NormalisableRange<float> { 0.0f, 1.0f }, defaultCrunch));
    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { clockParameterID, parameterVersion }, "Clock", 1, 64, defaultClock));
    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { downsampleParameterID, parameterVersion }, "Downsample", 1, 32,
        defaultDownsample));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { mixParameterID, parameterVersion }, "Mix",
        NormalisableRange<float> { 0.0f, 1.0f }, defaultMix));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { outputParameterID, parameterVersion }, "Output",
        NormalisableRange<float> { -24.0f, 12.0f }, defaultOutput));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { bypassParameterID, parameterVersion }, "Bypass", defaultBypass));
    // Keep this experiment at the end of the layout so the ten established
    // parameters retain their ordering and host automation identities.
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { bitstreamParameterID, bitstreamParameterVersion },
        "RIT Bitstream", defaultBitstream));

    return layout;
}

RitualAudioProcessor::RitualAudioProcessor()
    : AudioProcessor (makeBusesProperties()),
      parameters (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    budgetParameter       = parameters.getRawParameterValue (budgetParameterID);
    competitionParameter  = parameters.getRawParameterValue (competitionParameterID);
    memoryParameter       = parameters.getRawParameterValue (memoryParameterID);
    punchParameter        = parameters.getRawParameterValue (punchParameterID);
    crunchParameter       = parameters.getRawParameterValue (crunchParameterID);
    clockParameter        = parameters.getRawParameterValue (clockParameterID);
    downsampleParameter   = parameters.getRawParameterValue (downsampleParameterID);
    mixParameter          = parameters.getRawParameterValue (mixParameterID);
    outputParameter       = parameters.getRawParameterValue (outputParameterID);
    bypassParameter       = parameters.getRawParameterValue (bypassParameterID);
    bitstreamParameter    = parameters.getRawParameterValue (bitstreamParameterID);

    // The latency is fixed by bf::Engine::N and remains present while bypassed
    // so that host compensation never changes as the bypass parameter moves.
    setLatencySamples (dryDelaySamples);

    budgetSmoother.setCurrentAndTargetValue (readParameter (budgetParameter, defaultBudget, 0.0f, 1.0f));
    competitionSmoother.setCurrentAndTargetValue (readParameter (competitionParameter, defaultCompetition, 0.0f, 1.0f));
    memorySmoother.setCurrentAndTargetValue (readParameter (memoryParameter, defaultMemory, 0.0f, 1.0f));
    punchSmoother.setCurrentAndTargetValue (readParameter (punchParameter, defaultPunch, 0.0f, 1.0f));
    crunchSmoother.setCurrentAndTargetValue (readParameter (crunchParameter, defaultCrunch, 0.0f, 1.0f));
    mixSmoother.setCurrentAndTargetValue (readParameter (mixParameter, defaultMix, 0.0f, 1.0f));
    outputSmoother.setCurrentAndTargetValue (readParameter (outputParameter, defaultOutput, -24.0f, 12.0f));
}

RitualAudioProcessor::~RitualAudioProcessor() = default;

const juce::String RitualAudioProcessor::getName() const
{
    return "Ritual Codec";
}

void RitualAudioProcessor::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    juce::ignoreUnused (maximumExpectedSamplesPerBlock);

    setLatencySamples (dryDelaySamples);
    engine.reset();
    prepared = false;
    preparedSampleRate = 0;
    samplePosition = 0;

    for (auto& channel : dryDelay)
        channel.fill (0.0f);

    for (auto& channel : streamDelay)
        channel.fill (0.0f);

    stream.reset();
    configuredStreamBitrate = 0;
    streamPosition = 0;
    streamWarmSamples = 0;
    streamFadeSamples = 1;
    streamMix = 0.0f;
    streamRunning = false;
    streamBitrateKbps.store (0.0f, std::memory_order_relaxed);

    spectrumTap.prepare (getSampleRate());
    inputPeak.store (0.0f, std::memory_order_relaxed);
    outputPeak.store (0.0f, std::memory_order_relaxed);

    const auto inputChannels = getTotalNumInputChannels();
    const auto outputChannels = getTotalNumOutputChannels();
    const auto channelCount = inputChannels == outputChannels ? inputChannels : 0;
    preparedChannels = juce::jlimit (1, maxChannels, channelCount);

    const bool validLayout = inputChannels == outputChannels
                          && channelCount >= 1
                          && channelCount <= maxChannels;
    const bool validRate = isFiniteRate (sampleRate);

    int roundedRate = 0;
    if (validRate)
    {
        const auto rounded = std::llround (sampleRate);
        if (rounded >= 8000 && rounded <= 192000)
            roundedRate = static_cast<int> (rounded);
    }

    resetSmoothers (roundedRate > 0 ? static_cast<double> (roundedRate) : 0.0);

    if (validLayout && roundedRate > 0)
    {
        bf::Params initialParams;
        initialParams.budget = readParameter (budgetParameter, defaultBudget, 0.0f, 1.0f);
        initialParams.competition = readParameter (competitionParameter, defaultCompetition, 0.0f, 1.0f);
        initialParams.memory = readParameter (memoryParameter, defaultMemory, 0.0f, 1.0f);
        initialParams.punch = readParameter (punchParameter, defaultPunch, 0.0f, 1.0f);
        initialParams.crunch = readParameter (crunchParameter, defaultCrunch, 0.0f, 1.0f);
        initialParams.clock = readIntegerParameter (clockParameter, defaultClock, 1, 64);
        initialParams.downsample = readIntegerParameter (downsampleParameter, defaultDownsample, 1, 32);
        initialParams.transparent = false;

        try
        {
            engine = std::make_unique<bf::Engine> (roundedRate, channelCount, initialParams);
            preparedSampleRate = roundedRate;
        }
        catch (...)
        {
            // A host can probe an unsupported rate or a malformed environment.
            // Keep a deterministic delayed-dry fallback instead of failing audio.
            engine.reset();
        }

        try
        {
            const auto budget = readParameter (budgetParameter, defaultBudget, 0.0f, 1.0f);
            const auto bitrate = streamBitrateFor (budget, roundedRate, channelCount);
            stream = std::make_unique<rit::Streaming> (roundedRate, channelCount,
                                                        bitrate, streamVersion);
            configuredStreamBitrate = bitrate;
            streamBitrateKbps.store (actualBitrateKbps (bitrate, roundedRate),
                                     std::memory_order_relaxed);
            const auto fade = std::llround (sampleRate * parameterSmoothingSeconds);
            streamFadeSamples = juce::jmax (1, static_cast<int> (juce::jlimit<std::int64_t> (
                1, std::numeric_limits<int>::max(), fade)));
        }
        catch (...)
        {
            // The experimental path is optional.  A host probing an unusual
            // rate or a codec implementation that rejects its packet budget
            // continues to receive the established spectral effect.
            stream.reset();
            configuredStreamBitrate = 0;
            streamBitrateKbps.store (0.0f, std::memory_order_relaxed);
        }
    }

    // Even if the engine could not be constructed, processBlock remains safe and
    // returns the latency-aligned dry path.
    prepared = true;
}

void RitualAudioProcessor::releaseResources()
{
    engine.reset();
    stream.reset();
    prepared = false;
    preparedSampleRate = 0;
    samplePosition = 0;

    for (auto& channel : dryDelay)
        channel.fill (0.0f);

    for (auto& channel : streamDelay)
        channel.fill (0.0f);

    configuredStreamBitrate = 0;
    streamPosition = 0;
    streamWarmSamples = 0;
    streamFadeSamples = 1;
    streamMix = 0.0f;
    streamRunning = false;

    inputPeak.store (0.0f, std::memory_order_relaxed);
    outputPeak.store (0.0f, std::memory_order_relaxed);
    streamBitrateKbps.store (0.0f, std::memory_order_relaxed);
}

bool RitualAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (input.isDisabled() || output.isDisabled() || input != output)
        return false;

    return input == juce::AudioChannelSet::mono()
        || input == juce::AudioChannelSet::stereo();
}

bool RitualAudioProcessor::acceptsMidi() const
{
    return false;
}

bool RitualAudioProcessor::producesMidi() const
{
    return false;
}

bool RitualAudioProcessor::isMidiEffect() const
{
    return false;
}

juce::AudioProcessorParameter* RitualAudioProcessor::getBypassParameter() const
{
    return parameters.getParameter (bypassParameterID);
}

double RitualAudioProcessor::getTailLengthSeconds() const
{
    const auto rate = getSampleRate();
    const auto effectiveRate = std::isfinite (rate) && rate > 0.0 ? rate : 48000.0;
    return static_cast<double> (2 * dryDelaySamples + 32) / effectiveRate;
}

bool RitualAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* RitualAudioProcessor::createEditor()
{
    return new RitualAudioProcessorEditor (*this);
}

int RitualAudioProcessor::getNumPrograms()
{
    return 1;
}

int RitualAudioProcessor::getCurrentProgram()
{
    return 0;
}

void RitualAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String RitualAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return "Default";
}

void RitualAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void RitualAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages)
{
    processAudio (buffer, midiMessages, false);
}

void RitualAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer,
                                                 juce::MidiBuffer& midiMessages)
{
    processAudio (buffer, midiMessages, true);
}

void RitualAudioProcessor::processAudio (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages,
                                         bool forceBypass) noexcept
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int bufferChannels = buffer.getNumChannels();

    if (numSamples <= 0 || bufferChannels <= 0)
    {
        inputPeak.store (0.0f, std::memory_order_relaxed);
        outputPeak.store (0.0f, std::memory_order_relaxed);
        return;
    }

    const int channels = juce::jlimit (1, maxChannels, preparedChannels);
    const int channelsToWrite = juce::jmin (channels, bufferChannels);
    std::array<float*, maxChannels> channelData {};

    for (int channel = 0; channel < channelsToWrite; ++channel)
        channelData[static_cast<size_t> (channel)] = buffer.getWritePointer (channel);

    std::array<float, maxChannels> sample {};
    std::array<float, maxChannels> streamWet {};
    std::array<float, maxChannels> streamAligned {};
    std::array<float, maxChannels> drySample {};
    float blockInputPeak = 0.0f;
    float blockOutputPeak = 0.0f;

    for (int frame = 0; frame < numSamples; ++frame)
    {
        const auto delayIndex = static_cast<size_t> (samplePosition & (dryDelaySamples - 1));

        for (int channel = 0; channel < channels; ++channel)
        {
            float value = 0.0f;
            if (channel < channelsToWrite)
                value = channelData[static_cast<size_t> (channel)][frame];

            if (! std::isfinite (value))
                value = 0.0f;

            const auto channelIndex = static_cast<size_t> (channel);
            drySample[channelIndex] = dryDelay[channelIndex][delayIndex];
            sample[channelIndex] = value;
            dryDelay[static_cast<size_t> (channel)][delayIndex] = value;
            blockInputPeak = std::max (blockInputPeak, std::abs (value));
        }

        // The engine has no internal parameter interpolation.  Feeding it one
        // sample at a time makes each SmoothedValue step consume exactly one
        // sample, including across arbitrary host block boundaries.
        updateSmootherTargets();
        const float budget = finiteClamped (budgetSmoother.getNextValue(), defaultBudget, 0.0f, 1.0f);
        const float competition = finiteClamped (competitionSmoother.getNextValue(), defaultCompetition, 0.0f, 1.0f);
        const float memory = finiteClamped (memorySmoother.getNextValue(), defaultMemory, 0.0f, 1.0f);
        const float punch = finiteClamped (punchSmoother.getNextValue(), defaultPunch, 0.0f, 1.0f);
        const float crunch = finiteClamped (crunchSmoother.getNextValue(), defaultCrunch, 0.0f, 1.0f);
        const float mix = finiteClamped (mixSmoother.getNextValue(), defaultMix, 0.0f, 1.0f);
        const float outputDb = finiteClamped (outputSmoother.getNextValue(), defaultOutput, -24.0f, 12.0f);
        const int clock = readIntegerParameter (clockParameter, defaultClock, 1, 64);
        const int downsample = readIntegerParameter (downsampleParameter, defaultDownsample, 1, 32);
        const bool bypass = forceBypass || readBoolParameter (bypassParameter, defaultBypass);
        const bool wantsStream = readBoolParameter (bitstreamParameter, defaultBitstream);

        // Start the experimental path on the audio thread without allocating.
        // It is warmed for the two 128-sample stages (codec plus alignment)
        // before the crossfade can expose it at the fixed plugin latency.
        if (wantsStream && ! streamRunning && stream != nullptr)
        {
            stream->reset();
            for (auto& channel : streamDelay)
                channel.fill (0.0f);
            streamPosition = 0;
            streamWarmSamples = dryDelaySamples;
            streamMix = 0.0f;
            streamRunning = true;
        }

        if (streamRunning && stream != nullptr)
        {
            const auto requestedBitrate = streamBitrateFor (budget, preparedSampleRate,
                                                             channels);
            // Streaming::setBitrate resets its fixed packet state.  Apply
            // automation at a packet boundary so a moving budget cannot
            // repeatedly erase the in flight block on every sample.
            if (requestedBitrate != configuredStreamBitrate
                && (streamPosition % rit::block) == 0
                && stream->setBitrate (requestedBitrate))
            {
                configuredStreamBitrate = requestedBitrate;
                streamBitrateKbps.store (actualBitrateKbps (requestedBitrate,
                                                            preparedSampleRate),
                                         std::memory_order_relaxed);
                for (auto& channel : streamDelay)
                    channel.fill (0.0f);
            }

            // The stream API accepts interleaved samples.  Calling it with one
            // sample keeps this wrapper independent of the host block size and
            // avoids a callback-time scratch allocation.
            stream->process (sample.data(), streamWet.data(), 1);
            const auto streamDelayIndex = static_cast<size_t> (
                streamPosition & static_cast<std::uint64_t> (streamDelaySamples - 1));
            for (int channel = 0; channel < channels; ++channel)
            {
                const auto channelIndex = static_cast<size_t> (channel);
                streamAligned[channelIndex] = streamDelay[channelIndex][streamDelayIndex];
                streamDelay[channelIndex][streamDelayIndex] = streamWet[channelIndex];
            }
            ++streamPosition;

            if (streamWarmSamples > 0)
                --streamWarmSamples;
            else
            {
                const auto target = wantsStream ? 1.0f : 0.0f;
                const auto step = 1.0f / static_cast<float> (juce::jmax (1, streamFadeSamples));
                if (streamMix < target)
                    streamMix = juce::jmin (target, streamMix + step);
                else if (streamMix > target)
                    streamMix = juce::jmax (target, streamMix - step);

                if (! wantsStream && streamMix <= 0.0f)
                {
                    streamRunning = false;
                    stream->reset();
                    streamPosition = 0;
                    for (auto& channel : streamDelay)
                        channel.fill (0.0f);
                }
            }
        }

        if (engine != nullptr)
        {
            bf::Params currentParams;
            currentParams.budget = budget;
            currentParams.competition = competition;
            currentParams.memory = memory;
            currentParams.punch = punch;
            currentParams.crunch = crunch;
            currentParams.clock = clock;
            currentParams.downsample = downsample;
            currentParams.transparent = false;

            // All fields above are finite and bounded, so this cannot throw.
            engine->setParams (currentParams);
            engine->process (sample.data(), sample.data(), 1);
        }

        const float outputGain = std::pow (10.0f, outputDb * 0.05f);
        for (int channel = 0; channel < channelsToWrite; ++channel)
        {
            const auto channelIndex = static_cast<size_t> (channel);
            const float dry = drySample[channelIndex];
            float classicValue = dry;

            if (engine != nullptr && ! bypass)
            {
                const float wet = std::isfinite (sample[channelIndex]) ? sample[channelIndex] : dry;
                classicValue = (dry + (wet - dry) * mix) * outputGain;
            }

            const float streamValue = (dry
                                       + (std::isfinite (streamAligned[channelIndex])
                                              ? streamAligned[channelIndex] - dry
                                              : 0.0f) * mix) * outputGain;
            float value = classicValue + (streamValue - classicValue) * streamMix;
            if (bypass)
                value = dry;

            if (! std::isfinite (value))
                value = 0.0f;
            else if (std::abs (value) < 1.0e-20f)
                value = 0.0f;

            channelData[channelIndex][frame] = value;
            blockOutputPeak = std::max (blockOutputPeak, std::abs (value));
        }

        if (channelsToWrite > 0) {
            const auto right = channelsToWrite > 1 ? 1u : 0u;
            spectrumTap.push(drySample[0], drySample[right],
                             channelData[0][frame], channelData[right][frame]);
        }
        ++samplePosition;
    }

    // A bus layout probe should prevent this case, but hosts are permitted to
    // pass a buffer with extra channels.  Do not leak uninitialised input data.
    for (int channel = channelsToWrite; channel < bufferChannels; ++channel)
        buffer.clear (channel, 0, numSamples);

    inputPeak.store (blockInputPeak, std::memory_order_relaxed);
    outputPeak.store (blockOutputPeak, std::memory_order_relaxed);
}

void RitualAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    destData.reset();

    auto xml = parameters.copyState().createXml();
    if (xml == nullptr)
        return;

    xml->setAttribute ("stateVersion", stateVersion);
    copyXmlToBinary (*xml, destData);
}

void RitualAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Keep malformed or unreasonably large host data from reaching APVTS.
    if (data == nullptr || sizeInBytes <= 0 || sizeInBytes > 16 * 1024 * 1024)
        return;

    try
    {
        auto xml = getXmlFromBinary (data, sizeInBytes);
        if (xml == nullptr || ! xml->hasTagName ("PARAMETERS"))
            return;

        // Version 1 states predate the optional stream mode and contain the
        // original ten parameters.  Validate them against the old shape, then
        // append an explicit false bitstream parameter before replacing APVTS
        // state so every restored tree has the current eleven-parameter shape.
        const int sourceVersion = xml->hasAttribute ("stateVersion")
                                    ? xml->getIntAttribute ("stateVersion", -1)
                                    : 1;
        if (sourceVersion == 1)
        {
            if (! isValidLegacyStateXml (*xml))
                return;

            if (auto* bitstream = xml->createNewChildElement ("PARAM"))
            {
                bitstream->setAttribute ("id", bitstreamParameterID);
                bitstream->setAttribute ("value", 0.0);
            }
            else
            {
                return;
            }
        }
        else if (sourceVersion != stateVersion || ! isValidStateXml (*xml))
        {
            return;
        }

        const auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid() && state.hasType ("PARAMETERS"))
            parameters.replaceState (state);
    }
    catch (...)
    {
        // State restoration is a host/message-thread operation.  A corrupt
        // state should leave the existing parameters intact and never escape.
    }
}

bool RitualAudioProcessor::isKnownParameterID (juce::StringRef id) noexcept
{
    return id == juce::StringRef (budgetParameterID)
        || id == juce::StringRef (competitionParameterID)
        || id == juce::StringRef (memoryParameterID)
        || id == juce::StringRef (punchParameterID)
        || id == juce::StringRef (crunchParameterID)
        || id == juce::StringRef (clockParameterID)
        || id == juce::StringRef (downsampleParameterID)
        || id == juce::StringRef (mixParameterID)
        || id == juce::StringRef (outputParameterID)
        || id == juce::StringRef (bypassParameterID)
        || id == juce::StringRef (bitstreamParameterID);
}

bool RitualAudioProcessor::isValidStateXml (const juce::XmlElement& xml) const noexcept
{
    if (! xml.hasTagName ("PARAMETERS"))
        return false;

    if (! xml.hasAttribute ("stateVersion")
        || xml.getIntAttribute ("stateVersion", -1) != stateVersion)
        return false;

    if (xml.getNumChildElements() != 11)
        return false;

    for (auto* child = xml.getFirstChildElement(); child != nullptr; child = child->getNextElement())
    {
        if (! child->hasTagName ("PARAM"))
            return false;

        const auto id = child->getStringAttribute ("id");
        if (! isKnownParameterID (id) || ! child->hasAttribute ("value"))
            return false;

        // Reject duplicate parameter entries.  APVTS otherwise accepts the
        // first/last one depending on listener timing, which is not stable.
        for (auto* previous = xml.getFirstChildElement(); previous != child; previous = previous->getNextElement())
            if (previous->hasTagName ("PARAM")
                && previous->getStringAttribute ("id") == id)
                return false;

        const auto value = child->getDoubleAttribute (
            "value", std::numeric_limits<double>::quiet_NaN());
        if (! std::isfinite (value))
            return false;

        const auto range = parameters.getParameterRange (id);
        if (! std::isfinite (static_cast<double> (range.start))
            || ! std::isfinite (static_cast<double> (range.end))
            || value < static_cast<double> (range.start)
            || value > static_cast<double> (range.end))
            return false;
    }

    return true;
}

bool RitualAudioProcessor::isValidLegacyStateXml (const juce::XmlElement& xml) const noexcept
{
    if (! xml.hasTagName ("PARAMETERS") || xml.getNumChildElements() != 10)
        return false;

    if (xml.hasAttribute ("stateVersion")
        && xml.getIntAttribute ("stateVersion", -1) != 1)
        return false;

    for (auto* child = xml.getFirstChildElement(); child != nullptr; child = child->getNextElement())
    {
        if (! child->hasTagName ("PARAM"))
            return false;

        const auto id = child->getStringAttribute ("id");
        if (! isKnownParameterID (id) || id == juce::StringRef (bitstreamParameterID)
            || ! child->hasAttribute ("value"))
            return false;

        for (auto* previous = xml.getFirstChildElement(); previous != child;
             previous = previous->getNextElement())
            if (previous->hasTagName ("PARAM")
                && previous->getStringAttribute ("id") == id)
                return false;

        const auto value = child->getDoubleAttribute (
            "value", std::numeric_limits<double>::quiet_NaN());
        if (! std::isfinite (value))
            return false;

        const auto range = parameters.getParameterRange (id);
        if (! std::isfinite (static_cast<double> (range.start))
            || ! std::isfinite (static_cast<double> (range.end))
            || value < static_cast<double> (range.start)
            || value > static_cast<double> (range.end))
            return false;
    }

    return true;
}

float RitualAudioProcessor::finiteClamped (float value, float fallback,
                                           float minimum, float maximum) noexcept
{
    if (! std::isfinite (value))
        value = fallback;
    if (! std::isfinite (value))
        value = minimum;
    return juce::jlimit (minimum, maximum, value);
}

int RitualAudioProcessor::finiteIntClamped (float value, int fallback,
                                            int minimum, int maximum) noexcept
{
    value = finiteClamped (value, static_cast<float> (fallback),
                           static_cast<float> (minimum), static_cast<float> (maximum));
    return juce::jlimit (minimum, maximum, juce::roundToInt (value));
}

unsigned RitualAudioProcessor::streamBitrateFor (float budget, int sampleRate,
                                                 int channels) noexcept
{
    if (sampleRate < 8000 || sampleRate > 192000 || channels < 1 || channels > maxChannels)
        return 0;

    // A packet contains a six-byte packet header and one five-byte channel
    // header plus at least one 15-bit record per channel.  Derive the minimum
    // from that overhead so every rate/channel layout remains decodable.
    const auto minimumPacketBytes = 6u + static_cast<unsigned> (channels) * 7u;
    const auto minimum = ceilUnsigned (static_cast<double> (minimumPacketBytes)
                                       * 8.0 * static_cast<double> (sampleRate)
                                       / (1000.0 * static_cast<double> (rit::block)));
    if (minimum == 0)
        return 0;

    // Keep the control useful across sample rates.  384 kbps is the upper
    // end at 48 kHz, while high-rate sessions scale the range with the valid
    // packet overhead instead of falling below the format's minimum.
    const auto maximum = std::max (384u, minimum * 4u);
    const auto amount = finiteClamped (budget, defaultBudget, 0.0f, 1.0f);
    const auto value = static_cast<double> (minimum)
                     + static_cast<double> (maximum - minimum) * amount;
    return juce::jlimit (minimum, maximum, ceilUnsigned (value));
}

float RitualAudioProcessor::readParameter (const std::atomic<float>* parameter,
                                           float fallback, float minimum,
                                           float maximum) const noexcept
{
    return finiteClamped (parameter != nullptr
                              ? parameter->load (std::memory_order_relaxed)
                              : fallback,
                          fallback, minimum, maximum);
}

int RitualAudioProcessor::readIntegerParameter (const std::atomic<float>* parameter,
                                                int fallback, int minimum,
                                                int maximum) const noexcept
{
    return finiteIntClamped (parameter != nullptr
                                 ? parameter->load (std::memory_order_relaxed)
                                 : static_cast<float> (fallback),
                             fallback, minimum, maximum);
}

bool RitualAudioProcessor::readBoolParameter (const std::atomic<float>* parameter,
                                              bool fallback) const noexcept
{
    if (parameter == nullptr)
        return fallback;

    const auto value = parameter->load (std::memory_order_relaxed);
    return std::isfinite (value) ? value >= 0.5f : fallback;
}

void RitualAudioProcessor::resetSmoothers (double sampleRate) noexcept
{
    int smoothingSteps = 0;
    if (std::isfinite (sampleRate) && sampleRate > 0.0)
    {
        const auto steps = std::floor (sampleRate * parameterSmoothingSeconds);
        if (std::isfinite (steps) && steps > 0.0
            && steps < static_cast<double> (std::numeric_limits<int>::max()))
            smoothingSteps = static_cast<int> (steps);
    }

    budgetSmoother.reset (smoothingSteps);
    competitionSmoother.reset (smoothingSteps);
    memorySmoother.reset (smoothingSteps);
    punchSmoother.reset (smoothingSteps);
    crunchSmoother.reset (smoothingSteps);
    mixSmoother.reset (smoothingSteps);
    outputSmoother.reset (smoothingSteps);

    budgetSmoother.setCurrentAndTargetValue (readParameter (budgetParameter, defaultBudget, 0.0f, 1.0f));
    competitionSmoother.setCurrentAndTargetValue (readParameter (competitionParameter, defaultCompetition, 0.0f, 1.0f));
    memorySmoother.setCurrentAndTargetValue (readParameter (memoryParameter, defaultMemory, 0.0f, 1.0f));
    punchSmoother.setCurrentAndTargetValue (readParameter (punchParameter, defaultPunch, 0.0f, 1.0f));
    crunchSmoother.setCurrentAndTargetValue (readParameter (crunchParameter, defaultCrunch, 0.0f, 1.0f));
    mixSmoother.setCurrentAndTargetValue (readParameter (mixParameter, defaultMix, 0.0f, 1.0f));
    outputSmoother.setCurrentAndTargetValue (readParameter (outputParameter, defaultOutput, -24.0f, 12.0f));
}

void RitualAudioProcessor::updateSmootherTargets() noexcept
{
    budgetSmoother.setTargetValue (readParameter (budgetParameter, defaultBudget, 0.0f, 1.0f));
    competitionSmoother.setTargetValue (readParameter (competitionParameter, defaultCompetition, 0.0f, 1.0f));
    memorySmoother.setTargetValue (readParameter (memoryParameter, defaultMemory, 0.0f, 1.0f));
    punchSmoother.setTargetValue (readParameter (punchParameter, defaultPunch, 0.0f, 1.0f));
    crunchSmoother.setTargetValue (readParameter (crunchParameter, defaultCrunch, 0.0f, 1.0f));
    mixSmoother.setTargetValue (readParameter (mixParameter, defaultMix, 0.0f, 1.0f));
    outputSmoother.setTargetValue (readParameter (outputParameter, defaultOutput, -24.0f, 12.0f));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RitualAudioProcessor();
}

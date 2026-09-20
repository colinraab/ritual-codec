// SPDX-License-Identifier: AGPL-3.0-or-later
#include "rit_stream.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <vector>
#include <cstdlib>
#include <new>

static unsigned oversizedAllocations = 0;
void* operator new(std::size_t bytes)
{
    if (bytes > 1024 * 1024) { ++oversizedAllocations; throw std::bad_alloc(); }
    if (auto* memory = std::malloc(bytes == 0 ? 1 : bytes)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace
{
void require (bool condition, const char* message)
{
    rit::require (condition, message);
}

uint64_t readLittleEndian (const rit::Bytes& bytes, size_t offset, unsigned count)
{
    require (offset <= bytes.size() && count <= bytes.size() - offset,
             "Test tried to read past a byte stream");
    uint64_t value = 0;
    for (unsigned index = 0; index < count; ++index)
        value |= uint64_t (bytes[offset + index]) << (8u * index);
    return value;
}

void writeLittleEndian (rit::Bytes& bytes, size_t offset, uint64_t value, unsigned count)
{
    require (offset <= bytes.size() && count <= bytes.size() - offset,
             "Test tried to write past a byte stream");
    for (unsigned index = 0; index < count; ++index)
        bytes[offset + index] = static_cast<uint8_t> (value >> (8u * index));
}

void recomputeChecksums (rit::Bytes& bytes)
{
    constexpr size_t fileHeaderBytes = 24;
    require (bytes.size() >= fileHeaderBytes, "Encoded stream has no file header");

    size_t packet = fileHeaderBytes;
    while (packet < bytes.size())
    {
        require (bytes.size() - packet >= 6, "Encoded stream has a short packet header");
        const auto packetBytes = static_cast<size_t> (readLittleEndian (bytes, packet, 2));
        require (packetBytes >= 6 && packetBytes <= bytes.size() - packet,
                 "Encoded stream has an invalid packet size");
        const auto checksum = rit::crc (bytes.data() + packet + 6, packetBytes - 6);
        writeLittleEndian (bytes, packet + 2, checksum, 4);
        packet += packetBytes;
    }
    require (packet == bytes.size(), "Encoded stream has trailing packet bytes");
}

rit::Audio makeAudio (unsigned rate, unsigned channels, size_t frames)
{
    rit::Audio audio { rate, channels, std::vector<float> (frames * channels) };
    for (size_t frame = 0; frame < frames; ++frame)
        for (unsigned channel = 0; channel < channels; ++channel)
        {
            const auto phase = static_cast<double> (frame) * (0.017 + 0.009 * channel);
            auto value = 0.19 * std::sin (phase) + 0.07 * std::cos (phase * 0.37 + channel);
            if (frame % 131 == 19)
                value += channel == 0 ? 0.8 : -0.6;
            audio.samples[frame * channels + channel] = static_cast<float> (value);
        }
    return audio;
}

float finiteClamped (float value)
{
    if (! std::isfinite (value))
        return 0.0f;
    return std::max (-4.0f, std::min (4.0f, value));
}

rit::Audio sanitize (const rit::Audio& input)
{
    auto output = input;
    for (auto& value : output.samples)
        value = finiteClamped (value);
    return output;
}

void checkFiniteDecoded (const rit::Audio& decoded, const rit::Audio& expected,
                         const char* message)
{
    require (decoded.rate == expected.rate && decoded.channels == expected.channels,
             message);
    require (decoded.samples.size() == expected.samples.size(), message);
    for (const auto value : decoded.samples)
        require (std::isfinite (value), message);
}

void expectRejected (const rit::Bytes& bytes, const char* message)
{
    bool rejected = false;
    try
    {
        static_cast<void> (rit::decode (bytes));
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    require (rejected, message);
}

uint32_t nextRandom (uint32_t& state)
{
    // Fixed-width xorshift keeps this mutation corpus reproducible on every
    // standard library and does not depend on implementation RNG details.
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

std::vector<size_t> payloadPositions (const rit::Bytes& bytes)
{
    std::vector<size_t> positions;
    constexpr size_t fileHeaderBytes = 24;
    size_t packet = fileHeaderBytes;
    while (packet < bytes.size())
    {
        const auto packetBytes = static_cast<size_t> (readLittleEndian (bytes, packet, 2));
        require (packetBytes >= 6 && packetBytes <= bytes.size() - packet,
                 "Encoded stream has an invalid packet size");
        for (size_t offset = packet + 6; offset < packet + packetBytes; ++offset)
            positions.push_back (offset);
        packet += packetBytes;
    }
    return positions;
}

void checkMutationCorpus (const rit::Audio& source, unsigned kbps, unsigned version)
{
    const auto original = rit::encode (source, kbps, version);
    const auto positions = payloadPositions (original);
    require (! positions.empty(), "Encoded stream has no payload bytes");

    // Changing only bounded energy/scale metadata keeps the records valid and
    // proves that a payload mutation with a repaired checksum reaches the
    // reconstruction path and remains finite.
    auto metadataMutation = original;
    for (size_t packet = 24; packet < metadataMutation.size();)
    {
        const auto packetBytes = static_cast<size_t> (readLittleEndian (metadataMutation, packet, 2));
        size_t channelData = packet + 6;
        for (unsigned channel = 0; channel < source.channels; ++channel)
        {
            const auto channelHeader = channelData;
            writeLittleEndian (metadataMutation, channelHeader + 1, 0xffffu, 2);
            writeLittleEndian (metadataMutation, channelHeader + 3, 0xffffu, 2);
            const auto rawCount = static_cast<unsigned> (metadataMutation[channelHeader]);
            const auto count = version == 2
                                 ? ((rawCount & 0x80u) != 0
                                       ? rawCount & 0x7fu
                                       : rawCount == 0 ? rit::block : rawCount)
                                 : rawCount;
            channelData += 5 + (count * 15 + 7) / 8;
        }
        packet += packetBytes;
    }
    recomputeChecksums (metadataMutation);
    const auto metadataDecoded = rit::decode (metadataMutation);
    checkFiniteDecoded (metadataDecoded, source, "Maximum payload metadata decoded non-finitely");

    uint32_t state = 0x9e3779b9u ^ version ^ (source.channels << 8u)
                   ^ (source.rate * 33u) ^ kbps;
    unsigned accepted = 0;
    constexpr unsigned mutations = 600;
    for (unsigned iteration = 0; iteration < mutations; ++iteration)
    {
        auto candidate = original;
        const auto changes = 1u + nextRandom (state) % 4u;
        for (unsigned change = 0; change < changes; ++change)
        {
            const auto payload = static_cast<size_t> (nextRandom (state) % positions.size());
            const auto bit = static_cast<unsigned> (nextRandom (state) & 7u);
            candidate[positions[payload]] ^= static_cast<uint8_t> (1u << bit);
        }
        // CRC bytes are packet headers and are intentionally repaired after
        // mutating only payload bytes. This exercises semantic validation.
        recomputeChecksums (candidate);

        rit::Audio decoded;
        try { decoded = rit::decode (candidate); }
        catch (const std::exception&) { continue; }
        ++accepted;
        // Keep assertions outside the parser rejection catch.
        checkFiniteDecoded (decoded, source, "Accepted mutation decoded non-finitely");
    }
    require (accepted > 0, "CRC-repaired mutation corpus never reached a valid decode");
}

void checkMalformedHeaders (const rit::Bytes& valid)
{
    constexpr size_t fileHeaderBytes = 24;
    require (valid.size() >= fileHeaderBytes, "Valid stream has no file header");

    // A header-only stream with an enormous frame count must fail before the
    // decoder reserves a sample vector proportional to that count.
    auto huge = rit::Bytes (valid.begin(), valid.begin() + static_cast<std::ptrdiff_t> (fileHeaderBytes));
    writeLittleEndian (huge, 12, std::numeric_limits<uint64_t>::max(), 8);
    expectRejected (huge, "Huge frame header was accepted or reached allocation");

    auto zeroFrames = huge;
    writeLittleEndian (zeroFrames, 12, 0, 8);
    expectRejected (zeroFrames, "Zero frame header was accepted");

    auto invalidPacket = huge;
    writeLittleEndian (invalidPacket, 12, 1, 8);
    writeLittleEndian (invalidPacket, 20, 5, 2);
    expectRejected (invalidPacket, "Short packet header was accepted");

    const std::array<std::pair<size_t, uint64_t>, 8> badFields {
        std::pair<size_t, uint64_t> { 4, 3 },
        std::pair<size_t, uint64_t> { 6, 3 },
        std::pair<size_t, uint64_t> { 7, 8 },
        std::pair<size_t, uint64_t> { 8, 0 },
        std::pair<size_t, uint64_t> { 8, std::numeric_limits<uint32_t>::max() },
        std::pair<size_t, uint64_t> { 20, 0 },
        std::pair<size_t, uint64_t> { 20, rit::maxPacketBytes + 1u },
        std::pair<size_t, uint64_t> { 22, 1 }
    };
    for (const auto& [offset, value] : badFields)
    {
        auto malformed = huge;
        const auto width = offset == 4 ? 2u : offset == 6 || offset == 7 ? 1u
                                                                          : offset == 8 ? 4u : 2u;
        writeLittleEndian (malformed, offset, value, width);
        expectRejected (malformed, "Malformed RIT header was accepted");
    }

    for (size_t cut = 0; cut < valid.size(); ++cut)
        expectRejected (rit::Bytes (valid.begin(), valid.begin() + static_cast<std::ptrdiff_t> (cut)),
                        "Truncated RIT stream was accepted");

    auto trailing = valid;
    trailing.push_back (0);
    expectRejected (trailing, "Trailing RIT data was accepted");
}

std::vector<float> runTinyStreaming (const rit::Audio& input, unsigned kbps, unsigned version)
{
    rit::Streaming stream (input.rate, input.channels, kbps, version);
    require (stream.isValid(), "Streaming rejected a valid reconstructed rate");

    std::vector<float> output (input.samples.size(), 0.0f);
    constexpr std::array<size_t, 9> callbackFrames { 1, 2, 3, 7, 11, 31, 5, 17, 127 };
    const auto totalFrames = input.samples.size() / input.channels;
    size_t position = 0;
    size_t callback = 0;
    while (position < totalFrames)
    {
        const auto frames = std::min (callbackFrames[callback % callbackFrames.size()],
                                      totalFrames - position);
        stream.process (input.samples.data() + position * input.channels,
                        output.data() + position * input.channels,
                        static_cast<int> (frames));
        position += frames;
        ++callback;
    }
    return output;
}

void checkStreamingReconstruction (unsigned rate, unsigned channels, unsigned version)
{
    const auto kbps = rate / 100;
    auto extreme = makeAudio (rate, channels, 259);
    extreme.samples[0] = std::numeric_limits<float>::quiet_NaN();
    extreme.samples[1 % extreme.samples.size()] = std::numeric_limits<float>::infinity();
    extreme.samples[2 % extreme.samples.size()] = -std::numeric_limits<float>::infinity();
    extreme.samples[3 % extreme.samples.size()] = 99.0f;
    extreme.samples[4 % extreme.samples.size()] = -99.0f;

    // The callback sanitizes these values before packetizing. Reconstruct the
    // corresponding file at each newly selected sample rate and compare the
    // delayed callback output against that independent decoder.
    const auto clean = sanitize (extreme);
    const auto expected = rit::decode (rit::encode (clean, kbps, version));
    const auto actual = runTinyStreaming (extreme, kbps, version);
    const auto delay = static_cast<size_t> (rit::Streaming::latencySamples());
    require (expected.samples.size() == actual.size(), "Streaming reconstruction changed length");
    for (size_t frame = 0; frame < expected.samples.size() / channels; ++frame)
        for (unsigned channel = 0; channel < channels; ++channel)
        {
            const auto index = frame * channels + channel;
            const auto wanted = frame < delay
                                  ? 0.0f
                                  : expected.samples[(frame - delay) * channels + channel];
            require (std::equal_to<> {} (actual[index], wanted),
                     "Tiny-buffer streaming differs from reconstructed decoder");
            require (std::isfinite (actual[index]), "Tiny-buffer streaming produced nonfinite output");
        }
}
}

int main () try
{
    for (const auto version : { 1u, 2u })
        for (const auto channels : { 1u, 2u })
        {
            const auto source = makeAudio (48000, channels, 257);
            checkMutationCorpus (source, 256, version);
            checkMalformedHeaders (rit::encode (source, 256, version));

            auto nonfinite = source;
            nonfinite.samples[0] = std::numeric_limits<float>::quiet_NaN();
            bool rejected = false;
            try { static_cast<void> (rit::encode (nonfinite, 256, version)); }
            catch (const std::exception&) { rejected = true; }
            require (rejected, "Encoder accepted nonfinite source audio");
        }

    for (const auto rate : { 8000u, 44100u, 48000u, 96000u, 192000u })
        for (const auto channels : { 1u, 2u })
            for (const auto version : { 1u, 2u })
                checkStreamingReconstruction (rate, channels, version);

    require (oversizedAllocations == 0, "Malformed input attempted oversized allocation");
    std::cout << "PASS: deterministic CRC-repaired mutations, bounded malformed headers, "
                 "and tiny-buffer streaming reconstruction across rates\n";
}
catch (const std::exception& error)
{
    std::cerr << error.what() << '\n';
    return 1;
}

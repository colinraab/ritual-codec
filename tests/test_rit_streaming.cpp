// SPDX-License-Identifier: AGPL-3.0-or-later
#include "rit_stream.hpp"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <new>
#include <iostream>
#include <vector>

namespace
{
std::atomic<bool> forbidAlloc { false };
std::atomic<unsigned> forbiddenAllocations { 0 };
}

void* operator new (std::size_t size)
{
    if (auto* pointer = std::malloc (size == 0 ? 1 : size))
    {
        if (forbidAlloc.load (std::memory_order_relaxed))
            forbiddenAllocations.fetch_add (1, std::memory_order_relaxed);
        return pointer;
    }
    throw std::bad_alloc();
}

void* operator new[] (std::size_t size)
{
    return ::operator new (size);
}

void operator delete (void* pointer) noexcept { std::free (pointer); }
void operator delete[] (void* pointer) noexcept { std::free (pointer); }
void operator delete (void* pointer, std::size_t) noexcept { std::free (pointer); }
void operator delete[] (void* pointer, std::size_t) noexcept { std::free (pointer); }

namespace
{
void require (bool condition, const char* message)
{
    rit::require (condition, message);
}

std::vector<float> runStreaming (const rit::Audio& input, unsigned kbps, unsigned version)
{
    rit::Streaming stream (input.rate, input.channels, kbps, version);
    require (stream.isValid(), "Streaming codec rejected valid setup");

    std::vector<float> output (input.samples.size(), 0.0f);
    size_t position = 0;
    const int blockSizes[] = { 1, 127, 513, 37, 256 };
    unsigned blockIndex = 0;
    while (position < input.samples.size() / input.channels)
    {
        const auto frames = std::min<size_t> (
            static_cast<size_t> (blockSizes[blockIndex++ % (sizeof (blockSizes) / sizeof (*blockSizes))]),
            input.samples.size() / input.channels - position);
        stream.process (input.samples.data() + position * input.channels,
                        output.data() + position * input.channels,
                        static_cast<int> (frames));
        position += frames;
    }
    return output;
}

void compareDelayed (const rit::Audio& input, unsigned kbps, unsigned version)
{
    const auto expected = rit::decode (rit::encode (input, kbps, version));
    const auto actual = runStreaming (input, kbps, version);
    const auto delay = rit::Streaming::latencySamples();
    for (size_t frame = 0; frame < input.samples.size() / input.channels; ++frame)
        for (unsigned channel = 0; channel < input.channels; ++channel)
        {
            const auto index = frame * input.channels + channel;
            const float wanted = frame < delay
                                   ? 0.0f
                                   : expected.samples[(frame - delay) * input.channels + channel];
            require (std::equal_to<>{} (actual[index], wanted),
                     "Streaming output differs from packet decoder");
            require (std::isfinite (actual[index]), "Streaming output is nonfinite");
        }
}
}

int main() try
{
    // Version 2 marks a transient packet by setting bit 7 of its channel
    // count. The impulse remains at its sample position after decoding.
    rit::Audio impulse { 48000, 1, std::vector<float> (128, 0.0f) };
    impulse.samples[37] = 0.8f;
    const auto impulseBytes = rit::encode (impulse, 96, 2);
    require (impulseBytes[30] == 0x81u, "Version 2 did not select sparse transient mode");
    const auto impulseDecoded = rit::decode (impulseBytes);
    require (std::abs (impulseDecoded.samples[37]) > 0.79f,
             "Sparse transient lost its peak");
    for (unsigned n = 0; n < 37; ++n)
        require (std::equal_to<>{} (impulseDecoded.samples[n], 0.0f), "Sparse transient introduced pre-echo");

    rit::Audio music { 48000, 2, std::vector<float> (128 * 8 * 2) };
    for (size_t frame = 0; frame < music.samples.size() / 2; ++frame)
    {
        music.samples[frame * 2] = 0.35f * static_cast<float> (
            std::sin (static_cast<double> (frame) * 0.031) + 0.25 * std::sin (static_cast<double> (frame) * 0.007));
        music.samples[frame * 2 + 1] = 0.31f * static_cast<float> (
            std::sin (static_cast<double> (frame) * 0.027 + 0.4) + 0.2 * std::sin (static_cast<double> (frame) * 0.011));
    }
    compareDelayed (music, 256, 1);
    compareDelayed (music, 256, 2);

    // The sparse mode must also preserve stereo packet timing and arbitrary
    // host block boundaries.
    rit::Audio transient { 48000, 2, std::vector<float> (128 * 9 * 2, 0.0f) };
    transient.samples[23 * 2] = 0.8f;
    transient.samples[23 * 2 + 1] = -0.6f;
    transient.samples[128 * 4 * 2 + 91 * 2] = 0.5f;
    transient.samples[128 * 4 * 2 + 91 * 2 + 1] = 0.45f;
    compareDelayed (transient, 96, 2);

    for (unsigned rate : { 8000u, 44100u, 48000u, 96000u, 192000u })
        for (unsigned channels : { 1u, 2u })
            for (unsigned version : { 1u, 2u })
            {
                rit::Audio varied { rate, channels, std::vector<float> (1025 * channels) };
                for (size_t i = 0; i < varied.samples.size(); ++i)
                    varied.samples[i] = 0.2f * static_cast<float> (std::sin (static_cast<double> (i) * 0.07));
                compareDelayed (varied, rate / 100, version);
            }
    // Exercise the v2 zero-count escape for a full 128-coefficient spectrum.
    rit::Audio dense { 48000, 1, std::vector<float> (1024, 0.25f) };
    require (rit::encode (dense, 1024, 2)[30] == 0, "Full spectral count encoding changed");
    compareDelayed (dense, 1024, 2);
    // A sparse zero-count remains illegal even with a valid checksum.
    auto malformed = impulseBytes;
    malformed[30] = 0x80u;
    const auto checksum = rit::crc (malformed.data() + 30, malformed.size() - 30);
    for (unsigned i = 0; i < 4; ++i) malformed[26 + i] = static_cast<uint8_t> (checksum >> (i * 8));
    bool rejected = false;
    try { static_cast<void> (rit::decode (malformed)); }
    catch (const std::exception&) { rejected = true; }
    require (rejected, "Invalid sparse mode count accepted");

    rit::Streaming stream (48000, 2, 96, 2);
    require (stream.latencySamples() == 128, "Streaming latency changed");
    require (! stream.setBitrate (1), "Invalid bitrate accepted");
    require (stream.bitrate() == 96, "Invalid bitrate changed state");
    require (stream.setBitrate (128), "Valid bitrate rejected");
    require (stream.packetBytes() >= 20, "Packet bytes below channel overhead");

    const std::vector<float> resetInput (128 * 2, 0.1f);
    std::vector<float> first (resetInput.size()), second (resetInput.size());
    stream.reset();
    stream.process (resetInput.data(), first.data(), 64);
    stream.reset();
    stream.process (resetInput.data(), second.data(), 64);
    require (std::equal (first.begin(), first.end(), second.begin(), [] (float a, float b) {
        return std::equal_to<>{} (a, b);
    }), "Streaming reset is not deterministic");

    // Allocate all test storage before entering the callback guard. The
    // packet path uses fixed arrays, including when a nonzero block is flushed.
    rit::Streaming guarded (48000, 1, 96, 2);
    std::array<float, 128> guardedInput{};
    std::array<float, 128> guardedOutput{};
    for (unsigned i = 0; i < guardedInput.size(); ++i)
        guardedInput[i] = i == 19 ? 0.8f : 0.0f;
    forbiddenAllocations.store (0, std::memory_order_relaxed);
    forbidAlloc.store (true, std::memory_order_relaxed);
    guarded.process (guardedInput.data(), guardedOutput.data(), 128);
    require (guarded.setBitrate (128), "Bitrate update failed in callback guard");
    guarded.process (guardedInput.data(), guardedOutput.data(), 128);
    forbidAlloc.store (false, std::memory_order_relaxed);
    require (forbiddenAllocations.load (std::memory_order_relaxed) == 0,
             "Streaming callback allocated memory");

    require (std::abs (guardedOutput[19]) > 0.79f,
             "Changing bitrate reset pending decoded audio");

    // In-place processing reads a frame before writing its delayed output.
    rit::Streaming separate (48000, 1, 96, 2);
    rit::Streaming inPlace (48000, 1, 96, 2);
    std::array<float, 256> source{};
    std::array<float, 256> separateOutput{};
    for (unsigned i = 0; i < source.size(); ++i)
        source[i] = 0.25f * static_cast<float> (std::sin (i * 0.03));
    auto inPlaceBuffer = source;
    separate.process (source.data(), separateOutput.data(), static_cast<int> (source.size()));
    inPlace.process (inPlaceBuffer.data(), inPlaceBuffer.data(), static_cast<int> (source.size()));
    require (std::equal (separateOutput.begin(), separateOutput.end(), inPlaceBuffer.begin(),
                         [] (float a, float b) { return std::abs (a - b) < 1.0e-7f; }),
             "In-place streaming changed the packet result");

    rit::Streaming invalid (48000, 3, 128, 9);
    require (! invalid.isValid() && ! invalid.setBitrate (128),
             "Invalid streaming setup accepted");
    std::array<float, 6> invalidOutput { 1, 1, 1, 1, 1, 1 };
    invalid.process (invalidOutput.data(), invalidOutput.data(), 2);
    for (const auto value : invalidOutput)
        require (std::equal_to<>{} (value, 1.0f), "Invalid channel layout touched output");

    std::cout << "PASS: v1/v2 packet decoding, sparse transients, fixed-latency streaming, reset and bitrate validation\n";
}
catch (const std::exception& error)
{
    std::cerr << error.what() << '\n';
    return 1;
}

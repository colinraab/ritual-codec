// SPDX-License-Identifier: AGPL-3.0-or-later
#include "rit_stream.hpp"
#include <iostream>
#include <random>
static void rejects (const rit::Bytes& b)
{
    bool failed = false;
    try
    {
        rit::decode (b);
    }
    catch (const std::exception&)
    {
        failed = true;
    }
    rit::require (failed, "Malformed stream accepted");
}
int main()
try
{
    for (unsigned channels : {1u, 2u})
        for (unsigned rate : {44100u, 48000u, 96000u})
            for (size_t length : {1u, 127u, 128u, 129u, 4096u})
            {
                rit::Audio a{rate, channels, {}};
                a.samples.resize (length * channels);
                for (size_t n = 0; n < length; ++n)
                    for (unsigned c = 0; c < channels; ++c)
                        a.samples[n * channels + c] =
                            .2f * static_cast<float> (
                                      std::sin (static_cast<double> (n) * (.03 + .09 * c)));
                const auto b = rit::encode (a, 256);
                const auto d = rit::decode (b);
                rit::require (d.rate == rate && d.channels == channels &&
                                  d.samples.size() == a.samples.size(),
                              "Round-trip metadata/length");
                rit::require (b == rit::encode (a, 256), "Nondeterministic encoder");
                rit::require (d.samples == rit::decode (b).samples, "Decoder retains state");
                for (float x : d.samples)
                    rit::require (std::isfinite (x), "Nonfinite decode");
                for (size_t n = 0; n + 128 <= length; n += 128)
                    for (unsigned c = 0; c < channels; ++c)
                    {
                        double in = 0, out = 0;
                        for (size_t j = 0; j < 128; ++j)
                        {
                            const double x = a.samples[(n + j) * channels + c],
                                         y = d.samples[(n + j) * channels + c];
                            in += x * x;
                            out += y * y;
                        }
                        rit::require (std::abs (std::sqrt (in / 128) - std::sqrt (out / 128)) <
                                          4.0 / 65535,
                                      "Block RMS not preserved");
                    }
                for (size_t cut = 0; cut < b.size(); cut += 17)
                    rejects (rit::Bytes (b.begin(), b.begin() + static_cast<std::ptrdiff_t> (cut)));
                auto bad = b;
                bad.push_back (0);
                rejects (bad);
                bad = b;
                bad.back() ^= 1;
                rejects (bad);
                bad = b;
                bad[4] = 3;
                rejects (bad);
            }
    rit::Audio silence{48000, 2, std::vector<float> (512)};
    auto d = rit::decode (rit::encode (silence, 96));
    for (float x : d.samples)
        rit::require (x == 0, "Silence produced output");
    for (unsigned offset = 0; offset < rit::block; ++offset)
    {
        rit::Audio impulse{48000, 1, std::vector<float> (128)};
        impulse.samples[offset] = .8f;
        const auto decoded = rit::decode (rit::encode (impulse, 96));
        double energy = 0;
        for (float x : decoded.samples)
            energy += static_cast<double> (x) * x;
        rit::require (std::abs (energy - .64) < .001, "Impulse energy lost");
    }
    for (float level : {-4.f, 4.f})
    {
        rit::Audio dc{48000, 1, std::vector<float> (129, level)};
        const auto decoded = rit::decode (rit::encode (dc, 96));
        for (float x : decoded.samples)
            rit::require (std::isfinite (x), "Endpoint input overflow");
    }
    bool lowRateRejected = false;
    try
    {
        rit::encode (silence, 1);
    }
    catch (const std::exception&)
    {
        lowRateRejected = true;
    }
    rit::require (lowRateRejected, "Impossible bitrate accepted");
    // Recomputed checksum forces malformed payload tests beyond the CRC check.
    auto b = rit::encode (silence, 96);
    b[30] = 255;
    auto checksum = rit::crc (b.data() + 30, 26);
    for (unsigned i = 0; i < 4; ++i)
        b[26 + i] = static_cast<uint8_t> (checksum >> (8 * i));
    rejects (b);
    // Random input/truncation cannot request an unbounded decode allocation.
    std::mt19937 gen (42);
    for (unsigned i = 0; i < 1000; ++i)
    {
        rit::Bytes noise (i % 600);
        for (auto& x : noise)
            x = static_cast<uint8_t> (gen());
        rejects (noise);
    }
    std::cout << "PASS: standalone round trips, repeatability, block energy, boundaries, "
                 "malformed/truncated streams\n";
}
catch (const std::exception& e)
{
    std::cerr << e.what() << '\n';
    return 1;
}

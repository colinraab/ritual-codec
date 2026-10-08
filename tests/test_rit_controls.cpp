// SPDX-License-Identifier: AGPL-3.0-or-later
#include "rit_stream.hpp"
#include <iostream>
#include <limits>

static void check (bool ok, const char* text)
{
    rit::require (ok, text);
}
int main()
try
{
    rit::Audio a{48000, 2, {}};
    for (unsigned n = 0; n < rit::block * 20 + 37; ++n)
    {
        const float x = n / 128 % 4 == 0
                            ? (n % 128 == 0 ? .8f : 0.f)
                            : .3f * std::sin (float (n) * (.021f + float (n / 128) * .03f)) +
                                  .13f * std::sin (float (n) * 1.7f);
        a.samples.push_back (x);
        a.samples.push_back (-x);
    }
    for (unsigned version : {1u, 2u})
    {
        const auto base = rit::encode (a, 96, version);
        check (base == rit::encode (a, 96, version, {}), "Neutral controls changed bytes");
        for (auto controls : {rit::EncoderControls{0, 0, 1, 0}, rit::EncoderControls{1, .95f, 0, 0},
                              rit::EncoderControls{1, 0, 0, 0}, rit::EncoderControls{1, 0, 1, 1},
                              rit::EncoderControls{.4f, .7f, .6f, .8f}})
        {
            const auto bytes = rit::encode (a, 96, version, controls);
            const auto decoded = rit::decode (bytes);
            if (version == 2 || controls.punch > 0 || controls.memory > 0)
                check (bytes != base, "Control did not affect encoding");
            check (bytes.size() == base.size(), "Controls changed packet budget");
            rit::Streaming stream (a.rate, a.channels, 96, version);
            stream.setControls (controls);
            auto input = a.samples;
            input.resize (input.size() + rit::block * a.channels, 0);
            std::vector<float> output (input.size());
            for (size_t pos = 0; pos < input.size() / 2;)
            {
                const auto count = std::min (size_t (17), input.size() / 2 - pos);
                stream.process (input.data() + pos * 2, output.data() + pos * 2,
                                static_cast<int> (count));
                pos += count;
            }
            for (size_t n = 0; n < a.samples.size(); ++n)
            {
                check (std::isfinite (decoded.samples[n]), "Nonfinite output");
                check (std::abs (decoded.samples[n] - output[n + rit::block * 2]) < 1e-6f,
                       "Offline/stream mismatch");
            }
            stream.reset();
            std::array<float, rit::block * 2> zeros{}, silent{};
            stream.process (zeros.data(), silent.data(), rit::block);
            for (float x : silent)
                check (std::abs (x) < 1e-20f, "Reset leaked history");
        }
    }
    rit::Audio quiet{48000, 1, std::vector<float> (256, .0001f)};
    for (float level : {0.f, .00001f, .0001f, 1.f})
    {
        std::fill (quiet.samples.begin(), quiet.samples.end(), level);
        auto decoded = rit::decode (rit::encode (quiet, 96, 2, {0, 1, 0, 1}));
        for (float x : decoded.samples)
            check (std::isfinite (x), "Quiet crunched block invalid");
    }
    rit::Streaming safe (48000, 1);
    safe.setControls (
        {std::numeric_limits<float>::quiet_NaN(), 2, -1, std::numeric_limits<float>::infinity()});
    std::cout
        << "PASS: encoder controls, fixed budget, independent decoding, streaming and reset\n";
    return 0;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << '\n';
    return 1;
}

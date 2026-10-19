// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Colin Raab and contributors.
#include "audio_input.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
#include <string>

static void writeFile (const char* path, const rit::Bytes& b)
{
    std::ofstream f (path, std::ios::binary);
    rit::require (bool (f), "Cannot open output");
    rit::require (bool (f.write (reinterpret_cast<const char*> (b.data()),
                                 static_cast<std::streamsize> (b.size()))),
                  "Write failed");
}
static rit::Bytes wav (const rit::Audio& a)
{
    // The float32 WAV header stores sizes as 32-bit values.
    rit::require (a.samples.size() <= (UINT32_MAX - 48) / 4,
                  "Output too large for a WAV file (4 GiB limit)");
    rit::Bytes b;
    rit::put (b, 0x46464952, 4);
    rit::put (b, 48 + a.samples.size() * 4, 4);
    rit::put (b, 0x45564157, 4);
    rit::put (b, 0x20746d66, 4);
    rit::put (b, 16, 4);
    rit::put (b, 3, 2);
    rit::put (b, a.channels, 2);
    rit::put (b, a.rate, 4);
    rit::put (b, a.rate * a.channels * 4, 4);
    rit::put (b, a.channels * 4, 2);
    rit::put (b, 32, 2);
    rit::put (b, 0x74636166, 4);
    rit::put (b, 4, 4);
    rit::put (b, a.samples.size() / a.channels, 4);
    rit::put (b, 0x61746164, 4);
    rit::put (b, a.samples.size() * 4, 4);
    for (float x : a.samples)
    {
        uint32_t v;
        std::memcpy (&v, &x, 4);
        rit::put (b, v, 4);
    }
    return b;
}
int main (int argc, char** argv)
try
{
    if (argc < 4)
    {
        std::cerr << "Experimental Ritual bitstream\nritual-stream encode input-audio output.rit "
                     "[kbps=128] [version=2] [--competition 0..1] [--memory 0..1] [--punch 0..1] "
                     "[--crunch 0..1]\nritual-stream decode input.rit output.wav\n";
        return 1;
    }
    ritual_input::differentFiles (argv[2], argv[3]);
    const std::string mode = argv[1];
    rit::require (std::string (argv[2]) != argv[3], "Input and output must differ");
    if (mode == "encode")
    {
        unsigned kbps = 128, version = 2;
        rit::EncoderControls controls;
        int arg = 4;
        if (arg < argc && std::string (argv[arg]).rfind ("--", 0) != 0)
        {
            std::string v = argv[arg++];
            size_t end = 0;
            const auto n = std::stoul (v, &end);
            rit::require (end == v.size() && n >= 1 && n <= 4096, "Invalid bitrate");
            kbps = static_cast<unsigned> (n);
        }
        if (arg < argc && std::string (argv[arg]).rfind ("--", 0) != 0)
        {
            const std::string v = argv[arg++];
            rit::require (v == "1" || v == "2", "Unsupported version");
            version = v == "1" ? 1u : 2u;
        }
        unsigned seen = 0;
        while (arg < argc)
        {
            const std::string flag = argv[arg++];
            float* target = nullptr;
            unsigned mask = 0;
            if (flag == "--competition")
            {
                target = &controls.competition;
                mask = 1;
            }
            else if (flag == "--memory")
            {
                target = &controls.memory;
                mask = 2;
            }
            else if (flag == "--punch")
            {
                target = &controls.punch;
                mask = 4;
            }
            else if (flag == "--crunch")
            {
                target = &controls.crunch;
                mask = 8;
            }
            else
                throw std::runtime_error ("Unknown encoder option: " + flag);
            rit::require ((seen & mask) == 0, "Duplicate encoder option");
            seen |= mask;
            rit::require (arg < argc, "Missing encoder option value");
            const std::string value = argv[arg++];
            size_t end = 0;
            const float x = std::stof (value, &end);
            rit::require (end == value.size() && std::isfinite (x) && x >= 0 && x <= 1,
                          "Encoder controls must be finite values from 0 to 1");
            *target = x;
        }
        const auto a = ritual_input::read (argv[2]);
        const auto b = rit::encode (a, kbps, version, controls);
        writeFile (argv[3], b);
        const double seconds = static_cast<double> (a.samples.size()) / (a.channels * a.rate);
        std::cout << "{\"bytes\":" << b.size() << ",\"version\":" << version
                  << ",\"requested_kbps\":" << kbps
                  << ",\"file_kbps\":" << static_cast<double> (b.size()) * 8 / seconds / 1000
                  << ",\"frame_samples\":128}\n";
    }
    else if (mode == "decode")
    {
        rit::require (argc == 4, "Unexpected arguments");
        const auto a = rit::decode (ritual_input::readFile (argv[2]));
        writeFile (argv[3], wav (a));
        std::cout << "{\"samples_per_channel\":" << a.samples.size() / a.channels
                  << ",\"sample_rate\":" << a.rate << "}\n";
    }
    else
        throw std::runtime_error ("Expected encode or decode");
    return 0;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << '\n';
    return 2;
}

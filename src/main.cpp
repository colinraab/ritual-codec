// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Colin Raab and contributors.
#include "classic_engine.hpp"
#include "audio_input.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>
static void w32 (std::ostream& s, uint32_t x)
{
    for (int i = 0; i < 4; i++)
        s.put (char (x >> (8 * i)));
}
static void w16 (std::ostream& s, uint16_t x)
{
    s.put (char (x));
    s.put (char (x >> 8));
}
int main (int argc, char** argv)
try
{
    if (argc < 3)
    {
        std::cerr
            << "Usage: ritual-codec INPUT_AUDIO OUTPUT.wav [budget competition memory punch crunch "
               "clock downsample transparent]\nInput: PCM16/24 or float32 WAV; MP3/FLAC with "
               "RITUAL_CLI_SNDFILE=ON; output float32. Delay trimmed, tail flushed.\n";
        return 1;
    }
    classic::Params p;
    if (argc > 3)
        p.budget = std::stof (argv[3]);
    if (argc > 4)
        p.competition = std::stof (argv[4]);
    if (argc > 5)
        p.memory = std::stof (argv[5]);
    if (argc > 6)
        p.punch = std::stof (argv[6]);
    if (argc > 7)
        p.crunch = std::stof (argv[7]);
    if (argc > 8)
        p.clock = std::stoi (argv[8]);
    if (argc > 9)
        p.downsample = std::stoi (argv[9]);
    if (argc > 10)
        p.transparent = std::stoi (argv[10]) != 0;
    ritual_input::differentFiles (argv[1], argv[2]);
    const auto audio = ritual_input::read (argv[1]);
    const int ch = static_cast<int> (audio.channels), sr = static_cast<int> (audio.rate);
    const size_t n = audio.samples.size(), count = n / audio.channels;
    std::vector<float> input = audio.samples;
    input.resize (n + classic::N * audio.channels, 0);
    std::vector<float> output (input.size());
    classic::Engine engine (sr, ch, p);
    auto begin = std::chrono::steady_clock::now();
    for (size_t i = 0; i < count + classic::N; i += 64)
    {
        int block = std::min<size_t> (64, count + classic::N - i);
        engine.process (input.data() + i * ch, output.data() + i * ch, block);
    }
    double elapsed =
        std::chrono::duration<double> (std::chrono::steady_clock::now() - begin).count();
    // The float32 WAV header stores sizes as 32-bit values.
    if (n > (UINT32_MAX - 48) / 4)
        throw std::runtime_error ("Output too large for a WAV file (4 GiB limit)");
    std::ofstream o (argv[2], std::ios::binary);
    if (! o)
        throw std::runtime_error ("Cannot open output");
    o.write ("RIFF", 4);
    w32 (o, 48 + n * 4);
    o.write ("WAVEfmt ", 8);
    w32 (o, 16);
    w16 (o, 3);
    w16 (o, ch);
    w32 (o, sr);
    w32 (o, sr * ch * 4);
    w16 (o, ch * 4);
    w16 (o, 32);
    o.write ("fact", 4);
    w32 (o, 4);
    w32 (o, count);
    o.write ("data", 4);
    w32 (o, n * 4);
    float peak = 0;
    for (size_t i = classic::N * ch; i < output.size(); i++)
    {
        float v = output[i];
        if (! std::isfinite (v))
            throw std::runtime_error ("Nonfinite output");
        peak = std::max (peak, std::abs (v));
        uint32_t u;
        std::memcpy (&u, &v, 4);
        w32 (o, u);
    }
    if (! o)
        throw std::runtime_error ("Write failed");
    std::cout << "{\"latency_samples\":" << classic::N
              << ",\"latency_ms\":" << 1000. * classic::N / sr << ",\"seconds\":" << elapsed
              << ",\"realtime_factor\":" << elapsed / (double (count) / sr) << ",\"peak\":" << peak
              << "}\n";
}
catch (const std::exception& e)
{
    std::cerr << e.what() << "\n";
    return 2;
}
